#!/usr/bin/env python3
# make_clean_skins.py -- clean versions of the held weapons' skins that have blood painted into them (the axe, the
# knights' swords, the ogre's chainsaw, the grunt's shotgun), for Gore > Blood on You and Your Gear > "Clean Weapon
# Skins" (vr_gore_clean_skins; ROUND21.md, "Clean weapon skins"): the blood that gear takes is dynamic now
# (vr_wounds.cpp), so a weapon starts clean and is clean again after a wash.
#
# What is shipped is not a skin but a patch: progs/<model>_<skin>.clean, a list of (texel, texel to copy) pairs, for
# the skin whose pixels hash to the value it names. The engine applies it as the skin loads (vr_cleanskins.cpp): every
# bloody texel is replaced by a copy of a clean texel of the same skin. The patch holds no pixel, so nothing of the art
# (id's, or a pack's made from id's) is distributed; it builds the clean skin from the player's own file, and a file
# that differs from the one it was made for (another version, a mod's) is left as it is.
#
# Usage:
#   python Misc/quakevr/make_clean_skins.py [--preview <dir>]
#       the shipped models' 8-bit skins (quakevr/progs): their patches, quakevr/progs/<model>_<skin>.clean.
#       --preview: before/after images of every changed skin (the whole skin, then each changed island enlarged).
#   python Misc/quakevr/make_clean_skins.py --hq <game dir> [--preview <dir>]
#       a texture pack's full-colour replacements of the same skins (<game dir>/progs/<model>_<skin>.png or .tga,
#       DarkPlaces' names), at their full resolution: <game dir>/progs/<model>_<skin>_hq.clean beside each. Run on
#       the player's machine, on his pack's files (nothing of it is shipped).
#
# How a skin is cleaned (clean()):
#   1. Where: the texels the model's triangles cover (its islands; two texels more round them, for filtering and
#      mips), within the skin's areas below (SKINS: the parts that are the weapon, less what is red by design: a lamp).
#   2. Which: blood by its colour: a red (hue within 16 degrees of pure red), saturated (0.45 and up) texel, and the
#      darker, duller reds (to 0.25) joined to one: a splash's rim, a smear's thin end. Never a fullbright texel, nor
#      one the engine's background fill may change (pairs_hash).
#   3. With what: each bloody texel, from the rim of a stain inwards, takes the clean texel of its island (or of the
#      entry's sources) whose neighbourhood (5 x 5) matches its own best (the texels known so far), nearer ones
#      preferred a little, the continuation of its neighbours' sources tried first (the metal's grain, a scratch, an
#      edge carry on). A guide (guide()) keeps big stains from going flat: the colour expected there, the clean
#      texels' round it plus the stain's own variations of brightness (the bevel and the grain the blood was painted
#      over); a copy's 3 x 3 surroundings must match it, the texel itself a little. Then two passes again over the
#      whole stain with full neighbourhoods. A copy is a texel of the skin: the same palette, dither and noise.
#
# Pure Python with numpy and Pillow (as make_crates.py). The output is the same on every run (seeded).

import argparse
import colorsys
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mdlgen import HEADER, read_skins  # noqa: E402

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
PROGS = os.path.join(ROOT, "quakevr", "progs")

MAGIC = b"QVRCLEAN"
VERSION = 1

