// vr_bladeedit.cpp -- the blade shape editor: see vr_bladeedit.hpp.

#include "vr_bladeedit.hpp"
#include "vr_admelee.hpp"
#include "vr_backend.hpp"
#include "vr_blade.hpp"
#include "vr_cvars.hpp"
#include "vr_handpose.hpp"
#include "vr_hands.hpp"
#include "vr_holstersetup.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_meleeshow.hpp"
#include "vr_menu.hpp"
#include "vr_menuui.hpp"
#include "vr_text3d.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <math.h>
#include <string.h>

namespace qvr::bladeedit
{
namespace
{

using weapons::Key;

// Every setting it changes (Undo's and Cancel's snapshot): the tip's offset and the blade's keys.
constexpr Key managedKeys[] = {Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ, Key::BladeNear, Key::BladeWidth,
    Key::BladeSpine, Key::BladeSpine1Off, Key::BladeSpine1Side, Key::BladeSpine2Off, Key::BladeSpine2Side,
    Key::BladeSpine3Off, Key::BladeSpine3Side, Key::BladeSpine4Off, Key::BladeSpine4Side, Key::BladeSpine5Off,
    Key::BladeSpine5Side, Key::BladeHead, Key::BladeHeadLoX, Key::BladeHeadLoY, Key::BladeHeadLoZ, Key::BladeHeadHiX,
    Key::BladeHeadHiY, Key::BladeHeadHiZ};
constexpr int managedCount = static_cast<int>(sizeof(managedKeys) / sizeof(managedKeys[0]));
constexpr Key spineKeys[blade::spinePoints][2] = {{Key::BladeSpine1Off, Key::BladeSpine1Side},
    {Key::BladeSpine2Off, Key::BladeSpine2Side}, {Key::BladeSpine3Off, Key::BladeSpine3Side},
    {Key::BladeSpine4Off, Key::BladeSpine4Side}, {Key::BladeSpine5Off, Key::BladeSpine5Side}};

struct Snapshot
{
    float v[managedCount]{};
};

enum HandleId : int
{
    HTip,
    HNear,
    HWidth,
    HSpine1,
    HHeadLo = HSpine1 + blade::spinePoints,
    HHeadHi,
    HCount
};

enum ButtonId : int
{
    BtnUndo,
    BtnReset,
    BtnStraighten,
    BtnHead,
    BtnCancel,
    BtnDone,
    BtnCount
};

constexpr float PANEL_DISTANCE = 0.55f; // metres ahead of the eyes
constexpr float PANEL_DROP = 0.32f;     // metres below them (under the blade held out ahead)
constexpr float BUTTON_WIDTH = 0.17f;   // metres
constexpr float BUTTON_HEIGHT = 0.055f;
constexpr float BUTTON_GAP = 0.015f;
constexpr float BUTTON_DEPTH = 0.08f;   // a hand this near the panel's plane (metres) is at a button
constexpr float HANDLE_REACH = 0.05f;   // metres: a hand this near a handle is at it
constexpr float MELEE_RADIUS = 3.f;     // units: QC VR_MELEE_RADIUS (the swept points' radius at width 1)
constexpr float DEFAULT_NEAR = 0.3f;    // the near end's share when QC's isn't known (a sword's, a gun's: VR_Melee_NearBack)
constexpr float WIDTH_AT = 0.6f;        // the width's handle beside the blade at this share of the line
constexpr int CURVE_STEPS = 24;
constexpr int UNDO_MAX = 64;
constexpr float TICK_UNITS = 1.f;       // a haptic tick each unit a held handle moves

// The weapon hand's blade as laid out now: the hand (the line's start), its way to the tip, its length, the line's normal
// in the blade's plane (P) and the model's +y (Y), as QC VR_Melee_Line and VR_Melee_BendLine have them.
struct Geo
{
    glm::vec3 hand{0.f}, tip{0.f}, a{1.f, 0.f, 0.f}, p{0.f, 0.f, 1.f}, y{0.f, 1.f, 0.f};
    float len{1.f};
    blade::Shape shape;
    float nearBack{DEFAULT_NEAR};
    float radius{MELEE_RADIUS};
    int kind{0};             // its AD kind (the model's curve and head; 0 none)
    bool modelCurve{false};  // the model's own curve shown (no curve of its own)
    bool hasHead{false};
    glm::vec3 headLo{0.f}, headHi{0.f};
};

struct Session
{
    bool active{false};
    int world{0};
    int returnPage{-1};
    int hand{HAND_MAIN}; // the weapon's
    int slot{-1};
    int owner[managedCount]{}; // the slot each managed key is written to (an inheriting weapon's source's)
    Snapshot entry;
    za::Vector<Snapshot> undo;
    float bladesWas{0.f}; // vr_debug_melee_blades on entering
    int grab{-1};         // the handle held (by the other hand)
    glm::vec3 grabOffset{0.f};
    glm::vec3 lastTick{0.f};
    int hover{-1};
    int hoverButton[HAND_COUNT]{-1, -1};
    bool eaten[HAND_COUNT][6]{};
    glm::vec3 panelAt{0.f}, panelRight{0.f, -1.f, 0.f}, panelUp{0.f, 0.f, 1.f}, panelNormal{-1.f, 0.f, 0.f};
    float panelYaw{0.f};
    za::String feedback;
    double feedbackUntil{0.0};
    Geo geo;
    bool geoValid{false};
};
Session ses;

[[nodiscard]] int otherHand()
{
    return ses.hand == HAND_MAIN ? HAND_OFF : HAND_MAIN;
}

[[nodiscard]] cvar_t* var(int i)
{
    return weapons::cvar(ses.owner[i], managedKeys[i]);
}

[[nodiscard]] int indexOf(Key key)
{
    for(int i = 0; i < managedCount; i++)
    {
        if(managedKeys[i] == key)
        {
            return i;
        }
    }
    return -1;
}

void set(Key key, float v)
{
    const int i = indexOf(key);
    cvar_t* c = i >= 0 ? var(i) : nullptr;
    if(c && za::fabs(c->value - v) > 1e-5f)
    {
        Cvar_SetValueQuick(c, roundf(v * 10000.f) / 10000.f);
    }
}

[[nodiscard]] Snapshot take()
{
    Snapshot s;
    for(int i = 0; i < managedCount; i++)
    {
        const cvar_t* c = var(i);
        s.v[i] = c ? c->value : 0.f;
    }
    return s;
}

void restore(const Snapshot& s)
{
    for(int i = 0; i < managedCount; i++)
    {
        set(managedKeys[i], s.v[i]);
    }
}

[[nodiscard]] int changedFrom(const Snapshot& s)
{
    const Snapshot now = take();
    int n = 0;
    for(int i = 0; i < managedCount; i++)
    {
        n += za::fabs(now.v[i] - s.v[i]) > 1e-5f ? 1 : 0;
    }
    return n;
}

void pushUndo()
{
    if(static_cast<int>(ses.undo.size()) >= UNDO_MAX)
    {
        for(int i = 0; i + 1 < UNDO_MAX; i++)
        {
            ses.undo[static_cast<za::SizeT>(i)] = ses.undo[static_cast<za::SizeT>(i + 1)];
        }
        ses.undo.popBack();
    }
    ses.undo.pushBack(take());
}

void say(const za::String& text)
{
    ses.feedback = text;
    ses.feedbackUntil = realtime + 2.5;
    Con_Printf("Edit Blade Shape: %s\n", text.cStr());
}

void haptic(int hand, float seconds, float amplitude)
{
    if(Backend* be = backend(); be && !vr_disablehaptics.value && hand >= 0 && hand < HAND_COUNT)
    {
        be->haptic(hand, seconds, 160.f, amplitude);
    }
}

[[nodiscard]] const char* modelName()
{
    const qmodel_t* m = weapons::heldModel(ses.hand);
    return m ? m->name : "";
}

// The blade now (Geo), from the weapon hand as the client sends it to the server (its place and turn, its muzzle as drawn
// moved with the hand since: vr_client.cpp handMuzzle).
[[nodiscard]] bool layOut(const hands::State& s, Geo& g)
{
    const int h = ses.hand;
    if(!s.valid || !s.muzzleValid[h])
    {
        return false;
    }
    g.hand = s.pos[h];
    g.tip = s.muzzle[h] + (s.pos[h] - s.placedFrom[h]);
    g.len = glm::length(g.tip - g.hand);
    if(g.len < 2.f || g.len > 80.f)
    {
        return false;
    }
    g.a = (g.tip - g.hand) / g.len;
    glm::vec3 f, r, u;
    hands::angleVectors(s.rot[h], f, r, u);
    glm::vec3 y = -r - g.a * glm::dot(-r, g.a);
    if(glm::length(y) < 1e-4f)
    {
        return false;
    }
    g.y = glm::normalize(y);
    g.p = glm::normalize(glm::cross(g.y, g.a));
    g.shape = blade::shape(ses.slot);
    g.kind = admelee::kindOfModel(modelName());
    g.modelCurve = !g.shape.spine && g.kind > 0 && vr_melee_curved_blades.value != 0.f &&
                   admelee::bend(g.kind, 0.5f) != glm::vec2{0.f};
    // The near end: its own, else the kind's as QC laid it last (Show Melee Blades is on), else a sword's.
    g.nearBack = g.shape.nearBack;
    if(g.nearBack < 0.f)
    {
        g.nearBack = DEFAULT_NEAR;
        glm::vec3 n, gr, t;
        if(meleeshow::lastLine(h, n, gr, t) && glm::length(t - gr) > 1.f)
        {
            g.nearBack = glm::length(n - gr) / glm::length(t - gr);
        }
    }
    g.radius = MELEE_RADIUS * za::max(0.25f, vr_melee_range_multiplier.value) * g.shape.width;
    g.hasHead = g.shape.head || (g.kind > 0 && admelee::head(g.kind, g.headLo, g.headHi));
    if(g.shape.head)
    {
        g.headLo = g.shape.headLo;
        g.headHi = g.shape.headHi;
    }
    return true;
}

// The curve's way off the line at share `f` (shares of its length): its own, the model's, or none.
[[nodiscard]] glm::vec2 curveAt(const Geo& g, float f)
{
    if(g.shape.spine)
    {
        return vr_melee_curved_blades.value != 0.f ? blade::spineAt(g.shape, f) : glm::vec2{0.f};
    }
    return g.modelCurve && f > 0.f ? admelee::bend(g.kind, f) : glm::vec2{0.f};
}

[[nodiscard]] glm::vec3 pointAt(const Geo& g, float f)
{
    const glm::vec2 k = curveAt(g, f);
    return g.hand + g.a * (g.len * f) + (g.p * k.x + g.y * k.y) * g.len;
}

[[nodiscard]] glm::vec3 boxPoint(const Geo& g, const glm::vec3& s)
{
    return g.hand + (g.a * s.x + g.p * s.y + g.y * s.z) * g.len;
}

[[nodiscard]] bool shown(const Geo& g, int h)
{
    return h >= 0 && h < HCount && (h < HHeadLo || g.hasHead);
}

[[nodiscard]] glm::vec3 handleAt(const Geo& g, int h)
{
    switch(h)
    {
        case HTip: return g.tip;
        case HNear: return g.hand - g.a * (g.len * g.nearBack);
        case HWidth: return pointAt(g, WIDTH_AT) + g.p * g.radius;
        case HHeadLo: return boxPoint(g, g.headLo);
        case HHeadHi: return boxPoint(g, g.headHi);
        default: return pointAt(g, blade::spineShare(h - HSpine1));
    }
}

[[nodiscard]] const char* handleName(int h)
{
    switch(h)
    {
        case HTip: return "Tip";
        case HNear: return "Near End";
        case HWidth: return "Width";
        case HHeadLo: return "Head Corner";
        case HHeadHi: return "Head Corner";
        default: return "Curve";
    }
}

[[nodiscard]] glm::vec4 handleColour(int h)
{
    switch(h)
    {
        case HTip: return {1.f, 0.3f, 0.25f, 1.f};
        case HNear: return {1.f, 0.6f, 0.15f, 1.f};
        case HWidth: return {0.3f, 0.85f, 1.f, 1.f};
        case HHeadLo:
        case HHeadHi: return {1.f, 0.35f, 1.f, 1.f};
        default: return {0.35f, 1.f, 0.45f, 1.f};
    }
}

// The curve of its own, seeded (the first drag of a curve point) from the one shown: the model's, or straight.
void ensureSpine(const Geo& g)
{
    if(g.shape.spine)
    {
        return;
    }
    for(int i = 0; i < blade::spinePoints; i++)
    {
        const glm::vec2 k = curveAt(g, blade::spineShare(i));
        set(spineKeys[i][0], k.x);
        set(spineKeys[i][1], k.y);
    }
    set(Key::BladeSpine, 1.f);
}

// A head's box of its own, seeded from the model's (an AD hammer's or mace's) or a small box at the far end.
void ensureHead(const Geo& g)
{
    if(g.shape.head)
    {
        return;
    }
    const glm::vec3 lo = g.hasHead ? g.headLo : glm::vec3{0.8f, -0.1f, -0.06f};
    const glm::vec3 hi = g.hasHead ? g.headHi : glm::vec3{1.f, 0.1f, 0.06f};
    set(Key::BladeHeadLoX, lo.x);
    set(Key::BladeHeadLoY, lo.y);
    set(Key::BladeHeadLoZ, lo.z);
    set(Key::BladeHeadHiX, hi.x);
    set(Key::BladeHeadHiY, hi.y);
    set(Key::BladeHeadHiZ, hi.z);
    set(Key::BladeHead, 1.f);
}

// The held handle to `t` (the world).
void drag(const Geo& g, int h, const glm::vec3& t)
{
    switch(h)
    {
        case HTip:
        {
            // The tip's offset solved through the drawn weapon: the muzzle's move per unit of each offset axis (the
            // offset's frame is the model's, scaled and mirrored as drawn).
            const glm::vec3 now = weapons::vec(ses.slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ);
            glm::vec3 base, col[3];
            if(!view::muzzleWithOffset(ses.hand, now, base))
            {
                return;
            }
            for(int i = 0; i < 3; i++)
            {
                glm::vec3 e{0.f};
                e[i] = 1.f;
                glm::vec3 m;
                if(!view::muzzleWithOffset(ses.hand, now + e, m))
                {
                    return;
                }
                col[i] = m - base;
            }
            const glm::mat3 j{col[0], col[1], col[2]};
            if(za::fabs(glm::determinant(j)) < 1e-6f)
            {
                return;
            }
            const glm::vec3 next = now + glm::inverse(j) * (t - g.tip);
            set(Key::MuzzleOffsetX, za::clamp(next.x, -80.f, 80.f));
            set(Key::MuzzleOffsetY, za::clamp(next.y, -80.f, 80.f));
            set(Key::MuzzleOffsetZ, za::clamp(next.z, -80.f, 80.f));
            break;
        }
        case HNear: set(Key::BladeNear, za::clamp(-glm::dot(t - g.hand, g.a) / g.len, 0.f, 2.f)); break;
        case HWidth:
        {
            const float base = MELEE_RADIUS * za::max(0.25f, vr_melee_range_multiplier.value);
            set(Key::BladeWidth, za::clamp(glm::length(t - pointAt(g, WIDTH_AT)) / base, 0.2f, 5.f));
            break;
        }
        case HHeadLo:
        case HHeadHi:
        {
            ensureHead(g);
            const glm::vec3 d = (t - g.hand) / g.len;
            const glm::vec3 s{glm::dot(d, g.a), glm::dot(d, g.p), glm::dot(d, g.y)};
            const bool lo = h == HHeadLo;
            set(lo ? Key::BladeHeadLoX : Key::BladeHeadHiX, s.x);
            set(lo ? Key::BladeHeadLoY : Key::BladeHeadHiY, s.y);
            set(lo ? Key::BladeHeadLoZ : Key::BladeHeadHiZ, s.z);
            break;
        }
        default:
        {
            ensureSpine(g);
            const int i = h - HSpine1;
            const glm::vec3 d = t - (g.hand + g.a * (g.len * blade::spineShare(i)));
            set(spineKeys[i][0], za::clamp(glm::dot(d, g.p) / g.len, -0.6f, 0.6f));
            set(spineKeys[i][1], za::clamp(glm::dot(d, g.y) / g.len, -0.6f, 0.6f));
            break;
        }
    }
}

// The handle's value, for its label.
[[nodiscard]] const char* valueText(const Geo& g, int h) // (va's)
{
    switch(h)
    {
        case HTip: return va("%.1f units", static_cast<double>(g.len));
        case HNear: return va("%.2f%s", static_cast<double>(g.nearBack), g.shape.nearBack < 0.f ? " (kind's)" : "");
        case HWidth: return va("%.2fx (%.1f units)", static_cast<double>(g.shape.width), static_cast<double>(g.radius));
        case HHeadLo:
        case HHeadHi:
        {
            const glm::vec3& s = h == HHeadLo ? g.headLo : g.headHi;
            return va("%.2f %.2f %.2f%s", static_cast<double>(s.x), static_cast<double>(s.y), static_cast<double>(s.z),
                g.shape.head ? "" : " (model's)");
        }
        default:
        {
            const glm::vec2 k = curveAt(g, blade::spineShare(h - HSpine1));
            return va("%d: %+.3f %+.3f%s", h - HSpine1 + 1, static_cast<double>(k.x), static_cast<double>(k.y),
                g.shape.spine ? "" : g.modelCurve ? " (model's)" : " (straight)");
        }
    }
}

void grab(int hand, int h)
{
    if(!ses.geoValid || !shown(ses.geo, h))
    {
        return;
    }
    pushUndo();
    ses.grab = h;
    ses.grabOffset = handleAt(ses.geo, h) - hands::current().pos[hand];
    ses.lastTick = handleAt(ses.geo, h);
    haptic(hand, 0.03f, 0.6f);
}

void release()
{
    if(ses.grab < 0)
    {
        return;
    }
    ses.grab = -1;
    haptic(otherHand(), 0.02f, 0.3f);
    if(!ses.undo.empty() && changedFrom(ses.undo.back()) == 0)
    {
        ses.undo.popBack(); // (nothing moved)
    }
}

void undo()
{
    if(ses.grab >= 0)
    {
        return; // (let go first)
    }
    if(ses.undo.empty())
    {
        say("nothing to undo");
        return;
    }
    restore(ses.undo.back());
    ses.undo.popBack();
    say("undone");
}

// The blade's keys back to the shipped defaults (the tip's offset too).
void resetBlade()
{
    pushUndo();
    for(int i = 0; i < managedCount; i++)
    {
        if(cvar_t* c = var(i); c && c->default_string)
        {
            set(managedKeys[i], static_cast<float>(Q_atof(c->default_string)));
        }
    }
    say("the blade's shape and tip back to their defaults");
}

void straighten()
{
    pushUndo();
    for(const auto& k : spineKeys)
    {
        set(k[0], 0.f);
        set(k[1], 0.f);
    }
    set(Key::BladeSpine, 1.f);
    say("straight (Reset Blade: the model's own curve again)");
}

void toggleHead()
{
    if(!ses.geoValid)
    {
        return;
    }
    pushUndo();
    if(ses.geo.shape.head)
    {
        set(Key::BladeHead, 0.f);
        say("no head box of its own (the model's, if it has one)");
    }
    else
    {
        ensureHead(ses.geo);
        say("a head box of its own: drag its two corners");
    }
}

void press(int b)
{
    switch(b)
    {
        case BtnUndo: undo(); break;
        case BtnReset: resetBlade(); break;
        case BtnStraighten: straighten(); break;
        case BtnHead: toggleHead(); break;
        case BtnCancel: finish(false); break;
        case BtnDone: finish(true); break;
        default: break;
    }
}

[[nodiscard]] const char* buttonLabel(int b)
{
    switch(b)
    {
        case BtnUndo: return "Undo";
        case BtnReset: return "Reset Blade";
        case BtnStraighten: return "Straighten";
        case BtnHead: return ses.geoValid && ses.geo.shape.head ? "Head Box: On" : "Head Box: Off";
        case BtnCancel: return "Cancel";
        default: return "Done";
    }
}

[[nodiscard]] glm::vec3 buttonAt(int b)
{
    const float m2u = units::metresToUnits();
    const float col = static_cast<float>(b % 3 - 1), row = static_cast<float>(b / 3);
    return ses.panelAt + ses.panelRight * (col * (BUTTON_WIDTH + BUTTON_GAP) * m2u) +
           ses.panelUp * ((0.5f - row) * (BUTTON_HEIGHT + BUTTON_GAP) * m2u);
}

[[nodiscard]] int buttonAtHand(const glm::vec3& p)
{
    const float m2u = units::metresToUnits();
    for(int b = 0; b < BtnCount; b++)
    {
        const glm::vec3 d = p - buttonAt(b);
        if(za::fabs(glm::dot(d, ses.panelRight)) <= (BUTTON_WIDTH * 0.5f + 0.01f) * m2u &&
            za::fabs(glm::dot(d, ses.panelUp)) <= (BUTTON_HEIGHT * 0.5f + 0.01f) * m2u &&
            za::fabs(glm::dot(d, ses.panelNormal)) <= BUTTON_DEPTH * m2u)
        {
            return b;
        }
    }
    return -1;
}

// The other hand's handle (the nearest within HANDLE_REACH) and each hand's panel button; a tick on reaching a new one.
void hoverFrame(const hands::State& s)
{
    const int other = otherHand();
    int best = -1;
    if(ses.geoValid)
    {
        float bestD = HANDLE_REACH * units::metresToUnits();
        for(int h = 0; h < HCount; h++)
        {
            if(!shown(ses.geo, h))
            {
                continue;
            }
            const float d = glm::length(handleAt(ses.geo, h) - s.pos[other]);
            if(d <= bestD)
            {
                best = h;
                bestD = d;
            }
        }
    }
    if(ses.grab >= 0)
    {
        best = ses.grab;
    }
    if(best != ses.hover && best >= 0)
    {
        haptic(other, 0.015f, 0.25f);
    }
    ses.hover = best;
    for(int h = 0; h < HAND_COUNT; h++)
    {
        const int b = buttonAtHand(s.pos[h]);
        if(b != ses.hoverButton[h] && b >= 0)
        {
            haptic(h, 0.015f, 0.25f);
        }
        ses.hoverButton[h] = b;
    }
}

void editFrame(const hands::State& s)
{
    if(ses.grab < 0 || !ses.geoValid)
    {
        return;
    }
    const glm::vec3 t = s.pos[otherHand()] + ses.grabOffset;
    drag(ses.geo, ses.grab, t);
    if(glm::length(t - ses.lastTick) >= TICK_UNITS)
    {
        ses.lastTick = t;
        haptic(otherHand(), 0.01f, 0.2f);
    }
    ses.geoValid = layOut(s, ses.geo); // (drawn as it is now)
}

void drawBlade(const Geo& g)
{
    const glm::vec4 bladeColour{0.9f, 0.95f, 1.f, 0.9f};
    const glm::vec4 tube{0.3f, 0.85f, 1.f, 0.35f};
    // From the near end straight to the hand, then along the curve to the tip; its width as four lines beside it.
    lines::line(handleAt(g, HNear), g.hand, 0.18f, glm::vec4{1.f, 0.6f, 0.15f, 0.8f}, bladeColour);
    glm::vec3 prev = g.hand;
    for(int i = 1; i <= CURVE_STEPS; i++)
    {
        const float f = static_cast<float>(i) / CURVE_STEPS;
        const glm::vec3 at = pointAt(g, f);
        lines::line(prev, at, 0.22f, bladeColour, bladeColour);
        const float f0 = static_cast<float>(i - 1) / CURVE_STEPS;
        if(f0 >= 0.2f)
        {
            for(const glm::vec3& side : {g.p, -g.p, g.y, -g.y})
            {
                lines::line(prev + side * g.radius, at + side * g.radius, 0.08f, tube, tube);
            }
        }
        prev = at;
    }
    // The curve's points off the line: a thin tie from the line to each.
    for(int i = 0; i < blade::spinePoints; i++)
    {
        const float f = blade::spineShare(i);
        lines::line(g.hand + g.a * (g.len * f), pointAt(g, f), 0.06f, glm::vec4{0.35f, 1.f, 0.45f, 0.4f},
            glm::vec4{0.35f, 1.f, 0.45f, 0.4f});
    }
    if(g.hasHead)
    {
        glm::vec3 c[8];
        for(int i = 0; i < 8; i++)
        {
            c[i] = boxPoint(g, {(i & 1) ? g.headHi.x : g.headLo.x, (i & 2) ? g.headHi.y : g.headLo.y,
                                   (i & 4) ? g.headHi.z : g.headLo.z});
        }
        const glm::vec4 col{1.f, 0.35f, 1.f, 0.7f};
        for(int i = 0; i < 8; i++)
        {
            for(int bit = 1; bit < 8; bit <<= 1)
            {
                if(!(i & bit))
                {
                    lines::line(c[i], c[i | bit], 0.12f, col, col);
                }
            }
        }
    }
    const hands::State& s = hands::current();
    for(int h = 0; h < HCount; h++)
    {
        if(!shown(g, h))
        {
            continue;
        }
        const bool held = ses.grab == h, hovered = ses.hover == h;
        const glm::vec4 col = held ? glm::vec4{0.2f, 1.f, 0.35f, 1.f} : hovered ? glm::vec4{1.f, 0.9f, 0.2f, 1.f} : handleColour(h);
        const glm::vec3 at = handleAt(g, h);
        (held || hovered ? lines::glowPoint : lines::point)(at, held || hovered ? 1.1f : 0.75f, col);
        if(held || hovered)
        {
            const za::String text = za::String{handleName(h)} + " " + valueText(g, h);
            text3d::queueOverlay(text, at + glm::vec3{0.f, 0.f, 1.5f}, glm::vec3{0.f, s.headAngles.y, 0.f}, 0.04f);
        }
    }
    lines::point(s.pos[otherHand()], ses.grab >= 0 ? 0.5f : 0.35f,
        ses.grab >= 0 ? glm::vec4{0.2f, 1.f, 0.35f, 1.f} : glm::vec4{1.f, 1.f, 1.f, 0.8f});
}

void drawPanel()
{
    const float m2u = units::metresToUnits();
    const glm::vec3 angles{0.f, ses.panelYaw, 0.f};
    for(int b = 0; b < BtnCount; b++)
    {
        const glm::vec3 at = buttonAt(b);
        const bool hovered = ses.hoverButton[HAND_OFF] == b || ses.hoverButton[HAND_MAIN] == b;
        const glm::vec4 color = hovered ? glm::vec4{1.f, 0.85f, 0.15f, 1.f} : glm::vec4{0.5f, 0.7f, 1.f, 0.8f};
        const glm::vec3 r = ses.panelRight * (BUTTON_WIDTH * 0.5f * m2u), u = ses.panelUp * (BUTTON_HEIGHT * 0.5f * m2u);
        const glm::vec3 back = at - ses.panelNormal * 0.15f;
        const glm::vec4 face = hovered ? glm::vec4{0.35f, 0.3f, 0.05f, 0.85f} : glm::vec4{0.04f, 0.06f, 0.1f, 0.8f};
        lines::triangle(back - r - u, back + r - u, back + r + u, face);
        lines::triangle(back - r - u, back + r + u, back - r + u, face);
        const glm::vec3 c[4] = {at - r - u, at + r - u, at + r + u, at - r + u};
        for(int i = 0; i < 4; i++)
        {
            (hovered ? lines::glow : lines::line)(c[i], c[(i + 1) % 4], hovered ? 0.25f : 0.12f, color, color);
        }
        constexpr float chars = 13.f; // ("Head Box: Off")
        const float scale = za::min(BUTTON_WIDTH * 0.85f * m2u / (8.f * chars), BUTTON_HEIGHT * 0.55f * m2u / 8.f);
        text3d::queue(buttonLabel(b), at, angles, text3d::Align::Centre, scale);
    }
    za::String text = va("%c%c%c%c%c %c%c%c%c%c", 'B' | 0x80, 'L' | 0x80, 'A' | 0x80, 'D' | 0x80, 'E' | 0x80, 'S' | 0x80,
        'H' | 0x80, 'A' | 0x80, 'P' | 0x80, 'E' | 0x80);
    const cvar_t* id = weapons::cvar(ses.slot, Key::ID);
    text += va(" (slot %d: %s)", ses.slot + 1, id ? COM_SkipPath(id->string) : "?");
    text += "\nother hand's grip on a dot: drag it";
    text += "\nred tip, orange near end, blue width,";
    text += "\ngreen curve, pink head corners";
    text += "\nbutton: touch it, pull the trigger";
    text += "\nB/Y: undo     menu button: done";
    int lineCount = 6;
    if(realtime < ses.feedbackUntil)
    {
        text += "\n" + ses.feedback;
        lineCount++;
    }
    constexpr float HINT_SCALE = 0.05f;
    const float half = 8.f * HINT_SCALE * (static_cast<float>(lineCount) * 0.5f + 0.4f);
    const float top = (BUTTON_HEIGHT * 1.5f + BUTTON_GAP * 0.5f + 0.02f) * m2u;
    text3d::queueOverlay(text, ses.panelAt + ses.panelUp * (top + half), angles, HINT_SCALE);
}

void placePanel(const hands::State& s)
{
    const float m2u = units::metresToUnits();
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, s.headAngles.y, 0.f}, fwd, right, up);
    ses.panelAt = s.head + fwd * (PANEL_DISTANCE * m2u) - up * (PANEL_DROP * m2u);
    ses.panelRight = right;
    ses.panelUp = up;
    ses.panelNormal = -fwd;
    ses.panelYaw = s.headAngles.y;
}

