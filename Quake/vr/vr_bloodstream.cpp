// vr_bloodstream.cpp -- see vr_bloodstream.hpp.

#include "vr_bloodstream.hpp"
#include "vr_anchor.hpp"
#include "vr_cvars.hpp"
#include "vr_decals.hpp"
#include "vr_dents.hpp"
#include "vr_engine.hpp"
#include "vr_geom.hpp"
#include "vr_gore.hpp"
#include "vr_goresound.hpp"
#include "vr_lines.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"
#include "vr_ragdoll.hpp"
#include "vr_render.hpp"
#include "vr_trace.hpp"
#include "vr_wounds.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/quaternion.hpp>

namespace qvr::bloodstream
{
namespace
{

constexpr int hardMaxStreams = 64;  // vr_gore_streams_max's ceiling (the storage)
constexpr int maxPoints = 56;       // a stream's chain (45 a second, living as long as they fall: under a second)
constexpr float emitRate = 45.f;    // points a second
constexpr float arcStep = 0.1f;     // seconds between the landing's arc samples,
constexpr int arcSteps = 12;        // so many (1.2 s of fall at most)
constexpr double arcInterval = 0.08; // seconds between a stream's landing traces
constexpr double paintInterval = 0.15; // seconds between a stream's marks where it lands
constexpr float heartRate = 1.35f;  // beats a second
constexpr float surfaceInset = 0.15f; // units its start sits under the (dented) surface: no gap at the hole
constexpr int maxDropsAFrame = 12;  // spurts, and splashes, a stream throws in one frame at most
constexpr float splashRate = 45.f;  // splash drops a second at vr_gore_streams_splash 1, at its strongest

za::FastNonCryptoRng rng{static_cast<za::U64>(za::Clock::nowNanoseconds())};

[[nodiscard]] float rnd(float lo, float hi)
{
    return lo + (hi - lo) * rng.getF(0.f, 1.f);
}

struct Point
{
    glm::vec3 pos{0.f}, vel{0.f};
    float age = 0.f, life = 0.f;
    float width = 0.f; // as it left the wound
    float speed0 = 1.f; // its speed then (the taper: thinner as it speeds up)
    float birthSep = 0.f; // how far from the point before it, as it left (the stretch: vr_gore_streams_break)
    bool cut = false;     // not joined to the point before it (the wound jumped)
};

enum class Pose : za::U8
{
    None,
    Animated, // the entity's model as drawn (anchor::posedVertex)
    Ragdoll   // a ragdoll's body (ragdoll::drawnPart)
};

struct Stream
{
    bool used = false;
    bool emitting = false;
    int ent = 0;
    const qmodel_t* model = nullptr;
    // Where on the model: the struck triangle's corners (.mdl vertices) and the point's barycentrics on it (the dents'
    // own placing: wounds::strikeTriangles); or one vertex (all three the same) where no triangle was found.
    int corner[3] = {0, 0, 0};
    glm::vec3 bary{1.f, 0.f, 0.f};
    bool onTriangle = false;
    glm::vec3 restC[3]{glm::vec3{0.f}, glm::vec3{0.f}, glm::vec3{0.f}}; // its corners in the rest pose (the dents')
    glm::vec3 restN{0.f, 0.f, 1.f};
    glm::vec3 surfN{0.f, 0.f, 1.f}; // the surface's outward normal there, as drawn
    float push = 0.f;               // how far the dents push it in there now (vr_gore_streams_info)
    int oldVertex = -1;             // the vertex nearest the hit (the binding before: vr_gore_streams_info)
    Pose pose = Pose::None;
    glm::vec3 outLocal{1.f, 0.f, 0.f}; // the way it pours, in the pose's frame (yaw-only, or the ragdoll body's)
    glm::vec3 outWorld{1.f, 0.f, 0.f};
    glm::vec3 wound{0.f}, lastWound{0.f};
    bool hasLast = false;
    // The body's motion there and the way out, smoothed (vr_gore_streams_smooth): the animation's frame-to-frame jumps
    // don't fling it.
    glm::vec3 smoothVel{0.f};
    glm::vec3 smoothOut{0.f}; // (zero: not yet)
    double start = 0.0;
    float duration = 6.f;
    float power = 1.f;  // how hard it pours (the hit's damage)
    float phase = 0.f;  // its heartbeat's
    float speedMul = 1.f; // its own force (vr_gore_streams_speed_var)
    // Its way out's wander (vr_gore_streams_wobble): two sines a side, each at its own rate (times the wobble rate) and
    // phase.
    float wobFreq[4] = {1.f, 1.f, 1.f, 1.f};
    float wobPhase[4] = {0.f, 0.f, 0.f, 0.f};
    // Its thickness's swell (vr_gore_streams_thickness_var): three sines, each its own rate and phase.
    float thkFreq[3] = {1.f, 1.f, 1.f};
    float thkPhase[3] = {0.f, 0.f, 0.f};
    float emitAcc = 0.f;
    float spurtAcc = 0.f, splashAcc = 0.f; // the drops owed (vr_gore_streams_spurt_rate, _splash)
    float lastStrength = 0.f;
    glm::vec3 lastV0{0.f};      // its way out and speed as last emitted
    // The landing, traced every arcInterval.
    double nextArc = 0.0;
    bool lands = false;
    bool landsOnThing = false; // on a monster, a prop, a door (no pools painted there)
    float landTime = 1.2f;
    glm::vec3 land{0.f}, landN{0.f, 0.f, 1.f};
    glm::vec3 color{0.4f, 0.02f, 0.015f};
    // The marks where it lands.
    double nextPaint = 0.0;
    glm::vec3 poolAt{0.f};
    float poolSize = 0.f;
    int pools = 0;
    int runs = 0;
    glm::vec3 runAt{0.f};
    // The chain: pts[(head - k) mod maxPoints] the k-th newest.
    Point pts[maxPoints];
    int head = -1, count = 0;
};

Stream streams[hardMaxStreams];
double lastTime = -1.0;
lightcache_t light{}; // lightColor's lookup
int spurtsMade = 0, splashesMade = 0; // drops thrown since the map began (vr_decal_count)
double busySeconds = 0.0; // the streams' own time (moving, tracing, drawing, throwing drops) since the last count,
int busyFrames = 0;       // over these frames with any (vr_decal_count)
// The worst stretch drawn since the last vr_gore_streams_info: a piece's length over its length as poured (and as
// gravity spreads it), and in units; the pieces broken or faded for it, and the wound's jumps that cut it.
struct StretchStats
{
    float maxRatio = 0.f, maxLength = 0.f;
    int segments = 0, faded = 0, broken = 0, cuts = 0;
};
StretchStats stretch;
constexpr float jumpDist = 6.f;      // units the wound moves in a frame at least, and
constexpr float jumpSpeed = 400.f;   // as fast as this at least: a jump (a new frame, a turn), not its motion
constexpr float maxBodySpeed = 400.f; // the body's motion it takes at most (units/s)

// Wounds received this frame, started in frame() (the entities posed then).
struct Pending
{
    int ent;
    glm::vec3 at, dir;
    int amount;
};
constexpr int maxPending = 32;
Pending pending[maxPending];
int pendingCount = 0;

// Your melee wounds seen and the streams they opened (blade, blunt, thrown blade: onWound), since the map began; shown
// by vr_gore_streams_info (the rates' tests).
struct MeleeTally
{
    int seen[3];
    int opened[3];
};
MeleeTally meleeTally{};

// The posed vertices (binding a new stream to its nearest): kept, grown once.
za::Vector<glm::vec3> scratchRest, scratchNow, scratchTris;

[[nodiscard]] int maxStreams()
{
    return za::clamp(static_cast<int>(vr_gore_streams_max.value), 0, hardMaxStreams);
}

[[nodiscard]] bool enabled()
{
    return vr_gore_streams.value != 0.f && gore::level() > 0 && cl.worldmodel && maxStreams() > 0;
}

[[nodiscard]] glm::vec3 lightColor(const glm::vec3& p)
{
    vec3_t v = {p.x, p.y, p.z};
    const float l = cl.worldmodel ? static_cast<float>(R_LightPoint(v, 0.f, &light)) : 110.f;
    const float k = za::clamp(za::max(l, 40.f) / 100.f, 0.4f, 1.4f);
    return glm::min(glm::vec3{0.45f, 0.025f, 0.02f} * k, glm::vec3{1.f});
}

[[nodiscard]] bool inLiquid(const glm::vec3& p)
{
    vec3_t v = {p.x, p.y, p.z};
    const mleaf_t* leaf = Mod_PointInLeaf(v, cl.worldmodel);
    return leaf && leaf->contents < CONTENTS_SOLID;
}

[[nodiscard]] glm::vec3 bytePoint(const float m[16], const glm::vec3& v)
{
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12], m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
        m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
}

[[nodiscard]] glm::vec3 yawTurn(const glm::vec3& v, float yawDegrees)
{
    const float a = yawDegrees * 0.0174532925f;
    const float c = za::cos(a), s = za::sin(a);
    return {v.x * c - v.y * s, v.x * s + v.y * c, v.z};
}

// Two unit vectors square to the unit `a` and to each other.
void across(const glm::vec3& a, glm::vec3& u, glm::vec3& w)
{
    u = glm::normalize(glm::cross(a, za::fabs(a.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f}));
    w = glm::cross(a, u);
}

// The unit `a` turned a random way up to `degrees` from itself (evenly over the cone's cap).
[[nodiscard]] glm::vec3 inCone(const glm::vec3& a, float degrees)
{
    if(degrees <= 0.f)
    {
        return a;
    }
    glm::vec3 u, w;
    across(a, u, w);
    const float c = 1.f - rnd(0.f, 1.f) * (1.f - za::cos(degrees * 0.0174532925f));
    const float s = za::sqrt(za::max(0.f, 1.f - c * c));
    const float phi = rnd(0.f, 6.2831853f);
    return glm::normalize(a * c + (u * za::cos(phi) + w * za::sin(phi)) * s);
}

// The model an entity is of: a ragdoll drawn with its skinned model ("#rag") counts as its .mdl.
[[nodiscard]] const qmodel_t* ownModel(const entity_t& e)
{
    const qmodel_t* source = ragdoll::sourceModel(&e);
    return source ? source : e.model;
}

[[nodiscard]] const entity_t* entityOf(int ent, const qmodel_t* model)
{
    if(ent <= 0 || ent >= cl.num_entities || ent == cl.viewentity)
    {
        return nullptr;
    }
    const entity_t& e = cl_entities[ent];
    if(!e.model || e.model->type != mod_alias || (model && ownModel(e) != model))
    {
        return nullptr;
    }
    return &e;
}

// A ragdoll body's vertex `v` in the world, and the body's turn; false if `ent` is not a ragdoll now.
[[nodiscard]] bool ragdollVertex(int ent, int v, glm::vec3& at, glm::quat& rot)
{
    glm::vec3 pos;
    float scale;
    const ragdoll::Rig* rig = nullptr;
    if(!ragdoll::drawnPart(ent, 0, rot, pos, scale, &rig) || !rig || !rig->model || v < 0 || v >= rig->numVerts ||
        static_cast<za::SizeT>(v) >= rig->vertBone.size())
    {
        return false;
    }
    const int bone = rig->vertBone[static_cast<za::SizeT>(v)];
    if(!ragdoll::partShown(ent, bone) || (bone != 0 && !ragdoll::drawnPart(ent, bone, rot, pos, scale)))
    {
        return false;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(rig->model)));
    if(!hdr || v >= hdr->numverts)
    {
        return false;
    }
    const auto* tv = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const trivertx_t& t = tv[v]; // the rest pose (0)
    const glm::vec3 r{t.v[0] * hdr->scale[0] + hdr->scale_origin[0], t.v[1] * hdr->scale[1] + hdr->scale_origin[1],
        t.v[2] * hdr->scale[2] + hdr->scale_origin[2]};
    at = rot * (r * scale) + pos;
    return true;
}

