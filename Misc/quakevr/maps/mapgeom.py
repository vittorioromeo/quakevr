# mapgeom.py -- geometry for the script-generated maps (vrstart2_gen.py): exact convex brushes from points, prisms
# over a triangulated height field, Valve 220 texture axes, Perlin noise, a Delaunay triangulation, and the .map
# writer. Pure Python (no numpy): the machines that build the maps have none.
#
# Precision: every brush point is snapped to 1/8 unit (exact in binary floating point) and the planes are found with
# exact integer arithmetic (the points times 8), so the faces qbsp reads are exactly the ones meant: neighbouring
# brushes that share points share planes.
import math
import random

SNAP = 8  # points are multiples of 1/SNAP


def snap(v):
    return round(v * SNAP) / SNAP


def fmt(v):
    v = snap(v)
    if v == int(v):
        return str(int(v))
    return ("%.3f" % v).rstrip("0").rstrip(".")


# ---- vectors (tuples)
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))


def norm(a):
    l = length(a)
    return (a[0] / l, a[1] / l, a[2] / l) if l > 1e-12 else (0.0, 0.0, 0.0)


def lerp(a, b, t):
    return a + (b - a) * t


def smoothstep(e0, e1, x):
    if e0 == e1:
        return 0.0 if x < e0 else 1.0
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


# ---- textures: a face's texture is (name, U, V, uoff, voff, scale_u, scale_v); TexFn(normal, centre) -> that
class Tex:
    """A texture and how it is laid on a face: `mode` 'world' (projected along the face's main axis, as TrenchBroom's
    paraxial default), 'grain' (U along `axis`, the wood's grain, laid into the face), 'face' (U along the face's
    horizontal, V down it: cliffs, without the stretch of a world projection)."""

    def __init__(self, name, scale=1.0, mode="world", axis=None, uoff=0.0, voff=0.0, end=None, rot=0.0):
        self.name, self.scale, self.mode, self.axis = name, scale, mode, axis
        self.uoff, self.voff, self.end, self.rot = uoff, voff, end, rot

    def spec(self, n, centre):
        mode, name, sc = self.mode, self.name, self.scale
        if mode == "grain":
            g = norm(self.axis)
            gp = sub(g, mul(n, dot(g, n)))
            if length(gp) < 0.3:  # an end of the beam: its end grain, or the world projection
                if self.end:
                    return self.end.spec(n, centre)
                mode = "world"
            else:
                # id's wood textures have their grain along V (up the picture)
                v = norm(gp)
                u = norm(cross(v, n))
                return (name, u, v, self.uoff, self.voff, sc, sc)
        if mode == "face" and abs(n[2]) < 0.97:
            u = norm((-n[1], n[0], 0.0))
            v = norm(cross(n, u))
            if v[2] > 0:
                v = mul(v, -1)
            return (name, u, v, self.uoff, self.voff, sc, sc)
        ax, ay, az = abs(n[0]), abs(n[1]), abs(n[2])
        if az >= ax and az >= ay:
            u, v = (1.0, 0.0, 0.0), (0.0, -1.0, 0.0)
        elif ax >= ay:
            u, v = (0.0, 1.0, 0.0), (0.0, 0.0, -1.0)
        else:
            u, v = (1.0, 0.0, 0.0), (0.0, 0.0, -1.0)
        if self.rot:
            c, s = math.cos(math.radians(self.rot)), math.sin(math.radians(self.rot))
            u, v = add(mul(u, c), mul(v, s)), add(mul(v, c), mul(u, -s))
        return (name, u, v, self.uoff, self.voff, sc, sc)


def as_texfn(t):
    if isinstance(t, Tex):
        return t.spec
    if isinstance(t, str):
        return Tex(t).spec
    return t


