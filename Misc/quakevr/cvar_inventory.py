#!/usr/bin/env python3
"""Quake VR cvar inventory (docs/vr-port/CVAR_AUDIT.md).

For every cvar Quake VR declares (Quake/vr/vr_cvars.inc's QVR_CVAR lines, and the stand-alone `cvar_t x = {"x", ...}`
definitions in Quake/vr), where it is read and written: the engine's C/C++ (comments stripped; the menus, the config
migration and the motion recorder counted apart, as they read a setting without it changing anything), the QC (its
cvarh_ handle or a cvar("name") string), the shipped .cfg files, the menus, the tests and tools under Misc/quakevr, the
calibration board, the docs. Plus its default, whether it is archived, and the last commit that touched its line in
vr_cvars.inc.

    python Misc/quakevr/cvar_inventory.py [--csv out.csv] [--dead] [--name regex]

With no options it prints the counts. --dead lists the cvars no code reads (engine logic or QC): the candidates for
"definitely dead" (check each: a name built at run time, `"vr_mhull_" + cls`, is not seen). The generated families
(vr_prop_*, vr_wofs_*, vr_retro_*: built from tables in vr_props.cpp, vr_weapons.cpp, vr_retro.cpp) are not listed.
"""

import argparse
import csv
import os
import re
import subprocess
import sys
from collections import defaultdict

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

DECL_RE = re.compile(r'^\s*QVR_CVAR\(\s*([A-Za-z_0-9]+)\s*,\s*"((?:[^"\\]|\\.)*)"\s*,\s*([A-Z_| ]+)\)\s*(?://\s*(.*))?$')
STANDALONE_RE = re.compile(r'^\s*cvar_t\s+([A-Za-z_0-9]+)\s*=?\s*\{\s*"([a-z_0-9]+)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*,\s*([A-Z_| ]+)\}')
TOKEN_RE = re.compile(r"[A-Za-z_][A-Za-z_0-9]*")
WRITE_RE = re.compile(r"Cvar_Set\w*\s*\(|cvar_set\s*\(|Cvar_SetQuick|Cvar_SetValue|Cvar_SetROM|vr_default\b")

# Engine files whose references read a setting without it doing anything there.
MENU_FILES = re.compile(r"vr_menu[^/\\]*\.(cpp|inc|hpp)$|vr_menuui\.cpp$")
MIGRATION_FILES = re.compile(r"vr_cvars\.(cpp|hpp)$")
RECORDER_FILES = re.compile(r"vr_motion[^/\\]*\.cpp$")


