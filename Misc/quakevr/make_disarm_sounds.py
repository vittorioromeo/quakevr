#!/usr/bin/env python3
# make_disarm_sounds.py -- the player disarmed's sounds (QC vr_playerdisarm.qc VR_PD_Disarm; docs/vr-port/CREDITS.md,
# "Player disarm sounds"), cut from two CC0 packs (not in the repository):
#   StarNinjas, "20 Sword Sound Effects" (OpenGameArt: https://opengameart.org/content/20-sword-sound-effects):
#     sword_clash_-_starninjas_0.zip -> sword_clash.1.ogg
#   Kenney, "Interface Sounds" (https://kenney.nl/assets/interface-sounds): kenney_interfaceSounds.zip ->
#     Audio/error_003.ogg
# Usage: python Misc/quakevr/make_disarm_sounds.py <folder with the packs extracted: clash/ and kenney/>
# Needs ffmpeg on the PATH. Writes quakevr/sound/vr/disarm_clang.wav and disarm_sting.wav: mono, 22050 Hz, 16-bit,
# trimmed under 1 s with a fade-out, peaks at -1 dBFS.
#
# Picked by the numbers (no listening; f6_sfx_an: peak, attack = onset to peak, ring = onset to the last 1% of the peak,
# brightness as the zero-crossing rate): the clang, sword_clash.1: an instant attack (0 ms) and the longest ring of the
# ten clashes (0.8 s, ~6 kHz: bright metal), the weapon struck away; the swords' swings (sword.N) open on a whoosh
# (145-325 ms to the peak). The sting, error_003: an instant attack, 0.3 s long and mid-pitched (~1.4 kHz), clearer than
# the 0.1-0.2 s blips and the low thuds (error_005..008, ~0.1 kHz) of the rest. The Medieval Weapon Impacts pack's
# takes are minute-long multi-hit recordings: not used.
import os, subprocess, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "quakevr", "sound", "vr")

CUTS = [
    # (source under the folder, output, keep (s), fade-out (s))
    ("clash/sword_clash.1.ogg", "disarm_clang.wav", 0.9, 0.3),
    ("kenney/Audio/error_003.ogg", "disarm_sting.wav", 0.5, 0.1),
]


def peak_db(path, keep):
    r = subprocess.run(["ffmpeg", "-hide_banner", "-i", path, "-t", str(keep), "-ac", "1", "-af", "volumedetect", "-f",
                        "null", "-"], capture_output=True, text=True)
    for line in r.stderr.splitlines():
        if "max_volume:" in line:
            return float(line.split("max_volume:")[1].split("dB")[0])
    return 0.0


def main():
    if len(sys.argv) != 2:
        print(__doc__ or "usage: make_disarm_sounds.py <folder>")
        return 1
    src = sys.argv[1]
    for rel, name, keep, fade in CUTS:
        path = os.path.join(src, rel)
        gain = -1.0 - peak_db(path, keep)
        af = "highpass=f=30,afade=t=out:st=%.3f:d=%.3f,volume=%.2fdB" % (keep - fade, fade, gain)
        out = os.path.join(OUT, name)
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", path, "-t", str(keep), "-ac", "1", "-ar", "22050", "-af", af,
                        "-c:a", "pcm_s16le", out], check=True)
        print("wrote %s (%.2f s, gain %+.1f dB)" % (out, keep, gain))
    return 0


if __name__ == "__main__":
    sys.exit(main())
