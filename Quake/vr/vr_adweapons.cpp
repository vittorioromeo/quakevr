// vr_adweapons.cpp -- see vr_adweapons.hpp. AD support.

#include "vr_mdlbuild.hpp"
#include "vr_bytes.hpp"
#include "vr_adweapons.hpp"

#include "vr_backend.hpp"
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"

#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <stdio.h>
#include <string.h>

namespace qvr::adweapons
{
namespace
{

constexpr size_t headerSize = 84;
constexpr int frameCount = 9;  // the Quake VR weapons' (their animations' frame numbers stay in range)
constexpr byte capColour = 2;  // the holes' fans: Quake's palette's dark grey (as the guns' dark metal)
constexpr int capRows = 4;     // its texels, under everything else in the skin
constexpr float capWeld = 0.05f; // the holes found on the surface welded this close (the rounding's hairline cracks closed)
constexpr int noteBins = 8;      // vr_adweapon_models: a made part's top in this many slices along x (Parts::note)
// The Widowmaker's turning barrels (his third notes, 2026-10-19: a Gatling auto shotgun): their axis (y 0, z spinZ, along
// x), the barrels' centres spinR from it; spinPoses frames after the still ones, a whole turn (each barrel its own
// texels), spinStep degrees apart (the frames blended between: 0.7% in at the middle, unseen).
constexpr float spinZ = 6.8f, spinR = 2.f;
constexpr int spinPoses = 27;
constexpr float spinStep = 360.f / static_cast<float>(spinPoses);

// A source model as measured (another release: not made).
struct Source
{
    const char* path;
    int numVerts, numTris, skinW, skinH;
};

// A rectangle of the made skin's texels (AD's skin is its first rows), laid on a face: s across it and t along the
// weapon's x (swap: s along x, t across).
struct Rect
{
    float s0, t0, s1, t1;
    bool swap;
};
constexpr Rect noRect{0.f, 0.f, 0.f, 0.f, false};

struct Parts;
void widowmakerParts(Parts& p);
void plasmaParts(Parts& p);

// One weapon: AD's view model in one pose (`frame`), its pieces holding `drop` taken off, laid into the Quake VR weapon's
// frame (src axis/up -> dst axis/up, scaled by k, srcPivot onto dstPivot), cut to dot(clipN, p) >= clipD, the Quake VR
// model's pieces holding `graftSeeds` put on as they are, the holes closed (the loop whose middle is within 3 units of
// apexNear fanned from apexAt, the others from their middles); packed from `corner` (the Quake VR model's).
struct Recipe
{
    const char* name;
    Source source;
    const char* frame;
    glm::vec3 srcAxis, srcUp, dstAxis, dstUp;
    float k;
    glm::vec3 srcPivot, dstPivot;
    int drop[3];
    bool clip;
    glm::vec3 clipN;
    float clipD;
    Source graft;
    int graftSeeds[3];
    glm::vec3 corner;
    bool apex;
    glm::vec3 apexNear, apexAt;
    const char* standIn;
    bool takeSource;        // AD's triangles laid in (the Widowmaker: only its skin; every part made)
    Rect cutCap;            // the cut's cap: these texels laid flat on it (noRect: the dark fan)
    void (*parts)(Parts&);  // the parts made of simple shapes in AD's texels (null: none)
    int liftSeed;           // a grafted piece (its vertex; -1 none) moved by `lift` onto the body
    glm::vec3 lift;
};

// The measurements (taken on AD 1.80 patch 1's models; ROUND22.md "AD's weapons as VR models"):
// - The Shadow Axe (v_shadaxe3.mdl, frame0: AD's axe in its right hand): the arm and hand (the piece holding vertex 257)
//   off; the handle's axis (its vertices' principal axis) onto the Quake VR axe's (from (0.2 0 -3.55) along (4.2 0
//   11.55): vr_axestick.cpp), the blade's way out from it onto the axe's (0.9398 0 -0.3417); 0.85 of its size (the blade
//   12.6 units out, as the axe's 12; the handle as thick); the handle's foot half a unit up the axe's (its rim above the
//   axe's corner), 0.04 to the left (its head's side within the corner).
// - The Widowmaker (v_shot3.mdl, fire1: three barrels in a shell open below and behind, tilted 4.5 degrees down): level,
//   1.85 times (25.5 units from the breech plate to the muzzle, the super shotgun 28), the breech's bottom at the super
//   shotgun's receiver's back (1.2 0 2.8); the super shotgun's grip, trigger guard and trigger (v_shot2.mdl's pieces of
//   vertices 151, 241, 281); the open bottom fanned from its middle, flat (13.5 0 3).
// - The Plasma Gun (v_plasma.mdl, base1: level, centred): as it is, its pistol grip, trigger and stock's bar (the pieces
//   of vertices 78, 711, 409) off, the grip's place (-9 0 -13) onto the lightning gun's grip (-0.5 0 4.5), cut just
//   behind the hand (x -5.55: the stock out, the lightning gun's corner kept); the lightning gun's grip, trigger guard and
//   trigger (v_light.mdl's pieces of vertices 234, 403, 493), the trigger 0.6 up (his notes 2026-10-19: it hung 0.48 below
//   the body, detached; vr_adweapon_models prints each grafted piece's least gap up to the body).
const Recipe recipes[] = {
    {"progs/v_shadaxe3.mdl#vr", {"progs/v_shadaxe3.mdl", 281, 292, 512, 128}, "frame0", {0.5949f, 0.2699f, 0.7571f},
        {0.6493f, 0.3939f, -0.6506f}, {0.3419f, 0.f, 0.9397f}, {0.9398f, 0.f, -0.3417f}, 0.85f, {16.1405f, -23.6240f, -36.1937f},
        {0.3710f, 0.04f, -3.0802f}, {257, -1, -1}, false, {0.f, 0.f, 0.f}, 0.f, {nullptr, 0, 0, 0, 0}, {-1, -1, -1},
        {-2.997f, -1.6813f, -3.9006f}, false, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, "progs/v_axe.mdl", true, noRect, nullptr, -1,
        {0.f, 0.f, 0.f}},
    {"progs/v_shot3.mdl#vr", {"progs/v_shot3.mdl", 206, 238, 256, 256}, "fire1", {12.85f, 0.f, -1.f}, {0.f, 0.f, 1.f},
        {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, 1.85f, {2.57f, 0.f, -8.6f}, {1.2f, 0.f, 2.8f}, {-1, -1, -1}, false, {0.f, 0.f, 0.f}, 0.f,
        {"progs/v_shot2.mdl", 1207, 983, 512, 146}, {151, 241, 281}, {-15.5775f, -4.6938f, -4.2402f}, false, {0.f, 0.f, 0.f},
        {0.f, 0.f, 0.f}, "progs/v_shot2.mdl", false, noRect, widowmakerParts, -1, {0.f, 0.f, 0.f}},
    {"progs/v_plasma.mdl#vr", {"progs/v_plasma.mdl", 786, 427, 128, 128}, "base1", {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f},
        {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, 1.f, {-9.f, 0.f, -13.f}, {-0.5f, 0.f, 4.5f}, {78, 711, 409}, true, {1.f, 0.f, 0.f}, -5.55f,
        {"progs/v_light.mdl", 1358, 865, 512, 178}, {234, 403, 493}, {-5.6211f, -9.3429f, -6.0018f}, false, {0.f, 0.f, 0.f},
        {0.f, 0.f, 0.f}, "progs/v_light.mdl", true, {1.f, 74.f, 31.f, 97.f, false}, plasmaParts, 493, {0.f, 0.f, 0.6f}},
};

// The Plasma Gun's cell (the lightning gun's magazine and well, made by make_mags.py in v_light.mdl's space: the cell
// hangs below its seat) turned a quarter about x so that it stands out of the gun's inner side (+y: towards the body
// held in the right hand; mirrored in the left), its seat moved into the receiver made on that side (plasmaParts). Named
// as the parts of "progs/v_plasma.mdl#vr" (vr_weapons.cpp slotForPart: the Plasma Gun's slot, its Scale and offsets;
// vr_view.cpp magMounts, loadPorts).
struct SidePart
{
    const char* name;
    const char* from;
};
constexpr SidePart sideParts[] = {
    {"progs/vr_mag_on_v_plasma.mdl#vr", "progs/vr_mag_on_v_light.mdl"},
    {"progs/vr_magwell_on_v_plasma.mdl#vr", "progs/vr_magwell_on_v_light.mdl"},
};
constexpr glm::vec3 lightSeat{11.6f, 0.f, 3.f};  // the lightning gun's (vr_view.cpp magMounts)
// The cell's top, inside the gun; its middle 3.6 further out (+y). (His notes 2026-10-18: 0.6 further out than it was,
// so that it shows a little more; vr_view.cpp magMounts, vr_view_internal.hpp loadPorts.)
constexpr glm::vec3 plasmaSeat{11.6f, 2.1f, 4.5f};

void put(za::Vector<byte>& out, int v)
{
    const int le = LittleLong(v);
    out.emplaceBackRange(reinterpret_cast<const byte*>(&le), 4);
}

void putf(za::Vector<byte>& out, float v)
{
    const float le = LittleFloat(v);
    out.emplaceBackRange(reinterpret_cast<const byte*>(&le), 4);
}

// A model read: one pose's positions, its texture coordinates and triangles, its first skin.
struct Mesh
{
    int skinW{0}, skinH{0};
    za::Vector<byte> skin;
    za::Vector<glm::vec3> pos;
    za::Vector<int> st;   // onseam, s, t per vertex
    za::Vector<int> tris; // facesfront, a, b, c per triangle
};

// `bytes` as `src` measures it; the pose named `frame` (null: the first).
[[nodiscard]] bool parse(const za::Vector<byte>& bytes, const Source& src, const char* frame, Mesh& m)
{
    const byte* b = bytes.data();
    const size_t size = bytes.size();
    if(size < headerSize || qvr::bytes::le32(b, 0) != IDPOLYHEADER || qvr::bytes::le32(b, 4) != ALIAS_VERSION || qvr::bytes::le32(b, 52) != src.skinW || qvr::bytes::le32(b, 56) != src.skinH ||
        qvr::bytes::le32(b, 60) != src.numVerts || qvr::bytes::le32(b, 64) != src.numTris || qvr::bytes::le32(b, 48) < 1)
    {
        return false;
    }
    const int numSkins = qvr::bytes::le32(b, 48), numFrames = qvr::bytes::le32(b, 68), nv = src.numVerts, nt = src.numTris;
    const glm::vec3 scale{qvr::bytes::lef32(b, 8), qvr::bytes::lef32(b, 12), qvr::bytes::lef32(b, 16)}, origin{qvr::bytes::lef32(b, 20), qvr::bytes::lef32(b, 24), qvr::bytes::lef32(b, 28)};
    const size_t skinBytes = static_cast<size_t>(src.skinW) * static_cast<size_t>(src.skinH);
    size_t at = headerSize;
    m.skin.clear();
    for(int s = 0; s < numSkins; s++)
    {
        if(at + 8 > size)
        {
            return false;
        }
        const byte* picture = nullptr;
        if(qvr::bytes::le32(b, at) == 0)
        {
            picture = b + at + 4;
            at += 4 + skinBytes;
        }
        else
        {
            const int n = za::max(qvr::bytes::le32(b, at + 4), 1);
            picture = b + at + 8 + static_cast<size_t>(n) * 4;
            at += 8 + static_cast<size_t>(n) * 4 + static_cast<size_t>(n) * skinBytes;
        }
        if(s == 0)
        {
            if(picture + skinBytes > b + size)
            {
                return false;
            }
            m.skin.emplaceBackRange(picture, skinBytes);
        }
    }
    if(at + static_cast<size_t>(nv) * 12 + static_cast<size_t>(nt) * 16 > size)
    {
        return false;
    }
    m.skinW = src.skinW;
    m.skinH = src.skinH;
    m.st.clear();
    for(int i = 0; i < nv * 3; i++)
    {
        m.st.pushBack(qvr::bytes::le32(b, at + static_cast<size_t>(i) * 4));
    }
    at += static_cast<size_t>(nv) * 12;
    m.tris.clear();
    for(int i = 0; i < nt * 4; i++)
    {
        const int v = qvr::bytes::le32(b, at + static_cast<size_t>(i) * 4);
        if(i % 4 && (v < 0 || v >= nv))
        {
            return false;
        }
        m.tris.pushBack(v);
    }
    at += static_cast<size_t>(nt) * 16;
    size_t poseAt = 0;
    for(int f = 0; f < numFrames && !poseAt; f++)
    {
        if(at + 4 > size)
        {
            return false;
        }
        const int type = qvr::bytes::le32(b, at);
        at += 4;
        int members = 1;
        if(type != 0)
        {
            members = za::max(qvr::bytes::le32(b, at), 1);
            at += 4 + 8 + static_cast<size_t>(members) * 4;
        }
        for(int k = 0; k < members && !poseAt; k++)
        {
            const size_t nameAt = at + 8, vertsAt = at + 24;
            at = vertsAt + static_cast<size_t>(nv) * 4;
            if(at > size)
            {
                return false;
            }
            char name[17] = {};
            memcpy(name, b + nameAt, 16);
            if(!frame || !strcmp(name, frame))
            {
                poseAt = vertsAt;
            }
        }
    }
    if(!poseAt)
    {
        return false;
    }
    m.pos.clear();
    for(int v = 0; v < nv; v++)
    {
        const byte* p = b + poseAt + static_cast<size_t>(v) * 4;
        m.pos.pushBack(glm::vec3{p[0] * scale.x + origin.x, p[1] * scale.y + origin.y, p[2] * scale.z + origin.z});
    }
    return true;
}

[[nodiscard]] bool readFile(FILE* f, long offset, long length, za::Vector<byte>& out)
{
    if(length <= 0 || length > 16 * 1024 * 1024)
    {
        return false;
    }
    out.resize(static_cast<za::SizeT>(length));
    return fseek(f, offset, SEEK_SET) == 0 && fread(out.data(), 1, out.size(), f) == out.size();
}

// The search paths' first `src.path` that is the release measured (its pak's entry or its folder's file), whatever
// shadows it in the game's lookup (AD's v_plasma.mdl is Rogue's name too: vr_ad_data.inc skips AD's).
[[nodiscard]] bool load(const Source& src, const char* frame, Mesh& m, unsigned int* pathId)
{
    za::Vector<byte> bytes;
    for(const searchpath_t* s = com_searchpaths; s; s = s->next)
    {
        bool read = false;
        if(s->pack)
        {
            for(int i = 0; i < s->pack->numfiles && !read; i++)
            {
                const packfile_t& e = s->pack->files[i];
                if(q_strcasecmp(e.name, src.path))
                {
                    continue;
                }
                if(FILE* f = Sys_fopen(s->pack->filename, "rb"))
                {
                    read = readFile(f, e.filepos, e.filelen, bytes);
                    fclose(f);
                }
                break;
            }
        }
        else
        {
            char path[MAX_OSPATH];
            q_snprintf(path, sizeof(path), "%s/%s", s->filename, src.path);
            if(FILE* f = Sys_fopen(path, "rb"))
            {
                const bool sized = fseek(f, 0, SEEK_END) == 0;
                const long length = sized ? ftell(f) : 0;
                read = sized && readFile(f, 0, length, bytes);
                fclose(f);
            }
        }
        if(read && parse(bytes, src, frame, m))
        {
            if(pathId)
            {
                *pathId = s->path_id;
            }
            return true;
        }
    }
    return false;
}

// Vertices at one place (to a 0.001 grid) are one.
[[nodiscard]] za::Vector<int> weldExact(const za::Vector<glm::vec3>& pos)
{
    ankerl::unordered_dense::map<uint64_t, int> at;
    za::Vector<int> w;
    for(za::SizeT i = 0; i < pos.size(); i++)
    {
        const auto q = [&](float x) { return static_cast<uint64_t>(static_cast<int64_t>(x * 1000.f + (x < 0.f ? -0.5f : 0.5f)) & 0x1fffff); };
        const uint64_t key = q(pos[i].x) | (q(pos[i].y) << 21) | (q(pos[i].z) << 42);
        w.pushBack(at.try_emplace(key, static_cast<int>(i)).first->second);
    }
    return w;
}

[[nodiscard]] int findRoot(za::Vector<int>& parent, int a)
{
    while(parent[a] != a)
    {
        parent[a] = parent[parent[a]];
        a = parent[a];
    }
    return a;
}

// Per triangle of `m`: whether it is in a welded piece holding one of `seeds` (-1: none).
[[nodiscard]] za::Vector<bool> piecesOf(const Mesh& m, const int (&seeds)[3])
{
    const za::Vector<int> w = weldExact(m.pos);
    za::Vector<int> parent;
    for(za::SizeT i = 0; i < m.pos.size(); i++)
    {
        parent.pushBack(static_cast<int>(i));
    }
    const za::SizeT nt = m.tris.size() / 4;
    for(za::SizeT t = 0; t < nt; t++)
    {
        const int a = w[m.tris[t * 4 + 1]], b = w[m.tris[t * 4 + 2]], c = w[m.tris[t * 4 + 3]];
        parent[findRoot(parent, a)] = findRoot(parent, b);
        parent[findRoot(parent, b)] = findRoot(parent, c);
    }
    za::Vector<bool> in(nt, false);
    for(const int seed : seeds)
    {
        if(seed < 0 || seed >= static_cast<int>(m.pos.size()))
        {
            continue;
        }
        const int root = findRoot(parent, w[seed]);
        for(za::SizeT t = 0; t < nt; t++)
        {
            in[t] = in[t] || findRoot(parent, w[m.tris[t * 4 + 1]]) == root;
        }
    }
    return in;
}

// The model being made: positions, texels (absolute: no seams), triangles (clockwise from outside, as Quake's), and each
// vertex's smoothing group (its normal is the surface's of the vertices at its place in its group: 0 AD's and the grafted
// parts', one each for a made part's flat faces and its rounded sides, and for each hole's fan).
struct Out
{
    za::Vector<glm::vec3> pos;
    za::Vector<int> s, t;
    za::Vector<int> group;
    za::Vector<int> tris;
    za::Vector<bool> spins; // a vertex of the part that turns about spinAxis in the spin frames (the Widowmaker's barrels)
    int groups{1};
    bool spinning{false};   // the vertices made now are of it

    int vert(const glm::vec3& p, int ss, int tt, int g = 0)
    {
        pos.pushBack(p);
        s.pushBack(ss);
        t.pushBack(tt);
        group.pushBack(g);
        spins.pushBack(spinning);
        return static_cast<int>(pos.size()) - 1;
    }

    int newGroup()
    {
        return groups++;
    }
};

// The triangles of `m` marked `keep`, their positions through `xf`, their texels moved by (ds, dt) (a back face's seam
// vertex its own copy, half a skin over: the skin may change its width).
template <typename Xf>
void take(Out& out, const Mesh& m, const za::Vector<bool>& keep, bool keepMarked, Xf&& xf, int ds, int dt)
{
    za::Vector<int> remap(m.pos.size() * 2, -1);
    for(za::SizeT tri = 0; tri < m.tris.size() / 4; tri++)
    {
        if(keep[tri] != keepMarked)
        {
            continue;
        }
        const bool front = m.tris[tri * 4] != 0;
        for(int c = 0; c < 3; c++)
        {
            const int v = m.tris[tri * 4 + 1 + c];
            const bool seam = m.st[static_cast<za::SizeT>(v) * 3] != 0 && !front;
            int& r = remap[static_cast<za::SizeT>(v) * 2 + (seam ? 1 : 0)];
            if(r < 0)
            {
                const int ss = m.st[static_cast<za::SizeT>(v) * 3 + 1] + (seam ? m.skinW / 2 : 0);
                r = out.vert(xf(m.pos[v]), ss + ds, m.st[static_cast<za::SizeT>(v) * 3 + 2] + dt);
            }
            out.tris.pushBack(r);
        }
    }
}

// The triangles from `first` on cut to dot(n, p) >= d.
void clip(Out& out, const glm::vec3& n, float d, za::SizeT first)
{
    za::Vector<int> tris;
    for(za::SizeT i = first; i < out.tris.size(); i++)
    {
        tris.pushBack(out.tris[i]);
    }
    out.tris.resize(first);
    ankerl::unordered_dense::map<uint64_t, int> cuts;
    const auto dist = [&](int v) { return glm::dot(n, out.pos[v]) - d; };
    const auto cut = [&](int a, int b) { // a inside, b outside
        const uint64_t key = (static_cast<uint64_t>(a) << 32) | static_cast<uint32_t>(b);
        if(const auto it = cuts.find(key); it != cuts.end())
        {
            return it->second;
        }
        const float da = dist(a), db = dist(b), k = da / (da - db);
        const int v = out.vert(out.pos[a] + (out.pos[b] - out.pos[a]) * k,
            out.s[a] + static_cast<int>(static_cast<float>(out.s[b] - out.s[a]) * k + 0.5f),
            out.t[a] + static_cast<int>(static_cast<float>(out.t[b] - out.t[a]) * k + 0.5f), out.group[a]);
        cuts[key] = v;
        return v;
    };
    for(za::SizeT i = 0; i + 2 < tris.size(); i += 3)
    {
        const int v[3] = {tris[i], tris[i + 1], tris[i + 2]};
        const bool in[3] = {dist(v[0]) >= 0.f, dist(v[1]) >= 0.f, dist(v[2]) >= 0.f};
        const int count = in[0] + in[1] + in[2];
        if(count == 3)
        {
            out.tris.pushBack(v[0]);
            out.tris.pushBack(v[1]);
            out.tris.pushBack(v[2]);
            continue;
        }
        for(int r = 0; r < 3 && count > 0; r++)
        {
            const int a = v[r], b = v[(r + 1) % 3], c = v[(r + 2) % 3];
            if(count == 1 && in[r])
            {
                const int ab = cut(a, b), ac = cut(a, c);
                out.tris.pushBack(a);
                out.tris.pushBack(ab);
                out.tris.pushBack(ac);
                break;
            }
            if(count == 2 && !in[r])
            {
                const int ba = cut(b, a), ca = cut(c, a);
                out.tris.pushBack(ba);
                out.tris.pushBack(b);
                out.tris.pushBack(c);
                out.tris.pushBack(ba);
                out.tris.pushBack(c);
                out.tris.pushBack(ca);
                break;
            }
        }
    }
}

// Vertices within `tol` of each other are one (the first of them).
[[nodiscard]] za::Vector<int> weldNear(const za::Vector<glm::vec3>& pos, float tol)
{
    za::Vector<int> w;
    for(za::SizeT i = 0; i < pos.size(); i++)
    {
        int to = static_cast<int>(i);
        for(za::SizeT j = 0; j < i; j++)
        {
            if(w[j] == static_cast<int>(j) && za::abs(pos[i].x - pos[j].x) <= tol && za::abs(pos[i].y - pos[j].y) <= tol &&
                za::abs(pos[i].z - pos[j].z) <= tol)
            {
                to = static_cast<int>(j);
                break;
            }
        }
        w.pushBack(to);
    }
    return w;
}

[[nodiscard]] uint64_t edgeKey(int a, int b)
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(a)) << 32) | static_cast<uint32_t>(b);
}

// The surface's holes: the boundary loops (each the welded vertices in order, the hole's edges reversed), split where a
// walk comes back to a vertex. The degenerate triangles (two corners welded) dropped first. `open`: the boundary edges.
[[nodiscard]] za::Vector<za::Vector<int>> holes(Out& out, const za::Vector<int>& w, int& open)
{
    za::Vector<int> kept;
    for(za::SizeT i = 0; i + 2 < out.tris.size(); i += 3)
    {
        const int a = w[out.tris[i]], b = w[out.tris[i + 1]], c = w[out.tris[i + 2]];
        if(a != b && b != c && a != c)
        {
            kept.pushBack(out.tris[i]);
            kept.pushBack(out.tris[i + 1]);
            kept.pushBack(out.tris[i + 2]);
        }
    }
    out.tris = kept;
    ankerl::unordered_dense::map<uint64_t, int> count;
    for(za::SizeT i = 0; i + 2 < out.tris.size(); i += 3)
    {
        for(int c = 0; c < 3; c++)
        {
            count[edgeKey(w[out.tris[i + c]], w[out.tris[i + (c + 1) % 3]])]++;
        }
    }
    ankerl::unordered_dense::map<int, za::Vector<int>> outs; // boundary half-edges b -> a, sorted for a stable walk
    za::Vector<uint64_t> keys;
    for(const auto& [key, n] : count)
    {
        keys.pushBack(key);
    }
    for(za::SizeT i = 1; i < keys.size(); i++) // (insertion sort: a few thousand edges, once)
    {
        for(za::SizeT j = i; j > 0 && keys[j - 1] > keys[j]; j--)
        {
            const uint64_t k = keys[j];
            keys[j] = keys[j - 1];
            keys[j - 1] = k;
        }
    }
    open = 0;
    za::Vector<int> starts;
    for(const uint64_t key : keys)
    {
        const int a = static_cast<int>(key >> 32), b = static_cast<int>(key & 0xffffffffu);
        const auto twin = count.find(edgeKey(b, a));
        const int extra = count[key] - (twin == count.end() ? 0 : twin->second);
        for(int i = 0; i < extra; i++)
        {
            outs[b].pushBack(a);
            starts.pushBack(b);
            open++;
        }
    }
    za::Vector<za::Vector<int>> loops;
    for(const int s : starts)
    {
        while(!outs[s].empty())
        {
            za::Vector<int> walk;
            walk.pushBack(s);
            int v = outs[s].back();
            outs[s].popBack();
            while(v != s && !outs[v].empty())
            {
                walk.pushBack(v);
                const int next = outs[v].back();
                outs[v].popBack();
                v = next;
            }
            if(v != s)
            {
                continue; // no way back: left
            }
            za::Vector<int> stack;
            for(const int x : walk)
            {
                za::SizeT at = stack.size();
                for(za::SizeT i = 0; i < stack.size(); i++)
                {
                    if(stack[i] == x)
                    {
                        at = i;
                        break;
                    }
                }
                if(at < stack.size())
                {
                    za::Vector<int> part;
                    for(za::SizeT i = at; i < stack.size(); i++)
                    {
                        part.pushBack(stack[i]);
                    }
                    if(part.size() >= 3)
                    {
                        loops.pushBack(part);
                    }
                    stack.resize(at + 1);
                }
                else
                {
                    stack.pushBack(x);
                }
            }
            if(stack.size() >= 3)
            {
                loops.pushBack(stack);
            }
        }
    }
    return loops;
}

// Every hole fanned shut (dark texels at (capS, capT)); how many.
int seal(Out& out, const Recipe& r, int capS, int capT)
{
    const za::Vector<int> w = weldNear(out.pos, capWeld);
    int open = 0;
    const za::Vector<za::Vector<int>> loops = holes(out, w, open);
    for(const za::Vector<int>& loop : loops)
    {
        glm::vec3 middle{0.f};
        for(const int v : loop)
        {
            middle += out.pos[v];
        }
        middle /= static_cast<float>(loop.size());
        const glm::vec3 apex = r.apex && glm::length(middle - r.apexNear) < 3.f ? r.apexAt : middle;
        // The cut's cap (every corner on the cutting plane): the recipe's texels laid flat on it (y across, z up).
        bool onCut = r.clip && r.cutCap.s1 > r.cutCap.s0;
        glm::vec2 lo{1e30f}, hi{-1e30f};
        for(const int v : loop)
        {
            onCut = onCut && za::abs(glm::dot(r.clipN, out.pos[v]) - r.clipD) < 0.01f;
            lo = glm::min(lo, glm::vec2{out.pos[v].y, out.pos[v].z});
            hi = glm::max(hi, glm::vec2{out.pos[v].y, out.pos[v].z});
        }
        const auto texel = [&](const glm::vec3& p, int axis) {
            if(!onCut)
            {
                return axis == 0 ? capS : capT;
            }
            const glm::vec2 f = (glm::vec2{p.y, p.z} - lo) / glm::max(hi - lo, glm::vec2{1e-3f});
            return axis == 0 ? static_cast<int>(r.cutCap.s0 + (r.cutCap.s1 - r.cutCap.s0) * f.x + 0.5f)
                             : static_cast<int>(r.cutCap.t1 - (r.cutCap.t1 - r.cutCap.t0) * f.y + 0.5f);
        };
        const int g = out.newGroup(); // (flat: never smoothed with the surface round it)
        const int a = out.vert(apex, texel(apex, 0), texel(apex, 1), g);
        const za::SizeT first = out.pos.size();
        for(const int v : loop)
        {
            const glm::vec3 p = out.pos[v];
            out.vert(p, texel(p, 0), texel(p, 1), g);
        }
        for(za::SizeT i = 0; i < loop.size(); i++)
        {
            out.tris.pushBack(static_cast<int>(first + i));
            out.tris.pushBack(static_cast<int>(first + (i + 1) % loop.size()));
            out.tris.pushBack(a);
        }
    }
    return static_cast<int>(loops.size());
}

struct Built
{
    za::Vector<byte> file;
    int verts{0}, tris{0}, sealed{0}, openAfter{0}, grafted{0}, dropped{0}, made{0};
    glm::vec3 lo{0.f}, hi{0.f};
    bool cornerKept{false};
    glm::vec3 muzzle{0.f};          // a gun's: the middle of its front end; the axe's: the blade edge's top
    glm::vec3 edgeLo{0.f}, edgeHi{0.f}; // the axe's blade edge's ends (vr_axestick.cpp's table)
    glm::vec3 seedLo[3]{}, seedHi[3]{};  // each grafted piece's box (Recipe::graftSeeds; empty: none)
    float adAbove[3]{};                  // and the body's surface straight over its top (1e30: none)
    float adGap[3]{};                    // the least gap up to it
    const char* noteName{nullptr};       // a made part's box (Parts::note)
    glm::vec3 noteLo{0.f}, noteHi{0.f};
    float noteTop[noteBins]{};
    int spinVerts{0};                    // the vertices turned in the spin frames (spinFrames)
};

[[nodiscard]] const Recipe* recipeOf(const char* name)
{
    for(const Recipe& r : recipes)
    {
        if(!q_strcasecmp(r.name, name))
        {
            return &r;
        }
    }
    return nullptr;
}

// The basis of `axis` and the part of `up` across it: x along the axis, z up, y = z x x.
void basis(const glm::vec3& axis, const glm::vec3& up, glm::vec3& x, glm::vec3& y, glm::vec3& z)
{
    x = glm::normalize(axis);
    z = glm::normalize(up - x * glm::dot(up, x));
    y = glm::cross(z, x);
}

// What a recipe's made parts are laid with: the model being made, the Quake VR model the grip came from (its pieces laid
// again: take) and its texels' move, the dark texels (the caps' grey, the bores' black), and the muzzle they make.
struct Parts
{
    Out& o;
    const Mesh& graft;
    int graftDs, graftDt;
    int capS, capT, boreS, boreT;
    glm::vec3 muzzle{0.f};
    bool muzzleSet{false};
    // A made part's box and the made model's surface over its top (vr_adweapon_models: how it meets what is above it).
    const char* noteName{nullptr};
    glm::vec3 noteLo{0.f}, noteHi{0.f};
    float noteTop[noteBins]{};
};

// The box of the vertices from `from` on as the part `name` (Parts::note*).
void note(Parts& p, const Out& o, za::SizeT from, const char* name)
{
    p.noteName = name;
    p.noteLo = glm::vec3{1e30f};
    p.noteHi = glm::vec3{-1e30f};
    for(za::SizeT i = from; i < o.pos.size(); i++)
    {
        p.noteLo = glm::min(p.noteLo, o.pos[i]);
        p.noteHi = glm::max(p.noteHi, o.pos[i]);
    }
    // Its top's height along x (its vertices' highest in each of noteBins slices of its box).
    for(int k = 0; k < noteBins; k++)
    {
        p.noteTop[k] = -1e30f;
    }
    for(za::SizeT i = from; i < o.pos.size(); i++)
    {
        const float f = (o.pos[i].x - p.noteLo.x) / za::max(p.noteHi.x - p.noteLo.x, 1e-3f);
        const int k = za::min(noteBins - 1, static_cast<int>(f * noteBins));
        p.noteTop[k] = za::max(p.noteTop[k], o.pos[i].z);
    }
}

[[nodiscard]] int texel(float v)
{
    return static_cast<int>(v + 0.5f);
}

// One triangle, turned to face `outward` (clockwise seen from there).
void face(Out& o, int a, int b, int c, const glm::vec3& outward)
{
    const glm::vec3 n = glm::cross(o.pos[c] - o.pos[a], o.pos[b] - o.pos[a]);
    const bool keep = glm::dot(n, outward) >= 0.f;
    o.tris.pushBack(a);
    o.tris.pushBack(keep ? b : c);
    o.tris.pushBack(keep ? c : b);
}

// The texel of `r` at the share `across` of its width and `along` of its length (swapped: the other way).
[[nodiscard]] glm::ivec2 texelOf(const Rect& r, float across, float along)
{
    const float u = r.swap ? along : across, v = r.swap ? across : along;
    return {texel(r.s0 + (r.s1 - r.s0) * u), texel(r.t0 + (r.t1 - r.t0) * v)};
}

// A flat end at `x` of the convex section `sec` (y z), facing `dir` (-1 or 1 along x): fanned from its middle, `r`'s
// texels laid on it (y across, z up).
void cap(Out& o, const za::Vector<glm::vec2>& sec, float x, float dir, const Rect& r)
{
    glm::vec2 mid{0.f}, lo{1e30f}, hi{-1e30f};
    for(const glm::vec2& q : sec)
    {
        mid += q;
        lo = glm::min(lo, q);
        hi = glm::max(hi, q);
    }
    mid /= static_cast<float>(sec.size());
    const glm::vec2 size = glm::max(hi - lo, glm::vec2{1e-3f});
    const int g = o.newGroup();
    const auto vert = [&](const glm::vec2& q) {
        const glm::vec2 f = (q - lo) / size;
        const glm::ivec2 st = texelOf(Rect{r.s0, r.t0, r.s1, r.t1, false}, f.x, 1.f - f.y);
        return o.vert(glm::vec3{x, q.x, q.y}, st.x, st.y, g);
    };
    const int c = vert(mid);
    za::Vector<int> ring;
    for(const glm::vec2& q : sec)
    {
        ring.pushBack(vert(q));
    }
    for(za::SizeT i = 0; i < ring.size(); i++)
    {
        face(o, c, ring[i], ring[(i + 1) % ring.size()], glm::vec3{dir, 0.f, 0.f});
    }
}

// The convex section `sec` (y z, in order round it) from x0 to x1. Each side flat with its own of `faces` (nFaces 1: the
// same for all) across it (perFace); else one rounded surface: the rect's s by the way round (mirror: across it over half
// the way, back over the other), t along x. Its ends closed flat with end0, end1 (null: left open).
void extrude(Out& o, const za::Vector<glm::vec2>& sec, float x0, float x1, const Rect* faces, int nFaces, bool perFace, bool mirror,
    const Rect* end0, const Rect* end1)
{
    const za::SizeT n = sec.size();
    glm::vec2 mid{0.f};
    za::Vector<float> along;
    float perimeter = 0.f;
    for(za::SizeT i = 0; i < n; i++)
    {
        mid += sec[i];
        along.pushBack(perimeter);
        perimeter += glm::length(sec[(i + 1) % n] - sec[i]);
    }
    mid /= static_cast<float>(n);
    const auto share = [&](za::SizeT i) {
        const float f = i >= n ? 1.f : along[i] / perimeter;
        return mirror ? 1.f - za::abs(2.f * f - 1.f) : f;
    };
    const int rounded = o.newGroup();
    for(za::SizeT i = 0; i < n; i++)
    {
        const Rect& r = faces[nFaces == 1 ? 0 : static_cast<int>(i) % nFaces];
        const glm::vec2 a = sec[i], b = sec[(i + 1) % n];
        const float ua = perFace ? 0.f : share(i), ub = perFace ? 1.f : share(i + 1);
        const int g = perFace ? o.newGroup() : rounded;
        const glm::ivec2 a0 = texelOf(r, ua, 0.f), a1 = texelOf(r, ua, 1.f), b0 = texelOf(r, ub, 0.f), b1 = texelOf(r, ub, 1.f);
        const int va0 = o.vert({x0, a.x, a.y}, a0.x, a0.y, g), va1 = o.vert({x1, a.x, a.y}, a1.x, a1.y, g);
        const int vb0 = o.vert({x0, b.x, b.y}, b0.x, b0.y, g), vb1 = o.vert({x1, b.x, b.y}, b1.x, b1.y, g);
        const glm::vec2 out = (a + b) * 0.5f - mid;
        face(o, va0, va1, vb1, glm::vec3{0.f, out.x, out.y});
        face(o, va0, vb1, vb0, glm::vec3{0.f, out.x, out.y});
    }
    if(end0)
    {
        cap(o, sec, x0, -1.f, *end0);
    }
    if(end1)
    {
        cap(o, sec, x1, 1.f, *end1);
    }
}

// A box from `lo` to `hi`: its sides `faces` (as extrude's section's edges: -z, +y, +z, -y), its ends end0 (-x), end1 (+x).
void box(Out& o, const glm::vec3& lo, const glm::vec3& hi, const Rect (&faces)[4], const Rect& end0, const Rect& end1)
{
    const za::Vector<glm::vec2> sec{{lo.y, lo.z}, {hi.y, lo.z}, {hi.y, hi.z}, {lo.y, hi.z}};
    extrude(o, sec, lo.x, hi.x, faces, 4, true, false, &end0, &end1);
}

constexpr int roundSides = 14;

// A circle round `c` (y z): from its -y side over its top and round (a strip mirrored on it has its middle on top).
[[nodiscard]] za::Vector<glm::vec2> circle(const glm::vec2& c, float r)
{
    za::Vector<glm::vec2> pts;
    for(int k = 0; k < roundSides; k++)
    {
        const float a = 3.14159265f * (1.f - 2.f * static_cast<float>(k) / static_cast<float>(roundSides));
        pts.pushBack(c + glm::vec2{za::cos(a), za::sin(a)} * r);
    }
    return pts;
}

// A barrel along x from x0 to x1 round `c` (y z), radius r: `body` up to its brake (the last brakeLen), `brake` there (each
// strip across its top half and back under it), its breech closed with `breech`; at the muzzle a flat ring in to the bore
// (radius rin), the bore `depth` deep, black.
void barrel(Parts& p, const glm::vec2& c, float r, float rin, float x0, float x1, float brakeLen, float depth, const Rect& body,
    const Rect& brake, const Rect& breech)
{
    Out& o = p.o;
    const za::Vector<glm::vec2> outer = circle(c, r), inner = circle(c, rin);
    extrude(o, outer, x0, x1 - brakeLen, &body, 1, false, true, &breech, nullptr);
    extrude(o, outer, x1 - brakeLen, x1, &brake, 1, false, true, nullptr, nullptr);
    const int ring = o.newGroup(), bore = o.newGroup(), bottom = o.newGroup();
    const za::SizeT n = outer.size();
    za::Vector<int> rim, mouth, deep;
    for(za::SizeT i = 0; i < n; i++)
    {
        rim.pushBack(o.vert({x1, outer[i].x, outer[i].y}, p.capS, p.capT, ring));
        mouth.pushBack(o.vert({x1, inner[i].x, inner[i].y}, p.capS, p.capT, ring));
    }
    za::Vector<int> wallTop, wallDeep;
    for(za::SizeT i = 0; i < n; i++)
    {
        wallTop.pushBack(o.vert({x1, inner[i].x, inner[i].y}, p.boreS, p.boreT, bore));
        wallDeep.pushBack(o.vert({x1 - depth, inner[i].x, inner[i].y}, p.boreS, p.boreT, bore));
        deep.pushBack(o.vert({x1 - depth, inner[i].x, inner[i].y}, p.boreS, p.boreT, bottom));
    }
    for(za::SizeT i = 0; i < n; i++)
    {
        const za::SizeT j = (i + 1) % n;
        face(o, rim[i], rim[j], mouth[j], glm::vec3{1.f, 0.f, 0.f});
        face(o, rim[i], mouth[j], mouth[i], glm::vec3{1.f, 0.f, 0.f});
        const glm::vec2 in = c - (inner[i] + inner[j]) * 0.5f; // (the bore's wall faces its axis)
        face(o, wallTop[i], wallTop[j], wallDeep[j], glm::vec3{0.f, in.x, in.y});
        face(o, wallTop[i], wallDeep[j], wallDeep[i], glm::vec3{0.f, in.x, in.y});
    }
    const int middle = o.vert({x1 - depth, c.x, c.y}, p.boreS, p.boreT, bottom);
    for(za::SizeT i = 0; i < n; i++)
    {
        face(o, middle, deep[i], deep[(i + 1) % n], glm::vec3{1.f, 0.f, 0.f});
    }
}

// The Widowmaker as a big two-handed gun (his note 2026-10-18: AD's view model is a shell of three half barrels, open
// below and at the muzzle; it read as a pistol): every part made, in AD's own texels (v_shot3.mdl's 256x256 skin), in the
// super shotgun's frame, its grip where the super shotgun's is (the rear handle: grafted with the trigger guard and the
// trigger, so the hand holds it as the super shotgun). Behind the barrels a receiver (x -2..12.5, 7.2 wide, 7.4 tall, its
// top's edges cut: AD's vented brake block on its sides, AD's band along its top, the breech plate's ring on its back);
// three whole barrels (radius 1.6: one on top, two below it, as AD's) to x 44, AD's barrel strips on them and its brakes on
// their last 7 units, their bores black, two of AD's bands round each; under them at x 24..31 a mount and the front handle
// (the super shotgun's grip again, 0.88 its size: the other hand's, vr_weapons.inc slot 34's hotspot). Everything above the
// super shotgun's corner (z -4.24: the settings scale about it).
void widowmakerParts(Parts& p)
{
    Out& o = p.o;
    const float cs = static_cast<float>(p.capS), ct = static_cast<float>(p.capT);
    const Rect dark{cs, ct, cs, ct, false};
    const Rect strips[3] = {{56.f, 125.f, 81.f, 255.f, false}, {25.f, 125.f, 50.f, 255.f, false}, {86.f, 125.f, 111.f, 255.f, false}};
    const Rect brakes[3] = {{56.f, 82.f, 81.f, 124.f, false}, {25.f, 82.f, 50.f, 124.f, false}, {86.f, 82.f, 111.f, 124.f, false}};
    const Rect band{2.f, 1.f, 130.f, 11.f, false};
    const Rect bandAlong{2.f, 1.f, 130.f, 11.f, true};
    const Rect vents{128.f, 132.f, 168.f, 176.f, false};
    const Rect breechPlate{118.f, 35.f, 158.f, 75.f, false};
    const Rect underside{28.f, 140.f, 46.f, 250.f, false};

    const za::Vector<glm::vec2> receiver{{-3.6f, 3.f}, {3.6f, 3.f}, {3.6f, 9.2f}, {2.4f, 10.4f}, {-2.4f, 10.4f}, {-3.6f, 9.2f}};
    const Rect receiverFaces[6] = {underside, vents, bandAlong, bandAlong, bandAlong, vents};
    extrude(o, receiver, -2.f, 12.5f, receiverFaces, 6, true, false, &breechPlate, &dark);

    // The shell port (his VR notes 2026-10-18: an auto shotgun loaded on the inner side, +y, by the rear handle;
    // vr_view_internal.hpp loadPorts: its middle 6.5 3.6 5.6): a dark opening on the receiver's side, framed by a raised
    // rim 0.5 proud (AD's band), its inner faces dark. Its spent shells leave by the slot on the other side (vr_shells.cpp).
    // His third notes (2026-10-19): half the size (it was x 2.6..10.4, z 4.5..6.7, its rim 0.4 wide, 0.5 proud), about
    // the same middle; vr_reload_port_widow_radius halved with it (1.25).
    const Rect rimFaces[4] = {bandAlong, bandAlong, bandAlong, bandAlong};
    const Rect rimTop[4] = {dark, bandAlong, bandAlong, bandAlong};   // (its underside faces the opening)
    const Rect rimBottom[4] = {bandAlong, bandAlong, dark, bandAlong}; // (its top faces it)
    constexpr float px0 = 4.55f, px1 = 8.45f, pz0 = 5.05f, pz1 = 6.15f, rim = 0.2f, side = 3.6f, proud = 0.25f;
    box(o, {px0, side - 0.3f, pz0}, {px1, side + 0.05f, pz1}, {dark, dark, dark, dark}, dark, dark);
    box(o, {px0 - rim, side - 0.1f, pz1}, {px1 + rim, side + proud, pz1 + rim}, rimTop, bandAlong, bandAlong);
    box(o, {px0 - rim, side - 0.1f, pz0 - rim}, {px1 + rim, side + proud, pz0}, rimBottom, bandAlong, bandAlong);
    box(o, {px0 - rim, side - 0.1f, pz0}, {px0, side + proud, pz1}, rimFaces, bandAlong, dark);
    box(o, {px1, side - 0.1f, pz0}, {px1 + rim, side + proud, pz1}, rimFaces, dark, bandAlong);
    box(o, {5.f, -side - 0.05f, 7.f}, {9.5f, -side + 0.3f, 8.4f}, {dark, dark, dark, dark}, dark, dark); // (the ejection slot)

    // The barrels (his third notes: a Gatling auto shotgun): an even triangle round the spin axis (y 0, z spinZ; they were
    // at 0 8.6 and +-1.85 5.9), turned about it in the spin frames (spinFrames): the barrel on top fires.
    o.spinning = true;
    const glm::vec2 centres[3] = {{0.f, spinZ + spinR}, {-0.8660254f * spinR, spinZ - 0.5f * spinR}, {0.8660254f * spinR, spinZ - 0.5f * spinR}};
    for(int i = 0; i < 3; i++)
    {
        barrel(p, centres[i], 1.6f, 1.f, 11.5f, 44.f, 7.f, 3.f, strips[i], brakes[i], dark);
        for(const float x : {18.f, 29.5f})
        {
            extrude(o, circle(centres[i], 1.9f), x, x + 1.6f, &band, 1, false, false, &dark, &dark);
        }
    }
    o.spinning = false;
    // The muzzle's mark: a speck (two faces back to back, 0.12 across) at the top barrel's bore's middle in the muzzle's
    // plane, still in the spin frames. The muzzle anchor (44 0 8.8) binds to the nearest triangle (vr_anchor.hpp): the
    // bore's middle is open, so it was the mouth's edge, a unit to the side.
    {
        const int g = o.newGroup();
        const glm::vec3 mark{44.f, 0.f, spinZ + spinR};
        const int a = o.vert(mark + glm::vec3{0.f, -0.06f, -0.04f}, p.boreS, p.boreT, g);
        const int b = o.vert(mark + glm::vec3{0.f, 0.06f, -0.04f}, p.boreS, p.boreT, g);
        const int c = o.vert(mark + glm::vec3{0.f, 0.f, 0.07f}, p.boreS, p.boreT, g);
        face(o, a, b, c, glm::vec3{1.f, 0.f, 0.f});
        face(o, a, b, c, glm::vec3{-1.f, 0.f, 0.f});
    }

    // (Its bottom at 2.3, was 3: the front handle's top slants down to 2.56 at its back, a gap under the mount his third
    // notes saw; vr_adweapon_models prints the handle's top along x.)
    const za::Vector<glm::vec2> mount{{-1.5f, 2.3f}, {1.5f, 2.3f}, {1.5f, 5.8f}, {-1.5f, 5.8f}};
    const Rect mountFaces[4] = {underside, bandAlong, bandAlong, bandAlong};
    extrude(o, mount, 24.f, 31.f, mountFaces, 4, true, false, &dark, &dark);
    const int gripSeed[3] = {151, -1, -1};
    const za::Vector<bool> grip = piecesOf(p.graft, gripSeed);
    const glm::vec3 top{3.3f, 0.f, 4.55f}, to{27.5f, 0.f, 3.2f};
    const za::SizeT handleFrom = o.pos.size();
    take(o, p.graft, grip, true, [&](const glm::vec3& q) { return to + (q - top) * 0.88f; }, p.graftDs, p.graftDt);
    note(p, o, handleFrom, "front handle");

    p.muzzle = glm::vec3{44.f, 0.f, spinZ + spinR}; // (the top barrel's: vr_weapons.inc slot 34's MuzzleAnchor)
    p.muzzleSet = true;
}

// The Plasma Gun's cell receiver (his note 2026-10-18: the cell on the inner side, not below as the lightning gun's): a
// block on its +y side round the cell's way out (plasmaSeat: the cell 3.7 along x, 3.2 tall, turned out of it), AD's
// texels (v_plasma.mdl's 128x128 skin): the riveted panel outside, the rusty plate round it.
// His second notes (2026-10-18): a real hole where the cell goes, seen with it out (it was flat): the block is a frame
// round a pocket a little bigger than the cell (3.9 along x, 3.5 tall), its inner faces and its floor dark.
void plasmaParts(Parts& p)
{
    // His third notes (2026-10-19): the hole's inside darker than the gun outside, so that it reads as going in: black
    // (the bores' texels), not the caps' dark grey; its floor over the gun's own side (y 3.25 there: it was 2.95, under
    // AD's riveted panel, which showed through the hole as the outside does).
    Out& o = p.o;
    const float bs = static_cast<float>(p.boreS), bt = static_cast<float>(p.boreT);
    const Rect dark{bs, bt, bs, bt, false};
    const Rect panel{32.f, 47.f, 64.f, 64.f, false};
    const Rect plate{33.f, 1.f, 64.f, 12.f, true};
    const float x = plasmaSeat.x, z = plasmaSeat.z, y0 = 2.9f, y1 = 4.6f;
    const float bx0 = x - 2.5f, bx1 = x + 2.6f, bz0 = z - 2.1f, bz1 = z + 2.1f; // (the block)
    const float hx0 = x - 1.95f, hx1 = x + 1.95f, hz0 = z - 1.75f, hz1 = z + 1.75f; // (its hole)
    const Rect sides[4] = {plate, panel, plate, plate};
    const Rect below[4] = {plate, panel, dark, plate}; // (its top faces the hole)
    const Rect above[4] = {dark, panel, plate, plate}; // (its underside faces it)
    box(o, {bx0, y0, bz0}, {hx0, y1, bz1}, sides, plate, dark);
    box(o, {hx1, y0, bz0}, {bx1, y1, bz1}, sides, dark, plate);
    box(o, {hx0, y0, bz0}, {hx1, y1, hz0}, below, plate, plate);
    box(o, {hx0, y0, hz1}, {hx1, y1, bz1}, above, plate, plate);
    // Its floor (his third notes: the gun's own side showed through the hole, its riveted panel as outside): just over
    // the gun's surface met looking in at the hole (the highest of a grid of rays in along -y), black.
    float floorY = y0;
    for(int i = 0; i <= 6; i++)
    {
        for(int j = 0; j <= 6; j++)
        {
            const float qx = hx0 + (hx1 - hx0) * static_cast<float>(i) / 6.f, qz = hz0 + (hz1 - hz0) * static_cast<float>(j) / 6.f;
            for(za::SizeT t = 0; t + 2 < o.tris.size(); t += 3)
            {
                const glm::vec3 &a = o.pos[o.tris[t]], &b = o.pos[o.tris[t + 1]], &e = o.pos[o.tris[t + 2]];
                const float den = (b.z - e.z) * (a.x - e.x) + (e.x - b.x) * (a.z - e.z);
                if(za::abs(den) < 1e-9f)
                {
                    continue;
                }
                const float u = ((b.z - e.z) * (qx - e.x) + (e.x - b.x) * (qz - e.z)) / den;
                const float v = ((e.z - a.z) * (qx - e.x) + (a.x - e.x) * (qz - e.z)) / den;
                const float y = u * a.y + v * b.y + (1.f - u - v) * e.y;
                if(u >= 0.f && v >= 0.f && u + v <= 1.f && y < y1 - 0.5f)
                {
                    floorY = za::max(floorY, y);
                }
            }
        }
    }
    const za::SizeT floorFrom = o.pos.size();
    box(o, {hx0, floorY - 0.4f, hz0}, {hx1, floorY + 0.05f, hz1}, {dark, dark, dark, dark}, dark, dark);
    note(p, o, floorFrom, "cell pocket's floor");
}

// The vertices' welding for their normals: at one place (to a 0.001 grid) and in one smoothing group (Out::group).
[[nodiscard]] za::Vector<int> weldGrouped(const za::Vector<glm::vec3>& pos, const za::Vector<int>& group)
{
    ankerl::unordered_dense::map<uint64_t, int> at;
    za::Vector<int> w;
    for(za::SizeT i = 0; i < pos.size(); i++)
    {
        const auto q = [&](float x) { return static_cast<uint64_t>(static_cast<int64_t>(x * 1000.f + (x < 0.f ? -0.5f : 0.5f)) & 0x3ffff); };
        const uint64_t key = q(pos[i].x) | (q(pos[i].y) << 18) | (q(pos[i].z) << 36) | (static_cast<uint64_t>(group[i] & 0x3ff) << 54);
        w.pushBack(at.try_emplace(key, static_cast<int>(i)).first->second);
    }
    return w;
}

[[nodiscard]] bool build(const Recipe& r, Built& out, const char*& why, unsigned int* pathId)
{
    Mesh src;
    if(!load(r.source, r.frame, src, pathId))
    {
        why = "AD's model (the release measured) not found";
        return false;
    }
    Mesh graft;
    if(r.graft.path && !load(r.graft, nullptr, graft, nullptr))
    {
        why = "Quake VR's model to take the grip from not found";
        return false;
    }

    // The grafted pieces' texels: their rectangle of the Quake VR model's skin, copied under AD's.
    const za::Vector<bool> dropped = piecesOf(src, r.drop);
    const za::Vector<bool> grafted = r.graft.path ? piecesOf(graft, r.graftSeeds) : za::Vector<bool>{};
    int rx0 = 1 << 30, ry0 = 1 << 30, rx1 = -1, ry1 = -1;
    for(za::SizeT t = 0; t < grafted.size(); t++)
    {
        if(!grafted[t])
        {
            continue;
        }
        const bool front = graft.tris[t * 4] != 0;
        for(int c = 0; c < 3; c++)
        {
            const int v = graft.tris[t * 4 + 1 + c];
            const int s = graft.st[static_cast<za::SizeT>(v) * 3 + 1] + (graft.st[static_cast<za::SizeT>(v) * 3] && !front ? graft.skinW / 2 : 0);
            const int tt = graft.st[static_cast<za::SizeT>(v) * 3 + 2];
            rx0 = za::min(rx0, s);
            rx1 = za::max(rx1, s);
            ry0 = za::min(ry0, tt);
            ry1 = za::max(ry1, tt);
        }
    }
    const int gw = rx1 >= rx0 ? za::min(rx1, graft.skinW - 1) - rx0 + 1 : 0;
    const int gh = ry1 >= ry0 ? za::min(ry1, graft.skinH - 1) - ry0 + 1 : 0;
    const int skinW = za::max(src.skinW, gw), skinH = src.skinH + gh + capRows;
    za::Vector<byte> skin(static_cast<za::SizeT>(skinW) * static_cast<za::SizeT>(skinH), capColour);
    for(int y = 0; y < src.skinH; y++)
    {
        memcpy(skin.data() + static_cast<size_t>(y) * skinW, src.skin.data() + static_cast<size_t>(y) * src.skinW, static_cast<size_t>(src.skinW));
    }
    for(int y = 0; y < gh; y++)
    {
        memcpy(skin.data() + static_cast<size_t>(src.skinH + y) * skinW, graft.skin.data() + static_cast<size_t>(ry0 + y) * graft.skinW + rx0,
            static_cast<size_t>(gw));
    }

    // AD's pieces kept, laid into the weapon's frame; cut; the grip put on; the holes closed.
    glm::vec3 sx, sy, sz, dx, dy, dz;
    basis(r.srcAxis, r.srcUp, sx, sy, sz);
    basis(r.dstAxis, r.dstUp, dx, dy, dz);
    const auto lay = [&](const glm::vec3& p) {
        const glm::vec3 d = p - r.srcPivot;
        return r.dstPivot + (dx * glm::dot(d, sx) + dy * glm::dot(d, sy) + dz * glm::dot(d, sz)) * r.k;
    };
    Out o;
    if(r.takeSource)
    {
        take(o, src, dropped, false, lay, 0, 0);
        if(r.clip)
        {
            clip(o, r.clipN, r.clipD, 0);
        }
    }
    const za::SizeT adTris = o.tris.size();
    const za::SizeT adPos = o.pos.size();
    if(r.graft.path)
    {
        // The piece of r.liftSeed moved by r.lift (the Plasma Gun's trigger up onto its body), the others as they are.
        const int liftSeeds[3] = {r.liftSeed, -1, -1};
        const za::Vector<bool> lifted = piecesOf(graft, liftSeeds);
        za::Vector<bool> still = grafted;
        for(za::SizeT t = 0; t < still.size(); t++)
        {
            still[t] = still[t] && !lifted[t];
        }
        take(o, graft, still, true, [](const glm::vec3& p) { return p; }, -rx0, src.skinH - ry0);
        take(o, graft, lifted, true, [&](const glm::vec3& p) { return p + r.lift; }, -rx0, src.skinH - ry0);
    }
    // Each grafted piece's box (as laid) and the body's surface over its top (vr_adweapon_models: how it meets the body).
    za::Vector<glm::vec3> tops[3];
    for(int k = 0; k < 3; k++)
    {
        out.seedLo[k] = glm::vec3{1e30f};
        out.seedHi[k] = glm::vec3{-1e30f};
        out.adAbove[k] = 1e30f;
        if(!r.graft.path || r.graftSeeds[k] < 0)
        {
            continue;
        }
        const int one[3] = {r.graftSeeds[k], -1, -1};
        const za::Vector<bool> piece = piecesOf(graft, one);
        const glm::vec3 move = r.graftSeeds[k] == r.liftSeed ? r.lift : glm::vec3{0.f};
        for(za::SizeT t = 0; t < piece.size(); t++)
        {
            for(int c = 0; c < 3 && piece[t]; c++)
            {
                const glm::vec3 q = graft.pos[graft.tris[t * 4 + 1 + c]] + move;
                out.seedLo[k] = glm::min(out.seedLo[k], q);
                out.seedHi[k] = glm::max(out.seedHi[k], q);
            }
        }
        for(za::SizeT t = 0; t < piece.size(); t++)
        {
            for(int c = 0; c < 3 && piece[t]; c++)
            {
                tops[k].pushBack(graft.pos[graft.tris[t * 4 + 1 + c]] + move);
            }
        }
    }
    (void)adPos;
    const za::SizeT graftEnd = o.tris.size();
    out.grafted = static_cast<int>((o.tris.size() - adTris) / 3);
    out.dropped = 0;
    for(const bool d : dropped)
    {
        out.dropped += d;
    }
    // The bores' black: a few texels of the cap rows' corner (the dark grey's are at their other end).
    for(int y = skinH - capRows; y < skinH; y++)
    {
        for(int x = 0; x < 3; x++)
        {
            skin[static_cast<za::SizeT>(y) * static_cast<za::SizeT>(skinW) + static_cast<za::SizeT>(x)] = 0;
        }
    }
    Parts parts{o, graft, -rx0, src.skinH - ry0, skinW - 2, skinH - 2, 1, skinH - 2};
    if(r.parts)
    {
        r.parts(parts);
    }
    out.noteName = parts.noteName;
    out.noteLo = parts.noteLo;
    out.noteHi = parts.noteHi;
    memcpy(out.noteTop, parts.noteTop, sizeof(out.noteTop));
    out.made = static_cast<int>(o.tris.size() / 3) - static_cast<int>(adTris / 3) - out.grafted;
    out.sealed = seal(o, r, skinW - 2, skinH - 2);
    // Straight up from each grafted piece's vertices within 0.3 of its top: the first triangle met that is not a grafted
    // piece's (AD's, a made part, a hole's fan): the least gap's height.
    for(int k = 0; k < 3; k++)
    {
        float best = 1e30f;
        for(const glm::vec3& q : tops[k])
        {
            if(q.z < out.seedHi[k].z - 0.3f)
            {
                continue;
            }
            for(za::SizeT i = 0; i + 2 < o.tris.size(); i += 3)
            {
                if(i >= adTris && i < graftEnd)
                {
                    continue;
                }
                const glm::vec3 &a = o.pos[o.tris[i]], &b = o.pos[o.tris[i + 1]], &e = o.pos[o.tris[i + 2]];
                const float den = (b.y - e.y) * (a.x - e.x) + (e.x - b.x) * (a.y - e.y);
                if(za::abs(den) < 1e-9f)
                {
                    continue;
                }
                const float u = ((b.y - e.y) * (q.x - e.x) + (e.x - b.x) * (q.y - e.y)) / den;
                const float v = ((e.y - a.y) * (q.x - e.x) + (a.x - e.x) * (q.y - e.y)) / den;
                const float z = u * a.z + v * b.z + (1.f - u - v) * e.z;
                if(u >= 0.f && v >= 0.f && u + v <= 1.f && z >= q.z - 0.05f && z - q.z < best)
                {
                    best = z - q.z;
                    out.adAbove[k] = z;
                    out.adGap[k] = z - q.z;
                }
            }
        }
    }
    {
        za::Vector<int> w = weldNear(o.pos, capWeld);
        Out copy = o;
        (void)holes(copy, w, out.openAfter);
    }

    // Only the vertices used; their normals (the welded surface's, area weighted).
    za::Vector<int> used(o.pos.size(), -1);
    Out c;
    for(int& v : o.tris)
    {
        if(used[v] < 0)
        {
            c.spinning = o.spins[v];
            used[v] = c.vert(o.pos[v], o.s[v], o.t[v], o.group[v]);
        }
        v = used[v];
    }
    c.tris = o.tris;
    const za::Vector<int> w = weldGrouped(c.pos, c.group);
    za::Vector<glm::vec3> normal(c.pos.size(), glm::vec3{0.f});
    for(za::SizeT i = 0; i + 2 < c.tris.size(); i += 3)
    {
        const glm::vec3& a = c.pos[c.tris[i]];
        const glm::vec3 n = glm::cross(c.pos[c.tris[i + 2]] - a, c.pos[c.tris[i + 1]] - a);
        for(int k = 0; k < 3; k++)
        {
            normal[w[c.tris[i + k]]] += n;
        }
    }

    // The spin frames (a model with a turning part: spinPoses after the frameCount still ones): its vertices and normals
    // turned about the spin axis, pose k by k of spinStep degrees, clockwise seen from behind (+x ahead, +y left).
    for(const bool sp : c.spins)
    {
        out.spinVerts += sp ? 1 : 0;
    }
    const int poses = frameCount + (out.spinVerts > 0 ? spinPoses : 0);
    const auto posed = [&](int pose, za::SizeT i, const glm::vec3& v, bool point) {
        if(pose < frameCount || !c.spins[i])
        {
            return v;
        }
        const float a = -glm::radians(spinStep * static_cast<float>(pose - frameCount));
        const float ca = za::cos(a), sa = za::sin(a);
        const float y = v.y, z = point ? v.z - spinZ : v.z;
        return glm::vec3{v.x, y * ca - z * sa, (point ? spinZ : 0.f) + y * sa + z * ca};
    };

    // Packed from the Quake VR model's corner (a vertex past it: the corner moved, placement off).
    glm::vec3 lo{1e30f}, hi{-1e30f};
    for(int pose = 0; pose < poses; pose += pose < frameCount ? frameCount : 1)
    {
        for(za::SizeT i = 0; i < c.pos.size(); i++)
        {
            const glm::vec3 p = posed(pose, i, c.pos[i], true);
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
        }
    }
    out.cornerKept = lo.x >= r.corner.x - 1e-4f && lo.y >= r.corner.y - 1e-4f && lo.z >= r.corner.z - 1e-4f;
    lo = glm::min(lo, r.corner);
    out.lo = lo;
    out.hi = hi;
    const glm::vec3 pack = glm::max((hi - lo) / 255.f, glm::vec3{1e-4f});
    float radius = 0.f;
    for(const glm::vec3& p : c.pos)
    {
        radius = za::max(radius, glm::length(p));
    }

    // Where its muzzle is: a gun's front end's middle (its AD part's vertices within half a unit of its front); the axe's
    // blade edge (the AD part's vertices furthest out along the blade's way, within 2.5 units of the furthest: its ends along the handle).
    const bool axe = r.graft.path == nullptr;
    za::SizeT adVerts = 0;
    for(za::SizeT i = 0; i < adTris; i++)
    {
        adVerts = za::max(adVerts, static_cast<za::SizeT>(c.tris[i]) + 1);
    }
    float furthest = -1e30f;
    for(za::SizeT i = 0; i < adVerts; i++)
    {
        furthest = za::max(furthest, axe ? glm::dot(c.pos[i], r.dstUp) : c.pos[i].x);
    }
    int count = 0;
    out.edgeLo = glm::vec3{0.f, 0.f, 1e30f};
    out.edgeHi = glm::vec3{0.f, 0.f, -1e30f};
    out.muzzle = glm::vec3{0.f};
    for(za::SizeT i = 0; i < adVerts; i++)
    {
        const glm::vec3& p = c.pos[i];
        if((axe ? glm::dot(p, r.dstUp) : p.x) < furthest - (axe ? 2.5f : 0.5f))
        {
            continue;
        }
        out.muzzle += p;
        count++;
        if((axe ? glm::dot(p, r.dstAxis) : p.z) < (axe ? glm::dot(out.edgeLo, r.dstAxis) : out.edgeLo.z) || count == 1)
        {
            out.edgeLo = p;
        }
        if((axe ? glm::dot(p, r.dstAxis) : p.z) > (axe ? glm::dot(out.edgeHi, r.dstAxis) : out.edgeHi.z) || count == 1)
        {
            out.edgeHi = p;
        }
    }
    out.muzzle = axe ? out.edgeHi : out.muzzle / static_cast<float>(za::max(count, 1));
    if(parts.muzzleSet)
    {
        out.muzzle = parts.muzzle; // (its made barrels' front's middle)
    }

    za::Vector<byte>& f = out.file;
    f.clear();
    put(f, IDPOLYHEADER);
    put(f, ALIAS_VERSION);
    putf(f, pack.x);
    putf(f, pack.y);
    putf(f, pack.z);
    putf(f, lo.x);
    putf(f, lo.y);
    putf(f, lo.z);
    putf(f, radius);
    putf(f, 0.f);
    putf(f, 0.f);
    putf(f, 0.f);
    put(f, 1);
    put(f, skinW);
    put(f, skinH);
    put(f, static_cast<int>(c.pos.size()));
    put(f, static_cast<int>(c.tris.size() / 3));
    put(f, poses);
    put(f, 0); // synctype
    put(f, 0); // flags
    putf(f, 1.f);
    put(f, 0); // a single skin
    f.emplaceBackRange(skin.data(), skin.size());
    for(za::SizeT i = 0; i < c.pos.size(); i++)
    {
        put(f, 0);
        put(f, c.s[i]);
        put(f, c.t[i]);
    }
    for(za::SizeT i = 0; i + 2 < c.tris.size(); i += 3)
    {
        put(f, 1);
        put(f, c.tris[i]);
        put(f, c.tris[i + 1]);
        put(f, c.tris[i + 2]);
    }
    za::Vector<byte> packed;
    byte bmin[4], bmax[4];
    for(int frame = 0; frame < poses; frame++)
    {
        if(frame == 0 || frame >= frameCount) // (the still frames: one pose)
        {
            packed.clear();
            for(int k = 0; k < 3; k++)
            {
                bmin[k] = 255;
                bmax[k] = 0;
            }
            bmin[3] = bmax[3] = 0;
            for(za::SizeT i = 0; i < c.pos.size(); i++)
            {
                byte b[4];
                const glm::vec3 p = posed(frame, i, c.pos[i], true);
                for(int k = 0; k < 3; k++)
                {
                    const float q = (p[k] - lo[k]) / pack[k];
                    b[k] = static_cast<byte>(za::max(0, za::min(255, static_cast<int>(q + 0.5f))));
                    bmin[k] = za::min(bmin[k], b[k]);
                    bmax[k] = za::max(bmax[k], b[k]);
                }
                const glm::vec3 n = posed(frame, i, normal[w[i]], false);
                b[3] = qvr::mdlbuild::nearestNormal(glm::length(n) > 1e-12f ? glm::normalize(n) : glm::vec3{0.f, 0.f, 1.f});
                packed.emplaceBackRange(b, 4);
            }
        }
        put(f, 0); // a single frame
        f.emplaceBackRange(bmin, 4);
        f.emplaceBackRange(bmax, 4);
        char name[16] = {};
        if(frame < frameCount)
        {
            q_snprintf(name, sizeof(name), "vr%d", frame);
        }
        else
        {
            q_snprintf(name, sizeof(name), "spin%d", frame - frameCount);
        }
        f.emplaceBackRange(reinterpret_cast<const byte*>(name), 16);
        f.emplaceBackRange(packed.data(), packed.size());
    }
    out.verts = static_cast<int>(c.pos.size());
    out.tris = static_cast<int>(c.tris.size() / 3);
    return true;
}

void setf(byte* f, size_t at, float v)
{
    const float le = LittleFloat(v);
    memcpy(f + at, &le, 4);
}

// A side part (sideParts): the file of its source turned a quarter about x round lightSeat (its -z becomes +y: x' = x,
// y' = seat'.y - (z - seat.z), z' = seat'.z + (y - seat.y)) and lightSeat moved onto plasmaSeat; exact (the packed
// bytes: y' = 255 - z, z' = y, their scales swapped), its normals turned with it. Null: not found or not a model.
[[nodiscard]] byte* sideFile(const SidePart& part, unsigned int* path_id)
{
    const char* name = part.name;
    const char* base = part.from;
    byte* f = COM_LoadMallocFile(base, path_id);
    if(!f)
    {
        return nullptr;
    }
    const size_t size = static_cast<size_t>(com_filesize);
    if(size < headerSize || qvr::bytes::le32(f, 0) != IDPOLYHEADER || qvr::bytes::le32(f, 4) != ALIAS_VERSION)
    {
        VR_HeapFree(f);
        return nullptr;
    }
    const int numSkins = qvr::bytes::le32(f, 48), skinW = qvr::bytes::le32(f, 52), skinH = qvr::bytes::le32(f, 56), numVerts = qvr::bytes::le32(f, 60), numTris = qvr::bytes::le32(f, 64),
              numFrames = qvr::bytes::le32(f, 68);
    const glm::vec3 scale{qvr::bytes::lef32(f, 8), qvr::bytes::lef32(f, 12), qvr::bytes::lef32(f, 16)}, origin{qvr::bytes::lef32(f, 20), qvr::bytes::lef32(f, 24), qvr::bytes::lef32(f, 28)};
    const glm::vec3 eye{qvr::bytes::lef32(f, 36), qvr::bytes::lef32(f, 40), qvr::bytes::lef32(f, 44)};
    const glm::vec3 p = lightSeat, s = plasmaSeat;
    const glm::vec3 scale2{scale.x, scale.z, scale.y};
    const glm::vec3 origin2{origin.x - p.x + s.x, s.y + p.z - origin.z - 255.f * scale.z, origin.y - p.y + s.z};
    for(int i = 0; i < 3; i++)
    {
        setf(f, 8 + static_cast<size_t>(i) * 4, scale2[i]);
        setf(f, 20 + static_cast<size_t>(i) * 4, origin2[i]);
    }
    setf(f, 36, eye.x - p.x + s.x);
    setf(f, 40, s.y - (eye.z - p.z));
    setf(f, 44, s.z + (eye.y - p.y));
    const size_t skinBytes = static_cast<size_t>(za::max(skinW, 0)) * static_cast<size_t>(za::max(skinH, 0));
    size_t at = headerSize;
    for(int k = 0; k < numSkins && at + 8 <= size; k++)
    {
        at += qvr::bytes::le32(f, at) == 0 ? 4 + skinBytes : 8 + static_cast<size_t>(za::max(qvr::bytes::le32(f, at + 4), 1)) * (4 + skinBytes);
    }
    at += static_cast<size_t>(numVerts) * 12 + static_cast<size_t>(numTris) * 16;
    const auto turnBox = [&](size_t boxAt) {
        byte* lo = f + boxAt;
        byte* hi = f + boxAt + 4;
        const byte loY = lo[1], loZ = lo[2], hiY = hi[1], hiZ = hi[2];
        lo[1] = static_cast<byte>(255 - hiZ);
        hi[1] = static_cast<byte>(255 - loZ);
        lo[2] = loY;
        hi[2] = hiY;
    };
    float radius = 0.f;
    for(int fr = 0; fr < numFrames && at + 4 <= size; fr++)
    {
        const int type = qvr::bytes::le32(f, at);
        at += 4;
        int members = 1;
        if(type != 0)
        {
            members = za::max(qvr::bytes::le32(f, at), 1);
            turnBox(at + 4);
            at += 4 + 8 + static_cast<size_t>(members) * 4;
        }
        for(int m = 0; m < members; m++)
        {
            const size_t vertsAt = at + 24;
            if(vertsAt + static_cast<size_t>(numVerts) * 4 > size)
            {
                VR_HeapFree(f);
                return nullptr;
            }
            turnBox(at);
            for(int v = 0; v < numVerts; v++)
            {
                byte* b = f + vertsAt + static_cast<size_t>(v) * 4;
                const byte y = b[1], z = b[2];
                b[1] = static_cast<byte>(255 - z);
                b[2] = y;
                const int ni = za::min(static_cast<int>(b[3]), NUMVERTEXNORMALS - 1);
                b[3] = qvr::mdlbuild::nearestNormal(glm::vec3{r_avertexnormals[ni][0], -r_avertexnormals[ni][2], r_avertexnormals[ni][1]});
                const glm::vec3 q{b[0] * scale2.x + origin2.x, b[1] * scale2.y + origin2.y, b[2] * scale2.z + origin2.z};
                radius = za::max(radius, glm::length(q));
            }
            at = vertsAt + static_cast<size_t>(numVerts) * 4;
        }
    }
    setf(f, 32, radius);
    Con_DPrintf("AD weapon model: %s made (%s turned out of the Plasma Gun's side)\n", name, base);
    return f;
}

// A hand's Widowmaker barrels (spinFrame): their turn (degrees, 0..360: 0 the model's own, a barrel on top each 120),
// speed (degrees a second), and, coasting down, the ease from where they were (p0, at speed v0, at t0) to a barrel on top
// (p1) in `ease` seconds (a cubic: v0 at its start, still at its end).
struct Spin
{
    float phase{0.f}, speed{0.f};
    bool coasting{false};
    float p0{0.f}, p1{0.f}, v0{0.f}, ease{0.f};
    double t0{0.0};
    double lastShot{-1.0}, lastTime{-1.0};
    bool held{false};
    glm::vec3 topFront{0.f}, axisFront{0.f}; // as drawn last: the top barrel's front middle, the spin axis's front (info)
};
Spin spins[2];

// Each hand's weapon as spinFrame saw it last (vr_ad_widowmaker_spin_info, when not held).
struct Drawn
{
    bool visible{false};
    const qmodel_t* model{nullptr};
};
Drawn drawn[2];
float artSeen = -1.f;   // vr_ad_widowmaker_art as spinFrame last saw it (-1: not yet)
bool artLoaded = false; // the Widowmaker's model read last was the hand-made one (derivedFile)

// The Widowmaker drawn (its spin frames), checked once a model (and again when it is read anew: vr_model_reload keeps
// the model's slot). Its barrels: three ("spin0".. frames: the made one and the hand-made one, its export_mdl.py), a
// third of a turn a shot, at rest a barrel on top; a model with "twin0".. frames has two (a half turn a shot).
struct SpinModel
{
    const qmodel_t* model{nullptr};
    const void* hdr{nullptr};
    bool spins{false};
    int barrels{3};
};
SpinModel spinModel;

constexpr const char* widowmakerName = "progs/v_shot3.mdl#vr";
constexpr const char* widowmakerArtName = "progs/v_widowmaker.mdl"; // the hand-made model (vr_ad_widowmaker_art)

[[nodiscard]] bool spinsBarrels(const qmodel_t* m)
{
    if(!m || m->type != mod_alias || q_strcasecmp(m->name, widowmakerName))
    {
        return false;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(m)));
    if(m != spinModel.model || hdr != spinModel.hdr)
    {
        spinModel.model = m;
        spinModel.hdr = hdr;
        const bool framesOk = hdr && hdr->numframes == frameCount + spinPoses;
        const bool three = framesOk && !strcmp(hdr->frames[frameCount].name, "spin0");
        const bool two = framesOk && !strcmp(hdr->frames[frameCount].name, "twin0");
        spinModel.spins = three || two;
        spinModel.barrels = two ? 2 : 3;
    }
    return spinModel.spins;
}

