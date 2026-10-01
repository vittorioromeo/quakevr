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
    Hotspot, // a hotspot (point, Hand Pitch/Yaw/Roll; a blade's share), posed with the other hand
    Holster  // the weapon in a kind of holster (its Holstered X/Y/Z, Pitch/Yaw/Roll): put there with either hand's grip
};

// Posing the weapon in a holster (Target::Holster): the right holster of the kind, as it is on the body, floats in front
// of the player (turned so that its outside faces them); the weapon sits in it as its settings put it, either hand's grip
// takes it and carries it, and letting go leaves it where it is. Everything is in the world; turns are bases (columns:
// forward, left, up, as anglesBasis). The view places it on its first frame (`placed`) and works out the candidate.
struct HolsterSession
{
    weapons::HolsterKind kind{weapons::HolsterKind::Hip};
    bool placed{false};
    // The holster's frame (holsterFrame: out off the body, up, side outwards) and the point it turns about (where the
    // hand reaches for it).
    glm::vec3 out{1.f, 0.f, 0.f}, up{0.f, 0.f, 1.f}, side{0.f, 1.f, 0.f};
    glm::vec3 pivot{0.f};
    // The weapon with all its Holstered settings 0 (as the holster alone places it), and the holster model.
    glm::vec3 basePos{0.f};
    glm::mat3 baseTurn{1.f};
    glm::vec3 slotPos{0.f};
    glm::mat3 slotTurn{1.f};
    bool slotDrawn{false};
    glm::vec3 tiltAxis{0.f, 1.f, 0.f};
    // The weapon as it is now (its model's origin, as placed), and the hand carrying it (-1: none) with the weapon's
    // place in that hand (taken on the first frame after the grip: `grabTaken`).
    glm::vec3 weaponPos{0.f};
    glm::mat3 weaponTurn{1.f};
    int grabHand{-1};
    bool grabTaken{false};
    glm::vec3 grabPos{0.f};
    glm::mat3 grabTurn{1.f};
    bool moved{false};     // moved since it was last set (or placed): the menu button sets it before leaving
    bool fromSettings{false}; // put the weapon back where its settings place it (after an undo), keeping the holster
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
    HolsterSession holster;       // Target::Holster
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
    weapons::HolsteredPose holstered; // Target::Holster: Holstered X/Y/Z, Pitch/Yaw/Roll
    // Target::Holster: the weapon in the holster's frame (out, side, up), from its pivot: its place, forward and up
    // (vr_pose_check compares the weapon holstered on the body with them).
    glm::vec3 inHolsterPos{0.f}, inHolsterFwd{1.f, 0.f, 0.f}, inHolsterUp{0.f, 0.f, 1.f};
    // The posing hand's rig (as drawn, before the palm's fit) and its drawn palm, in the weapon's model frame
    // (the view's hotspot frame): vr_pose_check compares the hand holding the weapon afterwards with them.
    glm::mat4 rigInWeapon{1.f};
    glm::vec3 palmInWeapon{0.f};
    glm::mat4 rigWorld{1.f}; // and in the world (vr_pose_check: the hand held with the controller where it was)
    bool palmFitted{false};  // palmInWeapon is the palm fitted by the grasp (drawn solved), not the rig's
};

void init(); // the commands

[[nodiscard]] bool active();
[[nodiscard]] Session& session();
[[nodiscard]] Candidate& candidate();

// The hand that poses (the weapon hand for the weapon, the other for a hotspot) and the one that confirms. In a holster
// either hand carries the weapon and either confirms: the one carrying it (else the weapon hand), and the other.
[[nodiscard]] int posingHand();
[[nodiscard]] int confirmHand();
// Whether the Hand Only, Hand and Weapon Together and a hotspot's Held Hand offsets are reset to 0 on confirming
// (vr_pose_reset_offsets): the view poses without them then.
[[nodiscard]] bool resetOffsets();

// Starts posing the weapon in `slot` (its model `model`) held in `weaponHand`, the weapon or hotspot `hotspot` (-1: a
// new one, the first free; Target::Holster: the weapons::HolsterKind); `returnPage`: the VR Settings page to reopen on
// leaving. False (said why) if it can't.
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

// Whether the posing hand is drawn wrapping the weapon (the grasp solved, the palm fitted): for a moment after each set
// (the grip play will give it), or always with vr_pose_solve 1. Otherwise it is drawn at its controller, unsolved, its
// fingers the controller's curls, passing through the weapon: free to be put where it should be.
[[nodiscard]] bool showSolved();
// The view, drawing the hand solved: the palm fitted as the pose last set holds it (vr_pose_check), if the hand is still
// where it was set.
void solvedPalm(const Candidate& c);

// Once a frame (VR_BeginFrame, after the texts are cleared): its text, leaving when the game does.
void frame();

} // namespace qvr::posing
