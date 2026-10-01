// vr_posing.cpp -- the weapon posing mode (see vr_posing.hpp): what it poses, the controllers while it runs, confirming
// (the settings written, as the Weapon Offsets page writes them), undoing, its text and its commands.
//
// The geometry is the view's (vr_view.cpp, "Weapon posing mode"): it draws the floating weapon, draws the posing hand at
// its controller wrapping it, and works out every frame what confirming would write (Candidate).

#include "vr_posing.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_menuui.hpp"
#include "vr_text3d.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"

#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Copysign.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/String/ToString.hpp"

#include <string.h>

namespace qvr::posing
{

namespace
{

using weapons::HotspotType;
using weapons::Key;

Session current;
Candidate latest;
int sessionWorld = -1; // the map it was started on (worldGeneration): a new one ends it

// Undo: each confirm's settings as they were (every cvar it changed, once), newest last.
struct Change
{
    cvar_t* var;
    za::String was;
};
struct Step
{
    za::Vector<Change> changes;
    za::String what;
};
za::Vector<Step> undoSteps;

// The last pose confirmed, for vr_pose_check.
struct Posed
{
    bool valid{false};
    Target target{Target::Weapon};
    int weaponHand{1};
    int hotspot{0};
    glm::mat4 rigInWeapon{1.f};
    glm::vec3 palmInWeapon{0.f};
    glm::mat4 rigWorld{1.f};
    bool palmFitted{false}; // palmInWeapon seen solved (solvedPalm)
    // Target::Holster: the weapon's model and its place in the holster (Candidate::inHolster*).
    weapons::HolsterKind kind{weapons::HolsterKind::Hip};
    qmodel_t* model{nullptr};
    glm::vec3 inHolsterPos{0.f}, inHolsterFwd{1.f, 0.f, 0.f}, inHolsterUp{0.f, 0.f, 1.f};
};
Posed lastPosed;

// After a set, the hand is shown solved (the grip it will have) until then.
constexpr double solvedShowSeconds = 1.5;
double solvedUntil = 0.0;

// A line said on confirming, undoing, switching (shown a few seconds).
za::String feedback;
double feedbackUntil = 0.0;

// The buttons pressed while posing: their release is posing's too (not a stray key, a voice note's end, the torch's).
bool eaten[2][6]{};

constexpr float stickDeadzone = 0.2f;
constexpr float turnSpeed = 120.f; // degrees a second, the stick all the way

[[nodiscard]] const char* handName(int hand)
{
    return hand == HAND_MAIN ? "main" : "off";
}

[[nodiscard]] const char* kindName(weapons::HolsterKind kind)
{
    switch(kind)
    {
        case weapons::HolsterKind::Upper: return "Upper (Chest)";
        case weapons::HolsterKind::Shoulder: return "Shoulder (Back)";
        default: return "Hip";
    }
}

[[nodiscard]] const char* typeName(HotspotType type)
{
    switch(type)
    {
        case HotspotType::Grip: return "Grip";
        case HotspotType::Blade: return "Blade";
        case HotspotType::Cup: return "Cup";
        default: return "None";
    }
}

// "v_shot2" for progs/v_shot2.mdl.
[[nodiscard]] za::String shortName(const char* model)
{
    const char* base = strrchr(model, '/');
    za::String s = base ? base + 1 : model;
    if(const za::SizeT dot = s.rfind('.'); dot != za::StringView::nPos)
    {
        s.resize(dot);
    }
    return s;
}

[[nodiscard]] za::String slotName(int slot)
{
    const cvar_t* id = weapons::cvar(slot, Key::ID);
    return id ? shortName(id->string) : za::String{"?"};
}

// The confirm hand's buttons: A and B on the main hand, X and Y on the off hand.
[[nodiscard]] const char* lowerButton(int hand)
{
    return hand == HAND_MAIN ? "A" : "X";
}
[[nodiscard]] const char* upperButton(int hand)
{
    return hand == HAND_MAIN ? "B" : "Y";
}

void haptic(int hand, float seconds, float amplitude)
{
    if(Backend* be = backend(); be && !vr_disablehaptics.value)
    {
        be->haptic(hand, seconds, 160.f, amplitude);
    }
}

void say(const za::String& text, double seconds = 4.0)
{
    feedback = text;
    feedbackUntil = realtime + seconds;
    Con_Printf("posing: %s\n", text.cStr());
}

// The slot a posed setting is written to: the weapon's own, or the one it inherits it from (the Weapon Offsets page
// edits those too). Written there, the weapon's own copy (if it has one) goes back to its default so that it inherits
// the pose: posed from either weapon, both hold it so.
void write(Step& step, int slot, Key key, float value)
{
    const auto remember = [&](cvar_t* var) {
        for(const Change& c : step.changes)
        {
            if(c.var == var)
            {
                return;
            }
        }
        step.changes.pushBack({var, var->string});
    };
    const int from = weapons::inheritsFrom(slot);
    const int target = from >= 0 && weapons::inheritable(key) ? from : slot;
    cvar_t* var = weapons::cvar(target, key);
    if(!var)
    {
        return;
    }
    remember(var);
    Cvar_SetValueQuick(var, value);
    if(target != slot)
    {
        cvar_t* own = weapons::cvar(slot, key);
        if(own && strcmp(own->string, own->default_string) != 0)
        {
            remember(own);
            Cvar_SetQuick(own, own->default_string);
        }
    }
}

// The slot whose settings are written (the source of an inherited weapon's).
[[nodiscard]] int writtenSlot()
{
    const int from = weapons::inheritsFrom(current.slot);
    return from >= 0 ? from : current.slot;
}

[[nodiscard]] int firstFreeHotspot(int slot)
{
    for(int i = 0; i < weapons::maxHotspots; i++)
    {
        if(weapons::hotspot(slot, i).type == HotspotType::None)
        {
            return i;
        }
    }
    return -1;
}

// A hotspot's type to pose: its own, or a grip for a new one.
[[nodiscard]] HotspotType typeFor(int slot, int index)
{
    const HotspotType t = weapons::hotspot(slot, index).type;
    return t == HotspotType::None ? HotspotType::Grip : t;
}

void confirm()
{
    if(!current.active)
    {
        return;
    }
    const Candidate& c = latest;
    if(!c.valid)
    {
        say("nothing to set yet: put the hand on the weapon");
        return;
    }
    Step step;
    const int slot = current.slot;
    const bool reset = resetOffsets();
    char buf[256];
    if(current.target == Target::Holster)
    {
        const weapons::HolsterKind kind = current.holster.kind;
        const float values[] = {c.holstered.offset.x, c.holstered.offset.y, c.holstered.offset.z, c.holstered.angles.x,
            c.holstered.angles.y, c.holstered.angles.z};
        for(int f = 0; f < weapons::holsteredFields; f++)
        {
            write(step, slot, weapons::holsteredKey(kind, f), values[f]);
        }
        q_snprintf(buf, sizeof(buf), "set %s in the %s holsters: %.1f %.1f %.1f, turn %.0f %.0f %.0f",
            slotName(writtenSlot()).cStr(), kindName(kind), values[0], values[1], values[2], values[3], values[4], values[5]);
        current.holster.moved = false;
    }
    else if(current.target == Target::Weapon)
    {
        constexpr Key place[] = {Key::OffsetX, Key::OffsetY, Key::OffsetZ, Key::Pitch, Key::Yaw, Key::Roll};
        const float values[] = {c.offset.x, c.offset.y, c.offset.z, c.angles.x, c.angles.y, c.angles.z};
        for(int i = 0; i < 6; i++)
        {
            write(step, slot, place[i], values[i]);
        }
        if(reset)
        {
            constexpr Key residual[] = {Key::WholeX, Key::WholeY, Key::WholeZ, Key::WholePitch, Key::WholeYaw, Key::WholeRoll,
                Key::HandOnlyX, Key::HandOnlyY, Key::HandOnlyZ, Key::HandOnlyPitch, Key::HandOnlyYaw, Key::HandOnlyRoll};
            for(const Key k : residual)
            {
                write(step, slot, k, 0.f);
            }
        }
        q_snprintf(buf, sizeof(buf), "set %s in the hand: offset %.2f %.2f %.2f, turn %.1f %.1f %.1f%s",
            slotName(writtenSlot()).cStr(), c.offset.x, c.offset.y, c.offset.z, c.angles.x, c.angles.y, c.angles.z,
            reset ? " (tuning offsets 0)" : "");
    }
    else
    {
        const weapons::Hotspot& h = c.spot;
        const int i = current.hotspot;
        write(step, slot, weapons::hotspotKey(i, 0), static_cast<float>(static_cast<int>(h.type)));
        for(int k = 0; k < 3; k++)
        {
            write(step, slot, weapons::hotspotKey(i, 1 + k), h.pos[k]);
        }
        if(h.type != HotspotType::Blade)
        {
            for(int k = 0; k < 3; k++)
            {
                write(step, slot, weapons::hotspotKey(i, 5 + k), h.angles[k]);
            }
            if(reset)
            {
                for(int k = 0; k < 6; k++)
                {
                    write(step, slot, weapons::hotspotKey(i, 10 + k), 0.f);
                }
            }
        }
        // A hotspot given to a weapon whose two-handed use is forbidden (the axe, Mjolnir, the grappling hook) would
        // never be taken: it is allowed (as the Weapon Offsets page does).
        if(static_cast<int>(weapons::value(slot, Key::TwoHMode)) == 2) // WPN_2H_FORBIDDEN (vr_twohand.cpp)
        {
            write(step, slot, Key::TwoHMode, 0.f);
            Con_Printf("Two-handed use allowed for this weapon (it has a hotspot now).\n");
        }
        if(h.type == HotspotType::Blade)
        {
            q_snprintf(buf, sizeof(buf), "set %s hotspot %d: Blade at %.2f of the way to the tip", slotName(writtenSlot()).cStr(),
                i + 1, h.pos.x);
        }
        else
        {
            q_snprintf(buf, sizeof(buf), "set %s hotspot %d: %s at %.2f %.2f %.2f, hand %.0f %.0f %.0f%s",
                slotName(writtenSlot()).cStr(), i + 1, typeName(h.type), h.pos.x, h.pos.y, h.pos.z, h.angles.x, h.angles.y,
                h.angles.z, reset ? " (held hand offset 0)" : "");
        }
    }
    step.what = buf;
    undoSteps.pushBack(ZA_MOVE(step));
    lastPosed = {true, current.target, current.weaponHand, current.hotspot, c.rigInWeapon, c.palmInWeapon, c.rigWorld,
        c.palmFitted, current.holster.kind, current.model, c.inHolsterPos, c.inHolsterFwd, c.inHolsterUp};
    solvedUntil = realtime + solvedShowSeconds; // the grip it gives, for a moment
    say(buf);
    S_LocalSound("weapons/pkup.wav");
    haptic(posingHand(), 0.05f, 0.7f);
    haptic(confirmHand(), 0.05f, 0.7f);
}

void undo()
{
    if(undoSteps.empty())
    {
        say("nothing to undo");
        return;
    }
    const Step step = ZA_MOVE(undoSteps.back());
    undoSteps.popBack();
    for(za::SizeT k = step.changes.size(); k-- > 0;) // (the last first)
    {
        Cvar_SetQuick(step.changes[k].var, step.changes[k].was.cStr());
    }
    lastPosed.valid = false;
    if(current.active && current.target == Target::Holster)
    {
        current.holster.fromSettings = true; // the weapon back where the settings now put it
        current.holster.grabHand = -1;
    }
    say("undone: " + step.what);
    S_LocalSound("misc/menu2.wav");
    if(current.active)
    {
        haptic(confirmHand(), 0.03f, 0.4f);
    }
}

// Weapon -> hotspot 1 .. 4 -> weapon.
void next()
{
    if(current.target == Target::Holster)
    {
        // Hip -> upper -> shoulder -> hip: placed again in front (the Weapon Offsets page edits that kind then).
        HolsterSession& h = current.holster;
        h.kind = static_cast<weapons::HolsterKind>((static_cast<int>(h.kind) + 1) % weapons::holsterKinds);
        h.placed = false;
        h.grabHand = -1;
        Cvar_SetValueQuick(&vr_weapon_holster, static_cast<float>(static_cast<int>(h.kind) + 1));
        latest.valid = false;
        S_LocalSound("misc/menu1.wav");
        return;
    }
    if(current.target == Target::Weapon)
    {
        current.target = Target::Hotspot;
        current.hotspot = 0;
    }
    else if(current.hotspot + 1 < weapons::maxHotspots)
    {
        current.hotspot++;
    }
    else
    {
        current.target = Target::Weapon;
    }
    if(current.target == Target::Hotspot)
    {
        current.type = typeFor(current.slot, current.hotspot);
    }
    latest.valid = false;
    S_LocalSound("misc/menu1.wav");
}

// A hotspot's type: grip -> cup -> blade -> grip.
void nextType()
{
    if(current.target != Target::Hotspot)
    {
        return;
    }
    current.type = current.type == HotspotType::Grip ? HotspotType::Cup
                   : current.type == HotspotType::Cup ? HotspotType::Blade
                                                      : HotspotType::Grip;
    latest.valid = false;
    S_LocalSound("misc/menu1.wav");
}

// The floating weapon turned by `yaw` (left, about the vertical) and `tilt` (the muzzle up, about the axis across the
// view), degrees, about its middle.
void turn(float yaw, float tilt)
{
    if(current.target == Target::Holster)
    {
        HolsterSession& h = current.holster;
        if(!h.placed || (yaw == 0.f && tilt == 0.f))
        {
            return;
        }
        // The holster and the weapon in it turned together about the holster's point (the pose between them stays).
        const glm::mat3 m = glm::mat3_cast(glm::angleAxis(glm::radians(yaw), glm::vec3{0.f, 0.f, 1.f}) *
                                           glm::angleAxis(glm::radians(-tilt), h.tiltAxis));
        h.out = m * h.out;
        h.up = m * h.up;
        h.side = m * h.side;
        h.basePos = h.pivot + m * (h.basePos - h.pivot);
        h.baseTurn = m * h.baseTurn;
        h.slotPos = h.pivot + m * (h.slotPos - h.pivot);
        h.slotTurn = m * h.slotTurn;
        if(h.grabHand < 0)
        {
            h.weaponPos = h.pivot + m * (h.weaponPos - h.pivot);
            h.weaponTurn = m * h.weaponTurn;
        }
        return;
    }
    if(!current.placed || (yaw == 0.f && tilt == 0.f))
    {
        return;
    }
    const glm::mat3 m = glm::mat3_cast(glm::angleAxis(glm::radians(yaw), glm::vec3{0.f, 0.f, 1.f}) *
                                       glm::angleAxis(glm::radians(-tilt), current.tiltAxis));
    current.turn = m * current.turn;
    current.modelOrigin = current.pivot + m * (current.modelOrigin - current.pivot);
}

void resetTurn()
{
    if(current.target == Target::Holster)
    {
        // In front again, as it started: the holster where you look, the weapon where its settings put it.
        current.holster.placed = false;
        current.holster.grabHand = -1;
        latest.valid = false;
        return;
    }
    current.turn = current.startTurn;
    current.modelOrigin = current.startOrigin;
}

[[nodiscard]] za::String targetLine()
{
    const int poser = posingHand();
    if(current.target == Target::Holster)
    {
        return za::String{"The "} + kindName(current.holster.kind) + " holster: take the weapon with a grip, put it in it";
    }
    if(current.target == Target::Weapon)
    {
        return za::String{"The weapon: pose your "} + handName(poser) + " hand on it";
    }
    return "Hotspot " + za::toString(current.hotspot + 1) + " (" + typeName(current.type) + "): pose your " +
           handName(poser) + " hand";
}

// vr_pose [weapon|1..4|new|stop] [main|off]
void pose_f()
{
    const char* what = Cmd_Argc() > 1 ? Cmd_Argv(1) : "weapon";
    if(!q_strcasecmp(what, "stop") || !q_strcasecmp(what, "off") || !q_strcasecmp(what, "0"))
    {
        if(current.active)
        {
            stop(false);
        }
        return;
    }
    int weaponHand = static_cast<int>(vr_pose_weapon_hand.value) == 0 ? HAND_OFF : HAND_MAIN;
    if(Cmd_Argc() > 2)
    {
        weaponHand = !q_strcasecmp(Cmd_Argv(2), "off") ? HAND_OFF : HAND_MAIN;
        Cvar_SetValueQuick(&vr_pose_weapon_hand, weaponHand == HAND_MAIN ? 1.f : 0.f);
    }
    Target target = Target::Weapon;
    int index = 0;
    if(!q_strcasecmp(what, "new"))
    {
        target = Target::Hotspot;
        index = -1;
    }
    else if(!q_strcasecmp(what, "hip") || !q_strcasecmp(what, "upper") || !q_strcasecmp(what, "shoulder"))
    {
        target = Target::Holster;
        index = !q_strcasecmp(what, "hip") ? 0 : !q_strcasecmp(what, "upper") ? 1 : 2;
    }
    else if(what[0] >= '1' && what[0] <= '4' && !what[1])
    {
        target = Target::Hotspot;
        index = what[0] - '1';
    }
    else if(q_strcasecmp(what, "weapon") != 0)
    {
        Con_Printf("usage: vr_pose [weapon | 1..4 | new | hip | upper | shoulder | stop] [main | off]\n"
                   "  poses the weapon in the main hand (or else the off hand's), held in the hand given; hip, upper,\n"
                   "  shoulder: in that kind of holster\n");
        return;
    }
    // The weapon held (the main hand's, else the off hand's).
    int from = HAND_MAIN;
    int slot = weapons::heldSlot(from);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        from = HAND_OFF;
        slot = weapons::heldSlot(from);
    }
    start(slot, weapons::heldModel(from), weaponHand, target, index, -1);
}

void confirm_f()
{
    confirm();
}

void undo_f()
{
    undo();
}

void next_f()
{
    if(current.active)
    {
        next();
    }
}

void type_f()
{
    if(current.active)
    {
        nextType();
    }
}

// vr_pose_turn <yaw> <tilt>: turns the floating weapon (degrees); no arguments: back to the start.
void turn_f()
{
    if(Cmd_Argc() < 3)
    {
        resetTurn();
        return;
    }
    turn(Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2)));
}

