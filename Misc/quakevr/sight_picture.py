# sight_picture.py [progs dir] [name.mdl[#tag]:rx,ry,rz:fx,fy,fz ...]: each sighted gun's sight picture from its mesh
# (frame 0), with the eye on the sight line (as the flatscreen ironsight puts it; ROUND24.md, "Flat pose leftovers").
# ONLY=<text>: the guns whose name has it; YS=1: also the posts', the muzzle's and the whole gun's y. The lines below are
# vr_sightalign_table.inc's and the painted ones (vr_sight_lines). Pure Python. Prints, per gun: "rays: the front sight
# seen down to D under its top" (D: how much of the front sight shows over whatever is nearer; 0: hidden), then:
# the occluder's top in the line's vertical plane (elevation from the eye, as height at the front sight: >0 hides), the occluder's top in the line's vertical plane (elevation from the eye, as height at the front sight: >0 hides),
# the front sight's top and the rear's tops (relative to the line, at their own distances), the bore and the posts' y.
import os, struct, sys, math
PROGS = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'quakevr', 'progs')


def load(path):
    d = open(path, 'rb').read()
    scale = struct.unpack_from('<3f', d, 8); tr = struct.unpack_from('<3f', d, 20)
    nskins, sw, sh, nv, nt, nf, st, flags, size = struct.unpack_from('<iiiiiiiif', d, 48)
    o = 84
    for i in range(nskins):
        g = struct.unpack_from('<i', d, o)[0]; o += 4
        if g == 0: o += sw * sh
        else:
            n = struct.unpack_from('<i', d, o)[0]; o += 4 + 4 * n + n * sw * sh
    o += 12 * nv
    tris = [struct.unpack_from('<iiii', d, o + 16 * i)[1:] for i in range(nt)]; o += 16 * nt
    t = struct.unpack_from('<i', d, o)[0]; o += 4
    if t != 0:
        n = struct.unpack_from('<i', d, o)[0]; o += 4 + 8 + 4 * n
    o += 8 + 16
    V = [struct.unpack_from('<BBBB', d, o + 4 * i) for i in range(nv)]
    V = [(v[0] * scale[0] + tr[0], v[1] * scale[1] + tr[1], v[2] * scale[2] + tr[2]) for v in V]
    return V, tris


def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def norm(a):
    l = math.sqrt(dot(a, a)); return (a[0] / l, a[1] / l, a[2] / l)


def picture(name, rear, front, eyeback=30.0, band=0.04):
    V, T = load(os.path.join(PROGS, name.split('#')[0]))
    u = norm(sub(front, rear)); L = dot(sub(front, rear), u)
    eye = (rear[0] - u[0] * eyeback, rear[1] - u[1] * eyeback, rear[2] - u[2] * eyeback)
    up = norm((-u[0] * u[2], -u[1] * u[2], 1 - u[2] * u[2]))
    side = (u[1] * up[2] - u[2] * up[1], u[2] * up[0] - u[0] * up[2], u[0] * up[1] - u[1] * up[0])
    # Sample every triangle's points (edges subdivided) near the line's vertical plane (|lateral angle| small), between
    # the eye and the front sight; their elevation angle from the eye, as height at the front's distance.
    occl = -9.0; occl_at = None; front_top = -9.0; rear_top = -9.0
    D = eyeback + L
    for (a, b, c) in T:
        P = (V[a], V[b], V[c])
        for i in range(9):
            for j in range(9 - i):
                k = 8 - i - j
                q = tuple((P[0][m] * i + P[1][m] * j + P[2][m] * k) / 8.0 for m in range(3))
                d = sub(q, eye); f = dot(d, u)
                if f < 1.0: continue
                lat = dot(d, side) / f; el = dot(d, up) / f
                if abs(lat) * D > band: continue
                hD = el * D  # as height at the front sight's distance
                if f < eyeback + L - 0.6:
                    if hD > occl: occl, occl_at = hD, f - eyeback
                elif f <= eyeback + L + 0.6:
                    front_top = max(front_top, hD)
    # Rays from the eye to points under the front sight's top (delta below it, at the front's x): the first one that
    # something nearer than the front sight (0.6 before it) stops: the front sight is seen down to there.
    def hit(o, dvec, tmax):
        best = None
        for (a, b, c) in T:
            A, B, C = V[a], V[b], V[c]
            e1, e2 = sub(B, A), sub(C, A)
            pv = (dvec[1] * e2[2] - dvec[2] * e2[1], dvec[2] * e2[0] - dvec[0] * e2[2], dvec[0] * e2[1] - dvec[1] * e2[0])
            det = dot(e1, pv)
            if abs(det) < 1e-9: continue
            tv = sub(o, A); uu = dot(tv, pv) / det
            if uu < 0 or uu > 1: continue
            qv = (tv[1] * e1[2] - tv[2] * e1[1], tv[2] * e1[0] - tv[0] * e1[2], tv[0] * e1[1] - tv[1] * e1[0])
            vv = dot(dvec, qv) / det
            if vv < 0 or uu + vv > 1: continue
            t = dot(e2, qv) / det
            if 1e-4 < t < tmax and (best is None or t < best[0]): best = (t, A[0])
        return best
    seen = None
    for i in range(1, 61):
        dl = i * 0.01
        tgt = (front[0] - up[0] * dl, front[1] - up[1] * dl, front[2] - up[2] * dl)
        dv = sub(tgt, eye); dist = math.sqrt(dot(dv, dv)); dv = (dv[0] / dist, dv[1] / dist, dv[2] / dist)
        h = hit(eye, dv, dist - 0.6)
        if h:
            seen = (dl - 0.01, h[1]); break
    print('   rays: the front sight seen down to %.2f under its top (stopped at x %.2f)' % seen if seen else
          '   rays: the front sight seen down to 0.60+ under its top')
    # The rear sight's tops either side of the notch (within 1.2 of the middle), relative to the line at the rear.
    ears = [v for v in V if abs(dot(sub(v, rear), u)) < 0.8 and 0.1 < abs(dot(sub(v, rear), side)) < 1.3]
    ear_top = max((dot(sub(v, rear), up) for v in ears), default=float('nan'))
    print('%-16s line rear %s front %s (%.2f deg up): occluder top %+.3f at %.2f along; front top %+.3f; visible front %.3f; '
          'rear ears top %+.3f' % (name, tuple(round(x, 3) for x in rear), tuple(round(x, 3) for x in front),
                                   math.degrees(math.asin(u[2])), occl, occl_at or 0, front_top, front_top - occl, ear_top))
    return V


