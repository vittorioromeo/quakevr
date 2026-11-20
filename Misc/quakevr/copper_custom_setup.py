#!/usr/bin/env python3
"""copper_custom_setup.py <test base dir>: installs the two Copper custom-map packages dwell_maps.sh plays into a test
base the way the Map Library leaves them (DWELL.md 9.2): each package's files in <base>/qvr_addons/<sha16>/, laid out
as a game dir, and one line per file in <base>/cache/maps_installed.txt (sha256, bytes, path). The Map Library's
install always downloads; this does not: it only copies what is already on this machine.

- Ancient (v1.1, made for Copper): copied (never moved) from an installed copy, the first of QVR_ANCIENT_FROM (a
  qvr_addons/<sha16> folder) or the Steam Quake folder's qvr_addons/5255c088915839ad.
- Underdark Overbright v1.1 (Lunaran and Scampie, for Copper): unpacked from its Quaddicted zip in the kit's
  downloads/underdark/ (or QVR_UDOB_ZIP), checked by its SHA-256.

Idempotent: a package already registered is left alone. Prints one `copper_custom:` line per package."""

import hashlib
import os
import shutil
import sys
import zipfile

MAGIC = "#quakevr-maps-installed-2"
ANCIENT = "5255c088915839ad65ce5da896462dd40760a72c107fa453c9a9ca725fd61205"
UDOB = "cf7cead3291fac81196b9b9fa772afa8a460f4f4f9a8677bc3c8407d432e2ca6"
STEAM = "C:/Program Files (x86)/Steam/steamapps/common/Quake"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvrtest  # noqa: E402 (the kit folder)
KIT = qvrtest.KIT


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


def ancient(dest):
    src = os.environ.get("QVR_ANCIENT_FROM") or os.path.join(STEAM, "qvr_addons", ANCIENT[:16])
    if not os.path.isfile(os.path.join(src, "maps", "ancient.bsp")):
        return "no installed copy at %s" % src
    shutil.copytree(src, dest, dirs_exist_ok=True)
    return None


def udob(dest):
    z = os.environ.get("QVR_UDOB_ZIP") or os.path.join(KIT, "downloads", "underdark", "udob_v1_1.zip")
    if not os.path.isfile(z):
        return "no zip at %s" % z
    h = hashlib.sha256()
    with open(z, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    if h.hexdigest() != UDOB:
        return "%s is not v1.1 (SHA-256 %s)" % (z, h.hexdigest())
    with zipfile.ZipFile(z) as zf:
        for i in zf.infolist():
            name = i.filename.replace("\\", "/")
            if i.is_dir() or name.startswith("/") or ".." in name.split("/") or "__macosx/" in name.lower():
                continue
            out = os.path.join(dest, name)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            with zf.open(i) as s, open(out, "wb") as d:
                shutil.copyfileobj(s, d)
    return None


def main():
    base = sys.argv[1]
    registry = os.path.join(base, "cache", "maps_installed.txt")
    os.makedirs(os.path.dirname(registry), exist_ok=True)
    have = registered(registry)
    for label, sha, fill in (("ancient", ANCIENT, ancient), ("udob", UDOB, udob)):
        if sha in have:
            print("copper_custom: %s installed already" % label)
            continue
        dest = os.path.join(base, "qvr_addons", sha[:16])
        why = fill(dest)
        if why:
            print("copper_custom: %s NOT installed: %s" % (label, why))
            continue
        print("copper_custom: %s installed, %d files" % (label, register(registry, sha, dest)))


if __name__ == "__main__":
    main()
