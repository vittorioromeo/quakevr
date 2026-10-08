// Shared immutable model facts. Exact identities and path traits do not imply model type,
// entity ownership, scale, flags, animation frame or settings: callers keep those decisions live.
#pragma once

#include "Zancle/Base/IntTypes.hpp"
#include "vr_modelmetadata.h"

struct qmodel_s;
namespace qvr::retro { enum class Category; }

namespace qvr::modelmeta
{
enum class Id
{
    Unknown = VR_MODEL_ID_Unknown,
#define QVR_MODEL_ID(name, path) name = VR_MODEL_ID_##name,
#define QVR_MODEL_TRAIT(name, match, text)
#include "vr_modelmetadata.inc"
#undef QVR_MODEL_ID
#undef QVR_MODEL_TRAIT
    Count = VR_MODEL_ID_Count
};
enum class Trait
{
#define QVR_MODEL_ID(name, path)
#define QVR_MODEL_TRAIT(name, match, text) name = VR_MODEL_TRAIT_##name,
#include "vr_modelmetadata.inc"
#undef QVR_MODEL_ID
#undef QVR_MODEL_TRAIT
    Count = VR_MODEL_TRAIT_Count
};
static_assert(static_cast<int>(Trait::Count) <= 64);

struct ModelMetadata
{
    Id id{Id::Unknown};
    za::U64 traits{};

    [[nodiscard]] bool is(Id value) const { return value != Id::Unknown && id == value; }
    [[nodiscard]] bool has(Trait value) const { return (traits & (za::U64{1} << static_cast<int>(value))) != 0; }
};

// Main thread only. Stable through other insertions; valid until map/game-dir change or model reload.
// Null has no identity/traits. Does not load a model or retain pointers into its evictable alias data.
[[nodiscard]] const ModelMetadata& get(const qmodel_s* model);
[[nodiscard]] inline bool is(const qmodel_s* model, Id id) { return get(model).is(id); }
[[nodiscard]] inline bool has(const qmodel_s* model, Trait trait) { return get(model).has(trait); }
// Quake's grenade models: progs/grenade.mdl, the mission packs' multi-grenade (progs/mervup.mdl) and proximity grenade
// (progs/proxbomb.mdl). The launchers' shots, the ogres', and the pouches' grenades (QC vr_grenade.qc VR_HandGrenade_Make:
// held, lying about, a launcher's round going in); drawn at vr_grenade_scale (vr_props.cpp drawnSize).
[[nodiscard]] inline bool isQuakeGrenade(const qmodel_s* model)
{
    const ModelMetadata& m = get(model);
    return m.is(Id::Grenade) || m.is(Id::Mervup) || m.is(Id::Proxbomb);
}
// has(model, Trait::Submodel) from the model's name alone (its rule: a map's brush submodel, "*N"), without the cache's
// lookup: for loops over every entity many times a frame (worldtrace::world). Any thread.
[[nodiscard]] bool isSubmodel(const qmodel_s* model);
// Pre-load hooks and other string-only callers: no cache, engine access, or mutable state; worker-safe.
[[nodiscard]] ModelMetadata describePath(const char* path);
[[nodiscard]] Id identifyPath(const char* path);
[[nodiscard]] const char* path(Id id);
// Live type/flags; entity overrides (held weapons, small gibs) belong to the consumer.
[[nodiscard]] retro::Category category(const qmodel_s* model);
// Whether an alias model's skin is a Quake .mdl's (skinwidth x skinheight Quake texels): a .mdl's own, or a ragdoll's
// skinned copy of one ("<model>#rag": skeletal, with the .mdl's skins and size). Other MD3 and IQM models' textures are
// painted at about four times a Quake skin's density (retro textures and lighting take a quarter of their size).
[[nodiscard]] bool quakeSkin(const qmodel_s* model, const void* aliashdr);
// Lazily copied skeletal body masks (Hands/Arms/Torso/Legs, two bits per bone, first 48 bones).
bool bodyParts(const qmodel_s* model, float out[4]);
// Body bones whose name ends in _r, as two packed 24-bit masks (wounds).
void rightBones(const qmodel_s* model, float out[2]);
// Copied bone-name map, lazy like bodyParts; returns the first or last duplicate as requested.
// Main thread only; positions, inverse matrices and animated poses remain in the caller.
[[nodiscard]] int boneIndex(const qmodel_s* model, const char* name, bool last = false);
enum class BoneRole { Other, Chest, Head };
[[nodiscard]] BoneRole boneRole(const char* name); // pure, also used while building rigs on workers
// Regression command: independent legacy predicates, cache growth, live flags, invalidation, real loaded models.
void test_f();
} // namespace qvr::modelmeta
