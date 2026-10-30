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

enum class Material : int
{
    None,  // not a surface (sky, liquids, clip), or none near
    Stone, // stone, brick, concrete, the rest
    Dirt,  // ground, grass, mud, sand, rock faces (debris::Material::Natural)
    Metal,
    Wood,
    Flesh, // (QC's living things: their blood's own path)
    Count
};

enum class HitKind : int
{
    Bullet,    // a pellet, a bullet (TE_GUNSHOT)
    Nail,      // TE_SPIKE
    SuperNail, // TE_SUPERSPIKE
    Bolt,      // the thunderbolt's beam playing on it
    Count
};

[[nodiscard]] const char* materialName(Material m);

// A hit of `kind` at `org` (just off the surface it hit: Quake's puff point): the table's effects for the material
// of the surface nearest it.
void hit(HitKind kind, const glm::vec3& org);

// A beam (CL_ParseBeam) of model `model` from entity `ent`: the thunderbolt's haze where it ends on a surface.
void beam(const qmodel_s* model, int ent, const glm::vec3& start, const glm::vec3& end);

void clear();
void registerCommands(); // vr_impact_test

} // namespace qvr::impacts
