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
# Glowing textures light their surroundings (on by default; --no-glow): textures with fullbright pixels
# (buttons, computer panels, glowing runes), or, where they have none, a replacement texture's glowing
# _luma image (QRP: light1_*, tlight05, tlight10; --no-luma ignores those), get lights in the colour of
# what glows, as bright as the share of them that glows and the other glowing things in the room allow:
# small faces a point light each in front of them, textures with big faces ericw's surface lights
# ("_surface"); strongly coloured ones brighter and further reaching, so that a red button tints its
# room. --glow-budget sets how much light a glowing texture shares out in a room (300; 600 before round
# 10, and with --bright), --glow-scale scales all of it.
#
# Light fixtures (lamps, light panels, strip lights: textures named *light* or *lamp*, and those
# relight_textures.cfg names) are lamps, not decoration: each gets a light of its own, 250 for a lone
# one (--fixture-scale), half of it by a mapper's light of 300 that is on from the start (--fixture-lit;
# less reduced by a weaker one; one that starts off, like those of e1m1's lanterns until you walk in,
# does not count), less for a small or faint one and in a room of many.
# Until round 15 they shared half a budget over the whole map, which left every one too faint to get
# a light: they looked lit and lit nothing. Misc/quakevr/relight_textures.cfg sets, per texture (and
# map), whether it is a fixture, a glow or nothing, and its brightness, colour and reach; --list-glows
# lists each map's glowing textures and their lights without relighting. These entities are only
# given to `light`: the relit map keeps its own.
#
# The results go into quakevr/relit/<game>/maps/<map>.bsp, .lit and .lux. Quake VR loads them in place of
# <game>'s own maps (vr_relit_maps 1, the default; 0 plays the original lighting). The maps are
# id Software's: the relit copies are made on your machine from the ones you own and are not
# distributed. The .lux holds where the light comes from at each luxel (deluxemaps: Quake VR shades the
# bumps of the baked light with it, vr_deluxemap), and the .bsp a grid of the light in the air (the
# LIGHTGRID_OCTREE BSPX lump, every 32 units): both from ericw-tools 2 (OUTPUT_ARGS).
#
# Usage:
#   python Misc/quakevr/relight_maps.py --quake "C:/Program Files (x86)/Steam/steamapps/common/Quake"
#       [--light C:/tools/ericw-tools-2.0.0-alpha11-win64/light.exe] [--games id1 hipnotic rogue] [--out quakevr/relit]
#       [--light-args "..."] [--force] [--only e1m1 ...] [--no-glow] [--glow-scale 1.0]
#       [--glow-budget 300] [--fixture-scale 1.0] [--fixture-lit 0.5] [--textures <cfg>] [--no-luma]
#       [--list-glows] [--bright] [--vis-dir <folder with id1.vis hipnotic.vis rogue.vis>]
#
# ericw-tools 2.0.0-alpha11 (GPL): https://github.com/ericwa/ericw-tools/releases/tag/2.0.0-alpha11 (v0.18.1
# before round 17; it still works, without the light grid). `light` may also be given by the ERICW_LIGHT
# environment variable or found on PATH; else DEFAULT_LIGHT.
#
# Already relit maps are skipped unless their source, the options or `light`'s version changed (--force
# relights all).
#
# See-through water (--vis-dir, or the QUAKEVR_VISPATCH environment variable): id's maps were vised
# with water as a wall, so the engine keeps their liquids opaque; with the VisPatch data files
# (id1.vis, hipnotic.vis, rogue.vis, or <game>/vispatch.dat) the relit maps get water-vised
# visibility too (vis_maps.py, which can also do it on its own).

import argparse
import fnmatch
import hashlib
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

import quakepak
import vis_maps

