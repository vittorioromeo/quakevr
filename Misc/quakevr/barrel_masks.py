"""barrel_masks.py -- pre-marks each gun's barrel mask (weapons.txt's fx_gfx_mask; Quake/vr/vr_barrelmask.hpp; ROUND23.md,
"Barrel glow: the fire frames, the revolver, a painted barrel").

    python Misc/quakevr/barrel_masks.py [--write] [--preview out.png] [--only v_shot.mdl,...]

A gun's barrel is the slender part running back from its muzzle: each bore's axis (along the model's +x, through the
muzzle: its Muzzle anchor plus offset, or the model's bores' ends below), centred on the muzzle's ring of vertices; its
radius that ring's (a little over); a triangle is the barrel's when its three corners lie within that radius of a bore's
axis and behind the muzzle (from `back`, if given: no further back), and not in one of its `exclude` boxes (the
shotgun's pump round its magazine tube). Bodies, stocks and pumps lie outside the barrel's radius.

--write: the masks into quakevr/data/weapons.txt (weapons_data.py promote, each slot's own); --preview: a PNG of each
gun from the side and from above, the marked triangles orange (a review; nothing written).
"""

import argparse
import math
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import weapons_data  # noqa: E402

ROOT = weapons_data.ROOT
SEXTETS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"

# Model -> its barrel's spec: bores (y, z) to use instead of the muzzle's (two-barrelled guns), radius scale, back (no
# further back than this x), exclude boxes ((x0, y0, z0), (x1, y1, z1)).
SPECS = {
    "progs/v_shot.mdl": {"exclude_model": "progs/vr_pump_on_v_shot.mdl", "radius": 1.9, "back": 12.0},
    "progs/v_shot2.mdl": {"bores": [(-1.53, 6.39), (1.47, 6.39)]},
    "progs/v_nail.mdl": {"bores": [(-3.67, 3.84), (3.77, 3.84)]},
    "progs/v_lava.mdl": {"bores": [(-3.67, 3.84), (3.77, 3.84)]},
    "progs/v_nail2.mdl": {},
    "progs/v_lava2.mdl": {},
    "progs/v_rock.mdl": {},
    "progs/v_multi.mdl": {},
    "progs/v_prox.mdl": {},
    "progs/v_rock2.mdl": {},
    "progs/v_multi2.mdl": {},
    "progs/v_light.mdl": {},
    "progs/v_plasma.mdl": {},
    "progs/v_laserg.mdl": {},
    "progs/v_gruntgun.mdl": {},
    "progs/v_enfrifle.mdl": {},
    "progs/v_nailpistol.mdl": {},
    "progs/v_flaregun.mdl": {},
    "progs/v_revolver.mdl": {"back": 8.5},
}


class Mdl:
    """A .mdl's triangles (its drawing order) and rest pose (pose 0) in its units."""

    def __init__(self, path):
        d = open(path, "rb").read()
        h = struct.unpack_from("<4si3f3ff3f8i", d, 0)
        scale, origin = h[2:5], h[5:8]
        numskins, sw, sh, numverts, numtris = h[12:17]
        off = 84
        for _ in range(numskins):
            g, = struct.unpack_from("<i", d, off)
            off += 4
            if g == 0:
                off += sw * sh
            else:
                nb, = struct.unpack_from("<i", d, off)
                off += 4 + 4 * nb + nb * sw * sh
        off += 12 * numverts
        self.tris = [struct.unpack_from("<4i", d, off + 16 * i)[1:] for i in range(numtris)]
        off += 16 * numtris
        t, = struct.unpack_from("<i", d, off)
        off += 4 if t == 0 else 4 + 4 + 8 + 4 * struct.unpack_from("<i", d, off + 4)[0]
        off += 24  # (the pose's box and name)
        self.rest = []
        for i in range(numverts):
            x, y, z, _ = struct.unpack_from("<4B", d, off + 4 * i)
            self.rest.append((x * scale[0] + origin[0], y * scale[1] + origin[1], z * scale[2] + origin[2]))


