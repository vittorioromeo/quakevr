"""bspvis.py -- read a Quake BSP (version 29) and answer PVS questions about it.

Used for measuring visibility in `start` (the hidden staircase report). No engine needed.
    python bspvis.py start.bsp ents "<x> <y> <z>" <radius>
    python bspvis.py start.bsp leaf "<x> <y> <z>"
    python bspvis.py start.bsp diff "<x1 y1 z1>" "<x2 y2 z2>"
    python bspvis.py start.bsp box "<minx miny minz> <maxx maxy maxz>"
"""
import struct
import sys

CONTENTS = {-1: "empty", -2: "solid", -3: "water", -4: "slime", -5: "lava", -6: "sky", -7: "origin", -8: "clip"}


class BSP:
    def __init__(self, path):
        d = open(path, "rb").read()
        ver = struct.unpack_from("<i", d, 0)[0]
        assert ver == 29, ver
        lumps = [struct.unpack_from("<ii", d, 4 + 8 * i) for i in range(15)]

        def lump(i):
            o, s = lumps[i]
            return d[o:o + s]

        self.ents = lump(0).split(b"\n")
        planes = [struct.unpack_from("<ffffi", lump(1), 20 * i) for i in range(len(lump(1)) // 20)]
        self.planes = planes
        verts = [struct.unpack_from("<fff", lump(3), 12 * i) for i in range(len(lump(3)) // 12)]
        self.verts = verts
        self.visblob = lump(4)  # leaf->visofs is a byte offset straight into it
        nodes = [struct.unpack_from("<i2h6h2H", lump(5), 24 * i) for i in range(len(lump(5)) // 24)]
        self.nodes = nodes
        texinfo = [struct.unpack_from("<8fii", lump(6), 48 * i) for i in range(len(lump(6)) // 48)]
        self.texinfo = texinfo
        faces = [struct.unpack_from("<hhihh4Bi", lump(7), 20 * i) for i in range(len(lump(7)) // 20)]
        self.faces = faces
        self.models = [struct.unpack_from("<9f4i3i", lump(14), 64 * i) for i in range(len(lump(14)) // 64)]
        leafs = [struct.unpack_from("<ii6h2H4B", lump(10), 28 * i) for i in range(len(lump(10)) // 28)]
        self.leafs = leafs
        self.numleafs = len(leafs)
        self.marksurfaces = [struct.unpack_from("<H", lump(11), 2 * i)[0] for i in range(len(lump(11)) // 2)]
        self.surfedges = [struct.unpack_from("<i", lump(13), 4 * i)[0] for i in range(len(lump(13)) // 4)]
        self.edges = [struct.unpack_from("<HH", lump(12), 4 * i) for i in range(len(lump(12)) // 4)]
        self.texnames = self._texnames(lump(2))

    def _texnames(self, blob):
        if not blob:
            return []
        n = struct.unpack_from("<i", blob, 0)[0]
        out = []
        for i in range(n):
            o = struct.unpack_from("<i", blob, 4 + 4 * i)[0]
            out.append(blob[o:o + 16].split(b"\0")[0].decode("latin-1"))
        return out

    def pointleaf(self, p):
        """The leaf index a point is in (the engine's Mod_PointInLeaf)."""
        i = 0
        while i >= 0:
            node = self.nodes[i]
            plane = self.planes[node[0]]
            d = plane[0] * p[0] + plane[1] * p[1] + plane[2] * p[2] - plane[3]
            i = node[1] if d > 0 else node[2]
            if i < 0:
                return -(i + 1)
        return -(i + 1)

    def decompress(self, leaf):
        """The PVS bit string of a leaf (a bytearray of numleafs bits), as Mod_DecompressVis."""
        n = (self.numleafs + 7) >> 3
        out = bytearray(n)
        ofs = self.leafs[leaf][1]
        if ofs < 0:
            for i in range(n):
                out[i] = 0xFF
            return out
        src = self.visblob[ofs:]
        k = 0
        i = 0
        while k < n and i < len(src):
            c = src[i]
            i += 1
            if c == 0:
                run = min(src[i], n - k) if i < len(src) else 0
                i += 1
                k += run
            else:
                c = min(c, n - k)
                for _ in range(c):
                    out[k] = src[i] if i < len(src) else 0
                    k += 1
                    i += 1
        return out

    @staticmethod
    def bits(data):
        """The leaf numbers a PVS bit string names (the bits are 1-based: bit b is leaf b+1)."""
        n = len(data) * 8
        return [b for b in range(n) if data[b >> 3] & (1 << (b & 7))]

    def leafbox(self, leaf):
        lf = self.leafs[leaf]
        return lf[2:5], lf[5:8]

    def leaffaces(self, leaf):
        lf = self.leafs[leaf]
        first, count = lf[8], lf[9]
        return self.marksurfaces[first:first + count]

    def faceinfo(self, face):
        f = self.faces[face]
        n = f[3]
        pts = []
        for k in range(n):
            e = self.surfedges[f[2] + k]
            v = self.edges[abs(e)][0 if e > 0 else 1]
            pts.append(self.verts[v])
        cx = sum(p[0] for p in pts) / max(n, 1)
        cy = sum(p[1] for p in pts) / max(n, 1)
        cz = sum(p[2] for p in pts) / max(n, 1)
        ti = self.texinfo[f[4]]
        mi = ti[8]
        return (cx, cy, cz), (self.texnames[mi] if 0 <= mi < len(self.texnames) else "?")

    def show_leaf(self, leaf, maxfaces=6):
        lo, hi = self.leafbox(leaf)
        print(f"  leaf {leaf} {CONTENTS.get(self.leafs[leaf][0], self.leafs[leaf][0])} box ({lo[0]:.0f} {lo[1]:.0f} "
              f"{lo[2]:.0f})-({hi[0]:.0f} {hi[1]:.0f} {hi[2]:.0f}) faces {self.leafs[leaf][9]}")
        for f in self.leaffaces(leaf)[:maxfaces]:
            c, tex = self.faceinfo(f)
            print(f"      face {f} tex {tex:<16} centre ({c[0]:.0f} {c[1]:.0f} {c[2]:.0f})")


def parse_args(argv):
    return [tuple(float(x) for x in a.split()) for a in argv]


def main():
    path, cmd = sys.argv[1], sys.argv[2]
    b = BSP(path)
    print(f"{path}: {b.numleafs} leafs, {len(b.faces)} faces, {len(b.nodes)} nodes, {len(b.ents)} entity lines")
    args = parse_args(sys.argv[3:])
    if cmd == "ents":
        c, r = args[0], args[1][0]
        for line in b.ents:
            s = line.decode("latin-1")
            if '"origin"' not in s:
                continue
            o = None
            for part in s.split('"'):
                if part.startswith("origin "):
                    o = [float(x) for x in part.split()[1:4]]
            if o is None:
                continue
            if max(abs(o[i] - c[i]) for i in range(3)) <= r:
                print("  ", s.strip()[:220])
    elif cmd == "leaf":
        leaf = b.pointleaf(args[0])
        print(f"point {args[0]} leaf {leaf} contents {CONTENTS.get(b.leafs[leaf][0])} "
              f"pvs_leaves {len(b.bits(b.decompress(leaf)))}")
        b.show_leaf(leaf)
    elif cmd == "diff":
        a, c = args
        la, lc = b.pointleaf(a), b.pointleaf(c)
        va, vc = b.bits(b.decompress(la)), b.bits(b.decompress(lc))
        sa, sc = set(va), set(vc)
        print(f"leaf {la} pvs {len(sa)}  vs  leaf {lc} pvs {len(sc)}")
        only_a = sorted(sa - sc)
        only_c = sorted(sc - sa)
        print(f"  only in the first: {len(only_a)} {only_a[:40]}")
        print(f"  only in the second: {len(only_c)} {only_c[:40]}")
        for leaf in only_c[:12]:
            b.show_leaf(leaf)
    elif cmd == "box":
        lo, hi = args
        for leaf in range(1, b.numleafs):
            lmin, lmax = b.leafbox(leaf)
            if all(lmin[i] >= lo[i] and lmax[i] <= hi[i] for i in range(3)):
                b.show_leaf(leaf)


main()
