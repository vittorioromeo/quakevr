# Armed monsters

A monster holding one of your weapons, drawn as the weapon lying about is, fired and swung with your weapons' own code,
disarmed, re-armed and dropping it on death (ROUND22.md, "Armed grunts" and the sections after it). The code knows
nothing of the monster that holds the weapon: what it needs is the monster's **row** in the armed monsters' table.
Rows now: the grunt (`monster_vr_grunt`, row 0), the enforcer (`monster_vr_enforcer`, row 1) and the knight
(`monster_vr_knight`, row 2: every weapon class, a sword by default; guns lying about taken up only with
`vr_agknight_ranged_pickup` 1 (`vr_am_takes_ranged`, `VR_AM_Takes`); the grunt's shipped pose keys, fitted (C++
`agpose` `shippedFrom`); a map's knights and death knights (`monster_vr_hknight`, row 3) are these rows by default:
`vr_agknight` 1, `vr_agdknight` 1 (since 2026-11-03), QC `VR_AKN_Replace`, in the grunts' scope `VR_AGR_InScope`, not
statues, a trigger's copies, MG3's infected or AD's variants; a sword alone, so no pack; `ROUND23.md`, "Knights as armed
humanoids", "Armed knights by default"). Their ranged pickups stay off for those: a gun comes only from a spawn's
weapon (the toolgun, the Rig Studio, a wave, a map's "weapon"), and one spawned holding a ranged weapon takes ranged
weapons up whatever the setting says (`.vr_ag_born_ranged`, set in `VR_AM_Spawn`, read by `VR_AM_Takes`). `VR_AM_GUN_KINDS` (2): the kinds the gun holders' loops mean (pose categories,
replacements); the toolgun lists the knights while their settings are on.

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

