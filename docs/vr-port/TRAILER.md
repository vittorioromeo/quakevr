# Trailer tools: highlight markers and a rough cut

Two tools to save editing time on gameplay footage:

1. **Highlight markers** (in the game): while you record, the game logs its cool moments with their times, and
   writes markers DaVinci Resolve imports onto the timeline.
2. **Rough cut** (`Misc/quakevr/trailer/`): from the recordings, their logs and a music track, a first cut of the
   trailer (the best moments, trimmed, built up, cut on the beat) as a timeline Resolve imports over the original
   files (nothing re-encoded).

## 1. Highlight markers

**Graphics > Recording > Highlight Markers > Log Highlights** (or `vr_highlights_start` / `vr_highlights_stop`, or
`vr_highlights 1` / `0`; bindable). Not saved: every session starts with it off.

The order that matters: **start recording first (OBS), then turn Log Highlights on.** Turning it on makes the **sync
mark**: a white flash over the whole desktop window for 0.2 s (`vr_highlights_flash`, seconds; never in the headset)
and a 1 kHz beep (`vr_highlights_beep`, its volume). The log's times count from it, so the flash's frame in the video
is where the log's time 0 is. Started recording late? **Sync Mark Now** (`vr_highlights_sync`) makes another one
(logged as a `sync` row at its time; the scripts' `--nth 2` finds the second flash).

Times are **real seconds** (the engine's `realtime`), so they stay right in slow motion and bullet time: they match a
recording made at real speed. Each row also has the game's own clock (`game_time`, slowed in slow motion).

### What is logged

| Kind | When | Score |
|---|---|---|
| `kill` | you kill a monster (or a barrel you blew up does): tagged `headshot`, `melee`, `blast`; the weapon (`thrown Axe`, `rocket`, `Shotgun`, `headbutt`, ...) | 1, +1.5 headshot, +1 melee, +2 headbutt, +3 thrown weapon, +1 ogre/fiend/vore/death knight, +2 shambler |
| `gib` | a kill that gibbed it | the kill's +2 |
| `multikill` | kills no more than `vr_highlights_multikill` (3) game seconds apart, 2 or more: one row spanning them | 2 a kill |
| `corpsegib` | a corpse blown apart | 1.5 |
| `parry` | a blow parried (weapon and hand, or crossed arms) | 3 |
| `shoveparry` | an enemy's shove parried | 4 |
| `counter` | a counter-attack landing after a parry | 4 |
| `bullettime` | bullet time, one row as long as it ran | 4 |
| `explosion` | a blast that killed 2 or more | 2 + 1.5 a kill |
| `barrel` | an explosive box blown up | 2 (3 if it killed) |
| `chain` | a grenade set off by a blast, or a box blown up by another blast | 4 |
| `shotgrenade` | a grenade set off by your shot or blow | 4, 6 in flight |
| `axestick` | a thrown axe stuck in a monster | 3 |
| `grapplepull` | the grappling hook bites a monster and reels it in | 3 |
| `swing` | swinging on the hook (off the floor, over 250 u/s) for 0.6 s or more: one row as long as it lasted | 2 + its seconds (at most 5) |
| `mark` | `vr_highlight_mark [score] [words]` (Mark This Moment): yours | 5 |
| `sync` | the sync mark | 0 |

`vr_debug_highlights 1` (Debug > Logging > Highlights) prints each row as it is logged.

### Files

In `quakevr/highlights/` (the game folder), named by the time the log started:

- `<date>_<time>.csv`: a row a moment, written as it happens (a crash keeps them): `t, game_time, kind, score,
  duration, count, map, subject, detail`. Bullet time, multi-kills and swings are written when they end (with their
  start's `t`), so the rows are not quite in order.
- `<date>_<time>.json`, when the log stops: the same rows, sorted, with the start's wall-clock time and frame rate.
- `<date>_<time>.edl`, when the log stops: Resolve's timeline markers for moments scoring at least
  `vr_highlights_min_score` (2: plain kills left out), at `vr_highlights_fps` (60), the sync mark at the timeline's
  start (01:00:00:00).

### Into Resolve: the markers

The EDL is the one Resolve itself writes for markers (Timelines > Export > Timeline Markers to EDL): each marker a
one-frame event with a `|C:ResolveColor<colour> |M:<name> |D:<frames>` line.

Easiest, the recording whole with its markers (any sync offset, found from the flash):

```
python Misc/quakevr/trailer/markers.py quakevr/highlights/<log>.csv --rec <recording>.mp4 --fcpxml <recording>.fcpxml
```

In Resolve: **File > Import > Timeline...**, pick the `.fcpxml` (leave "Automatically import source clips into media
pool" on): a timeline with the recording and every moment as a marker on the clip.

Or markers onto your own timeline (the recording placed whole at the timeline's start, 01:00:00:00):

1. `python Misc/quakevr/trailer/markers.py <log>.csv --rec <recording>.mp4` (finds the flash; or `--sync <seconds>`
   where you see it) writes `<log>.edl` with the recording's sync offset in the times. (The game's own `.edl` assumes
   the flash at the timeline's start: trim the clip's head to the flash frame and it lines up as it is.)
2. In Resolve's **Media Pool**, right-click the timeline > **Timelines > Import > Timeline Markers from EDL...**, pick
   the `.edl`. The timeline's frame rate must be the EDL's (`--fps`, default the recording's).

## 2. Rough cut

Python 3 with `numpy` and `av` (PyAV 18 is installed here: FFmpeg's libraries in a Python package, so any recording or music format
FFmpeg reads; no ffmpeg on PATH needed). Nothing else (no scipy or librosa: the beat tracker is plain numpy).

```
python Misc/quakevr/trailer/rough_cut.py --rec take1.mp4 --log take1.csv --rec take2.mkv --log take2.csv --sync 14.2
       --music track.mp3 --length 60 --out trailer.fcpxml
```

- `--rec`/`--log`/`--sync` go in pairs, by order; `--sync` is the sync mark's time in that recording (default `auto`:
  `sync_detect.py` finds the flash, exact to the frame, or the beep when there is no flash).
- Moments: the logs' rows scoring at least `--min-score` (2), `--kinds gib,parry,...` or `--exclude kill`; rows less
  than `--merge-gap` (1 s) apart make one moment (scored its best plus half the rest's).
- Clips: `--pre` (2 s) before the moment, `--post` (1.5 s) after it (or after its end), at most `--max-clip` (5 s),
  at least `--min-clip` (1 s).
- The best moments that fit `--length` (60 s) are taken, then ordered by `--order`: `hook` (default: the second best
  opens, the rest build up, the best last), `build` (weakest first), `chrono`, `score`.
- With `--music`: the tempo and beats are found (`--bpm` if you know it), and each cut is moved to the nearest beat
  (`--cut-every` 2 beats, or `--bars` for the bars' first beats), a clip growing at most `--max-stretch` (1.5 s) or
  shrinking (from both sides, keeping half a second before its moment) to reach it. The music runs under the clips
  from `--music-start`.
- Out: `--out` (FCPXML 1.8; `--version 1.9`), checked (`fcpxml.py check`): the clips end to end, inside their files,
  on the timeline's frame grid; markers inside their clips.

Into Resolve: **File > Import > Timeline...**, the `.fcpxml` ("Automatically import source clips into media pool"
on). The timeline: the clips on video and audio track 1 (the game's sound), the music on the audio track below,
every moment a marker on its clip. If Resolve asks for the media, point it at the recordings' folder (the FCPXML
refers to them by their full paths: keep them where they were).

### The other scripts

- `sync_detect.py <recording>`: the flashes and beeps found and the sync offset.
- `beats.py <music>`: the tempo, beats and bars found.
- `markers.py`: above.
- `selftest.py <folder>`: makes a test recording (a flash and beep at 3 s), a 126 BPM music track and a log
  (`make_test_media.py`), and checks the sync (to the frame), the tempo and beats (within 25 ms), the EDL's
  timecodes, and a rough cut (valid, every cut on a beat). `--log <a real log.csv>` uses that log's rows.

## Notes

- The flash is the game's white, but the window's post-process tints it (156 230 255 at the default settings): the
  detector takes any frame whose mean brightness jumps by 60 to over 170 (of 255).

- The flash is drawn over the window's 2D layer, so a capture of the desktop window shows it in every Window View
  (Left Eye, Smoothed Mirror, Spectator Camera). A capture of the headset's view (e.g. through the runtime) does not:
  use the beep (`sync_detect.py` falls back to it; ~40 ms of sound latency later than the flash).
- Slow sounds (`vr_timescale_sound`) pitch the beep down if a log starts in slow motion: the beep isn't found then
  (the flash still is).
