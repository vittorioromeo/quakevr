#!/usr/bin/env python3
"""settings_schema.py -- the converter to one settings schema (design review 9): each setting's menu row written once.

A menu page's row for a setting (`toggle(...)`, `slider(...)`, `cycle(...)` with only literal arguments, and its
`.extend(...)` / `.help(...)`) becomes `row(opt::<cvar>)`; its label, range, step, format, extent and help go to one line
of Quake/vr/vr_menu_rows.inc (QVR_TOGGLE / QVR_SLIDER / QVR_CYCLE, `_BYNAME` for a row that found its cvar by name).
`.advanced()` / `.developer()` stay on the page (where the row shows), as does anything after another modifier
(`.unavailableBecause(...)`). A setting with two different rows (another label or range on another page) keeps its
first in the schema; the other stays written out on its page.

  python Misc/quakevr/settings_schema.py convert [--dry]   rewrite the pages and write vr_menu_rows.inc (run once)
  python Misc/quakevr/settings_schema.py stats             what convert would do

The weapon and prop offset pages (vr_menu_wofs.inc, vr_menu_props.inc: design review 3's) and the Tunables page are left
alone. Run from the repository's root.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
VR = os.path.join(ROOT, "Quake", "vr")
ROWS = os.path.join(VR, "vr_menu_rows.inc")
SKIP = {"vr_menu_wofs.inc", "vr_menu_props.inc", "vr_menu_tunables.inc", "vr_menu_rows.inc"}
# Cvars a row may not take into the schema (its variable not visible where row() is built, or not in every build).
EXCLUDE = set()

TOKEN = re.compile(r'''
    (?P<ws>\s+)
  | (?P<lc>//[^\n]*)
  | (?P<bc>/\*.*?\*/)
  | (?P<str>"(?:[^"\\\n]|\\.)*")
  | (?P<chr>'(?:[^'\\\n]|\\.)*')
  | (?P<num>(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?[fFuUlL]*)
  | (?P<id>[A-Za-z_]\w*)
  | (?P<pp>\#[^\n]*)
  | (?P<op>::|->|.)
''', re.X | re.S)


def tokenize(text):
    toks = []
    for m in TOKEN.finditer(text):
        kind = m.lastgroup
        toks.append((kind, m.group(), m.start(), m.end()))
    return toks


class Fail(Exception):
    pass


def split_args(sig, i):
    """sig[i] is '(': the top-level arguments (lists of tokens) and the index after the matching ')'."""
    depth, args, cur = 0, [], []
    j = i + 1
    while j < len(sig):
        t = sig[j]
        v = t[1]
        if v in "([{":
            depth += 1
        elif v in ")]}":
            if depth == 0:
                if v != ")":
                    raise Fail("bracket")
                if cur or args:
                    args.append(cur)
                return args, j + 1
            depth -= 1
        if v == "," and depth == 0:
            args.append(cur)
            cur = []
        else:
            cur.append(t)
        j += 1
    raise Fail("unclosed")


def strings(arg):
    if not arg or any(t[0] != "str" for t in arg):
        raise Fail("not a string literal")
    return [t[1] for t in arg]


def number(arg):
    vals = [t[1] for t in arg]
    if len(arg) == 1 and arg[0][0] == "num":
        return vals[0]
    if len(arg) == 2 and vals[0] == "-" and arg[1][0] == "num":
        return "-" + vals[1]
    raise Fail("not a number literal")


def numval(text):
    t = text.rstrip("fFuUlL")
    return float(t)


def cvar_ref(arg, globals_):
    vals = [t[1] for t in arg]
    if len(arg) == 1 and arg[0][0] == "str" and re.fullmatch(r'"[A-Za-z_]\w*"', vals[0]):
        return vals[0][1:-1], True
    if len(arg) == 1 and arg[0][0] == "id" and vals[0] in globals_:
        return vals[0], False
    if len(arg) == 2 and vals[0] == "&" and arg[1][0] == "id" and vals[1] in globals_:
        return vals[1], False
    raise Fail("cvar")


def choices(arg):
    """{{v, "label"}, ...} -> [(v, [label strings])]."""
    if not arg or arg[0][1] != "{" or arg[-1][1] != "}":
        raise Fail("choices")
    inner = arg[1:-1]
    out = []
    j = 0
    while j < len(inner):
        if inner[j][1] != "{":
            raise Fail("choice")
        k = j + 1
        while k < len(inner) and inner[k][1] != "}":
            k += 1
        body = inner[j + 1:k]
        comma = [n for n, t in enumerate(body) if t[1] == ","]
        if len(comma) != 1:
            raise Fail("choice body")
        v = number(body[:comma[0]])
        lab = strings(body[comma[0] + 1:])
        out.append((v, lab))
        j = k + 1
        if j < len(inner):
            if inner[j][1] != ",":
                raise Fail("choice sep")
            j += 1
    if not out:
        raise Fail("no choices")
    return out


def parse_row(sig, i, globals_):
    """sig[i] is toggle/slider/cycle at an item's start: (row dict, index of the first token not taken)."""
    kind = sig[i][1]
    if i + 1 >= len(sig) or sig[i + 1][1] != "(":
        raise Fail("call")
    args, j = split_args(sig, i + 1)
    row = dict(kind=kind)
    if kind == "toggle":
        if len(args) != 2:
            raise Fail("toggle args")
        row["label"] = strings(args[0])
        row["name"], row["byname"] = cvar_ref(args[1], globals_)
    elif kind == "slider":
        if len(args) != 6:
            raise Fail("slider args")
        row["label"] = strings(args[0])
        row["name"], row["byname"] = cvar_ref(args[1], globals_)
        row["range"] = [number(a) for a in args[2:5]]
        row["format"] = strings(args[5])
    else:
        if len(args) != 3:
            raise Fail("cycle args")
        row["label"] = strings(args[0])
        row["name"], row["byname"] = cvar_ref(args[1], globals_)
        row["choices"] = choices(args[2])
    row["ext"] = None
    row["help"] = None
    row["mods"] = []
    end = j  # the last token taken into the schema, exclusive
    k = j
    while k + 2 < len(sig) and sig[k][1] == "." and sig[k + 1][0] == "id" and sig[k + 2][1] == "(":
        m = sig[k + 1][1]
        try:
            margs, after = split_args(sig, k + 2)
        except Fail:
            break
        if m in ("advanced", "developer") and not margs:
            row["mods"].append(m)
        elif m == "extend" and kind == "slider" and row["ext"] is None and not margs:
            row["ext"] = "auto"
        elif m == "extend" and kind == "slider" and row["ext"] is None and len(margs) == 2:
            try:
                row["ext"] = (number(margs[0]), number(margs[1]))
            except Fail:
                break
        elif m == "help" and row["help"] is None and len(margs) == 1:
            try:
                row["help"] = strings(margs[0])
            except Fail:
                break
        else:
            break
        k = after
        if m not in ("advanced", "developer"):
            end = k
    if row["mods"] and end < k:
        pass
    # modifiers after the last schema part stay where they are (not re-emitted)
    row["mods_moved"] = []
    kk = j
    while kk < end:
        if sig[kk][1] == "." and sig[kk + 1][1] in ("advanced", "developer"):
            row["mods_moved"].append(sig[kk + 1][1])
        kk += 1
    return row, end


