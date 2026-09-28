# normaldetail.py -- what normalbake.py raises on each of Quake VR's own models: the shapes their meshes are too coarse
# to carry, placed from what the generators know of them (bones, parts, materials, the skins' painted lines), in the
# model's own units. See docs/vr-port/ROUND21.md, "Baked normal maps".
#
# Every recipe returns, per raster entry, the surface gradient of the heights it raises (model units per unit): the
# baker bends the engine's normal by it. Heights defined in space (a function of the position) are the same on both
# sides of a UV seam, so the seam doesn't show.

import math

import numpy as np

try:
    from . import normalbake as nb
except ImportError:
    import normalbake as nb


def smooth(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3 - 2 * x)


def gauss(x, s):
    return np.exp(-(x / s) ** 2)


def seg_dist(p, a, b):
    """Distance from points p (n, k) to the segment a-b, and the parameter along it (0 at a, 1 at b)."""
    ab = b - a
    t = np.clip(((p - a) @ ab) / max(ab @ ab, 1e-12), 0.0, 1.0)
    return np.linalg.norm(p - (a + t[:, None] * ab), axis=1), t


# ----------------------------------------------------------------------------
# The jointed hand (hand_rig.md5mesh): rig space, hand units (about 1.2 cm), +x to the fingers, +y the palm's side,
# +z the thumb's side.

FINGERS = ("thumb", "index", "middle", "ring", "pinky")


class HandRig:
    """The hand's anatomy as its mesh has it: each finger's joints (the rings of vertices at them, where the author's
    edit put them), its tip, its dorsal side (where the nail is painted)."""

    def __init__(self, low):
        names = [j[0] for j in low.joints]
        self.index = {n: i for i, n in enumerate(names)}
        W = np.zeros((len(low.P), len(names)))
        for v, ws in enumerate(low.extra["weights"]):
            for j, b in ws:
                W[v, j] += b
        W /= np.maximum(W.sum(1, keepdims=True), 1e-9)
        self.W = W
        dom = W.argmax(1)
        P = low.P
        self.finger_of_joint = np.full(len(names), -1)
        for i, n in enumerate(names):
            for f, fn in enumerate(FINGERS):
                if n.startswith(fn + "_"):
                    self.finger_of_joint[i] = f
        self.rings, self.radius, self.tip, self.dorsal = {}, {}, {}, {}
        for f, fn in enumerate(FINGERS):
            pts = []
            for k in (1, 2, 3):
                ring = dom == self.index.get("%s_%d_half" % (fn, k), -1)
                if fn == "thumb" and k == 1 or ring.sum() < 3:
                    c = np.array(low.joints[self.index["%s_%d" % (fn, k)]][2], float)
                    r = 1.0
                else:
                    c = P[ring].mean(0)
                    r = np.linalg.norm(P[ring] - c, axis=1).mean()
                pts.append(c)
                self.radius[(f, k)] = r
            d = normalize(pts[2] - pts[1])
            seg3 = dom == self.index["%s_3" % fn]
            tipd = ((P[seg3] - pts[2]) @ d).max() if seg3.any() else 2.0
            self.tip[f] = pts[2] + d * tipd
            self.rings[f] = pts
        # the dorsal side: away from the palm (-y) for the fingers; the thumb's, where its nail is (the distal
        # vertices furthest from the palm's side of its axis, found below from the mesh)
        for f in range(5):
            a = normalize(self.tip[f] - self.rings[f][2])
            if f == 0:
                seg3 = dom == self.index["thumb_3"]
                # the thumb's nail faces away from the index finger's side and the palm: the distal vertices'
                # mean offset from the axis, turned towards -y
                off = P[seg3] - self.rings[0][2]
                off -= np.outer(off @ a, a)
                cand = normalize(-np.array([0.0, 1.0, 0.0]) + 0.0 * a)
                # pick the direction of the offset cloud most opposed to +y (the palm)
                d = cand - (cand @ a) * a
                self.dorsal[f] = normalize(d)
            else:
                d = np.array([0.0, -1.0, 0.0])
                self.dorsal[f] = normalize(d - (d @ a) * a)

    def polyline(self, f):
        return self.rings[f] + [self.tip[f]]


def normalize(v):
    return v / max(np.linalg.norm(v), 1e-12)


