#!/usr/bin/env python3
"""menu_panels_test.py <agent> -- several VR menu panels open at once (MULTI_MENU.md stages 3 and 4; ROUND22.md,
"Several menu panels"), headless with the mock headset on e1m1, read from vr_menu_panels' MENUPANEL(S) lines:
  1. New Panel (vr_menu_panels open): a second panel, on the first's page; placed beside it (right)
  2. the second on another page (focused, menu_vr 0): the first keeps its page
  3. the laser on the first: it is focused, its row follows the laser; on the second: likewise, the first's row kept
  4. a key goes to the focused panel only
  5. both on the same page, the same row: a click on the first changes the setting; the second draws the new value the
     next frame
  6. a page for one panel at a time (Weapon Offsets) open in the first: refused in the second (its page kept)
  7. Back from the second's first page closes it (the first stays open)
  8. New Panel again: on the VR Settings (the first shows a page for one panel at a time), where it was last
  9. both hands at once: the off hand's laser on the second, the main hand's on the first: each panel's row follows its
     own laser
 10. the menu button (Back to game) hides them; opened again (and after a map change): each panel back as left
 11. Close Panel (vr_menu_panels close) still removes one
Prints PASS/FAIL per check; exits 1 on a failure.
"""
import os
import re
import subprocess
import sys

agent = sys.argv[1] if len(sys.argv) > 1 else "bloodfx"
KIT = os.environ.get("KIT", "C:/OHWorkspace/qvr-kit")
BASH = os.environ.get("KIT_BASH", "C:/Program Files/Git/usr/bin/bash.exe")
if not os.path.exists(BASH):
    BASH = "bash"
SHORTCUTS = ["levels", "advanced", "relighting", "checklist", "maps", "search", "console", "recording", "settings"]
H0 = "vr_mock_hand main 0.2 1.2 -0.3 0 0 0;vr_mock_hand off -0.2 1.2 -0.3 0 0 0"
steps = [
    ("open", f"map e1m1;wait60;{H0};menu_vr 3;wait20;vr_menu_panels open;wait20"),
    ("pages", "vr_menu_panels focus 2;menu_vr 0;wait10"),
    ("laser1", "vr_mock_laser 100 40 panel 1;wait10"),
    ("laser2", "vr_mock_laser 100 60 panel 2;wait10"),
    ("key2", "vr_mock_key downarrow;wait5"),
    ("same", "menu_vr 3;wait10;vr_mock_laser 100 20 panel 2;wait5;vr_mock_laser 100 41 panel 2;wait10;"
             "vr_mock_laser 100 20 panel 1;wait5;vr_mock_laser 100 41 panel 1;wait10"),
    ("click1", "vr_mock_button main trigger 1;wait1;vr_mock_button main trigger 0;wait1"),
    ("refuse", "vr_mock_laser off;wait2;vr_menu_panels focus 1;menu_vr 23;wait5;vr_menu_panels focus 2;menu_vr 23;wait5"),
    ("back2", "menu_vr 0;wait3;vr_mock_key escape;wait5"),
    ("reopen", "vr_menu_panels open;wait10"),
    ("hands", "vr_menu_panels focus 1;menu_vr 3;wait5;vr_mock_laser 100 20 panel 2 hand off;vr_mock_laser 100 60 panel 1;wait10"),
    ("hands2", "vr_mock_laser 100 60 panel 2 hand off;wait10"),
    ("pre", "vr_mock_laser off;vr_mock_laser off hand off;vr_menu_panels focus 2;wait5"),
    ("closed", "vr_mock_laser off;vr_mock_laser off hand off;wait2;vr_mock_button main menu 1;wait5;vr_mock_button main menu 0;wait10"),
    ("again", "vr_mock_button main menu 1;wait5;vr_mock_button main menu 0;wait20;vr_menu_panels focus 2;wait5"),
    ("map", "vr_mock_button main menu 1;wait5;vr_mock_button main menu 0;wait10;map e1m2;wait60;"
            "vr_mock_button main menu 1;wait5;vr_mock_button main menu 0;wait20;vr_menu_panels focus 2;wait5"),
    # 13: every corner shortcut that opens a page acts on the panel it is pressed on (the second), the first untouched
    ("sc0", "vr_mock_laser off;vr_mock_laser off hand off;vr_menu_panels focus 2;menu_vr 0;vr_menu_level 2;wait10"),
]
for n in SHORTCUTS:
    steps.append((n, f"vr_mock_laser {n} panel 2;wait10;vr_mock_button main trigger 1;wait1;vr_mock_button main trigger 0;wait5"))
    if n == "levels":
        steps.append(("lvback", "vr_mock_laser off;vr_mock_key escape;wait5"))
