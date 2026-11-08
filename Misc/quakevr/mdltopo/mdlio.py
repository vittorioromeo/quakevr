# mdlio.py -- Quake .mdl reader/writer (numpy) for the topology audit and the render-compare harness.
# Keeps every block raw so a model can be written back byte-identical, or with its triangles/poses changed.
# Positions are decoded exactly as the engine decodes them: origin + scale * byte (the stored, quantised grid).
import struct
import numpy as np

HEADER = struct.Struct("<4si3f3ff3f8if")  # ident version scale origin radius eye numskins sw sh nv nt nf sync flags size


class Mdl:
    pass


def read(path_or_bytes):
    data = path_or_bytes if isinstance(path_or_bytes, (bytes, bytearray)) else open(path_or_bytes, "rb").read()
    h = list(HEADER.unpack_from(data, 0))
    m = Mdl()
    m.header = h
    m.scale = np.array(h[2:5], dtype=np.float64)
    m.origin = np.array(h[5:8], dtype=np.float64)
    m.numskins, m.sw, m.sh, m.nv, m.nt, m.nf = h[12:18]
    off = HEADER.size
    m.skins_raw_start = off
    m.skin0 = None
    for _ in range(m.numskins):
        (group,) = struct.unpack_from("<i", data, off)
        off += 4
        if group == 0:
            px = np.frombuffer(data, np.uint8, m.sw * m.sh, off)
            off += m.sw * m.sh
        else:
            (n,) = struct.unpack_from("<i", data, off)
            off += 4 + 4 * n
            px = np.frombuffer(data, np.uint8, m.sw * m.sh, off)
            off += n * m.sw * m.sh
        if m.skin0 is None:
            m.skin0 = px.reshape(m.sh, m.sw)
    m.skins_raw = data[m.skins_raw_start:off]
    st = np.frombuffer(data, "<i4", 3 * m.nv, off).reshape(m.nv, 3).copy()
    off += 12 * m.nv
    m.onseam, m.s, m.t = st[:, 0], st[:, 1], st[:, 2]
    tr = np.frombuffer(data, "<i4", 4 * m.nt, off).reshape(m.nt, 4).copy()
    off += 16 * m.nt
    m.facesfront = tr[:, 0]
    m.tris = tr[:, 1:4]
    # frames: list of (kind, groupinfo, [poses]); pose = (bbmin4, bbmax4, name16, verts(nv,4) uint8)
    m.frames = []
    poses = []
    m.pose_names = []
    for _ in range(m.nf):
        (kind,) = struct.unpack_from("<i", data, off)
        off += 4
        if kind == 0:
            p, off = _pose(data, off, m.nv)
            m.frames.append((0, None, [len(poses)]))
            poses.append(p)
        else:
            (n,) = struct.unpack_from("<i", data, off)
            ginfo = data[off: off + 12 + 4 * n]
            off += 12 + 4 * n
            idx = []
            for _ in range(n):
                p, off = _pose(data, off, m.nv)
                idx.append(len(poses))
                poses.append(p)
            m.frames.append((1, ginfo, idx))
    m.poses_raw = poses
    m.tail = data[off:]
    m.bytes = np.stack([p[3][:, :3] for p in poses]).astype(np.int32)  # (P, nv, 3)
    m.normidx = np.stack([p[3][:, 3] for p in poses]).astype(np.int32)  # (P, nv)
    m.pose_names = [p[2].split(b"\0")[0].decode("latin1") for p in poses]
    m.pos = m.origin + m.scale * m.bytes  # (P, nv, 3) float64
    return m


def _pose(data, off, nv):
    bbmin = data[off: off + 4]
    bbmax = data[off + 4: off + 8]
    name = data[off + 8: off + 24]
    v = np.frombuffer(data, np.uint8, 4 * nv, off + 24).reshape(nv, 4).copy()
    return (bbmin, bbmax, name, v), off + 24 + 4 * nv


