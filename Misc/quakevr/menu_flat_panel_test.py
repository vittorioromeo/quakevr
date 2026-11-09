#!/usr/bin/env python3
"""menu_flat_panel_test.py <agent> -- the VR menu layout on a flat screen (vr_menu_flat_vr_style) and the zoom buttons,
headless (ROUND24.md, "VR menu layout on a flat screen, zoom buttons").
  flat run (vr_enabled 0):
    1. the layout: the corner's buttons a column beside the menu (menu_vr pos "beside"), the panel's rect (MENUZOOM)
    2. the mouse through the panel: a click on the column's VR Settings opens it (vr_mock_mouse settings click)
    3. the zoom buttons: - then + + step vr_menu_flat_scale by 1.05 (0.9 -> 0.857 -> 0.9 -> 0.945), the button staying
       under the mouse; writeconfig has the last value (it persists)
    4. a corner dragged: the bottom right corner pressed, the mouse moved by -120 -60: the panel's middle moves as much;
       the wheel twice while held: smaller round the mouse (the corner still at the mouse); released: no longer dragging
    5. dragged far off the window: its middle stays on it (a corner on the window)
    6. the dialogs: Quit's message and a confirmation (vr_test_confirm) centred on the panel at the menu's scale (menu_vr pos
       dialog lines), Escape on Quit's back to the main menu; the keys still move the selection (vr_mock_key downarrow)
    7. screenshots at three sizes (0.9 middle, 0.55 off-centre, 1.0) -> scratch/flatmenu_flat.png
  VR run (the mock headset):
    8. the column and zoom buttons drawn (MENUZOOM buttons shown), vr_menu_scale 0.25 as shipped, the panel's size
    9. the laser on + and the trigger: vr_menu_scale 0.275, the panel 1.1 times as tall; on -: back to 0.25
   10. vr_menu_zoom reset vr: 0.25; screenshots at 0.25, 0.2, 0.3 and Quit's -> scratch/flatmenu_vr.png; Quit still the headset's
       dialog (M_Confirm)
Waits are counted frames; each run ends on its "MENUFLAT done" line. Prints PASS/FAIL per check and
"menu flat panel: N passed, M failed"; exits 1 on a failure.
"""
import os
import re
import subprocess
import sys

agent = sys.argv[1] if len(sys.argv) > 1 else "flatwall"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvrtest  # noqa: E402  (the tests' library: Misc/quakevr/qvrtest.py)
KIT = qvrtest.KIT
BASH = os.environ.get("KIT_BASH", "C:/Program Files/Git/usr/bin/bash.exe")
if not os.path.exists(BASH):
    BASH = "bash"
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

FILTER = "STEP|MENUZOOM|menu_vr pos: text from|menu_vr pos: menu|menu_vr pos: dialog|vr_mock_mouse|vr_mock_key|vr_menu_zoom|MENUPANEL|MENUFLAT|exit=|TIMEOUT"

flat = ";".join([
    "vr_enabled 0", "wait5", "map start", "wait30", "vr_menu_flat_vr_style 1", "vr_menu_flat_scale 0.9", "vr_menu_flat_y 0.5", "menu_main", "wait5",
    "echo STEP layout", "menu_vr pos", "vr_menu_zoom",
    "echo STEP click", "vr_mock_mouse settings click", "wait3", "menu_vr pos",
    "echo STEP zoomout", "vr_mock_mouse zoomout click", "wait3", "vr_menu_zoom",
    "echo STEP zoomin", "vr_mock_mouse zoomin click", "wait3", "vr_mock_mouse zoomin click", "wait3", "vr_menu_zoom",
    "writeconfig flatzoom_test",
    "echo STEP drag", "vr_menu_zoom reset flat", "wait3", "vr_menu_zoom", "vr_mock_mouse corner br", "vr_mock_mouse press",
    "vr_mock_mouse by -120 -60", "wait1", "vr_menu_zoom", "vr_mock_mouse wheel down", "vr_mock_mouse wheel down", "wait1",
    "vr_menu_zoom", "vr_mock_mouse by 0 0", "vr_mock_mouse release", "wait2", "vr_menu_zoom",
    "echo STEP clamp", "vr_mock_mouse corner tl", "vr_mock_mouse press", "vr_mock_mouse by -5000 -5000", "vr_mock_mouse release",
    "wait2", "vr_menu_zoom",
    "echo STEP dialogs", "vr_menu_zoom reset flat", "menu_main", "wait3", "menu_quit", "wait3", "menu_vr pos",
    "vr_mock_key escape", "wait2", "menu_vr pos", "vr_test_confirm", "wait3", "menu_vr pos", "vr_mock_key escape", "wait2",
    "echo STEP keys", "vr_menu_zoom reset flat", "menu_main", "wait3", "menu_vr pos", "vr_mock_key downarrow", "wait2",
    "menu_vr pos", "vr_mock_key downarrow", "wait2", "menu_vr pos", "vr_mock_mouse settings click", "wait3",
    "echo STEP shots", "wait3", "screenshot", "wait2", "vr_menu_flat_scale 0.55", "vr_menu_flat_x 0.36", "vr_menu_flat_y 0.4",
    "wait3", "screenshot", "wait2", "vr_menu_flat_scale 1", "vr_menu_flat_x 0.5", "vr_menu_flat_y 0.5", "wait3", "screenshot",
    "wait2", "vr_menu_zoom reset flat",
    "echo STEP classic", "vr_menu_flat_vr_style 0", "menu_main", "wait3", "menu_vr pos", "vr_mock_mouse zoomout click",
    "wait3", "vr_menu_zoom", "vr_menu_zoom reset classic",
    "echo MENUFLAT done", "toggleconsole", "quit"])

