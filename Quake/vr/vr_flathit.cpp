// vr_flathit.cpp -- flatscreen play's feedback when he is hit (docs/vr-port/FLATPLAY.md, "Hit feedback"; his words
// 2026-11-06: "Please add a tunable camera aimpunch + directional hit effects to provide feedback when the player is hit.
// Make everything tunable", and "reduce the amount of movements the arms/weapons get when the player is hit during ADS").
//   - The punch: each hit turns the view along the blow (from ahead it tips up, from his right it turns him left, from
//     behind it tips down), quickly (vr_flat_hitpunch_attack), then eases back over vr_flat_hitpunch_time. It is put into
//     the aim itself (cl.viewangles), as each frame's change of it: while the view is punched the crosshair, the sights and
//     the shots go with it (a shot goes where the crosshair is, always), and once it settles the aim is back exactly where
//     the mouse put it (his own moves during it kept).
//   - The marks: an arc round the crosshair toward where each hit came from, turning as he turns and moves, fading out.
//   - Sighted, the punch is vr_flat_hitpunch_ads of itself, and the hands' knock (vr_painknock.cpp) vr_flat_ads_hitreact.
// The low-health blood at the screen's edges is vr_blood.cpp's, with flatscreen sizes (vr_flat_blood_health_size, _opacity).

#include "vr_flathit.hpp"
#include "vr_api.h"
#include "vr_tunables.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatsight.hpp"
#include "vr_geom.hpp"
#include "vr_hands.hpp"

#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <stdlib.h>
#include <string.h>

extern "C" cvar_t cl_maxpitch; // cl_main.c (johnfitz's variable pitch clamping)
extern "C" cvar_t cl_minpitch;

namespace qvr::flathit
{

namespace
{

constexpr int maxHits = 8;
constexpr float refDamage = 20.f;   // a hit of this much (armour and blood together) punches vr_flat_hitpunch degrees
constexpr float noDirection = 24.f; // units: a hit from nearer him than this (his own blast) has no way it came from
constexpr int arcSegments = 9;      // the mark's arc, drawn as this many pieces
constexpr float markRgb[3] = {0.78f, 0.02f, 0.02f};

struct Hit
{
    double at{-1e9};       // cl.time
    glm::vec2 punch{0.f};  // degrees at its full size: pitch (down positive), yaw (left positive)
    glm::vec3 from{0.f};   // where it came from (world)
    bool directed{false};  // false: no way (a fall, lava, the world, his own blast): no mark
    float strength{0.f};   // the mark's share of vr_flat_hitdir (0.5..1, with the damage)
};

struct Probe
{
    float rawKnock{0.f}, drawnKnock{0.f}, share{1.f}; // the hands' knock (keepHitReact), the most since the last print
    float punchPeak{0.f};                             // the punch's size, degrees, the most since the last print,
    float punchRaw{0.f};                              // and before the ironsight's share
};

struct State
{
    Hit hits[maxHits];
    int next{0};
    int count{0};           // hits taken in the mode (vr_flat_hit_probe)
    glm::vec2 applied{0.f}; // the punch now in cl.viewangles: pitch, yaw
    double lastTime{-1.0};
    char onHit[4096]{};     // vr_flat_hit_test: commands run at the next hit, once
    Probe probe;
};

State st;

[[nodiscard]] float tune(const Tunable& v, float lo, float hi)
{
    return v.value != v.value ? lo : za::clamp(v.value, lo, hi);
}

[[nodiscard]] bool playing()
{
    return flatplay::active() && cls.state == ca_connected && cls.signon == SIGNONS && cl.worldmodel && !cl.intermission &&
           !cls.demoplayback && cl.stats[STAT_HEALTH] > 0 && cl.viewentity >= 1 && cl.viewentity < cl.num_entities;
}

[[nodiscard]] glm::vec3 myOrigin()
{
    return geom::vec(cl_entities[cl.viewentity].origin);
}

void clearHits()
{
    for(Hit& h : st.hits)
    {
        h = {};
    }
    st.next = 0;
}

// The sighted share of a punch or a knock: 1 hipfire, `sighted` as far as the ironsight has come.
[[nodiscard]] float sightedShare(const Tunable& sighted)
{
    return glm::mix(1.f, tune(sighted, 0.f, 1.f), geom::smooth01(flatsight::adsShare()));
}

// The punch wanted this frame (degrees, pitch and yaw): every live hit's, by its rise and ease back, the sum held to
// vr_flat_hitpunch_max, sighted kept at vr_flat_hitpunch_ads.
[[nodiscard]] glm::vec2 punchWanted()
{
    const float attack = tune(vr_flat_hitpunch_attack, 0.f, 0.5f);
    const float back = tune(vr_flat_hitpunch_time, 0.01f, 3.f);
    glm::vec2 want{0.f};
    for(const Hit& h : st.hits)
    {
        const float age = static_cast<float>(cl.time - h.at);
        if(age < 0.f || age >= attack + back)
        {
            continue;
        }
        const float shape = age < attack ? geom::smooth01(age / attack) : 1.f - geom::smooth01((age - attack) / back);
        want += h.punch * shape;
    }
    const float most = tune(vr_flat_hitpunch_max, 0.f, 45.f);
    const float size = glm::length(want);
    if(size > most)
    {
        want *= size > 0.f ? most / size : 0.f;
    }
    st.probe.punchRaw = za::max(st.probe.punchRaw, glm::length(want));
    return want * sightedShare(vr_flat_hitpunch_ads);
}

// Where a hit's mark points on the screen: degrees round the crosshair from straight up (ahead), clockwise (right).
[[nodiscard]] float markAngle(const Hit& h)
{
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, cl.viewangles[YAW], 0.f}, fwd, right, up);
    const glm::vec3 to = h.from - myOrigin();
    return glm::degrees(za::atan2(glm::dot(to, right), glm::dot(to, fwd)));
}

