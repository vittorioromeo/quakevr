"""Bake title variants to pick from: each one a QVR Feature Title rendered to its own ProRes 4444 clip with alpha.

    python Misc/trailer/titles/bake_variants.py variants.txt --out "<folder>" [--project "QVR Titles Preview"]
        [--size 1920x1080] [--fps 60] [--only REGEX] [--still 4] [--no-sheet] [--no-reel]

variants.txt: one variant a line, `section | n | title | subtitle` ('#' starts a comment; " / " in the subtitle is the
titles.txt line break "|"; an optional 5th column is extra titles.txt keys, `seconds=4.5 corner=bl`). Everything else
is the template's and titles.txt's defaults (top-right, 6 s, the subtitle 1 s after the title).

Each variant becomes `<section>_v<n> - <title>.mov` in --out (characters Windows refuses in a file name dropped); a
clip already there is kept (not rendered again). The titles are made the way apply_titles makes a section's title (a
small timeline holding one title, set by set_title) and rendered the way `apply_titles --bake` renders (one batch, the
Draft toggle off, only our own jobs taken off the queue); the timelines and their bin are removed afterwards.

--project: bake in that project (e.g. a scratch one) instead of the open one: the open project is saved first (when
it is a saved one), and opened again at the end. The frame size and rate are the open project's current timeline's
(read before switching), else 1920x1080 at 60 fps; --size / --fps override them.

Then, with ffmpeg on the PATH: `contact sheet.png` (one hold frame of each, --still seconds in, over dark grey,
labelled section_vN, in rows of --columns) and `all variants.mp4` (1080p H.264, every variant over dark grey one after
another, labelled).
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import apply_titles as at  # noqa: E402

BIN = "Variants"                  # the bin (under QVR Titles) and the prefix of the variants' timelines
REF = "QVR Variants (frame)"      # a timeline giving the variants their frame size and rate
GREY = "0x262626"
BAD_CHARS = '<>:"/\\|?*'


def parse_variants(text):
    """-> [{key, title, titles_txt}] in file order."""
    out = []
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        cols = [c.strip() for c in line.split("|")]
        if len(cols) not in (4, 5) or not cols[1].isdigit():
            raise ValueError("variants line %d: expected 'section | n | title | subtitle [| key=value ...]': %r"
                             % (n, raw))
        section, num, title, sub = cols[:4]
        extra = cols[4].split() if len(cols) == 5 else []
        block = ["[%s]" % title, "marker: %s_v%s" % (section, num), "subtitle: %s" % sub.replace(" / ", "|")]
        block += ["%s: %s" % tuple(kv.split("=", 1)) for kv in extra]
        out.append({"key": "%s_v%s" % (section, num), "title": title, "titles_txt": "\n".join(block)})
    keys = [v["key"] for v in out]
    dup = sorted({k for k in keys if keys.count(k) > 1})
    if dup:
        raise ValueError("variants: the same section and number twice: %s" % ", ".join(dup))
    return out


def file_stem(v):
    title = "".join(ch for ch in v["title"] if ch not in BAD_CHARS and ord(ch) >= 32)
    return "%s - %s" % (v["key"], re.sub(r"\s+", " ", title).strip(" ."))


def frame_of(resolve):
    """(width, height, fps) of the open project's current timeline, or None."""
    p = resolve.GetProjectManager().GetCurrentProject()
    tl = p and p.GetCurrentTimeline()
    if tl is None:
        return None
    return (int(tl.GetSetting("timelineResolutionWidth")), int(tl.GetSetting("timelineResolutionHeight")),
            float(tl.GetSetting("timelineFrameRate")))


def switch_project(resolve, name):
    """Save the open project (when it is a saved one: saving the default untitled project asks where, in a dialog)
    and open `name`. -> the project left open before."""
    pm = resolve.GetProjectManager()
    cur = pm.GetCurrentProject()
    old = cur.GetName() if cur else None
    if old == name:
        return old
    if old and old in (pm.GetProjectListInCurrentFolder() or []):
        if not pm.SaveProject():
            raise RuntimeError("could not save %r" % old)
        print("bake_variants: saved %r" % old)
    if not pm.LoadProject(name):
        raise RuntimeError("could not open the project %r" % name)
    print("bake_variants: opened %r" % name)
    return old


