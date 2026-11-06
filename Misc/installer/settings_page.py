#!/usr/bin/env python3
"""settings_page.py -- Setup's settings page: its manifest from the game, and the check that it is not stale.

Misc/installer/settings_page.txt lists the page's settings (categories, subcategories, cvar names, in order). Everything
else about a setting (label, help, kind, range, choices, default, menu path) comes from the game's own menu rows: the
game's `vr_menu_dump_settings <cvar>...` prints them (MS| console lines), run headless through the kit's run.sh. They
go to quakevr/settings_page.json (the manifest, shipped in the package; Setup reads it from the install).

  python Misc/installer/settings_page.py update <agent> [--log FILE]   write the manifest from the game (or a saved log)
  python Misc/installer/settings_page.py check <agent> [--log FILE]    fail when the manifest is stale (exit 1)
  python Misc/installer/settings_page.py script                         the game's console script (for run.sh -Script)
  options: --list FILE (default settings_page.txt), --manifest FILE (default quakevr/settings_page.json),
           --save-log FILE (the game's MS| lines kept)

The check fails ("settings_page: FAIL ...") when a listed cvar is gone from the game, is not saved with the config
(CVAR_ARCHIVE; a graphics preset, kind "apply", is the one exception), the list is malformed, or the manifest differs
from what the game says now (a label, help, range, choice or default changed, a setting added or removed: run
`update`). A setting no menu row shows any more is a WARNING (it is left off the page). The last line is the verdict:
"settings_page check: N passed, M failed". Run by the release test suite (Misc/release/run_test_suite.py).
"""

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
LIST = os.path.join(HERE, "settings_page.txt")
MANIFEST = os.path.join(ROOT, "quakevr", "settings_page.json")
sys.path.insert(0, os.path.join(ROOT, "Misc", "quakevr"))

SCHEMA = 1
KINDS = ("toggle", "choice", "slider", "apply")


class ListError(Exception):
    pass


def read_list(path):
    """[(category, subcategory, cvar)] in order."""
    out, seen = [], set()
    cat = sub = None
    with open(path, encoding="utf-8") as f:
        for n, raw in enumerate(f, 1):
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            if line.startswith("["):
                if not line.endswith("]"):
                    raise ListError("%s:%d: a group line is [Category > Subcategory]" % (path, n))
                parts = [p.strip() for p in line[1:-1].split(">")]
                if len(parts) > 2 or not all(parts):
                    raise ListError("%s:%d: a group line is [Category] or [Category > Subcategory]" % (path, n))
                cat, sub = parts[0], parts[1] if len(parts) > 1 else ""
                continue
            if cat is None:
                raise ListError("%s:%d: %s before any [Category]" % (path, n, line))
            if " " in line or "\t" in line:
                raise ListError("%s:%d: one cvar a line (%s)" % (path, n, line))
            if line in seen:
                raise ListError("%s:%d: %s listed twice" % (path, n, line))
            seen.add(line)
            out.append((cat, sub, line))
    if not out:
        raise ListError("%s: no settings" % path)
    return out


def script(entries):
    return "vr_menu_dump_settings %s; toggleconsole; quit" % " ".join(e[2] for e in entries)


