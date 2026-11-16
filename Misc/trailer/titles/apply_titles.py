"""Put a QVR Feature Title at the start of every trailer section of the current Resolve timeline, from titles.txt.

    python Misc/trailer/titles/apply_titles.py [titles.txt] [--dry-run] [--track-name "QVR Titles"]

(or Workspace > Scripts > QVR Apply Titles, installed by install_titles.py). Resolve must be running with the
trailer's timeline open; from outside Resolve, Preferences > System > General > External scripting using: Local.

The rule: a section starts at the **timeline marker named like the section** (the section's `marker:`, or its title;
case and spaces at the ends ignored). Put the marker on the section's first frame (on the cut).

Each section gets its own small timeline holding one title, "QVR - <section>", in the media pool bin
"QVR Titles/<this timeline>", placed (nested, video only) on the track named "QVR Titles" (made on top when missing)
at the marker. Running again updates the text in those timelines in place (the nested copies follow) and moves or
re-places only what changed: no duplicates. A placed title whose section left titles.txt is taken off the track.
The track should hold only these titles; anything else on it is left alone (and reported if a title would overlap).

An `[Intro]` block makes the full-screen QVR Intro Title instead (line1/line2/line3, blood, backdrop): it goes at the
marker named "Intro" (or its `marker:`), or at the timeline's start when there is no such marker.

Fast previews: `--bake` renders each title once to a ProRes 4444 clip with alpha ("<section timeline> [<digest>].mov"
in "QVR Titles (baked)" next to the trailer's media, or --bake-dir) and places that instead of the live title; the
digest covers the title's comp, frame size and length, so unchanged titles reuse their clip. A plain run keeps a baked
title baked while it is unchanged (else it goes back live); `--unbake` or `--rebuild` puts the live titles back.
`--draft on|off` sets every title's Draft (fast preview) toggle; a bake always renders with it off.
"""
import argparse
import hashlib
import json
import os
import re
import sys
import tempfile
import textwrap
import time

TEMPLATE = "QVR Feature Title"           # the Fusion title's name in Effects > Titles
TRACK_NAME = "QVR Titles"
BIN_NAME = "QVR Titles"
PREFIX = "QVR - "                        # section timelines' names
FALLBACK_FONT = ("Cinzel", "Bold")       # draws the title characters DpQuake maps to logos (Q, q, ~, ...)
CORNERS = {"top-right": 0, "top-left": 1, "bottom-left": 2, "bottom-right": 3,
           "tr": 0, "tl": 1, "bl": 2, "br": 3}
DEFAULTS = {"corner": "top-right", "seconds": "4.5", "wrap": "40", "in": "", "out": "", "intensity": ""}
KEYS = set(DEFAULTS) | {"subtitle", "marker"}
INTRO_TEMPLATE = "QVR Intro Title"
INTRO = "intro"                          # the [Intro] block's header (case ignored) and default marker name
INTRO_DEFAULTS = {"line1": "Quake VR", "line2": "UNLEASHED", "line3": "v1.1 Release Trailer", "seconds": "5",
                  "marker": "Intro", "out": "", "intensity": "", "blood": "", "backdrop": ""}
INTRO_KEYS = set(INTRO_DEFAULTS)
# the intro's timing inputs (frames) and their template defaults, made for 60 fps: scaled on other frame rates
INTRO_TIMING = {"L1Start": 0, "L1In": 46, "L2Start": 34, "L2In": 44, "RuleStart": 66, "RuleIn": 26, "L3Start": 84,
                "L3In": 34, "SweepStart": 24, "SweepFrames": 110, "OutFrames": 36}
ON_OFF = {"on": 1, "yes": 1, "true": 1, "1": 1, "off": 0, "no": 0, "false": 0, "0": 0}
BAKE_DIR = "QVR Titles (baked)"         # --bake's folder, next to the trailer's media
BAKED_RE = re.compile(r"^(.*) \[([0-9a-f]{10})\]\.mov$")   # a baked clip: "<section timeline> [<digest>].mov"


# --- titles.txt ----------------------------------------------------------------------------------------------------

