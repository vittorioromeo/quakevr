// vr_splitjoin.cpp -- splitscreen phase 10 (docs/vr-port/SPLITSCREEN.md section 20): Multiplayer > Splitscreen, the join
// screen (flatscreen; with a headset the Multiplayer menu is as it was).
//
// Seat 1 is the keyboard and mouse (his answer). While the screen is up it reads every controller itself (in_sdl.c's
// IN_PadsToJoinScreen: no pad's keys reach a menu or a seat): a controller's A or Start joins it as the next free seat
// (2-4), its B leaves, and once joined its Start starts the game (or, a local multiplayer game running, goes back to
// it: the drop-in). Any controller's d-pad or left stick moves through the rows; an unjoined controller's B backs out.
// The keyboard and mouse drive the rows as any menu's: the game (coop or deathmatch), the map, the skill, the layout
// (scr_splitlayout) beside a picture of the tiles the seats will have, Start Game. A seat joins with no game running:
// it is in play and connects to the next local game (vr_seatplay.cpp's CL_SeatFrame), the one Start Game begins
// (maxplayers 4: room for another controller to drop in later).
//
// vr_split_join_test <seats 2-4> <coop|dm>: pads join, leave and join again on the screen, the screen's keys, Start on a
// joined pad starts the game; every seat signs on in its own slot driven by its own pad; then a seat leaves and joins
// again from the screen over the running game. Verdict "split_join_test: N passed, M failed".
#include "vr_seat.hpp"
#include "vr_flatholster.hpp"

#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_testkit.hpp"

#include <SDL.h>
#include <cstring>

