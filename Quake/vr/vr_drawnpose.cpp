#include "vr_alloccount.h"
// vr_drawnpose.cpp -- where an entity's body is drawn now (vr_drawnpose.hpp).

#include "vr_drawnpose.hpp"
#include "vr_hitreact.hpp"
#include "vr_hitreact_internal.hpp"
#include "vr_api_render.h"
#include "vr_mem.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rig2.hpp"

#include <cstring>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace qvr::drawnpose
{

namespace
{

// `ent`'s client entity number on this listen server's own view; 0 none.
[[nodiscard]] int viewNum(edict_t* ent)
{
    if(!ent || ent->free || !sv.active || cls.state != ca_connected || !cl_entities)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(ent);
    return num > 0 && num < cl.num_entities ? num : 0;
}

// Where client entity `num` alive on the authored rig is drawn: its model space (the bones', the skinned copy's) to the
// world (its move's lerp; as vr_modelcollide.cpp places its skinned copy: the copy's scale 1, its origin 0). False: no
// model, or a degenerate one.
[[nodiscard]] bool livePlacement(int num, glm::mat4& m)
{
    const entity_t& e = cl_entities[num];
    if(!e.model)
    {
        return false;
    }
    float yaw = 0.f;
    const glm::vec3 o = hitreact_detail::drawnOrigin(&e, &yaw);
    float m16[16];
    vec3_t origin{o.x, o.y, o.z}, angles{e.angles[0], yaw, e.angles[2]};
    R_EntityMatrix(m16, origin, angles, e.scale);
    VR_AliasPreTransform(&e, m16);
    VR_AliasPostTransform(&e, m16);
    m = glm::make_mat4(m16);
    const glm::mat3 a{m};
    return glm::length(a[0]) >= 1e-6f && glm::determinant(a) > 0.f;
}

// armedMesh's results by entity (the main thread: the server's QC and the client's frame): what made them (rig2::liveKey's
// publish and rig generation, the placement) and the world vertices (normals: when they were asked for). A frame asks
// for the same armed humanoid's body many times (each melee sweep's line, each shot, the arms' checks: PERF_1102.md).
struct MeshKey
{
    int num{0};
    za::U32 serial{0};
    int generation{-1};
    glm::mat4 m{1.f};
    [[nodiscard]] bool operator==(const MeshKey& o) const
    {
        return num == o.num && serial == o.serial && generation == o.generation && !memcmp(&m, &o.m, sizeof(m));
    }
};
struct MeshMemo
{
    MeshKey key;
    bool hasNormals{false};
    za::U32 build{0}; // (armedMesh's `build`: lastBuild when made)
    za::Vector<glm::vec3> pos, nor;
};
[[nodiscard]] za::SizeT heldBytes(const MeshMemo& x) // (vr_mem.hpp)
{
    return mem::heldBytes(x.pos) + mem::heldBytes(x.nor);
}
struct ArmedMeshes
{
    za::Vector<MeshMemo> memos; // (one an entity: its last)
    auto members() { return mem::list(memos); }
};
mem::Cache<ArmedMeshes> armedMeshes{"drawn pose armed meshes", mem::MapChange | mem::ModelReload};
constexpr za::SizeT maxMeshMemos = 256; // (past it: all begun again)
za::U32 lastBuild = 0;                  // the memos' makings counted (never 0 again: a wrap skips it)
} // namespace

Source bone(edict_t* ent, int b, rigpose::Rigid& out, float& scale)
{
    const int num = viewNum(ent);
    if(num <= 0 || b < 0)
    {
        return Source::None;
    }
    // A ragdoll (dead, or an active one: a stagger, a shove's fall, a tug): its part as drawn.
    if(glm::quat rot; ragdoll::drawnPart(num, b, rot, out.pos, scale))
    {
        out.rot = rot;
        return Source::Ragdoll;
    }
    // Alive on the authored rig: its bone as last drawn (model space), placed as the client draws the entity.
    glm::quat brot;
    glm::vec3 bpos;
    glm::mat4 m;
    if(!cl_entities[num].model || !rig2::drawnBone(num, b, brot, bpos) || !livePlacement(num, m))
    {
        return Source::None;
    }
    const glm::mat3 a{m};
    const float s = glm::length(a[0]);
    out.rot = glm::normalize(glm::quat_cast(a * (1.f / s)) * brot);
    out.pos = a * bpos + glm::vec3{m[3]};
    scale = s;
    return Source::Live;
}

bool armedMesh(edict_t* ent, za::Vector<glm::vec3>& out, za::Vector<glm::vec3>* normals, za::U32* build)
{
    return armedMesh(viewNum(ent), out, normals, build);
}

bool armedMesh(int num, za::Vector<glm::vec3>& out, za::Vector<glm::vec3>* normals, za::U32* build)
{
    if(build)
    {
        *build = 0;
    }
    glm::mat4 m;
    // (its own .mdl: while a frame draws it, between rig2::swapModels and restoreModels, cl_entities[num].model is its
    // skinned copy, armsWanted on which says no: Show Disarm Volumes drew its hand's hull from its animation frame then,
    // ROUND23.md "The cyan hand hull off")
    const qmodel_t* own = num > 0 ? rig2::sourceModel(&cl_entities[num]) : nullptr;
    if(num <= 0 || ragdoll::published(num) ||
        !hitreact::armsWantedAs(&cl_entities[num], own ? own : cl_entities[num].model))
    {
        return false;
    }
    // Made already for this publish and placement (a frame's melee sweeps, shots and arm checks ask for the same body
    // dozens of times): the same vertices again.
    MeshKey key;
    key.num = num;
    const bool keyed = rig2::liveKey(num, key.serial, key.generation) && livePlacement(num, key.m);
    if(keyed)
    {
        for(const MeshMemo& memo : armedMeshes.memos)
        {
            if(memo.key == key && (!normals || memo.hasNormals))
            {
                out = memo.pos;
                if(normals)
                {
                    *normals = memo.nor;
                }
                if(build)
                {
                    *build = memo.build;
                }
                return true;
            }
        }
    }
    if(!rig2::liveVertices(num, out, normals) || !livePlacement(num, m))
    {
        return false;
    }
    const glm::mat3 a{m};
    const glm::vec3 t{m[3]};
    for(glm::vec3& v : out)
    {
        v = a * v + t;
    }
    if(normals)
    {
        for(glm::vec3& n : *normals)
        {
            const glm::vec3 w = a * n;
            const float len = glm::length(w);
            n = len > 1e-6f ? w / len : glm::vec3{0.f}; // (a collapsed bone's: none)
        }
    }
    if(keyed)
    {
        za::Vector<MeshMemo>& memos = armedMeshes.memos;
        MeshMemo* memo = nullptr;
        for(MeshMemo& x : memos)
        {
            memo = x.key.num == num ? &x : memo;
        }
        if(!memo)
        {
            if(memos.size() >= maxMeshMemos)
            {
                memos.clear(); // (entities gone: begun again)
            }
            memos.pushBack(MeshMemo{});
            memo = &memos.back();
        }
        memo->key = key;
        memo->pos = out;
        memo->hasNormals = normals != nullptr;
        if(normals)
        {
            memo->nor = *normals;
        }
        lastBuild = lastBuild + 1u == 0u ? 1u : lastBuild + 1u;
        memo->build = lastBuild;
        if(build)
        {
            *build = lastBuild;
        }
    }
    return true;
}

const char* sourceName(Source s)
{
    return s == Source::Ragdoll ? "ragdoll" : s == Source::Live ? "live" : "none";
}

} // namespace qvr::drawnpose