# ---- brushes
class Brush:
    """A convex brush: its faces as (p0, p1, p2, spec), the points ordered so that (p0 - p1) x (p2 - p1) points out."""

    def __init__(self, faces):
        self.faces = faces

    def text(self):
        out = ["{"]
        for p0, p1, p2, (name, u, v, uo, vo, su, sv) in self.faces:
            pts = " ".join("( %s %s %s )" % (fmt(p[0]), fmt(p[1]), fmt(p[2])) for p in (p0, p1, p2))
            out.append("%s %s [ %s %s %s %s ] [ %s %s %s %s ] 0 %s %s" % (
                pts, name, _f(u[0]), _f(u[1]), _f(u[2]), _f(uo), _f(v[0]), _f(v[1]), _f(v[2]), _f(vo), _f(su), _f(sv)))
        out.append("}")
        return "\n".join(out)


def _f(x):
    if abs(x - round(x)) < 1e-9:
        return str(int(round(x)))
    return ("%.6f" % x).rstrip("0").rstrip(".")


def _orient(p0, p1, p2, outward):
    n = cross(sub(p0, p1), sub(p2, p1))
    return (p0, p1, p2) if dot(n, outward) > 0 else (p2, p1, p0)


def hull(points, tex, tex_for=None):
    """The convex hull of `points` as a brush (None if flat). `tex`: a Tex, a name or a TexFn for every face;
    `tex_for(normal, centre)` -> a Tex/name/None overrides it per face."""
    texfn = as_texfn(tex)
    pts = sorted(set((round(p[0] * SNAP), round(p[1] * SNAP), round(p[2] * SNAP)) for p in points))
    n = len(pts)
    if n < 4:
        return None
    planes = {}
    for i in range(n):
        pi = pts[i]
        for j in range(i + 1, n):
            pj = pts[j]
            a = (pj[0] - pi[0], pj[1] - pi[1], pj[2] - pi[2])
            for k in range(j + 1, n):
                pk = pts[k]
                b = (pk[0] - pi[0], pk[1] - pi[1], pk[2] - pi[2])
                nn = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
                if nn == (0, 0, 0):
                    continue
                d = nn[0] * pi[0] + nn[1] * pi[1] + nn[2] * pi[2]
                pos = neg = False
                for q in pts:
                    s = nn[0] * q[0] + nn[1] * q[1] + nn[2] * q[2] - d
                    if s > 0:
                        pos = True
                    elif s < 0:
                        neg = True
                    if pos and neg:
                        break
                if pos and neg:
                    continue
                if pos:  # all on the positive side: the outward normal is -nn
                    nn, d = (-nn[0], -nn[1], -nn[2]), -d
                g = math.gcd(math.gcd(abs(nn[0]), abs(nn[1])), math.gcd(abs(nn[2]), abs(d)))
                key = (nn[0] // g, nn[1] // g, nn[2] // g, d // g)
                if key not in planes:
                    planes[key] = nn
    if len(planes) < 4:
        return None
    faces = []
    for key, nn in planes.items():
        d = key[3]
        on = [q for q in pts if key[0] * q[0] + key[1] * q[1] + key[2] * q[2] == d]
        # the three points on the plane that span the largest triangle
        best, ba = None, -1
        m = len(on)
        for i in range(m):
            for j in range(i + 1, m):
                for k in range(j + 1, m):
                    c = cross(sub(on[j], on[i]), sub(on[k], on[i]))
                    ar = dot(c, c)
                    if ar > ba:
                        ba, best = ar, (on[i], on[j], on[k])
        p0, p1, p2 = [tuple(c / SNAP for c in p) for p in best]
        normal = norm(nn)
        p0, p1, p2 = _orient(p0, p1, p2, normal)
        centre = tuple(sum(q[i] for q in on) / (len(on) * SNAP) for i in range(3))
        fn = texfn
        if tex_for:
            t = tex_for(normal, centre)
            if t is not None:
                fn = as_texfn(t)
        faces.append((p0, p1, p2, fn(normal, centre)))
    return Brush(faces)


def box(x0, y0, z0, x1, y1, z1, tex, tex_for=None):
    x0, x1 = min(x0, x1), max(x0, x1)
    y0, y1 = min(y0, y1), max(y0, y1)
    z0, z1 = min(z0, z1), max(z0, z1)
    return hull([(x, y, z) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)], tex, tex_for)


