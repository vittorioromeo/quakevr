# zfpairs.py outdir model.mdl... -> writes <outdir>/<name>.pairs.json: the static different-texel z-fight pairs (risk_static_diff)
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import audit
out = sys.argv[1]
for p in sys.argv[2:]:
    au = audit.Audit(p, hidden=True)
    au.run()
    sel = [q for q in au.zpairs if (not q["hidden"]) and q["kind"] in ("exact", "crossing") and q["all"] and q["same_texels"] < 0.95]
    hid = [int(t) for t in getattr(au, "hidden_tris", [])]
    json.dump({"pairs": sel, "hidden_tris": hid, "nt": int(au.m.nt)}, open(os.path.join(out, os.path.basename(p) + ".pairs.json"), "w"),
              default=lambda o: o.item() if hasattr(o, "item") else str(o))
    print(os.path.basename(p), "static diff pairs", len(sel), "tris", len({q["a"] for q in sel} | {q["b"] for q in sel}), flush=True)
