#!/usr/bin/env python3
# polish_weapons.py -- round 21's pass over the view models ("improve their look while keeping the same art style
# and low-poly look"): small low-poly details on the crudest spots and edge wear painted into the skins, without
# moving anything the author tuned.
#
# Usage: python Misc/quakevr/polish_weapons.py [output progs folder] [model ...]   (default: quakevr/progs, all)
#
# The inputs are the models as rounds 16-20 left them (improve_weapons*.py, make_swords.py), kept byte for byte in
# Misc/quakevr/src_models/r21/: running it again gives the same files. After rerunning an earlier generator, copy
# its output there and run this again.
#
# What stays (mdlpolish.py): the header's scale and origin (what the weapon offsets and Scale apply about), every
# old vertex's bytes and index, every old triangle, UV and texel but the edge wear's. New vertices and triangles are
# appended, sharing no vertex with the old ones, so vr_anchor.cpp's strip order -- every anchor index (hand, muzzle,
# two-handed grip, ammo screen and button, shell port) -- names the same vertex at the same place (checked here for
# every anchor of every slot using the model), the hotspots (model space) stay where they were, and the new parts
# fit inside the old bounds (the write fails otherwise). The new parts keep clear of where the hands hold the gun
# (the grips, triggers, foregrips, pumps): the fitted fingers close on the same surfaces as before.
# The double shotgun then goes through reuv_shot2.py (POST): its fore-end's stretched UVs re-mapped and repainted,
# its holes closed; UVs and texels there change, the old vertices, triangles and anchors do not.
# The shotgun then goes through split_auto_pump (its auto pump): the first of its fore-end's rings taken out (its own
# vertices collapsed: no anchor, the strip order unchanged), the other three given their own texels, the moving
# fore-end and the gun without it written apart (progs/vr_pump_on_v_shot.mdl, vr_pumpbody_on_v_shot.mdl).
#
# What is added, in the guns' own ramps (never a fullbright index: the sights and screens keep theirs):
# - bands: a low-poly ring (chamfered edges) round a barrel, a tube or a housing, on the outline of what it goes
#   round (the barrels' muzzle crowns, clamps, the launchers' tube joints, the axe's neck);
# - bolt heads: hexagonal, a lit chamfer, where panels would be fastened (receivers, bodies, hinge pins);
# - ribs along the top, seen whenever the gun is aimed: the shotgun's ventilated rib, the double's sighting rib;
# - the skins' edge wear (mdlpolish.edge_wear): along the id models' box corners and bevels, the first texel row in
#   lighter within its own colours, some texels of the next scuffed.
# Every part is carried by the old piece it sits on through every frame (recoil, the pump, spinning barrels).

import math
import os
import re
import sys

import numpy as np

import genguard
import mdlpolish as mp
import refine_laserg
import reuv_shot2
from improve_weapons import strip_order

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "src_models", "r21")
ORIGINAL = os.path.join(HERE, "src_models")  # the models before round 16: the id-made triangles and skin rows
INC = os.path.join(HERE, "..", "..", "Quake", "vr", "vr_weapons.inc")

X, Y, Z = np.array([1.0, 0, 0]), np.array([0, 1.0, 0]), np.array([0, 0, 1.0])

mp.MATERIALS.update({
    "gunmetal": [0, 32, 1, 33, 2, 34, 3, 35, 36],  # the shotguns' and nailguns' blue-black
    "bolt": [1, 2, 3, 4, 5, 6],           # worn steel bolt heads on the brown guns
    "darkbrown": [16, 174, 17, 173, 18, 19, 172, 20, 171],
})


def side_studs(p, xs, z, material, radius=0.5, height=0.28, sides=(1, -1), reach=12.0):
    """Bolt heads on both sides (a ray in from +-y at each x, height z)."""
    for x in xs:
        for s in sides:
            p.stud((x, s * reach, z), (0, -s, 0), radius=radius, height=height, material=material)


