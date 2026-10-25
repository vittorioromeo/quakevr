// vr_particles.hpp -- Quake VR's own particles (the old engine's r_part.cpp, ported): textured,
// coloured, spinning and fading sprites for the QC's particle2 presets (QVR_PARTICLE_PRESET_*:
// bullet puffs, blood, explosions, lightning, smoke, sparks, gun smoke, teleports, pickup and
// force grab sparkles, lava spikes, liquid splashes). They also stand in for Quake's own effects (r_part.c): wall
// hits, explosions, the trails of rockets, lava balls, grenades, gibs and the scrag's, hell
// knight's and vore's projectiles (fire and smoke; glowing ribbons with sparkles in the colours of
// their lights), the tarbaby's explosion, lava splashes and teleports. The textures are
// quakevr/textures/particle_*.tga (and a generated disc and glow), premultiplied and mipmapped;
// glows are added to the scene, the rest alpha blended, in one draw through vr_gfx in the scene's
// translucent pass (VR_DrawSceneTranslucent). With vr_soft_particles they fade out close in front of the opaque scene
// (its distances: vr_water.hpp), as do the sprites (explosions, bubbles), left to that pass by r_sprite.c.
//
// vr_particles 0 falls back to Quake's own particle effects.

#pragma once

#include <glm/glm.hpp>

