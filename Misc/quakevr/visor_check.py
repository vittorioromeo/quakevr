# visor_check.py <log>: visor_test.sh's verdicts, from the vr_flat_visor_report lines after each "visor_test: <phase>".
import re
import sys

log = open(sys.argv[1], encoding="utf-8", errors="replace").read().replace("\r", "")
# The console wraps long lines in the run's output: joined back (a report line ends at "log lines N").
log = re.sub(r" +", " ", re.sub(r"\n(?!visor|composed|exit|$)", " ", log))

phases = {}
phase = None
for line in log.split("\n"):
    m = re.match(r"visor_test: (.+)$", line.strip())
    if m:
        phase = m.group(1).strip()
        phases.setdefault(phase, {"panels": {}})
        continue
    if phase is None:
        continue
    m = re.match(r"visor: active (\d) frame \d+; sway ([-\d.]+) ([-\d.]+) \(lag ([-\d.]+) ([-\d.]+), bob ([-\d.]+)\); "
                 r"glitch ([-\d.]+) fog ([-\d.]+) underwater (\d) fade ([-\d.]+) alarm ([-\d.]+) boot ([-\d.]+) ([-\d.]+) ([-\d.]+)",
                 line.strip())
    if m and "active" not in phases[phase]:
        v = [float(x) for x in m.groups()]
        phases[phase].update(active=int(v[0]), sway=(v[1], v[2]), glitch=v[6], fog=v[7], underwater=int(v[8]),
                             boot=(v[11], v[12], v[13]))
        continue
    m = re.match(r"visor: shake ([-\d.]+) \(([-\d.]+) ([-\d.]+)\) logfade ([-\d.]+) glass (\d)", line.strip())
    if m and "shake" not in phases[phase]:
        v = [float(x) for x in m.groups()]
        phases[phase].update(shake=v[0], shakeoff=(v[1], v[2]), logfade=v[3], glass=int(v[4]))
        h = re.search(r"hits (\d+) health (-?\d+)", line)
        if h:
            phases[phase].update(hits=int(h.group(1)), health=int(h.group(2)))
        continue
    m = re.match(r"visor: panel (\w+) drawn (\d) rect ([-\d.]+) ([-\d.]+) ([-\d.]+) ([-\d.]+) contact (\d) "
                 r"\(view (\d+) x (\d+)\)", line.strip())
    if m and m.group(1) not in phases[phase]["panels"]:
        phases[phase]["panels"][m.group(1)] = (int(m.group(2)), *(float(x) for x in m.groups()[2:6]),
                                               int(m.group(8)), int(m.group(9)), int(m.group(7)))

passed = failed = 0


def check(ok, what):
    global passed, failed
    print(("PASS " if ok else "FAIL ") + what)
    if ok:
        passed += 1
    else:
        failed += 1


def get(p, key, default=None):
    return phases.get(p, {}).get(key, default)


def sway(p):
    s = get(p, "sway", (99.0, 99.0))
    return max(abs(s[0]), abs(s[1]))


rest = phases.get("rest", {"panels": {}})
NAMES = ("vitals", "status", "lefthand", "righthand", "log", "sideleft", "sideright")
inside = len(rest["panels"]) == 7 and all(
    d == 1 and -0.01 * w <= x0 < x1 <= 1.01 * w and -0.01 * h <= y0 < y1 <= 1.01 * h
    for (d, x0, y0, x1, y1, w, h, c) in rest["panels"].values())  # (1%: a lens and a zoom can put a box a little past an edge)
check(get("rest", "active") == 1 and inside, f"rest: the visor on by default, its seven panels inside the view ({rest['panels']})")
if inside:
    P = rest["panels"]
    cx = lambda q: (q[1] + q[3]) * 0.5 / q[5]  # noqa: E731 (its middle, in shares of the view)
    cy = lambda q: (q[2] + q[4]) * 0.5 / q[6]  # noqa: E731
    where = {"vitals": (cx(P["vitals"]) < 0.2 and cy(P["vitals"]) < 0.2), "status": (cx(P["status"]) > 0.8 and cy(P["status"]) < 0.2),
             "lefthand": (cx(P["lefthand"]) < 0.2 and cy(P["lefthand"]) > 0.8),
             "righthand": (cx(P["righthand"]) > 0.8 and cy(P["righthand"]) > 0.8),
             "log": (abs(cx(P["log"]) - 0.5) < 0.01 and cy(P["log"]) < 0.15),
             "sideleft": (cx(P["sideleft"]) < 0.1 and abs(cy(P["sideleft"]) - 0.5) < 0.02),
             "sideright": (cx(P["sideright"]) > 0.9 and abs(cy(P["sideright"]) - 0.5) < 0.02)}
    check(all(where.values()), f"rest: each panel in its place ({where})")
    mirror = all(abs(P[a][1] + P[b][3] - P[a][5]) < 1.5 and abs(P[a][2] - P[b][2]) < 1.5
                 for a, b in (("lefthand", "righthand"), ("sideleft", "sideright")))
    check(mirror, "rest: left and right mirrored (the hands' panels, the side mounts)")
for ph in ("rest", "turn", "walk", "pincushion", "barrel"):  # (the lens at -0.15 zoomed 0.92, and at 0.15 zoomed 1.05)
    pan = phases.get(ph, {"panels": {}})["panels"]
    check(len(pan) == 7 and all(v[7] == 1 for v in pan.values()),
          f"{ph}: every mount meets the screen's edges with no gap ({ {k: v[7] for k, v in pan.items()} })")
