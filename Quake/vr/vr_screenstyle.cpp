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
    // The super nailgun: a riveted bezel in the sloped rear face of a raised block on the receiver's top, at its back
    // end (v_nail2.blend, Misc/quakevr/make_screen_housing_nail2.py, which reads this entry's opening, radius and
    // offset); the back of the gun round the hand is free. In window heights h: the window 1.6h x 1h (h 28.56 font
    // pixels), corners chamfered 0.25h; the glass tilted with the face (weapons.txt wpntxt_pitch 39.5: 50 degrees from
    // level), 0.05h behind the bezel's front (the offset: from weapons.txt's place onto it, world units right/up/out).
    // No box.
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
            .offset = {0.0255f, -0.0002f, 0.0013f},
            .box = 0.f,
            .housing = true,
            .opening = {23.02f, 14.28f}}},
    // The lightning gun: a flat bronze bezel with chamfered corners on the receiver's upright rear face under the rear
    // sight, two rivets on each side frame (v_light.blend, Misc/quakevr/make_screen_housing_light.py reads `opening`,
    // `radius`, `size` and `offset` from here and snaps them to the model's grid). The glass upright, 0.06 glass heights
    // behind the bezel's front: the offset moves it from weapons.txt's place (model -2.964 -0.218 9.762) to the
    // opening's (-2.791 -0.172 9.767; 2.551 model units a world unit). Smaller digits (size): two lines in the window.
    // v_light_plasma.mdl (stem "light_plasma") has no housing: the default.
    {"light",
        {.radius = -6.82f,
            .bevel = 1.6f,
            .vignette = 0.4f,
            .sheen = 0.6f,
            .soft = 0.55f,
            .glass = 0.13f,
            .tint = {0.9f, 1.f, 0.9f},
            .crt = 0.4f,
            .size = 0.68f,
            .offset = {-0.0184f, 0.002f, -0.0677f},
            .box = 0.f,
            .housing = true,
            .opening = {25.37f, 15.81f}}},
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
