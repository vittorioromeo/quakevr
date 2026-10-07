// vr_props.inc -- per-prop settings: what a thing a hand carries weighs and how it is held (Held Object Offsets and
// Held Object Weights; see vr_props.hpp and docs/vr-port/ROUND21.md, "Weight: spring model, stamina, held object
// offsets; explosive boxes").
//   QVR_PROP_KEY(Enum, "key", "default"): cvars vr_prop_<key>_01 .. vr_prop_<key>_48, every slot's default
//   QVR_PROP_DEFAULT(slot, Enum, "value"): a slot's own defaults (its ID, the model it is for, first)
// A slot is a kind of prop, by the model it is drawn with (its ID: "progs/gib1.mdl", "maps/b_explob.bsp"). A model
// with no slot has every key at its default: its mass estimated from its size and what it is made of. Adding a key:
// ROUND21.md's section says how.

#ifdef QVR_PROP_KEY
QVR_PROP_KEY(ID, "id", "")                   // the model ("" or "-1": a free slot)
QVR_PROP_KEY(Mass, "mass", "0")              // kg; 0: estimated (its volume times a density for what it is); -1: a monster's head, its ragdoll's share (box3d::headPropMass)
QVR_PROP_KEY(Inertia, "inertia", "1")        // times the inertia of its box: how sluggishly it turns in the hand
QVR_PROP_KEY(ComX, "com_x", "0")             // units off the middle of its drawn box, in its model's axes: its
QVR_PROP_KEY(ComY, "com_y", "0")             // centre of mass (a torch's head, a hammer's)
QVR_PROP_KEY(ComZ, "com_z", "0")
QVR_PROP_KEY(Throw, "throw", "0")            // times the hand's throw; 0: from the mass (vr_weight_throw_mass)
QVR_PROP_KEY(GripMode, "grip", "0")          // how it sits in the hand (vr_grip.hpp): 0 where taken; 1 fixed (Grip X..Roll);
                                             // 2 in the palm (a rock); 3 along the handle (a torch)
