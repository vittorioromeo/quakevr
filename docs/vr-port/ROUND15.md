# Feedback round 15: plan and notes

From the fifth batch of voice notes (21 notes, two profiles).

| # | Note | Request | Status |
|---|---|---|---|
| 1 | r1m1 02-16-40, vrfiringrange 02-12-19 | Slower on every load (120 to 80 fps) | nothing leaks on our side; `vr_memstats`, `vr_memstats_log` |
| 2 | e1m1 01-52-30 | Light fixtures should light more, customisable | done: relight, `relight_textures.cfg` |
| 3 | e1m1 01-54-54 | Gib corpses with normal damage | done |
| 4 | e1m2 01-58-37 | Holster models not visible | done: the body hid them |
| 5 | e1m2 01-59-00, 02-06-13 | Splashes and water sounds | done |
| 6 | e1m2 01-59-00 | Geometric waves | done |
| 7 | e1m2 02-03-41 | Holster "once" haptic a longer burst | done |
| 8 | e1m2 02-04-12 | Haptic when catching force-grabbed things | done |
| 9 | e1m1 01-47-01 | No force-grab lines while holding the flashlight | done |
| 10 | r1m1 02-14-49 | Sword hits register near the hilt | done: swept points along the weapon |
| 11 | r1m1 02-15-54, vrfiringrange 02-24-03, 02-24-15 | Two-handed swords; both in vrfiringrange | done |
| 12 | start 02-21-12 | Line artifacts up close on bumpy walls | done: the baked-light bumps' gradient |
| 13 | start 02-21-59 | Shell casings | done: shotgun, double shotgun on reload |
| 14 | vrfiringrange 02-26-50 | Batting projectiles more lenient, with options | done |
| 15 | vrtutorial 02-17-27 | Map boards like the gadget's CRT | done |
| 16 | vrtutorial 02-18-26 | Throwing and melee independent of the frame rate | done |
| 17 | vrfiringrange 02-24-38 | The tuned settings as shipped defaults | done: `quakevr/vr_defaults.cfg` |

## Shipped defaults

`quakevr/vr_defaults.cfg` (executed by `default.cfg`, so the saved config still wins) holds the settings the author
tuned, as `vr_default <cvar> <value>` lines: each sets the value and makes it the cvar's default, so the menus' resets
and the presets return to it. `vr_savedefaults` rewrites the file from the current settings: the archived Quake VR
cvars that differ from the compiled-in defaults, less the personal and bookkeeping ones (height calibration, OpenXR
runtime, microphone, per-weapon offsets, version counters). Configs saved before keep their values (they save every
archived cvar); new players and "Reset to defaults" get the tuned ones.

## Memory log

`vr_memstats_log` (Graphics > Performance > Memory Log; 60 s by default, 0 off) writes a row to
`quakevr/profile/memstats_<date>.csv` every so many seconds and 5 s after each map load: the frame time since the last
row, VRAM used and free (all processes) and the driver's evictions, the working set, private bytes, hunk, managed
textures, live GL objects and render targets made. The console command `vr_memstats` prints the same.

## Leaks

Voice notes r1m1_2026-09-26_02-16-40 ("slower and slower on every load ... 120 at the beginning, now around 80") and
vrfiringrange_2026-09-26_02-12-19 ("goes a bit slower as time goes on").

**What the profiles say.** Comparing `profile_e1m1_2026-09-26_01-42-10.csv` with `profile_e1m2_2026-09-26_02-03-40.csv`
(same session, 3292x3524 eyes), the game's own work did not grow: the eyes' GPU time went from 2.3 / 1.9 ms to 2.1 /
1.7 ms, world+brush 1.6 to 1.4 ms. The only scopes that grew are the runtime's: `xr submit` (xrEndFrame) 6.6 to 8.0 ms
on the CPU, and its GPU span 3.2 to 5.0 ms; `xr acquire`'s GPU span 0.35 to 0.56 ms. The frame period went from 8.37
ms (120 Hz) to 9.9 ms.

**What SteamVR's logs say** (`Steam/logs`, the 01:35 - 02:27 session): SteamVR made the three swapchains once (two
eyes and the panel) and never again, so the game did not pile up swapchains in the runtime. The compositor logged
`WaitForAcquire timed out (FAILED); rendering the next frame before the driver took the sync texture` at 01:38 (a
burst), 01:44, 01:47 and 02:09. This means the headset's driver (Virtual Desktop's streamer) was late taking the
frames. Over the whole session, 46228 of 362992 frames (13%) were reprojected, with 0 dropped.

**Measured on our side.** I added `vr_memstats`, which prints:

- the GPU's memory, all programs', through GL_NVX_gpu_memory_info, with the driver's eviction count;
- the process's working set and private bytes;
- the hunk;
- Ironwail's managed textures, how many of them are normal maps, and their megabytes;
- every live GL object (textures, buffers, framebuffers, queries, programs), found by probing names, so it covers
  the engine, the module and the runtime's swapchain images in our context;
- how often VR render targets were remade;
- the decal count;
- the average frame time since the last call.

I ran it on the mock backend with the author's graphics cvars (all `vr_*` / `r_*` / `gl_*` from his `ironwail.cfg`:
normal maps, parallax, 8192 shadow atlas, 12 dlight shadows, water effects, bloom, decals, flashlight, profiler on).
Runs:

- **Map loads:** 12 loads cycling e1m1, e1m2, vrfiringrange and r1m1, firing the super nailgun and rockets on each.
- **Other ways of reloading:** 3 `restart`s, `changelevel`, `save`/`load` ×3, `vr_restart` ×3, `vid_restart` ×2,
  and `vr_render_scale` 0.8 → 1.
- **Single-map soak:** about 2.5 minutes of continuous firing on one map.

Results (the numbers after the second load of each map are identical on every later load):

| after loading | textures (normal maps) | GL textures / buffers / framebuffers / queries | working set |
|---|---|---|---|
| e1m1, 1st | 508 (221) | 542 / 155 / 24 / 840 | 171 MB |
| r1m1, 1st | 565 (250) | 605 / 199 / 30 / 840 | 195 MB |
| e1m1, 2nd / 3rd | 558 (246) / 558 (246) | 598 / 199 / 30 / 840 both | 195 / 195 MB |
| r1m1, 2nd / 3rd | 565 (250) / 565 (250) | 605 / 199 / 30 / 840 both | 196 / 197 MB |

- **Across map loads:** VRAM stayed at 6.26 - 6.32 GB for all programs, with 0 evictions. Frame time on the same map
  stayed flat, 4.08 - 4.14 ms at a 250 fps cap.
- **The first vrfiringrange load:** it adds 6 textures and 6 framebuffers not managed by Ironwail, made once and
  kept (VR render targets). The VR render targets are remade about 3 times per load: the ammo screens change size
  with the weapon, and each old one is deleted.
- **Single-map soak:** decals grew to 286 (capped at `vr_decal_max`; they expire after `vr_decal_life`). Frame time
  stayed at 4.00 - 4.01 ms, and the GL counts did not change.
- **Why the second load of a map is higher than the first:** the extra textures, buffers and RAM are models and sounds
  that stay cached after the first maps (Ironwail keeps alias models across maps). They settle and stay flat from
  then on.

**Leaks and fixes**

- **GPU timer queries piling up while a dialog is open** (`vr_main.cpp`, `VR_ModalMessageFrame`). With `vr_profile`
  on, a yes/no dialog shown in VR ("Load last save?", "start a new game?", `vid_test`) redraws at headset rate but
  never ended a profiler frame. Every redraw kept adding GPU timestamp queries, 64 more at a time, and scope records,
  for as long as the dialog stayed up. The dialog frames now go through `VR_ProfileFrame`. This only matters with the
  profiler on, and does not explain the voice notes.
- **Water refraction: a stale scene size** (`vr_water.cpp`, `sceneDistances`). This was a correctness bug, not a
  leak. The size of the scene depth it reads was cached by texture name. GL reuses deleted names, so after the eye
  framebuffers were remade at another size (`vr_render_scale`, a window resize), the half-size distance target could
  keep the old size. It is now re-asked when `vid`'s size changes.
- **Checked and fine:**
  - Normal and height maps take their base texture's owner, so they are freed with the map's textures, and
    `TexMgr_FreeTexture` clears their bookkeeping.
  - Water caustics volume (one texture, re-uploaded per map).
  - Refraction distance targets, bloom targets, shadow atlas and spot tiles, stereo resample and eye framebuffers:
    each deletes the old object before making a new one.
  - Ammo-screen and gadget targets: 4 + 1 fixed slots, remade only when the text's size changes.
  - Particle and decal atlases: made once.
  - Panel and eye swapchains: made once a session.
  - Profiler rings.
  - The CPU-side containers are being audited separately. One thing to watch: `VR_TouchLinks` (vr_physics.cpp)
    scans every edict for each mover, so its cost grows with `num_edicts` within a map.

**Conclusion.** Nothing on our side grows with map loads or with time played. The slowdown is downstream of
xrEndFrame: the runtime or the streamer took longer to take each frame, while the game's own GPU work stayed the
same. Checks for the author:

1. **Restart only Quake VR once it has slowed down.** If the frame rate is back at 120, it is the game: send
   `vr_memstats` output from before and after. If it is not back until SteamVR or Virtual Desktop is restarted too,
   it is them.
