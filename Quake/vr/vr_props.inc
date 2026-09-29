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
QVR_PROP_KEY(Mass, "mass", "0")              // kg; 0: estimated (its volume times a density for what it is)
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
#endif

#ifdef QVR_PROP_RETIRED
// Keys no longer used, kept registered (unsaved, read by nothing) so that configs holding them load silently: the rocks'
// and bricks' Blunt (1 + Blunt x the square root of the mass), folded into the weight's damage curve and their Melee and
// Throw Damage x (round 21, "Spring only; Weapon Weights and Held Object Weights; weight and damage").
QVR_PROP_RETIRED("blunt")
#endif

#ifdef QVR_PROP_DEFAULT
// The explosive boxes (misc_explobox, misc_explobox2): physics props, heavy (a hand can carry one, two do it better),
// thrown only a little way, never force grabbed.
QVR_PROP_DEFAULT(0, ID, "maps/b_explob.bsp")
QVR_PROP_DEFAULT(0, Mass, "40")
QVR_PROP_DEFAULT(0, ForceGrab, "0")
QVR_PROP_DEFAULT(1, ID, "maps/b_exbox2.bsp")
QVR_PROP_DEFAULT(1, Mass, "25")
QVR_PROP_DEFAULT(1, ForceGrab, "0")
// The flashlight in the hand (vr_flashlight.cpp: its grip and fingers are its own page's): its weight only.
QVR_PROP_DEFAULT(2, ID, "progs/vrflashlight.mdl")
QVR_PROP_DEFAULT(2, Mass, "0.4")
// The ogres' grenades (vr_grenade.qc: caught, held and thrown back): light and small in the hand, thrown with the hand's
// whole speed. Also their mass in Box3D (5.2 kg from their hull at 2000 kg/m^3 before: they knock boxes less now).
QVR_PROP_DEFAULT(3, ID, "progs/grenade.mdl")
QVR_PROP_DEFAULT(3, Mass, "1.2")
// In the palm, in one hand (round 21, "Hand grenades from the back pouch": a grenade from the pouch, or caught): its
// middle over the fist's grip channel, the fingers round it. vr_props_version 44 gives these to configs saved before.
QVR_PROP_DEFAULT(3, GripMode, "2")
QVR_PROP_DEFAULT(3, TwoHands, "0")
QVR_PROP_DEFAULT(4, ID, "progs/mervup.mdl")
QVR_PROP_DEFAULT(4, Mass, "1.2")
// A wall torch taken off its wall (vr_walltorch.qc; ROUND21.md, "Wall torches you can take"): a wooden stick along its
// model's +x (the butt at -14, the handle to -7, the head to +1.8), its weight in its head; held by its handle through the
// fist, flame up (Along the Handle: where it was taken along the handle, leaning as taken), in one hand (the other may take
// another torch), its head the melee's striking end. (Slot 17: apart from the next props' entries; vr_cfg_version 25
// gives it to configs saved before it; vr_props_version 40 its grip.)
QVR_PROP_DEFAULT(16, ID, "progs/vrtorch.mdl")
QVR_PROP_DEFAULT(16, Mass, "0.9")
QVR_PROP_DEFAULT(16, ComX, "4")
QVR_PROP_DEFAULT(16, GripMode, "3")
QVR_PROP_DEFAULT(16, TwoHands, "0")
QVR_PROP_DEFAULT(16, HandleFrom, "-12.4")
QVR_PROP_DEFAULT(16, HandleTo, "-8.2")
QVR_PROP_DEFAULT(16, TipX, "2.3")
QVR_PROP_DEFAULT(16, ButtX, "-14")
// Gibs and heads (round 21, "Spring only; Weapon Weights and Held Object Weights; weight and damage"): Box3D's estimate
// from their drawn hulls at flesh's density made them 18 to 405 kg (a gib2 292, a grunt's head 73), so they lagged in the
// hand like explosive boxes, left it at a fifth of its speed, and would strike and hit thrown several times harder by the
// weight's damage curve. Their masses as things of their kind weigh: the Mass is also their Box3D mass. (Slots 34..48,
// from the table's end: vr_cfg_version 27 gives them to configs saved before; props::fleshSlots.)
QVR_PROP_DEFAULT(33, ID, "progs/gib1.mdl")
QVR_PROP_DEFAULT(33, Mass, "0.8")
QVR_PROP_DEFAULT(34, ID, "progs/gib2.mdl")
QVR_PROP_DEFAULT(34, Mass, "2.5")
QVR_PROP_DEFAULT(35, ID, "progs/gib3.mdl")
QVR_PROP_DEFAULT(35, Mass, "2")
QVR_PROP_DEFAULT(36, ID, "progs/h_player.mdl")
QVR_PROP_DEFAULT(36, Mass, "5")
QVR_PROP_DEFAULT(37, ID, "progs/h_guard.mdl")
QVR_PROP_DEFAULT(37, Mass, "5")
QVR_PROP_DEFAULT(38, ID, "progs/h_dog.mdl")
QVR_PROP_DEFAULT(38, Mass, "3")
QVR_PROP_DEFAULT(39, ID, "progs/h_mega.mdl")
QVR_PROP_DEFAULT(39, Mass, "5")
QVR_PROP_DEFAULT(40, ID, "progs/h_knight.mdl")
QVR_PROP_DEFAULT(40, Mass, "5")
QVR_PROP_DEFAULT(41, ID, "progs/h_hellkn.mdl")
QVR_PROP_DEFAULT(41, Mass, "6")
QVR_PROP_DEFAULT(42, ID, "progs/h_ogre.mdl")
QVR_PROP_DEFAULT(42, Mass, "9")
QVR_PROP_DEFAULT(43, ID, "progs/h_wizard.mdl")
QVR_PROP_DEFAULT(43, Mass, "3")
QVR_PROP_DEFAULT(44, ID, "progs/h_zombie.mdl")
QVR_PROP_DEFAULT(44, Mass, "4")
QVR_PROP_DEFAULT(45, ID, "progs/h_shal.mdl")
QVR_PROP_DEFAULT(45, Mass, "6")
QVR_PROP_DEFAULT(46, ID, "progs/h_shams.mdl")
QVR_PROP_DEFAULT(46, Mass, "15")
QVR_PROP_DEFAULT(47, ID, "progs/h_demon.mdl")
QVR_PROP_DEFAULT(47, Mass, "12")
// The rocks and bricks lying about (vr_debris.cpp, QC vr_debris.qc; slots 17-25, vr_prop_*_18 to _26, after the torch's): their Mass is
// left estimated, stone's density times their size (vr_props.cpp stoneDensity: a rock 0.3 to 1.7 kg, a whole brick
// about 1.9), since each piece is sized apart. Their damage is the weight's (round 21, "Spring only; Weapon Weights and
// Held Object Weights; weight and damage"): a punch holding one hits Box Punch Damage (1.5) x Melee Damage 1.4 x the
// weight's curve by its mass (a 1 kg rock 1.79, a brick 2.1); the whole bricks (a tip: clubs, whose blows take no Box
// Punch Damage) Melee Damage 2.1; thrown, Thrown Box Damage x 2.1 x the curve. The rocks and the half brick one hand; the
// whole, chipped and broken bricks (19 and 13 cm) both hands too (vr_props_version 39), then carried as a box, not a club.
// Held (round 21, "Held props: grip modes, live offsets, palm grip, torch handle"): the rocks and the half brick In the
// Palm (their middle in the palm's, the fingers round them), the whole bricks and the broken piece fixed (by an end).
QVR_PROP_DEFAULT(17, ID, "progs/vr_rock1.mdl")
QVR_PROP_DEFAULT(17, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(17, MeleeDamage, "1.4")
QVR_PROP_DEFAULT(17, TwoHands, "0")
QVR_PROP_DEFAULT(17, GripMode, "2")
QVR_PROP_DEFAULT(18, ID, "progs/vr_rock2.mdl")
QVR_PROP_DEFAULT(18, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(18, MeleeDamage, "1.4")
QVR_PROP_DEFAULT(18, TwoHands, "0")
QVR_PROP_DEFAULT(18, GripMode, "2")
QVR_PROP_DEFAULT(19, ID, "progs/vr_rock3.mdl")
QVR_PROP_DEFAULT(19, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(19, MeleeDamage, "1.4")
QVR_PROP_DEFAULT(19, TwoHands, "0")
QVR_PROP_DEFAULT(19, GripMode, "2")
QVR_PROP_DEFAULT(20, ID, "progs/vr_rock4.mdl")
QVR_PROP_DEFAULT(20, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(20, MeleeDamage, "1.4")
QVR_PROP_DEFAULT(20, TwoHands, "0")
QVR_PROP_DEFAULT(20, GripMode, "2")
QVR_PROP_DEFAULT(21, ID, "progs/vr_rock5.mdl")
QVR_PROP_DEFAULT(21, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(21, MeleeDamage, "1.4")
QVR_PROP_DEFAULT(21, TwoHands, "0")
QVR_PROP_DEFAULT(21, GripMode, "2")
QVR_PROP_DEFAULT(22, ID, "progs/vr_brick1.mdl")
QVR_PROP_DEFAULT(22, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(22, MeleeDamage, "2.1")
QVR_PROP_DEFAULT(22, TwoHands, "1")
QVR_PROP_DEFAULT(22, GripMode, "1")
QVR_PROP_DEFAULT(22, GripX, "0.8")
QVR_PROP_DEFAULT(22, GripZ, "-1.6")
QVR_PROP_DEFAULT(22, TipX, "2.4")
QVR_PROP_DEFAULT(22, ButtX, "-2.4")
QVR_PROP_DEFAULT(23, ID, "progs/vr_brick2.mdl")
QVR_PROP_DEFAULT(23, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(23, MeleeDamage, "2.1")
QVR_PROP_DEFAULT(23, TwoHands, "1")
QVR_PROP_DEFAULT(23, GripMode, "1")
QVR_PROP_DEFAULT(23, GripX, "0.8")
QVR_PROP_DEFAULT(23, GripZ, "-1.6")
QVR_PROP_DEFAULT(23, TipX, "2.4")
QVR_PROP_DEFAULT(23, ButtX, "-2.4")
QVR_PROP_DEFAULT(24, ID, "progs/vr_brick3.mdl")
QVR_PROP_DEFAULT(24, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(24, MeleeDamage, "1.4")
QVR_PROP_DEFAULT(24, TwoHands, "0")
QVR_PROP_DEFAULT(24, GripMode, "2")
QVR_PROP_DEFAULT(25, ID, "progs/vr_brick4.mdl")
QVR_PROP_DEFAULT(25, ThrowDamage, "2.1")
QVR_PROP_DEFAULT(25, MeleeDamage, "2.1")
QVR_PROP_DEFAULT(25, TwoHands, "1")
QVR_PROP_DEFAULT(25, GripMode, "1")
QVR_PROP_DEFAULT(25, GripX, "0")
QVR_PROP_DEFAULT(25, GripZ, "-1.6")
QVR_PROP_DEFAULT(25, TipX, "1.6")
QVR_PROP_DEFAULT(25, ButtX, "-1.6")
#endif
