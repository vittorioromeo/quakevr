# Round 21

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
