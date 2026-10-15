// vr_bloodstream.cpp -- see vr_bloodstream.hpp.

#include "vr_bloodstream.hpp"
#include "vr_anchor.hpp"
#include "vr_cvars.hpp"
#include "vr_decals.hpp"
#include "vr_engine.hpp"
#include "vr_gore.hpp"
#include "vr_lines.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"
#include "vr_ragdoll.hpp"
#include "vr_render.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
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
    int vertex = 0;
    Pose pose = Pose::None;
    glm::vec3 outLocal{1.f, 0.f, 0.f}; // the way it pours, in the pose's frame (yaw-only, or the ragdoll body's)
    glm::vec3 outWorld{1.f, 0.f, 0.f};
    glm::vec3 wound{0.f}, lastWound{0.f};
    bool hasLast = false;
    double start = 0.0;
    float duration = 6.f;
    float power = 1.f;  // how hard it pours (the hit's damage)
    float phase = 0.f;  // its heartbeat's
    float emitAcc = 0.f;
    // The landing, traced every arcInterval.
    double nextArc = 0.0;
    bool lands = false;
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

// The posed vertices (binding a new stream to its nearest): kept, grown once.
za::Vector<glm::vec3> scratchRest, scratchNow;

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
    if(bone != 0 && !ragdoll::drawnPart(ent, bone, rot, pos, scale))
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

[[nodiscard]] glm::vec3 animatedVertex(const entity_t& e, int v)
{
    float m[16];
    render::entityMatrix(e, false, e.scale ? e.scale : ENTSCALE_DEFAULT, glm::vec3{0.f}, m);
    return bytePoint(m, anchor::posedVertex(e, v, 0.f));
}

// Where the wound is now, and the way it pours; false if its body is gone.
[[nodiscard]] bool locate(Stream& s)
{
    const entity_t* e = entityOf(s.ent, s.model);
    if(!e)
    {
        return false;
    }
    glm::quat rot;
    glm::vec3 at;
    if(ragdollVertex(s.ent, s.vertex, at, rot))
    {
        if(s.pose != Pose::Ragdoll)
        {
            s.outLocal = glm::inverse(rot) * s.outWorld; // (it fell: the way it poured, now the body's)
            s.pose = Pose::Ragdoll;
        }
        s.wound = at;
        s.outWorld = rot * s.outLocal;
        return true;
    }
    if(s.pose == Pose::Ragdoll)
    {
        return false; // (a ragdoll no more: removed, gibbed)
    }
    if(s.pose != Pose::Animated)
    {
        s.outLocal = yawTurn(s.outWorld, -e->angles[1]);
        s.pose = Pose::Animated;
    }
    s.wound = animatedVertex(*e, s.vertex);
    s.outWorld = yawTurn(s.outLocal, e->angles[1]);
    return true;
}

