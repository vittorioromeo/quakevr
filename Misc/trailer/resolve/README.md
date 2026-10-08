# Pulse on markers (DaVinci Resolve script)

`pulse_on_markers.py` makes a Fusion clip (the logo overlay) punch on the beat markers you've placed on the timeline.
For each marker of one colour (Blue by default), it keys a Transform node called **Pulse**, put between the clip's
MediaIn and whatever MediaIn fed:

- Size is 1.0 on the beat, 1.06 two frames later, and back to 1.0 ten frames after the beat.
- If the next beat comes sooner, the pulse is cut short and never overlaps the next one.
- Optionally, a SoftGlow (**PulseGlow**) flares its Gain in the same rhythm.

Run it again after moving markers: Pulse and PulseGlow are rebuilt from scratch, so the old keys disappear.

Written for DaVinci Resolve 21.1 (installed here: 21.1.0.17). It is Python 3, which Resolve 21.1 runs from the
Scripts menu in the free version as well, so no Lua version is needed.

## Install (once)

1. Close DaVinci Resolve.
2. In File Explorer, paste this into the address bar and press Enter:
   `%APPDATA%\Blackmagic Design\DaVinci Resolve\Support\Fusion\Scripts`
3. If there's no `Edit` folder there, create it (New folder, name it `Edit`).
4. Copy `pulse_on_markers.py` into that `Edit` folder.
5. Start Resolve. The script is now under **Workspace > Scripts** on the Edit page. To use it from every page, put it
   in a `Utility` folder next to `Edit` instead.

## Use

1. Put the logo clip on a video track. It needs a Fusion composition: open it once in the Fusion page; the script
   adds one if it has none.
2. Mark the beats with **Blue** timeline markers: select the timeline (not the clip), then press M on each beat.
3. Put the playhead over the logo clip, on the Edit page.
4. Run **Workspace > Scripts > pulse_on_markers**.
5. Open **Workspace > Console** to read its summary, for example:
   `pulse_on_markers: 12 Blue markers -> 12 beats on 'logo' (comp frames 30..410), 36 Size keys.`
6. To undo it: in the Fusion page, press Ctrl+Z, or delete the Pulse node.

## Settings (at the top of the script; edit with any text editor)

| Setting | Default | What |
|---------|---------|------|
| `MARKER_COLOR` | `"Blue"` | which markers count (`None`: all of them) |
| `CLIP_NAME` | `None` | the clip to animate by name (`None`: the clip under the playhead) |
| `PEAK_SIZE` | `1.06` | how big the punch is |
| `ATTACK`, `DECAY` | `2`, `10` | frames from the beat to the peak, and back to rest |
| `PULSE_NODE` | `"Pulse"` | the Transform's name |
| `GLOW`, `GLOW_NODE`, `GLOW_PEAK` | `False`, `"PulseGlow"`, `0.6` | the optional glow flare |
| `MARKERS_RELATIVE` | `True` | timeline marker frames count from the timeline's start (Resolve's usual) |

## How beats become comp frames

A timeline marker's key is its offset from the timeline's start, so it sits at timeline frame `GetStartFrame() + key`.
The clip starts at `item.GetStart()`, and its Fusion comp renders from `COMPN_RenderStart` (the trimmed-off head). So
the comp frame is `timeline frame - item.GetStart() + COMPN_RenderStart`. Markers outside the clip are skipped.

## Checked here (Resolve can't run headless)

- `python Misc/trailer/resolve/test_pulse_on_markers.py` runs it against a mock of the API. It checks the colour
  filter, the marker-to-comp-frame mapping, the keys (including beats too close together), the node chain
  MediaIn1 -> Pulse -> PulseGlow -> MediaOut1, and a second run rebuilding the nodes cleanly.
- The Resolve calls match the local API reference
  (`C:\ProgramData\Blackmagic Design\DaVinci Resolve\Support\Developer\Scripting\DaVinciResolveScript.pyi`):
  `GetProjectManager`, `GetCurrentProject`, `GetCurrentTimeline`, `GetMarkers`, `GetStartFrame`,
  `GetCurrentVideoItem`, `GetTrackCount("video")`, `GetItemListInTrack`, `GetStart`, `GetEnd`, `GetLeftOffset`,
  `GetFusionCompCount`, `GetFusionCompByIndex`, `AddFusionComp`, `scriptapp`.
- **Not confirmed locally:** the Fusion composition calls. The local reference leaves `FusionComp` opaque, and the
  Fusion Scripting Guide isn't installed. They are the standard Fusion API: `FindTool`, `GetToolList`, `AddTool`,
  `SetAttrs({"TOOLS_Name"})`, `GetAttrs()["COMPN_RenderStart"]`, `tool.Input = other.Output`,
  `Output.GetConnectedInputs`, `Input.GetConnectedOutput`, `Input.ConnectTo`, `Tool.Delete`,
  `AddModifier(input, "BezierSpline")`, `SetInput(input, value, frame)`, `Lock`, `Unlock`, `StartUndo`, `EndUndo` and
  `CurrentFrame.FlowView.GetPos`.
- **Easing:** the keys use a BezierSpline's default smooth handles; the script doesn't set custom handles.
- If a call misbehaves, the Console shows the error. The likeliest spots are node wiring on unusual comps, and the
  `COMPN_RenderStart` offset on trimmed clips (check one beat by eye).
