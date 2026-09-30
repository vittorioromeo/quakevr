#!/usr/bin/env python3
# make_burst_sounds.py -- cuts the grunts' burst rifles' rounds (QC vr_enemyguns.qc VR_BurstSound, orig_mon_soldier.qc
# army_fire; docs/vr-port/ROUND21.md, "The grunts' burst rifles") out of a recording of an AK-47 from
# The Free Firearm Sound Library (Ben Jaszczak, Brian Nelson, Kevin Heras, Matthew Nanney), CC0 (public domain; see
# docs/vr-port/CREDITS.md), into quakevr/sound/vr/burst1..3.wav:
#
#   page:      https://opengameart.org/content/the-free-firearm-sound-library
#   download:  https://opengameart.org/sites/default/files/Prepared%20SFX%20Library.7z (194 MB; 7-Zip opens it)
#   used:      Prepared SFX Library/AK-47/C_31P.wav (96 kHz 24-bit stereo, 8.8 s): two single shots, outdoors, the
#              range's slap and roll behind them (the take's other files: closer and drier, or full-auto strings whose
#              rounds' tails are cut by the next)
#
# The recording is not in the repository (id-free, but large): extract the library and run
#   python Misc/quakevr/make_burst_sounds.py "<...>/Prepared SFX Library" [out folder, default quakevr/sound/vr]
#
# What is cut (seconds in C_31P.wav; the onset is the shot's first sample above a fifth of its peak):
#   burst1.wav  the first shot:   onset 0.3380, from 0.3350 to 0.8350
#   burst2.wav  the second shot:  onset 4.4014, from 4.3984 to 4.8984
#   burst3.wav  the second shot again, played 5% slower (a little deeper and longer), from the same cut
# Three, a little apart, picked at random (VR_BurstSound): the same round three times sounds mechanical.
#
# Each: the two channels mixed to mono, high-passed at 30 Hz (first order), resampled to 22050 Hz (FFT, with the
# recording around the cut, the top 10% below the new Nyquist tapered off), faded in 1 ms and out over the last
# 0.15 s (a cosine). The shot as heard downrange: the bullet's crack, then the muzzle's blast 17 ms after it, the
# range's slap from ~0.1 s. Levels: one gain for the three (+15.6 dB), a burst of them (0.1 s apart, as a grunt fires)
# as loud over its loudest 0.3 s as Quake's grunt's shotgun blast (soldier/sattck1.wav, the same sound as the player's
# weapons/guncock.wav): -8.0 dBFS RMS; its loudest 50 ms -5.7 (the shotgun's -6.0), a round's loudest 10 ms -3.9..-4.9
# (-3.7). The crack's and the blast's peaks (~2.5% of the samples) rounded off under -1 dBFS by the soft knee
# make_chainsaw_sounds.py uses.

import os
import struct
import sys
import wave

import numpy as np

SRC_RATE = 96000
RATE = 22050
PRE = 0.003  # seconds before the onset
LENGTH = 0.5  # seconds from the start of the cut
FADE_OUT = 0.15
BURST_GAP = 0.1
BURST_300MS_DB = -8.0  # Quake's soldier/sattck1.wav's (= weapons/guncock.wav's) loudest 0.3 s

# (name, onset in C_31P.wav, playback speed)
CUTS = (
    ("burst1.wav", 0.3380, 1.0),
    ("burst2.wav", 4.4014, 1.0),
    ("burst3.wav", 4.4014, 0.95),
)


def load(path):
    w = wave.open(path)
    assert w.getframerate() == SRC_RATE and w.getsampwidth() == 3, path
    b = np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8).reshape(-1, 3).astype(np.int32)
    a = ((b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)) << 8 >> 8).astype(np.float64) / 8388608.0
    return a.reshape(-1, w.getnchannels()).mean(axis=1)


def high_pass(x, hz=30.0):
    k = np.exp(-2 * np.pi * hz / SRC_RATE)
    out = np.empty_like(x)
    prev_in = prev_out = 0.0
    for i, v in enumerate(x):
        prev_out = k * (prev_out + v - prev_in)
        prev_in = v
        out[i] = prev_out
    return out


