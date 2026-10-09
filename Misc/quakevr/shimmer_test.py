# shimmer_test.py -- measures temporal shimmer (edge crawl, texture sparkle) in the headset's eye images, per setting.
#
# For each config (a name and console commands), the game (the agent kit's mock headset) loads the map, stands at a
# spot, pauses, and takes the left eye's image at a few head turns a fraction of a pixel apart (0.03 degrees: about half
# a pixel at 2048-pixel eyes). In a region of the image it prints, between consecutive images:
#   step   the mean luminance change (0..255): what any sub-pixel movement changes;
#   pops   the share of pixels changing by more than 16 and more than 32 levels: pixels popping on and off rather than
#          sliding (a thin line broken into dashes that crawl, texels sparkling) -- the shimmer;
#   detail the image's mean luminance gradient (how much detail there is to shimmer).
# The last image repeats the previous turn: its change (floor) must be 0 (nothing else moving).
#
# usage: python Misc/quakevr/shimmer_test.py <agent> [--eye 2048] [--exec cfg] [--map vrstart]
#            [--pos "-1201 -1195 48 0 91 0"] [--look 8] [--region "900 1090 300 70"] [--region2 ...]
#            name="console commands" ...
#   e.g. shimmer_test.py aliasing base="echo base" msaa4="vid_fsaa 4" noretro="vr_retro 0"
# --exec runs a config from the game folder before each (e.g. one with the settings of a report). The region is in a
# 2048-pixel eye's coordinates (scaled to --eye). Screen dithering is turned off (r_dither 0, vr_dither 0): it does not
# move with the image. Uses the kit's run.sh (C:/OHWorkspace/qvr-kit); eye images land in quakevr/eyeshots (overwritten
# by the next run). Standard library only.
# ROUND21.md, "Shimmer at the pier and the bridge".

import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import quakeimage  # noqa: E402

KIT_RUN = "C:/OHWorkspace/qvr-kit/run.sh"
BASH = "C:/Program Files/Git/usr/bin/bash.exe"
YAWS = [0.0, 0.03, 0.06, 0.09, 0.12, 0.12]  # the last repeats: the floor


def load_lum(path, region):
    with open(path, "rb") as f:
        w, h, rgb = quakeimage.read_rgb(f.read(), "png")
    x0, y0, rw, rh = region
    out = []
    for y in range(y0, y0 + rh):
        i = (y * w + x0) * 3
        row = rgb[i : i + rw * 3]
        out.extend((row[j] * 2 + row[j + 1] * 5 + row[j + 2]) / 8.0 for j in range(0, len(row), 3))
    return out, rw, rh


def metrics(frames, rw, rh):
    n = len(frames)
    steps, p16, p32 = [], [], []
    for a, b in zip(frames[: n - 2], frames[1 : n - 1]):
        d = [abs(p - q) for p, q in zip(a, b)]
        steps.append(sum(d) / len(d))
        p16.append(sum(1 for v in d if v > 16) / len(d))
        p32.append(sum(1 for v in d if v > 32) / len(d))
    floor = sum(abs(p - q) for p, q in zip(frames[-2], frames[-1])) / len(frames[-1])
    a = frames[0]
    g = sum(abs(a[y * rw + x + 1] - a[y * rw + x]) + abs(a[(y + 1) * rw + x] - a[y * rw + x])
            for y in range(rh - 1) for x in range(rw - 1)) / ((rw - 1) * (rh - 1))
    k = len(steps)
    return sum(steps) / k, 100 * sum(p16) / k, 100 * sum(p32) / k, floor, g


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("agent")
    ap.add_argument("--eye", type=int, default=2048)
    ap.add_argument("--exec", dest="cfg", default="")
    ap.add_argument("--map", default="vrstart")
    ap.add_argument("--pos", default="-1201 -1195 48 0 91 0")
    ap.add_argument("--look", type=float, default=8.0)
    ap.add_argument("--region", default="900 1090 300 70")
    ap.add_argument("--region2", default="760 1150 540 200")
    ap.add_argument("configs", nargs="+")
    a = ap.parse_args()
    configs = [c.split("=", 1) for c in a.configs]

    s = ["vr_mock_eye_size %d" % a.eye, "vr_restart"]
    for name, cv in configs:
        s += ["echo CONFIG " + name]
        if a.cfg:
            s += ["exec " + a.cfg]
        s += ["r_dither 0", "vr_dither 0", cv, "map " + a.map, "wait30", "setpos " + a.pos, "vr_mock_look %g 0" % a.look,
              "wait40", "pause", "wait5"]
        for y in YAWS:
            s += ["vr_mock_look %g %g" % (a.look, y), "wait3", "vr_eyeshot 1", "wait3"]
        s += ["pause", "wait2"]
    s += ["toggleconsole", "quit"]
    r = subprocess.run([BASH, KIT_RUN, a.agent, "-Script", ";".join(s), "-Filter", "CONFIG|ENGINE|TIMEOUT", "-Timeout", "1800"],
                       capture_output=True, text=True)
    if "ENGINE" in r.stdout or "TIMEOUT" in r.stdout:
        print(r.stdout[-2000:])
        return 1

    shots = "C:/OHWorkspace/qvr-agents/%s/quakevr/eyeshots" % a.agent  # the agent's game folder (run.sh's)
    if not os.path.isdir(shots):
        shots = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "quakevr", "eyeshots")
    k = a.eye / 2048.0
    for label, reg in (("region", a.region), ("region2", a.region2)):
        region = [int(float(v) * k) for v in reg.split()]
        print("%s %s (eye %d):" % (label, region, a.eye))
        for ci, (name, _) in enumerate(configs):
            frames = []
            for fi in range(len(YAWS)):
                lum, rw, rh = load_lum(os.path.join(shots, "%s_%03d_L.png" % (a.map, ci * len(YAWS) + fi)), region)
                frames.append(lum)
            step, p16, p32, floor, g = metrics(frames, rw, rh)
            print("  %-16s step %.2f  pops>16 %.2f%%  pops>32 %.2f%%  floor %.2f  detail %.1f" % (name, step, p16, p32, floor, g))
    return 0


if __name__ == "__main__":
    sys.exit(main())
