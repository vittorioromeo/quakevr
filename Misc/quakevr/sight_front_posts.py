#!/usr/bin/env python3
"""sight_front_posts.py -- the front posts of the weapons with glowing iron sights (vr_sights.cpp frontPosts): each
skin's patches of sight texels (palette 224..239, 252, 253), each placed by the triangles mapping it on frame 0."""
import struct, sys

SIGHT = set(range(224, 240)) | {252, 253}

def load(path):
    b = open(path, 'rb').read()
    o = 0
    ident, ver = struct.unpack_from('<4si', b, o); o += 8
    scale = struct.unpack_from('<3f', b, o); o += 12
    trans = struct.unpack_from('<3f', b, o); o += 12
    o += 4 + 12
    numskins, sw, sh, nv, nt, nf, sync, flags = struct.unpack_from('<8i', b, o); o += 32
    o += 4
    skins = []
    for _ in range(numskins):
        g, = struct.unpack_from('<i', b, o); o += 4
        if g == 0:
            skins.append(b[o:o + sw * sh]); o += sw * sh
        else:
            n, = struct.unpack_from('<i', b, o); o += 4 + 4 * n
            for _ in range(n):
                skins.append(b[o:o + sw * sh]); o += sw * sh
    st = []
    for _ in range(nv):
        st.append(struct.unpack_from('<3i', b, o)); o += 12
    tris = []
    for _ in range(nt):
        tris.append(struct.unpack_from('<4i', b, o)); o += 16
    t, = struct.unpack_from('<i', b, o); o += 4
    if t != 0:
        n, = struct.unpack_from('<i', b, o); o += 4 + 8 + 4 * n
    o += 8 + 16
    verts = []
    for _ in range(nv):
        v = struct.unpack_from('<4B', b, o); o += 4
        verts.append(tuple(v[k] * scale[k] + trans[k] for k in range(3)))
    return sw, sh, skins, st, tris, verts

def raster(p, sw, sh):
    (x0, y0), (x1, y1), (x2, y2) = p
    out = []
    minx, maxx = int(min(x0, x1, x2)), int(max(x0, x1, x2)) + 1
    miny, maxy = int(min(y0, y1, y2)), int(max(y0, y1, y2)) + 1
    def e(ax, ay, bx, by, px, py):
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax)
    area = e(x0, y0, x1, y1, x2, y2)
    for y in range(max(0, miny - 1), min(sh, maxy + 1)):
        for x in range(max(0, minx - 1), min(sw, maxx + 1)):
            px, py = x + 0.5, y + 0.5
            w0 = e(x1, y1, x2, y2, px, py); w1 = e(x2, y2, x0, y0, px, py); w2 = e(x0, y0, x1, y1, px, py)
            if area == 0:
                continue
            if (w0 >= -abs(area) * 0.02 and w1 >= -abs(area) * 0.02 and w2 >= -abs(area) * 0.02) or \
               (w0 <= abs(area) * 0.02 and w1 <= abs(area) * 0.02 and w2 <= abs(area) * 0.02):
                out.append((x, y))
    return out

def main(path):
    sw, sh, skins, st, tris, verts = load(path)
    skin = skins[0]
    owner = {}
    for ff, a, bb, c in tris:
        p = []
        for vi in (a, bb, c):
            on, s, t = st[vi]
            if not ff and on:
                s += sw // 2
            p.append((s, t))
        cen = tuple(sum(verts[vi][k] for vi in (a, bb, c)) / 3 for k in range(3))
        for (x, y) in raster(p, sw, sh):
            if skin[y * sw + x] in SIGHT:
                owner.setdefault((x, y), []).append(cen)
    sight = {(x, y) for y in range(sh) for x in range(sw) if skin[y * sw + x] in SIGHT}
    seen = set()
    comps = []
    for px in sorted(sight):
        if px in seen:
            continue
        stack = [px]; seen.add(px); comp = []
        while stack:
            x, y = stack.pop(); comp.append((x, y))
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    q = (x + dx, y + dy)
                    if q in sight and q not in seen:
                        seen.add(q); stack.append(q)
        comps.append(comp)
    print('//', path, sw, sh, 'skins', len(skins), 'sight texels', len(sight), 'mapped', len(owner))
    front = None
    for comp in sorted(comps, key=len, reverse=True):
        xs = [p[0] for p in comp]; ys = [p[1] for p in comp]
        cs = [c for p in comp for c in owner.get(p, [])]
        rect = (min(xs), min(ys), max(xs), max(ys))
        if cs:
            m = tuple(sum(c[k] for c in cs) / len(cs) for k in range(3))
            ms = '(%.1f %.1f %.1f)' % m
            if front is None or m[0] > front[0]:
                front = (m[0], rect)
        else:
            ms = 'unmapped'
        if len(comp) >= 3 or cs:
            print('//   patch of %4d texels: x %d..%d y %d..%d, model %s' % (len(comp), rect[0], rect[2], rect[1], rect[3], ms))
    return sw, sh, front[1] if front else None


# Usage (from quakevr/progs): python ../../Misc/quakevr/sight_front_posts.py v_shot.mdl vr_pump_on_v_shot.mdl ...
# Prints vr_sights.cpp's frontPosts rows: each skin's sight patch furthest forward (+x, the barrel's way) is the front
# post; a part model whose skin maps no sight (vr_<part>_on_<gun>.mdl: the pump's fore-end, the broken-open frame)
# takes its gun's patch.
known = {}
for p in sys.argv[1:]:
    sw, sh, rect = main(p)
    base = p.split('_on_')[-1] if '_on_' in p else None
    if rect is None and base in known:
        rect = known[base]
    if rect is None:
        print('// %s: no front post' % p)
        continue
    known[p] = rect
    print('    {"progs/%s", %d, %d, %d, %d, %d, %d},' % (p, sw, sh, rect[0], rect[1], rect[2], rect[3]))
