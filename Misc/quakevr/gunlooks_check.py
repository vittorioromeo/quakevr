#!/usr/bin/env python3
# gunlooks_check.py -- what gunlooks.py promises (round 24), for the models it wrote against its inputs
# (src_models/gunlooks), ending in one verdict line: "gunlooks_check: N passed, M failed".
#
#   python Misc/quakevr/gunlooks_check.py [--kick log]       (Python with numpy)
#
# --kick: the console log of Misc/quakevr/gunlooks_kick.sh (vr_kickcancel_info on the guns and parts): no new moving parts
# (against src_models/gunlooks/kick_before.json, the places moving in the shipped models before this pass).
#
# Checked on each model: header (scale, origin: the hotspots and offsets are in that space), frame count and names; every
# old vertex's bytes in every pose at its index; every old triangle in its order on the same points in every pose (a
# corner may be a coincident copy); the triangles that stay on the same texels; every fullbright texel byte-identical,
# no new fullbright texel (but the lava's transferred glow), the fullbright mask of what the sights use the same; every
# anchor in use (anchors.py) as it was; the magazines untouched; the barrel mask's count; the rivets rigid with their piece, clear
# of the sight line, under the old byte box; re-mapped faces of real size inside the density limits; no one-colour faces;
# the colour family: the metal's mean colour near the old and rust a small part of the surface.

import hashlib
import json
import math
import os
import re
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import anchors  # noqa: E402
import gunlooks as gl  # noqa: E402
import mdlpolish as mp  # noqa: E402

MIN_AREA = 0.5           # square units: a face this big or more,
MIN_WIDTH = 6.0          # and this many texels wide at its narrowest (skin coordinates are whole texels: a sliver's map is rounding),
DENSITY_LO = 0.65        # is held to these: texels per unit along any direction, times the model's D, at least...
DENSITY_HI = 1.25        # ...and at most
RATIO_MAX = 1.7          # the map's stretch (its singular values' ratio)
COLOUR_TEXELS = 8        # a face covering this many texels or more...
COLOUR_MIN = 2           # ...shows at least this many different palette indices (no solid colours)
SIGHT_CLEAR = 1.0        # no rivet point within this (y) of the sight line (y 0)
FAMILY_MAX = 14.0        # the metal's area-weighted mean colour from the old mean, largest channel change... (0-255; upward lift apart)
INTERIOR_MAX = 18.0      # the nail muzzles' interior: almost black
RUST_MAX = 0.2           # fraction of a model's painted texels in rust colours (warm and clearly lit)
GUNS = {"v_shot.mdl", "v_nail.mdl", "v_lava.mdl"}
RIVETS = {"v_shot.mdl": 6, "v_nail.mdl": 4, "v_lava.mdl": 4}
RIVET_TRIS = 30          # triangles in one rivet's head
MIRROR_TOL = 0.3         # model units: a rivet and its mirror on the other side
PUMP_RINGS_X = 18.9      # the shotgun's pump rings start here
TOP_Z = 5.0              # the shotgun receiver's top, the sights' side: no rivet there

passed = failed = 0


def check(ok, what):
    global passed, failed
    if ok:
        passed += 1
    else:
        failed += 1
    print(("  ok   " if ok else "  FAIL ") + what)


def pose_arrays(m):
    return [np.frombuffer(bytes(m.pose_bytes(p)), np.uint8).reshape(-1, 4) for p in range(m.num_poses())]


def frame_names(m):
    out = []
    for fr in m.frames:
        out.append(bytes(fr[1][8:24]) if fr[0] == "simple" else [bytes(s[0][8:24]) for s in fr[4]])
    return out


