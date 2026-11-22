# Pole weapons: lances, spears, staffs (design; dwellweap, 2026-11-22)

His notes (vrtesthall_2026-11-22_15-38-38, 15-40-11): make the Crystal Lance feel very good, then extend the same system to
plain spears that impale, and staffs. A pole weapon is **a pole + an optional head**: the Lance is a pole with an energy
tip (a gun), a spear a pole with a blade tip (melee only), a staff a pole with two blunt ends.

## What a pole weapon is made of (one weapons.txt slot each)

| Part | Where it lives now | Lance | Spear (planned) |
| --- | --- | --- | --- |
| Pole grip, other hand anywhere, sliding | `vr_twohand.cpp` `FREE_POLE`, `anygrip_mode 3` | yes | same |
| Weight | `w_mass`, `w_com`, `w_len` | 7.2 kg (dwellweap: +20%) | lighter, balance nearer the middle |
| Blow along the pole | `bl_near` .. tip (the blade line), QC `DW_LanceMelee` (the axe's damage) | yes | a generic `VR_PoleMelee` (below) |
| Reverse grip (head down) | `rev 1` (vr_view_weapon.cpp reverseTurn), QC `VR_RevGrip_Flip` | `rev 1`, flip allowed (B: no magazine to eject) | same |
| Shot from the tip | `muzzle_av` the tip, `shot_pitch` along the pole | the crystal beam | none |
| Feed | QC `DW_LanceFedFromPouch`: no magazine, cells from the pouch | yes | none |
| Flat play | `vr_flat_pole_*` spear pose (vr_flatplay.cpp polePose) | yes | same |

## The parts still to build (in this order)

1. **The main hand slides too** (his item 4: "the whole shaft is a grip"). Today the held weapon's origin is the main
   hand's grip point, so only the other hand slides. Plan: a per-hand `poleAlong` offset (units along the blade line from the
   slot's grip point), kept in `twohand` beside the off hand's pole state; `vr_view_weapon.cpp` moves the drawn weapon by
   `-poleAlong * bladeDir` in the hand's frame and the QC muzzle/blade line read the drawn pose already (`VRGetMuzzlePos`).
   The grip slides while the hand's grip is half-closed (the analog grip under 0.6: a loose fist slides, a closed one holds),
   clamped between `bl_near` and the head's start. Same code for both hands (the off hand's slide is the existing one).
2. **The cord** (his item 2). Reuse `vr_adtank.cpp`'s carried hose (Verlet `vr_verlet.cpp`, `coil::Cord`, `gfx::drawTube`,
   `pushOutOfTorso`): a new `Hose` kind with one end at the pole's butt (a model point, like `adguns::hoseInlet`), the
   other at the ammo pouch (`body` pouch anchor) while the player has cells (`STAT_CELLS > 0`), else free (a stub, as
   `stepStub` steps the cut hose: one pinned end, falling and swinging). Length about 1.6 m, slack; it unplugs with a
   short pulse when cells run out and plugs back when cells are picked up. Holstered: hidden.
3. **Thrust**: the blade line's stab detection (the dagger's) with the pole's reach: a thrust lands with the head only
   (tip zone of the blade line), damage by the thrust's speed along the pole; a spear's head may **impale** (the
   existing embed system, `vr_embedhold.qc`: a blade stuck in a body, its handle held; for a spear the monster pinned
   until pulled out or the spear let go).
4. **A plain spear** (falls out of 1-3): a slot `progs/v_spear.mdl#vr` (our own model: a recipe of a pole and a leaf
   blade), `WID_SWORD` with a spear kind flag (as the dagger is a sword kind), `anygrip_mode 3`, `rev 1`, no muzzle; QC
   melee by `VR_PoleMelee` (the Lance's blow generalised: damage key per kind, the blade's sounds). Not built: it needs
   1 and 3 to feel right.
5. **Staff**: both ends blunt, the blade line both ways (a second blow line from the butt), club damage. Later.

## Built so far (dwellweap)

- The Lance's beam is drawn (it was ended the frame it began because its model is not `v_light.mdl`: `VR_BeamGone`),
  violet sparks where it enters and leaves each body and where it stops, a heavy single shot's kick (6 cm, 5 deg,
  0.35 s), its crystal head glowing violet after each shot (gunfx barrel heat, `barrelHeat`'s colour for the Lance).
- 20% heavier (`w_mass` 7.2). No magazine (cells from the pouch). `rev 1` and the eject flip allowed for the Lance
  (the flip itself not yet proven by a test: the mock's B press did not flip it).
