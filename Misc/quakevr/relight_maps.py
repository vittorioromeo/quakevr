#!/usr/bin/env python3
# relight_maps.py -- re-lights your own copy of Quake's maps with ericw-tools' `light`, for Quake VR.
#
# Quake's 1996 lightmaps have no ambient occlusion, no bounced light and no colour, which makes
# the world look flat in VR. `light` can re-light a compiled .bsp from the light entities inside
# it: this script does that for every map of the installed campaigns (id1, hipnotic, rogue, or the
# folders you name), with smooth shadows (-extra4), strong ambient occlusion (-dirt) and coloured
# light (-lit), and no bounced light: shade stays dark, as in DarkPlaces (--bright: the earlier,
# brighter look, with bounce and weaker ambient occlusion). Geometry and entities stay as they are:
# only the light changes.
#
# Glowing textures light their surroundings (--glow, on by default): textures with fullbright pixels
# (buttons, computer panels, light fixtures, glowing runes) get lights in the colour of their glowing
# pixels, as bright as the share of them that glows and the other glowing things in the room allow:
# small faces a point light each in front of them, textures with big faces ericw's surface lights
# ("_surface"); strongly coloured ones brighter and further reaching, so that a red button tints its
# room; fixtures (named *light*) half as bright, since mappers put lights by them (glow_lights). These
# entities are only given to `light`: the relit map keeps its own. --glow-budget sets how much light a
# glowing texture shares out in a room (300; 600 before round 10, and with --bright).
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
#       [--glow-budget 300] [--bright] [--vis-dir <folder with id1.vis hipnotic.vis rogue.vis>]
#
# ericw-tools: https://github.com/ericwa/ericw-tools/releases (v0.18.1 was used; GPL). `light` may
# also be given by the ERICW_LIGHT environment variable or found on PATH.
#
# Already relit maps are skipped unless their source or the options changed (--force relights all).
#
# See-through water (--vis-dir, or the QUAKEVR_VISPATCH environment variable): id's maps were vised
# with water as a wall, so the engine keeps their liquids opaque; with the VisPatch data files
# (id1.vis, hipnotic.vis, rogue.vis, or <game>/vispatch.dat) the relit maps get water-vised
# visibility too (vis_maps.py, which can also do it on its own).

import argparse
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile

import quakepak
import vis_maps

# Smooth shadow edges, strong ambient occlusion in corners, coloured .lit output. No bounced light:
# it fills the shade and brightens maps (which the 2021 re-release was criticised for), where
# DarkPlaces, the moodier look, has none.
DEFAULT_LIGHT_ARGS = "-extra4 -dirt -dirtscale 1.5 -dirtdepth 96 -lit"
# The look before round 10 (--bright): weaker ambient occlusion and a little bounced light.
BRIGHT_LIGHT_ARGS = "-extra4 -dirt -dirtscale 1.0 -dirtdepth 96 -bounce -bouncescale 0.5 -lit"
DEFAULT_GLOW_BUDGET = 300.0
BRIGHT_GLOW_BUDGET = 600.0

# Brush models used as items (health and ammo boxes, the exploding box) are not levels.
SKIP_PREFIXES = ("b_",)


def game_maps(game_dir):
    """Maps of one game folder as the engine sees them: paks in order (later ones win), then loose
    files (which win over paks)."""
    maps = {}
    for pak in quakepak.game_paks(game_dir):
        maps.update((name, data) for name, data in quakepak.read_pak(pak).items()
                    if name.startswith("maps/") and name.endswith(".bsp"))
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
    for pak in quakepak.game_paks(game_dir):
        found = quakepak.read_pak(pak).get(wanted, found)
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