// A triangle's outward normal (the dents' and the wounds' winding).
[[nodiscard]] glm::vec3 outward(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 n = glm::cross(b - a, c - a);
    const float l = glm::length(n);
    return l > 1e-8f ? -n / l : glm::vec3{0.f, 0.f, 1.f};
}

// A Quake .mdl's header, if its triangles and pose 0 can be read (the dents' rest pose); null otherwise.
[[nodiscard]] const aliashdr_t* quakeHeader(const qmodel_t* model)
{
    if(!model || model->type != mod_alias)
    {
        return nullptr;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || !hdr->vertexes || !hdr->indexes || !hdr->meshdesc ||
        hdr->numframes <= 0 || hdr->numverts <= 0 || hdr->numbones)
    {
        return nullptr;
    }
    return hdr;
}

// The stream bound to the triangle of .mdl vertices `c` (header `hdr`) at barycentrics `bary`: its corners' rest places
// (the dents').
void bindCorners(Stream& s, const aliashdr_t& hdr, const int c[3], const glm::vec3& bary)
{
    const auto* rest = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(&hdr) + hdr.vertexes); // pose 0
    for(int k = 0; k < 3; k++)
    {
        s.corner[k] = c[k];
        const trivertx_t& t = rest[c[k]];
        s.restC[k] = glm::vec3{t.v[0] * hdr.scale[0] + hdr.scale_origin[0], t.v[1] * hdr.scale[1] + hdr.scale_origin[1],
            t.v[2] * hdr.scale[2] + hdr.scale_origin[2]};
    }
    s.bary = bary;
    s.restN = outward(s.restC[0], s.restC[1], s.restC[2]);
    s.onTriangle = true;
}

// Whether the stream's corners are all on parts its ragdoll (`rig`) draws.
[[nodiscard]] bool cornersShown(const Stream& s, const ragdoll::Rig& rig)
{
    for(int k = 0; k < (s.onTriangle ? 3 : 1); k++)
    {
        const za::SizeT v = static_cast<za::SizeT>(s.corner[k]);
        if(s.corner[k] < 0 || v >= rig.vertBone.size() || !ragdoll::partShown(s.ent, rig.vertBone[v]))
        {
            return false;
        }
    }
    return true;
}

