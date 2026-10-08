# shadow_pop_test.py -- shadows must fade, never pop: walks the player back and forth by vrstart's campaign terrace
# brazier (vrstart_gen.py: the west one at (-1340, -81), beside the Quake lectern), where the flames' shadowed lights
# (vr_torch_light_shadows) and the map lights (vr_shadow_maplights) change as you step, logging every shadowed light's
# strength each frame (vr_shadow_stats 2), and checks that none changes faster than a 0.4 s fade.
#
#   python Misc/quakevr/shadow_pop_test.py script [--long]   # writes quakevr/shadowwalk.cfg (git-ignored)
#   bash <kit>/run.sh <agent> -Script "exec shadowwalk.cfg" -Filter "shadowsel|^-?[0-9]+[-+]" | python Misc/quakevr/shadow_pop_test.py check
#
# --long walks ~600 units each way instead of ~150 (the brazier's shadow fades out and others in).
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
FADE_RATE = 2.6  # strength per second at most (a 0.4 s fade, and a little)


def wait(n):
    return ["w10"] * (n // 10) + ["wait"] * (n % 10)


def script(long_walk):
    leg = 110 if long_walk else 30
    out = ['alias w10 "wait;wait;wait;wait;wait;wait;wait;wait;wait;wait"', "map vrstart"] + wait(60)
    # (setpos turns noclip on; noclip turns it off again: walking, not flying)
    out += ["god", "notarget", "setpos -1360 -280 136 0 78 0", "noclip"] + wait(90) + ["vr_mock_look 0 0", "vr_shadow_stats 2"]
    for _ in range(3):
        for d in (-1, 1):
            out += ["vr_mock_stick off 0 %d" % d] + wait(leg) + ["vr_mock_stick off 0 0"] + wait(50)
    out += ["vr_shadow_stats 0", "toggleconsole", "quit"]
    path = os.path.join(ROOT, "quakevr", "shadowwalk.cfg")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    print("wrote %s" % path)


def check(lines):
    recs = []
    for line in lines:
        line = line.rstrip("\n")
        if line.startswith("shadowsel"):
            recs.append(line)
        elif recs and line and not line.startswith("exit="):
            recs[-1] += line  # (the console wraps long lines)
    rows = []
    for r in recs:
        m = re.match(r"shadowsel ([\d.]+) at (\S+) (\S+) (\S+): dlights(.*); map lights(.*)", r)
        if m:
            d = {("dlight", k): float(v) for k, _, v in re.findall(r"(-?\d+)([+-])([\d.]+)", m.group(5))}
            d.update({("map light", k): float(v) for k, _, v in re.findall(r"(-?\d+)([+-])([\d.]+)", m.group(6))})
            rows.append((float(m.group(1)), float(m.group(3)), d))
    if len(rows) < 2:
        print("FAIL: no vr_shadow_stats 2 lines")
        return 1
    flips = fades = 0
    for (t0, _, a), (t1, y1, b) in zip(rows, rows[1:]):
        for k in set(a) | set(b):
            step = abs(b.get(k, 0.0) - a.get(k, 0.0))
            fades += step > 1e-6
            if step > max(0.1, (t1 - t0) * FADE_RATE + 0.02):
                flips += 1
                if flips <= 10:
                    print("POP t=%.3f y=%.0f %s %s: %.2f -> %.2f" % (t1, y1, k[0], k[1], a.get(k, 0.0), b.get(k, 0.0)))
    ys = [r[1] for r in rows]
    print("%s: %d frames, y %.0f..%.0f, %d strength changes, %d pops" % ("PASS" if flips == 0 else "FAIL", len(rows), min(ys),
                                                                        max(ys), fades, flips))
    return 1 if flips else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "script":
        script("--long" in sys.argv)
    elif len(sys.argv) > 1 and sys.argv[1] == "check":
        sys.exit(check(open(sys.argv[2], encoding="utf-8", errors="replace") if len(sys.argv) > 2 else sys.stdin))
    else:
        print(__doc__ if __doc__ else "usage: shadow_pop_test.py script [--long] | check [log]")