def prism(tri, zb, top_tex, side_tex):
    """A terrain prism: the triangle `tri` ((x, y, z) x 3, integers) on top, vertical sides down to z `zb`."""
    a, b, c = tri
    # counter-clockwise from above
    if (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]) < 0:
        b, c = c, b
    faces = []
    nt = norm(cross(sub(b, a), sub(c, a)))
    faces.append(_orient(a, b, c, nt) + (as_texfn(top_tex)(nt, tuple((a[i] + b[i] + c[i]) / 3 for i in range(3))),))
    nb = (0.0, 0.0, -1.0)
    faces.append(_orient((a[0], a[1], zb), (b[0], b[1], zb), (c[0], c[1], zb), nb) + (as_texfn(side_tex)(nb, (a[0], a[1], zb)),))
    for p, q in ((a, b), (b, c), (c, a)):
        ns = norm((q[1] - p[1], -(q[0] - p[0]), 0.0))
        pts = ((p[0], p[1], zb), (q[0], q[1], zb), (p[0], p[1], zb + 64))
        faces.append(_orient(*pts, ns) + (as_texfn(side_tex)(ns, ((p[0] + q[0]) / 2, (p[1] + q[1]) / 2, zb)),))
    return Brush(faces)


def ngon(cx, cy, r, sides, phase=0.0):
    return [(cx + r * math.cos(phase + 2 * math.pi * i / sides), cy + r * math.sin(phase + 2 * math.pi * i / sides))
            for i in range(sides)]


def cylinder(p, q, r, sides, tex, phase=0.0):
    """A prism of `sides` round the segment p..q (radius r): posts, logs, rungs, pilings."""
    axis = norm(sub(q, p))
    ref = (0.0, 0.0, 1.0) if abs(axis[2]) < 0.9 else (1.0, 0.0, 0.0)
    e1 = norm(cross(axis, ref))
    e2 = cross(axis, e1)
    pts = []
    for i in range(sides):
        a = phase + 2 * math.pi * (i + 0.5) / sides
        off = add(mul(e1, r * math.cos(a)), mul(e2, r * math.sin(a)))
        pts.append(add(p, off))
        pts.append(add(q, off))
    return hull(pts, tex)


def beam(p, q, w, h, tex, up=(0.0, 0.0, 1.0)):
    """A rectangular beam from p to q, `w` wide and `h` tall (its `up` side up)."""
    axis = norm(sub(q, p))
    side = norm(cross(axis, up))
    if length(side) < 0.5:
        side = norm(cross(axis, (1.0, 0.0, 0.0)))
    upv = cross(side, axis)
    pts = []
    for s in (-0.5, 0.5):
        for t in (-0.5, 0.5):
            off = add(mul(side, s * w), mul(upv, t * h))
            pts.append(add(p, off))
            pts.append(add(q, off))
    return hull(pts, tex)


# ---- noise
class Perlin:
    def __init__(self, seed):
        rnd = random.Random(seed)
        p = list(range(256))
        rnd.shuffle(p)
        self.p = p + p
        self.g = [(math.cos(2 * math.pi * i / 16), math.sin(2 * math.pi * i / 16)) for i in range(16)]

    def __call__(self, x, y):
        xi, yi = math.floor(x), math.floor(y)
        xf, yf = x - xi, y - yi
        xi &= 255
        yi &= 255
        p, g = self.p, self.g

        def grad(h, dx, dy):
            gx, gy = g[h & 15]
            return gx * dx + gy * dy

        u = xf * xf * xf * (xf * (xf * 6 - 15) + 10)
        v = yf * yf * yf * (yf * (yf * 6 - 15) + 10)
        aa = p[p[xi] + yi]
        ab = p[p[xi] + yi + 1]
        ba = p[p[xi + 1] + yi]
        bb = p[p[xi + 1] + yi + 1]
        x1 = lerp(grad(aa, xf, yf), grad(ba, xf - 1, yf), u)
        x2 = lerp(grad(ab, xf, yf - 1), grad(bb, xf - 1, yf - 1), u)
        return lerp(x1, x2, v) * 1.4  # about -1..1

    def fbm(self, x, y, octaves=4, lac=2.0, gain=0.5):
        s, a, f = 0.0, 1.0, 1.0
        for _ in range(octaves):
            s += a * self(x * f, y * f)
            f *= lac
            a *= gain
        return s


