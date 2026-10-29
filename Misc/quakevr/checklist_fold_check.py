#!/usr/bin/env python3
"""checklist_fold_check.py <agent> -- the Checklist's folding sections (ROUND23.md, "Checklist: folding sections"),
headless, in two runs of the agent's build:
  run 1: every section's total matches quakevr/checklist.txt and its open count the items' marks (CLSEC, CLITEM);
         section A folded: the Checklist page (vr_menu_dump) still shows its header and fold row, none of its items;
         Reload List keeps it folded; ticking A's first item moves A's open count by one (then unticked again);
  run 2: (a restart) A still folded; the default: with every item of the smallest section B passed and the folds back
         to default, a reload folds B and leaves A unfolded; B's items unticked again, a reload unfolds B.
The ticks and folds are put back as they were found. Prints "checklist_fold_check: N passed, M failed" and exits 1 on a
failure. Each run ends on its own CLFOLD_END line (-QuitOn)."""
import os
import re
import subprocess
import sys

AGENT = sys.argv[1] if len(sys.argv) > 1 else "checklist"
KIT = os.environ.get("KIT", "C:/OHWorkspace/qvr-kit")
TREE = f"C:/OHWorkspace/qvr-agents/{AGENT}"
GAME = f"{TREE}/quakevr"

passed = 0
failed = 0


def check(ok, what):
    global passed, failed
    if ok:
        passed += 1
    else:
        failed += 1
        print(f"FAIL: {what}")


def parse_list():
    """The file's sections: (name, [item texts]) in order (as vr_checklist.cpp reads it)."""
    sections = []
    loose = []
    text = open(f"{GAME}/checklist.txt", encoding="utf-8-sig").read()
    for line in text.split("\n"):
        line = line.strip(" \t\r\n")
        if not line or line.startswith("#") or line.startswith("//"):
            continue
        if len(line) >= 2 and line[0] == "[" and line[-1] == "]":
            sections.append((line[1:-1].strip(), []))
            continue
        line = re.sub(r"\s*\{test:[^{}]*\}$", "", line)
        (sections[-1][1] if sections else loose).append(line)
    return sections, loose


BASH = os.environ.get("BASH_EXE", "C:/Program Files/Git/bin/bash.exe" if os.name == "nt" else "bash")  # (not WSL's)
LINE_START = re.compile(r"^(CL[A-Z]+\||CLPHASE |CLFOLD_END|MD\||exit=|run\.ps1|run\.sh)")


def run(script, tag):
    out = subprocess.run([BASH, f"{KIT}/run.sh", AGENT, "-Script", script + ";echo CLFOLD_END;toggleconsole;quit",
                          "-QuitOn", "^CLFOLD_END", "-Timeout", "180", "-Filter", r"^CL|^MD\||rror|ENGINE|TIMEOUT"],
                         capture_output=True, text=True, errors="replace").stdout
    # The long lines come back wrapped at the console's width: each piece joined to the line it goes on.
    joined = []
    for line in out.splitlines():
        if joined and line and not LINE_START.match(line):
            joined[-1] += line
        elif line:
            joined.append(line)
    out = "\n".join(joined)
    os.makedirs(f"{TREE}/scratch", exist_ok=True)
    open(f"{TREE}/scratch/checklist_fold_{tag}.log", "w", encoding="utf-8").write(out)
    check("CLFOLD_END" in out and "TIMEOUT" not in out, f"{tag}: the run reached its end line")
    return out


def phases(out):
    """The output split at its CLPHASE markers: {name: [lines]}."""
    result = {}
    current = None
    for line in out.splitlines():
        m = re.match(r"^CLPHASE (\w+)", line)
        if m:
            current = m.group(1)
            result[current] = []
        elif current:
            result[current].append(line)
    return result


def listing(lines):
    """A vr_checklist listing's sections {index: (open, total, collapsed, name)} and items {index: (mark, section)}."""
    secs, items = {}, {}
    sums = 0
    for line in lines:
        f = line.split("|")
        sums += 1 if f[0] == "CLSUM" else 0
        if sums > 1:
            break  # (the phase's first listing only: each command after it prints its own)
        if f[0] == "CLSEC" and len(f) >= 6:
            secs[int(f[1])] = (int(f[2]), int(f[3]), f[4] == "collapsed", "|".join(f[5:]))
        elif f[0] == "CLITEM" and len(f) >= 4:
            items[int(f[1])] = (f[2], f[3])
    return secs, items


def page_rows(lines):
    """The Checklist page's rows in a vr_menu_dump: [(kind, label)]."""
    rows, inside = [], False
    for line in lines:
        f = line.split("|")
        if len(f) >= 3 and f[1] == "PAGE":
            inside = f[3] == "Checklist" if len(f) >= 4 else False
        elif inside and len(f) >= 3:
            rows.append((f[1], f[2]))
    return rows


sections, loose = parse_list()
check(len(sections) > 1, "the file has sections")
ticks_path = f"{GAME}/checklist_ticks.txt"
folds_path = f"{GAME}/checklist_sections.txt"
ticks_before = open(ticks_path, "rb").read() if os.path.exists(ticks_path) else None
folds_before = open(folds_path, "rb").read() if os.path.exists(folds_path) else None

first = len(loose)
starts = []
for name, its in sections:
    starts.append(first)
    first += len(its)
