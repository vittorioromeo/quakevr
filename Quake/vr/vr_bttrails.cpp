// vr_bttrails.cpp -- see vr_bttrails.hpp.

#include "vr_modelmetadata.hpp"
#include "vr_bttrails.hpp"
#include "vr_bullettime.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_haze.hpp"
#include "vr_mem.hpp"
#include "vr_portals.hpp"
#include "vr_profile.hpp"
#include "vr_stereo.hpp"
#include "vr_units.hpp"
#include "vr_water.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <stdlib.h>

namespace qvr::bttrails
{
namespace
{

// What a trail follows (vr_bullettime_trails_hitscan, _nails, _explosives, _enemy; the test's always).
enum Kind : int
{
    KindNone = 0,
    KindHitscan,
    KindNail,
    KindExplosive,
    KindEnemy,
    KindTest,
};

constexpr int kMaxTrails = 64;
constexpr int kMaxPoints = 40;       // places kept a trail (the oldest dropped)
constexpr float kSegment = 10.f;     // units its projectile flies before a place is kept
constexpr float kPiece = 24.f;       // the ribbon's longest piece (longer ones split, up to kMaxSplit)
constexpr int kMaxSplit = 8;
constexpr float kMinSpeed = 200.f;   // units a second of the game's time: slower entities make no trail (a grenade at rest)
constexpr float kJump = 256.f;       // a projectile further than this from its last place went through a teleporter: a new trail
constexpr float kHeadRamp = 10.f;    // units behind the projectile the bend fades in over
constexpr float kWiden = 28.f;       // ... and the trail widens over to its full width
constexpr float kNearFade = 40.f;    // units from the eye a trail fades out within (from a quarter of it)
constexpr float kEaseIn = 0.15f;     // real seconds they take to come in as bullet time starts
constexpr int kMargin = 32;          // pixels round the trails copied too (the most they bend)

struct Point
{
    glm::vec3 pos{0.f};
    float odometer = 0.f; // units from the trail's first place
    double time = 0.;     // the trails' clock (real seconds)
};

struct Trail
{
    bool used = false;
    bool live = false; // still following its projectile
    Kind kind = KindNone;
    float seed = 0.f;  // its ripples' phase
    // An entity's: its number, model and the frame it was last relinked.
    int ent = 0;
    const qmodel_s* model = nullptr;
    unsigned seen = 0;
    // A hitscan shot's: its line, its head's speed (units a second of the game's time) and since when (cl.time).
    glm::vec3 from{0.f}, dir{0.f};
    float distance = 0.f, speed = 0.f;
    double start = 0.;
    // Its places, the oldest first; and where its projectile is now (the newest place, kept or not).
    Point points[kMaxPoints];
    int count = 0;
    Point head;
};

Trail trails[kMaxTrails];
double clockNow = 0.;  // real seconds (VR_AdvanceTime)
unsigned frameNow = 0; // VR_AdvanceTime's count
float ease = 0.f;      // 0 .. 1: in with bullet time, out over vr_bullettime_trails_fade after
bool creating = false; // new trails start (bullet time, or vr_bullettime_trails 2)
int liveCount = 0;     // trails in use
unsigned seedState = 0x9E3779B9u;

[[nodiscard]] float random01()
{
    seedState = seedState * 1664525u + 1013904223u;
    return static_cast<float>(seedState >> 8) / static_cast<float>(1u << 24);
}

[[nodiscard]] float lifeSeconds()
{
    return za::clamp(vr_bullettime_trails_life.value, 0.05f, 10.f);
}

[[nodiscard]] bool kindWanted(Kind kind)
{
    switch(kind)
    {
        case KindHitscan: return vr_bullettime_trails_hitscan.value != 0.f;
        case KindNail: return vr_bullettime_trails_nails.value != 0.f;
        case KindExplosive: return vr_bullettime_trails_explosives.value != 0.f;
        case KindEnemy: return vr_bullettime_trails_enemy.value != 0.f;
        case KindTest: return true;
        default: return false;
    }
}

// Its width over a bullet's.
[[nodiscard]] float kindWidth(Kind kind)
{
    return kind == KindExplosive ? 2.f : kind == KindEnemy ? 1.5f : 1.f;
}

[[nodiscard]] const char* kindName(Kind kind)
{
    switch(kind)
    {
        case KindHitscan: return "hitscan";
        case KindNail: return "nail";
        case KindExplosive: return "explosive";
        case KindEnemy: return "enemy";
        case KindTest: return "test";
        default: return "none";
    }
}

// A free trail, else the one whose projectile passed last the longest ago.
[[nodiscard]] Trail& allocate(Kind kind)
{
    Trail* best = nullptr;
    for(Trail& t : trails)
    {
        if(!t.used)
        {
            best = &t;
            break;
        }
        if(!best || t.head.time < best->head.time)
        {
            best = &t;
        }
    }
    if(!best->used)
    {
        liveCount++;
    }
    *best = Trail{};
    best->used = true;
    best->live = true;
    best->kind = kind;
    best->seed = random01() * 6.2831853f;
    return *best;
}

void release(Trail& t)
{
    if(t.used)
    {
        liveCount--;
    }
    t = Trail{};
}

void keep(Trail& t, const Point& p)
{
    if(t.count == kMaxPoints)
    {
        for(int i = 1; i < kMaxPoints; i++)
        {
            t.points[i - 1] = t.points[i];
        }
        t.count--;
    }
    t.points[t.count++] = p;
}

// The projectile is at `p` now.
void extend(Trail& t, const glm::vec3& p)
{
    if(t.count == 0)
    {
        t.head = {p, 0.f, clockNow};
        keep(t, t.head);
        return;
    }
    const Point& last = t.points[t.count - 1];
    const float d = glm::distance(p, last.pos);
    t.head = {p, last.odometer + d, clockNow};
    if(d >= kSegment)
    {
        keep(t, t.head);
    }
}

// A hitscan shot's head, where cl.time has it; its end kept as it gets there.
void flyHitscan(Trail& t)
{
    if(!t.live || t.ent != 0 || t.speed <= 0.f)
    {
        return;
    }
    const float flown = static_cast<float>(cl.time - t.start) * t.speed;
    if(flown < 0.f)
    {
        return;
    }
    extend(t, t.from + t.dir * za::min(flown, t.distance));
    if(flown >= t.distance)
    {
        if(t.count > 0 && glm::distance(t.points[t.count - 1].pos, t.head.pos) > 0.01f)
        {
            keep(t, t.head);
        }
        t.live = false;
    }
}

// What kind of trail an entity's model makes (KindNone: none).
[[nodiscard]] Kind entityKind(int ent, const entity_t& e)
{
    const qmodel_s* m = e.model;
    const int flags = m->flags;
    if(flags & EF_GIB)
    {
        return KindNone;
    }
    if(flags & (EF_TRACER | EF_TRACER2 | EF_TRACER3 | EF_ZOMGIB))
    {
        return KindEnemy; // scrag spit, hell knight flames, vore balls, zombies' flesh
    }
    if(flags & EF_ROCKET)
    {
        return KindExplosive; // rockets (and Chthon's lava balls)
    }
    const modelmeta::ModelMetadata& meta = modelmeta::get(m);
    if((flags & EF_GRENADE) || modelmeta::isQuakeGrenade(m))
    {
        return VR_GrenadeTrail(ent) ? KindExplosive : KindNone; // not a hand grenade with its pin in
    }
    if(meta.is(modelmeta::Id::Spike) || meta.is(modelmeta::Id::SSpike) || meta.is(modelmeta::Id::Lspike) ||
        meta.is(modelmeta::Id::Lasrspik))
    {
        return KindNail;
    }
    if(meta.is(modelmeta::Id::Laser))
    {
        return KindEnemy; // an enforcer's
    }
    return KindNone;
}

// ----------------------------------------------------------------------------
// The shader. Vertex: the ribbon's corners in the world. Fragment: the bend across the ribbon (a rod's magnification
// and ripples running along it) as a shift in the world, projected; the copy read there, blended in at the edges.

constexpr const char* vertexSource = R"(#version 430
layout(location = 0) in vec4 Pos;   // xyz the world, w across the ribbon (-1 .. 1)
layout(location = 1) in vec4 Side;  // xyz the way across it (unit), w its half width
layout(location = 2) in vec4 Data;  // x its strength here (0 .. 1), y the odometer (units), z its seed
layout(location = 0) uniform mat4 ViewProj;
layout(location = 0) out vec4 WorldPos;
layout(location = 1) out vec4 SideOut;
layout(location = 2) out vec4 DataOut;
void main()
{
    WorldPos = Pos;
    SideOut = Side;
    DataOut = Data;
    gl_Position = ViewProj * vec4(Pos.xyz, 1.0);
}
)";

constexpr const char* fragmentSource = R"(#version 430
layout(binding = 6) uniform sampler2D Scene;     // the copy (vr_haze.cpp copyScene)
layout(binding = 8) uniform sampler2D Distances; // how far the opaque scene is along the view (half size)
layout(location = 0) uniform mat4 ViewProj;
layout(location = 1) uniform vec4 Eye;       // xyz, w the trails' clock (real seconds)
layout(location = 2) uniform vec4 Params;    // x strength, y the scene's distances (1) or none (0)
layout(location = 5) uniform vec4 Viewport;  // the scene's, in the framebuffer's pixels
layout(location = 6) uniform vec4 CopyRect;  // the part copied, in the viewport's pixels (x0, y0, x1, y1)
layout(location = 0) in vec4 WorldPos;
layout(location = 1) in vec4 Side;
layout(location = 2) in vec4 Data;
layout(location = 0) out vec4 Out;

float SceneDistance(vec2 p) // along the view
{
    return texelFetch(Distances, clamp(ivec2(p * 0.5), ivec2(0), textureSize(Distances, 0) - 1), 0).r;
}

void main()
{
    float a = clamp(WorldPos.w, -1.0, 1.0);
    float edge = 1.0 - a * a;
    float f = Data.x * edge * edge;
    if (f < 0.003)
        discard;
    // A glass rod: what is behind it seen drawn in towards its core; ripples run along it (anchored in the world).
    float o = Data.y, t = Eye.w, s = Data.z;
    float ripple = 0.6 * sin(o * 0.19 + s - t * 6.5) + 0.35 * sin(o * 0.43 - 1.7 * s + t * 3.9 + a * 2.5);
    float across = -1.1 * a + ripple * edge;
    vec3 q = WorldPos.xyz;
    vec3 w = Side.xyz * (across * Side.w * Params.x * Data.x);
    vec4 c0 = ViewProj * vec4(q, 1.0), c1 = ViewProj * vec4(q + w, 1.0);
    vec2 shift = (c1.xy / c1.w - c0.xy / c0.w) * 0.5 * Viewport.zw;
    if (Params.y > 0.0 && SceneDistance(gl_FragCoord.xy + shift) < 1.0 / gl_FragCoord.w - 4.0)
        shift = vec2(0.0); // not what is in front of the trail (a hand, a gun, a monster): its colours would smear
    vec2 p = clamp(gl_FragCoord.xy - Viewport.xy + shift, CopyRect.xy + 0.5, CopyRect.zw - 0.5);
    Out = vec4(texture(Scene, p / Viewport.zw).rgb, clamp(f * 4.0, 0.0, 1.0));
}
)";

