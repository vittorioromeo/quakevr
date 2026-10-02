"""Picks and copies the Quetoo material maps Quake VR ships (quakevr/textures_quetoo/; its README.md).

Usage: select_quetoo_maps.py <quetoo-data>/target/default/textures/quake <dest> <stats.txt>... [--match 0.5]

Each stats file is a console log of `vr_extmaps_stats all` after every map (vr_extmaps 1, vr_extmaps_dir at the full
Quetoo folder, vr_extmaps_match 0.001 so every texture's match is measured), e.g. one run with id's textures and one
with the QRP pack (the kit's run.sh -Base qbase / qrp). A texture name is kept when its best match in any log reaches
--match; its files are resolved as vr_extmaps.cpp does (packName, the .mat's diffusemap/normalmap/specularmap, an
animation's frame-less picture, _norm, _spec, the first of _luma/_glow) and copied unchanged.
"""

import os
import re
import shutil
import sys


def pack_name(tex):
    t = tex.lower()[:16]
    s = t[1:] if t[:1] in "*#" else t
    if s[:1] == "+" and len(s) > 2:
        return s[2:] + "+" + s[1]
    return s


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    need = 0.5
    if "--match" in sys.argv:
        need = float(sys.argv[sys.argv.index("--match") + 1])
        args.remove(sys.argv[sys.argv.index("--match") + 1])
    src, dest, logs = args[0], args[1], args[2:]
    files = {f.lower(): f for f in os.listdir(src)}

    best = {}
    for log in logs:
        for line in open(log, encoding="latin-1"):
            m = re.match(r"^(\S+)\s+(used|differs|none)\s+match\s+(-?[\d.]+)", line)
            if m and m.group(2) != "none":
                best[m.group(1)] = max(best.get(m.group(1), -9.0), float(m.group(3)))

    def image(base):
        for ext in (".png", ".tga", ".jpg"):  # Image_LoadImage's order
            if base + ext in files:
                return files[base + ext]
        return None

    def material(base):
        refs = {}
        name = files.get(base + ".mat")
        if not name:
            return refs, None
        depth = 0
        for line in open(os.path.join(src, name)):
            depth += line.count("{") - line.count("}")
            parts = line.split()
            if depth == 1 and len(parts) >= 2 and parts[0] in ("diffusemap", "normalmap", "specularmap"):
                refs[parts[0]] = os.path.basename(parts[1])
        return refs, name

    def needed(tex):
        base = pack_name(tex)
        refs, mat = material(base)
        diffuse = image(refs.get("diffusemap", base))
        if not diffuse and "+" in base and "diffusemap" not in refs:
            base = base.split("+")[0]
            refs, mat = material(base)
            diffuse = image(refs.get("diffusemap", base))
        if not diffuse:
            return set()
        out = {diffuse} | ({mat} if mat else set())
        for key, suffix in (("normalmap", "_norm"), ("specularmap", "_spec")):
            f = image(refs[key]) if key in refs else image(base + suffix)
            if f:
                out.add(f)
        for suffix in ("_luma", "_glow"):
            f = image(base + suffix)
            if f:
                out.add(f)
                break
        return out

    keep = sorted(t for t, v in best.items() if v >= need)
    chosen = sorted(set().union(*[needed(t) for t in keep])) if keep else []
    os.makedirs(dest, exist_ok=True)
    for f in chosen:
        shutil.copy2(os.path.join(src, f), os.path.join(dest, f))
    size = sum(os.path.getsize(os.path.join(dest, f)) for f in chosen)
    print(f"{len(keep)} texture names, {len(chosen)} files, {size / 1e6:.1f} MB to {dest}")


if __name__ == "__main__":
    main()