def finger_coords(p, pts, dorsal0):
    """For points p near a finger (polyline pts: MCP, PIP, DIP, tip): the length along it s, the angle round it from
    the dorsal side phi (-pi..pi), the distance from its axis r, and the joints' places along it."""
    best = np.full(len(p), np.inf)
    s = np.zeros(len(p))
    phi = np.zeros(len(p))
    r = np.zeros(len(p))
    acc = 0.0
    joints = [0.0]
    for i in range(len(pts) - 1):
        a, b = pts[i], pts[i + 1]
        L = np.linalg.norm(b - a)
        ax = (b - a) / L
        t = np.clip((p - a) @ ax, -L * 0.6 if i == 0 else 0.0, L * (1.4 if i == len(pts) - 2 else 1.0))
        foot = a + np.outer(t, ax)
        off = p - foot
        d = np.linalg.norm(off, axis=1)
        take = d < best
        dor = dorsal0 - (dorsal0 @ ax) * ax
        dor /= np.linalg.norm(dor)
        side = np.cross(ax, dor)
        ang = np.arctan2(off @ side, off @ dor)
        best = np.where(take, d, best)
        s = np.where(take, acc + t, s)
        phi = np.where(take, ang, phi)
        r = np.where(take, d, r)
        acc += L
        joints.append(acc)
    return s, phi, r, joints