Its arms (posed and skinned, ROUND22.md "Armed humanoids' arms: posed and skinned"; the only arms since the
refactor's wave C1, ROUND23.md: the old rigid-piece arms, `vr_agarms 0`, are gone): `vr_am_anchor` / `_anchor_at` (a chest
vertex its poses are placed from; the engine row has the same index), `vr_am_sh_r` / `_sh_l` (its shoulders at rest),
`vr_am_arm` (shoulder to wrist), `vr_am_grip_r` / `_grip_l` and `vr_am_gun_fw` (its hands on its own gun at rest, the
gun's way): `vr_agarms_info` prints all of these for a loaded model (and its skin: arm vertices, hands, joints).

## Arms: posed by the weapon, IK, skinned

| Part | Where |
| --- | --- |
| Where each weapon is held (its entity frame is the hand's, as your controller's is) | QC `vr_armedgrunt_pose.qc`: `VR_AGP_ArmsGun` (long guns from the rest pose's hold, raised by `vr_agarms_shoulder`; one-handed and dual out from the shoulder), `VR_AGP_BladeHand` (melee: the tip on the blade's line, the knuckles leading), fists and free hands as points, `VR_AGP_RunBob`, the reach clamp; flags `VR_AGP_ANCHOR` (64), `VR_AGP_REACH` (128) |
| A weapon's own points (muzzle or tip, butt, second grip) in its frame | `weapondrawnpose(p, 0, 8..10)` (`view::weaponLocalGeometry`) |
| The arms' IK, the hands' grip frames, the weapons drawn on the hands | `vr_agarms.cpp` `solve` (`vr_hitreact.cpp` `relinkSkinned` calls it) |
| The skin (weights per vertex, once a model) | `vr_agarms.cpp` `skinOf`; the alias shader's `RigArm` (`vr_glsl.h`), rows from `kickInstance` |
| Grips: one per kind and hand for every weapon, a weapon's own on top | `vr_aggrip.cpp` (`global`, `offset`); VR Settings > Armed Grunts > Grip Editor (Edits: Every Weapon / This Weapon Only) |
| Grips set by hand: the Pose Studio (Grip Editor > Drag Mode, `vr_posestudio`) | `vr_posestudio.cpp` (the boxes, the drag, the panel); `relinkSkinned` calls `posestudio::adjust` before the solve (a dragged weapon at the controller) and `note` after it (`agarms::Placed`'s hands, elbows, shoulders, chest); `aggrip::setPreview` / `set` |
| Poses set by hand (Pose Studio stages 2-4): pose keys per kind, weapon class, pose and strike phase | `vr_agpose.cpp` (the keys: `vr_agrunt_poses`, the shipped table, copy between kinds, the dump); QC `vr_armedgrunt_pose.qc` `VR_AGK_*` (the classes, poses and off-hand modes, and the code's strides, are the
engine's `agpose::Class`/`Pose`/`OffMode` from one header both include, `Quake/vr/vr_agpose_shared.h`, with the
weapons' attack classes `VR_AG_CLASS_*`; read through `weapondrawnpose(world, code, 11..18)`, blended by the pose's weights, laid on the targets after the torso's turn; the blow's line moved with them in `VR_AGP_Strike`; the studio's hold `VR_AGK_Hold` through `weapondrawnpose(g, kind*100+class, 19)`); a pole past 0.8 weighed up in `solveArm`; frozen monsters can't be disarmed, tugged or struck |
| The off hand per pose key (a single gun): its grip offset on the gun and its hold (Auto / On the Gun / Free) | `agpose::GripPos`, `GripTurn`, `OffHand` (parts 8-10, missing from older text: zero, Auto), read by QC through `weapondrawnpose(world, code, 25..27)`; QC `VR_AGK_Keys` blends the offset and picks the deciding key's hold (Pickup's if set, else Aim's while aimed, else Run's if set while running, else Carry's), the gun branch of `VR_AGP_Pose` turns it into `VR_AGP_FREE` / `VR_AGP_TWO`, and the pose message's off-hand block (both flags set: `parseArmPose`) carries the offset and the free hand's LPos/LTurn; `agarms::Targets::gripOff` / `freeOff` laid on the second grip, the cup, the hanging hand (`vr_agarms.cpp`) |
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
guns"): each in one hand along the rifle line's pitch straight ahead, the left posed as the right is
(`VR_AGP_ArmsGun`); the second fires a frame behind the first (`VR_AG_AtkFrame`: shots and rounds in turn, streams together);
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

- The tug (always physical; its body: `vr_agrunt_tug_sway`, `_flail`, ragactive `staggerTugSway`): `VR_AG_TugRagBegin` (a sure-footed stagger, the hand holding its forearm through the
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
- Falls (`vr_agrunt_fall_keep`): a gentle one keeps the weapon on the ragdoll's hand (`VR_AG_FallViolent`: the impact
  rule, `VR_AG_FallImpact`: it lets go only as its body, having kept up `vr_agrunt_fall_impact_speed` for
  `vr_agrunt_fall_impact_time`, loses `vr_agrunt_fall_keep_impact` m/s within `vr_agrunt_fall_impact_window` on a floor or
  wall it flew into, held by your hands too; contact is the ragdoll strikes' own, `VR_RagImpact_Hit`; test
  `vr_agrunt_test 202`; the older height, speed and fling rules were retired 2026-11-02); down, a grab takes it at once and a blow over
  `VR_AG_DOWN_SWING` knocks it away.
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
- The weapon stops the shot (`vr_agrunt_shotdisarm_block`, default 1, replacing `_absorb`): a pellet or a nail whose
  first hit along its way is the weapon (its volume at size 1, crossing its line half a unit before the body:
  `VR_AGS_Blocks`) stops there (`VR_AGS_Stopped`: sparks, a tink; no damage, no blood), each pellet on its own; the hand
  first is the body's. Lightning and explosions are not stopped. Works with shot disarms off too.
- Test `vr_agrunt_test 50` (`QC/vr_armedgrunt_shot_test.qc`; its cases with the block off, as measured); scenario
  `agrunt_shootout`. Test `vr_agrunt_test 75` (`QC/vr_armedgrunt_downtug_test.qc`): weapon-only shots by held weapon
  and player weapon, block off and on.
- Grabbing a downed one's weapon: test `vr_agrunt_test 74` (same file): knocked down 4 ways, grabbed at 3 points while
  down and getting up, pulled 3 ways; never stuck. The engine's ragdoll grab leaves a fist on an armed weapon to the
  weapon's grab (`grabRagdoll`, as foegrab's `tryTake`).

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

- `vr_agrunt_repl` (Combat > Armed Grunts > Armed Humanoid Replacements; on by default since 2026-10-30, the page's other switches off): `monster_army` /
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

## AD's and Quoth's variants (ROUND25.md, "humrepl")

His words (2026-11-21): "replace the 'hardcoded' humanoid variants introduced by these expansions with the closest
corresponding armed humanoid version". `vr_agrunt_repl_variants` (1; with `vr_agrunt_repl` and AD's scope,
`vr_agrunt_repl_ad`; the crossbow knight with `vr_agknight`): AD's spawn function of each replaced variant (Quoth's maps
play by AD's rules, so Quoth's too) calls `VR_HUM_Replace` (`QC/vr_humrepl.qc`) after AD's own spawn exceptions, and the
variant becomes the armed row of its build holding its gun, with its health (the map's `health`, else AD's). It becomes
that row's monster (classname and spawn: the path AD's maps' plain grunts and knights already take), marked a
replacement (`.vr_agr`: trigger-spawn keys kept), `.vr_hum_variant` the variant; it rolls nothing. Its gun is not native:
the armed layer fires it (AD's guns as the player has them, `vr_ad_enemyguns.qc`).

| Variant (placed: AD, Quoth) | What it is | Becomes | Lost | Gained |
| --- | --- | --- | --- | --- |
| `monster_army_rocket` (51, 73) | AD's `mon_soldier` (our grunt's mesh), red jacket, rockets, backs off up close | grunt + Rocket Soldier's Launcher (`WID_ADROCKET`), 45 hp | its red skin (the recolour gives red), its back-off, its obituary | everything armed: disarms, tugs, parries, the gun shot out or taken, its pack, its ragdoll holding the gun |
| `monster_army_grenade` (25, 0) | the same, green jacket, grenades | grunt + Grenade Soldier's Launcher, 60 hp | green (explosives are red here) | as above |
| `monster_army_plasma` (39, 0) | the same, blue jacket, plasma bolts | grunt + Plasma Soldier's Rifle, 75 hp | | as above, blue |
| `monster_defender` (83, 117) | AD's `mon_enforcer`, shotgun, armour | enforcer + Defender's Shotgun, 100 hp | its own skin | as above (shells: no recolour) |
| `monster_eliminator` (57, 49) | `mon_enforcer`, plasma gun | enforcer + Centurion's Plasma Gun (the eliminator's own is gone: `WID_ADELIMINATOR`), 120 hp | | as above, blue |
| `monster_pyro` (13, 44) | `mon_pyro`: an enforcer's build scaled 1.35, armoured, flamethrower, burns | enforcer + Pyro's Flamethrower, 100 hp | its bulk and model | as above, orange. The enforcer, not the grunt: its build (AD's own rig for it is the enforcer's) and its armoured close-range role |
| `monster_fumigator` (11, 0) | the pyro's build, poison sprayer | enforcer + Fumigator's Poison Sprayer, 100 hp | | as above, green |
| `monster_dcrossbow` (387, 0) | AD's crossbow knight | knight + Crossbow Knight's Crossbow, 75 hp | its model and skin | as above (no recolour) |

