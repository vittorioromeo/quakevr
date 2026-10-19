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
#include "vr_collectfx.hpp"
#include "vr_cvars.hpp"
#include "vr_handrig.hpp"
#include "vr_props.hpp"
#include "vr_ragdoll.hpp"
#include "vr_hitreact.hpp"
#include "vr_flamewobble.hpp"
#include "vr_brainjiggle.hpp"
#include "vr_toolgun.hpp"
#include "vr_hotblade.hpp"
#include "vr_main.hpp"
#include "vr_walltorch.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Algorithm/Unique.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/gtc/type_ptr.hpp>


using namespace qvr;

namespace
{

[[nodiscard]] const client::EntityVr* networkData(const entity_t* e)
{
    if(e < cl_entities || e >= cl_entities + cl_max_edicts)
    {
        if(const view::ViewEntity* ve = view::find(e); ve && ve->netEntity > 0)
        {
            return client::entityVr(ve->netEntity); // (a part of a world entity: a lying gun's magazine)
        }
        if(const client::EntityVr* ghost = toolgun::ghostVr(e))
        {
            return ghost; // (the toolgun's ghost of an item: as it will be once placed)
        }
        return collectfx::entityVr(e); // (a thing put away, drawn going in: the item's own)
    }

    return client::entityVr(static_cast<int>(e - cl_entities));
}

void applyPre(const entity_t* e, bool mirrored, const glm::vec3* extra, const client::EntityVr* net, float m[16])
{
    const weapons::ModelTransform t = weapons::modelTransform(e->model);

    // A thing put away, shrinking into a holster or pouch (vr_collectfx.cpp), about its origin: all of it.
    if(const float k = collectfx::shrink(e); k != 1.f)
    {
        ApplyScale(m, k, k, k);
    }

    // A prop's Size (Held Object Offsets), about its origin: everything below grows with it.
    if(const float size = props::drawnSize(e->model); size != 1.f)
    {
        ApplyScale(m, size, size, size);
    }

    // A swung torch's flame, flattened and stretched back (vr_walltorch.cpp), about its origin.
    if(glm::vec3 k; walltorch::stretch(e, k))
    {
        ApplyScale(m, k.x, k.y, k.z);
    }

    if(mirrored)
    {
        ApplyScale(m, 1.f, -1.f, 1.f);
    }

    if(const view::ViewEntity* ve = view::find(e); ve && ve->scale != glm::vec3{1.f})
    {
        ApplyScale(m, ve->scale.x, ve->scale.y, ve->scale.z);
    }

    if(extra)
    {
        // Tuned at the default scale, like the models they attach to.
        const float s = t.active ? weapons::offsetScale() : 1.f;
        ApplyTranslation(m, extra->x * s, extra->y * s, extra->z * s);
    }

    if(net && net->scale != glm::vec3{0.f})
    {
        // The network scale is an offset from 1, applied about model_scale_origin.
        const glm::vec3& o = net->scaleOrigin;
        ApplyTranslation(m, -o.x, -o.y, -o.z);
        ApplyScale(m, net->scale.x + 1.f, net->scale.y + 1.f, net->scale.z + 1.f);
        ApplyTranslation(m, o.x, o.y, o.z);
    }

    if(t.active)
    {
        ApplyScale(m, t.k, t.k, t.k);
        ApplyTranslation(m, t.offset.x, t.offset.y, t.offset.z);
    }
}

void applyPost(const entity_t* e, const client::EntityVr* net, float m[16])
{
    if(const float k = avatar::modelScale(e); k > 0.f)
    {
        ApplyScale(m, k, k, k);
    }

    if(const weapons::ModelTransform t = weapons::modelTransform(e->model); t.active)
    {
        ApplyScale(m, t.scale.x, t.scale.y, t.scale.z);
    }

    if(net && net->offset != glm::vec3{0.f})
    {
        ApplyTranslation(m, net->offset.x, net->offset.y, net->offset.z);
    }
}

} // namespace

namespace qvr::render
{

namespace
{

void entityMatrixAt(const entity_t& e, const float* at, const float* turn, bool mirrored, int scale, const glm::vec3& extra,
    const client::EntityVr* net, float out[16])
{
    vec3_t origin, angles; // R_EntityMatrix takes them non-const
    VectorCopy(at, origin);
    VectorCopy(turn, angles);
    R_EntityMatrix(out, origin, angles, static_cast<unsigned char>(scale));
    applyPre(&e, mirrored, &extra, net, out);

    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e.model));
    ApplyTranslation(out, hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]);
    ApplyScale(out, hdr->scale[0], hdr->scale[1], hdr->scale[2]);

    applyPost(&e, net, out);
}

