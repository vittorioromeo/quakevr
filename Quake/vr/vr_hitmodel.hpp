// vr_hitmodel.hpp -- precise hit detection (round 21, "Precise hit detection (models, not boxes)"): shots,
// projectiles, the grappling hook, melee blows and thrown things meet a monster's model -- its triangles as drawn
// -- rather than Quake's box round it (a grunt's is 32 units wide, its body half that; the corners and the space
// between the legs were hits). The box stays the broad phase: a move that meets it (grown by the tolerance) is
// then tested against the model; one that misses the model goes on through the box to what is behind.
//
// Whose model: monsters and their corpses (FL_MONSTER; the training dummy is one), drawn with a Quake .mdl. Players,
// doors, props, gibs and heads keep their boxes (a player's body is not the collision: the VR body is drawn from the
// tracked headset and hands). A replacement model without Quake's vertex animation (MD5, IQM) keeps the box.
//
// The model as drawn (the server's copy of the client's lerp): the pose of its frame (a framegroup's by time), and
// the 0.1 s blend from the previous pose the client draws when the frame changes (r_lerpmodels); a walking monster's
// origin and angles likewise (r_lerpmove, MOVETYPE_STEP): what you see is what you hit. Placed with its origin,
// angles, scale and the networked model_scale / model_scale_origin / model_offset (vr_render.cpp's transform).
//
// The test: the moving thing is a segment (a traceline's, a missile's move this frame, a melee point's sweep) swept
// by a sphere of radius r: the class's tolerance (vr_hit_tolerance_*) plus the moving thing's own size (the largest
// sphere in its box: 0 for shots and missiles). The model is grown by r along its vertex normals (the .mdl's own,
// lerped with the pose) and the segment is tested against the grown triangles (Moller-Trumbore, both sides). A
// segment that starts inside the grown model (its first crossing is a way out; or none, and a ray onwards leaves it)
// hits at its start. The hit's point on the model itself is the same place on the ungrown triangle (barycentric).
//
// Each model's triangles are sorted once into a bounding volume hierarchy (median split of their centroids averaged
// over all its poses, 4 a leaf); each pose then has its nodes' bounds (bytes, as the vertices are stored: 6 bytes a
// node). A blend between two poses tests the union of both poses' bounds. Built for every precached alias model at
// map load (no first-hit hitch), and for a model precached later at its first use.

#pragma once

#include <glm/glm.hpp>

struct edict_s;

namespace qvr::hitmodel
{

// What moves, for its tolerance (vr_hit_tolerance_guns, _grapple, _melee, _thrown). The engine's move type carries it
// (MOVE_HITMODEL and the class in the two bits above it, world.h); QC passes it to hitmodel_segment.
enum class Class : int
{
    Guns = 0,
    Grapple = 1,
    Melee = 2,
    Thrown = 3,
};

struct Hit
{
    float t{1.f};              // along the segment, 0..1 (0: it started inside)
    bool startInside{false};
    glm::vec3 point{0.f};      // where the segment meets the grown model
    glm::vec3 surface{0.f};    // the same place on the model itself
    glm::vec3 normal{0.f, 0.f, 1.f}; // the grown surface's, facing the segment's start
};

// vr_hit_precise.
[[nodiscard]] bool enabled();

// With the option on: whether `ent`'s model is what is hit (a monster or a corpse with a Quake .mdl).
[[nodiscard]] bool target(edict_s* ent);

[[nodiscard]] float tolerance(Class c);

// The segment a..b, swept by a sphere of `radius`, against `ent`'s model as drawn now: the first hit before `maxT`.
// Recorded as the last hit (restPoint, the debug view).
bool segment(edict_s* ent, const glm::vec3& a, const glm::vec3& b, float radius, float maxT, Class c, Hit& out);

// The engine's clip (SV_ClipToLinks): the box (at the entity's origin and where it is drawn, grown by the mover's box
// and the tolerance) as the broad phase, then the model. False: missed (the move goes on through the box).
bool clip(edict_s* ent, const glm::vec3& a, const glm::vec3& b, const glm::vec3& mins, const glm::vec3& maxs, float tolerance,
    Class c, float maxT, Hit& out);

// Where on the model standing (its "stand" frame, or frame 0) the point `p` on its drawn surface is, placed at its
// origin and turned with its yaw: the frame positional damage's head sphere and regions are measured in. The last
// hit's triangle when `p` is its point; else the drawn triangle nearest `p`. False: `ent` is no target.
bool restPoint(edict_s* ent, const glm::vec3& p, glm::vec3& out);

void serverFrame();  // the lerp's state (VR_ServerFrameEnd)
void afterLoad();    // every precached alias model's hierarchy built
void reset();        // the world is going (the hunk: models, edicts)
void debugDraw();    // vr_debug_hits (the view, each frame)

void stats_f();      // vr_hitmodel_stats
void bench_f();      // vr_hitmodel_bench [rays]
void check_f();      // vr_hitmodel_check [reset|print]

} // namespace qvr::hitmodel