GLuint program = 0;
bool programFailed = false;

struct Vertex
{
    glm::vec4 pos;
    glm::vec4 side;
    glm::vec4 data;
};
static_assert(sizeof(Vertex) == 48);

struct Sample
{
    glm::vec3 pos;
    float age, along, odometer;
};

// The ribbons, made for the first eye and drawn in both (the main thread).
struct TrailScratch
{
    za::Vector<Sample> path;
    za::Vector<Vertex> vertices;
    auto members() { return qvr::mem::list(path, vertices); }
};
mem::Scratch<TrailScratch> scratch{"bttrails"};

// A trail's places from its projectile back, split into pieces, cut where they are older than its life or further than
// the longest trail; false if none is left.
bool makePath(const Trail& t, float life, float maxLength, za::Vector<Sample>& path)
{
    path.clear();
    if(t.count == 0)
    {
        return false;
    }
    const auto add = [&](const Point& p) -> bool {
        const float age = static_cast<float>(clockNow - p.time);
        if(path.empty())
        {
            path.pushBack({p.pos, age, 0.f, p.odometer});
            return age < life;
        }
        const Sample prev = path.back();
        const float length = glm::distance(prev.pos, p.pos);
        if(length < 0.01f)
        {
            return true;
        }
        const int n = za::clamp(static_cast<int>(za::ceil(length / kPiece)), 1, kMaxSplit);
        for(int k = 1; k <= n; k++)
        {
            const float f = static_cast<float>(k) / static_cast<float>(n);
            const Sample s{glm::mix(prev.pos, p.pos, f), prev.age + (age - prev.age) * f, prev.along + length * f,
                prev.odometer + (p.odometer - prev.odometer) * f};
            if(s.age >= life || s.along >= maxLength)
            {
                const Sample q = path.back();
                float cut = 1.f;
                if(s.age >= life && s.age > q.age)
                {
                    cut = za::min(cut, (life - q.age) / (s.age - q.age));
                }
                if(s.along >= maxLength && s.along > q.along)
                {
                    cut = za::min(cut, (maxLength - q.along) / (s.along - q.along));
                }
                cut = za::clamp(cut, 0.f, 1.f);
                path.pushBack({glm::mix(q.pos, s.pos, cut), q.age + (s.age - q.age) * cut, q.along + (s.along - q.along) * cut,
                    q.odometer + (s.odometer - q.odometer) * cut});
                return false;
            }
            path.pushBack(s);
        }
        return true;
    };
    const Point& newest = t.points[t.count - 1];
    bool more = true;
    if(t.head.time > newest.time || glm::distance(t.head.pos, newest.pos) > 0.01f)
    {
        more = add(t.head);
    }
    for(int i = t.count - 1; i >= 0 && more; i--)
    {
        more = add(t.points[i]);
    }
    return path.size() >= 2;
}