check(sway("rest") == 0, f"rest: standing still, no sway ({get('rest', 'sway')})")
check(0.05 < sway("turn") <= 0.6001, f"turn: the panels lag the turn a fraction of a degree ({get('turn', 'sway')})")
check(sway("settle") == 0, f"settle: let go, back to rest within half a second ({get('settle', 'sway')})")
walk = max(sway("walk"), sway("walk") if "walk" in phases else 0)
check(0.005 < walk <= 0.35, f"walk: the walk's bob moves them a little ({get('walk', 'sway')})")
check(sway("still") == 0, f"still: standing again, still ({get('still', 'sway')})")
check(get("hit", "glitch", 0) > 0.3 and get("hit later", "glitch", 1) == 0,
      f"hit: the glitch at once ({get('hit', 'glitch')}), gone half a second later ({get('hit later', 'glitch')})")
check(get("hit", "shake", 0) > 0 and max(abs(x) for x in get("shaking", "shakeoff", (0, 0))) > 0.01
      and get("hit later", "shake", 1) == 0 and get("hit later", "shakeoff", (1, 1)) == (0, 0),
      f"hit: the visor shakes ({get('hit', 'shake')}, offset {get('shaking', 'shakeoff')}), still again half a second later")
h0, h1, h2, h3 = (get(p, "hits", -1) for p in ("hit later", "overheal", "health down", "real hit"))
check(get("overheal", "health") == 150 and get("health down", "health") == 120 and h0 >= 1 and h0 == h1 == h2
      and get("health down", "shake", 1) == 0 and get("health down", "glitch", 1) == 0,
      f"no hit: health going down by itself (150 to {get('health down', 'health')}, as a megahealth's overheal ticks away) "
      f"neither shakes nor glitches it (hits {h0} {h1} {h2}, shake {get('health down', 'shake')})")
check(h3 == h2 + 1 and get("real hit", "shake", 0) > 0 and get("real hit", "health") == 100,
      f"real hit: the server's damage message shakes it, no health taken (hits {h3}, shake {get('real hit', 'shake')})")
check(get("rest", "glass") == 1, "rest: the frosted glass on by default")
check(get("water", "underwater") == 1 and get("water", "fog", 0) > 0.9,
      f"water: under water the visor fogs up ({get('water', 'fog')})")
check(0 < get("surfaced", "fog", 0) < get("water", "fog", 0) and get("dry", "fog", 1) == 0,
      f"water: surfaced it clears ({get('surfaced', 'fog')}, then {get('dry', 'fog')})")
b0, b1 = get("boot", "boot", (1, 1, 1)), get("booted", "boot", (0, 0, 0))
check(min(b0) < 1 and min(b1) == 1, f"boot: the power-on sweep under way {b0}, then done {b1}")
check(get("off", "active") == 0, "status: vr_flat_visor 0 turns it off (the status bar back)")

def pan(p, name):
    return phases.get(p, {"panels": {}})["panels"].get(name)


def contact_all(p):
    pn = phases.get(p, {"panels": {}})["panels"]
    return len(pn) == 7 and all(v[7] == 1 for k, v in pn.items() if v[0] == 1)


R0, Ri, Ro = pan("side mid", "sideleft"), pan("side in", "sideleft"), pan("side out", "sideleft")
Rr0, Rri, Rro = pan("side mid", "sideright"), pan("side in", "sideright"), pan("side out", "sideright")
ok = all((R0, Ri, Ro, Rr0, Rri, Rro)) and Ri[1] < R0[1] < Ro[1] and Rri[3] > Rr0[3] > Rro[3]
check(ok and contact_all("side in") and contact_all("side out"),
      f"side: vr_flat_visor_side_out 0.3 / 1 / 3 the side readings' inner edge at {Ri and Ri[1]} / {R0 and R0[1]} / {Ro and Ro[1]}, "
      "mirrored on the right, the edges met")
Rb, Rrb = pan("side back", "sideleft"), pan("side back", "sideright")
check(all((Rb, Rrb, Ri, Rri)) and Rb[3] < Ri[3] - 1 and Rrb[1] > Rri[1] + 1,
      f"side: -2 pulls the side readings back past the edges (their inner edge at {Rb and Rb[3]}, {Ri and Ri[3]} at 0.3)")
L0, Ls, Lb = pan("log mid", "log"), pan("log small", "log"), pan("log big", "log")
V, S = pan("rest", "vitals"), pan("rest", "status")
clear = all((L0, V, S)) and L0[1] > V[3] and L0[3] < S[1]
check(clear, f"log: at the top's middle, clear of the corners' readings ({L0}, vitals {V}, status {S})")
wid = lambda q: q[3] - q[1]  # noqa: E731
okl = all((L0, Ls, Lb)) and wid(Ls) < wid(L0) * 0.6 and Ls[2] > L0[2] and Lb[2] < L0[2] and wid(Lb) >= wid(L0)
okl = okl and Lb[1] > V[3] and Lb[3] < S[1]
check(okl and contact_all("log small") and contact_all("log big"),
      f"log: _log_width 0.5 with _log_out 3 smaller and lower ({Ls}), 2 with 0.2 larger but fitted, nearer the edge ({Lb}), the edge met")
Lu = pan("log up", "log")
check(Lu is not None and Lb is not None and Lu[0] == 1 and Lu[4] < Lb[4] - 1,
      f"log: _log_out -2 pulls it up past the edge (its bottom at {Lu and Lu[4]}, {Lb and Lb[4]} at 0.2)")
H, B = pan("log hidden", "log"), pan("log back", "log")
check(H is not None and H[0] == 0 and get("log hidden", "logfade", 1) == 0 and B is not None and B[0] == 1
      and get("log back", "logfade", 0) > 0.5,
      f"autohide: no lines, the log's mount gone ({get('log hidden', 'logfade')}); a message, back ({get('log back', 'logfade')})")

print(f"visor_test: {passed} passed, {failed} failed")
sys.exit(1 if failed else 0)
