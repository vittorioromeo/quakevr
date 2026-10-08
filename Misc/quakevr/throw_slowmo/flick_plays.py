# flick_plays.py <out> <Misc/quakevr folder> [stretch] -- wrist flicks for the mock (flick_slowmo_test.sh; ROUND21.md,
# "Wrist flicks in bullet time"): the forearm still, the hand turning about the wrist (the controller's point 7 cm ahead
# of it), the hand's pitch from a0 to a1 over `dur` s (min-jerk), let go at the middle: three forward and down, three
# upward (gentle, medium, fast), two upward with the hand drifting up, nudges and lobs (stroke); then throw_plays.py's overhand throw without its
# wrist (overhand0, the arm alone) and with it. `stretch`: every motion that many times slower (3.333: made slowly with
# the world in bullet time at 0.3x). Prints the play's length (s).
import sys, math
sys.path.insert(0, sys.argv[2])
import throw_plays as tp
RATE = 1000.0; GUN = 70.0; WD = 0.07
# The flicks' controller pitched as the throws' frame takes it (vr_hands.cpp throwFrame: 39.5 degrees, whatever
# vr_gunangle): its forward then points from the wrist to the controller's point, as on a real hand.
FLICK_GUN = 39.5
WRIST = (0.20, 1.30, -0.30)
def flick(a0, a1, dur, drift=0.0):
    # `drift`: the wrist rising meanwhile, that fastest (m/s; min-jerk), a hand not quite still.
    n = int(dur * RATE); keys = []
    for i in range(n + 1):
        s = i / n; a = a0 + (a1 - a0) * tp.minjerk(s); r = math.radians(a); up = drift * dur / 1.875 * tp.minjerk(s)
        keys.append((s * dur, WRIST[0], WRIST[1] + up + WD * math.sin(r), WRIST[2] - WD * math.cos(r), a + FLICK_GUN - GUN))
    return keys, 0.5 * dur
throws = [("flick_fast", flick(100, -10, 0.10)), ("flick", flick(100, 0, 0.14)), ("flick_soft", flick(90, 10, 0.2))]
# Upward flicks (vrfiringrange_2026-10-08_10-30-08: "flicking my wrist upwards without moving my hand much, to throw
# something towards the sky"): the hand from 45 degrees below level to 55 above, let go just past level (the point then
# going up); peaks of about 12 (gentle), 24 and 40 rad/s.
throws += [("up_gentle", flick(-45, 55, 0.27)), ("up_medium", flick(-45, 55, 0.136)), ("up_fast", flick(-45, 55, 0.082))]
# The gentle and medium ones with the hand rising 0.5 m/s at most meanwhile ("without moving my hand much").
throws += [("up_gentle_d", flick(-45, 55, 0.27, 0.5)), ("up_medium_d", flick(-45, 55, 0.136, 0.5))]
# Nudges (vrfiringrange_2026-10-08_14-20-31: "a tiny upward hand movement throws massively high in bullet time"): the
# wrist straight, the hand rising 5, 10 and 15 cm over 0.15 s (min-jerk), let go at 60% of it; one of 10 cm with a
# small upward flick. And lobs: the arm, wrist straight, forward and up at 45 degrees 40, 50 and 60 cm over 0.25 s (the
# deliberate throw that, made slowly with the world, must still go as at full speed).
def stroke(dist, dur, up=1.0, fwd=0.0, a0=0.0, a1=0.0, rel=0.6):
    n = int(dur * RATE); keys = []
    for i in range(n + 1):
        s = i / n; m = tp.minjerk(s); a = a0 + (a1 - a0) * m; r = math.radians(a)
        keys.append((s * dur, WRIST[0], WRIST[1] + dist * up * m + WD * math.sin(r), WRIST[2] - dist * fwd * m - WD * math.cos(r),
                     a + FLICK_GUN - GUN))
    return keys, rel * dur
throws += [("nudge5", stroke(0.05, 0.15)), ("nudge10", stroke(0.10, 0.15)), ("nudge15", stroke(0.15, 0.15))]
throws += [("nudge10_f", stroke(0.10, 0.15, a0=-20, a1=40))]
throws += [(f"lob{d}", stroke(d / 100, 0.25, 0.7071, 0.7071)) for d in (40, 50, 60)]
tp.RATE = 1000.0 # smooth between the frames: at 90 keys a second the overhand moved by 20% with where the frames fell
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
    # The flicks' keyed point is the one whose velocity the throw takes (not the legacy pose's raw point, the grip
    # some centimetres off it): 7 cm ahead of the wrist, as vr_throw_wrist_dist says of a real hand. The overhand
    # throws as throw_plays.py makes them (the legacy pose).
    L.append(f"{t0 - 0.45:.3f} cmd vr_controller_legacy_pose {0 if name.startswith(('flick', 'up_', 'nudge', 'lob')) else 1}")
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
