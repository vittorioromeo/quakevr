#!/usr/bin/env python3
# make_nailpistol_sounds.py -- the nail pistol's shot (QC vr_nailpistol.qc W_FireNailPistol), synthesised: quakevr/sound/
# vr/nailpistol_shot.wav, 22050 Hz mono 16-bit, 0.32 s. Its parts: a sharp pneumatic crack (a burst of noise, bright,
# dying in 25 ms), a low thump under it (a falling sine, 110 to 60 Hz), and the copper slide's two clacks (back at 18 ms,
# home at 70 ms: short rings of a few inharmonic partials over a click), a little tail of air. The game plays the super
# nailgun's spike2.wav pitched up under it (sound_pitch, 135%), so this file carries the gun's own character, not its punch
# alone. Deterministic (a fixed seed): rerun it to get the same file.
# Also its re-rack (vr_nailpistol.qc VR_NailPistol_HandFrame: a magazine with rounds put in an empty gun, the slide
# locked back runs home): quakevr/sound/vr/nailpistol_rack.wav, 0.26 s: the slide stop's tick, the slide's short scrape
# forward, its slam home (a louder, lower clack over a little body thunk) and the ring after.
#
# Usage: python Misc/quakevr/make_nailpistol_sounds.py [output folder]

import math
import os
import random
import struct
import sys
import wave

RATE = 22050
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "quakevr", "sound", "vr")


def env(t, attack, decay):
    if t < 0:
        return 0.0
    if t < attack:
        return t / attack
    return math.exp(-(t - attack) / decay)


def shot():
    rnd = random.Random(1729)
    n = int(RATE * 0.32)
    out = [0.0] * n
    lp = 0.0
    hp_prev = 0.0
    hp = 0.0
    phase = 0.0
    for i in range(n):
        t = i / RATE
        # The crack: white noise, high-passed (bright), a 0.4 ms attack, 25 ms decay; a longer, softer hiss of air.
        w = rnd.uniform(-1.0, 1.0)
        hp = 0.82 * (hp + w - hp_prev)
        hp_prev = w
        lp += 0.35 * (w - lp)
        crack = hp * env(t, 0.0004, 0.025) * 0.9 + lp * env(t, 0.002, 0.09) * 0.25
        # The thump: a falling sine.
        f = 60.0 + 50.0 * math.exp(-t / 0.03)
        phase += 2.0 * math.pi * f / RATE
        thump = math.sin(phase) * env(t, 0.001, 0.045) * 0.8
        # The slide's clacks: back, then home (a little louder, lower).
        clack = 0.0
        for at, gain, base in ((0.018, 0.45, 2300.0), (0.070, 0.55, 1900.0)):
            u = t - at
            if u >= 0:
                ring = sum(math.sin(2.0 * math.pi * base * k * u + k) / (1 + j)
                           for j, k in enumerate((1.0, 1.47, 2.31, 3.12)))
                clack += gain * (ring * 0.35 * env(u, 0.0002, 0.012) + rnd.uniform(-1, 1) * env(u, 0.0001, 0.002))
        out[i] = crack + thump + clack
    peak = max(abs(v) for v in out) or 1.0
    k = 0.93 / peak
    # A soft knee on the sum, then the 16-bit file.
    return [math.tanh(v * k * 1.3) / math.tanh(1.3) for v in out]


def rack():
    rnd = random.Random(4104)
    n = int(RATE * 0.26)
    out = [0.0] * n
    bp_lo = 0.0
    bp_hi = 0.0
    phase = 0.0
    for i in range(n):
        t = i / RATE
        w = rnd.uniform(-1.0, 1.0)
        # The scrape: noise band-passed (two one-pole filters), swelling from 6 ms to the slam at 34 ms.
        bp_lo += 0.55 * (w - bp_lo)
        bp_hi += 0.08 * (bp_lo - bp_hi)
        scrape = (bp_lo - bp_hi) * (0.22 * (t - 0.006) / 0.028 if 0.006 <= t < 0.034 else 0.0)
        # The clacks: the slide stop's small tick, then the slam home (lower, louder), each a ring over a click.
        clack = 0.0
        for at, gain, base, decay in ((0.0, 0.3, 3100.0, 0.006), (0.034, 0.9, 1650.0, 0.022)):
            u = t - at
            if u >= 0:
                ring = sum(math.sin(2.0 * math.pi * base * k * u + k) / (1 + j)
                           for j, k in enumerate((1.0, 1.47, 2.31, 3.12, 4.05)))
                clack += gain * (ring * 0.4 * env(u, 0.0002, decay) + rnd.uniform(-1, 1) * env(u, 0.0001, 0.0025))
        # The gun's body under the slam: a short low thunk.
        u = t - 0.034
        thunk = 0.0
        if u >= 0:
            phase += 2.0 * math.pi * (140.0 + 80.0 * math.exp(-u / 0.01)) / RATE
            thunk = math.sin(phase) * env(u, 0.0005, 0.03) * 0.5
        out[i] = scrape + clack + thunk
    peak = max(abs(v) for v in out) or 1.0
    k = 0.93 / peak
    return [math.tanh(v * k * 1.3) / math.tanh(1.3) for v in out]


def write(path, samples):
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in samples))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else OUT
    path = os.path.join(out, "nailpistol_shot.wav")
    write(path, shot())
    print("wrote", path)
    path = os.path.join(out, "nailpistol_rack.wav")
    write(path, rack())
    print("wrote", path)


if __name__ == "__main__":
    main()
