// vr_posing.hpp -- the weapon posing mode: a weapon floats still in front of the player, the hand is posed on it and the
// other hand's button makes that pose the weapon's settings (its place in the hand, or a hotspot's). Client-side only:
// while it runs, the game sees the hands held still and no buttons (vr_client.cpp), and the view draws the floating
// weapon and the posing hand (vr_view.cpp).

#pragma once

#include "vr_engine.hpp"
#include "vr_weapons.hpp"

namespace qvr::posing
{

enum class Target : int
{
    Weapon,  // the weapon's place in the hand (Offset X/Y/Z, Pitch/Yaw/Roll), posed with the weapon hand
    Hotspot  // a hotspot (point, Hand Pitch/Yaw/Roll; a blade's share), posed with the other hand
};

// What is being posed, and where the weapon floats.
struct Session
{
    bool active{false};
    int slot{-1};                 // the posed weapon's own slot (its model's); the settings go where it takes them from
    qmodel_t* model{nullptr};
    int weaponHand{1};            // the hand holding the weapon (HAND_MAIN 1, HAND_OFF 0): it poses the weapon
    Target target{Target::Weapon};
    int hotspot{0};               // 0..3
    weapons::HotspotType type{weapons::HotspotType::Grip}; // the posed hotspot's type
    int returnPage{-1};           // the VR Settings page reopened on leaving (-1: none)

    // The floating weapon: its model's origin (the model frame's, weapons::ModelTransform's offset included) and turn
    // (the hands' angle basis: forward, left, up). The view places it on its first frame (`placed`), about 40 cm ahead
    // of the head at chest height, level, facing where the head faces, its grip there; the sticks turn it about `pivot`.
    bool placed{false};
    glm::vec3 modelOrigin{0.f};
    glm::mat3 turn{1.f};
    glm::vec3 pivot{0.f};
    glm::vec3 tiltAxis{0.f, 1.f, 0.f};
    glm::vec3 startOrigin{0.f};
    glm::mat3 startTurn{1.f};
};

// What confirming now would write, worked out by the view each frame from the posing hand as drawn (what you see is
// what you confirm).
struct Candidate
{
    bool valid{false};
    glm::vec3 offset{0.f};   // Target::Weapon: Offset X/Y/Z,
    glm::vec3 angles{0.f};   // and Pitch/Yaw/Roll
    weapons::Hotspot spot;   // Target::Hotspot: the hotspot (type, point or share, Hand Pitch/Yaw/Roll; the rest as it is)
    // The posing hand's rig (as drawn, before the palm's fit) and its drawn palm, in the weapon's model frame
    // (the view's hotspot frame): vr_pose_check compares the hand holding the weapon afterwards with them.
    glm::mat4 rigInWeapon{1.f};
    glm::vec3 palmInWeapon{0.f};
    glm::mat4 rigWorld{1.f}; // and in the world (vr_pose_check: the hand held with the controller where it was)
};

void init(); // the commands

[[nodiscard]] bool active();
[[nodiscard]] Session& session();
[[nodiscard]] Candidate& candidate();

// The hand that poses (the weapon hand for the weapon, the other for a hotspot) and the one that confirms.
[[nodiscard]] int posingHand();
[[nodiscard]] int confirmHand();
// Whether the Hand Only, Hand and Weapon Together and a hotspot's Held Hand offsets are reset to 0 on confirming
// (vr_pose_reset_offsets): the view poses without them then.
[[nodiscard]] bool resetOffsets();

// Starts posing the weapon in `slot` (its model `model`) held in `weaponHand`, the weapon or hotspot `hotspot` (-1: a
// new one, the first free); `returnPage`: the VR Settings page to reopen on leaving. False (said why) if it can't.
bool start(int slot, qmodel_t* model, int weaponHand, Target target, int hotspot, int returnPage);
void stop(bool reopenMenu);

// The controllers while posing (vr_input.cpp): a press or release of a hand's button (true: taken, not a key), and the
// sticks each frame.
enum class Button : int
{
    Trigger,
    Grip,
    Primary,   // A / X: confirm
    Secondary, // B / Y: undo
    StickClick,
    Menu
};
bool button(int hand, Button b, bool down);
void sticks(const glm::vec2& off, const glm::vec2& main);

// Once a frame (VR_BeginFrame, after the texts are cleared): its text, leaving when the game does.
void frame();

} // namespace qvr::posing
