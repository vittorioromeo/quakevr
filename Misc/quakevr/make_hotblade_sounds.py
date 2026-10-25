#!/usr/bin/env python3
# make_hotblade_sounds.py -- the incandescent blades' sounds (QC vr_hotblade.qc; Weapons > Incandescent Blades;
# docs/vr-port/ROUND22.md, "Incandescent blades"), synthesised (no recordings):
#
#   quakevr/sound/vr/blade_sizzle.wav  iron heating in a flame: a soft frying crackle (sparse sharp pops over a thin
#                                      high hiss), 0.5 s, played twice a second while the blade is in the flame
#   quakevr/sound/vr/blade_hot.wav     the blade turning red-hot: a short whoosh of the flame taking it and a bright
#                                      metallic ring (inharmonic partials, a slow beat) dying away, 1.1 s
#   quakevr/sound/vr/blade_quench.wav  a red-hot blade dipped in water: a violent bubbling burst, then a long steam hiss
#                                      falling in pitch and loudness, 1.4 s
#
# 22050 Hz, mono, 16 bit (as the other synthesised sounds), each peak normalised then soft clipped.
# Deterministic (a fixed seed). Pure Python (no numpy).
#
# Usage: python Misc/quakevr/make_hotblade_sounds.py [output folder, default quakevr/sound/vr]

import math
import os
import random
import struct
import sys
import wave

RATE = 22050
SEED = 4242


def biquad(x, kind, centre, q):
    """RBJ band-pass (0 dB peak) or low-pass of the list x."""
    w = 2.0 * math.pi * centre / RATE
    alpha = math.sin(w) / (2.0 * q)
    c = math.cos(w)
    if kind == "band":
        b0, b1, b2 = alpha, 0.0, -alpha
    else:
        b0, b1, b2 = (1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0
    a0, a1, a2 = 1.0 + alpha, -2.0 * c, 1.0 - alpha
    b0, b1, b2, a1, a2 = b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0
    y = [0.0] * len(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        o = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1 = x1, v
        y2, y1 = y1, o
        y[i] = o
    return y


def highpass(x, cutoff):
    rc = 1.0 / (2.0 * math.pi * cutoff)
    a = rc / (rc + 1.0 / RATE)
    y = [0.0] * len(x)
    px = py = 0.0
    for i, v in enumerate(x):
        py = a * (py + v - px)
        px = v
        y[i] = py
    return y


def noise(rng, n):
    return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def envelope(n, attack, release_tau, hold=0.0):
    out = []
    for i in range(n):
        t = i / RATE
        a = min(1.0, t / attack) if attack > 0 else 1.0
        r = 1.0 if t < hold else math.exp(-(t - hold) / release_tau)
        out.append(a * r)
    return out


def fade_ends(x, fade_in=0.004, fade_out=0.03):
    n = len(x)
    fi = max(1, int(fade_in * RATE))
    fo = max(1, int(fade_out * RATE))
    for i in range(n):
        k = min(1.0, (i + 1) / fi, (n - i) / fo)
        x[i] *= k
    return x


def finish(x, peak):
    m = max(1e-9, max(abs(v) for v in x))
    out = [math.tanh(1.4 * v / m) / math.tanh(1.4) * peak for v in x]
    return fade_ends(out)


def pops(rng, n, rate, length=0.004, centre=3800.0):
    """Sparse sharp crackles: `rate` a second, each a tiny decaying band-limited click."""
    out = [0.0] * n
    t = 0.0
    while True:
        t += rng.expovariate(rate)
        i0 = int(t * RATE)
        if i0 >= n:
            break
        m = int(length * RATE)
        amp = rng.uniform(0.3, 1.0)
        click = [rng.uniform(-1.0, 1.0) * math.exp(-k / (0.0012 * RATE)) for k in range(m)]
        click = biquad(click, "band", centre * rng.uniform(0.7, 1.4), 1.2)
        for k, v in enumerate(click):
            if i0 + k < n:
                out[i0 + k] += amp * v * 4.0
    return out


def make_sizzle(rng):
    n = int(0.5 * RATE)
    hiss = highpass(biquad(noise(rng, n), "band", 6500.0, 0.7), 2500.0)
    crackle = pops(rng, n, 70.0)
    env = [0.75 + 0.25 * math.sin(2.0 * math.pi * 9.0 * i / RATE) for i in range(n)]
    x = [(0.35 * h + c) * e for h, c, e in zip(hiss, crackle, env)]
    return finish(x, 0.6)


def make_hot(rng):
    n = int(1.1 * RATE)
    # The flame taking it: a breathy low whoosh, swelling then gone.
    whoosh = biquad(noise(rng, n), "low", 900.0, 0.7)
    wenv = [min(1.0, (i / RATE) / 0.06) * math.exp(-max(0.0, i / RATE - 0.06) / 0.12) for i in range(n)]
    # The ring: an iron bar's inharmonic partials, beating slowly, the high ones dying first.
    partials = [(1180.0, 1.0, 0.55), (1180.0 * 2.76, 0.55, 0.32), (1180.0 * 5.40, 0.3, 0.18), (1180.0 * 8.93, 0.15, 0.1)]
    ring = [0.0] * n
    for f, a, tau in partials:
        ph = rng.uniform(0.0, 2.0 * math.pi)
        for i in range(n):
            t = i / RATE
            beat = 1.0 + 0.25 * math.sin(2.0 * math.pi * 3.1 * t)
            ring[i] += a * beat * math.sin(2.0 * math.pi * f * t + ph) * math.exp(-t / tau) * min(1.0, t / 0.003)
    crackle = pops(rng, n, 25.0)
    cenv = envelope(n, 0.01, 0.35)
    x = [0.8 * w * we + 0.55 * r + 0.25 * c * ce for w, we, r, c, ce in zip(whoosh, wenv, ring, crackle, cenv)]
    return finish(x, 0.7)


def make_quench(rng):
    n = int(1.4 * RATE)
    # The burst: violent bubbling (dense random pops, lower) in the first 0.3 s.
    bubbles = pops(rng, n, 260.0, length=0.006, centre=1500.0)
    benv = envelope(n, 0.005, 0.12, hold=0.08)
    # The steam: a broad hiss, its band falling from about 7 to 3 kHz as it dies.
    raw = noise(rng, n)
    seg = int(0.05 * RATE)
    steam = []
    for s0 in range(0, n, seg):
        t = s0 / RATE
        centre = 3000.0 + 4000.0 * math.exp(-t / 0.5)
        part = biquad(raw[max(0, s0 - 64):s0 + seg], "band", centre, 0.6)
        steam.extend(part[min(64, s0):])
    steam = highpass(steam[:n], 1200.0)
    senv = envelope(n, 0.02, 0.45, hold=0.15)
    x = [0.8 * b * be + 1.0 * s * se for b, be, s, se in zip(bubbles, benv, steam, senv)]
    return finish(x, 0.75)


def write_wav(path, samples):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", max(-32767, min(32767, int(round(v * 32767))))) for v in samples))


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join("quakevr", "sound", "vr")
    os.makedirs(out_dir, exist_ok=True)
    rng = random.Random(SEED)
    for name, make in (("blade_sizzle", make_sizzle), ("blade_hot", make_hot), ("blade_quench", make_quench)):
        samples = make(rng)
        path = os.path.join(out_dir, name + ".wav")
        write_wav(path, samples)
        print("%s: %.2f s" % (path, len(samples) / RATE))


if __name__ == "__main__":
    main()
