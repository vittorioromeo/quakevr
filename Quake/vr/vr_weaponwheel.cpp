// vr_weaponwheel.cpp -- the headset's weapon wheel (vr_weaponwheel.hpp).
//
// The image is the flat holster menu's (gadget::renderHolsterMenu, the same render target: the two never show together,
// one is flatscreen play's, this a headset's): its slots laid out on a ring, the hovered one lit with the drop target's
// double frame, those a pick would not change greyed, a stroke from the ring's middle to the cursor. It is shown on a quad
// facing the eyes by the holster menu's own composite (flatholster::drawImage: its face, the CRT screen).
//
// A pick is the provider's; the holsters' sends the client command "vr_wheel_pick <hand> <slot>" (a remote server's
// too: sv_user.c lets it through), which runs QC VR_Wheel_Pick (vr_flatholster.qc) with `self` the player who sent it:
// the holster menu's and the number keys' own steps (VR_FlatHolster_DrawInto, _Put), the headset's weapon records,
// sounds, refusals and messages, and every player sees the weapons' stats and models change as ever.

#include "vr_weaponwheel.hpp"

#include "vr_api.h"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatholster.hpp"
#include "vr_flatplay.hpp"
#include "vr_gadget.hpp"
#include "vr_gfx.hpp"
#include "vr_main.hpp"
#include "vr_menuui.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_renderdoc.hpp"
#include "vr_testkit.hpp"
#include "vr_timescale.hpp"
#include "vr_toolgun.hpp"
#include "vr_weapon_ids.h"

#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Constants.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <stdio.h>
#include <stdlib.h>

