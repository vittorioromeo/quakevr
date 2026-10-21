#!/usr/bin/env python3
# make_gore_sounds.py -- the gore's sounds (vr_goresound.cpp; docs/vr-port/ROUND22.md, "Gore sounds"), cut from CC0
# recordings (public domain; credited in docs/vr-port/CREDITS.md, "Gore sounds") into quakevr/sound/vr/gore/
# (22050 Hz, mono, 16-bit PCM). Each source's page was checked to say Creative Commons 0 before it was used.
#
#   Freesound high-quality previews (MP3; the sound's page links it, no login needed for the preview), named fs_<id>.mp3:
#       654101  "Blood Gush", FlaxGod                            https://freesound.org/s/654101/
#       563315  "Gushing Flesh", magnuswaker                     https://freesound.org/s/563315/
#       800865  "Blood squirting", TyrantTim                     https://freesound.org/s/800865/
#       518474  "wringing wet cloth onto paving", phonoflora     https://freesound.org/s/518474/
#       482836  "R08-56-Man Choking on Water", craigsmith        https://freesound.org/s/482836/
#       545723  "Gargles", Bronxio                               https://freesound.org/s/545723/
#       826999  "Gurgling-Choking", JadeW40                      https://freesound.org/s/826999/
#       411683  "Cabbage Rip", neilsher                          https://freesound.org/s/411683/
#       814734  "Gore Tear 1", MalikArsYasakani                  https://freesound.org/s/814734/
#       697398  "Watermelon Ripping", CJspellsfish               https://freesound.org/s/697398/
#       574752  "Bone Breaks (celery snaps)", ericnorcross81     https://freesound.org/s/574752/
#       762893  "Bones Breaking", FelixL-H                       https://freesound.org/s/762893/
#       684877  "Tomato Squish", ominouswhoosh                   https://freesound.org/s/684877/
#       840618  "bloody_jab_squelch", MrGungus                   https://freesound.org/s/840618/
#       531571  "squishy footsteps to walking in a puddle", patchytherat  https://freesound.org/s/531571/
#       540077  "Meat Slap 1", magnuswaker                       https://freesound.org/s/540077/
#       223967  "d0 Meatslap on Glass 1", dav0r                  https://freesound.org/s/223967/
#   OpenGameArt, "8 wet squish, slurp impacts", Independent.nu (Ljudbank), FLAC in a .7z:
#       https://opengameart.org/content/8-wet-squish-slurp-impacts
#       download: https://opengameart.org/sites/default/files/independent_nu_ljudbank-wet_squish_slurp_impacts.7z
#
# The sources are not in the repository: put the previews and the pack's impsplat/impactsplat0N.mp3.flac into one folder
# (subfolders are searched) and run
#   python Misc/quakevr/make_gore_sounds.py <that folder> [out folder, default quakevr/sound/vr/gore]
# MP3 and FLAC are decoded with the engine's own codec DLLs (Windows/codecs/x64: libmpg123, libFLAC) through ctypes.
#
# Every cut (seconds in its source) is in CUTS below. A cut is high-passed (40 Hz; pour 80 Hz), resampled, its silence
# before the onset trimmed (3 ms kept), faded in 3 ms and out over its last FADE_OUT (squared), and brought to its
# kind's loudness (the loudest 50 ms, dBFS RMS, on make_physics_sounds.py's scale: its flesh impacts are -14 and -11),
# peaks bent under -1 dBFS by a soft knee. Layered sounds (tear: a rip, a wet layer, a crack; choke: a gargle, a weak
# cough) are each peak-normalised, mixed at their gains and offsets, then levelled as one. The chokes are pitched down
# (played slower: a monster's throat, not a man's). The pour grains are 0.6 s at one RMS loudness with 80 ms
# equal-power fades (vr_goresound.cpp plays them back to back, crossing by 80 ms), the windows of the stream whose
# 20 ms loudness varies least.
#
# The synthetic placeholders these replace: make_gore_placeholders.py (kept as a fallback, nothing ships them).

