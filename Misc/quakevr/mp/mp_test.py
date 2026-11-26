"""mp_test.py -- the multiplayer end-to-end tests (mp_harness.py; docs/vr-port/MP_PLAN.md, "Test harness").

    python mp_test.py --base <game folder> [--exe <ironwail.exe>] [--out <folder>] [--listen] [--flat A|B]
                      [--lag <ms>] [--jitter <ms>] [--loss <0..1>] <test>

Tests:
    coop     a server on e1m1 (coop 1) and two clients A and B: both join; each sees the other's player entity; A walks
             (the mock's stick) and B sees him start moving within the bound, then the same with B walking; the server
             changes level to e1m2: both are taken along and see each other again; everything quits cleanly.
             --lag/--jitter/--loss put client B behind a proxy (mp_proxy.py); the bounds grow by the lag it adds.
    bandwidth  both idle on e1m1: B's link down under 25 KB/s (armed humanoids' poses only of those in sight), with
             vr_net_cullposes 0 measured too; no packet down over 1408 B (reliable chunks at most 1200 B).
    retransmit  (B12b) the reliable window's resends, B 100 ms away: a one-chunk message's lost chunk resent at its
             timeout (twice the measured round trip, under 0.75 s; NetQuake's: 1 s), a five-chunk one's lost first chunk
             at the later ones' acks (under 1.5 round trips); both arrive. (B13) A two-chunk one's first chunk overtaken
             by the second, whose ack reaches the server after the first's: the message and the next one arrive
             (NetQuake's receiver dropped the overtaking chunk but acked it: the window stalled).
    armed    (B12) six armed humanoids ahead of B in vrtesthall, idle and in combat: B's link down by kind (vr_net_bytes)
             with their poses outside his snapshot (vr_net_snapposes 0) and in it, under the budgets; under vr_net_rate
             6000: his link within it, a snapshot every datagram, the nearest poses more often than the farthest.
    dedicated  (B17) a dedicated server's status line (vr_sv_status) at the start, as A comes and goes and every 2 s;
             status's pings; pausable 0. (B18) its server.cfg (in a base folder of the run's own) read after
             autoexec.cfg, the command line's +commands over it.
    fuzz     (B18) B's packets up corrupted (the proxy's bit flips, cuts, random bytes): the server runs on, A plays
             on; then the server's packets down to B: B's game runs on (left with an error or playing).
    jitter   (B20) B 150 ms behind each way give or take 30 (reordered) falls onto e1m1's rising lift: none of his moves
             lost (the server reads his overtaken datagrams), the move buffer deepened with his jitter, the lift's
             predicted place under a unit. soak --soak-minutes N: the soak kept going N minutes, sampled each minute
             (memory, edicts, pace, B's link and moves); QVR_MP_LOW_IMPACT=1: idle priority.
    version a client of another Quake VR network version (vr_net_test_version 1) is rejected with the reason; one of the
             same version joins.
    campfollow (mpcampaign) a joining client plays the server's campaign (QVR_SVC_CAMPAIGN): rogue's client on a Quake
             start, Quake's on a Scourge of Armagon start (in place), Quoth's (left, switched, joined again), its own
             campaign back after each leave; a client lacking the campaign's data refused with what is missing.
    avatar  B has A's avatar (QVR_SVC_AVATAR): head and drawn hands within 2 units of the server's, the main hand's
             angles within 3 degrees, his weapon; none of himself; A closes his off hand: its finger curls at B within a
             step of the server's, closed; B draws A's jointed hands (and his weapon's model) where A drew them; the
             bytes it costs and the avatars the server sent and skipped as unchanged.
    avatarcal  each client's avatar calibration (height, arm length, gloves, body...) reaches the other and the server.
    ropeclimb  (ropes4) another player's climbing pose on a rope: flat A walks into one and climbs it, B sees his hands
             on it and his feet round it, hanging still and swinging, then off it
    ropes    climbable ropes (QVR_SVC_CLIMBROPE): a rope and a chain the server hangs reach B with the server's points;
             pushed, B sees them swing; at rest again, B's copies within a unit of the server's.
    body     B draws A's body (avatar::poseOther: the code that poses A's own): A's build's model, its wrists at A's drawn
             hands, its joints (head, pelvis, shoulders, wrists) within 1.5 units of A's own body and its scale A's; a
             screenshot pair (B facing A); A's Height raised: B's copy grows by as much (its scale and shoulders), as A's own.
    prefs    per-player preferences (vr_clientprefs.cpp): A on the Sticky weapon grip, B on Hold (the server's own):
             each given a shotgun with his grips let go, A keeps it and B's falls; then the other way round.
    grab     the rig up: the server has each player's fists as his client poses them; B's shotgun falls from his
             open hand and he closes it on it: taken, judged by his own fist on the dedicated server.
    guard    (review 2026-11-18 finding 1) a remote player's own values held to a body's bounds (vr_moveguard.hpp): A,
             tall, with a rocket launcher up, out and across at arm's length, is never held; B's lying moves
             (vr_mp_test_cheat: hands and muzzle across the room, 500 m/s, a 20000 u/s room-scale walk, a second's
             rewind) and a 500-unit fist sphere are held and counted (vr_mp_status mp_guard), his walk cut to a head's (and in his
             replay as on the server: review B6).
    cmdguard (review 2026-11-18 finding 9) client string commands: the test shot and punch refused by a server without
             vr_mp_watch (the harness's), taken with it; each client's commands under a budget: both players' own
             (spawn's vr_rig chunks...) far under it, B's flood of 150 in a frame cut to it and the rest dropped.
    weaponwheel  the headset's weapon wheel: B holds his B, his right hand onto his shoulder holster's slot, lets go: the
             server and A have the shotgun in his right hand (QC VR_Wheel_Pick through "vr_wheel_pick").
    teleport client-side traces: B (a remote client) traces his own map down to his floor; he aims a teleport with his off
             hand (his own trace: valid) and lets go: the server moves him there, A sees him there.
    held     the held weapon's drawn pose up: B holds a shotgun still; the server places its hotspots where B's view
             draws them; B's off hand takes its grip hotspot: the server's two-handed hold of it is B's own.
    rejoin   (B15) in and out of a coop game in progress: C joins after the world changed and has B's world (each
             entity at rest, the precaches, statics, light styles); B leaves and joins again: his slot and things
             (vr_mp_rejoin_keep), a suicide gives his level-start ones; B's game killed comes back on another port:
             his ghost closed at once, his slot and things; the server quits: C is told, still running.
    rejoin_keep  (review finding 8) the leavers' table: a game under a leaver's name with another vr_mp_clientkey is
             a new player; a leaver away across a level change respawns with what he carried into it; the 17th leaver's
             record takes the oldest's place; the leaver path in a level loaded from a coop save.
    download a dedicated server on a Map Library package's map: A (it installed) mounts it and joins; B (lacking it) is
             told with vr_mp_download 0, and with 1 installs it through the map installer (a local index and mirror)
             and joins.
    pickup   a weapon dropped by one player and picked up by the other, each way: A's shotgun falls from his opened
             hand, B (put beside it) closes his hand on it; then B lets it fall and A takes it.
    attach   (attach5) the bayonet synced: a listen host A, B (flat) and C; B's bayonet on his shotgun as A and C draw it
             held, holstered, stuck in a grunt by his V thrust (hurt), dropped; C picks it up (still on), throws it;
             then deathmatch: B's thrust hurts A (vr_attach_report; the server's vr_dm_test 25/26).
    forcegrab  force grab of a weapon of the level in coop: a super shotgun pickup ahead (vr_test_spawn 116); B's force
             grab (point, trigger, flick, grip) brings it to his hand and the pickup stays for the others; then A's.
    rules    server rules (B6): B joins with his own balance settings and plays by the server's (his rows and the
             Gameplay Presets' buttons locked, his config keeps his own); the server changes two: B follows; B leaves:
             his own back. A preference and a sound on a rule page stay his.
    cosmetics  B's skin tone, gloves and nails on his hands and body as A draws them are B's own; A's copy of B's body
             at B's height; B's holstered shotgun where B drew it (and the server's holsterpos for him), his pauldrons,
             his seated body, his nailgun's magazine in the server's hull of it (B6b); B's armour's plates (yellow,
             their pieces) on A's copy of his body, fewer as his armour wears; A's view of B (B6c).
    flatbody  a flat-screen B's body as A draws it is at his flat body height (B6b).
    disarm   (B10) A holds a shotgun out; B's hand closes on it and pulls gently: it slips off; B yanks: it is his. B tosses
             it to A, who takes it. B's holsters as A draws them (a shotgun in each slot, one drawn, after his death). A
             shell, a nail magazine and a rocket from A's pouch let fall: B's hand takes each up and lets it go at his
             pouch (his reserve up by A's loss); a nail magazine and a rocket B loads into his own gun (B8h). A
             holding a shotgun out leaves: B's grip on it in his body's hand takes no hold (review finding 7). --part
             give|use: the rounds alone.
    predictclimb  (B8h, B8i, B8j) on vrclimb, B 150 ms behind each way hangs from the ledge, pulls himself up and lets
             go, then mantles onto it: the hang's steps, the mantle's test and the mantle run in his moves, his replay's
             corrections under a unit (the hold's take and the mantle's start too); at rest he is where the server has
             him; a hold taken in the air (B8j part 2); the hands' climb out of the start map's pool (B8j part 3); the
             grapple's reel and hang from vrclimb's ceiling (B8j part 4). --part hang|mantle|airgrab|waterclimb|grapple:
             one alone.
    predictslide (B14) on e1m1, B 150 ms behind each way and 5% lost slides (vr_slide: the turning stick pulled
             down) running north: the slide in his moves, his corrections under a unit; twice, the second ended by a
             jump; at rest where the server has him.
    voice    (B10v) A's voice (a tone in place of his microphone) heard at B: in order through the jitter buffer,
             the tone kept, the delay under 250 ms (+ the added lag); nearer louder, the radio full; his volume,
             muted (the server told: none of his frames sent B; a rename keeps it); push-to-talk released, voice activation; the
             server's voice budget and B's rate cap hold his voice bytes; RNNoise on; A and a third player C talking at
             once, both heard at B (a fair part each under a small budget); no microphone. --lag/--jitter/--loss: B's
             link; --listen: A hosts.
    dm       (B11) a dedicated deathmatch server on dm4 (fraglimit 3): A and B join at deathmatch starts; two FrikBot bots
             added (impulse 100): four players listed at both, the bots move; A killed by a bot (vr_hurt) and respawned by his trigger, then A frags the bots (vr_hurt) to the fraglimit; the fraglimit ends the match: the intermission at both, their scoreboards the same.
    holstersync  (holstersync) A (VR) and B (flat) with holster settings of their own, each held before the other
             (vr_dm_test 23): each draws the other's holsters where the other's own client has them (within 7 cm), not
             where his own sit; a bot's wear the shipped settings (A's own only once A is back at the defaults).
    variantowned  (variantowned) a dedicated coop server, A (VR) and B (flat) owning the Crystal Lance with empty hands
             (vr_ad_test 233-234): B's weapon key 8 gives the Lance, not a plain lightning gun; a coop respawn keeps it owned.
    botrevolver  (vrnotes1122a) a dedicated deathmatch server: A adds a bot; the server's vr_dm_test 14: the bot draws
             and fires a revolver of its own at A (slugs, not trigger pulls), and one it took loaded (his, by a
             disarm) in either hand, without shells of its own.
    deathlook  (vrnotes1122a) a VR listen host and a flat client B in yellow armour: B killed whole, his body in yellow
             plates at both; B respawned in red armour: his old body still yellow, the same pieces (its own state, sent
             for its entity); the ragdoll body (vr_death_view 1), then Quake's body queue (vr_death_view 0).
    coopbots (MP bots phase 1) a dedicated coop server on e1m1: A adds a bot (impulse 100); the server's QC test brings a
             grunt in front of it: it kills it, never taking a player for its enemy; A through e1m1's exit: on e1m2 the
             bot is back with what it had (its super nailgun).
    wristscore  (2026-11-21) the wrist gadget's multiplayer scoreboard page: A alone on e1m1 has none; on a deathmatch
             server A and B join, A adds a bot and kills it: both pages list the three with their scoreboard's frags,
             in its order, each marked as himself; A's page shown, its image dumped (A_wristscore.png).
    lockstep (B14b) two runs of the same play on a lockstep server (vr_mp_lockstep: one move of each client a world
             step, by number; vr_fixed_frames 1, sv_random_seed 1; its clients vr_mp_lockstep_ahead 4): A and B join
             e1m1, the world held at steps 36, 360 and 720 (vr_mp_lockstep_until) for the inputs (B walks north, A's
             impulse 241 makes a rottweiler, A strafes) and the hashes: vr_physics_hash, vr_bench_statehash (its fields
             named when they differ), vr_nav_hash the same in both runs; B's prediction misses the same.
             --tracelog N: both sides' traces of B's move N (vr_mp_predict_tracelog). (B14e) Every move each player
             sent the same in both runs (the server's vr_mp_lockstep_movelog), every edict field compared.
    lockstep_throw (B14e) the same on e1m1 with props thrown: B's shotgun from his main hand, A's axe from his
             off hand, given into a gripping hand and swung up hard as it lets go; held at 36, 72, 144, 216, 432.
    vanish   (B16) a vanished server at its client: B's link down cut, "not responding" after 3 s, the link back; A,
             the listen host, untouched; A's game killed: B told, then out at his vr_mp_client_timeout to the server
             browser with the reason.
    emptyflood  (review 2026-11-18, finding 5) empty UDP datagrams (EMPTY_FLOOD_RATE a second, from a stranger's
             socket) at the server's one socket while A and B play: each still sees the other walk off within the bound,
             nobody leaves (an empty datagram's read of 0 used to end the drain of every client's datagrams).
    spoof    (review 2026-11-18, finding 2) forgeries in a player's name from his address (B's proxy): connect requests
             from his own address and port (his live connection kept: closed only once silent 3 s), unreliable
             datagrams far ahead of his sequences each way (outside the window: dropped, both still play), reliable
             chunks down at a blind guess of the sequences (0..1023: never taken; the channels start at random, network
             version 39); a stranger's player info has no ports, this machine's (127.0.0.1) has them.
    backoff  (review 2026-11-18, finding 12) B's acks cut for BACKOFF_WATCH_S (a peer not reading): the server's
             timed resends of its reliable message space out past 1 s to 4 s (not the window every second for 300 s),
             and the message after it arrives once the acks come back; the server's vr_net_rate_max over B's
             vr_net_rate (his 0: the server's; under it: his own; the server's 0: no ceiling).
    lookup   (review 2026-11-18, finding 6) host names looked up on a worker: the room service's name looked up for 3 s
             (vr_net_test_lookup_delay) while the dedicated server answers its console within 0.5 s, then registered with;
             every lookup failing (vr_net_test_lookup_fail): one "not found" line, the tries 2 s, 4 s, 8 s apart (net_lookups),
             none while a remote client is in, one again once he left; a favourite by name (localhost) in the
             server browser is asked once looked up.
    navfollow  (2026-11-20, not in the suite) vr_nav_follow: B (a remote client, flat) follows A along the server's
             navmesh by his own client's moves on e1m1, down the step and through the first room's door; A walks on, B
             follows and arrives within reach; B's own move hands control back, the follow walks on after
             vr_nav_follow_idle; vr_nav_follow 0 stops it. navfollow_listen: the same with the listen host A following B.
    botnavdebug (flakehunt, 2026-11-21, not in the suite) the bots' navmesh paths in vr_nav_debug on a client: a
             deathmatch server on e1m1, A (remote; botnavdebug_listen: the host) with vr_nav_debug 2 adds a bot; the
             server's vr_dm_test 19 has it chase A by the navmesh: A's client has its path and draws it; off drops it.
    roomnonce  (review 2026-11-18, finding 11) the room service's replies to a host checked (a stand-in service):
             REFUSED, BADVERSION and REGISTERED with another nonce change nothing; COOKIE (protocol version 2) with
             his nonce has him register again at once with it, then REGISTERED gives his code; a REFUSED with his
             nonce is taken.
    suite    coop, coop with B at 150 ms each way and 2% loss, bandwidth, armed, retransmit, version, avatar, avatarcal, body, prefs, grab, guard,
             teleport, held, ground, campaign, campaign_listen, campfollow, flow, rejoin, rejoin_keep, download, pickup, forcegrab, melee, moves, climb,
             rules, cosmetics, flatbody, loss, predict, predictclimb, lagcomp, cmdguard, disarm, voice, voice with B 50 ms (30 ms
             jitter, 5% loss) away, dm, lockstep, lockstep_throw, nat (B23: room codes through simulated routers).
             The network layer's review checks (2026-11-18): emptyflood, spoof, backoff, lookup, roomnonce.
             (B25) --only a,b runs just those; --suite-repeat N runs them N times over; --load N (any test) keeps N
             busy CPU workers at normal priority running meanwhile (mp_load.py: the machine busy with other work, as
             in the suite's real runs; --load-duty the share of time each spins).

The base folder is a Quake folder with id1 (and hipnotic, rogue, quakevr: -game as the kit's runs). Its
id1/autoexec.cfg runs in every game: --reset-autoexec empties it first (the kit's test bases: a run's script).
The last line is the verdict, "mp_<test>: N passed, M failed".
"""

import argparse
import math
import os
import random
import re
import sys
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from mp_harness import GAMES, LINKS, Instance, Session, StepFailed, fields, format_kinds, kind_rates  # noqa: E402
from mp_harness import busy_ports, default_base, default_base_port, free_base_port  # noqa: E402
from mp_nat import Nat  # noqa: E402
from mp_load import Load  # noqa: E402
import subprocess  # noqa: E402

REPO = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
EXE = os.path.join(REPO, "Windows", "VisualStudio", "Build-ironwail", "bin", "x64", "Release", "ironwail.exe")

BASE_MS = 300  # the bound on seeing another player move off, without added lag (two 72 Hz ticks, a lerp, frames)


def walk(s, mover, watcher, mover_ent, label, bound_ms, self_bound_ms):
    """`mover` walks forward for a moment (the mock's stick, or +forward when flat); `watcher` must see his entity
    move off from rest within bound_ms of the mover's mark. The mover's own view of himself moving off (no client
    prediction yet: a round trip) is a metric, bounded by self_bound_ms."""
    rest = r"mp_(move|pos): .*ent=%d self=0" % mover_ent
    watcher.quiet(rest, 0.5, 20, "%s at rest" % mover.name)
    since_w, since_m = watcher.mark(), mover.mark()
    go = "+forward" if mover.flat else "vr_mock_stick off 0 1"
    stop = "-forward" if mover.flat else "vr_mock_stick off 0 0"
    mover.cmd("vr_mp_mark %s; %s" % (label, go))
    _, mark = mover.wait(r"mp_mark: .* %s " % label, 10, since_m)
    t0 = int(fields(mark)["t"])
    _, seen = watcher.wait(r"mp_move: .*ent=%d self=0" % mover_ent, 10, since_w, "%s moving" % mover.name)
    _, own = mover.wait(r"mp_move: .*self=1", 10, since_m, "%s's own move" % mover.name)
    dt = int(fields(seen)["t"]) - t0
    dself = int(fields(own)["t"]) - t0
    s.metric("%s_seen_ms" % label, dt)
    s.metric("%s_self_ms" % label, dself)
    s.check("%s: %s sees %s move within %d ms" % (label, watcher.name, mover.name, bound_ms), 0 <= dt <= bound_ms,
            "%d ms" % dt)
    s.check("%s: %s sees himself move within %d ms (no prediction: a round trip)" % (label, mover.name, self_bound_ms),
            0 <= dself <= self_bound_ms, "%d ms" % dself)
    mover.cmd(stop)
    watcher.wait(r"mp_stop: .*ent=%d self=0" % mover_ent, 10, since_w, "%s stopping" % mover.name)


def join(s, listen=False, flat="", lag=0.0, jitter=0.0, loss=0.0, level="e1m1", first=None, srv_first="", b_first="",
         a_second=False):
    """A server on `level` (coop 1) and clients A and B joined, each seeing the other -> (server, a, b, ea, eb).
    `first(srv, a, ea)`: A joins alone first and this runs before B connects (a map with one start: A moved off it, or
    B's spawn telefrags him). `srv_first`, `b_first`: console commands the server runs before its map, B before he
    connects. `a_second` (a dedicated server): B joins first, A after him (A not entity 1: no rule of entity 1's)."""
    if listen:
        a = srv = s.server(dedicated=False, flat="A" in flat, name="A")
    else:
        srv = s.server(dedicated=True)
    srv.start()
    # (a listen host: maxplayers and listen before the map, its start-up map (vrstart, a server of 1) shut down first;
    # a dedicated server has its -dedicated 4 slots)
    srv.cmd("%s%scoop 1; deathmatch 0; skill 1; map %s; vr_mp_status" % (
        "disconnect; maxplayers 4; listen 1; " if listen else "", srv_first + "; " if srv_first else "", level))
    srv.wait(r"mp_server: map=%s" % level, 120, what="the server on %s" % level)
    if not listen:
        a = s.client("A", flat="A" in flat).start()
    # (B always through a proxy: with no lag it only counts B's traffic)
    b = s.client("B", flat="B" in flat, lag_ms=lag, jitter_ms=jitter, loss=loss, proxy=True).start()
    if not listen and not a_second:
        a.cmd(s.connect_cmd(a))
    if first:
        _, ja = a.wait(r"mp_event: .*joined map=%s" % level, 120, what="A joined %s" % level)
        first(srv, a, int(fields(ja)["self"]))
    if b_first:
        b.cmd(b_first)
    b.cmd(s.connect_cmd(b))
    if a_second and not listen:
        b.wait(r"mp_event: .*joined map=%s" % level, 120, what="B joined %s" % level)
        a.cmd(s.connect_cmd(a))

    _, ja = a.wait(r"mp_event: .*joined map=%s" % level, 120, what="A joined %s" % level)
    _, jb = b.wait(r"mp_event: .*joined map=%s" % level, 120, what="B joined %s" % level)
    ea, eb = int(fields(ja)["self"]), int(fields(jb)["self"])
    s.check("A and B joined coop %s" % level, ea != eb, "A is entity %d, B %d" % (ea, eb))

    _, sa = a.wait(r"mp_seen: .*ent=%d self=0" % eb, 20, what="A seeing B")
    _, sb = b.wait(r"mp_seen: .*ent=%d self=0" % ea, 20, what="B seeing A")
    s.check("each sees the other's player entity", True, "A sees B at %s; B sees A at %s" % (
        sa.split("org=")[1], sb.split("org=")[1]))
    return srv, a, b, ea, eb


def coop(s, listen, flat, lag, jitter, loss):
    srv, a, b, ea, eb = join(s, listen, flat, lag, jitter, loss)

    proxy = getattr(b, "proxy", None)
    before = proxy.snapshot() if proxy else None
    walk(s, a, b, ea, "a_walks", BASE_MS + lag + jitter, BASE_MS + 200)
    walk(s, b, a, eb, "b_walks", BASE_MS + lag + jitter, BASE_MS + 2 * (lag + jitter) + 200)
    if proxy:
        # B's traffic in play (both in the map, walking and standing): the bandwidth baseline (MP_PLAN.md, B7).
        w = proxy.window(before, proxy.snapshot())
        s.metric("b_link_in_play", w)
        s.note("B's link in play: up %(pkts_s)s pkts/s, %(bytes_s)s B/s, mean %(mean_B)s B" % w["up"] +
               "; down %(pkts_s)s pkts/s, %(bytes_s)s B/s, mean %(mean_B)s B" % w["down"])
    # The server's view of each client's datagrams (entities in sight and sent, bytes, frames full).
    since = srv.mark()
    srv.cmd("vr_net_stats")
    srv.wait(r"vr_net_stats: \d+ entities", 10, since)
    srv.quiet(r"^  client", 0.3, 10)
    for line in srv.lines[since:]:
        if line.startswith("  client") or line.startswith("vr_net_stats"):
            s.note("server: " + line.strip())

    since_a, since_b = a.mark(), b.mark()
    srv.cmd("changelevel e1m2; vr_mp_status")
    a.wait(r"mp_event: .*joined map=e1m2", 120, since_a, "A on e1m2")
    b.wait(r"mp_event: .*joined map=e1m2", 120, since_b, "B on e1m2")
    s.check("changelevel e1m2 takes both along", True)
    a.wait(r"mp_seen: .*self=0", 20, since_a, "A seeing B on e1m2")
    b.wait(r"mp_seen: .*self=0", 20, since_b, "B seeing A on e1m2")
    s.check("each sees the other on e1m2", True)


IDLE_BUDGET = 10 * 1024  # B's link down, both players idle on e1m1 (MP_PLAN.md, section 2.3: B7b's acknowledged snapshots)
IDLE_PACKETS = 360       # the window: five seconds of 72 Hz datagrams


def snap_status(srv, eb):
    """The server's snapshots to client entity `eb` (vr_mp_status's "mp_netsnap:" fields; {} none)."""
    since = srv.mark()
    srv.cmd("vr_mp_status")
    srv.wait(r"mp_server: map=", 10, since)
    sn = [fields(l) for l in srv.lines[since:] if l.startswith("mp_netsnap: slot=%d " % (eb - 1))]
    return sn[-1] if sn else {}


def client_snap_status(c):
    """A client's snapshots read and its interpolation buffer (vr_mp_status's "mp_snapcl:" fields; {} none)."""
    since = c.mark()
    c.cmd("vr_mp_status")
    c.wait(r"mp_snapcl: ", 10, since)
    sn =[fields(l) for l in c.lines[since:] if l.startswith("mp_snapcl: ")]
    return sn[-1] if sn else {}
RELIABLE_CHUNK = 1200    # a reliable message's chunk at most (net_dgrm.c DGRM_RELIABLE_CHUNK)
NET_HEADER = 8


def bandwidth(s):
    """Both players idle on e1m1: B's link down under the budget (the armed humanoids' poses only of those in his
    sight); every packet down at most an unreliable datagram's MTU (1400) and every reliable chunk at most 1200 B."""
    srv, a, b, ea, eb = join(s)
    proxy = b.proxy
    b.quiet(r"mp_(move|pos): ", 0.5, 20, "both at rest")
    windows = {}
    for cull in (1, 0):
        since = srv.mark()
        srv.cmd("vr_net_cullposes %d; echo mp_cull %d" % (cull, cull))
        srv.wait(r"mp_cull %d" % cull, 10, since)
        start = proxy.wait_packets("down", 72, 20)  # (a second for the change to take)
        end = proxy.wait_packets("down", IDLE_PACKETS, 60) if start else None
        if not end:
            raise StepFailed("B's link carried fewer than %d packets down in 60 s" % IDLE_PACKETS)
        windows[cull] = w = proxy.window(start, end)
        s.metric("b_idle_cull%d" % cull, w)
        s.note("B idle, vr_net_cullposes %d: down %s pkts/s, %s B/s, mean %s B; up %s B/s, mean %s B" % (
            cull, w["down"]["pkts_s"], w["down"]["bytes_s"], w["down"]["mean_B"], w["up"]["bytes_s"],
            w["up"]["mean_B"]))
    down = windows[1]["down"]["bytes_s"]
    up = windows[1]["up"]["bytes_s"]
    s.check("B's link up idle on e1m1 under %d B/s (quantized delta moves, B7)" % UP_BUDGET, up < UP_BUDGET,
            "%d B/s, mean %d B a packet (was 45 KB/s, 640 B)" % (up, windows[1]["up"]["mean_B"]))
    s.check("B's link down idle on e1m1 under %d B/s" % IDLE_BUDGET, down < IDLE_BUDGET,
            "%d B/s, mean %d B a packet (%d B/s with every pose to every client; was 20 KB/s before B7b)" % (
                down, windows[1]["down"]["mean_B"], windows[0]["down"]["bytes_s"]))
    # (B7b: B acknowledges the server's snapshots in his moves; the server codes his against the one he acknowledged)
    sn = snap_status(srv, eb)
    s.metric("b_netsnap", sn)
    s.check("the server delta-codes B's snapshots against the ones he acknowledged",
            bool(sn) and int(sn["delta"]) > 0 and int(sn["delta"]) > 10 * int(sn["full"]), "%s" % sn)
    most = proxy.stats["down"][3]
    s.check("every packet down at most %d B (no reliable chunk over %d B)" % (1400 + NET_HEADER, RELIABLE_CHUNK),
            most <= 1400 + NET_HEADER, "largest %d B" % most)
    srv.cmd("vr_net_cullposes 1")


HALL_START_Y, HALL_Z = -288.0, 24.0  # vrtesthall's start (facing +y): its hall ahead
# Six armed grunts in a line ahead of A in the hall (vr_physics_spawn monster_vr_grunt: its own weapon roll), each `dist`
# along his facing (+x or +y from his spot, the hall either way); B beside him: about 110 to 410 units from B.
ARMED_SPAWNS = (110, 170, 230, 290, 350, 410)
ARMED_B = (-200.0, HALL_START_Y, HALL_Z)
# B's link down with the six in sight (B12's measurement, their poses outside his snapshot -> in it): idle 47.3 -> 6.3
# KB/s, in combat 56.3 -> 31.9 KB/s (MP_PLAN.md, section 2.3: 6-10 KB/s idle, 25-35 KB/s in combat)
ARMED_IDLE_BUDGET = 10 * 1024
ARMED_COMBAT_BUDGET = 35 * 1024
ARMED_RATE = 12000       # B's vr_net_rate in combat, B/s: well under what the six cost him (about 32 KB/s)
ARMED_ENT_MEAN = 9.0     # B12b: an entity's changed update by its fields, B (head included; byte by byte it was ~10.5); B12c: 9 (8 before: a cosmetic fragment that waited for its budget changed over more frames)
ARMED_COSMETIC = 128     # B12c: vr_net_cosmetic_bytes' default: the cosmetic fragments' bytes in a datagram at most
RATE_SLACK = 1.15        # the link down under his rate within this (a datagram's burst, the reliable)


def net_window(s, srv, b, label, packets=IDLE_PACKETS):
    """B's link down over `packets` datagrams, with what he read by kind over it (vr_net_bytes reset .. poses) ->
    (proxy window, kinds in B/s, [(dist, entries) of each armed humanoid's pose], the server's mp_netsnap for B, the
    snapshot's entity entries by op [(count, bytes) gone, whole, byte by byte, by fields]: B12b's)."""
    proxy = b.proxy
    since = b.mark()
    b.cmd("vr_net_bytes reset")
    srv.cmd("vr_net_classes reset")  # (B12c: the server's entity entries by classname over the window)
    b.wait(r"mp_netbytes: ", 10, since)
    start = proxy.wait_packets("down", 1, 20)
    end = proxy.wait_packets("down", packets, 60) if start else None
    if not end:
        # (B12c: why, when it is known: B dropped (the server's game ended: a map command, as the gib storm's press of
        # vrtesthall's NEW GAME button did) or his link stalled)
        left = [l.strip() for l in b.lines[since:] if l.startswith("mp_event:") and l.rstrip().endswith(" left")]
        spawned = [l.strip() for l in srv.lines if l.startswith("SpawnServer:")]
        raise StepFailed("%s: B's link carried fewer than %d packets down in 60 s%s" % (
            label, packets, " (B left the game: %s; the server's map loads: %d)" % (left[0], len(spawned)) if left else ""))
    w = proxy.window(start, end)
    ssince = srv.mark()
    srv.cmd("vr_net_classes")
    since = b.mark()
    b.cmd("vr_net_bytes poses")
    _, line = b.wait(r"mp_netbytes: ", 10, since)
    b.wait(r"mp_netprio: done", 10, since)
    prio = [(float(fields(l)["dist"]), int(fields(l)["entries"]), int(fields(l)["ent"]), float(fields(l)["weight"]))
            for l in b.lines[since:] if l.startswith("mp_netprio: ent=")]
    kinds = kind_rates(fields(line), w["down"]["bytes_s"])
    ops = [tuple(int(x) for x in p.split("/")) for p in fields(line).get("ent_ops", "0/0,0/0,0/0,0/0").split(",")]
    s.metric("armed_%s" % label, {"link": w, "kinds": kinds, "prio": prio, "ent_ops": ops})
    s.note("%s: down %d B/s (mean %d B, %s pkts/s); by kind: %s; poses (dist, entries): %s" % (
        label, w["down"]["bytes_s"], w["down"]["mean_B"], w["down"]["pkts_s"], format_kinds(kinds),
        " ".join("%.0f:%d" % p[:2] for p in sorted(prio))))
    s.note("%s: the snapshot's entity entries (count/bytes): gone %d/%d, whole %d/%d, byte by byte %d/%d, by fields %d/%d"
           % ((label,) + tuple(v for op in ops for v in op)))
    srv.wait(r"mp_netclass: done", 10, ssince)
    classes = [fields(l) for l in srv.lines[ssince:] if l.startswith("mp_netclass: class=")]
    s.note("%s: the server's entity entries by class (to every remote client; bytes/entries/whole/things): %s" % (
        label, ", ".join("%s %s/%s/%s/%s" % (c["class"], c["bytes"], c["entries"], c["whole"], c["ents"]) for c in classes)))
    return w, kinds, sorted(prio), snap_status(srv, b.ent), ops


RETRANSMIT_LAG = 100     # ms each way: B's link in mp_retransmit (a round trip of 0.2 s)
RETRANSMIT_TIMEOUT = 0.75 # s: a lost lone chunk resent within (twice the round trip and 50 ms, 0.45-0.5 s; NetQuake's: 1 s)
RETRANSMIT_FAST = 1.5     # round trips: a lost chunk with later ones in flight resent within (at their acks)
REORDER_SIZE = 2000       # B (B13): two chunks, the first overtaken by the second
REORDER_EXTRA = 0.06      # s: the first chunk held back (the second's ack twice as long)
REORDER_MOST = 3.0        # s: the message (then the next) read within (NetQuake's window: never)


def wait_or_none(inst, pattern, timeout, after):
    """inst.wait's line, or None when it times out (a check's own failure, the scenario going on)."""
    try:
        return inst.wait(pattern, timeout, after)[1]
    except StepFailed:
        return None


def dm_frag_gained(inst, slots, after, timeout):
    """mp_dm: the first mp_frags line from `after` on where one of `slots` has more frags than its line before (a frag
    scored: a bot's own suicides may have put its count below 0), or None after `timeout` seconds."""
    rx = re.compile(r'mp_frags: .*slot=(\d+) .*frags=(-?\d+)')
    deadline = time.monotonic() + timeout * inst.s.time_scale
    while True:
        inst.read()
        last = {}
        for i, line in enumerate(inst.lines):
            m = rx.search(line)
            if not m or int(m.group(1)) not in slots:
                continue
            slot, frags = int(m.group(1)), int(m.group(2))
            if i >= after and slot in last and frags > last[slot]:
                return line
            last[slot] = frags
        if time.monotonic() > deadline:
            return None
        time.sleep(0.05)


def retransmit(s):
    """(B12b) The reliable window's resends, B 100 ms away each way: a one-chunk reliable message down (300 B) with its
    chunk dropped is resent at its timeout (twice the round trip measured and a little, under 0.75 s; NetQuake's was a
    second); a five-chunk one (5000 B) with its first chunk dropped is resent at the acks of the later ones (under 1.5
    round trips); both arrive whole."""
    lag = RETRANSMIT_LAG
    srv, a, b, ea, eb = join(s, lag=lag)
    rtt = 2 * lag / 1000.0
    proxy = b.proxy
    for _ in range(4):  # (a few reliable messages: the server measures the round trip)
        since = b.mark()
        srv.cmd("vr_net_reliable_test %d 100" % eb)
        b.wait(r"mp_reliable: 100 bytes", 10, since)
    for label, size, least, most in (("a lone chunk", 300, 0, RETRANSMIT_TIMEOUT),
                                     ("the first of five chunks", 5000, 1200, RETRANSMIT_FAST * rtt)):
        proxy.drop_reliable_down(1, least)
        since = b.mark()
        t0 = time.monotonic()
        srv.cmd("vr_net_reliable_test %d %d" % (eb, size))
        b.wait(r"mp_reliable: %d bytes" % size, 10, since, what="B's %d B reliable message" % size)
        took = time.monotonic() - t0
        after = list(proxy.resent_after.values())
        s.check("%s dropped (%d B message): resent within %.2f s, the message whole" % (label, size, most),
                len(after) == 1 and after[0] <= most,
                "resent %s s after the drop; the message read %.2f s after the command" % (
                    ", ".join("%.3f" % x for x in after) or "never", took))
    # (B13) the reordering that stalled NetQuake's window: a two-chunk message's first chunk overtaken by the second,
    # whose ack reaches the server after the first's (the receiver used to drop the second and ack it)
    proxy.reorder_reliable_down(1200, REORDER_EXTRA)
    since = b.mark()
    t0 = time.monotonic()
    srv.cmd("vr_net_reliable_test %d %d" % (eb, REORDER_SIZE))
    got = wait_or_none(b, r"mp_reliable: %d bytes" % REORDER_SIZE, REORDER_MOST, since)
    took = time.monotonic() - t0
    since = b.mark()
    srv.cmd("vr_net_reliable_test %d 100" % eb)
    after = wait_or_none(b, r"mp_reliable: 100 bytes", REORDER_MOST, since)
    s.check("a chunk overtaken, its ack after the first's (%d B message): the message whole within %.1f s, the next "
            "one too" % (REORDER_SIZE, REORDER_MOST),
            proxy.reordered["held"] and proxy.reordered["ack_late"] and got is not None and after is not None,
            "staged %s; the message %s; the next %s" % (proxy.reordered, "read after %.2f s" % took if got else "never read",
                                                       "read" if after else "never read"))
    since = srv.mark()
    srv.cmd("net_stats")
    _, line = srv.wait(r"windows resent: fast", 10, since)
    s.note("server: %s" % " ".join(l.strip() for l in srv.lines[since:] if l.startswith("windows resent")))


def alive(srv, ent):
    """(B12c) Whether entity `ent` on the server is a living thing now (the edict command's health over 0)."""
    since = srv.mark()
    srv.cmd("edict %d; echo mp_edict_done" % ent)
    n, _ = srv.wait(r"mp_edict_done", 10, since)
    for l in srv.lines[since:n]:
        f = l.split()
        if len(f) == 2 and f[0] == "health":
            try:
                return float(f[1]) > 0
            except ValueError:
                return False
    return False


def respawn_if_dead(srv, c, ent):
    """(mpready) Client `c` (entity `ent`) alive before a step that needs him: a bot's own shot may have killed him
    (mp_dm: A killed by a bot's nailgun after vr_dm_test 1, then "A's main grip on the server" timed out). His trigger
    brings him back (PlayerDeathThink: the buttons up, then one down). Whether he is alive at the end."""
    if alive(srv, ent):
        return True
    since = c.mark()
    for _ in range(30):
        c.cmd("vr_mock_button main trigger 1")
        got = wait_or_none(c, r"mp_event: .* respawned", 0.5, since)
        c.cmd("vr_mock_button main trigger 0")
        if got or wait_or_none(c, r"mp_event: .* respawned", 0.5, since):
            break
    return alive(srv, ent)


def armed(s):
    """(B12) Six armed humanoids ahead of B in vrtesthall: what his link carries down, by kind, with their poses outside
    his snapshot (vr_net_snapposes 0, as before) and in it; idle (both players notarget) and in combat (they shoot at
    him, both god); then under a low rate (vr_net_rate): his link under it, his own state every frame, the nearest
    humanoids' poses more often than the farthest's, nothing undecodable."""
    srv, a, b, ea, eb = join(s, level="vrtesthall", srv_first="developer 1",
                             first=lambda srv, a, ea: put_at(srv, a, ea, (-260.0, HALL_START_Y - 60.0, HALL_Z), 90))
    b.ent = eb
    a.cmd("god; notarget")
    b.cmd("god; notarget")
    put_at(srv, b, eb, ARMED_B, 90)
    for dist in ARMED_SPAWNS:
        # (ahead of the first player, A: an armed grunt rolling its own weapon; vr_physics_spawn, the server's console)
        since = srv.mark()
        srv.cmd("vr_physics_spawn monster_vr_grunt %d 0" % dist)
        _, made = srv.wait(r"vr_physics_spawn: \d+ monster_vr_grunt at", 20, since, "an armed grunt %d ahead" % dist)
        s.note("spawned: " + made.split("vr_physics_spawn: ")[1].strip())
    # (their poses reach B: the bytes of them he read)
    seen = 0
    for _ in range(40):
        since = b.mark()
        b.cmd("vr_net_bytes")
        _, line = b.wait(r"mp_netbytes: ", 10, since)
        f = fields(line)
        seen = int(f["pose"]) + int(f["snap_pose"])
        if seen > 500:
            break
    s.check("the armed humanoids' poses reach B", seen > 500, "%d B of them read" % seen)
    out = {}
    for snapposes in (0, 1):
        since = srv.mark()
        srv.cmd("vr_net_snapposes %d; echo mp_snapposes %d" % (snapposes, snapposes))
        srv.wait(r"mp_snapposes %d" % snapposes, 10, since)
        out[("idle", snapposes)] = net_window(s, srv, b, "idle_snapposes%d" % snapposes)

    # In combat: notarget off (a toggle), they turn on the players.
    a.cmd("notarget")
    b.cmd("notarget")
    for snapposes in (0, 1):
        since = srv.mark()
        srv.cmd("vr_net_snapposes %d; echo mp_snapposes %d" % (snapposes, snapposes))
        srv.wait(r"mp_snapposes %d" % snapposes, 10, since)
        out[("combat", snapposes)] = net_window(s, srv, b, "combat_snapposes%d" % snapposes)
    # Under a low rate, still in combat (each pose changes every frame: the nearest should come most often).
    sn0 = snap_status(srv, eb)
    b.cmd("vr_net_rate %d" % ARMED_RATE)
    for _ in range(60):  # (his preference up: the server's report of his rate)
        if int(snap_status(srv, eb).get("rate", 0)) == ARMED_RATE:
            break
    since = srv.mark()
    srv.cmd("vr_net_posewaits %d" % (eb - 1))  # (B19: the server's waits of B's pose records, from here)
    srv.wait(r"mp_posewait: done", 10, since)
    w, kinds, prio, sn, _ = net_window(s, srv, b, "rate%d" % ARMED_RATE)
    since = srv.mark()
    srv.cmd("vr_net_posewaits")
    srv.wait(r"mp_posewait: done", 10, since)
    waits = [(int(fields(l)["ent"]), int(fields(l)["sent"]), float(fields(l)["wait"]), float(fields(l)["weight"]))
             for l in srv.lines[since:] if l.startswith("mp_posewait: ent=")]
    b.cmd("vr_net_rate 0")
    down = w["down"]["bytes_s"]
    s.check("under vr_net_rate %d: the server holds him to it" % ARMED_RATE, sn and int(sn["rate"]) == ARMED_RATE and
            int(sn["waited"]) > int(sn0.get("waited", 0)), "%s" % sn)
    s.check("under vr_net_rate %d: B's link down within %g of it" % (ARMED_RATE, RATE_SLACK),
            down <= ARMED_RATE * RATE_SLACK, "%d B/s, mean %d B" % (down, w["down"]["mean_B"]))
    # (B16) the datagrams down in the window (the proxy's count of unreliable packets: B13d's flake counted the
    # reliable chunks too, a busy fight's many, as datagrams without a snapshot)
    sent = int(sn["sent"]) - int(sn0["sent"]) if sn and sn0 else 0
    grams = w["down"]["datagrams"]
    s.check("under it: a snapshot (his own state) still in every datagram", grams > 0 and sent >= grams,
            "%d snapshots sent from before the window to after it; %d datagrams in it (of %d packets)" % (
                sent, grams, w["down"]["pkts"]))
    # (B12c) only the humanoids alive at the window's end (alive through it: they don't come back): one killed in it
    # sent poses only until it fell (a near one killed early read as the rate's fault: 5 and 10 entries against 60)
    every = prio
    # (B16) ordered by their weights for B as he read them (vr_netsnap.cpp weightOf: 256 / (256 + distance), twice
    # within 60 degrees of his view): B13d's flake ordered them by distance alone, and in the fight a near one beside
    # or behind him (weight 0.64 at 145 units) rightly came less often than a far one ahead (0.96 at 279)
    prio = sorted((p for p in prio if alive(srv, p[2])), key=lambda p: -p[3])
    near = [p[1] for p in prio[:2]]
    far = [p[1] for p in prio[-2:]]
    # (B12b) by how much depends on how far apart they stood. (B19) And on how tight the rate is in the window: a
    # record's priority is its weight times one plus the frames it waited, so it waits whole frames; when the fight's
    # other updates leave room for most poses every frame or two, every pose comes every frame or two whatever its
    # weight (the share flattens towards 1), and only a tight budget gives shares near the weights' ratio. The fight's
    # traffic is real time (the grunts' shots and moves): B12b's "half way to the weights' ratio" failed 1 run in 4 at
    # 1.10-1.36 against 1.16-1.49 wanted. Nor are the entries B read a measure of it: a pose goes only when it changed,
    # and a humanoid idle in the window changes less (a suite run: weights 0.517 and 0.498, 58 and 132 entries). The
    # check is the server's own (vr_net_posewaits): the frames each changed pose waited before it went, by its weight
    # then: the highest two weights' waits no longer than the lowest two's, shorter once the lowest wait at all. The
    # client's share a metric.
    ratio = (sum(p[3] for p in prio[:2]) / max(1e-6, sum(p[3] for p in prio[-2:]))) if len(prio) >= 4 else 1.0
    share = sum(near) / max(1, sum(far))
    live = sorted((p for p in waits if p[1] >= 10 and alive(srv, p[0])), key=lambda p: -p[3])
    hi = sum(p[2] for p in live[:2]) / 2 if len(live) >= 4 else 0.0
    lo = sum(p[2] for p in live[-2:]) / 2 if len(live) >= 4 else 0.0
    wratio = (sum(p[3] for p in live[:2]) / max(1e-6, sum(p[3] for p in live[-2:]))) if len(live) >= 4 else 1.0
    s.metric("armed_rate_share", {"share": round(share, 3), "weights": round(ratio, 3), "wait_hi": round(hi, 3),
                                  "wait_lo": round(lo, 3), "wait_weights": round(wratio, 3)})
    if len(live) < 4:
        # (the fight killed too many in the window to compare two and two: noted, not failed)
        s.note("under it: the humanoids' pose waits not compared: %d alive with 10 poses sent at the end of %s" % (
            len(live), waits))
    s.check("under it: the humanoids weighted most (near, ahead) wait less for their poses to go than the least (the "
            "server's waits; weights within 1.15 of each other: not told apart)",
            len(live) < 4 or wratio < 1.15 or (hi <= lo and (lo < 1.05 or hi < lo)),
            "highest two weights' mean wait %.3f, lowest two's %.3f (weights %.2f; ent, sent, wait, weight of %s); the "
            "entries B read: highest two %s, lowest two %s" % (hi, lo, wratio, live, near, far))
    cs = client_snap_status(b)
    s.check("under it: none undecodable", cs and int(cs["undecodable"]) == 0, "%s" % cs)

    # (B12b) the moving things' updates by their fields: most changed entries so, each a few bytes (byte by byte: ~10.5)
    # (B24: the encoder sends each update the smaller way, so the share by fields follows the fight -- 80-97% in the
    # suite's runs, once 79.9% with the mean at 6.5 B -- and the bytes are what count: the mean over all of them under
    # ARMED_ENT_MEAN, the share only "most": over half)
    ops = out[("combat", 1)][4]
    changed = ops[2][0] + ops[3][0]
    mean = (ops[2][1] + ops[3][1]) / changed if changed else 0
    s.check("combat: the entities' changed updates go by their fields, %g B each at most" % ARMED_ENT_MEAN,
            changed > 0 and ops[3][0] > 0.5 * changed and mean <= ARMED_ENT_MEAN,
            "%d of %d by fields, %.1f B each (byte by byte %d/%d, by fields %d/%d)" % (
                ops[3][0], changed, mean, ops[2][0], ops[2][1], ops[3][0], ops[3][1]))

    # (B12c) the cosmetic fragments (small gibs, explosion chunks) within their own budget in each of B's datagrams
    # (vr_net_cosmetic_bytes), over the combat window with the poses in his snapshot
    s0, s1 = out[("combat", 0)][3], out[("combat", 1)][3]
    if s0 and s1 and "cosmetic" in s1:
        snaps = max(1, int(s1["sent"]) - int(s0["sent"]))
        per = (int(s1["cosmetic"]) - int(s0["cosmetic"])) / float(snaps)
        s.check("combat: the cosmetic fragments' entries within their budget, %d B a datagram" % ARMED_COSMETIC,
                per <= ARMED_COSMETIC, "%.1f B a datagram over %d snapshots, %d of their records waited" % (
                    per, snaps, int(s1["cwaited"]) - int(s0["cwaited"])))
    for mode, budget in (("idle", ARMED_IDLE_BUDGET), ("combat", ARMED_COMBAT_BUDGET)):
        before, kb = out[(mode, 0)][0]["down"]["bytes_s"], out[(mode, 0)][1]
        after, ka = out[(mode, 1)][0]["down"]["bytes_s"], out[(mode, 1)][1]
        # (outside it: at most the datagrams already on their way when the server switched)
        s.check("%s: the poses go in B's snapshot (none outside it)" % mode,
                ka.get("snap_pose", 0) > 0 and ka.get("pose", 0) <= 0.02 * ka.get("snap_pose", 0),
                "in it %d B/s, outside %d B/s (before: outside %d B/s)" % (ka.get("snap_pose", 0), ka.get("pose", 0),
                                                                          kb.get("pose", 0)))
        s.check("%s: B's link down with six armed humanoids in sight under %d B/s" % (mode, budget),
                budget <= 0 or after < budget, "%d B/s (their poses outside his snapshot: %d B/s)" % (after, before))
    cs = client_snap_status(b)
    s.check("B read every snapshot (none undecodable)", cs and int(cs["undecodable"]) == 0, "%s" % cs)


def version(s):
    """A client of another Quake VR network version (vr_net_test_version) is rejected with the reason, and a right one
    still joins after it."""
    srv = s.server(dedicated=True)
    srv.start()
    srv.cmd("coop 1; deathmatch 0; skill 1; map e1m1; vr_mp_status")
    srv.wait(r"mp_server: map=e1m1", 120, what="the server on e1m1")
    c = s.client("C", extra=["+vr_net_test_version", "1"]).start()
    c.cmd(s.connect_cmd(c))
    _, why = c.wait(r"network version \d+, your game 1", 60, what="C's reject")
    s.check("a client of another network version is told why", True, why.strip())
    _, logged = srv.wait(r"Rejected a connection", 10, what="the server's note of it")
    s.check("the server notes the rejected client", True, logged.strip())
    a = s.client("A").start()
    a.cmd(s.connect_cmd(a))
    a.wait(r"mp_event: .*joined map=e1m1", 120, what="A joined e1m1")
    s.check("a client of the same network version joins", True)


def campfollow(s):
    """(mpcampaign) A joining client plays the server's campaign (QVR_SVC_CAMPAIGN, before each svc_serverinfo), and its
    own comes back once it left. A dedicated coop server, A a fresh game (Dissolution's campaign, rogue: the -game
    list's last, as a player who last chose it):
    1. the server on Quake's start: A joins it (in place: Quake's start.bsp, not rogue's "Model *43 not found"), his
       Load Autosave list this campaign's (the server's coop saves listed); after leaving, rogue again;
    2. A on Quake's (his own start map) joins the server on Scourge of Armagon's start: hipnotic's start.bsp;
    3. the server on Quoth's start (a campaign with folders of its own): A leaves, switches and joins again; after
       leaving, Quake's again (his own);
    4. C, whose game lacks Quoth (vr_mp_test_campaign_missing quoth), is refused, told what is missing; no Host_Error."""
    srv = s.server(dedicated=True)
    srv.start()

    def serve(cmd, label):
        since = srv.mark()
        srv.cmd("coop 1; deathmatch 0; skill 1; %s; vr_mp_status" % cmd)
        srv.wait(r"mp_server: map=start", 180, since, label)

    def campaign_of(g):
        since = g.mark()
        g.cmd("vr_campaign_probe maps/start.bsp")
        _, line = g.wait(r"VR campaign source: maps/start\.bsp -> .*; campaign \w+", 30, since, "%s's probe" % g.name)
        return re.search(r"campaign (\w+)\s*$", line).group(1), line.strip()

    def join_server(g, label):
        since = g.mark()
        g.cmd(s.connect_cmd(g))
        g.wait(r"mp_event: .*joined map=start", 180, since, label)
        return since

    def leave(g):
        since = g.mark()
        g.cmd("disconnect")
        got = wait_quietly(g, r"mp_campaign: left the server: .* is this game's campaign again", 60, since,
                           "%s's own campaign back" % g.name)
        return got[1].strip() if got else None

    # 1. rogue's client, Quake's server on start
    serve("vr_campaign_select id1", "the server on Quake's start")
    a = s.client("A").start()
    first, _ = campaign_of(a)
    s.check("A starts in Dissolution's campaign (rogue), as a player who last chose it", first == "rogue", first)
    since0 = join_server(a, "A on the server's Quake start")
    said = a.find(r"mp_campaign: the server plays Quake: this game's campaign while joined", since0)
    now, line = campaign_of(a)
    s.check("1. A (rogue) joins the server on Quake's start: Quake's campaign while joined, its start.bsp read",
            said is not None and now == "id1", "%s; %s" % (said and said[1].strip(), line))
    since = a.mark()
    a.cmd("vr_autosave_list cold")
    _, listed = a.wait(r"vr_autosave_list: \d+ autosaves.*this game: campaign \d+", 30, since, "A's Load Autosave list")
    s.check("1. A's Load Autosave list is the server's campaign's (this game: campaign 0)",
            "this game: campaign 0" in listed, listed.strip())
    back = leave(a)
    now, line = campaign_of(a)
    s.check("1. A leaves: Dissolution's campaign again", back is not None and now == "rogue", "%s; %s" % (back, line))

    # 2. A on Quake's own start map joins Scourge of Armagon's server
    since = a.mark()
    a.cmd("vr_campaign_select id1")
    a.wait(r"mp_event: .*joined map=start", 180, since, "A on his own Quake start")
    serve("vr_campaign_select hipnotic", "the server on Scourge of Armagon's start")
    since = join_server(a, "A on the server's Scourge of Armagon start")
    said = a.find(r"mp_campaign: the server plays Scourge of Armagon: this game's campaign while joined", since)
    now, line = campaign_of(a)
    s.check("2. A (Quake's) joins the server on Scourge of Armagon's start: hipnotic's start.bsp",
            said is not None and now == "hipnotic", "%s; %s" % (said and said[1].strip(), line))
    back = leave(a)
    now, _ = campaign_of(a)
    s.check("2. A leaves: Quake's campaign again", back is not None and now == "id1", "%s; %s" % (back, now))

    # 3. a campaign with folders of its own: leave, switch, join again
    serve("vr_campaign_native quoth", "the server on Quoth's start")
    since = join_server(a, "A on the server's Quoth start (joined again)")
    switched = a.find(r"mp_campaign: the server plays Quoth: switching this game to it, then joining again", since)
    again = a.find(r"mp_campaign: .* is this game's campaign now; joining again", since)
    now, line = campaign_of(a)
    s.check("3. A joins the server on Quoth: left, switched to it, joined again (Quoth's start.bsp)",
            switched is not None and again is not None and now == "quoth",
            "%s; %s; %s" % (switched and switched[1].strip(), again and again[1].strip(), line))
    back = leave(a)
    now, _ = campaign_of(a)
    s.check("3. A leaves: Quake's campaign again (the folders rebuilt back)", back is not None and now == "id1",
            "%s; %s" % (back, now))
    s.check("A never hit a Host_Error", a.find(r"Host_Error", since0) is None,
            (a.find(r"Host_Error", since0) or (0, ""))[1].strip())

    # 4. a game lacking the server's campaign is refused, told why
    c = s.client("C", extra=["+vr_mp_test_campaign_missing", "quoth"]).start()
    since = c.mark()
    c.cmd(s.connect_cmd(c))
    _, why = c.wait(r"mp_campaign: the server plays Quoth .*can't join: .*missing", 60, since, "C's refusal")
    s.check("4. C, lacking Quoth, is refused with what is missing", True, why.strip())
    s.check("4. C never joined the map and hit no Host_Error",
            c.find(r"mp_event: .*joined map=", since) is None and c.find(r"Host_Error", since) is None, "")


RENDEZVOUS = os.path.join(REPO, "Misc", "rendezvous")


def nat(s):
    """(B23) NAT traversal. The room service's own test (Misc/rendezvous/test_rendezvous.py: its protocol and abuse
    limits). Then the service run locally and games behind simulated routers (mp_nat.py, -natgateway):
    1. One server socket: a host behind a router that doesn't keep port numbers, his port forwarded to another
       outside, is joined by his outside address (NetQuake's socket per client named a port of his own inside the
       accept, which led nowhere outside).
    2. Room codes: a host behind a cone NAT (non-port-preserving) gets a room code; A behind another cone NAT joins
       by it (both punch); B behind a symmetric NAT is told there's no direct link; an unknown code and a service that
       doesn't answer are told so.
    3. (2026-11-21) The port check (the service's probe from another port): without the room service it reads unknown
       (1.'s host), behind the cone NAT without a forward not reachable (2.'s), with the port forwarded reachable;
       with a version 2 service (--newest 2) the room code still comes and the check reads unknown."""
    out = subprocess.run([sys.executable, os.path.join(RENDEZVOUS, "test_rendezvous.py")], capture_output=True,
                         text=True, timeout=120)
    verdict = [l for l in out.stdout.splitlines() if l.startswith("rendezvous_test:")]
    s.check("the room service's protocol and limits (test_rendezvous.py)", out.returncode == 0 and verdict,
            verdict[0] if verdict else out.stdout[-300:])
    rport = s.take_port()
    service = subprocess.Popen([sys.executable, os.path.join(RENDEZVOUS, "qvr_rendezvous.py"), "--bind", "127.0.0.1",
                                "--port", str(rport), "--report", "0", "--verbose"],
                               stdout=open(os.path.join(s.out, "rendezvous.log"), "w"), stderr=subprocess.STDOUT)
    services = [service]
    oport = s.take_port()
    nats = []
    try:
        def router(ip, kind, seed):
            n = Nat(ip, kind, seed=seed).start()
            nats.append(n)
            s.note(n.describe())
            return n
        room = ["+vr_mp_room", "1", "+vr_mp_rendezvous", "127.0.0.1:%d" % rport]
        # 1. A forwarded host behind a non-preserving router, joined by his outside address
        fwd = router("127.0.0.2", "cone", 1)
        fport = s.take_port()
        outside = fwd.forward(fport, s.port)
        srv = s.server(dedicated=True, extra=["-natgateway", str(fwd.gateway_port)])
        srv.start()
        srv.cmd("coop 1; deathmatch 0; skill 1; map e1m1; vr_mp_status")
        srv.wait(r"mp_server: map=e1m1", 120, what="the forwarded server on e1m1")
        c = s.client("C").start()
        c.cmd("name C; connect 127.0.0.2:%d" % outside)
        c.wait(r"mp_event: .*joined map=e1m1", 120, what="C joined the forwarded host")
        s.check("a host behind a router that doesn't keep port numbers, his port %d forwarded to %d outside, is joined "
                "at 127.0.0.2:%d (one server socket)" % (s.port, outside, outside), True)
        since = srv.mark()
        srv.cmd("vr_mp_roomcode")
        _, line = srv.wait(r"Port \d+: ", 30, since, "the port check's line")
        s.check("the port check without the room service (vr_mp_room 0): unknown (room service off)",
                "Port %d: unknown (room service off)" % s.port in line, line.strip())
        c.stop()
        srv.stop()
        # 2. Room codes through cone NATs
        hnat = router("127.0.0.3", "cone", 2)
        h = Instance(s, "host", s.take_port(), dedicated=4, extra=["-natgateway", str(hnat.gateway_port)] + room)
        s.instances.append(h)
        h.start()
        h.cmd("coop 1; deathmatch 0; skill 1; map e1m1; vr_mp_status")
        h.wait(r"mp_server: map=e1m1", 120, what="the room's host on e1m1")
        _, line = h.wait(r"Room code: [A-Z]{6} ", 30, what="the host's room code")
        code = re.search(r"Room code: ([A-Z]{6})", line).group(1)
        outside_h = hnat.outside_port(h.port)
        s.check("the host behind a cone NAT gets a room code, the service seeing his router's outside port",
                outside_h and outside_h != h.port and ("127.0.0.3:%d" % outside_h) in line, line.strip())
        # (the port check, version 3) the service's probe from its other port, filtered by his router: not reachable
        _, line = h.wait(r"Port \d+ is (not )?reachable", 30, what="the host's port check")
        since = h.mark()
        h.cmd("vr_mp_roomcode")
        _, shown = h.wait(r"Port \d+: ", 30, since, "the port check's line")
        s.check("the port check behind a cone NAT without a forward: not reachable (the probe filtered)",
                ("Port %d is not reachable" % h.port) in line and
                ("Port %d: not reachable from the internet (forward UDP %d" % (h.port, h.port)) in shown,
                "%s | %s" % (line.strip(), shown.strip()))
        anat = router("127.0.0.4", "cone", 3)
        a = s.client("A", extra=["-natgateway", str(anat.gateway_port)] + room).start()
        a.cmd("name A; connect @%s" % code.lower())
        a.wait(r"mp_event: .*joined map=e1m1", 120, what="A joined by the room code")
        s.check("A behind another cone NAT joins by the room code (hole punching: %d of the routers' datagrams in "
                "filtered)" % (hnat.stats["filtered"] + anat.stats["filtered"]), True)
        bnat = router("127.0.0.5", "symmetric", 4)
        b = s.client("B", extra=["-natgateway", str(bnat.gateway_port)] + room).start()
        t0 = time.monotonic()
        b.cmd("name B; connect @%s" % code)
        _, why = b.wait(r"doesn't allow a direct link", 60, what="B's no-direct-link reason")
        s.check("B behind a symmetric NAT is told there is no direct link, within %.0f s" % (time.monotonic() - t0),
                time.monotonic() - t0 < 30, why.strip())
        since = b.mark()
        b.cmd("connect @%s" % ("ZZZZZZ" if code != "ZZZZZZ" else "YYYYYY"))
        _, why = b.wait(r"No room [A-Z]{6} at", 30, since, "B's unknown-code reason")
        s.check("an unknown room code is told so", True, why.strip())
        since = b.mark()
        b.cmd("vr_mp_rendezvous 127.0.0.1:%d; connect @%s" % (s.take_port(), code))
        _, why = b.wait(r"does not answer", 30, since, "B's no-service reason")
        s.check("a room service that doesn't answer is told so", True, why.strip())
        a.cmd("vr_mp_roomcode")
        # 3. (the port check) a host whose port is forwarded (the same number outside) reads reachable
        fnat = router("127.0.0.6", "cone", 5)
        fhport = s.take_port()
        forwarded = fnat.forward(fhport, fhport)
        f = Instance(s, "fwdhost", fhport, dedicated=4, extra=["-natgateway", str(fnat.gateway_port)] + room)
        s.instances.append(f)
        f.start()
        f.cmd("coop 1; deathmatch 0; skill 1; map e1m1")
        _, line = f.wait(r"Port \d+ is (not )?reachable", 120, what="the forwarded host's port check")
        s.check("the port check with UDP %d forwarded: reachable (the probe got in)" % fhport,
                forwarded == fhport and ("Port %d is reachable" % fhport) in line, line.strip())
        # ... and with a version 2 service (not yet redeployed): unknown, its room code still given
        old = subprocess.Popen([sys.executable, os.path.join(RENDEZVOUS, "qvr_rendezvous.py"), "--bind", "127.0.0.1",
                                "--port", str(oport), "--report", "0", "--newest", "2"],
                               stdout=open(os.path.join(s.out, "rendezvous_v2.log"), "w"), stderr=subprocess.STDOUT)
        services.append(old)
        since = f.mark()
        f.cmd("vr_mp_rendezvous 127.0.0.1:%d" % oport)
        _, line = f.wait(r"Room code: [A-Z]{6} ", 30, since, "the room code from the old service")
        _, line2 = f.wait(r"Port \d+: unknown \(the room service .* is too old", 30, since, "the old service's check")
        s.check("a version 2 room service still gives a room code; the port check reads unknown (too old)",
                "Room code:" in line, "%s | %s" % (line.strip(), line2.strip()))
        s.metric("nat", {n.outside_ip: dict(n.stats) for n in nats})
    finally:
        for n in nats:
            s.note(n.summary())
            n.stop()
        for p in services:
            p.terminate()
            p.wait(10)


def netsafe(s):
    """(B24) No test talks to the internet (mp_harness.py, "No test talks to the internet"). The harness's own rule: a
    game it would start, or a command it would send, pointed off this machine is refused before it happens. Then the
    engine's fence (-loopbackonly, which every harness game has): a dedicated server hosting a room with its room
    service at a link-local address (169.254.0.1: never routed, so even a broken fence reaches no internet host) and
    then at a name under .invalid (never looked up) prints NET SAFETY refusals and sends nothing. (syncfix2) HTTP too:
    a client's start-up map index reads its cache only; maps_fetch and vr_update_check_now print their refusals."""
    from mp_harness import command_net_problem, launch_net_values, net_safety_problem
    unsafe = [
        ("a default game with vr_mp_room 1", net_safety_problem(*launch_net_values(["x", "+vr_mp_room", "1"]))),
        ("vr_mp_room 1 with a rendezvous by name", net_safety_problem("1", "example.invalid:26100")),
        ("a command setting vr_mp_room 1 on a default game",
         command_net_problem("map e1m1; vr_mp_room 1", {"room": "0", "rendezvous": "vittorioromeo.com:26100"})),
        ("a room-code join through the default service",
         command_net_problem("connect @ABCDEF", {"room": "0", "rendezvous": "vittorioromeo.com:26100"})),
        ("a connect to a name", command_net_problem("connect example.invalid:26000", {"room": "0", "rendezvous": ""})),
        ("a connect to a LAN address", command_net_problem("connect 192.168.1.2", {"room": "0", "rendezvous": ""})),
    ]
    safe = [
        ("the harness's games (vr_mp_room 0)", net_safety_problem(*launch_net_values(["x", "+vr_mp_room", "0"]))),
        ("vr_mp_room 1 with a local service", net_safety_problem("1", "127.0.0.1:27600")),
        ("a room-code join through a local service",
         command_net_problem("vr_mp_rendezvous 127.0.0.1:27601; connect @ABCDEF", {"room": "0",
                                                                                  "rendezvous": "vittorioromeo.com:26100"})),
        ("connects to 127.x and localhost", command_net_problem("connect 127.0.0.2:27500; connect localhost:27500",
                                                                {"room": "0", "rendezvous": ""})),
    ]
    s.check("the harness refuses every setting or command that points a game off this machine",
            all(p for _, p in unsafe), "%d of %d refused%s" % (
                sum(1 for _, p in unsafe if p), len(unsafe), "".join("; NOT: " + n for n, p in unsafe if not p)))
    s.check("and lets the loopback ones through", not any(p for _, p in safe),
            "; ".join("%s: %s" % (n, p) for n, p in safe if p))
    try:
        Instance(s, "unsafe", s.take_port(), dedicated=2, extra=["+vr_mp_room", "1"]).start()
        started = True
    except StepFailed:
        started = False
    s.check("a game the harness would start with vr_mp_room 1 and the default service is not started", not started)
    g = Instance(s, "fence", s.take_port(), dedicated=2, net_guard_test=True,
                 extra=["+vr_mp_room", "1", "+vr_mp_rendezvous", "169.254.0.1:26100"])
    s.instances.append(g)
    g.start()
    s.check("every harness game starts with -loopbackonly", "-loopbackonly" in g.args())
    g.cmd("coop 1; deathmatch 0; map e1m1")
    g.wait(r"NET: -loopbackonly", 60, what="its -loopbackonly line")
    _, line = g.wait(r"NET SAFETY: refused 169\.254\.0\.1", 120, what="the link-local service refused")
    s.check("-loopbackonly refuses a room service at a non-loopback address", True, line.strip())
    since = g.mark()
    g.cmd("vr_mp_rendezvous qvr-netsafe.invalid:26100")
    _, line = g.wait(r"NET SAFETY: refused qvr-netsafe\.invalid", 30, since, "the named service refused")
    s.check("-loopbackonly refuses a name without looking it up", True, line.strip())
    g.cmd("vr_mp_room 0")
    # (syncfix2) HTTP: a client's start-up map index reads its cache only (no fetch started), Slipseer's and the update
    # check's start-up passes never go out; asked for (maps_fetch, vr_update_check_now), each prints its refusal.
    c = Instance(s, "httpfence", s.take_port(), dedicated=0, net_guard_test=True, extra=["+vr_maps_fetch", "1"])
    s.instances.append(c)
    c.start()
    _, line = c.wait(r"map index: started \(", 120, what="the start-up map index pass")
    s.check("a client's start-up map index reads its cache only (no HTTP fetch)", "its cache only:" in line,
            line.strip())
    # (syncfix3: its end first -- a maps_fetch while it reads is "a fetch is already under way", no refusal: the
    # rerun-alone flake, the game started faster than in the suite)
    c.wait(r"map index: (\d+ packages, from the cache|none \()", 60, what="the start-up map index pass's end")
    since = c.mark()
    c.cmd("maps_fetch")
    _, line = c.wait(r"NET SAFETY: refused the map index fetch from https://", 30, since, "the map index fetch refused")
    s.check("-loopbackonly refuses an asked-for map index fetch", True, line.strip())
    _, line = c.wait(r"NET SAFETY: refused Slipseer's listing fetch", 30, since, "Slipseer's fetch refused")
    s.check("-loopbackonly refuses an asked-for Slipseer fetch", True, line.strip())
    c.cmd("vr_update_check_now")
    _, line = c.wait(r"NET SAFETY: refused the update check at https://", 30, since, "the update check refused")
    s.check("-loopbackonly refuses an asked-for update check", True, line.strip())
    c.read()
    went = [ln for ln in c.lines if "curl" in ln.lower() or re.search(r"\b[1-9]\d* fetched now", ln)]
    s.check("no HTTP transfer started (no curl error, nothing 'fetched now')", not went, "; ".join(went[:3]))


def roomui(s):
    """(B24, not in the suite) The room-code menus as a player sees them, flatscreen: a listen host (4 players) with the
    room service run here gets his code; his Host Game page shows it (host_roomcode.png); a player's Join Game page with
    that code typed in its box (joiner_roomcode.png); then the player joins by it."""
    rport = s.take_port()
    service = subprocess.Popen([sys.executable, os.path.join(RENDEZVOUS, "qvr_rendezvous.py"), "--bind", "127.0.0.1",
                                "--port", str(rport), "--report", "0"],
                               stdout=open(os.path.join(s.out, "rendezvous.log"), "w"), stderr=subprocess.STDOUT)
    try:
        room = ["+vr_mp_room", "1", "+vr_mp_rendezvous", "127.0.0.1:%d" % rport]
        h = s.server(dedicated=False, flat=True, name="host", extra=room)
        h.size = (960, 540)  # (the menus readable)
        h.start()
        h.cmd("disconnect; maxplayers 4; listen 1; coop 1; deathmatch 0; skill 1; map e1m1")
        _, line = h.wait(r"Room code: [A-Z]{6} ", 120, what="the host's room code")
        code = re.search(r"Room code: ([A-Z]{6})", line).group(1)
        s.check("the listen host gets a room code from the local service", True, line.strip())
        # (the port check) no router between: the service's probe gets in; the net HUD's host line says so
        h.wait(r"Port \d+ is (not )?reachable", 30, what="the host's port check")
        since = h.mark()
        h.cmd("vr_net_hud 1; vr_net_hud_report")
        _, rep = h.wait(r"mp_nethud: ", 30, since, "the net HUD's report")
        s.check("the listen host's net HUD line: his port reachable", 'host_line="port %d reachable"' % h.port in rep,
                rep.strip()[-60:])
        since = h.mark()
        h.cmd("con_notifytime 0; menu_hostgame; wait; wait; wait; echo roomui_menu_open")  # (drawn before the shot)
        h.wait(r"roomui_menu_open", 30, since, "the host's menu open")
        shot = h.screenshot("roomcode")
        s.note("Host Game page: %s" % shot)
        j = s.client("joiner", flat=True, extra=room)
        j.size = (960, 540)
        j.start()
        j.wait(r"UDP Initialized", 60, what="the joiner up")
        since = j.mark()
        j.cmd("con_notifytime 0; menu_joingame %s; wait; wait; wait; echo roomui_menu_open" % code)
        j.wait(r"roomui_menu_open", 30, since, "the joiner's menu open")
        shot = j.screenshot("roomcode")
        s.note("Join Game page: %s" % shot)
        j.cmd("togglemenu; name J; connect @%s" % code)
        j.wait(r"mp_event: .*joined map=e1m1", 120, what="the joiner in by the code")
        s.check("the player joins by the code typed on the Join Game page", True)
    finally:
        service.terminate()
        service.wait(10)


STATUS_EVERY_S = 2  # the test's vr_sv_status (the default: 60)


def dedicated(s):
    """(B17) A dedicated server's comforts: its console's status line (vr_sv_status: the map, the mode, the players
    with their pings, its uptime) at the map's start, when A joins and leaves, and every STATUS_EVERY_S seconds;
    `status` gives A's ping; no player can pause it (pausable 0 by default on a dedicated server). (B18) Its server.cfg
    read by itself, after autoexec.cfg and before the command line's +commands: one in a base folder of the run's own
    (a second -basedir: the shared base holds none; its id1, as quakevr is searched in one base folder only, the one
    with the game code), whose vr_sv_status the command line's overrides."""
    user = os.path.join(s.out, "server_user")
    os.makedirs(os.path.join(user, "id1"), exist_ok=True)
    with open(os.path.join(user, "id1", "server.cfg"), "w", newline="\n") as f:
        f.write("// mp_test.py dedicated (B18)\nhostname \"qvr servercfg test\"\nvr_sv_status 97\n"
                "echo mp_servercfg: read\n")
    srv = s.server(dedicated=True, extra=["-basedir", user, "+vr_sv_status", str(STATUS_EVERY_S)])
    srv.start()
    srv.wait(r"mp_servercfg: read", 60, what="the server's server.cfg read")
    execs = [l.strip() for l in srv.lines if l.startswith("execing ")]
    order = [l.split()[1] for l in execs]
    s.check("server.cfg exec'd by itself, after autoexec.cfg", "server.cfg" in order and "autoexec.cfg" in order and
            order.index("autoexec.cfg") < order.index("server.cfg"), "%s" % execs)
    since = srv.mark()
    srv.cmd("hostname; vr_sv_status")
    _, host = srv.wait(r'"hostname" is "', 10, since, "the hostname")
    _, every = srv.wait(r'"vr_sv_status" is "', 10, since, "vr_sv_status")
    s.check("server.cfg's settings hold (its hostname), the command line's +commands over them (vr_sv_status %d, "
            "not its 97)" % STATUS_EVERY_S, "qvr servercfg test" in host and '"%d"' % STATUS_EVERY_S in every.split(" is ")[1],
            "%s; %s" % (host.strip(), every.strip()))
    srv.cmd("coop 1; deathmatch 0; skill 1; map e1m1; vr_mp_status")
    srv.wait(r"mp_server: map=e1m1", 120, what="the server on e1m1")
    _, empty = srv.wait(r"status: e1m1 \(coop, skill 1\) \| 0 of 4 players \| map ", 30, what="the empty server's status line")
    s.check("the dedicated server's status line at the map's start (map, mode, players, times)", True, empty.strip())
    a = s.client("A").start()
    since = srv.mark()
    a.cmd("name A; " + s.connect_cmd(a))
    a.wait(r"mp_event: .*joined map=e1m1", 120, what="A joined e1m1")
    _, joined = srv.wait(r"status: .* 1 of 4 players: A \d+ ms", 30, since, "the status line with A in")
    s.check("a status line when A comes in, with his ping", True, joined.strip())
    # (the next ones come every STATUS_EVERY_S seconds of real time: two more, their ping measured by then)
    n, _ = srv.wait(r"status: .* 1 of 4 players: A \d+ ms", 30, srv.mark(), "the next status line")
    t0 = time.monotonic()
    srv.wait(r"status: .* 1 of 4 players: A \d+ ms", 30, n + 1, "the one after")
    gap = time.monotonic() - t0
    s.check("the status line again every %d s" % STATUS_EVERY_S, STATUS_EVERY_S * 0.5 <= gap <= STATUS_EVERY_S * 3,
            "%.2f s apart" % gap)
    since = srv.mark()
    srv.cmd("status")
    _, row = srv.wait(r"^#1\s+A\s+.* \d+ ms", 10, since, "status's row for A")
    s.check("status gives each player's ping", True, row.strip())
    since = a.mark()
    a.cmd("pause")
    _, refused = a.wait(r"Pause not allowed", 10, since, "A's pause refused")
    s.check("no player can pause a dedicated server (pausable 0)", True, refused.strip())
    since = srv.mark()
    a.cmd("disconnect")
    _, left = srv.wait(r"status: .* 0 of 4 players \|", 30, since, "the status line when A left")
    s.check("a status line when A leaves", True, left.strip())


AVATAR_UNITS = 2.0  # B's copy of A's head and hands against the server's: quantized to an eighth, a frame or so apart
AVATAR_DEGREES = 3.0


def avatar_fields(line):
    """An "mp_avatar:" / "mp_avatar_sv:" line -> {name: [floats]} (head, hand0, hand1, rot1, wpn, ent...)."""
    out = {}
    for m in re.finditer(r"(\w+)=([-\d. ]+?)(?= \w+=|$)", line.strip()):
        out[m.group(1)] = [float(x) for x in m.group(2).split()]
    return out


def angle_diff(a, b):
    return max(abs((x - y + 180.0) % 360.0 - 180.0) for x, y in zip(a, b))


CURL_STEP = 1.0 / 16.0  # a finger joint's curl as sent: sixteenths of a frame (vr_move.cpp writeDrawnHand)
CLOSED_CURL = 1.5       # A's off hand closed (the grip): its fingers' mean curl at B at least this (frames; open ~0)


def reports(s, insts):
    """vr_mp_avatars on each of `insts` at once -> per instance its lines from that report."""
    marks = [i.mark() for i in insts]
    for i in insts:
        i.cmd("vr_mp_avatars")
    out = []
    for i, since in zip(insts, marks):
        n, _ = i.wait(r"mp_avatars: done", 10, since, "%s's avatars" % i.name)
        out.append(i.lines[since:n + 1])
    return out


def by_ent(lines, prefix):
    return {int(avatar_fields(l)["ent"][0]): avatar_fields(l) for l in lines if l.startswith(prefix + ":")}


def drawn_at(lines, ent):
    """B's "mp_avatar_drawn:" lines for `ent` -> {(side, kind): (model, [x, y, z])}."""
    out = {}
    for l in lines:
        m = re.match(r"mp_avatar_drawn: ent=(\d+) side=(\d) kind=(\w+) model=(\S+) mirrored=\d at=([-\d. ]+)", l.strip())
        if m and int(m.group(1)) == ent:
            out[(int(m.group(2)), m.group(3))] = (m.group(4), [float(x) for x in m.group(5).split()])
    return out


def avatar(s):
    """B has A's avatar (QVR_SVC_AVATAR): his head and both drawn hands within 2 units of where the server has them from
    A's moves, the main hand's angles within 3 degrees, his weapon; B gets no avatar of himself. A closes his off hand:
    B gets its fingers' curls within a step of the server's, closed. B draws A's jointed hands and his weapon's model where
    A drew them (within 2 units of A's own). The bytes it costs, idle (an unchanged avatar every vr_net_avatar_keyframe
    frames) and the server's counts."""
    srv, a, b, ea, eb = join(s)
    b.quiet(r"mp_(move|pos): ", 0.5, 20, "both at rest")
    # A closes his off hand (the mock's grip): his fingers curl, eased in over vr_hand_fit_blend; B's copy is read until
    # they are closed (a definite end: at most 20 reports).
    # (and the shotgun, WID_SHOTGUN 4, into his main hand: the test give, impulse 150 + 4; held by its grip, or the hand
    # lets it fall at once: the default grip-to-hold)
    grip_then_give(srv, a, "vr_mock_button main grip 1; vr_mock_button off grip 1")
    srv.cmd("vr_debug_carry 0; developer 0")
    got = sv = own = None
    lines_b = []
    for _ in range(20):
        lines_s, lines_b, lines_a = reports(s, [srv, b, a])
        sv, got, own = by_ent(lines_s, "mp_avatar_sv"), by_ent(lines_b, "mp_avatar"), by_ent(lines_a, "mp_avatar_own")
        if ea in got and sum(got[ea]["curl0"][:15]) / 15.0 >= CLOSED_CURL and ea in sv and ea in own and \
                own[ea]["wmdl"][1] > 0 and got[ea]["wmdl"][1] > 0 and \
                max(abs(x - y) for k in ("curl0", "curl1") for x, y in zip(got[ea][k], sv[ea][k])) <= CURL_STEP + 1e-4 and \
                (1, "weapon") in drawn_at(lines_b, ea):
            # (B13c: and B's last drawn frame had it: the avatar with the weapon can arrive after the frame B's report
            # describes was drawn -- under load the suite's flake, "drawn: None" with the model indices equal)
            break
    s.check("B has A's avatar", ea in got, "B has %s; the server %s" % (sorted(got), sorted(sv)))
    s.check("B has no avatar of himself", eb not in got)
    if ea not in got or ea not in sv:
        return
    mine, theirs = got[ea], sv[ea]
    for l in lines_b:
        if l.startswith("mp_avatar"):
            s.note("B: " + l.strip())
    worst = max(max(abs(x - y) for x, y in zip(mine[k], theirs[k])) for k in ("head", "hand0", "hand1"))
    s.metric("avatar_worst_units", worst)
    s.check("A's head and drawn hands at B within %g units of the server's" % AVATAR_UNITS, worst <= AVATAR_UNITS,
            "worst %.2f; B: hand1 %s, server: hand1 %s" % (worst, mine["hand1"], theirs["hand1"]))
    turn = angle_diff(mine["rot1"], theirs["rot1"])
    s.check("A's main hand's angles at B within %g degrees" % AVATAR_DEGREES, turn <= AVATAR_DEGREES, "%.2f" % turn)
    s.check("A's weapon at B", mine["wpn"][0] == theirs["wpn"][0], "%d (server %d)" % (mine["wpn"][0], theirs["wpn"][0]))
    off = max(abs(x - y) for k in ("curl0", "curl1") for x, y in zip(mine[k], theirs[k]))
    s.metric("avatar_curl_worst", off)
    s.check("A's finger curls at B within a step (%g) of the server's" % CURL_STEP, off <= CURL_STEP + 1e-4,
            "worst %.4f; B: curl0 %s" % (off, mine["curl0"][:15]))
    closed = sum(mine["curl0"][:15]) / 15.0
    s.metric("avatar_offhand_closed_curl", closed)
    s.check("A's off hand closed at B (mean curl at least %g frames)" % CLOSED_CURL, closed >= CLOSED_CURL, "%.2f" % closed)

    # B draws them: the jointed hands and the main hand's weapon (its model the one A drew) where A drew them.
    mine_own = own.get(ea)
    d = drawn_at(lines_b, ea)
    s.check("B draws A's two jointed hands", (0, "hand") in d and (1, "hand") in d, "drawn: %s" % sorted(d))
    if mine_own and (1, "hand") in d:
        gap = max(abs(x - y) for x, y in zip(d[(1, "hand")][1], mine_own["hand1"]))
        s.metric("avatar_drawn_hand_gap", gap)
        s.check("A's main hand drawn at B within %g units of where A drew it" % AVATAR_UNITS, gap <= AVATAR_UNITS,
                "%.2f" % gap)
    has_weapon = mine_own is not None and int(mine_own["wmdl"][1]) > 0
    s.check("A's main hand holds the given shotgun (A's own drawn model, the server's weapon)",
            has_weapon and int(theirs["wpn"][0]) == 4,
            "A's model index %s, the server's weapon %d" % (mine_own.get("wmdl") if mine_own else None, theirs["wpn"][0]))
    if has_weapon:
        s.check("B draws A's weapon in his main hand", (1, "weapon") in d and int(mine["wmdl"][1]) == int(mine_own["wmdl"][1]),
                "drawn: %s; B's model index %d, A's %d" % (d.get((1, "weapon")), mine["wmdl"][1], mine_own["wmdl"][1]))
        if (1, "weapon") in d:
            gap = max(abs(x - y) for x, y in zip(d[(1, "weapon")][1], mine_own["weapon1"]))
            s.metric("avatar_drawn_weapon_gap", gap)
            s.check("A's weapon drawn at B within %g units of where A drew it" % AVATAR_UNITS, gap <= AVATAR_UNITS,
                    "%.2f (%s)" % (gap, d[(1, "weapon")][0]))
    for l in lines_s:
        if l.startswith("mp_avatar_sent:"):
            s.note("server: " + l.strip())
    if not has_weapon:
        since = srv.mark()
        srv.cmd("vr_dumpplayer %d; echo mp_dump_done" % ea)
        srv.wait(r"mp_dump_done", 10, since)
        for l in srv.lines[since:]:
            if "weapon" in l or "vr_dumpplayer" in l:
                s.note("server: " + l.strip())

    start = b.proxy.wait_packets("down", 72, 20)
    end = b.proxy.wait_packets("down", IDLE_PACKETS, 60) if start else None
    if end:
        w = b.proxy.window(start, end)["down"]
        s.metric("b_idle_down_with_avatar", w)
        s.note("B idle with A's avatar: down %s B/s, mean %s B" % (w["bytes_s"], w["mean_B"]))

    # The pair to look at: A's own view of his hands and shotgun, and B turned to face A (his hands and the shotgun as A
    # drew them, on his body), each saved into the session's folder.
    # B and A turn to face each other and B takes a step towards A (the stick, let go as he moves off); A
    # holds both hands up in front of him.
    b_at = by_ent(lines_a, "mp_avatar").get(eb)  # (B's head as A has it)
    if b_at:
        yaw_b = math.degrees(math.atan2(mine["head"][1] - b_at["head"][1], mine["head"][0] - b_at["head"][0]))
        since_a, since_b = a.mark(), b.mark()
        a.cmd("vr_mock_hand main 0.15 1.6 -0.45 0 0 0; vr_mock_hand off -0.15 1.6 -0.45 0 0 0; "
              "vr_mock_turn_to %.0f" % (yaw_b + 180.0))
        b.cmd("vr_mock_turn_to %.0f" % yaw_b)
        a.wait(r"vr_mock_turn_to: facing", 20, since_a, "A turned to face B")
        b.wait(r"vr_mock_turn_to: facing", 20, since_b, "B turned to face A")
        a.cmd("vr_mock_turn_to off")
        since_b = b.mark()
        b.cmd("vr_mock_turn_to off; vr_mock_stick off 0 1")
        b.wait(r"mp_move: .*self=1", 20, since_b, "B stepping towards A")
        b.cmd("vr_mock_stick off 0 0")
        b.wait(r"mp_stop: .*self=1", 20, since_b, "B stopping")
        reports(s, [b])  # (a frame or more after A's hands moved: B has them)
        s.note("screenshots: %s, %s" % (a.screenshot("own_hands"), b.screenshot("sees_A")))


def cal_of(lines, prefix):
    """"mp_avatar_cal[_own|_sv]:" lines -> {ent: {field: int}}."""
    out = {}
    for l in lines:
        if l.startswith(prefix + ":"):
            f = dict(m.groups() for m in re.finditer(r"(\w+)=(-?\d+)", l))
            out[int(f.pop("ent"))] = {k: int(v) for k, v in f.items()}
    return out


def reports_until(s, insts, done, what, tries=20):
    """vr_mp_avatars on `insts` until done(lines per instance) (a definite end: at most `tries` reports) -> (lines, n)."""
    for n in range(1, tries + 1):
        lines = reports(s, insts)
        if done(lines):
            return lines, n
    s.check(what, False, "not after %d reports" % tries)
    raise StepFailed(what)


def rope_reports(insts):
    """vr_rope_info on each of `insts` at once -> per instance its lines from that report."""
    marks = [i.mark() for i in insts]
    for i in insts:
        i.cmd("vr_rope_info")
    out = []
    for i, since in zip(insts, marks):
        n, _ = i.wait(r"rope_info: done", 10, since, "%s's ropes" % i.name)
        out.append(i.lines[since:n + 1])
    return out


def ropes_of(lines, prefix):
    """rope_sv / rope_cl lines -> {entity: (points, chain, last point)}."""
    out = {}
    for l in lines:
        if l.startswith(prefix + ":"):
            f = dict(kv.split("=", 1) for kv in l.split(":", 1)[1].split() if "=" in kv)
            out[int(f["ent"])] = (int(f["n"]), int(f["chain"]), tuple(float(x) for x in f["last"].split(",")))
    return out


def ropes(s):
    """Climbable ropes (vr_climbrope.cpp, QVR_SVC_CLIMBROPE; docs/vr-port/ROPES.md): the dedicated server hangs a rope
    and a chain ahead of A; B (through the proxy) has both, each with the server's points, style and ends (its bottom
    within a unit of the server's at rest); the server pushes them: B sees them swing (his copy's bottom moves over 20
    units); after the push B's copies are where the server's are (a unit; over the loopback, swinging)."""
    srv, a, b, ea, eb = join(s)
    srv.cmd("vr_rope_spawn 0 128; vr_rope_spawn 1 96")
    srv.wait(r"vr_rope_spawn: \d+ chain", 20, what="the server hung a rope and a chain")

    def matching(tol):
        def done(lines):
            sv, cl = ropes_of(lines[0], "rope_sv"), ropes_of(lines[1], "rope_cl")
            return len(sv) >= 2 and all(e in cl and cl[e][0] == v[0] and cl[e][1] == v[1] and
                                        math.dist(cl[e][2], v[2]) < tol for e, v in sv.items())
        return done

    def until(done, what, seconds=60):
        # (a definite end: the condition; the clock only a safety net)
        end = time.time() + seconds
        n = 0
        while time.time() < end:
            n += 1
            lines = rope_reports([srv, b])
            if done(lines):
                return lines, n
        s.check(what, False, "not after %d reports (%d s)" % (n, seconds))
        raise StepFailed(what)

    lines, n = until(matching(1.0), "B has the rope and the chain where the server has them")
    sv = ropes_of(lines[0], "rope_sv")
    for l in lines[1]:
        if l.startswith("rope_cl"):
            s.note("B: " + l.strip())
    s.check("B has the server's rope and chain (points, style, bottom within a unit)", True,
            "after %d reports: %s" % (n, sorted((e, v[0], v[1]) for e, v in sv.items())))
    rest = {e: ropes_of(lines[1], "rope_cl")[e][2] for e in sv}
    srv.cmd("vr_rope_push 300 0 0")

    def swinging(lines):
        cl = ropes_of(lines[1], "rope_cl")
        return all(e in cl and math.dist(cl[e][2], rest[e]) > 20.0 for e in rest)

    _, n = until(swinging, "B sees them swing after the server's push", 20)
    s.check("the server pushes them: B sees both swing (bottom over 20 units from rest)", True, "after %d reports" % n)
    _, n = until(matching(1.0), "after the push: B's copies where the server's are", 90)
    s.check("after the push: B's copies within a unit of the server's", True, "after %d reports" % n)


ROPE_CLIMB_AT = (200.0, -300.0, 24.0)    # mp_ropeclimb (vrropes): A's spot, facing north (the rope hung 96 ahead of him)
ROPE_WATCH_AT = (200.0, -520.0, 24.0)    # B's, behind him, facing him
ROPE_HANDS_NEAR, ROPE_FEET_NEAR = 10.0, 12.0  # A's drawn wrists, ankles from the rope as B has it (units), hanging still
ROPE_SWING_SLACK = 6.0                   # swinging: a frame's step more (B draws the rope's latest points, A lerped)
ROPE_PELVIS_MATCH = 4.0                  # hanging still: B's copy's pelvis off the rope within this of A's own (ropepose)


def ropeclimb(s):
    """Another player's climbing pose on a rope (ROPES.md "Multiplayer"; vr_climb.cpp flatRope, vr_avatar_climb.cpp
    ropeFoot), flat-screen A on vrropes, B (VR, through the proxy) watching: the server hangs a rope ahead of A; A walks
    into it with forward held (the flat rope's take), climbs looking up; B's copy of A: climbing (weight 0.9 or more), his
    wrists on the rope and his ankles round it as B has the rope (mp_ropepose); A pumps it level: B sees him swing (over
    24 units across) with the hands and feet still on it (a frame's step more); A jumps off: B's copy holds no more."""
    srv, a, b, ea, eb = join(s, flat="A", level="vrropes",
                             first=lambda srv, a, ea: put_at(srv, a, ea, ROPE_CLIMB_AT, 90))
    put_at(srv, b, eb, ROPE_WATCH_AT, 90)
    srv.cmd("vr_climb_stamina 0; vr_rope_spawn 0 480")
    srv.wait(r"vr_rope_spawn: \d+", 20, what="the server hung a rope ahead of A")

    def a_at():
        return org_of(status(srv), "mp_client", "slot", ea - 1)

    start = a_at()
    a.cmd("+forward; +lookup")
    risen = None
    for _ in range(120):
        at = a_at()
        if at and at[2] - start[2] >= 48:
            risen = at
            break
    a.cmd("-forward; -lookup")
    s.check("A walks into the rope and climbs it (the server has him from %s to %s)" % (start, risen), risen is not None,
            "the server's A at %s" % a_at())
    if not risen:
        raise StepFailed("A on the rope")

    # Hanging still (ropes4): the server's A at rest on it between two reads, the hand over hand climb over (his copy's
    # hands go from hold to hold eased, its legs wrap the rope over a third of a second: no pose to judge mid-climb).
    prev, still = risen, None
    for _ in range(60):
        at = a_at()
        if at and prev and max(abs(at[i] - prev[i]) for i in range(3)) < 1.0:
            still = at
            break
        prev = at
    s.check("A hangs still on the rope (the server has him at %s)" % (still,), still is not None, "the server's A at %s" % prev)

    def pose(lines):
        return by_ent(lines[0], "mp_ropepose").get(ea)

    def held_wrists(p):
        # (a hand that doesn't hold is -1: a flat player's weapon hand keeps its weapon, off the rope)
        return [p[k][0] for k in ("wrist0", "wrist1") if p[k][0] >= 0.0]

    def own(lines):
        # (ropepose) A's own body on the rope as A draws it: what B's copy should hang like
        return by_ent(lines[1], "mp_ropepose_own").get(ea)

    lines, n = reports_until(s, [b, a], lambda ls: (pose(ls) or {}).get("climb", [0.0])[0] >= 0.9, "B's copy of A climbing the rope")
    p = pose(lines)
    s.note("B: " + " ".join("%s=%.1f" % (k, v[0]) for k, v in sorted(p.items()) if k != "ent"))
    o = own(lines)
    if o:
        s.note("A's own: " + " ".join("%s=%.1f" % (k, v[0]) for k, v in sorted(o.items()) if k != "ent"))
    # (ropepose) B's copy hangs as A does: its pelvis as far off the rope as A's own (were 6 units further: 20 against 14)
    s.check("B's copy of A hangs as A's own body does: the pelvis %.1f from the rope, A's own %.1f (within %g)"
            % (p["pelvis"][0], o["pelvis"][0] if o else -1.0, ROPE_PELVIS_MATCH),
            o is not None and abs(p["pelvis"][0] - o["pelvis"][0]) <= ROPE_PELVIS_MATCH)
    hands, feet = max(held_wrists(p) or [99.0]), max(p["ankle0"][0], p["ankle1"][0])
    s.check("B's copy of A on the rope: climbing %.2f, the wrists %.1f from it at most (%.0f), the ankles %.1f (%.0f)"
            % (p["climb"][0], hands, ROPE_HANDS_NEAR, feet, ROPE_FEET_NEAR), hands <= ROPE_HANDS_NEAR and feet <= ROPE_FEET_NEAR,
            "after %d reports" % n)
    # (ropepose) MP_ROPECLIMB_SHOTS=1: B floats beside A (noclip, 64 units off his side, at his hips) and screenshots his
    # copy hanging, then at the swing's end (not in the suite: the default run is unchanged).
    shots = os.environ.get("MP_ROPECLIMB_SHOTS") == "1" and still
    if shots:
        side = (still[0] + 64.0, still[1], still[2] - 8.0)
        b.cmd("setpos %.1f %.1f %.1f 0 180 0" % side)
        for _ in range(60):
            got = org_of(status(srv), "mp_client", "slot", eb - 1)
            if got and math.hypot(got[0] - side[0], got[1] - side[1]) <= 4:
                break
        reports(s, [b])
        reports(s, [b])
        s.note("screenshot (B beside A hanging): %s" % b.screenshot("ropepose_hang"))
    # The swing: A pumps it, looking level (the flat rope's keys: forward pushes the swing his way).
    a.cmd("centerview; +forward")
    xs, worst, pelvis = [], [0.0, 0.0], [0.0, 0.0]
    for _ in range(30):
        at = a_at()
        if at:
            xs.append(at[1])
        ls = reports(s, [b, a])
        q, qo = pose(ls), own(ls)
        if q:
            worst[0] = max([worst[0]] + held_wrists(q))
            worst[1] = max(worst[1], q["ankle0"][0], q["ankle1"][0])
            pelvis[0] = max(pelvis[0], q["pelvis"][0])
        if qo:
            pelvis[1] = max(pelvis[1], qo["pelvis"][0])
        if xs and max(xs) - min(xs) > 48:
            break
    if shots:
        s.note("screenshot (B beside A swinging): %s" % b.screenshot("ropepose_swing"))
    a.cmd("-forward")
    s.note("swinging: B's copy's pelvis %.1f from the rope at most, A's own %.1f" % (pelvis[0], pelvis[1]))
    swing = max(xs) - min(xs) if xs else 0.0
    s.check("A pumps the rope: the server swings him %.0f units (over 24); B's copy stays on it swinging: the wrists %.1f "
            "(%.0f), the ankles %.1f (%.0f)" % (swing, worst[0], ROPE_HANDS_NEAR + ROPE_SWING_SLACK, worst[1],
                                                ROPE_FEET_NEAR + ROPE_SWING_SLACK),
            swing > 24 and worst[0] <= ROPE_HANDS_NEAR + ROPE_SWING_SLACK and worst[1] <= ROPE_FEET_NEAR + ROPE_SWING_SLACK)
    a.cmd("+jump")
    _, n = reports_until(s, [b], lambda ls: not pose(ls) and (body_of(ls[0], "mp_body").get(ea) or {}).get("climb", [1.0])[0] < 0.5,
                         "B's copy of A off the rope")
    a.cmd("-jump")
    s.check("A jumps off: B's copy holds the rope no more", True, "after %d reports" % n)


POLE_A_AT = (-440.0, -490.0, 24.0)   # mp_pole (vrropes): A south of the metal pole (-440 -450, upright), facing north
POLE_B_AT = (-440.0, -410.0, 24.0)   # ... B north of it facing south (flat: the second climber), or watching from further
POLE_WATCH_AT = (-440.0, -640.0, 24.0)
POLE_IN_AT = (-432.0, -450.0, 24.0)  # (ropes8e) B standing in the pole's box (his box against it 1.5 into it from the east)
MONKEY_A_AT = (-60.0, 600.0, 24.0)   # mp_monkey (vrropes): A under the level rope (y 600, z 100, x -300..300), facing east
MONKEY_WATCH_AT = (0.0, 420.0, 24.0)


def rope_climber(s, srv, inst, ent, what, keys):
    """`inst` (flat) holds `keys` until the server has him 48 units up (or along) from where he was -> his place then,
    once he is still."""
    def at():
        return org_of(status(srv), "mp_client", "slot", ent - 1)
    start = at()
    inst.cmd(" ".join("+%s;" % k for k in keys))
    moved = None
    for _ in range(120):
        p = at()
        if p and start and max(abs(p[i] - start[i]) for i in range(3)) >= 48:
            moved = p
            break
    inst.cmd(" ".join("-%s;" % k for k in keys))
    s.check("%s (the server has him from %s to %s)" % (what, start, moved), moved is not None, "the server's at %s" % (at(),))
    if not moved:
        raise StepFailed(what)
    prev = moved
    for _ in range(60):
        p = at()
        if p and prev and max(abs(p[i] - prev[i]) for i in range(3)) < 1.0:
            return p
        prev = p
    return prev


def rope_copy_check(s, viewer, other, ent, what, feet):
    """`viewer`'s copy of player `ent` on a rope or pole: climbing, its held wrists within ROPE_HANDS_NEAR of it as the
    viewer has it, (`feet`) the ankles within ROPE_FEET_NEAR (a monkey traverse's legs hang: not judged)."""
    def pose(ls):
        return by_ent(ls[0], "mp_ropepose").get(ent)
    lines, n = reports_until(s, [viewer, other], lambda ls: (pose(ls) or {}).get("climb", [0.0])[0] >= 0.9, what)
    p = pose(lines) or {}
    s.note("%s: " % viewer.name + " ".join("%s=%.1f" % (k, v[0]) for k, v in sorted(p.items()) if k != "ent"))
    if not p:
        s.check(what, False, "no mp_ropepose after %d reports" % n)
        return
    hands = max([p[k][0] for k in ("wrist0", "wrist1") if p[k][0] >= 0.0] or [99.0])
    ankles = max(p["ankle0"][0], p["ankle1"][0])
    s.check("%s: climbing %.2f, the wrists %.1f from it at most (%.0f)%s" % (
        what, p["climb"][0], hands, ROPE_HANDS_NEAR, (", the ankles %.1f (%.0f)" % (ankles, ROPE_FEET_NEAR)) if feet else ""),
        hands <= ROPE_HANDS_NEAR and (not feet or ankles <= ROPE_FEET_NEAR))


def pole(s, vr_viewer=False):
    """(ropes8d) Another player on a pole (ROPES.md "Poles", "Multiplayer"), vrropes' upright metal pole: flat A walks into
    it and climbs (forward); flat B (mp_pole) walks into it from the other side and climbs it too (two climbers pass one
    pole's boxes: passSolids' solidPass), each seeing the other's copy on it: climbing, the hands on it, the legs round it.
    mp_pole_vr: B (VR, watching) sees A's copy so."""
    srv, a, b, ea, eb = join(s, flat="A" if vr_viewer else "AB", level="vrropes",
                             first=lambda srv, a, ea: put_at(srv, a, ea, POLE_A_AT, 90))
    put_at(srv, b, eb, POLE_WATCH_AT if vr_viewer else POLE_B_AT, 90 if vr_viewer else 270)
    srv.cmd("vr_climb_stamina 0")
    if not vr_viewer:
        # B's walk into the pole's box (not taking it: vr_flat_rope 0) predicted against it: his corrections few and small
        srv.cmd("vr_flat_rope 0")
        b.cmd("vr_flat_rope 0; centerview")
        def pred():
            since = b.mark()
            b.cmd("vr_mp_predict_report")
            _, l = b.wait(r"mp_predict: ", 10, since, "B's prediction report")
            return fields(l)
        was = pred()
        b.cmd("+forward")
        pushed = 0
        for _ in range(120):  # (until he has pushed against it for 10 reads: his box stops 8.5 units off the pole)
            bat = org_of(status(srv), "mp_client", "slot", eb - 1)
            pushed += 1 if bat and bat[1] < -438.0 else 0
            if pushed >= 10:
                break
        b.cmd("-forward")
        now = pred()
        bat = org_of(status(srv), "mp_client", "slot", eb - 1)
        dn = int(now["corr_n"]) - int(was["corr_n"])
        # (ropes8e) stopped by its box on the server: his box against entities (8 half) off its face (1.5), not through
        # (ropes8d had him at y -474: passSolids took his touching box as in one, ROUND25 ropes8e)
        s.check("B walking into the pole (not taking it) is stopped by its box: the server has him at %s (the pole at y "
                "-450, his box's face 9.5 off it)" % (bat,), bool(bat) and -444.0 <= bat[1] <= -438.0)
        s.check("B's walk at the pole predicted as the server ran it: prediction on=%s, %d corrections more (2 at most), "
                "the most %s" % (now["on"], dn, now["corr_max"]), now["on"] == "1" and dn <= 2)
        srv.cmd("vr_flat_rope 1")
        b.cmd("vr_flat_rope 1")
        put_at(srv, b, eb, POLE_B_AT, 270)
    a.cmd("centerview")
    rope_climber(s, srv, a, ea, "A walks into the pole and climbs it", ["forward"])
    if not vr_viewer:
        # (ropes8e) Two passing it at once, a box really in its box: A climbs on over B's head; B set standing in the
        # pole's box (his box 1.5 into it from the east) is let through as A is (pass: both; in: B), and takes it from
        # there and climbs it too (not held by it). (Walking out of it is vr_flat_rope 0's, which would drop A.)
        rope_climber(s, srv, a, ea, "A climbs on up the pole", ["forward"])
        put_at(srv, b, eb, POLE_IN_AT, 180)
        b.cmd("centerview")
        solid = [fields(l) for l in rope_reports([srv])[0] if l.startswith("rope_solid")]
        both = (1 << (ea - 1)) | (1 << (eb - 1))
        s.check("A on the pole and B standing in its box both let through it (B's box in it)",
                any(int(f["pass"]) & both == both and int(f.get("in", 0)) & (1 << (eb - 1)) for f in solid),
                "rope_solid %s" % solid)
        rope_climber(s, srv, b, eb, "B, standing in its box, takes the pole A is on and climbs it too", ["forward"])
        # the server lets both through its boxes (vr_rope_info's rope_solid: pass, a bit a client), not only its owner
        solid = [fields(l) for l in rope_reports([srv])[0] if l.startswith("rope_solid")]
        both = (1 << (ea - 1)) | (1 << (eb - 1))
        s.check("the server lets both climbers through the pole's boxes", any(int(f["pass"]) & both == both for f in solid),
                "rope_solid %s" % solid)
    rope_copy_check(s, b, a, ea, "%s B's copy of A on the pole" % ("VR" if vr_viewer else "flat"), True)
    if not vr_viewer:
        rope_copy_check(s, a, b, eb, "flat A's copy of B on the pole", True)


def monkey(s, vr_viewer=True):
    """(ropes8d) Another player on a strung rope (ROPES.md "Strung ropes"), vrropes' level rope: flat A jumps up to it
    (taken overhead), goes along it hand over hand (forward, facing along it); B's copy of him (mp_monkey: VR B;
    mp_monkey_flat: flat B) hangs under it: climbing, the hands on it (the legs hang: not judged), as he goes too."""
    srv, a, b, ea, eb = join(s, flat="A" if vr_viewer else "AB", level="vrropes",
                             first=lambda srv, a, ea: put_at(srv, a, ea, MONKEY_A_AT, 0))
    put_at(srv, b, eb, MONKEY_WATCH_AT, 90)
    srv.cmd("vr_climb_stamina 0")
    a.cmd("centerview; +jump")
    view = "VR" if vr_viewer else "flat"
    rope_copy_check(s, b, a, ea, "%s B's copy of A hanging under the strung rope" % view, False)
    a.cmd("-jump")
    rope_climber(s, srv, a, ea, "A goes along the strung rope hand over hand", ["forward"])
    rope_copy_check(s, b, a, ea, "%s B's copy of A still under the rope where he went" % view, False)


def avatarcal(s):
    """The avatar calibration (QVR_SVC_AVATARCAL): each client's own (height, arm length, handedness, gloves, body...)
    reaches the other and the server exactly; A changes his gloves: B gets the change; B leaves: A forgets his; B joins
    again: he gets A's at his spawn, A gets his."""
    srv, a, b, ea, eb = join(s)
    # A's own differ from the defaults (B keeps his): what B must see is A's, not his own settings.
    a.cmd("vr_height_calibration 1.82; vr_body_arm_length 1.1; vr_gloves 2; vr_glove_hue 120; vr_body_build 3; "
          "vr_skin_tone -0.4; vr_gadget_arm 1")

    def same(lines):
        la, lb, ls = lines
        own_a, own_b = cal_of(la, "mp_avatar_cal_own").get(ea), cal_of(lb, "mp_avatar_cal_own").get(eb)
        return own_a is not None and own_a["height"] == 1820 and own_a["gloves"] == 2 and \
            cal_of(lb, "mp_avatar_cal").get(ea) == own_a and cal_of(la, "mp_avatar_cal").get(eb) == own_b and \
            cal_of(ls, "mp_avatar_cal_sv").get(ea) == own_a and cal_of(ls, "mp_avatar_cal_sv").get(eb) == own_b

    (la, lb, ls), n = reports_until(s, [a, b, srv], same, "each has the other's calibration, as his own game has it")
    for l in lb:
        if l.startswith("mp_avatar_cal"):
            s.note("B: " + l.strip())
    s.check("B has A's calibration as A's game has it, and A has B's; the server both", True, "after %d reports" % n)

    a.cmd("vr_gloves 1")
    (lb,), n = reports_until(s, [b], lambda ls: (cal_of(ls[0], "mp_avatar_cal").get(ea) or {}).get("gloves") == 1,
                             "A's gloves changed: B has the change")
    s.check("A changes his gloves: B has the change", True, "after %d reports" % n)

    b.cmd("disconnect")
    (la,), n = reports_until(s, [a], lambda ls: eb not in cal_of(ls[0], "mp_avatar_cal"), "B left: A forgets his")
    s.check("B leaves: A no longer has his calibration", True, "after %d reports" % n)

    since = b.mark()
    b.cmd(s.connect_cmd(b))
    _, jb = b.wait(r"mp_event: .*joined map=e1m1", 120, since, "B joined again")
    eb = int(fields(jb)["self"])
    (la, lb), n = reports_until(s, [a, b], lambda ls: cal_of(ls[1], "mp_avatar_cal").get(ea) ==
                                cal_of(ls[0], "mp_avatar_cal_own").get(ea) and eb in cal_of(ls[0], "mp_avatar_cal"),
                                "B joined again: each has the other's calibration")
    s.check("B joins again: he has A's calibration (sent at his spawn), A has his", True, "after %d reports" % n)


BODY_UNITS = 1.5    # B's copy of A's body: its joints within this of A's own body (units)
BODY_WRIST = 1.0    # ... its wrists within this of A's drawn hands' wrists (the arms reach them)
BODY_SCALE = 0.005  # ... its scale (world units a metre) within this share of A's own
BODY_SHOULDER_CAL = 0.2  # ... with A's arms calibrated, its shoulders within this of A's own (units; MP_PLAN B3c)


# mp_rules: settings the server sets, B's own and what B must have (name: (server's, B's own, B's while joined)). A
# gameplay preset's switch (the server's off), a melee timing rule, a deep page's (Monster Groups) and the Grappling
# Hook's; one the server leaves at its default while B's differs (B gets the default); a preference (B's own stays) and
# a sound on a rule page (a player's own: not a rule).
RULES = {
    "vr_parry_stamina": ("0", "1", "0"),
    "vr_melee_speed": ("4.5", "2", "4.5"),
    "vr_mgrp_ogre_kdscale": ("0.25", "2", "0.25"),
    "vr_grapple_drop_grace": ("0.3", "0.05", "0.3"),
    "vr_knockdown": (None, "0", "1"),
    # (B6c: Locomotion's balance settings, rules by name; its comfort and controls stay each player's)
    "vr_crouch_speed_scale": ("0.5", "0.9", "0.5"),
    "vr_slope_walk": ("0", "1", "0"),
    "vr_crouch_speed": (None, "0", "1"),
}
NOT_RULES = {"vr_gunangle": "55", "vr_bash_sound": "0.2", "vr_movement_mode": "0", "vr_move_curve": "1.5"}


def rules_report(inst, names):
    """vr_serverrules <names> on `inst` -> ({name: {rule, value, own, locked}}, {count, remote, shadowed})."""
    since = inst.mark()
    inst.cmd("vr_serverrules %s; echo mp_rules_done" % " ".join(names))
    n, _ = inst.wait(r"mp_rules_done", 10, since, "%s's rules" % inst.name)
    out, head = {}, {}
    for l in inst.lines[since:n]:
        if l.startswith("mp_rule:"):
            f = dict(m.groups() for m in re.finditer(r"(\w+)=(\S+)", l))
            out[f.pop("name")] = f
        elif l.startswith("mp_rules:"):
            head = dict(m.groups() for m in re.finditer(r"(\w+)=(\S+)", l))
    return out, head


def rules_until(s, inst, names, done, what, tries=30):
    """rules_report until done(report, head) (at most `tries` reports) -> (report, head, n)."""
    for n in range(1, tries + 1):
        r, h = rules_report(inst, names)
        if done(r, h):
            return r, h, n
    s.check(what, False, "not after %d reports: %r" % (tries, rules_report(inst, names)))
    raise StepFailed(what)


def same(a, b):
    return abs(float(a) - float(b)) < 1e-4


def rules(s):
    """Server rules (vr_serverrules.cpp; MP_PLAN B6): every balance setting (the rule pages' settings, the gameplay
    preset's, the melee timing) is the server's. B joins with his own values: he plays by the server's (his cvars hold
    them while joined), the rows dimmed and the Gameplay Presets' buttons locked; his own written to his config; a
    preference and a sound stay his. The server changes rules: B follows. B leaves: his own values back."""
    names = list(RULES) + list(NOT_RULES)
    srv_first = "; ".join("%s %s" % (k, v[0]) for k, v in RULES.items() if v[0] is not None)
    b_first = "; ".join(["%s %s" % (k, v[1]) for k, v in RULES.items()] + ["%s %s" % kv for kv in NOT_RULES.items()])
    srv, a, b, ea, eb = join(s, srv_first=srv_first, b_first=b_first)

    def joined_ok(r, h):
        return h.get("shadowed") == "1" and all(k in r and same(r[k]["value"], v[2]) for k, v in RULES.items())

    r, h, n = rules_until(s, b, names, joined_ok, "B plays by the server's rules")
    s.note("B: %r %r" % (h, r))
    s.check("B joined: his rules are the server's (a default where the server left one)", True, "after %d reports" % n)
    s.check("B's own values kept for when he leaves",
            all(same(r[k]["own"], v[1]) for k, v in RULES.items()), repr({k: r[k]["own"] for k in RULES}))
    s.check("B's rule rows locked (dimmed in his menu)", all(r[k]["locked"] == "1" for k in RULES),
            repr({k: r[k]["locked"] for k in RULES}))
    s.check("a preference and a sound on a rule page stay B's own, unlocked",
            all(r[k]["rule"] == "0" and r[k]["locked"] == "0" and same(r[k]["value"], v) for k, v in NOT_RULES.items()),
            repr({k: r[k] for k in NOT_RULES}))
    rs, hs = rules_report(srv, names)
    s.check("the server and B know the same rules, every balance page's",
            hs.get("count") == h.get("count") and int(h.get("count", 0)) > 1000, "server %s, B %s" % (
                hs.get("count"), h.get("count")))
    # (Review 2026-11-18, finding 14: each setting's kind written where it is declared, vr_cvars.inc's QVR_RULE, QVR_PREF,
    # QVR_OWN; every setting on a rule page has one, the old substring list only the check's guess)
    kinds = {k: rs.get(k, {}).get("kind") for k in list(RULES) + ["vr_gunangle", "vr_bash_sound"]}
    want = dict({k: "rule" for k in RULES}, vr_gunangle="pref", vr_bash_sound="own")
    s.check("every setting on a rule page has its kind (none unset); the rules', a preference's and a sound's as written",
            hs.get("unset") == "0" and h.get("unset") == "0" and kinds == want,
            "unset: server %s, B %s; kinds %r" % (hs.get("unset"), h.get("unset"), kinds))

    since = b.mark()
    b.cmd("vr_menu_press Gameplay Presets > Classic;echo mp_pressed")  # (";": the line's end is not in the label)
    _, l = b.wait(r"vr_menu_press: Gameplay Presets > Classic", 10, since, "B pressing Classic")
    s.check("B's Gameplay Presets buttons locked (a preset sets rules)", "locked" in l, l.strip())

    since = b.mark()
    b.cmd("writeconfig mp_rules_b; echo mp_rules_written")
    b.wait(r"mp_rules_written", 10, since, "B's config written")
    cfg = os.path.join(s.base, "quakevr", "mp_rules_b.cfg")
    written = {}
    if os.path.exists(cfg):
        with open(cfg, encoding="ascii", errors="replace") as f:
            for line in f:
                m = re.match(r'(\w+) "([^"]*)"', line)
                if m:
                    written[m.group(1)] = m.group(2)
    # (a sparse config: a setting at its default is not written)
    s.check("B's config holds his own values, not the server's", all(
        same(written[k], v[1]) if k in written else v[1] == "1" for k, v in RULES.items()),
        "%s: %r" % (cfg, {k: written.get(k) for k in RULES}))

    srv.cmd("vr_parry_stamina 1; vr_mgrp_ogre_kdscale 0.75")
    r, h, n = rules_until(s, b, names, lambda r, h: same(r["vr_parry_stamina"]["value"], 1) and
                          same(r["vr_mgrp_ogre_kdscale"]["value"], 0.75), "the server's changed rules reach B")
    s.check("the server changes two rules: B plays by the new values", True, "after %d reports" % n)

    b.cmd("disconnect")
    r, h, n = rules_until(s, b, names, lambda r, h: h.get("shadowed") == "0", "B left: his own values back")
    s.check("B leaves: his own values back, nothing locked", all(
        same(r[k]["value"], v[1]) and r[k]["locked"] == "0" for k, v in RULES.items()),
        repr({k: (r[k]["value"], r[k]["locked"]) for k in RULES}))


LOOK_STEP = 0.01  # a colour's part: B's own look and A's copy of it (his calibration's whole numbers: 255ths, degrees)


def looks_of(lines, prefix):
    """"mp_look:" / "mp_look_own:" lines -> {(ent, part): fields (avatar_fields: kind, tone, glove, nails)}."""
    out = {}
    for l in lines:
        if l.startswith(prefix + ":"):
            f = avatar_fields(l.split(":", 1)[1])
            out[(int(f["ent"][0]), int(f["part"][0]))] = f
    return out


def look_off(x, y):
    """The largest difference between two looks' colours (inf: a kind or a part missing)."""
    if not x or not y or x["kind"] != y["kind"]:
        return float("inf")
    return max(abs(p - q) for k in ("tone", "glove", "nails") for p, q in zip(x[k], y[k]))


def cosmetics(s):
    """Cosmetics others see (MP_PLAN B6): B's skin tone, gloves (kind and colour) and painted nails on his hands and
    body as A draws them are B's own (his avatar calibration), not A's; B's height: A's copy of his body is as tall; A's
    own hands keep A's look."""
    srv, a, b, ea, eb = join(s)
    b.cmd("vr_height_calibration 1.9; vr_gloves 2; vr_glove_hue 120; vr_glove_saturation 0.8; vr_glove_lightness 0.4; "
          "vr_skin_tone -0.4; vr_nails 1; vr_nail_hue 300; vr_nail_saturation 0.7; vr_nail_lightness 0.5")
    a.cmd("vr_gloves 0; vr_nails 0; vr_skin_tone 0")

    def seen(lines):
        la, lb = lines
        own_b, at_a = looks_of(lb, "mp_look_own"), looks_of(la, "mp_look")
        return all(look_off(own_b.get((eb, p)), at_a.get((eb, p))) <= LOOK_STEP for p in (0, 1)) and \
            (own_b.get((eb, 0)) or {}).get("glove", [0] * 4)[3] == 2

    (la, lb), n = reports_until(s, [a, b], seen, "A draws B's hands with B's look")
    own_b, at_a = looks_of(lb, "mp_look_own"), looks_of(la, "mp_look")
    for p in (0, 1, 2):
        s.note("part %d: B's own %r, A's copy %r" % (p, own_b.get((eb, p)), at_a.get((eb, p))))
    s.check("A draws B's hands with B's skin tone, gloves (full, green) and nails", True, "after %d reports, off by %.4f" % (
        n, max(look_off(own_b.get((eb, p)), at_a.get((eb, p))) for p in (0, 1))))
    s.check("A draws B's body with B's skin tone", look_off(own_b.get((eb, 2)), at_a.get((eb, 2))) <= LOOK_STEP,
            "B's %r, A's %r" % (own_b.get((eb, 2)), at_a.get((eb, 2))))
    own_a = looks_of(la, "mp_look_own")
    s.check("A's own hands keep his own look (no gloves, no nails)", all(
        (own_a.get((ea, p)) or {}).get("glove", [1] * 4)[3] == 0 and (own_a.get((ea, p)) or {}).get("nails", [1] * 4)[3] == 0
        for p in (0, 1)), repr({p: own_a.get((ea, p)) for p in (0, 1)}))

    (la,), n = reports_until(s, [a], lambda ls: abs((body_of(ls[0], "mp_body").get(eb) or {}).get("eyeheight", [0])[0] -
                                                    1.9) < 0.002, "A's copy of B's body at B's height")
    s.check("B's height: A's copy of his body is posed at it", True, "after %d reports" % n)
    cosmetics_b6b(s, srv, a, b, ea, eb)
    cosmetics_b6c(s, srv, a, b, ea, eb)


def plates_of(lines, prefix):
    """"mp_plates:" / "mp_plates_own:" lines -> {ent: fields (type, pieces, skin, slots, shown) with "model"}."""
    out = {}
    for l in lines:
        if l.startswith(prefix + ":"):
            f = avatar_fields(re.sub(r"\s*model=\S+", "", l.split(":", 1)[1]))
            m = re.search(r"\bmodel=(\S+)", l)
            f["model"] = m.group(1) if m else ""
            out[int(f["ent"][0])] = f
    return out


def cosmetics_b6c(s, srv, a, b, ea, eb):
    """MP_PLAN B6c: B's armour's plates on A's copy of his body: B given yellow armour (150: every piece), A draws the
    plates' model made over B's body model, in yellow, the pieces B's own body shows, posed on B's bones (its slots
    shown); no pauldron under a plate, as on B's own; B's armour worn down: A's copy sheds the pieces B's own does. A
    looks at B: one image of B's holstered shotgun, pauldrons and plates as A draws them. B11c part 2: A's copy's plates
    come in by their morph (A's vr_player_plates_morph_time, 2 s here) and settle, as B's own do."""
    srv.cmd("vr_player_plates 1")
    a.cmd("vr_player_plates 1")
    b.cmd("vr_player_plates 1; vr_body_pauldrons 1; vr_body_pauldron_style 2")
    la, lb = reports(s, [a, b])
    head_a = (by_ent(lb, "mp_avatar").get(ea) or {}).get("head")
    head_b = (by_ent(la, "mp_avatar").get(eb) or {}).get("head")
    if head_a is not None and head_b is not None:
        face(s, a, b, head_a, head_b)
        s.note("screenshot (A sees B: holstered shotgun, steel pauldrons): %s" % a.screenshot("sees_B_cosmetics"))
    # (B11c part 2: A's copy's plates come in by their morph, as B's own do; A's morph time at its longest, 2 s, so that a
    # report or two sees it under way)
    a.cmd("vr_player_plates_morph_time 2")
    b.cmd("give a 150")

    def morphing(lines):
        copy = plates_of(lines[0], "mp_plates").get(eb)
        return copy is not None and copy["coming"][0] > 0

    (la, lb), n = reports_until(s, [a, b], morphing, "A's copy of B's plates comes in by a morph")
    copy = plates_of(la, "mp_plates")[eb]
    s.check("B11c: A's copy's plates come in by their morph, as B's own do (a morph begun, its shader value coming in)",
            copy["begun"][0] >= 1 and 2.0 < copy["morph"][0] <= 3.0 and copy["coming"][0] == copy["pieces"][0],
            "begun %d, coming %d of %d, morph %.3f, after %d reports" % (copy["begun"][0], copy["coming"][0],
                                                                          copy["pieces"][0], copy["morph"][0], n))

    def plated(want):
        def done(lines):
            la, lb = lines
            own, copy = plates_of(lb, "mp_plates_own").get(eb), plates_of(la, "mp_plates").get(eb)
            return own is not None and copy is not None and own["type"] == [2.0] and own["pieces"][0] > 0 and \
                copy["pieces"] == own["pieces"] and copy["type"] == own["type"] and want(own["pieces"][0])
        return done

    (la, lb), n = reports_until(s, [a, b], plated(lambda p: True), "A draws B's yellow plates, his pieces")
    own, copy = plates_of(lb, "mp_plates_own")[eb], plates_of(la, "mp_plates")[eb]
    body_at_a = (body_of(la, "mp_body").get(eb) or {}).get("model", "")
    s.note("B's own: %r; A's copy: %r" % (own, copy))
    full = int(own["pieces"][0])
    s.check("A draws B's plates: yellow, the pieces B's own body shows", True, "type %d, pieces %d, after %d reports" % (
        copy["type"][0], full, n))
    s.check("A's copy's plates are the plates' model of B's body model there, in yellow (skin 1)",
            copy["model"] == body_at_a + "#armor" and copy["skin"] == [1.0], "plates %s skin %r on body %s" % (
                copy["model"], copy["skin"], body_at_a))
    s.check("A's copy's plates are posed on B's bones (their slots shown)", copy["shown"][0] > 0 and
            copy["shown"][0] <= copy["slots"][0], "%d of %d slots" % (copy["shown"][0], copy["slots"][0]))
    caps_own, caps_a = pauldrons_of(lb, "mp_pauldron_own"), pauldrons_of(la, "mp_pauldron")
    sides_own = sorted(side for (e, side) in caps_own if e == eb)
    sides_a = sorted(side for (e, side) in caps_a if e == eb)
    s.check("no pauldron under a plate: A's copy shows B's caps where B's own body does", sides_own == sides_a,
            "B's own caps on sides %r, A's copy's %r (pieces %d)" % (sides_own, sides_a, full))
    s.check("A wears none: no plates of his own", (plates_of(la, "mp_plates_own").get(ea) or {}).get("pieces", [0]) ==
            [0.0], repr(plates_of(la, "mp_plates_own").get(ea)))
    if head_a is not None and head_b is not None:
        s.note("screenshot (A sees B plated): %s" % a.screenshot("sees_B_plates"))

    def settled(lines):
        copy = plates_of(lines[0], "mp_plates").get(eb)
        return copy is not None and copy["coming"][0] == 0 and copy["pieces"][0] == full
    (la, lb), n = reports_until(s, [a, b], settled, "A's copy of B's plates settles", tries=200)
    copy = plates_of(la, "mp_plates")[eb]
    s.check("B11c: A's copy's plates settle (none coming, the settled entity drawn, the same morphs begun)",
            copy["skin"] == [1.0] and copy["shown"][0] > 0, "begun %d, skin %r, %d of %d slots shown, after %d reports" % (
                copy["begun"][0], copy["skin"], copy["shown"][0], copy["slots"][0], n))
    a.cmd("vr_player_plates_morph_time 0.6")

    b.cmd("give a 101")
    (la, lb), n = reports_until(s, [a, b], plated(lambda p: bin(int(p)).count("1") < bin(full).count("1")),
                                "A's copy sheds the pieces B's own does")
    own, copy = plates_of(lb, "mp_plates_own")[eb], plates_of(la, "mp_plates")[eb]
    s.check("B's armour worn down: A's copy shows the fewer pieces B's own does", True, "pieces %d (from %d), after %d "
            "reports" % (own["pieces"][0], full, n))


HOLSTER_UNITS = 1.0  # B's holstered weapon at A, and the server's holsterpos for B, within this of where B drew it
HOLSTER_SLOT = 2     # the holster B is given a shotgun in (impulse 90 + it: the holsterweapon<slot> fields, body::Holster)


def holsters_of(lines, prefix):
    """"mp_holster:" / "mp_holster_own:" / "mp_holster_sv:" / "mp_holster_point_own:" lines -> {(ent, slot): fields}."""
    out = {}
    for l in lines:
        if l.startswith(prefix + ":"):
            f = avatar_fields(l.split(":", 1)[1])
            m = re.search(r"model=(\S+)", l)
            f["model"] = m.group(1) if m else ""
            out[(int(f["ent"][0]), int(f["slot"][0]))] = f
    return out


def pauldrons_of(lines, prefix):
    """"mp_pauldron:" / "mp_pauldron_own:" lines -> {(ent, side): fields (skin, scale, at)}."""
    out = {}
    for l in lines:
        if l.startswith(prefix + ":"):
            f = avatar_fields(l.split(":", 1)[1])
            out[(int(f["ent"][0]), int(f["side"][0]))] = f
    return out


def flatbody(s):
    """MP_PLAN B6b: a flat-screen player's body as others draw it is at his body height (vr_flat_body_height), as his own
    is, not his headset Height."""
    srv, a, b, ea, eb = join(s, flat="B", b_first="vr_height_calibration 1.8; vr_flat_body_height 1.5")

    def posed(lines):
        copy = body_of(lines[0], "mp_body").get(eb)
        return copy is not None and abs(copy["eyeheight"][0] - 1.5) < 0.002

    (la,), n = reports_until(s, [a], posed, "A's copy of flat B's body at his flat body height")
    s.check("flat B: A's copy of his body posed at his flat body height (1.5 m, his Height 1.8)", True, "after %d reports" % n)


SLIDE_SHARE = 0.5  # mp_slide: the slide pose's share (BodyPitch::sliding) a body has at least, mid-slide


def slide_peaks(s, a, b, ea, eb, own_check, tries=60):
    """Reports on A and B until A's copy of B's body and (`own_check`) B's own reach the slide pose (each its own peak:
    the reports are frames apart) -> (A's copy's peak sliding, its most leant-back pitch, B's own two, reports)."""
    copy, own = (0.0, 0.0), (0.0, 0.0)
    n = 0
    for n in range(1, tries + 1):
        la, lb = reports(s, [a, b])
        c = body_of(la, "mp_body").get(eb)
        o = body_of(lb, "mp_body_own").get(eb)
        if c and "sliding" in c:
            copy = (max(copy[0], c["sliding"][0]), min(copy[1], c["pitch"][0]))
        if o and "sliding" in o:
            own = (max(own[0], o["sliding"][0]), min(own[1], o["pitch"][0]))
        if copy[0] >= SLIDE_SHARE and (own[0] >= SLIDE_SHARE or not own_check):
            break
    return copy, own, n


def slide(s, flat=True):
    """mpslide (2026-11-20): his words, "from another client's perspective it looks like the sliding player is just
    crouched". The server's slide (slide::onServer) goes in B's avatar's gear byte (avatarGearSliding: no new message,
    no protocol change); A's copy of B's body takes the slide pose (BodyPitch::sliding, the trunk leant back, the legs
    out in front), as B's own does. Flat B (mp_slide: +flatcrouch from a run), or a VR mock B (mp_slide_vr: vr_slide,
    the turning stick pulled down while running; his own body, a client of another server, poses no slide of its own:
    slide::headsetPose is the host's only)."""
    s.client_sound = True  # (syncfix5: A hears B's slide: the sounds A started round him, vr_mp_near)
    srv, a, b, ea, eb = join(s, flat="B" if flat else "")
    put_at(srv, b, eb, OPEN_SPOT, 90)
    b.quiet(r"mp_(move|pos): ", 0.5, 20, "B at rest")
    since = b.mark()
    # (flatgreen3: VR B runs too: cl_alwaysrun 0 is the shipped default, and a walk's 200 is under vr_flat_slide_min's
    # 220: no slide at all)
    b.cmd("cl_alwaysrun 1; +forward" if flat else "cl_alwaysrun 1; vr_slide 1; vr_mock_stick off 0 1")
    b.wait(r"mp_move: .*self=1", 20, since, "B running north")
    # Up to his run's speed before the slide (a slide starts from a run): 40 units north of his spot in A's copy (at
    # most 40 reports: a definite end; on a busy machine the press once found him still walking, no slide).
    for _ in range(40):
        c = body_of(reports(s, [a])[0], "mp_body").get(eb)
        if c and c["pelvis"][1] - OPEN_SPOT[1] >= 40.0:
            break
    t0 = time.time()
    b.cmd("+flatcrouch" if flat else "vr_mock_stick main 0 -1")
    copy, own, n = slide_peaks(s, a, b, ea, eb, flat)
    heard = near_counts(near_lines(a, eb, time.time() - t0 + 0.5))  # (syncfix5: the sounds A started at B since the press)
    b.cmd("-flatcrouch; -forward" if flat else "vr_mock_stick main 0 0; vr_mock_stick off 0 0")
    what = "flat B" if flat else "VR B"
    s.check("%s slides: A hears it from him (the scuff and the scrape, from his copy's slide: syncfix5)" % what,
            heard.get("sound:vr/slide_start.wav", 0) >= 1 and heard.get("sound:vr/slide_loop.wav", 0) >= 1,
            "A's sounds round him: %s" % (sorted(k for k in heard if k.startswith("sound:")) or "-"))
    s.metric("slide_copy", copy[0])
    s.metric("slide_own", own[0])
    if flat:
        s.check("%s slides: his own body in the slide pose (sliding %g at least)" % (what, SLIDE_SHARE),
                own[0] >= SLIDE_SHARE, "sliding %.2f, pitch %.1f" % own)
    s.check("%s slides: A's copy of his body in the slide pose too, leant back (the server's word)" % what,
            copy[0] >= SLIDE_SHARE and copy[1] < -1.0, "sliding %.2f, pitch %.1f; after %d reports" % (copy + (n,)))


ADS_HEAD_EYE = 2.0   # mp_adspose: A's copy of sighted flat B: its drawn eyes within this of B's eye (units; before adspose 5.1, adspose 3.8; the cheek's offset 1.2)
ADS_REACH = 1.02     # (noted) no arm stretched past this share of its own length
ADS_WIDS = list(range(1, 32)) + [0]  # the weapons tried (vr_test_give_wid; 0: impulse 2, the shotgun he starts with, which a give does not take him back to): those with a sight line are measured
ADS_POSTURES = ("stand", "crouch", "slide")  # each weapon: standing, crouched, sliding (the slow slide below; MP_ADSPOSE_SLIDE=0: none) ...
ADS_LOOKS = (("level", 0.0), ("up", -40.0), ("down", 40.0))  # ... each looking level, 40 up, 40 down
ADS_CAMS = (("side", (52.0, 10.0), 180.0), ("front", (6.0, 64.0), 270.0))  # A's spots off B (B faces north) and his yaw
ADS_VIEW_FOV = 55.0  # A's field of view: B fills the shot
ADS_VIEW_HEIGHT = 22.0  # A's eye above his origin (A aims at B's copy's head: crouched or sliding B in the shot too)
ADS_FACE_W = 0.08    # metres: the face's half width and ...
ADS_FACE_AHEAD = (-0.12, 0.45)  # ... its depth along the sights off the drawn eyes: an arm above the sight line there crosses the face
ADS_POSED_AFTER = 0.1  # s of the server's time: B's moves after his still message reach the server and come back to A
ADS_SLIDE_START = 0.0  # a slide starts this far south of the spot, slowly (the rules below), A's camera following (adspose3: 0 -- 40 south was in the wall behind the coop starts, the server put him 40 north of the spot, stuck)
ADS_SLIDE_RULES = "vr_flat_slide_min 20; vr_flat_slide_boost 0; vr_flat_slide_friction 0; vr_flat_slide_end 5; vr_flat_slide_stamina 0; vr_flat_slide_cooldown 0"
ADS_SLIDE_WALK = "cl_alwaysrun 0; cl_forwardspeed 60"  # B's walk into the slide, kept (no friction): on his sights about half
# that (adspose3: 30 slid not at all, under vr_flat_slide_min 20); he has 38 units north of the spot before he is stopped


def seg_points(a, b, n=12):
    return [[a[i] + (b[i] - a[i]) * k / n for i in range(3)] for k in range(n + 1)]


def adspose_numbers(own, adsl, copy, pitch):
    """B's own body (mp_body_own), his ADS line (mp_adspose_own) and A's copy of his body (mp_body) at a look `pitch`
    (B faces north) -> the measures: eye to the copy's drawn eyes, rear sight to them, the arms' reach; per arm its elbow
    above the copy's drawn eyes along the look's up (under the gun: negative), and how far any of it stands above the
    sight line in front of the face (nothing across the face: negative)."""
    out = {"copy_eye": dist(copy["headeye"], adsl["eye"]), "rear_eye": dist(adsl["rear"], copy["headeye"]),
           "head_up": adsl["eye"][2] - copy["headeye"][2]}
    p = math.radians(pitch)
    fwd, up, right = (0.0, math.cos(p), -math.sin(p)), (0.0, math.sin(p), math.cos(p)), (1.0, 0.0, 0.0)
    m = copy["m2w"][0]
    e = copy["headeye"]
    dot = lambda v, w: sum(x * y for x, y in zip(v, w))
    for h in (0, 1):
        S, E, W = copy["shoulder%d" % h], copy["elbow%d" % h], copy["wrist%d" % h]
        out["reach%d" % h] = dist(S, W)
        out["elbow%d" % h] = dot([E[i] - e[i] for i in range(3)], up)
        face = -99.0
        for q in seg_points(S, E) + seg_points(E, W):
            d = [q[i] - e[i] for i in range(3)]
            if abs(dot(d, right)) < ADS_FACE_W * m and ADS_FACE_AHEAD[0] * m < dot(d, fwd) < ADS_FACE_AHEAD[1] * m:
                face = max(face, dot(d, up))
        out["face%d" % h] = face
    return out


def ads_cam(srv, a, ea, at, pitch, yaw, tries=60):
    """A put at `at` looking `pitch` down, `yaw` (setpos), until the server has him there (a definite end: `tries`)."""
    a.cmd("setpos %.1f %.1f %.1f %.1f %.1f 0; noclip" % (at[0], at[1], at[2], pitch, yaw))
    for _ in range(tries):
        got = org_of(status(srv), "mp_client", "slot", ea - 1)
        if got and math.hypot(got[0] - at[0], got[1] - at[1]) <= 4:
            return
    raise StepFailed("A never put at %s" % (at,))


def adspose(s):
    """adspose (2026-11-21): his words, "The ADS pose for some weapons looks silly". A flat B holds each weapon with a
    sight line on its sights (vr_flat_aim 1), standing, crouched and sliding, each looking level, 40 up and 40 down; flat
    A watches from his side and from in front, aimed at his head. Measured: A's copy of B's drawn eyes against the eye
    B's view and sights are solved from (mp_adspose_own), the rear sight against them, the arms (adspose2: the elbows
    under the gun, nothing across the face); a screenshot from A of each. MP_ADSPOSE_SEEN=0 in the environment: both with
    vr_flat_seen_ads 0, the body as before adspose (the before shots); MP_ADSPOSE_WIDS=5,8: those weapons only;
    MP_ADSPOSE_SLIDE=0: no slides (adspose3: they start on the dedicated server, B walked into them from the spot)."""
    seen = os.environ.get("MP_ADSPOSE_SEEN", "1")
    srv, a, b, ea, eb = join(s, flat="AB")
    srv.cmd(ADS_SLIDE_RULES)
    put_at(srv, b, eb, OPEN_SPOT, 90)
    put_at(srv, a, ea, (OPEN_SPOT[0] + ADS_CAMS[0][1][0], OPEN_SPOT[1] + ADS_CAMS[0][1][1], OPEN_SPOT[2]), ADS_CAMS[0][2])
    b.cmd("god; notarget; vr_flat_seen_ads %s; %s; %s" % (seen, ADS_SLIDE_RULES, ADS_SLIDE_WALK))
    a.cmd("god; notarget; impulse 1; fov %g; vr_flat_seen_ads %s" % (ADS_VIEW_FOV, seen))
    b.quiet(r"mp_(move|pos): ", 0.5, 20, "both at rest")
    rows, worst = [], {"copy_eye": 0.0, "rear_eye": 0.0, "elbow": -99.0, "face": -99.0, "slide_up": 0.0}
    last_model, stand_up, slides = "", None, [0, 0]

    def settle(pitch, crouched, first):
        """B's weapon in his hand, on its sights at the look and the crouch asked for and still (his eye and rear sight
        over two reports), A's copy of his body posed from an avatar B sent after that (the server's times: B's message
        when he was still, the copy's drawn time ADS_POSED_AFTER later) and still over 3 reports running -> (adsl, own,
        copy, model) or None (no sight line). A definite end: 120 reports (the travel is 18 frames, the stance's ease 29,
        the crouch's). adspose3: the copy was taken still while it still showed the look before (the interpolation's
        delay is longer than 3 reports): its eyes 7.0 off, an arm over the line."""
        got, still, model, prev_b, still_since = None, 0, "-", None, None
        for k in range(120):
            la, lb = reports(s, [a, b])
            adsl = by_ent(lb, "mp_adspose_own").get(eb)
            own, copy = body_of(lb, "mp_body_own").get(eb), body_of(la, "mp_body").get(eb)
            m = next((re.search(r"wmodel=(\S+)", l) for l in lb if l.startswith("mp_adspose_own:")), None)
            model = m.group(1) if m else "-"
            b_ready = adsl and own and copy and (model != last_model or not first or k >= 30) and \
                abs(adsl["look"][0] - pitch) < 1.0 and (adsl["sight"][0] == 0 or adsl["ads"][0] >= 0.99) and \
                (stand_up is None or (adsl["eyeup"][0] < stand_up - 4.0) == crouched)
            b_still = b_ready and prev_b is not None and dist(prev_b["eye"], adsl["eye"]) < 0.05 and \
                dist(prev_b["rear"], adsl["rear"]) < 0.05
            still_since = (still_since if still_since is not None else prev_b["stime"][0]) if b_still else None
            prev_b = adsl if b_ready else None
            ready = b_still and "posedat" in copy and copy["posedat"][0] >= still_since + ADS_POSED_AFTER
            if ready and got and all(dist(got[2][j], copy[j]) < 0.05 for j in ("headeye", "wrist0", "wrist1")):
                still += 1
            else:
                still = 0
            got = (adsl, own, copy, model) if ready else None
            if still >= 3:
                return got
        return None

    def slide_at(pitch):
        """B walks north into a slow slide (the rules above) on his sights at the look -> (adsl, own, copy) while A's copy
        of him slides and he is on them, or None (a definite end: 60 + 80 + 150 reports)."""
        start = (OPEN_SPOT[0], OPEN_SPOT[1] - ADS_SLIDE_START, OPEN_SPOT[2])
        # (adspose3: B at the start as the server has him first, then walking 4 units north of it as the server has him --
        # the copy's place was read before, still at the spot 40 north of the start, and the crouch went at once with B at
        # rest on the server: "slide: none on the press: speed 0"; 4 units: his walk's speed reached)
        b.cmd("-forward; -flatcrouch")
        b.cmd("setpos %.1f %.1f %.1f %.1f 90 0; noclip" % (start[0], start[1], start[2], pitch))
        got = None
        for _ in range(60):
            got = org_of(status(srv), "mp_client", "slot", eb - 1)
            if got and math.hypot(got[0] - start[0], got[1] - start[1]) <= 4:
                break
        b.cmd("+forward")
        for _ in range(80):
            got = org_of(status(srv), "mp_client", "slot", eb - 1)
            if got and got[1] - start[1] >= 4.0:
                break
        b.cmd("+flatcrouch")
        seen_most = [0.0, 0.0, 0.0, 0.0]  # (a miss's note: the most of A's copy's slide, B's own, his travel; how far he went)
        prev_up, low_since = None, None
        for _ in range(150):
            la, lb = reports(s, [a, b])
            adsl = by_ent(lb, "mp_adspose_own").get(eb)
            own, copy = body_of(lb, "mp_body_own").get(eb), body_of(la, "mp_body").get(eb)
            seen_most = [max(seen_most[0], copy.get("sliding", [0])[0] if copy else 0), max(seen_most[1], own.get("sliding", [0])[0] if own else 0),
                         max(seen_most[2], adsl["ads"][0] if adsl else 0), max(seen_most[3], copy["pelvis"][1] - start[1] if copy else 0)]
            b_on = adsl and own and own.get("sliding", [0])[0] >= SLIDE_SHARE and abs(adsl["look"][0] - pitch) < 1.0 and \
                adsl["ads"][0] >= 0.99
            # (adspose3: B down in his slide -- his eye's height over his origin still over two reports: the crouch's ease
            # gone -- and A's copy posed from his avatar after that: taken at once, the copy still came down, 6 over)
            low = b_on and prev_up is not None and abs(adsl["eyeup"][0] - prev_up) < 0.05
            low_since = (low_since if low_since is not None else adsl["stime"][0]) if low else None
            prev_up = adsl["eyeup"][0] if b_on else None
            if low and copy and "sliding" in copy and copy["sliding"][0] >= SLIDE_SHARE and \
                    copy.get("posedat", [0])[0] >= low_since + ADS_POSED_AFTER:
                return adsl, own, copy
        s.note("slide missed: A's copy sliding %.2f at most, B's own %.2f, on the sights %.2f, %.0f units north" % tuple(seen_most))
        return None

    def shots(name, copy, lead):
        """A at each camera spot off B's copy (`lead`: units north he moves meanwhile), aimed at its head; a shot each."""
        aim = [copy["headeye"][0], copy["headeye"][1] + lead, copy["headeye"][2] - 8.0]
        for cam, off, yaw in ADS_CAMS:
            at = (copy["pelvis"][0] + off[0], copy["pelvis"][1] + off[1] + lead, OPEN_SPOT[2])
            down = math.degrees(math.atan2(at[2] + ADS_VIEW_HEIGHT - aim[2], math.hypot(aim[0] - at[0], aim[1] - at[1])))
            ads_cam(srv, a, ea, at, down, yaw)
            a.screenshot("%s_%s" % (name, cam))

    wids = [int(w) for w in os.environ["MP_ADSPOSE_WIDS"].split(",")] if os.environ.get("MP_ADSPOSE_WIDS") else ADS_WIDS
    for wid in wids:
        srv.cmd("vr_test_give_wid %d" % max(wid, 1))
        b.cmd("%s; vr_flat_aim 1" % ("impulse 149" if wid else "impulse 2"))
        sighted = True
        for posture in [p for p in ADS_POSTURES if p != "slide" or os.environ.get("MP_ADSPOSE_SLIDE", "1") == "1"]:
            for look, pitch in ADS_LOOKS:
                first = posture == ADS_POSTURES[0] and look == ADS_LOOKS[0][0]
                if posture == "slide":
                    got = slide_at(pitch)
                    slides[0 if got else 1] += 1
                    if not got:
                        b.cmd("-forward; -flatcrouch")
                        s.note("wid %d %s slide %s: no slide on the sights seen" % (wid, last_model, look))
                        continue
                    adsl, own, copy = got
                    model = last_model
                else:
                    b.cmd("-forward; %s; setpos %.1f %.1f %.1f %.1f 90 0; noclip" % (
                        "+flatcrouch" if posture == "crouch" else "-flatcrouch", OPEN_SPOT[0], OPEN_SPOT[1], OPEN_SPOT[2], pitch))
                    got = settle(pitch, posture == "crouch", first)
                    if not got or got[0]["sight"][0] == 0:
                        sighted = False
                        break  # (no sight line on it: the next weapon)
                    adsl, own, copy, model = got
                    last_model = model
                    if first:
                        stand_up = adsl["eyeup"][0]
                n = adspose_numbers(own, adsl, copy, pitch)
                rows.append((wid, model, posture, look, n))
                s.note("wid %d %s %s %s: eye->copy's drawn eyes %.1f (eye above them %.1f), rear sight->drawn eyes %.1f, "
                       "reach %.1f %.1f, elbows over the eyes %.1f %.1f, arms over the line before the face %.1f %.1f" % (
                           wid, model, posture, look, n["copy_eye"], n["head_up"], n["rear_eye"], n["reach0"], n["reach1"],
                           n["elbow0"], n["elbow1"], n["face0"], n["face1"]))
                if posture != "slide":
                    worst["copy_eye"] = max(worst["copy_eye"], n["copy_eye"])
                    worst["rear_eye"] = max(worst["rear_eye"], n["rear_eye"])
                else:  # (moving, the copy is the interpolation's delay behind along the way: its eyes' height judged)
                    worst["slide_up"] = max(worst["slide_up"], abs(n["head_up"]))
                worst["elbow"] = max(worst["elbow"], n["elbow0"], n["elbow1"])
                worst["face"] = max(worst["face"], n["face0"], n["face1"])
                shots("ads_w%02d_%s_%s" % (wid, posture, look), copy, 10.0 if posture == "slide" else 0.0)
                if posture == "slide":
                    b.cmd("-forward; -flatcrouch")
            if not sighted:
                break
    b.cmd("vr_flat_aim 0; -flatcrouch; -forward")
    s.metric("ads_rows", len(rows))
    s.metric("ads_copy_eye_worst", worst["copy_eye"])
    s.metric("ads_elbow_worst", worst["elbow"])
    s.metric("ads_face_worst", worst["face"])
    s.check("weapons with a sight line measured (at least 8 sighted poses)", len(rows) >= 8, "%d" % len(rows))
    if os.environ.get("MP_ADSPOSE_SLIDE", "1") == "1":
        s.check("slides on the sights seen (every weapon's three looks)", slides[1] == 0, "%d seen, %d missed" % tuple(slides))
        s.check("A's copy of sighted sliding B: its drawn eyes within %g of B's eye's height" % ADS_HEAD_EYE,
                worst["slide_up"] <= ADS_HEAD_EYE, "worst %.2f" % worst["slide_up"])
    s.check("A's copy of sighted B: its drawn eyes within %g of B's eye (standing, crouched)" % ADS_HEAD_EYE,
            worst["copy_eye"] <= ADS_HEAD_EYE, "worst %.2f" % worst["copy_eye"])
    s.check("A's copy of sighted B: both elbows under the gun (below the drawn eyes along the look's up) at every look",
            worst["elbow"] < 0.0, "worst %.2f above" % worst["elbow"])
    s.check("A's copy of sighted B: no arm above the sight line in front of the face at any look", worst["face"] <= 0.0,
            "worst %.2f above" % worst["face"])
    # (the rear sight's distance is B's own relief, not the copy's: the laser cannon's is 24 to 29; noted, not judged)
    s.note("the rear sight off the copy's drawn eyes: worst %.2f" % worst["rear_eye"])


def dolls_of(lines):
    """"mp_doll:" lines -> {player ent: fields (avatar_fields) with "model" and "dollmodel"}."""
    out = {}
    for l in lines:
        if l.startswith("mp_doll:"):
            f = avatar_fields(l)
            m = re.search(r"model=(\S+)", l)
            f["model"] = m.group(1) if m else ""
            m = re.search(r"dollmodel=(\S+)", l)
            f["dollmodel"] = m.group(1) if m else ""
            out[int(f["ent"][0])] = f
    return out


DOLL_PARTS = 19  # avatardoll: the avatar's own ragdoll's parts (quakevr/rigs/vrdoll*.rig2.txt, Misc/quakevr/make_vrdoll.py)
DOLL_LEG = 22.06  # ... its left leg's rest length, hip to knee to ankle (model units)
DOLL_SHORT, DOLL_TALL = 1.55, 1.95  # the two players' eye heights (m) in mp_deathbody
DOLL_ARM_A = 1.2  # backlog1122c: A's vr_body_arm_length (B keeps 1): his ragdoll's arms that much longer than the model's


def doll_rigs_of(lines):
    """"mp_doll_rig:" lines -> {player ent: (doll, parts, dollscale, leg, arm, restarm)}."""
    out = {}
    for l in lines:
        m = re.match(r"mp_doll_rig: ent=(\d+) doll=(\d+) parts=(\d+) dollscale=(\S+) leg=(\S+)(?: arm=(\S+) restarm=(\S+))?", l.strip())
        if m:
            out[int(m.group(1))] = (int(m.group(2)), int(m.group(3)), float(m.group(4)), float(m.group(5)),
                                    float(m.group(6) or 0), float(m.group(7) or 0))
    return out


def doll_model_of(body_model):
    """His avatar's model (progs/vrbody_brawny_bare.mdl) -> his dead body's model on the server (progs/vrdoll_brawny.mdl)."""
    return body_model.replace("_bare", "").replace("vrbody", "vrdoll")


def deathbody_case(s, srv, a, b, ea, eb, victim, ev, own_models, tone, label):
    """`victim` (entity `ev`) killed whole (vr_hurt 110: health -10, not gibbed: his ragdoll body, vr_deathdoll.qc):
    A and B each draw his body as his own body (mp_doll: his build's model in his look), not id's ranger. avatardoll: the
    body the server made is his avatar's own ragdoll (his build's progs/vrdoll*.mdl, its 19 parts, the avatar rig's
    proportions at his size), sent to B (ragsync) as the host has it."""
    since = srv.mark()
    srv.cmd("vr_hurt 110 %d" % (ev - 1))
    dead = wait_quietly(srv, r"vr_hurt: .* dead", 10, since, "%s dead on the server" % label)
    s.check("%s killed whole (vr_hurt 110)" % label, dead is not None, dead[1].strip() if dead else "no vr_hurt line")
    seen, rigs, n = {}, {}, 0
    for n in range(1, 41):
        la, lb = reports(s, [a, b])
        for who, lines in (("A", la), ("B", lb)):
            d = dolls_of(lines).get(ev)
            if d and who not in seen:
                seen[who] = d
            r = doll_rigs_of(lines).get(ev)
            if r and r[1] > 0 and who not in rigs:
                rigs[who] = r
        if len(seen) == 2 and len(rigs) == 2:
            break
    for who in ("A", "B"):
        d, r = seen.get(who), rigs.get(who)
        want = doll_model_of(own_models[ev])
        s.check("%s: %s's body on the server is %s's avatar's own (%s), a ragdoll of its %d parts at %s" % (
                label, label, label, want, DOLL_PARTS, who), d is not None and r is not None and
                d["dollmodel"].split("#")[0] == want and r[1] == DOLL_PARTS,
                "doll %s, %s parts, ragdoll %s" % (d["dollmodel"] if d else "-", r[1] if r else "no", d.get("ragdoll") if d else "-"))
        if r:
            s.check("%s: its parts follow the avatar rig at its size at %s (the leg %.2f = %.2f x %.3f)" % (
                    label, who, r[3], DOLL_LEG, r[2]), r[2] > 0.0 and abs(r[3] / r[2] - DOLL_LEG) < 0.5,
                    "leg %.2f, scale %.3f: %.2f model units" % (r[3], r[2], r[3] / max(r[2], 1e-6)))
    # (backlog1122c: the ragdoll in his body's proportions, his arms as his avatar's: A's vr_body_arm_length, B's 1)
    want_arm = DOLL_ARM_A if ev == ea else 1.0
    for who in ("A", "B"):
        r = rigs.get(who)
        if r:
            s.check("%s: its arm %s's arm length at %s (%.2f over the model's %.2f = %.3f, want %.2f)" % (
                    label, label, who, r[4], r[5], r[4] / max(r[5], 1e-6), want_arm),
                    r[5] > 0.0 and abs(r[4] / r[5] - want_arm) < 0.03)
    if "A" in rigs and "B" in rigs:
        s.check("%s: B's ragdoll the host's size (ragsync: scale %.3f, %.3f)" % (label, rigs["A"][2], rigs["B"][2]),
                abs(rigs["A"][2] - rigs["B"][2]) < 0.01)
        s.check("%s: B's ragdoll the host's proportions (arm %.2f, %.2f)" % (label, rigs["A"][4], rigs["B"][4]),
                abs(rigs["A"][4] - rigs["B"][4]) < 0.15)
    for who in ("A", "B"):
        if who in rigs:
            seen[who]["rig"] = rigs[who]
    for who in ("A", "B"):
        d = seen.get(who)
        viewer = "%s (his own)" % who if (who == "A") == (ev == ea) else who
        s.check("%s: %s draws %s's dead body as %s's own body (his build's model %s)" % (label, viewer, label, label,
                own_models[ev]), d is not None and d["model"] == own_models[ev],
                "%s; the doll %s, ragdoll %s, look %s tone %s gloves %s hands %s, after %d reports" % (
                    d["model"], d["dollmodel"], d.get("ragdoll"), d.get("look"), d.get("tone"), d.get("gloves"),
                    d.get("hands"), n) if d else "no mp_doll line after %d reports" % n)
        if d:
            s.check("%s: %s's copy wears %s's skin tone (%.2f) and gloves" % (label, viewer, label, tone),
                    abs(d["tone"][0] - tone) < 0.02 and d["gloves"][0] == 2 and d["hands"] == [1.0, 1.0],
                    "tone %.2f gloves %d hands %s" % (d["tone"][0], d["gloves"][0], d["hands"]))
            gap = dist(d["pelvis"], d["dollat"])
            s.check("%s: %s's copy lies at the doll (its pelvis within 40 units of the doll's origin)" % (label, viewer),
                    gap <= 40.0, "%.1f units" % gap)
    return seen


def deathbody(s):
    """mpslide (2026-11-20): his words, "When a player is killed, their ragdoll is the default Quake ranger model, not
    their own customized player model". A listen host (VR mock, the brawny build, a dark skin tone, full gloves) and a
    flat client B (the slim build, a pale tone, full gloves). A dies: B (a client of another server: the doll's death
    animation) and A himself (the host: its ragdoll) draw A's dead body as A's own body; A respawns, then B dies: A and
    B himself draw B's. Each doll's copy: the dead player's build's model, his skin tone and gloves, his jointed hands,
    lying where the doll is."""
    srv, a, b, ea, eb = join(s, listen=True, flat="B")
    # (avatardoll: a short A and a tall B, their bodies' ragdolls sized so; backlog1122c: their arms unmeasured, untweaked:
    # the model's times vr_body_arm_length, A's longer)
    arms = "vr_bodycal_upper_arm 0; vr_bodycal_forearm 0; vr_body_tweak_upper_arm 0; vr_body_tweak_forearm 0"
    a.cmd("vr_body_build 2; vr_skin_tone -0.5; vr_gloves 2; vr_height_calibration %.2f; vr_body_arm_length %g; %s" % (
        DOLL_SHORT, DOLL_ARM_A, arms))
    b.cmd("vr_body_arm_length 1; %s" % arms)
    # (B plays flat: his body is his flat body height's, backlog1122b)
    b.cmd("vr_body_build 0; vr_skin_tone 0.6; vr_gloves 2; vr_height_calibration %.2f; vr_flat_body_height %.2f" % (
        DOLL_TALL, DOLL_TALL))

    def dressed(lines):
        la, lb = lines
        oa, ob = body_of(la, "mp_body_own").get(ea), body_of(lb, "mp_body_own").get(eb)
        ca, cb = body_of(lb, "mp_body").get(ea), body_of(la, "mp_body").get(eb)
        return oa and ob and ca and cb and ca["model"] == oa["model"] and cb["model"] == ob["model"] and \
            oa["model"] != ob["model"]

    (la, lb), n = reports_until(s, [a, b], dressed, "each draws the other in his own build", 40)
    models = {ea: body_of(la, "mp_body_own")[ea]["model"], eb: body_of(lb, "mp_body_own")[eb]["model"]}
    s.note("builds: A %s, B %s (after %d reports)" % (models[ea], models[eb], n))
    seen_a = deathbody_case(s, srv, a, b, ea, eb, a, ea, models, -0.5, "A")
    s.check("A respawned (his trigger)", respawn_if_dead(srv, a, ea))
    seen_b = deathbody_case(s, srv, a, b, ea, eb, b, eb, models, 0.6, "B")
    ra, rb = seen_a.get("A", {}).get("rig"), seen_b.get("A", {}).get("rig")
    want = DOLL_TALL / DOLL_SHORT
    s.check("the tall B's body is the short A's scaled by their heights (%.2f / %.2f = %.3f)" % (DOLL_TALL, DOLL_SHORT, want),
            ra is not None and rb is not None and abs(rb[2] / ra[2] - want) < 0.03 * want,
            "A %.3f, B %.3f: %.3f" % (ra[2], rb[2], rb[2] / ra[2]) if ra and rb else "a ragdoll not seen")
    if ra and rb:
        s.note("legs: short A %.2f, tall B %.2f units" % (ra[3], rb[3]))
    deathbody_respawned(s, srv, a, b, eb, models, seen_b, "B")
    da, db = seen_a.get("A"), seen_b.get("A")
    if da and db and os.environ.get("MP_DEATHBODY_SHOT") == "1":
        # (both bodies lying, seen from above beside them by the host, alive again: a screenshot)
        mid = [(da["dollat"][k] + db["dollat"][k]) / 2.0 for k in range(3)]
        at = (mid[0] - 60.0, mid[1], mid[2] + 4.0)
        ads_cam(srv, a, ea, at, 0.0, 0.0)
        a.cmd("vr_mock_camera 0 2.2 -0.9 62 0")  # (the eyes 2.2 m up and ahead, looking 62 degrees down at them)
        reports(s, [a])  # (a frame drawn from there first)
        s.note("screenshot (A's short brawny body and B's tall lean one, seen by A): %s" % a.screenshot("deathbody_two"))


def dolls_all(lines):
    """"mp_doll:" lines -> [fields] (dolls_of's), every one: a player's bodies may be several (mpbugs4: his old ones)."""
    out = []
    for l in lines:
        if l.startswith("mp_doll:"):
            f = avatar_fields(l)
            m = re.search(r"model=(\S+)", l)
            f["model"] = m.group(1) if m else ""
            out.append(f)
    return out


def deathbody_respawned(s, srv, a, b, ev, own_models, before, label):
    """mpbugs4 (his words: "Killed bot/player ragdolls are still the Quake ranger model"): `label` (entity `ev`, the flat
    client B) respawns; his old body still lies where he died. A (the host: its ragdoll) and B himself (a client: its
    death frame) still draw that body (the doll each drew before) as his own body, his build's model, while his living
    copy is drawn too (a doll slot past the players': vr_mpavatar.cpp assignDollSlots), not id's ranger."""
    since = b.mark()
    for _ in range(30):
        if alive(srv, ev):
            break
        b.cmd("+attack")  # (a flat player's fire button: PlayerDeathThink)
        got = wait_or_none(b, r"mp_event: .* respawned", 0.5, since)
        b.cmd("-attack")
        if got or wait_or_none(b, r"mp_event: .* respawned", 0.5, since):
            break
    s.check("%s respawned (his fire button)" % label, alive(srv, ev))
    seen, n = {}, 0
    for n in range(1, 41):
        la, lb = reports(s, [a, b])
        for who, lines in (("A", la), ("B", lb)):
            old = int(before[who]["doll"][0]) if before.get(who) and "doll" in before[who] else -1
            for d in dolls_all(lines):
                if who not in seen and int(d["ent"][0]) == ev and int(d.get("doll", [0])[0]) == old:
                    seen[who] = d
        if len(seen) == 2:
            break
    for who in ("A", "B"):
        d = seen.get(who)
        viewer = "%s (his own)" % who if who == "B" else who
        s.check("%s respawned: %s still draws %s's old body as %s's own body (his build's model %s), not the ranger" % (
                label, viewer, label, label, own_models[ev]), d is not None and d["model"] == own_models[ev],
                "%s in slot %d, after %d reports" % (d["model"], int(d.get("slot", [0])[0]), n) if d else
                "no mp_doll line for his old body (doll %s) after %d reports" % (
                    int(before[who]["doll"][0]) if before.get(who) and "doll" in before[who] else "?", n))


def respawn_flat(srv, c, ent):
    """A flat client `c` (entity `ent`) dead: his fire button until he respawns (PlayerDeathThink) -> alive."""
    since = c.mark()
    for _ in range(30):
        if alive(srv, ent):
            break
        c.cmd("+attack")
        got = wait_or_none(c, r"mp_event: .* respawned", 0.5, since)
        c.cmd("-attack")
        if got or wait_or_none(c, r"mp_event: .* respawned", 0.5, since):
            break
    return alive(srv, ent)


def deathlook(s):
    """vrnotes1122a (his note dm3_2026-11-22: a dead player's body gained the armour he picked up after his respawn): a
    dead body keeps its look as he died, never his live state again. A VR listen host and a flat client B (yellow
    armour, his plates on). B killed whole: his body (QC vr_deathdoll.qc) wears yellow plates at A and at B himself (the
    server's word on the body's own entity: .vr_pp_* copied at death); B respawns and takes red armour: his living copy
    red at A, his old body still yellow, the same pieces, at both. Then the same with vr_death_view 0 (Quake's body
    queue, CopyToBodyQue: the body made at his respawn)."""
    srv, a, b, ea, eb = join(s, listen=True, flat="B")
    srv.cmd("vr_player_plates 1")
    b.cmd("vr_player_plates 1")
    seen_dolls = set()  # (the bodies of the cases before: a case looks at its own body only)
    for case, view in (("doll", 1), ("bodyque", 0)):
        srv.cmd("vr_death_view %d" % view)
        if not alive(srv, eb):
            s.check("%s: B alive first" % case, respawn_flat(srv, b, eb))
        # (no armour, B at 10 health, then yellow armour: the next blow kills him whole, plates left on him)
        b.cmd("give a 0")
        reports_until(s, [a, b], lambda lines: (plates_of(lines[0], "mp_plates").get(eb) or {}).get("pieces", [0]) == [0.0],
                      "%s: B wears no armour first" % case, 40)
        since = srv.mark()
        srv.cmd("vr_hurt 90 %d" % (eb - 1))
        hurt = wait_quietly(srv, r"vr_hurt: .* health", 10, since, "B hurt")
        s.check("%s: B at 10 health" % case, hurt is not None and " at 10 health" in hurt[1],
                hurt[1].strip() if hurt else "no vr_hurt line")
        b.cmd("give a 150")

        def yellow(lines):
            copy = plates_of(lines[0], "mp_plates").get(eb)
            return copy is not None and copy["type"] == [2.0] and copy["pieces"][0] > 0

        reports_until(s, [a, b], yellow, "%s: B wears yellow plates (A's copy of him)" % case, 40)
        since = srv.mark()
        srv.cmd("vr_hurt 40 %d" % (eb - 1))
        dead = wait_quietly(srv, r"vr_hurt: .* dead", 10, since, "B dead")
        s.check("%s: B killed whole (10 health, 40 damage through yellow armour)" % case, dead is not None,
                dead[1].strip() if dead else "no vr_hurt line")
        if view == 0:
            s.check("%s: B respawned (his fire button: his body copied to the queue)" % case, respawn_flat(srv, b, eb))
        # His body's plates at each viewer: yellow, some pieces (the body's own entity: its "doll" number), the same in
        # two reports running (a piece the killing blow shed has morphed out).
        bodies, prev, n = {}, {}, 0
        for n in range(1, 41):
            la, lb = reports(s, [a, b])
            for who, lines in (("A", la), ("B", lb)):
                for d in dolls_all(lines):
                    if who in bodies or int(d["ent"][0]) != eb or d.get("plates", [0])[0] != 2.0 or \
                            int(d.get("doll", [0])[0]) in seen_dolls:
                        continue
                    p = prev.get(who)
                    if p and p["doll"] == d["doll"] and p["plates"] == d["plates"]:
                        bodies[who] = d
                    prev[who] = d
            if len(bodies) == 2:
                break
        for who in ("A", "B"):
            d = bodies.get(who)
            s.check("%s: %s draws B's dead body in yellow plates (some pieces)" % (case, who),
                    d is not None and d["plates"][1] > 0, "plates %s, doll %s, after %d reports" % (
                        d["plates"], d["doll"], n) if d else "no yellow mp_doll line for B after %d reports" % n)
        seen_dolls.update(int(d["doll"][0]) for d in bodies.values())
        if len(bodies) < 2:
            continue
        if view == 1:
            s.check("%s: B respawned (his fire button)" % case, respawn_flat(srv, b, eb))
        b.cmd("give a 200")
        # B's living copy red at A; every report meanwhile, and a few after, his old body as it was at both.
        changed = []
        red = False
        for n in range(1, 31):
            la, lb = reports(s, [a, b])
            copy = plates_of(la, "mp_plates").get(eb)
            red = red or (copy is not None and copy["type"] == [3.0])
            for who, lines in (("A", la), ("B", lb)):
                for d in dolls_all(lines):
                    if int(d.get("doll", [0])[0]) == int(bodies[who]["doll"][0]) and \
                            d.get("plates") != bodies[who]["plates"]:
                        changed.append("%s: %s at report %d" % (who, d.get("plates"), n))
            if red and n >= 10:
                break
        s.check("%s: B respawned with red armour (A's copy of him red)" % case, red,
                "after %d reports" % n)
        s.check("%s: his old body keeps its plates at A and B (yellow, pieces %d / %d), never his live red ones" % (
                case, bodies["A"]["plates"][1], bodies["B"]["plates"][1]), not changed, "; ".join(changed[:4]))


RAG_BL = re.compile(r"mp_bl: side=(\w+) t=(\S+) n=(\d+) m=(\S+) o=(\S+) a=(\S+) rag=(\d)")
RAG_BP = re.compile(r"mp_bp: side=(\w+) t=(\S+) n=(\d+) b=(\d+) p=(\S+) q=(\S+) src=(\w+)")
RAG_SECONDS = 16  # each side's vr_mp_bodylog: game time (the play below and its settling)


def rag_vec(text):
    return [float(x) for x in text.split(",")]


def rag_quat(a):
    """Quake angles (pitch, yaw, roll; degrees) -> a quaternion (w, x, y, z): one fixed convention for both sides."""
    p, y, r = [math.radians(v) / 2.0 for v in a]
    cy, sy, cp, sp, cr, sr = math.cos(y), math.sin(y), math.cos(p), math.sin(p), math.cos(r), math.sin(r)
    return (cy * cp * cr + sy * sp * sr, cy * cp * sr - sy * sp * cr, cy * sp * cr + sy * cp * sr, sy * cp * cr - cy * sp * sr)


def rag_qang(q1, q2):
    d = abs(sum(x * y for x, y in zip(q1, q2))) / max(1e-9, math.sqrt(sum(x * x for x in q1) * sum(y * y for y in q2)))
    return math.degrees(2.0 * math.acos(min(1.0, d)))


def rag_kind(model, rag):
    m = model.lower()
    if rag:
        return "ragdoll"
    for key, kind in (("h_", "head"), ("gib", "gib"), ("crate", "crate"), ("barrel", "barrel"), ("sword", "weapon"),
                      ("w_", "weapon"), ("g_", "weapon"), ("debris", "chunk"), ("soldier", "monster"), ("grunt", "monster")):
        if key in m.split("/")[-1]:
            return kind
    return m.split("/")[-1]


def rag_series(lines, side):
    """vr_mp_bodylog lines of `side` -> ({ent: [(t, origin, quat, model, rag)]}, {(ent, part): [(t, pos, quat, src)]})."""
    ents, parts = {}, {}
    for l in lines:
        m = RAG_BL.match(l)
        if m and m.group(1) == side:
            ents.setdefault(int(m.group(3)), []).append((float(m.group(2)), rag_vec(m.group(5)), rag_quat(rag_vec(m.group(6))),
                                                          m.group(4), m.group(7) == "1"))
            continue
        m = RAG_BP.match(l)
        if m and m.group(1) == side:
            parts.setdefault((int(m.group(3)), int(m.group(4))), []).append(
                (float(m.group(2)), rag_vec(m.group(5)), tuple(rag_vec(m.group(6))), m.group(7)))
    return ents, parts


def rag_errors(host, client):
    """Each client sample against the host's (the server's, every tick) at its time, interpolated -> [(pos error, turn
    error, moving, client sample)]; moving: the host's moved more than 0.05 units or 0.1 degrees over that tick."""
    out = []
    for key, cs in client.items():
        hs = host.get(key)
        if not hs or len(hs) < 2:
            continue
        times = [h[0] for h in hs]
        j = 0
        for c in cs:
            t = c[0]
            while j + 1 < len(hs) and times[j + 1] < t:
                j += 1
            if j + 1 >= len(hs) or not (times[j] <= t <= times[j + 1]) or times[j + 1] - times[j] > 0.05:
                continue
            h0, h1 = hs[j], hs[j + 1]
            f = (t - h0[0]) / max(1e-9, h1[0] - h0[0])
            p = [x + (y - x) * f for x, y in zip(h0[1], h1[1])]
            q0, q1 = h0[2], h1[2]
            sgn = 1.0 if sum(x * y for x, y in zip(q0, q1)) >= 0 else -1.0
            q = tuple(x + (sgn * y - x) * f for x, y in zip(q0, q1))
            moving = dist(h0[1], h1[1]) > 0.05 or rag_qang(q0, q1) > 0.1
            out.append((dist(p, c[1]), rag_qang(q, c[2]), moving, c))
    return out


def rag_stats(errs):
    if not errs:
        return None
    pe = sorted(e[0] for e in errs)
    re_ = sorted(e[1] for e in errs)
    n = len(pe)
    return {"n": n, "pos_mean": sum(pe) / n, "pos_p95": pe[min(n - 1, int(0.95 * n))], "pos_max": pe[-1],
            "rot_mean": sum(re_) / n, "rot_p95": re_[min(n - 1, int(0.95 * n))], "rot_max": re_[-1]}


def ragsync_phase(s, srv, a, b, eb, on, left):
    """One play with vr_net_ragdolls `on`: a grunt knocked down (an active ragdoll) and killed (a dead one), a crate and
    a sword thrown tumbling, another grunt knocked down and gibbed; both sides log what they have each tick (vr_mp_bodylog:
    A the server's, B what he draws) -> {kind: stats} of B against A, and B's ragdoll bytes a second while it moved and
    once settled."""
    label = "ragdolls%d" % on
    since = srv.mark()
    srv.cmd("vr_net_ragdolls %d; vr_physics_spawn monster_army 200 %d" % (on, left))
    srv.wait(r"vr_physics_spawn: \d+ monster_army at", 20, since, "a grunt ahead (%s)" % label)
    sn0 = snap_status(srv, eb)
    since_a, since_b = a.mark(), b.mark()
    b.cmd("vr_net_bytes reset; vr_mp_bodylog %d 700" % RAG_SECONDS)
    b.wait(r"mp_bodylog: seconds=", 10, since_b)
    # (the play, in counted frames of the logic: the knockdown's fall, the corpse's, the throws' flights and landings)
    steps = ((0.0, "vr_mp_bodylog %d 700" % RAG_SECONDS), (0.5, "vr_knockdown_test 0"), (2.0, "vr_knockdown_test 3"),
             (3.0, "vr_physics_spawn vr_crate 120 %d; vr_physics_spawn weapon_knight_sword 110 %d" % (left - 40, left + 40)),
             (0.15, "vr_physics_fling vr_crate 260 90 260; vr_physics_fling weapon_knight_sword 300 90 300"),
             (0.5, "vr_physics_spawn monster_army 170 %d" % left), (0.5, "vr_knockdown_test 0"), (0.5, "vr_knockdown_test 4"))
    for i, (after, cmd) in enumerate(steps):
        if after > 0:
            # (the server's game time, counted: vr_mp_after)
            mark = srv.mark()
            srv.cmd("vr_mp_after %.2f rag%d_%d" % (after, on, i))
            srv.wait(r"mp_after: .* rag%d_%d" % (on, i), 60, mark, "%s step %d" % (label, i))
        srv.cmd(cmd)
    a.wait(r"mp_bodylog: done", 120, since_a, "A's log (%s)" % label)
    b.wait(r"mp_bodylog: done", 120, since_b, "B's log (%s)" % label)
    sn1 = snap_status(srv, eb)
    # (ragsync2) B's changed records that waited over the play (his rate, the datagram's room), the ragdolls' among them
    waits = {k: int(sn1.get(k, 0)) - int(sn0.get(k, 0)) for k in ("waited", "rwaited", "sent")}
    s.note("%s: B's snapshots %d, changed records that waited %d (ragdolls' %d, the others' %d)" % (
        label, waits["sent"], waits["waited"], waits["rwaited"], waits["waited"] - waits["rwaited"]))
    s.metric("%s_waited_others" % label, waits["waited"] - waits["rwaited"])
    since_bb = b.mark()
    b.cmd("vr_net_bytes")
    _, moved = b.wait(r"mp_netbytes: ", 10, since_bb)
    moved_rag = int(fields(moved)["snap_rag"])
    # Settled: B's ragdoll bytes over two seconds once they have settled. (smallfix: a ragdoll still sliding or rolling
    # after the play's last blast made the one window right after it fail; up to 5 windows of 2 s, the first quiet one
    # counts, how many it took a note: ragdolls that never settle still fail.)
    settled_rag, windows = 0, 0
    for windows in range(1, 6):
        since_bb = b.mark()
        b.cmd("vr_net_bytes reset; echo mp_ragsettle_begin")
        b.wait(r"mp_ragsettle_begin", 10, since_bb)
        mark = srv.mark()
        srv.cmd("vr_mp_after 2 ragsettled%d_%d" % (on, windows))  # (two seconds of the server's game time, counted)
        srv.wait(r"mp_after: .* ragsettled%d_%d" % (on, windows), 60, mark, "two settled seconds")
        since_bb = b.mark()
        b.cmd("vr_net_bytes")
        _, settled = b.wait(r"mp_netbytes: ", 30, since_bb)
        settled_rag = int(fields(settled)["snap_rag"])
        if settled_rag / 2.0 <= 0.02 * moved_rag / RAG_SECONDS + 1:
            break
    s.note("%s: settled after %d window(s) of 2 s" % (label, windows))
    ha, pa = rag_series(a.lines[since_a:], "sv")
    hb, pb = rag_series(b.lines[since_b:], "cl")
    out = {}
    for err in rag_errors(ha, hb):
        c = err[3]
        model, rag = c[3], c[4]
        out.setdefault(("entity", rag_kind(model, rag)), []).append(err)
    for err in rag_errors(pa, pb):
        out.setdefault(("parts", err[3][3]), []).append(err)
    table = {}
    for key, errs in sorted(out.items()):
        mv = [e for e in errs if e[2]]
        st, sm = rag_stats(errs), rag_stats(mv)
        table[key] = (st, sm)
        s.note("%s %s %s: all n=%d pos mean %.2f p95 %.2f max %.2f u, turn mean %.2f p95 %.2f max %.2f deg; moving %s" % (
            label, key[0], key[1], st["n"], st["pos_mean"], st["pos_p95"], st["pos_max"], st["rot_mean"], st["rot_p95"],
            st["rot_max"], ("n=%d pos mean %.2f p95 %.2f max %.2f u, turn mean %.2f p95 %.2f max %.2f deg" % (
                sm["n"], sm["pos_mean"], sm["pos_p95"], sm["pos_max"], sm["rot_mean"], sm["rot_p95"], sm["rot_max"]))
            if sm else "none"))
    secs = RAG_SECONDS
    s.note("%s: B's ragdoll bytes %d over the play (%.0f B/s), %d over 2 s settled" % (label, moved_rag, moved_rag / secs,
                                                                                    settled_rag))
    s.metric("%s_rag_Bps" % label, round(moved_rag / secs))
    s.metric("%s_rag_settled_B" % label, settled_rag)
    return table, moved_rag, settled_rag


def ragsync(s):
    """ragsync (2026-11-21): his words, "Ragdolls don't seem to be synchronized. Especially in coop, they should be,
    for active ragdolls" and "Can you check if props and other physics entities are synchronized?". A VR listen host A and
    a flat client B side by side in vrtesthall; the same play twice (ragsync_phase), the ragdolls' parts out of B's
    snapshot (vr_net_ragdolls 0, as before) and in it: B's drawn things against the server's at the same server time.
    With them: B draws the ragdolls by their parts (not the animation), within RAG_PART_POS units and RAG_PART_TURN
    degrees of the server's while they move (p95); the props, gibs and the thrown sword within RAG_ENT_POS / RAG_ENT_TURN;
    settled ragdolls cost B nothing."""
    srv, a, b, ea, eb = join(s, listen=True, flat="B", level="vrtesthall", srv_first="developer 1",
                             first=lambda srv, a, ea: put_at(srv, a, ea, (-260.0, HALL_START_Y - 60.0, HALL_Z), 90))
    a.cmd("god; notarget")
    b.cmd("god; notarget")
    put_at(srv, b, eb, ARMED_B, 90)
    before, _, _ = ragsync_phase(s, srv, a, b, eb, 0, -70)
    after, moved, settled = ragsync_phase(s, srv, a, b, eb, 1, 70)
    parts = after.get(("parts", "rag"))
    s.check("with vr_net_ragdolls 1 B draws the ragdolls by their parts", parts is not None and parts[0]["n"] > 500,
            "%d part samples drawn by parts; %s by the animation" % (
                parts[0]["n"] if parts else 0, after.get(("parts", "anim"), (None,))[0]["n"] if after.get(("parts", "anim")) else 0))
    if parts and parts[1]:
        sm = parts[1]
        s.check("moving ragdoll parts at B within %.1f units and %.1f degrees of the server's (p95)" % (RAG_PART_POS, RAG_PART_TURN),
                sm["pos_p95"] <= RAG_PART_POS and sm["rot_p95"] <= RAG_PART_TURN,
                "p95 %.2f units, %.2f degrees (max %.2f, %.2f)" % (sm["pos_p95"], sm["rot_p95"], sm["pos_max"], sm["rot_max"]))
    anim = before.get(("parts", "anim"))
    if anim and anim[1] and parts and parts[1]:
        s.note("ragdoll parts while moving, p95: before %.1f units %.1f degrees (the animation), after %.2f units %.2f degrees" % (
            anim[1]["pos_p95"], anim[1]["rot_p95"], parts[1]["pos_p95"], parts[1]["rot_p95"]))
    for key, (st, sm) in sorted(after.items()):
        # (the props thrown: the crate and the sword; gibs, heads, the dropped weapons and backpacks are notes: the
        # small gibs' cosmetic budget and the rate's waits are B12c's, as without ragdolls)
        if key[0] != "entity" or not sm or key[1] not in ("crate", "weapon"):
            continue
        s.check("%s at B within %.1f units and %.1f degrees of the server's while it moves (p95)" % (key[1], RAG_ENT_POS, RAG_ENT_TURN),
                sm["pos_p95"] <= RAG_ENT_POS and sm["rot_p95"] <= RAG_ENT_TURN,
                "p95 %.2f units, %.2f degrees over %d samples" % (sm["pos_p95"], sm["rot_p95"], sm["n"]))
    s.check("B's ragdoll bytes while they moved", moved > 0, "%d B" % moved)
    s.check("settled ragdolls cost B (almost) nothing (under 2%% of the moving play's bytes a second)",
            settled / 2.0 <= 0.02 * moved / (RAG_SECONDS) + 1, "%d B in 2 s settled; %d B in %.0f s of play" % (
                settled, moved, RAG_SECONDS))


RAG_PART_POS, RAG_PART_TURN = 2.0, 3.0  # a moving ragdoll part at B against the server's, p95 (units, degrees)
RAG_ENT_POS, RAG_ENT_TURN = 2.0, 3.0    # a moving prop, gib or weapon


def cosmetics_b6b(s, srv, a, b, ea, eb):
    """MP_PLAN B6b: B's holstered shotgun drawn at A where B drew it, and the server's holsterpos for B where B's
    holster is; B's pauldrons (steel, bigger) on A's copy of his body as on his own; B seated: A's copy posed as his own
    seated body (no leg fit), the joints where B's own has them; B's nailgun's seated magazine in the server's drawn
    weapon hull of it."""
    b.quiet(r"mp_(move|pos): ", 0.5, 20, "both at rest")
    since = srv.mark()
    srv.cmd("developer 1")
    b.cmd("impulse %d" % (90 + HOLSTER_SLOT))
    srv.wait(r"holster give: .* holster %d a shotgun" % HOLSTER_SLOT, 10, since, "B's holster given a shotgun")

    def holstered(lines):
        la, lb, ls = lines
        own, at_a = holsters_of(lb, "mp_holster_own").get((eb, HOLSTER_SLOT)), holsters_of(la, "mp_holster").get(
            (eb, HOLSTER_SLOT))
        return own is not None and at_a is not None and own["model"] == at_a["model"] and \
            dist(own["at"], at_a["at"]) <= HOLSTER_UNITS

    (la, lb, ls), n = reports_until(s, [a, b, srv], holstered, "A draws B's holstered shotgun where B drew it")
    own, at_a = holsters_of(lb, "mp_holster_own")[(eb, HOLSTER_SLOT)], holsters_of(la, "mp_holster")[(eb, HOLSTER_SLOT)]
    s.metric("holster_off", dist(own["at"], at_a["at"]))
    s.check("A draws B's holstered shotgun where B drew it (within %g units)" % HOLSTER_UNITS, True, "%s %.2f off, after %d "
            "reports" % (at_a["model"], dist(own["at"], at_a["at"]), n))
    point, sv = holsters_of(lb, "mp_holster_point_own").get((eb, HOLSTER_SLOT)), holsters_of(ls, "mp_holster_sv").get(
        (eb, HOLSTER_SLOT))
    off = dist(point["at"], sv["at"]) if point and sv else float("inf")
    s.metric("holsterpos_off", off)
    s.check("the server's holsterpos for B is B's holster as he drew it (within %g units)" % HOLSTER_UNITS,
            off <= HOLSTER_UNITS, "%.2f (B's %r, the server's %r)" % (off, point, sv))

    b.cmd("vr_body_pauldrons 1; vr_body_pauldron_style 2; vr_body_pauldron_size 1.3")
    a.cmd("vr_body_pauldrons 1; vr_body_pauldron_style 0; vr_body_pauldron_size 1")

    def pauldrons(lines):
        la, lb = lines
        own, at_a = pauldrons_of(lb, "mp_pauldron_own"), pauldrons_of(la, "mp_pauldron")
        return all(own.get((eb, side)) and at_a.get((eb, side)) and own[(eb, side)]["skin"] == at_a[(eb, side)]["skin"] ==
                   [4.0] and abs(own[(eb, side)]["scale"][0] - at_a[(eb, side)]["scale"][0]) <= 1 for side in (0, 1))

    (la, lb), n = reports_until(s, [a, b], pauldrons, "A draws B's steel pauldrons at his size")
    own, at_a = pauldrons_of(lb, "mp_pauldron_own"), pauldrons_of(la, "mp_pauldron")
    off = max(dist(own[(eb, side)]["at"], at_a[(eb, side)]["at"]) for side in (0, 1))
    s.metric("pauldron_off", off)
    s.check("A draws B's pauldrons (steel, his size) on his body where B's own are (within %g units)" % BODY_UNITS,
            off <= BODY_UNITS, "%.2f off, scale %r (B's %r), after %d reports" % (
                off, at_a[(eb, 0)]["scale"], own[(eb, 0)]["scale"], n))

    b.cmd("vr_bodycal_seated 1")

    def seated(lines):
        la, lb = lines
        copy, own = body_of(la, "mp_body").get(eb), body_of(lb, "mp_body_own").get(eb)
        return copy is not None and own is not None and copy.get("seated") == [1.0] and \
            dist(copy["pelvis"], own["pelvis"]) <= BODY_UNITS

    (la, lb), n = reports_until(s, [a, b], seated, "A's copy of B seated, posed as his own")
    copy, own = body_of(la, "mp_body")[eb], body_of(lb, "mp_body_own")[eb]
    worst = max(dist(copy[k], own[k]) for k in ("pelvis", "headjoint"))  # (the feet's stance is each body's own)
    s.metric("seated_joint_worst", worst)
    s.check("B seated: A's copy of his body posed as his own (pelvis and head within %g units)" % BODY_UNITS,
            worst <= BODY_UNITS, "%.2f, after %d reports" % (worst, n))
    b.cmd("vr_bodycal_seated 0")

    # His nailgun's magazine in the server's hull of his drawn weapon (immersive reloading's magazines shown).
    srv.cmd("vr_reload_mode 3; vr_debug_box3d 1")
    b.cmd("vr_reload_mode 3")
    since = srv.mark()
    grip_then_give(srv, b, "vr_mock_button main grip 1", impulse=150 + WID_NAILGUN)
    got = wait_quietly(srv, r"box3d: main hand's weapon progs/v_nail\.mdl: .* and its magazine", 20, since,
                       "the server's hull of B's nailgun")
    s.check("the server's drawn weapon hull of B's nailgun has its seated magazine", got is not None,
            "no 'box3d: main hand's weapon progs/v_nail.mdl ... and its magazine' in the server's log")
    srv.cmd("vr_debug_box3d 0")


def body_of(lines, prefix):
    """"mp_body:" / "mp_body_own:" lines -> {ent: fields (avatar_fields) with "model"}."""
    out = {}
    for l in lines:
        if l.startswith(prefix + ":"):
            f = avatar_fields(l)
            m = re.search(r"model=(\S+)", l)
            f["model"] = m.group(1) if m else ""
            out[int(f["ent"][0])] = f
    return out


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def face(s, a, b, head_a, head_b):
    """A and B turn to face each other (B steps towards A, the stick let go as he moves off); A holds both hands up."""
    yaw_b = math.degrees(math.atan2(head_a[1] - head_b[1], head_a[0] - head_b[0]))
    since_a, since_b = a.mark(), b.mark()
    a.cmd("vr_mock_hand main 0.2 1.3 -0.35 0 0 0; vr_mock_hand off -0.25 1.45 -0.3 0 0 0; "
          "vr_mock_turn_to %.0f" % (yaw_b + 180.0))
    b.cmd("vr_mock_turn_to %.0f" % yaw_b)
    a.wait(r"vr_mock_turn_to: facing", 20, since_a, "A turned to face B")
    b.wait(r"vr_mock_turn_to: facing", 20, since_b, "B turned to face A")
    a.cmd("vr_mock_turn_to off")
    since_b = b.mark()
    b.cmd("vr_mock_turn_to off; vr_mock_stick off 0 1")
    b.wait(r"mp_move: .*self=1", 20, since_b, "B stepping towards A")
    b.cmd("vr_mock_stick off 0 0")
    b.wait(r"mp_stop: .*self=1", 20, since_b, "B stopping")


def body(s):
    """B draws A's body, posed from A's avatar by the code that poses A's own (vr_avatar.cpp poseOther): A's build's
    model, its arms reaching A's drawn hands, its joints where A's own body has them, its scale A's (his calibration).
    A raises his Height: B's copy grows as A's own does. A screenshot pair: B facing A before and after."""
    srv, a, b, ea, eb = join(s)
    b.quiet(r"mp_(move|pos): ", 0.5, 20, "both at rest")
    a.cmd("vr_body_build 2")  # (the brawny build: B must draw A's, not his own athletic one)
    la, lb = reports(s, [a, b])
    head_a = (by_ent(lb, "mp_avatar").get(ea) or {}).get("head")
    head_b = (by_ent(la, "mp_avatar").get(eb) or {}).get("head")
    s.check("each has the other's avatar", head_a is not None and head_b is not None)
    if head_a is None or head_b is None:
        return
    face(s, a, b, head_a, head_b)

    def settled(lines):
        la, lb = lines
        own, copy = body_of(la, "mp_body_own").get(ea), body_of(lb, "mp_body").get(ea)
        return own is not None and copy is not None and "brawny" in own["model"] and copy["model"] == own["model"] and \
            abs(copy["m2w"][0] / own["m2w"][0] - 1.0) <= BODY_SCALE and dist(copy["pelvis"], own["pelvis"]) <= BODY_UNITS

    (la, lb), n = reports_until(s, [a, b], settled, "B draws A's body (his build) where A's own is", 30)
    own, copy = body_of(la, "mp_body_own")[ea], body_of(lb, "mp_body")[ea]
    s.note("A: " + next(l.strip() for l in la if l.startswith("mp_body_own:")))
    s.note("B: " + next(l.strip() for l in lb if l.startswith("mp_body:")))
    s.check("B draws A's body as A's build's model", copy["model"] == own["model"], "%s (A's own %s), after %d reports"
            % (copy["model"], own["model"], n))
    reach = max(dist(copy["wrist%d" % h], copy["handwrist%d" % h]) for h in (0, 1))
    s.metric("body_wrist_to_hand", reach)
    s.check("B's copy's wrists at A's drawn hands (within %g units)" % BODY_WRIST, reach <= BODY_WRIST, "%.2f" % reach)
    worst, which = 0.0, ""
    for k in ("headjoint", "pelvis", "shoulder0", "shoulder1", "wrist0", "wrist1"):
        d = dist(copy[k], own[k])
        if d > worst:
            worst, which = d, k
    s.metric("body_joint_worst", worst)
    s.check("B's copy's head, pelvis, shoulders and wrists within %g units of A's own" % BODY_UNITS, worst <= BODY_UNITS,
            "worst %.2f (%s)" % (worst, which))
    feet = max(dist(copy["ankle%d" % f], own["ankle%d" % f]) for f in (0, 1))
    s.metric("body_ankle_worst", feet)
    s.note("ankles: B's copy %.2f units from A's own (their stance is each body's own)" % feet)
    s.note("screenshot: %s" % b.screenshot("sees_A_body"))

    # A raises his Height: his body is scaled to it, and B's copy (his calibration, sent on change) with it.
    before_m2w, before_sh, before_eye = copy["m2w"][0], dist(copy["shoulder0"], copy["shoulder1"]), copy["eyeheight"][0]
    height = 1.95
    a.cmd("vr_height_calibration %g" % height)

    def grown(lines):
        la, lb = lines
        own, copy = body_of(la, "mp_body_own").get(ea), body_of(lb, "mp_body").get(ea)
        return own is not None and copy is not None and own["m2w"][0] > before_m2w * 1.05 and \
            abs(copy["m2w"][0] / own["m2w"][0] - 1.0) <= BODY_SCALE

    (la, lb), n = reports_until(s, [a, b], grown, "A's Height raised: B's copy grows as A's own", 30)
    own, copy = body_of(la, "mp_body_own")[ea], body_of(lb, "mp_body")[ea]
    ratio = copy["m2w"][0] / before_m2w
    sh = dist(copy["shoulder0"], copy["shoulder1"]) / before_sh
    want = height / before_eye
    s.metric("body_grown_scale", ratio)
    s.metric("body_grown_shoulders", sh)
    s.check("A's Height raised: B's copy's scale is A's own (within %g)" % BODY_SCALE,
            abs(copy["m2w"][0] / own["m2w"][0] - 1.0) <= BODY_SCALE, "B %.3f, A %.3f, after %d reports"
            % (copy["m2w"][0], own["m2w"][0], n))
    s.check("B's copy taller by the Height's ratio (scale and shoulders, within 2%)",
            abs(ratio / want - 1.0) <= 0.02 and abs(sh / want - 1.0) <= 0.02,
            "scale x%.3f, shoulders x%.3f, the Height x%.3f" % (ratio, sh, want))
    s.note("screenshot: %s" % b.screenshot("sees_A_tall"))

    # A calibrates his arms (Body Calibration's results: arm lengths, shoulders wider, higher, further back, their rise
    # and swing; MP_PLAN B3c): B's copy takes them from his calibration, its shoulders where A's own body has them.
    before_w = dist(own["shoulder0"], own["shoulder1"])
    a.cmd("vr_bodycal_upper_arm 31; vr_bodycal_forearm 28; vr_bodycal_shoulders_back 0.02; vr_bodycal_shoulders_up 0.03; "
          "vr_bodycal_shoulders_out 0.05; vr_bodycal_shoulder_rise 30; vr_bodycal_shoulder_swing 15")

    def calibrated(lines):
        la, lb = lines
        own, copy = body_of(la, "mp_body_own").get(ea), body_of(lb, "mp_body").get(ea)
        return own is not None and copy is not None and dist(own["shoulder0"], own["shoulder1"]) > before_w + 1.0 and \
            max(dist(copy["shoulder%d" % h], own["shoulder%d" % h]) for h in (0, 1)) <= BODY_SHOULDER_CAL

    (la, lb), n = reports_until(s, [a, b], calibrated, "A's arms calibrated: B's copy's shoulders A's own", 30)
    own, copy = body_of(la, "mp_body_own")[ea], body_of(lb, "mp_body")[ea]
    off = max(dist(copy["shoulder%d" % h], own["shoulder%d" % h]) for h in (0, 1))
    s.metric("body_cal_shoulders", off)
    s.check("A's arms calibrated (shoulders %.1f units wider): B's copy's shoulders within %g units of A's own"
            % (dist(own["shoulder0"], own["shoulder1"]) - before_w, BODY_SHOULDER_CAL), off <= BODY_SHOULDER_CAL,
            "%.2f after %d reports" % (off, n))




def prefs_report(s, srv):
    """vr_mp_prefs on the server -> {ent: {"has": 0|1, "same": n, <pref>: value...}} (its "mp_prefs_sv:" lines)."""
    since = srv.mark()
    srv.cmd("vr_mp_prefs; echo mp_prefs_done")
    n, _ = srv.wait(r"mp_prefs_done", 10, since, "the server's prefs")
    out = {}
    for l in srv.lines[since:n]:
        if l.startswith("mp_prefs_sv:"):
            f = dict(m.groups() for m in re.finditer(r"(\w+)=(-?[\d.e+-]+)", l))
            out[int(f.pop("ent"))] = {k: float(v) for k, v in f.items()}
    return out


def weapons_held(s, srv, ea, eb, keeper, dropper, label, need=5, tries=30):
    """The server's main-hand weapons of A and B (its avatars from their moves), reported until `keeper` has held the
    shotgun (4) and `dropper` held none for `need` reports in a row (a definite end: at most `tries` reports)."""
    run_ok, seen = 0, []
    for _ in range(tries):
        (lines,) = reports(s, [srv])
        sv = by_ent(lines, "mp_avatar_sv")
        k = int(sv[keeper]["wpn"][0]) if keeper in sv else -1
        d = int(sv[dropper]["wpn"][0]) if dropper in sv else -1
        seen.append((k, d))
        run_ok = run_ok + 1 if (k == 4 and d == 0) else 0
        if run_ok >= need:
            break
    name = {ea: "A", eb: "B"}
    s.check("%s: %s keeps the given shotgun (his Sticky grip), %s lets it fall (his Hold), for %d reports" % (
        label, name[keeper], name[dropper], need), run_ok >= need,
        "the server's main-hand weapons (%s, %s) by report: %s" % (name[keeper], name[dropper], seen[-12:]))


def prefs_qc(srv, self_ent, other_ent, name):
    """The server's QC read of preference `name` (cvar_hget) with self and other those entities -> its value."""
    since = srv.mark()
    srv.cmd("vr_mp_prefs_qc %d %d %s" % (self_ent, other_ent, name))
    _, l = srv.wait(r"^mp_prefs_qc: self=%d other=%d " % (self_ent, other_ent), 10, since, "the server's QC read")
    return float(fields(l)["value"])


def prefs_qc_checks(s, srv, ea, eb):
    """(Review 2026-11-18, finding 13) cvar_hget's player: self when he is one, else other. A (Sticky, 1) and B (the
    server's Hold, 0) have theirs; an empty slot's player entity has none (as a bot, or a player whose have not come):
    with self that one and other A, the server's own -- A's no longer (self's fall-through to other)."""
    empty = max(e for e in range(1, 5) if e not in (ea, eb))
    got = {
        "self A, other B": prefs_qc(srv, ea, eb, "vr_weapon_grip_mode"),
        "self B, other A": prefs_qc(srv, eb, ea, "vr_weapon_grip_mode"),
        "self world, other A (a touch: self the item)": prefs_qc(srv, 0, ea, "vr_weapon_grip_mode"),
        "self a player without preferences (%d), other A" % empty: prefs_qc(srv, empty, ea, "vr_weapon_grip_mode"),
    }
    want = [1.0, 0.0, 1.0, 0.0]
    s.check("cvar_hget's player: self's own when a player (a player without preferences: the server's, never other's), "
            "else other's", list(got.values()) == want, "%s (want %s)" % (got, want))


def prefs(s):
    """Per-player preferences (vr_clientprefs.cpp): each client's go up and the server judges each player by his own.
    A picks the Sticky weapon grip (vr_weapon_grip_mode 1), B keeps Hold (0), the dedicated server's own is Hold: both
    are given a shotgun with their grips let go; A keeps it, B's falls. Then the other way round: A back to Hold drops
    his, B on Sticky keeps a new one."""
    srv, a, b, ea, eb = join(s)
    a.cmd("vr_weapon_grip_mode 1")

    def arrived(want_a):
        for _ in range(20):
            got = prepped = prefs_report(s, srv)
            if got.get(ea, {}).get("has") == 1 and got.get(eb, {}).get("has") == 1 and \
                    got[ea].get("vr_weapon_grip_mode", 0.0) == want_a and \
                    got[eb].get("vr_weapon_grip_mode", 0.0) == 1.0 - want_a:
                return prepped
        return got

    got = arrived(1.0)
    s.check("the server has both players' preferences, A's Sticky grip his own, B's the defaults",
            got.get(ea, {}).get("vr_weapon_grip_mode") == 1.0 and got.get(eb, {}).get("has") == 1 and
            "vr_weapon_grip_mode" not in got.get(eb, {}), "server: %s" % got)
    prefs_qc_checks(s, srv, ea, eb)
    since = a.mark()
    a.cmd("vr_mp_prefs; echo mp_prefs_done")
    n, _ = a.wait(r"mp_prefs_done", 10, since, "A's prefs")
    own = [l.strip() for l in a.lines[since:n] if l.startswith("mp_prefs_own:")]
    s.note("A: %s" % own)
    s.check("A's own report: sent, Sticky grip", bool(own) and "sent=1" in own[0] and "vr_weapon_grip_mode=1" in own[0],
            "%s" % own)

    give = "vr_mock_button main grip 0; vr_mock_button off grip 0; impulse 154"
    a.cmd(give)
    b.cmd(give)
    weapons_held(s, srv, ea, eb, ea, eb, "A Sticky, B Hold")

    # The other way round: A's change goes up (his shotgun falls: Hold, his grip let go), B's too (a new give kept).
    a.cmd("vr_weapon_grip_mode 0")
    b.cmd("vr_weapon_grip_mode 1")
    got = arrived(0.0)
    s.check("the server has both players' changed preferences (A Hold, B Sticky)",
            "vr_weapon_grip_mode" not in got.get(ea, {"vr_weapon_grip_mode": 1}) and
            got.get(eb, {}).get("vr_weapon_grip_mode") == 1.0, "server: %s" % got)
    b.cmd(give)
    weapons_held(s, srv, ea, eb, eb, ea, "A Hold, B Sticky")


def own_prefs(inst):
    """vr_mp_prefs on a client -> its "mp_prefs_own:" fields."""
    since = inst.mark()
    inst.cmd("vr_mp_prefs; echo mp_prefs_done")
    n, _ = inst.wait(r"mp_prefs_done", 10, since, "%s's prefs" % inst.name)
    for l in inst.lines[since:n]:
        if l.startswith("mp_prefs_own:"):
            return {k: float(v) for k, v in re.findall(r"(\w+)=(-?[\d.e+-]+)", l)}
    return {}


GRIP_REPRESS_S = 0.25  # game seconds after a let-go before the harness presses a grip again (the QC's bounce: 0.125)


def let_go_still(srv, inst, hand):
    """`inst` lets go of `hand`'s grip once THROW_SETTLE_S of the server's game time has passed: a drop, not a throw (a
    hand put in place by `vr_mock_hand` moves that fast in one frame: the mock's velocity for it, 21 m/s for 3 frames;
    let go within the throw estimate's window, the weapon is thrown down at 10 m/s, bounces and slides away)."""
    server_time_past(srv, server_time(srv) + THROW_SETTLE_S)
    inst.cmd("vr_mock_button %s grip 0" % hand)


def grip_then_give(srv, inst, grips, hand="main", impulse=154):
    """`grips` (the mock's grip buttons, its hands) on `inst`, then the test give of a shotgun (impulse 154: 150 + WID 4;
    170 + WID in the off hand) once the server has that hand's grip pressed. The give in the same frame as the press could
    reach the server a move before the grip (the move's grip bit is the client's next frame's): the default grip-to-hold
    let it fall at once (B3's mp_avatar 14/1, A's main hand empty). Leaves the server's grip log on (vr_debug_carry 2,
    developer 1)."""
    srv.cmd("vr_debug_carry 2; developer 1")
    since = srv.mark()
    inst.cmd(grips)
    srv.wait(r"grip: .*%s hand pressed at" % hand, 10, since, "%s's %s grip on the server" % (inst.name, hand))
    inst.cmd("impulse %d" % impulse)


def server_time(srv):
    """The server's game clock (vr_mp_status's "mp_server: ... time=")."""
    since = srv.mark()
    srv.cmd("vr_mp_status")
    _, line = srv.wait(r"mp_server: .*time=", 10, since, "the server's status")
    return float(fields(line)["time"])


def server_time_past(srv, t, tries=200):
    """Asks the server its game clock until it is past `t` (a definite end: at most `tries` asks, each a frame or more)."""
    for _ in range(tries):
        if server_time(srv) > t:
            return
    raise StepFailed("the server's clock never passed %.3f" % t)


FRAME_SUM_SLACK = 0.7  # a palm frame's 13 values each within the step it is sent again at (0.05), and its 3 decimals
RIG_SUM_SLACK = 1.4    # the grip fit's hand rig: its 27 values each within that step


def grab(s):
    """The rig up (vr_clientprefs.cpp "vr_rig"): the dedicated server has each player's fists as his client poses them
    (within the 64ths of a unit they are sent in); B lets a given shotgun fall from his low hand (Hold) and closes his
    hand on it: the server takes it into his hand by his own fist (its count of grabs judged by a remote player's fist)."""
    srv, a, b, ea, eb = join(s)
    own_b = own_a = {}
    got = {}
    def frames_ok(sv, own):
        return all(own.get("frame%d" % h) == 1 and sv.get("frame%d" % h) == 1 and
                   abs(sv.get("framesum%d" % h, 0) - own.get("framesum%d" % h, 0)) <= FRAME_SUM_SLACK and
                   own.get("rig%d" % h) == 1 and sv.get("rig%d" % h) == 1 and
                   abs(sv.get("rigsum%d" % h, 0) - own.get("rigsum%d" % h, 0)) <= RIG_SUM_SLACK for h in (0, 1))

    for _ in range(20):
        own_a, own_b, got = own_prefs(a), own_prefs(b), prefs_report(s, srv)
        ok = all(got.get(e, {}).get("fist%d" % h, 0) > 0 and
                 got[e]["fist%d" % h] == own["fist%d" % h] and
                 abs(got[e]["fistsum%d" % h] - own["fistsum%d" % h]) <= own["fist%d" % h] * 4 / 128.0 + 0.01
                 for e, own in ((ea, own_a), (eb, own_b)) for h in (0, 1))
        if ok and frames_ok(got.get(ea, {}), own_a) and frames_ok(got.get(eb, {}), own_b):
            break
    for e, own, name in ((ea, own_a, "A"), (eb, own_b, "B")):
        sv = got.get(e, {})
        s.check("the server has %s's palm frames and grip fit hand rigs as his client measures them (B3d, B3f: their "
                "sums within %.2f, %.2f)" % (name, FRAME_SUM_SLACK, RIG_SUM_SLACK), frames_ok(sv, own),
                "server %s; %s's own %s" % ({k: v for k, v in sv.items() if k.startswith(("frame", "rig"))},
                                            name, {k: v for k, v in own.items() if k.startswith(("frame", "rig"))}))
    s.check("the dedicated server poses a hand for the grip fit (its rig's points: the hand's mesh loaded there)",
            got.get(eb, {}).get("rigpts", 0) > 0, "rigpts=%s" % got.get(eb, {}).get("rigpts"))
    for e, own, name in ((ea, own_a, "A"), (eb, own_b, "B")):
        sv = got.get(e, {})
        s.check("the server has %s's fists as his client poses them (spheres, their sum within the 64ths sent)" % name,
                all(sv.get("fist%d" % h, 0) > 0 and sv.get("fist%d" % h) == own.get("fist%d" % h) and
                    abs(sv.get("fistsum%d" % h, 0) - own.get("fistsum%d" % h, 0)) <= own.get("fist%d" % h, 0) * 4 / 128.0 + 0.01
                    for h in (0, 1)),
                "server %s; %s's own %s" % ({k: v for k, v in sv.items() if k.startswith("fist")},
                                            name, {k: v for k, v in own.items() if k.startswith("fist")}))

    # B's main hand low in front of him (the mock's room metres), gripping the shotgun given; he opens it: it falls (Hold).
    def b_weapon_until(want, tries=30):
        seen = []
        for _ in range(tries):
            (lines,) = reports(s, [srv])
            seen.append(int(by_ent(lines, "mp_avatar_sv").get(eb, {}).get("wpn", [-1])[0]))
            if seen[-1] == want:
                break
        return seen

    # (the server's log: "grip: player's main hand let go at <time>", "grab: main hand, thrown_weapon: the fist touches...",
    # "weapon: ... not taken by the main hand at <time>: grip 1, pressed ... s ago, let go ... s ago")
    grip_then_give(srv, b, "vr_mock_hand main 0.1 0.25 -0.35 0 0 0; vr_mock_button main grip 1; vr_mock_button off grip 0")
    held = b_weapon_until(4)
    since = srv.mark()
    let_go_still(srv, b, "main")
    seen = b_weapon_until(0)
    s.check("B's given shotgun in his gripping hand, then falls from it opened", held[-1] == 4 and seen[-1] == 0,
            "B's main weapon by report: %s, then %s" % (held, seen))
    before = prefs_report(s, srv).get(eb, {}).get("fistgrabs", 0)
    # A grip pressed again within 0.125 s of letting go is no grab (vr_handgrabutil.qc: a bounce of the trigger). B3's
    # harness pressed as soon as the server had the gun fall, often within that: that press never took it (its "2 runs of
    # 4"). Each press here comes GRIP_REPRESS_S of the server's game time after the let-go (the server's "let go at").
    # The gun falls by his feet and can knock against his box and slide: pressed while it slid out of the fist, it is let
    # go and pressed again, as a player would (at most 3 presses).
    # A hand's touch is judged as it or the thing moves: a gun come to rest under a still hand is never touched (B3f's
    # mp_grab 12/1: the press after it stopped). Each press is made with the hand where it let go, then lowered 6 cm onto
    # the gun, as a player reaches for it.
    # The gun can still slide as the hand comes down on it (B3f: 1 run of 4 missed all 3 presses): the hand is placed on
    # it again before every report while pressing, 2 cm into it and 4 cm over it by turns (onto_each_report).
    seen, presses = [], 0
    for presses in range(1, 4):
        _, gone = srv.wait(r"grip: .*main hand let go at", 10, since, "B's let-go on the server")
        server_time_past(srv, float(gone.rsplit(" ", 1)[1]) + GRIP_REPRESS_S)
        b.cmd("vr_mock_hand main 0.1 0.25 -0.35 0 0 0; vr_mock_button main grip 1")
        seen += b_weapon_until(4, 2)
        if seen[-1] != 4:
            seen += onto_each_report(b, "vr_mock_hand_to main spot 0", b_weapon_until, 4, 10)
        if seen[-1] == 4:
            break
        since = srv.mark()
        b.cmd("vr_mock_button off grip 0")
    srv.cmd("vr_debug_carry 0; developer 0")
    after = prefs_report(s, srv).get(eb, {}).get("fistgrabs", 0)
    s.metric("b_fist_grabs", after - before)
    s.metric("b_retake_presses", presses)
    s.check("B closes his hand on it: the server judges it by his own fist (it touches the dropped shotgun)",
            after > before, "fist touches %d -> %d" % (before, after))
    # (9 runs of 9 since the server stopped seeing a grip's edge again in frames without a move; a second press in 1)
    s.check("B's grip takes the dropped shotgun back into his hand (at most 3 presses)", seen[-1] == 4,
            "B's main weapon by report while pressing: %s" % seen)


def status(inst):
    """vr_mp_status on `inst` -> its lines from that report."""
    since = inst.mark()
    inst.cmd("vr_mp_status; echo mp_status_done")
    n, _ = inst.wait(r"mp_status_done", 10, since, "%s's status" % inst.name)
    return inst.lines[since:n]


def org_of(lines, prefix, key, value):
    """The org of the `prefix` line ("mp_client", "mp_player") whose `key` is `value` -> [x, y, z] or None."""
    for l in lines:
        if l.startswith(prefix + ":"):
            m = re.search(r"\b%s=(\d+)\b.*org=([-\d.]+) ([-\d.]+) ([-\d.]+)" % key, l)
            if m and int(m.group(1)) == value:
                return [float(m.group(i)) for i in (2, 3, 4)]
    return None


TELEPORT_MIN = 48  # mp_teleport: B's aim lands this far ahead of him at least (units)


def teleport(s):
    """Client-side traces (vr_trace.cpp: worldtrace::move on a remote client traces the client's own map) and a remote
    client's teleport: B, a client of the dedicated server, traces down from his player to the floor his server has him
    on (his origin 24 over it); he aims his off hand ahead and down with +teleport (vr_teleport_enabled 1): his own aim
    lands on the floor (valid); let go, the server moves him there and A sees him there."""
    srv, a, b, ea, eb = join(s)
    lines = status(b)
    own = org_of(lines, "mp_player", "ent", eb)
    tr = [l.strip() for l in lines if l.startswith("mp_trace:")]
    f = dict(re.findall(r"(\w+)=([-\d.]+)", tr[0])) if tr and "none" not in tr[0] else {}
    s.check("B's own trace down (no local server: the client's map) stops on the floor 24 under his origin",
            bool(f) and own is not None and f.get("hosted") == "0" and float(f["frac"]) < 1.0 and
            float(f["normal"]) > 0.99 and abs(float(f["floor"]) - (own[2] - 24.0)) <= 1.0,
            "B at %s; %s" % (own, tr))

    # B aims: the off hand at his chest, raised from steep to level (the mock's pitch up; the hand's aim is the
    # controller's turned by his gun angle) until his aim lands on the floor at least TELEPORT_MIN units ahead.
    # (turned left and right too: from one of the two coop spawn spots straight ahead is a wall 46 units off, the aim
    # never on the floor: 1 run in 4)
    b.cmd("vr_teleport_enabled 1; +teleport")
    got, impact, tried = [], None, []
    aims = [(pitch, yaw) for yaw in (0, 35, -35) for pitch in range(58, 72)]
    for pitch, yaw in aims:
        if impact is not None:
            break
        b.cmd("vr_mock_hand off -0.2 1.2 -0.3 %d %d 0" % (pitch, yaw))
        # (the aim settled: the same report twice in a row, the hand's new pose reached; at most 20 reports)
        last = None
        for _ in range(20):
            got = [l.strip() for l in status(b) if l.startswith("mp_teleport:")]
            if got and "aiming=1" in got[0] and got[0] == last:
                break
            last = got[0] if got else None
        m = re.search(r"valid=1 .*impact=([-\d.]+) ([-\d.]+) ([-\d.]+)", got[0]) if got else None
        here = [float(m.group(i)) for i in (1, 2, 3)] if m else None
        tried.append((pitch, yaw, round(dist(here[:2], own[:2]), 1) if here and own else None))
        if here and own and dist(here[:2], own[:2]) >= TELEPORT_MIN:
            impact = here
    s.check("B's teleport aim (his own trace on his client) lands on a floor %d units ahead or more" % TELEPORT_MIN,
            impact is not None, "pitch, yaw, units ahead: %s; last %s" % (tried, got[0].strip() if got else "-"))
    if impact is None:
        return
    srv.cmd("developer 1")  # (the server's "teleport: asked to ..." / "teleport refused: ..." in its log)
    before = org_of(status(srv), "mp_client", "slot", eb - 1)
    b.cmd("-teleport")
    at = before
    for _ in range(60):
        at = org_of(status(srv), "mp_client", "slot", eb - 1)
        if at and dist(at, impact) <= 2.0:
            break
    srv.cmd("developer 0")
    sent = [l.strip() for l in status(b) if l.startswith("mp_teleport:")]
    s.note("B after the release: %s" % sent)
    s.metric("b_teleport_units", round(dist(before, at), 1) if before and at else -1)
    s.check("the server moves B to where his client aimed (within 2 units)",
            at is not None and before is not None and dist(at, impact) <= 2.0,
            "B from %s to %s; aimed %s" % (before, at, impact))
    seen = None
    for _ in range(40):
        seen = org_of(status(a), "mp_player", "ent", eb)
        if seen and dist(seen, impact) <= 4.0:
            break
    s.check("A sees B where he teleported (within 4 units)", seen is not None and dist(seen, impact) <= 4.0,
            "A sees B at %s; aimed %s" % (seen, impact))
    b.cmd("vr_teleport_enabled 0; vr_mock_hand off")


def drawn_report(inst, prefix):
    """vr_mp_drawn on `inst` -> {(ent, hand): {"drawn", "slot", "help", "carrying", "spots": {i: [type, x, y, z]}}}
    from its `prefix` lines ("mp_drawn_own": the client's own view; "mp_drawn_sv": the server's of each player)."""
    since = inst.mark()
    inst.cmd("vr_mp_drawn; echo mp_drawn_done")
    n, _ = inst.wait(r"mp_drawn_done", 10, since, "%s's held weapons" % inst.name)
    out = {}
    for l in inst.lines[since:n]:
        if l.startswith(prefix + ":"):
            f = dict(re.findall(r"(\w+)=([-\d.,]+)", l))
            spots = {int(k[2:]): [float(x) for x in v.split(",")] for k, v in f.items() if re.match(r"hs\d$", k)}
            out[(int(f["ent"]), int(f["hand"]))] = {k: int(float(f[k])) for k in ("drawn", "slot", "help", "carrying")}
            out[(int(f["ent"]), int(f["hand"]))]["spots"] = spots
    return out


def gun_lines(lines, prefix, ent):
    """`prefix` lines ("mp_gun", "mp_gun_own", "mp_lamp", "mp_lamp_own") of `ent` in a report -> {side or 0: fields};
    "at" and "dir" as 3 floats, the model a string, the rest numbers."""
    out = {}
    for l in lines:
        if not l.startswith(prefix + ":"):
            continue
        f = {}
        for k, v in re.findall(r"(\w+)=((?:[-\d.]+ [-\d.]+ [-\d.]+)|\S+)", l):
            parts = v.split()
            try:
                f[k] = [float(x) for x in parts] if len(parts) == 3 else float(v)
            except ValueError:
                f[k] = v
        if int(f.get("ent", -1)) == ent:
            out[int(f.get("side", 0))] = f
    return out


LOOK_KEYS = ("bits", "flags", "clip", "rv")


def look_same(watch, own):
    return bool(watch) and bool(own) and all(int(watch[k]) == int(own[k]) for k in LOOK_KEYS)


def gun_until(s, a, b, eb, side, want, what, tries=40):
    """Reports from A and B until want(A's mp_gun of B's `side`, B's mp_gun_own of it) -> (A's, B's, reports). A definite
    end: at most `tries` reports, then the check fails."""
    seen = ({}, {})
    for n in range(1, tries + 1):
        la, lb = reports(s, [a, b])
        seen = (gun_lines(la, "mp_gun", eb).get(side, {}), gun_lines(lb, "mp_gun_own", eb).get(side, {}))
        if want(*seen):
            return seen[0], seen[1], n
    s.check(what, False, "not after %d reports: A's %s; B's own %s" % (tries, seen[0], seen[1]))
    raise StepFailed(what)


def press_until(s, b, eb, button, done, what, tries=40):
    """B's main `button` pressed until done(B's own mp_gun_own of his main hand), then let go. (Let go a report before:
    the server sees a new press, an edge; and the revolver ignores B/Y for 0.3 s after it swings: pressed again while
    not done.)"""
    for _ in range(tries):
        b.cmd("vr_mock_button main %s 0" % button)
        reports(s, [b])
        b.cmd("vr_mock_button main %s 1" % button)
        for _ in range(4):
            (lb,) = reports(s, [b])
            if done(gun_lines(lb, "mp_gun_own", eb).get(1, {})):
                b.cmd("vr_mock_button main %s 0" % button)
                return
    b.cmd("vr_mock_button main %s 0" % button)
    s.check(what, False, "B's own gun never changed")
    raise StepFailed(what)


RV_HELD, RV_OPEN = 131072, 4096


def live_chambers(rv):
    return sum(1 for i in range(6) if (int(rv) >> (2 * i)) & 3)


def wpnsync(s, flat=""):
    """MP_PLAN "Held weapons' state" (wpnsync): what B does to the gun in his hand and to his torch reaches A's copy of
    him within a few reports, drawn by the code that draws A's own: the heavy revolver's cylinder swung out and its
    shells thrown out, the super shotgun broken open, the nailgun's magazine dropped, the torch taken and lit (its
    model, its lights, where it points). `flat` "A": A plays flat-screen."""
    srv, a, b, ea, eb = join(s, flat=flat)
    grips = "vr_mock_hand main 0.1 0.25 -0.35 0 0 0; vr_mock_button main grip 1; vr_mock_button off grip 0"

    # The heavy revolver (WID 31, impulse 149 with the server's vr_test_give_wid), loaded, its crane home.
    srv.cmd("vr_test_give_wid 31")
    grip_then_give(srv, b, grips, impulse=149)
    wa, wb, n = gun_until(s, a, b, eb, 1, lambda w, o: look_same(w, o) and int(o["rv"]) & RV_HELD and w.get("parts") == 1,
                          "A draws B's revolver in its parts")
    s.check("A draws B's revolver in its parts, its crane home, its shells in (his look as his own stats)",
            wa["swing"] < 1 and wa["whole"] == 0 and wa["shells"] == live_chambers(wb["rv"]) > 0,
            "A's %s; B's own %s; after %d reports" % (wa, wb, n))
    press_until(s, b, eb, "secondary", lambda o: int(o.get("rv", 0)) & RV_OPEN, "B's revolver swung out (B/Y)")
    wa, wb, n = gun_until(s, a, b, eb, 1, lambda w, o: look_same(w, o) and w.get("swing", 0) >= 90,
                          "A sees B's revolver's cylinder swung out")
    s.metric("revolver_open_reports", n)
    s.check("A sees B's revolver's cylinder swung out (95 deg), its shells still in",
            wa["shells"] == live_chambers(wb["rv"]) > 0, "A's %s; after %d reports" % (wa, n))
    press_until(s, b, eb, "secondary", lambda o: live_chambers(o.get("rv", 0)) == 0, "B's revolver's shells out (B/Y)")
    wa, wb, n = gun_until(s, a, b, eb, 1, lambda w, o: look_same(w, o) and w.get("shells", -1) == 0,
                          "A sees B's revolver emptied")
    s.check("A sees B's revolver's chambers emptied (no shells drawn)", wa["swing"] >= 90, "A's %s; after %d reports" % (wa, n))

    # The super shotgun (WID 5) broken open by B/Y: its barrels down, its loaded chambers.
    b.cmd("impulse 155")
    gun_until(s, a, b, eb, 1, lambda w, o: look_same(w, o) and "shot2" in str(w.get("model")) and w.get("whole") == 1,
              "A draws B's super shotgun whole (shut)")
    press_until(s, b, eb, "secondary", lambda o: int(o.get("flags", 0)) & 32, "B's super shotgun broken open (B/Y)")
    wa, wb, n = gun_until(s, a, b, eb, 1, lambda w, o: look_same(w, o) and w.get("parts") == 1 and w.get("ssg", 0) > 30,
                          "A sees B's super shotgun broken open")
    s.metric("ssg_open_reports", n)
    s.check("A sees B's super shotgun broken open (its barrels down, in its parts)", True, "A's %s; after %d reports" % (wa, n))

    # The nailgun (WID 6): its magazine in (its rounds' frame), then dropped by B/Y.
    b.cmd("impulse 156")
    wa, wb, n = gun_until(s, a, b, eb, 1, lambda w, o: look_same(w, o) and "nail" in str(w.get("model")) and w.get("mag") == 1,
                          "A draws B's nailgun's magazine in")
    s.check("A draws B's nailgun with its magazine in (B's clip %d)" % int(wb["clip"]), int(wb["clip"]) > 0,
            "A's %s; B's own %s" % (wa, wb))
    press_until(s, b, eb, "secondary", lambda o: int(o.get("flags", 0)) & 16, "B's nailgun's magazine dropped (B/Y)")
    wa, wb, n = gun_until(s, a, b, eb, 1, lambda w, o: look_same(w, o) and w.get("mag") == 0,
                          "A sees B's nailgun without its magazine")
    s.metric("mag_out_reports", n)
    s.check("A sees B's nailgun without its magazine", True, "A's %s; after %d reports" % (wa, n))

    # The torch: B takes it in his off hand and lights it; A draws it where B does and lights its beam from it.
    b.cmd("vr_flashlight_give left")
    b.cmd("vr_flashlight_toggle")
    lamp = own = {}
    for n in range(1, 41):
        la, lb = reports(s, [a, b])
        lamp = gun_lines(la, "mp_lamp", eb).get(0, {})
        own = gun_lines(lb, "mp_lamp_own", eb).get(0, {})
        if lamp.get("lit") == 1 and own and int(own["flags"]) & 4 and lamp.get("lights", 0) >= 3:
            break
    s.check("A lights B's torch (its spot, spill and glow lights)", lamp.get("lit") == 1 and lamp.get("lights", 0) >= 3,
            "A's %s; B's own %s" % (lamp, own))
    s.check("A draws B's torch's model", lamp.get("shown") == 1, "A's %s" % lamp)
    if lamp.get("at") and own.get("at"):
        off = dist(lamp["at"], own["at"])
        aim = sum(x * y for x, y in zip(lamp["dir"], own["dir"]))
        s.metric("lamp_off", round(off, 2))
        s.metric("lamp_aim_cos", round(aim, 4))
        s.check("A's copy of B's torch where B's is (within 2 units), pointing his way (cos >= 0.99)",
                off <= 2.0 and aim >= 0.99, "%.2f units off, cos %.4f" % (off, aim))
    b.cmd("vr_flashlight_toggle")
    for n in range(1, 41):
        (la,) = reports(s, [a])
        lamp = gun_lines(la, "mp_lamp", eb).get(0, {})
        if lamp.get("lit") == 0 and lamp.get("lights", 1) == 0:
            break
    s.check("B's torch switched off: A's copy dark (no lights)", lamp.get("lit") == 0 and lamp.get("lights", 1) == 0,
            "A's %s" % lamp)

def weapon_until(s, srv, ent, want, tries=30):
    """The server's main-hand weapon of player `ent` by report until it is `want` (at most `tries` reports)."""
    seen = []
    for _ in range(tries):
        (lines,) = reports(s, [srv])
        seen.append(int(by_ent(lines, "mp_avatar_sv").get(ent, {}).get("wpn", [-1])[0]))
        if seen[-1] == want:
            break
    return seen


def spots_off(own, sv):
    """The farthest the server's hotspots are from the client's own (the same types at the same indices), or None."""
    a, b = own.get("spots", {}), sv.get("spots", {})
    if not a or set(a) != set(b) or any(int(a[i][0]) != int(b[i][0]) for i in a):
        return None
    return max(dist(a[i][1:], b[i][1:]) for i in a)


HOTSPOT_SLACK = 0.5  # units: a held weapon's hotspots on the server from where its holder's view draws them


def held(s):
    """The held weapon's drawn pose up (vr_heldpose.cpp): B holds a given shotgun still in his main hand; the dedicated
    server places its hotspots (QC weaponhotspot) where B's own view draws them; B's off hand takes its grip hotspot:
    the server's two-handed hold of B's off hand (QC weaponanygrip) is his own view's (1: by a hotspot)."""
    srv, a, b, ea, eb = join(s)
    grip_then_give(srv, b, "vr_mock_hand main 0.1 0.25 -0.35 0 0 0; vr_mock_button main grip 1; vr_mock_button off grip 0")
    seen = weapon_until(s, srv, eb, 4)
    s.check("B holds the given shotgun (the server's report)", seen[-1] == 4, "B's main weapon by report: %s" % seen)
    own = sv = {}
    off = None
    for _ in range(20):
        own = drawn_report(b, "mp_drawn_own").get((eb, 1), {})
        sv = drawn_report(srv, "mp_drawn_sv").get((eb, 1), {})
        off = spots_off(own, sv)
        if off is not None and off <= HOTSPOT_SLACK and sv.get("drawn") == 1:
            break
    s.note("B's own: %s; the server's: %s" % (own, sv))
    s.check("B's view draws the shotgun in his main hand with its hotspots", own.get("drawn") == 1 and bool(own.get("spots")),
            "B's own: %s" % own)
    s.check("the server has B's drawn shotgun (its slot, as drawn)", sv.get("drawn") == 1 and sv.get("slot") == own.get("slot"),
            "server: %s; B's own: %s" % (sv, own))
    if off is not None:
        s.metric("hotspot_off", round(off, 3))
    s.check("the server places B's hotspots where his view draws them (within %.1f)" % HOTSPOT_SLACK,
            off is not None and off <= HOTSPOT_SLACK, "farthest off: %s" % off)

    grips = sorted(i for i, v in own.get("spots", {}).items() if int(v[0]) == 1)
    if not grips:
        raise StepFailed("the shotgun has no grip hotspot: %s" % own)
    b.cmd("vr_mock_hand_to off heldspot %d palm; vr_mock_button off grip 1" % grips[0])
    own0 = sv0 = {}
    for _ in range(20):
        own0 = drawn_report(b, "mp_drawn_own").get((eb, 0), {})
        sv0 = drawn_report(srv, "mp_drawn_sv").get((eb, 0), {})
        if own0.get("help", 0) > 0 and sv0.get("help") == own0.get("help"):
            break
    # (1 by a hotspot; 2 anywhere on it: vr_weapon_grab_anywhere, on by default, takes a hotspot's hold as anywhere's)
    s.check("B's off hand holds the shotgun at its grip hotspot (his view: two-handed, 1 or 2)", own0.get("help", 0) > 0,
            "B's own off hand: %s" % own0)
    s.check("the server's two-handed hold of B's off hand is his view's (weaponanygrip)",
            sv0.get("help", 0) > 0 and sv0.get("help") == own0.get("help"), "server: %s; B's own: %s" % (sv0, own0))

    # The hand-off (QC VRTryHandOff, by the server's weaponanygrip of B's off hand): B lets go with his main hand; the
    # shotgun stays in his off hand, carried off its handle where it holds it.
    b.cmd("vr_mock_button main grip 0")
    wpn = []
    for _ in range(30):
        (lines,) = reports(s, [srv])
        wpn = by_ent(lines, "mp_avatar_sv").get(eb, {}).get("wpn", [])
        own0 = drawn_report(b, "mp_drawn_own").get((eb, 0), {})
        sv0 = drawn_report(srv, "mp_drawn_sv").get((eb, 0), {})
        # (B22: and the hotspots settled -- the two reports are a frame or more apart, and the carried shotgun still
        # moving into the off hand's hold made B21's 0.62 units off, a flake)
        settled = spots_off(own0, sv0)
        if own0.get("carrying") == 1 and sv0.get("carrying") == 1 and wpn and int(wpn[0]) == 0 and \
                settled is not None and settled <= HOTSPOT_SLACK:
            break
    s.note("after the hand-off: the server's main weapon of B %s; B's own off hand %s; the server's %s" % (wpn, own0, sv0))
    # (B's view draws the weapons the server gives his hands: the shotgun drawn in his off hand is the server's hand-off)
    s.check("B's main hand let go: the server hands the shotgun off to his off hand (weaponanygrip)",
            bool(wpn) and int(wpn[0]) == 0 and own0.get("drawn") == 1 and own0.get("slot") == own.get("slot"),
            "the server's main weapon of B: %s; B's off hand draws %s" % (wpn, own0))
    carried_off = spots_off(own0, sv0)
    if carried_off is not None:
        s.metric("carried_hotspot_off", round(carried_off, 3))
    # (B3f: 1.4 units off before, the client's hand frame of a carried weapon taken from its entity)
    s.check("the server places the carried shotgun's hotspots where B's view draws them (within %.1f)" % HOTSPOT_SLACK,
            carried_off is not None and carried_off <= HOTSPOT_SLACK, "farthest off: %s" % carried_off)
    # (the off hand's place: B's own view's, the server's move's)
    hand_own = ground_report(b, "mp_drawn_own").get((eb, 0), {}).get("hand")
    hand_sv = ground_report(srv, "mp_drawn_sv").get((eb, 0), {}).get("hand")
    if hand_own and hand_sv:
        s.metric("carried_hand_off", round(dist(hand_own, hand_sv), 3))
    s.check("the server has B's off hand carrying it off its handle, as his view draws it (weapondrawnpose)",
            own0.get("carrying") == 1 and sv0.get("carrying") == 1, "server: %s; B's own: %s" % (sv0, own0))


def ground_report(inst, prefix):
    """vr_mp_drawn's B3f parts on `inst` -> {(ent, hand): {"gs": (weapon, index), "rev": (weapon, choice, cosine),
    "hand": [x, y, z]}} from its `prefix` lines (the ground spot an empty hand is near, the weapon lying nearest it and
    how it would take it: -1 not reversible, 0 upright, 1 reversed)."""
    since = inst.mark()
    inst.cmd("vr_mp_drawn; echo mp_drawn_done")
    n, _ = inst.wait(r"mp_drawn_done", 10, since, "%s's held weapons" % inst.name)
    out = {}
    for l in inst.lines[since:n]:
        m = re.match(r"%s: ent=(\d+) hand=(\d)" % prefix, l)
        gs = re.search(r"\bgs=(-?\d+),(-?\d+)", l)
        rev = re.search(r"\brev=(-?\d+),(-?\d+),(-?[\d.]+)", l)
        hand = re.search(r"\bat=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)", l)
        rot = re.search(r"\brot=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)", l)
        if m and gs and rev and hand and rot:
            out[(int(m.group(1)), int(m.group(2)))] = {
                "gs": (int(gs.group(1)), int(gs.group(2))),
                "rev": (int(rev.group(1)), int(rev.group(2)), float(rev.group(3))),
                "hand": [float(hand.group(i)) for i in (1, 2, 3)],
                "rot": [float(rot.group(i)) for i in (1, 2, 3)]}
            dr = re.search(r"\bdrawn=(\d+)", l)
            if dr:
                out[(int(m.group(1)), int(m.group(2)))]["drawn"] = int(dr.group(1))
            ns = re.search(r"\bnspeed=(-?[\d.]+)", l)
            if ns:
                out[(int(m.group(1)), int(m.group(2)))]["nspeed"] = float(ns.group(1))
            no = re.search(r"\bnorg=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)", l)
            if no:
                out[(int(m.group(1)), int(m.group(2)))]["norg"] = [float(no.group(i)) for i in (1, 2, 3)]
    return out


def wait_quietly(inst, pattern, timeout, since, what):
    """inst.wait, or None when it times out (the scenario checks instead of stopping)."""
    try:
        return inst.wait(pattern, timeout, since, what)
    except StepFailed:
        return None


def at_rest(srv, key, ent, tries=100):
    """The weapon `ent` lying nearest the server's hand `key` (ent, hand) still (its speed, `nspeed`, under 0.5) in two
    reports running -> True; False after `tries` reports (a safety net)."""
    still = 0
    for _ in range(tries):
        g = ground_report(srv, "mp_drawn_sv").get(key, {})
        moving = g.get("rev", (0,))[0] != ent or g.get("nspeed", 1.0) >= 0.5
        still = 0 if moving else still + 1
        if still >= 2:
            return True
    return False


ONTO_DEPTHS_CM = (-2, 4)  # a retaking hand into the weapon and up again, alternately (each reach down a move: a touch
#                            judged; 1 cm steps left a fist 0.77 cm off a sword on the floor)


def onto_each_report(b, target, until, want, tries):
    """B's hand placed on `target` (a `vr_mock_hand_to` target less its height) before each of up to `tries` reports
    (`until(want, 1)`), alternately ONTO_DEPTHS_CM into it, until the report says `want` -> the reports' values."""
    seen = []
    for i in range(tries):
        b.cmd("%s %d" % (target, ONTO_DEPTHS_CM[i % 2]))
        seen += until(want, 1)
        if seen[-1] == want:
            break
    return seen


def retake(srv, b, hand, gone_since, taken_re, above, down, presses=3, settled=None):
    """B presses `hand`'s grip GRIP_REPRESS_S of the server's game time after its let-go (logged after `gone_since`) with
    the hand placed by `above` (and, given `settled`, once it says the server has the hand there), then moves it into the
    weapon (`down`, a `vr_mock_hand_to` target less its height: placed again every half second, 2 cm into it and 4 cm over it by turns: a
    hand's touch is judged as it moves, and the weapon may still slide), until the server logs `taken_re` (at most
    `presses` presses, each let go again when it took nothing) -> the line."""
    since = gone_since
    for _ in range(presses):
        _, gone = srv.wait(r"grip: .*%s hand let go at" % hand, 10, since, "B's %s let-go on the server" % hand)
        server_time_past(srv, float(gone.rsplit(" ", 1)[1]) + GRIP_REPRESS_S)
        above() if callable(above) else b.cmd(above)
        if settled:
            settled()
        since = srv.mark()
        b.cmd("vr_mock_button %s grip 1" % hand)
        srv.wait(r"grip: .*%s hand pressed at" % hand, 10, since, "B's %s grip on the server" % hand)
        got = None
        for i in range(12):  # (onto it again every half second: it may still slide; a reach down every other time)
            b.cmd("%s %d" % (down, ONTO_DEPTHS_CM[i % 2]))
            got = wait_quietly(srv, taken_re, 0.5, since, "B's %s hand taking it" % hand)
            if got:
                return got[1]
        since = srv.mark()
        b.cmd("vr_mock_button %s grip 0" % hand)
    return None


def ground(s):
    """What the server asks of a remote player's grab of a weapon lying about, and his magnetic holsters (B3f): B's
    holster press (vr_magholster_return) reaches the server's QC as his; his empty hand at a hotspot of the shotgun he
    dropped: the server's ground spot of it is his view's (weapongrabspot), and his grip there carries it by that
    hotspot; his off hand over a knight's sword he dropped (hotspots off: by its handle), turned so that his view takes it
    reversed: the server's reverse grip choice is his view's (weapondrawnpose 43 by his own hand and vr_revgrip), and his
    grip takes it so."""
    srv, a, b, ea, eb = join(s)
    srv.cmd("developer 1")
    since = srv.mark()
    b.cmd("vr_magholster_return")
    got = wait_quietly(srv, r"magholster: player %d asked 4" % eb, 10, since, "B's holster press on the server")
    s.check("B's magnetic holster press (return them all) reaches the server's QC as his (magholsterpoll)", got is not None,
            "no 'magholster: player %d asked 4' in the server's log" % eb)

    # The shotgun given, its hotspots as B's view draws them; let go, it falls by his feet.
    grip_then_give(srv, b, "vr_mock_hand main 0.1 0.25 -0.35 0 0 0; vr_mock_button main grip 1; vr_mock_button off grip 0")
    weapon_until(s, srv, eb, 4)
    own = {}
    for _ in range(20):
        own = drawn_report(b, "mp_drawn_own").get((eb, 1), {})
        if own.get("spots"):
            break
    grips = sorted(i for i, v in own.get("spots", {}).items() if int(v[0]) == 1)
    if not grips:
        raise StepFailed("the shotgun has no grip hotspot: %s" % own)
    gone_since = srv.mark()
    let_go_still(srv, b, "main")
    seen = weapon_until(s, srv, eb, 0)
    s.check("B's shotgun falls from his opened hand", seen[-1] == 0, "B's main weapon by report: %s" % seen)
    # (B17: and gone from B's own hand in his view -- the server's report comes first; a hand sent to "the weapon lying
    # nearest" while his view still drew it held went to another one 25 m away, the suite's flake)
    for _ in range(30):
        if drawn_report(b, "mp_drawn_own").get((eb, 1), {}).get("drawn", 1) == 0:
            break
    # (B25: and come to rest on the server first, as the sword below: under load the hand was put on its hotspot while it
    # still slid at 12-23 u/s, and its jumps after it knocked it 1.5 m away at 95 u/s (a "blow"), no take; the sword's
    # checks then found the shotgun nearest)
    shotgun = 0
    for _ in range(30):
        shotgun = ground_report(srv, "mp_drawn_sv").get((eb, 1), {}).get("rev", (0,))[0]
        if shotgun > 0:
            break
    at_rest(srv, (eb, 1), shotgun)

    # His main hand over its grip hotspot as it lies (placed again each report: it may still slide), within the view's
    # reach of a ground spot (15 cm).
    above = "vr_mock_hand_to main spot %d 6" % grips[0]
    own_g = sv_g = {}
    for _ in range(30):
        b.cmd(above)
        own_g = ground_report(b, "mp_drawn_own").get((eb, 1), {})
        sv_g = ground_report(srv, "mp_drawn_sv").get((eb, 1), {})
        # (its ground spot at that hotspot: dropped still, the shotgun lies under the hand, whose view first finds another)
        if own_g.get("gs", (0, -1))[0] > 0 and own_g["gs"][1] == grips[0] and sv_g.get("gs") == own_g.get("gs"):
            break
    s.note("B's own main hand: %s; the server's: %s" % (own_g, sv_g))
    s.check("B's view finds his empty main hand at the shotgun's grip hotspot %d" % grips[0],
            own_g.get("gs", (0, -1))[0] > 0 and own_g["gs"][1] == grips[0], "B's own: %s" % own_g)
    s.check("the server's ground spot of B's main hand is his view's (weapongrabspot)",
            own_g.get("gs", (0, -1))[0] > 0 and sv_g.get("gs") == own_g.get("gs"),
            "server %s; B's own %s" % (sv_g.get("gs"), own_g.get("gs")))
    line = retake(srv, b, "main", gone_since, r"weapon: .* taken into the main hand", above,
                  "vr_mock_hand_to main spot %d" % grips[0])
    s.note("the server's take: %s" % line)
    s.check("B's grip takes the shotgun by that hotspot (the server's QC: 'by its hotspot %d')" % grips[0],
            line is not None and ("by its hotspot %d" % grips[0]) in line, "server: %s" % line)

    # The sword in his off hand (impulse 170 + WID_SWORD 13), let go; hotspots off on B: taken by its handle.
    # (the main hand with the carried shotgun raised out of the way: a sword let go onto it was flung 90 units, 2 runs of 6)
    b.cmd("vr_weapon_grab_hotspots 0; vr_weapon_grab_anywhere 0; vr_mock_hand main 0.45 1.3 -0.1 0 0 0")
    grip_then_give(srv, b, "vr_mock_hand off -0.1 0.25 -0.35 0 0 0; vr_mock_button off grip 1", "off", 183)
    off = []
    for _ in range(30):
        (lines,) = reports(s, [srv])
        off.append(int((by_ent(lines, "mp_avatar_sv").get(eb, {}).get("wpn", [-1, -1]) + [-1, -1])[1]))
        if off[-1] == 13:
            break
    s.check("B holds the given sword in his off hand (the server's report)", off[-1] == 13, "B's off weapon: %s" % off)
    nearest_before = ground_report(srv, "mp_drawn_sv").get((eb, 0), {}).get("rev", (0,))[0]
    gone_since = srv.mark()
    let_go_still(srv, b, "off")
    own_r = sv_r = {}
    tried = []
    # (the hand's yaw turns the way its held blade would point: against the lying blade's way, reversed)
    turn = "0 0 0"
    # (the hand over its handle as it lies (`spot -1`: its origin): with hotspots off, a fist on it takes it by its
    # handle; a fist on its blade lying flat was held 2.9 cm off it by the hand's body, short of a take, 1 run in 5,
    # where at its handle vr_weapon_grab_handle_leniency allows it, as a player reaching for the hilt)
    # (turned up high in one frame, then put over the sword in the next: `vr_mock_hand_to` steps from the hand's last
    # place, so the two in one frame left it where the turn put it every other time; turned low by the sword, it swept
    # through it, flinging it at 473 u/s, 3 runs of 3)
    # (B17: 12 cm over its handle, not 6: placed exactly there (vr_mock_hand_to_absolute), the turned fist 6 cm over a
    # sword lying on its edge was 7.5 cm into it, its grip not yet pressed, and shoved it 2 m away, 1 suite run in 1)
    # (B25: not turned at head height instead: from there `spot -1`, the weapon lying nearest the hand, was another one
    # and the retake took nothing, 2 runs under load; the sword's tipping is waited out in agreed() instead)
    def over(turn):
        def place():
            since = b.mark()
            b.cmd("vr_mock_hand off -0.1 1.0 -0.35 %s; echo mp_over_turned" % turn)
            b.wait(r"mp_over_turned", 10, since, "B's off hand turned")
            b.cmd("vr_mock_hand_to off spot -1 12")
        return place

    def agreed():
        """B's own and the server's reports of his off hand until the server has the hand where his view has it (its
        angles within 3 degrees: the move with the new turn arrived) and the same choice, its cosine within 0.2 (the sword
        as B draws it caught up with the server's: -0.84 against -0.6 once, the same hand angles) (at most 20 reports each)."""
        # (B25: and the server's cosine the same as in its report before, within 0.05: the sword lying on its edge tipped
        # over slowly after the hand's turn, its origin still (nspeed 0): B's own cosine -0.91, -0.72, 0.02, 0.68 at the
        # same hand, the take upright under load; at most 40 reports each)
        o = v = {}
        prev = None
        for _ in range(40):
            o = ground_report(b, "mp_drawn_own").get((eb, 0), {})
            v = ground_report(srv, "mp_drawn_sv").get((eb, 0), {})
            still = prev is not None and v.get("rev", (0, -1, 9))[:2] == prev[:2] and \
                abs(v.get("rev", (0, -1, 9))[2] - prev[2]) <= 0.05
            prev = v.get("rev", (0, -1, 9))
            if still and o.get("rev", (0, -1, 0))[0] > 0 and v.get("rev", (0,))[0] == o["rev"][0] and \
                    v["rev"][1] == o["rev"][1] and angle_diff(o["rot"], v["rot"]) <= 3.0 and \
                    abs(v["rev"][2] - o["rev"][2]) <= 0.2:
                break
        return o, v

    # (first the sword come to rest by the hand: the weapon lying nearest it one that may be held reversed; a sword
    # still tumbling from the hand was once not the nearest, a run of 3; the hand placed over it while it still tumbled
    # flung it away at 400 u/s, 2 runs of 3: the server's sword at rest first, as a player waits for it to land)
    # (the sword: the weapon lying nearest his off hand once the server has it let go, not the one nearest before)
    srv.wait(r"grip: .*off hand let go at", 10, gone_since, "B's off hand let-go on the server")
    sword = 0
    for _ in range(30):
        sword = ground_report(srv, "mp_drawn_sv").get((eb, 0), {}).get("rev", (0,))[0]
        if sword > 0 and sword != nearest_before:
            break
    s.check("the sword let go comes to rest on the server", sword > 0 and sword != nearest_before and at_rest(srv, (eb, 0), sword),
            "the sword: entity %d (the weapon lying nearest before: %d)" % (sword, nearest_before))
    over(turn)()
    for _ in range(30):
        own_r = ground_report(b, "mp_drawn_own").get((eb, 0), {})
        if own_r.get("rev", (0, -1, 0))[1] >= 0:
            break
    for turn in ("0 0 0", "0 180 0", "0 90 0", "0 -90 0"):
        over(turn)()
        own_r, sv_r = agreed()
        tried.append((turn, own_r.get("rev"), sv_r.get("rev"), own_r.get("rot"), sv_r.get("rot")))
        if own_r.get("rev", (0, -1, 0))[1] == 1:
            break
    s.note("the sword's reverse grip by the hand's turn (B's own, the server's; their hand angles): %s" % tried)
    s.check("B's view would take the sword lying by him reversed at one of the hand's turns tried",
            own_r.get("rev", (0, -1, 0))[1] == 1, "tried %s" % tried)
    s.check("the server's reverse grip choice for B's off hand is his view's (the same sword, choice, cosine within 0.2)",
            own_r.get("rev", (0,))[0] > 0 and sv_r.get("rev", (0,))[0] == own_r["rev"][0] and
            sv_r["rev"][1] == own_r["rev"][1] and abs(sv_r["rev"][2] - own_r["rev"][2]) <= 0.2,
            "server %s; B's own %s" % (sv_r.get("rev"), own_r.get("rev")))
    line = retake(srv, b, "off", gone_since, r"revgrip: .* taken (reversed|upright)",
                  over(turn), "vr_mock_hand_to off spot -1", settled=agreed)
    s.note("the server's take: %s" % line)
    want = "reversed" if own_r.get("rev", (0, -1, 0))[1] == 1 else "upright"
    s.check("B's grip takes the sword %s, as his view chose (the server's QC VR_RevGrip_OnTake)" % want,
            line is not None and (" taken %s" % want) in line, "server: %s" % line)
    srv.cmd("vr_debug_carry 0; developer 0")


INV_KEYS = ("health", "armor", "items", "shells", "nails", "rockets", "cells", "weapon", "clip")  # (clip: the main hand's gun's)
IT_ROCKET_LAUNCHER = 32
IT_SUPER_NAILGUN = 8


def inventories(srv):
    """The server's players' inventories (vr_mp_status's `mp_client: ... inv=`) -> {slot: {INV_KEYS...}}, and the map."""
    since = srv.mark()
    srv.cmd("vr_mp_status")
    n, last = srv.wait(r"mp_server: map=", 10, since, "the server's status")
    out = {}
    for l in srv.lines[since:n]:
        m = re.match(r"mp_client: slot=(\d+) .*spawned=1 .*inv=([-\d,]+)", l)
        if m:
            out[int(m.group(1))] = dict(zip(INV_KEYS, (int(x) for x in m.group(2).split(","))))
    return out, fields(last).get("map", "")


def inventories_until(srv, done, tries=60, seconds=20.0):
    """inventories(srv) until done(inv, map) -> (inv, map) (the last). Gives up only after `tries` reports AND `seconds`
    of real time: a fast server answered 60 reports before a client's forwarded `give` reached it (mp_rejoin, 2026-11-20:
    B killed before his rockets were his, then the give landed: "items 4097 -> 4129")."""
    inv, mapname = {}, ""
    end, n = time.monotonic() + seconds, 0
    while True:
        inv, mapname = inventories(srv)
        n += 1
        if done(inv, mapname) or (n >= tries and time.monotonic() >= end):
            break
    return inv, mapname


def carried(before, after, keys=("items", "shells", "nails", "rockets", "cells", "weapon"), change=True):
    """The inventory keys that differ (a level's change keeps these; health is clamped to 50..100, shells raised to 25 at
    least: QC's SetChangeParms; `change` False: the same game state, no such rule)."""
    def want(k):
        v = before.get(k)
        return max(v, 25) if change and k == "shells" and v is not None else v
    return [k for k in keys if want(k) != after.get(k)]


def campaign(s):
    """The coop campaign flow (B4): a dedicated server's coop e1m1 to e1m2 to e1m3, both players' inventories carried by
    each change of level; a save on e1m2 (each player's block: `// qvr_coop`, `// qvr_coopslot`), then its load from
    e1m3 with both players kept (each back in his own slot with what he had); B killed after the load respawns with his
    level-start inventory of the loaded level (his saved parms), not of the level he loaded from."""
    t0 = time.time()
    srv, a, b, ea, eb = join(s)
    sa, sb = ea - 1, eb - 1
    a.cmd("give 3; give 4; give s 60; give n 80")
    b.cmd("give 5; give 6; give n 90; give r 9")
    inv1, _ = inventories_until(srv, lambda inv, m: inv.get(sa, {}).get("nails") == 80 and
                                inv.get(sb, {}).get("rockets") == 9)
    s.note("e1m1: A %s; B %s" % (inv1.get(sa), inv1.get(sb)))
    s.check("A's and B's gifts on the server (e1m1)", inv1.get(sa, {}).get("nails") == 80 and
            inv1.get(sb, {}).get("rockets") == 9, "%s" % inv1)

    def change(level, before, label):
        since_a, since_b = a.mark(), b.mark()
        srv.cmd("changelevel %s" % level)
        a.wait(r"mp_event: .*joined map=%s" % level, 120, since_a, "A on %s" % level)
        b.wait(r"mp_event: .*joined map=%s" % level, 120, since_b, "B on %s" % level)
        inv, m = inventories_until(srv, lambda inv, m: m == level and sa in inv and sb in inv)
        for slot, name in ((sa, "A"), (sb, "B")):
            diff = carried(before.get(slot, {}), inv.get(slot, {}))
            s.check("%s: %s's inventory carried (weapons, ammo, the weapon in hand)" % (label, name), not diff and
                    inv.get(slot, {}).get("health", 0) >= 50,
                    "before %s; after %s (differ: %s)" % (before.get(slot), inv.get(slot), diff))
        return inv

    start2 = change("e1m2", inv1, "e1m1 to e1m2")
    # B takes a rocket launcher on e1m2 (after its start: not in his level-start parms), A some cells
    b.cmd("give 7")
    a.cmd("give c 40")
    inv2, _ = inventories_until(srv, lambda inv, m: inv.get(sb, {}).get("items", 0) & IT_ROCKET_LAUNCHER and
                                inv.get(sa, {}).get("cells") == 40)
    s.check("B's rocket launcher and A's cells on e1m2", inv2.get(sb, {}).get("items", 0) & IT_ROCKET_LAUNCHER and
            inv2.get(sa, {}).get("cells") == 40, "%s" % inv2)

    since = srv.mark()
    srv.cmd("save b4coop")
    srv.wait(r"Saving game to", 10, since, "the server's save")
    path = None
    for _ in range(50):
        path = next((os.path.join(s.base, d, "b4coop.sav") for d in os.listdir(s.base)
                     if os.path.isfile(os.path.join(s.base, d, "b4coop.sav")) and
                     os.path.getmtime(os.path.join(s.base, d, "b4coop.sav")) >= t0 - 5), None)
        if path:
            text = open(path, encoding="latin-1").read()
            if "\n// qvr_coop " in text and text.rstrip().endswith("}"):
                break
        time.sleep(0.1)
    text = open(path, encoding="latin-1").read() if path else ""
    coop_line = re.search(r"^// qvr_coop (\d+) (\d+) (\d+)$", text, re.M)
    slots_in = re.findall(r"^// qvr_coopslot (\d+) 1 (\d+) ", text, re.M)
    s.note("the save: %s; %s; players in: %s" % (path, coop_line.group(0) if coop_line else None, slots_in))
    s.check("a coop save on the dedicated server, each player's block in it (qvr_coop: 4 slots; qvr_coopslot for A's "
            "and B's)", coop_line is not None and coop_line.group(2) == "4" and
            sorted(int(x[0]) for x in slots_in) == sorted((sa, sb)) and "// qvr_save 2 " in text,
            "qvr_coop %s; slots in %s" % (coop_line.group(0) if coop_line else None, slots_in))

    inv3 = change("e1m3", inv2, "e1m2 to e1m3")
    a.cmd("give 8")
    b.cmd("give 8")
    inventories_until(srv, lambda inv, m: all(inv.get(x, {}).get("items", 0) & 64 for x in (sa, sb)))

    since_a, since_b, since_srv = a.mark(), b.mark(), srv.mark()
    srv.cmd("load b4coop")
    _, ja = a.wait(r"mp_event: .*joined map=e1m2", 120, since_a, "A back on e1m2")
    _, jb = b.wait(r"mp_event: .*joined map=e1m2", 120, since_b, "B back on e1m2")
    loaded, m = inventories_until(srv, lambda inv, m: m == "e1m2" and sa in inv and sb in inv)
    for slot, name in ((sa, "A"), (sb, "B")):
        diff = carried(inv2.get(slot, {}), loaded.get(slot, {}), INV_KEYS, change=False)
        s.check("the load from e1m3: %s kept, back on e1m2 in his slot with what he had at the save" % name,
                m == "e1m2" and not diff, "saved %s; loaded %s (differ: %s)" % (inv2.get(slot), loaded.get(slot), diff))
    s.check("the load keeps both players in their slots (each his own entity again)",
            (int(fields(ja)["self"]), int(fields(jb)["self"])) == (ea, eb),
            "A %s, B %s; before %d, %d" % (fields(ja).get("self"), fields(jb).get("self"), ea, eb))

    s.check("the server gave each saved player's block to his slot ('coop save: 2 players of 4 slots')",
            srv.find(r"coop save: 2 players of 4 slots", since_srv) is not None, "")

    # B suicides (kill: QC's ClientKill respawns him at once): his inventory the loaded level's start (his saved parms:
    # no rocket launcher), not e1m3's (with it)
    since_b = b.mark()
    b.cmd("kill")
    b.wait(r"suicides", 10, since_b, "B's suicide")
    back, _ = inventories_until(srv, lambda inv, m: sb in inv and not inv[sb]["items"] & IT_ROCKET_LAUNCHER, 20)
    diff = carried(start2.get(sb, {}), back.get(sb, {}), ("items", "shells", "nails", "rockets", "cells"), change=False)
    s.note("B respawned: %s; his e1m2 start %s; his e1m3 start %s" % (back.get(sb), start2.get(sb), inv3.get(sb)))
    s.check("B respawns with his level-start inventory of the loaded e1m2 (his saved parms: no rocket launcher)",
            back.get(sb, {}).get("health", 0) > 0 and not diff and not back[sb]["items"] & IT_ROCKET_LAUNCHER,
            "respawned %s; e1m2 start %s (differ: %s)" % (back.get(sb), start2.get(sb), diff))
    s.metric("inventories", {"e1m1": inv1, "e1m2_start": start2, "e1m2_saved": inv2, "e1m3_start": inv3,
                             "loaded": loaded, "b_respawned": back.get(sb)})


def campaign_listen(s):
    """A coop save on a listen host (A): loaded with B connected (both kept, the host's own client reconnecting as at a
    changelevel), then by A alone after he left for a single player server (the save's 4 players set again: maxplayers
    and listen), C joining first and B after him, each into his own saved slot by his name (B4c: C's connection would
    take B's slot, the first free one) with what he had. Then (REVIEW_MP_2026-11-18 finding 3) the host loads it alone
    renamed: his own client still in slot 0 and the game's own player (campaign_listen_host_slot)."""
    srv, a, b, ea, eb = join(s, listen=True)
    sa, sb = ea - 1, eb - 1
    c = s.client("C").start()
    c.cmd("name C; " + s.connect_cmd(c))
    _, jc = c.wait(r"mp_event: .*joined map=e1m1", 120, what="C joined e1m1")
    ec = int(fields(jc)["self"])
    sc = ec - 1
    a.cmd("name A; give 3; give s 44")
    b.cmd("name B; give 7; give r 12")
    c.cmd("give 5; give n 33")
    inv1, _ = inventories_until(a, lambda inv, m: inv.get(sa, {}).get("shells") == 44 and
                                inv.get(sb, {}).get("rockets") == 12 and inv.get(sc, {}).get("nails") == 33)
    s.check("A's, B's and C's gifts on the listen host", inv1.get(sa, {}).get("shells") == 44 and
            inv1.get(sb, {}).get("rockets") == 12 and inv1.get(sc, {}).get("nails") == 33, "%s" % inv1)
    since = a.mark()
    a.cmd("save b4listen")
    a.wait(r"Saving game to", 10, since, "the host's save")
    a.cmd("give 8")
    b.cmd("give 8")
    inventories_until(a, lambda inv, m: all(inv.get(x, {}).get("items", 0) & 64 for x in (sa, sb)))

    since_a, since_b, since_c = a.mark(), b.mark(), c.mark()
    a.cmd("load b4listen")
    _, ja = a.wait(r"mp_event: .*joined map=e1m1", 120, since_a, "A in the loaded game")
    _, jb = b.wait(r"mp_event: .*joined map=e1m1", 120, since_b, "B in the loaded game")
    c.wait(r"mp_event: .*joined map=e1m1", 120, since_c, "C in the loaded game")
    loaded, _ = inventories_until(a, lambda inv, m: sa in inv and sb in inv and sc in inv and inv[sa]["shells"] == 44)
    for slot, name in ((sa, "A"), (sb, "B"), (sc, "C")):
        diff = carried(inv1.get(slot, {}), loaded.get(slot, {}), INV_KEYS, change=False)
        s.check("the host's load with B connected: %s kept in his slot with what he had at the save" % name,
                not diff, "saved %s; loaded %s (differ: %s)" % (inv1.get(slot), loaded.get(slot), diff))
    s.check("both in their own slots again", (int(fields(ja)["self"]), int(fields(jb)["self"])) == (ea, eb),
            "A %s, B %s" % (fields(ja).get("self"), fields(jb).get("self")))

    # A leaves for a single player server (B dropped), then loads the coop save alone; B joins it again
    since_a, since_b = a.mark(), b.mark()
    a.cmd("disconnect; maxplayers 1; map e1m1")
    a.wait(r"mp_event: .*joined map=e1m1", 120, since_a, "A alone on e1m1")
    since_a = a.mark()
    a.cmd("load b4listen")
    a.wait(r"mp_event: .*joined map=e1m1", 120, since_a, "A in the coop save loaded alone")
    st = status(a)
    srv_line = next((l for l in st if l.startswith("mp_status:")), "")
    s.check("the coop save loaded by a single player host plays it with its 4 players",
            fields(srv_line).get("clients") == "4", srv_line)
    # C first: the first free slot is B's (1); his name gives him his own (2). Then B, into his.
    since_c = c.mark()
    c.cmd(s.connect_cmd(c))
    _, jc = c.wait(r"mp_event: .*joined map=e1m1", 120, since_c, "C joining the loaded game")
    since_b = b.mark()
    b.cmd(s.connect_cmd(b))
    _, jb = b.wait(r"mp_event: .*joined map=e1m1", 120, since_b, "B joining the loaded game")
    alone, _ = inventories_until(a, lambda inv, m: sb in inv and inv[sb]["rockets"] == 12 and sc in inv and
                                 inv[sc]["nails"] == 33)
    for slot, ent, j, name in ((sc, ec, jc, "C (joining first: matched by name)"), (sb, eb, jb, "B")):
        diff = carried(inv1.get(slot, {}), alone.get(slot, {}), INV_KEYS, change=False)
        s.check("%s joins into his saved slot with what he had at the save" % name,
                int(fields(j)["self"]) == ent and not diff,
                "entity %s (was %d); saved %s; now %s (differ: %s)" % (fields(j).get("self"), ent, inv1.get(slot),
                                                                     alone.get(slot), diff))
    diff = carried(inv1.get(sa, {}), alone.get(sa, {}), INV_KEYS, change=False)
    s.check("A, alone at the load, has what he had at the save", not diff,
            "saved %s; now %s (differ: %s)" % (inv1.get(sa), alone.get(sa), diff))
    campaign_listen_host_slot(s, a, sa, inv1)


def host_load(s, a, name):
    """A, renamed `name`, leaves for a single player server and loads the coop save b4listen alone -> (his entity, the
    server's mp_server fields, the server's lines since the load)."""
    since_a = a.mark()
    a.cmd("name %s; disconnect; maxplayers 1; map e1m1" % name)
    a.wait(r"mp_event: .*joined map=e1m1", 120, since_a, "A alone on e1m1")
    since_a = a.mark()
    a.cmd("load b4listen")
    _, ja = a.wait(r"mp_event: .*joined map=e1m1", 120, since_a, "A (%s) in the coop save loaded alone" % name)
    lines = a.lines[since_a:]
    st = status(a)
    return int(fields(ja)["self"]), fields(next((l for l in st if l.startswith("mp_server:")), "")), lines


def campaign_listen_host_slot(s, a, sa, inv1):
    """(REVIEW_MP_2026-11-18 finding 3) The host's own client takes slot 0 in a loaded coop save whatever name slot 0 was
    saved under, and the game's own player (progs::localPlayer, clientprefs::ownSlot) is his. Renamed Z (no saved slot
    of his): he takes slot 0's saved player (before: the first free slot nobody waited for, slot 3, with the game's own
    player slot 0's, nobody's until A's name came back). Renamed B (B's saved slot 1 his by name): B's block swapped
    into slot 0, slot 1 left for A's name."""
    ent, srv, lines = host_load(s, a, "Z")
    said = next((l for l in lines if "coop save: the host Z takes slot 0" in l), None)
    s.check("the host renamed Z loads the coop save alone: his own client in slot 0 (entity 1), the game's own player "
            "his (localPlayer: entity 1; ownSlot 0)", ent == 1 and srv.get("own") == "1" and srv.get("ownslot") == "0"
            and said is not None and "no saved slot of his own" in said,
            "entity %d; mp_server own=%s ownslot=%s; %s" % (ent, srv.get("own"), srv.get("ownslot"),
                                                           said.strip() if said else "no 'takes slot 0' line"))
    inv, _ = inventories_until(a, lambda inv, m: sa in inv)
    diff = carried(inv1.get(sa, {}), inv.get(sa, {}), INV_KEYS, change=False)
    s.check("the host renamed Z has slot 0's saved player (what A had at the save)", not diff,
            "saved %s; now %s (differ: %s)" % (inv1.get(sa), inv.get(sa), diff))

    ent, srv, lines = host_load(s, a, "B")
    said = next((l for l in lines if "coop save: the host B takes slot 0" in l), None)
    s.check("the host renamed B (B's saved slot 1) loads it: slot 0 (entity 1), localPlayer his, B's saved block swapped "
            "into slot 0", ent == 1 and srv.get("own") == "1" and srv.get("ownslot") == "0" and said is not None and
            "his own saved slot 1 swapped" in said,
            "entity %d; mp_server own=%s ownslot=%s; %s" % (ent, srv.get("own"), srv.get("ownslot"),
                                                           said.strip() if said else "no 'takes slot 0' line"))


def saved_text(s, relname, t0, tries=50):
    """The save `relname` (".sav" added) in a game folder under the base, written since `t0` (wall clock) and whole
    (its last line the edict count, written just before the save thread closes it: the last edict's brace alone came
    while the file was still open, and the game lists no file open for writing: Sys_GetFileTime) -> (path, text);
    (None, "") if none came (at most `tries` looks a tenth apart)."""
    for _ in range(tries):
        for d in os.listdir(s.base):
            path = os.path.join(s.base, d, relname + ".sav")
            if os.path.isfile(path) and os.path.getmtime(path) >= t0 - 1:
                text = open(path, encoding="latin-1").read()
                if "\n// qvr_coop " in text and re.search(r"// \d+ edicts\s*$", text):
                    return path, text
        time.sleep(0.1)
    return None, ""


def own_status(inst):
    """inst's own `mp_status:` fields (its map, intermission, health as its client sees them)."""
    return fields(next((l for l in status(inst) if l.startswith("mp_status:")), ""))


NAVFOLLOW_TARGET = (96.0, 576.0, 25.0)  # e1m1: past the first room's west door (x 209-255), a step down from the start
NAVFOLLOW_REACH = 112.0  # the follower ends this near the followed (vr_nav_follow_stop 64 along the path, a walk's slack)


def navfollow_status(inst):
    """`vr_nav_follow_status` on `inst` -> its fields (state, target, corners, next, driving, paused, arrived)."""
    since = inst.mark()
    inst.cmd("vr_nav_follow_status")
    _, line = inst.wait(r"navfollow: state=", 10, since, "%s's follow status" % inst.name)
    return {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)\b", line.split(" at=")[0])}


def navfollow(s, listen=False, flat="AB"):
    """(navfollow) vr_nav_follow: a player walked after another along the server's navmesh by his own client's moves.
    Both flat on coop e1m1 (god, notarget). The leader is put past the first room's west door; the follower, at a coop
    start, follows him (dedicated: B, a remote client, follows A; --listen: the host A follows B): down the step, across
    the room and through the door. The leader walks on a while; the follower ends within NAVFOLLOW_REACH of him with
    arrived=1. The follower's own move hands control back (paused=1) and the follow walks on after vr_nav_follow_idle
    without one (paused=0); vr_nav_follow 0 stops it."""
    srv, a, b, ea, eb = join(s, listen=listen, flat=flat)
    leader, follower, el, ef = (b, a, eb, ea) if listen else (a, b, ea, eb)
    for inst in (a, b):
        inst.cmd("god; notarget")
    put_at(srv, leader, el, NAVFOLLOW_TARGET, yaw=180.0)
    since = follower.mark()
    follower.cmd("vr_nav_follow %d" % el)
    _, line = follower.wait(r"vr_nav_follow: (following|no other)", 20, since, "the follow begun")
    s.check("%s's vr_nav_follow %d follows %s" % (follower.name, el, leader.name), "following" in line, line.strip())
    # The follower walks to him: within reach and arrived (a definite end: 300 reports).
    start = org_of(status(srv), "mp_client", "slot", ef - 1)
    got, st, near = None, {}, None
    for _ in range(300):
        lines = status(srv)
        got = org_of(lines, "mp_client", "slot", ef - 1)
        near = org_of(lines, "mp_client", "slot", el - 1)
        st = navfollow_status(follower)
        if got and near and dist(got[:2], near[:2]) <= NAVFOLLOW_REACH and st.get("arrived") == 1:
            break
    s.metric("navfollow_first_units", round(dist(got[:2], near[:2]), 1) if got and near else -1)
    s.check("%s walks from %s through the door to %s: within %d units, arrived" % (follower.name, start, near,
            NAVFOLLOW_REACH), got is not None and near is not None and dist(got[:2], near[:2]) <= NAVFOLLOW_REACH and
            st.get("arrived") == 1 and got[0] < 209.0, "at %s, %s" % (got, st))
    # The leader walks on (west, +forward) at least 80 units (a wall stops him past that); the follower follows on and arrives again.
    leader.cmd("+forward")
    for _ in range(200):
        moved = org_of(status(srv), "mp_client", "slot", el - 1)
        if moved and near and dist(moved[:2], near[:2]) >= 80.0:
            break
    leader.cmd("-forward")
    for _ in range(300):
        lines = status(srv)
        got = org_of(lines, "mp_client", "slot", ef - 1)
        near = org_of(lines, "mp_client", "slot", el - 1)
        st = navfollow_status(follower)
        if got and near and dist(got[:2], near[:2]) <= NAVFOLLOW_REACH and st.get("arrived") == 1:
            break
    s.metric("navfollow_after_walk_units", round(dist(got[:2], near[:2]), 1) if got and near else -1)
    s.check("%s walked on; %s follows: within %d units, arrived" % (leader.name, follower.name, NAVFOLLOW_REACH),
            got is not None and near is not None and dist(got[:2], near[:2]) <= NAVFOLLOW_REACH and st.get("arrived") == 1,
            "%s at %s, %s at %s, %s" % (follower.name, got, leader.name, near, st))
    # His own move hands it back at once; it walks on after vr_nav_follow_idle without one.
    follower.cmd("+back")
    st = {}
    for _ in range(50):
        st = navfollow_status(follower)
        if st.get("paused") == 1:
            break
    follower.cmd("-back")
    s.check("%s's own move hands control back (paused)" % follower.name, st.get("paused") == 1, str(st))
    for _ in range(100):
        st = navfollow_status(follower)
        if st.get("paused") == 0 and st.get("driving") == 1:
            break
    s.check("... and the follow walks on after vr_nav_follow_idle without his input", st.get("paused") == 0 and
            st.get("driving") == 1, str(st))
    since = follower.mark()
    follower.cmd("vr_nav_follow 0")
    follower.wait(r"vr_nav_follow: off", 20, since, "the follow off")
    st = navfollow_status(follower)
    s.check("vr_nav_follow 0: off", st.get("state") == 0 and st.get("driving") == 0, str(st))


def navbots_status(inst):
    """`vr_nav_follow_status`'s navbots line on `inst` -> its fields (asked, paths, corners, drawn)."""
    since = inst.mark()
    inst.cmd("vr_nav_follow_status")
    _, line = inst.wait(r"navbots: asked=", 10, since, "%s's bot paths status" % inst.name)
    return {k: int(v) for k, v in fields(line).items() if re.fullmatch(r"-?\d+", v)}


def botnavdebug(s, listen=False):
    """(flakehunt; his word 2026-11-21: "Ensure the debug view works here too") The bots' navmesh paths in the navmesh
    debug view (vr_nav_debug) on a client: a deathmatch server on e1m1 (dedicated: A a remote client; listen: A the
    host), A draws the navmesh (vr_nav_debug 2) and adds a bot (impulse 100); the server's QC test vr_dm_test 19 makes
    the bot chase A by the navmesh past the first room's west door (vr_bot_navmesh 1). While it walks A's client has a
    bot path from the server (QVR_SVC_BOTPATH) and draws it (navbots: paths >= 1, drawn lines >= 1); the test passes;
    vr_nav_debug 0 drops the paths (asked=0, paths=0)."""
    level = "e1m1"
    if listen:
        a = srv = s.server(dedicated=False, name="A")
    else:
        srv = s.server(dedicated=True)
    srv.start()
    srv.cmd("%scoop 0; deathmatch 1; noexit 1; fraglimit 0; timelimit %d; skill 1; map %s; vr_mp_status" % (
        "disconnect; maxplayers 4; listen 1; " if listen else "", DM_TIMELIMIT, level))
    srv.wait(r"mp_server: map=%s" % level, 120, what="the server on %s" % level)
    if not listen:
        a = s.client("A").start()
        a.cmd(s.connect_cmd(a))
    a.wait(r"mp_event: .*joined map=%s" % level, 120, what="A joined %s" % level)
    a.cmd("name A; vr_nav_debug 2")
    since_a = a.mark()
    a.cmd("impulse 100")
    wait_quietly(a, r"mp_frags: .*named=1", 10, since_a, "the bot named at A")
    since_srv = srv.mark()
    srv.cmd("developer 1; vr_dm_test 19")
    # (case 1's chase: the bot's "navmesh to A (a player it saw)" line, after case 0's walk with the navmesh off)
    wait_quietly(srv, r"botnav: .* navmesh to A", 150, since_srv, "the bot's navmesh chase begun")
    best, verdict = {}, None
    for _ in range(200):
        st = navbots_status(a)
        if st.get("paths", 0) >= 1 and st.get("drawn", 0) >= best.get("drawn", 0):
            best = st
        verdict = srv.find(r"dmtest: \d+ passed, \d+ failed", since_srv)
        if verdict or (best.get("paths", 0) >= 1 and best.get("drawn", 0) >= 1):
            break
    s.check("A's client (%s) has the bot's navmesh path from the server and draws it (vr_nav_debug 2)" % (
        "the listen host" if listen else "a remote client"),
        best.get("paths", 0) >= 1 and best.get("corners", 0) >= 2 and best.get("drawn", 0) >= 1, str(best))
    if not verdict:
        _, verdict = srv.wait(r"dmtest: \d+ passed, \d+ failed", 150, since_srv, "vr_dm_test 19's verdict")
    else:
        verdict = verdict[1]
    # (the chase's own outcome is vr_dm_test 19's, a bot-AI test of its own with a known flake: the bot sees him at the
    # door, goes back to its waypoints and wanders; noted here, the drawing being this scenario's)
    s.note("vr_dm_test 19 (the bot's navmesh chase): %s" % verdict.strip())
    a.cmd("vr_nav_debug 0")
    st = {}
    for _ in range(20):
        st = navbots_status(a)
        if st.get("asked") == 0:
            break
    s.check("vr_nav_debug 0: the bots' paths dropped (asked=0, paths=0)", st.get("asked") == 0 and st.get("paths") == 0,
            str(st))


def coopbots(s):
    """(MP bots phase 1) Bots in a coop game (FrikBot's coop rules, the hands of a flat-screen player): on a dedicated
    coop server on e1m1 A adds a bot (impulse 100): three players listed at A; the server's QC test (vr_dm_test 11)
    brings a grunt of the map in front of the bot: the bot kills it and never takes a player for its enemy, and is given
    a super nailgun; then A in e1m1's exit (vr_goto_exit), his trigger through the intermission: on e1m2 the bot is back
    (FrikBot's bot_return) with what it had (vr_coopsave.cpp: kept by its name as a leaver's, given back as it connects:
    its super nailgun)."""
    srv, a, b, ea, eb = join(s)
    sa, sb = ea - 1, eb - 1
    srv.cmd("developer 1")
    a.cmd("impulse 100")
    table = {}
    for _ in range(20):
        since_a = a.mark()
        table = frag_table(a)
        if len(table) >= 3:
            break
        wait_quietly(a, r"mp_frags: ", 3, since_a, "A's scoreboard")
    bots = sorted(slot for slot in table if slot not in (sa, sb))
    s.check("A adds a coop bot (impulse 100): three players listed at A", len(table) == 3 and len(bots) == 1,
            "%s" % table)
    if len(bots) != 1:
        return
    since_srv = srv.mark()
    srv.cmd("god; vr_dm_test 11")
    _, verdict = srv.wait(r"dmtest: \d+ passed, \d+ failed", 60, since_srv, "vr_dm_test 11's verdict")
    got = [l.strip() for l in srv.lines[since_srv:] if "dmtest: " in l]
    s.check("the coop bot kills a grunt brought in front of it, never taking a player for its enemy (vr_dm_test 11)",
            " 0 failed" in verdict, "%s %s" % (verdict.strip(), got[-5:]))
    inv0, _ = inventories_until(srv, lambda inv, m: sa in inv and sb in inv)
    hurt0 = {k: inv0.get(k, {}).get("health", 0) for k in (sa, sb)}

    since_a, since_b, since_srv = a.mark(), b.mark(), srv.mark()
    srv.cmd("vr_goto_exit %d" % sa)
    got = wait_quietly(srv, r"vr_goto_exit: .* in the exit to e1m2", 10, since_srv, "A in e1m1's exit")
    s.check("the server puts A in e1m1's exit (vr_goto_exit)", got is not None, "no 'vr_goto_exit: ... e1m2' line")
    a.cmd("vr_mock_button main trigger 1")
    ja = wait_quietly(a, r"mp_event: .*joined map=e1m2", 60, since_a, "A on e1m2")
    jb = wait_quietly(b, r"mp_event: .*joined map=e1m2", 60, since_b, "B on e1m2")
    a.cmd("vr_mock_button main trigger 0")
    s.check("A's trigger press ends the intermission: A and B on e1m2", ja is not None and jb is not None,
            "A %s; B %s" % (ja and ja[1], jb and jb[1]))
    if ja is None or jb is None:
        return
    back = wait_quietly(srv, r"coop: bot (.+) is back with what it had \(health \S+, items (\d+)\)", 30, since_srv,
                        "the bot back on e1m2")
    # (read after the "back" line: the server's log read up to it)
    kept = srv.find(r"coop: bot (.+)'s things kept for the next level \(health \S+, items (\d+)\)", since_srv)
    s.check("the bot's things kept at the level change (vr_coopsave.cpp VR_CoopNoteBots)", kept is not None,
            "no 'coop: bot ... kept' line")
    items = int(re.search(r"items (\d+)", back[1]).group(1)) if back else 0
    s.check("the bot is back on e1m2 with what it had: its super nailgun (items %d)" % items,
            back is not None and items & IT_SUPER_NAILGUN != 0, back[1].strip() if back else "no 'is back' line")
    table = {}
    for _ in range(20):
        since_a = a.mark()
        table = frag_table(a)
        if len(table) >= 3:
            break
        wait_quietly(a, r"mp_frags: ", 3, since_a, "A's scoreboard on e1m2")
    s.check("three players listed at A on e1m2 (the bot came along)", len(table) == 3, "%s" % table)
    s.note("A and B's health on e1m1 before the exit: %s (god on the server's QC test only)" % hurt0)


def flow(s):
    """The rest of the coop campaign flow (B4b), on a dedicated server: B put in e1m1's exit (vr_goto_exit): both see
    the intermission, A's trigger held goes on to e1m2 (QC's IntermissionThink: a press after its 2 seconds); there the
    level-start autosave, both players in it (autosave/coop~e1m2: Host_CheckCoopAutosave); B killed for real (vr_hurt:
    QC's T_Damage, not `kill`): dead on his client and the server, his trigger pressed respawns him with his level-start
    inventory of e1m2 (QC's respawn in coop: setspawnparms), not the rocket launcher he took after it."""
    srv, a, b, ea, eb = join(s)
    sa, sb = ea - 1, eb - 1
    # (the harness starts its games with autosaves off; e1m2's start has a monster in sight: waited for 2 seconds of
    # game time, not 60; the autosave's waits logged)
    srv.cmd("sv_autosave 1; vr_autosave_safe_maxdelay 2; developer 1")
    since_a, since_b, since_srv = a.mark(), b.mark(), srv.mark()
    srv.cmd("vr_goto_exit %d" % sb)
    got = wait_quietly(srv, r"vr_goto_exit: .* in the exit to e1m2", 10, since_srv, "B in e1m1's exit")
    s.check("the server puts B in e1m1's exit (vr_goto_exit)", got is not None, "no 'vr_goto_exit: ... e1m2' line")
    seen = {}
    for _ in range(60):
        seen = {"A": own_status(a), "B": own_status(b)}
        if all(int(v.get("intermission", "0")) > 0 for v in seen.values()):
            break
    s.check("A and B both see the intermission (their clients' cl.intermission)",
            all(int(v.get("intermission", "0")) > 0 for v in seen.values()),
            "A %s; B %s" % (seen["A"].get("intermission"), seen["B"].get("intermission")))
    s.check("A is told B exited the level", a.find(r"exited the level", since_a) is not None, "")
    # a save on the dedicated server in the intermission: refused by QC's state (it has no client of its own)
    since_srv = srv.mark()
    srv.cmd("save b4inter")
    got = wait_quietly(srv, r"Can't save in intermission|Saving game to", 10, since_srv, "the server's save answer")
    s.check("a save on the dedicated server in the intermission is refused (QC's intermission_running)",
            got is not None and "Can't save" in got[1], got[1].strip() if got else "no answer")

    # A's trigger held through the intermission's 2 seconds: on to e1m2, both
    a.cmd("vr_mock_button main trigger 1")
    ja = wait_quietly(a, r"mp_event: .*joined map=e1m2", 60, since_a, "A on e1m2")
    jb = wait_quietly(b, r"mp_event: .*joined map=e1m2", 60, since_b, "B on e1m2")
    a.cmd("vr_mock_button main trigger 0")
    s.check("A's trigger press ends the intermission: A and B on e1m2", ja is not None and jb is not None,
            "A %s; B %s" % (ja and ja[1], jb and jb[1]))
    if ja is None or jb is None:
        return
    t0 = time.time()

    # the level-start autosave, both in
    start, _ = inventories_until(srv, lambda inv, m: m == "e1m2" and sa in inv and sb in inv)
    got = wait_quietly(srv, r"Saving game to .*autosave/coop~e1m2", 30, since_srv, "the coop autosave of e1m2")
    path, text = saved_text(s, os.path.join("autosave", "coop~e1m2"), t0)
    slots_in = sorted(int(x) for x in re.findall(r"^// qvr_coopslot (\d+) 1 ", text, re.M))
    s.note("the autosave: %s; players in: %s" % (path, slots_in))
    s.check("e1m2's level-start autosave on the dedicated server (autosave/coop~e1m2), A and B in it",
            got is not None and path is not None and slots_in == sorted((sa, sb)),
            "save line %s; file %s; slots in %s" % (got and got[1], path, slots_in))
    # the Load Autosave menu's list (vr_autosave_list) has it for a host to load, marked a coop one with its players
    # (listed again while the server may still be closing it: a file open for writing is not listed)
    row = None
    for _ in range(5):
        since_a = a.mark()
        a.cmd("vr_autosave_list cold")
        wait_quietly(a, r"vr_autosave_list: \d+ autosaves", 10, since_a, "A's Load Autosave list")
        row = a.find(r"autosave \d+: autosave/coop~e1m2 \|.*Coop, 2 players", since_a)
        if row:
            break
    s.check("Load Autosave lists the coop autosave (coop~e1m2, Coop, 2 players)", row is not None,
            row[1].strip() if row else "not listed")

    # B takes a rocket launcher (not in his level-start parms), then dies for real
    b.cmd("give 7")
    inventories_until(srv, lambda inv, m: inv.get(sb, {}).get("items", 0) & IT_ROCKET_LAUNCHER)
    since_b, since_srv = b.mark(), srv.mark()
    srv.cmd("vr_hurt 1000 %d" % sb)
    dead = wait_quietly(srv, r"vr_hurt: .* dead", 10, since_srv, "B dead on the server")
    own = {}
    for _ in range(40):
        own = own_status(b)
        if int(own.get("health", "1")) <= 0:
            break
    s.check("B killed for real (vr_hurt 1000: T_Damage): dead on the server and on his client",
            dead is not None and int(own.get("health", "1")) <= 0, "server %s; B's client health %s" %
            (dead and dead[1], own.get("health")))
    s.check("B is not respawned at once (a real death: not kill's)", b.find(r"suicides", since_b) is None and
            int(own.get("health", "1")) <= 0, "")

    # his trigger pressed and let go until he is back (QC's PlayerDeathThink: all buttons up, then one down)
    back, presses = {}, 0
    for presses in range(1, 31):
        b.cmd("vr_mock_button main trigger 1")
        back, _ = inventories(srv)
        b.cmd("vr_mock_button main trigger 0")
        if back.get(sb, {}).get("health", 0) > 0:
            break
        back, _ = inventories(srv)
        if back.get(sb, {}).get("health", 0) > 0:
            break
    diff = carried(start.get(sb, {}), back.get(sb, {}), ("items", "shells", "nails", "rockets", "cells"), change=False)
    s.note("B respawned after %d presses: %s; his e1m2 start %s" % (presses, back.get(sb), start.get(sb)))
    s.check("B's trigger respawns him (coop: QC's respawn), with his e1m2 start inventory (no rocket launcher)",
            back.get(sb, {}).get("health", 0) > 0 and not diff and not back[sb]["items"] & IT_ROCKET_LAUNCHER,
            "respawned %s; start %s (differ: %s)" % (back.get(sb), start.get(sb), diff))
    own = own_status(b)
    s.check("B's client sees himself alive again", int(own.get("health", "0")) > 0, "health %s" % own.get("health"))


WENT = re.compile(r"mp_went: n=(\d+) m=(\S+) f=(-?\d+) sk=(-?\d+) o=(\S+) (\S+) (\S+)")
# (shells apart: a shotgun's magazine filled from them at a fresh spawn can come back in the reserve as well, MP_PLAN
# B15 open item; checked not fewer)
REJOIN_KEYS = ("health", "armor", "items", "shells", "nails", "rockets", "cells", "weapon")  # (B17: shells and health
#   exact -- a return is not a level change: no 25-shell floor, no 50-health floor)


def world_of(inst):
    """inst's world as its messages built it (vr_mp_world) -> ({entity: (model, frame, skin, (x, y, z))}, summary)."""
    since = inst.mark()
    inst.cmd("vr_mp_world")
    n, last = inst.wait(r"mp_world: ", 10, since, "%s's world" % inst.name)
    ents = {}
    for l in inst.lines[since:n]:
        m = WENT.match(l)
        if m:
            ents[int(m.group(1))] = (m.group(2), int(m.group(3)), int(m.group(4)), tuple(float(m.group(k)) for k in (5, 6, 7)))
    return ents, fields(last)


def names_until(srv, want, tries=60):
    """The server's players by name (vr_mp_status's mp_client lines) until each of `want` is in -> {name: [slots]}."""
    got = {}
    for _ in range(tries):
        got = {}
        for l in status(srv):
            if l.startswith("mp_client: "):
                f = fields(l)
                got.setdefault(f.get("name", ""), []).append(int(f["slot"]))
        if all(w in got for w in want):
            break
    return got


def rejoin(s):
    """(B15) In and out of a coop game in progress, on a dedicated server. The world changed (A's shotgun let fall on the
    floor, B given weapons and ammo and hurt), C joins: his world (each entity at rest in B's, its model, frame, skin
    and place; the precaches, static entities and light styles) is B's, who was there from the start. B leaves
    (disconnect) and joins again by his name: his old slot, with what he had (vr_mp_rejoin_keep); his suicide respawns
    him with his level-start things, not those. B's game crashes (killed) and comes back on another port: the server
    takes the same name from the same address, silent, as him back (his ghost closed at once, not after
    net_messagetimeout), his slot and things again. The server quits: C is told and is back out, still running."""
    srv, a, b, ea, eb = join(s)
    sa, sb = ea - 1, eb - 1
    a.cmd("name A")
    b.cmd("name B")
    names_until(srv, ("A", "B"))

    # The world changed before C comes: A's shotgun lying on the floor, B's gifts, B hurt (health 70)
    put_at(srv, a, ea, OPEN_SPOT, 90)
    grip_then_give(srv, a, "vr_mock_hand main 0.1 0.25 -0.35 0 0 0; vr_mock_button main grip 1; vr_mock_button off grip 0")
    weapon_until(s, srv, ea, 4)
    let_go_still(srv, a, "main")
    gun, spot = lying(srv, ea)
    srv.cmd("vr_debug_carry 0; developer 0")
    b.cmd("give 3; give 5; give s 44; give n 77")
    srv.cmd("vr_hurt 30 %d" % sb)
    inv, _ = inventories_until(srv, lambda inv, m: inv.get(sb, {}).get("nails") == 77 and
                               inv[sb].get("health") == 70 and sa in inv)
    s.note("the shotgun (entity %d) lies at %s; before: A %s, B %s" % (gun, spot, inv.get(sa), inv.get(sb)))

    # C joins the game in progress: at the coop start A left, beside B's (A between them, by the gun)
    c = s.client("C").start()
    c.cmd("name C; " + s.connect_cmd(c))
    _, jc = c.wait(r"mp_event: .*joined map=e1m1", 120, what="C joined e1m1")
    c.wait(r"mp_seen: .*ent=%d self=0" % eb, 20, what="C seeing B")
    wb1, sumb = world_of(b)
    wc, sumc = world_of(c)
    wb2, _ = world_of(b)
    rest = {n: e for n, e in wb1.items() if n in wb2 and wb2[n][:3] == e[:3] and
            max(abs(p - q) for p, q in zip(wb2[n][3], e[3])) < 0.5}
    differ, missing = [], []
    for n, e in sorted(rest.items()):
        o = wc.get(n)
        if o is None:
            missing.append((n, e[0]))
        elif o[:3] != e[:3] or max(abs(p - q) for p, q in zip(o[3], e[3])) > 1.0:
            differ.append((n, e, o))
    s.note("B's world: %d entities (%d at rest), C's %d; B %s; C %s" % (len(wb1), len(rest), len(wc), sumb, sumc))
    s.check("C, joining a game in progress, has every entity at rest in B's view as B has it (model, frame, skin, "
            "place within a unit)", len(rest) >= 3 and not differ and not missing,
            "%d at rest; differ %s; missing %s" % (len(rest), differ[:4], missing[:6]))
    s.check("C has the shotgun A let fall where B has it", gun in wc and gun in wb1 and wc[gun][0] == wb1[gun][0] and
            max(abs(p - q) for p, q in zip(wc[gun][3], wb1[gun][3])) <= 1.0,
            "B %s; C %s" % (wb1.get(gun), wc.get(gun)))
    s.check("C's precaches, static entities and light styles are B's",
            all(sumb.get(k) == sumc.get(k) for k in ("models", "sounds", "statics", "styles")),
            "B %s; C %s" % (sumb, sumc))

    # B leaves and joins again by his name: his slot, his things
    since_srv, since_b = srv.mark(), b.mark()
    b.cmd("disconnect")
    left = wait_quietly(srv, r"coop: B left", 20, since_srv, "the server keeping B's things")
    b.cmd("name B; " + s.connect_cmd(b))
    _, jb = b.wait(r"mp_event: .*joined map=e1m1", 120, since_b, "B back on e1m1")
    back = wait_quietly(srv, r"coop: B is back", 20, since_srv, "B given his things back")
    now, _ = inventories_until(srv, lambda inv, m: sb in inv and inv[sb].get("nails") == 77, 30)
    diff = carried(inv.get(sb, {}), now.get(sb, {}), REJOIN_KEYS, change=False)
    s.check("B, back after leaving, is in his old slot with what he had (weapons, ammo, health 70)",
            left is not None and back is not None and int(fields(jb)["self"]) == eb and not diff,
            "self %s (was %d); before %s; now %s (differ %s)" % (fields(jb).get("self"), eb, inv.get(sb),
                                                                 now.get(sb), diff))
    since_b = b.mark()
    b.cmd("kill")
    b.wait(r"suicides", 10, since_b, "B's suicide")
    fresh, _ = inventories_until(srv, lambda inv, m: sb in inv and inv[sb].get("nails") == 0 and inv[sb].get("health", 0) > 0, 20)
    s.check("B's suicide respawns him with his level-start things (a new player's on e1m1), not those he came back with",
            fresh.get(sb, {}).get("nails") == 0 and fresh[sb].get("health") == 100 and not fresh[sb]["items"] & 2,
            "%s" % fresh.get(sb))

    # (B25) Another game by B's name from his address while B's link up is silent over QVR_GHOST_SILENCE (a hitch, a
    # busy machine): not B back from a crash (its own vr_mp_clientkey), B's connection kept, the other given a slot of
    # its own. (By the name alone the server closed a live player's connection: the suite's joins under load, where
    # every harness game was "player".) (B25b: back in the default run. B25 split it out: B's return below came back
    # with health 0 and his shells twice: D's join had moved the coop starts' turn on, B's return took the start C
    # stood on and telefragged him, B then dying and taking C's backpack; coop spawns now skip a taken start,
    # VR_CoopSpawnPoint. `--part samename` stops after it.)
    rejoin_same_name(s, srv, b, ea, eb, sb)
    if PART == "samename":
        return

    # B's game crashes (killed behind his proxy: his old connection silent, no port unreachable to the server); he comes
    # back from another port, by the same name, with what he had (given again after his suicide)
    rejoin_crash(s, srv, a, b, c, ea, eb, sb)


def rejoin_same_name(s, srv, b, ea, eb, sb):
    """(B25) rejoin --part samename: another game by B's name while B is silent (see rejoin)."""
    b.proxy.blackout_up = True
    t0 = server_time(srv)
    server_time_past(srv, t0 + 4.0)
    since_srv = srv.mark()
    d = s.client("D")
    d.start()
    d.cmd("name B; " + s.connect_cmd(d))
    _, jd = d.wait(r"mp_event: .*joined map=e1m1", 120, what="the other game by B's name joined")
    b.proxy.blackout_up = False
    ghost = srv.find(r"B back from .* the old one closed", since_srv)
    since_b = b.mark()
    b.cmd("vr_mp_status")
    _, st_b = b.wait(r"mp_status: ", 30, since_b, "B's status after his link came back")
    srv_status = status(srv)
    s.check("another game by B's name from his address, his link silent 4 s: B's connection kept (not 'back from a "
            "crash': its own key), it given a slot of its own",
            ghost is None and int(fields(jd)["self"]) not in (ea, eb) and
            org_of(srv_status, "mp_client", "slot", sb) is not None and fields(st_b).get("connected") == "1",
            "ghost %s; the other's entity %s; B's slot on the server %s; B's status %s" % (
                ghost, fields(jd)["self"], org_of(srv_status, "mp_client", "slot", sb), st_b.strip()))
    # (renamed before it leaves: a coop leaver's things are kept by his name, and B's return below must find B's)
    d.cmd("name D")
    names_until(srv, ("A", "B", "C", "D"))
    d.cmd("disconnect")
    d.stop()
    s.instances.remove(d)


def rejoin_crash(s, srv, a, b, c, ea, eb, sb):
    """rejoin's last part: B's crash and return; the server quits."""
    # (hurt to 30 and his shells 17: under a level change's floors, 50 and 25, that a return must not apply)
    b.cmd("give 7; give r 9")
    srv.cmd("vr_hurt 70 %d" % sb)
    inv_b, _ = inventories_until(srv, lambda inv, m: sb in inv and inv[sb].get("rockets") == 9 and inv[sb].get("health") == 30)
    s.check("(setup) B's rocket launcher, 9 rockets and health 30 are his at the server before his crash",
            inv_b.get(sb, {}).get("rockets") == 9 and inv_b.get(sb, {}).get("health") == 30, "%s" % inv_b.get(sb))
    b.proc.kill()
    b.proc.wait()
    killed = time.monotonic()
    s.instances.remove(b)
    s.note("B's game killed (a crash): his old connection silent from now")
    b2 = s.client("B2")
    b2.clientkey = b.clientkey  # (B25: the same game, restarted: its vr_mp_clientkey kept in its config)
    b2.start()
    since = b2.mark()
    b2.cmd("vr_mp_status")
    b2.wait(r"mp_status: ", 120, since, "B's game started again")
    # (the server takes a connection silent over 3 s of real time as a crashed game's: QVR_GHOST_SILENCE, a real-time
    # threshold; B's game restarting takes longer, this only makes sure)
    time.sleep(max(0.0, 3.5 - (time.monotonic() - killed)))
    since_srv = srv.mark()
    b2.cmd("name B; " + s.connect_cmd(b2))
    _, jb = b2.wait(r"mp_event: .*joined map=e1m1", 120, what="B back on e1m1")
    ghost = wait_quietly(srv, r"B back from .* the old one closed", 10, since_srv, "B's ghost closed")
    back = wait_quietly(srv, r"coop: B is back", 10, since_srv, "B given his things back")
    now, _ = inventories_until(srv, lambda inv, m: sb in inv and inv[sb].get("rockets") == 9, 30)
    diff = carried(inv_b.get(sb, {}), now.get(sb, {}), REJOIN_KEYS, change=False)
    s.check("B, back from a crash on another port, takes his own slot at once (his silent old connection closed)",
            ghost is not None and int(fields(jb)["self"]) == eb,
            "%s; self %s (was %d)" % (ghost[1].strip() if ghost else "no 'back from' line", fields(jb).get("self"), eb))
    s.check("B, back from his crash, has what he had (health 30 and 17 shells: no level change's floors)",
            back is not None and not diff and inv_b.get(sb, {}).get("health") == 30,
            "before %s; now %s (differ %s)" % (inv_b.get(sb), now.get(sb), diff))
    srv.mark()
    tele = srv.find(r"telefragged")
    s.check("(B25b) no coop join or return telefragged a player standing at a start (VR_CoopSpawnPoint)", tele is None,
            tele[1].strip() if tele else "none")
    players = names_until(srv, ("A", "B", "C"))
    s.check("no ghost: A, B and C each in one slot", all(len(players.get(n, [])) == 1 for n in ("A", "B", "C")),
            "%s" % players)

    # The server quits: C is told, out of the game, his own still running
    since_c = c.mark()
    srv.cmd("quit")
    told = wait_quietly(c, r"mp_event: .* left", 30, since_c, "C told the server quit")
    own = own_status(c)
    s.check("the server quits: C is told and is out of the game, his game still running",
            told is not None and own.get("connected") == "0" and c.proc.poll() is None,
            "%s; connected %s" % (told[1].strip() if told else "not told", own.get("connected")))


def leave(srv, inst, name, what):
    """`inst` (named `name`) disconnects -> the server's "coop: <name> left" line (None: no record kept)."""
    since = srv.mark()
    inst.cmd("disconnect")
    srv.wait(r"Client %s removed" % name, 20, since, what)
    return wait_quietly(srv, r"coop: %s left" % name, 3, since, what + " (his things kept)")


def come_back(s, srv, inst, name, level, what):
    """`inst` connects as `name` -> (his entity, the server's "coop: <name> is back" line or None)."""
    since, since_srv = inst.mark(), srv.mark()
    inst.cmd("name %s; %s" % (name, s.connect_cmd(inst)))
    _, j = inst.wait(r"mp_event: .*joined map=%s" % level, 120, since, what)
    return int(fields(j)["self"]), wait_quietly(srv, r"coop: %s is back" % name, 5, since_srv, what + " (his things)")


def rejoin_keep(s):
    """(REVIEW_MP_2026-11-18 finding 8) The leavers' table (B15, vr_mp_rejoin_keep), on a dedicated server. A game under
    a leaver's name with another vr_mp_clientkey is a new player (E as B: none of B's things), and B's own game back gets
    them. B leaves with a rocket launcher, the level changes, he comes back: his suicide respawns him with it (his
    level-start parms what he carried into the level, not the last level's start). 17 names leave: the oldest record
    makes room for the 17th, who comes back with his things (before: the 17th was not kept, the feature stopped). In a
    level loaded from a coop save B leaves and comes back with his things (before: a new player for that whole level)."""
    srv, a, b, ea, eb = join(s)
    sa, sb = ea - 1, eb - 1
    a.cmd("name A")
    b.cmd("name B")
    names_until(srv, ("A", "B"))

    # Another game by B's name (its own key): a new player; then B's own, his things
    b.cmd("give 5; give n 77")
    inventories_until(srv, lambda inv, m: inv.get(sb, {}).get("nails") == 77)
    left = leave(srv, b, "B", "B leaving")
    e = s.client("E").start()
    ee, back = come_back(s, srv, e, "B", "e1m1", "E (named B) joining")
    inv, _ = inventories_until(srv, lambda inv, m: ee - 1 in inv, 20)
    s.check("key: another game under B's name (another vr_mp_clientkey) is a new player, not B back",
            left is not None and back is None and inv.get(ee - 1, {}).get("nails") == 0,
            "B's leave %s; %s; E's things %s" % (left and left[1].strip(), back[1].strip() if back else "not 'back'",
                                                 inv.get(ee - 1)))
    e.cmd("name E")
    names_until(srv, ("A", "E"))
    leave(srv, e, "E", "E leaving")
    e.stop()
    s.instances.remove(e)
    eb2, back = come_back(s, srv, b, "B", "e1m1", "B back")
    inv, _ = inventories_until(srv, lambda inv, m: eb2 - 1 in inv and inv[eb2 - 1].get("nails") == 77, 20)
    s.check("key: B's own game back has his slot and things", back is not None and eb2 == eb and
            inv.get(sb, {}).get("nails") == 77, "%s; entity %d (was %d); %s" % (
                back[1].strip() if back else "not 'back'", eb2, eb, inv.get(sb)))

    # A level change while B is away: his return's respawn gives what he carried into the level
    b.cmd("give 7; give r 9")
    inventories_until(srv, lambda inv, m: inv.get(sb, {}).get("items", 0) & IT_ROCKET_LAUNCHER)
    left = leave(srv, b, "B", "B leaving on e1m1")
    since_a = a.mark()
    srv.cmd("changelevel e1m2")
    a.wait(r"mp_event: .*joined map=e1m2", 120, since_a, "A on e1m2")
    eb2, back = come_back(s, srv, b, "B", "e1m2", "B back on e1m2")
    inv, _ = inventories_until(srv, lambda inv, m: m == "e1m2" and inv.get(sb, {}).get("items", 0) & IT_ROCKET_LAUNCHER,
                               20)
    since_b = b.mark()
    b.cmd("kill")
    b.wait(r"suicides", 10, since_b, "B's suicide")
    fresh, _ = inventories_until(srv, lambda inv, m: inv.get(sb, {}).get("health", 0) > 0 and
                                 inv[sb].get("items", 0) & IT_ROCKET_LAUNCHER, 20)
    s.check("level change: B, away across it, respawns after his return with what he carried into e1m2 (his rocket "
            "launcher: his level-start parms refreshed), not e1m1's start",
            left is not None and back is not None and fresh.get(sb, {}).get("items", 0) & IT_ROCKET_LAUNCHER and
            fresh[sb].get("rockets") == 9, "back %s; respawned %s" % (back is not None, fresh.get(sb)))

    # 17 names leave (B's record taken back above; E's is the oldest): the 17th's record replaces the oldest
    d = s.client("D").start()
    for i in range(1, 17):
        ed, _ = come_back(s, srv, d, "D%d" % i, "e1m2", "D%d joining" % i)
        if i == 16:
            d.cmd("give 5; give n 55")
            inventories_until(srv, lambda inv, m: inv.get(ed - 1, {}).get("nails") == 55)
        since = srv.mark()
        kept = leave(srv, d, "D%d" % i, "D%d leaving" % i)
    dropped = srv.find(r"coop: what E had .* is forgotten for D16's", since)
    ed, back = come_back(s, srv, d, "D16", "e1m2", "D16 back")
    inv, _ = inventories_until(srv, lambda inv, m: inv.get(ed - 1, {}).get("nails") == 55, 20)
    s.check("table: the 17th leaver's record takes the oldest's place (E's), and he comes back with his things",
            kept is not None and dropped is not None and back is not None and inv.get(ed - 1, {}).get("nails") == 55,
            "kept %s; %s; back %s; D16 %s" % (kept is not None, dropped[1].strip() if dropped else "no eviction line",
                                              back is not None, inv.get(ed - 1)))
    leave(srv, d, "D16", "D16 leaving again")
    d.stop()
    s.instances.remove(d)

    # A level loaded from a coop save (A and B kept): B leaves and comes back with what he had
    since, since_a, since_b = srv.mark(), a.mark(), b.mark()
    srv.cmd("save keeptest")
    srv.wait(r"Saving game to", 10, since, "the server's save")
    srv.cmd("load keeptest")
    a.wait(r"mp_event: .*joined map=e1m2", 120, since_a, "A in the loaded game")
    b.wait(r"mp_event: .*joined map=e1m2", 120, since_b, "B in the loaded game")
    b.cmd("give 6; give n 66")
    inventories_until(srv, lambda inv, m: inv.get(sb, {}).get("nails") == 66)
    left = leave(srv, b, "B", "B leaving the loaded level")
    eb2, back = come_back(s, srv, b, "B", "e1m2", "B back in the loaded level")
    inv, _ = inventories_until(srv, lambda inv, m: inv.get(eb2 - 1, {}).get("nails") == 66, 20)
    s.check("loaded: in a level loaded from a coop save B, back after leaving, has his slot and things",
            left is not None and back is not None and eb2 == eb and inv.get(sb, {}).get("nails") == 66,
            "left %s; back %s; entity %d (was %d); %s" % (left is not None, back is not None, eb2, eb, inv.get(sb)))


DL_TITLE = "QVR MP Download"
DL_MAP = "qvrmpdl"  # the package's one map (Quake VR's own vrexample.bsp under another name: no id asset in it)
INDEX_URL = "https://www.quaddicted.com/api/v1/"  # the default vr_maps_index_url (a test run reads its cache only)


def dl_package(s, port):
    """The test's map package: a zip of maps/qvrmpdl.bsp in the run's folder, served on 127.0.0.1:port; the server's and
    B's own user base dirs (their last -basedir: cache/, qvr_addons/) given a map index that lists it -> (sha256, the
    user dirs, the HTTP server)."""
    import hashlib
    import http.server
    import socketserver
    import threading
    import zipfile
    serve = os.path.join(s.out, "www")
    os.makedirs(serve, exist_ok=True)
    zpath = os.path.join(serve, "qvrmpdl.zip")
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        z.write(os.path.join(REPO, "quakevr", "maps", "vrexample.bsp"), "maps/%s.bsp" % DL_MAP)
    with open(zpath, "rb") as f:
        sha = hashlib.sha256(f.read()).hexdigest()
    # sha title author date types modes sizes themes bytes startmap extract progs urls description files rating userRating
    line = "\t".join([sha, DL_TITLE, "Quake VR test", "2026-11-14", "map", "coop", "small", "",
                      str(os.path.getsize(zpath)), DL_MAP, "{base}/", "0", "http://127.0.0.1:%d/qvrmpdl.zip" % port,
                      "mp_download's package", "1", "0", "0"])
    users = {}
    for who in ("server", "B"):
        users[who] = os.path.join(s.out, who + "_user")
        os.makedirs(os.path.join(users[who], "cache"), exist_ok=True)
        with open(os.path.join(users[who], "cache", "maps_index.txt"), "w", newline="\n") as f:
            f.write("\n".join(["#quakevr-mapindex-3", "#" + INDEX_URL, "#" + str(int(time.time())), line]) + "\n")

    class Quiet(http.server.SimpleHTTPRequestHandler):
        def log_message(self, *a):
            pass

    socketserver.ThreadingTCPServer.allow_reuse_address = True
    httpd = socketserver.ThreadingTCPServer(("127.0.0.1", port), lambda *a, **k: Quiet(*a, directory=serve, **k))
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    s.note("the package %s (sha %s) served on 127.0.0.1:%d" % (DL_TITLE, sha[:12], port))
    return sha, users, httpd


def download(s):
    """Map download (B4c): a dedicated server plays a Map Library package's map (installed by A's maps_install in the
    user dir they share); a client that has the package (A) mounts it and joins again; a client that lacks it
    (B, his own user dir) is told what to install with vr_mp_download 0, and with 1 installs it through the map
    installer (his own index, its mirror, its checks: the server sends nothing but the package's sha256), mounts it and
    joins. A leaves before B joins (the test map has one spawn spot)."""
    sha, users, httpd = dl_package(s, s.take_port())
    try:
        # (A installs it in the user dir he shares with the server: a dedicated server reads no map index)
        a = s.client("A", extra=["-basedir", users["server"], "+vr_maps_allow_local", "1"]).start()
        # (a fresh user dir: its caches cold, the game's start takes about a minute, its shaders compiled again)
        a.wait(r"map index: [0-9]+ packages", 240, what="A's map index (its cache) read")
        a.cmd("maps_install %s" % sha[:16])
        a.wait(r"%s - \d+ file\(s\) written" % DL_TITLE, 60, what="A's install of the package")
        srv = s.server(dedicated=True)
        srv.extra = ["-basedir", users["server"]]
        srv.start()
        srv.cmd("coop 1; deathmatch 0; skill 1; map %s; vr_mp_status" % DL_MAP)
        srv.wait(r"mp_server: map=%s" % DL_MAP, 120, what="the server on the package's map")

        a.cmd(s.connect_cmd(a))
        _, why = a.wait(r"mp_package: .*installed", 60, what="A told the server's package")
        a.wait(r"mp_event: .*joined map=%s" % DL_MAP, 120, what="A joined the package's map")
        s.check("A (the package installed) mounts it and joins the server's map", True, why.strip())
        since = a.mark()
        a.cmd("disconnect")
        a.wait(r"mp_event: .*left", 20, since, "A left")

        b = s.client("B", extra=["-basedir", users["B"], "+vr_maps_allow_local", "1", "+vr_mp_download", "0"]).start()
        b.wait(r"map index: [0-9]+ packages", 240, what="B's map index (its cache) read")  # (else he would fetch Quaddicted's)
        since = b.mark()
        b.cmd(s.connect_cmd(b))
        _, why = b.wait(r"mp_package: .*vr_mp_download 0", 60, since, "B refused with vr_mp_download 0")
        s.check("B (lacking it, vr_mp_download 0) is told what to install, nothing downloaded",
                b.find(r"mp_event: .*joined", since) is None and
                not os.path.isdir(os.path.join(users["B"], "qvr_addons", sha[:16])), why.strip())
        since = b.mark()
        b.cmd("vr_mp_download 1; " + s.connect_cmd(b))
        _, wrote = b.wait(r"%s - \d+ file\(s\) written" % DL_TITLE, 60, since, "B's install of the package")
        _, again = b.wait(r"mp_package: .* mounted; joining again", 30, since, "B joining again")
        b.wait(r"mp_event: .*joined map=%s" % DL_MAP, 120, since, "B joined the package's map")
        s.check("B (vr_mp_download 1) installs the package through the map installer, then joins", True,
                "%s; %s" % (wrote.strip(), again.strip()))
        bsp = os.path.join(users["B"], "qvr_addons", sha[:16], "maps", DL_MAP + ".bsp")
        s.check("B's copy is in his own package folder, as the Map Library installs it", os.path.isfile(bsp), bsp)
        spawned = [l for l in status(srv) if re.match(r"mp_client: .*spawned=1", l)]
        s.check("the server has B in its game", len(spawned) == 1, "; ".join(spawned))
    finally:
        httpd.shutdown()


OPEN_SPOT = (480.0, -296.0, 72.0)  # e1m1: between the coop starts (432/528 -296), the way north open ahead (yaw 90)
COOP_SPOTS = ((432.0, -296.0, 72.0), (528.0, -296.0, 72.0))  # e1m1's coop starts (the single player start, on its
#                                                                 step, put a player there stuck: SV_CheckStuck moved him)


def side_spot(srv, other):
    """Out of OPEN_SPOT's way: the coop start farther from player `other` (who may still stand on his)."""
    at = org_of(status(srv), "mp_client", "slot", other - 1) or [0.0, 0.0, 0.0]
    return max(COOP_SPOTS, key=lambda p: math.hypot(p[0] - at[0], p[1] - at[1]))


THROW_SETTLE_S = 0.5  # game seconds a hand is held still before it lets go (past vr_throw's window of samples)


def put_at(srv, inst, ent, at, yaw=None, tries=60):
    """`inst`'s player put at `at` (setpos: it turns noclip on; "noclip" turns it off again), facing `yaw` if given,
    until the server has him within 4 units of it across -> his origin there (a definite end: `tries` reports)."""
    inst.cmd("setpos %.1f %.1f %.1f%s; noclip" % (at[0], at[1], at[2], "" if yaw is None else " 0 %.1f 0" % yaw))
    got = None
    for _ in range(tries):
        got = org_of(status(srv), "mp_client", "slot", ent - 1)
        if got and math.hypot(got[0] - at[0], got[1] - at[1]) <= 4:
            return got
    raise StepFailed("%s never put at %s (the server has him at %s)" % (inst.name, at, got))


LYING_REACH = 96.0  # (B22) a weapon let fall lies within this of the hand that held it (a gentle drop: ~30 units)


def lying(srv, ent, tries=100):
    """The weapon lying nearest player `ent`'s main hand on the server, at rest in two reports running ->
    (its entity, its origin) (a definite end: `tries` reports)."""
    still, g = 0, {}
    for _ in range(tries):
        g = ground_report(srv, "mp_drawn_sv").get((ent, 1), {})
        # (a weapon: the reports before the let-go reached the server say none, nspeed -1 -- B16's mp_rejoin flake took
        # "entity 0 at the origin" for the shotgun at rest when the suite's load delayed the let-go. B22: and the hand
        # empty, the weapon by it -- before the let-go reached the server, the nearest lying weapon was another one,
        # anywhere on the map (an armed grunt's, 2600 units off: B21's mp_rejoin "C has the shotgun" flake))
        near = g.get("norg") and g.get("hand") and math.dist(g["norg"], g["hand"]) <= LYING_REACH
        still = still + 1 if g.get("rev", (0,))[0] > 0 and near and g.get("drawn", 1) == 0 and \
            0.0 <= g.get("nspeed", -1.0) < 0.5 else 0
        if still >= 2:
            return g["rev"][0], g["norg"]
    raise StepFailed("no weapon at rest by player %d's hand: %s" % (ent, g))


def hand_over(s, srv, giver, taker, eg, et, give):
    """The giver (at OPEN_SPOT) lets his shotgun fall from his opened main hand (given first if `give`); the taker, put
    beside it, closes his empty main hand on it: the server puts it in the taker's hand by his own grip and fist."""
    label = "%s to %s" % (giver.name, taker.name)
    low = "vr_mock_hand main 0.1 0.25 -0.35 0 0 0"
    put_at(srv, giver, eg, OPEN_SPOT, 90)
    if give:
        grip_then_give(srv, giver, "%s; vr_mock_button main grip 1; vr_mock_button off grip 0" % low)
    else:
        giver.cmd(low)
    held = weapon_until(s, srv, eg, 4)
    # (the hand still a moment before it opens: the mock reports a hand put elsewhere in one frame as moving that fast,
    # and a release within the throw's window threw the gun 500 units: B to A, 1 run of 3)
    server_time_past(srv, server_time(srv) + THROW_SETTLE_S)
    giver.cmd("vr_mock_button main grip 0")
    gone = weapon_until(s, srv, eg, 0)
    s.check("%s: %s's shotgun falls from his opened hand" % (label, giver.name), held[-1] == 4 and gone[-1] == 0,
            "%s's main weapon by report: %s, then %s" % (giver.name, held, gone))
    gun, spot = lying(srv, eg)
    at = org_of(status(srv), "mp_client", "slot", eg - 1)
    d = [spot[0] - at[0], spot[1] - at[1]]
    n = math.hypot(d[0], d[1]) or 1.0
    beside = (spot[0] + d[0] / n * 24, spot[1] + d[1] / n * 24, at[2])
    put_at(srv, taker, et, beside)
    s.note("%s: the shotgun (entity %d) lies at %s; %s put at %s" % (label, gun, spot, taker.name, beside))

    # The taker's empty main hand pressed and let go once (the server's let-go that retake starts from), then over the
    # gun's grip and onto it with the grip pressed (retake: up to 3 presses).
    taker.cmd("vr_mock_hand main 0.1 0.6 -0.2 0 0 0; vr_mock_button main grip 0")
    since = srv.mark()
    taker.cmd("vr_mock_button main grip 1")
    srv.wait(r"grip: %s's main hand pressed at" % taker.name, 10, since, "%s's grip on the server" % taker.name)
    since = srv.mark()
    taker.cmd("vr_mock_button main grip 0")
    line = retake(srv, taker, "main", since, r"weapon: .* taken into the main hand", "vr_mock_hand_to main spot 0 6",
                  "vr_mock_hand_to main spot 0")
    s.note("%s: the server's take: %s" % (label, line))
    took = weapon_until(s, srv, et, 4)
    left = weapon_until(s, srv, eg, 0, 1)
    s.check("%s: %s's grip takes the shotgun %s let go of (the server's: in his main hand; %s's still empty)" % (
        label, taker.name, giver.name, giver.name), line is not None and took[-1] == 4 and left[-1] == 0,
        "%s's main weapon by report: %s; %s's: %s" % (taker.name, took, giver.name, left))


def pickup(s):
    """A weapon dropped by one player and picked up by the other, each way: A (given a shotgun) opens his hand, it falls;
    B, put beside it, closes his hand on it: his. Then B lets it fall and A takes it back."""
    srv, a, b, ea, eb = join(s)
    a.cmd("name A")
    b.cmd("name B")
    hand_over(s, srv, a, b, ea, eb, True)
    put_at(srv, a, ea, side_spot(srv, eb))
    hand_over(s, srv, b, a, eb, ea, False)
    srv.cmd("vr_debug_carry 0; developer 0")


# attach5 (his words 2026-11-26: "Attachments should also be obviously synced in multiplayer"): the bayonet on B's shotgun as
# A (the listen host) and C draw it, by their vr_attach_report (vr_view.cpp: every attachment drawn last frame).
ATTACH_STAB_DIST = 42.0  # units: the grunt spawned ahead of B for V's thrust (vr_attach_test.qc test 80's)
ATTACH_TRIES = 30        # reports before a drawn bayonet counts as missing (a definite end)
ATTACH_HOLSTER_IMPULSE = 74  # flat holster menu: holster 4 (left upper) <-> the right hand
ATTACH_DRAW_IMPULSE = 71     # flat holster menu: holster 3 (right hip: his start's shotgun) <-> the right hand
ATTACH_THROW_HOLD = 0.6      # game seconds C holds the flat throw key down: a throw, not a drop
ATTACH_THROW_KEY = "b"       # vr_flat_key_throw as shipped
ATTACH_GRAB_GAP = 10.0       # units north of the dropped gun C stands, over it (H reaches vr_flat_grab_reach, 0.65 m: 20 units, level from his eyes)
ATTACH_GRAB_SIDES = ((0, 1), (1, 0), (-1, 0), (0, -1), (0, 1), (1, 0))  # C over the gun from the north, east, west, south in turn
ATTACH_GRAB_PITCH = 60.0     # degrees C looks down at it (the grab's cone is round his aim)
ATTACH_THROWN_MIN = 64.0     # units from C a thrown gun comes to rest at least (a drop falls by his feet)
ATTACH_DM_C_SPOT = (OPEN_SPOT[0] + 64, OPEN_SPOT[1] + 21, OPEN_SPOT[2])  # C beside them, in sight
ATTACH_NEAR = 48.0       # units: a lying gun's drawn bayonet within this of the server's gun (its mount is on the gun)


def attach_drawn(inst):
    """vr_attach_report on `inst` -> [{"where", "ent", "kind", "gun", "at"}] (the attachments it drew last frame)."""
    since = inst.mark()
    inst.cmd("vr_attach_report")
    n, _ = inst.wait(r"attach_report: \d+ drawn", 10, since, "%s's drawn attachments" % inst.name)
    out = []
    for l in inst.lines[since:n]:
        m = re.match(r"attach_drawn: where=(\w+) ent=(-?\d+) kind=(\d+) gun=(\S+) at=([-\d.]+) ([-\d.]+) ([-\d.]+)", l)
        if m:
            out.append({"where": m.group(1), "ent": int(m.group(2)), "kind": int(m.group(3)), "gun": m.group(4),
                        "at": [float(m.group(i)) for i in (5, 6, 7)]})
    return out


def attach_server(srv):
    """vr_dm_test 26 (vr_attach_test.qc) -> ({player ent: {"main": (wid, kind), "off": .., "holsters": [kinds]}},
    [{"wid", "attach", "stuck", "speed", "at"}] the thrown weapons, [{"class", "health"}] the monsters near players)."""
    since = srv.mark()
    srv.cmd("vr_dm_test 26")
    n, _ = srv.wait(r"attachmp: report done", 10, since, "the server's attachment report")
    players, lying, monsters = {}, [], []
    for l in srv.lines[since:n]:
        m = re.match(r"attachmp: player ent=(\d+) main=(-?\d+),(\d+) off=(-?\d+),(\d+) holsters=([\d,]+)", l)
        if m:
            players[int(m.group(1))] = {"main": (int(m.group(2)), int(m.group(3))),
                                        "off": (int(m.group(4)), int(m.group(5))),
                                        "holsters": [int(x) for x in m.group(6).split(",")]}
        m = re.match(r"attachmp: lying wid=(-?\d+) attach=(\d+) stuck=(\d) speed=(\d+) at=([-\d]+) ([-\d]+) ([-\d]+)", l)
        if m:
            lying.append({"wid": int(m.group(1)), "attach": int(m.group(2)), "stuck": int(m.group(3)),
                          "speed": int(m.group(4)), "at": [float(m.group(i)) for i in (5, 6, 7)]})
        m = re.match(r"attachmp: monster ent=\d+ class=(\S+) health=(-?[\d.]+)", l)
        if m:
            monsters.append({"class": m.group(1), "health": float(m.group(2))})
    return players, lying, monsters


def attach_seen_until(s, viewers, want, what, tries=ATTACH_TRIES):
    """Each viewer's vr_attach_report until want(viewer, its drawn list) holds for all -> True; the check fails after
    `tries` rounds (a definite end)."""
    last = {}
    show = lambda: "; ".join("%s: %s" % (k, [(d["where"], d["ent"], d["kind"], [round(x) for x in d["at"]]) for d in x])
                             for k, x in last.items())
    for _ in range(tries):
        ok = True
        for v in viewers:
            last[v.name] = attach_drawn(v)
            ok = ok and want(v, last[v.name])
        if ok:
            s.check(what, True, show())
            return True
    s.check(what, False, "not after %d reports: %s" % (tries, show()))
    return False


def has_drawn(where, ent=None, near=None, kind=1):
    """A want() for attach_seen_until: a bayonet drawn `where`, on entity `ent` (if given), within ATTACH_NEAR of `near`
    (a server's gun origin, if given)."""
    return lambda v, drawn: any(d["where"] == where and d["kind"] == kind and (ent is None or d["ent"] == ent) and
                                (near is None or math.dist(d["at"], near) <= ATTACH_NEAR) for d in drawn)


def attach_server_until(srv, done, tries=ATTACH_TRIES):
    """attach_server until done(players, lying, monsters) -> its last report (a definite end: `tries` reports)."""
    got = attach_server(srv)
    for _ in range(tries - 1):
        if done(*got):
            break
        got = attach_server(srv)
    return got


def attach_arm(s, srv, b, eb, label, draw="impulse 154"):
    """B (flat): a shotgun in his main hand (`draw`: the test give, impulse 154; kept if he has one), a bayonet in his off
    hand (vr_dm_test 25), J (impulse 52): the server's record has it -> True."""
    if weapon_until(s, srv, eb, 4, 1)[-1] != 4:
        b.cmd(draw)
    held = weapon_until(s, srv, eb, 4)
    since = srv.mark()
    srv.cmd("vr_dm_test_who %d; vr_dm_test 25" % eb)
    got = wait_quietly(srv, r"attachmp: (bayonet in|no) player", 10, since, "the bayonet given")
    b.cmd("impulse 52")
    pl, _, _ = attach_server_until(srv, lambda p, l, m: p.get(eb, {}).get("main") == (4, 1))
    ok = pl.get(eb, {}).get("main") == (4, 1)
    s.check("%s: B (flat) holds the shotgun, is given a bayonet in his off hand (vr_dm_test 25) and J (impulse 52) puts it "
            "on (the server's record)" % label, held[-1] == 4 and got is not None and "bayonet in" in got[1] and ok,
            "main weapon by report %s; %s; B on the server %s" % (held, got[1].strip() if got else "no attachmp line",
                                                                    pl.get(eb)))
    return ok


def attach(s):
    """(attach5) The bayonet in multiplayer: a listen host A (VR), B and C (flat) in coop e1m1. B puts a bayonet on his
    shotgun (vr_dm_test 25 gives it, J: impulse 52); A and C draw it (vr_attach_report) on his gun held, holstered (impulse
    74: the left upper holster <-> his right hand) and drawn again, and lying when he drops it (impulse 79); C takes it
    with H, looking down at it (A and B draw it on C's gun, the server's record still has it); C throws it (the throw key
    held: A and B draw it where it comes to rest). B armed again: his V thrust sticks the gun in a grunt by the bayonet's
    point (hurt; A and C draw it on the stuck gun). Then deathmatch (changelevel): B draws his holstered shotgun, a bayonet
    on it; the server puts A in front of him (vr_dm_test 27): his thrust hurts A."""
    srv, a, b, ea, eb = join(s, listen=True, flat="B")
    c = s.client("C", flat=True).start()
    c.cmd("name C; " + s.connect_cmd(c))
    _, jc = c.wait(r"mp_event: .*joined map=e1m1", 120, what="C joined e1m1")
    ec = int(fields(jc)["self"])
    a.wait(r"mp_seen: .*ent=%d self=0" % ec, 20, what="A seeing C")
    a.cmd("name A")
    b.cmd("name B")
    srv.cmd("developer 1; vr_debug_carry 2")  # (the server's grip log: C's take)
    others = (a, c)

    # Held.
    attach_arm(s, srv, b, eb, "held")
    attach_seen_until(s, [b], has_drawn("hand_main"), "B draws the bayonet on the shotgun in his own hand")
    attach_seen_until(s, others, has_drawn("other_held", eb), "held: A and C draw the bayonet on B's shotgun")

    # Holstered (the flat holster menu's 60 + 3 * 4 + 2: the left upper holster, empty at the start, and his right hand
    # trade; the shoulders' are not drawn, the hips' hold his axe and shotgun), then drawn again.
    b.cmd("impulse %d" % ATTACH_HOLSTER_IMPULSE)
    pl, _, _ = attach_server_until(srv, lambda p, l, m: 1 in p.get(eb, {}).get("holsters", []))
    s.check("holstered: the server's record in B's holster keeps the bayonet", 1 in pl.get(eb, {}).get("holsters", []),
            "B on the server: %s" % pl.get(eb))
    if not attach_seen_until(s, others, has_drawn("other_holster", eb),
                             "holstered: A and C draw the bayonet on B's holstered shotgun"):
        (la,) = reports(s, [a])
        s.note("A's holstered guns of B: %s" % [l.strip() for l in la if l.startswith("mp_holster: ent=%d " % eb)])
    b.cmd("impulse %d" % ATTACH_HOLSTER_IMPULSE)
    attach_server_until(srv, lambda p, l, m: p.get(eb, {}).get("main") == (4, 1))
    attach_seen_until(s, others, has_drawn("other_held", eb), "drawn again: A and C draw the bayonet on B's shotgun")

    # Dropped (the holster menu's 79: the right hand's).
    put_at(srv, b, eb, OPEN_SPOT, 90)
    b.cmd("impulse 79")
    gone = weapon_until(s, srv, eb, 0)
    gun, spot = lying(srv, eb)
    _, ly, _ = attach_server(srv)
    dropped = [x for x in ly if x["attach"] == 1 and math.dist(x["at"], spot) <= 8]
    s.check("dropped: B's shotgun lies on the floor, its record's bayonet on it", gone[-1] == 0 and bool(dropped),
            "B's main weapon %s; lying %s (the gun at %s)" % (gone, ly, spot))
    attach_seen_until(s, [a, b, c], has_drawn("lying", near=spot), "dropped: A, B and C draw the bayonet on the lying shotgun")

    # C (flat) takes it with H (the grab key) into his free hand; A and B draw it on his gun.
    put_at(srv, b, eb, (OPEN_SPOT[0], OPEN_SPOT[1] + 120, OPEN_SPOT[2]))  # (out of the way: the way north is open)
    # (H reaches from his eyes along his aim, VR_FlatGrab_FindTarget: level within vr_flat_grab_reach, down to the floor at
    # his feet, inside its cone: so he stands over it looking down at it)
    took = []
    for dx, dy in ATTACH_GRAB_SIDES:  # (a definite end: one press of the grab toggle from each side of it)
        # (where it lies now: placing him beside it can nudge it)
        _, ly, _ = attach_server(srv)
        gun_at = next((x["at"] for x in ly if x["attach"] == 1), spot)
        at = (gun_at[0] + dx * ATTACH_GRAB_GAP, gun_at[1] + dy * ATTACH_GRAB_GAP, OPEN_SPOT[2])
        yaw = math.degrees(math.atan2(-dy, -dx))
        put_at(srv, c, ec, at, yaw)
        c.cmd("setpos %.1f %.1f %.1f %g %.1f 0; noclip" % (at[0], at[1], at[2], ATTACH_GRAB_PITCH, yaw))
        server_time_past(srv, server_time(srv) + 0.1)  # (the setpos and noclip toggles done first: none in noclip)
        c.cmd("impulse 96")  # (flat H: a loose weapon in reach taken into a free hand, vr_flatgrab.qc)
        took += weapon_until(s, srv, ec, 4, 10)
        if took[-1] == 4:
            break
    pl, _, _ = attach_server_until(srv, lambda p, l, m: p.get(ec, {}).get("main") == (4, 1))
    s.check("C picks up the dropped shotgun and it still carries the bayonet (the server's record)",
            took[-1] == 4 and pl.get(ec, {}).get("main") == (4, 1), "C's main weapon %s; C on the server %s" % (took, pl.get(ec)))
    attach_seen_until(s, [a, b], has_drawn("other_held", ec), "A and B draw the bayonet on the shotgun in C's hand")
    attach_seen_until(s, [c], has_drawn("hand_main"), "C draws the bayonet on the shotgun in his own hand")

    # C throws it (the flat throw key, B, held down, then let go: the key itself, as his client sends 40 while it is up).
    put_at(srv, c, ec, OPEN_SPOT, 90)
    c.cmd("vr_mock_key %s down" % ATTACH_THROW_KEY)
    server_time_past(srv, server_time(srv) + ATTACH_THROW_HOLD)
    c.cmd("vr_mock_key %s up" % ATTACH_THROW_KEY)
    # (the gun gone from his hand, then at rest: thrown, it lies well ahead of where he stands, not at his feet)
    pl, _, _ = attach_server_until(srv, lambda p, l, m: p.get(ec, {}).get("main", (4,))[0] == 0)
    _, ly, _ = attach_server_until(srv, lambda p, l, m: any(x["attach"] == 1 for x in l) and
                                   all(x["speed"] == 0 for x in l if x["attach"] == 1))
    thrown = [x for x in ly if x["attach"] == 1 and math.hypot(x["at"][0] - OPEN_SPOT[0], x["at"][1] - OPEN_SPOT[1]) >= ATTACH_THROWN_MIN]
    s.check("thrown: C's shotgun leaves his hand and comes to rest well ahead, the record on the thrown gun keeps the bayonet",
            pl.get(ec, {}).get("main", (4,))[0] == 0 and bool(thrown), "C on the server %s; thrown weapons %s" % (pl.get(ec), ly))
    if thrown:
        attach_seen_until(s, [a, b], has_drawn("lying", near=thrown[0]["at"]), "thrown: A and B draw the bayonet on C's thrown shotgun")

    # Stuck: B armed again; a grunt spawned ahead of him (impulse 241, coop's one test impulse); V's thrust.
    put_at(srv, c, ec, (OPEN_SPOT[0], OPEN_SPOT[1] + 72, OPEN_SPOT[2]))  # (off the spot first)
    put_at(srv, b, eb, OPEN_SPOT, 90)
    attach_arm(s, srv, b, eb, "again")
    srv.cmd("vr_test_spawn 0; vr_test_spawn_dist %g" % ATTACH_STAB_DIST)
    b.cmd("impulse 241")
    _, _, mon = attach_server_until(srv, lambda p, l, m: len(m) > 0)
    before = min((x["health"] for x in mon), default=None)
    since = srv.mark()
    b.cmd("vr_flat_punch v")
    by_b = lambda x: x["stuck"] and x["attach"] == 1 and math.hypot(x["at"][0] - OPEN_SPOT[0], x["at"][1] - OPEN_SPOT[1]) <= 96
    pl, ly, mon = attach_server_until(srv, lambda p, l, m: any(by_b(x) for x in l))  # (not the thrown gun, stuck elsewhere)
    stuck = [x for x in ly if by_b(x)]
    after = min((x["health"] for x in mon), default=None)
    s.check("V: B's bayonet stab hurts the grunt and the shotgun sticks in it by the bayonet, the bayonet on its record",
            bool(stuck) and before is not None and (after is None or after < before),
            "stuck guns: %s; grunt health %s -> %s" % (stuck, before, after))
    if stuck:
        attach_seen_until(s, others, has_drawn("lying", near=stuck[0]["at"]),
                          "stuck: A and C draw the bayonet on B's gun stuck in the grunt")
    s.note("the server's log of the stab: %s" % [l.strip() for l in srv.lines[since:] if "stick" in l or "damage" in l][:6])
    srv.cmd("developer 0")
    attach_dm(s, srv, a, b, c)


def attach_dm(s, srv, a, b, c):
    """attach's deathmatch part: the map again under deathmatch 1 (the clients reconnecting); B's shotgun (his deathmatch
    holstered one, drawn) given a bayonet; A held in front of him: V's thrust hurts A."""
    since_a, since_b, since_c = a.mark(), b.mark(), c.mark()
    # (changelevel: a listen host's "map" drops the remote clients)
    srv.cmd("coop 0; deathmatch 1; noexit 1; fraglimit 0; timelimit %d; changelevel e1m1" % DM_TIMELIMIT)
    _, ja = a.wait(r"mp_event: .*joined map=e1m1", 120, since_a, "A in the deathmatch game")
    _, jb = b.wait(r"mp_event: .*joined map=e1m1", 120, since_b, "B in the deathmatch game")
    _, jc = c.wait(r"mp_event: .*joined map=e1m1", 120, since_c, "C in the deathmatch game")
    ea, eb = int(fields(ja)["self"]), int(fields(jb)["self"])
    ec = int(fields(jc)["self"])
    srv.cmd("developer 1")
    attach_arm(s, srv, b, eb, "deathmatch (his holstered shotgun drawn: no test gives)", "impulse %d" % ATTACH_DRAW_IMPULSE)
    # (no setpos in deathmatch: the server's aid puts them, vr_dm_test 27; each facing the nearest other player; A put again
    # once B is there, to face him)
    spots = ((a, ea, (MELEE_SPOT[0], MELEE_SPOT[1] + ATTACH_STAB_DIST, MELEE_SPOT[2])), (b, eb, MELEE_SPOT),
             (a, ea, (MELEE_SPOT[0], MELEE_SPOT[1] + ATTACH_STAB_DIST, MELEE_SPOT[2])), (c, ec, ATTACH_DM_C_SPOT))
    for inst, ent, at in spots:
        since = srv.mark()
        srv.cmd("vr_dm_test_who %d; vr_dm_test_x %g; vr_dm_test_y %g; vr_dm_test_z %g; vr_dm_test 27" % (ent, at[0], at[1], at[2]))
        srv.wait(r"attachmp: (player %d put|no player)" % ent, 10, since, "%s put by the server" % inst.name)
    at_b = org_of(status(srv), "mp_client", "slot", eb - 1)
    at_a = org_of(status(srv), "mp_client", "slot", ea - 1)
    s.check("deathmatch: the server puts B and A face to face (vr_dm_test 27)", bool(at_a and at_b) and
            abs(math.dist(at_a[:2], at_b[:2]) - ATTACH_STAB_DIST) <= 4, "A at %s, B at %s" % (at_a, at_b))
    attach_seen_until(s, [a, c], has_drawn("other_held", eb), "deathmatch: A and C draw the bayonet on B's shotgun")
    a.cmd(EMPTY_HAND)
    before = health_of(srv, ea)
    since = srv.mark()
    b.cmd("vr_flat_punch v")
    got = wait_quietly(srv, r"damage: player [-\d.]+ by [^\n]*", 10, since, "B's stab landing on A")
    after = health_of(srv, ea)
    s.check("deathmatch: B's bayonet thrust hurts A", after < before or got is not None,
            "A's health %d -> %d; %s" % (before, after, got[1].strip() if got else "no damage line"))
    srv.cmd("developer 0")



# The mock's force grab (Misc/quakevr/forcegrab_block_test.sh's): the empty main hand pointing down at a thing 150 units
# ahead, the trigger pulled, the hand flicked up 0.3 m in 0.1 s, then the grip, 0.3 s after the flick: about 0.2 s before
# the pull's 0.49 s flight ends, the middle of vr_forcegrab_catch_early's 0.35 (the script's grip at 0.55 came 0.354 s
# before its arrival on the dedicated server, every run: just early).
FORCEGRAB_PLAY = ("0.000 main 0.2 1.2 -0.3 60 0 0", "0.000 off -0.25 1.1 -0.2 70 0 0", "0.300 cmd +attack",
                  "0.400 main 0.2 1.2 -0.3 60 0 0", "0.500 main 0.2 1.5 -0.25 60 0 0", "0.600 cmd -attack",
                  "0.680 grip main 0", "0.700 grip main 1", "0.700 cmd +grabright", "2.000 main 0.2 1.5 -0.25 60 0 0")


def force_grab(s, srv, inst, ent, play, label):
    """`inst` at OPEN_SPOT (facing the way north) force grabs the weapon pickup 150 units ahead: the server's catch, and
    his main hand's weapon -> that weapon (0: none)."""
    put_at(srv, inst, ent, OPEN_SPOT, 90)
    inst.cmd("-grabright; vr_mock_button main grip 0; vr_mock_hand main 0.2 1.2 -0.3 60 0 0; "
             "vr_mock_hand off -0.25 1.1 -0.2 70 0 0")
    weapon_until(s, srv, ent, 0)
    since = srv.mark()
    inst.cmd("vr_mock_play %s" % play)
    got = wait_quietly(srv, r"force grab: (caught|missed|arrived)", 20, since, "%s's force grab" % inst.name)
    s.note("%s: the server: %s" % (label, got[1].strip() if got else None))
    seen = weapon_until(s, srv, ent, -2, 10)  # (its last reports: whatever he holds)
    back = wait_quietly(srv, r"pickup: weapon_supershotgun back at", 5, since, "the pickup back at its place")
    wpn = seen[-1]
    s.check("%s: %s's force grab catches the level's super shotgun (the server's catch; his main hand holds it)" % (
        label, inst.name), got is not None and "caught weapon_supershotgun" in got[1] and wpn not in (0, -1, 4),
        "server: %s; his main weapon by report: %s" % (got[1].strip() if got else None, seen))
    s.check("%s: in coop the pickup stays for the others (back at its place)" % label, back is not None,
            "no 'pickup: weapon_supershotgun back at' on the server")
    # (the play stopped, its grip kept: he keeps what he caught; let go, it lay where the next player is put and his
    # force grab never started, 1 run of 1)
    inst.cmd("vr_mock_play")
    return wpn


def forcegrab(s):
    """Force grab of a weapon of the level in coop, by each player: a super shotgun pickup (the map's kind, vr_test_spawn
    116) 150 units ahead of B; B's force grab (the mock's: point, trigger, flick, grip) brings it to his hand, his; it is
    back at its place for the others (coop leaves it); then A, put where B stood, force grabs it: his too."""
    srv, a, b, ea, eb = join(s)
    a.cmd("name A")
    b.cmd("name B")
    play = os.path.join(s.out, "forcegrab_play.txt")
    with open(play, "w", encoding="ascii") as f:
        f.write("\n".join(FORCEGRAB_PLAY) + "\n")
    srv.cmd("developer 1; vr_debug_carry 2; vr_test_spawn 116; vr_test_spawn_dist 150")
    put_at(srv, b, eb, OPEN_SPOT, 90)
    since = srv.mark()
    b.cmd("impulse 241")
    _, spawned = srv.wait(r"test spawn: weapon_supershotgun at", 10, since, "the super shotgun pickup ahead of B")
    s.note("the server: %s" % spawned.strip())
    wb = force_grab(s, srv, b, eb, play, "B")
    put_at(srv, b, eb, side_spot(srv, ea))
    wa = force_grab(s, srv, a, ea, play, "A")
    s.metric("force_grab_wid", max(wa, wb))
    srv.cmd("developer 0")


def weaponwheel(s):
    """The headset's weapon wheel in coop (Quake/vr/vr_weaponwheel.cpp; QC VR_Wheel_Pick): B (a remote client) runs
    vr_weaponwheel_test mp: a shotgun given into his right shoulder holster (impulse 91), his B held opens the wheel round
    his right hand, the hand onto the shoulder's slot, let go: his client sends "vr_wheel_pick 1 1". His own client has the
    shotgun in his main hand (the test's verdict), the server has it there, and A sees it (B's avatar's weapon at A)."""
    srv, a, b, ea, eb = join(s)
    a.cmd("name A")
    b.cmd("name B")
    srv.cmd("developer 1")
    since, sv_since = b.mark(), srv.mark()
    b.cmd("developer 1; vr_weaponwheel_test mp")
    _, verdict = b.wait(r"weaponwheel_test: \d+ passed, \d+ failed", 60, since, "B's weapon wheel test")
    for l in b.lines[since:]:
        if l.startswith("weaponwheel_test:") or l.startswith("weapon wheel:"):
            s.note("B: " + l.strip())
    for l in srv.lines[sv_since:]:
        if l.startswith("weapon wheel:"):
            s.note("server: " + l.strip())
    s.check("B's wheel: the shotgun drawn into his right hand by his own client's account", " 0 failed" in verdict,
            verdict.strip())
    seen = weapon_until(s, srv, eb, 4)
    s.check("the server: B's main hand holds the shotgun from his holster", seen[-1] == 4, "by report: %s" % seen)
    got = {}
    for _ in range(30):  # (a definite end: at most 30 reports)
        (lines_a,) = reports(s, [a])
        got = by_ent(lines_a, "mp_avatar")
        if eb in got and int(got[eb]["wpn"][0]) == 4:
            break
    s.check("A sees B's shotgun drawn (B's avatar's weapon at A)", eb in got and int(got[eb]["wpn"][0]) == 4,
            "A has B's weapon %s" % (got[eb].get("wpn") if eb in got else None))
    srv.cmd("developer 0")


HOLD_OUT = "vr_mock_hand main 0.15 1.2 -0.45 0 0 0"  # mp_disarm: a gun held out in front at the chest
B10_GAP = 48.0  # mp_disarm: units between the two players, face to face (a hand reaches the other's held gun)
VIEW_GAP = 0.5  # (B27) mp_disarm: B's drawn round and port against the server's, still (were 0.5-2.9 and 0.6-1.3)
# The pulls (rel: from where the hand is, metres in tracking space; +z towards the puller): gently, 0.3 m in 1.5 s
# (0.2 m/s: it slips off), and a yank, 0.3 m in 0.1 s (3 m/s; vr_player_disarm_pull 1.5).
GENTLE_PULL = ("0.000 rel main 0 0 0", "1.500 rel main 0 0 0.3", "2.000 rel main 0 0 0.3")
YANK_PULL = ("0.000 rel main 0 0 0", "0.100 rel main 0 0 0.3", "0.500 rel main 0 0 0.3")
# A toss underhand to the player ahead: the hand back low, swung forward and up 0.6 m in 0.15 s, let go at its top.
TOSS_PLAY = ("0.000 main 0.15 1.0 -0.10 0 0 0", "0.400 main 0.15 1.0 -0.10 0 0 0", "0.550 main 0.15 1.3 -0.65 0 0 0",
             "0.550 grip main 0", "1.000 main 0.15 1.3 -0.65 0 0 0")


def hand_at(srv, inst, ent, target, tries=60):
    """`inst`'s main hand moved to world point `target` (vr_mock_hand_to) until the server has it within 2 units ->
    its place there (a definite end: `tries` reports)."""
    got = None
    for i in range(tries):
        if i % 10 == 0:
            inst.cmd("vr_mock_hand_to main %.1f %.1f %.1f" % tuple(target))
        got = ground_report(srv, "mp_drawn_sv").get((ent, 1), {}).get("hand")
        if got and dist(got, target) <= 2:
            return got
    raise StepFailed("%s's hand never at %s on the server (it has %s)" % (inst.name, target, got))


def grab_weapon_of(s, srv, taker, victim, et, ev, label):
    """`taker`'s empty main hand onto the shotgun `victim` holds out (its grip hotspot as the server places it), the grip
    pressed once the server has the hand there -> the server's "takes hold" line (None: no hold)."""
    # (B25: until the server draws it in his hand: the shotgun given (weapon_until: his inventory's) was not yet drawn
    # there in a suite run under load, {'drawn': 0, 'slot': -1}; at most 40 reports)
    spots = []
    for _ in range(40):
        sv = drawn_report(srv, "mp_drawn_sv").get((ev, 1), {})
        spots = [v[1:] for v in sv.get("spots", {}).values() if int(v[0]) == 1] or \
            [v[1:] for v in sv.get("spots", {}).values()]
        if spots:
            break
    if not spots:
        raise StepFailed("%s: no hotspot of %s's shotgun on the server: %s" % (label, victim.name, sv))
    taker.cmd("vr_mock_play; vr_mock_button main grip 0")
    hand_at(srv, taker, et, spots[0])
    server_time_past(srv, server_time(srv) + GRIP_REPRESS_S)  # (the hand still: a grab, not a swipe; the let-go's bounce)
    since = srv.mark()
    taker.cmd("vr_mock_button main grip 1")
    got = wait_quietly(srv, r"pdisarm: %s's hand 1 takes hold of %s's weapon" % (taker.name, victim.name), 10, since,
                       "%s's hold on %s's shotgun" % (taker.name, victim.name))
    s.note("%s: %s's hand on %s's shotgun's hotspot %s: %s" % (label, taker.name, victim.name, spots[0],
                                                             got[1].strip() if got else None))
    return got[1] if got else None


def holsters_synced(s, a, b, eb, label):
    """B's holsters as A draws them are B's own (every slot B draws, the same model, within HOLSTER_UNITS; none more)."""
    def same(lines):
        la, lb = lines
        own = {k: v for k, v in holsters_of(lb, "mp_holster_own").items() if k[0] == eb}
        at_a = {k: v for k, v in holsters_of(la, "mp_holster").items() if k[0] == eb}
        return set(own) == set(at_a) and all(own[k]["model"] == at_a[k]["model"] and
                                             dist(own[k]["at"], at_a[k]["at"]) <= HOLSTER_UNITS for k in own)
    try:
        (la, lb), n = reports_until(s, [a, b], same, "%s: A draws B's holsters as B does" % label)
    except StepFailed as e:
        s.check("%s: A draws B's holsters as B does" % label, False, str(e))
        return
    own = sorted("%d:%s" % (k[1], v["model"]) for k, v in holsters_of(lb, "mp_holster_own").items() if k[0] == eb)
    s.check("%s: A draws B's holsters as B does (slot:model, within %g units)" % (label, HOLSTER_UNITS), True,
            "%s, after %d reports" % (own or "none", n))


def disarm(s):
    """MP B10 (his voice notes 2026-11-15): another player's weapon taken by a grab and a pull, given back by a toss,
    holsters seen by the other. A holds a shotgun out, B faces him; B's hand closes on its grip hotspot and pulls gently:
    it slips off, A keeps it; B grabs again and yanks: the shotgun is B's (A's hand empty). B tosses it to A: it lands by
    A and A's grip takes it. B's holsters (a shotgun given in each slot, one taken out, after his death) as A draws them."""
    # (A second: entity 2 on the dedicated server, as a later player is; the toss's retake by his hand against the gun)
    srv, a, b, ea, eb = join(s, a_second=True)
    a.cmd("name A")
    b.cmd("name B")
    if AMMO_PART:  # (--part give|use: the rounds alone)
        srv.cmd("vr_debug_carry 2; developer 1")
        ammo_give(s, srv, a, b, ea, eb)
        return
    put_at(srv, b, eb, side_spot(srv, ea))
    put_at(srv, a, ea, OPEN_SPOT, 90)
    put_at(srv, b, eb, (OPEN_SPOT[0], OPEN_SPOT[1] + B10_GAP, OPEN_SPOT[2]), 270)
    grip_then_give(srv, a, "%s; vr_mock_button main grip 1; vr_mock_button off grip 0" % HOLD_OUT)
    held = weapon_until(s, srv, ea, 4)
    s.check("A holds a shotgun out (the server's report)", held[-1] == 4, "A's main weapon by report: %s" % held)
    b.cmd("vr_mock_hand main 0.15 1.2 -0.2 0 0 0; vr_mock_button main grip 0; vr_mock_button off grip 0")

    # The gentle pull: the hold slips off, A keeps his shotgun.
    hold = grab_weapon_of(s, srv, b, a, eb, ea, "gentle")
    s.check("gentle: B's grip on A's shotgun takes hold of it (on its drawn model, a still hand)", hold is not None,
            "no 'takes hold' on the server")
    since = srv.mark()
    b.cmd("vr_mock_play %s" % play(s, b, "gentle", GENTLE_PULL))
    slip = wait_quietly(srv, r"pdisarm: B lets go of A's weapon \(slipped off", 10, since, "B's hold slipping off")
    kept = weapon_until(s, srv, ea, 4, 3)
    mine = weapon_until(s, srv, eb, 0, 3)
    s.check("gentle: a pull too gentle (0.2 m/s) slips off: A keeps his shotgun, B's hand empty",
            slip is not None and kept[-1] == 4 and mine[-1] == 0,
            "server: %s; A's main weapon %s, B's %s" % (slip[1].strip() if slip else None, kept, mine))

    # The yank: his.
    hold = grab_weapon_of(s, srv, b, a, eb, ea, "yank")
    since = srv.mark()
    b.cmd("vr_mock_play %s" % play(s, b, "yank", YANK_PULL))
    took = wait_quietly(srv, r"pdisarm: A's .* taken by B \(hand 1\): pulled at", 10, since, "B's yank")
    mine = weapon_until(s, srv, eb, 4)
    gone = weapon_until(s, srv, ea, 0)
    s.check("yank: B's grip and a hard pull (3 m/s) take A's shotgun into B's hand (A's hand empty)",
            hold is not None and took is not None and mine[-1] == 4 and gone[-1] == 0,
            "server: %s; B's main weapon %s, A's %s" % (took[1].strip() if took else None, mine, gone))
    s.metric("disarm_takes", 1 if took else 0)

    # The toss back: B swings it underhand towards A and lets go; it lands by A; A's empty hand takes it.
    a.cmd("vr_mock_button main grip 0; vr_mock_hand main 0.1 0.6 -0.2 0 0 0")
    server_time_past(srv, server_time(srv) + THROW_SETTLE_S)
    since = srv.mark()
    b.cmd("vr_mock_play %s" % play(s, b, "toss", TOSS_PLAY))
    thrown = wait_quietly(srv, r"thrown: velocity", 10, since, "B's toss")
    gone = weapon_until(s, srv, eb, 0)
    gun, spot = lying(srv, ea)
    at = org_of(status(srv), "mp_client", "slot", ea - 1)
    s.note("toss: %s; the shotgun (entity %d) lies at %s, A at %s" % (thrown[1].strip() if thrown else None, gun, spot, at))
    # (B19) Within a step and a reach of him (96 units). The play's release falls at a real-time point of B's swing
    # (the throw's age 0.06-0.27 s after its fastest sample, his frames as the machine runs them): the earliest is the
    # highest lob (60 u/s up against 47-55), and one such (B18's suite, 1 run of 3) slid on to lie 71 units off, by the
    # start room's south wall, past the 64 asked; the landings otherwise 26-45 units from him.
    far = math.hypot(spot[0] - at[0], spot[1] - at[1])
    s.metric("toss_lands_from_a", round(far, 1))
    s.check("toss: B's shotgun leaves his hand thrown towards A and lands within a step and a reach of him (96 units)",
            thrown is not None and gone[-1] == 0 and far <= 96, "B's main weapon %s; it lies %.0f units from A" % (gone, far))
    # (A steps up to it: 20 units short of it on his side, as hand_over's taker; a toss lands within a step, not a reach.
    # B19: lying by a wall, that spot can be in it (B18's "A never put at": SV_CheckStuck put him back), so 32 and 44
    # short after it)
    d = [spot[0] - at[0], spot[1] - at[1]]
    n = math.hypot(d[0], d[1]) or 1.0
    for short in (20.0, 32.0, 44.0):
        if n <= short:
            break
        try:
            put_at(srv, a, ea, (spot[0] - d[0] / n * short, spot[1] - d[1] / n * short, at[2]), tries=20)
            break
        except StepFailed:
            if short == 44.0:
                raise
            s.note("toss: A not put %.0f units short of the shotgun (a wall there?): further back" % short)
    since = srv.mark()
    a.cmd("vr_mock_button main grip 1")
    srv.wait(r"grip: A's main hand pressed at", 10, since, "A's grip on the server")
    since = srv.mark()
    a.cmd("vr_mock_button main grip 0")
    # (by its handle, in reach whichever way the toss left it lying: gun mode, vr_weapon_grab_handle_leniency over the
    # floor; its forend's hotspot (spot 0) could lie turned under the gun, out of the lowest fist's reach: no slack there)
    line = retake(srv, a, "main", since, r"weapon: .* taken into the main hand", "vr_mock_hand_to main spot -1 6",
                  "vr_mock_hand_to main spot -1")
    back = weapon_until(s, srv, ea, 4)
    s.check("toss: A's grip takes the shotgun B tossed him (the server's: in his main hand)",
            line is not None and back[-1] == 4, "server: %s; A's main weapon %s" % (line, back))

    # B's holsters as A sees them: a shotgun given in each slot, one taken out by hand, after his death.
    b.cmd("vr_mock_play; vr_mock_button main grip 0")
    holsters_synced(s, a, b, eb, "holsters at the start")
    for slot in range(6):
        since = srv.mark()
        b.cmd("impulse %d" % (90 + slot))
        wait_quietly(srv, r"holster give: .* holster %d a shotgun" % slot, 5, since, "B's holster %d given" % slot)
    holsters_synced(s, a, b, eb, "a shotgun given in each holster")
    since = srv.mark()
    b.cmd("vr_mock_hand_to main holster %d" % HOLSTER_SLOT)
    server_time_past(srv, server_time(srv) + THROW_SETTLE_S)
    b.cmd("vr_mock_button main grip 1")
    out = weapon_until(s, srv, eb, -2, 15)
    s.note("holster %d drawn by B's hand: his main weapon by report %s" % (HOLSTER_SLOT, out))
    holsters_synced(s, a, b, eb, "one taken out by hand")
    since = srv.mark()
    b.cmd("kill")
    server_time_past(srv, server_time(srv) + 3.0)
    b.cmd("vr_mock_button main grip 0; +jump; wait; -jump")
    server_time_past(srv, server_time(srv) + 1.0)
    holsters_synced(s, a, b, eb, "after B's death and respawn")
    ammo_give(s, srv, a, b, ea, eb)
    disarm_leaver(s, srv, a, b, ea, eb)
    srv.cmd("vr_debug_carry 0; developer 0")


def disarm_leaver(s, srv, a, b, ea, eb):
    """(REVIEW_MP_2026-11-18 finding 7) A player who left can't be disarmed: A holds a shotgun out and leaves the coop
    game (his body stays: ClientDisconnect's set_suicide_frame, DEAD_DEAD, with his health, FL_CLIENT and the gun in his
    frozen hand); B's empty hand closes on the gun's hotspot where the server had it: no hold (before: B took it, and A
    back by his name had it again, vr_mp_rejoin_keep)."""
    b.cmd("vr_mock_play; vr_mock_button main grip 0; vr_mock_button off grip 0")
    put_at(srv, b, eb, side_spot(srv, ea))  # (out of A's spot first: ammo_give leaves B where A stood)
    put_at(srv, a, ea, OPEN_SPOT, 90)
    put_at(srv, b, eb, (OPEN_SPOT[0], OPEN_SPOT[1] + B10_GAP, OPEN_SPOT[2]), 270)
    a.cmd("vr_mock_play; vr_mock_button main grip 0; vr_mock_button off grip 0")  # (a fresh press for the give)
    server_time_past(srv, server_time(srv) + GRIP_REPRESS_S)
    grip_then_give(srv, a, "%s; vr_mock_button main grip 1" % HOLD_OUT)
    held = weapon_until(s, srv, ea, 4)
    empty = weapon_until(s, srv, eb, 0)
    # (A's main hand where the server has it: on the gun's line from butt to tip, which stays on his body's frozen hand;
    # its hotspots are his client's drawn ones, gone with his slot's held pose when he leaves)
    grip = None
    for _ in range(40):
        grip = ground_report(srv, "mp_drawn_sv").get((ea, 1), {}).get("hand")
        if grip:
            break
    s.check("leaver: A holds a shotgun out, B's main hand empty (the server's reports)",
            held[-1] == 4 and empty[-1] == 0 and grip, "A's main weapon %s, B's %s; A's hand %s" % (held, empty, grip))
    if not grip:
        return
    since = srv.mark()
    a.cmd("disconnect")
    left = wait_quietly(srv, r"coop: A left|Client A removed", 20, since, "A leaving")
    hand_at(srv, b, eb, grip)
    server_time_past(srv, server_time(srv) + GRIP_REPRESS_S)
    since = srv.mark()
    b.cmd("vr_mock_button main grip 1")
    # (his body's netname is empty once he left: SV_DropClient clears the name it points to)
    hold = wait_quietly(srv, r"pdisarm: B's hand 1 takes hold of ", 3, since, "no hold on A's body's gun")
    mine = weapon_until(s, srv, eb, 0, 5)
    s.check("leaver: B's grip on the shotgun in the hand of A's body (A gone) takes no hold; B's hand stays empty",
            left is not None and hold is None and mine[-1] == 0,
            "A left: %s; server: %s; B's main weapon %s" % (left[1].strip() if left else None,
                                                           hold[1].strip() if hold else "no hold", mine))
    b.cmd("vr_mock_button main grip 0")


# The rounds handed over (MP B10b): (what, the main hand's weapon whose pouch kind it is, its give, the reserve's key,
# the round's classname, a part of its model's name).
AMMO_KINDS = (
    ("shell", 4, "", "shells", "vr_ammo_shell", "vr_shell"),
    ("nail magazine", 6, "give n 100", "nails", "vr_ammo_mag", "vr_mag_nail"),
    ("rocket", 10, "give r 20", "rockets", "vr_ammo_front", "vr_round_rocket"),
)


def ammo_give(s, srv, a, b, ea, eb):
    """Rounds handed over (Weapon Mode Immersive's ammo pouch, vr_reload_mode 3): a shell, a nail magazine and a rocket in
    turn. A takes one from his pouch (his main hand's weapon gives its kind) and lets it fall ahead of him; B, put where
    A stood, takes it up (from the floor, not a round of his own pouch) and lets go of it at his own pouch: A's reserve
    that much less, B's that much more (the single player rules, one path: the loose round's carry, VR_Reload_LetGo,
    VR_Reload_Refund). Then a shell the other way, B's to A (a listen host's own player: the single player's case), B
    armed with a shotgun first."""
    for inst in (srv, a, b):
        inst.cmd("vr_reload_mode 3")
    srv.cmd("developer 1; vr_reload_debug %d" % int(os.environ.get("QVR_RELOAD_DEBUG", "1")))
    for kind in AMMO_KINDS:
        if AMMO_PART in ("", "give"):
            ammo_give_one(s, srv, a, b, ea, eb, kind[1] != 4, *kind)
    if AMMO_PART in ("", "give"):
        ammo_give_one(s, srv, b, a, eb, ea, True, *AMMO_KINDS[0])
    # Used by B (MP B8h): a magazine and a rocket A let fall loaded by B into his own gun (its room made first).
    for kind in AMMO_KINDS[1:]:
        if AMMO_PART in ("", "use"):
            ammo_give_one(s, srv, a, b, ea, eb, True, *kind, use=True)


def arm_with(s, srv, inst, ent, wid, give):
    """`inst`'s main hand given weapon `wid` (WID 4 the shotgun, 6 the nailgun, 10 the rocket launcher) by its impulse,
    `give` first (its ammo) -> the server's main weapon by report."""
    if give:
        inst.cmd(give)
    inst.cmd("vr_mock_button main grip 1")
    server_time_past(srv, server_time(srv) + THROW_SETTLE_S)
    inst.cmd("impulse %d" % (150 + wid))
    return weapon_until(s, srv, ent, wid, 15)


def make_room(s, srv, inst, ent, wid, what):
    """Room in `inst`'s main hand's gun for one more `what` (he faces north, the way open ahead): the nailgun's magazine
    out by its button (it falls where he stands: done away from where the round he takes lies), the launcher fired level
    until empty (the test impulses are single player's alone) -> True when the server has his gun's magazine empty."""
    clip = lambda: inventories(srv)[0].get(ent - 1, {}).get("clip", -1)
    since = srv.mark()
    if wid == 6:
        inst.cmd("vr_mock_button main secondary 1")
        server_time_past(srv, server_time(srv) + 0.1)
        inst.cmd("vr_mock_button main secondary 0")
        # (QuakeC's hands: 1 the main, cVR_MainHand; 0 the off)
        wait_quietly(srv, r"reload: .* out of the gun \(hand 1", 5, since, "%s's magazine out" % inst.name)
        server_time_past(srv, server_time(srv) + 1.5)  # (it falls and settles away from him)
    else:
        inst.cmd("vr_mock_hand main 0.15 1.2 -0.40 70 0 0")  # (the mock's level gun: 70 degrees up from its rest)
        # (B25: the trigger held until the server's magazine is down by one, not 0.1 s of the server's time: under load
        # B's game ran +attack and -attack in one frame, no shot, the magazine left at 1 after the 4 tries)
        for _ in range(6):  # (the launcher holds a few: one shot each, its refire time between)
            before = clip()
            if before <= 0:
                break
            inst.cmd("+attack")
            for _ in range(40):
                server_time_past(srv, server_time(srv) + 0.05)
                if clip() < before:
                    break
            inst.cmd("-attack")
            server_time_past(srv, server_time(srv) + 1.0)
    left = clip()
    s.note("%s's %s gun emptied: its magazine %d" % (inst.name, what, left))
    return left == 0


def ammo_give_one(s, srv, a, b, ea, eb, arm, what, wid, give, key, classname, model, use=False):
    """One round of kind `what` from `a`'s pouch to `b`'s (ammo_give); `arm`: `a` is given the weapon first; `use`: `b`,
    armed with the same gun with room for it, loads it into his gun instead (at its loading port, his off hand)."""
    an, bn = a.name, b.name
    a.cmd("vr_mock_play; vr_mock_button off grip 0")
    b.cmd("vr_mock_play; vr_mock_button main grip 0; vr_mock_button off grip 0")
    put_at(srv, b, eb, side_spot(srv, ea), 90 if use else None)
    roomy = True
    if use:
        armed = arm_with(s, srv, b, eb, wid, give)
        roomy = armed[-1] == wid and make_room(s, srv, b, eb, wid, what)
        s.check("use: %s holds the %s's gun with room for one (by report)" % (bn, what), roomy,
                "%s's main weapon %s (its magazine emptied: the notes)" % (bn, armed))
    put_at(srv, a, ea, OPEN_SPOT, 90)
    if arm:
        arm_with(s, srv, a, ea, wid, give)
    inv0, _ = inventories(srv)
    have0 = inv0.get(ea - 1, {}).get(key, 0)
    server_time_past(srv, server_time(srv) + THROW_SETTLE_S)
    a.cmd("vr_mock_hand_to off ammopouch")
    server_time_past(srv, server_time(srv) + THROW_SETTLE_S)
    a.cmd("vr_mock_button off grip 1")
    inv1, _ = inventories_until(srv, lambda inv, m: inv.get(ea - 1, {}).get(key, 0) < have0, 30)
    a.cmd("vr_mock_hand off -0.15 1.2 -0.45 0 0 0")
    server_time_past(srv, server_time(srv) + THROW_SETTLE_S)
    a.cmd("vr_mock_button off grip 0")
    server_time_past(srv, server_time(srv) + 1.5)
    taken = have0 - inv1.get(ea - 1, {}).get(key, 0)
    s.check("ammo: %s takes a %s out of his pouch and lets it fall (his reserve less)" % (an, what), taken >= 1,
            "%s's %s %s -> %s" % (an, key, have0, inv1.get(ea - 1, {}).get(key)))
    at = org_of(status(srv), "mp_client", "slot", ea - 1)
    put_at(srv, a, ea, side_spot(srv, eb))
    put_at(srv, b, eb, (at[0], at[1] + 8, at[2]), 90)
    b.cmd("vr_mock_hand off -0.15 0.6 -0.35 0 0 0")
    server_time_past(srv, server_time(srv) + THROW_SETTLE_S)
    # (B's own client's entities: a remote client has no server to ask for the round's classname, so the one A let fall
    # is found by its model; B10's "nearest vr_ammo_shell" found none there, the hand stayed at B's pouch and took a
    # pair of his own: the "open item" was the test's.)
    mine0 = inv1.get(eb - 1, {}).get(key, 0)
    took = None
    for _ in range(3):
        # (At its origin: 1 unit over it, a shell lying on its side was 3.1 cm off the fist.)
        b.cmd("vr_mock_hand_to off nearestmodel %s" % model)
        server_time_past(srv, server_time(srv) + THROW_SETTLE_S)
        since = srv.mark()
        b.cmd("vr_mock_button off grip 1")
        took = wait_quietly(srv, r"carry: %s held at" % classname, 2, since, "%s's hand taking the %s" % (bn, what))
        if took:
            break
        b.cmd("vr_mock_button off grip 0")
        server_time_past(srv, server_time(srv) + THROW_SETTLE_S)
    inv_t, _ = inventories(srv)
    from_floor = took is not None and inv_t.get(eb - 1, {}).get(key, 0) == mine0
    s.check("ammo: %s's hand takes up the %s %s dropped (the server's carry of it, not one of his own pouch)" % (bn, what, an),
            from_floor, "carry %s; %s's %s %s -> %s" % ("seen" if took else "not seen", bn, key, mine0,
                                                       inv_t.get(eb - 1, {}).get(key)))
    server_time_past(srv, server_time(srv) + THROW_SETTLE_S)
    if use:
        ammo_load(s, srv, b, eb, what, key, from_floor and roomy, inv_t.get(eb - 1, {}).get(key, 0))
        b.cmd("vr_mock_button off grip 0; vr_mock_button main grip 0")
        return
    b.cmd("vr_mock_hand_to off ammopouch")
    server_time_past(srv, server_time(srv) + THROW_SETTLE_S)
    b.cmd("vr_mock_button off grip 0")
    inv2, _ = inventories_until(srv, lambda inv, m: inv.get(eb - 1, {}).get(key, 0) > mine0, 10)
    got = inv2.get(eb - 1, {}).get(key, 0) - mine0
    s.metric("ammo_into_%s_reserve_%s" % (bn.lower(), key), got)
    s.check("ammo: let go of at %s's own pouch, the %s goes into his reserve (as much as %s lost)" % (bn, what, an),
            from_floor and got == taken, "%s's %s %s -> %s (%s lost %s)" % (bn, key, mine0, inv2.get(eb - 1, {}).get(key), an, taken))


def ammo_load(s, srv, b, eb, what, key, ready, mine0):
    """`b`'s off hand, holding a round taken up from the floor, brought to his main hand's gun's loading port (as his
    client draws it: a remote client's own view): seated or loaded (the server's reload log), his reserve unchanged."""
    since = srv.mark()
    loaded = None
    if ready:
        # (The gun held up as front_test's launcher, its muzzle 70 degrees up; the round in the off hand as it was taken
        # up lying along his facing, turned as much: along the barrel, its butt to the muzzle.)
        # (B25b: each placement waited for at B's game (its echo) before the server's 0.2 s, and the whole tried again
        # from the start pose twice: in 2 full suite runs of 6 the magazine stopped 2.5 units off the port (leniency 2)
        # and stayed there, the port and the round each a unit apart from where B's previous placement had them.)
        # (B26: why. The gun moves as the off hand comes to it: its port shifted 0.6-1.3 units between the hand's start
        # pose, 6 under the port and at it (the server's reload rows, debug 2), so a placement aimed at the port seen
        # before it lands that far off, and each new one moves the gun again. A player watches the magazine and the well
        # and corrects: so does the test now, by the server's own rows (the round's place and the port's, after each
        # placement at it): the next placement moved by their gap (`lport 0 dx dy dz`), up to 4 at the port.)
        seat = r"reload: (.* seated \(hand 1|1 into the gun \(hand 1)"
        row = re.compile(r"reload: a held round ([\d.]+) units from the port \(leniency ([\d.]+)\): at (\S+) (\S+) (\S+), "
                         r"the port (\S+) (\S+) (\S+)")
        srv.cmd("vr_reload_debug 2")
        if int(os.environ.get("QVR_RELOAD_DEBUG", "1")) >= 3:
            b.cmd("vr_debug_carry 1; vr_debug_body_collide 1; developer 1")  # (B27: B's drawn meets and pushes, beside the server's rows)
        gaps = []
        # (B27: B's view against the server's, each still pose before a placement: the gun's port and the held round's
        # point as B draws them (his lport's print) against the server's last row there. The gun moved as the round
        # came near it (the drawn meets' pushes) and B's round was drawn up to 3 units off the server's (its place in
        # his hand taken from where the server had it a delay before): now none of either, the place sent.)
        b_port = re.compile(r"vr_mock_hand_to: the other hand's gun's loading port: (\S+) (\S+) (\S+)")
        b_ref = re.compile(r"vr_mock_hand_to: held \d+ at .* its reference (\S+) (\S+) (\S+)")
        views = []  # (round gap, port gap) per still pose
        for attempt in range(3):
            if attempt:
                s.note("%s's %s not in his gun yet: placed again (try %d)" % (b.name, what, attempt + 1))
            b.cmd("vr_mock_hand main 0.15 1.0 -0.40 140 0 0; vr_mock_hand off -0.15 1.0 -0.40 70 0 0")
            mark_s = srv.mark()
            server_time_past(srv, server_time(srv) + THROW_SETTLE_S)
            off = [0.0, 0.0, 0.0]
            for below in (6, 0, 0, 0, 0):
                srv.mark()  # (read the log so far)
                if srv.find(seat, since):
                    break
                last = [m for m in (row.search(l) for l in srv.lines[mark_s:]) if m]
                mark_b = b.mark()
                b.cmd("vr_mock_hand_to off lport %d %.2f %.2f %.2f; echo mp_lport_placed" % (below, off[0], off[1], off[2]))
                b.wait(r"mp_lport_placed", 20, mark_b, "%s's hand placed at the port" % b.name)
                bp = [m for m in (b_port.search(l) for l in b.lines[mark_b:]) if m]
                br = [m for m in (b_ref.search(l) for l in b.lines[mark_b:]) if m]
                if last and bp and br:
                    v = [float(x) for x in last[-1].groups()]
                    seen_r, seen_p = [float(x) for x in br[0].groups()], [float(x) for x in bp[0].groups()]
                    views.append((math.dist(seen_r, v[2:5]), math.dist(seen_p, v[5:8])))
                mark_s = srv.mark()
                server_time_past(srv, server_time(srv) + 0.2)
                if below == 0:
                    rows = [m for m in (row.search(l) for l in srv.lines[mark_s:]) if m]
                    if rows:
                        v = [float(x) for x in rows[-1].groups()]
                        gaps.append(v[0])
                        off = [off[i] + v[5 + i] - v[2 + i] for i in range(3)]
            loaded = wait_quietly(srv, seat, 5, since, "%s's %s into his gun" % (b.name, what))
            if loaded:
                break
        srv.cmd("vr_reload_debug %d" % int(os.environ.get("QVR_RELOAD_DEBUG", "1")))
        s.note("%s's %s at the port: the server's gaps after each placement at it %s" % (
            b.name, what, ", ".join("%.1f" % g for g in gaps)))
        worst_r = max((r for r, _ in views), default=-1.0)
        worst_p = max((p for _, p in views), default=-1.0)
        s.metric("ammo_view_gap_%s_round" % key, round(worst_r, 2))
        s.metric("ammo_view_gap_%s_port" % key, round(worst_p, 2))
        s.check("use: %s draws the %s in his hand and his gun's port where the server has them (each still pose, under "
                "%.1f units)" % (b.name, what, VIEW_GAP), views and worst_r < VIEW_GAP and worst_p < VIEW_GAP,
                "the most apart: round %.2f, port %.2f (%d poses: %s)" % (worst_r, worst_p, len(views), ", ".join(
                    "%.2f/%.2f" % v for v in views)))
    inv, _ = inventories(srv)
    mine = inv.get(eb - 1, {}).get(key, 0)
    clip = inv.get(eb - 1, {}).get("clip", 0)
    s.check("use: the %s taken up goes into %s's own gun at its port (its magazine no longer empty), his reserve "
            "unchanged" % (what, b.name), loaded is not None and clip > 0 and mine == mine0,
            "server: %s; his gun's magazine %d; %s's %s %s -> %s" % (loaded[1].strip() if loaded else None, clip, b.name,
                                                                    key, mine0, mine))


FACE_GAP = 40.0  # mp_melee: units between the two players, face to face (their boxes 8 apart: a punch's reach)
# The mock's punch (melee_phase_test.sh's grunt_fast: a closed fist, 0.85 m forward in 0.15 s, 5.7 m/s), held out, then
# drawn back; the bash (the weapon held across, as the guard: a parry's stance), pushed 0.45 m forward in 0.1 s.
PUNCH_PLAY = ("0.000 main 0.00 1.15 -0.20 0 0 0", "0.300 main 0.00 1.15 -0.20 0 0 0", "0.450 main 0.00 1.15 -1.05 0 0 0",
              "1.000 main 0.00 1.15 -1.05 0 0 0", "1.600 main 0.00 1.15 -0.20 0 0 0", "2.000 main 0.00 1.15 -0.20 0 0 0")
GUARD = "vr_mock_hand main 0.15 1.35 -0.35 0 90 0"  # a crowbar held across in front (parry_pose_test.sh's guard)
BASH_PLAY = ("0.000 main 0.15 1.35 -0.35 0 90 0", "0.600 main 0.15 1.35 -0.35 0 90 0", "0.700 main 0.15 1.35 -0.80 0 90 0",
             "1.200 main 0.15 1.35 -0.80 0 90 0", "1.700 main 0.15 1.35 -0.35 0 90 0", "2.000 main 0.15 1.35 -0.35 0 90 0")
MELEE_SPOT = OPEN_SPOT  # e1m1: the attacker here facing north, the other FACE_GAP north of him facing south
# The shove (VR_Bash_Stance 2: an open hand, its palm facing ahead: the main hand's palm is its -right, so turned 90
# degrees to the right), pushed forward as the bash is.
PALM = "vr_mock_hand main 0.15 1.35 -0.35 0 -90 0"
SHOVE_PLAY = ("0.000 main 0.15 1.35 -0.35 0 -90 0", "0.600 main 0.15 1.35 -0.35 0 -90 0",
              "0.700 main 0.15 1.35 -0.80 0 -90 0", "1.200 main 0.15 1.35 -0.80 0 -90 0",
              "1.700 main 0.15 1.35 -0.35 0 -90 0", "2.000 main 0.15 1.35 -0.35 0 -90 0")


def health_of(srv, ent):
    """Player `ent`'s health on the server."""
    inv, _ = inventories(srv)
    return inv.get(ent - 1, {}).get("health", 0)


def face_off(s, srv, att, dfd, ea, ed):
    """`att` at MELEE_SPOT facing north, `dfd` FACE_GAP north of him facing him -> (att's origin, dfd's origin)."""
    put_at(srv, dfd, ed, side_spot(srv, ea))  # (out of the way first: the two may be trading places; the coop start
    #                                             the attacker is not on: B onto A's, 1 run of 2, stayed stuck where he was)
    oa = put_at(srv, att, ea, MELEE_SPOT, 90)
    od = put_at(srv, dfd, ed, (MELEE_SPOT[0], MELEE_SPOT[1] + FACE_GAP, MELEE_SPOT[2]), 270)
    return oa, od


def play(s, inst, name, lines):
    """`lines` written to the run's folder as a mock play, played on `inst` -> the play's path."""
    path = os.path.join(s.out, "%s_%s.txt" % (name, inst.name))
    with open(path, "w", encoding="ascii") as f:
        f.write("\n".join(lines) + "\n")
    inst.cmd("vr_mock_play %s" % path)
    return path


def damage_line(srv, ent, since, timeout, what):
    """The server's `damage: player <n> by a melee blow` for player `ent` after `since` -> its damage (None: none)."""
    got = wait_quietly(srv, r"damage: player [-\d.]+ by a melee blow .*health", timeout, since, what)
    return float(re.search(r"damage: player ([-\d.]+)", got[1]).group(1)) if got else None


def punch(s, srv, att, dfd, ea, ed, label):
    """`att`'s closed fist punched at `dfd` (face to face, FACE_GAP apart): the server deals it to `dfd` -> its damage."""
    face_off(s, srv, att, dfd, ea, ed)
    att.cmd("vr_mock_hand main 0.00 1.15 -0.20 0 0 0; vr_mock_button main grip 1")
    weapon_until(s, srv, ea, 0)
    before = health_of(srv, ed)
    since = srv.mark()
    play(s, att, "punch", PUNCH_PLAY)
    dmg = damage_line(srv, ed, since, 15, "%s's punch landing" % att.name)
    after = health_of(srv, ed)
    s.check("%s: %s's punch lands on %s (the server's blow, judged by %s's own hand; his health down)" % (
        label, att.name, dfd.name, att.name), dmg is not None and dmg > 0 and after < before,
        "damage %s; %s's health %d -> %d" % (dmg, dfd.name, before, after))
    att.cmd("vr_mock_play; vr_mock_button main grip 0")
    return dmg


def guard_up(s, srv, inst, ent):
    """`inst` holds a crowbar across in front (his guard, parry_pose_test.sh's), given by the test (impulse 167); kept
    if he holds it already (his grip still pressed: no new press to wait for)."""
    if weapon_until(s, srv, ent, 17, 1)[-1] == 17:
        inst.cmd(GUARD)
        return
    grip_then_give(srv, inst, "%s; vr_mock_button main grip 1" % GUARD, "main", 167)
    weapon_until(s, srv, ent, 17)


def pushed(srv, ent, start, way, need, tries=40):
    """Player `ent`'s farthest from `start` along y times `way` (+1 north) by report, until `need` units -> (that far,
    where) (a definite end: `tries` reports)."""
    best, where = 0.0, None
    for _ in range(tries):
        at = org_of(status(srv), "mp_client", "slot", ent - 1)
        if at and (at[1] - start[1]) * way > best:
            best, where = (at[1] - start[1]) * way, at
        if best >= need:
            break
    return best, where


def parry(s, srv, att, dfd, ea, ed, label, open_dmg):
    """`dfd` (his crowbar across in front) parries `att`'s punch: the server's parry by `dfd`'s own guard, the blow's
    damage cut, `att` pushed back."""
    oa, _ = face_off(s, srv, att, dfd, ea, ed)
    guard_up(s, srv, dfd, ed)
    att.cmd("vr_mock_hand main 0.00 1.15 -0.20 0 0 0; vr_mock_button main grip 1")
    since = srv.mark()
    play(s, att, "punch", PUNCH_PLAY)
    got = wait_quietly(srv, r"parry: player with hand", 15, since, "%s's parry" % dfd.name)
    dmg = damage_line(srv, ed, since, 3, "the parried blow's damage")
    back, at = pushed(srv, ea, oa, -1, 16)
    s.check("%s: %s's guard parries %s's punch (the server's parry by his own crowbar)" % (label, dfd.name, att.name),
            got is not None, got[1].strip() if got else "no 'parry: player' on the server")
    s.check("%s: the parried blow is cut (%s, against %s unparried)" % (label, dmg, open_dmg),
            dmg is None or (open_dmg is not None and dmg < open_dmg), "damage after the parry: %s" % dmg)
    s.check("%s: %s is pushed back by the parry" % (label, att.name), back > 4,
            "%s's origin %s -> %s (%.1f back)" % (att.name, oa, at, back))
    att.cmd("vr_mock_play; vr_mock_button main grip 0")


def bash(s, srv, att, dfd, ea, ed, label):
    """`att` (his crowbar across, held) pushes it forward into `dfd`: the server's shove of a player, `dfd` moved."""
    _, od = face_off(s, srv, att, dfd, ea, ed)
    guard_up(s, srv, att, ea)
    since = srv.mark()
    play(s, att, "bash", BASH_PLAY)
    got = wait_quietly(srv, r"bash hit: player", 15, since, "%s's bash" % att.name)
    moved, at = pushed(srv, ed, od, 1, 16)
    s.check("%s: %s's bash shoves %s (the server's shove of a player)" % (label, att.name, dfd.name), got is not None,
            got[1].strip() if got else "no 'bash hit: player' on the server")
    s.check("%s: %s is moved by the shove" % (label, dfd.name), moved > 8,
            "%s's origin %s -> %s (%.1f away)" % (dfd.name, od, at, moved))
    att.cmd("vr_mock_play")


def shove(s, srv, att, dfd, ea, ed, label):
    """`att`'s empty main hand, open, its palm facing ahead, pushed forward into `dfd`: the server's shove (VR_Bash_Find
    and VR_Bash_Hit, as the bash's: an empty hand can only shove), `dfd` moved."""
    _, od = face_off(s, srv, att, dfd, ea, ed)
    att.cmd("vr_mock_button main grip 0; vr_mock_button main trigger 0; %s" % PALM)
    weapon_until(s, srv, ea, 0)
    since = srv.mark()
    play(s, att, "shove", SHOVE_PLAY)
    got = wait_quietly(srv, r"bash hit: player", 15, since, "%s's shove" % att.name)
    moved, at = pushed(srv, ed, od, 1, 16)
    s.check("%s: %s's open palm shoves %s (the server's shove of a player, his hand empty)" % (label, att.name,
            dfd.name), got is not None, got[1].strip() if got else "no 'bash hit: player' on the server")
    s.check("%s: %s is moved by the palm's shove" % (label, dfd.name), moved > 8,
            "%s's origin %s -> %s (%.1f away)" % (dfd.name, od, at, moved))
    att.cmd("vr_mock_play")


WID_SWORD = 13
WID_NAILGUN = 6
SWORD_GIVE = 150 + WID_SWORD  # the test give of a sword
# A sword swung level from the right to the left across the front (its blade turned 150 degrees in 0.2 s, the hand
# going across with it: about 13 m/s at the tip), held there, then brought back slowly.
SWING_PLAY = ("0.000 main 0.95 1.25 -0.15 0 -100 0", "0.400 main 0.95 1.25 -0.15 0 -100 0",
              "0.700 main -0.55 1.25 -0.45 0 75 0", "1.400 main -0.55 1.25 -0.45 0 75 0",
              "3.400 main 0.95 1.25 -0.15 0 -100 0", "3.600 main 0.95 1.25 -0.15 0 -100 0")
SWING_READY = "vr_mock_hand main 0.95 1.25 -0.15 0 -100 0"
# The other's crowbar held upright in front, across a level swing's way (square to its blade and its way: the
# directional parry's), its grip under the blade's height. (flatgreen3: the hand's pitch 59.5, was 90 under the old
# baseline's vr_gunangle 39.5; the shipped 70 turned the crowbar 30.5 further: 52 deg off square, no parry. Now 25-27
# deg off, score 1.00; 120.5 the other way: 79 off.)
UPRIGHT = "vr_mock_hand main -0.50 1.05 -0.45 59.5 0 0"
# (MP bots phase 1) The crowbar held level across in front, pointing to his right, over his chest: square to an overhead
# blow's blade and its way down (a bot's swing: a flat-screen player's overhead swing), across its way.
LEVEL_GUARD = "vr_mock_hand main -0.45 1.45 -0.50 0 -90 0"
JITTER_MS = 30.0  # B's link's jitter for the swing at it (sub-tick samples)
JITTER_SLACK = 0.2  # the jittered swing's strength within this share of the clean one's


EMPTY_HAND = "vr_mock_hand main 0.20 1.00 -0.10 0 0 0"  # a hand let go, drawn back low at the side


def settled(srv, ents, tries=100):
    """The weapons lying nearest each hand of players `ents` still (`nspeed` under 0.5, or none) in two reports running
    -> True; False after `tries` reports (a safety net)."""
    still = 0
    for _ in range(tries):
        g = ground_report(srv, "mp_drawn_sv")
        moving = any(g.get((e, h), {}).get("nspeed", 0.0) >= 0.5 for e in ents for h in (0, 1))
        still = 0 if moving else still + 1
        if still >= 2:
            return True
    return False


def give_until(s, srv, inst, ent, pose, impulse, wid):
    """`inst` holds `wid` in his main hand at `pose` (the test give `impulse` after his grip), kept if he has it."""
    has = weapon_until(s, srv, ent, wid, 1)[-1]
    if has == wid:
        inst.cmd(pose)
        return
    if has != 0:  # (another weapon in that hand: let go of first, for a new press of the grip; the hand drawn back
        #             and the weapon come to rest first, or the press retakes it falling against the hand, 1 run of 3)
        inst.cmd("vr_mock_button main grip 0; %s" % EMPTY_HAND)
        weapon_until(s, srv, ent, 0)
        settled(srv, (ent,))
    grip_then_give(srv, inst, "%s; vr_mock_button main grip 1" % pose, "main", impulse)
    weapon_until(s, srv, ent, wid)


def swing(s, srv, att, dfd, ea, ed, label, guard, open_dmg=None):
    """`att`'s sword swung level across `dfd` (face to face): `guard` None (his hands empty), "level" (his crowbar held
    across: the guard's pose, along the blade's way) or "upright" (across its way: the directional parry's) -> (the
    damage dealt or None, the server's dirparry line or None, its guard parry line or None)."""
    face_off(s, srv, att, dfd, ea, ed)
    for inst in (att, dfd):
        inst.cmd("give h 100")
    if guard is None:
        dfd.cmd("vr_mock_button main grip 0; %s" % EMPTY_HAND)
        weapon_until(s, srv, ed, 0)
    else:
        give_until(s, srv, dfd, ed, GUARD if guard == "level" else UPRIGHT, 167, 17)
    give_until(s, srv, att, ea, SWING_READY, SWORD_GIVE, WID_SWORD)
    # (a weapon just let go of, still falling between them, is struck by the swing: a blow on a thrown weapon and none
    # on the other, 1 run of 3)
    settled(srv, (ea, ed))
    since = srv.mark()
    if os.environ.get("MP_SWING_TRACE"):
        srv.cmd("developer 3")
    play(s, att, "swing", SWING_PLAY)
    got = wait_quietly(srv, r"damage: player [-\d.]+ by a melee blow .*health", 4 if guard == "upright" else 15, since,
                       "%s's swing landing" % att.name)
    dmg = float(re.search(r"damage: player ([-\d.]+)", got[1]).group(1)) if got else None
    zone = re.search(r" x([\d.]+) at ", got[1]) if got else None
    swing.strength = dmg / float(zone.group(1)) if dmg is not None and zone and float(zone.group(1)) > 0 else dmg
    drp = wait_quietly(srv, r"dirparry: player's blow (parried|lands)[^\n]*", 1 if dmg is not None else 4, since,
                       "the server's directional parry of %s's swing" % att.name)
    grd = wait_quietly(srv, r"parry: player with hand", 0.5, since, "a guard parry")
    att.cmd("vr_mock_play")
    drp = drp[1].strip() if drp else None
    s.note("%s, %s: damage %s (its strength, the zone's multiplier taken out: %s); %s" % (
        label, guard or "empty-handed", dmg, swing.strength, drp or "no dirparry line"))
    return dmg, drp, (grd[1].strip() if grd else None)


def buffer_ticks(srv, ent):
    """(B19) The server's move buffer for player `ent`: its ticks that ran more than one of his moves and those that ran
    none (vr_mp_status `tick_more=`, `tick_none=`) -> (more, none), or None."""
    for l in status(srv):
        m = re.match(r"mp_netmoves: slot=(\d+) .*tick_more=(\d+) tick_none=(\d+)", l)
        if m and int(m.group(1)) == ent - 1:
            return int(m.group(2)), int(m.group(3))
    return None


def netmoves_of(srv, ent):
    """(B20) The server's mp_netmoves fields for player `ent` (vr_mp_status), or {}."""
    for l in status(srv):
        m = re.match(r"mp_netmoves: slot=(\d+) ", l)
        if m and int(m.group(1)) == ent - 1:
            return fields(l)
    return {}


JITTER_MS = 30          # mp_jitter: B's delay varies by this much either way (with PREDICT_LAG): packets reordered
JITTER_DEPTH_LEAST = 3  # ... the move buffer's depth reached at least (vr_net_movebuffer 2 under a quiet link)


def jitter_lift(s):
    """(B20) The move buffer under a jittery link: B, PREDICT_LAG ms behind each way give or take JITTER_MS (his
    packets reordered past their repeats), falls onto e1m1's rising lift and rides it (predict_lift). The server reads
    his overtaken datagrams' moves (none lost: before, NetQuake dropped them, 6-11 of his moves never ran in a lift run
    at 30 ms, 50-70 at 60 ms) and its buffer deepens with his jitter (vr_net_movebuffer_max), so his moves still run one
    a tick: the lift's predicted place under a unit (before: 2-4 units every run)."""
    srv, a, b, ea, eb = join(s, lag=PREDICT_LAG, jitter=JITTER_MS)
    srv.cmd("vr_mp_watch 3")
    b.cmd("vr_mp_watch 3")
    predict_lift(s, srv, a, b, ea, eb)
    nm = netmoves_of(srv, eb)
    s.metric("jitter_netmoves", nm)
    # (a move overtaken past the depth in his first ticks, before the buffer has measured his jitter, may still go)
    s.check("B's moves lost at the server: 2 at most (his overtaken datagrams read)", nm and 0 <= int(nm.get("lost", -1)) <= 2,
            "lost %s of %s, stale ticks waited %s" % (nm.get("lost"), nm.get("run"), nm.get("tick_gap")))
    s.check("the move buffer deepened with his jitter (to %d at least)" % JITTER_DEPTH_LEAST,
            nm and int(nm.get("depth_max", 0)) >= JITTER_DEPTH_LEAST,
            "depth now %s, at most %s (spread %s)" % (nm.get("depth"), nm.get("depth_max"), nm.get("spread_max")))
    pr = s.metrics.get("b_predict_lift", {})
    s.check("the lift's predicted place under him moved under a unit", float(pr.get("lift_max", 99)) < PREDICT_CORR_MOST,
            "lift_max %s" % pr.get("lift_max"))


def merged_moves(srv, ent):
    """Player `ent`'s moves bunched on the server so far: merged (vr_mp_status `merged=`: a move in a frame with one of
    his already) plus, under the move buffer (B19), the moves it kept waiting a tick (`wait_sum=`) and its ticks with
    none to run (`tick_none=`: a late move's); -1: none."""
    merged, kept = -1, 0
    for l in status(srv):
        m = re.match(r"mp_client: slot=(\d+) .*merged=(\d+)", l)
        if m and int(m.group(1)) == ent - 1:
            merged = int(m.group(2))
        m = re.match(r"mp_netmoves: slot=(\d+) .*tick_none=(\d+) wait_sum=(\d+)", l)
        if m and int(m.group(1)) == ent - 1:
            kept = int(m.group(2)) + int(m.group(3))
    return merged + kept if merged >= 0 else -1


def blade(s, srv, a, b, ea, eb):
    """A blade's swing between players, each way: it lands on the other empty-handed; his crowbar held level across in
    front (the guard's pose) does not parry it (along its way: the directional parry reads the swing's line, not the
    guard); held upright across its way, it does (no damage, sparks); then B swings at A again with 30 ms of jitter on
    his link (moves bunched and merged on the server): the same hit."""
    clean = {}
    for att, dfd, e1, e2 in ((a, b, ea, eb), (b, a, eb, ea)):
        label = "%s's sword on %s" % (att.name, dfd.name)
        dmg, _, _ = swing(s, srv, att, dfd, e1, e2, label, None)
        clean[att.name] = swing.strength
        s.check("%s: the swing lands on him empty-handed" % label, dmg is not None and dmg > 0, "damage %s" % dmg)
        dmg, drp, grd = swing(s, srv, att, dfd, e1, e2, label, "level")
        s.check("%s: his crowbar held level along its way does not parry it (the line's, not the guard's)" % label,
                grd is None and drp is not None and "lands" in drp and dmg is not None and dmg > 0,
                "guard: %s; dirparry: %s; damage %s" % (grd, drp, dmg))
        dmg, drp, grd = swing(s, srv, att, dfd, e1, e2, label, "upright")
        s.check("%s: his crowbar held upright across its way parries it (no damage)" % label,
                drp is not None and "parried" in drp and dmg is None, "dirparry: %s; damage %s" % (drp, dmg))
    proxy = getattr(b, "proxy", None)
    if proxy:
        proxy.jitter = JITTER_MS / 1000.0
    m0 = merged_moves(srv, eb)
    dmg, drp, _ = swing(s, srv, b, a, eb, ea, "B's sword on A at %d ms jitter" % JITTER_MS, None)
    m1 = merged_moves(srv, eb)
    if proxy:
        proxy.jitter = 0.0
    ref, got = clean.get("B"), swing.strength
    s.metric("swing_jitter_strength", got if got is not None else -1)
    s.metric("swing_jitter_merged", m1 - m0)
    s.check("B's moves came bunched at %d ms jitter (merged, or kept by the move buffer, during the swing)" % JITTER_MS,
            m1 > m0, "%d of his moves merged or kept (%d -> %d)" % (m1 - m0, m0, m1))
    # (its strength: the damage less the zone's multiplier, which is where on A the blade met him: 0.6 an arm, 1.5 the
    # head; a few units either way at the same swing)
    # (B25b: the clean reference taken again once when the two differ: the swing is vr_mock_play's real-time gesture,
    # and under load one clean take came out at 4.98 against 3.92-3.98 in every other run, the jittered one 3.92)
    within = lambda r: got is not None and r is not None and abs(got - r) <= JITTER_SLACK * r
    refs = [ref]
    if not within(ref):
        swing(s, srv, b, a, eb, ea, "B's sword on A again without jitter (the reference again)", None)
        refs.append(swing.strength)
        s.note("B's clean swing taken again: strength %s (the first %s)" % (swing.strength, ref))
    s.check("B's swing at %d ms jitter lands as without (its strength within %d%%)" % (JITTER_MS, JITTER_SLACK * 100),
            within(refs[-1]), "strength %s (damage %s), without jitter %s" % (got, dmg, refs))


def melee(s, only_blade=False):
    """Melee between players, each way: A's punch hurts B (judged by A's hands on the server); B's guard parries A's
    punch (cut, A pushed back); B's bash shoves A; A's open palm shoves B; then the other way round; then a sword's
    swing each way (blade: landing, not parried by the guard, parried across its way; at 30 ms jitter)."""
    srv, a, b, ea, eb = join(s)
    a.cmd("name A")
    b.cmd("name B")
    srv.cmd("developer 1; vr_debug_shots 1; vr_melee_dmg_multiplier 0.25")
    for att, dfd, e1, e2 in ((a, b, ea, eb), (b, a, eb, ea)):
        if only_blade:
            break
        label = "%s on %s" % (att.name, dfd.name)
        dmg = punch(s, srv, att, dfd, e1, e2, label)
        parry(s, srv, att, dfd, e1, e2, label, dmg)
        bash(s, srv, dfd, att, e2, e1, "%s on %s" % (dfd.name, att.name))
        for inst in (a, b):  # (the crowbar let go of: both empty-handed again)
            inst.cmd("vr_mock_button main grip 0")
        shove(s, srv, att, dfd, e1, e2, label)
    blade(s, srv, a, b, ea, eb)
    srv.cmd("developer 0; vr_debug_shots 0")


# vr_mpavatar.hpp: the avatar's first byte (mpavatar::movesByte; the server's vr_mp_status `moves=`)
MV_CROUCHED, MV_LYING, MV_GROUND = 2, 4, 8
CROUCH_HEAD = 0.9  # m: the mock's head this high crouches (under the crouched box's top)
PRONE_HEAD = 0.4   # m: and this high lies down (crawl_test.py's: the eyes under vr_prone_eye)
LYING_PITCH = 30.0  # degrees: the watcher's copy of him laid at least this far forward (lying: 0.5 of the way at least)


def guard_counts(srv):
    """vr_mp_status's "mp_guard:" lines on the server -> {player entity: {moves, places, speeds, walks, fists, spots,
    rewinds}} (vr_moveguard.hpp: each client's own values the server held to a body's bounds)."""
    out = {}
    for l in status(srv):
        if l.startswith("mp_guard:"):
            f = {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", l)}
            out[f.pop("slot") + 1] = f
    return out


GUARD_CHEAT_S = 2.0  # mp_guard: seconds B's moves lie
# mp_guard: A's main hand from the mock's standing pose straight up over his head (the launcher pointing up), out ahead
# at arm's length, across to his left (pointing left), out ahead again: 0.4 s a move, 0.5 s held at each (at most
# 3.7 m/s, 3.5 rad/s).
GUARD_ARM_PLAY = ("0.000 main 0.2 1.3 -0.4 0 0 0", "0.400 main 0.15 2.75 -0.1 80 0 0", "0.900 main 0.15 2.75 -0.1 80 0 0",
                  "1.300 main 0.1 1.6 -0.8 0 0 0", "1.800 main 0.1 1.6 -0.8 0 0 0", "2.200 main -0.6 1.7 -0.4 0 80 0",
                  "2.700 main -0.6 1.7 -0.4 0 80 0", "3.100 main 0.1 1.6 -0.8 0 0 0", "3.300 main 0.1 1.6 -0.8 0 0 0")
GUARD_ARM_S = 3.4  # (the play's length and a frame or few)


def guard(s):
    """(Review 2026-11-18 finding 1; MP_DESIGN P2-6) A remote player's own values held to a body's bounds
    (vr_moveguard.hpp). A plays honestly as a tall player (his Height and head at 1.95 m) with a rocket launcher (a long
    weapon) held up at arm's length, then out ahead, his off hand on it: the server holds none of his values. B's moves
    lie (vr_mp_test_cheat: his main hand, muzzle and loading port 2000 units ahead, his hands' speeds 500 m/s, a room-scale
    walk of 20000 units a second, his view a second behind): the server holds each kind and counts it, his room-scale
    walk cut to a walking head's budget; a fist of 500-unit spheres (a raw vr_rig) held to a hand's size. B honest
    again: nothing more held."""
    srv, a, b, ea, eb = join(s)
    # (each held value named: "mp_guard_held:" lines; QVR_MP_GUARD_DEBUG 0.5: also those over half their bound)
    srv.cmd("vr_mp_guard_debug %s" % os.environ.get("QVR_MP_GUARD_DEBUG", "1"))
    honest_from = srv.mark()
    a.cmd("vr_height_calibration 1.95; vr_mock_hand head 0 1.95 0")
    wpn = arm_with(s, srv, a, ea, 10, "")
    s.check("A holds the rocket launcher (the server's report)", wpn and wpn[-1] == 10, "A's main weapon: %s" % wpn)
    # His arm carries the launcher from pose to pose as an arm does, 0.4 s a move (a play: the mock's jump in one frame
    # reads as 1.5 m in its 50 ms, 30 m/s, and the launcher's turn as a flick at its length: a throw's estimate over the
    # 40 m/s bound in 3 runs of 4 once the frames came faster), held 0.5 s at each.
    a_play = play(s, a, "guard_arm", GUARD_ARM_PLAY)
    s.note("A's arm's play: %s" % a_play)
    server_time_past(srv, server_time(srv) + GUARD_ARM_S)
    own = drawn_report(a, "mp_drawn_own").get((ea, 1), {})
    grips = sorted(i for i, v in own.get("spots", {}).items() if int(v[0]) == 1)
    if grips:  # (his off hand on its grip: held in both)
        a.cmd("vr_mock_hand main 0.1 1.6 -0.8 0 0 0; vr_mock_hand_to off heldspot %d palm; vr_mock_button off grip 1"
              % grips[0])
        server_time_past(srv, server_time(srv) + 1.0)
    s.note("A's drawn main hand: %s; its grips %s" % (own, grips))

    before = guard_counts(srv)
    a_held = [l for l in srv.lines[honest_from:] if l.startswith("mp_guard_held: slot=%d " % (ea - 1))]
    lines = status(srv)
    b_from = org_of(lines, "mp_client", "slot", eb - 1)
    b.cmd("vr_mp_test_cheat 15")
    # (a fist of one sphere of 500 units, past the short's 511: 32000 64ths)
    b.cmd("cmd vr_rig 2 1 1 0 0000000000007d00")
    server_time_past(srv, server_time(srv) + GUARD_CHEAT_S)
    b.cmd("vr_mp_test_cheat 0")
    during = guard_counts(srv)
    lines = status(srv)
    b_to = org_of(lines, "mp_client", "slot", eb - 1)
    ga, gb = during.get(ea, {}), during.get(eb, {})
    s.note("the server's counts before: %s; after B's lies: %s" % (before, during))
    held_a = {k: v for k, v in ga.items() if k != "moves" and v != 0}
    s.check("A's honest moves (tall, a long weapon up, out and across, two-handed): none of his values held",
            ga.get("moves", 0) > 100 and not held_a, "A: %s; held: %s" % (ga, a_held[:6]))
    s.check("B's lies held and counted: places, speeds, the room-scale walk, the view's rewind, the fist",
            all(gb.get(k, 0) > 0 for k in ("places", "speeds", "walks", "rewinds", "fists")), "B: %s" % gb)
    if b_from and b_to:
        travel = math.hypot(b_to[0] - b_from[0], b_to[1] - b_from[1])
        # (the budget: walkMost 6 m/s for the time and walkBurst 2 m, at the floor's 1.5 world scale: 39.4 units a metre)
        most = (6.0 * (GUARD_CHEAT_S + 1.0) + 2.0) * 39.4
        s.metric("cheat_travel", round(travel, 1))
        s.check("B's room-scale walk of 20000 units a second cut to a walking head's (under %.0f units in %.0f s)" % (
            most, GUARD_CHEAT_S), travel <= most, "B went %.1f units: %s -> %s" % (travel, b_from, b_to))
    pr = prefs_report(s, srv).get(eb, {})
    # (a sphere's centre within 0.3 m and its radius 0.1 m of a hand, at 39.4 units a metre)
    s.check("B's 500-unit fist sphere held to a hand's size (the server's fist sum)",
            0.0 < pr.get("fistsum1", 0.0) <= 0.1 * 39.4 + 1e-3 or pr.get("fist1", 0) > 1, "B's server prefs: %s" % pr)

    server_time_past(srv, server_time(srv) + 0.5)  # (his lying moves' repeats gone by)
    settled = guard_counts(srv).get(eb, {})
    server_time_past(srv, server_time(srv) + 1.5)
    after = guard_counts(srv).get(eb, {})
    s.check("B honest again: nothing more of his held", after.get("moves", 0) > settled.get("moves", 0) and all(
        after.get(k) == settled.get(k) for k in ("places", "speeds", "walks", "rewinds")),
        "B 0.5 s after: %s; 2 s after: %s" % (settled, after))

    # (review B6) B's room-scale walk alone past its budget (vr_mp_test_cheat 4): his replay cuts it as the server's moves
    # do, from the server's budget in his state (QVR_SVC_PREDICT): his moves' results still the server's bit for bit.
    # (A first steps off the walk's line, forward and sideways: e1m1's coop starts put him just east of B, where B's walk
    # east ran into him on the server and not in his replay, which traces no players -- 54 of 58 results off.)
    a.cmd("vr_mock_stick off 1 1")
    server_time_past(srv, server_time(srv) + 0.75)
    a.cmd("vr_mock_stick off 0 0")
    server_time_past(srv, server_time(srv) + 0.5)
    pr = predict_segment(s, b, "a room-scale walk past its budget", "vr_mp_test_cheat 4", 60)
    b.cmd("vr_mp_test_cheat 0")
    predict_exact_check(s, "past his room-scale budget (review B6)", pr)
    s.check("... and his replay cut it as the server did: no correction past a unit (review B6)",
            float(pr.get("corr_max", 99)) < 1.0, "corr_n=%s corr_max=%s" % (pr.get("corr_n"), pr.get("corr_max")))


def server_moves(srv, ent):
    """Player `ent`'s `moves=` on the server (vr_mp_status), -1: none."""
    for l in status(srv):
        m = re.match(r"mp_client: slot=(\d+) .*moves=(\d+)", l)
        if m and int(m.group(1)) == ent - 1:
            return int(m.group(2))
    return -1


def copy_of(s, watcher, ent):
    """The watcher's copy of player `ent`'s body by report -> its fields (moves, pitch, lying, headjoint...) or {}."""
    (lines,) = reports(s, [watcher])
    return body_of(lines, "mp_body").get(ent, {})


def near_xy(at, to, within):
    """Whether point `at` ([x, y, z] or None) is within `within` units of `to` across."""
    return bool(at) and bool(to) and math.hypot(at[0] - to[0], at[1] - to[1]) <= within


def moves_until(s, srv, mover, watcher, em, label, verb, how, test, tries=40):
    """The server's moves for `em` and the watcher's copy of him by report until test(server moves, copy) -> both."""
    want = "%s: %s" % (verb, how)
    sv, cp = -1, {}
    for _ in range(tries):
        sv, cp = server_moves(srv, em), copy_of(s, watcher, em)
        if sv >= 0 and cp and test(sv, cp):
            break
    ok = sv >= 0 and bool(cp) and test(sv, cp)
    s.check("%s: %s %s (the server: moves=%d; %s's copy: moves=%s pitch=%s lying=%s)" % (
        label, mover.name, want, sv, watcher.name, cp.get("moves"), cp.get("pitch"), cp.get("lying")), ok,
        "copy's head joint %s, climbing %s" % (cp.get("headjoint"), cp.get("climb")))
    return sv, cp


def moves_each(s, srv, mover, watcher, em, ew, label):
    """`mover` (FACE_GAP from `watcher`, face to face) crouches, lies down, gets up and jumps: the server's boxes and
    ground follow his head and keys, and the watcher's copy of his body lies down, gets up, leaves the ground with him."""
    _, od = face_off(s, srv, watcher, mover, ew, em)
    mover.cmd("vr_mock_hand head; vr_mock_hand main 0.2 1.0 -0.3 0 0 0; vr_mock_hand off -0.2 1.0 -0.3 0 0 0")
    # (the copy read where face_off put him: a copy still at his last place was read as his standing one, B8b)
    _, stand = moves_until(s, srv, mover, watcher, em, label, "stands", "standing, on the ground",
                           lambda sv, cp: sv & MV_GROUND and not sv & (MV_CROUCHED | MV_LYING) and
                           cp["moves"][0] == sv and cp["lying"][0] == 0.0 and near_xy(cp.get("headjoint"), od, 24))
    mover.cmd("vr_mock_hand head 0 %.2f 0" % CROUCH_HEAD)
    # (until the copy's head is down too: the moves byte is the newest message's, the head the interpolation buffer's,
    # a few frames behind it -- read at the byte alone, the copy stood still: B8b)
    top = stand.get("headjoint", [0.0, 0.0, 0.0])[2]
    _, crouch = moves_until(s, srv, mover, watcher, em, label, "crouches", "crouched (his crouched box), not lying",
                            lambda sv, cp: sv & MV_CROUCHED and not sv & MV_LYING and int(cp["moves"][0]) & MV_CROUCHED
                            and cp.get("headjoint") and top - cp["headjoint"][2] > 8)
    if stand.get("headjoint") and crouch.get("headjoint"):
        drop = stand["headjoint"][2] - crouch["headjoint"][2]
        s.check("%s: %s's copy of %s crouches with him (its head %.1f units lower)" % (label, watcher.name, mover.name,
                drop), drop > 8, "standing %s, crouched %s" % (stand["headjoint"], crouch["headjoint"]))
    mover.cmd("vr_mock_hand head 0 %.2f 0" % PRONE_HEAD)
    moves_until(s, srv, mover, watcher, em, label, "lies down", "lying (his lying box), the copy laid forward",
                lambda sv, cp: sv & MV_LYING and int(cp["moves"][0]) & MV_LYING and cp["pitch"][0] >= LYING_PITCH and
                cp["lying"][0] >= 0.5)
    mover.cmd("vr_mock_hand head")
    moves_until(s, srv, mover, watcher, em, label, "gets up", "standing again, the copy upright",
                lambda sv, cp: not sv & (MV_CROUCHED | MV_LYING) and cp["lying"][0] == 0.0 and cp["pitch"][0] < 1.0)
    mover.cmd("+jump")
    moves_until(s, srv, mover, watcher, em, label, "jumps", "off the ground (the server's and the copy's)",
                lambda sv, cp: not sv & MV_GROUND and not int(cp["moves"][0]) & MV_GROUND, 60)
    mover.cmd("-jump")


# e1m4's deep water (e1m1's pool is 24 units deeper than a player under it at most: he is on its bottom at once). By its
# BSP's player hull (scratch b5c_scan.py): free 200 units below and 380 north of SWIM_SPOT; the swimmer swims slowly
# north (cl_forwardspeed 30: no sinking while he swims), the watcher in the water 200 units west of his way.
SWIM_LEVEL = "e1m4"
SWIM_SPOT = (1072.0, 1300.0, 560.0)
SWIM_WATCH = (872.0, 1400.0, 560.0)
SWIM_PITCH = 15.0  # degrees: the watcher's copy of him tilted at least this far forward (vr_body_swim_tilt)


def swim_each(s, srv, mover, watcher, em, ew, label, spot, watch, yaw):
    """`mover` (put at `spot`, facing `yaw`) swims under water in e1m4 while `watcher` (at `watch`) watches: the server
    has him fully under (waterlevel 3) and off the bottom, and the watcher's copy of his body tilts forward, swimming.
    (Each put where he was already, the second time round: put at the spot the other had sunk from, A's setpos ran and
    the server kept him where he was, 1 run of 1: found stuck there, by something of the other's left at that spot.)"""
    mover.cmd("vr_mock_hand head; -forward; cl_forwardspeed 30")
    put_at(srv, mover, em, spot, yaw)
    put_at(srv, watcher, ew, watch)
    mover.cmd("+forward")
    moves_until(s, srv, mover, watcher, em, label, "swims", "under water, off the bottom; the copy tilted forward",
                lambda sv, cp: sv >> 4 & 3 == 3 and not sv & MV_GROUND and int(cp["moves"][0]) >> 4 & 3 == 3 and
                cp["pitch"][0] >= SWIM_PITCH, 60)
    mover.cmd("-forward")


def moves(s):
    """Moves synced (MP_PLAN.md, B5b, B5c): B crouches, lies down, gets up and jumps while A watches -- the server's
    boxes and ground follow him and A's copy of his body lies down and gets up with him; then the other way round. Then
    each swims in e1m4's deep water, the other watching: his copy swims tilted."""
    srv, a, b, ea, eb = join(s)
    a.cmd("name A")
    b.cmd("name B")
    moves_each(s, srv, b, a, eb, ea, "B, A watching")
    moves_each(s, srv, a, b, ea, eb, "A, B watching")
    since_a, since_b = a.mark(), b.mark()
    srv.cmd("changelevel %s" % SWIM_LEVEL)
    a.wait(r"mp_event: .*joined map=%s" % SWIM_LEVEL, 120, since_a, "A on %s" % SWIM_LEVEL)
    b.wait(r"mp_event: .*joined map=%s" % SWIM_LEVEL, 120, since_b, "B on %s" % SWIM_LEVEL)
    swim_each(s, srv, b, a, eb, ea, "B, A watching", SWIM_SPOT, SWIM_WATCH, 90)
    swim_each(s, srv, a, b, ea, eb, "A, B watching", SWIM_WATCH, SWIM_SPOT, 0)


# vrclimb (climb_plays.py's hang and overtop): facing the ledge east from CLIMB_SPOT; its hold (vr_climb_try 94 176 46:
# "lenient hold 98 176 49") 1.54 m up and 0.63 m ahead in play space at the clients' scale (31.7 units a metre: the
# mock's hand at 0.69 m ahead was at x 99.8; climb_plays.py's 1.638 and 0.69 were "no hold": inside the ledge). The
# watcher off the map's one start. The mantle's head leant in to the wall, as a player at a wall does.
CLIMB_SPOT = (78.0, 176.0, 24.0)
CLIMB_WATCH = (-40.0, -310.0, 24.0)  # (climb_jump.py's site, off the map's one start)
CLIMB_WATCH_YAW = 76.0  # (towards CLIMB_SPOT)
HOLD_Y, HOLD_FWD = 1.54, -0.63
CLIMB_LEAN = ("0.000 head 0 1.646 0", "0.600 head 0 1.62 -0.28")
MV_CLIMB_OFF, MV_CLIMB_MAIN = 64, 128  # vr_mpavatar.hpp: a hand holds a ledge (the server's hold, B5c)
MANTLE_RISE = 24.0  # units: the mantle puts him on the ledge's top at least this much higher
MANTLE_TRIES = 3    # the overtop play at most this often (a definite end)


def hang_play():
    """Both hands onto the ledge, pulled down 0.25 m (no mantle) and held there (30 s: the play stopped once seen); the
    head upright, as climb_plays.py's hang (leant in, the hands went into the face: "no hold")."""
    lines = ["0.000 head 0 1.646 0"]
    for h, x in (("off", -0.12), ("main", 0.12)):
        at = "%s %.2f %.3f %.2f 40 0 0" % (h, x, HOLD_Y, HOLD_FWD)
        down = "%s %.2f %.3f %.2f 40 0 0" % (h, x, HOLD_Y - 0.25, HOLD_FWD)
        lines += ["0.000 %s %.2f 1.2 -0.3 40 0 0" % (h, x), "0.500 " + at, "1.000 " + at, "2.000 " + down,
                  "30.000 " + down]
    return lines + ["0.700 cmd +graboff", "0.750 cmd +grabmain"]


def overtop_play():
    """climb_plays.py overtop: both hands on the ledge, pulled down to the hips and in, pushed down and out over the
    top (the mantle), let go at 4 s."""
    lines = list(CLIMB_LEAN)
    for h, x in (("off", -0.12), ("main", 0.12)):
        lines += ["0.000 %s %.2f 1.1 -0.2 70 0 0" % (h, x * 2), "0.600 %s %.2f %.3f %.2f 70 0 0" % (h, x, HOLD_Y, HOLD_FWD),
                  "1.000 %s %.2f %.3f %.2f 70 0 0" % (h, x, HOLD_Y, HOLD_FWD), "2.000 %s %.2f 1.0 -0.35 70 0 0" % (h, x),
                  "2.300 %s %.2f 1.0 -0.35 70 0 0" % (h, x), "2.900 %s %.2f 0.95 -0.75 70 0 0" % (h, x),
                  "4.000 %s %.2f 0.95 -0.75 70 0 0" % (h, x), "5.000 %s %.2f 1.1 -0.2 70 0 0" % (h, x * 2)]
    return lines + ["0.700 cmd +graboff", "0.750 cmd +grabmain", "4.000 cmd -graboff", "4.000 cmd -grabmain"]


def climb_each(s, srv, mover, watcher, em, ew, label):
    """`mover` takes vrclimb's ledge with both hands and hangs from it while `watcher` watches: the server holds him
    (his own holds, per player), the watcher's copy of his body climbs (its climbing weight: the trunk hanging under the
    head, the feet off the floor); he lets go: both back; then he mantles onto the ledge: the server puts him on top."""
    mover.cmd("vr_mock_play; -graboff; -grabmain; vr_mock_hand head")
    start = put_at(srv, mover, em, CLIMB_SPOT, 0)  # (first: the watcher's spot is the last mover's, now on the ledge)
    put_at(srv, watcher, ew, CLIMB_WATCH, CLIMB_WATCH_YAW)
    play(s, mover, "hang", hang_play())
    both = MV_CLIMB_OFF | MV_CLIMB_MAIN
    moves_until(s, srv, mover, watcher, em, label, "hangs", "both hands on the ledge (the server's holds and the copy's)",
                lambda sv, cp: sv & both == both and int(cp["moves"][0]) & both == both and cp.get("climb", [-1.0])[0] >= 0.9, 60)
    mover.cmd("vr_mock_play; -graboff; -grabmain")
    moves_until(s, srv, mover, watcher, em, label, "lets go", "no holds; the copy climbs no more",
                lambda sv, cp: not sv & both and not int(cp["moves"][0]) & both and cp.get("climb", [-1.0])[0] == 0.0, 60)
    # (B17: up to MANTLE_TRIES plays, as a player tries again: the play's keyframes are real time, and on a loaded PC its
    # coarse frames once took no hold (B8j's flake; the suite's B17 run 2: B left under the ledge, A then never put
    # there). Each attempt noted.)
    top = None
    for attempt in range(MANTLE_TRIES):
        mover.cmd("vr_mock_play; -graboff; -grabmain")
        start = put_at(srv, mover, em, CLIMB_SPOT, 0)
        since = srv.mark()
        t0 = time.time()
        play(s, mover, "overtop", overtop_play())
        for _ in range(80):
            at = org_of(status(srv), "mp_client", "slot", em - 1)
            if at and at[2] - start[2] >= MANTLE_RISE:
                top = at
                break
        if top:
            break
        s.note("%s: %s's mantle %d took him nowhere: played again" % (label, mover.name, attempt + 1))
    mantled = wait_quietly(srv, r"climb: .*mantle", 1, since, "the server's mantle")
    s.check("%s: %s mantles onto the ledge (the server's: from %s to %s)" % (label, mover.name, start, top),
            top is not None, "the server's mantle line: %s" % (mantled[1].strip() if mantled else None))
    # (syncfix5) His mantle's grunt is a server sound (climb::grunt, CHAN_VOICE): the watcher hears it from him.
    heard = near_counts(near_lines(watcher, em, time.time() - t0 + 0.5))
    grunts = ("player/plyrjmp8.wav", "player/land2.wav", "vr/derived/plyrjmp8_low.wav", "vr/derived/land2_low.wav",
              "player/pain2.wav")  # (vr_climb.cpp mantleGrunts)
    s.check("%s: %s's mantle grunt heard by %s from him (a server sound)" % (label, mover.name, watcher.name),
            any(heard.get("sound:" + g, 0) for g in grunts),
            "the watcher's sounds round him: %s" % (sorted(k for k in heard if k.startswith("sound:")) or "-"))
    mover.cmd("vr_mock_play; -graboff; -grabmain")


def climb(s):
    """Climbing between players (MP_PLAN.md, B5c), on vrclimb (one start: A moved off it before B joins): B hangs from
    the ledge while A watches, lets go, mantles onto it; then A, B watching."""
    s.client_sound = True  # (syncfix5: the watcher hears the mantle's grunt)
    srv, a, b, ea, eb = join(s, level="vrclimb", first=lambda srv, a, ea: put_at(srv, a, ea, CLIMB_WATCH,
                                                                                         CLIMB_WATCH_YAW))
    a.cmd("name A")
    b.cmd("name B")
    climb_each(s, srv, b, a, eb, ea, "B, A watching")
    climb_each(s, srv, a, b, ea, eb, "A, B watching")


LOSS = 0.10              # mp_loss: B's game packets lost, each way
TRACE_FRAMES = 300       # B's frames traced each time (about 4 s)
UP_BUDGET = 12 * 1024    # B's link up in play, B/s (MP_PLAN.md, section 2.3: 8-12 KB/s, B7's quantized delta moves)
LOST_MOST = 0.02         # B's moves the server never runs, at most (one is lost only with three packets in a row)
JUMP_RATIO, JUMP_SLACK = 3.0, 2.0  # a frame's step over 3 times both its neighbours' and 2 units more: a jump
SMOOTH_MOST = 0.5        # the hands' offset from the body: median |second difference| / median |step| at most


def trace_of(lines):
    """B's "mp_trace:" lines -> [(cl.time, org, h0, h1)]."""
    out = []
    for l in lines:
        if not l.startswith("mp_trace: t="):
            continue
        p = l.split()
        vec3 = {}
        for i, tok in enumerate(p):
            key = tok.split("=")[0]
            if key in ("org", "h0", "h1") and i + 2 < len(p):
                vec3[key] = [float(tok.split("=")[1]), float(p[i + 1]), float(p[i + 2])]
        ct = float(fields(l)["ct"])
        out.append((ct, vec3["org"], vec3["h0"], vec3["h1"]))
    return out


def steps(points):
    return [dist(points[i], points[i - 1]) for i in range(1, len(points))]


def jumps(st, dts=None):
    """Frames whose step is over JUMP_RATIO times both neighbours' and JUMP_SLACK more (a position that jumps). With
    `dts` (each step's advance of the clock the copy is drawn at: B8c), the neighbours' speeds times this frame's
    advance: a long frame on a busy machine moves the copy further without a jump."""
    if dts is None or len(dts) != len(st):
        return sum(1 for i in range(1, len(st) - 1) if st[i] > JUMP_RATIO * max(st[i - 1], st[i + 1]) + JUMP_SLACK)
    n = 0
    for i in range(1, len(st) - 1):
        lo, hi = dts[i - 1], dts[i + 1]
        near = max(st[i - 1] / lo * dts[i] if lo > 1e-5 else st[i - 1],
                   st[i + 1] / hi * dts[i] if hi > 1e-5 else st[i + 1])
        n += 1 if st[i] > JUMP_RATIO * near + JUMP_SLACK else 0
    return n


def clock_steps(lines):
    """B's "mp_trace:" lines -> each frame's advance of the buffer's clock (rt), or None (an older build's trace)."""
    tl = [l for l in lines if l.startswith("mp_trace: t=")]
    if not tl or any(" rt=" not in l for l in tl):
        return None
    rts = [float(fields(l)["rt"]) for l in tl]
    return [rts[i] - rts[i - 1] for i in range(1, len(rts))]


def median(xs):
    xs = sorted(xs)
    return xs[len(xs) // 2] if xs else 0.0


def roughness(points):
    """median |second difference| / median |step| of a path (0 smooth; a staircase of steps and stops: about 1 or more)."""
    st = steps(points)
    d2 = [dist([points[i + 1][k] - points[i][k] for k in range(3)], [points[i][k] - points[i - 1][k] for k in range(3)])
          for i in range(1, len(points) - 1)]
    return median(d2) / max(median(st), 1e-3)


def loss_trace(s, a, b, ea, lerp, label):
    """A walks and turns (the mock's sticks: a circle) while B traces his copy of A for TRACE_FRAMES frames with
    vr_mp_avatar_lerp `lerp` -> (body jumps, hand jumps, the hands' offset roughness, frames a second)."""
    since = b.mark()
    b.cmd("vr_mp_avatar_lerp %d; vr_mp_trace %d %d" % (lerp, ea, TRACE_FRAMES))
    b.wait(r"^mp_trace: done", 120, since, "B's trace of A (%s)" % label)
    tr = trace_of(b.lines[since:])
    body = [t[1] for t in tr]
    offs = [[t[2][k] - t[1][k] for k in range(3)] for t in tr]
    offs1 = [[t[3][k] - t[1][k] for k in range(3)] for t in tr]
    dts = clock_steps(b.lines[since:])
    body_j = jumps(steps(body), dts)
    hand_j = jumps(steps([t[2] for t in tr]), dts) + jumps(steps([t[3] for t in tr]), dts)
    rough = max(roughness(offs), roughness(offs1))
    span = tr[-1][0] - tr[0][0] if len(tr) > 1 else 0.0
    fps = (len(tr) - 1) / span if span > 0 else 0.0
    s.note("%s: %d frames (%.0f a second of B's game clock), body step median %.2f, jumps body %d hands %d, hand offset "
           "roughness %.2f" % (label, len(tr), fps, median(steps(body)), body_j, hand_j, rough))
    return body_j, hand_j, rough, fps


def lossy(s):
    """MP_PLAN B7: B's link loses 10% of the game packets each way. B's moves still reach the server (each packet
    repeats the moves after the one it acknowledged: the server runs nearly all of them); B's copy of A, walking in a
    circle, moves continuously (no jump of his body or hands frame to frame) and his hands ride his body smoothly
    (vr_mp_avatar_lerp: between the server's last two messages, in step with his entity's lerp; off for comparison);
    B's link up in play under the budget."""
    srv, a, b, ea, eb = join(s, loss=LOSS)
    proxy = b.proxy
    since_srv = srv.mark()
    srv.cmd("vr_mp_status")
    srv.wait(r"mp_server: map=", 10, since_srv)
    before = [l for l in srv.lines[since_srv:] if l.startswith("mp_netmoves: slot=%d " % (eb - 1))]
    start = proxy.snapshot()
    a.cmd("vr_mock_stick off 0 1; vr_mock_stick main 0.5 0")
    b.cmd("vr_mock_stick off 0 1")  # (B walks too: his moves up change every frame)
    off = loss_trace(s, a, b, ea, 0, "lerp off")
    on = loss_trace(s, a, b, ea, 1, "lerp on")
    w = proxy.window(start, proxy.snapshot())
    a.cmd("vr_mock_stick off 0 0; vr_mock_stick main 0 0")
    b.cmd("vr_mock_stick off 0 0")
    s.metric("b_link_loss", w)
    s.note("B's link in play at %d%% loss: up %s B/s (mean %s B), down %s B/s (mean %s B)" % (
        LOSS * 100, w["up"]["bytes_s"], w["up"]["mean_B"], w["down"]["bytes_s"], w["down"]["mean_B"]))
    since_srv = srv.mark()
    srv.cmd("vr_mp_status")
    srv.wait(r"mp_server: map=", 10, since_srv)
    nm = [fields(l) for l in srv.lines[since_srv:] if l.startswith("mp_netmoves: slot=%d " % (eb - 1))]
    s.check("the server reads B's moves as QVR_CLC_MOVES", bool(nm) and int(nm[-1]["run"]) > 0,
            "%s; before the walk: %s" % (nm[-1] if nm else "none", before[-1] if before else "none"))
    if nm:
        run_n, lost = int(nm[-1]["run"]), int(nm[-1]["lost"])
        s.metric("b_moves", nm[-1])
        s.check("at %d%% loss the server runs all but %d%% of B's moves (the repeats)" % (LOSS * 100, LOST_MOST * 100),
                lost <= LOST_MOST * (run_n + lost), "run %d, lost %d, recovered from repeats %s, undecodable %s" % (
                    run_n, lost, nm[-1]["recovered"], nm[-1]["undecodable"]))
        s.check("some of B's moves came only in a later packet's repeats", int(nm[-1]["recovered"]) > 0,
                "recovered %s" % nm[-1]["recovered"])
        s.check("every QVR_CLC_MOVES had its base (the acknowledged move)", int(nm[-1]["undecodable"]) == 0,
                "undecodable %s" % nm[-1]["undecodable"])
    # (B7b: the entities' interpolation buffer -- a delay behind the newest message, adaptive to the gaps, with a
    # history of messages -- draws his body and hands without a jump; NetQuake's two-message lerp jumped 1-3 times.)
    s.metric("loss_jumps", {"body": on[0], "hands": on[1], "body_lerp_off": off[0], "hands_lerp_off": off[1]})
    s.check("at %d%% loss A's body never jumps in B's view (%d frames)" % (LOSS * 100, TRACE_FRAMES), on[0] == 0,
            "%d jumps (avatar lerp off: %d)" % (on[0], off[0]))
    s.check("at %d%% loss A's hands never jump in B's view" % (LOSS * 100), on[1] == 0,
            "%d jumps (avatar lerp off: %d)" % (on[1], off[1]))
    cs = client_snap_status(b)
    s.metric("b_snapcl", cs)
    s.check("B reads every snapshot that came (each one's acknowledged base kept)",
            bool(cs) and int(cs["decoded"]) > 0 and int(cs["undecodable"]) == 0, "%s" % cs)
    s.check("B draws the others from the interpolation buffer", bool(cs) and int(cs["interp"]) == 1, "%s" % cs)
    s.check("A's hands ride his body smoothly in B's view (offset roughness at most %.1f)" % SMOOTH_MOST,
            on[2] <= SMOOTH_MOST, "%.2f (lerp off: %.2f; B's frames a second %.0f)" % (on[2], off[2], on[3]))
    # (mpopen) at the server's 72 packets a second: B sends a packet a frame, and his frames a second follow the machine's
    # load (77-90 in the suite), so the raw B/s moved with it (9525-12975 against 12288 run to run); the mean packet
    # does not (119-144 B)
    up72 = w["up"]["mean_B"] * 72
    s.check("B's link up walking under %d B/s at %d%% loss (its mean packet at 72 a second)" % (UP_BUDGET, LOSS * 100),
            up72 < UP_BUDGET, "%d B/s at 72 packets a second: mean %d B (was 640 B a move); measured %d B/s at %.1f packets "
            "a second" % (up72, w["up"]["mean_B"], w["up"]["bytes_s"], w["up"]["pkts_s"]))


PREDICT_LAG = 150        # mp_predict: B's added delay each way, ms (a 300 ms round trip)
PREDICT_MOVES_MOST = 2   # B's own move shows this many moves (client ticks) after his press at most: the press's, drawn as the next comes
PREDICT_CORR_MOST = 1.0  # a correction on flat ground (the replay all on the floor, no wall) at most, units
PREDICT_LEGS = 4         # B walks there and back twice
PREDICT_LEG_STATES = 72  # each leg: this many of the server's states read (about a second)
PART, PART_REPEAT, PART_WATCH = "", 1, 1  # --part, --repeat, --watch (predict's segments alone)
PART_JITTER, PART_LOSS = 0.0, 0.0  # (B20) predict --part: --jitter/--loss on B's link too (with PREDICT_LAG)
AMMO_PART = ""  # --part give|use: mp_disarm's rounds alone (handed over; loaded into B's gun)
PERMOVE_SKIP = 0  # --permove-skip: the server's vr_net_permove_skip in predictclimb (a check that its parts fail unpredicted)


def predict_report(inst):
    """vr_mp_status's "mp_predict:" fields on a client ({} none)."""
    return fields(next((l for l in status(inst) if l.startswith("mp_predict:")), ""))


def predict_until(s, inst, key, at_least, what, tries=200):
    """Polls `inst`'s prediction report until its `key` is `at_least` -> the report (a definite end: `tries`)."""
    pr = {}
    for _ in range(tries):
        pr = predict_report(inst)
        if pr and float(pr.get(key, 0)) >= at_least:
            return pr
    raise StepFailed("%s: %s never reached %s (%s)" % (what, key, at_least, pr))


def rest_gap(s, srv, b, eb, what, states=36, tries=5):
    """(mpflakes) B at rest on both sides -> (gap, B's drawn place, the server's): the server's copy of him read until it
    stood still over `states` of his client's states counted from now (each read; at most `tries`), then his own place.
    (The waits were counted from a planned state number: when the polls had overshot it, the reads came at once, the
    server's copy still sliding to his stop a round trip behind his prediction: 1.4-1.9 units off.)"""
    own = org_of(status(b), "mp_player", "self", 1)
    sv = org_of(status(srv), "mp_client", "slot", eb - 1)
    s.note("B at rest (%s): at once %.2f units from the server's copy (%s, %s)" % (
        what, dist(own, sv) if own and sv else 99.0, own, sv))
    for _ in range(tries):
        predict_until(s, b, "states", int(predict_report(b).get("states", 0)) + states, "B at rest (%s)" % what)
        sv2 = org_of(status(srv), "mp_client", "slot", eb - 1)
        still = bool(sv and sv2) and dist(sv, sv2) < 0.05
        sv = sv2
        if still:
            break
    own = org_of(status(b), "mp_player", "self", 1)
    return (dist(own, sv) if own and sv else 99.0), own, sv


def own_move_frames(s, b, label, go, stop):
    """B's own drawn entity moving off after his press, in his moves sent (vr_mp_predict_probe) -> (moves, ms)."""
    since = b.mark()
    b.cmd("vr_mp_predict_probe %s; %s" % (label, go))
    _, moved = b.wait(r"mp_predict_moved: .*%s" % label, 20, since, "B's own move (%s)" % label)
    f = fields(moved)
    # (moving as his watch sees him first: stopped at once, a few units off, he was never "moving" and no stop came)
    b.wait(r"mp_move: .*self=1", 20, since, "B moving (%s)" % label)
    b.cmd(stop)
    b.wait(r"mp_stop: .*self=1", 20, since, "B at rest again (%s)" % label)
    return int(f["moves"]), int(f["ms"])


def predict_segment(s, b, what, go, states, legs=(), reset=True):
    """B's prediction over one segment of moves (the report reset, `go` sent, then each of `legs` (console commands) for
    `states` of the server's states read) -> the report at its end."""
    first = "%s%s" % ("vr_mp_predict_report reset; " if reset else "", go)
    if first:
        b.cmd(first)
    n = int(predict_report(b).get("states", 0))
    for leg in legs or ("",):
        if leg:
            b.cmd(leg)
        n += states
        pr = predict_until(s, b, "states", n, "B %s" % what)
    s.note("B's prediction, %s: %s" % (what, " ".join("%s=%s" % kv for kv in sorted(pr.items()))))
    return pr


PREDICT_EXACT_MISS_MOST = 0.01  # B14: a share of his moves' results off the server's (bit for bit) at most ...
PREDICT_EXACT_OFF_MOST = 0.001  # ... and how far, units (an ulp on a step: the client's trace and SV_Move's)


def predict_exact_check(s, what, pr, least=20, lost=0):
    """MP_PLAN B14: his moves are deterministic -- his replay's result for the move each new state is after against the
    server's, bit for bit (the origin; hash_sv and hash_cl, FNV-1a over the states compared, equal when all are): at
    least `least` compared, at most PREDICT_EXACT_MISS_MOST of them off, none by PREDICT_EXACT_OFF_MOST. `lost`: moves
    the server never had in the segment (each of its packets lost: moves_lost): the state after one is a move short of
    his replay, so up to that many more off, by at most LOST_MOVE_MOST."""
    n, miss = int(pr.get("exact_n", 0)), int(pr.get("exact_miss", 999))
    off_most = LOST_MOVE_MOST if lost else PREDICT_EXACT_OFF_MOST
    s.check("B's moves' results equal the server's bit for bit %s (an ulp at most, rarely)" % what,
            n >= least and miss <= PREDICT_EXACT_MISS_MOST * (n + miss) + lost and
            float(pr.get("exact_max", 99)) < off_most and (miss > 0 or pr.get("hash_sv") == pr.get("hash_cl")),
            "%s equal, %s not (by %s at most; velocity: %s not; %s held to the server's clock, not compared; %d of his "
            "moves the server never had), hashes %s %s" % (n, miss, pr.get("exact_max"), pr.get("vel_miss"),
                                                          pr.get("exact_held"), lost, pr.get("hash_sv"), pr.get("hash_cl")))


def predict_corr_check(s, what, pr, least=20, lost=0):
    """The segment's corrections and drawn offset under PREDICT_CORR_MOST (at least `least` states compared). (B18) A
    move that met a mover is measured apart (contact_max: predict_lift's check); the drawn offset eases it in, so it
    may reach it. `lost` (predict_exact_check's): a move the server never had is corrected by its length, at most
    LOST_MOVE_MOST."""
    contact = float(pr.get("contact_max", 0))
    most = LOST_MOVE_MOST if lost else PREDICT_CORR_MOST
    s.check("B's corrections %s under %.1f unit (and his drawn offset)" % (what, most),
            int(pr.get("corr_n", 0)) >= least and float(pr.get("corr_max", 99)) < most and
            float(pr.get("err_max", 99)) < max(most, contact + 0.1),
            "max %s over %s states (in the air: %s over %s), drawn offset at most %s; meeting a mover %s over %s; %d of "
            "his moves the server never had" % (pr.get("corr_max"), pr.get("corr_n"), pr.get("air_max"), pr.get("air_n"),
                                                pr.get("err_max"), pr.get("contact_max"), pr.get("contact_n"), lost))


# A move the server never had (every packet carrying it lost): his replay ran it, the server did not -- his correction
# and that state's miss a move's travel at most: a slide's top speed (twice a run's 320 u/s, with room) for 1/72 s.
LOST_MOVE_MOST = 2.0 * 320.0 / 72.0 + 1.0


def moves_lost(srv, ent):
    """The server's count of player `ent`'s moves it never had (mp_netmoves lost=: every packet carrying one lost)."""
    return int(netmoves_of(srv, ent).get("lost", 0))


# e1m1's lift (func_plat *7, at the bottom until a player stands on it: 150 units up at 150 u/s, held up while he stays)
LIFT_TOP = 31.0 + 24.0               # his origin on it at the top
LIFT_STEP = 150.0 / 72               # its move in a server tick, units
# A on its one half at the bottom (its top -119) starts it; B over its other half, 60 units over his place on it at
# the top, falls onto it as it rises (B8e: a fall onto a moving lift)
LIFT_A = (-584.0, 2656.0, -94.0)
LIFT_DROP = (-504.0, 2656.0, LIFT_TOP + 60.0)
# e1m4's deep water (hull 0's contents: water 48 units round him, 60 over and under; scratch b8e_water2.py): swimming
SWIM_SPOT = (336.0, 1300.0, 388.0)
# the start map's pool (deep: he sinks), at its surface 16 units east of a ledge on its west edge (scratch b8f_ledge3.py)
WATERJUMP_SPOT = (1313.0, 1681.0, 86.0)
WATERJUMP_YAW = 180


def predict_moves(s, srv, a, b, ea, eb):
    """MP_PLAN B8d: the prediction's moves. A jump (QC's PlayerJump, run in the move whose key pressed it: his own jump
    shows at his press, and the air's corrections stay under a unit), a crouched walk (the crouched box's rules), and a
    ride on e1m1's lift (the lift he stands on carries him between the server's states: no correction, no offset)."""
    srv.cmd("vr_mp_watch 1")  # (the server's trace of B's jumps and held moves: mp_predict_sv)
    frames, ms = own_move_frames(s, b, "jumped", "+jump", "-jump")
    s.metric("own_jump_frames", frames)
    s.check("B's own jump shows %d move(s) after his press at most (%d ms each way)" % (PREDICT_MOVES_MOST, PREDICT_LAG),
            0 <= frames <= PREDICT_MOVES_MOST, "%d moves, %d ms" % (frames, ms))
    # (the walking jumps cross e1m1's 16-unit step south of the spot, under its ceiling: a crouched box read there from
    # his head against the server's origin, not his own, was a ceiling his replay met and the server's walk did not)
    pr = predict_segment(s, b, "jumping in place and walking", "+jump", 36,
                         ("-jump", "+jump", "-jump; vr_mock_stick off 0 0.5", "+jump", "-jump", "vr_mock_stick off 0 -0.5",
                          "+jump", "-jump; vr_mock_stick off 0 0", ""))
    s.metric("b_predict_jump", pr)
    s.check("B jumped (states read in the air)", int(pr.get("air_n", 0)) >= 20, "%s" % pr.get("air_n"))
    predict_corr_check(s, "jumping", pr)
    b.cmd("vr_mock_hand head 0 %.2f 0" % CROUCH_HEAD)
    predict_until(s, b, "states", int(predict_report(b).get("states", 0)) + 36, "B crouched")
    pr = predict_segment(s, b, "walking crouched", "", 72, ("vr_mock_stick off 0 0.5", "vr_mock_stick off 0 -0.5",
                                                            "vr_mock_stick off 0 0"))
    s.metric("b_predict_crouch", pr)
    s.check("B's crouched rules (his speed under the standing one's)", 0 < float(pr.get("maxspeed", 0)) < 300,
            "maxspeed %s" % pr.get("maxspeed"))
    predict_corr_check(s, "walking crouched", pr)
    b.cmd("vr_mock_hand head")
    # (standing up at once is a head's leap: a room-scale jump, his own)
    predict_until(s, b, "states", int(predict_report(b).get("states", 0)) + 72, "B stood up")
    predict_lift(s, srv, a, b, ea, eb)


def predict_lift(s, srv, a, b, ea, eb):
    """MP_PLAN B8e/B8f: B falls onto e1m1's lift as it rises and rides it up: his corrections stay under a unit."""
    b.cmd("notarget; god")
    a.cmd("notarget; god")
    # A on the lift at its bottom starts it up; B put over its other half falls onto it as it rises (counted from his
    # prediction's snap to the spot on: his replay meets the lift where it will be, B8e, and it carries him up). (Put
    # over it before it moves, his fall would start it: its start is the server's, a correction no replay can foresee.)
    put_at(srv, a, ea, LIFT_A, 90)
    # (mpflakes) The buffer's ticks counted from before he is put over it: read after his prediction's snap there, the
    # count began late and could miss a bunched tick near his landing -- the strict bound then applied to a landing
    # whose lift was a step off (ROUND25's "lift landing 1.354")
    tp = buffer_ticks(srv, eb)
    since = b.mark()
    put_at(srv, b, eb, LIFT_DROP, 90)
    b.wait(r"mp_predict_corr: c=\d{3,}", 20, since, "B's prediction put over the lift")
    b.cmd("vr_mp_predict_report reset")
    t0 = buffer_ticks(srv, eb)
    early = (t0[0] - tp[0]) + (t0[1] - tp[1]) if tp and t0 else -1
    s.note("the buffer's ticks off one a tick between his put-over and the report's reset: %d" % early)
    s.metric("b_predict_lift_buffer_off_early", early)
    t0 = tp or t0
    pr = predict_segment(s, b, "falling onto the lift and riding it", "", 36,
                         ("", "", "", "vr_mock_stick off 0.3 0", "vr_mock_stick off 0 0"), reset=False)
    t1 = buffer_ticks(srv, eb)
    s.metric("b_predict_lift", pr)
    # (B19) the server's move buffer: its ticks that ran more than one of B's moves or none during the segment (a backlog
    # over vr_net_movebuffer, or none kept to run: the lift a step off where his replay met it, as before the buffer)
    off = (t1[0] - t0[0]) + (t1[1] - t0[1]) if t0 and t1 else -1
    s.metric("b_predict_lift_buffer_off", off)
    s.check("B fell onto the lift while it rose (a landing on it moving)", int(pr.get("land_moving", 0)) >= 1,
            "%s landings" % pr.get("land_moving"))
    sv = org_of(status(srv), "mp_client", "slot", eb - 1)
    s.check("B rode the lift up (the server has him at its top; states the lift under him moved: 12 at least)",
            bool(sv) and abs(sv[2] - LIFT_TOP) < 2.0 and int(pr.get("ride_n", 0)) >= 12,
            "%s, %s states" % (sv, pr.get("ride_n")))
    predict_corr_check(s, "falling onto the lift and riding it", pr)
    # (B18) the move that met it: within one of its steps (one of his moves bunched with another in a server tick, or
    # a tick without one, in the moves not yet acknowledged: the lift a step off where his replay met it). (B19) The
    # server's move buffer runs his moves one a tick: under a unit, and the lift's own predicted place too, unless the
    # buffer itself ran a tick with more than one or none (then within one of its steps)
    most = PREDICT_CORR_MOST if off == 0 else LIFT_STEP + 0.1
    s.check("B's correction where he met the lift, and the lift's predicted place, under %.2f units (the buffer's ticks "
            "off one a tick: %d)" % (most, off),
            off >= 0 and float(pr.get("contact_max", 99)) < most and float(pr.get("lift_max", 99)) < most,
            "contact %s over %s, the lift's place %s" % (pr.get("contact_max"), pr.get("contact_n"), pr.get("lift_max")))
    gap, own, sv = rest_gap(s, srv, b, eb, "on the lift")
    s.check("on the lift at rest B stands where the server has him (within 1 unit)", gap <= 1.0,
            "B draws himself at %s, the server has him at %s" % (own, sv))
    b.cmd("notarget; god")


def predict_crawl(s, srv, a, b, eb):
    """MP_PLAN B8g: the lying box's crawl predicted. On e1m1's open floor B lies down (his head low: the lying box)
    and crawls with the stick, forward, aside and back (its share of the crawl run in each move: VR_AfterCrawlMove's
    crawl speed, glide and cap, the lying box's rules; the hands' pulls stay the server's): his corrections stay under a
    unit, and he got somewhere."""
    put_at(srv, b, eb, OPEN_SPOT, 90)
    b.quiet(r"mp_predict_corr: ", 1.0, 20, "B settled at his spot")
    b.cmd("vr_mock_hand head 0 %.2f 0" % PRONE_HEAD)
    predict_until(s, b, "lying", 1, "B lying down (the lying box in his state)")
    predict_until(s, b, "states", int(predict_report(b).get("states", 0)) + 36, "B lay down")
    start = org_of(status(srv), "mp_client", "slot", eb - 1)
    # (aside to the west, away from A at 528 -296: other players are not in his replay -- crawling back into A, the
    # server stopped him and his replay did not, 0.1-0.2 units a state)
    pr = predict_segment(s, b, "crawling", "", 72, ("vr_mock_stick off 0 1", "vr_mock_stick off -1 0",
                                                    "vr_mock_stick off 0 -1", "vr_mock_stick off 0 0", ""))
    s.metric("b_predict_crawl", pr)
    sv = org_of(status(srv), "mp_client", "slot", eb - 1)
    s.check("B crawled lying down (states lying walked move by move: 250 at least, his arms' crawl on; the server "
            "moved him 10 units at least)", int(pr.get("crawl_n", 0)) >= 250 and int(pr.get("unwalked", 1)) == 0 and
            int(pr.get("arms", 0)) == 1 and bool(start) and bool(sv) and dist(start, sv) >= 10.0,
            "%s lying states, %s unwalked, arms %s, from %s to %s" % (pr.get("crawl_n"), pr.get("unwalked"),
                                                                       pr.get("arms"), start, sv))
    predict_corr_check(s, "crawling", pr)
    own = org_of(status(b), "mp_player", "self", 1)
    gap = dist(own, sv) if own and sv else 99.0
    s.check("lying at rest B is where the server has him (within 1 unit)", gap <= 1.0,
            "B draws himself at %s, the server has him at %s" % (own, sv))
    b.cmd("vr_mock_hand head")
    predict_until(s, b, "states", int(predict_report(b).get("states", 0)) + 72, "B got up")


def predict_swim(s, srv, a, b, eb):
    """MP_PLAN B8e: swimming predicted. On e1m4, B in its deep water swims with the stick (SV_WaterMove, the swim's
    stick scale, glide and cap, QC WaterMove's drag, the jump key's rise, the drift down: each move's water level his
    replay finds as the server does): his corrections stay under a unit."""
    since_a, since_b = a.mark(), b.mark()
    srv.cmd("changelevel e1m4")
    a.wait(r"mp_event: .*joined map=e1m4", 120, since_a, "A on e1m4")
    b.wait(r"mp_event: .*joined map=e1m4", 120, since_b, "B on e1m4")
    b.cmd("notarget; god")  # (his breath runs out under water: no drowning)
    put_at(srv, b, eb, SWIM_SPOT, 90)
    # (B17: his own place there, not a big correction's line: in a suite run his prediction's first state on e1m4 came
    # after the put, taken there without a correction, and the wait timed out)
    own = None
    for _ in range(100):
        own = org_of(status(b), "mp_player", "self", 1)
        if own and math.hypot(own[0] - SWIM_SPOT[0], own[1] - SWIM_SPOT[1]) <= 4:
            break
    if not own or math.hypot(own[0] - SWIM_SPOT[0], own[1] - SWIM_SPOT[1]) > 4:
        raise StepFailed("B's own place never came to the water's spot: %s" % (own,))
    b.quiet(r"mp_predict_corr: ", 1.0, 20, "B settled in the water")
    pr = predict_segment(s, b, "swimming", "", 36,
                         ("vr_mock_stick off 0 0.6", "vr_mock_stick off 0.6 0", "+jump",
                          "-jump; vr_mock_stick off 0 -0.6", "vr_mock_stick off 0 0", ""))
    s.metric("b_predict_swim", pr)
    s.check("B swam (states in deep water: 150 at least)", int(pr.get("swim_n", 0)) >= 150, "%s" % pr.get("swim_n"))
    predict_corr_check(s, "swimming", pr)
    b.cmd("notarget; god")


def predict_waterjump(s, srv, a, b, eb, first=True):
    """MP_PLAN B8f: Quake's jump out of the water predicted (QC CheckWaterJump's WaterJumpTry run in each move: solid
    at the waist ahead, open at the eyes; then SV_WaterJump's carry over the edge). On the start map's pool (its low
    edge: flatmove_test.sh's), B swims at the edge with the server on Quake's jump out (vr_water_jump 1; 0 is the
    hands' climb out, the server's): he is lifted out onto dry ground, and his corrections stay under a unit."""
    if first:
        since_a, since_b = a.mark(), b.mark()
        srv.cmd("changelevel start; vr_water_jump 1")
        a.wait(r"mp_event: .*joined map=start", 120, since_a, "A on start")
        b.wait(r"mp_event: .*joined map=start", 120, since_b, "B on start")
    b.cmd("notarget; god")
    put_at(srv, b, eb, WATERJUMP_SPOT, WATERJUMP_YAW)
    # (he sinks: swimming at once, the jump key held to keep him up, from his prediction's snap to the spot on -- the teleport's correction left out)
    own = None
    for _ in range(100):
        own = org_of(status(b), "mp_player", "self", 1)
        if own and math.hypot(own[0] - WATERJUMP_SPOT[0], own[1] - WATERJUMP_SPOT[1]) <= 4:
            break
    else:
        raise StepFailed("B's prediction never put in the pool (he draws himself at %s)" % own)
    pr = predict_segment(s, b, "jumping out of the water", "+jump; vr_mock_stick off 0 1", 36,
                         ("", "", "-jump; vr_mock_stick off 0 0", ""))
    s.metric("b_predict_waterjump", pr)
    sv = org_of(status(srv), "mp_client", "slot", eb - 1)
    s.check("B jumped out of the water (states in the water jump: 5 at least; the server has him on the edge's top)",
            int(pr.get("wj_n", 0)) >= 5 and bool(sv) and sv[2] >= WATERJUMP_SPOT[2] + 24.0,
            "%s states, the server has him at %s" % (pr.get("wj_n"), sv))
    predict_corr_check(s, "jumping out of the water", pr)
    own = org_of(status(b), "mp_player", "self", 1)
    gap = dist(own, sv) if own and sv else 99.0
    s.check("out of the water at rest B stands where the server has him (within 1 unit)", gap <= 1.0,
            "B draws himself at %s, the server has him at %s" % (own, sv))


def predict_mantle(s, srv, a, b, ea, eb):
    """MP_PLAN B8h: the mantle predicted. On vrclimb B takes the ledge with both hands, pulls himself down and pushes
    over the top (climb_plays.py's overtop, mp_climb's): the server's mantle runs in his moves (its way and time in the
    state: pmove::mantleStep, the code single player's mantle frame steps with), his replay carries him along it at each
    move's time: his corrections stay under a unit, and on the top at rest he stands where the server has him. B8j: the
    mantle's start too (its test in each move, pmove::hangMantle: the server's and his replay's), under a unit."""
    b.cmd("vr_mock_play; -graboff; -grabmain; vr_mock_hand head")
    start = put_at(srv, b, eb, CLIMB_SPOT, 0)
    b.quiet(r"mp_predict_corr: ", 1.0, 20, "B settled at the wall")
    since = srv.mark()
    b.cmd("vr_mp_predict_report reset")
    play(s, b, "overtop", overtop_play())
    pr = predict_segment(s, b, "mantling onto the ledge", "", 72, ("", "", "", "", ""), reset=False)
    s.metric("b_predict_mantle", pr)
    mantled = wait_quietly(srv, r"climb: .*mantle", 1, since, "the server's mantle")
    sv = org_of(status(srv), "mp_client", "slot", eb - 1)
    s.check("B mantled onto the ledge (states mantling walked move by move: 5 at least; the server has him %d units up "
            "at least)" % MANTLE_RISE, int(pr.get("mantle_n", 0)) >= 5 and bool(sv) and sv[2] - start[2] >= MANTLE_RISE,
            "%s mantling states, the server has him at %s (from %s); %s" % (
                pr.get("mantle_n"), sv, start, mantled[1].strip() if mantled else "no mantle line"))
    predict_corr_check(s, "mantling", pr)
    trans_check(s, "the mantle's start (B8j: the test in each move, pmove::hangMantle, his replay's too) and the hold's take", pr)
    own = org_of(status(b), "mp_player", "self", 1)
    gap = dist(own, sv) if own and sv else 99.0
    s.check("on the top at rest B stands where the server has him (within 1 unit)", gap <= 1.0,
            "B draws himself at %s, the server has him at %s" % (own, sv))
    b.cmd("vr_mock_play; -graboff; -grabmain")


def trans_check(s, what, pr):
    """MP_PLAN B8j: a climb's transitions (a hold taken, the hang's mantle begun: `trans_n`, `trans_max`, counted apart
    from the other corrections) predicted: each such state's correction under a unit."""
    s.check("B's corrections at %s under 1.0 unit" % what,
            int(pr.get("trans_n", 0)) >= 1 and float(pr.get("trans_max", 99)) < 1.0,
            "%s such states, the longest %s units" % (pr.get("trans_n"), pr.get("trans_max")))


def predict_climb(s):
    """MP_PLAN B8h: climbing predicted, on vrclimb (one start: A moved off it before B joins), B 150 ms behind the
    server each way: the hang (predict_hang), the mantle (predict_mantle), a hold taken in the air (predict_airgrab,
    B8j part 2). `--part hang|mantle|airgrab`: one alone, `--repeat` times."""
    srv, a, b, ea, eb = join(s, lag=PREDICT_LAG, level="vrclimb",
                             first=lambda srv, a, ea: put_at(srv, a, ea, CLIMB_WATCH, CLIMB_WATCH_YAW))
    a.cmd("name A")
    b.cmd("name B")
    srv.cmd("vr_mp_watch %d; vr_climb_debug 1; vr_net_permove_skip %d" % (PART_WATCH, PERMOVE_SKIP))
    b.cmd("vr_mp_watch %d" % PART_WATCH)
    for k in range(PART_REPEAT if PART else 1):
        if PART:
            s.note("--part %s, take %d" % (PART, k + 1))
        if PART in ("", "hang"):
            predict_hang(s, srv, a, b, ea, eb)
        if PART in ("", "mantle"):
            predict_mantle(s, srv, a, b, ea, eb)
        if PART in ("", "airgrab"):
            predict_airgrab(s, srv, a, b, ea, eb)
        if PART in ("", "grapple"):
            predict_grapple(s, srv, a, b, eb)  # (on vrclimb: before waterclimb's start map)
        if PART in ("", "waterclimb"):
            predict_waterclimb(s, srv, a, b, eb, first=k == 0)


SLIDE_LOSS = 0.05  # mp_predictslide: B's game packets lost, each way (with PREDICT_LAG)


def predict_slide(s):
    """MP_PLAN B14: a headset's slide (vr_slide, experimental) predicted. B, 150 ms behind the server each way with 5%
    of his packets lost each way, runs north on e1m1's open floor (half a second, to his run's speed) and pulls the turning stick straight down (the move's
    slide bit): the slide runs in his moves on the server and in his replay (vr_slide.cpp's slide::move, one function
    for single player's SV_AirMove and the moves; the slide's state and the server's numbers in his state), so his
    states stay walked move by move and his corrections under a unit; a second take after the cooldown ends with a
    jump; at rest he stands where the server has him."""
    srv, a, b, ea, eb = join(s, lag=PREDICT_LAG, loss=SLIDE_LOSS)
    srv.cmd("vr_mp_watch %d" % PART_WATCH)
    b.cmd("cl_alwaysrun 1; vr_slide 1; vr_mp_watch %d" % PART_WATCH)  # (running: a walk is under the slide's least speed)
    takes = (("held to its end", ("", "vr_mock_stick main 0 -1", "", "vr_mock_stick main 0 0; vr_mock_stick off 0 0", "")),
             ("ended by a jump", ("", "vr_mock_stick main 0 -1", "+jump", "-jump; vr_mock_stick main 0 0; "
                                  "vr_mock_stick off 0 0", "")))
    for k, (what, legs) in enumerate(takes):
        put_at(srv, b, eb, OPEN_SPOT, 90)
        b.quiet(r"mp_predict_corr: ", 1.0, 20, "B settled at his spot")
        lost_from = moves_lost(srv, eb)
        pr = predict_segment(s, b, "sliding, %s" % what, "vr_mock_stick off 0 1", 36, legs)
        lost = moves_lost(srv, eb) - lost_from  # (5% lost each way: now and then a move's every copy)
        s.metric("b_predict_slide%d" % (k + 1), pr)
        s.check("B slid (%s): the server's slide in his states, every one walked move by move" % what,
                int(pr.get("slide_n", 0)) >= 10 and int(pr.get("unwalked", 1)) == 0 and
                int(pr.get("unpredicted", 1)) == 0,
                "%s states sliding, %s unwalked, %s unpredicted" % (pr.get("slide_n"), pr.get("unwalked"),
                                                                     pr.get("unpredicted")))
        predict_corr_check(s, "sliding (%s)" % what, pr, lost=lost)
        predict_exact_check(s, "(sliding, %s)" % what, pr, lost=lost)
        own = org_of(status(b), "mp_player", "self", 1)
        sv = org_of(status(srv), "mp_client", "slot", eb - 1)
        gap = dist(own, sv) if own and sv else 99.0
        s.check("at rest after the slide (%s) B stands where the server has him (within 1 unit)" % what, gap <= 1.0,
                "B draws himself at %s, the server has him at %s" % (own, sv))
def hang_pull_play():
    """Both hands onto vrclimb's ledge, pulled down 0.25 m (the body up: no mantle), held, let up again 0.15 m (the body
    down), held, let go at 4.5 s (he falls to the floor)."""
    lines = list(CLIMB_LEAN)  # (overtop_play's lean and hand turn: its hold is taken)
    for h, x in (("off", -0.12), ("main", 0.12)):
        at = "%s %.2f %.3f %.2f 70 0 0" % (h, x, HOLD_Y, HOLD_FWD)
        down = "%s %.2f %.3f %.2f 70 0 0" % (h, x, HOLD_Y - 0.25, HOLD_FWD)
        half = "%s %.2f %.3f %.2f 70 0 0" % (h, x, HOLD_Y - 0.10, HOLD_FWD)
        lines += ["0.000 %s %.2f 1.1 -0.2 70 0 0" % (h, x * 2), "0.600 " + at, "1.000 " + at, "2.000 " + down,
                  "2.600 " + down, "3.400 " + half, "6.000 " + half]
    return lines + ["0.700 cmd +graboff", "0.750 cmd +grabmain", "4.500 cmd -graboff", "4.500 cmd -grabmain"]


def predict_hang(s, srv, a, b, ea, eb):
    """MP_PLAN B8i: the hang predicted. On vrclimb B (150 ms behind each way) takes the ledge with both hands, pulls
    himself up, lets himself down a little, and lets go: each move's hands run the hang's pull and move on the server
    (vr_pmove.cpp's hangStep: the code single player's hanging frame runs) and in his replay (the holds, `owed` and the
    slipping in the state); the reach, the owed's settling and the stamina are the server's once a frame (the mantle's
    test runs in each move: B8j). His corrections stay under a unit (the hold's take too), and on the floor at rest he
    stands where the server has him."""
    b.cmd("vr_mock_play; -graboff; -grabmain; vr_mock_hand head")
    start = put_at(srv, b, eb, CLIMB_SPOT, 0)
    b.quiet(r"mp_predict_corr: ", 1.0, 20, "B settled at the wall")
    since = srv.mark()
    b.cmd("vr_mp_predict_report reset")
    play(s, b, "hangpull", hang_pull_play())
    pr = predict_segment(s, b, "hanging from the ledge", "", 72, ("", "", "", "", ""), reset=False)
    s.metric("b_predict_hang", pr)
    held = wait_quietly(srv, r"climb: .*(takes hold|holds)", 1, since, "the server's hold")
    mantled = wait_quietly(srv, r"climb: .*mantle", 1, since, "a mantle (none wanted)")
    s.check("B hung from the ledge (states hanging walked move by move: 60 at least; no mantle)",
            int(pr.get("hang_n", 0)) >= 60 and not mantled,
            "%s hanging states (rose %s units at most); %s; %s" % (pr.get("hang_n"), pr.get("hang_rise"),
                held[1].strip() if held else "no hold line", mantled[1].strip() if mantled else "no mantle"))
    s.check("B's body rose hanging, as his pull asked (8 units at least over where he took hold: the pull 0.25 m)",
            float(pr.get("hang_rise", 0)) >= 8.0, "%s" % pr.get("hang_rise"))
    predict_corr_check(s, "hanging", pr)
    trans_check(s, "the hold's take", pr)
    sv = org_of(status(srv), "mp_client", "slot", eb - 1)
    own = org_of(status(b), "mp_player", "self", 1)
    gap = dist(own, sv) if own and sv else 99.0
    s.check("let go, on the floor at rest B stands where the server has him (within 1 unit)",
            gap <= 1.0 and bool(sv) and abs(sv[2] - start[2]) <= 2.0,
            "B draws himself at %s, the server has him at %s (he started at %s)" % (own, sv, start))
    b.cmd("vr_mock_play; -graboff; -grabmain")


AIRGRAB_Y, AIRGRAB_FWD = HOLD_Y - 0.55, -0.52  # (under the lip standing, in front of the face: passed on the way up, the
# feet still vr_climb_min_height under the top)


def airgrab_play():
    """A jump at vrclimb's ledge, both hands held under its lip in front of the face, the grips pressed on the way up
    (no hold there yet: the press keeps looking in the air, vr_climb_air_grab_time) and taken as the hands pass the lip;
    held still, let go at 3 s (he falls to the floor)."""
    lines = list(CLIMB_LEAN)
    for h, x in (("off", -0.12), ("main", 0.12)):
        at = "%s %.2f %.3f %.2f 70 0 0" % (h, x, AIRGRAB_Y, AIRGRAB_FWD)
        lines += ["0.000 %s %.2f 1.1 -0.2 70 0 0" % (h, x * 2), "0.600 " + at, "4.000 " + at]
    return lines + ["1.000 cmd +jump", "1.020 cmd +graboff", "1.020 cmd +grabmain", "1.100 cmd -jump",
                    "3.000 cmd -graboff", "3.000 cmd -grabmain"]


def predict_airgrab(s, srv, a, b, ea, eb):
    """MP_PLAN B8j part 2: a hold taken in the air predicted. On vrclimb B (150 ms behind each way) jumps at the ledge,
    his grips pressed on the way up under the lip, and takes it as his hands pass it: the take runs in each move on the
    server (climb::moveTake: the hold search, the press's memory, the take's rules) and in his replay (climb::replayTake:
    the same search on the world's ledge map, built from his own BSP), so the take's state is no correction (`trans_max`
    under a unit, where it was the fall since, a round trip's); then he hangs, lets go and falls."""
    b.cmd("vr_mock_play; -graboff; -grabmain; vr_mock_hand head")
    start = put_at(srv, b, eb, CLIMB_SPOT, 0)
    b.quiet(r"mp_predict_corr: ", 1.0, 20, "B settled at the wall")
    since, since_b = srv.mark(), b.mark()
    b.cmd("vr_mp_predict_report reset")
    play(s, b, "airgrab", airgrab_play())
    pr = predict_segment(s, b, "taking the ledge in the air", "", 72, ("", "", "", "", ""), reset=False)
    s.metric("b_predict_airgrab", pr)
    held = wait_quietly(srv, r"climb: .* hand holds at .* from \(", 1, since, "the server's hold")
    m = re.search(r"from \(([-\d.]+) ([-\d.]+) ([-\d.]+)\)", held[1]) if held else None
    took_z = float(m.group(3)) if m else None
    s.check("B took the ledge in the air (the server's hold taken with his body off the floor: 4 units up at least)",
            took_z is not None and took_z >= CLIMB_SPOT[2] + 4.0,
            "%s (he stood at %s)" % (held[1].strip() if held else "no hold line", start))
    replayed = wait_quietly(b, r"mp_predict_take: replay", 1, since_b, "B's replay's take")
    s.check("B's replay took it too (climb::replayTake on his own ledge map)", bool(replayed),
            replayed[1].strip() if replayed else "no take in his replay")
    predict_corr_check(s, "taking the ledge in the air", pr)
    trans_check(s, "the hold's take in the air", pr)
    sv = org_of(status(srv), "mp_client", "slot", eb - 1)
    own = org_of(status(b), "mp_player", "self", 1)
    gap = dist(own, sv) if own and sv else 99.0
    s.check("let go, on the floor at rest B stands where the server has him (within 1 unit)",
            gap <= 1.0 and bool(sv) and abs(sv[2] - CLIMB_SPOT[2]) <= 2.0,
            "B draws himself at %s, the server has him at %s (he started at %s)" % (own, sv, start))
    b.cmd("vr_mock_play; -graboff; -grabmain")


def waterclimb_play():
    """At the start map's pool, swimming west to its edge (the stick forward, the jump key held at the surface), the
    main hand raised over the edge's top 0.4 m ahead and gripping at 2.5 s, pulled down 0.9 m in a second (0.9 m/s:
    the climb wants 0.3) -- lifted out as the hand passes over the top; let go, the stick let go at 3.6 s."""
    hand = "main 0.15 %.2f -0.40 70 0 0"
    return ["0.000 head 0 1.646 0", "0.000 off -0.30 1.1 -0.2 70 0 0", "0.000 " + hand % 1.5, "2.500 " + hand % 1.5,
            "3.500 " + hand % 0.6, "6.000 " + hand % 0.6, "0.000 cmd +jump", "0.000 cmd vr_mock_stick off 0 1",
            "2.500 cmd +grabmain", "3.600 cmd -grabmain", "3.600 cmd -jump", "3.600 cmd vr_mock_stick off 0 0"]


def predict_waterclimb(s, srv, a, b, eb, first=True):
    """MP_PLAN B8j part 3: the hands' climb out of the water predicted (VR's default, vr_water_jump 0; climbing on).
    On the start map's pool B swims to its west edge (no ledge of the ledge map there: a bank the climb's hold does not
    take), grips over the top and pulls his hand down: the climb (pmove::waterClimbTry, the function single player's
    frame calls through QC's builtin) runs in each move on the server and in his replay (the empty hands in the
    state), so the lift out is no correction (it was the water jump's whole rise, a round trip late); out on the top
    at rest he stands where the server has him."""
    if first:
        since_a, since_b = a.mark(), b.mark()
        srv.cmd("changelevel start; vr_water_jump 0; developer 1")
        a.wait(r"mp_event: .*joined map=start", 120, since_a, "A on start")
        b.wait(r"mp_event: .*joined map=start", 120, since_b, "B on start")
    b.cmd("notarget; god; vr_mock_play; -graboff; -grabmain; vr_mock_hand head")
    put_at(srv, b, eb, WATERJUMP_SPOT, WATERJUMP_YAW)
    own = None
    for _ in range(100):
        own = org_of(status(b), "mp_player", "self", 1)
        if own and math.hypot(own[0] - WATERJUMP_SPOT[0], own[1] - WATERJUMP_SPOT[1]) <= 4:
            break
    else:
        raise StepFailed("B's prediction never put in the pool (he draws himself at %s)" % own)
    since, since_b = srv.mark(), b.mark()
    b.cmd("vr_mp_predict_report reset")
    play(s, b, "waterclimb", waterclimb_play())
    pr = predict_segment(s, b, "climbing out of the water by hand", "", 72, ("", "", "", "", "", "", ""), reset=False)
    s.metric("b_predict_waterclimb", pr)
    climbed = wait_quietly(srv, r"waterjump: climbed out by hand", 1, since, "the server's climb out")
    sv = org_of(status(srv), "mp_client", "slot", eb - 1)
    s.check("B climbed out of the water by hand (the server's climb; his moves' rules carry it: wclimb on; the server "
            "has him on the edge's top)", bool(climbed) and int(pr.get("wclimb", 0)) >= 1 and bool(sv) and
            sv[2] >= WATERJUMP_SPOT[2] + 24.0,
            "%s; wclimb %s; the server has him at %s" % (climbed[1].strip() if climbed else "no climb line",
                                                       pr.get("wclimb"), sv))
    replayed = wait_quietly(b, r"mp_predict_wclimb: replay", 1, since_b, "B's replay's climb out")
    s.check("B's replay climbed him out too (pmove::waterClimbTry in his moves)",
            bool(replayed) and int(pr.get("wclimb_n", 0)) >= 1,
            "%s (%s replays)" % (replayed[1].strip() if replayed else "no climb in his replay", pr.get("wclimb_n")))
    predict_corr_check(s, "climbing out of the water by hand", pr)
    own = org_of(status(b), "mp_player", "self", 1)
    gap = dist(own, sv) if own and sv else 99.0
    s.check("out of the water at rest B stands where the server has him (within 1 unit)", gap <= 1.0,
            "B draws himself at %s, the server has him at %s" % (own, sv))
    b.cmd("vr_mock_play; -graboff; -grabmain; -jump; vr_mock_stick off 0 0")


GRAPPLE_AT = (CLIMB_SPOT[0] - 8.0, CLIMB_SPOT[1], 500.0)  # (vrclimb's ceiling, 512, over the climb spot: a hook in the
# world, GH_ANCHOR; the rope 476 units)
GRAPPLE_RISE = 48.0  # units the reel lifts him at least (a second of it)


def predict_grapple(s, srv, a, b, eb):
    """MP_PLAN B8j part 4: the grapple's pull predicted. On vrclimb B (150 ms behind each way) takes a grappling gun in
    his main hand, fires it (the server steers the hook into the ceiling over him: vr_grapple_test_aim), reels himself
    up with B (vr_mock_button main secondary) for a second, hangs on the rope, lets the trigger go and falls: the rope's
    hold and reel (pmove::grappleStep, the function single player's frame calls through QC's builtin grapplehold) run
    in each move on the server and in his replay (the rope's corner, length, reel speed and ramp in the state, each
    move's B/Y), so the pull and the hang are no correction (they were the rope's whole pull, a round trip late); on the
    floor at rest he stands where the server has him. (On vrclimb as joined: a changelevel would spawn A and B on its
    one start.)"""
    srv.cmd("vr_grapple_debug 1; developer 1")
    b.cmd("notarget; god; vr_mock_play; -graboff; -grabmain; -attack; vr_mock_button main secondary 0; vr_mock_hand head")
    start = put_at(srv, b, eb, CLIMB_SPOT, 180)
    b.quiet(r"mp_predict_corr: ", 1.0, 20, "B settled")
    since, since_b = srv.mark(), b.mark()
    b.cmd("vr_mock_hand main 0.15 1.3 -0.3 150 0 0")  # (the gun pointing up, 80 degrees: the server steers the hook)
    grip_then_give(srv, b, "vr_mock_button main grip 1", impulse=150 + 1)  # (WID_GRAPPLE 1)
    srv.cmd("vr_debug_carry 0")
    since = srv.mark()
    for _ in range(20):
        b.cmd("+attack")
        fired = wait_quietly(srv, r"grapple: hand 1 fires hook", 0.5, since, "the server's hook fired")
        if fired:
            break
        b.cmd("-attack")
    for _ in range(4):
        srv.cmd("vr_grapple_test_aim %.0f %.0f %.0f" % GRAPPLE_AT)
    bit = wait_quietly(srv, r"grapple: hook \S+ \(hand 1\) bit ", 5, since, "the hook's bite")
    s.check("B's hook bit the ceiling over him (the server's: fired from his main hand, a hook in the world)",
            bool(fired) and bool(bit), "%s; %s" % (fired[1].strip() if fired else "not fired",
                                                 bit[1].strip() if bit else "no bite"))
    if not bit:
        b.cmd("-attack; vr_mock_play")
        return
    b.quiet(r"mp_predict_corr: ", 1.0, 20, "B settled on the hook")
    pr = predict_segment(s, b, "reeled up by the grapple", "vr_mock_button main secondary 1", 36)
    sv_up = org_of(status(srv), "mp_client", "slot", eb - 1)
    pr = predict_segment(s, b, "hanging on the rope, let go", "", 72,
                         ("vr_mock_button main secondary 0", "-attack; impulse 45", ""), reset=False)  # (45: the quick release)
    s.metric("b_predict_grapple", pr)
    reeled = wait_quietly(srv, r"grapple: hand 1 reel off after .* rope ", 1, since, "the server's reel")
    s.check("B was reeled up (the server's rope in his moves: grapple_n 30 at least; the server had him %d units up at "
            "least)" % GRAPPLE_RISE, int(pr.get("grapple_n", 0)) >= 30 and bool(sv_up) and
            sv_up[2] - start[2] >= GRAPPLE_RISE,
            "%s states with the rope; the server had him at %s (from %s); %s" % (
                pr.get("grapple_n"), sv_up, start, reeled[1].strip() if reeled else "no reel line"))
    predict_corr_check(s, "reeled up and hanging on the grapple's rope", pr)
    s.check("B's corrections with the rope in his moves under 1.0 unit (grapple_max)",
            int(pr.get("grapple_n", 0)) >= 30 and float(pr.get("grapple_max", 99)) < 1.0,
            "%s such states, the longest %s units" % (pr.get("grapple_n"), pr.get("grapple_max")))
    s.note("the hook's coming off (the server's quick release: not predicted, eased in apart): %s such states, the "
           "longest %s units" % (pr.get("grapple_off_n"), pr.get("grapple_off_max")))
    sv = org_of(status(srv), "mp_client", "slot", eb - 1)
    own = org_of(status(b), "mp_player", "self", 1)
    gap = dist(own, sv) if own and sv else 99.0
    # (the swing can drop him on the long ledge's top, 48 units up, 2 units from the spot: either floor)
    s.check("let go, at rest on the floor (or the ledge's top) B stands where the server has him (within 1 unit)",
            gap <= 1.0 and bool(sv) and min(abs(sv[2] - start[2]), abs(sv[2] - start[2] - 48.0)) <= 2.0,
            "B draws himself at %s, the server has him at %s (he started at %s)" % (own, sv, start))
    b.cmd("vr_mock_play; -attack; vr_mock_button main secondary 0; vr_mock_button main grip 0")


def predict(s):
    """MP_PLAN B8: B, 150 ms behind the server each way, predicts his own walking. His moves run on the server one by
    one for their own frame's time (vr_pmove.cpp, the code his client runs too); the server's state after the newest
    move it ran comes back with that move's number, and his client replays the moves after it from there. His own
    move shows at the frame of his press (without prediction: a round trip, about 22 frames); walking there and back on
    e1m1's flat floor, each new state moves his prediction by under a unit; at rest he stands where the server has him."""
    srv, a, b, ea, eb = join(s, lag=PREDICT_LAG, jitter=PART_JITTER if PART else 0.0, loss=PART_LOSS if PART else 0.0)
    if PART:
        # (--part: one of its segments alone, `--repeat` times: a flake's cause traced, vr_mp_watch 2)
        srv.cmd("vr_mp_watch %d" % PART_WATCH)
        b.cmd("vr_mp_watch %d" % PART_WATCH)
        for k in range(PART_REPEAT):
            s.note("--part %s, take %d" % (PART, k + 1))
            if PART == "lift":
                predict_lift(s, srv, a, b, ea, eb)  # (once a run: the lift is at its top after it)
                break
            elif PART == "waterjump":
                predict_waterjump(s, srv, a, b, eb, first=k == 0)
            elif PART == "swim":
                predict_swim(s, srv, a, b, eb)
            elif PART == "crawl":
                predict_crawl(s, srv, a, b, eb)
        return
    put_at(srv, b, eb, OPEN_SPOT, 90)
    b.quiet(r"mp_predict_corr: ", 1.0, 20, "B settled at his spot")
    b.cmd("vr_mp_predict_report reset")  # (the counts from here: put_at's setpos is a teleport)
    b.cmd("vr_mp_watch 1")
    frames, ms = own_move_frames(s, b, "predicted", "vr_mock_stick off 0 1", "vr_mock_stick off 0 0")
    s.metric("own_move_frames", frames)
    s.metric("own_move_ms", ms)
    s.check("B's own move shows %d move(s) after his press at most (%d ms each way)" % (PREDICT_MOVES_MOST, PREDICT_LAG),
            0 <= frames <= PREDICT_MOVES_MOST, "%d moves, %d ms" % (frames, ms))
    pr0 = predict_report(b)
    s.check("B predicts (the server's states read, his moves replayed)", bool(pr0) and int(pr0.get("states", 0)) > 0 and
            int(pr0.get("on", 0)) == 1, "%s" % pr0)
    states = int(pr0.get("states", 0))
    for leg in range(PREDICT_LEGS):
        b.cmd("vr_mock_stick off 0 %s" % ("0.5" if leg % 2 == 0 else "-0.5"))
        states += PREDICT_LEG_STATES
        predict_until(s, b, "states", states, "B's leg %d" % leg)
    since = b.mark()
    b.cmd("vr_mock_stick off 0 0")
    b.wait(r"mp_stop: .*self=1", 20, since, "B at rest after his walk")
    pr = predict_until(s, b, "states", states + 36, "B at rest")
    s.metric("b_predict", pr)
    s.note("B's prediction: %s" % " ".join("%s=%s" % kv for kv in sorted(pr.items())))
    s.check("B replays the moves the server has not run yet (about a round trip of them)",
            float(pr.get("depth_mean", 0)) >= 10, "mean %s moves replayed a frame" % pr.get("depth_mean"))
    s.check("B's corrections on flat ground under %.1f unit" % PREDICT_CORR_MOST,
            int(pr.get("flat_n", 0)) >= 50 and float(pr.get("flat_max", 99)) < PREDICT_CORR_MOST,
            "max %s over %s states on flat ground (all states: max %s, mean %s)" % (
                pr.get("flat_max"), pr.get("flat_n"), pr.get("corr_max"), pr.get("corr_mean")))
    gap, own, sv = rest_gap(s, srv, b, eb, "after his walk")
    predict_exact_check(s, "(walking on flat ground)", pr, least=50)
    s.check("at rest B stands where the server has him (within 1 unit)", gap <= 1.0,
            "B draws himself at %s, the server has him at %s" % (own, sv))
    # Without prediction (his client draws the server's state): a round trip before his own move shows.
    b.cmd("vr_net_predict 0")
    b.quiet(r"mp_(move|pos): .*self=1", 0.5, 20, "B at rest, drawn as the server has him")
    frames0, ms0 = own_move_frames(s, b, "unpredicted", "vr_mock_stick off 0 -1", "vr_mock_stick off 0 0")
    b.cmd("vr_net_predict 1")
    s.metric("own_move_frames_unpredicted", frames0)
    s.check("without prediction his own move takes the round trip (the test tells them apart)",
            frames0 > PREDICT_MOVES_MOST + 5, "%d moves, %d ms" % (frames0, ms0))
    b.quiet(r"mp_predict_corr: ", 1.0, 20, "B predicted again")
    predict_moves(s, srv, a, b, ea, eb)
    predict_crawl(s, srv, a, b, eb)
    predict_swim(s, srv, a, b, eb)
    predict_waterjump(s, srv, a, b, eb)



LAGCOMP_LAG = 150          # mp_lagcomp: B's added delay each way, ms
LAGCOMP_GAP = 160.0        # units north of B that A strafes at, facing him
LAGCOMP_LEGS = 4           # A's strafes, alternately right and left ...
LAGCOMP_LEG_SHOTS = 4      # ... B's test shots at him in each (the first of a leg, A still speeding up, not counted)
LAGCOMP_SHOT_MOST = 800.0   # ms: and his test shot (a reliable command, after the move that would carry a real shot) at most
LAGCOMP_HIT_LEAST = 0.8    # of the counted shots, judged where B saw him, this many hit him at least
LAGCOMP_BACK = (250.0, 500.0)  # ms: how far back the others were put for B's shots (his round trip and his buffer's delay)
LAGCOMP_HOLDS = (3, 6, 3, 6)  # (B13c) the held legs: A's updates to B wait this many snapshots in each one more (the
                           # server's vr_net_test_hold: as B's rate's waits leave them; 6 past his buffer's rest gap)
                           # (B25: four, not two: whether a shot finds the frames' rewind off depends on where in the
                           # hold's cycle B's view was, a matter of wall-clock timing; the check takes the worst of 12)
LAGCOMP_ERR_MEAN = 2.0     # units: A put back at most this far from where B drew him, on average ...
LAGCOMP_ERR_MOST = 6.0     # ... and at most (the shot's command a frame after his draw; coordinates' rounding)
LAGCOMP_CAP_SHOWS = 3.0    # units: under the cap the frames' rewind (before B13c) is off by this much more on average


def cmd_counts(srv):
    """vr_mp_status's "mp_cmds:" lines on the server -> {player entity: {used, dropped, least}}."""
    out = {}
    for l in status(srv):
        if l.startswith("mp_cmds:"):
            f = {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", l)}
            out[f.pop("slot") + 1] = f
    return out


CMD_FLOOD = 150  # mp_cmdguard: B's string commands in one frame (his budget: 64 at once, 32 a second)


def cmdguard(s):
    """(Review 2026-11-18 finding 9) Client string commands. The harness's test shot and punch (vr_lagcomp_shot,
    vr_lagcomp_punch) are refused by a server without vr_mp_watch (every server but the harness's) and taken with it. A
    client's commands have a budget (VR_ClientCommandBudget): both players' own state (their fists' vr_rig chunks at
    spawn, palm frames, preferences, avatar calibration, held poses) stays far under it; B's flood of CMD_FLOOD commands
    in one frame is cut to it, the rest dropped and the server's console told; a second later B's commands run again."""
    srv, a, b, ea, eb = join(s)
    srv.cmd("developer 1")
    server_time_past(srv, server_time(srv) + 2.0)  # (their spawns' commands all in)
    honest = cmd_counts(srv)
    s.note("the server's command counts after both joined: %s" % honest)
    s.check("both players' own commands (spawn's state, preferences, calibration) under their budgets: none dropped, "
            "half the burst or more left", all(honest.get(e, {}).get("used", 0) > 0 and honest[e].get("dropped") == 0 and
                                                 honest[e].get("least", 0) >= 32 for e in (ea, eb)), "%s" % honest)

    srv.cmd("vr_mp_watch 0")
    since = srv.mark()
    b.cmd("vr_mp_shoot %d; vr_mp_punch %d" % (ea, ea))
    _, l = srv.wait(r"vr_lagcomp_punch from .*refused", 10, since, "the server refusing B's test punch")
    shots = [x for x in srv.lines[since:] if x.startswith("mp_lagcomp_")]
    refused = [x.strip() for x in srv.lines[since:] if "refused (a test's" in x]
    s.check("without vr_mp_watch the server refuses the test shot and punch (no trace, no report)",
            len(refused) == 2 and not shots, "refused: %s; reports: %s" % (refused, shots))
    srv.cmd("vr_mp_watch 1")
    since = srv.mark()
    b.cmd("vr_mp_shoot %d" % ea)
    srv.wait(r"mp_lagcomp_shot: slot=%d " % (eb - 1), 10, since, "the harness's server taking B's test shot")
    s.check("with vr_mp_watch (the harness's servers) it takes them", True, "")

    before = cmd_counts(srv).get(eb, {})
    since = srv.mark()
    b.cmd("; ".join(["cmd vr_magpoll 0"] * CMD_FLOOD))
    _, note = srv.wait(r"commands dropped", 10, since, "the server dropping B's flood")
    server_time_past(srv, server_time(srv) + 0.5)
    after = cmd_counts(srv).get(eb, {})
    ran = after.get("used", 0) - before.get("used", 0)
    dropped = after.get("dropped", 0) - before.get("dropped", 0)
    s.note("B's flood: %s; counts before %s, after %s" % (note.strip(), before, after))
    s.check("B's flood of %d commands in one frame cut to his budget (at most 64 and a few run, the rest dropped)" %
            CMD_FLOOD, ran <= 70 and dropped >= CMD_FLOOD - 70 and ran + dropped >= CMD_FLOOD - 2,
            "ran %d, dropped %d" % (ran, dropped))
    server_time_past(srv, server_time(srv) + 1.5)
    since = srv.mark()
    b.cmd("vr_mp_shoot %d" % ea)
    srv.wait(r"mp_lagcomp_shot: slot=%d " % (eb - 1), 10, since, "B's commands taken again after his flood")
    a_after = cmd_counts(srv).get(ea, {})
    s.check("B's commands run again a moment later; A's never dropped", a_after.get("dropped") == 0, "A: %s" % a_after)


def lagcomp(s):
    """MP_PLAN B8b: lag compensation. B, 150 ms behind the server each way, shoots at A strafing across his view (a test
    shot, vr_mp_shoot: from his eyes at A's middle as B draws him, traced on the server as shots are, MOVE_HITMODEL). The
    server judges each against A where B's screen showed him (his move's time less his interpolation buffer's delay:
    vr_lagcomp.cpp) and against A where it has him now: the first hits nearly always, the second mostly misses (A has
    moved on by his speed times B's lag), which tells the two apart."""
    srv, a, b, ea, eb = join(s, lag=LAGCOMP_LAG)
    a.cmd("name A")
    b.cmd("name B")
    put_at(srv, b, eb, MELEE_SPOT, 90)
    put_at(srv, a, ea, (MELEE_SPOT[0], MELEE_SPOT[1] + LAGCOMP_GAP, MELEE_SPOT[2]), 270)
    counted = rewound = plain = 0
    punches = p_rewound = p_plain = 0
    backs, moves = [], []
    errs, cap_errs, cap_frame_errs = [], [], []
    for leg in range(LAGCOMP_LEGS + len(LAGCOMP_HOLDS)):
        capped = leg >= LAGCOMP_LEGS
        if capped:
            srv.cmd("vr_net_test_hold %d" % LAGCOMP_HOLDS[leg - LAGCOMP_LEGS])  # (B13c: the rewind at the places B was sent)
        a.cmd("vr_mock_stick off %d 0" % (1 if leg % 2 == 0 else -1))
        for shot in range(LAGCOMP_LEG_SHOTS):
            since = srv.mark()
            b.cmd("vr_mp_shoot %d; vr_mp_punch %d" % (ea, ea))  # (together: A's legs as long as before B8c's punch)
            _, line = srv.wait(r"mp_lagcomp_shot: slot=%d " % (eb - 1), 20, since, "B's shot %d.%d" % (leg, shot))
            _, pline = srv.wait(r"mp_lagcomp_punch: slot=%d " % (eb - 1), 20, since, "B's punch %d.%d" % (leg, shot))
            f = fields(line)
            if shot == 0:
                continue
            if capped:
                cap_errs.append(float(f["err"]))
                cap_frame_errs.append(float(f["err_frame"]))
                continue
            errs.append(float(f["err"]))
            counted += 1
            rewound += 1 if int(f["rewound"]) == ea else 0
            plain += 1 if int(f["plain"]) == ea else 0
            backs.append(float(f["back_ms"]))
            moves.append(float(f["move_ms"]))
            # (B8c: a test punch the same way -- a fist's line into A's middle as B draws him, judged as melee's sweep
            # is, hitmodel_segment, in the same scope)
            f = fields(pline)
            punches += 1
            p_rewound += 1 if int(f["rewound"]) == ea else 0
            p_plain += 1 if int(f["plain"]) == ea else 0
    a.cmd("vr_mock_stick off 0 0")
    srv.cmd("vr_net_test_hold 0")
    sn = client_snap_status(b)  # (B13c: his buffer's delay, the others drawn behind: vr_net_sendhz's cost)
    s.metric("lagcomp_interp_delay_ms", float(sn.get("delay_ms", -1)))
    s.note("B's interpolation buffer: delay %s ms, %s holds" % (sn.get("delay_ms"), sn.get("holds")))
    mean = lambda xs: sum(xs) / len(xs) if xs else 99.0
    s.metric("lagcomp_err", {"free": errs, "capped": cap_errs, "capped_frames": cap_frame_errs})
    s.check("A put back where B drew him (%.1f units on average, %.0f at most)" % (LAGCOMP_ERR_MEAN, LAGCOMP_ERR_MOST),
            bool(errs) and mean(errs) <= LAGCOMP_ERR_MEAN and max(errs) <= LAGCOMP_ERR_MOST,
            "mean %.2f, most %.2f: %s" % (mean(errs), max(errs or [99]), errs))
    s.check("A's updates held %s snapshots (as under B's rate cap) too: at the places B was sent" % (LAGCOMP_HOLDS,),
            bool(cap_errs) and mean(cap_errs) <= LAGCOMP_ERR_MEAN and max(cap_errs) <= LAGCOMP_ERR_MOST,
            "mean %.2f, most %.2f: %s" % (mean(cap_errs), max(cap_errs or [99]), cap_errs))
    # (B25: by the worst shot, not the mean: a held shot's frames' rewind is off only when B's view fell between two of
    # A's held updates, not on one -- the hold's phase against his shot's, wall-clock timing: the mean of 6 was 1.09 and
    # 2.69 in two suite runs under load (their worst 4.94 and 12.75), 4-8 alone)
    s.check("the server's frames at his view time (the rewind before B13c) are off by more there (the worst held shot)",
            bool(cap_frame_errs) and max(cap_frame_errs) >= max(cap_errs or [99]) + LAGCOMP_CAP_SHOWS,
            "frames' worst %.2f vs %.2f (means %.2f, %.2f): %s" % (max(cap_frame_errs or [0]), max(cap_errs or [99]),
                                                                  mean(cap_frame_errs), mean(cap_errs), cap_frame_errs))
    s.metric("lagcomp_punch", {"counted": punches, "rewound": p_rewound, "plain": p_plain})
    s.note("B's punches at A: %d counted, %d landed where B saw him, %d where the server has him now" % (
        punches, p_rewound, p_plain))
    s.check("B's punches at A strafing land where B saw him (melee lag compensated): %d of %d" % (p_rewound, punches),
            punches > 0 and p_rewound >= LAGCOMP_HIT_LEAST * punches, "%d of %d" % (p_rewound, punches))
    s.check("without it most punches miss: %d of %d" % (p_plain, punches), p_plain <= punches // 2,
            "plain %d, rewound %d" % (p_plain, p_rewound))
    s.metric("lagcomp", {"counted": counted, "rewound": rewound, "plain": plain, "backs": backs, "moves": moves})
    s.note("B's shots at A: %d counted, %d hit where B saw him, %d where the server has him now; back %s ms, his "
           "moves' %s ms" % (counted, rewound, plain, backs, moves))
    s.check("B's shots at A strafing hit where B saw him (lag compensated): %d of %d" % (rewound, counted),
            counted > 0 and rewound >= LAGCOMP_HIT_LEAST * counted, "%d of %d" % (rewound, counted))
    s.check("without it most miss (the test tells them apart): %d of %d" % (plain, counted),
            plain <= counted // 2 and rewound - plain >= counted // 3, "plain %d, rewound %d" % (plain, rewound))
    s.check("the others put back by B's round trip and his buffer's delay (%.0f..%.0f ms)" % LAGCOMP_BACK,
            bool(backs) and all(LAGCOMP_BACK[0] <= x <= LAGCOMP_SHOT_MOST for x in backs), "%s" % backs)
    s.check("his moves say as much (each move's view time: what lag compensation of his real shots uses)",
            bool(moves) and all(LAGCOMP_BACK[0] <= x <= LAGCOMP_BACK[1] for x in moves), "%s" % moves)
    # (mpreview B2) a remote human's real blast and bolt (QC's FireBulletsImpl, LightningDamage) rewind the others once
    # each, their pellets' and lines' tracelines inside that one scope (vr_dm_test 12: one frame)
    # (mpreview D8, first: an inbox line longer than its 4 KB read is dropped, and the inbox goes on with the next)
    since_b = b.mark()
    b.cmd("echo mpfix_long " + "x" * 5000)
    b.cmd("echo mpfix_inbox_after")
    _, after = b.wait(r"mpfix_inbox_after", 20, since_b, "B's inbox past a 5000-byte line")
    s.check("B's inbox drops a line longer than its read and runs the next (vr_mptest.cpp pollInbox)",
            b.find(r"inbox line longer than", since_b) is not None and b.find(r"mpfix_long", since_b) is None, after)
    since = srv.mark()
    srv.cmd("vr_dm_test 12")
    _, verdict = srv.wait(r"dmtest: \d+ passed, \d+ failed", 30, since, "vr_dm_test 12's verdict")
    got = [l.strip() for l in srv.lines[since:] if "dmtest: " in l]
    s.check("a remote human's blast and bolt each rewind the others once, not per pellet or line (vr_dm_test 12)",
            " 0 failed" in verdict, "%s %s" % (verdict.strip(), got[-4:]))


VOICE_HZ = 440             # A's test tone (vr_voicechat_test_tone: in place of his microphone)
VOICE_NAME = "VoiceA"       # A's name (his row on B's Voice Chat page)
VOICE_DELAY_MS = 250        # A's tone captured -> heard at B (the capture's frame, the server, B's jitter delay and lead)
VOICE_BUDGET = 8000         # vr_net_voice_bytes' default, B/s (a frame and its copy in the next datagram: ~6.8 KB/s)
VOICE_LOW_BUDGET = 1500     # the server's budget cut to this: B's voice bytes held to it, frames dropped
VOICE_RATE = 8000           # B's vr_net_rate: his voice budget half of it (4000 B/s), still over A's ~3.3 KB/s
VOICE_SLACK = 1.2           # a budget's bucket burst and the measuring window's edges
VOICE_TONE_C = 660          # C's test tone (two talking at once)
VOICE_TWO_LOW_BUDGET = 5000 # the server's budget, B/s, under both talkers' new frames (~3.4 KB/s each): each a fair part
VOICE_TWO_ALIKE = 0.7       # the two talkers' shares of their frames that reach B: the smaller at least this of the larger
# ... over this many of A's frames (20 ms each): the counts are read one game after another and B's lag after them,
# frames in flight at the window's ends counted on one side only; at 100 (2 s) a busy machine's reports apart put one
# talker's share at 0.38 and the other's at 0.56 (1 suite of 6 under --load 16)
VOICE_TWO_FRAMES = 250


def voice_report(inst, hz=VOICE_HZ):
    """`inst`'s vr_voicechat_report -> (its sending line's fields, {slot: its speaker line's fields})."""
    since = inst.mark()
    inst.cmd("vr_voicechat_report %d" % hz)
    i, _ = inst.wait(r"mp_voice: done", 10, since, what="%s's voice report" % inst.name)
    send, speakers = {}, {}
    for line in inst.lines[since:i + 1]:
        if "mp_voice_send:" in line:
            send = fields(line)
        elif "mp_voice: slot=" in line:
            f = fields(line)
            speakers.setdefault(int(f["slot"]), {}).update(f)
        elif "mp_voice_row: slot=" in line:
            # (the Voice Chat page's row for him: his name, his volume, muted, talking)
            row = line.split("mp_voice_row: ", 1)[1].strip()
            slot, text = row.split(" ", 1)
            speakers.setdefault(int(slot.split("=")[1]), {})["row"] = text
    return send, speakers


def voice_out(f):
    """A speaker line's frames out of B's jitter buffer (played, recovered by FEC, concealed)."""
    return int(f["played"]) + int(f["recovered"]) + int(f["concealed"]) if f else 0


def voice_until(s, inst, slot, done, what, tries=400):
    """`inst`'s reports until done(send, speaker of `slot`) -> those (a definite end: `tries` reports)."""
    send, sp = {}, None
    for _ in range(tries):
        send, speakers = voice_report(inst)
        sp = speakers.get(slot)
        if done(send, sp):
            return send, sp
    raise StepFailed("%s: %s never came (last: %s)" % (inst.name, what, sp))


def voice_relay(srv, slot):
    """The server's relay counts for listener `slot` (vr_voicechat_relay's "mp_voice_relay:" line; {} none)."""
    since = srv.mark()
    srv.cmd("vr_voicechat_relay")
    i, _ = srv.wait(r"mp_voice_relay: done", 10, since, what="%s's relay report" % srv.name)
    return next((fields(l) for l in srv.lines[since:i] if "slot=%d " % slot in l), {})


def voice_relay_until(srv, slot, done, what, tries=400):
    """voice_relay until done(its fields) (a definite end: `tries` reports) -> those."""
    f = {}
    for _ in range(tries):
        f = voice_relay(srv, slot)
        if done(f):
            return f
    raise StepFailed("%s: %s never came (last: %s)" % (srv.name, what, f))


def voice_bytes(inst):
    """`inst`'s voice bytes read since its vr_net_bytes reset."""
    since = inst.mark()
    inst.cmd("vr_net_bytes")
    _, line = inst.wait(r"mp_netbytes: ", 10, since, what="%s's byte counts" % inst.name)
    return int(fields(line)["voice"])


def voice_rate(s, b, a, sa, label, frames=100):
    """B's voice bytes a second while A sends `frames` more frames (vr_net_bytes reset first) -> B/s."""
    since = b.mark()
    b.cmd("vr_net_bytes reset")
    b.wait(r"mp_netbytes: ", 10, since)
    t0 = time.monotonic()
    start = int(voice_report(a)[0]["frames"])
    voice_until(s, a, sa, lambda send, sp: int(send.get("frames", 0)) >= start + frames, "%s: %d more frames" % (label, frames))
    rate = voice_bytes(b) / max(0.1, time.monotonic() - t0)
    s.metric("voice_bytes_s_" + label, round(rate))
    return rate


def voice(s, listen=False, lag=0.0, jitter=0.0, loss=0.0):
    """(B10v) A's voice (a test tone in place of his microphone, -nomic: none opened) heard at B: received, played in
    order through the jitter buffer, the tone's frequency kept, within the delay bound; at his head (nearer louder, the
    radio full); muted and his volume; push-to-talk released ends it, voice activation starts and stops it; the server's
    voice budget and B's rate cap hold his voice bytes; noise suppression on keeps it flowing; a press without a
    microphone breaks nothing."""
    # (B joins through the jitter and loss: B13's selective acks; before them a join through 30 ms of reordering and 5%
    # loss could stall in its signon)
    srv, a, b, ea, eb = join(s, listen=listen, lag=lag, jitter=jitter, loss=loss)
    sa = ea - 1
    bound = VOICE_DELAY_MS + lag + jitter
    a.cmd("name %s" % VOICE_NAME)
    voice_until(s, b, sa, lambda send, sp: sp and sp.get("row", "").startswith(VOICE_NAME), "A's name on B's Voice Chat page")
    since_a, since_b = a.mark(), b.mark()
    a.cmd("vr_voicechat_denoise 0; vr_voicechat_mode 0; vr_voicechat_test_level 0.25; vr_voicechat_test_tone %d; "
          "vr_voicechat_reset; +vr_voicechat" % VOICE_HZ)
    _, tx = a.wait(r"mp_voice_tx: ", 20, since_a, what="A sending his first voice frame")
    _, rx = b.wait(r"mp_voice_rx: t=\d+ slot=%d " % sa, 20, since_b, what="B playing A's first voice frame")
    delay = int(fields(rx)["t"]) - int(fields(tx)["t"])
    s.metric("voice_delay_ms", delay)
    s.check("A's first voice frame heard at B within %d ms" % bound, 0 <= delay < bound,
            "%d ms (B decodes it %s ms ahead of his mix)" % (delay, fields(rx)["ahead_ms"]))

    # three seconds of tone: B plays every frame A sent (in order, the lost recovered or concealed), the tone kept
    _, sp = voice_until(s, b, sa, lambda send, sp: voice_out(sp) >= 150, "150 frames of A's voice at B")
    send_a = voice_report(a)[0]
    _, speakers = voice_report(b)
    sp = speakers[sa]
    sent, out = int(send_a["frames"]), voice_out(sp)
    s.metric("voice_b", sp)
    # (with jitter a few may come late before his delay grows to it: the first spurt starts at 40 ms, its jitter not
    # measured yet; each late one raises the delay by a frame, at most 8 times)
    most_late = 8 if jitter > 0 else 0
    s.check("B's jitter buffer gives out every frame A sent (played, recovered, concealed; at most %d late)" % most_late,
            sent - 20 <= out <= sent + 2 and int(sp["late"]) <= most_late,
            "A sent %d, B gave out %d: %s played, %s recovered by FEC, %s concealed, %s late, %s skipped, jitter %s ms" % (
                sent, out, sp["played"], sp["recovered"], sp["concealed"], sp["late"], sp["skipped"], sp["jitter_ms"]))
    share_min = 0.5 if loss > 0 else 0.8
    s.check("the tone heard at B (%d Hz, its share of the power over %.1f)" % (VOICE_HZ, share_min),
            # (its zero crossings: a concealed frame's glitches add a few)
            float(sp["tone"]) > share_min and abs(float(sp["freq"]) - VOICE_HZ) < (25 if loss > 0 else 10),
            "share %s, %s Hz, rms %s; %s copies, %s frames of delay added for late ones" % (
                sp["tone"], sp["freq"], sp["rms"], sp["dups"], sp["stretched"]))
    if loss > 0:
        s.check("B's losses recovered (Opus FEC) or concealed", int(sp["recovered"]) + int(sp["concealed"]) > 0,
                "%s recovered, %s concealed" % (sp["recovered"], sp["concealed"]))
    s.check("B shows A talking", sp["talking"] == "1", "talking=%s" % sp["talking"])

    # at his head: nearer louder; the radio full
    near_d, near_g = float(sp["dist"]), float(sp["gain"])
    put_at(srv, a, ea, (OPEN_SPOT[0], OPEN_SPOT[1] + 700.0, OPEN_SPOT[2]))
    _, far = voice_until(s, b, sa, lambda send, sp: sp and float(sp["dist"]) > near_d + 400, "A's voice from afar at B")
    s.check("A's voice quieter from farther (vr_voicechat_attenuation 0.5)", float(far["gain"]) < near_g - 0.15,
            "%.0f units: gain %.3f; %s units: %s" % (near_d, near_g, far["dist"], far["gain"]))
    b.cmd("vr_voicechat_radio 1")
    _, radio = voice_until(s, b, sa, lambda send, sp: sp and float(sp["gain"]) > 0.99, "the radio's full volume")
    s.check("the radio: A at full volume wherever he is", True, "%s units: gain %s" % (radio["dist"], radio["gain"]))
    b.cmd("vr_voicechat_radio 0")
    put_at(srv, a, ea, OPEN_SPOT)

    # his volume, then muted: from his row on B's Voice Chat page (VR Settings > HUD, Haptics and Sound > Voice Chat),
    # its buttons pressed as a click would (vr_menu_press "<row> / <button>"; B10v part 2)
    _, lit = voice_until(s, b, sa, lambda send, sp: sp and sp.get("row", "").startswith(VOICE_NAME), "A's row on B's page")
    s.check("B's Voice Chat page: A's row, lit while he talks", "talking" in lit["row"] and "100%" in lit["row"],
            "row \"%s\"" % lit["row"])
    page = "vr_menu_press Voice Chat > %s / " % VOICE_NAME
    b.cmd(page + "Quieter; " + page + "Quieter")
    _, half = voice_until(s, b, sa, lambda send, sp: sp and float(sp["dist"]) < near_d + 200 and
                          float(sp["gain"]) < 0.6, "A at half volume")
    s.check("A's own volume at B (his row's Quieter twice: 50%)", float(half["gain"]) <= 0.5 + 1e-3 and "50%" in half["row"],
            "gain %s, row \"%s\"" % (half["gain"], half["row"]))
    b.cmd(page + "Louder; " + page + "Louder; " + page + "Mute")
    _, muted = voice_until(s, b, sa, lambda send, sp: sp and sp["muted"] == "1" and sp["talking"] == "0", "A muted")
    played0 = voice_out(muted)
    # (B tells the server whom he muted, vr_voicechat_listen: the relay sends him none of A's frames, only a note while A
    # talks, so his row still says so; REVIEW_MP_2026-11-18 finding 4)
    relay0 = voice_relay_until(srv, eb - 1, lambda f: int(f.get("muted", 0)) == 1 << sa, "B's mute of A at the server")
    # (his count from once the frames sent before the mute are in: 8 of A's held back, 160 ms, past B's link's 50 ms and
    # its 30 ms of jitter; counted at once, 3 still on their way arrived after it in a voice_lossy run)
    relay0 = voice_relay_until(srv, eb - 1, lambda f: int(f["out_muted"]) >= int(relay0["out_muted"]) + 8,
                               "8 of A's frames held back from B")
    rec0 = int(voice_report(b)[1][sa]["received"])
    relay1 = voice_relay_until(srv, eb - 1, lambda f: int(f["out_muted"]) >= int(relay0["out_muted"]) + 25,
                               "25 of A's frames held back from B")
    _, later = voice_until(s, b, sa, lambda send, sp: sp and "muted, talking" in sp.get("row", ""), "A's row: muted, talking")
    s.check("muted (his row's Mute): the server sends B none of A's frames, none played; his row says muted, talking",
            int(later["received"]) <= rec0 + 2 and voice_out(later) == played0 and "100%" in later["row"],
            "received %d -> %s, given out %d -> %d, held back at the server %s -> %s, row \"%s\"" % (
                rec0, later["received"], played0, voice_out(later), relay0["out_muted"], relay1["out_muted"], later["row"]))
    # (REVIEW_MP_2026-11-18 finding 15) a rename keeps the mute: A as VoiceA2 still muted at B, at the server too
    a.cmd("name %s2" % VOICE_NAME)
    _, ren = voice_until(s, b, sa, lambda send, sp: sp and sp.get("row", "").startswith(VOICE_NAME + "2"), "A's new name at B")
    relay_ren = voice_relay(srv, eb - 1)
    s.check("A renamed while muted: still muted at B and at the server (a mute is not escaped by a new name)",
            ren["muted"] == "1" and "muted" in ren["row"] and int(relay_ren.get("muted", 0)) == 1 << sa,
            "row \"%s\", the server's mask for B %s" % (ren["row"], relay_ren.get("muted")))
    a.cmd("name %s" % VOICE_NAME)
    voice_until(s, b, sa, lambda send, sp: sp and sp.get("row", "").startswith(VOICE_NAME + "   "), "A's name back at B")
    b.cmd(page + "Unmute")
    voice_until(s, b, sa, lambda send, sp: sp and sp["talking"] == "1", "A heard again")

    # the budgets: the server's (B's voice bytes held to it, frames dropped), B's rate cap (half of it his voice's)
    full = voice_rate(s, b, a, sa, "full")
    s.check("A's voice on B's link within the voice budget (%d B/s)" % VOICE_BUDGET,
            1500 < full < VOICE_BUDGET * VOICE_SLACK, "%.0f B/s" % full)
    since = srv.mark()
    srv.cmd("vr_net_voice_bytes %d; echo mp_voice_budget" % VOICE_LOW_BUDGET)
    srv.wait(r"mp_voice_budget", 10, since)
    low = voice_rate(s, b, a, sa, "low")
    since = srv.mark()
    srv.cmd("vr_voicechat_relay")
    i, _ = srv.wait(r"mp_voice_relay: done", 10, since)
    relay_b = next((fields(l) for l in srv.lines[since:i] if "slot=%d " % (eb - 1) in l), {})
    s.check("the server's budget cut to %d B/s: B's voice bytes held to it, A's late frames dropped" % VOICE_LOW_BUDGET,
            low < VOICE_LOW_BUDGET * VOICE_SLACK and int(relay_b.get("out_dropped", 0)) > 0,
            "%.0f B/s; the relay to B: %s" % (low, relay_b))
    srv.cmd("vr_net_voice_bytes %d" % VOICE_BUDGET)
    b.cmd("vr_net_rate %d" % VOICE_RATE)
    capped = voice_rate(s, b, a, sa, "rate")
    s.check("under B's vr_net_rate %d: his voice within half of it, still heard" % VOICE_RATE,
            1500 < capped < VOICE_RATE / 2 * VOICE_SLACK, "%.0f B/s" % capped)
    b.cmd("vr_net_rate 0")

    # push-to-talk released: the spurt ends at B; voice activation sends while the tone is loud, stops when it is not
    a.cmd("-vr_voicechat")
    voice_until(s, a, sa, lambda send, sp: send.get("sending") == "0", "A's sending stopped")
    _, ended = voice_until(s, b, sa, lambda send, sp: sp and sp["talking"] == "0", "A's spurt over at B")
    s.check("push-to-talk released: A no longer heard talking at B", True, "%s spurts" % ended["spurts"])
    # the Push-to-Talk key (vr_flat_key_voicechat, CAPSLOCK; B10v part 2) held: sent; let go: not
    a.cmd("vr_mock_key CAPSLOCK down")
    voice_until(s, b, sa, lambda send, sp: sp and sp["talking"] == "1", "A heard at B while his Push-to-Talk key is held")
    a.cmd("vr_mock_key CAPSLOCK up")
    send, _ = voice_until(s, a, sa, lambda send, sp: send.get("sending") == "0", "A's Push-to-Talk key let go")
    s.check("the Push-to-Talk key: A sent while it is held, not once let go", True, "%s spurts" % send["spurts"])
    a.cmd("vr_voicechat_mode 1")
    voice_until(s, a, sa, lambda send, sp: send.get("sending") == "1", "voice activation sending the tone")
    a.cmd("vr_voicechat_test_level 0")
    send, _ = voice_until(s, a, sa, lambda send, sp: send.get("sending") == "0", "voice activation stopped on silence")
    s.check("voice activation: sent while the tone is loud, stopped after it falls silent", True, "%s spurts" % send["spurts"])

    # noise suppression on: the frames still flow and come through
    a.cmd("vr_voicechat_mode 0; vr_voicechat_denoise 1; vr_voicechat_test_level 0.25; +vr_voicechat")
    rec0 = int(voice_report(b)[1][sa]["received"])
    _, dn = voice_until(s, b, sa, lambda send, sp: sp and int(sp["received"]) >= rec0 + 50, "frames with RNNoise on")
    s.check("RNNoise on: A's frames still reach B", True, "tone share after RNNoise %s" % dn["tone"])

    # (REVIEW_MP_2026-11-18 finding 4) two talking at once: C joins and talks with A, B hears both whole under the default
    # budget and each a fair part under one too small for both (before: the lower slot's new frames and copies came
    # first, the other got a third of his frames, under the small budget none)
    voice_two(s, srv, a, b, sa, loss)

    # no microphone (-nomic): a press says so and breaks nothing
    since = a.mark()
    a.cmd("-vr_voicechat; vr_voicechat_test_tone 0; +vr_voicechat")
    a.wait(r"Voice chat: no microphone", 10, since, what="A told he has no microphone")
    send, _ = voice_report(a)
    s.check("push-to-talk without a microphone: told, nothing opened, the game on", send.get("mic") == "0" and send.get("sending") == "0", "%s" % send)
    a.cmd("-vr_voicechat")


def voice_two(s, srv, a, b, sa, loss):
    """A and a third player C talking at once, heard at B (voice's last part)."""
    c = s.client("C").start()
    since_c = c.mark()
    c.cmd(s.connect_cmd(c))
    _, jc = c.wait(r"mp_event: .*joined map=e1m1", 120, since_c, what="C joined e1m1")
    sc = int(fields(jc)["self"]) - 1
    a.cmd("vr_voicechat_denoise 0")
    c.cmd("vr_voicechat_denoise 0; vr_voicechat_mode 0; vr_voicechat_test_level 0.25; vr_voicechat_test_tone %d; "
          "+vr_voicechat" % VOICE_TONE_C)
    voice_until(s, b, sc, lambda send, sp: sp and sp.get("talking") == "1", "C talking at B")
    voice_until(s, b, sa, lambda send, sp: sp and sp.get("talking") == "1", "A talking at B")
    # (B's link with jitter and loss: the copies, which cover NetQuake's drop of an overtaken datagram, get only what the
    # two talkers' new frames leave, so fewer arrive; the two still alike)
    for label, budget, least in (("two", VOICE_BUDGET, 0.5 if loss > 0 else 0.9),
                                 ("two_low", VOICE_TWO_LOW_BUDGET, 0.3 if loss > 0 else 0.5)):
        since = srv.mark()
        srv.cmd("vr_net_voice_bytes %d; echo mp_voice_budget" % budget)
        srv.wait(r"mp_voice_budget", 10, since)
        voice_report(b)  # (a report between: the budget in force)
        sent0 = {sa: int(voice_report(a)[0]["frames"]), sc: int(voice_report(c)[0]["frames"])}
        got0 = voice_report(b)[1]
        voice_until(s, a, sa, lambda send, sp: int(send.get("frames", 0)) >= sent0[sa] + VOICE_TWO_FRAMES,
                    "%s: A's %d more frames" % (label, VOICE_TWO_FRAMES), tries=1500)
        sent1 = {sa: int(voice_report(a)[0]["frames"]), sc: int(voice_report(c)[0]["frames"])}
        got1 = voice_report(b)[1]
        shares = {}
        for slot in (sa, sc):
            # (his frames that reached B, each once: what the relay sent him, whatever B's own frame rate made of their
            # play-out on a busy machine)
            heard = lambda f: int(f["received"]) - int(f["dups"])
            shares[slot] = (heard(got1[slot]) - heard(got0[slot])) / max(1, sent1[slot] - sent0[slot])
        s.metric("voice_%s_shares" % label, shares)
        s.check("two talking under %d B/s: each of A's and C's frames reach B, at least %.0f%% of them, alike (within %.0f%%)" % (
                    budget, least * 100, (1 - VOICE_TWO_ALIKE) * 100),
                min(shares.values()) >= least and min(shares.values()) >= VOICE_TWO_ALIKE * max(shares.values()),
                "A %.2f, C %.2f of their frames reached B" % (shares[sa], shares[sc]))
    srv.cmd("vr_net_voice_bytes %d" % VOICE_BUDGET)
    c.cmd("-vr_voicechat")


DM_MAP = "dm4"          # (B11) a small deathmatch map with FrikBot's waypoints compiled in (QC/waypoints)
DM_FRAGLIMIT = 3
DM_TIMELIMIT = 8        # minutes: QC's own end of the match (a safety net: the fraglimit must end it first)
BOT_HAND_GAP = 12.0     # units: a bot's jointed hand drawn at most this far from its controller (B11 part 3)
BOT_GUN_GRIP = 24.0     # units: ... its gun's model origin at most this far from the hand holding it


# (B14b) The lockstep runs: the server's world steps one move of each client a step, by number (vr_mp_lockstep); its
# clients at most LOCK_AHEAD moves ahead; the world held at each of LOCK_STEPS for the hashes and the next inputs.
LOCK_AHEAD = 4
TRACELOG = 0  # (--tracelog)
# (B14e) Every edict field compared, the client-reported ones too (the ammo pouch, the light on him, the melee history):
# a lockstep client's frames wait until it is in the game, and each move is made having read the same server messages
# (vr_mptest.cpp: VR_LockstepReadsMore), so its moves are the same every run, checked move by move (lock_moves_diff).
LOCK_STEPS = (36, 360, 720)
LOCK_DM_STEPS = (36, 72, 360, 720)
LOCK_THROW_STEPS = (36, 72, 144, 216, 432)  # (B14e: the grips, the gives, the throws, in flight, at rest)
LOCK_VALUE_FIELDS = ("ammopouch", "stl_light", "mh_", "vr_posetime")  # (printed where two runs differ)
LOCK_SERVER = ("+vr_fixed_frames", "1", "+sys_ticrate", "0.002", "+sv_random_seed", "1", "+vr_mp_lockstep", "2",
               "+vr_mp_lockstep_until", str(LOCK_STEPS[0]), "+vr_hull_async", "0", "+vr_mp_lockstep_movelog", "1")
LOCK_CLIENT = ("+vr_fixed_frames", "1", "+vr_mock_fast", "1", "+vr_mp_lockstep_ahead", str(LOCK_AHEAD))


def lock_status(inst, prefix):
    """vr_mp_lockstep_status's lines starting with `prefix` -> their fields (in order)."""
    since = inst.mark()
    inst.cmd("vr_mp_lockstep_status")
    inst.wait(r"^mp_lockstep(_cl)?: (step|newest)=", 10, since, "%s's lockstep status" % inst.name)
    return [fields(l) for l in inst.lines[since:] if l.startswith(prefix)]


def lock_hold(srv, clients, step, first=False):
    """The world held after `step` (vr_mp_lockstep_until; `first`: the server's start-up value), then each client
    waiting at his newest move the server has run plus LOCK_AHEAD: a command a client runs now lands on his next move,
    the same one every run."""
    since = 0 if first else srv.mark()
    if not first:
        srv.cmd("vr_mp_lockstep_until %d" % step)
    srv.wait(r"^mp_lockstep: held step=%d " % step, 300, since, "the world held at step %d" % step)
    lasts = {int(f["slot"]): int(f["last"]) for f in lock_status(srv, "mp_lockstep_slot:")}
    for c, ent in clients:
        want = lasts.get(ent - 1, -1) + LOCK_AHEAD
        f = {}
        for _ in range(300):
            f = lock_status(c, "mp_lockstep_cl:")[-1]
            if int(f["newest"]) == want and f["waiting"] == "1":
                break
            time.sleep(0.02)
        else:
            raise StepFailed("%s never waited at move %d (step %d): %s" % (c.name, want, step, f))


def lock_hashes(srv):
    """The server's hashes now: the rigid bodies (vr_physics_hash), every edict's fields (vr_bench_statehash), the
    navmesh and the monsters' paths (vr_nav_hash) -> {kind: its line}; "fields": each QuakeC field's hash over the
    edicts (vr_bench_statehash fields: what two runs differ in)."""
    since = srv.mark()
    srv.cmd("vr_physics_hash; vr_bench_statehash fields; vr_nav_hash")
    out = {}
    for kind, rx in (("physics", r"^vr_physics_hash: "), ("state", r"^vr_bench_statehash: "), ("nav", r"^vr_nav_hash: ")):
        _, line = srv.wait(rx, 20, since, "the server's %s hash" % kind)
        out[kind] = line.split(": ", 1)[1].split(" (server random seed")[0].strip()
    out["fields"] = dict(l.split()[1:3] for l in srv.lines[since:] if l.startswith("statefield "))
    return out


def lock_values(srv, ents, names):
    """The players' values of the QuakeC fields starting with one of `names` (vr_bench_statehash values: each 4-byte
    slot as a float) -> {"ent field": value}: what two runs' differing fields hold."""
    out = {}
    for name in names:
        since = srv.mark()
        srv.cmd("vr_bench_statehash values %s %s" % (name, " ".join(str(e) for e in ents)))
        srv.wait(r"^statevalue end", 10, since, "the %s values" % name)
        for l in srv.lines[since:]:
            parts = l.split()
            if len(parts) == 4 and parts[0] == "statevalue":
                out["%s %s" % (parts[1], parts[2])] = parts[3]
    return out


def lock_field_diff(f1, f2):
    """The QuakeC fields whose hashes two runs' lock_hashes differ in (names, at most 12)."""
    names = sorted(k for k in set(f1) | set(f2) if f1.get(k) != f2.get(k))
    return ", ".join(names[:12]) + (" (+%d)" % (len(names) - 12) if len(names) > 12 else "")


def lockstep_run(s, r, dm=False, throw=False):
    """One lockstep run on e1m1 (coop): A and B (both the mock headset) join; held at each of LOCK_STEPS: the hashes;
    after the first B walks north and A's impulse makes a rottweiler (vr_test_spawn 7) ahead of him, which hunts them
    (the navmesh's paths); after the second B stops and A strafes. -> {step: hashes}, the stopped games.
    `dm` (B14c): a deathmatch on dm4 instead, held at each of LOCK_DM_STEPS: after the first A adds a FrikBot bot
    (impulse 100) and B walks north, after the second A adds another, after the third B stops and A strafes.
    `throw` (B14e): on e1m1, held at each of LOCK_THROW_STEPS: B grips his main hand low, is given a shotgun into it
    (impulse 154), swings it up and lets go in one move (the mock's hand that fast: a hard throw), and the prop flies;
    A grips his off hand and is given an axe (impulse 172: 170 + WID_AXE 2, the off hand) and throws it the same way."""
    tag = ("dm" if dm else "throw" if throw else "") + ("" if r == 1 else str(r))
    if r > 1 or dm or throw:
        s.port = s.next_port + 10  # (each run's own ports)
        s.next_port = s.port + 1
    mapname = DM_MAP if dm else "e1m1"
    srv = s.server(dedicated=True, name="server" + tag, extra=LOCK_SERVER)
    srv.start()
    srv.cmd(("coop 0; deathmatch 1; fraglimit 0; timelimit 0; skill 1; map %s; vr_mp_status" if dm else
             "coop 1; deathmatch 0; skill 1; map %s; vr_mp_status") % mapname)
    srv.wait(r"mp_server: map=%s" % mapname, 120, what="the server on %s" % mapname)
    a = s.client("A" + tag, extra=LOCK_CLIENT).start()
    b = s.client("B" + tag, extra=LOCK_CLIENT).start()
    a.cmd(s.connect_cmd(a))
    _, ja = a.wait(r"mp_event: .*joined map=%s" % mapname, 120, what="%s joined" % a.name)
    b.cmd(s.connect_cmd(b))
    _, jb = b.wait(r"mp_event: .*joined map=%s" % mapname, 120, what="%s joined" % b.name)
    ea, eb = int(fields(ja)["self"]), int(fields(jb)["self"])
    clients = ((a, ea), (b, eb))
    if TRACELOG:  # (--tracelog N: both sides' traces of B's move N, vr_mp_predict_tracelog)
        srv.cmd("vr_mp_predict_tracelog %d" % TRACELOG)
        b.cmd("vr_mp_predict_tracelog %d" % TRACELOG)
    hashes = {}

    def held(step, first=False):
        lock_hold(srv, clients, step, first)
        hashes[step] = lock_hashes(srv)
        hashes[step]["values"] = lock_values(srv, (ea, eb), LOCK_VALUE_FIELDS)

    if throw:
        held(LOCK_THROW_STEPS[0], first=True)
        b.cmd("vr_mock_hand main 0.1 0.25 -0.35 0 0 0; vr_mock_button main grip 1; vr_mock_button off grip 0")
        a.cmd("vr_mock_hand off -0.1 0.25 -0.35 0 0 0; vr_mock_button off grip 1; vr_mock_button main grip 0")
        held(LOCK_THROW_STEPS[1])
        b.cmd("impulse 154")
        a.cmd("impulse 172")
        held(LOCK_THROW_STEPS[2])
        def holding():
            (lines,) = reports(s, [srv])
            sv = by_ent(lines, "mp_avatar_sv")
            return {"A": int(sv.get(ea, {}).get("wpn", [-1, -1])[1]), "B": int(sv.get(eb, {}).get("wpn", [-1])[0])}

        hashes["holding"] = holding()
        b.cmd("vr_mock_hand main 0.25 1.5 -0.8 -20 0 0; vr_mock_button main grip 0")
        a.cmd("vr_mock_hand off -0.25 1.5 -0.8 -20 0 0; vr_mock_button off grip 0")
        held(LOCK_THROW_STEPS[3])
        hashes["thrown"] = holding()
        held(LOCK_THROW_STEPS[4])
    elif dm:
        held(LOCK_DM_STEPS[0], first=True)
        a.cmd("impulse 100")
        b.cmd("vr_mock_stick off 0 1")
        held(LOCK_DM_STEPS[1])
        a.cmd("impulse 100")
        held(LOCK_DM_STEPS[2])
        b.cmd("vr_mock_stick off 0 0")
        a.cmd("vr_mock_stick off 1 0")
        held(LOCK_DM_STEPS[3])
        hashes["players"] = frag_table(a)
    else:
        held(LOCK_STEPS[0], first=True)
        srv.cmd("vr_test_spawn 7; vr_test_spawn_dist 250")
        a.cmd("impulse 241")
        b.cmd("vr_mock_stick off 0 1")
        held(LOCK_STEPS[1])
        b.cmd("vr_mock_stick off 0 0")
        a.cmd("vr_mock_stick off 1 0")
        held(LOCK_STEPS[2])
    hashes["predict"] = predict_report(b)
    for inst in (a, b, srv):
        inst.stop()
    hashes["moves"] = lock_moves(srv)
    s.note("run %s: %s" % (tag, {st: {k: v for k, v in h.items() if k not in ("fields", "values")}
                                 for st, h in hashes.items() if st not in ("predict", "players", "moves")}))
    return hashes


def lock_moves(srv):
    """(B14e) The moves the lockstep world ran, from the server's mp_movelog lines (vr_mp_lockstep_movelog): {(slot,
    number from his first): [every value as the world got it]}."""
    moves = {}
    for l in srv.lines:
        if l.startswith("mp_movelog: "):
            f = l.split()
            moves.setdefault((f[1], int(f[2][4:])), {})[int(f[3][5:])] = [int(x) for x in f[4:]]
    return {k: sum((p[i] for i in sorted(p)), []) for k, p in moves.items()}


def lock_compare(s, h1, h2, steps):
    """Two lockstep runs' hashes at each of `steps` checked equal: the rigid bodies, the navmesh, every edict field
    (the players' values of LOCK_VALUE_FIELDS noted where they differ); then every move each player sent."""
    for step in steps:
        for kind in ("physics", "nav"):
            same = h1[step][kind] == h2[step][kind]
            s.check("step %d: the %s hash the same in both runs" % (step, kind), same,
                    h1[step][kind] if same else "%s | %s" % (h1[step][kind], h2[step][kind]))
        f1, f2 = h1[step]["fields"], h2[step]["fields"]
        off = [k for k in set(f1) | set(f2) if f1.get(k) != f2.get(k)]
        s.check("step %d: every edict field the same in both runs" % step, not off and f1,
                "%d fields; %s%s" % (len(f1), h1[step]["state"], ("; differ: " + lock_field_diff(
                    {k: f1.get(k) for k in off}, {k: f2.get(k) for k in off})) if off else ""))
        v1, v2 = h1[step]["values"], h2[step]["values"]
        vdiff = sorted(k for k in set(v1) | set(v2) if v1.get(k) != v2.get(k))
        if vdiff:
            s.note("step %d: values that differ: %s" % (step, "; ".join("%s: %s | %s" % (k, v1.get(k), v2.get(k))
                                                                     for k in vdiff[:16])))
    s.check("the play changed the world (the edicts' hash moved on)",
            len(set(h1[st]["state"] for st in steps)) == len(steps))
    lock_moves_diff(s, h1, h2)


def lock_moves_diff(s, h1, h2):
    """(B14e) Each player's moves the same in both runs (every value the world got, by number from his first; the
    lockstep's inputs: a client's view a function of its moves). Where they differ, the first few moves noted with the
    values' indices (vr_netmove.cpp visitMove's order) and both runs' values."""
    m1, m2 = h1["moves"], h2["moves"]
    for slot in sorted(set(k[0] for k in m1) | set(k[0] for k in m2)):
        r1 = set(k[1] for k in m1 if k[0] == slot)
        r2 = set(k[1] for k in m2 if k[0] == slot)
        rels = sorted(r1 & r2)
        off = [r for r in rels if m1[(slot, r)] != m2[(slot, r)]]
        s.check("%s: every move the same in both runs" % slot, rels and r1 == r2 and not off,
                "%d | %d moves, %d differ%s" % (len(r1), len(r2), len(off), (" from %d" % off[0]) if off else ""))
        for r in off[:6]:
            v1, v2 = m1[(slot, r)], m2[(slot, r)]
            idx = [i for i in range(min(len(v1), len(v2))) if v1[i] != v2[i]]
            s.note("  move %d: %s" % (r, " ".join("[%d]%d|%d" % (i, v1[i], v2[i]) for i in idx[:14])))


def lockstep_dm(s):
    """(B14c) Two lockstep deathmatch runs on dm4 with two FrikBot bots (lockstep_run dm): the bots' thinking (random()
    on the server's seeded stream), their waypoints' navigation and their walks through the players' moves the same in
    both: every hash equal at each held step."""
    h1 = lockstep_run(s, 1, dm=True)
    h2 = lockstep_run(s, 2, dm=True)
    lock_compare(s, h1, h2, LOCK_DM_STEPS)
    p1, p2 = h1["players"], h2["players"]
    s.check("four players in each run (A, B, two bots), named and scored the same", len(p1) == 4 and p1 == p2,
            "%s | %s" % (p1, p2))


def lockstep_throw(s):
    """(B14e) Two lockstep runs of props thrown by players (lockstep_run throw): B's shotgun from his main hand, A's
    axe from his off hand, given into a gripping hand and thrown hard; every hash equal at each held step (the
    thrown props' rigid bodies, their flight and their landing), every move the same."""
    h1 = lockstep_run(s, 1, throw=True)
    h2 = lockstep_run(s, 2, throw=True)
    for h, run in ((h1, 1), (h2, 2)):
        s.check("run %d: each player holds the prop given before his throw (the server's avatars)" % run,
                h["holding"]["A"] > 0 and h["holding"]["B"] > 0, str(h["holding"]))
        s.check("run %d: neither holds it after his throw" % run, h["thrown"]["A"] <= 0 and h["thrown"]["B"] <= 0,
                str(h["thrown"]))
    lock_compare(s, h1, h2, LOCK_THROW_STEPS)
    st = LOCK_THROW_STEPS
    s.check("the throws moved the rigid bodies (the physics hash from the throw on, each held step's its own)",
            len(set(h1[x]["physics"] for x in st[2:])) == len(st[2:]), " | ".join(h1[x]["physics"] for x in st[2:]))


def lockstep(s):
    """(B14b) Two lockstep runs of the same play (lockstep_run): at each held step the rigid bodies', the edicts' and
    the navmesh's and paths' hashes the same in both; the play did something (B moved, the rottweiler has a path)."""
    h1 = lockstep_run(s, 1)
    h2 = lockstep_run(s, 2)
    lock_compare(s, h1, h2, LOCK_STEPS)
    agents = re.search(r"(\d+) agents, paths ([0-9a-f]+) \((\d+) found\)", h1[LOCK_STEPS[2]]["nav"])
    s.check("a monster followed a navmesh path", agents is not None and int(agents.group(3)) > 0,
            h1[LOCK_STEPS[2]]["nav"])
    # B's replays against the server's results (B14), a note: the states he compares at are those his datagrams carried
    # (not every step's), so the counts differ between runs; the rottweiler's bite at about step 350 knocks him back
    # between two of his moves (QuakeC's knockback: not predicted), a miss when that state reaches him.
    p1, p2 = h1["predict"], h2["predict"]
    s.note("B's prediction: " + " ".join("%s=%s|%s" % (k, p1.get(k), p2.get(k))
                                          for k in ("exact_n", "exact_miss", "exact_max", "vel_miss")))


def frag_table(inst, since=0):
    """A client's scoreboard from its mp_frags lines since `since`: slot -> (name, frags)."""
    table = {}
    for l in inst.lines[since:]:
        # (anywhere in the line: an obituary printed without its newline runs into it, "playerHudsonmp_frags: ...")
        if "mp_frags:" in l:
            l = l[l.index("mp_frags:"):]
            f = fields(l)
            name = l.split('name="', 1)[1].split('"', 1)[0]
            if f["named"] == "1":
                table[int(f["slot"])] = (name, int(f["frags"]))
            else:
                table.pop(int(f["slot"]), None)
    return table


BOTGRAB_OFF = 14.0  # mp_botgrab: units the second grab is off the drawn weapon's middle (square to it): nothing taken


def botgrab_case(s, srv, a, who, ent, label):
    """(bots3) vr_dm_test 16 holds the victim (`who` 0 the bot, 1 B) still; A's client's drawing of its weapon (mp_drawnwpn,
    entity `ent`'s main hand) gives the grab's points: BOTGRAB_OFF units off it nothing is taken hold of (vr_dm_test 18),
    at its middle it is taken (17)."""
    for test, off in ((18, BOTGRAB_OFF), (17, 0.0)):
        since = srv.mark()
        since_a = a.mark()
        srv.cmd("vr_dm_test_who %d; vr_dm_test 16" % who)
        _, ready = a.wait(r"dmtest: hold: .*(ready|FAIL)", 30, since_a, "%s: the victim held" % label)
        if "FAIL" in ready:
            s.check("%s: the victim held (vr_dm_test 16)" % label, False, ready.strip())
            return

        def drawn(lines):
            return [avatar_fields(l) for l in lines[0] if l.startswith("mp_drawnwpn: ent=%d side=1 " % ent)]
        (la,), _ = reports_until(s, [a], drawn, "%s: A draws the victim's weapon" % label)
        w = drawn([la])[-1]
        butt, muzzle = w["butt"], w["muzzle"]
        mid = [(p + q) * 0.5 for p, q in zip(butt, muzzle)]
        axis = [q - p for p, q in zip(butt, muzzle)]
        side = [axis[1], -axis[0], 0.0]  # (level, square to the weapon)
        n = math.sqrt(sum(c * c for c in side)) or 1.0
        at = [m + c / n * off for m, c in zip(mid, side)] if any(side) else [mid[0], mid[1], mid[2] + off]
        since = srv.mark()
        srv.cmd("vr_dm_test_x %.2f; vr_dm_test_y %.2f; vr_dm_test_z %.2f; vr_dm_test %d" % (at[0], at[1], at[2], test))
        _, verdict = srv.wait(r"dmtest: \d+ passed, \d+ failed", 30, since, "%s: vr_dm_test %d's verdict" % (label, test))
        got = [l.strip() for l in srv.lines[since:] if "dmtest: " in l or "pdisarm: " in l]
        s.check("%s: %s (vr_dm_test %d; drawn butt %s, muzzle %s)" % (
            label, "a grab %g units off the drawn weapon takes nothing" % off if off else
            "a grab at the drawn weapon's middle takes it", test, butt, muzzle), " 0 failed" in verdict,
            "%s %s" % (verdict.strip(), got[-4:]))


def botgrab(s):
    """(bots3; his notes 2026-11-21: "if I try to grab their weapon where it visually is, nothing happens") The disarm
    grab at another's weapon where it is drawn: on a dedicated deathmatch server B is held 64 units in front of A (vr_dm_test 16), holding a
    shotgun out; then a bot held there (vr_dm_test 16). For each, A's client reports where it draws the victim's weapon
    (vr_mp_avatars' mp_drawnwpn), and the server closes A's empty hand there: taken (vr_dm_test 17); BOTGRAB_OFF units off
    it: nothing (18). The bot's weapon is placed by the server (view::placeHeld) and drawn there by every client."""
    srv = s.server(dedicated=True)
    srv.start()
    srv.cmd("coop 0; deathmatch 1; noexit 1; fraglimit 0; timelimit %d; skill 1; map %s; vr_mp_status" % (
        DM_TIMELIMIT, DM_MAP))
    srv.wait(r"mp_server: map=%s" % DM_MAP, 120, what="the server on %s" % DM_MAP)
    a = s.client("A").start()
    b = s.client("B").start()
    a.cmd(s.connect_cmd(a))
    _, ja = a.wait(r"mp_event: .*joined map=%s" % DM_MAP, 120, what="A joined %s" % DM_MAP)
    b.cmd(s.connect_cmd(b))
    _, jb = b.wait(r"mp_event: .*joined map=%s" % DM_MAP, 120, what="B joined %s" % DM_MAP)
    ea, eb = int(fields(ja)["self"]), int(fields(jb)["self"])
    a.cmd("name A; %s" % EMPTY_HAND)
    b.cmd("name B")
    # (his gun held out, his grip held: the test's hold gives him a shotgun, deathmatch having no test gives)
    b.cmd("%s; vr_mock_button main grip 1; vr_mock_button off grip 0" % HOLD_OUT)
    botgrab_case(s, srv, a, 1, eb, "player B")
    # a bot (FrikBot: impulse 100 on the server through A), held in front of A by the test
    since_a = a.mark()
    a.cmd("impulse 100")
    a.wait(r"mp_frags: .*named=1", 10, since_a, "the bot named at A")
    bots = []
    for _ in range(10):
        bots = sorted(slot for slot in frag_table(a) if slot not in (ea - 1, eb - 1))
        if bots:
            break
        since_c = a.mark()
        wait_quietly(a, r"mp_frags: ", 5, since_c, "the bot listed at A")
    s.check("A lists the bot", len(bots) == 1, "%s" % frag_table(a))
    if bots:
        botgrab_case(s, srv, a, 0, bots[0] + 1, "the bot")


def gear_until(s, inst, ent, what, tries=40):
    """vr_mp_gearcheck <ent> on `inst` until it lists that copy (posed; a definite end: at most `tries` reports) -> its
    "mp_gear:" fields, or None (a failed check)."""
    for _ in range(tries):
        since = inst.mark()
        inst.cmd("vr_mp_gearcheck %d 1" % ent)  # (1: each piece against the viewer's own, mp_gearpiece lines)
        n, _ = inst.wait(r"mp_gearcheck: \d+ players", 10, since, what)
        got = [avatar_fields(l) for l in inst.lines[since:n + 1] if l.startswith("mp_gear: ent=%d " % ent)]
        if got:
            got[-1]["pieces"] = [l.strip() for l in inst.lines[since:n + 1] if l.startswith("mp_gearpiece: ent=%d " % ent)]
            return got[-1]
    s.check(what, False, "not listed in %d reports" % tries)
    return None


def botgear_case(s, viewer, ent, label):
    """One copy as `viewer` draws it: its two hands and no other hand model (an empty holster draws no fists), its
    holstered weapons and sheaths on its trunk, its hands at its arms' wrists (vr_mp_gearcheck's limits)."""
    g = gear_until(s, viewer, ent, "%s: %s draws it" % (label, viewer.name))
    if g is None:
        return
    s.check("%s: %s draws two hands and no extra hand models, its holsters and sheaths on its body" % (label, viewer.name),
            g["ok"][0] == 1 and g["hands"][0] == 2,
            "hands %d, holsters %d, sheaths %d, farthest %.1f from the trunk (at most %.1f), hands %.1f from the wrists "
            "(at most %.1f)" % (g["hands"][0], g["holsters"][0], g["sheaths"][0], g["trunk"][0], g["trunkmost"][0],
                                g["wrist"][0], g["wristmost"][0]))
    # (vrnotes1122a; his voice note 2026-11-22: a bot's holsters "not properly rotated on its body") turned with its trunk
    s.check("%s: %s draws its holsters turned with its trunk (sheaths along its up and facing out, guns in them, all "
            "numbers)" % (label, viewer.name), g["turned"][0] == 1,
            "finite %d, tilt %.1f (at most 70), facing %.1f (at most 60), guns %.1f from their sheaths (at most %.1f)" % (
                g["finite"][0], g["tilt"][0], g["facing"][0], g["hang"][0], g["hangmost"][0]))
    # (fbmisc1123; his report 2026-11-23: guns floating off a copy's hips and chest, turned wrong, its chest sheaths out on
    # the shoulders) each holstered gun and sheath where the viewer's own sits on his body, the same gun in the same holster
    s.check("%s: %s draws its holstered guns and sheaths as its own sit on its body (within 7 cm and 20 degrees)" % (
        label, viewer.name), g["vsown"][0] >= 4 and g["asown"][0] == 1,
            "%d compared, farthest %.3f m, most turned %.1f degrees" % (g["vsown"][0], g["place"][0], g["axis"][0]))
    for p in g["pieces"]:
        s.note(p)
    return g


def botgear(s):
    """(mpbothands; his report 2026-11-22: in the multiplayer scenes every bot had an extra pair of fists, a gun and a shovel
    floating beside its body) The copies of other players as drawn: on a dedicated deathmatch server the flat-screen B is
    held in front of the VR A (vr_dm_test 16), then a bot (its holsters empty but a shotgun: the QC names the fists' model
    for an empty one). A draws each, and B draws A, with exactly their two hands (vr_mp_gearcheck: no hand model in a
    holster), the holsters and sheaths on the trunk and the hands at the wrists."""
    srv = s.server(dedicated=True)
    srv.start()
    srv.cmd("coop 0; deathmatch 1; noexit 1; fraglimit 0; timelimit %d; skill 1; map %s; vr_mp_status" % (
        DM_TIMELIMIT, DM_MAP))
    srv.wait(r"mp_server: map=%s" % DM_MAP, 120, what="the server on %s" % DM_MAP)
    a = s.client("A").start()
    b = s.client("B", flat=True).start()
    a.cmd(s.connect_cmd(a))
    _, ja = a.wait(r"mp_event: .*joined map=%s" % DM_MAP, 120, what="A joined %s" % DM_MAP)
    b.cmd(s.connect_cmd(b))
    _, jb = b.wait(r"mp_event: .*joined map=%s" % DM_MAP, 120, what="B joined %s" % DM_MAP)
    ea, eb = int(fields(ja)["self"]), int(fields(jb)["self"])
    a.cmd("name A")
    b.cmd("name B")
    since_a = a.mark()
    # (23: 16's hold, every holster of both filled, the same gun in the same holster: fbmisc1123)
    srv.cmd("vr_dm_test_who 1; vr_dm_test 23")
    _, ready = a.wait(r"dmtest: hold: .*(ready|FAIL)", 30, since_a, "player B held in front of A")
    s.check("player B held in front of A (vr_dm_test 23)", "FAIL" not in ready, ready.strip())
    gb = botgear_case(s, a, eb, "the flat player B")
    botgear_case(s, b, ea, "the VR player A")
    if os.environ.get("MP_BOTGEAR_SHOTS"):
        s.note("screenshot (B sees A's holsters): %s" % b.screenshot("botgear_A"))
    srv.cmd("vr_dm_test 0")
    # a bot (FrikBot: impulse 100 on the server through A), held in front of A by the test
    since_a = a.mark()
    a.cmd("impulse 100")
    a.wait(r"mp_frags: .*named=1", 10, since_a, "the bot named at A")
    bots = []
    for _ in range(10):
        bots = sorted(slot for slot in frag_table(a) if slot not in (ea - 1, eb - 1))
        if bots:
            break
        since_c = a.mark()
        wait_quietly(a, r"mp_frags: ", 5, since_c, "the bot listed at A")
    s.check("A lists the bot", len(bots) == 1, "%s" % frag_table(a))
    if not bots:
        return
    since_a = a.mark()
    srv.cmd("vr_dm_test_who 0; vr_dm_test 23")
    _, ready = a.wait(r"dmtest: hold: .*(ready|FAIL)", 30, since_a, "the bot held in front of A")
    s.check("the bot held in front of A (vr_dm_test 23)", "FAIL" not in ready, ready.strip())
    gbot = botgear_case(s, a, bots[0] + 1, "the bot")
    if gb is not None and gbot is not None:
        # (B's copy as his own client drew his holsters: what a bot's, placed by A's client, must look like)
        dt, df = abs(gbot["tilt"][0] - gb["tilt"][0]), abs(gbot["facing"][0] - gb["facing"][0])
        s.check("the bot's holsters lie as player B's (his client's own) do: tilt and facing within 20 degrees",
                dt <= 20 and df <= 20, "bot tilt %.1f facing %.1f, B's %.1f and %.1f" % (
                    gbot["tilt"][0], gbot["facing"][0], gb["tilt"][0], gb["facing"][0]))
    if os.environ.get("MP_BOTGEAR_SHOTS"):
        s.note("screenshot (A sees the bot's holsters): %s" % a.screenshot("botgear_bot"))
    srv.cmd("vr_dm_test 0")


GEARPIECE_RE = re.compile(r"mp_gearpiece: ent=\d+ holster=(\d+) (gun|sheath) at=\(([-\d.]+) ([-\d.]+) ([-\d.]+)\) "
                          r"own=\(([-\d.]+) ([-\d.]+) ([-\d.]+)\)")
# (A: hips 4.5 units further out, chest holsters 4.5 up; B: hips 4 down, chest 3.5 in: each pair a hand's width or so
# from the other's and from the defaults)
HOLSTERSYNC_PREFS = {"A": "vr_hip_offset_y 16; vr_upper_holster_offset_z 13",
                     "B": "vr_hip_offset_z -4; vr_upper_holster_offset_y 4"}
HOLSTERSYNC_DEFAULTS = "vr_hip_offset_y 11.5; vr_upper_holster_offset_z 8.5; vr_hip_offset_z 0; vr_upper_holster_offset_y 7.5"


def gear_pieces(g, which):
    """mp_gearpiece lines of a gear_until result -> {(holster, gun|sheath): (x, y, z)}: `which` "at" the copy's own
    piece in its body's frame, "own" the viewer's own in his."""
    out = {}
    for p in g["pieces"] if g else []:
        m = GEARPIECE_RE.match(p)
        if m:
            v = [float(x) for x in m.groups()[2:]]
            out[(int(m.group(1)), m.group(2))] = tuple(v[:3] if which == "at" else v[3:])
    return out


def holstersync_pair(s, viewer, owner, ev, eo, tries=20):
    """`viewer` draws `owner`'s copy with `owner`'s own holster places (his settings, sent in his calibration), not his
    own: each piece within 7 cm of where `owner`'s client has it on his own body (both in their bodies' frames:
    vr_mp_gearcheck's pieces), and farther than that from the viewer's own (his differ by design)."""
    label = "%s's copy at %s" % (owner.name, viewer.name)
    worst, common, gv = None, 0, None
    for _ in range(tries):  # (a definite end: at most `tries` report pairs, his calibration in by then)
        gv = gear_until(s, viewer, eo, "%s: drawn" % label)
        go = gear_until(s, owner, ev, "%s: the owner's own pieces" % label)
        if gv is None or go is None:
            return
        copy, own = gear_pieces(gv, "at"), gear_pieces(go, "own")
        keys = sorted(set(copy) & set(own))
        common = len(keys)
        worst = max((math.dist(copy[k], own[k]) for k in keys), default=None)
        if common >= 4 and worst is not None and worst <= 0.07:
            break
    s.check("%s: its holstered guns and sheaths where %s's own client has them on his body (within 7 cm)" % (
        label, owner.name), common >= 4 and worst is not None and worst <= 0.07,
            "%d pieces compared, farthest %s m" % (common, "none" if worst is None else "%.3f" % worst))
    s.check("%s: not where %s's own sit (his settings differ; before, every copy wore the viewer's): sameprefs 0, "
            "farthest from his own over 7 cm" % (label, viewer.name),
            gv["sameprefs"][0] == 0 and gv["place"][0] > 0.07,
            "sameprefs %d, farthest from the viewer's own %.3f m" % (gv["sameprefs"][0], gv["place"][0]))
    s.check("%s: on its body, turned with its trunk, two hands (vr_mp_gearcheck ok)" % label, gv["ok"][0] == 1,
            "hands %d trunk %.1f wrist %.1f tilt %.1f facing %.1f hang %.1f" % (
                gv["hands"][0], gv["trunk"][0], gv["wrist"][0], gv["tilt"][0], gv["facing"][0], gv["hang"][0]))
    s.metric("holstersync_%s_at_%s" % (owner.name, viewer.name), {"farthest_m": worst, "pieces": common,
                                                                   "vs_viewer_m": gv["place"][0]})


def variantowned(s):
    """(variantowned; his decision 2026-11-24) Dwell's weapon variants owned per player: on a dedicated coop server with
    A (VR) and B (flat), every player owns the Crystal Lance with his hands empty (vr_ad_test 233); B presses the weapon
    key 8 on his own client and the lightning gun the server gives him is the Lance, not a plain one; then each player
    respawned as coop does still owns it, the weapon key's gun the Lance (vr_ad_test 234, dwell/dw_test.qc)."""
    srv, a, b, ea, eb = join(s, flat="B", b_first="vr_flat_holsters 0")  # (B's weapon keys: off with his flat holsters on)
    since = srv.mark()
    srv.cmd("vr_ad_test 233")
    _, ready = srv.wait(r"dwowned_mp: ready", 20, since, "every player owning the Crystal Lance")
    s.check("every player owns the Crystal Lance, his hands empty", "2 players" in ready, ready.strip())
    since = srv.mark()
    b.cmd("impulse 8")
    srv.cmd("vr_ad_test 234")
    _, v = srv.wait(r"dwowned_mp: \d+ passed, \d+ failed", 60, since, "the variant checks' verdict")
    s.check("B's weapon key gives the Lance; a coop respawn keeps it owned", v.strip().endswith(", 0 failed"), v.strip())


def holstersync(s):
    """(holstersync; his words 2026-11-23: "Other players' holster placement should be synced in multiplayer, they should
    not all share the host's preferences") A (VR) and B (flat) each with holster settings of their own, on a dedicated
    deathmatch server, B held in front of A (vr_dm_test 23: every holster filled, the same gun in the same holster): each
    draws the other's copy with the other's own placement (his client's own pieces, within 7 cm), not his own. Then a bot
    held there: with A's own settings changed it wears the shipped ones, not A's (sameprefs 0, far from A's own); with A
    back at the defaults (vr_mp_holster_defaults: 0 differ) A's own (sameprefs 1, within 7 cm and 20 degrees)."""
    srv = s.server(dedicated=True)
    srv.start()
    srv.cmd("coop 0; deathmatch 1; noexit 1; fraglimit 0; timelimit %d; skill 1; map %s; vr_mp_status" % (
        DM_TIMELIMIT, DM_MAP))
    srv.wait(r"mp_server: map=%s" % DM_MAP, 120, what="the server on %s" % DM_MAP)
    a = s.client("A").start()
    b = s.client("B", flat=True).start()
    a.cmd(s.connect_cmd(a))
    _, ja = a.wait(r"mp_event: .*joined map=%s" % DM_MAP, 120, what="A joined %s" % DM_MAP)
    b.cmd(s.connect_cmd(b))
    _, jb = b.wait(r"mp_event: .*joined map=%s" % DM_MAP, 120, what="B joined %s" % DM_MAP)
    ea, eb = int(fields(ja)["self"]), int(fields(jb)["self"])
    a.cmd("name A; " + HOLSTERSYNC_PREFS["A"])
    b.cmd("name B; " + HOLSTERSYNC_PREFS["B"])
    since_a = a.mark()
    srv.cmd("vr_dm_test_who 1; vr_dm_test 23")
    _, ready = a.wait(r"dmtest: hold: .*(ready|FAIL)", 30, since_a, "player B held in front of A")
    s.check("player B held in front of A (vr_dm_test 23)", "FAIL" not in ready, ready.strip())
    holstersync_pair(s, a, b, ea, eb)
    holstersync_pair(s, b, a, eb, ea)
    if os.environ.get("MP_BOTGEAR_SHOTS"):
        s.note("screenshot (B sees A's holsters): %s" % b.screenshot("holstersync_A"))
    srv.cmd("vr_dm_test 0")
    since_a = a.mark()
    a.cmd("impulse 100")
    a.wait(r"mp_frags: .*named=1", 10, since_a, "the bot named at A")
    bots = []
    for _ in range(10):
        bots = sorted(slot for slot in frag_table(a) if slot not in (ea - 1, eb - 1))
        if bots:
            break
        since_c = a.mark()
        wait_quietly(a, r"mp_frags: ", 5, since_c, "the bot listed at A")
    s.check("A lists the bot", len(bots) == 1, "%s" % frag_table(a))
    if not bots:
        return
    since_a = a.mark()
    srv.cmd("vr_dm_test_who 0; vr_dm_test 23")
    _, ready = a.wait(r"dmtest: hold: .*(ready|FAIL)", 30, since_a, "the bot held in front of A")
    s.check("the bot held in front of A (vr_dm_test 23)", "FAIL" not in ready, ready.strip())
    g = gear_until(s, a, bots[0] + 1, "the bot: A draws it")
    if g is not None:
        s.check("the bot's holsters wear the shipped settings, not A's own (sameprefs 0, farthest from A's over 7 cm)",
                g["sameprefs"][0] == 0 and g["place"][0] > 0.07 and g["ok"][0] == 1,
                "sameprefs %d, farthest from A's own %.3f m, ok %d" % (g["sameprefs"][0], g["place"][0], g["ok"][0]))
    since_a = a.mark()
    a.cmd(HOLSTERSYNC_DEFAULTS + "; vr_mp_holster_defaults")
    _, line = a.wait(r"mp_holster_defaults: ", 10, since_a, "A's holsters against the shipped table")
    s.check("A at the shipped settings: his holsters as the shipped table (vr_mp_holster_defaults: 0 differ)",
            line.strip().startswith("mp_holster_defaults: 0 of"), line.strip())
    g = gear_until(s, a, bots[0] + 1, "the bot: A at the defaults draws it")
    if g is not None:
        s.check("A at the shipped settings: the bot's holsters as his own (sameprefs 1, within 7 cm and 20 degrees)",
                g["sameprefs"][0] == 1 and g["asown"][0] == 1 and g["vsown"][0] >= 4 and g["ok"][0] == 1,
                "sameprefs %d, %d compared, farthest %.3f m, most turned %.1f" % (
                    g["sameprefs"][0], g["vsown"][0], g["place"][0], g["axis"][0]))
    srv.cmd("vr_dm_test 0")


def botrevolver(s):
    """(vrnotes1122a; his voice note 2026-11-22: a bot wielding the heavy revolver aimed at him and never shot) On a
    dedicated deathmatch server A adds a bot; the server's QC test vr_dm_test 14 arms it: a revolver of its own and shells,
    drawn and fired at A (three slugs: its trigger let go between them); then a revolver it took loaded (by a disarm) in
    its main hand with no shells of its own, and in its off hand beside its shotgun: three slugs each (a bot never
    reloads: a taken gun's clip goes with its ammo, frikbot/bot_fight.qc bot_clips_to_ammo)."""
    srv = s.server(dedicated=True)
    srv.start()
    srv.cmd("coop 0; deathmatch 1; noexit 1; fraglimit 0; timelimit %d; skill 1; map %s; vr_mp_status" % (
        DM_TIMELIMIT, DM_MAP))
    srv.wait(r"mp_server: map=%s" % DM_MAP, 120, what="the server on %s" % DM_MAP)
    a = s.client("A").start()
    a.cmd(s.connect_cmd(a))
    _, ja = a.wait(r"mp_event: .*joined map=%s" % DM_MAP, 120, what="A joined %s" % DM_MAP)
    ea = int(fields(ja)["self"])
    a.cmd("name A")
    since_a = a.mark()
    a.cmd("impulse 100")
    a.wait(r"mp_frags: .*named=1", 10, since_a, "the bot named at A")
    bots = []
    for _ in range(10):
        bots = sorted(slot for slot in frag_table(a) if slot != ea - 1)
        if bots:
            break
        since_c = a.mark()
        wait_quietly(a, r"mp_frags: ", 5, since_c, "the bot listed at A")
    s.check("A lists the bot", len(bots) == 1, "%s" % frag_table(a))
    if not bots:
        return
    since_srv = srv.mark()
    srv.cmd("vr_dm_test 14")
    _, verdict = srv.wait(r"dmtest: \d+ passed, \d+ failed", 90, since_srv, "vr_dm_test 14's verdict")
    got = [l.strip() for l in srv.lines[since_srv:] if "dmtest: " in l]
    for l in got:
        if "PASS" in l or "FAIL" in l:
            s.note(l)
    s.check("the bot fires the revolver at A: its own, and one it took loaded in either hand (vr_dm_test 14)",
            " 0 failed" in verdict and "FAIL" not in " ".join(got), "%s %s" % (verdict.strip(), got[-4:]))


def debugviz_case(s, srv, a, who, ent, label, host):
    """(bots4) vr_dm_test 16 holds the victim (`who` 0 the bot, 1 B) in front of A; A's mp_debugviz line for entity `ent`:
    his disarm volumes (the host's from its QC, a client's rebuilt from the weapon it draws) along his drawn weapon, and
    his five hit zones with the head zone at his drawn head."""
    since_a = a.mark()
    srv.cmd("vr_dm_test_who %d; vr_dm_test 16" % who)
    _, ready = a.wait(r"dmtest: hold: .*(ready|FAIL)", 30, since_a, "%s: the victim held" % label)
    if "FAIL" in ready:
        s.check("%s: the victim held (vr_dm_test 16)" % label, False, ready.strip())
        return

    def pick(lines, prefix):
        return [avatar_fields(l) for l in lines[0] if l.startswith(prefix)]

    def done(lines):
        return pick(lines, "mp_debugviz: ent=%d " % ent) and pick(lines, "mp_drawnwpn: ent=%d side=1 " % ent) and \
            pick(lines, "mp_avatar: ent=%d " % ent)
    got = reports_until(s, [a], done, "%s: A reports his debug views" % label)
    if not got:
        return
    (la,), _ = got
    srv.cmd("vr_dm_test 0")  # (the hold let go: the next case's vr_dm_test 16 picks its victim afresh)
    v = pick([la], "mp_debugviz: ent=%d " % ent)[-1]
    w = pick([la], "mp_drawnwpn: ent=%d side=1 " % ent)[-1]
    av = pick([la], "mp_avatar: ent=%d " % ent)[-1]
    # (the grab capsule's line against the drawn weapon's butt and muzzle: a client's is the drawn line itself; a host's
    # the server's (VR_Parry_WeaponLine: the butt and the melee's far end), within a few units of it)
    off = max(math.dist(v["grab0"], w["butt"]), math.dist(v["grab1"], w["muzzle"]))
    most = 0.5 if not host else DEBUGVIZ_HOST_OFF
    s.check("%s: A draws his disarm volumes along the weapon he draws (%s)" % (label, "the host's" if host else
            "a client's rebuild"), v["volumes"][0] >= 2 and off <= most,
            "volumes %d, grab line %s..%s, drawn %s..%s, %.1f units off (at most %g)" % (
                v["volumes"][0], v["grab0"], v["grab1"], w["butt"], w["muzzle"], off, most))
    head_off = math.dist(v["head"], av["head"])
    # (hitzones) His body's shape as shots meet it (vr_playerhit.cpp): the head, the torso, two capsules an arm and a leg
    s.check("%s: A draws his ten hit zones, the head's at his drawn head" % label,
            v["zones"][0] == 10 and head_off <= DEBUGVIZ_HEAD_OFF,
            "zones %d, head zone %s, drawn head %s, %.1f units apart (at most %g)" % (
                v["zones"][0], v["head"], av["head"], head_off, DEBUGVIZ_HEAD_OFF))


DEBUGVIZ_HOST_OFF = 4.0  # mp_debugviz: units the host's grab line (the server's) may be off the weapon A draws
DEBUGVIZ_HEAD_OFF = 4.5  # ... units the head zone's middle may be off his drawn eyes (the skull's middle: 2.5 behind, 1 up)


def debugviz(s, listen):
    """(bots4; his note: "The show disarm volumes debug view doesn't seem to work on bots", "being able to see their
    hitboxes would be great ... make sure it also works with bots and players") Show Disarm Volumes and Show Hit Zones on
    a deathmatch with B and a bot, each held in front of A (vr_dm_test 16): on a dedicated server A is a client and
    rebuilds them (debugviz); A hosting (debugviz_listen) draws the server's own. A's vr_mp_avatars' mp_debugviz lines."""
    rules = "vr_player_disarm_pvp 1; vr_player_disarm_grab 1; vr_player_disarm 1"
    level = "coop 0; deathmatch 1; noexit 1; fraglimit 0; timelimit %d; skill 1; %s; map %s; vr_mp_status" % (
        DM_TIMELIMIT, rules, DM_MAP)
    if listen:
        a = srv = s.server(dedicated=False, name="A")
        srv.start()
        srv.cmd("disconnect; maxplayers 4; listen 1; " + level)
    else:
        srv = s.server(dedicated=True)
        srv.start()
        srv.cmd(level)
    srv.wait(r"mp_server: map=%s" % DM_MAP, 120, what="the server on %s" % DM_MAP)
    if not listen:
        a = s.client("A").start()
    b = s.client("B").start()
    if not listen:
        a.cmd(s.connect_cmd(a))
    _, ja = a.wait(r"mp_event: .*joined map=%s" % DM_MAP, 120, what="A joined %s" % DM_MAP)
    b.cmd(s.connect_cmd(b))
    _, jb = b.wait(r"mp_event: .*joined map=%s" % DM_MAP, 120, what="B joined %s" % DM_MAP)
    ea, eb = int(fields(ja)["self"]), int(fields(jb)["self"])
    a.cmd("name A; %s; vr_debug_disarm 1; vr_debug_hitzones 1" % EMPTY_HAND)
    b.cmd("name B; %s; vr_mock_button main grip 1; vr_mock_button off grip 0" % HOLD_OUT)
    debugviz_case(s, srv, a, 1, eb, "player B", listen)
    since_a = a.mark()
    a.cmd("impulse 100")
    a.wait(r"mp_frags: .*named=1", 10, since_a, "the bot named at A")
    bots = []
    for _ in range(10):
        bots = sorted(slot for slot in frag_table(a) if slot not in (ea - 1, eb - 1))
        if bots:
            break
        since_c = a.mark()
        wait_quietly(a, r"mp_frags: ", 5, since_c, "the bot listed at A")
    s.check("A lists the bot", len(bots) == 1, "%s" % frag_table(a))
    if bots:
        # (mpopen: the host's rare miss, 55 units off once in a suite, is the bot's AI swinging at A while held: the
        # server's line and the drawn one read a frame apart. vr_bot_freeze stills it but stops the bot's drawn weapon
        # (no mp_drawnwpn side=1 at a listen host: the case never completes), so it stays a known flake)
        debugviz_case(s, srv, a, 0, bots[0] + 1, "the bot", listen)


def deathmatch(s):
    """(B11) A dedicated deathmatch server (deathmatch 1, fraglimit 3) on dm4: A and B (B through the proxy) join at
    deathmatch starts; A adds two FrikBot bots (impulse 100): both clients list four players, the bots move;
    A is killed by a bot (the server's vr_hurt with the bot as attacker) and his trigger respawns him; then
    A's kills of the bots (vr_hurt, A the attacker) reach the fraglimit (B14c: the bots' frags go down in the lava):
    the intermission at both clients, their scoreboards the same."""
    srv = s.server(dedicated=True)
    srv.start()
    # (B25: no fraglimit until A's frags: the bots play by themselves meanwhile and once reached it in the QC tests' steps
    # under load -- the intermission, the level again, the bots gone: dmtest 7 and 8 found no bot, 7 checks failed. The
    # QC's StartFrame reads fraglimit every frame: set below, before A's kills.) (noexit 1: and dm4's exit kills, not
    # changes the level: a bot that wandered into it had the level start again, the bots gone, in the same steps)
    srv.cmd("coop 0; deathmatch 1; noexit 1; fraglimit 0; timelimit %d; skill 1; map %s; vr_mp_status" % (
        DM_TIMELIMIT, DM_MAP))
    srv.wait(r"mp_server: map=%s" % DM_MAP, 120, what="the server on %s" % DM_MAP)
    a = s.client("A").start()
    b = s.client("B", proxy=True).start()
    # (A first, B once A is in: the server's QC tests take the first human in the players' list as theirs, and mp_test
    # poses A's hands -- B16's suite run had B connect first under the load, its crowbar falling from his open hand in
    # vr_dm_test 3, his shotgun out of the bot's reach in 6)
    a.cmd(s.connect_cmd(a))
    _, ja = a.wait(r"mp_event: .*joined map=%s" % DM_MAP, 120, what="A joined %s" % DM_MAP)
    b.cmd(s.connect_cmd(b))
    _, jb = b.wait(r"mp_event: .*joined map=%s" % DM_MAP, 120, what="B joined %s" % DM_MAP)
    ea, eb = int(fields(ja)["self"]), int(fields(jb)["self"])
    _, sa = a.wait(r"mp_seen: .*self=1", 20, what="A seeing himself")
    _, sb = b.wait(r"mp_seen: .*self=1", 20, what="B seeing himself")
    oa, ob = sa.split("org=")[1].strip(), sb.split("org=")[1].strip()
    s.check("A and B joined the deathmatch at two starts, A first (the QC tests' human)", ea < eb and oa != ob, "A (%d) at %s, B (%d) at %s" % (
        ea, oa, eb, ob))

    # two bots (FrikBot: impulse 100 on the server through A)
    since_a, since_b = a.mark(), b.mark()
    a.cmd("impulse 100")
    a.wait(r"mp_frags: .*named=1", 10, since_a, "the first bot named at A")
    since_a2 = a.mark()
    a.cmd("impulse 100")
    a.wait(r"mp_frags: .*named=1", 10, since_a2, "the second bot named at A")
    b.wait(r"mp_frags: .*named=1", 10, since_b, "the bots named at B")
    # (B19: B's first bot's name was waited for, the second's not: B listed three once in a suite run; up to 10
    # more of B's name lines, until he lists four)
    # (B25: A's too: under load A listed three, a bot's name not yet his ({3: 'FrikBot'}, slot 2 missing), and the
    # bots' list came out short)
    for c in (a, b):
        for _ in range(10):
            if len(frag_table(c)) >= 4 and all(n != "FrikBot" for n, _ in frag_table(c).values()):
                break
            since_c = c.mark()
            if not wait_quietly(c, r"mp_frags: .*named=1", 5, since_c, "another name at %s" % c.name):
                break
    ta, tb = frag_table(a), frag_table(b)
    bots = sorted(slot for slot in ta if slot not in (ea - 1, eb - 1))
    s.check("both clients list four players (A, B, two bots)", len(ta) == 4 and len(tb) == 4 and len(bots) == 2,
            "A: %s; B: %s" % (ta, tb))
    # (B11 part 2) deathmatch's rules with VR weapons and bots, the server's QC test (vr_dmrules_test.qc): a weapon
    # pickup taken and back (dm 1) or staying (dm 2), the bot's let-go weapon removed; disarms between A and a bot (PvP on,
    # PvP off, PvE off by default); the bot's shot knocks A's weapon out: his line at his client. At once (two bots on dm4
    # may reach the fraglimit by themselves in under a minute)
    since_t, since_ts, since_tb = a.mark(), srv.mark(), b.mark()
    srv.cmd("vr_dm_test 1")
    _, verdict = srv.wait(r"dmtest: \d+ passed, \d+ failed", 60, since_ts, "the deathmatch rules' test's verdict")
    a.wait(r"dmtest: ", 5, since_t, "its lines at A") # (its checks broadcast; the verdict echoed on the server's console)
    a.mark()
    fails = [l for l in a.lines[since_t:] if l.startswith("dmtest: FAIL")]
    s.check("the deathmatch rules' test (items, PvP and PvE disarms with a bot) passes", " 0 failed" in verdict,
            "%s %s" % (verdict.strip(), fails))
    # (the test's human is the first one alive: B when a bot's shot had just killed A)
    told = (wait_or_none(a, r"Your weapon is knocked out of your hand", 10, since_t) or
            wait_or_none(b, r"Your weapon is knocked out of your hand", 1, since_tb))
    s.check("the test's human is told his weapon was knocked out (the bot's shot)", told is not None, "%s" % (told,))

    # (B11 part 3, part 3) a bot's sword swing at A, its hand swinging it across its front (bot_setvrfields), its blade's
    # line read by the directional parry as a player's swing: A's crowbar (given by the test) held low at his side, the
    # blow lands (vr_dm_test 2); held level across its way, parried (vr_dm_test 3). (MP bots phase 1: the bot's swing is a
    # flat-screen player's overhead swing, its blow the melee's sweep.) The bot walks through the players'
    # moves meanwhile (vr_pmove_bots).
    # (his grip held, as a given weapon held through a pose's change wants: the test empties his hand first, then gives
    # the crowbar into the gripping hand, grip_then_give's order)
    srv.cmd("vr_debug_carry 2; developer 1; vr_debug_shots 1") # (the grip's, the parry's and the blow's lines)
    s.check("A alive for the bot's swings (respawned if a bot's own shot had killed him)", respawn_if_dead(srv, a, ea), "")
    since_g = srv.mark()
    a.cmd("vr_mock_button main grip 1; %s" % EMPTY_HAND)
    srv.wait(r"grip: .*main hand pressed at", 10, since_g, "A's main grip on the server")
    for test, pose, what in ((2, EMPTY_HAND, "at his side: the bot's swing lands"),
                             (3, LEVEL_GUARD, "level across its way: the bot's overhead swing parried")):
        a.cmd(pose)
        since_p = srv.mark()
        srv.cmd("vr_dm_test %d" % test)
        _, verdict = srv.wait(r"dmtest: \d+ passed, \d+ failed", 30, since_p, "vr_dm_test %d's verdict" % test)
        got = [l.strip() for l in srv.lines[since_p:] if "dmtest: FAIL" in l or "dirparry: " in l or "parry: " in l or
               "by a melee blow" in l]
        s.check("A's crowbar %s (vr_dm_test %d)" % (what, test), " 0 failed" in verdict,
                "%s %s" % (verdict.strip(), got[-4:]))
    # (B11 part 4) a bot's off hand as a player's, its AI deciding (bot_offhand_ai), its hand through the players' code:
    # it punches A 40 units ahead (its fist's blow through the melee's sweep: vr_dm_test 4), takes a nailgun lying in
    # front of it by its hand's touch and grip (5), and with PvP disarms on closes its hand on A's shotgun (given into his
    # gripping hand) and pulls it away (6).
    a.cmd(EMPTY_HAND)
    for test, what in ((4, "the bot punches A: its fist's blow lands through the melee's sweep"),
                       (5, "the bot's hand takes a nailgun lying in front of it"),
                       (6, "the bot's hand closes on A's shotgun and pulls it away")):
        since_p = srv.mark()
        srv.cmd("vr_dm_test %d" % test)
        _, verdict = srv.wait(r"dmtest: \d+ passed, \d+ failed", 30, since_p, "vr_dm_test %d's verdict" % test)
        got = [l.strip() for l in srv.lines[since_p:] if "dmtest: " in l or "pdisarm: " in l or "weapon: " in l]
        s.check("%s (vr_dm_test %d)" % (what, test), " 0 failed" in verdict, "%s %s" % (verdict.strip(), got[-6:]))
    a.cmd("vr_mock_button main grip 0; %s" % EMPTY_HAND)
    # (B13b) deathmatch's caps on loose gibs and dropped weapons, the oldest first, none out of deathmatch (7); a respawn
    # with every start taken comes in beside one, nobody telefragged (8)
    for test, what in ((7, "deathmatch's caps take the oldest gibs and weapons, none out of deathmatch"),
                       (8, "a respawn with every start taken comes in beside one, nobody telefragged")):
        since_p = srv.mark()
        srv.cmd("vr_dm_test %d" % test)
        _, verdict = srv.wait(r"dmtest: \d+ passed, \d+ failed", 30, since_p, "vr_dm_test %d's verdict" % test)
        got = [l.strip() for l in srv.lines[since_p:] if "dmtest: " in l]
        s.check("%s (vr_dm_test %d)" % (what, test), " 0 failed" in verdict, "%s %s" % (verdict.strip(), got[-5:]))
    # (MP bots phase 1) the bots' hands as a flat-screen player's (vr_flatplay.cpp poseBot): a bot at skill 3 with
    # vr_bot_parry_chance 100 raises the flat guard against the other bot's overhead sword swing (parried), at chance 0
    # the blow lands (9); a bot holding a nailgun and a super nailgun of its own takes the second gun into its off hand
    # and fires both at A (10)
    for test, what in ((9, "a bot parries the other bot's timed overhead blow (its chance 100), the blow lands at 0"),
                       (10, "a bot dual wields a nailgun and a super nailgun and fires both at A")):
        since_p = srv.mark()
        srv.cmd("vr_dm_test %d" % test)
        _, verdict = srv.wait(r"dmtest: \d+ passed, \d+ failed", 30, since_p, "vr_dm_test %d's verdict" % test)
        got = [l.strip() for l in srv.lines[since_p:] if "dmtest: " in l or "botparry: " in l or "botdual: " in l]
        s.check("%s (vr_dm_test %d)" % (what, test), " 0 failed" in verdict, "%s %s" % (verdict.strip(), got[-6:]))
    srv.cmd("vr_debug_carry 0; developer 0; vr_debug_shots 0")

    # (seen by A or by B: B's start may see neither bot for a while, out of his sight)
    moving = [wait_or_none(a, r"mp_move: .*ent=%d self=0" % (slot + 1), 30, since_a) or
              wait_or_none(b, r"mp_move: .*ent=%d self=0" % (slot + 1), 5, since_b) for slot in bots]
    s.check("the bots move (A or B sees each move off)", all(moving), "%s" % moving)

    # (B11 part 3) the bots' bodies: no client draws a bot's hands, so the server sends its controllers and weapons and
    # each client places them by its own hands' code (view::solveHands: setupWeapon, setupHand), then poses its body on
    # them as any player's (seen by A or B, whichever has a bot in sight first)
    solved, posed = None, None
    for _ in range(40):
        for c in (a, b):
            since_s = c.mark()
            c.cmd("vr_mp_avatars")
            c.wait(r"mp_avatar_own:", 5, since_s, "an avatars report")
            for l in c.lines[since_s:]:
                m = re.match(r"mp_solved: ent=(\d+) hand0=(\d) gap0=(\S+) wpn0=(\S+) grip0=(\S+) hand1=(\d) gap1=(\S+) "
                             r"wpn1=(\S+) grip1=(\S+)", l)
                if m and int(m.group(1)) - 1 in bots and m.group(6) == "1" and m.group(8) != "-":
                    solved = solved or l.strip()
                    body = body_of(c.lines[since_s:], "mp_body").get(int(m.group(1)))
                    posed = posed or body
        if solved and posed:
            break
    s.metric("dm_bot_hands", solved)
    g = re.match(r"mp_solved: ent=\d+ hand0=(\d) gap0=(\S+) wpn0=\S+ grip0=\S+ hand1=(\d) gap1=(\S+) wpn1=(\S+) "
                 r"grip1=(\S+)", solved or "")
    s.check("a bot's hands placed by the client's own code: both jointed hands drawn, its gun in its main hand at its "
            "controller", bool(g) and g.group(1) == "1" and g.group(3) == "1" and float(g.group(2)) < BOT_HAND_GAP and
            float(g.group(4)) < BOT_HAND_GAP and float(g.group(6)) < BOT_GUN_GRIP, "%s" % solved)
    s.check("its body posed on them (mp_body, as any player's)", bool(posed), "%s" % (posed and posed.get("model")))

    # A is killed by the first bot (the server's vr_hurt with the bot as attacker: a real death, QC's ClientObituary
    # giving the bot his frag) and his trigger brings him back (PlayerDeathThink: the buttons up, then one down), until
    # the bot's frags (and any the bots score by themselves) reach the fraglimit: the intermission
    killer = bots[0]
    kills, respawns, first = 0, 0, None
    for _ in range(2):
        if a.find(r"mp_event: .*intermission=[1-9]", since_a):
            break
        since_k = a.mark()
        # (A already dead, killed by a bot's own shot and not yet respawned: that death is this round's)
        events = [i for i in range(since_a, since_k) if re.search(r"mp_event: .* (died|respawned)", a.lines[i])]
        if events and " died" in a.lines[events[-1]]:
            since_k = events[-1]
        else:
            srv.cmd("vr_hurt 1000 %d %d" % (ea - 1, killer + 1))
        _, died = a.wait(r"mp_event: .* (died|intermission=[1-9])", 20, since_k, "A killed by the bot")
        if "died" not in died:
            break
        kills += 1
        first = first or died
        if kills == 1:
            # (a frag up from the bot's count before: its own suicides may have put it below 0)
            credited = dm_frag_gained(b, (bots[0], bots[1]), since_b, 10)
            s.check("A's death by the bot credits the bot a frag at B", credited is not None, "%s" % credited)
        respawned = None
        for _ in range(30):
            if a.find(r"mp_event: .*intermission=[1-9]", since_k):
                break
            a.cmd("vr_mock_button main trigger 1")
            respawned = wait_or_none(a, r"mp_event: .* respawned", 0.5, since_k)
            a.cmd("vr_mock_button main trigger 0")
            respawned = respawned or wait_or_none(a, r"mp_event: .* respawned", 0.5, since_k)
            if respawned:
                respawns += 1
                break
    s.check("A killed by the bot, then his trigger respawns him", kills >= 1 and respawns >= 1,
            "%d deaths, %d respawns; the first at %s" % (kills, respawns, first.split("org=")[1] if first else "-"))

    # (B14c) A frags the bots (the server's vr_hurt with A as attacker) until his frags reach the fraglimit: the bots'
    # own frags can't get there for sure, as dm4's lava takes one off a bot every few seconds (FrikBot's falls, B14b's
    # failing runs: 17 of them in a minute against the test's 5 kills)
    # (B25: the fraglimit set now, above everyone's frags so far: A's kills reach it, not a bot's frags at once)
    limit = max([DM_FRAGLIMIT] + [f + 1 for _, f in frag_table(a).values()])
    srv.cmd("fraglimit %d" % limit)
    server_time_past(srv, server_time(srv) + 0.1)
    a_kills = 0
    for i in range(limit * 3):
        if a.find(r"mp_event: .*intermission=[1-9]", since_a):
            break
        mine = frag_table(a).get(ea - 1, ("", 0))[1]
        if mine >= limit:
            break
        since_f = a.mark()
        srv.cmd("vr_hurt 1000 %d %d" % (bots[i % 2], ea))  # (a bot dead in the lava now: the other one next)
        if wait_or_none(a, r"mp_frags: .*slot=%d named=1 .*frags=%d" % (ea - 1, mine + 1), 3, since_f):
            a_kills += 1
    s.check("A frags the bots (vr_hurt, A the attacker): his frags count them", a_kills >= 1, "%d kills" % a_kills)

    # the fraglimit ends the match
    _, ia = a.wait(r"mp_event: .*intermission=[1-9]", 30, since_a, "the intermission at A")
    _, ib = b.wait(r"mp_event: .*intermission=[1-9]", 30, since_b, "the intermission at B")
    ta, tb = frag_table(a), frag_table(b)
    top = max(f for _, f in ta.values()) if ta else 0
    s.metric("dm_frags", {"A": ta, "B": tb})
    s.check("the fraglimit (%d) reached: the intermission at A and B" % limit, top >= limit,
            "A's scoreboard %s" % ta)
    s.check("A's and B's scoreboards agree", ta == tb, "A %s; B %s" % (ta, tb))
    # (a QC ASSERT is a bprint: the clients see it, the dedicated server's log does not; until 2026-11-20 every run's
    # bot nailgun loop ran into its sword: getCurrentAmmoCount, weapons.qc)
    asserts = [i for i, l in enumerate(a.lines) if l.startswith("Assertion failure")]
    s.check("no QC assertion in the match (A's log)", not asserts,
            "%s" % [" ".join(x.strip() for x in a.lines[i:i + 3]) for i in asserts[:2]])


def wrist_rows(inst, what):
    """vr_gadget_scores at `inst`: the wrist gadget's scoreboard page's rows as it draws them -> (its head line's fields,
    [(slot, name, frags, you)] in the page's order). A definite end: the head line, then its last row's line."""
    since = inst.mark()
    inst.cmd("vr_gadget_scores")
    _, head = inst.wait(r"wristscore: page=", 20, since, what)
    hf = fields(head)
    n = int(hf["rows"])
    if n:
        inst.wait(r"wristscore: row=%d " % (n - 1), 10, since, what)
    inst.mark()
    rows = []
    for l in inst.lines[since:]:
        if "wristscore: row=" in l:
            l = l[l.index("wristscore: row="):]
            f = fields(l)
            rows.append((int(f["slot"]), l.split('name="', 1)[1].split('"', 1)[0], int(f["frags"]), f["you"] == "1"))
    return hf, rows


def wrist_rows_agree(inst, own_slot, what, tries=6):
    """The page's rows at `inst` against its own scoreboard (mp_frags): the same players and frags, in the scoreboard's
    order (the most frags first), `own_slot` marked as him -> (ok, detail). Again (up to `tries`) when a frag between the
    two (a bot's own death in dm4's lava) made them differ."""
    detail = ""
    for _ in range(tries):
        hf, rows = wrist_rows(inst, what)
        table = frag_table(inst)
        same = sorted((slot, name, frags) for slot, name, frags, _ in rows) == sorted(
            (slot, name, frags) for slot, (name, frags) in table.items())
        ordered = all(rows[i][2] >= rows[i + 1][2] for i in range(len(rows) - 1))
        you = [slot for slot, _, _, y in rows if y] == [own_slot]
        detail = "page %s; rows %s; scoreboard %s" % (hf, rows, table)
        if same and ordered and you:
            return True, detail
    return False, detail


def wristscore(s):
    """(wristscore) The wrist gadget's multiplayer scoreboard page (vr_gadget_scores.inc; MULTIPLAYER.md, "The scoreboard
    on the wrist"). A alone on e1m1 first (single player): his gadget's pages never reach the scoreboard (vr_gadget_page
    three times) and vr_gadget_scores lists nobody. Then a dedicated deathmatch server on dm4 (no frag limit): A and B
    join, A adds a FrikBot bot (impulse 100) and kills it (the server's vr_hurt, A the attacker): at A and at B the page's
    rows (vr_gadget_scores: the rows it draws) are the three players with the frags of the client's own scoreboard
    (mp_frags), A's kill counted, in the scoreboard's order, each client marked as himself; A's gadget turned to the page
    (vr_gadget_page) shows it, its image dumped (vr_gadget_screen_dump: <out>/A_wristscore.png)."""
    a = s.client("A").start()
    since = a.mark()
    a.cmd("disconnect; maxplayers 1; coop 0; deathmatch 0; map e1m1")
    a.wait(r"mp_event: .*joined map=e1m1 .*maxclients=1", 120, since, "A alone on e1m1")
    since = a.mark()
    a.cmd("vr_gadget_page 1; vr_gadget_page 1; vr_gadget_page 1")
    a.wait(r"gadget page: ", 10, since, "A's pages")
    while len([l for l in a.lines[since:] if "gadget page: " in l]) < 3:
        a.wait(r"gadget page: ", 10, a.mark(), "A's three pages")
    pages = [l.split("gadget page: ", 1)[1].split()[0] for l in a.lines[since:] if "gadget page: " in l]
    hf, rows = wrist_rows(a, "A's page in single player")
    s.check("single player: no scoreboard page (the pages never reach it, it lists nobody)",
            "2" not in pages and hf["available"] == "0" and not rows, "pages %s; %s" % (pages, hf))

    srv = s.server(dedicated=True)
    srv.start()
    srv.cmd("coop 0; deathmatch 1; noexit 1; fraglimit 0; timelimit %d; skill 1; map %s; vr_mp_status" % (
        DM_TIMELIMIT, DM_MAP))
    srv.wait(r"mp_server: map=%s" % DM_MAP, 120, what="the server on %s" % DM_MAP)
    b = s.client("B", proxy=True).start()
    since_a = a.mark()
    a.cmd(s.connect_cmd(a))
    _, ja = a.wait(r"mp_event: .*joined map=%s" % DM_MAP, 120, since_a, "A joined %s" % DM_MAP)
    b.cmd(s.connect_cmd(b))
    _, jb = b.wait(r"mp_event: .*joined map=%s" % DM_MAP, 120, what="B joined %s" % DM_MAP)
    ea, eb = int(fields(ja)["self"]), int(fields(jb)["self"])
    since_a, since_b = a.mark(), b.mark()
    a.cmd("impulse 100")
    a.wait(r"mp_frags: .*named=1", 10, since_a, "the bot named at A")
    b.wait(r"mp_frags: .*named=1", 10, since_b, "the bot named at B")
    for c in (a, b):  # (the bot's own name, not FrikBot's placeholder: as deathmatch's)
        for _ in range(10):
            if len(frag_table(c)) >= 3 and all(n != "FrikBot" for n, _ in frag_table(c).values()):
                break
            if not wait_quietly(c, r"mp_frags: .*named=1", 5, c.mark(), "another name at %s" % c.name):
                break
    ta, tb = frag_table(a), frag_table(b)
    bots = sorted(slot for slot in ta if slot not in (ea - 1, eb - 1))
    s.check("both clients list three players (A, B, a bot)", len(ta) == 3 and len(tb) == 3 and len(bots) == 1,
            "A: %s; B: %s" % (ta, tb))
    if len(bots) != 1:
        return

    # A kills the bot (vr_hurt: the bot's slot, A the attacker): his frag at both clients
    mine = ta.get(ea - 1, ("", 0))[1]
    since_a, since_b = a.mark(), b.mark()
    srv.cmd("vr_hurt 1000 %d %d" % (bots[0], ea))
    got_a = wait_or_none(a, r"mp_frags: .*slot=%d named=1 .*frags=%d" % (ea - 1, mine + 1), 10, since_a)
    got_b = wait_or_none(b, r"mp_frags: .*slot=%d named=1 .*frags=%d" % (ea - 1, mine + 1), 10, since_b)
    s.check("A kills the bot: his frag (%d) at A and B" % (mine + 1), got_a is not None and got_b is not None,
            "A: %s; B: %s" % (got_a, got_b))
    for c, own in ((a, ea - 1), (b, eb - 1)):
        ok, detail = wrist_rows_agree(c, own, "%s's page" % c.name)
        s.check("%s's wrist page lists the three with the scoreboard's frags, in its order, him marked" % c.name, ok,
                detail)

    # A's gadget turned to the page (the outer button's turn: past AD's page if its data is loaded)
    turned = None
    for _ in range(3):
        since_a = a.mark()
        a.cmd("vr_gadget_page 1")
        _, line = a.wait(r"gadget page: ", 10, since_a, "A's next page")
        if line.split("gadget page: ", 1)[1].startswith("2"):
            turned = line
            break
    hf, rows = wrist_rows(a, "A's page shown")  # (a later frame: the page drawn before the dump)
    s.check("A's gadget turned to the scoreboard page shows it", turned is not None and hf["shown"] == "1",
            "%s; %s" % (turned, hf))
    since_a = a.mark()
    a.cmd("vr_gadget_screen_dump mp_A_wristscore")
    _, line = a.wait(r"vr_gadget_screen_dump: |no screen yet", 20, since_a, "A's gadget screen dumped")
    if "no screen yet" not in line:
        rel = line.split("vr_gadget_screen_dump: ", 1)[1].split(" (", 1)[0].strip()
        for game in [g for g in GAMES if g != "-game"][::-1] + ["id1"]:
            p = os.path.join(s.base, game, rel)
            if os.path.exists(p):
                out = os.path.join(s.out, "A_wristscore.png")
                os.replace(p, out)  # (moved out of the shared base)
                s.note("screenshot (A's wrist scoreboard page): %s" % out)
                break

    # B's AD page follows the server's game (STAT_QVR_ADHUD's ADHUD_DATA bit; before, a client kept his last local map's
    # vr_ad_mounted): the server in this base has AD's data exactly when A's single-player e1m1 offered the page. (Every
    # agent base has AD's data, so only the "offered" case runs here; B's own cvar is printed for the record.)
    since_b = b.mark()
    b.cmd("vr_ad_mounted; vr_gadget_page 1; vr_gadget_page 1; vr_gadget_page 1")
    b.wait(r"gadget page: ", 10, since_b, "B's pages")
    while len([l for l in b.lines[since_b:] if "gadget page: " in l]) < 3:
        b.wait(r"gadget page: ", 10, b.mark(), "B's three pages")
    b_pages = [l.split("gadget page: ", 1)[1].split()[0] for l in b.lines[since_b:] if "gadget page: " in l]
    b_own = [l.strip() for l in b.lines[since_b:] if '"vr_ad_mounted" is' in l]
    s.check("B's AD page follows the server's game (offered: %s)" % ("1" in pages),
            ("1" in b_pages) == ("1" in pages), "B's pages %s; A's single-player %s; B's own %s" % (b_pages, pages, b_own))


# ---- B13: the server browser, the net HUD, the soak ----
def browser_rows(inst, since):
    """vr_mp_browse's rows printed from `since` on (up to its mp_browser_done line) -> [dict] (name: the rest of the line)."""
    rows = []
    for l in inst.lines[since:]:
        if l.startswith("mp_browser_done:"):
            break
        if l.startswith("mp_browser: row="):
            head, name = l.rstrip().split(" name=", 1)
            f = fields(head)
            f["name"] = name
            rows.append(f)
    return rows


def browse(inst, what):
    """A search (vr_mp_browse) -> its rows, the done line's fields."""
    since = inst.mark()
    inst.cmd("vr_mp_browse")
    _, done = inst.wait(r"mp_browser_done: ", 15, since, what)
    return browser_rows(inst, since), fields(done)


def row_of(rows, address):
    return next((r for r in rows if r["address"] == address), None)


def browser(s):
    """(B13) The server browser (vr_serverbrowser.cpp): a dedicated server on this PC is a favourite of A's: A's search
    asks it by its address (NetQuake's search left out a server at its own address): its row has its name, map,
    players and a ping; a favourite with no server shows no reply. A joins from the row (vr_mp_browse join: the menu's
    Enter): the server is first in his recent list; searched again while in, it counts him. The favourite taken off and
    the recent one forgotten: neither is listed. A screenshot of the page (menu_servers)."""
    srv = s.server(dedicated=True)
    srv.start()
    srv.cmd('hostname "Browse Test"; coop 1; deathmatch 0; skill 1; map e1m1; vr_mp_status')
    srv.wait(r"mp_server: map=e1m1", 120, what="the server on e1m1")
    a = s.client("A").start()
    here = "127.0.0.1:%d" % s.port
    dead = "127.0.0.1:%d" % s.take_port()  # (no server there)
    since = a.mark()
    a.cmd('vr_mp_recent ""; vr_mp_favourites ""; vr_mp_favourite %s; vr_mp_favourite %s' % (here, dead))
    _, on = a.wait(r"mp_browser: favourite %s on" % re.escape(dead), 60, since, "the favourites set")
    rows, done = browse(a, "A's first search")
    r, d = row_of(rows, here), row_of(rows, dead)
    s.check("the server on this PC answers A's search (a favourite, asked by its address)",
            r is not None and r["answered"] == "1" and r["favourite"] == "1" and r["name"] == "Browse Test" and
            r["map"] == "e1m1" and r["users"] == "0" and r["max"] == "4" and r["version"] == "3", "%s" % r)
    ping = int(r["ping"]) if r else -1
    pings = [ping]
    # (the least of up to three searches: one made while either game is held up, A just started or the PC busy after
    # the suite's soak, measures that hitch -- 698 ms once -- not the round trip)
    while r and not 0 <= ping <= 60 * s.time_scale and len(pings) < 3:
        again = row_of(browse(a, "A's search again (its ping)")[0], here)
        pings.append(int(again["ping"]) if again and again["answered"] == "1" else -1)
        ping = min((p for p in pings if p >= 0), default=-1)
    s.metric("browser_ping_ms", ping)
    s.check("its ping is the reply's round trip (0..60 ms on this PC: the server's frame, the client's)",
            0 <= ping <= 60 * s.time_scale, "%d ms (searches: %s)" % (ping, pings))
    s.check("the favourite without a server is listed, no reply", d is not None and d["answered"] == "0",
            "%s; done: %s" % (d, done))
    s.check("the favourites come first in their order", [x["address"] for x in rows[:2]] == [here, dead],
            "%s" % [x["address"] for x in rows])

    # joined from its row (the menu's Enter)
    since = a.mark()
    a.cmd("vr_mp_browse join %d" % rows.index(r))
    a.wait(r"mp_event: .*joined map=e1m1", 120, since, "A joined from the browser")
    since = a.mark()
    a.cmd("vr_mp_favourite")
    _, recent = a.wait(r"mp_recent: ", 10, since, "A's lists")
    s.check("the server joined is first in A's recent list", recent.split("mp_recent: ", 1)[1].strip().split(";")[0] == here,
            recent.strip())
    rows, _ = browse(a, "A's search while in")
    r = row_of(rows, here)
    s.check("searched again while A is in, the server counts him; the row is a favourite and recent",
            r is not None and r["users"] == "1" and r["favourite"] == "1" and r["recent"] == "1", "%s" % r)

    # the page itself (one screenshot), then the lists emptied through the rows (the menu's F and Del)
    since = a.mark()
    a.cmd("disconnect; menu_servers; vr_mp_browse")
    a.wait(r"mp_browser_done: ", 15, since, "the page's search")
    s.note("screenshot (the server browser): %s" % a.screenshot("browser"))
    a.cmd("toggleconsole")
    rows, _ = browse(a, "A's search after leaving")
    since = a.mark()
    a.cmd("vr_mp_browse favourite %d" % rows.index(row_of(rows, here)))
    a.wait(r"mp_browser: favourite %s off" % re.escape(here), 10, since, "the favourite taken off")
    rows, _ = browse(a, "A's search after the favourite went")
    r = row_of(rows, here)
    s.check("taken off the favourites, the server stays as a recent one", r is not None and r["favourite"] == "0" and
            r["recent"] == "1", "%s" % r)
    since = a.mark()
    a.cmd("vr_mp_browse forget %d" % rows.index(r))
    a.wait(r"mp_browser: forgot %s" % re.escape(here), 10, since, "the recent one forgotten")
    rows, _ = browse(a, "A's last search")
    s.check("forgotten, it is not listed (no LAN broadcast reaches a server bound to 127.0.0.1)",
            row_of(rows, here) is None and [x["address"] for x in rows] == [dead], "%s" % [x["address"] for x in rows])


NETHUD_LAG_MS = 50
NETHUD_LOSS = 0.05


VANISH_TIMEOUT = 8  # (B16) B's vr_mp_client_timeout in mp_vanish, s (its default 30: the test's real-time wait shorter)


FUZZ_RATE = 0.3      # the chance each of B's game packets is corrupted while fuzzing (mp_proxy.py mangle)
FUZZ_PACKETS = 30    # ... until this many were, each way (B joining again each time his link ends: ~2 min)
FUZZ_CAP_S = 300     # ... a safety net, s


def fuzz_until(s, b, direction, what):
    """B's packets `direction` corrupted until FUZZ_PACKETS were (FUZZ_CAP_S at most). Each time his link ends (the
    server dropped him for a malformed message, or his game left on an illegible one: no packet that way for 2 s) he
    joins again, uncorrupted while he does -> (how many, his joins)."""
    p = b.proxy
    t0 = time.monotonic()
    joins = 0
    while sum(p.corrupted[direction]) < FUZZ_PACKETS and time.monotonic() - t0 < FUZZ_CAP_S:
        p.corrupt[direction] = FUZZ_RATE
        last, still = -1, time.monotonic()
        while sum(p.corrupted[direction]) < FUZZ_PACKETS and time.monotonic() - t0 < FUZZ_CAP_S:
            n = p.stats[direction][0]
            if n != last:
                last, still = n, time.monotonic()
            elif time.monotonic() - still > 2:
                break
            time.sleep(0.05)
        p.corrupt[direction] = 0.0
        if sum(p.corrupted[direction]) >= FUZZ_PACKETS or b.proc.poll() is not None:
            break
        if p.stats[direction][0] == last:  # (his link ended: in again)
            since = b.mark()
            b.cmd("disconnect; " + s.connect_cmd(b))
            if wait_quietly(b, r"mp_event: .*joined map=e1m1", 60, since, "B joined again") is None:
                break
            joins += 1
    n = sum(p.corrupted[direction])
    s.note("%s: %d packets corrupted (bits flipped, cut short, random, garbage after, a command after: %s) in %.1f s; B joined again "
           "%d times" % (what, n, p.corrupted[direction], time.monotonic() - t0, joins))
    return n, joins


def alive_check(s, inst, what):
    """`inst`'s game still running and answering its console -> True."""
    if inst.proc.poll() is not None:
        s.check("%s: still running" % what, False, "exited with %s" % inst.proc.returncode)
        return False
    since = inst.mark()
    inst.cmd("echo mp_fuzz_alive")
    ok = wait_quietly(inst, r"^mp_fuzz_alive", 30, since, what) is not None
    s.check("%s: still running and answering" % what, ok, "answered" if ok else "no answer in 30 s")
    return ok


FUZZ_SEED = 0              # (B26) --seed: the proxy's corruptions (0: a new one each run, printed)
FUZZ_SIGNON_JOINS = 40     # (B26) fuzz --part signon: B's joins (--repeat N: N instead)
FUZZ_SIGNON_RATE = 0.2     # ... each of the server's packets down corrupted at this chance through each join


# (B26) Hostile server commands, each after one game packet's message down to B (mp_proxy.py inject): the values a
# client's parsers must check. The suite's run takes these; `fuzz --part inject` every command code with garbage.
FUZZ_HOSTILE = [
    ("svc_setview 32767", bytes([5, 0xFF, 0x7F])),  # B25b's crash: the view entity past the client's array
    ("svc_setview -1", bytes([5, 0xFF, 0xFF])),
    ("svc_setview -32768", bytes([5, 0x00, 0x80])),
    ("svc_updatestat 255", bytes([3, 255, 1, 0, 0, 0])),
    ("svc_updatefrags 255", bytes([14, 255, 1, 0])),
    ("svc_updatecolors 255", bytes([17, 255, 1])),
    ("svc_updatename 255", bytes([13, 255, 0x41, 0])),
    ("svc_lightstyle 255", bytes([12, 255, 0x61, 0])),
]


def fuzz_sweep():
    """Every server command code, and every svc_quakevr sub-command, with all-0xFF and all-0x7F arguments."""
    out = []
    for fill in (0xFF, 0x7F):
        for cmd in range(57):
            if cmd in (2, 11):  # (svc_disconnect, svc_serverinfo: B leaves or starts a map; their own checks)
                continue
            out.append(("svc %d, args 0x%02X" % (cmd, fill), bytes([cmd]) + bytes([fill] * 12)))
        for sub in range(64):
            out.append(("svc_quakevr %d, args 0x%02X" % (sub, fill), bytes([39, sub]) + bytes([fill] * 12)))
    return out


def fuzz_inject(s, b, hostile):
    """(B26) Each of `hostile` (name, bytes) after one packet's message down to B, B in the game: his game runs on
    (each a Host_Error back to the menu, or taken and checked); B joins again whenever he left. -> how many left."""
    p = b.proxy
    left, notes = 0, []
    for i, (name, data) in enumerate(hostile):
        since = b.mark()
        before = p.injected["down"]
        p.inject["down"].append(data)
        t0 = time.monotonic()
        while p.injected["down"] == before and time.monotonic() - t0 < 10:
            time.sleep(0.02)
        b.cmd("echo mp_fuzz_inject_%d" % i)
        b.cmd("echo mp_fuzz_inject_%d_after" % i)  # (a frame after the packet's: its parse done)
        hit = wait_quietly(b, r"^mp_fuzz_inject_%d_after" % i, 30, since, "B after %s" % name)
        if b.proc.poll() is not None:
            s.check("B's game survives the hostile command %s" % name, False, "exited with %s" % b.proc.returncode)
            return left
        if hit is None:
            s.check("B's game answers after the hostile command %s" % name, False, "no answer in 30 s")
            return left
        b.read()
        err = next((l.strip() for l in b.lines[since:] if l.startswith("Host_Error")), None)
        if err or own_status(b).get("connected") != "1":
            left += 1
            notes.append("%s: %s" % (name, err[:70] if err else "left"))
            js = b.mark()
            b.cmd("disconnect; " + s.connect_cmd(b))
            if wait_quietly(b, r"mp_event: .*joined map=e1m1", 60, js, "B joined again") is None:
                s.check("B joins again after the hostile command %s" % name, False, "not in 60 s")
                return left
    s.note("hostile commands down: %d sent, B left %d times: %s" % (len(hostile), left, notes[:12]))
    s.check("B's game survives %d hostile commands down" % len(hostile), b.proc.poll() is None, "running")
    return left


# (B27) Hostile client commands up, each after one of B's packets' messages (mp_proxy.py inject "up"): the string
# commands the server takes from a client (sv_user.c's list) with values its parsers must check, and the admin ones a
# guest must not have (kick, ban: NetQuake let any coop client kick the others, ban addresses).
def _cmd_up(text):
    return bytes([4]) + text.encode("latin-1") + b"\0"


FUZZ_HOSTILE_UP = [(t[:40], _cmd_up(t)) for t in [
    "kick # 1", "kick # 2", "kick A", "ban 127.0.0.1", "ban 127.0.0.1 off","name " + "x" * 1000, "color 999999 -5", "say " + "%s%n" * 60,
    "tell #99 hi", "tell \x01\xff\xfe", "give h 2147483647", "give a -2147483648", "setpos nan nan nan",
    "setpos 1e30 -1e30 1e30 1e30 1e30 1e30", "ping", "status", "pause", "kill", "prespawn", "spawn", "begin",
    "vr_avatarcal " + "99999 " * 64, "vr_avatarcal -1 -1", "vr_prefs " + "\xff" * 300, "vr_prefs 99999 nan inf",
    "vr_rig " + "1e39 " * 40, "vr_rigframe 9 9 " + "nan " * 30, "vr_rigframe -1 -1 0", "vr_heldpose 99 " + "1e30 " * 40,
    "vr_heldpose -5 nan", "vr_heldpose 3 1 1 200 " + "0 " * 62 + "1 200",  # (mpfix C1: slot, leaveSlot past the table)
    "vr_grabspot -1 99999", "vr_grabspot 99999 -1", "vr_magpoll 255 255 255",
    "vr_lagcomp_shot 1e30 nan 0 0 0 0", "vr_lagcomp_punch -1 99999 0", "vr_scene_count " + "9" * 400]]


def fuzz_sweep_up():
    """(B27) Every client command code (clc 0-17, but clc_disconnect), with all-0xFF and all-0x7F arguments of 0, 12
    and 64 bytes (clc_move's, QVR_CLC_MOVES' and QVR_CLC_VOICE's own reads meet them)."""
    out = []
    for fill in (0xFF, 0x7F):
        for cmd in range(18):
            if cmd == 2:
                continue
            for n in (0, 12, 64):
                out.append(("clc %d, %d args 0x%02X" % (cmd, n, fill), bytes([cmd]) + bytes([fill] * n)))
    return out


def fuzz_inject_up(s, srv, a, b, ea, eb, hostile):
    """(B27) Each of `hostile` (name, bytes) after one of B's packets' message up, B in the game: the server runs on
    (each a drop of B with a reason, or taken and checked), A stays in it (no guest's kick or ban reaches him); B joins
    again whenever he was dropped. -> how many drops."""
    p = b.proxy
    dropped, notes = 0, []
    reasons = {}
    for i, (name, data) in enumerate(hostile):
        before = p.injected["up"]
        sv_since = srv.mark()
        p.inject["up"].append(data)
        t0 = time.monotonic()
        while p.injected["up"] == before and time.monotonic() - t0 < 10:
            time.sleep(0.02)
        if p.injected["up"] == before:
            # (B28: only after an unreliable datagram: B sends one each frame in the game)
            s.check("the hostile command up %s went (after one of B's datagrams)" % name, False, "not in 10 s")
            return dropped
        server_time_past(srv, server_time(srv) + 0.1)  # (read and run by the server)
        if srv.proc.poll() is not None:
            s.check("the server survives the hostile command up %s" % name, False, "exited with %s" % srv.proc.returncode)
            return dropped
        # (B28) the server's word (its clients' slots), not A's or B's games': a dropped client is told only when his
        # connection can take a message then (SV_DropClient), else his game finds out at its timeout
        slots = set(int(m.group(1)) for m in (re.match(r"mp_client: slot=(\d+)", l) for l in status(srv)) if m)
        srv.read()
        sv_lines = [l.strip() for l in srv.lines[sv_since:]]
        if ea - 1 not in slots:
            s.check("A stays in the game after B's hostile command up %s" % name, False, "A's slot gone: %s" % [
                l for l in sv_lines if not l.startswith("mp_")][-4:])
            return dropped
        if eb - 1 not in slots:
            dropped += 1
            why = next((l for l in sv_lines if l.startswith("SV_ReadClientMessage")), "removed")
            reasons[why] = reasons.get(why, 0) + 1
            notes.append(name)
            js = b.mark()
            b.cmd("disconnect; " + s.connect_cmd(b))
            hit = wait_quietly(b, r"mp_event: .*joined map=e1m1", 60, js, "B joined again")
            if hit is None:
                s.check("B joins again after the hostile command up %s" % name, False, "not in 60 s")
                return dropped
            eb = int(fields(hit[1])["self"])
    s.note("hostile commands up: %d sent, B dropped %d times: %s; the server's reasons %s" % (
        len(hostile), dropped, notes[:12], reasons))
    s.check("the server survives %d hostile commands up, A in the game" % len(hostile), srv.proc.poll() is None and
            own_status(a).get("connected") == "1", "running")
    return dropped


def fuzz_signon(s, srv, b):
    """(B26) B25b's crash: B's game exited 0xC0000005 after a corrupt packet down in a join's signon (a garbage
    svc_setview: the view entity out of the client's array, read by the hands' next frame). B joins FUZZ_SIGNON_JOINS
    times (--repeat N) with the server's packets down corrupted from his connect until he is in the game or his game
    leaves it (Host_Error, the illegible message's reason); each join must end one way or the other, his game running.
    Then B still answers and joins clean, the server still answers."""
    p = b.proxy
    joins = PART_REPEAT if PART_REPEAT > 1 else FUZZ_SIGNON_JOINS
    outcome = {"joined": 0, "left": 0, "stalled": 0}
    reasons = set()
    crashed = False
    for _ in range(joins):
        since = b.mark()
        b.cmd("disconnect")
        p.corrupt["down"] = FUZZ_SIGNON_RATE
        b.cmd(s.connect_cmd(b))
        hit = wait_quietly(b, r"mp_event: .*joined map=e1m1|^Host_Error", 30, since, "B's fuzzed join")
        p.corrupt["down"] = 0.0
        if b.proc.poll() is not None:
            crashed = True
            break
        if hit is None:
            outcome["stalled"] += 1
        elif hit[1].startswith("Host_Error"):
            outcome["left"] += 1
            reasons.add(hit[1].strip()[:90])
        else:
            outcome["joined"] += 1
    s.note("signon fuzz: %s; %d packets down corrupted %s; reasons (first 6): %s" % (
        outcome, sum(p.corrupted["down"]), p.corrupted["down"], sorted(reasons)[:6]))
    s.check("B's game survives %d joins under corrupt packets down" % joins, not crashed,
            "exited with %s" % b.proc.returncode if crashed else "running")
    s.check("each fuzzed join ends (in the game or left with a reason)", outcome["stalled"] == 0, str(outcome))
    if alive_check(s, b, "B's game after the fuzzed joins"):
        since = b.mark()
        b.cmd("disconnect; " + s.connect_cmd(b))
        ok = wait_quietly(b, r"mp_event: .*joined map=e1m1", 60, since, "B's clean join") is not None
        s.check("B joins clean afterwards", ok, "joined" if ok else "not in 60 s")
    alive_check(s, srv, "the server after B's fuzzed joins")


def fuzz(s):
    """(B18) A broken or hostile peer can't crash a game: a public server takes strangers' packets, and its players'
    games take a stranger server's. On a dedicated server with A and B (through his proxy), each walking: B's packets up
    corrupted (bits flipped, cut short, random bytes, garbage after; FUZZ_RATE of them until FUZZ_PACKETS were; B
    joining again each time the server drops him): the server still runs and answers, A plays on (his place on the
    server moves, his game connected). Then the server's packets down to B, the same way: B's game still runs and
    answers (each illegible message a Host_Error back to the menu, not a crash), the server and A untouched."""
    srv, a, b, ea, eb = join(s)
    seed = FUZZ_SEED or random.randrange(1, 1 << 31)
    b.proxy.set_fuzz_seed(seed)
    s.note("fuzz seed %d (--seed %d: the same corruptions in the same packets' order)" % (seed, seed))
    if PART == "signon":
        fuzz_signon(s, srv, b)
        return
    if PART == "inject":
        fuzz_inject(s, b, FUZZ_HOSTILE + fuzz_sweep())
        alive_check(s, srv, "the server after the hostile commands")
        return
    if PART == "injectup":
        # (B27; B28: the bytes now after one of B's datagrams' messages, not an ack's: the server parses them)
        fuzz_inject_up(s, srv, a, b, ea, eb, FUZZ_HOSTILE_UP + fuzz_sweep_up())
        alive_check(s, b, "B's game after his hostile commands up")
        return
    fuzz_inject(s, b, FUZZ_HOSTILE)
    # (B28) and up: the hostile client commands and every clc code with garbage (fuzz --part injectup's), B in the game
    fuzz_inject_up(s, srv, a, b, ea, eb, FUZZ_HOSTILE_UP + fuzz_sweep_up())
    for inst in (a, b):
        inst.cmd("god; notarget")
    a.cmd("vr_mock_stick off 0.3 0.6")

    def a_plays(what):
        before = org_of(status(srv), "mp_client", "slot", ea - 1)
        a.cmd("vr_mock_stick off -0.3 -0.6")
        moved = None
        for _ in range(60):
            now = org_of(status(srv), "mp_client", "slot", ea - 1)
            if before and now and dist(before, now) > 16.0:
                moved = now
                break
        a.cmd("vr_mock_stick off 0.3 0.6")
        own = own_status(a)
        s.check("%s: A plays on (the server moves him, his game connected)" % what,
                moved is not None and own.get("connected") == "1",
                "from %s to %s; connected %s" % (before, moved, own.get("connected")))

    sv_since = srv.mark()
    n, joins = fuzz_until(s, b, "up", "B's packets up")
    srv.read()
    drops = [l.strip() for l in srv.lines[sv_since:] if l.startswith("SV_ReadClientMessage")]
    s.note("the server dropped B %d times: %s" % (len(drops), sorted(set(drops))[:6]))
    s.check("B's malformed messages up are dropped with a reason (the server's console)", len(drops) > 0,
            "%d" % len(drops))
    s.check("B's packets up corrupted: %d" % FUZZ_PACKETS, n >= FUZZ_PACKETS, "%d (B joined again %d times)" % (n, joins))
    if alive_check(s, srv, "the server after B's corrupt packets up"):
        a_plays("B's packets up corrupted")
    alive_check(s, b, "B's game after his corrupt packets up")

    b_since = b.mark()
    n, joins = fuzz_until(s, b, "down", "the server's packets down to B")
    errs = [l.strip() for l in b.lines[b_since:] if l.startswith("Host_Error")]
    s.note("B's game left %d times: %s" % (len(errs), sorted(set(errs))[:6]))
    s.check("the server's packets down to B corrupted: %d" % FUZZ_PACKETS, n >= FUZZ_PACKETS,
            "%d (B joined again %d times)" % (n, joins))
    alive_check(s, b, "B's game after corrupt packets down")
    if alive_check(s, srv, "the server after B's corrupt packets down"):
        a_plays("B's packets down corrupted")


GUEST_TALK_BURST = 4  # (B28) vr_server.cpp talkBurst: a guest's messages taken in its window before he is silenced


def guests(s):
    """(B28) A guest may not run the host's commands: NetQuake let any coop client kick the others (Host_Kick_f refused
    only in deathmatch) and pause everyone's game. A hosts a listen server (pausable 1), B joins (a guest): with
    vr_mp_guest_admin 0 (the default) B's kick of A, pause and vr_scene_clean are refused and he is told so, his ban
    too (the server console's own), A still in and the game not paused; A, the host, pauses and unpauses. B's talk:
    GUEST_TALK_BURST messages, then he is silenced (told), A sees no more; the host's own talk is not held. Then
    vr_mp_guest_admin 1: B's pause and unpause go (as NetQuake's); and the host kicks B. Each wait is on the line its
    step must print."""
    srv, a, b, ea, eb = join(s, listen=True)
    a.cmd("pausable 1; vr_mp_guest_admin 0")
    since_a, since_b = a.mark(), b.mark()
    for c in ("kick # %d" % ea, "pause", "vr_scene_clean gibs", "ban 127.0.0.1"):
        b.cmd(c)
    told = []
    for what in ("kick", "pause", "vr_scene_clean"):
        hit = wait_quietly(b, r"^%s: refused, only the host may" % what, 20, since_b, "B told: %s refused" % what)
        told.append(hit is not None)
    banned = wait_quietly(b, r"^ban: refused, only the server's console may", 20, since_b, "B told: ban refused")
    s.check("a guest's kick, pause and scene cleaning refused, each told so (vr_mp_guest_admin 0)", all(told),
            "told %s" % dict(zip(("kick", "pause", "vr_scene_clean"), told)))
    since = a.mark()
    a.cmd("ban; echo mp_guests_ban")
    a.wait(r"^mp_guests_ban", 10, since, "the host's ban list")
    s.check("a guest's ban refused (the server console's own), told so, no address banned", banned is not None and
            a.find(r"^Banning not active", since) is not None, "told %s; host: %s" % (
                banned is not None, "not active" if a.find(r"^Banning not active", since) else "a ban active"))
    rows = status(a)
    slots = set(int(m.group(1)) for m in (re.match(r"mp_client: slot=(\d+)", l) for l in rows) if m)
    paused = a.find(r"paused the game", since_a)
    s.check("A still in, the game not paused", ea - 1 in slots and eb - 1 in slots and paused is None,
            "slots %s; %s" % (sorted(slots), "paused" if paused else "not paused"))

    # the host's own pause
    since_b = b.mark()
    a.cmd("pause")
    p1 = wait_quietly(b, r"paused the game", 20, since_b, "B told the host paused")
    a.cmd("pause")
    p2 = wait_quietly(b, r"unpaused the game", 20, since_b, "B told the host unpaused")
    s.check("the host pauses and unpauses", p1 is not None and p2 is not None, "%s; %s" % (
        p1[1].strip() if p1 else "no pause", p2[1].strip() if p2 else "no unpause"))

    # a guest's talk flood, the host's own talk
    since_a, since_b = a.mark(), b.mark()
    for i in range(GUEST_TALK_BURST + 2):
        b.cmd("say flood_%d" % (i + 1))
    silenced = wait_quietly(b, r"^You can't talk for \d+ more seconds \(more than", 20, since_b, "B silenced")
    since = a.mark()
    a.cmd("echo mp_guests_talk")  # (B's messages before his notice came in the same frame as it, or none)
    a.wait(r"^mp_guests_talk", 10, since, "A's console after B's talk")
    a.read()
    seen = sorted(set(int(m.group(1)) for m in (re.search(r"flood_(\d+)", l) for l in a.lines[since_a:]) if m))
    s.check("a guest's talk: %d messages, then he is silenced and told so" % GUEST_TALK_BURST,
            silenced is not None and seen == list(range(1, GUEST_TALK_BURST + 1)),
            "A saw %s; B %s" % (seen, "told" if silenced else "not told"))
    since_b = b.mark()
    for i in range(GUEST_TALK_BURST + 2):
        a.cmd("say hostsays_%d" % (i + 1))
    last = wait_quietly(b, r"hostsays_%d" % (GUEST_TALK_BURST + 2), 20, since_b, "B seeing the host's talk")
    s.check("the host's own talk is not held", last is not None, "B saw all %d" % (GUEST_TALK_BURST + 2) if last else
            "B missed the last")

    # the host allows guests: B's pause goes; then the host kicks B
    a.cmd("vr_mp_guest_admin 1")
    since_a = a.mark()
    b.cmd("pause")
    g1 = wait_quietly(a, r"paused the game", 20, since_a, "B's pause allowed")
    b.cmd("pause")
    g2 = wait_quietly(a, r"unpaused the game", 20, since_a, "B's unpause")
    s.check("vr_mp_guest_admin 1: a guest's pause and unpause go", g1 is not None and g2 is not None, "%s; %s" % (
        g1[1].strip() if g1 else "refused", g2[1].strip() if g2 else "not unpaused"))
    a.cmd("vr_mp_guest_admin 0")
    # (the server's word: B's game is told "Kicked by" only when his reliable channel is free then -- SV_DropClient's
    # svc_disconnect; else he learns at his timeout: B29)
    since_a = a.mark()
    a.cmd("kick # %d" % eb)
    kicked = wait_quietly(a, r"^Client .* removed", 20, since_a, "B removed by the host's kick")
    rows = status(a)
    slots = set(int(m.group(1)) for m in (re.match(r"mp_client: slot=(\d+)", l) for l in rows) if m)
    s.check("the host kicks a guest", kicked is not None and eb - 1 not in slots and ea - 1 in slots,
            "removed %s; slots %s" % (kicked is not None, sorted(slots)))


def vanish(s):
    """(B16) A vanished server, at its client: A hosts a listen server (his own client on the loopback), B joins
    through a proxy, both with vr_mp_client_timeout VANISH_TIMEOUT. B's link down cut (the proxy drops it): after 3 s
    B's net HUD and view say the server is not responding, A's own game is untouched (no notice, still in it); the
    link back: B says so and plays on. Then A's game is killed (a crash: behind B's proxy no port unreachable reaches
    B, as over the internet): B says not responding, then at VANISH_TIMEOUT (not net_messagetimeout's 300 s) leaves the
    game with the reason, the server browser open, his game still running. (The waits are on B's console lines: the
    silence is real time, the lesser of the wall clock and the game's.)"""
    srv, a, b, ea, eb = join(s, listen=True)
    for inst in (a, b):
        inst.cmd("vr_mp_client_timeout %d" % VANISH_TIMEOUT)
    since_a, since_b = a.mark(), b.mark()
    b.proxy.blackout = True
    cut = time.monotonic()
    s.note("B's link down cut")
    notice = wait_quietly(b, r"^Server not responding \(", 15, since_b, "B's notice")
    after = time.monotonic() - cut
    fb = nethud_report(b, 0, "B's net HUD silent")
    fa = nethud_report(a, 0, "A's net HUD (the host)")
    b.proxy.blackout = False
    s.check("B's link cut: after 3 s he is told the server is not responding (console, HUD line, the view's centre "
            "print)", notice is not None and 2.5 <= after <= 6.0 and fb["notice"] == "1" and
            fb["notice_line"].startswith("SERVER NOT RESPONDING"),
            "%s after %.1f s; HUD notice %s line '%s', silence %s s" % (
                notice[1].strip() if notice else "no notice", after, fb["notice"], fb["notice_line"], fb["silence"]))
    back = wait_quietly(b, r"^Server responding again", 15, since_b, "B hearing the server again")
    own_b = own_status(b)
    s.check("the link back: B says so and is still in the game", back is not None and own_b.get("connected") == "1",
            "%s; connected %s" % ("told" if back else "not told", own_b.get("connected")))
    own_a = own_status(a)
    s.check("A, the listen host, untouched meanwhile: no notice, his own game on", fa["notice"] == "0" and
            fa["remote"] == "0" and a.find(r"Server not responding", since_a) is None and own_a.get("connected") == "1",
            "notice %s remote %s; connected %s" % (fa["notice"], fa["remote"], own_a.get("connected")))

    # A's game crashes (killed): B hears nothing more from it
    since_b = b.mark()
    a.proc.kill()
    a.proc.wait()
    killed = time.monotonic()
    s.instances.remove(a)
    s.note("A's game (the host) killed")
    notice = wait_quietly(b, r"^Server not responding \(", 15, since_b, "B's notice of the dead host")
    noticed = time.monotonic() - killed
    dropped = wait_quietly(b, r"^Server not responding for \d+ s: disconnected", VANISH_TIMEOUT + 20, since_b,
                           "B dropping the dead host")
    gone = time.monotonic() - killed
    left = wait_quietly(b, r"mp_event: .* left", 10, since_b, "B out of the game")
    own_b = own_status(b)
    s.metric("vanish_notice_s", round(noticed, 2))
    s.metric("vanish_drop_s", round(gone, 2))
    s.check("the host gone: B told it is not responding after 3 s", notice is not None and 2.0 <= noticed <= 6.0,
            "%s after %.1f s" % (notice[1].strip() if notice else "no notice", noticed))
    s.check("then at his timeout (%d s, not 300) B leaves the game with the reason, back at the server browser, his "
            "game running" % VANISH_TIMEOUT, dropped is not None and VANISH_TIMEOUT - 1.0 <= gone <= VANISH_TIMEOUT + 5 and
            left is not None and own_b.get("connected") == "0" and own_b.get("menu") == "1" and b.proc.poll() is None,
            "%s after %.1f s; left %s; connected %s menu %s" % (dropped[1].strip() if dropped else "not dropped", gone,
                                                                "yes" if left else "no", own_b.get("connected"),
                                                                own_b.get("menu")))


def nethud_report(inst, windows, what):
    since = inst.mark()
    inst.cmd("vr_net_hud_report %d" % windows)
    _, line = inst.wait(r"mp_nethud: ", 30, since, what)
    f = fields(line.split(" line0=")[0])
    f["line0"] = line.split('line0="', 1)[1].split('"', 1)[0]
    f["line1"] = line.split('line1="', 1)[1].split('"', 1)[0]
    f["notice_line"] = line.split('notice_line="', 1)[1].split('"', 1)[0] if 'notice_line="' in line else ""
    return f


def nethud(s):
    """(B13) The net HUD (vr_net_hud, vr_nethud.cpp): A joins directly, B through a proxy 50 ms each way losing 5% of
    the packets; over three one-second windows B's HUD shows a ping of his moves' round trip (100 ms and the frames
    between), the datagrams lost (a few %), the bytes each way; A's a few ms and no loss. B walks (the mock's stick):
    his corrections a second are shown (a metric). The lines drawn: a screenshot of B's."""
    srv = s.server(dedicated=True)
    srv.start()
    srv.cmd("coop 1; deathmatch 0; skill 1; map e1m1; vr_mp_status")
    srv.wait(r"mp_server: map=e1m1", 120, what="the server on e1m1")
    a = s.client("A").start()
    b = s.client("B", lag_ms=NETHUD_LAG_MS, loss=NETHUD_LOSS).start()
    a.cmd(s.connect_cmd(a))
    b.cmd(s.connect_cmd(b))
    a.wait(r"mp_event: .*joined map=e1m1", 120, what="A joined")
    b.wait(r"mp_event: .*joined map=e1m1", 120, what="B joined")
    a.cmd("vr_net_hud 1")
    b.cmd("vr_net_hud 2")
    fa = nethud_report(a, 3, "A's net HUD over 3 s")
    fb = nethud_report(b, 3, "B's net HUD over 3 s")
    for k in ("ping", "loss", "in", "out", "resent", "fix_rate"):
        s.metric("nethud_a_" + k, float(fa[k]))
        s.metric("nethud_b_" + k, float(fb[k]))
    rtt = 2 * NETHUD_LAG_MS
    s.check("B's HUD: a whole window, his moves' round trip as the ping", fb["window"] == "1" and fb["predicts"] == "1"
            and rtt <= int(fb["ping"]) <= rtt + 60 * s.time_scale, "ping %s ms (last %s; the link's %d)" % (
                fb["ping"], fb["ping_last"], rtt))
    s.check("B's HUD: the datagrams lost (5% each way: 1-12% of the server's)", 1.0 <= float(fb["loss"]) <= 12.0,
            "%s%%" % fb["loss"])
    s.check("B's HUD: the bytes each way", float(fb["in"]) > 0.5 and float(fb["out"]) > 0.5, "in %s out %s KB/s" % (
        fb["in"], fb["out"]))
    s.check("A's HUD: a few ms, nothing lost", fa["window"] == "1" and int(fa["ping"]) <= 40 * s.time_scale and
            float(fa["loss"]) == 0.0, "ping %s ms, loss %s%%" % (fa["ping"], fa["loss"]))
    s.check("the lines drawn: one at A (vr_net_hud 1), B's two", fa["line0"].startswith("ping") and
            fb["line1"].startswith("in "), "A: '%s'; B: '%s' / '%s'" % (fa["line0"], fb["line0"], fb["line1"]))
    # B walks: his corrections a second (a metric: the moves' replay is exact on flat ground, so mostly none)
    b.cmd("vr_mock_stick off 0 1")
    fw = nethud_report(b, 2, "B's net HUD walking")
    b.cmd("vr_mock_stick off 0 0")
    s.metric("nethud_b_walk_fix_rate", float(fw["fix_rate"]))
    s.metric("nethud_b_walk_fix_mean", float(fw["fix_mean"]))
    s.note("B walking: %s / %s" % (fw["line0"], fw["line1"]))
    s.note("screenshot (B's net HUD): %s" % b.screenshot("nethud"))


SOAK_MAP = "dm3"   # (dm4's few starts: 16 players telefrag each other at their respawns)
SOAK_BOTS = 14     # A, B and 14 FrikBot bots: 16 players (MAX_SCOREBOARD)
SOAK_SECONDS = 10  # the window: the server's game time (vr_mp_after)
SOAK_FRAGS = 5     # at least this many frags scored in it (the bots fight)
SOAK_GIBS_MAX = 64     # vr_dm_gibs_max's default (B13b)
SOAK_WEAPONS_MAX = 32  # vr_dm_weapons_max's default
SOAK_PACE_MIN = 0.8    # the server's game time a second of wall clock, at least (B13: 0.18-0.74; B13b: ~1)
SOAK_RATE = 80000      # vr_net_rate's default (B/s), B's
SOAK_LINK_MAX = 45000  # B's link down, B/s at most (B13b: 24-33 KB/s with the broadcast culled, by the fights; unculled at
#                        real pace ~38 KB/s and more; the culling's own check is its bytes)
SOAK_TELEFRAGS_MAX = 10  # telefrags in the window at most (B13: ~100-200)


def cpu_seconds(inst):
    """A game's CPU time so far (user and kernel, GetProcessTimes) in seconds."""
    import ctypes
    from ctypes import wintypes
    c, e, k, u = (wintypes.FILETIME() for _ in range(4))
    ctypes.windll.kernel32.GetProcessTimes(wintypes.HANDLE(int(inst.proc._handle)), ctypes.byref(c), ctypes.byref(e),
                                           ctypes.byref(k), ctypes.byref(u))
    return sum((t.dwHighDateTime << 32 | t.dwLowDateTime) for t in (k, u)) / 1e7


def profile_lines(inst, seconds, what):
    """vr_profile_report over the last `seconds` -> its lines (the frame's zones, quakec's top functions)."""
    since = inst.mark()
    inst.cmd("vr_profile_report %.1f" % seconds)
    _, line = inst.wait(r"^views \(the rest is shared\)|vr_profile_report: no", 20, since, what)
    if line.startswith("views"):
        inst.wait(r"traces and builtins|GPU: \d+ frames|no GPU times", 10, since, what)
    inst.mark()
    return [l.rstrip() for l in inst.lines[since:] if l.strip()]


def add_bots(a, since=0):
    """SOAK_BOTS FrikBot bots added by A (impulse 100), each named at A in its lines from `since` (a definite end: each
    named within 15 s)."""
    for i in range(SOAK_BOTS):
        a.cmd("impulse 100")
        while len(frag_table(a, since)) < 3 + i:  # (a bot's own line: frags lines repeat the named ones)
            a.wait(r"mp_frags: .*named=1", 15, a.mark(), "bot %d named at A" % (i + 1))


def soak(s):
    """(B13) The CPU of many players: a dedicated deathmatch server (no frag or time limit) on dm3 with A, B (through a
    proxy: his link counted) and 14 FrikBot bots: 16 players. Once all are listed, a window of 10 s of the server's game
    time (vr_mp_after): each game's CPU a second of wall clock (its process times), its vr_profile report (the frame's
    zones; quakec's top functions; profile_<game>.txt), the server's edicts and Box3D bodies (vr_mp_edicts), B's link
    down, both net HUDs. Checks: 16 players at both clients, frags scored, the edicts bounded, nobody dropped. The CPU,
    the server's pace and B's link are metrics (not timings when
    the author's game runs: low impact)."""
    srv = s.server(dedicated=True, maxplayers=16)
    srv.start()
    srv.cmd("vr_mp_watch 0; coop 0; deathmatch 1; fraglimit 0; timelimit 0; skill 1; map %s; vr_mp_status" % SOAK_MAP)
    srv.wait(r"mp_server: map=%s" % SOAK_MAP, 120, what="the server on %s" % SOAK_MAP)
    a = s.client("A").start()
    b = s.client("B", proxy=True).start()
    a.cmd(s.connect_cmd(a))
    b.cmd(s.connect_cmd(b))
    a.wait(r"mp_event: .*joined map=%s" % SOAK_MAP, 120, what="A joined")
    b.wait(r"mp_event: .*joined map=%s" % SOAK_MAP, 120, what="B joined")
    add_bots(a)
    for inst, what in ((a, "A"), (b, "B")):
        while len(frag_table(inst)) < 2 + SOAK_BOTS:
            inst.wait(r"mp_frags: .*named=1", 15, inst.mark(), "the bots named at %s" % what)
        s.check("16 players listed at %s" % what, len(frag_table(inst)) == 2 + SOAK_BOTS)

    since = srv.mark()
    srv.cmd("vr_mp_edicts")
    _, ed0 = srv.wait(r"mp_edicts: ", 30, since, "the server's edicts")
    s.note("server edicts, the bots in: %s" % ed0.strip())

    # the window: SOAK_SECONDS of the server's game time
    for inst in (srv, a, b):
        inst.cmd("vr_profile 1; vr_profile_gpu 0")
    a.cmd("vr_net_hud 1")
    b.cmd("vr_net_hud 2")
    srv.cmd("vr_net_broadcast_report reset")
    t0 = time.monotonic()
    cpu0 = {i.name: cpu_seconds(i) for i in (srv, a, b)}
    p0 = b.proxy.snapshot()
    a_window = a.mark()  # (its lines read up to now: the frags so far)
    start = sum(f for _, f in frag_table(a).values())
    since = srv.mark()
    srv.cmd("vr_mp_after %g soak_window_done" % SOAK_SECONDS)
    srv.wait(r"mp_after: .*soak_window_done", 600, since, "the soak's %g s of the server's game time" % SOAK_SECONDS)
    span = time.monotonic() - t0
    since2 = srv.mark()
    srv.cmd("vr_mp_edicts")
    _, ed1 = srv.wait(r"mp_edicts: ", 30, since2, "the server's edicts")
    _, bx = srv.wait(r"mp_box3d: ", 30, since2, "the server's Box3D bodies")
    s.note("server edicts, the window's end: %s" % ed1.strip())
    s.note("server Box3D, the window's end: %s" % bx.strip())
    used0, used1 = int(fields(ed0)["used"]), int(fields(ed1)["used"])
    s.metric("soak_edicts", used1)
    s.metric("soak_box3d_bodies", int(fields(bx)["bodies"]))
    s.metric("soak_box3d_awake", int(fields(bx)["awake"]))
    # (B13: a telefrag storm at the starts once dropped two weapons a death until ED_Alloc ran out of its 16384)
    s.check("the server's edicts stay bounded (under 2048 after the window)", used1 < 2048, "%d -> %d" % (used0, used1))
    # (B13b: deathmatch's caps, the oldest first; a frame's new deaths may add a few before the next frame's trim)
    fe = fields(ed1)
    gibs = int(fe.get("gibs", "-1")) - int(fe.get("vr_smallgib", "0")) - int(fe.get("vr_limb", "0"))  # (their own caps)
    weapons = int(fields(ed1).get("thrown_weapon", "0"))
    s.metric("soak_gibs", gibs)
    s.metric("soak_weapons", weapons)
    s.check("the loose gibs capped (vr_dm_gibs_max %d, a frame's deaths more)" % SOAK_GIBS_MAX,
            0 <= gibs <= SOAK_GIBS_MAX + 32, "%d gibs and heads" % gibs)
    s.check("the dropped weapons capped (vr_dm_weapons_max %d, a frame's deaths more)" % SOAK_WEAPONS_MAX,
            weapons <= SOAK_WEAPONS_MAX + 16, "%d weapons" % weapons)
    a.mark()
    scored = sum(f for _, f in frag_table(a).values()) - start
    p1 = b.proxy.snapshot()
    cpu = {i.name: (cpu_seconds(i) - cpu0[i.name]) / span for i in (srv, a, b)}
    s.check("the players scored frags in the window (the bots fight)", scored >= SOAK_FRAGS, "%d in %.1f s" % (
        scored, span))
    s.metric("soak_frags", scored)
    for name, c in cpu.items():
        s.metric("soak_cpu_%s" % name, round(c, 3))
    s.note("CPU a second of wall clock over %.1f s: server %.2f, A %.2f, B %.2f%s" % (
        span, cpu["server"], cpu["A"], cpu["B"], " (low impact: idle priority)" if s.low_impact else ""))
    w = b.proxy.window(p0, p1)
    s.metric("soak_b_down_bytes_s", w["down"]["bytes_s"])
    s.note("B's link down: %s" % w["down"])
    # (B13: the server ran at 0.18-0.74 of real time and B's link took 15-65 KB/s; B13b's caps, culled broadcast, spared
    # gib touches and rate cap made them checks)
    s.check("B's link down under %d B/s (his rate cap %d)" % (SOAK_LINK_MAX, SOAK_RATE), w["down"]["bytes_s"] <=
            SOAK_LINK_MAX, "%d B/s" % w["down"]["bytes_s"])
    ratio = SOAK_SECONDS / span
    s.metric("soak_server_realtime_ratio", round(ratio, 3))
    s.check("the server keeps pace: %g s of its game time in under %.1f s of wall clock" % (
        SOAK_SECONDS, SOAK_SECONDS / SOAK_PACE_MIN), ratio >= SOAK_PACE_MIN, "%.1f s (%.2f of real time%s)" % (
        span, ratio, ", low impact" if s.low_impact else ""))
    since_b = srv.mark()
    srv.cmd("vr_net_broadcast_report")
    _, br = srv.wait(r"mp_broadcast: ", 30, since_b, "the server's broadcast report")
    fb = fields(br)
    s.metric("soak_broadcast_sent", int(fb["sent"]))
    s.metric("soak_broadcast_culled", int(fb["culled"]))
    s.check("the broadcast culled by sight (vr_net_cullbroadcast)", int(fb["culled"]) > 0, br.strip())
    hitches = [l.strip() for l in srv.lines[since:] if l.startswith("vr_profile: hitch")]
    s.note("server hitches: %d; the last: %s" % (len(hitches), hitches[-1] if hitches else "-"))
    telefrags = sum(1 for l in a.lines if "was telefragged by" in l)
    s.metric("soak_telefrags", telefrags)
    s.note("telefrags seen at A (the bots in and the window): %d" % telefrags)
    in_window = sum(1 for l in a.lines[a_window:] if "was telefragged by" in l)
    s.metric("soak_telefrags_window", in_window)
    # (B13: ~100-200 of living players in 10 s, the bots standing at dm3's few starts; B13b's respawn beside a start)
    s.check("few telefrags in the window (a respawn beside a taken start)", in_window <= SOAK_TELEFRAGS_MAX,
            "%d" % in_window)
    reports = {}
    for inst in (srv, a, b):
        lines = profile_lines(inst, min(span, 30.0), "%s's profile" % inst.name)
        reports[inst.name] = lines
        with open(os.path.join(s.out, "profile_%s.txt" % inst.name), "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
        head = [l for l in lines if l.startswith("vr_profile_report:") or l.startswith("frame ")]
        s.note("%s: %s" % (inst.name, " | ".join(head)))
    for inst, what in ((a, "A"), (b, "B")):
        f = nethud_report(inst, 1, "%s's net HUD" % what)
        s.note("%s's net HUD: %s / %s" % (what, f["line0"], f["line1"]))
        s.metric("soak_%s_ping" % what, int(f["ping"]))
    left = [l for inst in (a, b) for l in inst.lines if l.startswith("mp_event:") and l.rstrip().endswith(" left")]
    s.check("nobody left the game", not left, "%s" % left)
    if SOAK_MINUTES > 0:
        soak_long(s, srv, a, b, SOAK_MINUTES)
# ---- (B13's end) ----


SOAK_MINUTES = 0           # (B20) --soak-minutes: the long soak after the window (0: none)
SOAK_CHANGE = []           # (B21) --soak-change: the map again after these minutes of it (B24: "20,40"; none: never)
SOAK_HEAP_SITES = ""       # (B22) --heap-sites MIN:MAX: the server's live blocks of that size by call stack (soak_sites.txt)
SOAK_LONG_SAMPLE = 60.0    # its samples, a minute of the server's game time apart
SOAK_LONG_WARMUP = 2       # ... the memory measured from the end of this many (the first minutes fill caches)
SOAK_LONG_GROWTH_MB = 48   # each game's private bytes may grow by at most this after the warm-up
SOAK_LONG_EDICTS_MAX = 2048


def private_mb(inst):
    """A game's private bytes (PROCESS_MEMORY_COUNTERS_EX.PrivateUsage, GetProcessMemoryInfo) in MB."""
    import ctypes
    from ctypes import wintypes

    class PMC(ctypes.Structure):
        _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD)] + [
            (n, ctypes.c_size_t) for n in ("PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage",
                                           "QuotaPagedPoolUsage", "QuotaPeakNonPagedPoolUsage",
                                           "QuotaNonPagedPoolUsage", "PagefileUsage", "PeakPagefileUsage",
                                           "PrivateUsage")]
    pmc = PMC()
    pmc.cb = ctypes.sizeof(PMC)
    ctypes.windll.psapi.GetProcessMemoryInfo(wintypes.HANDLE(int(inst.proc._handle)), ctypes.byref(pmc), pmc.cb)
    return pmc.PrivateUsage / (1024.0 * 1024.0)


def heap_live(inst, out, minute):
    """(B22) A game's live heap blocks by size (vr_heap live): appends (minute, size, pages, blocks, live_kb,
    committed_kb) rows to `out`; returns the live MB."""
    since = inst.mark()
    inst.cmd("vr_heap live 1024; echo heap_live_done")
    _, head = inst.wait(r"vr_heap live: ", 30, since, "%s's live heap" % inst.name)
    inst.wait(r"heap_live_done", 30, since, "%s's live heap rows" % inst.name)
    for l in inst.lines[since:]:
        if l.startswith("vr_heap_live:"):
            f = fields(l)
            out.append((minute, int(f["size"]), int(f["pages"]), int(f["blocks"]), int(f["live_kb"]),
                        int(f["committed_kb"]), int(f.get("reserved_kb", 0)), int(f.get("empty", 0))))
    hm = re.search(r"vr_heap live: ([\d.]+) MB", head)
    return float(hm.group(1)) if hm else -1.0


def soak_long(s, srv, a, b, minutes):
    """(B20) The soak kept going `minutes` minutes of the server's game time, sampled each minute: each game's private
    bytes, the server's edicts and Box3D bodies, its pace (game time a second of wall clock), B's link down, B's moves as
    the server ran them (the move buffer). Leaks and drift: each game's memory grows by at most SOAK_LONG_GROWTH_MB after
    the warm-up, the edicts stay bounded, the server keeps pace every minute, B's link stays under its bound, B's moves
    are not lost, nobody left. Samples: soak_long.txt."""
    rows = []
    live_rows = []  # (B22) (minute, size, pages, blocks, live_kb, committed_kb): soak_live.txt
    changed = {}  # (B21) each client's lines over the map change (--soak-change)
    p_prev = b.proxy.snapshot()
    for m in range(1, minutes + 1):
        t0 = time.monotonic()
        since = srv.mark()
        srv.cmd("vr_mp_after %g soak_minute_%d" % (SOAK_LONG_SAMPLE, m))
        srv.wait(r"mp_after: .*soak_minute_%d\b" % m, SOAK_LONG_SAMPLE * 10, since, "the soak's minute %d" % m)
        span = time.monotonic() - t0
        since = srv.mark()
        srv.cmd("vr_mp_edicts; vr_mp_status")
        _, ed = srv.wait(r"mp_edicts: ", 30, since, "the server's edicts")
        _, bx = srv.wait(r"mp_box3d: ", 30, since, "the server's Box3D bodies")
        _, nm = srv.wait(r"mp_netmoves: slot=1 ", 30, since, "B's moves at the server")
        _, bm = srv.wait(r"mp_box3dmem: ", 30, since, "the server's Box3D memory")
        p = b.proxy.snapshot()
        w = b.proxy.window(p_prev, p)
        p_prev = p
        fn = fields(nm)
        row = {"minute": m, "pace": round(SOAK_LONG_SAMPLE / span, 3), "edicts": int(fields(ed)["used"]),
               "bodies": int(fields(bx)["bodies"]), "down": int(w["down"]["bytes_s"]), "up": int(w["up"]["bytes_s"]),
               "b_lost": int(fn.get("lost", 0)), "b_run": int(fn.get("run", 0)), "b_depth": int(fn.get("depth", 0))}
        # (B21) Box3D's live memory at the server, its contact manifolds' pools, the longest manifold count seen
        fm = fields(bm)
        row.update({"box3d_mb": float(fm.get("box3d", -1)), "pool_mb": float(fm.get("pool_mb", -1)),
                    "long_mb": float(fm.get("long_mb", -1)), "longest": int(fm.get("longest", -1)),
                    "prop_hulls": int(fm.get("prop_hulls", -1)), "gun_hulls": int(fm.get("gun_hulls", -1))})
        for inst in (srv, a, b):
            row["mem_" + inst.name] = round(private_mb(inst), 1)
            # (mimalloc's committed bytes after it returned its cached pages: the heap's own use. The private bytes
            # above also hold the GPU driver's and the allocator's caches, ~100 MB steps either way: a metric only)
            since = inst.mark()
            inst.cmd("vr_heap collect")
            _, hl = inst.wait(r"vr_heap: process working set", 30, since, "%s's heap" % inst.name)
            hm = re.search(r"mimalloc reserved [\d.]+ MB, committed ([\d.]+) MB", hl)
            row["heap_" + inst.name] = float(hm.group(1)) if hm else -1.0
        # (B22) the server's live blocks by size each minute (soak_live.txt): a leak is a size that keeps growing
        row["live_server"] = heap_live(srv, live_rows, m)
        if SOAK_HEAP_SITES and m == SOAK_LONG_WARMUP:
            # (B22) --heap-sites MIN:MAX: the server's blocks of that size traced from here to the end, by call stack
            srv.cmd("vr_heap_sites %s %s" % tuple(SOAK_HEAP_SITES.split(":")))
        rows.append(row)
        s.note("soak minute %d: %s" % (m, " ".join("%s=%s" % kv for kv in row.items() if kv[0] != "minute")))
        if m in SOAK_CHANGE:
            # (B21) the map again mid-soak: what the old map's Box3D world and the rest held comes back
            since_a, since_b, since = a.mark(), b.mark(), srv.mark()
            srv.cmd("changelevel %s; vr_mp_status" % SOAK_MAP)
            a.wait(r"mp_event: .*joined map=%s" % SOAK_MAP, 120, since_a, "A on %s again" % SOAK_MAP)
            b.wait(r"mp_event: .*joined map=%s" % SOAK_MAP, 120, since_b, "B on %s again" % SOAK_MAP)
            # (B22: FrikBot's bots do not follow a changelevel: added again, the minutes after the change stay busy)
            add_bots(a, since_a)
            # (their "left" at a change is the change's, not a drop)
            changed.setdefault(a.name, []).append((since_a, a.mark()))
            changed.setdefault(b.name, []).append((since_b, b.mark()))
            since = srv.mark()
            srv.cmd("vr_physics_mem; vr_heap collect")
            _, bm = srv.wait(r"vr_physics_mem: ", 30, since, "the server's Box3D memory after the change")
            _, hl = srv.wait(r"vr_heap: process working set", 30, since, "the server's heap after the change")
            hm = re.search(r"mimalloc reserved [\d.]+ MB, committed ([\d.]+) MB", hl)
            after = float(hm.group(1)) if hm else -1.0
            heap_live(srv, live_rows, -m)  # (soak_live.txt's minute -m: just after the change)
            s.metric("soak_long_change%d_heap_before_mb" % m, row["heap_server"])
            s.metric("soak_long_change%d_heap_after_mb" % m, after)
            s.metric("soak_long_change%d_box3d_after_mb" % m, float(fields(bm).get("box3d", -1)))
            s.note("soak: the map again after minute %d: the server's heap %.1f -> %.1f MB, Box3D %.2f -> %s MB" % (
                m, row["heap_server"], after, row["box3d_mb"], fields(bm).get("box3d", "?")))
            s.check("the map change gives the old map's Box3D memory back (under 8 MB after it)",
                    0.0 <= float(fields(bm).get("box3d", -1)) < 8.0, bm.strip())
    with open(os.path.join(s.out, "soak_long.txt"), "w", encoding="utf-8") as f:
        for row in rows:
            f.write(" ".join("%s=%s" % kv for kv in row.items()) + "\n")
    if SOAK_HEAP_SITES:
        since = srv.mark()
        srv.cmd("vr_heap_sites stop 30; echo heap_sites_done")
        n, _ = srv.wait(r"heap_sites_done", 120, since, "the server's live heap sites")
        with open(os.path.join(s.out, "soak_sites.txt"), "w", encoding="utf-8") as f:
            f.write("\n".join(srv.lines[since:n]) + "\n")
    with open(os.path.join(s.out, "soak_live.txt"), "w", encoding="utf-8") as f:
        for r in live_rows:
            f.write("minute=%d size=%d pages=%d blocks=%d live_kb=%d committed_kb=%d reserved_kb=%d empty=%d\n" % r)
    warm = rows[min(SOAK_LONG_WARMUP, len(rows)) - 1]
    last = rows[-1]
    for inst in (srv, a, b):
        s.metric("soak_long_mem_%s_growth_mb" % inst.name, round(last["mem_" + inst.name] - warm["mem_" + inst.name], 1))
        k = "heap_" + inst.name
        grow = last[k] - warm[k]
        s.metric("soak_long_%s_growth_mb" % k, round(grow, 1))
        s.check("%s's heap (mimalloc's committed, collected) after the warm-up grows by at most %d MB over %d minutes" % (
            inst.name, SOAK_LONG_GROWTH_MB, minutes), grow <= SOAK_LONG_GROWTH_MB,
            "%.1f -> %.1f MB (minute %d to %d)" % (warm[k], last[k], warm["minute"], last["minute"]))
    s.metric("soak_long_edicts_max", max(r["edicts"] for r in rows))
    s.metric("soak_long_bodies_max", max(r["bodies"] for r in rows))
    s.metric("soak_long_pace_min", min(r["pace"] for r in rows))
    s.metric("soak_long_down_max", max(r["down"] for r in rows))
    s.check("the server's edicts stay under %d every minute" % SOAK_LONG_EDICTS_MAX,
            all(r["edicts"] < SOAK_LONG_EDICTS_MAX for r in rows), "%s" % [r["edicts"] for r in rows])
    s.check("the server keeps pace every minute (%.1f of real time at least)" % SOAK_PACE_MIN,
            all(r["pace"] >= SOAK_PACE_MIN for r in rows), "%s" % [r["pace"] for r in rows])
    s.check("B's link down under %d B/s every minute" % SOAK_LINK_MAX, all(r["down"] <= SOAK_LINK_MAX for r in rows),
            "%s" % [r["down"] for r in rows])
    runs = last["b_run"] + last["b_lost"]
    s.check("B's moves lost at the server under 1%", runs > 0 and last["b_lost"] <= runs * 0.01,
            "%d lost of %d" % (last["b_lost"], runs))
    left = [l for inst in (a, b) for i, l in enumerate(inst.lines) if l.startswith("mp_event:") and
            l.rstrip().endswith(" left") and not any(lo <= i < hi for lo, hi in changed.get(inst.name, []))]
    s.check("nobody left the game over the long soak", not left, "%s" % left[:3])


EMPTY_FLOOD_RATE = 20000  # empty datagrams a second at the server (finding 5: far more than its reads of the socket)
EMPTY_FLOOD_BOUND = 600   # ms: a player seen walking off during the flood (BASE_MS and room for the busy machine)


class Flood:
    """(finding 5) A stranger's UDP datagrams at `port` on this machine, `rate` a second, from a thread: `payload` each
    (b"": empty ones); `sent` counts them."""

    def __init__(self, port, rate, payload=b""):
        import socket
        import threading
        self.to = ("127.0.0.1", port)
        self.rate = rate
        self.payload = payload
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.sent = 0
        self.stop_flag = False
        self.thread = threading.Thread(target=self.run, daemon=True)

    def run(self):
        start = time.monotonic()
        while not self.stop_flag:
            due = int((time.monotonic() - start) * self.rate)
            while self.sent < due:
                try:
                    self.sock.sendto(self.payload, self.to)
                except OSError:
                    pass
                self.sent += 1
            time.sleep(0.001)

    def start(self):
        self.thread.start()
        return self

    def stop(self):
        self.stop_flag = True
        self.thread.join(2)
        self.sock.close()


def emptyflood(s):
    """(review 2026-11-18, finding 5) Empty datagrams at the server's one socket while A and B play. recvfrom's 0 for
    an empty datagram read as "nothing waiting": each one ended a drain of the socket every client's datagrams come to,
    so a stream of them faster than the server's reads queued everyone's moves behind them until the socket's buffer
    dropped them. Now each read skips them (NET_EMPTY_READS at most a call): both players still see each other walk
    off within EMPTY_FLOOD_BOUND ms during the flood, and nobody leaves."""
    srv, a, b, ea, eb = join(s)
    since_a, since_b = a.mark(), b.mark()
    flood = Flood(s.port, EMPTY_FLOOD_RATE).start()
    t0 = time.monotonic()
    try:
        time.sleep(1.0)  # (the socket's buffer full of them before the walks, as a stream of them would leave it)
        walk(s, b, a, eb, "b_walks_flooded", EMPTY_FLOOD_BOUND, EMPTY_FLOOD_BOUND + 400)
        walk(s, a, b, ea, "a_walks_flooded", EMPTY_FLOOD_BOUND, EMPTY_FLOOD_BOUND + 400)
    finally:
        flood.stop()
    took = time.monotonic() - t0
    s.note("flood: %d empty datagrams in %.1f s (%.0f a second)" % (flood.sent, took, flood.sent / max(took, 1e-3)))
    s.check("the flood ran at %d a second or more" % (EMPTY_FLOOD_RATE // 2), flood.sent / max(took, 1e-3) >=
            EMPTY_FLOOD_RATE // 2, "%.0f a second" % (flood.sent / max(took, 1e-3)))
    left = [l.strip() for inst, since in ((a, since_a), (b, since_b)) for l in inst.lines[since:]
            if l.startswith("mp_event:") and l.rstrip().endswith(" left")]
    s.check("nobody left the game during the flood", not left, "%s" % left[:2])
    walk(s, b, a, eb, "b_walks_after", BASE_MS + 200, BASE_MS + 400)


def server_net_version(port):
    """The Quake VR network version of the server on `port`, as a stranger learns it: a connect request of version 0
    from a socket of its own, and the reject's reason ("network version N") -> N (None: no reject read)."""
    import socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0))
    sock.settimeout(2.0)
    try:
        sock.sendto(ctl_packet(bytes([0x01]) + b"QUAKE\0" + bytes([3]) + b"QVR\0" + (0).to_bytes(4, "little") +
                               b"probe\0" + b"probe\0"), ("127.0.0.1", port))
        pkt, _ = sock.recvfrom(2048)
        m = re.search(rb"network version (\d+)", pkt)
        return int(m.group(1)) if m else None
    except OSError:
        return None
    finally:
        sock.close()


def ctl_packet(body):
    return ((0x80000000 | (4 + len(body))).to_bytes(4, "big")) + body


def game_packet(flags, seq, body):
    return ((flags | (8 + len(body))).to_bytes(4, "big")) + (seq & 0xFFFFFFFF).to_bytes(4, "big") + body


def player_info(port, src_ip, players=4):
    """CCREQ_PLAYER_INFO for slots 0..players-1 to the server on `port` from a socket on `src_ip` -> [(name, address)]."""
    import socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((src_ip, 0))
    sock.settimeout(0.5)
    out = []
    try:
        for n in range(players):
            sock.sendto(ctl_packet(bytes([0x03, n])), ("127.0.0.1", port))
            try:
                pkt, _ = sock.recvfrom(2048)
            except OSError:
                continue
            if len(pkt) < 6 or pkt[4] != 0x84:
                continue
            body = pkt[6:]
            name, body = body.split(b"\0", 1)
            body = body[12:]  # (colors, frags, connect time: three longs)
            out.append((name.decode("ascii", "replace"), body.split(b"\0", 1)[0].decode("ascii", "replace")))
    finally:
        sock.close()
    return out


SPOOF_BOUND = BASE_MS + 200  # ms: a player seen walking off after the forgeries


def spoof(s):
    """(review 2026-11-18, finding 2) Forged datagrams in a player's name. B plays through his proxy, which writes from
    the address the server knows him by, so what it forges is what a stranger who learnt his address and port could send
    (UDP's source address is the sender's to write):
    1. connect requests from B's own address and port, his connection long past its first 2 s: NetQuake's server took
       one as B back from a crash and closed his connection; now only a connection silent QVR_GHOST_SILENCE is closed.
    2. unreliable datagrams far ahead of each direction's sequence, and at 0xFFFFFFFE: NetQuake's unsigned compare made
       every later real one stale (B's view frozen); now outside the window, dropped (net_stats: out of window).
    3. reliable chunks down with sequences 0..1023, each a whole message (a stufftext): NetQuake's channels started at 0,
       so one landed on the next sequence and ran; now each side's first sequence is random (network version 39).
    4. CCREQ_PLAYER_INFO: from another address (127.0.0.2) the players' addresses come without ports; from 127.0.0.1
       (the `test` command on the server's own machine) with them."""
    import random as rnd
    srv, a, b, ea, eb = join(s)
    p = b.proxy
    version = server_net_version(s.port)
    s.check("the server's network version, as a stranger reads it from a reject", version is not None, "%s" % version)
    since_srv, since_a, since_b = srv.mark(), a.mark(), b.mark()

    # 1. connect requests from B's address and port
    connect = ctl_packet(bytes([0x01]) + b"QUAKE\0" + bytes([3]) + b"QVR\0" + (version or 0).to_bytes(4, "little") +
                         b"B\0" + b"forged\0" + rnd.getrandbits(32).to_bytes(4, "little"))
    for _ in range(3):
        p.forge_up(connect, True)
        time.sleep(1.0)
    clients = [l for l in status(srv) if l.startswith("mp_client: ") and "spawned=1" in l]
    closed = srv.find(r"the old one closed|connection silent|Client .* removed", since_srv)
    s.check("forged connect requests from B's own address and port: his live connection kept (the server's clients: "
            "A and B)", closed is None and len(clients) == 2,
            "%s; %d clients" % (closed[1].strip() if closed else "not closed", len(clients)))
    walk(s, b, a, eb, "b_walks_after_connects", SPOOF_BOUND, SPOOF_BOUND + 300)

    # 2. unreliable datagrams far ahead, each way
    up, down = p.last_unreliable["up"], p.last_unreliable["down"]
    s.check("the proxy saw both directions' unreliable sequences", up is not None and down is not None,
            "up %s down %s" % (up, down))
    for seq in ((up or 0) + 100000, 0xFFFFFFFE):
        p.forge_up(game_packet(0x00100000, seq, bytes([1])), False)  # (clc_nop)
    for seq in ((down or 0) + 100000, 0xFFFFFFFE):
        p.forge_down(game_packet(0x00100000, seq, bytes([1])))  # (svc_nop)
    walk(s, a, b, ea, "a_seen_after_far_datagrams", SPOOF_BOUND, SPOOF_BOUND + 300)
    walk(s, b, a, eb, "b_seen_after_far_datagrams", SPOOF_BOUND, SPOOF_BOUND + 300)
    since = b.mark()
    b.cmd("net_stats")
    _, line = b.wait(r"out of window\s+=", 10, since, "B's net_stats")
    s.check("B dropped the far datagrams down as outside the window", int(line.split("=")[1]) >= 1, line.strip())

    # 3. reliable chunks down at a blind guess of the sequences
    since_b = b.mark()
    for seq in range(1024):
        p.forge_down(game_packet(0x00010000 | 0x00080000, seq, bytes([9]) + b"echo qvr_forged_chunk\n\0"))
    since = b.mark()
    srv.cmd("vr_net_reliable_test %d 100" % eb)
    got = wait_quietly(b, r"mp_reliable: 100 bytes", 10, since, "B's reliable message after the forged chunks")
    b.read()
    ran = b.find(r"qvr_forged_chunk", since_b)
    s.check("1024 forged reliable chunks (sequences 0..1023): none taken, the server's next message read",
            ran is None and got is not None, "%s; the next message %s" % (
                "ran: " + ran[1].strip() if ran else "none ran", "read" if got else "not read in 10 s"))

    # 4. player info: ports to this machine only
    far = player_info(s.port, "127.0.0.2")
    near = player_info(s.port, "127.0.0.1")
    s.check("player info to another address (127.0.0.2): the players' addresses without their ports",
            len(far) >= 2 and all(":" not in addr for _, addr in far), "%s" % far)
    s.check("player info to 127.0.0.1 (this machine's `test`): with them", len(near) >= 2 and
            all(":" in addr for _, addr in near), "%s" % near)
    forged_connects(s, srv, version)
    left = [l.strip() for inst, since in ((a, since_a), (b, since_b)) for l in inst.lines[since:]
            if l.startswith("mp_event:") and l.rstrip().endswith(" left")]
    s.check("nobody left the game", not left, "%s" % left[:2])


UNHEARD_TIMEOUT_S = 15.0  # s: a server's new client that never acks a chunk is dropped (QVR_UNHEARD_TIMEOUT)


def connect_request(version, name, key, cookie=None):
    body = (bytes([0x01]) + b"QUAKE\0" + bytes([3]) + b"QVR\0" + (version or 0).to_bytes(4, "little") + name + b"\0" +
            key + b"\0" + (0x1234).to_bytes(4, "little"))
    return ctl_packet(body + (cookie.to_bytes(4, "little") if cookie is not None else b""))


def clients_until(srv, want, timeout=3.0):
    """The server's active clients (vr_mp_status) until there are `want` (or `timeout` s) -> the last count."""
    t0 = time.monotonic()
    while True:
        n = sum(1 for l in status(srv) if l.startswith("mp_client: "))
        if n == want or time.monotonic() - t0 > timeout:
            return n
        time.sleep(0.1)


def forged_connects(s, srv, version):
    """(mpfix A1, A2) Connect requests from sockets that never play (a stranger's, or forged with any source):
    1. 16 requests without the server's cookie, each from its own port: each answered by a CCREP_CHALLENGE no bigger
       than the request (NetQuake gave each a slot and resent the serverinfo window for 300 s: 16 filled a server, and a
       spoofed source made it a reflector), no slot given. A wrong cookie: challenged again.
    2. one that echoes its cookie gets a slot (CCREP_ACCEPT), but never acks: one reliable chunk in flight at most, and
       dropped after QVR_UNHEARD_TIMEOUT.
    3. (A2) that player back on another port while his old connection is silent: two requests in one server frame (his
       game retrying). NetQuake closed the old qsocket at the first and gave the freed one to the second, while his old
       slot still held it (two slots on one qsocket); now the old one is marked, its slot drops it, a retry joins once."""
    import socket
    socks = []
    try:
        for _ in range(16):
            sk = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sk.bind(("127.0.0.1", 0))
            sk.setblocking(False)
            socks.append(sk)
        req = connect_request(version, b"stranger", b"0123456789abcdef")
        for sk in socks:
            sk.sendto(req, ("127.0.0.1", s.port))
        time.sleep(1.5)
        got = []
        for sk in socks:
            pkts = []
            while True:
                try:
                    pkts.append(sk.recvfrom(4096)[0])
                except OSError:
                    break
            got.append(pkts)
        challenges = sum(1 for p in got if len(p) == 1 and len(p[0]) == 9 and p[0][4] == 0x86)
        biggest = max([sum(len(x) for x in p) for p in got] or [0])
        s.check("16 connect requests without a cookie: each answered by one CCREP_CHALLENGE (9 bytes), no more bytes "
                "than the %d-byte request" % len(req), challenges == 16 and biggest <= len(req),
                "%d challenged; the most one socket got %d B" % (challenges, biggest))
        clients = [l for l in status(srv) if l.startswith("mp_client: ")]
        s.check("...and no slot given (the server's clients: A and B)", len(clients) == 2, "%d clients" % len(clients))

        sk = socks[0]
        cookie = int.from_bytes(got[0][0][5:9], "little") if got[0] else 0
        sk.sendto(connect_request(version, b"stranger", b"0123456789abcdef", cookie ^ 0x10), ("127.0.0.1", s.port))
        time.sleep(0.5)
        try:
            p = sk.recvfrom(4096)[0]
        except OSError:
            p = b""
        s.check("a wrong cookie: challenged again", len(p) == 9 and p[4] == 0x86, "%d B, kind %s" % (
            len(p), hex(p[4]) if len(p) > 4 else "-"))

        since = srv.mark()
        sk.sendto(connect_request(version, b"stranger", b"0123456789abcdef", cookie), ("127.0.0.1", s.port))
        t0 = time.monotonic()
        accepted = False
        seqs = set()
        biggest = 0
        sk.settimeout(0.5)
        dropped = None
        while time.monotonic() - t0 < UNHEARD_TIMEOUT_S + 10 and dropped is None:
            try:
                p = sk.recvfrom(4096)[0]
            except OSError:
                p = b""
            if len(p) >= 9 and p[4] == 0x81:
                accepted = True
            elif len(p) >= 8 and (int.from_bytes(p[0:4], "big") & 0x00010000):  # (NETFLAG_DATA: a reliable chunk)
                seqs.add(int.from_bytes(p[4:8], "big"))
                biggest = max(biggest, len(p))
            srv.read()
            dropped = srv.find(r"never answered its accept", since)
        took = time.monotonic() - t0
        s.check("its cookie echoed: accepted (a slot)", accepted, "")
        s.check("never acked: one reliable chunk in flight (the serverinfo's first, resent), no window", len(seqs) == 1,
                "%d sequences, the biggest datagram %d B" % (len(seqs), biggest))
        s.check("...and dropped after %.0f s (QVR_UNHEARD_TIMEOUT), not net_messagetimeout's 300" % UNHEARD_TIMEOUT_S,
                dropped is not None and took < UNHEARD_TIMEOUT_S + 5, "%.1f s" % took)

        # 3. a ghost back on another port (A2)
        old, new = socks[1], socks[2]
        key = b"fedcba9876543210"
        def cookie_of(sk2):
            sk2.settimeout(1.0)
            sk2.sendto(connect_request(version, b"ghost", key), ("127.0.0.1", s.port))
            p = sk2.recvfrom(4096)[0]
            return int.from_bytes(p[5:9], "little")
        old.sendto(connect_request(version, b"ghost", key, cookie_of(old)), ("127.0.0.1", s.port))
        first = clients_until(srv, 3)
        time.sleep(3.5)  # (real time: his old connection silent past QVR_GHOST_SILENCE, 3 s)
        c = cookie_of(new)
        since = srv.mark()
        for _ in range(2):
            new.sendto(connect_request(version, b"ghost", key, c), ("127.0.0.1", s.port))
        srv.wait(r"the old one closed", 10, since, "the ghost's old connection closed")
        mid = clients_until(srv, 2)
        new.sendto(connect_request(version, b"ghost", key, c), ("127.0.0.1", s.port))
        after = clients_until(srv, 3)
        s.check("(A2) a ghost back on another port, two requests in one frame: his old slot dropped, none shares its "
                "qsocket (the server's clients %d, then %d, then %d after his retry: 3, 2, 3)" % (first, mid, after),
                (first, mid, after) == (3, 2, 3), "")
    finally:
        for sk in socks:
            sk.close()


BACKOFF_WATCH_S = 16.0   # s B's acks are cut (finding 12: the resends 1 s apart before; now 2 s, then 4 s)
BACKOFF_STALLED = 4.0    # s: the resend timeout's most once two timed resends went unanswered (DGRM_RESEND_STALLED)
BACKOFF_RATE = 6000      # B/s: the server's vr_net_rate_max in the test


def rate_until(srv, eb, want, tries=60):
    """The server's rate for B's snapshots (vr_mp_status's mp_netsnap rate) until it is `want` -> the last read."""
    got = None
    for _ in range(tries):
        got = snap_status(srv, eb).get("rate")
        if got is not None and int(got) == want:
            break
    return got


def backoff(s):
    """(review 2026-11-18, finding 12) The reliable resend's back-off and the server's rate ceiling.
    1. B's acks up cut (the proxy drops everything B sends: a peer that is not reading, a game gone without a word) while
       the server sends him a reliable message: its timed resends (the proxy times each send of each chunk down) were
       capped at 1 s apart, the whole window every second for net_messagetimeout's 300 s; now, after two unanswered,
       2 s then 4 s apart (DGRM_RESEND_STALLED). The acks back: the message after it arrives within the longest wait.
    2. vr_net_rate_max (the server's): B's vr_net_rate 0 ("no cap") is the server's ceiling, one over it is held to
       it, one under it is his own; the server's 0: no ceiling (B's 0 then none)."""
    srv, a, b, ea, eb = join(s)
    p = b.proxy
    for _ in range(4):  # (a few reliable messages: the server measures the round trip)
        since = b.mark()
        srv.cmd("vr_net_reliable_test %d 100" % eb)
        b.wait(r"mp_reliable: 100 bytes", 10, since)
    p.track_chunks_down()
    p.blackout_up = True
    t0 = time.monotonic()
    srv.cmd("vr_net_reliable_test %d 300" % eb)
    try:
        time.sleep(BACKOFF_WATCH_S)
    finally:
        p.blackout_up = False
    sends = {k: list(v) for k, v in (p.chunk_sends or {}).items()}
    p.track_chunks_down(False)
    seq, times = max(sends.items(), key=lambda kv: len(kv[1])) if sends else (None, [])
    gaps = [round(y - x, 2) for x, y in zip(times, times[1:])]
    s.note("the stalled chunk %s: sent %d times in %.0f s, gaps %s" % (seq, len(times), BACKOFF_WATCH_S, gaps))
    s.check("B's acks cut %.0f s: the server's resends of a chunk space out past 1 s up to %.0f s (sent %d times, not "
            "one a second)" % (BACKOFF_WATCH_S, BACKOFF_STALLED, len(times)),
            len(gaps) >= 3 and max(gaps) >= BACKOFF_STALLED - 0.6 and max(gaps) <= BACKOFF_STALLED + 0.6 and
            len(times) <= 2 + BACKOFF_WATCH_S / 2, "gaps %s" % gaps)
    since = b.mark()
    srv.cmd("vr_net_reliable_test %d 100" % eb)
    got = wait_or_none(b, r"mp_reliable: 100 bytes", BACKOFF_STALLED + 4, since)
    s.check("the acks back: the next message arrives within %.0f s" % (BACKOFF_STALLED + 4), got is not None,
            "read" if got else "not read (%.1f s after the cut began)" % (time.monotonic() - t0))

    # 2. the server's ceiling over B's rate
    srv.cmd("vr_net_rate_max %d" % BACKOFF_RATE)
    for own, want in ((0, BACKOFF_RATE), (20000, BACKOFF_RATE), (4000, 4000)):
        b.cmd("vr_net_rate %d" % own)
        got = rate_until(srv, eb, want)
        s.check("vr_net_rate_max %d, B's vr_net_rate %d: the server sends him at most %d B/s" % (BACKOFF_RATE, own, want),
                got is not None and int(got) == want, "rate %s" % got)
    srv.cmd("vr_net_rate_max 0")
    b.cmd("vr_net_rate 0")
    got = rate_until(srv, eb, 0)
    s.check("vr_net_rate_max 0 and B's 0: no cap", got is not None and int(got) == 0, "rate %s" % got)
    b.cmd("vr_net_rate 80000")


class FakeRoomService:
    """(review 2026-11-18, findings 6 and 11) A stand-in for the room service on 127.0.0.1:`port` (the wire format of
    Misc/rendezvous/qvr_rendezvous.py): it records every datagram (time, bytes, source) and answers only as the test
    says (send)."""

    REGISTER, REGISTERED, REFUSED, BADVERSION, COOKIE = 0x01, 0x81, 0x85, 0x86, 0x87

    def __init__(self, port):
        import socket
        import threading
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", port))
        self.sock.settimeout(0.1)
        self.requests = []
        self.stop_flag = False
        self.thread = threading.Thread(target=self.run, daemon=True)

    def run(self):
        while not self.stop_flag:
            try:
                data, src = self.sock.recvfrom(2048)
            except OSError:
                continue
            self.requests.append((time.monotonic(), data, src))

    def start(self):
        self.thread.start()
        return self

    def stop(self):
        self.stop_flag = True
        self.thread.join(2)
        self.sock.close()

    def wait_request(self, kind, after=0, timeout=10.0, test=None):
        """The first request of `kind` from index `after` on (and test(data) true) -> (index, time, data, src); None."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            for i in range(after, len(self.requests)):
                t, data, src = self.requests[i]
                if len(data) >= 18 and data[0:4] == b"QVRR" and data[5] == kind and (test is None or test(data)):
                    return i, t, data, src
            time.sleep(0.02)
        return None

    def send(self, kind, to, nonce, code=b"\0" * 6, extra=None, cookie=None, version=2):
        out = bytearray(32)
        out[0:4] = b"QVRR"
        out[4] = version
        out[5] = kind
        out[8:14] = code
        out[14:18] = nonce
        out[18:22] = bytes(int(x) for x in to[0].split("."))
        out[22:24] = to[1].to_bytes(2, "big")
        if extra is not None:
            out[18] = extra
        if cookie is not None:
            out[24:32] = cookie
        self.sock.sendto(bytes(out), to)


def lookups_of(inst):
    """net_lookups on `inst` -> {name: fields}."""
    since = inst.mark()
    inst.cmd("net_lookups")
    inst.wait(r"net_lookups: \d+ names", 10, since, "%s's net_lookups" % inst.name)
    return {fields(l)["name"]: fields(l) for l in inst.lines[since:] if l.startswith("net_lookup: ")}


LOOKUP_DELAY = 3.0  # s each lookup waits on its worker (vr_net_test_lookup_delay: a slow resolver)
LOOKUP_FRAME = 0.5  # s the server answers its console within meanwhile


def lookup(s):
    """(review 2026-11-18, finding 6) Host names are looked up on a worker (vr_netlookup.cpp), never on the game
    thread: with no working DNS each gethostbyname held a host's frame for the resolver's timeout, seconds.
    1. The room service named "localhost" (a name: the lookup's path), each lookup slowed to LOOKUP_DELAY s: while it
       runs, the dedicated server answers its console within LOOKUP_FRAME s; then it registers with the service.
    2. Every lookup failing (vr_net_test_lookup_fail): "not found" said once, the tries 2 s then 4 s apart (doubling); a remote
       client in: no try however overdue; he leaves: the next try at once.
    3. A server browser favourite by name (localhost:port) is asked once looked up (a search or two)."""
    rport = s.take_port()
    fake = FakeRoomService(rport).start()
    try:
        srv = s.server(dedicated=True, extra=["+vr_net_test_lookup_delay", str(LOOKUP_DELAY)])
        srv.start()
        srv.cmd("coop 1; deathmatch 0; skill 1; map e1m1; vr_mp_status")
        srv.wait(r"mp_server: map=e1m1", 120, what="the server on e1m1")
        t0 = time.monotonic()
        srv.cmd("vr_mp_rendezvous localhost:%d; vr_mp_room 1" % rport)
        worst, n = 0.0, 0
        while time.monotonic() - t0 < LOOKUP_DELAY - 0.5:
            since = srv.mark()
            sent = time.monotonic()
            srv.cmd("echo mp_lookup_ping %d" % n)
            srv.wait(r"^mp_lookup_ping %d" % n, 10, since, "the server's echo")
            worst = max(worst, time.monotonic() - sent)
            n += 1
            time.sleep(0.2)
        looking = lookups_of(srv).get("localhost", {})
        reg = fake.wait_request(FakeRoomService.REGISTER, 0, LOOKUP_DELAY + 10)
        s.check("the room service's name looked up on a worker (%.0f s): the server answered its console within %.1f s "
                "meanwhile (%d echoes), then registered" % (LOOKUP_DELAY, LOOKUP_FRAME, n),
                worst <= LOOKUP_FRAME and n >= 3 and looking.get("state") == "looking" and reg is not None and
                reg[1] - t0 >= LOOKUP_DELAY - 0.5,
                "worst echo %.2f s; the lookup %s; REGISTER %s" % (worst, looking.get("state"),
                                                                    "%.1f s after" % (reg[1] - t0) if reg else "none"))
        srv.cmd("vr_mp_room 0")
        code = srv.stop()
        s.instances.remove(srv)
        s.check("the first server quits cleanly", code == 0, "exit %s" % code)

        # 2. failing lookups: the back-off, and none while a remote client is in
        srv = s.server(dedicated=True, extra=["+vr_net_test_lookup_fail", "1"], name="server2")  # (a log of its own)
        srv.start()
        srv.cmd("coop 1; deathmatch 0; skill 1; map e1m1; vr_mp_status")
        srv.wait(r"mp_server: map=e1m1", 120, what="the failing server on e1m1")
        since = srv.mark()
        srv.cmd("vr_mp_rendezvous localhost:%d; vr_mp_room 1" % rport)
        seen = []
        for _ in range(9):
            e = lookups_of(srv).get("localhost", {})
            if e and (not seen or seen[-1] != (e.get("tries"), e.get("wait"))):
                seen.append((e.get("tries"), e.get("wait")))
            time.sleep(1.0)
        waits = [float(w) for _, w in seen if float(w) > 0]
        told = [l for l in srv.lines[since:] if "was not found" in l]
        s.check("failing lookups tried again 2 s, then 4 s apart (doubling), said once",
                waits[:2] == [2.0, 4.0] and len(told) == 1, "tries/waits %s; %d lines" % (seen, len(told)))
        a = s.client("A").start()
        a.cmd("name A; " + s.connect_cmd(a))
        a.wait(r"mp_event: .*joined map=e1m1", 120, what="A joined")
        e = lookups_of(srv).get("localhost", {})
        tries, due = int(e.get("tries", 0)), float(e.get("next", 0))
        time.sleep(min(due, 40.0) + 1.5)
        e2 = lookups_of(srv).get("localhost", {})
        s.check("a remote client in: no lookup started however overdue", int(e2.get("tries", -1)) == tries and
                float(e2.get("next", -1)) == 0.0, "tries %s then %s; next %s then %s" % (
                    tries, e2.get("tries"), due, e2.get("next")))
        a.cmd("disconnect")
        again = None
        for _ in range(30):
            e3 = lookups_of(srv).get("localhost", {})
            if int(e3.get("tries", 0)) > tries:
                again = e3
                break
            time.sleep(0.2)
        s.check("he left: the next try at once", again is not None, "%s" % (again or e3))
        srv.cmd("vr_mp_room 0")

        # 3. a favourite by name in A's server browser
        here = "localhost:%d" % s.port
        since = a.mark()
        a.cmd('vr_mp_recent ""; vr_mp_favourites ""; vr_mp_favourite %s' % here)
        a.wait(r"mp_browser: favourite %s on" % re.escape(here), 60, since, "the favourite set")
        answered = None
        for k in range(4):
            r = row_of(browse(a, "A's search %d" % k)[0], here)
            if r and r["answered"] == "1":
                answered = k + 1
                break
        s.check("a favourite by name (%s) answers A's search once looked up" % here, answered is not None,
                "after %s searches" % answered)
    finally:
        fake.stop()


def roomnonce(s):
    """(review 2026-11-18, finding 11) A host's room replies authenticated by his request's nonce. NetQuake-style trust of
    the source address let anyone forge the service's REFUSED (no registration for 60 s) or BADVERSION (600 s), and the
    shared code died 2 minutes later. A stand-in service (FakeRoomService) answers the dedicated server's REGISTER:
    1. REFUSED, BADVERSION and REGISTERED with another nonce: no line, no code.
    2. COOKIE with his nonce (version 2's cookie round): his next REGISTER at once, the cookie in it.
    3. REGISTERED with that request's nonce: his room code. 4. REFUSED with it: taken (said)."""
    rport = s.take_port()
    fake = FakeRoomService(rport).start()
    try:
        srv = s.server(dedicated=True)
        srv.start()
        srv.cmd("coop 1; deathmatch 0; skill 1; map e1m1; vr_mp_status")
        srv.wait(r"mp_server: map=e1m1", 120, what="the server on e1m1")
        since = srv.mark()
        srv.cmd("vr_mp_rendezvous 127.0.0.1:%d; vr_mp_room 1" % rport)
        first = fake.wait_request(FakeRoomService.REGISTER, 0, 30)
        if first is None:
            raise StepFailed("no REGISTER from the server")
        i, _, data, src = first
        s.check("the host's REGISTER is of version 2, no cookie yet", data[4] == 2 and not any(data[18:26]),
                "version %d, cookie %s" % (data[4], data[18:26].hex()))
        other = bytes(x ^ 0x5A for x in data[14:18])
        fake.send(FakeRoomService.REFUSED, src, other, extra=1)
        fake.send(FakeRoomService.BADVERSION, src, other, extra=9)
        fake.send(FakeRoomService.REGISTERED, src, other, code=b"ZZZZZZ")
        srv.cmd("echo mp_roomnonce_forged")
        srv.wait(r"^mp_roomnonce_forged", 10, since, "the server's frame after the forgeries")
        time.sleep(0.5)
        srv.read()
        acted = [l.strip() for l in srv.lines[since:] if "has no room" in l or "speaks version" in l or "Room code" in l]
        s.check("REFUSED, BADVERSION and REGISTERED with another nonce: ignored", not acted, "%s" % acted[:2])
        # the cookie round, to his latest request
        latest = fake.wait_request(FakeRoomService.REGISTER, len(fake.requests), 5)  # (a new one: 2 s to its next)
        if latest is None:
            raise StepFailed("no REGISTER to answer")
        k, _, data, src = latest
        sent = time.monotonic()
        fake.send(FakeRoomService.COOKIE, src, data[14:18], cookie=b"qvrcook8")
        again = fake.wait_request(FakeRoomService.REGISTER, k + 1, 5, lambda d: d[18:26] == b"qvrcook8")
        s.check("COOKIE with his nonce: he registers again at once with the cookie", again is not None and
                again[1] - sent < 1.0, "%.2f s" % (again[1] - sent) if again else "not with it")
        if again is None:
            return
        _, _, data, src = again
        fake.send(FakeRoomService.REGISTERED, src, data[14:18], code=b"ABCDEF")
        _, line = srv.wait(r"Room code: ABCDEF", 10, since, "his room code")
        s.check("REGISTERED with his nonce: his code", True, line.strip())
        fake.send(FakeRoomService.REFUSED, src, data[14:18], extra=1)
        _, line = srv.wait(r"has no room for this game", 10, since, "the refusal")
        s.check("REFUSED with his nonce: taken", True, line.strip())
        srv.cmd("vr_mp_room 0")
    finally:
        fake.stop()


# ---------------------------------------------------------------------------------------------------------------------
# The sync audit (MP_PLAN.md, "Sync audit 2026-11-21"): what of B's player another player gets. B does a thing; B's own
# game and A's each list what they draw, light and hear round B (vr_mp_near: the models, dynamic lights, sounds); what B
# has of himself and A has not of him is a gap, measured. A report, not a regression: every probe "passes" when both
# reports came; the gaps are in the notes, summary.json's metrics and syncaudit_table.txt.

SYNC_RADIUS = 80.0   # units round B's origin
SYNC_SPOT = OPEN_SPOT
SYNC_GAP = 200.0     # B this far north of A (the way open ahead), facing him: A's own things out of the radius


def near_lines(inst, ent, seconds):
    since = inst.mark()
    inst.cmd("vr_mp_near %d %g %g" % (ent, SYNC_RADIUS, seconds))
    n, _ = inst.wait(r"mp_near: done", 10, since, "%s's vr_mp_near" % inst.name)
    return inst.lines[since:n + 1]


def near_counts(lines):
    """vr_mp_near's lines -> {"model:<name>" | "faded:<name>" (drawn see-through) | "glow:<name>" (a powerup's glow on
    it) | "wounded:<name>" (a painted wounds mask on it, syncfix5) | "light:r<radius/10*10>" | "sound:<name>": count}."""
    c = {}
    for line in lines:
        line = line.strip()
        keys = []
        m = re.match(r"mp_near_ent: whose=\S+ model=(\S+) d=\S+ alpha=(\d+) .* glow=(\S+)(?: wound=(\d))?", line)
        if m:
            keys.append("model:" + m.group(1))
            keys += ["faded:" + m.group(1)] if 0 < int(m.group(2)) < 255 else []
            keys += ["glow:" + m.group(1)] if float(m.group(3)) != 0.0 else []
            keys += ["wounded:" + m.group(1)] if m.group(4) == "1" else []
        m = re.match(r"mp_near_light: key=-?\d+ radius=(\d+)", line)
        keys += ["light:r%d" % (int(m.group(1)) // 10 * 10)] if m else []
        m = re.match(r"mp_near_snd: name=(\S+)", line)
        keys += ["sound:" + m.group(1)] if m else []
        for k in keys:
            c[k] = c.get(k, 0) + 1
    return c


def minus(a, b):
    """What `a` has more of than `b` -> {key: how many more}."""
    return {k: n - b.get(k, 0) for k, n in a.items() if n > b.get(k, 0)}


def sync_wait(srv, seconds):
    server_time_past(srv, server_time(srv) + seconds)


def sync_punch(s, srv, a, b, ea, eb):
    b.cmd("vr_mock_hand main 0.00 1.15 -0.20 0 0 0; vr_mock_button main grip 1")
    sync_wait(srv, 0.3)
    play(s, b, "syncpunch", PUNCH_PLAY)
    sync_wait(srv, 1.2)


def sync_punch_off(s, srv, a, b, ea, eb):
    b.cmd("vr_mock_play; vr_mock_button main grip 0")
    sync_wait(srv, 0.5)


def sync_shoot(s, srv, a, b, ea, eb):
    arm_with(s, srv, b, eb, 4, "give s 20")
    b.cmd("vr_mock_hand main 0.15 1.2 -0.40 70 0 0")
    sync_wait(srv, 0.3)
    a.cmd("vr_debug_weaponfx 2")  # (mpopen: A's copy of B's shotgun drawn stroking: "autopump player N hand H drawn")
    since_a = a.mark()
    b.cmd("+attack")
    sync_wait(srv, 0.15)
    b.cmd("-attack")
    sync_wait(srv, 0.4)
    a.cmd("vr_debug_weaponfx 0; echo mp_shoot_watched")
    n, _ = a.wait(r"mp_shoot_watched", 10, since_a, "A's stroke lines")
    backs = [float(m.group(1)) for m in (re.search(r"autopump player %d hand \d drawn t \S+ travel (\S+)" % eb, l)
                                         for l in a.lines[since_a:n]) if m]
    s.check("shoot: B's auto pump drawn stroking on his copy at A (its fore-end back, then home)",
            len(backs) >= 3 and max(backs) > 1.0, "%d frames drawn so, back at most %.2f units" % (
                len(backs), max(backs) if backs else 0.0))


def sync_walk(s, srv, a, b, ea, eb):
    b.cmd("+back" if b.flat else "vr_mock_stick off 0 -1")  # (backwards: away from A)
    sync_wait(srv, 1.0)


def sync_walk_off(s, srv, a, b, ea, eb):
    b.cmd("-back" if b.flat else "vr_mock_stick off 0 0")


def sync_nothing(s, srv, a, b, ea, eb):
    sync_wait(srv, 1.0)


def sync_cheat(impulse):
    """The Debug menu's powerup cheats (impulse 190 quad, 191 pentagram, 192 ring, 193 biosuit: an hour; syncfix: in coop
    they give the powerups again, the harness's holster gives moved to 90-95)."""
    def on(s, srv, a, b, ea, eb):
        b.cmd("impulse %d" % impulse)
        sync_wait(srv, 1.0)
    return on


def sync_hurt(s, srv, a, b, ea, eb):
    """syncfix5: B hurt by A twice (vr_hurt 8, A the attacker: a blow's wound, QVR_SVC_WOUND, which every client gets):
    his own body and hands painted (yours), and A's copy of him (the copy's capsules, the pool's masks)."""
    # (syncfix6: vr_hurt's broadcast -- the wound -- now reaches the clients: a console command's sv.datagram writes were
    # dropped by the frame's SV_ClearDatagram, server::commandBroadcastBegin/End; the row needs B's own wound too)
    a.cmd("vr_wounds_debug 2")  # (their "wounds:" lines in the logs: what was received and painted where)
    b.cmd("vr_wounds_debug 2")
    # (mpopen: his gore on his copy too -- his stream, its pour heard by A, his drips; each stream's chance made sure)
    a.cmd("vr_player_gore_stream_chance 1")
    b.cmd("vr_player_gore_stream_chance 1")
    for _ in range(2):
        srv.cmd("vr_hurt 8 %d %d" % (eb - 1, ea))
        sync_wait(srv, 0.4)
    sync_wait(srv, 0.8)  # (the streams land: their pour)
    a.cmd("vr_wounds_debug 0")
    b.cmd("vr_wounds_debug 0")
    since_a = a.mark()
    a.cmd("vr_player_gore_info")
    _, line = a.wait(r"player gore others: ", 10, since_a, "A's player gore counters")
    m = re.search(r"others: (\d+) wounds bound, (\d+) streams opened \((\d+) pouring\), (\d+) drips, (\d+) bleeding", line)
    s.check("hurt: B's gore on his copy at A (his wounds bound, a stream opened, drips falling)",
            m is not None and int(m.group(1)) >= 1 and int(m.group(2)) >= 1 and int(m.group(4)) >= 1, line.strip())
    a.cmd("vr_player_gore_stream_chance 0.5")
    b.cmd("vr_player_gore_stream_chance 0.5")


def no_gap(prefix, drawn_only=False, need_own=False, exact=False):
    """A synced row: nothing of `prefix` ("glow:", "faded:", "light:", ...) that B has of himself and A has not of him;
    and A has some (the probe did show it). `drawn_only`: only of the models A draws of him at all (a model he lacks is
    another row's gap). `need_own`: B must have some himself (else the probe showed nothing: a fail, not a pass).
    `exact`: that key alone (mpopen: "light:r20" also began "light:r200", a muzzle flash's random radius)."""
    def hit(k):
        return k == prefix if exact else k.startswith(prefix)

    def check(gap, seen_added, seen, own):
        missing = sorted(k for k in gap if hit(k) and
                         (not drawn_only or "model:" + k.split(":", 1)[1] in seen))
        got = sorted(k for k in seen_added if hit(k))
        mine = any(hit(k) for k in own)  # (B has none himself: a flat player's gadget, say)
        return not missing and (bool(got) or not mine) and (mine or not need_own),"B only: %s; A has: %s%s" % (missing or "-", got or "-", "" if mine else " (B has none)")
    return check


def seen_has(prefix):
    """A row B's own list cannot show (his own view's parts): A has some of `prefix` round him."""
    def check(gap, seen_added, seen, own):
        got = sorted(k for k in seen_added if k.startswith(prefix))
        return bool(got), "A has: %s" % (got or "-")
    return check


def any_both(prefix):
    """A row whose keys vary run to run on each side (a sound's random variant): B has some of `prefix` and A some."""
    def check(gap, seen_added, seen, own):
        mine = sorted(k for k in own if k.startswith(prefix))
        got = sorted(k for k in seen_added if k.startswith(prefix))
        return bool(mine) and bool(got), "B: %s; A: %s" % (mine or "-", got or "-")
    return check


SYNC_PROBES = (  # (name, B's action, its undo, the synced rows' checks); idle first: the baseline
    ("idle", sync_nothing, None, (("the holster sheaths", no_gap("model:legholster.mdl")),  # (syncfix batch 2: worn gear)
                                  ("the wrist gadget", no_gap("model:vrgadget.mdl")),
                                  ("its straps", no_gap("model:vrgadget_strap.mdl")),
                                  ("the grenade pouch", no_gap("model:vrpouch.mdl")),
                                  ("the ammo pouch", no_gap("model:vrpouch_ammo.mdl")),
                                  # (syncfix2: the rounds in it, the gadget screen's two lights)
                                  ("the rounds in the ammo pouch", no_gap("model:vr_shell_live")),
                                  ("the gadget screen's glow", no_gap("light:r10")),
                                  ("the gadget screen's beam", no_gap("light:r30")))),
    # (syncfix5; before the shot: vr_hurt's blow has no hit record of its own, and a shotgun's left over makes it a pellet's,
    # whose wounds come by the pellets alone)
    ("hurt", sync_hurt, None, (("blood on his copy", no_gap("wounded:", drawn_only=True, need_own=True)),
                               # (mpopen: his stream's pour, a client's own sound, heard by A from his copy's stream)
                               ("his blood's pour heard", any_both("sound:vr/gore/pour")))),
    # (they last the run: the probes after them compare against idle as before)
    ("quad", sync_cheat(190), None, (("Quad's glow on his copy", no_gap("glow:")), ("its hand lights", no_gap("light:r1")))),
    ("ring", sync_cheat(192), None, (("the ring's fade on his copy", no_gap("faded:", drawn_only=True)),)),
    ("punch", sync_punch, sync_punch_off, ()),
    ("shoot", sync_shoot, None, (("his spent casings", no_gap("model:vr_shell.mdl")),  # (syncfix2: batch 3)
                                 # (syncfix5: the auto pump's clacks, his ammo screen's light, his barrel's heat light)
                                 ("the auto pump's clacks", no_gap("sound:vr/autopump")),
                                 ("his ammo screen's light", no_gap("light:r20", exact=True)),
                                 ("his barrel's heat light", no_gap("light:r30", exact=True)))),
    ("walk", sync_walk, sync_walk_off, ()),  # (last: B walks off his spot)
)


def syncaudit(s, flat):
    s.client_sound = True
    srv, a, b, ea, eb = join(s, flat=flat)
    put_at(srv, b, eb, (SYNC_SPOT[0], SYNC_SPOT[1] + SYNC_GAP, SYNC_SPOT[2]), 270)
    put_at(srv, a, ea, SYNC_SPOT, 90)
    sync_wait(srv, 1.0)
    base_b = base_a = None
    rows_out = []
    for name, act, undo, rows in SYNC_PROBES:
        t0 = time.time()
        act(s, srv, a, b, ea, eb)
        seconds = time.time() - t0 + 0.3
        own = near_counts(near_lines(b, 0, seconds))
        seen = near_counts(near_lines(a, eb, seconds))
        if base_b is None:
            base_b, base_a = own, seen
            gap, extra = minus(own, seen), minus(seen, own)
            both = {k: n for k, n in own.items() if k in seen}
        else:  # (what the action added on each side, against the idle baseline)
            add_b, add_a = minus(own, base_b), minus(seen, base_a)
            gap, extra = minus(add_b, add_a), minus(add_a, add_b)
            both = {k: n for k, n in add_b.items() if k in add_a}
        s.metric("sync_%s" % name, {"own": own, "seen": seen, "gap": gap, "extra": extra, "both": both})
        s.note("%s: B has, A not: %s | A has, B not: %s | both: %s" % (name, gap or "-", extra or "-", both or "-"))
        rows_out.append("%-6s gap(B only): %s\n%-6s extra(A only): %s\n%-6s both: %s" % (
            name, gap or "-", "", extra or "-", "", both or "-"))
        s.check("%s: both reports came (B's own view of himself, A's of B)" % name, bool(own) and bool(seen),
                "B %d kinds, A %d kinds" % (len(own), len(seen)))
        for row, check in rows:  # (syncfix: the audit's gaps fixed, each row's own check)
            ok, detail = check(gap, add_a if base_b is not own else seen, seen, own)
            s.check("%s: %s synced" % (name, row), ok, detail)
        if undo:
            undo(s, srv, a, b, ea, eb)
    # A bot's holsters and what it carries (syncfix batch 2; his voice note 2026-11-21): A adds a coop bot; what A draws
    # round it: its holster sheaths placed on its body by A's own client (no client of its own drew them). It stands
    # still where it spawned (vr_bot_freeze, syncfix3: the pictures below).
    srv.cmd("vr_bot_freeze 1")
    a.cmd("impulse 100")
    table = {}
    for _ in range(10):
        since_a = a.mark()
        table = frag_table(a)
        if len(table) >= 3:
            break
        wait_quietly(a, r"mp_frags: ", 3, since_a, "A's scoreboard")
    bots = sorted(slot for slot in table if slot not in (ea - 1, eb - 1))
    seen_bot, own_sheaths = {}, 0
    for _ in range(10):  # (until A's copy of it is posed: its sheaths go on its body as drawn)
        sync_wait(srv, 0.5)
        lines = near_lines(a, bots[0] + 1, 1.0) if bots else []
        seen_bot = near_counts(lines)
        # (its own: within 30 units of its origin -- B's, if near, stand farther)
        own_sheaths = sum(1 for l in lines if re.search(r"model=legholster\.mdl d=(\S+)", l) and
                          float(re.search(r"model=legholster\.mdl d=(\S+)", l).group(1)) <= 30.0)
        if own_sheaths >= 4:
            break
    s.metric("sync_bot", seen_bot)
    s.note("bot: A draws round it: %s" % (seen_bot or "-"))
    s.check("bot: its holster sheaths on its copy at A (4: the hips', the uppers')", own_sheaths >= 4,
            "%d within 30 units; %s" % (own_sheaths, seen_bot or "no bot"))
    # (syncfix2, syncfix3) A looks at the bot from its front, its two sides and its back, the bot standing still
    # (vr_bot_freeze, from where it spawned: the start's lit room) and A facing its middle: the sheaths and what they
    # hold on its body, for a person to judge (bot_holsters_<side>.png).
    shots = []
    at = byaw = None
    if bots:
        since = srv.mark()  # (a bot has no mp_client line: its edict's origin and angles)
        srv.cmd("edict %d; echo mp_edict_done" % (bots[0] + 1))
        n, _ = srv.wait(r"mp_edict_done", 10, since, "the bot's edict")
        for l in srv.lines[since:n]:
            m = re.search(r"^\s*(origin|angles)\s+'\s*([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)", l)
            if m and m.group(1) == "origin":
                at = [float(m.group(i)) for i in (2, 3, 4)]
            elif m:
                byaw = float(m.group(3))
    if at is not None:
        byaw = byaw or 0.0
        s.note("bot at %s facing %.0f (frozen)" % (at, byaw))
        a.cmd("vr_flashlight_clip_head right; vr_flashlight 1")  # (A's head torch on it: lit from where A looks)
        for side, off in (("front", 0), ("left", 90), ("back", 180), ("right", 270)):
            r = math.radians(byaw + off)  # (A stands that way from it and looks back at it)
            try:  # (a side against a wall: none from there; an aid, not a check)
                put_at(srv, a, ea, (at[0] + 60 * math.cos(r), at[1] + 60 * math.sin(r), at[2]),
                       (byaw + off + 180) % 360, tries=15)
            except StepFailed:
                s.note("bot: no picture from its %s (no room there)" % side)
                continue
            sync_wait(srv, 0.5)
            shots.append(a.screenshot("bot_holsters_" + side))
        a.cmd("vr_flashlight 0")
    srv.cmd("vr_bot_freeze 0")
    s.note("bot holster screenshots: %s" % (", ".join(str(p) for p in shots) or "-"))
    with open(os.path.join(s.out, "syncaudit_table.txt"), "w", encoding="ascii") as f:
        f.write("B %s, A VR mock, dedicated server; radius %g round B\n" % ("flat" if flat else "VR mock", SYNC_RADIUS))
        f.write("\n".join(rows_out + ["bot    A draws round it: %s" % (seen_bot or "-")]) + "\n")


def run(args, test, lag=0.0, jitter=0.0, loss=0.0):
    # (testspeed) the games start without the hub (mp_harness: -nostartmap; its clients select Quake's campaign as the
    # hub did: flatgreen3, mp_flow, mp_predict and mp_predictclimb need no hub since). QVR_MP_STARTMAP=1 in the
    # environment: the hub for every scenario.
    # (mpopen) the scenario's block of ports free first: a game left over (or another run's) holding its server port made
    # the next scenarios' servers exit at start; the next free block then, said
    port, busy = free_base_port(args.port)
    s = Session(args.exe, args.base, args.out, test, port=port)
    if busy:
        s.note("ports %s held at the start (a game left over or another run): this scenario on %d" % (busy, port))
        print("  note: %s: ports %s held at the start; on %d instead" % (test, busy, port), flush=True)
    s.server_extra = args.server_set.split() if args.server_set else []  # (B13c: e.g. "+vr_net_sendhz 36")
    failure = None
    try:
        if test == "mp_bandwidth":
            bandwidth(s)
        elif test == "mp_armed":
            armed(s)
        elif test == "mp_retransmit":
            retransmit(s)
        elif test == "mp_version":
            version(s)
        elif test in ("mp_pole", "mp_pole_vr"):
            pole(s, vr_viewer=test == "mp_pole_vr")
        elif test in ("mp_monkey", "mp_monkey_flat"):
            monkey(s, vr_viewer=test == "mp_monkey")
        elif test == "mp_avatarcal":
            avatarcal(s)
        elif test == "mp_ropes":
            ropes(s)
        elif test == "mp_ropeclimb":
            ropeclimb(s)
        elif test == "mp_avatar":
            avatar(s)
        elif test == "mp_body":
            body(s)
        elif test == "mp_prefs":
            prefs(s)
        elif test == "mp_guard":
            guard(s)
        elif test == "mp_cmdguard":
            cmdguard(s)
        elif test == "mp_grab":
            grab(s)
        elif test == "mp_campfollow":
            campfollow(s)
        elif test == "mp_navfollow":
            navfollow(s)
        elif test == "mp_navfollow_listen":
            navfollow(s, listen=True)
        elif test == "mp_navfollow_vr":
            navfollow(s, flat="A")
        elif test == "mp_botnavdebug":
            botnavdebug(s)
        elif test == "mp_botnavdebug_listen":
            botnavdebug(s, listen=True)
        elif test == "mp_teleport":
            teleport(s)
        elif test == "mp_held":
            held(s)
        elif test == "mp_wpnsync":
            wpnsync(s)
        elif test == "mp_wpnsync_flat":
            wpnsync(s, flat="A")
        elif test == "mp_ground":
            ground(s)
        elif test == "mp_campaign":
            campaign(s)
        elif test == "mp_campaign_listen":
            campaign_listen(s)
        elif test == "mp_flow":
            flow(s)
        elif test == "mp_rejoin":
            rejoin(s)
        elif test == "mp_rejoin_keep":
            rejoin_keep(s)
        elif test == "mp_download":
            download(s)
        elif test == "mp_pickup":
            pickup(s)
        elif test == "mp_moves":
            moves(s)
        elif test == "mp_melee":
            melee(s)
        elif test == "mp_blade":
            melee(s, only_blade=True)
        elif test == "mp_climb":
            climb(s)
        elif test == "mp_forcegrab":
            forcegrab(s)
        elif test == "mp_weaponwheel":
            weaponwheel(s)
        elif test == "mp_rules":
            rules(s)
        elif test == "mp_cosmetics":
            cosmetics(s)
        elif test == "mp_flatbody":
            flatbody(s)
        elif test == "mp_slide":
            slide(s)
        elif test == "mp_slide_vr":
            slide(s, flat=False)
        elif test == "mp_adspose":
            adspose(s)
        elif test == "mp_deathbody":
            deathbody(s)
        elif test == "mp_ragsync":
            ragsync(s)
        elif test == "mp_loss":
            lossy(s)
        elif test == "mp_predict":
            predict(s)
        elif test == "mp_predictslide":
            predict_slide(s)
        elif test == "mp_predictclimb":
            predict_climb(s)
        elif test == "mp_disarm":
            disarm(s)
        elif test == "mp_lagcomp":
            lagcomp(s)
        elif test == "mp_lockstep":
            lockstep(s)
        elif test == "mp_lockstep_dm":
            lockstep_dm(s)
        elif test == "mp_browser":
            browser(s)
        elif test == "mp_nethud":
            nethud(s)
        elif test == "mp_dedicated":
            dedicated(s)
        elif test == "mp_vanish":
            vanish(s)
        elif test == "mp_jitter":
            jitter_lift(s)
        elif test == "mp_fuzz":
            fuzz(s)
        elif test == "mp_guests":
            guests(s)
        elif test == "mp_nat":
            nat(s)
        elif test == "mp_emptyflood":
            emptyflood(s)
        elif test == "mp_spoof":
            spoof(s)
        elif test == "mp_backoff":
            backoff(s)
        elif test == "mp_lookup":
            lookup(s)
        elif test == "mp_roomnonce":
            roomnonce(s)
        elif test == "mp_netsafe":
            netsafe(s)
        elif test == "mp_roomui":
            roomui(s)
        elif test == "mp_soak":
            soak(s)
        elif test == "mp_lockstep_throw":
            lockstep_throw(s)
        elif test == "mp_dm":
            deathmatch(s)
        elif test == "mp_botgear":
            botgear(s)
        elif test == "mp_variantowned":
            variantowned(s)
        elif test == "mp_holstersync":
            holstersync(s)
        elif test == "mp_attach":
            attach(s)
        elif test == "mp_botrevolver":
            botrevolver(s)
        elif test == "mp_deathlook":
            deathlook(s)
        elif test == "mp_botgrab":
            botgrab(s)
        elif test == "mp_debugviz":
            debugviz(s, False)
        elif test == "mp_debugviz_listen":
            debugviz(s, True)
        elif test == "mp_coopbots":
            coopbots(s)
        elif test == "mp_wristscore":
            wristscore(s)
        elif test == "mp_syncaudit":
            syncaudit(s, args.flat or "")
        elif test in ("mp_voice", "mp_voice_lossy"):
            voice(s, args.listen, lag, jitter, loss)
        else:
            coop(s, args.listen, args.flat or "", lag, jitter, loss)
    except StepFailed as e:
        failure = e
    except Exception as e:  # (a scenario's own bug: its games still stopped, the verdict still printed)
        traceback.print_exc()
        failure = StepFailed("the scenario failed: %r" % e)
    return s.finish(failure)


def print_bytes_table():
    """B's link per scenario (MP_PLAN B7, B12): up and down, and down by kind (vr_net_bytes, scaled to the link's rate):
    poses (in the snapshot + outside it), entities, avatars, his own state (player state + walk), the snapshot's header,
    sounds and temp entities, the rest of the unreliable, the reliable."""
    print("mp_bytes: scenario, B's link up B/s (mean B), down B/s (mean B); down by kind B/s: pose ent av own hdr snd+te rest rel")
    for test, w in LINKS:
        k = w.get("kinds") or {}
        g = lambda *names: sum(k.get(n, 0) for n in names)
        print("mp_bytes: %-20s up %6d (%4d)  down %6d (%4d)  %6d %6d %6d %6d %5d %5d %5d %5d" % (
            test, w["up"]["bytes_s"], w["up"]["mean_B"], w["down"]["bytes_s"], w["down"]["mean_B"],
            g("snap_pose", "pose"), g("snap_ent", "ent"), g("snap_av", "avatar"), g("snap_cd", "snap_pred", "cd"),
            g("snap_hdr"), g("sound", "tent"), g("rest"), g("reliable")))


SUITE = [
    ("mp_coop", ()),
    ("mp_coop_lag", (150, 0, 0.02)),
    ("mp_bandwidth", ()),
    ("mp_armed", ()),
    ("mp_retransmit", ()),
    ("mp_version", ()),
    ("mp_dedicated", ()),
    ("mp_avatar", ()),
    ("mp_avatarcal", ()),
    ("mp_ropes", ()),
    ("mp_ropeclimb", ()),
    ("mp_pole", ()),
    ("mp_monkey", ()),
    ("mp_body", ()),
    ("mp_prefs", ()),
    ("mp_grab", ()),
    ("mp_guard", ()),
    ("mp_teleport", ()),
    ("mp_held", ()),
    ("mp_wpnsync", ()),
    ("mp_wpnsync_flat", ()),
    ("mp_ground", ()),
    ("mp_campaign", ()),
    ("mp_campaign_listen", ()),
    ("mp_campfollow", ()),
    ("mp_flow", ()),
    ("mp_rejoin", ()),
    ("mp_rejoin_keep", ()),
    ("mp_download", ()),
    ("mp_pickup", ()),
    ("mp_forcegrab", ()),
    ("mp_weaponwheel", ()),
    ("mp_melee", ()),
    ("mp_moves", ()),
    ("mp_climb", ()),
    ("mp_rules", ()),
    ("mp_cosmetics", ()),
    ("mp_flatbody", ()),
    ("mp_slide", ()),
    ("mp_slide_vr", ()),
    ("mp_adspose", ()),
    ("mp_deathbody", ()),
    ("mp_deathlook", ()),
    ("mp_loss", ()),
    ("mp_predict", ()),
    ("mp_predictclimb", ()),
    ("mp_predictslide", ()),
    ("mp_lagcomp", ()),
    ("mp_cmdguard", ()),
    ("mp_disarm", ()),
    ("mp_voice", ()),
    ("mp_voice_lossy", (50, 30, 0.05)),
    ("mp_dm", ()),
    ("mp_coopbots", ()),
    ("mp_botgrab", ()),
    ("mp_botgear", ()),
    ("mp_holstersync", ()),
    ("mp_attach", ()),
    ("mp_variantowned", ()),
    ("mp_botrevolver", ()),
    ("mp_debugviz", ()),
    ("mp_debugviz_listen", ()),
    ("mp_wristscore", ()),
    ("mp_lockstep", ()),
    ("mp_lockstep_dm", ()),
    ("mp_lockstep_throw", ()),
    ("mp_browser", ()),
    ("mp_nethud", ()),
    ("mp_vanish", ()),
    ("mp_fuzz", ()),
    ("mp_guests", ()),
    ("mp_jitter", ()),
    ("mp_nat", ()),
    ("mp_emptyflood", ()),
    ("mp_spoof", ()),
    ("mp_backoff", ()),
    ("mp_lookup", ()),
    ("mp_roomnonce", ()),
]  # (mp_netsafe runs first, before these; the rest in this order. The tuples: lag ms, jitter ms, loss)
# --profile long (testspeed, 2026-11-22): the soaks and diagnostics, a quarter of the suite's time, run only there and in
# --profile all (releases, nightly): adspose (screenshots for a person, measurements), fuzz (the corrupting proxy),
# rejoin_keep, voice_lossy. The default profile is the rest. --only runs the names it gives whatever the profile.
LONG = {"mp_adspose", "mp_fuzz", "mp_rejoin_keep", "mp_voice_lossy"}


def in_profile(name, profile):
    return profile == "all" or (name in LONG) == (profile == "long")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("test", choices=["coop", "bandwidth", "armed", "retransmit", "version", "avatar", "avatarcal", "body", "prefs", "grab", "guard", "teleport", "held", "ground", "campaign", "campaign_listen", "campfollow", "flow", "rejoin", "download", "pickup", "forcegrab", "weaponwheel", "melee", "blade", "moves", "climb", "rules", "cosmetics", "flatbody", "slide", "slide_vr", "adspose", "deathbody", "deathlook", "loss", "predict", "predictclimb", "predictslide", "lagcomp", "cmdguard", "disarm", "voice", "dm", "lockstep", "lockstep_dm", "lockstep_throw", "browser", "nethud", "vanish", "fuzz", "guests", "dedicated", "soak", "jitter", "nat", "netsafe", "roomui", "suite", "emptyflood", "spoof", "backoff", "lookup", "roomnonce", "rejoin_keep", "coopbots", "botgrab", "botgear", "holstersync", "variantowned", "botrevolver", "debugviz", "debugviz_listen", "wristscore", "wpnsync", "wpnsync_flat", "navfollow", "navfollow_listen", "navfollow_vr", "botnavdebug", "botnavdebug_listen", "syncaudit", "ragsync", "ropes", "ropeclimb", "pole", "pole_vr", "monkey", "monkey_flat", "attach"])
    tree_base = default_base(REPO)
    ap.add_argument("--base", required=tree_base is None, default=tree_base,
                    help="the game folder (id1, quakevr...; default: this checkout's own, the kit's bases/<checkout>/qbase)")
    ap.add_argument("--exe", default=EXE)
    ap.add_argument("--out", default=os.path.join(REPO, "scratch", "mp"))
    ap.add_argument("--port", type=int, default=default_base_port(REPO),
                    help="the scenarios' base port (default: this checkout's own block, 27600 on; a busy block: the next free one)")
    ap.add_argument("--listen", action="store_true", help="client A hosts a listen server (else a dedicated server)")
    ap.add_argument("--flat", default="", help="which clients play flatscreen: A, B or AB")
    ap.add_argument("--lag", type=float, default=0.0, help="client B's added one-way delay, ms")
    ap.add_argument("--jitter", type=float, default=0.0, help="client B's delay varies by this much, ms")
    ap.add_argument("--loss", type=float, default=0.0, help="client B's game packet loss, 0..1, each way")
    ap.add_argument("--reset-autoexec", action="store_true", help="empty <base>/id1/autoexec.cfg first")
    ap.add_argument("--server-set", default="", help="the dedicated server's extra command line (e.g. \"+vr_net_sendhz 36\")")
    ap.add_argument("--permove-skip", type=int, default=0,
                    help="predictclimb: the server's vr_net_permove_skip (1 the hands' climb out, 2 the grapple: left to "
                         "QuakeC once a frame; its part should then fail)")
    ap.add_argument("--part", default="", choices=["", "lift", "waterjump", "swim", "crawl", "hang", "mantle", "airgrab", "waterclimb", "grapple", "give", "use", "samename", "signon", "inject", "injectup"],
                    help="predict: only this segment (traced with vr_mp_watch 1; --watch 2); predictclimb: hang, mantle, airgrab, waterclimb or grapple; disarm: give or use, the rounds alone")
    ap.add_argument("--seed", type=int, default=0, help="fuzz: the proxy's corruptions from this seed (0: a new one, printed)")
    ap.add_argument("--tracelog", type=int, default=0, help="lockstep: both sides' traces of B's move N (vr_mp_predict_tracelog)")
    ap.add_argument("--repeat", type=int, default=1, help="predict --part: takes in one run (lift: one)")
    ap.add_argument("--watch", type=int, default=1, help="predict --part: vr_mp_watch on the server and B (3: corrections over 0.05 traced)")
    ap.add_argument("--soak-minutes", type=int, default=0, help="soak: keep going this many minutes of the server's game time after the window, sampled each minute (B20: leaks, drift)")
    ap.add_argument("--heap-sites", default="", help="soak --soak-minutes: MIN:MAX, the server's live blocks of that size by call stack from the warm-up on (B22: vr_heap_sites)")
    ap.add_argument("--soak-change", default="", help="soak --soak-minutes: the map again after this minute, or these (\"20,40\") (B21: does the memory come back)")
    ap.add_argument("--only", default="", help="suite: just these scenarios, comma-separated (\"mp_dm,mp_lagcomp\"; mp_netsafe always first)")
    ap.add_argument("--profile", default="default", choices=["default", "long", "all"],
                    help="suite: default (without the soaks and diagnostics in LONG), long (just those), all (releases)")
    ap.add_argument("--suite-repeat", type=int, default=1, help="suite: its scenarios this many times over in one run")
    ap.add_argument("--load", type=int, default=0, help="(B25) N busy CPU workers at normal priority for the whole run "
                    "(mp_load.py: other builds and tests on the machine, the suite's real condition; 0: none)")
    ap.add_argument("--load-duty", type=float, default=1.0, help="--load: each worker busy this share of the time")
    args = ap.parse_args()
    load = Load(args.load, args.load_duty).start()
    try:
        main_run(args)
    finally:
        load.stop()


def main_run(args):
    global SOAK_CHANGE, SOAK_HEAP_SITES
    SOAK_CHANGE = [int(x) for x in args.soak_change.split(",") if x.strip() and int(x) > 0]
    SOAK_HEAP_SITES = args.heap_sites
    global PART, PART_REPEAT, PART_WATCH, PART_JITTER, PART_LOSS, AMMO_PART, PERMOVE_SKIP, TRACELOG, SOAK_MINUTES
    PART, PART_REPEAT, PART_WATCH = args.part, max(1, args.repeat), max(1, args.watch)
    PART_JITTER, PART_LOSS = args.jitter, args.loss
    SOAK_MINUTES = max(0, args.soak_minutes)
    PERMOVE_SKIP = args.permove_skip
    TRACELOG = args.tracelog
    global FUZZ_SEED
    FUZZ_SEED = args.seed
    AMMO_PART = args.part if args.part in ("give", "use") else ""
    autoexec = os.path.join(args.base, "id1", "autoexec.cfg")
    # (The checkout's own base, the default: its autoexec is the last run.sh run's script, rewritten by each: emptied.)
    tree_base = default_base(REPO)
    if args.reset_autoexec or (tree_base and os.path.normcase(os.path.abspath(args.base)) == os.path.normcase(os.path.abspath(tree_base))):
        with open(autoexec, "w", encoding="ascii") as f:
            f.write("// emptied by mp_test.py --reset-autoexec (the harness drives its games through -cmdinbox)\n")
    elif os.path.exists(autoexec):
        with open(autoexec, encoding="ascii", errors="replace") as f:
            if any(l.strip().split(";")[0] in ("quit", "map", "disconnect") or l.startswith("map ") for l in f):
                print("mp_test: %s runs a script (a kit run's: map/quit); --reset-autoexec empties it" % autoexec)
                sys.exit(2)
    if args.test == "coop":
        ok = run(args, "mp_coop", args.lag, args.jitter, args.loss)
    elif args.test in ("bandwidth", "wpnsync", "wpnsync_flat", "armed", "retransmit", "version", "avatar", "avatarcal", "body", "prefs", "grab", "guard", "teleport", "held", "ground",
                       "campaign", "campaign_listen", "campfollow", "flow", "rejoin", "download", "pickup", "forcegrab", "melee", "blade", "moves", "climb", "rules", "cosmetics", "flatbody", "slide", "slide_vr", "adspose", "deathbody", "deathlook", "loss", "predict", "predictclimb", "predictslide", "lagcomp", "cmdguard", "disarm", "dm", "lockstep", "lockstep_dm", "lockstep_throw", "browser", "nethud", "vanish", "fuzz", "guests", "dedicated", "soak", "jitter", "nat", "netsafe", "roomui", "emptyflood", "spoof", "backoff", "lookup", "roomnonce", "rejoin_keep", "coopbots", "botgrab", "botgear", "holstersync", "variantowned", "botrevolver", "debugviz", "debugviz_listen", "wristscore", "navfollow", "navfollow_listen", "navfollow_vr", "botnavdebug", "botnavdebug_listen", "syncaudit", "ragsync", "ropes", "ropeclimb", "pole", "pole_vr", "monkey", "monkey_flat", "attach"):
        ok = run(args, "mp_" + args.test)
    elif args.test == "voice":
        ok = run(args, "mp_voice", args.lag, args.jitter, args.loss)
    else:
        only =[x.strip() for x in args.only.split(",") if x.strip()]
        unknown = [x for x in only if x not in [n for n, _ in SUITE] + ["mp_netsafe"]]
        if unknown:
            print("mp_suite: --only names no suite scenario: %s" % unknown)
            sys.exit(2)
        ok = run(args, "mp_netsafe")  # (B24: first: the fences that keep every game off the internet; always)
        if not ok:
            print("mp_suite: FAILED (mp_netsafe: the network fences are broken; nothing else run)")
            sys.exit(1)
        for _ in range(max(1, args.suite_repeat)):
            for name, link in SUITE:
                if (name in only) if only else in_profile(name, args.profile):
                    ok = run(args, name, *link) and ok
        # B's link per scenario (MP_PLAN B7: byte counts against mp_bandwidth's): what the next phases cut.
        print_bytes_table()
        print("mp_suite: %s" % ("passed" if ok else "FAILED"))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
