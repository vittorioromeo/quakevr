#!/usr/bin/env python3
# make_reload_ssg_sounds.py -- cuts immersive reloading's super shotgun break-open and close (QC vr_reload.qc
# VR_Reload_SsgBreak, VR_Reload_SsgClose; phase 2b) out of zer0_sol's "Shotgun Reload Sound effects", CC0 (public
# domain; see docs/vr-port/CREDITS.md), the same pack as the shell inserts (make_reload_shell_sounds.py):
#
#   page:      https://opengameart.org/content/shotgun-reload-sound-effects
#   used:      ShotgunSounds/Rack.mp3 and Shell in Chamber.mp3 (44.1 kHz mono MP3s)
#
# The recordings are not in the repository: unpack the download and run (PyAV decodes the MP3s; numpy)
#   python Misc/quakevr/make_reload_ssg_sounds.py "<...>/ShotgunSounds" [out folder, default quakevr/sound/vr]
#
#   reload_ssg_open.wav   Rack.mp3, 0.60-1.02 s: the action pulled back and run forward, "shk-chk": the top lever thrown
#                         and the barrels dropping against their stop
#   reload_ssg_close.wav  Shell in Chamber.mp3, 1.06-1.34 s: its last, heaviest slam (a tick, then the action home):
#                         the barrels snapped shut and the lever's latch closing on them
#
# Each high-passed at 30 Hz, faded in and out, brought to an A-weighted loudness a little above the shell insert's (the
# action is heavier: the open -12.6 dB, the close -11.6 dB; the insert -14.6) with make_reload_shell_sounds.py's soft
# knee under -1 dBFS; 44100 Hz 16-bit mono.

import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_reload_shell_sounds as shell  # noqa: E402

RATE = shell.RATE
FADE_IN = 0.008
FADE_OUT = 0.04

# (output, source, start s, end s, A-weighted target dB)
CUTS = [
    ("reload_ssg_open.wav", "Rack.mp3", 0.60, 1.02, -12.6),
    ("reload_ssg_close.wav", "Shell in Chamber.mp3", 1.06, 1.34, -11.6),
]


def cut(a, start, end, pad=0.05):
    s, e, p = int(start * RATE), int(end * RATE), int(pad * RATE)
    y = shell.high_pass(a[max(0, s - p):e])[s - max(0, s - p):]
    i, o = int(FADE_IN * RATE), int(FADE_OUT * RATE)
    y[:i] *= 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, i, endpoint=False))
    y[len(y) - o:] *= 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, o))
    return y


def main():
    src = sys.argv[1]
    out_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "..", "..", "quakevr", "sound", "vr")
    for name, take, start, end, target in CUTS:
        y = cut(shell.load(os.path.join(src, take)), start, end)
        g = 1.0
        for _ in range(30):  # the soft knee after the gain: iterated
            g *= 10 ** ((target - shell.aweighted_db(shell.soft_limit(y * g))) / 20)
        y = shell.soft_limit(y * g)
        shell.write_wav(os.path.join(out_dir, name), y)
        print(f"{name}: {len(y) / RATE:.3f} s, gain {20 * np.log10(g):+.1f} dB, A-weighted {shell.aweighted_db(y):.1f} dB, "
              f"peak {20 * np.log10(np.max(np.abs(y))):.1f} dBFS")


if __name__ == "__main__":
    main()