vr = ";".join([
    "map start", "wait30", "menu_main", "wait5",
    "echo STEP vrlayout", "menu_vr pos", "vr_menu_zoom", "vr_menu_panel", "screenshot", "wait2",
    "echo STEP vrzoomin", "vr_mock_laser zoomin", "wait3", "vr_mock_button main trigger 1", "wait3",
    "vr_mock_button main trigger 0", "wait3", "vr_mock_laser off", "wait3", "vr_menu_zoom", "vr_menu_panel",
    "echo STEP vrzoomout", "vr_mock_laser zoomout", "wait3", "vr_mock_button main trigger 1", "wait3",
    "vr_mock_button main trigger 0", "wait3", "vr_mock_laser off", "wait3", "vr_menu_zoom", "vr_menu_panel",
    "echo STEP vrreset", "vr_menu_zoom in", "vr_menu_zoom reset vr", "vr_menu_zoom",
    "echo STEP vrshots", "vr_menu_scale 0.2", "wait3", "screenshot", "wait2", "vr_menu_scale 0.3", "wait3", "screenshot", "wait2",
    "vr_menu_zoom reset vr",
    "echo STEP vrdialogs", "menu_main", "wait3", "menu_quit", "wait3", "menu_vr pos", "screenshot", "wait2", "vr_mock_key escape", "wait2",
    "echo MENUFLAT done", "toggleconsole", "quit"])


def run(script, out, extra):
    args = [BASH, f"{KIT}/run.sh", agent, "-Script", script, "-QuitOn", "MENUFLAT done", "-Timeout", "240",
            "-Filter", FILTER, "-Out", out, "-Clean"] + extra
    return subprocess.run(args, capture_output=True, text=True).stdout


out_flat = run(flat, "flatmenu_flat.png", [])
out_vr = run(vr, "flatmenu_vr.png", [])


def lines_of(out):
    text = re.sub(r"\s+", " ", out)
    for k in ["STEP ", "MENUZOOM", "menu_vr pos", "vr_mock_mouse:", "vr_mock_key:", "vr_menu_zoom:", "MENUPANEL",
              "MENUFLAT", "exit=", "TIMEOUT"]:
        text = text.replace(k, "\n" + k)
    return [l.strip() for l in text.splitlines() if l.strip()]


fl = lines_of(out_flat)
vl = lines_of(out_vr)
if os.environ.get("VERBOSE"):
    print("\n".join(fl + ["----"] + vl))
fail = 0
passed = 0


def check(ok, what):
    global fail, passed
    print(("PASS " if ok else "FAIL ") + what)
    fail += 0 if ok else 1
    passed += 1 if ok else 0


def after(lines, name):
    i = next((n for n, l in enumerate(lines) if l == f"STEP {name}"), len(lines))
    rest = lines[i + 1:]
    j = next((n for n, l in enumerate(rest) if l.startswith("STEP ")), len(rest))
    return rest[:j]


def flatzoom(lines):
    """[(scale, x, y, rect(x, y, w, h), dragging)] from the MENUZOOM flat lines."""
    out = []
    for l in lines:
        m = re.match(r"MENUZOOM flat: (\w+), vr_menu_flat_scale ([\d.]+) .*?middle ([\d.]+) ([\d.]+), rect (-?\d+) (-?\d+) "
                     r"(\d+) x (\d+) of (\d+) x (\d+), (\w+ ?\w*),", l)
        if m:
            g = m.groups()
            out.append((float(g[1]), float(g[2]), float(g[3]), tuple(int(v) for v in g[4:8]), g[10] == "dragging",
                        (int(g[8]), int(g[9]))))
    return out


