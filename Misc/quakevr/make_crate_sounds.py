#!/usr/bin/env python3
# make_crate_sounds.py -- the wooden crates' sounds (QC vr_crates.qc; docs/vr-port/ROUND21.md, "Wooden crates"), made
# from the physics sounds' wood knocks (quakevr/sound/vr/phys/wood_*.wav: Kenney's Impact Sounds, CC0; CREDITS.md)
# and synthesised splintering:
#   quakevr/sound/vr/crate_break1..3.wav   a crate breaking: two or three heavy wood knocks a few milliseconds apart,
#                                          pitched down a little, over a crackle of splinters and a low thump
#   quakevr/sound/vr/crate_dust1..2.wav    a piece bursting into dust: a light knock pitched up, a short crackle and the
#                                          hiss of the dust
# 22050 Hz, mono, 16 bit, as the physics sounds. Deterministic (fixed seeds).
#
# Usage: python Misc/quakevr/make_crate_sounds.py [output game folder]

import os
import sys
import wave

import numpy as np

RATE = 22050


def read(path):
    with wave.open(path) as w:
        assert w.getnchannels() == 1 and w.getsampwidth() == 2
        data = np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(np.float64) / 32768.0
        if w.getframerate() != RATE:
            data = resample(data, RATE / w.getframerate())
    return data


def write(path, x):
    x = np.clip(x, -1, 1)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((x * 32767).astype(np.int16).tobytes())


def resample(x, factor):
    """Stretched by `factor` (2: twice as long, an octave down)."""
    n = int(len(x) * factor)
    return np.interp(np.arange(n) / factor, np.arange(len(x)), x)


def place(out, x, at):
    i = int(at * RATE)
    end = min(len(out), i + len(x))
    out[i:end] += x[:end - i]


def onepole_lp(x, cutoff):
    a = np.exp(-2 * np.pi * cutoff / RATE)
    y = np.zeros_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc = (1 - a) * v + a * acc
        y[i] = acc
    return y


def click(rng, length, bright):
    """One splinter: a short burst of noise, decaying, high-passed by `bright` (0..1)."""
    n = int(length * RATE)
    noise = rng.normal(0, 1, n)
    hp = noise - onepole_lp(noise, 1500 + 3000 * (1 - bright))
    env = np.exp(-np.arange(n) / (n * 0.25))
    return hp * env


def crackle(rng, dur, count, loud):
    out = np.zeros(int(dur * RATE) + RATE // 10)
    times = np.sort(rng.exponential(dur * 0.35, count))
    for t in times:
        if t < dur:
            c = click(rng, rng.uniform(0.002, 0.007), rng.uniform(0.3, 1.0))
            place(out, c * loud * rng.uniform(0.3, 1.0) * np.exp(-t / dur * 2.0), t)
    return out


def normalise(x, peak=0.9):
    m = np.max(np.abs(x))
    return x * (peak / m) if m > 0 else x


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    game = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr")
    phys = os.path.join(here, "..", "..", "quakevr", "sound", "vr", "phys")
    out_dir = os.path.join(game, "sound", "vr")
    os.makedirs(out_dir, exist_ok=True)
    heavy = [read(os.path.join(phys, "wood_h%d.wav" % k)) for k in range(1, 5)]
    medium = [read(os.path.join(phys, "wood_m%d.wav" % k)) for k in range(1, 5)]
    light = [read(os.path.join(phys, "wood_l%d.wav" % k)) for k in range(1, 5)]

    for k in range(3):
        rng = np.random.default_rng(900 + k)
        out = np.zeros(int(0.7 * RATE))
        # The knocks: the boards giving way one after another.
        picks = rng.permutation(4)[:2 + (k != 1)]
        t = 0.0
        for j, p in enumerate(picks):
            src = heavy[p] if j == 0 else medium[p]
            place(out, resample(src, rng.uniform(1.05, 1.25)) * (1.0 if j == 0 else 0.7), t)
            t += rng.uniform(0.018, 0.05)
        # The splinters, and a low thump under the first knock.
        place(out, crackle(rng, 0.35, 70, 0.45), 0.0)
        thump = np.sin(2 * np.pi * np.cumsum(np.linspace(95, 55, int(0.12 * RATE))) / RATE)
        thump *= np.exp(-np.arange(len(thump)) / (0.035 * RATE))
        place(out, thump * 0.5, 0.0)
        fade = np.ones(len(out))
        fade[-int(0.1 * RATE):] = np.linspace(1, 0, int(0.1 * RATE))
        write(os.path.join(out_dir, "crate_break%d.wav" % (k + 1)), normalise(out * fade))

    for k in range(2):
        rng = np.random.default_rng(950 + k)
        out = np.zeros(int(0.45 * RATE))
        place(out, resample(light[k * 2], rng.uniform(0.75, 0.85)) * 0.8, 0.0)
        place(out, crackle(rng, 0.12, 18, 0.35), 0.0)
        dust = rng.normal(0, 1, len(out))
        dust = dust - onepole_lp(dust, 2500)
        dust = onepole_lp(dust, 7000) * np.exp(-np.arange(len(out)) / (0.09 * RATE)) * 0.12
        out += dust
        fade = np.ones(len(out))
        fade[-int(0.08 * RATE):] = np.linspace(1, 0, int(0.08 * RATE))
        write(os.path.join(out_dir, "crate_dust%d.wav" % (k + 1)), normalise(out * fade, 0.7))
    print("crate_break1..3.wav, crate_dust1..2.wav -> " + out_dir)


if __name__ == "__main__":
    main()
