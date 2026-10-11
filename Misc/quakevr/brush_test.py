# brush_test.py <agent> [--shots] [--perf] -- runtime brushes and the toolgun's Brush tool, headless (docs/vr-port/
# BRUSH_TOOL.md; ROUND22.md, "Runtime brushes: the toolgun's Brush tool"). vrfiringrange unless said (its start: 316 -556,
# facing -x, the floor at 32 there and 17 further on):
#   selftest  vr_brush_selftest: boxes, a wedge and a prism as .map text and back, their hulls at points (PASS);
#   stand     the player put over a box lands on it (its entity his ground); the box removed, he falls to the floor;
#   climb     a box 48 high, its face 18 ahead: the mantle play (both hands on its lip, pulled down) mantles onto it;
#   monster   a platform in the air: a grunt at one end comes at the player at the other, along it, standing on it;
#   crate     a crate let fall onto a box rests on it; the box removed, it falls to the floor;
#   rocket    a rocket fired at a box: its scorch on the box (vr_brush_list's decals); removed with it;
#   sight     a grunt behind a wall of box doesn't come; the wall removed, it does (it sees the player);
#   tool      the Brush tool with the mock hand: a box made by dragging its base and raising it, moved, resized (the
#             top's handle), painted, copied (the ghost stamped), the copy deleted; a wedge made the same way, turned 45
#             degrees by the hand (Rotate), the turn undone;
#   save      a save with a box and a turned, painted wedge loads with them as they were (ids, revisions, bounds), traces
#             stop on them; a new box after is 3;
#   shapes    the player stands on a wedge's slope (z 77, vr_hull_width 16: the bevels), a box trace stops 8 off a turned
#             box's corner;
#             undo takes back a move, a turn and a removal;
#   files     vr_brush_save / vr_brush_load round trip; a new map with vr_brush_autoload 1 gets its brush file's;
#   light     (with --shots) a pillar lit by a dynamic light, its shadow behind it, then without it.
# --shots: scratch/brush_shots.png (the tool's ghost, the scorch, the dynamic light's shadow, the panel).
# --perf: 50 pillars: what making them takes, the frames before and after (--exclusive: timings).
# PASS: every check as said, no crash.
import os, re, subprocess, sys

AGENT = sys.argv[1] if len(sys.argv) > 1 else "brushtool"
SHOTS = "--shots" in sys.argv
PERF = "--perf" in sys.argv
KIT = os.environ.get("KIT", "C:/OHWorkspace/qvr-kit")
BASH = os.environ.get("KIT_BASH", "C:/Program Files/Git/usr/bin/bash.exe")
if not os.path.exists(BASH):
    BASH = "bash"
SCRATCH = f"C:/OHWorkspace/qvr-agents/{AGENT}/scratch"
os.makedirs(SCRATCH, exist_ok=True)
FILT = "brush|box3d: .*(convex pieces|retired)|decals laid|lets go|^  [0-9]+: entity|toolgun|climb: mantle|saved game|walltime|^ *(frame|render|world) |ENGINE|CRASH|exit=|TIMEOUT|composed"
fails = []

def run(cmds, out=None, extra=()):
    args = [BASH, KIT + "/run.sh", *extra, AGENT, "-Script", ";".join(cmds + ["toggleconsole", "quit"]), "-Filter", FILT,
            "-Clean", "-Timeout", "300"]
    if out:
        args += ["-Out", out]
    p = subprocess.run(args, capture_output=True, text=True, timeout=900)
    t = p.stdout + p.stderr
    if "ENGINE" in t or "CRASH" in t or "TIMEOUT" in t:
        fails.append("engine error, crash or timeout")
    return t

def ents(t, num="last"):
    # (origin z, ground brush id or 0) of each vr_brush_ent line
    out = []
    for m in re.finditer(r"brush ent: (\d+) (\S+) at (-?[\d.]+) (-?[\d.]+) (-?[\d.]+), (on ground|in the air), ground (-?\d+)( \(brush (\d+)\))?, health -?\d+, enemy (\d+)", t):
        out.append((float(m.group(3)), float(m.group(4)), float(m.group(5)), int(m.group(9) or 0), m.group(2), int(m.group(10))))
    return out

def check(ok, what):
    print(("ok   " if ok else "FAIL ") + what)
    if not ok:
        fails.append(what)