// This frame's ribbons, turned to face `facing`.
void build(const glm::vec3& facing)
{
    QVR_PROFILE("bt trails");
    za::Vector<Vertex>& out = scratch.vertices;
    out.clear();
    za::Vector<Sample>& path = scratch.path;
    const float life = lifeSeconds();
    const float m2u = units::metresToUnits();
    const float maxLength = za::clamp(vr_bullettime_trails_length.value, 0.1f, 50.f) * m2u;
    const float width = za::clamp(vr_bullettime_trails_width.value, 0.5f, 200.f) * 0.01f * m2u;
    for(Trail& t : trails)
    {
        if(!t.used)
        {
            continue;
        }
        flyHitscan(t);
        if(!kindWanted(t.kind) || !makePath(t, life, maxLength, path))
        {
            continue;
        }
        const float halfWidth = 0.5f * width * kindWidth(t.kind);
        const za::SizeT n = path.size();
        glm::vec3 lastSide{0.f};
        Vertex prevA{}, prevB{};
        for(za::SizeT i = 0; i < n; i++)
        {
            const Sample& s = path[i];
            const glm::vec3 tangent = path[i > 0 ? i - 1 : 0].pos - path[i + 1 < n ? i + 1 : n - 1].pos;
            const glm::vec3 toEye = facing - s.pos;
            glm::vec3 side = glm::cross(tangent, toEye);
            const float l = glm::length(side);
            side = l > 1e-5f ? side / l : lastSide;
            lastSide = side;
            // None near the eye (a shot of yours starts there: its ribbon would fill the view), nor seen end on (the
            // ribbon edge on, its width all along the view).
            const float eyeDistance = glm::length(toEye);
            const float sine = l / za::max(glm::length(tangent) * eyeDistance, 1e-5f);
            const float view = glm::smoothstep(kNearFade * 0.25f, kNearFade, eyeDistance) * glm::smoothstep(0.1f, 0.35f, sine);
            const float ageShare = za::clamp(s.age / life, 0.f, 1.f);
            const float head = glm::smoothstep(0.f, kHeadRamp, s.along);
            const float fade = (i + 1 == n) ? 0.f : ease * head * view * za::pow(1.f - ageShare, 1.3f);
            const float hw = halfWidth * (0.35f + 0.65f * glm::smoothstep(0.f, kWiden, s.along)) * (1.f + 0.6f * ageShare);
            const glm::vec4 sideW{side, hw}, data{fade, s.odometer, t.seed, 0.f};
            const Vertex a{glm::vec4(s.pos - side * hw, -1.f), sideW, data};
            const Vertex b{glm::vec4(s.pos + side * hw, 1.f), sideW, data};
            if(i > 0 && (prevA.data.x > 0.f || fade > 0.f))
            {
                out.pushBack(prevA);
                out.pushBack(prevB);
                out.pushBack(b);
                out.pushBack(prevA);
                out.pushBack(b);
                out.pushBack(a);
            }
            prevA = a;
            prevB = b;
        }
    }
}

