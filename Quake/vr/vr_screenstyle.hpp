// vr_screenstyle.hpp -- the weapons' ammo screens' look, per weapon (ROUND24.md, "Ammo screens as part of the gun").
//
// Every ammo screen is deep dark glass set in a thick dark frame: rounded (or chamfered) corners, an inner bevel and an
// inset shadow under the top edge, a vignette, a faint sheen, pale glowing phosphor digits (Shade::Screen's State::frame and
// State::glass; vr_weapon_screen_style 0: the old square screen). The table in vr_screenstyle.cpp gives each weapon its
// own (by its model: v_shot.mdl and g_shot.mdl are "shot"), with a default for the rest: the corner radius, the bevel,
// the tint, the screen's size and an offset to fit it into a housing modelled on the gun (the shotgun's bezel and bolts).

#pragma once

#include <glm/glm.hpp>

namespace qvr::screenstyle
{

struct Style
{
    float radius;      // the corners, in font pixels (a character is 8): > 0 rounded (the radius), < 0 chamfered (the cut)
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
    // The housing's opening, half its width and height in font pixels (0: the glass sized by its text). The glass is
    // then always this size and shape (its text centred, made finer when it needs more room), so that it fills the
    // bezel's opening: Misc/quakevr/make_screen_housing_<gun>.py (shot, ssg, nailpistol) read this table (the
    // opening, `radius`, the SSG's `offset`) to model it: change it, rebuild the engine and run the gun's script.
    glm::vec2 opening{0.f, 0.f};
    // The glass as a trapezoid, wider at the bottom: its top edge's width as a fraction of the bottom's (1: the
    // rectangle). `opening.x` is then the bottom's half width, `radius` < 0 chamfers its corners (cut that much along
    // both edges; a rounded radius is taken as a chamfer), and the text, upright and centred, is made finer when its
    // top line wouldn't fit the narrower top (make_screen_housing_light.py reads it).
    float taper{1.f};
    // Reloading on, the magazine's rounds over the total are grown to fill the opening (vr_ammo_mag_scale, text3d's
    // Glyphs::fill): never past the font's size times this (0: no cap). 1 for a housing built round its screen's text as
    // it was without one (the Widowmaker's, the AD Plasma Gun's): the digits keep their size, made smaller only when
    // they would not fit.
    float fillMax{0.f};
};

// The style of the weapon whose model is `model` (a model's name, "progs/v_shot.mdl"; null or unknown: the default).
// `mirrored`: the model drawn mirrored (a gun in the left hand): the offset's right (x) turned round, so the glass keeps
// its place in the mirrored housing (the glass itself is never mirrored: its right stays the reader's right).
[[nodiscard]] const Style& forModel(const char* model, bool mirrored = false);

// The default style (the ammo pouch's counter, the tool gun's screen, any screen of no weapon).
[[nodiscard]] const Style& fallback();

// vr_weapon_screen_style: the rounded glass look on (else the old square screen, every weapon the same).
[[nodiscard]] bool enabled();

} // namespace qvr::screenstyle