// Its body a ragdoll (`rig`) that doesn't draw the part its wound was on: bound afresh to the nearest of the ragdoll's
// drawn surface (a triangle on parts shown) to where it pours from, within rebindReach; false if none is (it stops: what
// is falling lands). Counted in vr_gore_streams_info.
constexpr float rebindReach = 32.f;
int rebinds = 0, unbound = 0; // since the map began
[[nodiscard]] bool rebind(Stream& s, const ragdoll::Rig& rig)
{
    const aliashdr_t* hdr = quakeHeader(rig.model);
    if(!hdr || !ragdoll::skinnedVertices(s.ent, scratchNow, nullptr, true))
    {
        unbound++;
        return false;
    }
    bool shown[ragdoll::maxBones];
    for(int b = 0; b < ragdoll::maxBones; b++)
    {
        shown[b] = b < rig.numBones && ragdoll::partShown(s.ent, b);
    }
    const auto* base = reinterpret_cast<const byte*>(hdr);
    const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
    const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
    const glm::vec3 from = s.wound;
    float bestD = rebindReach;
    int best[3] = {-1, -1, -1};
    glm::vec3 bestBary{1.f, 0.f, 0.f};
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        int c[3];
        bool on = true;
        for(int k = 0; k < 3 && on; k++)
        {
            c[k] = mesh[indexes[i + k]].vertindex;
            const za::SizeT v = static_cast<za::SizeT>(c[k]);
            on = v < scratchNow.size() && v < rig.vertBone.size() && rig.vertBone[v] < ragdoll::maxBones && shown[rig.vertBone[v]];
        }
        if(!on || c[0] == c[1] || c[1] == c[2] || c[0] == c[2])
        {
            continue;
        }
        glm::vec3 bary;
        const glm::vec3 q = geom::closestOnTriangle(from, scratchNow[static_cast<za::SizeT>(c[0])],
            scratchNow[static_cast<za::SizeT>(c[1])], scratchNow[static_cast<za::SizeT>(c[2])], &bary);
        const float d = glm::distance(q, from);
        if(d < bestD)
        {
            bestD = d;
            bestBary = bary;
            for(int k = 0; k < 3; k++)
            {
                best[k] = c[k];
            }
        }
    }
    if(best[0] < 0)
    {
        unbound++;
        return false;
    }
    bindCorners(s, *hdr, best, bestBary);
    s.pose = Pose::None; // (its way out taken afresh in the new part's frame: locate)
    rebinds++;
    if(developer.value)
    {
        Con_Printf("bloodstream %d: entity %d's wound bound afresh to its ragdoll, %.1f units off\n",
            static_cast<int>(&s - streams), s.ent, bestD);
    }
    return true;
}

// Vertex `v` of the stream's body as drawn now (a ragdoll's, else the animated model's through matrix `m`); for a
// ragdoll its bone's turn too.
[[nodiscard]] bool vertexNow(const Stream& s, const entity_t& e, bool rag, const float m[16], int v, glm::vec3& at, glm::quat& rot)
{
    if(rag)
    {
        return ragdollVertex(s.ent, v, at, rot);
    }
    at = bytePoint(m, anchor::posedVertex(e, v, 0.f));
    return true;
}

// Where the wound is now, and the way it pours; false if its body is gone. The point on its triangle as drawn, pushed in
// as its dents push the corners (the vertex shader's push: dents::pushAt), a little under it.
[[nodiscard]] bool locate(Stream& s)
{
    const entity_t* e = entityOf(s.ent, s.model);
    if(!e)
    {
        return false;
    }
    glm::quat rot;
    glm::vec3 p[3];
    float m[16]{};
    // A ragdoll now (its pelvis drawn): a wound on a part it doesn't draw (the gun he dropped, its head cut off) bound
    // afresh to its body, or stopped. (It was posed as the animated corpse, left where he died: ROUND22.md, "Blood
    // streams follow the ragdoll".)
    const ragdoll::Rig* rig = nullptr;
    glm::vec3 pos0;
    float scale0;
    const bool ragNow = ragdoll::drawnPart(s.ent, 0, rot, pos0, scale0, &rig) && rig;
    if(ragNow && !cornersShown(s, *rig) && !rebind(s, *rig))
    {
        return false;
    }
    const bool rag = ragdollVertex(s.ent, s.corner[0], p[0], rot);
    if(ragNow && !rag)
    {
        return false;
    }
    if(rag)
    {
        if(s.pose != Pose::Ragdoll)
        {
            s.outLocal = glm::inverse(rot) * s.outWorld; // (it fell: the way it poured, now the body's)
            s.pose = Pose::Ragdoll;
        }
        s.outWorld = rot * s.outLocal;
    }
    else
    {
        if(s.pose == Pose::Ragdoll)
        {
            return false; // (a ragdoll no more: removed, gibbed)
        }
        // (As drawn: a running monster's place and turn along its step, not the last message's, 10-15 units ahead.)
        glm::vec3 drawnAt, drawnTurn;
        render::drawnPlace(*e, drawnAt, drawnTurn);
        if(s.pose != Pose::Animated)
        {
            s.outLocal = yawTurn(s.outWorld, -drawnTurn.y);
            s.pose = Pose::Animated;
        }
        s.outWorld = yawTurn(s.outLocal, drawnTurn.y);
        render::drawnEntityMatrix(*e, e->scale ? e->scale : ENTSCALE_DEFAULT, m);
        p[0] = bytePoint(m, anchor::posedVertex(*e, s.corner[0], 0.f));
    }
    if(!s.onTriangle)
    {
        s.surfN = s.outWorld;
        s.push = 0.f;
        s.wound = p[0];
        return true;
    }
    glm::quat r2;
    for(int k = 1; k < 3; k++)
    {
        if(!vertexNow(s, *e, rag, m, s.corner[k], p[k], r2))
        {
            p[k] = p[0];
        }
    }
    const glm::vec3 n = outward(p[0], p[1], p[2]);
    glm::vec3 at{0.f};
    float push = 0.f;
    for(int k = 0; k < 3; k++)
    {
        const float d = dents::pushAt(e, s.restC[k], s.restN);
        at += s.bary[k] * (p[k] - n * d);
        push += s.bary[k] * d;
    }
    s.surfN = n;
    s.push = push;
    s.wound = at - n * surfaceInset;
    return true;
}

// `ent`'s model's vertices as drawn now (the world) into scratchNow; false if none.
[[nodiscard]] bool posedNow(int ent)
{
    za::Vector<glm::vec3>& now = scratchNow;
    if(!ragdoll::skinnedVertices(ent, now, nullptr, true))
    {
        const entity_t* e = entityOf(ent, nullptr);
        if(!e || !anchor::posedVertices(*e, 0.f, scratchRest, now))
        {
            return false;
        }
        float m[16];
        render::drawnEntityMatrix(*e, e->scale ? e->scale : ENTSCALE_DEFAULT, m);
        for(glm::vec3& p : now)
        {
            p = bytePoint(m, p);
        }
    }
    return !now.empty();
}

