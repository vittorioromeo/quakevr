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
// Phase 7 part 2: each seat's own drawn body (vr_avatar: posed at its own player); the shared particles fed once per
// event (an explosion every seat reads drawn once, one only a later seat reads drawn too).
#include "vr_seat.hpp"

#include "vr_avatar.hpp"
#include "vr_cvars.hpp"
#include "vr_emissive.hpp"
#include "vr_engine.hpp"
#include "vr_geom.hpp"
#include "vr_particles.hpp"
#include "vr_protocol.hpp"
#include "vr_api.h"
#include "vr_visor.hpp"

#include "Zancle/Math/Abs.hpp"
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
    WaitBooted, // every seat's visor powered on (its boot sweep), then the screenshot
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
    bool torches = false;      // vr_seat_render_test <n> torches: each seat's own torch lights checked (a map with torches)
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

// The shared particle pool fed once per event (CL_SeatEffectFirst): an explosion every seat's client reads in a frame adds
// one explosion's particles, one only a later seat reads adds its own; an entity's trail is one seat's a frame.
void checkEffects()
{
    const int n = renderTest.seats;
    vec3_t org;
    if(!playerOrigin(0, org))
    {
        return;
    }
    org[2] += 64.f;
    const int before = particles::liveCount();
    for(int s = 0; s < n; s++)
    {
        const Scope in(at(s));
        R_ParticleExplosion(org);
    }
    const int once = particles::liveCount() - before;
    {
        const Scope in(at(0));
        R_ParticleExplosion(org);
    }
    const int again = particles::liveCount() - before - once;
    check(once > 0 && again > 0 && za::abs(again - once) <= once / 4,
        va("an explosion all %d seats read: its particles once (%d; one seat's: %d)", n, once, again));
    org[0] += 48.f;
    const int before2 = particles::liveCount();
    {
        const Scope in(at(n - 1));
        R_ParticleExplosion(org);
    }
    check(particles::liveCount() > before2, va("an explosion only seat %d reads: its particles (%d)", n,
        particles::liveCount() - before2));
    // An entity's trail (entity numbers are the server's: the same in every seat's client): one seat's a frame.
    constexpr int trailKey = MAX_EDICTS - 1; // (no entity of the test's)
    int fed = 0;
    for(int s = 0; s < n; s++)
    {
        const Scope in(at(s));
        fed += CL_SeatEffectFirst(SEATFX_TRAIL, trailKey, nullptr) ? 1 : 0;
    }
    check(fed == 1, va("an entity's trail fed by one seat of %d (%d)", n, fed));

    // Part 3: a server event (a message's bytes) every seat reads, its Quake VR particles spawned in each seat's parse
    // (CL_SeatFxBegin/End): one seat's kept; a later seat's own event (other bytes) kept too.
    byte event[8] = {'s', 'e', 'a', 't', 'f', 'x', 0, 1};
    const sizebuf_t savedMsg = net_message;
    const int savedRead = msg_readcount;
    net_message.data = event;
    net_message.cursize = sizeof(event);
    const auto parse = [&](const int s) {
        const Scope in(at(s));
        msg_readcount = 0;
        seatfxmark_t mark;
        CL_SeatFxBegin(&mark, 0);
        (void)particles::spawn(geom::vec(org), glm::vec3{0.f, 0.f, 1.f}, particles::Preset::Steam, 24);
        msg_readcount = sizeof(event);
        CL_SeatFxEnd(&mark);
    };
    const int before3 = particles::liveCount();
    for(int s = 0; s < n; s++)
    {
        parse(s);
    }
    const int all = particles::liveCount() - before3;
    event[7] = 2; // (another event)
    const int before4 = particles::liveCount();
    parse(n - 1);
    const int own = particles::liveCount() - before4;
    net_message = savedMsg;
    msg_readcount = savedRead;
    check(all > 0 && own > 0 && za::abs(all - own) <= own / 2,
        va("a Quake VR event all %d seats read: its particles once (%d; one seat's: %d)", n, all, own));
}

