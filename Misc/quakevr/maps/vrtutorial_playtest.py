# vrtutorial_playtest.py -- plays the tutorial through headless (the mock headset): every gate passed by the mechanic
# it teaches, and checks each gate's outcome. The walks use the mock autopilot (vr_mock_walk_to: the head faces the next
# point, the stick pushes forward); hands press, grip, punch, throw, load and aim with the vr_mock_* commands.
#
#   python Misc/quakevr/maps/vrtutorial_playtest.py script [--god] [--from GATE] [--ledge]   # writes quakevr/vrtut2play.cfg
#   bash <kit>/run.sh <agent> -Timeout 900 -Script "exec vrtut2play.cfg" -Filter "^PT|vr_mock_walk_to|Player pos|^health|ENGINE|TIMEOUT" > out.txt
#   python Misc/quakevr/maps/vrtutorial_playtest.py check < out.txt                 # the gate table
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
R3_PLAT_Z = 156     # room 3's ladder block (vrtutorial_gen.py)

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


def take_lying_weapon(hand, after=20):
    """The weapon lying nearest taken by its handle. On its origin, its handle: a lying weapon's box is a 3-unit cube
    there, which the hand has to touch (a point along its drawn length, "weapon 0.53", is up to 8 units off it); the
    grip pressed and the hand kept on it frame by frame within the press's window (a hand touching it pushes it).
    Nothing lying: nothing done."""
    for up in (6, 2):
        c("vr_mock_hand_to %s nearest thrown_weapon %d" % (hand, up))
        w(4)
    c("+grab%s" % hand, "vr_mock_button %s grip 1" % hand)
    for _ in range(8):
        c("vr_mock_hand_to %s nearest thrown_weapon 0" % hand, "wait")
    w(after)


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


def take_to_holster(hand, cls, holster=3, hover=20, onto=0, tries=1):
    """The hand to the nearest `cls`, gripped, brought to a holster and let go there (collected). Leaning in. With
    `tries` > 1, again a little off each time (a thin thing lying any which way), the grip pressed once the hand is
    there: a try that took it has collected it at the holster, and the next finds none (nothing done)."""
    offsets = [(0, 0, 0), (0, 0, -3), (2, 0, 0), (-2, 0, 0), (0, 2, 0), (0, -2, 0), (0, 0, 3), (0, 0, -5)]
    for k in range(tries):
        dx, dy, dz = offsets[k % len(offsets)]
        c(LEAN)
        w(10)
        # over it first (a hand coming at it pushes it), the grip closed, then down onto it (reload_test.sh's way);
        # a retry: down onto it first, a little off, then the grip
        for _ in range(2):
            c("vr_mock_hand_to %s nearest %s %d" % (hand, cls, hover))
            w(4)
        if k == 0:
            c("+grab%s" % hand, "vr_mock_button %s grip 1" % hand, "wait")
        for _ in range(2):
            c("vr_mock_hand_to %s nearest %s %d" % (hand, cls, onto + dz))
            w(4)
        if dx or dy:
            c("vr_mock_hand_to %s by %d %d 0" % (hand, dx, dy))
            w(3)
        if k:
            c("+grab%s" % hand, "vr_mock_button %s grip 1" % hand, "wait")
        w(12)
        c(STAND)
        w(10)
        for _ in range(2):
            c("vr_mock_hand_to %s holster %d" % (hand, holster))
            w(10)
        let_go(hand)
        c("vr_mock_hand %s" % hand)
        w(30)