void drawView()
{
    const float strength = za::clamp(vr_bullettime_trails_strength.value, 0.f, 3.f);
    if(vr_bullettime_trails.value == 0.f || liveCount == 0 || ease <= 0.f || strength <= 0.f || !cl.worldmodel ||
        programFailed || portals::viewing())
    {
        return;
    }
    // The ribbons: made once a frame, facing the first eye (the same in both); the spectator camera's its own.
    if(!stereo::isRenderingEye() || stereo::isSpectator() || stereo::isFirstEye())
    {
        build(glm::vec3(r_framedata.eyepos[0], r_framedata.eyepos[1], r_framedata.eyepos[2]));
    }
    const za::Vector<Vertex>& vertices = scratch.vertices;
    if(vertices.empty())
    {
        return;
    }
    GLuint color = 0, depth = 0;
    int samples = 1, viewport[4];
    const GLuint sceneFbo = VR_SceneTarget(&color, &depth, &samples, viewport);
    if(!color || viewport[2] <= 0 || viewport[3] <= 0)
    {
        return;
    }
    const int vw = viewport[2], vh = viewport[3];

    // The part of the view they cover (all of it if one reaches behind the eye).
    const glm::mat4 viewProj = glm::make_mat4(r_framedata.viewproj);
    float rx0 = 1e30f, ry0 = 1e30f, rx1 = -1e30f, ry1 = -1e30f;
    bool full = false;
    for(const Vertex& v : vertices)
    {
        const glm::vec4 c = viewProj * glm::vec4(glm::vec3(v.pos), 1.f);
        if(c.w < 1.f)
        {
            full = true;
            break;
        }
        const float x = (c.x / c.w * 0.5f + 0.5f) * static_cast<float>(vw), y = (c.y / c.w * 0.5f + 0.5f) * static_cast<float>(vh);
        rx0 = za::min(rx0, x);
        ry0 = za::min(ry0, y);
        rx1 = za::max(rx1, x);
        ry1 = za::max(ry1, y);
    }
    int x0 = 0, y0 = 0, x1 = vw, y1 = vh;
    if(!full)
    {
        x0 = za::max(0, static_cast<int>(za::floor(za::max(rx0, -1e6f))) - kMargin);
        y0 = za::max(0, static_cast<int>(za::floor(za::max(ry0, -1e6f))) - kMargin);
        x1 = za::min(vw, static_cast<int>(za::ceil(za::min(rx1, 1e6f))) + kMargin);
        y1 = za::min(vh, static_cast<int>(za::ceil(za::min(ry1, 1e6f))) + kMargin);
        if(x1 <= x0 || y1 <= y0)
        {
            return;
        }
    }

    QVR_GPU_PROFILE("bt trails");
    if(!program)
    {
        program = gfx::glProgram(vertexSource, fragmentSource, "vr bullet time trails");
        programFailed = !program;
        if(programFailed)
        {
            return;
        }
    }
    GLuint distances = 0;
    {
        QVR_GPU_PROFILE("distances");
        distances = water::opaqueSceneDistances();
    }
    const GLuint copy = haze::copyScene(sceneFbo, color, samples, viewport, x0, y0, x1, y1);
    if(!copy)
    {
        return;
    }

    glEnable(GL_SCISSOR_TEST);
    glScissor(viewport[0] + x0, viewport[1] + y0, x1 - x0, y1 - y0);
    GL_UseProgram(program);
    GL_BindNative(GL_TEXTURE6, GL_TEXTURE_2D, copy);
    GL_BindNative(GL_TEXTURE8, GL_TEXTURE_2D, distances);
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, r_framedata.viewproj);
    GL_Uniform4fFunc(1, r_framedata.eyepos[0], r_framedata.eyepos[1], r_framedata.eyepos[2], static_cast<float>(clockNow));
    GL_Uniform4fFunc(2, strength, distances ? 1.f : 0.f, 0.f, 0.f);
    GL_Uniform4fFunc(5, static_cast<float>(viewport[0]), static_cast<float>(viewport[1]), static_cast<float>(vw), static_cast<float>(vh));
    GL_Uniform4fFunc(6, static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(x1), static_cast<float>(y1));
    GL_SetState(GLS_BLEND_ALPHA | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(3));
    GLuint buf;
    GLbyte* ofs;
    GL_Upload(GL_ARRAY_BUFFER, vertices.data(), vertices.size() * sizeof(Vertex), &buf, &ofs);
    GL_BindBuffer(GL_ARRAY_BUFFER, buf);
    GL_VertexAttribPointerFunc(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), ofs);
    GL_VertexAttribPointerFunc(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), ofs + sizeof(glm::vec4));
    GL_VertexAttribPointerFunc(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), ofs + 2 * sizeof(glm::vec4));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glDisable(GL_SCISSOR_TEST);
}

