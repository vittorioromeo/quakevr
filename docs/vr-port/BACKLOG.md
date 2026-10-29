# Backlog: agreed work for the next rounds

Items the author approved or asked about, not yet started. Each has a note on where it came from.

## The 1.1.0 plan (agreed 2026-10-17)

**Before 1.1.0:**
- His VR test pass of the 2026-10-17 ships (checklist top, tracker v7), then a fresh VR Tracy capture
  (`vr_tracy_preset full`) and a VR RenderDoc capture (passes now named) to confirm the perf work on real hardware.
- Decisions: all settled on next (checked 2026-10-23, branch `agent/open11`): particle size cap 0.15
  (`vr_particle_max_screen`), Item Pickup Mode Immersive by default with Auto-Collect an option (his 2026-10-18 note),
  the shadow atlas sized per map (`vr_shadow_atlas_grow` 1; branch `agent/dec11`, ROUND22.md "1.1 decisions"), awaiting
  his VR check.
- Open issues (checked 2026-10-23; ROUND22.md, "1.1 open issues: checked on next, three fixed"):
  - Fixed on `agent/open11`, awaiting his VR check: AD ogre smash too wide (comes down ahead, normal arc); stiff neck
    after a save load (`.vr_necksnapped`); flame crackle lost on load (restarted after the load).
  - Closed, already fixed: `weapons::value` and an inheriting slot's own defaults (adgunvariants' `QVR_WEAPON_OWN`;
    AD slots inherit nothing since adnoinherit); AD crossbow hold floats (adcrossbow2's turned model and his pose,
    adnoinherit's values; his VR check).
  - Fixed on `agent/wallhit2`, awaiting his VR check: crowbar/shotgun double wall hits (one wall blow a swing: not
    again until the weapon is off that wall, nor a much slower one within 1.2 s; `Misc/quakevr/melee_wall_takes.sh`
    replays his takes at a wall; ROUND22.md "One wall blow a swing").
  - Still open: dragging an enemy by the legs (real: after the leg pull the body doesn't follow the hands at any Pull
    Hold Strength, while a head drag does; cause not found); close two-hand crate grips pull one hand in (gripfit2); chain stretch on fast
    swings (magchain2: worst piece 78 % now, not fixed by more iterations); the Italian player's startup crash (waiting
    on his beta2 bug report; the OpenXR layer skip `f3bbe28ae` covers the likely cause).
- Release: a beta2 feedback window, then merge next into vr-ironwail and release 1.1.0 as Latest. If 1.1.0 is more
  than about a week away, a 1.0.2 from vr-ironwail with just the Matriarch/Sacrifice/Shub fixes.

**After 1.1.0, in this order** (pure refactors; waiting avoids conflicts with beta fixes and the Steam Frame branches,
and keeps refactor risk away from a release):
1. Formatting: clang-format (Zancle configs) on all first-party code, `// clang-format off/on` around hand-aligned
   tables, then clang-tidy fixes; one commit listed in `.git-blame-ignore-revs`. Never Ironwail's code.
2. Ironwail extraction: about 1,600 lines of VR code moved out of Ironwail files into `Quake/vr/` (pure moves), the VR
   main menu out of `menu.c`, the VR heap macro remap, `IRONWAIL_DIFF.md` refreshed.
3. Monster setup and weapon-id tables: `VR_MonsterSetup` and the `WeaponIdTo*` branch chains become data tables.
Each verified by an unchanged physics hash, melee eval and `vr_menu_path_check`.

**Later, lower priority:**
- Parallax option B (about a day; recovers about 15% wasted world shading, roughly 0.05-0.15 ms per eye net; extra
  depth copy pass and buffer; correctness risk for depth readers). Only if world shading stays the top GPU cost.
- Parallax depth write (off in High, on in Ultra since 2026-10-23; ~1.3 ms over off in an AD fight even with the near
  split): a tiled-depth early-out was tried and didn't pay (ROUND22.md "Parallax depth write: the tiles' early-out";
  local branch `agent/pdepthhiz`). Revisit only with a cheaper depth source (no MSAA decompress) or a driver that keeps
  early depth with conservative depth.
