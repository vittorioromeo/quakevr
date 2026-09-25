#!/usr/bin/env python3
# relight_maps.py -- re-lights your own copy of Quake's maps with ericw-tools' `light`, for Quake VR.
#
# Quake's 1996 lightmaps have no ambient occlusion, no bounced light and no colour, which makes
# the world look flat in VR. `light` can re-light a compiled .bsp from the light entities inside
# it: this script does that for every map of the installed campaigns (id1, hipnotic, rogue, or the
# folders you name), with smooth shadows (-extra4), ambient occlusion (-dirt), a gentle bounce and
# coloured light (-lit). Geometry and entities stay as they are: only the light changes.
#
# Glowing textures light their surroundings (--glow, on by default): textures with fullbright pixels
# (buttons, computer panels, light fixtures, glowing runes) get ericw's surface lights ("_surface"
# light entities, in the colour of their glowing pixels, as bright as the share of them that glows;
# fixtures (named *light*) half as bright, since mappers put lights by them). These entities are only
# given to `light`: the relit map keeps its own.
#
# The results go into quakevr/relit/<game>/maps/<map>.bsp and .lit. Quake VR loads them in place of
# <game>'s own maps (vr_relit_maps 1, the default; 0 plays the original lighting). The maps are
# id Software's: the relit copies are made on your machine from the ones you own and are not
# distributed.
#
# Usage:
#   python Misc/quakevr/relight_maps.py --quake "C:/Program Files (x86)/Steam/steamapps/common/Quake"
#       --light C:/tools/ericw-tools/bin/light.exe [--games id1 hipnotic rogue] [--out quakevr/relit]
#       [--light-args "..."] [--force] [--only e1m1 ...] [--no-glow] [--glow-scale 1.0]
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


