# Feature titles for the trailer (DaVinci Resolve)

Each section of the 1.1 feature trailer opens with a corner title: a big **title** in the Quake font (DpQuake),
embossed **metal** (bronze/gunmetal with grime in the cavities and rust patches, a dark outline and drop shadow, a
faint blood-red under-glow), a grimy red rule, and a small tracked **subtitle** (Cinzel) over a soft dark scrim. In: a
blood-red ink burn-in from the corner's side (noise-edged reveal, red ink leading the metal, blur and smear settling, a
dying-torch flicker) while a **light sweeps across the letters** (left to right by default), a moving specular
highlight on the bevels; the subtitle a few frames later. **Blood** coats the lower edges of the strokes and **drips**
run down from them, lengthening through the hold; it is wet (its own relief, sharp glints when the light passes, darker
on the shadowed side, a slow glisten). Out: the text burns back into the corner through red embers, blurring away.
All procedural (Fusion noise, masks, blurs, expressions, Custom tools): change the text or the corner and the
animation follows; nothing is keyframed.

| File | What |
|------|------|
| `QVR Feature Title.setting` | the Fusion title (a macro): Effects > Titles > Fusion Titles > **QVR Feature Title** |
| `install_titles.py` | installs it, the fonts (current user, no admin) and a Workspace > Scripts launcher |
| `titles.txt` | the sections: title, subtitle, corner, length |
| `apply_titles.py` | puts / updates a title at every section's marker on the current timeline |
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
   4.5 by default, at most the title clip's length, Resolve's standard generator duration: 5 s unless changed),
   `in:` / `out:` (seconds), `intensity:` (0-2). Lines before the first block set defaults.
3. **Run** `python Misc/trailer/titles/apply_titles.py` (`--dry-run` first to see what it would do;
   `--check` only parses titles.txt). Each section gets a small timeline of its own holding one title,
   "QVR - <marker>", in the media pool bin *QVR Titles/<trailer timeline>*, placed (nested, video only) on the video
   track named **QVR Titles** (added on top the first time) at its marker.
4. **Change anything and run it again**: the text, corner and length are updated in those timelines in place (the
   nested copies follow at once); a moved marker moves its title; a section removed from titles.txt is taken off the
   track. Nothing is duplicated. After changing the template itself (or reinstalling it), run with `--rebuild`:
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

## Tweaking the look

Open a section's timeline ("QVR - ..." in the bin, or double-click the nested clip), select the title and use the
Inspector: Title, Subtitle, Corner, **Accent** (the blood colour), **Intensity** (ink glow, flicker, smear),
**Blood** (0-2: coat, drip count and length; 0 = none), **Metal Tint** (the metal's colour: bronze by default; greyer
for gunmetal, redder and darker for rusted iron), **Relief Depth** (0-2: bevel height and width), **Light Sweep**
(0-2: the sweeping light's brightness; 0 = only the fixed top-left key light), **Sweep Direction** (left to right /
right to left), **Sweep Start** and **Sweep Length** (frames from the title's first frame), In / Out (frames), Hold
(frames; 0 = until the clip's end; apply_titles sets it from `seconds:`), Subtitle Delay, Scrim, Margin X / Y (safe
area, fractions of the frame), Title size, font and tracking, Subtitle size, font and tracking. Lighting and blood
follow the title's own extent, so any text length works. A change made
there by hand is overwritten for the fields apply_titles sets (text, corner, hold, and in/out/intensity when given in
titles.txt) on its next run; the rest stays. To change the default look for every title, edit the inputs' values in
`QVR Feature Title.setting` (or open a title in the Fusion page, adjust the nodes inside the macro), reinstall, and
`--rebuild`. Sizes and margins are fractions of the frame, so the same title works at 1080p and 4K.

## With takes and OBS

The game's takes (docs/vr-port/TRAILER.md, "Takes and clips") end a session with `timeline.fcpxml` of the keep
takes, and `rough_cut.py` builds a cut from highlight logs. Import either into Resolve (File > Import > Timeline), cut
the trailer, then mark the section starts and run apply_titles: the titles live on their own track above the
footage, so re-importing or re-cutting the footage only needs the markers moved and apply_titles run again.

Cost: about 0.25 s a frame at 1080p on the author's machine (the lighting and blood run only on a crop around the
title); a 4.5 s title renders in about a minute. Resolve's render cache (Playback > Render Cache > Smart) makes
playback smooth after the first pass.

## Preview

Checked by rendering a two-title sample (top-right and bottom-left, 4 s each, 1080p60, over a dark noise background)
through Resolve's scripting: the in (burn-in, ink, light sweep), hold (drips lengthened) and out frames, the subtitle
wrapping and the bottom corners' layout; a 4K render of a section timeline (same layout, relief and blood scale with
the frame); a "Quad Damage" title (Q and q in Cinzel Bold, no logo). Not checked: the IM Fell English face in place of
Cinzel.
