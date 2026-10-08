# Logo splash: sound sources

`sound.py` builds the track from two kinds of source only. Nothing was downloaded, and no audio file is in the
repository.

## Quake's own sounds (id Software, from the player's id1 paks)

These are read in place from `C:\OHWorkspace\qvr-kit\qbase\id1\pak*.pak` when the track is built. They are id assets:
never commit them or any file extracted from them. They are fine mixed into the trailer's own audio.

| Sound | Used for |
|-------|----------|
| `player/udeath.wav`, `player/gib.wav`, `zombie/z_gib.wav` | the gib burst |
| `blob/land1.wav`, `blob/hit1.wav`, `zombie/z_hit.wav` | the blood splash hitting the wall |
| `ambience/drip1.wav` | the occasional drip |
| `misc/deepthud.wav`, `hipweap/mjolhit.wav`, `misc/clang.wav` | the letters' heavy steel slams (alternating, pitched per letter) |
| `items/r_item2.wav` | the "UNLEASHED" materialise moment |
| `weapons/r_exp3.wav`, `player/lburn1.wav`, `player/lburn2.wav` | the fire catching |
| `ambience/fire1.wav` | the burning loop to the end (two decorrelated copies) |
| `weapons/ax1.wav`, `player/axhit2.wav` | the axe (full version only) |
| `player/tornoff2.wav`, `zombie/z_gib.wav`, `player/udeath.wav` (its start) | the head's rip and pop (head stem) |

## Synthesis (in `sound.py`)

- Sub booms and thumps (decaying sine sweeps) under the burst, each letter and the ignition.
- Noise transients and whooshes: each letter's fall at the camera, the fire's whoomp.
- Inharmonic struck-metal rings (per-letter partials, so no two are alike), plus the glint's ring.
- Wet splats: band-passed, wobbling noise bursts for the spray, the squirted blood and the finger's wet clicks.
- The finger smear: filtered noise following the finger's speed along its path (`pre/meta.json`), panned with it.
- The materialise shimmer (rising noise and gliding partials), the dust swells, the fire's roar and crackles.
- A dark synthetic room reverb (decaying noise impulse response) on a send.

## Internet sources

None. The CC0/public-domain sources Vittorio allowed (opengameart.org, freesound CC0, Sonniss GDC bundles) weren't
needed: these rules forbid unattended downloads, and Quake's sounds plus synthesis covered every cue. A downloaded file
would be listed here with its URL, licence and author, and kept out of git.

## Mix

48 kHz, 24-bit stereo. Events are placed on the timeline's frames (800 samples a frame) and panned to where they
happen on screen. Each letter's slam lands on its camera kick. The master is levelled to -14 LUFS integrated
(ITU-R BS.1770-4 gating), with a look-ahead limiter holding the true peak at -1 dBTP (4x oversampled). The no-grunt
track measured -14.08 LUFS and -1.04 dBTP.
