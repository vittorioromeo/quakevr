#!/usr/bin/env python3
# make_autopump_sounds.py -- the shotgun's auto pump's two clacks (vr_autopump.cpp; docs/vr-port/ROUND21.md, "Shotgun
# auto pump"), cut out of zer0_sol's "Shotgun Reload Sound effects", CC0 (public domain; see docs/vr-port/CREDITS.md),
# into quakevr/sound/vr/autopump_back.wav and autopump_home.wav:
#
#   page:      https://opengameart.org/content/shotgun-reload-sound-effects
#   used:      ShotgunSounds/Rack.mp3 (44.1 kHz mono MP3): a pump shotgun racked once, back and home
#
# The recordings are not in the repository: unpack the download and run (PyAV decodes the MP3; numpy)
#   python Misc/quakevr/make_autopump_sounds.py "<...>/ShotgunSounds" [out folder, default quakevr/sound/vr]
#
# The rack is two clusters: the action unlocking (a sharp click, at 0.639 s) and the slide running back (to 0.81 s),
# then the slide run home and slammed shut (its loudest moment about 0.91 s). The engine plays the first as the stroke
# starts and the second its LEAD seconds before the stroke ends, so the slam is the fore-end arriving home:
#   autopump_back.wav  the click and the slide's first run: PRE_BACK before the click, BACK long in all
#   autopump_home.wav  the slam: LEAD before its loudest sample, HOME long in all (vr_autopump.cpp homeSoundLead)
# Each: high-passed at 30 Hz (first order), kept at 44100 Hz 16-bit (the clicks are bright), faded in over its first
# 5 ms and out over its last 30 ms (cosines), its peak brought to PEAK (the recording clips: its peaks are rounded off
# by a soft knee first); vr_autopump_sound scales them in the game (0.5 by default: well under the shot).

import os
import struct
import sys

import av
import numpy as np

RATE = 44100
PRE_BACK = 0.012
BACK = 0.105      # about the stroke's way back at its default 0.3 s (35%)
LEAD = 0.053      # vr_autopump.cpp homeSoundLead
HOME = 0.140
FADE_IN = 0.005
FADE_OUT = 0.030
PEAK = 0.7


def load(path):
    c = av.open(path)
    s = c.streams.audio[0]
    assert s.codec_context.sample_rate == RATE, path
    frames = list(c.decode(s))
    return np.concatenate([f.to_ndarray() for f in frames], axis=1).astype(np.float64).mean(axis=0)


def high_pass(x, hz=30.0):
    rc = 1.0 / (2 * np.pi * hz)
    a = rc / (rc + 1.0 / RATE)
    y = np.zeros_like(x)
    for i in range(1, len(x)):
        y[i] = a * (y[i - 1] + x[i] - x[i - 1])
    return y


def soft_knee(y, knee=0.6):
    """Peaks over `knee` (of the clip's own peak) rounded off towards it (tanh)."""
    m = np.abs(y).max()
    k = knee * m
    over = np.abs(y) > k
    out = y.copy()
    out[over] = np.sign(y[over]) * (k + (m - k) * np.tanh((np.abs(y[over]) - k) / (m - k)))
    return out


def cut(x, start, length):
    a = int(round(start * RATE))
    y = x[a:a + int(round(length * RATE))].copy()
    n_in, n_out = int(FADE_IN * RATE), int(FADE_OUT * RATE)
    y[:n_in] *= 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, n_in))
    y[-n_out:] *= 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, n_out))
    y = soft_knee(y)
    return y * (PEAK / np.abs(y).max())


def write_wav(path, y):
    pcm = np.clip(np.round(y * 32767), -32768, 32767).astype("<i2").tobytes()
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(pcm)) + pcm)


def peak_in(x, t0, t1):
    a, b = int(t0 * RATE), int(t1 * RATE)
    return (a + int(np.argmax(np.abs(x[a:b])))) / RATE


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__ if __doc__ else "usage: make_autopump_sounds.py <ShotgunSounds folder> [out folder]")
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                                                               "quakevr", "sound", "vr")
    x = high_pass(load(os.path.join(src, "Rack.mp3")))
    click = peak_in(x, 0.60, 0.67)
    slam = peak_in(x, 0.86, 0.96)
    for name, start, length in (("autopump_back.wav", click - PRE_BACK, BACK), ("autopump_home.wav", slam - LEAD, HOME)):
        y = cut(x, start, length)
        write_wav(os.path.join(out, name), y)
        print("%-18s Rack.mp3 %.4f-%.4f s" % (name, start, start + length))


if __name__ == "__main__":
    main()
