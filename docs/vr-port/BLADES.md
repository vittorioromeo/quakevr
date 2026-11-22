# Blades: one drawn shape for every system

His words (2026-11-21): "Cuts should definitely follow the drawn blade shape. Why do we have multiple systems? Isn't the
drawn blade shape enough for everything?" It is: since bladeone (ROUND25.md, 2026-11-22) every QC system that asks where a
blade is reads the shape drawn in Edit Blade Shape (vr_bladeedit.cpp; keys `bl_*` in quakevr/data/weapons.txt,
vr_weapons.inc "The blade's shape"; QC reads them through `weapondrawnpose 7`, `admeleehead`, `admeleebend`).

## The drawn shape

- **The line**: from the holding hand (or its own Line Start, `bl_org_*`) to the muzzle (or its own Blade Tip,
  `bl_tip_*`); the near end `bl_near` of it behind the hand (a pommel, a handle's end, a butt). QC `VR_Melee_Line`
  (vr_melee_params.qc): `vr_mline_grip`, `_far`, `_near`.
- **The curve**: `bl_spine` and five points (`bl_s1..5_*`), else the model's own (AD's curved swords), else straight.
  `VR_Melee_At(f)`: share `f` of the line on the curve.
- **The Blade Start**: `bl_from` (with its bend `bl_from_off/_side`): where the blade proper begins, a share of the line.
  A weapon with none (-1) takes its kind's, `VR_BLADE_FROM_*` (sword 0.27, axe 0.75, club 0.3, chainsaw 0.55, crowbar 0.2,
  gun 0.5): its first striking point, where the editor draws its purple Blade Start "(kind's)".
- **Width** `bl_width` (the swept points' radius) and a **head box** `bl_head*` (hammers, maces).

## The functions (vr_melee_params.qc, after VR_Melee_Layout)

| Function | Answers |
|---|---|
| `VR_Blade_From(thing, own)` | where the blade proper starts: his `bl_from` exactly, else the kind's. No floor, no per-system start |
| `VR_Blade_KindFrom(thing)` | the kind's start (the table above; a fist's 1: its knuckles) |
| `VR_Blade_Along(grip, far, at)` | a point's place along the line (0 the grip, 1 the tip, below 0 the pommel) |
| `VR_Blade_OnEdge(thing, own, f, slack)` | whether share `f` is on the blade proper |
| `VR_Blade_EdgePoint(thing, at)` | the blade proper's nearest point to `at`, on its curve (share into `vr_blade_f`) |
| `VR_Melee_PartAt(thing, f, own)` | the part's name, blunt below the Blade Start |

