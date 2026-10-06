# make_vrslopes_map.py [--compile]: writes quakevr/maps/vrslopes.map, the sloped-mantle test map (ROUND21.md, "Climbing:
# mantling onto sloping tops"), and with --compile builds it (qbsp, vis, light; C:\OHWorkspace\ericw-tools-2.0.0-alpha11-win64).
# A row of blocks along x, each with its lip at y 0 and z 48 (as vrclimb's long ledge: `setpos <x> -18 24 0 90 0`,
# facing +y, the lip 18 ahead), their tops: level, rising away from the lip at 10, 20, 30 and 45 degrees, falling away
# at 20, sloping across the lip at 15, and rising at 20 under a low slab (no room). Their x: BLOCKS below.
import math, os, shutil, subprocess, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
OUT = os.path.join(ROOT, "quakevr", "maps", "vrslopes.map")
TOOLS = r"C:\OHWorkspace\ericw-tools-2.0.0-alpha11-win64"

# (name, the top's slope in degrees: + rising away from the lip, - falling; "x" across; a slab over it)
BLOCKS = [("level", 0, ""), ("up10", 10, ""), ("up20", 20, ""), ("up30", 30, ""), ("up45", 45, ""),
          ("down20", -20, ""), ("across15", 15, "x"), ("up20slab", 20, "slab")]
WIDTH, DEPTH, GAP, LIP = 96, 96, 64, 48


def sub(a, b): return [a[i] - b[i] for i in range(3)]
def add(a, b): return [a[i] + b[i] for i in range(3)]
def mul(a, k): return [x * k for x in a]
def cross(a, b): return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
def norm(a):
    l = math.sqrt(sum(x * x for x in a))
    return [x / l for x in a]


def face(p0, n, tex):
    """A brush face through p0 facing out along n (Valve 220; outward = (p2 - p0) x (p1 - p0), as qbsp reads it)."""
    n = norm(n)
    u = norm(cross(n, [0, 0, 1])) if abs(n[2]) < 0.9 else [1, 0, 0]
    v = cross(u, n)  # cross(v, u) == n
    p1, p2 = add(p0, mul(u, 64)), add(p0, mul(v, 64))
    f = lambda p: " ".join(f"{c:.6g}" for c in p)
    t = cross(n, u)
    return (f"( {f(p0)} ) ( {f(p1)} ) ( {f(p2)} ) {tex} [ {u[0]:.6g} {u[1]:.6g} {u[2]:.6g} 0 ] "
            f"[ {t[0]:.6g} {t[1]:.6g} {t[2]:.6g} 0 ] 0 1 1")


def brush(planes):
    return "{\n" + "\n".join(face(p, n, t) for p, n, t in planes) + "\n}"


def box(x0, y0, z0, x1, y1, z1, tex):
    return brush([([x0, y0, z0], [-1, 0, 0], tex), ([x1, y1, z1], [1, 0, 0], tex), ([x0, y0, z0], [0, -1, 0], tex),
                  ([x1, y1, z1], [0, 1, 0], tex), ([x0, y0, z0], [0, 0, -1], tex), ([x1, y1, z1], [0, 0, 1], tex)])


def block(x0, deg, kind):
    x1 = x0 + WIDTH
    a = math.radians(abs(deg))
    if kind == "x":
        top = ([x0 + WIDTH / 2, 0, LIP], [-math.sin(a), 0, math.cos(a)])
    else:
        top = ([x0, 0, LIP], [0, -math.sin(a) if deg >= 0 else math.sin(a), math.cos(a)])
    B = [brush([([x0, 0, 0], [-1, 0, 0], "qvr_wall"), ([x1, DEPTH, 0], [1, 0, 0], "qvr_wall"),
                ([x0, 0, 0], [0, -1, 0], "qvr_wall"), ([x1, DEPTH, 0], [0, 1, 0], "qvr_wall"),
                ([x0, 0, 0], [0, 0, -1], "qvr_wall"), (top[0], top[1], "qvr_floor")])]
    if kind == "slab":
        B.append(box(x0, -8, 100, x1, DEPTH, 116, "qvr_ceiling"))
    return B


B = []
X0, X1 = -64, len(BLOCKS) * (WIDTH + GAP) + 64
Y0, Y1, Z0, Z1 = -160, DEPTH + 64, -16, 256
B.append(box(X0 - 16, Y0 - 16, Z1, X1 + 16, Y1 + 16, Z1 + 16, "qvr_ceiling"))
B.append(box(X0 - 16, Y0 - 16, Z0, X1 + 16, Y1 + 16, 0, "qvr_floor"))
B.append(box(X0 - 16, Y0 - 16, Z0, X0, Y1 + 16, Z1, "qvr_wall"))
B.append(box(X1, Y0 - 16, Z0, X1 + 16, Y1 + 16, Z1, "qvr_wall"))
B.append(box(X0, Y0 - 16, Z0, X1, Y0, Z1, "qvr_wall"))
B.append(box(X0, Y1, Z0, X1, Y1 + 16, Z1, "qvr_wall"))
for i, (name, deg, kind) in enumerate(BLOCKS):
    B += block(i * (WIDTH + GAP), deg, kind)

ents = ['{\n"classname" "info_player_start"\n"origin" "48 -100 24"\n"angle" "90"\n}']
for i in range(len(BLOCKS) + 1):
    ents.append(f'{{\n"classname" "light"\n"origin" "{i * (WIDTH + GAP) - GAP / 2} -60 200"\n"light" "300"\n}}')

with open(OUT, "w", newline="\n") as f:
    f.write('// Game: Quake VR\n// Format: Valve\n// entity 0\n{\n"classname" "worldspawn"\n"mapversion" "220"\n'
            '"wad" "quakevr/wads/quakevr_dev.wad"\n"_tb_mod" "hipnotic;rogue;quakevr"\n'
            '"message" "Quake VR sloped mantle test"\n"worldtype" "2"\n"light" "120"\n')
    f.write("\n".join(B) + "\n}\n")
    for i, e in enumerate(ents):
        f.write(f"// entity {i + 1}\n{e}\n")
for i, (name, deg, kind) in enumerate(BLOCKS):
    print(f"{name}: setpos {i * (WIDTH + GAP) + WIDTH // 2} -18 24 0 90 0")

if "--compile" in sys.argv:
    work = os.path.join(ROOT, "scratch", "vrslopes_build")
    os.makedirs(work, exist_ok=True)
    src, bsp = os.path.join(work, "vrslopes.map"), os.path.join(work, "vrslopes.bsp")
    shutil.copyfile(OUT, src)
    for cmd in ([os.path.join(TOOLS, "qbsp.exe"), "-nolog", "-nopercent", "-wadpath", ROOT, src, bsp],
                [os.path.join(TOOLS, "vis.exe"), "-nolog", "-nopercent", bsp],
                [os.path.join(TOOLS, "light.exe"), "-nolog", "-nopercent", "-lit", bsp]):
        r = subprocess.run(cmd, capture_output=True, text=True)
        lines = (r.stdout + r.stderr).splitlines()
        bad = [l for l in lines if "WARNING" in l.upper() or "ERROR" in l.upper()]
        print(f"{os.path.basename(cmd[0])}: exit {r.returncode}" + "".join("\n  " + w for w in bad[:8]))
        if r.returncode:
            sys.exit(1)
    for ext in (".bsp", ".lit"):
        shutil.copyfile(os.path.join(work, "vrslopes" + ext), os.path.join(os.path.dirname(OUT), "vrslopes" + ext))