void check_f()
{
    if(!lastPosed.valid)
    {
        Con_Printf("vr_pose_check: nothing confirmed (or it was undone)\n");
        return;
    }
    if(lastPosed.target == Target::Holster)
    {
        view::holsterPoseCheck(lastPosed.kind, lastPosed.model, lastPosed.inHolsterPos, lastPosed.inHolsterFwd,
            lastPosed.inHolsterUp);
        return;
    }
    view::posingCheck(lastPosed.target == Target::Weapon, lastPosed.weaponHand, lastPosed.rigInWeapon,
        lastPosed.palmFitted ? &lastPosed.palmInWeapon : nullptr, lastPosed.rigWorld);
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_pose", pose_f);
    Cmd_AddCommand("vr_pose_confirm", confirm_f);
    Cmd_AddCommand("vr_pose_undo", undo_f);
    Cmd_AddCommand("vr_pose_next", next_f);
    Cmd_AddCommand("vr_pose_type", type_f);
    Cmd_AddCommand("vr_pose_turn", turn_f);
    Cmd_AddCommand("vr_pose_check", check_f);
}

bool active()
{
    return current.active && sessionWorld == worldGeneration();
}

Session& session()
{
    return current;
}

Candidate& candidate()
{
    return latest;
}

int posingHand()
{
    if(current.target == Target::Holster)
    {
        return current.holster.grabHand >= 0 ? current.holster.grabHand : current.weaponHand;
    }
    return current.target == Target::Weapon ? current.weaponHand : 1 - current.weaponHand;
}