def vrscale(lines):
    return [float(m.group(1)) for m in (re.match(r"MENUZOOM vr: vr_menu_scale ([\d.]+)", l) for l in lines) if m]


def panel_height(lines):
    hs = []
    for l in lines:
        m = re.search(r"MENUPANEL.*?size ([\d.]+) x ([\d.]+)", l)
        if m:
            hs.append(float(m.group(2)))
    return hs


# 1
lay = after(fl, "layout")
pos = next((l for l in lay if l.startswith("menu_vr pos: text from")), "")
check("(beside)" in pos, f"flat: the column beside the menu: {pos[13:]}")
z = flatzoom(lay)
check(bool(z) and z[0][0] == 0.9 and z[0][3][2] == round(z[0][5][0] * 0.9), f"flat: panel 0.9 of the window: {z[:1]}")
check(any("MENUZOOM buttons: flat, shown yes" in l for l in lay), "flat: zoom buttons drawn, stepping the flat panel")
# 2
click = after(fl, "click")
check(any(re.match(r"vr_mock_mouse: clicked, menu \d+$", l) for l in click) and
      any("rows from y" in l or "menu_vr pos: text from" in l for l in click), "flat: the mouse clicked VR Settings through the panel")
m = next((re.match(r"vr_mock_mouse: at (-?[\d.]+) (-?[\d.]+), menu", l) for l in click if l.startswith("vr_mock_mouse: at")), None)
check(m is not None, f"flat: the mouse landed on the button's spot: {m.group(0) if m else 'none'}")
# 3
zo = flatzoom(after(fl, "zoomout"))
zi = flatzoom(after(fl, "zoomin"))
check(bool(zo) and abs(zo[-1][0] - 0.857) < 0.002, f"flat: - steps 0.9 to {zo[-1][0] if zo else None}")
check(bool(zi) and abs(zi[-1][0] - 0.945) < 0.002, f"flat: + + steps on to {zi[-1][0] if zi else None}")
cfg = os.path.join(ROOT, "quakevr", "flatzoom_test.cfg")
saved = ""
if os.path.exists(cfg):
    saved = next((l.strip() for l in open(cfg, encoding="latin-1") if l.startswith("vr_menu_flat_scale")), "")
    os.makedirs(os.path.join(ROOT, "scratch"), exist_ok=True)
    os.replace(cfg, os.path.join(ROOT, "scratch", "flatzoom_test.cfg"))
check("0.945" in saved, f"flat: the zoom persists in the config: {saved}")
# 4
d = flatzoom(after(fl, "drag"))
ok = len(d) >= 4
if ok:
    start, moved, wheeled, released = d[-4:]
    (sx, sy, sw, sh), (mx, my) = start[3], moved[3][:2]
    dx, dy = mx - sx, my - sy
    check(abs(dx + 120) <= 1 and abs(dy + 60) <= 1 and moved[4], f"flat: corner drag moved the panel by {dx} {dy}")
    wx, wy, ww, wh = wheeled[3]
    # (after vr_menu_zoom reset flat: from vr_menu_flat_scale's default, 0.954)
    check(abs(wheeled[0] - round(0.954 / 1.1025, 3)) < 0.003, f"flat: wheel twice while dragging: size {wheeled[0]}")
    # the bottom right corner stayed by the mouse: the mouse 7 px outside it (vr_mock_mouse corner), that gap scaled with
    # the panel
    mouse = (sx + sw + 7 - 120, sy + sh + 7 - 60)
    gap = 7 * wheeled[0] / start[0]
    check(abs(wx + ww + gap - mouse[0]) <= 2 and abs(wy + wh + gap - mouse[1]) <= 2,
          f"flat: resized round the grab point: corner {wx + ww} {wy + wh}, mouse {mouse}, gap {gap:.1f}")
    check(not released[4] and released[3] == wheeled[3], "flat: released: not dragging, the panel stays")
else:
    check(False, f"flat: drag steps printed ({len(d)})")
c = flatzoom(after(fl, "clamp"))
if c:
    (x, y, w, h), (W, H) = c[-1][3], c[-1][5]
    check(c[-1][1] == 0.0 and c[-1][2] == 0.0 and x + w > 0 and y + h > 0, f"flat: dragged off, a corner stays on: {c[-1][3]}")
else:
    check(False, "flat: clamp step printed")