def take_spinning(hand, cls, holster, onto=7, rounds=3):
    """A floating pickup that spins (a key): the hand held on it and the grip pressed again and again (a press takes
    what the fist touches within 0.2 s of it: VR_CARRY_GRIP_WINDOW; the thin card's turn has to come round to the
    fist), then brought to a holster (collected if held). `rounds` times."""
    for r in range(rounds):
        if r:   # (a card knocked off its float lies on the floor: a hand reaching for it slantwise stops on the floor)
            c("vr_mock_walk_to nearest %s 44" % cls)   # (not over it: a touch by the body is not a hand's)
            w(120)
            c("vr_mock_walk_to off")
            onto = 2   # (lying flat: the fist on it)
        c(LEAN if not r else CROUCH)   # (crouched, not leant: a lean takes the body along, onto it)
        w(10)
        for _ in range(2):
            c("vr_mock_hand_to %s nearest %s %d" % (hand, cls, onto + 20))
            w(4)
        for _ in range(2):
            c("vr_mock_hand_to %s nearest %s %d" % (hand, cls, onto))
            w(4)
        for _ in range(12):
            if r:   # (lying still, it is touched only as the hand comes onto it: the grip pressed just before)
                c("vr_mock_hand_to %s nearest %s %d" % (hand, cls, onto + 12))
                w(4)
            c("+grab%s" % hand, "vr_mock_button %s grip 1" % hand)
            if r:
                c("vr_mock_hand_to %s nearest %s %d" % (hand, cls, onto))
            w(12)
            c("-grab%s" % hand, "vr_mock_button %s grip 0" % hand)
            w(2)
            c("vr_mock_hand_to %s nearest %s %d" % (hand, cls, onto))   # (nothing taken: still over it)
            w(2)
        c("+grab%s" % hand, "vr_mock_button %s grip 1" % hand)
        w(12)
        c(STAND)
        w(10)
        for _ in range(2):
            c("vr_mock_hand_to %s holster %d" % (hand, holster))
            w(10)
        let_go(hand)
        c("vr_mock_hand %s" % hand)
        w(30)


LEDGE_CHECKS = False   # --ledge: also the high ledge's negatives (its own run: the extra time shifts the later gates')
LEDGE_GATES = ("nojump", "noreach")


def ledge_checks():
    """Room 3's high ledge (100 over the ladder block) is neither jumped onto nor reached without a jump."""
    # not without the hands: a jump at it lands back on the block
    jump_to(3200, 704, 2, 80)
    pos("nojump")
    # nor without a jump: a hand up at an adult's full reach (2.2 m over the block: 72 units) gripping at the wall's
    # face finds nothing to hold
    walk(3140, 704, 4, 60)
    c("vr_mock_turn_to 0")
    w(30)
    c("vr_mock_hand_to main 3166 704 %d" % (R3_PLAT_Z + 72), "wait", "wait",
      "vr_mock_hand_to main 3166 704 %d" % (R3_PLAT_Z + 72), "+grabmain", "vr_mock_button main grip 1")
    w(8)
    pull("main", 60, 25)
    w(20)
    let_go("main")
    c("vr_mock_hand main")
    w(30)
    pos("noreach")
    walk(3140, 704, 4, 60)
    c("vr_mock_turn_to 0")
    w(30)


def g3_jump_climb():
    mark("jump", "start")
    walk(2490, 704, 12, 150)
    jump_to(2700, 704, 10)          # the 32 barrier at 2576
    pos("barrier1")
    jump_to(2900, 704, 8)           # the 40 barrier at 2768
    pos("barrier2")
    # the ladder (rungs every 20 from 16 at x 2953): hand over hand, then the block's lip at 156
    walk(2918, 704, 4, 100)
    c("vr_mock_turn_to 0")
    w(60)
    # (back from beyond the 40 barrier the run's momentum carries him ~15 units past 2918, out of the rungs' reach:
    # walked up to the ladder again once he has stopped)
    walk(2920, 704, 4, 60)
    w(30)
    hands = ("off", "main")
    for i, z in enumerate((76, 96, 116, 136)):
        h, other = hands[i % 2], hands[1 - i % 2]
        grab(h, 2953, 712 if h == "off" else 696, z)
        if i:
            let_go(other)
        pull(h, 24)
        w(20)   # (the body catches up with the pull)
    grab("off" if hands[0] != h else "main", 2962, 704, R3_PLAT_Z - 2)
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
    if LEDGE_CHECKS:
        ledge_checks()
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
    take_to_holster("off", "item_health", 2)
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
    walk(5192, 1340, 6, 100)   # by where the crate stood (the card lies there; not standing in its box)
    take_spinning("main", "item_key1", 3)
    walk(5216, 1180, 8, 200)
    walk(5216, 1100, 8, 120)
    w(60)
    walk(5216, 960, 8, 200)
    pos("key")


