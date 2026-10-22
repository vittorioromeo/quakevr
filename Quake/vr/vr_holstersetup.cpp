// vr_holstersetup.cpp -- see vr_holstersetup.hpp.

#include "vr_holstersetup.hpp"
#include "vr_backend.hpp"
#include "vr_body.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_holsterzones.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_menuui.hpp"
#include "vr_seated.hpp"
#include "vr_text3d.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <math.h>
#include <string.h>

namespace qvr::holstersetup
{
namespace
{

using zones::ZoneCount;

// Every setting it can change (Undo's and Cancel's snapshot): the pairs' offsets standing and seated, the pouches' (likewise),
// each holster's trim, each zone's size.
cvar_t* const managedCvars[] = {&vr_hip_offset_x, &vr_hip_offset_y, &vr_hip_offset_z, &vr_upper_holster_offset_x,
    &vr_upper_holster_offset_y, &vr_upper_holster_offset_z, &vr_shoulder_holster_offset_x, &vr_shoulder_holster_offset_y,
    &vr_shoulder_holster_offset_z, &vr_hip_holster_seated_x, &vr_hip_holster_seated_y, &vr_hip_holster_seated_z,
    &vr_upper_holster_seated_x, &vr_upper_holster_seated_y, &vr_upper_holster_seated_z, &vr_shoulder_holster_seated_x,
    &vr_shoulder_holster_seated_y, &vr_shoulder_holster_seated_z, &vr_ammo_pouch_x, &vr_ammo_pouch_y, &vr_ammo_pouch_z,
    &vr_ammo_pouch_seated_x, &vr_ammo_pouch_seated_y, &vr_ammo_pouch_seated_z, &vr_grenade_pouch_x, &vr_grenade_pouch_y,
    &vr_grenade_pouch_z, &vr_grenade_pouch_seated_x, &vr_grenade_pouch_seated_y, &vr_grenade_pouch_seated_z,
    &vr_holster_trim_lshoulder_x, &vr_holster_trim_lshoulder_y, &vr_holster_trim_lshoulder_z, &vr_holster_trim_rshoulder_x,
    &vr_holster_trim_rshoulder_y, &vr_holster_trim_rshoulder_z, &vr_holster_trim_lhip_x, &vr_holster_trim_lhip_y,
    &vr_holster_trim_lhip_z, &vr_holster_trim_rhip_x, &vr_holster_trim_rhip_y, &vr_holster_trim_rhip_z,
    &vr_holster_trim_lupper_x, &vr_holster_trim_lupper_y, &vr_holster_trim_lupper_z, &vr_holster_trim_rupper_x,
    &vr_holster_trim_rupper_y, &vr_holster_trim_rupper_z, &vr_holster_size_lshoulder, &vr_holster_size_rshoulder,
    &vr_holster_size_lhip, &vr_holster_size_rhip, &vr_holster_size_lupper, &vr_holster_size_rupper,
    &vr_holster_size_ammopouch, &vr_holster_size_grenadepouch};
constexpr int managedCount = static_cast<int>(sizeof(managedCvars) / sizeof(managedCvars[0]));

struct Snapshot
{
    float v[managedCount]{};
};

// The three settings a drag moves (forward, outwards or right, up: as the sliders).
struct Params
{
    cvar_t* c[3]{};
};

// The panel's buttons, in order (vr_mock_hand_to <hand> setupbutton <index>).
enum ButtonId : int
{
    BtnSymmetry,
    BtnUndo,
    BtnResetOne,
    BtnResetAll,
    BtnCancel,
    BtnDone,
    BtnCount
};

constexpr float PANEL_DISTANCE = 0.55f;  // metres ahead of the eyes
constexpr float PANEL_DROP = 0.1f;       // metres below them
constexpr float BUTTON_WIDTH = 0.17f;    // metres
constexpr float BUTTON_HEIGHT = 0.055f;
constexpr float BUTTON_GAP = 0.015f;
constexpr float BUTTON_DEPTH = 0.08f;    // a hand this atShipped the panel's plane (metres) is at a button
constexpr float HOVER_RATIO = 1.2f;      // a hand this deep in a zone's box (1: its surface) is at it
constexpr float DEAD_ZONE = 0.75f;       // units the hands move before a held holster follows (no accidental drags)
constexpr float SNAP_RADIUS = 0.6f;      // units: a holster this atShipped its shipped place goes to it
constexpr float SETUP_SIZE_MIN = 0.5f;
constexpr float SETUP_SIZE_MAX = 2.5f;
constexpr float PARAM_LIMIT = 60.f;      // units either way (or the value on entering, if beyond)
constexpr int UNDO_MAX = 64;

// The holster or pouch held, by one hand or two (a bit each: 1 << hand).
struct Edit
{
    int zone{-1};
    int hands{0};
    bool live{false};      // the hands moved past DEAD_ZONE: it follows
    bool changed{false};
    bool snapped{false};   // at its shipped place (SNAP_RADIUS)
    glm::vec3 grabMid{0.f}; // the hands' middle when they took hold (or one let go)
    glm::vec3 pivot0{0.f};  // the zone's reach point then
    glm::vec3 params0{0.f};
    glm::vec3 params{0.f};  // the solver's (unrounded)
    glm::mat3 solve{1.f};   // a move of the reach point to the settings' (damped least squares of the probed Jacobian)
    float spread0{1.f};
    float size0{1.f};
    glm::vec3 lastTick{0.f}; // the reach point at the last haptic tick
};

struct Session
{
    bool active{false};
    int world{0};
    int returnPage{-1};
    Snapshot entry;
    za::Vector<Snapshot> undo;
    Edit edit;
    int lastZone{-1};                    // the holster Reset Holster resets (the last held or hovered)
    int hover[HAND_COUNT]{-1, -1};        // the zone each hand is at
    int hoverButton[HAND_COUNT]{-1, -1};  // the panel button each hand is at
    bool eaten[HAND_COUNT][6]{};
    glm::vec2 stick[HAND_COUNT]{};
    // The panel, placed on entering: its middle, its axes (right, up; facing the eyes) and the eyes' yaw then.
    glm::vec3 panelAt{0.f}, panelRight{0.f, -1.f, 0.f}, panelUp{0.f, 0.f, 1.f}, panelNormal{-1.f, 0.f, 0.f};
    float panelYaw{0.f};
    za::String feedback;
    double feedbackUntil{0.0};
};
Session ses;

[[nodiscard]] float defaultOf(const cvar_t& c)
{
    return c.default_string ? static_cast<float>(Q_atof(c.default_string)) : 0.f;
}

void setIfChanged(cvar_t* c, float v)
{
    if(za::fabs(c->value - v) > 1e-5f)
    {
        Cvar_SetValueQuick(c, v);
    }
}

[[nodiscard]] Snapshot take()
{
    Snapshot s;
    for(int i = 0; i < managedCount; i++)
    {
        s.v[i] = managedCvars[i]->value;
    }
    return s;
}

void restore(const Snapshot& s)
{
    for(int i = 0; i < managedCount; i++)
    {
        setIfChanged(managedCvars[i], s.v[i]);
    }
}

[[nodiscard]] bool same(const Snapshot& a, const Snapshot& b)
{
    for(int i = 0; i < managedCount; i++)
    {
        if(za::fabs(a.v[i] - b.v[i]) > 1e-5f)
        {
            return false;
        }
    }
    return true;
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
    Con_Printf("Holster Setup: %s\n", text.cStr());
}

void haptic(int hand, float seconds, float amplitude)
{
    if(Backend* be = backend(); be && !vr_disablehaptics.value && hand >= 0 && hand < HAND_COUNT)
    {
        be->haptic(hand, seconds, 160.f, amplitude);
    }
}

[[nodiscard]] bool isHolster(int zone)
{
    return zone >= 0 && zone < body::HolsterCount;
}

[[nodiscard]] int pairOf(int zone)
{
    return isHolster(zone) ? zone ^ 1 : -1; // (body::Holster: left, right of each kind)
}

[[nodiscard]] const char* label(int zone)
{
    switch(zone)
    {
        case body::LeftShoulder: return "Left Back";
        case body::RightShoulder: return "Right Back";
        case body::LeftHip: return "Left Hip";
        case body::RightHip: return "Right Hip";
        case body::LeftUpper: return "Left Chest";
        case body::RightUpper: return "Right Chest";
        case zones::ZoneAmmoPouch: return "Ammo Pouch";
        default: return "Grenade Pouch";
    }
}

[[nodiscard]] bool symmetry()
{
    return vr_holstersetup_symmetry.value != 0.f;
}

// The pair's offsets (standing or seated) for a holster's kind.
[[nodiscard]] Params pairParams(int zone, bool seated)
{
    switch(zone)
    {
        case body::LeftHip:
        case body::RightHip:
            return seated ? Params{{&vr_hip_holster_seated_x, &vr_hip_holster_seated_y, &vr_hip_holster_seated_z}}
                          : Params{{&vr_hip_offset_x, &vr_hip_offset_y, &vr_hip_offset_z}};
        case body::LeftUpper:
        case body::RightUpper:
            return seated ? Params{{&vr_upper_holster_seated_x, &vr_upper_holster_seated_y, &vr_upper_holster_seated_z}}
                          : Params{{&vr_upper_holster_offset_x, &vr_upper_holster_offset_y, &vr_upper_holster_offset_z}};
        default:
            return seated
                       ? Params{{&vr_shoulder_holster_seated_x, &vr_shoulder_holster_seated_y, &vr_shoulder_holster_seated_z}}
                       : Params{{&vr_shoulder_holster_offset_x, &vr_shoulder_holster_offset_y,
                             &vr_shoulder_holster_offset_z}};
    }
}

[[nodiscard]] Params trimParams(int zone)
{
    switch(zone)
    {
        case body::LeftShoulder:
            return {{&vr_holster_trim_lshoulder_x, &vr_holster_trim_lshoulder_y, &vr_holster_trim_lshoulder_z}};
        case body::RightShoulder:
            return {{&vr_holster_trim_rshoulder_x, &vr_holster_trim_rshoulder_y, &vr_holster_trim_rshoulder_z}};
        case body::LeftHip: return {{&vr_holster_trim_lhip_x, &vr_holster_trim_lhip_y, &vr_holster_trim_lhip_z}};
        case body::RightHip: return {{&vr_holster_trim_rhip_x, &vr_holster_trim_rhip_y, &vr_holster_trim_rhip_z}};
        case body::LeftUpper: return {{&vr_holster_trim_lupper_x, &vr_holster_trim_lupper_y, &vr_holster_trim_lupper_z}};
        default: return {{&vr_holster_trim_rupper_x, &vr_holster_trim_rupper_y, &vr_holster_trim_rupper_z}};
    }
}

[[nodiscard]] Params pouchParams(int zone, bool seated)
{
    if(zone == zones::ZoneAmmoPouch)
    {
        return seated ? Params{{&vr_ammo_pouch_seated_x, &vr_ammo_pouch_seated_y, &vr_ammo_pouch_seated_z}}
                      : Params{{&vr_ammo_pouch_x, &vr_ammo_pouch_y, &vr_ammo_pouch_z}};
    }
    return seated ? Params{{&vr_grenade_pouch_seated_x, &vr_grenade_pouch_seated_y, &vr_grenade_pouch_seated_z}}
                  : Params{{&vr_grenade_pouch_x, &vr_grenade_pouch_y, &vr_grenade_pouch_z}};
}

// What dragging `zone` moves now: a pouch's own offsets; a holster's pair's (Symmetry) or its trim; seated, the seated
// layout's.
[[nodiscard]] Params paramsFor(int zone)
{
    const bool seated = seated::active();
    if(!isHolster(zone))
    {
        return pouchParams(zone, seated);
    }
    return symmetry() ? pairParams(zone, seated) : trimParams(zone);
}

void toDefaults(const Params& p)
{
    for(cvar_t* c : p.c)
    {
        setIfChanged(c, defaultOf(*c));
    }
}

[[nodiscard]] glm::vec3 get(const Params& p)
{
    return {p.c[0]->value, p.c[1]->value, p.c[2]->value};
}

// The values alone, for a probe (put back at once): no callbacks, nothing saved.
void setRaw(const Params& p, const glm::vec3& v)
{
    for(int i = 0; i < 3; i++)
    {
        p.c[i]->value = v[i];
    }
}

[[nodiscard]] bool pivotWith(const hands::State& s, int zone, const Params& p, const glm::vec3& v, glm::vec3& out)
{
    const glm::vec3 was = get(p);
    setRaw(p, v);
    const bool ok = zones::pivotNow(s, zone, out);
    setRaw(p, was);
    return ok;
}

// The reach point's moves for each setting's (one unit each), probed: a move wanted to the settings' (damped least
// squares, so that a setting that barely moves it, a holster round the back of the torso, doesn't run away).
[[nodiscard]] glm::mat3 solverAt(const hands::State& s, int zone, const Params& p, const glm::vec3& v)
{
    glm::vec3 base;
    if(!pivotWith(s, zone, p, v, base))
    {
        return glm::mat3{0.f};
    }
    glm::mat3 j{0.f};
    for(int i = 0; i < 3; i++)
    {
        glm::vec3 probe = v, moved;
        probe[i] += 1.f;
        if(pivotWith(s, zone, p, probe, moved))
        {
            j[i] = moved - base;
        }
    }
    const glm::mat3 jt = glm::transpose(j);
    glm::mat3 a = jt * j;
    for(int i = 0; i < 3; i++)
    {
        a[i][i] += 0.02f;
    }
    return glm::inverse(a) * jt;
}

[[nodiscard]] glm::vec3 handsMid(int handBits, const hands::State& s)
{
    glm::vec3 sum{0.f};
    int n = 0;
    for(int h = 0; h < HAND_COUNT; h++)
    {
        if(handBits & (1 << h))
        {
            sum += s.pos[h];
            n++;
        }
    }
    return n > 0 ? sum / static_cast<float>(n) : sum;
}

// The held zone's start again from now (a hand took hold or let go): no jump.
void rebase(const hands::State& s)
{
    Edit& e = ses.edit;
    const Params p = paramsFor(e.zone);
    e.params0 = e.params = get(p);
    e.solve = solverAt(s, e.zone, p, e.params0);
    if(!zones::pivotNow(s, e.zone, e.pivot0))
    {
        e.pivot0 = glm::vec3{0.f};
    }
    e.lastTick = e.pivot0;
    e.grabMid = handsMid(e.hands, s);
    e.spread0 = za::max(0.5f, glm::distance(s.pos[HAND_OFF], s.pos[HAND_MAIN]));
    e.size0 = zones::sizeCvar(e.zone).value;
    e.live = e.live && e.hands != 0; // (the dead zone again only for a new hold)
}

// Sets `zone`'s size (and its pair's, with Symmetry), rounded.
void setSize(int zone, float v)
{
    v = za::clamp(roundf(v / 0.05f) * 0.05f, SETUP_SIZE_MIN, SETUP_SIZE_MAX);
    setIfChanged(&zones::sizeCvar(zone), v);
    if(symmetry() && pairOf(zone) >= 0)
    {
        setIfChanged(&zones::sizeCvar(pairOf(zone)), v);
    }
}

void endEdit()
{
    Edit& e = ses.edit;
    if(e.zone >= 0 && !ses.undo.empty() && same(ses.undo.back(), take()))
    {
        ses.undo.popBack(); // (nothing changed: no undo step)
    }
    e = Edit{};
}

void grab(int hand, int zone)
{
    const hands::State& s = hands::current();
    Edit& e = ses.edit;
    if(e.zone < 0)
    {
        pushUndo();
        e = Edit{};
        e.zone = zone;
        e.hands = 1 << hand;
        ses.lastZone = zone;
        rebase(s);
        haptic(hand, 0.04f, 0.6f);
        return;
    }
    if(e.zone != zone || (e.hands & (1 << hand)))
    {
        haptic(hand, 0.02f, 0.15f); // (one holster at a time)
        return;
    }
    e.hands |= 1 << hand; // both hands: resize it (and move it with their middle)
    rebase(s);
    e.live = true;
    haptic(hand, 0.04f, 0.6f);
}

void release(int hand)
{
    Edit& e = ses.edit;
    if(e.zone < 0 || !(e.hands & (1 << hand)))
    {
        return;
    }
    e.hands &= ~(1 << hand);
    haptic(hand, 0.02f, 0.3f);
    if(e.hands == 0)
    {
        endEdit();
        return;
    }
    rebase(hands::current());
}

void editFrame(const hands::State& s)
{
    Edit& e = ses.edit;
    if(e.zone < 0 || !s.valid)
    {
        return;
    }
    const glm::vec3 d = handsMid(e.hands, s) - e.grabMid;
    if(!e.live && glm::length(d) > DEAD_ZONE)
    {
        e.live = true;
    }
    const bool both = e.hands == 3;
    if(e.live)
    {
        // Where the reach point goes: as far as the hands' middle has moved since they took hold, solved for the settings
        // (two steps a frame of the probed solver; its nonlinearity, the torso's front and sides, is caught up over frames).
        const Params p = paramsFor(e.zone);
        const glm::vec3 target = e.pivot0 + d;
        for(int it = 0; it < 2; it++)
        {
            glm::vec3 now;
            if(!pivotWith(s, e.zone, p, e.params, now))
            {
                break;
            }
            glm::vec3 step = e.solve * (target - now);
            const float len = glm::length(step);
            if(len > 6.f)
            {
                step *= 6.f / len;
            }
            e.params += step;
            for(int i = 0; i < 3; i++)
            {
                const float lo = za::min(-PARAM_LIMIT, e.params0[i]), hi = za::max(PARAM_LIMIT, e.params0[i]);
                e.params[i] = za::clamp(e.params[i], lo, hi);
            }
        }
        // Near its shipped place: snapped there (a tick).
        glm::vec3 shipped;
        bool atShipped = true;
        for(int i = 0; i < 3; i++)
        {
            shipped[i] = defaultOf(*p.c[i]);
            atShipped = atShipped && za::fabs(e.params[i] - shipped[i]) < SNAP_RADIUS;
        }
        if(atShipped && !e.snapped)
        {
            for(int h = 0; h < HAND_COUNT; h++)
            {
                if(e.hands & (1 << h))
                {
                    haptic(h, 0.03f, 0.5f);
                }
            }
        }
        e.snapped = atShipped;
        for(int i = 0; i < 3; i++)
        {
            const float v = atShipped ? shipped[i] : roundf(e.params[i] / 0.05f) * 0.05f;
            if(za::fabs(p.c[i]->value - v) > 1e-5f)
            {
                Cvar_SetValueQuick(p.c[i], v);
                e.changed = true;
            }
        }
        // A light tick every unit (3 cm) it goes.
        glm::vec3 at;
        if(zones::pivotNow(s, e.zone, at) && glm::distance(at, e.lastTick) >= 1.f)
        {
            e.lastTick = at;
            for(int h = 0; h < HAND_COUNT; h++)
            {
                if(e.hands & (1 << h))
                {
                    haptic(h, 0.01f, 0.12f);
                }
            }
        }
    }
    if(both)
    {
        const float spread = glm::distance(s.pos[HAND_OFF], s.pos[HAND_MAIN]);
        setSize(e.zone, e.size0 * spread / e.spread0);
    }
    else
    {
        // One hand: its stick up (larger) or down.
        const int h = e.hands == 1 ? 0 : 1;
        const float y = ses.stick[h].y;
        if(za::fabs(y) > 0.25f)
        {
            const float dt = static_cast<float>(za::clamp(host_frametime, 0.0, 0.1));
            const float v = za::clamp(zones::sizeCvar(e.zone).value * expf(y * 0.8f * dt), SETUP_SIZE_MIN, SETUP_SIZE_MAX);
            // (Kept unrounded while the stick is held: small steps add up.)
            setIfChanged(&zones::sizeCvar(e.zone), v);
            if(symmetry() && pairOf(e.zone) >= 0)
            {
                setIfChanged(&zones::sizeCvar(pairOf(e.zone)), v);
            }
        }
    }
}

void resetZone(int zone)
{
    if(zone < 0)
    {
        say("hold or point at a holster first");
        return;
    }
    pushUndo();
    const bool seated = seated::active();
    if(!isHolster(zone))
    {
        toDefaults(pouchParams(zone, seated));
        setIfChanged(&zones::sizeCvar(zone), 1.f);
        say(za::String{label(zone)} + ": back to its shipped place");
    }
    else if(symmetry())
    {
        toDefaults(pairParams(zone, seated));
        for(const int z : {zone, pairOf(zone)})
        {
            toDefaults(trimParams(z));
            setIfChanged(&zones::sizeCvar(z), 1.f);
        }
        say(za::String{label(zone)} + " and its pair: back to their shipped place");
    }
    else
    {
        toDefaults(trimParams(zone));
        setIfChanged(&zones::sizeCvar(zone), 1.f);
        say(za::String{label(zone)} + ": back with its pair (mirrored)");
    }
}

void resetAll()
{
    pushUndo();
    const bool seated = seated::active();
    for(int z = 0; z < ZoneCount; z++)
    {
        toDefaults(isHolster(z) ? pairParams(z, seated) : pouchParams(z, seated));
        if(isHolster(z))
        {
            toDefaults(trimParams(z));
        }
        setIfChanged(&zones::sizeCvar(z), 1.f);
    }
    say(seated ? "every holster and pouch back to its shipped place (seated)"
               : "every holster and pouch back to its shipped place");
}

void undo()
{
    if(ses.edit.zone >= 0)
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

void press(int b)
{
    switch(b)
    {
        case BtnSymmetry:
            Cvar_SetValueQuick(&vr_holstersetup_symmetry, symmetry() ? 0.f : 1.f);
            say(symmetry() ? "Symmetry on: a holster moves its pair mirrored" : "Symmetry off: each holster on its own");
            break;
        case BtnUndo: undo(); break;
        case BtnResetOne: resetZone(ses.lastZone); break;
        case BtnResetAll: resetAll(); break;
        case BtnCancel: finish(false); break;
        case BtnDone: finish(true); break;
        default: break;
    }
}

[[nodiscard]] const char* buttonLabel(int b)
{
    switch(b)
    {
        case BtnSymmetry: return symmetry() ? "Symmetry: On" : "Symmetry: Off";
        case BtnUndo: return "Undo";
        case BtnResetOne: return "Reset Holster";
        case BtnResetAll: return "Reset All";
        case BtnCancel: return "Cancel";
        default: return "Done";
    }
}

// Button `b`'s middle on the panel (3 a row: Symmetry, Undo, Reset Holster; Reset All, Cancel, Done).
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

// Each hand's zone (the one it is deepest in, within HOVER_RATIO) and panel button; a tick on reaching a new one.
void hoverFrame(const hands::State& s)
{
    for(int h = 0; h < HAND_COUNT; h++)
    {
        int best = -1;
        float bestRatio = HOVER_RATIO;
        for(int z = 0; z < ZoneCount; z++)
        {
            zones::Box b;
            if(!zones::boxNow(s, z, b))
            {
                continue;
            }
            const float r = zones::ratioIn(b, s.pos[h]);
            if(r <= bestRatio)
            {
                best = z;
                bestRatio = r;
            }
        }
        if(ses.edit.hands & (1 << h))
        {
            best = ses.edit.zone;
        }
        const int button = buttonAtHand(s.pos[h]);
        if((best != ses.hover[h] && best >= 0) || (button != ses.hoverButton[h] && button >= 0))
        {
            haptic(h, 0.015f, 0.25f);
        }
        ses.hover[h] = best;
        ses.hoverButton[h] = button;
        if(best >= 0 && ses.edit.zone < 0)
        {
            ses.lastZone = best;
        }
    }
}

void box(const zones::Box& b, const glm::vec4& fill, const glm::vec4& edge, float width)
{
    glm::vec3 c[8];
    for(int i = 0; i < 8; i++)
    {
        c[i] = b.centre + b.axis[0] * (i & 1 ? b.half.x : -b.half.x) + b.axis[1] * (i & 2 ? b.half.y : -b.half.y) +
               b.axis[2] * (i & 4 ? b.half.z : -b.half.z);
    }
    // Its six faces (two triangles each), see-through, over the body.
    constexpr int faces[6][4] = {{0, 2, 6, 4}, {1, 3, 7, 5}, {0, 1, 5, 4}, {2, 3, 7, 6}, {0, 1, 3, 2}, {4, 5, 7, 6}};
    for(const auto& f : faces)
    {
        lines::triangle(c[f[0]], c[f[1]], c[f[2]], fill, true);
        lines::triangle(c[f[0]], c[f[2]], c[f[3]], fill, true);
    }
    for(int i = 0; i < 8; i++)
    {
        for(int bit = 1; bit < 8; bit <<= 1)
        {
            if(!(i & bit))
            {
                lines::line(c[i], c[i | bit], width, edge, edge);
            }
        }
    }
}

void drawZones(const hands::State& s)
{
    const glm::vec3 angles{0.f, s.headAngles.y, 0.f};
    for(int z = 0; z < ZoneCount; z++)
    {
        zones::Box b;
        if(!zones::boxNow(s, z, b))
        {
            continue;
        }
        const bool held = ses.edit.zone == z;
        const bool paired = ses.edit.zone >= 0 && symmetry() && pairOf(ses.edit.zone) == z;
        const bool hovered = ses.hover[HAND_OFF] == z || ses.hover[HAND_MAIN] == z;
        const glm::vec4 fill = held      ? glm::vec4{0.2f, 1.f, 0.35f, 0.28f}
                               : paired  ? glm::vec4{0.2f, 0.8f, 1.f, 0.2f}
                               : hovered ? glm::vec4{1.f, 0.85f, 0.15f, 0.24f}
                                         : glm::vec4{0.3f, 0.6f, 1.f, 0.12f};
        const glm::vec4 edge{fill.r, fill.g, fill.b, 0.9f};
        box(b, fill, edge, held || hovered ? 0.2f : 0.12f);
        // Its name over it (the back holsters' too: seen in a mirror), and its size when not 1.
        za::String text{label(z)};
        if(za::fabs(zones::size(z) - 1.f) > 0.01f)
        {
            text += va(" %.2fx", static_cast<double>(zones::size(z)));
        }
        text3d::queueOverlay(text, b.centre + b.axis[2] * (b.half.z + 1.2f), angles, 0.05f);
    }
    for(int h = 0; h < HAND_COUNT; h++)
    {
        const bool holding = (ses.edit.hands & (1 << h)) != 0;
        lines::point(s.pos[h], holding ? 0.5f : 0.35f,
            holding ? glm::vec4{0.2f, 1.f, 0.35f, 1.f} : glm::vec4{1.f, 1.f, 1.f, 0.8f});
    }
}

void drawPanel(const hands::State& s)
{
    const float m2u = units::metresToUnits();
    const glm::vec3 angles{0.f, ses.panelYaw, 0.f};
    for(int b = 0; b < BtnCount; b++)
    {
        const glm::vec3 at = buttonAt(b);
        const bool hovered = ses.hoverButton[HAND_OFF] == b || ses.hoverButton[HAND_MAIN] == b;
        const glm::vec4 color = hovered ? glm::vec4{1.f, 0.85f, 0.15f, 1.f} : glm::vec4{0.5f, 0.7f, 1.f, 0.8f};
        const glm::vec3 r = ses.panelRight * (BUTTON_WIDTH * 0.5f * m2u), u = ses.panelUp * (BUTTON_HEIGHT * 0.5f * m2u);
        // Its face (dark, lighter while a hand is at it) just behind its label, and its frame.
        const glm::vec3 back = at - ses.panelNormal * 0.15f;
        const glm::vec4 face = hovered ? glm::vec4{0.35f, 0.3f, 0.05f, 0.85f} : glm::vec4{0.04f, 0.06f, 0.1f, 0.8f};
        lines::triangle(back - r - u, back + r - u, back + r + u, face);
        lines::triangle(back - r - u, back + r + u, back - r + u, face);
        const glm::vec3 c[4] = {at - r - u, at + r - u, at + r + u, at - r + u};
        for(int i = 0; i < 4; i++)
        {
            (hovered ? lines::glow : lines::line)(c[i], c[(i + 1) % 4], hovered ? 0.25f : 0.12f, color, color);
        }
        // The label, plain text, one size for all: the longest ("Symmetry: Off", 13 characters) in 85% of the width (each
        // character 8 x scale units), at most 55% of the height.
        const char* name = buttonLabel(b);
        constexpr float chars = 13.f;
        const float scale = za::min(BUTTON_WIDTH * 0.85f * m2u / (8.f * chars), BUTTON_HEIGHT * 0.55f * m2u / 8.f);
        text3d::queue(name, at, angles, text3d::Align::Centre, scale);
    }
    // The hints over the panel, in short lines (about the panel's width at HINT_SCALE), readable at its distance.
    za::String text = va("%c%c%c%c%c%c%c %c%c%c%c%c%s", 'H' | 0x80, 'O' | 0x80, 'L' | 0x80, 'S' | 0x80, 'T' | 0x80,
        'E' | 0x80, 'R' | 0x80, 'S' | 0x80, 'E' | 0x80, 'T' | 0x80, 'U' | 0x80, 'P' | 0x80,
        seated::active() ? " (seated)" : "");
    text += "\ngrip a box: drag it";
    text += "\nboth grips on it: resize it";
    text += "\n(or hold it and push the stick)";
    text += "\nbutton: touch it, pull the trigger";
    text += "\nB/Y: undo     menu button: done";
    int lineCount = 6;
    if(realtime < ses.feedbackUntil)
    {
        text += "\n" + ses.feedback;
        lineCount++;
    }
    constexpr float HINT_SCALE = 0.055f;
    const float half = 8.f * HINT_SCALE * (static_cast<float>(lineCount) * 0.5f + 0.4f); // (the backing's pad)
    const float top = (BUTTON_HEIGHT * 1.5f + BUTTON_GAP * 0.5f + 0.02f) * m2u;          // the panel's top, and a gap
    text3d::queueOverlay(text, ses.panelAt + ses.panelUp * (top + half), angles, HINT_SCALE);
    (void)s;
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

[[nodiscard]] bool single()
{
    return sv.active && svs.maxclients == 1 && !cls.demoplayback;
}

// vr_holstersetup: enters it (from the console: no page reopened).
void start_f()
{
    start(-1);
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

// vr_holstersetup_reset [all]: Reset Holster (the last held or hovered) or Reset All.
void reset_f()
{
    if(!ses.active)
    {
        return;
    }
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "all"))
    {
        resetAll();
    }
    else
    {
        resetZone(Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : ses.lastZone);
    }
}

// vr_holstersetup_report: whether it runs, what is held, and each zone's reach point (units from the eyes in the body's
// frame: forward, right, up) and size.
void report_f()
{
    const hands::State& s = hands::current();
    Con_Printf("setup active %d paused %d symmetry %d seated %d held %s hands %d undo %d\n", ses.active ? 1 : 0,
        pausesGame() ? 1 : 0, symmetry() ? 1 : 0, seated::active() ? 1 : 0, zones::name(ses.edit.zone), ses.edit.hands,
        static_cast<int>(ses.undo.size()));
    if(!s.valid)
    {
        return;
    }
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, s.bodyYaw, 0.f}, fwd, right, up);
    for(int z = 0; z < ZoneCount; z++)
    {
        glm::vec3 p;
        if(!zones::pivotNow(s, z, p))
        {
            continue;
        }
        const glm::vec3 d = p - s.head;
        Con_Printf("setup %s at %.2f %.2f %.2f size %.2f\n", zones::name(z), static_cast<double>(glm::dot(d, fwd)),
            static_cast<double>(glm::dot(d, right)), static_cast<double>(glm::dot(d, up)),
            static_cast<double>(zones::size(z)));
    }
}

// vr_holstersetup_press <index>: presses panel button `index` (as a hand's trigger there).
void press_f()
{
    if(ses.active && Cmd_Argc() > 1)
    {
        press(Q_atoi(Cmd_Argv(1)));
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_holstersetup", start_f);
    Cmd_AddCommand("vr_holstersetup_done", done_f);
    Cmd_AddCommand("vr_holstersetup_cancel", cancel_f);
    Cmd_AddCommand("vr_holstersetup_undo", undo_f);
    Cmd_AddCommand("vr_holstersetup_reset", reset_f);
    Cmd_AddCommand("vr_holstersetup_report", report_f);
    Cmd_AddCommand("vr_holstersetup_press", press_f);
}

bool active()
{
    return ses.active && ses.world == worldGeneration();
}

bool pausesGame()
{
    return active() && single();
}

bool start(int returnPage)
{
    const auto refuse = [](const char* why) {
        Con_Printf("Holster Setup: %s\n", why);
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
    if(!single())
    {
        return refuse("only in a single player game (it pauses the game)");
    }
    if(cl.intermission || cl.stats[STAT_HEALTH] <= 0)
    {
        return refuse("not now");
    }
    if(posing::active())
    {
        posing::stop(false);
    }
    if(key_dest == key_menu)
    {
        menuui::backToGame(HAND_MAIN);
    }
    Cbuf_AddText("-attack\n-offhandattack\n");
    twohand::reset();

    ses.active = true;
    ses.world = worldGeneration();
    ses.returnPage = returnPage;
    ses.entry = take();
    ses.undo.clear();
    ses.edit = Edit{};
    ses.lastZone = -1;
    for(int h = 0; h < HAND_COUNT; h++)
    {
        ses.hover[h] = ses.hoverButton[h] = -1;
        ses.stick[h] = glm::vec2{0.f};
    }
    memset(ses.eaten, 0, sizeof(ses.eaten));
    placePanel(hands::current());
    say(seated::active() ? "started (seated: the seated layout is edited)" : "started");
    S_LocalSound("misc/menu3.wav");
    return true;
}

void finish(bool keep)
{
    if(!ses.active)
    {
        return;
    }
    ses.edit = Edit{};
    if(!keep)
    {
        restore(ses.entry);
    }
    const int page = ses.returnPage;
    int changed = 0;
    const Snapshot now = take();
    for(int i = 0; i < managedCount; i++)
    {
        changed += za::fabs(now.v[i] - ses.entry.v[i]) > 1e-5f ? 1 : 0;
    }
    ses.active = false;
    ses.undo.clear();
    Con_Printf("Holster Setup: %s (%d settings changed)\n", keep ? "done" : "cancelled, everything put back", changed);
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
        if(mine && b == posing::Button::Grip)
        {
            release(hand);
        }
        // A grip held since before keeps the game's hold (its release kept from it), as the posing mode.
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
            if(ses.hover[hand] >= 0)
            {
                grab(hand, ses.hover[hand]);
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
    ses.stick[HAND_OFF] = off;
    ses.stick[HAND_MAIN] = main;
}

void frame()
{
    if(!ses.active)
    {
        return;
    }
    // Leaving with the map, the game, the headset (the changes kept); the menu opened some other way: likewise.
    if(ses.world != worldGeneration() || cls.state != ca_connected || !vrActive() || key_dest == key_menu)
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
    hoverFrame(s);
    editFrame(s);
    drawZones(s);
    drawPanel(s);
}

bool highlighted(int zone)
{
    return active() && zone >= 0 &&
           (ses.edit.zone == zone || ses.hover[HAND_OFF] == zone || ses.hover[HAND_MAIN] == zone);
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

} // namespace qvr::holstersetup
