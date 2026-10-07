#!/usr/bin/env python3
"""make_release.py -- a Quake VR: Unleashed release, ready to publish, from a built package.

From the package Windows/package-quakevr.ps1 made (dist/QuakeVR with its manifest.json, or dist/QuakeVR.zip), writes
into --out (default dist/release/<tag>):

  QuakeVR.zip       the package (zipped here from the folder, files at the zip's root, as package-quakevr.ps1 does)
  <textures>.zip    the HD texture pack, when --textures is given (the feed's "hdtextures" component)
  QuakeVR-Setup.exe the installer, when --setup is given (a release asset, not in the feed)
  other assets      --asset <file>, e.g. ericw-tools-2.0.0-alpha11-src.zip (its GPL source must sit beside the package)
  latest.json       the release feed in the exact format the installer reads (Installer/src/QuakeVR.Installer.Core/
                    Packaging/ReleaseFeed.cs: schema 1, version, package, components; each file's name, size, SHA-256
                    and download addresses, GitHub first)
  PUBLISH.txt       the commands and uploads that publish it (nothing is published by this script)

  python Misc/quakevr/make_release.py --package dist/QuakeVR [--textures quakevr-hq-textures-png-2026-10-03.zip]
         [--setup Installer/.../publish/QuakeVR-Setup.exe] [--asset <file>]... [--tag <tag>] [--notes "text"]
         [--url-base <template>]... [--out <dir>]

The download addresses are --url-base templates ({tag} and {file} replaced), in order; by default only GitHub's
https://github.com/vittorioromeo/quakevr/releases/download/{tag}/{file} (the release's own assets: a latest.json keeps
pointing at its own release's files). Add --url-base https://vittorioromeo.com/quakevr/releases/{tag}/{file} only once
the files are uploaded there too. The installer reads latest.json from
https://github.com/vittorioromeo/quakevr/releases/latest/download/latest.json (this release's latest.json asset, once it
is the newest non-prerelease release), then https://vittorioromeo.com/quakevr/latest.json (docs/vr-port/INSTALLER.md,
"Publishing a release").
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import sys
import zipfile

GITHUB = "https://github.com/vittorioromeo/quakevr/releases/download/{tag}/{file}"
REPO = "vittorioromeo/quakevr"
SITE_FEED = "https://vittorioromeo.com/quakevr/latest.json"


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def feed_file(path, tag, bases):
    name = os.path.basename(path)
    return {
        "file": name,
        "size": os.path.getsize(path),
        "sha256": sha256_of(path),
        "urls": [b.replace("{tag}", tag).replace("{file}", name) for b in bases],
    }


def read_manifest(package):
    """The package's manifest.json (from the folder or the zip, at its root or under one top folder) and the zip to ship."""
    if os.path.isdir(package):
        path = os.path.join(package, "manifest.json")
        if not os.path.isfile(path):
            sys.exit(f"{package}: no manifest.json (package-quakevr.ps1 writes it)")
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    with zipfile.ZipFile(package) as z:
        names = [n for n in z.namelist() if n.replace("\\", "/").split("/")[-1] == "manifest.json"]
        names.sort(key=lambda n: n.count("/"))
        if not names or names[0].count("/") > 1:
            sys.exit(f"{package}: no manifest.json at its root (package-quakevr.ps1 writes it)")
        return json.loads(z.read(names[0]).decode("utf-8"))


def check_package(package, manifest):
    """Every manifest file present with its size and SHA-256 (as the installer will check), when given a folder."""
    if not os.path.isdir(package):
        return
    for f in manifest.get("files", []):
        p = os.path.join(package, *f["path"].split("/"))
        if not os.path.isfile(p) or os.path.getsize(p) != f["size"] or sha256_of(p) != f["sha256"].lower():
            sys.exit(f"{package}: {f['path']} does not match its manifest: run package-quakevr.ps1 again")


