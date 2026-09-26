# Round 20: your seventh batch of notes, upscaling, ambient occlusion

The notes from the firing range, e1m1, e3m1, e4m1 and start (the evening of 2026-09-26), plus the features asked for
in chat: FSR 1 / NIS upscaling and foveated rendering, dynamic ambient occlusion, finger poses on grips, the
flashlight on guns, wearable armour. Your tuned settings and weapon placements ship as the defaults.

| Area | Result |
|---|---|
| Your settings as defaults | `quakevr/vr_defaults.cfg` regenerated from your config (93 settings, graphics ones included); your 39 weapon placement changes baked into `vr_weapons.inc` (slots 1, 2, 5, 6, 7, 9, 10, 17, and the alternates 13, 14, 15 by the same amounts), applied once (`vr_wofs_version` 13) |
| Menus | Weapon Only X/Y/Z (the gun moves, the hand stays); the right stick only scrolls; force grab saturation; the tall panel no longer shows the Quake plaque |
| Firing range | 13 more monster buttons on the other side (`vr_spawnpanel.bsp`, `vr_spawnbutton.bsp`) |
| Game messages | a hologram over the wrist gadget ("You need the gold keycard", secrets, pickups), apart from the diagnostic log |
| Weapon models | the nailgun's grip; see-through faces sealed (super nailgun, lava ones, grenade/mine launchers, rocket launcher, the hinge); lightning gun sights in the player hue; a hole checker (`Misc/quakevr/check_mdl_holes.py`) |
| Body | hip holsters follow the thighs; wading and swimming legs kick where the stick moves you |
| Weapon effects | the flash no longer shrinks in two hands; lava nailguns glow; a morph between a gun's normal and lava models; ammo screens and buttons only on the gun in a hand; the invisible holstered gun fixed |
| Water | ripples move the wave geometry (sliders); recorded water sounds (Freesound, CC0) |
| Lava, barrels | lava lights the walls (relit maps: rerun the relight); exploding barrels have the rocket's explosion light |
| Physics | items stuck in corners no longer spin; held objects sit against the drawn hand; carrying starts on the grip's press only (a fist moved onto a gib no longer grabs it) |
| Upscaling, foveated | FSR 1 or NIS for render scales below 1; foveated rendering (NVIDIA variable-rate shading) saves 18/45/64% of the world pass at levels 1/2/3 |
| Finger poses | per-weapon finger and thumb offsets and openness, for grips that overlap the hand |
| Melee | blade, hilt, bash and shove told apart by pose, hold and speed; a bash or a shove with a weapon bats projectiles back |
| Flashlight | clip it to a gun (B or Y); it goes off when a map is started afresh (on across level changes) |
| Armour | armour pickups are physics objects, worn by letting go over your torso |
| Ambient occlusion | round what moves, boxes round brush models, models' own creases: 0.05–0.14 ms at the Quest 3's size |

Also: the integration test's hang was the test script's (`togglemenu` from the VR menu goes to the main menu, where
`quit` asks to confirm), not the game. The QC compiler note in the melee section below is corrected there: only the
`? :` precedence is fteqcc's quirk.

## Menus: weapon-only offsets, right stick, force grab saturation

Notes vrfiringrange_2026-09-26_20-02-57, 20-11-18, 20-23-04 and 20-25-10.

### Weapon Only X/Y/Z (Weapon Offsets page)

The offsets move the hand too because the drawn hand is placed at an anchor vertex of the weapon model
(`view::anchorPosition(weapon, HandAnchorVertex, HandOffset)`), so it rides on the weapon. In the weapon entity's
frame (turned by the hand's angles plus the weapon's Pitch/Yaw/Roll, mirrored for the off hand), before Ironwail's
model matrix, `vr_render.cpp` puts

- a point `a` of the weapon model (`scale_origin + scale * Scale * vertex`) at `k * (Offset + a)`
  (`S(k) * T(Offset)`, `k = ModelTransform::k = vr_world_scale / 0.75 * vr_gunmodelscale`), and
- the drawn hand at `offsetScale() * HandOffset + k * (Offset + a_hand)` (`anchorPosition`'s extra is applied before
  `S(k)`, times `offsetScale() = vr_world_scale / 1.25 * vr_gunmodelscale / 0.7`).

So `Offset += d` moves every point of the weapon (muzzle, foregrip, ammo screen) by `k * d`, and the hand stays put
when `HandOffset -= d * k / offsetScale()`. `k / offsetScale() = (1.25 * 0.7) / 0.75 = 7/6` at any world or gun
model scale (computed from the two functions anyway). Both offsets are in the same mirrored frame, so the signs
agree for the off hand too.

`weapons::moveWeaponOnly(slot, d)` does exactly that to the real `vr_wofs_x/y/z_NN` and `vr_wofs_hand_x/y/z_NN`
cvars; the new sliders are "virtual": `vr_weapon_only_x/y/z` (not archived) whose change by `d` calls it for the
page's weapon (the main hand's from the console), zeroed whenever the Weapon Offsets page is built or the weapon is
reset. The page shows them under *Weapon Only (Hand Stays)*, in Offset's units and axes, for weapons (not the empty
hand's slot). Nothing else changes: the muzzle, aim, two-handed grips and screens follow the real offsets as before.

No weapon-only Pitch/Yaw/Roll: the weapon's angle offsets are *added* to the hand's Euler angles (`place()` with
`{-rot.x + o.x, rot.y + o.y, rot.z + o.z}`), not composed with them, so turning the weapon about the grip with the
hand fixed would need a different compensation for every hand pose (it is exact only while the hand is level).

Verified in the mock (`vr_dumpview` prints origins to 0.001 now), shotgun in both hands (slot 2, `impulse 154/174`),
`vr_world_scale 1`, `vr_gunmodelscale 0.7` (`k = 0.9333`):

| step | main hand_base | off hand_base | main muzzle moved | off muzzle moved |
|---|---|---|---|---|
| start | 304.611 -551.531 66.200 | 308.030 -558.388 67.074 | | |
| `vr_weapon_only_x 2` | unchanged (0.000) | unchanged (0.000) | (-1.808 -0.249 -0.392), 1.867 = k * 2 | 1.867 |
| `_y -1.5`, `_z 0.7` | unchanged | unchanged | 1.545 = k * 1.655 | 1.545 |
| control: `vr_wofs_x_02` +2 alone | moved 1.867 | moved | 1.867 | |

The cvars went Offset (0.598, 1.933, 1.180) -> (2.598, 0.433, 1.880), Hand (2.400, 0.2, 0.7) -> (0.067, 1.95,
-0.117): 7/6 of each step. Through the menu (`menu_vr 18`, `vr_world_scale 1.25`, `vr_gunmodelscale 1`, `k =
1.667`): the off stick's right twice, then once more, set Weapon Only X to 0.3, Offset X +0.3, Hand X -0.35; the
hand stayed to 0.000, the muzzle moved 0.4995 (k * 0.3 = 0.5).

### The main hand's stick in menus

`vr_input.cpp`: in menus the main hand's stick (the right one, unless Left Handed) now only scrolls a page with a
scrollbar (`menuui::scrollStick`) or, on pages without one, is DPAD up/down; its left and right are ignored, so it
can't change sliders, cycles or toggles (in the VR pages and Ironwail's menus alike: they all take the same DPAD /
arrow keys). The off hand's stick, the laser and A are unchanged: the off stick still changes values. Verified in
the mock: on Weapon Only X, the main stick held right then left (40 frames each) changed nothing; the off stick's
right did.

### Force grab saturation

`vr_forcegrab_saturation` (default 0.7, 0..2; Colours page, under Force Grab Hue): each force grab part's made
saturation times it, and times `vr_player_saturation` while the hue follows the player's (as `vr_sight_saturation`
does; `hue::saturation(own, ownSaturation, s)` in `vr_hue.hpp`). It colours the aiming beam, the tendril, the
target's glow (`VR_EntityGlowColor`) and the sparkles (`particles::inForceGrabHue`: the flying object's trail and the
gun pickup's sparkles). The luminance matching keeps the blue reference at the made saturation, so a paler setting
doesn't glow much brighter. Default 0.7 brings the tendril's halo (made 0.83) to 0.58, the wrist gadget screen's lines
and text (0.55..0.58); the glow 0.65 -> 0.46, the sparkles 0.75 -> 0.53. Selecting the setting shows the preview
tendril from the off hand, as the hue does. Screenshots: 1, 0.7, 0.3, and hue 215 at 0.7 (scratchpad `fgsat20.png`).

## Firing range: more monsters

Note vrfiringrange_2026-09-26_20-25-57: buttons for the missing monsters, "on the other side".

The map's five buttons (soldier, ogre, zombie, shambler, wizard) stand on the west railing: each a `func_button`
brush (door02_1, `+0basebtn` face) on a tech10_1 panel (16 x 64 x 80, world brush), a `func_worldtext_banner` label
under it, and a `func_enemy_dispenser` in the middle of the range that the button targets (`"weapon"` = which
monster). The second row, thirteen buttons, stands on the other side: along the platform's east edge (x 380..396, the
floor ends at 396), facing west, north to south (left to right as you face it): knight, death knight, dog, enforcer,
fiend, vore, spawn, gremlin, centroid (Scourge of Armagon), mummy, phantom swordsman, wrath, overlord (Dissolution of
Eternity). Thirteen 64-unit panels side by side fill y -44..-876 exactly: the north end stops short of the health
and armour platform's side, the south end of the ammo mat (x 192..383, y <= -878.5); nothing else is within the
panels' box (checked against the BSP's hull, z 18..110), and the player start (316 -556) is 56 units in front.

- **Models**: the entity file can't add brushes, so each button is two point entities drawing small external brush
  models, as the ammo boxes do: `maps/vr_spawnpanel.bsp` on a `func_wall` and `maps/vr_spawnbutton.bsp` on a
  `func_button` (`"angle" "0"`: pushed into the panel; pressed by a hand, a weapon or the body like the map's
  buttons, same code). They are the map's own soldier button and panel brushes, textures and alignment included,
  turned to face west, built by `Misc/quakevr/make_spawn_buttons.py` (ericw-tools qbsp `-nofill` and light: a flat
  minimum light and one light in front, since they can't share the map's lightmap; about as bright as the first row).
  `func_button` and `func_wall` now precache a model that isn't the map's own (`VR_PrecacheOwnModel`, buttons.qc).
- **Labels**: `func_worldtext_banner` as before, `"worldtext_scale" "0.75"` (the panels touch; the first row's
  are 16 apart) and two lines for the long names (`death\nknight`, `phantom\nswordsman`).
- **Dispensers** (buttons.qc): ids 5..17 (the list is in the comment above `VR_EnemyDispenser_Model`), spread in a
  grid around the first row's (flyers higher). The spawned monster now gets its map classname (also the first
  row's: `func_enemy_dispenser_use` left it empty, so their sight sounds and the like, which go by classname, were
  missing). A new dispenser precaches only its monster's model at load; the rest (heads, sounds) is precached late
  on the first press (VR progs allow it).
- **Missing data**: the mission packs' monsters (and the registered game's) may not be installed, and precaching a
  missing model is an error. A new VR builtin, `float(string path) fileexists` (`vr_builtins.cpp`,
  `COM_FileExists`), guards them: the model isn't precached, and pressing the button centreprints "Needs Scourge of
  Armagon (hipnotic)" / "Needs Dissolution of Eternity (rogue)" / "Needs the registered Quake (pak1.pak)" instead.
- **Left out**: rotfish and eel (water monsters: the water is the lake below the platform, out of reach), the spike
  mine (`monster_spikemine` is a stub with the fiend's model; the real one is the trap `trap_spike_mine`), the
  bosses (Chthon, Shub-Niggurath, Armagon, the dragon, Hephaestus, the guardians), the statue and ogre-marksman
  variants.

Verified in the mock (`vr_body_interactions 1`, the body walked into each button): the thirteen buttons each spawn
their monster (`edicts`: one each of monster_knight, _hell_knight, _dog, _enforcer, _demon1, _shalrath, _tarbaby,
_gremlin, _scourge, _mummy, _sword, _wrath, _super_wrath), no errors; the first row unchanged. QC: 0 warnings.
Screenshots: the new row from the front and both ends and the first row (scratchpad `range20f.png`), a pressed
button and the spawned monsters (`range20g.png`). The "needs" message path is untested here (the test game has both
mission packs).

## Game messages as a hologram

Note e4m1_2026-09-26_20-33-12: messages such as "You need the gold keycard" shown like the log over the wrist gadget,
projected by it as a hologram (a light cone, an effect on the text), and the game's messages told apart from the
diagnostic ones (those higher up or farther away).

### Which messages are the game's

- **Centre prints** (`SCR_CenterPrint`, hooked by `VR_GameCenterPrint`): a key needed, a secret found, the maps'
  trigger and door messages, runes. Not the intermission's (finale, cutscene: the gadget is put away then) nor the
  options menu's preview (ignored while a menu is open).
- **Server prints** (`svc_print`: `cl_parse.c` now calls `Con_ServerPrint`, which marks the console lines it prints as
  the server's, `vr_con_server[]` next to the wrist log's `vr_con_times[]`): the progs' `sprint`/`bprint`, i.e.
  pickups ("You got the Grenade Launcher", pieced together from several sprints into one console line), powerups
  running out, deaths, chat. Less the engine's own replies sent the same way (`engineLine` in `vr_gadget.cpp`: the
  server's `VERSION` banner, one word then `ON`/`OFF` as `godmode ON`, `noclip OFF`, `usage:`/`current values:`,
  ping lists, `"x" changed to "y"`, pause, kicked, and lines with no letters such as setpos's figures).
- **Everything else is diagnostic**: the engine's own `Con_Printf`/`Con_DPrintf`/`Con_Warning` (the `VR: ...` lines,
  cvar changes, errors, "Wrote screenshots/..."). These stay in the wrist log.

The centre print's echo in the console (Ironwail's `con_logcenterprint` quake bars) is left out of the log, and
`Con_LogCenterPrint` no longer wipes the log's recent lines (it cleared the notify times; the log keeps its own).

### The hologram

`vr_gadget.cpp` (`collectMessages`, `renderHologram`, `layoutHologram`, `drawHologram`):

- The messages of the last `vr_messages_hologram_time` seconds (5): the game's console lines and the centre print,
  oldest at the top, the newest nearest the gadget, at most 10 lines / 6 messages (40 characters a line, wrapped).
  A repeated centre print (a locked door touched again) keeps the message on instead of adding another.
- Their text is drawn white on black into a small image (336 x 128 font pixels, 4 texels each, with mipmaps for the
  glow: target "gadget hologram"), only when the messages change; each message a block of its own.
- Each eye's translucent pass (`text3d::drawTranslucent` calls `gadget::drawHologram`) draws one quad per block,
  world-space and facing the viewer (laid out once a frame, like the log), its bottom `vr_messages_hologram_height`
  cm (1) over the top of the gadget as seen (the casing's highest corner along the view's up, whichever way the
  wrist is turned), in the gadget screen's colour (`vr_gadget_screen_hue`, the player's by default), not depth
  tested (over the other hand, as the log).