def hand_recipe(low, raster, skin_rgb):
    """The hand's surface gradients: its forms from the rig (knuckles, tendons, pads, nails, veins) and the creases
    painted into its skin made grooves."""
    rig = HandRig(low)
    T = low.T[raster.tri]
    Wt = np.einsum("ik,ikj->ij", raster.bary, rig.W[T])
    fw = np.zeros((len(raster.pix), 5))
    for j in range(Wt.shape[1]):
        f = rig.finger_of_joint[j]
        if f >= 0:
            fw[:, f] += Wt[:, j]
    # the thumb's metacarpal is the palm's (its ball)
    for n in ("thumb_1", "thumb_1_half", "thumb_1_quarter", "thumb_1_threequarter"):
        if n in rig.index:
            fw[:, 0] -= Wt[:, rig.index[n]]
    finger = np.where(fw.max(1) > 0.5, fw.argmax(1), -1)
    n = raster.n
    dorsal_w = smooth((-n[:, 1] - 0.1) / 0.5)  # the back of the hand and fingers
    palmar_w = smooth((n[:, 1] - 0.1) / 0.5)

    knuckles = []
    for f in range(1, 5):
        c = rig.rings[f][0] + rig.dorsal[f] * rig.radius[(f, 1)] * 0.9
        knuckles.append(c)
    mid_z = np.mean([k[2] for k in knuckles])
    wrist_x = min(r[0][0] for r in rig.rings.values()) - 7.5

    def palm_field(p, ctx):
        h = np.zeros(len(p))
        dw = ctx["dorsal"]
        pw = ctx["palmar"]
        for f in range(1, 5):
            c = knuckles[f - 1]
            # the knuckle: a broad dome over the joint, a little longer across than along
            d = p - c
            h += 0.50 * np.exp(-(d[:, 0] / 0.62) ** 2 - (d[:, 1] / 0.7) ** 2 - (d[:, 2] / 0.72) ** 2)
            # its extensor tendon, from the wrist to the knuckle, in the back of the hand, fading at both ends
            w0 = np.array([wrist_x + 3.0, c[1], mid_z + (c[2] - mid_z) * 0.7])
            dist, t = seg_dist(p[:, [0, 2]], w0[[0, 2]], c[[0, 2]])
            h += dw * 0.16 * gauss(dist, 0.23) * smooth((t - 0.05) / 0.3) * smooth((0.97 - t) / 0.25)
            # the pad under the finger in the palm
            q = np.array([c[0] - 0.9, 0.0, c[2]])
            h += pw * 0.22 * np.exp(-((p[:, 0] - q[0]) / 0.75) ** 2 - ((p[:, 2] - q[2]) / 0.9) ** 2)
        # the heel of the hand on the little finger's side (hypothenar)
        h += pw * 0.30 * np.exp(-((p[:, 0] - (wrist_x + 6.0)) / 2.2) ** 2 - ((p[:, 2] - (knuckles[3][2] + 0.3)) / 1.1) ** 2)
        # the ball of the thumb (thenar), over its metacarpal on the palm's side
        a0, a1 = rig.rings[0][0], rig.rings[0][1]
        c = a0 * 0.45 + a1 * 0.55 + np.array([0.0, 0.9, -0.5])
        h += pw * 0.36 * np.exp(-np.sum(((p - c) / np.array([1.6, 1.4, 1.1])) ** 2, 1))
        # two veins over the back, between the tendons, wandering a little
        for z0, z1, ph in ((knuckles[1][2] * 0.5 + knuckles[2][2] * 0.5, mid_z - 0.8, 0.0),
                           (knuckles[0][2] * 0.5 + knuckles[1][2] * 0.5, mid_z + 1.4, 1.7)):
            x = p[:, 0]
            t = np.clip((x - (wrist_x + 2.5)) / (knuckles[1][0] - 1.4 - (wrist_x + 2.5)), 0, 1)
            zc = z1 + (z0 - z1) * t + 0.25 * np.sin(t * 7.0 + ph)
            h += dw * 0.075 * gauss(p[:, 2] - zc, 0.14) * smooth(t / 0.15) * smooth((1 - t) / 0.2)
        return h

    def finger_field(p, ctx):
        f = ctx["f"]
        s, phi, r, J = finger_coords(p, rig.polyline(f), rig.dorsal[f])
        h = np.zeros(len(p))
        dphi = np.abs(phi)
        pal = np.abs(np.pi - dphi)
        L3 = J[3] - J[2]
        k1 = 0.0 if f else 1.0  # the thumb's first ring is at its metacarpal's base: its segments are one further
        # knuckles over the middle and end joints (the thumb's: over its two joints)
        for jj, amp, sl in ((1, 0.26, 0.42), (2, 0.17, 0.34)):
            h += amp * gauss(s - J[jj], sl) * gauss(dphi, 0.75)
        # the pads under each phalanx, and the fingertip's
        for a, b, amp in ((J[0], J[1], 0.17), (J[1], J[2], 0.16), (J[2], J[3], 0.21)):
            m = a + (b - a) * (0.55 if b != J[3] else 0.5)
            h += amp * gauss(s - m, (b - a) * 0.33) * gauss(pal, 0.95)
        # the nail: a plate over the end of the last phalanx, its root under a fold, its free edge at the tip
        start = J[2] + L3 * 0.40
        w = 0.62 + 0.1 * k1
        inside = smooth((s - start) / 0.10) * smooth((w - dphi) / 0.12)
        h += 0.06 * inside
        return h

    ctx_palm = {"dorsal": dorsal_w, "palmar": palmar_w}
    eps = nb.pixel_size(raster) * 0.5
    grad = np.zeros((len(raster.pix), 3))
    heights = np.zeros(len(raster.pix))
    pm = finger < 0
    sub_palm = SubRaster(raster, pm)
    grad[pm] = nb.gradient_3d(sub_palm, palm_field, eps[pm], {"dorsal": dorsal_w[pm], "palmar": palmar_w[pm]})
    heights[pm] = palm_field(raster.pos[pm], {"dorsal": dorsal_w[pm], "palmar": palmar_w[pm]})
    for f in range(5):
        m = finger == f
        if not m.any():
            continue
        sr = SubRaster(raster, m)
        ctx = {"f": f, "dorsal": dorsal_w[m], "palmar": palmar_w[m]}
        # near the palm the finger also carries the palm's knuckles and pads (the seam between them agrees)
        grad[m] = nb.gradient_3d(sr, lambda q, c: finger_field(q, c) + palm_field(q, c), eps[m], ctx)
        heights[m] = finger_field(raster.pos[m], ctx) + palm_field(raster.pos[m], ctx)
    # the creases, wrinkles and nail folds painted into the skin, as grooves
    cov = nb.coverage(low, 1)
    lines = nb.painted_lines(skin_rgb.astype(np.float64), cov)
    lines = nb.blur(lines, 0.6, cov)
    groove = nb.dilate(np.dstack([lines] * 3), cov, 8)[0][..., 0]
    field = nb.sampler(-0.085 * groove)
    grad += nb.gradient_uv(raster, field, d=0.35)
    heights += field(raster.uv)
    return grad, heights


class SubRaster:
    """Some entries of a raster, with what the gradients need."""

    def __init__(self, r, m):
        self.low = r.low
        self.scale = r.scale
        self.n, self.t, self.b = r.n[m], r.t[m], r.b[m]
        self.pos, self.uv, self.tri = r.pos[m], r.uv[m], r.tri[m]


# ----------------------------------------------------------------------------
# Bevels: the rounded edges of the hard creases the engine draws (where the faces on either side have their own
# normals), a few texels wide on each side, turning each face's normal half way towards the other's at the edge.


