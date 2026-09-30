#!/usr/bin/env python3
# make_physics_sounds.py -- the props' physics sounds (vr_physsound.cpp; docs/vr-port/ROUND21.md, "Physics sounds"):
# impacts by material and weight, scrape grains for props sliding with friction, and the climbing hands' grab on a hold,
# into quakevr/sound/vr/phys/ (22050 Hz, mono, 16-bit PCM). All sources are CC0 (public domain; credited in
# docs/vr-port/CREDITS.md, "Physics sounds"):
#
#   Kenney, "Impact Sounds" 1.0 (130 sounds, OGG):  https://kenney.nl/assets/impact-sounds
#       download: https://kenney.nl/media/pages/assets/impact-sounds/87b4ddecda-1677589768/kenney_impact-sounds.zip
#   Freesound high-quality previews (MP3; the sound's page links it, no login needed for the preview):
#       481861  "R18-33-Drag Wood Box on Wood Floor.wav", craigsmith     https://freesound.org/s/481861/
#       690010  "Wooden Crate dragging on Concrete.mp3", matth3wc04     https://freesound.org/s/690010/
#       844755  "steel plate dragging on wooden floor B", MeanRaccoon   https://freesound.org/s/844755/
#       545562  "kettlebell concrete drag metal rock stone earth.wav", rsellick  https://freesound.org/s/545562/
#
# The sources are not in the repository: put the Kenney zip's Audio folder and the previews (named fs_<id>.mp3) into one
# folder and run
#   python Misc/quakevr/make_physics_sounds.py <that folder> [out folder, default quakevr/sound/vr/phys]
# The OGG and MP3 files are decoded with the engine's own codec DLLs (Windows/codecs/x64: libvorbisfile, libmpg123)
# through ctypes: nothing to install.
#
# What is made:
#   <mat>_<w><n>.wav   impacts: mat wood, metal, soft (backpacks), flesh (gibs, heads); w l(ight), m(edium), h(eavy);
#                      n 1..4 (Kenney's impact<Kind>_<weight>_000..003). Each is high-passed at 40 Hz, resampled, its
#                      silence before the hit trimmed (3 ms kept), its tail cut where it falls 50 dB under the peak and
#                      faded, and brought to a loudness by weight (the loudest 50 ms: light -17, medium -14, heavy
#                      -11 dBFS RMS), peaks limited to -1 dBFS. The engine scales them by impact speed and mass.
#                      (Stone and brick props keep make_sounds.py's rock1..3 and brick1..3.)
#   scrape_<mat><n>.wav  scrape grains, mat wood, metal, stone, soft; n 1..4: 0.5 s of steady sliding each (the windows
#                      of a recording whose 20 ms loudness varies least, among its louder parts), high-passed at 80 Hz,
#                      at one loudness (-20 dBFS RMS), with 80 ms equal-power fades: the engine plays them back to back on
#                      two channels, each starting as the last fades out, so they crossfade into one scrape whose
#                      loudness follows the slide (vr_physsound.cpp). soft: the wood grains low-passed at 700 Hz and
#                      quieter (a sack or a body dragged).
#   grab_<mat><n>.wav  a climbing hand taking a hold, mat wood, metal, stone; n 1..3: a palm's slap (Kenney's
#                      impactSoft_medium) over a quiet tap of the hold's material (wood: impactWood_light; metal:
#                      impactMetal_light low-passed at 1.8 kHz, muted; stone: footstep_concrete), 0.25 s at most.

import ctypes
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import genguard  # noqa: E402

RATE = 22050
CODECS = os.path.join(ROOT, "Windows", "codecs", "x64")


# ---- decoding (the engine's codec DLLs)

def _dlls():
    os.add_dll_directory(CODECS)
    vf = ctypes.CDLL(os.path.join(CODECS, "libvorbisfile-3.dll"))
    mp = ctypes.CDLL(os.path.join(CODECS, "libmpg123-0.dll"))

    class VorbisInfo(ctypes.Structure):
        _fields_ = [("version", ctypes.c_int), ("channels", ctypes.c_int), ("rate", ctypes.c_long)]

    vf.ov_info.restype = ctypes.POINTER(VorbisInfo)
    mp.mpg123_new.restype = ctypes.c_void_p
    return vf, mp


