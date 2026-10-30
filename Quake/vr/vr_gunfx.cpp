// vr_gunfx.cpp -- see vr_gunfx.hpp.

#include "vr_gunfx.hpp"
#include "vr_adguns.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"
#include "vr_random.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"
#include "vr_weapon_ids.h"
#include "vr_weapons.hpp"

#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"

#include <stdlib.h>
#include <string.h>


namespace qvr::gunfx
{
namespace
{

using weapons::Key;

// What a weapon's shots look like (the brief's table, ROUND23.md "Punchier gunfire").
enum class Kind : int
{
    None,
    Shotgun,      // the shotgun, the grunts' gun, AD's defender: sparks, smoke, the port's puff
    SuperShotgun, // the super shotgun, the Widowmaker: each barrel's sparks, smoke, the open breech's puff
    Revolver,     // sparks, smoke (its casings at the reload: vr_shells.cpp)
    Nail,         // the nail pistol and nailguns: few small sparks, a small quick puff, the barrel's heat
    Launcher,     // the grenade launchers, the proximity gun: a few sparks, a large cloud
    Rocket,       // the rocket launchers: a few sparks, a large cloud, the backblast
    Flare,        // sparks (redder), smoke
    Energy        // the thunderbolt, the laser cannon, the enforcers' rifle, AD's plasma guns: electric arcs, no smoke
};
constexpr const char* kindNames[] = {"none", "shotgun", "supershotgun", "revolver", "nail", "launcher", "rocket", "flare",
    "energy"};

struct Look
{
    int sparks;         // a barrel's
    float sparkSpeed;   // units a second
    float sparkSpread;  // the cone's width (sideways speed / forward)
    float sparkSize;
    glm::vec3 sparkColour;
    int smoke;          // the muzzle's cone's puffs
    float smokeSize;
    float smokeSpeed;
    float smokeLife;
    float smokeAlpha;
    int arcs;           // ElectricSparks
    bool port;          // a shotgun's port's puff
    bool backblast;
    float heat;         // the barrel's heat a shot adds (it smokes over 0.35, cools 0.35 a second)
    int barrels;
};

constexpr Look looks[] = {
    // sparks: n speed spread size colour; smoke: n size speed life alpha; arcs port back heat barrels
    {0, 0.f, 0.f, 0.f, {0.f, 0.f, 0.f}, 0, 0.f, 0.f, 0.f, 0.f, 0, false, false, 0.f, 1},                       // None
    {12, 300.f, 0.2f, 1.f, {1.f, 0.72f, 0.35f}, 7, 1.1f, 150.f, 0.55f, 0.32f, 0, true, false, 0.12f, 1},        // Shotgun
    {10, 300.f, 0.2f, 1.f, {1.f, 0.72f, 0.35f}, 9, 1.2f, 160.f, 0.6f, 0.34f, 0, true, false, 0.15f, 2},         // SuperShotgun
    {10, 320.f, 0.16f, 0.9f, {1.f, 0.78f, 0.42f}, 6, 0.9f, 140.f, 0.5f, 0.3f, 0, false, false, 0.1f, 1},       // Revolver
    {4, 220.f, 0.18f, 0.7f, {1.f, 0.8f, 0.5f}, 2, 0.6f, 90.f, 0.32f, 0.2f, 0, false, false, 0.05f, 1},          // Nail
    {4, 160.f, 0.3f, 1.f, {1.f, 0.65f, 0.3f}, 10, 1.9f, 170.f, 0.8f, 0.3f, 0, false, false, 0.2f, 1},           // Launcher
    {5, 200.f, 0.28f, 1.f, {1.f, 0.65f, 0.3f}, 10, 2.f, 190.f, 0.8f, 0.3f, 0, false, true, 0.2f, 1},            // Rocket
    {10, 240.f, 0.22f, 1.f, {1.f, 0.45f, 0.25f}, 6, 1.f, 130.f, 0.55f, 0.3f, 0, false, false, 0.1f, 1},         // Flare
    {0, 0.f, 0.f, 0.f, {0.f, 0.f, 0.f}, 0, 0.f, 0.f, 0.f, 0.f, 5, false, false, 0.f, 1},                        // Energy
};
static_assert(za::getArraySize(looks) == za::getArraySize(kindNames));

[[nodiscard]] Kind kindOf(int wid)
{
    switch(wid)
    {
        case WID_SHOTGUN:
        case WID_GRUNTGUN:
        case WID_ADDEFENDER: return Kind::Shotgun;
        case WID_SUPER_SHOTGUN: return Kind::SuperShotgun;
        case WID_REVOLVER: return Kind::Revolver;
        case WID_NAILPISTOL:
        case WID_NAILGUN:
        case WID_SUPER_NAILGUN: return Kind::Nail;
        case WID_GRENADE_LAUNCHER:
        case WID_PROXIMITY_GUN:
        case WID_ADGRENADE: return Kind::Launcher;
        case WID_ROCKET_LAUNCHER:
        case WID_ADROCKET: return Kind::Rocket;
        case WID_FLAREGUN: return Kind::Flare;
        case WID_LIGHTNING:
        case WID_LASER_CANNON:
        case WID_ENFRIFLE:
        case WID_ADPLASMA:
        case WID_ADCENTURION: return Kind::Energy;
        default: return Kind::None; // the flamethrowers, the crossbow, melee, the grapple, the toolgun, Mjolnir
    }
}

// Our weapons' models (the name before any '#'): their ids (QC vr_weaponutil.qc VR_WEAPONS, both models of a row).
struct ModelId
{
    const char* name;
    int wid;
};
constexpr ModelId modelIds[] = {
    {"progs/v_shot.mdl", WID_SHOTGUN},
    {"progs/v_shot2.mdl", WID_SUPER_SHOTGUN},
    {"progs/v_shot3.mdl", WID_SUPER_SHOTGUN}, // AD's Widowmaker ("progs/v_shot3.mdl#vr")
    {"progs/v_widowmaker.mdl", WID_SUPER_SHOTGUN},
    {"progs/v_nail.mdl", WID_NAILGUN},
    {"progs/v_lava.mdl", WID_NAILGUN},
    {"progs/v_nail2.mdl", WID_SUPER_NAILGUN},
    {"progs/v_lava2.mdl", WID_SUPER_NAILGUN},
    {"progs/v_rock.mdl", WID_GRENADE_LAUNCHER},
    {"progs/v_multi.mdl", WID_GRENADE_LAUNCHER},
    {"progs/v_prox.mdl", WID_PROXIMITY_GUN},
    {"progs/v_rock2.mdl", WID_ROCKET_LAUNCHER},
    {"progs/v_multi2.mdl", WID_ROCKET_LAUNCHER},
    {"progs/v_light.mdl", WID_LIGHTNING},
    {"progs/v_plasma.mdl", WID_LIGHTNING},
    {"progs/v_laserg.mdl", WID_LASER_CANNON},
    {"progs/v_gruntgun.mdl", WID_GRUNTGUN},
    {"progs/v_enfrifle.mdl", WID_ENFRIFLE},
    {"progs/v_nailpistol.mdl", WID_NAILPISTOL},
    {"progs/v_flaregun.mdl", WID_FLAREGUN},
    {"progs/v_revolver.mdl", WID_REVOLVER},
};

// Each shooter's last shot (the fire rate's budget): your hands, then the monsters' (the oldest given up).
struct Shooter
{
    int key{-1};
    double last{-1.0};
};
constexpr int maxShooters = 18;
Shooter shooters[maxShooters];

// Your hands: a shot due at the next frame, the barrel's heat.
struct HandState
{
    bool due{false};
    int slot{-1};
    float heat{0.f};
    float wisps{0.f}; // the wisps owed (fractions carried)
    double lastFrame{-1.0};
};
HandState hands[2];

za::FastNonCryptoRng rng{0x9E3779B97F4A7C15ull}; // (visual only)

// The budget of a shot by `key`: 1 at a shot every 0.3 s or slower, down to 0.35 at the super nailgun's rate and faster.
[[nodiscard]] float rateScale(int key)
{
    Shooter* s = &shooters[0];
    for(Shooter& o : shooters)
    {
        if(o.key == key)
        {
            s = &o;
            break;
        }
        if(o.last < s->last)
        {
            s = &o;
        }
    }
    const double since = s->key == key && s->last >= 0.0 ? cl.time - s->last : 1.0;
    s->key = key;
    s->last = cl.time;
    return za::clamp(static_cast<float>(since) / 0.3f, 0.35f, 1.f);
}

[[nodiscard]] int scaled(int n, float budget)
{
    if(n <= 0 || budget <= 0.f)
    {
        return 0;
    }
    return za::max(1, static_cast<int>(static_cast<float>(n) * budget + 0.5f));
}

// A direction in a cone round `fwd` (`spread`: its sideways reach), `right` and `up` across it.
[[nodiscard]] glm::vec3 inCone(const glm::vec3& fwd, const glm::vec3& right, const glm::vec3& up, float spread)
{
    return glm::normalize(fwd + right * vrand::range(rng, -spread, spread) + up * vrand::range(rng, -spread, spread));
}

struct Counts
{
    int sparks{0}, smoke{0}, glow{0}, arcs{0}, port{0}, back{0}, dust{0};
};

int put(const particles::GunParticle& g)
{
    return particles::gunParticle(g) ? 1 : 0;
}

void sparksAt(const glm::vec3& at, const Muzzle& m, const glm::vec3& right, const glm::vec3& up, const Look& look, int n,
    Counts& c)
{
    for(int i = 0; i < n; i++)
    {
        particles::GunParticle g;
        g.kind = particles::GunParticle::Spark;
        const glm::vec3 dir = inCone(m.fwd, right, up, look.sparkSpread);
        g.org = at + dir * vrand::range(rng, 0.f, 1.5f);
        g.vel = dir * (look.sparkSpeed * vrand::range(rng, 0.45f, 1.2f)) + m.drift;
        g.color = glm::vec4{glm::mix(look.sparkColour, glm::vec3{1.f, 0.93f, 0.7f}, vrand::range(rng, 0.f, 0.6f)), 1.f};
        g.scale = look.sparkSize * vrand::range(rng, 0.35f, 0.6f);
        g.life = vrand::range(rng, 0.06f, 0.18f);
        g.drag = 3.f;
        g.gravity = 0.4f;
        g.streak = 0.022f;
        c.sparks += put(g);
    }
}

// A cone of smoke out of `at` along `dir`: `n` puffs, `size`, `speed`, `life`, `alpha`, `colour` (grey by default),
// the shooter's motion in it.
void smokeCone(const glm::vec3& at, const glm::vec3& dir, const glm::vec3& right, const glm::vec3& up, const glm::vec3& drift,
    int n, float size, float speed, float life, float alpha, float spread, int& count, const glm::vec3& tint = glm::vec3{1.f})
{
    for(int i = 0; i < n; i++)
    {
        particles::GunParticle g;
        g.kind = particles::GunParticle::Smoke;
        const glm::vec3 d = inCone(dir, right, up, spread);
        g.org = at + d * vrand::range(rng, 0.f, 2.f);
        g.vel = d * (speed * vrand::range(rng, 0.35f, 1.15f)) + drift * 0.8f;
        const float grey = vrand::range(rng, 0.4f, 0.52f);
        g.color = glm::vec4{glm::vec3{grey, grey, grey * 0.97f} * tint, alpha * vrand::range(rng, 0.7f, 1.f)};
        g.scale = size * vrand::range(rng, 2.5f, 4.f);
        g.grow = size * vrand::range(rng, 18.f, 28.f);
        g.life = life * vrand::range(rng, 0.7f, 1.1f);
        g.drag = 6.f;
        g.gravity = -0.04f;
        count += put(g);
    }
}

// The flash's light in its smoke: a warm glow filling the cone's start for a moment.
void flashGlow(const Muzzle& m, const Look& look, Counts& c)
{
    particles::GunParticle g;
    g.kind = particles::GunParticle::Glow;
    g.org = m.at + m.fwd * (2.f * look.smokeSize);
    g.vel = m.fwd * 30.f + m.drift;
    g.color = glm::vec4{1.f, 0.62f, 0.28f, 0.45f};
    g.scale = 6.f * look.smokeSize;
    g.grow = 30.f * look.smokeSize;
    g.life = 0.07f;
    g.drag = 6.f;
    c.glow += put(g);
}

// The backblast: smoke out behind the launcher and dust off the floor under its back, if near.
void backblast(const Muzzle& m, const glm::vec3& right, const glm::vec3& up, float budget, Counts& c)
{
    const float length = m.gunLength > 0.f ? m.gunLength : 30.f;
    const glm::vec3 rear = m.at - m.fwd * length;
    smokeCone(rear, -m.fwd, right, up, m.drift, scaled(8, budget), 1.6f, 200.f, 0.75f, 0.28f, 0.4f, c.back);
    const glm::vec3 below = rear - glm::vec3{0.f, 0.f, 80.f};
    const float f = worldtrace::line(rear, below);
    if(f >= 1.f)
    {
        return;
    }
    const glm::vec3 floor = rear + (below - rear) * f + glm::vec3{0.f, 0.f, 2.f};
    const glm::vec3 back = glm::normalize(glm::vec3{-m.fwd.x, -m.fwd.y, 0.f} + glm::vec3{1e-4f, 0.f, 0.f});
    const int n = scaled(6, budget * (1.f - f * 0.6f));
    for(int i = 0; i < n; i++)
    {
        particles::GunParticle g;
        g.kind = particles::GunParticle::Smoke;
        const float a = vrand::range(rng, 0.f, 6.2831853f);
        const glm::vec3 out = glm::vec3{za::cos(a), za::sin(a), 0.f} * 0.6f + back;
        g.org = floor + out * vrand::range(rng, 2.f, 8.f);
        g.vel = out * vrand::range(rng, 40.f, 90.f) + glm::vec3{0.f, 0.f, vrand::range(rng, 8.f, 20.f)};
        const float k = vrand::range(rng, 0.3f, 0.38f);
        g.color = glm::vec4{k, k * 0.9f, k * 0.76f, 0.26f * vrand::range(rng, 0.7f, 1.f)};
        g.scale = vrand::range(rng, 3.f, 5.f);
        g.grow = vrand::range(rng, 15.f, 25.f);
        g.life = vrand::range(rng, 0.8f, 1.3f);
        g.drag = 3.f;
        g.gravity = -0.01f;
        c.dust += put(g);
    }
}

[[nodiscard]] bool on(const cvar_t& v)
{
    return v.value != 0.f;
}

// The shot itself (shot() and the test): returns what it made.
void report(int wid, Kind kind, int shooter, float budget, const Counts& c)
{
    if(vr_debug_gunfx.value)
    {
        Con_Printf("gunfx shot who %s %d wid %d kind %s budget %.2f sparks %d smoke %d glow %d arcs %d port %d back %d dust %d\n",
            shooter < 2 ? "player" : "enemy", shooter, wid, kindNames[static_cast<int>(kind)], budget, c.sparks, c.smoke, c.glow,
            c.arcs, c.port, c.back, c.dust);
    }
}

Counts emit(int wid, const Muzzle& m, int shooter, bool adBurst, bool ports)
{
    Counts c;
    const Kind kind = kindOf(wid);
    const Look& look = looks[static_cast<int>(kind)];
    const float budget = rateScale(shooter) * za::clamp(vr_gunfx_intensity.value, 0.f, 3.f);
    if(kind == Kind::None || budget <= 0.f || !particles::enabled())
    {
        report(wid, kind, shooter, budget, c);
        return c;
    }
    glm::vec3 right = glm::cross(m.fwd, m.up);
    if(glm::dot(right, right) < 1e-6f)
    {
        right = glm::vec3{0.f, 1.f, 0.f};
    }
    right = glm::normalize(right);
    const glm::vec3 up = glm::normalize(glm::cross(right, m.fwd));
    if(on(vr_gunfx_sparks) && !adBurst)
    {
        for(int b = 0; b < look.barrels; b++)
        {
            const float side = look.barrels > 1 ? (b == 0 ? -1.4f : 1.4f) : 0.f;
            sparksAt(m.at + right * side, m, right, up, look, scaled(look.sparks, budget), c);
        }
        if(look.arcs > 0)
        {
            const int n = scaled(look.arcs, budget);
            c.arcs += particles::spawn(m.at, m.fwd, particles::Preset::ElectricSparks, n) ? n : 0;
        }
    }
    if(on(vr_gunfx_smoke) && look.smoke > 0)
    {
        smokeCone(m.at, m.fwd, right, up, m.drift, scaled(look.smoke, budget), look.smokeSize, look.smokeSpeed, look.smokeLife,
            look.smokeAlpha, 0.22f, c.smoke);
        flashGlow(m, look, c);
    }
    if(on(vr_gunfx_port) && look.port && !ports)
    {
        // A gun that throws no casings (the grunts'): its port's puff now, out of its right side behind the muzzle.
        const float length = m.gunLength > 0.f ? m.gunLength * 0.6f : 16.f;
        smokeCone(m.at - m.fwd * length + right * 1.f, glm::normalize(right + up * 0.5f), m.fwd, up, m.drift,
            scaled(3, budget), 0.5f, 40.f, 0.45f, 0.28f, 0.35f, c.port);
    }
    if(on(vr_gunfx_backblast) && look.backblast && !adBurst)
    {
        backblast(m, right, up, budget, c);
    }
    report(wid, kind, shooter, budget, c);
    return c;
}

// The barrel's heat: wisps of smoke off a hot muzzle (`dt` seconds since the last frame).
void heatWisps(HandState& h, const Muzzle& m, float dt)
{
    h.heat = za::max(0.f, h.heat - 0.35f * dt);
    const float over = h.heat - 0.35f;
    if(over <= 0.f || !on(vr_gunfx_heat))
    {
        h.wisps = 0.f;
        return;
    }
    h.wisps += over * 24.f * dt * za::clamp(vr_gunfx_intensity.value, 0.f, 3.f);
    for(; h.wisps >= 1.f; h.wisps -= 1.f)
    {
        particles::GunParticle g;
        g.kind = particles::GunParticle::Smoke;
        g.org = m.at + glm::vec3{vrand::range(rng, -0.4f, 0.4f), vrand::range(rng, -0.4f, 0.4f), vrand::range(rng, 0.f, 0.6f)};
        g.vel = m.fwd * vrand::range(rng, 1.f, 3.f) + glm::vec3{0.f, 0.f, vrand::range(rng, 8.f, 14.f)};
        const float grey = vrand::range(rng, 0.42f, 0.52f);
        g.color = glm::vec4{grey, grey, grey, 0.12f * za::min(over * 2.f + 0.4f, 1.f)};
        g.scale = vrand::range(rng, 0.8f, 1.2f);
        g.grow = vrand::range(rng, 4.f, 6.f);
        g.life = vrand::range(rng, 0.9f, 1.4f);
        g.drag = 1.5f;
        g.gravity = -0.06f;
        (void)particles::gunParticle(g);
    }
}

// vr_gunfx_test <wid> [count]: that weapon's shot effects `count` times (1), 48 units ahead of the view, pointing away.
void test_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_gunfx_test <weapon id> [count]: its shots' particles 48 units ahead\n");
        return;
    }
    const int wid = atoi(Cmd_Argv(1));
    const int count = Cmd_Argc() > 2 ? za::clamp(atoi(Cmd_Argv(2)), 1, 64) : 1;
    vec3_t f, r, u;
    AngleVectors(r_refdef.viewangles, f, r, u); // (the view drawn: the head's)
    Muzzle m;
    m.fwd = glm::vec3{f[0], f[1], f[2]};
    m.up = glm::vec3{u[0], u[1], u[2]};
    m.at = glm::vec3{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]} + m.fwd * 48.f - m.up * 8.f;
    for(int i = 0; i < count; i++)
    {
        (void)emit(wid, m, 2 + 4096, false, false);
    }
    Con_Printf("vr_gunfx_test: weapon %d (%s) x%d at %.1f %.1f %.1f\n", wid, kindNames[static_cast<int>(kindOf(wid))], count,
        m.at.x, m.at.y, m.at.z);
}

} // namespace

