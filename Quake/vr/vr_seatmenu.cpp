// vr_seatmenu.cpp -- splitscreen phase 9 (docs/vr-port/SPLITSCREEN.md): menus per seat.
//
// His answer (section 7, question 4): customizable; by default in coop opening the menu pauses the game and only seat 1
// opens the full menu, the other seats their own small menu. So:
//   - seat 1 (index 0: the keyboard and mouse, and the pad no seat has) has the full menu and the console, as always
//     (key_dest is his);
//   - a seat 2-4's Start opens its seat menu, drawn in its tile and driven by its own pad only (keys.c routes a seat's
//     keys by Key_SeatDest: its seat menu, or the game; never the full menu or the console): Resume, Look Sensitivity
//     and Invert Look (the seat's own, on its stick), Controls (its pad's buttons), Controller Bindings (split20: the
//     Controller Bindings screen for its own pad's bindings, vr_bindsetup.cpp, in its tile), Leave Seat (asked twice);
//   - scr_splitmenu (archived, default 0): 1 gives every seat's Start the full menu (one menu, as before phase 9);
//   - scr_splitpause (archived, default 1): a menu open (seat 1's full menu or any seat menu) holds the local game while
//     more than one seat plays: 0 never, 1 in coop (deathmatch runs on), 2 in coop and deathmatch. Only a game of local
//     players pauses (a network player in it: no pause).
// One seat in play: nothing here acts (Key_SeatDest is key_dest, no pause, the look exactly as before).
//
// vr_seat_menu_test (Misc/quakevr/seat_menu_test.sh: coop and deathmatch): the full menu by pad 1 with one seat; seat 2's
// Start opens its seat menu (not the full menu), its pad drives it (cursor, sensitivity, invert), its stick walks no one
// while it is open; seat 1's pad opens the full menu meanwhile and each pad drives its own seat's menu; the pause by mode
// and option; the console stays seat 1's; scr_splitmenu 1 gives seat 2 the full menu; Leave Seat. Verdict
// "seat_menu_test: N passed, M failed".
#include "vr_seat.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"

#include "vr_bindpanel.hpp"
#include "vr_bindsetup.hpp"
#include "vr_engine.hpp"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN
#include "vr_flatholster.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatprocreload.hpp"
#include "vr_testkit.hpp"
#include "vr_visor.hpp"

#include <SDL.h>

cvar_t scr_splitmenu = {"scr_splitmenu", "0", CVAR_ARCHIVE};   // 0: seats 2-4 open their seat menu; 1: the full menu
cvar_t scr_splitpause = {"scr_splitpause", "1", CVAR_ARCHIVE}; // a menu holds the game: 0 never, 1 coop, 2 coop and dm

