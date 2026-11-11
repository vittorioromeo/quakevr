// vr_screenstyle.cpp -- see vr_screenstyle.hpp.

#include "vr_screenstyle.hpp"
#include "vr_cvars.hpp"

namespace qvr::screenstyle
{
namespace
{

// The default: every weapon's screen without a housing of its own. A thick near-black frame (the box) round it.
constexpr Style defaultStyle{.radius = 4.f,
    .bevel = 1.6f,
    .vignette = 0.35f,
    .sheen = 0.6f,
    .soft = 0.5f,
    .glass = 0.13f,
    .tint = {1.f, 1.f, 1.f},
    .crt = 0.4f,
    .size = 1.f,
    .offset = {0.f, 0.f, 0.f},
    .box = 1.5f,
    .housing = false};

struct Entry
{
    const char* stem; // the model's name without its folder, its v_ or g_ and its extension
    Style style;
};

// Per weapon. To give another gun its housing: model the bezel on its .blend (a per-gun script, as
// Misc/quakevr/make_screen_housing_shot.py, _ssg.py and _nailpistol.py), then add its
// entry here with `.housing = true`, the box 0 and the offset that seats the glass in the inset (ROUND24.md recipe).
constexpr Entry table[] = {
    // The shotgun: the rounded bezel with four bolts on the receiver's end face (v_shot.blend, modelled by
    // Misc/quakevr/make_screen_housing_shot.py from this entry's opening, radius and out). The glass upright
    // (weapons.txt wpntxt_pitch 2) a little way into the opening, between the bezel's front and the leaning end face:
    // the offset (right, up, out; world units) moves it from weapons.txt's place (model 0.862 -0.068 2.582) to the
    // opening's (1.177 -0.03 2.48, drawn 0.023 further in; 2.029 model units a world unit), so a config's own
    // wpntxt_x/y/z still land.
    {"shot",
        {.radius = 3.f,
            .bevel = 2.f,
            .vignette = 0.4f,
            .sheen = 0.7f,
            .soft = 0.55f,
            .glass = 0.13f,
            .tint = {0.9f, 1.f, 0.9f},
            .crt = 0.4f,
            .size = 1.f,
            .offset = {-0.0185f, -0.0505f, -0.1555f},
            .box = 0.f,
            .housing = true,
            .opening = {11.f, 8.5f}}},
    // The super shotgun: a small window in a low bezel on the breech block's sloped top, under the rib's sight block, a
    // rivet each side (v_shot2.blend, make_screen_housing_ssg.py reads `opening`, `radius` and `offset` from here to
    // model it); the glass sunk a little into the gun, no box: the bezel frames it.
    {"shot2",
        {.radius = 0.f,
            .bevel = 1.6f,
            .vignette = 0.5f,
            .sheen = 0.6f,
            .soft = 0.55f,
            .glass = 0.05f,
            .tint = {0.9f, 1.f, 0.9f},
            .crt = 0.4f,
            .size = 1.36f,
            .offset = {-0.01f, -0.018f, -0.032f},
            .box = 0.f,
            .housing = true,
            .opening = {14.2f, 11.15f}}},
    // The nail pistol: a chunky bevelled housing on the slide's rear face, a rounded window over a grille
    // (v_nailpistol.blend, Misc/quakevr/make_screen_housing_nailpistol.py, which reads this entry's opening, radius and size
    // and weapons.txt's wpntxt_scale to model the window). The glass fills the window; no box (the window frames it).
    {"nailpistol",
        {.radius = 4.f,
            .bevel = 1.6f,
            .vignette = 0.4f,
            .sheen = 0.6f,
            .soft = 0.55f,
            .glass = 0.13f,
            .tint = {0.9f, 1.f, 0.9f},
            .crt = 0.4f,
            .size = 1.f,
            .offset = {0.f, 0.f, 0.f},
            .box = 0.f,
            .housing = true,
            .opening = {24.f, 14.f}}},
    // The nailgun: a bevelled bezel with two round fasteners each side on the receiver's rear, between the barrels, in
    // front of the block with the red light (v_nail.blend, Misc/quakevr/make_screen_housing_nail.py reads `opening`,
    // `radius`, `size` and `offset` from here to model it). As his mockup draws it: a wide window (2:1), its corners cut
    // at 45 degrees, the digits at today's size; on the model's byte grid 1.627 x 0.814 model units (a font pixel 0.0372
    // at wpntxt_scale 0.2), the cut 0.16. The offset (right, up, out; world units) centres the glass in the window,
    // 0.065 behind the bezel's front.
    {"nail",
        {.radius = -4.4f,
            .bevel = 1.6f,
            .vignette = 0.4f,
            .sheen = 0.6f,
            .soft = 0.55f,
            .glass = 0.13f,
            .tint = {0.9f, 1.f, 0.9f},
            .crt = 0.4f,
            .size = 1.f,
            .offset = {0.0132f, -0.0024f, 0.0014f},
            .box = 0.f,
            .housing = true,
            .opening = {22.f, 11.f}}},
    // The super nailgun: a riveted bezel in the long, low rear slope of a wedge on the receiver's top, at its back end
    // (v_nail2.blend, Misc/quakevr/make_screen_housing_nail2.py, which reads this entry's opening, radius and offset);
    // the back of the gun round the hand is free. In window heights h: the window 1.6h x 1h (h 28.56 font pixels),
    // corners chamfered 0.25h; the glass tilted with the face (weapons.txt wpntxt_pitch 70.5: 19 degrees from level,
    // facing up to the eye), 0.05h behind the bezel's front (the offset: from weapons.txt's place onto it, world units
    // right/up/out). No box.
    {"nail2",
        {.radius = -7.2f,
            .bevel = 1.6f,
            .vignette = 0.45f,
            .sheen = 0.6f,
            .soft = 0.55f,
            .glass = 0.1f,
            .tint = {0.9f, 1.f, 0.9f},
            .crt = 0.4f,
            .size = 1.f,
            .offset = {0.0255f, 0.0031f, 0.0013f},
            .box = 0.f,
            .housing = true,
            .opening = {23.02f, 14.28f}}},
    // The lightning gun: a flat bronze plate over the receiver's whole upright rear face under the rear sight (its
    // trapezoid), the window a trapezoid too (`taper`: its top 0.73 of its bottom), chamfered, sunk into the plate, two
    // rivets each side between them (v_light.blend, Misc/quakevr/make_screen_housing_light.py reads `opening`, `taper`,
    // `radius`, `size` and `offset` from here and snaps them to the model's grid). The glass upright, 0.06 window heights
    // behind the plate's front: the offset moves it from weapons.txt's place (model -2.964 -0.218 9.762) to the
    // window's (2.551 model units a world unit). Smaller digits (size): two lines fit the window's narrower top.
    // v_light_plasma.mdl (stem "light_plasma") has no housing: the default.
    {"light",
        {.radius = -4.71f,
            .bevel = 1.6f,
            .vignette = 0.4f,
            .sheen = 0.6f,
            .soft = 0.55f,
            .glass = 0.13f,
            .tint = {0.9f, 1.f, 0.9f},
            .crt = 0.4f,
            .size = 0.8f,
            .offset = {-0.0184f, 0.002f, -0.0742f},
            .box = 0.f,
            .housing = true,
            .opening = {31.15f, 15.68f},
            .taper = 0.7308f}},
    // The rocket launcher: a raised bronze enclosure on the sloped shoulder where the tube steps down to the rear
    // exhaust, near the grip (v_rock2.blend, Misc/quakevr/make_screen_housing_rocket.py reads `opening`, `radius`,
    // `size` and `offset` from here). Its face leans 38.7 degrees from level; the window square, 1h (h 2.016 model
    // units: four of the grid's rows up the face); the glass leans with it (weapons.txt wpntxt_pitch 51.26), rounded
    // 0.15h in the square window, 0.02h behind the face (the offset: from weapons.txt's place onto it, world units
    // right/up/out). Rounded, chamfered edges; two round rivets each side. No box.
    {"rock2",
        {.radius = 4.61f,
            .bevel = 1.6f,
            .vignette = 0.4f,
            .sheen = 0.6f,
            .soft = 0.55f,
            .glass = 0.13f,
            .tint = {0.9f, 1.f, 0.9f},
            .crt = 0.4f,
            .size = 1.f,
            .offset = {-0.0039f, 0.0315f, 0.033f},
            .box = 0.f,
            .housing = true,
            .opening = {15.54f, 15.35f}}},
    // The grenade launcher: a square riveted bronze bezel on the rear face of the top block (the pyramid's upper face
    // under the top ridge), the window chamfered, a rivet in each of the bezel's corners (v_rock.blend,
    // Misc/quakevr/make_screen_housing_gl.py reads `opening`, `radius` and `offset` from here). The plate leans on the
    // grid's plane nearest the face (56.4 degrees from level; weapons.txt slot 5 wpntxt_pitch 33.58); the window 1H
    // square (H 2.21 model units: four of its rows), corners cut 0.12H (radius < 0: cut); the glass 0.04H behind the
    // plate's front (the offset: from weapons.txt's place onto it, world units right/up/out). No box.
    {"rock",
        {.radius = -3.87f,
            .bevel = 1.6f,
            .vignette = 0.4f,
            .sheen = 0.6f,
            .soft = 0.55f,
            .glass = 0.13f,
            .tint = {0.9f, 1.f, 0.9f},
            .crt = 0.4f,
            .size = 1.f,
            .offset = {0.0544f, -0.3619f, -0.1192f},
            .box = 0.f,
            .housing = true,
            .opening = {15.48f, 15.64f}}},
    // The grenade launcher's multi-grenade model (v_multi.blend): the same housing, by the same script, on its own grid
    // and its face (slot 13).
    {"multi",
        {.radius = -3.88f,
            .bevel = 1.6f,
            .vignette = 0.4f,
            .sheen = 0.6f,
            .soft = 0.55f,
            .glass = 0.13f,
            .tint = {0.9f, 1.f, 0.9f},
            .crt = 0.4f,
            .size = 1.f,
            .offset = {0.0443f, -0.2030f, -0.3323f},
            .box = 0.f,
            .housing = true,
            .opening = {15.51f, 15.70f}}},
};

[[nodiscard]] bool stemIs(const char* model, const char* stem)
{
    const char* base = model;
    for(const char* p = model; *p; p++)
    {
        if(*p == '/' || *p == '\\')
        {
            base = p + 1;
        }
    }
    if((base[0] == 'v' || base[0] == 'g') && base[1] == '_')
    {
        base += 2;
    }
    const char* s = stem;
    while(*s && *base == *s)
    {
        base++;
        s++;
    }
    return *s == '\0' && (*base == '\0' || *base == '.');
}

} // namespace

const Style& fallback()
{
    return defaultStyle;
}

const Style& forModel(const char* model)
{
    if(model)
    {
        for(const Entry& e : table)
        {
            if(stemIs(model, e.stem))
            {
                return e.style;
            }
        }
    }
    return defaultStyle;
}

bool enabled()
{
    return vr_weapon_screen_style.value != 0.f;
}

} // namespace qvr::screenstyle
