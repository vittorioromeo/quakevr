// vr_portals.cpp -- see vr_portals.hpp.

#include "vr_portals.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_gfx.hpp"
#include "vr_hands.hpp"
#include "vr_held.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_move.hpp"
#include "vr_physics.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_stereo.hpp"
#include "vr_view.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Container/Array.hpp"
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
// A body this near a gate's plane (either side, over its opening) straddles it. The server carries a player at the start
// of a tick (VR_PortalClientCross, before the move), so the tick whose move takes his torso past the plane sends him
// still behind it, not yet carried, while the client's eye is already through (eyeThrough: drawn from the
// destination); and once carried, an eye still behind the exit's face is drawn from the source. VR_PortalAddPVS sends
// both rooms' entities while he straddles a gate: else the room he sees lost its doors, lifts and floors (start's
// pentagram floor, func_bossgate) for that one frame, seen through.
constexpr float kStraddle = 32.f;
[[nodiscard]] bool triggerActive(const edict_t* trig);

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
    int sourceTarget = 0;  // Native authored retargeting invalidates the cached destination immediately.
    bool paired = false;   // it comes out of another gate's face (pairExits): its exit (reverseSide) is a real aperture
};

struct PortalScratch
{
    za::Vector<gfx::Vertex> mask;
    za::Vector<byte> sourcePvs;
    za::Vector<glm::vec3> added;
    auto members() { return mem::list(mask, sourcePvs, added); }
};
mem::Scratch<PortalScratch> scratch{"portal mask and PVS"};

za::Vector<Side> sides;
[[nodiscard]] Side reverseSide(const Side& entry);
[[nodiscard]] bool onGate(const Side& sd, const glm::vec3& onPlane, float margin);
const qmodel_t* builtFor = nullptr;
int builtGeneration = -1;
float builtPairExits = -1.f; // vr_slipgate_pair_exits as built (pairExits)
int chosen = -1;  // the side looked through this frame (-1: none)
int lastChosen = -1;
struct ViewCandidate { int side = -1; float score = 0.f; };
za::Array<ViewCandidate, 8> candidates;
za::Array<int, 8> renderedSides;
int candidateViews = 0, renderedViews = 0; // current camera only; layers are copied before its main scene

glm::vec3 chosenOrigin{0.f}, chosenAngles{0.f}; // last rendered camera, for flat/eye/spectator diagnostics

bool inView = false;      // drawing the view through the gate
unsigned texture = 0;     // destination texture array for the current camera (0: none)
int textureEye = -1;      // eye identifier for diagnostic capture filenames
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

