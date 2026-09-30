#!/usr/bin/env python3
# make_crowbar.py -- the crowbar (WID_CROWBAR, QC weapons.qc W_CrowbarMelee; docs/vr-port/ROUND21.md, "The crowbar"):
# quakevr/progs/v_crowbar.mdl, built here from scratch (an original design: no model or texture is copied).
#
# A gooseneck wrecking bar, 67 cm: a hexagonal steel bar (2.2 cm across the flats), a flat chisel at its lower end
# (bent a little off the bar's line), a black tape grip over its lower third (both hands' room, as a sword's handle),
# and at its upper end a hook bent through 160 degrees ending in a forked claw (a nail puller). Painted a worn dark red,
# its paint chipped along the bar's edges and rubbed off at both working ends, where the ground steel shows.
#
# It is laid where the knights' swords are (make_swords.py: along the axe's handle in progs/v_axe.mdl, the hand closing
# just under a sword's crossguard), so the swords' weapon settings hold it the same way (vr_weapons.inc, slot 23: the
# cvars' _24; its Offset is the sword's moved for the model's other bounds, as the weapon scales about their corner):
# the main hand's fist on the tape's upper part (21 cm from the chisel's tip), the other hand's place for a two-handed
# grip below it on the tape (Hotspot 1, a Grip), and the bar above the hands (Hotspot 2, a Blade: the off hand slides
# along the upper part, short of the hook). The hook curls towards the axe head's side (the swords' edges): a chop
# lands with the hook's claw, as a pick's. Its far end (the weapon settings' MuzzleAnchorVertex: the melee line's tip,
# the parry line's end, the Blade grip's axis, which runs through it along the bar) is the hook's back where the bar's
# line carried on up meets it (a ring of the bend put there: tip_angle).
#
# The bar is a loft of rings along its centre line (8 points: the hexagon's 6 corners and the middles of the two flats
# facing the bend's way, so the claw's end can be forked), each flat its own strip of vertices: flat-shaded across the
# bar (the hexagon's facets, as the id models), smooth along it (the bend). The skin (512 x 64) unrolls the bar: s along
# its length, t round it; the two end faces sample the bare steel.
# Nine identical frames (a weapon's frame numbers).
#
# Usage: python Misc/quakevr/make_crowbar.py [output progs folder] [--keep-edited | --force]
# Then: python Misc/quakevr/bake_normals.py v_crowbar.mdl (its normal map), and check the anchor printed below against
# vr_weapons.inc's slot 23 MuzzleAnchorVertex (200).

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import genguard  # noqa: E402
import mdlgen  # noqa: E402
from mdlgen import add, cross, dot, mul, norm, sub  # noqa: E402

FRAMES = 9
SKIN_W, SKIN_H = 512, 64

# Centimetres to the weapon model's units: the swords' Scale (0.34) and 3.81 cm a Quake unit (Quake/vr/vr_units.hpp).
K = 1.0 / (3.81 * 0.34)

# The swords' frame in v_axe.mdl's space (make_swords.py): the handle's bottom and direction, the crossguard's foot
# 12.5 units up it (the hand closes under it).
HANDLE_BOTTOM = (0.2, 0.0, -3.55)
HANDLE_DIR = (4.2, 0.0, 11.55)
GRIP_TOP = 12.5
TIP_BELOW_GUARD = 30.0  # cm: the chisel's tip below the crossguard's foot (the main fist's middle 22 cm up the bar)

# The bar (cm): half the thickness across the flats (a) and the hexagon's corner radius (b); the hook's bend radius
# (of the centre line) and where it starts; the claw.
A = 1.1
B = A / math.cos(math.radians(30))
BEND_R = 3.8
BEND_AT = 62.5
BEND_END = 160.0  # degrees the hook turns through
CLAW_LEN = 4.5
NOTCH = 1.9       # the claw's fork, deep
CHISEL_BEND = 10.0  # degrees the chisel is bent off the bar, over its last 6 cm
TAPE = (6.6, 29.0)  # the grip's tape, from and to (cm up the bar)
TAPE_K = 1.12       # the tape's thickness (the bar's section scaled)

# The ring: (N, V) in units of (A, B): N across the flats, towards the hook's inside; V along the hook's axis of bending.
RING = [(1, 0), (1, 0.5), (0, 1), (-1, 0.5), (-1, 0), (-1, -0.5), (0, -1), (1, -0.5)]
FLATS = [(k, (k + 1) % 8) for k in range(8)]  # each flat between two ring points

