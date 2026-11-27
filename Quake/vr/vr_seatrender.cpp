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
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include "vr_avatar.hpp"
#include "vr_backend.hpp"
#include "vr_blood.hpp"
#include "vr_cvars.hpp"
#include "vr_emissive.hpp"
#include "vr_engine.hpp"
#include "vr_geom.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_mpavatar.hpp"
#include "vr_particles.hpp"
#include "vr_protocol.hpp"
#include "vr_text3d.hpp"
#include "vr_trace.hpp"
#include "vr_weapon_ids.h"
#include "vr_api.h"
#include "vr_api_render.h"
#include "vr_view.hpp"
#include "vr_visor.hpp"

#include "Zancle/Math/Abs.hpp"
#include "vr_testkit.hpp"


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
    Hands,      // split13: over a few drawn frames, each seat's first-person hands, gun and ammo screen, the others' avatars
    AnimFind,   // split13 (anim): a grunt spawned ahead of both seats, found in seat 1's client, drawn by both
    AnimLive,   // its pose drawn the same in both seats' passes of each frame, and animating
    AnimDead,   // killed: its ragdoll drawn in both passes of each frame, the same pose
    AnimOneNear, // seat 2 turned away: seat 1 draws the ragdoll (never the standing pose) each frame
    GunsSame,   // split18 (guns): both seats a nail pistol in the main hand; seat 1 fires: seat 2's gun never animates
    GunsApart,  // seat 2's nail pistol in its off hand: each seat's drawn in its own hand every frame, seat 1 firing
    WaitBooted, // every seat's visor powered on (its boot sweep), then the screenshot
    WaitShot,   // the screenshot taken (the frame after its command)
    FlashBase,  // split11: no seat under water; each seat's picture read (its probe grid), then every flash on every seat
    FlashOn,    // the flashes held (damage, burn, water, the pain's blood); each tile still its own view; a screenshot
    FlashShot,  // the screenshot taken; the flashes off
    FlashOff,   // each tile back to its picture (nothing stuck)
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
    byte base[maxSeats][SEAT_PROBEGRID * SEAT_PROBEGRID] = {}; // split11: each seat's picture before the flashes
    // split13 (Step::Hands), by seat: the frames read, those whose pass was wrong (and the last such pass), its ammo
    // screen found on its gun, the screen's text changing, the farthest another seat's avatar hand was from that seat's own
    struct Hands
    {
        int frames = 0;
        int badPasses = 0;
        view::SeatPass bad;
        int screenFound = 0;
        int screenChanged = 0;
        za::U32 screenHash = 0;
        float avatarOff[maxSeats][2] = {}; // seat o's avatar hands as this seat draws them, from seat o's own (units)
        float avatarApart[maxSeats] = {}; // and from this seat's own hands (the bug: these near 0)
        int avatarSeen[maxSeats] = {};
    } hands[maxSeats];
    int handsDrawn[maxSeats] = {}; // each seat's drawn count at the last frame read
    bool handsArmed = false;       // every seat's gun drawn in its hand (the frames read from then)
    // split13 (vr_seat_render_test 2 anim): the probed monster (its entity number), each seat's pass's draw of it last
    // (poseProbe: the frame, its pose's hash, its bones), and the anim steps' tallies
    bool anim = false;
    int probeNum = 0;
    struct Probe
    {
        int frame = -1;
        za::U32 hash = 0;
        int bones = 0;
    } probe[maxSeats];
    int animRead = 0;    // frames read in the step
    int animSame = 0;    // both seats drew it, the same pose
    int animBones = 0;   // ... and as a ragdoll (bones)
    int animChanged = 0; // seat 1's pose changed from the frame before
    int animNear = 0;    // (AnimOneNear) seat 1 drew it as a ragdoll
    int animFar = 0;     // (AnimOneNear) seat 2 drew it
    za::U32 animLast = 0;
    float yaw1 = 0.f;    // seat 1's yaw (seat 2 turned so for the anim steps)
    int animFlicker = 0;           // split18: a seat's pose back to the one two frames before (readAnim)
    za::U32 animPrev[2][2] = {};   // each seat's last two poses' hashes
    // split18 (vr_seat_render_test 2 guns): each seat's held guns as its pass drew them last (poseProbe: the frame, the
    // pose's hash, the model matrix's origin), and the guns steps' tallies by seat
    bool guns = false;
    struct GunProbe
    {
        int frame = -1;
        za::U32 hash = 0;
        glm::vec3 at{0.f};
        const qmodel_t* model = nullptr;
    } gunProbe[maxSeats][2];
    int gunHand[maxSeats] = {};     // the hand each seat's nail pistol is in this step
    int gunRead = 0;                // frames read in the step
    int gunDrawn[maxSeats] = {};    // its nail pistol drawn in its pass that frame, a nail pistol
    int gunNear[maxSeats] = {};     // ... within gunHandReach of its own hand
    float gunFar[maxSeats] = {};    // the farthest it was from that hand
    int gunChanges[maxSeats] = {};  // its pose changed from the frame before
    za::U32 gunLast[maxSeats] = {};
    float frameRate = 0.f; // split18: vr_fixed_frames_rate before the anim run set its own (0: not set)
} renderTest;

