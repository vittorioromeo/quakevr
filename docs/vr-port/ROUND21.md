# Round 21

## Motion recorder

Your proposal: record your own motions, each labelled with what it should do, so that the melee can be tuned
against them, replayed in the mock headset. `docs/vr-port/MOTIONS.md` has the steps, the categories, every column,
playback, the evaluation and the synthetic takes.

- **Where**: VR Settings > Advanced VR Options > **Motion Recorder** (under Melee). Pick the **Category** (named by
  the result expected: Expected Slash, Expected Stab, No Hit, Expected Bash, Expected Parry Pose, Not Parry Pose,
  Expected Parry Bash, Expected Hilt/Pommel, Expected Punch, Expected Palm Shove 1H / 2H, Expected Gun Strike, Other)
  and optionally a **Detail** (the swing's direction, the weapon, ...), turn **Arm Recorder** on, go back to the game.
- **Recording**: in the firing range, in front of the training dummy, **click the off hand's stick** to start a
  take (a beep, a buzz, "REC slash #4" in view), do the motion, **click again** to end it ("SAVED slash #4 (slash:
  4)"). Many in a row: the category stays chosen, and each take is a new file,
  `quakevr/motions/<label>_<date>_<time>.csv`, counted per category in the menu (the files, so across sessions).
  **Delete Last Take** moves a bad one into `motions/discarded/`. A double click (under 0.2 s) is not kept (a
  buzzer). A take that can't be written says so loudly and is kept in memory (`vr_motion_save_unsaved`).
- **What a take holds**, every frame at the headset's rate: the head and both hands as the runtime reported them
  and as the game placed them, in the world, relative to you and relative to the dummy; the buttons, the analog
  trigger and grip, the fingers; the weapons, the two-handed grip, the striking points and the handle's end as the
  melee computes them; your position (the client's and the server's), velocity, yaw and view; the dummy's position,
  angles, box and class; the parry and bash-guard state; every melee event the game registered (hits with their
  damage, kind and striking point, strokes, pushes, parries, batting); every setting that places the hands and the
  weapons, and the melee's. 0.5 s before the click (the lead-in) and 0.3 s after (late hits) too.
- **The button**: the off hand's stick click, the one control free during melee on every controller (Touch
  through SteamVR or VDXR, Index, Vive, WMR); while armed its binding (run) rests. `vr_motion_button 1`: the main
  hand's.

**Playback** (`vr_motion_play <take>`, mock headset): the take's tracking and controls frame by frame at its
recorded frame times, the server frames where they ran, the settings as recorded, the player put where the take has
them relative to the dummy, weapons and grips (two-handed too) taken again, then a report against the take's own
events. **Evaluation** (`vr_motion_eval [folder|pattern]`): every take in the map loaded afresh, judged against
`quakevr/motions/expect.cfg`, a table in `motions/eval_<date>.csv`; deterministic (two runs write the same table)
and about 1.6 s a take. `Misc/quakevr/motion_synth.py` writes synthetic takes from curves.

**Checked on your 398 takes** (Quest 3, VDXR, 120 Hz, world scale 1.25), replayed with their own settings
(`vr_motion_eval ... recorded`): **383 register exactly the hits they registered live** (the same kind, hand,
target, striking point, damage and time): all 82 recorded with the full settings header, and 301 of the 316 before
it (their weapon offsets taken from your config). The 15 others are mostly guns swung as clubs (a hit with "the
grip" appears or vanishes), whose placement in the hand the round's fitted hands changed after they were recorded.
The replayed hands stay within 0.1 units (3 mm) of the take's, relative to the dummy, median; 0.6 units (2 cm) at
the 95th percentile.

What today's melee makes of them (your settings, against `quakevr/motions/expect.cfg`), for the melee's rework:
palm shoves 17/17, punches 24/28 (most "punches" register as slaps or overhead blows), gun strikes 30/35, slashes
117/133, parry poses 48/55, no hit 24/39, and no stab (1/28: thrusts or swings), hilt/pommel strike (0/27) or parry
bash (0/36) registers as such. Resampled at 72 Hz (`rate 72`), 2 of 8 punches change kind: the melee is frame-rate
sensitive.

Keep recording as you were; the takes are sufficient for exact replays.

## Fitted hands

BACKLOG's "Fitted hands", steps 1 to 4, with the weapon offset simplification and the two-handed hotspots you asked
for during the round.

| Step | Result |
|---|---|
| 1. Jointed hand | One skinned model per hand: three joints a finger, an opposable thumb. At the old curl frames it matches the six models exactly, and the curl moves smoothly between them. Skins, blood, powerups, lighting, shadows and the flashlight behave as before. `vr_hand_rig 0` brings back the six models. |
| 3. Weapons | The hand sits where the controller is. The weapon has one transform in the hand. The fingers wrap its grip on their own. Five finger bias sliders and the thumb placement remain as overrides. 30 keys are retired. Muzzles did not move (0.0000 units); foregrips moved at most 0.0001 units. |
| Hotspots | Up to 4 two-handed grips per weapon, each typed Grip or Blade, with a bias. The helping hand takes the nearest one, less its bias, and the fingers wrap it. There is a menu editor, a Show Hotspots view and a QC query. Today's foregrips and sword blade grips were migrated into them exactly. |
| 2. Physics objects | The same solver closes the fingers on boxes, backpacks, gibs, heads and armour, using their real triangles. No finger passes through, and none floats. Limitation: objects are large next to the hand, and the server fit presses them against the fist. Many grips therefore stay close to a fist resting on the object. |
| 4. Palm placement | The palm slides flush with what it holds (at most 5 cm for things, 3 cm for weapons) and turns up to 20° toward its surface. The change eases in and out over about 0.1 s. |

Costs: 0.01 ms more on the main thread per frame than the six models, and no GPU change. Each solve takes 3–28 ms,
but it runs on a worker thread, and the main thread's share is 0.003–0.04 ms. See [Costs](#costs).

### How it works

**The jointed hand** (`Misc/quakevr/make_hand_rig.py` → `quakevr/progs/hand_rig.md5mesh/.md5anim/.mdl`,
`hand_rig_NN_00.lmp`, `Quake/vr/vr_handrig_data.inc`; drawn by `Quake/vr/vr_handrig.cpp`):

- The script reads the six hand models (the palm, the thumb and four fingers, six curl frames each).
- It splits each finger into three segments along its rings of vertices. It fits the joint pivots by least squares
  and each frame's joint turns as quaternions.
- It stores every vertex in its segment's frame, per frame, so each old frame is reproduced exactly (checked to
  1e-14 in Python).
