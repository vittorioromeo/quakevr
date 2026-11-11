#!/usr/bin/env python3
"""checklist_tests_check.py [tree] -- offline check of the Checklist's Test buttons (no game run; the run-time check is
Misc/quakevr/scenario_check.sh: vr_scenario validate and runall).
For every "{test:<id>}" tag in quakevr/checklist.txt: quakevr/scenarios/<id>.txt exists and parses as vr_scenario.cpp
reads it (each line a known directive with its arguments, exactly one `map`, a `give` into main or off, a `spawn`'s
quoted name and offsets, a `start`'s numbers), and its map is one of ours (quakevr/maps/<map>.bsp) or an id, mission
pack or Arcane Dimensions map. Lists the untagged lines and the scene files no line uses (bench_* are the benchmarks').
Also the format: no "(agent/", "Test:" / "Test scene:" outside the tag, no scratch/ qvr-agents paths or .sh names, no
"[ ]" / "- [ ]" prefix, and the section headers in order ([New...] (a ship's unsorted lines), [Failed...] and [0. ...]
optional, then 1, 2, 3 with no gap or duplicate). `checklist_tests_check.py --lines <file>` vets only the item lines of a cl_lines file (for cl_insert.py).
Prints "checklist_tests_check: N passed, M failed" and exits 1 on a failure."""
import os
import re
import sys

TREE = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1] != "--lines" else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
GAME = os.path.join(TREE, "quakevr")
DIRECTIVES = {"title", "hint", "map", "start", "god", "notarget", "health", "give", "spawn", "spawnarmed", "cvar", "cmd"}
EXTERNAL_MAP = re.compile(r"^(start|end|e[1-4]m[1-9]|dm[1-6]|hip[1-3]m[1-6]|hipdm1|hipend|r[12]m[1-8]|ctf\w*|ad_\w+)$")
NUM = re.compile(r"^-?\d+(\.\d+)?$")

passed = 0
failed = 0


def check(ok, what):
    global passed, failed
    if ok:
        passed += 1
    else:
        failed += 1
        print(f"FAIL: {what}")


def words(line):
    """A directive line's words, "quoted" names as one."""
    return [a or b for a, b in re.findall(r'"([^"]*)"|(\S+)', line)]


def check_scene(sid):
    path = os.path.join(GAME, "scenarios", sid + ".txt")
    if not os.path.isfile(path):
        check(False, f"{sid}: no quakevr/scenarios/{sid}.txt")
        return
    maps = []
    problems = []
    for n, raw in enumerate(open(path, encoding="utf-8-sig"), 1):
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith("//"):
            continue
        w = words(line)
        d = w[0].lower()
        if d not in DIRECTIVES:
            problems.append(f"line {n}: unknown directive '{w[0]}'")
        elif d == "map":
            maps.append(w[1] if len(w) > 1 else "")
        elif d == "start" and not (2 <= len(w) - 1 <= 4 and all(NUM.match(x) for x in w[1:])):
            problems.append(f"line {n}: start needs 2 to 4 numbers")
        elif d == "give" and not (len(w) == 3 and w[2].lower() in ("main", "off")):
            problems.append(f"line {n}: give needs \"<entry>\" main|off")
        elif d == "spawn" and not (3 <= len(w) <= 6 and all(NUM.match(x) for x in w[2:])):
            problems.append(f"line {n}: spawn needs \"<entry>\" fwd left [up] [yaw]")
        elif d in ("cvar", "cmd", "title", "hint", "health") and len(w) < 2:
            problems.append(f"line {n}: {d} needs an argument")
    if len(maps) != 1:
        problems.append(f"{len(maps)} map lines (one needed)")
    elif not (os.path.isfile(os.path.join(GAME, "maps", maps[0] + ".bsp")) or EXTERNAL_MAP.match(maps[0])):
        problems.append(f"map {maps[0]} not in quakevr/maps nor an id, mission pack or AD map")
    check(not problems, f"{sid}: " + "; ".join(problems))


BAD_FORMS = [
    (re.compile(r"\(agent/"), "a branch name \"(agent/...\""),
    (re.compile(r"\bTest( scene)?:"), "\"Test:\" / \"Test scene:\" prose (use a {test:<id>} tag)"),
    (re.compile(r"scratch/|qvr-agents|qvr-kit"), "a scratch or tree path"),
    (re.compile(r"\b[\w.-]+\.sh\b"), "a script name (.sh)"),
    (re.compile(r"^(- )?\[ ?[xX]?\]"), "a checkbox prefix (lines are plain text)"),
]


def line_problems(line):
    """What is wrong with one checklist item line (its {test:<id>} tag cut off first: the one place a test id goes)."""
    body = re.sub(r"\s*\{test:[^{}]*\}$", "", line)
    out = [what for rx, what in BAD_FORMS if rx.search(body)]
    if "{test:" in body:
        out.append("a {test:...} tag that is not at the very end")
    return out


def check_sections(headers):
    """Optional unnumbered "[New..." (the lines ships add, not yet sorted) and "[Failed..." first, an optional "[0. ..."
    next, then "[1. ...", "[2. ..." with no gap, duplicate or reorder."""
    nums = []
    for h in headers:
        m = re.match(r"^\[(\d+)\.\s", h)
        if m:
            nums.append(int(m.group(1)))
        else:
            check((h.startswith("[New") or h.startswith("[Failed")) and not nums,
                  f"section '{h}': not numbered (only a leading [New...] or [Failed...] may be)")
    first = 0 if nums and nums[0] == 0 else 1
    check(nums == list(range(first, first + len(nums))), f"sections out of order, duplicated or with a gap: {nums}")


def lines_file_main(path):
    """--lines <file>: vet the item lines a ship is about to insert; headers and comments are skipped."""
    bad = 0
    for raw in open(path, encoding="utf-8-sig"):
        line = raw.strip()
        if not line or line.startswith("#") or (line[0] == "[" and line[-1] == "]"):
            continue
        for p in line_problems(line):
            bad += 1
            print(f"BAD LINE ({p}): {line[:90]}")
    print(f"checklist_tests_check --lines: {bad} problems")
    return 1 if bad else 0


def main():
    if len(sys.argv) > 2 and sys.argv[1] == "--lines":
        return lines_file_main(sys.argv[2])
    headers = []
    tags = []
    untagged = []
    for raw in open(os.path.join(GAME, "checklist.txt"), encoding="utf-8-sig"):
        line = raw.strip()
        if line and line[0] == "[" and line[-1] == "]":
            headers.append(line)
        if not line or line.startswith("#") or line.startswith("//") or (line[0] == "[" and line[-1] == "]"):
            continue
        for p in line_problems(line):
            check(False, f"{p}: {line[:80]}")
        m = re.search(r"\{test:([^{}]*)\}$", line)
        if m:
            tags.append(m.group(1).strip())
        else:
            untagged.append(line)
    check_sections(headers)
    for sid in sorted(set(tags)):
        check(bool(re.match(r"^[A-Za-z0-9_]+$", sid)), f"'{sid}': a scene id is one word")
        check_scene(sid)
    scenes = {f[:-4] for f in os.listdir(os.path.join(GAME, "scenarios")) if f.endswith(".txt")}
    unused = sorted(s for s in scenes - set(tags) if not s.startswith("bench_"))
    print(f"checklist_tests_check: {len(tags)} of {len(tags) + len(untagged)} lines tagged, {len(set(tags))} scenes")
    for line in untagged:
        print(f"  untagged: {line[:90]}")
    if unused:
        print(f"  scenes no line uses: {' '.join(unused)}")
    print(f"checklist_tests_check: {passed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