// The vertex of scratchNow (posedNow) nearest `at`; -1 if none within `reach`.
[[nodiscard]] int nearestVertex(const glm::vec3& at, float reach)
{
    const za::Vector<glm::vec3>& now = scratchNow;
    int best = -1;
    float bestD = 1e30f;
    for(za::SizeT i = 0; i < now.size(); i++)
    {
        const glm::vec3 d = now[i] - at;
        const float d2 = glm::dot(d, d);
        if(d2 < bestD)
        {
            bestD = d2;
            best = static_cast<int>(i);
        }
    }
    return bestD < reach * reach ? best : -1;
}

// A free slot for a stream on `ent`: past its cap or the cap in all, the weakest's.
[[nodiscard]] Stream* slotFor(int ent)
{
    const int perBody = za::max(1, static_cast<int>(vr_gore_streams_per_body.value));
    const int most = maxStreams();
    int onBody = 0, live = 0;
    Stream* weakestOnBody = nullptr;
    Stream* weakest = nullptr;
    Stream* free = nullptr;
    auto left = [](const Stream& s) { return s.emitting ? s.start + s.duration - cl.time : -1.0; };
    for(int i = 0; i < hardMaxStreams; i++)
    {
        Stream& s = streams[i];
        if(!s.used)
        {
            free = free ? free : (i < most ? &s : nullptr);
            continue;
        }
        live++;
        if(!weakest || left(s) < left(*weakest))
        {
            weakest = &s;
        }
        if(s.ent == ent && s.emitting)
        {
            onBody++;
            if(!weakestOnBody || left(s) < left(*weakestOnBody))
            {
                weakestOnBody = &s;
            }
        }
    }
    if(onBody >= perBody)
    {
        return weakestOnBody;
    }
    if(live < most && free)
    {
        return free;
    }
    return weakest;
}

void start(int ent, const glm::vec3& at, const glm::vec3& dir, int amount, float reach = 24.f)
{
    const entity_t* e = entityOf(ent, nullptr);
    if(!e || !posedNow(ent))
    {
        return;
    }
    const int v = nearestVertex(at, reach);
    // Where the hit's line meets the model as drawn: its triangle (the wound's and the dent's own point).
    const glm::vec3 way = glm::length(dir) > 0.01f ? glm::normalize(dir) : glm::vec3{0.f, 0.f, -1.f};
    const aliashdr_t* hdr = quakeHeader(ownModel(*e));
    int tri = -1;
    glm::vec3 hit{0.f}, hitN{0.f};
    if(hdr)
    {
        const auto* base = reinterpret_cast<const byte*>(hdr);
        const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
        const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
        scratchTris.clear();
        bool fits = true;
        for(int i = 0; i + 2 < hdr->numindexes && fits; i += 3)
        {
            for(int k = 0; k < 3; k++)
            {
                const za::SizeT vi = mesh[indexes[i + k]].vertindex;
                fits = fits && vi < scratchNow.size();
                scratchTris.pushBack(fits ? scratchNow[vi] : glm::vec3{0.f});
            }
        }
        tri = fits ? wounds::strikeTriangles(scratchTris, at, way, hit, hitN) : -1;
        if(tri >= 0 && glm::distance(hit, at) > reach)
        {
            tri = -1;
        }
    }
    if(tri < 0 && v < 0)
    {
        return;
    }
    Stream* slot = slotFor(ent);
    if(!slot)
    {
        return;
    }
    Stream& s = *slot;
    s = Stream{};
    s.used = true;
    s.emitting = true;
    s.ent = ent;
    s.model = ownModel(*e);
    s.oldVertex = v;
    if(tri >= 0)
    {
        const auto* base = reinterpret_cast<const byte*>(hdr);
        const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
        const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
        int c[3];
        for(int k = 0; k < 3; k++)
        {
            c[k] = mesh[indexes[tri + k]].vertindex;
        }
        const za::SizeT t0 = static_cast<za::SizeT>(tri);
        glm::vec3 bary;
        (void)geom::closestOnTriangle(hit, scratchTris[t0], scratchTris[t0 + 1], scratchTris[t0 + 2], &bary);
        bindCorners(s, *hdr, c, bary);
    }
    else
    {
        s.corner[0] = s.corner[1] = s.corner[2] = v;
    }
    for(int i = 0; i < 3; i++)
    {
        s.thkFreq[i] = i == 0 ? rnd(0.55f, 0.9f) : i == 1 ? rnd(1.3f, 2.1f) : rnd(2.9f, 4.4f);
        s.thkPhase[i] = rnd(0.f, 1.f);
    }
    // It pours back toward the shot (the hole faces it) and out from the body's middle, a little up.
    glm::vec3 drawnAt, drawnTurn;
    render::drawnPlace(*e, drawnAt, drawnTurn);
    glm::vec3 radial = at - glm::vec3{drawnAt.x, drawnAt.y, at.z};
    radial = glm::length(radial) > 0.1f ? glm::normalize(radial) : glm::vec3{0.f};
    const glm::vec3 back = glm::length(dir) > 0.01f ? -glm::normalize(dir) : radial;
    glm::vec3 out = back * 0.6f + radial * 0.6f + glm::vec3{rnd(-0.2f, 0.2f), rnd(-0.2f, 0.2f), rnd(0.05f, 0.3f)};
    s.outWorld = glm::length(out) > 0.01f ? glm::normalize(out) : glm::vec3{0.f, 0.f, 1.f};
    // Each its own way (one blast's pellets all share the hit's direction), force and wander.
    s.outWorld = inCone(s.outWorld, za::clamp(vr_gore_streams_spread.value, 0.f, 90.f));
    const float speedVar = za::clamp(vr_gore_streams_speed_var.value, 0.f, 0.9f);
    s.speedMul = rnd(1.f - speedVar, 1.f + speedVar);
    for(int i = 0; i < 4; i++)
    {
        s.wobFreq[i] = (i % 2 == 0) ? rnd(0.7f, 1.3f) : rnd(1.7f, 2.6f);
        s.wobPhase[i] = rnd(0.f, 1.f);
    }
    s.start = cl.time;
    s.duration = za::clamp(vr_gore_streams_time.value, 1.f, 30.f) * rnd(0.75f, 1.25f);
    s.power = za::clamp(0.7f + amount / 40.f, 0.7f, 1.4f);
    s.phase = rnd(0.f, 1.f);
    s.nextArc = 0.0;
    s.nextPaint = cl.time + 0.2;
    if(!locate(s))
    {
        s.used = false;
    }
}

// How strongly it pours at age `t`: a gush, weakening, beating.
[[nodiscard]] float flow(const Stream& s, float t)
{
    const float u = za::clamp(t / s.duration, 0.f, 1.f);
    const float envelope = za::pow(1.f - u, 1.4f) * (t < 0.4f ? 1.35f - 0.35f * t / 0.4f : 1.f);
    const float beat = za::sin((t * heartRate + s.phase) * 6.2831853f);
    const float pulse = 0.55f + 0.45f * za::max(0.f, beat) * za::max(0.f, beat);
    return envelope * pulse;
}

[[nodiscard]] float gravity()
{
    return za::max(100.f, sv_gravity.value);
}

