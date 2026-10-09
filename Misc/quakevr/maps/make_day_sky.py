# make_day_sky.py -- draws vrtutorial's day sky: a sky box (quakevr/gfx/env/qvrday{rt,bk,lf,ft,up,dn}.png), our own
# pictures (no game's data): a clear blue gradient, pale at the horizon, fair-weather clouds lit from the sun's side,
# and the sun with its glow where the map's sunlight comes from (vrtutorial_gen.py's _sunlight_mangle).
#
#   python Misc/quakevr/maps/make_day_sky.py [--size 512]
#
# Deterministic (fixed seeds). The faces follow Ironwail's sky box layout (make_vs2_sky.py's helpers).
import argparse
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from make_vs2_sky import SKYTEXORDER, SUFFIX, Noise3, dir_to_face, face_dir, png  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
OUTDIR = os.path.join(ROOT, "quakevr", "gfx", "env")
NAME = "qvrday"
# the sunlight's direction (vrtutorial_gen.py's _sunlight_mangle, yaw and pitch: where the light travels): the sun is
# opposite it
SUN_MANGLE = (225, -55)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=int, default=512)
    args = ap.parse_args()
    N = args.size
    L = 256  # the smooth layers are drawn at this size and enlarged
    yaw, pitch = map(math.radians, SUN_MANGLE)
    sun = (-math.cos(pitch) * math.cos(yaw), -math.cos(pitch) * math.sin(yaw), -math.sin(pitch))
    clouds, wisps = Noise3(23), Noise3(31)

    def smooth(d):
        x, y, z = d
        el = max(-0.3, z)
        # the gradient: deep blue at the zenith, pale and hazy at the horizon; a dusty ground haze below it
        k = max(0.0, min(1.0, el / 0.7)) ** 0.55
        col = [196 * (1 - k) + 58 * k, 214 * (1 - k) + 112 * k, 232 * (1 - k) + 204 * k]
        if el < 0:
            g = min(1.0, -el / 0.12)
            col = [col[0] * (1 - g) + 150 * g, col[1] * (1 - g) + 152 * g, col[2] * (1 - g) + 146 * g]
        sd = x * sun[0] + y * sun[1] + z * sun[2]
        # fair-weather clouds: flattened toward the horizon, thicker in places, their sun side bright
        if el > -0.02:
            h = 1.0 / max(0.12, el + 0.08)
            px, py = x * h * 0.9, y * h * 0.9
            c = clouds.fbm(px + 11, py + 3, 0.7, 5)
            w = wisps.fbm(px * 2.3, py * 2.3, 4.1, 4)
            cov = max(0.0, c - 0.5) * 4.0 * max(0.0, min(1.0, (el + 0.02) / 0.25))
            cov = min(1.0, cov + max(0.0, w - 0.62) * 0.8 * max(0.0, min(1.0, el / 0.3)))
            shade = 0.72 + 0.28 * max(0.0, sd) + 0.12 * (c - 0.5)
            base = [246 * shade, 246 * shade, 250 * min(1.0, shade + 0.04)]
            col = [col[i] * (1 - cov) + base[i] * cov for i in range(3)]
        else:
            cov = 0.0
        # the sun's glow
        glow = math.exp(-max(0.0, 1 - sd) * 9) * 60 + math.exp(-max(0.0, 1 - sd) * 90) * 90
        col = [col[0] + glow, col[1] + glow * 0.95, col[2] + glow * 0.8]
        return col, cov

    faces = []
    for f in range(6):
        axis = SKYTEXORDER.index(f)
        lay = [[smooth(face_dir(axis, 2 * px / L - 1, 1 - 2 * py / L)) for px in range(L + 1)] for py in range(L + 1)]
        img = [[0.0] * (3 * N) for _ in range(N)]
        for y in range(N):
            fy = y / (N - 1) * L
            j = min(int(fy), L - 1)
            ty = fy - j
            for x in range(N):
                fx = x / (N - 1) * L
                i = min(int(fx), L - 1)
                tx = fx - i
                a, b, c, d = lay[j][i][0], lay[j][i + 1][0], lay[j + 1][i][0], lay[j + 1][i + 1][0]
                for k in range(3):
                    top = a[k] + (b[k] - a[k]) * tx
                    bot = c[k] + (d[k] - c[k]) * tx
                    img[y][3 * x + k] = top + (bot - top) * ty
        faces.append(img)
    # the sun's disc
    f, u, w = dir_to_face(sun)
    img = faces[f]
    sr = 0.03 * (N - 1)
    cx, cy = u * (N - 1), w * (N - 1)
    for y in range(int(cy - sr - 3), int(cy + sr + 4)):
        for x in range(int(cx - sr - 3), int(cx + sr + 4)):
            if 0 <= x < N and 0 <= y < N:
                a = max(0.0, min(1.0, (1.08 - math.hypot(x - cx, y - cy) / sr) / 0.1))
                for c, t in enumerate((255, 252, 240)):
                    img[y][3 * x + c] = img[y][3 * x + c] * (1 - a) + t * a
    os.makedirs(OUTDIR, exist_ok=True)
    rd = random.Random(3)
    for f, img in enumerate(faces):
        rows = [[max(0, min(255, int(v + rd.random()))) for v in img[y]] for y in range(N)]
        path = os.path.join(OUTDIR, NAME + SUFFIX[f] + ".png")
        png(path, N, N, rows)
        print("wrote %s (%d bytes)" % (os.path.relpath(path, ROOT), os.path.getsize(path)))


if __name__ == "__main__":
    main()