LEAN = "vr_mock_hand head 0 1.7 -0.3"   # the head leant 30 cm forward (over a table: within vr_lean_radius)
STAND = "vr_mock_hand head 0 1.7 0"
CROUCH = "vr_mock_hand head 0 1.0 0"   # the head down 70 cm (reaching the floor)


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


GOD = False   # --god: god mode from the fights on (not before: the fall must hurt, the kit must heal)


def g8_fight():
    mark("fight", "start")
    if GOD:
        c("god")
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
    # (from the west, the targets' side: the fight's corpse lies where it fell, east, often by the gun, and a hand
    # coming over it grips the corpse)
    walk(5250, 690, 16, 150)
    c("vr_mock_walk_to nearest thrown_weapon 28")
    w(200)
    # gripped at its handle (0.53 of its drawn length: held, ready to fire; elsewhere it is only carried), the hand
    # turned level (a grip's angle; the punches left it pitched)
    c("vr_mock_hand main 0.2 1.0 -0.3 0 0 0")
    w(5)
    # three tries (the dropped gun may lie against the body or the corpse; a try after a good one only drops and
    # takes it again: its hand_to finds it lying at the hand)
    take_lying_weapon("main")
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
    # a second pass for any missed (the box's last 8 shells)
    load_shells(8)
    for (x, y, z) in R8_TARGETS:
        shoot_at(x, y - 2, LOW + z)
    c("vr_mock_hand_aim main off")
    w(60)
    # the shotgun put away in a holster (the right hip): the hands free to climb
    for _ in range(2):
        c("vr_mock_hand_to main holster 3")
        w(10)
    let_go("main")
    c("vr_mock_hand main")
    w(20)
    walk(4790, 288, 8, 300)
    walk(4700, 288, 8, 200)
    pos("weapons")


def g10_dark():
    mark("dark", "start")
    walk(4700, 288, 8, 300)
    # the flashlight from the belt (TESTING.md's mock reach), switched on by its trigger, held in the off hand
    c("vr_mock_fingers off 0 0", "vr_mock_hand off -0.066 1.38 -0.072 -80 0 0")
    w(40)
    c("vr_mock_button off grip 1")
    w(20)
    c("vr_mock_button off trigger 1")
    w(4)
    c("vr_mock_button off trigger 0", "vr_mock_hand off -0.2 1.3 -0.35 0 0 0")
    w(20)
    # the course: corridor 1 south over a block, 2 north over a raised stretch, 3 south over a block climbed, 4 north
    # up a step and a ledge, out west
    for (x, y) in ((4600, 256), (4392, 256), (4392, 90)):
        walk(x, y, 10, 200)
    jump_to(4392, -200, 12, 120)
    pos("dark_block1")
    for (x, y) in ((4392, -290), (4140, -290), (4140, -200)):
        walk(x, y, 10, 200)
    jump_to(4140, 40, 14, 100)
    for (x, y) in ((4140, 160), (4140, 290), (3880, 290), (3880, 100)):
        walk(x, y, 10, 200)
    pos("dark_block2")
    # the 48 block: climbed (a hand on its lip, pulled)
    c("vr_mock_turn_to 270")
    w(100)
    grab("main", 3880, 82, LOW + 46)
    pull("main", 50, 25)
    w(40)
    let_go("main")
    c("vr_mock_hand main")
    w(20)
    pos("dark_climb")
    for (x, y) in ((3880, -100), (3880, -290), (3600, -290), (3600, -210)):
        walk(x, y, 10, 200)
    jump_to(3600, -136, 6, 60)
    jump_to(3600, -20, 8, 100)
    pos("dark_ledge")
    for (x, y) in ((3600, 120), (3500, 256), (3400, 256)):
        walk(x, y, 10, 200)
    pos("dark")