namespace qvr::seat
{

namespace
{

enum Item
{
    ItemResume,
    ItemSens,
    ItemInvert,
    ItemControls,
    ItemBindings, // split20: the Controller Bindings screen, the seat's own (bindsetup::openInSeat)
    ItemLeave,
    ItemCount
};

constexpr float sensStep = 0.25f;
constexpr float sensMin = 0.25f;
constexpr float sensMax = 4.f;

struct SeatMenuState
{
    bool open = false;
    int cursor = ItemResume;
    bool controls = false;     // the Controls page shown
    bool bindings = false;     // split20: the seat's Controller Bindings screen shown (its pad drives it)
    bool confirmLeave = false; // Leave Seat asked once: A again leaves, anything else not
    bool leaveArmed = false;   // A pressed on the question: the seat leaves on its release (no press carried to seat 1)
    float lookScale = 1.f;     // the seat's stick look sensitivity (its own; phase 12's profiles take the cvars)
    bool invertLook = false;   // its own invert, on top of joy_invert
};

[[nodiscard]] SeatMenuState& stateOf(const int s)
{
    return part<SeatMenuState>(at(s), Part::SeatMenu);
}

[[nodiscard]] bool seatMenus()
{
    return scr_splitmenu.value == 0.f && inPlay() > 1;
}

void shut(SeatMenuState& m)
{
    m.open = false;
    m.controls = false;
    m.bindings = false;
    m.confirmLeave = false;
    m.leaveArmed = false;
}

void openMenu(const int s)
{
    SeatMenuState& m = stateOf(s);
    shut(m);
    m.open = true;
    m.cursor = ItemResume;
    CL_ReleaseMoveButtons(); // (the current seat's: what it held lets go, as the full menu does)
}

[[nodiscard]] bool isKey(const int key, const int a, const int b)
{
    return key == a || key == b;
}

void menuKeyDown(const int s, const int key)
{
    SeatMenuState& m = stateOf(s);
    const bool back = isKey(key, K_ESCAPE, K_BBUTTON);
    const bool enter = isKey(key, K_ENTER, K_ABUTTON) || key == K_KP_ENTER;
    if(m.bindings)
    {
        m.bindings = bindsetup::seatKey(key); // (the seat's own bindings; Back returns here)
        return;
    }
    if(m.controls)
    {
        if(back || enter)
        {
            m.controls = false;
        }
        return;
    }
    if(m.confirmLeave)
    {
        if(enter)
        {
            m.leaveArmed = true; // (on the release: VR_SeatMenuKey)
        }
        else
        {
            m.confirmLeave = false;
        }
        return;
    }
    if(back)
    {
        shut(m);
        return;
    }
    if(isKey(key, K_UPARROW, K_DPAD_UP))
    {
        m.cursor = (m.cursor + ItemCount - 1) % ItemCount;
    }
    else if(isKey(key, K_DOWNARROW, K_DPAD_DOWN))
    {
        m.cursor = (m.cursor + 1) % ItemCount;
    }
    else if(isKey(key, K_LEFTARROW, K_DPAD_LEFT) || isKey(key, K_RIGHTARROW, K_DPAD_RIGHT) || enter)
    {
        const int dir = isKey(key, K_LEFTARROW, K_DPAD_LEFT) ? -1 : 1;
        switch(m.cursor)
        {
            case ItemResume:
                if(enter)
                {
                    shut(m);
                }
                break;
            case ItemSens:
                m.lookScale += static_cast<float>(dir) * sensStep;
                m.lookScale = m.lookScale > sensMax ? (enter ? sensMin : sensMax) : m.lookScale < sensMin ? sensMin : m.lookScale;
                break;
            case ItemInvert: m.invertLook = !m.invertLook; break;
            case ItemControls:
                if(enter)
                {
                    m.controls = true;
                }
                break;
            case ItemBindings:
                if(enter)
                {
                    m.bindings = bindsetup::openInSeat();
                }
                break;
            case ItemLeave:
                if(enter)
                {
                    m.confirmLeave = true;
                }
                break;
            default: break;
        }
    }
}

void drawItem(const int y, const char* label, const char* value, const bool selected)
{
    M_Print(88, y, label);
    if(value)
    {
        M_PrintWhite(232, y, value);
    }
    if(selected)
    {
        M_DrawCharacter(76, y, 12 + (static_cast<int>(realtime * 4) & 1));
    }
}

void drawMenu(const int s, const SeatMenuState& m)
{
    GL_SetCanvas(CANVAS_MENU);
    if(m.bindings && bindsetup::seatScreenOpen())
    {
        bindsetup::seatDraw(); // (the seat's current: its own bindings, its pad's picture)
        return;
    }
    if(m.controls)
    {
        // split11: flat play's whole controller layout (seat::padLayout), each with the seat's own button for it
        int count = 0;
        const PadAction* layout = padLayout(count);
        M_DrawTextBox(32, 20, 30, count + 4);
        M_PrintWhite(48, 30, va("seat %d: controls", s + 1));
        int y = 44;
        for(int i = 0; i < count; i++)
        {
            int keys[1] = {-1};
            const bool bound = Key_GetKeysForCommand(layout[i].command, keys, 1, KDM_GAMEPAD) > 0;
            M_Print(48, y, layout[i].label);
            M_PrintWhite(208, y, bound ? Key_KeynumToFriendlyString(keys[0]) : "-");
            y += 8;
        }
        M_Print(48, y + 4, "B: back");
        return;
    }
    M_DrawTextBox(64, 48, 24, 11);
    M_PrintWhite(80, 60, va("seat %d", s + 1));
    drawItem(76, "Resume", nullptr, m.cursor == ItemResume);
    drawItem(88, "Look Sensitivity", va("x%.2f", static_cast<double>(m.lookScale)), m.cursor == ItemSens);
    drawItem(100, "Invert Look", m.invertLook ? "on" : "off", m.cursor == ItemInvert);
    drawItem(112, "Controls", nullptr, m.cursor == ItemControls);
    drawItem(124, "Controller Bindings", nullptr, m.cursor == ItemBindings);
    drawItem(136, "Leave Seat", nullptr, m.cursor == ItemLeave);
    if(m.confirmLeave)
    {
        M_PrintWhite(80, 152, "leave? A: yes  B: no");
    }
    else if(VR_SeatMenusPause())
    {
        M_Print(80, 152, "the game is paused");
    }
}

// The seat whose menu holds the game (for the other tiles' note; -1: seat 1's full menu or none).
[[nodiscard]] int holdingSeat()
{
    for(int s = 1; s < maxSeats; s++)
    {
        if(plays(s) && stateOf(s).open)
        {
            return s;
        }
    }
    return -1;
}

void menuCommand_f()
{
    const int s = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) - 1 : -1;
    if(s < 1 || s >= maxSeats || !plays(s))
    {
        Con_Printf("seat_menu <2-%d> : that seat's own menu opened or shut (its Start; a seat that plays)\n", maxSeats);
        return;
    }
    if(stateOf(s).open)
    {
        shut(stateOf(s));
        return;
    }
    const Scope in(at(s));
    openMenu(s);
}

} // namespace

void closeMenu(const int i)
{
    if(i > 0 && i < maxSeats)
    {
        shut(stateOf(i));
    }
}

} // namespace qvr::seat

extern "C" int VR_SeatMenusOn()
{
    return qvr::seat::seatMenus() ? 1 : 0;
}

extern "C" int VR_SeatMenuIsOpen(const int seat)
{
    using namespace qvr::seat;
    return seat > 0 && seat < maxSeats && plays(seat) && stateOf(seat).open ? 1 : 0;
}

extern "C" void VR_SeatMenuToggle()
{
    using namespace qvr::seat;
    const int s = current().index;
    if(s <= 0 || !plays(s))
    {
        return;
    }
    if(stateOf(s).open)
    {
        shut(stateOf(s));
    }
    else
    {
        openMenu(s);
    }
}

extern "C" void VR_SeatMenuKey(const int key, const int down)
{
    using namespace qvr::seat;
    const int s = current().index;
    if(s <= 0 || !plays(s))
    {
        return;
    }
    SeatMenuState& m = stateOf(s);
    if(down)
    {
        menuKeyDown(s, key);
        return;
    }
    if(m.leaveArmed && (isKey(key, K_ENTER, K_ABUTTON) || key == K_KP_ENTER))
    {
        shut(m);
        Cbuf_AddText(va("seat_leave %d\n", s + 1)); // (vr_seatplay.cpp's leave, as the console's)
    }
}

