// vr_portals.cpp -- see vr_portals.hpp.

#include "vr_portals.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_move.hpp"
#include "vr_physics.hpp"
#include "vr_progs.hpp"
#include "vr_server.hpp"
#include "vr_stereo.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Tan.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string.h>

extern "C" void SV_AddToFatPVS(vec3_t org, mnode_t* node, qmodel_t* worldmodel); // sv_main.c

namespace qvr::portals
{
namespace
{

// The whole feature (vr_slipgates). With it off nothing here runs: no gate is ever built, none is looked through, no
// view is drawn through one, nothing is carried or traced through one, and Quake's trigger_teleport is what moves the
// player - exactly as before the feature existed. Every entry point below asks this first; update() forgets the gates
// built so far, so nothing else has anything to act on. Flipping it takes effect at once, no map reload.
[[nodiscard]] bool enabled()
{
    return vr_slipgates.value > 0.f;
}

constexpr float kReach = 24.f;    // how far round a trigger's brush its slipgate's faces may be
constexpr float kStand = 24.f;    // a standing player's origin over the floor
constexpr float kRange = 1536.f;  // how far from the head a gate is looked through
constexpr float kOblique = 0.25f; // the oblique near plane's slope in the depth (see VR_PortalClip)

// A side of a slipgate: its faces in one plane, seen from in front of it, and where they lead.
struct Side
{
    glm::vec3 normal{0.f}; // towards where it is seen from
    float dist = 0.f;
    glm::vec3 mins{0.f}, maxs{0.f};
    float area = 0.f;
    int leaf = -1;         // the leaf in front of it, in the PVS's bits (-1: none found: no PVS test)
    glm::vec3 from{0.f};   // its point carried onto `to`
    glm::vec3 to{0.f};
    glm::vec3 dest{0.f};   // the destination's point (where a player arrives)
    float yaw = 0.f;       // degrees a view's yaw turns going through
    glm::mat3 turn{1.f};   // the same turn
    int trigger = 0;       // its trigger_teleport's edict
};

za::Vector<Side> sides;
const qmodel_t* builtFor = nullptr;
int builtGeneration = -1;
int chosen = -1;  // the side looked through this frame (-1: none)
int lastChosen = -1;

bool inView = false;      // drawing the view through the gate
unsigned texture = 0;     // the view through the gate for textureEye's own view (0: none)
int textureEye = -1;
glm::vec4 eyeRect{-1.f, -1.f, 1.f, 1.f}; // the gate's box on the eye's screen (NDC), with a margin
bool eyeRectAll = true;   // ... or the whole screen (the gate's box reaches behind the eye)

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return {v[0], v[1], v[2]};
}

[[nodiscard]] glm::mat3 turnAboutZ(float degrees)
{
    return glm::mat3{glm::rotate(glm::mat4{1.f}, glm::radians(degrees), glm::vec3{0.f, 0.f, 1.f})};
}

[[nodiscard]] float surfaceArea(const qmodel_t* m, const msurface_t* s)
{
    glm::vec3 first{0.f}, prev{0.f}, sum{0.f};
    for(int k = 0; k < s->numedges; k++)
    {
        const int e = m->surfedges[s->firstedge + k];
        const glm::vec3 p = vec(m->vertexes[e >= 0 ? m->edges[e].v[0] : m->edges[-e].v[1]].position);
        if(k == 0)
        {
            first = p;
        }
        else if(k >= 2)
        {
            sum += glm::cross(prev - first, p - first);
        }
        prev = p;
    }
    return 0.5f * glm::length(sum);
}

[[nodiscard]] bool solidAt(qmodel_t* m, const glm::vec3& p)
{
    vec3_t q{p.x, p.y, p.z};
    return Mod_PointInLeaf(q, m)->contents == CONTENTS_SOLID;
}

// Where a side leads: the turn and the points carried onto each other (vr_portals.hpp), and the leaf in front of it.
void finish(Side& sd, qmodel_t* m, glm::vec3 dest, float destYaw)
{
    // The destination's point is a player's origin there, 27 units over its marker (info_teleport_destination): one
    // stands on the floor under it (a teleported player falls onto it), so that is where the gate leads.
    {
        vec3_t start{dest.x, dest.y, dest.z}, end{dest.x, dest.y, dest.z - 96.f};
        vec3_t mins{-16.f, -16.f, -24.f}, maxs{16.f, 16.f, 32.f};
        const trace_t tr = SV_Move(start, mins, maxs, end, MOVE_NOMONSTERS, nullptr);
        if(!tr.startsolid && tr.fraction < 1.f)
        {
            dest.z = tr.endpos[2];
        }
    }
    sd.dest = dest;
    const glm::vec3 c = (sd.mins + sd.maxs) * 0.5f;
    const glm::vec3 onPlane = c - sd.normal * (glm::dot(sd.normal, c) - sd.dist);
    if(za::fabs(sd.normal.z) < 0.7f)
    {
        // a wall's gate: walking into it is walking along the destination's yaw, its foot plus a standing origin's
        // height the destination's point
        const glm::vec2 into = glm::normalize(-glm::vec2{sd.normal});
        sd.yaw = destYaw - glm::degrees(za::atan2(into.y, into.x));
        sd.from = glm::vec3{onPlane.x, onPlane.y, sd.mins.z + kStand};
        sd.to = dest;
    }
    else
    {
        // a floor's gate: seen from over the destination's point (under its ceiling, at most 96 units); a ceiling's:
        // from its floor
        sd.yaw = destYaw;
        sd.from = onPlane;
        if(sd.normal.z > 0.f)
        {
            float clear = 8.f;
            while(clear < 96.f && !solidAt(m, dest + glm::vec3{0.f, 0.f, clear + 8.f}))
            {
                clear += 8.f;
            }
            sd.to = dest + glm::vec3{0.f, 0.f, za::max(clear - 8.f, 8.f)};
        }
        else
        {
            sd.to = dest - glm::vec3{0.f, 0.f, kStand - 2.f};
        }
    }
    sd.turn = turnAboutZ(sd.yaw);

    sd.leaf = -1;
    for(const float out : {8.f, 24.f})
    {
        vec3_t q{c.x + sd.normal.x * out, c.y + sd.normal.y * out, c.z + sd.normal.z * out};
        const mleaf_t* leaf = Mod_PointInLeaf(q, m);
        if(leaf && leaf->contents != CONTENTS_SOLID)
        {
            sd.leaf = static_cast<int>(leaf - m->leafs) - 1;
            break;
        }
    }
}

// The map's gates: each trigger_teleport's teleport faces, by plane, and its destination (the server's entities).
void build()
{
    sides.clear();
    builtFor = sv.worldmodel;
    builtGeneration = worldGeneration();
    qmodel_t* m = sv.worldmodel;
    if(!m)
    {
        return;
    }
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    const int mangleField = ED_FindFieldOffset("mangle");
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free || strcmp(PR_GetString(e->v.classname), "trigger_teleport"))
        {
            continue;
        }
        const char* target = PR_GetString(e->v.target);
        if(!target[0])
        {
            continue;
        }
        edict_t* d = nullptr;
        for(int j = 1; j < qcvm->num_edicts && !d; j++)
        {
            edict_t* t = EDICT_NUM(j);
            if(!t->free && !strcmp(PR_GetString(t->v.targetname), target))
            {
                d = t;
            }
        }
        if(!d)
        {
            continue;
        }
        const eval_t* mangle = mangleField >= 0 ? GetEdictFieldValue(d, mangleField) : nullptr;
        const float destYaw = mangle ? mangle->vector[1] : d->v.angles[1];
        const glm::vec3 lo = vec(e->v.absmin) - kReach, hi = vec(e->v.absmax) + kReach;

