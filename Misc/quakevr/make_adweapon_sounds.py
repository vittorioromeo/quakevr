#!/usr/bin/env python3
# make_adweapon_sounds.py -- three sounds for Arcane Dimensions' weapons in Quake VR (docs/vr-port/ROUND22.md, "Widowmaker
# Gatling"), synthesized here from nothing but noise and sines (no recording: nothing to credit), into quakevr/sound/vr/:
#
#   widowmaker_shot.wav  the Widowmaker's shot (QC AD_FireWidowmaker), "very, very powerful sounding" (his notes,
#                        2026-10-19): a sharp crack, a muzzle blast, a deep thump falling 170 -> 48 Hz over a 34 Hz
#                        sub, a boom and a rolling low tail (~1 s), the action's two clacks (75 and 115 ms), all driven
#                        into a soft knee: denser than the super shotgun and the rocket launcher (weapons/shotgn2.wav,
#                        sgun1.wav: RMS -4.2 dB over their first 0.3 s, -7.1 dB under 150 Hz; this -3.9 and -4.9),
#                        never clipped (peak 0.97).
#   widowmaker_motor.wav the barrels' motor, a seamless 1 s loop (a cue point: Quake loops it): a hum, a gear whine,
#                        the barrels' rattle and brush hiss; the engine plays it faster and louder as they spin
#                        (vr_adweapons.cpp motorFrame, vr_ad_widowmaker_motor).
#   xbow_bolt_in.wav     the AD crossbow's next bolt morphing into its groove (vr_adcrossbow.cpp, its volume
#                        vr_adgun_crossbow_bolt_sound): a soft rising shimmer, subtle.
#
#   python Misc/quakevr/make_adweapon_sounds.py [out folder, default quakevr/sound/vr]
#
# 22050 Hz 16-bit mono; a fixed random seed (the same files every run). Pure Python (no numpy).

import math
import os
import random
import struct
import sys

RATE = 22050


def write_wav(path, samples, loop=False):
    """A 16-bit mono WAV; `loop`: with a cue point at 0 (Quake's loop start, snd_mem.c GetWavinfo: it plays on until
    stopped)."""
    peak = max(1e-9, max(abs(s) for s in samples))
    data = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in samples)
    cue = b""
    if loop:
        cue = b"cue " + struct.pack("<II", 28, 1) + struct.pack("<II4sIII", 1, 0, b"data", 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(cue) + len(data)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16))
        f.write(cue)
        f.write(b"data" + struct.pack("<I", len(data)) + data)
    print("%s: %d samples (%.3f s), peak %.3f%s" % (path, len(samples), len(samples) / RATE, peak, ", looped" if loop else ""))


def low_pass(x, hz):
    a = 1.0 - math.exp(-2.0 * math.pi * hz / RATE)
    y, out = 0.0, []
    for v in x:
        y += a * (v - y)
        out.append(y)
    return out


def high_pass(x, hz):
    lp = low_pass(x, hz)
    return [v - l for v, l in zip(x, lp)]


def noise(n, rng):
    return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def normalize(x, peak):
    m = max(1e-9, max(abs(v) for v in x))
    return [v * peak / m for v in x]


def fade_out(x, seconds):
    n = int(seconds * RATE)
    for i in range(n):
        k = len(x) - n + i
        x[k] *= 0.5 + 0.5 * math.cos(math.pi * i / n)
    return x


def clack(n, at, partials, tau, amp, rng):
    """A metallic clack at `at` s: damped sines (hz) and a short click of noise."""
    out = [0.0] * n
    start = int(at * RATE)
    click = high_pass(noise(int(0.004 * RATE), rng), 2500.0)
    for i in range(start, n):
        t = (i - start) / RATE
        v = sum(math.sin(2.0 * math.pi * hz * t + ph) for hz, ph in partials) / len(partials) * math.exp(-t / tau)
        if i - start < len(click):
            v += click[i - start] * math.exp(-t / 0.0012) * 1.5
        out[i] = v * amp
    return out


