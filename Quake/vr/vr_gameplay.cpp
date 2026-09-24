// vr_gameplay.cpp -- opt-in gameplay rule changes (vr_gameplayfix_* cvars).
//
// These change vanilla behaviour, so they default to off and are enabled by the Quake VR
// game folder's quakevr.cfg.

#include "vr_cvars.hpp"

using namespace qvr;

namespace
{

// Traces straight down from a point on the bottom face of the entity's bounding box.
[[nodiscard]] bool traceFloorBelow(edict_t* ent, float xOffset, float yOffset, trace_t& out)
{
    vec3_t start, end;
    start[0] = ent->v.origin[0] + xOffset;
    start[1] = ent->v.origin[1] + yOffset;
    start[2] = ent->v.origin[2] + ent->v.mins[2];
    VectorCopy(start, end);
    end[2] -= 256.f;

    out = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NOMONSTERS, ent);
    return out.fraction < 1.f && !out.allsolid;
}

} // namespace

// droptofloor: instead of sweeping the whole bounding box (which fails when an item's box
// starts slightly inside a wall), trace down from the centre of the box's bottom face and
// then from its four corners, and rest on the first floor found.
extern "C" int VR_DropToFloor()
{
    if(!vr_gameplayfix_droptofloor.value)
    {
        return 0;
    }

    edict_t* ent = PROG_TO_EDICT(pr_global_struct->self);

    const float offsets[5][2] = {
        {0.f, 0.f},
        {ent->v.mins[0], ent->v.mins[1]},
        {ent->v.mins[0], ent->v.maxs[1]},
        {ent->v.maxs[0], ent->v.mins[1]},
        {ent->v.maxs[0], ent->v.maxs[1]},
    };

    trace_t trace;
    for(const auto& offset : offsets)
    {
        if(traceFloorBelow(ent, offset[0], offset[1], trace))
        {
            ent->v.origin[2] = trace.endpos[2] - ent->v.mins[2];
            SV_LinkEdict(ent, false);
            ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) | FL_ONGROUND);
            ent->v.groundentity = EDICT_TO_PROG(trace.ent);
            G_FLOAT(OFS_RETURN) = 1.f;
            return 1;
        }
    }

    G_FLOAT(OFS_RETURN) = 0.f;
    return 1;
}