        const za::SizeT first = sides.size();
        for(int k = 0; k < m->nummodelsurfaces; k++)
        {
            const msurface_t* s = &m->surfaces[m->firstmodelsurface + k];
            const texture_t* t = s->texinfo ? m->textures[s->texinfo->texnum] : nullptr;
            if(!t || t->type != TEXTYPE_TELE || s->mins[0] > hi.x || s->mins[1] > hi.y || s->mins[2] > hi.z ||
                s->maxs[0] < lo.x || s->maxs[1] < lo.y || s->maxs[2] < lo.z)
            {
                continue;
            }
            const float sign = (s->flags & SURF_PLANEBACK) ? -1.f : 1.f;
            const glm::vec3 n = vec(s->plane->normal) * sign;
            const float dist = s->plane->dist * sign;
            // only the sides the trigger reaches out in front of (a sheet's two sides can lead to two places, each
            // its own trigger's)
            float front = -1e9f;
            for(int c = 0; c < 8; c++)
            {
                const glm::vec3 corner{(c & 1) ? e->v.absmax[0] : e->v.absmin[0], (c & 2) ? e->v.absmax[1] : e->v.absmin[1],
                    (c & 4) ? e->v.absmax[2] : e->v.absmin[2]};
                front = za::max(front, glm::dot(n, corner) - dist);
            }
            if(front < 0.5f)
            {
                continue;
            }
            Side* sd = nullptr;
            for(za::SizeT q = first; q < sides.size() && !sd; q++)
            {
                if(glm::dot(sides[q].normal, n) > 0.999f && za::fabs(sides[q].dist - dist) < 0.5f)
                {
                    sd = &sides[q];
                }
            }
            if(!sd)
            {
                sides.pushBack(Side{});
                sd = &sides.back();
                sd->normal = n;
                sd->dist = dist;
                sd->trigger = i;
                sd->mins = vec(s->mins);
                sd->maxs = vec(s->maxs);
            }
            sd->mins = glm::min(sd->mins, vec(s->mins));
            sd->maxs = glm::max(sd->maxs, vec(s->maxs));
            sd->area += surfaceArea(m, s);
        }
        // A gate's brush is a liquid's: its faces are drawn from inside it too, their fronts in it. Only the faces
        // looking out into the open are sides.
        for(za::SizeT q = first; q < sides.size();)
        {
            const Side& sd = sides[q];
            const glm::vec3 c = (sd.mins + sd.maxs) * 0.5f + sd.normal * 2.f;
            vec3_t out{c.x, c.y, c.z};
            if(Mod_PointInLeaf(out, m)->contents != CONTENTS_EMPTY)
            {
                sides[q] = sides.back();
                sides.popBack();
                continue;
            }
            finish(sides[q], m, vec(d->v.origin), destYaw);
            q++;
        }
    }
    PR_PopQCVM(oldVm);
    Con_DPrintf("VR portals: %d slipgate sides\n", static_cast<int>(sides.size()));
}

[[nodiscard]] bool current()
{
    return builtFor == sv.worldmodel && builtGeneration == worldGeneration();
}

[[nodiscard]] bool inPvs(const byte* vis, int leaf)
{
    return !vis || leaf < 0 || (vis[leaf >> 3] & (1 << (leaf & 7)));
}

[[nodiscard]] glm::vec3 nearestPoint(const Side& sd, const glm::vec3& p)
{
    return glm::clamp(p, sd.mins, sd.maxs);
}

// Whether side i is seen through another gate (the line from the eye to its middle crosses another side over its gate,
// from that side's front): the gate in front is the one looked through (a slipgate behind a slipgate, as in start).
[[nodiscard]] bool behindGate(za::SizeT i, const glm::vec3& eye)
{
    const glm::vec3 middle = (sides[i].mins + sides[i].maxs) * 0.5f;
    for(za::SizeT j = 0; j < sides.size(); j++)
    {
        const Side& o = sides[j];
        if(j == i || o.trigger == sides[i].trigger)
        {
            continue;
        }
        const float d0 = glm::dot(o.normal, eye) - o.dist, d1 = glm::dot(o.normal, middle) - o.dist;
        if(d0 <= 0.f || d1 >= -1.f)
        {
            continue;
        }
        const glm::vec3 c = eye + (middle - eye) * (d0 / (d0 - d1));
        if(!glm::any(glm::lessThan(c, o.mins - 1.f)) && !glm::any(glm::greaterThan(c, o.maxs + 1.f)))
        {
            return true;
        }
    }
    return false;
}

} // namespace