// The exact same rigid transform as movement and shots. Screen-coordinate sampling in LiquidPortal requires this
// camera: moving it nearer without changing the projection makes the view and a shot through that pixel disagree.
// VR_PortalView draws from here; vr_portals_view and vr_portals_shot report it.
[[nodiscard]] glm::vec3 carriedView(const Side& sd, const glm::vec3& head)
{
    return sd.turn * (head - sd.from) + sd.to;
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

// A gate whose destination stands in front of another gate of its size, facing the way one walks out (a two-way pair, as
// vrslipgates' and most custom maps' are: the destination marker stands clear of that gate's trigger, 48 units out, for
// Quake's teleport of monsters): the crossing comes out of that gate's face, not out of the open air in front of it. Else
// the player popped out 48 units past the gate he seemed to walk out of, the view through showed the room from there,
// and the exit plane (reverseSide) stood in the middle of the room: a prop or a held object straddling it there was cut
// and drawn again back at the entrance, and Box3D dropped its contacts behind that plane (ROUND21.md, "Slipgates: exits
// on their gates"). `to` is moved so that the carried aperture is that gate's (its middle onto that gate's middle: a
// sill's height too, which the destination marker on the floor left out).
// A side's aperture's two extents in its plane (a wall's: its width along the wall and its height; a floor's: its sides,
// the shorter first), from its box (a slanted wall's face spans its box corner to corner).
[[nodiscard]] glm::vec2 apertureExtents(const Side& sd)
{
    const glm::vec3 size = sd.maxs - sd.mins;
    if(za::fabs(sd.normal.z) < 0.7f)
    {
        const glm::vec3 along = glm::normalize(glm::cross(sd.normal, glm::vec3{0.f, 0.f, 1.f}));
        return {za::fabs(size.x * along.x) + za::fabs(size.y * along.y), size.z};
    }
    return {za::min(size.x, size.y), za::max(size.x, size.y)};
}

void pairExits()
{
    if(vr_slipgate_pair_exits.value <= 0.f) { return; }
    for(za::SizeT i = 0; i < sides.size(); i++)
    {
        Side& sd = sides[i];
        const Side exit = reverseSide(sd);
        const glm::vec3 exitMid = (exit.mins + exit.maxs) * 0.5f;
        const glm::vec2 exitSize = apertureExtents(sd);
        float best = 1e9f;
        glm::vec3 shift{0.f};
        for(za::SizeT j = 0; j < sides.size(); j++)
        {
            const Side& o = sides[j];
            if(j == i || glm::dot(o.normal, exit.normal) < 0.999f) { continue; }
            const float out = glm::dot(o.normal, exitMid) - o.dist; // how far out of that gate's face it lands
            const glm::vec3 mid = (o.mins + o.maxs) * 0.5f;
            const glm::vec3 across = (mid - exitMid) + o.normal * out; // the miss along the face
            if(out < -1.f || out > 128.f || glm::any(glm::greaterThan(glm::abs(apertureExtents(o) - exitSize), glm::vec2{4.f})) ||
               glm::length(across) > 64.f || out >= best)
            {
                continue;
            }
            best = out;
            shift = mid - exitMid;
        }
        sd.paired = best < 1e9f;
        if(sd.paired && glm::length(shift) > 0.01f)
        {
            sd.to += shift;
            Con_DPrintf("VR portals: side %d comes out of its pair's face (moved %.1f %.1f %.1f)\n", static_cast<int>(i),
                shift.x, shift.y, shift.z);
        }
    }
}

// The map's gates: each trigger_teleport's teleport faces, by plane, and its destination (the server's entities).
void build()
{
    sides.clear();
    builtFor = sv.worldmodel;
    builtGeneration = worldGeneration();
    builtPairExits = vr_slipgate_pair_exits.value;
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
                sd->sourceTarget = e->v.target;
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
    pairExits();
    PR_PopQCVM(oldVm);
    Con_DPrintf("VR portals: %d slipgate sides\n", static_cast<int>(sides.size()));
}

[[nodiscard]] bool current()
{
    if(builtFor != sv.worldmodel || builtGeneration != worldGeneration() || builtPairExits != vr_slipgate_pair_exits.value)
    {
        return false;
    }
    if(vr_campaign.value >= 3.f && vr_campaign.value <= 5.f)
    {
        // Use the server VM directly: current() also runs from the client's render path.
        // Checking only cached gates avoids a full entity scan in every movement/trace call.
        for(const Side& side : sides)
        {
            if(side.trigger <= 0 || side.trigger >= sv.qcvm.num_edicts) { return false; }
            const auto* trigger = reinterpret_cast<const edict_t*>(
                reinterpret_cast<const byte*>(sv.qcvm.edicts) + side.trigger * sv.qcvm.edict_size);
            if(trigger->free || trigger->v.target != side.sourceTarget) { return false; }
        }
    }
    return true;
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

// This frame's view test (update() and vr_portals_view share it, so the reasons printed are the ones acted on):
// the head, the leaf it is in and its PVS, and where it looks. Invalid when the head is not in the world.
struct ViewTest
{
    const byte* vis = nullptr;
    glm::vec3 head{0.f}, forward{0.f};
    bool valid = false;
};

[[nodiscard]] ViewTest viewTest(const float* origin = nullptr, const float* viewAngles = nullptr)
{
    ViewTest vt;
    const hands::State& s = hands::current();
    if(!origin && !s.valid) { return vt; }
    const glm::vec3 head = origin ? vec(origin) : s.head;
    const glm::vec3 angle = viewAngles ? vec(viewAngles) : s.headAngles;
    vec3_t h{head.x, head.y, head.z};
    mleaf_t* leaf = Mod_PointInLeaf(h, cl.worldmodel);
    if(!leaf || leaf->contents == CONTENTS_SOLID)
    {
        return vt;
    }
    vec3_t angles{angle.x, angle.y, angle.z}, f, r, u;
    AngleVectors(angles, f, r, u);
    vt.vis = Mod_LeafPVS(leaf, cl.worldmodel);
    vt.head = head;
    vt.forward = vec(f);
    vt.valid = true;
    return vt;
}

// Why side i is not looked through this frame (nullptr: it is a candidate). `d` is the head's distance to its nearest
// point, `score` what it ranks by (its area over its square distance, held a while).
[[nodiscard]] const char* rejected(const ViewTest& vt, za::SizeT i, float& d, float& score)
{
    const Side& sd = sides[i];
    d = glm::distance(nearestPoint(sd, vt.head), vt.head);
    score = sd.area / (d * d + 4096.f) * (static_cast<int>(i) == lastChosen ? 1.5f : 1.f);
    if(glm::dot(sd.normal, vt.head) - sd.dist < 0.f) // (to the plane: the walk through it)
    {
        return "behind its plane";
    }
    if(!inPvs(vt.vis, sd.leaf))
    {
        return "not in the head's PVS";
    }
    if(d > kRange)
    {
        return "out of range";
    }
    const glm::vec3 toMiddle = (sd.mins + sd.maxs) * 0.5f - vt.head;
    const float len = glm::length(toMiddle);
    if(len > 64.f && glm::dot(vt.forward, toMiddle / len) < 0.2f)
    {
        return "not looked at";
    }
    if(behindGate(i, vt.head))
    {
        return "behind another gate";
    }
    return nullptr;
}

} // namespace

void update(const float* origin, const float* angles)
{
    chosen = -1;
    texture = 0;
    textureEye = -1;
    candidateViews = renderedViews = 0;
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
    if(sides.empty())
    {
        return;
    }
    chosenOrigin = origin ? vec(origin) : hands::current().head;
    chosenAngles = angles ? vec(angles) : hands::current().headAngles;
    const ViewTest vt = viewTest(origin, angles);
    if(!vt.valid)
    {
        return;
    }

    const int limit = viewLimit();
    for(za::SizeT i = 0; i < sides.size(); i++)
    {
        float d = 0.f, score = 0.f;
        if(rejected(vt, i, d, score)) { continue; }
        chosen = static_cast<int>(i);
        if(!wantedForView()) { continue; } // offscreen gates must not consume the view budget
        int at = candidateViews;
        if(at == limit)
        {
            at = limit - 1;
            if(score <= candidates[at].score) { continue; }
        }
        else { ++candidateViews; }
        while(at > 0 && score > candidates[at - 1].score)
        {
            candidates[at] = candidates[at - 1];
            --at;
        }
        candidates[at] = {static_cast<int>(i), score};
    }
    chosen = candidateViews ? candidates[0].side : -1;
    lastChosen = chosen;
}

int viewLimit() { return za::clamp(static_cast<int>(vr_portals_maxviews.value), 1, 8); }
int viewCount() { return candidateViews; }
int layer() { return renderedViews; }
void selectView(int view) { chosen = candidates[view].side; }

bool wantedForView()
{
    if(!enabled() || chosen < 0)
    {
        return false;
    }
    const Side& sd = sides[static_cast<za::SizeT>(chosen)];
    vec3_t f, r, u;
    AngleVectors(r_refdef.viewangles, f, r, u);
    float rt = za::tan(glm::radians(r_refdef.fov_x * 0.5f));
    float up = za::tan(glm::radians(r_refdef.fov_y * 0.5f));
    float l = -rt, dn = -up;
    if(stereo::isRenderingEye() && !stereo::isSpectator())
    {
        const Fov& fov = frameState().eyes[stereo::eye()].fov;
        l = za::tan(fov.left); rt = za::tan(fov.right); up = za::tan(fov.up); dn = za::tan(fov.down);
    }
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
        const glm::vec3 v = corner - vec(r_refdef.vieworg);
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
    if(renderedViews < 8) { renderedSides[renderedViews++] = chosen; }
}

bool viewing()
{
    return inView;
}

namespace
{
struct LightView
{
    glm::vec3 eye{0.f};
    za::Vector<byte> pvs;
};
za::SizeT heldBytes(const LightView& v) { return mem::heldBytes(v.pvs); }
struct LightViews
{
    za::Array<LightView, 9> views;
    auto members() { return mem::list(views); }
};
// Client frame, before light selection. PVS rows are owned: Mod_LeafPVS reuses its decompression buffer.
mem::Scratch<LightViews> lightViews{"portal light views"};
int lightViewCount = 0;
} // namespace

void prepareLightViews(const glm::vec3& eye)
{
    lightViewCount = 0;
    if(!cl.worldmodel) { return; }
    const int bytes = (cl.worldmodel->numleafs + 7) / 8;
    const auto add = [&](const glm::vec3& camera, const glm::vec3& leafPoint)
    {
        vec3_t p{leafPoint.x, leafPoint.y, leafPoint.z};
        mleaf_t* leaf = Mod_PointInLeaf(p, cl.worldmodel);
        LightView& view = lightViews.views[lightViewCount++];
        view.eye = camera;
        view.pvs.resize(bytes);
        const byte* vis = Mod_LeafPVS(leaf, cl.worldmodel);
        memcpy(view.pvs.data(), vis, bytes);
    };
    add(eye, eye);
    if(!enabled() || vr_portals.value <= 0.f || !sv.active) { return; }
    if(!current()) { build(); }
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    for(const Side& sd : sides)
    {
        if(lightViewCount >= 9) { break; }
        if(glm::dot(sd.normal, eye) < sd.dist || !inPvs(lightViews.views[0].pvs.data(), sd.leaf) ||
           glm::distance(eye, nearestPoint(sd, eye)) > kRange || !triggerActive(EDICT_NUM(sd.trigger)))
        {
            continue;
        }
        add(carriedView(sd, eye), sd.to + sd.turn * -sd.normal * 8.f);
    }
    PR_PopQCVM(oldVm);
}

float lightDistance(const glm::vec3& pos, int leaf)
{
    float best = 1e9f;
    for(int i = 0; i < lightViewCount; i++)
    {
        const LightView& view = lightViews.views[i];
        if(inPvs(view.pvs.data(), leaf)) { best = za::min(best, glm::distance(pos, view.eye)); }
    }
    return best;
}

int lightGates(const glm::vec3& light, float radius, LightGate* out, int capacity)
{
    if(!enabled() || vr_portals.value <= 0.f || !sv.active || !sv.worldmodel) { return 0; }
    if(!current()) { build(); }
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    int count = 0;
    for(const Side& sd : sides)
    {
        if(count == capacity) { break; }
        if(glm::dot(sd.normal, light) - sd.dist <= 1.f || glm::distance(light, nearestPoint(sd, light)) >= radius ||
           !triggerActive(EDICT_NUM(sd.trigger)))
        {
            continue;
        }
        out[count++] = {sd.from, sd.to, sd.normal, sd.mins, sd.maxs, sd.turn, sd.dist};
    }
    PR_PopQCVM(oldVm);
    return count;
}

// ----------------------------------------------------------------------------
// vr_portals_shot (Debug > Slipgates): the view through a gate read back from its own targets, to measure what it
// actually shows rather than which side was picked. Its colour and its depth, over the whole target and over the
// gate's box on the eye's screen. Mask/hidden-area depth is at the near plane; only depths strictly between clear
// and near values count as geometry, so the mask cannot masquerade as a rendered room.

namespace
{

bool shotPending = false;
int shotCount = 0;

struct Luma
{
    float mean = 0.f, max = 0.f; // luma 0..255
    int black = 0, count = 0;    // under 12
    int geometry = 0;           // excludes clear and the near-depth mask
    float depthMax = 0.f;       // includes the mask; not evidence of scene geometry by itself
};

// What `box` (the gate's box on the eye's screen, in NDC: x,y its lower corner, z,w its upper) of a read-back scene
// shows. `rgb` is RGBA floats as GL reads them, `depth` the same in its units.
[[nodiscard]] Luma measure(const za::Vector<float>& rgb, const za::Vector<float>& depth, int width, int height,
    const glm::vec4& box)
{
    Luma r;
    const int x0 = za::max(0, static_cast<int>((box.x + 1.f) * 0.5f * width));
    const int x1 = za::min(width, static_cast<int>((box.z + 1.f) * 0.5f * width));
    const int y0 = za::max(0, static_cast<int>((box.y + 1.f) * 0.5f * height));
    const int y1 = za::min(height, static_cast<int>((box.w + 1.f) * 0.5f * height));
    for(int y = y0; y < y1; y++)
    {
        for(int x = x0; x < x1; x++)
        {
            const za::SizeT i = static_cast<za::SizeT>(y) * width + x;
            const float l = (0.299f * rgb[i * 4] + 0.587f * rgb[i * 4 + 1] + 0.114f * rgb[i * 4 + 2]) * 255.f;
            r.mean += l;
            r.max = za::max(r.max, l);
            r.black += l < 12.f ? 1 : 0;
            r.count++;
            r.geometry += depth[i] > 0.f && depth[i] < 1.f ? 1 : 0;
            r.depthMax = za::max(r.depthMax, depth[i]);
        }
    }
    r.mean /= za::max(r.count, 1);
    return r;
}

void printLuma(const char* what, const Luma& r)
{
    Con_Printf("  %-9s %6d px  mean %6.1f max %6.1f black%% %5.1f geometry%% %5.1f depth max %.4f\n", what, r.count,
        r.mean, r.max, 100.f * r.black / za::max(r.count, 1), 100.f * r.geometry / za::max(r.count, 1), r.depthMax);
}

} // namespace

void requestShot()
{
    shotPending = true;
}

bool shotWanted()
{
    return shotPending;
}

void takeShot(unsigned sceneFbo, int width, int height)
{
    shotPending = false;
    if(!sceneFbo || width <= 0 || height <= 0)
    {
        Con_Printf("VR portal shot: no view through a gate to read.\n");
        return;
    }
    const za::SizeT n = static_cast<za::SizeT>(width) * height;
    za::Vector<float> rgb(n * 4), depth(n);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, sceneFbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, rgb.data());
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, 0);

    Con_Printf("VR portal shot %d: side %d eye %d, %dx%d, head (%.0f %.0f %.0f)\n", shotCount, chosen, textureEye,
        width, height, chosenOrigin.x, chosenOrigin.y, chosenOrigin.z);
    if(chosen >= 0)
    {
        const Side &s = sides[chosen];
        const glm::vec3 carried = carriedView(s, chosenOrigin);
        Con_Printf("  drawn from (%.0f %.0f %.0f), turned %.0f\n", carried.x, carried.y, carried.z,
            s.yaw);
    }
    Con_Printf("  its box: x %.3f..%.3f y %.3f..%.3f%s\n", eyeRect.x, eyeRect.z, eyeRect.y, eyeRect.w,
        eyeRectAll ? " (the whole screen)" : "");
    const glm::vec4 all{-1.f, -1.f, 1.f, 1.f};
    printLuma("whole", measure(rgb, depth, width, height, all));
    printLuma(eyeRectAll ? "whole box" : "gate box", measure(rgb, depth, width, height, eyeRect));

    za::Vector<byte> png(n * 3);
    for(za::SizeT i = 0; i < n; i++)
    {
        for(int c = 0; c < 3; c++)
        {
            png[i * 3 + c] = static_cast<byte>(za::clamp(rgb[i * 4 + c], 0.f, 1.f) * 255.f + 0.5f);
        }
    }
    const za::String dir = za::String{com_gamedir} + "/portalshots";
    files::createDirectories(dir.cStr());
    const za::String path = dir + "/" + va("%s_%03d.png", cl.mapname[0] ? cl.mapname : "none", shotCount);
    if(Image_WritePNGPath(path.cStr(), png.data(), width, height, 24, false))
    {
        Con_Printf("  Wrote %s\n", path.cStr());
    }
    ++shotCount;
}

} // namespace qvr::portals

