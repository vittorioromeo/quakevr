#!/usr/bin/env python3
# make_hand_feminine.py -- the jointed hand's second shape, "feminine" (vr_hand_shape 1; ROUND22.md, "Feminine hand"):
#   quakevr/progs/hand_rig_f.md5mesh, .md5anim   the hand, reshaped
#   quakevr/progs/hand_rig_f.mdl                 its placeholder (Ironwail loads an MD5 only as an .mdl's replacement)
#
# Usage: py -3.13 Misc/quakevr/make_hand_feminine.py [--stats] [--preview out.png]
#   --stats    the two shapes' sizes (widths, thicknesses, lengths) and how far the vertices moved
#   --preview  a quick picture of both hands from the back, the side and the palm (standard grey, feminine red)
#
# Made from the shipped hand (quakevr/progs/hand_rig.md5mesh, the author's edit in Blender: docs/vr-port/
# HANDS_IN_BLENDER.md) by moving its vertices only. What stays exactly as the standard hand's:
#   - the joints (every bone's place, so every pivot, hinge and curl frame) and the weights: the grasp solver, the
#     weapons' grips and hotspots, two-handed grips, the trigger finger, the pouch and the reloads line up unchanged;
#   - the topology and the texture coordinates (same 455 vertices, 656 triangles, same islands): the skins (clean and
#     bloodied), their normal map, the skin masks (skin tone, gloves), the gloves' painted leather and relief and the
#     nail masks all lie on the same places of the hand, so the shader says "hand_rig" and the model uses them as they
#     are (each texel lands where it did, on a slimmer surface). The gloves' shells are made from this mesh as it loads
#     (vr_handrig.cpp gloveShells), and the grasp solver's spheres refitted to it as it loads (vr_handrig.cpp derive:
#     each moved and resized with the skin round it).
# What changes (rig space, hand units, about 1.2 cm):
#   - the fingers and the thumb: each cross-section narrower and thinner about its own middle (FINGER_WIDTH,
#     FINGER_THICK, THUMB_SIZE), the joints' rings a little more (finer knuckles, KNUCKLE_FINE);
#   - the fingertips tapered (TIP_TAPER) and a little shorter (TIP_LENGTH), the nail's base ring moved towards the
#     last joint (NAIL_BASE): a longer nail bed on a shorter tip (the nail plate is painted between that ring and the
#     tip: it stretches with them);
#   - the palm: narrower about its middle towards the wrist (PALM_WIDTH), only its edges drawn in at the knuckles
#     (the knuckles are the fingers' pivots), thinner from the back (PALM_THICK: the palm's side stays where the
#     handles are); the ball of the thumb kept fuller (THENAR_KEEP); the wrist slimmer, the ring inside the forearm's
#     cuff a little only (the bare forearm runs on into it: WRIST_*).
#
# Run it again after editing the standard hand (it reshapes whatever hand_rig.md5mesh is). The feminine hand can be
# edited in Blender itself as the standard one can (import hand_rig_f.md5mesh); then this refuses to overwrite it
# (genguard.py: --keep-edited, --force).

import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "blender", "addons", "quakevr_hand"))
import genguard  # noqa: E402
import md5hand  # noqa: E402

PROGS = os.path.normpath(os.path.join(HERE, "..", "..", "quakevr", "progs"))

FINGER_WIDTH = 0.86  # the fingers' cross-sections across (side to side), of the standard's
FINGER_THICK = 0.92  # ... and through (back to palm)
THUMB_SIZE = 0.90  # the thumb's (both ways alike)
KNUCKLE_FINE = 0.96  # the rings at the middle and last joints, a little more (finer knuckles)
KNUCKLE_REACH = 0.35  # ... falling off over this distance from the joint
TIP_TAPER = 0.09  # the fingertips narrowed by this much more at their end, from TIP_TAPER_FROM of the last segment
TIP_TAPER_FROM = 0.3
TIP_LENGTH = 0.93  # the last segment's length past its joint, of the standard's
NAIL_BASE = (0.45, 0.30)  # the ring under the nail's base: from this share of the last segment to this one
PALM_WIDTH = [(-6.5, 0.97), (-5.2, 0.97), (-4.4, 0.93), (-2.5, 0.89), (0.1, 0.90), (2.0, 0.92)]  # across, by x
PALM_EDGE = 0.86  # at the knuckles: the palm's edges past the outer fingers' axes drawn in by this
PALM_KNUCKLES = (2.0, 4.2)  # from the middle mode (about the palm's middle) to the knuckles' (x)
PALM_THICK = [(-6.5, 0.98), (-5.2, 0.98), (-4.4, 0.95), (-2.5, 0.93), (2.0, 0.94), (6.0, 0.94)]  # back to palm, by x
THENAR_KEEP = 0.6  # the ball of the thumb narrowed this much less (its share of the thumb's weights)

