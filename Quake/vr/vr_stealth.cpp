#include "vr_alloccount.h"
// vr_stealth.cpp -- the monsters' senses' engine side: the light at a point and the host's flashlight beam (QC
// vr_stealth.qc; docs/vr-port/STEALTH_PLAN.md).

#include "vr_stealth.hpp"

#include "vr_engine.hpp"
#include "vr_flashlight.hpp"
#include "vr_progs.hpp"

#include <glm/glm.hpp>

namespace qvr::stealth
{

namespace
{

// The light trace's cache: one point sampled at a time (each player's, a tenth of a second apart).
lightcache_t lightCache{};

} // namespace

void PF_stealthlight()
{
    const float* p = G_VECTOR(OFS_PARM0);
    if(!cl.worldmodel || cl.worldmodel != sv.worldmodel)
    {
        G_FLOAT(OFS_RETURN) = -1.f;
        return;
    }
    vec3_t at{p[0], p[1], p[2]};
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
    G_FLOAT(OFS_RETURN) = light;
}

void PF_flashlightbeam()
{
    edict_t* who = G_EDICT(OFS_PARM0);
    const int what = static_cast<int>(G_FLOAT(OFS_PARM1));
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = out[1] = out[2] = 0.f;
    glm::vec3 lens, dir;
    float range = 0.f, cosOuter = 1.f;
    // Only the host's own player (the first client) has a lamp the server knows of.
    if(cls.demoplayback || cls.state != ca_connected || NUM_FOR_EDICT(who) != 1 ||
       !flashlight::beamNow(lens, dir, range, cosOuter))
    {
        return;
    }
    const glm::vec3 v = what == 0 ? lens : what == 1 ? dir : glm::vec3{1.f, range, cosOuter};
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

} // namespace qvr::stealth