void entityMatrixWith(const entity_t& e, bool mirrored, int scale, const glm::vec3& extra, const client::EntityVr* net,
    float out[16])
{
    entityMatrixAt(e, e.origin, e.angles, mirrored, scale, extra, net, out);
}

} // namespace

void drawnPlace(const entity_t& e, glm::vec3& origin, glm::vec3& angles)
{
    origin = glm::vec3{e.origin[0], e.origin[1], e.origin[2]};
    angles = glm::vec3{e.angles[0], e.angles[1], e.angles[2]};
    if(!r_lerpmove.value || !(e.lerpflags & LERP_MOVESTEP) || (e.lerpflags & LERP_RESETMOVE))
    {
        return;
    }
    const glm::vec3 current{e.currentorigin[0], e.currentorigin[1], e.currentorigin[2]};
    const glm::vec3 currentAngles{e.currentangles[0], e.currentangles[1], e.currentangles[2]};
    if(current != origin || currentAngles != angles)
    {
        vec3_t startOrigin, startAngles;
        R_MoveLerpStart(&e, startOrigin, startAngles); // a move starting this frame: at its start (where the last is drawn)
        origin = glm::vec3{startOrigin[0], startOrigin[1], startOrigin[2]};
        angles = glm::vec3{startAngles[0], startAngles[1], startAngles[2]};
        return;
    }
    const float blend = R_MoveLerpBlend(&e);
    const glm::vec3 previous{e.previousorigin[0], e.previousorigin[1], e.previousorigin[2]};
    const glm::vec3 previousAngles{e.previousangles[0], e.previousangles[1], e.previousangles[2]};
    origin = previous + (current - previous) * blend;
    glm::vec3 d = currentAngles - previousAngles;
    for(int i = 0; i < 3; i++)
    {
        d[i] = d[i] > 180.f ? d[i] - 360.f : d[i] < -180.f ? d[i] + 360.f : d[i];
    }
    angles = previousAngles + d * blend;
}

void drawnEntityMatrix(const entity_t& e, int scale, float out[16])
{
    glm::vec3 origin, angles;
    drawnPlace(e, origin, angles);
    const float at[3] = {origin.x, origin.y, origin.z};
    const float turn[3] = {angles.x, angles.y, angles.z};
    entityMatrixAt(e, at, turn, false, scale, glm::vec3{0.f}, networkData(&e), out);
}

void anchorMatrix(const view::ViewEntity& ve, const glm::vec3& extra, float out[16])
{
    // (A copy of a world entity, or a part of one, with its networked transform: ve.netEntity.)
    entityMatrixWith(ve.ent, ve.mirrored, ENTSCALE_DEFAULT, extra,
        ve.netEntity > 0 ? client::entityVr(ve.netEntity) : networkData(&ve.ent), out);
}

void entityMatrix(const entity_t& e, bool mirrored, int scale, const glm::vec3& extra, float out[16])
{
    entityMatrixWith(e, mirrored, scale, extra, networkData(&e), out);
}

} // namespace qvr::render

extern "C" int VR_AliasMirrored(const entity_t* e)
{
    const view::ViewEntity* ve = view::find(e);
    return ve && ve->mirrored;
}

extern "C" int VR_AliasCullPosedOn(const entity_t* e, int depth)
{
    // (your body's light is kept as it is drawn in an eye, for the bare hands: drawn there whatever its box)
    const int mode = CLAMP(0, static_cast<int>(vr_alias_cull_posed.value), 2);
    return mode && (!e || depth || !view::keepsBareLight(e)) ? mode : 0;
}

extern "C" int VR_AliasDepthPrepass(const entity_t* e)
{
    if(vr_body_prepass.value == 0.f)
    {
        return 0;
    }
    const int group = VR_ViewEntityGroup(e);
    return group == VR_DRAW_BODY || group == VR_DRAW_MIRROR;
}

extern "C" int VR_IsViewEntity(const entity_t* e)
{
    return view::find(e) != nullptr;
}

extern "C" int VR_ViewEntityGroup(const entity_t* e)
{
    return view::drawGroup(e);
}