# Smooth shadow edges, strong ambient occlusion in corners, coloured .lit output. No bounced light:
# it fills the shade and brightens maps (which the 2021 re-release was criticised for), where
# DarkPlaces, the moodier look, has none. ericw-tools 2.0.0-alpha11 gives the same light as v0.18.1 did
# with these (round 17: the luxels of e1m1, e1m2, e1m6, e2m1 and start agree to within 1%).
DEFAULT_LIGHT_ARGS = "-extra4 -dirt -dirtscale 1.5 -dirtdepth 96 -lit"
# The look before round 10 (--bright): weaker ambient occlusion and a little bounced light.
BRIGHT_LIGHT_ARGS = "-extra4 -dirt -dirtscale 1.0 -dirtdepth 96 -bounce -bouncescale 0.5 -lit"
# What else `light` writes (round 17; ericw-tools 2): the light's direction at each luxel (.lux beside the
# .lit: deluxemaps, the bumps' real light direction in Quake VR, vr_deluxemap) and a grid of the light in
# the air every 32 units (the LIGHTGRID_OCTREE BSPX lump inside the .bsp, about 0.3 MB a map: models' light).
OUTPUT_ARGS = "-lux -lightgrid"
# The ericw-tools the relit maps are made with (--light, ERICW_LIGHT or PATH win over it): 2.0.0-alpha11,
# https://github.com/ericwa/ericw-tools/releases/tag/2.0.0-alpha11 (GPL).
DEFAULT_LIGHT = "C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64/light.exe"
# In each map's stamp: a change to what this script gives `light` (beyond the lights and options) relights.
REVISION = "round 17: id's light 0"
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
    """The .bsp with its entity lump replaced by `text` (repacked: its BSPX lumps, such as light's
    LIGHTGRID_OCTREE, stay where engines look for them)."""
    return vis_maps.packed(data, {0: text.encode("latin-1") + b"\0"})


def entities_text(data):
    offset, length = lump(data, 0)
    return data[offset : offset + length].split(b"\0")[0].decode("latin-1")


def id_light_values(text):
    """The entities with id's `light` rule for a light of "light" "0": it gets the default, 300 (id's light.exe
    and ericw-tools v0.18 did so; ericw-tools 2 takes it as 0: e1m4's six torches went dark). For `light` only."""
    def fix(match):
        block = match.group(0)
        keys = dict(re.findall(r'"([^"]*)"\s+"([^"]*)"', block))
        if not keys.get("classname", "").startswith("light"):
            return block
        try:
            if float(keys.get("light", "300").split()[-1]) != 0:
                return block
        except (ValueError, IndexError):
            return block
        return re.sub(r'"light"\s+"[^"]*"', '"light" "300"', block)
    return re.sub(r"\{[^{}]*\}", fix, text)


def texture_faces(data):
    """{miptex index: [(centre, area, normal, points) of each face using it]} (BSP29; empty for BSP2)."""
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
        faces.setdefault(m, []).append((centre, area, normal, pts))
    return faces


