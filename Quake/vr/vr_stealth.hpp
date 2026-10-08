#pragma once
// vr_stealth.hpp -- the monsters' senses' engine side (QC vr_stealth.qc; docs/vr-port/STEALTH_PLAN.md): the light at a
// point and on each player, each player's flashlight beam, as QC builtins; the client's own measurement for its move.

#include <glm/vec3.hpp>

namespace qvr::stealth
{

// The client: the light on its own player at `point` (his origin), as the stealth AI reckons it (stealthlight's sum: his
// map's lightmaps and the dynamic lights he sees, his own flashlight's left out), measured ten times a second; -1 with no
// world (or playing a demo). Sent in his VR move (vr_move.hpp) for the server's clientlight.
[[nodiscard]] float lightAt(const glm::vec3& point);

// The same measured now (not the tenth of a second's), and the dynamic lights' share of it added to `dynamic`:
// vr_gear_lights_info.
[[nodiscard]] float lightFresh(const glm::vec3& point, float* dynamic);

// float(vector at) stealthlight: the light at `at` as Quake's lightmaps give it (R_LightPoint: 128 is full light) plus
// the dynamic lights there (Quake's falloff), the host's flashlight's own lights left out; -1 with no client world.
void PF_stealthlight();

// float(entity player) clientlight: the light on that player as his own client measured it (lightAt, sent in his VR
// move); -1 unknown (a client that sent none: a flat-screen build's, a bot).
void PF_clientlight();

// vector(entity player, float what) flashlightbeam: the player's flashlight beam as his client lit it (sent in his VR
// move: the host's and each coop client's own): what 0 its lens, 1 its axis, 2 '<on> <range> <cosine of its cone's half
// angle>' ('0 0 0' while off).
void PF_flashlightbeam();

// float(vector a, vector b) pvsvisible: whether b is in a's potentially visible set (the world's leaves): a noise's way
// round a corner or through a doorway, as against through solid walls.
void PF_pvsvisible();

// float(vector a, vector b, float nomonsters, entity ignore) traceseethrough: traceline (its trace_* globals), but on
// through what can be seen through: a fence or grate (an alpha-tested '{' texture) and an entity drawn see-through (its
// alpha under 1: a glass func_wall); at most 8 of them in a line. Returns how many it went through. Sight, light and
// sound pass them (the stealth AI's); shots and bodies don't (traceline's).
void PF_traceseethrough();

// void(float what) stealthprofile: 1 begins, 0 ends the QC stealth AI's work as the profiler's "stealth" scope (nested
// calls: the outermost pair); -1 at a server frame's start forgets an unbalanced one.
void PF_stealthprofile();

} // namespace qvr::stealth
