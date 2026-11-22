// vr_seatrender.cpp -- splitscreen phase 7 (docs/vr-port/SPLITSCREEN.md): the test of each seat drawn in its own tile.
//
// vr_seat_render_test <n> (2-4; run in a local coop or deathmatch game, maxplayers n or more, -flatscreenmode, or right
// before its map command): seats 2..n join by mock pads, each seat turned its own way; once every seat's view has been
// drawn a few times (gl_screen.c's per-seat draw records, seatview_t's drawn*), the checks: the tiles are the layout's
// (SCR_SeatRect), inside the window, apart and covering it; each tile's view is drawn from its own seat's player (the
// nearest seat player to the drawn eye is the seat's own); each tile shows its seat's marker (scr_seatmarkers 2: the
// marker's pixel read back after the frame's draw) and a picture of its own (the tiles' middles differ); a screenshot
// of the layout; then the seats leave and the one seat left is drawn in the whole window again. Verdict
// "seat_render_test: N passed, M failed".
// Phase 7 part 2: each seat's own drawn body (vr_avatar: posed at its own player, its own feet's steps).
#include "vr_seat.hpp"

#include "vr_avatar.hpp"
#include "vr_engine.hpp"
#include "vr_testkit.hpp"

#include <cmath>

namespace qvr::seat
{

namespace
{

enum class Step
{
    Off,
    WaitGame,   // seat 1 (index 0) in the game
    WaitJoined, // seats 2..n signed on
    WaitDrawn,  // every seat's view drawn a few times, turned its own way
    WaitShot,   // the screenshot taken (the frame after its command)
    WaitAlone,  // the seats left: seat 1 drawn in the whole window again
};

// seat-shared: vr_seat_render_test's own state machine
struct RenderTest
{
    testkit::Tally tally;
    Step step = Step::Off;
    int frames = 0;            // in this step
    int seats = 2;             // the seats the test plays with
    int drawnAt[maxSeats] = {}; // each seat's drawn count when the step began
    float notifyTime = 3.f;     // con_notifytime before (the notify lines hidden while the markers are read)
} renderTest;

constexpr int stepFrames = 1440; // a step's give-up (20 s of game time): a FAIL "timed out"
constexpr int drawsWanted = 4;   // each seat's view drawn this many times before the checks (the markers' read-back)

bool check(const bool ok, const char* what)
{
    if(renderTest.tally.check(ok, "seat_render_test", what))
    {
        Con_Printf("seat_render_test: PASS %s\n", what);
    }
    return ok;
}

void go(const Step s)
{
    renderTest.step = s;
    renderTest.frames = 0;
    for(int i = 0; i < maxSeats; i++)
    {
        renderTest.drawnAt[i] = V_SeatViewOf(i)->drawnframes;
    }
}

[[nodiscard]] bool signedOn(const int s)
{
    const clientseat_t* n = CL_SeatNetIfMade(s);
    return n && n->cstatic.state == ca_connected && n->cstatic.signon == SIGNONS;
}

[[nodiscard]] bool localGameInPlay()
{
    return sv.active && svs.maxclients > 1 && signedOn(0) && !cls.demoplayback;
}

void finish()
{
    for(int s = maxSeats - 1; s >= 1; s--)
    {
        if(plays(s))
        {
            Cbuf_AddText(va("seat_leave %d\n", s + 1));
        }
        IN_MockPad(s - 1, false);
    }
    Cvar_Set("scr_seatmarkers", "0");
    Cvar_SetValue("con_notifytime", renderTest.notifyTime);
    renderTest.step = Step::Off;
    renderTest.tally.verdict("seat_render_test");
}

// Seat s's player's origin in its own client world (false: not there yet).
[[nodiscard]] bool playerOrigin(const int s, vec3_t out)
{
    const clientseat_t* n = CL_SeatNetIfMade(s);
    if(!n || n->cstate.viewentity <= 0 || n->cstate.viewentity >= n->max_edicts)
    {
        return false;
    }
    VectorCopy(n->entities[n->cstate.viewentity].origin, out);
    return true;
}

[[nodiscard]] float flatDistance(const vec3_t a, const vec3_t b)
{
    return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]));
}