// vr_bladeedit [main|off]: enters it on that hand's weapon (default: the main hand's).
void start_f()
{
    const bool off = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "off");
    start(off ? HAND_OFF : HAND_MAIN, -1);
}

void done_f()
{
    finish(true);
}

void cancel_f()
{
    finish(false);
}

void undo_f()
{
    undo();
}

void reset_f()
{
    if(ses.active)
    {
        resetBlade();
    }
}

// vr_bladeedit_press <index>: presses panel button `index` (as a hand's trigger there).
void press_f()
{
    if(ses.active && Cmd_Argc() > 1)
    {
        press(Q_atoi(Cmd_Argv(1)));
    }
}

// vr_bladeedit_report: whether it runs, the slot, the handle held, each handle's place (units from the hand along the
// line, its normal P and the model's +y Y) and the curve's point at 0.5 in the world (QC's VR_Melee_At(0.5) is there).
void report_f()
{
    Con_Printf("bladeedit active %d slot %d hand %d grab %d hover %d undo %d\n", ses.active ? 1 : 0, ses.slot + 1, ses.hand,
        ses.grab, ses.hover, static_cast<int>(ses.undo.size()));
    if(!ses.active || !ses.geoValid)
    {
        return;
    }
    const Geo& g = ses.geo;
    Con_Printf("bladeedit line %.2f near %.3f radius %.2f spine %d curve %d head %d\n", static_cast<double>(g.len),
        static_cast<double>(g.nearBack), static_cast<double>(g.radius), g.shape.spine ? 1 : 0, g.modelCurve ? 1 : 0,
        g.hasHead ? 1 : 0);
    for(int h = 0; h < HCount; h++)
    {
        if(!shown(g, h))
        {
            continue;
        }
        const glm::vec3 d = handleAt(g, h) - g.hand;
        Con_Printf("bladeedit handle %d %s at %.2f %.2f %.2f: %s\n", h, handleName(h), static_cast<double>(glm::dot(d, g.a)),
            static_cast<double>(glm::dot(d, g.p)), static_cast<double>(glm::dot(d, g.y)), valueText(g, h));
    }
    const glm::vec3 o = hands::current().pos[otherHand()] - g.hand, push = handpose::wallPush(otherHand());
    Con_Printf("bladeedit other hand at %.2f %.2f %.2f, pushed %.2f %.2f %.2f\n", static_cast<double>(glm::dot(o, g.a)),
        static_cast<double>(glm::dot(o, g.p)), static_cast<double>(glm::dot(o, g.y)), static_cast<double>(push.x),
        static_cast<double>(push.y), static_cast<double>(push.z));
    const glm::vec3 mid = pointAt(g, 0.5f);
    Con_Printf("bladeedit curve 0.5 world %.2f %.2f %.2f\n", static_cast<double>(mid.x), static_cast<double>(mid.y),
        static_cast<double>(mid.z));
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_bladeedit", start_f);
    Cmd_AddCommand("vr_bladeedit_done", done_f);
    Cmd_AddCommand("vr_bladeedit_cancel", cancel_f);
    Cmd_AddCommand("vr_bladeedit_undo", undo_f);
    Cmd_AddCommand("vr_bladeedit_reset", reset_f);
    Cmd_AddCommand("vr_bladeedit_press", press_f);
    Cmd_AddCommand("vr_bladeedit_report", report_f);
}

