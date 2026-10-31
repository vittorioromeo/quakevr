// vr_region_shared.h -- where on a body a hit struck: one vocabulary for the QuakeC (positional damage's part,
// PositionalRegion and VR_RegionPick, weapons.qc; the hit record's hit_zone, combat.qc) and the engine (the hit
// reaction's bone regions, vr_hitreact.cpp: Region, regionOf). The part crosses as a number (QVR_SVC_HITREACT's part),
// so both sides read these. Only #defines and // comments (FTEQCC's preprocessor reads them as C's). Design review 6
// ("one location vocabulary"; ROUND23.md "One hit record").

#ifndef VR_REGION_SHARED_H
#define VR_REGION_SHARED_H

#define VR_REGION_NONE -1 // positional damage does not apply (no head, the option off), or not known
#define VR_REGION_BODY 0  // the torso: what is none of the others
#define VR_REGION_HEAD 1  // the head sphere (and a rig's head and jaw bones)
#define VR_REGION_ARM 2   // an extremity: far out from its middle (a rig's arm bones)
#define VR_REGION_LEG 3   // below its origin (a rig's leg bones)
#define VR_REGION_COUNT 4

#endif
