// vr_blade.cpp -- see vr_blade.hpp.

#include "vr_blade.hpp"
#include "vr_anchor.hpp"
#include "vr_cvars.hpp"
#include "vr_render.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Floor.hpp"

#include <glm/gtc/type_ptr.hpp>

namespace qvr::blade
{
namespace
{

using weapons::Key;

// vr_blade_dump fit's vertex lists (kept between calls).
struct FitScratch
{
    za::Vector<glm::vec3> rest;
    za::Vector<glm::vec3> now;
};
FitScratch fitScratch;

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
    {Key::BladeFrom, "BladeFrom"}, {Key::BladeFromOff, "BladeFromOff"}, {Key::BladeFromSide, "BladeFromSide"},
    {Key::BladeOrgX, "BladeOrgX"}, {Key::BladeOrgY, "BladeOrgY"}, {Key::BladeOrgZ, "BladeOrgZ"},
    {Key::BladeTipX, "BladeTipX"}, {Key::BladeTipY, "BladeTipY"}, {Key::BladeTipZ, "BladeTipZ"}};

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
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "fit"))
    {
        const int slot = Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) - 1 : weapons::heldSlot(1);
        if(slot >= 0 && slot < weapons::numSlots)
        {
            dumpFit(slot);
        }
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
    s.origin = weapons::vec(slot, Key::BladeOrgX, Key::BladeOrgY, Key::BladeOrgZ);
    s.tipOffset = weapons::vec(slot, Key::BladeTipX, Key::BladeTipY, Key::BladeTipZ);
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
    Con_Printf("// %s (slot %d, vr_wofs_*_%02d): the blade's shape and tip (lines of quakevr/data/weapons.txt's block "
               "\"weapon %d\"; * changed)\n",
        id ? id->string : "?", slot, slot + 1, slot);
    for(const Named& n : dumped)
    {
        const cvar_t* var = weapons::cvar(weapons::ownerSlot(slot, n.key), n.key);
        if(!var)
        {
            continue;
        }
        const bool changed = var->default_string && ZA_STRCMP(var->string, var->default_string) != 0;
        Con_Printf("    %s \"%s\"%s\n", weapons::keyName(n.key), var->string, changed ? " //*" : "");
    }
}