import ctypes
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import genguard  # noqa: E402
import make_physics_sounds as phys  # noqa: E402  (MP3 decoding, filters, resampling, loudness, the WAV writer)

RATE = phys.RATE

# Loudest 50 ms (dBFS) by kind; pour: the grains' RMS.
LEVEL_DB = {"spurt": -13.0, "choke": -14.0, "tear": -12.0, "squelch": -14.0, "step": -16.0, "slap": -13.0,
            "splash": -14.0}
POUR_RMS_DB = -18.0
FADE_OUT = {"spurt": 0.08, "choke": 0.08, "tear": 0.08, "squelch": 0.05, "step": 0.08, "slap": 0.05, "splash": 0.07}

# out name: layers, each (source file, start s, end s, gain, offset s in the mix); then options:
#   lp: a low-pass (Hz, one pole, once) on the mix (the sprays darkened into a low gush; the steps' hiss),
#   pitch: played at this speed (under 1: lower and longer).
CUTS = {
    # a pulse of blood: the gush and its bubbly tail
    "spurt1": ([("fs_654101.mp3", 3.22, 3.62, 1.0, 0.0)], {"lp": 2500.0}),   # a wrung rag's gush
    "spurt2": ([("fs_563315.mp3", 0.00, 0.36, 1.0, 0.0)], {}),               # gushing flesh (already low)
    "spurt3": ([("fs_800865.mp3", 1.78, 2.16, 1.0, 0.0)], {"lp": 2500.0}),   # a squirt
    # a choke: a wet gargle, then a weak cough
    "choke1": ([("fs_482836.mp3", 0.22, 0.98, 1.0, 0.0),                     # a man choking on water: the gargle
                ("fs_482836.mp3", 11.53, 11.76, 0.35, 0.73)],                 # his splutter, as the cough
               {"pitch": 0.85}),
    "choke2": ([("fs_545723.mp3", 13.00, 13.75, 1.0, 0.0),                   # a low gargle
                ("fs_826999.mp3", 4.58, 4.78, 0.4, 0.72)],                   # a choking hack, as the cough
               {"pitch": 0.9}),
    # a limb torn off: a wet rip, then a bone's crack 0.15-0.2 s in
    "tear1": ([("fs_411683.mp3", 1.20, 1.62, 0.8, 0.0),                      # cabbage ripping
               ("fs_814734.mp3", 3.90, 4.32, 0.5, 0.0),                      # a wet gore layer
               ("fs_574752.mp3", 0.36, 0.56, 1.0, 0.18)], {}),               # a celery snap
    "tear2": ([("fs_697398.mp3", 4.84, 5.30, 1.0, 0.0),                      # a watermelon ripping
               ("fs_411683.mp3", 2.84, 3.20, 0.5, 0.02),                     # cabbage ripping, under it
               ("fs_762893.mp3", 0.14, 0.42, 1.0, 0.20)], {}),               # bones breaking
    # a hand closing on wet flesh
    "squelch1": ([("fs_684877.mp3", 5.84, 6.10, 1.0, 0.0)], {}),             # a tomato squished
    "squelch2": ([("fs_840618.mp3", 0.00, 0.28, 1.0, 0.0)], {}),             # a bloody jab's squelch
    "squelch3": ([("impactsplat04.mp3.flac", 0.12, 0.40, 1.0, 0.0)], {}),    # a wet squish (OpenGameArt)
    # a quiet wet step
    "step1": ([("fs_531571.mp3", 3.02, 3.28, 1.0, 0.0)], {"lp": 3000.0}),
    "step2": ([("fs_531571.mp3", 3.75, 4.01, 1.0, 0.0)], {"lp": 3000.0}),
    "step3": ([("fs_531571.mp3", 6.72, 6.98, 1.0, 0.0)], {"lp": 3000.0}),
    # a wet lump hitting a wall
    "slap1": ([("fs_540077.mp3", 0.10, 0.34, 1.0, 0.0)], {}),                # a meat slap
    "slap2": ([("fs_223967.mp3", 0.05, 0.29, 1.0, 0.0)], {}),                # meat slapped on glass
    "slap3": ([("impactsplat03.mp3.flac", 0.09, 0.31, 1.0, 0.0)], {}),       # a wet splat (OpenGameArt)
    # a foot stepping into a pool of blood: the puddle's heavier steps, a small wet splat under two of them
    "splash1": ([("fs_531571.mp3", 7.10, 7.40, 1.0, 0.0)], {"lp": 4000.0}),
    "splash2": ([("fs_531571.mp3", 6.70, 7.04, 1.0, 0.0),
                 ("impactsplat08.mp3.flac", 0.06, 0.32, 0.45, 0.0)], {"lp": 4000.0}),
    "splash3": ([("fs_531571.mp3", 2.28, 2.56, 1.0, 0.0),
                 ("impactsplat05.mp3.flac", 0.04, 0.32, 0.4, 0.0)], {"lp": 4000.0}),
}
# The blood stream's grains: its steadiest 0.6 s windows (of 1.9..4.45 s: the stream on the paving, before the drips).
POUR = {"pour1": ("fs_518474.mp3", 2.76), "pour2": ("fs_518474.mp3", 3.46)}
GRAIN = 0.6
GRAIN_FADE = 0.08


