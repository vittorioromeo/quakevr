"""Test media for the trailer scripts (selftest.py): a short "recording" with a sync mark as the game makes it, and a
music track with a known tempo. Nothing of id's: plain colours and tones.

    python make_test_media.py OUTDIR [--sync 3.0] [--length 40] [--bpm 126] [--log LOG.csv]

Writes OUTDIR/recording.mp4 (640x360, 60 fps, H.264 and AAC: what OBS records), with a white flash 0.2 s long at
--sync and the 1 kHz beep 40 ms after it (the sound's latency), some loud noise bursts (shots) elsewhere;
OUTDIR/music.wav (--bpm, its first beat at 0.5 s: kick on the beats, snare on 2 and 4, hats on the eighths); and
OUTDIR/highlights.csv, a log of moments over the recording (or --log's rows, those that fit it).
"""

import argparse
import csv
import math
import os
import sys
import wave

import av
import numpy as np

FPS = 60
W, H = 640, 360
AUDIO_RATE = 48000


def recording(path, length, sync):
    rng = np.random.default_rng(7)
    c = av.open(path, "w")
    vs = c.add_stream("libx264", rate=FPS)
    vs.width, vs.height, vs.pix_fmt = W, H, "yuv420p"
    vs.options = {"crf": "28", "preset": "veryfast"}
    aus = c.add_stream("aac", rate=AUDIO_RATE)
    aus.layout = "stereo"

    n = int(length * FPS)
    ys, xs = np.mgrid[0:H, 0:W]
    for i in range(n):
        t = i / FPS
        if sync <= t < sync + 0.2:
            img = np.empty((H, W, 3), np.uint8)
            img[:] = (156, 230, 255)  # (the game's white flash as the window shows it: its post-process tints it)
        else:
            # A dim scene that changes: a slow colour drift and a block crossing it.
            base = np.array([60 + 30 * math.sin(t * 0.7), 70 + 20 * math.sin(t * 0.3 + 1), 80], np.float32)
            img = np.empty((H, W, 3), np.uint8)
            img[:] = (base + 10 * np.sin(xs[..., None] / 40.0 + t)).clip(0, 255).astype(np.uint8)
            x0 = int((t * 120) % (W - 80))
            img[140:220, x0:x0 + 80] = (200, 120, 40)
        frame = av.VideoFrame.from_ndarray(img, format="rgb24")
        frame.pts = i
        for p in vs.encode(frame):
            c.mux(p)
    for p in vs.encode():
        c.mux(p)

    total = int(length * AUDIO_RATE)
    sound = rng.normal(0, 0.01, total).astype(np.float32)
    # Shots: loud noise bursts.
    for at in np.arange(5.0, length - 1, 2.7):
        k = int(at * AUDIO_RATE)
        burst = rng.normal(0, 0.4, int(0.12 * AUDIO_RATE)) * np.exp(-np.arange(int(0.12 * AUDIO_RATE)) / 1500.0)
        sound[k:k + burst.size] += burst[:max(0, total - k)].astype(np.float32)
    # The beep: 1 kHz, 0.25 s, 5 ms ramps, 40 ms after the flash.
    k = int((sync + 0.04) * AUDIO_RATE)
    m = int(0.25 * AUDIO_RATE)
    ramp = int(0.005 * AUDIO_RATE)
    j = np.arange(m)
    beep = 0.5 * np.sin(2 * np.pi * 1000 * j / AUDIO_RATE) * np.minimum(1, np.minimum(j / ramp, (m - 1 - j) / ramp))
    sound[k:k + m] += beep.astype(np.float32)
    stereo = np.vstack([sound, sound]).astype(np.float32)
    for s0 in range(0, total, 1024):
        chunk = np.ascontiguousarray(stereo[:, s0:s0 + 1024])
        frame = av.AudioFrame.from_ndarray(chunk, format="fltp", layout="stereo")
        frame.sample_rate = AUDIO_RATE
        frame.pts = s0
        for p in aus.encode(frame):
            c.mux(p)
    for p in aus.encode():
        c.mux(p)
    c.close()