def run_game(agent, entries):
    import qvrtest  # (Misc/quakevr: the kit's run.sh)
    out = qvrtest.run_sh(agent, ["-Filter", r"menu settings dumped", "-QuitOn", "menu settings dumped",
                                 "-Timeout", "300", "-Script", script(entries)], timeout=600)
    # The console log itself (run.sh's printed lines are cut at the console's width; a help is longer).
    log = os.path.join(qvrtest.game_dir(agent), "qconsole.log")
    try:
        with open(log, encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return out


def parse_dump(text):
    """{cvar: {key: value}} from the MS| lines."""
    out = {}
    for line in text.splitlines():
        at = line.find("MS|")
        if at < 0:
            continue
        parts = line[at + 3:].rstrip("\r").split("|", 2)
        if len(parts) < 3:
            continue
        cvar, key, rest = parts
        d = out.setdefault(cvar, {"choices": []})
        if key in ("choice", "choicehelp"):
            value, label = rest.split("|", 1)
            value = fmt_number(float(value))
            if key == "choice":
                d["choices"].append({"value": value, "label": json.loads(label)})
            else:
                for c in d["choices"]:
                    if c["value"] == value:
                        c["help"] = json.loads(label)
        else:
            d[key] = json.loads(rest)
    return out


def fmt_number(v):
    return ("%d" % v) if v == int(v) else ("%g" % v)


def build(entries, dump):
    """(manifest, failures, warnings)."""
    fails, warns, settings = [], [], []
    for cat, sub, cvar in entries:
        d = dump.get(cvar)
        if d is None:
            fails.append("%s: not in the game's dump (did the run finish?)" % cvar)
            continue
        if not d.get("exists"):
            fails.append("%s: no such cvar in the game any more (renamed or removed: fix settings_page.txt)" % cvar)
            continue
        kind = d.get("kind", "none")
        if kind == "none":
            warns.append("%s: no menu row shows it any more (left off the page; fix settings_page.txt)" % cvar)
            continue
        if kind not in KINDS:
            fails.append("%s: kind %s unknown to Setup" % (cvar, kind))
            continue
        if kind != "apply" and not d.get("archive"):
            fails.append("%s: not saved with the config (no CVAR_ARCHIVE): the game would forget it" % cvar)
            continue
        s = {"cvar": cvar, "category": cat, "subcategory": sub, "kind": kind, "label": d.get("label", cvar),
             "help": d.get("help", ""), "path": d.get("path", ""), "default": d.get("default", "")}
        if kind == "slider":
            for k in ("min", "max", "step", "format", "hardmin", "hardmax"):
                if k in d:
                    s[k] = d[k]
        if kind in ("choice", "apply"):
            if not d["choices"]:
                fails.append("%s: a %s with no choices" % (cvar, kind))
                continue
            s["choices"] = d["choices"]
        settings.append(s)
    return {"schema": SCHEMA, "source": "Misc/installer/settings_page.py from settings_page.txt and the game's menu rows",
            "settings": settings}, fails, warns


def diff(old, new):
    """What changed between two manifests, as lines."""
    out = []
    if old.get("schema") != new.get("schema"):
        out.append("schema %s -> %s" % (old.get("schema"), new.get("schema")))
    a = {s["cvar"]: s for s in old.get("settings", [])}
    b = {s["cvar"]: s for s in new.get("settings", [])}
    for c in a:
        if c not in b:
            out.append("%s: in the manifest, no longer on the page" % c)
    for c in b:
        if c not in a:
            out.append("%s: on the page, not in the manifest" % c)
            continue
        for k in sorted(set(a[c]) | set(b[c])):
            if a[c].get(k) != b[c].get(k):
                out.append("%s: %s changed in the game: %s -> %s" % (c, k, json.dumps(a[c].get(k))[:80],
                                                                     json.dumps(b[c].get(k))[:80]))
    if not out and [s["cvar"] for s in old.get("settings", [])] != [s["cvar"] for s in new.get("settings", [])]:
        out.append("the order changed")
    return out


def main(argv):
    args = list(argv)
    opts = {}
    for k in ("--list", "--manifest", "--log", "--save-log"):
        if k in args:
            i = args.index(k)
            opts[k] = args[i + 1]
            del args[i:i + 2]
    if not args or args[0] not in ("update", "check", "script"):
        print(__doc__)
        return 2
    cmd = args[0]
    try:
        entries = read_list(opts.get("--list", LIST))
    except ListError as e:
        print("settings_page: FAIL %s" % e)
        print("settings_page check: 0 passed, 1 failed")
        return 1
    if cmd == "script":
        print(script(entries))
        return 0
    if "--log" in opts:
        with open(opts["--log"], encoding="utf-8", errors="replace") as f:
            text = f.read()
    else:
        if len(args) < 2:
            print("usage: settings_page.py %s <agent> [--log FILE]" % cmd)
            return 2
        text = run_game(args[1], entries)
    if "--save-log" in opts:
        with open(opts["--save-log"], "w", encoding="utf-8") as f:
            f.write("\n".join(l for l in text.splitlines() if "MS|" in l or "menu settings dumped" in l) + "\n")
    if "menu settings dumped" not in text:
        print("settings_page: FAIL the game's dump did not finish (no 'menu settings dumped' line)")
        print("settings_page check: 0 passed, 1 failed")
        return 1
    manifest, fails, warns = build(entries, parse_dump(text))
    path = opts.get("--manifest", MANIFEST)
    for w in warns:
        print("settings_page: WARNING %s" % w)
    if cmd == "update":
        for f_ in fails:
            print("settings_page: FAIL %s" % f_)
        if fails:
            print("settings_page update: not written (%d failed)" % len(fails))
            return 1
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            json.dump(manifest, f, indent=1, ensure_ascii=False)
            f.write("\n")
        print("settings_page update: %d settings written to %s" % (len(manifest["settings"]), path))
        return 0
    try:
        with open(path, encoding="utf-8") as f:
            old = json.load(f)
    except (OSError, ValueError) as e:
        old = {}
        fails.append("the manifest %s can't be read (%s): run update" % (path, e))
    changes = diff(old, manifest) if old else []
    for c in changes:
        fails.append("stale manifest: %s (run: python Misc/installer/settings_page.py update <agent>)" % c)
    for f_ in fails:
        print("settings_page: FAIL %s" % f_)
    passed = len(manifest["settings"]) - len([f_ for f_ in fails])
    print("settings_page check: %d passed, %d failed" % (max(0, passed), len(fails)))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