def hand(pitch, yaw=0):
    return f"vr_mock_hand main 0.0 1.5 -0.3 {pitch} {yaw} 0;wait15"

def trig(v, w=3):
    return f"vr_mock_button main trigger {v};wait{w}"

def mantle_play(path, hy=1.49, fwd=-0.69, extra=(), pull_at=1.0):
    # both hands onto a lip at hy (tracking metres), pulled down and in from pull_at over 1 s (a mantle), let go 0.6 s
    # after; `extra`: more lines (commands at their times)
    L = []
    for h, x in (("off", -0.12), ("main", 0.12)):
        L += [f"0.000 {h} {x} 1.2 -0.3 0 0 0", f"0.500 {h} {x} {hy} {fwd} 0 0 0", f"{pull_at:.3f} {h} {x} {hy} {fwd} 0 0 0",
              f"{pull_at + 1:.3f} {h} {x} {hy - 0.8} {fwd + 0.2} 0 0 0", f"{pull_at + 2:.3f} {h} {x} {hy - 0.8} {fwd + 0.2} 0 0 0"]
    L += ["0.700 cmd +graboff", "0.750 cmd +grabmain", f"{pull_at + 1.6:.3f} cmd -graboff", f"{pull_at + 1.6:.3f} cmd -grabmain"]
    L += list(extra)
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(L) + "\n")

R = ["map vrfiringrange", "wait60", "god", "notarget"]

# selftest, stand, crate, save (one run)
t = run(R + ["vr_brush_selftest",
             "vr_brush_here 64 48 \"\" 160", "wait5", "setpos 160 -560 120 0 90 0", "noclip", "wait60", "vr_brush_ent 1",
             "vr_brush_delete 1", "wait60", "vr_brush_ent 1",
             "vr_brush_box 128 -592 17 192 -528 81", "wait5", "vr_brush_spawn vr_crate 160 -560 200 0 drop", "wait144",
             "vr_brush_ent last", "vr_brush_delete all", "wait144", "vr_brush_ent last",
             "vr_brush_box 128 -592 17 192 -528 81", "vr_brush_shape wedge 200 -700 17 300 -640 60", "vr_brush_rotate 2 0 30 0",
             "vr_brush_texture 2 city6_4 1", "wait5", "vr_brush_list", "save brush_test", "wait5", "vr_brush_delete all",
             "load brush_test", "wait60", "vr_brush_list", "vr_brush_trace 160 -560 140 160 -560 12",
             "vr_brush_trace 250 -670 140 250 -670 0", "vr_brush_box 300 -592 17 340 -528 81", "wait5", "vr_brush_list"])
check("brush selftest: PASS" in t, "selftest")
e = ents(t)
check(len(e) >= 4 and e[0][3] == 1 and abs(e[0][2] - 89) < 1.5, f"stand: on the box at z 89 ({e[0] if e else None})")
check(len(e) >= 4 and e[1][3] == 0 and e[1][2] < 50, f"stand: the box removed, on the floor ({e[1] if len(e) > 1 else None})")
check(len(e) >= 4 and e[2][3] > 0 and abs(e[2][2] - 97) < 2, f"crate: resting on the box ({e[2] if len(e) > 2 else None})")
check(len(e) >= 4 and e[3][3] == 0 and e[3][2] < 40, f"crate: the box removed, on the floor ({e[3] if len(e) > 3 else None})")
# the save keeps the box and the turned wedge (its id, revision, faces, bounds and painted face), and a new one gets id 3
lists = [re.findall(r"^  (\d+): entity \d+, rev (\d+), (\d+) faces, (\S+), ([-\d. ]+), \d+ decals", b, re.M)
         for b in re.split(r"brushes: \d+ of", t)[1:]]
before = [x for x in lists if len(x) == 2]
check("brush: 2 brushes of the saved game made again" in t and len(before) >= 2 and before[0] == before[1] and before[0][1][2] == "5",
      f"save: loaded with its box and its turned wedge, as they were ({before[:2]})")
traces = re.findall(r"brush trace: fraction [\d.]+, end [-\d.]+ [-\d.]+ ([-\d.]+),.*?entity (-?\d+)( \(brush (\d+)\))?", t)
check(len(traces) == 2 and abs(float(traces[0][0]) - 81) < 1 and traces[0][3] == "1" and traces[1][3] == "2",
      f"save: the loaded box and wedge stop traces ({traces})")