def texture_faces(data):
    """{miptex index: [(centre, area, normal) of each face using it]} (BSP29; empty for BSP2)."""
    if struct.unpack_from("<i", data, 0)[0] != 29:
        return {}
    pofs, plen = lump(data, 1)
    vofs, vlen = lump(data, 3)
    tofs, tlen = lump(data, 6)
    fofs, flen = lump(data, 7)
    eofs, elen = lump(data, 12)
    sofs, slen = lump(data, 13)
    planes = [struct.unpack_from("<3f", data, pofs + i * 20) for i in range(plen // 20)]
    verts = [struct.unpack_from("<3f", data, vofs + i * 12) for i in range(vlen // 12)]
    edges = [struct.unpack_from("<2H", data, eofs + i * 4) for i in range(elen // 4)]
    surfedges = struct.unpack_from("<%di" % (slen // 4), data, sofs)
    miptex_of = [struct.unpack_from("<i", data, tofs + i * 40 + 32)[0] for i in range(tlen // 40)]
    faces = {}
    for i in range(flen // 20):
        planenum, side, first, count, texinfo = struct.unpack_from("<hhihh", data, fofs + i * 20)
        pts = []
        for k in range(count):
            e = surfedges[first + k]
            pts.append(verts[edges[e][0]] if e >= 0 else verts[edges[-e][1]])
        if not pts:
            continue
        area = 0.0
        for k in range(1, len(pts) - 1):
            a = [pts[k][j] - pts[0][j] for j in range(3)]
            b = [pts[k + 1][j] - pts[0][j] for j in range(3)]
            c = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
            area += 0.5 * (c[0] ** 2 + c[1] ** 2 + c[2] ** 2) ** 0.5
        centre = tuple(sum(p[j] for p in pts) / len(pts) for j in range(3))
        normal = planes[planenum] if 0 <= planenum < len(planes) else (0.0, 0.0, 1.0)
        if side:
            normal = tuple(-x for x in normal)
        m = miptex_of[texinfo] if 0 <= texinfo < len(miptex_of) else -1
        faces.setdefault(m, []).append((centre, area, normal))
    return faces


def solid_at(data):
    """A function telling whether a point is inside the world's solid (BSP29 hull 0; never for BSP2)."""
    if struct.unpack_from("<i", data, 0)[0] != 29:
        return lambda p: False
    pofs, _ = lump(data, 1)
    nofs, _ = lump(data, 5)
    lofs, _ = lump(data, 10)

    def solid(p):
        node = 0
        for _ in range(4096):
            planenum, front, back = struct.unpack_from("<ihh", data, nofs + node * 24)
            nx, ny, nz, dist = struct.unpack_from("<4f", data, pofs + planenum * 20)
            child = front if p[0] * nx + p[1] * ny + p[2] * nz - dist >= 0 else back
            if child < 0:
                (contents,) = struct.unpack_from("<i", data, lofs + (-child - 1) * 28)
                return contents == -2
            node = child
        return False
    return solid


# `light` spawns a surface light about every SURFLIGHT x SURFLIGHT units of a face, at least one a face.
SURFLIGHT = 128.0
# Faces of one glowing texture closer than GLOW_OBJECT are one thing (the faces of a button, a panel,
# a slipgate); things closer than GLOW_ROOM light the same room together.
GLOW_OBJECT = 64.0
GLOW_ROOM = 256.0


def near(c, d, r):
    return sum((c[j] - d[j]) ** 2 for j in range(3)) < r * r


def spawned(faces):
    """How many lights `light` spawns on these faces."""
    return sum(max(1.0, f[1] / SURFLIGHT ** 2) for f in faces)


def things(faces):
    """Faces grouped into things (closer than GLOW_OBJECT): [(the faces, how many lights it counts as)].
    A thing counts as its area in lights, or the square root of its faces if more (`light` puts one on
    each, but a button's faces in the wall light little, and a cluster of small panels is not as bright
    as all of them)."""
    groups = []
    for f in faces:
        joined = [g for g in groups if any(near(f[0], h[0], GLOW_OBJECT) for h in g)]
        merged = [f]
        for g in joined:
            groups.remove(g)
            merged += g
        groups.append(merged)
    return [(g, max(len(g) ** 0.5, sum(h[1] for h in g) / SURFLIGHT ** 2)) for g in groups]


def crowd(thing, everyone):
    """How many lights light the room of a glowing thing: all the glowing things (`everyone`, of any
    glowing texture) within GLOW_ROOM of it. A map's buttons are far apart, each lighting its room
    alone; a slipgate's frame, lamps and panels share one room."""
    c = thing[0][0][0]
    return max(1.0, sum(n for g, n in everyone if near(c, g[0][0], GLOW_ROOM)))


def glow_lights(data, palette, scale, budget_base):
    """Light entities for the map's textures with fullbright pixels (palette 224-254).
    Each glowing texture has a budget of light (more the more of it glows), shared by the glowing
    things in the room (crowd()), none brighter than 130: a button alone in its room glows round
    itself, big or many glowing faces glow faintly each, and do not flood the room. Strongly coloured
    glows (red buttons, blue panels) get up to twice the budget, a cap 30% higher, twice the reach and
    their colour a tenth of the way to white, so that they tint the room; white and pale ones a quarter
    of the way to white. Small glowing faces (buttons, panels, signs) get a light each in front of them,
    as bright as their own room allows, unless in a wall; textures with big faces (a slipgate) get ericw's
    surface lights ("_surface"), as bright as their most crowded room allows. Fixtures (named *light*)
    get half, shared by all their lights in the map, since mappers put lights by them."""
    faces = texture_faces(data)
    solid = solid_at(data)
    offset, length = lump(data, 2)
    if length < 4:
        return ""
    (count,) = struct.unpack_from("<i", data, offset)
    glows = []
    for i in range(count):
        (mip,) = struct.unpack_from("<i", data, offset + 4 + i * 4)
        if mip < 0 or not faces.get(i):  # (an animation's other frames: `light` finds no faces named so)
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
        glows.append((name, share, (r, g, b), faces[i], things(faces[i])))
    everyone = [t for glow in glows for t in glow[4]]

    out = []
    for name, share, (r, g, b), used, mine in glows:
        top = max(r, g, b, 1.0)
        sat = (top - min(r, g, b)) / top  # 0 white .. 1 pure colour
        white = 0.25 - 0.15 * sat
        r, g, b = (r * (1 - white) + top * white, g * (1 - white) + top * white, b * (1 - white) + top * white)
        colour = "%d %d %d" % (r * 255 / top, g * 255 / top, b * 255 / top)
        budget = budget_base * min(1.0, 0.4 + share * 2) * scale
        cap = 130 * scale * (1 + 0.3 * sat)
        if "light" in name.lower():
            value = min(130 * scale, budget * 0.5 / spawned(used))
        elif all(f[1] <= 2 * SURFLIGHT ** 2 for f in used):
            for thing in mine:
                value = min(cap, budget * (1 + sat) / crowd(thing, everyone))
                spots = [tuple(c[j] + n[j] * 2 for j in range(3)) for c, _, n in thing[0]]
                spots = [p for p in spots if not solid(p)]
                for p in spots:
                    each = value / len(spots) ** 0.5
                    if each >= 12:
                        out.append('{\n"classname" "light"\n"origin" "%g %g %g"\n"light" "%d"\n"wait" "%.2f"\n'
                                   '"_color" "%s"\n}\n' % (p[0], p[1], p[2], each, 1 / (1 + sat), colour))
            continue
        else:
            value = min(cap, budget * (1 + sat) / max(crowd(t, everyone) for t in mine))
        if value < 12:
            continue
        out.append('{\n"classname" "light"\n"_surface" "%s"\n"light" "%d"\n"wait" "%.2f"\n"_color" "%s"\n'
                   '"_surface_offset" "2"\n}\n' % (name, value, 1 / (1 + sat), colour))
    return "".join(out)


def is_level(name, data):
    base = os.path.basename(name)
    if base.startswith(SKIP_PREFIXES):
        return False
    (version,) = struct.unpack_from("<i", data, 0)
    return version == 29 or data[:4] in (b"BSP2", b"2PSB")


def water_vise(path, patches, base):
    """Gives the relit map at `path` VisPatch's water-vised visibility, if there is a patch for it (and
    it has not got it already)."""
    entry = patches.get(base + ".bsp") if patches else None
    if entry is None:
        return
    with open(path, "rb") as f:
        data = f.read()
    result = vis_maps.vispatch(data, entry)
    if result is None:
        print("%s: the water-vis patch is for another version of the map, left alone" % base)
    elif result != data:
        with open(path, "wb") as f:
            f.write(result)


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
    parser.add_argument("--light-args", help="light's options (default: %r)" % DEFAULT_LIGHT_ARGS)
    parser.add_argument("--only", nargs="*", help="map names (e1m1 ...) to relight, for trying options")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--no-glow", action="store_true", help="no surface lights for glowing textures")
    parser.add_argument("--glow-scale", type=float, default=1.0, help="brightness of the glowing textures' light")
    parser.add_argument("--glow-budget", type=float,
                        help="light a glowing texture shares out (default %g)" % DEFAULT_GLOW_BUDGET)
    parser.add_argument("--vis-dir", default=os.environ.get("QUAKEVR_VISPATCH"),
                        help="folder with the VisPatch files (id1.vis ...): see-through water (vis_maps.py)")
    parser.add_argument("--bright", action="store_true",
                        help="the look before round 10: bounced light, weaker ambient occlusion, twice the glow")
    args = parser.parse_args()
    if args.light_args is None:
        args.light_args = BRIGHT_LIGHT_ARGS if args.bright else DEFAULT_LIGHT_ARGS
    if args.glow_budget is None:
        args.glow_budget = BRIGHT_GLOW_BUDGET if args.bright else DEFAULT_GLOW_BUDGET

    light = find_light(args.light)
    light_args = args.light_args.split()
    total = done = failed = 0
    palette = None
    if not args.no_glow:
        palette = pak_file(os.path.join(args.quake, "id1"), "gfx/palette.lmp")
        if not palette:
            print("gfx/palette.lmp not found: glowing textures will not light")

    for game in args.games:
        game_dir = os.path.join(args.quake, game)
        if not os.path.isdir(game_dir):
            print("%s: not installed, skipped" % game)
            continue
        out_dir = os.path.join(args.out, game, "maps")
        os.makedirs(out_dir, exist_ok=True)
        patches = vis_maps.load_patches(args.vis_dir, game) if args.vis_dir else None

        for name, data in sorted(game_maps(game_dir).items()):
            base = os.path.splitext(os.path.basename(name))[0]
            if args.only and base not in args.only:
                continue
            if not is_level(name, data):
                continue
            total += 1
            # The glowing textures' lights are in the stamp: a change to how they are made relights.
            lights = glow_lights(data, palette, args.glow_scale, args.glow_budget) if palette else ""
            stamp_value = hashlib.sha1(data + (" ".join(light_args) + lights).encode("latin-1")).hexdigest()
            out_bsp = os.path.join(out_dir, base + ".bsp")
            out_lit = os.path.join(out_dir, base + ".lit")
            stamp = os.path.join(out_dir, base + ".relit")
            if not args.force and os.path.isfile(out_bsp) and os.path.isfile(stamp):
                with open(stamp) as f:
                    if f.read().strip() == stamp_value:
                        water_vise(out_bsp, patches, base)
                        continue

            with tempfile.TemporaryDirectory() as tmp:
                work = os.path.join(tmp, base + ".bsp")
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
                water_vise(out_bsp, patches, base)
                with open(stamp, "w") as f:
                    f.write(stamp_value + "\n")
                done += 1
                print("ok")

    print("%d maps: %d relit, %d up to date, %d failed" % (total, done, total - done - failed, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
