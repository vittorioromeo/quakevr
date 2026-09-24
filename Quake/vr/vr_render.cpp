// vr_render.cpp -- hooks into Ironwail's alias renderer (r_alias.c): per-entity mirroring,
// networked model scale/offset, per-model weapon scaling, zero blending and lighting.
//
// Ironwail builds an alias entity's model matrix as
//   R_EntityMatrix(origin, angles) * T(hdr->scale_origin) * S(hdr->scale)
// VR_AliasPreTransform runs between the two factors, VR_AliasPostTransform after them:
//   R * [mirror] * [network scale about scale_origin] * S(k) * T(offset)     (pre)
//     * T(hdr->scale_origin) * S(hdr->scale)                                   (Ironwail)
//     * S(model scale) * T(network model_offset)                               (post)

#include "vr_render.hpp"
#include "vr_engine.hpp"
#include "vr_anchor.hpp"
#include "vr_avatar.hpp"
#include "vr_client.hpp"
#include "vr_weapons.hpp"

using namespace qvr;

namespace
{

[[nodiscard]] const client::EntityVr* networkData(const entity_t* e)
{
    if(e < cl_entities || e >= cl_entities + cl_max_edicts)
    {
        return nullptr;
    }

    return client::entityVr(static_cast<int>(e - cl_entities));
}

void applyPre(const entity_t* e, bool mirrored, const glm::vec3* extra, float m[16])
{
    if(mirrored)
    {
        ApplyScale(m, 1.f, -1.f, 1.f);
    }

    if(extra)
    {
        // Tuned at the default scale, like the models they attach to.
        const float s = weapons::modelTransform(e->model).active ? weapons::offsetScale() : 1.f;
        ApplyTranslation(m, extra->x * s, extra->y * s, extra->z * s);
    }

    if(const client::EntityVr* net = networkData(e); net && net->scale != glm::vec3{0.f})
    {
        // The network scale is an offset from 1, applied about model_scale_origin.
        const glm::vec3& o = net->scaleOrigin;
        ApplyTranslation(m, -o.x, -o.y, -o.z);
        ApplyScale(m, net->scale.x + 1.f, net->scale.y + 1.f, net->scale.z + 1.f);
        ApplyTranslation(m, o.x, o.y, o.z);
    }

    if(const weapons::ModelTransform t = weapons::modelTransform(e->model); t.active)
    {
        ApplyScale(m, t.k, t.k, t.k);
        ApplyTranslation(m, t.offset.x, t.offset.y, t.offset.z);
    }
}

void applyPost(const entity_t* e, float m[16])
{
    if(const float k = avatar::modelScale(e); k > 0.f)
    {
        ApplyScale(m, k, k, k);
    }

    if(const weapons::ModelTransform t = weapons::modelTransform(e->model); t.active)
    {
        ApplyScale(m, t.scale.x, t.scale.y, t.scale.z);
    }

    if(const client::EntityVr* net = networkData(e); net && net->offset != glm::vec3{0.f})
    {
        ApplyTranslation(m, net->offset.x, net->offset.y, net->offset.z);
    }
}

} // namespace

namespace qvr::render
{

void anchorMatrix(const view::ViewEntity& ve, const glm::vec3& extra, float out[16])
{
    entity_t e = ve.ent;
    R_EntityMatrix(out, e.origin, e.angles, ENTSCALE_DEFAULT);
    applyPre(&ve.ent, ve.mirrored, &extra, out);

    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e.model));
    ApplyTranslation(out, hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]);
    ApplyScale(out, hdr->scale[0], hdr->scale[1], hdr->scale[2]);

    applyPost(&ve.ent, out);
}

} // namespace qvr::render

extern "C" int VR_AliasMirrored(const entity_t* e)
{
    const view::ViewEntity* ve = view::find(e);
    return ve && ve->mirrored;
}

extern "C" int VR_IsViewEntity(const entity_t* e)
{
    return view::find(e) != nullptr;
}

extern "C" void VR_AliasPreTransform(const entity_t* e, float matrix[16])
{
    applyPre(e, VR_AliasMirrored(e), nullptr, matrix);
}

extern "C" void VR_AliasPostTransform(const entity_t* e, float matrix[16])
{
    applyPost(e, matrix);
}

// Brush entities (the ammo and health boxes are brush models) take the networked scale and offset
// too.
extern "C" void VR_BrushTransform(const entity_t* e, float matrix[16])
{
    applyPre(e, false, nullptr, matrix);
    applyPost(e, matrix);
}

// Zero-blend data for the alias instance's spare int: zero pose (premultiplied by the vertex
// count, like Pose1/Pose2) in the low 24 bits, blend factor * 255 in the high 8 bits.
extern "C" int VR_AliasZeroBlend(const entity_t* e, const void* aliashdr, int totalverts)
{
    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(aliashdr);
    const view::ViewEntity* ve = view::find(e);
    if(!ve || ve->zeroBlend <= 0.f || hdr->poseverttype != aliashdr_t::PV_QUAKE1)
    {
        return 0;
    }

    const int blend = CLAMP(0, static_cast<int>(ve->zeroBlend * 255.f + 0.5f), 255);
    const long long pose = static_cast<long long>(anchor::zeroPose(hdr)) * totalverts;
    if(blend == 0 || pose >= (1 << 24))
    {
        return 0;
    }

    return static_cast<int>(static_cast<unsigned>(blend) << 24 | static_cast<unsigned>(pose));
}

extern "C" void VR_AliasLightModifier(const entity_t* e, float lightcolor[3])
{
    const view::ViewEntity* ve = view::find(e);
    if(ve && ve->lightMultiply)
    {
        for(int i = 0; i < 3; i++)
        {
            lightcolor[i] *= ve->lightMod[i];
        }
    }
}
