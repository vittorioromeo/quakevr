// vr_blade.cpp -- see vr_blade.hpp.

#include "vr_blade.hpp"
#include "vr_cvars.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Math/Clamp.hpp"

namespace qvr::blade
{
namespace
{

using weapons::Key;

constexpr Key spineKeys[spinePoints][2] = {{Key::BladeSpine1Off, Key::BladeSpine1Side},
    {Key::BladeSpine2Off, Key::BladeSpine2Side}, {Key::BladeSpine3Off, Key::BladeSpine3Side},
    {Key::BladeSpine4Off, Key::BladeSpine4Side}, {Key::BladeSpine5Off, Key::BladeSpine5Side}};

struct Named
{
    Key key;
    const char* name; // its vr_weapons.inc enum name
};

// The blade's keys and the tip's offset, as vr_weapons.inc names them (vr_blade_dump).
constexpr Named dumped[] = {{Key::MuzzleOffsetX, "MuzzleOffsetX"}, {Key::MuzzleOffsetY, "MuzzleOffsetY"},
    {Key::MuzzleOffsetZ, "MuzzleOffsetZ"}, {Key::BladeNear, "BladeNear"}, {Key::BladeWidth, "BladeWidth"},
    {Key::BladeSpine, "BladeSpine"}, {Key::BladeSpine1Off, "BladeSpine1Off"}, {Key::BladeSpine1Side, "BladeSpine1Side"},
    {Key::BladeSpine2Off, "BladeSpine2Off"}, {Key::BladeSpine2Side, "BladeSpine2Side"},
    {Key::BladeSpine3Off, "BladeSpine3Off"}, {Key::BladeSpine3Side, "BladeSpine3Side"},
    {Key::BladeSpine4Off, "BladeSpine4Off"}, {Key::BladeSpine4Side, "BladeSpine4Side"},
    {Key::BladeSpine5Off, "BladeSpine5Off"}, {Key::BladeSpine5Side, "BladeSpine5Side"}, {Key::BladeHead, "BladeHead"},
    {Key::BladeHeadLoX, "BladeHeadLoX"}, {Key::BladeHeadLoY, "BladeHeadLoY"}, {Key::BladeHeadLoZ, "BladeHeadLoZ"},
    {Key::BladeHeadHiX, "BladeHeadHiX"}, {Key::BladeHeadHiY, "BladeHeadHiY"}, {Key::BladeHeadHiZ, "BladeHeadHiZ"},
    {Key::BladeFrom, "BladeFrom"}, {Key::BladeFromOff, "BladeFromOff"}, {Key::BladeFromSide, "BladeFromSide"}};

// The curve's knots: the hand, the five points, the tip.
[[nodiscard]] glm::vec2 knot(const Shape& s, int i)
{
    return i <= 0 || i > spinePoints ? glm::vec2{0.f} : s.point[i - 1];
}

// The fist's blow line's settings (vr_blade_dump fist).
const cvar_t* const fistDumped[] = {&vr_melee_fist_from_x, &vr_melee_fist_from_y, &vr_melee_fist_from_z, &vr_melee_fist_to_x,
    &vr_melee_fist_to_y, &vr_melee_fist_to_z, &vr_melee_fist_width, &vr_melee_fist_mirror};

void dump_f()
{
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "fist"))
    {
        dumpFist();
        return;
    }
    dump(Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) - 1 : weapons::heldSlot(1));
}

} // namespace

Shape shape(int slot)
{
    Shape s;
    if(slot < 0 || slot >= weapons::numSlots)
    {
        return s;
    }
    s.nearBack = weapons::value(slot, Key::BladeNear);
    s.width = weapons::value(slot, Key::BladeWidth);
    s.spine = weapons::value(slot, Key::BladeSpine) >= 0.5f;
    for(int i = 0; i < spinePoints; i++)
    {
        s.point[i] = {weapons::value(slot, spineKeys[i][0]), weapons::value(slot, spineKeys[i][1])};
    }
    s.head = weapons::value(slot, Key::BladeHead) >= 0.5f;
    s.headLo = weapons::vec(slot, Key::BladeHeadLoX, Key::BladeHeadLoY, Key::BladeHeadLoZ);
    s.headHi = weapons::vec(slot, Key::BladeHeadHiX, Key::BladeHeadHiY, Key::BladeHeadHiZ);
    s.from = weapons::value(slot, Key::BladeFrom);
    s.fromOff = {weapons::value(slot, Key::BladeFromOff), weapons::value(slot, Key::BladeFromSide)};
    return s;
}

glm::vec2 fromAt(const Shape& s, float f)
{
    if(!hasFrom(s) || !(f > 0.f && f < 1.f))
    {
        return glm::vec2{0.f};
    }
    return s.fromOff * (f <= s.from ? f / s.from : (1.f - f) / (1.f - s.from));
}

glm::vec2 spineAt(const Shape& s, float f)
{
    if(!(f > 0.f && f < 1.f))
    {
        return glm::vec2{0.f};
    }
    // Uniform knots 1/6 apart: segment i from knot i to i + 1, its neighbours' tangents (the ends' phantoms mirrored: a
    // straight start from the hand, a straight run into the tip).
    const float x = f * static_cast<float>(spinePoints + 1);
    const int i = za::clamp(static_cast<int>(x), 0, spinePoints);
    const float t = x - static_cast<float>(i);
    const glm::vec2 p1 = knot(s, i), p2 = knot(s, i + 1);
    const glm::vec2 p0 = i == 0 ? 2.f * p1 - p2 : knot(s, i - 1);
    const glm::vec2 p3 = i + 1 == spinePoints + 1 ? 2.f * p2 - p1 : knot(s, i + 2);
    const float t2 = t * t, t3 = t2 * t;
    return 0.5f * (2.f * p1 + (p2 - p0) * t + (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t2 + (3.f * p1 - p0 - 3.f * p2 + p3) * t3);
}

void init()
{
    Cmd_AddCommand("vr_blade_dump", dump_f);
}

void dump(int slot)
{
    if(slot < 0 || slot >= weapons::numSlots)
    {
        Con_Printf("vr_blade_dump [slot 1..%d]: no such slot (default: the main hand's weapon)\n", weapons::numSlots);
        return;
    }
    const cvar_t* id = weapons::cvar(slot, Key::ID);
    Con_Printf("// %s (slot %d, cvars _%02d): the blade's shape and tip (vr_weapons.inc lines; * changed)\n",
        id ? id->string : "?", slot, slot + 1);
    for(const Named& n : dumped)
    {
        const cvar_t* var = weapons::cvar(weapons::ownerSlot(slot, n.key), n.key);
        if(!var)
        {
            continue;
        }
        const bool changed = var->default_string && strcmp(var->string, var->default_string) != 0;
        Con_Printf("QVR_WEAPON_DEFAULT(%d, %s, \"%s\")%s\n", slot, n.name, var->string, changed ? " //*" : "");
    }
}

void dumpFist()
{
    Con_Printf("// the fist's blow line (vr_cvars.inc lines, their comments left out; * changed)\n");
    for(const cvar_t* var : fistDumped)
    {
        const bool changed = var->default_string && strcmp(var->string, var->default_string) != 0;
        Con_Printf("QVR_CVAR(%s, \"%s\", CVAR_ARCHIVE)%s\n", var->name, var->string, changed ? " //*" : "");
    }
}

} // namespace qvr::blade