def parse_titles(text):
    """-> (defaults, [section dict]). A section: '[Title]' then 'key: value' lines; lines before the first section
    set defaults for all. '#' starts a comment line."""
    defaults, sections, cur = dict(DEFAULTS), [], None
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        m = re.fullmatch(r"\[(.+)\]", line)
        if m:
            cur = {"title": m.group(1).strip(), "line": n}
            if cur["title"].lower() == INTRO:
                cur["intro"] = True
            sections.append(cur)
            continue
        m = re.fullmatch(r"([A-Za-z0-9]+)\s*:\s*(.*)", line)
        keys = INTRO_KEYS if cur is not None and cur.get("intro") else KEYS
        if not m or m.group(1).lower() not in keys:
            raise ValueError("titles.txt line %d: expected '[Title]' or one of %s: %r"
                             % (n, ", ".join(sorted(keys)), raw))
        (cur if cur is not None else defaults)[m.group(1).lower()] = m.group(2).strip()
    out = []
    for s in sections:
        if s.get("intro"):
            out.append(parse_intro(s))
            continue
        sec = dict(defaults)
        sec.update(s)
        corner = sec["corner"].lower().replace(" ", "-")
        if corner not in CORNERS:
            raise ValueError("titles.txt line %d: corner %r (top-right, top-left, bottom-left, bottom-right)"
                             % (s["line"], sec["corner"]))
        sec["corner_index"] = CORNERS[corner]
        sec["key"] = (sec.get("marker") or sec["title"]).strip()
        sec["subtitle_text"] = format_subtitle(sec.get("subtitle", ""), int(sec["wrap"]))
        out.append(sec)
    keys = [s["key"].lower() for s in out]
    dup = sorted({k for k in keys if keys.count(k) > 1})
    if dup:
        raise ValueError("titles.txt: two sections with the same marker name: %s" % ", ".join(dup))
    if sum(1 for s in out if s.get("intro")) > 1:
        raise ValueError("titles.txt: more than one [Intro] block")
    return defaults, out


def parse_intro(s):
    """The [Intro] block: its own defaults (the feature sections' defaults don't apply)."""
    sec = dict(INTRO_DEFAULTS)
    sec.update(s)
    sec["key"] = sec["marker"].strip() or INTRO_DEFAULTS["marker"]
    # DpQuake draws Q, q and ~ (and codes past ASCII) as id's logos: the template draws a leading Q itself, any other
    # one would show a logo
    bad = [ch for ch in sec["line1"][1:] if ch in "Qq~" or ord(ch) > 126]
    if bad or (sec["line1"][:1] and (sec["line1"][0] == "~" or ord(sec["line1"][0]) > 126)):
        raise ValueError("titles.txt line %d: line1 %r: only a leading Q can be drawn (the Quake font maps Q, q, ~ "
                         "and non-ASCII characters to logos)" % (s["line"], sec["line1"]))
    if sec["backdrop"] and sec["backdrop"].lower() not in ON_OFF:
        raise ValueError("titles.txt line %d: backdrop %r (on or off)" % (s["line"], sec["backdrop"]))
    return sec


def format_subtitle(sub, wrap):
    """'|' is a line break; with none, lines are wrapped at `wrap` characters (0: never)."""
    if "|" in sub:
        return "\n".join(p.strip() for p in sub.split("|"))
    return "\n".join(textwrap.wrap(sub, wrap)) if wrap > 0 and sub else sub


# --- Resolve -------------------------------------------------------------------------------------------------------

def get_resolve():
    r = globals().get("resolve")
    if r is not None:
        return r
    api = os.environ.get("RESOLVE_SCRIPT_API",
                         r"C:\ProgramData\Blackmagic Design\DaVinci Resolve\Support\Developer\Scripting")
    os.environ.setdefault("RESOLVE_SCRIPT_LIB", r"C:\Program Files\Blackmagic Design\DaVinci Resolve\fusionscript.dll")
    sys.path.append(os.path.join(api, "Modules"))
    try:
        import DaVinciResolveScript as dvr
    except ImportError:
        return None
    return dvr.scriptapp("Resolve")


def video_items(tl, track):
    return list(tl.GetItemListInTrack("video", track) or [])


