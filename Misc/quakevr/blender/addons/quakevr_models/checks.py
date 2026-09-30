# checks.py -- what the engine takes from Quake VR's other models (not weapons: vr_weapons.inc's anchors are in mdl.py,
# as is the wrist gadget's screen), and the report the export prints about it. Plain Python (no bpy, no numpy).
#
# The flashlight (vrflashlight.mdl) is read by the engine as it loads it (vr_flashlight.cpp, Shape): its lens (where
# the beam, the lens's glow and the light start, and their size), its tail (the belt clip, the cord), its switch (the
# clicks) and its outline (how close it goes under or beside a gun). flashlight_shape() below finds them exactly as
# the engine does; keep the two the same. Everything else the engine takes from the models it takes from points and
# sizes written in its code: those are checked here and the export says what to change if they moved.

import math

UNIT_M = 0.0381  # metres per Quake unit at vr_world_scale 1
FULLBRIGHT = 224  # palette indices from here on are fullbright


def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _len(a):
    return math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])


def places(model, pose=0):
    """The vertices' places in model space in a pose (the first by default)."""
    vb = model.pose_verts(model.poses()[pose])
    return [model.place(vb[4 * i:4 * i + 3]) for i in range(model.num_verts)]


def corner_texel(model, tri, k):
    """The skin coordinate (s, t) of a triangle's corner k: a back-facing triangle reads an onseam vertex half a skin
    further right."""
    ff = tri[0]
    on, s, t = model.st[tri[1 + k]]
    if on and not ff:
        s += model.skin_size[0] // 2
    return s, t


def skin_image(model, k):
    """Skin k's (first) image."""
    return model.skins[k][2][0]


def samples(a, b, c, step):
    """Points over a triangle no further than `step` apart (its corners included), as the engine samples surfaces."""
    span = max(math.dist(a, b), math.dist(b, c), math.dist(c, a))
    n = max(1, min(64, int(math.ceil(span / step))))
    for u in range(n + 1):
        for v in range(n + 1 - u):
            fu, fv = u / n, v / n
            yield (a[0] + (b[0] - a[0]) * fu + (c[0] - a[0]) * fv, a[1] + (b[1] - a[1]) * fu + (c[1] - a[1]) * fv,
                   a[2] + (b[2] - a[2]) * fu + (c[2] - a[2]) * fv)


# ----------------------------------------------------------------------------
# The flashlight

FLASHLIGHT = "vrflashlight.mdl"
# vr_flashlight.cpp's defaults (used when the model has no lens: see flashlight_shape), as make_flashlight.py makes it.
FL_DEFAULT_LENS = (1.982, 0.0, 0.0)
FL_DEFAULT_LENS_R = 0.4147
FL_DEFAULT_TAIL = (-1.365, 0.0, 0.0)
FL_DEFAULT_SWITCH = (0.840, 0.0, 0.373)
FL_STEP = 0.03  # units between surface samples
FL_BIN = 0.05  # units along the axis per bin of the outline
FL_BUMP = 0.03  # units the switch must stand out of the outline on the other side to be found
FL_GRIP_R = 0.0138 / UNIT_M  # the grip rings' radius the hands were tuned on (make_flashlight.py R_RING)


