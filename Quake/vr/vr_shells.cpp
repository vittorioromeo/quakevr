// vr_shells.cpp -- see vr_shells.hpp.
//
// Each casing is a point with a velocity, an orientation and an angular velocity. In the air it
// falls at 9.81 m/s^2 (in Quake units at vr_world_scale, times sv_gravity / 800: low-gravity
// maps are floaty), with a little air drag (much more in a liquid, where it sinks slowly);
// a line trace through the world and the brush entities (worldtrace::world, the client's own
// data) finds what it hits: it bounces off with some restitution and friction, tumbling anew, and
// tinks. On a floor too slow to bounce it lies down on its side, rolls (across its axis) or slides
// (along it) to a stop and rests; a resting casing only checks now and then that its floor is still
// there (a lift gone down).

#include "vr_shells.hpp"
#include "vr_engine.hpp"
#include "vr_anchor.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_flick.hpp"
#include "vr_hands.hpp"
#include "vr_particles.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include <vector>

namespace qvr::shells
{
namespace
{

enum Flags : int
{
    FlagFlick = 1, // a flick reload: flung by the spinning weapon
};

// A weapon's ejection ports, in its model's space (as its frames' vertices: +x forward, +y left,
// +z up; mirrored with the model in the off hand), picked on the models (v_shot: just out of the
// ejection port on the receiver's right side, improve_weapons3.py; v_shot2: the two chambers at the
// breech). Casings go out along `dir` at `speed` metres per second, tumbling about `spinAxis`
// (model space) at `spin` radians per second. `anchor` is a vertex near the ports (an anchor index, see
// vr_anchor.hpp) whose move from frame 0 moves them with the firing animation (the recoil).
struct Weapon
{
    const char* model;
    int ports;
    glm::vec3 port[2];
    glm::vec3 dir[2];
    float speed;
    glm::vec3 spinAxis;
    float spin;
    float smoke;  // puff at the port,
    int sparks;   // and sparks
    int anchor;
};

constexpr Weapon weaponTable[] = {
    {"progs/v_shot.mdl", 1, {{14.5f, -2.75f, 4.65f}}, {{-0.3f, -1.f, 0.65f}}, 1.6f, {0.f, 0.f, 1.f}, -16.f, 1.f, 3, 70},
    {"progs/v_shot2.mdl", 2, {{11.6f, 1.2f, 6.9f}, {11.6f, -1.2f, 6.9f}}, {{-1.f, 0.15f, 0.5f}, {-1.f, -0.15f, 0.5f}},
        1.6f, {0.f, 1.f, 0.f}, 9.f, 1.5f, 1, 17},
};

// The casings' models (the QC's `kind`).
constexpr const char* kindModels[] = {"progs/vr_shell.mdl"};

// Quake's weapons are drawn about a quarter bigger than the real ones (the shotgun is 1.25 m
// long): the shells are too, to match them.
constexpr float modelScale = 1.25f;
// Half the shell's width (the rim's, make_shell.py) in metres, as drawn: how high a lying one's
// middle is.
constexpr float shellRadius = 0.0112f * modelScale;
constexpr int maxShells = 64;
constexpr float fadeTime = 1.5f; // seconds it fades out over, at the end of vr_shells_life
constexpr float trailTime = 1.2f; // seconds it trails smoke for

// A flick reload's casings leave when the spin has turned the barrels down (the breech open
// upwards, in front of the hand): thrown forward and a little up, away from the head, they fall
// in front of the player. They wait for it at most this long (the spin takes a third of a second).
constexpr float flickEjectAngle = 255.f;
constexpr double flickEjectWait = 0.6;

struct Shell
{
    bool active{false};
    int kind{0};
    glm::vec3 pos{0.f};
    glm::vec3 vel{0.f};
    glm::quat rot{1.f, 0.f, 0.f, 0.f};
    glm::vec3 angVel{0.f}; // radians per second, world axes
    glm::vec3 ground{0.f, 0.f, 1.f}; // the floor's normal, while on it
    double born{0.0};
    double nextCheck{0.0}; // resting: when to look for the floor again
    double nextTink{0.0};
    float smoke{1.f};
    int tinks{0};
    bool onGround{false};
    bool resting{false};
    entity_t ent{};
};

struct Pending
{
    int hand;
    int kind;
    int count;
    int flags;
    double time;
};

// Each hand's ejection port as last seen, for its speed.
struct Track
{
    const qmodel_t* model{nullptr};
    glm::vec3 pos{0.f};
    glm::vec3 vel{0.f};
    double time{-1.0};
};

Shell shells[maxShells];
std::vector<Pending> pending;
Track tracks[2];
double lastRun = -1.0;
int lastFrame = -1;

std::mt19937 rng{std::random_device{}()};

[[nodiscard]] float rnd(float lo, float hi)
{
    return std::uniform_real_distribution<float>{lo, hi}(rng);
}

[[nodiscard]] glm::vec3 onSphere()
{
    const float z = rnd(-1.f, 1.f);
    const float a = rnd(0.f, 6.2831853f);
    const float r = std::sqrt(std::max(0.f, 1.f - z * z));
    return {r * std::cos(a), r * std::sin(a), z};
}

[[nodiscard]] float unitsPerMetre()
{
    return units::metresToUnits();
}

[[nodiscard]] float gravity()
{
    return 9.81f * unitsPerMetre() * std::clamp(sv_gravity.value / 800.f, 0.f, 4.f);
}

[[nodiscard]] float restHeight()
{
    return shellRadius * unitsPerMetre();
}

[[nodiscard]] bool inLiquid(const glm::vec3& p)
{
    if(!cl.worldmodel)
    {
        return false;
    }
    vec3_t v{p.x, p.y, p.z};
    const int c = Mod_PointInLeaf(v, cl.worldmodel)->contents;
    return c <= CONTENTS_WATER && c != CONTENTS_SKY;
}

[[nodiscard]] const Weapon* weaponFor(const qmodel_t* model)
{
    if(!model)
    {
        return nullptr;
    }
    for(const Weapon& w : weaponTable)
    {
        if(!strcmp(model->name, w.model))
        {
            return &w;
        }
    }
    return nullptr;
}

// Where the firing animation has moved the ports from frame 0 (model units).
[[nodiscard]] glm::vec3 animationShift(const view::ViewEntity& ve, const Weapon& w)
{
    if(w.anchor < 0 || !ve.ent.model || ve.ent.model->type != mod_alias)
    {
        return glm::vec3{0.f};
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(ve.ent.model));
    const glm::vec3 now = anchor::posedVertex(ve.ent, w.anchor, ve.zeroBlend);
    const glm::vec3 rest = anchor::posedVertex(ve.ent, w.anchor, 1.f);
    return (now - rest) * glm::vec3{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
}

// A direction in the model's space, in the world.
[[nodiscard]] glm::vec3 worldDir(const view::ViewEntity& ve, const glm::vec3& at, const glm::vec3& dir)
{
    const glm::vec3 d = view::modelPoint(ve, at + dir) - view::modelPoint(ve, at);
    const float l = glm::length(d);
    return l > 1e-6f ? d / l : glm::vec3{0.f, 0.f, 1.f};
}

// Follows each hand's first port, for the speed a casing leaves with (the hand's swing, a flick's
// spin, the player's own movement).
void trackPorts(const view::ViewEntity (&weapons)[2])
{
    for(int hand = 0; hand < 2; hand++)
    {
        const view::ViewEntity& ve = weapons[hand];
        Track& t = tracks[hand];
        const Weapon* w = ve.visible ? weaponFor(ve.ent.model) : nullptr;
        if(!w)
        {
            t = Track{};
            continue;
        }

        const glm::vec3 pos = view::modelPoint(ve, w->port[0] + animationShift(ve, *w));
        const float dt = static_cast<float>(cl.time - t.time);
        if(t.model == ve.ent.model && t.time >= 0.0 && dt > 0.f && dt < 0.2f)
        {
            glm::vec3 v = (pos - t.pos) / dt;
            const float speed = glm::length(v);
            const float cap = 12.f * unitsPerMetre(); // a teleport or a respawn is no swing
            v = speed > cap * 3.f ? glm::vec3{0.f} : speed > cap ? v * (cap / speed) : v;
            t.vel = glm::mix(t.vel, v, 0.6f);
        }
        else if(t.model != ve.ent.model)
        {
            t.vel = glm::vec3{0.f};
        }
        if(dt > 0.f || t.time < 0.0)
        {
            t.model = ve.ent.model;
            t.pos = pos;
            t.time = cl.time;
        }
    }
}

[[nodiscard]] Shell& newShell()
{
    Shell* oldest = &shells[0];
    for(Shell& s : shells)
    {
        if(!s.active)
        {
            return s;
        }
        if(s.born < oldest->born)
        {
            oldest = &s;
        }
    }
    return *oldest;
}

// A flick's casing, thrown out of the spinning gun: forward (the way the head looks, turned a
// little away from it towards where the gun is) and a little up, scattered, with a quarter of the
// hand's own swing (not the spin's speed, which flung them into the face) and the player's movement.
[[nodiscard]] glm::vec3 flickVelocity(int hand, const glm::vec3& pos)
{
    const hands::State& hs = hands::current();
    const float upm = unitsPerMetre();
    const glm::vec3 look = hands::forward({0.f, hs.headAngles.y, 0.f});
    glm::vec3 away{pos.x - hs.head.x, pos.y - hs.head.y, 0.f};
    away = glm::length(away) > 1e-3f ? glm::normalize(away) : look;
    glm::vec3 out = look * 0.7f + away * 0.3f;
    out = glm::length(out) > 1e-3f ? glm::normalize(out) : look;
    const glm::vec3 side = glm::normalize(glm::cross(out, glm::vec3{0.f, 0.f, 1.f}));

    glm::vec3 handVel = hs.vel[hand] * upm * 0.25f;
    const float cap = 1.5f * upm;
    if(glm::length(handVel) > cap)
    {
        handVel *= cap / glm::length(handVel);
    }
    const float speed = rnd(1.1f, 1.7f) * upm;
    return out * speed + side * (rnd(-0.35f, 0.35f) * upm) + glm::vec3{0.f, 0.f, rnd(0.3f, 0.7f) * upm} + handVel +
           glm::vec3{cl.velocity[0], cl.velocity[1], cl.velocity[2]};
}

// Never at the face: a casing starting within an arm's length of the head does not fly towards it
// (it goes away from it at least a little, along the ground), and does not rise to the eyes.
[[nodiscard]] glm::vec3 awayFromFace(const glm::vec3& pos, glm::vec3 vel)
{
    const hands::State& hs = hands::current();
    if(!hs.valid)
    {
        return vel;
    }
    const float upm = unitsPerMetre();
    const glm::vec3 player{cl.velocity[0], cl.velocity[1], 0.f};
    glm::vec3 toHead{hs.head.x - pos.x, hs.head.y - pos.y, 0.f};
    const float dist = glm::length(toHead);
    if(dist >= 1.f * upm || dist < 1e-3f)
    {
        return vel;
    }
    toHead /= dist;
    const float towards = glm::dot(vel - player, toHead); // relative to the player, who moves along
    const float wanted = -0.3f * upm;
    if(towards > wanted)
    {
        vel -= toHead * (towards - wanted);
    }
    // Its highest point at least 15 cm under the eyes.
    const float g = gravity();
    const float room = hs.head.z - 0.15f * upm - pos.z;
    const float maxUp = room > 0.f ? std::sqrt(2.f * g * room) : 0.f;
    vel.z = std::min(vel.z, maxUp);
    return vel;
}

void eject(const view::ViewEntity (&weapons)[2], const Pending& p)
{
    const view::ViewEntity& ve = weapons[p.hand];
    const Weapon* w = ve.visible ? weaponFor(ve.ent.model) : nullptr;
    if(!w || p.kind < 0 || p.kind >= static_cast<int>(std::size(kindModels)))
    {
        return; // the weapon is gone from the hand (thrown, holstered) or has no port
    }

    const bool flick = (p.flags & FlagFlick) != 0;
    const glm::vec3 shift = animationShift(ve, *w);
    const float upm = unitsPerMetre();

    for(int i = 0; i < p.count; i++)
    {
        const int port = i % w->ports;
        const glm::vec3 at = w->port[port] + shift;
        const glm::vec3 pos = view::modelPoint(ve, at);

        // The weapon's axes at the port (right-handed again when the model is mirrored).
        const glm::vec3 fwd = worldDir(ve, at, {1.f, 0.f, 0.f});
        glm::vec3 up = worldDir(ve, at, {0.f, 0.f, 1.f});
        const glm::vec3 left = glm::normalize(glm::cross(up, fwd));
        up = glm::cross(fwd, left);
        const glm::vec3 dir = worldDir(ve, at, w->dir[port]);

        Shell& s = newShell();
        s = Shell{};
        s.active = true;
        s.kind = p.kind;
        s.born = cl.time;
        s.pos = pos;
        s.smoke = flick ? 1.5f : 1.f;

        if(flick)
        {
            s.vel = flickVelocity(p.hand, pos);
        }
        else
        {
            const float speed = w->speed * upm * rnd(0.8f, 1.2f);
            const glm::vec3 scatter = onSphere() * (speed * 0.15f);
            s.vel = dir * speed + scatter + tracks[p.hand].vel * 0.9f;
        }
        s.vel = awayFromFace(pos, s.vel);

        // Lying in the chamber: its open end forward, the weapon's up its up.
        s.rot = glm::quat_cast(glm::mat3{fwd, left, up});
        const glm::vec3 spinAxis = worldDir(ve, at, w->spinAxis);
        s.angVel = spinAxis * (w->spin * rnd(0.75f, 1.25f)) + onSphere() * rnd(1.f, 4.f);
        if(flick)
        {
            s.angVel += onSphere() * rnd(12.f, 22.f);
        }

        particles::shellEject(pos, dir, w->smoke * (flick ? 1.5f : 1.f), w->sparks);
    }
}

void tink(Shell& s, float impact)
{
    const float threshold = 0.7f * unitsPerMetre(); // metres per second into the surface
    if(vr_shells_sound.value <= 0.f || impact < threshold || s.tinks >= 4 || cl.time < s.nextTink || inLiquid(s.pos))
    {
        return;
    }
    s.tinks++;
    s.nextTink = cl.time + 0.06;

    static const char* const names[] = {"vr/shell_tink1.wav", "vr/shell_tink2.wav", "vr/shell_tink3.wav"};
    sfx_t* sfx = S_PrecacheSound(names[std::uniform_int_distribution<int>{0, 2}(rng)]);
    if(!sfx)
    {
        return;
    }
    const float loud = std::clamp(impact / (4.f * unitsPerMetre()), 0.15f, 1.f);
    vec3_t org{s.pos.x, s.pos.y, s.pos.z};
    S_StartSound(0, 0, sfx, org, std::min(vr_shells_sound.value, 1.f) * 0.45f * loud, 2.f);
}

// The rotation turning unit vector `a` onto `b` (not opposite ones: a lying shell is never
// upside down along the floor's plane).
[[nodiscard]] glm::quat shortestArc(const glm::vec3& a, const glm::vec3& b)
{
    const glm::vec3 c = glm::cross(a, b);
    const glm::quat q{1.f + glm::dot(a, b), c.x, c.y, c.z};
    const float l = glm::length(q);
    return l > 1e-4f ? q / l : glm::quat{1.f, 0.f, 0.f, 0.f};
}

void spinBy(Shell& s, const glm::vec3& angVel, float dt)
{
    const float a = glm::length(angVel) * dt;
    if(a > 1e-6f)
    {
        s.rot = glm::normalize(glm::angleAxis(a, glm::normalize(angVel)) * s.rot);
    }
}

void fly(Shell& s, float dt)
{
    const bool liquid = inLiquid(s.pos);
    s.vel.z -= gravity() * dt * (liquid ? 0.2f : 1.f);
    s.vel *= std::exp(-(liquid ? 4.f : 0.15f) * dt);
    s.angVel *= std::exp(-(liquid ? 2.f : 0.1f) * dt);

    const glm::vec3 from = s.pos;
    const glm::vec3 to = s.pos + s.vel * dt;
    const trace_t tr = worldtrace::world(from, to);
    if(tr.startsolid || tr.allsolid)
    {
        s.resting = true; // stuck in something: leave it be
        s.vel = glm::vec3{0.f};
        return;
    }

    if(tr.fraction < 1.f)
    {
        const glm::vec3 n = worldtrace::normal(tr);
        const float into = -glm::dot(s.vel, n);
        glm::vec3 tangent = s.vel + n * into;
        tangent *= 0.65f; // friction of the hit
        const float bounce = into * rnd(0.3f, 0.45f);
        s.vel = tangent + n * bounce;
        s.angVel = s.angVel * 0.5f + onSphere() * std::min(into / unitsPerMetre() * 6.f, 25.f);
        s.pos = worldtrace::endPos(tr) + n * restHeight();
        tink(s, into);

        if(n.z > 0.7f && bounce < 0.6f * unitsPerMetre())
        {
            s.onGround = true;
            s.ground = n;
            s.vel -= n * glm::dot(s.vel, n);
        }
    }
    else
    {
        s.pos = to;
    }
    spinBy(s, s.angVel, dt);

    const float age = static_cast<float>(cl.time - s.born);
    if(age < trailTime && !liquid)
    {
        particles::shellTrail(from, s.pos, s.smoke * (1.f - age / trailTime));
    }
}

void roll(Shell& s, float dt)
{
    const glm::vec3 n = s.ground;
    const float r = restHeight();

    // Lie down on the side: the axis turns into the floor's plane.
    const glm::vec3 axis = s.rot * glm::vec3{1.f, 0.f, 0.f};
    glm::vec3 flat = axis - n * glm::dot(axis, n);
    flat = glm::length(flat) > 1e-3f ? glm::normalize(flat) : glm::normalize(glm::cross(n, glm::vec3{0.3f, 1.f, 0.f}));
    const glm::quat lieDown = shortestArc(axis, flat);
    s.rot = glm::normalize(glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, lieDown, std::min(1.f, dt * 14.f)) * s.rot);

    // Rolls across its axis, slides along it; both slow down.
    const glm::vec3 along = flat * glm::dot(s.vel, flat);
    const glm::vec3 across = s.vel - along;
    s.vel = along * std::exp(-7.f * dt) + across * std::exp(-2.2f * dt);
    spinBy(s, glm::cross(n, across) / r, dt);

    const glm::vec3 to = s.pos + s.vel * dt;
    const trace_t tr = worldtrace::world(s.pos, to);
    if(tr.fraction < 1.f && !tr.startsolid)
    {
        const glm::vec3 wn = worldtrace::normal(tr);
        s.vel -= wn * (1.3f * glm::dot(s.vel, wn)); // off a wall
        s.pos = worldtrace::endPos(tr) + wn * 0.05f;
    }
    else if(!tr.startsolid)
    {
        s.pos = to;
    }

    // Still on a floor? Off a ledge it falls again.
    const trace_t down = worldtrace::world(s.pos + n * r, s.pos - n * (r + 1.5f));
    if(down.fraction >= 1.f || down.startsolid || worldtrace::normal(down).z <= 0.7f)
    {
        s.onGround = false;
        return;
    }
    s.ground = worldtrace::normal(down);
    s.pos = worldtrace::endPos(down) + s.ground * r;

    if(glm::length(s.vel) < 0.02f * unitsPerMetre() && glm::dot(axis, flat) > 0.999f)
    {
        s.resting = true;
        s.vel = glm::vec3{0.f};
        s.nextCheck = cl.time + rnd(0.3f, 0.6f);
    }
}

// A resting casing looks now and then whether its floor is still under it.
void rest(Shell& s)
{
    if(cl.time < s.nextCheck)
    {
        return;
    }
    s.nextCheck = cl.time + rnd(0.3f, 0.6f);
    const float r = restHeight();
    const trace_t down = worldtrace::world(s.pos, s.pos - s.ground * (r + 1.f));
    if(down.fraction >= 1.f && !down.startsolid)
    {
        s.resting = false;
        s.onGround = false;
        s.tinks = 2; // a quieter landing
    }
}

void addToScene(Shell& s, qmodel_t* model, float life)
{
    const float age = static_cast<float>(cl.time - s.born);
    const float alpha = std::clamp((life - age) / fadeTime, 0.f, 1.f);
    if(!model || cl_numvisedicts >= MAX_VISEDICTS || alpha <= 0.f)
    {
        return;
    }

    entity_t& e = s.ent;
    if(e.model != model)
    {
        e = entity_t{};
        e.lerpflags |= LERP_RESETANIM;
    }
    e.model = model;
    e.frame = 0;
    e.skinnum = 0;
    e.colormap = vid.colormap;
    e.scale = static_cast<unsigned char>(CLAMP(1.f, units::worldScale() * ENTSCALE_DEFAULT * modelScale + 0.5f, 255.f));
    e.alpha = alpha >= 1.f ? ENTALPHA_DEFAULT : static_cast<unsigned char>(ENTALPHA_ENCODE(alpha));

    const glm::mat3 m = glm::mat3_cast(s.rot);
    const glm::vec3 a = hands::anglesFromVectors(m[0], m[2]);
    const glm::vec3 angles{-a.x, a.y, a.z}; // alias models' pitch is the other way
    for(int i = 0; i < 3; i++)
    {
        e.origin[i] = s.pos[i];
        e.angles[i] = angles[i];
    }
    cl_visedicts[cl_numvisedicts++] = &e;
}

void ejectTest_f()
{
    if(cls.state != ca_connected)
    {
        return;
    }
    const int hand = Cmd_Argc() > 1 ? CLAMP(0, Q_atoi(Cmd_Argv(1)), 1) : HAND_MAIN;
    const int count = Cmd_Argc() > 2 ? CLAMP(1, Q_atoi(Cmd_Argv(2)), 8) : 1;
    const int flags = Cmd_Argc() > 3 && Q_atoi(Cmd_Argv(3)) ? FlagFlick : 0;
    if(flags & FlagFlick)
    {
        flick::spin(hand); // as a flick: the casings leave with the spin
    }
    pending.push_back({hand, 0, count, flags, cl.time});
}

} // namespace

void parseEject()
{
    const int hand = MSG_ReadByte();
    const int kind = MSG_ReadByte();
    const int count = MSG_ReadByte();
    const int flags = MSG_ReadByte();
    const float delay = MSG_ReadByte() / 100.f;

    if(!vr_shells.value || (hand != HAND_OFF && hand != HAND_MAIN) || pending.size() >= 16)
    {
        return;
    }
    pending.push_back({hand, kind, std::min(count, 8), flags, cl.time + delay});
}

void frame(const view::ViewEntity (&weapons)[2])
{
    if(lastFrame == host_framecount)
    {
        return;
    }
    lastFrame = host_framecount;

    trackPorts(weapons);

    for(std::size_t i = 0; i < pending.size();)
    {
        // A flick's casings wait for the spin to turn the barrels down.
        const Pending& pe = pending[i];
        const float spun = (pe.flags & FlagFlick) ? flick::spinAngle(pe.hand) : -1.f;
        const bool waitSpin = spun >= 0.f && spun < flickEjectAngle && cl.time < pe.time + flickEjectWait;
        if(cl.time >= pe.time && !waitSpin)
        {
            if(vr_shells.value)
            {
                eject(weapons, pending[i]);
            }
            pending.erase(pending.begin() + static_cast<std::ptrdiff_t>(i));
        }
        else
        {
            i++;
        }
    }

    const float dt = lastRun >= 0.0 ? static_cast<float>(CLAMP(0.0, cl.time - lastRun, 0.05)) : 0.f;
    lastRun = cl.time;
    const float life = std::max(vr_shells_life.value, 1.f);

    qmodel_t* models[std::size(kindModels)];
    for(std::size_t k = 0; k < std::size(kindModels); k++)
    {
        models[k] = Mod_ForName(kindModels[k], false);
    }

    for(Shell& s : shells)
    {
        if(!s.active)
        {
            continue;
        }
        if(!vr_shells.value || cl.time - s.born > life || cl.time < s.born)
        {
            s.active = false;
            continue;
        }
        if(dt > 0.f)
        {
            if(s.resting)
            {
                rest(s);
            }
            else if(s.onGround)
            {
                roll(s, dt);
            }
            else
            {
                fly(s, dt);
            }
        }
        addToScene(s, models[s.kind], life);
    }
}

void clear()
{
    for(Shell& s : shells)
    {
        s.active = false;
    }
    pending.clear();
    for(Track& t : tracks)
    {
        t = Track{};
    }
    lastRun = -1.0;
}

void registerCommands()
{
    Cmd_AddCommand("vr_shells_eject", ejectTest_f);
}

// Casings in the world (vr_memstats).
int liveCount()
{
    int n = 0;
    for(const Shell& s : shells)
    {
        n += s.active ? 1 : 0;
    }
    return n;
}

} // namespace qvr::shells
