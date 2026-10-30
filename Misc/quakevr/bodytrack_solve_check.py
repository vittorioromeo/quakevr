#!/usr/bin/env python3
# bodytrack_solve_check.py -- body tracking's solve over a recorded take against stored reference positions
# (bodytrack_solve_test.sh; ROUND23.md, "FBT regression check").
#   trim <dump.csv> <take.csv> <rows>      a vr_bodytrack_dump capture cut to what vr_bodytrack_replay reads (no solve
#                                          columns), its first <rows> frames
#   ref <dump.csv> <ref.csv> <rows>        a replay's dump cut to the solve's columns (the reference)
#   compare <dump.csv> <ref.csv> <tol>     each frame's solved joints against the reference (Quake units); prints
#                                          "bodytrack_solve: N passed, M failed" and exits 1 on any failure
import csv
import math
import sys

SOLVE_PREFIXES = ('av_', 'tw_', 'fit_', 'w_')
JOINTS = ('pelvis', 'top', 'shoulder_l', 'elbow_l', 'wrist_l', 'shoulder_r', 'elbow_r', 'wrist_r')
WEIGHTS = ('w_torso', 'w_arm_l', 'w_arm_r', 'fit_l_w', 'fit_r_w')


def read(path):
    with open(path, newline='') as f:
        return list(csv.DictReader(f))


def trim(src, dst, rows):
    with open(src, newline='') as f:
        r = csv.reader(f)
        header = next(r)
        keep = [i for i, h in enumerate(header) if not h.startswith(SOLVE_PREFIXES)]
        with open(dst, 'w', newline='') as o:
            w = csv.writer(o, lineterminator='\n')
            w.writerow([header[i] for i in keep])
            for n, row in enumerate(r):
                if n >= rows:
                    break
                w.writerow([row[i] for i in keep])


def ref(src, dst, rows):
    data = read(src)[:rows]
    cols = ['frame'] + list(WEIGHTS) + ['av_%s_%s' % (j, k) for j in JOINTS for k in 'xyz']
    with open(dst, 'w', newline='') as o:
        w = csv.writer(o, lineterminator='\n')
        w.writerow(cols)
        for i, row in enumerate(data):
            w.writerow([i] + [row[c] for c in cols[1:]])


def compare(dump, refPath, tol):
    got = read(dump)
    want = read(refPath)
    passed = failed = 0
    worst = (0.0, '', -1)
    if len(got) < len(want):
        print('bodytrack_solve: the dump has %d frames, the reference %d' % (len(got), len(want)))
        failed += 1
    for i in range(min(len(got), len(want))):
        g, w = got[i], want[i]
        ok = True
        for j in JOINTS:
            a = [float(g['av_%s_%s' % (j, k)]) for k in 'xyz']
            b = [float(w['av_%s_%s' % (j, k)]) for k in 'xyz']
            d = math.dist(a, b)
            if d > worst[0]:
                worst = (d, j, i)
            ok = ok and d <= tol
        for c in WEIGHTS:
            ok = ok and abs(float(g[c]) - float(w[c])) <= 0.02
        passed += ok
        failed += not ok
    print('bodytrack_solve: worst %.2f units (%s, frame %d), tolerance %.2f' % (worst[0], worst[1], worst[2], tol))
    print('bodytrack_solve: %d passed, %d failed' % (passed, failed))
    return failed == 0


if __name__ == '__main__':
    mode = sys.argv[1]
    if mode == 'trim':
        trim(sys.argv[2], sys.argv[3], int(sys.argv[4]))
    elif mode == 'ref':
        ref(sys.argv[2], sys.argv[3], int(sys.argv[4]))
    elif mode == 'compare':
        sys.exit(0 if compare(sys.argv[2], sys.argv[3], float(sys.argv[4])) else 1)
    else:
        sys.exit('usage: bodytrack_solve_check.py trim|ref|compare ...')
