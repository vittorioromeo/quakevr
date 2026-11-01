"""barrelheat_test.py -- barrel heat (Quake/vr/vr_gunfx.cpp heatShot / heatCool / heatGlow; ROUND23.md, "Barrel heat
settings"): your gun's barrel heats as it fires, its glow following the heat, cools over the Cool-Down Time once you
stop, and a weapon whose Barrel Heat multiplier is 0 never heats.

    python Misc/quakevr/barrelheat_test.py <agent>   (the kit: qvrtest.KIT)

One run (the kit's run.sh, fast mode, vr_debug_gunfx 1 printing "gunfx heat hand H wid W heat X glow Y" at each shot
and "gunfx heat cold hand H wid W after S s peak P" when a barrel has cooled), Heat-Up Time 1 s, Cool-Down Time 3 s,
Glow Strength 1:
- zero: the nailgun with its Barrel Heat 0 (vr_wofs_fx_gfx_heat_04 0), fired 0.5 s: every shot's heat 0;
- shotgun: fired 1.5 s: each shot adds 0.35 (a slow gun's most), the heat rising, the glow with it;
- nail: the nailgun fired 0.75 s: each shot adds 0.1 (its 0.1 s over Heat-Up Time), the glow following
  (Glow Strength x (heat - 0.15) / 0.85); then it cools to cold 0.15 s (a shot's interval and a half) + peak x 3 s after
  its last shot. The run ends on that "cold" line.
Prints "barrelheat_test: PASS/FAIL <what>" lines and "barrelheat_test: N passed, M failed".
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvrtest  # noqa: E402

HEAT = re.compile(r"gunfx heat hand (\d+) wid (\d+) heat ([\d.]+) glow ([\d.]+)")
COLD = re.compile(r"gunfx heat cold hand (\d+) wid (\d+) after ([\d.]+) s peak ([\d.]+)")
PHASE = re.compile(r"heattest (\w+)")

T = qvrtest.Tally("barrelheat_test")
check = T.check


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("agent")
    a = ap.parse_args()

    script = ("map e1m1;wait60;god;notarget;vr_debug_gunfx 1;vr_gunfx 1;vr_gunfx_heat 1;vr_gunfx_heat_time 1;"
              "vr_gunfx_cool_time 3;vr_gunfx_heat_glow 1;vr_weapon_grip_mode 1;impulse 9;wait2;"
              "vr_mock_hand main 0.15 1.2 -0.45 70 0 0;wait5;"
              "vr_wofs_fx_gfx_heat_04 0;vr_test_give_wid 6;impulse 149;wait5;echo heattest zero;+attack;wait36;-attack;"
              "wait5;vr_wofs_fx_gfx_heat_04 1;"
              "vr_test_give_wid 4;impulse 149;wait5;echo heattest shotgun;+attack;wait108;-attack;wait5;"
              "vr_test_give_wid 6;impulse 149;wait5;echo heattest nail;+attack;wait54;-attack;echo heattest stopped;"
              "wait3600;echo heattest gave up;toggleconsole;quit")  # (a safety net: the run ends on the "cold" line)
    quit_on = "gunfx heat cold hand \\d+ wid 6 "
    lines = qvrtest.run_sh(a.agent, ["-Script", script, "-Filter", "^(gunfx heat|heattest)", "-QuitOn", quit_on,
                                     "-Timeout", "180"], timeout=300).splitlines()

    phase = None
    heats = {"zero": [], "shotgun": [], "nail": [], "stopped": []}
    cold = None
    for l in lines:
        m = PHASE.search(l)
        if m and m.group(1) in heats:
            phase = m.group(1)
            continue
        m = HEAT.search(l)
        if m and phase:
            heats[phase].append((int(m.group(2)), float(m.group(3)), float(m.group(4))))
            continue
        m = COLD.search(l)
        if m and int(m.group(2)) == 6:
            cold = (float(m.group(3)), float(m.group(4)))

    z = heats["zero"]
    check(len(z) >= 3 and all(w == 6 and h == 0.0 and g == 0.0 for w, h, g in z),
          "zero: the nailgun with Barrel Heat 0 never heats (%d shots, heats %s)" % (len(z), [h for _, h, _ in z][:6]))

    s = heats["shotgun"]
    sh = [h for _, h, _ in s]
    check(len(s) >= 2 and all(w == 4 for w, _, _ in s), "shotgun: fired %d shots (%s)" % (len(s), sh))
    check(len(s) >= 2 and abs(sh[0] - 0.35) < 0.002 and all(b > a_ or b >= 0.999 for a_, b in zip(sh, sh[1:])),
          "shotgun: its heat rises 0.35 a shot to full (%s)" % sh)
    check(len(s) >= 2 and s[-1][2] > s[0][2] > 0.0, "shotgun: its glow rises with the heat (%s)" % [g for _, _, g in s])

    n = heats["nail"]
    nh = [h for _, h, _ in n]
    steps = [b - a_ for a_, b in zip(nh, nh[1:])]
    check(len(n) >= 5 and all(w == 6 for w, _, _ in n), "nail: fired %d shots" % len(n))
    check(len(n) >= 5 and abs(nh[0] - 0.1) < 0.002 and all(abs(d - 0.1) < 0.002 for d in steps),
          "nail: each shot adds 0.1 (no cooling between shots): %s" % nh)
    want = [min(max((h - 0.15) / 0.85, 0.0), 1.0) for h in nh]
    check(len(n) >= 5 and all(abs(g - w) < 0.002 for (_, _, g), w in zip(n, want)) and n[-1][2] > 0.5,
          "nail: the glow follows the heat (%s)" % [g for _, _, g in n])
    check(not heats["stopped"], "nail: no shot after letting go (%d)" % len(heats["stopped"]))

    if cold is None:
        check(False, "nail: cooled (no 'cold' line; the run's end: %s)" % lines[-4:])
    else:
        after, peak = cold
        expect = 0.15 + peak * 3.0
        check(n and abs(peak - nh[-1]) < 0.002, "nail: the peak is the last shot's heat (%.3f)" % peak)
        check(abs(after - expect) < 0.03,
              "nail: cold %.3f s after its last shot (expected %.3f: 0.15 s + peak %.3f x Cool-Down 3 s)" % (after, expect, peak))

    return T.verdict()


if __name__ == "__main__":
    raise SystemExit(main())
