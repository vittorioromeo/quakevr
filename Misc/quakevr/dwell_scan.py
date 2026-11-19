#!/usr/bin/env python3
# dwell_scan.py -- Phase 0 analysis of Dwell (v2.2) and Copper for Quake VR (docs/vr-port/DWELL.md).
#
# Reads (never writes) a Dwell game folder, Copper's QuakeC, a decompile of Dwell's progs.dat (fteqcc -d), our QC, the
# official paks and installed Map Library packages, and writes plain-text tables into --out:
#   classes.txt   every spawnable Dwell/Copper classname: placements, maps, who defines it (Copper, Dwell, ours, AD)
#   permap.txt    per Dwell map: entity count, Dwell-only and Copper-only classnames, worldspawn keys, music entities
#   assets.txt    Dwell's files vs id1/Hipnotic/Rogue/quakevr/AD (same name: identical or different), MDL/SPR versions
#   music.txt     the music folder (sizes, hashes, duplicates) and which track each map asks for
#   detect.txt    the Dwell/Copper signal score of every map: Dwell's, official ones, AD's, installed packages
#   funcs.txt     functions Dwell adds to Copper and Copper functions Dwell dropped (decompile vs source)
#
#   python -I Misc/quakevr/dwell_scan.py --dwell <dwellv2p2> --copper <copper_src> --decomp <decompile dir>
#          --qc QC --quakevr quakevr --official <pak or folder>... --addons <qvr_addons> --out scratch/dwell_an
#
# Treats every input as data (no code from them is run). Python 3.8+, standard library only.

import argparse
import collections
import glob
import hashlib
import os
import re
import struct

SPAWN_RE = re.compile(
    r"^(monster|item|weapon|func|trigger|info|misc|target|path|light|ambient|trap|play|event|air_bubbles|viewthing|"
    r"worldspawn|fx|dev|test|ammo|key|rune|hazard|shooter|storycam|point|env|camera|cutscene|noclass|gib|"
    r"bullethell|dwell|boss|text|book|fog|sky)")
FUNC_RE = re.compile(r"^\s*void\s*\(\s*\)\s*(\w+)\s*(?:=|\{)", re.M)
FUNC_C_RE = re.compile(r"^\s*void\s+(\w+)\s*\(\s*(?:void)?\s*\)\s*(?:=\s*)?\{?\s*$", re.M)
ANYFUNC_RE = re.compile(r"^\s*[\w.]+\s*\([^)]*\)\s*(\w+)\s*(?:=|\{)", re.M)


def pak_entries(path):
    """[(name lowercased, offset, length)] of a .pak."""
    with open(path, "rb") as f:
        head = f.read(12)
        if head[:4] != b"PACK":
            return []
        dirofs, dirlen = struct.unpack("<ii", head[4:12])
        f.seek(dirofs)
        d = f.read(dirlen)
    out = []
    for i in range(0, dirlen, 64):
        name = d[i:i + 56].split(b"\0", 1)[0].decode("latin-1").lower()
        ofs, ln = struct.unpack("<ii", d[i + 56:i + 64])
        out.append((name, ofs, ln))
    return out


class Source:
    """A game folder or a single pak: name -> a reader of the bytes."""

    def __init__(self, label, paths):
        self.label = label
        self.files = {}
        for p in paths:
            if os.path.isdir(p):
                for root, _, names in os.walk(p):
                    for n in names:
                        full = os.path.join(root, n)
                        rel = os.path.relpath(full, p).replace("\\", "/").lower()
                        if n.lower().endswith(".pak"):
                            self._pak(full)
                        else:
                            self.files.setdefault(rel, ("file", full, 0, os.path.getsize(full)))
            elif p.lower().endswith(".pak") and os.path.isfile(p):
                self._pak(p)

    def _pak(self, full):
        # (a later pak shadows an earlier one: paks are read in name order, pak1 over pak0)
        for name, ofs, ln in pak_entries(full):
            self.files[name] = ("pak", full, ofs, ln)

    def read(self, name):
        kind, full, ofs, ln = self.files[name]
        with open(full, "rb") as f:
            if kind == "pak":
                f.seek(ofs)
                return f.read(ln)
            return f.read()

    def size(self, name):
        return self.files[name][3]


