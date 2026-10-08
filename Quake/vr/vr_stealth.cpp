#include "vr_alloccount.h"
// vr_stealth.cpp -- the monsters' senses' engine side: the light at a point and on each player, each player's flashlight
// beam (QC vr_stealth.qc; docs/vr-port/STEALTH_PLAN.md). Each client measures the light on its own player and sends it
// with its lamp's beam in its VR move (vr_move.hpp): a coop client's lamp and the light he stands in are his own.

#include "vr_stealth.hpp"

#include "vr_engine.hpp"
#include "vr_flashlight.hpp"
#include "vr_move.hpp"
#include "vr_profile.h"
#include "vr_progs.hpp"
#include "vr_server.hpp"

#include <glm/glm.hpp>

namespace qvr::stealth
{

namespace
{

// The light trace's cache: one point sampled at a time (each player's, a tenth of a second apart).
lightcache_t lightCache{};

// This client's own player's light, as last measured (lightAt: ten times a second).
struct OwnLight
{
    float light{-1.f};
    double at{-1.0};
    const qmodel_t* world{nullptr};
};
OwnLight ownLight;

// The QC's stealth scopes (stealthprofile): nested calls make one "stealth" profiler scope.
struct ProfileScope
{
    int depth{0};
    bool open{false};
};
ProfileScope profileScope;

[[nodiscard]] float lightNow(const glm::vec3& point)
{
    vec3_t at{point.x, point.y, point.z};
    float light = static_cast<float>(R_LightPoint(at, 0.f, &lightCache));
    for(const dlight_t& dl : cl_dlights)
    {
        if(dl.die < cl.time || dl.radius <= 0.f || flashlight::ownsLight(dl.key))
        {
            continue;
        }
        const glm::vec3 d{at[0] - dl.origin[0], at[1] - dl.origin[1], at[2] - dl.origin[2]};
        const float add = dl.radius - glm::length(d);
        if(add > 0.f)
        {
            // (the colour's mean: a white light's 1; the flashlight-style scaled ones aside)
            const float c = (dl.color[0] + dl.color[1] + dl.color[2]) * (1.f / 3.f);
            light += add * (c > 0.f ? glm::min(c, 1.f) : 1.f);
        }
    }
    return light;
}

// The client's player edict `who` is, or -1.
[[nodiscard]] int clientOf(edict_t* who)
{
    const int client = NUM_FOR_EDICT(who) - 1;
    if(!sv.active || client < 0 || client >= svs.maxclients || !svs.clients[client].active)
    {
        return -1;
    }
    return client;
}

} // namespace

float lightAt(const glm::vec3& point)
{
    if(!cl.worldmodel || cls.state != ca_connected || cls.demoplayback || cls.signon != SIGNONS)
    {
        ownLight = {};
        return -1.f;
    }
    if(ownLight.world != cl.worldmodel || cl.time >= ownLight.at + 0.1 || cl.time < ownLight.at)
    {
        ownLight = {lightNow(point), cl.time, cl.worldmodel};
    }
    return ownLight.light;
}

void PF_stealthlight()
{
    const float* p = G_VECTOR(OFS_PARM0);
    if(!cl.worldmodel || cl.worldmodel != sv.worldmodel)
    {
        G_FLOAT(OFS_RETURN) = -1.f;
        return;
    }
    G_FLOAT(OFS_RETURN) = lightNow({p[0], p[1], p[2]});
}

void PF_clientlight()
{
    edict_t* who = G_EDICT(OFS_PARM0);
    G_FLOAT(OFS_RETURN) = -1.f;
    if(clientOf(who) < 0)
    {
        return;
    }
    const VrMove* move = server::clientMove(who);
    if(move && move->light >= 0.f)
    {
        G_FLOAT(OFS_RETURN) = glm::min(move->light, 4096.f);
    }
}

void PF_flashlightbeam()
{
    edict_t* who = G_EDICT(OFS_PARM0);
    const int what = static_cast<int>(G_FLOAT(OFS_PARM1));
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = out[1] = out[2] = 0.f;
    if(clientOf(who) < 0)
    {
        return;
    }
    const VrMove* move = server::clientMove(who);
    if(!move || !move->lampLit || move->lampRange <= 0.f || glm::length(move->lampDir) < 0.5f)
    {
        return;
    }
    const glm::vec3 v = what == 0   ? move->lampLens
                        : what == 1 ? glm::normalize(move->lampDir)
                                    : glm::vec3{1.f, glm::min(move->lampRange, 8192.f), glm::clamp(move->lampCos, -1.f, 1.f)};
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

void PF_pvsvisible()
{
    const float* a = G_VECTOR(OFS_PARM0);
    const float* b = G_VECTOR(OFS_PARM1);
    G_FLOAT(OFS_RETURN) = 1.f;
    if(!sv.worldmodel)
    {
        return;
    }
    vec3_t va{a[0], a[1], a[2]}, vb{b[0], b[1], b[2]};
    mleaf_t* la = Mod_PointInLeaf(va, sv.worldmodel);
    mleaf_t* lb = Mod_PointInLeaf(vb, sv.worldmodel);
    const int l = static_cast<int>(lb - sv.worldmodel->leafs) - 1;
    if(la == lb || l < 0)
    {
        return; // the same leaf, or b in the solid outside (a wall's inside): as seen
    }
    const byte* pvs = Mod_LeafPVS(la, sv.worldmodel);
    G_FLOAT(OFS_RETURN) = (pvs[l >> 3] & (1 << (l & 7))) ? 1.f : 0.f;
}

void PF_stealthprofile()
{
    const float what = G_FLOAT(OFS_PARM0);
    ProfileScope& s = profileScope;
    if(what > 0.f)
    {
        if(s.depth++ == 0 && vr_profile_on)
        {
            VR_ProfileBegin("stealth");
            s.open = true;
        }
        return;
    }
    if(what < 0.f)
    {
        s = {}; // (a server frame's start: a scope a QC error jumped out of is forgotten; the profiler drops its own)
        return;
    }
    if(s.depth <= 1)
    {
        if(s.open)
        {
            VR_ProfileEnd();
        }
        s = {};
        return;
    }
    s.depth--;
}

} // namespace qvr::stealth