# ----------------------------------------------------------------------------
# Recipes (model space: x forward, y left, z up; frame 0)

def shotgun(p):
    # Muzzle crown round the barrel (its piece only: not the front sight's post), and a clamp further back round
    # the barrel and the magazine tube.
    p.band((32.35, 0.02, 4.23), X, Z, 0.8, 0.2, "gunmetal")
    # Pins through the receiver ahead of the trigger group (the fitted hand's thumb stays behind x 8).
    side_studs(p, (13.2, 16.6), 1.7, "gunmetal", radius=0.45)
    # A ventilated rib along the barrel's top, under the line from the receiver's top to the front sight (z 6.21):
    # posts and a flat bar, seen whenever the player looks along the gun.
    key = p.carrier_at((25.0, 0.0, 5.62))
    for x in (20.0, 23.6, 27.2, 30.8):
        p.box((x, 0.0, 5.7), Z, Y, X, (0.16, 0.2, 0.28), "gunmetal", key)
    p.bar((19.4, 0.0, 5.9), (31.9, 0.0, 5.9), Z, 0.5, 0.14, "gunmetal", key=key, bevel=0.05)
    loading_port(p)
    auto_pump(p)


# The shotgun's auto pump (vr_autopump.cpp; ROUND21.md, "Shotgun auto pump"): after each shot the fore-end is driven
# back and forward by the gun itself, on two guide rods along the shoulders of the fore-end, either side of the barrel
# (seen from above and from the side, over the fore-end's top), from an actuator housing on each side of the
# receiver's front to a yoke clamped round the barrel ahead of the fore-end. The fore-end (the id model's ribbed rings,
# the last three: the first, next to the receiver, is taken out to leave room for the stroke and show the rods) carries
# a shoe round each rod at its front and back, a strap along each rod's underside joining them, and an action bar back
# from the rear shoe towards the housing. split_auto_pump (after the polish) writes the moving part apart
# (progs/vr_pump_on_v_shot.mdl) and the gun without it (progs/vr_pumpbody_on_v_shot.mdl), drawn instead of the gun
# while it cycles; v_shot.mdl keeps both, at rest (holstered, lying in the world).
PUMP_RAIL_Y, PUMP_RAIL_Z, PUMP_RAIL_R = 2.2, 4.78, 0.17  # the rods' axes (+-y) and radius
PUMP_RAIL_X0, PUMP_RAIL_X1 = 17.2, 31.6                  # their ends, in the housings and the yoke's lugs
PUMP_RINGS = ((18.9, 21.0), (21.8, 24.1), (24.9, 27.2), (27.9, 30.3))  # the id fore-end's rings, along x
PUMP_TAG = "pump"


