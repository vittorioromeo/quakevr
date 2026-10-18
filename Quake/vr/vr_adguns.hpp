// vr_adguns.hpp -- AD support: Arcane Dimensions' armed soldiers' guns as weapons (QC vr_ad_enemyguns.qc,
// WID_ADROCKET..WID_ADFUMIGATOR), each a model of its own made as it loads from the player's own copy of AD's monster
// model (nothing of AD's written anywhere), as limb models are (vr_limbmodel.hpp).
//
// "<monster model>#gun<skin>" (progs/mon_soldier.mdl#gun4): the gun's triangles (the vertices vr_monstermods.cpp hides
// in the soldier's deaths) as its firing frame has them, laid as Quake VR's enemy guns are (make_enemyguns.py): the
// barrel along +x, up +z, the hand's place at the origin, scaled to a weapon's length; packed from the corner of the
// Quake VR gun whose weapon settings it inherits (vr_weapons.inc, slots 27-31), so they hold it. Its one skin the
// monster's <skin> (the soldier of that kind's). One frame, no effects.
//
// "<AD weapon model>#gun0" (progs/w_dcrossbow.mdl#gun0, the crossbow knight's crossbow): one of AD's own weapon models
// authored sideways, whole and unscaled, turned a quarter about z to face +x as our guns do (vr_adguns.cpp
// turnedModels), so its weapon slot's offsets turn it about the same axes as every other gun's.

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"

namespace qvr::adguns
{

inline constexpr const char* suffix = "#gun";

struct Built
{
    za::Vector<byte> file;
    int verts{0}, tris{0};
    glm::vec3 lo{0.f}, hi{0.f}; // its box (model units: the hand's place at 0 0 0)
    glm::vec3 muzzle{0.f};      // the middle of its front end (vr_weapons.inc's MuzzleAnchor)
    bool cornerKept{false};     // packed from the settings' corner (else a vertex past it moved it: placement off)
};

// The gun of `base` (one of AD's soldiers' models, the release measured) in skin `skin`, from its file's bytes.
bool build(const char* base, int skin, const byte* src, size_t size, Built& out, const char*& why);

// Mod_LoadModel (VR_DerivedModelFile): the .mdl file of a gun model named so (allocated with VR_HeapMalloc), or null.
[[nodiscard]] byte* derivedFile(const char* name, unsigned int* path_id);

// The weapon settings (vr_weapons.cpp): whether a slot's ID `id` is gun model `name`'s base name, whatever its skin
// ("progs/mon_soldier.mdl#gun" names "progs/mon_soldier.mdl#gun4").
[[nodiscard]] bool slotNames(const char* id, const char* name);

// vr_adgun_models: each gun made now, its triangles, box and muzzle (the weapon settings' anchors).
void info_f();

} // namespace qvr::adguns