def zip_folder(folder, out_zip):
    """The folder's files at the zip's root (as Compress-Archive -Path <dist>\\* does), sorted, deflated."""
    with zipfile.ZipFile(out_zip, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for root, dirs, files in os.walk(folder):
            dirs.sort()
            for name in sorted(files):
                p = os.path.join(root, name)
                z.write(p, os.path.relpath(p, folder).replace(os.sep, "/"))


def default_tag(version):
    # "2026-10-06 c131f4bf" -> "v2026-10-06-c131f4bf" (a git tag: no spaces)
    return "v" + re.sub(r"[^0-9A-Za-z._-]+", "-", version).strip("-")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--package", required=True, help="dist/QuakeVR (folder with manifest.json) or dist/QuakeVR.zip")
    ap.add_argument("--textures", help="the HD texture pack's zip (the feed's hdtextures component)")
    ap.add_argument("--setup", help="QuakeVR-Setup.exe (dotnet publish's single file): a release asset")
    ap.add_argument("--asset", action="append", default=[], help="another release asset (repeatable)")
    ap.add_argument("--tag", help="the release's git tag (default: v + the package's version)")
    ap.add_argument("--notes", help="a line for latest.json's notes")
    ap.add_argument("--url-base", action="append", help="download address template with {tag} and {file} (repeatable, in order)")
    ap.add_argument("--out", help="output folder (default dist/release/<tag>)")
    a = ap.parse_args()

    manifest = read_manifest(a.package)
    version = manifest.get("version") or sys.exit("the manifest has no version")
    check_package(a.package, manifest)
    tag = a.tag or default_tag(version)
    bases = a.url_base or [GITHUB]
    out = a.out or os.path.join("dist", "release", tag)
    os.makedirs(out, exist_ok=True)

    package_zip = os.path.join(out, "QuakeVR.zip")
    if os.path.isdir(a.package):
        zip_folder(a.package, package_zip)
    else:
        shutil.copyfile(a.package, package_zip)
    assets = [package_zip]
    feed = {"schema": 1, "version": version, "package": feed_file(package_zip, tag, bases), "components": {}}
    if a.textures:
        tex = os.path.join(out, os.path.basename(a.textures))
        shutil.copyfile(a.textures, tex)
        feed["components"]["hdtextures"] = feed_file(tex, tag, bases)
        assets.append(tex)
    for extra in ([a.setup] if a.setup else []) + a.asset:
        dest = os.path.join(out, os.path.basename(extra))
        shutil.copyfile(extra, dest)
        assets.append(dest)
    if a.notes:
        feed["notes"] = a.notes
    latest = os.path.join(out, "latest.json")
    with open(latest, "w", encoding="utf-8", newline="\n") as f:
        json.dump(feed, f, indent=2)
        f.write("\n")
    assets.append(latest)

    names = " ".join(f'"{os.path.basename(p)}"' for p in assets)
    publish = f"""Quake VR: Unleashed {version}: release {tag} (made by Misc/quakevr/make_release.py; nothing is published yet)

1. GitHub: tag the commit the package was built from and create the release with every file of this folder as an
   asset (latest.json included: https://github.com/{REPO}/releases/latest/download/latest.json then serves it). Not a
   draft and not a prerelease: "latest" skips both. From this folder:

   gh release create {tag} --repo {REPO} --target <commit> --title "Quake VR: Unleashed {version}" --notes-file <notes.md> {names}

2. vittorioromeo.com: upload latest.json to {SITE_FEED} (the installer's second feed; the folder
   /quakevr/ must exist). Optional: also upload the other files to the address of a second --url-base and run this script
   again with it, so the downloads have a mirror.

3. Check: qvr-setup feed --url https://github.com/{REPO}/releases/latest/download/latest.json
          qvr-setup feed --url {SITE_FEED}
   (each prints the version and the package's size), then the installer's Install with no local package.
"""
    with open(os.path.join(out, "PUBLISH.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write(publish)
    print(f"{out}: {len(assets)} files, latest.json version {version}, tag {tag}")
    print(publish)


if __name__ == "__main__":
    main()
