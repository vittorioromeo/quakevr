# Feedback round 16: plan and notes

From the sixth batch of voice notes (16 notes, the first memory log).

| # | Note | Request | Status |
|---|---|---|---|
| 1 | vrfiringrange 04-35-34 | Sword falls when the main hand lets go of a two-handed hold | done: hand-off |
| 2 | vrfiringrange 04-36-26 | Guns stay in the foregrip hand when the main hand lets go | done: carried by the foregrip |
| 3 | vrfiringrange 04-40-19 | Parry stopped working; a weapon held level across should parry | done: the weapon's line |
| 4 | vrfiringrange 04-41-37 | Corpses gib too easily; more for big monsters | done: 80, up to 280 |
| 5 | vrfiringrange 04-43-14 | Laser cannon stuck in its firing frame when out of cells | done (and the lightning gun's extra bolt) |
| 6 | vrfiringrange 04-44-21 | Lava nails glow and light | done |
| 7 | vrfiringrange 04-44-59 | The lightning beam lights its whole length | done |
| 8 | e1m1 04-26-51 | Flick-reload shells fall in front, not in the face | done |
| 9 | e1m1 04-28-54 | Real Wave Height range tripled | done: up to 24 |
| 10 | vrfiringrange 04-42-13 | Iron sight hue | done: `vr_sight_hue`, `vr_sight_saturation` |
| 11 | vrfiringrange 04-45-59 | A better sawn-off double shotgun | done |
| 12 | vrfiringrange 04-47-40 | The rocket launcher needs a handle | done |
| 13 | vrfiringrange 04-46-57 | Super nailgun's grooves as geometry | done |
| 14 | vrfiringrange 04-48-15 | A proper lightning gun grip | done |
| 15 | vrfiringrange 04-48-38 | Trigger guards (nailgun and others) | done |
| 16 | vrfiringrange 04-45-39 | Slower again: check the log | memory flat; the log now times every phase |

## Smaller changes

- **Real Wave Height** goes to 24 units (was 8); the culling margin follows (`kMaxSwell` 32).
- **Lightning gun:** it fired one more bolt after the trigger was released (and one with no cells): the loop's stop
  now stops the caller too (`player_light_Impl` returns whether to fire).
- **Held items put back in the world** (`vr_rigid.cpp` `keepInWorld`): an item put back at its last free place and
  still buried there (a door or a lift moved in) was put back every frame for ever; now it's left alone.
- **Your memory log** (04:20–04:50): VRAM, RAM and GL objects flat for half an hour over five maps; the frame time
  rose from 8.4 to 9.4 ms in vrfiringrange's last minutes with nothing growing. See "Slowdown" for the new columns
  that will say whose time it is next time.

## Iron sight hue

Voice note vrfiringrange 04-42-13: the weapons' iron sights in a colour of the player's choosing, as the gadget's
and the ammo screens' hue.

**Which weapons have sights.** The glowing sights are painted into the skins in the fullbright "fire" palette
indices, 224..239, 252 and 253 (the double shotgun's gradient: deep red rim, orange, pale yellow core; the shotgun's
was given the same by `Misc/quakevr/recolor_shotgun_sight.py`). A survey of every `progs/v_*.mdl` (id1, hipnotic,
rogue, quakevr) for fire texels the triangles use: only the **shotgun** (`v_shot.mdl`: two rear posts and a front
post) and the **double shotgun** (`v_shot2.mdl`: a ring) have sights, and those are the only fire texels they show.
The other weapons' fire texels are their muzzle flashes (nailguns, grenade and rocket launchers: geometry collapsed
out of sight except while firing), the lava nailguns' lava and the laser cannon's coils; the axe, swords, hammer,
grapple and lightning gun have none. The shotgun's pickup (`g_shot0.mdl`) carries its sights too (Quake's dark
red), so it follows the same hue.

**How.** `vr_sights.cpp` hooks the 8-bit upload (`TexMgr_LoadImage8`): a skin of one of those three models
(texture name `progs/v_shot.mdl:frame0`, its `_glow` mask and skin groups alike) is converted with a copy of the
palette whose fire entries are turned round the hue circle (HSV: each entry keeps its value and its saturation
times `vr_sight_saturation`, so the gradient keeps its shape and brightness). At the default the palette is the
normal one (nothing changes). Changing the cvars uploads those skins again from their source
(`TexMgr_ReloadImagesNamed`, as `gl_fullbrights` does), so the settings page's live preview shows the gun in hand
recoloured at once; `vid_restart` and `gl_fullbrights` keep the colour. The sights' glow (`vr_weapon_glow`, the
alias shader's fullbright boost, and the bloom) comes from those texels, so it takes the same colour; there is no
separate sight light.

**Cvars** (menu: Wrist Gadget > Colours, next to the screen and casing hues; the hue also next to Weapon Sight Glow
on the Graphics page):

- `vr_sight_hue` (degrees, 0..355; default 30, the sights' own orange: palette 236 is 29 degrees; 0 red, 120
  green, 240 blue). The gradient's spread (6..38 degrees as painted) goes along: the core stays a little warmer
  than the rim.
- `vr_sight_saturation` (default 1; 0 white, up to 2).

**For whoever remakes the weapon models:** on `v_shot.mdl`, `v_shot2.mdl` and `g_shot0.mdl`, paint the sights
in palette 224..239, 252, 253 (fullbright), and use none of those indices anywhere else on those skins that is
drawn (a muzzle flash, a hot barrel): everything in them follows the sight hue. A new sighted weapon is one line
in `sightedModels` (`vr_sights.cpp`).

**In the headset:** shotgun and double shotgun at a few hues (menu slider with the gun in hand: it changes live);
the sights' glow and bloom the same colour; nothing else on the guns changes colour; saturation 0 gives white
sights; a map change and a restart keep the setting; a shotgun pickup on the floor follows too.

## Lava nails, beam lights, shells