def model_path(model):
    for base in ("quakevr", "id1"):
        p = os.path.join(ROOT, base, model)
        if os.path.exists(p):
            return p
    return None


def premark(m, muzzle, spec):
    rest = m.rest
    bores = spec.get("bores") or [(muzzle[1], muzzle[2])]
    xs = [v[0] for v in rest]
    length = max(muzzle[0] - min(xs), 1.0)
    rcap = max(1.5, 0.08 * length)

    def radial(v, axes):
        return min(math.hypot(v[1] - y, v[2] - z) for y, z in axes)

    # The muzzle's ring: the vertices near each bore's axis at the front; the axes centred on them.
    axes = []
    for y, z in bores:
        near = [v for v in rest if math.hypot(v[1] - y, v[2] - z) <= rcap]
        if not near:
            continue
        front = max(v[0] for v in near)
        ring = [v for v in near if v[0] >= front - 1.5]
        lo_y, hi_y = min(v[1] for v in ring), max(v[1] for v in ring)
        lo_z, hi_z = min(v[2] for v in ring), max(v[2] for v in ring)
        axes.append(((lo_y + hi_y) / 2, (lo_z + hi_z) / 2, front))
    if not axes:
        return [], 0.0
    yz = [(a[0], a[1]) for a in axes]
    front = max(a[2] for a in axes)
    ring = [radial(v, yz) for v in rest if v[0] >= front - 1.5 and radial(v, yz) <= rcap]
    radius = spec.get("radius", max(ring) * 1.08 + 0.1)
    back = spec.get("back", -1e9)
    skip = set()
    if spec.get("exclude_model"):  # a part drawn apart (the shotgun's pump): its triangles, by their middles
        part = Mdl(model_path(spec["exclude_model"]))
        keys = {tuple(round(sum(part.rest[i][k] for i in t) / 3.0, 1) for k in range(3)) for t in part.tris}
        skip = {n for n, t in enumerate(m.tris) if tuple(round(sum(rest[i][k] for i in t) / 3.0, 1) for k in range(3)) in keys}
    inside = [radial(v, yz) <= radius and back <= v[0] <= front + 0.5 for v in rest]
    out = []
    for t, tri in enumerate(m.tris):
        if t in skip or not all(inside[v] for v in tri):
            continue
        c = [sum(rest[v][k] for v in tri) / 3.0 for k in range(3)]
        if any(all(lo[k] <= c[k] <= hi[k] for k in range(3)) for lo, hi in spec.get("exclude", [])):
            continue
        out.append(t)
    return out, radius


def encode(marked, numtris):
    bits = [0] * numtris
    for t in marked:
        bits[t] = 1
    chars = []
    for i in range(0, numtris, 6):
        chars.append(SEXTETS[sum(bits[i + k] << k for k in range(6) if i + k < numtris)])
    return "%d:%s" % (numtris, "".join(chars))