namespace qvr::weaponwheel
{

namespace
{

// The image's virtual pixels (the holster menu's renderer: its header on top, its footer at the bottom), the ring's
// middle, its radius (the slots' middles), a slot's size, and where the hand counts as in the middle (nothing lit) or
// outside (let go: cancelled).
constexpr float panelW = 260.f;
constexpr float panelH = 276.f;
constexpr glm::vec2 ringMid{130.f, 142.f};
constexpr float ringR = 92.f;
constexpr float slotW = 70.f;
constexpr float slotH = 54.f;
constexpr float innerR = 26.f;
constexpr float outerR = ringR + 46.f;
constexpr float fadeSeconds = 0.1f;
constexpr int giveUp = 400; // the test: frames a step may wait

struct WheelState
{
    const Provider* provider{nullptr}; // setProvider's (nullptr: the holsters')
    int hand{-1};                      // open for this hand (-1 closed)
    int shownHand{0};                  // the last hand it opened for (its fade out)
    glm::vec3 centre{0.f}, right{0.f, -1.f, 0.f}, up{0.f, 0.f, 1.f}, normal{-1.f, 0.f, 0.f}; // the ring's middle; towards the eyes
    Choice choices[maxChoices];
    int count{0};
    int hovered{-1};      // the lit choice (-1 none)
    bool outside{false};  // the hand off the wheel: letting go cancels
    glm::vec2 cursor{ringMid};
    float fade{0.f};
    double lastTime{-1.0};
    unsigned texture{0}; // the image, rendered this frame (render)
    int ticks{0}, opens{0}, picks{0}, cancels{0}; // (vr_weaponwheel_dump, the test)
};
WheelState ws;

[[nodiscard]] const Provider& provider()
{
    return ws.provider ? *ws.provider : holsterProvider();
}

// ---------------------------------------------------------------------------------------------------------------------
// The holsters' provider: the holster menu's model (flatholster::holsterSlot), clockwise from the top: the empty hand,
// then the right shoulder, chest and hip, then the left hip, chest and shoulder (the body's own places round the hand).

constexpr int holsterOrder[] = {1, 5, 3, 2, 4, 0};

void copyText(char* to, size_t size, const char* from)
{
    q_strlcpy(to, from ? from : "", size);
}

int listHolsters(int hand, Choice* out, int max)
{
    const flatholster::HolsterSlot held = flatholster::handSlot(hand);
    const bool handFull = held.name[0] != 0;
    const bool immersive = vr_holster_mode.value == 0.f;
    int n = 0;
    if(n < max)
    {
        Choice& c = out[n++];
        c = Choice{};
        c.key = -1;
        copyText(c.title, sizeof(c.title), "EMPTY HAND");
        copyText(c.name, sizeof(c.name), handFull ? "put away" : "");
        c.usable = handFull;
    }
    for(const int i : holsterOrder)
    {
        if(n >= max)
        {
            break;
        }
        const flatholster::HolsterSlot s = flatholster::holsterSlot(i);
        Choice& c = out[n++];
        c = Choice{};
        c.key = i;
        copyText(c.title, sizeof(c.title), s.title);
        copyText(c.name, sizeof(c.name), s.name);
        copyText(c.model, sizeof(c.model), s.model);
        // An empty holster takes the hand's weapon (a quick slot's copy); a full one gives its own (immersive: the same
        // weapon already in the hand is a quick slot's copy only).
        c.usable = s.name[0] ? (immersive || q_strcasecmp(s.model, held.model) != 0) : handFull;
    }
    return n;
}

void pickHolster(int hand, const Choice& c)
{
    if(cls.state != ca_connected || cls.signon != SIGNONS)
    {
        return;
    }
    MSG_WriteByte(&cls.message, clc_stringcmd);
    MSG_WriteString(&cls.message, va("vr_wheel_pick %d %d", hand, c.key));
}

const Provider holsters{"holsters", listHolsters, pickHolster};

// ---------------------------------------------------------------------------------------------------------------------
// The layout.

[[nodiscard]] glm::vec2 slotMid(int i, int n)
{
    const float a = static_cast<float>(i) * za::tau / static_cast<float>(za::max(n, 1));
    return ringMid + glm::vec2{za::sin(a), -za::cos(a)} * ringR;
}

// The choice under panel point `p` (-1 none: the middle, or outside: `outside`).
[[nodiscard]] int choiceAt(const glm::vec2& p, bool& outside)
{
    const glm::vec2 d = p - ringMid;
    const float r = glm::length(d);
    outside = r > outerR || p.x < 0.f || p.y < 0.f || p.x > panelW || p.y > panelH;
    if(outside || r < innerR || ws.count <= 0)
    {
        return -1;
    }
    float a = za::atan2(d.x, -d.y); // (clockwise from the top)
    if(a < 0.f)
    {
        a += za::tau;
    }
    const float step = za::tau / static_cast<float>(ws.count);
    return static_cast<int>(za::floor(a / step + 0.5f)) % ws.count;
}

[[nodiscard]] float unitsPerPixel()
{
    return za::clamp(vr_weaponwheel_size.value, 4.f, 80.f) / panelW;
}

// Panel point `p` in the world (on the ring's plane; `lift` units towards the eyes).
[[nodiscard]] glm::vec3 worldAt(const glm::vec2& p, float lift = 0.f)
{
    const float k = unitsPerPixel();
    return ws.centre + ws.right * ((p.x - ringMid.x) * k) - ws.up * ((p.y - ringMid.y) * k) + ws.normal * lift;
}

// The hand's point projected onto the ring's plane, in panel pixels.
[[nodiscard]] glm::vec2 cursorOf(const glm::vec3& hand)
{
    const glm::vec3 d = hand - ws.centre;
    const float k = unitsPerPixel();
    return ringMid + glm::vec2{glm::dot(d, ws.right), -glm::dot(d, ws.up)} / k;
}

void tick(int hand)
{
    ws.ticks++;
    haptics::pulse(hand, 0.012f, 0.35f, 180.f);
}

[[nodiscard]] int handWeapon(int hand)
{
    return cl.stats[hand == HAND_MAIN ? protocol::STAT_QVR_WEAPON : protocol::STAT_QVR_WEAPON2];
}

void close(bool picked)
{
    (picked ? ws.picks : ws.cancels)++;
    ws.hand = -1;
}

// ---------------------------------------------------------------------------------------------------------------------
// vr_weaponwheel_test (Misc/quakevr/weaponwheel_test.sh; a headset's game, the mock's): the wheel driven by the mock's
// buttons and hands (vr_mock_button, vr_mock_hand_to onto a slot's place in the world), each step waiting on what it
// changes (at most giveUp frames: FAIL "timed out"). Single player: a nailgun in the right hand (gripped) and a shotgun
// in the right shoulder holster; B held opens it round the right hand, time slows; the hand onto the shoulder's slot
// (lit, a tick), let go: the shotgun in the right hand, the nailgun in a holster; the left hand's Y, the hand moved off
// the wheel, let go: nothing changes; the left hand (its grip not held) takes the nailgun: still held a while later; the
// right hand's EMPTY HAND: the shotgun back in its shoulder; time back at 1. "mp": only the first pick (the MP test's
// other player checks what he sees). Ends "weaponwheel_test: N passed, M failed".
struct Test
{
    bool on{false};
    bool mp{false};
    int step{0};
    int waited{0};
    int mark{0}; // a step's own count
    int nailgunAt{-1};
    testkit::Tally t;
};
Test test;

void result(bool ok, const char* what)
{
    test.t.check(ok);
    Con_Printf("weaponwheel_test: %s %s\n", ok ? "PASS" : "FAIL", what);
}

void finish()
{
    test.on = false;
    Cbuf_AddText("vr_mock_button main secondary 0; vr_mock_button off secondary 0\n");
    test.t.verdict("weaponwheel_test");
}

[[nodiscard]] const char* handName(int hand)
{
    return hand == HAND_MAIN ? "main" : "off";
}

[[nodiscard]] int holsterStat(int i)
{
    return cl.stats[protocol::STAT_QVR_HOLSTERWEAPON0 + i];
}

[[nodiscard]] int choiceOfKey(int key)
{
    for(int i = 0; i < ws.count; i++)
    {
        if(ws.choices[i].key == key)
        {
            return i;
        }
    }
    return -1;
}

// The mock's hand `hand` onto choice `i`'s slot (-1: off the wheel, to its left), at the hand's own depth.
void aimAt(int hand, int i)
{
    const glm::vec2 p = i >= 0 ? slotMid(i, ws.count) : ringMid - glm::vec2{outerR + 30.f, 0.f};
    const glm::vec3 w = worldAt(p, za::max(vr_weaponwheel_distance.value, 0.f));
    Cbuf_AddText(va("vr_mock_hand_to %s %.2f %.2f %.2f\n", handName(hand), w.x, w.y, w.z));
}

void press(int hand, bool down)
{
    const char* button = buttonKind() == 1 ? "stickclick" : buttonKind() == 2 ? "primary" : "secondary";
    Cbuf_AddText(va("vr_mock_button %s %s %d\n", handName(hand), button, down ? 1 : 0));
}

// The step waits until `ok`; then the next step (true). Past giveUp frames: FAIL "timed out" and the end.
[[nodiscard]] bool waitFor(bool ok, const char* what)
{
    if(ok)
    {
        test.step++;
        test.waited = 0;
        test.mark = 0;
        return true;
    }
    if(++test.waited > giveUp)
    {
        result(false, va("timed out: %s", what));
        finish();
    }
    return false;
}

void testStep()
{
    if(!test.on)
    {
        return;
    }
    const int main = HAND_MAIN, off = HAND_OFF;
    switch(test.step)
    {
    case 0:
        if(waitFor(cls.signon == SIGNONS && cl.stats[STAT_HEALTH] > 0 && !cl.intermission, "the game up"))
        {
            // (one impulse a frame: a move carries one)
            // (the hands held out in front at the chest, where a player opens it: in view)
            Cbuf_AddText("vr_mock_hand main 0.15 1.35 -0.4 0 0 0; vr_mock_hand off -0.15 1.35 -0.4 0 0 0; vr_mock_look 35 0\n");
            Cbuf_AddText(test.mp ? "vr_mock_button main grip 1; impulse 91\n"
                                 : "vr_mock_button main grip 1; vr_mock_button off grip 0; impulse 156\n");
        }
        break;
    case 1:
        if(!test.mp && test.mark == 0 && handWeapon(main) == WID_NAILGUN)
        {
            test.mark = 1;
            Cbuf_AddText("impulse 91\n");
        }
        if(waitFor(holsterStat(1) == WID_SHOTGUN && (test.mp || handWeapon(main) == WID_NAILGUN), "the shotgun holstered"))
        {
            press(main, true);
        }
        break;
    case 2:
    {
        const int frames = ++test.mark;
        if(waitFor(ws.hand == main, "the wheel open round the right hand"))
        {
            result(frames > 1, va("held, the wheel opens round that hand (after %d frames)", frames));
            result(ws.count == 7, va("seven slots: the empty hand and the six holsters (%d)", ws.count));
        }
        break;
    }
    case 3:
        // (single player only: a server keeps its own time)
        if(waitFor(test.mp || timescale::current() <= za::clamp(vr_weaponwheel_timescale.value, 0.05f, 1.f) + 0.02f,
               "time slowed") &&
            !test.mp)
        {
            result(true, va("time slowed while open (x%.2f)", timescale::current()));
        }
        break;
    case 4:
    {
        const int want = choiceOfKey(1);
        aimAt(main, want);
        if(waitFor(want >= 0 && ws.hovered == want, "the right hand onto the right shoulder's slot"))
        {
            result(ws.ticks > 1, va("the slot lit (%s), a tick (%d so far)", ws.choices[want].title, ws.ticks));
            Cbuf_AddText("screenshot\n");
        }
        break;
    }
    case 5:
        // (a few counted frames: the screenshot's image drawn first)
        aimAt(main, choiceOfKey(1));
        if(waitFor(++test.mark >= 3, "the screenshot"))
        {
            press(main, false);
        }
        break;
    case 6:
        if(waitFor(ws.hand < 0 && handWeapon(main) == WID_SHOTGUN && holsterStat(1) == WID_FIST, "the shotgun in the right hand"))
        {
            result(true, "let go on the right shoulder: the shotgun in the right hand, its holster empty");
            if(test.mp)
            {
                finish();
                return;
            }
            for(int i = 0; i < flatholster::holsterSlots; i++)
            {
                test.nailgunAt = holsterStat(i) == WID_NAILGUN ? i : test.nailgunAt;
            }
            if(!test.t.check(test.nailgunAt >= 0, "weaponwheel_test", "the nailgun put away in a holster"))
            {
                finish();
                return;
            }
            Con_Printf("weaponwheel_test: PASS the nailgun put away in a holster (%d)\n", test.nailgunAt);
            press(off, true);
        }
        break;
    case 7:
        if(waitFor(ws.hand == off, "the wheel open round the left hand"))
        {
            aimAt(off, -1);
        }
        break;
    case 8:
        aimAt(off, -1);
        if(waitFor(ws.outside, "the left hand off the wheel"))
        {
            result(ws.hovered < 0, "off the wheel: nothing lit");
            Cbuf_AddText("screenshot\n");
        }
        break;
    case 9:
        if(waitFor(++test.mark >= 3, "the screenshot"))
        {
            press(off, false);
        }
        break;
    case 10:
        // (counted frames for a pick's round trip: nothing must change)
        if(waitFor(++test.mark >= 30, "a pick's time"))
        {
            result(ws.hand < 0 && handWeapon(off) == WID_FIST && holsterStat(test.nailgunAt) == WID_NAILGUN && ws.cancels >= 1,
                "let go off the wheel: cancelled, nothing moved");
            press(off, true);
        }
        break;
    case 11:
        if(waitFor(ws.hand == off, "the wheel open round the left hand again"))
        {
            aimAt(off, choiceOfKey(test.nailgunAt));
        }
        break;
    case 12:
    {
        const int want = choiceOfKey(test.nailgunAt);
        aimAt(off, want);
        if(waitFor(want >= 0 && ws.hovered == want, "the left hand onto the nailgun's slot"))
        {
            press(off, false);
        }
        break;
    }
    case 13:
        if(waitFor(handWeapon(off) == WID_NAILGUN && holsterStat(test.nailgunAt) == WID_FIST, "the nailgun in the left hand"))
        {
            result(true, "the left hand (either hand) takes the nailgun from its holster");
        }
        break;
    case 14:
        // (counted frames: a weapon its grip does not hold falls at once; the wheel's stays until the grip is pressed)
        if(waitFor(++test.mark >= 40, "a while"))
        {
            result(handWeapon(off) == WID_NAILGUN, "the left hand keeps it, its grip not held");
            press(main, true);
        }
        break;
    case 15:
        (void)waitFor(ws.hand == main, "the wheel open round the right hand again");
        break;
    case 16:
    {
        const int want = choiceOfKey(-1);
        aimAt(main, want);
        if(waitFor(want >= 0 && ws.hovered == want, "the right hand onto EMPTY HAND"))
        {
            press(main, false);
        }
        break;
    }
    case 17:
        if(waitFor(ws.hand < 0 && handWeapon(main) == WID_FIST && holsterStat(1) == WID_SHOTGUN, "the shotgun put away"))
        {
            result(true, "EMPTY HAND: the shotgun back in the holster it came from");
        }
        break;
    case 18:
        if(waitFor(timescale::current() >= 0.99f, "time back at 1"))
        {
            result(true, "closed: time back at 1");
            finish();
        }
        break;
    default:
        finish();
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Commands.

// "vr_wheel_pick <hand> <slot>": a client's pick (the holsters' provider), on the server: QC VR_Wheel_Pick with `self`
// his player.
void serverPick_f()
{
    if(cmd_source != src_client || !sv.active || !host_client || !host_client->edict || Cmd_Argc() != 3)
    {
        return;
    }
    const int hand = atoi(Cmd_Argv(1)), slot = atoi(Cmd_Argv(2));
    if((hand != 0 && hand != 1) || slot < -1 || slot >= flatholster::holsterSlots)
    {
        return;
    }
    const progs::ServerVm vm;
    const func_t fn = progs::findFunction("VR_Wheel_Pick");
    if(!fn)
    {
        return;
    }
    const int self = pr_global_struct->self;
    const float parm[2]{G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1)};
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
    G_FLOAT(OFS_PARM0) = static_cast<float>(hand);
    G_FLOAT(OFS_PARM1) = static_cast<float>(slot);
    PR_ExecuteProgram(fn);
    pr_global_struct->self = self;
    G_FLOAT(OFS_PARM0) = parm[0], G_FLOAT(OFS_PARM1) = parm[1];
}

void dump_f()
{
    Con_Printf("weapon wheel: %s (hand %d), provider %s, %d choices, hovered %d%s, cursor %.0f %.0f, fade %.2f, time x%.2f; "
               "opens %d, picks %d, cancels %d, ticks %d\n",
        ws.hand >= 0 ? "open" : "closed", ws.hand, provider().name, ws.count, ws.hovered, ws.outside ? " (outside)" : "",
        ws.cursor.x, ws.cursor.y, ws.fade, timeScale(), ws.opens, ws.picks, ws.cancels, ws.ticks);
    for(int i = 0; i < ws.count; i++)
    {
        const Choice& c = ws.choices[i];
        Con_Printf("  %d: key %d, %s: %s%s\n", i, c.key, c.title, c.name[0] ? c.name : "(empty)", c.usable ? "" : " (greyed)");
    }
}

void test_f()
{
    test = Test{};
    test.on = true;
    test.mp = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "mp");
    Con_Printf("weaponwheel_test: start%s\n", test.mp ? " (mp)" : "");
}

} // namespace

const Provider& holsterProvider()
{
    return holsters;
}

void setProvider(const Provider* p)
{
    ws.provider = p;
}

int buttonKind()
{
    return za::clamp(static_cast<int>(vr_weaponwheel_button.value), 0, 2);
}

double holdSeconds()
{
    return za::clamp(static_cast<double>(vr_weaponwheel_hold.value), 0.0, 2.0);
}

bool canOpen(int hand)
{
    if(vr_weaponwheel.value == 0.f || !vrActive() || flatplay::active() || key_dest != key_game || cls.signon != SIGNONS ||
        cl.stats[STAT_HEALTH] <= 0 || cl.intermission || ws.hand >= 0 || toolgun::heldHand() == hand)
    {
        return false;
    }
    if(buttonKind() == 0)
    {
        // B and Y: the grappling hook's reel while it is in that hand; vr_menu_button 2 (Y) and 3 (B) hold them for the menu.
        const int menuHold = static_cast<int>(vr_menu_button.value);
        if(handWeapon(hand) == WID_GRAPPLE || (menuHold == 2 && hand == HAND_OFF) || (menuHold == 3 && hand == HAND_MAIN))
        {
            return false;
        }
    }
    return true;
}

void open(int hand)
{
    const hands::State& s = hands::current();
    if(!canOpen(hand) || !s.valid)
    {
        return;
    }
    ws.hand = ws.shownHand = hand;
    ws.count = provider().list(hand, ws.choices, maxChoices);
    ws.hovered = -1;
    ws.outside = false;
    ws.cursor = ringMid;
    // Beyond the hand from the eyes, facing them (upright: its up the world's, leaned to face them).
    const glm::vec3 at = s.pos[hand];
    glm::vec3 away = at - s.head;
    away = glm::length(away) > 0.01f ? glm::normalize(away) : glm::vec3{1.f, 0.f, 0.f};
    ws.centre = at + away * za::max(vr_weaponwheel_distance.value, 0.f);
    ws.normal = -away;
    const glm::vec3 side = glm::cross(glm::vec3{0.f, 0.f, 1.f}, ws.normal);
    ws.right = glm::length(side) > 0.01f ? glm::normalize(side) : glm::vec3{0.f, -1.f, 0.f};
    ws.up = glm::cross(ws.normal, ws.right);
    ws.opens++;
    menuui::uiSound(menuui::UiSound::Move);
    tick(hand);
    Con_DPrintf("weapon wheel: opens round the %s hand (%d choices)\n", hand == HAND_MAIN ? "right" : "left", ws.count);
}

void release(int hand)
{
    if(ws.hand != hand)
    {
        return;
    }
    if(ws.hovered >= 0 && ws.hovered < ws.count && ws.choices[ws.hovered].usable)
    {
        const Choice& c = ws.choices[ws.hovered];
        Con_DPrintf("weapon wheel: the %s hand takes %s (%s)\n", hand == HAND_MAIN ? "right" : "left", c.title,
            c.name[0] ? c.name : "empty");
        provider().pick(hand, c);
        menuui::uiSound(menuui::UiSound::Select);
        close(true);
        return;
    }
    Con_DPrintf("weapon wheel: let go %s (cursor %.0f %.0f): nothing\n",
        ws.outside ? "off the wheel" : ws.hovered >= 0 ? "on a greyed slot" : "in the middle", ws.cursor.x, ws.cursor.y);
    close(false);
}

bool isOpen()
{
    return ws.hand >= 0;
}

float timeScale()
{
    return ws.hand >= 0 ? za::clamp(vr_weaponwheel_timescale.value, 0.05f, 1.f) : 1.f;
}

void frame(const hands::State& s)
{
    const double now = realtime;
    const float dt = ws.lastTime >= 0.0 ? za::clamp(static_cast<float>(now - ws.lastTime), 0.f, 0.1f) : 0.f;
    ws.lastTime = now;
    if(ws.hand >= 0 && (vr_weaponwheel.value == 0.f || key_dest != key_game || cls.signon != SIGNONS ||
                           cl.stats[STAT_HEALTH] <= 0 || cl.intermission || !s.valid))
    {
        close(false); // (a menu, death, the level's end: closed, its button's letting go does nothing)
    }
    if(ws.hand >= 0)
    {
        ws.count = provider().list(ws.hand, ws.choices, maxChoices); // (what the holsters hold now)
        ws.cursor = cursorOf(s.pos[ws.hand]);
        const int was = ws.hovered;
        ws.hovered = choiceAt(ws.cursor, ws.outside);
        if(ws.hovered >= 0 && ws.hovered != was)
        {
            tick(ws.hand);
        }
    }
    ws.fade = za::clamp(ws.fade + (ws.hand >= 0 ? dt : -dt) / fadeSeconds, 0.f, 1.f);
    testStep();
}

void render()
{
    ws.texture = 0;
    if(ws.fade <= 0.f || ws.count <= 0)
    {
        return;
    }
    gadget::HolsterMenuSlot slots[maxChoices]{};
    using Drop = gadget::HolsterMenuSlot::Drop;
    for(int i = 0; i < ws.count; i++)
    {
        const Choice& c = ws.choices[i];
        gadget::HolsterMenuSlot& s = slots[i];
        const glm::vec2 mid = slotMid(i, ws.count);
        s.rect = {mid.x - slotW * 0.5f, mid.y - slotH * 0.5f, slotW, slotH};
        s.title = c.title;
        s.name = c.name;
        s.model = c.model;
        s.hovered = i == ws.hovered;
        s.drop = s.hovered && c.usable ? Drop::Valid : c.usable ? Drop::None : Drop::Invalid;
    }
    // The middle's cross, and a stroke from it to the hand (outside: to the wheel's edge that way).
    glm::vec2 to = ws.cursor - ringMid;
    const float len = glm::length(to);
    if(len > outerR)
    {
        to *= outerR / len;
    }
    const glm::vec4 lines[3] = {{ringMid.x - 6.f, ringMid.y, ringMid.x + 6.f, ringMid.y},
        {ringMid.x, ringMid.y - 6.f, ringMid.x, ringMid.y + 6.f}, {ringMid.x, ringMid.y, ringMid.x + to.x, ringMid.y + to.y}};
    gadget::HolsterMenuView v;
    v.size = {panelW, panelH};
    v.slots = slots;
    v.slotCount = ws.count;
    v.lines = lines;
    v.lineCount = len > innerR * 0.5f ? 3 : 2;
    v.header = ws.shownHand == HAND_MAIN ? "WEAPON WHEEL - RIGHT HAND" : "WEAPON WHEEL - LEFT HAND";
    const bool lit = ws.hovered >= 0 && ws.choices[ws.hovered].usable;
    v.footer = lit ? "Let go: take it" : ws.outside ? "Let go here: cancel" : "Move the hand to a slot";
    const gadget::VisorImage img = gadget::renderHolsterMenu(v);
    ws.texture = img.shown ? img.texture : 0;
}

void drawInEye(const hands::State& s)
{
    if(!ws.texture || ws.fade <= 0.f || !s.valid || renderdoc::hidingPanel())
    {
        return;
    }
    const glm::vec3 corner[4] = {worldAt({0.f, panelH}), worldAt({panelW, panelH}), worldAt({panelW, 0.f}), worldAt({0.f, 0.f})};
    flatholster::drawImage(ws.texture, {panelW, panelH}, corner, gfx::sceneViewProjection(), ws.fade);
}

void registerCommands()
{
    Cmd_AddCommand_ClientCommand("vr_wheel_pick", serverPick_f);
    Cmd_AddCommand("vr_weaponwheel_dump", dump_f);
    Cmd_AddCommand("vr_weaponwheel_test", test_f);
}

} // namespace qvr::weaponwheel
