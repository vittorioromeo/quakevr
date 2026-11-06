# Backlog: agreed work for the next rounds

Items the author approved or asked about, not yet started, grouped by area; each keeps a note on where it came from.
Retired items are in git history (last cleanup 2026-11-06).

## Proposed order (2026-11-05, waiting on his OK)

1. **Fixes:** done 2026-11-05.
2. **The 1.1 release:** release pass (QUEUE.md), lower-end presets, a VR Tracy capture. Only on his word.
3. **Flat-screen:** per-weapon pose, then procedural reloads, then the hotkey audit, the window's graphics, recorded motions.
4. **Armed humanoids** (the key feature): the enforcer's real backpack, a design study for crouching and crawling;
   then gremlins, AD's humanoids and their packs.
5. **Combat feel:** hit-confirm audio, crossed-sword parry, wounded monsters; embedded blades (finish, then props) and
   the active ragdoll research as larger pieces.
6. **Weapons and content:** AD guns' models and own weapons, bloody shotguns, reliefs, ammo type
   from the magazine, AD inventory.
7. **Refactors, right after 1.1:** formatting, Ironwail extraction, design review P3/P6/P8/P9/P10 (QUEUE.md).
8. **Big features, each needing its own decision:** multiplayer, modelling formats and glTF, seated play, player
   crawling, pillar climbing, TAA/DLSS, platforms, bHaptics.
9. **Performance and parked items:** only when a capture makes one the top cost, or when a player hits it.

Staying in the backlog by his decision (2026-11-05): pickup thinks, heart gib and berserk pack, TAA/DLSS.

## 2. The 1.1 release