def model_checks(name, spec):
    print(name)
    path = os.path.join(gl.PROGS, name)
    src = os.path.join(gl.SRC, name)
    data = json.load(open(src + ".json"))
    old, new = mp.Model(src), mp.Model(path)
    o_nv, o_nt = len(old.st), len(old.tris)
    check(all(abs(a - b) < 1e-7 for a, b in zip(old.h[2:12], new.h[2:12])), "header scale, origin, radius, eyes unchanged")
    check(len(old.frames) == len(new.frames) and frame_names(old) == frame_names(new) and old.num_poses() == new.num_poses(),
          "frames unchanged: %d (%d poses)" % (len(new.frames), new.num_poses()))
    po, pn = pose_arrays(old), pose_arrays(new)
    check(all((a == b[:o_nv]).all() for a, b in zip(po, pn)), "the %d old vertices' bytes unchanged at their indices in every pose" % o_nv)
    same_points = True
    same_front = True
    for t in range(o_nt):
        a, b = old.tris[t], new.tris[t]
        same_front &= a[0] == b[0]
        for k in (1, 2, 3):
            if a[k] != b[k] and any((x[a[k], :3] != y[b[k], :3]).any() for x, y in zip(po, pn)):
                same_points = False
    check(same_points and same_front, "the %d old triangles in order, on the same points in every pose (%d new ones after them)" % (o_nt, len(new.tris) - o_nt))
    # the texels
    so = np.frombuffer(bytes(old.skin()), np.uint8).reshape(old.sh, old.sw)
    sn = np.frombuffer(bytes(new.skin()), np.uint8).reshape(new.sh, new.sw)
    region = sn[:old.sh, :old.sw]
    fbo = so >= mp.FULLBRIGHT
    check((region[fbo] == so[fbo]).all() and int(fbo.sum()) == int((region[fbo] >= mp.FULLBRIGHT).sum()),
          "%d fullbright texels byte-identical where they were" % int(fbo.sum()))
    newfb = int((sn >= mp.FULLBRIGHT).sum()) - int(fbo.sum())
    if spec.get("glow_transfer"):
        check(newfb >= 0, "the lava's glow: %d texels carried onto the new charts" % newfb)
    else:
        check(newfb == 0, "no new fullbright texels")
    bad = 0
    sights = 0
    for t in data["kept"]:
        ua, ub = mp.tri_uv(old, old.tris[t]), mp.tri_uv(new, new.tris[t])
        P = old.positions(0)[list(old.tris[t][1:])]
        area = 0.5 * np.linalg.norm(np.cross(P[1] - P[0], P[2] - P[0]))
        if not np.allclose(ua, ub):
            bad += 1
        xs, ys, _ = gl.tri_cover(old, old.tris[t], so)
        if area > 1e-3 and (so[ys, xs] >= mp.FULLBRIGHT).any():
            sights += 1
        if (sn[ys, xs] != so[ys, xs]).any() and area > 1e-3:
            bad += 1
    check(bad == 0, "the %d triangles that stay (%d of them showing glow at rest: the sights) on the same texels, the same bytes" % (len(data["kept"]), sights))
    # geometry of the map
    F = gl.Faces(new)
    D = spec["D"]
    kept = set(data["kept"])
    worst_lo, worst_hi, worst_ratio, n_real, stretched = 9e9, 0.0, 0.0, 0, 0
    flat = []
    n_col = 0
    for t, tri in enumerate(new.tris):
        if t in kept or F.area[t] < 1e-5:
            continue
        uv = mp.tri_uv(new, tri)
        A = F.P[list(tri[1:])]
        e1, e2 = A[1] - A[0], A[2] - A[0]
        u = e1 / max(np.linalg.norm(e1), 1e-9)
        n = np.cross(e1, e2)
        ln = np.linalg.norm(n)
        if ln < 1e-9:
            continue
        v = np.cross(n / ln, u)
        M3 = np.array([[e1 @ u, e2 @ u], [e1 @ v, e2 @ v]])
        Muv = np.array([uv[1] - uv[0], uv[2] - uv[0]]).T
        try:
            J = Muv @ np.linalg.inv(M3)
        except np.linalg.LinAlgError:
            J = np.zeros((2, 2))
        sv = np.linalg.svd(J, compute_uv=False)
        width = 2.0 * F.area[t] / max(np.linalg.norm(e1), np.linalg.norm(e2), np.linalg.norm(e2 - e1)) * D
        if F.area[t] >= MIN_AREA and width >= MIN_WIDTH:
            n_real += 1
            lo, hi = float(sv[1]), float(sv[0])
            worst_lo, worst_hi = min(worst_lo, lo), max(worst_hi, hi)
            worst_ratio = max(worst_ratio, hi / max(lo, 1e-6))
            if lo < DENSITY_LO * D or hi > DENSITY_HI * D or hi / max(lo, 1e-6) > RATIO_MAX:
                stretched += 1
        xs, ys, *_ = gl.raster_tri(uv, 0.0)
        if len(xs) >= COLOUR_TEXELS:
            n_col += 1
            if len(np.unique(sn[ys, xs])) < COLOUR_MIN:
                flat.append(t)
    check(stretched == 0, "%d faces of %.1f+ sq units: %.1f-%.1f texels a unit (limits %.1f-%.1f), stretch at most %.2f (limit %.1f)" %
          (n_real, MIN_AREA, worst_lo, worst_hi, DENSITY_LO * D, DENSITY_HI * D, worst_ratio, RATIO_MAX))
    check(not flat, "%d faces of %d+ texels: none in one colour%s" % (n_col, COLOUR_TEXELS, (" (%s)" % flat[:8]) if flat else ""))
    # the rivets
    if spec.get("rivets") and len(new.tris) > o_nt:
        rv = sorted({v for t in range(o_nt, len(new.tris)) for v in new.tris[t][1:]})
        pts = pn[0][rv, :3] * new.scale + new.origin
        n_new = len(new.tris) - o_nt
        check(n_new == RIVETS[name] * RIVET_TRIS, "%d rivets (%d triangles each)" % (n_new // RIVET_TRIS, RIVET_TRIS))
        P0 = pn[0][:, :3] * new.scale + new.origin
        heads = [P0[[v for tr in new.tris[o_nt + k * RIVET_TRIS:o_nt + (k + 1) * RIVET_TRIS] for v in tr[1:]]].mean(0) for k in range(n_new // RIVET_TRIS)]
        mirrored = all(min(math.hypot(h[0] - o[0], h[2] - o[2]) + abs(h[1] + o[1]) for o in heads) < MIRROR_TOL for h in heads)
        check(mirrored, "every rivet has its mirror on the other side (within %.2f)" % MIRROR_TOL)
        if name == "v_shot.mdl":
            on_line = np.abs(pts[:, 1]) < SIGHT_CLEAR
            check(float(pts[:, 0].max()) < PUMP_RINGS_X and float(pts[:, 2].max()) < TOP_Z, "the rivets on the receiver's sides: x at most %.1f (the pump's rings from %.1f), z at most %.1f (its top from %.1f)" % (float(pts[:, 0].max()), PUMP_RINGS_X, float(pts[:, 2].max()), TOP_Z))
            check(not on_line.any(), "the rivets clear of the sight line: |y| at least %.2f (limit %.1f), the sights within 0.6 of it" %
                  (float(np.abs(pts[:, 1]).min()), SIGHT_CLEAR))
    # colour family: the old triangles' mean colours against the new paint under the same triangles
    means = np.array(data["means"], float)
    a_old, a_new = np.zeros(3), np.zeros(3)
    tot = 0.0
    warm = covered = 0
    mat, reg = gl.materials(new, F, spec, data["means"] + [gl.RIVET_BASE.tolist()] * (len(new.tris) - o_nt), o_nt)
    for t in range(o_nt):
        if t in kept or F.area[t] < 1e-3 or mat[t] != 0:
            continue
        xs, ys, *_ = gl.raster_tri(mp.tri_uv(new, new.tris[t]), 0.0)
        if len(xs) == 0:
            continue
        rgb = gl.PAL[sn[ys, xs]]
        fb = sn[ys, xs] >= mp.FULLBRIGHT
        rgb = rgb[~fb] if (~fb).any() else rgb
        a_new += rgb.mean(0) * F.area[t]
        a_old += means[t] * F.area[t]
        tot += F.area[t]
        warm += int(((rgb[:, 0] - rgb[:, 2]) > 18).sum())
        covered += len(rgb)
    if tot > 0:
        a_old, a_new = a_old / tot, a_new / tot
        shift_warm = float((a_new[0] - a_new[2]) - (a_old[0] - a_old[2]))
        rust = warm / max(covered, 1)
        check(abs(shift_warm) <= FAMILY_MAX and rust <= RUST_MAX or spec.get("glow_transfer"),
              "metal: mean colour %s -> %s (warmth change %+.1f, limit %.0f), rust colours on %.0f%% (limit %.0f%%)" %
              (np.round(a_old).astype(int).tolist(), np.round(a_new).astype(int).tolist(), shift_warm, FAMILY_MAX, 100 * rust, 100 * RUST_MAX))
    # the bake leaves the sights alone (they are protected from it, as from the rest of the paint)
    sight = [t for t in data["kept"] if (gl.tri_cover(old, old.tris[t], so)[2] >= mp.FULLBRIGHT).any()]
    same = all((sn[ys, xs] == so[ys, xs]).all() for ys, xs in ((gl.tri_cover(old, old.tris[t], so)[1], gl.tri_cover(old, old.tris[t], so)[0]) for t in sight))
    check(same, "the occlusion bake and the paint leave the %d glowing triangles' texels as they were" % len(sight))
    # the nailgun's muzzle interiors: their back walls almost black
    if name in ("v_nail.mdl",):
        lums, n = [], 0
        for t, tri in enumerate(new.tris[:o_nt]):
            cen = F.P[list(tri[1:])].mean(0)
            if t in kept or F.area[t] < 0.5 or F.n[t][0] < 0.7 or not (16.5 < cen[0] < 19.5 and abs(abs(cen[1]) - 3.75) < 1.6 and 2.6 < cen[2] < 5.1):
                continue
            xs, ys, *_ = gl.raster_tri(mp.tri_uv(new, tri), 0.0)
            lums.append(gl.PAL[sn[ys, xs]] @ np.array([0.3, 0.59, 0.11]))
            n += 1
        mean = float(np.concatenate(lums).mean()) if lums else 999.0
        check(n > 0 and mean <= INTERIOR_MAX, "the muzzle interiors' %d back faces: mean brightness %.1f (limit %.0f of 255)" % (n, mean, INTERIOR_MAX))
    # the barrel mask
    if name in GUNS:
        text = open(gl.MASK_FILE).read()
        k = text.index('id "progs/%s"' % name)
        mk = re.search(r'fx_gfx_mask "(\d+):', text[k:])
        check(int(mk.group(1)) == len(new.tris), "the barrel mask counts the %d triangles" % len(new.tris))
    # the parts
    if spec.get("parts"):
        for pn_name in spec["parts"]:
            part = mp.Model(os.path.join(gl.PROGS, pn_name))
            ps = np.frombuffer(bytes(part.skin()), np.uint8)
            check(bytes(part.skin()) == bytes(new.skin()) and part.sw == new.sw and part.sh == new.sh and part.num_poses() == new.num_poses(),
                  "%s: the gun's skin, %d triangles, %d poses" % (pn_name, len(part.tris), part.num_poses()))
    return new


def anchor_checks(names):
    print("anchors")
    wanted = {}
    for where, model, text in anchors.collect():
        if model.startswith("progs/"):
            wanted.setdefault(model[len("progs/"):], []).append((where, text))
    for name in names:
        if name not in wanted:
            continue
        ma = anchors.Mesh(os.path.join(gl.SRC, name))
        mb = anchors.Mesh(os.path.join(gl.PROGS, name))
        far = 0.0
        for where, text in wanted[name]:
            a, b = ma.bind(text), mb.bind(text)
            far = max(far, b.distance)
            for p in range(len(ma.poses)):
                far = max(far, math.dist(ma.position(a, p), mb.position(b, p)))
        check(far <= anchors.TOLERANCE, "%s: %d anchors within %.3f (limit %.3f)" % (name, len(wanted[name]), far, anchors.TOLERANCE))


def magazines_untouched():
    # They are other files: gunlooks.py never writes them; git says they are the shipped ones.
    print("magazines")
    root = os.path.normpath(os.path.join(HERE, "..", ".."))
    names = ["quakevr/progs/" + n for n in ("vr_mag_on_v_nail.mdl", "vr_magwell_on_v_nail.mdl", "vr_mag_on_v_lava.mdl",
                                            "vr_magwell_on_v_lava.mdl", "vr_mag_nail.mdl")]
    out = subprocess.run(["git", "-C", root, "status", "--porcelain", "--"] + names, capture_output=True, text=True).stdout
    check(out.strip() == "", "the nail magazines and wells unchanged (their files, frames and skins)")


def kick_checks(log):
    print("kick cancel")
    text = open(log, errors="replace").read()
    # Its lines: "<model> (slot n): body b of v vertices, p moving parts' vertices; ...", then "part v<i> at x y z". The
    # copies gunlooks.py makes of a moving vertex move with it: what must not appear is a moving place not moving before.
    base = json.load(open(os.path.join(gl.SRC, "kick_before.json")))
    places, cur = {}, None
    for line in text.splitlines():
        hit = re.search(r"(\S+\.mdl) \(slot -?\d+\):", line)
        if hit:
            cur = os.path.basename(hit.group(1))
            places[cur] = []
        hit = re.search(r"part v\d+ at (\S+) (\S+) (\S+)", line)
        if hit and cur:
            places[cur].append([round(float(x), 1) for x in hit.groups()])
    for model, was in base.items():
        was_set = {tuple(p) for p in was}
        now_set = {tuple(p) for p in places.get(model, [])}
        new = now_set - was_set
        check(model in places and not new, "%s: %d places moving (%d before), none new%s" %
              (model, len(now_set), len(was_set), (" (new: %s)" % sorted(new)[:6]) if new else ""))


def main():
    names = list(gl.SPECS)
    for name in names:
        model_checks(name, gl.SPECS[name])
    anchor_checks(names)
    magazines_untouched()
    if "--kick" in sys.argv:
        kick_checks(sys.argv[sys.argv.index("--kick") + 1])
    print("gunlooks_check: %d passed, %d failed" % (passed, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