extern "C" void VR_SeatMenuDraw()
{
    using namespace qvr::seat;
    const int s = current().index;
    if(s > 0 && seatMenus() && plays(s) && stateOf(s).open)
    {
        drawMenu(s, stateOf(s));
        return;
    }
    if(VR_SeatMenusPause())
    {
        const int h = holdingSeat();
        const char* line = h > 0 ? va("paused: seat %d's menu", h + 1) : "paused";
        GL_SetCanvas(CANVAS_MENU);
        M_PrintWhite(160 - 4 * static_cast<int>(ZA_STRLEN(line)), 8, line);
    }
}

extern "C" int VR_SeatMenusPause()
{
    using namespace qvr::seat;
    if(inPlay() < 2 || !sv.active || !svs.clients || svs.maxclients < 2)
    {
        return 0;
    }
    const int mode = static_cast<int>(scr_splitpause.value);
    if(mode <= 0 || (deathmatch.value != 0.f && mode < 2))
    {
        return 0;
    }
    bool open = key_dest == key_menu; // (seat 1's full menu, or any seat's with scr_splitmenu 1)
    for(int s = 1; s < maxSeats && !open; s++)
    {
        open = seatMenus() && plays(s) && stateOf(s).open;
    }
    if(!open)
    {
        return 0;
    }
    for(int i = 0; i < svs.maxclients; i++)
    {
        const client_t& c = svs.clients[i];
        if(c.active && c.netconnection && !NET_QSocketIsLocal(c.netconnection))
        {
            return 0; // (a network player in the game: it runs on for him)
        }
    }
    return 1;
}

extern "C" float VR_SeatLookScale()
{
    using namespace qvr::seat;
    const int s = current().index;
    return s > 0 ? stateOf(s).lookScale : 1.f;
}

extern "C" int VR_SeatInvertLook()
{
    using namespace qvr::seat;
    const int s = current().index;
    return s > 0 && stateOf(s).invertLook ? 1 : 0;
}

// ---- vr_seat_menu_test ----------------------------------------------------------------------------------------------