# The skins with blood painted in. areas: (x0, y0, x1, y1) of the skin (texels, x1 and y1 excluded) where blood is
# looked for (None: the whole skin); keep: areas left as they are (red by design); sources: where copies may come from
# (default: anywhere in the islands). The survey (every held, holstered
# and propped model of quakevr/progs; ROUND21.md) found no other: the rest's reds are paint (the crowbar, the
# hazard stripes of the multi-rocket and grappling hook's skins, the proximity launcher's panel), glows (the lava and
# plasma guns, the torch's flame) and wood (the crates, the planks, the torch's handle).
SKINS = [
    # The axe: its head (the blade, red over two thirds of it) and its haft.
    dict(model="v_axe.mdl", skin=0, areas=[(288, 20, 424, 104), (456, 10, 490, 116)], keep=[],
         sources=[(292, 27, 418, 86), (456, 10, 490, 116)]),  # the blade's face (not the socket's smooth flare)
    # The knight's sword: the blade's two sides (their outer half red) and the woven grip.
    dict(model="v_ksword.mdl", skin=0, areas=[(132, 8, 228, 26), (148, 222, 244, 242), (220, 186, 244, 226)], keep=[],
         sources=[(136, 12, 180, 22), (152, 227, 187, 237), (220, 186, 244, 226)]),  # the blade's clean steel, the grip
    # The hell knight's sword: the blade's two sides (drips); the hilt's dark-red wrap is the leather's.
    dict(model="v_hksword.mdl", skin=0, areas=[(32, 0, 162, 20), (288, 0, 418, 20)], keep=[]),
    # The ogre's chainsaw: the bar (both sides; its rust-brown stays), the motor's housing (both sides), the specks
    # on the handle.
    dict(model="v_chainsaw.mdl", skin=0, areas=[(26, 68, 54, 144), (282, 68, 310, 144), (0, 142, 60, 210),
                                                 (254, 142, 316, 210), (26, 6, 114, 52), (136, 46, 204, 62)], keep=[]),
    # The grunt's shotgun: its body's smears (both sides); its red lamp stays.
    dict(model="v_gruntgun.mdl", skin=0, areas=[(160, 0, 206, 104), (210, 134, 258, 238)], keep=[(230, 178, 240, 192)]),
]

# Blood's colour (hue in degrees from pure red, HSV saturation and value, 0..1).
HUE = 16.0
SAT_CORE = 0.45
SAT_RIM = 0.25
VAL_MIN = 0.06
RIM_REACH = 3  # texels from a saturated one that a duller red still counts (at the 8-bit skin's size)

PATCH = 2   # the neighbourhood's half size (5 x 5)
SEARCH = 40  # texels (at the 8-bit skin's size): how far a source is looked for first
NEAR = 0.15  # how much nearer sources are preferred (a share of the neighbourhood's cost at SEARCH)
PASSES = 2
GUIDE = 4.0  # how much a copy's surroundings' colour must follow guide()'s (a share of a neighbourhood texel's cost)
GUIDE_TEXEL = 0.5  # and the copied texel's own


# ---------------------------------------------------------------------------------------------------------------
# Files


def palette():
    """Quake's palette ((256, 3) uint8): quakevr/gfx/palette.lmp if there, else id1's pak0 (the kit's game folders)."""
    for path in (os.path.join(ROOT, "quakevr", "gfx", "palette.lmp"),):
        if os.path.exists(path):
            return np.frombuffer(open(path, "rb").read()[:768], np.uint8).reshape(256, 3)
    import quakepak
    for base in (os.environ.get("QUAKE_DIR", ""), "C:/OHWorkspace/qvr-kit/qbase"):
        pak = os.path.join(base, "id1", "pak0.pak")
        if base and os.path.exists(pak):
            return np.frombuffer(quakepak.read_pak(pak)["gfx/palette.lmp"][:768], np.uint8).reshape(256, 3)
    sys.exit("no palette: set QUAKE_DIR to the Quake folder (id1/pak0.pak)")


def load_mdl(path):
    """(skins: [(h, w) uint8, each skin's first frame], st: [(onseam, s, t)], tris: [(facesfront, a, b, c)], w, h)."""
    data = open(path, "rb").read()
    hdr = HEADER.unpack_from(data, 0)
    num_skins, w, h, num_verts, num_tris = hdr[12], hdr[13], hdr[14], hdr[15], hdr[16]
    raw, off = read_skins(data, HEADER.size, num_skins, w, h)
    skins = []
    for s in raw:
        (group,) = struct.unpack_from("<i", s, 0)
        start = 4 if group == 0 else 8 + 4 * struct.unpack_from("<i", s, 4)[0]
        skins.append(np.frombuffer(s[start:start + w * h], np.uint8).reshape(h, w))
    st = [struct.unpack_from("<3i", data, off + 12 * i) for i in range(num_verts)]
    off += 12 * num_verts
    tris = [struct.unpack_from("<4i", data, off + 16 * i) for i in range(num_tris)]
    return skins, st, tris, w, h


