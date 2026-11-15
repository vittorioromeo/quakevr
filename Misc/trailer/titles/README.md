# Feature titles for the trailer (DaVinci Resolve)

Each section of the 1.1 feature trailer opens with a corner title: a big **title** (Pirata One, bone white with a
dark outline and a faint blood-red under-glow), a grimy red rule, and a small tracked **subtitle** (Cinzel) over a soft
dark scrim. In: a blood-red ink burn-in from the corner's side (noise-edged reveal, red ink leading the bone text,
blur and smear settling, a dying-torch flicker, a few drips running down), the subtitle a few frames later. Out: the
text burns back into the corner through red embers, blurring away. All procedural (Fusion noise, masks, blurs,
expressions): change the text or the corner and the animation follows; nothing is keyframed.

| File | What |
|------|------|
| `QVR Feature Title.setting` | the Fusion title (a macro): Effects > Titles > Fusion Titles > **QVR Feature Title** |
| `install_titles.py` | installs it, the fonts (current user, no admin) and a Workspace > Scripts launcher |
| `titles.txt` | the sections: title, subtitle, corner, length |
| `apply_titles.py` | puts / updates a title at every section's marker on the current timeline |
| `fonts/` | Pirata One, Cinzel, IM Fell English (alternative subtitle face), with their SIL OFL licences |

Written and checked against DaVinci Resolve Studio 21.1.

## Once

1. `python Misc/trailer/titles/install_titles.py` (Windows). Restart Resolve after the first install (new fonts and
   templates are read at start-up).
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

## Tweaking the look

Open a section's timeline ("QVR - ..." in the bin, or double-click the nested clip), select the title and use the
Inspector: Title, Subtitle, Corner, **Accent** (the blood colour), **Intensity** (glow, flicker, smear, drips), In / Out
(frames), Hold (frames; 0 = until the clip's end; apply_titles sets it from `seconds:`), Subtitle Delay, Scrim, Margin
X / Y (safe area, fractions of the frame), Title / Subtitle size, font and colour, Subtitle Tracking. A change made
there by hand is overwritten for the fields apply_titles sets (text, corner, hold, and in/out/intensity when given in
titles.txt) on its next run; the rest stays. To change the default look for every title, edit the inputs' values in
`QVR Feature Title.setting` (or open a title in the Fusion page, adjust the nodes inside the macro), reinstall, and
`--rebuild`. Sizes and margins are fractions of the frame, so the same title works at 1080p and 4K.

## With takes and OBS

The game's takes (docs/vr-port/TRAILER.md, "Takes and clips") end a session with `timeline.fcpxml` of the keep
takes, and `rough_cut.py` builds a cut from highlight logs. Import either into Resolve (File > Import > Timeline), cut
the trailer, then mark the section starts and run apply_titles: the titles live on their own track above the
footage, so re-importing or re-cutting the footage only needs the markers moved and apply_titles run again.

## Preview

Checked by rendering a two-title sample (top-right and bottom-left, 4 s each, 1080p60, over a dark noise background)
through Resolve's scripting: the in (burn-in, ink, drips, blur settle), hold and out frames, the subtitle wrapping and
the bottom corners' layout (the rule and title sit above however many subtitle lines there are). Not checked: a 4K
timeline render, and the IM Fell English face in place of Cinzel.