def auto_pump(p):
    key = p.carrier_at((32.4, -1.2, 4.6))  # the barrel's and fore-end's piece: everything here recoils with it
    y0, z0, r = PUMP_RAIL_Y, PUMP_RAIL_Z, PUMP_RAIL_R
    for s in (1, -1):
        y = s * y0
        # The guide rod, polished steel.
        mid, half = 0.5 * (PUMP_RAIL_X0 + PUMP_RAIL_X1), 0.5 * (PUMP_RAIL_X1 - PUMP_RAIL_X0)
        p.revolve((mid, y, z0), X, Z, [(-half, 0.0), (-half, r), (half, r), (half, 0.0)], "steel", key, sides=6,
                  shades=[0, 0.25, 0, 0])
        # The actuator housing on the receiver's front: a blued cylinder round the rod's end, its front rim stepped
        # down to a dark seal where the rod comes out.
        p.revolve((17.75, y, z0), X, Z, [(-0.85, 0.0), (-0.85, 0.4), (0.62, 0.4), (0.78, 0.28), (0.9, 0.28),
                                         (0.9, 0.0)], "gunmetal", key, sides=6, shades=[0, 0.05, 0.25, -0.25, 0.1, 0],
                  phase=0.0)
        # The yoke's lug from the clamp out to the rod's front end.
        p.box((31.45, s * 1.86, z0), X, Y, Z, (0.3, 0.5, 0.27), "gunmetal", key, bevel=0.08)
    # The yoke's clamp round the barrel, ahead of the fore-end and behind the muzzle crown.
    p.band((31.45, 0.0, 4.3), X, Z, 0.6, 0.2, "gunmetal", key=key)

    # The moving part: shoes round the rods on the first and last of its rings, a strap under each rod between them,
    # and an action bar back from the rear shoe (towards the housing it runs into at the stroke's end).
    p.tag = PUMP_TAG
    for s in (1, -1):
        y = s * y0
        for x0, x1 in ((22.0, 23.0), (29.1, 30.1)):
            p.box((0.5 * (x0 + x1), y, z0 - 0.04), Z, Y, X, (0.33, 0.3, 0.5), "gunmetal", key, bevel=0.1)
        p.bar((22.4, y, z0 - 0.3), (29.6, y, z0 - 0.3), Z, 0.42, 0.12, "gunmetal", key=key)
        p.bar((20.5, y, z0 - 0.3), (22.2, y, z0 - 0.3), Z, 0.3, 0.16, "steel", key=key)
    p.tag = None


def pump_ring(m, mesh, ti):
    """Which of the id fore-end's rings (PUMP_RINGS) an old triangle of v_shot is part of (0..3), or -1."""
    _, a, b, c = m.tris[ti]
    root = mesh.piece[a]
    vs = np.nonzero(mesh.piece == root)[0]
    if len(vs) > 4:
        return -1  # the rings are faces of 2 triangles each, apart from the rest
    P = mesh.P[vs]
    for k, (x0, x1) in enumerate(PUMP_RINGS):
        if P[:, 0].min() >= x0 and P[:, 0].max() <= x1:
            return k
    return -1


def write_subset(src, tris, path):
    """`src` (an MDL file) with only the triangles `tris` (indices) and the vertices they use: the same header scale
    and origin, skins, frames (each vertex's bytes as they were)."""
    m = mp.Model(src)
    used = sorted({v for t in tris for v in m.tris[t][1:]})
    remap = {v: i for i, v in enumerate(used)}
    m.st = [m.st[v] for v in used]
    m.tris = [(m.tris[t][0],) + tuple(remap[v] for v in m.tris[t][1:]) for t in tris]
    for blk in m.pose_blocks():
        vb = blk[2] if blk[0] == "simple" else blk[1]
        vb[:] = b"".join(bytes(vb[4 * v:4 * v + 4]) for v in used)
    m.old_nv = len(used)
    m.write(path)


def tri_texels(m, tri):
    """The skin texels whose middles a triangle covers (its UVs as the engine maps them: mdlpolish.tri_uv)."""
    uv = mp.tri_uv(m, tri)
    (ax, ay), (bx, by), (cx, cy) = uv
    den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
    out = set()
    if abs(den) < 1e-9:
        return out
    for y in range(int(uv[:, 1].min()), int(math.ceil(uv[:, 1].max())) + 1):
        for x in range(int(uv[:, 0].min()), int(math.ceil(uv[:, 0].max())) + 1):
            px, py = x + 0.5, y + 0.5
            w0 = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / den
            w1 = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / den
            if min(w0, w1, 1 - w0 - w1) >= -0.05:
                out.add((x % m.sw, y))
    return out


