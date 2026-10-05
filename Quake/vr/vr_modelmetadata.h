// C access to the same identities and cached traits used by the C++ VR module.
#pragma once
struct qmodel_s;
enum vr_model_id
{
    VR_MODEL_ID_Unknown,
#define QVR_MODEL_ID(name, path) VR_MODEL_ID_##name,
#define QVR_MODEL_TRAIT(name, match, text)
#include "vr_modelmetadata.inc"
#undef QVR_MODEL_ID
#undef QVR_MODEL_TRAIT
    VR_MODEL_ID_Count
};
enum vr_model_trait
{
#define QVR_MODEL_ID(name, path)
#define QVR_MODEL_TRAIT(name, match, text) VR_MODEL_TRAIT_##name,
#include "vr_modelmetadata.inc"
#undef QVR_MODEL_ID
#undef QVR_MODEL_TRAIT
    VR_MODEL_TRAIT_Count
};
#ifdef __cplusplus
extern "C" {
#endif
enum vr_model_id VR_ModelIdentity(const struct qmodel_s* model);
int VR_ModelHasTrait(const struct qmodel_s* model, enum vr_model_trait trait);
void VR_ModelMetadataChanged(const struct qmodel_s* model);
#ifdef __cplusplus
}
#endif