def bake(resolve, variants, out_dir, size):
    """Make, render and remove the variants' timelines in the open project. -> [(variant, path, rendered)]."""
    w, h, fps = size
    project = resolve.GetProjectManager().GetCurrentProject()
    mp = project.GetMediaPool()
    root = mp.GetRootFolder()
    had_top = any(f.GetName() == at.BIN_NAME for f in root.GetSubFolderList())
    folder = at.find_bin(mp, BIN)
    prev = project.GetCurrentTimeline()
    made, todo, result = [], [], []
    for n in [BIN + " - " + v["key"] for v in variants] + [REF]:      # left by an interrupted run
        old = at.find_timeline(project, n)
        if old is not None:
            mp.DeleteTimelines([old])
    try:
        mp.SetCurrentFolder(folder)
        ref = mp.CreateEmptyTimeline(REF)
        if ref is None:
            raise RuntimeError("could not create the timeline %r" % REF)
        made.append(ref)
        ref.SetSetting("useCustomSettings", "1")
        for k, val in (("timelineFrameRate", "%g" % fps), ("timelineResolutionWidth", str(w)),
                       ("timelineResolutionHeight", str(h))):
            ref.SetSetting(k, val)
        project.SetCurrentTimeline(ref)
        for v in variants:
            _, (sec,) = at.parse_titles(v["titles_txt"])
            frames = max(1, round(float(sec["seconds"]) * fps))
            path = os.path.join(out_dir, file_stem(v) + ".mov")
            if os.path.isfile(path):
                result.append((v, path, False))
                print("  %-16s kept      %s" % (v["key"], os.path.basename(path)))
                continue
            st = at.make_section_timeline(project, mp, folder, BIN + " - " + v["key"], ref, at.TEMPLATE, frames)
            made.append(st)
            inner = at.video_items(st, 1)
            tool = at.title_tool(inner[0]) if inner else None
            if tool is None:
                raise RuntimeError("the %r title has no Corner input: reinstall it (install_titles.py)" % at.TEMPLATE)
            at.set_title(tool, sec, fps, frames, inner[0])
            tool = at.title_tool(inner[0])       # set_title re-imports the comp
            todo.append({"st": st, "frames": frames, "out_dir": out_dir, "stem": file_stem(v), "tool": tool,
                         "path": path})
            result.append((v, path, True))
            print("  %-16s to bake   %s (%d frames)" % (v["key"], os.path.basename(path), frames))
        if todo:
            at.render_bakes(resolve, project, ref, todo)
    finally:
        if prev is not None:
            project.SetCurrentTimeline(prev)
        if made and not mp.DeleteTimelines(made):
            print("bake_variants: could not remove the variants' timelines (bin %s/%s)" % (at.BIN_NAME, BIN))
        mp.SetCurrentFolder(root)
        mp.DeleteFolders([folder])
        top = next((f for f in root.GetSubFolderList() if f.GetName() == at.BIN_NAME), None)
        if top is not None and not had_top and not top.GetClipList() and not top.GetSubFolderList():
            mp.DeleteFolders([top])
    return result


def ff_text(s):
    """A drawtext value: quoted, with the filter syntax's specials escaped."""
    return "'" + s.replace("\\", "/").replace(":", "\\:").replace("'", "") + "'"