THROW = None   # the throw play's path (made by script())
END_PLAY = None


def g11_throw():
    mark("throw", "start")
    walk(2800, -262, 3, 300)            # at the rocks' table
    c("vr_mock_turn_to 270")
    w(100)
    grab_item("main", "vr_rock")
    walk(2928, -322, 3, 200)            # before the grate (its bars at y -360, the button 48 behind them)
    c("vr_mock_turn_to 270")
    w(100)
    c('vr_mock_play "%s"' % THROW)
    w(220)
    c("vr_mock_hand main")
    w(40)
    walk(2600, -192, 8, 300)
    walk(2470, -192, 8, 200)
    pos("throw")


def g12_fire():
    mark("fire", "start")
    walk(2432, -258, 3, 300)        # under the south wall's east torch (its flame at 2432 -281)
    c("vr_mock_turn_to 270")
    w(100)
    c("vr_mock_hand main 0.0 0.8 -0.6 70 0 0")
    w(5)
    for _ in range(2):
        c("vr_mock_hand_to main nearest light_torch_small_walltorch 0")
        w(5)
    c("+grabmain", "vr_mock_button main grip 1")
    w(10)
    for _ in range(4):
        c("vr_mock_hand_to main by 0 6 0")    # pulled off its bracket
        w(3)
    c("vr_mock_hand main 0.2 1.3 -0.4 70 0 0")
    w(30)
    # to the crates in the passage; the flame touched to the nearest
    walk(2186, 0, 3, 300)
    c("vr_mock_turn_to 180")
    w(150)
    for y in (-30, 0, 30):
        c("vr_mock_hand_to main 2138 %d %d" % (y, LOW + 40))
        w(30)
        c("vr_mock_hand main 0.2 1.3 -0.4 70 0 0")
        w(30)
    walk(2240, 0, 8, 200)           # back from the fire
    w(72 * 40)                      # it spreads and burns through
    pos("fire_burnt")
    walk(2096, 0, 8, 300)
    walk(1950, 0, 8, 300)
    pos("fire")
    c("vr_mock_hand main 0.2 1.0 -0.7 70 0 0")   # (dropped ahead, not at his feet: it would set him alight)
    w(10)
    let_go("main")
    c("vr_mock_hand main")
    w(20)


