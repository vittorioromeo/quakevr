// vr_setup.cpp -- see vr_setup.hpp.

#include "vr_setup.hpp"
#include "vr_bodycal.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_menu.hpp"
#include "vr_still.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <string.h>

namespace qvr::setup
{
namespace
{

// ---------------------------------------------------------------------------------------------------------------------
// The wall buttons' settings: each button steps its setting to the next preset (after the last, the first).

struct Preset
{
    float value;
    const char* name;
};

struct Option
{
    const char* key;   // vr_setup_option <key>
    const char* label; // in the message
    const char* cvar;
    Preset presets[4];
    int count;
};

constexpr Option options[] = {
    {"turning", "Turning", "vr_snap_turn", {{0.f, "Smooth"}, {30.f, "Snap 30"}, {45.f, "Snap 45"}, {90.f, "Snap 90"}}, 4},
    {"turnspeed", "Smooth Turn Speed", "vr_turn_speed", {{2.f, "Slow"}, {3.25f, "Normal"}, {5.f, "Fast"}}, 3},
    {"movedir", "Move Towards", "vr_movement_mode", {{1.f, "Head"}, {0.f, "Hand"}}, 2},
    {"sticks", "Swap Stick Functions", "vr_stick_swap", {{0.f, "Left moves"}, {1.f, "Right moves"}}, 2},
    {"run", "Default Speed", "cl_alwaysrun", {{1.f, "Run"}, {0.f, "Walk"}}, 2},
    {"teleport", "Teleport", "vr_teleport_enabled", {{0.f, "Off"}, {1.f, "On"}}, 2},
    {"grip", "Weapon Grip", "vr_weapon_grip_mode", {{0.f, "Hold"}, {1.f, "Sticky"}}, 2},
    {"gadget", "Wrist Gadget Arm", "vr_gadget_arm", {{0.f, "Left"}, {1.f, "Right"}}, 2},
    {"torch", "Flashlight Side", "vr_flashlight_side", {{0.f, "Left"}, {1.f, "Right"}}, 2},
    {"position", "Position", "vr_bodycal_seated", {{0.f, "Standing"}, {1.f, "Seated"}}, 2},
    {"scale", "World Scale", "vr_world_scale", {{1.f, "1.00: world larger"}, {1.2f, "1.20: normal"}, {1.5f, "1.50: world smaller"}}, 3},
    {"body", "Body", "vr_body_mode", {{0.f, "Off"}, {2.f, "Torso and Arms"}, {3.f, "Full Body"}}, 3},
    {"hud", "HUD", "vr_hud_mode", {{1.f, "Wrist Gadget"}, {0.f, "Status Bar"}}, 2},
    {"crosshair", "Crosshair", "vr_crosshair", {{0.f, "Off"}, {1.f, "Dot"}, {2.f, "Laser"}, {3.f, "Soft Laser"}}, 4},
    {"climb", "Climbing", "vr_climb", {{1.f, "On"}, {0.f, "Off"}}, 2},
    {"swim", "Swimming", "vr_swim", {{1.f, "Immersive"}, {0.f, "Vanilla"}}, 2},
    // vrstart's settings pavilion (ROUND21.md, "vrstart"): the old hub's raw-cvar buttons, with their screens now
    {"holsters", "Weapon Mode", "vr_holster_mode", {{0.f, "Immersive"}, {1.f, "Quick Slots"}}, 2},
    {"reload", "Reloading", "vr_reload_mode", {{3.f, "Immersive"}, {2.f, "Hip Holsters"}, {1.f, "All Holsters"}, {0.f, "Off"}}, 4},
    {"twohand", "Two-Handed Aim", "vr_2h_mode", {{2.f, "Virtual Stock"}, {1.f, "Basic"}, {0.f, "Off"}}, 3},
    {"tips", "Tips", "vr_tips", {{1.f, "Floating Screens"}, {2.f, "Wrist Gadget"}, {0.f, "Off"}}, 3},
};

[[nodiscard]] const Option* findOption(const char* key)
{
    for(const Option& o : options)
    {
        if(!q_strcasecmp(o.key, key))
        {
            return &o;
        }
    }
    return nullptr;
}

// The preset the setting is on (-1: none of them, a value set elsewhere).
[[nodiscard]] int currentPreset(const Option& o, const cvar_t& var)
{
    for(int i = 0; i < o.count; i++)
    {
        if(za::fabs(var.value - o.presets[i].value) < 0.001f)
        {
            return i;
        }
    }
    return -1;
}

void option_f()
{
    if(Cmd_Argc() < 2 || !q_strcasecmp(Cmd_Argv(1), "list"))
    {
        Con_Printf("vr_setup_option <key> [preset]: steps a setting to its next preset (or to preset 0..n-1), prints it and "
                   "saves the config (the calibration room's buttons)\n");
        for(const Option& o : options)
        {
            const cvar_t* var = Cvar_FindVar(o.cvar);
            const int at = var ? currentPreset(o, *var) : -1;
            Con_Printf("  %-10s %s (%s %s): %s\n", o.key, o.label, o.cvar, var ? var->string : "?",
                at >= 0 ? o.presets[at].name : "custom");
        }
        return;
    }
    const Option* o = findOption(Cmd_Argv(1));
    cvar_t* var = o ? Cvar_FindVar(o->cvar) : nullptr;
    if(!o || !var)
    {
        Con_Printf("vr_setup_option: no setting \"%s\" (vr_setup_option list)\n", Cmd_Argv(1));
        return;
    }
    const int at = currentPreset(*o, *var);
    const int next = Cmd_Argc() > 2 ? CLAMP(0, Q_atoi(Cmd_Argv(2)), o->count - 1) : (at + 1) % o->count;
    Cvar_SetValueQuick(var, o->presets[next].value);
    Con_Printf("%s: %s\n", o->label, o->presets[next].name);
    saveConfigNow();
}

// The value screen above each of the map's setting buttons (the server's edicts: a local game).
void drawOptionScreens()
{
    if(!sv.active || !sv.worldmodel || cls.state != ca_connected || !vrActive())
    {
        return;
    }
    qcvm_t* const old = qcvm;
    if(old != &sv.qcvm)
    {
        if(old)
        {
            PR_SwitchQCVM(nullptr);
        }
        PR_SwitchQCVM(&sv.qcvm);
    }
    constexpr const char* prefix = "vr_setup_option ";
    const size_t prefixLength = ZA_STRLEN(prefix);
    // The button's label board's top over its top (QC buttons.qc), when it has one: a screen goes above it, 2 clear of
    // it (its half height: 0.3's characters, vr_text3d.cpp's screens), or at least `least` over the button's top.
    const int labelTopField = ED_FindFieldOffset("vr_button_label_top");
    const auto overTop = [&](edict_t* e, float least) {
        const eval_t* const v = labelTopField >= 0 ? GetEdictFieldValue(e, labelTopField) : nullptr;
        constexpr float screenHalf = 2.4f * 0.5f + 2.4f * 0.375f + 2.4f * 0.3f;
        return v ? za::max(least, v->_float + 2.f + screenHalf) : least;
    };
    // A button's box where it rests (the author, 2026-10-09: the screens rode the press into the wall): its box now,
    // less how far it has moved from its rest place (QC's pos1, where func_button starts: its origin then).
    const int pos1Field = ED_FindFieldOffset("pos1");
    const auto restBox = [&](edict_t* e, glm::vec3& lo, glm::vec3& hi) {
        glm::vec3 moved{0.f};
        const eval_t* const p = pos1Field >= 0 ? GetEdictFieldValue(e, pos1Field) : nullptr;
        if(p && !strcmp(PR_GetString(e->v.classname), "func_button"))
        {
            moved = glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]} -
                    glm::vec3{p->vector[0], p->vector[1], p->vector[2]};
        }
        lo = glm::vec3{e->v.absmin[0], e->v.absmin[1], e->v.absmin[2]} - moved;
        hi = glm::vec3{e->v.absmax[0], e->v.absmax[1], e->v.absmax[2]} - moved;
    };
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free || !e->v.targetname)
        {
            continue;
        }
        const char* target = PR_GetString(e->v.targetname);
        // A hub's campaign buttons ("vr_activestartpaknameidx <n>; ..."): "SELECTED" over the one chosen (the portal
        // takes you to its start)
        constexpr const char* campaignPrefix = "vr_activestartpaknameidx ";
        const size_t campaignLength = ZA_STRLEN(campaignPrefix);
        if(!q_strncasecmp(target, campaignPrefix, campaignLength))
        {
            if(Q_atoi(target + campaignLength) == static_cast<int>(qvr::vr_activestartpaknameidx.value))
            {
                glm::vec3 lo, hi;
                restBox(e, lo, hi);
                const glm::vec3 dir{e->v.movedir[0], e->v.movedir[1], e->v.movedir[2]};
                // (in front of the button: a lectern's cap may overhang it)
                const glm::vec3 at3 = 0.5f * (lo + hi) + glm::vec3{0.f, 0.f, 0.5f * (hi.z - lo.z) + overTop(e, 14.f)} - dir * 8.f;
                text3d::queue("SELECTED", at3, glm::vec3{0.f, glm::degrees(za::atan2(dir.y, dir.x)), 0.f},
                    text3d::Align::Centre, 0.3f, true);
            }
            continue;
        }
        if(q_strncasecmp(target, prefix, prefixLength))
        {
            continue;
        }
        char key[32];
        q_strlcpy(key, target + prefixLength, sizeof(key));
        key[strcspn(key, " ;\n")] = 0;
        const Option* o = findOption(key);
        const cvar_t* var = o ? Cvar_FindVar(o->cvar) : nullptr;
        if(!var)
        {
            continue;
        }
        const int at = currentPreset(*o, *var);
        const char* text = at >= 0 ? o->presets[at].name : va("custom: %s", var->string);
        // Above the button at rest, just off the panel it is set in (it is pushed along movedir, into the panel): still
        // while it is pressed.
        glm::vec3 lo, hi;
        restBox(e, lo, hi);
        const glm::vec3 dir{e->v.movedir[0], e->v.movedir[1], e->v.movedir[2]};
        const glm::vec3 centre = 0.5f * (lo + hi);
        const glm::vec3 at3 = centre + glm::vec3{0.f, 0.f, 0.5f * (hi.z - lo.z) + overTop(e, 9.f)} + dir * 2.f;
        const float yaw = glm::degrees(za::atan2(dir.y, dir.x));
        text3d::queue(text, at3, glm::vec3{0.f, yaw, 0.f}, text3d::Align::Centre, 0.3f, true);
    }
    if(old != &sv.qcvm)
    {
        PR_SwitchQCVM(nullptr);
        if(old)
        {
            PR_SwitchQCVM(old);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The steps

enum class Step
{
    Idle,
    Welcome,    // the welcome, before the steps (a start: not a restart after the menu)
    Intro,      // a few seconds to stand in place (or set Position to Seated)
    Height,     // standing tall and still
    Body,       // Body Calibration running
    BodyReview, // its page open on a result it didn't trust
    Paused,     // the menu open in the calibration room (on Body Calibration's page): starts over when it closes
    Done,       // the summary
};

constexpr double welcomeSeconds = 7.0;
constexpr double introSeconds = 8.0;
constexpr double readSeconds = 3.0;       // a step's instructions before its countdown
constexpr double stillSeconds = 1.0;      // the head held this still...
constexpr float stillMetres = 0.015f;     // ...within this
constexpr double gotItSeconds = 1.2;
constexpr double doneSeconds = 12.0;
constexpr const char* roomMap = "vrcalibration";

struct Flow
{
    Step step{Step::Idle};
    double start{0.0}; // the step's, realtime
    bool pending{false}; // vr_setup: starts when the room has loaded
    int roomWorld{-1};   // the room's load the setup last started on by itself (flowFrame: any way into the room)
    int world{0};
    bool got{false};    // the step's measurement taken ("got it" shown)
    double gotAt{0.0};
    still::Countdown countdown;
    bool counted{false};
    glm::vec3 anchor{0.f}; // the head's place since it last moved (tracking metres)
    double anchorAt{0.0};
    const char* body{""}; // the summary's line: how the body step ended
    float ghostYaw{0.f};  // where the head looked as the step began: the height step's ghost stands there
    // vr_setup_debug: the ghost (stickman) from its first frame shown in this run to the summary
    bool ghostSeen{false};
    int ghostShown{0};
    int ghostHidden{0}; // frames it wasn't drawn in (but those with the menu open)
    Step traced{Step::Idle};
};
Flow flow;

// The text shown, built each frame (the main thread).
struct SetupScratch
{
    za::String text;
    auto members() { return qvr::mem::list(text); }
};
mem::Scratch<SetupScratch> scratch{"setup"};

// `s` in the console font's gold letters.
void appendGold(za::String& out, const char* s)
{
    for(const char* c = s; *c; c++)
    {
        out += static_cast<char>(*c | 0x80);
    }
}

[[nodiscard]] bool seated()
{
    return vr_bodycal_seated.value != 0.f;
}

[[nodiscard]] const char* stepName(Step step)
{
    switch(step)
    {
        case Step::Idle: return "idle";
        case Step::Welcome: return "welcome";
        case Step::Intro: return "intro";
        case Step::Height: return "height";
        case Step::Body: return "body";
        case Step::BodyReview: return "body review";
        case Step::Paused: return "paused";
        case Step::Done: return "done";
    }
    return "?";
}

// Where the room's doorway leads (QC changelevel_touch): the tutorial until it has been started once (a first start's
// way), else the hub.
[[nodiscard]] bool exitToTutorial()
{
    return vr_tutorial_started.value == 0.f;
}

void enter(Step step)
{
    flow.step = step;
    flow.start = realtime;
    flow.got = false;
    flow.counted = false;
    flow.anchorAt = realtime;
    flow.anchor = tracking().head.position;
    const hands::State& s = hands::current();
    flow.ghostYaw = s.valid ? s.headAngles.y : 0.f;
}

// The calibration room is loaded (a local game in it).
[[nodiscard]] bool inRoom()
{
    return sv.active && !q_strcasecmp(sv.name, roomMap);
}

void stop(const char* why)
{
    if(flow.step == Step::Idle)
    {
        return;
    }
    flow.step = Step::Idle;
    if(why)
    {
        S_LocalSound("misc/menu3.wav");
        Con_Printf("VR Calibration stopped (%s): the main menu's VR Calibration runs it again\n", why);
    }
}

// The menu opened while the setup runs. In the calibration room (calibration only: no buttons) it pauses on Body
// Calibration's page, whose first row is Position (standing or seated), and starts over when the menu closes; elsewhere
// (`vr_setup here`) it stops.
void menuOpened()
{
    if(!inRoom())
    {
        stop("the menu");
        return;
    }
    flow.step = Step::Paused;
    menu::reopen(menu::bodyCalibrationPage());
    Con_Printf("VR Calibration: paused on Body Calibration's page (Position: standing or seated); it starts over when the "
               "menu closes\n");
}

// `welcome`: a start (the room loaded, vr_setup here): the welcome first; a restart (the menu closed): the steps.
void begin(bool welcome)
{
    flow.world = worldGeneration();
    flow.body = "Body: skipped";
    if(welcome)
    {
        flow.ghostSeen = false;
        flow.ghostShown = 0;
        flow.ghostHidden = 0;
    }
    enter(welcome ? Step::Welcome : Step::Intro);
    Con_Printf(inRoom() ? "VR Calibration: starting (height, body); the menu button pauses it\n"
                        : "VR Calibration: starting (height, body); the menu button stops it\n");
}

void finish()
{
    enter(Step::Done);
    S_LocalSound("misc/talk.wav");
    Con_Printf(!inRoom()          ? "VR Calibration: done.\n"
               : exitToTutorial() ? "VR Calibration: done. The glowing doorway behind you leads to the tutorial.\n"
                                  : "VR Calibration: done. The glowing doorway behind you leads to the VR Hub.\n");
    if(vr_setup_debug.value != 0.f)
    {
        Con_Printf("vr_setup_debug: the stickman %s: shown %d frames, hidden %d since it first showed (menu frames not "
                   "counted)\n", flow.ghostSeen ? "shown" : "never shown", flow.ghostShown, flow.ghostHidden);
    }
    saveConfigNow();
}

void startBody()
{
    const float ghostYaw = flow.ghostYaw; // the height step's ghost's: the body step's stands there too
    enter(Step::Body);
    // Tests: a take of raw tracking played alongside, from the calibration's first frame (TESTING.md, "Body calibration").
    if(vr_setup_test_take.string[0])
    {
        Cmd_ExecuteString(va("vr_motion_play \"%s\" watch noplace", vr_setup_test_take.string), src_command);
    }
    if(!bodycal::restart(-1))
    {
        Con_Printf("VR Calibration: Body Calibration couldn't start: skipped\n");
        finish();
        return;
    }
    bodycal::setGhostYaw(ghostYaw);
}

void afterBody()
{
    if(bodycal::phase() == bodycal::Phase::Result && bodycal::trusted())
    {
        bodycal::apply(); // prints and saves
        flow.body = "Body: measured";
        finish();
        return;
    }
    if(bodycal::phase() == bodycal::Phase::Result)
    {
        Con_Printf("VR Calibration: the body's poses didn't agree: its page shows why (redo a pose, Apply or Cancel); "
                   "close the menu to go on\n");
        flow.body = "Body: see its page";
        menu::reopen(menu::bodyCalibrationPage());
        enter(Step::BodyReview);
        return;
    }
    za::String path;
    Con_Printf("VR Calibration: the body wasn't measured: run it again later from %s\n",
        menu::pathTo("Body Calibration", path) ? path.cStr() : "the Body Calibration page");
    flow.body = "Body: not measured";
    finish();
}

// The step's text, floating ahead of the eyes (as Body Calibration's), `drop` metres below them.
void drawText(za::StringView text, float drop = 0.18f)
{
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return;
    }
    const float m2u = units::metresToUnits();
    const glm::vec3 fwd = hands::forward(glm::vec3{0.f, s.headAngles.y, 0.f});
    const glm::vec3 at = s.head + fwd * (0.9f * m2u) - glm::vec3{0.f, 0.f, drop * m2u};
    text3d::queueOverlay(text, at, glm::vec3{0.f, s.headAngles.y, 0.f}, 0.045f);
}

void heightFrame(double now, za::String& text)
{
    const TrackingState& t = tracking();
    appendGold(text, "HEIGHT");
    text += "\n";
    text += seated() ? "Sit up straight and look ahead.\nHold still." : "Stand up straight and look ahead.\nHold still.";
    text += "\n\n";
    if(flow.got)
    {
        text += "got it";
        if(now - flow.gotAt >= gotItSeconds)
        {
            startBody();
        }
        return;
    }
    if(now - flow.start < readSeconds)
    {
        text += "get ready";
        return;
    }
    if(!flow.counted)
    {
        flow.counted = true;
        flow.countdown.start(now);
    }
    if(!flow.countdown.update(now))
    {
        text += va("%d", flow.countdown.remaining(now));
        return;
    }
    const glm::vec3 head = t.head.position;
    if(!t.head.valid || glm::distance(head, flow.anchor) > stillMetres)
    {
        flow.anchor = head;
        flow.anchorAt = now;
    }
    const float eyes = head.y;
    if(eyes < 0.5f || eyes > 2.5f)
    {
        text += va("is the headset on? (eyes at %.2f m)", eyes);
        flow.anchorAt = now;
        return;
    }
    text += "hold still";
    if(now - flow.anchorAt < stillSeconds)
    {
        return;
    }
    Cvar_SetValueQuick(&vr_height_calibration, eyes);
    Con_Printf("VR Calibration: height set: your eyes at %.2f m%s\n", eyes, seated() ? " (seated)" : "");
    if(!seated() && eyes < 1.3f)
    {
        Con_Printf("VR Calibration: that's low for standing. Playing seated? Open the menu: Position (on the page it opens) to "
                   "Seated, then close it to start over\n");
    }
    flow.got = true;
    flow.gotAt = now;
    S_LocalSound("misc/menu2.wav");
}

void flowFrame()
{
    if(flow.pending && sv.active && cls.state == ca_connected && cls.signon == SIGNONS && cl.worldmodel &&
        !q_strcasecmp(sv.name, roomMap))
    {
        flow.pending = false;
        flow.roomWorld = worldGeneration();
        begin(true);
    }
    // The room entered any other way (the hub's VR Calibration button is "map vrcalibration", a changelevel, the
    // console): the room is for calibrating only, so the setup starts there too, once per load of it.
    if(flow.step == Step::Idle && inRoom() && cls.state == ca_connected && cls.signon == SIGNONS && cl.worldmodel &&
        vrActive() && flow.roomWorld != worldGeneration())
    {
        flow.roomWorld = worldGeneration();
        begin(true);
    }
    if(flow.step == Step::Idle)
    {
        return;
    }
    if(flow.world != worldGeneration() || cls.state != ca_connected || cls.signon != SIGNONS || cl.intermission ||
        !vrActive())
    {
        stop(nullptr);
        return;
    }
    if(cl.stats[STAT_HEALTH] <= 0)
    {
        stop("you died");
        return;
    }
    const double now = realtime;
    if(flow.step == Step::Paused)
    {
        if(key_dest == key_menu)
        {
            return;
        }
        begin(false); // and its first frame now (the stickman with it: no frame without it)
    }
    if(flow.step == Step::BodyReview)
    {
        if(key_dest != key_menu && bodycal::phase() != bodycal::Phase::Capturing)
        {
            finish();
        }
        return;
    }
    if(flow.step == Step::Body)
    {
        if(bodycal::phase() == bodycal::Phase::Capturing)
        {
            return; // its own text and ghost
        }
        if(vr_setup_test_take.string[0])
        {
            Cmd_ExecuteString("vr_motion_play stop", src_command); // the test's take ends with the poses
        }
        if(key_dest == key_menu)
        {
            menuOpened();
            return;
        }
        afterBody();
        return;
    }
    if(key_dest == key_menu && flow.step != Step::Done)
    {
        menuOpened();
        return;
    }

    za::String& text = scratch.text;
    text.clear();
    if(flow.step == Step::Welcome && now - flow.start >= welcomeSeconds)
    {
        enter(Step::Intro);
    }
    if(flow.step == Step::Welcome)
    {
        // The author, 2026-10-10: the calibration started too abruptly. In the middle of the view, a few seconds.
        appendGold(text, "WELCOME TO QUAKE VR: UNLEASHED!");
        text += "\n\nWe will shortly begin the calibration.\nYou will be asked to pose and make\ncertain movements to "
                "calibrate your body.";
        drawText(text, 0.f);
        return;
    }
    appendGold(text, "VR CALIBRATION");
    text += "\n";
    switch(flow.step)
    {
        case Step::Intro:
        {
            // Body Calibration's first pose from here on, facing you (the author, 2026-10-10: the stickman, once shown,
            // stays until the end; the menu's restart comes back here): seated, none.
            const hands::State& hs = hands::current();
            if(!seated() && hs.valid)
            {
                bodycal::drawStandingGhost(hs.headAngles.y);
            }
            const int left = static_cast<int>(za::ceil(introSeconds - (now - flow.start)));
            text += "Your height, then your body.\n\n";
            text += seated() ? "Seated: sit where you will play.\n" : "Stand in the middle of your play space.\n";
            text += inRoom() ? "Playing seated? Open the menu: set\nPosition to Seated, then close it.\n\n" : "\n";
            text += va("starting in %d", za::max(left, 1));
            if(now - flow.start >= introSeconds)
            {
                enter(Step::Height);
            }
            break;
        }
        case Step::Height:
            // Body Calibration's first pose, standing tall, shown from the height step on (the author, 2026-10-10: the
            // ghost appeared only with Body Calibration); seated, no standing figure.
            if(!seated())
            {
                bodycal::drawStandingGhost(flow.ghostYaw);
            }
            heightFrame(now, text);
            break;
        case Step::Done:
        {
            appendGold(text, "DONE");
            // The height as the body step left it (its first pose measures it again).
            text += va("\nHeight: eyes at %.2f m\n%s\n\n", vr_height_calibration.value, flow.body);
            if(inRoom())
            {
                // Where its doorway leads: the tutorial at a first start (QC changelevel_touch), else the hub.
                text += exitToTutorial() ? "The glowing doorway behind you\nleads to the tutorial."
                                         : "The glowing doorway behind you\nleads to the VR Hub.";
            }
            if(now - flow.start >= doneSeconds || key_dest == key_menu)
            {
                flow.step = Step::Idle;
            }
            break;
        }
        default: break;
    }
    if(flow.step != Step::Idle && flow.step != Step::Body)
    {
        text += flow.step == Step::Done ? "" : inRoom() ? "\n\nmenu button: pause" : "\n\nmenu button: stop";
        drawText(text);
    }
}

// vr_setup_debug: each step's start, and the stickman's frames from its first shown in a run to the summary (printed
// there); 2: every frame (shown or hidden, the step). After flowFrame: bodycal::frame (vr_main.cpp) drew its ghost first.
void traceFrame()
{
    if(flow.step == Step::Idle || flow.step == Step::Done)
    {
        flow.traced = flow.step;
        return;
    }
    const bool shown = bodycal::ghostFrame() == host_framecount;
    const bool menu = key_dest == key_menu;
    flow.ghostSeen = flow.ghostSeen || shown;
    if(flow.ghostSeen && !menu)
    {
        (shown ? flow.ghostShown : flow.ghostHidden)++;
    }
    if(vr_setup_debug.value == 0.f)
    {
        flow.traced = flow.step;
        return;
    }
    if(flow.traced != flow.step)
    {
        Con_Printf("vr_setup_debug: %.2f %s (stickman %s)\n", realtime, stepName(flow.step), shown ? "shown" : "hidden");
        flow.traced = flow.step;
    }
    if(vr_setup_debug.value >= 2.f || (flow.ghostSeen && !shown && !menu))
    {
        Con_Printf("vr_setup_debug: frame %d %s stickman %s%s\n", host_framecount, stepName(flow.step),
            shown ? "shown" : "HIDDEN", menu ? " (menu)" : "");
    }
}

// vr_setup: the calibration room, a new game in it, the setup starting there; vr_setup here: the setup in this map now.
void setup_f()
{
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "here"))
    {
        if(cls.state != ca_connected || !sv.active)
        {
            Con_Printf("vr_setup here: in a game\n");
            return;
        }
        if(bodycal::phase() == bodycal::Phase::Capturing)
        {
            return;
        }
        begin(true);
        return;
    }
    stop(nullptr);
    flow.pending = true;
    // Next, before anything else queued (a test script's next commands).
    Cbuf_InsertText(va("disconnect\nmaxplayers 1\ndeathmatch 0\ncoop 0\nmap %s\n", roomMap));
}

