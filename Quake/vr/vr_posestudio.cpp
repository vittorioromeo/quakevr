// vr_posestudio.cpp -- the Pose Studio: see vr_posestudio.hpp.

#include "vr_posestudio.hpp"
#include "vr_aggrip.hpp"
#include "vr_backend.hpp"
#include "vr_bladeedit.hpp"
#include "vr_cvars.hpp"
#include "vr_files.hpp"
#include "vr_hands.hpp"
#include "vr_holstersetup.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_menuui.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/quaternion.hpp>

#include <string.h>
#include <time.h>

namespace qvr::posestudio
{
namespace
{

using rigpose::Rigid;

enum PartId : int
{
    PWeaponR,
    PWeaponL,
    PHandR,
    PHandL,
    PCount
};

enum ButtonId : int
{
    BtnScope,
    BtnUndo,
    BtnReset,
    BtnFrozen,
    BtnCancel,
    BtnDone,
    BtnCount
};

constexpr int grabStartPart = 99;      // partPoint's: the controller where the grab began
constexpr float PANEL_DISTANCE = 0.55f;// metres from the eyes...
constexpr float PANEL_SIDE = 45.f;     // ... this many degrees to the off hand's side of the view (the subject ahead stays clear)
constexpr float PANEL_DROP = 0.25f;    // metres below the eyes
constexpr float PANEL_FOLLOW = 1.0f;   // metres: the head this far from where the panel was placed brings it along
constexpr float BUTTON_WIDTH = 0.17f;  // metres
constexpr float BUTTON_HEIGHT = 0.055f;
constexpr float BUTTON_GAP = 0.015f;
constexpr float BUTTON_DEPTH = 0.08f;  // a hand this near the panel's plane (metres) is at a button
constexpr float HAND_REACH = 0.07f;    // metres: a controller this near a hand's grip point is at it
constexpr float WEAPON_REACH = 0.05f;  // metres: ... this near a weapon's box (or in it) is at it
constexpr float WEAPON_BIAS = 0.03f;   // metres: a hand near enough beats the weapon round it
constexpr float HAND_BOX = 2.f;        // units (the monster's): half a hand's box
constexpr float WEAPON_PAD = 1.2f;     // units: a weapon's box round its points
constexpr float TICK_UNITS = 1.f;      // a haptic tick each unit (or TICK_DEG) a held part moves
constexpr float TICK_DEG = 5.f;
constexpr int UNDO_MAX = 64;

struct Grab
{
    int part{-1};
    int hand{-1};     // the player's hand holding it
    int ent{0};       // the monster's (locked while held)
    int kind{0};
    int slotKey{-1};  // the entry: aggrip::allWeapons or the weapon's slot
    int gripHand{0};  // aggrip::Main / Off / Second
    Rigid rel;        // the part's world frame in the controller's frame at the grab
    Rigid handHeld;   // a weapon's drag: its hand's grip frame held there (model space)
    Rigid weaponHeld; // a hand's drag on the second grip: the right weapon's frame (model space)
    Rigid hotspot;    // ... the second grip's place on it (secondAt less its offset)
    Rigid own, glob;  // the entry's weapon's own and global offsets at the grab (the total is glob * own)
    Rigid total0;     // the total at the grab
    Rigid total;      // ... now
    Rigid weaponNow;  // a weapon's drag: its frame now (model space; adjust draws it there)
    glm::vec3 startPos{0.f};
    glm::vec3 tickPos{0.f};
    glm::quat tickRot{1.f, 0.f, 0.f, 0.f};
};

struct Session
{
    bool active{false};
    int world{0};
    int returnPage{-1};
    float freezeWas{0.f};
    za::String entry; // vr_agrunt_grips on entering (Cancel)
    za::Vector<za::String> undo;
    // the monsters noted: the nearest this frame (pending, filled while the relink runs) and the last complete
    int noteFrame{-1};
    float pendingDist{1e30f};
    Note pending;
    bool havePending{false};
    Note subject;
    bool haveSubject{false};
    int subjectFrame{-1};
    Note locked; // the held one's, while a part is held
    int lockedFrame{-1};
    Grab grab;
    int hover[HAND_COUNT]{-1, -1};
    int hoverButton[HAND_COUNT]{-1, -1};
    bool eaten[HAND_COUNT][6]{};
    glm::vec3 panelAt{0.f}, panelRight{0.f, -1.f, 0.f}, panelUp{0.f, 0.f, 1.f}, panelNormal{-1.f, 0.f, 0.f};
    glm::vec3 panelHead{0.f}; // the head where it was placed
    float panelYaw{0.f};
    za::String feedback;
    double feedbackUntil{0.0};
};
Session ses;

// --- frames --------------------------------------------------------------------------------------------------------

[[nodiscard]] Rigid compose(const Rigid& a, const Rigid& b)
{
    return Rigid{a.rot * b.rot, a.rot * b.pos + a.pos};
}

[[nodiscard]] Rigid inverse(const Rigid& a)
{
    Rigid r;
    r.rot = glm::inverse(a.rot);
    r.pos = -(r.rot * a.pos);
    return r;
}

[[nodiscard]] glm::quat yawQuat(float yawDeg)
{
    return glm::angleAxis(yawDeg * (3.14159265f / 180.f), glm::vec3{0.f, 0.f, 1.f});
}

[[nodiscard]] glm::vec3 toWorld(const Space& s, const glm::vec3& local)
{
    return s.origin + yawQuat(s.yawDeg) * (local * s.scale);
}

[[nodiscard]] Rigid toWorld(const Space& s, const Rigid& r)
{
    return Rigid{glm::normalize(yawQuat(s.yawDeg) * r.rot), toWorld(s, r.pos)};
}

[[nodiscard]] Rigid toModel(const Space& s, const Rigid& w)
{
    const glm::quat inv = glm::inverse(yawQuat(s.yawDeg));
    return Rigid{glm::normalize(inv * w.rot), inv * (w.pos - s.origin) / za::max(s.scale, 0.01f)};
}

// Where a controller takes parts: the hand before anything holds it out (the monster's body pushes the drawn hand off
// it, the controller goes on in), and its world frame (x forward, y left, z up).
[[nodiscard]] glm::vec3 grabber(const hands::State& s, int hand)
{
    return s.unresolvedPos[hand];
}

[[nodiscard]] Rigid controller(const hands::State& s, int hand)
{
    glm::vec3 f, r, u;
    hands::angleVectors(s.unresolvedRot[hand], f, r, u);
    return Rigid{glm::normalize(glm::quat_cast(glm::mat3{f, -r, u})), grabber(s, hand)};
}

[[nodiscard]] float angleDeg(const glm::quat& a, const glm::quat& b)
{
    const float d = za::fabs(glm::dot(glm::normalize(a), glm::normalize(b)));
    return 2.f * za::acos(za::min(1.f, d)) * (180.f / 3.14159265f);
}

// --- what is shown -------------------------------------------------------------------------------------------------

// The note the parts are drawn from: the held monster's while a part is held, else the nearest's.
[[nodiscard]] const Note* current()
{
    if(ses.grab.part >= 0)
    {
        return ses.lockedFrame >= host_framecount - 2 ? &ses.locked : nullptr;
    }
    return ses.haveSubject && ses.subjectFrame >= host_framecount - 2 ? &ses.subject : nullptr;
}

[[nodiscard]] bool heldR(const Note& n)
{
    return n.prop > 0 && n.placed.on[0];
}

[[nodiscard]] bool heldL(const Note& n)
{
    return n.propL > 0 && n.placed.on[1];
}

// Whether part `p` can be taken (a free hand, a fist: not yet: its pose's own key).
[[nodiscard]] bool shown(const Note& n, int p)
{
    switch(p)
    {
        case PWeaponR: return heldR(n);
        case PWeaponL: return heldL(n);
        case PHandR: return heldR(n);
        case PHandL: return heldL(n) || (heldR(n) && n.placed.held2);
        default: return false;
    }
}

// The grip entry part `p` edits (aggrip's hand).
[[nodiscard]] int gripHandOf(const Note& n, int p)
{
    if(p == PWeaponL || (p == PHandL && heldL(n)))
    {
        return aggrip::Off;
    }
    return p == PHandL ? aggrip::Second : aggrip::Main;
}

[[nodiscard]] int slotOf(const Note& n, int gripHand)
{
    return gripHand == aggrip::Off ? n.slotL : n.slot;
}

// The part's world frame: a weapon's (its entity's as placed), a hand's grip frame.
[[nodiscard]] Rigid partFrame(const Note& n, int p)
{
    switch(p)
    {
        case PWeaponR: return toWorld(n.space, n.placed.weapon[0]);
        case PWeaponL: return toWorld(n.space, n.placed.weapon[1]);
        case PHandR: return toWorld(n.space, n.placed.hand[0]);
        default: return toWorld(n.space, n.placed.hand[1]);
    }
}

// A weapon's box in its own frame (units of the model; round its butt, muzzle and second grip), else a small one.
void weaponBox(const Note& n, int p, glm::vec3& lo, glm::vec3& hi)
{
    const int num = p == PWeaponR ? n.prop : n.propL;
    lo = glm::vec3{-4.f, -1.f, -1.f};
    hi = glm::vec3{12.f, 1.f, 1.f};
    glm::vec3 muzzle, butt, second;
    int secondType = 0;
    if(num > 0 && num < cl.num_entities && cl_entities[num].model &&
        view::weaponLocalGeometry(cl_entities[num].model, muzzle, butt, second, secondType))
    {
        lo = glm::min(glm::min(muzzle, butt), glm::vec3{0.f});
        hi = glm::max(glm::max(muzzle, butt), glm::vec3{0.f});
        if(secondType == 1)
        {
            lo = glm::min(lo, second);
            hi = glm::max(hi, second);
        }
    }
    lo -= glm::vec3{WEAPON_PAD};
    hi += glm::vec3{WEAPON_PAD};
}

// How far world point `q` is from part `p` (metres; a weapon's: from its box, biased so a hand near enough wins).
[[nodiscard]] float partDistance(const Note& n, int p, const glm::vec3& q)
{
    const float u2m = 1.f / units::metresToUnits();
    const Rigid f = partFrame(n, p);
    if(p == PWeaponR || p == PWeaponL)
    {
        glm::vec3 lo, hi;
        weaponBox(n, p, lo, hi);
        const glm::vec3 local = glm::inverse(f.rot) * (q - f.pos) / za::max(n.space.scale, 0.01f);
        const glm::vec3 out = glm::max(glm::max(lo - local, local - hi), glm::vec3{0.f});
        return glm::length(out) * n.space.scale * u2m + WEAPON_BIAS;
    }
    return glm::length(q - f.pos) * u2m;
}

[[nodiscard]] float partReach(int p)
{
    return p == PWeaponR || p == PWeaponL ? WEAPON_REACH + WEAPON_BIAS : HAND_REACH;
}

// The point a mock hand goes to for part `p`: a weapon's box's middle, a hand's grip point.
[[nodiscard]] glm::vec3 partGrabPoint(const Note& n, int p)
{
    const Rigid f = partFrame(n, p);
    if(p == PWeaponR || p == PWeaponL)
    {
        glm::vec3 lo, hi;
        weaponBox(n, p, lo, hi);
        return f.pos + f.rot * ((lo + hi) * 0.5f * n.space.scale);
    }
    return f.pos;
}

[[nodiscard]] const char* partName(const Note& n, int p)
{
    switch(p)
    {
        case PWeaponR: return "Weapon (right hand)";
        case PWeaponL: return "Weapon (left hand)";
        case PHandR: return "Right hand";
        default: return heldL(n) ? "Left hand" : "Left hand (second grip)";
    }
}

[[nodiscard]] const char* slotLabel(int slot)
{
    const cvar_t* id = slot >= 0 && slot < weapons::numSlots ? weapons::cvar(slot, weapons::Key::ID) : nullptr;
    return id && id->string && id->string[0] ? COM_SkipPath(id->string) : "?";
}

// --- edits ---------------------------------------------------------------------------------------------------------

void say(const za::String& text)
{
    ses.feedback = text;
    ses.feedbackUntil = realtime + 3.0;
    Con_Printf("Pose Studio: %s\n", text.cStr());
}

void haptic(int hand, float seconds, float amplitude)
{
    if(Backend* be = backend(); be && !vr_disablehaptics.value && hand >= 0 && hand < HAND_COUNT)
    {
        be->haptic(hand, seconds, 160.f, amplitude);
    }
}

void pushUndo()
{
    if(static_cast<int>(ses.undo.size()) >= UNDO_MAX)
    {
        for(int i = 0; i + 1 < static_cast<int>(ses.undo.size()); i++)
        {
            ses.undo[static_cast<za::SizeT>(i)] = ses.undo[static_cast<za::SizeT>(i + 1)];
        }
        ses.undo.popBack();
    }
    ses.undo.pushBack(za::String{vr_agrunt_grips.string});
}

[[nodiscard]] bool scopeAll()
{
    return vr_agrunt_grip_scope.value == 0.f;
}

void grab(int hand, int p)
{
    const Note* np = current();
    if(!np || !shown(*np, p))
    {
        return;
    }
    const Note& n = *np;
    const hands::State& s = hands::current();
    Grab g;
    g.part = p;
    g.hand = hand;
    g.ent = n.ent;
    g.kind = n.kind;
    g.gripHand = gripHandOf(n, p);
    const int slot = slotOf(n, g.gripHand);
    if(slot < 0)
    {
        say("its weapon has no Weapon Offsets slot: nothing to set");
        return;
    }
    g.slotKey = scopeAll() ? aggrip::allWeapons : slot;
    g.glob = aggrip::toRigid(aggrip::global(n.kind, g.gripHand));
    g.own = aggrip::toRigid(aggrip::offset(n.kind, slot, g.gripHand));
    g.total0 = compose(g.glob, g.own);
    g.total = g.total0;
    const Rigid c = controller(s, hand);
    g.rel = compose(inverse(c), partFrame(n, p));
    g.handHeld = n.placed.hand[p == PWeaponL ? 1 : 0];
    g.weaponHeld = n.placed.weapon[0];
    g.hotspot = compose(n.t.secondAt, inverse(g.total0)); // (Second: secondAt = its point on the weapon * the offset)
    g.weaponNow = p == PWeaponL ? n.placed.weapon[1] : n.placed.weapon[0];
    g.startPos = grabber(s, hand);
    g.tickPos = grabber(s, hand);
    g.tickRot = c.rot;
    pushUndo();
    ses.grab = g;
    ses.locked = n;
    ses.lockedFrame = host_framecount;
    haptic(hand, 0.03f, 0.6f);
}

// The held part to the controller now: the entry's offset previewed (the drawing follows next frame).
void dragFrame(const hands::State& s)
{
    Grab& g = ses.grab;
    const Note* np = current();
    if(g.part < 0 || !np)
    {
        return;
    }
    const Note& n = *np;
    const Rigid c = controller(s, g.hand);
    const Rigid partNow = toModel(n.space, compose(c, g.rel));
    switch(g.part)
    {
        case PWeaponR:
        case PWeaponL:
            // (the weapon turned in the hand: the hand held where it was; adjust() draws the weapon here meanwhile)
            g.weaponNow = partNow;
            g.total = compose(inverse(g.handHeld), partNow);
            break;
        case PHandR: g.total = compose(inverse(partNow), n.t.weapon[0]); break;
        default:
            if(g.gripHand == aggrip::Off)
            {
                g.total = compose(inverse(partNow), n.t.weapon[1]);
            }
            else
            {
                // (the second grip's: the hand's frame there on the right weapon, less the grip's point)
                g.total = compose(inverse(g.hotspot), compose(inverse(g.weaponHeld), partNow));
            }
            break;
    }
    const Rigid entry = g.slotKey == aggrip::allWeapons ? compose(g.total, inverse(g.own)) : compose(inverse(g.glob), g.total);
    aggrip::setPreview(g.kind, g.slotKey, g.gripHand, aggrip::fromRigid(entry));
    if(glm::length(grabber(s, g.hand) - g.tickPos) >= TICK_UNITS || angleDeg(c.rot, g.tickRot) >= TICK_DEG)
    {
        g.tickPos = grabber(s, g.hand);
        g.tickRot = c.rot;
        haptic(g.hand, 0.01f, 0.2f);
    }
}

void release()
{
    Grab& g = ses.grab;
    if(g.part < 0)
    {
        return;
    }
    const Rigid entry = g.slotKey == aggrip::allWeapons ? compose(g.total, inverse(g.own)) : compose(inverse(g.glob), g.total);
    aggrip::clearPreview();
    aggrip::set(g.kind, g.slotKey, g.gripHand, aggrip::fromRigid(entry));
    const glm::vec3 d = g.total.pos - g.total0.pos;
    say(va("%s: moved %.2f units, turned %.1f deg in the hand (%s, %s)", g.part == PWeaponR || g.part == PWeaponL ? "weapon" : "hand",
        static_cast<double>(glm::length(d)), static_cast<double>(angleDeg(g.total.rot, g.total0.rot)),
        g.slotKey == aggrip::allWeapons ? "every weapon" : slotLabel(g.slotKey),
        g.gripHand == aggrip::Main ? "main hand" : g.gripHand == aggrip::Off ? "off hand" : "second grip"));
    haptic(g.hand, 0.02f, 0.3f);
    g = Grab{};
    if(!ses.undo.empty() && ses.undo.back() == za::String{vr_agrunt_grips.string})
    {
        ses.undo.popBack(); // (nothing changed)
    }
}

void undo()
{
    if(ses.grab.part >= 0)
    {
        return; // (let go first)
    }
    if(ses.undo.empty())
    {
        say("nothing to undo");
        return;
    }
    Cvar_SetQuick(&vr_agrunt_grips, ses.undo.back().cStr());
    ses.undo.popBack();
    say("undone");
}

void resetGrips()
{
    const Note* n = current();
    if(!n)
    {
        say("no armed humanoid near: walk up to one");
        return;
    }
    pushUndo();
    if(scopeAll())
    {
        aggrip::resetEntries(n->kind, aggrip::allWeapons);
        say(va("%s: every weapon's grips back to the defaults", n->kind == 0 ? "grunts" : "enforcers"));
    }
    else
    {
        for(const int slot : {n->slot, n->slotL})
        {
            if(slot >= 0)
            {
                aggrip::resetEntries(n->kind, slot);
            }
        }
        say(va("%s: %s's own grips back to the defaults", n->kind == 0 ? "grunts" : "enforcers", slotLabel(n->slot)));
    }
}

void dumpDone()
{
    za::String text;
    aggrip::dumpText(text);
    Con_Printf("%s", text.cStr());
    const time_t now = time(nullptr);
    const za::String dir = za::String{com_gamedir} + "/proposed_defaults";
    files::createDirectories(dir.cStr());
    za::String path = dir + "/agrunt_grips_" + files::timestamp(now) + ".txt";
    for(int i = 2; files::exists(path.cStr()); i++)
    {
        path = va("%s/agrunt_grips_%s_%d.txt", dir.cStr(), files::timestamp(now).cStr(), i);
    }
    za::String file = "// Quake VR: the Pose Studio's grips (Done), rows for Quake/vr/vr_aggrip.cpp's defaults (the Every Weapon\n"
                      "// ones for globalDefaults, the others for defaults); kept meanwhile in vr_agrunt_grips.\n";
    file += text;
    if(files::writeText(path.cStr(), file.toStringView()))
    {
        Con_Printf("Pose Studio: written to %s\n", path.cStr());
    }
}

void press(int b)
{
    switch(b)
    {
        case BtnScope:
            Cvar_SetValueQuick(&vr_agrunt_grip_scope, scopeAll() ? 1.f : 0.f);
            say(scopeAll() ? "edits move the grip for every weapon of this kind" : "edits nudge this weapon's own grip only");
            break;
        case BtnUndo: undo(); break;
        case BtnReset: resetGrips(); break;
        case BtnFrozen:
            Cvar_SetValueQuick(&vr_agrunt_freeze, vr_agrunt_freeze.value != 0.f ? 0.f : 1.f);
            say(vr_agrunt_freeze.value != 0.f ? "frozen: they hold still" : "unfrozen: watch them move (they may attack)");
            break;
        case BtnCancel: finish(false); break;
        case BtnDone: finish(true); break;
        default: break;
    }
}

[[nodiscard]] const char* buttonLabel(int b)
{
    switch(b)
    {
        case BtnScope: return scopeAll() ? "Every Weapon" : "This Weapon";
        case BtnUndo: return "Undo";
        case BtnReset: return "Reset Grips";
        case BtnFrozen: return vr_agrunt_freeze.value != 0.f ? "Frozen: On" : "Frozen: Off";
        case BtnCancel: return "Cancel";
        default: return "Done";
    }
}

// --- the panel -----------------------------------------------------------------------------------------------------

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

void placePanel(const hands::State& s)
{
    const float m2u = units::metresToUnits();
    const float yaw = s.headAngles.y + PANEL_SIDE;
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, yaw, 0.f}, fwd, right, up);
    ses.panelAt = s.head + fwd * (PANEL_DISTANCE * m2u) - up * (PANEL_DROP * m2u);
    ses.panelRight = right;
    ses.panelUp = up;
    ses.panelNormal = -fwd;
    ses.panelYaw = yaw;
    ses.panelHead = s.head;
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
        constexpr float chars = 12.f; // ("Every Weapon")
        const float scale = za::min(BUTTON_WIDTH * 0.85f * m2u / (8.f * chars), BUTTON_HEIGHT * 0.55f * m2u / 8.f);
        text3d::queue(buttonLabel(b), at, angles, text3d::Align::Centre, scale);
    }
    za::String text = va("%c%c%c%c %c%c%c%c%c%c", 'P' | 0x80, 'O' | 0x80, 'S' | 0x80, 'E' | 0x80, 'S' | 0x80, 'T' | 0x80,
        'U' | 0x80, 'D' | 0x80, 'I' | 0x80, 'O' | 0x80);
    const Note* n = current();
    if(n)
    {
        text += va("\n%s with %s%s%s", n->kind == 0 ? "Grunt" : "Enforcer", n->slot >= 0 ? slotLabel(n->slot) : "its fists",
            n->slotL >= 0 ? " + " : "", n->slotL >= 0 ? slotLabel(n->slotL) : "");
        text += va("\npose: %s", n->pose);
        text += scopeAll() ? va("\nedits: every weapon a %s holds", n->kind == 0 ? "grunt" : "enforcer")
                           : va("\nedits: %s's own grip only", slotLabel(n->slot));
    }
    else
    {
        text += "\nwalk up to an armed grunt or enforcer";
    }
    text += "\ngrip a box: move and turn it";
    text += "\n  weapon: how it sits in the hand";
    text += "\n  hand: the hand on the weapon";
    text += "\nbutton: touch it, pull the trigger";
    text += "\nB/Y: undo     menu button: done";
    int lineCount = 1;
    for(const char* c = text.cStr(); *c; c++)
    {
        lineCount += *c == '\n' ? 1 : 0;
    }
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

