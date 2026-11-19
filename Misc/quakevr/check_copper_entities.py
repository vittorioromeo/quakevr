"""Dwell's (and any Copper map's) entity coverage: which classnames and keys neither Quake VR's QC nor Copper's import has.

  python -I check_copper_entities.py [--maps <dwell>/maps] [--qc <QC dir>] [--map NAME] [--quiet]
                                     [--expect-missing N] [--expect-placements N] [--expect-fields N]

Reads the BSPs of a Copper mod's maps folder read-only (default Dwell 2.2's, the kit's downloads/dwell; b_* brush
models left out) and the QC sources, as check_mg3_entities.py (its reader and its rules for keys the engine handles
itself). A classname is Quake VR's (a spawn function of that name), Copper's (only Copper's import has one:
cu_<classname>, QC/copper) or missing (Dwell's own content: DWELL.md 2.1, P2); a key is resolved by a Quake VR field
or Copper's cu_<key>. Prints, per map, its Copper-only and missing classnames and its unknown keys, then the totals:

  copperentities: maps 24 classes 133 ours 90 copper 29 missing 14 placements 866 fields 12 field_uses 476

(2026-11-19: the 14 missing are exactly Dwell's own classnames of DWELL.md 2.1; the 12 keys are light-compiler keys
and Dwell's own: corpsetime, locksounds, skin_override, track_intermission, wateralpha.)

Exit 0, or 1 when an --expect-* total differs, 2 when the maps folder is not found. The maps are untrusted data:
nothing in them is executed or copied.
"""
import argparse
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_mg3_entities import DEFAULT_QC, engine_key, qc_names  # noqa: E402

DEFAULT_MAPS = "C:/OHWorkspace/qvr-kit/downloads/dwell/ex/dwellv2p2/maps"
PREFIX = "cu_"
DW_PREFIX = "dw_"  # Dwell's own, rebuilt (QC/dwell, DWELL.md P2a)


def entities(path):
    import re
    import struct
    data = open(path, "rb").read()
    off, ln = struct.unpack("<ii", data[4:12])  # BSP29, BSP2 and 2PSB: the entity lump first
    text = data[off:off + ln].decode("latin1").rstrip("\0")
    return [re.findall(r'"([^"]*)"\s*"([^"]*)"', m.group(1)) for m in re.finditer(r"\{([^{}]*)\}", text)]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--maps", default=DEFAULT_MAPS)
    ap.add_argument("--qc", default=DEFAULT_QC)
    ap.add_argument("--map", help="only this map (e.g. d1m1)")
    ap.add_argument("--quiet", action="store_true", help="totals only")
    ap.add_argument("--expect-missing", type=int)
    ap.add_argument("--expect-placements", type=int)
    ap.add_argument("--expect-fields", type=int)
    a = ap.parse_args()
    tag = "copperentities"
    if not os.path.isdir(a.maps):
        print("%s: the maps folder %s was not found (--maps)" % (tag, a.maps))
        return 2
    funcs, fields = qc_names(a.qc)
    maps = sorted(f for f in os.listdir(a.maps) if f.lower().endswith(".bsp") and not f.lower().startswith("b_"))
    if a.map:
        maps = [m for m in maps if m[:-4].lower() == a.map.lower()]

    classes, ours, copper, missing, dwell = (collections.Counter() for _ in range(5))
    missing_maps = collections.defaultdict(set)
    missing_fields = collections.Counter()
    for mp in maps:
        short = mp[:-4].lower()
        mc, cc, mf = collections.Counter(), collections.Counter(), collections.Counter()
        for kv in entities(os.path.join(a.maps, mp)):
            cls = next((v for k, v in kv if k == "classname"), "?")
            classes[cls] += 1
            if cls in funcs:
                ours[cls] += 1
            elif DW_PREFIX + cls in funcs:
                dwell[cls] += 1
            elif PREFIX + cls in funcs:
                copper[cls] += 1
                cc[cls] += 1
            else:
                mc[cls] += 1
            for k, v in kv:
                f = engine_key(k)
                if f and f != "classname" and f not in fields and PREFIX + f not in fields and v.strip():
                    mf[f] += 1
        missing.update(mc)
        missing_fields.update(mf)
        for c in mc:
            missing_maps[c].add(short)
        if not a.quiet:
            print("%-9s copper %4d missing %4d  %s%s" % (
                short, sum(cc.values()), sum(mc.values()), " ".join("%s:%d" % kv for kv in sorted(mc.items())),
                ("  | fields " + " ".join("%s:%d" % kv for kv in sorted(mf.items()))) if mf else ""))
    if not a.quiet:
        print("Copper's classnames (placements): " + " ".join("%s:%d" % kv for kv in sorted(copper.items())))
        print("missing classnames (placements/maps):")
        for c, n in sorted(missing.items()):
            print("  %-34s %4d/%d" % (c, n, len(missing_maps[c])))
        if missing_fields:
            print("unknown keys (uses): " + " ".join("%s:%d" % kv for kv in sorted(missing_fields.items())))
    placements = sum(missing.values())
    print("%s: maps %d classes %d ours %d copper %d dwell %d missing %d placements %d fields %d field_uses %d" % (
        tag, len(maps), len(classes), len(ours), len(copper), len(dwell), len(missing), placements, len(missing_fields),
        sum(missing_fields.values())))
    bad = ((a.expect_missing is not None and a.expect_missing != len(missing)) or
           (a.expect_placements is not None and a.expect_placements != placements) or
           (a.expect_fields is not None and a.expect_fields != len(missing_fields)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
