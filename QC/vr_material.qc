// vr_material.qc -- the material property table's QC side (DYNAMIC_SPAWNS.md, "Prerequisites available"): what a
// thing is made of (VR_MaterialOf, vr_crates.qc: one VR_MAT_* for every entity) and what that material is like (its
// row in Quake/vr/vr_material_shared.h: flammable, conductive, bleeds, sinks, splinters, sparks). Systems ask these,
// never a classname list of their own: fire (VR_Burn_Can), hits (VR_ImpactFx, VR_Material_Fleshy), lightning's
// conduction (VR_Shock_Conducts), buoyancy (the engine reads the same rows and the worn plates).

#include "../Quake/vr/vr_material_shared.h"

float(entity e) VR_MaterialOf; // (vr_crates.qc)

// Material `m`'s row: its VR_MATP_* bits.
float(float m) VR_Mat_Row =
{
    if(m == VR_MAT_STONE) return VR_MATROW_STONE;
    if(m == VR_MAT_DIRT) return VR_MATROW_DIRT;
    if(m == VR_MAT_METAL) return VR_MATROW_METAL;
    if(m == VR_MAT_WOOD) return VR_MATROW_WOOD;
    if(m == VR_MAT_FLESH) return VR_MATROW_FLESH;
    if(m == VR_MAT_ARMOUR) return VR_MATROW_ARMOUR;
    if(m == VR_MAT_TORCH) return VR_MATROW_TORCH;
    if(m == VR_MAT_GRENADE) return VR_MATROW_GRENADE;
    return VR_MATROW_NONE;
};

// Whether material `m` has property `xBit` (VR_MATP_*).
float(float m, float xBit) VR_Mat_Is = { return (VR_Mat_Row(m) & xBit) != 0; };

// Whether `e` (what it is made of: VR_MaterialOf) has property `xBit` (VR_MATP_*).
float(entity e, float xBit) VR_Mat_Has = { return VR_Mat_Is(VR_MaterialOf(e), xBit); };

// Its armour plates worn (.vr_ar_mask's bits: vr_agarmor.qc; 0 for what wears none).
float(entity e) VR_Mat_Plates =
{
    float n = 0;
    float m = e.vr_ar_mask;
    while(m > 0)
    {
        n = n + (m & 1);
        m = floor(m / 2);
    }
    return n;
};