void update()
{
    chosen = -1;
    texture = 0;
    if(!enabled())
    {
        sides.clear(); // (the feature off: the gates are forgotten, and stay so until it is turned on again)
        builtFor = nullptr;
        builtGeneration = -1;
        lastChosen = -1;
        return;
    }
    if(vr_portals.value <= 0.f || !sv.active || sv.state != ss_active || !cl.worldmodel || cl.worldmodel != sv.worldmodel)
    {
        return;
    }
    if(!current())
    {
        build();
    }
    const hands::State& s = hands::current();
    if(sides.empty() || !s.valid)
    {
        return;
    }
    vec3_t h{s.head.x, s.head.y, s.head.z};
    mleaf_t* leaf = Mod_PointInLeaf(h, cl.worldmodel);
    if(!leaf || leaf->contents == CONTENTS_SOLID)
    {
        return;
    }
    const byte* vis = Mod_LeafPVS(leaf, cl.worldmodel);
    vec3_t angles{s.headAngles.x, s.headAngles.y, s.headAngles.z}, f, r, u;
    AngleVectors(angles, f, r, u);
    const glm::vec3 forward = vec(f);

    float best = 0.f;
    for(za::SizeT i = 0; i < sides.size(); i++)
    {
        const Side& sd = sides[i];
        if(glm::dot(sd.normal, s.head) - sd.dist < 0.f || !inPvs(vis, sd.leaf)) // (to the plane: the walk through it)
        {
            continue;
        }
        const float d = glm::distance(nearestPoint(sd, s.head), s.head);
        const glm::vec3 toMiddle = (sd.mins + sd.maxs) * 0.5f - s.head;
        const float len = glm::length(toMiddle);
        if(d > kRange || (len > 64.f && glm::dot(forward, toMiddle / len) < 0.2f) || behindGate(i, s.head))
        {
            continue;
        }
        const float score = sd.area / (d * d + 4096.f) * (static_cast<int>(i) == lastChosen ? 1.5f : 1.f);
        if(score > best)
        {
            best = score;
            chosen = static_cast<int>(i);
        }
    }
    lastChosen = chosen;
}

bool wantedForEye(int eye)
{
    texture = 0;
    textureEye = -1;
    if(!enabled() || chosen < 0 || eye < 0 || eye > 1)
    {
        return false;
    }
    const Side& sd = sides[static_cast<za::SizeT>(chosen)];
    const hands::State& s = hands::current();
    vec3_t angles{s.eyeAngles[eye].x, s.eyeAngles[eye].y, s.eyeAngles[eye].z}, f, r, u;
    AngleVectors(angles, f, r, u);
    const Fov& fov = frameState().eyes[eye].fov;
    const float l = za::tan(fov.left), rt = za::tan(fov.right), up = za::tan(fov.up), dn = za::tan(fov.down);
    if(rt - l <= 0.f || up - dn <= 0.f)
    {
        return false;
    }

    // The gate's box on the eye's screen (its corners; the whole screen if one is behind the eye or near it).
    glm::vec2 lo{1e9f}, hi{-1e9f};
    eyeRectAll = false;
    for(int c = 0; c < 8; c++)
    {
        const glm::vec3 corner{(c & 1) ? sd.maxs.x : sd.mins.x, (c & 2) ? sd.maxs.y : sd.mins.y, (c & 4) ? sd.maxs.z : sd.mins.z};
        const glm::vec3 v = corner - s.eyeOrigin[eye];
        const float x = glm::dot(v, vec(f));
        if(x < 2.f)
        {
            eyeRectAll = true;
            break;
        }
        const glm::vec2 t{glm::dot(v, vec(r)) / x, glm::dot(v, vec(u)) / x};
        const glm::vec2 ndc{(2.f * t.x - (rt + l)) / (rt - l), (2.f * t.y - (up + dn)) / (up - dn)};
        lo = glm::min(lo, ndc);
        hi = glm::max(hi, ndc);
    }
    if(!eyeRectAll)
    {
        if(hi.x < -1.f || hi.y < -1.f || lo.x > 1.f || lo.y > 1.f)
        {
            return false; // not on this eye's screen
        }
        eyeRect = glm::clamp(glm::vec4{lo - 0.02f, hi + 0.02f}, -1.f, 1.f);
    }
    return true;
}

void beginView()
{
    inView = true;
}

void endView(unsigned tex)
{
    inView = false;
    texture = tex;
    textureEye = stereo::eye();
}

bool viewing()
{
    return inView;
}

} // namespace qvr::portals

using namespace qvr;

// R_RenderView, before R_SetupView: the view through the gate moved there (the eye's own, set up with its entities,
// carried through the gate).
extern "C" void VR_PortalView(void)
{
    if(!portals::enabled() || !portals::inView || portals::chosen < 0)
    {
        return;
    }
    const portals::Side& sd = portals::sides[static_cast<za::SizeT>(portals::chosen)];
    const glm::vec3 o = sd.turn * (portals::vec(r_refdef.vieworg) - sd.from) + sd.to;
    for(int i = 0; i < 3; i++)
    {
        r_refdef.vieworg[i] = o[i];
    }
    r_refdef.viewangles[YAW] += sd.yaw;
}

// R_SetupView: in the view through the gate, the leaf it is seen from is the destination's, just beyond its point (the
// view itself is carried behind it, as far as the eye is from the gate: in a wall, or another room, whose PVS would
// leave the destination out, dark); else the view's own.
extern "C" mleaf_t* VR_PortalViewLeaf(mleaf_t* leaf)
{
    if(!portals::enabled() || !portals::inView || portals::chosen < 0)
    {
        return leaf;
    }
    const portals::Side& sd = portals::sides[static_cast<za::SizeT>(portals::chosen)];
    const glm::vec3 p = sd.to + sd.turn * -sd.normal * 8.f;
    vec3_t q{p.x, p.y, p.z};
    mleaf_t* dest = Mod_PointInLeaf(q, cl.worldmodel);
    return dest && dest->contents != CONTENTS_SOLID ? dest : leaf;
}