namespace qvr::seat
{

namespace
{

enum Row
{
    RowMode,
    RowMap,
    RowSkill,
    RowLayout,
    RowStart,
    RowBack, // (a local multiplayer game running: back to it)
};

// The buttons the screen reads from each pad (its d-pad and left stick as arrows).
enum PadKey
{
    PadA,
    PadB,
    PadStart,
    PadUp,
    PadDown,
    PadLeft,
    PadRight,
    PadY, // split11: the controller layout shown (or the rows again)
    PadKeys
};

// seat-shared: the join screen (one screen: seat 1's menu) and its pads' last presses
struct JoinScreen
{
    int cursor = RowMode;
    bool dm = false;
    int level = -1; // M_NetLevelName's index (-1: the mode's first)
    int skill = 1;
    bool padHeld[4][PadKeys] = {};
    bool showButtons = false; // split11: the controller layout in the rows' place (a pad's Y, the keyboard's Tab)
} screen;

constexpr int rowY0 = 100;  // the rows' first line (menu units)
constexpr int rowStep = 10;
constexpr int seatY0 = 40;  // the seats' lines
constexpr int seatStep = 10;
constexpr int previewX = 236, previewY = 40, previewW = 76; // the layout's picture

[[nodiscard]] bool localMpGame()
{
    return sv.active && svs.maxclients > 1 && !cls.demoplayback;
}

[[nodiscard]] int rows()
{
    return localMpGame() ? RowBack + 1 : RowStart + 1;
}

// Row r's line (menu units): Start and Back a little below the settings.
[[nodiscard]] int rowY(const int r)
{
    return rowY0 + r * rowStep + (r >= RowStart ? 4 : 0);
}

[[nodiscard]] bool isDmLevel(const char* name)
{
    return !strncmp(name, "dm", 2);
}

// The mode's first level: coop e1m1 (or the game's second, its first after "start"), deathmatch the first "dm".
[[nodiscard]] int firstLevel(const bool dm)
{
    const int n = M_NetLevelCount();
    for(int i = 0; i < n; i++)
    {
        if(dm ? isDmLevel(M_NetLevelName(i)) : !strcmp(M_NetLevelName(i), "e1m1"))
        {
            return i;
        }
    }
    return dm || n < 2 ? 0 : 1;
}

[[nodiscard]] int level()
{
    if(screen.level < 0 || screen.level >= M_NetLevelCount())
    {
        screen.level = firstLevel(screen.dm);
    }
    return screen.level;
}

constexpr const char* layoutNames[5] = {"Auto", "Side by side", "Stacked", "One wide on top", "Grid"};
constexpr const char* skillNames[4] = {"Easy", "Normal", "Hard", "Nightmare"};

void sound(const char* s)
{
    S_LocalSound(s);
}

void startNow()
{
    IN_Activate();
    key_dest = key_game;
    m_state = m_none;
    if(sv.active)
    {
        Cbuf_AddText("disconnect\n");
    }
    Cbuf_AddText(va("maxplayers %d\n", maxSeats));
    Cbuf_AddText(va("deathmatch %d\ncoop %d\n", screen.dm ? 1 : 0, screen.dm ? 0 : 1));
    Cbuf_AddText(va("skill %d\n", screen.skill));
    Cbuf_AddText("campaign 0\n");
    Cbuf_AddText(va("map %s\n", M_NetLevelName(level())));
    Con_Printf("splitscreen: %s on %s, %d seat(s)\n", screen.dm ? "deathmatch" : "coop", M_NetLevelName(level()), inPlay());
}

extern "C" void M_Splitscreen_StartNow()
{
    startNow();
}

void start()
{
    if(sv.active)
    {
        M_Confirm("End this game and start a new one\nwith these seats?", "Start", "Cancel", 0.f, M_Splitscreen_StartNow);
        return;
    }
    startNow();
}

void backToGame()
{
    IN_Activate();
    key_dest = key_game;
    m_state = m_none;
}

void change(const int dir)
{
    switch(screen.cursor)
    {
        case RowMode:
            screen.dm = !screen.dm;
            screen.level = firstLevel(screen.dm);
            break;
        case RowMap:
        {
            const int n = M_NetLevelCount();
            screen.level = n > 0 ? (level() + dir + n) % n : 0;
            break;
        }
        case RowSkill: screen.skill = (screen.skill + dir + 4) % 4; break;
        case RowLayout:
        {
            const int l = static_cast<int>(Cvar_VariableValue("scr_splitlayout"));
            Cvar_SetValue("scr_splitlayout", static_cast<float>((CLAMP(0, l, 4) + dir + 5) % 5));
            break;
        }
        default: return;
    }
    sound("misc/menu3.wav");
}

void activate()
{
    switch(screen.cursor)
    {
        case RowStart:
            sound("misc/menu2.wav");
            start();
            return;
        case RowBack:
            sound("misc/menu2.wav");
            backToGame();
            return;
        default: change(1); return;
    }
}

void move(const int dir)
{
    screen.cursor = (screen.cursor + dir + rows()) % rows();
    sound("misc/menu1.wav");
}

// A pad's press on the screen (k: the pad; joined: the seat it plays as, -1 none).
void padPress(const int k, const int key)
{
    const int joined = seatOfPad(k);
    switch(key)
    {
        case PadA:
        case PadStart:
            if(joined < 0)
            {
                if(joinPad(k) > 0)
                {
                    sound("misc/menu2.wav");
                }
                return;
            }
            if(key == PadStart)
            {
                localMpGame() ? backToGame() : start();
            }
            return;
        case PadB:
            if(joined > 0)
            {
                leaveSeat(joined);
                sound("misc/menu3.wav");
            }
            else
            {
                M_Menu_MultiPlayer_f();
            }
            return;
        case PadUp: move(-1); return;
        case PadDown: move(1); return;
        case PadLeft: change(-1); return;
        case PadRight: change(1); return;
        case PadY: screen.showButtons = !screen.showButtons; return;
        default: return;
    }
}

constexpr float stickThreshold = 0.6f;

// Seat i's colour in the picture (as the seats' markers: 1 blue, 2 red, 3 green, 4 yellow).
constexpr float seatColors[4][3] = {{0.25f, 0.45f, 0.9f}, {0.85f, 0.25f, 0.2f}, {0.25f, 0.7f, 0.3f}, {0.85f, 0.75f, 0.2f}};

void drawPreview()
{
    vrect_t win;
    SCR_SeatWindow(&win);
    const int h = CLAMP(24, previewW * win.height / q_max(1, win.width), 60);
    constexpr float frame[3] = {0.f, 0.f, 0.f};
    Draw_FillEx(static_cast<float>(previewX - 1), static_cast<float>(previewY - 1), static_cast<float>(previewW + 2),
        static_cast<float>(h + 2), frame, 0.8f);
    const int n = inPlay();
    int i = 0;
    for(int s = 0; s < maxSeats; s++)
    {
        if(!plays(s))
        {
            continue;
        }
        vrect_t t;
        SCR_LayoutTileOf(n, i++, previewW, h, &t);
        Draw_FillEx(static_cast<float>(previewX + t.x + 1), static_cast<float>(previewY + t.y + 1),
            static_cast<float>(t.width - 2), static_cast<float>(t.height - 2), seatColors[s], 0.9f);
        M_DrawCharacter(previewX + t.x + t.width / 2 - 4, previewY + t.y + t.height / 2 - 4, '1' + s);
    }
    M_Print(previewX, previewY + h + 4, va("%d seat%s", n, n == 1 ? "" : "s"));
}

void printClipped(const int x, const int y, const char* text, const int chars, const bool white)
{
    char line[64];
    q_strlcpy(line, text, q_min(static_cast<int>(sizeof(line)), chars + 1));
    white ? M_PrintWhite(x, y, line) : M_Print(x, y, line);
}

} // namespace

// ---- the join screen's tests: vr_split_join_test ---------------------------------------------------------------------

namespace
{

// seat-shared: vr_split_join_test's own state machine
struct JoinTest
{
    testkit::Tally tally;
    bool on = false;
    int seats = 2;
    bool dm = false;
    int stage = 0;
    int frames = 0;  // in this stage (its give-up)
    int wait = 0;    // frames before the next stage (a press's down and up)
    int pad = 0;     // the pad being joined
    int releasePad = -1;
    int releaseButton = -1;
    int releaseKey = -1; // a keyboard key let go after the wait
    int slot = -1;   // the leaving seat's server slot
    bool wasGame = false; // a game ran when the test started
    float yaw = 0.f;      // split11: the pad seat's yaw as its holster menu opened (the look stick moves the cursor)
} joinTest;

constexpr int stageFrames = 1440; // a stage's give-up (20 s of game time): a FAIL "timed out"

bool jcheck(const bool ok, const char* what)
{
    if(joinTest.tally.check(ok, "split_join_test", what))
    {
        Con_Printf("split_join_test: PASS %s\n", what);
    }
    return ok;
}

void jpress(const int pad, const int button)
{
    IN_MockPadButton(pad, button, true);
    joinTest.releasePad = pad;
    joinTest.releaseButton = button;
    joinTest.wait = 4;
}

[[nodiscard]] bool signedOn(const int s)
{
    const clientseat_t* n = CL_SeatNetIfMade(s);
    return n && n->cstatic.state == ca_connected && n->cstatic.signon == SIGNONS;
}

[[nodiscard]] int slotOfSeat(const int s)
{
    for(int i = 0; sv.active && svs.clients && i < svs.maxclients; i++)
    {
        if(svs.clients[i].active && svs.clients[i].netconnection &&
            NET_QSocketLocalSeat(svs.clients[i].netconnection) == s)
        {
            return i;
        }
    }
    return -1;
}

void jnext()
{
    joinTest.stage++;
    joinTest.frames = 0;
}

void jfinish()
{
    JoinTest& t = joinTest;
    for(int k = 0; k < 4; k++)
    {
        for(int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++)
        {
            IN_MockPadButton(k, b, false);
        }
    }
    for(int s = maxSeats - 1; s > 0; s--)
    {
        leaveSeat(s);
    }
    for(int k = 0; k < 4; k++)
    {
        IN_MockPad(k, false);
    }
    if(key_dest == key_menu)
    {
        backToGame();
    }
    t.on = false;
    t.tally.verdict("split_join_test");
}

void jstage()
{
    JoinTest& t = joinTest;
    const int lastPad = t.seats - 2;
    switch(t.stage)
    {
        case 0: // no game: one seat; the Multiplayer menu's Splitscreen row opens the screen
            if(t.frames < 2)
            {
                return;
            }
            t.wasGame = sv.active; // (the run may start in a map: then Start Game asks first)
            jcheck(inPlay() == 1, va("start: one seat (%s)", sv.active ? va("a game on %s", sv.name) : "no game"));
            for(int k = 0; k <= lastPad; k++)
            {
                IN_MockPad(k, true);
            }
            jcheck(M_Splitscreen_Shown(), "flatscreen: Multiplayer has the Splitscreen row");
            M_Menu_MultiPlayer_f();
            for(int i = 0; i < 3; i++) // (to the fourth row, from the first)
            {
                M_Keydown(K_DOWNARROW, false);
            }
            M_Keydown(K_ENTER, false);
            jnext();
            return;
        case 1:
            jcheck(M_Splitscreen_Open() && M_Splitscreen_OwnsPads(), "Multiplayer > Splitscreen opens the join screen, which reads the pads");
            jpress(0, SDL_CONTROLLER_BUTTON_A);
            jnext();
            return;
        case 2:
            jcheck(seatOfPad(0) == 1 && inPlay() == 2, va("pad 1's A joins it as seat 2 (seat %d, %d in play)", seatOfPad(0) + 1, inPlay()));
            jcheck(M_Splitscreen_Open(), "the A reached no menu (the screen stays up)");
            jpress(0, SDL_CONTROLLER_BUTTON_B);
            jnext();
            return;
        case 3:
            jcheck(!plays(1) && inPlay() == 1 && M_Splitscreen_Open(), "its B leaves seat 2 (the screen stays up)");
            jpress(0, SDL_CONTROLLER_BUTTON_START);
            t.pad = 1;
            jnext();
            return;
        case 4: // the other pads join, one a stage
            if(t.pad == 1)
            {
                jcheck(seatOfPad(0) == 1, "pad 1's Start joins it again as seat 2");
            }
            else
            {
                jcheck(seatOfPad(t.pad - 1) == t.pad, va("pad %d's A joins it as seat %d", t.pad, t.pad + 1));
            }
            if(t.pad <= lastPad)
            {
                jpress(t.pad, SDL_CONTROLLER_BUTTON_A);
                t.pad++;
                return;
            }
            jcheck(inPlay() == t.seats, va("%d seats in play before any game (%d)", t.seats, inPlay()));
            jcheck(sv.active == t.wasGame && M_Splitscreen_Open(), "no game started yet, the screen up");
            if(t.dm)
            {
                jpress(lastPad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT); // (the Game row: deathmatch, by a pad: split11)
            }
            jnext();
            return;
        case 5:
            jcheck(screen.dm == t.dm && isDmLevel(M_NetLevelName(level())) == t.dm,
                va("the game row: %s on %s", screen.dm ? "deathmatch" : "coop", M_NetLevelName(level())));
            Cbuf_AddText("screenshot\n"); // (screenshots/: run.sh -Out composes them)
            t.wait = 2;
            jnext();
            return;
        case 6: // a joined pad's Start starts the game
            jpress(lastPad, SDL_CONTROLLER_BUTTON_START);
            jnext();
            return;
        case 7:
        {
            if(m_state == m_confirm && key_dest == key_menu) // (a game ran: "End this game?", answered by the pad's A)
            {
                jcheck(t.wasGame && M_Splitscreen_Open() && M_Splitscreen_OwnsPads(), "a game running: Start asks first, the pads still the screen's");
                jpress(lastPad, SDL_CONTROLLER_BUTTON_A);
                return;
            }
            bool all = sv.active && key_dest == key_game;
            for(int s = 0; s < t.seats; s++)
            {
                all = all && signedOn(s);
            }
            if(!all)
            {
                return;
            }
            jcheck(!M_Splitscreen_Open(), "Start shut the screen");
            jcheck((deathmatch.value != 0.f) == t.dm && (coop.value != 0.f) == !t.dm && svs.maxclients == maxSeats,
                va("the game: deathmatch %g, coop %g, maxplayers %d", static_cast<double>(deathmatch.value),
                    static_cast<double>(coop.value), svs.maxclients));
            jcheck(!strcmp(sv.name, M_NetLevelName(level())), va("on the chosen map (%s)", sv.name));
            bool slots = slotOfSeat(0) == 0;
            for(int s = 1; s < t.seats; s++)
            {
                slots = slots && slotOfSeat(s) > 0 && IN_PadSeat(s - 1) == s;
            }
            jcheck(slots, va("each of the %d seats signed on in its own slot, driven by its own pad", t.seats));
            t.wait = 8; // (Start let go meanwhile: no seat menu from it)
            jnext();
            return;
        }
        case 8:
        {
            bool menus = key_dest == key_game;
            for(int s = 1; s < t.seats; s++)
            {
                const Scope in(at(s));
                menus = menus && Key_SeatDest() == key_game;
            }
            jcheck(menus, "the Start that began the game opened no menu");
            Cbuf_AddText("screenshot\n");
            t.wait = 2;
            jnext();
            return;
        }
        case 9: // drop out and in over the running game: the screen again
            M_Menu_Splitscreen_f();
            t.slot = slotOfSeat(lastPad + 1);
            jpress(lastPad, SDL_CONTROLLER_BUTTON_B);
            jnext();
            return;
        case 10:
            if(t.slot >= 0 && svs.clients[t.slot].active)
            {
                return;
            }
            jcheck(!plays(lastPad + 1) && inPlay() == t.seats - 1, va("in the game: pad %d's B leaves seat %d, its slot freed", lastPad + 1, lastPad + 2));
            jpress(lastPad, SDL_CONTROLLER_BUTTON_A);
            jnext();
            return;
        case 11:
            if(!signedOn(lastPad + 1))
            {
                return;
            }
            jcheck(seatOfPad(lastPad) == lastPad + 1 && slotOfSeat(lastPad + 1) > 0, va("its A joins the running game again (seat %d, slot %d)", lastPad + 2, slotOfSeat(lastPad + 1)));
            jpress(lastPad, SDL_CONTROLLER_BUTTON_START);
            jnext();
            return;
        case 12:
        {
            jcheck(!M_Splitscreen_Open() && key_dest == key_game, "its Start goes back to the game");
            // split11: a controller seat's flat-play layout (seat::padLayout) where seat 1's copy leaves a button free
            const seatkeys_t* k = Key_SeatOf(lastPad + 1);
            const char* up = k->bindings[K_DPAD_UP];
            const char* x = k->bindings[K_XBUTTON];
            jcheck(up && !strcmp(up, "+flatholsters") && x && x[0],
                va("seat %d's pad has the holster menu (d-pad up: %s) and reload (X: %s)", lastPad + 2, up ? up : "-", x ? x : "-"));
            // the holster menu (the weapon wheel) by the pad: d-pad up held, the look stick pushed right
            IN_MockPadButton(lastPad, SDL_CONTROLLER_BUTTON_DPAD_UP, true);
            t.wait = 8;
            jnext();
            return;
        }
        case 13: // (the menu open: then the stick, so none of it turns the view before the menu has it)
        {
            {
                const Scope in(at(lastPad + 1));
                t.yaw = cl.viewangles[YAW];
            }
            IN_MockPadAxes(lastPad, 0.f, 0.f, 1.f, 0.f);
            t.wait = 16;
            jnext();
            return;
        }
        case 14:
        {
            {
                const Scope in(at(lastPad + 1));
                const float moved = flatholster::cursorFromMiddleX();
                jcheck(flatholster::open() && moved > 20.f && fabsf(cl.viewangles[YAW] - t.yaw) < 0.5f,
                    va("seat %d: d-pad up, holster menu %d; look stick, cursor %.0f, view %.1f", lastPad + 2,
                        flatholster::open() ? 1 : 0, static_cast<double>(moved), static_cast<double>(cl.viewangles[YAW] - t.yaw)));
            }
            IN_MockPadAxes(lastPad, 0.f, 0.f, 0.f, 0.f);
            IN_MockPadButton(lastPad, SDL_CONTROLLER_BUTTON_DPAD_UP, false);
            t.wait = 8;
            jnext();
            return;
        }
        case 15:
            // drop out in the game (no screen), then in again by the pad alone: Start, then A
            t.slot = slotOfSeat(lastPad + 1);
            leaveSeat(lastPad + 1);
            jnext();
            return;
        case 16:
        case 19:
            if(t.slot >= 0 && svs.clients[t.slot].active)
            {
                return;
            }
            jcheck(!plays(lastPad + 1) && IN_PadSeat(lastPad) < 0,
                va("seat %d left in the game: its pad drives no seat (not seat 1's: %d)", lastPad + 2, IN_PadSeat(lastPad) + 1));
            jpress(lastPad, t.stage == 16 ? SDL_CONTROLLER_BUTTON_START : SDL_CONTROLLER_BUTTON_A);
            jnext();
            return;
        case 17:
        case 20:
        {
            if(!signedOn(lastPad + 1))
            {
                return;
            }
            const bool start = t.stage == 17;
            jcheck(seatOfPad(lastPad) == lastPad + 1 && slotOfSeat(lastPad + 1) > 0 && !M_Splitscreen_Open() && key_dest == key_game,
                va("its %s joins the running game directly (seat %d, slot %d, no screen)", start ? "Start" : "A", lastPad + 2,
                    slotOfSeat(lastPad + 1)));
            t.wait = 8; // (let go meanwhile: the press that joined is no press in the seat)
            jnext();
            return;
        }
        case 18:
        case 21:
        {
            const Scope in(at(lastPad + 1));
            jcheck(Key_SeatDest() == key_game && !Key_Seat()->down[K_ESCAPE] && !Key_Seat()->down[K_ABUTTON],
                va("the %s that joined opened no seat menu, pressed nothing in the seat", t.stage == 18 ? "Start" : "A"));
            if(t.stage == 18)
            {
                t.slot = slotOfSeat(lastPad + 1);
                leaveSeat(lastPad + 1);
            }
            else
            {
                jfinish();
                return;
            }
            jnext();
            return;
        }
        default: jfinish(); return;
    }
}

// vr_split_join_test <seats 2-4> <coop|dm>: run with no game (-flatscreenmode); it ends with its verdict.
void joinTest_f()
{
    joinTest = JoinTest{};
    joinTest.seats = Cmd_Argc() > 1 ? CLAMP(2, Q_atoi(Cmd_Argv(1)), maxSeats) : 2;
    joinTest.dm = Cmd_Argc() > 2 && !strcmp(Cmd_Argv(2), "dm");
    joinTest.on = true;
    screen = JoinScreen{};
    Con_Printf("split_join_test: started (%d seats, %s)\n", joinTest.seats, joinTest.dm ? "deathmatch" : "coop");
}

} // namespace

void registerJoinCommands()
{
    Cmd_AddCommand("menu_splitscreen", M_Menu_Splitscreen_f);
    Cmd_AddCommand("vr_split_join_test", joinTest_f);
}

void joinTestFrame()
{
    JoinTest& t = joinTest;
    if(!t.on)
    {
        return;
    }
    if(t.wait > 0)
    {
        if(--t.wait == 2 && t.releasePad >= 0)
        {
            IN_MockPadButton(t.releasePad, t.releaseButton, false);
            t.releasePad = -1;
        }
        if(t.wait == 2 && t.releaseKey >= 0)
        {
            IN_MockKeyboardKey(t.releaseKey, false);
            t.releaseKey = -1;
        }
        return;
    }
    if(++t.frames > stageFrames)
    {
        jcheck(false, va("timed out (stage %d)", t.stage));
        jfinish();
        return;
    }
    jstage();
}

} // namespace qvr::seat