# Palette ramps (Quake's palette; dark to light).
PAINT = [65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79]  # (Quake's only reds: lit at 0.45, a deep red)
PAINT_EDGE = [64, 65, 66, 67, 16, 17]            # paint's broken rim, dark
STEEL = [2, 3, 4, 34, 5, 35, 6, 36, 7, 37, 8, 9, 10]  # greys and blue-greys: forged steel, ground bright only at the edges
TAPE_RAMP = [0, 0, 1, 1, 2, 2, 3, 4]
RUST = [16, 17, 18, 97, 98, 99, 100, 101]


class Station:
    """A ring's place: the centre line's point (u, w), its tangent and in-plane normal (unit, in (u, w)), how far along
    the bar it is (cm), the section's scale across (N) and along (V) the bend's axis, and per ring point how far it is
    pulled back along the tangent (the claw's fork)."""

    def __init__(self, p, t, s, kn=1.0, kv=1.0, back=None, kn_at=None):
        self.p, self.t, self.s, self.kn, self.kv = p, t, s, kn, kv
        self.n = (t[1], -t[0])  # the tangent turned: at the bar (t = +w) it is +u, the hook's inside
        self.back = back or [0.0] * 8
        self.kn_at = kn_at or {}  # ring points of another thickness (the fork's root)

    def point(self, k):
        nn, vv = RING[k]
        kn = self.kn_at.get(k, self.kn)
        u = self.p[0] + self.n[0] * nn * A * kn - self.t[0] * self.back[k]
        w = self.p[1] + self.n[1] * nn * A * kn - self.t[1] * self.back[k]
        return (u, vv * B * self.kv, w)


def stations():
    out = []
    # The chisel: bent CHISEL_BEND off the bar over its last 6 cm, thinning to an edge, a little wider.
    tc = (math.sin(math.radians(CHISEL_BEND)), math.cos(math.radians(CHISEL_BEND)))
    chisel = [(0.0, 0.16, 1.12), (0.6, 0.3, 1.15), (1.5, 0.5, 1.12), (3.0, 0.72, 1.06), (4.5, 0.9, 1.02)]
    for w, kn, kv in chisel:
        back = [0.0] * 8
        if w == 0.0:
            back[2] = back[6] = 0.35  # the edge's corners chamfered
        out.append(Station((-(6.0 - w) * tc[0], w * tc[1]), tc, w, kn, kv, back))
    w6 = 6.0 * math.cos(math.radians(CHISEL_BEND))
    # The bar from the chisel's bend up, the tape over the grip.
    shaft = [(6.0, 1.0), (TAPE[0], 1.0), (TAPE[0] + 0.2, TAPE_K), ((TAPE[0] + TAPE[1]) / 2, TAPE_K), (TAPE[1] - 0.2, TAPE_K),
             (TAPE[1], 1.0), (40.0, 1.0), (51.0, 1.0), (BEND_AT, 1.0)]
    for w, k in shaft:
        out.append(Station((0.0, w6 + (w - 6.0)), (0.0, 1.0), w, k, k))
    top = w6 + BEND_AT - 6.0
    s0 = BEND_AT
    # The hook: a bend of BEND_R about (BEND_R, top), towards +u.
    # (Every 15 degrees, and where the bar's line carried on up meets the hook's back: the muzzle anchor, TIP_STATION.)
    angles = sorted(set([BEND_END * i / int(BEND_END / 15) for i in range(1, int(BEND_END / 15) + 1)] + [tip_angle()]))
    for deg in angles:
        th = math.radians(deg)
        out.append(Station((BEND_R * (1 - math.cos(th)), top + BEND_R * math.sin(th)), (math.sin(th), math.cos(th)),
                           s0 + BEND_R * th))
    # The claw: straight on, flattening to a forked edge.
    th = math.radians(BEND_END)
    t = (math.sin(th), math.cos(th))
    p0 = out[-1].p
    s1 = out[-1].s
    root = CLAW_LEN - NOTCH  # the fork's root
    claw = [(1.2, 0.8, 1.05), (root, 0.55, 1.1), (CLAW_LEN, 0.2, 1.18)]
    for d, kn, kv in claw:
        back = [0.0] * 8
        kn_at = {}
        if d == CLAW_LEN:
            # The fork: the flats' middles pulled back to its root (as thick as the bar is there), the prongs either side.
            back[0] = back[4] = NOTCH - 0.02
            kn_at = {0: 0.55, 4: 0.55}
            back[2] = back[6] = 0.2  # the outer corners a little rounded
        out.append(Station(add2(p0, mul2(t, d)), t, s1 + d, kn, kv, back, kn_at))
    return out


