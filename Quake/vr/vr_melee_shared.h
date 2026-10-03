// vr_melee_shared.h -- the melee constants both the engine's melee builtins (vr_builtins.cpp) and the QuakeC
// (QC/vr_melee.qc) use, defined here only. Included by both: only #defines and // comments (FTEQCC's preprocessor
// reads them as C's). The QC reads the values as floats; the engine casts them to float, the same values.

#ifndef VR_MELEE_SHARED_H
#define VR_MELEE_SHARED_H

// The poses a hand's blow looks back through (VR_Melee_Track): a ring of this many in QC's .mh_hfar[], .mh_hgrip[],
// .mh_hwrist[] and .mh_hdt[]. The engine checks the progs' arrays are this long when they load (vr_progs.cpp).
#define VR_MELEE_HISTORY 24

#define VR_MELEE_RUN_TIME 0.3     // s looked back at most for how far a blow came (meleerun; VR_MELEE_RUN)
#define VR_MELEE_WIGGLE_BACK 0.6  // a weapon whose grip went the other way at this share of its speed now or more,
#define VR_MELEE_WIGGLE_TIME 0.25 // within this long (s), is wiggled, not swung (the author's axe wiggled at the dummy 0.75
                                  // and more, within 0.08-0.2 s; his cuts, stabs, pommel and gun strikes 0.46 at most). Not
                                  // a fist's: a punch pulled back and thrown again is a blow (his punches 0.41 at most)
#define VR_MELEE_WRIST_TIME 0.12  // s over which vr_melee_wrist_speed averages the wrist's travel (a wiggle's half-period)

#endif