// What you hold, right at the eyes (vr_nearclip_held): 1 its parts nearer than the near plane are drawn at it (depth
// clamp) rather than cut away, which showed the world through a shouldered gun; 2 also both sides of it while the eye
// is inside its bounds, so a gun pushed into the face blocks the view with its inside. `matrix` maps its vertices to
// the world (R_DrawAliasModel's, all of VR_AliasPreTransform's and VR_AliasPostTransform's in it).
extern "C" int VR_AliasNearEye(const entity_t* e, const float matrix[16], const void* aliashdr)
{
    const int mode = static_cast<int>(vr_nearclip_held.value);
    if(mode <= 0 || !VR_RenderingEye() || !VR_IsViewEntity(e))
    {
        return 0;
    }
    if(mode < 2)
    {
        return 1;
    }

    // The eye in the model's own space (the vertices' before their packing: scale and scale_origin), against its
    // bounds over every frame.
    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(aliashdr);
    const glm::vec4 v = glm::inverse(glm::make_mat4(matrix)) *
                        glm::vec4{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2], 1.f};
    constexpr float margin = 0.25f; // model units: an eye just outside a face
    for(int i = 0; i < 3; ++i)
    {
        const float p = v[i] * hdr->scale[i] + hdr->scale_origin[i];
        if(p < e->model->mins[i] - margin || p > e->model->maxs[i] + margin)
        {
            return 1;
        }
    }
    return 3;
}

extern "C" void VR_AliasPreTransform(const entity_t* e, float matrix[16])
{
    applyPre(e, VR_AliasMirrored(e), nullptr, networkData(e), matrix);
    hitreact::tiltModel(e, matrix); // a living monster struck, with no rig to kick: tilted (vr_hitreact.cpp)
    flamewobble::leanModel(e, matrix); // a map flame stirred by a fast hand (vr_flamewobble.cpp)
    brainjiggle::squashModel(e, matrix); // a brain chunk wobbling from a knock (vr_brainjiggle.cpp)
}

extern "C" void VR_AliasPostTransform(const entity_t* e, float matrix[16])
{
    if(ragdoll::drawMatrix(e, matrix))
    {
        return; // a ragdoll: placed where its bones are given from (vr_ragdoll.cpp)
    }
    applyPost(e, networkData(e), matrix);
}

// Brush entities (the ammo and health boxes are brush models) take the networked scale and offset
// too.
extern "C" float VR_EntityScale(const entity_t* e)
{
    return (e->scale ? ENTSCALE_DECODE(e->scale) : 1.f) * (e->model ? props::drawnSize(e->model) : 1.f);
}

