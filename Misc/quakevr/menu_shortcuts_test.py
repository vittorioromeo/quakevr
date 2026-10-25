#!/usr/bin/env python3
"""menu_shortcuts_test.py <agent> -- every shortcut to a VR menu page pressed, headless (ROUND22.md, "Menu pages known
by number, not by builder": the corner's Console and Map Library buttons opened Search in the release build, its
link folding the three pages' identical builders into one). Run it on the build the author plays (build.sh <agent>
--lto; the quick build folds them too). On start, Menu Detail: Developer (the Toolgun, Save Default and Checklist).
  1. menu_vr ids: each page its own number, each title once (the builders folded at link listed, harmless now)
  2. every corner button by the main hand's laser (vr_mock_laser <name>, the trigger): Search, Console, Map Library,
     VR Settings, Advanced VR, Levels, Relighting, Checklist, Recording, Main Menu, New Panel (a second panel), Back to
     Game (pressed twice: the menu closed), Quicksave, Quickload, Toolgun, Save Default (armed, then written)
  3. the corner buttons by the stick (the off stick clicked, moved down to the button, the primary pressed): Search,
     Console, Map Library, VR Settings, Advanced VR, Levels, Relighting, Checklist, Main Menu
  4. the main menu's Download Maps row: the Map Library
  5. menu_vr links: every link on every page pressed and every Search entry opened, each landing on its page
Waits are counted frames (a press: the laser placed, the trigger down and up); the run ends on the final verdict line.
Prints PASS/FAIL per check and "menu shortcuts: N passed, M failed"; exits 1 on a failure.
"""
import os
import re
import subprocess
import sys

agent = sys.argv[1] if len(sys.argv) > 1 else "menuicf"
KIT = os.environ.get("KIT", "C:/OHWorkspace/qvr-kit")
BASH = os.environ.get("KIT_BASH", "C:/Program Files/Git/usr/bin/bash.exe")
if not os.path.exists(BASH):
    BASH = "bash"

# The corner buttons, in their order (vr_menuui.cpp, Tool): the stick moves down through them from Back to Game.
TOOLS = ["back", "quicksave", "quickload", "main", "search", "console", "settings", "advanced", "levels", "maps",
         "relighting", "newpanel", "toolgun", "savedefaults", "checklist"]
PAGES = {"search": "Search", "console": "Console", "maps": "Map Library", "settings": "VR Settings",
         "advanced": "Advanced VR Options", "levels": "levels", "relighting": "Graphics - Relighting",
         "checklist": "Checklist", "recording": "Recording", "main": "main"}

s = []
a = s.append


def laser(name):
    a(f"vr_mock_laser {name};wait3;vr_mock_button main trigger 1;wait3;vr_mock_button main trigger 0;wait3;"
      "vr_mock_laser off;wait3")


def stick(name):
    a("vr_mock_button off stickclick 1;wait3;vr_mock_button off stickclick 0;wait3")
    for _ in range(TOOLS.index(name)):
        a("vr_mock_stick off 0 -1;wait3;vr_mock_stick off 0 0;wait3")
    a("vr_mock_button main primary 1;wait3;vr_mock_button main primary 0;wait3")


def expect(what):
    a(f"menu_vr expect {what}")


def step(name):
    a(f"echo STEP {name}")


a("map start;wait30;vr_menu_level 2;menu_vr ids")
# 4 first (the main menu's rows from the top: Continue and Load Autosave shown or not by the saves; up from the top
# wraps to Quit, then Advanced VR, Options, Play Custom Map, Download Maps).
a("menu_main;wait3")
for _ in range(5):
    a("vr_mock_key uparrow;wait2")
step("mainrow")
a("menu_vr pos;vr_mock_key enter;wait3")
expect("Map Library")
# 2: by the laser, from the VR Settings each time (and from each other: Search to Console to the Map Library).
a("menu_vr 0;wait3")
for name in ["search", "console", "maps"]:
    laser(name)
    expect(PAGES[name])
for name in ["settings", "advanced", "levels", "relighting", "checklist", "recording", "main", "search"]:
    a("menu_vr 0;wait3")
    laser(name)
    expect(PAGES[name])