def png(path, w, h, rgb):
    raw = b"".join(b"\0" + bytes(rgb[y * w * 3:(y + 1) * w * 3]) for y in range(h))

    def chunk(kind, data):
        c = struct.pack(">I", len(data)) + kind + data
        return c + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                           chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def raster(img, w, x0, y0, cw, ch, pts, colour):
    """A filled triangle (pts: three (u, v) in 0..1 of the cell) into the cell at x0, y0."""
    p = [(x0 + u * cw, y0 + (1 - v) * ch) for u, v in pts]
    lo_y, hi_y = int(max(y0, min(q[1] for q in p))), int(min(y0 + ch - 1, max(q[1] for q in p)))
    lo_x, hi_x = int(max(x0, min(q[0] for q in p))), int(min(x0 + cw - 1, max(q[0] for q in p)))
    (ax, ay), (bx, by), (cx, cy) = p
    den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
    if abs(den) < 1e-9:
        return
    for y in range(lo_y, hi_y + 1):
        for x in range(lo_x, hi_x + 1):
            l1 = ((by - cy) * (x + 0.5 - cx) + (cx - bx) * (y + 0.5 - cy)) / den
            l2 = ((cy - ay) * (x + 0.5 - cx) + (ax - cx) * (y + 0.5 - cy)) / den
            if l1 >= -0.01 and l2 >= -0.01 and l1 + l2 <= 1.01:
                i = (y * w + x) * 3
                img[i:i + 3] = colour


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--preview")
    ap.add_argument("--only")
    a = ap.parse_args()
    values = weapons_data.slot_values(with_defaults=True)
    rows = []
    for slot in sorted(values):
        v = values[slot]
        model = v.get("id", "0")
        if model not in SPECS or (a.only and os.path.basename(model) not in a.only.split(",")):
            continue
        path = model_path(model)
        if not path:
            print("slot %d %s: no model file" % (slot, model))
            continue
        anchor = [float(x) for x in v.get("muzzle_av", "0").split()] if len(v.get("muzzle_av", "").split()) == 3 else None
        if not anchor:
            print("slot %d %s: no muzzle anchor" % (slot, model))
            continue
        muzzle = [anchor[i] + float(v.get(k, "0")) for i, k in enumerate(("muzzle_x", "muzzle_y", "muzzle_z"))]
        m = Mdl(path)
        marked, radius = premark(m, muzzle, SPECS[model])
        if len(marked) < 20:  # (no barrel found round its muzzle, the grenade launchers': none, its ramp alone)
            print("slot %2d %-22s no barrel found (%d triangles): left unmarked" % (slot, model, len(marked)))
            continue
        rows.append((slot, model, m, marked))
        xs = [m.rest[i][0] for t in marked for i in m.tris[t]] or [0]
        print("slot %2d %-22s tris %4d marked %4d radius %.2f x %.1f..%.1f" % (slot, model, len(m.tris), len(marked),
                                                                             radius, min(xs), max(xs)))
    if a.preview and rows:
        cw, ch, cols = 300, 110, 2
        w, h = cw * cols, ch * len(rows)
        img = bytearray([24, 24, 28] * (w * h))
        for r, (slot, model, m, marked) in enumerate(rows):
            lo = [min(v[k] for v in m.rest) for k in range(3)]
            hi = [max(v[k] for v in m.rest) for k in range(3)]
            span = max(hi[0] - lo[0], 1e-3)
            on = set(marked)
            for view, (ku, kv) in enumerate(((0, 2), (0, 1))):
                vspan = max(hi[kv] - lo[kv], 1e-3)
                k = min((cw - 8) / span, (ch - 8) / vspan)

                def uv(p):
                    return ((p[ku] - lo[ku]) * k / cw + 4 / cw, (p[kv] - lo[kv]) * k / ch + 4 / ch)
                for t, tri in enumerate(m.tris):
                    if t not in on:
                        raster(img, w, view * cw, r * ch, cw, ch, [uv(m.rest[i]) for i in tri], bytes([120, 120, 130]))
                for t in marked:
                    raster(img, w, view * cw, r * ch, cw, ch, [uv(m.rest[i]) for i in m.tris[t]], bytes([255, 140, 20]))
        png(a.preview, w, h, img)
        print("preview: %s (%d guns: side, top)" % (a.preview, len(rows)))
    if a.write:
        stem = weapons_data.WEAPONS.stem
        cfg = os.path.join(ROOT, "scratch", "barrel_masks.cfg")
        with open(cfg, "w", encoding="utf-8") as f:
            for slot, model, m, marked in rows:
                f.write('%sfx_gfx_mask_%02d "%s"\n' % (stem, slot + 1, encode(marked, len(m.tris))))
        weapons_data.promote(cfg, False)


if __name__ == "__main__":
    main()
