// vr_material_shared.h -- the material property table: what each material (VR_MAT_*, vr_impact_shared.h) is like, one
// row a material. Shared by the engine (vr_box3d_step.cpp's buoyancy) and the QuakeC (QC/vr_material.qc: VR_Mat_*,
// read by fire, lightning's conduction, hits and impacts). Only #defines and // comments (FTEQCC's preprocessor reads
// them as C's). DYNAMIC_SPAWNS.md, "Prerequisites available": add a property here, never a classname list in a system.

#ifndef VR_MATERIAL_SHARED_H
#define VR_MATERIAL_SHARED_H

// Property bits.
#define VR_MATP_FLAMMABLE 1  // catches fire (VR_Burn_Can's gate: wood, flesh)
#define VR_MATP_CONDUCTIVE 2 // carries lightning (metal, armour; a liquid always does)
#define VR_MATP_BLEEDS 4     // bleeds when hit (flesh, armoured or not: VR_Material_Fleshy)
#define VR_MATP_SINKS 8      // denser than water (metal, armour: vr_box3d_step.cpp ownFloat)
#define VR_MATP_SPLINTERS 16 // splinters when hit (wood)
#define VR_MATP_SPARKS 32    // sparks when hit (metal)

// One row a material (VR_MAT_*): its property bits.
#define VR_MATROW_NONE 0
#define VR_MATROW_STONE (VR_MATP_SINKS)
#define VR_MATROW_DIRT (VR_MATP_SINKS)
#define VR_MATROW_METAL (VR_MATP_CONDUCTIVE | VR_MATP_SINKS | VR_MATP_SPARKS)
#define VR_MATROW_WOOD (VR_MATP_FLAMMABLE | VR_MATP_SPLINTERS)
#define VR_MATROW_FLESH (VR_MATP_FLAMMABLE | VR_MATP_BLEEDS)
#define VR_MATROW_ARMOUR (VR_MATP_FLAMMABLE | VR_MATP_BLEEDS | VR_MATP_CONDUCTIVE | VR_MATP_SINKS)
#define VR_MATROW_TORCH 0   // (a wall torch: wood, but a fire source itself; never set on fire)
#define VR_MATROW_GRENADE 0 // (a grenade shots set off)

// Buoyancy: a body's own Float (1 floats as flesh) loses this much for every armour plate it wears (.vr_ar_mask's bits);
// at most VR_MAT_PLATES_SINK_MOST: a grunt in all ten plates (vr_agarmor.qc VR_AR_PIECES) floats 0.85, as a knight
// at most: the kinds drawn in armour keep their own (ownFloat: a knight 0.85, an enforcer 0.92), the lower wins.
#define VR_MAT_PLATE_SINK 0.015
#define VR_MAT_PLATES_SINK_MOST 0.3

#endif
