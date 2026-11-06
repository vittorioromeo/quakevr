"""swimtilt_test.py <agent> -- the swimming tilt (vr_body_swim_tilt; vr_avatar_pitch.cpp) in the game, with the mock
headset and then flat-screen (-flatscreenmode): the same body pose code path. In vrfiringrange's pool (the swim sound
test's), the body shown from its left in front of you (vr_body_debug 3), a screenshot at each step:
  dry      at the start, on the floor: upright (angle 0);
  mid      12 frames after going under (setpos turns noclip on, noclip turns it off; gravity 0): leaning, not yet there;
  swim     100 frames more: the full 45;
  ledge    under the pool's exit ledge, about 33 units from its lip: straightening (between 0 and the full lean);
  surface  the head out (waterlevel 2): upright again;
  out      facing the ledge at the surface, forward and jump (Quake's water jump): upright on the way out.
Prints "swimtilt_test: PASS/FAIL <what>" and "swimtilt_test: N passed, M failed"; the screenshots composed into
scratch/swimtilt_vr.png and scratch/swimtilt_flat.png (in the order above). Each run ends on its own "swimtilt done".
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvrtest  # noqa: E402

PITCH = re.compile(r"body_pitch: angle (-?[\d.]+) target (-?[\d.]+) rate (-?[\d.]+) source (\w+) waterlevel (\d) "
                   r"ground (\d) ledge (-?[\d.]+)")
STEP = re.compile(r"swimtilt step (\w+)")

T = qvrtest.Tally("swimtilt_test")

SHOT = "vr_body_pitch;screenshot"
SCRIPT = ";".join([
    "map vrfiringrange", "wait60", "god", "notarget", "vr_body_debug 3", "vr_body_swim_tilt 1",
    "vr_body_swim_tilt_angle 45", "vr_body_swim_tilt_time 0.8", "vr_body_swim_tilt_ledge 64", "wait30",
    "echo swimtilt step dry", SHOT,
    # (gravity 0 and the least upward stick: still in the water, not sinking)
    "sv_gravity 0", "cl_upspeed 1", "+moveup", "setpos 370 560 -180 0 90 0", "noclip", "wait12", "echo swimtilt step mid", SHOT,
    "wait100", "echo swimtilt step swim", SHOT,
    "setpos 370 500 -50 0 90 0", "noclip", "wait120", "echo swimtilt step ledge", SHOT,
    "setpos 370 500 -10 0 90 0", "noclip", "wait120", "echo swimtilt step surface", SHOT,
    "-moveup", "cl_upspeed 200", "sv_gravity 800", "setpos 370 500 -10 0 270 0", "noclip", "+forward", "+jump", "wait20",
    "echo swimtilt step out", SHOT, "-jump", "-forward", "vr_body_debug 0",
    "echo swimtilt done", "toggleconsole", "quit"])


def steps(text):
    out = {}
    step = None
    for line in text.splitlines():
        m = STEP.search(line)
        if m:
            step = m.group(1)
            continue
        m = PITCH.search(line)
        if m and step and step not in out:
            out[step] = {"angle": float(m.group(1)), "target": float(m.group(2)), "source": m.group(4),
                         "wl": int(m.group(5)), "ground": int(m.group(6)), "ledge": float(m.group(7))}
    return out


def check_mode(mode, s):
    def got(step):
        if step not in s:
            T.check(False, "%s %s: no body_pitch line" % (mode, step))
            return None
        return s[step]

    p = got("dry")
    if p:
        T.check(p["angle"] == 0.0 and p["source"] == "none", "%s dry: angle %.2f source %s (0, none)" % (mode, p["angle"], p["source"]))
    p = got("mid")
    if p:
        T.check(p["wl"] == 3 and 1.0 < p["angle"] < 35.0 and p["source"] == "swim",
                "%s mid: waterlevel %d angle %.2f (between 1 and 35, easing in)" % (mode, p["wl"], p["angle"]))
    p = got("swim")
    if p:
        T.check(p["wl"] == 3 and p["angle"] > 44.0 and p["ledge"] < 0,
                "%s swim: waterlevel %d angle %.2f ledge %.1f (about 45, no ledge near)" % (mode, p["wl"], p["angle"], p["ledge"]))
    p = got("ledge")
    if p:
        T.check(p["wl"] == 3 and 0.0 <= p["ledge"] < 64.0 and 0.5 < p["angle"] < 40.0,
                "%s ledge: waterlevel %d ledge %.1f angle %.2f (straightening: between 0.5 and 40)" % (mode, p["wl"], p["ledge"], p["angle"]))
    p = got("surface")
    if p:
        T.check(p["wl"] == 2 and p["angle"] < 0.5, "%s surface: waterlevel %d angle %.2f (upright)" % (mode, p["wl"], p["angle"]))
    p = got("out")
    if p:
        T.check(p["angle"] < 0.5, "%s out: waterlevel %d angle %.2f (upright)" % (mode, p["wl"], p["angle"]))


def main():
    agent = sys.argv[1] if len(sys.argv) > 1 else "swimtilt"
    for mode, extra in (("vr", []), ("flat", ["-ExtraArgs", "-flatscreenmode"])):
        text = qvrtest.run_sh(agent, extra + ["-Clean", "-Out", "swimtilt_%s.png" % mode, "-Script", SCRIPT,
                                              "-Filter", "^(body_pitch|swimtilt|composed)|rror", "-QuitOn", "swimtilt done",
                                              "-Timeout", "180"])
        for line in text.splitlines():
            if line.startswith("composed") or "rror" in line:
                print(line)
        check_mode(mode, steps(text))
    return T.verdict()


if __name__ == "__main__":
    sys.exit(main())