Left as AD made them (phase 1; each a candidate the author may want, noted in ROUND25.md "humrepl"):

| Variant | Why not now |
| --- | --- |
| `monster_dknight` (475) / Quoth's `monster_death_knight` (95) | AD's own death knight (its model, its magic): the armed death knight with its sword (`VR_ADMELEE_DKNIGHT`) would fit, but it is a third of AD's knights: AD's balance changes most; his word first |
| `monster_dguard` (254) / `monster_death_guard`, `monster_deathguard` (Quoth 57, 13) | a sword soldier: a knight with the Death Guard's Sword would fit; with the death knights |
| `monster_dsergeant` (30), `monster_dfury` (69) | the flail and two fury swords on an armed knight: the flail's chain on an armed hand untested |
| `monster_hell_knight` 8192, `monster_hell_pointknight` (4) | the lightning point knight: a turret-like caster, 4 placed |
| `monster_zombiek` (435), `monster_dknightghost` (13) | undead (getting up again, a ghost): not what an armed humanoid is |
| `monster_justice` (17), `monster_judicator`, `monster_dprince` | bosses, oversized |
| `monster_centurion` (46) | a flyer on its hover board (no armed row flies) |
| `monster_jim` (52), `monster_seeker` (21), `monster_freddie` (11) | robots: no humanoid rig (Freddie's gun is his arm) |

Tests: `vr_ad_test 219` (`Misc/quakevr/humrepl_test.sh`: each variant its row, gun, health, look, counted once; a death
fires its target; off: AD's; looks by weapon; the client's looks), `vr_ad_test 198` (its rocket soldier armed now);
the maps' census `Misc/quakevr/humrepl_census.sh` (`vr_ad_test 220` on the 23 AD and 10 Quoth maps placing them, the
setting off and on: the same monster counts, none left).