// The arc from the wound at `v0`: where it lands, when.
void traceArc(Stream& s, const glm::vec3& v0)
{
    const float g = gravity();
    glm::vec3 prev = s.wound;
    s.lands = false;
    s.landsOnThing = false;
    s.landTime = arcStep * arcSteps;
    // (The things in its way through the local server: monsters, props, doors; never its own body nor you.)
    const edict_t* you = sv.active && svs.maxclients >= 1 ? svs.clients[0].edict : nullptr;
    for(int i = 1; i <= arcSteps; i++)
    {
        const float t = arcStep * static_cast<float>(i);
        const glm::vec3 p = s.wound + v0 * t + glm::vec3{0.f, 0.f, -0.5f * g * t * t};
        glm::vec3 where, n;
        float f = 1.f;
        const bool world = decals::trace(prev, p, where, n, f);
        const glm::vec3 to = world ? where : p;
        if(you)
        {
            const za::Optional<trace_t> tr = worldtrace::move(prev, glm::vec3{0.f}, glm::vec3{0.f}, to, MOVE_NORMAL, s.ent);
            if(tr && tr->fraction < 1.f && !tr->startsolid && tr->ent && tr->ent != sv.qcvm.edicts && tr->ent != you)
            {
                s.lands = true;
                s.landsOnThing = true;
                s.landTime = t - arcStep * (1.f - tr->fraction * f);
                s.land = worldtrace::endPos(*tr);
                s.landN = worldtrace::normal(*tr);
                return;
            }
        }
        if(world)
        {
            s.lands = true;
            s.landTime = t - arcStep * (1.f - f);
            s.land = where;
            s.landN = n;
            return;
        }
        prev = p;
    }
}

// Its thickness now (times its width) at age `t`: a swell and a thinning, unevenly (vr_gore_streams_thickness_var,
// _rate); `n` the swell's -1..1.
[[nodiscard]] float thickness(const Stream& s, float t, float& n)
{
    n = 0.f;
    const float amount = za::clamp(vr_gore_streams_thickness_var.value, 0.f, 1.f);
    if(amount <= 0.f)
    {
        return 1.f;
    }
    const float rate = za::clamp(vr_gore_streams_thickness_rate.value, 0.f, 10.f);
    auto wave = [&](int i) { return za::sin((t * rate * s.thkFreq[i] + s.thkPhase[i]) * 6.2831853f); };
    n = 0.5f * wave(0) + 0.3f * wave(1) + 0.2f * wave(2);
    return za::max(0.08f, 1.f + amount * n);
}

// The base drop size (vr_gore_streams_spurt_size).
[[nodiscard]] float dropSize()
{
    return za::clamp(vr_gore_streams_spurt_size.value, 0.02f, 3.f);
}

// Small drops spurting from the wound in a cone round its way out (vr_gore_streams_spurt*): `axis` and `speed` the
// stream's as it leaves, `bodyVel` the body's motion they take (smoothed, vr_gore_streams_inherit of it).
void spurt(Stream& s, float dt, float strength, const glm::vec3& axis, float speed, const glm::vec3& bodyVel)
{
    if(!vr_gore_streams_spurts.value)
    {
        s.spurtAcc = 0.f;
        return;
    }
    s.spurtAcc += dt * za::clamp(vr_gore_streams_spurt_rate.value, 0.f, 1000.f) * za::min(strength, 1.3f);
    const int n = za::min(maxDropsAFrame, static_cast<int>(s.spurtAcc));
    s.spurtAcc = za::min(s.spurtAcc - static_cast<float>(n), 1.f);
    const float cone = za::clamp(vr_gore_streams_spurt_cone.value, 0.f, 90.f);
    const float k = za::clamp(vr_gore_streams_spurt_speed.value, 0.f, 4.f);
    const float life = za::clamp(vr_gore_streams_spurt_life.value, 0.02f, 5.f);
    const float outliers = za::clamp(vr_gore_streams_spurt_outliers.value, 0.f, 1.f);
    const float outlierLife = za::clamp(vr_gore_streams_spurt_outlier_life.value, 0.02f, 10.f);
    const float floorZ = s.lands && s.landN.z > 0.6f && !s.landsOnThing ? s.land.z + 0.2f : s.wound.z - 512.f;
    for(int i = 0; i < n; i++)
    {
        const glm::vec3 d = inCone(axis, cone);
        float sp = speed * k * rnd(0.45f, 1.3f);
        float l = life * rnd(0.5f, 1.5f);
        float size = dropSize() * rnd(0.6f, 1.4f);
        if(rnd(0.f, 1.f) < outliers) // (a few fly further and last)
        {
            l = outlierLife * rnd(0.7f, 1.3f);
            sp *= rnd(1.f, 1.4f);
            size *= 1.25f;
        }
        particles::bloodDroplet(s.wound + d * 0.3f, d * sp + bodyVel, size, l, s.color, floorZ);
        spurtsMade++;
    }
}

// Drops splashing away where it lands (vr_gore_streams_splash): `vin` the stream's velocity as it gets there.
void splash(Stream& s, float dt, float strength, const glm::vec3& vin, float width)
{
    const float amount = za::clamp(vr_gore_streams_splash.value, 0.f, 10.f);
    if(amount <= 0.f || !s.lands || inLiquid(s.land + s.landN * 2.f))
    {
        s.splashAcc = 0.f;
        return;
    }
    s.splashAcc += dt * splashRate * amount * za::min(strength, 1.3f);
    const int n = za::min(maxDropsAFrame, static_cast<int>(s.splashAcc));
    s.splashAcc = za::min(s.splashAcc - static_cast<float>(n), 1.f);
    if(n <= 0)
    {
        return;
    }
    const glm::vec3 nrm = s.landN;
    glm::vec3 u, w;
    across(nrm, u, w);
    const float into = za::fabs(glm::dot(vin, nrm));
    const glm::vec3 along = vin - nrm * glm::dot(vin, nrm);
    const float speed = za::min(glm::length(vin), 300.f);
    const float size = dropSize() * za::clamp(vr_gore_streams_splash_size.value, 0.f, 4.f);
    const float floorZ = nrm.z > 0.6f ? s.land.z - 0.3f : s.land.z - 512.f;
    const float outliers = za::clamp(vr_gore_streams_spurt_outliers.value, 0.f, 1.f);
    for(int i = 0; i < n && size > 0.f; i++)
    {
        const float a = rnd(0.f, 6.2831853f);
        const glm::vec3 side = u * za::cos(a) + w * za::sin(a);
        const glm::vec3 vel = nrm * (rnd(0.12f, 0.4f) * za::min(into, 300.f) + rnd(8.f, 20.f)) + along * rnd(0.1f, 0.35f) +
                              side * speed * rnd(0.08f, 0.35f);
        const float l = rnd(0.12f, 0.32f) * (rnd(0.f, 1.f) < outliers ? 2.f : 1.f);
        particles::bloodDroplet(s.land + nrm * 0.35f + side * rnd(0.f, width), vel, size * rnd(0.5f, 1.2f), l, s.color, floorZ);
        splashesMade++;
    }
}