check(len(lists) >= 3 and [x[0] for x in lists[-1]] == ["1", "2", "3"], f"save: a new box after the load is 3 ({lists[-1] if lists else None})")

# shapes: the player put over a wedge stands on its slope (the bevels: not held up by its edge), a turned box's corner
# meets a box trace where it is; undo takes edits back
t = run(R + ["vr_brush_shape wedge 128 -592 17 256 -528 81", "wait5", "setpos 192 -560 150 0 90 0", "noclip", "wait90", "vr_brush_ent 1",
             "vr_brush_box 96 -700 17 160 -636 81", "vr_brush_rotate 2 0 45 0", "wait5",
             "vr_brush_trace 200 -668 60 100 -668 60 32", "vr_brush_move 2 0 0 8", "vr_brush_undo", "vr_brush_undo",
             "vr_brush_delete 1", "vr_brush_undo", "vr_brush_list"])
e = ents(t)
# (he may slide a little down it: his height is the slope's at his box's higher edge, x + 8, plus 24)
check(len(e) == 1 and e[0][3] == 1 and abs(e[0][2] - (17 + (e[0][0] + 8 - 128) * 0.5 + 24)) < 1.5, f"wedge: the player stands on its slope ({e})")
tr = re.findall(r"brush trace: fraction [\d.]+, end ([-\d.]+) ", t)
check(len(tr) == 1 and abs(float(tr[0]) - (128 + 45.25 + 8 + 0.03)) < 0.6, f"turned box: the player's box (vr_hull_width 16) stops 8 off its corner ({tr})")
check(t.count("undone: 2 changed (as it was)") == 2 and "undone: 1 removed (back as 1)" in t and
      re.search(r"^  2: entity \d+, rev \d+, 6 faces, \S+, 96 -700 17 \.\. 160 -636 81", t, re.M) is not None,
      "undo: a move and a turn taken back (the box as made), a removal undone")

# brush files: saved, removed, loaded (vr_brush_load); a new map with vr_brush_autoload 1 gets them
t = run(R + ["vr_brush_box 128 -592 17 192 -528 81", "vr_brush_shape cylinder 200 -700 17 264 -636 81 6", "vr_brush_save brush_test",
             "vr_brush_delete all", "vr_brush_load brush_test", "vr_brush_list", "vr_brush_save", "vr_brush_autoload 1",
             "map vrfiringrange", "wait60", "vr_brush_list", "vr_brush_autoload 0"])
counts = re.findall(r"brushes: (\d+) of", t)
check("2 brushes made from" in t and counts == ["2", "2"] and re.search(r"^  2: entity \d+, rev 1, 8 faces", t, re.M) is not None,
      f"files: saved and loaded, a new map with vr_brush_autoload gets them ({counts})")

# climb
p = SCRATCH + "/brush_mantle.txt"
mantle_play(p)
t = run(["vr_climb 1"] + R + ["vr_climb_debug 1", "vr_brush_box 202 -620 32 298 -492 80", "wait35", "vr_mock_play " + p,
                              "wait300", "vr_brush_ent 1"])
e = ents(t)
check("climb: mantled onto" in t and e and e[-1][3] == 1 and abs(e[-1][2] - 104) < 2, f"climb: mantled onto the box ({e[-1] if e else None})")

# a box made (its ledge map made: Show Ledges) and moved far off while the hands hold vrclimb's long ledge, then the
# mantle over it (the holds kept through the box's edit: ledges::alive; the edit drops only its own map)
p = SCRATCH + "/brush_mantle_edit.txt"
mantle_play(p, hy=1.51, extra=["0.900 cmd vr_brush_box -400 -400 0 -380 -380 40", "1.000 cmd vr_debug_ledges 1",
                               "1.200 cmd vr_debug_ledges 0", "1.300 cmd vr_brush_move 1 0 8 0"], pull_at=1.5)
t = run(["vr_climb 1", "map vrclimb", "wait30", "god", "notarget", "vr_climb_debug 1", "setpos 78 176 24 0 0 0", "noclip",
         "wait20", "vr_mock_play " + p, "wait400"])
check("brush: 1 moved 0 8 0" in t and "climb: mantled onto" in t and "lets go" not in t.split("brush: 1 moved")[-1].split("mantled")[0],
      "edit: a box moved while the hands hold the world's ledge: still held, mantled onto it")