extern "C" void VR_BrushTransform(const entity_t* e, float matrix[16])
{
    const client::EntityVr* net = networkData(e);
    applyPre(e, false, nullptr, net, matrix);
    applyPost(e, net, matrix);
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

namespace
{

// The gloves' shells `e`'s model has (vr_handrig.cpp gloveShells: surfaces after its first; 0 none).
[[nodiscard]] int gloveShells(const entity_t* e)
{
    return e->model && e->model->type == mod_alias ? handrig::gloveShellSurfaces(e->model->name) : 0;
}

// The shell drawn on `e` (its surface's number: 1 the hand's fingerless, 2 its full, 1 a bare forearm's cuff), or 0:
// no gloves, vr_glove_thickness 0, or nothing of them on `e` (the bracers over the wrist).
[[nodiscard]] int gloveShellWorn(const entity_t* e)
{
    const int shells = vr_glove_thickness.value > 0.f ? gloveShells(e) : 0;
    float glove[4], cuff[4];
    const int kind = shells ? view::gloveLook(e, glove, cuff) : 0;
    return kind == 1 && shells == 2 ? static_cast<int>(glove[3]) : kind == 2 && shells == 1 && cuff[0] > 0.f ? 1 : 0;
}

} // namespace

extern "C" void* VR_AliasSurfaceFirst(const entity_t* e, void* mainhdr)
{
    auto* hdr = static_cast<aliashdr_t*>(mainhdr);
    for(int i = gloveShells(e) ? gloveShellWorn(e) : 0; i > 0; i--)
    {
        hdr = Mod_NextSurface(hdr);
    }
    return hdr;
}

extern "C" void* VR_AliasSurfaceNext(const entity_t* e, void* mainhdr, void* hdr)
{
    auto* main = static_cast<aliashdr_t*>(mainhdr);
    if(!gloveShells(e))
    {
        return Mod_NextSurface(static_cast<aliashdr_t*>(hdr));
    }
    return hdr != main ? main : nullptr; // the shell, then the model's own (one) mesh
}

// R_DrawAliasModel_Real: the instance's Quake VR data (vraliasinstance_t). kind 1: drawn as usual; 0: r_showtris or
// r_showbboxes' skeletons; 2: a shadow map's caster, depth only: nothing of the light, glows or surface (all zero).
extern "C" void VR_AliasInstance(const entity_t* e, const float matrix[16], const void* aliashdr, int kind,
    vraliasinstance_t* out)
{
    out->portalclip[0] = out->portalclip[1] = out->portalclip[2] = 0.f;
    out->portalclip[3] = 1.f;
    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(aliashdr);
    // a glove's shell's thickness (vr_glove_thickness, mm), in the hand's model units (its casters' too)
    const float shell = gloveShellWorn(e) ? CLAMP(0.f, vr_glove_thickness.value, 4.f) *
            (gloveShells(e) == 2 ? handrig::rigUnitsPerMm : handrig::bodyUnitsPerMm) : 0.f;
    if(kind == 2)
    {
        ZA_MEMSET(out, 0, sizeof(*out));
        out->portalclip[3] = 1.f;
        out->gloveshell[0] = shell;
        return;
    }
    out->gloveshell[0] = shell;
    out->gloveshell[1] = out->gloveshell[2] = out->gloveshell[3] = 0.f;
    const bool standard = kind == 1;
    const bool lit = standard && !r_fullbright_cheatsafe && !r_lightmap_cheatsafe;
    VR_AliasLightDir(e, out->lightdir);
    VR_AliasAmbient(e, matrix, hdr, lit, &out->ambient[0][0]); // directional ambient (vr_model_ambient_dir)
    VR_AliasMorph(e, hdr, &out->ambient[0][0]); // a gun morphing into its other ammo's model
    out->glow[0] = VR_EntityGlow(e);
    gltexture_t* skin = hdr->gltextures[e->skinnum >= 0 && e->skinnum < hdr->numskins ? e->skinnum : 0][0];
    const int authored = TexMgr_NormalMapAuthored(skin); // its skin's normal map is an authored file's
    // the shader's shading on a par with the world (+), its bumps (vr_normalmap_models)
    out->glow[1] = (VR_ModelLightParity() ? 1.f : -1.f) * (1.f + VR_ModelBumps(e, authored));
    out->glow[2] = VR_EntityFullbrightBoost(e); // the held weapons' sights glow (vr_weapon_glow)
    if(standard) // its parallax depth in units; an authored map's heights (vr_parallax_authored) negative: the shader
    {            // walks them in all the world's steps
        const int heights = TexMgr_NormalMapParallax(skin);
        out->glow[3] = VR_ParallaxDepth(e, matrix, hdr->scale, heights) * (heights == 2 ? -1.f : 1.f);
    }
    else
    {
        out->glow[3] = 0.f;
    }
    ZA_MEMSET(out->surface, 0, sizeof(out->surface)); // rim light and reflections (vr_rim_light, vr_weapon_reflections)
    if(lit)
    {
        VR_AliasSurface(e, out->surface);
    }
    VR_AliasAO(e, out->ao); // dynamic ambient occlusion (vr_ao.cpp)
    out->ao[2] = VR_ModelNormalMapScale(authored); // how much its normal map bends the normal (authored ones their own strength)
    if(standard) // wounds painted on it (vr_wounds.cpp), and its wet coat
    {
        VR_AliasWound(e, out->wound, out->woundside);
        VR_AliasWetCoat(e, &out->gloveshell[2]);
    }
    else
    {
        ZA_MEMSET(out->wound, 0, sizeof(out->wound));
        ZA_MEMSET(out->woundside, 0, sizeof(out->woundside));
    }
    if(standard) // flesh dents (vr_dents.cpp)
    {
        VR_AliasDent(e, hdr, out->dent, out->dentscale);
    }
    else
    {
        ZA_MEMSET(out->dent, 0, sizeof(out->dent));
        ZA_MEMSET(out->dentscale, 0, sizeof(out->dentscale));
    }
    VR_RetroAlias(e, hdr, standard ? 1 : 0, out->retro, out->retropart); // retro textures (vr_retro.cpp): its set, its skin's Quake size
    if(standard) // bare forearms: the hands in your body's light (its direction and the light round it)
    {
        view::bareShade(e, out->lightdir, out->ambient);
    }
    if(!standard || !view::skinTint(e, out->skintint, out->skintintrange)) // bare forearms' ends to the hand's skin
    {
        ZA_MEMSET(out->skintint, 0, sizeof(out->skintint));
        ZA_MEMSET(out->skintintrange, 0, sizeof(out->skintintrange));
    }
    ZA_MEMSET(out->skintone, 0, sizeof(out->skintone));
    ZA_MEMSET(out->glove, 0, sizeof(out->glove));
    ZA_MEMSET(out->glovecuff, 0, sizeof(out->glovecuff));
    if(standard)
    {
        (void)view::skinTone(e, out->skintone);
        (void)view::gloveLook(e, out->glove, out->glovecuff);
        if(out->glove[3] > 0.f) // its painted leather (vr_glove_paint): Glove.w's 4s
        {
            const int paint = render::glovePaintFlags(e);
            out->glove[3] += 4.f * static_cast<float>(paint);
            out->gloveshell[1] = paint == 2 ? CLAMP(0.f, vr_glove_relief.value, 2.f) : 0.f; // its relief's normal map's strength
        }
    }
    if(out->glow[0] > 3.5f) // an incandescent blade glows on its blade only (vr_hotblade.cpp)
    {
        hotblade::bladePlane(e, matrix, out->hotblade);
    }
    else
    {
        ZA_MEMSET(out->hotblade, 0, sizeof(out->hotblade));
    }
    out->retro[3] = standard ? VR_RetroLightSkinScale(e->model, hdr, e->skinnum) : 0.f; // retro lighting (vr_retrolight.cpp): its Quake texels' share
}

// Muzzle flashes and the zero blend (round 20). A view model's flash (Quake's nailguns and launchers, the shotguns'
// generated flames: improve_weapons.py flame) is triangles collapsed into one point in frame 0 that open out in the
// firing frames. The two-handed recoil damping pulls every vertex towards frame 0, which shrank the flash with the
// recoil; the alias shader moves a flash's vertices instead by the damping of the gun vertex nearest to that point
// (frame 0): the flash keeps its size, carried by the steadied gun. Found here: the vertices of triangles collapsed
// into one point in frame 0 that spread over 2 units in some frame (tiny parts rounded into a point stay under 1).
extern "C" void VR_AliasFlameRefs(const void* aliashdr, unsigned short* refs)
{
    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(aliashdr);
    const int n = hdr->numverts_vbo;
    za::fill(refs, refs + n, static_cast<unsigned short>(0));
    if(hdr->poseverttype != aliashdr_t::PV_QUAKE1 || hdr->numframes < 1 || hdr->numposes < 2 || n <= 0)
    {
        return;
    }

    const auto* base = reinterpret_cast<const byte*>(hdr);
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
    const auto* verts = reinterpret_cast<const trivertx_t*>(base + hdr->vertexes);
    const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
    const trivertx_t* zero = verts + hdr->numverts * hdr->frames[0].firstpose;
    const auto at = [&](const trivertx_t* pose, int v) {
        const byte* p = pose[desc[v].vertindex].v;
        return glm::vec3{p[0] * hdr->scale[0], p[1] * hdr->scale[1], p[2] * hdr->scale[2]};
    };
    const auto key = [&](int v) {
        const byte* p = zero[desc[v].vertindex].v;
        return p[0] | p[1] << 8 | p[2] << 16;
    };

    ankerl::unordered_dense::map<int, za::Vector<int>> clusters; // by their point in frame 0
    for(int t = 0; t + 2 < hdr->numindexes; t += 3)
    {
        const int a = indexes[t], b = indexes[t + 1], c = indexes[t + 2];
        if(a < n && b < n && c < n && key(a) == key(b) && key(a) == key(c))
        {
            za::Vector<int>& cl = clusters[key(a)];
            cl.pushBackMultiple(a, b, c);
        }
    }

    za::Vector<bool> flash(static_cast<size_t>(n), false);
    za::Vector<int> points; // a vertex of each flash
    for(auto& [k, cl] : clusters)
    {
        za::quickSort(cl.begin(), cl.end());
        cl.erase(za::unique(cl.begin(), cl.end()), cl.end());
        float spread = 0.f;
        for(int p = 0; p < hdr->numposes && spread < 2.f; p++)
        {
            const trivertx_t* pose = verts + hdr->numverts * p;
            const glm::vec3 o = at(pose, cl[0]);
            for(int v : cl)
            {
                spread = za::max(spread, glm::distance(at(pose, v), o));
            }
        }
        if(cl.size() >= 3 && spread >= 2.f)
        {
            for(int v : cl)
            {
                flash[static_cast<size_t>(v)] = true;
            }
            points.pushBack(cl[0]);
        }
    }

    for(int f : points)
    {
        // The gun vertex nearest to the flash's point in frame 0.
        const glm::vec3 p = at(zero, f);
        int best = -1;
        float bestDist = 1e30f;
        for(int v = 0; v < n; v++)
        {
            const float d = flash[static_cast<size_t>(v)] ? 1e30f : glm::distance(at(zero, v), p);
            if(d < bestDist)
            {
                bestDist = d;
                best = v;
            }
        }
        if(best < 0 || best >= 0x7FFF)
        {
            continue;
        }
        for(int v = 0; v < n; v++)
        {
            if(flash[static_cast<size_t>(v)] && key(v) == key(f))
            {
                refs[v] = static_cast<unsigned short>(best + 1);
            }
        }
    }
}

// A gun morphing into its other ammo's model (vr_view.cpp, vr_weapon_morph_time): both models drawn, the one going
// out dissolving where the one coming in appears, over a noise in the model's own units (the two models share them:
// improve_weapons_alt.py), a glowing seam between. Into the instance's spare Ambient[2..5].w: the morph (+ in, - out;
// its edge's colour 2 * kind + its progress) and the model's scale (quantised steps to units).
extern "C" void VR_AliasMorph(const entity_t* e, const void* aliashdr, float ambient[24])
{
    const view::ViewEntity* ve = view::find(e);
    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(aliashdr);
    if(!ve || ve->morph == 0.f || hdr->poseverttype != aliashdr_t::PV_QUAKE1)
    {
        return;
    }
    ambient[2 * 4 + 3] = ve->morph;
    ambient[3 * 4 + 3] = hdr->scale[0];
    ambient[4 * 4 + 3] = hdr->scale[1];
    ambient[5 * 4 + 3] = hdr->scale[2];
}

extern "C" void VR_AliasLightOrigin(const entity_t* e, float origin[3])
{
    const view::ViewEntity* ve = view::find(e);
    for(int i = 0; i < 3; i++)
    {
        origin[i] = e->origin[i] + (ve ? ve->lightShift[i] : 0.f);
    }
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
    view::bareLight(e, lightcolor); // bare forearms: the hands in your body's light
}

namespace
{

// The skin masks looked up so far (vr_skin_tone), per (model, kind, skin): the texture, or none (its file missing: not
// searched for again). Cleared when any texture is freed (TexMgr_FreeCount: a map change, a model flush), since a kept
// pointer may then name a freed or reused texture. The masks themselves are unowned: one copy for every body model
// (the builds, bare or with bracers) using that skin, freed at a game change.
struct SkinMaskEntry
{
    const qmodel_t* model{nullptr};
    int kind{0};
    int skin{0};
    gltexture_t* texture{nullptr}; // (null: the file is missing)
};
struct SkinMaskCache
{
    unsigned int frees{0};
    int count{0};
    SkinMaskEntry entries[16]{};
};
SkinMaskCache skinMasks;

void skinMasksValidate()
{
    if(skinMasks.frees != TexMgr_FreeCount())
    {
        skinMasks.frees = TexMgr_FreeCount();
        skinMasks.count = 0;
    }
    if(skinMasks.count > static_cast<int>(sizeof(skinMasks.entries) / sizeof(skinMasks.entries[0])) - 3)
    {
        skinMasks.count = 0; // (full: those in use, three at most, are looked up again)
    }
}

// The entry for a model's skin mask (kind 1 a hand's, 2 the body's), or null when it is not looked up yet.
[[nodiscard]] const SkinMaskEntry* skinMaskFind(const qmodel_t* model, int kind, int skin)
{
    for(int i = 0; i < skinMasks.count; i++)
    {
        const SkinMaskEntry& c = skinMasks.entries[i];
        if(c.model == model && c.kind == kind && c.skin == skin)
        {
            return &c;
        }
    }
    return nullptr;
}

[[nodiscard]] int skinMaskSkin(const entity_t* e, int kind)
{
    return CLAMP(0, e->skinnum, kind == 1 ? 3 : 15);
}

void gloveLayersUpdate(const entity_t* const* own, int count); // (below)

} // namespace

// The masks are loaded before any alias batches hold pointers into the movable model cache. Drawing only
// looks up a mask: Image_LoadImage/TexMgr_LoadImage may move that cache and must not run inside a draw loop.
// Each (model, skin) is searched for once (a missing file too) until textures are freed.
void VR_PrecacheSkinMasks()
{
    skinMasksValidate();
    entity_t* own[3]{};
    view::woundTargets(own);
    for(entity_t* e : own)
    {
        if(!e || !e->model)
        {
            continue;
        }
        const int kind = view::skinMaskWanted(e);
        if(!kind)
        {
            continue;
        }
        const int skin = skinMaskSkin(e, kind);
        if(skinMaskFind(e->model, kind, skin))
        {
            continue;
        }
        char path[MAX_QPATH];
        q_snprintf(path, sizeof(path), "progs/%s_%02d_00_skinmask", kind == 1 ? "hand_rig" : "vrbody", skin);
        gltexture_t* texture = TexMgr_FindTexture(nullptr, path);
        if(!texture)
        {
            const int mark = Hunk_LowMark();
            int width, height;
            enum srcformat format;
            byte* pixels = Image_LoadImage(path, &width, &height, &format);
            if(pixels)
            {
                texture = TexMgr_LoadImage(nullptr, path, width, height, format, pixels, path, 0,
                    TEXPREF_MIPMAP | TEXPREF_NOPICMIP | TEXPREF_ALPHA); // (A: the skin's shading, the gloves')
            }
            Hunk_FreeToLowMark(mark);
        }
        skinMasks.entries[skinMasks.count++] = SkinMaskEntry{e->model, kind, skin, texture};
    }
    gloveLayersUpdate(own, 3);
}

gltexture_t* VR_AliasSkinMask(const entity_t* e)
{
    const int kind = view::skinMaskWanted(e);
    if(!kind || !e->model || skinMasks.frees != TexMgr_FreeCount())
    {
        return blacktexture;
    }
    const SkinMaskEntry* c = skinMaskFind(e->model, kind, skinMaskSkin(e, kind));
    return c && c->texture ? c->texture : blacktexture;
}

namespace
{

// The gloves' painted leather (vr_glove_paint; make_glove_paint.py, docs/vr-port/GLOVES.md) per hand skin
// (hand_rig_0N_00): progs/hand_rig_0N_00_glove (RGB the leather's detail, A its wear) and _glove_height (its relief),
// else skin 0's. Looked up once (a missing file too) until a texture is freed; freed and read again from their files at
// every map (worldGeneration) and on vr_gloves_reload, so edits show then. Unowned: one copy for every hand model.
struct GloveLayer
{
    bool looked{false};
    gltexture_t* paint{nullptr}; // (null: no painted layer: the shader's leather alone)
    gltexture_t* height{nullptr}; // its relief, made a normal map (gloveHeightToNormals; null: none: flat)
};
struct GloveLayers
{
    unsigned int frees{0};
    int world{-1}; // the map they were read for
    bool reload{false}; // vr_gloves_reload
    GloveLayer skins[4]{};
};
GloveLayers gloveLayers;

// The relief's height map (_glove_height, make_glove_paint.py): its R the height, 128 flat, 0..255 this many mm; at
// 1024 x 1024 this many texels a mm (the skin's 1.5 twice). Its slopes are steepened by gloveReliefGain, as the bracers'
// and pauldrons' baked relief is (normalbody.py, normaltiles.py: their stitches and seams deeper than real ones).
static constexpr float gloveHeightRangeMm = 2.5f;
static constexpr float gloveTexelsPerMm = 3.f;
static constexpr float gloveReliefGain = 2.f;

// A glove's height map (RGBA, as read) made its normal map in place, as normalbake.py height_to_tangent bakes the
// models': tangent space, x along u, y up the image (green up), RGB the unit normal (z kept: the mips' shorter normals
// widen the sheen, SpecularAA), A 255. Mipmapped, its far look is the average of its bumps (the shader's old taps of
// the heights a texel apart lost the slopes on coarser mips). ~4 ms at 1024 x 1024, at each read only.
void gloveHeightToNormals(byte* rgba, int width, int height)
{
    byte* h = static_cast<byte*>(Hunk_AllocNoFill(width * height)); // (freed with the image: the caller's mark)
    for(int i = 0; i < width * height; i++)
    {
        h[i] = rgba[i * 4];
    }
    // a step of one level over the two texels of a central difference, as a slope (mm a mm)
    const float k = gloveReliefGain * gloveHeightRangeMm / 255.f * gloveTexelsPerMm * (static_cast<float>(width) / 1024.f) * 0.5f;
    for(int y = 0; y < height; y++)
    {
        const byte* up = h + q_max(y - 1, 0) * width;
        const byte* row = h + y * width;
        const byte* down = h + q_min(y + 1, height - 1) * width;
        for(int x = 0; x < width; x++)
        {
            const float sx = (row[q_min(x + 1, width - 1)] - row[q_max(x - 1, 0)]) * k; // dh/du
            const float sy = (down[x] - up[x]) * k; // dh down the image: -dh/d(up)
            const float inv = 1.f / sqrtf(sx * sx + sy * sy + 1.f);
            byte* out = rgba + (y * width + x) * 4;
            out[0] = static_cast<byte>((-sx * inv * 0.5f + 0.5f) * 255.f + 0.5f);
            out[1] = static_cast<byte>((sy * inv * 0.5f + 0.5f) * 255.f + 0.5f);
            out[2] = static_cast<byte>((inv * 0.5f + 0.5f) * 255.f + 0.5f);
            out[3] = 255;
        }
    }
}

// The texture of the first of these files found (loaded now unless it is already), or null. The height map
// (_glove_height) is made a normal map as it loads (gloveHeightToNormals).
[[nodiscard]] gltexture_t* gloveTexture(int skin, const char* suffix)
{
    const bool relief = !q_strcasecmp(suffix, "glove_height");
    for(const int s : {skin, 0})
    {
        char path[MAX_QPATH];
        q_snprintf(path, sizeof(path), "progs/hand_rig_%02d_00_%s", s, suffix);
        if(gltexture_t* found = TexMgr_FindTexture(nullptr, path))
        {
            return found;
        }
        const int mark = Hunk_LowMark();
        int width, height;
        enum srcformat format;
        byte* pixels = Image_LoadImage(path, &width, &height, &format);
        if(pixels && relief && format != SRC_RGBA)
        {
            Con_Printf("glove paint: %s is not RGBA as read: no relief\n", path);
            pixels = nullptr;
        }
        const double t0 = Sys_DoubleTime();
        if(pixels && relief)
        {
            gloveHeightToNormals(pixels, width, height);
        }
        const double made = (Sys_DoubleTime() - t0) * 1000.0;
        gltexture_t* texture = pixels ? TexMgr_LoadImage(nullptr, path, width, height, format, pixels, path, 0,
                                            TEXPREF_MIPMAP | TEXPREF_NOPICMIP | TEXPREF_ALPHA |
                                                (relief ? TEXPREF_UNCOMPRESSED : TEXPREF_NONE))
                                      : nullptr;
        Hunk_FreeToLowMark(mark);
        if(texture)
        {
            if(relief)
            {
                Con_DPrintf("glove paint: %s read (%d x %d), its normal map made in %.1f ms\n", path, width, height, made);
            }
            else
            {
                Con_DPrintf("glove paint: %s read (%d x %d)\n", path, width, height);
            }
        }
        if(texture || s == 0)
        {
            return texture;
        }
    }
    return nullptr;
}

// Outside the draw loops (VR_PrecacheSkinMasks): the layers freed for a new map or a reload, looked up again when used.
void gloveLayersUpdate(const entity_t* const* own, int count)
{
    if(gloveLayers.world != worldGeneration() || gloveLayers.reload)
    {
        gloveLayers.world = worldGeneration();
        gloveLayers.reload = false;
        for(int skin = 0; skin < 4; skin++) // (by name: a kept pointer may be stale by now)
        {
            for(const char* suffix : {"glove", "glove_height"})
            {
                char path[MAX_QPATH];
                q_snprintf(path, sizeof(path), "progs/hand_rig_%02d_00_%s", skin, suffix);
                if(gltexture_t* t = TexMgr_FindTexture(nullptr, path))
                {
                    TexMgr_FreeTexture(t); // (read again from its file when next used: edits show)
                }
            }
        }
        for(GloveLayer& l : gloveLayers.skins)
        {
            l = GloveLayer{};
        }
    }
    if(gloveLayers.frees != TexMgr_FreeCount())
    {
        gloveLayers.frees = TexMgr_FreeCount();
        for(GloveLayer& l : gloveLayers.skins)
        {
            l = GloveLayer{};
        }
    }
    for(int i = 0; i < count; i++)
    {
        const entity_t* e = own[i];
        if(!e || !e->model || view::skinMaskWanted(e) != 1)
        {
            continue;
        }
        GloveLayer& l = gloveLayers.skins[skinMaskSkin(e, 1)];
        if(!l.looked)
        {
            l.looked = true;
            l.paint = gloveTexture(skinMaskSkin(e, 1), "glove");
            l.height = l.paint ? gloveTexture(skinMaskSkin(e, 1), "glove_height") : nullptr;
        }
    }
    gloveLayers.frees = TexMgr_FreeCount(); // (what was loaded above freed nothing)
}

// The painted layer for `e` (a hand's, vr_glove_paint on, looked up), or null.
[[nodiscard]] const GloveLayer* gloveLayerOf(const entity_t* e)
{
    if(vr_glove_paint.value <= 0.f || !e->model || gloveLayers.frees != TexMgr_FreeCount() ||
        view::skinMaskWanted(e) != 1)
    {
        return nullptr;
    }
    const GloveLayer& l = gloveLayers.skins[skinMaskSkin(e, 1)];
    return l.looked && l.paint ? &l : nullptr;
}

} // namespace

void VR_AliasGlovePaint(const entity_t* e, gltexture_t* out[2])
{
    const GloveLayer* l = gloveLayerOf(e);
    out[0] = l ? l->paint : greytexture;
    out[1] = l && l->height ? l->height : greytexture;
}

namespace qvr::render
{

void glovesReload_f()
{
    gloveLayers.reload = true;
    Con_Printf("vr_gloves_reload: the gloves' painted leather (progs/hand_rig_0N_00_glove*) read again at the next frame\n");
}

int glovePaintFlags(const entity_t* e)
{
    const GloveLayer* l = gloveLayerOf(e);
    return l ? (l->height ? 2 : 1) : 0;
}

} // namespace qvr::render
