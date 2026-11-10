#!/usr/bin/env python3
"""fcpxml_check.py -- checks a take session's timeline.fcpxml (Quake/vr/vr_takes.cpp; TRAILER.md, "Takes and clips")
against FCPXML 1.9/1.10's rules for what it uses, offline (Apple's DTD is not shipped here, nor needed):

- the elements nest as the DTD has them (fcpxml > resources, library > event > project > sequence > spine >
  asset-clip > marker; resources > format, asset > media-rep), each with only the DTD's attributes, the required ones
  present, ids unique, every ref and format to an id;
- every time is FCPXML's rational ("0s", "N/Ds", "Ns"), on the format's frame grid;
- the spine's clips follow each other without gaps, the sequence as long as them; each clip within its asset;
  each marker within its clip's source range;
- media-rep src is a file:// URL (and, with --files, the file is there).

  python Misc/quakevr/fcpxml_check.py <timeline.fcpxml> [--files]

Ends with "fcpxml_check: N passed, M failed".
"""
import os
import sys
import urllib.parse
import xml.etree.ElementTree as ET
from fractions import Fraction

# The DTD's attributes for the elements a take session writes (FCPXML 1.10; 1.9 the same for these).
ATTRS = {
    "fcpxml": ({"version"}, set()),
    "resources": (set(), set()),
    "format": ({"id"}, {"name", "frameDuration", "fieldOrder", "width", "height", "paspH", "paspV", "colorSpace",
                        "projection", "stereoscopic"}),
    "asset": ({"id"}, {"name", "uid", "start", "duration", "hasVideo", "format", "hasAudio", "videoSources",
                       "audioSources", "audioChannels", "audioRate", "customLUTOverride", "colorSpaceOverride",
                       "projectionOverride", "stereoscopicOverride", "auxVideoFlags"}),
    "media-rep": ({"src"}, {"kind", "sig", "suggestedFilename"}),
    "library": (set(), {"location", "colorProcessing"}),
    "event": (set(), {"name", "uid"}),
    "project": (set(), {"name", "uid", "id", "modDate"}),
    "sequence": ({"format"}, {"duration", "tcStart", "tcFormat", "audioLayout", "audioRate", "renderFormat",
                              "keywords"}),
    "spine": (set(), {"name", "offset", "format", "lane"}),
    "asset-clip": ({"ref"}, {"lane", "offset", "name", "start", "duration", "enabled", "format", "tcStart",
                             "tcFormat", "audioRole", "videoRole", "srcEnable", "audioStart", "audioDuration",
                             "modDate"}),
    "marker": ({"start", "value"}, {"duration", "completed", "note"}),
}
CHILDREN = {
    "fcpxml": ["resources", "library"],
    "resources": ["format", "asset"],
    "asset": ["media-rep"],
    "library": ["event"],
    "event": ["project"],
    "project": ["sequence"],
    "sequence": ["spine"],
    "spine": ["asset-clip"],
    "asset-clip": ["marker"],
    "media-rep": [],
    "format": [],
    "marker": [],
}

passed = failed = 0


def check(ok, what):
    global passed, failed
    if ok:
        passed += 1
    else:
        failed += 1
        print("fcpxml_check: FAIL " + what)
    return ok