def bsp_entities(data):
    if len(data) < 12:
        return []
    ofs, ln = struct.unpack("<ii", data[4:12])
    text = data[ofs:ofs + ln].split(b"\0", 1)[0].decode("latin-1", "replace")
    ents, cur = [], None
    for m in re.finditer(r'\{|\}|"([^"]*)"', text):
        t = m.group(0)
        if t == "{":
            cur, key = {}, None
        elif t == "}":
            if cur is not None:
                ents.append(cur)
            cur = None
        elif cur is not None:
            if key is None:
                key = m.group(1)
            else:
                cur[key.lower()] = m.group(1)
                key = None
    return ents


def qc_funcs(paths):
    """{function name: file} of every function defined in the .qc files (any signature)."""
    out = {}
    for p in paths:
        with open(p, encoding="latin-1") as f:
            text = re.sub(r"//[^\n]*", "", f.read())
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        for rx in (ANYFUNC_RE, FUNC_C_RE):
            for m in rx.finditer(text):
                out.setdefault(m.group(1).lower(), os.path.basename(p))
    return out


def spawnfuncs(paths):
    out = {}
    for p in paths:
        with open(p, encoding="latin-1") as f:
            text = re.sub(r"//[^\n]*", "", f.read())
        for rx in (FUNC_RE, FUNC_C_RE):
            for m in rx.finditer(text):
                n = m.group(1).lower()
                if SPAWN_RE.match(n):
                    out.setdefault(n, os.path.basename(p))
    return out


def quaked(paths):
    out = set()
    for p in paths:
        with open(p, encoding="latin-1") as f:
            out.update(m.group(1).lower() for m in re.finditer(r"/\*QUAKED\s+(\w+)", f.read()))
    return out


