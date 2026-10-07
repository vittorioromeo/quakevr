# mapgeom.py -- geometry for the script-generated maps (vrstart_gen.py): exact convex brushes from points, prisms
# over a triangulated height field, Valve 220 texture axes, Perlin noise, a Delaunay triangulation, and the .map
# writer. Pure Python (no numpy): the machines that build the maps have none.
#
# Precision: every brush point is snapped to 1/8 unit (exact in binary floating point) and the planes are found with
# exact integer arithmetic (the points times 8), so the faces qbsp reads are exactly the ones meant: neighbouring
# brushes that share points share planes. (Terrain prisms' tops are the exception: their heights are exact reals, written
# with 6 decimals, from terrain_mesh, so that neighbouring tops are exactly coplanar or clearly not.) ericw-tools 2.0's
# qbsp loses faces at slivers (terrain_mesh, unbend, hull's _folds, _axial and SURFACES are there for it).
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


def fmt_precise(v):
    if v == int(v):
        return str(int(v))
    s = ("%.6f" % v).rstrip("0").rstrip(".")
    return "0" if s == "-0" else s


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

    def __init__(self, faces, precise=False):
        self.faces = faces
        self.precise = precise  # its points written as they are (6 decimals), not snapped to 1/SNAP

    def text(self):
        out = ["{"]
        f = fmt_precise if self.precise else fmt
        for p0, p1, p2, (name, u, v, uo, vo, su, sv) in self.faces:
            pts = " ".join("( %s %s %s )" % (f(p[0]), f(p[1]), f(p[2])) for p in (p0, p1, p2))
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


# Surfaces a brush's corners keep clear of: (z(x, y) -> the surface's height there or None, sink). A corner within
# SETTLE_NEAR of one (but not on it) is moved SETTLE_DEPTH below it (sink) or to that side of it: a corner that pokes
# through the ground or the water by a fraction of a unit makes slivers ericw-tools 2.0's qbsp loses faces at.
SURFACES = []
SETTLE_NEAR = 1.5
SETTLE_DEPTH = 2.0
SETTLE_STATS = {"moved": 0}


def _settle(p):
    x, y, z = p
    for zfn, sink in SURFACES:
        g = zfn(x, y)
        if g is None:
            continue
        dz = z - g
        if 1e-3 < abs(dz) < SETTLE_NEAR:
            z = g - SETTLE_DEPTH if sink or dz < 0 else g + SETTLE_DEPTH
            SETTLE_STATS["moved"] += 1
    return (x, y, z)


