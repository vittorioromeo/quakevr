// vr_screenstyle.hpp -- the weapons' ammo screens' look, per weapon (ROUND24.md, "Ammo screens as part of the gun").
//
// Every ammo screen is rounded dark glass set in its frame: rounded corners, the glass's own dark colour under the face,
// a vignette, a thin dark inner bevel, a faint reflection, softer phosphor digits (Shade::Screen's State::frame and
// State::glass; vr_weapon_screen_style 0: the old square screen). The table in vr_screenstyle.cpp gives each weapon its
// own (by its model: v_shot.mdl and g_shot.mdl are "shot"), with a default for the rest: the corner radius, the bevel,
// the tint, the screen's size and an offset to fit it into a housing modelled on the gun (the shotgun's bezel and bolts).

#pragma once

#include <glm/glm.hpp>

namespace qvr::screenstyle
{

struct Style
{
    float radius;      // the corners' radius, in font pixels (a character is 8)
    float bevel;       // the inner bevel's width, in font pixels (0 none)
    float vignette;    // how much darker the glass is towards its edges (0..1)
    float sheen;       // the glass's faint reflection (0..1)
    float soft;        // the digits' softness: the phosphor bleeding into the glass (0..1)
    float glass;       // the glass's own colour under the face: the phosphor's colour times this (0 black)
    glm::vec3 tint;    // and that colour times this (per weapon: a cooler or warmer glass)
    float crt;         // the CRT look's strength times this (vr_weapon_screen_crt)
    float size;        // the screen's size times this (weapons.txt's wpntxt_scale)
    glm::vec3 offset;  // the screen moved along its right, up and out (units, after the weapon's own offset)
    float box;         // the bezel box round the glass: its width times this (0: none, the model's housing frames it)
    bool housing;      // the model has the screen's housing (its bezel and bolts): the offset fits the glass into it
};

// The style of the weapon whose model is `model` (a model's name, "progs/v_shot.mdl"; null or unknown: the default).
[[nodiscard]] const Style& forModel(const char* model);

// The default style (the ammo pouch's counter, the tool gun's screen, any screen of no weapon).
[[nodiscard]] const Style& fallback();

// vr_weapon_screen_style: the rounded glass look on (else the old square screen, every weapon the same).
[[nodiscard]] bool enabled();

} // namespace qvr::screenstyle