- New shade `Shade::Hologram` (mode 5 in `vr_gfx_gl.cpp`): the text's strokes with whitish cores and a soft halo
  (the screens' `glow()`), a dark haze round the letters (premultiplied alpha, up to 0.6) so it reads against a
  bright wall, scanlines drifting up, a slow bright band, a flicker, a tiny vertical shake, the colours split a
  little; the CRT screens' glitch bursts (bands torn sideways) now and then and whenever a message arrives.
- The beam: four additive faces from the gadget's screen (a little inside its edge) to the blocks' outline, a
  frustum of light fading across each face and towards the text, with slow streaks and scanlines rising through it.
- The projection: when the hologram appears (a message comes, or the wrist is raised again after 0.4 s away) it
  grows out of the screen over 0.35 s (from a thin line, narrower, with a glitch and the beam brighter); a new
  message's block grows over 0.2 s, pushing the older ones up. Each message fades in (0.15 s) and out (0.6 s).
- Shown only while the screen faces the viewer (as the log: fading in from 0.2 to 0.5 of facing) and is in view.
  Otherwise the centre print shows in front of the head as before: `SCR_CheckDrawCenterString` asks
  `VR_CenterPrintOnWrist()`, which hides it only while the hologram shows it (at least half faded in), so a message
  is never lost; the server's prints wait in the hologram until they time out (as they did in the log).
- `vr_messages_hologram_effect` (1; 0..2) scales the look: 0 is plain glowing text, no beam, no projection.

### The log (diagnostics)

The wrist log keeps the engine's lines only (the game's lines are the hologram's while it is on). It floats higher:
`vr_notify_wrist_height` cm (5) over the gadget as seen, and always 1.5 cm over the hologram's top (following it
smoothly as messages come and go); it is dimmer (`vr_notify_wrist_alpha` 0.7) and its colour greyed 40% towards
its luminance, so the two read as different things.

### Settings

Screens page ("HUD and Menus" section, `pageScreens`), new "Messages" header: Game Messages as Hologram
(`vr_messages_hologram` 1), Hologram Time (`_time` 5 s), Hologram Text Size (`_size` 1: about 8 mm characters),
Hologram Height (`_height` 1 cm), Hologram Effect (`_effect` 1), then the log's: Console Messages (`vr_notify_wrist`,
renamed from "Messages"), Console Message Time, Console Log Height (`vr_notify_wrist_height` 5 cm), Console Log
Brightness (`vr_notify_wrist_alpha` 0.7). `vr_message_test <center|print|console> <text>` (`\n` for new lines)
sends a message down each path.

### Tested (mock)

e1m1 and vrfiringrange, the off hand raised like a watch (`vr_mock_look 45 0; vr_mock_hand off 0.1 1.3 -0.3 -45 -90
-90`): the real paths (the armour's pickup "You got armor" via `sprint`, "You found a secret area!" from a
`trigger_secret` via `centerprint`), the test command's centre print, print and console line; `godmode ON`,
`noclip ON`, `notarget ON` and "Wrote screenshots/..." in the log, not the hologram; the wrist turned away: the
centre print back in front of the head; `vr_messages_hologram 0`: as before (the centre print in view, everything in
the log); the projection frame by frame; effect 0 and 2; on the bright firing range floor. Scratchpad `holo20.png`
(close-ups: two messages and the beam; the real pickup and secret; the multi-line message on a bright floor; the
wrist turned away), raw shots in `holo/p4`..`p7`.

To check in the headset: the text's size and height over the gadget (Hologram Text Size, Height), whether the beam
is too strong or too faint in dark and bright places (Hologram Effect), the scanlines and flicker at the Quest's
resolution (the mock's 960 x 540 hides most of them), that the centre print comes back in view as the wrist is
lowered, and that the log over the hologram stays out of the way.

## Weapon models

Notes vrfiringrange_2026-09-26_20-06-59 (the nailgun's handle looks weird and primitive: a grip like the shotguns'),
20-08-09 and 20-08-30 (the super nailgun's barrel ends and part of its handle are see-through; the lava one too),
20-09-45 (the grenade and mine launchers have missing faces seen from the other side), 20-15-13 (the nailgun's
hinge under the barrels shows from above, not from below), 20-23-25 (the rocket launcher's muzzle end is missing a
face) and 20-14-24 (the lightning gun's iron sights in the other weapons' colour). As always, each change is made
on the normal model and its alternate (the table in ROUND18 "Alternate weapon models").

**Why they were see-through.** Quake's alias renderer draws only the front of a triangle (clockwise seen from
outside; `glFrontFace(GL_CW)`, back faces culled). The models were made for a view model seen from behind and above:
wherever they were open (a tube's end, a box's bottom), or a triangle was wound the wrong way round, one sees
through them from the other side, which in VR is any side.

**The check.** `Misc/quakevr/check_mdl_holes.py [-v|-vv] [models]` (default: every `quakevr/progs/v_*.mdl`) welds
the vertices at the same place in every frame (skins split vertices along seams; generated parts share none) and
reports per model: open boundary loops (an edge only one triangle uses), and whether each is hidden (rays from its
middle hit the model in every direction: a grip's top inside the gun) or see-through; cracks (T-junctions: an edge
with the corners of the faces across it along it, which once rounded to the file's byte grid open a sliver, e.g.
0.03 wide along the super nailgun's back); flipped edges (two triangles running an edge the same way) and whether
they are in sight; inside-out closed pieces; stray open edges (no area); non-manifold edges. It exits 1 if any
model has a see-through loop, a crack wider than 0.01, a flipped edge in sight or an inside-out piece.

Before (this round's start) and after:

| Model | Before | After |
|---|---|---|
| v_nail / v_lava | hinge open below and in front; 2 slivers where the rear block meets the top rail; 3 flipped edges in the handle | ok (the flipped edges are now inside the new grip; the slivers closed) |
| v_nail2 / v_lava2 | 4 barrel mouths, the hub between them, the body's front and bottom open; 2 cracks along the back's upper corners (0.02..0.03 wide, round 16's grooves); the back groove cut through the hollow the hand reaches into | ok |
| v_rock / v_prox / v_multi | the whole front open (the tube's end, the ledge under it, the belly's front); the frame's front end | ok |
| v_rock2 / v_multi2 | the muzzle's end open; the grip's top and the nozzle's front ring (inside the tube) | ok |
| v_light / v_plasma | the body's underside open; the keel's back end wound the wrong way; 2 cracks at the back | ok |
| v_shot2 | the bead between the muzzles inside out | ok |
| v_grpple | the body's back underside open | ok |
| v_laserg | 5 cracks and 4 flipped edges where the folded blade meets the grip and neck | ok (the renders still show a hairline where the neck bends out of the grip: round 16's loft folds there) |
| v_axe | 2 cracks along the handle (0.13, 0.08 wide) | ok |
| v_shot, v_hammer, v_ksword, v_hksword, v_spike | ok | ok (unchanged) |

**The fixes.** `Misc/quakevr/seal_mdl.py` closes what the check finds, from the generators, with new triangles and
vertices only: old triangles and vertex indices stay, and the new triangles share no vertex with them, so the strip
order of vr_anchor.cpp (every anchor index, vr_shells.cpp's port) is unchanged (each generator checks its anchors
again after sealing). A cap runs along each open edge the other way round from its neighbour, so it faces out
whatever the shape (ear-clipped in the loop's plane); a crack gets slivers along its edge (no area before rounding;
after it, exactly the gap); a muzzle gets a bore (a rim, a wall into the barrel, a dark floor); a triangle wound the
wrong way, or an inside-out piece, gets a copy wound the other way. New vertices are computed per frame from the
loop's own corners, so caps and bores follow recoil and the super nailgun's spinning barrels. Per model:

- **Super nailgun** (`improve_weapons2.py` `seal_nail2`, both models): bores 3 units deep in the four barrel mouths
  (blue metal rim, dark wall, black floor), the hub's front capped, the body's missing front and bottom capped (one
  loop round the corner between them, split in two), the back's cracks filled. Two fixes to round 16's grooves: the
  floor at the chamfers is where the big faces' floors meet (sinking each thin chamfer 1 unit along its own normal
  turned bits of the floor over: they faced down and showed as holes from above), and the back groove, over the
  hollow the hand reaches into (its ceiling 0.77 under the top), is 0.55 deep instead of 1 (it cut through the
  ceiling). Anchors unchanged (hand 28, muzzle 123 / 148, two-handed 564 / 652).
- **Nailgun** (`seal_nail`): the hinge block's bottom and front; triangles closing the wedges where the rear
  block's sloping sides reach past the top rail.
- **Grenade launcher, proximity gun, multi-grenade launcher** (`improve_weapons3.py` `seal_launcher`): the tube's
  end is a bore (1 unit deep: the old face just behind it, at x 29.58 / 29.94, faces into the gun), the ledge under
  it and the belly's front capped.
- **Rocket launcher, multi-rocket launcher** (`improve_weapons.py` `seal_rocket_launcher`): the muzzle is a bore 6
  units down the tube (a thin rim, a sooty wall, the nozzle's black floor).
- **Lightning gun, plasma gun** (`seal_light`): the underside capped (brown), the keel's back end backed, cracks.
- **Double shotgun** (the bead backed), **grappling hook** (the back underside capped), **laser cannon** (cracks,
  flipped triangles backed; the neck's first ring is now the grip's last, bevel included, so they meet without a
  slit) and **axe** (`improve_weapons3.py` `build_axe`, from `src_models/v_axe.mdl`, now kept there byte for byte;
  its old vertices keep their bytes).

**The nailgun's grip** (`improve_weapons2.py` `nail_grip`, both models). The handle was a thin flat slab leaning
back 36 degrees behind the lower body, which the fist (its fingers curl round a line leaning back 16 degrees) did
not close round. Now the shotguns' pistol grip (`improve_weapons.py`'s `grip`: bevelled octagonal rings,
GRIP_PROFILE with one more ring up inside the rear block, the steel butt plate and bottom) goes through the fist,
ribbed in the pump's browns as on the shotgun (`ribbed_grip`), with the round-16 trigger guard and trigger as they
were. The slab folds into it: its back edge moves just inside the grip's back under the rear block (the block's
underside now slopes down onto the grip, a beavertail over the web of the hand), its butt inside the grip; its front
already was inside. The hand, the gun and every anchor stay where they were. The grip's butt reaches below the old
bounds, so the header's origin moves and the weapon offsets follow it: slots 3 and 11 (`_04`, `_12`) OffsetZ
0.799975 -> 1.51783 (X, Y unchanged). `settingsVersion` 12 resets those two slots once.

**The lightning gun's sights** (`light_sights`, both models). Its sights (the back faces of two rear posts over the
back of the body and the front post's tip over the muzzle) glowed in the electrodes' pale cyan (244..246). They now
use the shotguns' gradient in the same fire indices (a patch painted like `recolor_shotgun_sight.py`'s: light
orange at each sight's top, deep red at its edge), under new UVs for the sights' own vertices (no other triangle
uses them). `vr_sights.cpp` lists `progs/v_light.mdl` and `progs/v_plasma.mdl` now, so `vr_sight_hue` recolours
them as the shotguns'. No other texel of those skins may be in the fire indices: the lightning gun had none; the
plasma gun's coils (117 texels, 224..233) move to the nearest other fullbright colours (reds 247..249 and 240), which
look almost the same and keep their colour whatever the sight hue.

**Reproducible.** Running `improve_weapons.py`, `improve_weapons2.py` and `improve_weapons3.py` again gives the
same files; the printed settings are unchanged but for slots 3 and 11.

**Renders** (scratch `holeview.py`: back faces drawn magenta, so a magenta patch is a see-through spot in the
engine) from the side, the front, below and behind, before and after, and mock in-hand shots in vrfiringrange
(`impulse 156/157/158/160/161`, `impulse 43` for the alternate): side, upside down, muzzle towards the camera, the
usual hold.

**In the headset:**

- Super nailgun and lava super nailgun: look into the barrels from the front (dark bores, no see-through), at the
  body from the front and from below (closed), at the back from behind (no slivers along the upper corners). The
  back groove is shallower than the other two; the grooves' floors at the corners no longer flicker. Fire: the
  bores spin with the barrels.
- Nailgun and lava nailgun: the new grip in the fist (index finger at the trigger inside the guard, the butt under
  the pinky; is it too thick or too long?); the hinge from below; the rear block from the front.
- Grenade launcher, proximity gun (hipnotic), multi-grenade launcher (the alternate): the front, a dark bore; the
  belly's front closed.
- Rocket launcher and multi-rocket launcher: the muzzle's end, a dark bore.
- Lightning gun and plasma gun: the sights take the sight hue (Wrist Gadget > Colours) like the shotguns'; the
  electrodes stay cyan and the plasma coils red whatever the hue; the underside is closed.
- The engine needs a rebuild for the defaults and the sight list.

## Body: holsters on the legs, swimming kicks

Voice notes vrfiringrange_2026-09-26_20-16-14 (the leg holsters should follow the animated legs, by a factor) and
start_2026-09-26_20-28-19 (swimming or wading, the IK legs should kick where the stick moves the player).

### Hip holsters ride the thighs

With the full body (`vr_body_mode 3`) the hip holsters now go with the legs' animation: walking, stepping round on
the spot, tucking up in the air and kicking in the water. `vr_holster_leg_follow` (default 0.4; Body page, "Hip
Holsters Follow Legs") blends between where they were (0: fixed on the body, carried by the pelvis) and all the way
with the thighs (1).

- **What moves them.** `avatar::pose` keeps, per side, the thigh as posed and the thigh as it would be with the legs
  standing still under the body as it now is (the feet at home, no step, walk or water), both relative to the pelvis.
  `Follower::thigh` (vr_avatar.hpp) puts them on the pelvis as it is now and returns the difference: a turn about
  the hip joint (`ThighMotion`). So crouching, which already moves the holsters (round 15), is not counted twice:
  only the animation is.
- **Where on the thigh.** The holsters sit at the top of the thighs, by the hip joint, where a thigh barely moves.
  A holster moves as the point of the thigh it is strapped to: the point under it, but at least 0.2 m down the
  thigh (`THIGH_STRAP`, vr_body.cpp), and its plate turns with the thigh (slerped by the factor). At a brisk walk
  the thigh swings about 25-30 degrees from standing; at factor 1 the holster moves up to about 4.5 units (17 cm),
  so the default 0.4 gives about 2 units and 12 degrees of tilt.
- **What you see is what you grab.** The position goes through `body::holsterPositions`/`holsterPosition`, which
  both the drawing (`setupHolsters`, vr_view.cpp, unchanged) and the hands' hotspots (`body::updateHotspots`, sent
  to the server as `QVR_HS_*`) use; the reach markers (`vr_show_hip_holsters`) too. The server only receives the
  hotspot numbers, so there is nothing else to keep in step. The legs' animation is the one posed the frame
  before (the hotspots are worked out before the body is posed), carried by the current pelvis: the holster never
  lags the body, only the leg angle is a frame old, for the drawing and the grab alike.
- Off with torso-only bodies, `vr_body_anchors 0`, or no body posed (dead, intermission).

### Legs in water

All client side, in `vr_avatar.cpp` (`updateWater`): how high the water stands over the body's floor is found by
sampling the pelvis's column every 0.1 m (`Mod_PointInLeaf`), and the stick from `cl.cmd` (forward, side and up
moves, turned by the head's angles as the server steers swimming, `VR_MoveAngles`; full at `cl_forwardspeed`).
Nothing in vr_physics.cpp needed changing.

- **Wading** (on the bottom, water from 0.3 to 0.75 m and above, weight eased in): the walk gets heavier
  (`vr_body_wade`, default 1, "Wading Heaviness"): strides 20% shorter, the swinging foot lifted 0.1 m higher
  (knees up through the water), the cadence cap 30% lower. The walk's direction already follows the stick.
- **Swimming** (off the bottom in water above 0.85 m, or with the head under): the legs float. They trail behind
  where the stick moves you, up to 42 degrees from hanging down (half that going backwards, as the torso stays
  upright under the head; less when swimming up, more when diving), and flutter kick in turn across that, in the
  plane of the body's forward and the way you go, the knees bending through each kick. The kick is 6 degrees each
  way treading water, 22 at full stick (times `vr_body_swim_kick`, default 1, "Swimming Kicks"); 0.7 kicks a second
  treading water, plus `vr_body_swim_kick_rate` (default 1.5, "Swimming Kick Rate") at full stick. The toes point
  along the shins. With the stick left alone the legs tread water: slow, small kicks, knees bent.
- Everything eases in and out (the water state over about a quarter second, the stick's direction and strength
  faster), so walking, wading and swimming blend: the foot targets, the knees' direction and the feet's turn are
  mixed by the swimming weight.
- The hip holsters follow the kicks too (above).

### Checked

With the mock and the body preview (`vr_body_debug 3`, from the left; `2`, facing), `r_fullbright 1` to see it:

- **Swimming** in the firing range's pool (`setpos 600 450 -150`, then `noclip` to turn setpos's noclip off;
  `vr_swim_stick_speed 0` to stay in view): treading water, the knees bent in front; with the stick forward
  (looking 30 degrees down: diving) the legs trail back and kick in turn; stopping, back to treading.
- **Wading** in e1m2's shallows (1792 20, water 1.0 m deep): wade weight 1, the swinging knee lifted high.
- **Holsters** walking in e1m1 with follow 1, 0.4 and 0: the left hip holster moved up to 4.7 units and turned up to
  32 degrees with its thigh at 1, and not at all at 0.

### To check in the headset

- Whether 0.4 is the sweet spot: the holsters should look carried by the legs while walking, but still be easy to
  grab on the move (try 0.2-0.6). The strap depth (0.2 m) is a constant in vr_body.cpp.
- Swimming: whether the legs you glimpse behind and below you read as kicking with the stick; the kick's width and
  rate (the two sliders); whether the trail angle (42 degrees) is right with an upright body.
- Wading: whether the heavier walk reads in waist-deep water (the Wading Heaviness slider, 0 to 2).

## Weapon effects, pickups, holsters

Notes vrfiringrange_2026-09-26_20-06-19 (the muzzle flash shrinks when holding a gun with two hands), 20-07-26
(the lava nailguns should glow), 20-08-47 (morph between a gun's normal and lava models), 20-10-02 (the ammo screen
and the ammo button only on the gun in the hand) and 20-17-08 (a holstered gun sometimes invisible until taken and
put back).

### The muzzle flash and the two-handed recoil damping

**Cause.** A view model's flash is part of the model: triangles collapsed into one point in frame 0 that open out
in the firing frames (Quake's nailguns and launchers; the shotguns' flames of round 18, `improve_weapons.py`
`flame`/`show_flash`). Held two-handed, the gun's `TwoHZeroBlend` (0.8 on the shotguns, 0.75 on the launchers) blends
every vertex of the current frame towards frame 0 in the alias vertex shader, which steadies the recoil and also pulls
the flash's vertices back into their point: the flash was drawn at 20-25% of its size.

**Fix.** The flash's vertices are found when the model's vertex buffer is built (`VR_AliasFlameRefs`,
`vr_render.cpp`, called from `GLMesh_LoadVertexBuffer`): the vertices of triangles collapsed into one point in frame 0
that spread more than 2 units in some frame (tiny parts rounded into one point by the byte grid spread under 1). Each
gets the gun vertex nearest to its point in frame 0, stored in the pose data's two unused bytes (the position's 4th
byte and the normal's 4th, whose top bit marks it). The shader's zero blend (`ZeroBlend` in `gl_shaders.h`) moves
the other vertices towards frame 0 as before, and a flash vertex only by as much as its gun vertex is moved: the
flash keeps its full size and is carried by the steadied gun, at its muzzle. Found per model, nothing to maintain in
the generators: v_shot 55 vertices (plus 4 of a tiny part correctly left out), v_shot2 110, v_nail/v_lava 12,
v_nail2/v_lava2 48, v_rock/v_multi/v_prox 6, v_rock2/v_multi2 14; none on the lightning guns (their flash is not
geometry), the axe, the swords.

Checked with the mock (e1m1, the shotgun seen from the side, `vr_wofs_zb_02 0` then `0.8`, firing): with the
damping the flash is as long as without it and sits on the muzzle, while the gun's kick is damped.

### The lava nailguns glow (vr_lavagun_light)

The lava nailgun and super nailgun (`v_lava.mdl`, `v_lava2.mdl`, the nailguns with lava nails) carry a small
lava-coloured dynamic light (`emissive::lavaGunLight`, `vr_emissive.cpp`): along the barrels (55% of the way from the
hand anchor to the muzzle, 2 units over the gun, so it lights the hand, the arm and what is near rather than the
inside of the gun), unshadowed, flickering slowly as molten rock (a 1-3 Hz swell and a quicker shimmer). In the hands
at full strength; in the holsters and lying in the world (the nearest ones) at `vr_lavagun_light_idle`. Fading in
and out with the morph below.

| Cvar | Default | Menu (Graphics - Lights, Light Sources) |
|---|---|---|
| `vr_lavagun_light` | 1 | Lava Gun Light (0..3; 0 off) |
| `vr_lavagun_light_radius` | 72 | Lava Gun Light Reach |
| `vr_lavagun_light_flicker` | 0.35 | Lava Gun Light Flicker (0 steady) |
| `vr_lavagun_light_idle` | 0.4 | Lava Gun Light at Rest (holstered, lying) |

Presets: off on "Off (Quake)", the default from Low up.

### Morphing between a gun's two models (vr_weapon_morph_time)

The two models of a pair do not share their vertices (v_lava has 382 against v_nail's 370, split along more seams;
v_lava2 and v_multi2 are re-exports, v_lava2 with its windows cut in: 696 triangles against 540), so a vertex lerp
between them is not possible. But they share their model space and every part lands where it does on the other
(`improve_weapons_alt.py`, ROUND18), so the morph is a dissolve: when the hand's gun model changes to the other of its
pair (the button; `morphKind` in `vr_view.cpp`: nailgun/lava, super nailgun/lava, grenade/multi-grenade,
rocket/multi-rocket, lightning/plasma), for `vr_weapon_morph_time` seconds (0.4; 0 at once; menu Graphics - Models,
"Ammo Switch Morph") the old model is drawn too, where it would be held (`weaponMorph[hand]`). The alias fragment
shader (`Morph`) splits the two surfaces by a 3D value noise in the models' own units (`VR_AliasMorph` hands it the
model's scale and the progress in the instance's spare `Ambient[2..5].w`): the new model where the noise is under the
progress, the old one where it is over, and a thin glowing seam between (orange for the lava, yellow for the
multi-rockets, blue for the plasma), so the new paint spreads over the gun in molten veins. The button, the ammo
screen and the hand follow the new model; the old one is only drawn (the shadow pass draws both for that moment).

### Ammo screens and buttons on guns not in a hand (vr_weapon_screen_idle)

Quake VR's guns lying in the world are the view models themselves (thrown and dropped weapons, the firing range's
`func_weapon_grabbable`; the maps' `weapon_*` items are the old `g_*.mdl`, which have none of the anchors and are left
as they were). Now the holstered guns and the nearest 6 guns lying within 320 units carry their button and ammo
screen as a held one does (`idleAttachments` in `vr_view.cpp`), at the same anchors: the attachment's angles are
tuned against the hand, so the hand that would hold the gun is found from the gun's drawn angles by undoing its
angle offsets (`idleAttachmentAngles`). The screen shows what the client knows: the player's ammo for the gun (shells,
nails, rockets, cells; "--" for lava nails, multi-rockets and plasma, which are not in the stats), and for a
holstered gun with reloading on its clip over the clip size last seen with that gun in a hand. The buttons are only
drawn (pressing works on held guns). The CRT screen images went from 4 to 16 (`vr_text3d.cpp`), so the extra screens
look like the held ones. Menu: Screens page, "Screens on Weapons at Rest" (on).