def flashlight_shape(model):
    """What vr_flashlight.cpp's Shape reads from the model (model units, the first frame):
      lens, lens_r      the lens: the triangles lit in skin 1 (the "on" skin: their middle's texel fullbright there and
                        not the same in skin 0); its centre (their area-weighted middle, seen along the axis) and radius
                        (its furthest corner from that centre, across the axis)
      axis              the torch's axis: the model's x axis (the origin is on it, in the middle of the grip)
      tail              the rearmost point (min x), on the axis
      outline           per bin of FL_BIN from x0: the furthest the torch reaches from the axis on the half away from
                        the switch (z <= axis: the gun's side when it is clipped on one)
      switch            the switch: the middle of what stands up out of the round torch by more than FL_BUMP
                        (None: nothing does; see below)
    and for the report: lens_normal (the lens's facing), lens_tris, bumps {side: how far it stands out}."""
    P = places(model)
    out = {"lens": None, "lens_r": None, "lens_tris": 0, "lens_normal": None}
    if len(model.skins) >= 2:
        w, h = model.skin_size
        off, on = skin_image(model, 0), skin_image(model, 1)
        wsum, c, nsum, lit = 0.0, [0.0, 0.0, 0.0], [0.0, 0.0, 0.0], []
        for tri in model.tris:
            st = [corner_texel(model, tri, k) for k in range(3)]
            s = min(max(int(sum(q[0] for q in st) / 3.0), 0), w - 1)
            t = min(max(int(sum(q[1] for q in st) / 3.0), 0), h - 1)
            i = t * w + s
            if on[i] < FULLBRIGHT or on[i] == off[i]:
                continue
            a, b, cc = (P[v] for v in tri[1:])
            n = _cross(_sub(b, a), _sub(cc, a))  # (twice the area)
            if n[0] < 0:
                n = (-n[0], -n[1], -n[2])
            wt = n[0]  # the area seen along the axis
            mid = ((a[0] + b[0] + cc[0]) / 3, (a[1] + b[1] + cc[1]) / 3, (a[2] + b[2] + cc[2]) / 3)
            for k in range(3):
                c[k] += mid[k] * wt
                nsum[k] += n[k]
            wsum += wt
            lit.append(tri)
        if lit and wsum > 1e-9:
            centre = (c[0] / wsum, c[1] / wsum, c[2] / wsum)
            out["lens"] = centre
            out["lens_r"] = max(math.hypot(P[v][1] - centre[1], P[v][2] - centre[2]) for tri in lit for v in tri[1:])
            ln = _len(nsum)
            out["lens_normal"] = tuple(x / ln for x in nsum) if ln > 0 else (1.0, 0.0, 0.0)
            out["lens_tris"] = len(lit)
    lens = out["lens"] or FL_DEFAULT_LENS
    cy, cz = 0.0, 0.0
    out["axis"] = (cy, cz)
    x0 = min(p[0] for p in P)
    x1 = max(p[0] for p in P)
    out["tail"] = (x0, cy, cz)
    nb = int((x1 - x0) / FL_BIN) + 1
    out["x0"] = x0
    outline = [0.0] * nb
    lo = [[1e9, 1e9] for _ in range(nb)]  # per bin, the least y and z
    hi = [[-1e9, -1e9] for _ in range(nb)]  # the greatest
    pts = []

    def bin_of(x):
        return min(max(int((x - x0) / FL_BIN), 0), nb - 1)

    for tri in model.tris:
        a, b, c = (P[v] for v in tri[1:])
        for q in samples(a, b, c, FL_STEP):
            pts.append(q)
            k = bin_of(q[0])
            dy, dz = q[1] - cy, q[2] - cz
            if dz <= 0.02:
                outline[k] = max(outline[k], math.hypot(dy, dz))
            for j, d in ((0, dy), (1, dz)):
                lo[k][j] = min(lo[k][j], d)
                hi[k][j] = max(hi[k][j], d)
    out["outline"] = outline
    # What stands out of a round torch: a cut across it (a bin) taller than it is wide stands out up or down (on the
    # side further from the axis), wider than tall to a side, by the difference. A round torch's cuts are as tall as
    # they are wide, wherever its axis is (a head moved off the axis).
    bumps = {"+z": 0.0, "-z": 0.0, "+y": 0.0, "-y": 0.0}
    for k in range(nb):
        if hi[k][0] < lo[k][0]:
            continue
        wide, tall = hi[k][0] - lo[k][0], hi[k][1] - lo[k][1]
        if tall > wide:
            d = "+z" if hi[k][1] > -lo[k][1] else "-z"
            bumps[d] = max(bumps[d], tall - wide)
        else:
            d = "+y" if hi[k][0] > -lo[k][0] else "-y"
            bumps[d] = max(bumps[d], wide - tall)
    out["bumps"] = bumps
    # The switch: the middle of what stands on the +z side of the cuts standing out up by more than FL_BUMP, above
    # the round part (as high over the cut's bottom as it is wide).
    out["switch"] = None
    sw = []
    for q in pts:
        k = bin_of(q[0])
        wide, tall = hi[k][0] - lo[k][0], hi[k][1] - lo[k][1]
        if tall - wide > FL_BUMP and hi[k][1] > -lo[k][1] and q[2] - cz > lo[k][1] + wide + 0.01:
            sw.append(q)
    if sw:
        out["switch"] = tuple(sum(q[k] for q in sw) / len(sw) for k in range(3))
    return out


def outline_at(shape, x):
    """The outline's radius round x (the bin and its neighbours, as the engine looks it up); 0 beyond the ends."""
    o = shape["outline"]
    k = int(math.floor((x - shape["x0"]) / FL_BIN))
    vals = [o[j] for j in (k - 1, k, k + 1) if 0 <= j < len(o)]
    return max(vals) if vals else 0.0


