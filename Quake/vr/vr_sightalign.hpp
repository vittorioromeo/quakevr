// vr_sightalign.hpp -- Align Sights to My Aim (Weapon Offsets page): the held weapon turned (with the hand, about the
// fist) so that its sights line up in front of the dominant eye the way the player raises a gun with the eyes closed
// ("natural point of aim"), and its shots turned onto the sight line.
//
// Each weapon has a sight line: a rear and a front point in its model space (as its frames' vertices). The shotgun, the
// double shotgun, the lightning gun and the plasma gun have painted sights (their skins' glowing fire colours,
// vr_sights.cpp): the points are found on them (the rear notch's or ring's middle, the front post's top). The other guns
// have a line along the top of the barrel (a table), from above the grip to the muzzle; melee weapons have none.
//
// The capture: after a countdown, each time the weapon hand has been raised and held still (0.4 s), the dominant eye's
// place in the controller's frame is taken (vr_sight_align_captures times, the hand lowered between them); outliers are
// dropped and the rest averaged. The correction is the least turn of the hand and weapon together, about the middle of
// the fist, taking the sight line onto the ray from the eye through the front sight, then the least move putting it
// through the eye: stored as the weapon's Hand and Weapon Together offset (vr_wofs_whole_*). Its Shot Pitch and Shot Yaw
// are set so that its shots go along the sight line, and (a gun with a foregrip hotspot) its two-handed Aim Offset so that
// taking the foregrip keeps the gun as held in one hand. Apply, Cancel, then Undo (the exact values back).

#pragma once

#include "vr_engine.hpp"
#include "vr_hands.hpp"

#include <glm/glm.hpp>

namespace qvr::sightalign
{

// A weapon model's sight line in its model space (as its frames' vertices); valid false: none (melee weapons, the hand,
// models not known). `painted`: found on the skin's sights.
struct SightLine
{
    bool valid{false};
    bool painted{false};
    glm::vec3 rear{0.f};
    glm::vec3 front{0.f};
};
[[nodiscard]] SightLine sightLine(const qmodel_t* model);

void init(); // the commands
// Forgets the models' sight lines (a game directory change).
void resetCaches();

// Once a frame (vr_main.cpp, after the texts are cleared): the capture's text and state.
void frame();
// From the view, once the weapons are set up this frame: the capture's samples, and Show Sight Line.
void viewFrame(const hands::State& s);

// The Weapon Offsets page. start: the capture for the weapon held in `hand` (1 main, 0 off), the menu closed and
// reopened on `returnPage` with the result.
enum class Phase
{
    Idle,
    Capturing,
    Result, // a correction to Apply or Cancel
};
[[nodiscard]] Phase phase();
bool start(int hand, int returnPage);
void apply();
void cancel();
[[nodiscard]] bool canUndo();
void undo();
// Changes whenever the page's section must be built again (the phase, an undo made possible or not).
[[nodiscard]] int version();
// The section's text lines (0..): the last result, or why the held weapon can't be aligned; null past the last.
[[nodiscard]] const char* statusLine(int i);
// Whether the weapon in `hand` can be aligned (a gun with a sight line).
[[nodiscard]] bool alignable(int hand);

} // namespace qvr::sightalign
