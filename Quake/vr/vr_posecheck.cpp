// vr_posecheck.cpp -- the drawn poses' check (vr_debug_pose_check; Debug > Logs): each monster's .mdl as R_DrawAliasModel
// draws it this frame (its two poses, the blend between them, its matrix) against the model's own shapes, logging a body
// drawn collapsed ("posecheck:"): squashed flat between two poses, a pose out of the model's range, a blend that isn't a
// number, or a matrix that scales it unevenly. (Vittorio, 2026-10-09: a parried enemy's body "all distorted and
// flattened".)

#include "vr_engine.hpp"
#include "vr_cvars.hpp"

#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/glm.hpp>

#include <math.h>


using namespace qvr;

namespace
{

// The smallest extent on each axis over all of a model's poses (its own units): its "normal range".
struct ModelRange
{
    glm::vec3 minExtent{0.f};
};
ankerl::unordered_dense::map<const qmodel_t*, ModelRange> modelRanges;

[[nodiscard]] glm::vec3 poseExtent(const aliashdr_t* hdr, const trivertx_t* a, const trivertx_t* b, float blend)
{
    glm::vec3 lo{1e9f}, hi{-1e9f};
    for(int i = 0; i < hdr->numverts; i++)
    {
        const glm::vec3 pa{a[i].v[0], a[i].v[1], a[i].v[2]}, pb{b[i].v[0], b[i].v[1], b[i].v[2]};
        const glm::vec3 p = pa + (pb - pa) * blend;
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    return (hi - lo) * glm::vec3{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
}

[[nodiscard]] const ModelRange& modelRange(const qmodel_t* model, const aliashdr_t* hdr, const trivertx_t* base)
{
    if(const auto it = modelRanges.find(model); it != modelRanges.end())
    {
        return it->second;
    }
    ModelRange r{glm::vec3{1e9f}};
    for(int p = 0; p < hdr->numposes; p++)
    {
        const trivertx_t* v = base + static_cast<za::SizeT>(p) * static_cast<za::SizeT>(hdr->numverts);
        r.minExtent = glm::min(r.minExtent, poseExtent(hdr, v, v, 0.f));
    }
    return modelRanges[model] = r;
}


// A monster drawn squashed between two poses: since when (cl.time), and the worst of it. Quake's own frame lerps pass
// through such shapes for a tenth of a second (a run into an attack); a body held squashed longer is what is logged.
struct Squash
{
    double since{-1.0};
    double last{-1.0}; // the last frame it was drawn squashed
    float worst{1.f};
    bool logged{false};
};
ankerl::unordered_dense::map<int, Squash> squashes; // by entity number

constexpr float squashedEnds = 0.8f;  // its extent on an axis against the smaller of its two poses'
constexpr float squashedModel = 0.85f; // ... against the model's smallest over all its poses
constexpr double squashedLong = 0.2;   // s: twice a frame lerp of Quake's

void endSquash(int num, Squash& q, const char* model)
{
    if(q.logged)
    {
        Con_Printf("posecheck: %.3f ent %d (%s) squashed %.2f s in all (worst %.2f)\n", cl.time, num, model,
            q.last - q.since, q.worst);
    }
    q = Squash{};
}

} // namespace

// R_DrawAliasModel (standard draws): `matrix` its model matrix (R_EntityMatrix's, the model's scale, VR's transforms).
extern "C" void VR_DebugAliasPose(const entity_t* e, const void* aliashdr, int pose1, int pose2, float blend,
    const float matrix[16])
{
    const int mode = static_cast<int>(vr_debug_pose_check.value);
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    const int num = static_cast<int>(e - cl_entities);
    if(mode <= 0 || !hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || num <= cl.maxclients || num >= cl.num_entities ||
        !e->model || hdr->numframes < 20 || hdr->numverts <= 0) // (monsters: many frames; not the players' or the view's)
    {
        return;
    }
    if(vr_debug_pose_check_model.string[0] && !strstr(e->model->name, vr_debug_pose_check_model.string))
    {
        return;
    }

    const char* why = nullptr;
    float flat = 1.f, flatEnds = 1.f;
    glm::vec3 k{1.f};
    if(pose1 < 0 || pose1 >= hdr->numposes || pose2 < 0 || pose2 >= hdr->numposes)
    {
        why = "pose out of range";
    }
    else if(!isfinite(blend) || blend < 0.f || blend > 1.f)
    {
        why = "blend not in 0..1";
    }
    else
    {
        const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
        const trivertx_t* v1 = base + static_cast<za::SizeT>(pose1) * static_cast<za::SizeT>(hdr->numverts);
        const trivertx_t* v2 = base + static_cast<za::SizeT>(pose2) * static_cast<za::SizeT>(hdr->numverts);
        const glm::vec3 drawn = poseExtent(hdr, v1, v2, blend);
        const glm::vec3 ends = glm::min(poseExtent(hdr, v1, v1, 0.f), poseExtent(hdr, v2, v2, 0.f));
        const ModelRange& range = modelRange(e->model, hdr, base);
        for(int i = 0; i < 3; i++)
        {
            flat = za::min(flat, range.minExtent[i] > 0.f ? drawn[i] / range.minExtent[i] : 1.f);
            flatEnds = za::min(flatEnds, ends[i] > 0.f ? drawn[i] / ends[i] : 1.f);
        }
        // The matrix's scale on each of the model's axes against the model's own and the entity's (ENTSCALE).
        const float s = e->scale ? ENTSCALE_DECODE(e->scale) : 1.f;
        for(int i = 0; i < 3; i++)
        {
            const glm::vec3 col{matrix[i * 4], matrix[i * 4 + 1], matrix[i * 4 + 2]};
            k[i] = glm::length(col) / (hdr->scale[i] * s);
        }
        const float kLo = za::min(k.x, za::min(k.y, k.z)), kHi = za::max(k.x, za::max(k.y, k.z));
        if(kLo < 0.85f * kHi)
        {
            why = "matrix scales it unevenly";
        }
    }

    Squash& q = squashes[num];
    if(flatEnds < squashedEnds || flat < squashedModel)
    {
        if(q.since < 0.0 || cl.time - q.last > 0.05)
        {
            endSquash(num, q, e->model->name);
            q.since = cl.time;
        }
        q.last = cl.time;
        q.worst = za::min(q.worst, za::min(flatEnds, flat));
        if(!q.logged && cl.time - q.since >= squashedLong)
        {
            q.logged = true;
            why = why ? why : "squashed between its poses too long";
            if(mode >= 3)
            {
                Cbuf_InsertText("screenshot\n"); // (vr_debug_pose_check 3: a picture of it, ahead of a script's waits)
            }
        }
    }
    else if(q.since >= 0.0)
    {
        endSquash(num, q, e->model->name);
    }

    if(why || mode == 2)
    {
        Con_Printf("posecheck: %.3f ent %d (%s) frame %d poses %d-%d blend %.3f flat %.2f (ends %.2f) scale %.2f %.2f %.2f%s%s\n",
            cl.time, num, e->model->name, e->frame, pose1, pose2, blend, flat, flatEnds, k.x, k.y, k.z, why ? ": " : "",
            why ? why : "");
    }
}