int confirmHand()
{
    return 1 - posingHand();
}

bool resetOffsets()
{
    return vr_pose_reset_offsets.value != 0.f;
}

bool showSolved()
{
    return vr_pose_solve.value != 0.f || realtime < solvedUntil;
}

void solvedPalm(const Candidate& c)
{
    if(!lastPosed.valid || !c.valid || !c.palmFitted || lastPosed.target != current.target ||
        lastPosed.weaponHand != current.weaponHand || lastPosed.hotspot != current.hotspot)
    {
        return;
    }
    for(int i = 0; i < 4; i++)
    {
        for(int j = 0; j < 4; j++)
        {
            if(za::fabs(c.rigInWeapon[i][j] - lastPosed.rigInWeapon[i][j]) > 1e-3f)
            {
                return; // moved since the set
            }
        }
    }
    lastPosed.palmInWeapon = c.palmInWeapon;
    lastPosed.palmFitted = true;
}

bool start(int slot, qmodel_t* model, int weaponHand, Target target, int hotspot, int returnPage)
{
    const auto refuse = [](const char* why) {
        Con_Printf("Posing mode: %s\n", why);
        return false;
    };
    if(!vrActive())
    {
        return refuse("it needs the headset (or vr_backend mock)");
    }
    if(cls.state != ca_connected || !cl.worldmodel || !hands::current().valid)
    {
        return refuse("start a map first");
    }
    if(slot < 0 || slot == weapons::fistSlot() || !model || model->type != mod_alias || weapons::slotForModel(model) != slot)
    {
        return refuse("hold the weapon to pose");
    }
    if(target == Target::Holster && hotspot < 0)
    {
        hotspot = 0;
    }
    if(target == Target::Hotspot && hotspot < 0)
    {
        hotspot = firstFreeHotspot(slot);
        if(hotspot < 0)
        {
            return refuse("its four hotspots are taken: pose one of them again, or remove one");
        }
    }

    // Back to the game (the VR Settings page is reopened on leaving).
    if(key_dest == key_menu)
    {
        menuui::backToGame(weaponHand);
    }
    // Nothing the hands were doing goes on: no shot, no two-handed grip or carried gun.
    Cbuf_AddText("-attack\n-offhandattack\n");
    twohand::reset();

    current = Session{};
    current.active = true;
    current.slot = slot;
    current.model = model;
    current.weaponHand = weaponHand;
    current.target = target;
    current.hotspot = target == Target::Holster ? 0 : CLAMP(0, hotspot, weapons::maxHotspots - 1);
    current.type = typeFor(slot, current.hotspot);
    current.holster.kind =
        static_cast<weapons::HolsterKind>(target == Target::Holster ? CLAMP(0, hotspot, weapons::holsterKinds - 1) : 0);
    current.returnPage = returnPage;
    sessionWorld = worldGeneration();
    latest = Candidate{};
    undoSteps.clear();
    feedback.clear();
    ZA_MEMSET(eaten, 0, sizeof(eaten));

    const int from = weapons::inheritsFrom(slot);
    const za::String whose = from >= 0 ? " (the settings of " + slotName(from) + ", which it inherits)" : za::String{};
    if(target == Target::Holster)
    {
        Con_Printf("Posing %s%s: %s. A/X sets it, B/Y undoes; the menu button sets it and leaves.\n", slotName(slot).cStr(),
            whose.cStr(), targetLine().cStr());
    }
    else
    {
        Con_Printf("Posing %s%s: %s. Confirm with %s, undo with %s; the menu button leaves.\n", slotName(slot).cStr(),
            whose.cStr(), targetLine().cStr(), lowerButton(confirmHand()), upperButton(confirmHand()));
    }
    S_LocalSound("misc/menu3.wav");
    return true;
}

