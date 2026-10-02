"""A trailer's rough cut from recordings and their highlight logs (docs/vr-port/TRAILER.md): the best moments, trimmed
around them, ordered to build up, the cuts on the music's beats; written as an FCPXML timeline that DaVinci Resolve
imports (File > Import > Timeline...) over the original files (nothing re-encoded).

    python rough_cut.py --rec take1.mp4 --log take1.csv [--rec take2.mkv --log take2.csv --sync 12.4]
                        [--music track.mp3] [--length 60] [--out trailer.fcpxml]

Each --rec goes with the --log at the same place (and the --sync there: its sync mark's time in the recording,
"auto" by default: found by sync_detect.py, the flash or the beep). Moments are the log's rows (runs of them less
than --merge-gap apart make one, scored by its best plus half the rest's); the best that fit --length are taken, each --pre seconds before its moment and
--post after (at most --max-clip long), ordered by --order, and with --music each cut moved to the nearest beat
(--cut-every beats apart at the least, or bars with --bars).
"""

import argparse
import os
import sys
from fractions import Fraction

import beats as beatlib
import fcpxml
import hl_log
import media
import sync_detect


class Moment:
    """A run of a recording's moments close together: one clip."""

    def __init__(self, rec, events):
        self.rec = rec          # index into the recordings
        self.events = events    # [(video time, hl_log.Moment)]
        best = max(events, key=lambda e: e[1].score)
        self.peak = best[0]
        scores = sorted((m.score for _, m in events), reverse=True)
        self.score = scores[0] + 0.5 * sum(scores[1:])
        self.start = self.end = 0.0

    def kinds(self):
        seen = []
        for _, m in self.events:
            if m.kind not in seen:
                seen.append(m.kind)
        return "+".join(seen)


def find_moments(rec, info, log, sync, a):
    """The recording's moments: its log's rows in it (by score and kind), merged where their windows meet."""
    kinds = set(a.kinds.split(",")) if a.kinds else None
    exclude = set(a.exclude.split(",")) if a.exclude else set()
    rows = []
    for m in log:
        if m.kind == "sync" or m.kind in exclude or (kinds and m.kind not in kinds) or m.score < a.min_score:
            continue
        v = sync + m.t
        if 0.0 <= v <= info.duration:
            rows.append((v, m))
    rows.sort(key=lambda r: r[0])
    groups = []  # [events, first moment's time, last one's end]
    for v, m in rows:
        end = v + min(m.duration, a.max_clip)
        if groups and v <= groups[-1][2] + a.merge_gap:
            groups[-1][0].append((v, m))
            groups[-1][2] = max(groups[-1][2], end)
        else:
            groups.append([[(v, m)], v, end])
    out = []
    for events, first, last in groups:
        lo, hi = first - a.pre, last + a.post
        mo = Moment(rec, events)
        lo, hi = max(0.0, lo), min(info.duration, hi)
        # At most --max-clip, round the best moment (--pre before it).
        start = max(lo, mo.peak - a.pre)
        end = min(hi, start + a.max_clip)
        start = max(0.0, min(start, end - a.min_clip))
        mo.start, mo.end = start, end
        if end - start >= a.min_clip:
            out.append(mo)
    return out


def order(chosen, how):
    if how == "chrono":
        return sorted(chosen, key=lambda m: (m.rec, m.start))
    if how == "score":
        return sorted(chosen, key=lambda m: -m.score)
    up = sorted(chosen, key=lambda m: m.score)
    if how == "hook" and len(up) >= 3:
        # The second best opens, then the rest building up, the best last.
        return [up[-2]] + up[:-2] + [up[-1]]
    return up  # build: weakest first, the best last


def cut_grid(a, fps):
    """The timeline's frames a cut may fall on (from the music's beats), or None without music."""
    if not a.music:
        return None, None
    bpm, times, bars = beatlib.analyse(a.music, a.bpm, a.music_start, a.length * 2 + 30)
    pts = bars if a.bars else times[::max(1, a.cut_every)]
    grid = sorted({int(round((t - a.music_start) * fps)) for t in pts if t - a.music_start > 0})
    print("music: %.2f BPM, %d beats, cuts on every %s (%d points)" % (bpm, len(times),
          "bar" if a.bars else "%d beat%s" % (a.cut_every, "s" if a.cut_every > 1 else ""), len(grid)))
    return grid, bpm