def split_auto_pump(p, path):
    """After the polish: the first ring taken out of v_shot.mdl (its vertices collapsed onto one: its triangles
    vanish, the vertex and triangle orders, and so every anchor, stay); the other three given their own copy of their
    texels (the id skin paints the rings on the fore-end's body under them: the body's stripes would stay behind as the
    rings slide) and the body's stripes painted over in the colour between them; the moving pump and the gun without
    it written apart, next to it."""
    m = mp.Model(path)
    mesh = p.mesh
    ring = [pump_ring(m, mesh, t) if t < p.m.old_nt else -1 for t in range(len(m.tris))]
    gone = {v for t, k in enumerate(ring) if k == 0 for v in m.tris[t][1:]}
    first = min(gone)
    for blk in m.pose_blocks():
        vb = blk[2] if blk[0] == "simple" else blk[1]
        for v in gone:
            vb[4 * v:4 * v + 3] = vb[4 * first:4 * first + 3]

    # The texels: the rings', and those of every old triangle but theirs and the fore-end body's (the piece the
    # two-handed grip's anchor is on).
    ring_texels = set()
    other_texels = set()
    body = mesh.piece[strip_order(p.m.tris[:p.m.old_nt])[48]]
    for t in range(p.m.old_nt):
        if ring[t] >= 0:
            ring_texels |= tri_texels(m, m.tris[t])
        elif mesh.piece[m.tris[t][1]] != body:
            other_texels |= tri_texels(m, m.tris[t])
    assert not ring_texels & other_texels, "the rings share texels with more than the fore-end's body"
    # Rings 1-3 each copied (a texel round them, for filtering) into rows added under the skin, side by side.
    rects = []
    for k in (1, 2, 3):
        vs = sorted({v for t, kk in enumerate(ring) if kk == k for v in m.tris[t][1:]})
        s = [m.st[v][1] for v in vs]
        tt = [m.st[v][2] for v in vs]
        rects.append((vs, max(0, min(s) - 1), max(0, min(tt) - 1), min(m.sw, max(s) + 2), max(tt) + 2))
    rows = max(r[4] - r[2] for r in rects)
    assert sum(r[3] - r[1] for r in rects) <= m.sw, "the rings' copies don't fit side by side"
    t_new = m.grow_skin(rows)
    x = 0
    for vs, s0, t0, s1, t1 in rects:
        for _, _, ims in m.skins:
            for im in ims:
                for y in range(t1 - t0):
                    to, fr = (t_new + y) * m.sw + x, (t0 + y) * m.sw + s0
                    im[to:to + (s1 - s0)] = im[fr:fr + (s1 - s0)]
        for v in vs:
            m.st[v][1] += x - s0
            m.st[v][2] += t_new - t0
        x += s1 - s0
    # The body's stripes (the rings' old texels) painted over: each from the nearest texel up or down its column that
    # is the body's own (the dark colour between the rings).
    for _, _, ims in m.skins:
        for im in ims:
            src = bytes(im)
            for (s, t) in ring_texels:
                for d in range(1, 40):
                    hit = [u for u in (t - d, t + d) if 0 <= u < m.old_sh and (s, u) not in ring_texels]
                    if hit:
                        im[t * m.sw + s] = src[hit[0] * m.sw + s]
                        break
    m.write(path)
    old_nt = p.m.old_nt
    tags = [None] * old_nt + list(p.tri_tags)
    assert len(tags) == len(m.tris)
    pump = [t for t in range(len(m.tris)) if ring[t] > 0 or tags[t] == PUMP_TAG]
    body = [t for t in range(len(m.tris)) if ring[t] < 0 and tags[t] != PUMP_TAG]
    out = os.path.dirname(path)
    write_subset(path, pump, os.path.join(out, "vr_pump_on_v_shot.mdl"))
    write_subset(path, body, os.path.join(out, "vr_pumpbody_on_v_shot.mdl"))
    return len(pump), len(body)


SPLIT_OUTPUTS = {"v_shot.mdl": ["vr_pump_on_v_shot.mdl", "vr_pumpbody_on_v_shot.mdl"]}


