// vr_bloodstream.hpp -- blood streaming out of bullet holes (vr_gore_streams; ROUND22.md, "More gore"): a thin,
// continuous stream of blood pouring from where a shot or a nail struck a monster or a corpse, pulsing with a heartbeat,
// weakening over a few seconds, painting the floor (a pool spreading where it lands, moving on as the body moves) and
// running down a wall it falls on.
//
// The stream is a chain of points emitted from the wound about 45 times a second, each then falling on its own (gravity,
// the body's motion at the moment it left), drawn as soft ribbon segments between neighbours (vr_lines' scene lines):
// a velocity-stretched particle chain, so it reads as one unbroken stream that arcs off the body and trails behind it
// as the body moves. The segments thin as the blood speeds up falling (a real stream's continuity), and with the flow.
// The wound is a vertex of the model (the nearest to the hit when it struck) posed as drawn each frame: it follows the
// animation, and a ragdoll's bodies (vr_ragdoll.cpp) once the monster falls. Where it lands is one short arc traced
// through the world a dozen times a second (not each point): the points live until then, and the landing is painted
// with the decals (vr_decals.cpp: the gore's pools, drops and runs, so the permanent layer bakes them too).
//
// Capped per body (vr_gore_streams_per_body) and in all (vr_gore_streams_max); a new one past a cap takes the place of
// the weakest. Fixed storage: no allocations while they run.

#pragma once

#include <glm/glm.hpp>

namespace qvr::bloodstream
{

// A wound arrived (QVR_SVC_WOUND, vr_wounds.cpp): entity `ent` struck at `at` (the world) going `dir`, by `kind`
// (QVR_WOUND_*), `amount` its damage. Shots and nails may start a stream (vr_gore_streams_chance); handled next frame.
void onWound(int ent, const glm::vec3& at, const glm::vec3& dir, int kind, int amount);

// Once a frame, after the view's entities and the ragdolls are posed (vr_view.cpp): the streams moved, drawn, painted.
void frame();

// All gone (a new map).
void clear();

// How many are running, and their points (vr_decal_count).
void count();

// vr_gore_stream_test [n]: `n` streams (1) on the monster or corpse nearest the middle of the view, from its chest
// facing you, as if shot there.
void test_f();

} // namespace qvr::bloodstream
