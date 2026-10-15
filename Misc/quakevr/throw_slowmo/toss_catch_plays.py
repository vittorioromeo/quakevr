# toss_catch_plays.py <out> <Misc/quakevr folder> <slow> <toss cm> <catch delays...> -- the axe held in the main hand
# tossed straight up `toss` cm (min-jerk over 0.12 s + 0.25 s a metre, let go at 60% of it, the hand then still) and
# the grip closed again `delay` game seconds after the release (times `slow`, the real seconds of bullet time's 0.3x:
# 3.333), one trial a delay (toss_catch_test.sh; ROUND22.md, "Mid-air catches"). Each trial echoes TRIAL <cm> <delay>
# and, 0.6 game seconds after the release, CHECK (impulse 152 again: a new axe if it fell). Prints the play's length.
import sys, math
sys.path.insert(0, sys.argv[2])
import throw_plays as tp
RATE = 1000.0; GUN = 70.0; WD = 0.07; FLICK_GUN = 39.5
WRIST = (0.20, 1.30, -0.30)
slow = float(sys.argv[3]); d = float(sys.argv[4]) / 100; delays = [float(x) for x in sys.argv[5:]]
dur = 0.12 + 0.25 * d; rel = 0.6 * dur
n = int(dur * RATE); keys = []
for i in range(n + 1):
    s = i / n
    keys.append((s * dur, WRIST[0], WRIST[1] + d * tp.minjerk(s), WRIST[2] - WD, FLICK_GUN))
L = [f"0.000 main {keys[0][1]:.6f} {keys[0][2]:.6f} {keys[0][3]:.6f} {keys[0][4]:.4f} 0 0", "0.000 off -0.25 1.1 -0.2 70 0 0"]
t0 = 1.5
for dl in delays:
    L.append(f"{t0 - 1.0:.3f} main {keys[0][1]:.6f} {keys[0][2]:.6f} {keys[0][3]:.6f} {keys[0][4]:.4f} 0 0")
    L.append(f"{t0 - 0.95:.3f} cmd +grabright")
    L.append(f"{t0 - 0.95:.3f} grip main 1")
    L.append(f"{t0 - 0.9:.3f} cmd impulse 152")
    L.append(f"{t0 - 0.2:.3f} cmd echo TRIAL {sys.argv[4]} {dl}")
    for kk in keys:
        L.append(f"{t0 + kk[0]:.6f} main {kk[1]:.6f} {kk[2]:.6f} {kk[3]:.6f} {kk[4]:.4f} 0 0")
    L.append(f"{t0 + rel - 0.015:.6f} grip main 1")
    L.append(f"{t0 + rel:.6f} cmd -grabright")
    L.append(f"{t0 + rel + 0.035:.6f} grip main 0")
    tc = t0 + rel + dl * slow
    L.append(f"{tc:.6f} cmd +grabright")
    L.append(f"{tc:.6f} grip main 1")
    L.append(f"{t0 + rel + 0.6 * slow:.6f} cmd echo CHECK")
    t0 += 1.5 + 0.6 * slow
open(sys.argv[1], "w", newline="\n").write("\n".join(L) + "\n")
print(t0)
