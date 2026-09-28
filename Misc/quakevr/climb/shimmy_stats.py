# shimmy_stats.py <qconsole.log> [...]: the shimmy plays (climb_plays.py shimmy...; vr_climb_debug 2): per stroke (the
# lines between "STROKE" and "STROKEEND"), how far the body moved along the ledge (the axis the hands pushed along)
# against the hands' pull along it, how far it moved into or out of the face, the frames where it did less than half the
# pull's sideways part, and the body's distance from the face (x, the ledge at vrclimb: the face at x 96).
import sys, math
UPM = 26.25
for path in sys.argv[1:]:
    strokes = []; cur = None
    for line in open(path, encoding="utf-8", errors="replace"):
        if line.startswith("STROKEEND"):
            if cur: strokes.append(cur)
            cur = None
        elif line.startswith("STROKE"):
            cur = []
        elif line.startswith("climbtrace") and cur is not None:
            p = line.split()
            cur.append((p[2], tuple(map(float, p[4:7])), tuple(map(float, p[12:15])), tuple(map(float, p[16:19]))))
    print(path)
    tot_side = tot_pull = 0.0
    for i, s in enumerate(strokes):
        hang = [r for r in s if r[0].startswith("hang")]
        if not hang:
            print(f"  stroke {i + 1}: not hanging"); continue
        want = [sum(r[2][k] for r in hang) for k in range(3)]
        done = [sum(r[3][k] for r in hang) for k in range(3)]
        side = math.hypot(want[0], want[1]); ax = (want[0] / side, want[1] / side) if side > 1e-6 else (0, 1)
        # the sideways axis: the pull's horizontal part (the push is along the ledge); in/out is x at vrclimb and e1m1
        # (their ledges' faces are square to x)
        along = done[1] * (1 if want[1] >= 0 else -1); wanted = abs(want[1])
        stuck = sum(1 for r in hang if abs(r[2][1]) > 1e-3 and abs(r[3][1]) < 0.5 * abs(r[2][1]))
        xs = [r[1][0] for r in hang]
        print(f"  stroke {i + 1:2d}: along {along:6.2f} u of {wanted:5.2f} pulled ({along / UPM * 100:5.1f} cm, {100 * along / wanted if wanted else 0:5.1f}%),"
              f" in/out {done[0]:+6.2f} u (pulled {want[0]:+6.2f}), {stuck:2d}/{len(hang)} frames stuck, body x {min(xs):.2f}..{max(xs):.2f}")
        tot_side += along; tot_pull += wanted
    if tot_pull:
        print(f"  total along {tot_side:.2f} u of {tot_pull:.2f} ({100 * tot_side / tot_pull:.1f}%), {len(strokes)} strokes")