constexpr int animFramesWanted = 16; // split13: drawn frames read in each anim step
constexpr int animChangesWanted = 8;  // split18: the live grunt's pose changes seen (it animates) before AnimLive's check
constexpr int animLiveMaxFrames = 288; // ... read at most (2 s at the anim run's 144 frames a second)
// split18: the anim run's frames a second (vr_fixed_frames_rate): twice the server's ticks, so half the client frames read
// no update for the monster (his test at a high refresh rate: seat 2 drew it in its rest pose every other frame)
constexpr float animFrameRate = 144.f;
constexpr const char* gruntModel = "progs/soldier.mdl";

constexpr int handFramesWanted = 8; // split13: drawn frames read in Step::Hands
constexpr int gunSettleFrames = 72;  // split18: frames after the guns are given before they are read (in the hands, raised)
constexpr int gunFramesWanted = 48;  // ... drawn frames read in each guns step (seat 1 firing from the first)
constexpr float gunHandReach = 32.f; // units: a held gun's model origin from its hand's (the bug: left where it was)
// units: another seat's avatar hand from that seat's drawn hand (the networked hand as any client draws it: the empty off
// hand 5-6 units off, the gun hand 1-2; split12's bug drew it at the viewing seat's own hand, ~100 units off here)
constexpr float avatarHandTolerance = 8.f;
constexpr float screenReach = 16.f; // units: an ammo screen from its gun's origin

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
    if(renderTest.frameRate > 0.f)
    {
        Cvar_SetValue("vr_fixed_frames_rate", renderTest.frameRate);
    }
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
    return za::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]));
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
        {
            // split12: r_refdef is the current seat's (Seat::refdef): its input (the sighted mouse's fov scale) and its
            // client frame read its own view, not the last drawn seat's
            const Scope in(at(s));
            const float d = flatDistance(r_refdef.vieworg, v->drawnorg) + za::abs(r_refdef.vieworg[2] - v->drawnorg[2]);
            check(d < 0.01f, va("seat %d's r_refdef is its own view's in its frame (%.2f units off; its fov %.1f)", s + 1,
                static_cast<double>(d), static_cast<double>(r_refdef.basefov)));
        }
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
    // split11: each seat's pass adds its own first-person hands, weapons and body (seats 2-4 drew none: the view
    // entities went in once a frame, by seat 1's pass).
    for(int s = 0; s < n; s++)
    {
        int frame = -1;
        const int added = view::ownEntitiesAdded(s, frame);
        check(added >= 2 && frame >= host_framecount - 1,
            va("seat %d draws its own hands, weapons and body (%d view entities, %d frames ago)", s + 1, added,
                host_framecount - frame));
    }
    // split11: every seat sees every other seat's player as his IK avatar (vr_mpavatar's copies, each seat's own), not the
    // player model: seat 2 saw the host as id's ranger (one shared set of copies, added once a frame by seat 1's pass).
    for(int s = 0; s < n; s++)
    {
        int seen = 0;
        char missing[32] = "";
        for(int o = 0; o < n; o++)
        {
            const clientseat_t* net = CL_SeatNetIfMade(o);
            if(o == s || !net)
            {
                continue;
            }
            const int ent = net->cstate.viewentity;
            const Scope in(at(s));
            if(mpavatar::drawnNow(ent))
            {
                seen++;
            }
            else
            {
                q_strlcat(missing, va(" %d", o + 1), sizeof(missing));
            }
        }
        check(seen == n - 1, va("seat %d sees the other %d seats as IK avatars (%d; missing:%s)", s + 1, n - 1, seen,
            missing[0] ? missing : " none"));
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

// split11: every flash on seat s (the current one): a hit's damage flash, a burn's (fire's orange, as the bonus shift
// carries it), the water's warp (cl.forceunderwater) and the pain's blood on the screen (vr_blood). Held each frame (the
// shifts decay).
void flashesOn()
{
    cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
    cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
    cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
    cl.cshifts[CSHIFT_DAMAGE].percent = 60;
    cl.cshifts[CSHIFT_BONUS].destcolor[0] = 255;
    cl.cshifts[CSHIFT_BONUS].destcolor[1] = 96;
    cl.cshifts[CSHIFT_BONUS].destcolor[2] = 0;
    cl.cshifts[CSHIFT_BONUS].percent = 30;
    cl.forceunderwater = true;
}

void flashesOff()
{
    cl.cshifts[CSHIFT_DAMAGE].percent = 0;
    cl.cshifts[CSHIFT_BONUS].percent = 0;
    cl.forceunderwater = false;
    blood::clear();
}

// How alike pictures a and b are, whatever their brightness and contrast (a tint, a flash): the correlation of their
// blocks' luminance, -1..1 (0 for a flat picture). A view squashed into part of its tile (the warp's texture scale from
// the tile's size: split11's bug) or a stale picture in it does not follow the view's own: low.
[[nodiscard]] float pictureCorrelation(const byte* a, const byte* b)
{
    // (the inner blocks: the pain's blood lies round the screen's edges)
    constexpr int g = SEAT_PROBEGRID;
    double ma = 0.0, mb = 0.0;
    int n = 0;
    for(int y = 1; y < g - 1; y++)
    {
        for(int x = 1; x < g - 1; x++)
        {
            ma += a[y * g + x];
            mb += b[y * g + x];
            n++;
        }
    }
    ma /= n;
    mb /= n;
    double sab = 0.0, saa = 0.0, sbb = 0.0;
    for(int y = 1; y < g - 1; y++)
    {
        for(int x = 1; x < g - 1; x++)
        {
            const double da = a[y * g + x] - ma, db = b[y * g + x] - mb;
            sab += da * db;
            saa += da * da;
            sbb += db * db;
        }
    }
    return saa > 1e-6 && sbb > 1e-6 ? static_cast<float>(sab / za::sqrt(saa * sbb)) : 0.f;
}

[[nodiscard]] float pictureDiff(const byte* a, const byte* b)
{
    constexpr int n = SEAT_PROBEGRID * SEAT_PROBEGRID;
    int sum = 0;
    for(int i = 0; i < n; i++)
    {
        sum += za::abs(static_cast<int>(a[i]) - static_cast<int>(b[i]));
    }
    return static_cast<float>(sum) / n;
}

// split13: one drawn frame of every seat read (Step::Hands): each seat's pass (its own hands and gun once each, no other
// seat's first-person entity, no ghost of its gun), its ammo screen on its own gun and its text, the other seats'
// avatars' hands against those seats' own drawn hands. False: not every seat has drawn a new frame yet.
bool readHands()
{
    RenderTest& t = renderTest;
    const int n = t.seats;
    for(int s = 0; s < n; s++)
    {
        if(V_SeatViewOf(s)->drawnframes == t.handsDrawn[s])
        {
            return false;
        }
    }
    view::SeatPass pass[maxSeats];
    bool armed = true;
    for(int s = 0; s < n; s++)
    {
        t.handsDrawn[s] = V_SeatViewOf(s)->drawnframes;
        const Scope in(at(s));
        pass[s] = view::seatPass();
        armed = armed && (pass[s].gun[0] || pass[s].gun[1]);
        if(t.frames % 300 == 299)
        {
            Con_Printf("seat_render_test: seat %d waits for its gun: weapon %d, items %d, guns %d %d, hands %d %d\n", s + 1,
                cl.stats[protocol::STAT_QVR_WEAPON], cl.items, pass[s].gun[0], pass[s].gun[1], pass[s].hand[0],
                pass[s].hand[1]);
        }
    }
    if(!t.handsArmed && !armed)
    {
        return false; // (the shotguns drawn into the hands first: WaitJoined's impulse 2)
    }
    t.handsArmed = true;
    for(int s = 0; s < n; s++)
    {
        RenderTest::Hands& h = t.hands[s];
        const view::SeatPass& p = pass[s];
        h.frames++;
        const int handsShown = (p.hand[0] ? 1 : 0) + (p.hand[1] ? 1 : 0);
        const int gunsShown = (p.gun[0] ? 1 : 0) + (p.gun[1] ? 1 : 0);
        const bool wrong =
            handsShown == 0 || p.ownHands != handsShown || gunsShown != 1 || p.ownGuns != 1 || p.foreign || p.ghosts;
        h.badPasses += wrong ? 1 : 0;
        if(wrong || h.badPasses == 0)
        {
            h.bad = p; // (the last wrong pass, else the last pass)
        }
        const int gunHand = p.gun[1] ? 1 : 0; // ([1] the main hand)
        const glm::vec3* pos = nullptr;
        const za::U32* hash = nullptr;
        const int screens = text3d::passScreens(s, pos, hash);
        int nearest = -1;
        float best = screenReach;
        for(int i = 0; i < screens; i++)
        {
            const float d = glm::distance(pos[i], p.gunAt[gunHand]);
            if(d < best)
            {
                best = d;
                nearest = i;
            }
        }
        if(nearest >= 0)
        {
            h.screenChanged += h.screenFound > 0 && hash[nearest] != h.screenHash ? 1 : 0;
            h.screenHash = hash[nearest];
            h.screenFound++;
        }
        for(int o = 0; o < n; o++)
        {
            const clientseat_t* net = CL_SeatNetIfMade(o);
            if(o == s || !net)
            {
                continue;
            }
            entity_t* drawn[3];
            bool doll = false;
            const Scope in(at(s));
            if(!mpavatar::woundTargets(net->cstate.viewentity, drawn, doll))
            {
                continue;
            }
            for(int side = 0; side < 2; side++)
            {
                if(!pass[o].hand[side] || !drawn[1 + side])
                {
                    continue;
                }
                const glm::vec3 at{drawn[1 + side]->origin[0], drawn[1 + side]->origin[1], drawn[1 + side]->origin[2]};
                h.avatarOff[o][side] = za::max(h.avatarOff[o][side], glm::distance(at, pass[o].handAt[side]));
                const float apart = p.hand[side] ? glm::distance(at, p.handAt[side]) : 1e9f;
                h.avatarApart[o] = h.avatarSeen[o] ? za::min(h.avatarApart[o], apart) : apart;
                h.avatarSeen[o]++;
            }
        }
    }
    return true;
}

void checkHands()
{
    const RenderTest& t = renderTest;
    for(int s = 0; s < t.seats; s++)
    {
        const RenderTest::Hands& h = t.hands[s];
        const view::SeatPass& b = h.bad;
        check(h.badPasses == 0,
            va("seat %d's pass: own hands %d/%d, gun %d/%d, foreign %d, ghost guns %d (%d of %d frames wrong)", s + 1,
                b.ownHands, (b.hand[0] ? 1 : 0) + (b.hand[1] ? 1 : 0), b.ownGuns, (b.gun[0] ? 1 : 0) + (b.gun[1] ? 1 : 0),
                b.foreign, b.ghosts, h.badPasses, h.frames));
        check(h.screenFound == h.frames && h.screenChanged == 0,
            va("seat %d's ammo screen on its gun each pass, same text (%d of %d, changed %d)", s + 1, h.screenFound,
                h.frames, h.screenChanged));
        for(int o = 0; o < t.seats; o++)
        {
            if(o == s)
            {
                continue;
            }
            check(h.avatarSeen[o] > 0 && h.avatarOff[o][0] < avatarHandTolerance && h.avatarOff[o][1] < avatarHandTolerance,
                va("seat %d sees seat %d's avatar hands at its hands (%.2f %.2f off, %d read; %.0f from seat %d's)", s + 1,
                    o + 1, static_cast<double>(h.avatarOff[o][0]), static_cast<double>(h.avatarOff[o][1]), h.avatarSeen[o],
                    static_cast<double>(h.avatarApart[o]), s + 1));
        }
    }
}

// split13: every seat has drawn a new frame since the last read (handsDrawn), read now.
[[nodiscard]] bool newFrameDrawn()
{
    RenderTest& t = renderTest;
    for(int s = 0; s < t.seats; s++)
    {
        if(V_SeatViewOf(s)->drawnframes == t.handsDrawn[s])
        {
            return false;
        }
    }
    for(int s = 0; s < t.seats; s++)
    {
        t.handsDrawn[s] = V_SeatViewOf(s)->drawnframes;
    }
    return true;
}

// The grunt the anim run spawned, in seat 1's client (0: not there yet).
[[nodiscard]] int findGrunt()
{
    const Scope in(at(0));
    for(int i = cl.maxclients + 1; i < cl.num_entities; i++)
    {
        const entity_t& e = cl_entities[i];
        if(e.model && !q_strcasecmp(e.model->name, gruntModel))
        {
            return i;
        }
    }
    return 0;
}

// The anim steps' read of one drawn frame: both seats' passes drew the probed monster in the last frame, and alike.
void readAnim()
{
    RenderTest& t = renderTest;
    const RenderTest::Probe& a = t.probe[0];
    const RenderTest::Probe& b = t.probe[1];
    const int last = za::max(a.frame, b.frame);
    t.animRead++;
    const bool both = a.frame == last && b.frame == last;
    t.animSame += both && a.hash == b.hash ? 1 : 0;
    t.animBones += both && a.hash == b.hash && a.bones > 0 && b.bones > 0 ? 1 : 0;
    t.animChanged += t.animRead > 1 && a.hash != t.animLast ? 1 : 0;
    t.animLast = a.hash;
    t.animNear += a.frame == last && a.bones > 0 ? 1 : 0;
    t.animFar += b.frame == last ? 1 : 0;
    // split18: each seat's own draws, frame to frame: a pose that goes back to the one two frames before (his flicker:
    // the right pose, the rest pose, the right pose)
    for(int s = 0; s < 2; s++)
    {
        const za::U32 h = t.probe[s].hash;
        t.animFlicker += t.animRead > 2 && h == t.animPrev[s][1] && h != t.animPrev[s][0] ? 1 : 0;
        t.animPrev[s][1] = t.animPrev[s][0];
        t.animPrev[s][0] = h;
    }
}

// split18: the guns steps' read of one drawn frame: each seat's nail pistol (in t.gunHand[s]) drawn in its pass that frame,
// near its own hand, and its pose against the frame before.
void readGuns()
{
    RenderTest& t = renderTest;
    t.gunRead++;
    for(int s = 0; s < t.seats; s++)
    {
        const int h = t.gunHand[s];
        const RenderTest::GunProbe& g = t.gunProbe[s][h];
        if(g.frame != host_framecount && g.frame != host_framecount - 1)
        {
            continue; // (not drawn in its last pass)
        }
        if(!g.model || !modelmeta::is(g.model, modelmeta::Id::VNailPistol))
        {
            continue;
        }
        t.gunDrawn[s]++;
        const Scope in(at(s));
        const view::SeatPass p = view::seatPass();
        const float off = glm::distance(g.at, p.handAt[h]);
        t.gunFar[s] = za::max(t.gunFar[s], off);
        t.gunNear[s] += off < gunHandReach ? 1 : 0;
        t.gunChanges[s] += t.gunRead > 1 && g.hash != t.gunLast[s] ? 1 : 0;
        t.gunLast[s] = g.hash;
    }
}

void gunsGo(const Step s)
{
    RenderTest& t = renderTest;
    t.gunRead = 0;
    for(int i = 0; i < maxSeats; i++)
    {
        t.gunDrawn[i] = t.gunNear[i] = t.gunChanges[i] = 0;
        t.gunFar[i] = 0.f;
        t.gunLast[i] = 0;
    }
    go(s);
}

// The guns steps' checks: each seat's nail pistol drawn every frame in its own hand; seat 1's (firing) animating and seat 2's
// (idle) not.
void checkGuns(const char* what)
{
    RenderTest& t = renderTest;
    for(int s = 0; s < t.seats; s++)
    {
        check(t.gunDrawn[s] == t.gunRead && t.gunNear[s] == t.gunRead,
            va("%s: seat %d's nail pistol (%s hand) drawn every frame (%d of %d) in its hand (%d; at most %.1f units off)", what,
                s + 1, t.gunHand[s] == HAND_MAIN ? "main" : "off", t.gunDrawn[s], t.gunRead, t.gunNear[s],
                static_cast<double>(t.gunFar[s])));
    }
    check(t.gunChanges[0] > 0 && t.gunChanges[1] == 0,
        va("%s: seat 1 firing animates its own nail pistol alone (%d pose changes; seat 2's idle one %d)", what, t.gunChanges[0],
            t.gunChanges[1]));
}

void animGo(const Step s)
{
    RenderTest& t = renderTest;
    t.animRead = t.animSame = t.animBones = t.animChanged = t.animNear = t.animFar = t.animFlicker = 0;
    go(s);
}

[[nodiscard]] bool drawnSince(const int wanted)
{
    for(int s = 0; s < renderTest.seats; s++)
    {
        if(V_SeatViewOf(s)->drawnframes - renderTest.drawnAt[s] < wanted)
        {
            return false;
        }
    }
    return true;
}

constexpr float flashAlikeMin = 0.4f; // a flashed tile's picture still its view's (pictureCorrelation: 0.7-0.9 here, 0.49 for seat 1 of 3 since split16: its wide tile faces a dark wall and the pain vignette covers the lit edges; a broken one < 0.2)
constexpr float backDiffMax = 6.f;      // a tile back to its picture once the flashes are off

void testFrame()
{
    RenderTest& t = renderTest;
    t.frames++;
    if(t.frames > stepFrames)
    {
        check(false, va("timed out (step %d; probe %d drawn %d %d, %d)", static_cast<int>(t.step), t.probeNum, t.probe[0].frame, t.probe[1].frame, host_framecount));
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
                // (split13: a shotgun in the main hand, test impulse 150 + its id: its ammo screen read in Step::Hands)
                Cbuf_AddText(va("seat %d impulse %d\n", s + 1, 150 + WID_SHOTGUN));
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
            for(int s = 0; s < t.seats; s++)
            {
                t.handsDrawn[s] = V_SeatViewOf(s)->drawnframes;
            }
            go(Step::Hands);
            return;
        case Step::Hands:
            if(!readHands() || t.hands[0].frames < handFramesWanted)
            {
                return;
            }
            checkHands();
            if(t.guns)
            {
                // split18: a nail pistol (test impulse 149, vr_test_give_wid) in both seats' main hands (his report's gun:
                // its drawn slide, vr_nailslide.cpp)
                Cbuf_AddText(va("vr_test_give_wid %d; seat 1 impulse 149; seat 2 impulse 149\n", WID_NAILPISTOL));
                t.gunHand[0] = t.gunHand[1] = HAND_MAIN;
                gunsGo(Step::GunsSame);
                return;
            }
            if(!t.anim)
            {
                go(Step::WaitBooted);
                return;
            }
            {
                // split18: the grunt goes 160 units ahead of seat 1: the way with the most room (seat 1's own 30
                // degrees faced a wall at e1m1's start: the grunt spawned in it, out of every client's sight)
                const Scope in(at(0));
                vec3_t org{};
                t.yaw1 = cl.viewangles[YAW];
                if(playerOrigin(0, org))
                {
                    float best = -1.f;
                    for(int y = 0; y < 360; y += 15)
                    {
                        const float r = glm::radians(static_cast<float>(y));
                        const glm::vec3 from{org[0], org[1], org[2]};
                        const float room = worldtrace::line(from, from + glm::vec3{za::cos(r), za::sin(r), 0.f} * 260.f);
                        if(room > best + 0.01f)
                        {
                            best = room;
                            t.yaw1 = static_cast<float>(y);
                        }
                    }
                }
                cl.viewangles[YAW] = t.yaw1;
            }
            for(int s = 1; s < t.seats; s++) // (both seats face the grunt: it is spawned ahead of seat 1)
            {
                const Scope in(at(s));
                cl.viewangles[YAW] = t.yaw1;
            }
            Cbuf_AddText("seat 1 god; seat 2 god; vr_test_spawn_dist 160; vr_test_spawn 0; seat 1 impulse 241\n");
            animGo(Step::AnimFind);
            return;
        case Step::AnimFind:
            if(!t.probeNum)
            {
                t.probeNum = findGrunt();
                return;
            }
            if(!newFrameDrawn() || t.probe[0].frame < 0 || t.probe[0].frame != t.probe[1].frame)
            {
                return; // (both seats' passes draw it)
            }
            animGo(Step::AnimLive);
            return;
        case Step::AnimLive:
            if(!newFrameDrawn())
            {
                return;
            }
            readAnim();
            // (split18: until it has animated a few times too: a monster's first think is up to half a second after its
            // spawn, standing in its first frame meanwhile)
            if(t.animRead < animFramesWanted || (t.animChanged < animChangesWanted && t.animRead < animLiveMaxFrames))
            {
                return;
            }
            check(t.animSame == t.animRead && t.animChanged > 0 && t.animFlicker == 0,
                va("a live grunt: both seats' passes draw one pose a frame (%d of %d), it animates (%d changes), no pose "
                   "flicker in either seat (%d)", t.animSame, t.animRead, t.animChanged, t.animFlicker));
            Cbuf_AddText("vr_test_kill 1\n");
            animGo(Step::AnimDead);
            return;
        case Step::AnimDead:
            if(!newFrameDrawn() || (t.animRead == 0 && t.probe[0].bones == 0))
            {
                return; // (its ragdoll drawn first)
            }
            readAnim();
            if(t.animRead < animFramesWanted)
            {
                return;
            }
            check(t.animBones == t.animRead && t.animFlicker == 0,
                va("its ragdoll: both seats' passes draw it, the same pose, each frame (%d of %d; one pose %d), no flicker (%d)",
                    t.animBones, t.animRead, t.animSame, t.animFlicker));
            for(int s = 1; s < t.seats; s++)
            {
                const Scope in(at(s));
                cl.viewangles[YAW] = t.yaw1 + 180.f; // (seat 2 looks away: seat 1 alone near it in sight)
            }
            animGo(Step::AnimOneNear);
            return;
        case Step::AnimOneNear:
            if(!newFrameDrawn())
            {
                return;
            }
            readAnim();
            if(t.animRead < animFramesWanted)
            {
                return;
            }
            check(t.animNear == t.animRead,
                va("seat 2 looking away: seat 1 draws the ragdoll each frame, never standing (%d of %d; seat 2 drew it %d)",
                    t.animNear, t.animRead, t.animFar));
            t.probeNum = 0;
            go(Step::WaitBooted);
            return;
        case Step::GunsSame:
        case Step::GunsApart:
        {
            if(t.frames < gunSettleFrames)
            {
                return;
            }
            if(t.frames == gunSettleFrames)
            {
                Cbuf_AddText("seat 1 +attack\n"); // (seat 1 fires its main hand's nail pistol; seat 2 never fires)
                (void)newFrameDrawn(); // (the frames read from the next drawn one)
                return;
            }
            if(!newFrameDrawn())
            {
                return;
            }
            readGuns();
            if(t.gunRead < gunFramesWanted)
            {
                return;
            }
            Cbuf_AddText("seat 1 -attack\n");
            if(t.step == Step::GunsSame)
            {
                checkGuns("same hand");
                // seat 2's nail pistol to its off hand (impulse 48: the hands' weapons swapped, QC VR_FlatSwapHands)
                Cbuf_AddText("seat 2 impulse 48\n");
                t.gunHand[1] = HAND_OFF;
                gunsGo(Step::GunsApart);
                return;
            }
            checkGuns("other hands");
            go(Step::WaitBooted);
            return;
        }
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
            for(int s = 0; s < t.seats; s++)
            {
                const Scope in(at(s));
                cl.forceunderwater = false;
            }
            go(Step::FlashBase);
            return;
        case Step::FlashBase:
            if(!drawnSince(drawsWanted))
            {
                return;
            }
            for(int s = 0; s < t.seats; s++)
            {
                memcpy(t.base[s], V_SeatViewOf(s)->probegrid, sizeof(t.base[s]));
                const Scope in(at(s));
                flashesOn();
                blood::onDamage(10, 30, nullptr); // (the pain's blood: a neutral hit)
            }
            go(Step::FlashOn);
            return;
        case Step::FlashOn:
            for(int s = 0; s < t.seats; s++)
            {
                const Scope in(at(s));
                flashesOn();
            }
            if(!drawnSince(drawsWanted))
            {
                return;
            }
            for(int s = 0; s < t.seats; s++)
            {
                const seatview_t* v = V_SeatViewOf(s);
                const float alike = pictureCorrelation(t.base[s], v->probegrid);
                check(v->blend[3] > 0.15f && alike > flashAlikeMin,
                    va("seat %d flashed (damage, burn, water, pain): its tile still its view (tint %.2f, alike %.2f)", s + 1,
                        static_cast<double>(v->blend[3]), static_cast<double>(alike)));
            }
            Cbuf_AddText("screenshot\n");
            go(Step::FlashShot);
            return;
        case Step::FlashShot:
            for(int s = 0; s < t.seats; s++)
            {
                const Scope in(at(s));
                flashesOn();
            }
            if(t.frames < 4)
            {
                return;
            }
            for(int s = 0; s < t.seats; s++)
            {
                const Scope in(at(s));
                flashesOff();
            }
            go(Step::FlashOff);
            return;
        case Step::FlashOff:
            if(!drawnSince(drawsWanted))
            {
                return;
            }
            for(int s = 0; s < t.seats; s++)
            {
                const seatview_t* v = V_SeatViewOf(s);
                const float diff = pictureDiff(t.base[s], v->probegrid);
                check(v->blend[3] == 0.f && diff < backDiffMax,
                    va("seat %d's flashes over: its tile back to its picture (tint %.2f, off by %.1f)", s + 1,
                        static_cast<double>(v->blend[3]), static_cast<double>(diff)));
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
    renderTest.anim = Cmd_Argc() > 2 && !q_strcasecmp(Cmd_Argv(2), "anim");
    renderTest.guns = Cmd_Argc() > 2 && !q_strcasecmp(Cmd_Argv(2), "guns");
    if(renderTest.seats < 2 || renderTest.seats > maxSeats)
    {
        Con_Printf("vr_seat_render_test <2-%d>\n", maxSeats);
        return;
    }
    if(renderTest.anim || renderTest.guns)
    {
        renderTest.frameRate = vr_fixed_frames_rate.value;
        Cvar_SetValue("vr_fixed_frames_rate", animFrameRate);
    }
    go(Step::WaitGame);
    Con_Printf("seat_render_test: started (%d seats)\n", renderTest.seats);
}

} // namespace

void registerRenderCommands()
{
    Cmd_AddCommand("vr_seat_render_test", renderTest_f);
}

void poseProbe(const entity_t* e, const int pose1, const int pose2, const float blend, const float matrix[16])
{
    RenderTest& t = renderTest;
    if(t.guns && t.step != Step::Off)
    {
        // split18: the current seat's held guns as its pass draws them
        const int hand = view::ownGunHand(e);
        if(hand >= 0)
        {
            RenderTest::GunProbe& g = t.gunProbe[CLAMP(0, CL_SeatIndex(), maxSeats - 1)][hand];
            za::U32 h = 2166136261u; // (FNV-1a over the frame lerp, quantized)
            const int parts[3] = {pose1, pose2, static_cast<int>(lroundf(blend * 1000.f))};
            for(const int v : parts)
            {
                h = (h ^ static_cast<za::U32>(v)) * 16777619u;
            }
            g.frame = host_framecount;
            g.hash = h;
            g.at = glm::vec3{matrix[12], matrix[13], matrix[14]};
            g.model = e->model;
        }
    }
    if(t.probeNum <= 0 || !cl_entities || t.probeNum >= cl.num_entities || e != &cl_entities[t.probeNum])
    {
        return;
    }
    RenderTest::Probe& p = t.probe[CLAMP(0, CL_SeatIndex(), maxSeats - 1)];
    if(p.frame == host_framecount)
    {
        return; // (its first draw in the seat's pass)
    }
    za::U32 h = 2166136261u; // (FNV-1a over the pose, quantized)
    const auto mix = [&h](const float v, const float scale) {
        h = (h ^ static_cast<za::U32>(static_cast<int>(lroundf(v * scale)))) * 16777619u;
    };
    mix(static_cast<float>(pose1), 1.f);
    mix(static_cast<float>(pose2), 1.f);
    mix(blend, 1000.f);
    for(int i = 0; i < 16; i++)
    {
        mix(matrix[i], 100.f);
    }
    const float* bones = nullptr;
    p.bones = VR_AliasBonePoses(e, &bones);
    for(int i = 0; bones && i < p.bones * 12; i++)
    {
        mix(bones[i], 100.f);
    }
    p.frame = host_framecount;
    p.hash = h;
}

void renderTestFrame()
{
    if(renderTest.step != Step::Off)
    {
        testFrame();
    }
}

} // namespace qvr::seat
