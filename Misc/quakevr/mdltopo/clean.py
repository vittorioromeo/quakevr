# clean.py -- the topology fixes that need no render gate, for the script-made models (models_src/*/export_mdl.py):
# triangles of zero area in every pose dropped, then vertices identical in everything (bytes and normal in every pose,
# texel coords, seam flag) welded. The same rules and order as fix.py's degen and weld steps, so a generator's output
# matches what fix.py made of it byte for byte (both are invisible by construction: compare.py 0 px on every model
# fix.py changed this way).
#   python clean.py model.mdl [...]       (in place)
#   import clean; clean.clean_file(path)  -> (triangles dropped, vertices welded)
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import audit  # noqa: E402
import mdlio  # noqa: E402


def weld(m):
    """fix.py step_weld without the gate: (welded model, vertices welded)."""
    key = np.concatenate([m.bytes.transpose(1, 0, 2).reshape(m.nv, -1), m.normidx.T,
                          np.stack([m.s, m.t, m.onseam], 1)], 1)
    _, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    rep = first[inv.reshape(-1)]
    keepv = rep == np.arange(m.nv)
    if keepv.all():
        return m, 0
    newidx = np.cumsum(keepv) - 1
    m.tris = newidx[rep[m.tris]]
    m.onseam, m.s, m.t = m.onseam[keepv], m.s[keepv], m.t[keepv]
    m.bytes, m.normidx = m.bytes[:, keepv], m.normidx[:, keepv]
    m.nv = int(keepv.sum())
    return m, int((~keepv).sum())


def clean_file(path):
    a = audit.Audit(path, hidden=False)
    a.degenerate()
    m = mdlio.read(path)
    keep = ~a.degen_all
    m.tris, m.facesfront = m.tris[keep].copy(), m.facesfront[keep].copy()
    m.nt = int(keep.sum())
    m, nw = weld(m)
    mdlio.write(m, path)
    return int((~keep).sum()), nw


if __name__ == "__main__":
    for p in sys.argv[1:]:
        d, w = clean_file(p)
        print("clean: %s: %d zero-area triangles dropped, %d vertices welded" % (os.path.basename(p), d, w))