def strip_comments(text):
    """C/C++/QC text with comments blanked (newlines kept, so line numbers hold) and strings kept."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(text[i : j + 1])
            i = j + 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def git_files():
    out = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True).stdout
    return [f for f in out.splitlines() if "/external/" not in f]


def read(path):
    with open(os.path.join(ROOT, path), encoding="utf-8", errors="replace") as f:
        return f.read()


def declarations():
    cvars = {}
    inc = "Quake/vr/vr_cvars.inc"
    for no, line in enumerate(read(inc).splitlines(), 1):
        m = DECL_RE.match(line)
        if m:
            cvars[m.group(1)] = dict(name=m.group(1), default=m.group(2), flags=m.group(3).strip(),
                                     comment=(m.group(4) or "").strip(), decl=f"{inc}:{no}", ident=m.group(1))
    for path in git_files():
        if not path.startswith("Quake/vr/") or not path.endswith(".cpp"):
            continue
        for no, line in enumerate(read(path).splitlines(), 1):
            m = STANDALONE_RE.match(line)
            if m and m.group(2) not in cvars:
                cvars[m.group(2)] = dict(name=m.group(2), default=m.group(3), flags=m.group(4).strip(), comment="",
                                         decl=f"{path}:{no}", ident=m.group(1))
    return cvars


def blame(cvars):
    """The last commit to touch each vr_cvars.inc line: {line number: "date subject"}."""
    out = subprocess.run(["git", "blame", "--line-porcelain", "Quake/vr/vr_cvars.inc"], cwd=ROOT,
                         capture_output=True, text=True, encoding="utf-8", errors="replace").stdout
    result, commits, cur, final = {}, {}, None, None
    for line in out.splitlines():
        m = re.match(r"^([0-9a-f]{40}) \d+ (\d+)", line)
        if m:
            cur, final = m.group(1), int(m.group(2))
            commits.setdefault(cur, {})
        elif line.startswith("committer-time "):
            commits[cur]["time"] = int(line.split()[1])
        elif line.startswith("summary "):
            commits[cur]["summary"] = line[8:]
        elif line.startswith("\t"):
            result[final] = cur
    import datetime

    def fmt(sha):
        c = commits.get(sha, {})
        day = datetime.datetime.fromtimestamp(c["time"], datetime.timezone.utc).strftime("%Y-%m-%d") if "time" in c else "?"
        return f"{sha[:8]} {day} {c.get('summary', '')[:70]}"

    return {no: fmt(sha) for no, sha in result.items()}


def scan(cvars):
    names = {c["name"] for c in cvars.values()}
    idents = {c["ident"]: c["name"] for c in cvars.values()}
    handles = {"cvarh_" + n: n for n in names}
    refs = defaultdict(lambda: defaultdict(list))  # name -> category -> ["file:line"]

    def add(name, cat, path, no):
        refs[name][cat].append(f"{path}:{no}")

    for path in git_files():
        low = path.lower()
        base = os.path.basename(path)
        is_code = path.startswith("Quake/") and low.endswith((".c", ".cpp", ".h", ".hpp", ".inc"))
        is_qc = low.endswith((".qc", ".qh"))
        is_cfg = low.endswith(".cfg")
        is_doc = low.endswith(".md") or low.endswith(".txt")
        is_tool = path.startswith("Misc/quakevr/") and not is_cfg and not is_doc
        if not (is_code or is_qc or is_cfg or is_doc or is_tool):
            continue
        if path == "Quake/vr/vr_cvars.inc":
            continue
        try:
            text = read(path)
        except OSError:
            continue
        if is_code or is_qc:
            text = strip_comments(text)
        for no, line in enumerate(text.splitlines(), 1):
            if not is_code and "vr" not in line and "cvarh_" not in line:
                continue
            for tok in set(TOKEN_RE.findall(line)):
                name = idents.get(tok) if is_code else None
                if name is None and tok in names:
                    name = tok
                handle = False
                if name is None and is_qc and tok in handles:
                    name, handle = handles[tok], True
                if name is None:
                    continue
                if is_code:
                    if re.search(r"\bextern\s+cvar_t\b", line) or STANDALONE_RE.match(line):
                        continue
                    reads = re.search(r"\b" + tok + r"\.(value|string)", line)
                    if MENU_FILES.search(path):
                        cat = "menu_read" if reads else "menu"
                    elif MIGRATION_FILES.search(path):
                        cat = "migration"
                    elif RECORDER_FILES.search(path) and not name.startswith("vr_motion"):
                        cat = "recorder"
                    else:
                        cat = "write" if WRITE_RE.search(line) and not reads else "read"
                    add(name, cat, path, no)
                elif is_qc:
                    if handle and (re.match(r"^\s*float\s+cvarh_", line) or "VR_CVAR_HMAKE" in line):
                        continue
                    if "VR_CVAR_HMAKE" in line:
                        continue
                    add(name, "qc_write" if WRITE_RE.search(line) else "qc", path, no)
                elif is_cfg:
                    add(name, "cfg", path, no)
                elif path.endswith(("make_vrcalibration_map.py", "make_vrtesthall_map.py")):
                    add(name, "board", path, no)
                elif is_tool:
                    add(name, "tool", path, no)
                else:
                    add(name, "doc", path, no)
    # QC handles made but never read: the HMAKE line alone.
    return refs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv")
    ap.add_argument("--dead", action="store_true", help="list the cvars no engine logic or QC reads")
    ap.add_argument("--name", help="only these (regex)")
    ap.add_argument("--refs", action="store_true", help="with --name: every reference")
    args = ap.parse_args()

    cvars = declarations()
    refs = scan(cvars)
    lines = blame(cvars)
    rows = []
    for c in cvars.values():
        r = refs.get(c["name"], {})
        no = int(c["decl"].rsplit(":", 1)[1]) if c["decl"].startswith("Quake/vr/vr_cvars.inc") else 0
        row = dict(
            name=c["name"], default=c["default"], archived=int("CVAR_ARCHIVE" in c["flags"]), flags=c["flags"],
            decl=c["decl"], reads_cpp=len(r.get("read", [])), writes_cpp=len(r.get("write", [])),
            reads_qc=len(r.get("qc", [])), writes_qc=len(r.get("qc_write", [])), menu=len(r.get("menu", [])), menu_reads=len(r.get("menu_read", [])),
            migration=len(r.get("migration", [])), recorder=len(r.get("recorder", [])), cfg=len(r.get("cfg", [])),
            board=len(r.get("board", [])), tools=len(r.get("tool", [])), docs=len(r.get("doc", [])),
            last_commit=lines.get(no, ""), comment=c["comment"][:160],
            first_read=(r.get("read") or r.get("qc") or [""])[0])
        rows.append(row)
    if args.name:
        rows = [x for x in rows if re.search(args.name, x["name"])]
    if args.csv:
        with open(args.csv, "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
    if args.refs and args.name:
        for x in rows:
            print(x["name"])
            for cat, where in sorted(refs.get(x["name"], {}).items()):
                print(f"  {cat}: {' '.join(where[:12])}{' ...' if len(where) > 12 else ''}")
        return
    if args.dead:
        for x in rows:
            if x["reads_cpp"] == 0 and x["reads_qc"] == 0:
                print(f"{x['name']:44s} menu={x['menu']}/{x['menu_reads']} mig={x['migration']} rec={x['recorder']} w={x['writes_cpp']}"
                      f"+{x['writes_qc']} cfg={x['cfg']} tools={x['tools']} docs={x['docs']}  {x['decl']}")
        return
    total = len(rows)
    print(f"cvars: {total}")
    print(f"archived: {sum(x['archived'] for x in rows)}")
    print(f"with a menu reference: {sum(1 for x in rows if x['menu'])}")
    print(f"read by engine logic: {sum(1 for x in rows if x['reads_cpp'])}")
    print(f"read by QC: {sum(1 for x in rows if x['reads_qc'])}")
    print(f"read by neither (candidates): {sum(1 for x in rows if not x['reads_cpp'] and not x['reads_qc'])}")
    print(f"named in shipped cfgs: {sum(1 for x in rows if x['cfg'])}")
    print(f"named on the calibration board: {sum(1 for x in rows if x['board'])}")


if __name__ == "__main__":
    sys.exit(main())
