#!/usr/bin/env python3
"""make_release.py -- a Quake VR: Unleashed release, ready to publish, from a built package.

From the package Windows/package-quakevr.ps1 made (dist/QuakeVR with its manifest.json, or dist/QuakeVR.zip), writes
into --out (default dist/release/<tag>):

  QuakeVR.zip       the package (zipped here from the folder, files at the zip's root, as package-quakevr.ps1 does)
  <textures>.zip    the HD texture pack, only when --textures is given (an override). By default the feed's
                    "hdtextures" component is the pack already hosted on the support-files release
                    (Misc/release/support_assets.json: its URL, size and SHA-256; nothing copied or uploaded);
                    --no-textures leaves the component out
  QuakeVR-Setup.exe the installer, when --setup is given (a release asset, not in the feed)
  other assets      --asset <file> (ericw-tools' GPL source is hosted on the support-files release; --asset
                    ericw-tools-2.0.0-alpha11-src.zip attaches a copy to this release too)
  latest.json       the release feed in the exact format the installer reads (Installer/src/QuakeVR.Installer.Core/
                    Packaging/ReleaseFeed.cs: schema 1, version, package, components; each file's name, size, SHA-256
                    and download addresses, GitHub first)
  PUBLISH.txt       the commands and uploads that publish it (nothing is published by this script)

  python Misc/quakevr/make_release.py --package dist/QuakeVR [--textures <zip> | --no-textures]
         [--setup Installer/.../publish/QuakeVR-Setup.exe] [--asset <file>]... [--tag <tag>] [--notes "text"]
         [--url-base <template>]... [--out <dir>]

The download addresses are --url-base templates ({tag} and {file} replaced), in order; by default only GitHub's
https://github.com/vittorioromeo/quakevr/releases/download/{tag}/{file} (the release's own assets: a latest.json keeps
pointing at its own release's files). Add --url-base https://vittorioromeo.com/quakevr/releases/{tag}/{file} only once
the files are uploaded there too (for the hosted HD textures: .../releases/<support tag>/<file>). Bases on 127.0.0.1
or localhost (make_release.ps1 -Local) are not used for the hosted textures: a local server does not have them. The
installer reads latest.json from
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
SUPPORT_ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "release", "support_assets.json")
LOOPBACK = re.compile(r"^https?://(127\.0\.0\.1|localhost|\[::1\])([:/]|$)", re.I)


def read_support_assets(path):
    """Misc/release/support_assets.json: the support-files release (tag, URL template, each file's size and SHA-256)."""
    with open(path, encoding="utf-8") as f:
        sa = json.load(f)
    for key, e in sa["files"].items():
        if not re.fullmatch(r"[0-9a-f]{64}", e.get("sha256", "")) or int(e.get("size", 0)) <= 0 or not e.get("file"):
            sys.exit(f"{path}: bad entry {key}")
    return sa


def support_url(sa, key):
    return sa["url"].replace("{tag}", sa["tag"]).replace("{file}", sa["files"][key]["file"])


def hosted_feed_file(sa, key, bases):
    """A feed entry for a file hosted on the support-files release: its own URL first, then each non-local --url-base
    with the support tag (a mirror of that release), never this release's assets."""
    e = sa["files"][key]
    urls = [support_url(sa, key)]
    for b in bases:
        u = b.replace("{tag}", sa["tag"]).replace("{file}", e["file"])
        if b != GITHUB and not LOOPBACK.match(b) and u not in urls:
            urls.append(u)
    return {"file": e["file"], "size": int(e["size"]), "sha256": e["sha256"], "urls": urls}


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
    ap.add_argument("--textures", help="an HD texture pack zip to ship with this release (overrides the hosted one)")
    ap.add_argument("--no-textures", action="store_true", help="no hdtextures component in the feed")
    ap.add_argument("--support-assets", default=SUPPORT_ASSETS, help="the support-files list (default Misc/release/support_assets.json)")
    ap.add_argument("--setup", help="QuakeVR-Setup.exe (dotnet publish's single file): a release asset")
    ap.add_argument("--asset", action="append", default=[], help="another release asset (repeatable)")
    ap.add_argument("--tag", help="the release's git tag (default: v + the package's version)")
    ap.add_argument("--notes", help="a line for latest.json's notes")
    ap.add_argument("--url-base", action="append", help="download address template with {tag} and {file} (repeatable, in order)")
    ap.add_argument("--out", help="output folder (default dist/release/<tag>)")
    a = ap.parse_args()
    if a.textures and a.no_textures:
        sys.exit("--textures and --no-textures together: pick one")
    sa = read_support_assets(a.support_assets)

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
    elif not a.no_textures:
        feed["components"]["hdtextures"] = hosted_feed_file(sa, "hdtextures", bases)
        print(f"hdtextures: the hosted {sa['files']['hdtextures']['file']} ({sa['tag']}, not copied): "
              + ", ".join(feed["components"]["hdtextures"]["urls"]))
        if any(LOOPBACK.match(b) for b in bases):
            print("note: a local release's hdtextures still points at GitHub: ticking HD textures downloads the real "
                  f"{int(sa['files']['hdtextures']['size']) / 1e6:.0f} MB (--textures <zip> serves a copy locally)")
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

   The notes link ericw-tools' GPL source (the package ships light.exe): Source of ericw-tools' light.exe:
   {support_url(sa, "ericw_source")}

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