## Audit 2: the variants left as AD made them (ROUND25.md, "adhumaudit"; audit only, nothing converted)

His words (2026-11-22): "it would be nice for those to become armed humanoid too, so that they can wield different
weapons and wear dynamic armors. Please do an audit first ... If there are AD variants that fit any of the current
poses/rigs quite well (requiring small tweaks, possibly), then they'd be good candidates."

Two ways to convert one. **Row swap**: it becomes an existing row on our model (phase 1's path, `VR_HUM_Replace`: a
table line, its health, its weapon; its AD look lost; about a quarter of a day for a batch). **Look kept**: AD's model as
a model of an existing kind, sharing the kind's pose keys, grips, armour fit and IK, with its own per-model data (hand
and weapon vertices, anchor, frames): new, since rows are one model a kind; the engine already hides each AD weapon's
vertices (`knownSwords` in `vr_monstermods.cpp`, today in death frames only; `armedRigs` is already keyed by model), and
an authored rig would be made on AD's mesh read in place (`make_rig2.py` reading AD's pak: a small addition; never a
modified AD file). About 2 days for the mechanism and the first model, half a day to a day each after.

Builds measured on the first stand frame without the weapon (`Misc/quakevr/adhum_mdl.py`: height; shoulder band 72-86%
up, width), placements from the maps' entity lumps in AD's and Quoth's paks (`Misc/quakevr/adhum_census.py`; Quoth's sample maps
from the phase 1 census). Our rows: grunt 50.6 tall, 23.6 wide; enforcer 58.9, 32.9; knight (`vrknight.mdl`) 49.2, 24.4;
death knight (`vrhknight.mdl`) 62.8, 33.6. Screens (`Misc/quakevr/adhumaudit_shots.sh`, `vr_ad_test 222`; Debug > Tests,
"Armed Humanoids: AD Model Beside Its Row"): `C:\OHWorkspace\qvr-kit\scratch\adhumaudit_<NN>_<name>.png` (ours, both
overlaid with AD's translucent, AD's, the overlay side-on) and `adhumaudit_all.png` (the 12 together).