def g13_arena():
    mark("arena", "start")
    for (x, y) in ((1856, -200), (1856, -440), (1856, -592), (1500, -592), (1400, -592)):
        walk(x, y, 10, 250)
    # the shotgun and shells from the tables by the east wall. (Reloading set Off here, a player's choice on room 8's
    # RELOADING button: the gun fires from the reserve. Hand loading is room 8's gate; here a long fight.)
    c("vr_reload_mode 0")
    walk(1337, -700, 3, 200)
    c("vr_mock_turn_to 0")
    w(100)
    grab_item("main", "weapon_shotgun")
    for _ in range(3):
        walk(1337, -856, 3, 200)
        take_to_holster("off", "item_shells", 2)
        w(72 * 10)                 # (its restock puts the next box there)
    health("arena_before")
    c("vr_weapon_grab_anywhere 0")  # (a gun knocked away is taken again by its handle: the mock can't find one lying)
    walk(1100, -640, 8, 300)        # in: the door shuts, the countdown, the waves
    w(72 * 7)
    for k in range(30):
        c("vr_mock_turn_to monster", "vr_mock_hand_aim main monster", "vr_mock_walk_to monster 70")
        for _ in range(6):
            w(30)
            c("+attack")
            w(3)
            c("-attack")
        c("vr_mock_hand_aim main off", "vr_mock_turn_to off", "vr_mock_walk_to off")
        # a gun knocked out of the hand is taken again (nothing lying: nothing done)
        c("vr_mock_walk_to nearest thrown_weapon 26")
        w(100)
        # (the grip let go first: a knocked-out gun leaves it pressed, and only a new press takes; a gun still held
        # drops at the hand and is taken again by its handle)
        let_go("main")
        c("vr_mock_hand main 0.2 1.0 -0.3 0 0 0")
        w(2)
        take_lying_weapon("main", 10)
        if k % 3 == 2:
            walk(1337, -856, 3, 250)   # more shells (a box every 10 s there)
            take_to_holster("off", "item_shells", 2)
    health("arena")
    c("vr_mock_hand_aim main off", "vr_mock_turn_to off")
    # (round the pit's east side, not down its stairs: x 640..1024, y -1152..-768, the stairs on its north side)
    walk(1060, -700, 8, 300)
    walk(1060, -1250, 8, 300)
    walk(832, -1300, 8, 400)
    walk(832, -1500, 8, 300)
    walk(832, -1700, 8, 300)
    pos("arena")
    # the teleporter: its changelevel is a localcmd, run after the rest of this cfg: so the cfg ends here, and a play
    # (its commands run on the clock) reports and quits once the hub has loaded
    c('vr_mock_play "%s"' % END_PLAY, "vr_mock_walk_to 832 -1860 8")
    OUT.append("ENDS")


GATES = [("locomotion", g1_locomotion), ("button", g2_button), ("jump", g3_jump_climb), ("swim", g4_swim),
         ("fall", g5_fall), ("heal", g6_heal), ("melee", g7_melee_key), ("fight", g8_fight), ("weapons", g9_weapons),
         ("dark", g10_dark), ("throw", g11_throw), ("fire", g12_fire), ("arena", g13_arena)]
# where --from puts the player first (x y z yaw)
STARTS = {"jump": (2320, 704, 24, 0), "swim": (3600, 704, UP + 24, 0), "fall": (4300, 1200, UP + 24, 0),
          "heal": (4688, 1200, LOW + 24, 0), "melee": (5024, 1216, LOW + 24, 0), "fight": (5216, 1040, LOW + 24, 270),
          "weapons": (5216, 330, LOW + 24, 270), "dark": (4740, 288, LOW + 24, 180),
          "throw": (3380, 256, LOW + 24, 180), "fire": (2480, -192, LOW + 24, 180),
          "arena": (1900, 0, LOW + 24, 270)}


# ---------------------------------------------------------------------------------------------------------------------
# No softlocks (--softlock: its own run, from room 6): what happens when the player wastes things, dies or reloads.
def respawn():
    """Dead: a button press brings him back (Quake's), at the latest checkpoint taken (vr_tutorial.qc)."""
    w(150)
    for _ in range(3):
        c("+jump")
        w(5)
        c("-jump")
        w(30)


def sl_key_after_death():
    mark("sl_key", "start")
    walk(5180, 1344, 4, 200)
    c("vr_mock_turn_to 0")
    w(40)
    fist(True)
    for _ in range(14):
        c('vr_mock_play "%s"' % PUNCH)
        w(150)
    fist(False)
    w(30)
    walk(5192, 1340, 6, 100)
    take_spinning("main", "item_key1", 3)
    mark("sl_key", "kill")
    c("kill")
    respawn()
    pos("sl_respawn")          # back at room 6's checkpoint
    health("sl_respawn")
    walk(5216, 1180, 8, 200)
    walk(5216, 1100, 8, 120)
    w(60)
    walk(5216, 960, 8, 200)
    pos("sl_key")              # the key kept through death: its door opened


