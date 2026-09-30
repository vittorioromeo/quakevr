#!/usr/bin/env python3
# make_chainsaw_sounds.py -- cuts the ogres' chainsaw's sounds (QC vr_chainsaw.qc, vr_chainsaw.cpp) out of two recordings
# by Joseph Sardin, BigSoundBank, CC0 (public domain; see CREDITS.md), into quakevr/sound/vr/:
#
#   0982  "Chainsaw (Starting)", a Stihl MS260 (48 kHz mono):  https://bigsoundbank.com/sound-0982-chainsaw-starting.html
#         download: https://bigsoundbank.com/UPLOAD/bwf-en/0982.wav (3.6 MB)
#   0707  "Chainsaw #2", starting it, cutting a log (48 kHz mono):  https://bigsoundbank.com/chainsaw-2-s0707.html
#         download: https://bigsoundbank.com/UPLOAD/bwf-en/0707.wav (11 MB)
#
# The recordings are not in the repository (id-free, but large): download them into a folder and run
#   python Misc/quakevr/make_chainsaw_sounds.py <that folder> [out folder, default quakevr/sound/vr]
#
# What is cut (seconds in the recording):
#   saw_pull_weak.wav  0982 0.40..1.20  a pull that doesn't fire: the cord's zip, the engine turned over (QC: a pull
#                      with the engine already running, or no fuel; the client: a pull too slow, vr_chainsaw.cpp)
#   saw_pull.wav       0982 1.60..3.60  a strong pull: the zip, the engine firing a moment and dying (it didn't catch)
#   saw_start.wav      0982 6.35..11.0  the pull that starts it: it catches, races, settles; then the idle's loop
#                      (below) with a cue point: it idles on by itself once the start is over
#   saw_idle.wav       0982 11.0..~13   idling (~62 firings a second): the same loop as the start's tail
#   saw_run.wav        0707 28.35..~29.4  flat out, the chain running free (~188 a second): the rev before the log
#   saw_cut.wav        0707 43.0..~46   cutting the log: loaded, bogging down and picking up (~150 a second)
#   saw_stall.wav      0982 35.4..37.9  out of fuel: the idle dying away
#
# Loops: a loop's length is searched (within +-12%) for the best match of its end with its start (the correlation of the
# crossfade windows), then its last XFADE seconds are crossfaded (with gains keeping the power for that correlation)
# into the samples just before its start: its last sample runs on into its first. The loop is resampled
# circularly (FFT) to 22050 Hz, so the wrap stays seamless. Its cue point (and a LIST/ltxt "mark" with its length) is
# what Quake reads (snd_mem.c GetWavinfo): any channel playing it loops it until another sound replaces it.
# One-shots: resampled with the recording around them, trimmed, faded in 3 ms and out 40 ms.
# Levels: one gain for everything from 0982 (the idle at -20 dBFS RMS), one for 0707 (its loops at -15 dBFS), both raised
# by LOUDER_DB (the author, 2026-09-30: "a tiny bit louder"); peaks are kept under -1 dBFS by a soft knee.
#
# Without the recordings, the files made can be made louder or softer in place (their loops' cue points kept):
#   python Misc/quakevr/make_chainsaw_sounds.py --regain <dB> [folder, default quakevr/sound/vr]
# undoes the soft knee, scales and knees again, as making them at the new level would (to a sample's rounding). The
# files shipped were made at LOUDER_DB 0 and raised with --regain 3.

import os
import struct
import sys
import wave

import numpy as np

SRC_RATE = 48000
RATE = 22050
# 48000/22050 = 320/147: cuts on multiples of 320 source samples are whole numbers of output samples.
STEP = 320
XFADE = 0.12  # seconds: the loops' crossfade


def load(path):
    w = wave.open(path)
    assert w.getframerate() == SRC_RATE and w.getsampwidth() == 2, path
    a = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64) / 32768.0
    a = a.reshape(-1, w.getnchannels()).mean(axis=1)
    # Rumble and DC out (a first-order high-pass at 25 Hz: the idle's lowest firing is ~60 Hz).
    k = np.exp(-2 * np.pi * 25.0 / SRC_RATE)
    out = np.empty_like(a)
    prev_in = prev_out = 0.0
    for i, v in enumerate(a):  # (a few million samples: fine)
        prev_out = k * (prev_out + v - prev_in)
        prev_in = v
        out[i] = prev_out
    return out


def snap(seconds):
    return int(round(seconds * SRC_RATE / STEP)) * STEP