// R_SetFrustum, its view matrix made (Quake's view space: x forward, y left, z up), its projection's not yet multiplied
// in: in the view through the gate, the projection's depth row made oblique (Lengyel's), its near plane the gate's
// plane carried to the destination: with reversed Z, depth = 1 - kOblique * (P . v) / w, P the plane (in view space,
// positive beyond the gate, normalised), w the distance along the view. Nothing between the view and that plane is
// drawn (its depth over 1); beyond it, the depth falls as 1/distance as the usual reversed Z's does, and stays over 0
// within the eyes' field of view (kOblique under the cosine of its widest angle): no far plane.
extern "C" void VR_PortalClip(float proj[16], const float view[16])
{
    if(!portals::enabled() || !portals::inView || portals::chosen < 0 || !gl_clipcontrol_able)
    {
        return;
    }
    const portals::Side& sd = portals::sides[static_cast<za::SizeT>(portals::chosen)];
    const glm::vec3 n = sd.turn * sd.normal;
    const glm::vec4 world{-n, glm::dot(n, sd.to)};
    glm::vec4 p = world * glm::inverse(glm::make_mat4(view));
    p /= glm::length(glm::vec3{p});
    p.w = za::min(p.w, -1.f); // the view at least a unit in front of the plane: the depth's precision
    for(int c = 0; c < 4; c++)
    {
        proj[c * 4 + 2] = proj[c * 4 + 3] - portals::kOblique * p[c];
    }
}

// R_RenderScene, after the clear and the lenses' hidden area: in the view through the gate, the depth outside the
// gate's box on the eye's screen set to the near plane, so nothing there is shaded (only the gate's pixels are read).
extern "C" void VR_DrawPortalMask(void)
{
    if(!portals::enabled() || !portals::inView || portals::eyeRectAll || !gl_clipcontrol_able)
    {
        return;
    }
    const glm::vec4 r = portals::eyeRect;
    const glm::vec2 quads[4][2] = {{{-1.f, -1.f}, {1.f, r.y}}, {{-1.f, r.w}, {1.f, 1.f}}, {{-1.f, r.y}, {r.x, r.w}},
        {{r.z, r.y}, {1.f, r.w}}};
    za::Vector<gfx::Vertex> tris;
    for(const auto& q : quads)
    {
        if(q[1].x <= q[0].x || q[1].y <= q[0].y)
        {
            continue;
        }
        const glm::vec3 a{q[0].x, q[0].y, 1.f}, b{q[1].x, q[0].y, 1.f}, c{q[1].x, q[1].y, 1.f}, d{q[0].x, q[1].y, 1.f};
        for(const glm::vec3& p : {a, b, c, a, c, d})
        {
            gfx::Vertex v;
            v.pos = p;
            v.color = glm::vec4{0.f};
            tris.pushBack(v);
        }
    }
    if(tris.empty())
    {
        return;
    }
    gfx::State state;
    state.shade = gfx::Shade::Color;
    state.blend = gfx::Blend::Opaque;
    state.depthTest = true; // passes against the clear (GL skips depth writes without the test)
    state.depthWrite = true;
    gfx::draw(tris, glm::mat4{1.f}, state);
}

// VR_WaterView (each view's frame data): the side shown on the teleport faces in this eye's view (gl_shaders.h,
// LiquidPortal): its plane (normal towards the eye, distance), its box (w: how much of the view is shown, 0 none).
extern "C" void VR_PortalFrameData(float plane[4], float mins[4], float maxs[4])
{
    plane[0] = plane[1] = plane[2] = plane[3] = 0.f;
    mins[0] = mins[1] = mins[2] = mins[3] = 0.f;
    maxs[0] = maxs[1] = maxs[2] = maxs[3] = 0.f;
    if(!portals::enabled() || portals::inView || !portals::texture || portals::chosen < 0 || !stereo::isRenderingEye() ||
        stereo::isSpectator() || stereo::eye() != portals::textureEye)
    {
        return;
    }
    const portals::Side& sd = portals::sides[static_cast<za::SizeT>(portals::chosen)];
    for(int i = 0; i < 3; i++)
    {
        plane[i] = sd.normal[i];
        mins[i] = sd.mins[i];
        maxs[i] = sd.maxs[i];
    }
    plane[3] = sd.dist;
    mins[3] = za::clamp(vr_portals.value, 0.f, 1.f);
}

// R_DrawBrushModels_Water: the view through the gate for this eye's teleport faces (unit 17, PortalScene; 0: none).
extern "C" unsigned VR_PortalTexture(void)
{
    const bool mine =
        portals::enabled() && !portals::inView && stereo::isRenderingEye() && !stereo::isSpectator() && stereo::eye() == portals::textureEye;
    return mine ? portals::texture : 0u;
}

// SV_WriteEntitiesToClient, the client's PVS made: the PVS round the destinations of the gates in it (in front of
// them, near the client) added, so that what is there is sent and seen through them.
extern "C" void VR_PortalAddPVS(byte* pvs, const float org[3])
{
    if(!portals::enabled() || vr_portals.value <= 0.f || !sv.worldmodel || sv.state != ss_active)
    {
        return;
    }
    if(!portals::current())
    {
        portals::build();
    }
    const glm::vec3 o = portals::vec(org);
    const glm::vec3* added[8] = {};
    int numAdded = 0;
    for(const portals::Side& sd : portals::sides)
    {
        if(numAdded == 8)
        {
            break;
        }
        if(glm::dot(sd.normal, o) - sd.dist < 0.f || !portals::inPvs(pvs, sd.leaf) ||
            glm::distance(portals::nearestPoint(sd, o), o) > portals::kRange)
        {
            continue;
        }
        bool seen = false;
        for(int i = 0; i < numAdded && !seen; i++)
        {
            seen = *added[i] == sd.dest;
        }
        if(seen)
        {
            continue;
        }
        added[numAdded++] = &sd.dest;
        vec3_t d{sd.dest.x, sd.dest.y, sd.dest.z};
        SV_AddToFatPVS(d, sv.worldmodel->nodes, sv.worldmodel);
    }
}

