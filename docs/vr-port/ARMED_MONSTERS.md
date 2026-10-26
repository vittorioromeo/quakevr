# Armed monsters

A monster holding one of your weapons, drawn as the weapon lying about is, fired and swung with your weapons' own code,
disarmed, re-armed and dropping it on death (ROUND22.md, "Armed grunts" and the sections after it). The code knows
nothing of the monster that holds the weapon: what it needs is the monster's **row** in the armed monsters' table.
Rows now: the grunt (`monster_vr_grunt`, row 0) and the enforcer (`monster_vr_enforcer`, row 1).

## Where things are

| Part | File |
| --- | --- |
| The table (QC): one row a monster | `QC/vr_armedmon.qc` (`VR_AM_Row`, `VR_AM_Spawn` in `vr_armedgrunt.qc`) |
| The layer: pool, picking, prop, firing, melee, ammo, reloads, drops, AI | `QC/vr_armedgrunt.qc` |
| Its poses and attacks (punches, swings, the chainsaw; sent to clients) | `QC/vr_armedgrunt_pose.qc` |
| Disarming, tugs, falls, reach | `QC/vr_armedgrunt_play.qc` |
| Shooting the weapon out (hits on the held weapon or its hand adding up, the roll) | `QC/vr_armedgrunt_shot.qc` (hooks: `vr_armedgrunt_shot_defs.qc`) |
| Fields and hooks other files call | `QC/vr_armedgrunt_defs.qc` |
| Tests (`vr_agrunt_test N`, `vr_agrunt_test_kind` the row) | `QC/vr_armedgrunt_test.qc`, `QC/vr_armedgrunt_play_test.qc` |
| The engine's table (the hands' IK, the held weapon drawn on the body) | `Quake/vr/vr_hitreact.cpp` `armedRigs` |
| The models without their gun | `Misc/quakevr/make_vrgrunt.py` (`MODELS`) |
| The toolgun's spawn page: the table's lists, the ghost's weapons | `QC/vr_armedmon_ui.qc`, `vr_toolgun.cpp` (`armed`), `vr_toolgun_panel.cpp` (`armedPage`) |

The names keep the armed grunts' `VR_AG_*` / `vr_ag_*` / `vr_agrunt_*` (saves store think functions and fields by
name; the settings keep their cvars): read "AG" as "armed".

## A row (QC `VR_AM_Row`)

