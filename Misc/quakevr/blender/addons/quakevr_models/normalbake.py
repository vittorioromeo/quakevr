# normalbake.py -- bakes Quake VR's own models' normal maps (Misc/quakevr/bake_normals.py, and the add-ons' "Bake
# Normal Map" buttons; docs/vr-port/ROUND21.md, "Baked normal maps"). numpy only: no bpy, so it runs in Blender and
# out of it.
#
# The engine (gl_shaders.h, BumpedNormalK) has no vertex tangents: each pixel's frame comes from the screen
# derivatives of the position and the texture coordinates, the tangent t along which u grows and the bitangent b
# along which v grows (v down the image), both in the plane of the interpolated normal n, each of unit length; the
# map's x tilts along t, its y (green) up the image (-b), z along n. Here the same frame is built per texel of every
# triangle from its corners (what the derivatives give, exactly, on a flat triangle), so a normal the bake wants a
# texel to have comes out in the game as it went in, on mirrored and flipped islands alike.
#
# The models reuse their skins' texels (every face of a generated model's material maps to the same tile; the id
# weapons' sides share texels; the body's left and right limbs share theirs), so a texel's normal must suit every
# triangle drawing it: each covering triangle's normal is found, and where they disagree the texel keeps only what
# they agree on (as mdlpolish.edge_wear does with the skins' wear).

import math
import os
import struct

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ADDONS = os.path.dirname(HERE)


def _mdl():
    try:
        from . import mdl
    except ImportError:
        import mdl
    return mdl


def _qpal():
    try:
        from . import qpal
    except ImportError:
        import qpal
    return qpal


def _md5hand():
    import sys
    try:
        import bpy  # noqa: F401  (in Blender: the hand's add-on, as a package)
        if ADDONS not in sys.path:
            sys.path.append(ADDONS)
        from quakevr_hand import md5hand
    except ImportError:
        d = os.path.join(ADDONS, "quakevr_hand")
        if d not in sys.path:
            sys.path.append(d)
        import md5hand
    return md5hand


# ----------------------------------------------------------------------------
# Low-poly meshes as the engine draws them


class Low:
    """A model as the engine draws its skin: vertices P (model space) with the engine's normals N and texel
    coordinates UV (skin texels: x right, y down the image, (0, 0) the top left corner), triangles T (vertex indices),
    the skin's size (W, H)."""

    def __init__(self, name, W, H, P, N, UV, T, extra=None):
        self.name = name
        self.W, self.H = W, H
        self.P = np.asarray(P, np.float64)
        self.N = np.asarray(N, np.float64)
        self.N /= np.maximum(np.linalg.norm(self.N, axis=1, keepdims=True), 1e-12)
        self.UV = np.asarray(UV, np.float64)
        self.T = np.asarray(T, np.int64).reshape(-1, 3)
        self.extra = extra or {}
        a, b, c = (self.P[self.T[:, k]] for k in range(3))
        ng = np.cross(b - a, c - a)
        area = np.linalg.norm(ng, axis=1)
        ng = ng / np.maximum(area, 1e-12)[:, None]
        # outward: the side the vertex normals are on
        nsum = self.N[self.T].sum(axis=1)
        flip = np.einsum("ij,ij->i", ng, nsum) < 0
        ng[flip] *= -1
        self.ng = ng
        self.area = area * 0.5
        # texel-space derivatives of the position: P = Pa + Pu (u - ua) + Pv (v - va) on each triangle
        ua, ub, uc = (self.UV[self.T[:, k]] for k in range(3))
        e1, e2 = b - a, c - a
        d1, d2 = ub - ua, uc - ua
        det = d1[:, 0] * d2[:, 1] - d1[:, 1] * d2[:, 0]
        self.uvdet = det
        ok = np.abs(det) > 1e-12
        inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
        self.Pu = (e1 * d2[:, 1:2] - e2 * d1[:, 1:2]) * inv[:, None]
        self.Pv = (e2 * d1[:, 0:1] - e1 * d2[:, 0:1]) * inv[:, None]
        self.uv_ok = ok
        # the frame's orientation: +1 where (u, v) turn as the surface does seen from outside, -1 mirrored
        self.sign = np.sign(np.einsum("ij,ij->i", np.cross(self.Pu, self.Pv), ng))
        self.sign[self.sign == 0] = 1.0