# monster on a platform; sight through a wall
t = run(["map vrfiringrange", "wait60", "god", "vr_brush_box 0 -604 200 320 -508 216", "wait5", "setpos 296 -556 242 0 180 0",
         "noclip", "wait10", "vr_brush_spawn monster_army 32 -556 240 0", "wait5", "vr_brush_ent last", "wait288", "vr_brush_ent last"])
e = ents(t)
check(len(e) == 2 and e[0][3] == 1 and e[1][3] == 1 and abs(e[1][2] - 240) < 1 and e[1][0] - e[0][0] > 100,
      f"monster: a grunt walked along the box, on it ({e})")
t = run(["map vrfiringrange", "wait60", "god", "vr_brush_box 200 -700 16 216 -412 200", "wait5",
         "vr_brush_spawn monster_army 100 -556 50 0", "wait300", "vr_brush_ent last", "vr_brush_delete 1", "wait300", "vr_brush_ent last"])
e = ents(t)
check(len(e) == 2 and e[0][5] == 0 and e[1][5] == 1 and e[1][0] > 230, f"sight: no enemy through the wall; the wall gone, the player and comes ({e})")

# rocket
t = run(R + ["vr_weapon_grip_mode 1", "vr_brush_box 156 -620 32 188 -492 160", "impulse 9", "wait2", "impulse 160", "wait30",
             "vr_mock_hand main 0.08 1.2 -0.3 70 0 0", "wait30", "+attack", "wait10", "-attack", "wait60", "vr_brush_list",
             "vr_debug_box3d 1", "vr_brush_texture 1 city6_4", "wait5", "vr_brush_list", "vr_brush_move 1 0 8 0", "wait5",
             "vr_brush_list", "vr_brush_move 1 0 -8 0", "wait5", "vr_debug_box3d 0",
             "vr_brush_delete 1", "wait5", "vr_brush_box 156 -620 32 188 -492 160", "vr_brush_list"])
marks = [int(x) for x in re.findall(r"^  1: entity .*, (\d+) decals", t, re.M)]
check(len(marks) == 4 and marks[0] >= 1 and marks[1] == marks[0] and marks[2] == 0 and marks[3] == 0,
      f"rocket: its scorch on the box, kept when painted, gone when moved, none on a new box ({marks})")
pieces = re.findall(r"box3d: \*qvrbrush/1: \d+ convex pieces", t)
freed = re.findall(r"box3d: (\d+) retired hulls freed, (\d+) kept", t)
check(len(pieces) == 2 and len(freed) == 2 and freed[-1][1] == "0",
      f"rocket: Box3D's hulls made again for the two moves, not the paint; the old ones freed ({len(pieces)}, {freed})")

# marks on the floor under a box (laid before it, a unit under it) stay through its edits (only its own marks go)
t = run(R + ["vr_decal_stress 6 10 stack", "wait10", "vr_decal_count", "vr_brush_box 196 -580 18 244 -532 96", "wait5",
             "vr_brush_texture 1 city6_4", "wait5", "vr_brush_move 1 0 4 0", "wait5", "vr_brush_set 1 196 -580 18 244 -532 120",
             "wait5", "vr_decal_count", "vr_brush_list"])
laid = [int(x) for x in re.findall(r"decals laid: (\d+)", t)]
check(len(laid) == 2 and laid[0] >= 4 and laid[1] == laid[0], f"floor: the marks under a box kept through its edits ({laid})")

# the tool
ST = "vr_toolgun_brush_status"
t = run(R + ["impulse 169", "vr_weapon_grip_mode 1", "wait10", "vr_toolgun_tool 5", "vr_toolgun_brush_grid 8",
             "vr_toolgun_brush_mode_set create", hand(40), trig(1), hand(46, 25), trig(0), hand(72), trig(1), trig(0),
             "vr_toolgun_brush_mode_set move", hand(50, 10), trig(1), hand(50, -10), trig(0, 10),
             "vr_toolgun_brush_mode_set resize", hand(71, -6), trig(1), hand(82, -6), trig(0, 10),
             "vr_brush_texture_set city6_4", "vr_toolgun_brush_mode_set paint", hand(50, -10), trig(1), trig(0),
             "vr_toolgun_brush_mode_set copy", trig(1), trig(0), hand(35, 30), trig(1), trig(0), "vr_brush_list",
             "vr_toolgun_brush_mode_set delete", hand(35, 30), trig(1), trig(0), "vr_brush_list",
             # a wedge made as the box was, turned by the hand (40 degrees: 45 on the 15 degree steps), the turn undone
             "vr_brush_delete all", "vr_toolgun_brush_shape 1", "vr_toolgun_brush_mode_set create", hand(40), trig(1),
             hand(46, 25), trig(0), hand(72), trig(1), trig(0), "vr_toolgun_brush_shape 0", "vr_brush_list",
             "vr_toolgun_brush_mode_set rotate", hand(50, 10), trig(1), hand(50, 50), trig(0, 10), "vr_brush_list",
             "vr_brush_undo", "vr_brush_list"])