bool active()
{
    return ses.active && ses.world == worldGeneration();
}

bool start(int hand, int returnPage)
{
    const auto refuse = [](const char* why) {
        Con_Printf("Edit Blade Shape: %s\n", why);
        return false;
    };
    if(ses.active)
    {
        return false;
    }
    if(!vrActive())
    {
        return refuse("it needs the headset (or vr_backend mock)");
    }
    if(cls.state != ca_connected || !cl.worldmodel || !hands::current().valid)
    {
        return refuse("start a map first");
    }
    if(cl.intermission || cl.stats[STAT_HEALTH] <= 0)
    {
        return refuse("not now");
    }
    const int slot = weapons::heldSlot(hand);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return refuse(hand == HAND_MAIN ? "hold a weapon in the main hand" : "hold a weapon in the off hand");
    }
    if(twohand::carrying(hand))
    {
        return refuse("hold the weapon by its handle");
    }
    if(posing::active())
    {
        posing::stop(false);
    }
    if(holstersetup::active())
    {
        holstersetup::finish(true);
    }
    if(key_dest == key_menu)
    {
        menuui::backToGame(HAND_MAIN);
    }
    Cbuf_AddText("-attack\n-offhandattack\n");

    ses.active = true;
    ses.world = worldGeneration();
    ses.returnPage = returnPage;
    ses.hand = hand;
    ses.slot = slot;
    for(int i = 0; i < managedCount; i++)
    {
        ses.owner[i] = weapons::ownerSlot(slot, managedKeys[i]);
    }
    ses.entry = take();
    ses.undo.clear();
    ses.grab = -1;
    ses.hover = -1;
    for(int h = 0; h < HAND_COUNT; h++)
    {
        ses.hoverButton[h] = -1;
    }
    memset(ses.eaten, 0, sizeof(ses.eaten));
    ses.bladesWas = vr_debug_melee_blades.value;
    Cvar_SetValueQuick(&vr_debug_melee_blades, 1.f);
    placePanel(hands::current());
    ses.geoValid = layOut(hands::current(), ses.geo);
    const cvar_t* id = weapons::cvar(slot, Key::ID);
    say(va("started on %s (slot %d)", id ? id->string : "?", slot + 1));
    S_LocalSound("misc/menu3.wav");
    return true;
}

