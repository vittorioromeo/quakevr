"""FCPXML timelines for DaVinci Resolve (File > Import > Timeline...): clips cut from the original recordings (no
re-encode: the timeline refers to the files), each with its moments as markers, and a music track under them.

    python fcpxml.py check timeline.fcpxml     # the structural check (validate)

FCPXML 1.8 by default (the asset's file in its `src`; Resolve 16 and later import it); 1.9 puts the file in a
<media-rep> instead. Times are rationals of seconds ("1001/30000s"), every one a whole number of the timeline's frames.
"""

import os
import pathlib
import sys
from fractions import Fraction
from xml.sax.saxutils import quoteattr


def rational(seconds):
    """FCPXML's time: "0s", "5s" or "n/ds"."""
    f = Fraction(seconds)
    if f == 0:
        return "0s"
    if f.denominator == 1:
        return "%ds" % f.numerator
    return "%d/%ds" % (f.numerator, f.denominator)


def parse_time(text):
    if not text.endswith("s"):
        raise ValueError("not a time: %r" % text)
    body = text[:-1]
    if "/" in body:
        n, d = body.split("/")
        return Fraction(int(n), int(d))
    return Fraction(body)


class Clip:
    def __init__(self, asset, start, frames, name="", markers=()):
        self.asset = asset    # Asset
        self.start = start    # frames into the media (its in-point)
        self.frames = frames  # its length
        self.name = name
        self.markers = list(markers)  # (frame into the media, length in frames, text)


class Asset:
    def __init__(self, path, info):
        self.path = os.path.abspath(path)
        self.info = info  # media.Info
        self.id = None


class Timeline:
    """A sequence of clips at `fps` (a Fraction: 60, 30000/1001, ...) and a music file under them."""

    def __init__(self, fps, width, height, name="Quake VR trailer"):
        self.fps = Fraction(fps)
        self.frame = 1 / self.fps
        self.width, self.height = width, height
        self.name = name
        self.assets = []
        self.clips = []
        self.music = None  # (Asset, frames into the music to start at, frames of it: from the timeline's start)

    def frames(self, seconds):
        return int(round(Fraction(seconds) * self.fps))

    def time(self, frames):
        return rational(frames * self.frame)

    def asset(self, path, info):
        for a in self.assets:
            if a.path == os.path.abspath(path):
                return a
        a = Asset(path, info)
        self.assets.append(a)
        return a

    def length(self):
        return sum(c.frames for c in self.clips)

    def write(self, path, version="1.8"):
        rid = 1
        formats = {}  # (fps, w, h) -> id

        def format_id(fps, w, h):
            nonlocal rid
            key = (Fraction(fps), w, h)
            if key not in formats:
                formats[key] = "r%d" % rid
                rid += 1
            return formats[key]

        seq_format = format_id(self.fps, self.width, self.height)
        lines = ['<?xml version="1.0" encoding="UTF-8"?>', "<!DOCTYPE fcpxml>", '<fcpxml version="%s">' % version,
                 "  <resources>"]
        asset_lines = []
        for a in self.assets + ([self.music[0]] if self.music else []):
            a.id = "r%d" % rid
            rid += 1
        for a in self.assets + ([self.music[0]] if self.music else []):
            i = a.info
            # The asset's length, floored to the timeline's frames (all its clips' times are on that grid).
            dur = self.time(int(Fraction(i.duration) * self.fps))
            attrs = ["id=%s" % quoteattr(a.id), "name=%s" % quoteattr(os.path.basename(a.path)), 'start="0s"',
                     "duration=%s" % quoteattr(dur)]
            if i.has_video:
                attrs += ['hasVideo="1"', "format=%s" % quoteattr(format_id(i.fps, i.width, i.height))]
            if i.has_audio:
                attrs += ['hasAudio="1"', 'audioSources="1"', 'audioChannels="%d"' % max(1, i.audio_channels),
                          'audioRate="%d"' % i.audio_rate]
            url = pathlib.Path(a.path).resolve().as_uri()
            if version == "1.8":
                attrs.insert(2, "src=%s" % quoteattr(url))
                asset_lines.append("    <asset %s/>" % " ".join(attrs))
            else:
                asset_lines.append("    <asset %s>" % " ".join(attrs))
                asset_lines.append('      <media-rep kind="original-media" src=%s/>' % quoteattr(url))
                asset_lines.append("    </asset>")
        for (fps, w, h), fid in sorted(formats.items(), key=lambda kv: int(kv[1][1:])):
            lines.append('    <format id="%s" frameDuration="%s" width="%d" height="%d"/>' % (fid, rational(1 / fps), w, h))
        lines += asset_lines
        lines.append("  </resources>")
        total = self.length()
        lines += ["  <library>", "    <event name=%s>" % quoteattr(self.name),
                  "      <project name=%s>" % quoteattr(self.name),
                  '        <sequence format="%s" duration="%s" tcStart="0s" tcFormat="NDF" audioLayout="stereo" '
                  'audioRate="48k">' % (seq_format, self.time(total)),
                  "          <spine>"]
        offset = 0
        for n, c in enumerate(self.clips):
            attrs = 'ref="%s" offset="%s" name=%s start="%s" duration="%s" tcFormat="NDF"' % (
                c.asset.id, self.time(offset), quoteattr(c.name or os.path.basename(c.asset.path)),
                self.time(c.start), self.time(c.frames))
            lines.append("            <asset-clip %s>" % attrs)
            if n == 0 and self.music:
                m, music_start, music_frames = self.music
                # Connected below the first clip (lane -1), from the timeline's start to its end: its offset is in
                # the first clip's own time (its in-point).
                lines.append('              <asset-clip ref="%s" lane="-1" offset="%s" name=%s start="%s" duration="%s"/>'
                             % (m.id, self.time(c.start), quoteattr(os.path.basename(m.path)),
                                self.time(music_start), self.time(music_frames)))
            for at, length, text in c.markers:
                lines.append('              <marker start="%s" duration="%s" value=%s/>'
                             % (self.time(at), self.time(max(1, length)), quoteattr(text)))
            lines.append("            </asset-clip>")
            offset += c.frames
        lines += ["          </spine>", "        </sequence>", "      </project>", "    </event>", "  </library>",
                  "</fcpxml>"]
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(lines) + "\n")