QVR_PROP_KEY(GripX, "grip_x", "0")           // Grip Mode 1: the model's origin in the right hand's frame, units (x forward,
QVR_PROP_KEY(GripY, "grip_y", "0")           // y left, z up; mirrored for the left hand); the others: an offset on top,
QVR_PROP_KEY(GripZ, "grip_z", "0")           // moved in the hand's frame
QVR_PROP_KEY(GripPitch, "grip_pitch", "0")   // and its turn, degrees: Grip Mode 1 as the model's angles off the hand's; the
QVR_PROP_KEY(GripYaw, "grip_yaw", "0")       // others: turned in the hand's frame (pitch up, yaw left, roll right) about
QVR_PROP_KEY(GripRoll, "grip_roll", "0")     // where the hand holds it
QVR_PROP_KEY(FingerManual, "fgr_manual", "0") // 0: the fingers wrap it (fitted); 1: each at its curl below
QVR_PROP_KEY(FingerCurlThumb, "fgr_curl_thumb", "0.6")
QVR_PROP_KEY(FingerCurlIndex, "fgr_curl_index", "0.6")
QVR_PROP_KEY(FingerCurlMiddle, "fgr_curl_middle", "0.6")
QVR_PROP_KEY(FingerCurlRing, "fgr_curl_ring", "0.6")
QVR_PROP_KEY(FingerCurlPinky, "fgr_curl_pinky", "0.6")
QVR_PROP_KEY(FingerThumbAcross, "fgr_thumb_across", "0")
QVR_PROP_KEY(Overlap, "overlap", "-1")       // cm the fitted fingers and palm may sink into it; -1: Fit Overlap: Things
QVR_PROP_KEY(TwoHands, "two_hands", "1")     // 1: the other hand may hold it too (vr_carry_two_hands); 0: one hand only
QVR_PROP_KEY(ForceGrab, "forcegrab", "-1")   // -1: as the game has it; 0: never force grabbed
QVR_PROP_KEY(TipX, "tip_x", "0")             // melee: its striking end (a torch's head), units in its model's axes; all
QVR_PROP_KEY(TipY, "tip_y", "0")             // three 0: it strikes as the fist holding it (a box). Set, it is a club:
QVR_PROP_KEY(TipZ, "tip_z", "0")             // a line from its butt (below) through the hand to here (vr_melee.qc)
QVR_PROP_KEY(ButtX, "butt_x", "0")           // melee: its other end (a pommel strike's), units in its model's axes
QVR_PROP_KEY(ButtY, "butt_y", "0")
QVR_PROP_KEY(ButtZ, "butt_z", "0")
// Round 21, "Spring only; Weapon Weights and Held Object Weights; weight and damage" (Held Object Weights): its own
// multipliers of the spring's global settings (Aiming: Spring, vr_weight_spring_*) and of its melee and thrown damage.
QVR_PROP_KEY(SpringStiffness, "stiffness", "1")
QVR_PROP_KEY(SpringDamping, "damping", "1")
QVR_PROP_KEY(SpringStrength, "strength", "1")
QVR_PROP_KEY(SpringSag, "sag", "1")
QVR_PROP_KEY(SpringSwing, "swing", "1")
QVR_PROP_KEY(SpringTwoHanded, "help_2h", "1")
QVR_PROP_KEY(SpringSnap, "snap", "1")
QVR_PROP_KEY(MeleeDamage, "melee_dmg", "1")
QVR_PROP_KEY(ThrowDamage, "throw_dmg", "1")
// Round 21, "Held props: grip modes, live offsets, palm grip, torch handle": Grip Mode 3's handle.
QVR_PROP_KEY(HandleFrom, "handle_from", "0") // where the fist may hold it along its long axis, units in its model's axes
QVR_PROP_KEY(HandleTo, "handle_to", "0")     // from its origin; both 0: the butt's half, short of its end
QVR_PROP_KEY(HandleTilt, "handle_tilt", "25") // degrees its handle may lean off the fist's grip channel, as taken
// Round 21, "Prop size": times its model's size, about its origin (0.05x steps): as drawn, its Box3D body and hull, its
// drawn box (a solid prop's Quake box) and its lengths above (Centre of Mass, Tip, Butt, Handle, a Fixed grip's place).
QVR_PROP_KEY(Size, "size", "1")
// Round 21, "Thrown axes: blade leniency, spin in the air": times Throwing and Physics' Spin Alignment
// (vr_throw_spin_align) for it thrown (0: its tumble as it left the hand).
QVR_PROP_KEY(SpinAlign, "spin_align", "1")
// Round 21, "Prop settings for a held weapon; estimated mass x; the thumb round a rock": times its estimated mass (Mass 0:
// its volume, which follows its Size, times its material's density), as the hands weigh it and as Box3D's body. A Mass
// set is used as set (neither this nor the Size scales it).
QVR_PROP_KEY(MassScale, "mass_scale", "1")
#endif


