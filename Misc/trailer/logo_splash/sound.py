# sound.py -- the logo splash's sound track: Quake's own sounds (read in place from the player's id1 paks, never
# copied) layered with synthesis, placed on the timeline's frames (common.py), panned where things happen on screen,
# through a short room reverb, then levelled to about -14 LUFS integrated with a -1 dBTP true-peak limit.
#
#   python sound.py [--variant nogrunt|full] [--work <dir>] [--out track.wav] [--id1 <id1 folder>]
#
# 48 kHz, 24-bit stereo WAV, exactly the video's length (800 samples a frame at 60 fps). SOUNDS.md lists the sources.

import argparse
import io
import json
import math
import os
import struct
import sys
import wave

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "..", "quakevr"))
import common as C  # noqa: E402
import quakepak  # noqa: E402

SR = 48000
SPF = SR // C.FPS            # samples a frame


# ---------------------------------------------------------------------------------------------------------------
# Sources

class Quake:
    """The id1 paks' sounds, decoded to float at 48 kHz on demand."""

    def __init__(self, id1):
        self.files = {}
        for p in quakepak.game_paks(id1):
            self.files.update({k: v for k, v in quakepak.read_pak(p).items() if k.endswith(".wav")})
        self.cache = {}

    def get(self, name, pitch=1.0):
        key = (name, round(pitch, 4))
        if key not in self.cache:
            with wave.open(io.BytesIO(self.files["sound/" + name])) as w:
                n, ch, sw, rate = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
                raw = w.readframes(n)
            if sw == 1:
                x = (np.frombuffer(raw, np.uint8).astype(np.float32) - 128) / 128
            else:
                x = np.frombuffer(raw, "<i2").astype(np.float32) / 32768
            x = x.reshape(-1, ch).mean(1)
            x -= x.mean()
            self.cache[key] = resample(x, SR / rate / pitch)
        return self.cache[key]


