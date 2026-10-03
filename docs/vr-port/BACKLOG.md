# Backlog: agreed work for the next rounds

Items the author approved or asked about, not yet started. Each has a note on where it came from.

## Next round (approved)

### Fitted hands (the author: "please implement it, but not for this round, for the next one")

Done in round 21 (steps 1–4, plus weapon hotspots: `ROUND21.md`). Left: the controllers' finger-touch sensors
(OpenXR touch paths), and objects placed into the open hand rather than against the fist (server-side carrying).

Hands that wrap what they hold instead of a rigid pose: physics objects (boxes, backpacks, gibs, heads, armour)
and weapon handles, Half-Life: Alyx style. Planned in four steps:

1. **A jointed hand** (1 round): three segments per finger plus an opposable thumb, generated procedurally like the
   body (`Misc/quakevr/make_vrbody.py`), driven by today's curls first so it looks the same (skinned through the
   avatar code, or 15 rigid segments).
2. **Grasp solver for physics objects** (1 round): at the grip, each finger closes joint by joint until its segments
   touch the object's surface (the real triangles round 20's `held::surfaceFit` already extracts), solved once and
   kept relative to the object; re-solved when the grip or the controller's finger sensors change.
3. **Weapons** (1–1.5 rounds + headset tuning): per-weapon hints (trigger point, grip axis, foregrip surface, "index
   on the trigger", "thumb over the top"); the fingers wrap the grip's triangles; the two-handed helping hand
   (foregrip, the sword's blade grip) through the same solver, replacing the fixed 2H hand poses.
4. **Palm placement** (0.5 round): the palm turns and slides to sit flush, the drawn hand leaving the tracked
   controller by at most ~5 cm / 20°, eased in over ~0.1 s.

Controller finger sensors (Touch: index, thumb) blend in: a lifted index straightens away from the trigger; fingers
stop at contact. Risks: the visual hand drifting from the controller (caps), thin geometry (hints), the wrist
matching the body's arms, multiplayer (finger poses are client-side). Supersedes round 20's per-weapon finger
sliders for what it covers (they stay as overrides).

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
- **Ragdoll physics for corpses and dismemberment** (the author, 2026-10-02: "research ragdoll physics for enemy
  corpses and dismemberment"): research only, nothing built yet. Questions to answer: how to rig Quake's vertex-animated
  .mdl monsters (no skeletons) for ragdolls (derived bone chains per model, skinning the last death frame, or
  per-limb pieces); Box3D joints and their cost with many corpses (the thread benchmarks: physbench); blending from
  the death animation into the ragdoll; severing limbs/heads where hit (ties into small gibs, ThrowHead, wounds and
  blood decals); networking/saves; and which monsters first. Corpses are already one Box3D body each (ROUND21.md,
  "Corpse collision": `Kind::Corpse`, `catCorpse`, its mask and passes): a ragdoll would replace that body.
- **Vore shove** (the author, 2026-10-02: "I would like the vore to also have a shove attack when the player is
  close, later on"): extend the enemy shove (QC/vr_enemyshove.qc) to the vore (shalrath), with its own animation.
- **Flashlight optional; a brighter option** (the author, 2026-10-02): make the flashlight completely optional (off
  with no belt torch, no zones, no gadget hints, nothing that assumes it), and give an easy way to raise ambient
  lighting for players who don't want the moody atmosphere and prefer higher visibility (e.g. one "Brightness" or
  "Visibility" preset/slider on the main VR page that raises ambient/minimum light, exposure and tone mapping
  together, rather than many separate graphics settings).
