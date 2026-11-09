// vr_skinao.cpp -- ambient occlusion baked into a model's skin texels at load time (ROUND24.md, "Ammo screens as part of
// the gun": the contact shadows round rivets, sights and edges). See vr_skinao.hpp.
//
// The per-vertex bake (vr_ao.cpp, vr_ao_models) darkens a model's light by its vertices' occlusion: smooth over each
// triangle, it can't draw a shadow smaller than a triangle (a rivet's ring on the face it sits on, the groove under a
// sight). This bake does it per texel: each skin texel a triangle covers (rasterised in the skin's UV space; Quake's
// onseam back halves as drawn: the vertex buffer's own texture coordinates) gives a point on the first pose's surface and
// the triangle's normal; from just off it, the same cosine-weighted rays as the vertex bake (vr_ao_internal.hpp's
// rayDirs, reach and near-hit cut) against the model's own triangles; nearer hits count more. Texels several triangles
// share (mirrored or reused halves of the skin) take the average of their uses; texels no triangle covers take their
// covered neighbours' (two texels out: no dark or bright seams under filtering). The result is kept per skin texture,
// and the skin's 8-bit texels are expanded to colour and darkened as they are uploaded (TexMgr_LoadImage8 calls
// VR_SkinAOApply), so that a palette or gamma change (a reload from the file) keeps it; fullbright texels (palette
// 224..255) are left alone. Only Quake's 8-bit skins (a full-colour replacement skin keeps its own).

#include "vr_skinao.hpp"
#include "vr_ao_internal.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_jobs.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"

#include <glm/glm.hpp>

#include <cstring>