def resample(x, ratio):
    """Band-limited resampling by the FFT (ratio = output length / input length)."""
    n = len(x)
    m = max(1, int(round(n * ratio)))
    pad = 512
    xp = np.concatenate([np.zeros(pad, np.float32), x, np.zeros(pad, np.float32)])
    X = np.fft.rfft(xp)
    mp = int(round(len(xp) * ratio))
    Y = np.zeros(mp // 2 + 1, np.complex128)
    k = min(len(X), len(Y))
    Y[:k] = X[:k]
    y = np.fft.irfft(Y, mp) * (mp / len(xp))
    o = int(round(pad * ratio))
    return y[o:o + m].astype(np.float32)


def filt(x, fn):
    """x shaped in the frequency domain by fn(freqs in Hz) -> gain."""
    n = len(x)
    N = 1 << int(math.ceil(math.log2(n + 1)))
    X = np.fft.rfft(x, N)
    f = np.fft.rfftfreq(N, 1 / SR)
    return np.fft.irfft(X * fn(f), N)[:n].astype(np.float32)


def lp(fc, order=2):
    return lambda f: 1 / np.sqrt(1 + (f / fc) ** (2 * order))


def hp(fc, order=2):
    return lambda f: 1 / np.sqrt(1 + (fc / np.maximum(f, 1e-3)) ** (2 * order))


def bp(lo, hi, order=2):
    return lambda f: lp(hi, order)(f) * hp(lo, order)(f)


def env_exp(n, attack, decay):
    t = np.arange(n) / SR
    return (1 - np.exp(-t / max(attack, 1e-4))) * np.exp(-t / decay)


RNG = np.random.default_rng(2026)


def noise(n):
    return RNG.standard_normal(n).astype(np.float32)


def sine_sweep(dur, f0, f1, decay):
    n = int(dur * SR)
    t = np.arange(n) / SR
    f = f1 + (f0 - f1) * np.exp(-t / (dur * 0.25))
    ph = 2 * np.pi * np.cumsum(f) / SR
    return (np.sin(ph) * env_exp(n, 0.002, decay)).astype(np.float32)


def metal_ring(dur, base, rng, partials=9, decay=0.7):
    """An inharmonic struck-metal ring (a heavy steel letter)."""
    n = int(dur * SR)
    t = np.arange(n) / SR
    out = np.zeros(n, np.float32)
    for k in range(partials):
        fr = base * (1 + k * rng.uniform(0.9, 1.7)) * rng.uniform(0.97, 1.03) ** k
        a = rng.uniform(0.3, 1.0) / (1 + 0.5 * k)
        d = decay * rng.uniform(0.4, 1.0) / (1 + 0.15 * k)
        out += (a * np.sin(2 * np.pi * fr * t + rng.uniform(0, 6.28)) * np.exp(-t / d)).astype(np.float32)
    return out * (1 - np.exp(-t / 0.001))


def squelch(dur, rng, bright=1.0):
    """A wet splat: a quick burst of band-passed noise with a few resonant wobbles."""
    n = int(dur * SR)
    x = noise(n) * env_exp(n, 0.002, dur * 0.25)
    x = filt(x, bp(250 * bright, 3200 * bright, 2))
    t = np.arange(n) / SR
    wob = 1 + 0.6 * np.sin(2 * np.pi * rng.uniform(18, 40) * t)
    return x * wob


# ---------------------------------------------------------------------------------------------------------------
# The mix

class Mix:
    def __init__(self, n):
        self.dry = np.zeros((2, n), np.float32)
        self.send = np.zeros((2, n), np.float32)

    def add(self, x, at, gain=1.0, pan=0.0, rev=0.25):
        """x (mono) at sample `at`, gain, pan -1..1 (equal power), part of it sent to the reverb."""
        at = int(at)
        if at >= self.dry.shape[1] or len(x) == 0:
            return
        if at < 0:
            x = x[-at:]
            at = 0
        x = x[:self.dry.shape[1] - at] * gain
        a = (pan + 1) * math.pi / 4
        for c, g in enumerate((math.cos(a), math.sin(a))):
            self.dry[c, at:at + len(x)] += x * g
            self.send[c, at:at + len(x)] += x * g * rev


def reverb_ir(dur=1.6, seed=5):
    """A dark room: decorrelated decaying noise per channel, early reflections, highs dying first."""
    r = np.random.default_rng(seed)
    n = int(dur * SR)
    t = np.arange(n) / SR
    out = []
    for c in range(2):
        x = r.standard_normal(n).astype(np.float32) * np.exp(-t / (dur * 0.22))
        lo = filt(x, lp(2500))
        x = lo * (1 - np.exp(-t / 0.3)) + x * np.exp(-t / 0.3) * 0.5
        for k in range(6):
            d = int(r.uniform(0.008, 0.06) * SR)
            x[d] += r.uniform(0.3, 0.7) * (-1) ** k
        out.append(x / np.sqrt(np.sum(x * x)))
    return out


def convolve(x, h):
    n = len(x) + len(h) - 1
    N = 1 << int(math.ceil(math.log2(n)))
    return np.fft.irfft(np.fft.rfft(x, N) * np.fft.rfft(h, N), N)[:len(x)].astype(np.float32)


# Loudness (ITU-R BS.1770-4): K-weighting, 400 ms blocks, absolute and relative gates.
K1 = ([1.53512485958697, -2.69169618940638, 1.19839281085285], [1.0, -1.69065929318241, 0.73248077421585])
K2 = ([1.0, -2.0, 1.0], [1.0, -1.99004745483398, 0.99007225036621])


def biquad_response(b, a, N):
    w = np.exp(-1j * np.linspace(0, np.pi, N // 2 + 1))
    return (b[0] + b[1] * w + b[2] * w * w) / (a[0] + a[1] * w + a[2] * w * w)


def lufs(st):
    n = st.shape[1]
    N = 1 << int(math.ceil(math.log2(n + SR)))
    H = biquad_response(*K1, N) * biquad_response(*K2, N)
    y = np.stack([np.fft.irfft(np.fft.rfft(ch, N) * H, N)[:n] for ch in st])
    blk, hop = int(0.4 * SR), int(0.1 * SR)
    ms = np.array([np.mean(y[:, i:i + blk] ** 2, axis=1).sum() for i in range(0, n - blk, hop)])
    L = -0.691 + 10 * np.log10(np.maximum(ms, 1e-12))
    g = ms[L > -70]
    rel = -0.691 + 10 * np.log10(np.mean(g)) - 10
    g = ms[(L > -70) & (L > rel)]
    return -0.691 + 10 * np.log10(np.mean(g))


def true_peak(st):
    return max(float(np.max(np.abs(resample(ch.astype(np.float32), 4.0)))) for ch in st)


def limit(st, ceiling, look_ms=3.0, release_ms=90.0):
    """A look-ahead peak limiter (both channels linked)."""
    L = max(2, int(look_ms * SR / 1000))
    peak = np.max(np.abs(st), axis=0)
    graw = np.minimum(1.0, ceiling / np.maximum(peak, 1e-9))
    # The minimum over the next L samples (the gain is down before the peak arrives), then smoothed over L.
    pad = np.concatenate([graw, np.ones(L)])
    gmin = np.lib.stride_tricks.sliding_window_view(pad, L).min(axis=1)[:len(graw)]
    c = np.cumsum(np.concatenate([np.ones(L), gmin]))
    gs = (c[L:] - c[:-L]) / L
    r = 1 - math.exp(-1 / (release_ms * SR / 1000))
    out = np.empty_like(gs)
    g = 1.0
    for i, v in enumerate(gs):
        g = v if v < g else g + (v - g) * r
        out[i] = g
    return st * out


# ---------------------------------------------------------------------------------------------------------------
# The events

def pan_of(x, W=1920):
    return float(np.clip((x / W - 0.5) * 1.5, -0.9, 0.9))


def letter_centres(work):
    out = []
    for i in range(7):
        a = np.asarray(Image.open(os.path.join(work, "stills", "rest_%d.png" % i)).convert("RGBA"))[..., 3]
        xs = np.nonzero(a.max(0) > 20)[0]
        out.append((xs.min() + xs.max()) / 2 * 1920 / a.shape[1])
    return out


def build(variant, work, id1):
    q = Quake(id1)
    off = C.NOGRUNT_OFFSET if variant == "nogrunt" else 0
    frames = C.NOGRUNT_FRAMES if variant == "nogrunt" else C.FRAMES
    n = frames * SPF
    mix = Mix(n)
    rng = np.random.default_rng(7)

    def at(f):                      # the sample of the full version's frame f in this version
        return (f - off) * SPF

    cx = C.project(C.CHEST, 1920)[0]

    # The axe (the full version only): its whoosh as it spins in, its hit.
    if variant == "full":
        for k in range(4):
            w = noise(int(0.12 * SR))
            w = filt(w, bp(400 + 300 * k, 2500 + 500 * k)) * np.hanning(len(w)).astype(np.float32)
            mix.add(w, at(C.AXE_START + k * 4.5), 0.35, pan=-0.8 + 0.25 * k)
        mix.add(q.get("weapons/ax1.wav", 0.9), at(C.AXE_START + 2), 0.6, pan=-0.6)
        mix.add(q.get("player/axhit2.wav"), at(C.HIT), 0.9, pan=0.0)

    # The burst: a deep boom, the gib sound, a wet splash, and the blood landing all over the wall.
    h = at(C.HIT)
    mix.add(sine_sweep(0.9, 120, 38, 0.35), h, 1.1, rev=0.15)
    mix.add(filt(noise(int(0.5 * SR)) * env_exp(int(0.5 * SR), 0.001, 0.07), lp(1800)), h, 0.9)
    mix.add(q.get("player/udeath.wav", 0.92), h, 1.0, pan=0.0, rev=0.35)
    mix.add(q.get("zombie/z_gib.wav", 0.85), h + int(0.02 * SR), 0.8, pan=-0.15)
    mix.add(q.get("player/gib.wav", 1.0), h + int(0.01 * SR), 0.7, pan=0.15)
    mix.add(squelch(0.6, rng, 0.9), h, 0.7, rev=0.3)
    for k in range(26):             # the spray hitting the wall, out from the middle
        d = rng.uniform(0.02, 0.32) ** 1.3
        s = squelch(rng.uniform(0.05, 0.16), rng, rng.uniform(0.7, 1.6))
        mix.add(s, h + int(d * SR), rng.uniform(0.12, 0.35), pan=rng.uniform(-0.9, 0.9) * min(1, d * 4))
    for k, name in enumerate(("blob/land1.wav", "blob/hit1.wav", "zombie/z_hit.wav")):
        mix.add(q.get(name, rng.uniform(0.8, 1.1)), h + int((0.04 + 0.05 * k) * SR), 0.45, pan=(-0.5, 0.5, 0.1)[k])
    # Drips, now and then.
    for k in range(5):
        f = C.HIT + 40 + k * 37 + rng.uniform(0, 15)
        mix.add(q.get("ambience/drip1.wav", rng.uniform(0.9, 1.3))[:int(0.4 * SR)], at(f), 0.12,
                pan=rng.uniform(-0.6, 0.6))

    # The letters: a whoosh as each falls at the camera, then a heavy steel slam (the camera's kick), its ring, the
    # blood squirted out and the dust.
    xs = letter_centres(work)
    for i, land in enumerate(C.LAND):
        heavy = 1.3 if i >= 5 else 1.0
        p = pan_of(xs[i])
        r = np.random.default_rng(100 + i)
        fall = int(C.FALL / C.FPS * SR)
        w = noise(fall)
        t = np.arange(fall) / fall
        w = filt(w * (t ** 2.2).astype(np.float32), bp(200, 1800 + 3000 * r.random()))
        mix.add(w, at(land) - fall, 0.35 * heavy, pan=p * 0.6, rev=0.1)
        s = at(land)
        mix.add(sine_sweep(0.6, 95 * r.uniform(0.9, 1.1) / heavy, 40, 0.22 * heavy), s, 1.0 * heavy, pan=p * 0.4,
                rev=0.1)
        mix.add(filt(noise(int(0.25 * SR)) * env_exp(int(0.25 * SR), 0.0005, 0.03), lp(5000)), s, 0.7 * heavy, pan=p)
        mix.add(q.get("misc/deepthud.wav", r.uniform(0.85, 1.0) / heavy), s, 0.75 * heavy, pan=p, rev=0.3)
        mix.add(q.get("hipweap/mjolhit.wav" if i % 2 == 0 else "misc/clang.wav", r.uniform(0.75, 0.95) / heavy),
                s, 0.5, pan=p, rev=0.35)
        mix.add(metal_ring(1.6, r.uniform(150, 260) / heavy, r), s, 0.22 * heavy, pan=p, rev=0.45)
        mix.add(squelch(0.35, r, 0.8), s + int(0.008 * SR), 0.45 * heavy, pan=p)
        for k in range(6):
            mix.add(squelch(r.uniform(0.04, 0.1), r, r.uniform(0.8, 1.5)), s + int(r.uniform(0.03, 0.25) * SR),
                    r.uniform(0.08, 0.2), pan=float(np.clip(p + r.uniform(-0.5, 0.5), -1, 1)))
        dust = filt(noise(int(1.2 * SR)) * env_exp(int(1.2 * SR), 0.05, 0.35), bp(150, 900))
        mix.add(dust, s + int(0.02 * SR), 0.22 * heavy, pan=p, rev=0.4)

    # "Unleashed" written with a finger in the blood: a wet smear following the finger's speed, wet clicks.
    with open(os.path.join(work, "pre", "meta.json")) as f:
        meta = json.load(f)
    tip = meta["tip"]
    W = meta["width"]
    s0, s1 = at(C.FINGER_START), at(C.FINGER_END + 4)
    seg = np.zeros(s1 - s0, np.float32)
    pans = np.zeros(s1 - s0, np.float32)
    tt = np.array([p[2] for p in tip])
    px = np.array([p[0] for p in tip]) * 1920 / W
    py = np.array([p[1] for p in tip]) * 1920 / W
    fr = np.arange(C.FINGER_START, C.FINGER_END + 4)
    speed = np.zeros(len(fr))
    xpos = np.zeros(len(fr))
    for k, f in enumerate(fr):
        sel = (tt >= f) & (tt < f + 1)
        if sel.sum() > 1:
            speed[k] = np.hypot(np.diff(px[sel]), np.diff(py[sel])).sum()
        xpos[k] = px[min(np.searchsorted(tt, f), len(px) - 1)]
    speed = np.convolve(speed, np.ones(3) / 3, "same")
    spd = np.interp(np.arange(s1 - s0) / SPF, np.arange(len(fr)), speed / max(speed.max(), 1e-6))
    smear = filt(noise(s1 - s0), bp(120, 1400)) * spd.astype(np.float32)
    smear += filt(noise(s1 - s0), bp(1500, 4500)) * (spd ** 2).astype(np.float32) * 0.25
    pans = np.interp(np.arange(s1 - s0) / SPF, np.arange(len(fr)), (xpos / 1920 - 0.5) * 1.4)
    for c, g in enumerate((np.cos((pans + 1) * np.pi / 4), np.sin((pans + 1) * np.pi / 4))):
        mix.dry[c, s0:s1] += (smear * g * 0.25).astype(np.float32)
        mix.send[c, s0:s1] += (smear * g * 0.08).astype(np.float32)
    for k in range(40):             # wet clicks and tacky pulls as it goes
        f = r_f = C.FINGER_START + rng.uniform(0, C.FINGER_END - C.FINGER_START)
        kx = xpos[min(int(r_f - C.FINGER_START), len(xpos) - 1)]
        mix.add(squelch(rng.uniform(0.03, 0.08), rng, rng.uniform(1.0, 2.0)), at(f), rng.uniform(0.05, 0.14),
                pan=pan_of(kx))

    # Materialising: a rising hot shimmer through the letters, then the glint's ring.
    m0, m1 = at(C.MAT_START), at(C.MAT_END)
    nm = m1 - m0 + int(0.3 * SR)
    t = np.arange(nm) / SR
    rise = np.clip(t / ((m1 - m0) / SR), 0, 1)
    sh = filt(noise(nm), bp(800, 7000)) * (rise ** 2 * np.exp(-np.maximum(t - (m1 - m0) / SR, 0) / 0.08))
    glis = sum(np.sin(2 * np.pi * np.cumsum(fq * (1 + 1.5 * rise)) / SR) for fq in (220, 331, 497, 745)) * 0.08
    mix.add((sh * 0.5 + glis * rise).astype(np.float32), m0, 1.1, rev=0.4)
    mix.add(q.get("items/r_item2.wav", 0.8), m1 - int(0.05 * SR), 0.6, rev=0.5)
    mix.add(metal_ring(2.0, 880, np.random.default_rng(3), partials=7, decay=0.9), at(C.MAT_END + 4), 0.35,
            pan=0.2, rev=0.6)

    # The fire: a whoomp as it catches, then a burning bed to the end (Quake's fire loop, two decorrelated copies,
    # crackles, a low roar) following the flames' strength.
    fs = at(C.FIRE_START)
    nw = int(1.2 * SR)
    t = np.arange(nw) / SR
    wh = noise(nw) * env_exp(nw, 0.06, 0.35)
    wh = filt(wh, lp(3500)) * 1.0
    mix.add(wh.astype(np.float32), fs - int(0.05 * SR), 0.9, rev=0.3)
    mix.add(sine_sweep(1.0, 70, 32, 0.4), fs, 0.9, rev=0.1)
    mix.add(q.get("weapons/r_exp3.wav", 0.7), fs, 0.7, rev=0.4)
    mix.add(q.get("player/lburn1.wav", 0.9), fs + int(0.15 * SR), 0.5, pan=-0.3)
    mix.add(q.get("player/lburn2.wav", 0.85), fs + int(0.35 * SR), 0.4, pan=0.3)
    loop = q.get("ambience/fire1.wav")
    nb = n - fs
    if nb > 0:
        from fx import Fire
        inten = np.array([Fire.intensity(f + off) for f in range(C.FIRE_START - off, frames)], np.float32)
        envf = np.interp(np.arange(nb) / SPF, np.arange(len(inten)), inten) * np.clip(np.arange(nb) / (0.25 * SR), 0, 1)
        for c, shift in enumerate((0, len(loop) // 3)):
            reps = int(math.ceil((nb + shift) / len(loop))) + 1
            xf = int(0.05 * SR)
            body = np.tile(loop, reps)
            # Crossfade the seams of the tiled loop.
            for k in range(1, reps):
                j = k * len(loop)
                body[j - xf:j] = body[j - xf:j] * np.linspace(1, 0, xf) + loop[:xf] * np.linspace(0, 1, xf)
            bed = body[shift:shift + nb] * envf
            mix.dry[c, fs:] += bed * 1.4
            mix.send[c, fs:] += bed * 0.1
        roar = filt(noise(nb), bp(40, 300)) * envf
        crack = np.zeros(nb, np.float32)
        for k in range(int(nb / SR * 30)):
            p = int(RNG.uniform(0, nb - 2000))
            crack[p:p + 400] += noise(400) * np.exp(-np.arange(400) / RNG.uniform(20, 90)) * RNG.uniform(0.1, 1.0)
        crack = filt(crack, hp(1200)) * envf
        for c in range(2):
            mix.dry[c, fs:] += roar * 0.8 + crack * (0.5 if c == 0 else 0.42)

    # Room, master: level to about -14 LUFS, true peak under -1 dBTP.
    ir = reverb_ir()
    wet = np.stack([convolve(mix.send[c], ir[c]) for c in range(2)])
    st = mix.dry + wet * 0.6
    st = st / max(1e-9, np.max(np.abs(st)))
    target = -14.0
    ceiling = 10 ** (-1.5 / 20)
    src = st
    for _ in range(12):
        out = limit(st, ceiling)
        L = lufs(out)
        tp = true_peak(out)
        if tp > 10 ** (-1.0 / 20):                 # inter-sample peaks: a lower ceiling
            ceiling *= 10 ** (-1.0 / 20) / tp * 0.995
            continue
        if abs(L - target) < 0.1:
            break
        st = st * 10 ** ((target - L) / 20)
    del src
    # A few milliseconds of fade at the ends (no clicks).
    k = int(0.004 * SR)
    out[:, :k] *= np.linspace(0, 1, k)
    out[:, -k:] *= np.linspace(1, 0, k)
    return out


def build_head(id1):
    """The decapitated head's stem: the rip and pop of the cut, the neck's spurts (one per heartbeat, as the blood in
    head_clip.py), the drops pattering, panned with the head. Frame NOGRUNT_LEAD is the cut."""
    q = Quake(id1)
    n = (C.HEAD_FRAMES + 1) * SPF
    mix = Mix(n)
    rng = np.random.default_rng(11)
    cut = C.NOGRUNT_LEAD * SPF

    def pan_at(t):
        p = C.project(C.head_state(C.NOGRUNT_LEAD + t * C.FPS)[0], 1920)
        return pan_of(p[0]) if p else 0.0

    mix.add(q.get("player/tornoff2.wav", 1.0), cut - int(0.01 * SR), 1.0, pan=0.0, rev=0.2)
    mix.add(q.get("zombie/z_gib.wav", 1.15), cut, 0.6, pan=0.05, rev=0.2)
    mix.add(q.get("player/udeath.wav", 1.2)[:int(0.35 * SR)], cut, 0.45, rev=0.2)
    # The pop: a low thump and a wet crack.
    mix.add(sine_sweep(0.25, 160, 55, 0.07), cut, 0.9, rev=0.1)
    crk = filt(noise(int(0.06 * SR)) * env_exp(int(0.06 * SR), 0.0003, 0.008), bp(900, 6000))
    mix.add(crk, cut, 0.7)
    mix.add(squelch(0.4, rng, 1.1), cut + int(0.01 * SR), 0.6)
    # The spurts: squirts on the beats (4.2 a second), dying away.
    t = 0.0
    while t < 1.3:
        ph = (t * 4.2 + 0.3 / (2 * math.pi)) % 1.0
        peak_t = t + ((0.25 - ph) % 1.0) / 4.2          # where sin(2 pi 4.2 t + 0.3) peaks
        if peak_t >= 1.3:
            break
        amp = math.exp(-peak_t / 0.6)
        m = int(rng.uniform(0.07, 0.11) * SR)
        sq = filt(noise(m), bp(400, 3500)) * np.hanning(m).astype(np.float32) ** 0.6
        tt = np.arange(m) / SR
        sq *= (1 + 0.5 * np.sin(2 * np.pi * rng.uniform(25, 45) * tt)).astype(np.float32)
        mix.add(sq, cut + int((peak_t - 0.04) * SR), 0.55 * amp + 0.1, pan=pan_at(peak_t), rev=0.15)
        mix.add(squelch(0.08, rng, 1.4), cut + int(peak_t * SR), 0.25 * amp, pan=pan_at(peak_t))
        t = peak_t + 0.05
    # Drops pattering (off screen below).
    for k in range(30):
        tt = rng.uniform(0.3, 1.9)
        mix.add(squelch(rng.uniform(0.02, 0.05), rng, rng.uniform(1.2, 2.2)), cut + int(tt * SR),
                rng.uniform(0.04, 0.1), pan=rng.uniform(-0.3, 0.9))
    ir = reverb_ir(1.2, 8)
    wet = np.stack([convolve(mix.send[c], ir[c]) for c in range(2)])
    st = mix.dry + wet * 0.6
    st = st / max(1e-9, np.max(np.abs(st)))
    ceiling = 10 ** (-1.5 / 20)
    out = limit(st, ceiling)
    tp = true_peak(out)
    out = out * min(1.0, 10 ** (-1.0 / 20) / tp)
    k = int(0.004 * SR)
    out[:, -k:] *= np.linspace(1, 0, k)
    return out


def write_wav24(path, st):
    x = np.clip(st.T, -1, 1)
    i = np.ascontiguousarray(np.round(x * 8388607).astype("<i4"))
    b = i.view(np.uint8).reshape(-1, 4)[:, :3].tobytes()
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(3)
        w.setframerate(SR)
        w.writeframes(b)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--variant", default="nogrunt", choices=("full", "nogrunt", "head"))
    ap.add_argument("--work", default=C.WORK)
    ap.add_argument("--out", required=True)
    ap.add_argument("--id1", default=r"C:\OHWorkspace\qvr-kit\qbase\id1")
    a = ap.parse_args()
    st = build_head(a.id1) if a.variant == "head" else build(a.variant, a.work, a.id1)
    write_wav24(a.out, st)
    n = st.shape[1]
    print("%s: %d samples (%.3f s, %d frames), %.2f LUFS, true peak %.2f dBTP" % (
        a.out, n, n / SR, n // SPF, lufs(st), 20 * math.log10(true_peak(st))))


if __name__ == "__main__":
    main()