[[nodiscard]] bool overlaps(const vrect_t& a, const vrect_t& b)
{
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

// The markers' colours (gl_screen.c's SCR_DrawSeatMarker): seat 1 red, 2 green, 3 blue, 4 yellow.
[[nodiscard]] bool markerIs(const int s, const byte* px)
{
    constexpr int want[maxSeats][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0}};
    for(int c = 0; c < 3; c++)
    {
        if(want[s][c] != 0 ? px[c] < 160 : px[c] > 90)
        {
            return false;
        }
    }
    return true;
}

void checkTiles()
{
    const int n = renderTest.seats;
    vrect_t win;
    SCR_SeatWindow(&win);
    check(SCR_SeatsDrawn() == n, va("%d seats laid out (SCR_SeatsDrawn %d), layout %d (scr_splitlayout %s)", n,
        SCR_SeatsDrawn(), SCR_SplitLayout(), Cvar_VariableString("scr_splitlayout")));
    long long area = 0;
    bool inside = true, apart = true, asLaidOut = true;
    for(int s = 0; s < n; s++)
    {
        const seatview_t* v = V_SeatViewOf(s);
        vrect_t want;
        SCR_SeatRect(s, &want);
        const vrect_t& r = v->drawnrect;
        asLaidOut = asLaidOut && r.x == want.x && r.y == want.y && r.width == want.width && r.height == want.height;
        inside = inside && r.width > 0 && r.height > 0 && r.x >= 0 && r.y >= 0 && r.x + r.width <= win.width &&
                 r.y + r.height <= win.height;
        area += static_cast<long long>(r.width) * r.height;
        for(int o = 0; o < s; o++)
        {
            apart = apart && !overlaps(r, V_SeatViewOf(o)->drawnrect);
        }
        Con_Printf("seat_render_test: seat %d tile %d,%d %dx%d, eye %.0f %.0f %.0f, marker %d %d %d, middle %08x\n", s + 1,
            r.x, r.y, r.width, r.height, static_cast<double>(v->drawnorg[0]), static_cast<double>(v->drawnorg[1]),
            static_cast<double>(v->drawnorg[2]), v->probe[0], v->probe[1], v->probe[2], v->probesum);
    }
    check(asLaidOut, "each seat drawn in its layout tile (SCR_SeatRect)");
    check(inside && apart, "the tiles inside the window and apart");
    check(area == static_cast<long long>(win.width) * win.height,
        va("the tiles cover the window (%lld of %d pixels)", area, win.width * win.height));
}

void checkViews()
{
    const int n = renderTest.seats;
    vec3_t players[maxSeats];
    bool have = true;
    for(int s = 0; s < n; s++)
    {
        have = have && playerOrigin(s, players[s]);
    }
    if(!check(have, "every seat's client has its own player"))
    {
        return;
    }
    for(int s = 0; s < n; s++)
    {
        const seatview_t* v = V_SeatViewOf(s);
        int nearest = -1;
        float best = 1e9f;
        for(int o = 0; o < n; o++)
        {
            const float d = flatDistance(v->drawnorg, players[o]);
            if(d < best)
            {
                best = d;
                nearest = o;
            }
        }
        check(nearest == s && best < 32.f, va("seat %d's tile drawn from its own player's eye (%.1f units; nearest seat %d)",
            s + 1, static_cast<double>(best), nearest + 1));
        check(markerIs(s, v->probe), va("seat %d's tile shows its marker (%d %d %d)", s + 1, v->probe[0], v->probe[1],
            v->probe[2]));
        for(int o = 0; o < s; o++)
        {
            check(v->probesum != V_SeatViewOf(o)->probesum, va("seat %d's picture is not seat %d's", s + 1, o + 1));
        }
    }
}

