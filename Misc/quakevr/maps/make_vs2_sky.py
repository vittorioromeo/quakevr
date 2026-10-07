# make_vs2_sky.py -- draws vrstart2's night sky: a sky box (quakevr/gfx/env/vs2night{rt,bk,lf,ft,up,dn}.png), our
# own pictures (no game's data): a deep blue gradient to a faint glow at the horizon, thousands of stars, the Milky
# Way's band, a few thin clouds and the moon where vrstart2's moonlight comes from (its worldspawn _sunlight_mangle).
#
#   python Misc/quakevr/maps/make_vs2_sky.py [--size 1024]
#
# Deterministic (fixed seeds). The faces follow Ironwail's sky box layout (gl_sky.c: st_to_vec, skytexorder).
import argparse
import math
import os
import random
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
OUTDIR = os.path.join(ROOT, "quakevr", "gfx", "env")
NAME = "vs2night"
SUFFIX = ["rt", "bk", "lf", "ft", "up", "dn"]
SKYTEXORDER = [0, 2, 1, 3, 4, 5]
ST_TO_VEC = [(3, -1, 2), (-3, 1, 2), (1, 3, 2), (-1, -3, 2), (-2, -1, 3), (2, -1, -3)]
# the moonlight's direction (vrstart2_gen.py's _sunlight_mangle, yaw and pitch): the moon is opposite it
MOON_MANGLE = (240, -30)


def face_dir(axis, s, t):
    b = (s, t, 1.0)
    v = []
    for k in ST_TO_VEC[axis]:
        v.append(-b[-k - 1] if k < 0 else b[k - 1])
    l = math.sqrt(v[0] ** 2 + v[1] ** 2 + v[2] ** 2)
    return (v[0] / l, v[1] / l, v[2] / l)


def dir_to_face(d):
    """(file index, px, py in 0..1) of a direction."""
    ax = max(range(3), key=lambda i: abs(d[i]))
    best = None
    for axis in range(6):
        j3 = [abs(k) for k in ST_TO_VEC[axis]].index(3)
        if j3 != ax or (ST_TO_VEC[axis][j3] > 0) != (d[ax] > 0):
            continue
        # invert: b = (s, t, 1) scaled
        b = [0.0, 0.0, 0.0]
        for j, kk in enumerate(ST_TO_VEC[axis]):
            b[abs(kk) - 1] = d[j] if kk > 0 else -d[j]
        s, t = b[0] / b[2], b[1] / b[2]
        best = (SKYTEXORDER[axis], (s + 1) / 2, 1 - (t + 1) / 2)
    return best


class Noise3:
    def __init__(self, seed):
        r = random.Random(seed)
        self.p = [r.randrange(1 << 30) for _ in range(512)]

    def h(self, x, y, z):
        return (self.p[(x * 73 + y * 151 + z * 283) & 511] ^ (x * 19349663 ^ y * 83492791 ^ z * 2971215073)) % 10007 / 10007.0

    def __call__(self, x, y, z):
        xi, yi, zi = math.floor(x), math.floor(y), math.floor(z)
        fx, fy, fz = x - xi, y - yi, z - zi
        fx, fy, fz = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy), fz * fz * (3 - 2 * fz)
        v = 0.0
        for dx in (0, 1):
            for dy in (0, 1):
                for dz in (0, 1):
                    w = (fx if dx else 1 - fx) * (fy if dy else 1 - fy) * (fz if dz else 1 - fz)
                    v += w * self.h(xi + dx, yi + dy, zi + dz)
        return v

    def fbm(self, x, y, z, oct=4):
        s, a, f, n = 0.0, 1.0, 1.0, 0.0
        for _ in range(oct):
            s += a * self(x * f, y * f, z * f)
            n += a
            a *= 0.5
            f *= 2.03
        return s / n


