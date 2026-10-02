"""Beat tracking with numpy alone (no librosa or scipy): a music file's tempo, its beats and its bars' first beats.

    python beats.py music.mp3 [--bpm 128] [--start 0] [--length 120]

The method is the classic one (Ellis 2007, "Beat Tracking by Dynamic Programming"): an onset strength envelope (the
spectral flux of the log spectrum, 11.6 ms steps), the tempo from its autocorrelation (weighted towards 120 BPM, between
60 and 200), then the beats as the path through the envelope that best keeps that tempo. 4/4 is assumed for the bars:
their first beat is the phase of four whose beats have the most kick drum (the onsets under 150 Hz).
"""

import argparse
import sys

import numpy as np

import media

RATE = 22050
N_FFT = 1024
HOP = 256
LOW_HZ = 150.0  # the kick drum's band, for the bars' first beats


def onset_envelopes(samples):
    """The onset strength per HOP samples (frame k at frame_time(k)), normalized: of the whole spectrum, and of its
    band under LOW_HZ (the kick drum's)."""
    if samples.size < N_FFT * 2:
        return np.zeros(0), np.zeros(0)
    count = 1 + (samples.size - N_FFT) // HOP
    window = np.hanning(N_FFT).astype(np.float32)
    low = np.fft.rfftfreq(N_FFT, 1.0 / RATE) < LOW_HZ
    env = np.zeros(count)
    envLow = np.zeros(count)
    prev = None
    for c0 in range(0, count, 4096):
        c1 = min(count, c0 + 4096)
        idx = np.arange(N_FFT)[None, :] + HOP * np.arange(c0, c1)[:, None]
        mag = np.log1p(1000.0 * np.abs(np.fft.rfft(samples[idx] * window[None, :], axis=1)))
        first = c0
        if prev is not None:
            mag = np.vstack([prev[None, :], mag])
        else:
            first = c0 + 1
        rise = np.maximum(0.0, np.diff(mag, axis=0))
        env[first:c1] = rise.sum(axis=1)
        envLow[first:c1] = rise[:, low].sum(axis=1)
        prev = mag[-1]

    def normal(e):
        # Less its local mean (a second), half-wave rectified, to unit spread.
        k = int(RATE / HOP)
        e = np.maximum(0.0, e - np.convolve(e, np.ones(k) / k, mode="same"))
        sd = e.std()
        return e / sd if sd > 0 else e

    return normal(env), normal(envLow)


# A frame's time: its window's middle, plus the flux's lead (an onset raises the spectrum before the window's middle
# reaches it: measured on make_test_media.py's music, 13 ms early with these sizes).
LEAD = 0.013


def frame_time(k):
    return (k * HOP + N_FFT / 2) / RATE + LEAD


def tempo(env, lo=60.0, hi=200.0, centre=120.0):
    """BPM: the period whose multiples (1 to 4 periods: a comb) the envelope's autocorrelation is strongest at, read
    between whole frames, weighted by a log-normal prior around `centre`. (One lag alone is fooled when the beat falls
    between two frames: a sharp beat's peak splits in two.)"""
    fps = RATE / HOP
    # Smoothed a little (a beat's onset spread over its neighbouring frames), less its mean.
    kernel = np.exp(-0.5 * (np.arange(-3, 4) / 1.0) ** 2)
    e = np.convolve(env, kernel / kernel.sum(), mode="same")
    e = e - e.mean()
    n = e.size
    spec = np.fft.rfft(e, 2 * n)
    ac = np.fft.irfft(spec * np.conj(spec))[:n]
    ac = ac / (ac[0] if ac[0] > 0 else 1.0)
    lags = np.arange(n, dtype=float)
    candidates = np.arange(lo, hi + 0.05, 0.1)
    period = 60.0 * fps / candidates
    comb = np.zeros_like(candidates)
    for k, w in ((1, 1.0), (2, 0.5), (3, 0.33), (4, 0.25)):
        at = k * period
        comb += w * np.interp(at, lags, ac, right=0.0)
    prior = np.exp(-0.5 * (np.log2(candidates / centre) / 0.9) ** 2)
    return float(candidates[int(np.argmax(comb * prior))])


def track(env, bpm, tightness=100.0):
    """The beats' frames: the dynamic programme's best path at `bpm`."""
    period = (RATE / HOP) * 60.0 / bpm
    n = env.size
    score = env.astype(float).copy()
    back = np.full(n, -1)
    lo, hi = int(round(period / 2)), int(round(2 * period))
    offsets = np.arange(lo, hi + 1)
    penalty = -tightness * np.log(offsets / period) ** 2
    for i in range(lo, n):
        j = i - offsets
        valid = j >= 0
        if not valid.any():
            continue
        cand = np.where(valid, score[np.maximum(j, 0)] + penalty, -np.inf)
        k = int(np.argmax(cand))
        if cand[k] > 0:
            score[i] = env[i] + cand[k]
            back[i] = j[k]
    # The end: the best score in the last period.
    tail = max(0, n - int(round(period)))
    i = tail + int(np.argmax(score[tail:]))
    beats = []
    while i >= 0:
        beats.append(i)
        i = back[i]
    return np.array(beats[::-1])


def analyse(path, bpm=None, start=0.0, length=None):
    """(bpm, beat times, bar times) of a music file, times from the file's start."""
    samples, first = media.audio(path, RATE, start, length)
    env, envLow = onset_envelopes(samples)
    if env.size < 8:
        return 0.0, np.zeros(0), np.zeros(0)
    if not bpm:
        bpm = tempo(env)
    frames = track(env, bpm)
    times = np.array([first + frame_time(k) for k in frames])
    strength = [envLow[frames[p::4]].mean() if frames[p::4].size else 0.0 for p in range(4)]
    phase = int(np.argmax(strength))
    return bpm, times, times[phase::4]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("music")
    p.add_argument("--bpm", type=float, default=None, help="the tempo, when known (else found)")
    p.add_argument("--start", type=float, default=0.0)
    p.add_argument("--length", type=float, default=None)
    a = p.parse_args()
    bpm, beats, bars = analyse(a.music, a.bpm, a.start, a.length)
    print("tempo: %.2f BPM, %d beats, %d bars" % (bpm, len(beats), len(bars)))
    print("first beats: " + " ".join("%.3f" % t for t in beats[:8]))
    print("first bars:  " + " ".join("%.3f" % t for t in bars[:4]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
