"""pouch_pile.py -- the ammo pouch's contents designed (ROUND22.md, "Ammo pouch contents"; Quake/vr/vr_pouchitems.cpp):
the shells' pile table and the pouch frames' sizes, in its model units (+x out of the body, +y left, +z up).

    python Misc/quakevr/pouch_pile.py shells 0.04        the 20 shells' pile (vr_pouchitems.cpp pile) and its middles
    python Misc/quakevr/pouch_pile.py mags|gren|rockets  the depth each frame needs for its rows (make_ammo_pouch.py)

The pile: each shell dropped in at a random turn and tilt (seeded), resting at the lowest place 0.05 units clear of
the others (capsules) and inside the inside's outline, the middle preferred (strongly for the first six)."""
import math, random, sys

BACK_X, INNER, P = 0.1, 0.93, 6.0


def front_x(y, d, hw):
    a = min(1.0, abs(y) / hw)
    return BACK_X + d * (1.0 - a ** P) ** (1.0 / P)


RING = 22
_polys = {}


def inner_poly(d, hw):
    """make_pouch.py outline(depth, INNER) with RING points."""
    key = (d, hw)
    if key not in _polys:
        pts = []
        n_front = RING - 2
        for k in range(n_front):
            t = k / (n_front - 1)
            yy = hw * math.cos(math.pi * t)
            pts.append((front_x(yy, d, hw), yy))
        pts.append((BACK_X, -hw * 0.93))
        pts.append((BACK_X, hw * 0.93))
        mid = BACK_X + d * 0.45
        _polys[key] = [(mid + (px - mid) * INNER, py * INNER) for px, py in pts]
    return _polys[key]


def wall_clear(x, y, d, hw):
    """Signed distance inside the inner wall's outline (negative: outside)."""
    poly = inner_poly(d, hw)
    inside = False
    best = 1e9
    for i in range(len(poly)):
        (x1, y1), (x2, y2) = poly[i], poly[(i + 1) % len(poly)]
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
            inside = not inside
        dx, dy = x2 - x1, y2 - y1
        t = max(0.0, min(1.0, ((x - x1) * dx + (y - y1) * dy) / (dx * dx + dy * dy)))
        best = min(best, math.hypot(x - x1 - dx * t, y - y1 - dy * t))
    return best if inside else -best


def inner_back(d):
    return BACK_X + 0.45 * d * (1 - INNER)


def inner_front(d):
    return BACK_X + 0.45 * d + 0.55 * d * INNER


def inner_centre(d):
    return (inner_back(d) + inner_front(d)) * 0.5


# --- capsules ---------------------------------------------------------------------------------------------------
def seg_dist(p1, q1, p2, q2):
    d1 = [q1[i] - p1[i] for i in range(3)]
    d2 = [q2[i] - p2[i] for i in range(3)]
    r = [p1[i] - p2[i] for i in range(3)]
    a = sum(v * v for v in d1); e = sum(v * v for v in d2); f = sum(d2[i] * r[i] for i in range(3))
    c = sum(d1[i] * r[i] for i in range(3)); b = sum(d1[i] * d2[i] for i in range(3))
    den = a * e - b * b
    s = min(1, max(0, (b * f - c * e) / den)) if den > 1e-9 else 0.0
    t = (b * s + f) / e
    if t < 0:
        t = 0; s = min(1, max(0, -c / a))
    elif t > 1:
        t = 1; s = min(1, max(0, (b - c) / a))
    pa = [p1[i] + d1[i] * s for i in range(3)]
    pb = [p2[i] + d2[i] * t for i in range(3)]
    return math.dist(pa, pb)


def capsule(c, yaw, pitch, hl):
    ax = (math.cos(pitch) * math.cos(yaw), math.cos(pitch) * math.sin(yaw), math.sin(pitch))
    return [c[i] - ax[i] * hl for i in range(3)], [c[i] + ax[i] * hl for i in range(3)]


def caps_wall(p, q, r, d, hw, floor):
    w = 1e9
    for k in range(5):
        t = k / 4
        x, y, z = (p[i] + (q[i] - p[i]) * t for i in range(3))
        w = min(w, wall_clear(x, y, d, hw) - r, z - r - floor + 0.03)
    return w


# --- shells: a pile -------------------------------------------------------------------------------------------------
SH_R, SH_HL = 0.345, 2.297 * 0.5 - 0.345
SH_D, SH_HW, SH_FLOOR, SH_GAP, SH_W = 3.0, 4.6, 0.8, 0.05, float(sys.argv[2]) if len(sys.argv) > 2 else 0.1


def shell_pile(n, seed=7):
    rng = random.Random(seed)
    cx = inner_centre(SH_D)
    placed = []
    for i in range(n):
        best = None
        for trial in range(500):
            yaw = rng.uniform(0, math.pi)
            if rng.random() < 0.7:
                yaw = math.pi / 2 + rng.gauss(0, 0.35)  # mostly along the width (the pouch's long side)
            pitch = rng.gauss(0, 0.08)
            x = cx + rng.uniform(-0.9, 0.9)
            y = rng.uniform(-3.4, 3.4)
            z = SH_FLOOR + SH_R + abs(math.sin(pitch)) * SH_HL
            ok = False
            while z < SH_FLOOR + 5:
                p, q = capsule((x, y, z), yaw, pitch, SH_HL)
                if all(seg_dist(p, q, a, b) >= 2 * SH_R + SH_GAP for a, b, _ in placed):
                    ok = True
                    break
                z += 0.01
            if not ok or caps_wall(p, q, SH_R, SH_D, SH_HW, SH_FLOOR - 0.01) < 0.03:
                continue
            score = z + (0.15 if i < 6 else SH_W) * (abs(y) + 1.5 * abs(x - cx))
            if best is None or score < best[0]:
                best = (score, (x, y, z, yaw, pitch))
        x, y, z, yaw, pitch = best[1]
        p, q = capsule((x, y, z), yaw, pitch, SH_HL)
        placed.append((p, q, best[1]))
    return [b for _, _, b in placed]