// Where it lands: a pool spreading (a new one when the stream has moved off it), drops round it, specks; on a wall a
// run down it.
void paint(Stream& s, float strength)
{
    if(!s.lands || inLiquid(s.land + s.landN * 2.f))
    {
        return;
    }
    const float size = za::clamp(vr_gore_size.value, 0.25f, 4.f) * za::clamp(vr_gore_streams_pool.value, 0.f, 4.f);
    particles::bloodSpecks(s.land, s.landN, 1, s.color);
    if(size <= 0.f || s.landsOnThing) // (no pools on a monster or a prop: its splash only)
    {
        return;
    }
    if(s.landN.z > 0.6f)
    {
        const bool moved = s.pools == 0 || glm::distance(s.land, s.poolAt) > s.poolSize * 0.35f;
        const int most = za::max(1, static_cast<int>(vr_gore_streams_pools.value));
        if(moved && s.pools < most)
        {
            decals::MarkOptions o;
            o.grow = za::max(1.5f, static_cast<float>(s.start + s.duration - cl.time)) + 3.f;
            o.growFrom = 0.12f;
            o.darken = 0.35f;
            const float pool = rnd(16.f, 24.f) * size * za::sqrt(za::max(0.3f, strength));
            if(decals::place(decals::Mark::Pool, s.land, s.landN, pool, o))
            {
                s.poolAt = s.land;
                s.poolSize = pool;
                s.pools++;
            }
        }
        else if(moved && rnd(0.f, 1.f) < 0.5f)
        {
            decals::place(decals::Mark::Drop, s.land, s.landN, rnd(2.5f, 4.f) * size);
        }
    }
    else if(za::fabs(s.landN.z) < 0.35f && s.runs < 3 && (s.runs == 0 || glm::distance(s.land, s.runAt) > 6.f))
    {
        gore::wallRun(s.land, s.landN, rnd(3.f, 5.f) * size);
        s.runAt = s.land;
        s.runs++;
    }
}

// Its way out at age `t`, wandering (vr_gore_streams_wobble): slow smooth noise, two sines a side.
[[nodiscard]] glm::vec3 wobbled(const Stream& s, float t)
{
    const float amount = za::clamp(vr_gore_streams_wobble.value, 0.f, 45.f) * 0.0174532925f;
    if(amount <= 0.f)
    {
        return s.outWorld;
    }
    const float rate = za::clamp(vr_gore_streams_wobble_rate.value, 0.f, 10.f);
    auto wave = [&](int i) { return za::sin((t * rate * s.wobFreq[i] + s.wobPhase[i]) * 6.2831853f); };
    const float a = amount * (0.65f * wave(0) + 0.35f * wave(1));
    const float b = amount * (0.65f * wave(2) + 0.35f * wave(3));
    glm::vec3 u, w;
    across(s.outWorld, u, w);
    return glm::normalize(s.outWorld + u * a + w * b);
}

void update(Stream& s, float dt)
{
    const bool posed = s.emitting && locate(s);
    if(!posed)
    {
        s.emitting = false;
    }
    const float age = static_cast<float>(cl.time - s.start);
    float strength = 0.f;
    if(s.emitting)
    {
        strength = flow(s, age);
        if(age >= s.duration || strength < 0.02f)
        {
            s.emitting = false;
        }
    }
    if(s.emitting)
    {
        // The body's motion at the wound: clamped, low-passed over vr_gore_streams_smooth (the animation moves it in
        // jumps, a new velocity each frame of it), and only vr_gore_streams_inherit of it taken.
        const float smooth = za::clamp(vr_gore_streams_smooth.value, 0.f, 2.f);
        const float follow = smooth > 0.f && dt > 0.f ? 1.f - za::exp(-dt / smooth) : 1.f;
        const float breakAt = za::clamp(vr_gore_streams_break.value, 0.f, 100.f);
        bool jumped = false;
        if(s.hasLast && dt > 0.f)
        {
            const glm::vec3 moved = s.wound - s.lastWound;
            const float d = glm::length(moved);
            jumped = breakAt > 0.f && d > za::max(jumpDist, jumpSpeed * dt);
            glm::vec3 raw = jumped ? s.smoothVel : moved / dt; // (a jump is not a motion: kept as it was)
            const float l = glm::length(raw);
            raw *= l > maxBodySpeed ? maxBodySpeed / l : 1.f;
            s.smoothVel += (raw - s.smoothVel) * follow;
        }
        const glm::vec3 bodyVel = s.smoothVel * za::clamp(vr_gore_streams_inherit.value, 0.f, 1.5f);
        // Its thickness swells and thins as it pours (and so along it as it falls): a gush comes out a little faster.
        float swell = 0.f;
        const float thick = thickness(s, age, swell);
        const float thickAmount = za::clamp(vr_gore_streams_thickness_var.value, 0.f, 1.f);
        const float speed = za::clamp(vr_gore_streams_speed.value, 0.f, 400.f) * s.power * s.speedMul *
                            (0.35f + 0.9f * strength) * (1.f + 0.2f * thickAmount * swell);
        // Its way out, turned with the body (and wandering), smoothed as the motion is: a turn or a new frame of the
        // animation swings it, not snaps it.
        const glm::vec3 aim = wobbled(s, age);
        if(glm::dot(s.smoothOut, s.smoothOut) < 0.25f || follow >= 1.f)
        {
            s.smoothOut = aim;
        }
        else
        {
            const glm::vec3 m = s.smoothOut + (aim - s.smoothOut) * follow;
            s.smoothOut = glm::length(m) > 0.01f ? glm::normalize(m) : aim;
        }
        const glm::vec3 out = s.smoothOut;
        const glm::vec3 v0 = out * speed + bodyVel;
        s.lastV0 = v0;
        s.lastStrength = strength;
        if(cl.time >= s.nextArc || cl.time < s.nextArc - 1.0)
        {
            s.nextArc = cl.time + arcInterval;
            traceArc(s, v0);
            s.color = lightColor(s.wound);
        }
        // New points: from where the wound was over the frame (an even chain however fast the frames).
        s.emitAcc += dt * emitRate;
        const float width =
            za::clamp(vr_gore_streams_width.value, 0.05f, 4.f) * s.power * (0.45f + 0.55f * za::min(1.f, strength)) * thick;
        if(jumped && s.count > 0)
        {
            s.emitAcc = za::max(s.emitAcc, 1.f); // (a point at once where the wound is now, the stream cut there)
            stretch.cuts++;
        }
        const int n = za::min(8, static_cast<int>(s.emitAcc));
        s.emitAcc = za::min(s.emitAcc - static_cast<float>(n), 1.f);
        for(int k = 0; k < n; k++)
        {
            // (Not along the jump: the wound was never in between.)
            const float back = n > 1 && !jumped ? static_cast<float>(n - 1 - k) / static_cast<float>(n) : 0.f;
            const Point* older = s.count > 0 ? &s.pts[s.head] : nullptr;
            s.head = (s.head + 1) % maxPoints;
            s.count = za::min(s.count + 1, maxPoints);
            Point& p = s.pts[s.head];
            p.pos = s.hasLast ? glm::mix(s.wound, s.lastWound, back) : s.wound;
            p.vel = v0;
            p.age = back * dt;
            p.life = s.landTime;
            p.width = width * (1.f + thickAmount * rnd(-0.12f, 0.12f)); // (an uneven edge)
            p.speed0 = za::max(10.f, glm::length(v0));
            p.pos += p.vel * p.age;
            p.cut = jumped && k == 0;
            p.birthSep = older && older != &p ? glm::distance(p.pos, older->pos) : 0.f;
        }
        spurt(s, dt, strength, out, speed, bodyVel);
        if(cl.time >= s.nextPaint)
        {
            s.nextPaint = cl.time + paintInterval;
            if(age > s.landTime) // (the first of it has got there)
            {
                paint(s, strength);
            }
        }
        s.lastWound = s.wound;
        s.hasLast = true;
    }
    // Where it lands, while it gets there (the last of it too, after it stopped pouring).
    if(s.lands && s.count > 0 && (!s.emitting || age > s.landTime))
    {
        const glm::vec3 vin = s.lastV0 + glm::vec3{0.f, 0.f, -gravity() * s.landTime};
        const float w = za::clamp(vr_gore_streams_width.value, 0.05f, 4.f) * s.power;
        splash(s, dt, s.emitting ? s.lastStrength : s.lastStrength * 0.5f, vin, w);
    }

    // The chain falls.
    const float g = gravity();
    for(int k = 0; k < s.count; k++)
    {
        Point& p = s.pts[(s.head - k + maxPoints) % maxPoints];
        p.age += dt;
        p.pos += p.vel * dt;
        p.vel.z -= g * dt;
    }
    while(s.count > 0 && s.pts[(s.head - s.count + 1 + maxPoints) % maxPoints].age >= s.pts[(s.head - s.count + 1 + maxPoints) % maxPoints].life)
    {
        s.count--; // the oldest landed
    }
    if(!s.emitting && s.count == 0)
    {
        s.used = false;
    }
}