def build(clips, infos, tl, grid, a):
    """Lays the clips end to end on the timeline, each cut moved to the nearest grid point."""
    fps = tl.fps
    tl.clips = []
    cursor = 0
    plan = []
    for mo in clips:
        info = infos[mo.rec]
        last = int(Fraction(info.duration) * fps) - 1
        s, e, p = tl.frames(mo.start), tl.frames(mo.end), tl.frames(mo.peak)
        want = e - s
        if grid:
            lo = cursor + tl.frames(a.min_clip)
            hi = cursor + want + tl.frames(a.max_stretch)
            cands = [g for g in grid if lo <= g <= hi]
            if cands:
                g = min(cands, key=lambda g: abs(g - cursor - want))
                new = g - cursor
                if new < want:
                    # Shorter: taken off both sides as they had (pre before the peak, post after), half a second kept
                    # before the peak.
                    cut = want - new
                    pre, post = p - s, e - p
                    take_pre = min(max(0, pre - tl.frames(0.5)), int(round(cut * pre / max(1, pre + post))))
                    s += take_pre
                    e = s + new
                else:
                    e = s + new
                    if e > last:
                        s -= e - last
                        e = last
                    s = max(0, s)
                    e = s + new
        length = e - s
        markers = [(tl.frames(v), max(1, tl.frames(m.duration)), m.label()) for v, m in mo.events
                   if s <= tl.frames(v) < e]
        name = "%s %.1f" % (mo.kinds(), mo.score)
        tl.clips.append(fcpxml.Clip(tl.asset(a.rec[mo.rec], info), s, length, name, markers))
        plan.append((cursor, mo, s, e))
        cursor += length
    return plan


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--rec", action="append", required=True, help="a recording (repeat for more)")
    p.add_argument("--log", action="append", required=True, help="its highlight log, .csv or .json (as many as --rec)")
    p.add_argument("--sync", action="append", default=[],
                   help="its sync mark's time in the recording, seconds, or auto (default: auto)")
    p.add_argument("--music", help="a music file (any format FFmpeg reads): cuts on its beats, under the clips")
    p.add_argument("--music-start", type=float, default=0.0, help="seconds into the music the trailer starts at")
    p.add_argument("--bpm", type=float, default=None, help="the music's tempo, when known (else found)")
    p.add_argument("--cut-every", type=int, default=2, help="cuts at least this many beats apart (default 2)")
    p.add_argument("--bars", action="store_true", help="cuts on the bars' first beats only (4/4)")
    p.add_argument("--length", type=float, default=60.0, help="the trailer's length, seconds (default 60)")
    p.add_argument("--pre", type=float, default=2.0, help="seconds before a moment (default 2)")
    p.add_argument("--post", type=float, default=1.5, help="seconds after it (or after its end) (default 1.5)")
    p.add_argument("--max-clip", type=float, default=5.0, help="the longest clip, seconds (default 5)")
    p.add_argument("--min-clip", type=float, default=1.0, help="the shortest, seconds (default 1)")
    p.add_argument("--max-stretch", type=float, default=1.5, help="seconds a clip may grow to reach a beat")
    p.add_argument("--merge-gap", type=float, default=1.0,
                   help="moments less than this apart (seconds, after the last one's end) make one clip (default 1)")
    p.add_argument("--min-score", type=float, default=2.0, help="moments scoring less are left out (default 2)")
    p.add_argument("--kinds", help="only these kinds (comma-separated: gib,parry,...)")
    p.add_argument("--exclude", help="not these kinds")
    p.add_argument("--order", choices=["build", "hook", "chrono", "score"], default="hook",
                   help="build: weakest first, best last; hook: the second best first, then build (default); "
                        "chrono: as recorded; score: best first")
    p.add_argument("--fps", help="the timeline's frame rate (default: the first recording's), e.g. 60 or 60000/1001")
    p.add_argument("--version", choices=["1.8", "1.9"], default="1.8", help="FCPXML version (default 1.8)")
    p.add_argument("--name", default="Quake VR trailer")
    p.add_argument("--out", default="trailer.fcpxml")
    a = p.parse_args()
    if len(a.log) != len(a.rec):
        p.error("one --log per --rec")
    a.sync += ["auto"] * (len(a.rec) - len(a.sync))

    infos, moments = [], []
    for i, (rec, logpath, sync) in enumerate(zip(a.rec, a.log, a.sync)):
        info = media.Info(rec)
        infos.append(info)
        if sync == "auto":
            found = sync_detect.detect(rec, verbose=False)
            if found is None:
                p.error("%s: no sync mark found: give --sync" % rec)
            sync = found
        sync = float(sync)
        log = hl_log.load(logpath)
        found = find_moments(i, info, log, sync, a)
        print("%s: %.1f s, sync at %.3f s, %d rows, %d moments" % (os.path.basename(rec), info.duration, sync, len(log),
                                                                    len(found)))
        moments += found
    if not moments:
        print("no moments to cut")
        return 1

    fps = Fraction(a.fps) if a.fps else (infos[0].fps or Fraction(60))
    first = next((i for i in infos if i.has_video), infos[0])
    tl = fcpxml.Timeline(fps, first.width or 1920, first.height or 1080, a.name)
    grid, _ = cut_grid(a, fps)
    if a.music:
        tl.music = (fcpxml.Asset(a.music, media.Info(a.music)), tl.frames(a.music_start), 0)

    # The best that fit the length (snapping moves each cut half a beat or so; the weakest go until it fits).
    chosen = []
    total = 0.0
    for mo in sorted(moments, key=lambda m: -m.score):
        if total + (mo.end - mo.start) <= a.length + a.min_clip:
            chosen.append(mo)
            total += mo.end - mo.start
    while True:
        plan = build(order(chosen, a.order), infos, tl, grid, a)
        if tl.length() <= tl.frames(a.length * 1.05) or len(chosen) <= 1:
            break
        chosen.remove(min(chosen, key=lambda m: m.score))
    if tl.music:
        m, start, _ = tl.music
        room = int(Fraction(m.info.duration) * fps) - start
        if room < tl.length():
            print("note: the music (%.1f s from --music-start) is shorter than the cut (%.1f s): it ends early"
                  % (room / float(fps), tl.length() / float(fps)))
        tl.music = (m, start, min(room, tl.length())) if room > 0 else None

    for cursor, mo, s, e in plan:
        print("%7.2f s  %-26s %5.1f  %s %.2f-%.2f" % (cursor / float(fps), mo.kinds()[:26], mo.score,
                                                    os.path.basename(a.rec[mo.rec]), s / float(fps), e / float(fps)))
    tl.write(a.out, a.version)
    print("%s: %d clips, %.2f s%s" % (a.out, len(tl.clips), tl.length() / float(fps),
                                     ", music " + os.path.basename(a.music) if tl.music else ""))
    return 1 if fcpxml.check(a.out) else 0


if __name__ == "__main__":
    sys.exit(main())
