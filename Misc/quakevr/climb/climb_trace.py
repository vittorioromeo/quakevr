# climb_trace.py <qconsole.log>: reads the "climbtrace" lines (vr_climb_debug 2): per frame, the body's motion against
# the hands' pull; the frames where a hand takes hold or lets go;
# the largest difference (units and mm at 26.25 units/m).
import re, sys, math
UPM = 26.25
rows = []
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    if not line.startswith("climbtrace"):
        continue
    p = line.split()
    t = float(p[1]); what = p[2]
    org = tuple(map(float, p[4:7])); want = tuple(map(float, p[12:15])); moved = tuple(map(float, p[16:19]))
    off = line.split("| off ")[1].split(" | main ")[0].strip(); main = line.split("| main ")[1].strip()
    rows.append((t, what, org, want, moved, off[0], main[0], off, main))
def d(a, b): return math.dist(a, b)
def hand(txt):
    p = txt.split()
    return tuple(map(float, p[1:4]))
worst = (0, None); events = []; indep = (0, None)
for i in range(1, len(rows)):
    t, what, org, want, moved, oh, mh, off, main = rows[i]
    po = rows[i - 1]
    delta = tuple(org[k] - po[2][k] for k in range(3))
    change = (po[5], po[6]) != (oh, mh)
    if what.startswith("hang"):
        err = d(moved, want)
        if err > worst[0]: worst = (err, rows[i])
    if what.startswith("hang") and i >= 2:
        # independent of the engine's own figure: the pull from the logged tracked hands of the hands holding in
        # both frames (relative to the body), weighted by how far each moved
        pull = [0.0, 0.0, 0.0]; w_sum = 0.0
        for k, (a, b) in enumerate(((po[7], off), (po[8], main))):
            if a[0] == "H" and b[0] == "H":
                ra = [hand(a)[j] - rows[i - 2][2][j] for j in range(3)]; rb = [hand(b)[j] - po[2][j] for j in range(3)]
                mv = [rb[j] - ra[j] for j in range(3)]; w = math.hypot(*mv)
                pull = [pull[j] + mv[j] * w for j in range(3)]; w_sum += w
        # a frame's logged hands are placed from the body where the frame began (the last frame's end)
        ind = tuple(-pull[j] / w_sum if w_sum > 1e-6 else 0.0 for j in range(3))
        e = d(delta, ind)
        if e > indep[0]: indep = (e, rows[i], ind, delta)
    if change:
        # the frame the set of holding hands changed: the body's move that frame against what the hands wanted
        events.append((t, po[5] + po[6], oh + mh, delta, want if what.startswith("hang") else (0, 0, 0)))
print(f"{len(rows)} frames, hanging {sum(r[1].startswith('hang') for r in rows)}; worst |moved - wanted| {worst[0]:.4f} u ({worst[0] / UPM * 1000:.2f} mm)")
if worst[1]: print("  at", worst[1][0], worst[1][1], "want", worst[1][3], "moved", worst[1][4])
print(f"worst |body's move - hands' pull (from the logged hands)| {indep[0]:.4f} u ({indep[0] / UPM * 1000:.2f} mm)")
if indep[1]: print("  at", indep[1][0], indep[1][1], "pull", tuple(round(x, 3) for x in indep[2]), "moved", tuple(round(x, 3) for x in indep[3]))
for t, a, b, delta, want in events:
    pop = d(delta, want)
    print(f"t {t:8.3f} hands {a}->{b}: body moved {math.hypot(*delta):6.3f} u, wanted {math.hypot(*want):6.3f} u, difference {pop:.4f} u = {pop / UPM * 1000:.2f} mm")
