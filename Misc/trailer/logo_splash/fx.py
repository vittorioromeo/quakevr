# fx.py -- the logo splash's 2D effects (numpy): the blood on the wall (the burst's splash, the letters' splatters,
# the drips, "Unleashed" written in it), its wet shading, the flying droplets, dust, chips, embers, and the fire.
# Used by fx_precompute.py (the fields that don't depend on the frame) and composite.py (the frames).
#
# Pixel space: x right, y down, in a frame W x H. Every size here is in pixels of the 3840-wide frame and scaled by
# S = W / 3840, so the 1080p and 4K renders are the same picture.

import math
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C  # noqa: E402


# ---------------------------------------------------------------------------------------------------------------
# Filters

def box_axis(a, r, axis):
    """A box blur of radius r (integer) along one axis, edges clamped."""
    if r < 1:
        return a
    pad = [(0, 0)] * a.ndim
    pad[axis] = (r + 1, r)
    p = np.pad(a, pad, mode="edge")
    c = np.cumsum(p, axis=axis, dtype=np.float32)
    n = a.shape[axis]
    hi = np.take(c, np.arange(2 * r + 1, 2 * r + 1 + n), axis=axis)
    lo = np.take(c, np.arange(0, n), axis=axis)
    return (hi - lo) / (2 * r + 1)


def blur(a, sigma):
    """About a Gaussian of `sigma` pixels: three box passes (big sigmas on a smaller copy)."""
    if sigma < 0.5:
        return a
    if sigma > 24 and a.ndim == 2:
        k = int(sigma // 8)
        h, w = a.shape
        hs, ws = (h + k - 1) // k, (w + k - 1) // k
        small = np.asarray(Image.fromarray(a.astype(np.float32)).resize((ws, hs), Image.BILINEAR))
        small = blur(small, sigma / k)
        return np.asarray(Image.fromarray(small).resize((w, h), Image.BILINEAR))
    r = max(1, int(round(math.sqrt(12 * sigma * sigma / 3 + 1) / 2 - 0.5)))
    out = a.astype(np.float32)
    for _ in range(3):
        out = box_axis(out, r, 0)
        out = box_axis(out, r, 1)
    return out


def resize_f(a, w, h, resample=Image.BICUBIC):
    return np.asarray(Image.fromarray(a.astype(np.float32)).resize((w, h), resample))


def noise(h, w, cell, seed, octaves=4, gain=0.5):
    """Fractal value noise in about [0, 1]: random grids `cell` pixels apart and finer, smoothly upsampled."""
    r = np.random.default_rng(seed)
    out = np.zeros((h, w), np.float32)
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        c = max(2.0, cell / (2 ** o))
        gh, gw = int(h / c) + 3, int(w / c) + 3
        g = r.random((gh, gw)).astype(np.float32)
        up = resize_f(g, int(gw * c), int(gh * c))
        out += amp * up[:h, :w]
        tot += amp
        amp *= gain
    return out / tot


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def dilate(mask, r):
    """A max filter of radius r (square), via shifts of growing size."""
    out = mask.copy()
    k = 1
    done = 0
    while done < r:
        s = min(k, r - done)
        o = out.copy()
        o[s:, :] = np.maximum(o[s:, :], out[:-s, :])
        o[:-s, :] = np.maximum(o[:-s, :], out[s:, :])
        o2 = o.copy()
        o2[:, s:] = np.maximum(o2[:, s:], o[:, :-s])
        o2[:, :-s] = np.maximum(o2[:, :-s], o[:, s:])
        out = o2
        done += s
        k *= 2
    return out


# ---------------------------------------------------------------------------------------------------------------
# Stamps into the blood fields: T (thickness) and A (the frame it arrives)

class Field:
    def __init__(self, h, w):
        self.T = np.zeros((h, w), np.float32)
        self.A = np.full((h, w), 1e9, np.float32)
        self.h, self.w = h, w

    def _box(self, x0, y0, x1, y1):
        x0, y0 = max(0, int(math.floor(x0))), max(0, int(math.floor(y0)))
        x1, y1 = min(self.w, int(math.ceil(x1)) + 1), min(self.h, int(math.ceil(y1)) + 1)
        if x0 >= x1 or y0 >= y1:
            return None
        return x0, y0, x1, y1

    def capsule(self, p0, p1, r0, r1, height, t0, speed=1e9, mode="add"):
        """A tapered streak from p0 (radius r0) to p1 (r1), dome-shaped across, arriving from p0 at `speed` pixels a
        frame from frame t0."""
        rr = max(r0, r1) + 2
        b = self._box(min(p0[0], p1[0]) - rr, min(p0[1], p1[1]) - rr, max(p0[0], p1[0]) + rr, max(p0[1], p1[1]) + rr)
        if b is None:
            return
        x0, y0, x1, y1 = b
        yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        dx, dy = p1[0] - p0[0], p1[1] - p0[1]
        ll = dx * dx + dy * dy
        if ll < 1e-6:
            u = np.zeros_like(xx)
        else:
            u = np.clip(((xx - p0[0]) * dx + (yy - p0[1]) * dy) / ll, 0, 1)
        cx, cy = p0[0] + u * dx, p0[1] + u * dy
        d = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2)
        r = r0 + (r1 - r0) * u
        cov = np.clip(r - d + 0.5, 0, 1)
        prof = height * cov * np.sqrt(np.clip(1 - (d / np.maximum(r, 0.3)) ** 2, 0.04, 1))
        a = t0 + u * math.sqrt(ll) / speed
        T = self.T[y0:y1, x0:x1]
        A = self.A[y0:y1, x0:x1]
        hit = prof > 0.01
        if mode == "max":
            np.maximum(T, prof, out=T)
        else:
            T += prof
        A[hit] = np.minimum(A[hit], a[hit])

    def droplet(self, p, direction, r, length, height, t0, speed):
        """A drop that hit the wall moving along `direction`: a round head and a tail thrown on ahead of it."""
        dx, dy = direction
        tail = (p[0] + dx * length, p[1] + dy * length)
        self.capsule(p, tail, r, r * 0.25, height, t0, speed)