def music(path, length, bpm, first=0.5, rate=44100):
    rng = np.random.default_rng(11)
    total = int(length * rate)
    out = np.zeros(total, np.float32)
    beat = 60.0 / bpm

    def put(at, sig):
        k = int(at * rate)
        if k < total:
            out[k:k + sig.size] += sig[:total - k]

    t = np.arange(int(0.3 * rate)) / rate
    kick = (np.sin(2 * np.pi * (50 + 80 * np.exp(-t * 30)) * t) * np.exp(-t * 12)).astype(np.float32)
    ts = np.arange(int(0.15 * rate)) / rate
    snare = (rng.normal(0, 0.5, ts.size) * np.exp(-ts * 25)).astype(np.float32)
    th = np.arange(int(0.05 * rate)) / rate
    hat = (rng.normal(0, 0.15, th.size) * np.exp(-th * 80)).astype(np.float32)
    k = 0
    while first + k * beat < length:
        at = first + k * beat
        put(at, kick * (1.0 if k % 4 == 0 else 0.8))
        if k % 4 in (1, 3):
            put(at, snare)
        put(at, hat)
        put(at + beat / 2, hat)
        k += 1
    # A pad: a chord that changes every bar (no beats of its own).
    tt = np.arange(total) / rate
    out += 0.05 * np.sin(2 * np.pi * 220 * tt).astype(np.float32)
    out = (out / max(1e-6, np.abs(out).max()) * 0.8 * 32767).astype(np.int16)
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(np.repeat(out, 2).tobytes())
    return [first + i * beat for i in range(k)]


SYNTHETIC = [  # t, kind, score, duration, count, subject, detail
    (1.5, "kill", 1, 0, 1, "monster_army", "Shotgun"),
    (3.2, "gib", 5, 0, 1, "monster_dog", "rocket blast"),
    (6.0, "parry", 3, 0, 1, "monster_knight", "Knight's Sword, main hand"),
    (6.9, "counter", 4, 0, 1, "monster_knight", "slash"),
    (11.0, "bullettime", 4, 5.5, 1, "", "0.30x"),
    (12.5, "gib", 6, 0, 1, "monster_ogre", "Grenade Launcher headshot"),
    (14.0, "multikill", 6, 1.2, 3, "monster_army monster_army monster_dog", "3 kills"),
    (19.0, "shotgrenade", 6, 0, 1, "grenade", "spike"),
    (22.5, "axestick", 3, 0, 1, "monster_zombie", "Axe at 900 u/s"),
    (26.0, "explosion", 5, 0, 1, "explo_box", "barrel, 2 kills"),
    (26.1, "chain", 4, 0, 1, "explo_box", "barrel"),
    (30.0, "swing", 4.2, 2.2, 1, "", "2.2 s"),
    (33.0, "shoveparry", 4, 0, 1, "monster_enforcer", "crossed arms"),
]


def log(path, length, sync, source=None):
    fields = ["t", "game_time", "kind", "score", "duration", "count", "map", "subject", "detail"]
    rows = []
    if source:
        with open(source, encoding="utf-8", newline="") as f:
            rows = [r for r in csv.DictReader(f) if sync + float(r["t"]) + float(r["duration"]) < length - 0.5]
    else:
        rows.append(dict(t=0, game_time=1, kind="sync", score=0, duration=0, count=1, map="e1m1", subject="",
                         detail="start"))
        for t, kind, score, dur, count, subj, detail in SYNTHETIC:
            if sync + t + dur < length - 0.5:
                rows.append(dict(t=t, game_time=1 + t, kind=kind, score=score, duration=dur, count=count, map="e1m1",
                                 subject=subj, detail=detail))
    with open(path, "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)
    return len(rows)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("out")
    p.add_argument("--sync", type=float, default=3.0)
    p.add_argument("--length", type=float, default=40.0)
    p.add_argument("--bpm", type=float, default=126.0)
    p.add_argument("--music-length", type=float, default=70.0)
    p.add_argument("--log", help="a real highlight log to use (its rows that fit the recording)")
    a = p.parse_args()
    os.makedirs(a.out, exist_ok=True)
    recording(os.path.join(a.out, "recording.mp4"), a.length, a.sync)
    beats = music(os.path.join(a.out, "music.wav"), a.music_length, a.bpm)
    n = log(os.path.join(a.out, "highlights.csv"), a.length, a.sync, a.log)
    print("%s: recording.mp4 (%.0f s, sync at %.3f s), music.wav (%.0f BPM, %d beats from 0.5 s), highlights.csv "
          "(%d rows)" % (a.out, a.length, a.sync, a.bpm, len(beats), n))
    return 0


if __name__ == "__main__":
    sys.exit(main())
