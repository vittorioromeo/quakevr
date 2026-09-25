#!/usr/bin/env python3
# relight_maps.py -- re-lights your own copy of Quake's maps with ericw-tools' `light`, for Quake VR.
#
# Quake's 1996 lightmaps have no ambient occlusion, no bounced light and no colour, which makes
# the world look flat in VR. `light` can re-light a compiled .bsp from the light entities inside
# it: this script does that for every map of the installed campaigns (id1, hipnotic, rogue, or the
# folders you name), with smooth shadows (-extra4), ambient occlusion (-dirt), a gentle bounce and
# coloured light (-lit). Geometry and entities stay as they are: only the light changes.
#
# The results go into quakevr/relit/<game>/maps/<map>.bsp and .lit. Quake VR loads them in place of
# <game>'s own maps (vr_relit_maps 1, the default; 0 plays the original lighting). The maps are
# id Software's: the relit copies are made on your machine from the ones you own and are not
# distributed.
#
# Usage:
#   python Misc/quakevr/relight_maps.py --quake "C:/Program Files (x86)/Steam/steamapps/common/Quake"
#       --light C:/tools/ericw-tools/bin/light.exe [--games id1 hipnotic rogue] [--out quakevr/relit]
#       [--light-args "..."] [--force] [--only e1m1 ...]
#
# ericw-tools: https://github.com/ericwa/ericw-tools/releases (v0.18.1 was used; GPL). `light` may
# also be given by the ERICW_LIGHT environment variable or found on PATH.
#
# Already relit maps are skipped unless their source or the options changed (--force relights all).

import argparse
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile

# Smooth shadow edges, ambient occlusion in corners, a little bounced light (it brightens maps,
# which the 2021 re-release was criticised for: kept low), coloured .lit output.
DEFAULT_LIGHT_ARGS = "-extra4 -dirt -dirtscale 1.0 -dirtdepth 96 -bounce -bouncescale 0.5 -lit"

# Brush models used as items (health and ammo boxes, the exploding box) are not levels.
SKIP_PREFIXES = ("b_",)


def pak_maps(pak_path):
    """{name: bytes} of maps/*.bsp in a .pak."""
    maps = {}
    with open(pak_path, "rb") as f:
        data = f.read()
    ident, offset, length = struct.unpack_from("<4sii", data, 0)
    if ident != b"PACK":
        return maps
    for i in range(length // 64):
        raw, pos, size = struct.unpack_from("<56sii", data, offset + i * 64)
        name = raw.split(b"\0")[0].decode("latin-1").lower()
        if name.startswith("maps/") and name.endswith(".bsp"):
            maps[name] = data[pos : pos + size]
    return maps


def game_maps(game_dir):
    """Maps of one game folder as the engine sees them: paks in order (later ones win), then loose
    files (which win over paks)."""
    maps = {}
    paks = []
    for entry in os.listdir(game_dir):
        base, ext = os.path.splitext(entry.lower())
        if ext == ".pak" and base.startswith("pak") and base[3:].isdigit():
            paks.append((int(base[3:]), entry))
    for _, entry in sorted(paks):
        maps.update(pak_maps(os.path.join(game_dir, entry)))
    loose = os.path.join(game_dir, "maps")
    if os.path.isdir(loose):
        for entry in os.listdir(loose):
            if entry.lower().endswith(".bsp"):
                with open(os.path.join(loose, entry), "rb") as f:
                    maps["maps/" + entry.lower()] = f.read()
    return maps


def is_level(name, data):
    base = os.path.basename(name)
    if base.startswith(SKIP_PREFIXES):
        return False
    (version,) = struct.unpack_from("<i", data, 0)
    return version == 29 or data[:4] in (b"BSP2", b"2PSB")


def find_light(explicit):
    for candidate in (explicit, os.environ.get("ERICW_LIGHT"), shutil.which("light"), shutil.which("light.exe")):
        if candidate and os.path.isfile(candidate):
            return candidate
    sys.exit("ericw-tools' light not found: pass --light <path to light(.exe)> or set ERICW_LIGHT")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--quake", required=True, help="Quake folder (the one containing id1)")
    parser.add_argument("--light", help="ericw-tools light executable")
    parser.add_argument("--games", nargs="+", default=["id1", "hipnotic", "rogue"])
    parser.add_argument("--out", default=os.path.normpath(os.path.join(here, "..", "..", "quakevr", "relit")))
    parser.add_argument("--light-args", default=DEFAULT_LIGHT_ARGS)
    parser.add_argument("--only", nargs="*", help="map names (e1m1 ...) to relight, for trying options")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    light = find_light(args.light)
    light_args = args.light_args.split()
    total = done = failed = 0

    for game in args.games:
        game_dir = os.path.join(args.quake, game)
        if not os.path.isdir(game_dir):
            print("%s: not installed, skipped" % game)
            continue
        out_dir = os.path.join(args.out, game, "maps")
        os.makedirs(out_dir, exist_ok=True)

        for name, data in sorted(game_maps(game_dir).items()):
            base = os.path.splitext(os.path.basename(name))[0]
            if args.only and base not in args.only:
                continue
            if not is_level(name, data):
                continue
            total += 1
            stamp_value = hashlib.sha1(data + " ".join(light_args).encode()).hexdigest()
            out_bsp = os.path.join(out_dir, base + ".bsp")
            out_lit = os.path.join(out_dir, base + ".lit")
            stamp = os.path.join(out_dir, base + ".relit")
            if not args.force and os.path.isfile(out_bsp) and os.path.isfile(stamp):
                with open(stamp) as f:
                    if f.read().strip() == stamp_value:
                        continue

            with tempfile.TemporaryDirectory() as tmp:
                work = os.path.join(tmp, base + ".bsp")
                with open(work, "wb") as f:
                    f.write(data)
                print("%s/%s ..." % (game, base), end=" ", flush=True)
                result = subprocess.run([light] + light_args + [work], cwd=tmp, stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, text=True, errors="replace")
                lit = os.path.join(tmp, base + ".lit")
                if result.returncode != 0 or not os.path.isfile(lit):
                    failed += 1
                    print("failed")
                    print(result.stdout[-2000:])
                    continue
                shutil.copyfile(work, out_bsp)
                shutil.copyfile(lit, out_lit)
                with open(stamp, "w") as f:
                    f.write(stamp_value + "\n")
                done += 1
                print("ok")

    print("%d maps: %d relit, %d up to date, %d failed" % (total, done, total - done - failed, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
