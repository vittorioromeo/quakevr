// vr_props.inc -- per-prop settings: what a thing a hand carries weighs and how it is held (Held Object Offsets; see
// vr_props.hpp and docs/vr-port/ROUND21.md, "Weight: spring model, stamina, held object offsets; explosive boxes").
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
QVR_PROP_KEY(GripMode, "grip", "0")          // 0: held where it is taken; 1: always the same way (Grip X..Roll)
QVR_PROP_KEY(GripX, "grip_x", "0")           // with Grip Mode 1: the model's origin in the right hand's frame, units
QVR_PROP_KEY(GripY, "grip_y", "0")           // (x forward, y left, z up; mirrored for the left hand)
QVR_PROP_KEY(GripZ, "grip_z", "0")
QVR_PROP_KEY(GripPitch, "grip_pitch", "0")   // and its turn there, degrees (as the model's angles off the hand's)
QVR_PROP_KEY(GripYaw, "grip_yaw", "0")
QVR_PROP_KEY(GripRoll, "grip_roll", "0")
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
#endif
