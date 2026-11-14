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
    version  a client of another Quake VR network version (-mpnetversion 1) is rejected with the reason; one of the
             same version joins.
    avatar   B has A's avatar (QVR_SVC_AVATAR): head and drawn hands within 2 units of the server's, the main hand's
             angles within 3 degrees, his weapon; none of himself; A closes his off hand: its finger curls at B within a
             step of the server's, closed; B draws A's jointed hands (and his weapon's model) where A drew them; the
             bytes it costs and the avatars the server sent and skipped as unchanged.
    avatarcal  each client's avatar calibration (height, arm length, gloves, body...) reaches the other and the server.
    body     B draws A's body (avatar::poseOther: the code that poses A's own): A's build's model, its wrists at A's drawn
             hands, its joints (head, pelvis, shoulders, wrists) within 1.5 units of A's own body and its scale A's; a
             screenshot pair (B facing A); A's Height raised: B's copy grows by as much (its scale and shoulders), as A's own.
    prefs    per-player preferences (vr_clientprefs.cpp): A on the Sticky weapon grip, B on Hold (the server's own):
             each given a shotgun with his grips let go, A keeps it and B's falls; then the other way round.
    grab     the rig up: the server has each player's fists as his client poses them; B's shotgun falls from his
             open hand and he closes it on it: taken, judged by his own fist on the dedicated server.
    teleport client-side traces: B (a remote client) traces his own map down to his floor; he aims a teleport with his off
             hand (his own trace: valid) and lets go: the server moves him there, A sees him there.
    held     the held weapon's drawn pose up: B holds a shotgun still; the server places its hotspots where B's view
             draws them; B's off hand takes its grip hotspot: the server's two-handed hold of it is B's own.
    download a dedicated server on a Map Library package's map: A (it installed) mounts it and joins; B (lacking it) is
             told with vr_mp_download 0, and with 1 installs it through the map installer (a local index and mirror)
             and joins.
    suite    coop, coop with B at 150 ms each way and 2% loss, bandwidth, version, avatar, avatarcal, body, prefs, grab,
             teleport, held.

The base folder is a Quake folder with id1 (and hipnotic, rogue, quakevr: -game as the kit's runs). Its
id1/autoexec.cfg runs in every game: --reset-autoexec empties it first (the kit's test bases: a run's script).
The last line is the verdict, "mp_<test>: N passed, M failed".
"""

import argparse
import math
import os
import re
import sys
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from mp_harness import Session, StepFailed, fields  # noqa: E402

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


def join(s, listen=False, flat="", lag=0.0, jitter=0.0, loss=0.0):
    """A server on e1m1 (coop 1) and clients A and B joined, each seeing the other -> (server, a, b, ea, eb)."""
    if listen:
        a = srv = s.server(dedicated=False, flat="A" in flat, name="A")
    else:
        srv = s.server(dedicated=True)
    srv.start()
    # (a listen host: maxplayers and listen before the map, its start-up map (vrstart, a server of 1) shut down first;
    # a dedicated server has its -dedicated 4 slots)
    srv.cmd("%scoop 1; deathmatch 0; skill 1; map e1m1; vr_mp_status" % (
        "disconnect; maxplayers 4; listen 1; " if listen else ""))
    srv.wait(r"mp_server: map=e1m1", 120, what="the server on e1m1")
    if not listen:
        a = s.client("A", flat="A" in flat).start()
    # (B always through a proxy: with no lag it only counts B's traffic)
    b = s.client("B", flat="B" in flat, lag_ms=lag, jitter_ms=jitter, loss=loss, proxy=True).start()
    if not listen:
        a.cmd(s.connect_cmd(a))
    b.cmd(s.connect_cmd(b))

    _, ja = a.wait(r"mp_event: .*joined map=e1m1", 120, what="A joined e1m1")
    _, jb = b.wait(r"mp_event: .*joined map=e1m1", 120, what="B joined e1m1")
    ea, eb = int(fields(ja)["self"]), int(fields(jb)["self"])
    s.check("A and B joined coop e1m1", ea != eb, "A is entity %d, B %d" % (ea, eb))

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


IDLE_BUDGET = 25 * 1024  # B's link down, both players idle on e1m1 (MP_PLAN.md, section 2.3: B2's target)
IDLE_PACKETS = 360       # the window: five seconds of 72 Hz datagrams
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
    s.check("B's link down idle on e1m1 under %d B/s" % IDLE_BUDGET, down < IDLE_BUDGET,
            "%d B/s (%d B/s with every pose to every client)" % (down, windows[0]["down"]["bytes_s"]))
    most = proxy.stats["down"][3]
    s.check("every packet down at most %d B (no reliable chunk over %d B)" % (1400 + NET_HEADER, RELIABLE_CHUNK),
            most <= 1400 + NET_HEADER, "largest %d B" % most)
    srv.cmd("vr_net_cullposes 1")


def version(s):
    """A client of another Quake VR network version (-mpnetversion) is rejected with the reason, and a right one
    still joins after it."""
    srv = s.server(dedicated=True)
    srv.start()
    srv.cmd("coop 1; deathmatch 0; skill 1; map e1m1; vr_mp_status")
    srv.wait(r"mp_server: map=e1m1", 120, what="the server on e1m1")
    c = s.client("C", extra=["-mpnetversion", "1"]).start()
    c.cmd(s.connect_cmd(c))
    _, why = c.wait(r"network version \d+, your game 1", 60, what="C's reject")
    s.check("a client of another network version is told why", True, why.strip())
    _, logged = srv.wait(r"Rejected a connection", 10, what="the server's note of it")
    s.check("the server notes the rejected client", True, logged.strip())
    a = s.client("A").start()
    a.cmd(s.connect_cmd(a))
    a.wait(r"mp_event: .*joined map=e1m1", 120, what="A joined e1m1")
    s.check("a client of the same network version joins", True)


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
                max(abs(x - y) for x, y in zip(got[ea]["curl0"], sv[ea]["curl0"])) <= CURL_STEP + 1e-4:
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
    b.cmd("vr_mock_button main grip 0")
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
        b.cmd("vr_mock_button main grip 0")
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
        if own0.get("carrying") == 1 and sv0.get("carrying") == 1 and wpn and int(wpn[0]) == 0:
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
            ns = re.search(r"\bnspeed=(-?[\d.]+)", l)
            if ns:
                out[(int(m.group(1)), int(m.group(2)))]["nspeed"] = float(ns.group(1))
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
    b.cmd("vr_mock_button main grip 0")
    seen = weapon_until(s, srv, eb, 0)
    s.check("B's shotgun falls from his opened hand", seen[-1] == 0, "B's main weapon by report: %s" % seen)

    # His main hand over its grip hotspot as it lies (placed again each report: it may still slide), within the view's
    # reach of a ground spot (15 cm).
    above = "vr_mock_hand_to main spot %d 6" % grips[0]
    own_g = sv_g = {}
    for _ in range(30):
        b.cmd(above)
        own_g = ground_report(b, "mp_drawn_own").get((eb, 1), {})
        sv_g = ground_report(srv, "mp_drawn_sv").get((eb, 1), {})
        if own_g.get("gs", (0, -1))[0] > 0 and sv_g.get("gs") == own_g.get("gs"):
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
    b.cmd("vr_mock_button off grip 0")
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
    def over(turn):
        def place():
            since = b.mark()
            b.cmd("vr_mock_hand off -0.1 1.0 -0.35 %s; echo mp_over_turned" % turn)
            b.wait(r"mp_over_turned", 10, since, "B's off hand turned")
            b.cmd("vr_mock_hand_to off spot -1 6")
        return place

    def agreed():
        """B's own and the server's reports of his off hand until the server has the hand where his view has it (its
        angles within 3 degrees: the move with the new turn arrived) and the same choice (at most 20 reports each)."""
        o = v = {}
        for _ in range(20):
            o = ground_report(b, "mp_drawn_own").get((eb, 0), {})
            v = ground_report(srv, "mp_drawn_sv").get((eb, 0), {})
            if o.get("rev", (0, -1, 0))[0] > 0 and v.get("rev", (0,))[0] == o["rev"][0] and v["rev"][1] == o["rev"][1] \
                    and angle_diff(o["rot"], v["rot"]) <= 3.0:
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


INV_KEYS = ("health", "armor", "items", "shells", "nails", "rockets", "cells", "weapon")
IT_ROCKET_LAUNCHER = 32


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


def inventories_until(srv, done, tries=60):
    """inventories(srv) until done(inv, map) -> (inv, map) (the last, at most `tries` reports)."""
    inv, mapname = {}, ""
    for _ in range(tries):
        inv, mapname = inventories(srv)
        if done(inv, mapname):
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
    take B's slot, the first free one) with what he had."""
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


def saved_text(s, relname, t0, tries=50):
    """The save `relname` (".sav" added) in a game folder under the base, written since `t0` (wall clock) and whole
    (its last line the closing brace) -> (path, text); (None, "") if none came (at most `tries` looks a tenth apart)."""
    for _ in range(tries):
        for d in os.listdir(s.base):
            path = os.path.join(s.base, d, relname + ".sav")
            if os.path.isfile(path) and os.path.getmtime(path) >= t0 - 1:
                text = open(path, encoding="latin-1").read()
                # (whole: its last edict's brace, or the edict count after it)
                if "\n// qvr_coop " in text and (text.rstrip().endswith("}") or re.search(r"// \d+ edicts\s*$", text)):
                    return path, text
        time.sleep(0.1)
    return None, ""


def own_status(inst):
    """inst's own `mp_status:` fields (its map, intermission, health as its client sees them)."""
    return fields(next((l for l in status(inst) if l.startswith("mp_status:")), ""))


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
    since_a = a.mark()
    a.cmd("vr_autosave_list cold")
    wait_quietly(a, r"vr_autosave_list: \d+ autosaves", 10, since_a, "A's Load Autosave list")
    row = a.find(r"autosave \d+: autosave/coop~e1m2 \|.*Coop, 2 players", since_a)
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
        a.wait(r"map index: ", 60, what="A's map index (its cache) read")
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
        b.wait(r"map index: ", 60, what="B's map index (its cache) read")  # (else he would fetch Quaddicted's)
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


def run(args, test, lag=0.0, jitter=0.0, loss=0.0):
    s = Session(args.exe, args.base, args.out, test, port=args.port)
    failure = None
    try:
        if test == "mp_bandwidth":
            bandwidth(s)
        elif test == "mp_version":
            version(s)
        elif test == "mp_avatarcal":
            avatarcal(s)
        elif test == "mp_avatar":
            avatar(s)
        elif test == "mp_body":
            body(s)
        elif test == "mp_prefs":
            prefs(s)
        elif test == "mp_grab":
            grab(s)
        elif test == "mp_teleport":
            teleport(s)
        elif test == "mp_held":
            held(s)
        elif test == "mp_ground":
            ground(s)
        elif test == "mp_campaign":
            campaign(s)
        elif test == "mp_campaign_listen":
            campaign_listen(s)
        elif test == "mp_flow":
            flow(s)
        elif test == "mp_download":
            download(s)
        else:
            coop(s, args.listen, args.flat or "", lag, jitter, loss)
    except StepFailed as e:
        failure = e
    except Exception as e:  # (a scenario's own bug: its games still stopped, the verdict still printed)
        traceback.print_exc()
        failure = StepFailed("the scenario failed: %r" % e)
    return s.finish(failure)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("test", choices=["coop", "bandwidth", "version", "avatar", "avatarcal", "body", "prefs", "grab", "teleport", "held", "ground", "campaign", "campaign_listen", "flow", "download", "suite"])
    ap.add_argument("--base", required=True, help="the game folder (id1, quakevr...)")
    ap.add_argument("--exe", default=EXE)
    ap.add_argument("--out", default=os.path.join(REPO, "scratch", "mp"))
    ap.add_argument("--port", type=int, default=27500)
    ap.add_argument("--listen", action="store_true", help="client A hosts a listen server (else a dedicated server)")
    ap.add_argument("--flat", default="", help="which clients play flatscreen: A, B or AB")
    ap.add_argument("--lag", type=float, default=0.0, help="client B's added one-way delay, ms")
    ap.add_argument("--jitter", type=float, default=0.0, help="client B's delay varies by this much, ms")
    ap.add_argument("--loss", type=float, default=0.0, help="client B's game packet loss, 0..1, each way")
    ap.add_argument("--reset-autoexec", action="store_true", help="empty <base>/id1/autoexec.cfg first")
    args = ap.parse_args()
    autoexec = os.path.join(args.base, "id1", "autoexec.cfg")
    if args.reset_autoexec:
        with open(autoexec, "w", encoding="ascii") as f:
            f.write("// emptied by mp_test.py --reset-autoexec (the harness drives its games through -cmdinbox)\n")
    elif os.path.exists(autoexec):
        with open(autoexec, encoding="ascii", errors="replace") as f:
            if any(l.strip().split(";")[0] in ("quit", "map", "disconnect") or l.startswith("map ") for l in f):
                print("mp_test: %s runs a script (a kit run's: map/quit); --reset-autoexec empties it" % autoexec)
                sys.exit(2)
    if args.test == "coop":
        ok = run(args, "mp_coop", args.lag, args.jitter, args.loss)
    elif args.test in ("bandwidth", "version", "avatar", "avatarcal", "body", "prefs", "grab", "teleport", "held", "ground",
                       "campaign", "campaign_listen", "flow", "download"):
        ok = run(args, "mp_" + args.test)
    else:
        ok = run(args, "mp_coop")
        ok = run(args, "mp_coop_lag", 150, 0, 0.02) and ok
        ok = run(args, "mp_bandwidth") and ok
        ok = run(args, "mp_version") and ok
        ok = run(args, "mp_avatar") and ok
        ok = run(args, "mp_avatarcal") and ok
        ok = run(args, "mp_body") and ok
        ok = run(args, "mp_prefs") and ok
        ok = run(args, "mp_grab") and ok
        ok = run(args, "mp_teleport") and ok
        ok = run(args, "mp_held") and ok
        ok = run(args, "mp_ground") and ok
        ok = run(args, "mp_campaign") and ok
        ok = run(args, "mp_campaign_listen") and ok
        ok = run(args, "mp_flow") and ok
        ok = run(args, "mp_download") and ok
        print("mp_suite: %s" % ("passed" if ok else "FAILED"))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