// The turn from a barrel to the next (degrees).
[[nodiscard]] float barrelStep()
{
    return 360.f / static_cast<float>(spinModel.barrels);
}

// The barrels' fastest: a barrel a shot.
[[nodiscard]] float spinTop()
{
    return barrelStep() / za::max(vr_ad_widowmaker_refire.value, 0.05f) * za::max(vr_ad_widowmaker_spin.value, 0.f);
}

void advance(Spin& s, int hand, float dt)
{
    const float top = spinTop();
    const float refire = za::max(vr_ad_widowmaker_refire.value, 0.05f);
    const bool driven = top > 0.f && (client::attacking(hand) || (s.lastShot >= 0.0 && cl.time - s.lastShot < refire * 1.25));
    if(driven)
    {
        s.coasting = false;
        s.speed = za::min(top, s.speed + top / za::max(vr_ad_widowmaker_spinup.value, 0.01f) * dt);
        s.phase += s.speed * dt;
    }
    else if(s.speed > 0.f || s.coasting)
    {
        if(!s.coasting)
        {
            // The stop planned: the next barrel on top past the coast at the nominal rate (top over _spindown s), eased
            // there from this speed (as long as a steady slowing would take; at least a short settle).
            const float step = barrelStep();
            const float rate = za::max(top, step) / za::max(vr_ad_widowmaker_spindown.value, 0.05f);
            const float coast = s.speed * s.speed / (2.f * rate);
            s.p0 = s.phase;
            s.p1 = step * static_cast<float>(static_cast<int>((s.phase + coast) / step + 0.999f));
            s.p1 = za::max(s.p1, s.p0);
            s.v0 = s.speed;
            const float d = s.p1 - s.p0;
            s.ease = d <= 1e-3f ? 0.f : za::min(2.f * d / za::max(s.v0, 1e-3f), za::max(vr_ad_widowmaker_spindown.value, 0.3f));
            s.ease = za::max(s.ease, d > 1e-3f ? 0.15f : 0.f);
            s.ease = za::min(s.ease, d > 1e-3f ? 3.f * d / za::max(s.v0, 1e-3f) : 0.f); // (never past it)
            s.t0 = cl.time;
            s.coasting = true;
        }
        const float u = s.ease > 0.f ? za::min(1.f, static_cast<float>(cl.time - s.t0) / s.ease) : 1.f;
        const float d = s.p1 - s.p0, m = s.v0 * s.ease;
        s.phase = s.p0 + d * (3.f * u * u - 2.f * u * u * u) + m * (u - 2.f * u * u + u * u * u);
        s.speed = s.ease > 0.f ? (d * (6.f * u - 6.f * u * u) + m * (1.f - 4.f * u + 3.f * u * u)) / s.ease : 0.f;
        if(u >= 1.f)
        {
            s.phase = s.p1;
            s.speed = 0.f;
            s.coasting = false;
        }
    }
    if(s.phase >= 360.f)
    {
        s.phase -= 360.f;
        s.p0 -= 360.f;
        s.p1 -= 360.f;
    }
}