void finish(bool keep)
{
    if(!ses.active)
    {
        return;
    }
    ses.grab = -1;
    if(!keep)
    {
        restore(ses.entry);
    }
    const int changed = changedFrom(ses.entry);
    const int page = ses.returnPage;
    ses.active = false;
    ses.undo.clear();
    Cvar_SetValueQuick(&vr_debug_melee_blades, ses.bladesWas);
    Con_Printf("Edit Blade Shape: %s (%d settings changed)\n", keep ? "done" : "cancelled, everything put back", changed);
    if(keep && changed > 0)
    {
        blade::dump(ses.slot);
    }
    S_LocalSound(keep ? "misc/menu2.wav" : "misc/menu1.wav");
    if(page >= 0)
    {
        menu::reopen(page);
    }
}

bool button(int hand, posing::Button b, bool down)
{
    const int i = static_cast<int>(b);
    if(hand < 0 || hand >= HAND_COUNT || i < 0 || i >= 6)
    {
        return false;
    }
    if(!down)
    {
        const bool mine = ses.eaten[hand][i];
        ses.eaten[hand][i] = false;
        if(mine && b == posing::Button::Grip && hand == otherHand())
        {
            release();
        }
        // The weapon hand's grip held since before keeps the game's hold (its release kept from it).
        return mine || (active() && b == posing::Button::Grip);
    }
    if(!active() || key_dest != key_game)
    {
        return false;
    }
    ses.eaten[hand][i] = true;
    switch(b)
    {
        case posing::Button::Grip:
            if(hand == otherHand() && ses.hover >= 0)
            {
                grab(hand, ses.hover);
            }
            break;
        case posing::Button::Trigger:
            if(ses.hoverButton[hand] >= 0)
            {
                haptic(hand, 0.03f, 0.5f);
                press(ses.hoverButton[hand]);
            }
            break;
        case posing::Button::Secondary: undo(); break;
        case posing::Button::Menu: finish(true); break;
        default: break; // (A/X, the stick's press: nothing)
    }
    return true;
}

