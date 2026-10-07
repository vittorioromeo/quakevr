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
#
# What is added, in the guns' own ramps (never a fullbright index: the sights and screens keep theirs):
# - bands: a low-poly ring (chamfered edges) round a barrel, a tube or a housing, on the outline of what it goes
#   round (the barrels' muzzle crowns, clamps, the launchers' tube joints, the axe's neck);
# - bolt heads: hexagonal, a lit chamfer, where panels would be fastened (receivers, bodies, hinge pins);
# - ribs along the top, seen whenever the gun is aimed: the shotgun's ventilated rib, the double's sighting rib;
# - the skins' edge wear (mdlpolish.edge_wear): along the id models' box corners and bevels, the first texel row in
#   lighter within its own colours, some texels of the next scuffed.
# Every part is carried by the old piece it sits on through every frame (recoil, the pump, spinning barrels).

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


# The shotgun's loading port (docs/vr-port/RELOAD_PLAN.md: immersive reloading, shells pushed in from below): an opening
# under the receiver ahead of the trigger guard and behind the pump, as a pump gun's. Parts can only be added, never cut
# (the old triangles stay): the opening is a black floor laid just under the receiver's keel (following it, a grid of
# rays), walled in by a worn steel frame standing out of the receiver round it, deep enough to read as a hole from below; a
# brass shell lifter shows at its back. vr_view.cpp's loadPorts table has its middle (PORT below) for the reload.
PORT_X0, PORT_X1, PORT_HW = 11.0, 16.2, 0.78  # along the gun, and half its width
PORT_FRAME = 0.26                              # the frame's bars: width,
PORT_DEPTH = 0.42                              # and how far they stand out of the keel


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
    # The frame: four bars round it on the keel, standing PORT_DEPTH out (its inner faces the hole's walls).
    h = PORT_DEPTH
    m = PORT_FRAME / 2
    for y in (-PORT_HW - m, PORT_HW + m):
        a, b = under(PORT_X0 - 2 * m, y), under(PORT_X1 + 2 * m, y)
        p.bar(a - Z * (h / 2 - 0.1), b - Z * (h / 2 - 0.1), -Z, PORT_FRAME, h, "steel", key=key, bevel=0.04,
              levels=(0.25, 0.36))
    for x in (PORT_X0 - m, PORT_X1 + m):
        a, b = under(x, -PORT_HW), under(x, PORT_HW)
        p.bar(a - Z * (h / 2 - 0.1), b - Z * (h / 2 - 0.1), -Z, PORT_FRAME, h, "steel", key=key, bevel=0.04,
              levels=(0.25, 0.36))
    # The shell lifter's lip at the back of the opening: a brass plate across it, half as deep as the frame.
    a, b = under(PORT_X0 + 0.45, -PORT_HW + 0.08), under(PORT_X0 + 0.45, PORT_HW - 0.08)
    p.bar(a - Z * 0.12, b - Z * 0.12, -Z, 0.5, 0.2, "bronze", key=key, bevel=0.03)


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
    if name in POST:
        POST[name](os.path.join(out_dir, name))
    return p.m.old_nt, tris, verts, rows, texels


def main():
    args = sys.argv[1:]
    out_dir = args[0] if args and not args[0].endswith(".mdl") else os.path.join(HERE, "..", "..", "quakevr", "progs")
    names = [a for a in args if a.endswith(".mdl")] or sorted(list(RECIPES) + WEAR_ONLY)
    anchors = slot_anchors()
    # The files edited in Blender since a generator wrote them are not overwritten (genguard.py: --keep-edited, --force).
    guard = genguard.Guard("polish_weapons.py", [os.path.join(out_dir, n) for n in names])
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