void draw(const Stream& s)
{
    const glm::vec4 c{s.color, 0.95f};
    const float breakAt = za::clamp(vr_gore_streams_break.value, 0.f, 100.f);
    const float g = gravity();
    const Point* newer = nullptr;
    for(int k = 0; k < s.count; k++)
    {
        const Point& p = s.pts[(s.head - k + maxPoints) % maxPoints];
        if(p.age >= p.life)
        {
            newer = nullptr; // (landed: a gap)
            continue;
        }
        const float taper = za::clamp(za::sqrt(p.speed0 / za::max(1.f, glm::length(p.vel))), 0.35f, 1.f);
        const float w = p.width * taper;
        if(newer && newer->cut)
        {
            // (The wound jumped there: not joined.)
        }
        else if(newer)
        {
            // Its stretch: the piece's length over its length as poured, as gravity spreads it (the older point
            // falling faster all along); pieces of it poured at a different motion of the body fly apart, a ribbon.
            const float len = glm::distance(newer->pos, p.pos);
            const float spread = g * za::max(0.f, p.age - newer->age) * newer->age;
            const float ratio = len / (newer->birthSep + spread + 1.f);
            stretch.segments++;
            stretch.maxRatio = za::max(stretch.maxRatio, ratio);
            stretch.maxLength = za::max(stretch.maxLength, len);
            float fade = 1.f;
            if(breakAt > 0.f && ratio > 0.5f * breakAt)
            {
                fade = za::clamp((breakAt - ratio) / (0.5f * breakAt), 0.f, 1.f);
                stretch.faded += fade > 0.f;
                stretch.broken += fade <= 0.f;
            }
            if(fade > 0.f)
            {
                const glm::vec4 cf{s.color, 0.95f * fade};
                lines::sceneLine(newer->pos, p.pos, w * (0.5f + 0.5f * fade), cf, cf);
            }
        }
        else if(k == 0 && s.emitting)
        {
            lines::sceneLine(s.wound, p.pos, w, c, c); // (joined to the wound)
        }
        newer = &p;
    }
}

} // namespace

void onWound(int ent, const glm::vec3& at, const glm::vec3& dir, int kind, int amount, int melee)
{
    // Bullet holes: a shot or a pellet (1), a nail or a small missile (2); and your melee (3: a blade's or a blunt blow
    // in your hand, a blade you threw), each kind at its own chance.
    if((kind != 1 && kind != 2 && !(kind == 3 && melee != MeleeNone)) || !enabled() || ent == cl.viewentity)
    {
        return;
    }
    const cvar_t& chance = kind != 3              ? vr_gore_streams_chance
                           : melee == MeleeBlade  ? vr_gore_streams_melee_blade
                           : melee == MeleeBlunt  ? vr_gore_streams_melee_blunt
                                                  : vr_gore_streams_thrown_blade;
    const int tally = kind == 3 ? melee - 1 : -1;
    if(tally >= 0)
    {
        meleeTally.seen[tally]++;
    }
    if(rnd(0.f, 1.f) >= za::clamp(chance.value, 0.f, 1.f))
    {
        return;
    }
    if(tally >= 0)
    {
        meleeTally.opened[tally]++;
    }
    if(pendingCount < maxPending)
    {
        pending[pendingCount++] = {ent, at, dir, amount};
    }
}

void frame()
{
    QVR_PROFILE("bloodstreams");
    const double dtd = lastTime < 0.0 ? 0.0 : cl.time - lastTime;
    lastTime = cl.time;
    if(!enabled())
    {
        pendingCount = 0;
        for(Stream& s : streams)
        {
            s.emitting = false; // (those pouring stop; what is falling lands)
        }
        if(!cl.worldmodel)
        {
            clear();
            return;
        }
    }
    for(int i = 0; i < pendingCount; i++)
    {
        start(pending[i].ent, pending[i].at, pending[i].dir, pending[i].amount);
    }
    pendingCount = 0;
    const float dt = static_cast<float>(za::clamp(dtd, 0.0, 0.1));
    const double t0 = Sys_DoubleTime();
    bool any = false;
    for(Stream& s : streams)
    {
        if(s.used)
        {
            any = true;
            update(s, dt);
            if(s.used)
            {
                draw(s);
                // Pouring onto the floor (not a thing, not a liquid): its splatter, as loud as it flows (vr_goresound.cpp:
                // the nearest few).
                if(s.emitting && s.lands && !s.landsOnThing && s.landN.z > 0.6f && !inLiquid(s.land + s.landN * 2.f))
                {
                    goresound::pour(static_cast<int>(&s - streams), s.land, za::clamp(s.lastStrength * s.power, 0.f, 1.f));
                }
            }
        }
    }
    if(any)
    {
        busySeconds += Sys_DoubleTime() - t0;
        busyFrames++;
    }
}

void clear()
{
    for(Stream& s : streams)
    {
        s.used = false;
        s.emitting = false;
        s.count = 0;
    }
    pendingCount = 0;
    lastTime = -1.0;
    spurtsMade = splashesMade = 0;
    meleeTally = {};
    rebinds = unbound = 0;
}