# The shotgun's loading port (docs/vr-port/RELOAD.md: immersive reloading, shells pushed in from below): an opening
# under the receiver ahead of the trigger guard and behind the pump, as a pump gun's. Parts can only be added, never cut
# (the old triangles stay, and with them the anchors' strip order): the opening is a well under the receiver's keel, a
# steel housing round it whose inner walls, lined from a dull steel at its mouth to black up at the keel, go up to a black
# ceiling laid just under the keel (following it, a grid of rays): from below a deep dark hole (the author: the framed
# plate it was didn't read as one), the shells sliding up into it (vr_collectfx.cpp, its "into the gun" variant); a brass
# shell lifter shows at its back, up inside. vr_view.cpp's loadPorts table has its middle (PORT below) for the reload:
# the load point stays where it was (inside the well, near its mouth).
PORT_X0, PORT_X1, PORT_HW = 11.0, 16.2, 0.78  # along the gun, and half its width
PORT_FRAME = 0.26                              # the housing's walls: thickness,
PORT_DEPTH = 0.9                               # and how far they stand out of the keel (the well's depth)


def loading_port(p):
    key = p.carrier_at((13.6, 0.0, 0.9))

    def under(x, y):
        return p.hit((x, y, -10.0), Z)[0]

    # The floor, a step below the keel all over the opening (5 x 3 quads), facing down.
    xs = np.linspace(PORT_X0, PORT_X1, 6)
    ys = np.linspace(-PORT_HW, PORT_HW, 4)
    grid = [[under(x, y) - Z * 0.1 for y in ys] for x in xs]  # (z is stored in steps of 0.067)
    for i in range(len(xs) - 1):
        for j in range(len(ys) - 1):
            p.face([grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]], -Z, "black", 0.15, key)
    # The housing: a ring of walls round it on the keel, standing PORT_DEPTH out: their outer faces and their rim at the
    # mouth (facing down), mitred at the corners; their inner faces are the lining below (no faces of their own there:
    # two faces in one plane flicker, the author's note vrfiringrange_2026-10-07_22-01-29, as the bars' inner faces
    # did under the lining; the vertices' steps, 0.22 along x, 0.03 across, put both in the very same plane).
    mouth = PORT_DEPTH - 0.1  # (how far under the keel the walls and the lining reach)
    fr = PORT_FRAME
    inner = [(PORT_X0, -PORT_HW), (PORT_X1, -PORT_HW), (PORT_X1, PORT_HW), (PORT_X0, PORT_HW)]
    outer = [(PORT_X0 - fr, -PORT_HW - fr), (PORT_X1 + fr, -PORT_HW - fr), (PORT_X1 + fr, PORT_HW + fr),
             (PORT_X0 - fr, PORT_HW + fr)]
    outward = (-Y, X, Y, -X)  # (each side from corner i to i + 1: its outer face's way)
    for i in range(4):
        j = (i + 1) % 4
        o0, o1 = under(*outer[i]), under(*outer[j])
        i0, i1 = under(*inner[i]), under(*inner[j])
        p.face([o0 + Z * 0.1, o1 + Z * 0.1, o1 - Z * mouth, o0 - Z * mouth], outward[i], "steel", 0.36, key)
        p.face([i0 - Z * mouth, i1 - Z * mouth, o1 - Z * mouth, o0 - Z * mouth], -Z, "steel", 0.25, key)
    # The well's lining: the walls' inner faces, facing into it, in bands from its mouth (a dull steel) up to the keel
    # (black), so that it reads as deep.
    bands = ((0.0, 0.3, "black", 0.2), (0.3, 0.6, "steel", 0.08), (0.6, 1.0, "steel", 0.18))  # (from, to: of the depth)
    in_d = mouth - 0.1  # (from the ceiling down to the mouth)
    sides = (((PORT_X0, -PORT_HW), (PORT_X1, -PORT_HW), Y), ((PORT_X1, PORT_HW), (PORT_X0, PORT_HW), -Y),
             ((PORT_X0, PORT_HW), (PORT_X0, -PORT_HW), X), ((PORT_X1, -PORT_HW), (PORT_X1, PORT_HW), -X))
    for (x0, y0), (x1, y1), inward in sides:
        k0, k1 = under(x0, y0), under(x1, y1)
        for f, t, material, level in bands:
            p.face([k0 - Z * (0.1 + in_d * f), k1 - Z * (0.1 + in_d * f), k1 - Z * (0.1 + in_d * t),
                    k0 - Z * (0.1 + in_d * t)], inward, material, level, key)
    # The shell lifter's lip at the back of the well, up inside it: a brass plate across it.
    a, b = under(PORT_X0 + 0.45, -PORT_HW + 0.08), under(PORT_X0 + 0.45, PORT_HW - 0.08)
    p.bar(a - Z * 0.3, b - Z * 0.3, -Z, 0.5, 0.2, "bronze", key=key, bevel=0.03)


