// vr_adguns.hpp -- AD support: Arcane Dimensions' armed soldiers' guns as weapons (QC vr_ad_enemyguns.qc,
// WID_ADROCKET..WID_ADFUMIGATOR), each a model of its own made as it loads from the player's own copy of AD's monster
// model (nothing of AD's written anywhere), as limb models are (vr_limbmodel.hpp).
//
// "<monster model>#gun<skin>" (progs/mon_soldier.mdl#gun4): the gun's triangles (the vertices vr_monstermods.cpp hides
// in the soldier's deaths) as its firing frame has them, laid as Quake VR's enemy guns are (make_enemyguns.py): the
// barrel along +x, up +z, the hand's place at the origin, scaled to a weapon's length; packed from the corner of the
// Quake VR gun whose weapon settings it inherited (vr_weapons.inc slots 27-31 until vr_wofs_version 44: now the variants'). Its one skin the
// monster's <skin> (the soldier of that kind's). One frame, no effects.

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

// The guns as variants of our burst rifle (vr_adgunart.cpp; ROUND22.md, "AD guns as burst rifle variants"):
// "progs/v_gruntgun.mdl#adv<n>", n the gun (WID_ADROCKET's order: 0 rocket, 1 grenade, 2 plasma, 3 defender,
// 4 eliminator, 5 centurion, 6 pyro, 7 fumigator). Made as they load from Quake VR's v_gruntgun.mdl (its frame, grip and
// scale_origin, so slot 21's weapon settings hold them all) with each gun's own add-ons (a launch tube and its exhaust, the
// grenade launcher's barrel and drum from our v_rock.mdl, cathode tubes and coils, a pump and a shell carrier, a nozzle
// and a fuel canister) and its own skin (the rifle's recoloured, its add-ons' and decals painted: rockets, lightning,
// hazard stripes, flames, poison). Nothing written anywhere. The QC (VR_ADGun_Model) names these; the older
// "<monster model>#gun<skin>" guns above still load (saved games).
inline constexpr const char* variantSuffix = "#adv";
inline constexpr int variantCount = 8;
enum Variant : int
{
    Rocket,
    Grenade,
    Plasma,
    Defender,
    Eliminator,
    Centurion,
    Pyro,
    Fumigator
};

// The variant a model name is (-1: not one).
[[nodiscard]] int variantOf(const char* name);

// Mod_LoadModel: a variant's .mdl file (VR_HeapMalloc), or null (not a variant's name). Without its sources' files: the
// burst rifle's own file.
[[nodiscard]] byte* variantFile(const char* name, unsigned int* path_id);

// A fuelled variant's (the pyro's, the fumigator's) hose inlet in its model (the fuel canister's back end, low) and the
// way the hose leaves it (model space). False for the others.
[[nodiscard]] bool hoseInlet(int variant, glm::vec3& at, glm::vec3& dir);

// vr_adgun_variants: each variant made now (triangles, vertices, its box, whether the settings' corner held, the time).
void variants_f();

} // namespace qvr::adguns