void count()
{
    int live = 0, pouring = 0, points = 0;
    float thin = 1e30f, thick = 0.f;
    for(const Stream& s : streams)
    {
        live += s.used ? 1 : 0;
        pouring += s.used && s.emitting ? 1 : 0;
        points += s.used ? s.count : 0;
        for(int k = 0; s.used && k < s.count; k++)
        {
            const float w = s.pts[(s.head - k + maxPoints) % maxPoints].width;
            thin = za::min(thin, w);
            thick = za::max(thick, w);
        }
    }
    Con_Printf("bloodstreams: %d (%d pouring), %d points (%.2f-%.2f wide as they left), %d spurt and %d splash drops so far\n",
        live, pouring, points, points ? thin : 0.f, thick, spurtsMade, splashesMade);
    Con_Printf("bloodstreams: %.1f us a frame (over the %d frames with any since the last count)\n",
        busyFrames ? busySeconds * 1e6 / busyFrames : 0.0, busyFrames);
    busySeconds = 0.0;
    busyFrames = 0;
}

void forget(int ent, bool drop)
{
    for(Stream& s : streams)
    {
        if(s.used && s.ent == ent)
        {
            s.emitting = false; // (what is falling lands)
            if(drop)
            {
                s.used = false;
                s.count = 0;
            }
        }
    }
    int kept = 0;
    for(int i = 0; i < pendingCount; i++)
    {
        if(pending[i].ent != ent)
        {
            pending[kept++] = pending[i];
        }
    }
    pendingCount = kept;
}

void info_f()
{
    Con_Printf("bloodstreams: melee since the map began: blade %d of %d, blunt %d of %d, thrown blade %d of %d wounds opened one\n",
        meleeTally.opened[0], meleeTally.seen[0], meleeTally.opened[1], meleeTally.seen[1], meleeTally.opened[2],
        meleeTally.seen[2]);
    int shown = 0;
    for(const Stream& s : streams)
    {
        if(!s.used || !s.emitting)
        {
            continue;
        }
        const entity_t* e = entityOf(s.ent, s.model);
        if(!e)
        {
            continue;
        }
        // The dented surface at the wound (its start without the inset), and where the vertex nearest the hit is drawn
        // (the binding before: its rest place, no dent).
        const glm::vec3 surf = s.wound + s.surfN * surfaceInset;
        glm::vec3 old = surf;
        if(s.oldVertex >= 0)
        {
            glm::quat rot;
            float m[16]{};
            const bool rag = s.pose == Pose::Ragdoll;
            if(!rag)
            {
                render::drawnEntityMatrix(*e, e->scale ? e->scale : ENTSCALE_DEFAULT, m);
            }
            if(!vertexNow(s, *e, rag, m, s.oldVertex, old, rot))
            {
                old = surf;
            }
        }
        // How far its start is from the body as drawn now (its nearest vertex: a stream left behind is far).
        const int nv = posedNow(s.ent) ? nearestVertex(s.wound, 1e6f) : -1;
        const float off = nv >= 0 ? glm::distance(scratchNow[static_cast<za::SizeT>(nv)], s.wound) : -1.f;
        Con_Printf("stream %d: entity %d %s, %s, on %s; dent %.2f deep there; start %.2f out of it (%.2f from it); "
                   "the nearest vertex's %.2f out (%.2f from it); the body drawn %.1f from its start; lands %s\n",
            static_cast<int>(&s - streams), s.ent, s.model ? s.model->name : "?", s.pose == Pose::Ragdoll ? "ragdoll" : "animated",
            s.onTriangle ? "its struck triangle" : "a vertex", s.push, glm::dot(s.wound - surf, s.surfN), glm::distance(s.wound, surf),
            glm::dot(old - surf, s.surfN), glm::distance(old, surf), off,
            !s.lands ? "nowhere" : s.landsOnThing ? "on a thing" : "on the world");
        shown++;
    }
    Con_Printf("vr_gore_streams_info: %d pouring; since the map began %d bound afresh to a ragdoll, %d stopped (no body "
               "near)\n",
        shown, rebinds, unbound);
    // Since the last: the worst stretch drawn (ROUND22.md, "Blood streams: smoothed"), then counted afresh.
    Con_Printf("vr_gore_streams_info: stretch max %.2f (%.1f units at most) over %d pieces drawn; %d faded, %d broken, "
               "%d cuts at the wound's jumps\n",
        stretch.maxRatio, stretch.maxLength, stretch.segments, stretch.faded, stretch.broken, stretch.cuts);
    stretch = StretchStats{};
}

void test_f()
{
    if(!cl.worldmodel || cl.viewentity <= 0 || cl.viewentity >= cl.num_entities)
    {
        return;
    }
    vec3_t fwd, right, up;
    AngleVectors(cl.viewangles, fwd, right, up);
    const glm::vec3 f = glm::length(glm::vec3{fwd[0], fwd[1], 0.f}) > 0.01f ? glm::normalize(glm::vec3{fwd[0], fwd[1], 0.f})
                                                                           : glm::vec3{1.f, 0.f, 0.f};
    const entity_t& pl = cl_entities[cl.viewentity];
    const glm::vec3 eye{pl.origin[0], pl.origin[1], pl.origin[2] + 22.f};
    // The monster or corpse nearest the middle of the view (an alias model with a body: not a gib, not a weapon).
    int best = -1;
    float bestScore = -1.f;
    for(int i = 1; i < cl.num_entities; i++)
    {
        const entity_t& e = cl_entities[i];
        if(i == cl.viewentity || !e.model || e.model->type != mod_alias || (e.model->flags & (EF_GIB | EF_ROCKET)))
        {
            continue;
        }
        const glm::vec3 o{e.origin[0], e.origin[1], e.origin[2]};
        const glm::vec3 to{o.x - eye.x, o.y - eye.y, 0.f}; // (across: the view's yaw)
        const float d = glm::length(to);
        if(d < 16.f || d > 768.f || e.model->maxs[2] - e.model->mins[2] < 24.f)
        {
            continue;
        }
        const float score = glm::dot(to / d, f);
        if(developer.value)
        {
            Con_Printf("vr_gore_stream_test: entity %d %s %.0f units, %.2f\n", i, e.model->name, d, score);
        }
        if(score > 0.7f && score > bestScore)
        {
            bestScore = score;
            best = i;
        }
    }
    if(best < 0)
    {
        Con_Printf("vr_gore_stream_test: no monster ahead\n");
        return;
    }
    const entity_t& e = cl_entities[best];
    const int n = Cmd_Argc() > 1 ? za::clamp(atoi(Cmd_Argv(1)), 1, 16) : 1;
    const glm::vec3 o{e.origin[0], e.origin[1], e.origin[2]};
    glm::vec3 toEye = eye - o;
    toEye.z = 0.f;
    toEye = glm::length(toEye) > 0.1f ? glm::normalize(toEye) : glm::vec3{1.f, 0.f, 0.f};
    // A point on its front: half way between its feet and its top, in the middle (as drawn: a corpse lies low).
    const float mid = (e.model->mins[2] + e.model->maxs[2]) * 0.5f;
    for(int i = 0; i < n; i++)
    {
        const glm::vec3 side = glm::vec3{-toEye.y, toEye.x, 0.f} * rnd(-6.f, 6.f);
        const glm::vec3 at = o + toEye * 12.f + side + glm::vec3{0.f, 0.f, mid + 4.f + rnd(-6.f, 6.f)};
        start(best, at, -toEye, 20, 64.f);
    }
    Con_Printf("vr_gore_stream_test: %d on entity %d (%s)\n", n, best, e.model->name);
}

} // namespace qvr::bloodstream