// ----------------------------------------------------------------------------
// Walking and shooting through (vr_portals_walk): the server carries a player through a slipgate as the head comes to
// its plane, and what flies (missiles, grenades, gibs) as its path crosses it, by the side's own mapping: where it is,
// how it moves and where it looks are kept, turned and shifted as the view through the gate shows them, so the view
// does not jump. QuakeC's teleport_touch leaves players to it (portal_handles) unless one stays in the trigger a while
// without reaching the plane (a gate behind bars: Quake's teleport then); what the teleport does besides moving him is
// QuakeC's (VR_Portal_Crossed: the trigger's targets, what the hands carry). Monsters still teleport as in Quake.

namespace qvr::portals
{
namespace
{

constexpr float kCross = 24.f;    // how near the plane the body is watched for its crossing
constexpr float kAperture = 0.f;  // how far outside a gate's faces its aperture still counts as its opening
constexpr double kStuck = 1.0;    // seconds in a gate's trigger, not on his way through it, before Quake's teleport takes over
constexpr double kCooldown = 0.5; // seconds after a crossing before the next (no bouncing between two gates)

// Each client's time in a seamless gate's trigger (portal_handles) and last crossing.
struct ClientState
{
    int trigger = 0;
    double first = -1.0;
    double last = -1e9;
    double crossed = -1e9;
};
ClientState clients[MAX_SCOREBOARD];

[[nodiscard]] bool walkOn()
{
    return enabled() && vr_portals.value > 0.f && vr_portals_walk.value > 0.f && sv.active && sv.state == ss_active &&
        sv.worldmodel;
}

// As teleport_touch: a trigger waiting to be fired (a targetname, not IGNORE_TARGETNAME) is shut until then.
[[nodiscard]] bool triggerActive(const edict_t* trig)
{
    if(trig->free || static_cast<int>(trig->v.solid) != SOLID_TRIGGER)
    {
        return false;
    }
    return !PR_GetString(trig->v.targetname)[0] || (static_cast<int>(trig->v.spawnflags) & 8) || trig->v.nextthink >= qcvm->time;
}

[[nodiscard]] bool overlaps(const edict_t* a, const edict_t* b)
{
    for(int i = 0; i < 3; i++)
    {
        if(a->v.absmin[i] > b->v.absmax[i] + 1.f || a->v.absmax[i] < b->v.absmin[i] - 1.f)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] glm::vec3 carried(const Side& sd, const glm::vec3& p)
{
    return sd.turn * (p - sd.from) + sd.to;
}

[[nodiscard]] bool onGate(const Side& sd, const glm::vec3& onPlane, float margin)
{
    return !glm::any(glm::lessThan(onPlane, sd.mins - margin)) && !glm::any(glm::greaterThan(onPlane, sd.maxs + margin));
}

// The player's head (the client's, from its box; else Quake's view).
[[nodiscard]] glm::vec3 headOf(edict_t* ent)
{
    const VrMove* move = server::clientMove(ent);
    const glm::vec3 origin = vec(ent->v.origin);
    return move ? origin + (move->headPos - move->origin) : origin + vec(ent->v.view_ofs);
}

// The player's torso, as his collision box has it: the point on the body's own axis where its middle plane is, the
// plane that halves the box (his feet to the top of his head). A gate takes the player when that plane is through its
// plane: leaning in, reaching with the hands, or brushing the aperture does not cross. The box and not the head, because
// the box is what the world stops: what he collides with and what goes through are then the same thing.
[[nodiscard]] glm::vec3 torsoOf(edict_t* ent)
{
    return vec(ent->v.origin) + glm::vec3{0.f, 0.f, 0.5f * (ent->v.mins[2] + ent->v.maxs[2])};
}

// The side of trigger t whose plane `at` is nearest (within kCross of it, `at` over its aperture): the gate he stands
// in the middle of. nullptr when there is none.
[[nodiscard]] const Side* gateOf(int t, const glm::vec3& at)
{
    const Side* best = nullptr;
    float bestD = kCross;
    for(const Side& sd : sides)
    {
        if(sd.trigger != t)
        {
            continue;
        }
        const float d = glm::dot(sd.normal, at) - sd.dist;
        if(za::fabs(d) >= bestD || !onGate(sd, at - sd.normal * d, kAperture))
        {
            continue;
        }
        best = &sd;
        bestD = za::fabs(d);
    }
    return best;
}

// Whether `at` is over a side of trigger t's gate, within kCross of its plane: the engine's to carry through.
[[nodiscard]] bool overGate(int t, const glm::vec3& at)
{
    return gateOf(t, at) != nullptr;
}

[[nodiscard]] bool blocked(edict_t* ent, const glm::vec3& p)
{
    vec3_t q{p.x, p.y, p.z};
    return SV_Move(q, ent->v.mins, ent->v.maxs, q, MOVE_NORMAL, ent).startsolid;
}

// His box swept at the gate's plane: how near that plane his torso's middle plane can be brought (a sill, a ledge or
// bars just past the plane stop the box short). 0 or under: it reaches it and goes through.
[[nodiscard]] float reachOf(edict_t* ent, const Side& sd, const glm::vec3& origin, float d)
{
    if(d <= 0.f)
    {
        return 0.f;
    }
    vec3_t start{origin.x, origin.y, origin.z};
    const glm::vec3 to = origin - sd.normal * (d + 1.f);
    vec3_t end{to.x, to.y, to.z};
    const trace_t tr = SV_Move(start, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);
    return tr.startsolid ? d : d - tr.fraction * (d + 1.f);
}

// Whether the engine is the one to take him through this trigger's gate: his torso's middle plane over its aperture (he
// stands in the gate, and goes through its plane as he goes on), or his box stopped on the way to that plane (he is
// against the gate's frame, or against something before it: nothing is to teleport him, he collided). Neither: a trigger
// he is only brushing, or a gate his body can never reach (bars, a ledge) - which is Quake's teleport, after kStuck.
[[nodiscard]] bool engineCarries(edict_t* who, int t)
{
    const glm::vec3 torso = torsoOf(who);
    if(overGate(t, torso))
    {
        return true;
    }
    const Side* sd = nullptr;
    float d = 1e9f;
    for(const Side& s : sides)
    {
        if(s.trigger != t)
        {
            continue;
        }
        const float k = glm::dot(s.normal, torso) - s.dist;
        if(k < -kCross || k > kCross || za::fabs(k) >= za::fabs(d))
        {
            continue;
        }
        sd = &s;
        d = k;
    }
    return sd && reachOf(who, *sd, vec(who->v.origin), d) >= d - 1.f;
}

void setVec(float* out, const glm::vec3& v)
{
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// The player through the side: kept where he is and how he moves relative to the gate, his view turned with it (the
// head's yaw, svc_setangle: the client turns the play space so the head faces it). Where he does not fit there, a
// little further on, else at the destination's own point as Quake's teleport would.
void crossPlayer(edict_t* ent, const Side& sd, edict_t* trig, ClientState& cs)
{
    const glm::vec3 from = vec(ent->v.origin);
    glm::vec3 to = carried(sd, from);
    const glm::vec3 ahead = sd.turn * -sd.normal;
    bool fits = false;
    for(float step = 0.f; step <= 48.f && !fits; step += 8.f)
    {
        if(!blocked(ent, to + ahead * step))
        {
            to += ahead * step;
            fits = true;
        }
    }
    if(!fits)
    {
        if(blocked(ent, sd.dest))
        {
            return; // nowhere to go yet (something stands there): next frame
        }
        to = sd.dest;
    }

    setVec(ent->v.origin, to);
    setVec(ent->v.oldorigin, to);
    setVec(ent->v.velocity, sd.turn * vec(ent->v.velocity));
    const float* head = server::clientHeadAngles(ent);
    const float yaw = (head ? head[YAW] : ent->v.v_angle[YAW]) + sd.yaw;
    ent->v.angles[PITCH] = ent->v.angles[ROLL] = 0.f;
    ent->v.angles[YAW] = anglemod(yaw);
    ent->v.v_angle[YAW] = anglemod(ent->v.v_angle[YAW] + sd.yaw);
    ent->v.fixangle = 1;
    ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
    SV_LinkEdict(ent, true);
    cs.crossed = qcvm->time;
    cs.first = -1.0;
    Con_DPrintf("VR portal: carried edict %d through side %d: %.0f %.0f %.0f -> %.0f %.0f %.0f\n", NUM_FOR_EDICT(ent),
        static_cast<int>(&sd - sides.data()), from.x, from.y, from.z, to.x, to.y, to.z);

    // This machine's player (single player, a listen server's host): its client turns the play space by the gate's yaw
    // and keeps the head's lean and the stairs' easing through the jump, so that the view goes on as it was.
    if(cls.state == ca_connected && svs.maxclients >= 1 && ent == svs.clients[0].edict)
    {
        hands::portalCrossing(sd.yaw);
    }

    // What Quake's teleport does besides moving him (QuakeC: its targets, what the hands carry).
    if(const func_t fn = progs::findFunction("VR_Portal_Crossed"))
    {
        const int oldSelf = pr_global_struct->self;
        const int oldOther = pr_global_struct->other;
        pr_global_struct->self = EDICT_TO_PROG(trig);
        pr_global_struct->other = EDICT_TO_PROG(ent);
        pr_global_struct->time = qcvm->time;
        setVec(G_VECTOR(OFS_PARM0), from);
        PR_ExecuteProgram(fn);
        pr_global_struct->self = oldSelf;
        pr_global_struct->other = oldOther;
    }
}

} // namespace
} // namespace qvr::portals

// VR_ClientSpecialMove (SV_Physics_Client, before the move): the player carried through a seamless slipgate (its
// trigger touched, active) as his head comes within kCross of its plane, over the gate.
extern "C" void VR_PortalClientCross(edict_t* ent)
{
    using namespace qvr::portals;
    if(!walkOn())
    {
        return;
    }
    if(!current())
    {
        build();
    }
    const int num = NUM_FOR_EDICT(ent) - 1;
    if(sides.empty() || num < 0 || num >= MAX_SCOREBOARD || ent->v.health <= 0.f)
    {
        return;
    }
    ClientState& cs = clients[num];
    if(qcvm->time - cs.crossed < kCooldown)
    {
        return;
    }
    const glm::vec3 origin = vec(ent->v.origin);
    const glm::vec3 torso = torsoOf(ent);
    // The gate his body stands in the middle of: the side whose plane his torso's middle plane is nearest to (a
    // sheet's two sides: the one his body is in, not the one behind it). Its trigger touched and active, as Quake's
    // teleport would need it.
    const Side* best = nullptr;
    float bestD = kCross;
    for(const Side& sd : sides)
    {
        const edict_t* trig = EDICT_NUM(sd.trigger);
        if(!triggerActive(trig) || !overlaps(ent, trig))
        {
            continue;
        }
        const float d = glm::dot(sd.normal, torso) - sd.dist;
        if(za::fabs(d) >= bestD || !onGate(sd, torso - sd.normal * d, kAperture))
        {
            continue;
        }
        best = &sd;
        bestD = za::fabs(d);
    }
    if(!best)
    {
        return;
    }
    // Through when his middle plane is past the gate's (the view goes on through the gate, the room round it never seen
    // to change), or as near to it as his box can bring it (a sill, a ledge or bars just past the plane stop the box
    // short: as through as his body can get is through).
    const float d = glm::dot(best->normal, torso) - best->dist;
    if(d <= za::max(reachOf(ent, *best, origin, d), 0.f) + 1.f)
    {
        crossPlayer(ent, *best, EDICT_NUM(best->trigger), cs);
    }
}

// SV_Physics_Toss, before the move: what flies (missiles, grenades, gibs) carried through a seamless slipgate as this
// frame's path crosses its plane over the gate (from the front, nothing in the way), its speed and heading turned with it.
extern "C" void VR_PortalToss(edict_t* ent)
{
    using namespace qvr::portals;
    if(!walkOn())
    {
        return;
    }
    if(!current())
    {
        build();
    }
    // Its middle's path (a box's origin is at its foot: one sliding on a floor under a gate's sill is in its gate): a
    // model's drawn middle (a rigid body's box is its model's, not its Quake box: physics::modelCentre), else its box's.
    const glm::vec3 mid = physics::modelCentre(ent) - vec(ent->v.origin);
    const glm::vec3 o = vec(ent->v.origin) + mid, v = vec(ent->v.velocity);
    if(sides.empty() || glm::dot(v, v) < 1.f)
    {
        return;
    }
    const glm::vec3 e = o + v * static_cast<float>(host_frametime);
    for(const Side& sd : sides)
    {
        const float d0 = glm::dot(sd.normal, o) - sd.dist, d1 = glm::dot(sd.normal, e) - sd.dist;
        if(d0 < 0.f || d1 >= 0.f)
        {
            continue;
        }
        const glm::vec3 c = o + (e - o) * (d0 / (d0 - d1));
        edict_t* trig = EDICT_NUM(sd.trigger);
        if(!onGate(sd, c, 2.f) || !triggerActive(trig) || (static_cast<int>(trig->v.spawnflags) & 1)) // PLAYER_ONLY
        {
            continue;
        }
        // Its middle's way to the plane clear (its box, wider than the gate's brush, would reach the wall behind it).
        vec3_t start{o.x, o.y, o.z};
        vec3_t end{c.x, c.y, c.z};
        const trace_t tr = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NOMONSTERS, ent);
        if(tr.startsolid || tr.fraction < 1.f)
        {
            continue; // it hits something on the way
        }
        // Its middle carried through (its box's offset kept: it is axis-aligned), a little further on where it does not fit.
        const glm::vec3 dir = sd.turn * v, ahead = glm::normalize(dir);
        glm::vec3 to = carried(sd, c) - mid;
        bool fits = false;
        for(float step = 1.f; step <= 33.f && !fits; step += 8.f)
        {
            fits = !blocked(ent, to + ahead * step);
            if(fits)
            {
                to += ahead * step;
            }
        }
        if(!fits)
        {
            continue;
        }
        setVec(ent->v.origin, to);
        setVec(ent->v.oldorigin, to);
        setVec(ent->v.velocity, dir);
        ent->v.angles[YAW] = anglemod(ent->v.angles[YAW] + sd.yaw);
        const int spinField = ED_FindFieldOffset("vr_spin"); // a rigid body's spin (rad/s, the world's axes)
        if(spinField >= 0)
        {
            eval_t* spin = GetEdictFieldValue(ent, spinField);
            setVec(spin->vector, sd.turn * vec(spin->vector));
        }
        SV_LinkEdict(ent, false);
        return;
    }
}

// QuakeC (portal_handles, teleport_touch): 1 if the engine carries this player through the trigger's slipgate
// (vr_portals_walk), so that Quake's teleport leaves him be; 0 after kStuck seconds in it without reaching the plane.
extern "C" float VR_PortalHandles(edict_t* trig, edict_t* who)
{
    using namespace qvr::portals;
    const int num = NUM_FOR_EDICT(who) - 1;
    if(!walkOn() || num < 0 || num >= svs.maxclients || num >= MAX_SCOREBOARD)
    {
        return 0.f;
    }
    if(!current())
    {
        build();
    }
    const int t = NUM_FOR_EDICT(trig);
    bool gate = false;
    for(const Side& sd : sides)
    {
        gate = gate || sd.trigger == t;
    }
    if(!gate)
    {
        return 0.f;
    }
    ClientState& cs = clients[num];
    const bool mine = engineCarries(who, t);
    if(cs.trigger != t || qcvm->time - cs.last > 0.3 || (cs.first < 0.0) != mine)
    {
        cs.trigger = t; // (his to take: the time Quake's teleport waits does not run; it counts from when it is not)
        cs.first = mine ? -1.0 : qcvm->time;
    }
    cs.last = qcvm->time;
    return cs.first < 0.0 || qcvm->time - cs.first < kStuck ? 1.f : 0.f;
}

namespace qvr::portals
{

// vr_portals_info (Debug > Slipgates): the gates built for this map, and where the local player's body is against each
// of them - his box, his torso's middle plane, its distance from the gate's plane, whether it is over the aperture and
// how near his box can bring it. For checking a crossing by hand, and for finding a gate's geometry in a map.
void infoBody()
{
    Con_Printf("VR portals: %d sides (vr_slipgates %g, vr_portals %g, vr_portals_walk %g)\n", static_cast<int>(sides.size()),
        vr_slipgates.value, vr_portals.value, vr_portals_walk.value);
    for(int i = 0; i < static_cast<int>(sides.size()); i++)
    {
        const Side& sd = sides[static_cast<za::SizeT>(i)];
        const edict_t* trig = EDICT_NUM(sd.trigger);
        Con_Printf("  side %d: trigger %d plane (%.2f %.2f %.2f) %g\n", i, sd.trigger, sd.normal.x, sd.normal.y,
            sd.normal.z, sd.dist);
        Con_Printf("      aperture (%.0f %.0f %.0f)-(%.0f %.0f %.0f) area %.0f  trigger brush (%.0f %.0f %.0f)-(%.0f "
                   "%.0f %.0f)\n",
            sd.mins.x, sd.mins.y, sd.mins.z, sd.maxs.x, sd.maxs.y, sd.maxs.z, sd.area, trig->v.absmin[0],
            trig->v.absmin[1], trig->v.absmin[2], trig->v.absmax[0], trig->v.absmax[1], trig->v.absmax[2]);
        Con_Printf("      from (%.0f %.0f %.0f) to (%.0f %.0f %.0f) yaw %.1f\n", sd.from.x, sd.from.y, sd.from.z,
            sd.to.x, sd.to.y, sd.to.z, sd.yaw);
    }
    if(svs.maxclients < 1 || !svs.clients[0].edict)
    {
        return;
    }
    edict_t* ent = svs.clients[0].edict;
    const glm::vec3 origin = vec(ent->v.origin), torso = torsoOf(ent), head = headOf(ent);
    Con_Printf("  player at (%.0f %.0f %.0f) box (%.0f %.0f %.0f)-(%.0f %.0f %.0f)\n", origin.x, origin.y, origin.z,
        origin.x + ent->v.mins[0], origin.y + ent->v.mins[1], origin.z + ent->v.mins[2], origin.x + ent->v.maxs[0],
        origin.y + ent->v.maxs[1], origin.z + ent->v.maxs[2]);
    Con_Printf("      torso (%.0f %.0f %.0f) head (%.0f %.0f %.0f)\n", torso.x, torso.y, torso.z, head.x, head.y, head.z);
    for(const Side& sd : sides)
    {
        const float dt = glm::dot(sd.normal, torso) - sd.dist, dh = glm::dot(sd.normal, head) - sd.dist;
        if(za::fabs(dt) > kCross * 4.f)
        {
            continue;
        }
        Con_Printf("      side %d: torso %s %g (head %g), over the aperture %d, his box reaches %g -> %s\n",
            static_cast<int>(&sd - sides.data()), dt >= 0.f ? "in front of" : "past", dt, dh,
            onGate(sd, torso - sd.normal * dt, kAperture) ? 1 : 0, reachOf(ent, sd, origin, dt),
            engineCarries(ent, sd.trigger) ? "the engine carries him" : "Quake's teleport");
    }
}

void info_f()
{
    if(!sv.worldmodel)
    {
        Con_Printf("VR portals: no server world.\n");
        return;
    }
    if(!enabled())
    {
        Con_Printf("VR portals: off (vr_slipgates 0): no gates built, nothing goes through one, Quake's teleporters.\n");
        return;
    }
    if(!current())
    {
        build();
    }
    qcvm_t* oldVm = nullptr; // (the console's qcvm is not the server's: the sides name the server's edicts)
    PR_PushQCVM(&sv.qcvm, &oldVm);
    infoBody();
    PR_PopQCVM(oldVm);
}

void registerCommands()
{
    Cmd_AddCommand("vr_portals_info", info_f);
}

} // namespace qvr::portals

// ----------------------------------------------------------------------------
// Shots through (MOVE_PORTALS traces; vr_portals_walk): a traceline that crosses a side's plane from the front over the
// gate before it hits anything (a gate on a wall: at the wall) goes on from the destination's side, turned and shifted by
// the side's mapping, for the rest of its length (two gates at most). The trace's results are the last part's (the
// fraction of the whole length); QuakeC asks where it went into each gate and came out (portal_entry, portal_exit) and
// turns its directions as the shot was turned (portal_turn): the beams in two pieces, the blood and pushes the right way.

namespace qvr::portals
{
namespace
{

constexpr int kMaxTraceCrossings = 2;
int traceCrossings = 0;
glm::vec3 traceEntry[kMaxTraceCrossings];
glm::vec3 traceExit[kMaxTraceCrossings];
glm::mat3 traceTurn{1.f};

} // namespace
} // namespace qvr::portals

extern "C" void VR_PortalTraceBegin(void)
{
    qvr::portals::traceCrossings = 0;
    qvr::portals::traceTurn = glm::mat3{1.f};
}

extern "C" void VR_PortalTrace(const float start[3], const float end[3], int type, edict_t* passedict, trace_t* trace)
{
    using namespace qvr::portals;
    if(!walkOn())
    {
        return;
    }
    if(!current())
    {
        build();
    }
    glm::vec3 s = vec(start), e = vec(end);
    const float total = glm::distance(s, e);
    if(sides.empty() || total < 1.f)
    {
        return;
    }
    float before = 0.f; // the length gone before the current part
    for(int n = 0; n < kMaxTraceCrossings; n++)
    {
        const float len = glm::distance(s, e);
        if(trace->startsolid || len < 1.f)
        {
            break;
        }
        // The nearest gate the part crosses from its front, not past what it hits (a gate on a wall: at the wall).
        const Side* hit = nullptr;
        float bestT = 2.f;
        glm::vec3 at{0.f};
        for(const Side& sd : sides)
        {
            const float d0 = glm::dot(sd.normal, s) - sd.dist, d1 = glm::dot(sd.normal, e) - sd.dist;
            if(d0 < 0.f || d1 >= 0.f)
            {
                continue;
            }
            const float t = d0 / (d0 - d1);
            if(t >= bestT || t * len > trace->fraction * len + 1.f)
            {
                continue;
            }
            const glm::vec3 c = s + (e - s) * t;
            if(!onGate(sd, c, 1.f) || !triggerActive(EDICT_NUM(sd.trigger)))
            {
                continue;
            }
            hit = &sd;
            bestT = t;
            at = c;
        }
        if(!hit)
        {
            break;
        }
        const glm::vec3 dir = hit->turn * glm::normalize(e - s);
        const glm::vec3 from = carried(*hit, at) + dir * 0.5f;
        const glm::vec3 to = carried(*hit, e);
        traceEntry[n] = at;
        traceExit[n] = from;
        traceTurn = hit->turn * traceTurn;
        traceCrossings = n + 1;
        before += bestT * len;
        s = from;
        e = to;
        vec3_t a{s.x, s.y, s.z}, b{e.x, e.y, e.z};
        *trace = SV_Move(a, vec3_origin, vec3_origin, b, type, passedict);
    }
    if(traceCrossings)
    {
        trace->fraction = za::min((before + trace->fraction * glm::distance(s, e)) / total, 1.f);
    }
}

// QuakeC: the last MOVE_PORTALS traceline's gates (portal_crossings), where it went into the i-th and came out of it
// (portal_entry, portal_exit), a direction turned as it was (portal_turn).
extern "C" float VR_PortalCrossings(void)
{
    return static_cast<float>(qvr::portals::enabled() ? qvr::portals::traceCrossings : 0);
}

extern "C" void VR_PortalEntry(int i, float out[3])
{
    using namespace qvr::portals;
    const glm::vec3 p = enabled() && i >= 0 && i < traceCrossings ? traceEntry[i] : glm::vec3{0.f};
    setVec(out, p);
}

extern "C" void VR_PortalExit(int i, float out[3])
{
    using namespace qvr::portals;
    const glm::vec3 p = enabled() && i >= 0 && i < traceCrossings ? traceExit[i] : glm::vec3{0.f};
    setVec(out, p);
}

extern "C" void VR_PortalTurn(const float v[3], float out[3])
{
    using namespace qvr::portals;
    setVec(out, enabled() ? traceTurn * vec(v) : vec(v));
}
