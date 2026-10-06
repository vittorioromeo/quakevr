# Backlog: agreed work for the next rounds

Items the author approved or asked about, not yet started. Each has a note on where it came from.

## Next round (approved)

### Flies on corpses and gibs (the author, 2026-10-02: "add it to the to-do list")

Scourge of Armagon's head flies (one severed head in ten loops `misc/flys.wav`, player.qc HeadThink) were pointless
as they were, so they are off (`vr_head_flies` 0, Gore > Flies on Heads; NOTES.md vrfiringrange_2026-10-02_01-29-37).
To revisit and make interesting: flies on every corpse and gib (not only heads), arriving after it has lain a few
seconds, with fly particles buzzing round it as well as the sound.

### Repository chores

- **Funding links** (the author, after round 20): copy `.github/FUNDING.yml` from `master` to the port's branch
  (`github: SuperV1234`, `patreon: vittorioromeo`, the PayPal link) and add Ko-fi
  (`ko_fi: vittorioromeovee`, i.e. https://ko-fi.com/vittorioromeovee).

## Proposed, waiting on the author

- **Temporal AA, then DLSS/DLAA and FSR 3.1** (`docs/vr-port/TEMPORAL.md`): stage 1 (motion vectors for both
  eyes) is safe to start any time; the DLSS helper waits on the questions at the end of TEMPORAL.md (private use or
  public, the helper's licence, FSR 3.1, TAA as default AA, 120 vs 90 Hz with DLSS).
- **The graphics on the flat screen** (asked round 20): most features already draw on the desktop; to do: the
  float scene, tone mapping, grades and bloom for the window, and a check of each feature with VR off.
- **Lower-end graphics settings before a release** (the author, round 20: "maybe later on before release, you can
  prepare some lower-end graphics settings as well"): the shipped defaults (`quakevr/vr_defaults.cfg`) are the
  author's RTX 4090 settings; the presets need tuning for mid and low-end GPUs.
- **`.rtlights` support** (round 20 discussion): load DarkPlaces' hand-authored light lists where they exist to
  drive the shadowed map lights (better placed than the map's light entities).
- **Distance-field AO** (round 20 discussion): only if capsule/box AO leaves obvious gaps.
- **Vore shove** (the author, 2026-10-02: "I would like the vore to also have a shove attack when the player is
  close, later on"): extend the enemy shove (QC/vr_enemyshove.qc) to the vore (shalrath), with its own animation.
- **Flashlight optional; a brighter option** (the author, 2026-10-02): make the flashlight completely optional (off
  with no belt torch, no zones, no gadget hints, nothing that assumes it), and give an easy way to raise ambient
  lighting for players who don't want the moody atmosphere and prefer higher visibility (e.g. one "Brightness" or
  "Visibility" preset/slider on the main VR page that raises ambient/minimum light, exposure and tone mapping
  together, rather than many separate graphics settings).
- **Menus and settings for players, not only for tuning** (the author, 2026-10-03): `docs/vr-port/MENU_REVIEW.md` and
  `MENU_INVENTORY.md`. Built on branch `vr-ironwail-menus` (MENU_REVIEW.md, "Status"): Menu Detail levels, VR Settings
  for every player with Comfort and Handedness presets and volume, VR Calibration at a first start, one home per
  setting, changed settings marked with Reset This Page and Changed Settings, the `wait5` fix. To test in the headset
  and merge. Left: gameplay-feel presets, a comfort vignette, per-slot cvars to data files (the questions at the end
  of MENU_REVIEW.md).
- **Low priority: review the pickup-thinks research** (the author, 2026-10-03: "save your research regarding the
  nextthink stuff in a document so that we can review it later"): `docs/vr-port/PICKUP_THINKS.md`. Idle pickups think
  every 0.02 s; options to think less without changing gameplay (idle interval with exact deadlines, no thinks while
  carried, the spin left to the engine), and where thinking resolution would be lost (engine-side knocks, unless the
  engine wakes the pickup). Small absolute gain (QuakeC is ~0.07 ms a frame): review, then decide.


### To sort (the author's notes)

- KoFi links in installer, see CircuitLord's TF2 as an example
