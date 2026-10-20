#!/usr/bin/env python3
"""mantle_height_test.py <agent> -- Mantle Height (vr_climb_mantle_height; ROUND22.md, "Mantle height, one-hand drag by
part, shallower dents").

vrslopes' level top: both hands take its lip at 1.49 m (tracking) and pull down by `d` metres over 2 s, held 3 s more
(climb_plays.pullup, never let go). For each Mantle Height (8, the old fixed point, and the default) the pulls tried
in turn; prints which mantled and the head's height over the top as each did ("climb: mantle ... the head X over the
top"). PASS: every pull that mantles at the default also mantles at 8, the default's shallowest mantling pull is deeper
than 8's (a lean between the two is no mantle), and the head was over the top by the height when it mantled.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import climb_plays  # noqa: E402

AGENT = sys.argv[1] if len(sys.argv) > 1 else "fastship"
KIT = os.environ.get("KIT", "C:/OHWorkspace/qvr-kit")
W = f"C:/OHWorkspace/qvr-agents/{AGENT}"
HY = 1.49
PULLS = [0.20, 0.25, 0.30, 0.35, 0.40, 0.45, 0.50, 0.60, 0.80]
HEIGHTS = ["8", ""] + [h for h in os.environ.get("MH_HEIGHTS", "").split(",") if h]  # ("": the default)


def run(height):
    os.makedirs(os.path.join(W, "quakevr", "motions"), exist_ok=True)
    s = "vr_climb 1;map vrslopes;wait30;god;vr_climb_debug 1" + (f";vr_climb_mantle_height {height}" if height else "")
    for d in PULLS:
        name = f"mantleheight_{int(d * 100)}"
        L, _ = climb_plays.pullup(HY, HY - d, fwd_to=-0.69, hold=5.0)
        climb_plays.write(os.path.join(W, "quakevr", "motions", name + ".txt"), L)
        s += (f";echo == {d:.2f};setpos 48 -18 24 0 90 0;noclip;wait40;vr_mock_play quakevr/motions/{name}.txt;wait420;"
              "-graboff;-grabmain;vr_mock_play;wait20")
    bash = os.environ.get("BASH_EXE", "C:/Program Files/Git/bin/bash.exe")
    out = subprocess.run([bash, f"{KIT}/run.sh", AGENT, "-Script", s + ";toggleconsole;quit", "-Timeout", "400",
                          "-Filter", "rror|==|climb: mantle from"],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True).stdout
    with open(os.path.join(W, "scratch", f"mantleheight_{height or 'default'}.txt"), "w") as f:
        f.write(out)
    res, cur = {}, None
    for line in out.splitlines():
        m = re.search(r"== ([\d.]+)", line)
        if m:
            cur = float(m.group(1))
            continue
        m = re.search(r"the head ([-\d.]+) over the top \(Mantle Height (\d+)\)", line)
        if m and cur is not None and cur not in res:
            res[cur] = (float(m.group(1)), float(m.group(2)))
    return res


def main():
    results = {h or "default": run(h) for h in HEIGHTS}
    for h, res in results.items():
        print(f"Mantle Height {h}: " + ", ".join(
            f"{d:.2f} m: {'head %.1f' % res[d][0] if d in res else 'no mantle'}" for d in PULLS))
    old, new = results["8"], results["default"]
    first_old = min(old) if old else None
    first_new = min(new) if new else None
    ok = bool(old) and bool(new) and set(new) <= set(old) and first_new > first_old
    ok = ok and all(v[0] >= v[1] - 0.05 for r in results.values() for v in r.values())
    print(f"{'PASS' if ok else 'FAIL'}: the shallowest mantling pull {first_old} m at 8, {first_new} m at the default")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