JOINTS = md5hand.JOINTS
THUMB = 0


def unit(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


def smooth(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


def table(x, pts):
    return float(np.interp(x, [p[0] for p in pts], [p[1] for p in pts]))


def finger_of(j):
    """The finger joint j moves with, or -1 (the palm)."""
    _, kind, fi, _, _ = JOINTS[j]
    return -1 if kind == md5hand.PALM else fi


def load(path):
    with open(path) as f:
        mesh = md5hand.parse_md5mesh(f.read(), os.path.basename(path))
    rest = np.array([md5hand.rest_position(mesh["joints"], md5hand.vertex_weights(mesh, v)) for v in range(len(mesh["verts"]))])
    return mesh, rest


def islands(mesh):
    """Each vertex's island (the triangles' connected pieces: the skin's islands, the file splits vertices on its
    seams), labelled by the finger most of its weight moves with (-1 the palm)."""
    n = len(mesh["verts"])
    parent = list(range(n))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for a, b, c in mesh["tris"]:
        for u, v in ((a, b), (b, c)):
            ru, rv = find(u), find(v)
            if ru != rv:
                parent[ru] = rv
    root = [find(v) for v in range(n)]
    votes = {}
    for v in range(n):
        w = md5hand.vertex_weights(mesh, v)
        j = max(w, key=lambda x: x[1])[0]
        votes.setdefault(root[v], []).append(finger_of(j))
    label = {r: max(set(vs), key=vs.count) for r, vs in votes.items()}
    sizes = {}
    for r, vs in votes.items():
        sizes.setdefault(label[r], []).append(len(vs))
    assert sorted(label.values()) == [-1, 0, 1, 2, 3, 4], "the hand's islands aren't a palm and five fingers: %s" % sizes
    return np.array([label[r] for r in root])


class Finger:
    """A finger's (or the thumb's) bones at rest: its three pivots and its tip (the furthest of its skin)."""

    def __init__(self, mesh, fi, pts):
        js = mesh["joints"]
        self.p = [np.array(js[md5hand.segment_joint(fi, b)][2], float) for b in (1, 2, 3)]
        self.x = [unit(self.p[1] - self.p[0]), unit(self.p[2] - self.p[1])]
        # the last segment's direction: towards the middle of its far end (the thumb's turns from the one before)
        along = (pts - self.p[2]) @ self.x[1]
        far = pts[along > 0.7 * along.max()]
        self.x.append(unit(far.mean(0) - self.p[2]))
        self.tip = float(((pts - self.p[2]) @ self.x[2]).max())
        self.lengths = [np.linalg.norm(self.p[1] - self.p[0]), np.linalg.norm(self.p[2] - self.p[1]), self.tip]
        self.cum = [0.0, self.lengths[0], self.lengths[0] + self.lengths[1]]
        self.fi = fi

    def place(self, q):
        """(segment 0..2, distance along it, distance along the finger) of a point."""
        best = None
        for k in range(3):
            d = q - self.p[k]
            t = float(d @ self.x[k])
            hi = self.lengths[k] if k < 2 else 1e9
            tc = min(max(t, 0.0), hi)
            e = np.linalg.norm(d - self.x[k] * tc)
            if best is None or e < best[0] - 1e-9:
                best = (e, k, t)
        _, k, t = best
        return k, t, self.cum[k] + t

    def frame(self, k):
        """The segment's direction, towards the palm's side, across (to the thumb's side)."""
        x = self.x[k]
        y = np.array([0.0, 1.0, 0.0])
        y = unit(y - x * (y @ x))
        return x, y, np.cross(x, y)


def reshape(mesh, rest):
    part = islands(mesh)
    # One place per position (the file's copies of a vertex on a seam move together): a finger's copy over the palm's
    # (its first ring is the palm's front), and a web's (two fingers') moves half as each would.
    key = [tuple(p) for p in rest]
    owners = {}
    for v, k in enumerate(key):
        owners.setdefault(k, set()).add(int(part[v]))
    fingers = {}
    for fi in range(5):
        fingers[fi] = Finger(mesh, fi, rest[part == fi])

    # Each finger's sections' middles: the mean of its skin near a place along it (a soft window: its own ring).
    def centre(fi, s, q):
        f = fingers[fi]
        pts = rest[part == fi]
        ss = np.array([f.place(p)[2] for p in pts])
        w = np.exp(-((ss - s) / 0.22) ** 2)
        c = (pts * w[:, None]).sum(0) / w.sum()
        k, _, _ = f.place(q)
        x = f.x[k]
        return c + x * ((q - c) @ x)  # on the plane through q across the finger

    def finger_move(fi, q):
        f = fingers[fi]
        k, t, s = f.place(q)
        c = centre(fi, s, q)
        x, y, z = f.frame(k)
        r = q - c
        rx, ry, rz = r @ x, r @ y, r @ z
        if fi == THUMB:
            kw = kt = THUMB_SIZE
        else:
            kw, kt = FINGER_WIDTH, FINGER_THICK
        # finer knuckles: the rings at the middle and last joints
        near = min(abs(s - f.cum[1]), abs(s - f.cum[2]))
        fine = 1.0 - (1.0 - KNUCKLE_FINE) * max(0.0, 1.0 - near / KNUCKLE_REACH)
        kw *= fine
        kt *= math.sqrt(fine)
        # the fingertip: tapered, shorter, its nail's base ring nearer the joint
        along = 0.0
        if k == 2 and t > 0.0:
            a = t / f.tip
            taper = 1.0 - TIP_TAPER * smooth((a - TIP_TAPER_FROM) / (1.0 - TIP_TAPER_FROM))
            kw *= taper
            kt *= 1.0 - 0.6 * (1.0 - taper)
            a0, a1 = NAIL_BASE
            g = a * a1 / a0 if a < a0 else a1 + (a - a0) * (TIP_LENGTH - a1) / (1.0 - a0)
            along = (g - a) * f.tip
        return c + x * (rx + along) + y * (ry * kt) + z * (rz * kw)

    palm = rest[part == -1]
    xs = np.linspace(palm[:, 0].min(), palm[:, 0].max(), 24)

    def band(x, col, fn):
        d = np.abs(palm[:, 0] - x)
        sel = d <= max(0.6, np.sort(d)[min(12, len(d) - 1)])  # (at least the 13 nearest: a section's worth)
        return fn(palm[sel, col])

    mid_z = [0.5 * (band(x, 2, np.min) + band(x, 2, np.max)) for x in xs]
    palm_y = [band(x, 1, np.max) for x in xs]
    axes_z = sorted(fingers[fi].p[0][2] for fi in range(1, 5))  # the knuckles across: pinky .. index

    def thumb_share(v):
        return sum(b for j, b, _ in md5hand.vertex_weights(mesh, v) if finger_of(j) == THUMB)

    def palm_move(q, v):
        x, y, z = q
        share = thumb_share(v)
        # across: about the palm's middle; at the knuckles only the edges past the outer fingers
        k = table(x, PALM_WIDTH)
        k = 1.0 - (1.0 - k) * (1.0 - THENAR_KEEP * share)
        zm = float(np.interp(x, xs, mid_z))
        z_mid = zm + (z - zm) * k
        lo, hi = axes_z[0], axes_z[-1]
        ke = 1.0 - (1.0 - PALM_EDGE) * (1.0 - THENAR_KEEP * share)
        z_kn = lo + (z - lo) * ke if z < lo else hi + (z - hi) * ke if z > hi else z
        m = smooth((x - PALM_KNUCKLES[0]) / (PALM_KNUCKLES[1] - PALM_KNUCKLES[0]))
        z2 = (1 - m) * z_mid + m * z_kn
        # through: thinner from the back
        yp = float(np.interp(x, xs, palm_y))
        y2 = yp - (yp - y) * table(x, PALM_THICK)
        return np.array([x, y2, z2])

    out = rest.copy()
    done = {}
    for v, k in enumerate(key):
        if k in done:
            out[v] = done[k]
            continue
        own = sorted(o for o in owners[k] if o >= 0)
        q = rest[v]
        if own:
            p = np.mean([finger_move(fi, q) for fi in own], axis=0)
        else:
            p = palm_move(q, v)
        done[k] = p
        out[v] = p
    return out, part


def sizes(mesh, rest, part):
    """A few measures of a hand: the palm's width at the knuckles, its middle, the wrist; its thickness; each finger's
    width and thickness at its middle segment; the hand's length (wrist to the middle finger's tip)."""
    js = mesh["joints"]
    out = {}
    palm = rest[part == -1]
    for name, x in (("cuff", -6.4), ("wrist", -4.5), ("heel", -2.5), ("palm", 0.2), ("distal palm", 2.6)):
        sel = np.abs(palm[:, 0] - x) < 0.35
        out[name + " width"] = palm[sel, 2].max() - palm[sel, 2].min()
        out[name + " thick"] = palm[sel, 1].max() - palm[sel, 1].min()
    for fi, n in enumerate(md5hand.FINGERS):
        f = Finger(mesh, fi, rest[part == fi])
        pts = rest[part == fi]
        mid = 0.5 * (f.p[0] + f.p[1])  # the first segment's middle ring
        x, y, z = f.frame(0)
        d = np.abs((pts - mid) @ x)
        sel = d <= max(0.3, np.sort(d)[7])  # (its ring: 8 vertices)
        out[n + " width"] = np.ptp((pts[sel] - mid) @ z)
        out[n + " thick"] = np.ptp((pts[sel] - mid) @ y)
        out[n + " tip"] = f.tip
    mf = Finger(mesh, 2, rest[part == 2])
    out["length"] = (mf.p[2] + mf.x[2] * mf.tip)[0] - (-4.4)
    out["across all"] = np.ptp(rest[:, 2])
    return out


def preview(path, rest_a, rest_b, tris):
    """Back (looking down -y... +y), side and palm views, both hands overlaid (grey the standard, red the feminine)."""
    from PIL import Image
    S, W, H = 24, 3, 1
    views = [(0, 2, -1), (0, 1, 1), (0, 2, 1)]  # (screen x axis, screen y axis, depth sign along the third)
    img = np.full((H * 560, W * 560, 3), 255, np.uint8)
    for vi, (ax, ay, _) in enumerate(views):
        for rest, col in ((rest_a, (150, 150, 150)), (rest_b, (220, 40, 40))):
            ox, oy = vi * 560 + 280, 280
            for tri in tris:
                P = rest[list(tri)]
                X = ox + (P[:, ax] - 4.0) * S
                Y = oy - (P[:, ay] - 0.5) * S
                xs = np.arange(int(max(X.min(), vi * 560)), int(min(X.max() + 1, vi * 560 + 559)))
                ys = np.arange(int(max(Y.min(), 0)), int(min(Y.max() + 1, 559)))
                if not len(xs) or not len(ys):
                    continue
                gx, gy = np.meshgrid(xs + 0.5, ys + 0.5)
                (x0, x1, x2), (y0, y1, y2) = X, Y
                d = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
                if abs(d) < 1e-9:
                    continue
                l1 = ((gx - x0) * (y2 - y0) - (x2 - x0) * (gy - y0)) / d
                l2 = ((x1 - x0) * (gy - y0) - (gx - x0) * (y1 - y0)) / d
                inside = (l1 >= 0) & (l2 >= 0) & (l1 + l2 <= 1)
                iy, ix = np.nonzero(inside)
                if col[0] == 150:
                    img[ys[iy], xs[ix]] = col
                else:
                    edge = inside & ((l1 < 0.06) | (l2 < 0.06) | (l1 + l2 > 0.94))
                    ey, ex = np.nonzero(edge)
                    img[ys[ey], xs[ex]] = col
    Image.fromarray(img).save(path)


def main():
    stats = "--stats" in sys.argv
    prev = sys.argv[sys.argv.index("--preview") + 1] if "--preview" in sys.argv else None
    src = os.path.join(PROGS, "hand_rig.md5mesh")
    outs = [os.path.join(PROGS, n) for n in ("hand_rig_f.md5mesh", "hand_rig_f.md5anim", "hand_rig_f.mdl")]
    mesh, rest = load(src)
    new, part = reshape(mesh, rest)
    moved = np.linalg.norm(new - rest, axis=1)
    print("hand_rig_f: %d vertices, %d triangles; moved %.3f units at most, %.3f on average" % (
        len(rest), len(mesh["tris"]), moved.max(), moved.mean()))
    if stats:
        a, b = sizes(mesh, rest, part), sizes(mesh, new, part)
        for k in a:
            print("  %-16s %7.3f -> %7.3f  (%+.1f%%)" % (k, a[k], b[k], 100.0 * (b[k] / a[k] - 1.0)))
    if prev:
        preview(prev, rest, new, mesh["tris"])
        print("preview -> %s" % prev)
    if stats or prev:
        return
    guard = genguard.Guard("make_hand_feminine.py", outs)
    joints = [(n, parent, pos) for n, parent, pos, _ in mesh["joints"]]
    verts = []
    for v, (s, t, first, count) in enumerate(mesh["verts"]):
        infl = [(j, b) for j, b, _ in mesh["weights"][first:first + count]]
        verts.append(((s, t), infl, tuple(new[v])))
    cmd = "Misc/quakevr/make_hand_feminine.py"
    md5hand.write_md5mesh(outs[0], joints, verts, mesh["tris"], mesh["shader"] or "hand_rig", cmd)
    md5hand.write_md5anim(outs[1], joints, [tuple(p) for p in new], cmd)
    import make_hand_rig
    make_hand_rig.write_placeholder_mdl(outs[2])
    guard.finish()
    print("-> %s" % ", ".join(os.path.basename(o) for o in outs))


if __name__ == "__main__":
    main()
