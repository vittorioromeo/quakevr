"""gunfx_test.py -- the gunfire particles (Quake/vr/vr_gunfx.cpp; ROUND23.md, "Punchier gunfire") for every weapon
class, yours and the monsters': each shot makes the effects its class should (the brief's table) and the excluded
weapons none.

    python Misc/quakevr/gunfx_test.py <agent> [--kit C:/OHWorkspace/qvr-kit] [--only player|armed|grunt|enforcer|wisps|tube|slots]

Runs (the kit's run.sh, fast mode, vr_debug_gunfx 1 printing each shot's "gunfx shot who ... wid ... sparks ..."):
- player: every weapon id in turn into the main hand (impulse 149, vr_test_give_wid), a trigger pull each; ends on
  its "gunfxtest end" echo;
- armed: vr_agrunt_test 1 (an armed grunt with every weapon in turn, hunting you); ends on "agtest: every weapon";
- grunt, enforcer: one of each (vr_test_spawn 0, 8) hunting you; ends on its first shot's line;
- wisps: the muzzle wisps (vr_muzzlewisps.cpp) of the shotguns, nailguns, revolver, none of the rocket launcher's;
- tube: a grunt's own gun fired as a rocket tube (vr_gunfx_monster_test 1: AD's rocket soldier's path);
- slots: the per-weapon Gunfire multipliers (Weapon Offsets > Effects) at 0, 1 and 2, with the global amounts; ends on
  its "gunfxslot end" echo.
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
    if w == 29 or (w == 10 and own_hands):
        ok = ok and s["vent"] > 0  # the nail pistol's side vents; your rocket launcher's vents facing you (v_rock2.mdl)
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


WISP_START = re.compile(r"gunfx wisps start hand \d barrels (\d) after ([\d.]+) s")
WISP_FADED = re.compile(r"gunfx wisps faded hand \d barrels \d after [\d.]+ s charge [\d.]+ points (\d+) drawn (\d+)")
# (weapon id, frames the trigger is held, the wisps' barrels: 0 none)
WISP_GUNS = [(10, 4, 0), (4, 4, 1), (5, 4, 2), (6, 36, 2), (7, 36, 1), (31, 4, 1)]


def wisps_case(a):
    """Muzzle wisps (vr_muzzlewisps.cpp): each gun's start only after its last shot (vr_gunfx_wisp_delay 0.25 s later),
    one a barrel, drawn, then fade; none for a launcher. Short-lived here (time 0.5 s, fade 0.4 s): each gun waits for its
    "faded" line (vr_waitprint), the launcher its refire time (60 frames: wisps would start after 18)."""
    s = ("map e1m1;wait60;god;notarget;vr_debug_gunfx 1;vr_gunfx_wisp_time 0.5;vr_gunfx_wisp_fade 0.4;"
         "vr_weapon_grip_mode 1;impulse 9;wait2;vr_mock_hand main 0.15 1.2 -0.45 70 0 0;wait5")
    for w, hold, barrels in WISP_GUNS:
        s += ";vr_test_give_wid %d;impulse 149;wait5;echo gunfxtest wid %d;+attack;wait%d;-attack" % (w, w, hold)
        s += ";vr_waitprint 600 gunfx wisps faded" if barrels else ";wait60"  # (its refire: 0.8 s)
    lines = run(a.kit, a.agent, s + ";echo gunfxtest end", "gunfxtest end", 240)
    check(any("gunfxtest end" in l for l in lines), "wisps: the run reached its end")
    seen = {}
    cur = -1
    for l in lines:
        m = re.search(r"gunfxtest wid (\d+)", l)
        if m:
            cur = int(m.group(1))
            seen[cur] = {"events": []}
            continue
        if cur < 0:
            continue
        if LINE.search(l):
            seen[cur]["events"].append(("shot",))
        m = WISP_START.search(l)
        if m:
            seen[cur]["events"].append(("start", int(m.group(1)), float(m.group(2))))
        m = WISP_FADED.search(l)
        if m:
            seen[cur]["events"].append(("faded", int(m.group(1)), int(m.group(2))))
    for w, hold, barrels in WISP_GUNS:
        ev = seen.get(w, {"events": []})["events"]
        starts = [i for i, e in enumerate(ev) if e[0] == "start"]
        shots_at = [i for i, e in enumerate(ev) if e[0] == "shot"]
        if not barrels:
            check(bool(shots_at) and not starts, "wisps wid %d: fired, no wisps (%s)" % (w, ev))
            continue
        ok = bool(shots_at) and len(starts) == 1 and starts[0] > shots_at[-1]
        st = ev[starts[0]] if starts else None
        ok = ok and st[1] == barrels and st[2] >= 0.24
        faded = [e for e in ev if e[0] == "faded"]
        ok = ok and len(faded) == 1 and faded[0][1] > 0 and faded[0][2] > 0
        check(ok, "wisps wid %d: %d shot(s), %d barrel(s) after the last, drawn, faded (%s)" %
              (w, len(shots_at), barrels, [e for e in ev if e[0] != "shot"]))


def slots_case(a):
    """Gunfire multipliers (ROUND23.md, "Gunfire VFX settings"): the shotgun's slot (index 1: vr_wofs_*_02) at 0 makes no
    sparks or smoke, at 2 about twice the counts at 1 (vr_gunfx_test 4: its slot by the weapon's model); the global Spark
    and Smoke Amount 0.5 with the slot's 2 the counts at 1 again; your own shotgun fired with its slot at 0: no sparks, no
    smoke, no wisps (the hand's slot). Each vr_gunfx_test 0.42 s apart (30 frames: a full budget, 0.3 s since the last)."""
    keys = ("fx_gfx_sparks", "fx_gfx_smoke", "fx_gfx_wisps")
    def slot(v):
        return ";".join("vr_wofs_%s_02 %g" % (k, v) for k in keys)
    s = "map e1m1;wait60;god;notarget;vr_debug_gunfx 1"
    for label, sets in (("base", slot(1)), ("zero", slot(0)), ("double", slot(2)),
                        ("half", slot(2) + ";vr_gunfx_spark_amount 0.5;vr_gunfx_smoke_amount 0.5")):
        s += ";%s;echo gunfxslot %s;vr_gunfx_test 4;wait30" % (sets, label)
    s += (";vr_gunfx_spark_amount 1;vr_gunfx_smoke_amount 1;%s;vr_weapon_grip_mode 1;impulse 9;wait2;"
          "vr_mock_hand main 0.15 1.2 -0.45 70 0 0;wait5;vr_test_give_wid 4;impulse 149;wait5;echo gunfxslot fired;"
          "+attack;wait4;-attack;wait40;%s;echo gunfxslot end" % (slot(0), slot(1)))  # (wisps would start 0.26 s on)
    lines = run(a.kit, a.agent, s, "gunfxslot end", 120)
    check(any("gunfxslot end" in l for l in lines), "slots: the run reached its end")
    got = {}
    cur = None
    wisps = 0
    for l in lines:
        m = re.search(r"gunfxslot (\w+)", l)
        if m:
            cur = m.group(1)
            continue
        for sh in shots([l]):
            got.setdefault(cur, sh)
        if cur == "fired" and WISP_START.search(l):
            wisps += 1
    b, z, d, h, f = (got.get(k) for k in ("base", "zero", "double", "half", "fired"))
    check(bool(b) and b["sparks"] >= 3 and b["smoke"] > 0, "slots: at 1 sparks and smoke (%s)" % (b,))
    check(bool(z) and z["sparks"] == 0 and z["smoke"] == 0, "slots: at 0 no sparks, no smoke (%s)" % (z,))
    check(bool(b and d) and abs(d["sparks"] - 2 * b["sparks"]) <= 1 and abs(d["smoke"] - 2 * b["smoke"]) <= 1,
          "slots: at 2 twice the sparks and smoke (%s vs %s)" % (d, b))
    check(bool(b and h) and h["sparks"] == b["sparks"] and h["smoke"] == b["smoke"],
          "slots: global 0.5 times the slot's 2: as at 1 (%s)" % (h,))
    check(bool(f) and f["who"] == "player" and f["sparks"] == 0 and f["smoke"] == 0 and wisps == 0,
          "slots: your shotgun with its slot at 0: no sparks, smoke or wisps (%s, wisps %d)" % (f, wisps))


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
    if a.only in ("", "wisps"):
        wisps_case(a)
    if a.only in ("", "slots"):
        slots_case(a)
    if a.only in ("", "tube"):
        # A monster's own gun as a rocket tube (AD's rocket soldier: QC weaponfired QVR_FIRED_ROCKET), a grunt's gun
        # standing in for it (AD's soldier is its mesh): the backblast out of the gun's back, the exhaust smoking.
        s = setup + ";notarget;vr_test_spawn 0;vr_test_spawn_dist 200;impulse 241;wait30;vr_gunfx_monster_test 1;echo gunfxtest end"
        lines = run(a.kit, a.agent, s, "gunfxtest end", 120)
        found = [x for x in shots(lines) if x["who"] == "enemy"]
        check(len(found) == 1 and found[0]["wid"] == 10 and found[0]["back"] > 0 and found[0]["exhaust"] > 0,
              "a monster's rocket tube: backblast and exhaust (%s)" % (found[:1],))
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
