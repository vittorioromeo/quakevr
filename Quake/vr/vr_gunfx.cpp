// vr_gunfx.cpp -- see vr_gunfx.hpp.

#include "vr_gunfx.hpp"
#include "vr_adguns.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_impacts.hpp"
#include "vr_muzzlewisps.hpp"
#include "vr_particles.hpp"
#include "vr_geom.hpp"
#include "vr_profile.hpp"
#include "vr_random.hpp"
#include "vr_render.hpp"
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
    {10, 380.f, 0.2f, 1.9f, {1.f, 0.74f, 0.38f}, 14, 1.3f, 260.f, 0.55f, 0.55f, 0, true, false, 0.12f, 1},     // Shotgun
    {8, 380.f, 0.2f, 1.9f, {1.f, 0.74f, 0.38f}, 16, 1.5f, 280.f, 0.6f, 0.6f, 0, true, false, 0.15f, 2},       // SuperShotgun
    {8, 400.f, 0.17f, 1.8f, {1.f, 0.8f, 0.45f}, 10, 1.1f, 240.f, 0.5f, 0.5f, 0, false, false, 0.1f, 1},       // Revolver
    {4, 300.f, 0.18f, 1.5f, {1.f, 0.8f, 0.5f}, 5, 0.75f, 160.f, 0.35f, 0.4f, 0, false, false, 0.07f, 1},      // Nail
    {6, 220.f, 0.28f, 1.6f, {1.f, 0.65f, 0.3f}, 14, 2.1f, 220.f, 0.8f, 0.5f, 0, false, false, 0.2f, 1},       // Launcher
    {7, 260.f, 0.26f, 1.6f, {1.f, 0.65f, 0.3f}, 14, 2.2f, 250.f, 0.8f, 0.5f, 0, false, true, 0.2f, 1},        // Rocket
    {10, 320.f, 0.2f, 1.8f, {1.f, 0.45f, 0.25f}, 10, 1.2f, 210.f, 0.55f, 0.5f, 0, false, false, 0.1f, 1},     // Flare
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
    float vent{0.f};     // the nail pistol's vents still smoking, seconds (each shot adds)
    float exhaust{0.f};  // a rocket launcher's exhaust still smoking, seconds
    float ventOwed{0.f}; // (their puffs owed, fractions carried)
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
    int sparks{0}, smoke{0}, glow{0}, arcs{0}, port{0}, back{0}, dust{0}, vent{0}, exhaust{0};
    float light{-1.f}; // the place's light at the muzzle (placeLight), -1 if not looked up
};

lightcache_t placeCache; // (placeLight's)

// The place's light at `at` (the lightmap under it, R_LightPoint: 1 at Quake's full light), once a shot: the smoke's
// grey follows it (drawn emissive, as the flash lit it: never black in the dark, never glowing in a lit room).
[[nodiscard]] float placeLight(const glm::vec3& at)
{
    if(!cl.worldmodel)
    {
        return 1.f;
    }
    vec3_t v{at.x, at.y, at.z};
    return za::clamp(static_cast<float>(R_LightPoint(v, 0.f, &placeCache)) / 128.f, 0.f, 1.6f);
}

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
        // (out of the flash's core already: a few units ahead, so neither the flash nor the cone's first puffs hide them)
        g.org = at + dir * vrand::range(rng, 1.5f, 4.f);
        g.vel = dir * (look.sparkSpeed * vrand::range(rng, 0.5f, 1.15f)) + m.drift;
        g.color = glm::vec4{glm::mix(look.sparkColour, glm::vec3{1.f, 0.95f, 0.78f}, vrand::range(rng, 0.2f, 0.8f)), 1.f};
        g.scale = look.sparkSize * vrand::range(rng, 0.85f, 1.25f);
        g.life = vrand::range(rng, 0.12f, 0.28f);
        g.drag = 2.4f;
        g.gravity = 0.5f;
        g.streak = 0.035f;
        c.sparks += put(g);
    }
}

// The smoke's grey in a place lit `light` (placeLight): it is drawn emissive, so it takes the place's light here once.
// Lighter in a lit room, a dim grey (never black, never glowing) in the dark.
float smokeLight = 1.f; // (this shot's: emit() sets it)

[[nodiscard]] float smokeGrey()
{
    return vrand::range(rng, 0.9f, 1.05f) * (0.3f + 0.3f * za::min(smokeLight, 1.3f));
}