def key(row):
    """What the row is, for comparing two rows of one setting."""
    s = lambda pieces: "".join(p[1:-1] for p in pieces) if pieces is not None else None
    k = [row["kind"], row["byname"], s(row["label"]), s(row["help"])]
    if row["kind"] == "slider":
        k += [tuple(numval(v) for v in row["range"]), s(row["format"])]
        e = row["ext"]
        k.append(e if e in (None, "auto") else tuple(numval(v) for v in e))
    if row["kind"] == "cycle":
        k.append(tuple((numval(v), s(lab)) for v, lab in row["choices"]))
    return tuple(k)


def cvar_globals():
    names = set()
    for dirpath, _, files in os.walk(os.path.join(ROOT, "Quake")):
        for f in files:
            if f.endswith((".c", ".cpp", ".h", ".hpp", ".inc")):
                text = open(os.path.join(dirpath, f), encoding="utf-8", errors="replace").read()
                names |= set(re.findall(r"\bcvar_t\s+(\w+)\s*[;=]", text))
                names |= set(re.findall(r"^\s*QVR_CVAR\((\w+),", text, re.M))
    return names


STARTERS = {"{", ",", "(", "return", "=", "?", ":"}


def file_order(f):
    """The player pages' files first, then vr_menu.cpp, then the Debug pages': a setting on two pages keeps the first's."""
    return (2 if f in DEBUG_FILES else 1 if f.endswith(".cpp") else 0, f)


DEBUG_FILES = {"vr_menu_debug.inc", "vr_menu_debug_tests.inc", "vr_menu_automation.inc", "vr_menu_cheats.inc",
               "vr_menu_recording.inc", "vr_menu_ragtests.inc", "vr_menu_ragtool.inc", "vr_menu_ad_monsters.inc",
               "vr_menu_checklist.inc", "vr_menu_console.inc"}


def scan(globals_):
    files = sorted((f for f in os.listdir(VR) if re.fullmatch(r"vr_menu.*\.(inc|cpp)", f) and f not in SKIP),
                   key=file_order)
    found = []  # (file, row, start offset, end offset)
    fails = {}
    for f in files:
        text = open(os.path.join(VR, f), encoding="utf-8").read()
        toks = tokenize(text)
        sig = [t for t in toks if t[0] not in ("ws", "lc", "bc")]
        for i, t in enumerate(sig):
            if t[0] != "id" or t[1] not in ("toggle", "slider", "cycle"):
                continue
            if i + 1 >= len(sig) or sig[i + 1][1] != "(" or i == 0 or sig[i - 1][1] not in STARTERS:
                continue
            try:
                row, end = parse_row(sig, i, globals_)
            except Fail as e:
                fails[str(e)] = fails.get(str(e), 0) + 1
                continue
            if row["name"] in EXCLUDE:
                fails["excluded"] = fails.get("excluded", 0) + 1
                continue
            start, stop = t[2], sig[end - 1][3]
            inner = [x for x in toks if x[0] in ("lc", "bc", "pp") and start <= x[2] < stop]
            if inner:
                fails["comment inside"] = fails.get("comment inside", 0) + 1
                continue
            row["file"] = f
            found.append((f, row, start, stop))
    return found, fails


