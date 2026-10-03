# mdl.py -- a Quake .mdl's frames (knight_rig.py) as numpy arrays (positions in model units): load(path) -> (names, poses[f][v][3], tris)
import struct
import numpy as np


def load(path):
    b = path if isinstance(path, bytes) else open(path, 'rb').read()  # (a path, or the file's bytes)
    o = 0
    ident, version = struct.unpack_from('<4si', b, o); o += 8
    scale = np.array(struct.unpack_from('<3f', b, o)); o += 12
    trans = np.array(struct.unpack_from('<3f', b, o)); o += 12
    o += 4 + 12  # radius, eye
    numskins, sw, sh, nv, nt, nf, sync, flags, size = struct.unpack_from('<9i', b, o); o += 36
    for _ in range(numskins):
        t, = struct.unpack_from('<i', b, o); o += 4
        if t == 0:
            o += sw * sh
        else:
            n, = struct.unpack_from('<i', b, o); o += 4 + 4 * n + n * sw * sh
    o += nv * 12
    tris = []
    for i in range(nt):
        ff, a, c, d = struct.unpack_from('<4i', b, o); o += 16
        tris.append((a, c, d))
    names, poses = [], []
    for f in range(nf):
        t, = struct.unpack_from('<i', b, o); o += 4
        if t == 0:
            o += 8
            name = b[o:o + 16].split(b'\0')[0].decode('latin1'); o += 16
            v = np.frombuffer(b, dtype=np.uint8, count=nv * 4, offset=o).reshape(nv, 4)[:, :3]; o += nv * 4
            names.append(name); poses.append(v * scale + trans)
        else:
            n, = struct.unpack_from('<i', b, o); o += 4 + 8 + 4 * n
            for k in range(n):
                o += 8
                name = b[o:o + 16].split(b'\0')[0].decode('latin1'); o += 16
                v = np.frombuffer(b, dtype=np.uint8, count=nv * 4, offset=o).reshape(nv, 4)[:, :3]; o += nv * 4
                if k == 0:
                    names.append(name); poses.append(v * scale + trans)
    return names, np.array(poses), np.array(tris)