Voice notes vrfiringrange 04-44-21 (lava nails should glow in the dark and cast light like the hell knight's
flames), vrfiringrange 04-44-59 (the whole lightning beam should light the room) and e1m1 04-26-51 (a flick
reload's shells fly into the face; they should come out during the spin and fall in front of the player).

**Lava nails** (Rogue's second ammo for the nailgun and super nailgun, `progs/lspike.mdl`; the lava ogre's too).
Rogue's QC gives them `EF_VERYDIMLIGHT` and `EF_LAVATRAIL`, but those are bits 16 and 64, which Ironwail reads as
the 2021 rerelease's quad and candle lights and masks out for other progs, so until now they had no light and no
trail. Now, in `vr_emissive.cpp` (by model name, as the other projectiles' lights):

- **Light:** molten orange-red, radius 110 × `vr_projectile_lights`, flickering a little, unshadowed. The super
  nailgun keeps a dozen in the air (two guns twice as many), so only the nearest `vr_lavanail_lights` (default 8,
  0..16) are lit each frame. A nearer nail takes the light of the farthest one lit so far that frame.
- **Glow:** their skin is already fullbright (palette 229..236, dim oranges). The alias shader's fullbright boost
  (`VR_EntityFullbrightBoost`, 2.5 while projectile lights are on) makes it burn bright orange, so the bloom picks it
  up.
- **Streak:** `particles::lavaNailTrail`: short-lived additive glows every 5 units along the path (a streak of
  molten light, gone in 0.16 s) and an ember every 22 units, falling and dimming. With `vr_particles 0` there is
  none.

**Lightning beams** (`VR_BeamLights`, called from `CL_UpdateTEnts` for each live beam of a `progs/bolt*.mdl` model:
the lightning gun, the shambler's and Chthon's bolts; not the grappling hook's rope). One light every 80 units along
the beam, up to `vr_beam_lights` of them (default 6, 0..12), the last at the point it strikes. Along the beam
they're blue-white with a radius of twice the spacing (110..170), so they overlap into one stream. The one at the end
is bigger (230) and bluer. Each flickers (70..110%) and shifts up to a quarter of the spacing along the beam, 30 times
a second, like the bolt's segments. They cast no shadows (`dlightNoShadow`), so they never take one of the
`vr_shadow_dlights` shadows from the explosions and muzzle flashes. If the beam gets shorter and needs fewer lights,
the extra ones go out that frame.

**Cost** (mock, 1024×1024 eyes, graphics preset Ultra with `vr_shadow_dlights 12`, e1m1's start corridor, 200
frames each; GPU ms per frame):

| | idle | lightning, 0 lights | 6 lights | 12 lights | lava nails, 0 lights | 8 lights |
|---|---|---|---|---|---|---|
| all 3D (both eyes) | 0.654 | 0.704 | 0.724 | 0.731 | 0.684 | 0.698 |
| left eye world+brush | 0.177 | 0.178 | 0.187 | 0.189 | 0.178 | 0.179 |
| left eye VR particles | 0.001 | 0.022 | 0.022 | 0.022 | 0.010 | 0.015 |
| shadow maps | 0.046 | 0.046 | 0.046 | 0.046 | 0.046 | 0.046 |

Six beam lights add about 0.01 ms per eye to the world pass here (5%). At the Quest 3's 3292×3524 eyes, about 11
times the pixels, expect about 0.1 ms per eye while the beam is on. Twelve lights cost hardly more than six (the
clustered light loop only shades the tiles each light covers). The lava nails' lights are within the noise; their
streak adds about 0.005 ms. The CPU cost is nothing measurable (temp entities 0.002 ms).

**Cvars** (Graphics page, after Projectile Light: "Lava Nail Lights" and "Lightning Beam Lights"):
`vr_lavanail_lights` (8) and `vr_beam_lights` (6). The presets set them. Off (Quake) turns both off. Low keeps 4
lava nail lights and turns the beam lights off. Medium and up use the defaults.

**Shells: flick reload** (`vr_shells.cpp`, `vr_flick.cpp`: the new `flick::spinAngle`). The server still decides
the reload and sends the eject. On the client, a flick's casings now wait for the drawn spin to turn the barrels
down (255° of the 360°: the breech is open upwards, in front of the hand), for at most 0.6 s, before they leave.
They no longer take the breech's speed from the spin (4-6 m/s, which flung them over the shoulder into the face).
Each casing is thrown forward instead: mostly the way the head looks, turned 30% towards where the gun is. It
leaves at 1.1-1.7 m/s, pops up 0.3-0.7 m/s, is scattered sideways by ±0.35 m/s, and takes a quarter of the hand's
own swing (at most 1.5 m/s) plus the player's movement. `vr_shells_eject <hand> <count> 1` now starts a spin too,
for testing.

**Shells: never at the face** (all casings, `awayFromFace`). A casing that starts within an arm's length (1 m) of the
head never moves towards it along the ground: the part of its velocity towards the head (relative to the player)
is cut to 0.3 m/s away. It also can't rise higher than 15 cm under the eyes. This matters for the normal super
shotgun reload too: its "back and up out of the breech" (1.6 m/s) went at the head at about 1.4 m/s. Those shells
now pop up out of the breech and drop just in front and to the side. The shotgun's shells, thrown to the right, keep
their direction but lose their small backwards part.

**Tested with the mock:**

- Lava nails: `impulse 9; impulse 157; impulse 43` (the lava ammo) and a reload, fired down e1m1's start corridor
  at half speed with the muzzle flash off (`vr_flash_scale 0`), lights at 8 and at 0. The floor and the arches were
  lit orange along the nails' path, and dark with the lights at 0. The nails were bright orange streaks with embers.
  A log showed up to 7 nails lit at once from one super nailgun. With `vr_lavanail_lights 3`, never more than 3.
- Lightning: `impulse 161`, fired down the same corridor and in a dark room (e1m1 150 2392 40). The log showed 6
  lights on a 600-unit beam, and the room was lit blue-white along the whole beam; with 0 lights it was dark but for
  the muzzle flash.
- Flick reload: the super shotgun, two shots, then a scripted 75° wrist flick in 0.07 s (18.7 rad/s, `vr_mock_play`).
  "flick reload" was logged each time. The shells left at 257° of the spin, 0.68 m in front of the eyes and 0.35 m
  below them, at 1.1-1.6 m/s forward and 0.4-0.7 m/s up (1.1-1.6 m/s away from the head). They came to rest 1.7-2.0
  m in front of the player, within 0.25 m of straight ahead (vrfiringrange). Before this change they flew back over
  the shoulder and landed behind the player (round 15's test).
- Normal reload (the reload button): the shells used to go at the head at about 1.4 m/s. Now they go 0.3 m/s away
  from it, pop up out of the breech and rest 0.85-0.96 m in front and to the right.

**Check in the headset:**

- Lava nails in a dark place (e1m1's dark rooms, Rogue's maps): is the light on walls and floor too strong or too
  weak (`vr_projectile_lights` scales it), and are 8 lights enough with two super nailguns? Do the nails themselves
  glow enough with the bloom? Is the ember streak too busy?
- The lightning gun in a dark room: is the stream of light as intended, or too bright or too white near the beam?
  Does the flicker read as lightning or as a strobe? Check the frame time with `vr_profile 2` while firing. It should
  be about 0.1 ms per eye; `vr_beam_lights 3` halves the light count.
- A flick reload with the double shotgun: do the shells now leave during the spin, when the barrels point down, and
  fall in front of you? Is 255° the right moment? Too far or too near? Try the left hand too.
- The normal reload (reload button, hip holster): do the shells still look natural now that they can't come back at
  you?
- The double shotgun model (`v_shot2.mdl`) is being changed this round by someone else. If its breech moved, the
  shells' ports (11.6 ±1.2 6.9) and anchor vertex (17) in `vr_shells.cpp` may need moving too.

## Slowdown

Voice note vrfiringrange 04-45-39 ("the game is getting a bit slower again"), with the memory log
`memstats_2026-09-26_04-20-46.csv`.

**What that log says.** Memory and GL objects are flat for the whole session: VRAM 8.4 to 8.8 GB of 24, working set
320 to 350 MB, no evictions, the same GL textures, buffers and framebuffers after every row. The frame time was
8.35 to 8.65 ms (120 Hz) on start, e1m1, e1m2 and for ten minutes of vrfiringrange. From 04:43 it rose to 8.85,
8.94, 9.24, 9.25, 9.43 and 9.45 ms, while the author tried the laser cannon, the lava nails and the lightning gun.
The last row, 7.94 ms, is the menu and quitting. 6349 frames a minute at 120 Hz means about one frame in eight
missed its refresh. Render targets were remade 18 times in those minutes (50 to 68). They are the ammo screen:
its image takes the size of its text, so a weapon whose counter is wider or narrower remakes it. Each remake deletes
the old texture, and the GL texture count stayed at 607 to 609. That log had no timings, so it cannot say whose time
grew.

**New in the log** (`vr_memstats_log`, and printed by `vr_memstats`). Rows are averaged per frame since the last
row. The old columns come first, unchanged.

- Timing, always collected, whatever `vr_profile` is set to. The named profiler scopes of a handful of phases
  add up their CPU time every frame (`vr_profile.hpp`, `Phase`). With `vr_profile 0` a scope used to cost one flag
  test. Now it costs a pointer-keyed lookup, plus a clock read for the ~25 scopes that are phases.
  - `display_ms`: the runtime's refresh period (xrWaitFrame's predictedDisplayPeriod).
  - `slow_frames`: frames that took longer than 1.25 refreshes, so a refresh was missed.
  - `hitches`: frames over 250 ms (loads), left out of the averages.
  - `period_ms`, `period_max_ms`: a frame's start to the next's.
  - `host_ms`: our CPU frame, from its start to the end of the frame, submit included.
  - `busy_ms`: `host_ms` less the waits (`xr_wait`, acquire, release, submit, swap).
  - The runtime's calls: `xr_wait_ms` (the backend's beginFrame: xrWaitFrame, xrBeginFrame, the tracking),
    `xr_waitframe_ms` (xrWaitFrame alone), `xr_acquire_ms`, `xr_release_ms`, `xr_submit_ms` (xrEndFrame),
    `swap_ms`.
  - Our work: `commands_ms`, `server_ms`, `physics_ms`, `rigid_ms`, `client_ms`, `view_ents_ms`, `screen_ms` (both
    eyes, the runtime's calls between them included), `eyes_cpu_ms`, `run_particles_ms`, `sound_ms`.
  - GPU times, from one timestamp query at each end of a scope. They sit in a 6-frame ring, are read back late and
    never waited for (`gpu_frames` read back, `gpu_dropped` not ready in time): `gpu_eyes_ms` (and L, R),
    `gpu_shadows_ms`, `gpu_world_ms`, `gpu_alias_ms`, `gpu_particles_ms`, `gpu_vr_particles_ms`, `gpu_decals_ms`,
    and the runtime's calls' spans on the GPU: `gpu_acquire_ms`, `gpu_release_ms`, `gpu_submit_ms`.
- Counts, averaged over every 8th frame: `visedicts`, `dlights`, `shadow_dlights`, `shadow_maplights`, `particles`
  (Quake's), `vr_particles`, `beams`, `sound_channels` (playing), `texts` (queued world texts).
- Counts at the row: `edicts`, `edicts_high`, then these server entities: `monsters` (alive), `corpses` (a monster
  dead, its head included), `heads` (thrown heads that are not monsters), `gibs`, `missiles` and `thrown_weapons`.
  Also `cl_entities`, `decals`, `shells`, `world_texts`, `float_texts`, `boards` and `static_sounds`.
- `targets_by_name`: each render target's remakes, e.g. `ammo screen 1:46`. With `developer 1`, each remake prints
  its name and old and new size.
- `gl_scan_ms`: counting GL objects takes 12 to 13 ms, a visible hitch. So once it takes over 2 ms, it runs on map
  rows and every 5th timer row only. The rows in between repeat the last counts, and their `gl_scan_ms` is empty.

The per-row cost otherwise is below 0.1 ms. Also fixed: `vr_memstats` printed "0.0 s since the last: 0 frames" on
every call. The call counter was incremented inside the argument list, which is evaluated in no set order.

**Reproduced in the mock.** vrfiringrange with the author's `vr_*`, `r_*` and `gl_*` settings from `ironwail.cfg`
(decals 1024 / 240 s, 12 shadowed dlights, 8192 atlas, bloom, particles), `host_maxfps 250`, `vr_render_scale 2`
(2048² eyes). The play was scripted, 24 minutes in three chunks joined by save and load, 30-second rows. Each
round: three grunts (impulse 244); the super shotgun at them; rockets into the pile; then sweeps across the walls
with the lightning gun, the lava nails, the laser cannon, the super nailgun and grenades. Numbers per frame:

| | start (0 min) | 8 min | 16 min | 24 min, idle after |
|---|---|---|---|---|
| server entities / corpses / thrown weapons | 146 / 0 / 33 | 216 / 5 / 37 | 266 / 16 / 45 | 286 / 27 / 55 |
| decals | 0 | 0 (all expired) | 1024 (cap) | 822 |
| visible entities | 76 | 105 | 147 | 155 |
| host CPU (ms) | 0.36 | 0.37 | 0.46 | 0.45 |
| server (ms) | 0.036 | 0.043 | 0.065 | 0.065 |
| GPU eyes (ms) | 0.79 | 0.42 (other view) | 0.77 | 0.80 |
| GPU decals (ms) | 0.001 | 0.001 | 0.009 | 0.033 |
| GL textures / framebuffers | 501 / 31 | 503 / 31 | 503 / 31 | 503 / 31 |

The frame period held at 4.00 ms (the 250 fps cap) all the way. There were 0 dropped GPU frames. The ammo screen was
remade 46 times in the 15 rounds (7 weapon changes each), with the GL counts flat.

**What grows, and how much.** Only what the level keeps, and slowly:

- **Corpses.** Quake keeps dead monsters. A corpse that is shot is gibbed; its head stays.
- **Thrown weapons.** Weapons dropped or thrown stay in single player. The QC removes them after 10 s only in
  deathmatch, and each one thinks 50 times a second.
- **Decals,** up to `vr_decal_max`; they expire after `vr_decal_life`.

From 146 to 290 entities and 0 to 1024 decals, our CPU frame went from 0.36 to 0.46 ms. Server and physics account
for 0.03 ms of that, and building and drawing the extra entities for the rest. The eyes' GPU time stayed within
0.7 to 0.9 ms (the view and the firing vary more than that), and decals at their cap cost 0.03 ms. In the headset,
with 2.8 times the pixels, that would be under 0.1 ms. Gibs are removed after 10 to 20 s. Shells are capped at 64,
floating texts at 256, particles at 32768 (about 2000 at the peak here), and the particles expire. Sound channels
peaked at 5.

**Conclusion.** Nothing on our side grows enough to cost a millisecond. The worst is about 0.1 ms of CPU for a
level full of corpses, weapons and decals. The author's slowdown was 1.1 ms a frame on average, and it came as
missed refreshes. With the game's CPU and GPU work at 2 to 3 ms of the 8.3, that points the same way as round
15's profiles: at the runtime and the streamer (`xr submit` growing), or at GPU contention with them. Firing those
weapons also adds lights and particles for as long as the trigger is held. The new log answers this next time.
Nothing was changed to cap what grows. Corpses and thrown weapons could get an optional cleanup if the next log
blames them, but the numbers here say they won't.

**Noticed on the way:**

- A weapon dropped at the player's feet can loop in `keepInWorld` (`vr_rigid.cpp`). It is "buried", but also not
  on the ground, so it is put back where it was every frame, which relinks it and runs `VR_TouchLinks`. It is cheap
  (3 weapons: well under 0.01 ms), but the loop never ends, and with `developer 1` it prints a line every frame.
- The ammo screens' images are redrawn every frame (`renderScreens`), not only when their text changes (a few
  small targets; not a growth).

**For the author, next session.** Leave the memory log on (`vr_memstats_log 60`, the default) and play as before.
When it feels slower, note the time. Then send the `memstats_<date>.csv` (and `qconsole.log` if run with
`-condebug`). In the rows around that time, compare:

1. `period_ms` and `slow_frames`: how much slower it got, and whether refreshes were missed.
2. `busy_ms` and `gpu_eyes_ms` (our CPU and GPU work): if they grew with it, it is us, and the counts next to them
   (`corpses`, `thrown_weapons`, `decals`, `dlights`, `shadow_dlights`, `vr_particles`) say what.
3. `xr_submit_ms`, `gpu_submit_ms`, `xr_waitframe_ms`, `xr_acquire_ms`: if these grew while ours did not, the time
   is in SteamVR or Virtual Desktop. Then restart only SteamVR / Virtual Desktop, not the game, and see if it comes
   back.

## Hand-off

Voice notes vrfiringrange 04-35-34 (the sword falls when the upper hand lets go while the other still holds it, in
either direction) and 04-36-26 (the same for guns held two-handed: the gun should stay in the other hand, in the
pose it had on the foregrip, not usable, "almost like carrying a physics prop", until the main hand takes it again).

**Why the sword fell.** A weapon's hand letting go goes one of three ways (`DoHandImpl`, `weapons.qc`), by the
letting-go hand's hotspot: a holster puts it away, the "hand switch" spot passes it to the other hand, and
anything else (no hotspot, or the two-hand spots) drops or throws it. The hand switch spot is only the two hands
closer than 5 units (19 cm at world scale 1). Round 15's mock test had the hands 14 cm apart, so it got the switch.
With real hands on a sword grip the controllers are usually further apart than that, and a two-handed grip, once
taken, holds up to 20 units (76 cm) from its spot, so the letting-go hand was at the two-hand spot and the sword
was dropped. Guns had never been handed over. Held two-handed, the hands are always 5 to 25 units apart, so letting
go of the handle always dropped the gun.

**What it does now** (`vr_2h_handoff`, on by default; menus: VR Settings > Weapons "Two-Handed Hand-Off", and
Aiming Settings "2H Hand-Off"):

- **The server decides** (`VRTryHandOff`, `weapons.qc`). The letting-go hand's weapon would be dropped (no hotspot, the
  two-hand spots, or the new carried-grip spot). If at that moment the other hand is empty, gripping, not carrying
  an object, and steadying the weapon (the client's 2H aiming bit, or within 0.1 s of it), the weapon moves into the
  other hand's slot with its flags and clip, and the letting-go hand is empty. It plays a soft holster sound and
  gives the receiving hand a short buzz. A hand holding its own weapon never steadies another one, so dual wielding
  still drops as before. Holsters and the hand switch spot come first, as before.
- **Swords** (the knight's and the hell knight's, `WID_SWORD`, the only two-handed melee weapons) change hands. The
  other hand holds the sword normally, as its only hand, and swings it from there (the blade turns from the
  two-handed line to that hand's one-hand pose).
- **Guns** (everything else two-handed, whatever its foregrip) move to the other hand with
  `QVR_WPNFLAG_FOREGRIP_CARRIED` in its weapon flags. That flag is a hand's state, not the weapon's: it's stripped
  when the gun is thrown, holstered, switched, or taken, and on a level change (the new level has the gun held
  normally). A carried gun doesn't fire (`W_WeaponFrameImpl`), reload (holster, button and flick reloads), strike
  in melee (`PlayerVRMeleeImpl`) or aim two-handed.
- **Drawn as it was held** (`vr_twohand.cpp`, `vr_view.cpp`). Every frame a hand steadies a weapon, the view records
  where the holding hand (what the weapon is drawn from, with its mirroring) and the drawn helping hand on the
  foregrip are, relative to the helping hand's tracked pose. The record freezes when the holding hand's grip lets go,
  because the hand moves away in the frames before the server hands off. When the carry starts, the gun is drawn
  from that record, moving rigidly with the carrying hand, and the carrying hand is drawn on the foregrip as it was.
  So nothing jumps at the hand-off: the shotgun stays level, held by its pump. The ammo screen stays on the gun
  (drawn with the gun's pose). There's no muzzle, crosshair or two-hand grip spot while it's carried. If there's no
  record (a carry older than half a second, or a new connection), the gun is drawn as a normally held gun.
- **Taking it back.** The view keeps the carried gun's handle position (the hand anchor). The other, empty hand
  within 6 units of it gets the new hotspot `HS_CARRIED_GRIP` (10, `vr_twohand::updateHotspots` after the body's
  hotspots). Closing the grip there moves the gun into that hand, held normally (`VRTakeCarriedWeapon`, with a sound
  and a buzz). The carrying hand, still on the foregrip, steadies it again if it's close enough. Other ways out:
  let go of the foregrip to drop or throw it as usual; let go at a holster to holster it; let go with the hands
  together (the hand switch spot) to pass it into the other hand, held normally.
- `vr_status` prints each hand's weapon and "carrying by the foregrip".

**Multiplayer.** The server owns all of it (the flag travels in the weapon-flags stats the client already
receives). The carried pose is the owner's view only. Other players see the gun in that player's hand as the entity
data draws it (at the hand).

**Tests (mock, vrfiringrange).** Script `scratchpad/handoff/all.txt`, screenshots `handoff/all.png` (the sword right to left, then left to right;
the shotgun two-handed, carried, carried and turned, the main hand at the handle, taken back). Sword in the
main hand (`impulse 163`), the off hand gripping below it, then spread to 23 cm (hotspot 2, where it used to fall):
releasing the main hand leaves the sword in the off hand (weapon 13), which then throws it when it lets go. The
reverse (sword in the off hand, `impulse 183`, the main hand steadying): the main hand keeps it. Shotgun
(`impulse 154`), off hand on the foregrip (helping), main released: the off hand carries it, drawn in place; moving
and turning the off hand carries the gun with it; the off trigger fires nothing; the main hand at the handle gets
hotspot 10, and gripping there puts the gun back in the main hand with the off hand steadying again; a second
hand-off, then letting go of the foregrip, throws it.

**In the headset:**
- Sword, both ways: hold it two-handed, let go with the upper hand. Does it stay in the lower hand and swing
  normally from there? Is the buzz noticeable but not annoying?
- Shotgun (and the others): hold it by the foregrip, let go of the handle. Does it stay exactly where it was, level,
  hanging from the pump? Walk and turn: does it follow the hand without lag or wobble? Pull the trigger with that
  hand: nothing should happen.
- Take the handle again with the main hand: does it snap back to a normal hold, with the other hand steadying?
  Is 6 units (23 cm) from the handle easy enough to hit, or too eager (for example, when reaching past it)?
- Holster the carried gun with the carrying hand, and throw it by letting go of the foregrip.
- The main hand, now empty, can pick things up from the floor while the other hand carries the gun.

## Weapon models: double shotgun, rocket launcher

Voice notes vrfiringrange 04-45-59 (the sawn-off double shotgun's handle looks weird and amateurish), 04-47-40
(the rocket launcher has no handle at all: the hand holds nothing; it looks short) and 04-48-38 (trigger guards
and a bit more detail, without leaving Quake's style).

**Generator.** `Misc/quakevr/improve_weapons.py` rebuilds `quakevr/progs/v_shot2.mdl` and `v_rock2.mdl` from the
port's previous models, kept byte for byte in `Misc/quakevr/src_models/` (running it again gives the same files;
pure Python, no Blender). Every frame is kept (the firing animations), and so are the old geometry and skin texels
(except the shotgun's handle). New parts are low-poly and faceted as id's (bevelled boxes and octagons, 8-sided
sections) and are carried rigidly through the frames by three clusters of the gun body's vertices, so they recoil
with the gun. They are painted in the skins' unused corner (id's old hand texture), in Quake's palette:
walnut (reddish browns 16..21, 96..99) with grain, checkering and a sawn end grain; gun metal greys; the
launcher's browns. No fullbright index (224 and up) is used anywhere new: the shotgun's sight hue (vr_sights.cpp)
stays on the ring sight only, and the sight texels are untouched.

**Grips laid out on the drawn hand.** The script places `hand_base.mdl` and the finger models as vr_view.cpp
draws them on a held weapon (anchor vertex plus hand offsets, the default scales) and lays the grip out in the
hand's own space: through the curled fingers, the index finger on top at the trigger, the pinky just above the
butt, leaning back 16 degrees. The same grip in hand space is therefore the same size on both weapons, and both
fit the fist. The hand stays where it was relative to the controller; each hand is centred on its grip sideways.

- **Double shotgun** (slot 3 in the cvars, `_03`): the drooping handle (which only frames 0..2 had; the later
  frames still had the old stock, so it changed shape when firing) is replaced by a receiver behind the barrels
  (a knuckle under them up to the fore-end, the breech face just behind the chambers, a top lever), the stock cut
  down to a wrist over the web of the hand (sawn end grain at the back) and a checkered walnut pistol grip with a
  steel butt plate, a trigger and a trigger guard. The barrels, fore-end, sights, muzzle and animation are as they
  were.
- **Rocket launcher** (`_07`): a ribbed pistol grip under the back of the tube with a butt plate, a trigger and
  a trigger guard, and a back-blast nozzle behind the tube (narrow, over the back of the hand). The tube used to
  pass through the fist; the gun now sits 2 model units (about 2 cm) higher over the hand, like a pistol's slide
  over the index finger.

**Anchors and settings.** Anchor indices count the old engine's triangle strips (vr_anchor.cpp); the script ports
that strip builder and checks that every kept anchor still names the same vertex: shotgun muzzle 13, two-handed
grip 29, ammo screen 0 and the shell ejection anchor 17 (vr_shells.cpp: the chambers' ports at x 11.6 are still
in the barrels' breech ends, unchanged); launcher hand/screen/button 12, muzzle 17, two-handed grip 3. The
shotgun's hand anchor was on the removed handle and moves to the grip (65 -> 472, strip order). Both models' bounds
grow (the grips hang lower in the recoil frames), which moves the origin the weapon Scale is applied about, so the
weapon offsets are compensated to keep the old parts exactly where they were drawn. New defaults
(`vr_weapons.inc`): slot 2 HandAnchorVertex 472, HandOffset (0.641667, 0.186667, 2.216667), Offset X 5.068643,
Z -1.320374; slot 6 HandOffset Y -0.862165, Z 0.206667, Offset X 10.329055, Z 5.81605. `settingsVersion` 7 resets
both slots in existing configs once.

**Not done.** The grenade launcher (`v_rock.mdl`) has the same "holding the corner" hand; its belly sits where a
trigger guard would go, so it needs its own layout (the generator's grip would fit). No vertical foregrip on the
launcher: the fixed two-handed pose (the other hand cradling the tube) is unchanged.

**In the headset:** hold each gun and look at it from the side and from above: the fingers round the grip (not
floating, not sunk too far), the index finger at the trigger inside the guard, the butt just under the pinky; the
gun points where it did (the shotgun unchanged; the launcher about 2 cm higher). Fire both: grip, guard and
nozzle recoil with the gun; the shotgun no longer changes shape when firing. Shotgun: shells still come out of the
chambers on reload (and a flick reload); the ring sight keeps its hue setting and nothing else changes colour
with it. Two-handed hold on both, the ammo screens, the holsters. Textures under normal lighting (the walnut and
the receiver may want to be lighter or darker).

## Weapon models: nailguns, lightning gun

Voice notes vrfiringrange 04-46-57 (the super nailgun's body has grooves that are only painted: model them),
04-48-15 (the lightning gun's handle is very small: a bigger, ergonomic grip that reads as a one-handed weapon)
and 04-48-38 (the nailgun is fine; a trigger guard or some detail, in Quake's style).

**Generator.** `Misc/quakevr/improve_weapons2.py` rebuilds `quakevr/progs/v_nail2.mdl`, `v_light.mdl` and
`v_nail.mdl` from the port's previous models, kept byte for byte in `Misc/quakevr/src_models/` (pure Python, no
Blender; running it again gives the same files). Every frame is kept (recoil, the super nailgun's spinning
barrels, the muzzle flashes). New parts are low-poly and faceted, carried through the frames by the part of the gun
they are fixed to (never the barrels), and textured from 32 new skin rows (the skins grow downwards, so every old
UV and texel stays; no sight texels, no fullbright index). The paint uses each gun's own ramps: the lightning gun's
dark browns (16, 174..170, a knurled checker on the grip's sides), the nailgun's blue-black (0, 32..36). The grip
and the guards are laid out in the drawn hand's space, with the fist as measured for the double shotgun
(`improve_weapons.py`): the fingers curl round the grip, the index finger on top inside the guard, whose bar runs
between it and the middle finger.

- **Super nailgun** (slot 5 in the cvars, `_05`): the top three faces of the hexagonal body (and their corner
  chamfers) are re-cut as a grid along the painted grooves. The three grooves are now 1 unit deep, running across
  the top and down both upper bevels. Their walls lie where the skin paints them (the lit front wall facing the
  player is the painted bevel, the dark floor the painted slot), so the skin is unchanged. The silhouette is
  notched seen from the side. The outer strips are fanned from the old corners, so no new vertex sits on the
  edges shared with the lower faces (no cracks).
- **Lightning gun** (`_08`): a pistol grip under the back of the body, leaning back 16 degrees: octagonal, with
  knurled sides, plain front and back straps, a slight palm swell and a flared butt under the pinky. A small
  trigger guard round the index finger and a curved trigger. The old stub of a handle folds away inside the grip.
  The fist used to sit half inside the gun's body (its index finger inside it); the gun now sits 2.5 model units
  (about 2 cm) higher over the hand, so the index finger meets its underside. The hand stays where it was relative
  to the controller.
- **Nailgun** (`_04`): a trigger guard round the index finger, its bar carried back into the old grip's front, and
  a trigger hanging where the finger shows under the body. The hand and the grip are as they were.

**Anchors and settings.** The script ports vr_anchor.cpp's strip builder and prints every anchor's vertex before
and after. The lightning gun and the nailgun only gain parts that share no vertex with the old triangles, so all
their anchors keep their indices (lightning: hand/button/screen 57, muzzle 104, two-handed 230; nailgun: hand 5,
muzzle 33, two-handed 51, button 97, screen 0). The super nailgun's re-cut body changes the strip order: muzzle
129 -> 123 (still vertex 45, the barrel tip), two-handed grip 94 -> 564 (still vertex 25, the body's front
corner); hand/button/screen 28 unchanged. The lightning gun's grip reaches below and behind the old bounding
box, so the header's origin moves; the weapon's Scale applies about that origin, so the weapon offsets are
compensated (checked in the mock: the old parts drawn exactly where they were). New defaults (`vr_weapons.inc`):
slot 4 MuzzleAnchorVertex 123, TwoHHandAnchorVertex 564; slot 7 HandOffset (2.373392, 0.7647, -2.357872), Offset X
2.557358, Z 0.826192 (Y unchanged). `settingsVersion` 6 resets slot 4, 8 resets slot 7 (7 is the double shotgun
and rocket launcher's). The engine needs a rebuild for the new defaults (the models need none).

**Tested with the mock** (vrfiringrange, `r_fullbright 1`, poses from the side, below and behind): the grooves
read as real cuts and the nail flash still leaves the barrel tip; the lightning gun's fist wraps the knurled grip
under the body with the index finger at the trigger inside the guard, the butt below the pinky, and firing
(recoil frames) carries grip, guard and trigger along; the nailgun's guard sits round the index finger.

**In the headset:**

- Super nailgun: are the grooves deep enough in stereo (NAIL2_DEPTH, 1 unit)? Fire it: the body recoils with its
  grooves; the barrels spin as before. Two-handed hold and the ammo screen where they were.
- Lightning gun: does the hand close round the grip (not floating, not sunk), index finger at the trigger inside
  the guard? The gun is about 2 cm higher over the hand than before: does it still aim naturally, and does the
  beam leave the muzzle? Is the grip's size right (GRIP_PROFILE), and is the knurled brown too light or too dark
  next to the body?
- Nailgun: the guard should frame the index finger; the other fingers below it (the fist's fingers may touch the
  bar: it passes between them).
- Two-handed holds on all three; the holsters; the wrist/gun ammo screens.

## Weapon models: grenade launcher, shotgun, mission pack guns

The same voice notes (04-47-40, 04-48-38), for the rest of the guns held in one hand. Quake VR ships its own
`v_rock.mdl`, `v_shot.mdl`, `v_prox.mdl`, `v_laserg.mdl` and `v_multi.mdl` in `quakevr/progs/` (different from
id's, hipnotic's and rogue's), and `-game quakevr` comes last on the command line, so they win over the mission
packs' paks: the new models replace those files.

**Generator.** `Misc/quakevr/improve_weapons3.py` rebuilds the five models from the previous ones, kept byte for
byte in `Misc/quakevr/src_models/` (pure Python; running it again gives the same files). It reuses
`improve_weapons.py`'s machinery: the model reader and writer, the strip-order port of vr_anchor.cpp and its
anchor check, the rigid carrier (the new parts follow three clusters of the body through every frame, recoil
included), the lofts, and the grip, guard and trigger laid out in the drawn fist's space (GRIP_PROFILE,
GUARD_PATH, TRIGGER_PATH: the same size in the hand as on the double shotgun, the rocket launcher and the
lightning gun). Each skin grows downwards (40 to 48 new rows) for the new texels, so every old UV and texel stays.
The paint uses each gun's own ramps: blue-black metal (32..39), the launcher's and the pump's browns, the proximity
gun's dark reds, the laser cannon's browns. No fullbright index is used: the counts of texels at 224 and above
are unchanged on every skin, so the shotgun's sight texels are still the only ones vr_sights.cpp recolours.

- **Grenade launcher** (`v_rock.mdl`, slot 6 in the cvars, `_06`) and **proximity gun** (`v_prox.mdl`, `_11`,
  the same model with a red skin): the fist used to hold the tube's sloped back corner, and the belly (the deep
  body under the tube) sat where a guard would go. Now a frame under the back of the tube fills the slope and
  carries a ribbed pistol grip behind the belly, with a butt plate, a trigger, and a guard whose bar runs from the
  grip under the index finger into the belly's back face near its bottom: the belly is the guard's front. The
  frame's back end slopes up into the tube's pointed back. The gun sits 2.8 model units (about 3 cm) higher and
  4.3 units (about 4 cm) further forward over the hand, so the frame's underside rests on the index finger and the
  belly's back face is just in front of it (the pointing index finger reaches it). The grip is ribbed in the
  launcher's dark browns on `v_rock` and in dark reds on `v_prox`.
- **Multi-grenade launcher** (rogue, `v_multi.mdl`, `_14`): the same model again, modelled apart (its tube 0.06
  units higher, its belly's back face leaning from x 3.49 at the tube to 3.79 at its bottom): the same parts and
  layout, the grip in the launcher's browns, the guard meeting the leaning face where it passes (x 3.75). The gun
  moves over the hand as the grenade launcher does (2.8 units higher, 4.1 further forward).
- **Shotgun** (`v_shot.mdl`, `_02`): the fist held a thin stub of a grip that leaned back behind the fingers
  (the hand wrapped the receiver's corner). A pistol grip ribbed like the pump now goes through the fist, with a
  butt plate, a trigger and a trigger guard; the stub's lower end folds inside the grip's back (what is left is a
  small tang from the receiver's underside into the back strap). The hand, the barrel, the pump, the sights and
  the animation are as they were.
- **Laser cannon** (`v_laserg.mdl`, `_10`): held by a thin kinked blade rising from the back of the body, the hand
  above the body. Now a spade grip: a head over the fist (its tail over the web of the hand, its front over the
  index finger, with the trigger under it), a knurled grip down the fist's axis, a neck bending forward into the
  body's back, and a trigger guard from the grip round the index finger up into the head. The blade folds into
  the neck (the body's back face now closes onto it). The hand and the gun are where they were.
- **Mjolnir** is unchanged: the hand holds its shaft.

**Anchors and settings.** No old triangle or vertex index changes (the new parts are appended and share no vertex
with the old triangles; folded parts only move their vertices), and the script checks every anchor against the strip
order: grenade launcher hand/button/screen 33, two-handed 15, muzzle 0; proximity gun hand 33, screen 50, two-handed
15; multi-grenade launcher hand/button/screen 33, two-handed 15, muzzle 0; shotgun hand 165, two-handed 48, muzzle
1, button 0, screen 159, and vr_shells.cpp's ejection anchor 70 (the receiver's side, untouched: the port table is
unchanged); laser cannon hand/two-handed 4, muzzle 22, button 0, screen 226. The bounds change (grips lower, a stub
or blade folded away), which moves the origin the weapon Scale applies about, so the offsets are compensated. New
defaults (`vr_weapons.inc`): slot 1 Offset X 0.597903, Z 1.180206; slot 5 HandOffset (-1.51514, -1.515769,
0.775112), Offset (12.741572, 2.263517, 4.199072); slot 10 the same HandOffset, Offset (12.741546, 2.263517,
4.410033); slot 13 HandOffset (-1.401397, -1.521379, 0.772203), Offset (12.544076, 2.268325, 4.278901); slot 9
Offset X -1.434042. `settingsVersion` 9 resets slots 1, 5, 9, 10 and 13 in existing configs once. The engine needs a
rebuild for the new defaults.

**Tested with the mock** (vrfiringrange, `r_fullbright 1`, `impulse 154/158/159/162`, and `impulse 158; impulse 9;
impulse 43` for the multi-grenade launcher, poses from the side and the author's pose): each fist closes round its
grip with the index finger in the guard; the recoil frames carry the new parts with the gun (the launcher's recoil
pitches it 25 degrees: the grip turns with it).

**In the headset:**

- Grenade launcher, proximity gun, multi-grenade launcher (toggle the secondary ammo on the grenade launcher):
  does the hand close round the grip, the index finger inside the guard, the
  pointing finger just touching the belly's back? The gun is about 3 cm higher and 4 cm further forward over the
  hand than before: does it still aim naturally? Grenades still leave the muzzle; fire them: grip, frame and guard
  recoil with the gun. Two-handed hold, the ammo screen and the button where they were.
- Shotgun: the grip in the fist (not floating, not sunk), the index finger at the trigger; the ring sight still
  takes the sight hue and nothing else changes colour; shells still come out of the port on the receiver's side
  when firing and pumping.
- Laser cannon: the head sits over the index finger and the grip fills the fist; the neck meets the body without
  a gap. Is the head too plain or too big?
- Textures under normal lighting: the ribbed grips (brown, red) and the laser's knurl next to their bodies.

## Parry, corpses, laser cannon

Voice notes vrfiringrange 04-40-19 (parry doesn't work any more, with the axe or the sword, one hand or two; a
weapon held horizontally in front should parry, a gun too), 04-41-37 (corpses need more damage to gib: double for
normal enemies, more for the ogre, the shambler and so on) and 04-43-14 (the laser cannon stays in its firing
animation for a few seconds when it runs dry).

### Parry

**Why it failed.** The parry's code (`VR_Parry_Blocks`, `combat.qc`) hadn't changed since round 6: it tested the
hand's *forward*, the aim a gun would shoot along, and wanted it square to the blow (within `vr_parry_angle`, 50).
A gun's barrel is its forward, so a gun held across worked. But a sword's blade and the axe's handle are about 70
degrees off the hand's forward (the round-15 two-handed sword notes measure 63 to 77). Held level across in front
with the knuckles toward the enemy, the natural hold, the hand's forward points *at* the attacker, and the old
test refused it. It passed only with the wrist rolled so the forward pointed up or down, and it also passed with
the sword pointed straight at the knight (a thrust, no guard). Round 15 made it worse for the two-handed sword.
Before, the sword aimed two-handed as a gun does (`TwoHMode` 2: once the second hand took hold, the forward
followed the line between the hands, which is across when both hands hold it level). Since round 15 (`TwoHMode` 3) the blade lies along the hands, and
the forward is 70 degrees off it again. The probe below prints the old test next to the new one for the same poses
(mock, e1m1, a blow from straight ahead):

| pose (mock) | weapon line | old test | new |
|---|---|---|---|
| sword, one hand, level across (`-30.5 90 0`) | 10° off level, across | yes | yes |
| sword, one hand, across, 30° up (`39.5 30 90`) | 29° off level, 74° off the attacker's line | no | yes |
| sword, two hands, level across (hands side by side) | 2° off level, across | **no** | yes |
| sword pointed at the knight (`-30.5 0 0`) | level, 4° off the attacker's line | **yes** | no |
| sword upright (`39.5 0 0`) | 80° off level | no | no |
| axe, level across (`-30.5 90 0`) | 16° off level, across | yes | yes |
| axe, across, 39° up (`39.5 30 90`) | 39° off level, 45° off the line | no | yes |
| shotgun across (`39.5 90 0`) | 5° off level, 88° off the line | yes | yes |
| shotgun aimed at the knight | level, 1.5° off the line | no | no |

**The new rule** (`VR_Parry_Blocks` / `VR_Parry_LineBlocks`). The test uses the weapon itself: a line from its
butt (a third of its length behind the hand: a sword's pommel, a gun's stock) to its far end (the muzzle point:
the sword's tip, the axe's head, the gun's muzzle, the same anchors as the melee striking points). Held with two
hands, the line through both hands counts too, whichever way the weapon is drawn. A melee blow is parried when
either hand's line is:
- **about level**: within `vr_parry_angle` of horizontal (now 40 degrees; it was "off square to the blow");
- **across**: not pointing at the attacker (at least 30 degrees off its line, seen from above);
- **in front**: some point of it (five along the line) ahead of the body, measured from the head or from the
  middle of the player's box, whichever is further back. It must be up to `vr_parry_reach` ahead (new, 1.5 m;
  the old test took the hand up to 1.68 m ahead), within 90 cm to the side, and from 1 m below the eyes to 45 cm
  above them.

One hand or two, sword, axe, Mjolnir or any gun. The rewards are unchanged: the clang, sparks at the blocking
point (they were at the hand), haptics, the arm knock (`vr_parry_wobble`), the push-backs, the damage cut
(`vr_parry_reduction`), and the drop chance for one hand (`vr_parry_drop_chance`; 0 in the shipped defaults). The
crossed-arms parry is unchanged. The **bash** guard keeps the old hand-forward test (now `VR_Parry_HandAcross`,
still 50 degrees), so bashing is unchanged.

**Settings.** Gameplay > Parry and Bash: Parry Angle, Parry Reach (also on the old Melee Settings page). Config
version 10: a saved `vr_parry_angle 50` (the old default, with the old meaning) becomes 40.

**Developer lines** (`developer 1`): "parry: monster_knight with hand 1 (two hands)" when a blow is parried, and
now "parry: none: monster_knight at -90 degrees, 35 units" when it isn't, giving where the blow came from (0
ahead, positive on the left) and how far.

**Test impulses** (single player): `impulse 248` spawns a knight 64 units ahead of the head. `impulse 249` prints,
for each hand, whether its pose parries a blow from straight ahead: the line's angle off level and off the
attacker's line, the old test's answer, two hands, health, and the line's direction.

**Tests (mock, e1m1, knight from `impulse 248`, `vr_parry_drop_chance 0`, no god mode).** One-hand sword across:
the first blows from ahead were all parried (health 100 → 98 → 92 over four parries). Once pushed back, the knight
came round to the player's right (-90 degrees, since the mock player never turns), and there the blade points at it
and doesn't parry, as it should. Sword pointed at the knight: no parries, 100 → 14 in 4 s. Shotgun across and axe
across: parried from ahead. Two-handed sword level across: four parries in a row (health 100 → 95, 1 to 2 a blow).
The old test said no to that pose. Sword upright: no parries.

**In the headset** (`developer 1`, a knight or hell knight: e1m2/e1m3, or the firing range's waves):
- Hold the sword (one hand, then two) level across your chest or face as the knight swings: "parry" lines, the
  clang, a knock. Try the axe, and a gun held across by its grip.
- If it's still too strict, raise Parry Angle (Gameplay > Parry and Bash) or Parry Reach. If it's too easy (a
  weapon hanging at your side parries), lower them. A "parry: none ... at N degrees" line says where the blow came
  from: blows from the side need the guard turned to face them.
- A sword pointed at the knight should not parry. Neither should one held upright.

### Corpses

`vr_corpse_health` is now **80** (was 40), times a toughness per monster, set in `VR_Corpse_Parts` (about
(its health / a knight's 75) to the 0.6, at least 1). Before, the size-based factor gave 1.6 to every big monster
(not the hell knight: it's knight-sized).

| monster | toughness | corpse health (was) |
|---|---|---|
| grunt, enforcer, dog, knight, scrag, rotfish, eel | 1 | 80 (40) |
| gremlin | 1.25 | 100 (40) |
| ogre | 1.75 | 140 (64) |
| hell knight | 2 | 160 (40) |
| fiend, centroid (scourge) | 2.25 | 180 (64) |
| vore | 2.75 | 220 (64) |
| shambler | 3.5 | 280 (64) |

The menu's Corpse Health slider (Gameplay page) goes 10..300 in steps of 10. Config version 10: a saved 40 (the
old default) becomes 80. Mock: a knight's corpse "lies still, 80 to gib"; a shotgun blast took 24 (56 left).

**In the headset:** gib a grunt's and a knight's corpse (about two to three shotgun blasts, or a few sword blows),
then an ogre's and a shambler's. Is it about right, too tedious for the big ones, or still too quick?

### Laser cannon

**Cause.** `player_laser1..4` (`player.qc`) called `player_laser_Impl` and then set the view model's frame and
fired. Out of cells, `player_laser_Impl` went back to `player_run` (idle frame 0) and returned, but the caller
went on and set its firing frame (1) anyway. The think chain was now the stand animation's, and `player_stand`
never resets the weapon frame, so the cannon kept its firing frame until the player next moved or fired. The
original hipnotic code switched weapons when empty, which hid this.

**Fix.** `player_laser_Impl` takes the frame and the shot (`xFrame`, `xStat`) and sets them itself, after the
cells check. Mock (`give c 6`, fire held): the weapon frame went 4, 4, then 0 as soon as the cells ran out (0.2 s),
and stayed 0 with the trigger still held and after release. Before, it stayed at 1 through the 4 s held and the 1 s
after release.

**In the headset:** empty the laser cannon with the trigger held, in either hand: it should drop to its idle look
at once, with the click of an empty gun.

## Review: QuakeC

A review of the QuakeC from rounds 15 and 16 (`git diff 947ab035 -- QC/`), looking at how the features meet.
Fixed:

- **An impulse weapon switch kept the hand-off's carry** (`W_ChangeWeapon`, `weapons.qc`). The number keys (and
  impulse 225/226) change the main hand's weapon but kept its flags, so a gun the main hand carried by its foregrip
  left the new weapon "carried": unable to fire. Mock: off hand on the shotgun's handle, main on its foregrip, off
  released, `impulse 5`: the nailgun was still "carrying by the foregrip"; now it's held normally. The other ways a
  weapon changes (the cycle impulses, the test impulses, pickups, a gremlin's theft) already cleared the flags.
- **Eel corpses couldn't be shot or struck** (`VR_Corpse_Arm`, `combat.qc`). The eel's last death frame loops and
  sets `SOLID_NOT` every 0.1 s, undoing the corpse's `SOLID_NOT_BUT_TOUCHABLE`, so only explosions reached it. The
  loop now stops when the corpse is armed. Mock (r1m3, `impulse 205`): the four eels' corpses stayed solid 0; now 5,
  like the others.
- **A corpse's first hand strike could come from the wrong hand** (`VR_Corpse_StrikeFrame`). A corpse takes one hand
  strike per 0.35 s, and the main hand is checked first. With the off hand holding the sword two-handed, the
  steadying main hand struck first with its fist, which was weaker and made the wrong sound, and the sword's strike
  was lost. A gun carried by its foregrip also struck. Both are now skipped, as they are for blows
  (`PlayerVRMeleeImpl`). Mock: fist swings still gib a grunt's corpse (46 a strike, gibbed on the second).
- **A corpse a gremlin gorged on stayed a corpse** (`ThrowHead`, `player.qc`). The gremlin turns the corpse itself
  into a head, but `vr_corpse` stayed 2, so a hand's corpse strike also hit the head, on top of the gib's own
  strike. `ThrowHead` now clears it.
- **Parry with a two-handed weapon in the off hand** (`VR_Parry_Blocks`). The line through both hands was tested
  only for the main hand's weapon. An off-hand weapon steadied by the empty main hand is two-handed too (the
  client's 2H aiming bit). Now it's tested for either hand. The segment is the same either way round.

Checked and fine: the carry flag is cleared on every other path (level change, respawn, throw, drop, holster, hand
switch, cycle, death drops); pickups can't go into a carrying hand; `ejectcasings`, `haptic` and `handimpact` go
nowhere for bots (inactive clients); splashes are deduplicated and size-checked by the engine; a batted projectile is
aimed back only at a living thrower (a dead one's health is 0 or less, and the freed edict of a dead one keeps it); the test impulses 243-249 are single player only; new sounds
and models are precached (heads by their monsters). Multiplayer smoke test (`deathmatch 1` then `coop 1`,
`maxplayers 4`, two bots, test impulses, shotgun fire, suicide and respawn): no errors. The round's hand-off script
gives the same results as before.

Not changed (notes):
- A savegame keeps a carried gun carried. After loading, the client has no pose record, so the gun is drawn as held
  normally but still can't fire until the other hand takes its handle, or the hand lets go.
- `blow_wall_name` (`vr_juice.qc`) is written but never read.
- The weapon-cheat impulses 150-189 (from before this round) are blocked in deathmatch but still allowed in co-op.

## Review: engine

Rounds 15 and 16's engine code (`git diff 947ab035 -- Quake/`) read through for bugs between features, leftovers and
per-frame costs. Fixed:

- **`VR_TouchLinks` scanned every edict for every mover** (`vr_physics.cpp`, round 15's "Leaks" note). Each relink of
  a player, monster, missile or thrown weapon looped over all edicts, so the cost grew with movers times edicts. It
  now asks the area nodes (`SV_AreaEdicts`, new in `world.c`) for what is near the body's box and each hand's reach,
  and takes them in edict order, as the scan did. A temporary check against the old scan over about 9000 relinks
  (walking, every weapon fired, thrown weapons, three maps) found no difference, and the hand-off script gives the same
  results.
- **Shots into water took up to 340 point-contents per pellet** (`physics::liquidEntry`, QC's `liquidentry`, asked for
  every pellet of every shot). A super shotgun blast made about 4800 BSP descents, a few tenths of a millisecond in
  one server frame. It now walks the world's BSP leaves along the segment once, and the crossing is the plane's own
  instead of a bisection. Checked against the old stepping on e1m2 (shotguns, nails and grenades into and across the
  moat and the shallows, 16 places and directions): the same answers, within a unit.
- **The ammo screens were redrawn every frame** (`text3d::renderScreens`, noted under "Slowdown"). Each image is now
  drawn (a framebuffer pass and a mipmap rebuild) only when its text, alignment, size or colours change: once per shot.
- **Profiler phases: a GPU query past the end of its slot** (`vr_profile.cpp`, `endPhase`). A begin kept room for its
  own end only, so nested GPU scopes near the 96-query limit could write one past the array. That end is now skipped
  (the record isn't read back). Not reached today (about 45 queries a frame).
- **Memory log: no "map" row after loading a save or restarting the same map** (`vr_main.cpp`). A new map was told by
  its model, which Ironwail keeps for the same map. Loading now always starts the new map's 5 s count.
- **The geometric waves' buffers were kept after a map change with the waves off** (`vr_water.cpp`). They are freed
  on the map change.
- **Reloading the sighted skins** as `vr_sight_hue` changes (`TexMgr_ReloadImagesNamed`) now sets the flag Ironwail's
  own full reload sets, so a cache eviction during the reload can't free a texture under the walk.
- Small: a `static_assert` that the particle atlas has room for its cells (11 of 12 used); stale comments
  (`vr_particles.hpp` named a QC splash preset that doesn't exist; the waves' ranges); LIGHTING.md (the sights'
  colour, the lava nail and beam lights and their presets, the cost of round 15's baked bumps).

Checked and fine:
- Map changes, `vid_restart`, `vr_restart`, save and load: the waves' mesh is rebuilt for each map (and when
  `vr_water_geo_cell` changes), and Ironwail's `vid_restart` keeps its buffers. Shells, the hand-off records and the
  flick state are cleared with the client's state. The world text boards reset with the map. Beam and lava nail
  lights last a frame and go with the dlights. The refraction's scene size follows `vid`.
- `r_novis` and BSP2: the mesh's faces are picked with the view's PVS (every leaf with `r_novis`); leaf and
  mark-surface indices are ints.
- Both eyes: the mesh's faces are picked per view (each eye's frustum); the lights, shells, trails and boards once a
  frame.
- Protocol: `QVR_SVC_EJECT` (7 bytes, reliable, to the firing player only) is read in full before `vr_shells` is
  looked at, so a demo plays whatever it is set to. As with every round's new sub-commands, `svc_quakevr` has no
  version: a demo recorded now stops with an error in an older build. The splashes and point sounds go into the
  datagram with size checks, at most 3 sounds a frame.
- Config: version 10 moves a saved `vr_parry_angle 50` to 40 and `vr_corpse_health 40` to 80 (`vr_defaults.cfg`
  sets neither). The only new cvar callbacks (sight hue and saturation) are safe while the config loads.
- Timing: the weapon's weight smoothing and the throw fit use real time on purpose; shells, splashes' particles,
  lights and trails use `cl.time` (they stop when paused); the water sounds and hand splashes the server's time.

Not changed (notes):
- The weapon-cheat impulses (150 + id) accept any id. 164-167 (no such weapons) raise QC assertions and leave a thrown
  weapon with a NaN origin, which `keepInWorld` then puts back every frame (QC side).
- `VR_SightPalette` sets its cvar callbacks on the first texture upload after the cvars are registered. It works; an
  init call would be plainer.
- A map's world text boards keep their images after it is left, for the next map's boards to reuse: bounded by the
  most boards seen, not a leak.

Tests (mock, Release build, 0 warnings): e1m1, e1m2 and vrfiringrange in turn with every weapon fired, the waves
turned off across a map change and on again, `r_novis 1`, `vr_water_geo_cell 8` and back, `vid_restart`,
`vr_restart`, a flick's casings (`vr_shells_eject 1 2 1`), save and load, `vr_memstats`, the memory log every 5 s and
`vr_profile 1`: no errors, the GL counts flat, a "map" row after the load. The hand-off script: the same results as
before. Shots into e1m2's water: 34 splashes. The ammo screen: redrawn 14 times in 2200 frames (once per shot, and
when its colour changed).
