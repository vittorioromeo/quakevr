# Feedback round 15: plan and notes

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
