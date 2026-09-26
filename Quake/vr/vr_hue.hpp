// vr_hue.hpp -- the player's colours: one hue for everything that is the player's own.
//
// vr_player_hue (and vr_player_saturation) colour the player's effects alike: the wrist gadget's
// screen and light, the weapons' ammo screens, the wrist log and the other screen text, the iron
// sights, the force grab (its aiming line, the tendril, the target's glow, the sparkles), the
// teleport arc, the crosshair and the menu's laser. Each has its own hue setting too: -1 (any
// negative value) follows the player's, 0..360 is its own (vr_gadget_screen_hue, vr_sight_hue,
// vr_forcegrab_hue, vr_teleport_hue, vr_crosshair_hue, vr_menu_laser_hue).

#pragma once

#include "vr_color.hpp"
#include "vr_cvars.hpp"

#include <algorithm>

namespace qvr::hue
{

// Whether an effect's hue setting follows the player's.
[[nodiscard]] inline bool follows(const cvar_t& own)
{
    return own.value < 0.f;
}

// The hue (degrees) an effect takes from its own setting.
[[nodiscard]] inline float of(const cvar_t& own)
{
    return follows(own) ? vr_player_hue.value : own.value;
}

// An effect's saturation `s` (its own look), scaled by vr_player_saturation when it follows the
// player's hue (0: white, 1: as made, up to 2).
[[nodiscard]] inline float saturation(const cvar_t& own, float s)
{
    return follows(own) ? std::clamp(s * std::clamp(vr_player_saturation.value, 0.f, 2.f), 0.f, 1.f) : s;
}

// An effect's colour: its hue (own or the player's), saturation `s` and value `v`.
[[nodiscard]] inline glm::vec3 color(const cvar_t& own, float s, float v)
{
    return hsv(of(own), saturation(own, s), v);
}

// As color(), with an alpha.
[[nodiscard]] inline glm::vec4 color(const cvar_t& own, float s, float v, float alpha)
{
    return glm::vec4{color(own, s, v), alpha};
}

} // namespace qvr::hue
