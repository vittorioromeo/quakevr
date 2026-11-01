// vr_props.inc -- per-prop settings: what a thing a hand carries weighs and how it is held (Held Object Offsets and
// Held Object Weights; see vr_props.hpp and docs/vr-port/ROUND21.md, "Weight: spring model, stamina, held object
// offsets; explosive boxes").
//   QVR_PROP_KEY(Enum, "key"): a key of quakevr/data/props.txt, where every slot's values are (its `defaults` block:
//   every slot's value unless the slot's `prop <slot>` block has its own, its ID, the model it is for, first). The
//   config and the console name a slot's value vr_prop_<key>_NN (NN = slot + 1; vr_props.cpp's stand-ins, not cvars).
// A slot is a kind of prop, by the model it is drawn with (its ID: "progs/gib1.mdl", "maps/b_explob.bsp"). A model
// with no slot has every key at its default: its mass estimated from its size and what it is made of. Adding a key:
// here and its default in props.txt's `defaults` block (ROUND21.md's section says the rest).

#ifdef QVR_PROP_KEY
QVR_PROP_KEY(ID, "id")                       // the model ("" or "-1": a free slot)
QVR_PROP_KEY(Mass, "mass")                   // kg; 0: estimated (its volume times a density for what it is); -1: a monster's head, its ragdoll's share (box3d::headPropMass)
QVR_PROP_KEY(Inertia, "inertia")             // times the inertia of its box: how sluggishly it turns in the hand
QVR_PROP_KEY(ComX, "com_x")                  // units off the middle of its drawn box, in its model's axes: its
QVR_PROP_KEY(ComY, "com_y")                  // centre of mass (a torch's head, a hammer's)
QVR_PROP_KEY(ComZ, "com_z")
QVR_PROP_KEY(Throw, "throw")                 // times the hand's throw; 0: all of it, limited by its mass (throws by weight)
QVR_PROP_KEY(GripMode, "grip")               // how it sits in the hand (vr_grip.hpp): 0 where taken; 1 fixed (Grip X..Roll);
                                             // 2 in the palm (a rock); 3 along the handle (a torch)
QVR_PROP_KEY(GripX, "grip_x")                // Grip Mode 1: the model's origin in the right hand's frame, units (x forward,
QVR_PROP_KEY(GripY, "grip_y")                // y left, z up; mirrored for the left hand); the others: an offset on top,
QVR_PROP_KEY(GripZ, "grip_z")                // moved in the hand's frame
QVR_PROP_KEY(GripPitch, "grip_pitch")        // and its turn, degrees: Grip Mode 1 as the model's angles off the hand's; the
QVR_PROP_KEY(GripYaw, "grip_yaw")            // others: turned in the hand's frame (pitch up, yaw left, roll right) about
QVR_PROP_KEY(GripRoll, "grip_roll")          // where the hand holds it
QVR_PROP_KEY(FingerManual, "fgr_manual")      // 0: the fingers wrap it (fitted); 1: each at its curl below
QVR_PROP_KEY(FingerCurlThumb, "fgr_curl_thumb")
QVR_PROP_KEY(FingerCurlIndex, "fgr_curl_index")
QVR_PROP_KEY(FingerCurlMiddle, "fgr_curl_middle")
QVR_PROP_KEY(FingerCurlRing, "fgr_curl_ring")
QVR_PROP_KEY(FingerCurlPinky, "fgr_curl_pinky")
QVR_PROP_KEY(FingerThumbAcross, "fgr_thumb_across")
QVR_PROP_KEY(Overlap, "overlap")             // cm the fitted fingers and palm may sink into it; -1: Fit Overlap: Things
QVR_PROP_KEY(TwoHands, "two_hands")          // 1: the other hand may hold it too (vr_carry_two_hands); 0: one hand only
QVR_PROP_KEY(ForceGrab, "forcegrab")         // -1: as the game has it; 0: never force grabbed
QVR_PROP_KEY(TipX, "tip_x")                  // melee: its striking end (a torch's head), units in its model's axes; all
QVR_PROP_KEY(TipY, "tip_y")                  // three 0: it strikes as the fist holding it (a box). Set, it is a club:
QVR_PROP_KEY(TipZ, "tip_z")                  // a line from its butt (below) through the hand to here (vr_melee.qc)
QVR_PROP_KEY(ButtX, "butt_x")                // melee: its other end (a pommel strike's), units in its model's axes
QVR_PROP_KEY(ButtY, "butt_y")
QVR_PROP_KEY(ButtZ, "butt_z")
// Round 21, "Spring only; Weapon Weights and Held Object Weights; weight and damage" (Held Object Weights): its own
// multipliers of the spring's global settings (Aiming: Spring, vr_weight_spring_*) and of its melee and thrown damage.
QVR_PROP_KEY(SpringStiffness, "stiffness")
QVR_PROP_KEY(SpringDamping, "damping")
QVR_PROP_KEY(SpringStrength, "strength")
QVR_PROP_KEY(SpringSag, "sag")
QVR_PROP_KEY(SpringSwing, "swing")
QVR_PROP_KEY(SpringTwoHanded, "help_2h")
QVR_PROP_KEY(SpringSnap, "snap")
QVR_PROP_KEY(MeleeDamage, "melee_dmg")
QVR_PROP_KEY(ThrowDamage, "throw_dmg")
// Round 21, "Held props: grip modes, live offsets, palm grip, torch handle": Grip Mode 3's handle.
QVR_PROP_KEY(HandleFrom, "handle_from")      // where the fist may hold it along its long axis, units in its model's axes
QVR_PROP_KEY(HandleTo, "handle_to")          // from its origin; both 0: the butt's half, short of its end
QVR_PROP_KEY(HandleTilt, "handle_tilt")       // degrees its handle may lean off the fist's grip channel, as taken
// Round 21, "Prop size": times its model's size, about its origin (0.05x steps): as drawn, its Box3D body and hull, its
// drawn box (a solid prop's Quake box) and its lengths above (Centre of Mass, Tip, Butt, Handle, a Fixed grip's place).
QVR_PROP_KEY(Size, "size")
// Round 21, "Thrown axes: blade leniency, spin in the air": times Throwing and Physics' Spin Alignment
// (vr_throw_spin_align) for it thrown (0: its tumble as it left the hand).
QVR_PROP_KEY(SpinAlign, "spin_align")
// Round 21, "Prop settings for a held weapon; estimated mass x; the thumb round a rock": times its estimated mass (Mass 0:
// its volume, which follows its Size, times its material's density), as the hands weigh it and as Box3D's body. A Mass
// set is used as set (neither this nor the Size scales it).
QVR_PROP_KEY(MassScale, "mass_scale")
#endif
