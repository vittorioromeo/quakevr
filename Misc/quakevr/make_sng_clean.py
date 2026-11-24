# make_sng_clean.py -- the super nailgun's model cleaned on v_nail2.blend (the author's note vrtesthall_2026-11-23_23-19-33):
# - its own muzzle flash taken out: Quake's four flames, triangles collapsed into one point at each barrel's mouth in
#   frame 0 that open out in the firing frames. The barrels' spin plays the firing frames (vr_adweapons.cpp spinFrame),
#   so a flame stood on the gun while the barrels coasted. The flash is the weapon effects' now (vr_weaponfx.cpp, out of
#   the barrel that fires; weapons.txt slot 4 fx_flash 1): only on a shot, never on an empty gun.
# - the band round the barrels (one unit deep at x 36-37, circling all four) taken out: his "metal string".
#
# Usage, from the repository root (the options after the separator: [--dry] [--save other.blend]):
#   Git Bash:   "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup \
#                   Misc/quakevr/src_models/blend/v_nail2/v_nail2.blend --python Misc/quakevr/make_sng_clean.py -- --dry
# Leave out `-- --dry` to export (quakevr/progs/v_nail2.mdl, its normal map, weapons.txt: the barrel mask follows the
# triangles) and save the .blend; then `python Misc/quakevr/make_gun_variants.py build` (v_lava2.mdl, the same model in
# its other skin). Ends with "sngclean_nail2: ok, ..." or "sngclean_nail2: REFUSED: ..." (exit code 1). Run again: nothing
# left to take out ("0 flash triangles, 0 band triangles"), nothing saved. Refuses if any other face's UVs change.

import os
import sys

import bmesh
import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import housing_common as hc  # noqa: E402

STEM = "nail2"
MODEL = "v_nail2"
BAND_X = (35.9, 37.2)   # the band's depth along the barrels (frame 0)
BAND_MIN_SPAN = 10.0    # ... and it circles all four barrels: wider and taller than this


def frame0(me):
    """Frame 0's place of each vertex (the first shape key; the mesh's own without keys)."""
    if me.shape_keys and me.shape_keys.key_blocks:
        return [tuple(d.co) for d in me.shape_keys.key_blocks[0].data]
    return [tuple(v.co) for v in me.vertices]


def doomed(me):
    """The faces to take out: (flash, band) face index sets."""
    co = frame0(me)
    key = [tuple(round(c, 4) for c in p) for p in co]
    flash = {p.index for p in me.polygons if len({key[i] for i in p.vertices}) == 1}
    # The rest's parts, joined where they share a vertex or a place (Quake's seams split vertices).
    parent = list(range(len(co)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    first = {}
    for i, k in enumerate(key):
        if k in first:
            parent[find(i)] = find(first[k])
        else:
            first[k] = i
    for p in me.polygons:
        if p.index in flash:
            continue
        vs = list(p.vertices)
        for v in vs[1:]:
            parent[find(v)] = find(vs[0])
    parts = {}
    for p in me.polygons:
        if p.index not in flash:
            parts.setdefault(find(p.vertices[0]), []).append(p.index)
    band = set()
    for faces in parts.values():
        vs = {i for f in faces for i in me.polygons[f].vertices}
        lo = [min(co[i][a] for i in vs) for a in range(3)]
        hi = [max(co[i][a] for i in vs) for a in range(3)]
        if (lo[0] >= BAND_X[0] and hi[0] <= BAND_X[1] and hi[1] - lo[1] > BAND_MIN_SPAN and
                hi[2] - lo[2] > BAND_MIN_SPAN):
            band.update(faces)
    return flash, band


class Clean(hc.Run):
    def say(self, msg):
        print("sngclean_%s: %s" % (self.stem, msg))

    def fail(self, msg):
        print("sngclean_%s: REFUSED: %s" % (self.stem, msg))
        sys.stdout.flush()
        sys.exit(1)

    def gun_uvs(self):
        """The UVs of the faces this script keeps, each corner by its place (indices shift as faces go)."""
        me = bpy.data.objects[self.model].data
        flash, band = doomed(me)
        uv = me.uv_layers.active.data
        faces = []
        for p in me.polygons:
            if p.index in flash or p.index in band:
                continue
            faces.append(tuple(sorted((tuple(round(c, 4) for c in me.vertices[me.loops[li].vertex_index].co),
                                       round(uv[li].uv[0], 6), round(uv[li].uv[1], 6)) for li in p.loop_indices)))
        return sorted(faces)


def main(h):
    ob = bpy.data.objects[MODEL]
    me = ob.data
    flash, band = doomed(me)
    nflash, nband = len(flash), len(band)
    print("sngclean: %d flash triangles (frame 0 points), %d band triangles, of %d" % (nflash, nband, len(me.polygons)))
    if nband and nband > 400:
        h.fail("the band found is %d triangles: more than the band (156), the barrels caught too?" % nband)
    if flash or band:
        bm = bmesh.new()
        bm.from_mesh(me)
        bm.faces.ensure_lookup_table()
        gone = [bm.faces[i] for i in sorted(flash | band)]
        bmesh.ops.delete(bm, geom=gone, context="FACES_ONLY")
        loose = [v for v in bm.verts if not v.link_faces]
        bmesh.ops.delete(bm, geom=loose, context="VERTS")
        bm.to_mesh(me)
        bm.free()
        me.update()
        ob["qvr_sngclean"] = "flash and band taken out (make_sng_clean.py)"
    h.moved_verts, h.kept, h.made = 0, len(me.polygons), 0
    h.finish("%d flash and %d band triangles taken out, %d left" % (nflash, nband, len(me.polygons)))


Clean(STEM, MODEL).main(main)