# ---- Delaunay triangulation (Bowyer-Watson, exact integer predicates, walking point location)
def _orient2(a, b, c):
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


def _incircle(a, b, c, d):
    adx, ady = a[0] - d[0], a[1] - d[1]
    bdx, bdy = b[0] - d[0], b[1] - d[1]
    cdx, cdy = c[0] - d[0], c[1] - d[1]
    ad = adx * adx + ady * ady
    bd = bdx * bdx + bdy * bdy
    cd = cdx * cdx + cdy * cdy
    return (adx * (bdy * cd - bd * cdy) - ady * (bdx * cd - bd * cdx) + ad * (bdx * cdy - bdy * cdx))


def delaunay(points):
    """Triangles (index triples, counter-clockwise) of the integer points `points` (unique)."""
    P = list(points)
    n = len(P)
    M = 1 << 22
    P += [(-M, -M), (M, -M), (0, M)]
    T = [[n, n + 1, n + 2]]
    N = [[-1, -1, -1]]
    alive = [True]
    order = sorted(range(n), key=lambda i: (_hilbert(P[i][0], P[i][1])))
    last = 0
    for pi in order:
        p = P[pi]
        # walk to the triangle that holds p
        t = last if alive[last] else next(i for i in range(len(T) - 1, -1, -1) if alive[i])
        steps = 0
        while True:
            steps += 1
            tri = T[t]
            moved = False
            for e in range(3):
                a, b = P[tri[(e + 1) % 3]], P[tri[(e + 2) % 3]]
                if _orient2(a, b, p) < 0 and N[t][e] >= 0:
                    t = N[t][e]
                    moved = True
                    break
            if not moved or steps > 100000:
                break
        # the cavity: triangles whose circumcircle holds p
        cav = {t}
        stack = [t]
        while stack:
            c = stack.pop()
            for e in range(3):
                nb = N[c][e]
                if nb >= 0 and nb not in cav:
                    a, b, cc = (P[v] for v in T[nb])
                    if _incircle(a, b, cc, p) > 0:
                        cav.add(nb)
                        stack.append(nb)
        # its boundary edges (a, b) as seen from inside, with the triangle outside each
        bound = []
        for c in cav:
            for e in range(3):
                nb = N[c][e]
                if nb < 0 or nb not in cav:
                    bound.append((T[c][(e + 1) % 3], T[c][(e + 2) % 3], nb, c))
        for c in cav:
            alive[c] = False
        edge_owner = {}
        new = []
        for a, b, outside, old in bound:
            ti = len(T)
            T.append([pi, a, b])
            N.append([outside, -1, -1])
            alive.append(True)
            new.append(ti)
            if outside >= 0:
                for e in range(3):
                    if N[outside][e] == old:
                        N[outside][e] = ti
            edge_owner[(a, pi)] = (ti, 2)  # the edge (a, pi) is opposite b: index 2
            edge_owner[(pi, b)] = (ti, 1)  # the edge (pi, b)... opposite a: index 1
        for ti in new:
            _, a, b = T[ti]
            # neighbour across (pi, b) [index 1] is the new triangle that has edge (b, pi) as its (a, pi)
            o = edge_owner.get((b, pi))
            if o:
                N[ti][1] = o[0]
            o = edge_owner.get((pi, a))
            if o:
                N[ti][2] = o[0]
        last = new[0]
    return [tuple(T[i]) for i in range(len(T)) if alive[i] and max(T[i]) < n]