class MeshSurface:
    """The height of a triangulated height field (points P (x, y), triangles, heights H) at (x, y), or None outside."""
    CELL = 64

    def __init__(self, P, tris, H):
        self.P, self.tris, self.H = P, tris, H
        self.grid = {}
        C = self.CELL
        for ti, (a, b, c) in enumerate(tris):
            xs = (P[a][0], P[b][0], P[c][0])
            ys = (P[a][1], P[b][1], P[c][1])
            for i in range(int(min(xs) // C), int(max(xs) // C) + 1):
                for j in range(int(min(ys) // C), int(max(ys) // C) + 1):
                    self.grid.setdefault((i, j), []).append(ti)

    def _uv(self, ti, x, y):
        P = self.P
        a, b, c = self.tris[ti]
        (x0, y0), (x1, y1), (x2, y2) = P[a], P[b], P[c]
        det = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        return ((x - x0) * (y2 - y0) - (x2 - x0) * (y - y0)) / det, ((x1 - x0) * (y - y0) - (x - x0) * (y1 - y0)) / det

    def locate(self, x, y):
        """The triangle (index) that holds (x, y), or None."""
        for ti in self.grid.get((int(x // self.CELL), int(y // self.CELL)), ()):
            u, v = self._uv(ti, x, y)
            if u >= -1e-9 and v >= -1e-9 and u + v <= 1 + 1e-9:
                return ti
        return None

    def at(self, ti, x, y):
        H = self.H
        a, b, c = self.tris[ti]
        u, v = self._uv(ti, x, y)
        return H[a] + u * (H[b] - H[a]) + v * (H[c] - H[a])

    def __call__(self, x, y):
        ti = self.locate(x, y)
        return None if ti is None else self.at(ti, x, y)


def hull(points, tex, tex_for=None):
    """The convex hull of `points` as a brush (None if flat). `tex`: a Tex, a name or a TexFn for every face;
    `tex_for(normal, centre)` -> a Tex/name/None overrides it per face. Corners near the SURFACES are settled first."""
    texfn = as_texfn(tex)
    if SURFACES:
        points = [_settle(p) for p in points]
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
    ons = {key: [q for q in pts if key[0] * q[0] + key[1] * q[1] + key[2] * q[2] == key[3]] for key in planes}
    for key in _folds(ons):
        del planes[key]
    _axial(planes, ons)
    faces = []
    for key, nn in planes.items():
        on = ons[key]
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


AXIS_SNAP = 1.5   # degrees: a face this close to facing along an axis (but not quite)...
AXIS_DIST = 0.75  # ...is turned to face along it, if the brush grows by at most this many units for it (_axial)
AXIS_STATS = {"snapped": 0}


def _axial(planes, ons):
    """Turns the faces of a hull ({plane key: normal}, {plane key: its points}; 1/SNAP units) that nearly face along
    an axis (a beam rising a unit over its length, a log's flank, a rock's flat) to face exactly along it, through
    the face's outermost corner (the brush grows, by at most AXIS_DIST). Such a face beside the map's many axial
    faces (the flat ground, the boxes) is a pair of nearly parallel planes meeting far off in a sliver."""
    cosmax = math.cos(math.radians(AXIS_SNAP))
    unit = {}
    for k in planes:
        L = math.sqrt(k[0] * k[0] + k[1] * k[1] + k[2] * k[2])
        unit[k] = (k[0] / L, k[1] / L, k[2] / L, k[3] / (L * SNAP))
    for r in list(planes):
        u = unit[r]
        ax = max(range(3), key=lambda i: abs(u[i]))
        if abs(u[ax]) < cosmax or abs(u[ax]) == 1.0:
            continue
        n = [0, 0, 0]
        n[ax] = 1 if u[ax] > 0 else -1
        d = max(n[ax] * q[ax] for q in ons[r])
        nk = (n[0], n[1], n[2], d)
        nu = (float(n[0]), float(n[1]), float(n[2]), d / SNAP)
        # the corners round the turned face: from it and the faces next to the old one
        sr = set(ons[r])
        near = [k for k in planes if k != r and set(ons[k]) & sr]
        rest = [unit[k] for k in planes if k != r] + [nu]
        worst = None
        for x in range(len(near)):
            for y in range(x + 1, len(near)):
                v = _meet(unit[near[x]], unit[near[y]], nu)
                if v is None or any(q[0] * v[0] + q[1] * v[1] + q[2] * v[2] - q[3] > 1e-4 for q in rest):
                    continue
                out = max(unit[k][0] * v[0] + unit[k][1] * v[1] + unit[k][2] * v[2] - unit[k][3] for k in unit)
                worst = out if worst is None else max(worst, out)
        if worst is None or worst > AXIS_DIST:
            continue
        pts = [tuple(n[ax] * d if i == ax else q[i] for i in range(3)) for q in ons[r]]  # (d is along n)
        del planes[r]
        if nk in planes:
            ons[nk] = sorted(set(ons[nk]) | set(pts))
        else:
            planes[nk] = tuple(n)
            ons[nk] = sorted(set(pts))
        unit[nk] = nu
        AXIS_STATS["snapped"] += 1


FOLD_ANGLE = 3.0  # degrees: neighbouring faces of a brush closer than this to coplanar...
FOLD_DIST = 0.5   # ...are one face, if the brush grows by at most this many units for it (_folds)


def _folds(ons):
    """The planes to drop from a hull whose faces are `ons` ({plane key: its points}, in 1/SNAP units): of each pair
    of neighbouring faces nearly coplanar (within FOLD_ANGLE: points snapped to the grid fold a flat quad or a cap
    into two faces a fraction of a degree apart), the smaller, if the brush without its plane reaches at most FOLD_DIST
    beyond the hull. Such folds are slivers thinner than ericw-tools 2.0's qbsp's epsilons: it loses faces there
    (holes; see terrain_mesh)."""
    keys = list(ons)
    unit = {}
    for k in keys:
        L = math.sqrt(k[0] * k[0] + k[1] * k[1] + k[2] * k[2])
        unit[k] = (k[0] / L, k[1] / L, k[2] / L, k[3] / (L * SNAP))
    sets = {k: set(ons[k]) for k in keys}

    def area(k):
        on = ons[k]
        best = 0
        for i in range(1, len(on) - 1):
            for j in range(i + 1, len(on)):
                c = cross(sub(on[i], on[0]), sub(on[j], on[0]))
                best = max(best, dot(c, c))
        return best

    cosmax = math.cos(math.radians(FOLD_ANGLE))
    cands = []
    for i in range(len(keys)):
        for j in range(i + 1, len(keys)):
            a, b = unit[keys[i]], unit[keys[j]]
            c = a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
            if c > cosmax and len(sets[keys[i]] & sets[keys[j]]) >= 2:
                cands.append((-c, keys[i], keys[j]))
    if not cands:
        return []
    cands.sort()
    kept = set(keys)
    dropped = []
    for _, k1, k2 in cands:
        if k1 not in kept or k2 not in kept:
            continue
        r, partner = (k1, k2) if area(k1) < area(k2) else (k2, k1)
        # the new corners near r: from the planes round it and its partner
        near = [k for k in kept if k != r and (sets[k] & sets[r] or sets[k] & sets[partner])]
        rest = [unit[k] for k in kept if k != r]
        worst = None
        for x in range(len(near)):
            for y in range(x + 1, len(near)):
                for z in range(y + 1, len(near)):
                    v = _meet(unit[near[x]], unit[near[y]], unit[near[z]])
                    if v is None or any(q[0] * v[0] + q[1] * v[1] + q[2] * v[2] - q[3] > 1e-4 for q in rest):
                        continue
                    out = max(unit[k][0] * v[0] + unit[k][1] * v[1] + unit[k][2] * v[2] - unit[k][3] for k in keys)
                    worst = out if worst is None else max(worst, out)
        if worst is not None and worst <= FOLD_DIST:
            kept.discard(r)
            dropped.append(r)
        else:
            FOLD_STATS["kept"] += 1
    FOLD_STATS["dropped"] += len(dropped)
    return dropped


FOLD_STATS = {"dropped": 0, "kept": 0}


def _meet(a, b, c):
    """The point on the three planes (n, d), or None."""
    det = (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0]))
    if abs(det) < 1e-9:
        return None
    x = (a[3] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[3] * c[2] - b[2] * c[3]) + a[2] * (b[3] * c[1] - b[1] * c[3])) / det
    y = (a[0] * (b[3] * c[2] - b[2] * c[3]) - a[3] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[3] - b[3] * c[0])) / det
    z = (a[0] * (b[1] * c[3] - b[3] * c[1]) - a[1] * (b[0] * c[3] - b[3] * c[0]) + a[3] * (b[0] * c[1] - b[1] * c[0])) / det
    return (x, y, z)


def box(x0, y0, z0, x1, y1, z1, tex, tex_for=None):
    x0, x1 = min(x0, x1), max(x0, x1)
    y0, y1 = min(y0, y1), max(y0, y1)
    z0, z1 = min(z0, z1), max(z0, z1)
    return hull([(x, y, z) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)], tex, tex_for)


def prism(poly, zb, top_tex, side_tex):
    """A terrain prism: the convex polygon `poly` ((x, y, z) counter-clockwise from above, x and y integers, every
    point on one plane) on top, vertical sides down to z `zb`. The top's heights are written as they are (not snapped:
    terrain_mesh's are exact reals, see there); its plane is given by the three corners that span the largest
    triangle."""
    m = len(poly)
    best, ba = None, -1
    for i in range(m):
        for j in range(i + 1, m):
            for k in range(j + 1, m):
                ar = abs(_orient2(poly[i], poly[j], poly[k]))
                if ar > ba:
                    ba, best = ar, (poly[i], poly[j], poly[k])
    a, b, c = best
    nt = norm(cross(sub(b, a), sub(c, a)))
    if nt[2] < 0:
        nt = mul(nt, -1)
    centre = tuple(sum(p[i] for p in poly) / m for i in range(3))
    faces = [_orient(a, b, c, nt) + (as_texfn(top_tex)(nt, centre),)]
    nb = (0.0, 0.0, -1.0)
    faces.append(_orient((a[0], a[1], zb), (b[0], b[1], zb), (c[0], c[1], zb), nb) + (as_texfn(side_tex)(nb, (a[0], a[1], zb)),))
    for i in range(m):
        p, q = poly[i], poly[(i + 1) % m]
        ns = norm((q[1] - p[1], -(q[0] - p[0]), 0.0))
        pts = ((p[0], p[1], zb), (q[0], q[1], zb), (p[0], p[1], zb + 64))
        faces.append(_orient(*pts, ns) + (as_texfn(side_tex)(ns, ((p[0] + q[0]) / 2, (p[1] + q[1]) / 2, zb)),))
    return Brush(faces, precise=True)


# ---- terrain meshes for ericw-tools 2.0's qbsp
# Its faces come from the BSP's portals: a portal whose brush side it cannot find makes no face (its "N sides not
# found"), a hole in the map. That happens at slivers thinner than its epsilons: two neighbouring tops nearly but not
# exactly coplanar (the wedge between their planes) or a top a fraction of a unit off its neighbour along their edge
# (a step). terrain_mesh() makes a height field that has neither: every prism's top passes through its own corners
# (neighbours meet exactly: no steps), and neighbours are either exactly coplanar or clearly apart (the corners of a
# nearly coplanar pair moved onto one plane, their heights then exact reals, not grid points). Exactly coplanar
# neighbours of the same texture are then merged into convex polygons: one prism each (fewer brushes, fewer planes).
EXACT = 1e-6  # units: closer than this to a plane is on it


def _plane3(P, H, t):
    """z = a x + b y + c through the triangle t's corners."""
    i, j, k = t
    x0, y0 = P[i]
    x1, y1 = P[j]
    x2, y2 = P[k]
    z0, z1, z2 = H[i], H[j], H[k]
    det = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
    a = ((z1 - z0) * (y2 - y0) - (z2 - z0) * (y1 - y0)) / det
    b = ((x1 - x0) * (z2 - z0) - (x2 - x0) * (z1 - z0)) / det
    return a, b, z0 - a * x0 - b * y0


def _angle(p, q):
    """The angle (radians) between the planes z = p and z = q."""
    n1 = (-p[0], -p[1], 1.0)
    n2 = (-q[0], -q[1], 1.0)
    c = dot(n1, n2) / (length(n1) * length(n2))
    return math.acos(max(-1.0, min(1.0, c)))


def unbend(P, fixed, bend=6.0, rounds=8, seed=1, max_move=6.0):
    """Moves points of P (integer (x, y), unique) so that no point of the Delaunay triangulation has two edges nearly
    in line through it (straight within `bend` degrees but not exactly): the prisms' vertical sides along them are
    then nearly parallel planes crossing at the point, a sliver ericw-tools 2.0's qbsp mishandles (a thin slab of
    phantom solid standing out of the ground, missing faces). A point is moved onto the line exactly if an integer
    point within 2 units is on it, else away from it (2 to 4 units), never further than `max_move` from where it
    was; the indices in `fixed` (the box's edge) stay.
    Returns (points, how many moves, how many such points are left)."""
    P = list(P)
    P0 = list(P)
    rnd = random.Random(seed)
    cosb = math.cos(math.radians(bend))
    moves = 0
    left = 0
    for _ in range(rounds):
        tris = delaunay(P)
        nbrs = [set() for _ in P]
        for t in tris:
            for k in range(3):
                nbrs[t[k]].update((t[(k + 1) % 3], t[(k + 2) % 3]))
        taken = set(P)
        bad = []
        for b in range(len(P)):
            if b in fixed:
                continue
            B = P[b]
            ns = list(nbrs[b])
            worst = None
            for i in range(len(ns)):
                ax, ay = P[ns[i]][0] - B[0], P[ns[i]][1] - B[1]
                la = math.hypot(ax, ay)
                for j in range(i + 1, len(ns)):
                    cx, cy = P[ns[j]][0] - B[0], P[ns[j]][1] - B[1]
                    if ax * cy - ay * cx == 0:
                        continue  # exactly in line (or folded back): one plane
                    c = (ax * cx + ay * cy) / (la * math.hypot(cx, cy))
                    if c < -cosb and (worst is None or c < worst[0]):
                        worst = (c, ns[i], ns[j])
            if worst:
                bad.append((b, worst[1], worst[2]))
        left = len(bad)
        if not bad:
            break
        moved = set()
        for b, a, c in bad:
            if b in moved or a in moved or c in moved:
                continue
            B, A, C = P[b], P[a], P[c]
            best = None
            for dx in range(-2, 3):
                for dy in range(-2, 3):
                    q = (B[0] + dx, B[1] + dy)
                    if q not in taken and _orient2(A, C, q) == 0 and (best is None or dx * dx + dy * dy < best[0]):
                        best = (dx * dx + dy * dy, q)
            if best:
                q = best[1]
            else:
                # away from the line A-C, to the side B is on (or either)
                lx, ly = C[0] - A[0], C[1] - A[1]
                L = math.hypot(lx, ly)
                side = _orient2(A, C, B)
                sgn = 1 if side > 0 else -1 if side < 0 else rnd.choice((-1, 1))
                d = rnd.uniform(2, 4)
                q = (int(round(B[0] - sgn * ly / L * d)), int(round(B[1] + sgn * lx / L * d)))
                if q in taken:
                    continue
            if (q[0] - P0[b][0]) ** 2 + (q[1] - P0[b][1]) ** 2 > max_move * max_move:
                continue
            taken.discard(B)
            taken.add(q)
            P[b] = q
            moved.add(b)
            moves += 1
    return P, moves, left


def simplify_points(P, H, tol, keep):
    """Greedy insertion (Garland and Heckbert's): the points of P (x, y integers; heights H) a coarser height field
    needs so that none of the others is further than tol[i] from it (vertically): `keep` (indices) always, then, round
    by round, in each triangle the point furthest off it if that is more than its tolerance. Returns the indices kept."""
    S = set(keep)
    cand = [i for i in range(len(P)) if i not in S]
    while True:
        idx = sorted(S)
        tris = delaunay([P[i] for i in idx])
        tris = [tuple(idx[v] for v in t) for t in tris]
        surf = MeshSurface(P, tris, H)
        best = {}
        for i in cand:
            if i in S:
                continue
            x, y = P[i]
            ti = surf.locate(x, y)
            if ti is None:
                continue
            a, b, c = tris[ti]
            err = abs(H[i] - surf.at(ti, x, y))
            if err > tol[i] and (ti not in best or err > best[ti][0]):
                best[ti] = (err, i)
        if not best:
            return sorted(S)
        S.update(i for _, i in best.values())


def terrain_mesh(P, tris, H, key, pinned=(), near_dev=1.0, near_angle=1.5, max_drift=3.0, rounds=60, levels=(),
                 clear=2.0, near_flat=0.6):
    """P: the points (x, y) (integers), tris: their triangles (counter-clockwise), H: their heights, key(ti): what a
    triangle must share to be merged with a neighbour (its texture). A neighbouring pair is nearly coplanar when its
    planes are apart by under `near_dev` units at the far corners or `near_angle` degrees (and not EXACTly coplanar):
    a corner of such a pair is moved onto the other's plane (the move that leaves fewest nearly coplanar pairs round
    it; never a `pinned` point, never more than `max_drift` from where it was), until none are left.
    A top within `near_flat` degrees of level (not exactly) is made level first.
    `levels`: heights (the water's surface) no corner may be near: one within `clear` of a level is put on it (and
    pinned), and no move ends within `clear` of one (a corner a fraction of a unit through the water's surface is a
    sliver too). key is called with (triangle, heights) once they are final.
    Returns (heights, polygons, stats): polygons as (corner indices counter-clockwise, triangle indices)."""
    H = [float(h) for h in H]
    pinned = set(pinned)
    H0 = H[:]
    nt = len(tris)
    edge = {}
    vtris = [[] for _ in P]
    for ti, t in enumerate(tris):
        for k in range(3):
            edge[(t[k], t[(k + 1) % 3])] = ti
            vtris[t[k]].append(ti)
    pairs = []  # (ti, tj, apex of ti, apex of tj)
    for (a, b), ti in edge.items():
        tj = edge.get((b, a))
        if tj is not None and ti < tj:
            c = next(v for v in tris[ti] if v not in (a, b))
            d = next(v for v in tris[tj] if v not in (a, b))
            pairs.append((ti, tj, c, d))
    tpairs = [[] for _ in range(nt)]
    for pi, (ti, tj, c, d) in enumerate(pairs):
        tpairs[ti].append(pi)
        tpairs[tj].append(pi)
    pl = [_plane3(P, H, t) for t in tris]
    # near a level: a corner's clearance is `clear` units of height, or 4 units across the slope where that is more
    # (on a cliff a corner 2 units over the water meets the waterline a fraction of a unit away: a sliver), at most 12
    vclear = []
    vdrift = []
    for i in range(len(P)):
        g = max((math.hypot(pl[t][0], pl[t][1]) for t in vtris[i]), default=0.0)
        vclear.append(min(12.0, max(clear, 4.0 * g)))
        vdrift.append(max_drift * min(5.0, math.sqrt(1 + g * g)))  # (up and down a cliff: as far across it)
    for i, h in enumerate(H):
        for lv in levels:
            if abs(h - lv) < vclear[i]:
                H[i] = float(lv)
                pinned.add(i)
    H0 = H[:]
    pl = [_plane3(P, H, t) for t in tris]
    ang = math.radians(near_angle)

    def zat(p, v):
        return p[0] * P[v][0] + p[1] * P[v][1] + p[2]

    def dev(pi):
        ti, tj, c, d = pairs[pi]
        return max(abs(H[d] - zat(pl[ti], d)), abs(H[c] - zat(pl[tj], c)))

    def near(pi):
        g = dev(pi)
        if g < EXACT:
            return False
        ti, tj = pairs[pi][0], pairs[pi][1]
        # apart across the planes (on a cliff a vertical step is mostly along the face)
        a, b, _ = pl[ti] if abs(pl[ti][0]) + abs(pl[ti][1]) > abs(pl[tj][0]) + abs(pl[tj][1]) else pl[tj]
        return g / math.sqrt(1 + a * a + b * b) < near_dev or _angle(pl[ti], pl[tj]) < ang

    def local(v):
        s = set()
        for ti in vtris[v]:
            s.update(tpairs[ti])
        return s

    flat_ang = math.radians(near_flat)

    def tilted(t):
        """A top nearly but not exactly level (beside the map's level planes: the flat places, the boxes' tops)."""
        a, b, _ = pl[t]
        return 0 < a * a + b * b and math.atan(math.hypot(a, b)) < flat_ang

    moves = 0
    # nearly level tops made level: their corners to the middle one's height
    for _ in range(4):
        for t in range(nt):
            if not tilted(t):
                continue
            hs = sorted(H[v] for v in tris[t])
            z = hs[1]
            for v in tris[t]:
                if v in pinned or abs(z - H0[v]) > vdrift[v] or any(abs(z - lv) < vclear[v] for lv in levels):
                    continue
                H[v] = z
                for t2 in vtris[v]:
                    pl[t2] = _plane3(P, H, tris[t2])
                moves += 1
    for _ in range(rounds):
        todo = sorted((dev(pi), pi) for pi in range(len(pairs)) if near(pi))
        if not todo:
            break
        for _, pi in todo:
            if not near(pi):
                continue
            ti, tj, c, d = pairs[pi]
            best = None
            for v, z in ((d, zat(pl[ti], d)), (c, zat(pl[tj], c))):
                if v in pinned or abs(z - H0[v]) > vdrift[v] or any(abs(z - lv) < vclear[v] for lv in levels):
                    continue
                old = H[v]
                around = local(v)
                H[v] = z
                for t in vtris[v]:
                    pl[t] = _plane3(P, H, tris[t])
                cost = (sum(1 for q in around if near(q)) + sum(1 for t in vtris[v] if tilted(t)), abs(z - old))
                H[v] = old
                for t in vtris[v]:
                    pl[t] = _plane3(P, H, tris[t])
                if best is None or cost < best[0]:
                    best = (cost, v, z)
            if best:
                _, v, z = best
                H[v] = z
                for t in vtris[v]:
                    pl[t] = _plane3(P, H, tris[t])
                moves += 1
    left = sum(1 for pi in range(len(pairs)) if near(pi))
    left_tilted = sum(1 for t in range(nt) if tilted(t))
    drift = max(abs(H[i] - H0[i]) for i in range(len(P))) if P else 0.0
    moved = sum(1 for i in range(len(P)) if abs(H[i] - H0[i]) > EXACT)
    # groups of exactly coplanar neighbours with the same key, merged into convex polygons
    parent = list(range(nt))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for pi in range(len(pairs)):
        ti, tj, c, d = pairs[pi]
        if dev(pi) < EXACT and key(ti, H) == key(tj, H):
            parent[find(ti)] = find(tj)
    groups = {}
    for ti in range(nt):
        groups.setdefault(find(ti), []).append(ti)
    polys = []
    for members in groups.values():
        polys += _merge_convex(P, tris, members, edge)
    stats = dict(moves=moves, moved=moved, drift=drift, left=left, tilted=left_tilted, tris=nt, polys=len(polys))
    return H, polys, stats


def _cycle(edges):
    """The boundary edges (u, v) of a region as one cycle of vertices, or None (a hole, a pinch)."""
    nxt = {}
    for u, v in edges:
        if u in nxt:
            return None
        nxt[u] = v
    start = next(iter(nxt))
    cyc = [start]
    v = nxt[start]
    while v != start:
        cyc.append(v)
        v = nxt.get(v)
        if v is None or len(cyc) > len(nxt):
            return None
    return cyc if len(cyc) == len(nxt) else None


def _convex(P, cyc):
    m = len(cyc)
    for i in range(m):
        if _orient2(P[cyc[i - 1]], P[cyc[i]], P[cyc[(i + 1) % m]]) < 0:
            return False
    return True


def _merge_convex(P, tris, members, edge):
    """The triangles `members` (exactly coplanar, one texture) merged greedily into convex polygons."""
    owner = {ti: ti for ti in members}
    regions = {ti: [ti] for ti in members}
    bedges = {ti: set((tris[ti][k], tris[ti][(k + 1) % 3]) for k in range(3)) for ti in members}
    mset = set(members)

    def area(r):
        return sum(abs(_orient2(P[tris[t][0]], P[tris[t][1]], P[tris[t][2]])) for t in regions[r])

    changed = True
    while changed:
        changed = False
        for r in sorted(regions, key=lambda r: -area(r)):
            if r not in regions:
                continue
            while True:
                # neighbouring regions by shared boundary length, longest first
                nb = {}
                for (u, v) in bedges[r]:
                    t = edge.get((v, u))
                    if t is not None and t in mset:
                        o = owner[t]
                        if o != r:
                            nb[o] = nb.get(o, 0) + math.hypot(P[u][0] - P[v][0], P[u][1] - P[v][1])
                merged = False
                for o, _ in sorted(nb.items(), key=lambda kv: -kv[1]):
                    es = bedges[r] ^ bedges[o]
                    es = set(e for e in es if (e[1], e[0]) not in es)
                    cyc = _cycle(es)
                    if cyc is None or not _convex(P, cyc):
                        continue
                    bedges[r] = es
                    regions[r] += regions.pop(o)
                    del bedges[o]
                    for t in regions[r]:
                        owner[t] = r
                    merged = changed = True
                    break
                if not merged:
                    break
    out = []
    for r, ts in regions.items():
        cyc = _cycle(bedges[r])
        # drop the corners on a straight edge (their sides would be one plane twice)
        m = len(cyc)
        cyc = [cyc[i] for i in range(m) if _orient2(P[cyc[i - 1]], P[cyc[i]], P[cyc[(i + 1) % m]]) != 0]
        out.append((cyc, ts))
    return out


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

    def detail(self, name, classname="func_detail", **keys):
        """A TrenchBroom group holding one brush entity (func_detail by default): its brushes (appended to by the
        caller)."""
        self.groups.append((name, []))
        brushes = []
        k = {"classname": classname, "_tb_group": str(len(self.groups))}
        k.update(keys)
        self.add(k, brushes)
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
                if keys.get("classname") in ("func_wall", "func_detail") and not any(b is not None for b in brushes):
                    continue  # (an empty piece)
                f.write("// entity %d\n{\n" % idx)
                for k, v in keys.items():
                    f.write('"%s" "%s"\n' % (k, v))
                for i, b in enumerate(x for x in brushes if x is not None):
                    f.write("// brush %d\n%s\n" % (i, b.text()))
                f.write("}\n")
                idx += 1
