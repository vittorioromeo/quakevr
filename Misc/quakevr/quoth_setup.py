#!/usr/bin/env python3
"""quoth_setup.py <test base dir>: installs Quoth 2.2 and the 14 sample Quoth maps quoth_maps.sh plays into a test base
(docs/vr-port/QUOTH.md), the way the installer and the Map Library leave them. Never downloads: it only unpacks the
zips already in the kit's downloads/modsurvey/ (Quaddicted's copies), checked by their SHA-256.

- Quoth 2.2 (quoth2pt2full_2.zip): the zip's files, unmodified, in <base>/quoth/ (the installer's layout).
- Each sample map package: in <base>/qvr_addons/<sha16>/ laid out as a game dir (a file at the zip's root: a .bsp or
  .lit into maps/, a root pakN.pak unpacked, in order, as the Map Library does (metmon1d's map is in its pak0.pak);
  maps/, gfx/, sound/, progs/ kept; readmes, map sources, game code and configs left out), one line per file in
  <base>/cache/maps_installed.txt (sha256, bytes, path), as the Map Library's install writes it.

Idempotent: what is installed already is left alone. Prints one `quoth_setup:` line per package."""

import hashlib
import os
import shutil
import struct
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


def root_pak(name):
    """pakN.pak at the zip's root: N (the order the engine reads them in); else -1."""
    low = name.lower()
    if "/" in name or not (low.startswith("pak") and low.endswith(".pak")) or not low[3:-4].isdigit():
        return -1
    return int(low[3:-4])


def unregister(registry, sha):
    """The package's lines dropped from the registry (it is written again)."""
    with open(registry, encoding="utf-8") as f:
        lines = f.read().split("\n")
    with open(registry, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(l for l in lines if not l.startswith(sha + "\t")))


def package_path(name):
    """Where a zip (or a root pak's) entry goes in the package's game dir, or None (readmes, map sources, game code,
    startup configs: as vr_mapinstall.cpp's gameCode and startupConfig). A root pakN.pak is unpacked (unpack_paks)."""
    low = name.lower()
    leaf = low.rsplit("/", 1)[-1]
    if leaf in ("progs.dat", "qwprogs.dat", "csprogs.dat", "progs.lno", "quake.rc") or leaf.endswith(".cfg"):
        return None
    if low.startswith(GAME_DIRS):
        return name
    if "/" in name:
        return None
    if low.endswith((".bsp", ".lit")):
        return "maps/" + name
    return None


def unpack_paks(zpath, dest):
    """The zip's root pakN.pak files, in order, unpacked into the package's game dir (as the Map Library's install does:
    vr_mapinstall.cpp, "The packs at the package's root"); a later pak's file over an earlier one's. Returns the files."""
    out = []
    with zipfile.ZipFile(zpath) as zf:
        paks = sorted((root_pak(i.filename), i) for i in zf.infolist() if root_pak(i.filename) >= 0)
        for _, i in paks:
            d = zf.read(i)
            if d[:4] != b"PACK":
                continue
            ofs, ln = struct.unpack("<ii", d[4:12])
            for e in range(ln // 64):
                raw = d[ofs + e * 64: ofs + e * 64 + 56].split(b"\0")[0].decode("latin-1").replace("\\", "/")
                pos, size = struct.unpack("<ii", d[ofs + e * 64 + 56: ofs + e * 64 + 64])
                rel = package_path(raw)
                if not rel or raw.startswith("/") or ".." in raw.split("/"):
                    continue
                p = os.path.join(dest, rel)
                os.makedirs(os.path.dirname(p), exist_ok=True)
                with open(p, "wb") as f:
                    f.write(d[pos:pos + size])
                out.append(p)
    return out


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
        dest = os.path.join(base, "qvr_addons", sha[:16])
        kept = [n for n in (os.listdir(dest) if os.path.isdir(dest) else []) if root_pak(n) >= 0]
        if sha in have and not kept:
            print("quoth_setup: %s installed already" % label)
            continue
        if kept:
            # An earlier quoth_setup.py kept the root pak whole (a package folder does not mount it): moved aside (not
            # deleted), its files unpacked, the package's registry lines written again.
            aside = os.path.join(base, "cache", "quoth_setup_old")
            os.makedirs(aside, exist_ok=True)
            for n in kept:
                os.replace(os.path.join(dest, n), os.path.join(aside, sha[:16] + "_" + n))
            unregister(registry, sha)
        unpack(z, dest, package_path)
        paks = unpack_paks(z, dest)
        print("quoth_setup: %s installed, %d files (%d from its paks)" % (label, register(registry, sha, dest), len(paks)))


if __name__ == "__main__":
    main()
