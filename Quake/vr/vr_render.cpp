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

#include <algorithm>
#include <unordered_map>
#include <vector>

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
    const weapons::ModelTransform t = weapons::modelTransform(e->model);

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

    if(const client::EntityVr* net = networkData(e); net && net->scale != glm::vec3{0.f})
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
    entityMatrix(ve.ent, ve.mirrored, ENTSCALE_DEFAULT, extra, out);
}

void entityMatrix(const entity_t& e, bool mirrored, int scale, const glm::vec3& extra, float out[16])
{
    vec3_t origin, angles; // R_EntityMatrix takes them non-const
    VectorCopy(e.origin, origin);
    VectorCopy(e.angles, angles);
    R_EntityMatrix(out, origin, angles, static_cast<unsigned char>(scale));
    applyPre(&e, mirrored, &extra, out);

    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e.model));
    ApplyTranslation(out, hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]);
    ApplyScale(out, hdr->scale[0], hdr->scale[1], hdr->scale[2]);

    applyPost(&e, out);
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
    std::fill(refs, refs + n, static_cast<unsigned short>(0));
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

    std::unordered_map<int, std::vector<int>> clusters; // by their point in frame 0
    for(int t = 0; t + 2 < hdr->numindexes; t += 3)
    {
        const int a = indexes[t], b = indexes[t + 1], c = indexes[t + 2];
        if(a < n && b < n && c < n && key(a) == key(b) && key(a) == key(c))
        {
            std::vector<int>& cl = clusters[key(a)];
            cl.insert(cl.end(), {a, b, c});
        }
    }

    std::vector<bool> flash(static_cast<size_t>(n), false);
    std::vector<int> points; // a vertex of each flash
    for(auto& [k, cl] : clusters)
    {
        std::sort(cl.begin(), cl.end());
        cl.erase(std::unique(cl.begin(), cl.end()), cl.end());
        float spread = 0.f;
        for(int p = 0; p < hdr->numposes && spread < 2.f; p++)
        {
            const trivertx_t* pose = verts + hdr->numverts * p;
            const glm::vec3 o = at(pose, cl[0]);
            for(int v : cl)
            {
                spread = std::max(spread, glm::distance(at(pose, v), o));
            }
        }
        if(cl.size() >= 3 && spread >= 2.f)
        {
            for(int v : cl)
            {
                flash[static_cast<size_t>(v)] = true;
            }
            points.push_back(cl[0]);
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