def _v(p):
    return "(%.3f, %.3f, %.3f)" % tuple(p)


def flashlight_lines(shape):
    """The shape as the report's lines (units, and cm at the default size)."""
    lines = []
    if shape["lens"] is None:
        lines.append("CHECK    no lens: no triangle is painted fullbright in skin 1 (the \"on\" skin) where skin 0 differs: the "
                     "engine can't find the lens and uses its defaults (lens %s, radius %.3f): the beam and the glow "
                     "won't follow your lens. Map the lens's faces onto the skin's lens (the square painted "
                     "fullbright in skin 1), or paint where they are fullbright in skin 1" % (
                         _v(FL_DEFAULT_LENS), FL_DEFAULT_LENS_R))
    else:
        lines.append("lens     centre %s, radius %.3f (%.2f cm), %d triangles: the beam, its glow and the light start "
                     "there, this wide" % (_v(shape["lens"]), shape["lens_r"], shape["lens_r"] * UNIT_M * 100,
                                           shape["lens_tris"]))
    lines.append("tail     %s: clipped to the belt there, the cord goes in there; %.2f cm from the grip (the origin)" % (
        _v(shape["tail"]), -shape["tail"][0] * UNIT_M * 100))
    if shape["switch"] is None:
        lines.append("switch   not found (nothing stands out on +z): its clicks come from %s" % _v(FL_DEFAULT_SWITCH))
    else:
        lines.append("switch   %s (stands %.2f cm out): its clicks come from there" % (
            _v(shape["switch"]), shape["bumps"]["+z"] * UNIT_M * 100))
    return lines


def flashlight_report(old, new):
    """(lines, problems): the flashlight's shape as the engine will read it, before and after, and whatever it can't
    follow (the conventions the poses are written in: the beam along +x, the switch on +z, the grip at the origin)."""
    a, b = flashlight_shape(old), flashlight_shape(new)
    lines, problems = [], []
    lines += flashlight_lines(b)
    changed = []
    if (a["lens"] is None) != (b["lens"] is None) or (a["lens"] and max(
            abs(x - y) for x, y in zip(a["lens"] + (a["lens_r"],), b["lens"] + (b["lens_r"],))) > 1e-4):
        changed.append("the lens (was %s, radius %.3f)" % (_v(a["lens"] or FL_DEFAULT_LENS),
                                                            a["lens_r"] or FL_DEFAULT_LENS_R))
    if abs(a["tail"][0] - b["tail"][0]) > 1e-4:
        changed.append("the tail (was x %.3f)" % a["tail"][0])
    if changed:
        lines.append("changed  %s" % "; ".join(changed))
    if b["lens"] is None:
        problems.append(("no lens", 1.0))
    else:
        n = b["lens_normal"]
        tilt = math.degrees(math.acos(max(-1.0, min(1.0, n[0]))))
        if tilt > 10:
            problems.append(("the lens tilted", tilt))
            lines.append("CHECK    the lens faces %.0f degrees off +x: the beam and its glow go along +x (the torch's "
                         "axis), whatever way the lens faces. Keep the lens facing +x" % tilt)
    # The switch: the poses turn +z to the knuckles (in the hand), out from the body (on the belt, a gun, the head).
    side, bump = max(b["bumps"].items(), key=lambda kv: kv[1])
    if bump > FL_BUMP and side != "+z":
        problems.append(("the switch not on +z", bump))
        lines.append("CHECK    the part standing out most is on %s (%.2f cm), not +z: the engine turns +z to the "
                     "knuckles in the fist and out from the body on the belt, a gun and the head. Keep the switch on "
                     "+z (rotate the model about x)" % (side, bump * UNIT_M * 100))
    # The grip: the fist holds the torch round the model's origin, its axis along the fist's.
    x0, x1 = b["x0"], b["x0"] + len(b["outline"]) * FL_BIN
    r0 = outline_at(b, 0.0)
    off_axis = math.hypot(*b["axis"])
    if not (x0 < 0.0 < x1) or off_axis > r0:
        problems.append(("the grip off the torch", off_axis))
        lines.append("CHECK    the origin (0, 0, 0), where the fist holds it, isn't inside the torch (the axis passes "
                     "%.2f from it, the torch is %.2f thick there): move the torch so that the origin is in the middle "
                     "of the grip, on its axis" % (off_axis, r0))
    else:
        was = outline_at(a, 0.0)
        lines.append("grip     round the origin: %.2f cm across (was %.2f): %s" % (
            2 * r0 * UNIT_M * 100, 2 * was * UNIT_M * 100,
            "the fingers wrap what is there (vr_flashlight_low_fingers / _high_fingers 0, the default); with them "
            "set by hand (1) retune their curls" if abs(r0 - was) > 0.001 / UNIT_M else "unchanged"))
    if a["tail"][0] != b["tail"][0] and b["tail"][0] > -0.02 / UNIT_M:
        problems.append(("the tail in the fist", -b["tail"][0]))
        lines.append("CHECK    the tail is %.1f cm behind the grip: the fist covers it" % (-b["tail"][0] * UNIT_M * 100))
    return lines, problems