def ys(V, x0, x1, z0, z1, what):
    S = [v for v in V if x0 <= v[0] <= x1 and z0 <= v[2] <= z1]
    if not S:
        print('   %s: none' % what); return
    lo, hi = min(v[1] for v in S), max(v[1] for v in S)
    print('   %-34s y %.3f .. %.3f (middle %+.3f), z top %.3f, %d verts' % (what, lo, hi, (lo + hi) / 2, max(v[2] for v in S), len(S)))


LINES = {
    'v_shot.mdl': ((9.754, -0.05, 6.6116), (32.406, -0.0044, 6.2784)),  # (2026-11: the housings; the irons' tops)
    'v_shot2.mdl': ((12.823, -0.03, 7.7781), (27.818, 0.0066, 8.1676)),  # (2026-11: the notch sight; the irons' tops)
    'v_light.mdl': ((-2.7194, -0.0982, 11.4806), (31.0765, -0.0982, 11.3435)),  # (2026-11: the housing, the post)
    'v_plasma.mdl': ((-2.7194, -0.0982, 11.4806), (31.0765, -0.0982, 11.3435)),
    'v_gruntgun.mdl': ((-2.15, 0, 11.6), (28.15, 0, 11.6)),
    'v_enfrifle.mdl': ((6.30, 0, 10.799), (23.00, 0, 10.659)),
    'v_nailpistol.mdl': ((-1.6306, 0, 6.3763), (8.9316, 0, 6.3763)),  # (2026-11: painted, the ears' and the post's inserts)
    'v_flaregun.mdl': ((1.15, 0, 6.412), (11.75, 0, 6.412)),
    'v_revolver.mdl': ((3.764, 0, 9.178), (20.314, 0, 9.178)),
}
if len(sys.argv) > 2:
    for a in sys.argv[2:]:
        n, r, f = a.split(':'); LINES[n] = (tuple(map(float, r.split(','))), tuple(map(float, f.split(','))))
only = os.environ.get('ONLY')
for n, (r, f) in LINES.items():
    if only and only not in n: continue
    V = picture(n, r, f)
    if os.environ.get('YS'):
        ys(V, f[0] - 0.4, f[0] + 0.4, f[2] - 0.6, f[2] + 0.3, 'front post (top 0.6)')
        ys(V, r[0] - 0.6, r[0] + 0.6, r[2] + 0.02, r[2] + 1.0, 'rear ears (above the notch bottom)')
        mx = max(v[0] for v in V)
        ys(V, mx - 0.5, mx + 0.01, -99, 99, 'muzzle end (last 0.5)')
        ys(V, -99, 99, -99, 99, 'whole gun')
