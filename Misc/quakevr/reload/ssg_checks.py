#!/usr/bin/env python3
# ssg_checks.py -- reload_test.sh section 7's checks that read files (numpy, Pillow):
#   ssg_checks.py ring                 the super shotgun's rear sight ring is on its barrels part, not its frame
#                                      (make_ssg_open.py: it swings open with them): "ring barrels N frame M"
#   ssg_checks.py sights <png> ...     the sights' colour in screenshots: per shot, the pixels of the sights as recoloured
#                                      (the player's hue, yellow by default: within 12 degrees of 60) and as painted
#                                      (orange-red, not recoloured): "sights N/M ..."
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


def main():
    if sys.argv[1] == "ring":
        progs = os.path.join(HERE, "..", "..", "..", "quakevr", "progs")
        b = sight_tris(os.path.join(progs, "vr_ssg_barrels_on_v_shot2.mdl"))
        f = sight_tris(os.path.join(progs, "vr_ssg_frame_on_v_shot2.mdl"))
        print("ring barrels %d frame %d" % (b, f))
        return
    from PIL import Image
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