def widowmaker_shot():
    rng = random.Random(3412)
    n = int(1.25 * RATE)
    crack = high_pass(noise(n, rng), 1800.0)
    blast = low_pass(low_pass(noise(n, rng), 1400.0), 1400.0)
    boom = low_pass(low_pass(noise(n, rng), 260.0), 260.0)
    rumble = low_pass(low_pass(noise(n, rng), 120.0), 120.0)
    room = low_pass(noise(n, rng), 700.0)
    out = []
    thump = sub = 0.0
    for i in range(n):
        t = i / RATE
        attack = 1.0 - math.exp(-t / 0.0004)
        # The chest punch: a sine dropping 170 -> 48 Hz in the first tens of ms, held long; under it a 34 Hz sub.
        thump += 2.0 * math.pi * (48.0 + 122.0 * math.exp(-t / 0.018)) / RATE
        sub += 2.0 * math.pi * (34.0 + 20.0 * math.exp(-t / 0.05)) / RATE
        v = crack[i] * (math.exp(-t / 0.0025) * 2.2 + math.exp(-t / 0.012) * 0.35)  # the sharp crack
        v += blast[i] * 6.0 * math.exp(-t / 0.035) * attack                          # the muzzle blast
        v += math.sin(thump) * 1.6 * math.exp(-t / 0.11) * attack                    # the thump
        v += math.sin(sub) * 0.9 * math.exp(-t / 0.28) * attack                      # the sub's weight
        v += boom[i] * 30.0 * math.exp(-t / 0.22) * attack                           # the boom
        v += rumble[i] * 95.0 * math.exp(-t / 0.45) * (1.0 - math.exp(-t / 0.03))    # its rolling tail
        v += room[i] * 1.1 * math.exp(-t / 0.3) * (1.0 - math.exp(-t / 0.02))        # the room's answer
        out.append(v)
    back = clack(n, 0.075, [(2300.0, 0.0), (3650.0, 1.0), (5100.0, 2.0)], 0.005, 0.5, rng)
    home = clack(n, 0.115, [(1650.0, 0.5), (2850.0, 1.5), (420.0, 0.0)], 0.008, 0.7, rng)
    out = normalize(out, 1.0)
    out = [a + b + c for a, b, c in zip(out, back, home)]
    # Driven hard into a soft knee (Quake's own weapon sounds are as dense: their RMS a few dB under full scale), never
    # clipped: the boom and the tail come up with the punch.
    out = [math.tanh(SHOT_DRIVE * v) for v in normalize(out, 1.0)]
    return fade_out(normalize(out, 0.97), 0.12)


SHOT_DRIVE = 4.5


def loop_noise(n, cross, rng, filt):
    """Filtered noise `n` samples long that loops seamlessly (its end crossfaded into what leads into its start)."""
    y = filt(noise(n + 2 * cross, rng))
    y = y[cross:]  # (the filters settled)
    out = y[cross:cross + n]
    for i in range(n - cross, n):
        w = (i - (n - cross)) / cross
        out[i] = out[i] * (1.0 - w) + y[i + cross - n] * w
    return out


def widowmaker_motor():
    """The barrels' motor (vr_adweapons.cpp motorFrame): a loop, played faster and louder as they spin up. An electric
    motor's hum and its harmonics, a gear whine above, the barrels' rattle at their passing rate and brush hiss. Every
    tone a whole number of cycles in its second: the loop seamless."""
    rng = random.Random(5150)
    n = RATE  # one second
    hiss = loop_noise(n, 2205, rng, lambda x: high_pass(low_pass(x, 5000.0), 1200.0))
    rattle_noise = loop_noise(n, 2205, rng, lambda x: high_pass(low_pass(x, 2600.0), 500.0))
    out = []
    for i in range(n):
        t = i / RATE
        hum = (math.sin(2 * math.pi * 92 * t) + 0.6 * math.sin(2 * math.pi * 184 * t + 0.4) +
               0.35 * math.sin(2 * math.pi * 276 * t + 1.1) + 0.2 * math.sin(2 * math.pi * 368 * t + 2.0))
        whine = 0.55 * math.sin(2 * math.pi * 1104 * t) + 0.25 * math.sin(2 * math.pi * 2208 * t + 0.9)
        whine *= 1.0 + 0.15 * math.sin(2 * math.pi * 6 * t)  # (a slow beat, alive)
        # The barrels going by: 23 clicks a second, each a short burst of the rattle's noise.
        ph = (23.0 * t) % 1.0
        rattle = rattle_noise[i] * (math.exp(-ph / 0.06) * 1.6 + 0.15)
        out.append(hum * 0.5 + whine * 0.45 + rattle * 0.9 + hiss[i] * 0.35)
    out = [math.tanh(1.3 * v) for v in normalize(out, 1.0)]
    return normalize(out, 0.8)


def bolt_in():
    rng = random.Random(9077)
    n = int(0.45 * RATE)
    air = high_pass(noise(n, rng), 3000.0)
    out = []
    phase = 0.0
    for i in range(n):
        t = i / RATE
        u = t / (n / RATE)
        env = math.sin(0.5 * math.pi * min(1.0, t / 0.12)) ** 2 * (0.5 + 0.5 * math.cos(math.pi * max(0.0, (t - 0.12) / (n / RATE - 0.12))))
        f = 900.0 * (1400.0 / 900.0) ** u # rising
        phase += 2.0 * math.pi * f / RATE
        tone = math.sin(phase) + 0.5 * math.sin(2.01 * phase + 0.7) + 0.25 * math.sin(3.03 * phase + 1.9)
        tremolo = 1.0 + 0.35 * math.sin(2.0 * math.pi * 28.0 * t)
        out.append(env * (tone * tremolo / 1.75 + air[i] * 0.3))
    return normalize(out, 0.5)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "sound", "vr")
    write_wav(os.path.join(out_dir, "widowmaker_shot.wav"), widowmaker_shot())
    write_wav(os.path.join(out_dir, "widowmaker_motor.wav"), widowmaker_motor(), loop=True)
    write_wav(os.path.join(out_dir, "xbow_bolt_in.wav"), bolt_in())


if __name__ == "__main__":
    main()