def page_of(text, offset):
    m = None
    for m2 in re.finditer(r"^\[\[nodiscard\]\][^\n]*?(\w+)\(\)\s*$", text[:offset], re.M):
        m = m2
    return m.group(1) if m else "?"


def fmt_strings(pieces, indent):
    return ("\n" + indent).join(pieces)


def emit(row):
    lab = " ".join(row["label"])
    hp = fmt_strings(row["help"], "    ") if row["help"] is not None else "nullptr"
    sep = "\n    " if row["help"] is not None and (len(row["help"]) > 1 or len(row["help"][0]) > 60) else " "
    suffix = "_BYNAME" if row["byname"] else ""
    n = row["name"]
    if row["kind"] == "toggle":
        return f"QVR_TOGGLE{suffix}({n}, {lab},{sep}{hp})"
    if row["kind"] == "slider":
        e = row["ext"]
        ext = "noExt" if e is None else "ext()" if e == "auto" else f"ext({e[0]}, {e[1]})"
        lo, hi, st = row["range"]
        return f"QVR_SLIDER{suffix}({n}, {lab}, {lo}, {hi}, {st}, {' '.join(row['format'])}, {ext},{sep}{hp})"
    lines, cur = [], ""
    for v, lab_ in row["choices"]:
        c = "{%s, %s}" % (v, " ".join(lab_))
        if cur and len(cur) + len(c) + 2 > 114:
            lines.append(cur + ",")
            cur = ""
        cur = c if not cur else cur + ", " + c
    lines.append(cur)
    ch = "\n    ".join(lines)
    return f"QVR_CYCLE{suffix}({n}, {lab},{sep}{hp},\n    {ch})"


HEAD = """// vr_menu_rows.inc -- the settings' menu rows, each written once (design review 9's settings schema): its label, range,
// step, format, how far past the bar left and right go, help and choices. A page lists only `row(opt::<cvar>)` (and its
// headers); where it shows (its menu path) is the page it is on: `vr_menu_dump_paths` prints every row's, and
// Misc/quakevr/menu_paths.txt holds them for the scripts (the board maps). Made by Misc/quakevr/settings_schema.py from
// the pages' rows; edit it by hand now. A setting shown twice with different texts keeps its other row on its page.
//   QVR_TOGGLE(cvar, label, help)                                       Off / On
//   QVR_SLIDER(cvar, label, min, max, step, format, extent, help)       extent: noExt, ext() (four bars' widths past each
//                                                                       end; none below a bar from 0 up), ext(lo, hi)
//   QVR_CYCLE(cvar, label, help, {value, "label"}...)
//   ..._BYNAME: the cvar found by its name when the page is built (Cvar_FindVar: the row left out without it; Ironwail's
//   settings), not by its C++ variable. help: nullptr for none. The order is the pages' (the file each came from).
"""


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "stats"
    dry = "--dry" in sys.argv
    globals_ = cvar_globals()
    found, fails = scan(globals_)
    first = {}
    for f, row, s, e in found:
        first.setdefault(row["name"], row)
    take = [(f, row, s, e) for f, row, s, e in found if key(row) == key(first[row["name"]])]
    kept = len(found) - len(take)
    print(f"rows parsed: {len(found)}, to row(): {len(take)}, kept written out (a second, different row): {kept}, "
          f"settings: {len(first)}")
    print("not parsed:", ", ".join(f"{k} {v}" for k, v in sorted(fails.items(), key=lambda x: -x[1])))
    if mode != "convert":
        return
    by_file = {}
    for f, row, s, e in take:
        by_file.setdefault(f, []).append((row, s, e))
    out = [HEAD]
    written = set()
    order = sorted({f for f, _, _, _ in found}, key=file_order)
    for f in order:
        text = open(os.path.join(VR, f), encoding="utf-8").read()
        rows = by_file.get(f, [])
        page = None
        for row, s, e in rows:
            if row["name"] in written:
                continue
            p = page_of(text, s)
            if p != page:
                out.append(f"\n// {f}: {p}\n")
                page = p
            out.append(emit(row) + "\n")
            written.add(row["name"])
        new = text
        for row, s, e in sorted(rows, key=lambda r: -r[1]):
            mods = "".join(f".{m}()" for m in row["mods_moved"])
            new = new[:s] + f"row(opt::{row['name']}){mods}" + new[e:]
        if new != text and not dry:
            open(os.path.join(VR, f), "w", encoding="utf-8", newline="\n").write(new)
    if not dry:
        open(ROWS, "w", encoding="utf-8", newline="\n").write("".join(out))
    print(f"wrote {len(written)} rows to {os.path.relpath(ROWS, ROOT)}")


if __name__ == "__main__":
    main()