# ---------------------------------------------------------------------------------------------------------------
# The burst's splash

def chest_px(W):
    c, r, _ = C.project(C.CHEST, W)
    return c, r


def splash_fields(W, H, seed=7):
    """The burst's blood on the wall: (T, A), and the drips' specs."""
    S = W / 3840.0
    f = Field(H, W)
    cx, cy = chest_px(W)
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    # Warp the coordinates for an organic edge.
    wx = (noise(H, W, 260 * S, seed + 1, 4) - 0.5) * 120 * S + (noise(H, W, 40 * S, seed + 2, 3) - 0.5) * 22 * S
    wy = (noise(H, W, 260 * S, seed + 3, 4) - 0.5) * 120 * S + (noise(H, W, 40 * S, seed + 4, 3) - 0.5) * 22 * S
    px, py = xx + wx - cx, yy + wy - cy
    r = np.sqrt(px * px + py * py)
    th = np.arctan2(py, px)
    # The splash's outline: a wide ellipse (the logo's), lumpy, with a crown of sharp tendrils.
    a, b = 1380 * S, 560 * S
    re = 1.0 / np.sqrt((np.cos(th) / a) ** 2 + (np.sin(th) / b) ** 2)
    R = re * (1.0 + 0.06 * np.sin(th * 3 + 1.0) + 0.05 * np.sin(th * 7 + 2.0) + 0.04 * np.sin(th * 13 + 0.3))
    for k in range(70):
        tk = rng.uniform(-math.pi, math.pi)
        wk = rng.uniform(0.008, 0.035)
        horiz = abs(math.cos(tk)) ** 0.7
        ak = rng.uniform(0.05, 0.35) * (0.5 + horiz)
        d = np.angle(np.exp(1j * (th - tk)))
        R += re * ak * np.exp(-(d / wk) ** 2)
    F = 1.0 - r / R
    inside = np.clip(F * R / (1.2 * S) + 0.5, 0, 1)          # anti-aliased edge, about a pixel wide
    pool = noise(H, W, 180 * S, seed + 5, 5)
    T = inside * (0.35 + 0.9 * smoothstep(0.0, 0.5, F) * (0.4 + 0.8 * pool))
    # Thin, broken patches (where the spray was light).
    holes = noise(H, W, 70 * S, seed + 6, 3)
    T *= 1.0 - 0.75 * smoothstep(0.62, 0.7, holes) * smoothstep(0.35, 0.1, F)
    f.T += T.astype(np.float32)
    # It spreads out from the chest in about 9 frames, slowing.
    A = C.HIT + 1 + 8.0 * np.clip(r / R, 0, 1.5) ** 1.5
    f.A = np.where(T > 0.01, A, 1e9).astype(np.float32)
    del px, py, r, th, re, F, inside, pool, holes, wx, wy, xx, yy
    Rs = None

    def edge_point(t):
        """A point on the outline (about) at angle t and its outline radius."""
        rad = (1.0 / math.sqrt((math.cos(t) / a) ** 2 + (math.sin(t) / b) ** 2))
        return rad

    # Rays: long tapering spikes thrown out from the edge, longest sideways (the logo's).
    for k in range(95):
        t = rng.uniform(-math.pi, math.pi)
        horiz = abs(math.cos(t))
        rad = edge_point(t) * rng.uniform(0.75, 0.97)
        ln = edge_point(t) * rng.uniform(0.12, 0.45) * (0.6 + 1.4 * horiz ** 3)
        w0 = rng.uniform(6, 20) * S
        ox, oy = math.cos(t), math.sin(t)
        bend = rng.uniform(-0.15, 0.15)
        p = (cx + ox * rad, cy + oy * rad)
        tt = C.HIT + 3 + rng.uniform(0, 3)
        seg = 4
        for s in range(seg):
            ang = t + bend * (s + 1) / seg
            q = (p[0] + math.cos(ang) * ln / seg, p[1] + math.sin(ang) * ln / seg)
            f.capsule(p, q, w0 * (1 - s / seg), w0 * (1 - (s + 1) / seg) + 1.2 * S, 0.9, tt, 260 * S)
            tt += (ln / seg) / (260 * S)
            p = q
        for _ in range(rng.integers(1, 4)):   # drops flung off the tip
            d = rng.uniform(10, 120) * S
            q = (p[0] + math.cos(t) * d + rng.normal(0, 8 * S), p[1] + math.sin(t) * d + rng.normal(0, 8 * S))
            f.droplet(q, (math.cos(t), math.sin(t)), rng.uniform(2, 7) * S, rng.uniform(4, 20) * S, 0.8,
                      tt + d / (260 * S), 400 * S)
    # Satellite drops and mist around the splash.
    for k in range(1300):
        t = rng.uniform(-math.pi, math.pi)
        e = edge_point(t)
        rad = e * (0.95 + abs(rng.normal(0, 0.35)) * (0.6 + 0.8 * abs(math.cos(t))))
        p = (cx + math.cos(t) * rad, cy + math.sin(t) * rad)
        big = rng.random() < 0.18
        r0 = rng.uniform(4, 13) * S if big else rng.uniform(1.0, 3.5) * S
        h = 0.8 if big else rng.uniform(0.15, 0.6)
        f.droplet(p, (math.cos(t), math.sin(t)), r0, r0 * rng.uniform(0.5, 3.5), h,
                  C.HIT + 2 + rad / (300 * S), 500 * S)
    # Drips: where the splash's bottom edge is thick.
    drips = []
    thick = f.T > 0.45
    cols = rng.choice(np.arange(int(cx - 1500 * S), int(cx + 1500 * S)), 34, replace=False)
    for x in sorted(cols):
        x = int(np.clip(x, 0, W - 1))
        ys = np.nonzero(thick[:, x])[0]
        if len(ys) == 0:
            continue
        y0 = ys.max() - 6 * S
        drips.append(dict(x=float(x), y=float(y0), t0=float(C.HIT + rng.uniform(14, 70)),
                          length=float(rng.uniform(60, 620) * S * (1.3 if abs(x - cx) < 900 * S else 0.8)),
                          tau=float(rng.uniform(40, 150)), w=float(rng.uniform(5, 13) * S),
                          wob=float(rng.uniform(0, 6.28))))
    return f, drips