steps.append(("close2", "vr_menu_panels close 2;wait5"))
script = ";".join(f"{cmd};echo STEP {name};vr_menu_panels;vr_menu_panel" for name, cmd in steps) + ";toggleconsole;quit"
out = subprocess.run([BASH, f"{KIT}/run.sh", agent, "-Script", script, "-Filter", "STEP|MENUPANEL|one panel at a time|exit="],
                     capture_output=True, text=True).stdout
if os.environ.get("PANELS_RAW"):
    open(os.environ["PANELS_RAW"], "w").write(out)
text = re.sub(r"\s+", " ", out).replace("STEP ", "\nSTEP ")
state = {}
for line in text.splitlines():
    m = re.match(r"STEP (\w+) MENUPANELS open (\d) focused (\d)(.*)", line)
    if not m:
        continue
    panels = {}
    for p in re.finditer(r"MENUPANEL (\d) menu (\d+) page (-?\d+) \"([^\"]*)\" row \"([^\"]*)\" value (\S+) drawn (-?\d+)",
                         m.group(4)):
        panels[int(p.group(1))] = {"menu": int(p.group(2)), "page": int(p.group(3)), "row": p.group(5), "value": float(p.group(6)),
                                   "drawn": int(p.group(7))}
    place = re.search(r"MENUPANEL placed (\d) held \S+ armed \S+ centre \S+ \S+ \S+ from head (\S+) (\S+) (\S+)", m.group(4))
    state.setdefault(m.group(1), {"open": int(m.group(2)), "focused": int(m.group(3)), "panels": panels,
                                  "from": tuple(float(place.group(i)) for i in (2, 3, 4)) if place else None})
fail = 0


def check(ok, what):
    global fail
    print(("PASS " if ok else "FAIL ") + what)
    fail |= 0 if ok else 1


def get(step):
    return state.get(step, {"open": -1, "focused": -1, "panels": {}, "from": None})


def panel(step, i):
    return get(step)["panels"].get(i, {"page": -9, "row": "?", "value": -999.0, "drawn": -9})


for name, _ in steps:
    s = get(name)
    print("  %-7s open %d focused %d | %s" % (name, s["open"], s["focused"], " | ".join(
        "%d: page %d \"%s\" %g (%d)" % (i, p["page"], p["row"], p["value"], p["drawn"]) for i, p in sorted(s["panels"].items()))))
o = get("open")
check(o["open"] == 2 and o["focused"] == 2 and panel("open", 2)["page"] == 3, "1 New Panel: a second panel on the first's page")
check(o["from"] is not None and o["from"][0] > 100.0 and abs(o["from"][1]) < 30.0,
      "1 beside the first, to its right (from the head %s)" % (o["from"],))
check(panel("pages", 1)["page"] == 3 and panel("pages", 2)["page"] == 0, "2 two pages at once (World, VR Settings)")
l1, l2 = get("laser1"), get("laser2")
check(l1["focused"] == 1 and l2["focused"] == 2, "3 the laser focuses the panel it is on")
check(panel("laser2", 1)["row"] == panel("laser1", 1)["row"] and panel("laser2", 2)["row"] != panel("pages", 2)["row"],
      "3 each panel's row follows the laser on it, the other's kept")
check(panel("key2", 2)["row"] != panel("laser2", 2)["row"] and panel("key2", 1)["row"] == panel("laser2", 1)["row"],
      "4 a key moves the focused panel's row only")