def fnv1a(data):
    """FNV-1a, 32 bits, of `data` (bytes)."""
    h = 0x811C9DC5
    for b in bytes(data):
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def pairs_hash(pixels, pairs, bpp):
    """The hash a patch names: of the texels it reads and writes, pair by pair (the texel written, then the one read;
    each its bytes: an index, or RGBA), as vr_cleanskins.cpp computes it. Only those: the engine fills a skin's
    background before its first upload (Mod_FloodFillSkin) and not when it uploads it again, so the rest may differ."""
    flat = np.ascontiguousarray(pixels).reshape(-1, bpp)
    return fnv1a(flat[np.asarray(pairs).reshape(-1)].tobytes())


def flood_region(sk):
    """The texels Mod_FloodFillSkin may change: those of texel (0, 0)'s colour joined to it (4-neighbours)."""
    h, w = sk.shape
    region = np.zeros((h, w), bool)
    c = sk[0, 0]
    stack = [(0, 0)]
    region[0, 0] = True
    while stack:
        y, x = stack.pop()
        for ny, nx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
            if 0 <= ny < h and 0 <= nx < w and not region[ny, nx] and sk[ny, nx] == c:
                region[ny, nx] = True
                stack.append((ny, nx))
    return region


def write_patch(path, w, h, bpp, digest, pairs):
    """The patch: magic, version, width, height, bytes per pixel, the source's hash, the pair count, then the pairs
    (u32 texel, u32 texel copied into it; y * width + x), all little-endian."""
    with open(path, "wb") as f:
        f.write(MAGIC + struct.pack("<6I", VERSION, w, h, bpp, digest, len(pairs)))
        f.write(np.asarray(pairs, "<u4").tobytes())


# ---------------------------------------------------------------------------------------------------------------
# Where and which


def coverage(st, tris, w, h, scale=1):
    """The texels the triangles cover ((h*scale, w*scale) bool), a texel more round them (a texel's filtering)."""
    img = Image.new("L", (w * scale, h * scale), 0)
    dr = ImageDraw.Draw(img)
    for facesfront, a, b, c in tris:
        pts = []
        for v in (a, b, c):
            onseam, s, t = st[v]
            if onseam and not facesfront:
                s += w // 2
            pts.append((s * scale, t * scale))
        dr.polygon(pts, fill=255, outline=255)
    return dilate(np.array(img) > 0, scale)


def dilate(m, n=1):
    for _ in range(n):
        d = m.copy()
        d[1:] |= m[:-1]
        d[:-1] |= m[1:]
        d[:, 1:] |= m[:, :-1]
        d[:, :-1] |= m[:, 1:]
        m = d
    return m


def islands(m):
    """Connected parts of `m` (4-neighbours): labels ((h, w) int, -1 outside) and their count."""
    h, w = m.shape
    lab = -np.ones((h, w), np.int32)
    n = 0
    for y0, x0 in zip(*np.nonzero(m)):
        if lab[y0, x0] >= 0:
            continue
        stack = [(y0, x0)]
        lab[y0, x0] = n
        while stack:
            y, x = stack.pop()
            for ny, nx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                if 0 <= ny < h and 0 <= nx < w and m[ny, nx] and lab[ny, nx] < 0:
                    lab[ny, nx] = n
                    stack.append((ny, nx))
        n += 1
    return lab, n


def blood_classes(rgb, vmin=VAL_MIN):
    """Per colour ((n, 3) uint8): 2 a saturated blood red, 1 a duller one (a rim), 0 none."""
    out = np.zeros(len(rgb), np.uint8)
    for i, (r, g, b) in enumerate(rgb):
        hh, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
        hue = min(hh, 1.0 - hh) * 360.0
        if hue > HUE or v < vmin or r <= max(g, b):
            continue
        out[i] = 2 if s >= SAT_CORE else (1 if s >= SAT_RIM else 0)
    return out