void stop(bool reopenMenu)
{
    if(!current.active)
    {
        return;
    }
    const int page = current.returnPage;
    current.active = false;
    latest.valid = false;
    feedback.clear();
    Con_Printf("Posing mode: left\n");
    if(reopenMenu && page >= 0)
    {
        menu::reopen(page);
    }
}

bool button(int hand, Button b, bool down)
{
    const int i = static_cast<int>(b);
    if(hand < 0 || hand > 1)
    {
        return false;
    }
    if(!down)
    {
        const bool mine = eaten[hand][i];
        eaten[hand][i] = false;
        if(mine && b == Button::Grip && current.holster.grabHand == hand)
        {
            current.holster.grabHand = -1; // let go: the weapon stays where it is
            haptic(hand, 0.03f, 0.4f);
        }
        // A grip held since before posing stays held for the game (its release is kept from it): the weapon or thing
        // the hand held is still there afterwards, with vr_weapon_grip_mode 0 too.
        return mine || (active() && b == Button::Grip);
    }
    if(!active() || key_dest != key_game)
    {
        return false;
    }
    eaten[hand][i] = true;
    if(b == Button::Menu)
    {
        // In a holster, the menu button sets the weapon where it was left, then leaves.
        if(current.target == Target::Holster && current.holster.moved)
        {
            confirm();
        }
        stop(true);
        return true;
    }
    if(current.target == Target::Holster)
    {
        // Either hand: the grip carries the weapon, A/X sets, B/Y undoes, the trigger goes on to the next holster, the
        // stick click puts it all back in front as it started.
        switch(b)
        {
            case Button::Grip:
                current.holster.grabHand = hand;
                current.holster.grabTaken = false;
                haptic(hand, 0.03f, 0.5f);
                break;
            case Button::Primary: confirm(); break;
            case Button::Secondary: undo(); break;
            case Button::Trigger: next(); break;
            case Button::StickClick: resetTurn(); break;
            default: break;
        }
        return true;
    }
    if(hand != confirmHand())
    {
        return true; // the posing hand's buttons do nothing (its grip and trigger still close its fingers)
    }
    switch(b)
    {
        case Button::Primary: confirm(); break;
        case Button::Secondary: undo(); break;
        case Button::Trigger: next(); break;
        // The grip does nothing: the hand may hold the controller as it likes. The stick click is a hotspot's type, and
        // on the weapon puts it back as it started.
        case Button::StickClick:
            if(current.target == Target::Hotspot)
            {
                nextType();
            }
            else
            {
                resetTurn();
            }
            break;
        default: break;
    }
    return true;
}

