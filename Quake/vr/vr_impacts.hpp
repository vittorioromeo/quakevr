// vr_impacts.hpp -- what a hit throws up off a surface, by what the surface is made of and what hit it (ROUND23.md,
// "Punchier gunfire": impacts by surface material, the lightning's ozone haze). ONE table, `material x hit kind ->
// particles, decal, sound` (vr_impacts.cpp `table`), so the material ladders written elsewhere (QC TraceAttack,
// spawn_touchblood, VR_HitBlood/VR_HitNonFlesh/VR_HitBloodSplash; design_review.md proposal 4) can move onto it later.
// Today it adds, on top of each hit's own effects (cl_tent.c: Quake's puff, the decal, the tink), only what the
// surface's material makes:
// - stone: a puff of grey dust; dirt and natural ground: browner, more of it;
// - metal: hot sparks glancing off it;
// - wood: chips of wood and a little pale dust;
// - flesh: nothing new (the gore's mist, vr_gore_mist, comes with every blood burst);
// - the thunderbolt's beam on a surface: a faint bluish ozone haze lingering where it plays.
// A world hit's material: the texture of the surface nearest the hit (debris::materialOf's name table). At most
// maxLookups surface lookups a frame (a shotgun's pellets); nothing allocated (the particles' pool). Settings:
// vr_gunfx_impacts, vr_gunfx_ozone (Weapons > Weapon Effects > Gunfire Particles). vr_debug_gunfx 1 prints each
// ("gunfx impact ..."); vr_impact_test <kind> hits what you look at.

#pragma once

#include <glm/glm.hpp>

struct qmodel_s;

namespace qvr::impacts
{

// The table's keys, the QC's too (vr_impact_shared.h's VR_MAT_*, VR_HIT_*: the same numbers).
enum class Material : int
{
    None,    // not a surface (sky, liquids, clip), or none near
    Stone,   // stone, brick, concrete, the rest; a blunt blow on the world
    Dirt,    // ground, grass, mud, sand, rock faces (debris::Material::Natural)
    Metal,   // a metal surface; an explosive box, a fuel tank, a door, a lift (QC VR_Hit_Metal)
    Wood,    // a wooden surface; a crate, its pieces (QC VR_Crate_Wooden)
    Flesh,   // a living thing, a corpse, a gib, the rest of what takes a hit
    Armour,  // flesh in mail or plate (QC VR_Hit_Armoured)
    Torch,   // a wall torch (its own embers)
    Grenade, // a grenade shots set off
    Count
};

enum class HitKind : int
{
    Bullet,    // a pellet, a bullet (TE_GUNSHOT; a thing's: TraceAttack)
    Nail,      // TE_SPIKE (a thing's: a projectile's touch, spawn_touchblood)
    SuperNail, // TE_SUPERSPIKE
    Bolt,      // the thunderbolt's beam playing on it
    Pellet,    // a projectile pellet on no flesh (QC VR_HitNonFlesh)
    Blow,      // a melee blow (QC VR_HitBlood, SpawnBlood); a blunt blow's sounds
    Blade,     // a blade's blow's sound
    Splash,    // a blow sprayed every way (QC VR_HitBloodSplash, SpawnBloodSplash)
    Saw,       // the chainsaw's tick
    Bash,      // a blunt blow's or a thrown thing's knock; a thrown thing's splinters
    Count
};

[[nodiscard]] const char* materialName(Material m);

// A hit of `kind` at `org` (just off the surface it hit: Quake's puff point): the table's effects for the material
// of the surface nearest it.
void hit(HitKind kind, const glm::vec3& org);

// A beam (CL_ParseBeam) of model `model` from entity `ent`: the thunderbolt's haze where it ends on a surface.
void beam(const qmodel_s* model, int ent, const glm::vec3& start, const glm::vec3& end);

// QC: float impact(float material, float kind, vector org, vector dir, float amount): the row's effect on a thing
// (one particle2 message; vr_debug_impacts prints it), its preset (0: none sent).
void PF_impact();
// QC: string impactsound(float material, float kind, float slot): a sample of the row's sound set in `slot`
// (VR_IMPACT_SOUND, _LAYER, _OVER), picked with one of QC's random() numbers; "" when it has none (no number drawn).
void PF_impactsound();

void clear();
void registerCommands(); // vr_impact_test

} // namespace qvr::impacts