# ---------------------------------------------------------------------------------------------------------------
# The letters' landings

def footprint_info(alpha):
    """A letter's rest mask: its boundary points (x, y, outward normal) and its box."""
    m = alpha > 0.5
    ys, xs = np.nonzero(m)
    box = (xs.min(), ys.min(), xs.max(), ys.max())
    g = blur(m.astype(np.float32), 3)
    gy, gx = np.gradient(g)
    edge = (g > 0.2) & (g < 0.8)
    ey, ex = np.nonzero(edge)
    nx, ny = -gx[ey, ex], -gy[ey, ex]
    n = np.sqrt(nx * nx + ny * ny) + 1e-6
    return box, np.stack([ex, ey, nx / n, ny / n], 1).astype(np.float32)


def impact_fields(W, H, rests, seed=11):
    """The letters' splatters on the wall: (T, A), and the drips they start."""
    S = W / 3840.0
    f = Field(H, W)
    rng = np.random.default_rng(seed)
    drips = []
    for i, alpha in enumerate(rests):
        land = C.LAND[i]
        box, edge = footprint_info(alpha)
        bx = (box[0] + box[2]) * 0.5
        by = (box[1] + box[3]) * 0.5
        # Blood squeezed out from under the letter: a thick rim around it.
        m = (alpha > 0.5).astype(np.float32)
        x0, y0, x1, y1 = box
        pad = int(40 * S) + 2
        sl = (slice(max(0, y0 - pad), min(H, y1 + pad)), slice(max(0, x0 - pad), min(W, x1 + pad)))
        mm = m[sl]
        rim = blur(dilate(mm, int(14 * S) + 1), 4 * S) * (1 - mm)
        nz = noise(mm.shape[0], mm.shape[1], 30 * S, seed + i, 3)
        rim *= 0.4 + 0.9 * nz
        f.T[sl] += rim
        hit = rim > 0.02
        A = f.A[sl]
        A[hit] = np.minimum(A[hit], land + 1)
        # Spikes and drops squirted out from the edges.
        idx = rng.choice(len(edge), 44, replace=False)
        for k in idx:
            ex, ey, nx, ny = edge[k]
            # Mostly sideways and down/up along the wall, away from the letter's centre.
            ox, oy = ex - bx, ey - by
            on = math.hypot(ox, oy) + 1e-6
            dx, dy = 0.5 * nx + 0.5 * ox / on, 0.5 * ny + 0.5 * oy / on
            dn = math.hypot(dx, dy) + 1e-6
            dx, dy = dx / dn, dy / dn
            ln = rng.uniform(70, 420) * S
            w0 = rng.uniform(5, 14) * S
            p = (ex, ey)
            q = (ex + dx * ln, ey + dy * ln)
            f.capsule(p, q, w0, 1.0 * S, 0.8, land + 1, 220 * S)
            for _ in range(rng.integers(0, 3)):
                d = ln + rng.uniform(10, 90) * S
                c = (ex + dx * d + rng.normal(0, 6 * S), ey + dy * d + rng.normal(0, 6 * S))
                f.droplet(c, (dx, dy), rng.uniform(2, 6) * S, rng.uniform(3, 15) * S, 0.8, land + 1 + d / (220 * S),
                          400 * S)
        for _ in range(140):  # a fine spray
            k = rng.integers(len(edge))
            ex, ey, nx, ny = edge[k]
            d = abs(rng.normal(0, 120)) * S + 8 * S
            c = (ex + nx * d + rng.normal(0, 20 * S), ey + ny * d + rng.normal(0, 20 * S))
            f.droplet(c, (nx, ny), rng.uniform(1.0, 3.0) * S, rng.uniform(1, 8) * S, rng.uniform(0.2, 0.7),
                      land + 1 + d / (300 * S), 400 * S)
        # Drips from under the letter.
        for _ in range(rng.integers(1, 4)):
            x = rng.uniform(box[0] + 10 * S, box[2] - 10 * S)
            col = np.nonzero(alpha[:, int(x)] > 0.5)[0]
            if len(col) == 0:
                continue
            drips.append(dict(x=float(x), y=float(col.max() + 6 * S), t0=float(land + rng.uniform(6, 30)),
                              length=float(rng.uniform(80, 380) * S), tau=float(rng.uniform(40, 120)),
                              w=float(rng.uniform(5, 10) * S), wob=float(rng.uniform(0, 6.28))))
    return f, drips


def draw_drips(T, drips, frame, S):
    """The drips at `frame`, added to T: a run down the wall, thinning, with a bulb at its tip."""
    H, W = T.shape
    for d in drips:
        t = frame - d["t0"]
        if t <= 0:
            continue
        ln = d["length"] * (1 - math.exp(-t / d["tau"]))
        if ln < 2:
            continue
        x, y, w = d["x"], d["y"], d["w"]
        n = max(2, int(ln / (12 * S)) + 1)
        prev = (x, y)
        for k in range(1, n + 1):
            u = k / n
            px = x + math.sin(d["wob"] + u * 3.0) * 3 * S * u
            q = (px, y + ln * u)
            wa = w * (1 - 0.55 * (k - 1) / n)
            wb = w * (1 - 0.55 * k / n)
            _cap_add(T, prev, q, wa, wb, 0.9)
            prev = q
        _cap_add(T, prev, (prev[0], prev[1] + w * 0.6), w * 1.25, w * 1.1, 1.1)