using namespace qvr;

// R_RenderView, before R_SetupView: the view through the gate moved there (the eye's own, set up with its entities,
// carried through the gate: the same rigid mapping as movement and shots, see carriedView).
extern "C" void VR_PortalView(void)
{
    if(!portals::enabled() || !portals::inView || portals::chosen < 0)
    {
        return;
    }
    const portals::Side& sd = portals::sides[static_cast<za::SizeT>(portals::chosen)];
    const glm::vec3 o = portals::carriedView(sd, portals::vec(r_refdef.vieworg));
    for(int i = 0; i < 3; i++)
    {
        r_refdef.vieworg[i] = o[i];
    }
    r_refdef.viewangles[YAW] += sd.yaw;
}

// R_SetupView: in the view through the gate, the leaf it is seen from is the destination's, just beyond its point (the
// view itself is carried behind it: in a wall, or another room, whose PVS would leave the
// destination out, dark); else the view's own.
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

// R_MarkSurfaces: a view leaf with a liquid's or a gate's face in it takes the PVS round the view's origin (SV_FatPVS,
// for seeing through the surface). In the view through the gate that origin is the camera carried behind the
// destination (in a wall, or another room: its PVS empty or elsewhere, the world through the gate black: vrslipgates'
// loop, its turns and its platform gate, whose destinations' leaves touch a gate's face); there it is the destination's
// point, the one VR_PortalViewLeaf finds the leaf by.
extern "C" void VR_PortalPVSOrigin(float origin[3])
{
    if(!portals::enabled() || !portals::inView || portals::chosen < 0)
    {
        return;
    }
    const portals::Side& sd = portals::sides[static_cast<za::SizeT>(portals::chosen)];
    const glm::vec3 p = sd.to + sd.turn * -sd.normal * 8.f;
    vec3_t q{p.x, p.y, p.z};
    const mleaf_t* dest = Mod_PointInLeaf(q, cl.worldmodel);
    if(dest && dest->contents != CONTENTS_SOLID)
    {
        origin[0] = p.x;
        origin[1] = p.y;
        origin[2] = p.z;
    }
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
    // draw uploads synchronously; the next eye/portal may reuse this storage.
    auto& tris = portals::scratch.mask;
    tris.clear();
    tris.reserve(24); // four quads, six vertices each
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
extern "C" void VR_PortalFrameData(float plane[8][4], float mins[8][4], float maxs[8][4])
{
    memset(plane, 0, 8 * 4 * sizeof(float));
    memset(mins, 0, 8 * 4 * sizeof(float));
    memset(maxs, 0, 8 * 4 * sizeof(float));
    if(!portals::enabled() || portals::inView || !portals::texture) { return; }
    for(int layer = 0; layer < portals::renderedViews; ++layer)
    {
        const portals::Side& sd = portals::sides[portals::renderedSides[layer]];
        for(int i = 0; i < 3; i++)
        {
            plane[layer][i] = sd.normal[i];
            mins[layer][i] = sd.mins[i];
            maxs[layer][i] = sd.maxs[i];
        }
        plane[layer][3] = sd.dist;
        mins[layer][3] = za::clamp(vr_portals.value, 0.f, 1.f);
    }
}

// R_DrawBrushModels_Water: the view through the gate for this eye's teleport faces (unit 17, PortalScene; 0: none).
extern "C" unsigned VR_PortalTexture(void)
{
    const bool mine =
        portals::enabled() && !portals::inView && portals::renderedViews > 0;
    return mine ? portals::texture : 0u;
}

extern "C" int VR_PortalDrawing(void) { return portals::inView ? 1 : 0; }

extern "C" float VR_TeleportOpacity(void)
{
    return portals::enabled() ? za::clamp(vr_slipgate_surface_opacity.value, 0.f, 1.f) : 1.f;
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
    // Test only the original source visibility: adding destination rows must not
    // recursively make more entrances eligible. Rendering ranks gates per camera,
    // so a fixed first-eight server budget could omit entities in a rendered gate.
    auto& sourcePvs = portals::scratch.sourcePvs;
    sourcePvs.resize((sv.worldmodel->numleafs + 7) >> 3);
    memcpy(sourcePvs.data(), pvs, sourcePvs.size());
    auto& added = portals::scratch.added;
    added.clear();
    const auto add = [&added = added](const glm::vec3& p)
    {
        for(const glm::vec3& done : added)
        {
            if(done == p) { return; }
        }
        added.pushBack(p);
        vec3_t d{p.x, p.y, p.z};
        SV_AddToFatPVS(d, sv.worldmodel->nodes, sv.worldmodel);
    };
    // Whether `o` is within kStraddle of a side's plane, over its opening.
    const auto straddles = [&o](const portals::Side& sd)
    {
        const float d = glm::dot(sd.normal, o) - sd.dist;
        return za::fabs(d) < portals::kStraddle && portals::onGate(sd, o - sd.normal * d, portals::kStraddle);
    };
    for(const portals::Side& sd : portals::sides)
    {
        // A body straddling a gate: the eye may be drawn from the other room before or after the server carries the
        // body (see kStraddle), so both rooms are sent then.
        if(straddles(sd))
        {
            add(sd.dest);
        }
        else if(glm::dot(sd.normal, o) - sd.dist >= 0.f && portals::inPvs(sourcePvs.data(), sd.leaf) &&
            glm::distance(portals::nearestPoint(sd, o), o) <= portals::kRange)
        {
            add(sd.dest);
        }
        if(glm::distance(sd.to, o) < portals::kRange && straddles(portals::reverseSide(sd)))
        {
            const glm::vec3 c = (sd.mins + sd.maxs) * 0.5f;
            add(c - sd.normal * (glm::dot(sd.normal, c) - sd.dist) + sd.normal * 8.f); // just in front of the entry
        }
    }
}

// ----------------------------------------------------------------------------
// Walking and shooting through (vr_portals_walk): the server carries a player through a slipgate as the torso enters
// its plane, and what flies (missiles, grenades, gibs) as its path crosses it, by the side's own mapping: where it is,
// how it moves and where it looks are kept, turned and shifted as the view through the gate shows them, so the view
// does not jump. QuakeC's teleport_touch leaves players to it (portal_handles), including at a frame or sill;
// what the teleport does besides moving him is
// QuakeC's (VR_Portal_Crossed: the trigger's targets, what the hands carry). Monsters still teleport as in Quake.

namespace qvr::portals
{
namespace
{

constexpr float kCross = 24.f;    // how near the plane the body is watched for its crossing
constexpr float kAperture = 0.f;  // how far outside a gate's faces its aperture still counts as its opening
constexpr double kCooldown = 0.5; // seconds after a crossing before the next (no bouncing between two gates)

// Each client's last seamless crossing.
struct ClientState
{
    double crossed = -1e9;
    int generation = -1;
    int leavingSide = -1; // until the trailing half has cleared the destination plane
};
ClientState clients[MAX_SCOREBOARD];

ClientState& crossingState(edict_t* ent)
{
    ClientState& state = clients[NUM_FOR_EDICT(ent) - 1];
    if(state.generation != worldGeneration())
    {
        state = {};
        state.generation = worldGeneration();
    }
    return state;
}

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
    const bool official = vr_campaign.value >= 3.f && vr_campaign.value <= 5.f;
    const int ignoreTargetnameFlag = official ? 4 : 8;
    return !PR_GetString(trig->v.targetname)[0] ||
        (static_cast<int>(trig->v.spawnflags) & ignoreTargetnameFlag) || trig->v.nextthink >= qcvm->time;
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

[[nodiscard]] Side reverseSide(const Side& entry)
{
    Side exitGate;
    exitGate = entry;
    exitGate.from = entry.to;
    exitGate.to = entry.from;
    exitGate.normal = entry.turn * -entry.normal;
    exitGate.dist = glm::dot(exitGate.normal, entry.to);
    exitGate.turn = glm::transpose(entry.turn);
    exitGate.mins = glm::vec3{1e9f}; exitGate.maxs = glm::vec3{-1e9f};
    for(int c = 0; c < 8; c++)
    {
        const glm::vec3 p = carried(entry, glm::vec3{(c&1)?entry.maxs.x:entry.mins.x,
                (c&2)?entry.maxs.y:entry.mins.y, (c&4)?entry.maxs.z:entry.mins.z});
        exitGate.mins = glm::min(exitGate.mins, p); exitGate.maxs = glm::max(exitGate.maxs, p);
    }
    exitGate.yaw = -entry.yaw;
    return exitGate;
}

// (`margin` units of slack: a point on the plane lands a rounding step off a wall or floor gate's flat axis, where
// mins and maxs are equal; 1 for points from a mix along a crossing, as VR_PortalTrace's.)
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

// A recognised gate owns its player trigger even at its frame or below its sill. Waiting there must never bypass
// collision through QuakeC's old teleport. Triggers with no slipgate faces retain Quake's behaviour.
[[nodiscard]] bool engineCarries(int t)
{
    for(const Side& sd : sides)
    {
        if(sd.trigger == t)
        {
            return true;
        }
    }
    return false;
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

// The effective collision box's footprint on the gate plane must fit inside the opening. A centre inside it is not
// enough: the shoulders may still overlap the frame. Use the same narrowed box as SV_Move, including its hull height.
[[nodiscard]] bool bodyFitsGateAt(edict_t* ent, const Side& sd, const glm::vec3& origin)
{
    vec3_t mins, maxs;
    if(!VR_HullMoveBox(ent, ent->v.mins, ent->v.maxs, mins, maxs))
    {
        VectorCopy(ent->v.mins, mins);
        VectorCopy(ent->v.maxs, maxs);
    }
    for(int c = 0; c < 8; c++)
    {
        const glm::vec3 corner = origin + glm::vec3{(c & 1) ? maxs[0] : mins[0],
            (c & 2) ? maxs[1] : mins[1], (c & 4) ? maxs[2] : mins[2]};
        const glm::vec3 projected = corner - sd.normal * (glm::dot(sd.normal, corner) - sd.dist);
        if(!onGate(sd, projected, 0.03125f)) // the hull trace's 1/32-unit contact tolerance
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool bodyFitsGate(edict_t* ent, const Side& sd)
{
    return bodyFitsGateAt(ent, sd, vec(ent->v.origin));
}

void setVec(float* out, const glm::vec3& v)
{
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// Keep the exact transform; the split body trace checks exit clearance without a forward or sideways snap.
void crossPlayer(edict_t* ent, const Side& sd, edict_t* trig, ClientState& cs, int returnSide = -1)
{
    const glm::vec3 from = vec(ent->v.origin);
    glm::vec3 to = carried(sd, from);
    // Entry collision already tested both body halves. Keep the exact mapping and retain the split at the exit
    // until the trailing half clears it; a backing wall must not force a forward jump after the torso crosses.

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
    cs.leavingSide = returnSide >= 0 ? -1 : static_cast<int>(&sd - sides.data());
    Con_DPrintf("VR portal: carried edict %d through side %d: %.0f %.0f %.0f -> %.0f %.0f %.0f\n", NUM_FOR_EDICT(ent),
        returnSide >= 0 ? returnSide : cs.leavingSide, from.x, from.y, from.z, to.x, to.y, to.z);

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
        G_FLOAT(OFS_PARM1) = returnSide >= 0 ? 1.f : 0.f;
        PR_ExecuteProgram(fn);
        pr_global_struct->self = oldSelf;
        pr_global_struct->other = oldOther;
    }
}

} // namespace
} // namespace qvr::portals

// SV_Move: while straddling a gate, each half of the body collides in its own room. The normal, intact world trace
// remains the fallback outside a fully fitting aperture, and the gate's rim limits lateral motion during entry.
extern "C" int VR_PortalBodyMove(edict_t* ent, const float* start, const float* mins, const float* maxs,
                                 const float* end, int type, trace_t* trace)
{
    using namespace qvr::portals;
    if(!ent || !walkOn() || (type != MOVE_NORMAL && type != MOVE_NOMONSTERS) || NUM_FOR_EDICT(ent) < 1 ||
       NUM_FOR_EDICT(ent) > svs.maxclients || ent->v.health <= 0.f || vec(mins) != vec(ent->v.mins) ||
       vec(maxs) != vec(ent->v.maxs))
    {
        return 0;
    }
    if(!current()) { build(); }
    const glm::vec3 a = vec(start), b = vec(end);
    const glm::vec3 torso = a + glm::vec3{0.f, 0.f, 0.5f * (mins[2] + maxs[2])};
    const Side* gate = nullptr;
    Side exitGate;
    ClientState& state = crossingState(ent);
    if(state.leavingSide >= 0 && state.leavingSide < static_cast<int>(sides.size()))
    {
        const Side& entry = sides[state.leavingSide];
        exitGate = reverseSide(entry);
        vec3_t lo, hi;
        if(!VR_HullMoveBox(ent, mins, maxs, lo, hi))
        {
            VectorCopy(mins, lo);
            VectorCopy(maxs, hi);
        }
        bool clear = true;
        for(int c = 0; c < 8; c++)
        {
            const glm::vec3 p =
                a + glm::vec3{(c & 1) ? hi[0] : lo[0], (c & 2) ? hi[1] : lo[1], (c & 4) ? hi[2] : lo[2]};
            clear &= glm::dot(exitGate.normal, p) - exitGate.dist > 0.03125f;
        }
        if(clear || !bodyFitsGateAt(ent, exitGate, a)) { state.leavingSide = -1; }
        else { gate = &exitGate; }
    }
    float nearest = kCross;
    for(const Side& sd : sides)
    {
        if(gate == &exitGate) { break; }
        const float d = glm::dot(sd.normal, torso) - sd.dist;
        if(d < -kCross || za::fabs(d) >= nearest || !triggerActive(EDICT_NUM(sd.trigger)) ||
           !bodyFitsGateAt(ent, sd, a))
        {
            continue;
        }
        // Do not enter a sheet's opposite face while leaving it.
        if(d < 0.f && glm::dot(sd.normal, b - a) > 0.f) { continue; }
        nearest = za::fabs(d);
        gate = &sd;
    }
    if(!gate) { return 0; }
    const Side& sd = *gate;
    float limit = 1.f;
    if(!bodyFitsGateAt(ent, sd, b))
    {
        float lo = 0.f, hi = 1.f;
        for(int i = 0; i < 24; i++)
        {
            const float mid = (lo + hi) * 0.5f;
            if(bodyFitsGateAt(ent, sd, glm::mix(a, b, mid))) { lo = mid; }
            else { hi = mid; }
        }
        limit = lo;
    }
    const glm::vec3 stop = glm::mix(a, b, limit);
    vec3_t boxLo, boxHi;
    if(!VR_HullMoveBox(ent, mins, maxs, boxLo, boxHi))
    {
        VectorCopy(mins, boxLo);
        VectorCopy(maxs, boxHi);
    }
    vec3_t from{a.x, a.y, a.z}, to{stop.x, stop.y, stop.z};
    float sourcePlane[4]{sd.normal.x, sd.normal.y, sd.normal.z, sd.dist};
    trace_t source = SV_MovePortalHalf(from, boxLo, boxHi, to, type, ent, sourcePlane);
    const glm::vec3 destStart = carried(sd, a), destEnd = carried(sd, stop);
    glm::vec3 destLo{1e9f}, destHi{-1e9f};
    for(int c = 0; c < 8; c++)
    {
        const glm::vec3 offset = sd.turn * glm::vec3{(c & 1) ? boxHi[0] : boxLo[0], (c & 2) ? boxHi[1] : boxLo[1],
                                                     (c & 4) ? boxHi[2] : boxLo[2]};
        destLo = glm::min(destLo, offset);
        destHi = glm::max(destHi, offset);
    }
    const glm::vec3 ahead = sd.turn * -sd.normal;
    float destPlane[4]{ahead.x, ahead.y, ahead.z, glm::dot(ahead, sd.to)};
    vec3_t da{destStart.x, destStart.y, destStart.z}, db{destEnd.x, destEnd.y, destEnd.z};
    vec3_t dlo{destLo.x, destLo.y, destLo.z}, dhi{destHi.x, destHi.y, destHi.z};
    trace_t dest = SV_MovePortalHalf(da, dlo, dhi, db, type, ent, destPlane);
    const bool destHit = dest.fraction < source.fraction;
    *trace = destHit ? dest : source;
    if(destHit)
    {
        const glm::vec3 normal = glm::transpose(sd.turn) * vec(dest.plane.normal);
        setVec(trace->plane.normal, normal);
        trace->plane.dist = dest.plane.dist - glm::dot(vec(dest.plane.normal), sd.to) + glm::dot(normal, sd.from);
    }
    trace->startsolid = source.startsolid || dest.startsolid;
    trace->allsolid = source.allsolid || dest.allsolid;
    trace->fraction *= limit;
    if(limit < 1.f && trace->fraction >= limit && !trace->startsolid)
    {
        const glm::vec3 tangent = (b - a) - sd.normal * glm::dot(sd.normal, b - a);
        if(glm::length(tangent) > 1e-6f) { setVec(trace->plane.normal, -glm::normalize(tangent)); }
        trace->ent = qcvm->edicts;
    }
    setVec(trace->endpos, glm::mix(a, b, trace->fraction));
    return 1;
}

// VR_ClientSpecialMove (SV_Physics_Client, before the move): the player carried through a seamless slipgate (its
// trigger touched, active) once his torso reaches the plane and his collision box fits the aperture.
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
    ClientState& cs = crossingState(ent);
    if(cs.leavingSide >= 0 && cs.leavingSide < static_cast<int>(sides.size()))
    {
        const int index = cs.leavingSide;
        const Side exitGate = reverseSide(sides[index]);
        if(glm::dot(exitGate.normal, torsoOf(ent)) < exitGate.dist && bodyFitsGate(ent, exitGate))
        {
            crossPlayer(ent, exitGate, EDICT_NUM(exitGate.trigger), cs, index);
            return;
        }
    }
    if(qcvm->time - cs.crossed < kCooldown)
    {
        return;
    }
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
        // A thick two-sided sheet has another face behind this one. Being behind that opposite face is not entry:
        // the body must move into this face, rather than away from it or merely jump in front of the sheet.
        if(d > 0.f || za::fabs(d) >= bestD || glm::dot(sd.normal, vec(ent->v.velocity)) >= 0.f ||
           !onGate(sd, torso - sd.normal * d, kAperture) || !bodyFitsGate(ent, sd))
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
    // Collision must let the torso actually reach the plane. A frame or sill stopping the box is never permission
    // to cross; raised openings require a jump into them.
    crossPlayer(ent, *best, EDICT_NUM(best->trigger), cs);
}

// CL_RelinkEntities: an entity's last two places far apart (over 100 units on an axis: Quake's teleport, no lerp). The
// client draws a tick behind the server, lerping from the older place to the newer; snapping to the newer at a
// seamless crossing jumped the view a tick ahead and then held it still for a frame, a hitch at every slipgate. When
// the older place, near a gate's plane and over its opening (either way through), carried through it lands within a
// tick's move of the newer, `from` is it carried (and `yaw` the turn): the lerp goes on in the new room.
extern "C" int VR_PortalLerpFrom(const float older[3], const float newer[3], float from[3], float* yaw)
{
    using namespace qvr::portals;
    if(!walkOn() || !current())
    {
        return 0;
    }
    const glm::vec3 o = vec(older), n = vec(newer);
    float best = kStraddle * 2.f;
    for(const Side& entry : sides)
    {
        for(int back = 0; back < 2; back++)
        {
            const Side sd = back ? reverseSide(entry) : entry;
            const float d = glm::dot(sd.normal, o) - sd.dist;
            if(za::fabs(d) >= kStraddle || !onGate(sd, o - sd.normal * d, kStraddle))
            {
                continue;
            }
            const glm::vec3 c = carried(sd, o);
            const float miss = glm::distance(c, n);
            if(miss < best)
            {
                best = miss;
                setVec(from, c);
                *yaw = sd.yaw;
            }
        }
    }
    return best < kStraddle * 2.f ? 1 : 0;
}

namespace qvr::portals
{
glm::vec3 aiMap(int gate, const glm::vec3& value, bool direction)
{
    if(!walkOn() || !vr_portals_ai.value) { return value; }
    if(!current()) { build(); }
    if(gate <= 0 || gate > 2 * static_cast<int>(sides.size())) { return value; }
    const int count = static_cast<int>(sides.size());
    const Side sd = gate <= count ? sides[gate - 1] : reverseSide(sides[gate - count - 1]);
    if(!triggerActive(EDICT_NUM(sd.trigger))) { return value; }
    const Side inverse = reverseSide(sd);
    return direction ? inverse.turn * value : carried(inverse, value);
}

int aiImage(edict_t* observer, edict_t* target, const glm::vec3& from, const glm::vec3& point,
    int gate, glm::vec3& image)
{
    image = point;
    if(!walkOn() || !vr_portals_ai.value || !target || target->free ||
       !(static_cast<int>(target->v.flags) & FL_CLIENT) || target->v.health <= 0.f) { return 0; }
    if(!current()) { build(); }
    float best = 1e30f;
    int picked = 0;
    for(int i = 0; i < 2 * static_cast<int>(sides.size()); i++)
    {
        if(gate > 0 && gate != i + 1) { continue; }
        const int count = static_cast<int>(sides.size());
        const Side sd = i < count ? sides[i] : reverseSide(sides[i - count]);
        if(!triggerActive(EDICT_NUM(sd.trigger)) || (static_cast<int>(EDICT_NUM(sd.trigger)->v.spawnflags) & 1)) { continue; }
        const Side inverse = reverseSide(sd);
        const glm::vec3 candidate = carried(inverse, point);
        const float a = glm::dot(sd.normal, from) - sd.dist, b = glm::dot(sd.normal, candidate) - sd.dist;
        const float distance = glm::distance(from, candidate);
        if(a <= 0.f || b >= 0.f || distance >= best) { continue; }
        const glm::vec3 entry = glm::mix(from, candidate, a / (a - b));
        if(!onGate(sd, entry, 1.f)) { continue; }
        const glm::vec3 exit = carried(sd, entry);
        const glm::vec3 ray = point - exit;
        if(glm::length(ray) < 0.01f) { continue; }
        const glm::vec3 farStart = exit + glm::normalize(ray) * 0.05f;
        vec3_t start{from.x, from.y, from.z}, end{entry.x, entry.y, entry.z};
        // Ordinary traces in each room: they cannot accidentally recurse through another gate.
        const trace_t nearTrace = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NORMAL, observer);
        if(nearTrace.startsolid || nearTrace.fraction < 0.999f) { continue; }
        vec3_t farOrigin{farStart.x, farStart.y, farStart.z}, targetPoint{point.x, point.y, point.z};
        const trace_t farTrace = SV_Move(farOrigin, vec3_origin, vec3_origin, targetPoint, MOVE_NORMAL, observer);
        if(farTrace.startsolid || (farTrace.fraction < 0.999f && farTrace.ent != target)) { continue; }
        // *teleport shimmer reports water at the entry plane; it is not a water boundary.
        // Attack traces keep their ordinary contents checks in the reached room.
        image = candidate; best = distance; picked = i + 1;
    }
    return picked;
}

void pullSearchOrigins(const glm::vec3& from, za::Vector<glm::vec3>& out, float range)
{
    out.clear();
    out.pushBack(from);
    if(!walkOn()) { return; }
    if(!current()) { build(); }
    for(const Side& sd : sides)
    {
        if(!triggerActive(EDICT_NUM(sd.trigger)) || (static_cast<int>(EDICT_NUM(sd.trigger)->v.spawnflags) & 1) ||
           glm::dot(sd.normal, from) - sd.dist < 0.f) { continue; }
        // A gate whose aperture is out of reach takes nothing to the hand (pullImage's line from the hand crosses the
        // aperture within the range): its room is not searched (range < 0: every gate).
        if(range >= 0.f && glm::distance(nearestPoint(sd, from), from) > range + 1.f) { continue; }
        out.pushBack(carried(sd, from));
    }
}

glm::vec3 pullImage(const glm::vec3& from, const glm::vec3& point, int* gate)
{
    if(gate) { *gate = 0; }
    if(!walkOn()) { return point; }
    if(!current()) { build(); }
    glm::vec3 result = point;
    float best = 1e30f;
    for(int i = 0; i < static_cast<int>(sides.size()); ++i)
    {
        const Side& sd = sides[i];
        if(!triggerActive(EDICT_NUM(sd.trigger)) || (static_cast<int>(EDICT_NUM(sd.trigger)->v.spawnflags) & 1)) { continue; }
        // Only the inverse point transform is needed here, not reverseSide's eight-corner aperture bounds.
        const glm::vec3 image = glm::transpose(sd.turn) * (point - sd.to) + sd.from;
        if(vr_prop_query_verify.value && image != carried(reverseSide(sd), point))
        {
            Sys_Error("portal inverse point transform differs at gate %d", i);
        }
        const float d0 = glm::dot(sd.normal, from) - sd.dist, d1 = glm::dot(sd.normal, image) - sd.dist;
        const float distance = glm::distance(from, image);
        if(d0 < 0.f || d1 >= 0.f || distance >= best || distance > za::max(1.f, vr_forcegrab_distance.value) * 1.25f) { continue; }
        const glm::vec3 entry = glm::mix(from, image, d0 / (d0 - d1));
        if(!onGate(sd, entry, 1.f)) { continue; }
        const glm::vec3 exit = carried(sd, entry) + glm::normalize(point - carried(sd, entry)) * 0.5f;
        vec3_t a{from.x, from.y, from.z}, b{entry.x, entry.y, entry.z};
        const trace_t first = SV_Move(a, vec3_origin, vec3_origin, b, MOVE_NOMONSTERS, qcvm->edicts);
        if(first.startsolid || first.fraction < 0.999f) { continue; }
        vec3_t c{exit.x, exit.y, exit.z}, d{point.x, point.y, point.z};
        const trace_t second = SV_Move(c, vec3_origin, vec3_origin, d, MOVE_NOMONSTERS, qcvm->edicts);
        if(second.startsolid || second.fraction < 0.999f) { continue; }
        result = image;
        best = distance;
        if(gate) { *gate = i + 1; }
    }
    return result;
}
glm::vec3 pullImageSeen(const glm::vec3& from, const glm::vec3& point)
{
    if(!walkOn()) { return point; }
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    const glm::vec3 image = pullImage(from, point);
    PR_PopQCVM(oldVm);
    return image;
}
} // namespace qvr::portals

// A force grab homes in the destination room until it crosses the exit aperture,
// then maps its position, angles and velocities back to the player's room.
extern "C" void VR_PortalPullTarget(edict_t* ent, const float hand[3], int begin, float out[3])
{
    using namespace qvr::portals;
    const int field = ED_FindFieldOffset("fg_portal");
    setVec(out, vec(hand));
    if(field < 0) { return; }
    eval_t* state = GetEdictFieldValue(ent, field);
    if(begin)
    {
        int gate = 0;
        pullImage(vec(hand), physics::modelCentre(ent), &gate);
        state->_float = static_cast<float>(gate);
    }
    const int index = static_cast<int>(state->_float) - 1;
    if(index < 0) { return; }
    if(!walkOn() || index >= static_cast<int>(sides.size())) { state->_float = -1.f; return; }
    const Side& sd = sides[index];
    if(!triggerActive(EDICT_NUM(sd.trigger))) { state->_float = -1.f; return; }
    const Side exit = reverseSide(sd);
    const glm::vec3 middle = physics::modelCentre(ent);
    const float distance = glm::dot(exit.normal, middle) - exit.dist;
    if(distance <= 0.f)
    {
        // A moving hand can pull the trajectory outside the aperture. Drop
        // the object where it is rather than catching it through a wall.
        if(!onGate(exit, middle - exit.normal * distance, 2.f)) { state->_float = -1.f; return; }
        setVec(ent->v.origin, carried(exit, vec(ent->v.origin)));
        setVec(ent->v.oldorigin, vec(ent->v.origin));
        setVec(ent->v.velocity, exit.turn * vec(ent->v.velocity));
        ent->v.angles[YAW] = anglemod(ent->v.angles[YAW] + exit.yaw);
        state->_float = 0.f;
        SV_LinkEdict(ent, false);
        Con_DPrintf("force grab: crossed slipgate %d\n", index + 1);
        return;
    }
    setVec(out, carried(sd, vec(hand)));
}

namespace qvr::portals
{
static int aiRoute(edict_t* entity)
{
    if(!entity || !vr_portals_ai.value) { return 0; }
    const int offset = ED_FindFieldOffset("vr_ai_gate");
    return offset >= 0 ? static_cast<int>(GetEdictFieldValue(entity, offset)->_float) : 0;
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
    const bool debug = vr_portals_debug_split.value != 0.f;
    if(debug)
    {
        Con_Printf("toss %d: middle %.1f %.1f %.1f velocity %.1f %.1f %.1f\n", NUM_FOR_EDICT(ent), o.x, o.y, o.z, v.x, v.y, v.z);
    }
    if(sides.empty() || glm::dot(v, v) < 1.f)
    {
        return;
    }
    const glm::vec3 e = o + v * static_cast<float>(host_frametime);
    const int count = static_cast<int>(sides.size());
    const int route = aiRoute(ent);
    for(int i = -1; i < count; i++)
    {
        if(i < 0 && (route <= count || route > 2 * count)) { continue; }
        const Side sd = i < 0 ? reverseSide(sides[route - count - 1]) : sides[i];
        const float d0 = glm::dot(sd.normal, o) - sd.dist, d1 = glm::dot(sd.normal, e) - sd.dist;
        // Already past the plane, still straddling it and moving in (a frame it found no room in the destination, or
        // pushed in slowly by Box3D, whose copy at the destination lets it past the wall behind: else it went on
        // into that wall, uncarried): carried now.
        const float halfDepth = 0.5f * glm::dot(glm::abs(sd.normal), vec(ent->v.maxs) - vec(ent->v.mins));
        const bool behind = d0 < 0.f && d0 > -halfDepth && glm::dot(sd.normal, v) < 0.f;
        if(!behind && (d0 < 0.f || d1 >= 0.f))
        {
            continue;
        }
        const glm::vec3 c = behind ? o - sd.normal * d0 : o + (e - o) * (d0 / (d0 - d1));
        edict_t* trig = EDICT_NUM(sd.trigger);
        if(!onGate(sd, c, 2.f) || !triggerActive(trig) || (static_cast<int>(trig->v.spawnflags) & 1)) // PLAYER_ONLY
        {
            continue;
        }
        // Its middle's way to the plane clear (its box, wider than the gate's brush, would reach the wall behind it).
        vec3_t start{o.x, o.y, o.z};
        vec3_t end{c.x, c.y, c.z};
        const trace_t tr = behind ? trace_t{} : SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NOMONSTERS, ent);
        if(!behind && (tr.startsolid || tr.fraction < 1.f))
        {
            if(debug) { Con_Printf("toss %d: blocked on the way (%.2f)\n", NUM_FOR_EDICT(ent), tr.fraction); }
            continue; // it hits something on the way
        }
        // Its middle carried through (its box's offset kept: it is axis-aligned), a little further on where it does not fit.
        const glm::vec3 dir = sd.turn * v;
        const glm::vec3 to = carried(sd, vec(ent->v.origin));
        // Keep the straddling placement. Only its destination half needs to
        // clear the level; the trailing half belongs to the original room.
        const Side exit = reverseSide(sd);
        const float plane[4]{exit.normal.x, exit.normal.y, exit.normal.z, exit.dist};
        vec3_t at{to.x, to.y, to.z};
        // Its box turned with it (a gate turning 90 degrees swaps a long box's sides), 4 units in from each face: a prop
        // sliding on the floor rests on it (its Quake box a little round its model, under the floor), not a wall in the way.
        glm::vec3 lo{1e9f}, hi{-1e9f};
        for(int c = 0; c < 8; c++)
        {
            const glm::vec3 p = sd.turn * glm::vec3{(c & 1) ? ent->v.maxs[0] : ent->v.mins[0],
                (c & 2) ? ent->v.maxs[1] : ent->v.mins[1], (c & 4) ? ent->v.maxs[2] : ent->v.mins[2]};
            lo = glm::min(lo, p); hi = glm::max(hi, p);
        }
        const glm::vec3 inset = glm::min(glm::vec3{4.f}, (hi - lo) * 0.25f); // (a Quake box a little round its model)
        lo += inset; hi -= inset;
        vec3_t boxLo{lo.x, lo.y, lo.z}, boxHi{hi.x, hi.y, hi.z};
        const trace_t placement = SV_MovePortalHalf(at, boxLo, boxHi, at, MOVE_NOMONSTERS, ent, plane);
        if(placement.startsolid || placement.allsolid)
        {
            if(debug)
            {
                Con_Printf("toss %d: no room at %.1f %.1f %.1f (box %.1f %.1f %.1f .. %.1f %.1f %.1f, %s)\n", NUM_FOR_EDICT(ent),
                    to.x, to.y, to.z, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z, placement.allsolid ? "all solid" : "start solid");
            }
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
        const int gateField = ED_FindFieldOffset("vr_ai_gate");
        if(gateField >= 0) { GetEdictFieldValue(ent, gateField)->_float = 0.f; }
        SV_LinkEdict(ent, false);
        return;
    }
}

// QuakeC (portal_handles, teleport_touch): 1 if the engine carries this player through the trigger's slipgate
// (vr_portals_walk), so that Quake's teleport leaves him be until the body enters the opening.
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
    return engineCarries(NUM_FOR_EDICT(trig)) ? 1.f : 0.f;
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
        Con_Printf("      target \"%s\" cached destination (%.0f %.0f %.0f)\n", PR_GetString(trig->v.target),
            sd.dest.x, sd.dest.y, sd.dest.z);
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
            engineCarries(sd.trigger) ? "physical crossing only" : "Quake's teleport");
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

// vr_portals_view (Debug > Slipgates): why this frame looks through a gate or through none - every side of every gate
// in the map, and the rule that stops it (the same test update() acts on), plus what the last frame drew: the scene
// through a gate, or nothing (the gate's faces then show their own texture). For checking the view by hand, and for
// measuring how far away a gate is still looked through.
void viewInfo_f()
{
    if(!sv.worldmodel || !cl.worldmodel)
    {
        Con_Printf("VR portals: no world.\n");
        return;
    }
    if(!enabled())
    {
        Con_Printf("VR portals: off (vr_slipgates 0): nothing is looked through.\n");
        return;
    }
    if(!current())
    {
        build();
    }
    Con_Printf("VR portals: looking through side %d (%d sides, range %g, vr_portals %g)\n", chosen,
        static_cast<int>(sides.size()), kRange, vr_portals.value);
    Con_Printf("  last camera: %d gate views rendered (limit %d)\n", renderedViews, viewLimit());
    Con_Printf("  last frame: %s\n",
        texture ? "the scene through the gate, drawn for that camera" : "nothing drawn through a gate (its faces are dark)");
    if(chosen >= 0)
    {
        Con_Printf("  its box on that camera's screen: x %.3f..%.3f y %.3f..%.3f%s\n", eyeRect.x, eyeRect.z, eyeRect.y,
            eyeRect.w, eyeRectAll ? " (the whole screen)" : "");
        const Side &s = sides[chosen];
        Con_Printf("  its gate: plane through (%.0f %.0f %.0f) facing (%.2f %.2f %.2f), from (%.0f %.0f %.0f) to "
            "(%.0f %.0f %.0f), turned %.0f\n",
            s.from.x, s.from.y, s.from.z, s.normal.x, s.normal.y, s.normal.z, s.from.x, s.from.y, s.from.z, s.to.x,
            s.to.y, s.to.z, s.yaw);
    }
    const ViewTest vt = viewTest(&chosenOrigin.x, &chosenAngles.x);
    if(!vt.valid)
    {
        Con_Printf("  the head is not in the world: no gate is looked through.\n");
        return;
    }
    Con_Printf("  head (%.0f %.0f %.0f)\n", vt.head.x, vt.head.y, vt.head.z);
    if(chosen >= 0)
    {
        // Where the view through it is actually drawn from (VR_PortalView): the head carried through the gate.
        const Side &s = sides[chosen];
        const glm::vec3 carried = carriedView(s, vt.head);
        Con_Printf("  the view is drawn from (%.0f %.0f %.0f), turned %.0f\n", carried.x, carried.y, carried.z,
            s.yaw);
    }
    for(za::SizeT i = 0; i < sides.size(); i++)
    {
        float d = 0.f, score = 0.f;
        const char* why = rejected(vt, i, d, score);
        Con_Printf("  side %d: %-22s %6.0f units  score %.4g\n", static_cast<int>(i), why ? why : "looked through", d,
            score);
    }
}

void shot_f()
{
    requestShot();
    Con_Printf("VR portal shot: the next view through a gate will be read back.\n");
}

void pullTest_f()
{
    if(!sv.active || svs.maxclients < 1) { return; }
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    if(!current()) { build(); }
    edict_t* player = EDICT_NUM(1);
    const glm::vec3 origin = vec(player->v.origin);
    const Side* nearest = nullptr;
    float best = 1e30f;
    for(const Side& side : sides)
    {
        const float distance = glm::distance(origin, side.from);
        if(glm::dot(side.normal, origin) > side.dist && distance < best)
        { nearest = &side; best = distance; }
    }
    if(nearest)
    {
        const glm::vec3 point = carried(*nearest, nearest->from - nearest->normal * 64.f);
        const func_t fn = progs::findFunction("VR_Forcegrab_PortalTest");
        if(fn)
        {
            pr_global_struct->self = EDICT_TO_PROG(player);
            pr_global_struct->time = qcvm->time;
            setVec(G_VECTOR(OFS_PARM0), point);
            PR_ExecuteProgram(fn);
        }
    }
    PR_PopQCVM(oldVm);
}

void reachTest_f();
void rebuild_f() { if(sv.active) { build(); } }
void registerCommands()
{
    Cmd_AddCommand("vr_portals_rebuild", rebuild_f);
    Cmd_AddCommand("vr_portals_info", info_f);
    Cmd_AddCommand("vr_portals_view", viewInfo_f);
    Cmd_AddCommand("vr_portals_shot", shot_f);
    Cmd_AddCommand("vr_portals_pulltest", pullTest_f);
    Cmd_AddCommand("vr_portals_reachtest", reachTest_f);
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
        const int count = static_cast<int>(sides.size());
        const int route = aiRoute(passedict);
        Side reverse;
        for(int i = -1; i < count; i++)
        {
            if(i < 0 && (route <= count || route > 2 * count || n > 0)) { continue; }
            if(i < 0) { reverse = reverseSide(sides[route - count - 1]); }
            const Side& sd = i < 0 ? reverse : sides[i];
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

namespace qvr::portals
{
Reach reach(const glm::vec3& root, const glm::vec3& point)
{
    Reach out;
    out.position = point;
    if(!walkOn()) { return out; }
    if(!current()) { build(); }
    float first = 1.f;
    for(int i = 0; i < static_cast<int>(sides.size()); i++)
    {
        const Side& sd = sides[i];
        const float a = glm::dot(sd.normal, root) - sd.dist;
        const float b = glm::dot(sd.normal, point) - sd.dist;
        if(a < 0.f || b >= 0.f || a - b < 1e-5f) { continue; }
        const float t = a / (a - b);
        if(t >= first || !onGate(sd, glm::mix(root, point, t), 1.f) ||
           !triggerActive(EDICT_NUM(sd.trigger)) || (static_cast<int>(EDICT_NUM(sd.trigger)->v.spawnflags) & 1)) { continue; }
        const glm::vec3 on = glm::mix(root, point, t);
        vec3_t from{root.x, root.y, root.z}, to{on.x, on.y, on.z};
        const trace_t tr = SV_Move(from, vec3_origin, vec3_origin, to, MOVE_NOMONSTERS, qcvm->edicts);
        if(tr.startsolid || tr.fraction < 0.99f) { continue; }
        first = t;
        out.position = carried(sd, point);
        out.from = sd.from;
        out.to = sd.to;
        out.turn = sd.turn;
        out.yaw = sd.yaw;
        out.gate = i + 1;
    }
    return out;
}
}

namespace qvr::portals
{
namespace
{
int splitLogFrame = -1;
za::Vector<int> splitLogged; // the entities logged this frame (vr_portals_debug_split)

// vr_portals_debug_split: once a frame (the first view drawing it), an entity drawn cut by a gate, and the watched one
// (the main hand's held object, or an entity's number) also when not cut.
void logSplit(const entity_t* e, const Side* picked, const glm::vec3& centre, const glm::vec3 (&corners)[8])
{
    const float mode = vr_portals_debug_split.value;
    if(mode == 0.f || inView || e < cl_entities || e >= cl_entities + cl_max_edicts) { return; }
    const int num = static_cast<int>(e - cl_entities);
    const int watched = mode < 0.f ? cl.stats[protocol::STAT_QVR_CARRYMAIN] : static_cast<int>(mode);
    if(!picked && num != watched) { return; }
    if(splitLogFrame != host_framecount) { splitLogFrame = host_framecount; splitLogged.clear(); }
    for(const int done : splitLogged)
    {
        if(done == num) { return; }
    }
    splitLogged.pushBack(num);
    if(!picked)
    {
        Con_Printf("portal split: frame %d ent %d %s at %.1f %.1f %.1f whole middle %.1f %.1f %.1f\n", host_framecount, num,
            e->model->name, centre.x, centre.y, centre.z, centre.x, centre.y, centre.z);
        return;
    }
    float lo = 1e9f, hi = -1e9f;
    for(const glm::vec3& p : corners)
    {
        const float d = glm::dot(picked->normal, p) - picked->dist;
        lo = za::min(lo, d); hi = za::max(hi, d);
    }
    // Its middle in the room it is in (carried through when past the plane): what a test follows frame by frame.
    const float d = glm::dot(picked->normal, centre) - picked->dist;
    const glm::vec3 seen = d < 0.f ? carried(*picked, centre) : centre;
    Con_Printf("portal split: frame %d ent %d %s at %.1f %.1f %.1f cut by the plane %.2f %.2f %.2f %.1f (box %.1f..%.1f from it) middle %.1f %.1f %.1f\n",
        host_framecount, num, e->model->name, centre.x, centre.y, centre.z, picked->normal.x, picked->normal.y,
        picked->normal.z, picked->dist, lo, hi, seen.x, seen.y, seen.z);
}
} // namespace
} // namespace qvr::portals

namespace qvr::portals
{
namespace
{
// An exit with no gate of its own is only an aperture to what comes out of it: an entity drawn cut there must be moving
// out (its last two messages' places), not resting where the gate leads.
[[nodiscard]] bool movingOut(const entity_t* e, const Side& exit)
{
    if(e < cl_entities || e >= cl_entities + cl_max_edicts) { return false; }
    const glm::vec3 moved = vec(e->msg_origins[0]) - vec(e->msg_origins[1]);
    return glm::dot(exit.normal, moved) > 0.25f;
}
} // namespace
} // namespace qvr::portals

extern "C" int VR_PortalAlias(const entity_t* e, const float boundsMatrix[16], const float matrix[16],
    float mapped[16], float sourceClip[4], float destinationClip[4])
{
    using namespace qvr::portals;
    if(!e || !e->model || !walkOn()) { return 0; }
    if(!current()) { build(); }
    struct VmScope
    {
        qcvm_t* old = nullptr;
        VmScope() { PR_PushQCVM(&sv.qcvm, &old); }
        ~VmScope() { PR_PopQCVM(old); }
    } scope;
    const glm::mat4 bounds = glm::make_mat4(boundsMatrix);
    glm::vec3 corners[8], centre{0.f};
    for(int c = 0; c < 8; c++)
    {
        corners[c] = glm::vec3{bounds * glm::vec4{(c & 1) ? e->model->maxs[0] : e->model->mins[0],
            (c & 2) ? e->model->maxs[1] : e->model->mins[1], (c & 4) ? e->model->maxs[2] : e->model->mins[2], 1.f}};
        centre += corners[c] / 8.f;
    }
    // The player's own models (hands, guns) and what his hands hold (drawn where the tracked hands are, in his room's
    // coordinates): wholly past a gate's plane in front of him, they are drawn through it, not behind its surface.
    const bool own = qvr::view::find(e) != nullptr ||
        (e >= cl_entities && e < cl_entities + cl_max_edicts && held::drawnInHands(static_cast<int>(e - cl_entities)));
    // The body the hands reach from, where the hands are placed from (hands::State: the same frame's; the view entity's
    // lerped origin is already carried on the frame of a crossing while the hands are placed from the body before it:
    // a held object or a gun reaching through was drawn behind the gate's surface for that frame, a hitch).
    const hands::State& hs = hands::current();
    const glm::vec3 root = hs.valid ? hs.playerOrigin
        : cl.viewentity > 0 && cl.viewentity < cl.num_entities ? vec(cl_entities[cl.viewentity].origin) : hs.head;
    float nearest = 1e9f;
    Side picked;
    bool found = false;
    for(const Side& entry : sides)
    {
        if(!triggerActive(EDICT_NUM(entry.trigger)) || (static_cast<int>(EDICT_NUM(entry.trigger)->v.spawnflags) & 1)) { continue; }
        for(int reverse = 0; reverse < 2; reverse++)
        {
            const Side sd = reverse ? reverseSide(entry) : entry;
            if(reverse && !entry.paired && !own && !movingOut(e, sd)) { continue; }
            float lo = 1e9f, hi = -1e9f;
            for(const glm::vec3& p : corners)
            {
                const float d = glm::dot(sd.normal, p) - sd.dist;
                lo = za::min(lo, d); hi = za::max(hi, d);
            }
            const float d = glm::dot(sd.normal, centre) - sd.dist;
            const bool reaching = own && !reverse && glm::dot(sd.normal, root) >= sd.dist && glm::distance(root, centre) < 160.f;
            if(lo >= 0.f || (hi <= 0.f && !reaching) || za::fabs(d) >= nearest) { continue; }
            // Through the aperture: where the box meets the gate's plane (the middle of its edges' crossings: what passes
            // through, not the whole box's shadow on the plane, which a long gun held aslant, or the box of all its
            // frames, throws far past the aperture's edges); a model wholly through (a gun pushed in to the wrist): where
            // the line from the body to it crosses the plane.
            glm::vec3 meet{0.f};
            int crossings = 0;
            for(int c = 0; c < 8; c++)
            {
                for(int axis = 0; axis < 3; axis++)
                {
                    const int other = c | (1 << axis);
                    if(other == c) { continue; }
                    const float da = glm::dot(sd.normal, corners[c]) - sd.dist, db = glm::dot(sd.normal, corners[other]) - sd.dist;
                    if((da < 0.f) == (db < 0.f)) { continue; }
                    meet += glm::mix(corners[c], corners[other], da / (da - db));
                    crossings++;
                }
            }
            if(crossings > 0)
            {
                meet /= static_cast<float>(crossings);
            }
            else
            {
                const float a = glm::dot(sd.normal, root) - sd.dist, b = d;
                if(a - b < 1e-5f) { continue; }
                meet = glm::mix(root, centre, a / (a - b));
            }
            if(!onGate(sd, meet, 1.f)) { continue; }
            nearest = za::fabs(d); picked = sd; found = true;
        }
    }
    logSplit(e, found ? &picked : nullptr, centre, corners);
    if(!found) { return 0; }
    glm::mat4 transport{picked.turn};
    transport[3] = glm::vec4{picked.to - picked.turn * picked.from, 1.f};
    const glm::mat4 result = transport * glm::make_mat4(matrix);
    memcpy(mapped, glm::value_ptr(result), 16 * sizeof(float));
    const Side dest = reverseSide(picked);
    const glm::vec4 a{picked.normal, -picked.dist}, b{dest.normal, -dest.dist};
    memcpy(sourceClip, glm::value_ptr(a), 4 * sizeof(float));
    memcpy(destinationClip, glm::value_ptr(b), 4 * sizeof(float));
    return 1;
}

extern "C" void VR_PortalCarry(edict_t* box, edict_t* player, int hand, int begin)
{
    using namespace qvr::portals;
    const int field = ED_FindFieldOffset("carry_portal");
    const VrMove* move = server::clientMove(player);
    if(field < 0 || !move || hand < 0 || hand > 1) { return; }
    eval_t* saved = GetEdictFieldValue(box, field);
    const Reach mapped = reach(vec(player->v.origin), move->hands[hand].pos);
    const int previous = static_cast<int>(saved->_float);
    saved->_float = static_cast<float>(mapped.gate);
    if(begin || previous == mapped.gate) { return; }
    const auto carryBox = [&](const Side& sd) {
        setVec(box->v.origin, carried(sd, vec(box->v.origin)));
        setVec(box->v.oldorigin, vec(box->v.origin));
        setVec(box->v.velocity, sd.turn * vec(box->v.velocity));
        box->v.angles[YAW] = anglemod(box->v.angles[YAW] + sd.yaw);
    };
    if(previous > 0 && previous <= static_cast<int>(sides.size())) { carryBox(reverseSide(sides[previous - 1])); }
    if(mapped.gate > 0) { carryBox(sides[mapped.gate - 1]); }
    SV_LinkEdict(box, false);
    Con_DPrintf("portal reach: held object %d moved to gate %d\n", NUM_FOR_EDICT(box), mapped.gate);
}

// A reach is kept in tracking coordinates, but each part collides in its room.
extern "C" int VR_PortalReachMove(edict_t* player, const float* start, const float* mins, const float* maxs,
    const float* end, int type, trace_t* result)
{
    using namespace qvr::portals;
    if(!player || !walkOn()) { return 0; }
    if(!current()) { build(); }
    const glm::vec3 root = vec(player->v.origin), a = vec(start), b = vec(end);
    if(glm::distance(root, b) > 192.f) { return 0; }
    const Side* gate = nullptr;
    float nearest = 1e9f;
    for(const Side& sd : sides)
    {
        if(glm::dot(sd.normal, root) < sd.dist || !triggerActive(EDICT_NUM(sd.trigger)) ||
           (static_cast<int>(EDICT_NUM(sd.trigger)->v.spawnflags) & 1)) { continue; }
        bool beyond = false, fits = true;
        for(int corner = 0; corner < 8; corner++)
        {
            const glm::vec3 off{(corner & 1) ? maxs[0] : mins[0], (corner & 2) ? maxs[1] : mins[1], (corner & 4) ? maxs[2] : mins[2]};
            const glm::vec3 p = b + off;
            const float d = glm::dot(sd.normal, p) - sd.dist;
            beyond |= d < 0.f;
            fits &= onGate(sd, p - sd.normal * d, 0.f);
        }
        const float d = za::fabs(glm::dot(sd.normal, b) - sd.dist);
        if(beyond && fits && d < nearest) { gate = &sd; nearest = d; }
    }
    if(!gate) { return 0; }
    const Side& sd = *gate;
    const Side exit = reverseSide(sd);
    const float plane[4]{sd.normal.x, sd.normal.y, sd.normal.z, sd.dist};
    vec3_t sa, slo, shi, sb;
    VectorCopy(start, sa); VectorCopy(mins, slo); VectorCopy(maxs, shi); VectorCopy(end, sb);
    trace_t here = SV_MovePortalHalf(sa, slo, shi, sb, type, player, plane);
    const glm::vec3 da = carried(sd, a), db = carried(sd, b);
    glm::vec3 dlo{1e9f}, dhi{-1e9f};
    for(int c = 0; c < 8; c++)
    {
        const glm::vec3 p = sd.turn * glm::vec3{(c & 1) ? maxs[0] : mins[0], (c & 2) ? maxs[1] : mins[1], (c & 4) ? maxs[2] : mins[2]};
        dlo = glm::min(dlo, p); dhi = glm::max(dhi, p);
    }
    const float destPlane[4]{exit.normal.x, exit.normal.y, exit.normal.z, exit.dist};
    vec3_t from{da.x, da.y, da.z}, to{db.x, db.y, db.z};
    trace_t there = SV_MovePortalHalf(from, &dlo.x, &dhi.x, to, type, player, destPlane);
    const bool destHit = there.startsolid || (!here.startsolid && there.fraction < here.fraction);
    *result = destHit ? there : here;
    result->startsolid = here.startsolid || there.startsolid;
    result->allsolid = here.allsolid || there.allsolid;
    if(destHit)
    {
        const glm::vec3 n = glm::transpose(sd.turn) * vec(there.plane.normal);
        setVec(result->plane.normal, n);
        result->plane.dist = there.plane.dist - glm::dot(vec(there.plane.normal), sd.to) + glm::dot(n, sd.from);
    }
    setVec(result->endpos, glm::mix(a, b, result->fraction));
    return 1;
}

namespace qvr::portals
{
bool splitBounds(const glm::vec3& lo, const glm::vec3& hi, LightGate& gate, float margin, const glm::vec3* velocity)
{
    if(!walkOn()) { return false; }
    if(!current()) { build(); }
    // The gates' triggers are the server's edicts: read under its VM, whichever side asks (the client's held objects ask
    // as it reads the server's messages, VR_RelinkHeld > holdClear, with no VM or the client's active).
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    struct PopVm
    {
        qcvm_t* old;
        ~PopVm() { PR_PopQCVM(old); }
    } popVm{oldVm};
    float nearest = 1e9f;
    bool found = false;
    const glm::vec3 centre = (lo + hi) * 0.5f;
    for(const Side& entry : sides)
    {
        if(!triggerActive(EDICT_NUM(entry.trigger)) || (static_cast<int>(EDICT_NUM(entry.trigger)->v.spawnflags) & 1)) { continue; }
        for(int back = 0; back < 2; back++)
        {
            const Side sd = back ? reverseSide(entry) : entry;
            if(back && !entry.paired && (!velocity || glm::dot(sd.normal, *velocity) <= 1.f)) { continue; }
            float low = 1e9f, high = -1e9f;
            bool fits = true;
            for(int c = 0; c < 8; c++)
            {
                const glm::vec3 p{(c & 1) ? hi.x : lo.x, (c & 2) ? hi.y : lo.y, (c & 4) ? hi.z : lo.z};
                const float d = glm::dot(sd.normal, p) - sd.dist;
                low = za::min(low, d); high = za::max(high, d);
                fits &= onGate(sd, p - sd.normal * d, margin);
            }
            const float d = za::fabs(glm::dot(sd.normal, centre) - sd.dist);
            if(!fits || low >= 0.f || high <= 0.f || d >= nearest) { continue; }
            nearest = d; found = true;
            gate = {sd.from, sd.to, sd.normal, sd.mins, sd.maxs, sd.turn, sd.dist};
        }
    }
    return found;
}
}

namespace qvr::portals
{
Reach reachAlong(const glm::vec3& root, const glm::vec3& hand, const glm::vec3& point)
{
    Reach through = reach(root, hand);
    if(through.gate)
    {
        through.position = through.turn * (point - through.from) + through.to;
        return through;
    }
    through = reach(hand, point);
    return through.gate ? through : reach(root, point);
}

bool eyeThrough(const glm::vec3& body, glm::vec3& eye, glm::vec3& angles)
{
    if(!walkOn()) { return false; }
    if(!current()) { build(); }
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    struct PopVm
    {
        qcvm_t* old;
        ~PopVm() { PR_PopQCVM(old); }
    } popVm{oldVm};
    float first = 1.f;
    Side through{};
    bool found = false;
    for(const Side& entry : sides)
    {
        const edict_t* trig = EDICT_NUM(entry.trigger);
        if(!triggerActive(trig) || (static_cast<int>(trig->v.spawnflags) & 1)) { continue; }
        for(int back = 0; back < 2; back++)
        {
            // Forward: the eye in ahead of the body. Back (the exit's face): the body carried, the eye still behind.
            const Side sd = back ? reverseSide(entry) : entry;
            const float a = glm::dot(sd.normal, body) - sd.dist;
            const float b = glm::dot(sd.normal, eye) - sd.dist;
            if(a < 0.f || b >= 0.f || a - b < 1e-5f) { continue; }
            const float t = a / (a - b);
            if(t >= first || !onGate(sd, glm::mix(body, eye, t), 1.f)) { continue; }
            first = t;
            through = sd;
            found = true;
        }
    }
    if(!found) { return false; }
    eye = carried(through, eye);
    angles.y = anglemod(angles.y + through.yaw);
    return true;
}

void reachTest_f()
{
    if(!sv.active || svs.maxclients < 1) { return; }
    qcvm_t* old = nullptr;
    PR_PushQCVM(&sv.qcvm, &old);
    edict_t* player = EDICT_NUM(1);
    const VrMove* move = server::clientMove(player);
    if(move)
    {
        const glm::vec3 raw = move->hands[1].pos;
        const Reach mapped = reach(vec(player->v.origin), raw);
        const int offset = ED_FindFieldOffset("handpos");
        const glm::vec3 physical = offset >= 0 ? vec(GetEdictFieldValue(player, offset)->vector) : glm::vec3{0.f};
        vec3_t start{player->v.origin[0], player->v.origin[1], player->v.origin[2]}, end{raw.x, raw.y, raw.z};
        vec3_t lo{-1.f,-1.f,-1.f}, hi{1.f,1.f,1.f};
        trace_t tr{};
        const bool split = VR_PortalReachMove(player, start, lo, hi, end, MOVE_NOMONSTERS, &tr) != 0;
        Con_Printf("reachtest: gate=%d raw=(%.2f %.2f %.2f) mapped=(%.2f %.2f %.2f) error=%.4f splittrace=%d fraction=%.4f solid=%d\n",
            mapped.gate, raw.x, raw.y, raw.z, mapped.position.x, mapped.position.y, mapped.position.z,
            glm::distance(physical, mapped.position), split, split ? tr.fraction : 1.f, split && tr.startsolid);
        entity_t* own[3]{};
        view::woundTargets(own);
        entity_t* hand = own[2];
        if(hand && hand->model)
        {
            float matrix[16], bounds[16], mappedMatrix[16], source[4], dest[4];
            R_EntityMatrix(matrix, hand->origin, hand->angles, hand->scale);
            VR_AliasPreTransform(hand, matrix);
            memcpy(bounds, matrix, sizeof(bounds));
            const aliashdr_t* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(hand->model));
            ApplyTranslation(matrix, hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]);
            ApplyScale(matrix, hdr->scale[0], hdr->scale[1], hdr->scale[2]);
            VR_AliasPostTransform(hand, matrix);
            const int copies = VR_PortalAlias(hand, bounds, matrix, mappedMatrix, source, dest);
            Con_Printf("reachtest: hand_model=%s clipped_copies=%d bone_poses=%d\n", hand->model->name, copies ? 2 : 1, VR_AliasBonePoses(hand, nullptr));
        }
    }
    PR_PopQCVM(old);
}
}
