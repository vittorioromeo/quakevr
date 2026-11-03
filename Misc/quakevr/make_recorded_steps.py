#!/usr/bin/env python3
# make_recorded_steps.py -- cuts the great bare feet and the worn backpack's rip from recordings (quakevr/sound/vr):
#   step_thud1..5.wav   ogres, shamblers, fiends (vr_footsteps.cpp; the monsters' size is the engine's: a rate and a gain per kind)
#   pack_rip.wav        a worn backpack coming off (QC vr_wornpack.qc VR_WP_Off): only with --rip, into fs_pack_rip.wav beside the
#                       sources: the shipped file has his own edit on it (2026-11-03)
#   armor_crack.wav     an armed humanoid's plate breaking (QC vr_agarmor.qc VR_AgArmor_Break): only with --crack (no thuds then);
#                       --crack=<id> cuts a runner-up of CRACK_TAKES under the same name
#
# Sources, all CC0 (each page checked to say so). Freesound's HQ MP3 previews: the originals need a login. Not in the repository:
#       542858  "Giant footsteps", thebestmaker                  https://freesound.org/people/thebestmaker/sounds/542858/
#       721601  "Giant step", DasWaff                            https://freesound.org/people/DasWaff/sounds/721601/
#       591527  "Cloth Rip Even Long", Rudmer_Rotteveel          https://freesound.org/people/Rudmer_Rotteveel/sounds/591527/
#       51263   "Metal Cracking 2.wav", RutgerMuller             https://freesound.org/people/RutgerMuller/sounds/51263/
#       518731  "Cracking, snapping, and clanging (metal can)", Ashley12275  https://freesound.org/people/Ashley12275/sounds/518731/
#       875432  "Egg Steel Crack Hard", loganzsound              https://freesound.org/people/loganzsound/sounds/875432/
# Decode each into one folder first (this script has no decoder and uses only the standard library):
#       ffmpeg -i preview_hq.mp3 -ac 1 -ar 22050 -c:a pcm_s16le fs_<id>.wav
#
# Usage: python Misc/quakevr/make_recorded_steps.py <folder of fs_<id>.wav> [--rip] [--crack[=<id>]] [out_dir]   (default: quakevr/sound/vr)
#
# A thud: one stamp of 542858 (the foot: most of its energy is the 150-800 Hz a headset's speakers can play) over 721601's stomp
# low-passed (the weight, without the branches it breaks). Each is cut at its onset, so the first sample is the contact.

import math
import os
import struct
import sys
import wave

RATE = 22050
THUD_SECONDS = 0.5
THUD_PEAK = 0.75
THUD_DRIVE = 2.2  # (tanh: the level held up to the peak; today's mid-range and loudness or more)
WEIGHT_CUT = 900.0  # Hz: the stomp under this
FOOT_CUT = 2500.0  # Hz: the stamp's top softened (a great foot, no slap)
RIP_ONSET = 0.02
RIP_SECONDS = 0.62
RIP_FADE = 0.14
RIP_HIGH_PASS = 150.0
RIP_TOP = 8000.0  # Hz: the fibres' crackle, its sharpest edges softened once it is driven
RIP_DRIVE = 5.0
RIP_PEAK = 0.8
CRACK_HIGH_PASS = 120.0
CRACK_TOP = 9000.0  # Hz: the preview's fizz, once it is driven
CRACK_BODY_CUT = 2500.0  # Hz: the crack an octave down under it, this low-passed: the plate's weight, without a ring
CRACK_PEAK = 0.9  # (the old break's: the pickup's chime backwards, peak 0.91, loudest 100 ms -12.8 dB)
CRACK_FADE = 0.08
CRACK_TAKES = {
    # Freesound id: the onset's second, s kept, rate, the body's level, drive
    51263: (1.281, 0.42, 0.85, 0.3, 2.2),
    518731: (0.744, 0.42, 0.85, 0.3, 1.8),
    875432: (2.724, 0.42, 0.85, 0.3, 2.4),
}

# Seconds in the sources.
STOMP = 0.700  # 721601's main stomp
STOMP2 = 1.085 # and the smaller one after it

THUD_TAKES = (
    # 542858's stamp at, its level; 721601's stomp at, its rate, its level
    (0.6432, 1.0, STOMP, 1.0, 0.9),
    (2.1263, 1.0, STOMP, 0.9, 0.9),
    (3.4156, 1.0, STOMP, 1.1, 0.9),
    (4.7347, 1.0, STOMP2, 0.95, 1.3),
    (2.1263, 1.0, STOMP, 0.85, 1.0),
)
FOOT_RATES = (1.0, 1.0, 1.0, 1.0, 0.86)  # 542858's stamp's rate per take: the last one, a lower one of the same stamp