def face_lightmaps(data):
    """Each face's lightmap as the engine reads it: [(lighting offset or -1, styles, luxels per style)] (BSP29;
    the extents as Ironwail's CalcSurfaceExtents works them out)."""
    if struct.unpack_from("<i", data, 0)[0] != 29:
        return []
    vofs, vlen = lump(data, 3)
    tofs, tlen = lump(data, 6)
    fofs, flen = lump(data, 7)
    eofs, elen = lump(data, 12)
    sofs, slen = lump(data, 13)
    verts = [struct.unpack_from("<3f", data, vofs + i * 12) for i in range(vlen // 12)]
    edges = [struct.unpack_from("<2H", data, eofs + i * 4) for i in range(elen // 4)]
    surfedges = struct.unpack_from("<%di" % (slen // 4), data, sofs)
    texinfo = [struct.unpack_from("<8fii", data, tofs + i * 40) for i in range(tlen // 40)]
    out = []
    for i in range(flen // 20):
        _, _, first, count, ti = struct.unpack_from("<hhihh", data, fofs + i * 20)
        styles = struct.unpack_from("<4B", data, fofs + i * 20 + 12)
        (lightofs,) = struct.unpack_from("<i", data, fofs + i * 20 + 16)
        vecs = texinfo[ti]
        size = 0
        if not vecs[9] & 1:  # (TEX_SPECIAL: no lightmap)
            sizes = []
            for j in range(2):
                vals = []
                for k in range(count):
                    e = surfedges[first + k]
                    v = verts[edges[e][0]] if e >= 0 else verts[edges[-e][1]]
                    vals.append(struct.unpack("<f", struct.pack("<f", v[0] * vecs[j * 4] + v[1] * vecs[j * 4 + 1] +
                                                                  v[2] * vecs[j * 4 + 2] + vecs[j * 4 + 3]))[0])
                sizes.append(int(math.ceil(max(vals) / 16) - math.floor(min(vals) / 16)) + 1)
            size = sizes[0] * sizes[1]
        out.append((lightofs, styles, size))
    return out


def remapped_lux(lux, lux_data, lit_data):
    """The .lux of `lux_data` (a .bsp) laid out for `lit_data` (the same map lit by another run of `light`: the same
    faces, their lightmaps elsewhere and their styles maybe in another order); a face whose styles differ gets the
    light straight on (128 128 255)."""
    body = bytearray(b"\x80\x80\xff" * lump(lit_data, 8)[1])
    for (src, src_styles, size), (dst, dst_styles, _) in zip(face_lightmaps(lux_data), face_lightmaps(lit_data)):
        if src < 0 or dst < 0 or not size or set(src_styles) != set(dst_styles):
            continue
        for k, style in enumerate(dst_styles):
            if style == 255:
                break
            j = src_styles.index(style)
            body[(dst + k * size) * 3 : (dst + (k + 1) * size) * 3] = lux[8 + (src + j * size) * 3 : 8 + (src + (j + 1) * size) * 3]
    return lux[:8] + bytes(body)


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


# A lone light fixture's light (--fixture-scale scales it); FIXTURE_LIT of it when a mapper's light of 300
# or more (on from the start) is within FIXTURE_NEAR of it, less reduced by a weaker one. Fixtures' lights
# stand FIXTURE_OFFSET in front of them, one every FIXTURE_STEP x FIXTURE_STEP units of a big one.
FIXTURE_LIGHT = 250.0
FIXTURE_LIT = 0.5
FIXTURE_NEAR = 128.0
FIXTURE_OFFSET = 4.0
FIXTURE_STEP = 128.0
# How far out of a recess a fixture's light may go.
RECESS_MAX = 48.0
# Textures named so are light fixtures unless relight_textures.cfg says otherwise.
FIXTURE_WORDS = ("light", "lamp")
# How much of a texture must glow for it to light: glowing textures, fixtures.
GLOW_MIN_SHARE = 0.03
FIXTURE_MIN_SHARE = 0.005
DEFAULT_TEXTURES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "relight_textures.cfg")
RULE_KEYS = ("kind", "scale", "light", "color", "reach")


def load_rules(path):
    """The per-texture rules of a relight_textures.cfg: [(pattern game/map/texture, {key: value})]."""
    rules = []
    if not path or not os.path.isfile(path):
        return rules
    with open(path) as f:
        for number, line in enumerate(f, 1):
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            words = line.split()
            pattern = words[0].lower()
            pattern = "/".join(["*"] * (2 - pattern.count("/")) + [pattern])
            keys = {}
            for word in words[1:]:
                key, _, value = word.partition("=")
                if key not in RULE_KEYS or not value:
                    sys.exit("%s:%d: unknown setting %r (known: %s)" % (path, number, word, ", ".join(RULE_KEYS)))
                if key == "kind" and value not in ("fixture", "glow", "off"):
                    sys.exit("%s:%d: kind is fixture, glow or off" % (path, number))
                if key in ("scale", "light", "reach"):
                    value = float(value)
                if key == "color":
                    value = tuple(float(x) for x in value.split(","))
                    if len(value) != 3:
                        sys.exit("%s:%d: color is r,g,b (0-255)" % (path, number))
                keys[key] = value
            rules.append((pattern, keys))
    return rules


def texture_rule(rules, where, name):
    """The settings of every rule matching game/map/texture, later ones over earlier ones."""
    full = "%s/%s/%s" % (where[0], where[1], name.lower())
    merged = {}
    for pattern, keys in rules:
        if fnmatch.fnmatchcase(full, pattern):
            merged.update(keys)
    return merged


def read_tga(path):
    """(width, height, [(r, g, b)]) of an uncompressed or RLE true-colour TGA, or None."""
    try:
        with open(path, "rb") as f:
            data = f.read()
        idlen, cmap, kind = data[0], data[1], data[2]
        width, height, bits = struct.unpack_from("<HHB", data, 12)
        if cmap or kind not in (2, 10) or bits not in (24, 32):
            return None
        size = bits // 8
        pos = 18 + idlen
        if kind == 2:
            raw = data[pos : pos + width * height * size]
        else:
            out = bytearray()
            while len(out) < width * height * size and pos < len(data):
                head = data[pos]
                pos += 1
                n = (head & 0x7F) + 1
                if head & 0x80:
                    out += data[pos : pos + size] * n
                    pos += size
                else:
                    out += data[pos : pos + n * size]
                    pos += n * size
            raw = bytes(out)
        return width, height, list(zip(raw[2::size], raw[1::size], raw[0::size]))
    except (OSError, IndexError, struct.error):
        return None


class Lumas:
    """The glowing parts of replacement textures (QRP and the like: textures/<name>_luma.tga), which glow
    in game where the map's own texture has no fullbright pixels (light1_*, tlight05, tlight09, tlight10):
    looked up as the engine does, textures/<map>/ then textures/, in the game folder then id1."""

    def __init__(self, quake):
        self.quake = quake
        self.cache = {}

    def glow(self, game, mapname, name):
        """(share of pixels that glow, their mean colour) of a texture's luma image, or None."""
        file = name.lower().replace("*", "#") + "_luma.tga"
        for folder in dict.fromkeys((game, "id1")):
            for sub in (mapname, ""):
                path = os.path.join(self.quake, folder, "textures", sub, file)
                if path not in self.cache:
                    self.cache[path] = self.measure(path) if os.path.isfile(path) else None
                if self.cache[path]:
                    return self.cache[path]
        return None

    @staticmethod
    def measure(path):
        image = read_tga(path)
        if not image or not image[2]:
            return None
        # QRP's lumas are often dim (tlight10's at most 64): what glows is what is at least half the brightest.
        peak = max(max(p) for p in image[2])
        if peak < 32:
            return None
        glowing = [p for p in image[2] if max(p) * 2 >= peak]
        return (len(glowing) / float(len(image[2])),
                tuple(sum(p[j] for p in glowing) / len(glowing) for j in range(3)))


def map_lights(data):
    """The map's own lights: [(origin, starts off, light)] (a "start off" light with a target name is off
    until triggered: the fixture by it then looks lit but lights nothing)."""
    out = []
    for block in entities_text(data).split("}"):
        keys = dict(re.findall(r'"([^"]*)"\s+"([^"]*)"', block))
        if not keys.get("classname", "").startswith("light") or "origin" not in keys:
            continue
        try:
            origin = tuple(float(x) for x in keys["origin"].split()[:3])
            flags = int(float(keys.get("spawnflags", "0")))
            value = abs(float(keys.get("light", keys.get("_light", "300")).split()[-1]))
        except (ValueError, IndexError):
            continue
        out.append((origin, bool(keys.get("targetname")) and bool(flags & 1), value))
    return out


def face_samples(face):
    """Points on a face, one every FIXTURE_STEP x FIXTURE_STEP units (its centre if small)."""
    centre, area, normal, pts = face
    if area <= FIXTURE_STEP ** 2 or len(pts) < 3:
        return [centre]
    # Axes in the face's plane.
    helper = (0.0, 0.0, 1.0) if abs(normal[2]) < 0.9 else (1.0, 0.0, 0.0)
    u = (normal[1] * helper[2] - normal[2] * helper[1], normal[2] * helper[0] - normal[0] * helper[2],
         normal[0] * helper[1] - normal[1] * helper[0])
    ul = sum(x * x for x in u) ** 0.5
    u = tuple(x / ul for x in u)
    v = (normal[1] * u[2] - normal[2] * u[1], normal[2] * u[0] - normal[0] * u[2], normal[0] * u[1] - normal[1] * u[0])
    flat = [(sum(p[j] * u[j] for j in range(3)), sum(p[j] * v[j] for j in range(3))) for p in pts]
    base = sum(pts[0][j] * normal[j] for j in range(3))

    def inside(x, y):
        sign = 0
        for k in range(len(flat)):
            (ax, ay), (bx, by) = flat[k], flat[(k + 1) % len(flat)]
            c = (bx - ax) * (y - ay) - (by - ay) * (x - ax)
            if abs(c) > 1e-3:
                if sign and (c > 0) != (sign > 0):
                    return False
                sign = c
        return True

    lo = [min(p[k] for p in flat) for k in range(2)]
    hi = [max(p[k] for p in flat) for k in range(2)]
    out = []
    counts = [max(1, int((hi[k] - lo[k]) / FIXTURE_STEP + 0.5)) for k in range(2)]
    for i in range(counts[0]):
        for j in range(counts[1]):
            x = lo[0] + (hi[0] - lo[0]) * (i + 0.5) / counts[0]
            y = lo[1] + (hi[1] - lo[1]) * (j + 0.5) / counts[1]
            if inside(x, y):
                out.append(tuple(x * u[k] + y * v[k] + base * normal[k] for k in range(3)))
    return out or [centre]


def recess_depth(face, solid):
    """How far in front of a face its light goes: FIXTURE_OFFSET, or, for a lamp set in a recess (e1m6's
    ceiling lamps sit in coffers with a lip), FIXTURE_OFFSET past where the recess walls end, so that the
    lip does not shade the room (at most RECESS_MAX). The recess walls are found just outside the face's
    edges: solid there on two opposite sides, at a depth in front of the face, is a recess or a slot round
    it (one side, or two sides meeting in a corner, is a wall beside it, not a recess)."""
    centre, _, normal, pts = face
    if len(pts) < 3:
        return FIXTURE_OFFSET
    outside = []
    for k in range(len(pts)):
        mid = tuple((pts[k][j] + pts[(k + 1) % len(pts)][j]) / 2 for j in range(3))
        out = [mid[j] - centre[j] for j in range(3)]
        along = sum(out[j] * normal[j] for j in range(3))
        out = [out[j] - along * normal[j] for j in range(3)]
        length = sum(x * x for x in out) ** 0.5
        if length > 1e-3:
            out = [x / length for x in out]
            outside.append((tuple(mid[j] + out[j] * 4 for j in range(3)), out))
    last = FIXTURE_OFFSET
    d = FIXTURE_OFFSET
    while d <= RECESS_MAX:
        if solid(tuple(centre[j] + normal[j] * d for j in range(3))):
            return last
        walls = [out for o, out in outside if solid(tuple(o[j] + normal[j] * d for j in range(3)))]
        if not any(sum(a[j] * b[j] for j in range(3)) < -0.5 for a in walls for b in walls):
            return d if d == FIXTURE_OFFSET else min(RECESS_MAX, d + FIXTURE_OFFSET)
        last = d
        d += 2.0
    return last


def fixture_spots(faces, solid):
    """Where a fixture's light goes: one spot for a small fixture (in front of it, or in its middle, or
    on top of it, whichever is in the open: several lights close together would add up to several
    times the light), one every FIXTURE_STEP units in front of its faces for a big one (a long strip,
    a lit floor)."""
    total = sum(f[1] for f in faces) or 1.0
    centre = tuple(sum(f[0][j] * f[1] for f in faces) / total for j in range(3))
    spread = max((sum((f[0][j] - centre[j]) ** 2 for j in range(3)) ** 0.5 for f in faces), default=0.0)
    if spread <= FIXTURE_STEP / 2 and all(f[1] <= FIXTURE_STEP ** 2 for f in faces):
        facing = tuple(sum(f[2][j] * f[1] for f in faces) / total for j in range(3))
        length = sum(x * x for x in facing) ** 0.5
        candidates = []
        if length > 0.3:
            facing = tuple(x / length for x in facing)
            depth = max([recess_depth(f, solid) for f in faces
                         if sum(f[2][j] * facing[j] for j in range(3)) > 0.8] or [FIXTURE_OFFSET])
            candidates += [tuple(centre[j] + facing[j] * o for j in range(3)) for o in (depth, FIXTURE_OFFSET, 2.0)]
        top = max(p[2] for f in faces for p in f[3]) if faces[0][3] else centre[2]
        candidates += [centre, (centre[0], centre[1], top + FIXTURE_OFFSET)]
        for f in sorted(faces, key=lambda f: -f[1]):
            candidates.append(tuple(f[0][j] + f[2][j] * FIXTURE_OFFSET for j in range(3)))
        for p in candidates:
            if not solid(p):
                return [p]
        return []
    spots = []
    for f in faces:
        depth = recess_depth(f, solid)
        for p in face_samples(f):
            for offset in (depth, FIXTURE_OFFSET, 2.0):
                q = tuple(p[j] + f[2][j] * offset for j in range(3))
                if not solid(q):
                    if not any(near(q, r, FIXTURE_STEP / 2) for r in spots):
                        spots.append(q)
                    break
    return spots


def light_entity(origin, value, wait, colour, extra=""):
    return ('{\n"classname" "light"\n"origin" "%g %g %g"\n"light" "%d"\n"wait" "%.2f"\n"_color" "%s"\n%s}\n'
            % (origin[0], origin[1], origin[2], value, wait, colour, extra))


def glow_lights(data, palette, scale, budget_base, fixture_scale=1.0, fixture_lit=FIXTURE_LIT, rules=None,
                where=("*", "*"), lumas=None, report=None):
    """Light entities for the map's glowing textures: fullbright pixels (palette 224-254), or else the
    glowing part of a replacement texture's _luma image (`lumas`, a Lumas), in the colour of what glows.

    Light fixtures (lamps, light panels, strip lights: textures named *light* or *lamp*, and what the
    `rules` of relight_textures.cfg say; `where` is (game, map) for them) get a light of their own:
    FIXTURE_LIGHT for a lone fixture (times fixture_scale), fixture_lit of it by a mapper's light of 300
    that is on from the start, less reduced by a weaker one (a "start off" one does not count: it leaves
    the fixture looking lit and lighting nothing until triggered), a little less for a small fixture or one with little glowing, divided by the square root of
    the fixtures in its room (GLOW_ROOM): one light for a small fixture (fixture_spots), one every
    FIXTURE_STEP units for a big one, each divided by the square root of their number (down to 0.4).

    Other glowing textures have a budget of light (more the more of them glows), shared by the glowing
    things in the room (crowd()), none brighter than 130: a button alone in its room glows round itself,
    big or many glowing faces glow faintly each, and do not flood the room. Strongly coloured glows (red
    buttons, blue panels) get up to twice the budget, a cap 30% higher, twice the reach and their colour
    a tenth of the way to white, so that they tint the room; white and pale ones a quarter of the way to
    white. Small glowing faces (buttons, panels, signs) get a light each in front of them, as bright as
    their own room allows, unless in a wall; textures with big faces (a slipgate) get ericw's surface
    lights ("_surface"), as bright as their most crowded room allows.

    A rule's scale multiplies a texture's light, light= sets a fixture's (instead of FIXTURE_LIGHT),
    color= its colour, reach= how far it reaches (1: as computed; the light entities' "wait" divided by
    it), kind= fixture, glow or off. `report`, a list, gets a line for each glowing texture."""
    if rules is None:
        rules = load_rules(DEFAULT_TEXTURES)
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
        rule = texture_rule(rules, where, low)
        kind = rule.get("kind", "fixture" if any(w in low for w in FIXTURE_WORDS) else "glow")
        if kind == "off":
            continue
        least = FIXTURE_MIN_SHARE if kind == "fixture" else GLOW_MIN_SHARE
        pixels = data[base + pix : base + pix + width * height]
        glowing = [c for c in pixels if 224 <= c <= 254]
        share = len(glowing) / float(len(pixels))
        source = "fullbright"
        if share >= least:
            colour = tuple(sum(palette[c * 3 + j] for c in glowing) / len(glowing) for j in range(3))
        else:
            luma = lumas.glow(where[0], where[1], name) if lumas else None
            if luma and luma[0] >= least:
                share, colour, source = luma[0], luma[1], "luma"
            elif kind == "fixture" and "kind" in rule:
                # Named a fixture but nothing glows: the colour of its brightest tenth.
                ranked = sorted(pixels, key=lambda c: -sum(palette[c * 3 : c * 3 + 3]))[: max(1, len(pixels) // 10)]
                share, source = 0.1, "brightest"
                colour = tuple(sum(palette[c * 3 + j] for c in ranked) / len(ranked) for j in range(3))
            else:
                continue
        if "color" in rule:
            colour = rule["color"]
        glows.append((name, kind, share, colour, faces[i], things(faces[i]), rule, source))
    everyone = [t for glow in glows for t in glow[5]]
    fixtures = [t for glow in glows if glow[1] == "fixture" for t in glow[5]]
    lamps = map_lights(data) if fixtures else []

    out = []
    for name, kind, share, (r, g, b), used, mine, rule, source in glows:
        top = max(r, g, b, 1.0)
        sat = (top - min(r, g, b)) / top  # 0 white .. 1 pure colour
        white = 0.25 - 0.15 * sat
        r, g, b = (r * (1 - white) + top * white, g * (1 - white) + top * white, b * (1 - white) + top * white)
        colour = "%d %d %d" % (r * 255 / top, g * 255 / top, b * 255 / top)
        wait = 1 / (1 + sat) / rule.get("reach", 1.0)
        own = rule.get("scale", 1.0) * scale
        made = []
        if kind == "fixture":
            each = rule.get("light", FIXTURE_LIGHT) * own * fixture_scale * (0.6 + 0.4 * min(1.0, share * 5))
            for thing in mine:
                value = each
                c = thing[0][0][0]
                # A mapper's light by it (on from the start) already lights round it: the stronger, the less.
                by = [v for o, off, v in lamps if not off and any(near(o, f[0], FIXTURE_NEAR) for f in thing[0])]
                if by:
                    value *= 1 - (1 - fixture_lit) * min(1.0, max(by) / 300.0)
                value /= sum(1 for t in fixtures if near(c, t[0][0][0], GLOW_ROOM)) ** 0.5
                # A small fixture (a lamp of a few square units) a little less.
                value *= min(1.0, max(0.5, (sum(f[1] for f in thing[0]) / 1024.0) ** 0.5))
                spots = fixture_spots(thing[0], solid)
                for p in spots:
                    v = min(300 * own * fixture_scale, value * max(0.4, len(spots) ** -0.5))
                    if v >= 12:
                        made.append(v)
                        # No ambient occlusion on a fixture's own light: -dirt all but puts out a lamp set
                        # in a recess (e1m6's ceiling lamps kept a fifth of their light).
                        out.append(light_entity(p, v, wait, colour, '"_dirt" "-1"\n'))
        else:
            budget = budget_base * min(1.0, 0.4 + share * 2) * own
            cap = 130 * own * (1 + 0.3 * sat)
            if all(f[1] <= 2 * SURFLIGHT ** 2 for f in used):
                for thing in mine:
                    value = min(cap, budget * (1 + sat) / crowd(thing, everyone))
                    spots = [tuple(c[j] + n[j] * 2 for j in range(3)) for c, _, n, _ in thing[0]]
                    spots = [p for p in spots if not solid(p)]
                    for p in spots:
                        v = value / len(spots) ** 0.5
                        if v >= 12:
                            made.append(v)
                            out.append(light_entity(p, v, wait, colour))
            else:
                value = min(cap, budget * (1 + sat) / max(crowd(t, everyone) for t in mine))
                if value >= 12:
                    made.append(value)
                    out.append('{\n"classname" "light"\n"_surface" "%s"\n"light" "%d"\n"wait" "%.2f"\n"_color" "%s"\n'
                               '"_surface_offset" "2"\n}\n' % (name, value, wait, colour))
        if report is not None:
            report.append("  %-16s %-7s %-10s glows %5.1f%%  faces %4d  things %3d  lights %4d  %s" % (
                name, kind, source, share * 100, len(used), len(mine), len(made),
                "light %d..%d" % (min(made), max(made)) if made else "(too faint: none)"))
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
    vis, leafs = entry
    if vis_maps.lump_bytes(data, 4) == vis and vis_maps.lump_bytes(data, 10) == leafs:
        result = vis_maps.packed(data)  # patched already (a map that was up to date)
    else:
        result = vis_maps.vispatch(data, entry)  # (repacked, keeping the BSPX lumps)
        if result is None:
            print("%s: the water-vis patch is for another version of the map, left alone" % base)
            return
    if result != data:
        with open(path, "wb") as f:
            f.write(result)


def find_light(explicit):
    for candidate in (explicit, os.environ.get("ERICW_LIGHT"), shutil.which("light"), shutil.which("light.exe"),
                      DEFAULT_LIGHT):
        if candidate and os.path.isfile(candidate):
            return candidate
    sys.exit("ericw-tools' light not found: pass --light <path to light(.exe)> or set ERICW_LIGHT "
             "(ericw-tools 2.0.0-alpha11: https://github.com/ericwa/ericw-tools/releases/tag/2.0.0-alpha11)")


def light_version(light):
    """`light`'s version as it prints it ("ericw-tools 2.0.0-alpha11", "TyrUtils-ericw v0.18.1-..."): part of
    each map's stamp, so that a new `light` relights every map once."""
    try:
        with tempfile.TemporaryDirectory() as tmp:  # (v0.18 writes a light.log where it runs)
            result = subprocess.run([light], cwd=tmp, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                    errors="replace", timeout=60)
        found = re.search(r"---- light / (.+?) ----", result.stdout)
        if found:
            return found.group(1).strip()
    except (OSError, subprocess.TimeoutExpired):
        pass
    with open(light, "rb") as f:  # (unknown: the executable itself)
        return "sha1 " + hashlib.sha1(f.read()).hexdigest()


def light_command(light, args):
    """`light` and its options, less those an old `light` does not know (v0.18 has no -lightgrid): with a
    warning, once."""
    version = light_version(light)
    if not version.startswith("ericw-tools 2") and "-lightgrid" in args:
        print("%s: not ericw-tools 2: no -lightgrid (the maps get no light grid)" % version)
        if "-lightgrid_dist" in args:
            i = args.index("-lightgrid_dist")
            args = args[:i] + args[i + 4 :]
        args = [a for a in args if a != "-lightgrid"]
    return [light] + args, version


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--quake", required=True, help="Quake folder (the one containing id1)")
    parser.add_argument("--light", help="ericw-tools light executable (default: ERICW_LIGHT, PATH, then %s)"
                        % DEFAULT_LIGHT)
    parser.add_argument("--games", nargs="+", default=["id1", "hipnotic", "rogue"])
    parser.add_argument("--out", default=os.path.normpath(os.path.join(here, "..", "..", "quakevr", "relit")))
    parser.add_argument("--light-args", help="light's options for the look (default: %r; %r are added)"
                        % (DEFAULT_LIGHT_ARGS, OUTPUT_ARGS))
    parser.add_argument("--only", nargs="*", help="map names (e1m1 ...) to relight, for trying options")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--no-glow", action="store_true", help="no surface lights for glowing textures")
    parser.add_argument("--glow-scale", type=float, default=1.0, help="brightness of the glowing textures' light")
    parser.add_argument("--glow-budget", type=float,
                        help="light a glowing texture shares out (default %g)" % DEFAULT_GLOW_BUDGET)
    parser.add_argument("--fixture-scale", type=float, default=1.0,
                        help="brightness of light fixtures' light (lamps, light panels), times --glow-scale")
    parser.add_argument("--fixture-lit", type=float, default=FIXTURE_LIT,
                        help="a fixture's light by a mapper's light that is on from the start (default %g of "
                             "a lone fixture's)" % FIXTURE_LIT)
    parser.add_argument("--textures", default=DEFAULT_TEXTURES,
                        help="per-texture settings (default Misc/quakevr/relight_textures.cfg; '' for none)")
    parser.add_argument("--no-luma", action="store_true",
                        help="ignore replacement textures' _luma images (textures/*_luma.tga) in finding what glows")
    parser.add_argument("--list-glows", action="store_true",
                        help="list each map's glowing textures and their lights, relighting nothing")
    parser.add_argument("--vis-dir", default=os.environ.get("QUAKEVR_VISPATCH"),
                        help="folder with the VisPatch files (id1.vis ...): see-through water (vis_maps.py)")
    parser.add_argument("--bright", action="store_true",
                        help="the look before round 10: bounced light, weaker ambient occlusion, twice the glow")
    args = parser.parse_args()
    if args.light_args is None:
        args.light_args = BRIGHT_LIGHT_ARGS if args.bright else DEFAULT_LIGHT_ARGS
    if args.glow_budget is None:
        args.glow_budget = BRIGHT_GLOW_BUDGET if args.bright else DEFAULT_GLOW_BUDGET

    light = None if args.list_glows else find_light(args.light)
    rules = load_rules(args.textures)
    lumas = None if args.no_luma else Lumas(args.quake)
    light_args = args.light_args.split() + [a for a in OUTPUT_ARGS.split() if a not in args.light_args.split()]
    command, version = light_command(light, light_args) if light else ([], "")
    if light:
        print("relighting with %s (%s)" % (version, light))
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
            report = [] if args.list_glows else None
            lights = glow_lights(data, palette, args.glow_scale, args.glow_budget, args.fixture_scale,
                                 args.fixture_lit, rules, (game, base), lumas, report) if palette else ""
            if args.list_glows:
                print("%s/%s:" % (game, base))
                for line in report or ["  (nothing glows)"]:
                    print(line)
                continue
            # So are `light`'s version and options: a new `light` relights every map once.
            stamp_value = hashlib.sha1(data + (" ".join(command[1:]) + version + lights + REVISION).encode("latin-1")).hexdigest()
            out_bsp = os.path.join(out_dir, base + ".bsp")
            out_lit = os.path.join(out_dir, base + ".lit")
            out_lux = os.path.join(out_dir, base + ".lux")
            stamp = os.path.join(out_dir, base + ".relit")
            if not args.force and os.path.isfile(out_bsp) and os.path.isfile(stamp):
                with open(stamp) as f:
                    if f.read().strip() == stamp_value:
                        water_vise(out_bsp, patches, base)
                        continue

            with tempfile.TemporaryDirectory() as tmp:
                work = os.path.join(tmp, base + ".bsp")
                with open(work, "wb") as f:
                    f.write(with_entities(data, id_light_values(entities_text(data)) + lights))
                print("%s/%s ..." % (game, base), end=" ", flush=True)
                result = subprocess.run(command + [work], cwd=tmp, stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, text=True, errors="replace")
                lit = os.path.join(tmp, base + ".lit")
                lux = os.path.join(tmp, base + ".lux")
                if result.returncode != 0 or not os.path.isfile(lit):
                    failed += 1
                    print("failed")
                    print(result.stdout[-2000:])
                    continue
                with open(work, "rb") as f:
                    relit = f.read()
                # The map keeps its own entities (the surface lights were for `light` only); repacked either
                # way, with its BSPX lumps (the light grid) where engines look for them.
                with open(work, "wb") as f:
                    f.write(with_entities(relit, entities_text(data)))
                shutil.copyfile(work, out_bsp)
                shutil.copyfile(lit, out_lit)
                if os.path.isfile(lux):
                    shutil.copyfile(lux, out_lux)
                elif os.path.isfile(out_lux):
                    os.remove(out_lux)  # (a stale one would not match the new lightmap)
                water_vise(out_bsp, patches, base)
                with open(stamp, "w") as f:
                    f.write(stamp_value + "\n")
                done += 1
                print("ok")

    if args.list_glows:
        return 0
    print("%d maps: %d relit, %d up to date, %d failed" % (total, done, total - done - failed, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