# ----------------------------------------------------------------------------
# The others: points and sizes written in the engine's code, measured on the model before and after.


def _box(P):
    lo = tuple(min(q[k] for q in P) for k in range(3))
    hi = tuple(max(q[k] for q in P) for k in range(3))
    return lo, hi, tuple((lo[k] + hi[k]) / 2 for k in range(3))


def _geometry_changed(old, new):
    return old.num_verts != new.num_verts or old.tris != new.tris or old.pose_bytes() != new.pose_bytes()


def holster_report(old, new):
    """legholster.mdl (vr_view.cpp holsterOnBody): the weapon hangs at the origin, in the loops; the plate's back rests
    on the body, plateBack (2.95) from the origin along +y; +x forward, +z up (the belt loop), +y towards the body."""
    def back(P):  # the plate's back across its middle
        return max(q[1] for q in P if abs(q[0]) < 0.6)
    a, b = places(old), places(new)
    lines, problems = [], []
    ba, bb = back(a), back(b)
    lines.append("plate    its back at y %.2f across its middle (was %.2f): vr_view.cpp holsterOnBody rests it on the "
                 "body by plateBack 2.95" % (bb, ba))
    if abs(bb - ba) > 0.02:
        problems.append(("the plate's back", bb - ba))
        lines.append("CHECK    the back moved %+.2f units: in vr_view.cpp holsterOnBody set plateBack to %.2f (or it "
                     "floats off the body or sinks into it by that)" % (bb - ba, 2.95 + bb - ba))
    ca, cb = _box(a)[2], _box(b)[2]
    d = math.dist(ca, cb)
    lines.append("loops    the weapon's grip hangs at the origin (0, 0, 0): keep the loops round it; the holster's "
                 "middle %s (was %s)" % (_v(cb), _v(ca)))
    if d > 0.1:
        problems.append(("the holster moved off the weapon", d))
        lines.append("CHECK    the holster moved %.2f units from where the weapon hangs (the origin): the weapon stays "
                     "at the origin; move the holster back, or the weapon with vr_leg_holster_model_x/_y/_z" % d)
    return lines, problems


def pauldron_report(old, new):
    """vrpauldron.mdl, vrpauldron_arm.mdl (vr_view.cpp setupPauldrons): the origin on the LEFT shoulder joint of the
    body's bind pose (the right one is drawn mirrored), +x forward, +y the body's left, +z up; skins 0 leather, 1-3 the
    armours' colours, 4 steel (vr_body_pauldron_style)."""
    lines, problems = [], []
    if len(new.skins) < 5:
        problems.append(("the pauldron's skins", len(new.skins)))
        lines.append("CHECK    %d skins: vr_body_pauldron_style draws skin 0 (leather), 1-3 (the armour worn) and 4 "
                     "(steel): keep 5" % len(new.skins))
    else:
        lines.append("skins    %d: 0 leather, 1-3 the armours' colours (vr_body_pauldron_style 1), 4 steel (2)" %
                     len(new.skins))
    ca, cb = _box(places(old))[2], _box(places(new))[2]
    lines.append("origin   the left shoulder joint (the engine puts it there and turns the pad about it, %s); the pad's "
                 "middle %s (was %s)" % ("with the clavicle" if old.name.lower() == "vrpauldron.mdl" else
                                         "with the upper arm", _v(cb), _v(ca)))
    return lines, problems