// The barrels' motor (his notes 2026-10-19: a motor noise ramping up and down with the spin): a looped whine
// (vr/widowmaker_motor.wav, synthesized: Misc/quakevr/make_adweapon_sounds.py) on the hand's own sound channel (QC
// CHAN_HAND, CHAN_HAND2: from the hand, a channel of its own: the shot on the gun's channel never cut off), its pitch and
// volume set each frame from the spin's share of the top speed; it fades out in motorRelease s when the gun goes (dropped,
// holstered) and stops when the barrels rest.
constexpr const char* motorSoundName = "vr/widowmaker_motor.wav";
constexpr float motorPitchLow = 0.45f; // at a crawl; 1 (as made) at the top speed
constexpr float motorRelease = 0.12f;  // s the motor takes to fade to nothing when its gun is gone at once
constexpr float motorRest = 0.005f;    // a volume under this: stopped

struct Motor
{
    sfx_t* sfx{nullptr};
    channel_t* ch[2]{nullptr, nullptr};
    float volume[2]{0.f, 0.f}, pitch[2]{1.f, 1.f}; // as last set
    double lastTime[2]{-1.0, -1.0};
};
Motor motor;

[[nodiscard]] int motorChannel(int hand)
{
    return hand == HAND_OFF ? SND_CHAN_HAND2 : SND_CHAN_HAND;
}