// --- the parts -----------------------------------------------------------------------------------------------------

void box(const Rigid& f, const glm::vec3& lo, const glm::vec3& hi, float scale, float width, const glm::vec4& col)
{
    glm::vec3 c[8];
    for(int i = 0; i < 8; i++)
    {
        const glm::vec3 l{(i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z};
        c[i] = f.pos + f.rot * (l * scale);
    }
    for(int i = 0; i < 8; i++)
    {
        for(int bit = 1; bit < 8; bit <<= 1)
        {
            if(!(i & bit))
            {
                lines::line(c[i], c[i | bit], width, col, col);
            }
        }
    }
}

[[nodiscard]] glm::vec4 partColour(int p)
{
    return p == PWeaponR || p == PWeaponL ? glm::vec4{0.3f, 0.85f, 1.f, 0.75f} : glm::vec4{1.f, 0.55f, 0.15f, 0.8f};
}

void drawParts(const Note& n)
{
    const hands::State& s = hands::current();
    for(int p = 0; p < PCount; p++)
    {
        if(!shown(n, p))
        {
            continue;
        }
        const bool held = ses.grab.part == p;
        const bool hovered = !held && (ses.hover[HAND_OFF] == p || ses.hover[HAND_MAIN] == p);
        const glm::vec4 col = held ? glm::vec4{0.2f, 1.f, 0.35f, 1.f} : hovered ? glm::vec4{1.f, 0.9f, 0.2f, 1.f} : partColour(p);
        const float width = held || hovered ? 0.3f : 0.2f;
        const Rigid f = partFrame(n, p);
        if(p == PWeaponR || p == PWeaponL)
        {
            glm::vec3 lo, hi;
            weaponBox(n, p, lo, hi);
            box(f, lo, hi, n.space.scale, width, col);
        }
        else
        {
            box(f, glm::vec3{-HAND_BOX}, glm::vec3{HAND_BOX}, n.space.scale, width, col);
            // (its grip's axes: forward red, up blue: the way the weapon lies in it)
            lines::line(f.pos, f.pos + f.rot * glm::vec3{3.f * n.space.scale, 0.f, 0.f}, 0.15f, glm::vec4{1.f, 0.3f, 0.3f, 0.8f},
                glm::vec4{1.f, 0.3f, 0.3f, 0.8f});
            lines::line(f.pos, f.pos + f.rot * glm::vec3{0.f, 0.f, 2.f * n.space.scale}, 0.15f, glm::vec4{0.3f, 0.5f, 1.f, 0.8f},
                glm::vec4{0.3f, 0.5f, 1.f, 0.8f});
        }
        if(held || hovered)
        {
            const glm::vec3 at = partGrabPoint(n, p) + glm::vec3{0.f, 0.f, 2.5f * n.space.scale};
            text3d::queueOverlay(partName(n, p), at, glm::vec3{0.f, s.headAngles.y, 0.f}, 0.05f);
        }
    }
    // The free hands, the elbows and the chest: shown dim (the pose's own keys: not set here).
    const glm::vec4 dim{0.6f, 0.6f, 0.65f, 0.35f};
    for(int side = 0; side < 2; side++)
    {
        const glm::vec3 sh = toWorld(n.space, n.placed.shoulder[side]), el = toWorld(n.space, n.placed.elbow[side]);
        lines::line(sh, el, 0.15f, dim, dim);
        lines::line(el, toWorld(n.space, n.placed.hand[side].pos), 0.15f, dim, dim);
        lines::point(el, 0.4f, dim);
    }
    // Each controller near a part: a thin line to it.
    for(int h = 0; h < HAND_COUNT; h++)
    {
        if(ses.hover[h] >= 0 && ses.grab.part < 0 && shown(n, ses.hover[h]))
        {
            const glm::vec4 c{1.f, 0.9f, 0.2f, 1.f};
            lines::line(grabber(s, h), partGrabPoint(n, ses.hover[h]), 0.05f, c, c);
            lines::point(grabber(s, h), 0.5f, c);
        }
        else if(ses.grab.hand != h)
        {
            lines::point(grabber(s, h), 0.3f, glm::vec4{1.f, 1.f, 1.f, 0.7f}); // (where it takes: the drawn hand may be held out)
        }
    }
}

void hoverFrame(const hands::State& s)
{
    const Note* n = current();
    for(int h = 0; h < HAND_COUNT; h++)
    {
        int best = -1;
        float bestD = 1e30f;
        for(int p = 0; p < PCount && n; p++)
        {
            if(!shown(*n, p))
            {
                continue;
            }
            const float d = partDistance(*n, p, grabber(s, h));
            if(d <= partReach(p) && d < bestD)
            {
                best = p;
                bestD = d;
            }
        }
        if(ses.grab.part >= 0)
        {
            best = ses.grab.hand == h ? ses.grab.part : -1;
        }
        if(best != ses.hover[h] && best >= 0)
        {
            haptic(h, 0.015f, 0.25f);
        }
        ses.hover[h] = best;
        const int b = buttonAtHand(s.pos[h]);
        if(b != ses.hoverButton[h] && b >= 0)
        {
            haptic(h, 0.015f, 0.25f);
        }
        ses.hoverButton[h] = b;
    }
}

// --- commands ------------------------------------------------------------------------------------------------------

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

// vr_posestudio_press <index>: presses panel button `index` (as a hand's trigger there).
void press_f()
{
    if(ses.active && Cmd_Argc() > 1)
    {
        press(Q_atoi(Cmd_Argv(1)));
    }
}

// vr_posestudio_report: whether it runs, the subject, each part's place (world) and the grips it holds now.
void report_f()
{
    const Note* n = current();
    Con_Printf("posestudio active %d subject %d grab %d hover %d %d undo %d scope %s\n", ses.active ? 1 : 0, n ? n->ent : 0,
        ses.grab.part, ses.hover[HAND_OFF], ses.hover[HAND_MAIN], static_cast<int>(ses.undo.size()), scopeAll() ? "all" : "own");
    if(!n)
    {
        return;
    }
    Con_Printf("posestudio subject kind %d slot %d slotL %d pose \"%s\" held2 %d\n", n->kind, n->slot, n->slotL, n->pose,
        n->placed.held2 ? 1 : 0);
    for(int p = 0; p < PCount; p++)
    {
        if(!shown(*n, p))
        {
            continue;
        }
        const Rigid f = partFrame(*n, p);
        const glm::vec3 gp = partGrabPoint(*n, p);
        Con_Printf("posestudio part %d %s at %.2f %.2f %.2f grab %.2f %.2f %.2f\n", p, partName(*n, p), static_cast<double>(f.pos.x),
            static_cast<double>(f.pos.y), static_cast<double>(f.pos.z), static_cast<double>(gp.x), static_cast<double>(gp.y),
            static_cast<double>(gp.z));
    }
    // (each hand's total grip now: the weapon's frame in the hand's grip frame, as drawn; and as the offsets have it)
    for(const int gh : {static_cast<int>(aggrip::Main), static_cast<int>(aggrip::Off), static_cast<int>(aggrip::Second)})
    {
        const int slot = slotOf(*n, gh);
        if(slot < 0 || (gh == aggrip::Off && !heldL(*n)) || (gh == aggrip::Second && !n->placed.held2))
        {
            continue;
        }
        const Rigid T = compose(aggrip::toRigid(aggrip::global(n->kind, gh)), aggrip::toRigid(aggrip::offset(n->kind, slot, gh)));
        const aggrip::Offset o = aggrip::fromRigid(T);
        Con_Printf("posestudio grip %d total %.2f %.2f %.2f ang %.1f %.1f %.1f\n", gh, static_cast<double>(o.pos.x),
            static_cast<double>(o.pos.y), static_cast<double>(o.pos.z), static_cast<double>(o.angles.x),
            static_cast<double>(o.angles.y), static_cast<double>(o.angles.z));
        if(gh != aggrip::Second)
        {
            const int side = gh == aggrip::Main ? 0 : 1;
            const Rigid drawn = compose(inverse(n->placed.hand[side]), n->placed.weapon[side]);
            Con_Printf("posestudio drawn %d weapon-in-hand %.2f %.2f %.2f, turned %.1f deg from the offsets'\n", gh,
                static_cast<double>(drawn.pos.x), static_cast<double>(drawn.pos.y), static_cast<double>(drawn.pos.z),
                static_cast<double>(angleDeg(drawn.rot, T.rot)));
        }
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_posestudio", start_f);
    Cmd_AddCommand("vr_posestudio_done", done_f);
    Cmd_AddCommand("vr_posestudio_cancel", cancel_f);
    Cmd_AddCommand("vr_posestudio_undo", undo_f);
    Cmd_AddCommand("vr_posestudio_press", press_f);
    Cmd_AddCommand("vr_posestudio_report", report_f);
}

bool active()
{
    return ses.active && ses.world == worldGeneration();
}

bool start(int returnPage)
{
    const auto refuse = [](const char* why) {
        Con_Printf("Pose Studio: %s\n", why);
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
    if(!agarms::enabled())
    {
        return refuse("turn on Posed Arms first (vr_agarms 1)");
    }
    if(bladeedit::active())
    {
        bladeedit::finish(true);
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
    ses = Session{};
    ses.active = true;
    ses.world = worldGeneration();
    ses.returnPage = returnPage;
    ses.entry = za::String{vr_agrunt_grips.string};
    ses.freezeWas = vr_agrunt_freeze.value;
    Cvar_SetValueQuick(&vr_agrunt_freeze, 1.f);
    placePanel(hands::current());
    say("walk up to an armed grunt or enforcer; grip a box to move it");
    S_LocalSound("misc/menu3.wav");
    return true;
}

void finish(bool keep)
{
    if(!ses.active)
    {
        return;
    }
    if(ses.grab.part >= 0)
    {
        release();
    }
    aggrip::clearPreview();
    if(!keep)
    {
        Cvar_SetQuick(&vr_agrunt_grips, ses.entry.cStr());
    }
    const bool changed = ses.entry != za::String{vr_agrunt_grips.string};
    const int page = ses.returnPage;
    ses.active = false;
    ses.undo.clear();
    Cvar_SetValueQuick(&vr_agrunt_freeze, ses.freezeWas);
    Con_Printf("Pose Studio: %s\n", keep ? (changed ? "done" : "done (nothing changed)") : "cancelled, the grips put back");
    if(keep && changed)
    {
        dumpDone();
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
        if(mine && b == posing::Button::Grip && ses.grab.hand == hand)
        {
            release();
        }
        return mine;
    }
    if(!active() || key_dest != key_game)
    {
        return false;
    }
    // (the walk's buttons pass: A/X, the sticks' presses)
    if(b == posing::Button::Primary || b == posing::Button::StickClick)
    {
        return false;
    }
    ses.eaten[hand][i] = true;
    switch(b)
    {
        case posing::Button::Grip:
            if(ses.grab.part < 0 && ses.hover[hand] >= 0)
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
        default: break;
    }
    return true;
}

void frame()
{
    if(!ses.active)
    {
        return;
    }
    // Leaving with the map, the game, the headset, the menu opened (the changes kept).
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
    // (the nearest noted, complete: the relink's last pass)
    if(ses.havePending && ses.noteFrame >= host_framecount - 1)
    {
        ses.subject = ses.pending;
        ses.haveSubject = true;
        ses.subjectFrame = ses.noteFrame;
    }
    const float m2u = units::metresToUnits();
    if(glm::length(s.head - ses.panelHead) > PANEL_FOLLOW * m2u && ses.hoverButton[HAND_OFF] < 0 && ses.hoverButton[HAND_MAIN] < 0)
    {
        placePanel(s); // (walking off: it comes along)
    }
    dragFrame(s);
    hoverFrame(s);
    if(const Note* n = current())
    {
        drawParts(*n);
    }
    drawPanel();
}

void adjust(int ent, agarms::Targets& t)
{
    const Grab& g = ses.grab;
    if(!active() || g.part < 0 || g.ent != ent)
    {
        return;
    }
    if(g.part == PWeaponR || g.part == PWeaponL)
    {
        t.weapon[g.part == PWeaponR ? 0 : 1] = g.weaponNow; // (the weapon where the controller holds it)
    }
}

void note(const Note& n)
{
    if(!active())
    {
        return;
    }
    if(n.ent == ses.grab.ent && ses.grab.part >= 0)
    {
        ses.locked = n;
        ses.lockedFrame = host_framecount;
    }
    if(ses.noteFrame != host_framecount)
    {
        ses.noteFrame = host_framecount;
        ses.pendingDist = 1e30f;
        ses.havePending = false;
    }
    const hands::State& s = hands::current();
    const float d = glm::length(n.space.origin - s.head);
    if(d <= vr_posestudio_reach.value * units::metresToUnits() && d < ses.pendingDist)
    {
        ses.pendingDist = d;
        ses.pending = n;
        ses.havePending = true;
    }
}

bool partPoint(int hand, int index, const glm::vec3& offset, glm::vec3& out)
{
    if(!active() || hand < 0 || hand >= HAND_COUNT)
    {
        return false;
    }
    // (the mock moves the drawn hand by the way to the point: its push out of the body added, the controller lands on it)
    const hands::State& s = hands::current();
    const glm::vec3 push = s.pos[hand] - grabber(s, hand);
    if(index == grabStartPart)
    {
        if(ses.grab.part < 0)
        {
            return false;
        }
        out = ses.grab.startPos + offset + push;
        return true;
    }
    const Note* n = current();
    if(!n || index < 0 || index >= PCount || !shown(*n, index))
    {
        return false;
    }
    out = partGrabPoint(*n, index) + offset + push;
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

} // namespace qvr::posestudio
