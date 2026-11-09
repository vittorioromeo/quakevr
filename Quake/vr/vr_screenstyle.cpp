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
    // Misc/quakevr/make_screen_housing_shot.py from this entry's opening and radius). The glass upright (weapons.txt
    // wpntxt_pitch 2) halfway down the opening: the offset (right, up, out; world units) moves it from weapons.txt's
    // place (model 0.862 -0.068 2.582) to the opening's (1.066 -0.03 2.48; 2.029 model units a world unit), so a
    // config's own wpntxt_x/y/z still land.
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
            .offset = {-0.0185f, -0.0505f, -0.1004f},
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
    // `radius`, `size` and `offset` from here to model it). His spec: a square window, its corners cut at 45 degrees;
    // on the model's byte grid it is 0.697 x 0.712 model units (a font pixel 0.0372 at wpntxt_scale 0.2), the cut
    // 0.16. The offset (right, up, out; world units) centres the glass in the window, 0.057 behind the bezel's front.
    {"nail",
        {.radius = -4.2f,
            .bevel = 1.6f,
            .vignette = 0.4f,
            .sheen = 0.6f,
            .soft = 0.55f,
            .glass = 0.13f,
            .tint = {0.9f, 1.f, 0.9f},
            .crt = 0.4f,
            .size = 1.f,
            .offset = {0.0132f, -0.0296f, 0.0057f},
            .box = 0.f,
            .housing = true,
            .opening = {9.37f, 9.57f}}},
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
