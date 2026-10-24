// vr_posestudio.hpp -- the Pose Studio (VR Settings > Armed Grunts > Grip Editor > Drag Mode; the Debug menu;
// vr_posestudio; ROUND22.md, "Pose Studio"): an armed humanoid's grips set by hand, grab and place, as the blade shape
// editor shapes a blade (vr_bladeedit.hpp). The armed monsters hold still (vr_agrunt_freeze); walking up to one (the
// nearest within vr_posestudio_reach) shows wire boxes on its weapon(s) and hands. Either hand's grip takes the one it
// is at (yellow when near, green held) and moves and turns it with the controller:
// - a weapon: the weapon turned and moved in its hand (the hand held still), its grip offset stored on letting go (the
//   weapon back on the pose's line then, the hand round it as set);
// - a hand on a weapon: the hand moved and turned on the weapon (the weapon held still: the same offset from the hand's
//   side); the off hand on a two-handed weapon's second grip: that grip's offset.
// Edits land in the kind's global grip (Every Weapon) or the weapon's own (This Weapon), vr_agrunt_grip_scope; kept in
// vr_agrunt_grips as the Grip Editor's sliders keep them. A panel beside you (touch a button, pull the trigger): Scope,
// Undo, Reset Grips, Frozen On/Off, Cancel (everything back as it was) and Done (the changed grips printed as code and
// written to quakevr/proposed_defaults/). B/Y undoes, the menu button is Done. The sticks still walk and turn.

#pragma once

#include "vr_agarms.hpp"
#include "vr_engine.hpp"
#include "vr_posing.hpp"

#include <glm/glm.hpp>

namespace qvr::posestudio
{

// An armed monster's drawn space (hitreact's toWorld): model units -> the world.
struct Space
{
    glm::vec3 origin{0.f};
    float yawDeg{0.f};
    float scale{1.f};
};

// What relinkSkinned tells of each armed monster it solved this frame.
struct Note
{
    int ent{0};
    int kind{0};            // aggrip's (0 the grunt, 1 the enforcer)
    int slot{-1}, slotL{-1}; // its weapons' Weapon Offsets slots (right, left; -1 none)
    int prop{0}, propL{0};  // their client entities (0 none)
    const char* pose{""};   // the pose it is in, by name
    Space space;
    agarms::Targets t;      // the targets solved onto
    agarms::Placed placed;  // the arms as solved, the weapons as placed
    bool bodyFlag{false};   // the message turned its body (a strike, a take's bend, the pose keys): the next three
    glm::vec3 torso{0.f};   // its torso's turn as sent (yaw, pitch, roll; degrees)
    glm::vec3 elbowPole[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // its elbows' poles as sent (its chest's space)
    glm::vec3 waist{0.f};   // the point the torso turns about (model space)
};

void init(); // the commands

[[nodiscard]] bool active();

// Enters it; `returnPage`: the VR Settings page reopened on leaving (-1: none). False (said why) if it can't.
bool start(int returnPage);
// Leaves it: `keep` (Done) keeps every change, else (Cancel) puts the grips back as they were on entering.
void finish(bool keep);

// The controllers (vr_input.cpp): a press or release (true: taken, not a key).
bool button(int hand, posing::Button b, bool down);

// Once a frame (VR_BeginFrame, after the texts are cleared): a held part follows its hand; the boxes and panel queued.
void frame();

// relinkSkinned (vr_hitreact.cpp): before the solve, `ent`'s targets (a weapon held by the studio's drag drawn at the
// controller); after it, what was solved.
void adjust(int ent, agarms::Targets& t);
void note(const Note& n);

// Part `index` (0 the right weapon, 1 the left weapon, 2 the right hand, 3 the left hand; 99 the controller's place when
// the grab began) plus `offset` (world units), for controller `hand`, and the panel's button `index` (0 Scope, 1 Undo, 2 Reset Grips, 3 Frozen,
// 4 Cancel, 5 Done): where they are, for the mock hands (vr_mock_hand_to <hand> posepart|posebutton <index>). False: not
// active, not shown.
[[nodiscard]] bool partPoint(int hand, int index, const glm::vec3& offset, glm::vec3& out);
// (parts 4 the right elbow, 5 the left, 6 the chest; panel buttons 6 < Pose, 7 Play, 8 Pose >, 9 < Key, 10 Reset Pose,
// 11 Key >, 12 Weapon: Pose/Grip, 13 Copy to the other kind)

// QC (weapondrawnpose(e, kind * 100 + class, 19), every armed humanoid each server frame): its kind and weapon class
// noted; the pose the studio holds it in: x the pose (agpose::Pose; -1 none: not the subject, not running), y the
// phase, z Play's count (a change: play the strike once from its start).
[[nodiscard]] glm::vec3 qcHold(int ent, int kind, int cls);
[[nodiscard]] bool buttonPoint(int index, glm::vec3& out);

} // namespace qvr::posestudio