- Physics world built once per map (about 33 ms per load; props would keep their spawn-frame bodies). Cleanup only.
- Body triangles reordered arms-first (most of the pre-pass gain without the pass).
- View-entity leftovers (easeWrist 27-30 us, lying guns' buttons 19 us, tap verts 8 us, rig hand 10-13 us).
- Mid-game limb loads still on the main thread for unpredictable spawns (4-9 ms vs 1.2 ms at load).
- The "2D" pass in VR (0.18-0.2 ms) and the gadget screen's p95 spikes; empty OIT clears and sky draw.
- Tracy GPU zones lost after toggling GPU detail mid-run.
- About 55 test registrations left in `vr_main.cpp` (diag moves).
- Expansion textures missing reliefs: generate them like AD's.
- Foveation check (what `vr_foveated` does; NVIDIA shading-rate in GL) and a rendering-abstraction audit (GL calls
  per file) before any Vulkan decision; vkQuake (GPL-2.0) as the backend reference.
- Linux (WSL native build in progress), Steam Frame branches with kblood, standalone Quest (single-pass stereo kept
  for it).

## Next round (approved)

### Active ragdolls: research a reference video (author, 2026-10-22)

Research https://www.youtube.com/watch?v=r8Q1nwHPwJ0 for active ragdoll improvements (stagger, balance, get-up, the
active deaths), plus the references below. Target: Box3D active ragdolls on monsters with custom rigs; procedural, no
ML at runtime. Borrow ideas only unless noted. Not started.

- physics-4-fun (github.com/slopezjur/physics-4-fun), Godot 4 C#, a Euphoria tribute; no licence stated (ideas only).
  Steal: ankle, hip and stepping balance strategies; capture-point foot placement with 2-bone IK (could fix our
  stagger glide); implicit stable PD after Tan-Liu-Turk (no chatter under load); local motor collapse at a hit and
  wound clutching (a hand to the wound); parachute fall bracing (arms out before landing); raycast wall bracing; a
  5-phase get-up (hands, torso, lead knee, half-kneel, stand).
- bevy-ragdoll (github.com/sagan-software/bevy-ragdoll), Rust, MIT/Apache, early. Steal: hit profiles from pistol to
  explosion that weaken muscles locally and spread the impulse through the body; partial ragdolls (legs animated,
  upper body loose).
- UE5 PhysicsControl plugin (Epic, in the engine source; licensees only): mature design reference for per-body
  strength and damping profiles, named control sets and blending between them.
- Shrimple Ragdolls (sbox.game/fish/shrimple_ragdolls, s&box): a "Motor" mode like Euphoria plus procedural hit
  reactions taking position, force, radius and duration.
- Unity.Humanoid.ActiveRagdoll (github.com/EggyStudio/Unity.Humanoid.ActiveRagdoll), MPL-2.0: limbs weaken while in
  contact and regain strength after (Following, Losing, Gaining, Dead states); a mask for contacts that shouldn't
  weaken.
- Simpler, popular references: ashleve/ActiveRagdoll (PID, Unity), davidkimighty/TARS (MIT, Unity, 122 stars),
  TildeAsterisk/Physicanim (Unity, Mixamo), kressdev/RagdollTrainer (Unity ML-Agents).
- Research (heavy, offline training, not for runtime now): NVlabs/ProtoMotions, nv-tlabs/ASE, PARC (SIGGRAPH 2025,
  github.com/mshoe/PARC); push-recovery ideas only.
- Delirium physics (author, 2026-10-23): research for future active ragdoll improvements:
  https://github.com/PVCPC/delirium-physics, https://github.com/YoucefNabil/delirium-physics and
  https://github.com/YoucefNabil/skyrim-delirium (a Skyrim port). Not read yet: check their licences before borrowing
  anything beyond ideas.

### Pillar/pipe climbing (brace a hand on each side) (23:10:39, 2026-10-14)

Research climbing by bracing a hand on each side of a pillar or pipe and moving upward hand-over-hand. Open.

### Repository chores

## To manually revisit (author unsure)

### Flies on corpses and gibs (the author, 2026-10-02: "add it to the to-do list")

Scourge of Armagon's head flies (one severed head in ten loops `misc/flys.wav`, player.qc HeadThink) were pointless
as they were, so they are off (`vr_head_flies` 0, Gore > Flies on Heads; NOTES.md vrfiringrange_2026-10-02_01-29-37).
To revisit and make interesting: flies on every corpse and gib (not only heads), arriving after it has lain a few
seconds, with fly particles buzzing round it as well as the sound.

