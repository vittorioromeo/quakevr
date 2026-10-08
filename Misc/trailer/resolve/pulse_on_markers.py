# pulse_on_markers.py -- DaVinci Resolve (Workspace > Scripts): makes a Fusion clip pulse on the timeline's beat
# markers. For each marker of MARKER_COLOR on the current timeline it keys a Transform node ("Pulse", put between the
# clip's MediaIn and whatever it fed) so the picture punches up to PEAK_SIZE ATTACK frames after the beat and settles
# back to 1.0 DECAY frames after it; optionally a soft glow ("PulseGlow") flares with it.
#
# Run it again after moving markers: the Pulse (and PulseGlow) nodes are rebuilt from scratch each time, so the old
# keys go with them. Ctrl+Z in the Fusion page undoes a run.
#
# Install and use: Misc/trailer/resolve/README.md. Tested against a mock of the API (test_pulse_on_markers.py).

# --- Settings ---------------------------------------------------------------------------------------------------
MARKER_COLOR = "Blue"         # only markers of this colour (None: every marker)
CLIP_NAME = None              # the timeline clip to animate (None: the clip under the playhead on the video tracks)
PEAK_SIZE = 1.06              # Transform size at the peak (1.0: none)
ATTACK = 2                    # frames from the beat to the peak
DECAY = 10                    # frames from the beat back to 1.0
PULSE_NODE = "Pulse"          # the Transform node's name
GLOW = False                  # also flare a soft glow on each beat
GLOW_NODE = "PulseGlow"
GLOW_PEAK = 0.6               # the glow's Gain at the peak (0 between beats)
MARKERS_RELATIVE = True       # the timeline's marker frames count from the timeline's start (Resolve's usual)
# ------------------------------------------------------------------------------------------------------------------


def beat_frames(markers, color):
    """The marker frames (sorted) of the wanted colour. markers: {frame: {"color": ..., ...}} (Timeline.GetMarkers)."""
    out = []
    for f, info in (markers or {}).items():
        if color is None or (info or {}).get("color", "").lower() == color.lower():
            out.append(int(round(float(f))))
    return sorted(out)


def to_comp_frames(beats, timeline_start, item_start, item_end, comp_start, relative=True):
    """Timeline marker frames -> the clip's Fusion comp frames, keeping only the beats the clip covers.

    A marker at timeline frame F (timeline_start + its key when the keys are relative) falls on the clip's frame
    F - item_start; the comp's first rendered frame (COMPN_RenderStart: the clip's trimmed-off head) is the clip's
    first frame, so the comp frame is F - item_start + comp_start."""
    out = []
    for b in beats:
        F = timeline_start + b if relative else b
        if item_start <= F < item_end:
            out.append(F - item_start + comp_start)
    return out


def pulse_keys(comp_beats, base, peak, attack, decay):
    """{comp frame: value} for the pulses: base on the beat, peak `attack` frames on, base `decay` frames on. A
    pulse cut short by the next beat keeps the next beat's base key (no overlapping keys)."""
    keys = {}
    beats = sorted(comp_beats)
    for i, b in enumerate(beats):
        nxt = beats[i + 1] if i + 1 < len(beats) else None
        keys[b] = base
        if nxt is None or b + attack < nxt:
            keys[b + attack] = peak
        if nxt is None or b + decay < nxt:
            keys[b + decay] = base
    return dict(sorted(keys.items()))


# --- Resolve / Fusion ---------------------------------------------------------------------------------------------

def get_resolve():
    r = globals().get("resolve")
    if r is not None:
        return r
    try:
        import DaVinciResolveScript as bmd_mod          # an external run (Resolve Studio, external scripting on)
        return bmd_mod.scriptapp("Resolve")
    except ImportError:
        return None


def find_item(timeline, name):
    if name is None:
        return timeline.GetCurrentVideoItem()
    for t in range(1, timeline.GetTrackCount("video") + 1):
        for it in timeline.GetItemListInTrack("video", t) or []:
            if it.GetName() == name:
                return it
    return None