def png(path, w, h, rows):
    raw = b"".join(b"\0" + bytes(r) for r in rows)

    def chunk(t, data):
        return struct.pack(">I", len(data)) + t + data + struct.pack(">I", zlib.crc32(t + data) & 0xffffffff)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=int, default=1024)
    args = ap.parse_args()
    N = args.size
    L = 128  # the smooth layers (gradient, clouds, Milky Way) are drawn at this size and enlarged
    yaw, pitch = map(math.radians, MOON_MANGLE)
    moon = (-math.cos(pitch) * math.cos(yaw), -math.cos(pitch) * math.sin(yaw), -math.sin(pitch))
    # the Milky Way's plane: its normal, tilted
    mw_n = (0.35, -0.55, 0.76)
    l = math.sqrt(sum(c * c for c in mw_n))
    mw_n = tuple(c / l for c in mw_n)
    clouds, dust = Noise3(5), Noise3(9)

    def smooth(d):
        x, y, z = d
        el = max(-0.2, z)
        # the gradient: navy at the zenith, a faint blue-grey glow at the horizon
        k = max(0.0, min(1.0, el / 0.55)) ** 0.6
        col = [26 * (1 - k) + 3 * k, 34 * (1 - k) + 5 * k, 52 * (1 - k) + 14 * k]
        # the Milky Way
        dm = abs(x * mw_n[0] + y * mw_n[1] + z * mw_n[2])
        band = math.exp(-(dm / 0.16) ** 2) * (0.35 + 0.9 * dust.fbm(x * 5 + 3, y * 5, z * 5, 5))
        dark = dust.fbm(x * 9, y * 9 + 7, z * 9, 3)
        band *= max(0.0, 1.0 - 1.5 * max(0.0, dark - 0.5))
        col = [col[0] + 26 * band, col[1] + 26 * band, col[2] + 34 * band]
        # thin clouds, lit from the moon's side
        c = clouds.fbm(x * 2.2, y * 2.2, z * 4.0 + 1, 5)
        cl = max(0.0, c - 0.52) * 2.6 * max(0.0, min(1.0, (el + 0.05) / 0.3))
        md = x * moon[0] + y * moon[1] + z * moon[2]
        lit = 0.35 + 0.65 * max(0.0, md) ** 4
        col = [col[0] * (1 - cl) + 60 * lit * cl, col[1] * (1 - cl) + 66 * lit * cl, col[2] * (1 - cl) + 82 * lit * cl]
        # the moon's halo
        halo = math.exp(-max(0.0, 1 - md) * 60) * 70 + math.exp(-max(0.0, 1 - md) * 600) * 120
        col = [col[0] + halo * 0.85, col[1] + halo * 0.9, col[2] + halo]
        return col, cl

    faces = []
    for f in range(6):
        axis = SKYTEXORDER.index(f)
        lay = []
        for py in range(L + 1):
            row = []
            for px in range(L + 1):
                s = 2 * px / L - 1
                t = 1 - 2 * py / L
                row.append(smooth(face_dir(axis, s, t)))
            lay.append(row)
        img = [[0.0] * (3 * N) for _ in range(N)]
        cov = [[0.0] * N for _ in range(N)]
        for y in range(N):
            fy = y / (N - 1) * L
            j = min(int(fy), L - 1)
            ty = fy - j
            for x in range(N):
                fx = x / (N - 1) * L
                i = min(int(fx), L - 1)
                tx = fx - i
                a, b, c, d = lay[j][i], lay[j][i + 1], lay[j + 1][i], lay[j + 1][i + 1]
                for k in range(3):
                    top = a[0][k] + (b[0][k] - a[0][k]) * tx
                    bot = c[0][k] + (d[0][k] - c[0][k]) * tx
                    img[y][3 * x + k] = top + (bot - top) * ty
                cov[y][x] = (a[1] + (b[1] - a[1]) * tx) * (1 - ty) + (c[1] + (d[1] - c[1]) * tx) * ty
        faces.append((img, cov))
    # the stars: random directions, more along the Milky Way; dimmed by the clouds
    rnd = random.Random(42)
    stars = []
    for _ in range(9000):
        while True:
            v = (rnd.gauss(0, 1), rnd.gauss(0, 1), rnd.gauss(0, 1))
            l = math.sqrt(sum(c * c for c in v))
            v = tuple(c / l for c in v)
            dm = abs(sum(v[i] * mw_n[i] for i in range(3)))
            if rnd.random() < 0.35 + 0.65 * math.exp(-(dm / 0.2) ** 2):
                break
        mag = rnd.random() ** 6
        tint = rnd.choice([(1.0, 0.95, 0.85), (0.85, 0.9, 1.0), (1.0, 1.0, 1.0), (1.0, 0.85, 0.7)])
        stars.append((v, 60 + 195 * mag, mag, tint))
    for v, br, mag, tint in stars:
        if v[2] < -0.15:
            continue
        f, u, w = dir_to_face(v)
        img, cov = faces[f]
        cx, cy = u * (N - 1), w * (N - 1)
        r = 0.6 + 1.1 * mag * (N / 1024)
        for y in range(int(cy - 2 * r) - 1, int(cy + 2 * r) + 2):
            for x in range(int(cx - 2 * r) - 1, int(cx + 2 * r) + 2):
                if 0 <= x < N and 0 <= y < N:
                    g = math.exp(-((x - cx) ** 2 + (y - cy) ** 2) / (r * r))
                    k = br * g * max(0.0, 1 - 1.6 * cov[y][x])
                    for c in range(3):
                        img[y][3 * x + c] += k * tint[c]
    # the moon's disc: craters and a soft edge
    f, u, w = dir_to_face(moon)
    img, cov = faces[f]
    mr = 0.045 * (N - 1) / 2 * 2  # its radius in pixels (about 2.6 degrees)
    cx, cy = u * (N - 1), w * (N - 1)
    mare = Noise3(17)
    for y in range(int(cy - mr - 3), int(cy + mr + 4)):
        for x in range(int(cx - mr - 3), int(cx + mr + 4)):
            if 0 <= x < N and 0 <= y < N:
                d = math.hypot(x - cx, y - cy) / mr
                if d < 1.08:
                    a = max(0.0, min(1.0, (1.06 - d) / 0.08))
                    m = mare.fbm((x - cx) / mr * 2.2 + 5, (y - cy) / mr * 2.2, 1.3, 4)
                    lum = 235 - 70 * max(0.0, m - 0.45) * 2.2 - 25 * d * d
                    for c, t in enumerate((0.97, 0.98, 1.0)):
                        img[y][3 * x + c] = img[y][3 * x + c] * (1 - a) + lum * t * a
    os.makedirs(OUTDIR, exist_ok=True)
    rd = random.Random(3)
    for f, (img, _) in enumerate(faces):
        rows = []
        for y in range(N):
            # a little dither so the gradient has no bands
            rows.append([max(0, min(255, int(v + rd.random()))) for v in img[y]])
        path = os.path.join(OUTDIR, NAME + SUFFIX[f] + ".png")
        png(path, N, N, rows)
        print("wrote %s (%d bytes)" % (os.path.relpath(path, ROOT), os.path.getsize(path)))


if __name__ == "__main__":
    main()
