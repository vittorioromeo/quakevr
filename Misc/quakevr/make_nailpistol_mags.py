#!/usr/bin/env python3
# make_nailpistol_mags.py -- the nail pistol's magazine as immersive reloading's phase 2 wants it (QC vr_reload.qc,
# vr_nailpistol.qc; docs/vr-port/RELOAD.md), made from the art's own magazine (quakevr/progs/v_nailpistol_mag.mdl,
# Misc/quakevr/models_src/nailpistol/export_mdl.py: its origin at its top, out along its -Z):
#   quakevr/progs/vr_mag_npistol.mdl          the prop (a loose magazine, held, in the pouch): its origin at its middle
#                                              (it tumbles about it), its insertion axis along +z (its top, the feed end,
#                                              up), as make_mags.py's props.
#   quakevr/progs/vr_mag_on_v_nailpistol.mdl  as it sits in the gun, in v_nailpistol.mdl's model space (the Body's frame:
#                                              its top at 0.468 0 2.201, turned 12 degrees about +y as the grip leans
#                                              back; vr_view.cpp setupMagazines draws it with the gun's place and turn).
# It prints the points vr_view.cpp's magMounts and magProps and QC's VR_Reload_MagRef take (the seat: where a held
# magazine's middle sits when seated; its top above its middle), and the prop's size held (vr_pouchitems.cpp magFeet).
#
# Usage: python Misc/quakevr/make_nailpistol_mags.py [output progs folder]

import math
import os
import struct
import sys

import genguard
import mdlgen

HERE = os.path.dirname(os.path.abspath(__file__))
PROGS = os.path.join(HERE, "..", "..", "quakevr", "progs")
SOURCE = "v_nailpistol_mag.mdl"
TILT = math.radians(12.0)           # the grip leans back (build_nailpistol.py GRIP_TILT)
SEAT_TOP = (0.4678, 0.0, 2.2008)    # the magazine's top in the Body's frame (build_nailpistol.py MAG_TOP - HAND)
PROP_SIZE = 0.52                    # its prop slot's Size (vr_props.inc slot 56): the gun's Scale 0.45 as the nailgun's 0.48 to its 0.55


def read_mdl(path):
    """An MDL's skins (raw, w x h each), size, vertices (position, normal, (s, t)) and triangles: frame 0."""
    data = open(path, "rb").read()
    h = mdlgen.HEADER.unpack_from(data, 0)
    scale, origin = h[2:5], h[5:8]
    num_skins, w, hgt, num_verts, num_tris = h[12], h[13], h[14], h[15], h[16]
    off = mdlgen.HEADER.size
    skins = []
    for _ in range(num_skins):
        (group,) = struct.unpack_from("<i", data, off)
        assert group == 0, "single skins"
        skins.append(bytes(data[off + 4:off + 4 + w * hgt]))
        off += 4 + w * hgt
    st = []
    for _ in range(num_verts):
        onseam, s, t = struct.unpack_from("<3i", data, off)
        assert onseam == 0, "no seams (mdlgen's)"
        st.append((s, t))
        off += 12
    tris = []
    for _ in range(num_tris):
        _, a, b, c = struct.unpack_from("<4i", data, off)
        tris.append((a, b, c))
        off += 16
    (ftype,) = struct.unpack_from("<i", data, off)
    assert ftype == 0, "single frames"
    off += 4 + 8 + 16
    table = mdlgen.anorms()
    verts = []
    for i in range(num_verts):
        q = data[off + 4 * i:off + 4 * i + 4]
        p = tuple(q[k] * scale[k] + origin[k] for k in range(3))
        verts.append((p, table[q[3]], st[i]))
    return skins, w, hgt, verts, tris


def mesh_of(w, h, verts, tris, move):
    m = mdlgen.Mesh(w, h, {})
    m.verts = [(move(p), move(n, True), uv) for p, n, uv in verts]
    m.tris = list(tris)
    m.check_winding = lambda: 0  # (the source's smoothed normals: its triangles are the art's, wound as exported)
    return m


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else PROGS
    skins, w, h, verts, tris = read_mdl(os.path.join(PROGS, SOURCE))
    lo = [min(v[0][k] for v in verts) for k in range(3)]
    hi = [max(v[0][k] for v in verts) for k in range(3)]
    mid = [(lo[k] + hi[k]) * 0.5 for k in range(3)]
    c, s = math.cos(TILT), math.sin(TILT)

    def prop(p, normal=False):
        return tuple(p[k] - (0.0 if normal else mid[k]) for k in range(3))

    def seated(p, normal=False):
        x, y, z = p
        r = (c * x + s * z, y, -s * x + c * z)  # turned about +y (Blender's rotation_euler y: +x tips down)
        return r if normal else tuple(r[k] + SEAT_TOP[k] for k in range(3))

    files = [os.path.join(out, "vr_mag_npistol.mdl"), os.path.join(out, "vr_mag_on_v_nailpistol.mdl")]
    guard = genguard.Guard("make_nailpistol_mags.py", files)
    mdlgen.write_mdl(files[0], mesh_of(w, h, verts, tris, prop), skins, "vr_mag_npistol")
    mdlgen.write_mdl(files[1], mesh_of(w, h, verts, tris, seated), skins, "vr_mag_on")
    guard.finish()
    seat = seated(tuple(mid))
    print("source bounds %s .. %s, middle %s" % (["%.3f" % v for v in lo], ["%.3f" % v for v in hi],
                                                 ["%.3f" % v for v in mid]))
    print("prop: top %.3f above its middle; held at Size %.2f: %.3f wide (x), %.3f thick (y), %.3f tall" % (
        hi[2] - mid[2], PROP_SIZE, (hi[0] - lo[0]) * PROP_SIZE, (hi[1] - lo[1]) * PROP_SIZE, (hi[2] - lo[2]) * PROP_SIZE))
    print("seated: its middle (seat) %.4f %.4f %.4f, its top %.4f %.4f %.4f, out along %.4f 0 %.4f" % (
        seat[0], seat[1], seat[2], SEAT_TOP[0], SEAT_TOP[1], SEAT_TOP[2], -s, -c))


if __name__ == "__main__":
    main()