void skip_f()
{
    switch(flow.step)
    {
        case Step::Welcome: enter(Step::Intro); break;
        case Step::Intro: enter(Step::Height); break;
        case Step::Height: startBody(); break;
        case Step::Body:
            bodycal::stop(); // its result, if any, is left to its page
            finish();
            break;
        case Step::BodyReview: finish(); break;
        case Step::Paused: begin(false); break;
        case Step::Done: flow.step = Step::Idle; break;
        default: Con_Printf("vr_setup_skip: the setup isn't running\n"); break;
    }
}

void stop_f()
{
    stop("vr_setup_stop");
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_setup", setup_f);
    Cmd_AddCommand("vr_setup_option", option_f);
    Cmd_AddCommand("vr_setup_skip", skip_f);
    Cmd_AddCommand("vr_setup_stop", stop_f);
    Cmd_AddCommand("vr_menu_path_check", menu::pathCheck_f);
    Cmd_AddCommand("vr_menu_dump", menu::dump_f);
}

void frame()
{
    // A first start (vr_setup_pending, vr_cvars.cpp) begun without the headset: VR Calibration once it tracks, with no
    // game running or in the vrstart hub (where VR starts), as the main menu's VR Calibration row; only once. (Started
    // with VR on, vr_startgame goes to the calibration room at once: vr_main.cpp.)
    if(vr_setup_pending.value != 0.f && vrActive() && tracking().head.valid && (!sv.active || !strcmp(sv.name, VR_HubMap())) &&
        !running() && !flow.pending)
    {
        Cvar_SetValueQuick(&vr_setup_pending, 0.f);
        Con_Printf("VR: a first start: VR Calibration (the main menu's first row runs it again)\n");
        Cbuf_AddText("vr_setup\n");
    }
    flowFrame();
    traceFrame();
    drawOptionScreens();
}

bool running()
{
    return flow.step != Step::Idle;
}

const char* exitBoardName()
{
    return exitToTutorial() ? "VR TUTORIAL" : "VR HUB";
}

} // namespace qvr::setup