def pak_file(game_dir, wanted):
    """A file from a game folder's paks (later paks win), or None."""
    found = None
    paks = []
    for entry in os.listdir(game_dir):
        base, ext = os.path.splitext(entry.lower())
        if ext == ".pak" and base.startswith("pak") and base[3:].isdigit():
            paks.append((int(base[3:]), entry))
    for _, entry in sorted(paks):
        with open(os.path.join(game_dir, entry), "rb") as f:
            data = f.read()
        ident, offset, length = struct.unpack_from("<4sii", data, 0)
        if ident != b"PACK":
            continue
        for i in range(length // 64):
            raw, pos, size = struct.unpack_from("<56sii", data, offset + i * 64)
            if raw.split(b"\0")[0].decode("latin-1").lower() == wanted:
                found = data[pos : pos + size]
    return found


def lump(data, index):
    return struct.unpack_from("<ii", data, 4 + index * 8)


def with_entities(data, text):
    """The .bsp with its entity lump replaced by `text` (appended at the end; the old one is left
    unused where it was)."""
    blob = text.encode("latin-1") + b"\0"
    out = bytearray(data)
    while len(out) % 4:
        out.append(0)
    offset = len(out)
    out += blob
    struct.pack_into("<ii", out, 4, offset, len(blob))
    return bytes(out)


def entities_text(data):
    offset, length = lump(data, 0)
    return data[offset : offset + length].split(b"\0")[0].decode("latin-1")


def texture_areas(data):
    """{miptex index: how many lights `light` spawns on the faces using it (one about every 128 x 128
    units, at least one a face)} (BSP29; empty for BSP2)."""
    if struct.unpack_from("<i", data, 0)[0] != 29:
        return {}
    vofs, vlen = lump(data, 3)
    tofs, tlen = lump(data, 6)
    fofs, flen = lump(data, 7)
    eofs, elen = lump(data, 12)
    sofs, slen = lump(data, 13)
    verts = [struct.unpack_from("<3f", data, vofs + i * 12) for i in range(vlen // 12)]
    edges = [struct.unpack_from("<2H", data, eofs + i * 4) for i in range(elen // 4)]
    surfedges = struct.unpack_from("<%di" % (slen // 4), data, sofs)
    miptex_of = [struct.unpack_from("<i", data, tofs + i * 40 + 32)[0] for i in range(tlen // 40)]
    areas = {}
    for i in range(flen // 20):
        _, _, first, count, texinfo = struct.unpack_from("<hhihh", data, fofs + i * 20)
        pts = []
        for k in range(count):
            e = surfedges[first + k]
            pts.append(verts[edges[e][0]] if e >= 0 else verts[edges[-e][1]])
        area = 0.0
        for k in range(1, len(pts) - 1):
            a = [pts[k][j] - pts[0][j] for j in range(3)]
            b = [pts[k + 1][j] - pts[0][j] for j in range(3)]
            c = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
            area += 0.5 * (c[0] ** 2 + c[1] ** 2 + c[2] ** 2) ** 0.5
        m = miptex_of[texinfo] if 0 <= texinfo < len(miptex_of) else -1
        areas[m] = areas.get(m, 0.0) + max(1.0, area / (128.0 * 128.0))
    return areas


def glow_lights(data, palette, scale):
    """Surface light entities for the map's textures with fullbright pixels (palette 224-254).
    Each glowing texture has a budget of light (more the more of it glows), shared by the lights
    `light` spawns over its faces (at least one a face), none brighter than 130: a small button
    glows round itself, big or many glowing faces glow faintly each, and do not flood the room. The
    colour is a quarter of the way to white (a pure red floods a room with red)."""
    areas = texture_areas(data)
    offset, length = lump(data, 2)
    if length < 4:
        return ""
    (count,) = struct.unpack_from("<i", data, offset)
    out = []
    for i in range(count):
        (mip,) = struct.unpack_from("<i", data, offset + 4 + i * 4)
        if mip < 0:
            continue
        base = offset + mip
        name = data[base : base + 16].split(b"\0")[0].decode("latin-1")
        width, height, pix = struct.unpack_from("<iii", data, base + 16)
        low = name.lower()
        if not name or pix <= 0 or low.startswith(("*", "sky")) or width * height == 0:
            continue
        pixels = data[base + pix : base + pix + width * height]
        glowing = [c for c in pixels if 224 <= c <= 254]
        share = len(glowing) / float(len(pixels))
        if share < 0.03:
            continue
        r = sum(palette[c * 3] for c in glowing) / len(glowing)
        g = sum(palette[c * 3 + 1] for c in glowing) / len(glowing)
        b = sum(palette[c * 3 + 2] for c in glowing) / len(glowing)
        top = max(r, g, b, 1.0)
        r, g, b = (r * 0.75 + top * 0.25, g * 0.75 + top * 0.25, b * 0.75 + top * 0.25)
        spawned = max(1.0, areas.get(i, 1.0))
        budget = 600 * min(1.0, 0.4 + share * 2) * scale * (0.5 if "light" in low else 1.0)
        value = min(130 * scale, budget / spawned)
        if value < 12:
            continue
        out.append('{\n"classname" "light"\n"_surface" "%s"\n"light" "%d"\n"_color" "%d %d %d"\n'
                   '"_surface_offset" "2"\n}\n' % (name, value, r * 255 / top, g * 255 / top, b * 255 / top))
    return "".join(out)


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
    parser.add_argument("--no-glow", action="store_true", help="no surface lights for glowing textures")
    parser.add_argument("--glow-scale", type=float, default=1.0, help="brightness of the glowing textures' light")
    args = parser.parse_args()

    light = find_light(args.light)
    light_args = args.light_args.split()
    total = done = failed = 0
    palette = None
    if not args.no_glow:
        palette = pak_file(os.path.join(args.quake, "id1"), "gfx/palette.lmp")
        if not palette:
            print("gfx/palette.lmp not found: glowing textures will not light")
    glow_key = "" if not palette else " glow %g" % args.glow_scale

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
            stamp_value = hashlib.sha1(data + (" ".join(light_args) + glow_key).encode()).hexdigest()
            out_bsp = os.path.join(out_dir, base + ".bsp")
            out_lit = os.path.join(out_dir, base + ".lit")
            stamp = os.path.join(out_dir, base + ".relit")
            if not args.force and os.path.isfile(out_bsp) and os.path.isfile(stamp):
                with open(stamp) as f:
                    if f.read().strip() == stamp_value:
                        continue

            with tempfile.TemporaryDirectory() as tmp:
                work = os.path.join(tmp, base + ".bsp")
                lights = glow_lights(data, palette, args.glow_scale) if palette else ""
                with open(work, "wb") as f:
                    f.write(with_entities(data, entities_text(data) + lights) if lights else data)
                print("%s/%s ..." % (game, base), end=" ", flush=True)
                result = subprocess.run([light] + light_args + [work], cwd=tmp, stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, text=True, errors="replace")
                lit = os.path.join(tmp, base + ".lit")
                if result.returncode != 0 or not os.path.isfile(lit):
                    failed += 1
                    print("failed")
                    print(result.stdout[-2000:])
                    continue
                if lights:
                    # The map keeps its own entities (the surface lights were for `light` only).
                    with open(work, "rb") as f:
                        relit = f.read()
                    with open(work, "wb") as f:
                        f.write(with_entities(relit, entities_text(data)))
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