def shell_report(old, new):
    """vr_shell.mdl (vr_shells.cpp): its axis along +x, the origin in its middle (it tumbles about it), the rim's
    radius shellRadius (0.0112 m: how high a lying shell's middle is)."""
    lines, problems = [], []
    P = places(new)
    lo, hi, c = _box(P)
    r = max(math.hypot(q[1], q[2]) for q in P)
    ra = max(math.hypot(q[1], q[2]) for q in places(old))
    lines.append("rim      radius %.3f units (%.2f cm; was %.3f): vr_shells.cpp shellRadius 1.12 cm is how high a "
                 "lying shell's middle is" % (r, r * UNIT_M * 100, ra))
    if abs(r - ra) > 0.005:
        problems.append(("the shell's radius", r - ra))
        lines.append("CHECK    in vr_shells.cpp set shellRadius to %.4ff * modelScale (a lying shell floats or sinks "
                     "by the difference)" % (r * UNIT_M))
    if abs(c[0]) > 0.05 or math.hypot(c[1], c[2]) > 0.05:
        problems.append(("the shell off its middle", math.dist(c, (0, 0, 0))))
        lines.append("CHECK    its middle is at %s: it tumbles about the origin; centre it there" % _v(c))
    if hi[0] - lo[0] < max(hi[1] - lo[1], hi[2] - lo[2]):
        problems.append(("the shell's axis", 1.0))
        lines.append("CHECK    it is longer across than along x: its axis runs along +x (the open end forward)")
    return lines, problems


def button_report(old, new):
    """wpnbutton.mdl (vr_view.cpp): drawn at the weapon's button anchor; a fingertip within 2.7 units of its origin
    presses it: the origin is the button's middle."""
    lines, problems = [], []
    P = places(new)
    c = _box(P)[2]
    reach = max(math.dist(q, (0, 0, 0)) for q in P)
    lines.append("button   its middle %s, it reaches %.2f from the origin; a fingertip within 2.7 of the origin "
                 "presses it (vr_view.cpp)" % (_v(c), reach))
    if math.dist(c, (0, 0, 0)) > 0.3:
        problems.append(("the button off its origin", math.dist(c, (0, 0, 0))))
        lines.append("CHECK    it is off the origin: the press is tested round the origin; centre it there")
    if reach > 2.7:
        lines.append("note     bigger than the press's reach: its edges don't press it")
    return lines, problems


HAND_PARTS = ("hand_base.mdl", "finger_index.mdl", "finger_middle.mdl", "finger_ring.mdl", "finger_pinky.mdl",
              "finger_thumb.mdl")


def hand_part_report(old, new):
    """hand_base.mdl, finger_*.mdl: the hands drawn without the rig (vr_hand_rig 0), and what make_hand_rig.py builds
    the rigged hand from. Frames 0-5: the curls; skins 0-3: the blood (make_bloody_hands.py makes 1-3 from 0)."""
    lines, problems = [], []
    if len(new.poses()) != len(old.poses()):
        problems.append(("the hand's frames", len(new.poses())))
        lines.append("CHECK    %d frames (was %d): the fingers curl through frames 0-5" % (len(new.poses()),
                                                                                         len(old.poses())))
    skin = skin_image(old, 0) != skin_image(new, 0)
    if _geometry_changed(old, new) or skin:
        problems.append(("the hand: regenerate", 1.0))
        lines.append("CHECK    changed: drawn as it is only with vr_hand_rig 0. The rigged hand (hand_rig) is built from "
                     "these: run Misc/quakevr/make_hand_rig.py (it rewrites hand_rig.* and Quake/vr/vr_handrig_data.inc)"
                     "%s. vr_view.cpp's wrist (-6.86, -1.08, 1.42) and vr_held.cpp's fistSurface are measured on them" % (
                         "; skin 0 changed: run make_bloody_hands.py for the blood skins 1-3" if skin else ""))
    else:
        lines.append("geometry and skin 0 unchanged")
    return lines, problems


# vr_monstermods.cpp knownSwords: Quake VR's knights are told by their vertex and triangle counts, their swords'
# vertices listed (make_swords.py QVR_KNIGHT, QVR_HKNIGHT: the same lists).
KNIGHTS = {"knight.mdl": (655, 697, (334, 335, 336, 363, 364, 365, 524, 535, 536, 537, 538, 539, 540, 557, 558, 559,
                                     560, 561, 562, 563, 564, 565, 566, 582, 583, 584, 585, 586, 587, 588, 589, 590, 591,
                                     592, 593, 594, 595, 596, 597, 598, 599, 600, 601, 602, 603, 604, 607, 608, 609, 612,
                                     613, 614, 615, 617, 626, 627, 628, 639, 640, 645, 646)),
           "hknight.mdl": (538, 1000, (43, 45, 46, 47, 48, 526, 527, 531, 532)),
           # The ogre's chainsaw (the same knownSwords table; make_chainsaw.py cuts v_chainsaw.mdl from it).
           "ogre.mdl": (497, 1290, tuple(range(416, 497))),
           # The grunt's shotgun and the enforcer's laser rifle (the same table; make_enemyguns.py cuts v_gruntgun.mdl
           # and v_enfrifle.mdl from them).
           "soldier.mdl": (555, 810, tuple(range(463, 549))),
           "enforcer.mdl": (479, 984, (22, 23, 100) + tuple(range(400, 431)) + tuple(range(455, 479)))}

