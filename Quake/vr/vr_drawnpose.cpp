#include "vr_alloccount.h"
// vr_drawnpose.cpp -- where an entity's body is drawn now (vr_drawnpose.hpp).

#include "vr_drawnpose.hpp"
#include "vr_hitreact_internal.hpp"
#include "vr_api_render.h"
#include "vr_ragdoll.hpp"
#include "vr_rig2.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace qvr::drawnpose
{

Source bone(edict_t* ent, int b, rigpose::Rigid& out, float& scale)
{
    if(!ent || ent->free || b < 0 || !sv.active || cls.state != ca_connected || !cl_entities)
    {
        return Source::None;
    }
    const int num = NUM_FOR_EDICT(ent);
    if(num <= 0 || num >= cl.num_entities)
    {
        return Source::None;
    }
    // A ragdoll (dead, or an active one: a stagger, a shove's fall, a tug): its part as drawn.
    if(glm::quat rot; ragdoll::drawnPart(num, b, rot, out.pos, scale))
    {
        out.rot = rot;
        return Source::Ragdoll;
    }
    // Alive on the authored rig: its bone as last drawn (model space), placed as the client draws the entity (its move's
    // lerp; as vr_modelcollide.cpp places its skinned copy: the copy's scale 1, its origin 0).
    glm::quat brot;
    glm::vec3 bpos;
    const entity_t& e = cl_entities[num];
    if(!e.model || !rig2::drawnBone(num, b, brot, bpos))
    {
        return Source::None;
    }
    float yaw = 0.f;
    const glm::vec3 o = hitreact_detail::drawnOrigin(&e, &yaw);
    float m16[16];
    vec3_t origin{o.x, o.y, o.z}, angles{e.angles[0], yaw, e.angles[2]};
    R_EntityMatrix(m16, origin, angles, e.scale);
    VR_AliasPreTransform(&e, m16);
    VR_AliasPostTransform(&e, m16);
    const glm::mat4 m = glm::make_mat4(m16);
    const glm::mat3 a{m};
    const float s = glm::length(a[0]);
    if(s < 1e-6f || glm::determinant(a) <= 0.f)
    {
        return Source::None;
    }
    out.rot = glm::normalize(glm::quat_cast(a * (1.f / s)) * brot);
    out.pos = a * bpos + glm::vec3{m[3]};
    scale = s;
    return Source::Live;
}

const char* sourceName(Source s)
{
    return s == Source::Ragdoll ? "ragdoll" : s == Source::Live ? "live" : "none";
}

} // namespace qvr::drawnpose
