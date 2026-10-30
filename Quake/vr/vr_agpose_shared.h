// vr_agpose_shared.h -- the armed humanoids' weapon classes and pose numbers both the engine (vr_agpose.hpp's
// agpose::Class, Pose, OffMode; the toolgun's fits) and the QuakeC (QC/vr_armedgrunt.qc, vr_armedgrunt_pose.qc) use,
// defined here only. Included by both: only #defines and // comments (FTEQCC's preprocessor reads them as C's; the
// pattern of vr_melee_shared.h). The QC reads the values as floats, the engine as ints: the same numbers.

#ifndef VR_AGPOSE_SHARED_H
#define VR_AGPOSE_SHARED_H

// A held weapon's attack class (QC VR_AG_Class: how it attacks; the toolgun's fits, 4 x it).
#define VR_AG_CLASS_FISTS 0     // its fists
#define VR_AG_CLASS_MELEE 1     // a melee weapon
#define VR_AG_CLASS_GUN 2       // one shot an attack (the shotguns, the laser rifle)
#define VR_AG_CLASS_EXPLOSIVE 3 // one round an attack, kept away from its target (the launchers)
#define VR_AG_CLASS_AUTO 4      // one a frame (the nail guns, the thunderbolt, the laser cannon)
#define VR_AG_CLASS_BURST 5     // the grunts' burst rifle: three rounds

// The pose keys' weapon classes (agpose::Class). A single gun's class is its pose category (Gun the rifle, the others
// from 17); 7..16 never a class.
#define VR_AGK_FISTS 0
#define VR_AGK_GUN 1         // a rifle: one long gun
#define VR_AGK_MELEE 2       // a one-handed melee weapon
#define VR_AGK_MELEE2 3      // a two-handed one (the left hand on its second grip)
#define VR_AGK_SAW 4         // a chainsaw
#define VR_AGK_TWOMELEE 5    // a melee weapon in each hand
#define VR_AGK_TWOGUNS 6     // a gun in each hand
#define VR_AGK_PISTOL 17     // a pistol
#define VR_AGK_LAUNCHER 18   // a shoulder launcher
#define VR_AGK_SPRAYER 19    // a sprayer
#define VR_AGK_LASERCANNON 20
#define VR_AGK_WIDOWMAKER 21 // (the last category)
#define VR_AGK_NUM_CLASSES 22

// The poses (agpose::Pose); a strike's keys are its phases (0 the wind-up, 1 the blow, 2 the follow-through), a
// stance's phase 0 only.
#define VR_AGK_CARRY 0
#define VR_AGK_AIM 1
#define VR_AGK_RUN 2
#define VR_AGK_PICKUP 3
#define VR_AGK_JAB 10
#define VR_AGK_CROSS 11
#define VR_AGK_HOOK 12
#define VR_AGK_UPPERCUT 13
#define VR_AGK_OVERHEAD 14
#define VR_AGK_DIAGONAL 15
#define VR_AGK_HORIZONTAL 16
#define VR_AGK_BACKHAND 17
#define VR_AGK_SAWTHRUST 18
#define VR_AGK_DUAL 19
#define VR_AGK_SHOVE 20
#define VR_AGK_NUM_POSES 21

// A key's off hand (agpose::OffMode).
#define VR_AGK_OFF_AUTO 0 // the Weapon Holds'
#define VR_AGK_OFF_GUN 1  // on the gun even when the hold is one-handed
#define VR_AGK_OFF_FREE 2 // off the gun, hanging free
#define VR_AGK_NUM_OFF_MODES 3

// A key's code: kind * VR_AGK_KIND + class * VR_AGK_CLASS + pose * VR_AGK_POSE + phase (agpose::code).
#define VR_AGK_KIND 100000
#define VR_AGK_CLASS 1000
#define VR_AGK_POSE 10

#endif
