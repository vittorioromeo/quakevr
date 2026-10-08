#!/usr/bin/env python3
# ssg_checks.py -- reload_test.sh section 7's checks that read files (numpy, Pillow):
#   ssg_checks.py ring                 the super shotgun's rear sight ring is on its barrels part, not its frame
#                                      (make_ssg_open.py: it swings open with them): "ring barrels N frame M"
#   ssg_checks.py sights <png> ...     the sights' colour in screenshots: per shot, the pixels of the sights as recoloured
#                                      (the player's hue, yellow by default: within 12 degrees of 60) and as painted
#                                      (orange-red, not recoloured): "sights N/M ..."
#   ssg_checks.py blood <png> ...      blood in screenshots: per shot, its blood-red pixels (hue within 20 degrees of red,
#                                      saturated, not black): "blood N ..." (the super shotgun's blood shut and open)
#   ssg_checks.py snailwell            the super nailgun's magazine well on the flat lower band of its body's left face
#                                      (make_mags.py; the author's notes vrfiringrange_2026-10-07_23-58-30, 2026-10-08_
#                                      10-33-00): how far it stands off the face (its collar out along the magazine), how
#                                      far past the band's edges it reaches and how far into the body it goes (seated),
#                                      for v_nail2.mdl and v_lava2.mdl: "snailwell <gun> proud P over O into I ..."
#   ssg_checks.py pouchshells          the ammo pouch's shells (make_ammo_pouch.py; the author's note: overly big, not
#                                      symmetric), all five shown (frame 5), above its rim: the middle one's width across
#                                      (units) and how far the row is from its mirror image (the mean distance of each
#                                      point mirrored to the nearest point): "pouchshells across A asym M"
import colorsys
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))


def sight_tris(path):
    import numpy as np
    import mdlpolish as mp
    m = mp.Model(path)
    P = m.positions(0)
    sk = np.array(m.skins[0][2][0], np.uint8).reshape(m.sh, m.sw)
    n = 0
    for tri in m.tris:
        c = P[list(tri[1:])].mean(0)
        if not (12.0 < c[0] < 12.7 and c[2] > 6.9 and abs(c[1]) < 0.6):
            continue
        uv = mp.tri_uv(m, tri).mean(0)
        if 224 <= sk[int(min(m.sh - 1, uv[1])), int(min(m.sw - 1, uv[0]))] <= 239:
            n += 1
    return n


def snail_well(gun):
    """(proud, over, into): how far vr_magwell_on_<gun>'s vertices stand off the band's plane, past its two edges, and
    into the body below the plane."""
    import numpy as np
    import mdlpolish as mp
    progs = os.path.join(HERE, "..", "..", "..", "quakevr", "progs")
    m = mp.Model(os.path.join(progs, gun))
    P = m.positions(0)
    pts = []
    for tri in m.tris:
        a, b, c = P[list(tri[1:])]
        n = np.cross(b - a, c - a)
        if np.linalg.norm(n) < 0.6:
            continue
        n = n / np.linalg.norm(n)
        if abs(n[0]) < 0.05 and abs(abs(n[1]) - 0.93) < 0.03 and abs(abs(n[2]) - 0.36) < 0.04 and min(a[1], b[1], c[1]) > 4.3 and max(a[0], b[0], c[0]) < 20:
            pts += [a, b, c]
    band = np.array(pts)
    centre = band.mean(0)
    nrm = np.linalg.svd(band - centre)[2][2]
    nrm = nrm * np.sign(nrm[1])
    w = np.cross(nrm, [1.0, 0.0, 0.0])
    lo, hi = ((band - centre) @ w).min(), ((band - centre) @ w).max()
    W = mp.Model(os.path.join(progs, "vr_magwell_on_" + gun)).positions(0) - centre
    return (W @ nrm).max(), max(lo - (W @ w).min(), (W @ w).max() - hi), -(W @ nrm).min()


def pouch_shells():
    import numpy as np
    import mdlpolish as mp
    m = mp.Model(os.path.join(HERE, "..", "..", "..", "quakevr", "progs", "vrpouch_ammo.mdl"))
    P = m.positions(5)
    P = P[P[:, 2] > 1.6]  # (above the rim: the shells' heads and hulls)
    mid = P[np.abs(P[:, 1]) < 0.45]
    mirrored = P * np.array([1.0, -1.0, 1.0])
    d = np.sqrt(((mirrored[:, None, :] - P[None, :, :]) ** 2).sum(-1)).min(1)
    return mid[:, 0].max() - mid[:, 0].min(), d.mean()


def main():
    if sys.argv[1] == "pouchshells":
        print("pouchshells across %.2f asym %.3f" % pouch_shells())
        return
    if sys.argv[1] == "snailwell":
        print(" ".join("snailwell %s proud %.2f over %.2f into %.2f" % ((g,) + snail_well(g)) for g in ("v_nail2.mdl", "v_lava2.mdl")))
        return
    if sys.argv[1] == "ring":
        progs = os.path.join(HERE, "..", "..", "..", "quakevr", "progs")
        b = sight_tris(os.path.join(progs, "vr_ssg_barrels_on_v_shot2.mdl"))
        f = sight_tris(os.path.join(progs, "vr_ssg_frame_on_v_shot2.mdl"))
        print("ring barrels %d frame %d" % (b, f))
        return
    from PIL import Image
    if sys.argv[1] == "blood":
        counts = []
        for path in sys.argv[2:]:
            n = 0
            for r, g, b in Image.open(path).convert("RGB").getdata():
                h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
                if s > 0.5 and v > 0.12 and (h * 360 < 20 or h * 360 > 340):
                    n += 1
            counts.append(str(n))
        print("blood " + " ".join(counts))
        return
    out = []
    for path in sys.argv[2:]:
        near = red = 0
        im = Image.open(path).convert("RGB")
        for r, g, b in im.getdata():
            h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
            if s > 0.55 and v > 0.45:
                if abs(h * 360 - 60) <= 12:
                    near += 1
                elif h * 360 < 35 or h * 360 > 340:
                    red += 1
        out.append("%d/%d" % (near, red))
    print("sights " + " ".join(out))


if __name__ == "__main__":
    main()