// The channel still playing `hand`'s motor (another sound may have taken it since), or null. The mix done (S_MixJoin).
[[nodiscard]] channel_t* motorPlaying(int hand)
{
    channel_t* ch = motor.ch[hand];
    return ch && motor.sfx && ch->sfx == motor.sfx && ch->entnum == cl.viewentity && ch->entchannel == motorChannel(hand)
               ? ch
               : nullptr;
}

void motorStop(int hand)
{
    if(motor.ch[hand])
    {
        S_MixJoin();
        if(channel_t* ch = motorPlaying(hand))
        {
            ch->sfx = nullptr;
        }
    }
    motor.ch[hand] = nullptr;
    motor.volume[hand] = 0.f;
}

// Every frame: `hand`'s motor as its barrels spin (`s`; not held: at rest).
void motorFrame(int hand, const Spin& s)
{
    const float dt = motor.lastTime[hand] >= 0.0 ? za::clamp(static_cast<float>(cl.time - motor.lastTime[hand]), 0.f, 0.1f) : 0.f;
    motor.lastTime[hand] = cl.time;
    const float top = spinTop();
    const float u = s.held && top > 0.f ? za::clamp(s.speed / top, 0.f, 1.f) : 0.f;
    const float full = za::clamp(vr_ad_widowmaker_motor.value, 0.f, 1.f);
    float volume = cl.paused ? 0.f : full * za::min(1.f, u * 1.25f);
    if(volume < motor.volume[hand] && !s.held) // (gone at once: a short fade, not a cut)
    {
        volume = za::max(volume, motor.volume[hand] - full * dt / motorRelease);
    }
    if(!motor.sfx || volume < motorRest || cl.viewentity <= 0)
    {
        motorStop(hand);
        return;
    }
    if(s.held)
    {
        motor.pitch[hand] = motorPitchLow + (1.f - motorPitchLow) * u;
    }
    S_MixJoin();
    channel_t* ch = motorPlaying(hand);
    if(!ch)
    {
        vec3_t org;
        VectorCopy(cl_entities[cl.viewentity].origin, org);
        S_StartSoundPitch(cl.viewentity, motorChannel(hand), motor.sfx, org, volume, 1.f, motor.pitch[hand]); // (ATTN_NORM)
        motor.ch[hand] = nullptr;
        for(int i = 0; i < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; i++)
        {
            channel_t& c = snd_channels[i];
            if(c.sfx == motor.sfx && c.entnum == cl.viewentity && c.entchannel == motorChannel(hand))
            {
                motor.ch[hand] = &c;
                break;
            }
        }
        ch = motor.ch[hand];
        if(!ch)
        {
            motor.volume[hand] = 0.f; // (no sound system, or not heard: tried again next frame)
            return;
        }
    }
    ch->master_vol = static_cast<int>(volume * 255.f);
    ch->pitch = motor.pitch[hand];
    ch->resampled = 1; // (its end reckoned again at the new rate: snd_mix.c)
    motor.volume[hand] = volume;
}

} // namespace