// How a cone of smoke moves: its swirl (turbulence, units a second squared; vr_gunfx_turbulence scales it), the seconds
// it fades in over (the flash's moment: it billows out of the muzzle with it, not before), how fast it rises (times
// Quake's gravity, up) and how far ahead of `at` its puffs start at the cone's tip (units).
struct ConeMotion
{
    float turb, fadeIn, rise, lead;
};
constexpr ConeMotion frontal{150.f, 0.035f, 0.008f, 4.f}; // the muzzle's: forward, expanding, swirling, barely rising
constexpr ConeMotion rising{50.f, 0.f, 0.04f, 0.f};       // a port's puff: up and away
constexpr ConeMotion blast{170.f, 0.03f, 0.012f, 3.f};    // a rocket's backblast out of its exhaust

// A cone of smoke out of `at` along `dir`: `n` puffs, `size`, `speed` (the cone's reach is about speed / 6 units: the
// drag), `life`, `alpha`, its `motion`, `colour` (grey by default), the shooter's motion in it. The puffs are spread
// along the cone: the first slow, wide and dense at the muzzle (they rise a little: the smoke left hanging there), the
// last fast, narrow and thinner at its tip; each grows as it slows and the swirl carries it about, so the cone billows
// forward, opens out turbulently and fades over its life.
void smokeCone(const glm::vec3& at, const glm::vec3& dir, const glm::vec3& right, const glm::vec3& up, const glm::vec3& drift,
    int n, float size, float speed, float life, float alpha, float spread, const ConeMotion& motion, int& count,
    const glm::vec3& tint = glm::vec3{1.f})
{
    const float turb = motion.turb * za::clamp(vr_gunfx_turbulence.value, 0.f, 3.f);
    for(int i = 0; i < n; i++)
    {
        const float t = n > 1 ? static_cast<float>(i) / static_cast<float>(n - 1) : 0.5f; // muzzle .. tip
        particles::GunParticle g;
        g.kind = particles::GunParticle::Smoke;
        const glm::vec3 d = inCone(dir, right, up, spread * (1.3f - 0.7f * t));
        g.org = at + d * (vrand::range(rng, 0.f, 2.f) + motion.lead * t);
        g.vel = d * (speed * (0.12f + 0.95f * t) * vrand::range(rng, 0.85f, 1.15f)) + drift * 0.8f;
        const float grey = smokeGrey();
        g.color = glm::vec4{glm::vec3{grey, grey, grey * 0.97f} * tint, alpha * (1.f - 0.35f * t) * vrand::range(rng, 0.8f, 1.f)};
        g.scale = size * vrand::range(rng, 1.6f, 2.6f);
        g.grow = size * vrand::range(rng, 35.f, 50.f) * (0.7f + 0.6f * t);
        g.life = life * vrand::range(rng, 0.75f, 1.1f);
        g.drag = 6.f;
        g.gravity = -(t < 0.25f ? za::max(motion.rise, 0.04f) : motion.rise);
        g.turb = turb * vrand::range(rng, 0.7f, 1.3f);
        g.fadeIn = motion.fadeIn * vrand::range(rng, 0.7f, 1.3f);
        count += put(g);
    }
}

// A drawn gun's model (its own coordinates, as anchors and Blender have them) as placed in the world now: a point is
// `o + x * px + y * py + z * pz`. False if the muzzle comes with no gun (a monster's own).
struct GunFrame
{
    glm::vec3 o, x, y, z;
};
[[nodiscard]] const qmodel_t* gunModel(const Muzzle& m)
{
    return m.view ? m.view->ent.model : m.gun ? m.gun->model : nullptr;
}
[[nodiscard]] bool gunFrame(const Muzzle& m, GunFrame& f)
{
    if(m.view)
    {
        f.o = view::modelPoint(*m.view, glm::vec3{0.f});
        f.x = view::modelPoint(*m.view, glm::vec3{1.f, 0.f, 0.f}) - f.o;
        f.y = view::modelPoint(*m.view, glm::vec3{0.f, 1.f, 0.f}) - f.o;
        f.z = view::modelPoint(*m.view, glm::vec3{0.f, 0.f, 1.f}) - f.o;
        return true;
    }
    if(!m.gun || !m.gun->model || m.gun->model->type != mod_alias)
    {
        return false;
    }
    if(const view::ViewEntity* ve = view::find(m.gun))
    {
        Muzzle v;
        v.view = ve;
        return gunFrame(v, f);
    }
    // (entityMatrix maps the model's bytes: a model point is (p - scale_origin) / scale of them)
    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(m.gun->model));
    float a[16];
    render::entityMatrix(*m.gun, false, m.gun->scale ? m.gun->scale : ENTSCALE_DEFAULT, glm::vec3{0.f}, a);
    const glm::vec3 s = geom::vec(hdr->scale), so = geom::vec(hdr->scale_origin);
    const auto col = [&](int k) { return glm::vec3{a[k * 4], a[k * 4 + 1], a[k * 4 + 2]}; };
    f.x = col(0) / s.x;
    f.y = col(1) / s.y;
    f.z = col(2) / s.z;
    f.o = col(3) - f.x * so.x - f.y * so.y - f.z * so.z;
    return true;
}
[[nodiscard]] glm::vec3 at(const GunFrame& f, const glm::vec3& p)
{
    return f.o + f.x * p.x + f.y * p.y + f.z * p.z;
}