// Phase 8: a Quake VR message every seat reads (a gore sound: its memories are all shared): the first seat's parse reads
// it, a later seat's is a repeat (CL_SeatFxBegin's look-ahead: its length) whose bytes VR_ParseServerMessage skips;
// another event (other bytes) is no repeat. Each seat's player found as its seat's (seatOfPlayer: the casings).
void checkEvents()
{
    const int n = renderTest.seats;
    byte buf[64];
    sizebuf_t msg{};
    msg.data = buf;
    msg.maxsize = sizeof(buf);
    const auto write = [&](const int strength) {
        SZ_Clear(&msg);
        MSG_WriteByte(&msg, protocol::QVR_SVC_GORESOUND);
        MSG_WriteByte(&msg, 255); // (no kind: the parse reads it, plays nothing)
        MSG_WriteShort(&msg, 0);
        for(int i = 0; i < 3; i++)
        {
            MSG_WriteCoord(&msg, 0.f, cl.protocolflags);
        }
        MSG_WriteByte(&msg, strength);
    };
    const sizebuf_t savedMsg = net_message;
    const int savedRead = msg_readcount;
    const qboolean savedBad = msg_badread;
    const auto use = [&] {
        net_message.data = buf;
        net_message.cursize = msg.cursize;
        net_message.maxsize = msg.maxsize;
        msg_readcount = 0;
        msg_badread = false;
    };
    // A later seat's look-ahead (its parse's mark): the repeat's length, 0 if none.
    const auto probe = [&] {
        use();
        seatfxmark_t mark;
        CL_SeatFxBegin(&mark, 0);
        const int repeat = mark.repeat;
        msg_readcount = msg.cursize;
        CL_SeatFxEnd(&mark);
        return repeat;
    };
    write(1);
    int repeats = 0, readWhole = 0;
    for(int s = 0; s < n; s++)
    {
        const Scope in(at(s));
        if(s > 0)
        {
            repeats += probe() == msg.cursize ? 1 : 0;
        }
        use();
        const int handled = VR_ParseServerMessage(protocol::svc_quakevr);
        readWhole += handled && msg_readcount == msg.cursize && !msg_badread ? 1 : 0;
    }
    write(2); // (another event)
    int fresh = 0;
    {
        const Scope in(at(n - 1));
        fresh = probe();
    }
    net_message = savedMsg;
    msg_readcount = savedRead;
    msg_badread = savedBad;
    check(repeats == n - 1 && readWhole == n,
        va("a Quake VR message all %d seats read: a repeat in seats 2-%d (%d), read whole by each (%d)", n, n, repeats, readWhole));
    check(fresh == 0, va("another Quake VR message seat %d reads: no repeat (%d)", n, fresh));
    int own = 0;
    for(int s = 0; s < n; s++)
    {
        const Scope in(at(s));
        own += seatOfPlayer(cl.viewentity) == s ? 1 : 0;
    }
    check(own == n, va("each seat's player its seat's (%d of %d)", own, n));
}

// Part 3 (vr_seat_render_test <n> torches, on a map with torches by the starts: e1m6): each drawn seat's torch lights
// are its own client's, chosen by its own eye (VR_TorchLights in its relink): some lit, all near its eye.
void checkTorches()
{
    for(int s = 0; s < renderTest.seats; s++)
    {
        const Scope in(at(s));
        const seatview_t* v = V_SeatViewOf(s);
        float farthest = 0.f;
        const int lit = emissive::torchLightsLit(glm::vec3{v->drawnorg[0], v->drawnorg[1], v->drawnorg[2]}, farthest);
        check(lit > 0 && farthest < 1200.f, va("seat %d's own torch lights by its eye (%d lit, the farthest %.0f units off)",
            s + 1, lit, static_cast<double>(farthest)));
    }
}

// The channels playing sfx (S_StartSound's; read right after a start: its S_MixJoin left no mix posted).
[[nodiscard]] const channel_t* channelOf(const sfx_t* sfx, int* count)
{
    const channel_t* last = nullptr;
    *count = 0;
    for(int i = NUM_AMBIENTS; i < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; i++)
    {
        if(snd_channels[i].sfx == sfx)
        {
            last = &snd_channels[i];
            ++*count;
        }
    }
    return last;
}

// Phase 7 part 3: the sounds from each drawn seat's listener. One listener a seat; a sound every seat's client reads in a
// frame started once; a sound only the last seat reads (at its own eye) started, and heard at full volume there (the
// loudest listener's, its own seat's, not seat 1's alone).
void checkSounds()
{
    const int n = renderTest.seats;
    check(S_SeatListeners() == n, va("%d sound listeners, one a drawn seat (%d)", n, S_SeatListeners()));
    sfx_t* all = S_PrecacheSound("misc/talk.wav");
    sfx_t* last = S_PrecacheSound("misc/menu1.wav");
    vec3_t org;
    if(!check(shm && all && last && playerOrigin(0, org), "the sound system up (the test runs with -Sound, volume 0)"))
    {
        return;
    }
    org[2] += 32.f;
    int before = 0, after = 0;
    (void)channelOf(all, &before);
    for(int s = 0; s < n; s++)
    {
        const Scope in(at(s));
        S_StartSound(0, 0, all, org, 1.f, 1.f);
    }
    (void)channelOf(all, &after);
    check(after - before == 1, va("a sound all %d seats read: started once (%d)", n, after - before));
    vec3_t eye;
    VectorCopy(V_SeatViewOf(n - 1)->drawnorg, eye);
    {
        const Scope in(at(n - 1));
        S_StartSound(0, 0, last, eye, 1.f, 1.f);
    }
    const channel_t* ch = channelOf(last, &after);
    vec3_t d;
    VectorSubtract(eye, V_SeatViewOf(0)->drawnorg, d);
    const float seat1 = 2.f * 255.f * (1.f - VectorLength(d) / 1000.f); // (seat 1's alone: SND_Spatialize's at most)
    check(ch && ch->leftvol + ch->rightvol == 2 * ch->master_vol,
        va("a sound only seat %d reads, at its eye: started, full volume from its listener (%d+%d of %d; seat 1's alone "
           "at most %.0f, %.0f units off)",
            n, ch ? ch->leftvol : -1, ch ? ch->rightvol : -1, ch ? 2 * ch->master_vol : -1,
            static_cast<double>(seat1 > 0.f ? seat1 : 0.f), static_cast<double>(VectorLength(d))));
}

