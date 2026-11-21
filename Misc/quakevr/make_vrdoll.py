#!/usr/bin/env python3
# make_vrdoll.py -- the avatar's own ragdoll (docs/vr-port/MP_PLAN.md, "The avatar's own ragdoll"; ROUND25.md "avatardoll"):
# a dead player's body made of the body he is drawn with (make_vrbody.py's skeletal model), not id's ranger.
#
# For each build (vr_body_build 0..3: lean, athletic, brawny, feminine) it writes
#   quakevr/progs/vrdoll<_build>.mdl      the build's body (its md5mesh's bind pose) as a .mdl, one frame, the feet at
#                                         z = -24 (player.mdl's: the bottom of the player's box), facing +x; its skin the
#                                         build's bare, unarmoured skin (vrbody_00_00.tga) in Quake's palette, a quarter of
#                                         its size. What the ragdoll code rigs and simulates (vr_ragdoll.cpp fromAuthored);
#                                         drawn as itself only where the dead player's own body cannot be (vr_mp_body 0
#                                         on a client of a server that made it, more than 8 bodies in sight)
#   quakevr/rigs/vrdoll<_build>.rig2.txt  its authored rig (vr_rig2.cpp's format, as make_rig2.py's): 20 bones from the
#                                         md5's skeleton (its calves the shins; the forearm's twist helpers its forearm,
#                                         the wrist its hand), each vertex's weights the md5's own (the helpers' summed into
#                                         their bone: at most 4, none under 0.03, summing to 1), the joint limits the armed
#                                         grunt's (quakevr/rigs/vrgrunt.rig2.txt: a human of the same proportions; each ball's
#                                         swing directions turned square to this bone's neutral; the upper arms' and thighs'
#                                         neutral hanging down as his; the elbows' and knees' hinge axes from this straight
#                                         bind pose: the elbow bends forward, the knee back)
#
# The server's body (QC vr_deathdoll.qc) takes the dead player's build's model, scaled to his height and world scale
# (the engine's avatarbody builtin): the ragdoll's parts are his body's own proportions; each client draws his avatar on
# its bones (vr_mpavatar.cpp addDolls), his look and calibration.
#
# Usage: python Misc/quakevr/make_vrdoll.py [--quiet]

import math
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import genguard  # noqa: E402
from mdlgen import HEADER, anorms, cross, dot, norm, sub  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
PROGS = os.path.join(ROOT, 'quakevr', 'progs')
RIGS = os.path.join(ROOT, 'quakevr', 'rigs')

BUILDS = [('vrbody_lean', 'vrdoll_lean'), ('vrbody', 'vrdoll'), ('vrbody_brawny', 'vrdoll_brawny'), ('vrbody_fem', 'vrdoll_fem')]
FEET_Z = -24.0  # the model's floor: player.mdl's, the player's box's bottom
SKIN_DIV = 4    # the skin a quarter of the md5's (512 x 2048 -> 128 x 512)

# The doll's bones (the rig's order: parents first) and the md5 joints each takes (the first its pivot).
BONES = [
    ('pelvis', None, ['pelvis']),
    ('spine', 'pelvis', ['spine']),
    ('chest', 'spine', ['chest']),
    ('neck', 'chest', ['neck']),
    ('head', 'neck', ['head']),
    ('clavicle_l', 'chest', ['clavicle_l']),
    ('upperarm_l', 'clavicle_l', ['upperarm_l']),
    ('forearm_l', 'upperarm_l', ['forearm_l', 'foretwist1_l', 'foretwist2_l', 'foretwist3_l', 'foretwist4_l']),
    ('hand_l', 'forearm_l', ['hand_l', 'wrist_l']),
    ('clavicle_r', 'chest', ['clavicle_r']),
    ('upperarm_r', 'clavicle_r', ['upperarm_r']),
    ('forearm_r', 'upperarm_r', ['forearm_r', 'foretwist1_r', 'foretwist2_r', 'foretwist3_r', 'foretwist4_r']),
    ('hand_r', 'forearm_r', ['hand_r', 'wrist_r']),
    ('thigh_l', 'pelvis', ['thigh_l']),
    ('shin_l', 'thigh_l', ['calf_l']),
    ('foot_l', 'shin_l', ['foot_l']),
    ('thigh_r', 'pelvis', ['thigh_r']),
    ('shin_r', 'thigh_r', ['calf_r']),
    ('foot_r', 'shin_r', ['foot_r']),
]
# Each bone's end: its child's pivot (else: the head its crown, a hand along its forearm, a foot its toe).
END_CHILD = {'pelvis': 'spine', 'spine': 'chest', 'chest': 'neck', 'neck': 'head', 'clavicle_l': 'upperarm_l',
             'upperarm_l': 'forearm_l', 'forearm_l': 'hand_l', 'clavicle_r': 'upperarm_r', 'upperarm_r': 'forearm_r',
             'forearm_r': 'hand_r', 'thigh_l': 'shin_l', 'shin_l': 'foot_l', 'thigh_r': 'shin_r', 'shin_r': 'foot_r'}