#ifdef QVR_PROP_DEFAULT
// The explosive boxes (misc_explobox, misc_explobox2): physics props, heavy (a hand can carry one, two do it better),
// thrown only a little way, never force grabbed.
QVR_PROP_DEFAULT(0, ID, "maps/b_explob.bsp")
QVR_PROP_DEFAULT(0, Mass, "80.5")
QVR_PROP_DEFAULT(0, ForceGrab, "0")
QVR_PROP_DEFAULT(1, ID, "maps/b_exbox2.bsp")
QVR_PROP_DEFAULT(1, Mass, "40")
QVR_PROP_DEFAULT(1, ForceGrab, "0")
// The flashlight in the hand (vr_flashlight.cpp: its grip and fingers are its own page's): its weight only.
QVR_PROP_DEFAULT(2, ID, "progs/vrflashlight.mdl")
QVR_PROP_DEFAULT(2, Mass, "0.4")
// The ogres' grenades (vr_grenade.qc: caught, held and thrown back): light and small in the hand, thrown with the hand's
// whole speed. Also their mass in Box3D (5.2 kg from their hull at 2000 kg/m^3 before: they knock boxes less now).
QVR_PROP_DEFAULT(3, ID, "progs/grenade.mdl")
QVR_PROP_DEFAULT(3, Mass, "1.2")
// In the palm (round 21, "Hand grenades from the back pouch": a grenade from the pouch, or caught): its middle over the
// fist's grip channel, the fingers round it. vr_props_version 44 gives these to configs saved before. In both hands too
// since vr_props_version 56 (ROUND21.md, "Every prop in both hands": the other hand takes it to grip it again).
QVR_PROP_DEFAULT(3, GripMode, "2")
// The author's fit (NOTES.md vrfiringrange_2026-10-01_16-53-53, the thumb round it; vr_props_version 54): turned 10
// degrees, 1 unit up, the fingers sunk 0.75 cm into it.
QVR_PROP_DEFAULT(3, GripRoll, "10")
QVR_PROP_DEFAULT(3, GripZ, "1")
QVR_PROP_DEFAULT(3, Overlap, "0.75")
QVR_PROP_DEFAULT(4, ID, "progs/mervup.mdl")
QVR_PROP_DEFAULT(4, Mass, "1.2")
// The multi-grenade held as the grenade is (round 21, "Multi-grenades from the pouch": one from the pouch with B/Y held,
// or caught): in the palm. vr_props_version 53 gives these to configs saved before; both hands too since 56.
QVR_PROP_DEFAULT(4, GripMode, "2")
// The author's (NOTES.md vrfiringrange_2026-10-01_17-17-58; vr_props_version 54): turned 10 degrees, the fingers sunk
// 0.6 cm into it.
QVR_PROP_DEFAULT(4, GripYaw, "10")
QVR_PROP_DEFAULT(4, Overlap, "0.6")
// The author's weights and sizes (NOTES.md vrfiringrange_2026-10-02_00-55-03, "make them the defaults"; vr_props_version
// 55 gives slots 6-16 to configs that have them free, and his new values to those that hold the old ones): the items he
// gave slots in Held Object Offsets (their mass estimated, times Mass x; the mission packs' two heads weighed), and below
// the explosive boxes, the torch (estimated), the rocks and bricks (1.25x), the crates and their boards, gibs and heads.
QVR_PROP_DEFAULT(5, ID, "maps/b_rock0.bsp")
QVR_PROP_DEFAULT(5, Size, "1.25")
QVR_PROP_DEFAULT(5, MassScale, "1.25")
QVR_PROP_DEFAULT(6, ID, "maps/b_plas1.bsp")
QVR_PROP_DEFAULT(6, MassScale, "1.25")
QVR_PROP_DEFAULT(7, ID, "progs/h_grem.mdl")
QVR_PROP_DEFAULT(7, Mass, "-1")
QVR_PROP_DEFAULT(8, ID, "progs/h_scourg.mdl")
QVR_PROP_DEFAULT(8, Mass, "-1")
QVR_PROP_DEFAULT(9, ID, "progs/armor.mdl")
QVR_PROP_DEFAULT(9, MassScale, "0.6")
QVR_PROP_DEFAULT(10, ID, "maps/b_bh100.bsp")
QVR_PROP_DEFAULT(10, MassScale, "1.25")
QVR_PROP_DEFAULT(11, ID, "maps/b_bh10.bsp")
QVR_PROP_DEFAULT(11, MassScale, "1.15")
QVR_PROP_DEFAULT(12, ID, "maps/b_shell0.bsp")
QVR_PROP_DEFAULT(12, MassScale, "1.25")
QVR_PROP_DEFAULT(13, ID, "maps/b_nail0.bsp")
QVR_PROP_DEFAULT(13, MassScale, "1.25")
QVR_PROP_DEFAULT(14, ID, "maps/b_lnail0.bsp")
QVR_PROP_DEFAULT(14, MassScale, "1.25")
QVR_PROP_DEFAULT(15, ID, "maps/b_mrock1.bsp")
QVR_PROP_DEFAULT(15, Size, "1.25")
QVR_PROP_DEFAULT(15, MassScale, "1.25")
// A wall torch taken off its wall (vr_walltorch.qc; ROUND21.md, "Wall torches you can take"): a wooden stick along its
// model's +x (the butt at -14, the handle to -7, the head to +1.8), its weight in its head; held by its handle through the
// fist, flame up (Along the Handle: where it was taken along the handle, leaning as taken), in either hand or both (the
// author's choice, 2026-09-29: Two Hands on), its head the melee's striking end. (Slot 17: apart from the next props'
// entries; vr_cfg_version 25 gives it to configs saved before it; vr_props_version 40 its grip, 45 its Two Hands.)
QVR_PROP_DEFAULT(16, ID, "progs/vrtorch.mdl")
QVR_PROP_DEFAULT(16, Mass, "0")
QVR_PROP_DEFAULT(16, ComX, "4")
QVR_PROP_DEFAULT(16, GripMode, "3")
QVR_PROP_DEFAULT(16, HandleFrom, "-12.4")
QVR_PROP_DEFAULT(16, HandleTo, "-8.2")
QVR_PROP_DEFAULT(16, TipX, "2.3")
QVR_PROP_DEFAULT(16, ButtX, "-14")
// Gibs and heads (round 21, "Spring only; Weapon Weights and Held Object Weights; weight and damage"): Box3D's estimate
// from their drawn hulls at flesh's density made them 18 to 405 kg (a gib2 292, a grunt's head 73), so they lagged in the
// hand like explosive boxes, left it at a fifth of its speed, and would strike and hit thrown several times harder by the
// weight's damage curve. Their masses as things of their kind weigh: the Mass is also their Box3D mass. (Slots 34..48;
// vr_cfg_version 27 gives them to configs saved before; props::fleshSlots.) The author's heavier
// masses since 2026-10-02 (vr_props_version 55): a gib 8 to 20 kg, a head 10 to 70; lighter since 2026-10-07
// (vr_props_version 58, his config's): a gib 8 to 15, a head 5 to 65. Since vr_props_version 59 ("bring the head's
// weights closer to the ragdoll's"), a monster's head Mass -1: what a head cut off its ragdoll weighs (vr_limbs.qc
// VR_Limb_SetMass; box3d::headPropMass), 7% of its class's vr_ragdoll_<class>_mass times vr_limbs_mass_scale, followed
// live (a grunt's 5.6 kg, a dog's 2.8, a shambler's 19.6). The player's head (no ragdoll class) keeps its 5 kg.
QVR_PROP_DEFAULT(33, ID, "progs/gib1.mdl")
QVR_PROP_DEFAULT(33, Mass, "8")
QVR_PROP_DEFAULT(34, ID, "progs/gib2.mdl")
QVR_PROP_DEFAULT(34, Size, "0.75")
QVR_PROP_DEFAULT(34, Mass, "15")
QVR_PROP_DEFAULT(35, ID, "progs/gib3.mdl")
QVR_PROP_DEFAULT(35, Size, "0.8")
QVR_PROP_DEFAULT(35, Mass, "10")
QVR_PROP_DEFAULT(36, ID, "progs/h_player.mdl")
QVR_PROP_DEFAULT(36, Mass, "5")
QVR_PROP_DEFAULT(37, ID, "progs/h_guard.mdl")
QVR_PROP_DEFAULT(37, Size, "0.7")
QVR_PROP_DEFAULT(37, Mass, "-1")
QVR_PROP_DEFAULT(38, ID, "progs/h_dog.mdl")
QVR_PROP_DEFAULT(38, Size, "0.75")
QVR_PROP_DEFAULT(38, Mass, "-1")
QVR_PROP_DEFAULT(39, ID, "progs/h_mega.mdl")
QVR_PROP_DEFAULT(39, Size, "0.7")
QVR_PROP_DEFAULT(39, Mass, "-1")
QVR_PROP_DEFAULT(40, ID, "progs/h_knight.mdl")
QVR_PROP_DEFAULT(40, Size, "0.7")
QVR_PROP_DEFAULT(40, Mass, "-1")
QVR_PROP_DEFAULT(41, ID, "progs/h_hellkn.mdl")
QVR_PROP_DEFAULT(41, Size, "0.75")
QVR_PROP_DEFAULT(41, Mass, "-1")
QVR_PROP_DEFAULT(42, ID, "progs/h_ogre.mdl")
QVR_PROP_DEFAULT(42, Size, "0.65")
QVR_PROP_DEFAULT(42, Mass, "-1")
QVR_PROP_DEFAULT(43, ID, "progs/h_wizard.mdl")
QVR_PROP_DEFAULT(43, Mass, "-1")
QVR_PROP_DEFAULT(44, ID, "progs/h_zombie.mdl")
QVR_PROP_DEFAULT(44, Mass, "-1")
QVR_PROP_DEFAULT(45, ID, "progs/h_shal.mdl")
QVR_PROP_DEFAULT(45, Size, "0.75")
QVR_PROP_DEFAULT(45, Mass, "-1")
QVR_PROP_DEFAULT(46, ID, "progs/h_shams.mdl")
QVR_PROP_DEFAULT(46, Size, "0.9")
QVR_PROP_DEFAULT(46, Mass, "-1")
QVR_PROP_DEFAULT(47, ID, "progs/h_demon.mdl")
QVR_PROP_DEFAULT(47, Size, "0.7")
QVR_PROP_DEFAULT(47, Mass, "-1")
// The rocks and bricks lying about (vr_debris.cpp, QC vr_debris.qc; slots 17-25, vr_prop_*_18 to _26, after the torch's): their Mass is
// left estimated, stone's density times their size (vr_props.cpp stoneDensity: a rock 0.3 to 1.7 kg, a whole brick
// about 1.9), since each piece is sized apart. Their damage is the weight's (round 21, "Spring only; Weapon Weights and
// Held Object Weights; weight and damage"): a punch holding one hits Box Punch Damage (1.5) x Melee Damage 1.4 x the
// weight's curve by its mass (a 1 kg rock 1.79, a brick 2.1); the whole bricks (a tip: clubs, whose blows take no Box
// Punch Damage) Melee Damage 2.1; thrown, Thrown Box Damage x 2.1 x the curve. The whole, chipped and broken bricks (19 and
// 13 cm) both hands too (vr_props_version 39), then carried as a box, not a club; the rocks and the half brick too since
// vr_props_version 56 (every prop in both hands: the other hand takes it to grip it again).
// Held (round 21, "Held props: grip modes, live offsets, palm grip, torch handle"): every rock and brick In the Palm (its
// middle in the palm's, the fingers round it). The whole bricks and the broken piece were fixed (by an end) before the
// author moved them In the Palm (2026-09-29; vr_props_version 45). Their Grip X and Z (the fixed grip's, kept as an
// offset) went back to 0 when he found the palm needs none (NOTES.md start_2026-09-29_23-01-20; vr_props_version 48):
// every rock and brick In the Palm at the default offsets. Every one drawn 1.25 times its model's size (the author's,
// NOTES.md vrfiringrange_2026-10-01_00-25-50; vr_props_version 50), which makes its estimated mass 1.95 times its model's.
QVR_PROP_DEFAULT(17, ID, "progs/vr_rock1.mdl")
QVR_PROP_DEFAULT(17, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(17, MeleeDamage, "1.4")
QVR_PROP_DEFAULT(17, GripMode, "2")
QVR_PROP_DEFAULT(17, Size, "1.25")
QVR_PROP_DEFAULT(17, MassScale, "1.25")
QVR_PROP_DEFAULT(18, ID, "progs/vr_rock2.mdl")
QVR_PROP_DEFAULT(18, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(18, MeleeDamage, "1.4")
QVR_PROP_DEFAULT(18, GripMode, "2")
QVR_PROP_DEFAULT(18, Size, "1.25")
QVR_PROP_DEFAULT(18, MassScale, "1.25")
QVR_PROP_DEFAULT(19, ID, "progs/vr_rock3.mdl")
QVR_PROP_DEFAULT(19, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(19, MeleeDamage, "1.4")
QVR_PROP_DEFAULT(19, GripMode, "2")
QVR_PROP_DEFAULT(19, Size, "1.25")
QVR_PROP_DEFAULT(19, MassScale, "1.25")
QVR_PROP_DEFAULT(20, ID, "progs/vr_rock4.mdl")
QVR_PROP_DEFAULT(20, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(20, MeleeDamage, "1.4")
QVR_PROP_DEFAULT(20, GripMode, "2")
QVR_PROP_DEFAULT(20, Size, "1.25")
QVR_PROP_DEFAULT(20, MassScale, "1.25")
QVR_PROP_DEFAULT(21, ID, "progs/vr_rock5.mdl")
QVR_PROP_DEFAULT(21, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(21, MeleeDamage, "1.4")
QVR_PROP_DEFAULT(21, GripMode, "2")
QVR_PROP_DEFAULT(21, Size, "1.25")
QVR_PROP_DEFAULT(21, MassScale, "1.25")
QVR_PROP_DEFAULT(22, ID, "progs/vr_brick1.mdl")
QVR_PROP_DEFAULT(22, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(22, MeleeDamage, "2.1")
QVR_PROP_DEFAULT(22, TwoHands, "1")
QVR_PROP_DEFAULT(22, GripMode, "2")
QVR_PROP_DEFAULT(22, Size, "1.25")
QVR_PROP_DEFAULT(22, MassScale, "1.25")
QVR_PROP_DEFAULT(22, HandleTilt, "0")
QVR_PROP_DEFAULT(22, TipX, "2.4")
QVR_PROP_DEFAULT(22, ButtX, "-2.4")
QVR_PROP_DEFAULT(23, ID, "progs/vr_brick2.mdl")
QVR_PROP_DEFAULT(23, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(23, MeleeDamage, "2.1")
QVR_PROP_DEFAULT(23, TwoHands, "1")
QVR_PROP_DEFAULT(23, GripMode, "2")
QVR_PROP_DEFAULT(23, Size, "1.25")
QVR_PROP_DEFAULT(23, MassScale, "1.25")
QVR_PROP_DEFAULT(23, HandleTilt, "0")
QVR_PROP_DEFAULT(23, TipX, "2.4")
QVR_PROP_DEFAULT(23, ButtX, "-2.4")
QVR_PROP_DEFAULT(24, ID, "progs/vr_brick3.mdl")
QVR_PROP_DEFAULT(24, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(24, MeleeDamage, "1.4")
QVR_PROP_DEFAULT(24, GripMode, "2")
QVR_PROP_DEFAULT(24, Size, "1.25")
QVR_PROP_DEFAULT(25, ID, "progs/vr_brick4.mdl")
QVR_PROP_DEFAULT(25, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(25, MeleeDamage, "2.1")
QVR_PROP_DEFAULT(25, TwoHands, "1")
QVR_PROP_DEFAULT(25, GripMode, "2")
QVR_PROP_DEFAULT(25, Size, "1.25")
QVR_PROP_DEFAULT(25, MassScale, "1.25")
QVR_PROP_DEFAULT(25, TipX, "1.6")
QVR_PROP_DEFAULT(25, ButtX, "-1.6")
// The wooden crates (vr_crates.qc; ROUND21.md, "Wooden crates"): as heavy as the explosive boxes of their size, carried
// by hand as they are, never force grabbed. What they break into: light boards, a splinter and a batten, thrown and
// force grabbed as any small thing (vr_props_version 49 gives these slots to configs saved before); the small ones (a
// broken board, the splinter, a broken batten) held in the palm, the whole board where taken (51). Every one in both
// hands too (56: the broken board and the splinter were one hand).
QVR_PROP_DEFAULT(26, ID, "progs/vr_crate1.mdl")
QVR_PROP_DEFAULT(26, Mass, "40")
QVR_PROP_DEFAULT(26, ForceGrab, "0")
QVR_PROP_DEFAULT(27, ID, "progs/vr_crate2.mdl")
QVR_PROP_DEFAULT(27, Mass, "65")
QVR_PROP_DEFAULT(27, ForceGrab, "0")
QVR_PROP_DEFAULT(28, ID, "progs/vr_plank1.mdl")
QVR_PROP_DEFAULT(28, Mass, "4")
QVR_PROP_DEFAULT(29, ID, "progs/vr_plank2.mdl")
QVR_PROP_DEFAULT(29, GripMode, "2")
QVR_PROP_DEFAULT(29, Mass, "2.5")
QVR_PROP_DEFAULT(30, ID, "progs/vr_plank3.mdl")
QVR_PROP_DEFAULT(30, GripMode, "2")
QVR_PROP_DEFAULT(30, Mass, "0.15")
QVR_PROP_DEFAULT(31, ID, "progs/vr_plank4.mdl")
QVR_PROP_DEFAULT(31, GripMode, "2")
QVR_PROP_DEFAULT(31, Mass, "3")
// Immersive reloading's rounds (QC vr_reload.qc; docs/vr-port/RELOAD_PLAN.md): a shotgun shell from the ammo pouch, and
// the taped pair (vr_reload_shell_pairs). Drawn at the spent shells' 1.25 (vr_shells.cpp, the guns' own oversize), light,
// in one hand. Held fixed at the fingertips as for loading (Grip Mode 1: along the index finger, its brass end in the
// fingers, its crimp forward), pinched by the thumb, the index and the middle finger, the ring finger and the pinky
// tucked in (Fingers: manual). The author tunes them live in Held Object Offsets (a shell in the hand). vr_props_version
// 60 gives these slots to configs saved before.
QVR_PROP_DEFAULT(48, ID, "progs/vr_shell_live.mdl")
QVR_PROP_DEFAULT(48, Mass, "0.04")
QVR_PROP_DEFAULT(48, Size, "1.25")
QVR_PROP_DEFAULT(48, TwoHands, "0")
QVR_PROP_DEFAULT(48, GripMode, "1")
QVR_PROP_DEFAULT(48, GripX, "-0.2")
QVR_PROP_DEFAULT(48, GripY, "0.15")
QVR_PROP_DEFAULT(48, GripZ, "-1.5")
QVR_PROP_DEFAULT(48, FingerManual, "0")
QVR_PROP_DEFAULT(48, FingerCurlThumb, "0.55")
QVR_PROP_DEFAULT(48, FingerCurlIndex, "0.5")
QVR_PROP_DEFAULT(48, FingerCurlMiddle, "0.55")
QVR_PROP_DEFAULT(48, FingerCurlRing, "0.9")
QVR_PROP_DEFAULT(48, FingerCurlPinky, "0.95")
QVR_PROP_DEFAULT(49, ID, "progs/vr_shell_pair.mdl")
QVR_PROP_DEFAULT(49, Mass, "0.08")
QVR_PROP_DEFAULT(49, Size, "1.25")
QVR_PROP_DEFAULT(49, TwoHands, "0")
QVR_PROP_DEFAULT(49, GripMode, "1")
QVR_PROP_DEFAULT(49, GripX, "-0.2")
QVR_PROP_DEFAULT(49, GripY, "0.15")
QVR_PROP_DEFAULT(49, GripZ, "-1.5")
QVR_PROP_DEFAULT(49, FingerManual, "0")
QVR_PROP_DEFAULT(49, FingerCurlThumb, "0.5")
QVR_PROP_DEFAULT(49, FingerCurlIndex, "0.45")
QVR_PROP_DEFAULT(49, FingerCurlMiddle, "0.5")
QVR_PROP_DEFAULT(49, FingerCurlRing, "0.9")
QVR_PROP_DEFAULT(49, FingerCurlPinky, "0.95")
// The barrel (vrstart2's: vr_barrel, a crate in all but its shape; make_crates.py): a little lighter than the small
// crate, carried as the crates are, never force grabbed (vr_props_version 62 gives the slot to configs saved before).
QVR_PROP_DEFAULT(60, ID, "progs/vr_barrel.mdl")
QVR_PROP_DEFAULT(60, Mass, "30")
QVR_PROP_DEFAULT(60, ForceGrab, "0")
// Immersive reloading's magazines (phase 2; make_mags.py): the nailgun's, the super nailgun's, the thunderbolt's cell.
// Held in the palm (the fingers fitted round the box), in one hand; their weights. vr_props_version 61 gives these slots
// to configs saved before.
QVR_PROP_DEFAULT(50, ID, "progs/vr_mag_nail.mdl")
QVR_PROP_DEFAULT(50, Mass, "0.45")
QVR_PROP_DEFAULT(50, TwoHands, "0")
QVR_PROP_DEFAULT(50, GripMode, "2")
QVR_PROP_DEFAULT(50, Overlap, "0.4")
QVR_PROP_DEFAULT(51, ID, "progs/vr_mag_snail.mdl")
QVR_PROP_DEFAULT(51, Mass, "0.7")
QVR_PROP_DEFAULT(51, TwoHands, "0")
QVR_PROP_DEFAULT(51, GripMode, "2")
QVR_PROP_DEFAULT(51, Overlap, "0.4")
QVR_PROP_DEFAULT(52, ID, "progs/vr_mag_light.mdl")
QVR_PROP_DEFAULT(52, Mass, "0.9")
QVR_PROP_DEFAULT(52, TwoHands, "0")
QVR_PROP_DEFAULT(52, GripMode, "2")
QVR_PROP_DEFAULT(52, Overlap, "0.4")
#endif