def blood_mask(cls, where, reach):
    """The bloody texels: saturated ones, and duller ones joined to them within `reach` texels, in `where`."""
    core = (cls == 2) & where
    rim = (cls >= 1) & where
    m = core
    for _ in range(reach):
        m = m | (dilate(m) & rim)
    return m


def area_mask(shape, areas, keep, scale):
    m = np.zeros(shape, bool)
    for x0, y0, x1, y1 in areas or [(0, 0, shape[1] // scale, shape[0] // scale)]:
        m[y0 * scale:y1 * scale, x0 * scale:x1 * scale] = True
    for x0, y0, x1, y1 in keep:
        m[y0 * scale:y1 * scale, x0 * scale:x1 * scale] = False
    return m


# ---------------------------------------------------------------------------------------------------------------
# With what


def blur(a, sigma):
    """A Gaussian blur of the (h, w) array `a` (zero outside)."""
    r = max(1, int(3 * sigma))
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2)
    k /= k.sum()
    a = np.apply_along_axis(lambda v: np.convolve(v, k)[r:r + len(v)], 0, a)
    return np.apply_along_axis(lambda v: np.convolve(v, k)[r:r + len(v)], 1, a)


def local_stats(val, where, sigma):
    """The mean and standard deviation of `val` over the texels of `where` round each texel (Gaussian weights)."""
    wgt = blur(where.astype(np.float64), sigma)
    mean = blur(np.where(where, val, 0.0), sigma) / np.maximum(wgt, 1e-9)
    sq = blur(np.where(where, val * val, 0.0), sigma) / np.maximum(wgt, 1e-9)
    return mean, np.sqrt(np.maximum(sq - mean * mean, 0.0)), wgt


def guide(img, mask, valid, lab, scale):
    """The colour each bloody texel's copy should have ((h, w, 3); NaN elsewhere): the clean texels' round it, with
    the stain's own variations of brightness (a bevel's highlight, the noise, a scratch: the blood was painted over
    them) brought to the clean texels' contrast. Per island; where no clean texel is near, a wider round's."""
    lum = img @ np.array([0.299, 0.587, 0.114], np.float32)
    out = np.full(img.shape, np.nan)
    for i in np.unique(lab[mask]):
        isl = lab == i
        c, b = valid & isl, mask & isl
        if not c.any():
            continue
        _, cs, cw = local_stats(lum, c, 6.0 * scale)
        _, ws, _ = local_stats(lum, c, 24.0 * scale)
        far = cw < 0.02
        cs[far] = ws[far]
        bm, bs, _ = local_stats(lum, b, 6.0 * scale)
        gain = np.clip(cs / np.maximum(bs, 1.0), 0.5, 4.0)
        detail = (lum - bm) * gain
        for k in range(3):
            cm, _, _ = local_stats(img[..., k], c, 6.0 * scale)
            wm, _, _ = local_stats(img[..., k], c, 24.0 * scale)
            cm[far] = wm[far]
            out[..., k][b] = (cm + detail)[b]
    return out


def fill(img, mask, valid, lab, scale, seed):
    """For each texel of `mask`, the texel of `valid` (clean, same island preferably) whose copy continues the skin
    best: ((h, w) int32 of flat source indices, -1 where not masked). img: (h, w, 3) float."""
    h, w, _ = img.shape
    rng = np.random.default_rng(seed)
    r = PATCH
    offs = np.array([(dy, dx) for dy in range(-r, r + 1) for dx in range(-r, r + 1)])
    pad = np.zeros((h + 2 * r, w + 2 * r, 3), np.float32)
    pad[r:r + h, r:r + w] = img
    known = np.zeros((h + 2 * r, w + 2 * r), bool)
    known[r:r + h, r:r + w] = ~mask & (lab >= 0)
    srcok = np.zeros_like(known)
    srcok[r:r + h, r:r + w] = valid
    search = SEARCH * scale
    gl = guide(img, mask, valid, lab, scale)
    # the guide and the sources over a neighbourhood (a 3 x 3 mean): a copy's surroundings take the guide's colour,
    # the texel itself keeps the source's grain
    def box(a, ok):
        acc = np.zeros(a.shape, np.float64)
        cnt = np.zeros(a.shape[:2], np.float64)
        for oy in (-1, 0, 1):
            for ox in (-1, 0, 1):
                acc += np.roll(np.where(ok[..., None], a, 0.0), (oy, ox), (0, 1))
                cnt += np.roll(ok, (oy, ox), (0, 1))
        return acc / np.maximum(cnt, 1.0)[..., None]
    gmask = ~np.isnan(gl[..., 0])
    gl_mean = box(np.nan_to_num(gl), gmask)
    src_mean = box(img, valid)
    src = -np.ones((h, w), np.int64)

    # the valid texels of each island (and of the whole skin, for an island with too few)
    vy, vx = np.nonzero(valid)
    vlab = lab[vy, vx]
    by_island = {}
    for i in np.unique(lab[mask]):
        sel = vlab == i
        by_island[i] = (vy[sel], vx[sel]) if sel.sum() >= 24 else (vy, vx)

    def windows(cy, cx):
        """The candidates' neighbourhoods: (n, k, 3) colours and (n, k) usable."""
        yy = cy[:, None] + offs[None, :, 0] + r
        xx = cx[:, None] + offs[None, :, 1] + r
        return pad[yy, xx], srcok[yy, xx]

    def best(y, x, full):
        ty = y + offs[:, 0] + r
        tx = x + offs[:, 1] + r
        tw = known[ty, tx].copy()
        if full:
            tw[len(offs) // 2] = False  # its own value doesn't vote for itself
        tcol = pad[ty, tx]
        cy, cx = by_island[lab[y, x]]
        d2 = (cy - y) ** 2 + (cx - x) ** 2
        near = d2 <= search * search
        if near.sum() < 64:
            near = np.argsort(d2)[:256]
            cy, cx, d2 = cy[near], cx[near], d2[near]
        else:
            cy, cx, d2 = cy[near], cx[near], d2[near]
            if len(cy) > 2500:
                pick = rng.choice(len(cy), 2500, replace=False)
                cy, cx, d2 = cy[pick], cx[pick], d2[pick]
        # coherence: the neighbours' sources, shifted
        coh = []
        for dy, dx in offs:
            ny, nx = y + dy, x + dx
            if 0 <= ny < h and 0 <= nx < w and src[ny, nx] >= 0:
                sy, sx = divmod(int(src[ny, nx]), w)
                sy, sx = sy - dy, sx - dx
                if 0 <= sy < h and 0 <= sx < w and valid[sy, sx]:
                    coh.append((sy, sx))
        if src[y, x] >= 0:
            coh.append(divmod(int(src[y, x]), w))
        if coh:
            coh = np.array(coh)
            cy = np.concatenate([cy, coh[:, 0]])
            cx = np.concatenate([cx, coh[:, 1]])
            d2 = np.concatenate([d2, (coh[:, 0] - y) ** 2 + (coh[:, 1] - x) ** 2])
        cols, ok = windows(cy, cx)
        wt = tw[None, :].astype(np.float32)
        diff = ((cols - tcol[None]) ** 2).sum(2)
        cost = (diff * wt * ok + wt * (~ok) * 3.0 * 255 * 255).sum(1) / max(1.0, wt.sum())
        cost += NEAR * 3 * 40.0 ** 2 * d2 / float(search * search)
        if gmask[y, x]:
            cost += GUIDE * ((src_mean[cy, cx] - gl_mean[y, x]) ** 2).sum(1)
            cost += GUIDE_TEXEL * ((img[cy, cx] - gl[y, x]) ** 2).sum(1)
        k = int(np.argmin(cost))
        return cy[k] * w + cx[k]

    # the stains' texels by their distance from clean ones (onion peeling)
    order = []
    left = mask.copy()
    done = ~mask
    while left.any():
        layer = left & dilate(done)
        if not layer.any():  # unreachable (an island wholly bloody): its texels from the nearest anywhere
            layer = left
        ys, xs = np.nonzero(layer)
        p = rng.permutation(len(ys))
        order.append((ys[p], xs[p]))
        done = done | layer
        left = left & ~layer
    for ys, xs in order:
        for y, x in zip(ys, xs):
            s = best(y, x, False)
            src[y, x] = s
            sy, sx = divmod(int(s), w)
            pad[y + r, x + r] = img[sy, sx]
            known[y + r, x + r] = True
    for _ in range(PASSES):
        ys, xs = np.nonzero(mask)
        p = rng.permutation(len(ys))
        for y, x in zip(ys[p], xs[p]):
            s = best(y, x, True)
            src[y, x] = s
            sy, sx = divmod(int(s), w)
            pad[y + r, x + r] = img[sy, sx]
    return src


def clean(pixels, rgb, cov, entry, scale, fullbright=None, seed=1):
    """pixels: (h, w) indices with rgb (256, 3), or (h, w, 4) RGBA with rgb None. The mask and the copy map."""
    def classes(vmin):
        if rgb is not None:
            return blood_classes(rgb, vmin)[pixels]
        cols = pixels[..., :3].reshape(-1, 3)
        uniq, inv = np.unique(cols, axis=0, return_inverse=True)
        return blood_classes(uniq, vmin)[inv.reshape(-1)].reshape(pixels.shape[:2])
    cls = classes(VAL_MIN)
    reddish = classes(0.0) > 0  # never a source: a red too dark to count as blood is still one
    img = (rgb[pixels] if rgb is not None else pixels[..., :3]).astype(np.float32)
    where = dilate(cov, 2 * scale) & area_mask(cov.shape, entry["areas"], entry["keep"], scale)
    mask = blood_mask(cls, where, RIM_REACH * scale)
    if rgb is not None:  # never a texel the engine's background fill may change (pairs_hash)
        where_not = flood_region(pixels)
        mask &= ~where_not
    if fullbright is not None:
        mask &= ~fullbright
    valid = cov & ~mask & ~reddish
    if rgb is not None:
        valid &= ~where_not
    if entry.get("sources"):
        valid &= area_mask(cov.shape, entry["sources"], [], scale)
    if fullbright is not None:
        valid &= ~fullbright
    if rgb is None:
        valid &= pixels[..., 3] > 0
    lab, _ = islands(cov)
    for _ in range(2 * scale):  # the texels round an island are its (not a bridge to the next one)
        grown = lab.copy()
        for sh in ((1, 0), (-1, 0), (1, 1), (-1, 1)):
            nb = np.roll(lab, sh[0], axis=sh[1])
            grown = np.where((grown < 0) & (nb >= 0), nb, grown)
        lab = grown
    return mask, fill(img, mask, valid, lab, scale, seed)


# ---------------------------------------------------------------------------------------------------------------
# Output


def preview(path, before, after, cov, mask, title):
    """Before and after, side by side: the whole skin (unused texels dimmed), then the changed islands enlarged."""
    def dim(a):
        a = a.astype(np.float32).copy()
        a[~cov] *= 0.25
        return a.astype(np.uint8)
    h, w = before.shape[:2]
    big = max(1, min(4, 640 // max(1, w)))
    rows = [np.concatenate([dim(before), np.full((h, 4, 3), 40, np.uint8), dim(after)], 1)]
    rows[0] = np.kron(rows[0], np.ones((big, big, 1), np.uint8))
    lab, n = islands(cov)
    for i in range(n):
        m = lab == i
        if not (m & mask).any():
            continue
        ys, xs = np.nonzero(m)
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        k = max(1, min(8, 600 // max(1, x1 - x0), 300 // max(1, y1 - y0)))
        pair = np.concatenate([before[y0:y1, x0:x1], np.full((y1 - y0, 2, 3), 40, np.uint8), after[y0:y1, x0:x1]], 1)
        rows.append(np.kron(pair, np.ones((k, k, 1), np.uint8)))
    W = max(r.shape[1] for r in rows)
    H = sum(r.shape[0] + 16 for r in rows) + 16
    sheet = Image.new("RGB", (W, H), (24, 24, 36))
    d = ImageDraw.Draw(sheet)
    d.text((4, 2), "%s  (before | after; %d texels)" % (title, int(mask.sum())), fill=(255, 230, 0))
    y = 16
    for r in rows:
        sheet.paste(Image.fromarray(r), (0, y))
        y += r.shape[0] + 16
    sheet.save(path)


def run_shipped(args, pal):
    fullbright_index = np.zeros(256, bool)
    fullbright_index[224:] = True
    for entry in SKINS:
        if args.only and entry["model"] not in args.only:
            continue
        path = os.path.join(PROGS, entry["model"])
        skins, st, tris, w, h = load_mdl(path)
        sk = skins[entry["skin"]]
        cov = coverage(st, tris, w, h)
        mask, src = clean(sk, pal, cov, entry, 1, fullbright_index[sk])
        ys, xs = np.nonzero(mask)
        dst = ys * w + xs
        pairs = np.stack([dst, src[ys, xs]], 1)
        out = os.path.join(PROGS, "%s_%d.clean" % (entry["model"], entry["skin"]))
        write_patch(out, w, h, 1, pairs_hash(sk, pairs, 1), pairs)
        after = sk.copy().reshape(-1)
        after[dst] = sk.reshape(-1)[src[ys, xs]]
        after = after.reshape(h, w)
        print("%-16s skin %d  %3d x %-3d  %5d texels  -> %s" % (entry["model"], entry["skin"], w, h, len(dst),
                                                              os.path.relpath(out, ROOT)))
        if args.preview:
            os.makedirs(args.preview, exist_ok=True)
            preview(os.path.join(args.preview, "%s_%d.png" % (entry["model"], entry["skin"])), pal[sk], pal[after],
                    cov, mask, "%s skin %d" % (entry["model"], entry["skin"]))


def run_hq(args, pal):
    found = 0
    for entry in SKINS:
        base = os.path.join(args.hq, "progs", "%s_%d" % (entry["model"], entry["skin"]))
        img_path = next((base + e for e in (".png", ".tga", ".jpg") if os.path.exists(base + e)), None)
        if not img_path:
            continue
        found += 1
        _, st, tris, w, h = load_mdl(os.path.join(PROGS, entry["model"]))
        px = np.array(Image.open(img_path).convert("RGBA"))
        H, W = px.shape[:2]
        scale = W // w
        if scale < 1 or W != w * scale or H != h * scale:
            print("%s: %d x %d is not a whole multiple of the skin's %d x %d: skipped" % (img_path, W, H, w, h))
            continue
        cov = coverage(st, tris, w, h, scale)
        mask, src = clean(px, None, cov, entry, scale)
        ys, xs = np.nonzero(mask)
        dst = ys * W + xs
        out = base + "_hq.clean"
        pairs = np.stack([dst, src[ys, xs]], 1)
        write_patch(out, W, H, 4, pairs_hash(px, pairs, 4), pairs)
        print("%s: %d x %d, %d pixels -> %s" % (img_path, W, H, len(dst), out))
        if args.preview:
            after = px[..., :3].reshape(-1, 3).copy()
            after[dst] = px[..., :3].reshape(-1, 3)[src[ys, xs]]
            os.makedirs(args.preview, exist_ok=True)
            preview(os.path.join(args.preview, "%s_%d_hq.png" % (entry["model"], entry["skin"])), px[..., :3],
                    after.reshape(H, W, 3), cov, mask, img_path)
    print("%d full-colour skins found" % found)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--hq", help="a game folder whose progs/ has full-colour replacements of these skins")
    ap.add_argument("--preview", help="a folder for before/after images")
    ap.add_argument("--only", nargs="*", help="only these models (v_axe.mdl ...)")
    args = ap.parse_args()
    pal = palette()
    if args.hq:
        run_hq(args, pal)
    else:
        run_shipped(args, pal)


if __name__ == "__main__":
    main()
