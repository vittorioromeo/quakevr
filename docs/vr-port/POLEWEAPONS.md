# Pole weapons: lances, spears, staffs (design; dwellweap, 2026-11-22)

His notes (vrtesthall_2026-11-22_15-38-38, 15-40-11): make the Crystal Lance feel very good, then extend the same system to
plain spears that impale, and staffs. A pole weapon is **a pole + an optional head**: the Lance is a pole with an energy
tip (a gun), a spear a pole with a blade tip (melee only), a staff a pole with two blunt ends.

## What a pole weapon is made of (one weapons.txt slot each)

| Part | Where it lives now | Lance | Spear (planned) |
| --- | --- | --- | --- |
| Pole grip, other hand anywhere, sliding | `vr_twohand.cpp` `FREE_POLE`, `anygrip_mode 3` | yes | same |
| Weapon hand slides too | `vr_twohand.cpp` `poleShift`, `vr_pole_slide`, `vr_pole_slide_grip` | yes | same |
| Cord to the pouch | `vr_cellcord.cpp` (the laser cannon's cell cord), `vr_cellcord_lance` | yes | none |
| Thrust | QC `DW_LanceMelee` on a stab (`VR_MKIND_STAB`), `vr_dw_lance_thrust/_poke/_head` | yes | `VR_PoleMelee` |
| Weight | `w_mass`, `w_com`, `w_len` | 7.2 kg (dwellweap: +20%) | lighter, balance nearer the middle |
| Blow along the pole | `bl_near` .. tip (the blade line), QC `DW_LanceMelee` (the axe's damage) | yes | a generic `VR_PoleMelee` (below) |
| Reverse grip (head down) | `rev 1` (vr_view_weapon.cpp reverseTurn), QC `VR_RevGrip_Flip` | `rev 1`, flip allowed (B: no magazine to eject) | same |
| Shot from the tip | `muzzle_av` the tip, `shot_pitch` along the pole | the crystal beam | none |
| Feed | QC `DW_LanceFedFromPouch`: no magazine, cells from the pouch | yes | none |
| Flat play | `vr_flat_pole_*` spear pose (vr_flatplay.cpp polePose) | yes | same |

## The parts still to build (in this order)

1. **Impale** (a spear's, not the Lance's: an energy tip): the embed system (`vr_embedhold.qc` `VR_Embed_Blow`, the
   slot's `embed` key AXESTICK_POINT) already takes a stab into a body for any weapon so marked; for a spear the monster
   pinned until pulled out or the spear let go. Not tried on the Lance (a gun stuck in a body: its cord, its shots).
2. **The weapon hand sliding alone** (one hand on the pole, its grip loose): the pole through the fist by its inertia.
   Not built: today the weapon hand slides only against the other hand's closed grip.
3. **A plain spear** (falls out of 1-3): a slot `progs/v_spear.mdl#vr` (our own model: a recipe of a pole and a leaf
   blade), `WID_SWORD` with a spear kind flag (as the dagger is a sword kind), `anygrip_mode 3`, `rev 1`, no muzzle; QC
   melee by `VR_PoleMelee` (the Lance's blow generalised: damage key per kind, the blade's sounds). Not built: it needs
   1 and 3 to feel right.
4. **Staff**: both ends blunt, the blade line both ways (a second blow line from the butt), club damage. Later.

## Built so far (dwellweap)

- The Lance's beam is drawn (it was ended the frame it began because its model is not `v_light.mdl`: `VR_BeamGone`),
  violet sparks where it enters and leaves each body and where it stops, a heavy single shot's kick (6 cm, 5 deg,
  0.35 s), its crystal head glowing violet after each shot (gunfx barrel heat, `barrelHeat`'s colour for the Lance).
- 20% heavier (`w_mass` 7.2). No magazine (cells from the pouch). `rev 1` and the eject flip allowed for the Lance.

## Built in dwellweap2

- **The cord**: the laser cannon's cell cord (`vr_cellcord.cpp`, a coiled cord from the weapon's lowest point into a
  cell standing in the ammo pouch, loose and swinging with no cells, plugging back in) with a Lance kind: from the butt
  of its pole (the model's lowest point; `vr_cellcord_lance_x/y/z` move it), leaving along the pole; QC
  `VR_Reload_PlayerFrame` makes the pouch show cells while the Lance is held (`vr_cellcord_lance`, Weapons > Reloading
  > Cell Cords). Needs Reloading Immersive (the pouch), the default. Turns with the grip's flip (it is a model point).
- **The weapon hand slides** (`vr_twohand.cpp` `poleShift`): with the other hand closed on the pole (analog grip at or
  over `vr_pole_slide_grip`, 0.6) and the weapon hand's loose (under it; a button-only grip never), the other hand keeps
  its place on the pole and the weapon hand slides: `PoleSlide::along` (units from the handle towards the tip, between
  the butt less half a fist and 0.45 of the handle-to-tip line) changes by as much as the other hand's place would have.
  The view draws the weapon moved back along the pole by `along` (`vr_view_weapon.cpp`, after the reverse grip's move),
  so its muzzle, blade line and cord follow. Kept while the hand holds the same weapon.
- **The pole's axis** is now the line from the holding hand to the drawn tip (`beginFree`): `bladeDirection` (the
  two-handed aim's way) lay some 70 degrees off the Lance's pole, so the other hand's place slid off the pole.
- **Thrust** (QC `DW_LanceMelee`): a blow read as a stab (`VR_Melee_Decide`: the far end driven along the line) landing
  at or past `vr_dw_lance_head` (0.55) of the hand-to-tip line: the head, times `vr_dw_lance_thrust` (1.5); short of
  it a poke, times `vr_dw_lance_poke` (0.4). Developer print "dwlance: a thrust, the head (...)".
- **The B flip** works in the mock (`dwell_lance_test.sh` section 3: B held 10 frames; dwellweap's 2-frame press never
  reached a player frame).

## Built in lancecharge

- **A charged shot** (his note 2026-11-23: held fire drew the stream while the damage came in pulses): the trigger held
  charges it over `vr_dw_lance_charge` s (1; QC `DW_LanceChargeFrame`), let go full it fires, let go sooner nothing (no
  cells). The clients draw the charge at the crystal tip (`vr_lancecharge.cpp`: sparks gathering in, a violet light
  growing, a rising hum) for the player and every other player's copy (`QVR_SVC_LANCECHARGE`), and keep its bolt
  `vr_dw_lance_bolt` s (0.35). ROUND25.md "lancecharge" has the detail.

## Built in spear (2026-11-24)

- **The plain spear** (part 3 above): `progs/v_spear.mdl` (make_spear.py), the knight's sword of kind 13
  (`VR_SWORDKIND_SPEAR`), weapons.txt slot 57 (the Lance's hold: its model lies where the Lance's pole does). The blow is
  shared: `VR_PoleMelee` (QC/vr_poleweapon.qc, the Lance's former `DW_LanceMelee`), its head from the slot's `bl_from`.
  Impale (part 1) is the sword's point embed with the head as the blade. ROUND25.md "spear".

## The shaft (spearblunt, 2026-11-24)

- Every pole weapon is **a head and a shaft**: the head from `VR_PoleHeadFrom` (the slot's Blade Start, else
  `vr_dw_lance_head`) to the tip strikes as its kind's blade (the spear's point stabs and sticks, its edges cut; the
  Lance's crystal head its thrust); the shaft from the hand to the head, and the butt behind the hand, strike as a staff:
  blunt. One path for both: QC `VR_Melee_PoleLayout` (the melee points: the shaft's of kind `VR_MPT_BLUNT`, landing as
  the pommel kind at full weight), so `vr_melee_blunt` and every blunt rule (the hit's class, round wounds and dents, the
  knock, the knockdown rules' blunt blow, no stab and no embed); `VR_PoleMelee` deals it as a club (`vr_dmg_gun_bash`,
  the blunt layer). The engine's leg sweep (`vr_legsweep.cpp` `edgeShare`) has the same head: the shaft sweeps legs,
  the head cuts. A staff (part 4) would be the shaft's rule at both ends. ROUND25.md "spearblunt".