def connected_inputs(output):
    try:
        return list((output.GetConnectedInputs() or {}).values())
    except Exception:
        return []


def rebuild_node(comp, name, reg_id, after):
    """A fresh tool `name` (reg_id) wired after `after` (the old one, and its keys, removed first); whatever `after`
    fed now takes the new tool's output."""
    old = comp.FindTool(name)
    feeds = []
    if old is not None:
        feeds = [i for i in connected_inputs(old.Output)]
        up = old.Input.GetConnectedOutput()
        if up is not None:
            after = up.GetTool()
        old.Delete()
    else:
        feeds = connected_inputs(after.Output)
    flow = comp.CurrentFrame.FlowView if comp.CurrentFrame else None
    x, y = (flow.GetPos(after) if flow else (0, 0)) or (0, 0)
    tool = comp.AddTool(reg_id, x + 1, y)
    tool.SetAttrs({"TOOLS_Name": name})
    tool.Input = after.Output
    for inp in feeds:
        if inp.GetTool().Name != name:
            inp.ConnectTo(tool.Output)
    return tool


def animate(tool, comp, input_name, keys):
    tool.AddModifier(input_name, "BezierSpline")
    for f, v in keys.items():
        tool.SetInput(input_name, v, f)


def main():
    res = get_resolve()
    if res is None:
        print("pulse_on_markers: run it from DaVinci Resolve (Workspace > Scripts).")
        return
    project = res.GetProjectManager().GetCurrentProject()
    timeline = project.GetCurrentTimeline() if project else None
    if timeline is None:
        print("pulse_on_markers: no current timeline.")
        return
    beats = beat_frames(timeline.GetMarkers(), MARKER_COLOR)
    if not beats:
        print("pulse_on_markers: no %s markers on '%s'." % (MARKER_COLOR or "", timeline.GetName()))
        return
    item = find_item(timeline, CLIP_NAME)
    if item is None:
        print("pulse_on_markers: no clip (put the playhead over it, or set CLIP_NAME).")
        return
    comp = item.GetFusionCompByIndex(1) if item.GetFusionCompCount() > 0 else item.AddFusionComp()
    if comp is None:
        print("pulse_on_markers: '%s' has no Fusion composition." % item.GetName())
        return
    attrs = comp.GetAttrs() or {}
    comp_start = int(attrs.get("COMPN_RenderStart", item.GetLeftOffset()))
    cbeats = to_comp_frames(beats, int(timeline.GetStartFrame()), int(item.GetStart()), int(item.GetEnd()),
                            comp_start, MARKERS_RELATIVE)
    if not cbeats:
        print("pulse_on_markers: none of the %d markers falls on '%s'." % (len(beats), item.GetName()))
        return
    media_in = comp.FindTool("MediaIn1") or next(iter((comp.GetToolList(False, "MediaIn") or {}).values()), None)
    if media_in is None:
        print("pulse_on_markers: no MediaIn in '%s''s composition." % item.GetName())
        return
    comp.Lock()
    comp.StartUndo("Pulse on markers")
    try:
        pulse = rebuild_node(comp, PULSE_NODE, "Transform", media_in)
        size_keys = pulse_keys(cbeats, 1.0, PEAK_SIZE, ATTACK, DECAY)
        animate(pulse, comp, "Size", size_keys)
        glow_keys = {}
        if GLOW:
            glow = rebuild_node(comp, GLOW_NODE, "SoftGlow", pulse)
            glow_keys = pulse_keys(cbeats, 0.0, GLOW_PEAK, ATTACK, DECAY)
            animate(glow, comp, "Gain", glow_keys)
    finally:
        comp.EndUndo(True)
        comp.Unlock()
    print("pulse_on_markers: %d %s markers -> %d beats on '%s' (comp frames %d..%d), %d Size keys%s." % (
        len(beats), MARKER_COLOR or "", len(cbeats), item.GetName(), cbeats[0], cbeats[-1], len(size_keys),
        (", %d glow keys" % len(glow_keys)) if GLOW else ""))


if __name__ == "__main__" or "resolve" in globals():     # the Scripts menu (or a console run), not the tests
    main()