void fired(int hand)
{
    if(hand == HAND_MAIN || hand == HAND_OFF)
    {
        spins[hand].lastShot = cl.time;
    }
}

void spinFrame(view::ViewEntity (&weapons)[2])
{
    // The Widowmaker's model switched (vr_ad_widowmaker_art: hand-made or made from AD's): read again now
    // (vr_view_diag.cpp modelReload_f), as it is drawn next.
    if(vr_ad_widowmaker_art.value != artSeen)
    {
        if(artSeen >= 0.f)
        {
            Cbuf_InsertText("vr_model_reload \"progs/v_shot3.mdl#vr\"\n"); // (ahead of a script still waiting)
        }
        artSeen = vr_ad_widowmaker_art.value;
    }
    for(int hand = 0; hand < 2; hand++)
    {
        view::ViewEntity& w = weapons[hand];
        Spin& s = spins[hand];
        drawn[hand].visible = w.visible;
        drawn[hand].model = w.ent.model;
        if(!w.visible || !spinsBarrels(w.ent.model))
        {
            s = Spin{};
            motorFrame(hand, s);
            continue;
        }
        const float dt = s.held && s.lastTime >= 0.0 ? za::clamp(static_cast<float>(cl.time - s.lastTime), 0.f, 0.1f) : 0.f;
        s.held = true;
        s.lastTime = cl.time;
        advance(s, hand, dt);
        motorFrame(hand, s);
        float at = vr_ad_widowmaker_spin_at.value >= 0.f ? vr_ad_widowmaker_spin_at.value : s.phase;
        at -= 360.f * static_cast<float>(static_cast<int>(at / 360.f));
        const float k = at / spinStep;
        const int i = static_cast<int>(k);
        w.poseFrame[0] = frameCount + i % spinPoses;
        w.poseFrame[1] = frameCount + (i + 1) % spinPoses;
        w.poseBlend = k - static_cast<float>(i);
        w.zeroBlend = 0.f; // (the slot's blend towards frame 0 would pull the barrels back)
        if(vr_debug_weaponfx.value)
        {
            // (the hand-made model's: its outer barrel's front and its axis, 8.8 up: build_widowmaker.py)
            const bool two = spinModel.barrels == 2;
            s.topFront = view::modelPoint(w, two ? glm::vec3{44.f, -1.72f, 8.8f} : glm::vec3{44.f, 0.f, spinZ + spinR});
            s.axisFront = view::modelPoint(w, glm::vec3{44.f, 0.f, two ? 8.8f : spinZ});
        }
    }
}