def shotgun2(p):
    # A band round both barrels (over the rib), where a side-by-side's barrels are joined.
    p.band((24.0, 0.0, 7.1), X, Z, 0.7, 0.16, "gunmetal")
    side_studs(p, (12.3,), 4.4, "gunmetal", radius=0.55)  # the hinge pin
    side_studs(p, (7.4, 10.0), 5.6, "gunmetal", radius=0.4)
    # The sighting rib in the valley between the barrels, just under their tops, up to the bead.
    p.bar((12.9, 0.0, 7.25), (26.8, 0.0, 7.25), Z, 0.8, 0.6, "gunmetal", key=p.carrier_at((20.0, 0.0, 7.04)))


def nailgun(p):
    for y in (3.8, -3.8):
        p.band((15.6, y, 4.5), X, Z, 0.8, 0.22, "gunmetal")
        p.band((5.0, y, 4.5), X, Z, 0.8, 0.22, "gunmetal")
    side_studs(p, (4.0, 9.0), -0.1, "gunmetal", radius=0.45)


def nailgun2(p):
    p.band((36.5, 0.0, 0.8), X, Z, 1.0, 0.25, "gunmetal", only=None)
    side_studs(p, (4.0, 13.0), -3.0, "gunmetal", radius=0.5)


def launcher(p):
    # The grenade launcher's tube is as wide as the model's bounds: nothing on its sides. Bolts along the top
    # bevels and a band round the front end's rim.
    for x in (2.5, 9.0, 15.5):
        for s in (1, -1):
            p.stud((x, s * 2.6, 20), (0, 0, -1), radius=0.55, height=0.3, material="bolt")


def rocket(p):
    for x, w in ((3.0, 0.9), (14.0, 0.8), (26.0, 1.0)):
        p.band((x, 0.0, 4.75), X, Z, w, 0.22, "darkbrown")
    side_studs(p, (35.5, 38.0), 7.0, "bolt", radius=0.55)


def lightning(p):
    side_studs(p, (4.0, 11.0, 18.0), 6.2, "bolt", radius=0.5)
    p.band((32.4, 0.0, 9.6), X, Z, 0.8, 0.2, "darkbrown")


def axe(p):
    # The neck, where the head sits on the handle. (The head is as thick as the model's bounds: no bolts on it.)
    p.band((6.6, 0.0, 19.0), np.array([0.33, 0.0, 0.944]), X, 1.0, 0.2, "steel")


RECIPES = {
    "v_shot.mdl": shotgun,
    "v_shot2.mdl": shotgun2,
    "v_nail.mdl": nailgun,
    "v_lava.mdl": nailgun,
    "v_nail2.mdl": nailgun2,
    "v_lava2.mdl": nailgun2,
    "v_rock.mdl": launcher,
    "v_prox.mdl": launcher,
    "v_multi.mdl": launcher,
    "v_rock2.mdl": rocket,
    "v_multi2.mdl": rocket,
    "v_light.mdl": lightning,
    "v_plasma.mdl": lightning,
    "v_axe.mdl": axe,
}
WEAR_ONLY = ["v_grpple.mdl", "v_laserg.mdl", "v_hammer.mdl"]
# After the polish: the double shotgun's fore-end re-mapped (its old UVs were stretched) and its holes closed
# (reuv_shot2.py: the old vertices, triangles and anchors stay; those UVs and texels change); the laser cannon's
# stretched keel, bottom and grip re-mapped and its body's vents carved (refine_laserg.py: Blender, headless).
POST = {"v_shot2.mdl": reuv_shot2.fix, "v_laserg.mdl": refine_laserg.fix}