int weaponIdOf(const qmodel_t* model)
{
    if(!model)
    {
        return -1;
    }
    if(const int v = adguns::variantOf(model->name); v >= 0)
    {
        return v == adguns::EliminatorLegacy ? WID_ADCENTURION : WID_ADROCKET + v;
    }
    const char* hash = strchr(model->name, '#');
    const size_t len = hash ? static_cast<size_t>(hash - model->name) : strlen(model->name);
    for(const ModelId& id : modelIds)
    {
        if(strlen(id.name) == len && strncmp(id.name, model->name, len) == 0)
        {
            return id.wid;
        }
    }
    return -1;
}

void shot(int wid, const Muzzle& m, int shooter, bool adBurst, bool ports)
{
    if(!on(vr_gunfx))
    {
        return;
    }
    QVR_PROFILE("gunfx");
    (void)emit(wid, m, shooter, adBurst, ports);
}

void playerFired(int hand)
{
    if(hand >= 0 && hand < 2)
    {
        hands[hand].due = true;
    }
}

void handFrame(int hand, const view::ViewEntity& w, int slot)
{
    HandState& h = hands[hand];
    const float dt = h.lastFrame >= 0.0 ? za::clamp(static_cast<float>(cl.time - h.lastFrame), 0.f, 0.1f) : 0.f;
    h.lastFrame = cl.time;
    if(slot != h.slot)
    {
        h.heat = 0.f; // (another weapon, or none)
        h.wisps = 0.f;
        h.slot = slot;
    }
    const bool due = h.due;
    h.due = false;
    if(slot < 0 || !w.visible || !w.ent.model || (!due && h.heat <= 0.f))
    {
        return;
    }
    const int wid = weaponIdOf(w.ent.model);
    const Kind kind = kindOf(wid);
    if(kind == Kind::None)
    {
        h.heat = 0.f;
        if(due)
        {
            shot(wid, Muzzle{}, hand, false, true); // (nothing made: vr_debug_gunfx says so)
        }
        return;
    }
    Muzzle m;
    m.at = view::anchorPosition(w, weapons::anchor(slot, Key::MuzzleAnchor),
        weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
    const glm::vec3 o = view::modelPoint(w, glm::vec3{0.f});
    const glm::vec3 ax = view::modelPoint(w, glm::vec3{1.f, 0.f, 0.f}) - o;
    const glm::vec3 az = view::modelPoint(w, glm::vec3{0.f, 0.f, 1.f}) - o;
    if(glm::length(ax) < 1e-5f || glm::length(az) < 1e-5f)
    {
        return;
    }
    m.fwd = glm::normalize(ax);
    m.up = glm::normalize(az);
    m.drift = glm::vec3{cl.velocity[0], cl.velocity[1], cl.velocity[2]};
    m.gunLength = 1.1f * units::metresToUnits();
    if(due && on(vr_gunfx))
    {
        // An AD gun's own burst (vr_weaponfx.cpp adGunBurst) made its sparks and backblast already.
        const int variant = adguns::variantOf(w.ent.model->name);
        const bool adBurst = variant >= 0 && adguns::fireFeel(variant).burst != adguns::FireFeel::None;
        // The shotgun, the super shotgun and the Widowmaker throw their casings out of their ports (vr_shells.cpp).
        const bool ports = wid == WID_SHOTGUN || wid == WID_SUPER_SHOTGUN;
        shot(wid, m, hand, adBurst, ports);
        h.heat = za::min(h.heat + looks[static_cast<int>(kind)].heat, 1.5f);
    }
    if(on(vr_gunfx))
    {
        heatWisps(h, m, dt);
    }
}

void enemyFired(int ent, const qmodel_t* model, int wid, const Muzzle& m)
{
    if(!on(vr_gunfx_enemies))
    {
        return;
    }
    shot(model ? weaponIdOf(model) : wid, m, 2 + ent, false, false);
}

void portSmoke(const glm::vec3& at, const glm::vec3& dir, bool hot)
{
    if(!on(vr_gunfx) || !on(vr_gunfx_port) || !particles::enabled())
    {
        return;
    }
    const float budget = za::clamp(vr_gunfx_intensity.value, 0.f, 3.f) * (hot ? 1.6f : 1.f);
    glm::vec3 right = glm::cross(dir, glm::vec3{0.f, 0.f, 1.f});
    right = glm::dot(right, right) < 1e-6f ? glm::vec3{0.f, 1.f, 0.f} : glm::normalize(right);
    const glm::vec3 up = glm::normalize(glm::cross(right, dir));
    int n = 0;
    smokeCone(at, dir, right, up, glm::vec3{0.f}, scaled(4, budget), 0.5f, 45.f, 0.45f, 0.3f, 0.35f, n);
    if(vr_debug_gunfx.value)
    {
        Con_Printf("gunfx port at %.1f %.1f %.1f smoke %d%s\n", at.x, at.y, at.z, n, hot ? " (hot)" : "");
    }
}

float shellTrail()
{
    return on(vr_gunfx) ? za::max(vr_gunfx_shell_trail.value, 0.f) : 1.f;
}

void clear()
{
    for(Shooter& s : shooters)
    {
        s = Shooter{};
    }
    for(HandState& h : hands)
    {
        h = HandState{};
    }
}

void registerCommands()
{
    Cmd_AddCommand("vr_gunfx_test", test_f);
}

} // namespace qvr::gunfx
