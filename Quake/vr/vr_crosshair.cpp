// vr_crosshair.cpp -- see vr_crosshair.hpp. From the old engine's show_crosshair (vr_showfn.cpp).
//
// vr_crosshair: 0 none, 1 a dot where the aim meets a wall, 2 a laser from the muzzle to the
// first thing it hits, 3 the same laser fading in and out. vr_crosshair_depth > 0 puts it at a
// fixed distance instead; vr_crosshair_size and vr_crosshair_alpha size and fade it. Weapons
// whose crosshair mode is "forbidden" (melee) have none.

#include "vr_crosshair.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_lines.hpp"
#include "vr_protocol.hpp"
#include "vr_weapons.hpp"

namespace qvr::crosshair
{
namespace
{

enum Mode : int
{
    NONE = 0,
    POINT = 1,
    LINE = 2,
    LINE_SMOOTH = 3
};

[[nodiscard]] qmodel_t* weaponModel(int hand)
{
    const int index = hand == HAND_MAIN ? cl.stats[STAT_WEAPON] : cl.stats[protocol::STAT_QVR_WEAPONMODEL2];
    return index > 0 && index < MAX_MODELS ? cl.model_precache[index] : nullptr;
}

// Where the aim from `start` along `dir` meets the world (walls only, or also entities).
[[nodiscard]] glm::vec3 aimEnd(const glm::vec3& start, const glm::vec3& dir, bool entities)
{
    if(vr_crosshair_depth.value > 0.f)
    {
        return start + dir * vr_crosshair_depth.value;
    }

    const glm::vec3 farEnd = start + dir * 4096.f;
    if(!sv.active || svs.maxclients < 1 || !svs.clients[0].edict)
    {
        return farEnd; // not hosting: no world to trace against
    }

    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    vec3_t a{start.x, start.y, start.z};
    vec3_t b{farEnd.x, farEnd.y, farEnd.z};
    const trace_t tr = SV_Move(a, vec3_origin, vec3_origin, b, entities ? MOVE_NORMAL : MOVE_NOMONSTERS,
        svs.clients[0].edict);
    PR_PopQCVM(oldvm);

    return {tr.endpos[0], tr.endpos[1], tr.endpos[2]};
}

} // namespace

void queue(const hands::State& s)
{
    const int mode = static_cast<int>(vr_crosshair.value);
    const float size = CLAMP(0.f, vr_crosshair_size.value, 32.f);
    const float alpha = CLAMP(0.f, vr_crosshair_alpha.value, 1.f);
    if(mode == NONE || size <= 0.f || alpha <= 0.f || !s.valid)
    {
        return;
    }

    const glm::vec4 red{1.f, 0.f, 0.f, alpha};
    const glm::vec4 faded{1.f, 0.f, 0.f, alpha * 0.01f};

    for(int h = 0; h < HAND_COUNT; h++)
    {
        const int slot = weapons::slotForModel(weaponModel(h));
        if(!s.muzzleValid[h] || slot < 0 || slot == weapons::fistSlot() ||
            weapons::value(slot, weapons::Key::CrosshairMode) == 1.f)
        {
            continue;
        }

        const glm::vec3 start = s.muzzle[h];
        const glm::vec3 dir = hands::forward(s.rot[h]);

        if(mode == POINT)
        {
            glm::vec3 end = aimEnd(start, dir, false);
            end.z += vr_crosshairy.value;
            lines::point(end, size * std::fmax(1.f, glm::distance(start, end) * 0.01f), red);
            continue;
        }

        glm::vec3 end = aimEnd(start, dir, true);
        end.z += vr_crosshairy.value * 10.f;

        const float width = size * 0.3f;
        if(mode == LINE)
        {
            lines::line(start, end, width, red, red);
        }
        else
        {
            const glm::vec3 midA = glm::mix(start, end, 0.15f);
            const glm::vec3 midB = glm::mix(start, end, 0.7f);
            lines::line(start, midA, width, faded, red);
            lines::line(midA, midB, width, red, red);
            lines::line(midB, end, width, red, faded);
        }
    }
}

} // namespace qvr::crosshair