# ---- FLAC (the OpenGameArt pack), with the engine's libFLAC through ctypes

class _FrameHeader(ctypes.Structure):
    _fields_ = [("blocksize", ctypes.c_uint32), ("sample_rate", ctypes.c_uint32), ("channels", ctypes.c_uint32),
                ("channel_assignment", ctypes.c_int), ("bits_per_sample", ctypes.c_uint32)]


_WRITE = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.POINTER(_FrameHeader),
                          ctypes.POINTER(ctypes.POINTER(ctypes.c_int32)), ctypes.c_void_p)
_ERROR = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p)


def decode_flac(path):
    """Mono samples (-1..1) and their rate."""
    os.add_dll_directory(phys.CODECS)
    fl = ctypes.CDLL(os.path.join(phys.CODECS, "libFLAC-8.dll"))
    fl.FLAC__stream_decoder_new.restype = ctypes.c_void_p
    blocks, info = [], {}

    def on_write(_dec, hdr, buf, _client):
        h = hdr.contents
        info["rate"], info["bits"] = h.sample_rate, h.bits_per_sample
        chans = [np.ctypeslib.as_array(buf[c], shape=(h.blocksize,)).astype(np.float64) for c in range(h.channels)]
        blocks.append(np.mean(chans, axis=0))
        return 0  # FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE

    def on_error(_dec, status, _client):
        raise RuntimeError("FLAC error %d: %s" % (status, path))

    write_cb, error_cb = _WRITE(on_write), _ERROR(on_error)
    dec = ctypes.c_void_p(fl.FLAC__stream_decoder_new())
    assert fl.FLAC__stream_decoder_init_file(dec, path.encode(), write_cb, None, error_cb, None) == 0, path
    assert fl.FLAC__stream_decoder_process_until_end_of_stream(dec), path
    fl.FLAC__stream_decoder_finish(dec)
    fl.FLAC__stream_decoder_delete(dec)
    return np.concatenate(blocks) / float(1 << (info["bits"] - 1)), info["rate"]


def load(path, highpass=40.0):
    """A whole source, mono at RATE, high-passed (rumble and DC out). (scratch analysis uses it too)"""
    x, rate = decode_flac(path) if path.endswith(".flac") else phys.decode(path)
    return phys.resample(phys.highpass(x, rate, highpass), rate)


# ---- cutting