void clear()
{
    for(Spin& s : spins)
    {
        s = Spin{};
    }
    spinModel = SpinModel{};
    for(int hand = 0; hand < 2; hand++)
    {
        motorStop(hand);
        motor.lastTime[hand] = -1.0;
    }
}

void prepare()
{
    motor.sfx = S_PrecacheSound(motorSoundName);
}

void spinInfo_f()
{
    hands::State& hs = hands::current();
    for(int hand = 0; hand < 2; hand++)
    {
        const Spin& s = spins[hand];
        if(!s.held)
        {
            const qmodel_t* m = drawn[hand].model;
            const auto* hdr = m && m->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(m))) : nullptr;
            Con_Printf("widowmaker spin hand %d: not held (or no spin frames): drawn %s, visible %d, %d frames%s%s\n", hand,
                m ? m->name : "nothing", drawn[hand].visible ? 1 : 0, hdr ? hdr->numframes : 0,
                hdr && hdr->numframes > frameCount ? ", frame 9 " : "", hdr && hdr->numframes > frameCount ? hdr->frames[frameCount].name : "");
            continue;
        }
        const float k = s.phase / spinStep;
        Con_Printf("widowmaker spin hand %d: turn %.1f deg, speed %.0f deg/s (top %.0f)%s, frames %d %d blend %.2f, trigger %d, "
                   "last shot %.2f s ago\n",
            hand, s.phase, s.speed, spinTop(), s.coasting ? ", coasting" : "", frameCount + static_cast<int>(k) % spinPoses,
            frameCount + (static_cast<int>(k) + 1) % spinPoses, k - static_cast<float>(static_cast<int>(k)),
            client::attacking(hand) ? 1 : 0, s.lastShot < 0.0 ? -1.0 : cl.time - s.lastShot);
        Con_Printf("  motor: %s, volume %.2f, pitch %.2f\n", !motor.sfx ? "no sound" : motor.ch[hand] ? "playing" : "stopped",
            motor.volume[hand], motor.pitch[hand]);
        Con_Printf("  model: %s, %d barrels (%.0f deg a shot), at rest %s\n",
            artLoaded ? "hand-made (progs/v_widowmaker.mdl)" : "made from AD's", spinModel.barrels, barrelStep(),
            !s.coasting && s.speed <= 0.f && fmodf(s.phase, barrelStep()) < 0.01f ? "yes" : "no");
        if(hs.muzzleValid[hand])
        {
            Con_Printf("  muzzle %.2f %.2f %.2f\n", hs.muzzle[hand].x, hs.muzzle[hand].y, hs.muzzle[hand].z);
        }
        if(vr_debug_weaponfx.value)
        {
            Con_Printf("  drawn: the top barrel's front %.2f %.2f %.2f (from the muzzle %.2f), the barrels' middle %.2f %.2f %.2f "
                       "(%.2f)\n",
                s.topFront.x, s.topFront.y, s.topFront.z, glm::distance(s.topFront, hs.muzzle[hand]), s.axisFront.x,
                s.axisFront.y, s.axisFront.z, glm::distance(s.axisFront, hs.muzzle[hand]));
        }
    }
}

