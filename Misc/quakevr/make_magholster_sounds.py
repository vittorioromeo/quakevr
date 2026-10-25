#!/usr/bin/env python3
# make_magholster_sounds.py -- the magnetic holsters' sounds (QC vr_magholster.qc; docs/vr-port/ROUND22.md, "Magnetic
# holsters: chain origin, long throws, sounds, the hand point"), synthesised:
#   quakevr/sound/vr/magholster_zip.wav    a bound weapon flying home: a spring reel winding its chain back in, a
#                                          ratchet's ticks speeding up over a rising whirr (0.42 s: the flight's 0.4 s;
#                                          the holster's own click as it lands)
#   quakevr/sound/vr/magholster_draw.wav   a bound weapon drawn from its holster, over the holster's sound: the chain
#                                          paid out, a few light link clinks and the reel's slow ticks
# 22050 Hz, mono, 16 bit. Deterministic (fixed seeds). Quiet by design: the volumes are vr_magholster_zip_volume and
# vr_magholster_draw_volume.
#
# Usage: python Misc/quakevr/make_magholster_sounds.py [output game folder]   (default: ../../quakevr)

import os
import sys
import wave

import numpy as np

import genguard

RATE = 22050


def write(path, x):
    x = np.clip(x, -1, 1)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((x * 32767).astype(np.int16).tobytes())


def tick(rng, pitch, length=0.012):
    """A ratchet pawl's tick: a short burst of noise rung through two small metal partials."""
    n = int(length * RATE)
    t = np.arange(n) / RATE
    env = np.exp(-t / (length / 5))
    noise = rng.standard_normal(n) * np.exp(-t / 0.0015)
    ring = np.sin(2 * np.pi * 3100 * pitch * t) + 0.6 * np.sin(2 * np.pi * 5300 * pitch * t + 1.1)
    return (0.55 * noise + 0.45 * ring) * env


def clink(rng, pitch, length=0.06):
    """A chain link against the next: a few inharmonic partials, quickly gone."""
    n = int(length * RATE)
    t = np.arange(n) / RATE
    out = np.zeros(n)
    for f, a, d in ((2650, 1.0, 0.018), (4170, 0.7, 0.012), (6230, 0.45, 0.008), (7810, 0.3, 0.006)):
        out += a * np.sin(2 * np.pi * f * pitch * t + rng.uniform(0, 6.28)) * np.exp(-t / d)
    out[:24] *= np.linspace(0, 1, 24)
    return out * 0.5


def place(out, x, at):
    i = int(at * RATE)
    end = min(len(out), i + len(x))
    if end > i:
        out[i:end] += x[:end - i]


def bandpass_noise(rng, n, f0, f1, q=6.0):
    """Noise through a resonator swept from f0 to f1 (Hz) over its length (a whirr rising as the reel spins up)."""
    noise = rng.standard_normal(n)
    y = np.zeros(n)
    y1 = y2 = 0.0
    for i in range(n):
        f = f0 + (f1 - f0) * (i / n)
        w = 2 * np.pi * f / RATE
        r = np.exp(-w / (2 * q))
        a1 = 2 * r * np.cos(w)
        a2 = -r * r
        v = (1 - r) * noise[i] + a1 * y1 + a2 * y2
        y2, y1 = y1, v
        y[i] = v
    return y / (np.max(np.abs(y)) + 1e-9)


def zip_sound():
    rng = np.random.default_rng(2217)
    length = 0.42
    n = int(length * RATE)
    out = np.zeros(n)
    # The ratchet: ticks 32 ms apart at first, 7 ms at the end (the reel speeding up), each a little higher.
    at = 0.0
    k = 0
    while at < length - 0.02:
        u = at / length
        place(out, tick(rng, 0.9 + 0.35 * u) * (0.5 + 0.5 * u), at)
        at += 0.032 * (1 - u) + 0.007 * u
        k += 1
    # The whirr under it: rising, swelling, then cut off as the chain runs out.
    t = np.arange(n) / RATE
    swell = np.clip(t / 0.3, 0, 1) ** 1.5 * np.clip((length - t) / 0.03, 0, 1)
    out += 0.35 * bandpass_noise(rng, n, 900, 2600) * swell
    # A soft hiss of the chain through the holster's mouth near the end.
    hiss = rng.standard_normal(n) * np.clip((t - 0.3) / 0.1, 0, 1) * np.clip((length - t) / 0.02, 0, 1)
    out += 0.08 * hiss
    out *= 0.6 / np.max(np.abs(out))
    return out


def draw_sound():
    rng = np.random.default_rng(3319)
    length = 0.3
    out = np.zeros(int(length * RATE))
    for at, p, a in ((0.0, 1.0, 1.0), (0.035, 1.07, 0.7), (0.08, 0.95, 0.55), (0.14, 1.12, 0.4)):
        place(out, clink(rng, p) * a, at)
    # The reel paying out: three slow ticks.
    for i, at in enumerate((0.02, 0.07, 0.13)):
        place(out, tick(rng, 0.8) * (0.35 - 0.08 * i), at)
    out *= 0.55 / np.max(np.abs(out))
    return out


def main(args):
    here = os.path.dirname(os.path.abspath(__file__))
    game = args[0] if args else os.path.join(here, "..", "..", "quakevr")
    sounds = os.path.join(game, "sound", "vr")
    made = (("magholster_zip", zip_sound), ("magholster_draw", draw_sound))
    guard = genguard.Guard("make_magholster_sounds.py", [os.path.join(sounds, n + ".wav") for n, _ in made])
    os.makedirs(sounds, exist_ok=True)
    for name, fn in made:
        wav = os.path.join(sounds, name + ".wav")
        if wav in guard.kept:
            continue
        write(wav, fn())
        print("%s.wav -> %s" % (name, os.path.normpath(wav)))
    guard.finish()


if __name__ == "__main__":
    main(sys.argv[1:])