// The vertex of `ent`'s model nearest `at` as it is drawn now; -1 if none.
[[nodiscard]] int nearestVertex(int ent, const glm::vec3& at, float reach)
{
    za::Vector<glm::vec3>& now = scratchNow;
    if(!ragdoll::skinnedVertices(ent, now, nullptr, true))
    {
        const entity_t* e = entityOf(ent, nullptr);
        if(!e || !anchor::posedVertices(*e, 0.f, scratchRest, now))
        {
            return -1;
        }
        float m[16];
        render::entityMatrix(*e, false, e->scale ? e->scale : ENTSCALE_DEFAULT, glm::vec3{0.f}, m);
        for(glm::vec3& p : now)
        {
            p = bytePoint(m, p);
        }
    }
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
    if(!e)
    {
        return;
    }
    const int v = nearestVertex(ent, at, reach);
    if(v < 0)
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
    s.vertex = v;
    // It pours back toward the shot (the hole faces it) and out from the body's middle, a little up.
    glm::vec3 radial = at - glm::vec3{e->origin[0], e->origin[1], at.z};
    radial = glm::length(radial) > 0.1f ? glm::normalize(radial) : glm::vec3{0.f};
    const glm::vec3 back = glm::length(dir) > 0.01f ? -glm::normalize(dir) : radial;
    glm::vec3 out = back * 0.6f + radial * 0.6f + glm::vec3{rnd(-0.2f, 0.2f), rnd(-0.2f, 0.2f), rnd(0.05f, 0.3f)};
    s.outWorld = glm::length(out) > 0.01f ? glm::normalize(out) : glm::vec3{0.f, 0.f, 1.f};
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
    s.landTime = arcStep * arcSteps;
    for(int i = 1; i <= arcSteps; i++)
    {
        const float t = arcStep * static_cast<float>(i);
        const glm::vec3 p = s.wound + v0 * t + glm::vec3{0.f, 0.f, -0.5f * g * t * t};
        glm::vec3 where, n;
        float f;
        if(decals::trace(prev, p, where, n, f))
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
    if(size <= 0.f)
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
        glm::vec3 bodyVel{0.f};
        if(s.hasLast && dt > 0.f)
        {
            bodyVel = (s.wound - s.lastWound) / dt;
            const float l = glm::length(bodyVel);
            bodyVel *= l > 400.f ? 400.f / l : 1.f;
        }
        const float speed = za::clamp(vr_gore_streams_speed.value, 0.f, 400.f) * s.power * (0.35f + 0.9f * strength);
        const glm::vec3 v0 = s.outWorld * speed + bodyVel * 0.7f;
        if(cl.time >= s.nextArc || cl.time < s.nextArc - 1.0)
        {
            s.nextArc = cl.time + arcInterval;
            traceArc(s, v0);
            s.color = lightColor(s.wound);
        }
        // New points: from where the wound was over the frame (an even chain however fast the frames).
        s.emitAcc += dt * emitRate;
        const int n = za::min(8, static_cast<int>(s.emitAcc));
        s.emitAcc = za::min(s.emitAcc - static_cast<float>(n), 1.f);
        const float width = za::clamp(vr_gore_streams_width.value, 0.05f, 4.f) * s.power * (0.45f + 0.55f * za::min(1.f, strength));
        for(int k = 0; k < n; k++)
        {
            const float back = n > 1 ? static_cast<float>(n - 1 - k) / static_cast<float>(n) : 0.f;
            s.head = (s.head + 1) % maxPoints;
            s.count = za::min(s.count + 1, maxPoints);
            Point& p = s.pts[s.head];
            p.pos = s.hasLast ? glm::mix(s.wound, s.lastWound, back) : s.wound;
            p.vel = v0;
            p.age = back * dt;
            p.life = s.landTime;
            p.width = width;
            p.speed0 = za::max(10.f, glm::length(v0));
            p.pos += p.vel * p.age;
        }
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
        if(newer)
        {
            lines::sceneLine(newer->pos, p.pos, w, c, c);
        }
        else if(k == 0 && s.emitting)
        {
            lines::sceneLine(s.wound, p.pos, w, c, c); // (joined to the wound)
        }
        newer = &p;
    }
}

} // namespace

void onWound(int ent, const glm::vec3& at, const glm::vec3& dir, int kind, int amount)
{
    // Bullet holes: a shot or a pellet (1), a nail or a small missile (2).
    if((kind != 1 && kind != 2) || !enabled() || pendingCount >= maxPending || ent == cl.viewentity)
    {
        return;
    }
    if(rnd(0.f, 1.f) >= za::clamp(vr_gore_streams_chance.value, 0.f, 1.f))
    {
        return;
    }
    pending[pendingCount++] = {ent, at, dir, amount};
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
    for(Stream& s : streams)
    {
        if(s.used)
        {
            update(s, dt);
            if(s.used)
            {
                draw(s);
            }
        }
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
}

void count()
{
    int live = 0, pouring = 0, points = 0;
    for(const Stream& s : streams)
    {
        live += s.used ? 1 : 0;
        pouring += s.used && s.emitting ? 1 : 0;
        points += s.used ? s.count : 0;
    }
    Con_Printf("bloodstreams: %d (%d pouring), %d points\n", live, pouring, points);
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