FONT = ff_text(os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts", "arialbd.ttf"))


def ffmpeg(args, what):
    t0 = time.time()
    p = subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y"] + args, capture_output=True, text=True)
    if p.returncode:
        print("bake_variants: %s failed: %s" % (what, p.stderr.strip()[-800:]))
        return False
    print("bake_variants: %s in %.0f s" % (what, time.time() - t0))
    return True


def contact_sheet(clips, out, still, columns, size):
    """One frame `still` seconds in of each clip over grey, a third of the frame, labelled; `columns` a row."""
    w, h = (size[0] // 3) & ~1, (size[1] // 3) & ~1
    args, graph = [], []
    for i, (key, path) in enumerate(clips):
        args += ["-ss", "%g" % still, "-i", path]
        graph.append("color=c=%s:s=%dx%d:r=1[b%d];[%d:v]trim=end_frame=1,setpts=PTS-STARTPTS[f%d];"
                     "[b%d][f%d]overlay=shortest=1,scale=%d:%d,drawtext=fontfile=%s:text=%s:x=14:y=h-th-12:"
                     "fontsize=%d:fontcolor=white:box=1:boxcolor=black@0.6:boxborderw=6,setsar=1[t%d]"
                     % (GREY, size[0], size[1], i, i, i, i, i, w, h, FONT, ff_text(key), max(14, h // 14), i))
    rows = -(-len(clips) // columns)
    graph.append("%sconcat=n=%d:v=1:a=0,tile=%dx%d:padding=4:color=black[s]"
                 % ("".join("[t%d]" % i for i in range(len(clips))), len(clips), columns, rows))
    return ffmpeg(args + ["-filter_complex", ";".join(graph), "-map", "[s]", "-frames:v", "1", out], "contact sheet")


def reel(clips, out, fps):
    """Every clip over grey at 1080p, one after another, labelled bottom-left."""
    args, graph = [], []
    for i, (key, path) in enumerate(clips):
        args += ["-i", path]
        graph.append("color=c=%s:s=1920x1080:r=%g[b%d];[%d:v]scale=1920:1080,setpts=PTS-STARTPTS[f%d];"
                     "[b%d][f%d]overlay=shortest=1,drawtext=fontfile=%s:text=%s:x=24:y=h-th-20:fontsize=30:"
                     "fontcolor=white@0.85,format=yuv420p,setsar=1[v%d]"
                     % (GREY, fps, i, i, i, i, i, FONT, ff_text(key), i))
    graph.append("%sconcat=n=%d:v=1:a=0[r]" % ("".join("[v%d]" % i for i in range(len(clips))), len(clips)))
    return ffmpeg(args + ["-filter_complex", ";".join(graph), "-map", "[r]", "-c:v", "libx264", "-preset", "medium",
                          "-crf", "18", "-r", "%g" % fps, "-movflags", "+faststart", out], "reel")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("variants")
    ap.add_argument("--out", required=True, help="the folder for the clips, the contact sheet and the reel")
    ap.add_argument("--project", help="bake in this project (the open one is saved first and reopened at the end)")
    ap.add_argument("--size", help="WIDTHxHEIGHT (default: the open timeline's, else 1920x1080)")
    ap.add_argument("--fps", type=float, help="frame rate (default: the open timeline's, else 60)")
    ap.add_argument("--only", help="only the variants whose section_vN matches this regex")
    ap.add_argument("--still", type=float, default=4.0, help="the contact sheet's frame, seconds in (default 4)")
    ap.add_argument("--columns", type=int, default=3, help="the contact sheet's columns (default 3)")
    ap.add_argument("--no-sheet", action="store_true")
    ap.add_argument("--no-reel", action="store_true")
    ap.add_argument("--check", action="store_true", help="only parse the variants file and print the file names")
    a = ap.parse_args()
    with open(a.variants, encoding="utf-8") as f:
        variants = parse_variants(f.read())
    if a.only:
        variants = [v for v in variants if re.search(a.only, v["key"])]
    if a.check:
        for v in variants:
            _, (sec,) = at.parse_titles(v["titles_txt"])
            print("%s.mov\n  %s" % (file_stem(v), sec["subtitle_text"].replace("\n", "\n  ")))
        print("bake_variants: %d variants" % len(variants))
        return
    r = at.get_resolve()
    if r is None:
        sys.exit("bake_variants: Resolve not reachable (running? Preferences > System > General > "
                 "External scripting using: Local)")
    t0 = time.time()
    size = frame_of(r) or (1920, 1080, 60.0)
    if a.size:
        size = tuple(int(x) for x in a.size.lower().split("x")) + (size[2],)
    if a.fps:
        size = size[:2] + (a.fps,)
    print("bake_variants: %d variants at %dx%d %g fps into %s" % ((len(variants),) + size + (a.out,)))
    os.makedirs(a.out, exist_ok=True)
    back = switch_project(r, a.project) if a.project else None
    try:
        result = bake(r, variants, a.out, size)
    finally:
        if a.project:
            pm = r.GetProjectManager()
            pm.SaveProject()
            if back and back != a.project:
                ok = pm.LoadProject(back)
                print("bake_variants: %s %r" % ("reopened" if ok else "could not reopen", back))
    print("bake_variants: %d baked, %d kept, %.0f s" % (sum(1 for x in result if x[2]),
                                                       sum(1 for x in result if not x[2]), time.time() - t0))
    clips = [(v["key"], p) for v, p, _ in result if os.path.isfile(p)]
    if clips and shutil.which("ffmpeg"):
        if not a.no_sheet:
            contact_sheet(clips, os.path.join(a.out, "contact sheet.png"), a.still, a.columns, size)
        if not a.no_reel:
            reel(clips, os.path.join(a.out, "all variants.mp4"), size[2])
    elif clips:
        print("bake_variants: no ffmpeg on the PATH: no contact sheet or reel")


if __name__ == "__main__":
    main()