# ----------------------------------------------------------------------------

def slot_anchors():
    """{model file: set of anchor indices} from vr_weapons.inc's defaults."""
    d = {}
    for s, k, v in re.findall(r'QVR_WEAPON_DEFAULT\((\d+), (\w+), "([^"]*)"\)', open(INC).read()):
        d.setdefault(int(s), {})[k] = v
    out = {}
    for s, keys in d.items():
        m = keys.get("ID", "")
        if not m.startswith("progs/"):
            continue
        a = out.setdefault(m[len("progs/"):], set())
        for k in ("HandAnchorVertex", "MuzzleAnchorVertex", "TwoHHandAnchorVertex", "WpnButtonAnchorVertex",
                  "WpnTextAnchorVertex"):
            if k in keys:
                a.add(int(float(keys[k])))
    return out


def anchors_of(path, anchors):
    m = mp.Model(path)
    order = strip_order(m.tris)
    P = np.frombuffer(bytes(m.pose_bytes(0)), np.uint8).reshape(-1, 4)
    return {a: (order[a], bytes(P[order[a]])) for a in anchors}


def polish(name, out_dir):
    src = os.path.join(SRC, name)
    p = mp.Polisher(src, rows=16)
    wear = {}
    orig = os.path.join(ORIGINAL, name)
    if os.path.exists(orig):
        o = mp.Model(orig)
        wear = dict(tris=range(o.old_nt), max_t=o.old_sh)
    texels = mp.edge_wear(p.m, p.ramps, mesh=p.mesh, **wear)
    if name in RECIPES:
        RECIPES[name](p)
    tris, verts, rows = p.finish(os.path.join(out_dir, name))
    if name == "v_shot.mdl":
        split_auto_pump(p, os.path.join(out_dir, name))
    if name in POST:
        POST[name](os.path.join(out_dir, name))
    return p.m.old_nt, tris, verts, rows, texels


def main():
    args = sys.argv[1:]
    out_dir = args[0] if args and not args[0].endswith(".mdl") else os.path.join(HERE, "..", "..", "quakevr", "progs")
    names = [a for a in args if a.endswith(".mdl")] or sorted(list(RECIPES) + WEAR_ONLY)
    anchors = slot_anchors()
    # The files edited in Blender since a generator wrote them are not overwritten (genguard.py: --keep-edited, --force).
    guard = genguard.Guard("polish_weapons.py", [os.path.join(out_dir, n) for n in names] +
                           [os.path.join(out_dir, x) for n in names for x in SPLIT_OUTPUTS.get(n, [])])
    bad = 0
    for name in names:
        before = anchors_of(os.path.join(SRC, name), anchors.get(name, set()))
        old_nt, tris, verts, rows, texels = polish(name, out_dir)
        after = anchors_of(os.path.join(out_dir, name), anchors.get(name, set()))
        moved = [a for a in before if before[a] != after[a]]
        bad += len(moved)
        print("%-13s %4d -> %4d triangles (+%3d, x%.2f), %3d new vertices, %2d new skin rows, %4d texels worn; "
              "anchors %s%s" % (name, old_nt, old_nt + tris, tris, (old_nt + tris) / old_nt, verts, rows, texels,
                                ", ".join(str(a) for a in sorted(before)) or "none",
                                "" if not moved else "  MOVED: %s" % moved))
    guard.finish()
    if bad:
        sys.exit("anchors moved")


if __name__ == "__main__":
    main()