def sl_fight_save():
    mark("sl_fight", "start")
    c("vr_tips_test list")     # t2_rifle waiting for its trigger
    walk(5216, 1000, 8, 150)
    walk(5216, 860, 8, 150)
    w(150)
    c("vr_mock_turn_to monster", "vr_mock_walk_to monster 36")
    fist(True)
    for _ in range(10):
        c('vr_mock_play "%s"' % PUNCH)
        w(110)
        c("vr_mock_walk_to monster 36")
    c("vr_mock_turn_to off", "vr_mock_walk_to off")
    fist(False)
    w(60)
    mark("sl_save", "save")
    c("save t2test")
    w(30)
    c("load t2test")
    w(120)
    mark("sl_save", "loaded")
    c("vr_tips_test list")     # t2_rifle shown now (not waiting)
    # another enemy by the button (the rifle wasted, say), then a second press while it lives: refused
    walk(5408, 905, 3, 300)
    c("vr_mock_turn_to 90")
    w(100)
    press_hand("main", 5408, 921, LOW + 52, 20)
    w(200)
    press_hand("main", 5408, 921, LOW + 52, 20)
    w(60)
    mark("sl_fight", "end")


def sl_restock():
    mark("sl_restock", "start")
    walk(4880, 337, 3, 300)
    c("vr_mock_turn_to 90")
    w(100)
    grab_item("main", "weapon_shotgun")
    w(72 * 12)                 # the bench's restock puts another shotgun there within 8 s
    walk(4980, 337, 3, 150)
    take_to_holster("off", "item_shells", 2)
    take_to_holster("off", "item_shells", 2)
    w(72 * 10)                 # and another box
    mark("sl_restock", "end")


SOFTLOCK = [("sl_key", sl_key_after_death, (5024, 1216, LOW + 24, 0)),
            ("sl_fight", sl_fight_save, (5216, 1040, LOW + 24, 270)),
            ("sl_restock", sl_restock, (5216, 330, LOW + 24, 270))]

SOFTLOCK_CHECKS = [
    ("sl_respawn", "killed: back at room 6's checkpoint", lambda p: 4960 < p[0] < 5100 and 1150 < p[1] < 1300),
    ("sl_respawn_h", "with a new life (100 health)", lambda h: h >= 100),
    ("sl_key", "the keycard kept through death: its door opened", lambda p: 5152 < p[0] < 5280 and p[1] < 1000),
]

SOFTLOCK_LINES = [
    ("sl_waiting", r"t2_rifle: .*waiting for its trigger", "a triggered tip waits for its trigger"),
    ("sl_shown", r"PT sl_save loaded[\s\S]*t2_rifle: (?![^\n]*waiting)", "after a save and load: shown (its trigger kept)"),
    ("sl_another", r"(spawner r7_spawn: made a[\s\S]*){2}", "ANOTHER ENEMY made a second grunt"),
    ("sl_refused", r"Finish this one first", "a third while it lives: refused"),
    ("sl_shotgun", r"restock: a new weapon_shotgun", "the bench's shotgun restocked"),
    ("sl_shells", r"restock: a new item_shells", "the bench's shells restocked"),
]


def write_throw(path, elevation=5, gunangle=70.0):
    """One overhand throw of what the main hand holds (throw_plays.py's arc, released `elevation` degrees up): the
    hand brought back, swung over and let go of."""
    sys.path.insert(0, os.path.join(ROOT, "Misc", "quakevr"))
    import throw_plays
    keys, rel = throw_plays.arc_throw("overhand", 150, 40, 90 + elevation, 35, -45, 0.30)
    L = ["0.000 main 0.25 1.1 -0.2 70 0 0"]
    t0 = 1.0
    L.append("%.3f main %.4f %.4f %.4f %.2f 0 0" % (t0 - 0.5, keys[0][1], keys[0][2], keys[0][3], keys[0][4] + gunangle))
    L.append("%.3f grip main 1" % (t0 - 0.4))
    for k in keys:
        L.append("%.6f main %.6f %.6f %.6f %.4f 0 0" % (t0 + k[0], k[1], k[2], k[3], k[4] + gunangle))
    L.append("%.6f cmd -grabmain" % (t0 + rel))
    L.append("%.6f grip main 1" % (t0 + rel - 0.015))
    L.append("%.6f grip main 0" % (t0 + rel + 0.035))
    L.append("%.6f cmd vr_mock_button main grip 0" % (t0 + rel + 0.04))
    t1 = t0 + keys[-1][0] + 0.6
    L.append("%.3f main 0.25 1.1 -0.2 70 0 0" % t1)
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(L) + "\n")


