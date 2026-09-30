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

#include <cmath>
#include <cstring>
#include <string>
#include <string_view>
#include <tuple>

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
    {"movedir", "Move Towards", "vr_movement_mode", {{1.f, "Head"}, {0.f, "Off Hand"}}, 2},
    {"run", "Default Speed", "cl_alwaysrun", {{1.f, "Run"}, {0.f, "Walk"}}, 2},
    {"teleport", "Teleport", "vr_teleport_enabled", {{0.f, "Off"}, {1.f, "On"}}, 2},
    {"grip", "Weapon Grip", "vr_weapon_grip_mode", {{0.f, "Hold"}, {1.f, "Sticky"}}, 2},
    {"hand", "Main Hand", "vr_lefthanded", {{0.f, "Right"}, {1.f, "Left"}}, 2},
    {"position", "Position", "vr_bodycal_seated", {{0.f, "Standing"}, {1.f, "Seated"}}, 2},
    {"scale", "World Scale", "vr_world_scale", {{1.f, "1.00: world larger"}, {1.25f, "1.25: normal"}, {1.5f, "1.50: world smaller"}}, 3},
    {"body", "Body", "vr_body_mode", {{0.f, "Off"}, {2.f, "Torso and Arms"}, {3.f, "Full Body"}}, 3},
    {"hud", "HUD", "vr_hud_mode", {{1.f, "Wrist Gadget"}, {0.f, "Status Bar"}}, 2},
    {"crosshair", "Crosshair", "vr_crosshair", {{0.f, "Off"}, {1.f, "Dot"}, {2.f, "Laser"}, {3.f, "Soft Laser"}}, 4},
    {"climb", "Climbing", "vr_climb", {{1.f, "On"}, {0.f, "Off"}}, 2},
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
        if(std::fabs(var.value - o.presets[i].value) < 0.001f)
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
    const size_t prefixLength = std::strlen(prefix);
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free || !e->v.targetname)
        {
            continue;
        }
        const char* target = PR_GetString(e->v.targetname);
        if(q_strncasecmp(target, prefix, prefixLength))
        {
            continue;
        }
        char key[32];
        q_strlcpy(key, target + prefixLength, sizeof(key));
        key[std::strcspn(key, " ;\n")] = 0;
        const Option* o = findOption(key);
        const cvar_t* var = o ? Cvar_FindVar(o->cvar) : nullptr;
        if(!var)
        {
            continue;
        }
        const int at = currentPreset(*o, *var);
        const char* text = at >= 0 ? o->presets[at].name : va("custom: %s", var->string);
        // Above the button, just off the panel it is set in (it is pushed along movedir, into the panel).
        const glm::vec3 lo{e->v.absmin[0], e->v.absmin[1], e->v.absmin[2]};
        const glm::vec3 hi{e->v.absmax[0], e->v.absmax[1], e->v.absmax[2]};
        const glm::vec3 dir{e->v.movedir[0], e->v.movedir[1], e->v.movedir[2]};
        const glm::vec3 centre = 0.5f * (lo + hi);
        const glm::vec3 at3 = centre + glm::vec3{0.f, 0.f, 0.5f * (hi.z - lo.z) + 9.f} + dir * 2.f;
        const float yaw = glm::degrees(std::atan2(dir.y, dir.x));
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
    Intro,      // a few seconds to stand in place (or set Position to Seated)
    Height,     // standing tall and still
    Body,       // Body Calibration running
    BodyReview, // its page open on a result it didn't trust
    Hand,       // the main hand raised
    Done,       // the summary
};

constexpr double introSeconds = 8.0;
constexpr double readSeconds = 3.0;       // a step's instructions before its countdown
constexpr double stillSeconds = 1.0;      // the head held this still...
constexpr float stillMetres = 0.015f;     // ...within this
constexpr double gotItSeconds = 1.2;
constexpr double handHoldSeconds = 0.75;  // a hand held up this long
constexpr float handAboveMetres = 0.08f;  // above the eyes
constexpr double handGiveUpSeconds = 25.0;
constexpr double doneSeconds = 12.0;
constexpr const char* roomMap = "vrcalibration";

