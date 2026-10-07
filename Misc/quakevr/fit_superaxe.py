"""Dawn of the Machine's Super Axe laid as Quake VR's axe lies: the numbers vr_monstermods.cpp superAxePoses and the
Super Axe's weapon settings (vr_weapons.inc, slots 24 and 25) use. Reads the owned MG3 pak and quakevr/progs/v_axe.mdl
read-only; writes nothing (prints). Needs numpy.

  python Misc/quakevr/fit_superaxe.py [--pak <rerelease>/mg3/pak0.pak]

The model (progs/v_hammer.mdl in MG3's pak, 795 vertices, 1242 triangles) is a first-person view model: the axe (its
largest piece, pieces joined where vertices coincide: the seams' copies) and the arm that holds it. Its frame 0's handle
(from its end to where the head starts, measured) is put on the axe's handle line (from its end, (0.2, 0, -3.55), along
(4.2, 0, 11.55): vr_axestick.cpp), the blade the same way as the axe's (out of the handle, in the head's plane, the
side the head reaches further). Then: its new scale_origin (the bounds' low corner), the muzzle (the handle's line at
the head's middle; vr_anchor_nearest gives its anchor index, which is in the strip order), and the axe's Offset and
hotspots moved by 0.66 * the scale_origins' difference (a model point p is drawn at k p + (1 - k) o, k = 0.34).
"""
import argparse
import struct

import numpy as np

AXE_SCALE_ORIGIN = np.array([-2.99704432, -1.68134916, -3.90059924])  # quakevr/progs/v_axe.mdl's header
AXE_OFFSET = np.array([-0.7, 0.8, -0.4])
AXE_HOTSPOTS = {"Hotspot1": [1.6, -1.2, 1.8], "Hotspot2": [3.6, -0.6, 8.8]}
AXE_HANDLE_END = np.array([0.2, 0.0, -3.55])
AXE_HANDLE_DIR = np.array([4.2, 0.0, 11.55]) / np.linalg.norm([4.2, 0.0, 11.55])
AXE_HEAD_START = np.array([8.7, -0.1, 20.2])  # where its head starts on the handle (25 units up it)
SCALE = 0.34


def pak_read(path, name):
    with open(path, "rb") as f:
        _, ofs, length = struct.unpack("<4sii", f.read(12))
        f.seek(ofs)
        d = f.read(length)
        for i in range(0, length, 64):
            if d[i:i + 56].split(b"\0")[0].decode() == name:
                pos, size = struct.unpack("<ii", d[i + 56:i + 64])
                f.seek(pos)
                return f.read(size)
    raise SystemExit(f"{name} not in {path}")


def load(b):
    h = struct.unpack("<4si3f3ff3fiiiiiiiif", b[:84])
    scale, org = np.array(h[2:5]), np.array(h[5:8])
    nskins, sw, sh, nv, nt, nf = h[12:18]
    p = 84
    for _ in range(nskins):
        group, = struct.unpack("<i", b[p:p + 4])
        p += 4
        if group == 0:
            p += sw * sh
        else:
            n, = struct.unpack("<i", b[p:p + 4])
            p += 4 + 4 * n + n * sw * sh
    p += nv * 12
    tris = np.frombuffer(b[p:p + nt * 16], dtype="<i4").reshape(nt, 4)[:, 1:]
    p += nt * 16 + 4  # frame 0, a single frame
    raw = np.frombuffer(b[p + 24:p + 24 + nv * 4], dtype=np.uint8).reshape(nv, 4)[:, :3]
    return raw * scale + org, raw, tris


def pieces(raw, tris):
    parent = list(range(len(raw)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for t in tris:
        parent[find(t[0])] = find(t[1])
        parent[find(t[1])] = find(t[2])
    first = {}
    for i, k in enumerate(map(tuple, raw)):
        if k in first:
            parent[find(i)] = find(first[k])
        else:
            first[k] = i
    groups = {}
    for i in range(len(raw)):
        groups.setdefault(find(i), []).append(i)
    return sorted(groups.values(), key=len, reverse=True)


def frame(v, end, head_start, head):
    """Rows: the handle's direction, the blade's, their normal."""
    a = (head_start - end) / np.linalg.norm(head_start - end)
    h = v[head] - v[head].mean(0)
    n = np.linalg.svd(h)[2][2]  # the head's plane's normal: its thinnest direction
    b = np.cross(a, n)
    b /= np.linalg.norm(b)
    d = (v[head] - end) @ b
    if abs(d.min()) > abs(d.max()):
        b = -b
    return np.stack([a, b, np.cross(a, b)])


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--pak", default="C:/Program Files (x86)/Steam/steamapps/common/Quake/rerelease/mg3/pak0.pak")
    ap.add_argument("--axe", default="quakevr/progs/v_axe.mdl")
    args = ap.parse_args()

    v, raw, tris = load(pak_read(args.pak, "progs/v_hammer.mdl"))
    axe = pieces(raw, tris)[0]
    print(f"Super Axe: {len(v)} vertices, {len(tris)} triangles; the axe {len(axe)}, the arm {len(v) - len(axe)}")

    # Its handle in frame 0: its end and where its head starts (the means of its vertices' slices along the axe's
    # principal axis: the handle's slices are under 3 units across, the head's start where they widen).
    end = np.array([14.5, -14.1, -39.2])
    head_start = np.array([29.3, -6.8, -19.0])
    head = [i for i in axe if (v[i] - end) @ (head_start - end) / np.linalg.norm(head_start - end) >
            np.linalg.norm(head_start - end)]

    va, _, _ = load(open(args.axe, "rb").read())
    axe_head = [i for i in range(len(va)) if (va[i] - AXE_HANDLE_END) @ AXE_HANDLE_DIR > 24]
    rot = frame(va, AXE_HANDLE_END, AXE_HEAD_START, axe_head).T @ frame(v, end, head_start, head)
    move = AXE_HANDLE_END - rot @ end
    print("rot rows:", [[round(float(x), 5) for x in row] for row in rot])
    print("move:", [round(float(x), 4) for x in move])

    laid = v @ rot.T + move
    so = laid[axe].min(0)
    print("scale_origin:", np.round(so, 4), "high corner:", np.round(laid[axe].max(0), 3))
    along = (laid[axe] - AXE_HANDLE_END) @ AXE_HANDLE_DIR
    off = np.linalg.norm((laid[axe] - AXE_HANDLE_END) - np.outer(along, AXE_HANDLE_DIR), axis=1)
    head_t = along[off > 6]
    mid = (head_t.min() + head_t.max()) / 2
    print(f"head {head_t.min():.1f} to {head_t.max():.1f} units up the handle; muzzle (its middle):",
          np.round(AXE_HANDLE_END + AXE_HANDLE_DIR * mid, 3), "(vr_anchor_nearest owned/mg3/progs/v_hammer.mdl x y z)")
    d = (1 - SCALE) * (AXE_SCALE_ORIGIN - so)
    print("Offset:", np.round(AXE_OFFSET + d, 3))
    for k, h in AXE_HOTSPOTS.items():
        print(f"{k}:", np.round(np.array(h) - d, 3))


if __name__ == "__main__":
    main()