# 6b: the dialogs over the flat panel (Quit's message, M_Confirm's) centred on it, at the menu's scale (ROUND24.md, "The
# flat VR menu's Quit dialog")
def dialogs(lines):
    """[(box middle x, y, px per unit, area middle x, y, the menu's px per unit)] from the menu_vr pos dialog lines."""
    out, box = [], None
    for l in lines:
        m = re.match(r"menu_vr pos: dialog box .*?\(middle ([\d.]+) ([\d.]+)\), ([\d.]+) px per unit", l)
        if m:
            box = [float(v) for v in m.groups()]
        m = re.match(r"menu_vr pos: dialog area: .*?\(middle ([\d.]+) ([\d.]+)\), the menu's ([\d.]+) px per unit", l)
        if m and box:
            out.append(tuple(box + [float(v) for v in m.groups()]))
            box = None
    return out


dl = after(fl, "dialogs")
dg = dialogs(dl)
menus = [l for l in dl if l.startswith("menu_vr pos: menu")]
for i, what in enumerate(("Quit's message", "a confirmation (M_Confirm)")):
    if i < len(dg):
        bx, by, bu, ax, ay, au = dg[i]
        # Quit's box is Quake's, 4 units below the middle (y 76..132 of 200)
        check(abs(bx - ax) <= 2 and abs(by - ay) <= 8 * au and abs(bu - au) < 0.02,
              f"flat: {what} centred on the panel at its scale: box {bx} {by} ({bu} px/unit), panel {ax} {ay} ({au})")
    else:
        check(False, f"flat: {what}: dialog box printed")
check(len(menus) >= 2 and menus[0].startswith("menu_vr pos: menu 21") and menus[1].startswith("menu_vr pos: menu 1 "),
      f"flat: Escape on Quit's message back to the main menu: {[m[13:30] for m in menus[:2]]}")
vd = after(vl, "vrdialogs")
check(any(l.startswith("menu_vr pos: menu 28") for l in vd) and any("dialog area: the headset's canvas" in l for l in vd),
      "vr: Quit asks in the headset's dialog (M_Confirm), as before")
# 6
keys =[l for l in after(fl, "keys") if l.startswith("menu_vr pos: menu")]
check(len(keys) == 3 and keys[0] != keys[1] != keys[2], f"flat: the arrow keys move the selection: {[k[13:60] for k in keys]}")
cl = after(fl, "classic")
cpos = next((l for l in cl if l.startswith("menu_vr pos: text from")), "")
cz = next((re.match(r"MENUZOOM classic: scr_menuscale ([\d.]+) \(1 to ([\d.]+)\)", l) for l in cl
           if l.startswith("MENUZOOM classic")), None)
check("(over)" in cpos and cz is not None and abs(float(cz.group(1)) - round(float(cz.group(2)) / 1.1, 3)) < 0.006,
      f"classic flat row: - steps scr_menuscale from what the canvas fits: {cz.group(0) if cz else None}")
# 8
vlay = after(vl, "vrlayout")
vpos = next((l for l in vlay if l.startswith("menu_vr pos: text from")), "")
check("(beside)" in vpos, f"vr: column beside the menu: {vpos[13:]}")
check(vrscale(vlay)[:1] == [0.25], f"vr: vr_menu_scale as shipped: {vrscale(vlay)[:1]}")
check(any("MENUZOOM buttons: vr, shown yes" in l for l in vlay), "vr: zoom buttons drawn")
h0 = panel_height(vlay)
# 9
s_in, h_in = vrscale(after(vl, "vrzoomin")), panel_height(after(vl, "vrzoomin"))
s_out = vrscale(after(vl, "vrzoomout"))
check(s_in[-1:] == [0.275], f"vr: the laser on + : vr_menu_scale {s_in[-1:]}")
check(bool(h0 and h_in) and abs(h_in[-1] / h0[0] - 1.1) < 0.01, f"vr: the panel 1.1 times as tall: {h0[:1]} -> {h_in[-1:]}")
check(s_out[-1:] == [0.25], f"vr: the laser on - : back to {s_out[-1:]}")
check(vrscale(after(vl, "vrreset"))[-1:] == [0.25], "vr: vr_menu_zoom reset vr")
for name, lines in (("flat", fl), ("vr", vl)):
    check(any(l.startswith("MENUFLAT done") for l in lines) and not any(l.startswith("TIMEOUT") for l in lines),
          f"{name} run ended on its verdict line")
print(f"menu flat panel: {passed} passed, {fail} failed")
sys.exit(1 if fail else 0)
