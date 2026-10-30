#!/usr/bin/env python3
"""tune_promote.py <tunables.cfg> [--inc Quake/vr/vr_tunables.inc] [--check]

Makes a tunables.cfg's values (the tuning constants the author set off their defaults in the game: tune, Debug >
Tunables; Quake/vr/vr_tunables.cpp) the shipped defaults: each `tune <name> "<value>"` line's value is written as that
constant's default in vr_tunables.inc (QVR_TUNABLE(name, "default")). Run on a worker's tree, with his file read only
(his checkout's quakevr/tunables.cfg): the game never edits sources. Once the build with the new defaults is his, his
tunables.cfg's lines equal the defaults and the game drops them at the next quit (only changed values are kept).

--inc: the vr_tunables.inc to edit (default: this tree's). --check: print what would change, edit nothing.
Prints one line per constant changed, then "tune_promote: N promoted, M unchanged, K unknown"; exits 1 on an unknown
name (renamed or no longer a constant: fix by hand) or a value with a quote in it.
"""
import argparse
import os
import re
import sys


def read_cfg(path):
    """The tune lines' (name, value), in order; a later line for a name wins (as in the game)."""
    out = {}
    with open(path, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith("//"):
                continue
            m = re.match(r'^tune\s+(\S+)\s+"([^"]*)"\s*$', line) or re.match(r'^tune\s+(\S+)\s+(\S+)\s*$', line)
            if not m:
                print("tune_promote: %s:%d: not a tune line: %s" % (path, n, line))
                continue
            out[m.group(1).lower()] = m.group(2)
    return out


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[1])
    ap.add_argument("cfg")
    ap.add_argument("--inc", default=os.path.join(here, "..", "..", "Quake", "vr", "vr_tunables.inc"))
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()

    values = read_cfg(a.cfg)
    with open(a.inc, encoding="utf-8", newline="") as f:
        text = f.read()
    pattern = re.compile(r'^QVR_TUNABLE\((\w+), "([^"]*)"\)', re.M)
    known = {m.group(1).lower(): m.group(2) for m in pattern.finditer(text)}

    unknown = [n for n in values if n not in known]
    for n in unknown:
        print("tune_promote: %s is not a tuning constant in %s (renamed, or a setting again?)" % (n, a.inc))
    promoted = unchanged = 0

    def repl(m):
        nonlocal promoted, unchanged
        name = m.group(1).lower()
        if name not in values:
            return m.group(0)
        new = values[name]
        if new == m.group(2):
            unchanged += 1
            return m.group(0)
        promoted += 1
        print("  %s: \"%s\" -> \"%s\"" % (m.group(1), m.group(2), new))
        return 'QVR_TUNABLE(%s, "%s")' % (m.group(1), new)

    text = pattern.sub(repl, text)
    if not a.check and promoted:
        with open(a.inc, "w", encoding="utf-8", newline="") as f:
            f.write(text)
    print("tune_promote: %d promoted, %d unchanged, %d unknown%s" % (promoted, unchanged, len(unknown),
        " (--check: nothing written)" if a.check else ""))
    return 1 if unknown else 0


if __name__ == "__main__":
    sys.exit(main())