def tip_angle():
    """Degrees into the bend where the hook's back (its outer flat's middle, ring point 4) crosses the bar's line
    (u = 0): the far end of the melee line and the Blade hotspot's axis (vr_twohand.cpp bladeGripHand: the blade's
    axis runs through the muzzle point)."""
    return math.degrees(math.acos(BEND_R / (BEND_R + A)))


def add2(a, b):
    return (a[0] + b[0], a[1] + b[1])


def mul2(a, k):
    return (a[0] * k, a[1] * k)


# ----------------------------------------------------------------------------
# The mesh

S_PER_CM = 6.2
S0 = 12          # the skin's s at the chisel's tip
T_BAR = (2, 46)  # the skin's rows round the bar
CAP_ST = (4, 56)  # the end faces: a patch of bare steel


def ring_t(k):
    """The skin's row of ring point k (round the bar: its share of the perimeter)."""
    lengths = []
    for a, b in FLATS:
        na, va = RING[a]
        nb, vb = RING[b]
        lengths.append(math.hypot((na - nb) * A, (va - vb) * B))
    total = sum(lengths)
    return T_BAR[0] + (T_BAR[1] - T_BAR[0]) * sum(lengths[:k]) / total


class Out:
    def __init__(self):
        self.mesh = mdlgen.Mesh(SKIN_W, SKIN_H, {})
        self.normals = []  # accumulated per vertex before normalizing

    def vert(self, p, st):
        self.mesh.verts.append((p, (0.0, 0.0, 1.0), st))
        self.normals.append((0.0, 0.0, 0.0))
        return len(self.mesh.verts) - 1

    def tri(self, a, b, c, outward):
        """Clockwise seen from outside (Quake's front faces); degenerate ones dropped. Adds its normal to its vertices'."""
        p0, p1, p2 = (self.mesh.verts[i][0] for i in (a, b, c))
        n = cross(sub(p1, p0), sub(p2, p0))
        if dot(n, n) < 1e-10:
            return
        if dot(n, outward) > 0:
            b, c = c, b
            n = mul(n, -1.0)
        self.mesh.tris.append((a, b, c))
        for i in (a, b, c):
            self.normals[i] = add(self.normals[i], mul(n, -1.0))


def build():
    st = stations()
    out = Out()
    # Each flat's two edges at every station: its own vertices (hard edges round the bar, smooth along it).
    cols = []
    for f, (ka, kb) in enumerate(FLATS):
        col = []
        ta, tb = ring_t(f), ring_t(f + 1)
        for s in st:
            sa = int(round(S0 + s.s * S_PER_CM))
            col.append((out.vert(s.point(ka), (sa, int(round(ta)))), out.vert(s.point(kb), (sa, int(round(tb))))))
        cols.append(col)
    for f in range(8):
        for i in range(len(st) - 1):
            a0, b0 = cols[f][i]
            a1, b1 = cols[f][i + 1]
            mid = mul(add(add(st[i].point(FLATS[f][0]), st[i].point(FLATS[f][1])),
                          add(st[i + 1].point(FLATS[f][0]), st[i + 1].point(FLATS[f][1]))), 0.25)
            centre = mul(add((st[i].p[0], 0.0, st[i].p[1]), (st[i + 1].p[0], 0.0, st[i + 1].p[1])), 0.5)
            outward = sub(mid, centre)
            out.tri(a0, b0, b1, outward)
            out.tri(a0, b1, a1, outward)
    # The end faces (flat: their own vertices), laid as strips from the +N side to the -N side (the claw's fork's walls).
    for s, sign in ((st[0], -1.0), (st[-1], 1.0)):
        outward = (s.t[0] * sign, 0.0, s.t[1] * sign)
        ids = [out.vert(s.point(k), (CAP_ST[0] + k, CAP_ST[1] + (k % 3))) for k in range(8)]
        # 0 (1,0) 1 (1,.5) 2 (0,1) 3 (-1,.5) 4 (-1,0) 5 (-1,-.5) 6 (0,-1) 7 (1,-.5)
        out.tri(ids[2], ids[1], ids[3], outward)
        out.tri(ids[1], ids[0], ids[4], outward)
        out.tri(ids[1], ids[4], ids[3], outward)
        out.tri(ids[0], ids[7], ids[5], outward)
        out.tri(ids[0], ids[5], ids[4], outward)
        out.tri(ids[6], ids[5], ids[7], outward)
    verts = []
    for (p, _, uv), n in zip(out.mesh.verts, out.normals):
        verts.append((p, norm(n) if dot(n, n) > 1e-12 else (0.0, 0.0, 1.0), uv))
    out.mesh.verts = verts
    return out.mesh, st


