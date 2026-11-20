#!/usr/bin/env python3
"""quoth_maps.py <run.log>: Quoth's acceptance verdict (quoth_maps.sh; docs/vr-port/QUOTH.md). Per map: it loaded, its
ruleset (Arcane Dimensions' on every map), missing spawn functions, unknown function keys, QC errors; the start map's
exit and the loaded saves. Writes a per-map table beside the log (census.txt) and ends with
`quoth_maps: N passed, M failed`."""
import collections
import os
import re
import sys

# Quoth classnames that spawn nothing on purpose (QUOTH.md "Not played"): none.
ALLOWED_MISSING = set()


def entry():
    return {"rules": "", "missing": collections.Counter(), "keys": set(), "errors": [], "ran": False}


def main():
    log = sys.argv[1]
    with open(log, encoding="latin1") as f:
        lines = f.read().split("\n")
    maps = collections.OrderedDict()
    cur = None
    expect_class = False
    for raw in lines:
        line = raw.strip()
        m = re.search(r"quoth: map (\S+)", line)
        if m:
            name = m.group(1)
            if name in maps:  # (a map played twice: the second time its own row)
                name += "#2"
            cur = maps.setdefault(name, entry())
            continue
        m = re.search(r"quoth: (ran|load|chain) (\S+)", line)
        if m:
            if m.group(1) == "ran":
                if cur is not None:
                    cur["ran"] = True
            else:
                cur = maps.setdefault("<%s %s>" % (m.group(1), m.group(2)), entry())
                cur["ran"] = True
            continue
        if cur is None:
            continue
        m = re.search(r"quothkeys: (\d+) passed, (\d+) failed", line)
        if m:
            cur["ran"] = True
            if m.group(2) != "0" or m.group(1) == "0":
                cur["errors"].append(line)
            continue
        if "quothkeys: FAIL" in line:
            cur["errors"].append(line[:140])
            continue
        if line.startswith("Ruleset:"):
            cur["rules"] = line.split("(")[0].replace("Ruleset:", "").strip()
        elif line.startswith("No spawn function"):
            expect_class = True
        elif expect_class and "classname" in line:
            cur["missing"][line.split()[-1].strip('"')] += 1
            expect_class = False
        elif "names a function this progs lacks" in line:
            cur["keys"].add(re.sub(r".*key \"(\S+)\".*\((\S+)\).*", r"\1=\2", line))
        elif re.search(r"Couldn't spawn|Host_Error|SV_Error|OBJECT ERROR|QC error|PR_ExecuteProgram|not found", line):
            cur["errors"].append(line[:140])
    passed = failed = 0
    rows = []
    if not any("quothkeys:" in l and "passed" in l for l in lines):
        failed += 1
        rows.append("quothkeys      FAIL no verdict line (vr_ad_test 211)")
    for name, r in maps.items():
        bad = []
        if r["errors"]:
            bad.append("errors")
        if not name.startswith("<") and "#" not in name and r["rules"] != "Arcane Dimensions":
            bad.append("ruleset %s" % (r["rules"] or "none"))
        miss = {c: n for c, n in r["missing"].items() if c not in ALLOWED_MISSING}
        if miss:
            bad.append("missing")
        if not r["ran"]:
            bad.append("did not run")
        passed += not bad
        failed += bool(bad)
        rows.append("%-14s %-4s %-18s missing %3d %s; keys %s; errors %s%s" % (
            name, "FAIL" if bad else "PASS", r["rules"] or "-", sum(r["missing"].values()),
            " ".join("%s:%d" % kv for kv in sorted(miss.items())) or "-", " ".join(sorted(r["keys"])) or "-",
            " | ".join(r["errors"][:3]) or "-", "  [%s]" % ", ".join(bad) if bad else ""))
    with open(os.path.join(os.path.dirname(log), "census.txt"), "w") as f:
        f.write("\n".join(rows) + "\n")
    print("\n".join(rows))
    print("quoth_maps: %d passed, %d failed" % (passed, failed))


if __name__ == "__main__":
    main()