a("menu_vr 0;wait3")
laser("newpanel")
expect("panels 2")
expect("VR Settings")
laser("back")  # (each panel's Back in turn: the menu closed)
laser("back")
expect("closed")
a("togglemenu;wait3")
step("quicksave")
laser("quicksave")
expect("closed")
a("wait10;togglemenu;wait3")
step("quickload")
laser("quickload")
expect("closed")
a("wait30;togglemenu;wait3")
step("toolgun")
laser("toolgun")
expect("closed")
a("wait10;menu_vr 0;wait3")
step("savedefaults")
laser("savedefaults")
expect("VR Settings")
laser("savedefaults")
expect("VR Settings")
expect("done")
# (Several runs: the kit hands a script over on a command line, under 8191 characters with its other arguments.)
LIMIT = 6000
scripts = [";".join(s) + ";toggleconsole;quit"]
START = "map start;wait30;vr_menu_level 2"
END = "menu_vr expect done;toggleconsole;quit"
# 3: by the stick, as many buttons in a run as fit.
s.clear()
for name in ["search", "console", "maps", "settings", "advanced", "levels", "relighting", "checklist", "main"]:
    a("menu_vr 0;wait3")
    stick(name)
    expect(PAGES[name])
    chunk = ";".join(s)
    s.clear()
    if len(scripts) == 1 or len(scripts[-1]) + len(chunk) + len(END) > LIMIT:
        scripts.append(START + ";echo STEP stick")
    scripts[-1] += ";" + chunk
# 5
scripts.append(START + ";menu_vr 0;wait3;menu_vr links")
scripts = [x if x.endswith("quit") else x + ";" + END for x in scripts]

out = ""
if os.environ.get("LENGTHS"):
    print([len(x) for x in scripts])
    sys.exit(0)
for script in scripts:
    assert len(script) < LIMIT + 200, f"script too long for the kit's command line: {len(script)}"
    out += subprocess.run([BASH, f"{KIT}/run.sh", agent, "-Script", script, "-QuitOn", "menu expect: [0-9]+ passed",
                           "-Timeout", "300", "-Filter",
                           "STEP|menu expect|menu ids|menu links|menu_vr pos: menu 1|Quicksaving|Quickloading|Toolgun:|"
                           "Save Default|Saved [0-9]+ settings|New Panel|exit=|TIMEOUT"],
                          capture_output=True, text=True).stdout
text = re.sub(r"\s+", " ", out)
for k in ["STEP ", "menu expect", "menu ids", "menu links", "menu_vr pos", "Quicksaving", "Quickloading", "Toolgun:",
          "Save Default", "Saved ", "exit="]:
    text = text.replace(k, "\n" + k)
lines = [l.strip() for l in text.splitlines() if l.strip()]
if os.environ.get("VERBOSE"):
    print("\n".join(lines))
fail = 0
passed = 0


def check(ok, what):
    global fail, passed
    print(("PASS " if ok else "FAIL ") + what)
    fail += 0 if ok else 1
    passed += 1 if ok else 0


def after(stepname):
    i = next((n for n, l in enumerate(lines) if l == f"STEP {stepname}"), len(lines))
    return lines[i + 1:]


check(any(re.match(r"menu ids: \d+ pages, each its own number; \d+ share .*; OK", l) for l in lines), "menu_vr ids OK")
check(not any("menu ids: FAIL" in l for l in lines), "no page id problem")
row = next((l for l in after("mainrow") if l.startswith("menu_vr pos: menu 1")), "")
check("Download Maps" in row or "Map Library" in row, f"main menu row picked: {row[:80]}")
check(any(l.startswith("Quicksaving") for l in after("quicksave")), "Quicksave saves")
check(any(l.startswith("Quickloading") for l in after("quickload")), "Quickload loads")
check(any(l.startswith("Toolgun: in the") or l.startswith("Toolgun: already") for l in after("toolgun")), "Toolgun given")
sd = after("savedefaults")
check(any(l.startswith("Save Default: press again") for l in sd), "Save Default armed")
check(any(re.match(r"Saved \d+ settings", l) for l in sd), "Save Default written")
check(not any(l.startswith("New Panel:") for l in lines), "New Panel opened (no refusal)")
links = next((l for l in lines if re.match(r"menu links: \d+ links", l)), "")
check(bool(re.match(r"menu links: \d+ links and \d+ search entries opened, 0 failed", links)), f"links: {links[12:]}")
for l in lines:
    if "menu expect: FAIL" in l or "menu links: FAIL" in l:
        print("  " + l)
verdicts = [re.match(r"menu expect: (\d+) passed, (\d+) failed", l) for l in lines]
verdicts = [m for m in verdicts if m]
for run, m in enumerate(verdicts + [None] * (len(scripts) - len(verdicts)), 1):
    check(bool(m) and m.group(2) == "0", f"run {run} page checks: " +
          (f"{m.group(1)} passed, {m.group(2)} failed" if m else "no verdict (the run did not finish)"))
check(not any(l.startswith("TIMEOUT") for l in lines), "no run timed out")
print(f"menu shortcuts: {passed} passed, {fail} failed")
sys.exit(1 if fail else 0)