struct Flow
{
    Step step{Step::Idle};
    double start{0.0}; // the step's, realtime
    bool pending{false}; // vr_setup: starts when the room has loaded
    int world{0};
    bool got{false};    // the step's measurement taken ("got it" shown)
    double gotAt{0.0};
    still::Countdown countdown;
    bool counted{false};
    glm::vec3 anchor{0.f}; // the head's place since it last moved (tracking metres)
    double anchorAt{0.0};
    int raised{-1};       // the hand held up (HAND_MAIN, HAND_OFF), since raisedAt
    double raisedAt{0.0};
    const char* body{""}; // the summary's lines: how the body step ended,
    const char* hand{""}; // and the main hand
};
Flow flow;

// The text shown, built each frame (the main thread).
struct SetupScratch
{
    std::string text;
    auto members() { return qvr::mem::list(text); }
};
mem::Scratch<SetupScratch> scratch{"setup"};

// `s` in the console font's gold letters.
void appendGold(std::string& out, const char* s)
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

void enter(Step step)
{
    flow.step = step;
    flow.start = realtime;
    flow.got = false;
    flow.counted = false;
    flow.raised = -1;
    flow.anchorAt = realtime;
    flow.anchor = tracking().head.position;
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
        Con_Printf("VR Calibration stopped (%s): the START CALIBRATION button runs it again\n", why);
    }
}

void begin()
{
    flow.world = worldGeneration();
    flow.body = "Body: skipped";
    flow.hand = "Main hand: skipped";
    enter(Step::Intro);
    Con_Printf("VR Calibration: starting (height, body, main hand); the menu button stops it\n");
}

void finish()
{
    enter(Step::Done);
    S_LocalSound("misc/talk.wav");
    Con_Printf("VR Calibration: done. The buttons on the walls change the main options.\n");
    saveConfigNow();
}

void startBody()
{
    enter(Step::Body);
    // Tests: a take of raw tracking played alongside, from the calibration's first frame (TESTING.md, "Body calibration").
    if(vr_setup_test_take.string[0])
    {
        Cmd_ExecuteString(va("vr_motion_play \"%s\" watch noplace", vr_setup_test_take.string), src_command);
    }
    if(!bodycal::restart(-1))
    {
        Con_Printf("VR Calibration: Body Calibration couldn't start: skipped\n");
        enter(Step::Hand);
    }
}

void afterBody()
{
    if(bodycal::phase() == bodycal::Phase::Result && bodycal::trusted())
    {
        bodycal::apply(); // prints and saves
        flow.body = "Body: measured";
        enter(Step::Hand);
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
    std::string path;
    Con_Printf("VR Calibration: the body wasn't measured: run it again later from %s\n",
        menu::pathTo("Body Calibration", path) ? path.c_str() : "the Body Calibration page");
    flow.body = "Body: not measured";
    enter(Step::Hand);
}

// The step's text, floating ahead of the eyes (as Body Calibration's).
void drawText(std::string_view text)
{
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return;
    }
    const float m2u = units::metresToUnits();
    const glm::vec3 fwd = hands::forward(glm::vec3{0.f, s.headAngles.y, 0.f});
    const glm::vec3 at = s.head + fwd * (0.9f * m2u) - glm::vec3{0.f, 0.f, 0.18f * m2u};
    text3d::queueOverlay(text, at, glm::vec3{0.f, s.headAngles.y, 0.f}, 0.045f);
}

void heightFrame(double now, std::string& text)
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
        Con_Printf("VR Calibration: that's low for standing. Playing seated? Set POSITION (the stand) to Seated, then press "
                   "START CALIBRATION\n");
    }
    flow.got = true;
    flow.gotAt = now;
    S_LocalSound("misc/menu2.wav");
}

