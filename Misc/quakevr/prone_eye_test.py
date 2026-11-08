"""prone_eye_test.py <agent> -- a high Lying Down Below (vr_prone_eye over the lowest crouched box's top; vr_hull.cpp
proneEyeLimit, proneRiseEye; his note vrtesthall_2026-11-08_16-58-24: 22 still too low, the slider too short) with the
mock headset in vrteleporters' crouching room, the lowest crouched box 36:
  limit     vr_prone_eye 60: the limit held at 48 (the tallest crouched box's top, 52, less 4);
  down      vr_prone_eye 40, the eyes about 38 (on all fours): lying, box 16;
  bob       the eyes about 41 (under 40 + Getting Up Margin 2): still lying;
  margin    Getting Up Margin 8, the eyes about 45 (under 48): still lying;
  up        the eyes about 50 (over 48): up, a crouched box (44 or 52), not lying;
  low       vr_prone_eye 22 (the default), the eyes about 38: crouched, not lying.
Prints "prone_eye_test: PASS/FAIL <what>" and "prone_eye_test: N passed, M failed"; the run ends on its own
"prone_eye done".
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvrtest  # noqa: E402

STATUS = re.compile(r"prone: on (\d) lying (\d) box (\d+) eye (-?[\d.]+) limit (-?[\d.]+)")
STEP = re.compile(r"prone_eye step (\w+)")
T = qvrtest.Tally("prone_eye_test")
M2U = 32.67


def head(eye):
    """The mock head's height (metres) for eyes `eye` units over the feet (14.6 at 0.4 m)."""
    return 0.4 + (eye - 14.6) / M2U


def at(name, eye, before=()):
    return list(before) + ["vr_mock_hand head 0 %.3f 0" % head(eye), "wait20", "echo prone_eye step %s" % name,
                           "vr_prone_status"]


SCRIPT = ";".join(["map vrteleporters", "wait60", "god", "notarget", "vr_crouch_height 36", "vr_crouch_step 8",
                   "vr_prone 1", "setpos 480 -320 24 0 0 0", "wait5", "noclip 0", "wait5"] +
                  at("limit", 60, ["vr_prone_eye 60"]) +
                  ["vr_mock_hand head", "wait20"] +
                  at("down", 38, ["vr_prone_eye 40", "vr_prone_rise_margin 2"]) + at("bob", 41) +
                  at("margin", 45, ["vr_prone_eye 40", "vr_prone_rise_margin 8"]) + at("up", 50) +
                  ["vr_mock_hand head", "wait20"] + at("low", 38, ["vr_prone_eye 22", "vr_prone_rise_margin 2"]) +
                  ["echo prone_eye done", "toggleconsole", "quit"])


def main():
    agent = sys.argv[1] if len(sys.argv) > 1 else "prone"
    text = qvrtest.run_sh(agent, ["-Script", SCRIPT, "-Filter", "^prone|rror", "-QuitOn", "prone_eye done",
                                  "-Timeout", "180"])
    got, step = {}, None
    for line in text.splitlines():
        m = STEP.search(line)
        if m:
            step = m.group(1)
            continue
        m = STATUS.search(line)
        if m and step:
            got[step] = dict(lying=int(m.group(2)), box=int(m.group(3)), eye=float(m.group(4)), limit=float(m.group(5)))
            step = None

    def check(name, ok, what):
        g = got.get(name)
        T.check(bool(g) and ok(g), "%s: %s (%s)" % (name, what, g))

    check("limit", lambda g: g["limit"] == 48.0, "vr_prone_eye 60: the limit 48")
    check("down", lambda g: g["lying"] == 1 and g["box"] == 16, "vr_prone_eye 40, eyes about 38: lying, box 16")
    check("bob", lambda g: g["lying"] == 1, "eyes about 41, under 40 + 2: still lying")
    check("margin", lambda g: g["lying"] == 1, "Getting Up Margin 8, eyes about 45: still lying")
    check("up", lambda g: g["lying"] == 0 and g["box"] in (44, 52), "eyes about 50: up, a crouched box")
    check("low", lambda g: g["lying"] == 0 and g["box"] == 44, "vr_prone_eye 22, eyes about 38: crouched (44), not lying")
    return T.verdict()


if __name__ == "__main__":
    sys.exit(main())