def smooth_normals(low, raster, angle=35.0):
    """The model's shape as its facets stand for it: at each entry, its corners' normals averaged over the faces round
    each corner that turn less than `angle` degrees from its own (a faceted tube is round; a box stays a box),
    interpolated. Where the engine already draws it smooth this is its own normal."""
    key = {}
    weld = np.empty(len(low.P), np.int64)
    for i, p in enumerate(map(tuple, np.round(low.P, 4))):
        weld[i] = key.setdefault(p, i)
    faces = {}
    for t, tri in enumerate(low.T):
        for v in tri:
            faces.setdefault(weld[v], []).append(t)
    c = math.cos(math.radians(angle))
    corner = np.zeros((len(low.T), 3, 3))
    for t, tri in enumerate(low.T):
        for k, v in enumerate(tri):
            fs = np.array(faces[weld[v]])
            ng = low.ng[fs]
            ok = ng @ low.ng[t] > c
            acc = (ng[ok] * low.area[fs][ok, None]).sum(0)
            corner[t, k] = acc / max(np.linalg.norm(acc), 1e-12)
    # where the engine's own normal is smooth across faces (its vertex normal differs from the face's), keep it
    own = low.N[low.T]
    flat = np.einsum("tkj,tj->tk", own, low.ng) > 0.9995
    corner = np.where(flat[..., None], corner, own)
    n = np.einsum("ik,ikj->ij", raster.bary, corner[raster.tri])
    return nb.normalize(n)


def crease_edges(low, min_split=20.0, min_angle=12.0):
    """[(side triangle, its texel-space ends (2, 2), axis, angle)] for each side of each hard crease: an edge two
    triangles share (by position) where their corners' normals differ by more than min_split degrees."""
    key = {}
    weld = np.empty(len(low.P), np.int64)
    for i, p in enumerate(map(tuple, np.round(low.P, 4))):
        weld[i] = key.setdefault(p, i)
    edges = {}
    for t, tri in enumerate(low.T):
        for k in range(3):
            a, b = tri[k], tri[(k + 1) % 3]
            e = (min(weld[a], weld[b]), max(weld[a], weld[b]))
            edges.setdefault(e, []).append((t, a, b))
    cs = math.cos(math.radians(min_split))
    out = []
    for e, sides in edges.items():
        if len(sides) != 2:
            continue
        (t1, a1, b1), (t2, a2, b2) = sides
        n1, n2 = low.ng[t1], low.ng[t2]
        ang = math.degrees(math.acos(max(-1.0, min(1.0, float(n1 @ n2)))))
        if ang < min_angle:
            continue
        c2 = {weld[a2]: a2, weld[b2]: b2}
        split = min(float(low.N[a1] @ low.N[c2[weld[a1]]]), float(low.N[b1] @ low.N[c2[weld[b1]]]))
        if split > cs:
            continue
        for (t, a, b), no in (((t1, a1, b1), n2), ((t2, a2, b2), n1)):
            nt = low.ng[t]
            axis = np.cross(nt, no)
            if np.linalg.norm(axis) < 1e-6:
                continue
            out.append((t, low.UV[[a, b]], axis / np.linalg.norm(axis), math.radians(ang)))
    return out


def rotate(v, w):
    """v (n, 3) turned by the rotation vectors w (n, 3) (Rodrigues)."""
    ang = np.linalg.norm(w, axis=1)
    k = w / np.maximum(ang, 1e-12)[:, None]
    c, s = np.cos(ang)[:, None], np.sin(ang)[:, None]
    return v * c + np.cross(k, v) * s + k * (np.einsum("ij,ij->i", k, v)[:, None]) * (1 - c)


