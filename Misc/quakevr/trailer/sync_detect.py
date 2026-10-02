"""Finds a highlight log's sync mark in a recording (docs/vr-port/TRAILER.md): the white flash the game draws over the
desktop window as a log starts (vr_highlights_flash) and its 1 kHz beep (vr_highlights_beep, vr/sync_beep.wav).

    python sync_detect.py recording.mp4 [--start 0] [--search 300] [--nth 1]

Prints where each was found (seconds into the recording) and the sync offset to use: the flash's first frame (exact to
a frame; the beep comes a little later, the sound's own latency); the beep's when the video has no flash (the window
wasn't captured, or vr_highlights_flash 0).
"""

import argparse
import sys

import numpy as np

import media


def find_flashes(path, start=0.0, search=300.0):
    """The flashes in [start, start + search]: (first frame's time, its last frame's time), in order. A flash: a
    bright frame (mean luma over 170) after one at least 60 darker. (The game's white comes out of the window's
    post-process a little tinted: 156 230 255 at the default settings, luma 210.)"""
    times, values = media.brightness(path, start, search)
    flashes = []
    i = 1
    while i < len(values):
        if values[i] > 170 and values[i] - values[i - 1] > 60:
            j = i
            while j + 1 < len(values) and values[j + 1] > 170:
                j += 1
            flashes.append((float(times[i]), float(times[j])))
            i = j + 1
        else:
            i += 1
    return flashes


def find_beeps(path, start=0.0, search=300.0, rate=22050, freq=1000.0):
    """The beeps' starts in [start, start + search]: where the sound at `freq` (+-40 Hz) rises to most of it, at least
    0.15 s long. Resolution: 5.8 ms."""
    samples, first = media.audio(path, rate, start, search)
    if samples.size < 2048:
        return []
    n, hop = 1024, 128
    count = 1 + (samples.size - n) // hop
    window = np.hanning(n).astype(np.float32)
    bins = np.fft.rfftfreq(n, 1.0 / rate)
    inBand = (bins > freq - 40) & (bins < freq + 40)
    band = np.empty(count)
    total = np.empty(count)
    for c0 in range(0, count, 4096):  # (in pieces: a long search's spectra at once would take gigabytes)
        c1 = min(count, c0 + 4096)
        idx = np.arange(n)[None, :] + hop * np.arange(c0, c1)[:, None]
        spec = np.abs(np.fft.rfft(samples[idx] * window[None, :], axis=1)) ** 2
        band[c0:c1] = spec[:, inBand].sum(axis=1)
        total[c0:c1] = spec.sum(axis=1) + 1e-12
    ratio = band / total
    loud = band > max(1e-6, band.max() * 0.05)
    on = (ratio > 0.6) & loud
    beeps = []
    need = int(0.15 * rate / hop)
    i = 0
    while i < count:
        if on[i]:
            j = i
            while j + 1 < count and on[j + 1]:
                j += 1
            if j - i + 1 >= need:
                # Its rise: the first frame over half the beep's band power; the frame's centre is the sound's time.
                peak = band[i:j + 1].max()
                k = i
                while k > 0 and band[k - 1] > peak * 0.02:
                    k -= 1
                while band[k] < peak * 0.5:
                    k += 1
                beeps.append(first + (k * hop + n / 2) / rate)
            i = j + 1
        else:
            i += 1
    return beeps


def detect(path, start=0.0, search=300.0, nth=1, verbose=True):
    """The sync offset (seconds into the recording) of its `nth` sync mark, or None."""
    info = media.Info(path)
    flashes = find_flashes(path, start, search) if info.has_video else []
    beeps = find_beeps(path, start, search) if info.has_audio else []
    if verbose:
        for a, b in flashes:
            print("flash: %.3f s (%d frames)" % (a, round((b - a) * float(info.fps)) + 1))
        for b in beeps:
            print("beep:  %.3f s" % b)
    if len(flashes) >= nth:
        sync = flashes[nth - 1][0]
        near = [b for b in beeps if -0.1 < b - sync < 0.5]
        if verbose and near:
            print("beep after the flash: %+.3f s" % (near[0] - sync))
        return sync
    if len(beeps) >= nth:
        return beeps[nth - 1]
    return None


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("recording")
    p.add_argument("--start", type=float, default=0.0, help="seconds into the recording to start looking")
    p.add_argument("--search", type=float, default=300.0, help="seconds to look through")
    p.add_argument("--nth", type=int, default=1, help="which sync mark (vr_highlights_sync makes more)")
    a = p.parse_args()
    sync = detect(a.recording, a.start, a.search, a.nth)
    if sync is None:
        print("no sync mark found")
        return 1
    print("sync: %.3f" % sync)
    return 0


if __name__ == "__main__":
    sys.exit(main())
