"""barrelfx_test.py -- the barrel glow through the fire frames and on the revolver, the barrel masks and their paint
mode, the rifles' wisps (Quake/vr/vr_barrelmask.cpp, vr_hotblade.cpp barrelInstance, vr_gunfx.cpp wispLook; ROUND23.md,
"Barrel glow: the fire frames, the revolver, a painted barrel").

    python Misc/quakevr/barrelfx_test.py <agent>   (the kit: qvrtest.KIT)

One run (fast mode), vr_debug_gunfx 2 ("gunfx barrel ... drawn N off D pieces P masked M" each frame a barrel glows:
P the draws of a part drawn in the gun's place, M the draws with its mask):
- shotgun fired 1.5 s: it glows every frame, also while its auto pump strokes (drawn as the gun without its fore-end and
  the fore-end: pieces > 0, each with its mask); its pump's triangles none of them marked, the body's barrel marked;
- paint: a brush stroke on the pump (vr_barrelpaint_brush) marks them; the config written, the mask reset, the config
  read back: the painted mask again (then reset);
- revolver fired: drawn in its parts, they glow with its mask;
- the grunts' burst rifle and the enforcers' rifle fired: their muzzle wisps start ("gunfx wisps start").
Prints "barrelfx_test: PASS/FAIL <what>" lines and "barrelfx_test: N passed, M failed".
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvrtest  # noqa: E402

BARREL = re.compile(r"gunfx barrel draws hand (\d+) glow ([\d.]+) drawn (\d+) pieces (\d+) masked (\d+)")
REPORT = re.compile(r"barrelmask slot (\d+) \S+ tris (\d+) marked (\d+) mask (\d)")
PART = re.compile(r"barrelmask part (\S+) tris (\d+) marked (\d+) unmatched (\d+)")
BRUSH = re.compile(r"barrelpaint brush painted (\d+) triangles, (\d+) marked")
PHASE = re.compile(r"bfx (\w+)")

T = qvrtest.Tally("barrelfx_test")
check = T.check


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("agent")
    a = ap.parse_args()

    script = ("map e1m1;wait60;god;notarget;vr_debug_gunfx 2;vr_gunfx 1;vr_gunfx_heat 1;vr_gunfx_heat_time 1;"
              "vr_gunfx_cool_time 3;vr_gunfx_heat_glow 1;vr_gunfx_wisps 1;vr_weapon_grip_mode 1;impulse 9;wait2;"
              "vr_mock_hand main 0.15 1.2 -0.45 70 0 0;wait5;"
              "vr_test_give_wid 4;impulse 149;wait5;echo bfx shotgun;+attack;wait108;-attack;wait5;echo bfx masks;"
              "vr_barrelmask_report 2 -99 -99 -99 99 99 99 part progs/vr_pump_on_v_shot.mdl;"
              "vr_barrelmask_report 2 -99 -99 -99 99 99 99 part progs/vr_pumpbody_on_v_shot.mdl;"
              "vr_barrelpaint main;wait2;vr_barrelpaint_brush 25 0 2 2;vr_barrelpaint_done;wait2;"
              "writeconfig barrelfxtest;reset vr_wofs_fx_gfx_mask_02;echo bfx reset;vr_barrelmask_report 2;"
              "exec barrelfxtest.cfg;echo bfx loaded;vr_barrelmask_report 2;reset vr_wofs_fx_gfx_mask_02;"
              "vr_test_give_wid 31;impulse 149;wait10;echo bfx revolver;+attack;wait4;-attack;wait40;+attack;wait4;"
              "-attack;wait40;+attack;wait4;-attack;wait30;"
              "vr_test_give_wid 15;impulse 149;wait10;echo bfx grunt;+attack;wait40;-attack;wait90;"
              "vr_test_give_wid 16;impulse 149;wait10;echo bfx enforcer;+attack;wait40;-attack;wait90;"
              "echo bfx end;toggleconsole;quit")
    lines = qvrtest.run_sh(a.agent, ["-Script", script, "-Filter",
                                     "^(gunfx barrel|gunfx wisps start|barrelmask|barrelpaint|bfx|Paint Barrel)",
                                     "-QuitOn", "^bfx end", "-Timeout", "240"], timeout=360).splitlines()

    phase = None
    rows = {}
    reports = {}
    parts = {}
    wisps = {}
    brush = None
    for l in lines:
        m = PHASE.search(l)
        if m and l.startswith("bfx"):
            phase = m.group(1)
            continue
        m = BARREL.search(l)
        if m and phase:
            rows.setdefault(phase, []).append(tuple(float(m.group(i)) for i in range(2, 6)))  # glow drawn pieces masked
        m = REPORT.search(l)
        if m:
            reports.setdefault(phase, []).append(int(m.group(3)))
        m = PART.search(l)
        if m:
            parts[os.path.basename(m.group(1))] = (int(m.group(2)), int(m.group(3)), int(m.group(4)))
        m = BRUSH.search(l)
        if m:
            brush = (int(m.group(1)), int(m.group(2)))
        if "gunfx wisps start" in l and phase:
            wisps[phase] = wisps.get(phase, 0) + 1

    s = rows.get("shotgun", [])
    s = s[1:]  # (the first line: its draws counted from the next frame)
    stroke = [r for r in s if r[2] > 0]
    check(len(s) > 20 and all(r[1] + r[2] > 0 for r in s),
          "shotgun: glows every frame it is hot, drawn each frame (%d frames)" % len(s))
    check(len(stroke) > 3 and all(r[3] >= r[2] for r in stroke),
          "shotgun: glows on its auto pump's pieces while it strokes, with its mask (%d frames; %s)"
          % (len(stroke), stroke[:2]))
    check(s and all(r[3] > 0 for r in s), "shotgun: its mask on every frame's draws")
    pump = parts.get("vr_pump_on_v_shot.mdl")
    body = parts.get("vr_pumpbody_on_v_shot.mdl")
    check(pump is not None and pump[1] == 0, "mask: the pump (fore-end) cold, none of its triangles marked (%s)" % (pump,))
    check(body is not None and body[1] > 100 and body[2] < body[0] * 0.1,
          "mask: the gun without its fore-end marked through the gun's (%s)" % (body,))
    shipped = reports.get("masks", [None])[0]
    check(brush is not None and brush[0] > 0, "paint: a stroke on the pump marks it (%s)" % (brush,))
    check(shipped is not None and reports.get("reset") == [shipped] and brush is not None
          and reports.get("loaded") == [shipped + brush[0]],
          "paint: written, reset (%s), read back (%s): shipped %s + painted" % (reports.get("reset"), reports.get("loaded"), shipped))

    r = rows.get("revolver", [])[1:]
    check(len(r) > 5 and all(x[2] > 0 and x[3] > 0 for x in r),
          "revolver: glows on its parts with its mask (%d frames; %s)" % (len(r), r[:2]))
    check(wisps.get("grunt", 0) > 0, "burst rifle: its wisps start (%d)" % wisps.get("grunt", 0))
    check(wisps.get("enforcer", 0) > 0, "enforcer rifle: its wisps start (%d)" % wisps.get("enforcer", 0))
    check(any(l.startswith("bfx end") for l in lines), "the run ends on its last line (%s)" % lines[-2:])
    return T.verdict()


if __name__ == "__main__":
    raise SystemExit(main())