VF, MP = None, None


def _mono(raw, channels):
    a = np.frombuffer(bytes(raw), dtype="<i2").astype(np.float64) / 32768.0
    return a.reshape(-1, channels).mean(axis=1)


def decode(path):
    """Mono samples (-1..1) and their rate."""
    global VF, MP
    if VF is None:
        VF, MP = _dlls()
    buf = ctypes.create_string_buffer(65536)
    out = bytearray()
    if path.endswith(".ogg"):
        f = ctypes.create_string_buffer(8192)  # (an OggVorbis_File: under 1 KB)
        assert VF.ov_fopen(path.encode(), f) == 0, path
        info = VF.ov_info(f, -1).contents
        channels, rate = info.channels, info.rate
        stream = ctypes.c_int()
        while True:
            n = VF.ov_read(f, buf, 65536, 0, 2, 1, ctypes.byref(stream))
            if n <= 0:
                break
            out += buf.raw[:n]
        VF.ov_clear(f)
        return _mono(out, channels), rate
    MP.mpg123_init()
    err = ctypes.c_int()
    h = ctypes.c_void_p(MP.mpg123_new(None, ctypes.byref(err)))
    assert MP.mpg123_open(h, path.encode()) == 0, path
    rate, channels, enc = ctypes.c_long(), ctypes.c_int(), ctypes.c_int()
    MP.mpg123_getformat(h, ctypes.byref(rate), ctypes.byref(channels), ctypes.byref(enc))
    assert enc.value == 0xD0, "not signed 16-bit: %s" % path  # MPG123_ENC_SIGNED_16
    done = ctypes.c_size_t()
    while True:
        r = MP.mpg123_read(h, buf, 65536, ctypes.byref(done))
        out += buf.raw[:done.value]
        if r != 0 and done.value == 0:
            break
    MP.mpg123_close(h)
    MP.mpg123_delete(h)
    return _mono(out, channels.value), rate.value


# ---- processing

def highpass(x, rate, cutoff):
    k = np.exp(-2 * np.pi * cutoff / rate)
    out = np.empty_like(x)
    pi = po = 0.0
    for i, v in enumerate(x):
        po = k * (po + v - pi)
        pi = v
        out[i] = po
    return out


def lowpass(x, rate, cutoff, passes=2):
    a = 1.0 - np.exp(-2 * np.pi * cutoff / rate)
    for _ in range(passes):
        y = np.empty_like(x)
        s = 0.0
        for i, v in enumerate(x):
            s += a * (v - s)
            y[i] = s
        x = y
    return x


def resample(x, rate):
    """To RATE by FFT (band-limited: what is over RATE/2 goes), the ends padded so they don't wrap."""
    if rate == RATE:
        return x.copy()
    pad = int(0.05 * rate)
    y = np.concatenate([np.zeros(pad), x, np.zeros(pad)])
    n_out = int(round(len(y) * RATE / rate))
    spec = np.fft.rfft(y)
    keep = n_out // 2 + 1
    spec = spec[:keep] if len(spec) >= keep else np.concatenate([spec, np.zeros(keep - len(spec))])
    out = np.fft.irfft(spec, n_out) * (n_out / len(y))
    p = int(round(pad * RATE / rate))
    return out[p:len(out) - p]


def rms_db(x):
    return 20 * np.log10(max(1e-9, float(np.sqrt(np.mean(x * x)))))


def loudest_db(x, window=0.05):
    w = max(1, int(window * RATE))
    if len(x) <= w:
        return rms_db(x)
    c = np.concatenate([[0.0], np.cumsum(x * x)])
    m = (c[w:] - c[:-w]) / w
    return 10 * np.log10(max(1e-18, float(m.max())))