- The thumb gets a metacarpal joint that carries the thenar (the palm's vertices within 4.5 units of the thumb's
  base, weighted).
- It is an MD5 enhanced-model replacement, drawn through the body's skinned path (`VR_AliasBonePoses`, now asking
  `view::handBonePoses` first): one draw per hand instead of six.
- **Normals:** the MDL renderer's normals come distorted by the model's header scale. Each vertex has its own joint,
  whose rotation takes the bind normal to that distorted normal, so the shading matches the old models.
- **Blood:** the bloody skins overlap in UV between parts. The script lays them out on four bands of a 512×512 skin,
  so every part's blood pattern stays as it was.
- **Why skinned rather than 15 rigid segments:** one draw call, the exact old shapes at the old frames, and joints
  that bend without cracks.
- **The cost:** the hands no longer get the per-vertex baked ambient occlusion of alias models (`vr_ao.cpp` skips
  skinned hand rigs, as it does for the body). It is not visible at hand size in the tests below.

**Curls:** each finger's joints follow today's curls (trigger, grip, thumb, `vr_finger_grip_bias`,
`vr_finger_auto_close_thumb`, eased at `vr_finger_blending_speed`). A continuous path replaces the old frame
switches: frames 0..4 close, and frame 5 is frame 3's shape. The Weapon Offsets page's per-finger bias is added on
top.

**The grasp solver** (`Quake/vr/vr_grasp.cpp`). It is GraspIt!'s auto-grasp idea (below) on the hand's own
triangles and the held model's current-frame triangles (`grasp::worldTriangles`):

- Each finger closes its three joints together. When a segment touches, the joints before it stop and the rest go
  on. Contact is found by halving the step, to 1/1000 of a step. Contact means the hand's and the model's triangles
  cross (segment/triangle tests over a uniform grid).
- A settle step then lets a distal joint close further while the one before it opens, so a finger wraps rather
  than stopping stiff on its tip.
- A finger that already starts inside the model (a gun grip through the palm) closes from the fist outward instead
  (`fromClosed`).
- The thumb tries 16 turns of its metacarpal: opposition 0/15/30/45° × swing 0/±15/−30°, at a small cost per
  degree. It keeps the best wrap; failing that, the least inside.