### The invisible holstered gun

**Cause.** There are six holsters, but only four were drawn: the hips and the upper (chest) holsters. The shoulder
holsters (0 and 1, reached over the shoulder) had no drawn gun, and their reach (`vr_shoulder_holster_thresh` 7.8)
comes down over the top of the chest holsters' (6.5; in e1m1 with the default body the right shoulder's zone starts
about 0.05 m above the right chest holster's centre, measured with the mock's holster buzz: hotspot 9 up to hand
height 1.64 m, 4 from 1.65 m). `hotspot()` (`vr_body.cpp`) returned the first holster in the list whose zone the
hand was in, and the shoulders come first: a gun let go at the top of a chest holster went into the shoulder holster
over it and was drawn nowhere. Taking it from the same place (the shoulder's hotspot again) and putting it back a
little lower put it in the chest holster, where it showed.

**Fixes.** The hotspot is now the holster the hand is most within (the smallest distance over reach), so a hand
nearer the chest holster's centre gets the chest holster. And the shoulder holsters' guns are drawn (`holsterOnBack`
in `vr_view.cpp`): hanging down the back from 3 units behind the shoulder hotspot, their top away from the back,
tipped out 12 degrees, highlighted when hovered like the others, with their screen and button. Checked with the mock:
a gun let go at hand height 1.62 m over the chest holster goes into it (hotspot 9); the super shotgun let go over the
shoulder (1.8 m) is in the shoulder holster, drawn on the back (seen looking over the right shoulder).

### In the headset

- Two-handed shotgun, double shotgun, grenade and rocket launchers: the flash as big as one-handed, on the muzzle, the
  kick still damped.
- Lava nailguns: the glow on the hand and the arm (in a dark corner), its flicker; the Lights page sliders; the
  holstered and the lying ones dimmer.
- The ammo button on the nailguns, launchers and lightning gun: the morph's veins and seam over 0.4 s, nothing
  jumping; is 0.4 s right, is the seam too bright (its width and brightness are in the shader's `Morph`)?
- Guns on the firing range's tables and dropped ones: screen and button where they are on a held gun; the screen's
  text (ammo, "--" for the special ammo); taking one: the screen and button stay put on it.
- Holsters: put guns at the top edge of the chest holsters (they should go in, not vanish); reach over a shoulder and
  let a gun go: it hangs on your back (turn your head); is its place right with your body, does it poke into view?

Screenshots (scratchpad `wfx20/`): `t2c.png` (flash without and with the damping, the morph), `combo_all.png` (the
morph from behind, the lava light off and at 2, the shoulder holster), `t3_floor.png` (dropped guns with their
buttons and screens).

## Water: ripples and sounds

Voice notes e4m1_2026-09-26_20-30-59 (the ripples should move the wave geometry much more, with sliders) and
e4m1_2026-09-26_20-29-28 (the water sounds are too artificial: use free recorded assets instead).

### Ripples in the geometry

Round 18's ripples (`vr_water.cpp`, "Ripples"; `gl_shaders.h`, `LiquidRipples`) used one height for both the shape and
the lighting: 3 units for a hand's slap, `sqrt(strength / 10)` times that (0.35 to 2.6), at most 8, a 48-unit
wavelength. In the shape that is a 9 cm bump, which is why it was hard to see.

- **Height** (`vr_water_ripple_amplitude`, now 8, slider 0-24): the ripple's height in the geometry. The strength
  curve is now `(strength / 10)^0.8`, 0.25 to 3 times: a shot (4) 3.8 units, a hand (10) 8, a rocket (19) 13.4, a body
  (30-50) 19-24. One ripple is at most 24 units (`kMaxRipple`; `kMaxSwell`, which widens the mesh's bounds, is 64).
- **Normal strength** (`vr_water_ripple_normal`, new, 0.6, slider 0-2): the ripples' slopes in the lighting (fresnel,
  glints, refraction), times the geometry's slope. 1 lights the ripple exactly as it is shaped; 0.6 keeps the glints
  calmer now that the shape is 2.7 times higher. It goes to the shaders in `Water3.y` (unused before).
- **Wavelength** (`vr_water_ripple_wavelength`, now 64, slider 16-192). The grid is 16 units (`vr_water_geo_cell`),
  so 64 is 4 cells a wavelength: the crests are drawn from 4 vertices, not the 3 of before, which at the new heights
  looked like triangles. As before, a ripple is in the geometry from 2 cells a wavelength and fully from 3; below 2 it
  is in the lighting only.
- **Ripples at once** (`vr_water_ripple_max`, new, 32, slider 1-32): how many are kept; a new one takes the weakest's
  place. Fewer is cheaper when many splash at once.
- Speed and duration are unchanged (32 units/s, 2 s).

Menu: Graphics, Liquids, "Splashes and Ripples": Ripple Height (Geometry), Ripple Normal Strength, Ripple Speed,
Ripple Duration, Ripple Wavelength, Ripples at Once.

**Kept off the eye.** A body's splash round you is up to 24 units high, and with your head just over the water its
crest would rise through your eyes. `LiquidDisplace` now holds a ripple's crest (or, seen from under water, its
trough) 6 units short of the eye's height where the eye is, relaxing one unit per unit past 24 units out
(`gridRise`, the CPU copy that the rings and drops ride, does the same). The swells are unchanged.

**A bug that dropped ripples.** A ripple's time is the client's `cl.time`, which Quake pulls back to the server's time
when a message arrives. A ripple made just before that had a negative age for a frame, and `fillRipples` took a
negative age as "a map's before this one" and erased it. In the mock, several of my test splashes had no ripple at all.
Now only an age below -1 s erases a ripple; a slightly negative one waits.

**At the walls.** The ripples are still multiplied by the rim pin (0 at the rim, 1 at 32 units in), so the surface
still meets the walls exactly. A 24-unit body splash against the north wall of e1m2's big pool showed its crests
flattening into the rim, with no gaps (`w20/wall_grid.png`).

**Screenshots** (mock, e1m2's big pool, eye 15 units over the water, the author's swells; scratchpad `w20/`):

- `hand_before_after.png`: a hand's slap (strength 10) at 0.15, 0.5, 1.0 and 1.7 s, before (3 units, 48, normal 1) on
  the left, now (8, 64, 0.6) on the right. Before, the ring rides an almost flat surface. Now the splash point sinks
  into a crater, and a crest runs out under the ring.
- `body_before_after.png`: a body (strength 35). Left: old height, wavelength and normal (with the new strength curve,
  8.2 units). Right: now, 21.8 units.
- `eye_splash.png`: a body's splash right under the eye, with the eye 2 units over the water: the crests stop short of
  the eye.

### Water sounds

The synthesized splash, plip, slosh and stroke sounds (round 15, `make_sounds.py`) are replaced by recordings, four
variants of each (one for a hand pulled out), all **CC0** from Freesound. `docs/vr-port/CREDITS.md` lists every file
with its source, author and cut. I looked first at OpenGameArt's CC0 water packs (rubberduck's "40 CC0 water / splash /
slime SFX", ezwa's "6 short water splashes"). The first are designed, dense crackles with a lot of energy at 4-8 kHz,
and the second are a syringe emptied into water, so I didn't use them. I did not use Pixabay, Zapsplat or Mixkit: their
licences don't allow redistributing the files. BBC Sound Effects is not OK (its licence is restrictive).

| Event | Files | Recording |
|---|---|---|
| a body, a rocket, a heavy thing going in (strength >= 18) | `splash_big1..4` (1.8 s) | a person jumping into water, three takes (Nox_Sound); a close dive into a pool (felix.blume) |
| a hand or gun slapping, a thrown thing | `splash_small1..4` (0.8-1.05 s) | hand slaps (N-RAZM), a clean harbour-jump splash (blaukreuz/qubodup), a hand sweeping through water (morganveilleux) |
| a hand pulled out fast | `splash_out1` (1.1 s) | "coming out of water - woosh" (morganveilleux) |
| a shot, a nail, a grenade | `plip1..4` (0.33-0.36 s) | stones into a bucket: a tick and a rising bubble (vibe_crc); a stone, an object dropped in (14FPanska_Nemec_Petr, danhelbling) |
| wading | `slosh1..4` (0.6-0.7 s) | steps wading in shallow water (ryansitz) |
| a swimming stroke | `stroke1..4` (0.8-0.9 s) | a hand pushing through water, a swell of whoosh and splash (morganveilleux) |

**Processing** (a scratchpad script, `wsnd/cut.py`, using soundfile and scipy). Each file is a cut of the source's
Freesound high-quality preview (Ogg, 44.1-96 kHz). The cut is high-passed at 40 Hz, mixed to mono, resampled to
22050 Hz with a polyphase filter, and written as 16-bit PCM, the same format as the port's other sounds. Ironwail
loads 8- and 16-bit mono PCM at any rate and resamples it to the mixer's rate. The cut then starts 5 ms before the
sound, is faded in and out, and has stray clicks in its tail (a drip, a knock over 4 times the local level) held down.
Last, it is brought to a loudness target with a 2 ms look-ahead peak limiter: at most 6 dB off the peaks, peaks at
-1 dBFS. The loudness is the loudest 200 ms, band-passed at 80 Hz-5 kHz, as Quake's mixer plays it: with the default
`sndspeed 11025` it low-passes the whole mix at about 5 kHz.

The targets, next to Quake's own and the old synthesized sounds: `splash_big` -10.5 dB (Quake's `h2ohit1` -11.6,
`inh2o` -16.8; old -8.6), `splash_small` -12 (old -9.3), `plip` -16 (old -16.4), `slosh` -15 (old -14.4), `stroke`
-14.5 (old -14.6), `splash_out` -13.7. Within a set the variants are within 2 dB, except `splash_big1` and
`splash_small1`, 1.5 to 1.8 dB quieter: their sharp first crack is what the limiter's 6 dB stops. The sounds are
played at the same volumes as before (`vr_physics.cpp`). All 21 files together are 1.0 MB.