// A hit's mark's opacity now (0: gone).
[[nodiscard]] float markAlpha(const Hit& h)
{
    const float time = tune(vr_flat_hitdir_time, 0.05f, 5.f);
    const float age = static_cast<float>(cl.time - h.at);
    if(!h.directed || age < 0.f || age >= time)
    {
        return 0.f;
    }
    return tune(vr_flat_hitdir, 0.f, 1.f) * h.strength * (1.f - geom::smooth01(age / time));
}

void probe_f()
{
    Probe& p = st.probe;
    Con_Printf("flathitknock: raw %.4f drawn %.4f share %.3f (vr_flat_ads_hitreact %g, ads %.3f)\n", p.rawKnock, p.drawnKnock,
        p.share, vr_flat_ads_hitreact.value, flatsight::adsShare());
    Con_Printf("flathitpunch: hits %d peak %.3f raw %.3f now %.3f %.3f (pitch yaw) aim %.3f %.3f fov %.3f %.3f\n", st.count, p.punchPeak, p.punchRaw, st.applied.x,
        st.applied.y, cl.viewangles[PITCH], cl.viewangles[YAW], r_refdef.fov_x, r_refdef.fov_y);
    for(int i = 0; i < maxHits; i++)
    {
        const Hit& h = st.hits[i];
        if(const float a = markAlpha(h); a > 0.f)
        {
            Con_Printf("flathitmark: %d angle %.1f alpha %.3f\n", i, markAngle(h), a);
        }
    }
    p = {};
}

void test_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_flat_hit_test <a, b, waitN, ...>: runs those commands (a comma between them; waitN: N frames) at the next "
                   "hit in flatscreen play, once (armed: %s)\n",
            st.onHit[0] ? "yes" : "no");
        return;
    }
    // (Commas, not semicolons: a test script's line is split at those. waitN as the kit's scripts write it.)
    st.onHit[0] = 0;
    const char* s = Cmd_Args();
    while(*s)
    {
        while(*s == ' ' || *s == ',' || *s == '"')
        {
            s++;
        }
        const char* end = s;
        while(*end && *end != ',' && *end != '"')
        {
            end++;
        }
        const int length = static_cast<int>(end - s);
        if(length > 0)
        {
            char cmd[256];
            q_strlcpy(cmd, s, za::min(length + 1, static_cast<int>(sizeof(cmd))));
            const int waits = !strncmp(cmd, "wait", 4) && cmd[4] >= '0' && cmd[4] <= '9' ? za::clamp(atoi(cmd + 4), 1, 600) : 0;
            for(int i = 0; i < (waits ? waits : 1); i++)
            {
                q_strlcat(st.onHit, waits ? "wait\n" : cmd, sizeof(st.onHit));
                if(!waits)
                {
                    q_strlcat(st.onHit, "\n", sizeof(st.onHit));
                }
            }
        }
        s = end;
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flat_hit_probe", probe_f);
    Cmd_AddCommand("vr_flat_hit_test", test_f);
}