sa, c1 = get("same"), get("click1")
check(panel("same", 1)["page"] == panel("same", 2)["page"] == 3 and panel("same", 1)["row"] == panel("same", 2)["row"],
      "5 both on the same page and row")
check(panel("click1", 1)["value"] != panel("same", 1)["value"] and panel("click1", 2)["value"] == panel("click1", 1)["value"]
      and 0 <= panel("click1", 2)["drawn"] <= 1, "5 changed on the first: the second draws the new value the next frame (%g -> %g)"
      % (panel("same", 2)["value"], panel("click1", 2)["value"]))
check(panel("refuse", 1)["page"] == 23 and panel("refuse", 2)["page"] not in (23, -9) and "one panel at a time" in out,
      "6 Weapon Offsets in the first: refused in the second")
b = get("back2")
check(b["open"] == 1 and b["focused"] == 1 and panel("back2", 1)["page"] == 23, "7 Back out of the second closes it only")
r = get("reopen")
check(r["open"] == 2 and panel("reopen", 2)["page"] == 0, "8 New Panel again: on the VR Settings")
check(r["from"] is not None and o["from"] is not None and all(abs(a - c) < 0.5 for a, c in zip(r["from"], o["from"])),
      "8 where it was last")
h, h2 = get("hands"), get("hands2")
check(panel("hands", 1)["row"] != panel("hands", 2)["row"] or panel("hands", 1)["page"] != panel("hands", 2)["page"],
      "9 two lasers, two rows")
check(panel("hands2", 2)["row"] != panel("hands", 2)["row"] and panel("hands2", 1)["row"] == panel("hands", 1)["row"],
      "9 the off hand's laser moves its panel's row, the main hand's panel kept")
check(get("closed")["open"] == 0, "10 the menu button hides every panel (none shown)")
pr, ag, mp = get("pre"), get("again"), get("map")
same = lambda x, y: x["from"] is not None and y["from"] is not None and all(abs(a - c) < 2.0 for a, c in zip(x["from"], y["from"]))
near = lambda x, y: x["from"] is not None and y["from"] is not None and abs(sum(a * a for a in x["from"]) - sum(a * a for a in y["from"])) < 100.0  # (the new map's spawn turns the play space: the same distance round the head)
check(ag["open"] == 2 and panel("again", 1)["page"] == panel("pre", 1)["page"] and panel("again", 2)["page"] == panel("pre", 2)["page"]
      and panel("again", 2)["row"] == panel("pre", 2)["row"], "10 opened again: both panels back on their pages and rows")
check(same(pr, ag), "10 the second panel in the same place (%s -> %s)" % (pr["from"], ag["from"]))
check(mp["open"] == 2 and panel("map", 1)["page"] == panel("pre", 1)["page"] and panel("map", 2)["page"] == panel("pre", 2)["page"]
      and near(pr, mp), "11 after a map change: both panels back, same pages and place (%s)" % (mp["from"],))
base1, base2 = panel("sc0", 1), panel("sc0", 2)
for n in SHORTCUTS:
    p1, p2 = panel(n, 1), panel(n, 2)
    first_kept = p1["page"] == base1["page"] and p1["menu"] == base1["menu"]
    if n == "levels":
        check(p2["menu"] == 5 and first_kept, "13 the Levels shortcut on the second panel: the Levels open there, the first's page kept")
        check(panel("lvback", 2)["page"] == base2["page"] and panel("lvback", 2)["menu"] == 26,
              "13 Back from the Levels in the second panel: its page again")
        continue
    before = panel(SHORTCUTS[SHORTCUTS.index(n) - 1], 2)["page"]
    check(p2["menu"] == 26 and p2["page"] != before and first_kept,
          "13 the %s shortcut on the second panel: its page changed (%d), the first's kept" % (n, p2["page"]))
check(get("close2")["open"] == 1, "12 Close Panel still removes one")
check("exit=0" in out, "the game quit normally")
sys.exit(fail)