namespace qvr::particles
{

// Near the eye (vr_particle_near_cull, vr_particle_near_fade; units): a particle nearer than `cull` is not drawn, one
// nearer than `fade` fades. The CPU culls and the vertex shader fades by the same two.
struct NearLimits
{
    float cull, fade;
};
[[nodiscard]] NearLimits nearLimits();

// The QC's particle2 presets (QC/vr_defs.qc QVR_PARTICLE_PRESET_*), and the engine's own.
enum class Preset : int
{
    BulletPuff,
    Blood,
    Explosion,
    Lightning,
    Smoke,
    Sparks,
    GunSmoke,
    Teleport,
    GunPickup,
    GunForceGrab,
    LavaSpike,
    BigSmoke,
    ForceGrabTrail, // vr_fgfx.cpp: behind an object flying to the hand
    BloodTrail,     // vr_decals.cpp: behind a flying gib (`dir`: the way it goes)
    // Something hitting water, slime or lava (sent by the server's water splashes, vr_physics.cpp, and
    // QC's watersplash builtin): `org` on the surface, `dir` the way it went in, `count` how hard
    // (4 a bullet, 6-15 a hand or a thrown thing, 20-50 a body). Drops streaked along their motion
    // thrown up in a crown and a jet, fine spray, clouds of spray and mist, foam and rings lying on
    // the surface (on its waves), in the liquid's colour (lava glows and throws embers);
    // vr_water_splash scales how many (0 off), vr_water_splash_size how big, _ring_speed and
    // _ring_size the rings. Also the ripples on the liquid (water::addRipple), with or without
    // Quake VR's particles (as an explosion under a surface does).
    Splash,
    // A crate or one of its pieces breaking (QC vr_crates.qc): a cloud of wood dust drifting down and splinters thrown
    // out and falling; `dir` pushes them (the blow), `count` how much (8 a piece bursting, 30 to 50 a crate).
    WoodDust,
    // A running chainsaw's exhaust (QC vr_chainsaw.qc, lying running; vr_chainsaw.cpp in a hand): `count` faint puffs,
    // vr_chainsaw_smoke_alpha opaque, drifting out along `dir` and up.
    ChainsawSmoke,
    // A thunderbolt cell making or breaking contact as it is seated or taken out (QC vr_reload.qc): `count` small
    // blue-white sparks, bright and quick, thrown out and falling.
    ContactSparks,
    // A spent thunderbolt cell smoking (QC vr_reload.qc, vr_reload_battery_smoke_time): `count` thin grey wisps rising
    // off its top, each vr_reload_battery_smoke_alpha opaque.
    BatterySmoke,
    // AD support: Arcane Dimensions' flamethrowers (QC vr_ad_enemyguns.qc: the pyro's and the fumigator's streams, the
    // monsters' and the player's): `count` tongues of the torches' fire (CellFire) at random angles, carried along with
    // one of AD's flame sprites (`dir`: its velocity / 100); AdPoison's green. Also their muzzles' (`dir` slow).
    AdFlame,
    AdPoison,
    // AD support: a body poisoned (vr_ad_enemyguns.qc's poison): `count` puffs of green smoke over it, `dir` its half
    // size / 16 (the box they rise from).
    PoisonPuff,
    // AD support: an electric gun's muzzle and its bolt's hit (AD's plasma rifles: vr_ad_enemyguns.qc): `count` blue-white
    // sparks thrown along `dir` (a direction) and short crackling arcs.
    ElectricSparks,
    // AD support: a flamethrower's flame spent on a wall, the floor or other geometry (QC vr_ad_enemyguns.qc
    // VR_ADGun_FlameTouch; `dir` the way it flew): a scorch there every time (vr_adgun_fx_scorch, vr_decals.cpp
    // fromEffect) and `count` tongues of fire licking over it.
    AdFlameHit,
    // Fire put out by water (QC: a flamethrower's flame meeting the surface, a lava nail going in): `count` puffs of
    // white steam rising off `org`, quick and thin.
    Steam,
    // AD support: a fuel tank's burst field (QC vr_ad_enemyguns.qc VR_TankField_Look), `org` a spot on the ground in it,
    // `dir` (how far round it they spread, in 8 units; their size; their alpha: the field fading). TankFire: `count` tongues of
    // fire rising straight up off it, and embers; TankSmoke: `count` puffs of dark smoke rising off the flames' tops;
    // TankCloud: `count` big puffs of the poison cloud, a green smoke slowly billowing and churning low over the ground.
    TankFire,
    TankSmoke,
    TankCloud
};

// Spawns a preset's particles (count scaled by vr_particle_mult); false if they are off, for the
// caller to fall back on Quake's effects.
bool spawn(const glm::vec3& org, const glm::vec3& dir, Preset preset, int count);

// Whether they are on and drawn (the VR protocol, vr_particles): false for Quake's effects.
[[nodiscard]] bool enabled();

// Once, at startup: the particles' pool reserved to its cap (32768 of 128 bytes, 4 MB), so that it never reallocates
// (and copies every live particle) in the middle of a fight.
void init();

// A map's load (VR_NewMap): the particles' atlas made (its textures read, 20-30 ms), not at the first particle.
void prepare();

// Removes them all (a new map, a disconnect).
void clear();

// The gore's drops (vr_gore.cpp): one falling straight down from `org` for `fall` seconds (Quake's
// gravity) until it reaches `floorZ`, `size` about as big as a particle's scale, in `color` (lit
// already); and `count` tiny specks thrown up off `normal` where one lands.
void bloodDrip(const glm::vec3& org, float fall, float floorZ, float size, const glm::vec3& color);
void bloodSpecks(const glm::vec3& org, const glm::vec3& normal, int count, const glm::vec3& color);
// A drop of blood flying (the blood streams' spurts and splashes, vr_bloodstream.cpp): `size` units, fading and shrinking
// over its `life` seconds as it falls, in `color` (lit already); gone below `floorZ`. From the pool (no allocation).
// `wet`: let in a liquid (the caller's one test, water::bloodInLiquid): slowed to a slowly sinking cloud, its own life.
void bloodDroplet(const glm::vec3& org, const glm::vec3& vel, float size, float life, const glm::vec3& color, float floorZ,
    bool wet = false);
// vr_water_blood_spray [distance] [count]: a hit's blood (Preset::Blood, count 30) that far ahead (64), across the view:
// under water, its slowed clouds (vr_water_blood).
void bloodSprayTest_f();

// Exact burst of additional fire particles for a flame-model source.
int largeExplosionCount(); // diagnostic: live large textured blast fireballs
int fireSource(const glm::vec3& at, float sourceScale);
// The air a fast hand moves (vr_flamewobble.cpp): the fire and smoke particles within `radius` units of `at` carried
// towards going `vel` (units/s), the nearer the more, over `dt` seconds.
void gust(const glm::vec3& at, float radius, const glm::vec3& vel, float dt);

// Fiery/smoky trail of an incandescent physical explosion chunk; size in world units, heat 0..1.
void explosionDebrisTrail(const glm::vec3& from, const glm::vec3& to, float size, float heat);

// Spent casings (vr_shells.cpp): a faint puff of smoke and a few tiny sparks where one is thrown
// out going `dir` (`smoke` how much: a hot breech smokes more), and the thin wisp one trails from
// `from` to `to` as it flies (`strength` 0..1, fading as it cools). Nothing with vr_particles 0.
void shellEject(const glm::vec3& org, const glm::vec3& dir, float smoke, int sparks);
void shellTrail(const glm::vec3& from, const glm::vec3& to, float strength);

// A casing dropping into a liquid at `org` on its surface, going `dir` (`strength` 0..1: how fast): the splash
// preset's drops, ring and foam at a casing's size (the ripple is water::addRipple's, the caller's). Nothing with
// vr_particles 0 or vr_water_splash 0.
void shellSplash(const glm::vec3& org, const glm::vec3& dir, float strength);

// A lava nail's streak (vr_emissive.cpp) from `from` to `to`: a hot, short-lived glowing core and a
// few embers falling off it. Nothing with vr_particles 0.
// A running chainsaw's exhaust smoke (Preset::ChainsawSmoke): `count` puffs at `org` going out along `dir` (a unit
// vector, or zero), each vr_chainsaw_smoke_alpha opaque. Nothing with vr_particles 0.
void chainsawSmoke(const glm::vec3& org, const glm::vec3& dir, int count);

// A thunderbolt cell's contact sparks (Preset::ContactSparks) and a spent one's smoke (Preset::BatterySmoke): `count` of
// them at `org`. Nothing with vr_particles 0.
void contactSparks(const glm::vec3& org, int count);
void batterySmoke(const glm::vec3& org, int count);

void lavaNailTrail(const glm::vec3& from, const glm::vec3& to);

// A torch's fire (vr_walltorch.cpp): `count` puffs of dark smoke leaving its flame's top at `org`, drifting along
// `drift` (units/s) and rising, `alpha` opaque; and one burning drip falling off a torch held upside down at `org`,
// going `vel`. Nothing with vr_particles 0.
void torchSmoke(const glm::vec3& org, const glm::vec3& drift, int count, float alpha);
void torchDrip(const glm::vec3& org, const glm::vec3& vel);
// A body smouldering (vr_smoulder.cpp: struck by lightning, burning or burnt): `count` thin grey wisps off its skin at
// `org`, lifting off along `out` (the way out of the body there, a unit vector) and rising, `alpha` opaque. Nothing with
// vr_particles 0.
void smoulderSmoke(const glm::vec3& org, const glm::vec3& out, int count, float alpha);

// The counter glow (vr_meleehud.cpp): a golden ember leaving a held weapon's surface at `org`, going `vel`, `bright`
// 0..1 (its first alpha). Nothing with vr_particles 0.
void counterEmber(const glm::vec3& org, const glm::vec3& vel, float bright);

// Incandescent blades (vr_hotblade.cpp): an ember leaving the heated part of a blade at `org`, going `vel`, in `color`
// (the glow's hue) `bright` 0..1; and one of the particles a swung blade leaves behind (`spark`: a spark, else a small
// tongue of flame) at `org`, drifting `vel`, lingering `life` seconds. Nothing with vr_particles 0.
void hotEmber(const glm::vec3& org, const glm::vec3& vel, const glm::vec3& color, float bright);
void hotTrail(const glm::vec3& org, const glm::vec3& vel, const glm::vec3& color, float bright, float life, bool spark);

// The gore's blood mist (vr_gore_mist*): the large, faint clouds every blood effect makes, at `org` drifting along `dir`,
// as for a blood effect `count` strong. Nothing with vr_particles 0 or vr_gore_mist 0.
void bloodMist(const glm::vec3& org, const glm::vec3& dir, int count);
// vr_gore_mist_test [distance]: a bleed's mist that far ahead (64), drifting right.
void mistTest_f();
// vr_particle_light_report: the last frame's lit particles (vr_particle_light), their mean light and colour, its cost.
void lightReport_f();
// vr_particle_waste_report: the shaded fragments that showed (next to) nothing since the last (vr_debug_particle_waste).
void wasteReport_f();
// vr_water_foam_info: the shore's foam particles (vr_water_foam_particles and lava's, slime's): the world's shore
// segments, those near, the last frame's spawned, how many the rates ask to be alive (the cap: 600), the cost.
void shoreFoamInfo_f();
// Live particles (vr_memstats).
[[nodiscard]] int liveCount();
[[nodiscard]] int capacity(); // the pool's size (vr_limits)

} // namespace qvr::particles
