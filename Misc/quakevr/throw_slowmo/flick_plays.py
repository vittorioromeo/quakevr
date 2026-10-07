# flick_plays.py <out> <Misc/quakevr folder> [stretch] -- wrist flicks for the mock (flick_slowmo_test.sh; ROUND21.md,
# "Wrist flicks in bullet time"): the forearm still, the hand turning about the wrist (the controller's point 7 cm ahead
# of it), the hand's pitch from a0 down to a1 over `dur` s (min-jerk), let go at the middle; then throw_plays.py's
# overhand throw without its wrist (overhand0, the arm alone) and with it. `stretch`: every motion that many times slower
# (3.333: made slowly with the world in bullet time at 0.3x). Prints the play's length (s).
import sys, math
sys.path.insert(0, sys.argv[2])
import throw_plays as tp
RATE = 1000.0; GUN = 70.0; WD = 0.07
WRIST = (0.20, 1.30, -0.30)
def flick(a0, a1, dur):
    n = int(dur * RATE); keys = []
    for i in range(n + 1):
        s = i / n; a = a0 + (a1 - a0) * tp.minjerk(s); r = math.radians(a)
        keys.append((s * dur, WRIST[0], WRIST[1] + WD * math.sin(r), WRIST[2] - WD * math.cos(r), a))
    return keys, 0.5 * dur
throws = [("flick_fast", flick(100, -10, 0.10)), ("flick", flick(100, 0, 0.14)), ("flick_soft", flick(90, 10, 0.2))]
tp.RATE = 90.0
k, r = tp.arc_throw("overhand", 150, 40, 95, 35, -45, 0.30)
k0, r0 = tp.arc_throw("overhand0", 150, 40, 95, 0, 0, 0.30)
throws.append(("overhand0", (k0, r0)))
throws.append(("overhand", (k, r)))
stretch = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
L = ["0.000 main 0.25 1.1 -0.2 70 0 0", "0.000 off -0.25 1.1 -0.2 70 0 0"]
t0 = 1.0
for name, (keys, rel) in throws:
    L.append(f"{t0 - 0.5:.3f} main {keys[0][1]:.6f} {keys[0][2]:.6f} {keys[0][3]:.6f} {keys[0][4] + GUN:.4f} 0 0")
    L.append(f"{t0 - 0.45:.3f} cmd echo THROW {name}")
    L.append(f"{t0 - 0.4:.3f} cmd +grabmain")
    L.append(f"{t0 - 0.4:.3f} grip main 1")
    for kk in keys:
        L.append(f"{t0 + kk[0] * stretch:.6f} main {kk[1]:.6f} {kk[2]:.6f} {kk[3]:.6f} {kk[4] + GUN:.4f} 0 0")
    L.append(f"{t0 + (rel - 0.015) * stretch:.6f} grip main 1")
    L.append(f"{t0 + rel * stretch:.6f} cmd -grabmain")
    L.append(f"{t0 + (rel + 0.035) * stretch:.6f} grip main 0")
    t0 += 1.2 + keys[-1][0] * stretch
open(sys.argv[1], "w", newline="\n").write("\n".join(L) + "\n")
print(t0)