// The nail pistol's side vents (Misc/quakevr/models_src/nailpistol; v_nailpistol.mdl): three slots in each side of its
// receiver, x 4.91..7.57, the side faces at y +-1.2, each slot's z below.
constexpr float ventX0 = 4.91f, ventX1 = 7.57f, ventSide = 1.2f;
constexpr float ventZ[] = {3.08f, 3.83f, 4.59f};

// Light smoke out of `n` places in the nail pistol's vents (random slots, along each): drifting out of the side and up,
// a pale grey (not dark: the heat leaving the gun), thin, swirling a little; `strength` its opacity's share.
int ventSmoke(const GunFrame& f, const glm::vec3& drift, int n, float strength)
{
    if(n <= 0 || !particles::enabled())
    {
        return 0;
    }
    const float turb = 40.f * za::clamp(vr_gunfx_turbulence.value, 0.f, 3.f);
    const float light = za::min(smokeLight, 1.3f);
    const glm::vec3 outY = glm::normalize(f.y + glm::vec3{1e-6f});
    int made = 0;
    for(int i = 0; i < n; i++)
    {
        const float side = (rng.getI(0, 1) == 0) ? -1.f : 1.f;
        const float z = ventZ[rng.getI(0, 2)] + vrand::range(rng, -0.12f, 0.12f);
        const float x = vrand::range(rng, ventX0 + 0.25f, ventX1 - 0.25f);
        particles::GunParticle g;
        g.kind = particles::GunParticle::Smoke;
        g.org = at(f, glm::vec3{x, side * (ventSide + 0.05f), z});
        g.vel = outY * (side * vrand::range(rng, 4.f, 10.f)) + glm::vec3{0.f, 0.f, vrand::range(rng, 9.f, 16.f)} + drift * 0.8f;
        const float grey = za::min(0.95f, vrand::range(rng, 0.95f, 1.05f) * (0.55f + 0.3f * light));
        g.color = glm::vec4{grey, grey, grey * 0.98f, strength * vrand::range(rng, 0.22f, 0.32f)};
        g.scale = vrand::range(rng, 0.45f, 0.75f);
        g.grow = vrand::range(rng, 5.f, 9.f);
        g.life = vrand::range(rng, 0.6f, 1.f);
        g.drag = 2.f;
        g.gravity = -0.05f;
        g.turb = turb;
        g.fadeIn = 0.06f;
        made += put(g);
    }
    return made;
}

// A rocket launcher's exhaust (its back end, out of which the backblast goes): where and which way, in the world.
// The AD rocket launcher's from its own settings (adguns::FireFeel), ours (v_rock2.mdl) its tube's back end; else
// behind the muzzle by the gun's length.
struct ModelExhaust
{
    const char* name;
    glm::vec3 at, dir;
};
constexpr ModelExhaust exhausts[] = {
    {"progs/v_rock2.mdl", {-6.3f, -0.04f, 4.78f}, {-1.f, 0.f, 0.f}}, // (the tube's back ring, middle 4.78 high)
};
void exhaustOf(const Muzzle& m, glm::vec3& where, glm::vec3& dir, bool& onGun)
{
    const float length = m.gunLength > 0.f ? m.gunLength : 30.f;
    where = m.at - m.fwd * length;
    dir = -m.fwd;
    onGun = false;
    const qmodel_t* model = gunModel(m);
    GunFrame f;
    if(!model || !gunFrame(m, f))
    {
        return;
    }
    glm::vec3 p, d;
    if(const int v = adguns::variantOf(model->name); v >= 0 && adguns::fireFeel(v).backBlast)
    {
        p = adguns::fireFeel(v).exhaust;
        d = adguns::fireFeel(v).exhaustDir;
    }
    else
    {
        const char* hash = strchr(model->name, '#');
        const size_t len = hash ? static_cast<size_t>(hash - model->name) : strlen(model->name);
        const ModelExhaust* e = nullptr;
        for(const ModelExhaust& x : exhausts)
        {
            if(strlen(x.name) == len && strncmp(x.name, model->name, len) == 0)
            {
                e = &x;
            }
        }
        if(!e)
        {
            return;
        }
        p = e->at;
        d = e->dir;
    }
    const glm::vec3 w = at(f, p + d) - at(f, p);
    if(glm::dot(w, w) < 1e-8f)
    {
        return;
    }
    where = at(f, p);
    dir = glm::normalize(w);
    onGun = true;
}