2. **Virtual Desktop.** 3292x3524 per eye is about 2.5 times the Quest 3 panel's pixels (VD's quality preset times
   SteamVR's resolution), streamed at 120 Hz. The
   encoder load and VD's "network latency" / "decoder latency" overlay are the first things to look at. Also look at
   VD's automatic bitrate and its "Adjust bitrate automatically" option, and try 90 Hz or a lower VD quality preset.
   The compositor's "driver took the sync texture late" warnings point at the streamer.
3. **SteamVR's resolution** (per app: set it to 100%). Motion Smoothing. Its frame timing graph, looking at whether
   "Other" or the compositor grows.
4. **Heat and power:** GPU clocks and temperature over the session (HWiNFO / GPU-Z). The Quest's own battery and
   heat throttling is another candidate.
5. **The build:** the logged session ran `C:\OHWorkspace\quakevr-iw\...\ironwail.exe`, not the cleanup worktree's
   build.

**How to use `vr_memstats`:** type it in the console at the start, then after each map load (or bind it to a key).
With `-condebug` the lines go to `qconsole.log`. If the "textures", "GL" and "RAM" lines are the same on the second
and later loads of the same map, the game is not leaking. The "VRAM" line counts all programs, so it also shows
SteamVR's and Virtual Desktop's memory growing. A rising eviction count means the GPU is out of memory. See
TESTING.md, "Profiling".

## Corpses and haptics

Voice notes e1m1_2026-09-26_01-54-54 (gib corpses with any weapon, not only rockets), e1m2_2026-09-26_02-03-41 (the
holster haptic in "once" mode is too brief to notice) and e1m2_2026-09-26_02-04-12 (haptics when catching a
force-grabbed object).

**Corpses can be gibbed** (`vr_corpse_gib`, default 1; `vr_corpse_health`, default 40; menu: Throwing and
Physics, under the gib settings: "Gib Corpses" and "Corpse Health"). In stock Quake a corpse takes no damage at all. A rocket "gibs a corpse" only because its
100+ damage kills the monster far below its gib threshold, so it never becomes a corpse. Now it works like this
(QC/combat.qc, after `Killed`):

- **When a monster becomes a corpse.** A monster that died whole is watched (`VR_Corpse_Watch`, from `Killed`). It
  becomes a corpse once it is non-solid, its death animation is over (the frame stops changing) and it is at rest.
  So its backpack and weapon have already dropped, and the killing blow's push has already landed.
- **Its box.** The corpse is `SOLID_NOT_BUT_TOUCHABLE` and `DAMAGE_YES` (no autoaim), so it never blocks anyone.
  Shots and missiles stop at it, as they do at gibs (the engine's `MOVE_HITGIBS`). Its box lies on the floor: 30% of
  the monster's height (12 to 24 units), and wider than the standing box (0.4 of the height, at most 16 units more
  on each side), trimmed where a wall is near. A floating fish or eel gets a box round its middle instead.
- **What reaches it:**
  - shotguns and the super shotgun, nails, lightning;
  - rockets and grenades, direct hits and splash;
  - the lightning discharge;
  - fists and anything held in the hand (`VR_Corpse_StrikeFrame`, from `VR_Carry_HandFrame`). A hand swung at
    `vr_melee_speed` or faster through the corpse, or the line from the hand to the held weapon's end, strikes it.
    The damage is the same as a gib strike, ×1.5 with a weapon in hand. It plays a hit sound, spurts blood and gives
    a haptic. The regular melee trace (weapons.qc) passes through touchables, so this is a separate check.
  - Quad damage counts. Positional damage's leg and limb reduction is given back (`VR_Corpse_PositionalFactor`):
    for positional damage, a corpse's low box counts as all legs and limbs.
- **How much damage it takes.** `vr_corpse_health` times the monster's bulk (its box's size compared with a grunt's,
  from 1 to 2 times). With the default 40, a grunt corpse takes 2 shotgun blasts, 5 nails, 2 lightning hits, 1 good
  punch or 1 rocket.
- **What gibbing looks like** (`VR_Corpse_Gib`). The corpse is gibbed the way its own death code's gib branch would
  gib it: the same head model and gib models (statues throw stone gibs), the gib sound, and a blood mist. The mist
  is `VR_BloodMist`, split out of the gib burst in vr_carry.qc. The corpse entity becomes the head, which you can
  grab and burst like any other. A fish, which has no head, just goes. A hard blow (60 damage or more) throws the
  gibs further.
- **What doesn't happen again.** Its damage never reaches Quake's death code: `T_Damage` hands it to
  `VR_Corpse_Damage`, as it does for gibs. So nothing is counted, killed or triggered twice. The killing blow's push
  (`vr_kill_push`) is not applied to corpses. Corpse removal (`monster_check_remove_corpse`) works as before.
- **Which monsters.** Only the monsters whose death code can gib them. They are recognised by their `th_die`: grunt,
  enforcer, dog, fiend, ogre, knight, hell knight, vore, shambler, scrag, rotfish, gremlin, scourge and eel. Bosses
  and special deaths are left alone: Chthon, Shub-Niggurath, the dragon, Armagon, the lava men, wraths, zombies and
  mummies (these always gib), and spawns (these blow up).
- **Engine** (Quake/world.c). Missiles meet monsters with a fatter box (±15 units), but not touchable ones: a corpse,
  lying low, would otherwise stop nails flying well above it.

**Holster haptic, "once" mode** (QC/weapons.qc, `VRHolsterHaptic`). It used to be one 0.2 s pulse at amplitude 0.2,
barely there. It is now a burst of `vr_holster_haptic_time` seconds (new, default 0.25; menu: Immersion,
"Holster Haptic Time"). It is strong as the hand arrives and fades out. It is made of three pulses (0.65, 0.4 and 0.2
amplitude), each taking over from the one before, since a new vibration replaces the one playing.