| Field | What | Grunt | Enforcer |
| --- | --- | --- | --- |
| `vr_am_spawnname` | its spawn function (the toolgun's page makes it so) | `monster_vr_grunt` | `monster_vr_enforcer` |
| `vr_am_classname`, `vr_am_spawn` | its monster: the classname and spawn function it keeps (health, sounds, pains, deaths, ragdoll, hit zones, struggles: every system keyed on it) | `monster_army`, `monster_army` | `monster_enforcer`, `monster_enforcer` |
| `vr_am_model` | its model without the gun | `progs/vrgrunt.mdl` | `progs/vrenforcer.mdl` |
| `vr_am_hand` | its right hand's three vertices (their middle: the grip) | 32 26 31 | 41 364 365 |
| `vr_am_gun_rear`, `_muzzle`, `_roll` | the hidden gun's rear and muzzle vertex pairs, its top and bottom: its line and roll in every frame | 497 545, 498 514, 529 496 | 425 420, 407 100, 409 412 |
| `vr_am_f_stand`, `_n_stand` | stand frames (the legs under punches and swings) | 0, 8 | 0, 7 |
| `vr_am_f_run`, `vr_am_rund[8]` | run frames and their steps | 73, 11 15 10 10 8 15 10 8 | 23, 18 14 7 12 14 14 7 11 |
| `vr_am_f_atk` | 9 aiming frames (one shot in the 5th; streams 4th..8th) | 81 (shoot1) | 32 (attack2: its shot in attack6) |
| `vr_am_f_shove` | 6 shove frames (a gun's butt) | 114 | 102 |
| `vr_am_f_reload`, `_n_reload` | reload frames (11 drawn, these repeating) | 29, 11 (load) | 0, 7 (stand) |
| `vr_am_f_take` | picking a weapon up: this and the next 3 frames and back, a crouch or a knee (the hand reaches down to it) | 63 (painc4: a crouch) | 87 (paind5: a knee) |
| `vr_am_pain1`, `_pain2` | its pain sounds (tugs, disarms) | soldier/pain1, 2 | enforcer/pain1, 2 |
| `vr_am_loot_aid`, `_loot_n` | its backpack with no reserve | 5 shells | 5 cells |
| `vr_am_classes` | the weapon classes it may hold (1 << `VR_AG_CLASS_*`) | all | all |
| `vr_am_fit_scale`, `_fit_shift` | the poses (made on the grunt's body) onto its body: scaled, then moved | 1 1 1, 0 0 0 | 1 1.35 1.1, -3 0 7 |

The engine's row (`armedRigs` in `vr_hitreact.cpp`): its model id, vertex count, the right hand's and the gun's
vertices (ranges: never a hand's), the gun's line, its arm bones' names (the ragdoll rig's), its chest anchor vertex.

Its arms (`vr_agarms`, ROUND22.md "Armed humanoids' arms: posed and skinned"): `vr_am_anchor` / `_anchor_at` (a chest
vertex its poses are placed from; the engine row has the same index), `vr_am_sh_r` / `_sh_l` (its shoulders at rest),
`vr_am_arm` (shoulder to wrist), `vr_am_grip_r` / `_grip_l` and `vr_am_gun_fw` (its hands on its own gun at rest, the
gun's way): `vr_agarms_info` prints all of these for a loaded model (and its skin: arm vertices, hands, joints).

## Arms: posed by the weapon, IK, skinned (vr_agarms)

| Part | Where |
| --- | --- |
| Where each weapon is held (its entity frame is the hand's, as your controller's is) | QC `vr_armedgrunt_pose.qc`: `VR_AGP_ArmsGun` (long guns from the rest pose's hold, raised by `vr_agarms_shoulder`; one-handed and dual out from the shoulder), `VR_AGP_BladeHand` (melee: the tip on the blade's line, the knuckles leading), fists and free hands as points, `VR_AGP_RunBob`, the reach clamp; flags `VR_AGP_ANCHOR` (64), `VR_AGP_REACH` (128) |
| A weapon's own points (muzzle or tip, butt, second grip) in its frame | `weapondrawnpose(p, 0, 8..10)` (`view::weaponLocalGeometry`) |
| The arms' IK, the hands' grip frames, the weapons drawn on the hands | `vr_agarms.cpp` `solve` (`vr_hitreact.cpp` `relinkSkinned` calls it) |
| The skin (weights per vertex, once a model) | `vr_agarms.cpp` `skinOf`; the alias shader's `RigArm` (`vr_glsl.h`), rows from `kickInstance` |
| Grips: one per kind and hand for every weapon, a weapon's own on top | `vr_aggrip.cpp` (`global`, `offset`); VR Settings > Armed Grunts > Grip Editor (Edits: Every Weapon / This Weapon Only) |
| Grips set by hand: the Pose Studio (Grip Editor > Drag Mode, `vr_posestudio`) | `vr_posestudio.cpp` (the boxes, the drag, the panel); `relinkSkinned` calls `posestudio::adjust` before the solve (a dragged weapon at the controller) and `note` after it (`agarms::Placed`'s hands, elbows, shoulders, chest); `aggrip::setPreview` / `set` |
| Poses set by hand (Pose Studio stages 2-4): pose keys per kind, weapon class, pose and strike phase | `vr_agpose.cpp` (the keys: `vr_agrunt_poses`, the shipped table, copy between kinds, the dump); QC `vr_armedgrunt_pose.qc` `VR_AGK_*` (read through `weapondrawnpose(world, code, 11..18)`, blended by the pose's weights, laid on the targets after the torso's turn; the blow's line moved with them in `VR_AGP_Strike`; the studio's hold `VR_AGK_Hold` through `weapondrawnpose(g, kind*100+class, 19)`); a pole past 0.8 weighed up in `solveArm`; frozen monsters can't be disarmed, tugged or struck |
| Numbers, the contact sheet | `vr_agrunt_ik_err` (agarms, agt, agprop lines), `vr_agrunt_test 45` |
| A take's one-knee kneel: legs posed (IK) and skinned, the body above on the pelvis, its reach bend (`vr_aglegs`) | `vr_aglegs.cpp` (`solve`, `carry`, `rows`; `vr_aglegs_info`); the alias shader's `RigLeg`; the take's weight sent by QC `VR_AGP_Pose` (flag 1's byte); keys `agpose::legCode` (the Pose Studio's Pickup: pelvis, feet, knees boxes); test 45's take sheet (`scratch1 -1`) |

A new row needs only its row's fields above and its engine row's anchor: its skin is found from its rig.

## Hooks in the monster's own code

- Its death (`army_die`, `enf_die`): `if(!VR_AG_Dying()) { <its own gun drops> }` — an armed one's weapon falls.
- Its loot (`SoldierDrop`, `EnforcerDrop`): `if(VR_AG_SoldierDrop()) return;` — its reserve's ammo in its backpack.
- `CheckAnyAttack` (ai.qc) sends any `.vr_ag` monster to `VR_AG_CheckAttack` (no change needed).
- Its pains end in its own run (`enf_run1`, `army_run1`): `ai_run` -> `CheckAnyAttack` -> `VR_AG_CheckAttack` puts it
  back on the layer's run (no change needed).

## Dual wielding

Two slots: `vr_ag_wid/wflags/prop/reserve` and `vr_ag_wid2/...2`. The layer acts on the first; an event on the second
weapon (a grab, a blow near it, a blow on it, a parried left-hand swing) swaps the slots first (`VR_AG_Swap`), so every
rule (tugs, yanks, disarms, drops) is the one-weapon code. Which hand holds a weapon is its prop's `.vr_ag_left`; a
weapon left alone is passed to the right hand (`VR_AG_Settle`, after every drop). Pairs (`VR_AG_Pairs`): two one-handed
melee weapons (`VR_AG_OneHanded`: swords but AD's hammer and mace, the crowbar, the axe), or two guns not too big
(`VR_AG_DualGun`; the per-weapon flag `VR_AG_Big`: the hammers, the rocket launchers, the Laser Cannon, the chainsaw, the
two-handed melee weapons, the flamethrowers); never a blade and a gun. Dual guns (ROUND22.md, "Armed monsters: dual-wielded
guns"): each in one hand along the rifle line's pitch straight ahead, the left at the right hand mirrored
(`VR_AGP_Gun`); the second fires a frame behind the first (`VR_AG_AtkFrame`: shots and rounds in turn, streams together);
`VR_AG_ShootProp` adds the penalties (`vr_agrunt_dual_penalty`: spread and aim error times 1 / `vr_2h_spread_reduction`,
a heavy gun's kick up `weaponheft(model, 1)` x 0.1 s, coming back over `vr_heavy_recoil_time`); both reload together
(`vr_ag_reserve2`), a spent one with no reserve is thrown away. `vr_agrunt_dual_pickup`: one holding a weapon that pairs
goes for a second (`VR_AG_WantsSecond`). Tests `vr_agrunt_test 42` (pairs, penalty, kick, a fight, the pickup option),
`43` (test 41's frames with a shotgun and a nail gun), `44` (a row of dual-gun humanoids: scene `agrunt_dual_guns`). Map keys
`weapon2`/`weaponflags2`; from the pool `vr_agrunt_dual_chance` (Armed Grunts page: Dual Wielding). Attacks: the hands
in turn, a quarter of them both at once (mirrored keys), the other hand following up (`VR_AGP_Begin`, `vr_ag_act_hand`).
The left weapon's pose: `VR_AGP_MeleeLeft` (ready, carried at its side, swung on the right's keys mirrored); sent in
QVR_SVC_ARMPOSE behind flag 8 (its entity, its turn), drawn by the client at the left hand's target (`relinkArms`), the
left arm's IK on it. Lost one: out of a fight it goes for another one-handed weapon (`VR_AG_WantsSecond`). Dying: both
fall. Test: `vr_agrunt_test 40`; eyeshots `vr_agrunt_test 14` with `scratch4 1` (a sword in each left hand).

## The tug, its ragdoll, pickups, parries (ROUND22.md, "Armed monsters: the physical tug, ...")

- The tug (`vr_agrunt_tug_ragdoll`): `VR_AG_TugRagBegin` (a sure-footed stagger, the hand holding its forearm through the
  weapon: `ragdolltug`), `VR_AG_TugRagFrame` (the hold's force against `vr_agrunt_tug_break_force` / `_impulse`), the
  engine's `tugRagdoll` (vr_box3d_ragparts.cpp) and `Stagger.tug` (vr_ragactive.cpp: its arms' pull). Generic: any row
  whose monster can stagger as a ragdoll.
- On a ragdoll its weapons ride the limb nearest their grip (`VR_AG_RagProp`); the client's kicks (a guard, a hit
  reaction) carry them with the forearms (vr_hitreact.cpp `relinkArms`).
- Pickups: `VR_AG_TakeBegin` / `VR_AG_TakeGrab` (the weapon where it lay until the hand closes, then lifted:
  vr_armedgrunt_pose.qc `VR_AGP_Pose`); a new row needs only a crouching or kneeling `vr_am_f_take`.
- Parries: `T_Damage_VRMelee` runs `VR_Parry` for an armed monster's blow (combat.qc).
- A deliberate tug (ROUND22.md, "Armed monsters: a deliberate tug-of-war"): the weapon is grabbed only by the fist on
  its drawn shape (vr_physics.cpp `handOn`, `armedWeapon`; `vr_agrunt_grab_slack`), not by a hand moving faster than
  `vr_agrunt_grab_speed` (`VR_AG_PropHandTouch`); holding enemies (vr_foegrab.cpp `tryTake`) leaves a fist on such a
  weapon to it (`physics::fistOnArmedWeapon`). The break (`VR_AG_TugBreak`): only a pull above
  `vr_agrunt_tug_pull_force` counts, kept up `vr_agrunt_tug_pull_time` s; a yank `vr_agrunt_tug_snap` x the break
  force at once; wounds weaken the grip by `vr_agrunt_tug_hurt_ease` (no pain halving). Won: it stumbles
  `vr_agrunt_tug_won_stumble` s.
- Falls (`vr_agrunt_fall_keep`): a gentle one keeps the weapon on the ragdoll's hand (`VR_AG_FallViolent` measures the
  floor's drop, the fall's and the body's speeds against `vr_agrunt_fall_keep_height`, `_speed`, `_fling`); down, a
  grab takes it at once and a blow over `VR_AG_DOWN_SWING` knocks it away.
- Pickups: the row's `vr_am_f_take`, `vr_am_n_take` (down to the lowest frame, held there while the hand closes, back
  up: `vr_ag_takestep`); while it bends the hand reaches down with the bend and the weapon is sent as none (the client
  draws it where it lies).
- Tests 31..38 (`QC/vr_armedgrunt_play_test.qc`); scenarios `agrunt_tug`, `agrunt_parry`, `agrunt_pickup`, `agrunt_guard`,
  `agrunt_fall`.

## Shooting the weapon out (ROUND22.md, "Armed grunts: shooting the weapon out")

- Generic: the volumes are the held prop's own (`VR_AG_WeaponLine`: behind the grip to the muzzle) and its hand's (the
  grip to `VR_AGS_FOREARM` units towards the shoulder, guessed from the monster's yaw, its height and the prop's
  `vr_ag_left`). A row with a very different build (a wide or tall monster) may want its shoulder guess
  (`VR_AGS_HandLine`) to read the row.
- Hooks: `FireBulletsImpl` (each pellet's path, `VR_AGS_Shot`; TRUE stops it on the weapon), `LightningDamage`,
  `T_DamageDeal` (`VR_AGS_Hit`: splash; a player's shot skips `VR_AG_Hit`'s blow rule) and `VR_AG_WeaponStruck` (each
  held weapon's frame: the nails, rockets and grenades flying through it, the roll: `VR_AGS_Frame`). Each weapon keeps
  its own count on its prop (`.vr_ags_acc`): dual wielding needs nothing more.
- Test `vr_agrunt_test 50` (`QC/vr_armedgrunt_shot_test.qc`); scenario `agrunt_shootout`.

## The toolgun's spawn page

Spawn > Monsters > Armed Humanoid (`vr_toolgun_panel.cpp` `armedPage`; `docs/vr-port/TOOLGUN.md`): Kind, Right Hand,
Left Hand as dropdowns (the panel's `dropdown` widget, reusable). The lists are the table's, asked of QC once a map
(`QC/vr_armedmon_ui.qc`; `vr_toolgun.cpp` `armedRefresh`): the kinds `VR_AM_UiCount` and each row's name, spawn function
(`vr_am_spawnname`), model and stand frame; the weapons every "weapon" code 1..127 `VR_AM_UiWeapon` says a row holds
(`VR_AG_Decode`: its data there; the row's `vr_am_classes`), one-handed or not (`VR_AG_OneHanded`), by class. Right Hand:
Random from Pool ("weapon" 0: the pool and `vr_agrunt_dual_chance`), Unarmed (-1), a weapon; Left Hand ("weapon2"): None
or a weapon pairing with the right one (two one-handed melee weapons, or two guns: `toolgun::armed::pairs`). The choice is `vr_toolgun_armed_kind`, `_weapon`,
`_weapon2` (kept), made the spawn tool's with each pick (`vr_toolgun_select_armed [kind [weapon [weapon2]]]` from the
console). The ghost holds the weapons where the layer's own pose holds them a moment after it is made
(`VR_AM_UiGhost`: `VR_AGP_Pose` on stand-ins, quiet: placed, nothing sent; the melee stance settled), turned with the
ghost; the arms are the model's (no IK on a ghost). Debug > Toolgun: "Armed Humanoid Lists" (`vr_toolgun_armed_list`).

## Replacements: a map's grunts and enforcers (ROUND22.md, "Armed humanoid replacements")

- `vr_agrunt_repl` (Combat > Armed Grunts > Armed Humanoid Replacements; all off by default): `monster_army` /
  `monster_enforcer` call `VR_AGR_Replace(kind)` (`QC/vr_armedrepl.qc`) first, which runs `VR_AM_Spawn` with the row's
  own gun (`VR_AGR_NativeOf`). Its prop is native (`.vr_ag_native`): `VR_AGR_NativeShot` fires the monster's own shot,
  never spent, unscaled (`VR_AG_DamageScale` takes the inflictor); alone it attacks with its monster's chances, pauses
  and frames (`VR_AGR_NativeChance`, `_NativeFinished`, `_NatStep`); its loot is its monster's (`VR_AG_SoldierDrop`
  FALSE for a replacement with no reserve). A new row wants its native gun in `VR_AGR_NativeOf` and its attack's steps
  in `VR_AGR_NatFrame` / `_NatFires` / `_NatRefire`, its chances in `VR_AGR_NativeChance`.
- Variety (`VR_AGR_Pick`, seeded by the map, the spawn point and the kind): tiers `VR_AGR_TierCode`, eligibility
  `VR_AGR_Eligible` (owned by a player; rare ones from `vr_agrunt_repl_rare_map`), dual from the same rolls, a gun and an
  axe (`.vr_ag_gunaxe`: `VR_AGR_GunAxe` swaps the slots by range and ammo; `VR_AG_PairsFor` lets such a one pick up the
  other kind; the pose `VR_AGP_LeftBlade` beside a one-handed gun). A map's own are rolled on the first frame with a
  player (`VR_AGR_Frame`), a spawner's at once. Precaches at worldspawn: `VR_AGR_Precache`.
- Tests `vr_agrunt_test 70..73` (`QC/vr_armedrepl_test.qc`); scenes `agrunt_repl_gunaxe`, `agrunt_repl_map`.

## Adding a monster

1. Find its gun's vertices (vr_monstermods.cpp `knownSwords` has the stock monsters'), add a `MODELS` row to
   `make_vrgrunt.py` and run it (`python Misc/quakevr/make_vrgrunt.py`; the model goes to `quakevr/progs/`).
2. Find its right hand's vertices and the gun's line: the vertices that stay still in the gun's space over its living
   frames, on its right (an analysis like ROUND22.md's, "Armed monsters"); the gun's two farthest vertex pairs along
   it (which end is the muzzle: an attack frame), a top and a bottom near its middle.
3. A row in `VR_AM_Row` (a `VR_AM_<NAME>` number, `VR_AM_COUNT` up one, `vr_am_spawnname` its spawn function: the
   toolgun's Armed Humanoid page lists it by itself), its frames from its `$frame` lines.
4. Its spawn function in `vr_armedgrunt.qc`: `void() monster_vr_<name> = { VR_AM_Spawn(VR_AM_<NAME>); };` with a QUAKED
   comment, then `python Misc/trenchbroom/fgdgen.py`.
5. The two hooks in its death and loot functions (above).
6. The engine: its model id (`vr_modelmetadata.inc`), its monster file prefix (`vr_modelmetadata.cpp` `monsterFiles`),
   its ragdoll row (`vr_ragdoll.cpp`, the monster's seeds with the new id), its `armedRigs` row.
7. The toolgun: nothing (the Armed Humanoid page asks the table). Per-weapon entries in `vr_toolgun_packs.inc` only if
   wanted for `vr_toolgun_select` (appended: no number above moves; pack "Armed <Name>s": off the grid unless
   `vr_toolgun_armed_tiles 1`).
8. Test: `vr_agrunt_test_kind <row>`, then `vr_agrunt_test 1, 2, 3, 20..30` and the eyeshots (`vr_agrunt_test 14`,
   `15` with `scratch2` the phase).

## Arms in motion: the glitch finder, the robust solve, the upper body (ROUND22.md, "Armed humanoids in motion")

- **Glitch finder:** `vr_agrunt_test 46` (Debug > Automation & Self-Tests > Armed Humanoids: Glitch Finder): every kind
  and weapon class through its real fights; `vr_agglitch_report` the worst moments and each metric's worst and 95th
  percentile per subject (files in `quakevr/agglitch`); `vr_agglitch_shots N` eye images of the worst (front: left eye,
  side: right eye). Its 13-20 s: you at touching distance (states `close ...`; `agglitch close:` lines: its gun pointing
  up over 45 degrees, its muzzle over its head, its parry raised, disarms; `temp1 1`: that loop alone). Run it before
  and after any change to the poses, keys or the solve.
- **The solve each frame** (`vr_agarms.cpp`): body proxies (`vr_agbody.cpp`: torso, pelvis, head) the hands are pushed out
  of and the elbow kept out of (`vr_agarms_body`); the elbow's swing rate-limited the way round outside the body
  (`vr_agarms_elbow_rate`); the second grip's hysteresis and ease (`vr_agarms_second_hold`); a pickup's reach eased in by
  distance (`vr_agarms_reach_ease`); goal jumps eased (`vr_agarms_jump_ease`).
- **The shoulders** (ROUND22.md, "Armed humanoids' shoulders"): the skin from the real joint (`vr_agarms_shoulder_skin`:
  the deltoid or pauldron on the arm, no forearm turn on the upper arm's top half, the blend about the joint), a
  clavicle per side (`vr_agarms_clavicle`, `_most`: the joint shrugs and comes forward with the arm, the shoulder's top
  with it; rows 22..25 of `RigArm`), the shoulder's cone and twist in the IK (`vr_agarms_limits`, `_shoulder_cone`,
  `_shoulder_twist`), the upper arm's skin kept out of the torso (`vr_agarms_arm_clear`), the enforcer's pistols
  one-handed (`vr_agrunt_enforcer_one_hand`), its stand frames as it fires (`vr_agrunt_atk_stand`, the row's
  `vr_am_atk_stand`), armed ragdolls drawn with the arm skin's blend (`vr_ragdoll_arm_skin`). The finder's metrics
  armpen, swing, twist, stretch measure them; `vr_agarms_cost` the solve's time.
- **The upper body** (`vr_agrunt_upper`): the chest upright on the frames' hips (their own chest motion in shots, reloads,
  pains and runs taken out; a pickup's crouch, falls and deaths keep the frames'), breathing; QC's gun torso turns (aim
  twist `vr_agrunt_aim_twist`, kick lean, reload turn) as the body block, the Pose Studio's torso keys on top.
- **Round 2** (ROUND22.md, "Armed humanoids in motion, round 2"): a held weapon's turn is never eased, only its hand's
  place (`vr_agarms_aim_line`), and a gun's shots leave its drawn muzzle along its drawn line (`vr_agrunt_shot_drawn`,
  QC asks `weapondrawnpose(p, 0, 21/22)`: the turn and shift of the drawn weapon off the server's, a listen server's own
  view); a two-handed melee weapon (not the chainsaw) is swung one-handed, the off hand let go and taken again round the
  shoulder (`vr_agrunt_swing_one_hand`, `vr_agarms_second_release`); punches eased into the blow (`vr_agrunt_punch_ease`); a chainsaw's thrust keeps both handles within reach (`vr_agrunt_saw_reach`); a
  weapon taken up turns from how it lay the short way (`vr_agrunt_lift_turn`). Each 1 by default, 0 before.