namespace qvr::seat
{

namespace
{

// seat-shared: vr_seat_menu_test's own state machine
struct MenuTest
{
    testkit::Tally tally;
    bool on = false;
    int stage = 0;
    int frames = 0;      // in this stage (its give-up)
    int wait = 0;        // frames before the next stage (a press's down and up, a counted hold)
    int releasePad = -1; // the pad whose button lets go during the wait
    int releaseButton = -1;
    double svTime = 0.0; // the server's time when a hold started
    vec3_t origin{};     // seat 2's player where its stick was pushed
    bool dm = false;
} menuTest;

constexpr int seatPad = 0;          // the mock pad that joins seat 2
constexpr int hostPad = 1;          // the mock pad that drives seat 1 once seat 2 plays
constexpr int holdFrames = 12;      // a counted hold for the pause's checks (the server's time held or not)
constexpr int stageFrames = 1440;   // a stage's give-up (20 s of game time): a FAIL "timed out"

bool mcheck(const bool ok, const char* what)
{
    if(menuTest.tally.check(ok, "seat_menu_test", what))
    {
        Con_Printf("seat_menu_test: PASS %s\n", what);
    }
    return ok;
}

void press(const int pad, const int button)
{
    IN_MockPadButton(pad, button, true);
    menuTest.releasePad = pad;
    menuTest.releaseButton = button;
    menuTest.wait = 4;
}

void hold(const int frames)
{
    menuTest.svTime = sv.qcvm.time;
    menuTest.wait = frames;
}

[[nodiscard]] bool signedOn(const int s)
{
    const clientseat_t* n = CL_SeatNetIfMade(s);
    return n && n->cstatic.state == ca_connected && n->cstatic.signon == SIGNONS;
}

[[nodiscard]] edict_t* seatPlayer(const int s)
{
    for(int i = 0; sv.active && svs.clients && i < svs.maxclients; i++)
    {
        if(svs.clients[i].active && svs.clients[i].netconnection &&
            NET_QSocketLocalSeat(svs.clients[i].netconnection) == s)
        {
            return svs.clients[i].edict;
        }
    }
    return nullptr;
}

[[nodiscard]] keydest_t destOf(const int s)
{
    const Scope in(at(s));
    return Key_SeatDest();
}

void finish()
{
    MenuTest& t = menuTest;
    for(int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++)
    {
        IN_MockPadButton(seatPad, b, false);
        IN_MockPadButton(hostPad, b, false);
    }
    IN_MockPadAxes(seatPad, 0.f, 0.f, 0.f, 0.f);
    Cvar_Set("scr_splitmenu", "0");
    Cvar_Set("scr_splitpause", "1");
    if(plays(1))
    {
        Cbuf_AddText("seat_leave 2\n");
    }
    IN_MockPad(seatPad, false);
    IN_MockPad(hostPad, false);
    if(key_dest == key_menu)
    {
        key_dest = key_game;
    }
    t.on = false;
    t.tally.verdict("seat_menu_test");
}

void next()
{
    menuTest.stage++;
    menuTest.frames = 0;
}

[[nodiscard]] bool held()
{
    return sv.qcvm.time == menuTest.svTime;
}

void testStage()
{
    MenuTest& t = menuTest;
    SeatMenuState& m = stateOf(1);
    switch(t.stage)
    {
        case 0: // the local game in play: one seat, pad 1's Start opens the full menu as before
            if(!sv.active || !signedOn(0))
            {
                return;
            }
            t.dm = deathmatch.value != 0.f;
            IN_MockPad(seatPad, true);
            mcheck(IN_PadSeat(seatPad) == 0 && !VR_SeatMenusOn(), "one seat: pad 1 drives seat 1, no seat menus");
            press(seatPad, SDL_CONTROLLER_BUTTON_START);
            next();
            return;
        case 1:
            mcheck(key_dest == key_menu && !VR_SeatMenusPause(), "one seat: its Start opens the full menu (no splitscreen pause)");
            press(seatPad, SDL_CONTROLLER_BUTTON_START);
            next();
            return;
        case 2:
            mcheck(key_dest == key_game, "one seat: Start again shuts it");
            Cbuf_AddText(va("seat_join %d\n", seatPad));
            next();
            return;
        case 3: // seat 2 in: its Start opens its own menu
            if(!signedOn(1))
            {
                return;
            }
            IN_MockPad(hostPad, true);
            mcheck(IN_PadSeat(seatPad) == 1 && IN_PadSeat(hostPad) == 0, "two seats: pad 1 drives seat 2, pad 2 seat 1");
            press(seatPad, SDL_CONTROLLER_BUTTON_START);
            next();
            return;
        case 4:
            mcheck(m.open && m.cursor == ItemResume, "seat 2's Start opens seat 2's own menu");
            mcheck(key_dest == key_game && destOf(0) == key_game && destOf(1) == key_menu,
                "the full menu stays shut: seat 1 plays, seat 2's keys go to its menu");
            hold(holdFrames);
            next();
            return;
        case 5:
            if(t.dm)
            {
                mcheck(!held() && !VR_SeatMenusPause(), va("deathmatch: the game runs on (server time +%.3f)", sv.qcvm.time - t.svTime));
            }
            else
            {
                mcheck(held() && VR_SeatMenusPause(), "coop: seat 2's menu pauses the game (server time held)");
            }
            press(seatPad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            next();
            return;
        case 6:
            mcheck(m.cursor == ItemSens, "seat 2's d-pad moves its cursor");
            press(seatPad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            next();
            return;
        case 7:
            mcheck(za::fabs(m.lookScale - 1.25f) < 0.001f, va("seat 2's look sensitivity up (x%.2f)", static_cast<double>(m.lookScale)));
            {
                const Scope in(at(1));
                mcheck(za::fabs(VR_SeatLookScale() - 1.25f) < 0.001f, "the stick's look scale is seat 2's");
            }
            mcheck(VR_SeatLookScale() == 1.f && !VR_SeatInvertLook(), "seat 1's look exactly as before (x1, not inverted)");
            press(seatPad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            next();
            return;
        case 8:
            press(seatPad, SDL_CONTROLLER_BUTTON_A);
            next();
            return;
        case 9:
        {
            mcheck(m.invertLook && m.open, "seat 2's A toggles its Invert Look");
            edict_t* p = seatPlayer(1);
            if(p)
            {
                VectorCopy(p->v.origin, t.origin);
            }
            IN_MockPadAxes(seatPad, 0.f, -1.f, 0.f, 0.f); // (the stick forward while its menu is open)
            hold(holdFrames * 3);
            next();
            return;
        }
        case 10:
        {
            edict_t* p = seatPlayer(1);
            const float d = p ? za::sqrt((p->v.origin[0] - t.origin[0]) * (p->v.origin[0] - t.origin[0]) +
                                          (p->v.origin[1] - t.origin[1]) * (p->v.origin[1] - t.origin[1]))
                              : -1.f;
            IN_MockPadAxes(seatPad, 0.f, 0.f, 0.f, 0.f);
            mcheck(p && d < 1.f, va("seat 2's stick walks no one while its menu is open (%.1f units)", static_cast<double>(d)));
            mcheck(m.cursor == ItemSens && key_dest == key_game,
                va("its stick's up arrow moves its own menu's cursor (to %d), not seat 1's", m.cursor));
            press(hostPad, SDL_CONTROLLER_BUTTON_START);
            next();
            return;
        }
        case 11:
            mcheck(key_dest == key_menu && m.open, "seat 1's pad opens the full menu; seat 2's menu stays its own");
            press(seatPad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            next();
            return;
        case 12:
            mcheck(m.cursor == ItemInvert && key_dest == key_menu, "pad 1's d-pad drives seat 2's menu under the full menu");
            press(hostPad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            next();
            return;
        case 13:
            mcheck(m.cursor == ItemInvert && key_dest == key_menu, "pad 2's d-pad (seat 1's) leaves seat 2's menu alone");
            press(seatPad, SDL_CONTROLLER_BUTTON_B);
            next();
            return;
        case 14:
            mcheck(!m.open && key_dest == key_menu, "seat 2's B shuts its menu only");
            mcheck(VR_SeatMenusPause() == (t.dm ? 0 : 1), t.dm ? "deathmatch: seat 1's menu doesn't pause" : "coop: seat 1's full menu pauses");
            press(hostPad, SDL_CONTROLLER_BUTTON_START);
            next();
            return;
        case 15:
            mcheck(key_dest == key_game && !VR_SeatMenusPause(), "seat 1's Start shuts the full menu: no pause");
            hold(holdFrames);
            next();
            return;
        case 16:
            mcheck(!held(), "the game runs again");
            {
                const Scope in(at(1));
                Con_ToggleConsole_f();
            }
            mcheck(key_dest == key_game, "the console stays seat 1's (seat 2's toggleconsole does nothing)");
            Cvar_Set("scr_splitpause", t.dm ? "2" : "0");
            press(seatPad, SDL_CONTROLLER_BUTTON_START);
            next();
            return;
        case 17:
            mcheck(m.open, "seat 2's menu again");
            hold(holdFrames);
            next();
            return;
        case 18:
            if(t.dm)
            {
                mcheck(held() && VR_SeatMenusPause(), "scr_splitpause 2: deathmatch pauses too");
            }
            else
            {
                mcheck(!held() && !VR_SeatMenusPause(), "scr_splitpause 0: coop runs on");
            }
            Cvar_Set("scr_splitpause", "1");
            press(seatPad, SDL_CONTROLLER_BUTTON_B);
            next();
            return;
        case 19:
            mcheck(!m.open, "B shuts it");
            Cvar_Set("scr_splitmenu", "1");
            press(seatPad, SDL_CONTROLLER_BUTTON_START);
            next();
            return;
        case 20:
            mcheck(key_dest == key_menu && !m.open, "scr_splitmenu 1: seat 2's Start opens the full menu (as before phase 9)");
            press(seatPad, SDL_CONTROLLER_BUTTON_START);
            next();
            return;
        case 21:
            mcheck(key_dest == key_game, "and shuts it");
            Cvar_Set("scr_splitmenu", "0");
            press(seatPad, SDL_CONTROLLER_BUTTON_START);
            next();
            return;
        case 22: // the picture: seat 2's menu in its tile
            mcheck(m.open, "seat 2's menu for the picture");
            Cbuf_AddText("screenshot\n"); // (screenshots/: run.sh -Out composes it)
            t.wait = 3;
            next();
            return;
        case 23:
            press(seatPad, SDL_CONTROLLER_BUTTON_DPAD_UP); // (from Resume, up wraps to Leave Seat)
            next();
            return;
        case 24:
            mcheck(m.cursor == ItemLeave, "up from Resume: Leave Seat");
            press(seatPad, SDL_CONTROLLER_BUTTON_A);
            next();
            return;
        case 25:
            mcheck(m.confirmLeave && plays(1), "Leave Seat asks first");
            press(seatPad, SDL_CONTROLLER_BUTTON_A);
            next();
            return;
        case 26:
            if(plays(1))
            {
                return;
            }
            mcheck(IN_PadSeat(seatPad) < 0 && key_dest == key_game && !VR_SeatMenusOn(), // (split11: the pad that left drives no seat)
                "A again: seat 2 leaves, its pad drives no seat (its Start or A joins), no menu open");
            finish();
            return;
        default: finish(); return;
    }
}

void registerTestCommand_f()
{
    menuTest = MenuTest{};
    menuTest.on = true;
    Con_Printf("seat_menu_test: started\n");
}

} // namespace

void menuTestFrame()
{
    MenuTest& t = menuTest;
    if(!t.on)
    {
        return;
    }
    if(t.wait > 0)
    {
        t.wait--;
        if(t.wait == 2 && t.releasePad >= 0)
        {
            IN_MockPadButton(t.releasePad, t.releaseButton, false);
            t.releasePad = -1;
        }
        return;
    }
    if(++t.frames > stageFrames)
    {
        mcheck(false, va("timed out (stage %d)", t.stage));
        finish();
        return;
    }
    testStage();
}

// ---- vr_seat_bind_test (split20) ------------------------------------------------------------------------------------
//
// Each seat's own controller bindings: seat 2 joins by mock pad 1, opens its seat menu (Start), goes down to Controller
// Bindings and opens it (A): its own screen, its pad's picture; X picks the selected button, the d-pad moves, X swaps the
// two. Seat 2's two bindings swapped, seat 1's pad bindings unchanged, seat 2's bindings panel shows the swap; B back to
// the seat menu, B shuts it; then "writeconfig seatbind_test" (quakevr/seatbind_test.cfg). `vr_seat_bind_test check`
// (the next run, after "exec seatbind_test.cfg": a restart): the config's seat_bind lines read, seat 2 joins again and
// gets them: against a fresh join's copy (seat 3's), exactly two pad buttons differ, swapped. Verdict "seat_bind_test: N
// passed, M failed" (Misc/quakevr/seat_bind_test.sh runs both).

namespace
{

// seat-shared: vr_seat_bind_test's own state machine
struct BindTest
{
    testkit::Tally tally;
    bool on = false;
    bool checkRun = false; // the second run: the saved bindings read back
    int stage = 0;
    int frames = 0;
    int wait = 0;
    int releaseButton = -1;
    int keyA = -1, keyB = -1;
    za::String bindA, bindB;
    za::String host[MAX_KEYS]; // seat 1's pad bindings as the test began
    za::String panelBefore[bindpanel::maxRows]; // seat 2's panel rows before the swap
    int panelCount = 0;
} bindTest;

bool bcheck(const bool ok, const char* what)
{
    if(bindTest.tally.check(ok, "seat_bind_test", what))
    {
        Con_Printf("seat_bind_test: PASS %s\n", what);
    }
    return ok;
}

void bpress(const int button)
{
    IN_MockPadButton(seatPad, button, true);
    bindTest.releaseButton = button;
    bindTest.wait = 4;
}

[[nodiscard]] bool isPadKey(const int k)
{
    return (Key_GetDeviceMaskForKeynum(k) & KDM_GAMEPAD) != 0;
}

// What seat s's button k does as its Controller Bindings screen shows it (flatscreen play, padflat): the flat key that
// has it (the seat's own buttons: split21's seat_flatkey), else its bind.
[[nodiscard]] const char* bindOf(const int s, const int k)
{
    if(k < 0)
    {
        return "";
    }
    {
        const Scope as{at(s)};
        for(const cvar_t* c : flatplay::keyCvars())
        {
            if(flatplay::keyIs(*c, k))
            {
                return c->name;
            }
        }
    }
    const char* b = Key_SeatOf(s)->bindings[k];
    return b ? b : "";
}

void bfinish()
{
    for(int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++)
    {
        IN_MockPadButton(seatPad, b, false);
    }
    if(plays(1))
    {
        Cbuf_AddText("seat_leave 2\n");
    }
    IN_MockPad(seatPad, false);
    bindTest.on = false;
    bindTest.tally.verdict("seat_bind_test");
}

void bnext()
{
    bindTest.stage++;
    bindTest.frames = 0;
}

// seat `s`'s bindings panel rows (its own pad's buttons and their actions), as "button=action" texts: how many
int panelRows(const int s, za::String (&out)[bindpanel::maxRows])
{
    const Scope in(at(s));
    gadget::BindPanelRow r[bindpanel::maxRows];
    const int n = bindpanel::rows(r, bindpanel::maxRows);
    for(int i = 0; i < n; i++)
    {
        out[i] = va("%s=%s", r[i].keys[0], r[i].action);
    }
    return n;
}

// seat 2's panel now against before the swap: the same rows but two, whose actions changed places
[[nodiscard]] bool panelSwapped()
{
    za::String now[bindpanel::maxRows];
    const int n = panelRows(1, now);
    int diff[3] = {-1, -1, -1};
    int d = 0;
    for(int i = 0; i < n && n == bindTest.panelCount; i++)
    {
        if(!(now[i] == bindTest.panelBefore[i]))
        {
            diff[za::min(d, 2)] = i;
            d++;
        }
    }
    Con_Printf("seat_bind_test: panel %d rows (before %d), %d changed%s%s%s%s\n", n, bindTest.panelCount, d,
        d > 0 ? ": " : "", d > 0 ? now[diff[0]].cStr() : "", d > 1 ? ", " : "", d > 1 ? now[diff[1]].cStr() : "");
    return n > 0 && n == bindTest.panelCount && d == 2;
}

void bindEditStage()
{
    BindTest& t = bindTest;
    SeatMenuState& m = stateOf(1);
    switch(t.stage)
    {
        case 0:
            if(!sv.active || !signedOn(0))
            {
                return;
            }
            for(int k = 0; k < MAX_KEYS; k++)
            {
                t.host[k] = isPadKey(k) ? bindOf(0, k) : "";
            }
            bcheck(!bindsSaved(1), "a fresh run: seat 2 has no saved bindings");
            IN_MockPad(seatPad, true);
            Cbuf_AddText(va("seat_join %d\n", seatPad));
            bnext();
            return;
        case 1:
            if(!signedOn(1))
            {
                return;
            }
            bcheck(Key_SeatHasBindings(1), "seat 2 joined with its own bindings (seat 1's copied)");
            bpress(SDL_CONTROLLER_BUTTON_START);
            bnext();
            return;
        case 2:
            if(m.cursor != ItemBindings && m.open)
            {
                bpress(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                return;
            }
            bcheck(m.open && m.cursor == ItemBindings, "seat 2's menu: down to Controller Bindings");
            bpress(SDL_CONTROLLER_BUTTON_A);
            bnext();
            return;
        case 3:
        {
            const Scope in(at(1));
            bcheck(m.bindings && bindsetup::seatScreenOpen() && key_dest == key_game,
                "A opens seat 2's own Controller Bindings screen (in its tile; the full menu stays shut)");
            t.wait = 3; // (its tile draws it: the labels laid out)
            bnext();
            return;
        }
        case 4:
        {
            const Scope in(at(1));
            Cbuf_AddText("screenshot\n"); // (seat 2's screen in its tile: run.sh -Out composes it)
            t.keyA = bindsetup::cursorKey();
            t.bindA = bindOf(1, t.keyA);
            t.panelCount = panelRows(1, t.panelBefore);
            bpress(SDL_CONTROLLER_BUTTON_X);
            bnext();
            return;
        }
        case 5:
            bpress(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            bnext();
            return;
        case 6:
        {
            const Scope in(at(1));
            t.keyB = bindsetup::cursorKey();
            t.bindB = bindOf(1, t.keyB);
            bcheck(t.keyA >= 0 && t.keyB >= 0 && t.keyA != t.keyB && !(t.bindA == t.bindB),
                va("X picked %s (\"%s\"), the d-pad moved to %s (\"%s\")", Key_KeynumToString(t.keyA), t.bindA.cStr(),
                    t.keyB >= 0 ? Key_KeynumToString(t.keyB) : "-", t.bindB.cStr()));
            bpress(SDL_CONTROLLER_BUTTON_X);
            bnext();
            return;
        }
        case 7:
        {
            bcheck(t.bindB == bindOf(1, t.keyA) && t.bindA == bindOf(1, t.keyB), "X again: seat 2's two bindings swapped");
            int changed = 0;
            for(int k = 0; k < MAX_KEYS; k++)
            {
                changed += isPadKey(k) && !(t.host[k] == bindOf(0, k)) ? 1 : 0;
            }
            bcheck(changed == 0, va("seat 1's pad bindings unchanged (%d differ)", changed));
            bcheck(bindsSaved(1), "seat 2's bindings now its own (saved)");
            bcheck(panelSwapped(), "seat 2's bindings panel (its pad's buttons) shows the swap");
            bpress(SDL_CONTROLLER_BUTTON_B);
            bnext();
            return;
        }
        case 8:
            bcheck(!m.bindings && m.open, "B: back to seat 2's menu");
            bpress(SDL_CONTROLLER_BUTTON_B);
            bnext();
            return;
        case 9:
            bcheck(!m.open, "B again shuts it");
            Cbuf_AddText("writeconfig seatbind_test\n");
            Con_Printf("seat_bind_test: swapped %s and %s, written to seatbind_test.cfg\n", Key_KeynumToString(t.keyA),
                Key_KeynumToString(t.keyB));
            t.wait = 2;
            bnext();
            return;
        default: bfinish(); return;
    }
}

void bindCheckStage()
{
    BindTest& t = bindTest;
    switch(t.stage)
    {
        case 0:
            if(!sv.active || !signedOn(0))
            {
                return;
            }
            bcheck(bindsSaved(1) && !bindsSaved(2), "the restart read seat 2's saved bindings (and none for seat 3)");
            bcheck(!Key_SeatHasBindings(1), "seat 2 not yet joined: no bindings");
            IN_MockPad(seatPad, true);
            Cbuf_AddText(va("seat_join %d\n", seatPad));
            bnext();
            return;
        case 1:
        {
            if(!signedOn(1))
            {
                return;
            }
            const bool seat3Had = Key_SeatHasBindings(2);
            Key_SeatCopyBindings(2, 0); // (a fresh join's copy: seat 3, never saved)
            int diff[3] = {-1, -1, -1};
            int n = 0;
            for(int k = 0; k < MAX_KEYS; k++)
            {
                if(isPadKey(k) && ZA_STRCMP(bindOf(1, k), bindOf(2, k)))
                {
                    diff[za::min(n, 2)] = k;
                    n++;
                }
            }
            bcheck(n == 2 && !ZA_STRCMP(bindOf(1, diff[0]), bindOf(2, diff[1])) && !ZA_STRCMP(bindOf(1, diff[1]), bindOf(2, diff[0])),
                va("seat 2 joined with its saved bindings: %d pad buttons differ from a fresh join's (%s, %s), swapped", n,
                    diff[0] >= 0 ? Key_KeynumToString(diff[0]) : "-", diff[1] >= 0 ? Key_KeynumToString(diff[1]) : "-"));
            if(!seat3Had)
            {
                const Scope in(at(2));
                for(int k = 0; k < MAX_KEYS; k++)
                {
                    Key_SetBinding(k, nullptr);
                }
            }
            bfinish();
            return;
        }
        default: bfinish(); return;
    }
}

void bindTest_f()
{
    bindTest = BindTest{};
    bindTest.on = true;
    bindTest.checkRun = Cmd_Argc() > 1 && !ZA_STRCMP(Cmd_Argv(1), "check");
    Con_Printf("seat_bind_test: started%s\n", bindTest.checkRun ? " (check)" : "");
}

} // namespace

void bindTestFrame()
{
    BindTest& t = bindTest;
    if(!t.on)
    {
        return;
    }
    if(t.wait > 0)
    {
        t.wait--;
        if(t.wait == 2 && t.releaseButton >= 0)
        {
            IN_MockPadButton(seatPad, t.releaseButton, false);
            t.releaseButton = -1;
        }
        return;
    }
    if(++t.frames > stageFrames)
    {
        bcheck(false, va("timed out (stage %d)", t.stage));
        bfinish();
        return;
    }
    if(t.checkRun)
    {
        bindCheckStage();
    }
    else
    {
        bindEditStage();
    }
}

// ---- split21: vr_seat_split21_test (Misc/quakevr/seat_split21_test.sh) ------------------------------------------------
// The features of 11-27/28 each seat's own, flatscreen coop e1m1, seat 2 joined by a mock pad: its visor boots on its own
// (seat 1's stays up); both seats fire a shot and press R in the same frame: each its own procedural reload (its free hand,
// its trip counted once in its own state, a round seated in each); seat 2 takes a grenade from its back pouch into its off
// hand (vr_wheel_pick through its own connection): its hand holds it, seat 1's hands none. Verdict "seat_split21_test: N
// passed, M failed".
namespace
{
struct Split21Test
{
    testkit::Tally tally;
    bool on = false;
    int stage = 0;
    int frames = 0;
    float shells[2] = {0.f, 0.f}; // each seat's player's shells as R went down (the pouch's rounds come out of them)
} split21Test; // seat-shared: a test's own state

[[nodiscard]] float shellsOf(const int s)
{
    const edict_t* e = seatPlayer(s);
    return e ? e->v.ammo_shells : -1.f;
}

bool scheck(const bool ok, const char* what)
{
    if(split21Test.tally.check(ok, "seat_split21_test", what))
    {
        Con_Printf("seat_split21_test: PASS %s\n", what);
    }
    return ok;
}

void snext()
{
    split21Test.stage++;
    split21Test.frames = 0;
}

void sfinish()
{
    Cbuf_AddText("seat 1 -attack; seat 2 -attack\n");
    if(plays(1))
    {
        Cbuf_AddText("seat_leave 2\n");
    }
    IN_MockPad(seatPad, false);
    split21Test.on = false;
    split21Test.tally.verdict("seat_split21_test");
}

[[nodiscard]] bool bootedIn(const int s)
{
    const Scope in(at(s));
    return visor::active() && visor::booted();
}

[[nodiscard]] flatprocreload::Status reloadOf(const int s)
{
    const Scope in(at(s));
    return flatprocreload::status();
}

[[nodiscard]] bool freeHandBusy(const int s)
{
    const Scope in(at(s));
    return flatprocreload::busy(HAND_OFF);
}

[[nodiscard]] int grenadeIn(const int s, const int hand)
{
    const Scope in(at(s));
    return flatholster::handGrenade(hand);
}

constexpr int shotFrames = 36; // a shotgun's shot fired and its trigger let go (half a second at 72)

void split21Stage()
{
    Split21Test& t = split21Test;
    switch(t.stage)
    {
        case 0:
            if(!sv.active || !signedOn(0) || !bootedIn(0))
            {
                return; // (seat 1's visor up first)
            }
            Cbuf_AddText("vr_flat_holsters 0; vr_flat_autoreload 0; vr_flat_proc_reload 1; seat 1 god; seat 1 notarget\n");
            IN_MockPad(seatPad, true);
            Cbuf_AddText(va("seat_join %d\n", seatPad));
            snext();
            return;
        case 1:
            if(!signedOn(1))
            {
                return;
            }
            scheck(!bootedIn(1) && bootedIn(0), "seat 2 joined: its visor boots on its own, seat 1's stays up");
            Cbuf_AddText("seat 2 god; seat 2 notarget; seat 1 impulse 154; seat 2 impulse 154\n"); // (the shotguns)
            snext();
            return;
        case 2:
            if(!bootedIn(1))
            {
                return;
            }
            scheck(bootedIn(0), "seat 2's visor booted (seat 1's up all along)");
            Cbuf_AddText("seat 1 +attack; seat 2 +attack\n");
            snext();
            return;
        case 3:
            if(t.frames < shotFrames)
            {
                return;
            }
            t.shells[0] = shellsOf(0);
            t.shells[1] = shellsOf(1);
            Cbuf_AddText("seat 1 -attack; seat 2 -attack; seat 1 vr_mock_key r down; seat 2 vr_mock_key r down\n");
            snext();
            return;
        case 4:
            if(t.frames < 3)
            {
                return;
            }
            Cbuf_AddText("seat 1 vr_mock_key r up; seat 2 vr_mock_key r up\n");
            snext();
            return;
        case 5:
            if(!reloadOf(0).tripOn || !reloadOf(1).tripOn)
            {
                return;
            }
            scheck(freeHandBusy(0) && freeHandBusy(1), "both seats reload at once: each seat's off hand on its own trip");
            snext();
            return;
        case 6:
        {
            const flatprocreload::Status a = reloadOf(0), b = reloadOf(1);
            if(a.tripOn || b.tripOn)
            {
                return;
            }
            scheck(a.trips == 1 && b.trips == 1 && a.cancels == 0 && b.cancels == 0,
                va("each seat counted its own trip, none cancelled (seat 1 %d/%d, seat 2 %d/%d)", a.trips, a.cancels, b.trips, b.cancels));
            const float took0 = t.shells[0] - shellsOf(0), took1 = t.shells[1] - shellsOf(1);
            scheck(took0 >= 1.f && took1 >= 1.f, va("each seat's player took its own rounds from its pouch (seat 1 %.0f, seat 2 %.0f)",
                                                     static_cast<double>(took0), static_cast<double>(took1)));
            Cbuf_AddText("seat 2 give r 10; seat 2 cmd vr_wheel_pick 0 -5\n"); // (BACK POUCH into seat 2's off hand)
            snext();
            return;
        }
        case 7:
            if(grenadeIn(1, HAND_OFF) <= 0)
            {
                return;
            }
            scheck(grenadeIn(0, HAND_OFF) == 0 && grenadeIn(0, HAND_MAIN) == 0,
                va("seat 2 holds a grenade (kind %d), seat 1's hands none", grenadeIn(1, HAND_OFF)));
            sfinish();
            return;
        default: sfinish(); return;
    }
}

void split21Test_f()
{
    split21Test = Split21Test{};
    split21Test.on = true;
    Con_Printf("seat_split21_test: started (it runs once the map is up)\n");
}
} // namespace

void split21TestFrame()
{
    Split21Test& t = split21Test;
    if(!t.on)
    {
        return;
    }
    if(++t.frames > stageFrames)
    {
        for(int s = 0; s < 2; s++)
        {
            const flatprocreload::Status r = reloadOf(s);
            Con_Printf("seat_split21_test: seat %d: reload trips %d on %d seated %d cancels %d (%s); off hand grenade %d\n", s + 1,
                r.trips, r.tripOn ? 1 : 0, r.seated, r.cancels, r.lastCancel, grenadeIn(s, HAND_OFF));
        }
        scheck(false, va("timed out (stage %d)", t.stage));
        sfinish();
        return;
    }
    split21Stage();
}

void registerMenuCommands()
{
    Cmd_AddCommand("vr_seat_split21_test", split21Test_f);
    Cvar_RegisterVariable(&scr_splitmenu);
    Cvar_RegisterVariable(&scr_splitpause);
    Cmd_AddCommand("seat_menu", menuCommand_f);
    Cmd_AddCommand("vr_seat_menu_test", registerTestCommand_f);
    Cmd_AddCommand("vr_seat_bind_test", bindTest_f);
}

} // namespace qvr::seat