void handFrame(double now, std::string& text)
{
    appendGold(text, "MAIN HAND");
    text += "\n";
    text += "Raise your MAIN hand (the one\nyou shoot with) high above your\nhead, and hold it there.\n";
    text += "The other hand's stick moves\nyou; it wears the wrist gadget.\n\n";
    if(flow.got)
    {
        text += "got it";
        if(now - flow.gotAt >= gotItSeconds)
        {
            finish();
        }
        return;
    }
    if(now - flow.start < readSeconds)
    {
        text += "get ready";
        return;
    }
    const TrackingState& t = tracking();
    const float eyes = t.head.position.y;
    int up = -1;
    for(const int h : {HAND_MAIN, HAND_OFF})
    {
        const int other = h == HAND_MAIN ? HAND_OFF : HAND_MAIN;
        if(t.hands[h].valid && t.hands[h].position.y > eyes + handAboveMetres &&
            (!t.hands[other].valid || t.hands[other].position.y < eyes))
        {
            up = h;
        }
    }
    if(up != flow.raised)
    {
        flow.raised = up;
        flow.raisedAt = now;
    }
    if(up < 0)
    {
        if(now - flow.start > handGiveUpSeconds)
        {
            Con_Printf("VR Calibration: main hand unchanged (%s)\n", vr_lefthanded.value != 0.f ? "left" : "right");
            flow.hand = vr_lefthanded.value != 0.f ? "Main hand: left (unchanged)" : "Main hand: right (unchanged)";
            flow.got = true;
            flow.gotAt = now;
            return;
        }
        text += "one hand up, the other down";
        return;
    }
    text += "hold it";
    if(now - flow.raisedAt < handHoldSeconds)
    {
        return;
    }
    // (HAND_OFF is the left controller whatever the setting: vr_backend.hpp.)
    const bool changed = (up == HAND_OFF) != (vr_lefthanded.value != 0.f);
    Cvar_SetValueQuick(&vr_lefthanded, up == HAND_OFF ? 1.f : 0.f);
    const char* side = vr_lefthanded.value != 0.f ? "left" : "right";
    Con_Printf("VR Calibration: main hand: %s%s\n", side, changed ? " (changed)" : "");
    flow.hand = vr_lefthanded.value != 0.f ? "Main hand: left" : "Main hand: right";
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
        begin();
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
    if(flow.step == Step::BodyReview)
    {
        if(key_dest != key_menu && bodycal::phase() != bodycal::Phase::Capturing)
        {
            enter(Step::Hand);
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
            stop("the menu");
            return;
        }
        afterBody();
        return;
    }
    if(key_dest == key_menu && flow.step != Step::Done)
    {
        stop("the menu");
        return;
    }

    std::string& text = scratch.text;
    text.clear();
    appendGold(text, "VR CALIBRATION");
    text += "\n";
    switch(flow.step)
    {
        case Step::Intro:
        {
            const int left = static_cast<int>(std::ceil(introSeconds - (now - flow.start)));
            text += "Height, then your body, then your main hand.\n\n";
            text += seated() ? "Seated: sit where you will play.\n" : "Stand in the middle of your play space.\n";
            text += "Playing seated? Press POSITION on the\nstand to your right: Seated.\n\n";
            text += va("starting in %d", std::max(left, 1));
            if(now - flow.start >= introSeconds)
            {
                enter(Step::Height);
            }
            break;
        }
        case Step::Height: heightFrame(now, text); break;
        case Step::Hand: handFrame(now, text); break;
        case Step::Done:
        {
            appendGold(text, "DONE");
            // The height as the body step left it (its first pose measures it again).
            text += va("\nHeight: eyes at %.2f m\n%s\n%s\n\n", vr_height_calibration.value, flow.body, flow.hand);
            text += "Explore the room: the buttons on the\nwalls change the main options, the\nboards say where the rest is.";
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
        text += flow.step == Step::Done ? "" : "\n\nmenu button: stop";
        drawText(text);
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
        begin();
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
        case Step::Intro: enter(Step::Height); break;
        case Step::Height: startBody(); break;
        case Step::Body:
            bodycal::stop(); // its result, if any, is left to its page
            enter(Step::Hand);
            break;
        case Step::BodyReview: enter(Step::Hand); break;
        case Step::Hand: finish(); break;
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
}

void frame()
{
    flowFrame();
    drawOptionScreens();
}

bool running()
{
    return flow.step != Step::Idle;
}

} // namespace qvr::setup