def limit(x, ceiling=0.89):
    """Peaks over `ceiling` bent under it by a soft knee (tanh above 0.7 of it)."""
    knee = 0.7 * ceiling
    y = x.copy()
    over = np.abs(y) > knee
    y[over] = np.sign(y[over]) * (knee + (ceiling - knee) * np.tanh((np.abs(y[over]) - knee) / (ceiling - knee)))
    return y


def trimmed_hit(x):
    """The silence before the hit cut (3 ms kept), the tail cut where it stays 50 dB under the peak, faded 20 ms."""
    peak = np.abs(x).max()
    start = int(np.argmax(np.abs(x) > peak * 10 ** (-40 / 20)))
    start = max(0, start - int(0.003 * RATE))
    x = x[start:]
    loud = np.nonzero(np.abs(x) > peak * 10 ** (-50 / 20))[0]
    end = min(len(x), int(loud[-1]) + int(0.01 * RATE) if len(loud) else len(x))
    x = x[:end].copy()
    f = min(len(x) // 3, int(0.02 * RATE))
    x[-f:] *= np.linspace(1, 0, f) ** 2
    x[:16] *= np.linspace(0, 1, 16)
    return x


def at_loudness(x, target_db, loudest=True):
    now = loudest_db(x) if loudest else rms_db(x)
    return limit(x * 10 ** ((target_db - now) / 20))


def write_wav(path, x):
    data = (np.clip(x, -1, 1) * 32767).round().astype("<i2").tobytes()
    with open(path, "wb") as f:
        f.write(b"RIFF" + (36 + len(data)).to_bytes(4, "little") + b"WAVE")
        f.write(b"fmt " + (16).to_bytes(4, "little") + (1).to_bytes(2, "little") + (1).to_bytes(2, "little") +
                RATE.to_bytes(4, "little") + (RATE * 2).to_bytes(4, "little") + (2).to_bytes(2, "little") +
                (16).to_bytes(2, "little"))
        f.write(b"data" + len(data).to_bytes(4, "little") + data)


def load(path):
    x, rate = decode(path)
    return resample(highpass(x, rate, 40.0), rate)


# ---- the sounds

IMPACTS = {  # out name: Kenney's name
    "wood_l": "impactWood_light", "wood_m": "impactWood_medium", "wood_h": "impactWood_heavy",
    "metal_l": "impactMetal_light", "metal_m": "impactMetal_medium", "metal_h": "impactMetal_heavy",
    "soft_m": "impactSoft_medium", "soft_h": "impactSoft_heavy",
    "flesh_m": "impactPunch_medium", "flesh_h": "impactPunch_heavy",
}
WEIGHT_DB = {"l": -17.0, "m": -14.0, "h": -11.0}

# Scrape grains: (material, [(Freesound id, grains taken from it)]).
SCRAPES = [
    ("wood", [(481861, 3), (690010, 1)]),
    ("metal", [(844755, 4)]),
    ("stone", [(545562, 4)]),
]
GRAIN = 0.5    # s
FADE = 0.08    # s: the grains' crossfade (vr_physsound.cpp plays the next this long before one ends)
SCRAPE_DB = -20.0


def steady_windows(x, count):
    """The `count` windows of GRAIN s (not overlapping) whose 20 ms loudness varies least, among the louder ones."""
    f = int(0.02 * RATE)
    frames = len(x) // f
    db = np.array([rms_db(x[i * f:(i + 1) * f]) for i in range(frames)])
    per = int(GRAIN / 0.02)
    floor = np.percentile(db, 75) - 4.0
    cands = []
    for s in range(0, frames - per, 2):
        w = db[s:s + per]
        if w.mean() >= floor and w.min() > w.mean() - 12:
            cands.append((float(w.std()), s))
    cands.sort()
    chosen = []
    for score, s in cands:
        if all(abs(s - c) >= per for c, _ in chosen):
            chosen.append((s, score))
        if len(chosen) == count:
            break
    assert len(chosen) == count, "not enough steady windows"
    return [(s * f, score) for s, score in sorted(chosen)]


def grain(x):
    g = highpass(x, RATE, 80.0)
    g = g * 10 ** ((SCRAPE_DB - rms_db(g)) / 20)
    n = int(FADE * RATE)
    t = np.linspace(0, 1, n)
    g[:n] *= np.sin(t * np.pi / 2)
    g[-n:] *= np.cos(t * np.pi / 2)
    return limit(g)


def main():
    if len(sys.argv) < 2:
        print(__doc__ or "usage: make_physics_sounds.py <sources folder> [out folder]")
        sys.exit(2)
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "quakevr", "sound", "vr", "phys")
    audio = next((d for d, _, files in os.walk(src) if "impactWood_light_000.ogg" in files), src)  # (the zip's Audio)

    names = [f"{k}{n}" for k in IMPACTS for n in range(1, 5)]
    names += [f"scrape_{m}{n}" for m in ("wood", "metal", "stone", "soft") for n in range(1, 5)]
    names += [f"grab_{m}{n}" for m in ("wood", "metal", "stone") for n in range(1, 4)]
    os.makedirs(out, exist_ok=True)
    guard = genguard.Guard("make_physics_sounds.py", [os.path.join(out, n + ".wav") for n in names])

    def save(name, x):
        write_wav(os.path.join(out, name + ".wav"), x)
        print("%-16s %.3f s, loudest %.1f dBFS, peak %.2f" % (name, len(x) / RATE, loudest_db(x), np.abs(x).max()))

    for key, kenney in IMPACTS.items():
        for n in range(1, 5):
            x = trimmed_hit(load(os.path.join(audio, "%s_%03d.ogg" % (kenney, n - 1))))
            save(f"{key}{n}", at_loudness(x, WEIGHT_DB[key[-1]]))

    wood_grains = []
    for mat, sources in SCRAPES:
        n = 1
        for sid, count in sources:
            x = load(os.path.join(src, "fs_%d.mp3" % sid))
            for start, score in steady_windows(x, count):
                g = grain(x[start:start + int(GRAIN * RATE)])
                print("  %s from %d at %.2f s (loudness varies %.1f dB)" % (mat, sid, start / RATE, score))
                save(f"scrape_{mat}{n}", g)
                if mat == "wood":
                    wood_grains.append(g)
                n += 1
    for n, g in enumerate(wood_grains, 1):
        s = lowpass(g, RATE, 700.0)
        save(f"scrape_soft{n}", limit(s * 10 ** ((SCRAPE_DB - 6.0 - rms_db(s)) / 20)))

    taps = {"wood": ("impactWood_light", 0.45, None), "metal": ("impactMetal_light", 0.3, 1800.0),
            "stone": ("footstep_concrete", 0.5, None)}
    for mat, (kenney, gain, cut) in taps.items():
        for n in range(1, 4):
            slap = trimmed_hit(load(os.path.join(audio, "impactSoft_medium_%03d.ogg" % (n + 1))))
            tap = trimmed_hit(load(os.path.join(audio, "%s_%03d.ogg" % (kenney, n + 1))))
            if cut:
                tap = lowpass(tap, RATE, cut)
            slap = slap / (np.abs(slap).max() or 1.0)
            tap = tap / (np.abs(tap).max() or 1.0) * gain
            m = np.zeros(min(int(0.25 * RATE), max(len(slap), len(tap) + int(0.004 * RATE))))
            m[:min(len(m), len(slap))] += slap[:len(m)] * 0.8
            d = int(0.004 * RATE)  # the fingers' tap a moment after the palm
            m[d:d + min(len(tap), len(m) - d)] += tap[:len(m) - d]
            f = int(0.03 * RATE)
            m[-f:] *= np.linspace(1, 0, f) ** 2
            save(f"grab_{mat}{n}", at_loudness(m, -16.0))

    guard.finish()


if __name__ == "__main__":
    main()