def find_track(tl, name, dry):
    for t in range(1, tl.GetTrackCount("video") + 1):
        if tl.GetTrackName("video", t) == name:
            return t
    if dry:
        return tl.GetTrackCount("video") + 1
    tl.AddTrack("video")
    t = tl.GetTrackCount("video")
    tl.SetTrackName("video", t, name)
    return t


def find_bin(mp, timeline_name):
    root = mp.GetRootFolder()
    top = next((f for f in root.GetSubFolderList() if f.GetName() == BIN_NAME), None) or mp.AddSubFolder(root, BIN_NAME)
    sub = next((f for f in top.GetSubFolderList() if f.GetName() == timeline_name), None)
    return sub or mp.AddSubFolder(top, timeline_name)


def find_timeline(project, name):
    for i in range(1, project.GetTimelineCount() + 1):
        t = project.GetTimelineByIndex(i)
        if t.GetName() == name:
            return t
    return None


def title_tool(item, published="Corner"):
    comp = item.GetFusionCompByIndex(1)
    if comp is None:
        return None
    for tool in (comp.GetToolList(False) or {}).values():  # the macro: the tool with that published input
        if tool.GetAttrs().get("TOOLS_RegID") != "MediaOut" and tool.GetInput(published) is not None:
            return tool
    return None


def fallback_runs(text):
    """[(start, end)] of the characters the title font must not draw: DpQuake maps Q and q (and ~ and the codes past
    ASCII) to id Software's logos, never shown in our titles. They are drawn in FALLBACK_FONT instead."""
    runs = []
    for i, ch in enumerate(text):
        if ch in "Qq~" or ord(ch) > 126:
            if runs and runs[-1][1] == i:
                runs[-1] = (runs[-1][0], i + 1)
            else:
                runs.append((i, i + 1))
    return runs


def block_end(s, i):
    """Index just past the '}' closing the first '{' at or after i (no braces inside strings here)."""
    depth, j = 0, s.index("{", i)
    while True:
        depth += {"{": 1, "}": -1}.get(s[j], 0)
        j += 1
        if depth == 0:
            return j


def set_glyph_fallback(item, text):
    """FALLBACK_FONT for fallback_runs(text) through the macro's TitleCLS (a Text+ character-level styling modifier).
    Scripting can't set a styling value, so the item's comp is exported, given the styling as text and imported back.
    -> False when the comp has no TitleCLS (made with an older template: --rebuild)."""
    ents = ", ".join('{ 100, %d, %d, String = "%s" }, { 109, %d, %d, String = "%s" }'
                     % (a, b, FALLBACK_FONT[0], a, b, FALLBACK_FONT[1]) for a, b in fallback_runs(text))
    path = os.path.join(tempfile.gettempdir(), "qvr_apply_titles.comp")
    if not item.ExportFusionComp(path, 1):
        return False
    with open(path, encoding="utf-8") as f:
        s = f.read()
    at = s.find("TitleCLS = StyledTextCLS {")
    if at < 0:
        return False
    new = 'CharacterLevelStyling = Input { Value = StyledText { Array = { %s }, Value = "" }, }' % ents
    i = s.find("CharacterLevelStyling = Input {", at)
    if 0 <= i < block_end(s, at):
        j = block_end(s, i)
        if s[i:j] == new:
            return True
    else:  # an empty styling isn't saved: add the input at the top of the modifier's inputs
        if not ents:
            return True
        i = j = s.index("Inputs = {", at) + len("Inputs = {")
        new = "\n" + new + ","
    with open(path, "w", encoding="utf-8") as f:
        f.write(s[:i] + new + s[j:])
    return bool(item.ImportFusionComp(path))


def set_title(tool, sec, fps, frames, item=None):
    tool.SetInput("Title", sec["title"])
    tool.SetInput("Subtitle", sec["subtitle_text"])
    tool.SetInput("Corner", sec["corner_index"])
    if sec["intensity"]:
        tool.SetInput("Intensity", float(sec["intensity"]))
    if sec["in"]:
        tool.SetInput("InFrames", max(1, round(float(sec["in"]) * fps)))
    if sec["out"]:
        tool.SetInput("OutFrames", max(1, round(float(sec["out"]) * fps)))
    tin = tool.GetInput("InFrames") or 40
    tout = tool.GetInput("OutFrames") or 26
    tool.SetInput("HoldFrames", max(1, frames - int(tin) - int(tout)))
    # last: it re-imports the comp (with everything set above), so `tool` is stale afterwards
    if item is not None and not set_glyph_fallback(item, sec["title"]) and fallback_runs(sec["title"]):
        print("  %-28s Q and other logo glyphs not replaced: run with --rebuild" % sec["title"][:28])