### Bloody shotguns' own look (the author, MG3 decision 4, 2026-10-06: "Backlog TODO: distinct bloody textures/skins")

Dawn of the Machine's bloody shotgun and super shotgun (M3-14, `QC/vr_mg3_weapons.qc`) play as intended (0.28 s
refire; 28 pellets) but are drawn as Quake VR's own shotguns: MG3's `v_bloodshot.mdl`/`v_bloodshot2.mdl` are view
models with an arm, made for the flat view. To do: bloody skins for `progs/v_shot.mdl`/`v_shot2.mdl` (a second skin
picked while the bit is set: `MG3_BloodyBits()`), or MG3's models stripped and laid as the Super Axe's are
(`vr_monstermods.cpp superAxePoses`), with their own weapon settings slots.

## In progress, 2026-10-15 (other workers; not open here)

Glove customization, Model vertex wobble (2) (rigged monsters' vertices rebuilt from the fitted skeleton). Their texts were removed from
this file; the workers' ROUND22.md sections carry them.

- **Temporal AA, then DLSS/DLAA and FSR 3.1** (`docs/vr-port/TEMPORAL.md`, the author 2026-10-16: "start a worker on
  motion vectors + initial TAA implementation"): stages 2a (motion vectors) and 2b (TAA behind `vr_taa`, off by
  default) landed in a first cut (ROUND22.md, "Temporal AA: motion vectors and a first TAA"); HMD tuning next. The DLSS
  helper still waits on TEMPORAL.md's section 12 questions 1-3 and 5.

## Proposed, waiting on the author

- **The graphics on the flat screen** (asked round 20): most features already draw on the desktop; to do: the
  float scene, tone mapping, grades and bloom for the window, and a check of each feature with VR off.
- **Lower-end graphics settings before a release** (the author, round 20: "maybe later on before release, you can
  prepare some lower-end graphics settings as well"): the shipped defaults (`quakevr/vr_defaults.cfg`) are the
  author's RTX 4090 settings; the presets (`vr_graphics_preset`) exist, but their low/mid tuning is unverified.
- **`.rtlights` support** (round 20 discussion): load DarkPlaces' hand-authored light lists where they exist to
  drive the shadowed map lights (better placed than the map's light entities).
- **Distance-field AO** (round 20 discussion): only if capsule/box AO leaves obvious gaps.
- **Menus and settings for players, not only for tuning** (the author, 2026-10-03; `docs/vr-port/archive/MENU_REVIEW.md`):
  left: gameplay-feel presets (or gameplay kept as one tuned design) and the per-slot cvars (`vr_wofs_*`,
  `vr_prop_*`) to data files.
- **Performance leads left open by the 2026-10-03/05 reports** (the reports were removed 2026-10-06; git history has
  them: `PHYSICS_PERFORMANCE_RESULTS_2026-10-03.md`, "Not done", and `PERFORMANCE_BENCHMARK_20261005.md`, items 4-5):
  - *Hit-box traces:* skip `touchNearby` for props too slow to hurt, after checking the monsters' QC touch functions
    (a leaping dog's or fiend's touch acts on what it meets).
  - *Floating props' equilibrium sleep:* a design decision (they would stop bobbing).
  - *Touch queries:* `FL_EASYHANDTOUCH` grows every prop's box for the hands; prop-to-prop queries could use the
    ungrown box.
  - *Shadows in crowded, brightly lit scenes* (32 overlapping lights: 1.59 ms GPU against 1.15 ms without shadows):
    bone reuse, caching for stable lights and objects (the dlights' world casters: PERF_DECISIONS.md, "Leads"),
    per-light update budgets, resolution by projected importance (`vr_lighting.cpp`; the casters are set up once a
    pass over the lights since BENCHMARKS.md's "Second follow-up").
  - *Teleporter views* (about 0.8 ms GPU and CPU for one doorway): the destination's resolution chosen by the
    aperture's projected size, tighter destination draw lists, per-frame entity preparation shared across views, the
    shadow atlas drawn once for the portal views (PERF_DECISIONS.md, item 2).
- **Low priority: review the pickup-thinks research** (the author, 2026-10-03: "save your research regarding the
  nextthink stuff in a document so that we can review it later"): `docs/vr-port/PICKUP_THINKS.md`. Idle pickups think
  every 0.02 s; options to think less without changing gameplay (idle interval with exact deadlines, no thinks while
  carried, the spin left to the engine), and where thinking resolution would be lost (engine-side knocks, unless the
  engine wakes the pickup). Small absolute gain (QuakeC is ~0.07 ms a frame): review, then decide.

## Later (the author, 2026-10-11, before 1.0.0: "Make sure everything is in the backlog for later")

### Ideas from Quake Combat+ v5.7 (the author, 2026-10-16; ideas only: no source or assets reusable without its authors' permission)

- **Heart gib you can hold:** a beating, squeezable, throwable organ with a haptic pulse, on top of our physics gibs.
- **Berserk pack:** a level-long melee power state (red tint, bigger gib/decap pops, x2-3 melee damage) rewarding
  fists/axe/chainsaw play; plugs into the existing `vr_decap_pop_*` scale cvars.
- **Hit-confirm audio layering:** a meaty thud when most pellets connect, distinct kick/impact sounds, per-monster gib
  sounds.
- **Wounded monsters bleed a trail, then crawling half-bodies:** live low-health monsters drip decals; big monsters
  split in half and drag the torso with a smear (a "finish it" beat).

### Enemies' own weapons for the player (the author, 2026-10-15)

- **Phantom Swordsman's sword**: done ("Droppable, usable swords", above).
- **AD enemies with unique weapons:** review AD's monsters that carry their own weapons (beyond the soldiers' guns
  already usable, and separate from "AD soldiers' guns: refine their VR models") and make each usable by the player.
- **AD Deflector's gun (after 1.1; the author, 2026-10-23):** left out of `VR_ADGun_ForClass` by scope (the guns were
  built for the soldier family; ROUND22.md "Infected drops, Freddie gibbed, Deflector"). It has no single gun to copy:
  the yellow skin fires a flame wall plus 1-3 drunk rockets, the blue (PLASMA spawnflag) 3 plasma bolts plus rockets;
  AD gibs it as an inert `w_enforcergun.mdl`. To do: pick the attack(s) and one gun or one per skin, then a new WID and
  a ninth burst-rifle variant (`vr_adgunart.cpp`), weapon offsets, a `VR_ADGun_Fire` branch and `ForClass` entry, its
  gun hidden in its death frames and its rig re-measured without it, the gib cleared on a drop, a toolgun row and a
  test.

### AD soldiers' guns: refine their VR models (author, 2026-10-15)

His note: "We will eventually refine these models." The eight guns AD's armed soldiers drop are single use and their
own (ROUND22.md, "AD soldiers' guns: their own again, our shots inside"); their models are cut from AD's monster models
by the engine (`<monster>.mdl#gun<n>`, `vr_adgun_models`), low-poly and textured as the monster. Per gun:
- **Rocket soldier's launcher** (`mon_soldier.mdl#gun4`): fires our rocket. Model: a proper launcher shape, a muzzle
  that reads where the rocket leaves.
- **Grenade soldier's launcher** (`mon_soldier.mdl#gun6`): fires our grenade (no longer AD's oversized one). Model: a
  drum or tube that reads as a grenade launcher, distinct from ours.
- **Plasma soldier's rifle** (`mon_soldier.mdl#gun8`), **eliminator's** (`mon_enforcer.mdl#gun6`) and **centurion's**
  (`mon_centurion.mdl#gun0`) plasma guns: AD's plasma bolt with our hit. Models: three distinct energy guns; maybe a
  glowing cell or coil (a fullbright skin) and a charge readout of their single-use rounds.
- **Defender's shotgun** (`mon_enforcer.mdl#gun3`): our pellets. Model: a wide-barrelled riot gun.
- **Pyro's flamethrower** (`mon_pyro.mdl#gun0`) and **fumigator's poison sprayer** (`mon_fumigator.mdl#gun0`): AD's
  flames and puffs. Models: a fuel tank or canister that shows the fuel left.
For all: grip and muzzle offsets checked in the headset, a pickup outline, the spent look (VR_EnemyGun_Spent) kept.

### Look at QuakeRay (the author, 2026-10-19)
Check out https://github.com/f1ames0ff/QuakeRay for information about a Vulkan migration (see VULKAN_MIGRATION.md) and
better lighting effects. Not yet reviewed: licence, base engine and what is reusable unknown.

### Dual-wield parry: crossed swords (the author, 2026-10-18)
With a sword in each hand (e.g. the Death Fury's pair), crossing the two blades in front of the body should be a valid
parry pose; today it isn't recognised. Later, after the AD melee work.

### Multiplayer update (after 1.1; the author, 2026-10-18)
Reference: https://github.com/ObeseCatLord/Quakespasm-VR (vkQuake-based VR port, GPL-2.0-or-later: code can be
borrowed with its copyright headers kept and credit; per-file notices; its `Quake/ice` is QSS-M-derived with its own notices).
- Co-op rules: a "streamlined" mode (ammo respawn, shared keys, no friendly fire, teleport to a teammate) with the classic
  rules selectable (theirs: `coop_inventory_policy.h`, `sv_main.c`, `sv_coop_classic`). Check docs/vr-port/MULTIPLAYER.md first.
- Router-friendly joining: ICE/WebRTC NAT traversal and room links (theirs: `Quake/ice/*`, `docs/ice-transport.md`;
  needs GnuTLS/OpenSSL).
- Voice chat: Opus with voice activity detection, jitter buffer, voices placed in 3D (theirs: `voice.c`,
  `voice_jitter.c`, `voice_vad.c`, `voice_protocol.h`).
- Synchronisation of our VR systems for other players: physics props and ragdolls (box3d state or authoritative server
  sim + client smoothing), active ragdolls/flinch, held objects and two-hand grips, body IK pose (head, hands, body
  tracking), player body customisation (ranger head, gloves, appearance settings), holsters/magazines, gore and decals.
- Their prediction work (shared `pmove.c`, CSQC prediction, `predictive-*` docs) as a reference for movement feel.

### Seated play: holsters and pouches (the author, 2026-10-18)
The holster/pouch zones (hip, upper, shoulder, ammo, grenade, magholster, item pouches) are placed for standing play and
don't suit seated play. Needs a seated layout: zones relative to the seated body (chair, thighs, chest), reachable
without leaning, chosen automatically from the seated calibration (vr_setup seated flow) with its own defaults.

### Performance

- **Optimise decals** (PERF_DECISIONS.md 15): in a rocket fight (`play_e1m1_lights`) ~900 marks are 1.34 ms of the
  world pass's 2.67 at 2048 per eye, about half (~2.4 ms at his 2782 eyes); each pixel walks its cell's bucket (up to
  64 marks, a whole 80-byte record each before its early-out). Reading a mark's normal and middle first was tried and
  reverted (no gain, 0.6-0.9% of the pixels changed). `vr_decal_max` lowered to 768 meanwhile (config version 118).
  Left: the options with a visible change (PERF_DECISIONS.md 15), for him.

### Map and game loading (ROUND22.md, "Load-time speedups without their drawbacks (1.0.1, 2026-10-10, worktree `load101`)")

- **Model loading in parallel** (the calibration room's alias models, now 270 of the 430 ms: texture uploads, skins'
  decoding and copies, md5 replacements): the loaders share the hunk, cache and GL; would need each loader split into
  a decode into its own buffers (on the pool) and a commit in order on the main thread. Two exact pieces done instead.

### Art (ROUND21.md, "For the author": Blender)

- **Stretched UVs left** (ROUND21.md, "Patch 1.0.1: stretched skins re-mapped"): the grappling hook's claws (10% of it:
  their strips do not unwrap flat), the rocket launcher's and plasma gun's few faces whose strips would not unwrap
  flat; the same few on the lava super nailgun and multi rocket launcher (re-mapped too: ROUND22.md, "Patch 1.0.1:
  the lava super nailgun and multi rocket launcher re-mapped").
- **Body skins' texel density at the wrist**: about a sixth of the hands' (256 x 256 skins). The skins are the
  author's own repaint (commit 24fa2de6d): a re-map or a bigger skin only resamples his paint; a sharper sleeve needs
  him to paint the wrist and bracer at a higher resolution (or make_vrbody.py's painter at 512, losing his repaint).

### Hull build

- **Drop the remaining kept copies** (ROUND21.md, "The hull build's memory"): the last level's kept copies, 550 MB at
  once without the budget (about 240 with it), could go as the levels' between did.

### Test tooling

- **`vrtutorial_playtest.py` room 8**: the mock's hand loading often throws the shells instead of loading them, so the
  range's door stays shut and every later gate fails (the arena is run with `--from arena`).

## Left open by finished plans (2026-10-09)

### Stealth AI: limits (STEALTH.md; open since it was built, 2026-10-08)

- The meter is one per monster, on the most suspicious player in its sight (not a meter per player: the dark player's
  own suspicion isn't kept while the lit one holds it).
- A client's light and lamp are his own client's word (sent in his VR move): a modified client could send "dark".
- Alert monsters walk with Quake's movetogoal (no path finding): a point across a gap or up a ledge ends their walk when
  stuck (3 s without headway), then they search where they are.
- Quake's own relay stays: an idle monster that sees another turn Hostile (FoundTarget's `sight_entity`, a tenth of a
  second) turns Hostile at its enemy too, whatever the light on him (as `vr_stealth_share_*`, but with Quake's sight
  ranges).

### Magazines sliding home (RELOAD.md; noted, not done, 2026-10-08)

A magazine seated at its well drawn sliding the last bit home: a short seat slide, as the shells' slide
into the guns (vr_collectfx.cpp's "into the gun" variant, its path the well's axis).

### Dawn of the Machine: co-op and dm1 (MG3.md, decision 6, 2026-10-06)

MG3 ships single-player first (`soloOnly` 5, as Dopa); its co-op and its deathmatch map (dm1) are for later.

### A turned teleporter's exit bound (HITZONES_AND_PORTAL_REVIEW_2026-10-04, open P3; the review removed 2026-10-09)

The body carried through a gate is checked at the destination with an axis-aligned bound (`vr_portals.cpp`, the
destination body bound), so a gate turned by a non-cardinal angle can block a narrow exit that the body would fit
through. A bound turned with the gate (or a box test in the gate's frame) would fix it.

### Old QuakeC notes (the old engine's "TODO VR" markers, triaged 2026-10-09: TECHDEBT_2026-10-09.md 10)

The notes about organizing, hitboxes and ideas were dropped; these name real, small issues:
- **Rogue's teamplay variants** (`rogue_teamplay.qc`): their item and weapon management is commented out (never
  rewritten for hands and holsters), so `teamplay 3`-style Rogue modes don't work in VR; their impulses
  (`weapons.qc`) are off too.
- **`readytime`** (`vr_defs.qc`): one for every client (no shooting or taking weapons just after a load or a
  spawn); in co-op one player's spawn holds the others for that moment.

## Deferred from his release-day notes (2026-10-10)

- bHaptics support (vest, arms, head, hands: hits by direction, recoil, explosions, melee impacts) (the author,
  2026-10-11).
- Standalone Quest version (Android/Quest native build: OpenXR on Android, GLES/Vulkan renderer path, performance
  budget for the mobile GPU, packaging/sideloading) (the author, 2026-10-11).

TODO: linux support

## Rendering feasibility from 2026-10-12 voice notes

- **AD relief authoring pilot:** the pilot of 10-20 textures (reviewed height masks, `_bump`/`_norm` sidecars) is not
  started.
- **Optional vertex breathing reduction:** the dequantisation shipped; the per-bone rigid-fit option is the wobble
  item (2), in progress 2026-10-15 (another worker).
- **Hidden mock blank readback:** some isolated eye/window captures are entirely black even with flat lightstyles;
  investigate capture/target validity separately if needed. This is not evidence of the reported lamp flicker.
### Native AD inventory and interface (author, 2026-10-12 11:27:41)

Audited and the gadget's AD page added (ROUND22.md, same section). Left: the custom keys' names on it (strings, not
stats), the jump boots' power level, coop.

## TODO

- Vittorio is unsure whether nail pistols should appear as pickups in maps (crates / replacing some weapon spawns); armed grunts carrying them may be enough. Revisit after beta4.
  - Also consider flare guns and heavy revolvers.
- **Player armour parity** (Vittorio, 2026-10-28, from the armed humanoids' armour plan: C:\OHWorkspace\qvr-kit\scratch\research_agarmor.md, question 8): once armed humanoids wear generated per-limb plates (helmet, cuirass, pauldrons, bracers, thighs, shins), consider giving the player the same plates fitted to the player body (the generator can fit vrbody too), instead of the armour painted on the body skin; per-piece damage for the player too? Decide after the humanoids' armour ships.
- **Ammo type follows the loaded magazine:** with pouch reloading (v1.1), the magazine/ammo you load picks the gun's ammo type (e.g. a lava-nail magazine fires lava nails), making the ammo switch button unnecessary for magazine-fed weapons (his idea via a player exchange, 2026-10-16).
