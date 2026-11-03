#!/usr/bin/env python3
"""menu_paths.py -- Misc/quakevr/menu_paths.txt: where each setting, page and button is in the VR menus.

The game's `vr_menu_dump_paths` prints Search's index (every page and row under the VR Settings and the Advanced VR
Options, whatever Menu Detail shows, nearest the roots first) as MP| console lines; this keeps them in menu_paths.txt,
one per line, `<cvar, @page or empty>|<menu detail level>|<page title>|<row label>|<path>`, for the scripts: the board
maps' {menu:...} (make_vrcalibration_map.py: a setting's row named by its cvar, not by its page's title and label, which
a rename changes) and settings_schema.py (the cvar comments' paths). Regenerate it after a menu change:

  bash <kit>/run.sh <agent> -Filter "^MP\\|" -QuitOn "menu paths dumped" -Script "vr_menu_dump_paths;toggleconsole;quit" > log
  python Misc/quakevr/menu_paths.py log          write menu_paths.txt from the log's MP| lines
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PATHS = os.path.join(HERE, "menu_paths.txt")
HEAD = ("# menu_paths.txt -- every page and row of the VR menus (vr_menu_dump_paths; Misc/quakevr/menu_paths.py):\n"
        "# <cvar, @page or empty>|<menu detail level>|<page title>|<row label>|<path>\n")


def load(path=PATHS):
    """[(cvar, level, title, label, path)] in the dump's order."""
    out = []
    for line in open(path, encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        cvar, level, title, label, full = line.rstrip("\n").split("|", 4)
        out.append((cvar, int(level), title, label, full))
    return out


def check_spec(spec, entries=None):
    """A {menu:...} spec naming a page (its title) or a page's row: exit when no such page or row is in the menus."""
    title, _, label = (s.strip() for s in spec.partition(">"))
    for c, _, t, lab, _ in entries or load():
        if t.lower() == title.lower() and (not label or (c != "@page" and lab.lower() == label.lower())):
            return spec
    raise SystemExit(f"menu_paths.txt has no {{menu:{spec}}} (renamed? regenerate menu_paths.txt and fix the board)")


def board(title, label=None, cvar=None, entries=None):
    """A board's {menu:...}: a page (its title), a button on it (its label), or a setting's row on it (its cvar: the
    label read from menu_paths.txt). Exits when the menus have no such page, button or row."""
    entries = entries or load()
    if cvar:
        for c, _, t, lab, _ in entries:
            if c == cvar and t.lower() == title.lower():
                return "{menu:" + f"{t}>{lab}" + "}"
        raise SystemExit(f"menu_paths.txt has no row for {cvar} on {title} (regenerate it: menu_paths.py)")
    return "{menu:" + check_spec(f"{title}>{label}" if label else title, entries) + "}"


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    # (run.sh -Filter's output is word-wrapped at the console's width: a long row goes on over the lines after it, which
    # kept apart would cut its path. A row's lines end at the next MP| line or a blank one; the game's own qconsole.log,
    # not wrapped, reads the same.)
    lines = []
    open_row = False
    for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
        text = line.rstrip("\r\n")
        at = text.find("MP|")
        if at >= 0:
            lines.append(text[at + 3:])
            open_row = True
        elif not text.strip() or text.startswith("menu paths dumped"):
            open_row = False
        elif open_row:
            lines[-1] += text
    if not lines:
        raise SystemExit("no MP| lines in " + sys.argv[1])
    bad = [r for r in lines if r.count("|") != 4]
    if bad:
        raise SystemExit(f"{len(bad)} rows without their 5 fields, the first: {bad[0]}")
    open(PATHS, "w", encoding="utf-8", newline="\n").write(HEAD + "".join(r + "\n" for r in lines))
    print(f"wrote {os.path.relpath(PATHS)}: {len(lines)} entries")


if __name__ == "__main__":
    main()