def rational(text):
    """FCPXML's time: "0s", "Ns", "N/Ds"; None when it is not one."""
    if not text or not text.endswith("s"):
        return None
    body = text[:-1]
    try:
        if "/" in body:
            n, d = body.split("/")
            if not (n.isdigit() and d.isdigit()) or int(d) == 0:
                return None
            return Fraction(int(n), int(d))
        if not body.isdigit():
            return None
        return Fraction(int(body))
    except ValueError:
        return None


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    path, want_files = sys.argv[1], "--files" in sys.argv[2:]
    with open(path, "rb") as f:
        head = f.read(200)
    check(head.startswith(b'<?xml version="1.0" encoding="UTF-8"?>'), "an XML declaration with UTF-8 first")
    check(b"<!DOCTYPE fcpxml>" in head, "<!DOCTYPE fcpxml>")
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError as e:
        check(False, "well-formed XML: %s" % e)
        print("fcpxml_check: %d passed, %d failed" % (passed, failed))
        return 1
    check(root.tag == "fcpxml" and root.get("version") in ("1.9", "1.10", "1.11"), "fcpxml version 1.9 to 1.11")

    ids = {}
    for el in root.iter():
        req, opt = ATTRS.get(el.tag, (None, None))
        if not check(req is not None, "element <%s> is one the writer may use" % el.tag):
            continue
        missing = req - set(el.attrib)
        extra = set(el.attrib) - req - opt
        check(not missing, "<%s> has %s" % (el.tag, ", ".join(sorted(missing)) or "its required attributes"))
        check(not extra, "<%s>'s attributes are the DTD's (not %s)" % (el.tag, ", ".join(sorted(extra))))
        for child in el:
            check(child.tag in CHILDREN.get(el.tag, []), "<%s> inside <%s>" % (child.tag, el.tag))
        if "id" in el.attrib:
            check(el.get("id") not in ids, "id %s unique" % el.get("id"))
            ids[el.get("id")] = el
    tags = [c.tag for c in root]
    check(tags == ["resources", "library"], "fcpxml holds resources, then library")

    formats = {i: e for i, e in ids.items() if e.tag == "format"}
    assets = {i: e for i, e in ids.items() if e.tag == "asset"}
    check(len(formats) >= 1, "a format")
    frame = {}
    for i, f in formats.items():
        fd = rational(f.get("frameDuration"))
        check(fd is not None and fd > 0, "format %s's frameDuration is a time" % i)
        frame[i] = fd or Fraction(1, 60)
        check(f.get("width", "").isdigit() and f.get("height", "").isdigit(), "format %s's size" % i)

    def on_grid(t, fmt, what):
        fd = frame.get(fmt, Fraction(1, 60))
        check(t is not None and (t / fd).denominator == 1, "%s on the frame grid (%s)" % (what, t))

    for i, a in assets.items():
        fmt = a.get("format")
        check(fmt in formats, "asset %s's format is a format" % i)
        start, dur = rational(a.get("start", "0s")), rational(a.get("duration"))
        check(start is not None and dur is not None and dur > 0, "asset %s's start and duration are times" % i)
        on_grid(dur, fmt, "asset %s's duration" % i)
        reps = a.findall("media-rep")
        if check(len(reps) >= 1, "asset %s has its media-rep" % i):
            src = reps[0].get("src", "")
            check(src.startswith("file:///") or src.startswith("file://localhost/"), "asset %s's src is a file:// URL" % i)
            check(reps[0].get("kind", "original-media") in ("original-media", "proxy-media"), "media-rep kind")
            if want_files:
                p = urllib.parse.unquote(src[len("file:///"):])
                check(os.path.isfile(p), "asset %s's file is there (%s)" % (i, p))

    for seq in root.iter("sequence"):
        fmt = seq.get("format")
        check(fmt in formats, "the sequence's format is a format")
        total = Fraction(0)
        for spine in seq.findall("spine"):
            for clip in spine.findall("asset-clip"):
                name = clip.get("name", "?")
                a = assets.get(clip.get("ref"))
                if not check(a is not None, "%s's ref is an asset" % name):
                    continue
                if clip.get("format"):
                    check(clip.get("format") in formats, "%s's format is a format" % name)
                off = rational(clip.get("offset", "0s"))
                start = rational(clip.get("start", a.get("start", "0s")))
                dur = rational(clip.get("duration", a.get("duration")))
                if not check(None not in (off, start, dur) and dur > 0, "%s's offset, start and duration are times" % name):
                    continue
                for t, what in ((off, "offset"), (start, "start"), (dur, "duration")):
                    on_grid(t, fmt, "%s's %s" % (name, what))
                check(off == total, "%s follows the clip before without a gap (%s, %s)" % (name, off, total))
                a0, ad = rational(a.get("start", "0s")), rational(a.get("duration"))
                check(start >= a0 and start + dur <= a0 + ad, "%s lies within its asset" % name)
                total += dur
                for m in clip.findall("marker"):
                    ms, md = rational(m.get("start")), rational(m.get("duration", "0s"))
                    if check(ms is not None and md is not None, "%s's marker times" % name):
                        on_grid(ms, fmt, "%s's marker %s" % (name, m.get("value")))
                        check(start <= ms < start + dur, "%s's marker %s within the clip" % (name, m.get("value")))
                    check(bool(m.get("value", "").strip()), "%s's marker has a name" % name)
        sd = rational(seq.get("duration", "0s"))
        check(sd == total, "the sequence is as long as its clips (%s, %s)" % (sd, total))

    print("fcpxml_check: %d passed, %d failed" % (passed, failed))
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