namespace qvr::skinao
{
namespace
{

using ao::detail::RAYS;

// A skin texture's baked visibility (1 open .. 0 shut, 255 a byte), by the texture's name ("progs/v_shot.mdl:frame0").
struct Map
{
    za::String texName;
    int width{0}, height{0};
    za::Vector<unsigned char> vis;
};

struct State
{
    za::Vector<Map> maps;
    double lastSeconds{0.0}; // the last bake's time
    int lastTexels{0};       // and its covered texels
};
State state;

jobs::Site bakeSite{"skin ao bake"};

[[nodiscard]] bool isWeapon(const char* name)
{
    return std::strncmp(name, "progs/v_", 8) == 0 || std::strstr(name, "_on_v_") != nullptr;
}

[[nodiscard]] bool wanted(const char* name)
{
    return isWeapon(name) ? vr_ao_skin_weapons.value != 0.f : vr_ao_skin_models.value != 0.f;
}

// A triangle of the model as drawn: its three vertices (pose 0 positions), its skin coordinates (texels), its normal.
struct Face
{
    glm::vec3 p[3];
    glm::vec2 st[3];
    glm::vec3 n;
    glm::vec3 v0, e1, e2, mid;
    float rad;
    int vi[3]; // its vertices (the model's, not the vertex buffer's)
};

// A texel's use by a face: the point on it and its face.
struct Sample
{
    int texel;
    int face;
    glm::vec3 pos;
};

// The mesh of `hdr`'s first pose, as drawn (the vertex buffer's triangles and texture coordinates).
void faces(const aliashdr_t* hdr, za::Vector<Face>& out)
{
    const auto* base = reinterpret_cast<const byte*>(hdr);
    const auto* tv = reinterpret_cast<const trivertx_t*>(base + hdr->vertexes); // pose 0
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
    const auto* idx = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
    const glm::vec3 scale{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
    const glm::vec3 origin{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
    out.clear();
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        Face f{};
        glm::vec3 nsum{0.f};
        bool ok = true;
        for(int k = 0; k < 3 && ok; k++)
        {
            const int vbo = idx[i + k];
            ok = vbo < hdr->numverts_vbo && desc[vbo].vertindex < hdr->numverts;
            if(!ok)
            {
                break;
            }
            f.vi[k] = desc[vbo].vertindex;
            const trivertx_t& v = tv[desc[vbo].vertindex];
            f.p[k] = glm::vec3{v.v[0], v.v[1], v.v[2]} * scale + origin;
            f.st[k] = {static_cast<float>(desc[vbo].st[0]), static_cast<float>(desc[vbo].st[1])};
            const float* nn = r_avertexnormals[za::min<int>(v.lightnormalindex, NUMVERTEXNORMALS - 1)];
            nsum += glm::vec3{nn[0], nn[1], nn[2]};
        }
        if(!ok)
        {
            continue;
        }
        f.v0 = f.p[0];
        f.e1 = f.p[1] - f.p[0];
        f.e2 = f.p[2] - f.p[0];
        const glm::vec3 c = glm::cross(f.e1, f.e2);
        const float len = glm::length(c);
        if(len < 1e-6f)
        {
            continue;
        }
        f.n = c / len;
        if(glm::dot(f.n, nsum) < 0.f) // (outwards, as its vertices' normals)
        {
            f.n = -f.n;
        }
        f.mid = (f.p[0] + f.p[1] + f.p[2]) * (1.f / 3.f);
        f.rad = za::sqrt(za::max(glm::dot(f.p[0] - f.mid, f.p[0] - f.mid), glm::dot(f.p[1] - f.mid, f.p[1] - f.mid),
            glm::dot(f.p[2] - f.mid, f.p[2] - f.mid)));
        out.pushBack(f);
    }
}

// Each texel centre inside a face (in its skin coordinates), with the point it maps to.
void rasterise(const za::Vector<Face>& fs, int w, int h, za::Vector<Sample>& out)
{
    out.clear();
    for(int fi = 0; fi < static_cast<int>(fs.size()); fi++)
    {
        const Face& f = fs[static_cast<size_t>(fi)];
        const glm::vec2 a = f.st[0], b = f.st[1], c = f.st[2];
        const float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
        if(za::abs(area) < 1e-6f)
        {
            continue;
        }
        const int x0 = za::max(0, static_cast<int>(za::floor(za::min(a.x, b.x, c.x))));
        const int x1 = za::min(w - 1, static_cast<int>(za::floor(za::max(a.x, b.x, c.x))));
        const int y0 = za::max(0, static_cast<int>(za::floor(za::min(a.y, b.y, c.y))));
        const int y1 = za::min(h - 1, static_cast<int>(za::floor(za::max(a.y, b.y, c.y))));
        for(int y = y0; y <= y1; y++)
        {
            for(int x = x0; x <= x1; x++)
            {
                const glm::vec2 q{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
                const float w0 = ((b.x - q.x) * (c.y - q.y) - (c.x - q.x) * (b.y - q.y)) / area;
                const float w1 = ((c.x - q.x) * (a.y - q.y) - (a.x - q.x) * (c.y - q.y)) / area;
                const float w2 = 1.f - w0 - w1;
                constexpr float eps = -1e-4f;
                if(w0 < eps || w1 < eps || w2 < eps)
                {
                    continue;
                }
                out.pushBack({y * w + x, fi, f.p[0] * w0 + f.p[1] * w1 + f.p[2] * w2});
            }
        }
    }
}

// A sample's visibility: the share of its rays (over its face's hemisphere) the model's own triangles don't stop within
// the reach, nearer hits counting more (as the vertex bake).
[[nodiscard]] float visibility(const za::Vector<Face>& fs, const za::Vector<int>& near, const Sample& s, float reach,
    za::Vector<int>& cand)
{
    const Face& self = fs[static_cast<size_t>(s.face)];
    const glm::vec3 n = self.n;
    const float tmin = reach * ao::detail::NEAREST_SHARE;
    const glm::vec3 o = s.pos + n * (reach * ao::detail::LIFT_SHARE);
    cand.clear();
    for(const int t : near) // (the faces within reach of its own face: faceNeighbours)
    {
        if(t == s.face)
        {
            continue;
        }
        const Face& f = fs[static_cast<size_t>(t)];
        const glm::vec3 d = f.mid - s.pos;
        const float r = reach + f.rad;
        if(glm::dot(d, d) > r * r || glm::dot(d, n) < -f.rad)
        {
            continue;
        }
        cand.pushBack(t);
    }
    if(cand.empty())
    {
        return 1.f;
    }
    // Duff et al.'s frame round the normal.
    const float sign = n.z >= 0.f ? 1.f : -1.f;
    const float a = -1.f / (sign + n.z);
    const float b = n.x * n.y * a;
    const glm::vec3 tx{1.f + sign * n.x * n.x * a, sign * b, -sign * n.x};
    const glm::vec3 ty{b, sign + n.y * n.y * a, -n.y};
    float occ = 0.f;
    for(const glm::vec3& l : ao::detail::rayDirs)
    {
        const glm::vec3 d = tx * l.x + ty * l.y + n * l.z;
        float best = reach;
        for(const int t : cand)
        {
            const Face& f = fs[static_cast<size_t>(t)];
            const glm::vec3 pv = glm::cross(d, f.e2);
            const float det = glm::dot(f.e1, pv);
            if(za::abs(det) < 1e-9f)
            {
                continue;
            }
            const float inv = 1.f / det;
            const glm::vec3 sv = o - f.v0;
            const float u = glm::dot(sv, pv) * inv;
            if(u < 0.f || u > 1.f)
            {
                continue;
            }
            const glm::vec3 q = glm::cross(sv, f.e1);
            const float v = glm::dot(d, q) * inv;
            if(v < 0.f || u + v > 1.f)
            {
                continue;
            }
            const float dist = glm::dot(f.e2, q) * inv;
            if(dist > tmin && dist < best)
            {
                best = dist;
            }
        }
        if(best < reach)
        {
            occ += 1.f - best / reach;
        }
    }
    return 1.f - occ / RAYS;
}

// The bake's working buffers (one bake at a time: model loads are on the main thread).
struct Scratch
{
    za::Vector<Face> faces;
    za::Vector<Sample> samples;
    za::Vector<float> sampleVis;
    za::Vector<float> sum;
    za::Vector<int> count;
};
Scratch scratch;

[[nodiscard]] float reachOf(const za::Vector<Face>& fs)
{
    glm::vec3 lo{1e9f}, hi{-1e9f};
    for(const Face& f : fs)
    {
        for(const glm::vec3& p : f.p)
        {
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
        }
    }
    const float r = za::clamp(glm::distance(lo, hi) * ao::detail::REACH_SHARE, ao::detail::REACH_MIN, ao::detail::REACH_MAX);
    return r * za::clamp(vr_ao_skin_radius.value, 0.1f, 4.f);
}

Map* findMap(const char* texName)
{
    for(Map& m : state.maps)
    {
        if(m.texName == texName)
        {
            return &m;
        }
    }
    return nullptr;
}

} // namespace

bool bake(const aliashdr_t* hdr, int w, int h, za::Vector<unsigned char>& vis, BakeInfo* info)
{
    const double t0 = Sys_DoubleTime();
    Scratch& s = scratch;
    faces(hdr, s.faces);
    if(s.faces.empty() || w <= 0 || h <= 0)
    {
        return false;
    }
    rasterise(s.faces, w, h, s.samples);
    const float reach = reachOf(s.faces);
    s.sampleVis.resize(s.samples.size());
    const za::Vector<Face>& fs = s.faces;
    const za::Vector<Sample>& ss = s.samples;
    za::Vector<float>& sv = s.sampleVis;
    // Each face's neighbours: the faces any of its texels' rays could reach (their spheres within the reach of its own).
    za::Vector<za::Vector<int>> near(fs.size());
    jobs::parallelFor(bakeSite, fs.size(), 64, [&](za::SizeT b, za::SizeT e) {
        for(za::SizeT i = b; i < e; i++)
        {
            for(size_t t = 0; t < fs.size(); t++)
            {
                const glm::vec3 d = fs[t].mid - fs[i].mid;
                const float r = reach + fs[t].rad + fs[i].rad;
                if(t != i && glm::dot(d, d) <= r * r)
                {
                    near[i].pushBack(static_cast<int>(t));
                }
            }
        }
    });
    jobs::parallelFor(bakeSite, ss.size(), 256, [&](za::SizeT b, za::SizeT e) {
        za::Vector<int> cand; // (each chunk its own)
        for(za::SizeT i = b; i < e; i++)
        {
            sv[i] = visibility(fs, near[static_cast<size_t>(ss[i].face)], ss[i], reach, cand);
        }
    });

    // Texels several faces share: the average of their uses.
    const size_t n = static_cast<size_t>(w) * h;
    s.sum.assign(n, 0.f);
    s.count.assign(n, 0);
    for(size_t i = 0; i < ss.size(); i++)
    {
        s.sum[static_cast<size_t>(ss[i].texel)] += sv[i];
        s.count[static_cast<size_t>(ss[i].texel)]++;
    }
    int covered = 0;
    for(size_t i = 0; i < n; i++)
    {
        if(s.count[i] > 0)
        {
            s.sum[i] /= static_cast<float>(s.count[i]);
            covered++;
        }
    }
    // Two texels out from the covered ones: their covered neighbours' average (filtering reads past the edges).
    for(int pass = 0; pass < 2; pass++)
    {
        za::Vector<int> grown = s.count;
        za::Vector<float> value = s.sum;
        for(int y = 0; y < h; y++)
        {
            for(int x = 0; x < w; x++)
            {
                const size_t i = static_cast<size_t>(y) * w + x;
                if(s.count[i] > 0)
                {
                    continue;
                }
                float acc = 0.f;
                int k = 0;
                for(int dy = -1; dy <= 1; dy++)
                {
                    for(int dx = -1; dx <= 1; dx++)
                    {
                        const int nx = x + dx, ny = y + dy;
                        if(nx < 0 || ny < 0 || nx >= w || ny >= h)
                        {
                            continue;
                        }
                        const size_t j = static_cast<size_t>(ny) * w + nx;
                        if(s.count[j] > 0)
                        {
                            acc += s.sum[j];
                            k++;
                        }
                    }
                }
                if(k > 0)
                {
                    value[i] = acc / static_cast<float>(k);
                    grown[i] = 1;
                }
            }
        }
        s.count = grown;
        s.sum = value;
    }
    vis.resize(n);
    for(size_t i = 0; i < n; i++)
    {
        vis[i] = static_cast<unsigned char>(s.count[i] > 0 ? za::clamp(s.sum[i] * 255.f + 0.5f, 0.f, 255.f) : 255.f);
    }
    state.lastSeconds = Sys_DoubleTime() - t0;
    state.lastTexels = covered;
    if(info)
    {
        info->seconds = state.lastSeconds;
        info->texels = covered;
        info->samples = static_cast<int>(ss.size());
        info->faces = static_cast<int>(fs.size());
        info->reach = reach;
    }
    return true;
}

const unsigned char* mapFor(const char* texName, int& w, int& h)
{
    const Map* m = findMap(texName);
    if(!m)
    {
        return nullptr;
    }
    w = m->width;
    h = m->height;
    return m->vis.data();
}

void clear()
{
    state.maps.clear();
}

namespace
{

// vr_skinao_test [model] (Debug > Tests > Skin AO Bake): the shotgun's skin baked twice (the same bytes), and the
// texels round its rivets (small separate pieces of the mesh: islands of at most 40 vertices and 4 units across) darker
// than the same faces' texels away from every rivet. Ends "skinao_test: N passed, M failed".
void test_f()
{
    const char* name = Cmd_Argc() > 1 ? Cmd_Argv(1) : "progs/v_shot.mdl";
    testkit::Tally tally;
    const auto check = [&](bool ok, const char* what) {
        Con_Printf("skinao_test: %s: %s\n", ok ? "PASS" : "FAIL", what);
        tally.check(ok);
    };
    qmodel_t* mod = Mod_ForName(name, false);
    const auto* hdr = mod && mod->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(mod)) : nullptr;
    check(hdr != nullptr, va("%s loaded", name));
    if(!hdr)
    {
        tally.verdict("skinao_test");
        return;
    }
    za::Vector<unsigned char> a, b;
    BakeInfo info;
    const bool baked = bake(hdr, hdr->skinwidth, hdr->skinheight, a, &info) && bake(hdr, hdr->skinwidth, hdr->skinheight, b, nullptr);
    check(baked, va("baked: skin %d x %d, %d texels, %d uses, %d faces, reach %.2f, %.1f ms", hdr->skinwidth,
                     hdr->skinheight, info.texels, info.samples, info.faces, info.reach, info.seconds * 1000.0));
    check(baked && a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size()) == 0, "deterministic: two bakes, the same bytes");

    // The rivets: the mesh's small islands (vertices joined by faces).
    const za::Vector<Face>& fs = scratch.faces;
    za::Vector<int> parent(static_cast<size_t>(hdr->numverts));
    for(int i = 0; i < hdr->numverts; i++)
    {
        parent[static_cast<size_t>(i)] = i;
    }
    const auto root = [&](int v) {
        while(parent[static_cast<size_t>(v)] != v)
        {
            parent[static_cast<size_t>(v)] = parent[static_cast<size_t>(parent[static_cast<size_t>(v)])];
            v = parent[static_cast<size_t>(v)];
        }
        return v;
    };
    for(const Face& f : fs)
    {
        parent[static_cast<size_t>(root(f.vi[1]))] = root(f.vi[0]);
        parent[static_cast<size_t>(root(f.vi[2]))] = root(f.vi[0]);
    }
    {
        // Vertices at the same place are one (a skin seam splits a vertex in two): welded by their bytes.
        const auto* pv = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
        za::Vector<int> firstAt(1u << 24, -1);
        for(int i = 0; i < hdr->numverts; i++)
        {
            const size_t key = (static_cast<size_t>(pv[i].v[0]) << 16) | (static_cast<size_t>(pv[i].v[1]) << 8) | pv[i].v[2];
            if(firstAt[key] < 0)
            {
                firstAt[key] = i;
            }
            else
            {
                parent[static_cast<size_t>(root(i))] = root(firstAt[key]);
            }
        }
    }
    struct Island
    {
        glm::vec3 lo{1e9f}, hi{-1e9f};
        int verts{0};
    };
    za::Vector<Island> islands(static_cast<size_t>(hdr->numverts));
    const auto* tv = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const glm::vec3 scale{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
    const glm::vec3 origin{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
    for(int i = 0; i < hdr->numverts; i++)
    {
        Island& is = islands[static_cast<size_t>(root(i))];
        const glm::vec3 p = glm::vec3{tv[i].v[0], tv[i].v[1], tv[i].v[2]} * scale + origin;
        is.lo = glm::min(is.lo, p);
        is.hi = glm::max(is.hi, p);
        is.verts++;
    }
    za::Vector<glm::vec4> rivets; // centre, radius
    for(const Island& is : islands)
    {
        if(is.verts > 0 && is.verts <= 40 && glm::distance(is.lo, is.hi) <= 4.f)
        {
            rivets.pushBack(glm::vec4{(is.lo + is.hi) * 0.5f, glm::distance(is.lo, is.hi) * 0.5f});
        }
    }
    za::Vector<char> rivetFace(fs.size(), 0);
    for(size_t t = 0; t < fs.size(); t++)
    {
        const Island& is = islands[static_cast<size_t>(root(fs[t].vi[0]))];
        rivetFace[t] = is.verts <= 40 && glm::distance(is.lo, is.hi) <= 4.f ? 1 : 0;
    }
    // Around them (on the faces they sit on, within a unit of their edge, on their side) and away (faces turned as
    // those, more than two units from every rivet's edge).
    za::Vector<char> nearFace(fs.size(), 0);
    za::Vector<glm::vec3> nearNormals;
    const auto facingAsNear = [&](const glm::vec3& n) {
        for(const glm::vec3& m : nearNormals)
        {
            if(glm::dot(m, n) > 0.95f)
            {
                return true;
            }
        }
        return false;
    };
    double nearSum = 0.0, farSum = 0.0;
    int nearN = 0, farN = 0;
    const za::Vector<Sample>& ss = scratch.samples;
    for(int pass = 0; pass < 2; pass++)
    {
        for(size_t i = 0; i < ss.size(); i++)
        {
            const size_t t = static_cast<size_t>(ss[i].face);
            if(rivetFace[t])
            {
                continue;
            }
            float nearest = 1e9f;
            bool near = false;
            for(const glm::vec4& r : rivets)
            {
                const glm::vec3 c{r};
                const float d = glm::distance(c, ss[i].pos);
                nearest = za::min(nearest, d - r.w);
                near = near || (d < r.w + 1.f && glm::dot(c - ss[i].pos, fs[t].n) > 0.f);
            }
            if(pass == 0 && near)
            {
                if(!nearFace[t])
                {
                    nearNormals.pushBack(fs[t].n);
                }
                nearFace[t] = 1;
                nearSum += scratch.sampleVis[i];
                nearN++;
            }
            else if(pass == 1 && nearest > 2.f && facingAsNear(fs[t].n))
            {
                farSum += scratch.sampleVis[i];
                farN++;
            }
        }
    }
    const double nearMean = nearN ? nearSum / nearN : 1.0, farMean = farN ? farSum / farN : 0.0;
    check(rivets.size() >= 2, va("rivets found: %d", static_cast<int>(rivets.size())));
    check(nearN > 0 && farN > 0 && nearMean < farMean - 0.03,
        va("round the rivets darker: %.3f (%d texels) vs %.3f away on the same faces (%d)", nearMean, nearN, farMean, farN));
    tally.verdict("skinao_test");
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_skinao_test", test_f);
}

} // namespace qvr::skinao

using namespace qvr;

extern "C" void VR_SkinAOModel(qmodel_t* mod, const void* aliashdr)
{
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    if(!mod || !hdr || hdr->numskins <= 0 || hdr->numindexes < 3 || !skinao::wanted(mod->name))
    {
        return;
    }
    za::Vector<unsigned char> vis;
    skinao::BakeInfo info;
    if(!skinao::bake(hdr, hdr->skinwidth, hdr->skinheight, vis, &info))
    {
        return;
    }
    if(vr_ao_skin_debug.value != 0.f)
    {
        Con_Printf("skinao: %s baked, skin %d x %d, %d texels covered (%d uses, %d faces, reach %.2f) in %.1f ms\n", mod->name,
            hdr->skinwidth, hdr->skinheight, info.texels, info.samples, info.faces, info.reach, info.seconds * 1000.0);
    }
    // Each of its skins' textures given the map, then made again from the file through it (VR_SkinAOApply).
    for(int i = 0; i < hdr->numskins && i < MAX_SKINS; i++)
    {
        for(int j = 0; j < 4; j++)
        {
            gltexture_t* glt = hdr->gltextures[i][j];
            if(!glt || glt->source_format != SRC_INDEXED || (j > 0 && glt == hdr->gltextures[i][j - 1]))
            {
                continue;
            }
            skinao::Map* m = skinao::findMap(glt->name);
            if(!m)
            {
                m = &skinao::state.maps.emplaceBack();
                m->texName = glt->name;
            }
            m->width = hdr->skinwidth;
            m->height = hdr->skinheight;
            m->vis = vis;
            TexMgr_ReloadImage(glt, -1, -1);
        }
    }
}

extern "C" void VR_SkinAOApply(const char* texName, unsigned char* rgba, const unsigned char* src8, int stride, int w, int h)
{
    int mw = 0, mh = 0;
    const unsigned char* vis = skinao::mapFor(texName, mw, mh);
    if(!vis || mw != w || mh != h)
    {
        return;
    }
    const float strength = za::clamp(vr_ao_skin_strength.value, 0.f, 2.f);
    if(strength <= 0.f)
    {
        return;
    }
    for(int y = 0; y < h; y++)
    {
        for(int x = 0; x < w; x++)
        {
            const size_t i = static_cast<size_t>(y) * stride + x;
            if(src8[i] >= 224) // fullbright: lit by itself
            {
                continue;
            }
            const float v = static_cast<float>(vis[static_cast<size_t>(y) * w + x]) * (1.f / 255.f);
            const float k = za::clamp(1.f - strength * (1.f - v), 0.f, 1.f);
            unsigned char* p = rgba + i * 4;
            for(int c = 0; c < 3; c++)
            {
                p[c] = static_cast<unsigned char>(static_cast<float>(p[c]) * k + 0.5f);
            }
        }
    }
}
