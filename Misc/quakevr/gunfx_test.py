"""gunfx_test.py -- the gunfire particles (Quake/vr/vr_gunfx.cpp; ROUND23.md, "Punchier gunfire") for every weapon
class, yours and the monsters': each shot makes the effects its class should (the brief's table) and the excluded
weapons none.

    python Misc/quakevr/gunfx_test.py <agent> [--kit C:/OHWorkspace/qvr-kit] [--only player|armed|grunt|enforcer]

Runs (the kit's run.sh, fast mode, vr_debug_gunfx 1 printing each shot's "gunfx shot who ... wid ... sparks ..."):
- player: every weapon id in turn into the main hand (impulse 149, vr_test_give_wid), a trigger pull each; ends on
  its "gunfxtest end" echo;
- armed: vr_agrunt_test 1 (an armed grunt with every weapon in turn, hunting you); ends on "agtest: every weapon";
- grunt, enforcer: one of each (vr_test_spawn 0, 8) hunting you; ends on its first shot's line.
Prints "PASS/FAIL <what>" lines and "gunfx_test: N passed, M failed".
"""

import argparse
import re
import subprocess
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvrtest  # noqa: E402

# Git's bash on Windows (a plain "bash" may be WSL's, which cannot run the kit).
BASH = "C:/Program Files/Git/bin/bash.exe" if os.path.exists("C:/Program Files/Git/bin/bash.exe") else "bash"

# Weapon ids (Quake/vr/vr_weapon_ids.h) and what their shots should make.
SHOTGUNS = {4, 15, 23}          # shotgun, grunts' gun, AD defender: sparks, smoke (+ the port's puff)
SUPER = {5}
REVOLVER = {31}
NAILS = {6, 7, 29}
LAUNCHERS = {8, 9, 21}
ROCKETS = {10, 20}
FLARE = {30}
ENERGY = {11, 12, 16, 22, 25}
EXCLUDED = {1, 2, 3, 13, 14, 17, 18, 19, 24, 26, 27, 28}
AD = set(range(20, 29))         # the AD guns: your hands' own burst (vr_weaponfx adGunBurst) makes their sparks
GUNS = SHOTGUNS | SUPER | REVOLVER | NAILS | LAUNCHERS | ROCKETS | FLARE | ENERGY

# Your weapons fired (the melee weapons left out: a trigger pull fires nothing).
# (The AD guns need AD's data: impulse 149 gives none without it; vr_agrunt_test 12 covers them with it.)
PLAYER_WIDS = [4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 16, 29, 30, 31, 1, 19]
# The armed grunts' (vr_agrunt_test 1: indices 6..16): the thunderbolt is a stream (no weaponfired); the proximity gun's
# grunt keeps its distance from its mines and seldom fires in the test's 5 s (its shots, if any, are judged as every one).
ARMED_WIDS = [4, 5, 6, 7, 8, 10, 12, 15, 16]

LINE = re.compile(r"gunfx shot who (\w+) (\d+) wid (-?\d+) kind (\w+) budget ([\d.]+) sparks (\d+) smoke (\d+) glow (\d+) "
                  r"arcs (\d+) port (\d+) back (\d+) dust (\d+)")

passed = 0
failed = 0


def check(ok, what):
    global passed, failed
    print(("PASS " if ok else "FAIL ") + what)
    if ok:
        passed += 1
    else:
        failed += 1


def run(kit, agent, script, quit_on, timeout):
    cmd = [BASH, kit + "/run.sh", agent, "-Script", script + ";toggleconsole;quit", "-Filter", "^gunfx|^agtest: (every|test)",
           "-QuitOn", quit_on, "-Timeout", str(timeout)]
    r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    lines = r.stdout.splitlines()
    if not any(re.search(quit_on, l) for l in lines):
        print("(run ended without %r: its output's end)\n%s\n%s" % (quit_on, "\n".join(lines[-8:]), r.stderr[-800:]))
    return lines


LIGHT = re.compile(r"gunfx light ([\d.-]+) vent (\d+) exhaust (\d+)")


def shots(lines):
    out = []
    for l in lines:
        m = LINE.search(l)
        if m:
            g = m.groups()
            out.append({"who": g[0], "wid": int(g[2]), "kind": g[3], "sparks": int(g[5]), "smoke": int(g[6]),
                        "glow": int(g[7]), "arcs": int(g[8]), "port": int(g[9]), "back": int(g[10]), "dust": int(g[11]),
                        "vent": -1, "exhaust": -1})
        m = LIGHT.search(l)
        if m and out:  # (the shot's second line: the nail pistol's vents' puffs, a monster's rocket exhaust's)
            out[-1]["vent"] = int(m.group(2))
            out[-1]["exhaust"] = int(m.group(3))
    return out