def resample_circular(x):
    n = len(x)
    assert n % STEP == 0
    m = n // STEP * 147
    X = np.fft.rfft(x)
    Y = np.zeros(m // 2 + 1, dtype=complex)
    k = min(len(X), len(Y))
    Y[:k] = X[:k]
    if m % 2 == 0:
        Y[-1] = Y[-1].real  # the Nyquist bin of an even length is real
    return np.fft.irfft(Y, m) * (m / n)


def resample_segment(a, start, end, pad=0.1):
    """a[start:end] resampled, with the recording around it (so its edges aren't wrapped)."""
    p = snap(pad)
    seg = np.pad(a, p)[start:end + 2 * p]  # (silence past the recording's ends)
    y = resample_circular(seg)
    q = p // STEP * 147
    return y[q:len(y) - q]


def make_loop(a, start, length_guess):
    """A loop beginning at `start` (its crossfade taken from the samples before it): (the loop at 48 kHz, its length)."""
    x = snap(XFADE)
    lo, hi = snap(length_guess * 0.88), snap(length_guess * 1.12)
    # Matched on the engine's firings (below 400 Hz), not its hiss: the crossfade then lines up the pulses.
    span = a[start - x:start + hi]
    S = np.fft.rfft(span)
    S[np.fft.rfftfreq(len(span), 1 / SRC_RATE) > 400.0] = 0
    low = np.fft.irfft(S, len(span))
    head = low[:x]
    best = None
    for n in range(lo, hi + 1, STEP // 4):  # (a quarter step: 80 source samples, 1.7 ms)
        tail = low[n:n + x]
        r = float(np.dot(head, tail) / (np.linalg.norm(head) * np.linalg.norm(tail) + 1e-12))
        if best is None or r > best[0]:
            best = (r, n)
    r, n = best
    n = n // STEP * STEP if n % STEP < STEP // 2 else (n // STEP + 1) * STEP  # back onto a whole output sample
    head, tail = a[start - x:start], a[start + n - x:start + n]
    r = float(np.dot(head, tail) / (np.linalg.norm(head) * np.linalg.norm(tail) + 1e-12))
    loop = a[start:start + n].copy()
    # tail fades out into head fading in: the loop's end becomes the samples right before its start.
    t = (np.arange(x) + 0.5) / x
    fin = np.sin(0.5 * np.pi * t)   # head
    fout = np.cos(0.5 * np.pi * t)  # tail
    # power kept for correlation r: g^2 (fin^2 + fout^2 + 2 r fin fout) = 1
    g = 1.0 / np.sqrt(fin ** 2 + fout ** 2 + 2 * max(r, 0.0) * fin * fout)
    loop[n - x:] = g * (fout * tail + fin * head)
    return loop, n, r


def fades(y, fade_in=0.003, fade_out=0.04):
    y = y.copy()
    i, o = int(fade_in * RATE), int(fade_out * RATE)
    y[:i] *= np.linspace(0, 1, i, endpoint=False)
    y[len(y) - o:] *= np.linspace(1, 0, o)
    return y


def soft_limit(y, ceiling=0.89):
    """Below 0.7 x the ceiling untouched; above, a tanh knee up to the ceiling."""
    knee = 0.7 * ceiling
    s = np.sign(y)
    m = np.abs(y)
    over = m > knee
    m[over] = knee + (ceiling - knee) * np.tanh((m[over] - knee) / (ceiling - knee))
    return s * m


def unsoft_limit(y, ceiling=0.89):
    """soft_limit's inverse (a sample at the ceiling, which rounding can reach, as just under it)."""
    knee = 0.7 * ceiling
    s = np.sign(y)
    m = np.abs(y).astype(np.float64)
    over = m > knee
    t = np.clip((m[over] - knee) / (ceiling - knee), 0.0, 0.999)
    m[over] = knee + (ceiling - knee) * np.arctanh(t)
    return s * m


def regain(folder, db):
    """Scales the sound files made here by `db` in place: every chunk but the samples kept (the loops' cue points)."""
    g = 10 ** (db / 20)
    for name in sorted(os.listdir(folder)):
        if not (name.startswith("saw_") and name.endswith(".wav")):
            continue
        path = os.path.join(folder, name)
        with open(path, "rb") as f:
            raw = bytearray(f.read())
        pos = 12
        while pos + 8 <= len(raw):
            cid, size = raw[pos:pos + 4], struct.unpack("<I", raw[pos + 4:pos + 8])[0]
            if cid == b"data":
                y = np.frombuffer(bytes(raw[pos + 8:pos + 8 + size]), dtype="<i2").astype(np.float64) / 32767
                before = rms_db(y)
                y = soft_limit(unsoft_limit(y) * g)
                raw[pos + 8:pos + 8 + size] = np.clip(np.round(y * 32767), -32768, 32767).astype("<i2").tobytes()
                print(f"{name}: rms {before:.1f} -> {rms_db(y):.1f} dBFS, peak {np.max(np.abs(y)):.2f}")
            pos += 8 + size + (size & 1)
        with open(path, "wb") as f:
            f.write(raw)


def rms_db(y):
    return 20 * np.log10(np.sqrt(np.mean(y ** 2)) + 1e-12)


def write_wav(path, y, loop_start=None):
    pcm = np.clip(np.round(y * 32767), -32768, 32767).astype("<i2").tobytes()
    chunks = [b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16), b"data" + struct.pack("<I", len(pcm)) + pcm]
    if len(pcm) % 2:
        chunks[-1] += b"\0"
    if loop_start is not None:
        n = len(y)
        # one cue point: id 1, position, 'data', chunk start 0, block start 0, sample offset (Quake reads this one)
        chunks.append(b"cue " + struct.pack("<II", 28, 1) + struct.pack("<II4sIII", 1, loop_start, b"data", 0, 0, loop_start))
        # LIST adtl ltxt: cue 1, its length in samples, purpose "mark" (Quake: the loop's length)
        ltxt = struct.pack("<II4s4H", 1, n - loop_start, b"mark", 0, 0, 0, 0)
        chunks.append(b"LIST" + struct.pack("<I", 4 + 8 + len(ltxt)) + b"adtl" + b"ltxt" + struct.pack("<I", len(ltxt)) + ltxt)
    body = b"WAVE" + b"".join(chunks)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", len(body)) + body)


LOUDER_DB = 3.0


def main():
    if len(sys.argv) > 2 and sys.argv[1] == "--regain":
        folder = sys.argv[3] if len(sys.argv) > 3 else os.path.join(os.path.dirname(__file__), "..", "..", "quakevr", "sound", "vr")
        regain(folder, float(sys.argv[2]))
        return
    src = sys.argv[1]
    out_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "..", "..", "quakevr", "sound", "vr")
    start = load(os.path.join(src, "0982.wav"))
    log = load(os.path.join(src, "0707.wav"))

    # 0982: the idle's loop, and the start running into it.
    idle_at = snap(11.0)
    idle48, idle_n, idle_r = make_loop(start, idle_at, 2.0)
    pre48 = start[snap(6.35):idle_at]
    g_start = 10 ** ((-20.0 + LOUDER_DB - rms_db(idle48)) / 20)
    idle = resample_circular(idle48) * g_start
    pre = resample_segment(start, snap(6.35), idle_at) * g_start
    pre[: int(0.003 * RATE)] *= np.linspace(0, 1, int(0.003 * RATE), endpoint=False)
    start_wav = np.concatenate([pre, idle])

    one = lambda t0, t1, fo=0.04: fades(resample_segment(start, snap(t0), snap(t1)) * g_start, fade_out=fo)
    files = {
        "saw_pull_weak.wav": (one(0.40, 1.20, 0.08) * 2.0, None),  # (+6 dB: recorded softer than the rest)
        "saw_pull.wav": (one(1.60, 3.60, 0.15), None),
        "saw_start.wav": (start_wav, len(pre)),
        "saw_idle.wav": (idle, 0),
        "saw_stall.wav": (one(35.4, 37.9, 0.3), None),
    }
    # 0707: flat out, and cutting.
    run48, run_n, run_r = make_loop(log, snap(28.35), 1.0)
    cut48, cut_n, cut_r = make_loop(log, snap(43.0), 3.0)
    g_log = 10 ** ((-15.0 + LOUDER_DB - rms_db(np.concatenate([run48, cut48]))) / 20)
    files["saw_run.wav"] = (resample_circular(run48) * g_log, 0)
    files["saw_cut.wav"] = (resample_circular(cut48) * g_log, 0)

    for name, (y, loop) in files.items():
        y = soft_limit(y)
        write_wav(os.path.join(out_dir, name), y, loop)
        # The wrap's step (last sample to the loop's first) among the file's steps between neighbours: a percentile.
        seam = (f", wrap step {abs(y[loop] - y[-1]):.4f}: percentile {np.mean(np.abs(np.diff(y)) < abs(y[loop] - y[-1])) * 100:.0f}"
                if loop is not None else "")
        print(f"{name}: {len(y) / RATE:.2f} s, loop from {loop}" if loop is not None else f"{name}: {len(y) / RATE:.2f} s",
              f"rms {rms_db(y):.1f} dBFS, peak {np.max(np.abs(y)):.2f}{seam}")
    print(f"loops: idle {idle_n / SRC_RATE:.3f} s (match {idle_r:.2f}), run {run_n / SRC_RATE:.3f} s ({run_r:.2f}), "
          f"cut {cut_n / SRC_RATE:.3f} s ({cut_r:.2f})")


if __name__ == "__main__":
    main()
