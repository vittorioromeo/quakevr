// vr_axestick.hpp -- thrown axes stick (ROUND21.md, "Thrown axes stick"): a thrown axe whose blade goes into
// something first -- fast enough (vr_axestick_speed), the blade facing the way its edge goes (vr_axestick_angle) and
// square enough to the surface (vr_axestick_incidence) -- sticks in it with its edge in (vr_axestick_depth), at the
// angle it came in at: the level and brush entities (doors, lifts), the props (Box3D's: it rides with the prop) and the
// monsters (their models as drawn, vr_hitmodel.cpp: it rides with the triangle it went into). Anything else, a flat or
// handle-first throw, bounces off as before (Box3D). QC (QC/vr_axestick.qc) does the rest: the blow, the bleeding, the
// fall when the monster dies or the prop goes, the pull by a hand or a force grab.
//
// Which models have blades, and where, is a table here (the axe: progs/v_axe.mdl, a double-bitted head).

#pragma once

#include "vr_engine.hpp"

#include <glm/glm.hpp>

namespace qvr::axestick
{

// Box3D's step, for each awake prop before it (vr_box3d.cpp's beforeStep; `com` its centre of mass, `vel` its velocity
// in units/s, `spin` its angular velocity in rad/s, world axes, `dt` the step): a thrown axe whose blade meets something
// this step as above is stuck in it (its pose, the .vr_stick_* fields, QC's VR_AxeStick_Stuck). True if it stuck: it
// is no longer a prop (QC made it MOVETYPE_NONE).
bool beforeStep(edict_t* ent, const glm::vec3& com, const glm::vec3& vel, const glm::vec3& spin, float dt);

// Once a server frame, after everything moved (VR_ServerFrameEnd, after the monsters' drawn poses are kept): each stuck
// axe placed where what it is in has taken it (its frame times the axe's pose in it), rocked by a tug
// (.vr_stick_wiggle), its blade's edge and facing written for QC (.vr_stick_at, .vr_stick_dir).
void serverFrame();

} // namespace qvr::axestick