def judge(s, who, own_hands):
    """Whether shot `s` made what its weapon's class should; a reason if not."""
    w = s["wid"]
    total = s["sparks"] + s["smoke"] + s["glow"] + s["arcs"] + s["port"] + s["back"] + s["dust"]
    if w in EXCLUDED or w not in GUNS:
        return total == 0, "nothing"
    ad_own = own_hands and w in AD  # (their sparks, arcs and backblast: adGunBurst's)
    if w in ENERGY:
        return s["smoke"] == 0 and s["sparks"] == 0 and (ad_own or s["arcs"] > 0), "arcs, no smoke"
    # (a few sparks at least, every shell and nail gun, however fast it fires: the author's 2026-10-30 note)
    ok = s["smoke"] > 0 and s["glow"] > 0 and (ad_own or s["sparks"] >= 3)
    if w in ROCKETS:
        ok = ok and s["back"] > 0  # (the AD launcher too now: its exhaust's cone)
        if not own_hands:
            ok = ok and s["exhaust"] > 0  # (a monster's exhaust smokes at the shot; yours each frame after)
    if w == 29:
        ok = ok and s["vent"] > 0  # the nail pistol's side vents
    if w in SHOTGUNS and w != 4 or (w == 4 and not own_hands):
        ok = ok and s["port"] > 0  # (your shotgun's puff comes with its casing: a "gunfx port" line)
    if w in SUPER and not own_hands:
        ok = ok and s["sparks"] >= 2
    return ok, "sparks, smoke" + (", backblast" if w in ROCKETS else "") + (", port" if w in SHOTGUNS else "")


def judge_all(found, who, own_hands, expect):
    seen = set()
    for s in found:
        if s["who"] != who:
            continue
        ok, what = judge(s, who, own_hands)
        if not ok:
            check(False, "%s wid %d (%s): expected %s, got %s" % (who, s["wid"], s["kind"], what, s))
        seen.add(s["wid"])
    good = {s["wid"] for s in found if s["who"] == who and judge(s, who, own_hands)[0]}
    for w in expect:
        check(w in seen and w in good, "%s wid %d fired, its effects right" % (who, w))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("agent")
    ap.add_argument("--kit", default=qvrtest.KIT)
    ap.add_argument("--only", default="")
    a = ap.parse_args()
    setup = "map e1m1;wait60;god;vr_debug_gunfx 1"
    if a.only in ("", "player"):
        s = setup + ";notarget;vr_weapon_grip_mode 1;impulse 9;wait2;vr_mock_hand main 0.15 1.2 -0.45 70 0 0;wait5"
        for w in PLAYER_WIDS:
            # (the weapon's id echoed, its shot, then 0.7 s: the shot's frame, its pump's casing, the next weapon clear)
            s += ";vr_test_give_wid %d;impulse 149;wait5;echo gunfxtest wid %d;+attack;wait4;-attack;wait50" % (w, w)
        lines = run(a.kit, a.agent, s + ";echo gunfxtest end", "gunfxtest end", 240)
        check(any("gunfxtest end" in l for l in lines), "player: the run reached its end")
        # Each shot line belongs to the weapon echoed last.
        cur = -1
        found = []
        ports = set()
        for l in lines:
            m = re.search(r"gunfxtest wid (\d+)", l)
            if m:
                cur = int(m.group(1))
                continue
            if "gunfx port" in l and cur >= 0:
                ports.add(cur)
            m = LIGHT.search(l)
            if m and found:  # (the last shot's second line)
                found[-1]["vent"] = int(m.group(2))
                found[-1]["exhaust"] = int(m.group(3))
            for sh in shots([l]):
                if sh["wid"] != cur:
                    check(False, "player wid %d: a shot of weapon %d" % (cur, sh["wid"]))
                found.append(sh)
        judge_all(found, "player", True, [w for w in PLAYER_WIDS if w in GUNS])
        check(4 in ports, "player shotgun: its casing's port puff (gunfx port)")
    if a.only in ("", "armed"):
        # (wait: a safety net only, the run ends on its quit line)
        lines = run(a.kit, a.agent, setup + ";vr_agrunt_test 1;wait30000", "agtest: every weapon", 400)
        check(any("agtest: every weapon" in l for l in lines), "armed: vr_agrunt_test 1 reached its end")
        judge_all(shots(lines), "enemy", False, [w for w in ARMED_WIDS if w != 10])
        # The rocket launcher's grunt alone, where it has room (e1m1's start: 360 units ahead is a wall, it closes in
        # and clubs you): vr_agrunt_test 111 (weapon 11 alone) in vrtesthall; ends on its verdict line.
        lines = run(a.kit, a.agent, "map vrtesthall;wait60;god;vr_debug_gunfx 1;vr_agrunt_test 111;wait30000",
                    "agtest: test 111 over", 200)
        check(any("agtest: test 111 over" in l for l in lines), "armed rocket: vr_agrunt_test 111 reached its end")
        judge_all(shots(lines), "enemy", False, [10])
    for name, spawn, wid in (("grunt", 0, 15), ("enforcer", 8, 16)):
        if a.only not in ("", name):
            continue
        s = setup + ";vr_test_spawn %d;vr_test_spawn_dist 200;impulse 241;wait7200" % spawn
        lines = run(a.kit, a.agent, s, "gunfx shot who enemy .* wid %d " % wid, 120)
        judge_all(shots(lines), "enemy", False, [wid])
    print("gunfx_test: %d passed, %d failed" % (passed, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