HAND_LENGTH = 3.5  # units past the wrist (the body's hand bone holds only the bracer's lip: the jointed hand is drawn apart)

PALETTE_HEX = None  # (read from mdlpolish.py's text: no numpy needed)


def palette():
    text = open(os.path.join(HERE, 'mdlpolish.py')).read()
    m = re.search(r'PALETTE_HEX = \((.*?)\)', text, re.S)
    hexs = ''.join(re.findall(r'"([0-9a-f]+)"', m.group(1)))
    raw = bytes.fromhex(hexs)
    return [tuple(raw[i * 3:i * 3 + 3]) for i in range(256)]


def qmul(a, b):
    aw, ax, ay, az = a
    bw, bx, by, bz = b
    return (aw * bw - ax * bx - ay * by - az * bz, aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx, aw * bz + ax * by - ay * bx + az * bw)


def qrot(q, v):
    w, x, y, z = q
    r = qmul(qmul(q, (0.0, v[0], v[1], v[2])), (w, -x, -y, -z))
    return (r[1], r[2], r[3])


def read_md5(path):
    txt = open(path).read()
    joints = []
    jblock = re.search(r'joints \{(.*?)\}', txt, re.S).group(1)
    for m in re.finditer(r'"(\w+)"\s+(-?\d+)\s+\(\s*(\S+) (\S+) (\S+)\s*\)\s+\(\s*(\S+) (\S+) (\S+)\s*\)', jblock):
        x, y, z = float(m.group(6)), float(m.group(7)), float(m.group(8))
        w = -math.sqrt(max(0.0, 1.0 - x * x - y * y - z * z))
        joints.append((m.group(1), int(m.group(2)), (float(m.group(3)), float(m.group(4)), float(m.group(5))), (w, x, y, z)))
    verts = [(float(m.group(1)), float(m.group(2)), int(m.group(3)), int(m.group(4)))
             for m in re.finditer(r'vert \d+ \(\s*(\S+) (\S+)\s*\) (\d+) (\d+)', txt)]
    tris = [tuple(map(int, m.groups())) for m in re.finditer(r'tri \d+ (\d+) (\d+) (\d+)', txt)]
    weights = [(int(m.group(1)), float(m.group(2)), (float(m.group(3)), float(m.group(4)), float(m.group(5))))
               for m in re.finditer(r'weight \d+ (\d+) (\S+) \(\s*(\S+) (\S+) (\S+)\s*\)', txt)]
    return joints, verts, tris, weights


def read_tga(path):
    data = open(path, 'rb').read()
    idlen, cmtype, itype = data[0], data[1], data[2]
    w, h = struct.unpack_from('<HH', data, 12)
    bpp, desc = data[16], data[17]
    assert cmtype == 0 and itype == 2 and bpp in (24, 32), 'an uncompressed true-colour TGA'
    step = bpp // 8
    off = 18 + idlen
    rows = []
    for r in range(h):
        row = data[off + r * w * step: off + (r + 1) * w * step]
        rows.append([(row[i * step + 2], row[i * step + 1], row[i * step]) for i in range(w)])
    if not desc & 0x20:
        rows.reverse()  # (bottom-up stored: top row first here)
    return w, h, rows


