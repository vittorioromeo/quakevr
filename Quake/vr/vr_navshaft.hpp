// vr_navshaft.hpp -- lift shafts kept clear (docs/vr-port/NAVMESH.md, section 24): the space a lift sweeps below it is
// no place for a monster to stand. Quake's plats, doors and trains go back (or stop) when blocked, so a monster standing
// under a raised lift sends it back up each time it comes down (his report of 2026-11-25: e1m1's first lift, e2m4).
//
// A shaft: a mover that sweeps a vertical volume a monster can stand in, found at the load by its motion, not its
// classname: a mover with QC stops pos1/pos2 (func_plat, func_door and the mods' movers built on them) or a func_train's
// path corners, moving straight up and down, farther than its own height plus a step (so there is room under it at its
// top: a thin platform, not a column plat or a door that sinks into the floor or rises into the ceiling). Live: the space
// under the mover where it is now, down to the lowest its underside reaches, widened by vr_shaft_margin across: empty
// while the lift is down, the whole shaft while it is up.
//
// A monster keeps out of a live shaft unless it has a reason there (its enemy or its goal entity in it): SV_movestep
// refuses a step that takes it in or deeper (stepRefused); one inside without a reason walks out (the QC's
// VR_Shaft_Leave: escapeYaw); the stealth AI's points to walk to are moved out to its edge (clearPoint); the navmesh
// carves each shaft's floor into polygons of their own, costed (vr_shaft_cost) while the shaft is live, and a walker
// whose path leads in stands at its edge till the lift comes down (vr_nav.cpp).

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"

namespace qvr::navshaft
{

// A lift's shaft (Quake's space): the mover (edict number), its footprint (x, y; it moves only up and down), the lowest
// and the highest its underside reaches (its lowest and highest stops' absmin z).
struct Shaft
{
    int mover = 0;
    glm::vec2 lo{0.f}, hi{0.f};
    float zlo = 0.f, ztop = 0.f;
};

// VR_OnSpawnServerAfterLoad (nav::afterLoad, before the navmesh's gather: its shafts are carved): the map's shafts, from
// the QC's fields (a save's movers where it left them; their stops are fields).
void afterLoad();

// The map's shafts (vr_shaft 1; none with it 0).
[[nodiscard]] const za::Vector<Shaft>& shafts();

// The live part's top: the mover's underside where it is now; below `s.zlo + 2`: nothing under it (the lift down, or
// gone). -1e30 when the mover is gone.
[[nodiscard]] float liveTop(const Shaft& s);

// Whether a box (Quake's absmin, absmax) is in a shaft's live part, widened across by `margin`.
[[nodiscard]] bool boxIn(const Shaft& s, const glm::vec3& lo, const glm::vec3& hi, float margin);
// ... or (`whole`) in the whole of the way the mover goes, from its lowest underside to its highest top.
[[nodiscard]] bool boxInPart(const Shaft& s, const glm::vec3& lo, const glm::vec3& hi, float margin, bool whole);

// SV_movestep: a monster's step from `from` to `to` (origins) that takes its box into a live shaft (or deeper into one)
// where it has no reason to be. Nonzero refuses it (vr_shaft 1).
[[nodiscard]] bool stepRefused(const edict_t* ent, const glm::vec3& from, const glm::vec3& to);

// Builtin navshaft(m, what): the yaw out of the live shaft m is in, by the nearest way its box can go; -1: in none, or
// (what 0) it has a reason there. what 1: its reasons set aside and the whole of the mover's way counted (it blocked the
// lift, perhaps standing on its edge: QC VR_Shaft_Blocked).
[[nodiscard]] float escapeYaw(const edict_t* ent, bool force);
// SV_MoveToGoal (every mod's monsters walking by movetogoal): escapeYaw for a monster on the ground, else -1.
[[nodiscard]] float moveOutYaw(const edict_t* ent, bool force);
// Builtin navshaftbot(bot, goal, wish, what): FrikBot bots keep out of live shafts as monsters do (their moves are
// players', not SV_movestep): the yaw out of the live shaft the bot is in with no reason there (its enemy in it, or
// `goal`: its target1 when that is a thing, never a waypoint); -2: its move along `wish` would take it in (or deeper)
// with none: it waits at the edge (for the lift to come down); -1: nothing to do. `force`: it blocked a lift.
[[nodiscard]] float botStep(const edict_t* bot, const edict_t* goal, const glm::vec3& wish, bool force);

// Builtin navshaftclear(m, at): a place for m to walk to (its origin), moved out of any live shaft to beyond its margin
// on the nearest open side (unchanged when in none, or no side is open).
[[nodiscard]] glm::vec3 clearPoint(const edict_t* ent, const glm::vec3& at);

// The live shaft a step into would need a reason m doesn't have (nav's walker standing at a shaft's edge for its lift:
// the box at `to`); -1: none.
[[nodiscard]] int blocks(const edict_t* ent, const glm::vec3& to);

} // namespace qvr::navshaft