def _hilbert(x, y, order=16):
    x = (x + 32768) & 0xFFFF
    y = (y + 32768) & 0xFFFF
    d = 0
    s = 1 << (order - 1)
    while s > 0:
        rx = 1 if (x & s) else 0
        ry = 1 if (y & s) else 0
        d += s * s * ((3 * rx) ^ ry)
        if ry == 0:
            if rx == 1:
                x = s - 1 - x
                y = s - 1 - y
            x, y = y, x
        s >>= 1
    return d


# ---- polygons and distances (2D)
def point_in_poly(x, y, poly):
    inside = False
    n = len(poly)
    j = n - 1
    for i in range(n):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            inside = not inside
        j = i
    return inside


def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    l2 = dx * dx + dy * dy
    t = 0.0 if l2 == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / l2))
    qx, qy = ax + t * dx, ay + t * dy
    return math.hypot(px - qx, py - qy), t


def polyline_dist(px, py, pts):
    """(distance, index of the segment, t along it) to an open polyline."""
    best = (1e18, 0, 0.0)
    for i in range(len(pts) - 1):
        d, t = seg_dist(px, py, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1])
        if d < best[0]:
            best = (d, i, t)
    return best


def catmull_rom(ctrl, per_seg, closed=True):
    out = []
    n = len(ctrl)
    rng = range(n) if closed else range(n - 1)
    for i in rng:
        p0 = ctrl[(i - 1) % n] if closed else ctrl[max(i - 1, 0)]
        p1 = ctrl[i]
        p2 = ctrl[(i + 1) % n] if closed else ctrl[i + 1]
        p3 = ctrl[(i + 2) % n] if closed else ctrl[min(i + 2, n - 1)]
        for s in range(per_seg):
            t = s / per_seg
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1[k]) + (-p0[k] + p2[k]) * t + (2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k]) * t2
                                    + (-p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k]) * t3) for k in range(len(p1))))
    if not closed:
        out.append(tuple(ctrl[-1]))
    return out


# ---- the .map file
class MapWriter:
    def __init__(self):
        self.world = []          # brushes of the worldspawn
        self.groups = []         # (name, brushes): TrenchBroom groups (func_group, merged into the world by qbsp)
        self.entities = []       # (keys, brushes)

    def group(self, name):
        g = (name, [])
        self.groups.append(g)
        return g[1]

    def detail(self, name):
        """A TrenchBroom group holding one func_detail: its brushes (appended to by the caller)."""
        self.groups.append((name, []))
        brushes = []
        self.add({"classname": "func_detail", "_tb_group": str(len(self.groups))}, brushes)
        return brushes

    def add(self, keys, brushes=()):
        self.entities.append((dict(keys), brushes))  # (filled later by the caller: read at write time)

    def write(self, path, world_keys, header):
        with open(path, "w", newline="\n") as f:
            f.write(header)
            f.write("// entity 0\n{\n")
            for k, v in world_keys.items():
                f.write('"%s" "%s"\n' % (k, v))
            for i, b in enumerate(x for x in self.world if x is not None):
                f.write("// brush %d\n%s\n" % (i, b.text()))
            f.write("}\n")
            idx = 1
            for gid, (name, brushes) in enumerate(self.groups, 1):
                f.write('// entity %d\n{\n"classname" "func_group"\n"_tb_type" "_tb_group"\n"_tb_name" "%s"\n"_tb_id" "%d"\n'
                        % (idx, name, gid))
                for i, b in enumerate(x for x in brushes if x is not None):
                    f.write("// brush %d\n%s\n" % (i, b.text()))
                f.write("}\n")
                idx += 1
            for keys, brushes in self.entities:
                f.write("// entity %d\n{\n" % idx)
                for k, v in keys.items():
                    f.write('"%s" "%s"\n' % (k, v))
                for i, b in enumerate(x for x in brushes if x is not None):
                    f.write("// brush %d\n%s\n" % (i, b.text()))
                f.write("}\n")
                idx += 1