// The exhaust smoking after the shot: `n` slow puffs out of it and rising, mid grey, swirling (your hand's each frame
// for a moment, following the gun; a monster's at the shot, fading in one after another).
int exhaustSmoke(const glm::vec3& where, const glm::vec3& dir, const glm::vec3& drift, int n, float strength, bool staggered)
{
    if(n <= 0 || !particles::enabled())
    {
        return 0;
    }
    const float turb = 70.f * za::clamp(vr_gunfx_turbulence.value, 0.f, 3.f);
    int made = 0;
    for(int i = 0; i < n; i++)
    {
        particles::GunParticle g;
        g.kind = particles::GunParticle::Smoke;
        g.org = where + dir * vrand::range(rng, 0.5f, 2.f) + glm::vec3{vrand::range(rng, -0.6f, 0.6f), vrand::range(rng, -0.6f, 0.6f), 0.f};
        g.vel = dir * vrand::range(rng, 10.f, 25.f) + glm::vec3{0.f, 0.f, vrand::range(rng, 10.f, 18.f)} + drift * 0.8f;
        const float grey = smokeGrey() * 1.1f;
        g.color = glm::vec4{grey, grey, grey * 0.97f, strength * vrand::range(rng, 0.35f, 0.45f)};
        g.scale = vrand::range(rng, 1.f, 1.6f);
        g.grow = vrand::range(rng, 8.f, 13.f);
        g.life = vrand::range(rng, 0.8f, 1.2f);
        g.drag = 2.f;
        g.gravity = -0.06f;
        g.turb = turb;
        g.fadeIn = staggered ? vrand::range(rng, 0.05f, 0.5f) : 0.05f;
        made += put(g);
    }
    return made;
}

// A rocket launcher's vents facing you (its model's own coordinates): v_rock2.mdl's two slots in the back face of its
// head (its shoulder at x 33.3, round the tube; the slots y -4.72..-3.36 and 3.36..4.55, z 6.57..7.51), facing back.
struct ModelVents
{
    const char* name;
    glm::vec3 at[2];
    glm::vec3 dir;
};
constexpr ModelVents rocketVents[] = {
    {"progs/v_rock2.mdl", {{33.2f, -4.05f, 7.05f}, {33.2f, 3.95f, 7.05f}}, {-1.f, 0.f, 0.f}},
};
[[nodiscard]] const ModelVents* ventsOf(const Muzzle& m, GunFrame& f)
{
    const qmodel_t* model = gunModel(m);
    if(!model || !gunFrame(m, f))
    {
        return nullptr;
    }
    const char* hash = strchr(model->name, '#');
    const size_t len = hash ? static_cast<size_t>(hash - model->name) : strlen(model->name);
    for(const ModelVents& v : rocketVents)
    {
        if(strlen(v.name) == len && strncmp(v.name, model->name, len) == 0)
        {
            return &v;
        }
    }
    return nullptr;
}

