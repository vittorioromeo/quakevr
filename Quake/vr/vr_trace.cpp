// vr_trace.cpp -- see vr_trace.hpp.

#include "vr_trace.hpp"

namespace qvr::worldtrace
{

std::optional<trace_t> move(
    const glm::vec3& start, const glm::vec3& mins, const glm::vec3& maxs, const glm::vec3& end, int type)
{
    if(!sv.active || svs.maxclients < 1 || !svs.clients[0].active || !svs.clients[0].edict)
    {
        return std::nullopt;
    }

    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);

    vec3_t a{start.x, start.y, start.z};
    vec3_t b{end.x, end.y, end.z};
    vec3_t lo{mins.x, mins.y, mins.z};
    vec3_t hi{maxs.x, maxs.y, maxs.z};
    const trace_t tr = SV_Move(a, lo, hi, b, type, svs.clients[0].edict);

    PR_PopQCVM(oldvm);
    return tr;
}

} // namespace qvr::worldtrace