def _cap_add(T, p0, p1, r0, r1, h):
    H, W = T.shape
    rr = max(r0, r1) + 2
    x0, y0 = max(0, int(min(p0[0], p1[0]) - rr)), max(0, int(min(p0[1], p1[1]) - rr))
    x1, y1 = min(W, int(max(p0[0], p1[0]) + rr) + 1), min(H, int(max(p0[1], p1[1]) + rr) + 1)
    if x0 >= x1 or y0 >= y1:
        return
    yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    ll = dx * dx + dy * dy
    u = np.clip(((xx - p0[0]) * dx + (yy - p0[1]) * dy) / max(ll, 1e-6), 0, 1)
    d = np.sqrt((xx - p0[0] - u * dx) ** 2 + (yy - p0[1] - u * dy) ** 2)
    r = r0 + (r1 - r0) * u
    cov = np.clip(r - d + 0.5, 0, 1)
    prof = h * cov * np.sqrt(np.clip(1 - (d / np.maximum(r, 0.3)) ** 2, 0.04, 1))
    sub = T[y0:y1, x0:x1]
    np.maximum(sub, prof, out=sub)


# ---------------------------------------------------------------------------------------------------------------
# "Unleashed" written in the blood: a skeleton of the letters, walked letter by letter, timed along the walk

def thin(mask):
    """Zhang-Suen thinning of a boolean mask to one-pixel lines."""
    m = np.pad(mask.astype(np.uint8), 1)
    while True:
        changed = False
        for step in range(2):
            P2 = m[:-2, 1:-1]
            P3 = m[:-2, 2:]
            P4 = m[1:-1, 2:]
            P5 = m[2:, 2:]
            P6 = m[2:, 1:-1]
            P7 = m[2:, :-2]
            P8 = m[1:-1, :-2]
            P9 = m[:-2, :-2]
            nb = [P2, P3, P4, P5, P6, P7, P8, P9]
            B = sum(n.astype(np.int32) for n in nb)
            seq = nb + [P2]
            Acount = sum(((seq[k] == 0) & (seq[k + 1] == 1)).astype(np.int32) for k in range(8))
            c = m[1:-1, 1:-1] == 1
            if step == 0:
                cond = (P2 * P4 * P6 == 0) & (P4 * P6 * P8 == 0)
            else:
                cond = (P2 * P4 * P8 == 0) & (P2 * P6 * P8 == 0)
            rem = c & (B >= 2) & (B <= 6) & (Acount == 1) & cond
            if rem.any():
                m[1:-1, 1:-1][rem] = 0
                changed = True
        if not changed:
            break
    return m[1:-1, 1:-1].astype(bool)


