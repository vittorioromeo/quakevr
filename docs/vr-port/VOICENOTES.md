# Voice notes: what each note keeps

Recording a note (off hand at the mouth, hold Y; [PLAYTEST.md](PLAYTEST.md), "Voice notes while playing") saves, in
`quakevr/notes/` (his data, like `motions/`: not in git, not in the package):

| File | |
|---|---|
| `<map>_<date>_<time>.wav` | the note (16-bit mono) |
| `<note>.txt` | the context as the note started (below) |
| `<note>_replay.csv` | the instant replay: the last `vr_note_replay_seconds` (30) before the note as a motion take |
| `<note>_entities.csv` | the entities near the player over the same seconds, 10 times a second |
| `<note>_log.txt` | the console's lines over those seconds, each with its time before the note |
| `<note>_frames.csv` | every host frame's time over those seconds; the hitches marked and summed up in its header |
| `screenshots/<note>.png` | Ironwail's screenshot, taken as the note started |
| `<note>.transcript.txt` | Whisper's transcript (`Misc/quakevr/transcribe_notes.py`) |

`transcribe_notes.py` writes `NOTES.md`: each note's transcript, then its context grouped (Where, Checklist, Hands,
Nearby, Settings changed, Files, Other: every line kept) and its screenshot. `--md-only` rebuilds it without Whisper.

Code: `Quake/vr/vr_voicenotes.cpp` (recording, the files), `vr_notectx.cpp` (the context, the console and frame-time
rings, the settings changed), `vr_motion.cpp` (the instant replay's rings: "The instant replay").

## The context (`<note>.txt`)

`key: value` lines, one thing a line; a key may repeat (`nearby`, `setting`).

| Key | |
|---|---|
| `time`, `map`, `game time`, `position`, `view`, `health`, `skill` | the moment (the client's view) |
| `main weapon`, `off weapon` | the client's: QC weapon id and view model |
| `checklist` | the checklist item last under the Checklist page's cursor (or picked): `[section] text (open / ticked / no longer listed)`, how long before the note |
| `last test` | the scenario the checklist's Test button last ran (`notectx::testRun`), how long before |
| `main hand`, `off hand` | the server's: weapon id and name, view model, offsets slot (`vr_wofs_*_<slot>`), weapon flags, clip/size (the record's `wi_clip`), ammo and counter, `spent (depleted)` |
| `main held`, `off held` | a carried prop: model (classname) #entity |
| `main enemy`, `off enemy` | an enemy held (`vr_foegrab`): class #entity, by which part, the hand's hold and the enemy's, the stretch, how long, its health, whether the hand drags it; then the two-hand moves' state (the throw's hardest turn, the neck twist, a ragdoll carried after a lift or throw) |
| `main ragdoll`, `off ragdoll` | a ragdoll's limb held: class #entity, the rig's part, health, and its state (active, struggling: effort, fatigue, each behaviour's weight, what is held, floor, drag, prone, head-down, grip lag) |
| `main force grab`, `off force grab` | the force grab's target: model (classname) #entity, locked, pulled |
| `nearby` | each monster within 5 m, nearest first: class #entity, distance, health, think function (its animation state: `army_run3`), frame, enemy, an armed grunt's weapon (or fists, disarmed) and reserve, knocked down, dying or a corpse, held, headless, its ragdoll's state |
| `setting` | each archived setting changed this session (from the third frame on; not a take's playback's own): `name first -> now (n changes, the last s before the note)`, `back as it was` when it is |
| `replay`, `console`, `frame times` | the sidecars' names; the replay's frames and seconds; the frames' median, worst and hitch count |

## The instant replay (`<note>_replay.csv`)

While Voice Notes are on (`vr_notes 1`) and `vr_note_replay` is 1 (the default), every host frame's row of the motion
recorder (`makeRow`: the runtime's tracking, the controls, the fingers, the weapons, the player, the nearest monster, the
melee events; no QC sample: its striking points and parry state) goes into a ring kept for `vr_note_replay_seconds`, and
every 0.1 s a snapshot of the entities near the player (the 32 nearest monsters within 1024 units, the held props, the
player) into another. A new map (or a saved game loaded) starts them again: a take is of one map. As a note starts both
rings are handed to the thread pool as they are (nothing copied or sorted on the main thread) and start again; the pool
writes them.

The take is a motion take ([MOTIONS.md](MOTIONS.md), "The take file") of label `replay`, every row of phase `rec`, `t` 0
at its first row: `vr_motion_play <path> map <its map>` plays it in the mock headset (its target the take's nearest
monster at its start). Its header has the settings as the note started (the hand calibration, `settings`,
`weapon settings`), which playback applies.

`<note>_entities.csv`: `t` (seconds from the take's `t` 0), `sv_time`, `ent`, `class`, `think`, `model`, `x y z`,
`pitch yaw roll`, `vx vy vz` (world units, degrees), `health`, `armour` (the player's), `frame`, `flags`, `enemy`
(entity number), `knockdown`, `corpse`, `held` (`vr_knockdown`, `vr_corpse`, `vr_foegrab_hold`), `extra` (the player's
ammo, an armed grunt's `vr_ag_wid`).

## Cost

Measured headless (vrfiringrange, a grunt near, a shotgun, an enemy held; `vr_note_replay_stats`, Debug > Save Note
Context / Note Replay Cost): about 3 us a host frame (the row) and 1.2 us a server frame (the sample and the snapshot);
the rows take about 2.2 KB each: 30 s at 90 Hz about 6 MB, at 120 Hz 8 MB (the ring keeps its capacity); the entity ring
under 1 MB, the console ring (2048 prints) about 0.15 MB, the frame ring 0.12 MB. A note's start gathers everything in
about 1 ms (the text, the rings handed over, the settings copied; the take's settings lines made on the pool), less than
the screenshot it takes. Off (`vr_notes 0` or `vr_note_replay 0`): nothing is kept.

## Commands

| | |
|---|---|
| `vr_note_replay 0/1` | the instant replay (with `vr_notes` on) |
| `vr_note_replay_seconds 30` | its length (also the console's and the frame times'; at most 600) |
| `vr_note_context` | a note's context and sidecars saved now without a sound (tests; Debug > Save Note Context) |
| `vr_note_replay_stats` | `NOTESTATS|` the rings' cost, rows and memory (Debug > Note Replay Cost) |