using namespace qvr::seat;

extern "C" qboolean M_Splitscreen_Shown()
{
    return !VR_IsActive();
}

extern "C" qboolean M_Splitscreen_Open()
{
    return key_dest == key_menu &&
           (m_state == m_splitscreen || (m_state == m_confirm && M_Confirm_PrevState() == m_splitscreen));
}

extern "C" qboolean M_Splitscreen_OwnsPads()
{
    return M_Splitscreen_Open();
}

extern "C" void M_Menu_Splitscreen_f()
{
    IN_DeactivateForMenu();
    key_dest = key_menu;
    m_state = m_splitscreen;
    m_entersound = true;
    screen.skill = CLAMP(0, static_cast<int>(skill.value + 0.5f), 3);
    if(screen.cursor >= rows())
    {
        screen.cursor = RowMode;
    }
    for(int k = 0; k < 4; k++) // (the pads' buttons held as it opens are not presses)
    {
        screen.padHeld[k][PadA] = IN_PadDown(k, SDL_CONTROLLER_BUTTON_A);
        screen.padHeld[k][PadB] = IN_PadDown(k, SDL_CONTROLLER_BUTTON_B);
        screen.padHeld[k][PadStart] = IN_PadDown(k, SDL_CONTROLLER_BUTTON_START);
        screen.padHeld[k][PadUp] = IN_PadDown(k, SDL_CONTROLLER_BUTTON_DPAD_UP);
        screen.padHeld[k][PadDown] = IN_PadDown(k, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        screen.padHeld[k][PadLeft] = IN_PadDown(k, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        screen.padHeld[k][PadRight] = IN_PadDown(k, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    }
}

// A pad's press while the screen's question is up ("End this game?"): the question's keys.
static void confirmPress(const int key)
{
    switch(key)
    {
        case PadA:
        case PadStart: M_Keydown(K_ENTER, false); return;
        case PadB: M_Keydown(K_ESCAPE, false); return;
        case PadUp: M_Keydown(K_UPARROW, false); return;
        case PadDown: M_Keydown(K_DOWNARROW, false); return;
        case PadLeft: M_Keydown(K_LEFTARROW, false); return;
        case PadRight: M_Keydown(K_RIGHTARROW, false); return;
        default: return;
    }
}

extern "C" void M_Splitscreen_Pads()
{
    for(int k = 0; k < 4 && M_Splitscreen_OwnsPads(); k++)
    {
        bool now[PadKeys] = {};
        if(IN_PadName(k))
        {
            const float lx = IN_PadAxis(k, SDL_CONTROLLER_AXIS_LEFTX);
            const float ly = IN_PadAxis(k, SDL_CONTROLLER_AXIS_LEFTY);
            now[PadA] = IN_PadDown(k, SDL_CONTROLLER_BUTTON_A);
            now[PadB] = IN_PadDown(k, SDL_CONTROLLER_BUTTON_B);
            now[PadStart] = IN_PadDown(k, SDL_CONTROLLER_BUTTON_START);
            now[PadUp] = IN_PadDown(k, SDL_CONTROLLER_BUTTON_DPAD_UP) || ly < -stickThreshold;
            now[PadDown] = IN_PadDown(k, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ly > stickThreshold;
            now[PadLeft] = IN_PadDown(k, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || lx < -stickThreshold;
            now[PadRight] = IN_PadDown(k, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || lx > stickThreshold;
            now[PadY] = IN_PadDown(k, SDL_CONTROLLER_BUTTON_Y);
        }
        for(int b = 0; b < PadKeys && M_Splitscreen_OwnsPads(); b++)
        {
            const bool pressed = now[b] && !screen.padHeld[k][b];
            screen.padHeld[k][b] = now[b];
            if(pressed)
            {
                m_state == m_confirm ? confirmPress(b) : padPress(k, b);
            }
        }
    }
}

extern "C" void M_Splitscreen_Key(const int key)
{
    switch(key)
    {
        case K_ESCAPE:
        case K_BBUTTON:
        case K_MOUSE4:
        case K_MOUSE2: M_Menu_MultiPlayer_f(); return;
        case K_UPARROW: move(-1); return;
        case K_DOWNARROW: move(1); return;
        case K_LEFTARROW: change(-1); return;
        case K_RIGHTARROW: change(1); return;
        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON:
        case K_MOUSE1: activate(); return;
        case K_TAB:
        case K_YBUTTON: screen.showButtons = !screen.showButtons; return;
        default: return;
    }
}

extern "C" void M_Splitscreen_Mousemove(const int cx, const int cy)
{
    (void)cx;
    for(int r = 0; r < rows(); r++)
    {
        if(cy >= rowY(r) - 1 && cy < rowY(r) + rowStep - 1 && r != screen.cursor)
        {
            screen.cursor = r;
            sound("misc/menu1.wav");
        }
    }
}

extern "C" void M_Splitscreen_Draw()
{
    M_DrawPlaque(16, 4);
    qpic_t* p = Draw_CachePic("gfx/p_multi.lmp");
    M_DrawPic((320 - p->width) / 2, 4, p);
    M_PrintWhite(72, 28, "Splitscreen: who plays");

    // the seats: seat 1 the keyboard and mouse, the others the controllers that joined (or how to join)
    for(int s = 0; s < maxSeats; s++)
    {
        const int y = seatY0 + s * seatStep;
        M_PrintWhite(64, y, va("%d", s + 1));
        if(s == 0)
        {
            M_Print(80, y, "Keyboard and mouse");
            continue;
        }
        if(!plays(s))
        {
            M_Print(80, y, "A on a controller joins");
            continue;
        }
        const char* name = IN_PadName(padOf(s));
        printClipped(80, y, name ? name : "controller", 13, true);
        M_Print(80 + 14 * 8, y, "B:out");
    }
    drawPreview();

    // the rows
    const char* values[RowBack + 1] = {
        screen.dm ? "Deathmatch" : "Cooperative",
        va("%s %s", M_NetLevelName(level()), M_NetLevelDesc(level())),
        skillNames[CLAMP(0, screen.skill, 3)],
        layoutNames[CLAMP(0, static_cast<int>(Cvar_VariableValue("scr_splitlayout")), 4)],
        "",
        "",
    };
    // (the Map row's value: va's buffer is reused, so it is copied first)
    char map[64];
    q_strlcpy(map, values[RowMap], sizeof(map));
    values[RowMap] = map;
    constexpr const char* labels[RowBack + 1] = {"Game", "Map", "Skill", "Layout", "", ""};
    for(int r = 0; r < rows() && !screen.showButtons; r++)
    {
        const int y = rowY(r);
        if(r == RowStart)
        {
            M_PrintWhite(80, y, sv.active ? "Start a New Game" : "Start Game");
        }
        else if(r == RowBack)
        {
            M_PrintWhite(80, y, "Back to the Game");
        }
        else
        {
            M_Print(80, y, labels[r]);
            printClipped(136, y, values[r], 22, true);
        }
        if(r == screen.cursor)
        {
            M_DrawCharacter(68, y, 12 + (static_cast<int>(realtime * 4) & 1));
        }
    }
    if(screen.showButtons) // split11: a controller seat's layout in the game (seat::padLayout; its seat menu's Controls)
    {
        int count = 0;
        const PadAction* layout = padLayout(count);
        M_PrintWhite(16, rowY0 - 8, "A controller in the game:");
        for(int i = 0; i < count; i++)
        {
            const int x = i % 2 == 0 ? 16 : 168, y = rowY0 + 2 + (i / 2) * 8;
            const char* name = Key_KeynumToFriendlyString(layout[i].key);
            name += !strncmp(name, "DPAD ", 5) ? 5 : 0; // (the d-pad's Up, Down, Left, Right)
            printClipped(x, y, name, 6, true);
            printClipped(x + 56, y, layout[i].label, 12, false);
        }
    }
    M_Print(12, 172, "Pads: A joins, B leaves, Start plays");
    M_Print(12, 181, "Keys: arrows, Enter. Y or Tab: buttons");
    M_Print(12, 190, "In a game a free pad's Start joins");
}
