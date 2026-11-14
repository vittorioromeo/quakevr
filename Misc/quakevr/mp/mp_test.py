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
    suite    coop, coop with B at 150 ms each way and 2% loss, bandwidth, version, avatar, avatarcal, body, prefs, grab.

The base folder is a Quake folder with id1 (and hipnotic, rogue, quakevr: -game as the kit's runs). Its
id1/autoexec.cfg runs in every game: --reset-autoexec empties it first (the kit's test bases: a run's script).
The last line is the verdict, "mp_<test>: N passed, M failed".
"""

import argparse
import math
import os
import re
import sys
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


def grip_then_give(srv, inst, grips):
    """`grips` (the mock's grip buttons, its hands) on `inst`, then the test give of a shotgun (impulse 154: 150 + WID 4)
    once the server has his main grip pressed. The give in the same frame as the press could reach the server a move
    before the grip (the move's grip bit is the client's next frame's): the default grip-to-hold let it fall at once
    (B3's mp_avatar 14/1, A's main hand empty). Leaves the server's grip log on (vr_debug_carry 2, developer 1)."""
    srv.cmd("vr_debug_carry 2; developer 1")
    since = srv.mark()
    inst.cmd(grips)
    srv.wait(r"grip: .*main hand pressed at", 10, since, "%s's main grip on the server" % inst.name)
    inst.cmd("impulse 154")


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


def grab(s):
    """The rig up (vr_clientprefs.cpp "vr_rig"): the dedicated server has each player's fists as his client poses them
    (within the 64ths of a unit they are sent in); B lets a given shotgun fall from his low hand (Hold) and closes his
    hand on it: the server takes it into his hand by his own fist (its count of grabs judged by a remote player's fist)."""
    srv, a, b, ea, eb = join(s)
    own_b = own_a = {}
    got = {}
    for _ in range(20):
        own_a, own_b, got = own_prefs(a), own_prefs(b), prefs_report(s, srv)
        ok = all(got.get(e, {}).get("fist%d" % h, 0) > 0 and
                 got[e]["fist%d" % h] == own["fist%d" % h] and
                 abs(got[e]["fistsum%d" % h] - own["fistsum%d" % h]) <= own["fist%d" % h] * 4 / 128.0 + 0.01
                 for e, own in ((ea, own_a), (eb, own_b)) for h in (0, 1))
        if ok:
            break
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
    seen, presses = [], 0
    for presses in range(1, 4):
        _, gone = srv.wait(r"grip: .*main hand let go at", 10, since, "B's let-go on the server")
        server_time_past(srv, float(gone.rsplit(" ", 1)[1]) + GRIP_REPRESS_S)
        b.cmd("vr_mock_button main grip 1")
        seen += b_weapon_until(4, 8)
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
    ap.add_argument("test", choices=["coop", "bandwidth", "version", "avatar", "avatarcal", "body", "prefs", "grab", "suite"])
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
    elif args.test in ("bandwidth", "version", "avatar", "avatarcal", "body", "prefs", "grab"):
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
        print("mp_suite: %s" % ("passed" if ok else "FAILED"))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