def set_intro(tool, sec, fps, frames, clip_frames):
    tool.SetInput("Line1", sec["line1"])
    tool.SetInput("Line2", sec["line2"])
    tool.SetInput("Line3", sec["line3"])
    if abs(fps - 60) > 0.5:  # the template's timings are frames at 60 fps: keep their length in seconds
        for k, v in INTRO_TIMING.items():
            tool.SetInput(k, max(1 if k.endswith(("In", "Frames")) else 0, round(v * fps / 60)))
    if sec["intensity"]:
        tool.SetInput("Intensity", float(sec["intensity"]))
    if sec["blood"]:
        tool.SetInput("Blood", float(sec["blood"]))
    if sec["backdrop"]:
        tool.SetInput("Backdrop", ON_OFF[sec["backdrop"].lower()])
    if sec["out"]:
        tool.SetInput("OutFrames", max(1, round(float(sec["out"]) * fps)))
    tout = int(tool.GetInput("OutFrames") or 36)
    # the out ends with the placed (possibly trimmed) clip; 0 = the title clip's own end
    tool.SetInput("OutAt", 0 if frames >= clip_frames else max(1, frames - tout))


# --- baking: a title rendered once to a ProRes 4444 clip (with alpha) that plays in real time ------------------------

def base_name(name):
    """A placed item's section name: a baked clip's name without its ' [digest].mov'."""
    m = BAKED_RE.match(name or "")
    return m.group(1) if m else name


def is_baked(name):
    return bool(BAKED_RE.match(name or ""))


def comp_digest(item, extra):
    """A digest of everything the title's look depends on: its exported comp (the macro's inputs, hand tweaks
    included; the Draft toggle left out: a bake always renders at full quality) and the frame size, rate and length."""
    path = os.path.join(tempfile.gettempdir(), "qvr_apply_titles_digest.comp")
    if not item.ExportFusionComp(path, 1):
        return None
    with open(path, encoding="utf-8") as f:
        s = f.read()
    # only the tools (the comp's own header holds its current time, ids and preferences, which change by themselves),
    # without node positions
    at = s.find("\tTools = ")
    if at >= 0:
        s = s[at:block_end(s, at)]
    s = re.sub(r"\bDraft = Input \{[^}]*\},?", "", s)
    s = re.sub(r"\n\s*ViewInfo = [^\n]*", "", s)
    return hashlib.sha1((s + "\n" + extra).encode("utf-8")).hexdigest()[:10]


def bake_folder(main, override):
    """Next to the project media: the folder of the first file used on the trailer timeline, + BAKE_DIR (with no
    file there, Videos/BAKE_DIR/<timeline> in the user's folder)."""
    if override:
        return override
    for t in range(1, main.GetTrackCount("video") + 1):
        for it in video_items(main, t):
            mpi = it.GetMediaPoolItem()
            path = mpi and mpi.GetClipProperty("File Path")
            if path and os.path.isfile(path):
                return os.path.join(os.path.dirname(path), BAKE_DIR)
    return os.path.join(os.path.expanduser("~"), "Videos", BAKE_DIR, main.GetName())


def find_clip(folder, name):
    for c in folder.GetClipList() or []:
        if c.GetName() == name:
            return c
    return None


