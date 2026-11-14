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
    suite    coop, coop with B at 150 ms each way and 2% loss, bandwidth, version, avatar.

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
    a.cmd("vr_mock_button main grip 1; vr_mock_button off grip 1; impulse 154")
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
    ap.add_argument("test", choices=["coop", "bandwidth", "version", "avatar", "avatarcal", "suite"])
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
    elif args.test in ("bandwidth", "version", "avatar", "avatarcal"):
        ok = run(args, "mp_" + args.test)
    else:
        ok = run(args, "mp_coop")
        ok = run(args, "mp_coop_lag", 150, 0, 0.02) and ok
        ok = run(args, "mp_bandwidth") and ok
        ok = run(args, "mp_version") and ok
        ok = run(args, "mp_avatar") and ok
        ok = run(args, "mp_avatarcal") and ok
        print("mp_suite: %s" % ("passed" if ok else "FAILED"))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
