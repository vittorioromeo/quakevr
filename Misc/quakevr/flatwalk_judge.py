"""flatwalk_judge.py <gap most> <jump most> < a `flatwalk: <label> done ...` line: pass or fail, and why."""
import re
import sys

gap_most, jump_most = float(sys.argv[1]), float(sys.argv[2])
line = sys.stdin.read()
m = re.search(r"speed (\S+) wrist (\S+) (\S+) jump (\S+) (\S+) hand (\S+) (\S+) shoulder (\S+) (\S+)", line)
if not m:
    print("fail: no verdict line")
    sys.exit(0)
speed, g0, g1, j0, j1, h0, h1, s0, s1 = (float(x) for x in m.groups())
why = []
if speed < 100:
    why.append(f"speed {speed:.0f}: not walking")
if max(g0, g1) > gap_most:
    why.append(f"wrist off its rest {max(g0, g1):.3f} > {gap_most}")
if max(j0, j1) > jump_most:
    why.append(f"jump {max(j0, j1):.3f} > {jump_most}")
print(("fail: " + "; ".join(why)) if why else f"pass (wrist {max(g0, g1):.3f}, jump {max(j0, j1):.3f}, hand {h1:.2f}, shoulder {s1:.2f})")
