// vr_grip.hpp -- how a carried prop sits in the hand (Held Object Offsets' Grip, vr_props.inc; ROUND21.md, "Held props:
// grip modes, live offsets, palm grip, torch handle").
//
// A prop is placed in the hand once, as the hand takes it (QC's carrygrip, vr_carry.qc VR_Carry_Start), by its Grip
// mode, from where it was when taken:
// - Where Taken (0): as it was (moved against the curled fingers: vr_held_surface_fit);
// - Fixed (1): the same place every time: its origin at Grip X/Y/Z, turned by Grip Pitch/Yaw/Roll;
// - In the Palm (2): a small thing (a rock, a half brick, a grenade) with its middle (its centre of mass) in the middle of
//   the palm, resting on it, the face of it nearest the palm flat on it, turned about the palm's normal as it was taken;
// - Along the Handle (3): a stick (a torch) with its handle through the fist: its long axis along the fist's grip
//   channel (at most Handle Tilt degrees off it, leaning as taken), held at the point of the handle the fist was at
//   (Handle From .. To), its head towards the index finger unless taken the other way round, its roll as taken.
// Then, in every mode but Fixed, the same six sliders are an offset on top: moved (X forward, Y left, Z up) and turned
// (Pitch up, Yaw left, Roll right) in the hand's frame, about where the hand holds it (the palm; In the Palm, its
// middle; Along the Handle, the grip channel), the left hand's mirrored.
//
// The place is kept (the server's: by entity) with what it was worked out from, so a change to any of the prop's settings
// (props::settingsGeneration) places it again at once, from where it was taken: the Held Object Offsets page's sliders
// and its Grip mode move what the hand holds while it holds it. The client draws the local player's held props from the
// server's place when it has it (a listen server: vr_held.cpp), in the same frame.
//
// The hand's frame is the move's: its place and the axes of its angles (held::axesFromAngles(handrot, true): x forward,
// y left, z up). A place in it: the model's origin, and its axes (as held::axesFromAngles of its angles).

#pragma once

#include "vr_engine.hpp"

#include <vector>

namespace qvr::grip
{

enum class Mode : int
{
    WhereTaken = 0,
    Fixed = 1,
    Palm = 2,
    Handle = 3,
};

struct Place
{
    glm::vec3 pos{0.f};  // the model's origin in the hand's frame
    glm::mat3 rot{1.f};  // its axes there
};

// Where the empty hand holds things, in its frame: the middle of the palm's side and the way the palm faces (towards what
// it holds), and the grip channel (grasp::gripChannel: where a handle lies in the half-closed fingers; `channelDir` from
// the little finger's side to the index's). Measured from the jointed hand as it is drawn (vr_view.cpp, every frame);
// without it (a dedicated server, another player) the default hand's.
struct HandFrame
{
    bool measured{false};
    glm::vec3 palm{0.f};
    glm::vec3 palmNormal{0.f, 0.f, 1.f};
    glm::vec3 channelPoint{0.f};
    glm::vec3 channelDir{0.f, 0.f, 1.f};
    float channelRadius{0.f};
};

// Client side, every frame: `hand`'s (0 off, 1 main) frame, measured (or not: `f.measured` false).
void setHandFrame(int hand, const HandFrame& f);
// The local player's measured frame of `hand`, or the default hand's (mirrored for the off hand when `left`).
[[nodiscard]] HandFrame handFrame(int hand, bool left, bool local);

// The prop, as the grip sees it: its drawn box (its axes, relative to its origin), its drawn vertices (may be empty: then
// the box's corners), whether it is a brush model (its angles' pitch), and its model's settings slot.
struct Prop
{
    glm::vec3 lo{0.f}, hi{0.f};
    std::vector<glm::vec3> vertices;
    bool brush{false};
    int slot{-1};
};

// The place of `prop` in the hand for its settings now (`left`: the left hand's, mirrored), taken at `taken` (the place it
// had as the hand took it). `pivot`: where the offset turns it about (hand frame); `mode`: the settings' mode.
[[nodiscard]] Place place(const Prop& prop, const HandFrame& frame, bool left, const Place& taken, Mode* mode = nullptr,
    glm::vec3* pivot = nullptr);

// The In the Palm place alone (no offset): `taken` moved and turned the least so that it rests in `frame`'s palm, its
// middle (`centre`, its axes) on the palm's normal through the palm's middle. For anything held in the palm (the hand
// grenades).
[[nodiscard]] Place palmPlace(const Prop& prop, const glm::vec3& centre, const HandFrame& frame, const Place& taken);

// Server side (QC's carrygrip, carryplace; vr_builtins.cpp).
// As `player`'s `hand` (0 off, 1 main; `left`: the left one) at `handAngles` takes `e`: its place in the hand, taken from
// where it is now (its angles, and `offset`: its origin in the hand, forward, right, up, as .carry_offset). Kept, its
// turn set (its angles now) and its origin's place in the hand returned (forward, right, up).
[[nodiscard]] glm::vec3 serverTake(edict_t* e, edict_t* player, int hand, bool left, const float* handAngles, const glm::vec3& offset);
// Each frame it is held in one hand: its place in the hand (forward, right, up), placed again if its settings changed;
// its turn with the hand set (its angles). `offset`: .carry_offset as QC has it (kept if the grip has no place for it).
[[nodiscard]] glm::vec3 serverFrame(edict_t* e, const float* handAngles, const glm::vec3& offset);
// Held from where it is now (QC's carryangles at a regrip: the other hand let go of it, held in both): what it was taken
// at becomes where it is (its offset taken off), so that its settings still move it.
void serverKeep(edict_t* e, const float* handAngles, const glm::vec3& offset);
void forget(int num);
void resetServer();

// Client side, a listen server: the place in the hand the server has for entity `num` (placed again first if the
// settings changed). False: none (not held, or a remote server).
[[nodiscard]] bool serverPlace(int num, Place& out);

} // namespace qvr::grip