A = next(s for s in range(len(sections)) if sections[s][1])
x = starts[A]  # A's first item
B = min((s for s in range(len(sections)) if sections[s][1] and s != A), key=lambda s: len(sections[s][1]))

# Run 1.
out = run(";".join([
    "vr_checklist folds_default", "vr_checklist expand all",
    "echo CLPHASE all", "vr_checklist",
    f"vr_checklist collapse {A}",
    "echo CLPHASE dump", "vr_menu_dump 2",
    "echo CLPHASE reloaded", "vr_checklist reload", "vr_checklist",
    "echo CLPHASE ticked", f"vr_checklist tick {x}",
    "echo CLPHASE restored", f"vr_checklist tick {x}",
]), "run1")
ph = phases(out)
secs, items = listing(ph.get("all", []))
check(len(secs) == len(sections), f"sections: {len(secs)} listed, {len(sections)} in the file")
for s, (name, its) in enumerate(sections):
    o, t, c, n = secs.get(s, (-1, -1, True, ""))
    opens = sum(1 for i in range(starts[s], starts[s] + len(its)) if items.get(i, ("x",))[0] != "x")
    check(t == len(its) and o == opens and not c and n == name,
          f"section {s} '{name}': {o}/{t} listed, {opens}/{len(its)} by the file and marks, collapsed {c} "
          f"{[(i, items.get(i)) for i in range(starts[s], starts[s] + len(its)) if items.get(i, ('x',))[0] == 'x']}")
rows = page_rows(ph.get("dump", []))
labels = [label for kind, label in rows]
header_at = next((i for i, (k, l) in enumerate(rows) if k == "HEAD" and l.startswith(sections[A][0][:20])), -1)
check(header_at >= 0, f"section {A}'s header shown while folded")
check(header_at >= 0 and header_at + 1 < len(rows) and rows[header_at + 1][1].startswith("[+]"),
      f"its fold row under it, shown folded ({rows[header_at + 1][1] if 0 <= header_at < len(rows) - 1 else ''})")
after = rows[header_at + 2] if 0 <= header_at < len(rows) - 2 else ("", "")
check(after[0] == "HEAD" or after == ("", ""), f"none of its items listed (next row: {after})")
first_line = sections[A][1][0][:20]
check(not any(first_line in label for label in labels), "its first item's text not on the page")
check("Collapse All" in labels and "Expand All" in labels, "Collapse All and Expand All rows")
heads = [label for kind, label in rows if kind == "HEAD" and any(label.startswith(n[:20]) for n, _ in sections)]
check(len(heads) == len(sections), f"every section's header shown ({len(heads)} of {len(sections)})")
want_head = f"  {secs[A][0]}/{secs[A][1]}" if A in secs else "?"
check(header_at >= 0 and rows[header_at][1].endswith(want_head), f"its header's count ({rows[header_at][1] if header_at >= 0 else ''})")
secs2, _ = listing(ph.get("reloaded", []))
check(secs2.get(A, (0, 0, False))[2], "section A still folded after Reload List")
check(all(not secs2[s][2] for s in secs2 if s != A), "the others still unfolded after Reload List")
secs3, items3 = listing(ph.get("ticked", []))
was_open = items.get(x, ("x",))[0] != "x"
want = secs[A][0] - 1 if was_open else secs[A][0] + 1
check(secs3.get(A, (-9,))[0] == want, f"ticking item {x}: section A's open count {secs[A][0]} -> {secs3.get(A, (-9,))[0]} (want {want})")
check(sum(v[0] for v in secs3.values()) == sum(v[0] for v in secs.values()) + (want - secs[A][0]), "only section A's count moved")
secs4, _ = listing(ph.get("restored", []))
check(secs4.get(A, (-9,))[0] == secs[A][0], "unticked again: its count back")

# Run 2: a restart; then the default.
b_open = [i for i in range(starts[B], starts[B] + len(sections[B][1])) if items.get(i, ("x",))[0] != "x"]
out = run(";".join(
    ["echo CLPHASE restart", "vr_checklist"]
    + [f"vr_checklist pass {i}" for i in b_open]
    + ["vr_checklist folds_default", "vr_checklist reload", "echo CLPHASE bdone", "vr_checklist"]
    + [f"vr_checklist pass {i}" for i in b_open]
    + ["vr_checklist reload", "echo CLPHASE bopen", "vr_checklist"]), "run2")
ph = phases(out)
secs5, _ = listing(ph.get("restart", []))
check(secs5.get(A, (0, 0, False))[2], "section A still folded after a restart")
secs6, _ = listing(ph.get("bdone", []))
check(secs6.get(B, (1,))[0] == 0 and secs6[B][2], f"section {B} all passed: folded by default ({secs6.get(B)})")
check(not secs6.get(A, (0, 0, True))[2], "section A back to its default: unfolded")
secs7, _ = listing(ph.get("bopen", []))
check(secs7.get(B, (0, 0, True))[0] == len(b_open) and not secs7[B][2], f"section {B} open again: unfolded ({secs7.get(B)})")

# Put the ticks and folds back as found (the runs have ended: nothing writes them now).
for path, before in ((ticks_path, ticks_before), (folds_path, folds_before)):
    if before is not None:
        open(path, "wb").write(before)
    elif os.path.exists(path):
        open(path, "wb").write(b"")  # (no deletes: an empty file reads as no ticks, no folds)

print(f"checklist_fold_check: {passed} passed, {failed} failed")
sys.exit(1 if failed else 0)
