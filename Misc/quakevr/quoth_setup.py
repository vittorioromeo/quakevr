#!/usr/bin/env python3
"""quoth_setup.py <test base dir>: installs Quoth 2.2 and the 14 sample Quoth maps quoth_maps.sh plays into a test base
(docs/vr-port/QUOTH.md), the way the installer and the Map Library leave them. Never downloads: it only unpacks the
zips already in the kit's downloads/modsurvey/ (Quaddicted's copies), checked by their SHA-256.

- Quoth 2.2 (quoth2pt2full_2.zip): the zip's files, unmodified, in <base>/quoth/ (the installer's layout).
- Each sample map package: in <base>/qvr_addons/<sha16>/ laid out as a game dir (a file at the zip's root: a .bsp or
  .lit into maps/, a pak kept; maps/, gfx/, sound/, progs/ kept; readmes and map sources left out), one line per file
  in <base>/cache/maps_installed.txt (sha256, bytes, path), as the Map Library's install writes it.

Idempotent: what is installed already is left alone. Prints one `quoth_setup:` line per package."""

import hashlib
import os
import shutil
import sys
import zipfile

MAGIC = "#quakevr-maps-installed-2"
QUOTH_SHA = "9fce1af35ef7f635990be949d75f7faa18cc198bc19d9adc7d0575e234ab1139"
SAMPLES = ("apsp2", "dis_sp6", "e1m5quoth", "gth", "hrim_sp2", "ion02", "kellmet1", "lpqsp1", "metmon1d", "mfxsp10",
           "mhsp01", "ne_marb", "quarantine", "sksp2b")
GAME_DIRS = ("maps/", "gfx/", "sound/", "progs/", "music/")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvrtest  # noqa: E402 (the kit folder)
DOWNLOADS = os.path.join(qvrtest.KIT, "downloads", "modsurvey")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def registered(registry):
    if not os.path.isfile(registry):
        return set()
    with open(registry, encoding="utf-8") as f:
        lines = f.read().split("\n")
    if not lines or lines[0] != MAGIC:
        return set()
    return {l.split("\t", 1)[0] for l in lines[1:] if "\t" in l}


def register(registry, sha, folder):
    rows = []
    for root, _, files in os.walk(folder):
        for n in sorted(files):
            p = os.path.join(root, n)
            rows.append("%s\t%d\t%s" % (sha, os.path.getsize(p), os.path.relpath(p, folder).replace("\\", "/")))
    new = not os.path.isfile(registry) or os.path.getsize(registry) == 0
    with open(registry, "a", encoding="utf-8", newline="\n") as f:
        if new:
            f.write(MAGIC + "\n")
        f.write("\n".join(rows) + "\n")
    return len(rows)


def package_path(name):
    """Where a zip entry goes in the package's game dir, or None (readmes, map sources)."""
    low = name.lower()
    if low.startswith(GAME_DIRS):
        return name
    if "/" in name:
        return None
    if low.endswith((".bsp", ".lit")):
        return "maps/" + name
    if low.endswith(".pak"):
        return name
    return None


def unpack(zpath, dest, layout):
    n = 0
    with zipfile.ZipFile(zpath) as zf:
        for i in zf.infolist():
            name = i.filename.replace("\\", "/")
            if i.is_dir() or name.startswith("/") or ".." in name.split("/"):
                continue
            rel = layout(name)
            if not rel:
                continue
            out = os.path.join(dest, rel)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            with zf.open(i) as s, open(out, "wb") as d:
                shutil.copyfileobj(s, d)
            n += 1
    return n


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    base = sys.argv[1]
    quoth = os.path.join(base, "quoth")
    if os.path.isfile(os.path.join(quoth, "pak2.pak")):
        print("quoth_setup: quoth installed already")
    else:
        z = os.path.join(DOWNLOADS, "quoth", "quoth2pt2full_2.zip")
        if not os.path.isfile(z) or sha256(z) != QUOTH_SHA:
            sys.exit("quoth_setup: no Quoth 2.2 zip (SHA-256 %s...) at %s" % (QUOTH_SHA[:16], z))
        print("quoth_setup: quoth installed, %d files" % unpack(z, quoth, lambda n: n))
    registry = os.path.join(base, "cache", "maps_installed.txt")
    os.makedirs(os.path.dirname(registry), exist_ok=True)
    have = registered(registry)
    for label in SAMPLES:
        z = os.path.join(DOWNLOADS, "quothmaps", label + ".zip")
        if not os.path.isfile(z):
            print("quoth_setup: %s NOT installed: no zip at %s" % (label, z))
            continue
        sha = sha256(z)
        if sha in have:
            print("quoth_setup: %s installed already" % label)
            continue
        dest = os.path.join(base, "qvr_addons", sha[:16])
        unpack(z, dest, package_path)
        print("quoth_setup: %s installed, %d files" % (label, register(registry, sha, dest)))


if __name__ == "__main__":
    main()