**Force grab catch haptic** (QC/vr_wpnforcegrab.qc, `VR_Forcegrab_Catch`). There was a catch haptic, but it was never
felt. The catch runs in the flying object's think, and `haptic()` goes to `self`, which was the object, not the
player. It is now sent as the player, after the handover (so it replaces a pickup's own haptic): a firm 0.07 s
knock at 0.85 amplitude, then a 0.08 s fade at 0.4. The pickup or carry sound that follows the catch already plays,
so no sound was added.

**Tested with the mock** (`developer 1` prints each step):

- **Shotgun on the e1m1 grunt** (edict 31). Killed whole, it lay still in about a second, with 40 to go
  (`corpse: monster_army lies still`). Two more blasts of 24 each gibbed it, and the head became a grabbable,
  destroyable gib.
- **Nailgun:** 5 nails of 9 each.
- **Lightning:** 2 hits of 30. The kill count stayed at 1.
- **Rockets:** a grunt spawned with `impulse 244`, killed with nails, then gibbed by one rocket's direct hit (108).
  The kill count was 2, one per monster.
- **Fists:** a mocked downward swing through the corpse struck it for 60 and gibbed it.
- **Holster haptic:** a sweep of the hand through the holsters printed `holster haptic: ... 250 ms` once per entry,
  both holding the shotgun (the empty right upper holster) and with an empty hand (the right hip's weapon).
- **Force grab:** a backpack pulled and caught printed `force grab: catch haptic, hand 1`.

**To check in the headset:**

- **Corpses:**
  - Shoot, nail and punch corpses of several monsters. Check that shots at a corpse hit it (blood, then gibs) and
    that shots just over it pass.
  - Check that the corpse never blocks walking.
  - Check that the gibs and head look right for each monster.
  - Check that a corpse by a wall is still hittable.
  - Tune `vr_corpse_health` to taste.
- **Holster haptic:** in "Once" mode, check that the buzz is clearly felt and not too long. Try `vr_holster_haptic_time`
  0.15 to 0.35.
- **Force grab:** check that the catch is felt in the catching hand, and not doubled by a pickup's buzz.

## Light fixtures

Voice note e1m1_2026-09-26_01-52-30 ("these lights ... should emit a bit more light ... make it customizable ...
detect what sort of textures or props should emit light ... right now they look like they are off").

**What the fixtures were.** The lamp in the screenshot is one of e1m1's four lanterns in the pool room by the
slipgate: texture `tlight11` (16x64, 17% fullbright), a lamp head that is a `func_door` (`*34`-`*38`) rising out of a
world post. Its own light, `light` at 784 2496 -280 (300, style 36, `targetname t14`, `spawnflags 1`), starts off and
comes on when you walk through `trigger_once *33`; until then the lantern looks lit (its texture is fullbright) and
lights nothing, and after it the room stays dim.

**Why it lit nothing.** `glow_lights` gave fixtures (textures named `*light*`) half a glowing texture's budget, shared
by all of its faces in the map: 300 x 0.74 x 0.5 / 67 faces = 1.6 for `tlight11`, under the cut-off of 12. So no light
was made. The same was true for every `tlight*` texture in e1m1 (`tlight02` 4.8, `tlight07` 9.6, `tlight01` 4.2),
and, as far as I can tell, in every map: fixtures never got any light of their own. Fixtures whose 8-bit textures have
no fullbright pixels at all (`light1_*`, `light3_*`, `tlight05`/`09`/`10`) were skipped entirely, even though QRP's
`_luma` images make them glow in game.

**What changed** (`relight_maps.py`, new `Misc/quakevr/relight_textures.cfg`):

- **Fixtures light on their own.** A small fixture (a lantern, a panel) gets one point light: in front of it, or,
  for a lantern whose sides face every way, on top of it. Several lights in one spot would add up to several times
  the light, as the first try showed (the room blown out). A big fixture (a long strip, a lit floor) gets one light
  every 128 units, each divided by the square root of their number, down to 0.4.
- **Brightness.** 250 for a lone fixture (`--fixture-scale`). Half of that when a mapper's light of 300 or more is
  within 128 units and on from the start (`--fixture-lit`); a weaker light reduces it less, and a "start off" light
  does not count. A little less for a small or faintly glowing fixture. Divided by the square root of the fixtures in
  its room.
- **No ambient occlusion on fixture lights** (`"_dirt" "-1"`). With it, e1m6's recessed ceiling lamps kept about a
  fifth of their light.
- **Luma detection.** Where a texture has too few fullbright pixels, the glowing part of its replacement's `_luma`
  image decides what glows. The lookup is the engine's: `textures/<map>/`, then `textures/`, in the game, then id1.
  QRP's lumas are dim (`tlight10`'s peak is 64), so the glowing part is whatever is at least half the peak. This finds
  `light1_*`, `light3_*`, `tlight05`/`09`/`10`, shootable and floor buttons, key doors and stained-glass windows.
  `--no-luma` turns it off.
- **Per-texture settings.** `relight_textures.cfg` marks what is a fixture (`tlight*`, `light1_*`, `light3_*`,
  `ceil1_1`, `sfloor4_4`, rogue's `metal8_3`/`metal9_2`), what only glows (`sliplite`, `metal5_8`, `tech03_2`) and
  what is nothing (`tlight08`, plain trim). Settings: `kind=fixture|glow|off`, `scale=`, `light=`,
  `color=r,g,b`, `reach=`. Patterns can be per game or map (`e1m1/tlight11 scale=1.5`, `hipnotic/*/light3_1`), and the
  file is documented at its top. A texture you name a fixture that has nothing glowing takes its brightest pixels'
  colour.
- **`--list-glows`** prints each map's glowing textures, where the glow comes from (fullbright, luma or brightest),
  the kind, the lights and their range, without relighting.

**Result.** 73 maps relit, 3911 glow lights. Every fixture texture now gets lights, for example `tlight02` 329 of
them (up to 173), `tlight11` 80 (up to 235), `light1_4` 101. The relight was rerun into this worktree's
`quakevr/relit` and into `C:/OHWorkspace/quakevr-iw/quakevr/relit`, with `--vis-dir` (water-vis intact:
`vis_maps.py --check`). Mean brightness, flat view, lamps triggered:

- e1m1 lantern at the note's position: 12.5 → 30.2.
- e1m1's second lantern: 15.6 → 26.8.

**Second pass: the two cases that still looked off.** These were e1m6's ceiling lamps (`light1_4`) and e2m1's
`ceil1_1` column.

- **Recessed lamps** (`recess_depth`). e1m6's lamps sit in coffers with a lip about 10 units deep. A light 4 units
  under the face was inside the lip, which shaded the hall. A fixture's light now steps out along the face's normal
  until the recess walls end, then goes 4 units further, at most 48 (`RECESS_MAX`).
  - A recess is solid just outside two opposite edges of the face.
  - One wall beside the lamp, or two walls meeting in a corner, do not count as a recess, so a lamp by a corner stays
    at 4 units.
  - e1m6's lamp light moved from 4 to 14 units under its face. e2m1's `ceil1_1` column, which sits in a narrow slot,
    moved out 48.
  - Mapper lights that start off still do not count in the "lit already" reduction. That is why e1m6's hall lamps,
    whose room lights (`t34`) start off, get nearly full light.
- **Rules where the general fix was not enough** (`relight_textures.cfg`):
  - `light1_4 scale=2`: big blue bar panels in coffers. Their hall's own lights start off, and the coffer keeps much of
    the light in.
  - `ceil1_1 scale=2`: small lamps, often several in a column (e2m1), so the room share divides each by a lot.
- Rerun: only the maps whose lights changed were relit, 25 then 6 (`e1m6 e1m8 e3m2 e3m3 e3m4 r1m5` for
  `light1_4`), into both folders with the same `--vis-dir`.
- Mean brightness at the same views (before → first pass → second pass):
  - e1m6 lamp view: 4.2 → 4.4 → 10.0 (blue light on the coffer, the ceiling and the walls round it).
  - e2m1 column: 11.1 → 11.3 → 14.9.
  - e1m1: unchanged (30.2, 26.8).
- e1m6 seen from across the hall stays dark (4.3): until its trigger lights `t34`, that hall is meant to be dark, and
  only the lamps' own light reaches there.
- **A size bug found on the way** (`water_vise`). Patching water-vis on a map that was already up to date appended the
  visibility and leaf lumps again, so every rerun grew each relit `.bsp` by its visibility (e1m2: 1.45 → 1.53 → 1.61
  MB). `water_vise` now skips a map that has the patch already and writes it packed (`packed`: its lumps one after
  another; relit maps 150 → 140 MB). Reruns are byte-stable now: both folders were rerun and report "73 up to date"
  with identical files.

**Quake VR's own maps** (`relight_quakevr_maps.py`, which supports `--out`):

- **vrtutorial** was relit with the new fixture code: 21 glow lights before, 52 now. Its `tlight11` lamp posts get
  52-150 and its `tlight02` strip lights 25-83, halved by the tuned lamps beside them. It was written to scratch,
  copied into this worktree's `quakevr/maps/vrtutorial.bsp` and `.lit` (committed files, now modified) and into
  `C:/OHWorkspace/quakevr-iw/quakevr/maps`.
- Checked at four views: start room 55.1 → 55.5, dark corridor by the lamp posts 3.8 → 6.0, board corridor
  40.3 → 40.5, courtyard 86.0 → 88.0. Slightly brighter round the lamps, nothing blown out.
- **vrfiringrange was not rerun.** It has `"glow": False` (lit by sun and sky only; all light entities dropped), so
  the fixture code does not touch it: a rerun would give the same lighting. Its `.ent` also has another agent's
  uncommitted edit (two `func_weapon_grabbable`), which a rerun would rewrite.

**How to customise.** Edit `Misc/quakevr/relight_textures.cfg` or pass `--fixture-scale 1.5` / `--fixture-lit 0.7`.
Then run
`python Misc/quakevr/relight_maps.py --quake "<Quake>" --light <ericw light.exe> --vis-dir <vispatch folder>`
(add `--only e1m1` to try something). Only maps whose lights changed are relit, about 4 s each. Reload the map. To see
what a map has before changing anything: `... --list-glows --only e1m1`.

**In game.** Baked light cannot change without a relight. The fixtures' own glow is bloom, which is already tunable:
Graphics → "Bloom: White Lights" (`vr_bloom_white`, 0.5) for white and pale lamps like the lanterns, and "Bloom:
Coloured Lights" (`vr_bloom_color`) for coloured ones. There was no engine change.

**Not done (ideas).**

- **Torches and flames** (`progs/flame*.mdl`) are `light_torch_*` / `light_flame_*` entities with their own baked
  light. A dynamic flickering light per flame model, in the style of `vr_emissive.cpp`'s projectile lights, would make
  them feel alive.
- **Fixture glow following the switch.** A fixture's glow could follow its "start off" light's style, so that
  e1m1's lanterns are dark until triggered. Today the glow is always on, matching the always-bright texture.

Images (scratchpad, `fix/`):

- `fixtures_before_after.png`: first pass, before/after.
- `fixtures_v2.png`: before, first pass and second pass at the five views (the e1m1 note view, e1m1's second lantern,
  e1m6 twice, e2m1).
- `vrtutorial_before_after.png`.

## Holsters, grab lines, billboards

Voice notes e1m2_2026-09-26_01-58-37 (holster models not visible), e1m1_2026-09-26_01-47-01 (force grab lines
while holding the flashlight) and vrtutorial_2026-09-26_02-17-27 (map boards as CRT screens).

**Holsters.** The holsters were still drawn every frame (`vr_dumpview` lists the four slot models and the holstered
weapons as visible). The body was hiding them. Their positions are the old engine's: a hand's width behind the
eyes, at the hips and at the chest, made when there was no body. The torso (vr_body_mode 2 and 3) is about as wide,
so the chest covered the hip and upper holsters when looking down. With body mode 0 they showed up at once in the
same view. The body mesh and the offsets have not changed since round 11, so this was not a recent code change.
The torso offset in the config (`vr_body_torso_back 0.07`) and the full body are what the notes show.

The second cause: `hands::bodyAnchor`, which places the holsters, the shoulder stock and the chest anchors, measured
from the middle of the player's box. Since leaning (vr_lean_radius, round 10) the head, and the body drawn under it,
can be up to 12 units off that middle. The holsters stayed behind until the body slid back under the head.

Fixes:
- `bodyAnchor` (and the body yaw's chest estimate) now measure from under the head (`playerOrigin + lean`), like
  the hands and the body.
- `vr_body.cpp` `onTheBody`: while a body is drawn, the hip and upper holsters come forward onto the front of the
  body (the belly, the chest's front corners), 1.5 units clear of its surface. From make_vrbody.py's rings, the
  build, `vr_body_eye_forward/_up` and `vr_body_torso_back`, they move further forward as needed to be seen past the
  chest, by at most 2.5 units more. The hotspots use the same positions, so reach matches what is drawn. A larger
  `vr_hip_offset_x` / `vr_upper_holster_offset_x` still moves them further forward. With the author's config the hips
  moved 3.6 units (11 cm) forward and the upper holsters 4.8 units (15 cm). Both are inside the 6.5 unit reach, so
  the old reach still takes them.

**Grab lines.** The QC showed the aim beam (plus the target glow, eligible haptics and sparkles) for any empty
hand whose grip was not pressed. The flashlight takes the grip, so the server saw an empty, open hand. The move now
carries `QVR_BUTTON_OFFHANDBUSY` / `_MAINHANDBUSY` (`flashlight::holds`). The server turns them into `.vrbits0` bits
17 and 18 (`QVR_VRBITS0_*HAND_BUSY`), and `VR_Forcegrab_HandFrame` treats that hand as busy: no aim, no lock. The
trigger was already the flashlight's. `vr_fgfx.cpp` also drops that hand's target on the client, so nothing shows
during the round trip.

**Billboards.** `vr_worldtext_crt` (default 1; a strength, 0 = the old plain text, up to 2) and `vr_worldtext_hue`
(default 40, amber). Menu: Immersion Settings, "Map Boards as CRTs" and "Map Board Hue". Each world text is a small
monitor:
- a dark bezel box round its text block;
- the face is the board's own image through `Shade::Screen`: scanlines, grille, flicker, static and glowing text
  (`vr_screen_text_glow`), in one phosphor colour with the gadget's brightness and background;
- the soft edge glow (`vr_screen_glow`).

The image is a render target sized by the text (6 font pixels of margin, 8 texels a font pixel, fewer for very big
boards, at most 1.5 M texels). It is redrawn in the 2D pass only when the text or the palette changes. A banner's
pages differ in size, so a board keeps the size of its largest page so far. It grows once as the pages first come
round, and each page turn glitches it briefly. Each board glitches at its own moments (offset by its handle). Seen
from behind, it turns round like the plain text did. For distance, `Shade::Screen` now samples a mip level one step
sharper than the usual when the texels are much finer than the eye's pixels, instead of always level 0. This stops
far boards shimmering; the gadget and the ammo screens, seen close, are unchanged. The scanlines already fade out
when finer than a pixel. Button labels in vrstart become small screens too.

Tested with the mock (scratchpad `visfix_*.png`):
- `visfix_h7.png` (before) and `visfix_h9.png` (after) use the author's config, looking down 70 and 85 degrees.
  After the fix, the hip weapons and the upper holster plates show in front of the vest, also with `torso_back 0`.
- `visfix_f1.png`, vrfiringrange: the off hand aims at a weapon (beam and glow), takes the flashlight (the beam is
  gone, the main hand's remains), and lets go (the beam is back).
- `visfix_b1.png`, vrtutorial at the note's spot: CRT on and off, and the second page. vrstart shows boards across
  the start area.

In the headset:
- Look down with the full body. Are the holsters easy to see and reach where they are drawn? If the upper ones
  feel too far forward, lower `vr_upper_holster_offset_x` does not move them back while a body is drawn, so say so.
- Take the flashlight and point it at a weapon or a box: no beam, no buzz.
- In vrtutorial, check that the boards read from across the room without shimmering, that the page turns look
  right, and whether amber is the colour you want (`vr_worldtext_hue`).

## Two-handed swords

Voice notes r1m1 02-15-54 (hold the sword with two hands: the main hand up near the guard, the other below it)
and vrfiringrange 02-24-03, 02-24-15 (both swords, the knight's and the hell knight's, in the firing range, both
two-handed).

**Grip.** The main hand now closes just under the crossguard (it held the grip's lower part, about 3.5 cm below
the guard). The grip is longer so a second hand fits under the first: `Misc/quakevr/make_swords.py` has
`GRIP_TOP` 12.5 (the guard's foot along the axe's handle, just above the axe's hand) and `GRIP` 24 (was 15.3). The
grip now reaches past the handle's bottom, and the pommel follows it. It's about 30 cm long at the default scale
(each fist covers about 14 cm of it). The blade, guard and pommel are unchanged, and so is the vertex order, so
the tip anchor (4) still holds. Slots 18 and 19 in `vr_weapons.inc`: `Offset` makes up for the models' new
`scale_origin` (offset += (1 - scale) x (old origin - new origin)), so the models sit where they did and only
the grip moved. The hand anchors are 142 and 146 (a vertex of the grip's middle ring, where the hand is). The
visible hand is still at the controller. `vr_wofs_version` 5 resets both slots once.

**Two hands** (`Quake/vr/vr_twohand.cpp`, new weapon `TwoHMode` 3, "sword"):
- Hold the sword, then close the other hand on the grip **below** the main hand, within about 17 cm of the spot
  (the grab button, as for a gun's foregrip). It holds while you keep the grip and stay within 60 cm. The spot is
  the "fixed" 2H point (`TwoHDisplayMode` 1). That's anchor 143/147 (the grip's bottom ring) plus
  `TwoHFixedOffset`: one fist-width (11.5 model units) under the main hand, mirrored as the left hand is. The
  helping hand is drawn there, closed on the grip, turned like the main hand.
- The blade then lies along the line from the lower hand through the upper hand. The main hand leads and stays
  put; its angles turn by the least rotation that puts the blade on that line, blended in over 0.2 s. The weapon's
  `TwoHPitch`/`TwoHYaw` give the blade's direction in the model (70 degrees up from its forward). The blade is
  computed as the view draws it (the hand's angles plus the weapon's, through `R_EntityMatrix`), so the fit is
  exact at any wrist roll. The angles add as Euler angles, so the blade's angle to the hand isn't rigid: it
  measured 63 to 77 degrees depending on roll.
- Unlike a gun's grip, it holds when the blade touches a wall or the floor. Dropping it would snap the sword back
  mid-swing and cut the stroke short. The hands are still kept out of walls. There's no virtual stock.
- The hands' line must be within `vr_2h_angle_threshold` of the blade (0.65, about 50 degrees). Letting go with the
  main hand while the other holds the grip passes the sword to it (the hand-switch spot, as before).

**Melee, parry, bash.** Swings are the main hand's: its stroke, its speed, and its weapon's striking points
(which follow the two-handed blade). The helping hand no longer strikes as a fist of its own while it steadies a
weapon (`PlayerVRMeleeImpl`, `client.qc`; this also applies to a gun's foregrip). Parrying with two hands already
counted as two-handed (`combat.qc`): both hands buzz and the weapon is never knocked out of the hand. The guard
and bash test the main hand, as with one hand. Both still use the hand's forward, not the blade (unchanged, as
for the axe). On the dummy (mock, the round-14 motions with the off hand 12 cm down the blade): two-handed
overhead 51 (guard), soft overhead 60, side swing 60, one hit each, and no fist hits from the helping hand.
(Contact points and damage are the melee model's and depend on the distance.)

**Firing range.** Both swords lie on the floor in front of the east end of the weapon racks (by the lava
nailguns, toward the dummy), as `func_weapon_grabbable` with `"weapon" "13"`. The hell knight's has
`"weaponflags" "1"`. `func_weapon_grabbable` now passes an entity's `weaponflags` on (it passed 0).

Screenshots (scratchpad `sw2h/`): `t2_0` one hand under the guard, `t2_1` two hands, `t3_zoom` the grip up close,
`range_*` the swords in the firing range.

In the headset:
- Pick up either sword. Does the fist sit right under the guard, and is the longer grip the right length?
- Close the other hand under it and swing, overhead and sideways, on the dummy. Does the sword follow the line
  between your hands without lag or jumps? Does the lower hand look right on the grip? Its drawn spot is one
  model-fist below the main hand, so if the controllers sit closer together than that, the drawn hands are further
  apart than yours. `vr_wofs_2h_fxd_ox/oz_19` (and `_20` for the hell knight's) move it.
- Is grabbing the grip easy? It needs the helping hand within about 17 cm of the spot.
- Let go with the upper hand while holding with the lower: the sword changes hands.

## Geometric waves

Voice note e1m2_2026-09-26_01-59-00, second part: waves that move the surface itself, not only its shading.

**Feasibility.** Ironwail draws each liquid face as its own polygon (a fan of triangles). The map compilers do not
cut liquid faces (qbsp skips `TEX_SPECIAL`), so a pool is a few big flat polygons and there is nothing between the
corners to move. There were two ways to get vertices in the middle:

- **Tessellation shaders** on Ironwail's triangles (GL 4.x is there). The GPU-culled draw would send patches
  instead of triangles, and both liquid programs would get a control and an evaluation stage. The world program's
  dozen varyings would be passed through them, in a shader another agent is also editing. The harder part is the
  walls: a tessellated triangle can't tell a wall edge from a fan diagonal or an edge shared with the next face, so
  holding the rim still would need a separate "distance to the rim" field in the world. The levels would also follow
  the distance to each eye, so the two eyes would get different meshes and the vertices would pop as you move.
- **Cutting the faces into a grid** once per map (FitzQuake's `GL_SubdivideSurface` idea), in buffers of their own,
  with the height added in the vertex shader. This is simple, the same in both eyes, and gap-free by construction.
  I chose this one.

**What it does** (`vr_water.cpp`, "Geometric waves"; `r_world.c` `R_DrawLiquidMesh`; `gl_shaders.h`
`LIQUID_SWELL`):

- **Mesh.** On a map's first view, every level world face of water, slime or lava (not teleports) is cut along
  the world's grid lines, every `vr_water_geo_cell` (16) units. Each crossing is computed the same way from either
  face that shares the edge, and the faces' own corners are kept. So faces that share an edge share its vertices.
  The texture and lightmap coordinates come from Ironwail's vertices (read back once), so lit water stays lit.
  Sloped and vertical liquid faces go in unchanged.
- **The rim stays still.** Each vertex has a pin value: 0 on the pool's rim, rising smoothly to 1 at 32 units in.
  The rim is every part of a face's edge that no other face of the same liquid, side and height continues past:
  a wall, a pillar, another kind of liquid, a vertical liquid face. The pin depends only on where the vertex is, so
  the top of the water and its underside move together, and so do neighbouring faces. A check at load (since
  removed) found no vertex that moves on a shared edge without a twin: 916 in start, 4376 in e1m1, 3396 in e1m2.
- **Waves.** Three long swells (173, 109 and 71 units, travelling in three directions), `vr_water_geo_amplitude`
  (3) units high in all. Lava's are 1.8 times longer, 1.3 times higher and slow; slime's are small and sluggish;
  teleports have none. The vertex shaders of both liquid programs raise each vertex by `swell × pin`, fading to
  nothing between 512 and 1024 units away.
- **Shading.** The swells' slope goes into `LiquidWaves`' normal, so the light, fresnel, glints and refraction
  follow the geometry. The fragment position passed on stays the flat surface's: the face's normal, the fog, the
  caustics and the dynamic lights see the flat face, and only the position on screen and the depth move. Brush
  entity liquids stay flat but get the same swells in the shading.
- **Drawing.** Each view, `VR_WaterMarkVis` (called from `R_MarkSurfaces`) picks the faces that are in the PVS,
  in the frustum, and not behind the viewer's side of them by more than the swell. A face wholly past 1024 units is
  drawn as its plain polygon. `R_DrawBrushModels_Water` then draws the world's liquids from this mesh, with
  Ironwail's own programs, calls and instance data, and its own indirect commands. Everything else (OIT, lit water,
  refraction, the underwater view) is as before. With the waves off, the mesh isn't built and the world's liquids
  draw as before.
- **Settings.** `vr_water_geo_waves` (1), `vr_water_geo_amplitude` (3), `vr_water_geo_cell` (16; the mesh is
  remade when it changes). Menu: Graphics > Water and Liquids > Real Waves and Real Wave Height. The presets turn
  it off in Off (Quake) and Low, and on from Medium up.

**Not changed:** collision and swimming still use the flat surface. Things floating in the water, and splashes, do
not rise and fall with the swells.

**Cost** (mock, 2048x2048 per eye, e1m2 looking over the big pool): the water's GPU time went from 0.11 to 0.12 ms a
frame, both eyes. The mesh's vertices are the only thing added, and the pixels are the same. Scaled to 3292x3524,
the extra cost stays under a few hundredths of a millisecond. CPU: +0.02 ms per eye (the per-view face pick). The
meshes: start 6306 vertices / 10870 triangles, e1m1 13428 / 21054, e1m2 15412 / 25432, built in a few
milliseconds while the first view loads.

**Tested** with the mock (scratchpad `geo/`):
- `geo/e1m2_cmp.png`: e1m2's big pool at a grazing angle, with the waves off, at 3 units and at 6 units.
- `geo/g5.png`: the start map's pool from above, see-through and opaque, at 8 units, then from under water.
- `geo/g8.png`: e1m2 and start's lava.

I found no gaps at the rims or between faces.

In the headset:
- At a pool's edge, crouch until your eyes are just above the water and look across. Is the swell visible in
  stereo, and does 3 units look right (`vr_water_geo_amplitude`, up to 8)? Lava is bigger and slower.
- Look along the rim where water meets a wall, from above and from under water, for slits or flicker.
- Put your head under and look up: the underside should move with the top.

## Parallax artifacts

Voice note start_2026-09-26_02-21-12 ("If I look up close at walls with parallax, I can see this kind of line
artifacts. Is that normal or is this something that can be fixed?"). The screenshot shows the start map's stone ledge
from below. Up close there is a grid of short dark dashes, and on the darker wall under it there are rings of dashes
round a point.

**Cause: the bumps on the baked light, not the parallax.** I reproduced it in the mock at the note's spot with the
eyes rendered at 3072 x 3072 (`vr_render_scale 3`), which is close to the headset's pixel density, and then turned
things off one at a time:

- Parallax off (`vr_parallax 0`): the dashes stay, in the same places on screen.
- Map-light shadows and self shadows off, a larger shadow bias, `r_dither 0`: no change.
- **Bumps in Map Light off (`vr_normalmap_baked 0`): the dashes are gone.** Parallax is still on, and its relief is
  clean.

The bumps on the baked light guess where the light comes from by looking at where the lightmap gets brighter across
the surface. That guess used the screen derivatives of the filtered lightmap (`dFdx`/`dFdy`). But the texture filter
blends luxels with 8-bit weights (steps of 1/256 of a luxel, about 0.06 units), and the lightmap values are 8-bit
too. Up close, a pixel covers less than one of those steps. So between two pixels the derivative was either 0 or a
whole step. The code then divided that step by a pixel's width, multiplied by 96 and divided by the brightness, which
tilted the guessed light by the full 45 degrees. That made dark dashes along the lines where a step fell between two
pixels. The steps of the 8-bit brightness follow its contours, which gives the rings round lights. More pixels make it
worse, so it shows in the headset (3292 x 3524) more than on the desktop.

I checked the parallax itself for the causes that were suspected, with the baked bumps off, the depth at 6 and 16
against 64 steps, close up and at grazing angles. Its layers and refinement showed no contour lines, and 64 steps
looked the same as 16, because the limit of 1.5 steps per height texel already applies. I made no parallax change.

**Fix** (`gl_shaders.h`, world shader: `LightmapLumDerivs`, `LightmapGreen`, `LuxelSlope`). The slope is now worked
out from the luxels themselves in full precision, then multiplied by the (exact) screen derivatives of the lightmap
coordinates, so `BakedBump` is unchanged:

- The 4 x 4 luxels around the pixel are read with 4 `textureGather`s: the green channel, with the styles combined,
  one gather per quad and per style (4 per quad with 3 or 4 styles). Brightness is taken relative to green
  (times the contrast power `vr_light_contrast`), so the result is in the same units as before.
- Each luxel's slope is the central difference, blended across the cell. So the slope is continuous at luxel lines.
  The bilinear light's own slope jumps there, and with exact derivatives that showed as a faint straight line every
  16 units. `dFdx` had the same jump, but the noise hid it.
- A luxel beyond the cell is used only when its difference agrees with the cell's own. Lightmaps are packed with no
  gap, so the luxel next to a face's edge luxel in the atlas can belong to another face.

**Before / after:** `parallax_artifacts_before_after.png` in the scratchpad (2x crops, eyes at 3072): the ledge's
underside, the wall under it, and the ledge top, which also had a luxel line.

**Cost** (RTX 4090, mock eyes 1024 x 1024, world+brush GPU for both eyes, Bumps in Map Light on / off): the note's
spot 0.18 / 0.16 ms, the wall straight on 0.15 / 0.13, e1m1's first corridor 0.20 / 0.18. The old derivatives cost
almost nothing, so the fix adds about 0.02 ms. Scaled to the headset's eyes (about 11 times the pixels), that is
roughly 0.2 ms a frame. A first version with 12 `texelFetch`es cost twice as much.

**Check in the headset:**

- At the note's spot, put your face close to the ledge and the wall under it, and to other bumpy walls near lights.
  The dashes and the rings should be gone, while the relief and its shading stay.
- Look for straight lines in the shading every 16 units, or along a face's edge. A face's edge luxels can still
  differ from its neighbour's, but I saw none in my tests.
- If something still flickers up close, compare with Bumps in Map Light off. If it goes away, the cause is the baked
  bumps again. If not, it is the parallax, so try Parallax Depth lower.

## Shell casings

Voice note start_2026-09-26_02-21-59: spent shells should fall out of the weapons in the right direction, with gravity
and physics. The shotgun ejects one every time it cycles a round. The double shotgun ejects only when it's reloaded,
especially on a flick reload. The shells should be simple low-poly models in Quake's style, with a subtle smoke trail
or a few sparks.

**What you see** (`vr_shells`, default 1; `vr_shells_life`, default 20 s; `vr_shells_sound`, default 1; menu:
Immersion, "Shell Casings", "Shell Casing Life" and "Shell Casing Sound"):

- **Shotgun.** 0.22 s after each shot (the recoil and pump sound), a shell comes out of the port on the right side of
  the receiver. It goes right, a little up and back, at about 1.6 m/s plus the hand's own motion, tumbling end over
  end. The port leaves a faint puff of smoke and three tiny powder sparks.
- **Super shotgun.** Nothing comes out when it fires. When it's reloaded (a hip holster, the reload button or a flick),
  its spent shells come out of the two chambers at the breech: one or two, as many as were fired. They go back and up
  with more smoke (a hot breech). On a **flick reload** they're flung by the spinning gun: they take 60% of the
  breech's own speed (the spin, easily 3-4 m/s) and extra tumble, so they fly over your shoulder. With reloading off
  (`vr_reload_mode 0` or Quick Slots), the gun has no reload to eject on: its shells come out 0.45 s after each shot
  instead, as if it reloaded itself.
- **In flight.** Real gravity (9.81 m/s², times `sv_gravity`/800, so low-gravity maps are floaty) and a little air
  drag. For its first 1.2 s a shell trails a thin, fading wisp of smoke.
- **Landing.** A shell bounces off walls and floors with restitution 0.3-0.45 and some friction, tumbling anew, with a
  quiet metallic tink on each of its first few hard hits. Once it's too slow to bounce on a floor, it lies down on its
  side, rolls across its axis or slides along it to a stop, and rests. Off a ledge it falls again. In water it sinks
  slowly, without tinks. After `vr_shells_life` seconds it fades out over 1.5 s.

**How it works.**

- **Client-side debris** (`Quake/vr/vr_shells.cpp`). The shells aren't server entities, so there's no network cost and
  no edicts: a pool of at most 64 on the client (a new one replaces the oldest). Each shell has a position, velocity,
  orientation (quaternion), angular velocity and age. Each is drawn as an alias entity added to the scene with the VR
  view entities, so it gets the level's lighting, dynamic lights (muzzle flashes light it up) and fog.
- **Collisions** are line traces through the world and the moving brush models (`worldtrace::world`, from the
  client's own data, so it works in multiplayer too). A shell in the air traces once per frame, as does a rolling
  one (plus a short check for its floor). A resting shell costs nothing but a floor check every 0.3-0.6 s, so it
  falls if a lift goes down. At most that's 64 small entities and about 128 short traces a frame, and that only while
  they all move at once.
- **When: the QC says.** A new builtin, `ejectcasings(hand, kind, count, delay, flags)` (QC/builtins.qc), sends
  `svc_quakevr` / `QVR_SVC_EJECT` to the firing player only (`vr_server.cpp`, `sendEject`: 7 bytes). It's called by
  `W_FireShotgun` (the shotgun, delay 0.22; or the super shotgun firing its last shell with reloading off),
  `W_FireSuperShotgun` (reloading off only) and `VRReloadWeapon` (the super shotgun: the shells fired since the last
  reload, with `QVR_EJECT_FLICK` when `VR_HandGrabUtil_IsHandReloadFlicking`). So it's right whatever triggers the
  shot or the reload, and bots and other players' shots send nothing.
- **Where from and which way: the weapon model.** The client knows each hand's drawn weapon (`vr_view.cpp`). A small
  table in `vr_shells.cpp` gives, per model, the ports in model space (v_shot: the receiver's right side, 14.5 -2.8
  4.3; v_shot2: the chambers, 11.6 ±1.2 6.9), the ejection direction, speed and tumble. It also gives an anchor
  vertex that moves the ports with the firing animation, since v_shot recoils 7 units back while its shell is
  ejected. It's mirrored with the model, so a left hand's shotgun ejects to the left. The port is followed from frame
  to frame, so its speed goes into the shell: a swung hand, a flick's spin and the player's own movement all carry
  over. Adding another weapon is one table row (and, if it needs another casing, a model and a `kind`).
- **The model** (`progs/vr_shell.mdl`, `Misc/quakevr/make_shell.py`, 112 triangles). An 8-sided fired 12-gauge shell,
  7 cm by 2 cm: a red ribbed plastic hull with its crimp opened (dark and sooty inside), on a dull brass head with a
  rim and a primer. The colours are from Quake's palette (the dark reds, the browns and olive for the brass). It's
  drawn 1.25 times real size, because Quake's shotgun model is about that much bigger than a real one (1.25 m long).
  It's scaled with `vr_world_scale`.
- **The sound** (`quakevr/sound/vr/shell_tink1..3.wav`, `Misc/quakevr/make_sounds.py`). A brass ring (inharmonic
  partials), a plastic tock and a click, then a softer second bounce, in three pitches. It's played with
  `S_StartSound` at the shell, at idle attenuation and a volume from the impact speed (up to 0.45 × `vr_shells_sound`).
- **Particles** (`vr_particles.cpp`: `particles::shellEject`, `shellTrail`). The ejection puff and sparks and the
  trail. They're off with `vr_particles 0`.
- **Tuning command:** `vr_shells_eject [hand] [count] [flick]` ejects out of a hand's weapon without firing.

**Tested with the mock** (`vr_mock_hand`, `+attack`, `+reloadright`, a `vr_mock_play` flick; screenshots in the
scratchpad, `shells.png` and `shellshots/`):

- **Shotgun.** One shell per shot, out of the right side, flying right and down. It lands 1-2 m to the right, bounces
  (off the start pad's 16-unit step too), rolls and rests in 1-3 s. With many shots, they lie on the floor on their
  sides.
- **Super shotgun, reload button.** Two shells up and back out of the breech with a puff of smoke. A temporary client log
  showed one eject of 2 shells on the reload and nothing on the shot itself.
- **Super shotgun, flick** (a 75° wrist flick in 0.07 s, 18.7 rad/s). "flick reload" was logged, and the two shells
  left at about 100 units/s with the spinning gun and landed behind the player.
- In a close view, the muzzle flash's light makes a freshly ejected shell glow; it's the flash, not the skin.

**Check in the headset:**

- Does the shotgun's shell come out of where the port should be, at the right moment (it's timed to the recoil;
  `delay` in `W_FireShotgun`)? Does it go right, not into your face? Try the left hand too (mirrored).
- Flick-reload the super shotgun: do the shells fly out nicely with the spin, or too far/too fast? The share of the
  spin they take is `inherit` in `eject()`.
- Size and colour: at 1.25 times real they should match the gun. Too small to notice, or too bright?
- The tink: audible, but not louder than the room? It's `vr_shells_sound`.
- Performance with many shells lying around. It should be nothing, but fire a few boxes of shells and watch the
  frame time.

## Water splashes and sounds

Voice notes e1m2_2026-09-26_01-59-00 (first part: "splashing effects for the water when you shoot in it or when you
jump or when items land in it") and e1m2_2026-09-26_02-06-13 ("sounds for when you wade through water or when you
stroke in the water or when your weapons hit the water ... audio and visual feedback when interacting with water").
The geometric waves of the first note are a separate change.

**The splash** is a new Quake VR particle preset, `Preset::Splash` (vr_particles.cpp, `splash`). It is sent like QC's
`particle2`, from the surface point, the direction the thing went in, and how hard (4 a shot, 6-15 a hand or a thrown
thing, 20-50 a body). Each client draws it from that, so both eyes see the same particles and the cost is a few dozen
sprites:

- a **crown** of drops thrown up and out, leaning away from where a slanted shot came from. The drops fall back and
  vanish at the surface (a new per-particle floor height);
- a **jet**: a thin column of drops thrown straight up (most of a bullet's splash);
- **foam**: soft puffs spreading on the surface;
- **ripples**: one to three rings lying flat on the surface, spreading and fading. These are a new generated ring
  sprite, and particles can now lie flat instead of facing the view.

The liquid is read from the map under the point. Water is pale blue-white and slime green. Lava throws hot
yellow-orange blobs, which are alpha blended, since added to the bright lava they vanished. Its ripples are a dark
crust, and it adds embers and a little dark smoke. Water and slime drops are shaded by the lightmap under them, so
they don't glow in a dark pool.

**What splashes, and what you hear** (the sounds come from the server, so everyone hears them). Sounds that belong to
a point are sent at that point, not at an entity, so your own hand's splash is placed at your hand.

| event | where | splash | sound |
|---|---|---|---|
| a bullet or pellet crossing a surface | QC `FireBulletsImpl` (players' and grunts' shots) → new builtins `liquidentry` + `watersplash` | small, leaning with the shot | `vr/plip.wav` |
| a rocket, grenade, nail, laser, gib, item, backpack or thrown weapon going in | `predictWaterEntry` before its move (vr_rigid.cpp hook), else `VR_AllowWaterSplash` (SV_CheckWaterTransition, rigid bodies too); only above `vr_water_splash_speed` | by speed × model size | small things (nails, grenades): `vr/plip`; else `vr/splash_small` or `vr/splash_big` by strength (instead of Quake's `h2ohit1`) |
| ... coming out | same | smaller | Quake's `h2ohit1` |
| an explosion under the surface (a rocket into a pool) | client side, in the explosion preset | a big splash above it, smaller the deeper | the explosion's |
| you going in (jumping, falling, walking in) | QC `WaterMove` → `VR_PlayerWaterSplash` | by fall and run speed | Quake's own (`inh2o`, `slimbrn2`, `inlava`) |
| a hand or a gun's muzzle hitting the surface going down (> 0.9 m/s) | `waterFeedback` (vr_physics.cpp, every frame) | by speed | `vr/splash_small`, plus a short buzz in that hand |
| ... pulled out going up (> 1.4 m/s) | same | smaller | `vr/splash_small`, quieter |
| wading (legs in, head out, on the bottom or walking in the room) | same | ripples at the legs | `vr/slosh1/2` alternating, at a walking pace (0.3 - 0.6 s by speed), louder waist deep |
| a swimming stroke, as its power gate opens | `VR_AfterWaterMove` (the stroke model's gate) | a splash if the hand is within 10 units of the surface | `vr/stroke1/2` alternating, at the hand, at most one per 0.3 s (both hands together make one sound) |

A splash no stronger than one already sent that frame within 12 units is dropped, sound and all. So a super
shotgun's 14 pellets into a pool make one splash and one plip, not 14 of each. At most 3 water sounds are sent per
server frame.

The predicted entry matters for projectiles. Quake only notices a thing is in water after its move. A rocket or
nail that goes in and hits the bottom in the same move is gone (or stopped) by then. And one fired into water from
close by was taken as "spawned in water" and never splashed at all. VR_RigidToss now also marks a thing that
starts its first move in the open as being in the open.

**The new sounds** are synthesized by `Misc/quakevr/make_sounds.py`:

- `splash_small` (0.5 s): a wet slap, a short body of noise, bubbles ringing up, and drops falling back.
- `splash_big` (1.2 s): a low thump, a crash that darkens as it falls away, many bubbles, and a rain of drops.
- `plip` (0.24 s): a tick and one bright bubble.
- `slosh1/2` (0.6 s): a soft swell of darkened noise with a low gurgle.
- `stroke1/2` (0.75 s): a whoosh of band noise with turbulence, full of bubbles.

Bubbles are modelled as rising sine rings (a bubble's Minnaert resonance). QC/world.qc precaches the sounds.

**Cvars and menu** (Graphics → Water and Liquids):

- `vr_water_splash` (default 1): "Splashes", 0 off to 2 (more drops). Each client reads it. The graphics preset
  "Off (Quake)" sets it to 0.
- `vr_water_sounds` (default 1): "Water Sounds", the volume. At 0 only Quake's own splash sounds are left.
- `vr_water_splash_speed` (existing, 150 u/s): how fast a thing must move to splash.
- `developer 2` prints each splash's point and strength.

**Tested** on the mock backend (e1m2's moat, surface at z 148; the shallows at 1792 20, waist deep; the start
map's lava). Everything below was confirmed with `developer 2` lines and screenshots:

- **Super shotgun:** into the moat from the ledge, one splash and one `plip`.
- **Nails:** 1750 u/s, a splash and a plip each.
- **Rocket:** 1000 u/s, strength 19, `splash_big`. Before the predicted entry, this rocket made no splash: it hit
  the bottom in the same move.
- **Grenade:** 659 u/s, a splash and a plip.
- **Dropped shotgun:** a rigid body at 234 u/s, strength 10.5, `splash_small`.
- **Player dropped from 150 units up:** a splash of strength 21.
- **Wading with the stick:** sloshes alternate at a walking pace, with ripples ahead.
- **Scripted slap:** 4.8 m/s down, then pulled out at 2 m/s. It gave splash 16 with `splash_small` at full volume,
  then a small exit splash at 0.26 volume. It gave no stroke sound; a slap's stroke is suppressed.
- **Scripted frog strokes** at the surface: one stroke sound per stroke, alternating, with a splash each.
- **`vr_particle_test 14 <n>`** on water (4, 12, 35) and on lava: the looks, in the screenshots.
- **`vr_water_splash 0`:** removes the particles.

I did not hear the sounds. The mock has audio, but I only checked the envelopes of the files
(`make_sounds.py`'s output).

**What to check in the headset:**

- **The sounds first.** They are synthesized and I haven't listened to them. Are the splash, plip, slosh and stroke
  believable, and loud enough next to Quake's own? `vr_water_sounds` scales them all. If one is bad, say which.
- Are the drops the right size and brightness? Look at pools in dark and bright places. `vr_water_splash 2` for more.
- Do shots into water plip, and do slanted shots lean the crown?
- Jump into the e1m2 moat: a big splash around you, with Quake's sound.
- Slap the water with an open hand, then with a gun: there should be a splash, a sound and a buzz. A slow dip should
  do nothing. Pull the hand out fast.
- Wade in the shallows: are the sloshes at a natural pace, and are the ripples at your legs visible when you look
  down?
- Swim with brisk strokes: one stroke sound per stroke, and no sound for the slow return.
- Throw a weapon, a backpack or a gib into water. Try a grenade or a rocket.
- Lava (start map, e1m7) and slime: the colours and the embers.

## Melee points, deflect, frame rate

Voice notes r1m1 02-14-49 (sword hits register near the hilt, not at the tip; hit with several points along the
weapon, the hilt too, one hit per swing), vrfiringrange 02-26-50 (batting projectiles back worked once in 20-30
tries: more lenient timing and reach, with settings) and vrtutorial 02-18-26 (throws and melee hits harder to line
up at a lower frame rate).

**Why sword hits missed.** Four causes stacked up:

- The hit test was two short lines cast along the hand's motion, from the hand and from the weapon's muzzle point,
  on each server frame of a blow. It tested where the weapon was at that moment. What the blade swept through
  between two frames, and anything along the blade between the hand and the tip, was never tested.
- The client stopped hands and weapons at monsters' boxes, not only at walls (`stopAtWall` in `vr_handpose.cpp`
  traced with `MOVE_NORMAL`, through the local server). A grunt's box is much wider than the grunt. The sword's tip
  reached the box first, the weapon was held at the box, and the hand was pushed back. The server saw the wrist
  jump back and started a new stroke, so the swing never became a blow. A close punch was stopped the same way.
- A stroke also restarted when the striking point moved more than 0.6 m in one server frame (meant to catch
  teleports). A hard sword swing's tip moves 0.6 m in 1/45 s, so at 45 fps, and at 90 fps (the server runs at
  45 Hz there too), sword swings never hit at all.
- The weapon's weight smoothing (`vr_wpn_pos_weight`) moved the hand a factor times the frame time of the way per
  frame, clocked by `cl.time`, which moves in steps with the server's messages. At 45 fps it didn't smooth at all.
  At 144 fps (server frames of 14 and 21 ms by turns) it moved the hand in jerks, and the server read the wrist as
  speeding up and slowing down by up to half again.

**Striking points** (`QC/vr_juice.qc`, "Striking points"). Each hand strikes with points along what it holds,
from the hand (`handpos`) to the weapon's far end (`muzzlepos`: the models' anchors, so a grip moved in
`vr_weapons.inc`, or the two-handed blade, moves them too):

| Weapon | Points (damage multiplier) |
|---|---|
| Sword | pommel (0.6), hilt (the hand, 0.6), guard (0.85), mid-blade, outer blade, tip (1) |
| Axe, Mjolnir | hand, handle (0.5), head x2 (1) |
| Gun as a club | grip, barrel, muzzle (1) |
| Fist (or a box in hand) | fist, knuckles (1) |

- Each server frame with a new pose, every point is swept from its last position to its current one. The sweep is
  a line, 3 units thick (`vr_melee_range_multiplier` scales this), tested against the boxes of things that bleed.
  Walls and brush entities are tested with an exact line. The sweep covers what the weapon passed through at any
  frame rate.
- A stroke's first contact is kept: what was hit, where, the point's direction and which point it was. The
  stroke's blow lands on it once. If the contact came earlier in the stroke, the blow lands as soon as the stroke
  becomes a blow; otherwise it lands at the contact, as long as the stroke goes on. Things that bleed come first.
  A wall counts only if it was touched in the last 0.1 s, and not by a point moving down (reaching for a holster).
- If several points strike in the same frame, the strongest wins (the blade over the hilt), then the earliest. A
  weak point's contact (hilt, pommel, guard, handle) waits 0.05 s for a stronger one to follow it in. So a pommel
  strike counts as a pommel strike only when the blade doesn't also connect.
- The blow's strength is multiplied by the point's multiplier (a pommel strike is about a third of a blade hit).
  The dummy names the point: `melee: Knight's Sword, swing with the tip, ...`.
- One hit per swing. After a hit, the hand must stop, or turn back after slowing to a third of the blow's peak
  speed (a jab after a jab). A wide arc keeps going fast as it curves round, so it is still the same blow. It used
  to rearm as soon as the arc's direction turned against the blow's, which gave two hits in one wide sweep. The
  swing sound plays once per stroke.
- The client no longer stops hands or weapons at monsters (anything that takes damage and isn't a brush), only at
  walls. Weapons pass through monsters as the hand does.

**Batting projectiles** (`VR_Deflect`). A batting swing is a stroke whose wrist reached `vr_deflect_speed` x
`vr_melee_speed`, still moving at half that. It doesn't need a real blow: no reach, snap or shape. Each frame of the
swing, the weapon's line (pommel to tip, fist to knuckles) is kept for `vr_deflect_window` seconds. A monster's
projectile is batted back when its path this frame, from 0.05 s behind it to where it will be at the frame's end,
passes within `vr_deflect_radius` (plus its size) of any kept line. So a swing that's a little early still bats
what flies into where the weapon went, and the test is swept, so the frame rate doesn't matter. The projectile flies
back where the hand points. If its thrower is within 25 degrees of that, it is aimed at the thrower. It goes 1.2x
faster than before (at least 500), as it did.

| Setting | Default | Menu (Gameplay > Feel) |
|---|---|---|
| `vr_deflect_radius` | 14 units | Batting Reach, 4-32 |
| `vr_deflect_speed` | 0.6 (x `vr_melee_speed`: 2.1 m/s) | Batting Swing Speed, 0.2-1.5 |
| `vr_deflect_window` | 0.2 s | Batting Timing, 0-0.5 |

Before, a projectile had to be within 16 units of the hand-to-muzzle line at the moment of a frame, during a full
blow (the late, fast part of a swing).

**Frame rate: what I checked.**

- Server frames: at `host_maxfps` above 72, Ironwail runs the server when 1/72 s has built up. At 90 fps that means
  every second frame (45 Hz, the same as 45 fps). At 144 fps it runs every second or third frame (72 and 48 Hz by
  turns). Anything measured per server frame therefore sees 45 Hz on a Quest at 90 Hz.
- Stroke tracking (`VR_Blow_Track`): speeds are distances over time, and the thresholds are in m/s, m/s2 and
  seconds. Two fixes:
  - The jump test is now a speed: 20 m/s for the wrist, 60 m/s for the striking point, and never less than the old
    0.3 m and 0.6 m.
  - Poses are timed at `time + frametime`. QC's `time` is the start of the server frame, while the pose is the
    client's as the frame ends. Timing by `time` made each speed one frame late, which is wrong when frames differ
    in length.
- Hit tests: they were point-in-time; now they are swept (above). Deflect likewise.
- Weapon weight smoothing (`vr_handpose.cpp`): the blend is now compounded, `1 - (1 - f)^(dt x 100)`, and clocked
  by `realtime` every frame. A weapon lags its hand by about 14 ms at any frame rate (the sword and axe: f = 0.5).
- Throwing (`vr_throw.cpp`): the release windows were already in seconds, on the runtime's clock. But the peak was
  the fastest sample, and at 45 fps samples are 22 ms apart, so the peak could fall between them: throws came out
  8% slower at 45 fps than at 72 (7.39 against 8.01 m/s). The peak speed is now the top of a quadratic fitted to
  the speeds within 30 ms of the fastest sample (least squares, kept within 20% of it). The direction and averaging
  are unchanged.
- Throw origin (`DropWeaponInHand`): the thrown weapon starts where it would have flown since the peak. That was
  skipped when the peak was over 0.15 s old, and the weapon then started from the follow-through, lower and
  behind. A lower frame rate makes that age larger. The age is now capped at 0.3 s instead. If a monster is on the
  way from the palm, the weapon starts at it.
- Unchanged, and fine: thrown rigid bodies substep with swept corners and a swept hit box; bashes and headbutts use
  the runtime's velocities and time windows.

**Measured** on the training dummy with scripted mock motions that play on the clock. `vr_mock_play <file>` plays
timed keyframes (poses, buttons, console commands), so the same motion runs at any frame rate; `wait`-paced scripts
ran at the server's rate. The settings were `vr_melee_speed 3.5` and `vr_melee_distance 0.25`. The sword's blade was
held level along the swing, and "d" is the gap from the player's origin to the dummy's face, in units. The mock's
hand reaches about 19 units ahead, and the sword's tip about 43. Each cell is damage, the point, and the wrist's
peak speed. The generator is `melee_gen4.py` in the session's scratchpad.

| Motion | 45 fps | 72 fps | 90 fps | 144 fps |
|---|---|---|---|---|
| Sword side swing, d 18 | 60 mid-blade (9.2) | 60 mid-blade (9.3) | 60 mid-blade (9.2) | 51 guard (9.3) |
| Sword side swing, d 30 | 60 blade (9.2) | 60 blade (9.3) | 60 blade (9.1) | 60 blade (9.1) |
| Sword side swing, d 38 (outer blade only) | 60 tip (9.2) | 60 tip (9.3) | 60 tip (9.2) | 60 tip (9.1) |
| Sword side swing, d 46 (out of reach) | - | - | - | - |
| Sword wide sweep (160 degrees), d 24 | 60 mid-blade (10.2) | 60 mid-blade (10.2) | 60 mid-blade (10.5) | 60 mid-blade (10.8) |
| Sword overhead, d 18 | 51 guard (9.6) | 60 mid-blade (9.6) | 60 mid-blade (9.5) | - |
| Sword overhead, d 34 | 60 blade (9.6) | 60 blade (9.6) | 60 blade (9.6) | 60 blade (9.5) |
| Sword overhead, d 44 (out of reach) | - | - | - | - |
| Sword pommel punch (blade back), d 16 | 12.4 pommel (5.8) | 13.1 pommel (6.0) | 14.7 pommel (6.4) | 11.3 pommel (5.1) |
| Sword slow flag wave, 2 cycles, d 24 | - | - | 19.6 mid-blade (4.4) | - |
| Axe side swing, d 18 | 40 head (10.2) | 40 head (10.3) | 40 head (10.9) | 40 head (10.2) |
| Axe side swing, d 30 | - | - | - | - |
| Axe overhead, d 18 | 40 head (10.5) | 40 head (10.4) | 40 head (10.3) | 40 head (10.4) |
| Punch, d 16 | 16.2 knuckles (6.4) | 17.1 knuckles (6.6) | 17.9 knuckles (6.6) | 18 knuckles (6.5) |
| Shotgun as a club, overhead, d 18 | 24 grip (9.8) | 24 grip (9.8) | 24 grip (9.4) | 24 grip (9.6) |
| Spike 600 u/s, sword, swing -0.15 s | batted | batted | batted | batted |
| Spike 600 u/s, sword, swing -0.10 s | batted | batted | batted | batted |
| Spike 600 u/s, sword, swing -0.05 s | batted | batted | batted | batted |
| Spike 600 u/s, sword, swing +0.00 s | batted | batted | batted | batted |
| Spike 600 u/s, sword, swing +0.05 s | - | - | batted | - |
| Spike 300 u/s, sword, swing -0.20 s | batted | batted | batted | batted |
| Spike 300 u/s, sword, swing -0.10 s | batted | batted | batted | batted |
| Spike 300 u/s, sword, swing +0.00 s | batted | batted | batted | batted |
| Spike 300 u/s, sword, swing +0.05 s | - | - | - | - |
| Spike 600 u/s, fist, swing -0.10 s | batted | batted | batted | batted |
| Spike 600 u/s, fist, swing +0.00 s | - | batted | batted | batted |
| Throw the axe at the dummy, 110 units (estimate, damage) | 8.05 m/s, 73.9 | 8.23 m/s, 74.9 | 8.08 m/s, 73.9 | 8.13 m/s, 74.5 |
| Throw the sword, 110 units | 8.05 m/s, 122.9 | 8.23 m/s, 124.9 | 8.18 m/s, 124.5 | 8.17 m/s, 124.4 |

Before these changes, at 72 fps (and the old test geometry): sword side swings where only the outer blade reached
missed, and so did the overhead at 34 units. At 45 fps, a debug trace showed every hard sword swing reset by the
tip's "jump": no hits. The old swept spike test batted 3 of 5 timings at 600 units/s and 1 of 4 at 300. The throw
estimate was 7.39 / 8.01 / 7.52 / 7.92 m/s at 45 / 72 / 90 / 144 fps, and is now 8.05 / 8.23 / 8.08 / 8.13.

Notes:
- "Deflect" rows: the spike is aimed 16 units ahead of the face (the front of the player's box), from 300 units, and
  a side swing at chest height takes 0.24 s. The offset is when the swing's middle passes, relative to the spike's
  arrival (negative is early). "Late" swings (+0.05) mostly miss, because the spike reaches the player first.
- The sword's reach is physical now: the tip reaches about 43 units in this mock, so 46 misses. Old hits at a
  distance came from the forward lines' extra reach (22 to 52 units beyond the hand), and those are gone. The axe's
  head reaches less than 30 units, the fist about 20.
- The slow flag wave (`sword_wave_24`, a 0.6 s stroke) peaks at 4.4 to 4.6 m/s, just over a swung weapon's least
  (4.375). It hits once in a while at any frame rate. That's the round-14 threshold, unchanged.
- A second run at 45, 90 and 144 fps matched this one: every sword, axe, fist and club motion hit the same way,
  the throws came out at 8.04 to 8.18 m/s, and the batting results were the same. The misses marked above (the
  overhead at 18 units at 144 fps) and the extra hits (the wave at 90, the late spike at 90) didn't happen again.
  The sword overhead at 18 units hit 3 of 3 in a separate 144 fps run.

**In the headset** (firing range dummy):

- Sword: side swings at a distance where only the last third of the blade reaches the dummy should hit, with
  "the tip" or "the blade" in the dummy's line. Close in, "mid-blade" or "the guard". A punch with the sword held
  back over your shoulder should hit "with the pommel" for about 12. Each swing should hit once, even a wide sweep
  that drags the blade across the dummy.
- The weapon now passes through a monster's box instead of stopping at it. Does that look wrong anywhere (guns
  poking into monsters)? It's what made the tip register.
- Batting: stand in front of a scrag, an enforcer or a hell knight and swing at the projectile a moment before it
  arrives. A brisk swing (not a full blow) should bat it, even slightly early. If it's still too hard, raise
  Batting Reach and Batting Timing (Gameplay > Feel). If it's too easy, lower Batting Reach or raise Batting Swing
  Speed.
- Frame rate: compare melee and throws at 72 and 90 Hz (and 120). Hits and throw speeds should feel the same. The
  weapon now lags the hand by the same ~14 ms at every refresh rate. Before, it didn't lag at 45 fps and lagged more
  at 144.