def skin_of(tga_path, pal):
    w, h, rows = read_tga(tga_path)
    sw, sh = w // SKIN_DIV, h // SKIN_DIV
    cache = {}
    out = bytearray()
    for y in range(sh):
        for x in range(sw):
            acc = [0, 0, 0]
            for dy in range(SKIN_DIV):
                row = rows[y * SKIN_DIV + dy]
                for dx in range(SKIN_DIV):
                    c = row[x * SKIN_DIV + dx]
                    acc[0] += c[0]
                    acc[1] += c[1]
                    acc[2] += c[2]
            n = SKIN_DIV * SKIN_DIV
            c = (acc[0] // n, acc[1] // n, acc[2] // n)
            if c not in cache:
                # (the nearest of the non-fullbright colours)
                cache[c] = min(range(224), key=lambda i: (pal[i][0] - c[0]) ** 2 + (pal[i][1] - c[1]) ** 2 + (pal[i][2] - c[2]) ** 2)
            out.append(cache[c])
    return sw, sh, bytes(out)


def write_mdl(path, positions, normals, sts, tris, skin, sw, sh, frame_name):
    table = anorms()
    lo = [min(p[k] for p in positions) for k in range(3)]
    hi = [max(p[k] for p in positions) for k in range(3)]
    scale = [(hi[k] - lo[k]) / 255.0 or 1.0 for k in range(3)]
    radius = max(math.sqrt(dot(p, p)) for p in positions)
    data = bytearray(HEADER.pack(b'IDPO', 6, *scale, *lo, radius, 0.0, 0.0, 0.0, 1, sw, sh, len(positions), len(tris),
                                 1, 0, 0, 1.0))
    data += struct.pack('<i', 0) + skin
    for s, t in sts:
        data += struct.pack('<3i', 0, s, t)
    for a, b, c in tris:
        data += struct.pack('<4i', 1, a, b, c)
    data += struct.pack('<i', 0)
    data += bytes((0, 0, 0, 0)) + bytes((255, 255, 255, 0))
    data += frame_name.encode().ljust(16, b'\0')[:16]
    for p, n in zip(positions, normals):
        q = [max(0, min(255, int(round((p[k] - lo[k]) / scale[k])))) for k in range(3)]
        best = max(range(len(table)), key=lambda i: dot(table[i], n))
        data += bytes((q[0], q[1], q[2], best))
    with open(path, 'wb') as f:
        f.write(data)


def read_grunt_limits():
    limits = {}
    for line in open(os.path.join(RIGS, 'vrgrunt.rig2.txt')):
        t = line.split()
        if not t or t[0] != 'bone':
            continue
        d = {'joint': t[3]}
        i = 4
        while i < len(t):
            key = t[i]
            if key in ('pivot', 'end', 'neutral', 'fwd', 'side', 'axis'):
                d[key] = tuple(float(x) for x in t[i + 1:i + 4])
                i += 4
            elif key in ('swingx', 'swingy', 'twist', 'range'):
                d[key] = (float(t[i + 1]), float(t[i + 2]))
                i += 3
            else:
                i += 1
        limits[t[1]] = d
    return limits


def square_to(v, n, fallback):
    """v made square to unit n (normalised); `fallback` the same way if v lies along n."""
    for c in (v, fallback):
        o = sub(c, tuple(x * dot(c, n) for x in n))
        if math.sqrt(dot(o, o)) > 0.2:
            return norm(o)
    return norm(cross(n, (0.0, 0.0, 1.0)))


def f3(v):
    return '%.3f %.3f %.3f' % tuple(0.0 if abs(x) < 5e-4 else x for x in v)


def build(src, dst, pal, quiet):
    joints, verts, tris, weights = read_md5(os.path.join(PROGS, src + '.md5mesh'))
    jindex = {j[0]: i for i, j in enumerate(joints)}
    bone_of_joint = {}
    for b, (name, _, js) in enumerate(BONES):
        for j in js:
            bone_of_joint[jindex[j]] = b
    assert len(bone_of_joint) == len(joints), 'every md5 joint a doll bone'
    lift = (0.0, 0.0, FEET_Z)

    # The bind pose's vertices and each one's weights on the doll's bones.
    positions, vweights = [], []
    for (_, _, start, count) in verts:
        p = [0.0, 0.0, 0.0]
        wb = {}
        for k in range(start, start + count):
            j, bias, local = weights[k]
            jp, jq = joints[j][2], joints[j][3]
            r = qrot(jq, local)
            for c in range(3):
                p[c] += bias * (jp[c] + r[c])
            b = bone_of_joint[j]
            wb[b] = wb.get(b, 0.0) + bias
        positions.append(tuple(p[c] + lift[c] for c in range(3)))
        best = sorted(wb.items(), key=lambda kv: -kv[1])[:4]
        best = [(b, w) for b, w in best if w >= 0.03] or best[:1]
        total = sum(w for _, w in best)
        best = [(b, round(w / total, 3)) for b, w in best]
        fix = round(1.0 - sum(w for _, w in best), 3)  # (the rounding's remainder on the heaviest)
        best[0] = (best[0][0], round(best[0][1] + fix, 3))
        vweights.append(best)

    # Winding: Quake's triangles clockwise seen from outside (the mesh's signed volume tells the md5's).
    vol = sum(dot(positions[a], cross(positions[b], positions[c])) for a, b, c in tris)
    out_tris = [(a, c, b) for a, b, c in tris] if vol > 0 else list(tris)
    normals = [[0.0, 0.0, 0.0] for _ in positions]
    for a, b, c in out_tris:
        n = cross(sub(positions[c], positions[a]), sub(positions[b], positions[a]))  # (outward: the reverse of the stored order)
        for v in (a, b, c):
            for k in range(3):
                normals[v][k] += n[k]
    normals = [norm(tuple(n)) if dot(n, n) > 1e-12 else (0.0, 0.0, 1.0) for n in normals]

    sw, sh, skin = skin_of(os.path.join(PROGS, 'vrbody_00_00.tga'), pal)
    sts = [(max(0, min(sw - 1, int(round(u * sw)))), max(0, min(sh - 1, int(round(v * sh))))) for (u, v, _, _) in verts]
    write_mdl(os.path.join(PROGS, dst + '.mdl'), positions, normals, sts, out_tris, skin, sw, sh, 'doll')

    # The rig.
    pivots = {name: tuple(joints[jindex[js[0]]][2][c] + lift[c] for c in range(3)) for name, _, js in BONES}
    ends = {}
    for name, _, _ in BONES:
        if name in END_CHILD:
            ends[name] = pivots[END_CHILD[name]]
    head = [b for b, (n, _, _) in enumerate(BONES) if n == 'head'][0]
    crown = max(p[2] for p, w in zip(positions, vweights) if w[0][0] == head and w[0][1] > 0.5)
    ends['head'] = (pivots['head'][0], pivots['head'][1], crown)
    for side in 'lr':
        d = norm(sub(pivots['hand_' + side], pivots['forearm_' + side]))
        ends['hand_' + side] = tuple(pivots['hand_' + side][c] + d[c] * HAND_LENGTH for c in range(3))
        foot = [b for b, (n, _, _) in enumerate(BONES) if n == 'foot_' + side][0]
        toe = max((p for p, w in zip(positions, vweights) if w[0][0] == foot), key=lambda p: p[0])
        ends['foot_' + side] = (toe[0], pivots['foot_' + side][1], pivots['foot_' + side][2] - 1.0)

    grunt = read_grunt_limits()
    lines = ['qvrrig2 1', 'model progs/%s.mdl' % dst,
             '# Made by Misc/quakevr/make_vrdoll.py (its header: the method) from quakevr/progs/%s.md5mesh; the limits' % src,
             '# the armed grunt\'s (vrgrunt.rig2.txt). Model units, x forward, y left, z up; degrees.']
    for name, parent, _ in BONES:
        g = grunt[name]
        piv, end = pivots[name], ends[name]
        d = norm(sub(end, piv))
        head_txt = 'bone %s %s %s pivot %s end %s' % (name, parent or '-', g['joint'], f3(piv), f3(end))
        if g['joint'] == 'root':
            lines.append(head_txt)
        elif g['joint'] == 'hinge':
            # (straight in the bind pose: the elbow bends forward, its axis d x forward; the knee back, forward x d)
            fwd = (1.0, 0.0, 0.0)
            axis = norm(cross(d, fwd)) if name.startswith('forearm') else norm(cross(fwd, d))
            lines.append('%s axis %s range %g %g' % (head_txt, f3(axis), g['range'][0], g['range'][1]))
        else:
            neutral = norm(g['neutral']) if name.startswith(('upperarm', 'thigh')) else d
            fwd = square_to(g['fwd'], neutral, g['side'])
            side = norm(cross(neutral, fwd))
            if dot(side, g['side']) < 0:
                side = tuple(-x for x in side)
            lines.append('%s neutral %s fwd %s side %s swingx %g %g swingy %g %g twist %g %g' % (
                head_txt, f3(neutral), f3(fwd), f3(side), g['swingx'][0], g['swingx'][1], g['swingy'][0], g['swingy'][1],
                g['twist'][0], g['twist'][1]))
    for v, w in enumerate(vweights):
        lines.append('w %d %s' % (v, ' '.join('%s %g' % (BONES[b][0], x) for b, x in w)))
    with open(os.path.join(RIGS, dst + '.rig2.txt'), 'w', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    if not quiet:
        height = max(p[2] for p in positions) - min(p[2] for p in positions)
        print('%s: %d vertices, %d triangles, skin %dx%d, %.1f units tall, crown %.1f, winding %s' % (
            dst, len(positions), len(out_tris), sw, sh, height, crown, 'flipped' if vol > 0 else 'kept'))


def main():
    quiet = '--quiet' in sys.argv
    pal = palette()
    # (genguard: a rig the Rig Studio's Range of Motion window saved, or a model edited by hand, is not overwritten)
    paths = [os.path.join(PROGS, dst + '.mdl') for _, dst in BUILDS] + [os.path.join(RIGS, dst + '.rig2.txt') for _, dst in BUILDS]
    guard = genguard.Guard('make_vrdoll.py', paths)
    for src, dst in BUILDS:
        build(src, dst, pal, quiet)
    guard.finish()


if __name__ == '__main__':
    main()