def resample(x, speed):
    """x (at SRC_RATE) at RATE, played `speed` times as fast; the band edge tapered (no brick wall to ring)."""
    n = len(x)
    m = int(round(n * RATE / (SRC_RATE * speed)))
    X = np.fft.rfft(x)
    Y = np.zeros(m // 2 + 1, dtype=complex)
    k = min(len(X), len(Y))
    Y[:k] = X[:k]
    f = np.arange(len(Y)) / (len(Y) - 1)  # 0..1 of the new Nyquist
    Y *= np.where(f < 0.9, 1.0, 0.5 + 0.5 * np.cos(np.pi * np.clip((f - 0.9) / 0.1, 0, 1)))
    return np.fft.irfft(Y, m) * (m / n)


def cut(a, onset, speed, pad=0.1):
    start = int(round((onset - PRE) * SRC_RATE))
    end = start + int(round(LENGTH * speed * SRC_RATE))
    p = int(pad * SRC_RATE)
    seg = high_pass(a[start - p:end + p])
    y = resample(seg, speed)
    q = int(round(p * RATE / (SRC_RATE * speed)))
    y = y[q:q + int(round(LENGTH * RATE))]
    i, o = int(0.001 * RATE), int(FADE_OUT * RATE)
    y[:i] *= np.linspace(0, 1, i, endpoint=False)
    y[len(y) - o:] *= 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, o))
    return y


def soft_limit(y, ceiling=0.89):
    """Below 0.7 x the ceiling untouched; above, a tanh knee up to the ceiling."""
    knee = 0.7 * ceiling
    s = np.sign(y)
    m = np.abs(y)
    over = m > knee
    m[over] = knee + (ceiling - knee) * np.tanh((m[over] - knee) / (ceiling - knee))
    return s * m


def loudest_db(y, seconds, rate=RATE):
    """The RMS of y's loudest `seconds`, in dBFS."""
    w = int(seconds * rate)
    return 20 * np.log10(max(np.sqrt(np.mean(y[i:i + w] ** 2)) for i in range(0, max(1, len(y) - w + 1), max(1, w // 8))) + 1e-12)


def burst(rounds):
    """The rounds as a grunt fires them: one every 0.1 s (army_atk5..7), each ringing on."""
    gap = int(BURST_GAP * RATE)
    out = np.zeros(gap * (len(rounds) - 1) + max(len(y) for y in rounds))
    for k, y in enumerate(rounds):
        out[k * gap:k * gap + len(y)] += y
    return out


def write_wav(path, y):
    pcm = np.clip(np.round(y * 32767), -32768, 32767).astype("<i2").tobytes()
    body = b"WAVE" + b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16) + b"data" + struct.pack("<I", len(pcm)) + pcm
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", len(body)) + body)


def main():
    src = sys.argv[1]
    out_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "..", "..", "quakevr", "sound", "vr")
    a = load(os.path.join(src, "AK-47", "C_31P.wav"))
    ys = [(name, cut(a, onset, speed)) for name, onset, speed in CUTS]
    # One gain for the three: a burst of them (burst1..3, 0.1 s apart) as loud over its loudest 0.3 s as Quake's shotgun
    # blast (the soft knee after it: iterated).
    g = 1.0
    for _ in range(30):
        g *= 10 ** ((BURST_300MS_DB - loudest_db(burst([soft_limit(y * g) for _, y in ys]), 0.3)) / 20)
    rounds = []
    for name, y in ys:
        over = np.mean(np.abs(y * g) > 0.7 * 0.89) * 100
        y = soft_limit(y * g)
        rounds.append(y)
        write_wav(os.path.join(out_dir, name), y)
        print(f"{name}: {len(y) / RATE:.2f} s, gain {20 * np.log10(g):+.1f} dB (knee {over:.1f}% of samples), loudest 10 ms "
              f"{loudest_db(y, 0.01):.1f} dBFS, 50 ms {loudest_db(y, 0.05):.1f}, rms {20 * np.log10(np.sqrt(np.mean(y ** 2))):.1f}, "
              f"peak {20 * np.log10(np.max(np.abs(y))):.1f}")
    b = burst(rounds)
    print(f"a burst: loudest 50 ms {loudest_db(b, 0.05):.1f} dBFS, 300 ms {loudest_db(b, 0.3):.1f}")


if __name__ == "__main__":
    main()