// Each seat's drawn body (vr_body_mode: the flat body) is its own: posed at its own player (the nearest seat player to
// the posed head), kept apart from the others' (avatar::seatSwitched).
void checkBodies()
{
    const int n = renderTest.seats;
    vec3_t players[maxSeats];
    for(int s = 0; s < n; s++)
    {
        if(!playerOrigin(s, players[s]))
        {
            return; // (checkViews said so)
        }
    }
    for(int s = 0; s < n; s++)
    {
        const Scope in(at(s));
        avatar::Skeleton sk;
        if(!check(avatar::skeleton(sk), va("seat %d's body posed", s + 1)))
        {
            continue;
        }
        const vec3_t head = {sk.head.x, sk.head.y, sk.head.z};
        int nearest = -1;
        float best = 1e9f;
        for(int o = 0; o < n; o++)
        {
            const float d = flatDistance(head, players[o]);
            if(d < best)
            {
                best = d;
                nearest = o;
            }
        }
        check(nearest == s && best < 32.f, va("seat %d's body posed at its own player (%.1f units; nearest seat %d)", s + 1,
            static_cast<double>(best), nearest + 1));
    }
}

void testFrame()
{
    RenderTest& t = renderTest;
    t.frames++;
    if(t.frames > stepFrames)
    {
        check(false, va("timed out (step %d)", static_cast<int>(t.step)));
        finish();
        return;
    }
    switch(t.step)
    {
        case Step::Off: return;
        case Step::WaitGame:
            if(!localGameInPlay())
            {
                return;
            }
            check(SCR_SeatsDrawn() == 1, "one seat: drawn alone");
            Cvar_Set("scr_seatmarkers", "2");
            t.notifyTime = Cvar_VariableValue("con_notifytime");
            Cvar_Set("con_notifytime", "0"); // (the console's notify lines, over the whole window, would hide a marker)
            for(int s = 1; s < t.seats; s++)
            {
                IN_MockPad(s - 1, true);
                Cbuf_AddText(va("seat_join %d\n", s - 1));
            }
            go(Step::WaitJoined);
            return;
        case Step::WaitJoined:
        {
            for(int s = 1; s < t.seats; s++)
            {
                if(!signedOn(s))
                {
                    return;
                }
            }
            check(inPlay() == t.seats, va("%d seats play", t.seats));
            for(int s = 0; s < t.seats; s++) // (each seat its own way: the tiles' pictures differ even from one spot)
            {
                const Scope in(at(s));
                cl.viewangles[YAW] = 90.f * static_cast<float>(s) + 30.f;
                cl.viewangles[PITCH] = 0.f;
            }
            go(Step::WaitDrawn);
            return;
        }
        case Step::WaitDrawn:
            for(int s = 0; s < t.seats; s++)
            {
                if(V_SeatViewOf(s)->drawnframes - t.drawnAt[s] < drawsWanted)
                {
                    return;
                }
            }
            checkTiles();
            checkViews();
            checkBodies();
            Cbuf_AddText("screenshot\n"); // (screenshots/: run.sh -Out composes it)
            go(Step::WaitShot);
            return;
        case Step::WaitShot:
            if(t.frames < 4)
            {
                return;
            }
            for(int s = t.seats - 1; s >= 1; s--)
            {
                Cbuf_AddText(va("seat_leave %d\n", s + 1));
                IN_MockPad(s - 1, false);
            }
            go(Step::WaitAlone);
            return;
        case Step::WaitAlone:
        {
            if(inPlay() != 1 || V_SeatViewOf(0)->drawnframes - t.drawnAt[0] < drawsWanted)
            {
                return;
            }
            vrect_t win;
            SCR_SeatWindow(&win);
            const vrect_t& r = V_SeatViewOf(0)->drawnrect;
            check(SCR_SeatsDrawn() == 1 && r.x == 0 && r.y == 0 && r.width == win.width && r.height == win.height,
                va("back to one seat: drawn in the whole window (%dx%d)", r.width, r.height));
            finish();
            return;
        }
    }
}

// vr_seat_render_test [n]: n seats (2-4, default 2) drawn, each in its tile.
void renderTest_f()
{
    renderTest = RenderTest{};
    renderTest.seats = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 2;
    if(renderTest.seats < 2 || renderTest.seats > maxSeats)
    {
        Con_Printf("vr_seat_render_test <2-%d>\n", maxSeats);
        return;
    }
    go(Step::WaitGame);
    Con_Printf("seat_render_test: started (%d seats)\n", renderTest.seats);
}

} // namespace

void registerRenderCommands()
{
    Cmd_AddCommand("vr_seat_render_test", renderTest_f);
}

void renderTestFrame()
{
    if(renderTest.step != Step::Off)
    {
        testFrame();
    }
}

} // namespace qvr::seat
