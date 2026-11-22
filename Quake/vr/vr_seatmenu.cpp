// vr_seatmenu.cpp -- splitscreen phase 9 (docs/vr-port/SPLITSCREEN.md): menus per seat.
//
// His answer (section 7, question 4): customizable; by default in coop opening the menu pauses the game and only seat 1
// opens the full menu, the other seats their own small menu. So:
//   - seat 1 (index 0: the keyboard and mouse, and the pad no seat has) has the full menu and the console, as always
//     (key_dest is his);
//   - a seat 2-4's Start opens its seat menu, drawn in its tile and driven by its own pad only (keys.c routes a seat's
//     keys by Key_SeatDest: its seat menu, or the game; never the full menu or the console): Resume, Look Sensitivity
//     and Invert Look (the seat's own, on its stick), Controls (its pad's buttons), Leave Seat (asked twice);
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

#include "vr_engine.hpp"
#include "vr_testkit.hpp"

#include <SDL.h>
#include <cmath>
#include <cstring>

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
    M_DrawTextBox(64, 48, 24, 10);
    M_PrintWhite(80, 60, va("seat %d", s + 1));
    drawItem(76, "Resume", nullptr, m.cursor == ItemResume);
    drawItem(88, "Look Sensitivity", va("x%.2f", static_cast<double>(m.lookScale)), m.cursor == ItemSens);
    drawItem(100, "Invert Look", m.invertLook ? "on" : "off", m.cursor == ItemInvert);
    drawItem(112, "Controls", nullptr, m.cursor == ItemControls);
    drawItem(124, "Leave Seat", nullptr, m.cursor == ItemLeave);
    if(m.confirmLeave)
    {
        M_PrintWhite(80, 140, "leave? A: yes  B: no");
    }
    else if(VR_SeatMenusPause())
    {
        M_Print(80, 140, "the game is paused");
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
        M_PrintWhite(160 - 4 * static_cast<int>(strlen(line)), 8, line);
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
            mcheck(std::fabs(m.lookScale - 1.25f) < 0.001f, va("seat 2's look sensitivity up (x%.2f)", static_cast<double>(m.lookScale)));
            {
                const Scope in(at(1));
                mcheck(std::fabs(VR_SeatLookScale() - 1.25f) < 0.001f, "the stick's look scale is seat 2's");
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
            const float d = p ? std::sqrt((p->v.origin[0] - t.origin[0]) * (p->v.origin[0] - t.origin[0]) +
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
            mcheck(IN_PadSeat(seatPad) == 0 && key_dest == key_game && !VR_SeatMenusOn(),
                "A again: seat 2 leaves, pad 1 back to seat 1, no menu open");
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

void registerMenuCommands()
{
    Cvar_RegisterVariable(&scr_splitmenu);
    Cvar_RegisterVariable(&scr_splitpause);
    Cmd_AddCommand("seat_menu", menuCommand_f);
    Cmd_AddCommand("vr_seat_menu_test", registerTestCommand_f);
}

} // namespace qvr::seat