// How the blow line and its curve sit on the model (vr_blade_dump fit [slot]; ROUND25.md "axelag"): the weapon laid in
// the hand as drawn (render::anchorMatrix, the hand at the origin: forward, left, up), the line from its start to its tip,
// and at each tenth of it the curve's point (the striking surface QC sweeps) against the model's vertices in that slice
// of the line: how far out along the curve's normal they reach on each side (units), the nearest vertex to the point.
// A curve point far from every vertex strikes with air; a slice whose vertices lie all to one side of the point is
// the blade drawn off the line.
void dumpFit(int slot)
{
    const cvar_t* id = weapons::cvar(slot, Key::ID);
    qmodel_t* model = id && id->string[0] ? Mod_ForName(id->string, false) : nullptr;
    if(!model || model->type != mod_alias)
    {
        Con_Printf("vr_blade_dump fit: slot %d has no alias model\n", slot + 1);
        return;
    }
    view::ViewEntity ve;
    ve.ent.model = model;
    ve.ent.scale = ENTSCALE_DEFAULT;
    float raw[16];
    render::anchorMatrix(ve, glm::vec3{0.f}, raw);
    const glm::mat4 toHand = glm::make_mat4(raw);
    za::Vector<glm::vec3>& rest = fitScratch.rest;
    za::Vector<glm::vec3>& now = fitScratch.now;
    if(!anchor::posedVertices(ve.ent, 1.f, rest, now) || rest.empty())
    {
        Con_Printf("vr_blade_dump fit: %s has no vertices\n", model->name);
        return;
    }
    const Shape s = shape(slot);
    const glm::vec3 f{1.f, 0.f, 0.f}, r{0.f, -1.f, 0.f}, u{0.f, 0.f, 1.f};
    const glm::vec3 hand = f * s.origin.x + r * s.origin.y + u * s.origin.z;
    const glm::vec3 tip = view::anchorPosition(ve, weapons::anchor(slot, Key::MuzzleAnchor),
                              weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ)) +
                          f * s.tipOffset.x + r * s.tipOffset.y + u * s.tipOffset.z;
    const float len = glm::length(tip - hand);
    if(len < 1.f)
    {
        Con_Printf("vr_blade_dump fit: %s's line is too short\n", model->name);
        return;
    }
    const glm::vec3 a = (tip - hand) / len;
    const glm::vec3 y = glm::normalize(-r - a * glm::dot(-r, a));
    const glm::vec3 p = glm::normalize(glm::cross(y, a));
    float lo = 1e9f, hi = -1e9f;
    glm::vec3 butt{0.f}; // the vertex farthest from the tip: a handle's end
    for(const glm::vec3& v : rest)
    {
        const glm::vec3 w{toHand * glm::vec4{v, 1.f}};
        const float x = glm::dot(w - hand, a) / len;
        lo = za::min(lo, x);
        hi = za::max(hi, x);
        if(glm::distance(w, tip) > glm::distance(butt, tip))
        {
            butt = w;
        }
    }
    // The handle's way (the hand to that end) against the line: an axe's line near its handle, as the axe's, chops
    // across it; far off it, a chop with the edge leading runs along the line (a stab to VR_Decap_Slash).
    const glm::vec3 grip = view::anchorPosition(ve, weapons::anchor(slot, Key::HandAnchor), glm::vec3{0.f});
    const float apart = glm::degrees(za::acos(za::clamp(glm::dot(glm::normalize(grip - butt), a), -1.f, 1.f)));
    Con_Printf("fit: %s, the line %.1f units (start %.1f %.1f %.1f, tip %.1f %.1f %.1f; forward, left, up), the model %.2f .. %.2f "
               "of it, spine %d, from %.2f\n",
        model->name, len, hand.x, hand.y, hand.z, tip.x, tip.y, tip.z, lo, hi, s.spine ? 1 : 0, s.from);
    Con_Printf("fit: the hand anchor at %.1f %.1f %.1f, the handle's end at %.1f %.1f %.1f (%.1f behind the hand), the handle %.0f "
               "degrees off the line\n",
        grip.x, grip.y, grip.z, butt.x, butt.y, butt.z, glm::distance(grip, butt), apart);
    for(int k = 0; k <= 10; k++)
    {
        const float at = static_cast<float>(k) / 10.f;
        const glm::vec2 c = (s.spine ? spineAt(s, at) : glm::vec2{0.f}) + fromAt(s, at);
        const glm::vec3 pt = hand + a * (len * at) + (p * c.x + y * c.y) * len;
        float pmin = 1e9f, pmax = -1e9f, near = 1e9f;
        int count = 0;
        for(const glm::vec3& v : rest)
        {
            const glm::vec3 w{toHand * glm::vec4{v, 1.f}};
            near = za::min(near, glm::distance(w, pt));
            if(za::fabs(glm::dot(w - hand, a) / len - at) <= 0.05f)
            {
                const float off = glm::dot(w - hand, p);
                pmin = za::min(pmin, off);
                pmax = za::max(pmax, off);
                count++;
            }
        }
        Con_Printf("fit: %.1f the point %+.1f off the line (side %+.1f), the model's vertices there %+.1f .. %+.1f off (%d), "
                   "the nearest %.1f away\n",
            at, c.x * len, c.y * len, count ? pmin : 0.f, count ? pmax : 0.f, count, near);
    }
    // The blade's plane drawn in text: along the line across, off it down the page (+ off at the bottom; a row two columns' worth,
    // the model fitting the width): '#' the model's vertices, '.' the line, '*' the curve, 'H' the hand, 'T' the tip.
    constexpr int cols = 64, rows = 28;
    char grid[rows][cols + 1];
    for(int i = 0; i < rows; i++)
    {
        ZA_MEMSET(grid[i], ' ', cols);
        grid[i][cols] = 0;
    }
    const float x0 = za::min(lo, 0.f) * len - 0.5f;
    const float sc = static_cast<float>(cols) / ((za::max(hi, 1.f) - za::min(lo, 0.f)) * len + 1.f); // columns a unit
    const auto plot = [&](const glm::vec3& w, char ch) {
        const int cx = static_cast<int>(za::floor((glm::dot(w - hand, a) - x0) * sc));
        const int cy = static_cast<int>(za::floor(glm::dot(w - hand, p) * sc * 0.5f + static_cast<float>(rows) * 0.5f));
        if(cx >= 0 && cx < cols && cy >= 0 && cy < rows && (ch != '#' || grid[cy][cx] == ' '))
        {
            grid[cy][cx] = ch;
        }
    };
    for(const glm::vec3& v : rest)
    {
        plot(glm::vec3{toHand * glm::vec4{v, 1.f}}, '#');
    }
    for(int k = 0; k <= 40; k++)
    {
        const float at = static_cast<float>(k) / 40.f;
        const glm::vec2 c = (s.spine ? spineAt(s, at) : glm::vec2{0.f}) + fromAt(s, at);
        plot(hand + a * (len * at), '.');
        plot(hand + a * (len * at) + (p * c.x + y * c.y) * len, '*');
    }
    plot(hand, 'H');
    plot(tip, 'T');
    for(int i = 0; i < rows; i++)
    {
        Con_Printf("fit| %s\n", grid[i]);
    }
}

void dumpFist()
{
    Con_Printf("// the fist's blow line (vr_cvars.inc lines, their comments left out; * changed)\n");
    for(const cvar_t* var : fistDumped)
    {
        const bool changed = var->default_string && ZA_STRCMP(var->string, var->default_string) != 0;
        Con_Printf("QVR_CVAR(%s, \"%s\", CVAR_ARCHIVE)%s\n", var->name, var->string, changed ? " //*" : "");
    }
}

} // namespace qvr::blade