// Light smoke out of a rocket launcher's vents (`n` puffs, either vent): back out of them, at you, and up, curling.
int rocketVentSmoke(const GunFrame& f, const ModelVents& v, const glm::vec3& drift, int n, float strength)
{
    if(n <= 0 || !particles::enabled())
    {
        return 0;
    }
    const float turb = 40.f * za::clamp(vr_gunfx_turbulence.value, 0.f, 3.f);
    const glm::vec3 back = glm::normalize(at(f, v.dir) - at(f, glm::vec3{0.f}) + glm::vec3{1e-6f});
    const glm::vec3 side = glm::normalize(f.y + glm::vec3{1e-6f});
    int made = 0;
    for(int i = 0; i < n; i++)
    {
        const int k = rng.getI(0, 1);
        const float out = k == 0 ? -1.f : 1.f;
        particles::GunParticle g;
        g.kind = particles::GunParticle::Smoke;
        g.org = at(f, v.at[k] + glm::vec3{-0.1f, vrand::range(rng, -0.5f, 0.5f), vrand::range(rng, -0.3f, 0.3f)});
        g.vel = back * vrand::range(rng, 5.f, 11.f) + side * (out * vrand::range(rng, 1.f, 4.f)) +
                glm::vec3{0.f, 0.f, vrand::range(rng, 8.f, 14.f)} + drift * 0.8f;
        const float grey = za::min(0.9f, smokeGrey() * 1.15f);
        g.color = glm::vec4{grey, grey, grey * 0.98f, strength * vrand::range(rng, 0.24f, 0.34f)};
        g.scale = vrand::range(rng, 0.5f, 0.8f);
        g.grow = vrand::range(rng, 6.f, 9.f);
        g.life = vrand::range(rng, 0.6f, 1.f);
        g.drag = 2.f;
        g.gravity = -0.05f;
        g.turb = turb;
        g.fadeIn = 0.05f;
        made += put(g);
    }
    return made;
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

// The backblast: a swirling cone out of the exhaust, the exhaust smoking (a monster's: here; your hand's: handFrame), and
// dust off the floor under it, if near.
void backblast(const Muzzle& m, const glm::vec3& right, const glm::vec3& up, float budget, bool own, Counts& c)
{
    glm::vec3 rear, out;
    bool onGun = false;
    exhaustOf(m, rear, out, onGun);
    if(vr_debug_gunfx.value)
    {
        Con_Printf("gunfx exhaust at %.1f %.1f %.1f dir %.2f %.2f %.2f (%s), muzzle %.1f %.1f %.1f\n", rear.x, rear.y, rear.z, out.x,
            out.y, out.z, onGun ? "the gun's" : "behind the muzzle", m.at.x, m.at.y, m.at.z);
    }
    smokeCone(rear, out, right, up, m.drift, scaled(16, budget), 1.5f, 240.f, 0.85f, 0.6f, 0.3f, blast, c.back);
    GunFrame gf;
    if(const ModelVents* v = ventsOf(m, gf))
    {
        c.vent += rocketVentSmoke(gf, *v, m.drift, scaled(7, budget), 1.f); // (the vents facing you; your hand's go on: afterSmoke)
    }
    if(!own)
    {
        c.exhaust += exhaustSmoke(rear, out, m.drift, scaled(6, budget), 1.f, true);
    }
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
        Con_Printf("gunfx light %.2f vent %d exhaust %d\n", c.light, c.vent, c.exhaust); // (own line: long ones wrap)
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
    c.light = placeLight(m.at);
    smokeLight = c.light;
    if(on(vr_gunfx_sparks) && !adBurst)
    {
        for(int b = 0; b < look.barrels; b++)
        {
            const float side = look.barrels > 1 ? (b == 0 ? -1.4f : 1.4f) : 0.f;
            // (a few at least, however fast it fires: the super nailgun's too)
            const int n = look.sparks > 0 ? za::max(za::min(3, look.sparks), scaled(look.sparks, budget)) : 0;
            sparksAt(m.at + right * side, m, right, up, look, n, c);
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
            look.smokeAlpha, 0.2f, frontal, c.smoke);
        flashGlow(m, look, c);
        // The nail pistol's side vents: its heat out of them (your hand's go on a moment: handFrame).
        GunFrame f;
        if(wid == WID_NAILPISTOL && vr_nailpistol_smoke.value > 0.f && gunFrame(m, f))
        {
            c.vent = ventSmoke(f, m.drift, scaled(6, budget * za::min(vr_nailpistol_smoke.value, 3.f)), 1.f);
        }
    }
    if(on(vr_gunfx_port) && look.port && !ports)
    {
        // A gun that throws no casings (the grunts'): its port's puff now, out of its right side behind the muzzle.
        const float length = m.gunLength > 0.f ? m.gunLength * 0.6f : 16.f;
        smokeCone(m.at - m.fwd * length + right * 1.f, glm::normalize(right + up * 0.5f), m.fwd, up, m.drift,
            scaled(4, budget), 0.6f, 70.f, 0.45f, 0.45f, 0.35f, rising, c.port);
    }
    // (an AD rocket launcher too: its own burst has flames and sparks out of its exhaust, not this smoke)
    if(on(vr_gunfx_backblast) && look.backblast)
    {
        backblast(m, right, up, budget, shooter < 2, c);
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
    // A faint glow at the muzzle once the barrel is hot (past 0.5: about 3 s of a nailgun's fire), a dull red brighter
    // as it heats; made again each frame (it lives 2 or 3), so it follows the gun.
    if(const float hot = h.heat - 0.5f; hot > 0.f)
    {
        particles::GunParticle g;
        g.kind = particles::GunParticle::Glow;
        g.org = m.at - m.fwd * 1.f;
        g.vel = m.drift;
        g.color = glm::vec4{1.f, 0.3f, 0.07f, 0.3f * za::min(hot, 1.f)};
        g.scale = 1.6f;
        g.life = 0.03f;
        (void)particles::gunParticle(g);
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

// Your gun's vents and exhaust smoking on after its shots, following it (`dt` seconds since the last frame): the nail
// pistol's vents (fading over h.vent: a quick string leaves them smoking longer), a rocket launcher's exhaust.
void afterSmoke(HandState& h, const Muzzle& m, int wid, float dt)
{
    h.vent = za::max(0.f, h.vent - dt);
    h.exhaust = za::max(0.f, h.exhaust - dt);
    if((h.vent <= 0.f && h.exhaust <= 0.f) || !on(vr_gunfx_smoke) || !particles::enabled())
    {
        h.ventOwed = 0.f;
        return;
    }
    const float intensity = za::clamp(vr_gunfx_intensity.value, 0.f, 3.f);
    if(h.vent > 0.f && wid == WID_NAILPISTOL)
    {
        h.ventOwed += 22.f * dt * za::min(h.vent * 2.f, 1.f) * intensity * za::clamp(vr_nailpistol_smoke.value, 0.f, 3.f);
        GunFrame f;
        if(h.ventOwed >= 1.f && gunFrame(m, f))
        {
            const int n = static_cast<int>(h.ventOwed);
            h.ventOwed -= static_cast<float>(n);
            const int made = ventSmoke(f, m.drift, n, za::min(h.vent * 2.5f, 1.f));
            if(vr_debug_gunfx.value >= 2.f)
            {
                Con_Printf("gunfx after vent %d\n", made);
            }
        }
    }
    else if(h.exhaust > 0.f && on(vr_gunfx_backblast) && kindOf(wid) == Kind::Rocket)
    {
        h.ventOwed += 18.f * dt * za::min(h.exhaust * 1.5f, 1.f) * intensity;
        if(h.ventOwed >= 1.f)
        {
            const int n = static_cast<int>(h.ventOwed);
            h.ventOwed -= static_cast<float>(n);
            glm::vec3 where, dir;
            bool onGun = false;
            exhaustOf(m, where, dir, onGun);
            const int made = exhaustSmoke(where, dir, m.drift, n, za::min(h.exhaust * 1.5f, 1.f), false);
            // The vents facing you smoke on too (one puff in two, lighter).
            GunFrame f;
            const ModelVents* v = ventsOf(m, f);
            const int vents = v ? rocketVentSmoke(f, *v, m.drift, (n + 1) / 2, za::min(h.exhaust * 1.2f, 0.8f)) : 0;
            if(vr_debug_gunfx.value >= 2.f)
            {
                Con_Printf("gunfx after exhaust %d vents %d at %.1f %.1f %.1f\n", made, vents, where.x, where.y, where.z);
            }
        }
    }
}

// The guns that leave smoke wisps off their barrels after firing (vr_muzzlewisps.hpp): a shot's share of full smoke (the
// shotguns' one shot fills them, a burst of the nailguns'), the smoke's opacity at full, and their barrels.
struct WispLook
{
    float charge, alpha;
    int barrels;
};
[[nodiscard]] WispLook wispLook(int wid)
{
    switch(wid)
    {
        case WID_SHOTGUN: return {1.f, 0.32f, 1};
        case WID_SUPER_SHOTGUN: return {1.f, 0.32f, 2};
        case WID_REVOLVER: return {0.7f, 0.26f, 1};
        case WID_FLAREGUN: return {0.7f, 0.26f, 1};
        case WID_NAILPISTOL: return {0.2f, 0.22f, 1};
        case WID_NAILGUN: return {0.12f, 0.26f, 2};
        case WID_SUPER_NAILGUN: return {0.08f, 0.3f, 1}; // (its barrels turn: one wisp out of their middle)
        default: return {0.f, 0.f, 0};
    }
}

// Where the barrels' bores end on the models with two (their own coordinates; Misc/quakevr/anchors.py's frame 0: the
// middle of each bore's front ring). Other two-barrelled guns: either side of the muzzle.
struct ModelBarrels
{
    const char* name;
    glm::vec3 at[muzzlewisps::maxBarrels];
};
constexpr ModelBarrels barrelEnds[] = {
    {"progs/v_shot2.mdl", {{28.5f, -1.53f, 6.39f}, {28.5f, 1.47f, 6.39f}}},
    {"progs/v_nail.mdl", {{20.21f, -3.67f, 3.84f}, {20.21f, 3.77f, 3.84f}}},
};

// Your gun's barrels this frame, for their wisps (`fired`: its shot this frame).
void barrelWisps(int hand, const Muzzle& m, int wid, bool fired)
{
    const WispLook look = wispLook(wid);
    muzzlewisps::Gun g;
    g.fwd = m.fwd;
    g.charge = look.charge;
    g.alpha = look.alpha;
    g.barrels = look.barrels;
    g.at[0] = g.at[1] = m.at;
    if(look.barrels > 1)
    {
        GunFrame f;
        const qmodel_t* model = gunModel(m);
        const ModelBarrels* e = nullptr;
        if(model && gunFrame(m, f))
        {
            const char* hash = strchr(model->name, '#');
            const size_t len = hash ? static_cast<size_t>(hash - model->name) : strlen(model->name);
            for(const ModelBarrels& x : barrelEnds)
            {
                if(strlen(x.name) == len && strncmp(x.name, model->name, len) == 0)
                {
                    e = &x;
                }
            }
        }
        if(e)
        {
            g.at[0] = at(f, e->at[0]);
            g.at[1] = at(f, e->at[1]);
        }
        else
        {
            const glm::vec3 right = glm::normalize(glm::cross(m.fwd, m.up) + glm::vec3{1e-6f});
            g.at[0] = m.at - right * 1.4f;
            g.at[1] = m.at + right * 1.4f;
        }
    }
    muzzlewisps::handFrame(hand, g, fired);
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
        h.vent = 0.f;
        h.exhaust = 0.f;
        h.slot = slot;
    }
    const bool due = h.due;
    h.due = false;
    if(slot < 0 || !w.visible || !w.ent.model ||
        (!due && h.heat <= 0.f && h.vent <= 0.f && h.exhaust <= 0.f && !muzzlewisps::smoking(hand)))
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
    m.view = &w;
    if(due && on(vr_gunfx))
    {
        // An AD gun's own burst (vr_weaponfx.cpp adGunBurst) made its sparks and backblast already.
        const int variant = adguns::variantOf(w.ent.model->name);
        const bool adBurst = variant >= 0 && adguns::fireFeel(variant).burst != adguns::FireFeel::None;
        // The shotgun, the super shotgun and the Widowmaker throw their casings out of their ports (vr_shells.cpp).
        const bool ports = wid == WID_SHOTGUN || wid == WID_SUPER_SHOTGUN;
        shot(wid, m, hand, adBurst, ports);
        h.heat = za::min(h.heat + looks[static_cast<int>(kind)].heat, 1.5f);
        if(wid == WID_NAILPISTOL)
        {
            h.vent = za::min(h.vent + 0.45f, 1.5f); // (a string of shots: its vents smoke on longer)
        }
        if(kind == Kind::Rocket)
        {
            h.exhaust = 0.9f;
        }
    }
    if(on(vr_gunfx))
    {
        heatWisps(h, m, dt);
        afterSmoke(h, m, wid, dt);
        barrelWisps(hand, m, wid, due);
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
    smokeLight = placeLight(at);
    smokeCone(at, dir, right, up, glm::vec3{0.f}, scaled(5, budget), 0.6f, 70.f, 0.45f, 0.45f, 0.35f, rising, n);
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
    impacts::clear();
    muzzlewisps::clear();
}

void registerCommands()
{
    Cmd_AddCommand("vr_gunfx_test", test_f);
    impacts::registerCommands();
}

} // namespace qvr::gunfx
