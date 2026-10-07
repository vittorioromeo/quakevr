#!/usr/bin/env python3
# make_reload_shell_sounds.py -- cuts immersive reloading's shotgun shell insert (QC vr_reload.qc VR_Reload_Load,
# VR_Reload_ShellInSound; docs/vr-port/ROUND21.md) out of zer0_sol's "Shotgun Reload Sound effects", CC0 (public
# domain; see docs/vr-port/CREDITS.md), into quakevr/sound/vr/reload_shell_in.wav, reload_shell_in_2.wav and
# reload_shell_in_3.wav:
#
#   page:      https://opengameart.org/content/shotgun-reload-sound-effects
#   used:      ShotgunSounds/First Shell.mp3, Subsequent Shells.mp3 and 5 Shell Reload.mp3 (44.1 kHz mono MP3s)
#
# The recordings are not in the repository: unpack the download and run (PyAV decodes the MP3s; numpy)
#   python Misc/quakevr/make_reload_shell_sounds.py "<...>/ShotgunSounds" [out folder, default quakevr/sound/vr]
#
# Each shell in the takes is two clusters: the shell handled at the loading port (rattling clicks), then pushed into
# the tube (a scrape rising as the spring gives) ending in the shell latch's click on its rim. Only the second is cut:
# the clip starts PRE before the latch's click (its loudest sample) and runs on TAIL after it:
#   reload_shell_in.wav    First Shell.mp3        click 0.8001 s (no after-tick)
#   reload_shell_in_2.wav  Subsequent Shells.mp3  click 1.1487 s (the thumb's tick 0.11 s after it kept)
#   reload_shell_in_3.wav  5 Shell Reload.mp3     the fourth shell's, click 4.5954 s (its tick kept too)
# Three, picked at random per insert (VR_Reload_ShellInSound), so a tube filled shell by shell does not repeat.
#
# Each: high-passed at 30 Hz (first order; the takes have no low end anyway), kept at 44100 Hz 16-bit (5-8% of the
# clicks' energy is above 11 kHz, which 22050 Hz would lose), faded in over its first 15 ms and out over its last
# 30 ms (cosines), brought to the A-weighted loudness of the synthesised insert it replaces (-14.6 dB over its loud
# frames, those within 20 dB of the loudest; Quake's weapons/guncock.wav: -18.1) with the latch's click rounded off
# under -1 dBFS by make_burst_sounds.py's soft knee. The "full" cue (reload_full.wav, a low double knock) and the
# "can't" click (reload_blocked.wav) stay synthesised (make_sounds.py): the full knock layers under the last insert's
# click, giving the bright recording weight; the takes' "Shell in Chamber" slide-close is a long bright triple click
# that would smear it and sounds like the action being cycled.

import os
import struct
import sys

import av
import numpy as np

RATE = 44100
PRE = 0.080  # seconds of scrape before the latch's click
FADE_IN = 0.015
FADE_OUT = 0.030
TARGET_A_DB = -14.6  # the synthesised reload_shell_in.wav's A-weighted loudness (see aweighted_db)
CEILING = 0.89  # -1 dBFS

# (output, source, the latch's click in seconds, seconds after it)
CUTS = [
    ("reload_shell_in.wav", "First Shell.mp3", 0.8001, 0.100),
    ("reload_shell_in_2.wav", "Subsequent Shells.mp3", 1.1487, 0.150),
    ("reload_shell_in_3.wav", "5 Shell Reload.mp3", 4.5954, 0.150),
]


def load(path):
    c = av.open(path)
    s = c.streams.audio[0]
    assert s.codec_context.sample_rate == RATE, path
    frames = list(c.decode(s))
    assert frames and frames[0].format.is_planar, path  # the MP3 decoder's float planar: (channels, samples)
    return np.concatenate([f.to_ndarray() for f in frames], axis=1).astype(np.float64).mean(axis=0)


def high_pass(x, hz=30.0):
    k = np.exp(-2 * np.pi * hz / RATE)
    out = np.empty_like(x)
    prev_in = prev_out = 0.0
    for i, v in enumerate(x):
        prev_out = k * (prev_out + v - prev_in)
        prev_in = v
        out[i] = prev_out
    return out


def cut(a, click, tail, pad=0.05):
    """From PRE before the click to `tail` after it, high-passed (with `pad` around it to settle), faded."""
    start = int(round((click - PRE) * RATE))
    end = int(round((click + tail) * RATE))
    p = int(pad * RATE)
    y = high_pass(a[start - p:end])[p:]
    i, o = int(FADE_IN * RATE), int(FADE_OUT * RATE)
    y[:i] *= 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, i, endpoint=False))
    y[len(y) - o:] *= 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, o))
    return y


def soft_limit(y, ceiling=CEILING):
    """Below 0.7 x the ceiling untouched; above, a tanh knee up to the ceiling (make_burst_sounds.py's)."""
    knee = 0.7 * ceiling
    s = np.sign(y)
    m = np.abs(y)
    over = m > knee
    m[over] = knee + (ceiling - knee) * np.tanh((m[over] - knee) / (ceiling - knee))
    return s * m


def aweighted_db(y, rate=RATE, hop=256):
    """A-weighted RMS (0 dB at 1 kHz) over the frames within 20 dB of the loudest, in dBFS."""
    f = np.fft.rfftfreq(len(y), 1 / rate)
    f2 = f ** 2

    def ra(f2):
        return (12194 ** 2 * f2 ** 2) / ((f2 + 20.6 ** 2) * np.sqrt((f2 + 107.7 ** 2) * (f2 + 737.9 ** 2)) * (f2 + 12194 ** 2) + 1e-30)

    w = np.fft.irfft(np.fft.rfft(y) * ra(f2) / ra(1000.0 ** 2), len(y))
    fr = w[:len(w) // hop * hop].reshape(-1, hop)
    r = np.sqrt((fr ** 2).mean(axis=1))
    return 20 * np.log10(np.sqrt(np.mean(fr[r > r.max() * 0.1] ** 2)) + 1e-12)


def write_wav(path, y):
    pcm = np.clip(np.round(y * 32767), -32768, 32767).astype("<i2").tobytes()
    body = b"WAVE" + b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16) + b"data" + struct.pack("<I", len(pcm)) + pcm
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", len(body)) + body)


def main():
    src = sys.argv[1]
    out_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "..", "..", "quakevr", "sound", "vr")
    for name, take, click, tail in CUTS:
        y = cut(load(os.path.join(src, take)), click, tail)
        g = 1.0
        for _ in range(30):  # the soft knee after the gain: iterated
            g *= 10 ** ((TARGET_A_DB - aweighted_db(soft_limit(y * g))) / 20)
        over = np.mean(np.abs(y * g) > 0.7 * CEILING) * 100
        y = soft_limit(y * g)
        write_wav(os.path.join(out_dir, name), y)
        print(f"{name}: {len(y) / RATE:.3f} s, gain {20 * np.log10(g):+.1f} dB (knee {over:.2f}% of samples), "
              f"A-weighted {aweighted_db(y):.1f} dB, peak {20 * np.log10(np.max(np.abs(y))):.1f} dBFS")


if __name__ == "__main__":
    main()
