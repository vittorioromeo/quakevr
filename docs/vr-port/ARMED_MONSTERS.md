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
vertices (ranges: never a hand's), the gun's line, its arm bones' names (the ragdoll rig's).

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
- Tests 31..35 (`QC/vr_armedgrunt_play_test.qc`); scenarios `agrunt_tug`, `agrunt_parry`, `agrunt_pickup`, `agrunt_guard`.

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