# What each of those monsters drops, and the generator that cuts it out.
DROPPED = {"knight.mdl": ("sword", "v_ksword.mdl", "make_swords.py"),
           "hknight.mdl": ("sword", "v_hksword.mdl", "make_swords.py"),
           "ogre.mdl": ("chainsaw", "v_chainsaw.mdl", "make_chainsaw.py"),
           "soldier.mdl": ("shotgun", "v_gruntgun.mdl", "make_enemyguns.py"),
           "enforcer.mdl": ("laser rifle", "v_enfrifle.mdl", "make_enemyguns.py")}


def knight_report(old, new):
    """knight.mdl, hknight.mdl (vr_monstermods.cpp knownSwords): the sword hidden as the knight dies (it drops
    v_ksword.mdl / v_hksword.mdl, which make_swords.py cuts from these)."""
    nv, nt, verts = KNIGHTS[old.name.lower()]
    what, dropped, generator = DROPPED[old.name.lower()]
    lines, problems = [], []
    if (new.num_verts, len(new.tris)) != (nv, nt):
        problems.append(("the %s's counts" % old.name.lower()[:-4], 1.0))
        lines.append("CHECK    %d vertices, %d triangles (vr_monstermods.cpp knownSwords expects %d, %d): the %s is "
                     "then not hidden as the monster dies (a knight's is found as in id's model, which may hide the "
                     "wrong triangles): set knownSwords' counts (and its vertex list, with %s's) to the new model" % (
                         new.num_verts, len(new.tris), nv, nt, what, generator))
    pa, pb = old.pose_bytes(), new.pose_bytes()
    moved = any(p[v] != q[v] for p, q in zip(pa, pb) for v in verts if v < new.num_verts)
    if moved:
        problems.append(("the %s" % what, 1.0))
        lines.append("CHECK    the %s moved: the one dropped (%s) is cut from this model by %s: run it again" % (
            what, dropped, generator))
    lines.append("%s    its %d vertices (knownSwords): %s" % (what, len(verts), "moved" if moved else "unchanged"))
    return lines, problems


CHECKS = {FLASHLIGHT: ("the flashlight: read by the engine as it loads it (vr_flashlight.cpp)", flashlight_report),
          "legholster.mdl": ("the holster (vr_view.cpp holsterOnBody)", holster_report),
          "vrpauldron.mdl": ("the pauldron's cap (vr_view.cpp setupPauldrons)", pauldron_report),
          "vrpauldron_arm.mdl": ("the pauldron's lames (vr_view.cpp setupPauldrons)", pauldron_report),
          "vr_shell.mdl": ("the spent shell (vr_shells.cpp)", shell_report),
          "wpnbutton.mdl": ("the ammo button (vr_view.cpp)", button_report),
          "knight.mdl": ("the knight's sword (vr_monstermods.cpp, make_swords.py)", knight_report),
          "hknight.mdl": ("the hell knight's sword (vr_monstermods.cpp, make_swords.py)", knight_report),
          "ogre.mdl": ("the ogre's chainsaw (vr_monstermods.cpp, make_chainsaw.py)", knight_report),
          "soldier.mdl": ("the grunt's shotgun (vr_monstermods.cpp, make_enemyguns.py)", knight_report),
          "enforcer.mdl": ("the enforcer's laser rifle (vr_monstermods.cpp, make_enemyguns.py)", knight_report)}
CHECKS.update({n: ("the hands without the rig (vr_view.cpp drawHand; make_hand_rig.py's source)", hand_part_report)
               for n in HAND_PARTS})


def report(old, new):
    """(title, lines, problems) for a model the engine takes points or sizes from, or None. A line that starts with
    CHECK says what to change; problems: [(what, amount)]."""
    c = CHECKS.get(old.name.lower())
    if c is None:
        return None
    lines, problems = c[1](old, new)
    return c[0], lines, problems
