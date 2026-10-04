"""bspvis.py -- read a Quake BSP (version 29) and answer PVS questions about it.

Used for measuring visibility in `start` (the hidden staircase report). No engine needed.
    python bspvis.py start.bsp ents "<x> <y> <z>" <radius>
    python bspvis.py start.bsp leaf "<x> <y> <z>"
    python bspvis.py start.bsp diff "<x1 y1 z1>" "<x2 y2 z2>"
    python bspvis.py start.bsp box "<minx miny minz> <maxx maxy maxz>"
    python bspvis.py start.bsp audit
"""
import re
import struct
import sys

CONTENTS = {-1: "empty", -2: "solid", -3: "water", -4: "slime", -5: "lava", -6: "sky", -7: "origin", -8: "clip"}


class BSP:
    def __init__(self, path):
        with open(path, "rb") as source:
            d = source.read()
        ver = struct.unpack_from("<i", d, 0)[0]
        assert ver == 29, ver
        lumps = [struct.unpack_from("<ii", d, 4 + 8 * i) for i in range(15)]

        def lump(i):
            o, s = lumps[i]
            return d[o:o + s]

        self.ents = [dict(re.findall(r'"([^"\n]*)"\s*"([^"\n]*)"', block))
                     for block in re.findall(r'\{([^{}]*)\}', lump(0).decode("latin-1"))]
        planes = [struct.unpack_from("<ffffi", lump(1), 20 * i) for i in range(len(lump(1)) // 20)]
        self.planes = planes
        verts = [struct.unpack_from("<fff", lump(3), 12 * i) for i in range(len(lump(3)) // 12)]
        self.verts = verts
        self.visblob = lump(4)  # leaf->visofs is a byte offset straight into it
        nodes = [struct.unpack_from("<i2h6h2H", lump(5), 24 * i) for i in range(len(lump(5)) // 24)]
        self.nodes = nodes
        texinfo = [struct.unpack_from("<8fii", lump(6), 40 * i) for i in range(len(lump(6)) // 40)]
        self.texinfo = texinfo
        faces = [struct.unpack_from("<hhihh4Bi", lump(7), 20 * i) for i in range(len(lump(7)) // 20)]
        self.faces = faces
        self.models = [struct.unpack_from("<9f4i3i", lump(14), 64 * i) for i in range(len(lump(14)) // 64)]
        leafs = [struct.unpack_from("<ii6h2H4B", lump(10), 28 * i) for i in range(len(lump(10)) // 28)]
        self.leafs = leafs
        # Submodels have their own leaf records. PVS rows contain only world visibility leaves,
        # excluding the common solid leaf 0 (dmodel_t.visleafs).
        self.numleafs = self.models[0][13]
        if not 0 <= self.numleafs < len(leafs):
            raise ValueError("invalid world visibility leaf count")
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
        if not 0 <= leaf <= self.numleafs:
            raise ValueError("not a world leaf")
        ofs = self.leafs[leaf][1]
        if leaf == 0 or ofs < 0:
            return bytearray([0xFF]) * n
        return decompress_vis(self.visblob, ofs, n)

    def bits(self, data):
        """World leaf numbers: PVS bit b names BSP leaf b+1; omit padding bits."""
        return [b + 1 for b in range(self.numleafs) if data[b >> 3] & (1 << (b & 7))]

    def leafbox(self, leaf):
        lf = self.leafs[leaf]
        return lf[2:5], lf[5:8]

    def entity_origin(self, ent):
        """Point entity's origin, or brush bounds centre (brush entities often omit origin)."""
        if "origin" in ent:
            return tuple(float(x) for x in ent["origin"].split())
        model = ent.get("model", "")
        if model.startswith("*"):
            bounds = self.models[int(model[1:])]
            return tuple((bounds[i] + bounds[i + 3]) * 0.5 for i in range(3))
        return None

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
            v = self.edges[abs(e)][0 if e >= 0 else 1]
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


def decompress_vis(blob, offset, row_size):
    """Quake RLE: nonzero bytes are literals; zero followed by N means N zero bytes."""
    out = bytearray()
    while len(out) < row_size:
        if not 0 <= offset < len(blob):
            raise ValueError("truncated PVS row")
        value = blob[offset]
        offset += 1
        if value:
            out.append(value)
            continue
        if offset >= len(blob):
            raise ValueError("truncated PVS zero run")
        count = blob[offset]
        offset += 1
        if count == 0 or len(out) + count > row_size:
            raise ValueError("invalid PVS zero run")
        out.extend(bytes(count))
    return out


def parse_args(argv):
    return [tuple(float(x) for x in a.split()) for a in argv]


def main():
    path, cmd = sys.argv[1], sys.argv[2]
    b = BSP(path)
    print(f"{path}: {b.numleafs} leafs, {len(b.faces)} faces, {len(b.nodes)} nodes, {len(b.ents)} entities")
    args = parse_args(sys.argv[3:])
    if cmd == "audit":
        missing = [leaf for leaf in range(1, b.numleafs + 1)
                   if leaf not in b.bits(b.decompress(leaf))]
        print(f"{b.numleafs} world visibility leaves; {len(b.leafs)} total leaf records; "
              f"{len(missing)} missing self bits: {missing}")
    elif cmd == "ents":
        c, r = args[0], args[1][0]
        for ent in b.ents:
            o = b.entity_origin(ent)
            if o is None:
                continue
            if max(abs(o[i] - c[i]) for i in range(3)) <= r:
                print("  ", ent)
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
        for leaf in range(1, b.numleafs + 1):
            lmin, lmax = b.leafbox(leaf)
            if all(lmin[i] >= lo[i] and lmax[i] <= hi[i] for i in range(3)):
                b.show_leaf(leaf)


if __name__ == "__main__":
    main()