def render_bake(resolve, project, st, main, frames, out_dir, stem):
    """Render frames 0..frames-1 of the section timeline st to out_dir/stem.mov, ProRes 4444 with alpha. -> path."""
    os.makedirs(out_dir, exist_ok=True)
    page = resolve.GetCurrentPage()
    project.SetCurrentTimeline(st)
    resolve.OpenPage("deliver")
    try:
        codecs = project.GetRenderCodecs("mov") or {}
        codec = next((v for k, v in codecs.items() if "4444" in k and "XQ" not in k), None)
        if codec is None or not project.SetCurrentRenderFormatAndCodec("mov", codec):
            raise RuntimeError("Resolve offers no ProRes 4444 render codec (%s)" % ", ".join(codecs))
        s0 = st.GetStartFrame()
        project.SetRenderSettings({"SelectAllFrames": False, "MarkIn": s0, "MarkOut": s0 + frames - 1,
                                   "TargetDir": out_dir, "CustomName": stem, "ExportVideo": True,
                                   "ExportAudio": False, "ExportAlpha": True,
                                   "FormatWidth": int(st.GetSetting("timelineResolutionWidth")),
                                   "FormatHeight": int(st.GetSetting("timelineResolutionHeight")),
                                   "FrameRate": float(st.GetSetting("timelineFrameRate"))})
        job = project.AddRenderJob()
        if not job or not project.StartRendering([job]):
            raise RuntimeError("could not start the render of %r" % st.GetName())
        while project.IsRenderingInProgress():
            time.sleep(0.25)
        status = project.GetRenderJobStatus(job) or {}
        project.DeleteRenderJob(job)    # only ours: the render queue is otherwise left alone
        if status.get("JobStatus") != "Complete":
            raise RuntimeError("the render of %r ended %r" % (st.GetName(), status.get("JobStatus")))
    finally:
        project.SetCurrentTimeline(main)
        resolve.OpenPage(page or "edit")
    path = os.path.join(out_dir, stem + ".mov")
    if not os.path.isfile(path):
        raise RuntimeError("the baked clip %r was not written" % path)
    return path


def baked_clip(resolve, project, mp, folder, st, main, item, tool, frames, out_dir, name, digest, sec):
    """The media pool clip of this title baked as it is now: reused when its digest matches, else rendered (with the
    Draft toggle off) and imported into the section's bin, the section's settings in its Comments."""
    stem = "%s [%s]" % (name, digest)
    clip = find_clip(folder, stem + ".mov")
    if clip is not None:
        return clip, False
    path = os.path.join(out_dir, stem + ".mov")
    rendered = not os.path.isfile(path)
    if rendered:
        draft = tool.GetInput("Draft")
        if draft:
            tool.SetInput("Draft", 0)
        try:
            render_bake(resolve, project, st, main, frames, out_dir, stem)
        finally:
            if draft:
                tool.SetInput("Draft", draft)
    mp.SetCurrentFolder(folder)
    clips = mp.ImportMedia([path]) or []
    if not clips:
        raise RuntimeError("could not import %r" % path)
    keep = {k: v for k, v in sec.items() if k not in ("line", "intro") and isinstance(v, (str, int, float))}
    clips[0].SetClipProperty("Comments", "QVR title bake of %r (%d frames): %s"
                             % (st.GetName(), frames, json.dumps(keep, sort_keys=True)))
    return clips[0], rendered


def make_section_timeline(project, mp, folder, name, main, template=TEMPLATE):
    mp.SetCurrentFolder(folder)
    st = mp.CreateEmptyTimeline(name)
    if st is None:
        raise RuntimeError("could not create timeline %r" % name)
    st.SetSetting("useCustomSettings", "1")
    for k in ("timelineFrameRate", "timelineResolutionWidth", "timelineResolutionHeight"):
        st.SetSetting(k, main.GetSetting(k))
    project.SetCurrentTimeline(st)
    st.SetCurrentTimecode(st.GetStartTimecode())
    if not st.InsertFusionTitleIntoTimeline(template):
        raise RuntimeError("could not insert the %r title: is it installed (install_titles.py)?" % template)
    project.SetCurrentTimeline(main)
    return st