void registerCommands()
{
    Cmd_AddCommand("vr_ad_widowmaker_spin_info", spinInfo_f);
}

byte* derivedFile(const char* name, unsigned int* path_id)
{
    for(const SidePart& part : sideParts)
    {
        if(!q_strcasecmp(name, part.name))
        {
            return sideFile(part, path_id);
        }
    }
    const Recipe* r = strstr(name, suffix) ? recipeOf(name) : nullptr;
    if(!r)
    {
        return nullptr;
    }
    // The Widowmaker: the hand-made model (in the same frame, nothing of AD's in it) unless vr_ad_widowmaker_art is 0.
    if(!q_strcasecmp(name, widowmakerName))
    {
        artLoaded = false;
    }
    if(!q_strcasecmp(name, widowmakerName) && vr_ad_widowmaker_art.value != 0.f)
    {
        if(byte* art = COM_LoadMallocFile(widowmakerArtName, path_id))
        {
            Con_DPrintf("AD weapon model: %s is the hand-made %s\n", name, widowmakerArtName);
            artLoaded = true;
            return art;
        }
        Con_DPrintf("AD weapon model: %s: no %s, made from AD's\n", name, widowmakerArtName);
    }
    Built b;
    const char* why = "";
    if(!build(*r, b, why, path_id))
    {
        Con_DPrintf("AD weapon model: %s: %s: %s stands in\n", name, why, r->standIn);
        return COM_LoadMallocFile(r->standIn, path_id);
    }
    byte* out = static_cast<byte*>(VR_HeapMalloc(b.file.size() + 1));
    memcpy(out, b.file.data(), b.file.size());
    out[b.file.size()] = 0;
    com_filesize = static_cast<int>(b.file.size());
    Con_DPrintf("AD weapon model: %s made: %d triangles, %d vertices, %d holes closed\n", name, b.tris, b.verts, b.sealed);
    return out;
}