What stays per system is about motion, never shape: a slash across the edge, not a stab along it (`vr_decap_slash_angle`);
a pommel or hilt strike (the blow's kind); the speeds (`vr_decap_speed`, `vr_embed_*_speed`); a stab by the tip.

## Audit (2026-11-22): every place that decides where a blade is

| System | Where | Geometry before bladeone | Now |
|---|---|---|---|
| Melee striking points | vr_melee_params.qc `VR_Melee_Layout`, `_LayoutFrom`, `VR_Melee_At` | drawn line, curve, Blade From; per-kind point shares as literals | the same; the kind's first striking point is `VR_BLADE_FROM_*` |
| Melee sweep and contact | vr_melee.qc `VR_Melee_Sweep`, ribs | drawn: points' swept lines, `bl_width`, head box | unchanged |
| Which swept line counts first | vr_melee.qc `VR_Melee_RibScore` | fixed 0.4 of the line for every weapon | `VR_Blade_From` |
| Pommel touch waits for the blade | vr_melee.qc `VR_Melee_Sweep` (pommel wait) | fixed 0.4 | `VR_Blade_From` |
| Stab or swing | vr_melee.qc `VR_Melee_DecideOn` | motion (along the line) and a point in the line's outer half (`f >= 0.5`) | kept (motion; see below) |
| Part struck, blunt or sharp | `VR_Melee_PartAt` (corpse strikes, a weapon carried off its handle) | fixed: sword 0.2, axe 0.6, chainsaw 0.45 | `VR_Blade_From` |
| Corpse strike | `VR_Corpse_StrikeAlong` | traces along the drawn line's ends | unchanged; its part from `VR_Melee_PartAt` |
| Beheading (live, corpse) | vr_decap.qc `VR_Decap_Slash` | `VR_Decap_BladeFrom`: sword 0.4 (a sword's own never nearer), axe 0.6 or its own; chainsaw `VR_SAW_BAR_START` 0.55 | `VR_Blade_From`; slash angle and speed kept |
| Limb cuts | vr_limbs.qc via `VR_Decap_Slash` | as beheading | as beheading |
| Cut limb sticks to the blade | vr_skewer.qc `VR_Skewer_CutOn` | straight line from `VR_Decap_BladeFrom` | `VR_Blade_EdgePoint` (on the curve) |
| Hose cut | vr_ad_enemyguns.qc `VR_HoseCut_Melee` | `VR_SAW_BAR_START` or `VR_Decap_BladeFrom` | `VR_Blade_From` |
| Chainsaw's running cut | vr_chainsaw.qc `VR_Chainsaw_Cut` | raw hand to muzzle, 0.55 of it | the drawn line (`VR_Melee_Line`) and `VR_Blade_From` |
| Embedding by a stab | vr_embedhold.qc `VR_Embed_StabWhy` | the drawn line; tip within `vr_embed_stab_tip` % of it | kept (a stab by the tip: motion) |
| Embedding by the edge, a thrown blade's contact, a thrown axe's beheading | engine vr_axestick.cpp `bladeFromKeys` | `bl_from`, else a guessed 0.25 | point blades (`embed` 1, a sword's kind): `bl_from`, else `blade::swordKindFrom` (vr_blade.hpp, = QC `VR_BLADE_FROM_SWORD`); edge blades use their drawn `em_*` edges |
| Reversed grip's guard | engine vr_view_weapon.cpp | `bl_from`, else 0.15 | open: where the guard is, not where the edge starts (his reversed-grip offsets are tuned on it) |
| Two-handed grip on a blade | engine vr_weapons.cpp `bladeFrom(Hotspot)`, vr_twohand.cpp | the blade hotspot's own range (at least 0.3) | not blade geometry: his hotspots |
| Blade on blade, parries | vr_armedgrunt_parry.qc | the grunt's weapon line, `vr_agrunt_blade_hand_zone` (m from the hand) | kept (a hand zone, not a shape) |

The engine has no weapon kind; its only stand-in is `embed` (1 by the point: a sword's kind; 2 by the edge, drawn edges of
its own), so vr_axestick.cpp takes the sword's start from one engine constant kept equal to QC's. Open: vr_view_weapon.cpp's
0.15 (the guard, a grip rule); the stab test's "outer half" (`f >= 0.5`), a fixed share: tying it to the blade proper would
retune melee stabs.

A blade with its own start and few striking points (an axe's: two) has them laid evenly from its start to its tip, as
many as the layout holds (`VR_Melee_LayoutFrom`): two points at the Super Axe's 0.05 and its tip swept the straight chord,
not the crescent bowed 0.38 of the line off it.

## His shapes against their models (`vr_blade_dump fit`, 2026-11-22)

- **Super Axe** (fit 25; the line 8.8 units): the handle's vertices hug the line at 0.0 .. 0.1 (25, 32 of them, +0.6 ..
  +2.7 off); the crescent's from 0.2 (17, then 38 .. 55 a tenth to 0.8). His curve runs +1.6 at 0.1, +2.8 at 0.2, most
  +3.4 at 0.4, +1.3 at 0.9, its nearest vertex 0.4 .. 2.3 units away. His Blade From 0.05 lies on the handle: the edge
  cuts from there.
- **Dagger** (fit 52; the line 10.2 units): the guard and grip at 0.0 .. 0.2 (72, 60, 6 vertices, -2.8 .. +2.3 off);
  past 0.2 hardly any vertex within the slice (the blade's thin vertices 0.4 .. 3.1 units off the straight line, no
  curve of its own). His Blade From 0.05 lies inside the guard: the guard cuts.

## The check

`vr_melee_sweep_test 7` (`bladeone: N passed, M failed`): every bladed weapon (the knight's sword, the dagger, the axe,
the Super Axe, the chainsaw, six of AD's swords) given in turn; its own Blade From kept exactly, the melee's first
striking point at the start, a point on the edge sharp and cut (decap), a point between the hand and the start blunt and
refused, a cut limb skewered where it was cut on the edge, at the start off it.
