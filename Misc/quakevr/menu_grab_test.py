#!/usr/bin/env python3
"""menu_grab_test.py <agent> -- the VR menu panel moved by a grip (vr_menu_grab; MULTI_MENU.md stage 1; ROUND22.md,
"Moving the menu panel"), headless with the mock headset on e1m1, read from vr_menu_panel's MENUPANEL lines:
  1. opened: in front of the head, not moved
  2. the laser on its frame (vr_mock_laser frame), the main grip pressed: held at once, the grip's key not sent
  3. the hand turned 30 degrees left: the panel turns with it (facing yaw +30) and swings round the hand
  4. the hand moved 30 cm right: the panel moves with it (its middle about 9.5 units right)
  5. let go: it stays, also with the hand back where it was
  6. on its face (vr_mock_laser 160 100), gripped: armed, not held; moved 2 cm: still armed; 10 cm: held, not jumped
  7. on its face, a stick pushed first: never held (the sliders' fine modifier)
  8. vr_menu_grab 0: the frame does nothing
  9. moved, the menus closed and opened again: stays where it was put (ROUND22.md, "Menu panels kept when the menu
     closes": his note, a moved panel stays placed); unplaced (vr_menu_panel reset), closed and opened: in front of the head
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
H0 = "vr_mock_hand main 0.2 1.2 -0.3 0 0 0"
G1 = "vr_mock_button main grip 1;wait5"
G0 = "vr_mock_button main grip 0;wait5"
steps = [
    ("opened", f"map e1m1;wait60;{H0};menu_vr 0;wait20"),
    ("frame", f"vr_debug_buttons 1;vr_mock_laser frame;wait5;{G1}"),
    ("turned", "vr_mock_hand main 0.2 1.2 -0.3 0 30 0;wait10"),
    ("moved", "vr_mock_hand main 0.5 1.2 -0.3 0 30 0;wait10"),
    ("letgo", G0),
    ("handback", f"vr_mock_laser off;{H0};wait20;vr_debug_buttons 0"),
    ("face", f"vr_menu_panel reset;wait5;vr_mock_laser 160 100;wait5;{G1}"),
    ("face2cm", "vr_mock_hand main 0.22 1.2 -0.3 0 0 0;wait5"),
    ("face10cm", "vr_mock_hand main 0.3 1.2 -0.3 0 0 0;wait5"),
    ("stick", f"{G0};{H0};vr_menu_panel reset;wait5;{G1};vr_mock_stick off 0 -1;wait5;vr_mock_stick off 0 0;wait5;"
              "vr_mock_hand main 0.3 1.2 -0.3 0 0 0;wait5"),
    ("off", f"{G0};{H0};vr_menu_grab 0;vr_mock_laser frame;wait5;{G1}"),
    ("placed", f"{G0};vr_menu_grab 2;{G1};vr_mock_hand main 0.2 1.2 -0.3 0 30 0;wait5;{G0}"),
    ("reopened", "vr_mock_laser off;vr_mock_button main menu 1;wait90;vr_mock_button main menu 0;wait30;menu_vr 0;wait20"),
    ("unplaced", "vr_menu_panel reset;vr_mock_button main menu 1;wait90;vr_mock_button main menu 0;wait30;menu_vr 0;wait20"),
]
script = ";".join(f"{cmd};echo STEP {name};vr_menu_panel" for name, cmd in steps) + ";toggleconsole;quit"
out = subprocess.run([BASH, f"{KIT}/run.sh", agent, "-Script", script, "-Filter", "STEP|MENUPANEL|grab|exit="],
                     capture_output=True, text=True).stdout
text = re.sub(r"\s+", " ", out).replace("STEP ", "\nSTEP ")
panel = {}
for line in text.splitlines():
    m = re.match(r"STEP (\w+) MENUPANEL placed (\d) held (-?\d) armed (-?\d) centre (\S+) (\S+) (\S+) from head \S+ \S+ \S+ "
                 r"facing yaw (\S+) pitch (\S+)", line)
    if m:
        panel[m.group(1)] = {"placed": int(m.group(2)), "held": int(m.group(3)), "armed": int(m.group(4)),
                             "centre": tuple(float(m.group(i)) for i in (5, 6, 7)), "yaw": float(m.group(8))}
fail = 0


def check(ok, what):
    global fail
    print(("PASS " if ok else "FAIL ") + what)
    fail |= 0 if ok else 1


def get(step):
    return panel.get(step, {"placed": -9, "held": -9, "armed": -9, "centre": (0.0, 0.0, 0.0), "yaw": 0.0})


for name, _ in steps:
    p = get(name)
    print("  %-9s placed %d held %d armed %d centre %.1f %.1f %.1f yaw %.1f" % ((name, p["placed"], p["held"], p["armed"]) +
                                                                            p["centre"] + (p["yaw"],)))
o, f, t, mv, lg, hb = (get(s) for s in ("opened", "frame", "turned", "moved", "letgo", "handback"))
check(o["placed"] == 0 and o["held"] == -1, "1 opened in front of the head, not moved")
check(f["held"] == 1 and f["placed"] == 1 and f["centre"] == o["centre"], "2 the frame gripped: held at once, not moved yet")
check("taken by the menu panel's grab" in out, "2 the grip's key not sent (taken by the grab)")
check(abs(t["yaw"] - o["yaw"] - 30.0) < 0.5 and t["centre"] != o["centre"], "3 turned with the hand (30 degrees)")
dx = [b - a for a, b in zip(t["centre"], mv["centre"])]
check(8.0 < (dx[0] ** 2 + dx[1] ** 2) ** 0.5 < 11.0 and abs(dx[2]) < 0.1, "4 moved with the hand (30 cm: %.1f units)" %
      (dx[0] ** 2 + dx[1] ** 2) ** 0.5)
check(lg["held"] == -1 and lg["placed"] == 1 and lg["centre"] == mv["centre"], "5 let go: stays")
check(hb["placed"] == 1 and hb["centre"] == mv["centre"], "5 stays with the hand back")
fa, f2, f10 = get("face"), get("face2cm"), get("face10cm")
check(fa["armed"] == 1 and fa["held"] == -1 and f2["armed"] == 1 and f2["placed"] == 0, "6 the face: armed, not held within 4 cm")
check(f10["held"] == 1 and f10["armed"] == -1 and f10["centre"] == o["centre"], "6 held after 10 cm, not jumped")
st = get("stick")
check(st["held"] == -1 and st["placed"] == 0 and st["armed"] == -1, "7 a stick pushed: never held")
check(get("off")["held"] == -1 and get("off")["placed"] == 0, "8 vr_menu_grab 0: the frame does nothing")
pl, ro, up = get("placed"), get("reopened"), get("unplaced")
check(pl["placed"] == 1 and ro["placed"] == 1 and ro["centre"] == pl["centre"],
      "9 closed and opened again: stays where it was put")
check(up["placed"] == 0 and up["centre"] == o["centre"], "9 unplaced, closed and opened again: back in front of the head")
check("exit=0" in out, "the game quit normally")
sys.exit(fail)
