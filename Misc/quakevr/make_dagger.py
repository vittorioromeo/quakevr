#!/usr/bin/env python3
# make_dagger.py -- the dagger (progs/v_dagger.mdl): the knight's sword (progs/v_ksword.mdl, make_swords.py) made a
# dagger. The same pieces, the same skin and the same layout (laid where the sword is, along the axe's handle, its
# model bounds' corner kept: the sword's weapon settings hold it the same way), each piece scaled in the blade's own
# frame (along it, across its width, through its thickness):
#   - the blade a third as long (from its root in the guard), its width 0.7 and its thickness 0.85 of the sword's;
#   - the crossguard narrower (0.6 across), thinner (0.8) and a little shorter along the blade (0.85);
#   - the grip shortened from its top under the guard to one hand's room (0.57 of its length; its cross-section, where
#     the hand anchor is, as the sword's);
#   - the pommel 0.75 of the sword's, moved up to the shorter grip's end, overlapping it as the sword's does.
# The skin is the sword's with its painted blood taken out: the sword's clean patch (progs/v_ksword.mdl_0.clean,
# make_clean_skins.py: each bloody texel a copy of a clean one of the same skin) applied to it. The normals are the
# sword's, carried through each piece's scaling (the inverse transpose), so its shading is the sword's.
# Nine identical frames, as the sword's (a weapon's frame numbers).
#
# Usage: python Misc/quakevr/make_dagger.py [output progs folder]
#   Prints the anchors and places the weapon settings want (weapons.txt slot 51): the tip (muzzle_av), the blade's
#   root (wpnbtn_av, wpntxt_av), and the blade-shape shares (bl_from, bl_near) along the hand-to-tip line.
# After the first build its source is the .blend (Misc/quakevr/src_models/blend/v_dagger/, blend_build.py): genguard.py
# keeps this script from writing it again.
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import genguard  # noqa: E402
from mdlgen import HEADER, add, anorms, cross, dot, mul, norm, read_skins, sub  # noqa: E402

# The sword's anchors (weapons.txt slot 18): the hand's, the tip's.
HAND = (-0.9793, 1.4925, -2.6023)
TIP = (28.9816, 0.121, 76.4247)
# Each piece's scales: along the blade, across its width, through its thickness.
BLADE = (1.0 / 3.0, 0.7, 0.85)
GUARD = (0.85, 0.6, 0.8)
GRIP = (0.57, 1.0, 1.0)
POMMEL = 0.75
# The fist's middle ahead of the hand anchor along the blade (model units), for the blade-shape shares: the melee line
# runs from the hand (its middle) to the tip.
FIST_MID = 6.0


def load(path):
    data = open(path, "rb").read()
    h = HEADER.unpack_from(data, 0)
    scale, origin = h[2:5], h[5:8]
    nskins, sw, sh, nv, nt, nf = h[12:18]
    skins, off = read_skins(data, HEADER.size, nskins, sw, sh)
    st = [struct.unpack_from("<3i", data, off + 12 * i) for i in range(nv)]
    off += 12 * nv
    tris = [struct.unpack_from("<4i", data, off + 16 * i) for i in range(nt)]
    off += 16 * nt
    frames = []
    for _ in range(nf):
        (typ,) = struct.unpack_from("<i", data, off)
        assert typ == 0, "a frame group"
        off += 12
        name = data[off:off + 16]
        off += 16
        frames.append((name, [struct.unpack_from("<4B", data, off + 4 * i) for i in range(nv)]))
        off += 4 * nv
    return dict(header=h, scale=scale, origin=origin, skins=skins, w=sw, h=sh, st=st, tris=tris, frames=frames)