| Variant (placed: AD; Quoth) | Model: verts, frames; build vs our row | Attacks, behaviour, hp | Fits | Tweaks | Lost / carried | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| `monster_dknight` (475 on 19 maps; Quoth `monster_death_knight` 95) | mon_dknight 911 (body 870 + sword 41), 244; 63.9 tall, 30.8 wide: the death knight's build (+2%, -8%) | 3 swings in turn (slice, smash, two-handed), 2 charge swings, a 6-shot fanned volley (`ad_hknight_shot`: the hell knight's own); 250 hp (id's hell knight's) | death knight (row 3) | row swap: none (his sword `VR_ADMELEE_DKNIGHT`, the row's spell is the same volley). Look kept: frames remapped (stand 0-11, run 106-111 is 6 frames, the row's run is 8: the layer needs a 6-step run; slice 112 the shove; a take from its deaths 211-), rig on his mesh | its look (row swap), its charge swings (the layer's own melee) / the volley, health, sword kept | **convert (small)**; a third of AD's knights, but hp and volley are a hell knight's: AD's balance barely moves |
| `monster_dguard` (254 on 10) | mon_dguard 585, 256; 51.4, 25.0: the knight's build (+4%, +2%) | 4 swings in turn (slice, charge swing, smash, two-handed); melee only; **30 hp** (short box, knight class) | knight (row 2) | row swap: the Death Guard's Sword (`VR_ADMELEE_DGUARD`), 30 hp. Look kept: frames remapped (run 109-114: 6 frames) | look / sword, health kept | **convert (small)** |
| `monster_death_guard`, `monster_deathguard` (Quoth's; 5 in Quoth's paks; 57 + 13 with the samples) | mon_dguardq 176, 166: **id's hell knight's frames, frame for frame**; 60.9, 31.9 (-3%, -5%) | overhead swing, charge B, a slow heavy fireball (magicb, sword pointed); 125 hp. Not AD's death guard: a hell knight reskin | death knight (row 3) | row swap: the hell knight's sword, 125 hp, its fireball as a variant spell (a short QC frame run on our magicb frames calling AD's launcher; `vr_am_spell` is one a row today: a per-variant pick). Look kept: no frame remap at all | look (row swap) / all carried | **convert (small)** |
| `monster_death_lord` (missed by phase 1; Quoth: 1) | mon_dlord 245, 166: id's hell knight's frames; 63.4, 32.4 | overhead swing, a homing spike ball, two-speed run; 400 hp | death knight (row 3) | as Quoth's death guard (its spike ball the variant spell) | look / all carried but the two-speed run | **convert (small)**, with Quoth's death guard |
| `monster_dfury` (69 on 15) | mon_dfury 1376, 255; **69.1** tall (+10%), 32.6 | two swords: dual slice, a leaping lunge, 24-frame charge, left/right swings, smash; magic A a 5-spike volley, magic B 3 rockets in an arc; 400 hp | death knight (row 3), dual wielding | row swap: two Death Fury's Swords (one-handed, pairs: dual wielding is there), its volley as the spell; its rocket arc a second spell (new: a choice of two) | its height (scale: the poses are fitted to a row's shoulders), the lunge (unless the leap `VR_Leap` is offered to armed humanoids), the rocket arc unless done | **convert with work** (about a day: the second spell, the lunge, a dual-sword stance check) |
| `monster_dsergeant` (30 on 10) | mon_dsergeant 1265 (the death knight's 870-vertex body + a 395-vertex flail), 255; 63.9, 31.1 | flail swings (chained head), 2 flail charges, a homing missile; 400 hp | death knight (row 3) | the flail: armed humanoids hold every AD melee weapon but it (`VR_AG_CODE_ADMELEE`: its head on its chain is only a hand's, `vr_box3d_flail.cpp`) | its flail (the point of it) unless a monster-held flail is built (1-2 days); a mace instead loses what it is | **not now** (30 placed; the flail first) |
| `monster_hell_pointknight` (4 on 1) | mon_hknight 250, 166 (id's frames) | stands and streams lightning at its target from spawn: a scripted turret | death knight | none useful | everything that makes it | **not worth it** |
| `monster_zombiek` (435 on 19) | mon_zombiek 161, 253; 50.5, **18.2** wide (-25%): a thin knight; its blade a loose piece of its rig, no weapon range | 2 stationary swings, a leap; 60 hp fixed, always in pain, falls and gets up, dies only gibbed (AD's zombie class) | knight (row 2) | its get-ups and gib-only death against the layer's disarms, tugs, falls and drops | the zombie (what it is) | **not worth it** (an undead that never dies; its rig keeps no weapon piece) |
| `monster_dknightghost` (13 on 1) | the death knight's, translucent | scenery: no damage, no count, walks a path | none | | | **not applicable** (not a monster) |
| `monster_justice` / `monster_baron` (17 on 1) | mon_justice 1008, 245; **75.6** tall (+20%), 42.0 wide (+25%) | sword, a jump attack, range magic; 600 hp: a mini-boss | death knight (row 3) | scale 1.2 (the poses, the grips, the armour), its jump, its magic | its size and jump | **not worth it now** (one map; the size is its role) |
| `monster_judicator` (1), `monster_dprince` (0) | mon_judge 1016: 102 tall (1.6x); the prince the death knight's mesh | bosses, scripted waves | | | | **not worth it** (bosses) |
| `monster_centurion` (46 on 5) | mon_centurion 284, 79; 57.1 standing on its board, 31.4: an enforcer's build | flies on a hover board, plasma; 120 hp | enforcer (row 1) | no armed row flies (the board is a flyer's movement, its own frames) | its flight | **not worth it** |
| `monster_skullwiz` (225 on 14; missed by phase 1) | mon_skullwiz 632, 158; 58.8, a robe to the floor (no legs to pose) | casts skulls, teleports when close; 120 hp | none (the grunt's height) | | | **not worth it** (a robed caster, no weapon) |
| `monster_jim`/`_bob` (52; 15), `_seeker`/`_juggernaut` (21), `_freddie`/`_edie` (11; 7) | robots | | | | | **not worth it** (no humanoid rig) |

Notes:
- Phase 1's grouping of `monster_dguard` with Quoth's `monster_death_guard` was wrong: AD's death guard is a 30 hp knight-sized
  swordsman; Quoth's is a 125 hp hell knight reskin with a fireball (and plays as `ad_monster_death_guard`, mon_dguardq).
- The best fits for keeping AD's look are the Quoth hell knight reskins (death guard, death lord, AD's point knight's mesh):
  id's hell knight's frames frame for frame, builds within 5%: the death knight kind's poses, grips and frame numbers as
  they are; only their meshes' vertices (hand, weapon, anchor) and a rig. Next the death knight family (dknight, sergeant,
  prince: one 870-vertex body, close to our death knight) and the death guard (the knight's build): a frame remap each.
- Everything armed is gained by each converted one: weapons from the pool or a map's `weapon`, disarms, tugs, parries, the
  weapon shot out or taken, the worn pack, dynamic armour (`VR_AgArmor_Give`: the row's armour fit), its ragdoll holding the
  weapon, the recolours by weapon.
- AD's balance: the death knight's hp and volley are a hell knight's (unchanged by a row swap); the death guard's 30 hp kept;
  what changes is their melee (the layer's swings, parries and disarms instead of AD's attack cycles and charges).

Proposed phase 2 (in order): (1) row swaps, small: `monster_dknight` / `monster_death_knight` (armed death knight, his
sword), `monster_dguard` (armed knight, the Death Guard's Sword, 30 hp), Quoth's death guard and death lord (armed death
knight, the hell knight's sword, a per-variant spell: their fireball, their spike ball), under phase 1's settings
(`vr_agrunt_repl_variants`, with `vr_agknight` / `vr_agdknight`), tests 219 and 220 extended; (2) the death fury, with
work (a second spell, the lunge); (3) if he wants AD's looks kept: "look kept" for the Quoth reskins first (no frame
remap), then mon_dknight and mon_dguard. The sergeant waits for a monster-held flail.

## Recolours by weapon (ROUND25.md, "humrepl")

His words: "cheap ways of improving/changing the appearance of the armed humanoids to make it obvious to the player
what they're fighting against ... run-time conditional hue/tint recolors on their textures, and also ... on the
backpacks". Every armed humanoid, however armed (a variant, a replacement's roll, the toolgun, a wave, a map's
`weapon`), gets a look from the weapon it holds as it is armed (`VR_HUM_Look`, called by `VR_AM_Spawn` and
`VR_AGR_Resolve`): `.vr_hum_look` = its row x 16 + the weapon's category (`VR_HUM_CAT_*`: 1 explosive, 2 energy, 3 flame,
4 poison, 5 shells, 6 nails, 7 melee, 8 its own gun; a gun beside a blade: the gun's; the enforcer's own laser
rifle is energy, fbmisc1123). It keeps it (a disarmed rocket
grunt stays red). Its worn pack takes its wearer's (the engine: `.vr_wp_wearer`, plus 4096).

- The engine (`Quake/vr/vr_humlook.cpp`): each entity's look sent where it changes (`QVR_SVC_HUMLOOK`, reliable; with the
  spawn state), the table `quakevr/humlook.txt` and the masks `quakevr/humlook_masks.txt` (read at first use;
  `vr_humlook_reload`), the alias instance's four `Recolor` rows (`instanceRows`, `VR_AliasInstance`, drawn instances only).
- Which texels (phase 2, ROUND25.md "humrepl2"; his verdict on phase 1: "The recolors look a bit unnatural and also affect
  the skin/faces of the enemies": a hue band can't tell brown cloth from brown flesh). A mask per skin, 2 bits a Quake
  texel (left, clothes, pack), made as the skin is first drawn from its 8-bit palette indices (read from the model's
  file: `progs/vrgrunt.mdl#rig2` reads `vrgrunt.mdl`), in one storage buffer the shader reads (`HumMasks`, SSBO 17;
  rewritten when edited). A texel's class: painted by hand if it is, else never if its row is flesh (`flesh 7`) or a
  quarter-plus of its 5x5 neighbours are (`fleshnear 0.35 2`: the grunt's arms and face have tan highlights on the
  uniform's row 10), else its row's class for its key (`rows grunt clothes 1 10`, enforcer 1 3 10, knight 1 10, dknight 1,
  the worn pack's canvas `rows pack pack 1`). The rows (the skins' histograms): uniforms and leather 1 and 10 (browns,
  tans), the enforcer's trousers 3, flesh 7, blood 4, knights' plates 10. Where rows are shared, our painted texels: the
  enforcer's baked backpack (`paint progs/vrenforcer.mdl ... pack`, its two pieces' clothes-row texels). A skin's own rows
  (`skinrows`) override its key's. AD's own models (`mon_*.mdl`) are never dyed: every armed humanoid, AD's variants
  too, draws our models (`vr_am_model`: vrgrunt, vrenforcer, vrknight, vrhknight; the masks a run of AD's variants makes:
  those three, the worn pack, h_guard). Checked (ROUND25 "backlog1122b"): were they ever drawn, mon_enforcer's and
  mon_pyro's masks keep their flesh (bare shoulders on row 7), mon_soldier's would dye its face's cheeks (rows 1 and 10:
  a `paint ... never` over its face then).
- The dye (`vr_glsl_alias.h` `QVR_ALIAS_FS_RECOLOR`, after the skin is read, before the light): a clothes texel by the
  clothes' row, a pack texel by the pack's (the enforcer's baked pack too), each keeping its own luminance in the row's
  colour (`HumDye`: a red stays red, never pink; clamped, not desaturated), or with `tint` its own colour times the
  colour, by `strength`, its lightness times `light`. On whatever the model draws (Quake's skin, a replacement, a retro
  texture: the mask at the Quake skin's size): no mod file is changed or copied. One branch on a per-instance value.
- The table: `<type> <category> <part> r g b strength light tint` (`grunt enforcer knight dknight`; `explosive energy flame
  poison`, any other category name or number too; `clothes` the body, `pack` the backpack). Shipped (subtler than phase 1):
  explosives red, energy blue, flame orange, poison green, strength 0.58-0.62 (knights' plates 0.75 of it, packs +0.1);
  shells, nails, melee and own guns have no row (as they were).
- `vr_humlook` (1) turns the drawing off; `vr_humlook_info [entity]` prints the table, the masks (their clothes and pack
  texels), the client's looks and the nearest armed humanoid's look and row; `vr_humlook_known` (read-only) the looks the
  client knows; `vr_humlook_file`, `vr_humlook_masks_file` the files (in quakevr/).
- The Texture Studio (Debug > Tools > Texture Studio; `vr_humstudio.cpp`, his ask: "An in-game tool to tweak the
  recoloring and masks would also be good"): Line Up (five of the type ahead, one per category: QC `VR_HUM_LineupKind`),
  Type, Weapon Category, Part; Red, Green, Blue, Strength, Lightness, Tint (`vr_humstudio_*`, laid into the table as they
  move); Show the Masks (`vr_humstudio_overlay`: clothes green, pack blue, the rest dark red); the 16 palette rows of the
  type's skin, each cycled none / clothes / pack (the type's rows, or with This Skin's Own Rows that skin's); Paint with the
  menu closed (`vr_humstudio_paint`: VR either hand's ray, its trigger paints `vr_humstudio_class` (clothes, pack, never,
  erase) with `vr_humstudio_brush`, its grip erases, B/Y undoes; flat the crosshair, attack paints) on the posed model's
  skin texel under the ray (the same strokes, only the ray differs); Undo (24 steps), Clear This Skin's Painting, Save
  (both files), Reload, Check the Masks (`vr_humstudio_check [masks]`: per mask its texels, any on the flesh's or blood's
  rows, a PASS/FAIL line).
- The pick (studio2, his words: "sometimes when I want to paint an area something else get painted ... hands, forearms,
  or parts of the enemies next to the backpacks"; "the backpack is intercepting the correct target"): the ray meets the
  model as drawn (`modelcollide::drawnTriangles`: a live one's on its authored rig with its arms solved, not its .mdl
  frame, which put hands and forearms elsewhere), only what `vr_humstudio_target` names (0 the part picked: Clothes the
  bodies, packs passed through; 1 bodies, 2 packs, 3 either), its texel from the place on the triangle (its corners'
  skin coordinates, in the .mdl's order). The brush (`vr_humstudio_brush`, cm on the surface, 0 one texel): every texel
  within it of the point hit, on whichever triangles carry it (no wrapping across the skin's halves). The cursor
  (`vr_humstudio_cursor`): the brush's texels drawn yellow (mask value 3, the shader's `rk == 3u`) wherever the skin
  shows them, and the skin's flat view in the top right (`studioDraw2D`, the 2D pass: masks tinted, brush yellow).
  Lineup options (QC `VR_HUM_LineupKind`'s bits): no backpacks (`vr_humstudio_nopack`, default 1), no armour (1), no
  weapons drawn (0), a backpack alone on a stand (`vr_humstudio_packstand` 1, its own look), turned
  (`vr_humstudio_turn`, `VR_HUM_LineupTurn`); Hold Still is `vr_agrunt_freeze`. No T/A pose: the stock models have no
  such frame (Hold Still's idle pose instead).
- Tests: `Misc/quakevr/humstudio2_test.sh` (the shipped files read and written back the same; `vr_humstudio_picktest`:
  each type, packs on, rays at the hands, forearm, back of the head and back beside the pack paint the texel there, a
  stroke and its undo; "humstudio2_test: PASS"), `Misc/quakevr/humrepl2_test.sh` (each type lined up, a stroke and its undo, `vr_humstudio_check 5`: no mask
  texel on the flesh or blood rows; "humrepl2_test: PASS"). Screens: `Misc/quakevr/humrepl_shots.sh [out] [overlay]`
  (`vr_ad_test 221`: each type's row before and after, or the masks shown).

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
  of and the elbow kept out of; the elbow's swing rate-limited the way round outside the body
  (`vr_agarms_elbow_rate`); the second grip's hysteresis and ease; a pickup's reach eased in by distance; goal jumps
  eased (their switches retired in wave C1: always on).
- **The shoulders** (ROUND22.md, "Armed humanoids' shoulders"): the skin from the real joint (always, since wave C1:
  the deltoid or pauldron on the arm, no forearm turn on the upper arm's top half, the blend about the joint), a
  clavicle per side (`vr_agarms_clavicle`, default 0.6, the one setting for the authored rigs too since wave C1;
  `_most`: the joint shrugs and comes forward with the arm, the shoulder's top
  with it; rows 22..25 of `RigArm`), the shoulder's cone and twist in the IK (`vr_agarms_shoulder_cone`, 0 turns them off,
  `_shoulder_twist`), the upper arm's skin kept out of the torso (always; the held wrist's clearance one constant,
  `armClearHeldMost`), the enforcer's pistols
  one-handed (`vr_agrunt_enforcer_one_hand`), its stand frames as it fires (`vr_agrunt_atk_stand`, the row's
  `vr_am_atk_stand`), armed ragdolls drawn with the arm skin's blend (`vr_ragdoll_arm_skin`). The finder's metrics
  armpen, swing, twist, stretch measure them; `vr_agarms_cost` the solve's time.
- **The upper body** (`vr_agrunt_upper`): the chest upright on the frames' hips (their own chest motion in shots, reloads,
  pains and runs taken out; a pickup's crouch, falls and deaths keep the frames'), breathing; QC's gun torso turns (aim
  twist `vr_agrunt_aim_twist`, kick lean, reload turn) as the body block, the Pose Studio's torso keys on top.
- **Round 2** (ROUND22.md, "Armed humanoids in motion, round 2"): a held weapon's turn is never eased, only its hand's
  place (always since wave C1), and a gun's shots leave its drawn muzzle along its drawn line (`vr_agrunt_shot_drawn`,
  QC asks `weapondrawnpose(p, 0, 21/22)`: the turn and shift of the drawn weapon off the server's, a listen server's own
  view); a two-handed melee weapon (not the chainsaw) is swung one-handed, the off hand let go and taken again round the
  shoulder (the second grip's release always); punches eased into the blow; a chainsaw's thrust keeps both handles
  within reach; a weapon taken up turns from how it lay the short way. Their A/B switches (`vr_agrunt_swing_one_hand`,
  `vr_agrunt_punch_ease`, `vr_agrunt_saw_reach`, `vr_agrunt_lift_turn`, with `vr_agrunt_run_key_stance`) retired
  2026-11-07, as the vr_agarms_* ones in wave C1.

## Open items moved from BACKLOG.md (2026-11-12)

BACKLOG.md keeps a one-line pointer to each; remove an item here when it ships.

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
