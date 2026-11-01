"""barrelglow_test.py -- a hot barrel's glow on the gun's metal and its light (Quake/vr/vr_hotblade.cpp barrelBlade,
barrelLine; vr_gunfx.cpp barrelHeat; ROUND23.md, "Barrel glow on the metal").

    python Misc/quakevr/barrelglow_test.py <agent>   (the kit: qvrtest.KIT)

One run (the kit's run.sh, fast mode), vr_debug_gunfx 2: "gunfx heat hand .." at each shot and, each frame a barrel
glows, "gunfx barrel hand H heat X glow G light L reach R part P at x y z drawn N off D" (N, D: the last frame's draws
of the glowing gun and how far its drawn muzzle was from where the light was placed from). Heat-Up 1 s, Cool-Down 3 s,
Glow Strength 1, Heat Light Strength 1:
- shotgun: fired 1.5 s: its barrel glows once the heat passes 0.15, the glow (heat - 0.15) / 0.85, the light following;
- nail: the nailgun fired 0.75 s while turning with the stick (the old lag case: the sprite glow trailed the gun) and
  walking: the muzzle moves, every frame's draws are exactly where the light was placed from (off < 0.01 unit);
- then it cools: the glow and light fade with the heat until the barrel lines stop; the run ends on the nailgun's
  "cold" line.
Prints "barrelglow_test: PASS/FAIL <what>" lines and "barrelglow_test: N passed, M failed".
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvrtest  # noqa: E402

BARREL = re.compile(r"gunfx barrel hand (\d+) heat ([\d.]+) glow ([\d.]+) light ([\d.]+) reach ([\d.]+) part ([\d.]+) "
                    r"at (-?[\d.]+) (-?[\d.]+) (-?[\d.]+) drawn (\d+) off ([\d.]+)")
PHASE = re.compile(r"glowtest (\w+)")

T = qvrtest.Tally("barrelglow_test")
check = T.check


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("agent")
    a = ap.parse_args()

    script = ("map e1m1;wait60;god;notarget;vr_debug_gunfx 2;vr_gunfx 1;vr_gunfx_heat 1;vr_gunfx_heat_time 1;"
              "vr_gunfx_cool_time 3;vr_gunfx_heat_glow 1;vr_gunfx_heat_light 1;vr_weapon_grip_mode 1;impulse 9;wait2;"
              "vr_mock_hand main 0.15 1.2 -0.45 70 0 0;wait5;"
              "vr_test_give_wid 4;impulse 149;wait5;echo glowtest shotgun;+attack;wait108;-attack;wait5;"
              "vr_test_give_wid 6;impulse 149;wait5;echo glowtest nail;vr_mock_stick main 1 0;vr_mock_stick off 0 0.6;"
              "+attack;wait54;-attack;wait36;vr_mock_stick main 0 0;vr_mock_stick off 0 0;echo glowtest cooling;"
              "wait3600;echo glowtest gave up;toggleconsole;quit")  # (a safety net: the run ends on the "cold" line)
    lines = qvrtest.run_sh(a.agent, ["-Script", script, "-Filter", "^(gunfx barrel|gunfx heat cold|glowtest)",
                                     "-QuitOn", "gunfx heat cold hand \\d+ wid 6 ", "-Timeout", "180"],
                           timeout=300).splitlines()

    phase = None
    rows = {"shotgun": [], "nail": [], "cooling": []}
    cold = False
    for l in lines:
        m = PHASE.search(l)
        if m:
            phase = m.group(1) if m.group(1) in rows else None
            continue
        m = BARREL.search(l)
        if m and phase:
            heat, glow, light, reach, part = (float(m.group(i)) for i in range(2, 7))
            at = tuple(float(m.group(i)) for i in range(7, 10))
            rows[phase].append((heat, glow, light, reach, part, at, int(m.group(10)), float(m.group(11))))
        if "gunfx heat cold" in l and " wid 6 " in l:
            cold = True

    def follows(r):
        heat, glow, light, reach, part = r[:5]
        share = min(max((heat - 0.15) / 0.85, 0.0), 1.0)
        # the glow: share x Glow Strength 1; the light: share x Heat Light 1 x its flicker (0.88..1); the reach: 48 x
        # (0.6 + 0.4 share); a barrel part along the gun
        return (abs(glow - share) < 0.002 and share * 0.875 - 0.002 <= light <= share + 0.002
                and abs(reach - 48.0 * (0.6 + 0.4 * share)) < 0.15 and part > 1.0)

    s = rows["shotgun"]
    check(len(s) > 20, "shotgun: its barrel glows (%d frames)" % len(s))
    check(s and min(r[0] for r in s) > 0.15, "shotgun: only over heat 0.15 (lowest %.3f)" % (min(r[0] for r in s) if s else -1))
    check(s and all(follows(r) for r in s), "shotgun: the glow and light follow the heat (first %s, last %s)"
          % (s[0][:5] if s else None, s[-1][:5] if s else None))

    n = rows["nail"]
    drawnRows = [r for r in n[1:]]
    moved = max((sum((x - y) ** 2 for x, y in zip(r1[5], r2[5])) ** 0.5 for r1, r2 in zip(n, n[1:])), default=0.0)
    check(len(n) > 20 and all(follows(r) for r in n), "nail: the glow and light follow the heat (%d frames)" % len(n))
    check(moved > 1.0, "nail: turning and walking, the muzzle moves (up to %.2f units a frame)" % moved)
    check(drawnRows and all(r[6] > 0 for r in drawnRows), "nail: the glowing gun drawn each frame (drawn %s)"
          % sorted(set(r[6] for r in drawnRows))[:6])
    worst = max((r[7] for r in drawnRows), default=-1.0)
    check(drawnRows and worst < 0.01, "nail: drawn exactly where its light is placed from each frame (off at most %.4f)" % worst)

    c = rows["cooling"]
    heats = [r[0] for r in c]
    bad = [r[:5] for r in c if not follows(r)][:2] + [(a_, b) for a_, b in zip(heats, heats[1:]) if b > a_ + 1e-6][:2]
    check(len(c) > 10 and not bad, "cooling: the glow and light fade with the heat (%d frames, %.3f -> %.3f%s)"
          % (len(c), heats[0] if heats else -1, heats[-1] if heats else -1, "; off: %s" % bad if bad else ""))
    check(cold, "cooling: the nailgun cooled ('cold' line; the run's end: %s)" % lines[-3:])

    return T.verdict()


if __name__ == "__main__":
    raise SystemExit(main())