def write(m, path):
    """Writes m back (its tris/facesfront/stverts/bytes/normidx may have been changed, nv/nt follow the arrays)."""
    nv = len(m.s)
    nt = len(m.tris)
    h = list(m.header)
    h[15], h[16] = nv, nt
    out = bytearray(HEADER.pack(*h))
    out += m.skins_raw
    out += np.stack([m.onseam, m.s, m.t], 1).astype("<i4").tobytes()
    out += np.concatenate([m.facesfront[:, None], m.tris], 1).astype("<i4").tobytes()
    for kind, ginfo, idx in m.frames:
        out += struct.pack("<i", kind)
        if kind:
            out += ginfo
        for pi in idx:
            bbmin, bbmax, name, _ = m.poses_raw[pi]
            v = np.concatenate([m.bytes[pi], m.normidx[pi][:, None]], 1).astype(np.uint8)
            out += bbmin + bbmax + name + v.tobytes()
    out += m.tail
    open(path, "wb").write(bytes(out))


def tri_uv(m):
    """(nt, 3, 2) texel coordinates as the engine's: (s + 0.5, t + 0.5), back-facing seam vertices shifted half a skin."""
    s = m.s[m.tris].astype(np.float64)
    t = m.t[m.tris].astype(np.float64)
    seam = (m.onseam[m.tris] != 0) & (m.facesfront[:, None] == 0)
    s = s + np.where(seam, m.sw / 2.0, 0.0)
    return np.stack([s + 0.5, t + 0.5], -1)


def outward_normals(P):
    """P (..., 3 corners, 3): Quake front faces are clockwise seen from outside -> outward = (v2-v0) x (v1-v0)."""
    return np.cross(P[..., 2, :] - P[..., 0, :], P[..., 1, :] - P[..., 0, :])


def anorms():
    import os
    import sys
    here = os.path.dirname(os.path.abspath(__file__))
    for d in (os.path.join(here, ".."), os.path.join(here, "..", "..", "Misc", "quakevr")):  # Misc/quakevr/mdltopo, scratch/topo
        if os.path.exists(os.path.join(d, "mdlpolish.py")):
            sys.path.insert(0, os.path.normpath(d))
            break
    import mdlpolish  # noqa
    return np.array(mdlpolish.anorms(), dtype=np.float64)


def palette():
    """id's palette (not ours to ship): $QVR_PALETTE, a palette.lmp next to this file, or gfx/palette.lmp from
    $QVR_BASEDIR/id1/pak0.pak (default: the kit's qbase); else a grey ramp (texel comparisons don't need it)."""
    import os
    for p in (os.environ.get("QVR_PALETTE", ""), os.path.join(os.path.dirname(os.path.abspath(__file__)), "palette.lmp")):
        if p and os.path.exists(p):
            return np.frombuffer(open(p, "rb").read(), np.uint8).reshape(256, 3)
    pak = os.path.join(os.environ.get("QVR_BASEDIR", "C:/OHWorkspace/qvr-kit/qbase"), "id1", "pak0.pak")
    if os.path.exists(pak):
        with open(pak, "rb") as f:
            _, dofs, dlen = struct.unpack("<4sii", f.read(12))
            f.seek(dofs)
            d = f.read(dlen)
            for i in range(dlen // 64):
                name, o, n = struct.unpack_from("<56sii", d, 64 * i)
                if name.split(b"\0")[0] == b"gfx/palette.lmp":
                    f.seek(o)
                    return np.frombuffer(f.read(n), np.uint8).reshape(256, 3)
    g = np.arange(256, dtype=np.uint8)
    return np.stack([g, g, g], 1)


def weld(m):
    """Welded vertex id per vertex: same stored bytes in every pose."""
    key = np.ascontiguousarray(m.bytes.transpose(1, 0, 2).reshape(m.nv, -1))
    _, inv = np.unique(key, axis=0, return_inverse=True)
    return inv.reshape(-1)
