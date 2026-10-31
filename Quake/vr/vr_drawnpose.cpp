#include "vr_alloccount.h"
// vr_drawnpose.cpp -- where an entity's body is drawn now (vr_drawnpose.hpp).

#include "vr_drawnpose.hpp"
#include "vr_hitreact.hpp"
#include "vr_hitreact_internal.hpp"
#include "vr_api_render.h"
#include "vr_ragdoll.hpp"
#include "vr_rig2.hpp"

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

bool armedMesh(edict_t* ent, za::Vector<glm::vec3>& out, za::Vector<glm::vec3>* normals)
{
    const int num = viewNum(ent);
    glm::mat4 m;
    if(num <= 0 || ragdoll::published(num) || !hitreact::armsWanted(&cl_entities[num]) ||
        !rig2::liveVertices(num, out, normals) || !livePlacement(num, m))
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
    return true;
}

const char* sourceName(Source s)
{
    return s == Source::Ragdoll ? "ragdoll" : s == Source::Live ? "live" : "none";
}

} // namespace qvr::drawnpose