- **Palm placement (step 4):** the solver tries palm moves on a grid, flush with the surface along the palm's
  normal. It scores closure plus wrap and keeps the best. It also turns the palm up to `vr_hand_fit_palm_turn`
  toward the surface under it (a ray along the palm's normal, or the nearest point on its side). The move is capped
  by `vr_hand_fit_palm` (things) and `vr_hand_fit_palm_weapon` (weapons).
- **Where it runs** (`vr_view.cpp` `updateGrasp`): once per grip change, on a worker thread (`std::async`).
  Results are cached, 32 of them, per model, frame and pose in the hand. A result stands while the thing stays
  within 0.5° and 0.15 units of where it was solved. A result for a hand that moved a little meanwhile (10°,
  2 units) is used until the next one lands, so nothing flickers while a weapon lags. Joints ease to the solved
  stops at the finger blending speed. The solved joint stops cap the controller's curls; they don't replace them,
  so an open hand still opens.

**Weapons: the simplified model:**

- The hand is always where tracking and the rig put it (`s.pos`, `s.rot`), plus the palm fit.
- The weapon is placed in the hand by one transform: Offset X/Y/Z, Pitch/Yaw/Roll, Scale. This is the same
  transform as before, so the gun, the muzzle and the aim are unchanged.
- The fingers wrap its grip automatically.
- Per weapon, only small overrides remain: a curl bias per finger (`fgr_bias_thumb` … `fgr_bias_pinky`, −1..1 of
  a full curl) and the thumb's place (`fgr_thumb_x/y/z`).
- The "Weapon Only" sliders, the hand anchor vertex and offset, and the fixed finger openness are gone (audit
  below).

**Hotspots:** up to four per weapon, in the weapon's model space.

- The world point of a hotspot `h` is `R_EntityMatrix(weapon) · [mirror] · S(k) · T(Offset + (0,0,vr_gunmodely)) · h`,
  the weapon's frame as drawn, before its Scale.
- **Grip:** a point where the other hand can hold the weapon: a foregrip, a pump, a magazine, the sword's handle.
- **Blade:** the half-sword grip along the blade. Its X is the share of the way from the hand to the tip where the
  grip is centred (0.75 on the swords). The zone runs from `max(0.3, X − 0.3)` to 1.05 of the hand-to-tip line, as
  round 18's blade grip did.
- **Bias** (units) is subtracted from the distance a hotspot is picked by: larger means easier to take.
- The helping hand takes the grip nearest to it, less the bias, and keeps it while it holds (`chosenGrip`).
- Two-handed aim and the take/keep distances use the chosen grip (`vr_twohand.cpp`: distance − bias). The blade
  uses its own share and bias.
- The solver wraps the helping hand's fingers there, like any other grasp.

### The data format (`vr_weapons.inc`, cvars `vr_wofs_<key>_NN`)

| Key | Values |
|---|---|
| `hsN_type` (N = 1..4) | 0 none, 1 grip, 2 blade |
| `hsN_x`, `hsN_y`, `hsN_z` | grip: the point in the weapon's model units (as above); blade: `hsN_x` is the share of the hand-to-tip line (y, z unused) |
| `hsN_bias` | units off its distance (0..10) |

The engine API (`vr_weapons.hpp`) is `weapons::hotspot(slot, i)`, `setHotspot`, `hotspotKey`, `maxHotspots` (4)
and `HotspotType`. The view API (`vr_view.hpp`) is `view::weaponHotspot(hand, i)`, which gives the world position
as drawn this frame, the type, the bias and the share, and `view::hotspotAt(hand, p)`, which maps a world point to
hotspot coordinates.

QC (`QC/builtins.qc`, for the melee code and mods):

    vector(entity player, float hand, float index) weaponhotspot;              // world position ('0 0 0' if none)
    float(entity player, float hand, float index, float what) weaponhotspotinfo; // what 0: type, 1: bias, 2: blade share

`hand` is `cVR_MainHand` or `cVR_OffHand`, and `index` runs 0..3. These are the local player's hotspots, as the view
draws them; other players get none (the view computes them client-side). `impulse 253` prints them. The query
doesn't change the blade/hilt/muzzle queries the melee code uses.

### The audit: every per-weapon key (`vr_wofs_<key>_NN`)

Kept means read as before. Replaced means a new mechanism does its job. Folded means its value is now expressed
elsewhere. Retired keys stay registered and unused (`weapons::retired`), so saved configs load without "unknown
command". **Print Changes to Console** skips them.

| Key(s) | Status | Notes |
|---|---|---|
| `id` | kept | The model to slot map. |
| `x`, `y`, `z` (Offset) | kept | The weapon in the hand. The hand no longer rides on it. |
| `pitch`, `yaw`, `roll` | kept | The weapon's turn in the hand. |
| `scale` | kept | The weapon's size. The fist slot's value still scales the hand. |
| `hand_av`, `hand_x/y/z` (hand anchor vertex and offset) | **retired** | The hand is at the controller. The old anchors were within 0.8 cm of it for every slot (`vr_hotspots_check`: 0.02–0.21 units, most under 0.1). |
| `gunoff_x/y/z` | **retired** | 0 in every slot. The muzzle check below confirms nothing moved. |
| `length` | **retired** | Never read. |
| `2h_fxd_mh_ox/oy/oz` | **retired** | Never read. |
| `w_hvelmult`, `w_htvelmult` | **retired** | Never read. |
| `fgr_x/y/z` (fingers offset per weapon) | **retired** | Replaced by the solver. Its job was to move the fingers off the grip. |
| `fgr_open`, `fgr_thumb`, `fgr_index`, `fgr_middle`, `fgr_ring`, `fgr_pinky` (openness) | **replaced** | The solver wraps the grip. `fgr_bias_*` is the per-finger override (0 by default). Your round-20 openness values described a fixed pose and are not carried over. |
| `2h_fgr_open`, `2h_fgr_thumb` | **replaced** | The helping hand's fingers are solved on the hotspot. |
| `2h_dispmd`, `2h_hand_av`, `2h_fxd_ox/oy/oz` (the fixed foregrip) | **folded** into hotspot 1 (Grip) | The same point (see Migration). |
| `2h_blade` | **folded** into hotspot 2 (Blade) on the swords | `hs2_x` = 0.75. |
| `fgr_thumb_x/y/z` | kept | Thumb placement ("Thumb X/Y/Z" under Fingers on the Weapon). |
| `hide_hand` | kept | |
| `zb`, `zb_2h` | kept | `zb_2h` applies while a grip hotspot is held. |
| `muzzle_av`, `muzzle_x/y/z` | kept | Unchanged. |
| `ch_mode_z` | kept | |
| `2h_mode` | kept | 0 normal, 1 no virtual stock, 2 no two-handed, 3 sword. |
| `2h_x/y/z`, `2h_pitch/yaw/roll` | kept | Renamed in the menu to **Aim Offset X/Y/Z** and **Aim Pitch/Yaw/Roll**: they move and turn the two-handed aim, not anything drawn. Round 20's "Other Hand X/Y/Z" and "Other Hand Pitch/Yaw/Roll" labels were wrong. |
| `2h_fxd_hp/hy/hr` | kept | The helping hand's angles on the foregrip. Pitch is used only on the swords. |
| `weight`, `w_posmult`, `w_dirmult`, `w_2hposmult`, `w_2hdirmult` | kept | |
| `wpntxt*`, `wpnbtn*` | kept | The ammo screen and button. |
| new: `fgr_bias_thumb/index/middle/ring/pinky` | added | −1..1 of a full curl, added to the wrap. |
| new: `hs1..4_type/x/y/z/bias` | added | Hotspots. |

Global cvars:

- `vr_weapon_only_x/y/z` are removed, together with the page's "Weapon Only" section. They were deltas for the
  menu, never saved.
- `vr_finger_grip_open` is retired and still registered.
- The global `vr_fingers_*` / `vr_finger_*_x/y/z` offsets, `vr_gunangle`, `vr_gunmodel*` and the rest are unchanged.
- `vr_gun_z_offset`'s help still says it moves the guns; it only sets the torso height for the wall sweep. I didn't
  change it.

### Migration (`vr_wofs_version` 15)

- **Shipped defaults:** the fixed foregrip of every slot that had one (1–7, 9–15, 18, 19) became hotspot 1 (Grip).
  `vr_hotspots_legacy` worked it out from the round-20 keys, with the weapon at the origin unturned, mirrored as
  the helping hand saw it, and the result was pasted into `vr_weapons.inc`. The swords' blade grip became
  hotspot 2 (Blade, 0.75).
- **Your saved config:** on load, any slot whose foregrip or blade keys, Offset or Scale differ from the shipped
  defaults is marked. The first time that weapon is drawn, its hotspots are rebuilt from your values the same way
  (`weapons::takeHotspotMigration`). This needs the model, so it happens in the view. Tested: a config with a
  custom `vr_wofs_2h_fxd_ox_02` got a hotspot at exactly the old foregrip for those values.
- **Your placements:** Offset, Pitch/Yaw/Roll, Scale and the muzzle are the same keys with the same values (your
  `30580890` placements are merged).
- **Verified** with `vr_hotspots_check`: every slot, 6 random weapon poses, both mirrors
  (`final/hotspots_check.txt`).
  - Foregrip, old against hotspot: at most 0.0001 units apart.
  - Muzzle, round 20's placement (with GunOffset) against now: 0.0000 units.
  - Old hand anchor against the controller: at most 0.21 units (0.79 cm, the grappling hook), most under 0.1.
- **End to end:** the base build (`498e187d`) and this one were run on the same mock pose for each weapon from the
  axe to the lightning gun, plus the sword. The main and off hand muzzles are identical to 3 decimals
  (`final/muzzles_old.txt`, `muzzles_new.txt`).

### The menu

- **Hand/Gun Calibration > Fingers:**
  - Jointed Hand (`vr_hand_rig`).
  - Fit Fingers to What You Hold (`vr_hand_fit`).
  - Palm Fit: Things (`vr_hand_fit_palm`, cm).
  - Palm Fit: Weapons (`vr_hand_fit_palm_weapon`, cm).
  - Palm Turn (`vr_hand_fit_palm_turn`, degrees).
- **Weapon Offsets**, in this order:
  - **Weapon in the Hand:** Offset X/Y/Z, Pitch/Yaw/Roll, Scale, Hide Hand.
  - **Fingers on the Weapon:** the five biases, Thumb X/Y/Z.
  - **Muzzle.**
  - **Other Hand's Grips (Hotspots):**
    - Hotspot 1–4 picks the one to edit.
    - Type: None, Grip or Blade.
    - X/Y/Z, or Along the Blade for a blade.
    - Put It Where the Other Hand Is.
    - Bias.
    - Remove This Hotspot.
    - Show Hotspots.
  - **Two-Handed Aim.**
  - **Ammo Screen.**
  - **This Weapon:** Weight, Print Changes to Console, Reset This Weapon.

  Round 20's Hand X/Y/Z, Weapon Only and openness sections are gone. The page rebuilds itself when another hotspot
  is picked or its type changes.
- **Show Hotspots** (`vr_show_weapon_hotspots`):
  - Grips show as green points and blades as orange lines. The hotspot being edited is white.
  - Each has a faint ball as large as its bias.
- The advanced list's older **Hotspots** page is the body's holster hotspots, not these.

### Settings

| Cvar | Default | |
|---|---|---|
| `vr_hand_rig` | 1 | Jointed hand. 0: the six models (the old path, kept). |
| `vr_hand_fit` | 1 | Fingers wrap what the hand holds. 0: the controller's curls only. |
| `vr_hand_fit_palm` | 5 | cm the palm may move to sit flush on a held thing. |
| `vr_hand_fit_palm_weapon` | 3 | The same on a weapon. At 5 cm (the first try), the hand looked detached from the guns in the before/after shots (3–4.6 cm). Now it stays within 2.8 cm. |
| `vr_hand_fit_palm_turn` | 20 | Degrees the palm may turn toward the surface. |
| `vr_debug_grasp` | 0 | 1 prints each solve (triangles, palm move, thumb turn, ms on the worker, ms from the request, ms on the main thread). 2 adds each finger's stops. |
| `vr_show_weapon_hotspots` | 0 | See above. |
| `vr_weapon_hotspot` | 1 | The hotspot the Weapon Offsets page edits. |

The archived cvars are saved. `vr_weapon_hotspot` and `vr_show_weapon_hotspots` are not.

Other changes:

- `MAX_CVARS` goes from 4096 to 8192 (`Quake/cvar.c`). The new per-weapon keys pushed the registered count over
  the limit: 32 slots × 106 keys, plus the rest.

Test commands:

- `vr_mock_fingers <hand> <trigger> <grip> [<thumb>]`.
- `vr_grasp_dump <hand> <file.obj>`: the drawn hand and the held model's triangles.
- `vr_hotspots_legacy`.
- `vr_hotspots_check`.
- `impulse 252`: a gib or head in the empty off hand, 9 kinds in turn.
- `impulse 253`: prints the held weapons' hotspots through the QC query.

### Files

- New:
  - `Misc/quakevr/make_hand_rig.py`
  - `Quake/vr/vr_handrig.hpp/.cpp`, `vr_handrig_data.inc`
  - `Quake/vr/vr_grasp.hpp/.cpp`
  - `quakevr/progs/hand_rig.*`, `hand_rig_0N_00.lmp`
- Changed:
  - `Quake/vr/vr_view.cpp/.hpp`: rig hands, grasp jobs, palm fit, hotspots, checks.
  - `vr_weapons.cpp/.hpp/.inc`: keys, retired list, hotspot API, migration.
  - `vr_twohand.cpp`: hotspot bias, blade from the hotspot.
  - `vr_hands.hpp`
  - `vr_menu.cpp`, `vr_menu_pages.inc`
  - `vr_cvars.inc`
  - `vr_avatar.cpp`, `vr_ao.cpp`
  - `vr_held.hpp/.cpp` (`heldEntity`)
  - `vr_builtins.cpp`
  - `vr_backend_mock.cpp`
  - `vr_main.cpp`
  - `Quake/cvar.c`
  - `QC/builtins.qc`, `QC/weapons.qc`

### Tests (mock headset; composites in the scratchpad's `hands/final/`)

- **`step1_main_hand_old_vs_jointed.png`, `step1_off_hand_old_vs_jointed.png`:** the empty hand in five finger
  poses (open, fist, trigger, thumb, grip), two views each, old models against the jointed hand, with the
  difference ×4. Silhouettes are identical. The small shading differences come from the old parts being lit as six
  separate entities.
- **`step1_blood.png`:** the bloody skins, the same patterns.
- **`step1_lighting_flashlight_powerups.png`** (e1m1): lit, the flashlight, the quad's glow, and the ring's
  transparency, old against jointed. They are the same.
- **`curl_sweep_old.png` / `curl_sweep_jointed.png` / `curl_sweep_open_jointed.png`:** grip curl 0.00 to 1.00 in
  steps of 0.05.
  - The old models step from frame to frame: jumps at 0.10, 0.30 and 0.50, and at 0.90 the hand opens back to
    frame 5 (frame 3's shape).
  - The jointed hand closes steadily with no jump. Round 20's openness jump is gone with the fixed openness.
- **`guns_before_after.png`:** the shotgun, super shotgun, nailgun, super nailgun, grenade launcher,
  rocket launcher and lightning gun, two views each.
  - Old six models: fingers through the grip, or a fist beside it.
  - Fitted: fingers around the grip. The hand is at the controller.
- **`guns_grasp_views.png`:** the same grips from `vr_grasp_dump` (front, below, oblique), not fitted against
  fitted. Not fitted, the fingers pass through the grips; fitted, they close round them.
- **`sword_blade_grip.png`, `twohanded_grasp_views.png`:**
  - Your screenshot's case: the sword held level across the face by the blade (`vrfiringrange_2026-09-27_01-27-29`).
  - The shotgun's foregrip.
  - Old, jointed not fitted, fitted. Fitted, the helping hand closes round the blade and the barrel. Not fitted,
    the fingers pass through them.
  - `foregrip_hotspot.png` adds Show Hotspots.
- **`objects_item_shells.png`, `objects_item_health.png`, `objects_backpack.png`, `objects_gib.png`,
  `objects_head.png`, `objects_armour.png`:** the off hand gripping each at three angles, before and after.
  - Fingers stop on the surface: none pass through, none float.
  - Where the server's fit has pressed the object against the knuckles, the result stays close to a fist on it.
  - The palm turn helps the armour, the backpack and one gib (20°).
- **`menu_weapon_offsets.png`, `menu_hand_gun_calibration.png`:** the pages.
- **Numbers:** `hotspots_check.txt`, `muzzles_old.txt` / `muzzles_new.txt`, `solves.txt`, `prof_final.txt`.

### Costs

All costs were measured with `run.sh --exclusive` on vrfiringrange. The shotgun was in the main hand and a gib in
the off hand, over 600 frames each.

| | view entities (CPU) | alias (CPU) | GPU |
|---|---|---|---|
| Six models (`vr_hand_rig 0`) | 0.02–0.03 ms | 0.06 ms | 0.52 ms |
| Jointed, not fitted | 0.03–0.04 ms | 0.05–0.06 ms | 0.52 ms |
| Fitted | 0.03–0.04 ms | 0.05–0.06 ms | 0.52 ms |

Solves on the worker thread, exclusive (`vr_debug_grasp 1`):

| Held | Solve time (worker) | Main thread's share |
|---|---|---|
| Guns | 11–17 ms | 0.01–0.03 ms |
| Axe | 21 ms | |
| Sword | 10 ms | |
| Gibs and heads | 3–28 ms | |
| Boxes | 15–30 ms | |

The hand eases to the result 12–32 ms after the grip. Until it lands, the fingers follow the controller or the last
grasp. Solves happen only when the grip, the held thing or its place in the hand changes (cache hits cost
nothing).

### Limitations and risks

- **Physics objects:** the server's `held::surfaceFit` places the object against the drawn fist before the fingers
  close. Most objects are several times the hand's size, so many grips end as a fist against the object. The
  object is not placed into the open hand. Moving the object to the palm, or letting the fingers open more before
  the server fits, would be next. It touches server-side carrying, which I left alone.
- **No finger-touch sensors:** a lifted index doesn't straighten off the trigger unless the trigger value drops.
  OpenXR's touch paths are not read yet.
- **Only the local player:** hotspots from QC are the local player's, and solves happen only for the local
  player's own view.
- **The palm on weapons** moves up to 3 cm, so the hand may look slightly off the controller on some guns. At 0 it
  stays exactly on the controller.
- **Thumbs on pistol grips** sometimes sit along the side rather than over the top. Thumb X/Y/Z per weapon can
  fix that.
- Retired keys stay registered, 30 × 32 cvars, which is why `MAX_CVARS` went up.
- The hands have lost their per-vertex baked AO.
- Picking up a two-handed grip triggers a new solve for the helping hand. For about 20 ms, its fingers show the
  controller's curls.

### In the headset

- [ ] The empty hands look and move as before, with smoother curls. Try grip half pressed and slow squeezes.
- [ ] Every gun: the hand is where your hand is, and the fingers are on the grip, not through it. If one is off,
      use Weapon Offsets > Weapon in the Hand (the gun moves in the hand), then Fingers on the Weapon (bias,
      thumb).
- [ ] Palm Fit: Weapons: at 3 cm, does any gun feel detached? Try 0 and 5.
- [ ] Two hands: the foregrip on the shotguns, launchers and nailguns. Hold the sword by the blade level across
      your face, as in your screenshot.
- [ ] Hotspots: add a second grip (for example the pump or the magazine) with Put It Where the Other Hand Is.
      Turn on Show Hotspots and check that the nearer one is taken. Bias makes one win.
- [ ] Your saved config: the foregrips are where you tuned them (your 2H values were converted once, on load).
- [ ] Boxes, backpacks, gibs, heads and armour: do the fingers stop on them? Does the fist-against-the-object look
      bother you? Palm Fit: Things and Palm Turn change it.
- [ ] Quad, pentagram, ring of shadows and the flashlight on the hands.
- [ ] Jointed Hand off: the old hands, for comparison.

## Wrist gadget, hologram, casings, flashlight

Five of your voice notes: casings splashing into water, a test button for the hologram, messages only on the
gadget (a chime from the wrist and a buzz instead), an FPS counter on the gadget, and a straight flashlight held like
a real one, with a second grip (B/Y) and clipped along the barrel.

| Note | Result |
|---|---|
| Casings in water | a tiny splash, the smallest ripple and a quiet, higher plip where a casing goes in; it then sinks slowly |
| Hologram test | Screens > Messages > **Show a Test Message**: one of the game's own messages, as the game sends them; press again and they stack |
| Messages only on the gadget | Screens > Messages > **Messages Only on the Gadget** (off by default): nothing in front of you; the message waits in the hologram until you look; a chime from the gadget and a double buzz |
| FPS counter | Graphics > Performance > **FPS Counter on the Gadget**: ` 90 FPS CPU  5.2 GPU  8.1` floating just under the gadget |
| Flashlight | a straight 13 cm torch through the fist; B/Y (away from a gun) flips between the low and the overhead grip; on a gun it lies parallel to the barrel |

### Casings into water (`vr_shells.cpp`, `vr_particles.cpp`)

- A casing crossing into water, slime or lava (flying, or rolling in on a sloping floor) finds the surface there and
  makes `particles::shellSplash`: the splash preset's parts at a casing's size. That is 5-9 small drops thrown up 3-12
  cm (half leave a little ring where they fall back), one ring riding the ripple's crest, and a wisp of foam. Lava
  adds an ember or two. It follows vr_water_splash, _size and _ring_size.
- The ripple is `water::addRipple` with a strength under 1. Those now get a lower floor: 1.1-2.3 units at your
  amplitude of 16, against a shot's 7.7. Other splashes are unchanged (they are 1 and up).
- The sound is `shell_plip1..3.wav`: the recorded plips (plip1, 3, 4) pitched up 1.4-1.65 times and cut to 0.2 s, made
  by `make_sounds.py` (credited in CREDITS.md). It plays at the surface at 0.25-0.55 × vr_shells_sound ×
  vr_water_sounds, louder the faster the casing goes in. The mixer has no pitch, so the pitch is in the files.
- The water takes 70% of the casing's speed and half its spin. It then sinks at the old slow rate.
- Cheap: at most 4 splashes and 2 sounds each tenth of a second, however many casings land. `developer 2` prints each
  one.

### Hologram: the test button, messages only on the gadget (`vr_gadget.cpp`)

**Which messages go where.** Before this round:

| Message | Where it shows |
|---|---|
| Centre prints: a key needed, a secret, the maps' trigger texts, runes | the hologram while the gadget faces you; otherwise in front of you as always |
| Server prints: pickups, a powerup running out, deaths, chat | the hologram (and at the top of the view with Console Messages set to In view or Both) |
| The engine's lines: cvars, cheats, errors, the level's name when it loads | the log over the gadget (diagnostics) |
| The intermission's and finale's texts | in front of you (the gadget is put away) |

**Notifications** are centre prints, plus server prints that aren't pickups ("You got/get/receive ..."). A pickup is
not news: the thing is already in your hand. The level's name is an engine line, printed as a map loads, and stays in
the log. It is also on the gadget's screen.

- **Show a Test Message** (`vr_message_test` with no arguments). Each press sends the next of six of the game's own
  messages, the way the game sends them, with its sound: "You need the gold key", e1m1's "You must press the three
  buttons...", "You found a secret area!", "Quad Damage is wearing off" (a print), e4m4's two-line exit text, "A
  secret cave has opened...". It works with the menu open. Raise the gadget beside the panel to see them.
- **Stacking.** The hologram now keeps its own queue of messages. They are taken from the console as they are printed,
  so a line pieced together from several prints comes through whole. Centre prints stack as well: before, a new one
  replaced the last. The same text again, like a locked door touched twice, keeps the shown one on.
- **Messages Only on the Gadget** (`vr_messages_hologram_only` 0/1):
  - The game's messages never show in front of you. That covers centre prints, and server prints when Console
    Messages is set to In view or Both. The engine's lines still go where Console Messages says.
  - A notification waits in the hologram until you look. Its life (Hologram Time) starts when you first see it,
    meaning the hologram faces you and has at least half faded in, and it grows in then. It is dropped after five
    minutes unseen, or on a new map. Waiting messages are kept ahead of pickups when space runs short.
  - A new notification while the gadget is out of view plays Quake's message sound (misc/talk.wav) at the gadget. It
    stays there as the arm moves, so you hear it in stereo from the wrist. The gadget's hand also gets two short soft
    pulses, like a watch.
  - The game's own talk.wav and secret.wav are moved to the gadget then, not doubled. They arrive just before their
    message, so one chime covers a burst.
  - Messages that come while you are looking behave as before.
- **Hooks:** `VR_GameSound` (cl_parse.c's sound packets), `VR_GameLineOnWrist` (Con_DrawNotify), and
  `VR_CenterPrintOnWrist`, which now also covers this mode.

### FPS counter (`vr_gadget.cpp`, `vr_main.cpp`)

- `vr_gadget_fps 1` shows ` 90 FPS CPU  5.2 GPU  8.1` just under the gadget as you see it. It faces you, like the
  hologram above it, and fades in with the gadget's facing.
- The characters are about 5 mm, in the screen's colour, with the hologram's shade: faint scanlines, no flicker or
  glitches, so the figures read steadily. The words are dimmer than the figures.
- **FPS** comes from the frames' own periods.
- **CPU** is our work per frame: the host frame less the runtime's and the swap's waits (the memory log's `busy_ms`).
- **GPU** is the eyes' drawing (`gpu_eyes_ms`), from the timer queries that run every frame anyway.
- All three are averaged over half a second, through a third reader of `profile::takePhases`. There is no new
  measurement code. Its small image is drawn again only when the figures change.
- **Cost** (exclusive runs, mock, e1m1, the gadget raised; 8 alternating 30-second blocks, `vr_memstats`):

  | | Off | On |
  |---|---|---|
  | CPU busy_ms | 0.611 | 0.611 |
  | GPU eyes | 0.465 | 0.473 |

  The difference is within noise. vr_profile agrees:
  - its GPU scopes cost 0.005-0.006 ms per eye;
  - its image costs 0.066 ms twice a second, which is 0.001 ms per frame on average.
- An earlier block order (all Off first) showed +0.07 ms of GPU time. It went away once the blocks alternated, so it
  was the GPU's clocks drifting, not the counter.

### Flashlight (`make_flashlight.py`, `vr_flashlight.cpp`)

**The model.** A straight tactical torch, 13 cm long:
- a knurled tube, 2.6 cm across, with three grip rings;
- a ribbed tail cap with a rubber button, where the cord goes in;
- a rubber switch on the tube just below the head;
- a finned head, 3.7 cm across, with a steel bezel and the lens.

It is flat-shaded like the other props. The origin is in the middle of the grip. `flashlight_flip.wav` is new: the
torch scuffing round in the palm and seating. The other sounds come out byte-identical.

**In the hand.**
- The tube goes through the curled fingers, along the fist's axis. The switch faces the knuckles, under the thumb.
- **Low grip** (the default): the beam comes out of the thumb's side. Pitch the hand forward a quarter turn from a
  pistol's aim to light ahead: the thumb points out and forward, as when you hold a torch at your side.
- **Overhead "searching" grip:** away from a gun, press **B/Y** with the torch in your hand and it turns round in the
  fist. The beam now comes out of the little finger's side: with the fist raised by your head and the thumb towards
  your face, it lights ahead. Press again to go back.
  - Each flip gives a click and a light buzz.
  - Each hand remembers its own grip for the next time it takes the torch, across letting go and across maps.
  - The flip is instant, a regrip. Animated over 0.16 s, the hand's grasp had to be solved again every frame and the
    palm jumped by up to 5 cm.
- **The fitted hands' grasp** wraps the fingers round the tube. At your In Hand Forward/Up (-5 / -4 cm), the palm
  moves 1.2-1.5 cm to fit, low or overhead, in either hand (`vr_debug_grasp`).
- **Voice notes:** while the off hand holds the torch, its Y turns the torch round even at your mouth, instead of
  starting a voice note. The overhead grip is by the head. The other hand's `+vr_note` binding, or putting the torch
  back first, still records.

**On the chest.** Stored hanging, lens down (see "Stored on the chest, worn on the head" below; it pointed forward
at first).

**On a gun.** It lies parallel to the barrel, the lens 1 cm behind the muzzle, running back along the gun, its switch
out to the side:
- It sits under the gun. Where the gun's underside is deep over the torch's length, it goes beside the gun on the side
  away from your body instead.
- This is fitted once per gun and size, from the gun's drawn triangles, in 0.2-0.5 ms: under the shotgun, super
  shotgun, nailgun, rocket launcher and lightning gun (4.7-10 cm below the aim line); beside the super nailgun and
  the grenade launcher.
- The attach rules are as in round 20. It follows the gun as drawn, in either hand, mirrored for the off hand.

**Everything else:**
- The beam comes out of the lens along the tube.
- A hand reaches the torch anywhere along its axis, from the tail to the lens, within 9 cm, both to take it and to
  switch it.
- The switch's clicks come from the switch, and the clamp's from the torch, instead of from inside your head.
- The haptics are as before.

### Stored on the chest, worn on the head (your two later flashlight notes)

"The flashlight should be stored vertically on the body, not horizontally, encouraging the player to attach it on a
weapon or hold it in the off-hand. It should also be possible to attach the flashlight on top of the player's head,
with the same controls as for the guns."

The torch now lives in three places: the chest (stored), a gun, and the head. You also hold it in either hand.

**Stored on the chest** (`mountPose`):
- It hangs straight down from a clip on a strap on the off hand's side: 9 cm above the chest joint and 8.5 cm to the
  side, its tube 2.2 cm in front of the chest.
- The lens is at the bottom, the switch faces out. The lens leans out from the body by **Lean Out**
  (`vr_flashlight_tilt`, 8°; the slider was Tilt Down).
- That places it under the collarbone, below and inside the upper holsters, and clear of the wrist gadget.
- Switched on there, it lights only the floor at your feet. That is on purpose: you take it, clip it on a gun, or
  put it on your head.
- Taking it is as before: the trigger at it switches it, and an empty hand's grip takes it. A hand reaches it anywhere
  along the tube, within 9 cm.
- Forward/Up/Out still move the clip.

**On the head** (`headPose`, mode `OnHead`). The head is where a head torch is most useful: it lights wherever you
look, and both hands stay free for a gun in each hand, climbing, or carrying.
- **To put it on**, hold the torch (either hand) against a temple or the forehead. Within 10 cm of the temple on that
  side, you get a tap and the lamp brightens, as by a gun. Then press B or Y (either hand's).
  - It clips on at that side's temple with the gun clamp's click and a buzz in the hand.
  - The overhead grip is held further out, so B/Y there still turns the torch round.
- **Where it sits:**
  - The lens is level with your eyes, 3.5 cm above them and 8.5 cm out to the side. The tube runs back along the side
    of your head.
  - The beam goes where the head looks, crossing your line of sight 4 m ahead.
  - The light comes from beside the eyes, not from them, so shadows still read.
  - Nothing of it is ever in view: it is behind the eye plane and out at the side.
  - The cord isn't drawn there (it runs behind the neck).
  - **On Head Forward/Up/Out** (`vr_flashlight_head_*`) move it.
- **To take it off**, put a hand at it:
  - B or Y sends it back to the chest on its cord;
  - B or Y while gripping it, or the grip alone, takes it into that hand;
  - either way with the clamp's detach click.
- **Voice notes:** the off hand at the head torch uses Y for the torch, not a voice note.

**Death, level changes, fresh starts:**
- On death, at the intermission and on any map change, the torch goes back to the chest from the head, a gun or a
  hand, switched on or off as it was.
- A fresh start (the map command, New Game, a loaded save) switches it off, on the chest.
- A suicide (`kill`) is not a death as the client sees it: the respawn is at once, so the torch stays on the head.

**Tested** (mock, e1m1; composite `round21_flash2/flashlight_storage_head.png`):
- the chest storage before and after, from your view (off, then on: only the floor at your feet lit) and in the body
  preview from the front and the side;
- the head mount from your view: looking ahead, turned right, and looking down-left, the beam following the head
  with nothing of the torch in view. Also in the body preview facing you, the lens at the temple;
- the off hand: taken, to the left temple, Y (on the head), Y at it (back to the chest);
- the main hand: taken, to the right temple, B (on the head), gripped off into the hand, let go (back to the chest);
- the overhead grip by the head still flips, not attaches;
- `changelevel`: back on the chest, still on. `map`: off.

### Tested (mock headset)

Composites are in the scratchpad's `round21_gadget/`:
- **`casings_before_after.png`** (e1m2's pool, every second frame): before, the casing vanishes into the water; after,
  the tiny splash, ring and ripple. The log shows "VR shell into a liquid: ... splash, plip".
- **`hologram_and_fps.png`:**
  - the test message pressed 1, 2, 3 and 5 times: they stack, and are gone 7 s later;
  - Messages Only with the wrist down: nothing in view. Raised 8 s later, the message is still there. It is gone 5 s
    after it was first seen;
  - e1m1's "You can jump up here..." trigger with the wrist down. Its talk.wav is moved to the gadget, with one chime
    and the buzz logged. It waits, and a secret's two messages stack with it;
  - the menu path: the cursor on Show a Test Message, pressed;
  - Console Messages In view: the game's print is left out of view while the engine's line stays;
  - the FPS counter under the gadget;
  - re-checked after merging the fitted hands.
- **`flashlight_in_hand.png`:** before and after, off hand, four sides.
- **`flashlight_grips.png`:** both grips in both hands, close and from 90 cm, with the beam on the wall. In every case
  the beam points ahead, including overhead with the thumb towards the face.
- **`flashlight_on_guns.png`:** every gun before and after, the off hand's guns mirrored.
- **`flashlight_chest.png`:** before and after.
- `fl_model_new.png`: the model on its own.

### In the headset

- [ ] Shoot the shotgun over water, and flick-reload over a pool: a small splash and a quiet plip per casing, nothing
      louder than the shot's own splash.
- [ ] Screens > Messages > Show a Test Message with the wrist raised: change Hologram Text Size, Height and Effect
      and press again. They stack.
- [ ] Messages Only on the Gadget on, wrist down:
  - [ ] walk into a trigger or a secret, or touch a locked door. Nothing in front of you, a chime from your wrist
        (left or right ear, as the arm is) and two soft pulses;
  - [ ] raise the wrist seconds later: the message is there, and fades after Hologram Time;
  - [ ] pick something up: no chime, no buzz, as intended. Tell me if you want pickups to notify too.
- [ ] Graphics > Performance > FPS Counter on the Gadget:
  - [ ] readable at arm's length?
  - [ ] check that the figures match what you feel (GPU is the eyes' drawing, CPU our work, both without the
        runtime's waits).
- [ ] Flashlight in either hand:
  - [ ] the tube in the fist, the fingers round it;
  - [ ] pitch the hand forward to light ahead;
  - [ ] B/Y away from a gun flips it to the overhead grip. Raise it by your head, thumb to your face: it lights
        ahead. Flip back;
  - [ ] let go and take it again: that hand's grip is kept.
- [ ] On the chest: hanging lens down, easy to grip with either hand, clear of the upper holsters and your arms?
      (Forward/Up/Out and Lean Out move it.)
- [ ] On the head, with either hand:
  - [ ] hold the torch at a temple: a tap, then B/Y puts it on your head. Look around: the beam follows, the
        shadows read, nothing in view;
  - [ ] take it off: B/Y at it (back to the chest), or grip it (into the hand).
- [ ] Clip it on each gun: parallel to the barrel, under it (beside the super nailgun and the grenade launcher). Say
      which gun looks off, and use On Gun Forward/Up/Out.
- [ ] Off hand holding the torch at your mouth: Y flips it rather than recording. Is that the right priority?

Not verified: the haptic pulses and the stereo position of the chime were only logged in the mock (it has no
haptics or ears), and the counter's legibility at the Quest 3's resolution was not checked (the mock is 960 × 540).