def maps_of(src):
    return sorted(n for n in src.files if n.startswith("maps/") and n.endswith(".bsp") and n.count("/") == 1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dwell", required=True)
    ap.add_argument("--copper", required=True)
    ap.add_argument("--decomp", required=True)
    ap.add_argument("--qc", required=True)
    ap.add_argument("--quakevr", required=True)
    ap.add_argument("--official", nargs="*", default=[], help="label=path[,path...]")
    ap.add_argument("--addons", default="")
    ap.add_argument("--index", default="")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)

    dwell = Source("dwell", [a.dwell])
    copper_qc = sorted(glob.glob(os.path.join(a.copper, "*.qc")))
    decomp_qc = sorted(glob.glob(os.path.join(a.decomp, "*.qc")))
    ours_qc = sorted(glob.glob(os.path.join(a.qc, "*.qc")))
    ad_qc = sorted(glob.glob(os.path.join(a.qc, "ad", "*.qc")))

    cop = spawnfuncs(copper_qc)
    cop_quaked = quaked(copper_qc)
    dw = spawnfuncs(decomp_qc)
    ours = spawnfuncs(ours_qc)
    ad_raw = spawnfuncs(ad_qc)
    ad = {}
    for n, f in ad_raw.items():
        ad[n] = f
    # AD's import prefixes every spawn function: ad_<classname> (AD-only names) and ad__<classname> (shared names).
    for n in qc_funcs(ad_qc):
        if n.startswith("ad__"):
            ad[n[4:]] = "ad__"
        elif n.startswith("ad_"):
            ad.setdefault(n[3:], "ad_")
    cop_all = qc_funcs(copper_qc)
    dw_all = qc_funcs(decomp_qc)

    # ---- per map entities
    dmaps = maps_of(dwell)
    per = {}
    for m in dmaps:
        per[m] = bsp_entities(dwell.read(m))
    use = collections.Counter()
    usemaps = collections.defaultdict(set)
    keys_by_class = collections.defaultdict(collections.Counter)
    for m, ents in per.items():
        for e in ents:
            c = e.get("classname", "").lower()
            use[c] += 1
            usemaps[c].add(m[5:-4])
            for k in e:
                keys_by_class[c][k] += 1

    # Spawnables: what the maps place, Copper's QUAKED classes, and the decompile's entity-like functions that are not
    # helpers (frames, think/touch/use...). The prefix heuristic alone would count every frame function.
    strict = re.compile(r"^(monster|item|weapon|func|trigger|target|misc|info|path|light|ambient|trap)_|^light$")
    helper = re.compile(r"(\d+[a-z]?|_(think|touch|use|spawn|spawner|go|blocked|die|pain|start|fire|point|setup|on|off|"
                        r"toggle|reset|wait|done|init|precache|remove|check|hit|attack|run|walk|stand|update|blink|"
                        r"find|go_down|go_up|crush|trigger|activate|fade|flicker|loop|bob|end|next|move|aim|explode))$")
    dw_spawn = set(n for n in dw if strict.match(n) and not helper.search(n)) | (set(use) & set(dw))
    cop_spawn = cop_quaked | (set(use) & set(cop))
    cop = {n: cop[n] for n in cop if n in cop_spawn}
    cop.update({n: "QUAKED" for n in cop_quaked if n not in cop})
    dw = {n: dw[n] for n in dw if n in dw_spawn}
    names = sorted(set(use) | set(dw) | set(cop))
    dwell_only = set()
    copper_only = set()
    rows = []
    for n in names:
        where = []
        if n in cop:
            where.append("copper")
        if n in dw:
            where.append("dwell")
        if n in ours:
            where.append("ours")
        if n in ad:
            where.append("AD" + ("(ad__)" if ad[n] == "ad__" else ""))
        if n in dw and n not in cop and n not in ours and n not in ad:
            dwell_only.add(n)
        if n in cop and n not in ours and n not in ad:
            copper_only.add(n)
        rows.append((n, use[n], len(usemaps[n]), ",".join(where) or "NONE", ",".join(sorted(usemaps[n]))[:120]))
    with open(os.path.join(a.out, "classes.txt"), "w") as f:
        f.write("classname\tplaced\tmaps\tdefined_by\tmap_list\n")
        for r in sorted(rows, key=lambda r: (r[3], r[0])):
            f.write("\t".join(str(x) for x in r) + "\n")
        f.write("\n# Dwell-only (in the decompile, not in Copper, ours or AD): %d\n" % len(dwell_only))
        f.write(" ".join(sorted(dwell_only)) + "\n")
        f.write("# Copper-only (Copper spawn functions not in ours or AD; Dwell inherits them): %d\n" % len(copper_only))
        f.write(" ".join(sorted(copper_only)) + "\n")
        f.write("# Copper QUAKED classes: %d; Copper spawn-like funcs: %d; Dwell spawn-like funcs: %d\n" % (
            len(cop_quaked), len(cop), len(dw)))
        f.write("# used by Dwell maps but defined nowhere in the decompile: %s\n" % " ".join(
            sorted(n for n in use if n and n not in dw_all)))
        f.write("\n# keys per Dwell-only / Copper-only classname (key:count)\n")
        for n in sorted(dwell_only | copper_only):
            if use[n]:
                f.write("%s\t%s\n" % (n, " ".join("%s:%d" % kv for kv in keys_by_class[n].most_common(25))))

    # ---- functions Dwell added / dropped vs Copper
    with open(os.path.join(a.out, "funcs.txt"), "w") as f:
        added = sorted(n for n in dw_all if n not in cop_all)
        dropped = sorted(n for n in cop_all if n not in dw_all)
        byfile = collections.Counter(dw_all[n] for n in added)
        f.write("# Dwell functions not in Copper's source: %d (Copper: %d, Dwell: %d)\n" % (len(added), len(cop_all), len(dw_all)))
        for fn, c in byfile.most_common():
            f.write("%s\t%d\n" % (fn, c))
        f.write("\n# added, non-frame (names without a trailing digit)\n")
        f.write(" ".join(n for n in added if not re.search(r"\d+$", n)) + "\n")
        f.write("\n# Copper functions absent from Dwell: %d\n" % len(dropped))
        f.write(" ".join(dropped) + "\n")
        dfiles = sorted(os.path.basename(p) for p in decomp_qc)
        cfiles = set(os.path.basename(p) for p in copper_qc)
        f.write("\n# decompiled files not in Copper: %s\n" % " ".join(x for x in dfiles if x not in cfiles))

    # ---- per map table
    with open(os.path.join(a.out, "permap.txt"), "w") as f:
        for m, ents in per.items():
            ws = ents[0] if ents else {}
            cls = collections.Counter(e.get("classname", "").lower() for e in ents)
            f.write("== %s  ents=%d  worldspawn: %s\n" % (m[5:-4], len(ents), " ".join(
                '%s="%s"' % (k, v[:60]) for k, v in ws.items() if k not in ("classname", "wad", "_tb_def"))))
            f.write("   dwell-only: %s\n" % " ".join("%s:%d" % (c, cls[c]) for c in sorted(cls) if c in dwell_only))
            f.write("   copper-only: %s\n" % " ".join("%s:%d" % (c, cls[c]) for c in sorted(cls) if c in copper_only))
            f.write("   undefined: %s\n" % " ".join("%s:%d" % (c, cls[c]) for c in sorted(cls) if c and c not in dw_all))
            for e in ents:
                c = e.get("classname", "").lower()
                if re.search(r"music|cdtrack|track|sound|cd_", c) or any(k in e for k in ("music", "track", "cdtrack")):
                    if c.startswith("ambient_") or c in ("worldspawn",):
                        continue
                    f.write("   music? %s\n" % " ".join('%s="%s"' % (k, v[:40]) for k, v in e.items()))

    # ---- assets
    others = []
    for spec in a.official:
        label, paths = spec.split("=", 1)
        others.append(Source(label, paths.split(",")))
    qvr = Source("quakevr", [a.quakevr])
    others.append(qvr)
    with open(os.path.join(a.out, "assets.txt"), "w") as f:
        tops = collections.Counter(n.split("/")[0] if "/" in n else "(root)" for n in dwell.files)
        f.write("# Dwell files: %d, %d bytes; by folder: %s\n" % (len(dwell.files), sum(dwell.size(n) for n in dwell.files),
                                                           " ".join("%s:%d" % kv for kv in tops.most_common())))
        ver = collections.Counter()
        for n in dwell.files:
            if n.endswith(".mdl") or n.endswith(".spr"):
                h = dwell.read(n)[:8]
                ver[(h[:4].decode("latin-1"), struct.unpack("<i", h[4:8])[0] if len(h) == 8 else -1)] += 1
        f.write("# model formats: %s\n" % " ".join("%s v%d:%d" % (k[0], k[1], c) for k, c in ver.items()))
        for o in others:
            same, diff = [], []
            for n in sorted(dwell.files):
                if n in o.files and not n.endswith(".pak") and not n.startswith("maps/"):
                    if hashlib.md5(dwell.read(n)).digest() == hashlib.md5(o.read(n)).digest():
                        same.append(n)
                    else:
                        diff.append(n)
            f.write("\n## vs %s: %d same-name files, %d identical, %d different\n" % (o.label, len(same) + len(diff), len(same), len(diff)))
            f.write("identical: %s\n" % " ".join(same))
            f.write("different: %s\n" % " ".join(diff))
        f.write("\n## Dwell-unique progs/ (no other source has the name): ")
        alln = set()
        for o in others:
            alln.update(o.files)
        uniq = sorted(n for n in dwell.files if n.startswith("progs/") and n not in alln)
        f.write("%d\n%s\n" % (len(uniq), " ".join(uniq)))
        uniqs = sorted(n for n in dwell.files if n.startswith("sound/") and n not in alln)
        f.write("## Dwell-unique sound/: %d\n" % len(uniqs))
        f.write("\n## all Dwell files\n")
        for n in sorted(dwell.files):
            f.write("%s\t%d\t%s\n" % (n, dwell.size(n), dwell.files[n][0]))

    # ---- music
    with open(os.path.join(a.out, "music.txt"), "w") as f:
        mus = sorted(n for n in dwell.files if n.startswith("music/"))
        hashes = collections.defaultdict(list)
        for n in mus:
            hashes[hashlib.sha256(dwell.read(n)).hexdigest()[:16]].append(n)
        f.write("# %d files, %d bytes, %d distinct contents\n" % (len(mus), sum(dwell.size(n) for n in mus), len(hashes)))
        for h, ns in hashes.items():
            f.write("%s\t%s\t%d\n" % (h, " ".join(ns), dwell.size(ns[0])))
        f.write("\n# per map: worldspawn sounds/music keys, entities with sounds+cd/music\n")
        for m, ents in per.items():
            ws = ents[0] if ents else {}
            extra = []
            for e in ents[1:]:
                c = e.get("classname", "")
                if "music" in c or "cd" in c or "track" in c:
                    extra.append("%s(%s)" % (c, ",".join("%s=%s" % (k, v) for k, v in e.items() if k in ("sounds", "count", "track", "music", "cdtrack", "targetname", "spawnflags"))))
            f.write("%s\tsounds=%s\tmusic=%s\t%s\n" % (m[5:-4], ws.get("sounds", "-"), ws.get("music", "-"), " ".join(extra)))

    # ---- detection
    def score(ents):
        cls = set(e.get("classname", "").lower() for e in ents)
        return sorted(cls & dwell_only), sorted(cls & copper_only)

    with open(os.path.join(a.out, "detect.txt"), "w") as f:
        f.write("# map\tdwell_signals\tcopper_signals\n")
        groups = [("dwell", dwell)] + [(o.label, o) for o in others if o is not qvr]
        if a.addons:
            for d in sorted(glob.glob(os.path.join(a.addons, "*"))):
                groups.append(("addon:" + os.path.basename(d), Source("addon", [d])))
        summary = []
        for label, src in groups:
            n2 = nd = nc = 0
            ms = maps_of(src)
            for m in ms:
                try:
                    ents = bsp_entities(src.read(m))
                except Exception:
                    continue
                dsig, csig = score(ents)
                if dsig or csig:
                    f.write("%s:%s\t%d %s\t%d %s\n" % (label, m[5:-4], len(dsig), ",".join(dsig), len(csig), ",".join(csig)))
                nd += len(dsig) >= 1
                nc += len(csig) >= 2
            summary.append("%s: %d maps, %d with >=1 Dwell signal, %d with >=2 Copper signals" % (label, len(ms), nd, nc))
        f.write("\n# summary\n" + "\n".join(summary) + "\n")
        if a.index and os.path.isfile(a.index):
            ext = collections.Counter()
            with open(a.index, encoding="utf-8", errors="replace") as ix:
                for line in ix:
                    p = line.rstrip("\n").split("\t")
                    if len(p) > 10:
                        e = p[10].lower()
                        if "copper" in e or "dwell" in e or "copper" in p[13].lower() if len(p) > 13 else False:
                            ext[e or "(none)"] += 1
            f.write("\n# Quaddicted index: packages whose install folder or description mentions copper/dwell, by folder\n")
            for k, v in ext.most_common():
                f.write("%s\t%d\n" % (k, v))


if __name__ == "__main__":
    main()
