"""Writes quakevr/motions/heavy_wiggle.mock: the main wrist (one-handed, aimed level) wiggled in yaw and roll at a few
rates, small flicks, each followed by a still second. Usage: python gen_wiggle.py <out.mock>"""
import math, sys

P, X, Y, Z = 70.0, 0.20, 1.20, -0.40
lines = []
def key(t, p=P, yw=0.0, r=0.0):
    lines.append("%.3f main %.3f %.3f %.3f %.1f %.1f %.1f" % (t, X, Y, Z, p, yw, r))

t = 0.0
key(0.0)
lines.append("0.000 off -0.350 1.100 -0.200 0 0 0")
t = 1.0
def step(name):
    lines.append("%.3f cmd echo STEP %s" % (t, name))

def wiggle(name, amp, hz, dur, axis):
    global t
    step(name)
    n = int(dur / 0.011)
    for i in range(n + 1):
        u = i * 0.011
        a = amp * math.sin(2 * math.pi * hz * u)
        key(t + u, yw=a if axis == 'yaw' else 0.0, r=a if axis == 'roll' else 0.0)
    t += dur
    key(t)
    t += 1.2

def flick(name, amp, dur, axis):
    global t
    step(name)
    n = int(2 * dur / 0.011)
    for i in range(n + 1):
        u = min(i * 0.011 / dur, 2.0)
        s = u if u <= 1 else 2 - u
        a = amp * s * s * (3 - 2 * s)
        key(t + i * 0.011, yw=a if axis == 'yaw' else 0.0, r=a if axis == 'roll' else 0.0)
    t += 2 * dur
    key(t)
    t += 1.2

wiggle("yaw30at3", 30, 3, 1.5, 'yaw')
wiggle("yaw45at4", 45, 4, 1.5, 'yaw')
wiggle("yaw60at2", 60, 2, 1.5, 'yaw')
flick("flick20", 20, 0.06, 'yaw')
flick("flick45", 45, 0.06, 'yaw')
wiggle("roll45at3", 45, 3, 1.5, 'roll')
lines.append("%.3f cmd echo STEP end" % t)
open(sys.argv[1], "w").write("\n".join(lines) + "\n")
print("%.1f s" % t)