// ----------------------------------------------------------------------------
// Commands.

// vr_bullettime_trails_test [count] [m/s] [distance]: `count` shots (a test's trails, whatever the kinds wanted) across
// the view `distance` units ahead (96), from its left to its right, at `m/s` of the game's time (20: slow enough to see
// them fly in bullet time); with no bullet time (and vr_bullettime_trails 1) they make none.
void test_f()
{
    const int count = Cmd_Argc() > 1 ? za::clamp(atoi(Cmd_Argv(1)), 1, 32) : 3;
    const float speed = (Cmd_Argc() > 2 ? za::clamp(static_cast<float>(atof(Cmd_Argv(2))), 0.1f, 2000.f) : 20.f) *
                        units::metresToUnits();
    const float ahead = Cmd_Argc() > 3 ? za::clamp(static_cast<float>(atof(Cmd_Argv(3))), 8.f, 4096.f) : 96.f;
    vec3_t fwd, right, up;
    AngleVectors(r_refdef.viewangles, fwd, right, up);
    const glm::vec3 eye(r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]);
    const glm::vec3 f(fwd[0], fwd[1], fwd[2]), r(right[0], right[1], right[2]), u(up[0], up[1], up[2]);
    const int before = liveCount;
    for(int i = 0; i < count; i++)
    {
        const float h = count > 1 ? (static_cast<float>(i) / static_cast<float>(count - 1) - 0.5f) * 0.8f * ahead : 0.f;
        const glm::vec3 from = eye + f * ahead - r * (ahead * 0.9f) + u * h;
        const glm::vec3 to = eye + f * (ahead * 1.15f) + r * (ahead * 0.9f) + u * (h * 0.7f);
        const glm::vec3 line = to - from;
        if(!creating)
        {
            break;
        }
        Trail& t = allocate(KindTest);
        t.from = from;
        t.distance = glm::length(line);
        t.dir = line / t.distance;
        t.speed = speed;
        t.start = cl.time;
        extend(t, from);
    }
    Con_Printf("vr_bullettime_trails_test: from %.0f %.0f %.0f ahead %.2f %.2f %.2f: %d shots, %d trails (%s)\n", eye.x,
        eye.y, eye.z, f.x, f.y, f.z, count, liveCount - before,
        creating ? "making them" : "none: not in bullet time");
}