// Phase 8: the underwater muffling per seat. Seat 1 alone under water (its cl.forceunderwater, set as the seats turned):
// one listener under water; the mix as a whole unfiltered, a sound seat 1's player makes muffled alone (its channel's
// seatwater: snd_mix.c paints it apart and filters it), one at the last seat's eye clear. Seat 1 out of the water after.
void checkWater()
{
    const int n = renderTest.seats;
    int wet = 0;
    for(int i = 0; i < S_SeatListeners(); i++)
    {
        wet += S_SeatWater(i);
    }
    check(wet == 1 && S_SeatsUnderwater(), va("one listener of %d under water, seat 1's (%d)", n, wet));
    sfx_t* ownSfx = S_PrecacheSound("misc/h2ohit1.wav");
    sfx_t* otherSfx = S_PrecacheSound("misc/outwater.wav");
    int count = 0;
    {
        const Scope in(at(0));
        S_StartSound(cl.viewentity, 0, ownSfx, cl_entities[cl.viewentity].origin, 1.f, 1.f);
    }
    const channel_t* own = channelOf(ownSfx, &count);
    vec3_t eye;
    VectorCopy(V_SeatViewOf(n - 1)->drawnorg, eye);
    {
        const Scope in(at(n - 1));
        S_StartSound(0, 1, otherSfx, eye, 1.f, 1.f);
    }
    const channel_t* other = channelOf(otherSfx, &count);
    check(own && other && own != other && own->seatwater && !other->seatwater,
        va("seat 1's own sound muffled alone (%d), seat %d's clear (%d)", own ? own->seatwater : -1, n,
            other ? other->seatwater : -1));
    // The static sounds (the level's, sent to every seat's client as it signs on): started once (seat 1's client; the
    // others' start none), spatialized from every listener as the other channels are: none twice.
    int statics = 0, twice = 0;
    for(int i = NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; i < total_channels; i++)
    {
        const channel_t& a = snd_channels[i];
        statics += a.sfx ? 1 : 0;
        for(int j = NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; a.sfx && j < i; j++)
        {
            twice += snd_channels[j].sfx == a.sfx && VectorCompare(snd_channels[j].origin, a.origin) ? 1 : 0;
        }
    }
    check(twice == 0, va("the level's static sounds started once (%d, none twice: %d)", statics, twice));
    const Scope in(at(0));
    cl.forceunderwater = false;
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
                cl.forceunderwater = s == 0; // (phase 8: seat 1 alone under water: its sounds muffled alone; checkWater)
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
            checkEffects();
            checkEvents();
            checkSounds();
            checkWater();
            if(t.torches)
            {
                checkTorches();
            }
            for(int s = 0; s < t.seats; s++) // (part 3: each seat's flat visor, drawn in its tile: the screenshot shows it)
            {
                const Scope in(at(s));
                check((visor::active() ? 1 : 0) == (vr_flat_visor.value != 0.f ? 1 : 0),
                    va("seat %d's visor HUD as vr_flat_visor %g says (%d)", s + 1, static_cast<double>(vr_flat_visor.value),
                        visor::active() ? 1 : 0));
            }
            go(Step::WaitBooted);
            return;
        case Step::WaitBooted:
            for(int s = 0; s < t.seats; s++) // (each seat's visor powered on before the screenshot: its panels shown)
            {
                const Scope in(at(s));
                if(visor::active() && !visor::booted())
                {
                    return;
                }
            }
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

// vr_seat_render_test [n] [torches]: n seats (2-4, default 2) drawn, each in its tile (torches: each seat's torch lights).
void renderTest_f()
{
    renderTest = RenderTest{};
    renderTest.seats = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 2;
    renderTest.torches = Cmd_Argc() > 2 && !q_strcasecmp(Cmd_Argv(2), "torches");
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
