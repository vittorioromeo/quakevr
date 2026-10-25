// vr_blade.hpp -- a weapon's blade shape for melee, per weapon slot (vr_weapons.inc, "The blade's shape"; ROUND22.md,
// "Blade shape editor"): the near end, the width, the curve and a head's box, as Weapon Offsets keep them. QC reads them
// through meleeblade (vr_builtins.cpp; QC vr_melee.qc VR_Melee_Line, _At, the track's head and width), the blade shape
// editor (vr_bladeedit.cpp) edits them in VR and vr_blade_dump prints them as vr_weapons.inc lines. Every key's default
// keeps the shape the code gave before (the kind's near end, the model's curve and head).

#pragma once

#include "vr_engine.hpp"

namespace qvr::blade
{

// The curve's control points: at shares 1/6 .. 5/6 of the line from the hand to the tip (the ends on the line).
inline constexpr int spinePoints = 5;
[[nodiscard]] constexpr float spineShare(int i)
{
    return static_cast<float>(i + 1) / static_cast<float>(spinePoints + 1);
}

struct Shape
{
    float nearBack{-1.f}; // the near end behind the hand, a share of the line (-1: the kind's own, QC VR_Melee_NearBack)
    float width{1.f};     // times the swept points' radius (QC VR_MELEE_RADIUS)
    bool spine{false};    // the curve below (false: the model's own: a curved AD sword's made spine, else straight)
    glm::vec2 point[spinePoints]{}; // each point off the line: x along its normal in the blade's plane, y along the model's +y
    bool head{false};     // the head's box below (false: the model's own: the AD hammer's and mace's)
    glm::vec3 headLo{0.f}, headHi{0.f}; // x along the line from the hand, y along its normal, z along the model's +y
    float from{-1.f};          // where the blade proper starts, a share of the line (-1: the kind's own, nothing bent)
    glm::vec2 fromOff{0.f};    // that point off the line, as the curve's points
    glm::vec3 origin{0.f};     // the line's start off the holding hand (units along its forward, right, up; Line Start)
    glm::vec3 tipOffset{0.f};  // the line's far end off the muzzle (the same frame; a gun's Tip, its shots' muzzle kept)
};

// Whether `s` moves the line's ends (Blade Org, Blade Tip: QC VR_Melee_Line's line start and tip).
[[nodiscard]] inline bool hasEnds(const Shape& s)
{
    return s.origin != glm::vec3{0.f} || s.tipOffset != glm::vec3{0.f};
}

// Whether `s` has a blade start of its own (Blade From: a share inside the line).
[[nodiscard]] inline bool hasFrom(const Shape& s)
{
    return s.from > 0.f && s.from < 1.f;
}

// The blade start's bend at share `f` (shares of the length, as spineAt): the line straight from the hand to the start's
// point and from it to the tip (0 at both ends, its offset at the start's share). Zero without a start of its own.
[[nodiscard]] glm::vec2 fromAt(const Shape& s, float f);

// Slot `slot`'s shape (its defaults for no slot).
[[nodiscard]] Shape shape(int slot);

// The curve's way off the line at share `f` (0 the hand, 1 the tip; outside them 0): a Catmull-Rom spline through the hand,
// the five points and the tip (the hand's and tip's ends extended straight), as the points' shares.
[[nodiscard]] glm::vec2 spineAt(const Shape& s, float f);

void init(); // the commands (vr_blade_dump)

// vr_blade_dump [slot 1..]: the slot's (default: the main hand's weapon's) blade keys as vr_weapons.inc lines.
void dump(int slot);

// vr_blade_dump fist: the empty hand's blow line (vr_melee_fist_*: QC VR_Melee_Line's fist) as vr_cvars.inc lines.
void dumpFist();

} // namespace qvr::blade