**In the engine.** `vr_physics.cpp`'s `variant("plip")` picks `vr/plip1..4.wav` at random, never the one played last
(this replaces the slosh and stroke alternation). QC `world.qc` precaches the 21 files. `make_sounds.py` no longer
makes the water sounds, and still makes the headshot, shell and melee sounds (its output for those is byte for byte
the same). The old `splash_small.wav`, `splash_big.wav` and `plip.wav` are deleted. Quake's own sounds for you going
in (`inh2o`, ...) and for things coming out (`h2ohit1`) are unchanged.

**Tested** in the mock with sound on. All 21 files load as 16-bit (`soundlist`). Shots into e1m2's shallows
played `plip4`, `plip1` and `plip3` (`developer 2`), so the random pick works. I can't listen: I chose the
recordings by their descriptions, waveforms and spectrograms (onset, decay, bubble chirps, no background noise, no
clipping).

### What to check in the headset

1. **The sounds.** Slap the water, pull a hand out fast, wade in the e1m2 shallows, swim brisk strokes, shoot the
   water, throw a grenade, fire a rocket into a pool, jump into the moat.
   - Do they sound real, and are they loud enough next to Quake's own? `vr_water_sounds` scales them all.
   - Say which variant sounds wrong: `developer 2` prints each one's name.
   - Everything is band-limited at about 5 kHz by Quake's default `sndspeed 11025` (`snd_mix.c` low-passes the
     whole mix). `sndspeed 44100` in the console (not saved; `-sndspeed 44100` on the command line) turns that off
     at once and keeps the recordings' sparkle, but it changes every sound.
2. **The ripples**, crouched at a pool's edge: is the crater and crest at the new defaults too much or too little?
   - Height: Ripple Height (Geometry), 8.
   - Lighting: Ripple Normal Strength, 0.6. Is 1 (lit as shaped) nicer, or too glinty?
   - Wavelength: 64. Longer reads better in the shape.
3. Jump into water: a body's ripple is 20 units or more. Do the crests round you look right, and does the clamp near
   your eyes (crests stop 6 units below them) show as a flat spot?
4. Splash against a wall and look for gaps where the surface meets it.
5. Shots into water: is a 3.8-unit ripple a shot too big?

## Lava light, barrel explosions

Notes start_2026-09-26_20-27-17 (the lava looks intense but lights nothing), e4m1_2026-09-26_20-30-32 (exploding
barrels have no explosion light) and e1m1_2026-09-26_21-37-09 (a closed hand grabs physics objects it moves onto).

### Lava lights the walls (baked, kind=liquid)

The relight skipped every texture named `*...` (liquids): lava was drawn fullbright but gave no light. Now
`relight_textures.cfg` has a fourth kind, `liquid`, and two rules:

    [*]lava*    kind=liquid light=175 reach=1.4 color=255,104,44
    [*]slime*   kind=liquid light=70 reach=1.5 color=90,200,40

(`[*]` is a literal `*`.) `relight_maps.liquid_spots` puts a point light every 96 units (`LIQUID_STEP`) over each
liquid face that faces the open air, 16 units over the surface (8 or 2 under a low ceiling; a liquid's faces seen
from inside it face lava, not air: none there), none closer than 48 to another (the BSP cuts a pool into many
faces). Each is `light` x `(5 / its liquid's lights within 192 units) ^ 0.4` when more than 5 (`LIQUID_LONE`,
`LIQUID_ROOM`, `LIQUID_CROWD`): a lake's lights add up far more than a channel's, so they get less each (e3m6's
lake down to 104, start's pit 123 to 175, a lone channel 175). Linear falloff reaching `light x reach` units
(`"wait"` 0.71), the rule's colour (not whitened like the glows), `"_dirt" "-1"` (the pool's edge against a wall
is a corner that `-dirt` would darken). They are only for `light` (the map keeps its entities), so it is baked: no cost in game; the light grid
gets them too, so models near lava are lit red. Water and teleporters give none (QRP's `#teleport` is dark,
mean 9 9 9).

Why point lights and not ericw's `"_surface" "*lava1"`: the surface lights go on both sides of a liquid's faces
(the undersides light the pit under the lava, which nobody sees), and their count and spacing are ericw's
(`-surflight_subdivide`); ours are one pass over the faces we already read, with the crowding control.