def apply(resolve, sections, track_name, dry, rebuild=False, mode=None, bake_dir=None, draft=None):
    project = resolve.GetProjectManager().GetCurrentProject()
    main = project and project.GetCurrentTimeline()
    if main is None:
        sys.exit("apply_titles: no timeline open in Resolve")
    if main.GetName().startswith(PREFIX):
        sys.exit("apply_titles: %r is a section title's own timeline: open the trailer's timeline" % main.GetName())
    fps = float(main.GetSetting("timelineFrameRate"))
    t0 = main.GetStartFrame()
    by_name = {}
    for off, m in (main.GetMarkers() or {}).items():
        by_name.setdefault(m["name"].strip().lower(), []).append(int(off))
    mp = project.GetMediaPool()
    track = find_track(main, track_name, dry)
    # by section name: a baked clip counts as its section's title
    placed = {base_name(it.GetName()): it
              for it in (video_items(main, track) if track <= main.GetTrackCount("video") else [])}
    size = "%sx%s@%g" % (main.GetSetting("timelineResolutionWidth"), main.GetSetting("timelineResolutionHeight"), fps)
    out_dir = None if dry or mode != "bake" else bake_folder(main, bake_dir)
    if out_dir:
        print("apply_titles: baking into %s" % out_dir)
    folder = None if dry else find_bin(mp, main.GetName())
    print("apply_titles: timeline %r, %g fps, %d sections, track V%d %r%s"
          % (main.GetName(), fps, len(sections), track, track_name, " (dry run)" if dry else ""))
    wanted, done, missing = set(), 0, []
    for sec in sections:
        frames = main_frames = max(1, round(float(sec["seconds"]) * fps))
        intro = bool(sec.get("intro"))
        offs = sorted(by_name.get(sec["key"].lower(), []))
        if not offs and intro:
            offs = [0]
            print("  %-28s no timeline marker named %r: at the timeline's start" % (sec["title"][:28], sec["key"]))
        if not offs:
            missing.append(sec["key"])
            print("  %-28s no timeline marker named %r: skipped" % (sec["title"][:28], sec["key"]))
            continue
        if len(offs) > 1:
            print("  %-28s %d markers named %r: the first is used" % (sec["title"][:28], len(offs), sec["key"]))
        at = t0 + offs[0]
        name = PREFIX + sec["key"]
        wanted.add(name)
        st = None if dry else find_timeline(project, name)
        action = []
        if rebuild and (st is not None or name in placed):
            action.append("rebuilt")
            if not dry:
                if name in placed:
                    main.DeleteClips([placed.pop(name)], False)
                if st is not None:
                    mp.DeleteTimelines([st])
                    st = None
        if not dry:
            if st is None:
                st = make_section_timeline(project, mp, folder, name, main, INTRO_TEMPLATE if intro else TEMPLATE)
                action.append("new")
            inner = video_items(st, 1)
            tool = title_tool(inner[0], "Line1" if intro else "Corner") if inner else None
            if tool is None:
                print("  %-28s %r holds no %s: skipped (--rebuild)"
                      % (sec["title"][:28], name, INTRO_TEMPLATE if intro else TEMPLATE))
                continue
            inner_len = int(inner[0].GetDuration())
            if frames > inner_len:
                print("  %-28s %s s is longer than the title clip (%d frames): shortened"
                      % (sec["title"][:28], sec["seconds"], inner_len))
                frames = inner_len
            if draft is not None and tool.GetInput("Draft") is not None:
                tool.SetInput("Draft", draft)
            if intro:
                set_intro(tool, sec, fps, frames, inner_len)
            else:
                set_title(tool, sec, fps, frames, inner[0])
        old = placed.get(name)
        want = None if dry else st.GetMediaPoolItem()
        old_name = old.GetName() if old is not None else ""
        if mode == "bake" and dry:
            action.append("bake")
        elif not dry and (mode == "bake" or (mode is None and not rebuild and is_baked(old_name))):
            tool = title_tool(inner[0], "Line1" if intro else "Corner")   # set_title re-imports the comp
            digest = comp_digest(inner[0], "%s:%d" % (size, frames))
            if digest is None:
                print("  %-28s could not export its comp: left live" % sec["title"][:28])
            elif mode == "bake":
                want, rendered = baked_clip(resolve, project, mp, folder, st, main, inner[0], tool, frames, out_dir,
                                            name, digest, sec)
                action.append("baked" if rendered else "baked-reused")
            elif old_name == "%s [%s].mov" % (name, digest):
                want = old.GetMediaPoolItem()      # still as baked: stays baked
            else:
                action.append("unbaked(changed: --bake again)")
        same = (old is not None and want is not None and old.GetMediaPoolItem() is not None
                and old.GetMediaPoolItem().GetUniqueId() == want.GetUniqueId())
        if old is not None and int(old.GetStart()) == at and int(old.GetDuration()) == frames and (dry or same):
            action.append("updated")
        else:
            if old is not None:
                action.append("moved")
                if not dry:
                    main.DeleteClips([old], False)
            else:
                action.append("placed")
            if not dry:
                clash = [i.GetName() for i in video_items(main, track)
                         if base_name(i.GetName()) not in placed and i.GetStart() < at + frames and i.GetEnd() > at]
                if clash:
                    print("  %-28s overlaps %s on V%d: placed anyway" % (sec["title"][:28], clash, track))
                ok = mp.AppendToTimeline([{"mediaPoolItem": want, "startFrame": 0, "endFrame": frames,
                                           "trackIndex": track, "recordFrame": at, "mediaType": 1}])
                if not ok:
                    print("  %-28s could not be placed at %s" % (sec["title"][:28], at))
                    continue
        done += 1
        print("  %-28s %-12s %s  %s, %s s%s" % (sec["title"][:28], "+".join(action), timecode(at, fps),
              "full screen" if intro else sec["corner"], sec["seconds"],
              (" (%d frames)" % frames) if frames != main_frames else ""))
    stale = [n for n in placed if n.startswith(PREFIX) and n not in wanted]
    for n in stale:
        print("  %-28s not in titles.txt (or its marker is gone): taken off V%d" % (n[len(PREFIX):][:28], track))
        if not dry:
            main.DeleteClips([placed[n]], False)
    print("apply_titles: %d of %d sections %s, %d removed, %d without a marker%s"
          % (done, len(sections), "planned" if dry else "applied", len(stale), len(missing),
             (": " + ", ".join(missing)) if missing else ""))