def mdl_low(path, frame=0):
    """An .mdl (frame `frame`'s pose) as the engine draws it: its file vertices with their normals (anorms), the
    back-facing triangles' onseam vertices half a skin on."""
    mdlmod = _mdl()
    with open(path, "rb") as f:
        model = mdlmod.Model(f.read(), os.path.basename(path))
    table = np.array(mdlmod.anorms())
    pose = model.pose_bytes()[frame]
    W, H = model.skin_size
    key, P, N, UV, T = {}, [], [], [], []
    for ff, a, b, c in model.tris:
        tri = []
        for v in (a, b, c):
            onseam, s, t = model.st[v]
            shift = bool(onseam and not ff)
            k = (v, shift)
            if k not in key:
                key[k] = len(P)
                x = pose[v]
                P.append(model.place(x[:3]))
                N.append(table[x[3]] if x[3] < len(table) else (0.0, 0.0, 1.0))
                UV.append((s + (W // 2 if shift else 0) + 0.5, t + 0.5))
            tri.append(key[k])
        T.append(tri)
    low = Low(os.path.basename(path), W, H, P, N, UV, T)
    low.model = model
    return low


def md5_normals(P, T):
    """MD5_ComputeNormals (gl_model.c): area-weighted face normals summed over vertices welded by position."""
    keys = {}
    weld = np.empty(len(P), np.int64)
    for i, p in enumerate(map(tuple, np.asarray(P, np.float32))):
        weld[i] = keys.setdefault(p, i)
    Pw = np.asarray(P, np.float64)
    i0, i1, i2 = weld[T[:, 0]], weld[T[:, 1]], weld[T[:, 2]]
    norm = np.cross(Pw[i2] - Pw[i0], Pw[i1] - Pw[i0])
    acc = np.zeros((len(P), 3))
    for i in (i0, i1, i2):
        np.add.at(acc, i, norm)
    return acc[weld]


def md5_low(path, skin_size):
    """An .md5mesh in its bind pose, as the engine draws it (normals as MD5_ComputeNormals makes them)."""
    md5hand = _md5hand()
    with open(path) as f:
        mesh = md5hand.parse_md5mesh(f.read(), os.path.basename(path))
    joints = mesh["joints"]
    P, UV, W8 = [], [], []
    W, H = skin_size
    for s, t, first, count in mesh["verts"]:
        ws = mesh["weights"][first:first + count]
        P.append(md5hand.rest_position(joints, ws))
        UV.append((s * W, t * H))
        W8.append([(j, b) for j, b, _ in ws])
    T = np.array(mesh["tris"], np.int64)
    N = md5_normals(P, T)
    low = Low(os.path.basename(path), W, H, P, N, UV, T, extra={"weights": W8})
    low.joints = joints
    low.md5 = mesh
    return low


# ----------------------------------------------------------------------------
# Rasterising in texel space


class Raster:
    """Every pixel centre of a map `scale` times the skin's size that each triangle covers: pixel index, triangle,
    barycentric weights (a pixel under several triangles appears once for each)."""

    def __init__(self, low, scale, grow=0.0):
        self.low = low
        self.scale = scale
        self.w, self.h = low.W * scale, low.H * scale
        pix, tri, bary = [], [], []
        uv = low.UV * scale
        for ti, (a, b, c) in enumerate(low.T):
            if not low.uv_ok[ti]:
                continue
            (x0, y0), (x1, y1), (x2, y2) = uv[a], uv[b], uv[c]
            d = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
            X0 = max(int(math.floor(min(x0, x1, x2) - 0.5 - grow)), 0)
            X1 = min(int(math.ceil(max(x0, x1, x2) + 0.5 + grow)), self.w)
            Y0 = max(int(math.floor(min(y0, y1, y2) - 0.5 - grow)), 0)
            Y1 = min(int(math.ceil(max(y0, y1, y2) + 0.5 + grow)), self.h)
            if X1 <= X0 or Y1 <= Y0:
                continue
            ys, xs = np.mgrid[Y0:Y1, X0:X1]
            px, py = xs.ravel() + 0.5, ys.ravel() + 0.5
            l0 = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) / d
            l1 = ((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) / d
            l2 = 1.0 - l0 - l1
            if grow > 0.0:
                # pixels within `grow` of the triangle (in pixels): their barycentrics clamped onto it
                inside = self._near(px, py, (x0, y0), (x1, y1), (x2, y2), grow, l0, l1, l2)
            else:
                inside = (l0 >= -1e-9) & (l1 >= -1e-9) & (l2 >= -1e-9)
            if not inside.any():
                continue
            L = np.stack([l0[inside], l1[inside], l2[inside]], 1)
            if grow > 0.0:
                L = np.clip(L, 0.0, None)
                L /= L.sum(1, keepdims=True)
            pix.append((ys.ravel() * self.w + xs.ravel())[inside])
            tri.append(np.full(inside.sum(), ti))
            bary.append(L)
        self.pix = np.concatenate(pix) if pix else np.zeros(0, np.int64)
        self.tri = np.concatenate(tri) if tri else np.zeros(0, np.int64)
        self.bary = np.concatenate(bary) if bary else np.zeros((0, 3))
        # the pixel's place on its triangle: model space, the interpolated (engine) normal, texel coordinates
        T = low.T[self.tri]
        self.pos = np.einsum("ik,ikj->ij", self.bary, low.P[T])
        n = np.einsum("ik,ikj->ij", self.bary, low.N[T])
        self.n = n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
        self.uv = np.einsum("ik,ikj->ij", self.bary, low.UV[T])
        self.frame()

    @staticmethod
    def _near(px, py, p0, p1, p2, grow, l0, l1, l2):
        inside = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
        d = np.full(px.shape, np.inf)
        for (ax, ay), (bx, by) in ((p0, p1), (p1, p2), (p2, p0)):
            ex, ey = bx - ax, by - ay
            L2 = ex * ex + ey * ey
            t = np.clip(((px - ax) * ex + (py - ay) * ey) / max(L2, 1e-12), 0, 1)
            d = np.minimum(d, np.hypot(px - ax - t * ex, py - ay - t * ey))
        return inside | (d <= grow)

    def frame(self):
        """The engine's frame at each pixel (BumpedNormalK, each axis of unit length): t along which u grows, b along
        which v grows (down the image), both perpendicular to the interpolated normal."""
        low, n = self.low, self.n
        s = low.sign[self.tri][:, None]
        t = np.cross(low.Pv[self.tri], n) * s
        b = np.cross(n, low.Pu[self.tri]) * s
        self.t = t / np.maximum(np.linalg.norm(t, axis=1, keepdims=True), 1e-12)
        self.b = b / np.maximum(np.linalg.norm(b, axis=1, keepdims=True), 1e-12)

    def encode(self, nb):
        """World (model-space) normals nb, one per entry, as the map's (x, y, z): what the engine turns back into nb."""
        t, b, n = self.t, -self.b, self.n  # y is up the image: -b
        d = np.einsum("ij,ij->i", nb, n)
        x = nb - d[:, None] * n
        # x = a t + c b' (t, b' in the tangent plane, not always at right angles): the 2x2 Gram system
        tt = np.einsum("ij,ij->i", t, t)
        bb = np.einsum("ij,ij->i", b, b)
        tb = np.einsum("ij,ij->i", t, b)
        xt = np.einsum("ij,ij->i", x, t)
        xb = np.einsum("ij,ij->i", x, b)
        det = tt * bb - tb * tb
        det = np.where(np.abs(det) < 1e-9, 1e-9, det)
        a = (xt * bb - xb * tb) / det
        c = (xb * tt - xt * tb) / det
        d = np.maximum(d, 0.05)
        k = 1.0 / np.sqrt(a * a + c * c + d * d)
        return np.stack([a * k, c * k, d * k], 1)

    def decode(self, m):
        """What the engine makes of map values m (x, y, z) at each entry: the world normal."""
        z = np.sqrt(np.maximum(1.0 - m[:, 0] ** 2 - m[:, 1] ** 2, 0.0025))
        v = self.t * m[:, 0:1] - self.b * m[:, 1:2] + self.n * z[:, None]
        return v / np.linalg.norm(v, axis=1, keepdims=True)


# ----------------------------------------------------------------------------
# Tangent-space helpers


def normalize(v):
    return v / np.maximum(np.linalg.norm(v, axis=-1, keepdims=True), 1e-12)


def rnm(base, detail):
    """Reoriented normal mapping: `detail` (tangent space) laid on `base` (tangent space); both (..., 3), z up."""
    t = base + np.array([0.0, 0.0, 1.0])
    u = detail * np.array([-1.0, -1.0, 1.0])
    return normalize(t * np.sum(t * u, -1, keepdims=True) / np.maximum(t[..., 2:3], 1e-6) - u)


def height_to_tangent(h, strength=1.0, wrap=False, mask=None):
    """A height field h (pixels of the map, in pixel units: 1 = one pixel up) as tangent-space normals (x right, y up
    the image). `mask`: where h is defined; differences reaching outside it are one-sided."""
    if wrap:
        dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
        dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    else:
        hp = np.pad(h, 1, mode="edge")
        dx = (hp[1:-1, 2:] - hp[1:-1, :-2]) * 0.5
        dy = (hp[2:, 1:-1] - hp[:-2, 1:-1]) * 0.5
    # y up the image: rows go down
    return normalize(np.stack([-dx * strength, dy * strength, np.ones_like(h)], -1))


# ----------------------------------------------------------------------------
# The map: resolving the covering triangles, margins, writing


def resolve(raster, tangent, weight=None, agree_deg=6.0, spread_deg=14.0):
    """One tangent-space normal per pixel from each covering triangle's (entries of `raster`). Where the triangles
    agree (within agree_deg of their mean) their mean; where they differ it fades to what they share: the mean pulled
    towards flat as far as the spread goes (at spread_deg and beyond, flat). Returns (h, w, 3) and the coverage mask."""
    npx = raster.w * raster.h
    wgt = np.ones(len(raster.pix)) if weight is None else weight.copy()
    # A texel's owners: the triangles drawing it at about the least magnification (texels per unit of surface). A
    # face that squeezes a whole tile onto a sliver (a cap's fan over a limb's block, a screw's side over the casing's
    # tile) borrows those texels; it can't show their detail anyway, and it doesn't get a say in it.
    low = raster.low
    dens = np.abs(low.uvdet) / np.maximum(2.0 * low.area, 1e-12)
    d = dens[raster.tri]
    least = np.full(npx, np.inf)
    np.minimum.at(least, raster.pix, d)
    wgt *= d <= least[raster.pix] * 2.0
    acc = np.zeros((npx, 3))
    cnt = np.zeros(npx)
    np.add.at(acc, raster.pix, tangent * wgt[:, None])
    np.add.at(cnt, raster.pix, wgt)
    covered = cnt > 0
    mean = normalize(acc / np.maximum(cnt, 1e-12)[:, None])
    # the largest angle between a covering triangle's normal and the mean
    cosang = np.where(wgt > 0, np.einsum("ij,ij->i", tangent, mean[raster.pix]), 1.0)
    worst = np.ones(npx)
    np.minimum.at(worst, raster.pix, cosang)
    ang = np.degrees(np.arccos(np.clip(worst, -1, 1)))
    keep = np.clip(1.0 - (ang - agree_deg) / max(spread_deg - agree_deg, 1e-6), 0.0, 1.0)
    flat = np.array([0.0, 0.0, 1.0])
    out = normalize(mean * keep[:, None] + flat * (1 - keep[:, None]))
    out[~covered] = flat
    return out.reshape(raster.h, raster.w, 3), covered.reshape(raster.h, raster.w), keep.reshape(raster.h, raster.w)


def dilate(img, mask, margin):
    """Pixels outside `mask` within `margin` pixels take the mean of their covered neighbours, ring by ring."""
    img = img.copy()
    m = mask.copy()
    for _ in range(margin):
        acc = np.zeros_like(img)
        cnt = np.zeros(m.shape)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dx == 0 and dy == 0:
                    continue
                sm = np.roll(np.roll(m, dy, 0), dx, 1)
                si = np.roll(np.roll(img, dy, 0), dx, 1)
                acc += si * sm[..., None]
                cnt += sm
        grow = (~m) & (cnt > 0)
        if not grow.any():
            break
        img[grow] = acc[grow] / cnt[grow][:, None]
        m = m | grow
    return img, m


def downsample(tn, factor):
    """Box-filtered tangent normals (h, w, 3) to 1/factor the size, renormalised."""
    if factor == 1:
        return tn
    h, w, _ = tn.shape
    v = tn.reshape(h // factor, factor, w // factor, factor, 3).mean((1, 3))
    return normalize(v)


def to_rgb8(tn):
    """Tangent normals as 8-bit RGB (x * 0.5 + 0.5 ...), z kept a real z."""
    return np.clip(np.round((tn * 0.5 + 0.5) * 255.0), 0, 255).astype(np.uint8)


def write_png(path, rgb):
    """An 8-bit RGB PNG (no gamma chunk: the values are linear), rows top to bottom."""
    import zlib
    h, w, c = rgb.shape
    raw = b"".join(b"\x00" + rgb[y].tobytes() for y in range(h))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)

    color = {3: 2, 4: 6}[c]
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, color, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def read_png(path):
    """An 8-bit RGB or RGBA PNG (non-interlaced) as (h, w, c) uint8."""
    import zlib
    with open(path, "rb") as f:
        data = f.read()
    pos, idat, w = 8, b"", None
    while pos < len(data):
        n, tag = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if tag == b"IHDR":
            w, h, depth, color = struct.unpack(">IIBB", body[:10])
            c = {2: 3, 6: 4, 0: 1}[color]
        elif tag == b"IDAT":
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    stride = w * c
    out = np.zeros((h, stride), np.uint8)
    prev = np.zeros(stride, np.int32)
    for y in range(h):
        ft = raw[y * (stride + 1)]
        line = np.frombuffer(raw, np.uint8, stride, y * (stride + 1) + 1).astype(np.int32)
        if ft == 0:
            cur = line
        elif ft == 2:
            cur = (line + prev) & 255
        else:
            cur = np.zeros(stride, np.int32)
            for x in range(stride):
                a = cur[x - c] if x >= c else 0
                b = prev[x]
                cc = prev[x - c] if x >= c else 0
                if ft == 1:
                    p = a
                elif ft == 3:
                    p = (a + b) // 2
                else:
                    pa, pb, pc = abs(b - cc), abs(a - cc), abs(a + b - 2 * cc)
                    p = a if pa <= pb and pa <= pc else (b if pb <= pc else cc)
                cur[x] = (line[x] + p) & 255
        out[y] = cur
        prev = cur
    return out.reshape(h, w, c)


# ----------------------------------------------------------------------------
# Heights to normals


def grad_uv_frame(raster):
    """Per entry, the surface gradients of the texel coordinates u and v (model units: texels per unit), in the
    triangle's plane."""
    low = raster.low
    Pu, Pv, ng = low.Pu[raster.tri], low.Pv[raster.tri], low.ng[raster.tri]
    det = np.einsum("ij,ij->i", np.cross(Pu, Pv), ng)
    det = np.where(np.abs(det) < 1e-12, 1e-12, det)[:, None]
    return np.cross(Pv, ng) / det, np.cross(ng, Pu) / det


def bend(n, grad):
    """The normal n of a surface raised by a height whose surface gradient is grad (both per entry)."""
    g = grad - np.einsum("ij,ij->i", grad, n)[:, None] * n
    return normalize(n - g)


def gradient_3d(raster, field, eps, ctx=None):
    """The surface gradient of field(positions, ctx) -> heights (model units) at each entry, by central differences
    along two directions in the surface, eps apart (about half a pixel)."""
    n = raster.n
    e1 = raster.t
    e2 = normalize(np.cross(n, e1))
    p = raster.pos
    e = eps[:, None] if np.ndim(eps) else eps
    h1 = field(p + e * e1, ctx) - field(p - e * e1, ctx)
    h2 = field(p + e * e2, ctx) - field(p - e * e2, ctx)
    den = np.maximum(2.0 * (eps if np.ndim(eps) else np.full(len(p), eps)), 1e-9)
    return (h1 / den)[:, None] * e1 + (h2 / den)[:, None] * e2


def gradient_uv(raster, field, ctx=None, d=0.25):
    """The surface gradient of field(texel coordinates, ctx) -> heights (model units) at each entry: its texel-space
    differences (d texels apart) through the triangle's mapping."""
    uv = raster.uv
    du = (field(uv + [d, 0.0], ctx) - field(uv - [d, 0.0], ctx)) / (2 * d)
    dv = (field(uv + [0.0, d], ctx) - field(uv - [0.0, d], ctx)) / (2 * d)
    gu, gv = grad_uv_frame(raster)
    return du[:, None] * gu + dv[:, None] * gv


def tangent_uv(raster, field, ctx=None, d=0.25):
    """A height field in texel units (field(texel coordinates) -> heights, 1 = a texel) straight as tangent-space
    normals: the same slope on every face that maps there, whatever its size (the generated models' tiles)."""
    uv = raster.uv
    du = (field(uv + [d, 0.0], ctx) - field(uv - [d, 0.0], ctx)) / (2 * d)
    dv = (field(uv + [0.0, d], ctx) - field(uv - [0.0, d], ctx)) / (2 * d)
    return normalize(np.stack([-du, dv, np.ones_like(du)], 1))  # y up the image: v runs down


def pixel_size(raster):
    """Per entry, the model-space size of one pixel of the map (the mean of its two sides)."""
    low = raster.low
    s = 0.5 * (np.linalg.norm(low.Pu[raster.tri], axis=1) + np.linalg.norm(low.Pv[raster.tri], axis=1)) / raster.scale
    return np.maximum(s, 1e-4)


# ----------------------------------------------------------------------------
# Images: blurs, sampling, the skin's painted lines


def blur(img, sigma, mask=None):
    """A Gaussian blur (separable); with `mask`, normalised within it (nothing outside bleeds in)."""
    if sigma <= 0:
        return img
    r = int(math.ceil(sigma * 3))
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2)
    k /= k.sum()

    def conv(a):
        a = np.apply_along_axis(lambda x: np.convolve(np.pad(x, r, mode="edge"), k, "valid"), 0, a)
        return np.apply_along_axis(lambda x: np.convolve(np.pad(x, r, mode="edge"), k, "valid"), 1, a)

    if mask is None:
        return conv(img)
    m = mask.astype(np.float64)
    return conv(img * m) / np.maximum(conv(m), 1e-6)


def sampler(img, scale=1.0):
    """A bilinear sampler of an image (rows down) at texel coordinates (x, y) * scale, clamped at the edges."""
    h, w = img.shape[:2]

    def f(uv, ctx=None):
        x = uv[:, 0] * scale - 0.5
        y = uv[:, 1] * scale - 0.5
        x0 = np.clip(np.floor(x).astype(np.int64), 0, w - 1)
        y0 = np.clip(np.floor(y).astype(np.int64), 0, h - 1)
        x1 = np.clip(x0 + 1, 0, w - 1)
        y1 = np.clip(y0 + 1, 0, h - 1)
        fx = np.clip(x - x0, 0, 1)
        fy = np.clip(y - y0, 0, 1)
        if img.ndim == 3:
            fx, fy = fx[:, None], fy[:, None]
        return ((img[y0, x0] * (1 - fx) + img[y0, x1] * fx) * (1 - fy) +
                (img[y1, x0] * (1 - fx) + img[y1, x1] * fx) * fy)

    return f


def coverage(low, scale):
    """The texels (at `scale`) the model's triangles cover."""
    r = Raster(low, scale)
    m = np.zeros(r.w * r.h, bool)
    m[r.pix] = True
    return m.reshape(r.h, r.w)


def painted_lines(rgb, mask, sigmas=(0.8, 1.4), lo=0.1, hi=0.35, min_len=8.0, full_len=26.0):
    """The dark lines painted into a skin (creases, wrinkles, outlines): 0..1 per texel, where the brightness has a
    valley narrower than a few texels, clearly longer than wide, and deep against the skin's own speckle."""
    lum = rgb[..., 0] * 0.3 + rgb[..., 1] * 0.59 + rgb[..., 2] * 0.11
    lum = lum / 255.0
    out = np.zeros(lum.shape)
    for s in sigmas:
        L = blur(lum, s, mask)
        Lp = np.pad(L, 1, mode="edge")
        dxx = Lp[1:-1, 2:] - 2 * L + Lp[1:-1, :-2]
        dyy = Lp[2:, 1:-1] - 2 * L + Lp[:-2, 1:-1]
        dxy = (Lp[2:, 2:] - Lp[2:, :-2] - Lp[:-2, 2:] + Lp[:-2, :-2]) * 0.25
        tr, det = dxx + dyy, dxx * dyy - dxy * dxy
        disc = np.sqrt(np.maximum(tr * tr * 0.25 - det, 0))
        l1, l2 = tr * 0.5 + disc, tr * 0.5 - disc  # l1 the larger: a valley's curvature across it
        valley = np.maximum(l1, 0) * s * s  # scale-normalised
        elong = np.clip((l1 - np.abs(l2)) / np.maximum(l1, 1e-9), 0, 1)
        # against the local speckle: the brightness's own spread nearby
        mu = blur(lum, 3.0, mask)
        sd = np.sqrt(np.maximum(blur(lum * lum, 3.0, mask) - mu * mu, 1e-6))
        strength = valley * elong / (sd + 0.02)
        out = np.maximum(out, strength)
    v = np.clip((out - lo) / (hi - lo), 0, 1)
    v = v * v * (3 - 2 * v) * mask
    # only lines: pieces of at least `min_len` texels end to end (the speckle's are a texel or two)
    lab = labels(v > 0.05)
    keep = np.zeros(lab.max() + 2)
    ys, xs = np.nonzero(lab >= 0)
    ids = lab[ys, xs]
    order = np.argsort(ids, kind="stable")
    ids, ys, xs = ids[order], ys[order], xs[order]
    starts = np.flatnonzero(np.r_[True, ids[1:] != ids[:-1]])
    ends = np.r_[starts[1:], len(ids)]
    for a, b in zip(starts, ends):
        ext = math.hypot(xs[a:b].max() - xs[a:b].min(), ys[a:b].max() - ys[a:b].min())
        k = np.clip((ext - min_len) / (full_len - min_len), 0, 1)
        keep[ids[a]] = k * k * (3 - 2 * k)
    return np.where(lab >= 0, v * keep[np.maximum(lab, 0)], 0.0)


def labels(b):
    """8-connected components of a boolean image: a label per True pixel (-1 elsewhere)."""
    h, w = b.shape
    lab = np.where(b, np.arange(h * w).reshape(h, w), h * w)
    big = h * w
    while True:
        p = np.pad(lab, 1, constant_values=big)
        m = lab.copy()
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                m = np.minimum(m, p[1 + dy:1 + dy + h, 1 + dx:1 + dx + w])
        m = np.where(b, m, big)
        # pointer jumping: each label takes its label's label
        flat = m.ravel()
        flat = np.where(flat < big, np.minimum(flat, flat[np.minimum(flat, big - 1)]), big)
        m = flat.reshape(h, w)
        if np.array_equal(m, lab):
            break
        lab = m
    return np.where(b, lab, -1)


def finish(raster, nworld, detail=None, margin=8, supersample=2, agree_deg=6.0, spread_deg=14.0):
    """The map from each entry's wanted normal (model space) and, laid on it, a tangent-space detail: resolved where
    triangles share texels, box-filtered down by `supersample`, margins grown round the islands. (h, w, 3) floats."""
    tan = raster.encode(nworld)
    if detail is not None:
        tan = rnm(tan, detail)
    img, cov, keep = resolve(raster, tan, agree_deg=agree_deg, spread_deg=spread_deg)
    f = supersample
    img = downsample(img, f)
    cov = cov.reshape(raster.h // f, f, raster.w // f, f).any((1, 3))
    img, _ = dilate(img, cov, margin)
    return img, keep


def skin_rgb(model, index=0):
    """An .mdl's skin as RGB (rows top to bottom), through Quake's palette."""
    pal = np.array(_qpal().PALETTE, np.uint8)
    w, h = model.skin_size
    g, _, ims = model.skins[0] if index == 0 else model.skins[index]
    return pal[np.frombuffer(ims[0], np.uint8)].reshape(h, w, 3)