Tuning (screenshots in the mock, noclip views over the lava of start, e1m7, e3m3, e3m6 and the note's own view):
the sum of overlapping linear lights saturates suddenly. 100/reach 2 lit start's pit faintly and e3m6's far walls
not at all; 160/2.5 and 200/1.4 (uniform) turned e1m7's and e3m3's lake walls flat orange (the red channel
clipped, texture lost); the crowding factor and a shorter reach keep the start pit's walls glowing strongly near
the lava and fading up, while lake walls stay textured. Colour 255,104,44 (a little orange: 255,80,20 read as a
dim, dark red on the brown stone). Slime: QRP's slime is dark and olive, so its light is faint on purpose (70,
reaching 105 units: a green tint on the walls right by e1m1's slime); e2m7's slime is in a black hall and shows
none.

What changes: 46 maps have lava or slime (id1 23, hipnotic 9, rogue 14), from 3 lights (dm1) to 368 (e3m1's slime);
relighting everything takes about 50 s as before. Relit into both `quakevr/relit` folders (this worktree's and
`C:/OHWorkspace/quakevr-iw/quakevr/relit`), with `--vis-dir`: all 73 maps water-vised, stamps identical. The VR
maps have no lava (vrstart, vrtutorial, vrfiringrange): `relight_quakevr_maps.py` not rerun. Per map or per
liquid: `relight_textures.cfg` (e.g. `e1m7/[*]lava1 scale=0.8`, `[*]slime* kind=off`); `relight_maps.py --quake
<Quake> --list-glows --only start` lists the liquid lights.

No dynamic flicker: a few pulsing unshadowed lights over big lava areas would cost a light each per frame for
every lava room in view, for a slow shimmer the lava's own animation already gives; not done.

Screenshots (scratchpad): `r20_start_note_before_after.png` (the note's view), `r20_lava_before_after.png`
(start, e1m7, e3m3, e3m6: before left, after right), `r20_slime_before.png` / `r20_slime_after.png`.

### Exploding barrels: the rocket's explosion

`barrel_explode` (misc.qc, `misc_explobox` and `misc_explobox2`) kept id's code: the sound on the box and particles
(`particle2(..., QVR_PARTICLE_PRESET_EXPLOSION)`), then the sprite. No `TE_EXPLOSION`, so none of what the client
does for one in `CL_ParseTEnt`: no dynamic light (`QVR_DLIGHT_EXPLOSION`, `vr_explosion_light_scale`, its shadows),
no heat haze, no scorch mark. Also, a box's origin is its corner (the brush model's bounds are 1..31, 1..63), so
the particles came from a corner and the sprite from a corner 32 up. Now it sends `TE_EXPLOSION` from the box's
middle (`(absmin + absmax) / 2`), as a rocket does, and drops its own sound and particles (the temp entity plays
`r_exp3` at the same volume and attenuation, and throws the explosion particles); the sprite is at the middle too.
No engine change.

Tested in e4m1 (barrels at 880 1056 112 and 832 1080 112, stood at 1040 1072 with the super shotgun): before, a
fireball and no light on the floor or walls; after, the floor and wall lit orange, both boxes (the second blown up
by the first) each with its light. `r20_barrel_r20old.png` / `r20_barrel_r20new.png` (4 frames: before the shot,
then about 0.03, 0.08 and 0.2 s after).

### Carrying starts only on the grip's press

`VR_Carry_Handtouch` (vr_carry.qc: ammo and health boxes, backpacks, gibs and heads) started a carry whenever the
touching hand's grip was held (`VR_HandGrabUtil_IsHandGrabbing`): a fist, or a hand already gripping, moved onto a
gib grabbed it, and a punch through one became a grab. Now `VR_Carry_GripPressed`: the grip is held and was
pressed this frame (the previous frame's bit is off) or at most 0.2 s ago (`VR_CARRY_GRIP_WINDOW`: the press may
come a frame or two before the hand's touch, and `mainhand/offhand_lastgrabtime` is recorded in PlayerPostThink,
after the touches). A grip already held nudges or strikes (`VR_Carry_Nudge` -> `VR_Gib_Struck`) instead. Weapon
pickups (`weapon_touch`) and force grab catches are unchanged; there is no engine-side grab (the engine only calls
`handtouch`). `VR_Carry_Start` prints `carry: taken` (developer 1), like its other `carry:` lines.

Tested in the mock (e1m1, `impulse 245` drops a gib under the off hand, `+graboff`/`-graboff`; the grip button
alone does not set the grab bit in the mock), old and new progs:

| step | before | after |
|---|---|---|
| grip held above, hand lowered slowly onto the gib (30 touches) | taken | not taken |
| then released and pressed again over it | taken | taken |
| fist swept through a gib at 2-3 m/s | taken | struck: "hit by player for 60", burst |

To check in the headset: grabbing a box or gib you reach for with the grip pressed just before touching it (the
0.2 s window) still works.

## Physics: spinning items, held fit

Notes e4m1_2026-09-26_20-29-59 (a backpack in the water at an edge "spinning and spazzing out") and
vrfiringrange_2026-09-26_20-00-16 (a held health box leaves too much space to the hand).

### Items stuck in corners

**Where the note was.** At 1405 751 in e4m1 the floor is a slab (z 64 to 80) over a large body of water (surface
72): the pool is a hole in the slab, and its water goes on under the slab all round. A backpack that drifts to the
pool's edge goes under the slab (buoyancy presses it up against the slab's underside), half under it, or against
the ledge below the slab's east side (z 32 to 48), where the gap between ledge and slab (16 units) is smaller than
a backpack (19 units tall).

**Reproduced** (mock, `host_maxfps 90`, every body's state printed each frame): backpacks dropped along the pool's
edges, dropped under the slab, placed half inside the slab's edge, placed in the ledge's gap, and 36 backpacks
dropped on the firing range's floor, at its walls and into its water. Three failures:

1. **Pushed out as velocity.** A corner found inside a surface was pushed out by a velocity (0.3 of its depth per
   step, up to 4 units: up to 108 u/s at 90 fps), and the body *kept* it. A backpack left half inside the slab's
   edge shot across the pool at 54 u/s. A body in a corner was pushed off one wall, spinning (the push is at a
   corner, off its centre), struck the other and was pushed back: at a wall of the firing range a backpack dropped
   among others rocked forever at up to 16 u/s and 2.2 rad/s, never falling asleep.
2. **Wedged.** A body taller than the gap it is in (the ledge under the slab) has corners inside the ledge pushed up
   and corners inside the slab pushed down, forever: it jittered by up to 10 u/s and 1.1 rad/s, its centre went
   into the slab, and `keepInWorld` put it back (227 times in 15 s for one backpack), and it went round again.
3. The put-back kept the body's spin (`vr_spin`), so a body put back went on turning.

**Fixes** (`vr_rigid.cpp`):

- **Split impulses.** A corner inside a surface is moved out by a velocity of its own, solved like the contacts
  (pushes only, all corners together) but used for that substep's move alone, never kept (a quarter of its depth
  per substep). The contacts' own velocity targets are only "no closer" and the bounce. Nothing is gained from
  getting out of a wall.
- **Unwedging.** A substep with two corners pushed out along opposite normals (their dot below -0.5) is wedged. A
  body wedged for 0.1 s is moved the least way out: along the world's axes or its own, one unit at a time up to its
  size, to the first place where its box (a unit smaller, the contacts push out of shallow overlaps) is out of
  solid at its centre, corners, edges' and faces' middles, along a clear line (never through a wall). If there is
  none it is held still (no dithering) and looked at again twice a second, until it is moved or pushed.
- `keepInWorld` puts a body back still, its spin cleared too.

**Before / after** (same scripts, the old path kept behind a temporary switch for the comparison):

| Case | Before | After |
|---|---|---|
| Backpack left half inside the slab's edge (e4m1 1392 750 58) | 54 u/s across the pool | eases out at 2-5 u/s, floats at the edge |
| 8 backpacks placed in the ledge's gap, tilted (e4m1 x 1545) | 227 put-backs, 2 jittering (10 u/s, 1.1 rad/s) for 15 s | 6 moved out, 1 held still; no put-backs, no movement after |
| 36 backpacks dropped on the firing range's floor, at a wall and into its water | one at the wall rocking forever (16 u/s, 2.2 rad/s) | all on land asleep; floating ones bob as before |
| 33 backpacks dropped round the e4m1 pool (the note's place) | settle (floating, or pressed under the slab) | the same |

The floating backpacks come to rest bobbing as in round 19. Bodies spawned inside a wall (my test's drop point) are
still put back every frame, as before; in play a backpack starts where a monster was.

**New test command:** `vr_rigid_place <number | classname | new> <x> <y> <z> [<pitch> <yaw> <roll> [<vx> <vy> <vz>
[<sx> <sy> <sz>]]]` puts a rigid body there, turned, moving and spinning so; `vr_rigid_place <..> main|off
[<forward> <left> <up> [<pitch> <yaw> <roll>]]` puts it at a hand. `vr_debug_throw 4` prints every contact of every
substep (the centre, each corner's normal, its velocity target and push). The wedge prints with `vr_debug_throw 3`.

### Held objects: fitted to the drawn hand and the drawn object

**Why there was space.** Round 18 pushed the object's *box* out of a 4 cm *ball round the grip* (the controller's
pose). Measured from the drawn hand's triangles in the grip's frame, the curled fist is not round the grip: it is
below and behind it (forward -19 to 0 cm, up -13 to -1 cm), its knuckles 4.2 cm towards the palm side. So:

- the object stopped where its box, grown by 4 cm in its own axes, cleared the grip: a box turned against the palm
  stopped up to 4 x (sqrt 2 - 1) further out at its edges, and a box gripped a little way from the hand (a hand
  takes a box it is near, not only one it is in) was not moved at all: a gap of a few cm;
- a box mostly below or behind the grip (where the fingers are) was not seen at all: the fingers sank into it;
- the brush boxes' boxes are a unit wider than drawn (Quake pads a model's bounds), an alias model's bounds cover all
  its frames.

**Now** (`held::surfaceFit`, `vr_held.cpp`): the fist is a height field measured from the drawn hand (`hand_base.mdl`
and the five fingers curled, as `vr_view.cpp` places them: 1 cm cells over the hand's forward and up, the furthest
the fist reaches towards the palm's side in each). The object is its drawn surface: the alias model's current frame
(with its networked scale and offset, and the weapon scaling), or the brush model's faces (the ammo and health
boxes), turned with it. Along each cell's line the object's surface nearest the back of the hand is found, and the
object is moved along the palm's normal until the tightest cell just touches: out of the fist, or *in* to it (up to
8 cm) when it was gripped short of it. The hand's forward and up come from the gripping player's hand angles; the
hand scales with the weapon scale (`weapons::offsetScale()`).

In the mock (a shells box gripped at the hand in six places and turns; the same with the health and nail boxes):

| Grip | Round 18 | Now |
|---|---|---|
| centred | 15.5 cm out | 15.2 |
| a little to the palm side, turned 60 degrees | 7.0 | 10.9 (the fingers were 3.9 cm in it) |
| to the palm side and down (the fingers' height) | 0 (fingers 6.5 cm in it) | 6.5 |
| beside the fist, 3.6 cm short of it | 0 (a 3.6 cm gap) | -3.6 (drawn in) |
| behind and below, turned | 6.2 | 13.2 |

Screenshots (from below the hand and from its side, the round 18 fit left, now right): the box at the fingers'
height was held with a visible gap, now against the fingers; the turned box's edge now meets the knuckles.

**New settings** (Carrying page: **Fit to the Hand** is `vr_held_surface_fit`, now in the menu; **Fit Gap**):

- `vr_held_fit_gap` (cm, default 0; -2 to 3 in the menu): space between the fingers and what they hold, negative
  sinks it in.
- `vr_held_fit_gaps` (console, default empty): per model, added to it: `"name=cm ..."`, a name matching the end of
  the model's, e.g. `vr_held_fit_gaps "b_bh25.bsp=-0.3 backpack.mdl=0.5 gib1.mdl=1"`.
- `vr_debug_throw 1` prints how far each grip moved the object.

Weapons are not affected (they have their own grips).

**Please check in the headset:**

1. Items in the e4m1 pool (and anywhere else you saw it): drop or throw backpacks and boxes into corners, under the
   slab's edge and into the gap below the walkway's east side. Nothing should spin or shake; a wedged one should hop
   out a few centimetres or stay still.
2. Letting go of a box partly inside a wall: it should ease out, not shoot off.
3. Held boxes, health boxes, backpacks and gibs, gripped from different sides and turns: do they sit against your
   fingers? If there is still space (or they sink in), set Fit Gap (for everything) or `vr_held_fit_gaps` (per model)
   and tell me the numbers you like.
4. The fist is measured with all fingers curled. With only the grip held (index straight) a box may sit a few
   millimetres short at the index finger's height.

## Upscaling and foveated rendering

Two ways to spend less GPU time per eye: render fewer pixels and upscale them better than bilinear (FSR 1 or NIS), or
shade fewer pixels where the lenses blur anyway (variable-rate shading). Files: `vr_upscale.cpp/.hpp`,
`vr_foveated.cpp/.hpp`, `vr_stereo.cpp` (the eye's passes), `external/fsr1`, `external/nis`; the menu is VR Settings >
Headset.

### The eye's passes, in order

1. The scene (`V_RenderView`): the clear, the lenses' hidden area, then, with `vr_foveated`, variable-rate shading on
   for the rest of the scene (world and its depth pre-pass, models, liquids, particles, decals, sky, hands and
   weapons), off at the end of the view.
2. Bloom (quarter size), then the post-process (`GL_PostProcess`): the glow added, the tone curve, the grade, the
   headset's gamma, the dither. At a render scale other than 1 it writes into a texture of the rendered size.
3. **The upscale** (new GPU scope `upscale`), from that texture into the eye's swapchain image: bilinear, FSR or NIS
   (below). FSR and NIS want what the display gets (tone-mapped, gamma-encoded, anti-aliased), and that is what the
   post-process leaves; the dither is at pixel scale, which FSR's input rules allow.
4. `vr_foveated_debug`'s overlay, then `vr_eyeshot 1` (it now takes the final image, after the upscale; `vr_eyeshot 2`
   still takes the rendered one with its float scene).
5. The UI, over the final image at the headset's full resolution, as before (round 14): lasers, the HUD panel or menu
   and pointer, the wrist log. So they stay sharp at any render scale and are neither upscaled nor sharpened.

### Upscaling (`vr_upscale`, VR Settings > Headset > Upscaling)

- **0 Bilinear:** the linear blit it always was.
- **1 FSR** (default): AMD FidelityFX Super Resolution 1. EASU (edge-adaptive, a 12-tap lanczos-like kernel shaped by
  the local gradient, clamped to the nearest 2x2 texels so there's no ringing) into a texture of the image's size, then
  RCAS (contrast-adaptive sharpening, limited to what would not clip) into the image.
- **2 NIS**: NVIDIA Image Scaling's NVScaler, a 6-tap scaler with 4 directional filters and its own sharpening, as a
  compute shader (32x24 pixel blocks) into the same texture, then copied into the image. It scales by 1 to 2 per side:
  below render scale 0.5 FSR is used instead.
- **Sharpness** (`vr_upscale_sharpness`, 0.5): 0 none; for FSR, RCAS at `2 * (1 - s)` stops below its strongest (0.5:
  1 stop); for NIS, its own slider at half the value (its 0.5 draws dark halos next to text, see below).
- **Radius** (`vr_upscale_radius`, 40 degrees; console): as OpenXR Toolkit does, the upscaler only runs within this
  angle of the lens centre, the eye's projection centre from its asymmetric field of view (so the two eyes' circles
  are mirrored), and the image is bilinear beyond, fading over the circle's last 8%. EASU is scissored to the circle's
  box, NIS dispatched over its blocks. 0: the whole image.
- **At render scale 1** nothing runs (the post-process writes straight into the image). `vr_upscale_sharpen_native 1`
  adds RCAS alone there (the post-process then goes through the texture). Above 1 it's the bilinear downsample.

Both vendors' headers are included unchanged (`external/fsr1/ffx_fsr1.h`, `external/nis/NIS_Scaler.h`,
`NIS_Config.h`, MIT; `docs/vr-port/CREDITS.md`) and compiled as they are: `external/embed_glsl.py` turns a header into
string pieces (`*.glsl.inc`, under MSVC's 16 KB limit per literal) that `vr_upscale.cpp` hands to the GL compiler after
its own definitions: for FSR a stand-in for the part of `ffx_a.h` it uses (types, bit casts, the approximate
reciprocals, as `ffx_a.h` defines them for `A_GLSL`), for NIS the textures and constants it names (its GLSL path is
Vulkan's: `sampler2D(texture, sampler)` is defined away). The constants are made on the CPU (`FsrEasuCon`'s formulas,
`NVScalerUpdateConfig` and the coefficient tables from `NIS_Config.h`). The mock's eye images can now be made bigger
for measuring (`vr_mock_eye_size`, up to 2048).

**Cost** (GPU ms per frame, **both eyes**, mock images 2048 x 2048, rendered at 0.7 = 1434 x 1434; e1m1's start
corridor, the frame paused, 3 intervals of 600 frames each alternating, medians; the spread was within 0.03):

| resample | 2048² (measured) | 3292 x 3524 (x 2.77 pixels) |
|---|---|---|
| bilinear blit | 0.056 | ~0.16 |
| FSR, within 40 degrees (default) | 0.107 | ~0.30 |
| FSR, whole image | 0.153 | ~0.42 |
| NIS, within 40 degrees | 0.200 | ~0.55 |
| NIS, whole image | 0.266 | ~0.74 |
| RCAS alone at scale 1 | 0.044 | ~0.12 |

The extrapolation scales by output pixels; with the Quest 3's wider field the 40 degree circle is a smaller share of
the image than in the mock (±45.8 degrees), so the radius-limited rows should be lower. Against it: the same scene at
0.7 instead of 1 saved 0.45 ms of scene time at 2048² (1.04 to 0.59; ~1.2 ms at the headset's size), so FSR's 0.107
(0.05 more than bilinear) is a quarter of what the lower scale saves.

**Image comparisons** (`vr_eyeshot`, QRP textures (`qbase3`), the left eye, 2048²: native 1.0 against 0.7 bilinear,
FSR and NIS; crops enlarged 2x): the tutorial's close text board, the "Finding a weapon" board across the room with its
railing, the grated floor, e1m1's corridor into the distance. "Detail" is the mean luminance gradient against native
(1 = as sharp as native):

| scene | bilinear | FSR (0.5) | NIS (at its 0.5) | NIS (at its 0.25 = our 0.5 now) |
|---|---|---|---|---|
| close text board | 0.85 | 0.90 | 0.99 | |
| distant text board, railing | 0.73 | 0.89 | 1.13 | 0.97 |
| e1m1 corridor | 0.69 | 0.79 | 0.88 | |

FSR is the closest to native: the letters of the far board keep their pixel edges where bilinear softens them, with
no halos; PSNR against native is the same as bilinear's (30.8 against 30.7 dB: the gain is in edges, not in the
average error). NIS at its own 0.5 was sharper than native with dark rims around the letters, hence the halved slider.
FSR's sharpness at 0.75 matches native's detail (0.99), at 1 it oversharpens (1.34). The bilinear ring outside 40
degrees shows no seam in a difference image (the fade).

### Foveated rendering (`vr_foveated`, VR Settings > Headset > Foveated Rendering)

NVIDIA's `GL_NV_shading_rate_image` (found at run time in the extension list; the menu item does nothing without it,
and `vr_foveated_debug` logs whether it was found and the driver's tile size, 16x16 on the 4090). Each eye has its own
R8UI shading-rate image, one texel per tile, whose value picks from a palette: full rate, once per 2x2 pixels, once per
4x4. A tile's rate comes from the angle between the view axis and the tile's point nearest the lens centre (so no
pixel is coarser than its angle asks): full rate within the inner angle, 2x2 to the outer one, 4x4 beyond.
Conservative is 45/60 degrees, balanced 35/50, aggressive 25/40; `vr_foveated_inner` and `vr_foveated_outer` set your
own. On a planar eye image the pixels per degree grow away from the centre (1.5 times at 35 degrees, 2 at 45, radially
even more), so a 2x2 tile at 45 degrees still has about the centre's angular resolution radially.

**Only the scene.** The rates are on from right after the scene's clear and hidden area (`VR_DrawHiddenArea`) to the
end of the eye's `V_RenderView`, and only while one of the eye's scene framebuffers is bound: `GL_BindFramebufferFunc`
is watched during the scene, so a pass into another target (the water's half-size scene distances, a screen's canvas)
shades every pixel, then the rates come back with the scene's framebuffer. Verified in the log of a frame: scene and OIT
framebuffers on, the distances' framebuffer off. Bloom, the post-process, the upscale, the UI and the mirror are after
the end. Notes:

- **Depth pre-pass** (round 19): depth only, no fragment shader, so the rates change nothing there; the shading pass
  after it is where the time goes.
- **MSAA:** it works with `vid_fsaa` (checked at 4): a coarse fragment covers all samples of its pixels; the driver
  caps a coarse fragment's samples (`GL_MAX_COARSE_FRAGMENT_SAMPLES_NV`), so 4x4 with 4x MSAA may be shaded finer.
- **Derivatives** are taken between coarse fragments: textures sample a coarser mip there, normal maps and parallax are
  fine (no seams in the shots); specular AA sees the wider footprint, so highlights there should be a little softer.
- **Both eyes**: separate images (the projection centres are mirrored); remade only when the size, field of view or
  angles change.
- `r_scale` 2 to 4 (Ironwail's pixel look) turns it off.

**Savings** (GPU ms per frame, both eyes, 2048² each at render scale 1, the same paused e1m1 corridor with three
shadowed `vr_light_test` lights and the author's settings, no hidden area):

| `vr_foveated` | world+brush | scene | eyes | mock shares 1x1 / 2x2 / 4x4 |
|---|---|---|---|---|
| 0 off | 0.834 | 1.039 | 1.355 | |
| 1 conservative | 0.682 (-18%) | 0.892 | 1.196 | |
| 2 balanced | 0.460 (-45%) | 0.663 | 0.973 | 37% / 56% / 7% (52% of the shading) |
| 3 aggressive | 0.304 (-64%) | 0.492 | 0.806 | |

The mock's field is ±45.8 degrees, narrower than the Quest 3's, so more of the headset's image is beyond each angle and
the savings there should be larger (conservative especially: in the mock its 45 degrees covers nearly everything). With
`vr_foveated_debug 1` the console prints each eye's shares for the headset's own field of view.

**Images:** in e1m1's shots, balanced leaves the pixels within 35 degrees bit-identical (PSNR 99), 49 dB between 35 and
50 degrees, 47.6 beyond; aggressive 44 and 47. In 2x crops of the periphery 2x2 is hard to see (a slightly softer
texture); 4x4 shows blocky texture detail at the image's edge. `vr_foveated_debug 1` tints 2x2 tiles yellow and 4x4
red, and draws the upscaler's circle in cyan, in the eyes and the mirror (the mirror shows the whole image, which the
lenses don't).

### Check in the headset

1. Render Scale 0.7, Upscaling FSR, then Bilinear, then NIS: text boards (the tutorial's, the firing range's) and
   grates should look clearly sharper with FSR than bilinear, and close to Render Scale 1. Look for shimmer on edges
   while moving the head; lower Sharpness if there is any. The menu, HUD and wrist log should be as sharp at 0.7 as at
   1.
2. With FSR at 0.7, look slowly from the centre to the edge of the lens: the change to bilinear at 40 degrees should
   not be visible. If it is, try `vr_upscale_radius 0` (everywhere) and tell me.
3. Foveated Rendering Balanced at Render Scale 1: can you see the periphery getting coarser (look with your eyes, not
   your head, towards the edges)? If not, try Aggressive. `vr_foveated_debug 1` shows where the rings are (then set it
   back to 0).
4. The frame timing (`vr_profile 1`, or Virtual Desktop's overlay): Render Scale 0.7 + FSR, and Foveated Balanced at 1,
   against 1 with neither. On the 4090 at 3292x3524 FSR should cost ~0.3 ms per frame for both eyes.
5. After these, `vr_profile`'s CSV has the new `upscale` scope; its header does not yet list `vr_upscale` or
   `vr_foveated` among the recorded cvars.

## Finger poses on grips

The author's notes (firing range, 21-16 to 21-20): the shotgun's, super shotgun's, grenade/mine launchers' and
rocket launcher's handles overlap the hand too much; rather than thinning every model, first give options to
tweak the fingers on a held weapon's grip.

**How the drawn hand is posed** (vr_view.cpp): `hand_base.mdl` (the palm) and five finger models, all placed at
the hand's pose plus offsets (`vr_fingers_and_base_*`, `vr_fingers_*` for the five fingers, `vr_finger_<name>_*`
per finger; hand model units, +x towards the fingertips, +y the palm's side, +z the index finger's side). Each
finger is curled by its model frame, 0 open .. 5 curled (the frames past 4 go back the other way: 5 is 3's
shape), from the controller: the index from the trigger, the other three from the grip, the thumb closed when it
touches the face buttons or (`vr_finger_auto_close_thumb`) when the others are mostly closed; blended at
`vr_finger_blending_speed` frames a second. Nothing depends on the weapon: holding a gun with the grip pressed
draws the full fist, and a weapon's hand anchor (Hand X/Y/Z) only moves the whole hand. The helping hand on a
foregrip ("fixed" two-handed mode) is the same hand moved onto the grip and turned by the weapon's
`2h_fxd_h*` angles; its fingers are its own controller's.

The fist's opening is small: the full curl tucks the fingertips back to the palm. On the round 20 guns' thick
pistol grips the grip then runs through the curled fingers (seen from the left, the fingers sink into it; the
author's screenshots). Measured in the mock, for the fingers of the shotgun's grip:

- opening the curl (fewer frames) makes it worse: half-curled fingers point straight out of the palm, into the
  grip (they vanish in it at half open);
- moving the fingers 0.75 units forward (towards the fingertips) puts them round the outside of the grip on
  every gun with a pistol grip, seen from the side and from behind; 1.5 is too far (a gap at the knuckles),
  0.75 back 0.5 (y) about the same as 0.75.

**New per-weapon settings** (weapon keys, `vr_wofs_<key>_NN`; 14 keys, 448 cvars, 2624 weapon cvars in all):

| key | menu (Weapon Offsets, "Fingers") | |
| --- | --- | --- |
| `fgr_x`, `fgr_y`, `fgr_z` | Fingers X (forward), Y (palm), Z (up) | the five fingers on the hand, added to `vr_fingers_*` |
| `fgr_thumb_x/_y/_z` | Thumb X, Y, Z | the thumb alone, added to `vr_finger_thumb_*` |
| `fgr_open` | Grip Openness | 0..1: the most the fingers close on this weapon (0 the full fist, 0.5 half, 1 open) |
| `fgr_thumb`, `fgr_index`, `fgr_middle`, `fgr_ring`, `fgr_pinky` | Thumb/Index/.../Pinky Openness | per finger, added to Grip Openness (negative: closes further) |
| `2h_fgr_open`, `2h_fgr_thumb` | Two-Handed: Other Hand Openness, Other Hand Thumb | the same for the helping hand on this weapon's foregrip |

and one global cvar, `vr_finger_grip_open` (Grip Openness (All Weapons) on the same page; default 0), added to
every weapon's Grip Openness and the helping hand's.

The settings apply only while the hand holds that weapon (not the empty hand, not the fist) or, for the `2h_`
ones, helps hold it. The openness limit blends in and out at the fingers' speed, so picking a weapon up eases
them open. A limit between frames is drawn as the frame above it blended towards frame 0 (the view entities'
zero blend, as the weapons' two-handed recoil damping); with every setting at 0 the fingers are whole frames
exactly as before. The offsets are mirrored for the off hand as the others. All the new keys show in "Print
Changes to Console" (it lists every key that differs from its default).

**Defaults**: `fgr_x 0.75` on the shotgun, super shotgun, nailgun, grenade launcher, rocket launcher, lightning
gun, laser cannon, proximity gun, lava nailgun and the multi grenade/rocket launchers (slots 1-3, 5-7, 9-11,
13, 14), each checked in the mock: the grip no longer shows through the fingers. The super nailguns (4, 12, the
body hides the grip in the mock's views), plasma gun, melee weapons, grapple and the empty hand stay 0; all
openness defaults are 0. No settings version bump: the keys are new, so saved configs pick up the defaults.

Screenshots (mock, fullbright; scratchpad `fingers21_*.png`): `side` (from the grip's left, per weapon: shotgun,
super shotgun, grenade launcher, proximity gun, rocket launcher; Fingers X 0, 0.75, 1.5, 0.75 with Y -0.5),
`back` (the same from behind the hand, as the author's screenshots), `offhand` (the off hand holding the shotgun,
0 and 0.75: mirrored right), `2h` (the helping hand on the shotgun's foregrip at Other Hand Openness 0, 0.3, and
0.3 with the thumb 0.4), `menu` (the page's new Fingers section).

`vr_dumpview` now prints the off hand's position (and whether it helps) and, per hand, the grip limits and the
drawn frame/blend per finger.

**Please check in the headset:**

1. Hold the shotgun, super shotgun, grenade/mine launchers and rocket launcher with the grip pressed: do the
   fingers now wrap the grip without sinking in? Open Weapon Offsets (the game keeps running behind it) and try
   Fingers X/Y/Z on the one in hand; the hand updates at once. "Print Changes to Console" gives the lines to send
   me.
2. Pull the trigger: the index finger curls round it as before (Index Openness limits it if it goes through the
   trigger guard).
3. Two-handed on a foregrip: if the helping hand's fingers sink into it, try Other Hand Openness (0.2-0.4).
4. The weapons without new defaults (super nailguns, plasma gun, axe, swords, hammer): tell me if any need it.

## Melee: blade, hilt, bash, shove

Voice notes vrfiringrange 20-18-41, 20-19-58, 20-21-08 and 20-22-22: many sword swings register as a bash; with two
hands, a normal diagonal cut from the right shoulder down to the left leg, even a very slow one, is almost always a
bash or a shove; empty hands moved slowly shove; a slow stab shoves; in a big swing the hilt lands before the blade and
spoils the hit. A shove should need force and open palms facing the enemy; a bash only the parry pose pushed; a sword
not held level should find it easier to hit with the blade. Later request: a bash or a shove with a weapon in hand bats
projectiles back too, with lenient settings.

### Why

Reproduced in the mock (vrfiringrange's training dummy and a knight from `impulse 248`; `vr_mock_play` keyframe files
from `scratchpad/r20/gen.py`; the shipped `vr_melee_speed 2.75`, `vr_melee_distance 0.2`, `vr_bash_speed 1.2`):

- **A swing that starts from a guard was a bash.** The bash's guard is the parry's pose: the blade within 40 degrees of
  level, across, in front. A two-handed cut from over the right shoulder starts with the blade raised across the head
  (33 degrees up, pointing back to the left: a parry's pose), a side swing with the blade cocked across. "Held still"
  was the wrist under 1.5 m/s in any one frame, and the push the hand faster than 1.2 m/s forward within 0.75 s. The cut's
  first forward motion was that push, however slow: the diagonal from a raised guard (1 s, 1H and 2H), its fast version,
  the side swings at 8 and 14 units all came out "bash 8"; three of three slow diagonals on the knight too. Nothing looked
  at the blade turning, which a push hardly does and a cut always does.
- **Slow empty hands shoved.** Both empty hands near each other in front (45 cm) were a guard with no palm test: held
  still, then pushed at 1.2 m/s, they shoved. Two palms pushed 30 cm in 0.4 s (peak 1.4 m/s), two fists pushed slowly
  and the note's slow two-handed diagonal made with empty hands all shoved. The one-hand palm shove needed 1.2 m/s too.
- **The steadying hand on a two-handed sword was an "empty hand".** `VR_Bash_PalmShove` asked `VRIsHandEmpty`, true for
  the off hand on the grip: palm ahead and moving forward, it shoved. (Not reproduced with the mock's grip pose, whose
  palm faces sideways; the path was open.)
- **The hilt reached first and landed.** A weak point's contact (pommel, hilt, guard) waited 0.05 s for the blade, then
  landed. Close in, the hands reach the monster before the blade has swept in: 1H overhead at 8 units "with the pommel"
  25.8 (the blade does 51), 2H diagonal at 8 units "with the pommel" 30 and 36, 2H overhead at 8 units "with the pommel"
  22.8.
- **Stabs needed a swing's speed.** A sword thrust needed 1.25 x `vr_melee_speed` (3.4 m/s at the wrist) and a 25 m/s2
  snap, and did 0.8 of a swing: a stab of 35 cm in 0.2 s (3.1 m/s) did nothing, one in 0.12 s did 19.6 "thrust". The
  "stab registers as a shove" of the note is the off hand: the steadying hand (above), or an open off hand pushed along.

### The rules now (`QC/vr_juice.qc`)

**Blade first.** Each hand's weapon now has an axis (the hand to its far end: the blade) and how fast it turns
(`blow_axisrate`, degrees/s, per new pose).
- A hilt, pommel, guard or handle contact is a *hilt strike* only when the weapon moves along whole: it doesn't turn
  faster than 150 degrees/s, its tip hasn't gone 1.5 times faster than the wrist in the stroke, nor 2.5 times (and
  over 4 m/s) in the frame, and the pommel may lead. Then it lands after 0.05 s, as before (a pommel punch with the blade back, the hilt
  brought down).
- Otherwise it is a swing's: it waits `vr_melee_hilt_window` (0.2 s) for the blade (mid-blade, outer blade, tip) to
  sweep in, which replaces it, and it never lands itself: after the window it is forgotten and the blade may still land
  later in the stroke. A contact is replaced only by the blade (the guard doesn't replace the pommel).
- A **stab**: a sword stroke along its blade (within 37 degrees), the tip leading, the tip no faster than 1.4 times the
  wrist and the blade turning under 150 degrees/s (a diagonal cut's blade points along its path too, but sweeps round).
  It needs `vr_melee_stab_speed` (0.8) x `vr_melee_speed` and a 12 m/s2 snap, does a full blow's weight (1, a straight
  thrust of a swung weapon is 0.8), and its speed factor starts from half its least speed. The dummy says "stab".

**Bash: the parry pose, held, then pushed.** The guard (the parry's pose, as before) must be held still, the wrist
under 1 m/s and the weapon turning under half of `vr_bash_swing_rate`, for `vr_bash_hold` (0.15 s). Then a push ahead
(within 53 degrees) faster than `vr_bash_speed` within 0.75 s, the weapon turning slower than `vr_bash_swing_rate`
(120 degrees/s), for 0.05 s. Once the weapon turned that fast in a stroke, that stroke can't bash. `developer 1`
prints "bash: none, the weapon turns N deg/s (a swing: Bash Swing Limit 120)" when a guard is pushed but swinging, to
tune the limit by. The hands-together guard is gone (`VR_Parry_HandsTogether`).

**Shove: open palms pushed.** A hand shoves when it is open (empty, the grip not held, not steadying the other hand's
weapon, not carrying or climbing), its palm faces ahead (within 53 degrees), it moves the way the palm faces (within
45 degrees) and forward faster than `vr_shove_speed` (1.8 m/s), its stroke got going at 8 m/s2 or more and went at least
10 cm straight ahead. Both hands (the second at half the speed will do): a shove; one: half as strong. Punches (palm in
or down), slaps (the hand goes sideways), slow reaches and hands on a grip don't shove.

**Bash batting.** With batting on (`vr_deflect`), a weapon bash (from the moment the push starts) or a shove while a hand
holds a weapon bats back, for `vr_bash_deflect_window` (0.3 s), any monster projectile whose path this frame (from 0.1 s
behind it) passes within `vr_bash_deflect_radius` (24 units, a swing's is 14) of the guard: each hand's weapon (butt to
far end), each empty hand, and the line between the hands while they're within 70 cm. It flies back ahead, at its
thrower when that is within 45 degrees of your facing (a swing's: 25 degrees of where the hand points), with the batting
sparks, sounds and haptics (`VR_Deflect_Send`, shared with the swing's batting).

### Settings (Gameplay > Parry, Bash and Headbutt)

| Setting | Default | Menu |
|---|---|---|
| `vr_bash_hold` | 0.15 s | Bash Guard Hold, 0-0.6 |
| `vr_bash_swing_rate` | 120 deg/s | Bash Swing Limit, 30-400 |
| `vr_shove_speed` | 1.8 m/s | Shove Speed, 0.8-4 |
| `vr_melee_hilt_window` | 0.2 s | Hilt Waits for Blade, 0.05-0.5 (header "Sword: Blade, Hilt, Stab") |
| `vr_melee_stab_speed` | 0.8 (x Swing Speed) | Stab Speed, 0.3-1.5 |
| `vr_bash_deflect_radius` | 24 units | Bash Batting Reach, 4-48 |
| `vr_bash_deflect_window` | 0.3 s | Bash Batting Timing, 0-1 |

All new, so no config migration. `vr_bash_speed` keeps 1.2 m/s: the hold and the swing limit now tell a push from a
swing, and the shove has its own speed. Raise Bash Speed if pushes still bash by accident.

**Found on the way (QC compiler).** A chain of four `&&` comparisons assigned to a value (the stab test, into a field
and then into a local) came out true with one of its comparisons false (a debug print showed the stroke going against
the blade, -0.18 m along it); written as nested ifs it is right. The new code uses nested ifs for such values. Older
lines of the same shape (`float same = tracked > 0 && ... && ...` in `VR_Blow_Update`) may be affected; not checked.
And `x = same && dt > 0 ? a : 0` gave 1 every frame: in fteqcc the `? :` binds tighter than `&&`, so it needs
parentheses.

*Coordinator's check:* a test of four-comparison `&&` chains assigned to a value gave the
right result in fteqcc; the wrong value above came from the `? :` in the same expression, parsed
as `a && (b ? x : y)`. So `&&` chains are safe and only `? :` next to `&&`/`||` needs parentheses; no other code has
that shape.

### Tests (mock)

Scripts in `scratchpad/r20/` (`gen.py` writes one `vr_mock_play` file per group: U unarmed, S sword one hand, W sword
two hands, K knight, D bash batting; `table.py` makes the table). vrfiringrange at `host_maxfps 90`, god mode, the
config's `vr_melee_speed 2.75` and `vr_melee_distance 0.2`; "d" is the gap between the player and the dummy's face in
units. "Before" is the round-19 QC (the same engine), "after" the new; each cell is the dummy's line: damage, what, the
point, the wrist's peak speed. The sword rows ran twice after the change; "again" is the second run where it differed.

| Motion | Before | After |
|---|---|---|
| unarmed one palm, very slow 0.3 m in 0.8 s | - | - |
| unarmed one palm, slow 0.3 m in 0.4 s | - | - |
| unarmed one palm, brisk 0.3 m in 0.25 s | 4 shove with the main hand | 4 shove with the main hand |
| unarmed one fist, slow push 0.3 m in 0.4 s | - | - |
| unarmed two palms, very slow 0.3 m in 0.8 s | - | - |
| unarmed two palms, slow 0.3 m in 0.4 s | 8 shove with both hands | - |
| unarmed two fists together, slow push 0.3 m in 0.4 s | 8 shove with both hands | - |
| unarmed two palms, real shove 0.3 m in 0.15 s | 8 shove with both hands | 8 shove with both hands |
| unarmed one palm, real shove 0.3 m in 0.15 s | 4 shove with the main hand | 4 shove with the main hand |
| unarmed punch 0.48 m in 0.14 s | 20 punch with the knuckles (6.2 m/s) | 20 punch with the knuckles (6.2 m/s) |
| unarmed slow punch 0.4 m in 0.3 s | - | - |
| unarmed slap 0.65 m in 0.15 s | 18.5 slap with the knuckles (6.6 m/s) | 19 slap with the knuckles (6.8 m/s) |
| unarmed two-hand slow diagonal 1.0 s | 8 shove with both hands | - |
| sword 1H diagonal slow 1.0 s | 53.2 overhead with the blade (5.8 m/s) | 60 overhead with the blade (6.8 m/s) (again: 59.5 overhead with the blade (6.3 m/s)) |
| sword 1H diagonal fast 0.3 s | 59.1 overhead with the blade (6.0 m/s) | 60 overhead with the blade (7.0 m/s) (again: 60 overhead with the blade (6.7 m/s)) |
| sword 1H diagonal from a high guard very slow 2.0 s | - | - |
| sword 1H diagonal from a high guard slow 1.0 s | 8 bash | no bash (a swing: the blade turned 123-148 deg/s) |
| sword 1H diagonal from a high guard fast 0.35 s | 8 bash | 52.7 swing with the blade (5.6 m/s) (again: 51.1 swing with the blade (5.5 m/s)) |
| sword 1H stab slow 0.35 m in 0.35 s | - | - |
| sword 1H stab medium 0.35 m in 0.2 s | - | 13.6 stab with the tip (3.1 m/s) (again: 13.7 stab with the tip (3.1 m/s)) |
| sword 1H stab fast 0.35 m in 0.12 s | 19.6 thrust with the tip (4.8 m/s) | 25.2 stab with the tip (4.9 m/s) (again: 27.4 stab with the tip (5.0 m/s)) |
| sword 1H guard held, push 0.3 m in 0.18 s | 8 bash | 8 bash |
| sword 1H guard held, slow push 0.3 m in 0.3 s | 8 bash | 8 bash |
| sword 1H hilt strike down 0.35 m in 0.12 s | 20.2 overhead with mid-blade (4.9 m/s) | 23.1 overhead with mid-blade (5.0 m/s) (again: 20.7 overhead with mid-blade (4.9 m/s)) |
| sword 1H pommel punch (blade back) 0.48 m in 0.14 s | 19.8 thrust with the pommel (5.9 m/s) | 19.9 thrust with the pommel (5.9 m/s) (again: 19.3 thrust with the pommel (5.8 m/s)) |
| sword 1H overhead 0.3 s, d 8 | 25.8 overhead with the pommel (5.4 m/s) | 51.6 overhead with mid-blade (5.5 m/s) (again: 51.7 overhead with mid-blade (5.5 m/s)) |
| sword 1H overhead 0.3 s, d 14 | 51.1 overhead with mid-blade (5.5 m/s) | 52.2 overhead with mid-blade (5.5 m/s) (again: 51.2 overhead with mid-blade (5.5 m/s)) |
| sword 1H overhead 0.3 s, d 24 | 50.8 overhead with the blade (5.4 m/s) | 52.1 overhead with mid-blade (5.5 m/s) (again: 52.4 overhead with mid-blade (5.5 m/s)) |
| sword 1H side swing 0.28 s, d 8 | 8 bash | 51 swing with mid-blade (5.5 m/s) (again: 50.6 swing with mid-blade (5.5 m/s)) |
| sword 1H side swing 0.28 s, d 14 | 8 bash | 52.2 swing with mid-blade (5.5 m/s) (again: 50.5 swing with mid-blade (5.4 m/s)) |
| sword 2H diagonal very slow 2.0 s | - | - |
| sword 2H diagonal slow 1.0 s | - | - |
| sword 2H diagonal medium 0.6 s | 60 overhead with the blade (8.7 m/s) | 60 overhead with the blade (9.5 m/s) (again: 60 overhead with the blade (10.1 m/s)) |
| sword 2H diagonal fast 0.35 s | 44.4 overhead with the blade (5.3 m/s) | 46.4 overhead with the blade (5.5 m/s) (again: 45.9 swing with the blade (5.5 m/s)) |
| sword 2H diagonal from a high guard very very slow 3. | - | - |
| sword 2H diagonal from a high guard very slow 2.0 s | - | - |
| sword 2H diagonal from a high guard slow 1.0 s | 8 bash | no bash (a swing: the blade turned 123-148 deg/s) |
| sword 2H diagonal from a high guard fast 0.35 s | 8 bash | 39.8 swing with the blade (4.7 m/s) |
| sword 2H diagonal close d 8 slow 1.0 s | 30 swing with the pommel (7.1 m/s) | 60 overhead with mid-blade (7.9 m/s) |
| sword 2H diagonal close d 8 fast 0.35 s | 36 swing with the pommel (7.0 m/s) | 44.4 swing with mid-blade (5.5 m/s) (again: 44.9 swing with mid-blade (5.5 m/s)) |
| sword 2H stab slow 0.35 m in 0.35 s | - | - |
| sword 2H stab fast 0.35 m in 0.14 s | 18 thrust with the tip (4.4 m/s) | 23.2 stab with the tip (4.5 m/s) (again: 24.5 stab with the tip (4.6 m/s)) |
| sword 2H guard held, push 0.3 m in 0.18 s | 8 bash | 8 bash |
| sword 2H guard held, slow push 0.3 m in 0.3 s | 8 bash | 8 bash |
| sword 2H overhead 0.35 s, d 8 | 22.8 overhead with the pommel (5.0 m/s) | 42.8 overhead with mid-blade (4.9 m/s) (again: 42.7 overhead with mid-blade (4.9 m/s)) |
| sword 2H overhead 0.35 s, d 24 | 43.1 overhead with mid-blade (4.9 m/s) | 43.2 overhead with the blade (4.9 m/s) (again: -) |
| deflect: sword 1H guard push, push -0.30 s | - | - |
| deflect: sword 1H guard push, push -0.15 s | batted (swing, 14 u) | batted (bash guard, 22 u) |
| deflect: sword 1H guard push, push +0.00 s | batted (swing, 14 u) | batted (bash guard, 22 u) |
| deflect: sword 1H guard push, push +0.10 s | - | - |
| deflect: shotgun across, guard push, push -0.15 s | batted (swing, 11 u) | batted (bash guard, 20 u) |
| deflect: shotgun across, guard push, push +0.00 s | batted (swing, 11 u) | batted (bash guard, 23 u) |
| deflect: shotgun in hand, off palm shove, push -0.15 | batted (swing, 11 u) | batted (bash guard, 21 u) |
| deflect: shotgun in hand, off palm shove, push +0.00 | batted (swing, 4 u) | batted (bash guard, 7 u) |
| deflect: sword swing (batting) for reference, -0.05 s | batted (swing, 2 u) | batted (swing, 9 u) |

- The "high guard" diagonal is the note's cut: two hands on the grip at the right shoulder, the blade raised 33 degrees
  across the head (a parry's pose), then down to the left leg. Slow (1 s) it has no blow either: the wrist peaks at
  1.7 m/s, under a swing's 3.4. The very slow ones (2 and 3 s) did nothing before or after. The plain diagonal (the
  blade up and back over the shoulder, not a parry's pose) was never a bash.
- The knight (`impulse 248`, the same two-handed high-guard diagonal, three times): "bash: 1 (weapon)" three times
  before, none after ("bash: none, the weapon turns 148 deg/s"). The knight walks round to the side, so the fast
  swings and pushes after that mostly missed it, before and after (one fast swing hit before, "overhead blow with
  mid-blade", and a push bashed it in some runs after).
- The two-handed overhead at 24 units hits in some runs and not others, before and after: its wrist gets going at
  24.8 m/s2 against the 25 a blow needs. The one-hand overheads and every close swing landed the blade in every run.
- "Hilt strike down" is the hilt brought down with the blade up; the dummy's box reaches above it, so the blade takes
  it, before and after. The pommel punch (the blade back over the shoulder, the pommel leading) still lands with the
  pommel.
- The round-14/19 melee script (`melee_final.txt`: punch, slap, overhead fist, palm shove, two-hand palm shove) gives
  the same lines before and after.
- Bash batting: the spike (`impulse 246`, 600 u/s from 300 units) against a push whose middle is at the offset from
  the spike's arrival. Before, the push itself was fast enough to count as a batting swing (`vr_deflect_speed` 0.6 x
  2.75 = 1.65 m/s), with the swing's 14 units; now the bash's guard bats it, 20 to 23 units from the weapon's line.
  The off-hand palm shove with the shotgun in the main hand bats it too. A push 0.3 s early or 0.1 s late misses both
  ways (early: the push is over before the spike arrives unless Bash Batting Timing is longer; late: the spike has hit).

### In the headset (firing range dummy; `developer 1` for the "bash:" lines)

- Two-handed sword: the note's cut from the right shoulder to the left leg, slow and fast, the dummy close: a blade hit
  ("overhead blow" or "swing ... with the blade / mid-blade") or nothing when slow, never a bash or a shove. If a swing
  still bashes, the console says how fast the blade turned: lower Bash Swing Limit below that (Gameplay > Parry, Bash
  and Headbutt).
- Level guard held still for a moment, then pushed: a bash, one hand and two. If your pushes tilt the blade and don't
  bash ("bash: none, the weapon turns N deg/s"), raise Bash Swing Limit above N; if it needs too long a pause, lower
  Bash Guard Hold.
- Empty hands: slow reaches and slow two-handed motions do nothing; punches and slaps are blows; open palms facing the
  dummy pushed hard shove ("shove with both hands", 8; one hand 4). Tune with Shove Speed (1.8 m/s).
- Close big swings: the dummy's line should name the blade (mid-blade, the blade, the tip), not the pommel or the
  guard. A pommel punch with the blade held back still names the pommel. Hilt Waits for Blade sets how long a hilt's
  touch waits for the blade.
- Stabs: drive the sword along its blade, tip first: "stab with the tip", from about 3 m/s (Stab Speed 0.8 x Swing
  Speed). A slow stab does nothing, never a shove.
- Bash batting: stand in front of an enforcer, a scrag or a knight (the firing range's waves) and push the guard at its
  shot as it comes; a sword and a gun held across, and a palm shove with a gun in the other hand. Bash Batting Reach and
  Timing are next to the batting settings.

## Flashlight on weapons, level loads

Notes e3m1_2026-09-26_22-43-18 (clip the flashlight to a weapon: take it from the chest, bring it to the gun in the
other hand, press B or Y; it follows the gun; take it off with the other hand and B or Y, or it goes back to the chest
when the gun is holstered or dropped) and vrfiringrange_2026-09-26_22-46-11 (the flashlight stays on across level
loads: right when the game moves on to the next level, wrong when a map is started from the console or the menu).

### Clipping it on a gun (`vr_flashlight.cpp`, mode `OnGun`)

- **On:** hold the flashlight (taken from the chest with the grip) near the gun in your other hand: within 12 cm of
  the gun's line from the hand to 3 cm past the muzzle. The lamp lights up (as when a hand is at it on the chest) and
  the flashlight hand gets a short tap. Then press B or Y. Either hand's button works: the flashlight hand's thumb is
  free, and the gun hand's is on its own B or Y. The lamp snaps under the barrel with a clamp's click
  (`sound/vr/flashlight_attach.wav`) and a buzz in both hands. It stays on or off as it was. You can then let go of
  the grip.
- **Where:** the lens is 3.5 cm behind the muzzle and 4 cm below the line the gun aims along, the body hanging below
  the barrel like a foregrip. The beam points where the gun aims. It follows the gun as drawn every frame (recoil,
  flick spin, two-handed aiming, a gun hanging from its foregrip), for guns in either hand. `vr_flashlight_gun_forward`,
  `_up` and `_out` (metres; *On Gun Forward/Up/Out* on the Flashlight page) move it for every gun. The muzzle anchor
  already differs per weapon, so no per-weapon key was needed. The cord still runs from the chest clip to the lamp.
- **Switching it on the gun:** the free hand at the lamp with its trigger, as on the chest. The gun hand's trigger
  always fires, even when the gun hand is within reach of the lamp. `vr_flashlight_toggle` works too.
- **Off:** put the free hand at the lamp and press B or Y.
  - Gripping the lamp: the lamp goes into that hand (click, `flashlight_detach.wav`) and is held as if taken from the
    chest. Let go of the grip and it springs back to the chest.
  - Not gripping: it flies back to the chest on its cord.
  - While the free hand grips the lamp, the gun hand's B or Y also takes it off (the note's "press Y or B with the other
    hand" either way round).
  - A grip at the lamp on its own does nothing to the flashlight. The game gets it, because the gun's foregrip is
    close: two-handed aiming still works with the lamp on. If the hand was on the foregrip and takes the lamp off,
    the game sees that grip let go (`flashlight::tookGrip`), so the two-handed grip ends and nothing stays grabbed.
- **Automatically off, back to the chest:** the gun leaves the hand (holstered, dropped, thrown, switched for another
  gun). Switching the gun's ammo with its button morphs it into its other model, and that keeps the lamp on
  (`view::sameGun`). Also back to the chest on death, intermission and any map change, switched on or off as before.
- **B and Y:** their bindings are next and previous weapon (`impulse 10` / `12`). The flashlight takes the press only
  in the cases above, and the release of a press it took. The off hand's Y at the mouth still records a voice note
  first.
- **Force grab:** only the hand *holding* the flashlight doesn't force grab (`flashlight::holds`). Once the lamp is on
  the gun, both hands force grab again.
- **Multiplayer:** unchanged. The flashlight is client-side (a view entity and a local dynamic light), so others don't
  see it, on the chest or on a gun.
- **Code:** `vr_input.cpp` now passes the upper face button (`HandInput::secondary`, K_BBUTTON main / K_YBUTTON off)
  to `flashlight::button` with the trigger and grip (`flashlight::Button`). `vr_view.cpp` gives the flashlight the
  gun as drawn: `view::weaponMount` (the model, the pose the gun is drawn from and its muzzle anchor) and
  `view::sameGun` (the ammo morph pairs). The clicks are made by `make_flashlight.py` with the switch's (a metal snap
  and the latch catching; for taking it off, a short scrape and a release click). The model and the switch's sounds
  come out byte-identical.

### Level loads

The on/off state is the client's (`st.on`). Level changes in the game (`changelevel`: trigger_changelevel,
the end-of-level intermission, the start maps' episode portals) keep it. A fresh start switches it off, on the chest: `Host_Map_f` (the
`map` command from the console, New Game, the episode/level menus, the VR menu's hub, tutorial and firing range) and
`Host_Loadgame_f` (a save or an autoload: the flashlight isn't in the save, so off). Both call the new hook
`VR_OnFreshStart` (vr_api.h, `// QVR` in host_cmd.c), which runs `flashlight::reset()`. `restart` (the level
restarting after death) is a continuation and keeps it.

### Tests (mock, e1m1 / e1m2; `fl21_log.txt`, `flash21d.png`, `flash21e.png`)

Shotgun in the main hand (`+grabright; impulse 154`), held level across the view (`vr_mock_hand main 0 1.45 -0.45
39 90 0`). The flashlight is taken from the chest with the off hand, switched on and brought under the barrel.

- Near, then Y: "clipped on the main hand's gun". `flash21d.png` 1-3: the lamp held near the barrel (lit up); on the
  gun under the muzzle, its beam along the barrel; the gun turned 40 degrees, the beam following onto the wall.
- Off hand at the lamp, grip + Y: "off the gun, into the other hand" (4: in the off hand).
- Off hand at the lamp, Y alone: "off the gun, back to the chest".
- Off hand gripping the lamp, the main hand's B: "into the other hand" (the dump shows the lamp in the off hand).
- Off hand trigger at the lamp on the gun: off, on. The main hand's trigger: no toggle (it fires).
- `-grabright` (the gun let go): "off the gun, back to the chest" (5-6: on the gun; then the gun is gone and the
  beam is back on the chest).
- On, `changelevel e1m2`: `vr_flashlight_toggle` says "off" (it was on; `flash21e.png` 1-2 lit, then dark). Then
  `map e1m2`: "reset (a fresh start)", and the toggle says "on" (it was off; 3 dark).

### In the headset

- Take the flashlight, bring it under the gun in your other hand: a tap and the lamp brightens. Press B or Y: a
  click, and it rides under the barrel pointing where you aim. Check that it sits well on each gun (under the
  barrel, near the muzzle); if not, say which gun and adjust *On Gun Forward/Up/Out*.
- Switch it with the free hand's trigger at the lamp; shoot with the gun hand: no toggles.
- Take it off: the free hand grips it and presses its B/Y (into the hand); B/Y without the grip (back to the chest).
- Holster the gun, drop it, throw it, switch weapons: it goes back to the chest. Switching the ammo with the gun's
  button keeps it on.
- Two-handed aiming with the lamp on the gun still works; grabbing the lamp off from the foregrip leaves no stuck grab.
- Go through an exit (changelevel) with it on: still on. Start a map from the menu or console, or load a save: off.

## Wearable armour

Note vrfiringrange_2026-09-26_22-47-13: the armour pickups as physics objects, worn by putting them over the torso
(green, yellow and red).

### How it works (`QC/items.qc`, armour section; `QC/vr_carry.qc`)

- **A physics object.** With `vr_armor_wear 1` (default) `item_armor1`, `item_armor2` and `item_armorInv` become
  carryables 0.3 s after they are placed (`AfterPlacementArmor`), the ammo and health boxes' way (`VR_Carry_Setup`:
  grip to carry, nudge with a hand or gun, throw, force grab, rigid body, floats in water). `progs/armor.mdl` is an
  alias model: its box is the model's own bounds at `vr_armor_scale` (default 0.5: Quake's armour is 31 units, about
  1.2 m tall; now about 0.6 m), not centred on its origin (the rigid body collides with the drawn model's box, as
  gibs and backpacks do; no engine change). Skins 0/1/2 as before. The mission packs have no armour of their own.
- **Where it waits.** It hangs at `vr_item_float_height`, as the map's other pickups, spinning, until a hand takes
  it or knocks it; then it is a rigid body and falls (0: it lies on the floor). Force grab pulls it from there.
- **Wearing it.** Let go of the grip with the armour over your torso (`VR_Armor_LetGo`): the carrying hand or the
  armour's middle in a generous volume by the head (0.1 to 0.7 m below the eyes, within 0.3 m of a point 6 cm in
  front of them, not behind the spine, turned with the head's yaw; the upper holsters are inside it). The pickup
  is armor_touch's own (through itemTouch): only if its type times value beats what you wear, with its sound,
  "You got armor", the flash, the body's plating (`vr_body_state`) and the deathmatch respawn. Not better: it doesn't
  go on, "Your armor is better", a dull knock (`player/land.wav`) and a double buzz, and it drops. Let go anywhere
  else: it drops (thrown with the hand's speed). It never goes into the pack at a holster and the trigger never
  takes it (`vr_carry_take` is for boxes).
- **Hint.** Brought over the torso while carried: the holster buzz (vr_holster_haptic_time, fading) if it would go
  on, a faint double tap if yours is better. No visual highlight: the glow round objects is the force grab's
  (engine, `vr_fgfx.cpp`), not reachable from QC without an engine change.
- **Walking over it** does not take it for a player with tracked hands, even with `vr_body_interactions 1`. Bots
  and flat-screen players (no tracked hands) still take it by touch, so co-op and deathmatch with bots work.
- **Deathmatch.** Taken, it comes back after the usual 20 s where and as it was placed, hanging again
  (`VR_Armor_Regen` instead of SUB_regen, which would have left it a trigger); knocked away and left, it goes back
  after `vr_forcegrabbable_return_time_deathmatch` and hangs there again. All of it is server-side.
- `vr_armor_wear 0` (or `vr_carry 0`): touching takes it, as before; the armour's form (object or classic floating
  pickup) is chosen when the map loads.

Two fixes in `vr_carry.qc` that the ammo and health boxes share:

- A force grab's catch hands the object to `VR_Carry_Handtouch`, which since "Carrying starts only on the grip's
  press" wanted the grip pressed within 0.2 s: a grip closed early for the catch (`vr_forcegrab_catch_early`) made
  the caught box, backpack or gib fall out of the hand. The catch has checked the grip already
  (`forcegrabCatching`).
- A box taken (`VR_Carry_Take`) kept its `carry_player`: in deathmatch, once it came back no one could carry it.

**Settings** (Carrying page, "Armour"): **Armour** `vr_armor_wear` (Touch takes it / Wear by hand), **Armour Size**
`vr_armor_scale` (0.3..1; next map).

**Test impulses** (single player): `impulse 250` drops an armour 24 units ahead (green, yellow, red in turn) and
prints the head and hands; `impulse 251` makes the last one come back 3 s after it is worn (deathmatch's respawn,
sped up). `developer 1` prints `armour:` lines (wearable at, over the torso, worn / not worn, back at).

### Tested (mock, vrfiringrange; scripts `armor_t1.sh`..`armor_t3.sh` in the scratchpad)

| Step | Result |
|---|---|
| green carried, let go in front | "let go", falls and lies on the floor |
| yellow carried to the chest (hand at 0 1.3 -0.12), let go | "over the torso", worn: armour 150 type 0.6 |
| red, same | worn: 200 / 0.8; the body's torso turns red |
| green, then yellow, over the chest with the red on | not worn ("Your armor is better"), dropped |
| walked through a hanging armour, `vr_body_interactions 1` | not taken |
| the same with `vr_armor_wear 0` | "You got armor" |
| force grab (trigger, flick, grip early) of a map armour, then to the chest | caught, carried, worn |
| worn with the respawn on (impulse 251), 3 s later | back at its place, hanging; carried again |

QC: 0 warnings. Screenshots (scratchpad `armor20.png`, body mode 3): the green hanging in front; looking down at it
lying between the feet; carrying the red; the red at the chest over the yellow worn; the red worn; the refused
armours on the floor.

### In the headset

- Reach for a floating armour and grip it (or force-grab it); bring it against your chest and let go: the buzz as
  you get there, the pickup sound, and your body's plating. Is the size (Armour Size) right, and is the chest area
  too generous or too tight (e.g. holding it up at your chin, or at your belly)?
- With better armour on, a worse one should knock, buzz twice and drop.
- Walking into a floating armour should do nothing; knocking it with a hand makes it fall; it floats in water.

## Dynamic ambient occlusion

The static world has its ambient occlusion baked (ericw-tools' `-dirt` in the relight), and models get the world's
occlusion through their ambient cube (ROUND17.md). Nothing that moves had any: a monster stood on a lit floor with no
darkening under it, a lift in its shaft left the walls round it as lit as if it were not there, and a model's own
creases (armpits, under a gun, a weapon's grooves) were lit like its outside. SSAO was rejected for VR (it differs
between the eyes, is noisy without temporal filtering, and costs a full-screen pass per eye). Three cheap terms
instead, all in world space, so both eyes see the same (`Quake/vr/vr_ao.cpp`, `AO_FUNCTIONS` in `gl_shaders.h`):

### 1. Ellipsoids round what moves (`vr_ao_dynamic`)

- **What:** every monster, pickup, gib, head, backpack and dropped weapon (alias models; not projectiles, flames,
  beams, see-through things: trail flags and `r_noshadow_list`) is one ellipsoid: its current frame's box through the
  matrix it is drawn with (the renderer's movement interpolation read without changing the entity, the networked
  scale, the item boxes' size), 0.8 of the box across (arms and guns reach its corners) and as tall (it rests on the
  floor). Your body (`vr_avatar.cpp`'s posed skeleton, read from the skinning matrices: each joint's bind position,
  posed, through the entity's matrix): the torso (pelvis to neck), each drawn limb (upper arms, forearms, and with
  the legs, `vr_body_mode 3`, thighs, calves and flat feet), the hands along the forearms, and a fainter (0.8) column
  from the neck to the floor under you (the parts alone are too thin to darken the floor round your feet; legs drawn
  or not). Held and holstered weapons and the hand models are left to the body's shapes.
- **Choice:** once a frame, for both eyes: the 64 nearest whose reach is in front of the view (with room for the other
  eye) and within 2048 units. The CPU cost is 0.004 ms a frame (e1m1, 10 occluders) to 0.018 ms (the firing range,
  59 candidates; `vr_profile` scope "ao occluders").
- **Uniform block 2** (`vr_ao.cpp`'s `GpuBlock`, 10.5 KB, uploaded and bound in each eye's `VR_PushMapLights`): per
  occluder 6 vec4s (centre and reach; three rows into its unit sphere; radii squared; strength, reach, its model's
  group). Each eye bins them into the light clusters' 32 x 16 screen tiles and their 32 depth slices (a mask per tile
  and per slice, ANDed in the shader; the binning is conservative, the test per pixel exact in world space, so both
  eyes get the same result). The depth slices matter: your body's shapes reach round the eye, so their tiles are the
  whole view; without the slices every far pixel tested them (the model pass 0.02 -> 0.18 ms in a test).
- **Shader** (`DynamicAO`, called in the world shader on the baked light before the dynamic lights, and in the alias
  shader on the model's own light): in the ellipsoid's unit-sphere space (the normal by the inverse transpose), a
  sphere's form factor `s^2 cos` (`s` the sine of its angular radius, 1 inside), smoothed where it sinks below the
  horizon (`(cos + s)^2 / 4s`), faded out to nothing at `vr_ao_dynamic_range` radii (2.5). Each occluder takes away
  at most 90%, the shares multiplied, at least 10% left. The normal it is worked out for leans towards where the baked
  light comes from (the deluxemap's direction, or the model's light direction; up without one): the baked light is
  mostly direct light from lamps, which a thing below a surface hardly hides. An occluder never darkens its own model
  (a group per model; your body, hands and weapons are one group, `VR_AliasAO`, `VR_BrushAOSelf`). Liquids are left
  alone.

### 2. Boxes round brush models (`vr_ao_brush`)

- Doors, lifts, platforms, trains, `func_wall`s, and the item boxes (`maps/b_*.bsp`) and the firing range's spawn
  panels: an oriented box (the model's bounds through its drawn matrix, `VR_BrushTransform`'s scale included).
- **Always, not only when moved:** ericw-tools' light gives brush models no shadow and no `-dirt` unless the mapper
  set `"_shadow"` (checked in its `ltface.cc`: the dirt rays hit only shadow casters), so a closed door or a lift at its
  map position is no more in the lightmap than a moving one. Submodels with `"_shadow"` in the map's entities do count
  as baked where the map was lit, and fade in over their first 16 units of movement.
- **The occlusion** (`AOBox`): the exact form factor of the box's faces that face the pixel, by Lambert's polygon
  formula (each edge's angle over its sine by Heitz et al.'s rational fit from the LTC paper, no `atan`), checked
  against a Monte Carlo reference (`boxocc.py`, `t.py`: within 0.001; the fit changes it by under 0.0001). Not
  clipped at the pixel's horizon (a box half behind a surface counts less than it should). A face in the pixel's own plane (a wall flush with a door or a lift's
  side) is not counted: its polygon winds round the pixel and would count as the whole hemisphere (the first build
  blackened e1m1's lift shaft wall). Strength 0.75 of the slider, reach 0.6 of the box's largest half size (16 to 64
  units), faded over the last three quarters; skipped where the whole box is behind the pixel's plane or out of reach.
- Result: the floor at a door's foot and in its slot when open, the floor round the item boxes, the panel wall round
  the spawn buttons, a lift's shaft walls next to it.

### 3. Models' own occlusion, baked per vertex (`vr_ao_models`)

- **Bake** (`VR_AliasVertexAO`, from `GLMesh_LoadVertexBuffer`): for every pose of every Quake model (`.mdl`, one
  surface), each vertex casts 24 cosine-weighted rays (Hammersley) over the hemisphere of its normal, against the
  model's own triangles within an eighth of the first pose's diagonal (a grunt's 9 units, a gun's 2 to 5; 1 .. 16);
  a hit at distance `t` counts `1 - t / reach`. Its own triangles and its neighbours' are left out (on byte-rounded
  vertices the rays near the horizon grazed them: a speckle over the whole model). Per pose, since monsters animate.
- **Stored** in the pose buffer's spare byte (the position's 4th, `meshxyz_t.xyz[3]`: 255 open); a muzzle flash's
  vertex keeps its gun vertex there as before (its flag in the normal's 4th byte), and gets none. The vertex shader
  reads it for both poses (`PoseAO`) and blends; `inst.AO.y` (the slider) scales it.
- **Applied** to the model's own light (the light at its feet, ambient cube, bumps), and in the square root (half, in
  the log) to dynamic lights, whose shadows stand for the rest.
- **Cost at load:** the rays for e1m1's 69 models take 2.0 s (player.mdl 0.8 s: 144 poses x 733 vertices, 1062
  triangles; demon.mdl 0.9 s), so the bake runs on a worker thread (4 threads for the poses, below normal priority)
  from a copy of the poses: the main thread spends 2.6 ms (e1m1, hashing and copying), the models have no occlusion
  of their own for the first second or two after a map loads, then their vertex buffers are built again. Cached per
  model name and content (FNV of the poses; a model of the same name from another game folder is another): later maps
  and `vid_restart` reuse it. No disk cache. Average openness: armour 0.96, backpack 0.85, the player 0.73, the view
  weapons 0.58 (laser cannon) to 0.8 (their hidden insides count too).

### Settings (Graphics - Shadows page, "Ambient Occlusion")

| Cvar | Default | Menu | |
|---|---|---|---|
| `vr_ao_dynamic` | 1 | Contact Shadows (0..2) | monsters, items, gibs, your body |
| `vr_ao_dynamic_range` | 2.5 | Contact Shadow Reach (1.25..5) | in their radii |
| `vr_ao_brush` | 1 | Door and Lift Shadows (0..2) | brush models' boxes |
| `vr_ao_models` | 1 | Model Self-Shadowing (0..2) | per-vertex, baked at load |
| `vr_ao_show` | command | | the occluders this frame, the bake times |

Presets: off in "Off (Quake)" and Low, the defaults from Medium up. Blob shadows are unchanged (`vr_blob_shadows`,
the author's 0): the ellipsoids are now the contact shadow under monsters and items; blobs stay the fallback for Low.

### Cost

`vr_profile`, mock headset at `vr_render_scale 2` (two 2048² eyes), `host_maxfps 1000`, paused, the same frame in
alternating intervals (two runs, three intervals each; GPU ms per frame for both eyes, the minimum of each):

| Scene | Off | All on | Ellipsoids only | Per-vertex only |
|---|---|---|---|---|
| firing range: 4 grunts, your body, 12 armours, 26 spawn panels and buttons (59 occluders) | eyes 0.862, world+brush 0.305, alias 0.007 | 0.914, 0.353, 0.014 | 0.880, 0.322, 0.009 | 0.874, 0.303, 0.007 |
| e1m1, a grunt 90 units ahead, your body (10 occluders) | eyes 0.998, world+brush 0.468, alias 0.008 | 1.014, 0.487, 0.009 | 1.008, 0.485, 0.010 | 0.994, 0.468, 0.008 |

So 0.016 ms (a usual room) to 0.052 ms (the firing range's rows of boxes) for both eyes at the mock's size; at the
Quest 3's eyes (2.77 times the pixels) about 0.05 to 0.14 ms, most of it the boxes. The per-vertex term costs
nothing measurable (one more read per vertex). Before the depth slices, the early outs and the cheaper box edges the
firing range cost 0.087 ms (0.052 now).

### Tested (mock; shots `ao21/` in the scratchpad, composite `ao21_before_after.png`)

- vrfiringrange, QRP (`qbase3`) and 8-bit (`qbase2`), each shot paused with all three off, then on:
  - a grunt (`impulse 244`) by the spawn panels: a soft contact shadow at its feet, its own creases darker (under
    the gun, arms, chest), the panels' boxes darkening the floor along their foot and the panel face round the buttons;
  - the armour and ammo platform: each armour and box has a soft ring on the floor under it;
  - your body (`vr_body_mode 3`, `vr_body_debug 2` shows it in front of you): the floor darker round the feet;
  - a gib (`impulse 245`) on the floor: a small dark rim round it;
  - the double shotgun in hand: the barrels' undersides and the fingers round the grip darker.
- e1m1: the sliding doors at 128 1780 opened: the floor darker at their slots; the lift (`*7`) ridden up: its shaft
  wall no longer blackened (the flush-face fix above).
- Presets: Low sets all three to 0, High to the defaults. No shader or engine errors; 0 warnings.

### What to check in the headset

- Monsters walking over lit floors and past walls: the contact shadow following them, no popping when one enters or
  leaves the nearest 64 (a room full of monsters), nothing different between the eyes.
- Looking down at yourself (`vr_body_mode 3`): the floor round your feet a little darker; crouching and stepping about.
  With the hands against a wall: a faint darkening there.
- Doors and lifts in motion (e1m2's doors, e2m1's lifts): the shaft walls near a lift, the floor under a door; tell me
  if a box looks too dark or its darkening too wide (`vr_ao_brush` 0.5), or if some brush model gets a wrong dark patch
  (a model whose bounds are much larger than its shape, e.g. an L-shaped platform, darkens round its whole box).
- Weapons in hand, close up: grooves and the gun's underside darker, no speckles; monsters' armpits and legs.
- Right after a map loads the models' own darkening appears within a second or two (the bake): tell me if it pops
  visibly, or if loading feels slower.
- If it is too strong or too weak overall: Contact Shadows, Contact Shadow Reach, Model Self-Shadowing.