def check(path, verbose=True):
    """The structural check (no DTD needed): well-formed; the version; every ref a resource; every time a valid
    rational on the sequence's frame grid; the spine's clips end to end; each clip and connected clip within its
    asset; each marker within its clip; the sequence's duration their sum; the media files there. Returns the list of
    problems (empty: valid)."""
    import xml.etree.ElementTree as ET

    problems = []
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError as e:
        return ["not well-formed XML: %s" % e]
    if root.tag != "fcpxml" or root.get("version") not in ("1.8", "1.9", "1.10", "1.11"):
        problems.append("root: %s version %s" % (root.tag, root.get("version")))
    res = root.find("resources")
    ids = {}
    for e in list(res) if res is not None else []:
        if e.get("id") in ids:
            problems.append("duplicate id %s" % e.get("id"))
        ids[e.get("id")] = e
    seq = root.find("./library/event/project/sequence")
    if seq is None:
        return problems + ["no library/event/project/sequence"]
    fmt = ids.get(seq.get("format"))
    if fmt is None or fmt.tag != "format":
        return problems + ["the sequence's format %s is not a format" % seq.get("format")]
    frame = parse_time(fmt.get("frameDuration"))

    def t(e, attr, default="0s"):
        try:
            v = parse_time(e.get(attr, default))
        except Exception as ex:  # noqa: BLE001 (a report, not a crash)
            problems.append("%s %s: %s" % (e.tag, attr, ex))
            return Fraction(0)
        if (v / frame).denominator != 1:
            problems.append("%s %s=%s is not on the %s frame grid" % (e.tag, attr, e.get(attr), fmt.get("frameDuration")))
        return v

    def asset_of(e):
        a = ids.get(e.get("ref"))
        if a is None or a.tag != "asset":
            problems.append("%s ref %s is not an asset" % (e.tag, e.get("ref")))
            return None
        return a

    def within_asset(e, a, start, dur):
        a0, ad = parse_time(a.get("start", "0s")), parse_time(a.get("duration"))
        if start < a0 or start + dur > a0 + ad:
            problems.append("%s %s: %s..%s outside its asset's 0..%s" % (e.tag, e.get("name"), float(start),
                                                                         float(start + dur), float(a0 + ad)))

    for a in [e for e in ids.values() if e.tag == "asset"]:
        rep = a.find("media-rep")
        src = a.get("src") or (rep.get("src") if rep is not None else None)
        if not src:
            problems.append("asset %s has no file" % a.get("id"))
            continue
        local = src[len("file:///"):] if src.startswith("file:///") else src
        from urllib.parse import unquote
        if not os.path.exists(unquote(local)):
            problems.append("asset %s: no file %s" % (a.get("id"), unquote(local)))

    spine = seq.find("spine")
    expect = Fraction(0)
    clips = 0
    markers = 0
    for c in list(spine) if spine is not None else []:
        if c.tag not in ("asset-clip", "gap"):
            problems.append("spine: unexpected %s" % c.tag)
            continue
        off, start, dur = t(c, "offset"), t(c, "start"), t(c, "duration")
        if off != expect:
            problems.append("spine clip %d at %s, after the last's end %s" % (clips + 1, float(off), float(expect)))
        if dur <= 0:
            problems.append("spine clip %d has no length" % (clips + 1))
        expect = off + dur
        clips += 1
        a = asset_of(c) if c.tag == "asset-clip" else None
        if a is not None:
            within_asset(c, a, start, dur)
        for k in c:
            if k.tag == "marker":
                markers += 1
                ms = t(k, "start")
                if not (start <= ms < start + dur):
                    problems.append("marker %r at %s outside its clip %s..%s" % (k.get("value"), float(ms), float(start),
                                                                                 float(start + dur)))
            elif k.tag == "asset-clip":
                ka = asset_of(k)
                if k.get("lane") is None:
                    problems.append("a connected clip without a lane")
                ks, kd, ko = t(k, "start"), t(k, "duration"), t(k, "offset")
                if ka is not None:
                    within_asset(k, ka, ks, kd)
                if ko < start:
                    problems.append("connected clip before its parent's start")
    if t(seq, "duration") != expect:
        problems.append("sequence duration %s, its clips' %s" % (float(t(seq, "duration")), float(expect)))
    if verbose:
        print("%s: FCPXML %s, %d clips, %d markers, %.2f s at %s fps: %s" % (
            os.path.basename(path), root.get("version"), clips, markers, float(expect), float(1 / frame),
            "valid" if not problems else "%d problems" % len(problems)))
        for p in problems:
            print("  " + p)
    return problems


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "check":
        sys.exit(1 if check(sys.argv[2]) else 0)
    print(__doc__)
    sys.exit(2)