def script(args):
    c('alias w10 "wait;wait;wait;wait;wait;wait;wait;wait;wait;wait"', "developer 1", "vr_tips 0", "vr_fixed_frames 1", "vr_climb_debug 1", "vr_debug_wallbuttons 1",
      "skill 0", "map vrtutorial")
    w(80)
    global GOD, LEDGE_CHECKS
    GOD = args.god
    LEDGE_CHECKS = args.ledge
    global PUNCH
    plays = os.path.join(ROOT, "scratch", "vrtut2_plays").replace("\\", "/")
    os.makedirs(plays, exist_ok=True)
    subprocess.run([sys.executable, os.path.join(ROOT, "Misc", "quakevr", "motion_synth.py"), "punch_straight",
                    "--distance", "0.95", "--mock", "--out", plays, "--name", "punch"], check=True,
                   capture_output=True)
    PUNCH = plays + "/" + [f for f in sorted(os.listdir(plays)) if f.startswith("punch") and f.endswith(".mock")][-1]
    global THROW, END_PLAY
    END_PLAY = plays + "/end.txt"
    with open(END_PLAY, "w", newline="\n") as f:
        f.write("12.0 cmd echo PT teleporter pos\n12.0 cmd viewpos\n12.5 cmd echo PT end\n13.0 cmd toggleconsole\n"
                "13.5 cmd quit\n")
    THROW = plays + "/throw.txt"
    write_throw(THROW)
    if args.softlock:
        runs = [(n, fn, st) for n, fn, st in SOFTLOCK if not args.start or n == args.start]
        for n, fn, (x, y, z, yaw) in runs:
            c("setpos %d %d %d 0 %d 0" % (x, y, z, yaw), "noclip")
            w(20)
            fn()
        c("echo PT end", "toggleconsole", "quit")
        path = os.path.join(ROOT, "quakevr", "vrtut2play.cfg")
        with open(path, "w", newline="\n") as f:
            f.write("\n".join(OUT) + "\n")
        print("wrote %s: %d lines (softlock)" % (path, len(OUT)))
        return
    names = [n for n, _ in GATES]
    start = names.index(args.start) if args.start else 0
    if args.start and args.start in STARTS:
        x, y, z, yaw = STARTS[args.start]
        c("setpos %d %d %d 0 %d 0" % (x, y, z, yaw), "noclip")
        w(20)
    for name, fn in GATES[start:]:
        fn()
    if "ENDS" in OUT:
        del OUT[OUT.index("ENDS"):]
    else:
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
    ("throw_press", r"pressed by player: vr_(rock|brick) thrown", "the button pressed by a thrown rock"),
    ("torch", r"walltorch: taken|walltorch: .*pulled out", "a wall torch taken by hand"),
    ("burning", r"burning: vr_crate", "the crates set on fire"),
    ("waves", r"Wave 3!", "the arena's waves came"),
    ("cleared", r"Arena cleared", "the arena cleared"),
    ("hub", r"(?m)SpawnServer: vrstart\s*$", "the hub (vrstart) loaded"),
    ("flashlight", r"flashlight: taken in the off hand", "the flashlight taken from the belt"),
    ("flashlight_on", r"flashlight: on", "the flashlight switched on"),
    ("grunt_dead", r"spawner r7_spawn: all dead", "the grunt beaten with fists"),
    ("rifle", r"weapon: main hand takes weapon (15|4) ", "the grunt's gun taken by hand"),
    ("rifle_held", r"(?m)(Grunt's )?Shotgun taken into the main hand, [\d.]+ units from its handle$", "held by its handle (ready to fire)"),
    ("swim_button", r"button: \S+ \(target \"\"\) pressed by player: hand", "the SWIMMING button pressed by hand"),
]