void onDamage(int armor, int blood, const float* from)
{
    if(!playing() || armor + blood <= 0)
    {
        return;
    }
    const glm::vec3 me = myOrigin();
    const glm::vec3 f = from ? glm::vec3{from[0], from[1], from[2]} : me;
    bool directed = glm::length(f - me) > noDirection && f != glm::vec3{0.f};
    if(directed)
    {
        // The world's middle (falls, lava, slime, drowning: vr_painknock.cpp's rule) has no way either.
        const glm::vec3 middle = 0.5f * (geom::vec(cl.worldmodel->mins) + geom::vec(cl.worldmodel->maxs));
        directed = glm::length(f - middle) > noDirection;
    }

    const float damage = static_cast<float>(armor + blood);
    const float deg = tune(vr_flat_hitpunch, 0.f, 20.f) * za::clamp(damage / refDamage, 0.25f, 2.f);
    const float pitchShare = tune(vr_flat_hitpunch_pitch, -3.f, 3.f);
    const float yawShare = tune(vr_flat_hitpunch_yaw, -3.f, 3.f);
    glm::vec2 punch{deg * pitchShare, 0.f}; // (no way: the view dips)
    if(directed)
    {
        // Along the blow: from ahead the head goes back (up), from behind forward (down), from above down, from his right
        // to the left.
        glm::vec3 fwd, right, up;
        hands::angleVectors({0.f, cl.viewangles[YAW], 0.f}, fwd, right, up);
        const glm::vec3 blow = glm::normalize(me - f);
        punch.x = deg * pitchShare * (glm::dot(blow, fwd) - blow.z);
        punch.y = -deg * yawShare * glm::dot(blow, right);
    }

    Hit& h = st.hits[st.next];
    h = {cl.time, punch, f, directed, 0.5f + 0.5f * za::clamp(damage / refDamage, 0.f, 1.f)};
    st.next = (st.next + 1) % maxHits;
    st.count++;
    Con_DPrintf("flathit: hit %d damage %d %s punch %.2f %.2f (pitch yaw)\n", st.count, armor + blood,
        directed ? va("from %.1f deg", markAngle(h)) : "from nowhere", punch.x, punch.y);

    if(st.onHit[0])
    {
        Cbuf_InsertText(st.onHit); // (ahead of what the script still has queued: its waits are the test's safety net)
        st.onHit[0] = 0;
    }
}

void frame()
{
    if(!playing() || cl.time < st.lastTime - 0.1)
    {
        // Out of the mode, dead, a new level: the hits go and what the punch had put into the aim stays (the view is his).
        clearHits();
        st.applied = glm::vec2{0.f};
        st.lastTime = cl.time;
        return;
    }
    st.lastTime = cl.time;
    const glm::vec2 want = punchWanted();
    st.probe.punchPeak = za::max(st.probe.punchPeak, glm::length(want));
    const glm::vec2 change = want - st.applied;
    if(change == glm::vec2{0.f})
    {
        return;
    }
    // Pitch within the look's own clamp: what it couldn't take is not owed back.
    const float pitchWas = cl.viewangles[PITCH];
    cl.viewangles[PITCH] = za::clamp(pitchWas + change.x, cl_minpitch.value, cl_maxpitch.value);
    st.applied.x += cl.viewangles[PITCH] - pitchWas;
    cl.viewangles[YAW] += change.y;
    st.applied.y += change.y;
}

void keepHitReact(int h, glm::vec3& pos, glm::vec3& angles)
{
    (void)h;
    if(!flatplay::active())
    {
        return;
    }
    const float share = sightedShare(vr_flat_ads_hitreact);
    Probe& p = st.probe;
    p.rawKnock = za::max(p.rawKnock, glm::length(pos));
    pos *= share;
    angles *= share;
    p.drawnKnock = za::max(p.drawnKnock, glm::length(pos));
    p.share = share;
}

} // namespace qvr::flathit

extern "C" void VR_FlatHitDraw(void)
{
    using namespace qvr;
    using namespace qvr::flathit;
    if(!playing() || tune(vr_flat_hitdir, 0.f, 1.f) <= 0.f)
    {
        return;
    }
    // The crosshair's canvas: 0 0 the view's middle, its height vid.guiheight / scale.
    const float height = static_cast<float>(vid.guiheight) / za::clamp(scr_crosshairscale.value, 1.f, 10.f);
    const float inner = tune(vr_flat_hitdir_radius, 0.02f, 0.5f) * height;
    const float outer = inner + tune(vr_flat_hitdir_width, 0.002f, 0.1f) * height;
    const float half = glm::radians(tune(vr_flat_hitdir_arc, 5.f, 180.f)) * 0.5f;
    bool canvas = false;
    for(const Hit& h : st.hits)
    {
        const float alpha = markAlpha(h);
        if(alpha <= 0.f)
        {
            continue;
        }
        if(!canvas)
        {
            GL_SetCanvas(CANVAS_CROSSHAIR);
            canvas = true;
        }
        const float mid = glm::radians(markAngle(h));
        for(int i = 0; i < arcSegments; i++)
        {
            const float u0 = static_cast<float>(i) / arcSegments, u1 = static_cast<float>(i + 1) / arcSegments;
            const float a0 = mid - half + 2.f * half * u0, a1 = mid - half + 2.f * half * u1;
            // (Its ends fade: the middle of the piece, -1..1 along the arc.)
            const float along = (u0 + u1) - 1.f;
            const float xy[8] = {inner * za::sin(a0), -inner * za::cos(a0), outer * za::sin(a0), -outer * za::cos(a0),
                outer * za::sin(a1), -outer * za::cos(a1), inner * za::sin(a1), -inner * za::cos(a1)};
            Draw_FillQuad(xy, markRgb, alpha * (1.f - 0.7f * along * along));
        }
    }
}
