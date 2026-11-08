# zfsheet.py model.mdl out.png [pairlist a:b,...] : contact sheet, one row per pair: tint (a red, b blue) | a wins | b wins
import json, os, sys
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import zflib, mdlio, raster  # noqa

p, outp = sys.argv[1], sys.argv[2]
name = os.path.basename(p)
m = mdlio.read(p)
if len(sys.argv) > 3:
    pairs = [tuple(int(x) for x in s.split(":")) for s in sys.argv[3].split(",")]
else:
    pairs = [(q["a"], q["b"]) for q in json.load(open(os.path.join(os.environ.get("ZF_WORK", "."), name + ".pairs.json")))["pairs"]]
size = int(os.environ.get("ZF_SIZE", "384"))
sc = zflib.Scene(m, size=size)
step = float(m.scale.max())
P = m.pos.shape[0]
poses = sorted(set([0, P // 2, P - 1]))
CELL = 112
rows = []
for k, (a, b) in enumerate(pairs):
    con = zflib.contested(sc, a, b, poses, 1.5 * step)
    if not con:
        print(k, a, b, "no contested px")
        rows.append(np.full((CELL, 3 * CELL + 8, 3), 30, np.uint8))
        continue
    (f, vi), (idx, ta, tb, sha, shb) = max(con.items(), key=lambda kv: int(np.count_nonzero(kv[1][1] != kv[1][2])) + len(kv[1][0]) * 0.01)
    tid, dep, tex, sh = sc.render(f, vi)
    base = raster.rgb(tex, sh, zflib.PAL, size).reshape(-1, 3).astype(np.float64)
    ia, ib = base.copy(), base.copy()
    ia[idx] = zflib.PAL[ta] * sha[:, None]
    ib[idx] = zflib.PAL[tb] * shb[:, None]
    tint = base.copy()
    tint[tid == a] = tint[tid == a] * 0.4 + np.array([255, 0, 0]) * 0.6
    tint[tid == b] = tint[tid == b] * 0.4 + np.array([0, 80, 255]) * 0.6
    tint[idx] = tint[idx] * 0.4 + np.array([255, 0, 255]) * 0.6
    ys, xs = idx // size, idx % size
    cy, cx = (ys.min() + ys.max()) // 2, (xs.min() + xs.max()) // 2
    half = max(20, int(max(ys.max() - ys.min(), xs.max() - xs.min()) * 0.8) + 10)
    half = min(half, size // 2)
    y0, x0 = int(np.clip(cy - half, 0, size - 2 * half)), int(np.clip(cx - half, 0, size - 2 * half))
    cells = []
    for img in (tint, ia, ib):
        im = np.clip(img * float(os.environ.get("ZF_GAIN", "2")), 0, 255).astype(np.uint8).reshape(size, size, 3)[y0:y0 + 2 * half, x0:x0 + 2 * half]
        r = np.linspace(0, 2 * half - 1, CELL).astype(int)
        cells.append(im[r][:, r])
        cells.append(np.full((CELL, 4, 3), 0, np.uint8))
    rows.append(np.concatenate(cells[:-1] + [np.full((CELL, 4, 3), 0, np.uint8)], 1)[:, :3 * CELL + 8])
    print(k, a, b, "pose", f, "view", vi, "px", len(idx), "diff", int(np.count_nonzero(ta != tb)))
per = 12
cols = [rows[i:i + per] for i in range(0, len(rows), per)]
H = per * (CELL + 4)
out = []
for c in cols:
    col = np.concatenate([np.concatenate([r, np.full((4, r.shape[1], 3), 255, np.uint8)], 0) for r in c], 0)
    col = np.concatenate([col, np.full((H - col.shape[0], col.shape[1], 3), 255, np.uint8)], 0)
    out += [col, np.full((H, 10, 3), 255, np.uint8)]
raster.write_png(outp, np.concatenate(out, 1))