def to_model(mesh):
    """From the bar's own frame (cm: u towards the hook, v across, w up the bar from the chisel's tip) into the swords'
    place in the axe's model space."""
    ax = norm(HANDLE_DIR)
    x = norm(sub((1.0, 0.0, 0.0), mul(ax, ax[0])))  # the way the axe's head points
    y = cross(ax, x)
    foot = add(HANDLE_BOTTOM, mul(ax, GRIP_TOP))

    def place(p):
        u, v, w = p
        return add(foot, add(mul(x, u * K), add(mul(y, v * K), mul(ax, (w - TIP_BELOW_GUARD) * K))))

    def turn(n):
        return norm(add(mul(x, n[0]), add(mul(y, n[1]), mul(ax, n[2]))))

    mesh.verts = [(place(p), turn(n), uv) for p, n, uv in mesh.verts]
    return place


# ----------------------------------------------------------------------------
# The skin

def hash2(i, j, seed):
    h = (i * 374761393 + j * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0


def value_noise(x, y, cell, seed):
    """Smooth noise 0..1 over cells of `cell` texels, wrapping round the bar (y)."""
    gx, gy = x / cell, y / cell
    x0, y0 = int(math.floor(gx)), int(math.floor(gy))
    fx, fy = gx - x0, gy - y0
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    rows = max(1, int(round((T_BAR[1] - T_BAR[0]) / cell)))

    def v(i, j):
        return hash2(i, j % rows, seed)

    a = v(x0, y0) + (v(x0 + 1, y0) - v(x0, y0)) * fx
    b = v(x0, y0 + 1) + (v(x0 + 1, y0 + 1) - v(x0, y0 + 1)) * fx
    return a + (b - a) * fy


def pick(ramp, level):
    return ramp[max(0, min(len(ramp) - 1, int(round(level * (len(ramp) - 1)))))]


def paint(st):
    """The skin's palette indices."""
    s_tip = S0
    s_end = S0 + st[-1].s * S_PER_CM
    s_neck = S0 + st[-1].s * S_PER_CM - CLAW_LEN * S_PER_CM  # the claw's root
    s_tape = (S0 + TAPE[0] * S_PER_CM, S0 + TAPE[1] * S_PER_CM)
    corners = [ring_t(k) for k in (1, 2, 3, 5, 6, 7)]  # the hexagon's corners (edges of the bar): worn first
    px = bytearray(SKIN_W * SKIN_H)
    for t in range(SKIN_H):
        for s in range(SKIN_W):
            d = hash2(s, t, 7)
            if t >= T_BAR[1] + 2 or t < T_BAR[0] - 1:
                # The end faces' patch and the unused rows: ground steel.
                px[t * SKIN_W + s] = pick(STEEL, 0.6 + 0.3 * value_noise(s * 0.3, t, 1.5, 23))
                continue
            tt = min(max(t + 0.5, T_BAR[0]), T_BAR[1])
            edge = min(abs(tt - c) for c in corners)
            # Bare steel at the working ends, the paint's border ragged.
            ragged = 5.0 * value_noise(s, t, 4.0, 11)
            if s < s_tip + 4.5 * S_PER_CM + ragged or s > s_neck + 1.2 * S_PER_CM - ragged:
                # Ground steel: streaks along the bar, brighter on the corners and towards the very ends, rust where the
                # paint gave out.
                far = min(s - s_tip, s_end - s) / S_PER_CM  # cm from the nearest end
                lit = 0.42 + 0.3 * value_noise(s * 0.25, t, 1.5, 3) + 0.05 * (d - 0.5) + max(0.0, 2.0 - far) * 0.08
                if edge < 0.8:
                    lit += 0.12
                col = pick(STEEL, min(1.0, lit))
                border = min(abs(s - (s_tip + 4.5 * S_PER_CM + ragged)), abs(s - (s_neck + 1.2 * S_PER_CM - ragged)))
                if border < 3.0 and value_noise(s, t, 2.0, 21) > 0.55:
                    col = pick(RUST, 0.25 + 0.4 * value_noise(s, t, 1.0, 22))
                px[t * SKIN_W + s] = col
                continue
            if s_tape[0] <= s <= s_tape[1]:
                # Black cloth tape, wound on a slant: each turn's edge a lighter line, scuffs where the hands go.
                phase = (s - s_tape[0] + (tt - T_BAR[0]) * 0.55) % 9.0
                lit = 0.35 + 0.25 * d
                if phase < 1.0:
                    lit = 0.9
                elif phase < 2.0:
                    lit = 0.05
                if value_noise(s, t, 3.0, 5) > 0.8:
                    lit += 0.35
                px[t * SKIN_W + s] = pick(TAPE_RAMP, min(1.0, lit))
                continue
            # Paint: worn off along the edges and in chips, scratched along the bar, darker near the ends.
            wear = value_noise(s, t, 3.0, 13) * 0.55 + value_noise(s, t, 9.0, 17) * 0.45
            wear += max(0.0, 1.6 - edge) * 0.22
            if wear > 0.8:
                px[t * SKIN_W + s] = pick(STEEL, 0.3 + 0.3 * value_noise(s * 0.3, t, 1.5, 27))
                continue
            if wear > 0.74:
                px[t * SKIN_W + s] = pick(PAINT_EDGE, d)
                continue
            lit = 0.45 + 0.3 * (value_noise(s, t, 6.0, 19) - 0.5) + 0.06 * (d - 0.5)
            if edge < 0.8:
                lit += 0.12  # the corners catch the light
            if hash2(s // 23, t // 3, 29) > 0.93 and (s + t) % 2 == 0:
                lit = 0.1  # a scratch
            px[t * SKIN_W + s] = pick(PAINT, max(0.0, min(1.0, lit)))
    return bytes(px)


def outward_check(mesh):
    """Triangles facing into the bar: each side face's normal must point away from the centre line near it."""
    return mesh.check_winding()


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    out_dir = args[0] if args else os.path.join(HERE, "..", "..", "quakevr", "progs")
    path = os.path.join(out_dir, "v_crowbar.mdl")
    guard = genguard.Guard("make_crowbar.py", [path])
    mesh, st = build()
    local = [v[0] for v in mesh.verts]
    tip_st = next(s for s in st if s.t != (0.0, 1.0) and abs(math.degrees(math.atan2(s.t[0], s.t[1])) - tip_angle()) < 1e-6)
    tp = tip_st.point(4)
    crown = min(range(len(local)), key=lambda i: math.dist(local[i], tp))
    place = to_model(mesh)
    bad = outward_check(mesh)
    if bad:
        raise SystemExit("%d triangles wound the wrong way" % bad)
    skin = paint(st)
    mdlgen.write_mdl(path, mesh, [skin], "crowbar", frames=[mesh] * (FRAMES - 1))
    # The anchors, in the engine's strip order (vr_anchor.cpp; improve_weapons.strip_order), and the hotspots' places.
    from improve_weapons import strip_order
    order = strip_order([(1,) + t for t in mesh.tris])
    cp = mesh.verts[crown][0]
    best = min(range(len(order)), key=lambda i: math.dist(mesh.verts[order[i]][0], cp))
    print("v_crowbar.mdl: %d vertices, %d triangles, %d frames -> %s" % (len(mesh.verts), len(mesh.tris), FRAMES,
                                                                         os.path.normpath(path)))
    print("  length %.1f cm; the hook's back on the bar's line (muzzle anchor) at %.2f cm up (u %.3f): strip index %d at "
          "%.2f %.2f %.2f" % (max(p[2] for p in local) - min(p[2] for p in local), tp[2], tp[0], best, *cp))
    for name, w in (("main fist", 22.0), ("lower grip (hotspot 1)", 11.5)):
        print("  %s at %.2f %.2f %.2f (model units)" % ((name,) + place((0.0, 0.0, w))))
    guard.finish()


if __name__ == "__main__":
    main()
