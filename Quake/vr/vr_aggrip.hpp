// vr_aggrip.hpp -- armed monsters' grip offsets (ROUND22.md, "Armed humanoids' arms, hands and grips"): where each
// weapon sits in an armed monster's hand, per monster kind (vr_hitreact.cpp's armed rigs: 0 the grunt, 1 the enforcer),
// weapon (its Weapon Offsets slot) and hand, tuned live in VR Settings > Armed Grunts > Grip Editor (vr_agrunt_grip_*),
// kept in vr_agrunt_grips (the changes from the defaults), printed as code by vr_agrunt_grip_dump.

#pragma once

#include "vr_engine.hpp"

#include <glm/glm.hpp>

namespace qvr::aggrip
{

inline constexpr int numKinds = 2;
enum Hand : int
{
    Main = 0,   // the weapon in its main (right) hand, relative to that hand
    Off = 1,    // the weapon in its off hand (dual wielding), relative to that hand
    Second = 2, // the off hand on a two-handed weapon's second grip, relative to that grip (the weapon's turn)
    NumHands = 3,
};

struct Offset
{
    glm::vec3 pos{0.f};    // units: x forward, y left, z up (the hand's grip frame's: the rest pose's gun's)
    glm::vec3 angles{0.f}; // degrees: pitch (down), yaw (left), roll
};

// A weapon's own offset (its Weapon Offsets slot; vr_agarms: on top of its kind's global one).
[[nodiscard]] Offset offset(int kind, int slot, int hand);

// vr_agarms: a kind's hand's grip frame for every weapon (the Grip Editor's All Weapons entry; its slot allWeapons).
inline constexpr int allWeapons = -2;
[[nodiscard]] Offset global(int kind, int hand);

// The editor's subject this frame: the armed monster nearest the view (relinkArms tells each one; `dist` its distance).
void noteSubject(int kind, int slot, int slotL, float dist);
// Once a frame (after the relink): the editor's sliders to its entry and back.
void frame();
// The subject's line for the menu (the Grip Editor's: its kind, its weapon, the hand).
[[nodiscard]] const char* subjectLine();

void registerCommands();

} // namespace qvr::aggrip
