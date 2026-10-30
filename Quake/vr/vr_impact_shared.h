// vr_impact_shared.h -- the impact table's two keys (vr_impacts.cpp: material x hit kind -> particles, decal, sound),
// shared by the engine (impacts::Material, impacts::HitKind) and the QuakeC (VR_MaterialOf, VR_ImpactFx,
// impactsound: QC/vr_crates.qc). Only #defines and // comments (FTEQCC's preprocessor reads them as C's). One row of
// the table is one material hit one way (design_review.md proposal 4; ROUND23.md "One impact-effect table").

#ifndef VR_IMPACT_SHARED_H
#define VR_IMPACT_SHARED_H

// What a hit surface or thing is made of. A world surface: its texture's (impacts.cpp, the client's wall hits:
// none, stone, dirt, metal, wood). A thing hit (QC VR_MaterialOf): metal, wood, flesh, armour, a torch, a grenade.
#define VR_MAT_NONE 0    // not a surface (sky, liquids, clip), or none near
#define VR_MAT_STONE 1   // stone, brick, concrete; a blunt blow on the world or on what takes no damage
#define VR_MAT_DIRT 2    // ground, grass, mud, sand, rock faces
#define VR_MAT_METAL 3   // an explosive box, a fuel tank, a door, a lift, a shootable brush (VR_Hit_Metal)
#define VR_MAT_WOOD 4    // a crate, its pieces, a target board (VR_Crate_Wooden)
#define VR_MAT_FLESH 5   // a living thing, a corpse, a gib; whatever else takes a hit
#define VR_MAT_ARMOUR 6  // flesh in mail or plate (VR_Hit_Armoured: knights, enforcers, AD's plated monsters)
#define VR_MAT_TORCH 7   // a wall torch (VR_WallTorch_IsTorch: its own embers)
#define VR_MAT_GRENADE 8 // a grenade shots set off (.vr_shootnade)
#define VR_MAT_COUNT 9

// How it was hit.
#define VR_HIT_BULLET 0    // a hitscan pellet (TraceAttack; the world's: TE_GUNSHOT)
#define VR_HIT_NAIL 1      // a projectile's touch (spawn_touchblood; the world's: TE_SPIKE)
#define VR_HIT_SUPERNAIL 2 // (the world's: TE_SUPERSPIKE; a thing's: as a nail)
#define VR_HIT_BOLT 3      // the thunderbolt's beam playing on a surface (the client's haze)
#define VR_HIT_PELLET 4    // a projectile pellet on no flesh (VR_HitNonFlesh: AD's pellets, AD's touches)
#define VR_HIT_BLOW 5      // a melee blow (VR_HitBlood); SpawnBlood's blood; a blunt blow's second layer (sounds)
#define VR_HIT_BLADE 6     // a blade's blow: its sound alone (the clang on metal)
#define VR_HIT_SPLASH 7    // a blow sprayed every way (VR_HitBloodSplash, SpawnBloodSplash)
#define VR_HIT_SAW 8       // the chainsaw's tick (VR_Chainsaw)
#define VR_HIT_BASH 9      // a blunt blow's or a thrown thing's knock (on metal); a thrown thing's splinters
#define VR_HIT_COUNT 10

// impactsound(material, kind, slot)'s slots.
#define VR_IMPACT_SOUND 0 // the hit's own sound (a shot's tink, a blade's clang, a blunt blow's metal knock)
#define VR_IMPACT_LAYER 1 // a blunt blow's second layer (VR_Blunt_HitLayer: soft, wood, flesh)
#define VR_IMPACT_OVER 2  // and what plays over it (armour's light metal)

#endif
