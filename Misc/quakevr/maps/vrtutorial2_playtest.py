# vrtutorial2_playtest.py -- plays the tutorial through headless (the mock headset): every gate passed by the mechanic
# it teaches, and checks each gate's outcome. The walks use the mock autopilot (vr_mock_walk_to: the head faces the next
# point, the stick pushes forward); hands press, grip, punch, throw, load and aim with the vr_mock_* commands.
#
#   python Misc/quakevr/maps/vrtutorial2_playtest.py script [--god] [--from GATE]   # writes quakevr/vrtut2play.cfg
#   bash <kit>/run.sh <agent> -Timeout 900 -Script "exec vrtut2play.cfg" -Filter "^PT|vr_mock_walk_to|Player pos|^health|ENGINE|TIMEOUT" > out.txt
#   python Misc/quakevr/maps/vrtutorial2_playtest.py check < out.txt                 # the gate table
#
# The cfg's lines "echo PT <gate> <what>" mark each step; "viewpos" and "edict 1" (its health line) after them give the
# state the checks read. (One cfg exec'd, the waits as aliases: the kit's -Script expands waitN into lines.)
import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
LOW = -144
UP = 256

OUT = []


def c(*cmds):
    OUT.extend(cmds)


def w(n):
    n = int(n)
    OUT.extend(["w10"] * (n // 10) + ["wait"] * (n % 10))


def mark(gate, what=""):
    c("echo PT %s %s" % (gate, what))


def pos(gate):
    mark(gate, "pos")
    c("viewpos")


def health(gate):
    mark(gate, "health")
    c("edict 1")


def walk(x, y, r=16, frames=None, gate=None):
    """The autopilot to (x, y); waits `frames` (default: enough at walking pace for 1500 units)."""
    c("vr_mock_walk_to %d %d %d" % (x, y, r))
    w(frames if frames is not None else 300)


def press_hand(hand, x, y, z, hold=20):
    """A hand put on a point (a button's face) for `hold` frames, then back."""
    c("vr_mock_hand_to %s %d %d %d" % (hand, x, y, z))
    w(hold)
    c("vr_mock_hand %s" % hand)


# ---------------------------------------------------------------------------------------------------------------------
# The gates, in order (each a function adding its steps)
def g1_locomotion():
    mark("locomotion", "start")
    pos("locomotion")
    for (x, y) in ((400, 256), (630, 256)):
        walk(x, y, 24, 150)
    # the bending hall: the curves need the head (or the stick) turned all the way round
    for (x, y) in ((690, 256), (785, 290), (850, 380), (912, 480), (940, 580), (1010, 670), (1104, 704), (1176, 704)):
        walk(x, y, 20, 90)
    pos("locomotion")


def g2_button():
    mark("button", "start")
    walk(1400, 640, 16, 150)
    pos("button_before")
    # the button on the east wall at y 592, z 56 (its face 6 units off the wall): the off hand on it
    press_hand("off", 1432, 592, 56, 30)
    w(60)
    walk(1520, 704, 16, 150)
    pos("button")
    walk(2000, 704, 24, 300)
    walk(2250, 704, 16, 150)
    pos("settings")


def jump_to(x, y, after=8, frames=150):
    """Run towards (x, y), jumping `after` frames into the run (over what is in the way)."""
    c("vr_mock_walk_to %d %d 16" % (x, y))
    w(after)
    c("+jump")
    w(4)
    c("-jump")
    w(frames)


def grab(hand, x, y, z):
    c("vr_mock_hand_to %s %g %g %g" % (hand, x, y, z), "wait", "wait", "vr_mock_hand_to %s %g %g %g" % (hand, x, y, z),
      "wait", "+grab%s" % hand, "vr_mock_button %s grip 1" % hand)
    w(6)


def let_go(hand):
    c("-grab%s" % hand, "vr_mock_button %s grip 0" % hand)


def pull(hand, dz, steps=15):
    """A holding hand pulled down `dz` units (the body rises), a step a frame."""
    for _ in range(steps):
        c("vr_mock_hand_to %s by 0 0 %g" % (hand, -dz / steps), "wait")


def take_to_holster(hand, cls, holster=3, hover=20, onto=0):
    """The hand to the nearest `cls`, gripped, brought to a holster and let go there (collected). Leaning in."""
    c(LEAN)
    w(10)
    # over it first (a hand coming at it pushes it), the grip closed, then down onto it (reload_test.sh's way)
    for _ in range(2):
        c("vr_mock_hand_to %s nearest %s %d" % (hand, cls, hover))
        w(4)
    c("+grab%s" % hand, "vr_mock_button %s grip 1" % hand, "wait")
    for _ in range(2):
        c("vr_mock_hand_to %s nearest %s %d" % (hand, cls, onto))
        w(4)
    w(15)
    c(STAND)
    w(10)
    for _ in range(2):
        c("vr_mock_hand_to %s holster %d" % (hand, holster))
        w(10)
    let_go(hand)
    c("vr_mock_hand %s" % hand)
    w(30)


def g3_jump_climb():
    mark("jump", "start")
    walk(2490, 704, 12, 150)
    jump_to(2700, 704, 10)          # the 32 barrier at 2576
    pos("barrier1")
    jump_to(2900, 704, 8)           # the 40 barrier at 2768
    pos("barrier2")
    # the ladder (rungs every 20 from 16 at x 2953): hand over hand, then the block's lip at 144
    walk(2918, 704, 4, 100)
    c("vr_mock_turn_to 0")
    w(60)
    hands = ("off", "main")
    for i, z in enumerate((76, 96, 116, 136)):
        h, other = hands[i % 2], hands[1 - i % 2]
        grab(h, 2953, 712 if h == "off" else 696, z)
        if i:
            let_go(other)
        pull(h, 24)
        w(20)   # (the body catches up with the pull)
    grab("off" if hands[0] != h else "main", 2962, 704, 142)
    let_go(h)
    pull("off" if hands[0] != h else "main", 50, 25)
    w(40)
    let_go("off")
    let_go("main")
    c("vr_mock_hand off", "vr_mock_hand main")
    w(30)
    pos("ladder")
    # the wall a jump's height higher (its lip at UP): a jump, the hand up, the lip caught at the top of it, pulled up
    walk(3140, 704, 4, 100)
    c("vr_mock_turn_to 0")
    w(40)
    c("vr_mock_hand main 0.15 1.9 -0.45 0 0 0", "wait", "+jump")
    w(20)
    c("-jump", "vr_mock_hand_to main 3166 704 %d" % (UP + 2), "+grabmain", "vr_mock_button main grip 1")
    w(8)
    pull("main", 60, 25)
    w(20)
    let_go("main")
    c("vr_mock_hand main")
    w(30)
    pos("ledge")
    walk(3400, 704, 16, 150)
    walk(3560, 704, 16, 150)
    pos("room4")


def g4_swim():
    mark("swim", "start")
    # the SWIMMING button on room 4a's north wall (x 3760, z UP + 48): Vanilla (the stick moves you where you look)
    walk(3760, 936, 4, 200)
    c("vr_mock_turn_to 90")
    w(40)
    c("vr_debug_wallbuttons 1")
    press_hand("off", 3760, 951, UP + 48, 30)
    # (the button's command (vr_setup_option swim) is a localcmd: queued after the rest of this cfg, which is all in
    # the command buffer already; the press is checked by its line, and the setting it makes is set here)
    c("vr_debug_wallbuttons 0", "vr_swim 0", "vr_mock_turn_to 0")
    w(40)
    # into pool A from its west side, down, north through the passage, east, up into pool B
    walk(3800, 800, 8, 150)
    walk(3920, 800, 16, 100)
    w(40)
    c("vr_mock_look 80 0", "vr_mock_stick off 0 1")
    w(70)
    c("vr_mock_stick off 0 0")
    pos("swim_down")
    c("vr_mock_look 0 0")
    walk(3920, 1136, 16, 300)
    walk(4120, 1136, 16, 200)
    pos("swim_passage")
    c("+jump")    # (up: the jump button swims up, as Quake's)
    w(100)
    pos("swim_up")
    c("vr_mock_walk_to 4236 1248 8")
    w(150)
    c("-jump")
    w(20)
    walk(4300, 1340, 8, 200)
    pos("swim")


def g5_fall():
    mark("fall", "start")
    walk(4320, 1200, 16, 200)
    walk(4600, 1200, 16, 200)
    health("fall_before")
    walk(4688, 1200, 8, 120)
    w(60)
    pos("fall")
    health("fall")


def g6_heal():
    mark("heal", "start")
    # the kit on the floor: gripped, held to a holster (it is used there)
    walk(4560, 1080, 8, 200)
    take_to_holster("main", "item_health")
    health("heal")
    walk(4900, 1216, 8, 200)   # the gate (full health): the door opens
    w(60)
    walk(5040, 1216, 8, 200)
    pos("heal")


PUNCH = None   # the punch play's path (made by script())


def g7_melee_key():
    mark("melee", "start")
    walk(5180, 1344, 4, 200)   # 0.95 m west of the crate's middle (5216, 1344)
    c("vr_mock_turn_to 0")
    w(40)
    c("+grabmain", "vr_mock_fingers main 1 1", "vr_mock_button main grip 1")
    for _ in range(14):
        c('vr_mock_play "%s"' % PUNCH)
        w(150)
    c("-grabmain", "vr_mock_fingers main 0 0", "vr_mock_button main grip 0", "vr_mock_hand main")
    w(30)
    mark("key", "find")
    walk(5212, 1330, 8, 100)   # by where the crate stood (the card lies there)
    take_to_holster("main", "item_key1", hover=30, onto=7)
    walk(5216, 1180, 8, 200)
    walk(5216, 1100, 8, 120)
    w(60)
    walk(5216, 960, 8, 200)
    pos("key")


LEAN = "vr_mock_hand head 0 1.7 -0.3"   # the head leant 30 cm forward (over a table: within vr_lean_radius)
STAND = "vr_mock_hand head 0 1.7 0"


def grab_item(hand, cls, hover=20, onto=0):
    """The hand over the nearest `cls`, the grip closed, down onto it: taken (a weapon: held). Leaning in."""
    c(LEAN)
    w(10)
    for _ in range(2):
        c("vr_mock_hand_to %s nearest %s %d" % (hand, cls, hover))
        w(4)
    c("+grab%s" % hand, "vr_mock_button %s grip 1" % hand, "wait")
    for _ in range(2):
        c("vr_mock_hand_to %s nearest %s %d" % (hand, cls, onto))
        w(4)
    w(15)
    c(STAND)
    w(10)


def fist(on):
    if on:
        c("+grabmain", "vr_mock_fingers main 1 1", "vr_mock_button main grip 1")
    else:
        c("-grabmain", "vr_mock_fingers main 0 0", "vr_mock_button main grip 0", "vr_mock_hand main")


def g8_fight():
    mark("fight", "start")
    walk(5216, 1000, 8, 150)
    walk(5216, 860, 8, 150)          # room 7's trigger: a grunt from the alcove in 2 s
    w(250)
    c("vr_mock_turn_to monster", "vr_mock_walk_to monster 36")
    fist(True)
    for _ in range(14):
        c('vr_mock_play "%s"' % PUNCH)
        w(110)
        c("vr_mock_walk_to monster 36", "viewpos")
    c("vr_mock_turn_to off", "vr_mock_walk_to off")
    fist(False)
    w(30)
    health("fight")
    # its rifle: walked to, gripped; the three targets on the west wall shot
    c("vr_mock_walk_to nearest thrown_weapon 28")
    w(200)
    # gripped at its handle (0.53 of its drawn length: held, ready to fire; elsewhere it is only carried), the hand
    # turned level (a grip's angle; the punches left it pitched)
    c("vr_mock_hand main 0.2 1.0 -0.3 0 0 0")
    w(5)
    for _ in range(2):
        c("vr_mock_hand_to main weapon 0.53 8")
        w(4)
    c("+grabmain", "vr_mock_button main grip 1", "wait")
    for _ in range(2):
        c("vr_mock_hand_to main weapon 0.53 0")
        w(4)
    w(20)
    c("vr_status")
    walk(5100, 672, 8, 200)
    c("vr_mock_turn_to 180", "vr_debug_wallbuttons 1")
    w(60)
    for y in (544, 672, 800):
        shoot_at(4903, y, -24, 50)
    c("vr_mock_hand_aim main off")
    w(30)
    let_go("main")     # (the rifle dropped: the hands free for the shotgun)
    c("vr_mock_hand main")
    w(30)
    walk(5216, 440, 8, 300)
    walk(5216, 330, 8, 200)
    pos("fight")


def shoot_at(x, y, z, frames=40):
    """The main hand aimed at (x, y, z) (vr_mock_hand_aim), the trigger pulled once."""
    c("vr_mock_hand_aim main %g %g %g" % (x, y, z))
    w(frames)
    c("+attack")    # (not the mock trigger: a mock button's binding runs after the rest of this cfg)
    w(3)
    c("-attack")
    w(30)


def load_shells(n):
    """n shells from the belly pouch (the off hand) into the port of the shotgun in the main hand."""
    for _ in range(n):
        c("vr_mock_hand_to off ammopouch", "wait", "wait", "wait", "wait", "wait", "vr_mock_hand_to off ammopouch")
        w(5)
        c("+graboff", "vr_mock_button off grip 1")
        w(10)
        for d in (6, 6, 0, 0):
            c("vr_mock_hand_to off lport %d" % d)
            w(6)
        w(6)
        let_go("off")
        c("vr_mock_hand off")
        w(8)


R8_TARGETS = [(4880, -64, 48), (5040, -224, 64), (5200, -96, 40), (5360, -320, 72), (5520, -160, 56),
              (4960, -400, 80), (5280, -464, 48), (5600, -416, 96), (5120, -32, 88), (5440, 16, 60)]


def g9_weapons():
    mark("weapons", "start")
    walk(4880, 337, 3, 300)          # at the bench (against it)
    c("vr_mock_turn_to 90")
    w(100)
    grab_item("main", "weapon_shotgun")
    walk(4980, 337, 3, 150)          # by the shells
    take_to_holster("off", "item_shells", 2)
    load_shells(8)
    walk(5216, 180, 8, 300)          # at the counter
    c("vr_mock_turn_to 270")
    w(150)
    for i, (x, y, z) in enumerate(R8_TARGETS):
        if i == 8:
            load_shells(4)
        shoot_at(x, y - 2, LOW + z)
    c("vr_mock_hand_aim main off")
    w(60)
    walk(4790, 288, 8, 300)
    walk(4700, 288, 8, 200)
    pos("weapons")


GATES = [("locomotion", g1_locomotion), ("button", g2_button), ("jump", g3_jump_climb), ("swim", g4_swim),
         ("fall", g5_fall), ("heal", g6_heal), ("melee", g7_melee_key), ("fight", g8_fight), ("weapons", g9_weapons)]
# where --from puts the player first (x y z yaw)
STARTS = {"jump": (2320, 704, 24, 0), "swim": (3600, 704, UP + 24, 0), "fall": (4300, 1200, UP + 24, 0),
          "heal": (4688, 1200, LOW + 24, 0), "melee": (5024, 1216, LOW + 24, 0), "fight": (5216, 1040, LOW + 24, 270),
          "weapons": (5216, 330, LOW + 24, 270)}


def script(args):
    c('alias w10 "wait;wait;wait;wait;wait;wait;wait;wait;wait;wait"', "developer 1", "vr_tips 0", "vr_fixed_frames 1", "vr_climb_debug 1",
      "map vrtutorial2")
    w(80)
    if args.god:
        c("god")
    global PUNCH
    plays = os.path.join(ROOT, "scratch", "vrtut2_plays").replace("\\", "/")
    os.makedirs(plays, exist_ok=True)
    subprocess.run([sys.executable, os.path.join(ROOT, "Misc", "quakevr", "motion_synth.py"), "punch_straight",
                    "--distance", "0.95", "--mock", "--out", plays, "--name", "punch"], check=True,
                   capture_output=True)
    PUNCH = plays + "/" + [f for f in sorted(os.listdir(plays)) if f.startswith("punch") and f.endswith(".mock")][-1]
    names = [n for n, _ in GATES]
    start = names.index(args.start) if args.start else 0
    if args.start and args.start in STARTS:
        x, y, z, yaw = STARTS[args.start]
        c("setpos %d %d %d 0 %d 0" % (x, y, z, yaw), "noclip")
        w(20)
    for name, fn in GATES[start:]:
        fn()
    c("echo PT end", "toggleconsole", "quit")
    path = os.path.join(ROOT, "quakevr", "vrtut2play.cfg")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(OUT) + "\n")
    print("wrote %s: %d lines" % (path, len(OUT)))


# ---------------------------------------------------------------------------------------------------------------------
# The checks: per gate, a condition on the state printed after its marks
def parse(lines):
    """[(gate, what, value)]: positions (x, y, z) after "pos" marks, health after "health" marks."""
    out = []
    cur = None
    for l in lines:
        m = re.match(r"PT (\S+) ?(\S*)", l)
        if m:
            cur = (m.group(1), m.group(2))
            continue
        m = re.search(r"Player pos: \(?\s*([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)", l)
        if m and cur and cur[1] == "pos":
            out.append((cur[0], "pos", tuple(float(v) for v in m.groups())))
            cur = None
            continue
        m = re.match(r"health\s+([-\d.]+)", l.strip())
        if m and cur and cur[1] == "health":
            out.append((cur[0] + "_h", "health", float(m.group(1))))
            cur = None
    return out


# (gate, a regex the log must have, what it shows)
LINE_CHECKS = [
    ("grunt_dead", r"spawner r7_spawn: all dead", "the grunt beaten with fists"),
    ("rifle", r"weapon: main hand takes weapon 15", "the grunt's rifle taken by hand"),
    ("rifle_held", r"Grunt's Shotgun taken into the main hand, [\d.]+ units from its handle$", "held by its handle (ready to fire)"),
    ("swim_button", r"button: \S+ \(target \"\"\) pressed by player: hand", "the SWIMMING button pressed by hand"),
]

CHECKS = [
    ("locomotion", "walked room 1 and round the bending hall", lambda p: p[0] > 1150 and 640 < p[1] < 768),
    ("button", "the button opened the door", lambda p: p[0] > 1470),
    ("settings", "through the settings room to room 3", lambda p: p[0] > 2200),
    ("barrier1", "jumped the 32 barrier", lambda p: p[0] > 2600),
    ("barrier2", "jumped the 40 barrier", lambda p: p[0] > 2810),
    ("ladder", "climbed the ladder onto the block", lambda p: p[2] > 150),
    ("ledge", "caught the high ledge at a jump's top, pulled up", lambda p: p[2] > UP + 10),
    ("room4", "on to room 4", lambda p: p[0] > 3550 and p[2] > UP),
    ("swim_passage", "swam down and through the passage", lambda p: p[0] > 4060 and p[2] < UP),
    ("swim", "up into pool B and out", lambda p: p[2] > UP + 10),
    ("fall", "fell through the hole to room 5", lambda p: p[2] < 0),
    ("fall_h", "the fall hurt (health under 100)", lambda h: h < 100),
    ("heal_h", "healed to full with a kit", lambda h: h >= 100),
    ("heal", "the health gate opened at full health", lambda p: p[0] > 4970),
    ("key", "punched the crate, took the keycard, opened its door", lambda p: 5152 < p[0] < 5280 and p[1] < 1000),
    ("fight_h", "survived the fist fight", lambda h: h > 0),
    ("fight", "the rifle shot the three targets: the door opened", lambda p: 5152 < p[0] < 5280 and p[1] < 400),
    ("weapons", "shotgun loaded by hand, reloaded, the range cleared", lambda p: 4528 < p[0] < 4760 and p[1] > 150),
]


def check():
    lines = sys.stdin.read().splitlines()
    rows = parse(lines)
    stuck = [l for l in lines if "stuck" in l]
    fails = 0
    for gate, what, ok in CHECKS:
        vals = [v for g, k, v in rows if g == gate]
        res = "PASS" if vals and ok(vals[-1]) else "FAIL"
        fails += res == "FAIL"
        print("%-14s %s  %s  (%s)" % (gate, res, what, vals[-1] if vals else "no state"))
    for gate, rx, what in LINE_CHECKS:
        res = "PASS" if any(re.search(rx, l) for l in lines) else "FAIL"
        fails += res == "FAIL"
        print("%-14s %s  %s" % (gate, res, what))
    for l in stuck[:10]:
        print("  " + l.strip())
    print("%d of %d gates passed" % (len(CHECKS) + len(LINE_CHECKS) - fails, len(CHECKS) + len(LINE_CHECKS)))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["script", "check"])
    ap.add_argument("--god", action="store_true", help="god mode (the fights can't kill him)")
    ap.add_argument("--from", dest="start", help="start at this gate (setpos there first)")
    args = ap.parse_args()
    if args.mode == "script":
        script(args)
    else:
        sys.exit(check())


if __name__ == "__main__":
    main()