- **Review the installer's settings page before 1.1** (Vittorio, 2026-11-06): go through every setting on the installer's "Set up the game" page (`Misc/installer/settings_page.txt`): which ones, their order, labels and help; decide whether "VR or flat screen" belongs there (today the page leaves it off: the game forces VR at every start).
- **Gameplay page before 1.1** (Vittorio, 2026-11-06): VR Settings must have a "Gameplay" section that tweaks every major gameplay decision from one simple menu (armed replacements, embedded blades, knockdowns, melee and parry, bullet time, arena, difficulty-like toggles...); check vr_menu_gameplay.inc covers them all, plainly named.
- **Armed replacements on by default in 1.1** (Vittorio, 2026-11-06): `vr_agrunt_repl` 1 (already the default; keep it).
- **Installer settings page** (users' request, 2026-11-06): after installing, a page to set the most common settings before the first launch; his call on scope and timing.
- **Release pass:** QUEUE.md lists it (release build tests, real packaging, beta4 upgrade path, mods, notes).
- **A VR Tracy capture** (`vr_tracy_preset full`) and a VR RenderDoc capture, to confirm the perf work on real hardware.
- **Lower-end graphics settings before a release** (the author, round 20: "maybe later on before release, you can
  prepare some lower-end graphics settings as well"): the shipped defaults (`quakevr/vr_defaults.cfg`) are the
  author's RTX 4090 settings; the presets (`vr_graphics_preset`) exist, but their low/mid tuning is unverified.

## 3. Flat-screen

- **Per-weapon flat-screen pose** (Vittorio, 2026-11-05): an easy way to tweak each weapon's pose in flat-screen; some should be held differently (pistols, rifles, shoulder launchers). Tweaking must keep sight alignment and the ironsight working.
- **Flat-screen immersive reloading** (Vittorio, 2026-11-05): on by default, from pre-recorded or procedural animation depending on the weapon. To start, make all reloads procedural with simple rules: e.g. the shotgun rolls a bit so the shell port is more accessible, and the off hand goes smoothly back and forth between the ammo pouch and the port until the reload is interrupted or the weapon is full.
- **Flat-screen hotkeys for VR interactions** (Vittorio, 2026-11-05): support most VR-specific interactions with proper hotkeys: parrying, switching ammo type, throwing, shoving, climbing and so on. Harder ones can stay VR-only. Audit each and propose a solution or suggestion for it.
- **Flat-screen left-handed mode** (Vittorio, 2026-11-05: "We can add a 'flatscreen lefthanded mode' later"): the mouse's
  buttons route by what the hands hold (`flatplay::routeFire`: one weapon, LMB fires it and RMB aims; dual wielding, LMB
  the right hand, RMB the left). A left-handed mode would change that behaviour (e.g. LMB the left hand when dual wielding).
- **Recording motions for flat-screen** (Vittorio, 2026-11-05): an easy way to record motions in VR for melee attacks and reloading, so they can be used in flat-screen mode as realistic animations.
- **The graphics on the flat screen** (asked round 20): most features already draw on the desktop; to do: the
  float scene, tone mapping, grades and bloom for the window, and a check of each feature with VR off.

## 4. Armed humanoids

### Pick Up a Second's default (the author, 2026-11-03)

The smarter pickup shipped (ROUND24.md, "Pick Up a Second, smarter": only a cheap trip, tighter in a fight);
`vr_agrunt_dual_pickup` is still 0. Left: his call on the default once he has played with it on.

### The enforcer's backpack made a real one (author, 2026-11-02)

His words: "For the enforcer, please make it a lift zone only for now. Add a backlog item to make it a real backpack."
Today (ROUND23.md, "Lift only") an armed enforcer, whatever it holds, has only a lift zone on the pack baked into
vrenforcer.mdl (`agarmor::bakedPackLo`, `.vr_wp_baked`): it can't come off, nothing counts on it, its loot is its own. To make it real: split the pack's vertices out of the model
(behind `Wearer::backCut` -10.5, mostly on the chest bone) so they hide when it comes off (a mesh split or a skin), a
matching detached prop (backpack.mdl is smaller and looks different), and the back underneath, probably unpainted, painted.
Then the usual three parts as the grunt's: the anchor, the ragdoll box, the loot through `VR_WP_DeathLoot`; drop
`.vr_wp_baked`. (His note 2026-11-03 02:25:39: a melee-only enforcer stays liftable while the model shows the pack.)

### Worn backpacks for AD's armed humanoids (author, 2026-11-02)

The rule (ROUND23.md, "Lift only"): armed humanoids that carry a ranged weapon wear a real backpack; melee-only or unarmed
ones none. Grunts, knights and death knights follow it. AD's own humanoids (AD's variants are never armed:
`VR_AGR_ADVariant`) need arming first (rows, rigs, shells), then the anchor (`agarmor::packRest`), the ragdoll box
(`createRagdollPack`: the rig's `chest` bone) and the loot. Cost: PERF_1102.md measured the worn packs (`limbpiece` 0.97 ms
a frame in E1M1's fight with 29 wearers, cut by about that much with one look a wearer (`box3d::wornPackAll`) and one link a
pack a frame): each wearer costs a little.

### More kinds and stances

- **Crouching and crawling armed humanoids** (Vittorio, 2026-11-05): think about what it would take. A shot leg could cripple them; both legs make them crawl while still shooting or attacking, or retreat if unarmed. Find a generic way that works with all humanoids without tweaking each animation by hand; the humanoid IK must look good crouching and crawling, and their aim must hit the target in any stance. Also consider leaning left and right near corners, and forward and back near vertical ledges, for a better view of the target.
- **Gremlins as armed humanoids** (Vittorio, 2026-11-02): Scourge of Armagon's gremlin already steals the player's weapon and fires it, so an armed gremlin fits its character: a small body with a full-size gun. Check its rig and hands first (a kind's fit on the grunt's keys may not carry over).

## 5. Combat and feel

### Small

- **Shooting yourself in VR** (Vittorio, 2026-11-06): the player can hurt themselves by pointing a weapon at their own limbs or body and firing: shots and blades hit the player's own body (limb hit, blood, damage), headset only.


### Dual-wield parry: crossed swords (the author, 2026-10-18)
With a sword in each hand (e.g. the Death Fury's pair), crossing the two blades in front of the body should be a valid
parry pose; today it isn't recognised. Later, after the AD melee work.

### Embedded blades

- **Embedded blades: finish and turn on** (Vittorio, 2026-11-03): `vr_embed`, off for 1.1. Hard to stick at first and inconsistent, detaches too easily in normal combat, sometimes attaches in the wrong place even on a dummy. Tune with Debug > Embedded Blades: Why in slow motion. Before turning it on: saves, level change, real heavy monsters, multiplayer, `eval`, `vr_knockdown` on (tests 14-20 are written, not run); test 7's and 13's failures.
- **Blades embedded in props** (Vittorio, 2026-11-05): wooden crates and barrels should let blades embed too. Pulling, grabbing or moving either the prop or the blade imparts force on both. First idea: the blade and its prop ignore collision with each other, and a Box3D joint moves them together convincingly while embedded. See how embedded blades in live enemies, active ragdolls and ragdolls handle this, and unify them if appropriate.

### Ideas from Quake Combat+ v5.7 (the author, 2026-10-16; ideas only: no source or assets reusable without its authors' permission)

- **Heart gib you can hold:** a beating, squeezable, throwable organ with a haptic pulse, on top of our physics gibs.
- **Berserk pack:** a level-long melee power state (red tint, bigger gib/decap pops, x2-3 melee damage) rewarding
  fists/axe/chainsaw play; plugs into the existing `vr_decap_pop_*` scale cvars.
- **Hit-confirm audio layering:** a meaty thud when most pellets connect, distinct kick/impact sounds, per-monster gib
  sounds.
- **Wounded monsters bleed a trail, then crawling half-bodies:** live low-health monsters drip decals; big monsters
  split in half and drag the torso with a smear (a "finish it" beat).

### Flies on corpses and gibs (the author, 2026-10-02: "add it to the to-do list")

Scourge of Armagon's head flies (one severed head in ten loops `misc/flys.wav`, player.qc HeadThink) were pointless
as they were, so they are off (`vr_head_flies` 0, Gore > Flies on Heads; NOTES.md vrfiringrange_2026-10-02_01-29-37).
To revisit and make interesting: flies on every corpse and gib (not only heads), arriving after it has lain a few
seconds, with fly particles buzzing round it as well as the sound.

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

## 6. Weapons and content

### Weapons and armour

- **Ammo type follows the loaded magazine:** with pouch reloading (v1.1), the magazine/ammo you load picks the gun's ammo type (e.g. a lava-nail magazine fires lava nails), making the ammo switch button unnecessary for magazine-fed weapons (his idea via a player exchange, 2026-10-16).

### Enemies' own weapons for the player (the author, 2026-10-15)
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

### Bloody shotguns' own look (the author, MG3 decision 4, 2026-10-06: "Backlog TODO: distinct bloody textures/skins")

Dawn of the Machine's bloody shotgun and super shotgun (M3-14, `QC/vr_mg3_weapons.qc`) play as intended (0.28 s
refire; 28 pellets) but are drawn as Quake VR's own shotguns: MG3's `v_bloodshot.mdl`/`v_bloodshot2.mdl` are view
models with an arm, made for the flat view. To do: bloody skins for `progs/v_shot.mdl`/`v_shot2.mdl` (a second skin
picked while the bit is set: `MG3_BloodyBits()`), or MG3's models stripped and laid as the Super Axe's are
(`vr_monstermods.cpp superAxePoses`), with their own weapon settings slots.

### Native AD inventory and interface (author, 2026-10-12 11:27:41)

Audited and the gadget's AD page added (ROUND22.md, same section). Left: the custom keys' names on it (strings, not
stats), the jump boots' power level, coop.

### Reliefs

- Expansion textures missing reliefs: generate them like AD's.
- **AD relief authoring pilot:** the pilot of 10-20 textures (reviewed height masks, `_bump`/`_norm` sidecars) is not
  started.

### Art (ROUND21.md, "For the author": Blender)

- **Stretched UVs left** (ROUND21.md, "Patch 1.0.1: stretched skins re-mapped"): the grappling hook's claws (10% of it:
  their strips do not unwrap flat), the rocket launcher's and plasma gun's few faces whose strips would not unwrap
  flat; the same few on the lava super nailgun and multi rocket launcher (re-mapped too: ROUND22.md, "Patch 1.0.1:
  the lava super nailgun and multi rocket launcher re-mapped").
- **Body skins' texel density at the wrist**: about a sixth of the hands' (256 x 256 skins). The skins are the
  author's own repaint (commit 24fa2de6d): a re-map or a bigger skin only resamples his paint; a sharper sleeve needs
  him to paint the wrist and bracer at a higher resolution (or make_vrbody.py's painter at 512, losing his repaint).

## 7. Refactors, right after 1.1

In this order (pure refactors; waiting avoids conflicts with beta fixes and keeps refactor risk away from a release):
1. Formatting: clang-format (Zancle configs) on all first-party code, `// clang-format off/on` around hand-aligned
   tables, then clang-tidy fixes; one commit listed in `.git-blame-ignore-revs`. Never Ironwail's code.
2. Ironwail extraction: about 1,600 lines of VR code moved out of Ironwail files into `Quake/vr/` (pure moves), the VR
   main menu out of `menu.c`, the VR heap macro remap, `IRONWAIL_DIFF.md` refreshed.
Each verified by an unchanged physics hash, melee eval and `vr_menu_path_check`. Monster setup and the weapon-id tables, once item 3 here, are done.

Then design review P3, P6 part 2, P8, P9, P10 (`scratch/refactor/design_review.md`, QUEUE.md).

### Menus for players

- **Menus and settings for players, not only for tuning** (the author, 2026-10-03; `docs/vr-port/archive/MENU_REVIEW.md`):
  left: gameplay-feel presets (or gameplay kept as one tuned design) and the per-slot cvars (`vr_wofs_*`,
  `vr_prop_*`) to data files.

## 8. Big features

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

### Modelling and body

- **Easier modelling and asset creation** (Vittorio, 2026-11-05): Blender is painful for Quake models: their format needs conversions on import and export, and being vertex based they don't suit usual workflows. Audit what Ironwail supports, find the most convenient formats, add them, and perhaps convert existing assets. (See also the glTF entry.) Started for weapons: the shotgun's source
  .blend and export on save (ROUND24.md, "The shotgun as a source .blend", "Blender: export on save"); the audit and
  the other models are left.
- **glTF (.glb) for the hand and body models, after 1.1** (Vittorio, 2026-11-03): so modellers can work in Blender with no add-on. A small parser (cgltf; ask before downloading) fills the structure the MD5 loader fills; MD5 stays as the fallback; a .glb loads only when present; vr_hand_reload and vr_model_reload read it. Watch: Y-up metres to Z-up Quake units, seams split, 4 weights, bone rest pose and roll against the engine's axis convention, a simple material rule (one base colour texture), skins' damage and armour variants stay separate files. Bone names and grip fits still apply. Weapons and other .mdl models stay out: frame numbers in QuakeC, triangle-keyed masks and anchors.
- **Player lying down and physical crawling** (Vittorio, 2026-11-03): the player can lie down and crawl by moving their body and arms.

### Pillar/pipe climbing (brace a hand on each side) (23:10:39, 2026-10-14)

Research climbing by bracing a hand on each side of a pillar or pipe and moving upward hand-over-hand. Open.

### Rendering

- **Temporal AA, then DLSS/DLAA and FSR 3.1** (`docs/vr-port/TEMPORAL.md`, the author 2026-10-16: "start a worker on
  motion vectors + initial TAA implementation"): stages 2a (motion vectors) and 2b (TAA behind `vr_taa`, off by
  default) landed in a first cut (ROUND22.md, "Temporal AA: motion vectors and a first TAA"); HMD tuning next. The DLSS
  helper still waits on TEMPORAL.md's section 12 questions 1-3 and 5.
- **`.rtlights` support** (round 20 discussion): load DarkPlaces' hand-authored light lists where they exist to
  drive the shadowed map lights (better placed than the map's light entities).
- **Distance-field AO** (round 20 discussion): only if capsule/box AO leaves obvious gaps.
- Foveation check (what `vr_foveated` does; NVIDIA shading-rate in GL) and a rendering-abstraction audit (GL calls
  per file) before any Vulkan decision; vkQuake (GPL-2.0) as the backend reference.

### Look at QuakeRay (the author, 2026-10-19)
Check out https://github.com/f1ames0ff/QuakeRay for information about a Vulkan migration (see VULKAN_MIGRATION.md) and
better lighting effects. Not yet reviewed: licence, base engine and what is reusable unknown.

### Platforms and hardware

- Linux (WSL native build in progress), Steam Frame branches with kblood, standalone Quest (single-pass stereo kept
  for it).
- Standalone Quest version (Android/Quest native build: OpenXR on Android, GLES/Vulkan renderer path, performance
  budget for the mobile GPU, packaging/sideloading) (the author, 2026-10-11).
- bHaptics support (vest, arms, head, hands: hits by direction, recoil, explosions, melee impacts) (the author,
  2026-10-11).

## 9. Performance (when a capture makes it the top cost)

- **Optimise decals** (PERF_DECISIONS.md 15): in a rocket fight (`play_e1m1_lights`) ~900 marks are 1.34 ms of the
  world pass's 2.67 at 2048 per eye, about half (~2.4 ms at his 2782 eyes); each pixel walks its cell's bucket (up to
  64 marks, a whole 80-byte record each before its early-out). Reading a mark's normal and middle first was tried and
  reverted (no gain, 0.6-0.9% of the pixels changed). `vr_decal_max` lowered to 768 meanwhile (config version 118).
  Left: the options with a visible change (PERF_DECISIONS.md 15), for him.
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

### Loading and builds

- **Model loading in parallel** (the calibration room's alias models, now 270 of the 430 ms: texture uploads, skins'
  decoding and copies, md5 replacements): the loaders share the hunk, cache and GL; would need each loader split into
  a decode into its own buffers (on the pool) and a commit in order on the main thread. Two exact pieces done instead.
- **Drop the remaining kept copies** (ROUND21.md, "The hull build's memory"): the last level's kept copies, 550 MB at
  once without the budget (about 240 with it), could go as the levels' between did.

## 10. Parked and tooling

### Stealth AI: limits (STEALTH.md; open since it was built, 2026-10-08)

- The meter is one per monster, on the most suspicious player in its sight (not a meter per player: the dark player's
  own suspicion isn't kept while the lit one holds it).
- A client's light and lamp are his own client's word (sent in his VR move): a modified client could send "dark".
- Alert monsters walk with Quake's movetogoal (no path finding): a point across a gap or up a ledge ends their walk when
  stuck (3 s without headway), then they search where they are.
- Quake's own relay stays: an idle monster that sees another turn Hostile (FoundTarget's `sight_entity`, a tenth of a
  second) turns Hostile at its enemy too, whatever the light on him (as `vr_stealth_share_*`, but with Quake's sight
  ranges).

### Dawn of the Machine: co-op and dm1 (MG3.md, decision 6, 2026-10-06)

MG3 ships single-player first (`soloOnly` 5, as Dopa); its co-op and its deathmatch map (dm1) are for later.

### Old QuakeC notes (the old engine's "TODO VR" markers, triaged 2026-10-09: TECHDEBT_2026-10-09.md 10)

The notes about organizing, hitboxes and ideas were dropped; these name real, small issues:
- **Rogue's teamplay variants** (`rogue_teamplay.qc`): their item and weapon management is commented out (never
  rewritten for hands and holsters), so `teamplay 3`-style Rogue modes don't work in VR; their impulses
  (`weapons.qc`) are off too.
- **`readytime`** (`vr_defs.qc`): one for every client (no shooting or taking weapons just after a load or a
  spawn); in co-op one player's spawn holds the others for that moment.

### Tooling

- Tracy GPU zones lost after toggling GPU detail mid-run.
- 11 test registrations left in `vr_main.cpp` (diag moves; it was about 55).
- **`vrtutorial_playtest.py` room 8**: the mock's hand loading often throws the shells instead of loading them, so the
  range's door stays shut and every later gate fails (the arena is run with `--from arena`).
- **Hidden mock blank readback:** some isolated eye/window captures are entirely black even with flat lightstyles;
  investigate capture/target validity separately if needed. This is not evidence of the reported lamp flicker.