def timecode(frame, fps):
    f, r = int(frame), int(round(fps))
    return "%02d:%02d:%02d:%02d" % (f // (3600 * r), (f // (60 * r)) % 60, (f // r) % 60, f % r)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("titles", nargs="?", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "titles.txt"))
    ap.add_argument("--dry-run", action="store_true", help="print what would be done, change nothing")
    ap.add_argument("--track-name", default=TRACK_NAME, help="the video track for the titles (default %(default)r)")
    ap.add_argument("--check", action="store_true", help="only parse titles.txt and print the sections")
    ap.add_argument("--rebuild", action="store_true",
                    help="make every section title again from the installed template (after changing the template)")
    ap.add_argument("--bake", action="store_true",
                    help="render each title once to a ProRes 4444 clip with alpha (real-time playback) and place that "
                         "instead of the live title; unchanged titles reuse their clip")
    ap.add_argument("--unbake", action="store_true", help="put the live titles back in place of baked clips")
    ap.add_argument("--bake-dir", help="the folder for baked clips (default: '%s' next to the trailer's media)" % BAKE_DIR)
    ap.add_argument("--draft", choices=["on", "off"],
                    help="set every title's Draft (fast preview) toggle; baking always renders at full quality")
    a = ap.parse_args()
    if a.bake and (a.unbake or a.rebuild):
        ap.error("--bake goes alone (--rebuild and --unbake put the live titles back)")
    with open(a.titles, encoding="utf-8") as f:
        _, sections = parse_titles(f.read())
    if a.check:
        for s in sections:
            if s.get("intro"):
                print("[%s] marker %r (else the timeline's start), full screen, %s s\n  %s / %s / %s"
                      % (s["title"], s["key"], s["seconds"], s["line1"], s["line2"], s["line3"]))
                continue
            print("[%s] marker %r, %s, %s s\n  %s" % (s["title"], s["key"], s["corner"], s["seconds"],
                                                     s["subtitle_text"].replace("\n", "\n  ")))
        print("apply_titles: %d sections parsed" % len(sections))
        return
    r = get_resolve()
    if r is None:
        sys.exit("apply_titles: Resolve not reachable (running? Preferences > System > General > "
                 "External scripting using: Local)")
    mode = "bake" if a.bake else "unbake" if a.unbake else None
    apply(r, sections, a.track_name, a.dry_run, a.rebuild, mode, a.bake_dir,
          None if a.draft is None else ON_OFF[a.draft])


if __name__ == "__main__":
    main()
