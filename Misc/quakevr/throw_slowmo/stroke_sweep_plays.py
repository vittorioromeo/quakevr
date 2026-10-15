# stroke_sweep_plays.py <out> <Misc/quakevr folder> [stretch] [kind] -- a sweep of throw strokes for the mock
# (stroke_sweep_test.sh; ROUND22.md, "Bullet-time throws: the stroke's tempo"): the wrist straight, the hand forward
# and up at 45 degrees (kind "lob") or straight up (kind "toss") over d = 3 .. 70 cm (min-jerk), in the time such a
# stroke takes at real speed (0.12 s + 0.25 s a metre), let go at 60% of it. `stretch`: every motion that many times
# slower (3.333: made slowly with the world at 0.3x). Prints the play's length (s).
import sys, math
sys.path.insert(0, sys.argv[2])
import throw_plays as tp
RATE = 1000.0; GUN = 70.0; WD = 0.07; FLICK_GUN = 39.5
WRIST = (0.20, 1.30, -0.30)
DISTS = (3, 5, 8, 12, 18, 25, 35, 50, 70)
def stroke(dist, dur, up, fwd, rel=0.6):
    n = int(dur * RATE); keys = []
    for i in range(n + 1):
        s = i / n; m = tp.minjerk(s)
        keys.append((s * dur, WRIST[0], WRIST[1] + dist * up * m, WRIST[2] - dist * fwd * m - WD, FLICK_GUN - GUN))
    return keys, rel * dur
stretch = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
kind = sys.argv[4] if len(sys.argv) > 4 else "lob"
up, fwd = (0.7071, 0.7071) if kind == "lob" else (1.0, 0.0)
throws = [(f"{kind}{d}", stroke(d / 100, 0.12 + 0.25 * d / 100, up, fwd)) for d in DISTS]
L = ["0.000 main 0.25 1.1 -0.2 70 0 0", "0.000 off -0.25 1.1 -0.2 70 0 0"]
t0 = 1.0
for name, (keys, rel) in throws:
    L.append(f"{t0 - 0.5:.3f} main {keys[0][1]:.6f} {keys[0][2]:.6f} {keys[0][3]:.6f} {keys[0][4] + GUN:.4f} 0 0")
    L.append(f"{t0 - 0.45:.3f} cmd echo THROW {name}")
    L.append(f"{t0 - 0.45:.3f} cmd vr_controller_legacy_pose 0")
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
