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
"""
import argparse
import os
import re
import sys
import textwrap

TEMPLATE = "QVR Feature Title"           # the Fusion title's name in Effects > Titles
TRACK_NAME = "QVR Titles"
BIN_NAME = "QVR Titles"
PREFIX = "QVR - "                        # section timelines' names
CORNERS = {"top-right": 0, "top-left": 1, "bottom-left": 2, "bottom-right": 3,
           "tr": 0, "tl": 1, "bl": 2, "br": 3}
DEFAULTS = {"corner": "top-right", "seconds": "4.5", "wrap": "40", "in": "", "out": "", "intensity": ""}
KEYS = set(DEFAULTS) | {"subtitle", "marker"}


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
            sections.append(cur)
            continue
        m = re.fullmatch(r"([A-Za-z]+)\s*:\s*(.*)", line)
        if not m or m.group(1).lower() not in KEYS:
            raise ValueError("titles.txt line %d: expected '[Title]' or one of %s: %r"
                             % (n, ", ".join(sorted(KEYS)), raw))
        (cur if cur is not None else defaults)[m.group(1).lower()] = m.group(2).strip()
    out = []
    for s in sections:
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
    return defaults, out


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


def title_tool(item):
    comp = item.GetFusionCompByIndex(1)
    if comp is None:
        return None
    for tool in (comp.GetToolList(False) or {}).values():     # the macro: the tool with the published Corner
        if tool.GetAttrs().get("TOOLS_RegID") != "MediaOut" and tool.GetInput("Corner") is not None:
            return tool
    return None


def set_title(tool, sec, fps, frames):
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


def make_section_timeline(project, mp, folder, name, main):
    mp.SetCurrentFolder(folder)
    st = mp.CreateEmptyTimeline(name)
    if st is None:
        raise RuntimeError("could not create timeline %r" % name)
    st.SetSetting("useCustomSettings", "1")
    for k in ("timelineFrameRate", "timelineResolutionWidth", "timelineResolutionHeight"):
        st.SetSetting(k, main.GetSetting(k))
    project.SetCurrentTimeline(st)
    st.SetCurrentTimecode(st.GetStartTimecode())
    if not st.InsertFusionTitleIntoTimeline(TEMPLATE):
        raise RuntimeError("could not insert the %r title: is it installed (install_titles.py)?" % TEMPLATE)
    project.SetCurrentTimeline(main)
    return st


def apply(resolve, sections, track_name, dry, rebuild=False):
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
    placed = {it.GetName(): it for it in (video_items(main, track) if track <= main.GetTrackCount("video") else [])}
    folder = None if dry else find_bin(mp, main.GetName())
    print("apply_titles: timeline %r, %g fps, %d sections, track V%d %r%s"
          % (main.GetName(), fps, len(sections), track, track_name, " (dry run)" if dry else ""))
    wanted, done, missing = set(), 0, []
    for sec in sections:
        frames = main_frames = max(1, round(float(sec["seconds"]) * fps))
        offs = sorted(by_name.get(sec["key"].lower(), []))
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
                st = make_section_timeline(project, mp, folder, name, main)
                action.append("new")
            inner = video_items(st, 1)
            tool = title_tool(inner[0]) if inner else None
            if tool is None:
                print("  %-28s %r holds no QVR Feature Title: skipped" % (sec["title"][:28], name))
                continue
            inner_len = int(inner[0].GetDuration())
            if frames > inner_len:
                print("  %-28s %s s is longer than the title clip (%d frames): shortened"
                      % (sec["title"][:28], sec["seconds"], inner_len))
                frames = inner_len
            set_title(tool, sec, fps, frames)
        old = placed.get(name)
        if old is not None and int(old.GetStart()) == at and int(old.GetDuration()) == frames:
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
                         if i.GetName() not in placed and i.GetStart() < at + frames and i.GetEnd() > at]
                if clash:
                    print("  %-28s overlaps %s on V%d: placed anyway" % (sec["title"][:28], clash, track))
                mpi = st.GetMediaPoolItem()
                ok = mp.AppendToTimeline([{"mediaPoolItem": mpi, "startFrame": 0, "endFrame": frames,
                                           "trackIndex": track, "recordFrame": at, "mediaType": 1}])
                if not ok:
                    print("  %-28s could not be placed at %s" % (sec["title"][:28], at))
                    continue
        done += 1
        print("  %-28s %-12s %s  %s, %s s%s" % (sec["title"][:28], "+".join(action), timecode(at, fps),
              sec["corner"], sec["seconds"], (" (%d frames)" % frames) if frames != main_frames else ""))
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
    a = ap.parse_args()
    with open(a.titles, encoding="utf-8") as f:
        _, sections = parse_titles(f.read())
    if a.check:
        for s in sections:
            print("[%s] marker %r, %s, %s s\n  %s" % (s["title"], s["key"], s["corner"], s["seconds"],
                                                     s["subtitle_text"].replace("\n", "\n  ")))
        print("apply_titles: %d sections parsed" % len(sections))
        return
    r = get_resolve()
    if r is None:
        sys.exit("apply_titles: Resolve not reachable (running? Preferences > System > General > "
                 "External scripting using: Local)")
    apply(r, sections, a.track_name, a.dry_run, a.rebuild)


if __name__ == "__main__":
    main()
