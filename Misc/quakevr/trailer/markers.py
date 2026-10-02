"""A highlight log's moments as DaVinci Resolve markers on a recording (docs/vr-port/TRAILER.md).

    python markers.py LOG.csv --rec recording.mp4 [--sync 12.34] [--min-score 2] [--fcpxml rec.fcpxml]
    python markers.py LOG.csv --sync 12.34 --fps 60

Writes LOG's .edl beside it (or --edl): Resolve's timeline markers (right-click the timeline in the Media Pool >
Timelines > Import > Timeline Markers from EDL...), for the recording placed whole at the timeline's start
(--tc-start, Resolve's 01:00:00:00 by default), its sync mark --sync seconds into it (found in --rec when not given:
sync_detect.py). With --fcpxml also a timeline to import (File > Import > Timeline...): the recording whole, the moments
as markers on it.
"""

import argparse
import sys
from fractions import Fraction

import fcpxml
import hl_log
import media
import sync_detect


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("log", help="the highlight log (.csv or .json)")
    p.add_argument("--rec", help="the recording (for its frame rate, and to find the sync mark)")
    p.add_argument("--sync", type=float, help="the sync mark's time in the recording, seconds")
    p.add_argument("--fps", help="the timeline's frame rate (default: the recording's, else 60)")
    p.add_argument("--tc-start", default="01:00:00:00", help="the timeline's start timecode (Resolve's default)")
    p.add_argument("--min-score", type=float, default=2.0, help="moments scoring less get no marker (default 2)")
    p.add_argument("--edl", help="the EDL to write (default: the log's name, .edl)")
    p.add_argument("--fcpxml", help="also a timeline of the whole recording with the markers on it")
    a = p.parse_args()

    info = media.Info(a.rec) if a.rec else None
    sync = a.sync
    if sync is None:
        if not a.rec:
            p.error("give --sync or --rec")
        sync = sync_detect.detect(a.rec)
        if sync is None:
            p.error("no sync mark found in %s: give --sync" % a.rec)
    fps = Fraction(a.fps) if a.fps else (info.fps if info and info.has_video else Fraction(60))
    moments = hl_log.load(a.log)
    if round(fps) != fps:
        print("note: %.3f fps: the EDL counts %d frames a second (Resolve's non-drop timecode for it)" % (fps, round(fps)))
    edl = a.edl or hl_log.default_out(a.log, ".edl")
    n = hl_log.write_edl(edl, moments, fps, sync, a.tc_start, a.min_score)
    print("%s: %d markers (sync at %.3f s, %s fps)" % (edl, n, sync, fps))

    if a.fcpxml:
        if not info:
            p.error("--fcpxml needs --rec")
        tl = fcpxml.Timeline(fps, info.width, info.height, "Quake VR highlights")
        frames = int(Fraction(info.duration) * fps)
        markers = []
        for m in moments:
            if m.score < a.min_score and m.kind != "sync":
                continue
            at = tl.frames(sync + m.t)
            if 0 <= at < frames:
                markers.append((at, max(1, tl.frames(m.duration)), m.label()))
        tl.clips.append(fcpxml.Clip(tl.asset(a.rec, info), 0, frames, "recording", markers))
        tl.write(a.fcpxml)
        print("%s: %d markers" % (a.fcpxml, len(markers)))
        return 1 if fcpxml.check(a.fcpxml) else 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