CHECKS = [
    ("locomotion", "walked room 1 and round the bending hall", lambda p: p[0] > 1150 and 640 < p[1] < 768),
    ("button", "the button opened the door", lambda p: p[0] > 1470),
    ("settings", "through the settings room to room 3", lambda p: p[0] > 2200),
    ("barrier1", "jumped the 32 barrier", lambda p: p[0] > 2600),
    ("barrier2", "jumped the 40 barrier", lambda p: p[0] > 2810),
    ("ladder", "climbed the ladder onto the block", lambda p: p[2] > 150),
    ("nojump", "the high ledge: a jump alone stays on the block", lambda p: p[2] < UP),
    ("noreach", "the high ledge: a reach to 2.2 m without a jump holds nothing", lambda p: p[2] < UP),
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
    ("dark_block1", "dark: over the first block", lambda p: p[1] < -100),
    ("dark_climb", "dark: climbed the 48 block", lambda p: p[2] > LOW + 60),
    ("dark_ledge", "dark: up the step and the ledge", lambda p: p[2] > LOW + 90),
    ("dark", "through the dark course to room 10", lambda p: p[0] < 3440),
    ("throw", "a thrown rock pressed the button: its door opened", lambda p: p[0] < 2540),
    ("fire", "burnt the crates in the passage: through to room 11b", lambda p: p[0] < 2032),
    ("arena_h", "alive after the arena's waves", lambda h: h > 0),
    ("arena", "the arena cleared: its way out open", lambda p: p[1] < -1560),
]


def check(softlock=False):
    global CHECKS, LINE_CHECKS
    if softlock:
        CHECKS, LINE_CHECKS = SOFTLOCK_CHECKS, []
    text = sys.stdin.read()
    lines = text.splitlines()
    rows = parse(lines)
    stuck = [l for l in lines if "stuck" in l]
    fails = 0
    n = 0
    for gate, what, ok in CHECKS:
        vals = [v for g, k, v in rows if g == gate]
        if gate in LEDGE_GATES and not vals:
            continue   # (--ledge's: not in this run)
        n += 1
        res = "PASS" if vals and ok(vals[-1]) else "FAIL"
        fails += res == "FAIL"
        print("%-14s %s  %s  (%s)" % (gate, res, what, vals[-1] if vals else "no state"))
    for gate, rx, what in (SOFTLOCK_LINES if softlock else LINE_CHECKS):
        res = "PASS" if re.search(rx, text) else "FAIL"
        fails += res == "FAIL"
        print("%-14s %s  %s" % (gate, res, what))
    for l in stuck[:10]:
        print("  " + l.strip())
    n += len(SOFTLOCK_LINES if softlock else LINE_CHECKS)
    print("%d of %d gates passed" % (n - fails, n))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["script", "check"])
    ap.add_argument("--softlock", action="store_true", help="the softlock checks instead (death, save and load, "
                    "wasted rifle, restocks); with check: their table")
    ap.add_argument("--god", action="store_true", help="god mode from the fist fight on (the fights can't kill him)")
    ap.add_argument("--from", dest="start", help="start at this gate (setpos there first)")
    ap.add_argument("--ledge", action="store_true", help="also room 3's high ledge's negatives (a jump alone, a reach "
                    "without a jump: neither gets up; run with --from jump, as they shift the later gates' timing)")
    args = ap.parse_args()
    if args.mode == "script":
        script(args)
    else:
        sys.exit(check(args.softlock))


if __name__ == "__main__":
    main()