def bevel_normals(low, raster, width=1.8, power=1.3, most=32.0, base=None, min_angle=12.0):
    """The engine's normals at each entry turned by the bevels of the creases near it (within `width` skin texels,
    measured on its own island: the creases of its triangle and of those sharing a vertex with it). The turn at the
    edge is half the crease's angle, at most `most` degrees: the same on a box's right-angled edges as on its
    chamfers', so that the faces sharing a tile of the skin agree."""
    recs = crease_edges(low, min_angle=min_angle)
    by_tri = {}
    for i, r in enumerate(recs):
        by_tri.setdefault(r[0], []).append(i)
    ring = [set() for _ in range(len(low.T))]
    vt = {}
    for t, tri in enumerate(low.T):
        for v in tri:
            vt.setdefault(v, []).append(t)
    for t, tri in enumerate(low.T):
        for v in tri:
            ring[t].update(vt[v])
    omega = np.zeros((len(raster.pix), 3))
    order = np.argsort(raster.tri, kind="stable")
    tris = raster.tri[order]
    starts = np.flatnonzero(np.r_[True, tris[1:] != tris[:-1]])
    ends = np.r_[starts[1:], len(tris)]
    for s0, s1 in zip(starts, ends):
        t = tris[s0]
        cand = [i for u in ring[t] for i in by_tri.get(u, ())]
        if not cand:
            continue
        idx = order[s0:s1]
        uv = raster.uv[idx]
        for i in cand:
            _, (pa, pb), axis, ang = recs[i]
            e = pb - pa
            L2 = max(e @ e, 1e-12)
            q = np.clip(((uv - pa) @ e) / L2, 0, 1)
            d = np.linalg.norm(uv - (pa + q[:, None] * e), axis=1)
            x = np.clip(1.0 - d / width, 0, 1)
            a = min(0.5 * ang, math.radians(most)) * x ** power
            omega[idx] += a[:, None] * axis
    return nb.normalize(rotate(raster.n if base is None else base, omega)), np.linalg.norm(omega, axis=1)


# ----------------------------------------------------------------------------
# Painted skins' materials (Quake's palette): wood and leather, the metals


def hsv(rgb):
    r, g, b = (rgb[..., i] / 255.0 for i in range(3))
    mx = np.maximum(np.maximum(r, g), b)
    mn = np.minimum(np.minimum(r, g), b)
    d = mx - mn
    h = np.where(d < 1e-6, 0.0,
                 np.where(mx == r, ((g - b) / np.maximum(d, 1e-6)) % 6,
                          np.where(mx == g, (b - r) / np.maximum(d, 1e-6) + 2, (r - g) / np.maximum(d, 1e-6) + 4))) * 60
    s = np.where(mx > 1e-6, d / np.maximum(mx, 1e-6), 0.0)
    return h, s, mx


def wood_mask(rgb, cov):
    """Wood (and leather): warm browns, saturated enough, not bright yellow or red (brass, glows)."""
    h, s, v = hsv(rgb.astype(np.float64))
    m = smooth((s - 0.28) / 0.12) * smooth((h - 12) / 6) * smooth((42 - h) / 6) * smooth((v - 0.06) / 0.05)
    return nb.blur(m, 1.2, cov) * cov


def grain(rgb, cov, wood, strength=0.55):
    """The painted grain of wooden parts (the skin's fine streaks) as heights, in texels."""
    lum = rgb[..., 0] * 0.3 + rgb[..., 1] * 0.59 + rgb[..., 2] * 0.11
    lum = lum / 255.0
    hp = nb.blur(lum, 0.7, cov) - nb.blur(lum, 2.5, cov)
    sd = np.sqrt(nb.blur(hp * hp, 4.0, cov)) + 0.01
    return np.clip(hp / sd, -2, 2) * 0.5 * strength * wood


def weapon_heights(low, skin, lines_depth=0.8, grain_depth=0.55):
    """A view model's texel-space heights (texels): the dark seams painted into its skin as V grooves (panel lines),
    the painted grain of its wooden parts."""
    rgb = skin.astype(np.float64)
    cov = nb.coverage(low, 1)
    lines = nb.painted_lines(rgb, cov, lo=0.12, hi=0.4, min_len=6.0, full_len=16.0)
    lines = nb.blur(lines, 0.5, cov)
    wood = wood_mask(rgb, cov)
    h = -lines_depth * lines * (1 - 0.6 * wood) + grain(rgb, cov, wood, grain_depth)
    h = nb.dilate(np.dstack([h] * 3), cov, 6)[0][..., 0]
    return h, lines, wood


SMOOTH_ANGLE = 35.0


def mdl_bake(low, scale, skin, bevel_width=1.8, lines_depth=0.8, grain_depth=0.55, supersample=2):
    """A view model's or generated model's map: its creases bevelled, its painted seams grooved, its wood grained."""
    r = nb.Raster(low, scale * supersample)
    nw, _ = bevel_normals(low, r, width=bevel_width, base=smooth_normals(low, r, SMOOTH_ANGLE), min_angle=SMOOTH_ANGLE)
    h, _, _ = weapon_heights(low, skin, lines_depth, grain_depth)
    detail = nb.tangent_uv(r, nb.sampler(h), d=0.35)
    img, keep = nb.finish(r, nw, detail, supersample=supersample)
    return img