# (the corners with the toolgun's muzzle on its lens's middle, weapon settings 42: -552 and -512 with the old one, ~1 cm
# off, the far edge a grid step further)
check(re.search(r"toolgun: box 1 made, 176 -616 17 \.\. 200 -560 81", t) is not None, "tool: created by dragging and raising")
check("toolgun: box 1 moved 0 40 0" in t, "tool: moved 40 sideways")
check("toolgun: box 1 resized to 176 -576 17 .. 200 -520 104" in t, "tool: the top pulled up to 104")
check("toolgun: box 1 painted city6_4" in t, "tool: painted")
check("toolgun: box 2 made as a copy" in t and "toolgun: box 2 removed" in t, "tool: copied, the copy deleted")
check(re.search(r"toolgun: wedge 1 made, 176 -616 17 \.\. 200 -560 81", t) is not None and
      re.search(r"^  1: entity \d+, rev 1, 5 faces", t, re.M) is not None, "tool: a wedge made by the same drag")
turned = re.findall(r"toolgun: brush 1 turned (-?\d+) (-?\d+) (-?\d+)", t)
after = re.findall(r"^  1: entity \d+, rev \d+, 5 faces, \S+, ([-\d. ]+), \d+ decals", t, re.M)
check(len(turned) == 1 and turned[0][1] in ("45", "-45") and len(after) == 3 and after[1] != after[0] and after[2] == after[0],
      f"tool: the wedge turned 45 by the hand, undone ({turned}, {after})")

if SHOTS:
    # the dynamic light's shadow (a pillar lit from its front left), the tool's ghost, a rocket's scorch, the panel
    run(R + ["vr_brush_box 196 -572 32 228 -540 128", "wait10", "vr_mock_look 20 60", "wait10", "vr_light_test 500 6 90", "wait3",
             "vr_mock_look 15 0", "wait10", "screenshot", "vr_brush_delete all", "wait400",
             "impulse 169", "vr_weapon_grip_mode 1", "wait10", "vr_toolgun_tool 5",
             "vr_toolgun_brush_mode_set create", hand(40), trig(1), hand(46, 25), trig(0), hand(72), "screenshot", trig(1), trig(0),
             "vr_brush_delete all", "impulse 160", "wait10", "vr_brush_box 156 -620 32 188 -492 160",
             "vr_mock_hand main 0.08 1.2 -0.3 70 0 0", "wait30", "+attack", "wait10", "-attack", "wait60", "screenshot",
             "impulse 169", "wait10", "vr_toolgun_tool 5", hand(70),
             "vr_mock_button main secondary 1;wait2;vr_mock_button main secondary 0;wait10", "screenshot"],
        out=SCRATCH + "/brush_shots.png")
    print("shots: " + SCRATCH + "/brush_shots.png")

if PERF:
    boxes = []
    for i in range(5):
        for j in range(10):
            x, y = 60 + 48 * i, -716 + 32 * j
            boxes.append(f"vr_brush_box {x} {y} 16 {x + 16} {y + 16} {48 + 8 * ((i + j) % 5)}")
    t = run(R + ["vr_profile_csv 1", "wait400", "vr_profile_report 4", "vr_walltime start"] + boxes +
            ["vr_walltime made", "wait5", "vr_walltime uploaded", "wait400", "vr_profile_report 4"], extra=("--exclusive",))
    for line in t.splitlines():
        if re.match(r"^ *(frame|render|world) |vr_walltime", line):
            print("perf: " + line.strip())

print("FAIL: " + "; ".join(fails) if fails else "PASS")
sys.exit(1 if fails else 0)