// vr_bullettime_trails_list: the trails, their kinds, places and strength.
void list_f()
{
    int n = 0;
    for(const Trail& t : trails)
    {
        if(!t.used)
        {
            continue;
        }
        n++;
        Con_Printf("trail %s ent %d %s points %d odometer %.0f head %.0f %.0f %.0f age %.2f\n", kindName(t.kind), t.ent,
            t.live ? "live" : "left", t.count, t.head.odometer, t.head.pos.x, t.head.pos.y, t.head.pos.z,
            static_cast<float>(clockNow - t.head.time));
    }
    Con_Printf("bttrails: %d trails, ease %.2f, %s, %d vertices last made\n", n, ease, creating ? "making" : "not making",
        static_cast<int>(scratch.vertices.size()));
}

} // namespace

void advance(double dt)
{
    const int mode = static_cast<int>(vr_bullettime_trails.value);
    if(mode == 0)
    {
        if(liveCount > 0 || ease > 0.f)
        {
            clear();
        }
        return;
    }
    // The entities' trails not relinked last frame: their projectiles are gone.
    for(Trail& t : trails)
    {
        if(t.used && t.ent != 0 && t.seen != frameNow)
        {
            t.live = false;
            t.ent = 0;
        }
    }
    clockNow += dt;
    frameNow++;
    creating = mode == 2 || bullettime::meter().active;
    const float step = static_cast<float>(dt);
    ease = creating ? za::min(1.f, ease + step / kEaseIn)
                    : za::max(0.f, ease - step / za::max(vr_bullettime_trails_fade.value, 0.01f));
    if(ease <= 0.f && !creating)
    {
        if(liveCount > 0)
        {
            clear();
        }
        return;
    }
    // Places older than their life (but the newest of those: the cut is made on the way to it), and the trails all
    // gone so.
    const float life = lifeSeconds();
    for(Trail& t : trails)
    {
        if(!t.used)
        {
            continue;
        }
        int old = 0;
        while(old + 1 < t.count && clockNow - t.points[old + 1].time > life)
        {
            old++;
        }
        if(old > 0)
        {
            for(int i = old; i < t.count; i++)
            {
                t.points[i - old] = t.points[i];
            }
            t.count -= old;
        }
        if(!t.live && clockNow - t.head.time > life)
        {
            release(t);
        }
    }
}

