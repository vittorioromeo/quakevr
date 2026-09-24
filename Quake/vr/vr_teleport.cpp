// vr_teleport.cpp -- see vr_teleport.hpp. Ported from the old engine's VR_DoTeleportation.
//
// While +teleport is held, a small box is swept from the player along the off hand's aim, up
// to vr_teleport_range units; the spot is valid when it lands on a floor or a gentle slope
// (normal z 0.75 .. 1), and is marked with particles (blue if valid, red otherwise). Letting
// go on a valid spot sends it with the move (VRBITS0_TELEPORTING); the server moves the
// player there. The sweep needs the local server's world, so it only works when hosting.

#include "vr_teleport.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"

namespace qvr::teleport
{
namespace
{

bool held = false;
bool wasHeld = false;
bool impactValid = false;
glm::vec3 impact{0.f};
double nextParticles = 0.0;

void teleportDown_f()
{
    held = true;
}

void teleportUp_f()
{
    held = false;
}

[[nodiscard]] bool trace(const hands::State& s)
{
    if(!sv.active || svs.maxclients < 1 || !svs.clients[0].edict)
    {
        return false;
    }

    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);

    edict_t* player = svs.clients[0].edict;
    vec3_t mins{-6.f, -6.f, -12.f};
    vec3_t maxs{6.f, 6.f, 12.f};

    const glm::vec3 end = s.pos[HAND_OFF] + hands::forward(s.rot[HAND_OFF]) * vr_teleport_range.value;
    vec3_t start{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
    vec3_t stop{end.x, end.y, end.z};
    const trace_t tr = SV_Move(start, mins, maxs, stop, MOVE_NORMAL, player);

    PR_PopQCVM(oldvm);

    impact = {tr.endpos[0], tr.endpos[1], tr.endpos[2] + 12.f}; // player origin is above the feet

    // Floors and slopes, not walls or ceilings.
    return tr.fraction < 1.f && !tr.allsolid && tr.plane.normal[2] >= 0.75f;
}

void drawAim(const hands::State& s)
{
    if(realtime < nextParticles)
    {
        return;
    }
    nextParticles = realtime + 0.05;

    // Quake palette: 208 blue, 73 red.
    const int color = impactValid ? 208 : 73;
    vec3_t zero{0.f, 0.f, 0.f};

    const glm::vec3 start = s.pos[HAND_OFF];
    const float length = glm::distance(start, impact);
    for(float d = 16.f; d < length; d += 24.f)
    {
        const glm::vec3 p = glm::mix(start, impact, d / length);
        vec3_t org{p.x, p.y, p.z};
        R_RunParticleEffect(org, zero, color, 1);
    }

    vec3_t org{impact.x, impact.y, impact.z - 12.f};
    R_RunParticleEffect(org, zero, color, 8);
}

} // namespace

void init()
{
    Cmd_AddCommand("+teleport", teleportDown_f);
    Cmd_AddCommand("-teleport", teleportUp_f);
}

bool update(const hands::State& s, glm::vec3& target)
{
    const bool aiming = held && vr_teleport_enabled.value && s.valid;

    bool release = false;
    if(aiming)
    {
        impactValid = trace(s);
        drawAim(s);
    }
    else if(wasHeld && impactValid)
    {
        release = true;
        target = impact;
        impactValid = false;
    }

    wasHeld = aiming;
    return release;
}

} // namespace qvr::teleport
