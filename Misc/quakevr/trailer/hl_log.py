"""Quake VR highlight logs (vr_highlights; docs/vr-port/TRAILER.md): reading them, and Resolve's marker EDL.

A log is <game dir>/highlights/<date>_<time>.csv (a row a moment, written as it happens: it survives a crash) and,
once the log stopped, .json (the same rows sorted, with the session's details). Both have, per moment:

  t          real seconds after the log's sync mark (the white flash and the beep at its start)
  game_time  the game's clock then (slowed in slow motion)
  kind       gib, kill, multikill, parry, counter, bullettime, ... (vr_highlights.cpp's table)
  score      how good it is (kills 1 and up, gibs 3 and up, a multi-kill 2 a kill, ...)
  duration   real seconds (bullet time, a multi-kill, a swing; 0: an instant)
  count      a multi-kill's kills
  map, subject (what it was done to: a classname), detail (with what)
"""

import csv
import json
import os
from fractions import Fraction

# Each kind's marker colour (as the game writes them: vr_highlights.cpp). Resolve's colours: Blue, Cyan, Green,
# Yellow, Red, Pink, Purple, Fuchsia, Rose, Lavender, Sky, Mint, Lemon, Sand, Cocoa, Cream.
COLOURS = {
    "sync": "Cream", "mark": "Fuchsia", "kill": "Blue", "gib": "Red", "corpsegib": "Rose", "multikill": "Rose",
    "parry": "Cyan", "counter": "Green", "shoveparry": "Mint", "bullettime": "Purple", "explosion": "Yellow",
    "barrel": "Sand", "chain": "Lemon", "shotgrenade": "Pink", "axestick": "Cocoa", "swing": "Sky",
    "grapplepull": "Lavender",
}


class Moment:
    __slots__ = ("t", "game_time", "kind", "score", "duration", "count", "map", "subject", "detail")

    def __init__(self, **kw):
        self.t = float(kw.get("t", 0.0))
        self.game_time = float(kw.get("game_time", 0.0))
        self.kind = str(kw.get("kind", ""))
        self.score = float(kw.get("score", 0.0))
        self.duration = float(kw.get("duration", 0.0))
        self.count = int(float(kw.get("count", 1) or 1))
        self.map = str(kw.get("map", ""))
        self.subject = str(kw.get("subject", ""))
        self.detail = str(kw.get("detail", ""))

    def label(self):
        """A short name: the kind, the score, what to and with what."""
        text = "%s %.1f" % (self.kind, self.score)
        if self.subject:
            text += " " + self.subject
        if self.detail:
            text += " - " + self.detail
        return text

    def __repr__(self):
        return "<%.2f %s>" % (self.t, self.label())


def load(path):
    """The moments of a log (.csv or .json; a .csv's .json beside it is not needed), sorted by time."""
    if path.lower().endswith(".json"):
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
        moments = [Moment(**e) for e in data["events"]]
    else:
        with open(path, encoding="utf-8", newline="") as f:
            moments = [Moment(**row) for row in csv.DictReader(f)]
    moments.sort(key=lambda m: m.t)
    return moments


def frames_to_tc(frames, fps):
    """HH:MM:SS:FF, non-drop, for a whole number of frames at an integer frame rate."""
    fps = int(round(fps))
    s, f = divmod(int(frames), fps)
    return "%02d:%02d:%02d:%02d" % (s // 3600, (s // 60) % 60, s % 60, f)


def tc_to_seconds(tc, fps):
    h, m, s, f = (int(x) for x in tc.replace(";", ":").split(":"))
    return h * 3600 + m * 60 + s + Fraction(f, int(round(fps)))


def marker_name(text):
    """Resolve's marker names: letters, digits and a few signs (a '|' would end the field)."""
    keep = set(" _-.+:()x")
    return "".join(c if c.isalnum() and c.isascii() or c in keep else " " for c in text)


def write_edl(path, moments, fps, sync=0.0, tc_start="01:00:00:00", min_score=0.0, title="Quake VR highlights"):
    """Resolve's marker EDL (Timelines > Import > Timeline Markers from EDL): a one-frame event a marker, at the
    timeline's `tc_start` plus `sync` (where the sync mark is in the recording, the recording placed at the timeline's
    start) plus the moment's t; its colour, name and length in frames on the line below. `fps` may be a fraction
    (60000/1001): the frames are counted at it, the timecode at its whole number (non-drop)."""
    nominal = int(round(fps))
    start = int(tc_to_seconds(tc_start, nominal) * nominal)
    lines = ["TITLE: " + marker_name(title), "FCM: NON-DROP FRAME", ""]
    n = 0
    for m in moments:
        if m.score < min_score and m.kind != "sync":
            continue
        at = int(round((sync + m.t) * float(fps)))
        if at < 0:
            continue
        n += 1
        a, b = frames_to_tc(start + at, nominal), frames_to_tc(start + at + 1, nominal)
        lines.append("%03d  001      V     C        %s %s %s %s  " % (n, a, b, a, b))
        lines.append(" |C:ResolveColor%s |M:%s |D:%d" % (COLOURS.get(m.kind, "Blue"), marker_name(m.label()),
                                                         max(1, int(round(m.duration * float(fps))))))
        lines.append("")
    with open(path, "w", encoding="ascii", newline="\r\n") as f:
        f.write("\n".join(lines) + "\n")
    return n


def default_out(path, suffix):
    base, _ = os.path.splitext(path)
    return base + suffix
