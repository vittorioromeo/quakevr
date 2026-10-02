"""The trailer scripts' test, on made-up media (make_test_media.py): the sync mark found, the music's tempo and beats,
the marker EDL's times, and a rough cut's FCPXML valid with its cuts on the beats.

    python selftest.py OUTDIR [--log a_real_highlight_log.csv]

Prints a PASS or FAIL line a check; exits 1 on any failure.
"""

import os
import re
import subprocess
import sys
from fractions import Fraction

import beats
import fcpxml
import hl_log
import sync_detect

HERE = os.path.dirname(os.path.abspath(__file__))
SYNC, BPM, FIRST_BEAT = 3.0, 126.0, 0.5


def run(*args):
    out = subprocess.run([sys.executable] + list(args), cwd=HERE, capture_output=True, text=True)
    if out.returncode:
        print(out.stdout + out.stderr)
    return out.returncode, out.stdout


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    out = sys.argv[1]
    extra = sys.argv[2:]
    failures = 0

    def check(ok, what):
        nonlocal failures
        print("%s  %s" % ("PASS" if ok else "FAIL", what))
        failures += 0 if ok else 1

    rc, _ = run("make_test_media.py", out, "--sync", str(SYNC), "--bpm", str(BPM), *extra)
    check(rc == 0, "test media made")
    rec, music, log = (os.path.join(out, n) for n in ("recording.mp4", "music.wav", "highlights.csv"))

    sync = sync_detect.detect(rec, verbose=False)
    check(sync is not None and abs(sync - SYNC) < 0.5 / 60, "sync mark found at %s s (made at %.3f)" % (sync, SYNC))
    beeps = sync_detect.find_beeps(rec)
    check(len(beeps) == 1 and abs(beeps[0] - SYNC - 0.04) < 0.015, "beep found at %s (made at %.3f)" % (beeps, SYNC + 0.04))

    bpm, times, bars = beats.analyse(music)
    true = [FIRST_BEAT + k * 60.0 / BPM for k in range(400)]
    errs = [min(abs(t - u) for u in true) for t in times]
    check(abs(bpm - BPM) < 0.5, "tempo %.2f BPM (made at %.0f)" % (bpm, BPM))
    check(len(times) > 100 and max(errs) < 0.025 and sum(errs) / len(errs) < 0.012,
          "%d beats, mean error %.1f ms, worst %.1f ms" % (len(times), 1000 * sum(errs) / len(errs), 1000 * max(errs)))
    check(len(bars) and abs(bars[0] - FIRST_BEAT) < 0.03, "first bar at %.3f (made at %.3f)" % (bars[0], FIRST_BEAT))

    # The marker EDL: each moment at 01:00:00:00 + sync + t, to the frame.
    rc, _ = run("markers.py", log, "--rec", rec, "--min-score", "0", "--fcpxml", os.path.join(out, "markers.fcpxml"))
    check(rc == 0, "markers.py: EDL and FCPXML written (the FCPXML checked valid)")
    edl = open(hl_log.default_out(log, ".edl"), encoding="ascii").read()
    tcs = re.findall(r"^\d{3}  001      V     C        (\S+) (\S+) (\S+) (\S+)  $", edl, re.M)
    moments = hl_log.load(log)
    want = [hl_log.frames_to_tc(216000 + round((SYNC + m.t) * 60), 60) for m in moments]
    check([t[0] for t in tcs] == want, "EDL: %d markers at their frames (%s ...)" % (len(tcs), " ".join(want[:3])))
    check(all(re.match(r"^ \|C:ResolveColor\w+ \|M:[^|]+ \|D:\d+$", l) for l in edl.splitlines() if l.startswith(" |")),
          "EDL: every marker line is Resolve's |C: |M: |D: form")

    # The rough cut: valid; its cuts on every second beat (the music from its start).
    cut = os.path.join(out, "trailer.fcpxml")
    rc, text = run("rough_cut.py", "--rec", rec, "--log", log, "--music", music, "--length", "20", "--out", cut)
    print("".join("      " + l + "\n" for l in text.splitlines()), end="")
    check(rc == 0 and not fcpxml.check(cut, verbose=False), "rough cut written and valid")
    import xml.etree.ElementTree as ET
    spine = ET.parse(cut).getroot().find("./library/event/project/sequence/spine")
    ends = [fcpxml.parse_time(c.get("offset")) + fcpxml.parse_time(c.get("duration")) for c in spine]
    grid = [Fraction(round((t - 0.0) * 60), 60) for t in times[::2]]
    off = [min(abs(e - g) for g in grid) for e in ends]
    check(len(ends) >= 3 and max(off) <= Fraction(1, 60), "%d cuts, each on a beat (worst %.1f frames off)"
          % (len(ends), float(max(off)) * 60))
    music_clip = spine[0].find("asset-clip")
    check(music_clip is not None and music_clip.get("lane") == "-1", "the music under the clips (lane -1)")

    print("selftest: %s" % ("all passed" if not failures else "%d FAILED" % failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
