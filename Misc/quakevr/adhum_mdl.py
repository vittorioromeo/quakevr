# adhum_mdl.py [model ...]: measures AD's humanoid models (read in place from AD's paks) beside Quake VR's armed rows'
# models (docs/vr-port/ARMED_MONSTERS.md, "Audit 2"). Pure Python (no numpy). Prints per model: vertices, frames, frame
# groups, frame 0's build (height, shoulder band width and depth), the weapon's extent (vr_monstermods.cpp knownSwords'
# ranges, left out of the build). AD_DIR: AD's folder (default the kit's qbase/ad).
import struct, sys, os, re
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import quakepak

AD = os.environ.get("AD_DIR", "C:/OHWorkspace/qvr-kit/qbase/ad/")
OURS = os.path.join(HERE, "..", "..", "quakevr", "progs") + "/"
files = {}
for p in quakepak.game_paks(AD):
    files.update(quakepak.read_pak(p))

def load(name):
    if name.startswith("ours:"):
        return open(OURS + name[5:], "rb").read()
    return files["progs/" + name]

def parse(d):
    h = struct.unpack_from("<4si3f3ff3fiiiiiiiiif", d, 0)
    scale = h[2:5]; origin = h[5:8]
    ns, sw, sh, nv, nt, nf = h[12:18]
    off = 84
    for _ in range(ns):
        g, = struct.unpack_from("<i", d, off); off += 4
        n = 1
        if g:
            n, = struct.unpack_from("<i", d, off); off += 4 + 4 * n
        off += sw * sh * n
    off += 12 * nv + 16 * nt
    frames = []
    for _ in range(nf):
        t, = struct.unpack_from("<i", d, off); off += 4
        if t:
            n, = struct.unpack_from("<i", d, off); off += 4 + 8 + 4 * n
            for k in range(n):
                name = d[off + 8:off + 24].split(b"\0")[0].decode("latin-1")
                vb = d[off + 24:off + 24 + 4 * nv]
                frames.append((name, vb)); off += 24 + 4 * nv
        else:
            name = d[off + 8:off + 24].split(b"\0")[0].decode("latin-1")
            vb = d[off + 24:off + 24 + 4 * nv]
            frames.append((name, vb)); off += 24 + 4 * nv
    return scale, origin, nv, frames

def verts(scale, origin, nv, vb):
    return [tuple(vb[4 * i + a] * scale[a] + origin[a] for a in range(3)) for i in range(nv)]

def groups(frames):
    out = []
    for i, (n, _) in enumerate(frames):
        g = re.sub(r"\d+$", "", n)
        if out and out[-1][0] == g:
            out[-1][2] = i
        else:
            out.append([g, i, i])
    return " ".join("%s %d-%d" % (g, a, b) for g, a, b in out)

def build(v, skip, label):
    body = [p for i, p in enumerate(v) if i not in skip]
    zs = [p[2] for p in body]; z0, z1 = min(zs), max(zs); H = z1 - z0
    band = [p for p in body if z0 + 0.72 * H <= p[2] <= z0 + 0.86 * H]
    w = max(p[1] for p in band) - min(p[1] for p in band) if band else 0
    dpt = max(p[0] for p in band) - min(p[0] for p in band) if band else 0
    s = "%s h %.1f (z %.1f..%.1f) shoulders w %.1f d %.1f" % (label, H, z0, z1, w, dpt)
    if skip:
        wv = [v[i] for i in skip if i < len(v)]
        ext = [max(p[a] for p in wv) - min(p[a] for p in wv) for a in range(3)]
        s += " | weapon %d verts extent x%.0f y%.0f z%.0f" % (len(wv), ext[0], ext[1], ext[2])
    return s

# name, weapon vertex ranges (knownSwords), stand frame index
MODELS = [
    ("ours:vrgrunt.mdl", [(463, 86)], 0),
    ("ours:vrenforcer.mdl", [(22, 2), (100, 1), (400, 31), (455, 24)], 0),
    ("ours:vrknight.mdl", [(334, 3), (363, 3), (524, 1), (535, 6), (557, 92)], 0),
    ("ours:vrhknight.mdl", [(42, 17), (332, 34), (525, 8)], 0),
    ("mon_dknight.mdl", [(870, 41)], 0),
    ("mon_dguard.mdl", [(544, 41)], 0),
    ("mon_dguardq.mdl", [(0, 25)], 0),
    ("mon_dsergeant.mdl", [(870, 395)], 0),
    ("mon_dfury.mdl", [(1240, 136)], 0),
    ("mon_dlord.mdl", [(69, 25)], 0),
    ("mon_hknight.mdl", [(74, 25)], 0),
    ("mon_zombiek.mdl", [], 0),
    ("mon_justice.mdl", [(0, 92)], 0),
    ("mon_judge.mdl", [(680, 100)], 0),
    ("mon_centurion.mdl", [(0, 17), (45, 16), (222, 62)], 0),
    ("mon_skullwiz.mdl", [], 0),
    ("mon_seeker.mdl", [], 0),
    ("mon_freddie.mdl", [], 0),
    ("mon_jim.mdl", [], 0),
    ("mon_deflector.mdl", [(0, 17), (222, 45)], 0),
    ("mon_dcrossbow.mdl", [(772, 348)], 0),
    ("mon_knight.mdl", [(247, 21)], 0),
]
want = sys.argv[1:] or [m[0] for m in MODELS]
for name, rngs, sf in MODELS:
    if name not in want:
        continue
    try:
        d = load(name)
    except KeyError:
        print("==", name, "missing"); continue
    scale, origin, nv, frames = parse(d)
    skip = set()
    for a, n in rngs:
        skip.update(range(a, a + n))
    v = verts(scale, origin, nv, frames[sf][1])
    print("== %s: %d verts, %d frames" % (name, nv, len(frames)))
    print("   ", build(v, skip, frames[sf][0]))
    print("   groups:", groups(frames))
