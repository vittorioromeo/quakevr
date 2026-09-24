# mdlgen.py -- small Quake MDL builder for Quake VR's generated props (make_holster.py,
# make_gadget.py): flat-shaded bevelled boxes on a palette-indexed skin, written as a one-frame
# MDL. Triangles are clockwise seen from outside, as Quake's (Ironwail: glFrontFace(GL_CW)).

import math
import os
import re
import struct


def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(a):
    l = math.sqrt(dot(a, a))
    return mul(a, 1.0 / l)


class Mesh:
    def __init__(self, skin_w, skin_h, regions):
        self.verts = []  # (position, normal, (s, t) in skin pixels)
        self.tris = []
        self.skin_w = skin_w
        self.skin_h = skin_h
        self.regions = regions  # name -> (s0, t0, s1, t1)

    def quad(self, p0, p1, p2, p3, region):
        """A quad p0 p1 p2 p3, counter-clockwise seen from outside."""
        n = norm(cross(sub(p1, p0), sub(p3, p0)))
        s0, t0, s1, t1 = self.regions[region]
        st = [(s0 + 1, t1 - 1), (s1 - 1, t1 - 1), (s1 - 1, t0 + 1), (s0 + 1, t0 + 1)]
        base = len(self.verts)
        for p, uv in zip((p0, p1, p2, p3), st):
            self.verts.append((p, n, uv))
        self.tris.append((base, base + 2, base + 1))
        self.tris.append((base, base + 3, base + 2))

    def box(self, centre, half, region, bevel=0.0, cap_region=None):
        """An axis-aligned box; with a bevel, its z-parallel edges are cut at 45 degrees."""
        cx, cy, cz = centre
        hx, hy, hz = half
        b = min(bevel, hx * 0.5, hy * 0.5)
        if b > 0.0:
            ring = [(hx - b, -hy), (hx, -hy + b), (hx, hy - b), (hx - b, hy), (-hx + b, hy), (-hx, hy - b),
                    (-hx, -hy + b), (-hx + b, -hy)]
        else:
            ring = [(hx, -hy), (hx, hy), (-hx, hy), (-hx, -hy)]
        lo = [(cx + x, cy + y, cz - hz) for x, y in ring]
        hi = [(cx + x, cy + y, cz + hz) for x, y in ring]
        n = len(ring)
        for i in range(n):
            j = (i + 1) % n
            self.quad(lo[i], lo[j], hi[j], hi[i], region)
        s0, t0, s1, t1 = self.regions[cap_region or region]
        for pts, up in ((hi, True), (lo, False)):
            normal = (0.0, 0.0, 1.0 if up else -1.0)
            base = len(self.verts)
            self.verts.append(((cx, cy, cz + (hz if up else -hz)), normal, ((s0 + s1) // 2, (t0 + t1) // 2)))
            for k, p in enumerate(pts):
                # The cap's texture follows its outline, within its region.
                u = s0 + 1 + (p[0] - cx + hx) / (2 * hx) * (s1 - s0 - 2)
                v = t0 + 1 + (p[1] - cy + hy) / (2 * hy) * (t1 - t0 - 2)
                self.verts.append((p, normal, (int(u), int(v))))
            for i in range(n):
                a, bb = base + 1 + i, base + 1 + (i + 1) % n
                self.tris.append((base, bb, a) if up else (base, a, bb))

    def check_winding(self):
        bad = 0
        for a, b, c in self.tris:
            p0, p1, p2 = self.verts[a][0], self.verts[b][0], self.verts[c][0]
            if dot(cross(sub(p1, p0), sub(p2, p0)), self.verts[a][1]) > 0.0:
                bad += 1
        return bad


def anorms():
    here = os.path.dirname(os.path.abspath(__file__))
    text = open(os.path.join(here, "..", "..", "Quake", "anorms.h")).read()
    return [tuple(float(x) for x in m.groups()) for m in
            re.finditer(r"\{\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\}", text)]


def write_mdl(path, mesh, skin, name):
    assert mesh.check_winding() == 0, "counter-clockwise triangles"
    table = anorms()
    positions = [v[0] for v in mesh.verts]
    lo = [min(p[k] for p in positions) for k in range(3)]
    hi = [max(p[k] for p in positions) for k in range(3)]
    scale = [(hi[k] - lo[k]) / 255.0 or 1.0 for k in range(3)]
    radius = max(math.sqrt(dot(p, p)) for p in positions)

    data = bytearray(struct.pack("<4si3f3f f3f 8i f", b"IDPO", 6, *scale, *lo, radius, 0.0, 0.0, 0.0,
                                 1, mesh.skin_w, mesh.skin_h, len(mesh.verts), len(mesh.tris), 1, 0, 0, 1.0))
    data += struct.pack("<i", 0) + skin
    for _, _, (s, t) in mesh.verts:
        data += struct.pack("<3i", 0, s, t)
    for a, b, c in mesh.tris:
        data += struct.pack("<4i", 1, a, b, c)
    data += struct.pack("<i", 0)
    data += bytes((0, 0, 0, 0)) + bytes((255, 255, 255, 0))
    data += name.encode().ljust(16, b"\0")[:16]
    for p, n, _ in mesh.verts:
        q = [max(0, min(255, int(round((p[k] - lo[k]) / scale[k])))) for k in range(3)]
        best = max(range(len(table)), key=lambda i: dot(table[i], n))
        data += bytes((q[0], q[1], q[2], best))
    with open(path, "wb") as f:
        f.write(data)


def dithered_skin(w, h, regions, ramps, seed, extra=None):
    """Palette indices: each region a ramp, mostly its middle, with lighter and darker specks;
    `extra(region, s, t)` may return an index to use instead."""
    rng = seed
    px = bytearray()
    for t in range(h):
        for s in range(w):
            region = next(r for r, (s0, t0, s1, t1) in regions.items() if s0 <= s < s1 and t0 <= t < t1)
            ramp = ramps[region]
            rng = (rng * 1103515245 + 12345) & 0x7FFFFFFF
            k = (rng >> 16) % 10
            i = len(ramp) // 2 + (-1 if k < 3 else 1 if k > 7 else 0)
            special = extra(region, s, t) if extra else None
            px.append(special if special is not None else ramp[max(0, min(len(ramp) - 1, i))])
    return bytes(px)
