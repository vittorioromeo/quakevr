# Feature titles for the trailer (DaVinci Resolve)

Each section of the 1.1 feature trailer opens with a corner title: a big **title** in the Quake font (DpQuake),
embossed **metal** (bronze/gunmetal with grime in the cavities and rust patches, a dark outline and drop shadow, a
faint blood-red under-glow), a grimy red rule, and a small tracked **subtitle** (Cinzel, with its own soft drop shadow)
over a soft dark scrim. In: a
blood-red ink burn-in from the corner's side (noise-edged reveal, red ink leading the metal, blur and smear settling, a
dying-torch flicker) while a **light sweeps across the letters** (left to right by default), a moving specular
highlight on the bevels; the subtitle a few frames later. **Blood** lies *behind* the letters (they stay clean and
readable): it pools out under the strokes' lower edges, seeps around them in patches, and **drips** run down from them,
lengthening through the hold; it is wet (its own relief, sharp glints when the light passes, darker
on the shadowed side, a slow glisten). Out: the text burns back into the corner through red embers, blurring away.
All procedural (Fusion noise, masks, blurs, expressions, Custom tools): change the text or the corner and the
animation follows; nothing is keyframed.

| File | What |
|------|------|
| `QVR Feature Title.setting` | the Fusion title (a macro): Effects > Titles > Fusion Titles > **QVR Feature Title** |
| `QVR Intro Title.setting` | the full-screen intro title: Effects > Titles > Fusion Titles > **QVR Intro Title** (see "The intro title") |
| `make_intro_title.py` | generates `QVR Intro Title.setting` (its ~90 tools and expressions are kept as tables there) |
| `install_titles.py` | installs both, the fonts (current user, no admin) and a Workspace > Scripts launcher |
| `titles.txt` | the intro and the sections: title, subtitle, corner, length |
| `apply_titles.py` | puts / updates the intro and a title at every section's marker on the current timeline |
| `fonts/` | DpQuake (Dead Pete's Quake font; free to distribute, always with its `dpquake.txt`), Cinzel, Pirata One (the previous title face), IM Fell English (alternative subtitle face), the last three with their SIL OFL licences |

Written and checked against DaVinci Resolve Studio 21.1.

## Once

1. `python Misc/trailer/titles/install_titles.py` (Windows). Restart Resolve after the first install (new fonts and
   templates are read at start-up), and after an update that adds a font (the metal version added DpQuake); then
   `apply_titles.py --rebuild` so existing titles take the new template.
2. To run `apply_titles.py` from a terminal: Resolve's **Preferences > System > General > External scripting using:
   Local** (Studio). Otherwise run it inside Resolve: **Workspace > Scripts > QVR Apply Titles** (the launcher the
   installer put there runs this folder's `apply_titles.py` on this folder's `titles.txt`; the output is in
   Workspace > Console).

## Each trailer

1. **Mark the sections.** On the trailer's timeline, put a **timeline marker** (select the timeline, not a clip; M) on
   the first frame of each section, and name it like the section's `marker:` in titles.txt (or its title). The colour
   doesn't matter. That's the whole rule: the title starts at the marker.
2. **Edit `titles.txt`**: one `[Title]` block per section, with `subtitle:` (`|` breaks the line), and optionally
   `corner:` (top-right by default; top-left, bottom-left, bottom-right), `seconds:` (on screen, in and out included;
   6 by default), `subdelay:` (seconds the title is alone once it is in, before the subtitle comes in; 1 by default),
   `in:` / `out:` (seconds), `intensity:` (0-2). Lines before the first block set defaults. The default timing at
   60 fps: the title comes in over 0.67 s, stays alone 1 s, the subtitle comes in over 0.67 s (in by 2.33 s), both
   hold to 5.57 s, out over 0.43 s. A shorter `seconds:` shortens the hold (apply_titles says when the subtitle would
   be fully on screen for less than 1.5 s). Each title clip is made 10 s long (longer when `seconds:` asks for more),
   not Resolve's standard generator duration (5 s): titles made before need `--rebuild` for more than 5 s.
3. **Run** `python Misc/trailer/titles/apply_titles.py` (`--dry-run` first to see what it would do;
   `--check` only parses titles.txt). Each section gets a small timeline of its own holding one title,
   "QVR - <marker>", in the media pool bin *QVR Titles/<trailer timeline>*, placed (nested, video only) on the video
   track named **QVR Titles** (added on top the first time) at its marker.
4. **Change anything and run it again**: the text, corner and length are updated in those timelines in place (the
   nested copies follow at once); a moved marker moves its title; a section removed from titles.txt is taken off the
   track. Nothing is duplicated. (Timeline names are unique in a Resolve project, so two trailer timelines with a
   section of the same marker share its "QVR - <marker>" timeline: same text, the current trailer's frame size.)
   After changing the template itself (or reinstalling it), run with `--rebuild`:
   existing titles keep the copy of the macro they were made with.

Why nested timelines: Resolve's scripting can only *insert* a title at the playhead on the destination track (V1,
rippling everything after it), while it can *place* a media pool item on any track at any frame. A section's own
timeline is such an item, so the trailer's clips are never touched.

## The Q (and the other logo glyphs)

DpQuake draws **Q and q as the Quake logo** (and `~` and a few Alt codes as Quake 2 / Arena / id logos): those are
never shown. `apply_titles.py` draws every Q, q, `~` and non-ASCII character of a title in **Cinzel Bold** instead
(a Roman capital Q that sits well with the Quake face; lowercase q is its small-cap Q, as DpQuake's lowercase is small
caps), through the macro's TitleCLS character-styling modifier. Typing a title with a Q by hand in the Inspector skips
this: put it in titles.txt and run apply_titles. A title made with the previous template (no TitleCLS) says so; run
with `--rebuild`.

## The intro title

**QVR Intro Title** fills the frame, centred: "Quake VR" in DpQuake as lit **metal** (the same relief, grime and
light sweep as the feature titles, bigger), **UNLEASHED** below in Cinzel Black, widely tracked, made of **fresh
blood** (wet relief with glints, splashes and splatter, drips running down from the letters and pooling on the rule),
a grimy metal-and-blood **rule** slicing in from the centre, and **v1.1 Release Trailer** in Cinzel. The lines come in
one after another (at 60 fps: line 1 from frame 0, the blood from 34 with a jolt, the rule from 66, line 3 from 84,
all in by frame 118, 1.97 s), hold 3 s with everything on screen (the drips lengthen, the pool spreads), then
everything burns away into a red-glowing blur over 0.6 s: 5.57 s in all. Transparent
by default, with a soft dark scrim behind the text, so it sits over footage; **Dark Backdrop** puts a full-frame dark
background under it.

The Q: DpQuake's Q is the Quake logo, so a **leading Q** of line 1 is drawn as DpQuake's own O with a blade-like tail
built in the macro (it follows the font's size and weight), and "Quake VR" reads as one font. Other Q, q, `~` or
non-ASCII characters in line 1 would show logos: apply_titles refuses them.

In titles.txt, an `[Intro]` block (keys `line1:`, `line2:`, `line3:`, `hold:` (3 s), `seconds:` (by default the
lines' in + hold + out; when given, the out ends there and the hold is what is left), `out:`, `blood:` (0-2),
`intensity:`, `backdrop: on/off`, `marker:`) puts it at the timeline marker named **Intro**, or at the timeline's
start when there is none, as "QVR - Intro" on the same QVR Titles track. Inspector: Line 1-3, Blood, Blood Colour,
Metal Tint, Relief Depth, Light Sweep (gain, direction, start, length), Intensity, each line's and the rule's Start
and In (frames), Hold (frames from when every line is in; 0 = to the clip's end), Out, Out Start (0 = after the
hold), Dark Backdrop, Scrim, Shadow Opacity / Softness / Offset,
Vertical Offset, Rule Width, and each line's size, font and tracking. The timings are frames at 60 fps; on another
frame rate apply_titles scales them to keep their length in seconds (overwriting hand changes to them). To change the
default look, edit `make_intro_title.py`, run it, then `install_titles.py` and `apply_titles.py --rebuild`.

## Tweaking the look

Open a section's timeline ("QVR - ..." in the bin, or double-click the nested clip), select the title and use the
Inspector: Title, Subtitle, Corner, **Accent** (the blood colour), **Intensity** (ink glow, flicker, smear),
**Blood** (0-2: coat, drip count and length; 0 = none), **Metal Tint** (the metal's colour: bronze by default; greyer
for gunmetal, redder and darker for rusted iron), **Relief Depth** (0-2: bevel height and width), **Light Sweep**
(0-2: the sweeping light's brightness; 0 = only the fixed top-left key light), **Sweep Direction** (left to right /
right to left), **Sweep Start** and **Sweep Length** (frames from the title's first frame), In / Out (frames), Hold
(frames; 0 = until the clip's end; apply_titles sets it from `seconds:`), Subtitle Delay (frames after the title
is in; apply_titles sets it from `subdelay:`), Scrim, **Shadow Opacity**
(0-1, 0.8; 0 = no drop shadows), **Shadow Softness** (blur, 9 px at 1080p) and **Shadow Offset** (down-right, 6 px at
1080p) for the title's and the subtitle's drop shadows (the subtitle's uses half the softness and offset, as its text
is smaller; both scale with the frame and follow the in/out blur), Margin X / Y (safe
area, fractions of the frame), Title size, font and tracking, Subtitle size, font and tracking. Lighting and blood
follow the title's own extent, so any text length works. Sizes measured on an image (the text's crop) are real pixels;
built-in tools (Crop, Background, blurs) take full-resolution pixels and apply Fusion's proxy themselves, so the
macros divide by the proxy scale `K` that a 1024-wide `ProxyProbe` Background measures (Custom tools use real pixels).
Keep that rule when adding a pixel-sized input. A change made
there by hand is overwritten for the fields apply_titles sets (text, corner, hold, and in/out/intensity when given in
titles.txt) on its next run; the rest stays. To change the default look for every title, edit the inputs' values in
`QVR Feature Title.setting` (or open a title in the Fusion page, adjust the nodes inside the macro), reinstall, and
`--rebuild`. Sizes and margins are fractions of the frame, so the same title works at 1080p and 4K.

## With takes and OBS

The game's takes (docs/vr-port/TRAILER.md, "Takes and clips") end a session with `timeline.fcpxml` of the keep
takes, and `rough_cut.py` builds a cut from highlight logs. Import either into Resolve (File > Import > Timeline), cut
the trailer, then mark the section starts and run apply_titles: the titles live on their own track above the
footage, so re-importing or re-cutting the footage only needs the markers moved and apply_titles run again.

Cost on the author's machine (titlespeed, 2026-11-16): a feature title about 0.13 s a frame at 1080p and 0.21 s at
4K, the intro about 0.48 s and 1.9 s (before: 0.23 / 0.66 and 0.8 / 4.9). The Custom tools (per-pixel interpreted,
on the CPU) are the cost: the lighting and blood run on crops around the text, and each colour and lighting tool
works only where its result can show (lossless lazy `if` gates). For real-time playback see "Fast previews".

## Fast previews

- **Draft (fast preview)** (a checkbox on both titles; `apply_titles.py --draft on|off` sets it on every title): flat metal and blood (no relief shading, light sweep or glints); the text, layout, timing, reveal, blood shapes and drips are as in the final. A modest saving (about 15% at 4K, little at 1080p, since the lighting now costs little anyway): bake for real time. Fusion's own proxy/HiQ switches don't reach titles on the Edit page, so Draft is yours to switch off before delivering (`--draft off`); `--bake` always renders at full quality.
- **`apply_titles.py --bake`** renders each title once to a ProRes 4444 clip with alpha (in *QVR Titles (baked)* next to the trailer's media, or `--bake-dir`) and places it on the QVR Titles track instead of the live title: real-time playback. Always full quality (Draft off). The clip's name carries a digest of the title's comp, frame size and length: unchanged titles reuse their clip without rendering; a plain run keeps baked titles baked until one changes (then it goes back live: `--bake` again). All the titles to render go to the render queue as one batch (only these jobs are taken off it afterwards); the Edit page stays usable but don't edit while it renders. Expect about the live cost per frame: the intro ~0.45 s a frame at 1080p (2.5 min for 5.57 s), a feature title ~0.15 s (1 min for 6 s). A title made with an older template (before the Draft toggle) renders much slower: apply_titles says so; `--rebuild` first.
- **`--unbake`** (or `--rebuild`) puts the live titles back; the section timelines were never touched, so nothing is lost.
- **Resolve's own**: right-click the titles track's clips > **Render Cache Fusion Output > On** (or Playback > Render Cache > **Smart**): each title renders once in the background, then plays from the cache until changed.
- **Playback > Timeline Proxy Resolution > Half / Quarter** plays the timeline at a lower resolution while editing; the delivered render is unaffected. Both titles follow Fusion's proxy scale (introfix, 2026-11-16: before, their crops, canvases and blurs were scaled down twice under a proxy, so the preview showed a fraction of the letters and the metal and blood misplaced). Don't expect it to make the titles much faster: at 1080p and below their cost is per frame, not per pixel (the intro ~0.45 s a frame at 1080p, 540p and 270p alike); it helps at 4K. Bake (or the render cache) for real-time playback.

## Preview

Blood behind the letters and the drop shadows (trailertitles3): the same two-title sample re-rendered; the shadows
measured against the previous render (pixels darkened below and right of the subtitle and title, none lightened).

Checked by rendering a two-title sample (top-right and bottom-left, 4 s each, 1080p60, over a dark noise background)
through Resolve's scripting: the in (burn-in, ink, light sweep), hold (drips lengthened) and out frames, the subtitle
wrapping and the bottom corners' layout; a 4K render of a section timeline (same layout, relief and blood scale with
the frame); a "Quad Damage" title (Q and q in Cinzel Bold, no logo). Not checked: the IM Fell English face in place of
Cinzel.
