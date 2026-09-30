// vr_physsound.hpp -- the physics sounds (docs/vr-port/ROUND21.md, "Physics sounds"): Box3D's props knocking into the
// level and each other (by what they are made of, how heavy they are and how hard they hit), scraping while they slide
// with friction, and a climbing hand taking a hold. Server side: sent to every client as Quake's sounds are. See
// vr_physsound.cpp.
#pragma once
#include "vr_engine.hpp"

#include <cstdint>

namespace qvr::physsound
{

// What a prop sounds like: its impacts' and scrapes' recordings (quakevr/sound/vr/phys, make_physics_sounds.py).
enum class Material : uint8_t
{
    None,  // silent here (a grenade: its own bounce, QC vr_grenade.qc)
    Wood,  // crates, explosive and health boxes, wall torches
    Metal, // weapons, ammo boxes, armour, keys, the flashlight, other pickups
    Stone, // rocks (vr_debris.cpp)
    Brick, // bricks
    Soft,  // backpacks
    Flesh, // gibs and heads
    Count
};

// The material of the prop `ent` drawn with `model` (by its model and class).
[[nodiscard]] Material materialOf(edict_t* ent, const qmodel_t* model);
[[nodiscard]] const char* materialName(Material m);

// A new map, loading (VR_OnSpawnServerBeforeLoad): the sounds precached (with Quake VR's progs), the bodies' state
// forgotten.
void precache();

// Box3D's hit events, each step (vr_box3d.cpp touches): prop `num` (of `material`, `mass` kg) hit something at `speed`
// m/s (the contact's approach) at `at` (units). Only the hardest of a frame per prop plays (frameEnd).
void hit(int num, Material material, float mass, float speed, const glm::vec3& at);

// A prop sliding this frame (vr_box3d.cpp, its contacts after the step): `slip` m/s at its contacts, `press` how hard it
// is pressed onto them (1: its weight). Called once a frame for each prop that slides; one not called stops scraping.
void slide(int num, Material material, float mass, float slip, float press, const glm::vec3& at);

// The end of Box3D's frame (VR_PhysicsFrameEnd): the frame's hits played (rate-limited per prop, the loudest first) and
// the scrapes carried on or stopped.
void frameEnd();

// Whether scrapes are wanted at all (vr_physsound, vr_physsound_scrape): Box3D skips looking at the contacts if not.
[[nodiscard]] bool scrapesWanted();

// A climbing hand of `player` took a hold at `at` (world units) on the brush entity `holdEnt` (nullptr: the world): a
// palm's slap with a tap of what the hold is made of (the texture under it: wood, metal, stone).
void grab(edict_t* player, const glm::vec3& at, edict_t* holdEnt);

} // namespace qvr::physsound