# --- magazines: two rows of three ------------------------------------------------------------------------------------
MAGS = {"nail": (2.068, 1.111), "snail": (2.232, 1.089), "cell": (1.665, 1.44)}  # held: width (y), thickness (x)
GAP_Y, GAP_X = 0.16, 0.12


def mag_layout(kind, n, d):
    w, t = MAGS[kind]
    cx = inner_centre(d)
    rows = [n] if n <= 3 else [3, n - 3]
    out = []
    for r, m in enumerate(rows):
        x = cx if len(rows) == 1 else cx + (-1 if r == 0 else 1) * (t + GAP_X) * 0.5
        for j in range(m):
            out.append((x, (j - (m - 1) * 0.5) * (w + GAP_Y)))
    return out


def mag_clear(kind, n, d, hw):
    w, t = MAGS[kind]
    c = 1e9
    for x, y in mag_layout(kind, n, d):
        for sx in (-1, 1):
            for sy in (-1, 1):
                c = min(c, wall_clear(x + sx * t / 2, y + sy * w / 2, d, hw))
    return c


# --- grenades: 3 at the back, 2 in front -------------------------------------------------------------------------------
GREN = {"grenade": 1.08, "mervup": 1.23, "prox": 0.92}
GGAP = 0.1


def gren_layout(kind, n, d, hw):
    r = GREN[kind]
    cx = inner_centre(d)
    D = 2 * r + GGAP
    if n <= 3:
        return [(cx, (j - (n - 1) * 0.5) * D) for j in range(n)], D, 0
    # the back row's pitch as wide as the pouch lets (the front pair nests deeper)
    half = (hw * 0.93 * INNER) - r - 0.15
    pitch = min(half, D * 1.45)
    dx = math.sqrt(max(0.0, D * D - (pitch * 0.5) ** 2))
    back, front = cx - dx * 0.5, cx + dx * 0.5
    pts = [(back, (j - 1) * pitch) for j in range(3)]
    pts += [(front, -pitch * 0.5)] if n == 4 else [(front, -pitch * 0.5), (front, pitch * 0.5)]
    return pts, pitch, dx


def gren_clear(kind, n, d, hw):
    r = GREN[kind]
    pts, _, _ = gren_layout(kind, n, d, hw)
    c = 1e9
    for x, y in pts:
        for k in range(16):
            a = k * math.pi / 8
            c = min(c, wall_clear(x + r * math.cos(a), y + r * math.sin(a), d, hw))
    return c


# --- rockets: lying along the width, a zig-zag against the back and the front ----------------------------------------
RK_R, RK_BUTT, RK_TIP = 0.79, 3.94, 3.78
RK_GAP = 0.06


def rocket_layout(n, d, floor):
    ib, ifr = inner_back(d), inner_front(d)
    cx = inner_centre(d)
    if n == 1:
        return [(cx, 0.0, floor + RK_R)]
    xb, xf = ib + RK_R + 0.07, ifr - RK_R - 0.07
    dx = xf - xb
    D = 2 * RK_R + RK_GAP
    dz = math.sqrt(max(0.0, D * D - dx * dx))
    return [((xb if k % 2 == 0 else xf), 0.0, floor + RK_R + k * dz) for k in range(n)]


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    if what in ("all", "mags"):
        for kind in MAGS:
            for hw in (4.4, 4.6, 4.8):
                for d in [2.5 + 0.1 * k for k in range(16)]:
                    if min(mag_clear(kind, n, d, hw) for n in range(1, 7)) >= 0.06:
                        print("mags %-5s hw %.1f: depth %.1f (centre %.2f)" % (kind, hw, d, inner_centre(d)))
                        break
    if what in ("all", "gren"):
        for kind in GREN:
            for hw in (4.6, 4.8, 5.0):
                for d in [2.5 + 0.1 * k for k in range(30)]:
                    if min(gren_clear(kind, n, d, hw) for n in range(1, 6)) >= 0.06:
                        pts, pitch, dx = gren_layout(kind, 5, d, hw)
                        print("gren %-7s hw %.1f: depth %.1f pitch %.2f dx %.2f" % (kind, hw, d, pitch, dx))
                        break
    if what in ("all", "rockets"):
        for hw in (4.8, 5.0, 5.2):
            for d in (3.4, 3.6, 3.7, 3.8):
                pts = rocket_layout(5, d, 0.0)
                # ends: the butt +-0.3 off along y and a 2 degree yaw
                cl = min(wall_clear(x + sx * RK_R, sy * (RK_BUTT + 0.2), d, hw) for x, _, _ in pts for sx in (-1, 1) for sy in (-1, 1))
                print("rockets hw %.1f d %.1f: wall %.2f  heights %s" % (hw, d, cl, [round(p[2], 2) for p in pts]))
    if what in ("all", "shells"):
        pile = shell_pile(20)
        for i, (x, y, z, yaw, pitch) in enumerate(pile):
            print("    {%.3ff, %.3ff, %.3ff, %.1ff, %.1ff}, // %d" % (x, y, z, math.degrees(yaw), math.degrees(pitch), i + 1))
        cx = inner_centre(SH_D)
        for n in (1, 3, 5, 10, 15, 20):
            sub = pile[:n]
            print("n %2d: centroid dx %.2f dy %.2f top %.2f" % (n, sum(p[0] for p in sub) / n - cx, sum(p[1] for p in sub) / n,
                                                             max(p[2] for p in sub) + SH_R))