DECODED = {}


def find(src, name):
    for d, _, files in os.walk(src):
        if name in files:
            return os.path.join(d, name)
    sys.exit("missing source: %s (in %s)" % (name, src))


def cut(src, name, a, b, highpass=40.0):
    """name's a..b s, mono at RATE: decoded once, cut with 0.1 s around it, high-passed, resampled, cut exactly."""
    if name not in DECODED:
        path = find(src, name)
        DECODED[name] = decode_flac(path) if path.endswith(".flac") else phys.decode(path)
    x, rate = DECODED[name]
    m = 0.1
    s0 = max(0, int((a - m) * rate))
    y = phys.resample(phys.highpass(x[s0:int((b + m) * rate)], rate, highpass), rate)
    o = int(round((a - s0 / rate) * RATE))
    return y[o:o + int(round((b - a) * RATE))]


def onset_trimmed(x):
    """The silence (and hiss) before the onset (24 dB under the peak) cut, 3 ms kept."""
    peak = np.abs(x).max()
    start = int(np.argmax(np.abs(x) > peak * 10 ** (-24 / 20)))
    return x[max(0, start - int(0.003 * RATE)):]


def faded(x, out_s):
    x = x.copy()
    n = int(0.003 * RATE)
    x[:n] *= np.linspace(0, 1, n)
    f = min(len(x) // 3, int(out_s * RATE))
    x[-f:] *= np.linspace(1, 0, f) ** 2
    return x


def pitched(x, speed):
    """Played at `speed` (resampled as if recorded at RATE * speed): lower and longer under 1."""
    return phys.resample(x, RATE * speed)


def one_shot(src, kind, layers, opts):
    mix = np.zeros(int(2.0 * RATE))
    end = 0
    for i, (name, a, b, gain, offset) in enumerate(layers):
        y = cut(src, name, a, b)
        if i > 0 or len(layers) == 1:
            y = onset_trimmed(y)
        y = faded(y, 0.03) if len(layers) > 1 else y
        y = y / (np.abs(y).max() or 1.0) * gain
        o = int(offset * RATE)
        mix[o:o + len(y)] += y
        end = max(end, o + len(y))
    x = onset_trimmed(mix[:end])
    if "lp" in opts:
        x = phys.lowpass(x, RATE, opts["lp"], passes=1)
    if "pitch" in opts:
        x = pitched(x, opts["pitch"])
    return phys.at_loudness(faded(x, FADE_OUT[kind]), LEVEL_DB[kind])


def pour_grain(src, name, start):
    g = cut(src, name, start, start + GRAIN, highpass=80.0)
    g = g * 10 ** ((POUR_RMS_DB - phys.rms_db(g)) / 20)
    n = int(GRAIN_FADE * RATE)
    t = np.linspace(0, 1, n)
    g[:n] *= np.sin(t * np.pi / 2)
    g[-n:] *= np.cos(t * np.pi / 2)
    return phys.limit(g)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__ or "usage: make_gore_sounds.py <sources folder> [out folder]")
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "quakevr", "sound", "vr", "gore")
    os.makedirs(out, exist_ok=True)
    names = list(CUTS) + list(POUR)
    guard = genguard.Guard("make_gore_sounds.py", [os.path.join(out, n + ".wav") for n in names])

    def save(name, x):
        phys.write_wav(os.path.join(out, name + ".wav"), x)
        print("%-9s %.3f s, loudest %.1f dBFS, rms %.1f, peak %.2f" % (
            name, len(x) / RATE, phys.loudest_db(x), phys.rms_db(x), np.abs(x).max()))

    for name, (layers, opts) in CUTS.items():
        save(name, one_shot(src, name.rstrip("0123456789"), layers, opts))
    for name, (srcname, start) in POUR.items():
        save(name, pour_grain(src, srcname, start))
    guard.finish()


if __name__ == "__main__":
    main()