bool wanted(bool enemy)
{
    return creating && vr_bullettime_trails.value != 0.f && kindWanted(enemy ? KindEnemy : KindHitscan);
}

void hitscan(const glm::vec3& from, const glm::vec3& to, float speed, bool enemy)
{
    const Kind kind = enemy ? KindEnemy : KindHitscan;
    if(!wanted(enemy))
    {
        return;
    }
    const glm::vec3 line = to - from;
    const float distance = glm::length(line);
    if(distance < 8.f)
    {
        return;
    }
    Trail& t = allocate(kind);
    t.from = from;
    t.dir = line / distance;
    t.distance = distance;
    t.speed = za::max(speed, 1.f);
    t.start = cl.time;
    extend(t, from);
}

void draw()
{
    drawView();
}

void clear()
{
    for(Trail& t : trails)
    {
        t = Trail{};
    }
    liveCount = 0;
    ease = 0.f;
    creating = false;
}

void registerCommands()
{
    Cmd_AddCommand("vr_bullettime_trails_test", test_f);
    Cmd_AddCommand("vr_bullettime_trails_list", list_f);
}

} // namespace qvr::bttrails

using namespace qvr;

// CL_RelinkEntities, each entity relinked this frame: a projectile's trail follows it.
extern "C" void VR_DistortionTrail(int ent)
{
    if(bttrails::ease <= 0.f || vr_bullettime_trails.value == 0.f || ent == cl.viewentity)
    {
        return;
    }
    const entity_t& e = cl_entities[ent];
    if(!e.model || e.model->type != mod_alias)
    {
        return;
    }
    using namespace bttrails;
    const Kind kind = entityKind(ent, e);
    Trail* trail = nullptr;
    for(Trail& t : trails)
    {
        if(t.used && t.ent == ent)
        {
            trail = &t;
            break;
        }
    }
    const glm::vec3 origin(e.origin[0], e.origin[1], e.origin[2]);
    // A new projectile in its place, or one gone through a teleporter: its old trail left where it was.
    if(trail && (kind == KindNone || trail->model != e.model || e.forcelink ||
                    (trail->count > 0 && glm::distance(trail->head.pos, origin) > kJump)))
    {
        trail->live = false;
        trail->ent = 0;
        trail = nullptr;
    }
    if(kind == KindNone || !kindWanted(kind))
    {
        return;
    }
    if(!trail)
    {
        // Only a flying one starts a trail (by its last two messages).
        const double mdt = cl.mtime[0] - cl.mtime[1];
        const glm::vec3 m0(e.msg_origins[0][0], e.msg_origins[0][1], e.msg_origins[0][2]);
        const glm::vec3 m1(e.msg_origins[1][0], e.msg_origins[1][1], e.msg_origins[1][2]);
        if(!creating || mdt <= 0. || glm::distance(m0, m1) < kMinSpeed * static_cast<float>(mdt))
        {
            return;
        }
        trail = &allocate(kind);
        trail->ent = ent;
        trail->model = e.model;
    }
    trail->seen = frameNow;
    trail->live = true;
    extend(*trail, origin);
}
