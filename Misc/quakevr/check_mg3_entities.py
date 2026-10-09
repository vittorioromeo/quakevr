"""Dawn of the Machine (MG3) entity coverage: which of the owned MG3 maps' classnames and keys Quake VR's QC lacks.

  python check_mg3_entities.py [--campaign mg3|mg1|dopa] [--pak <rerelease/<campaign>/pak0.pak>] [--qc <QC dir>]
                               [--map NAME] [--quiet] [--expect-missing N] [--expect-placements N] [--expect-fields N]

--campaign picks the owned rerelease PAK (default mg3; check_mg1_entities.py is this with mg1); the totals line is
prefixed with its name ("mg1entities: ...").

Reads the owned PAK read-only (the entity lumps of its BSPs; nothing is copied or written) and the VR QC sources:
a classname is resolved when a QC function of that name exists (a spawn function), a key when a QC entity field of
that name exists or the engine handles it itself (ED_ParseEdict: keys starting with "_" are discarded, "angle" is
"angles", "light" is "light_lev", "sky*", "fog" and "alpha" are accepted silently). Prints, per map, its missing
classnames with their placements and its unknown keys, then the totals:

  mg3entities: maps 22 classes 156 missing 47 placements 1397 fields 10 field_uses 33

Exit 0, or 1 when an --expect-* total differs, 2 when the PAK is not found. MG3.md (docs/vr-port) lists what
each missing name is and which task resolves it.
"""
import argparse
import collections
import glob
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_QC = os.path.normpath(os.path.join(HERE, "..", "..", "QC"))
STEAM_ROOTS = [
    r"C:/Program Files (x86)/Steam/steamapps/common/Quake/rerelease",
    r"C:/Program Files/Steam/steamapps/common/Quake/rerelease",
    os.path.expanduser("~/.steam/steam/steamapps/common/Quake/rerelease"),
]

FUNC_RE = re.compile(r'^\s*void\s*(?:\(\s*\)\s*([A-Za-z_]\w*)\s*(?:=|$)|([A-Za-z_]\w*)\s*\(\s*(?:void)?\s*\)\s*(?:\{|$))',
                     re.M)
# `.float a, b;`, `nosave .vector v;`, `.void(float dt) f;`, `.float arr[4];`
FIELD_RE = re.compile(r'^\s*(?:(?:nosave|noref|local)\s+)*\.\s*[A-Za-z_]\w*\s*(?:\([^)]*\))?\s+([^;=]+?)\s*[;=]', re.M)


