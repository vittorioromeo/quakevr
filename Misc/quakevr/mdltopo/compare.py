# compare.py -- the render-comparison gate for model topology fixes.
#   python compare.py A.mdl B.mdl [--size 256] [--views cube|axes|fibN] [--frames all|N|i,j,k] [--out diff.png]
#                     [--max-texel-px N] [--max-shade D]
# Renders both models with the same framing (the union of their boxes over all poses) from every view in every frame
# (orthographic, back faces culled, skin 0's texels unfiltered, Gouraud shade from the vertex normals and a fixed light)
# and diffs them per pixel:
#   texel px  pixels whose skin texel (palette index) or coverage differs  (the look of the surface)
#   shade     max |shade A - shade B| over pixels both cover (0..1)          (lighting from the vertex normals)
#   rgb       max / mean |RGB A - RGB B| (palette colour * shade)            (both together)
# Ends with "compare: PASS ..." or "compare: FAIL ..." (exit 1) against the thresholds (default: 0 texel px, shade 0.02).
# Z-fighting pairs resolve by triangle order (stable), so two coplanar faces with different texels are a real
# difference only when the winner changes: an unchanged model always compares exactly equal.
import argparse
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mdlio  # noqa: E402
import raster  # noqa: E402


def views(spec):
    if spec == "cube":
        return raster.cube_dirs()
    if spec == "axes":
        return np.array([(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)], dtype=np.float64)
    if spec.startswith("fib"):
        return raster.fib_dirs(int(spec[3:]))
    raise SystemExit("bad --views")


def frames(spec, P):
    if spec == "all":
        return list(range(P))
    if "," in spec:
        return [int(x) for x in spec.split(",")]
    n = int(spec)
    return sorted(set(np.linspace(0, P - 1, min(n, P)).round().astype(int).tolist()))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--size", type=int, default=256)
    ap.add_argument("--views", default="cube")
    ap.add_argument("--frames", default="all")
    ap.add_argument("--out", default=None)
    ap.add_argument("--max-texel-px", type=int, default=0)
    ap.add_argument("--max-shade", type=float, default=0.02)
    ap.add_argument("--debug", type=int, default=0, help="print up to N differing pixels per render (their triangles)")
    a = ap.parse_args()
    t0 = time.time()
    A, B = mdlio.read(a.a), mdlio.read(a.b)
    if A.pos.shape[0] != B.pos.shape[0]:
        print("compare: FAIL pose count differs (%d vs %d)" % (A.pos.shape[0], B.pos.shape[0]))
        sys.exit(1)
    an = mdlio.anorms()
    pal = mdlio.palette().astype(np.float64)
    c, R = raster.framing(A, B)
    SA, SB = raster.Shaded(A, mdlio.tri_uv(A), an), raster.Shaded(B, mdlio.tri_uv(B), an)
    V = views(a.views)
    fl = frames(a.frames, A.pos.shape[0])
    tot_tex = 0
    max_sh = 0.0
    max_rgb = 0.0
    sum_rgb = 0.0
    npx = 0
    worst = (-1, None)
    for f in fl:
        for vi, d in enumerate(V):
            view = raster.View(d, c, R, a.size)
            ida, dpa, ta, sa = SA.render(f, view)
            idb, dpb, tb, sb = SB.render(f, view)
            dt = int(np.count_nonzero(ta != tb))
            if a.debug and dt:
                k = np.nonzero((ta != tb) | (np.abs(sa - sb) > a.max_shade))[0][:a.debug]
                print("  pose %d view %d: px %s triA %s triB %s depthA-B %s" % (
                    f, vi, k.tolist(), ida[k].tolist(), idb[k].tolist(), np.round(dpa[k] - dpb[k], 6).tolist()))
            both = (ta >= 0) & (tb >= 0)
            ds = float(np.abs(sa - sb)[both].max()) if both.any() else 0.0
            ia = raster.rgb(ta, sa, pal, a.size).astype(np.int32)
            ib = raster.rgb(tb, sb, pal, a.size).astype(np.int32)
            dr = np.abs(ia - ib)
            tot_tex += dt
            max_sh = max(max_sh, ds)
            max_rgb = max(max_rgb, float(dr.max()))
            sum_rgb += float(dr.sum())
            npx += dr.size
            score = dt + int(ds * 1000)
            if score > worst[0]:
                worst = (score, (f, vi, ia, ib, dr))
    mean_rgb = sum_rgb / max(npx, 1)
    print("compare: %d poses x %d views at %dpx: texel px %d, shade max %.4f, rgb max %d mean %.5f (%.1fs)"
          % (len(fl), len(V), a.size, tot_tex, max_sh, max_rgb, mean_rgb, time.time() - t0))
    if a.out and worst[1] is not None:
        f, vi, ia, ib, dr = worst[1]
        mask = (dr.max(-1) > 0)
        hl = ib.copy()
        hl[mask] = (255, 0, 255)
        img = np.concatenate([ia, ib, hl], 1)
        raster.write_png(a.out, img)
        print("compare: worst pose %d view %d -> %s (A | B | B with differing px magenta)" % (f, vi, a.out))
    ok = tot_tex <= a.max_texel_px and max_sh <= a.max_shade
    print("compare: %s" % ("PASS" if ok else "FAIL"))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
