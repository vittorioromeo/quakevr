#!/usr/bin/env python3
"""Drop every per-class Pickup key from a saved config's vr_agrunt_poses (his ask of 2026-10-31: one Pickup pose).

Each kind keeps its single shared Pickup key (class 7, VR_AGK_SHARED) and its Pickup legs key (class 0, phase 1);
every other class's Pickup key (phase 0: an override) and "no override" mark (phase 2) goes, for any kind, in the
current code format (kind * 100000 + class * 1000 + pose * 10 + phase) or the old one (an enforcer's 1xxxx, classes
10..16: agpose::codeFromOld). Everything else in the file stays byte-identical. Prints what it dropped.

Usage: python Misc/quakevr/drop_pickup_overrides.py <cfg path> [--dry-run]
"""

import argparse
import re
import sys

KIND_STRIDE = 100000  # (agpose::kindStride)
SHARED_CLASS = 7      # (VR_AGK_SHARED)
PICKUP = 3            # (agpose::Pickup)
LEGS_PHASE = 1        # (agpose::legCode: the Pickup's legs, kept)
KIND_NAMES = ["grunt", "enforcer", "knight", "death knight"]
CVAR = re.compile(r'^(\s*vr_agrunt_poses\s+")([^"]*)(".*)$', re.S)


def new_code(c):
    """agpose::codeFromOld."""
    return (c // 10000) * KIND_STRIDE + c % 10000 if 10000 <= c < KIND_STRIDE and 10 <= (c // 1000) % 100 <= 16 else c


def class_pickup(entry):
    """The new-format code of `entry` if it is a per-class Pickup key or mark, else None."""
    parts = entry.split()
    if not parts:
        return None
    try:
        c = new_code(int(parts[0]))
    except ValueError:
        return None
    cls, pose, phase = (c // 1000) % 100, (c // 10) % 100, c % 10
    return c if pose == PICKUP and cls != SHARED_CLASS and phase != LEGS_PHASE else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cfg", help="the config to rewrite (his ironwail.cfg)")
    ap.add_argument("--dry-run", action="store_true", help="only print what would be dropped")
    args = ap.parse_args()

    with open(args.cfg, "rb") as f:
        data = f.read()
    lines = data.split(b"\n")
    dropped = []
    for i, raw in enumerate(lines):
        line = raw.decode("utf-8", "surrogateescape")
        m = CVAR.match(line)
        if not m:
            continue
        kept = []
        for entry in m.group(2).split(";"):
            c = class_pickup(entry)
            if c is None:
                kept.append(entry)
            else:
                dropped.append((c, entry.split()[0]))
        lines[i] = (m.group(1) + ";".join(kept) + m.group(3)).encode("utf-8", "surrogateescape")

    for c, old in dropped:
        kind = c // KIND_STRIDE
        print("dropped %s (%s, class %d, %s)" % (old, KIND_NAMES[kind] if kind < len(KIND_NAMES) else "kind %d" % kind,
                                                 (c // 1000) % 100, "override" if c % 10 == 0 else "no-override mark"))
    print("drop_pickup_overrides: %d dropped%s" % (len(dropped), " (dry run: file untouched)" if args.dry_run else ""))
    if dropped and not args.dry_run:
        with open(args.cfg, "wb") as f:
            f.write(b"\n".join(lines))


if __name__ == "__main__":
    sys.exit(main())