def read_wav(path):
    with wave.open(path, "rb") as w:
        if w.getframerate() != RATE or w.getnchannels() != 1 or w.getsampwidth() != 2:
            raise SystemExit("%s: decode to 22050 Hz, mono, 16-bit first (see the header)" % path)
        raw = w.readframes(w.getnframes())
    return [v / 32768.0 for v in struct.unpack("<%dh" % (len(raw) // 2), raw)]


def write_wav(path, samples):
    data = b"".join(struct.pack("<h", max(-32767, min(32767, int(s * 32767)))) for s in samples)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(data)) + data)


def cut(samples, at, seconds, rate=1.0):
    """`seconds` of `samples` from `at`, played at `rate` (under 1: lower and longer)."""
    out = []
    for i in range(int(seconds * RATE)):
        where = at * RATE + i * rate
        j = int(where)
        if j + 1 >= len(samples):
            break
        out.append(samples[j] + (samples[j + 1] - samples[j]) * (where - j))
    return out


def low_passed(samples, cut_hz, passes=2):
    a = 1.0 - math.exp(-2 * math.pi * cut_hz / RATE)
    for _ in range(passes):
        y = 0.0
        filtered = []
        for s in samples:
            y += a * (s - y)
            filtered.append(y)
        samples = filtered
    return samples


def high_passed(samples, cut_hz):
    return [s - y for s, y in zip(samples, low_passed(samples, cut_hz, 1))]


def normalised(samples):
    peak = max(abs(s) for s in samples) or 1.0
    return [s / peak for s in samples]


def finish(samples, drive, peak, fade_in, fade_out):
    """Saturated, faded at both ends (no click), to `peak`."""
    out = [math.tanh(drive * s) for s in samples]
    n = len(out)
    for i in range(int(fade_in * RATE)):
        out[i] *= i / (fade_in * RATE)
    for i in range(int(fade_out * RATE)):
        out[n - 1 - i] *= i / (fade_out * RATE)
    most = max(abs(s) for s in out) or 1.0
    return [s * peak / most for s in out]


def thud(stamps, stomps, take):
    stamp_at, stamp_level, stomp_at, stomp_rate, stomp_level = THUD_TAKES[take]
    foot = normalised(low_passed(cut(stamps, stamp_at, THUD_SECONDS, FOOT_RATES[take]), FOOT_CUT, 1))
    weight = normalised(low_passed(cut(stomps, stomp_at, THUD_SECONDS, stomp_rate), WEIGHT_CUT))
    out = [0.0] * int(THUD_SECONDS * RATE)
    for i in range(len(out)):
        out[i] = stamp_level * (foot[i] if i < len(foot) else 0.0) + stomp_level * (weight[i] if i < len(weight) else 0.0)
    return finish(out, THUD_DRIVE, THUD_PEAK, 0.002, 0.06)


def rip(cloth):
    onset = next(i for i, s in enumerate(cloth) if abs(s) >= RIP_ONSET) / RATE
    body = low_passed(high_passed(cut(cloth, onset, RIP_SECONDS), RIP_HIGH_PASS), RIP_TOP, 1)
    return finish(body, RIP_DRIVE, RIP_PEAK, 0.002, RIP_FADE)


def crack(source, take):
    at, seconds, rate, body_level, drive = CRACK_TAKES[take]
    start = int((at - 0.004) * RATE)
    top = max(abs(s) for s in source[start : start + int(0.03 * RATE)])
    onset = (start + next(i for i, s in enumerate(source[start:]) if abs(s) >= 0.1 * top)) / RATE
    edge = normalised(high_passed(cut(source, onset, seconds, rate), CRACK_HIGH_PASS))
    body = normalised(low_passed(cut(source, onset, seconds, rate / 2), CRACK_BODY_CUT))
    mixed = [e + body_level * b for e, b in zip(edge, body)]
    return finish(low_passed(normalised(mixed), CRACK_TOP, 1), drive, CRACK_PEAK, 0.001, CRACK_FADE)


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__ or "usage: make_recorded_steps.py <folder of fs_<id>.wav> [out_dir]")
    src = next(a for a in sys.argv[1:] if not a.startswith("--"))
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "quakevr", "sound", "vr")
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    out_dir = args[1] if len(args) > 1 else root
    os.makedirs(out_dir, exist_ok=True)
    cracks = [a for a in sys.argv[1:] if a.split("=")[0] == "--crack"]
    if cracks:
        take = int(cracks[0].split("=")[1]) if "=" in cracks[0] else next(iter(CRACK_TAKES))
        write_wav(os.path.join(out_dir, "armor_crack.wav"), crack(read_wav(os.path.join(src, "fs_%d.wav" % take)), take))
        return
    stamps = read_wav(os.path.join(src, "fs_542858.wav"))
    stomps = read_wav(os.path.join(src, "fs_721601.wav"))
    for take in range(len(THUD_TAKES)):
        write_wav(os.path.join(out_dir, "step_thud%d.wav" % (take + 1)), thud(stamps, stomps, take))
    if "--rip" in sys.argv:
        write_wav(os.path.join(src, "fs_pack_rip.wav"), rip(read_wav(os.path.join(src, "fs_591527.wav"))))


if __name__ == "__main__":
    main()