void info_f()
{
    Con_Printf("(the Widowmaker drawn: %s, vr_ad_widowmaker_art %g; below, the one made from AD's)\n",
        vr_ad_widowmaker_art.value != 0.f ? widowmakerArtName : "made from AD's", vr_ad_widowmaker_art.value);
    for(const Recipe& r : recipes)
    {
        Built b;
        const char* why = "";
        if(!build(r, b, why, nullptr))
        {
            Con_Printf("%s: %s\n", r.name, why);
            continue;
        }
        Con_Printf("%s: %d triangles, %d vertices (%d of AD's off, %d of Quake VR's grip on, %d made), %d holes closed, %d open "
                   "edges left, box %.2f %.2f %.2f .. %.2f %.2f %.2f%s, muzzle %.4f %.4f %.4f\n",
            r.name, b.tris, b.verts, b.dropped, b.grafted, b.made, b.sealed, b.openAfter, b.lo.x, b.lo.y, b.lo.z, b.hi.x, b.hi.y, b.hi.z,
            b.cornerKept ? "" : " (its corner moved: past the settings')", b.muzzle.x, b.muzzle.y, b.muzzle.z);
        if(!r.graft.path)
        {
            Con_Printf("  blade edge %.2f %.2f %.2f .. %.2f %.2f %.2f\n", b.edgeLo.x, b.edgeLo.y, b.edgeLo.z, b.edgeHi.x, b.edgeHi.y, b.edgeHi.z);
        }
        for(int k = 0; k < 3; k++)
        {
            if(r.graft.path && r.graftSeeds[k] >= 0 && b.seedLo[k].x <= b.seedHi[k].x)
            {
                Con_Printf("  grafted piece of vertex %d%s: box %.2f %.2f %.2f .. %.2f %.2f %.2f, AD's surface straight over its "
                           "top %s\n",
                    r.graftSeeds[k], r.graftSeeds[k] == r.liftSeed ? " (lifted)" : "", b.seedLo[k].x, b.seedLo[k].y,
                    b.seedLo[k].z, b.seedHi[k].x, b.seedHi[k].y, b.seedHi[k].z,
                    b.adAbove[k] < 1e29f ? va("at z %.2f (the least gap %.2f)", b.adAbove[k], b.adGap[k]) : "none");
            }
        }
        if(b.noteName)
        {
            Con_Printf("  %s: box %.2f %.2f %.2f .. %.2f %.2f %.2f; its top along x:", b.noteName, b.noteLo.x, b.noteLo.y,
                b.noteLo.z, b.noteHi.x, b.noteHi.y, b.noteHi.z);
            for(const float z : b.noteTop)
            {
                Con_Printf(z > -1e29f ? " %.2f" : " -", z);
            }
            Con_Printf("\n");
        }
        if(b.spinVerts > 0)
        {
            Con_Printf("  spin frames: %d of %d vertices turned, %d frames after the %d still ones\n", b.spinVerts, b.verts,
                spinPoses, frameCount);
        }
    }
}

} // namespace qvr::adweapons