def finger_times(alpha, t0, t1, S, shrink=1):
    """When each pixel of the letters is wiped clear (frame; 1e9 outside): the skeleton walked letter by letter,
    left to right, each stroke from its top end, every pixel timed by its nearest skeleton pixel."""
    H, W = alpha.shape
    mask = alpha > 0.35
    ys, xs = np.nonzero(mask)
    y0, y1, x0, x1 = ys.min() - 4, ys.max() + 5, xs.min() - 4, xs.max() + 5
    sub = mask[y0:y1, x0:x1]
    # Thin a half-size copy (quicker, cleaner), then scale the path back.
    k = max(1, int(round(2 * S)))
    small = np.asarray(Image.fromarray((sub * 255).astype(np.uint8)).resize(
        (sub.shape[1] // k, sub.shape[0] // k), Image.BILINEAR)) > 110
    sk = thin(small)
    pts = set(zip(*np.nonzero(sk)))
    # Letters: the mask's connected parts, left to right.
    lab = np.zeros(small.shape, np.int32)
    nlab = 0
    order = []
    for (py, px) in sorted(zip(*np.nonzero(small)), key=lambda p: (p[1], p[0])):
        if lab[py, px]:
            continue
        nlab += 1
        stack = [(py, px)]
        lab[py, px] = nlab
        while stack:
            a, b = stack.pop()
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = a + dy, b + dx
                    if 0 <= yy < small.shape[0] and 0 <= xx < small.shape[1] and small[yy, xx] and not lab[yy, xx]:
                        lab[yy, xx] = nlab
                        stack.append((yy, xx))
        order.append(nlab)
    # Walk each letter's skeleton depth first from its top-left end; time = distance walked (jumps cost a little).
    walk = []          # (y, x, arc length)
    arc = 0.0
    per_letter = []
    for L in order:
        lp = [p for p in pts if lab[p] == L]
        if not lp:
            continue
        lps = set(lp)

        def nbrs(p):
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    if dy or dx:
                        q = (p[0] + dy, p[1] + dx)
                        if q in lps:
                            yield q

        ends = [p for p in lp if sum(1 for _ in nbrs(p)) == 1] or lp
        start_arc = arc
        seen = set()
        while len(seen) < len(lps):
            rest = [p for p in ends if p not in seen] or [p for p in lp if p not in seen]
            s = min(rest, key=lambda p: p[0] * 0.6 + p[1])
            arc += 6.0
            stack = [(s, None)]
            last = None
            while stack:
                p, frm = stack.pop()
                if p in seen:
                    continue
                if last is not None:
                    arc += math.hypot(p[0] - last[0], p[1] - last[1])
                seen.add(p)
                walk.append((p[0], p[1], arc))
                last = p
                nb = [q for q in nbrs(p) if q not in seen]
                nb.sort(key=lambda q: -(q[0] - p[0]))      # downwards first (pushed last: popped first)
                for q in nb:
                    stack.append((q, p))
        per_letter.append((start_arc, arc))
        arc += 10.0
    total = arc
    tk = np.full(small.shape, np.inf, np.float32)
    for (py, px, a) in walk:
        tk[py, px] = t0 + (t1 - t0) * a / total
    # Spread the times over the letter (nearest skeleton pixel, by growing).
    known = np.isfinite(tk)
    for _ in range(120):
        if known[small].all():
            break
        cand = np.full_like(tk, np.inf)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            cand = np.minimum(cand, np.roll(np.roll(tk, dy, 0), dx, 1))
        upd = small & ~known & np.isfinite(cand)
        tk[upd] = cand[upd]
        known = np.isfinite(tk)
    tk[~small] = np.inf
    # Back to full size: nearest (times) inside the full mask, grown a few pixels to cover the edge.
    big = np.asarray(Image.fromarray(np.where(np.isfinite(tk), tk, 1e9).astype(np.float32)).resize(
        (sub.shape[1], sub.shape[0]), Image.NEAREST))
    big = np.where(big > 1e8, np.inf, big)
    # Fill pixels of the mask that the resize missed from neighbours.
    for _ in range(int(4 * S) + 3):
        miss = sub & ~np.isfinite(big)
        if not miss.any():
            break
        cand = np.full_like(big, np.inf)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            cand = np.minimum(cand, np.roll(np.roll(big, dy, 0), dx, 1))
        big = np.where(miss, cand, big)
    out = np.full((H, W), 1e9, np.float32)
    region = out[y0:y1, x0:x1]
    region[...] = np.where(np.isfinite(big), big, 1e9)
    # The tip of the finger at each frame: the walk's point (for the blood it pushes ahead of it).
    tip = [(x0 + px * k + k * 0.5, y0 + py * k + k * 0.5, t0 + (t1 - t0) * a / total) for (py, px, a) in walk]
    return out, tip


# ---------------------------------------------------------------------------------------------------------------
# Shading the blood

LIGHT = np.array([-0.45, -0.55, 0.70], np.float32)   # from the top left, in front (x right, y down, z to camera)
LIGHT /= np.linalg.norm(LIGHT)
HALF = LIGHT + np.array([0, 0, 1], np.float32)
HALF /= np.linalg.norm(HALF)


def shade_blood(T, S, char=0.0, heat=None, ripple=None):
    """Thickness -> premultiplied RGBA of wet blood: deep red where thick, bright where thin, a darker rim at the
    edges, lit by the key light with a sharp wet highlight. `char` darkens it (burnt), `heat` (0..1 map) adds an
    orange glow at its thin edges."""
    Tc = np.tanh(T * 1.4)                                   # thick pools flatten out (the surface tension)
    alpha = np.clip(T / 0.09, 0, 1) ** 0.85
    hgt = blur(Tc, 2.2 * S) * 16.0 * S
    if ripple is not None:                                   # the wet surface's small unevenness
        hgt += ripple * Tc * 3.0 * S
    gy, gx = np.gradient(hgt)
    nz = 1.0 / np.sqrt(gx * gx + gy * gy + 1.0)
    nx, ny = -gx * nz, -gy * nz
    ndl = np.clip(nx * LIGHT[0] + ny * LIGHT[1] + nz * LIGHT[2], 0, 1)
    ndh = np.clip(nx * HALF[0] + ny * HALF[1] + nz * HALF[2], 0, 1)
    # Colour through the film of blood (Beer-Lambert): thin is scarlet, thick nearly black-red.
    th = np.clip(T, 0, 3)
    r = 0.86 * np.exp(-1.7 * th) + 0.07
    g = 0.035 * np.exp(-3.0 * th)
    b = 0.03 * np.exp(-3.0 * th)
    # The rim: where the blood ends it is darker (dried, gathered), just inside the edge.
    edge = smoothstep(0.0, 0.25, T) * smoothstep(0.55, 0.12, blur(T, 3 * S))
    k = (0.42 + 0.75 * ndl) * (1 - 0.55 * edge) * (1 - 0.6 * char)
    spec = (ndh ** 90) * 1.6 + (ndh ** 14) * 0.14
    spec *= smoothstep(0.08, 0.3, T) * (1 - 0.7 * char)
    out = np.empty(T.shape + (4,), np.float32)
    out[..., 0] = r * k + spec * 1.0
    out[..., 1] = g * k + spec * 0.86
    out[..., 2] = b * k + spec * 0.84
    if heat is not None:
        glow = heat * (1 - 0.6 * smoothstep(0.1, 0.6, T)) * 1.2
        out[..., 0] += glow * 1.0
        out[..., 1] += glow * 0.42
        out[..., 2] += glow * 0.06
    out = np.clip(out, 0, 1)
    out[..., :3] *= alpha[..., None]
    out[..., 3] = alpha
    return out


# ---------------------------------------------------------------------------------------------------------------
# Compositing helpers (premultiplied RGBA float32)

def over(dst, src):
    """src over dst, in place."""
    a = src[..., 3:4]
    dst *= (1 - a)
    dst += src


def over_at(dst, src, x0, y0):
    """src (a small premultiplied tile) over dst at (x0, y0), clipped."""
    H, W = dst.shape[:2]
    h, w = src.shape[:2]
    ax0, ay0 = max(0, x0), max(0, y0)
    ax1, ay1 = min(W, x0 + w), min(H, y0 + h)
    if ax0 >= ax1 or ay0 >= ay1:
        return
    s = src[ay0 - y0:ay1 - y0, ax0 - x0:ax1 - x0]
    d = dst[ay0:ay1, ax0:ax1]
    d *= (1 - s[..., 3:4])
    d += s


def load_rgba(path, W):
    """A straight-alpha PNG as premultiplied float RGBA at width W."""
    im = Image.open(path).convert("RGBA")
    if im.width != W:
        im = im.resize((W, W * 9 // 16), Image.LANCZOS)
    a = np.asarray(im, np.float32) / 255.0
    a[..., :3] *= a[..., 3:4]
    return a


def capsule_rgba(dst, p0, p1, r, color, alpha, soft=1.0):
    """A motion-blurred round particle: a capsule from p0 to p1, its opacity spread over its length."""
    H, W = dst.shape[:2]
    rr = r + soft + 1
    x0, y0 = int(min(p0[0], p1[0]) - rr), int(min(p0[1], p1[1]) - rr)
    x1, y1 = int(max(p0[0], p1[0]) + rr) + 1, int(max(p0[1], p1[1]) + rr) + 1
    x0c, y0c, x1c, y1c = max(0, x0), max(0, y0), min(W, x1), min(H, y1)
    if x0c >= x1c or y0c >= y1c:
        return
    yy, xx = np.mgrid[y0c:y1c, x0c:x1c].astype(np.float32)
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    ll = dx * dx + dy * dy
    u = np.clip(((xx - p0[0]) * dx + (yy - p0[1]) * dy) / max(ll, 1e-6), 0, 1)
    d = np.sqrt((xx - p0[0] - u * dx) ** 2 + (yy - p0[1] - u * dy) ** 2)
    cov = np.clip((r - d) / soft + 0.5, 0, 1)
    smear = (2 * r) / (2 * r + math.sqrt(ll))
    a = cov * alpha * min(1.0, smear * 1.3)
    d_ = dst[y0c:y1c, x0c:x1c]
    d_ *= (1 - a[..., None])
    d_[..., 0] += a * color[0]
    d_[..., 1] += a * color[1]
    d_[..., 2] += a * color[2]
    d_[..., 3] += a


def glow_rgba(dst, p, r, color, alpha):
    """A soft additive-looking glow (a Gaussian dot)."""
    H, W = dst.shape[:2]
    rr = r * 3
    x0, y0 = max(0, int(p[0] - rr)), max(0, int(p[1] - rr))
    x1, y1 = min(W, int(p[0] + rr) + 1), min(H, int(p[1] + rr) + 1)
    if x0 >= x1 or y0 >= y1:
        return
    yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
    a = alpha * np.exp(-((xx - p[0]) ** 2 + (yy - p[1]) ** 2) / (r * r))
    d_ = dst[y0:y1, x0:x1]
    d_ *= (1 - a[..., None])
    d_[..., 0] += a * color[0]
    d_[..., 1] += a * color[1]
    d_[..., 2] += a * color[2]
    d_[..., 3] += a


# ---------------------------------------------------------------------------------------------------------------
# Particles (deterministic: a particle's place is a function of the frame)

def burst_particles(seed=3):
    """The blood thrown at the burst: 3D droplets from the chest, many at the camera."""
    rng = np.random.default_rng(seed)
    n = 700
    d = rng.normal(0, 1, (n, 3))
    d[:, 1] = -np.abs(d[:, 1]) * 1.3                          # towards the camera
    d[:, 0] *= 1.4
    d /= np.linalg.norm(d, axis=1, keepdims=True)
    sp = rng.uniform(2.5, 13.0, n) * (rng.random(n) ** 0.5)
    v = d * sp[:, None]
    v[:, 2] += rng.uniform(0.5, 3.0, n)
    p = np.array(C.CHEST)[None, :] + rng.normal(0, 0.08, (n, 3))
    size = rng.uniform(0.004, 0.022, n) * (1 + 2.5 * (rng.random(n) < 0.06))
    t0 = C.HIT + rng.uniform(0, 3, n)
    return dict(p=p, v=v, size=size, t0=t0, life=rng.uniform(0.5, 1.4, n))


def impact_particles(rests, W, seed=5):
    """Each landing's blood thrown off the wall, its dust and its chips (3D: starting on the wall)."""
    rng = np.random.default_rng(seed)
    ppm = C.px_per_m(W)
    blood, dust, chips, crown = [], [], [], []
    for i, alpha in enumerate(rests):
        land = C.LAND[i]
        box, edge = footprint_info(alpha)
        heavy = 1.4 if i >= 5 else 1.0
        for _ in range(int(260 * heavy)):
            ex, ey, nx, ny = edge[rng.integers(len(edge))]
            wx, wz = (ex - W / 2) / ppm, (W * 9 / 32 - ey) / ppm
            out = rng.uniform(1.0, 7.0)
            v = (nx * out + rng.normal(0, 0.6), -rng.uniform(0.3, 2.5), -ny * out + rng.uniform(0.5, 3.0))
            blood.append((wx, -0.03, wz, v[0], v[1], v[2], rng.uniform(0.008, 0.028) * (rng.random() ** 2 * 0.8 + 0.4), land + rng.uniform(0, 2),
                          rng.uniform(0.4, 1.0)))
        # The crown: a ring of fast streaks squirted out round the letter as it hits (fresh, bright blood).
        for _ in range(int(150 * heavy)):
            ex, ey, nx, ny = edge[rng.integers(len(edge))]
            wx, wz = (ex - W / 2) / ppm, (W * 9 / 32 - ey) / ppm
            sp = rng.uniform(4.0, 11.0) * heavy
            v = (nx * sp, -rng.uniform(0.5, 3.5), -ny * sp + rng.uniform(0.5, 2.5))
            crown.append((wx, -0.03, wz, v[0], v[1], v[2], rng.uniform(0.01, 0.032), land + rng.uniform(0, 1.5),
                          rng.uniform(0.15, 0.4)))
        # Dust: along the bottom and the sides of the letter, rolling outwards and up.
        x0, y0, x1, y1 = box
        for _ in range(int(20 * heavy)):
            side = rng.random()
            if side < 0.6:
                ex, ey = rng.uniform(x0, x1), y1 - rng.uniform(0, 20)
            else:
                ex, ey = (x0 if side < 0.8 else x1), rng.uniform(y0, y1)
            wx, wz = (ex - W / 2) / ppm, (W * 9 / 32 - ey) / ppm
            cxw = ((x0 + x1) / 2 - W / 2) / ppm
            dirx = np.sign(wx - cxw) * rng.uniform(0.2, 1.0)
            dust.append((wx, -0.05, wz, dirx * rng.uniform(0.3, 1.2), -rng.uniform(0.1, 0.8), rng.uniform(-0.1, 0.5),
                         rng.uniform(0.08, 0.17) * heavy, land, rng.uniform(0.55, 1.05), int(rng.integers(8))))
        for _ in range(int(16 * heavy)):
            ex, ey, nx, ny = edge[rng.integers(len(edge))]
            wx, wz = (ex - W / 2) / ppm, (W * 9 / 32 - ey) / ppm
            v = (nx * rng.uniform(1, 4), -rng.uniform(1, 5), -ny * rng.uniform(1, 3) + rng.uniform(1, 3))
            chips.append((wx, -0.03, wz, v[0], v[1], v[2], rng.uniform(0.006, 0.02), land, rng.uniform(0.4, 0.9)))
    return (np.array(blood, np.float64), np.array(dust, np.float64), np.array(chips, np.float64),
            np.array(crown, np.float64))


def ballistic(p, v, t):
    return p + v * t + np.array([0, 0, -0.5 * C.GRAVITY]) * t * t


def draw_droplets(dst, P, V, size, t0, life, frame, W, color=(0.33, 0.01, 0.01), spec=True, shutter=0.5):
    """3D drops at `frame`: projected, motion-blurred over the shutter, shrinking near their life's end."""
    ppm = C.px_per_m(W)
    for k in range(len(P)):
        t = (frame - t0[k]) / C.FPS
        if t < 0 or t > life[k]:
            continue
        ta = max(0.0, t - shutter / C.FPS)
        a = ballistic(P[k], V[k], ta)
        b = ballistic(P[k], V[k], t)
        pa, pb = C.project(a, W), C.project(b, W)
        if pa is None or pb is None or pb[2] < 0.6:
            continue
        r = size[k] * C.CAM_DIST / pb[2] * ppm
        if r < 0.4:
            r = 0.4
        if r > 140 * W / 3840:
            continue
        fade = min(1.0, (life[k] - t) / 0.15)
        if not (-200 < pb[0] < W + 200 and -200 < pb[1] < W * 9 / 16 + 200):
            continue
        capsule_rgba(dst, (pa[0], pa[1]), (pb[0], pb[1]), r, color, 0.95 * fade, soft=max(1.0, r * 0.15))
        if spec and r > 2.5:
            o = r * 0.35
            capsule_rgba(dst, (pa[0] - o, pa[1] - o), (pb[0] - o, pb[1] - o), r * 0.28, (1.0, 0.85, 0.82),
                         0.55 * fade, soft=1.0)


def puff_sprites(n=8, size=256, seed=21):
    """Soft dust puffs: fbm clouds in a round falloff (alpha), shaded lighter on top (value)."""
    out = []
    for k in range(n):
        nz = noise(size, size, size / 4, seed + k, 5)
        yy, xx = np.mgrid[0:size, 0:size].astype(np.float32) / size - 0.5
        r = np.sqrt(xx * xx + yy * yy) * 2
        a = np.clip(1 - r, 0, 1) ** 1.5 * smoothstep(0.3, 0.75, nz + 0.25 * (1 - r))
        shade = 0.75 + 0.35 * (0.5 - yy) + 0.2 * (nz - 0.5)
        out.append((a.astype(np.float32), shade.astype(np.float32)))
    return out


def draw_dust(dst, dust, sprites, frame, W, base=(0.5, 0.46, 0.42)):
    ppm = C.px_per_m(W)
    for d in dust:
        x, y, z, vx, vy, vz, rad, t0, life, kind = d
        t = (frame - t0) / C.FPS
        if t < 0 or t > life:
            continue
        drag = (1 - math.exp(-3.0 * t)) / 3.0                # slows down
        p = (x + vx * drag, y + vy * drag, z + vz * drag + 0.08 * t)
        pp = C.project(p, W)
        if pp is None:
            continue
        grow = rad * (0.45 + 2.0 * (1 - math.exp(-3.0 * t)))
        r = grow * C.CAM_DIST / pp[2] * ppm
        a_env = min(1.0, t / 0.05) * (1 - t / life) ** 1.8 * 0.5
        n = max(4, int(r * 2))
        a_sp, s_sp = sprites[int(kind) % len(sprites)]
        rot = (kind * 47 + t * 25) % 360
        A = np.asarray(Image.fromarray(a_sp).rotate(rot, Image.BILINEAR).resize((n, n), Image.BILINEAR))
        Sh = np.asarray(Image.fromarray(s_sp).rotate(rot, Image.BILINEAR).resize((n, n), Image.BILINEAR))
        tile = np.empty((n, n, 4), np.float32)
        a = A * a_env
        tile[..., 0] = a * base[0] * Sh
        tile[..., 1] = a * base[1] * Sh
        tile[..., 2] = a * base[2] * Sh
        tile[..., 3] = a
        over_at(dst, tile, int(pp[0] - n / 2), int(pp[1] - n / 2))


# ---------------------------------------------------------------------------------------------------------------
# Fire: flames rising from the logo, coloured by the installer's fire ramp (Installer/.../Skin/Procedural.cs), with
# its embers

FIRE_STOPS = [(0.00, 0x000000, 0.0), (0.10, 0x3C0802, 0.0), (0.25, 0x8E1A04, 0.55), (0.45, 0xD24A0C, 0.9),
              (0.70, 0xF5901E, 1.0), (0.86, 0xFFC850, 1.0), (1.00, 0xFFF2C0, 1.0)]


def fire_ramp():
    """256 x (r, g, b, a) straight colours, the installer's FireRamp stops."""
    out = np.zeros((256, 4), np.float32)
    for i in range(256):
        t = i / 255
        k = 0
        while k < len(FIRE_STOPS) - 2 and t > FIRE_STOPS[k + 1][0]:
            k += 1
        a0, c0, al0 = FIRE_STOPS[k]
        a1, c1, al1 = FIRE_STOPS[k + 1]
        f = (t - a0) / (a1 - a0)
        ca = np.array([(c0 >> 16) & 255, (c0 >> 8) & 255, c0 & 255], np.float32) / 255
        cb = np.array([(c1 >> 16) & 255, (c1 >> 8) & 255, c1 & 255], np.float32) / 255
        out[i, :3] = ca + (cb - ca) * f
        out[i, 3] = al0 + (al1 - al0) * f
    return out


class Fire:
    """Procedural flames on a fixed grid (GW x GH whatever the frame size), a function of the frame (no state, so
    frames render in parallel): the fuel's heat carried upwards and fading (a flame's length), its coordinates
    warped by noise rising with time (licking tongues), broken by a second rising noise (the tips tear off). The
    fuel: the logo's top edges (a steady burn) and, at the start, the whole splash (the burst)."""
    GW, GH = 960, 540

    def __init__(self, fuel_edge, fuel_area, ignite):
        self.edge = fuel_edge.astype(np.float32)
        self.area = fuel_area.astype(np.float32)
        self.ignite = ignite.astype(np.float32)
        GW, GH = self.GW, self.GH
        # Tall noises (their cells stretched upwards), scrolled up a little every frame.
        self.n1 = resize_f(noise(GH * 2, GW, 26, 101, 4), GW, GH * 4)
        self.n2 = resize_f(noise(GH * 2, GW, 12, 102, 4), GW, GH * 4)
        self.wx = resize_f(noise(GH * 2, GW, 40, 103, 3), GW, GH * 4)
        self.clump = resize_f(noise(GH, GW, 30, 104, 3), GW, GH * 2)

    @staticmethod
    def intensity(frame):
        if frame < C.FIRE_START:
            return 0.0
        t = (frame - C.FIRE_START) / C.FPS
        return 1.0 + 0.5 * math.exp(-((t - 0.22) / 0.28) ** 2) - 0.35 * smooth01((t - 0.5) / 1.0)

    @staticmethod
    def flare(frame):
        t = (frame - C.FIRE_START) / C.FPS
        return 0.04 + 0.45 * math.exp(-((t - 0.12) / 0.16) ** 2)

    def _scroll(self, tex, off):
        n = tex.shape[0]
        idx = (np.arange(self.GH) + int(off)) % n
        return tex[idx]

    def heat(self, frame):
        GW, GH = self.GW, self.GH
        if frame < C.FIRE_START:
            return None
        inten = self.intensity(frame)
        on = smoothstep(0.0, 9.0, frame - self.ignite)
        f = frame - C.FIRE_START
        clump = self._scroll(self.clump, -f * 0.6)
        fuel = (self.edge * (0.55 + 0.7 * clump) + self.area * self.flare(frame) * smoothstep(0.35, 0.7, clump))
        fuel *= on * inten
        # Carried up: each row keeps the fuel's heat below it, fading over a flame's length.
        L = 46.0 * inten
        k = math.exp(-1.0 / L)
        h = np.empty_like(fuel)
        h[GH - 1] = fuel[GH - 1]
        for y in range(GH - 2, -1, -1):
            np.maximum(fuel[y], h[y + 1] * k, out=h[y])
        # Licking: sample it through a sideways warp that rises with time.
        wx = (self._scroll(self.wx, f * 2.2) - 0.5) * 34
        wy = (self._scroll(self.n1, f * 1.7) - 0.5) * 18
        xs = np.clip(np.arange(GW)[None, :] + wx, 0, GW - 1.001)
        ys = np.clip(np.arange(GH)[:, None] + wy + 6, 0, GH - 1.001)
        x0, y0 = xs.astype(np.int32), ys.astype(np.int32)
        fx_, fy_ = xs - x0, ys - y0
        hw = (h[y0, x0] * (1 - fx_) + h[y0, x0 + 1] * fx_) * (1 - fy_) +              (h[y0 + 1, x0] * (1 - fx_) + h[y0 + 1, x0 + 1] * fx_) * fy_
        # Torn into tongues by a fast rising noise.
        n = self._scroll(self.n2, f * 3.4)
        n2 = self._scroll(self.n1, f * 2.3)
        fl = hw * np.clip(n * 1.5 + n2 * 0.6 - 0.62, 0, None) * 1.55
        return blur(np.clip(fl, 0, 1.3), 0.7)


def smooth01(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3 - 2 * x)


def fire_rgba(heat_grid, W, H, ramp, gain=1.0, up=None):
    """The heat grid as premultiplied RGBA at W x H."""
    if up is None:
        up = resize_f(heat_grid, W, H, Image.BICUBIC)
    idx = np.clip(up * gain * 255, 0, 255).astype(np.int32)
    c = ramp[idx]
    out = np.empty((H, W, 4), np.float32)
    out[..., :3] = c[..., :3] * c[..., 3:4]
    out[..., 3] = c[..., 3]
    return out


def embers(fuel_mask, W, H, seed=9):
    rng = np.random.default_rng(seed)
    ys, xs = np.nonzero(fuel_mask > 0.5)
    n = 520
    k = rng.integers(0, len(xs), n)
    S = W / 3840.0
    return dict(x=xs[k].astype(np.float32), y=ys[k].astype(np.float32),
                t0=(C.FIRE_START + rng.exponential(30, n) * (rng.random(n) < 0.6) + rng.uniform(0, C.FRAMES - C.FIRE_START, n) * (rng.random(n) >= 0.6)).astype(np.float32),
                vy=rng.uniform(4, 13, n) * S, vx=rng.normal(0, 2.5, n) * S, life=rng.uniform(40, 130, n),
                size=rng.uniform(1.6, 4.5, n) * S, ph=rng.uniform(0, 6.28, n), kind=rng.integers(0, 3, n))


EMBER_COL = [((1.0, 0.91, 0.72), (1.0, 0.63, 0.25)), ((1.0, 0.82, 0.5), (1.0, 0.42, 0.1)),
             ((1.0, 0.96, 0.85), (1.0, 0.69, 0.25))]


def draw_embers(dst, E, frame, burst):
    for k in range(len(E["x"])):
        t = frame - E["t0"][k]
        if t < 0 or t > E["life"][k]:
            continue
        def pos(tt):
            return (E["x"][k] + E["vx"][k] * tt + math.sin(E["ph"][k] + tt * 0.09) * 30 * (tt / 60) * (E["size"][k]),
                    E["y"][k] - E["vy"][k] * tt - burst * 3 * tt)
        p1 = pos(t)
        p0 = pos(max(0, t - 1.5))
        fade = math.sin(math.pi * min(1.0, t / E["life"][k])) ** 0.6
        flick = 0.75 + 0.25 * math.sin(t * 0.7 + E["ph"][k] * 5)
        core, edge = EMBER_COL[int(E["kind"][k])]
        glow_rgba(dst, p1, E["size"][k] * 3.0, edge, 0.35 * fade * flick)
        capsule_rgba(dst, p0, p1, E["size"][k], core, 0.95 * fade * flick, soft=1.0)