void sticks(const glm::vec2& off, const glm::vec2& main)
{
    if(!active())
    {
        return;
    }
    // In a holster, either stick (the hand carrying the weapon is busy).
    const glm::vec2 stick = current.target == Target::Holster ? off + main : confirmHand() == HAND_MAIN ? main : off;
    const auto dz = [](float v) {
        return za::fabs(v) < stickDeadzone ? 0.f : (v - za::copysign(stickDeadzone, v)) / (1.f - stickDeadzone);
    };
    const float dt = static_cast<float>(CLAMP(0.0, host_frametime, 0.1));
    turn(-dz(stick.x) * turnSpeed * dt, dz(stick.y) * turnSpeed * dt);
}

void frame()
{
    if(!current.active)
    {
        return;
    }
    // Leaving: the menu opened some other way (leaving the page), the map, the game, the headset.
    if(key_dest == key_menu || sessionWorld != worldGeneration() || cls.state != ca_connected || cl.intermission ||
        !vrActive() || cl.stats[STAT_HEALTH] <= 0)
    {
        stop(false);
        return;
    }
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return;
    }

    const int confirmer = confirmHand();
    const int from = weapons::inheritsFrom(current.slot);
    za::String text = va("%c%c%c%c%c%c%c %s", 'P' | 0x80, 'O' | 0x80, 'S' | 0x80, 'I' | 0x80, 'N' | 0x80, 'G' | 0x80, ':' | 0x80,
        slotName(current.slot).cStr());
    if(from >= 0)
    {
        text += " (sets " + slotName(from) + "'s)";
    }
    text += "\n" + targetLine() + "\n";
    if(current.target == Target::Holster)
    {
        text += "grip: take it, let go   A/X: set it   B/Y: undo   trigger: next holster";
        text += current.holster.moved ? "\nstick: turn it (click: back)   menu: set it and leave"
                                      : "\nstick: turn it (click: back)   menu: leave";
    }
    else
    {
        text += va("%s: set it   %s: undo   trigger: next", lowerButton(confirmer), upperButton(confirmer));
        text += current.target == Target::Hotspot ? "\nstick: turn it   click: type   menu: leave"
                                                  : "\nstick: turn it (click: back)   menu: leave";
    }
    if(realtime < feedbackUntil)
    {
        text += "\n" + feedback;
    }

    const float m2u = units::metresToUnits();
    const glm::vec3 fwd = hands::forward(glm::vec3{0.f, s.headAngles.y, 0.f});
    // Ahead, a little below the eyes: above the weapon (40 cm ahead at the chest) as it is seen.
    const glm::vec3 at = s.head + fwd * (0.8f * m2u) - glm::vec3{0.f, 0.f, 0.2f * m2u};
    text3d::queueOverlay(text, at, glm::vec3{0.f, s.headAngles.y, 0.f}, 0.045f);
}

} // namespace qvr::posing