def clean_skin(skin, w, h, patch_path):
    """The sword's skin (as stored, its group flag first) with its clean patch applied: each pair's texel a copy of its
    source texel, in order (the patch fills each stain from its rim inwards)."""
    data = open(patch_path, "rb").read()
    assert data[:8] == b"QVRCLEAN", "not a clean patch"
    ver, pw, ph, bpp, _digest, n = struct.unpack_from("<6I", data, 8)
    assert (pw, ph, bpp) == (w, h, 1), "a patch for another skin"
    pix = bytearray(skin[4:4 + w * h])
    pairs = struct.unpack_from("<%dI" % (2 * n), data, 8 + 24)
    for k in range(n):
        pix[pairs[2 * k]] = pix[pairs[2 * k + 1]]
    return skin[:4] + bytes(pix) + skin[4 + w * h:], n


def pieces(m, pos):
    """The model's separate pieces (by triangles and shared places), smallest first: blade, grip, pommel, guard."""
    nv = len(pos)
    parent = list(range(nv))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def join(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb

    for t in m["tris"]:
        join(t[1], t[2])
        join(t[2], t[3])
    seen = {}
    for i, v in enumerate(m["frames"][0][1]):
        if v[:3] in seen:
            join(i, seen[v[:3]])
        else:
            seen[v[:3]] = i
    groups = {}
    for i in range(nv):
        groups.setdefault(find(i), []).append(i)
    return sorted(groups.values(), key=len)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    progs = os.path.join(HERE, "..", "..", "quakevr", "progs")
    out_dir = args[0] if args else progs
    path = os.path.join(out_dir, "v_dagger.mdl")
    guard = genguard.Guard("make_dagger.py", [path])
    m = load(os.path.join(progs, "v_ksword.mdl"))
    scale, origin = m["scale"], m["origin"]
    table = anorms()
    q0 = m["frames"][0][1]
    assert all(f[1] == q0 for f in m["frames"]), "the sword's frames differ"
    pos = [tuple(origin[k] + v[k] * scale[k] for k in range(3)) for v in q0]
    nrm = [table[v[3]] for v in q0]
    blade, grip, pommel, guard_piece = pieces(m, pos)
    assert (len(blade), len(grip), len(pommel), len(guard_piece)) == (14, 40, 56, 120), "not make_swords.py's sword"

    # The blade's frame: along it (its root's middle to its tip), its thickness (the model's y, square to it), its width.
    tip_i = max(blade, key=lambda i: dot(sub(pos[i], HAND), sub(TIP, HAND)))
    root = [i for i in blade if dot(sub(pos[i], HAND), sub(TIP, HAND)) < dot(sub(pos[tip_i], HAND), sub(TIP, HAND)) * 0.5]
    root_c = mul(tuple(sum(pos[i][k] for i in root) for k in range(3)), 1.0 / len(root))
    a = norm(sub(pos[tip_i], root_c))
    t = norm(sub((0.0, 1.0, 0.0), mul(a, a[1])))
    wv = cross(t, a)

    def along(p):
        return dot(sub(p, HAND), a)

    def placer(centre, s, shift=0.0):
        def place(p):
            d = sub(p, centre)
            return add(add(add(centre, mul(a, dot(d, a) * s[0] + shift)), mul(wv, dot(d, wv) * s[1])), mul(t, dot(d, t) * s[2]))

        def turn(n):
            return norm(add(add(mul(a, dot(n, a) / s[0]), mul(wv, dot(n, wv) / s[1])), mul(t, dot(n, t) / s[2])))
        return place, turn

    def middle(ix):
        return mul(tuple(sum(pos[i][k] for i in ix) for k in range(3)), 1.0 / len(ix))

    grip_top = max(along(pos[i]) for i in grip)
    grip_bottom = min(along(pos[i]) for i in grip)
    grip_c = middle(grip)
    grip_c = add(grip_c, mul(a, grip_top - along(grip_c)))
    pommel_c = middle(pommel)
    pommel_top = max(along(pos[i]) for i in pommel)
    overlap = pommel_top - grip_bottom
    new_bottom = grip_top - (grip_top - grip_bottom) * GRIP[0]
    # The pommel scaled about its middle, then moved up so that its top overlaps the grip's new end as it did.
    shift = (new_bottom + overlap * POMMEL) - (along(pommel_c) + (pommel_top - along(pommel_c)) * POMMEL)
    how = {}
    for ix, (centre, s, sh) in ((blade, (root_c, BLADE, 0.0)), (guard_piece, (middle(guard_piece), GUARD, 0.0)),
                                (grip, (grip_c, GRIP, 0.0)), (pommel, (pommel_c, (POMMEL,) * 3, shift))):
        f = placer(centre, s, sh)
        for i in ix:
            how[i] = f
    new_pos = [how[i][0](pos[i]) for i in range(len(pos))]
    new_nrm = [how[i][1](nrm[i]) for i in range(len(pos))]
    assert all(new_pos[i][k] >= origin[k] - 1e-4 for i in range(len(pos)) for k in range(3)), "past the sword's corner"

    skin, cleaned = clean_skin(m["skins"][0], m["w"], m["h"], os.path.join(progs, "v_ksword.mdl_0.clean"))
    # Packed from the sword's bounds' corner (its weapon settings scale about it), the scale to the dagger's own extent.
    hi = [max(p[k] for p in new_pos) for k in range(3)]
    sc = [(hi[k] - origin[k]) / 255.0 for k in range(3)]
    radius = max(math.sqrt(dot(p, p)) for p in new_pos)
    hd = list(m["header"])
    hd[2:5] = sc
    hd[8] = radius
    out = bytearray(HEADER.pack(*hd))
    out += skin
    for s in m["st"]:
        out += struct.pack("<3i", *s)
    for tr in m["tris"]:
        out += struct.pack("<4i", *tr)
    packed = []
    for p, n in zip(new_pos, new_nrm):
        q = [max(0, min(255, int(round((p[k] - origin[k]) / sc[k])))) for k in range(3)]
        packed.append(bytes(q) + bytes((max(range(len(table)), key=lambda j: dot(table[j], n)),)))
    for name, _ in m["frames"]:
        out += struct.pack("<i", 0) + bytes((0, 0, 0, 0)) + bytes((255, 255, 255, 0)) + name + b"".join(packed)
    if path not in guard.kept:
        with open(path, "wb") as f:
            f.write(out)
    guard.finish()

    # What the weapon settings want, from the file as written (its quantised places).
    d = load(path)
    dp = [tuple(d["origin"][k] + v[k] * d["scale"][k] for k in range(3)) for v in d["frames"][0][1]]
    fist = add(HAND, mul(a, FIST_MID))
    tip_new = dp[tip_i]
    line = dot(sub(tip_new, fist), a)
    root_along = max(dot(sub(dp[i], fist), a) for i in guard_piece)
    near_along = -min(dot(sub(dp[i], fist), a) for i in pommel)
    root_vtx = min(blade, key=lambda i: math.dist(pos[i], (8.6198, 0.0134, 9.6413)))  # (the sword's wpnbtn_av)
    total = max(along(p) for p in dp) - min(along(p) for p in dp)
    print("v_dagger.mdl: %d vertices, %d triangles, %d frames, %d bloody texels cleaned -> %s" % (
        len(dp), len(d["tris"]), len(d["frames"]), cleaned, os.path.normpath(path)))
    print("  length %.2f model units (the sword's %.2f); blade from the guard %.2f (the sword's %.2f)" % (
        total, max(along(p) for p in pos) - min(along(p) for p in pos), along(tip_new) - along(root_c) * 1.0,
        along(pos[tip_i]) - along(root_c)))
    print('  muzzle_av "%.4f %.4f %.4f" (vertex %d)' % (tip_new + (tip_i,)))
    print('  wpnbtn_av / wpntxt_av "%.4f %.4f %.4f" (vertex %d, the blade\'s root)' % (dp[root_vtx] + (root_vtx,)))
    print("  bl_from %.3f (the guard's top on the hand-to-tip line), bl_near %.3f (the pommel's end behind the hand)" % (
        root_along / line, near_along / line))


if __name__ == "__main__":
    main()