def read_pak(path):
    files = {}
    with open(path, "rb") as f:
        ident, dofs, dlen = struct.unpack("<4sii", f.read(12))
        if ident != b"PACK":
            raise ValueError("not a PAK: " + path)
        f.seek(dofs)
        for _ in range(dlen // 64):
            e = f.read(64)
            name = e[:56].split(b"\0")[0].decode("latin1")
            files[name] = struct.unpack("<ii", e[56:])
    return files


def entities(pakfile, files, name):
    o, l = files[name]
    pakfile.seek(o)
    data = pakfile.read(l)
    # BSP29, BSP2 and 2PSB all keep the entity lump first: its offset and length follow the 4-byte version.
    off, ln = struct.unpack("<ii", data[4:12])
    text = data[off:off + ln].decode("latin1").rstrip("\0")
    out = []
    for m in re.finditer(r"\{([^{}]*)\}", text):
        out.append(re.findall(r'"([^"]*)"\s*"([^"]*)"', m.group(1)))
    return out


def qc_names(qcdir):
    funcs, fields = set(), set()
    for p in glob.glob(os.path.join(qcdir, "**", "*.qc"), recursive=True):
        src = open(p, encoding="latin1").read()
        src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
        src = re.sub(r"//[^\n]*", "", src)
        for m in FUNC_RE.finditer(src):
            funcs.add(m.group(1) or m.group(2))
        for m in FIELD_RE.finditer(src):
            for n in m.group(1).split(","):
                n = re.sub(r"\[.*?\]", "", n).strip()
                if re.fullmatch(r"[A-Za-z_]\w*", n):
                    fields.add(n)
    return funcs, fields


def engine_key(key):
    """The field ED_ParseEdict stores a key in, or None when the engine handles or discards it itself."""
    key = key.rstrip(" ")
    if key.startswith("_") or key.startswith("sky") or key in ("fog", "alpha"):
        return None
    return {"angle": "angles", "light": "light_lev"}.get(key, key)


def main(default_campaign="mg3"):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--campaign", default=default_campaign, choices=("mg3", "mg1", "dopa"))
    ap.add_argument("--pak")
    ap.add_argument("--qc", default=DEFAULT_QC)
    ap.add_argument("--map", help="only this map (e.g. map1)")
    ap.add_argument("--quiet", action="store_true", help="totals only")
    ap.add_argument("--expect-missing", type=int)
    ap.add_argument("--expect-placements", type=int)
    ap.add_argument("--expect-fields", type=int)
    a = ap.parse_args()
    tag = a.campaign + "entities"
    env = "QVR_%s_PAK" % a.campaign.upper()

    pak = a.pak or os.environ.get(env) or next(
        (p for p in (os.path.join(r, a.campaign, "pak0.pak") for r in STEAM_ROOTS) if os.path.isfile(p)), None)
    if not pak or not os.path.isfile(pak):
        print("%s: the owned rerelease/%s/pak0.pak was not found (--pak or %s)" % (tag, a.campaign, env))
        return 2

    funcs, fields = qc_names(a.qc)
    files = read_pak(pak)
    maps = sorted(k for k in files if k.startswith("maps/") and k.endswith(".bsp"))
    if a.map:
        maps = [m for m in maps if os.path.basename(m)[:-4] == a.map]

    classes = collections.Counter()
    missing_classes = collections.Counter()
    missing_maps = collections.defaultdict(set)
    missing_fields = collections.Counter()
    empty_keys = collections.Counter()  # unknown keys with an empty value: editor leftovers that set nothing
    with open(pak, "rb") as pf:
        for mp in maps:
            short = os.path.basename(mp)[:-4]
            mc = collections.Counter()
            mf = collections.Counter()
            for kv in entities(pf, files, mp):
                cls = next((v for k, v in kv if k == "classname"), "?")
                classes[cls] += 1
                if cls not in funcs:
                    mc[cls] += 1
                for k, v in kv:
                    f = engine_key(k)
                    if f and f != "classname" and f not in fields:
                        if v.strip():
                            mf[f] += 1
                        else:
                            empty_keys["%s/%s" % (short, f)] += 1
            missing_classes.update(mc)
            missing_fields.update(mf)
            for c in mc:
                missing_maps[c].add(short)
            if not a.quiet:
                cl = " ".join("%s:%d" % kv for kv in sorted(mc.items()))
                fl = " ".join("%s:%d" % kv for kv in sorted(mf.items()))
                print("%-9s missing %4d  %s%s" % (short, sum(mc.values()), cl, ("  | fields " + fl) if fl else ""))

    if not a.quiet:
        print("missing classnames (placements/maps):")
        for c, n in sorted(missing_classes.items()):
            print("  %-34s %4d/%d" % (c, n, len(missing_maps[c])))
        if missing_fields:
            print("unknown keys (uses): " + " ".join("%s:%d" % kv for kv in sorted(missing_fields.items())))
        if empty_keys:
            print("empty unknown keys, ignored (map/key:uses): " +
                  " ".join("%s:%d" % kv for kv in sorted(empty_keys.items())))
    placements = sum(missing_classes.values())
    print("%s: maps %d classes %d missing %d placements %d fields %d field_uses %d"
          % (tag, len(maps), len(classes), len(missing_classes), placements, len(missing_fields),
             sum(missing_fields.values())))
    bad = (a.expect_missing is not None and a.expect_missing != len(missing_classes)) or \
          (a.expect_placements is not None and a.expect_placements != placements) or           (a.expect_fields is not None and a.expect_fields != len(missing_fields))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