void sticks(const glm::vec2& off, const glm::vec2& main)
{
    (void)off;
    (void)main;
}

void frame()
{
    if(!ses.active)
    {
        return;
    }
    // Leaving with the map, the game, the headset, the menu opened, the weapon gone from the hand (the changes kept).
    if(ses.world != worldGeneration() || cls.state != ca_connected || !vrActive() || key_dest == key_menu ||
        weapons::heldSlot(ses.hand) != ses.slot)
    {
        ses.returnPage = -1;
        finish(true);
        return;
    }
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return;
    }
    ses.geoValid = layOut(s, ses.geo);
    hoverFrame(s);
    editFrame(s);
    if(ses.geoValid)
    {
        drawBlade(ses.geo);
    }
    drawPanel();
}

bool handlePoint(int index, const glm::vec3& offset, glm::vec3& out)
{
    if(!active() || !ses.geoValid || !shown(ses.geo, index))
    {
        return false;
    }
    const Geo& g = ses.geo;
    out = handleAt(g, index) + g.a * offset.x + g.p * offset.y + g.y * offset.z;
    return true;
}

bool buttonPoint(int index, glm::vec3& out)
{
    if(!active() || index < 0 || index >= BtnCount)
    {
        return false;
    }
    out = buttonAt(index);
    return true;
}

} // namespace qvr::bladeedit
