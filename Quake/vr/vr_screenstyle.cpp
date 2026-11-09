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

// Per weapon. To give another gun its housing: model the bezel on its .blend (make_screen_housing.py), then add its
// entry here with `.housing = true`, the box 0 and the offset that seats the glass in the inset (ROUND24.md recipe).
constexpr Entry table[] = {
    // The shotgun: the bezel with four bolts on the receiver's end face (v_shot.blend, make_screen_housing.py); the glass
    // a little smaller, sat in the bezel's inset, its own box only a hairline lip.
    {"shot",
        {.radius = 4.5f,
            .bevel = 2.f,
            .vignette = 0.4f,
            .sheen = 0.7f,
            .soft = 0.55f,
            .glass = 0.13f,
            .tint = {0.9f, 1.f, 0.9f},
            .crt = 0.4f,
            .size = 1.f,
            .offset = {0.f, 0.f, 0.f},
            .box = 1.2f,
            .housing = true}},
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
