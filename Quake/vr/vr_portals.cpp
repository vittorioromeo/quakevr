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
#include "vr_trace.hpp"
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_stereo.hpp"
#include "vr_view.hpp"
#include "vr_geom.hpp"
#include "vr_portals_internal.hpp"

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
using namespace detail;

namespace detail
{

// The whole feature (vr_teleporters). With it off nothing here runs: no gate is ever built, none is looked through, no
// view is drawn through one, nothing is carried or traced through one, and Quake's trigger_teleport is what moves the
// player - exactly as before the feature existed. Every entry point below asks this first; update() forgets the gates
// built so far, so nothing else has anything to act on. Flipping it takes effect at once, no map reload.
[[nodiscard]] bool enabled()
{
    return vr_teleporters.value > 0.f;
}

[[nodiscard]] bool triggerActive(const edict_t* trig);


mem::Scratch<PortalScratch> scratch{"portal mask and PVS"};

za::Vector<Side> sides;
// reverseSide of each of `sides`, made once they are built (build): the alias and brush models' split tests ask for each
// gate's exit for every model in every view. In step with `sides` only when the sizes match (build clears it first).
za::Vector<Side> exits;
[[nodiscard]] Side reverseSide(const Side& entry);
[[nodiscard]] bool onGate(const Side& sd, const glm::vec3& onPlane, float margin);
const qmodel_t* builtFor = nullptr;
int builtGeneration = -1;
float builtPairExits = -1.f; // vr_teleporter_pair_exits as built (pairExits)
int chosen = -1;  // the side being ranked or picked for a view (-1: none)
int lastChosen = -1;

za::Array<Level, kMaxDepth + 1> levels; // by the depth of the camera seeing them (the deepest sees none)
// The view of each depth being made (1..kMaxDepth; 0 unused): the side it looks through and its box on the eye's
// screen (NDC, with a margin; or the whole screen: the gate's box reaches behind the eye), within its parent's.
za::Array<int, kMaxDepth + 1> path{};
za::Array<glm::vec4, kMaxDepth + 1> rects{};
za::Array<bool, kMaxDepth + 1> rectAll{};
int rankDepth = 0; // the depth of the camera whose views update(), selectView() and wantedForView() rank and pick
int nestedViews = 0; // views within views drawn for this camera (vr_portals_recursion_max)
int drawDepth = 0; // the depth of the camera being drawn (0: no view through a gate)

glm::vec3 chosenOrigin{0.f}, chosenAngles{0.f}; // last rendered camera, for flat/eye/spectator diagnostics

glm::vec4 eyeRect{-1.f, -1.f, 1.f, 1.f}; // the box wantedForView found last (see rects)
bool eyeRectAll = true;

// The side the camera being drawn looks through (-1: none, the eye's own).
[[nodiscard]] int drawnSide()
{
    return drawDepth > 0 ? path[drawDepth] : -1;
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
// vrteleporters' and most custom maps' are: the destination marker stands clear of that gate's trigger, 48 units out, for
// Quake's teleport of monsters): the crossing comes out of that gate's face, not out of the open air in front of it. Else
// the player popped out 48 units past the gate he seemed to walk out of, the view through showed the room from there,
// and the exit plane (reverseSide) stood in the middle of the room: a prop or a held object straddling it there was cut
// and drawn again back at the entrance, and Box3D dropped its contacts behind that plane (ROUND21.md, "Teleporters: exits
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
    if(vr_teleporter_pair_exits.value <= 0.f) { return; }
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
               glm::length(across) > 64.f || glm::length(across) + 0.01f * out >= best)
            {
                continue;
            }
            best = glm::length(across) + 0.01f * out; // (the gate it lands on the middle of: a row of gates of one size)
            shift = mid - exitMid;
            sd.pair = static_cast<int>(j);
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
    exits.clear();
    builtFor = sv.worldmodel;
    builtGeneration = worldGeneration();
    builtPairExits = vr_teleporter_pair_exits.value;
    qmodel_t* m = sv.worldmodel;
    if(!m)
    {
        return;
    }
    progs::ServerVm serverVm;
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
        // A destination that moves (end's misc_teleporttrain, the spiked ball one rides into Shub-Niggurath to telefrag
        // her) is no gate: its point is the train's at build time (the map's load, or a save's), so the walk carried the
        // player there and the view showed it, and a ball inside Shub left no room to walk out. Quake's teleport takes
        // him, reading where the train is as he touches the trigger.
        if(!d || d->v.movetype != static_cast<float>(MOVETYPE_NONE) || d->v.velocity[0] != 0.f || d->v.velocity[1] != 0.f ||
            d->v.velocity[2] != 0.f)
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
    exits.reserve(sides.size());
    for(const Side& sd : sides)
    {
        exits.pushBack(reverseSide(sd)); // (each made in full: `exits` still shorter than `sides`)
    }
    Con_DPrintf("VR portals: %d teleporter sides\n", static_cast<int>(sides.size()));
}

[[nodiscard]] bool current()
{
    if(builtFor != sv.worldmodel || builtGeneration != worldGeneration() || builtPairExits != vr_teleporter_pair_exits.value)
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
// from that side's front): the gate in front is the one looked through (a teleporter behind a teleporter, as in start).
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


// The leaf a view through side `sd` is seen from: the destination's, just beyond its point (VR_PortalViewLeaf).
[[nodiscard]] mleaf_t* destinationLeaf(const Side& sd)
{
    const glm::vec3 p = sd.to + sd.turn * -sd.normal * 8.f;
    vec3_t q{p.x, p.y, p.z};
    mleaf_t* dest = Mod_PointInLeaf(q, cl.worldmodel);
    return dest && dest->contents != CONTENTS_SOLID ? dest : nullptr;
}

// `through`: a camera in a view through that side (carried behind its exit: often in a wall), seen from its
// destination's leaf.
[[nodiscard]] ViewTest viewTest(const float* origin = nullptr, const float* viewAngles = nullptr, int through = -1)
{
    ViewTest vt;
    const hands::State& s = hands::current();
    if(!origin && !s.valid) { return vt; }
    const glm::vec3 head = origin ? vec(origin) : s.head;
    const glm::vec3 angle = viewAngles ? vec(viewAngles) : s.headAngles;
    vec3_t h{head.x, head.y, head.z};
    mleaf_t* leaf = through >= 0 ? destinationLeaf(sides[static_cast<za::SizeT>(through)]) :
        Mod_PointInLeaf(h, cl.worldmodel);
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

// Whether side i shows in a view through side `through`: some of it beyond that view's near plane (the exit's
// plane, VR_PortalClip), where it is drawn.
[[nodiscard]] bool beyondExit(int through, za::SizeT i)
{
    const Side& p = sides[static_cast<za::SizeT>(through)];
    const glm::vec3 n = p.turn * p.normal;
    const float plane = glm::dot(n, p.to) - 1.f;
    const Side& sd = sides[i];
    for(int c = 0; c < 8; c++)
    {
        const glm::vec3 corner{(c & 1) ? sd.maxs.x : sd.mins.x, (c & 2) ? sd.maxs.y : sd.mins.y,
            (c & 4) ? sd.maxs.z : sd.mins.z};
        if(glm::dot(n, corner) < plane) { return true; }
    }
    return false;
}

} // namespace detail

int maxDepth()
{
    return enabled() ? 1 + za::clamp(static_cast<int>(vr_portals_recursion.value), 0, kMaxDepth - 1) : 1;
}

void update(const float* origin, const float* angles, int depth)
{
    depth = za::clamp(depth, 0, kMaxDepth);
    rankDepth = depth;
    chosen = -1;
    if(depth == 0)
    {
        for(Level& lv : levels) { lv.candidateViews = lv.renderedViews = 0; lv.texture = 0; lv.textureEye = -1; }
        drawDepth = 0;
        nestedViews = 0;
    }
    Level& level = levels[depth];
    level.candidateViews = level.renderedViews = 0;
    level.texture = 0;
    level.textureEye = -1;
    if(depth >= maxDepth()) { return; }
    if(!enabled())
    {
        sides.clear(); // (the feature off: the gates are forgotten, and stay so until it is turned on again)
        exits.clear();
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
    if(depth == 0)
    {
        chosenOrigin = origin ? vec(origin) : hands::current().head;
        chosenAngles = angles ? vec(angles) : hands::current().headAngles;
    }
    const int through = depth > 0 ? path[depth] : -1;
    if(depth > 0 && through < 0) { return; }
    const ViewTest vt = viewTest(origin, angles, through);
    if(!vt.valid)
    {
        return;
    }

    const int limit = viewLimit(depth);
    auto& candidates = level.candidates;
    for(za::SizeT i = 0; i < sides.size(); i++)
    {
        float d = 0.f, score = 0.f;
        if(rejected(vt, i, d, score) || (through >= 0 && !beyondExit(through, i))) { continue; }
        chosen = static_cast<int>(i);
        if(!wantedForView()) { continue; } // offscreen gates must not consume the view budget
        int at = level.candidateViews;
        if(at == limit)
        {
            at = limit - 1;
            if(score <= candidates[at].score) { continue; }
        }
        else { ++level.candidateViews; }
        while(at > 0 && score > candidates[at - 1].score)
        {
            candidates[at] = candidates[at - 1];
            --at;
        }
        candidates[at] = {static_cast<int>(i), score};
    }
    chosen = level.candidateViews ? candidates[0].side : -1;
    if(depth == 0) { lastChosen = chosen; }
}

// The eye's camera: vr_portals_maxviews; a view through a gate: vr_portals_recursion_views; deeper, the best one alone
// (each gate deeper a single line of views: at most vr_portals_recursion_views times vr_portals_recursion views
// within the views of an eye's gates, however many gates they see).
int viewLimit(int depth)
{
    const float limit = depth == 0 ? vr_portals_maxviews.value : depth == 1 ? vr_portals_recursion_views.value : 1.f;
    return za::clamp(static_cast<int>(limit), 1, 8);
}
int viewCount(int depth) { return levels[za::clamp(depth, 0, kMaxDepth)].candidateViews; }
int layer(int depth) { return levels[za::clamp(depth - 1, 0, kMaxDepth)].renderedViews; }
void selectView(int view, int depth)
{
    rankDepth = za::clamp(depth, 0, kMaxDepth - 1);
    chosen = levels[rankDepth].candidates[view].side;
    path[rankDepth + 1] = chosen;
    for(int d = rankDepth + 1; d <= kMaxDepth; d++) // what was drawn for another view of this depth is not its
    {
        levels[d].candidateViews = levels[d].renderedViews = 0;
        levels[d].texture = 0;
    }
}

int scaleAt(int depth)
{
    const int step = za::clamp(static_cast<int>(vr_portals_recursion_scale.value), 1, 4);
    int scale = 1;
    for(int d = 2; d <= depth; d++) { scale *= step; }
    return scale;
}

void carryCamera(int depth)
{
    const int side = depth > 0 && depth <= kMaxDepth ? path[depth] : -1;
    if(!enabled() || side < 0) { return; }
    const Side& sd = sides[static_cast<za::SizeT>(side)];
    const glm::vec3 o = carriedView(sd, vec(r_refdef.vieworg));
    for(int i = 0; i < 3; i++) { r_refdef.vieworg[i] = o[i]; }
    r_refdef.viewangles[YAW] += sd.yaw;
}

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
    // Within a view, only through that view's own gate (its box on the screen).
    if(rankDepth > 0 && !rectAll[rankDepth])
    {
        const glm::vec4 parent = rects[rankDepth];
        if(eyeRectAll)
        {
            eyeRect = parent;
            eyeRectAll = false;
        }
        eyeRect = glm::vec4{glm::max(glm::vec2{eyeRect}, glm::vec2{parent}),
            glm::min(glm::vec2{eyeRect.z, eyeRect.w}, glm::vec2{parent.z, parent.w})};
        if(eyeRect.x >= eyeRect.z || eyeRect.y >= eyeRect.w)
        {
            return false; // not seen through the gate the view is in
        }
    }
    rects[rankDepth + 1] = eyeRect;
    rectAll[rankDepth + 1] = eyeRectAll;
    return true;
}

void beginView(int depth)
{
    drawDepth = za::clamp(depth, 0, kMaxDepth);
    if(drawDepth >= maxDepth()) // the deepest views show no views of their own
    {
        levels[drawDepth].renderedViews = 0;
        levels[drawDepth].texture = 0;
    }
}

bool moreViews(int depth)
{
    return depth < 1 || nestedViews < za::clamp(static_cast<int>(vr_portals_recursion_max.value), 0, 64);
}

void endView(unsigned tex, int depth)
{
    depth = za::clamp(depth, 1, kMaxDepth);
    nestedViews += depth > 1 ? 1 : 0;
    drawDepth = depth - 1;
    Level& parent = levels[depth - 1];
    parent.texture = tex;
    parent.textureEye = stereo::eye();
    if(parent.renderedViews < 8) { parent.renderedSides[parent.renderedViews++] = path[depth]; }
}

bool viewing()
{
    return drawDepth > 0;
}

int viewDepth()
{
    return drawDepth;
}

namespace
{
constexpr int kLightViews = 16; // the eye's own and the views through gates whose torches light (prepareLightViews)
struct LightView
{
    glm::vec3 eye{0.f};
    za::Vector<byte> pvs;
    int side{-1};  // the gate it looks out of (-1: the eye's own)
    int depth{0};  // how many gates deep (0: the eye's own)
    glm::vec3 into{0.f}; // through a gate: its exit's plane, facing into the room it looks into (what it sees is
    float intoDist{0.f}; // in front: an unvised map's PVS has every room, and a carried eye can be near any of them)
};
za::SizeT heldBytes(const LightView& v) { return mem::heldBytes(v.pvs); }
struct LightViews
{
    za::Array<LightView, kLightViews> views;
    auto members() { return mem::list(views); }
};
// Client frame, before light selection. PVS rows are owned: Mod_LeafPVS reuses its decompression buffer.
mem::Scratch<LightViews> lightViews{"portal light views"};
int lightViewCount = 0;
} // namespace

// The eye's PVS, then the view through each gate in it (in front of the gate, near: kRange), and within those views
// the gates they see, a gate deeper each round, as many as the views drawn (vr_portals_recursion: maxDepth): a torch in
// a room two gates away lights what the view within a view shows, as its lightmaps already do. Each view's eye is
// carried through its gates (its distance to a torch is the way through them).
void prepareLightViews(const glm::vec3& eye)
{
    lightViewCount = 0;
    if(!cl.worldmodel) { return; }
    const int bytes = (cl.worldmodel->numleafs + 7) / 8;
    const auto add = [&](const glm::vec3& camera, const glm::vec3& leafPoint, int side, int depth)
    {
        vec3_t p{leafPoint.x, leafPoint.y, leafPoint.z};
        mleaf_t* leaf = Mod_PointInLeaf(p, cl.worldmodel);
        LightView& view = lightViews.views[lightViewCount++];
        view.eye = camera;
        view.side = side;
        view.depth = depth;
        if(side >= 0)
        {
            const Side& sd = sides[static_cast<za::SizeT>(side)];
            view.into = sd.turn * -sd.normal;
            view.intoDist = glm::dot(view.into, sd.to) - 1.f;
        }
        view.pvs.resize(bytes);
        const byte* vis = Mod_LeafPVS(leaf, cl.worldmodel);
        memcpy(view.pvs.data(), vis, bytes);
    };
    add(eye, eye, -1, 0);
    if(!enabled() || vr_portals.value <= 0.f || !sv.active) { return; }
    if(!current()) { build(); }
    progs::ServerVm serverVm;
    const int count = static_cast<int>(sides.size());
    int first = 0, last = 1; // this round's views: those added by the last
    for(int depth = 1; depth <= maxDepth() && first < last; depth++)
    {
        for(int v = first; v < last; v++)
        {
            const glm::vec3 from = lightViews.views[v].eye;
            const int out = lightViews.views[v].side;
            for(int s = 0; s < count && lightViewCount < kLightViews; s++)
            {
                const Side& sd = sides[static_cast<za::SizeT>(s)];
                if(glm::dot(sd.normal, from) < sd.dist || !inPvs(lightViews.views[v].pvs.data(), sd.leaf) ||
                   glm::distance(from, nearestPoint(sd, from)) > kRange || !triggerActive(EDICT_NUM(sd.trigger)))
                {
                    continue;
                }
                if(out >= 0)
                {
                    // Beyond the exit it looks out of (as the rooms sent to a client: VR_PortalAddPVS), not the gate
                    // it came out of nor one beside it.
                    const Side& exit = sides[static_cast<za::SizeT>(out)];
                    const glm::vec3 n = exit.turn * exit.normal;
                    bool beyond = false;
                    for(int c = 0; c < 8 && !beyond; c++)
                    {
                        const glm::vec3 corner{(c & 1) ? sd.maxs.x : sd.mins.x, (c & 2) ? sd.maxs.y : sd.mins.y,
                            (c & 4) ? sd.maxs.z : sd.mins.z};
                        beyond = glm::dot(n, corner) < glm::dot(n, exit.to) - 1.f;
                    }
                    if(!beyond) { continue; }
                }
                add(carriedView(sd, from), sd.to + sd.turn * -sd.normal * 8.f, s, depth);
            }
        }
        first = last;
        last = lightViewCount;
    }
}

float lightDistance(const glm::vec3& pos, int leaf)
{
    float best = 1e9f;
    for(int i = 0; i < lightViewCount; i++)
    {
        const LightView& view = lightViews.views[i];
        if(inPvs(view.pvs.data(), leaf) && (view.side < 0 || glm::dot(view.into, pos) >= view.intoDist))
        {
            best = za::min(best, glm::distance(pos, view.eye));
        }
    }
    return best;
}

int lightGates(const glm::vec3& light, float radius, LightGate* out, int capacity)
{
    if(!enabled() || vr_portals.value <= 0.f || !sv.active || !sv.worldmodel) { return 0; }
    if(!current()) { build(); }
    progs::ServerVm serverVm;
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
    return count;
}

// ----------------------------------------------------------------------------
// vr_portals_shot (Debug > Teleporters): the view through a gate read back from its own targets, to measure what it
// actually shows rather than which side was picked. Its colour and its depth, over the whole target and over the
// gate's box on the eye's screen. Mask/hidden-area depth is at the near plane; only depths strictly between clear
// and near values count as geometry, so the mask cannot masquerade as a rendered room. Its float scene goes to a .pfm
// beside the .png (as vr_eyeshot 2's), to compare with the eye's own float scene; and with the eye's in the same frame
// (the gate's face over the same pixels: what it shows of the view).

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

    // The view just drawn (endView has stepped back to its parent): its side, and the camera it was drawn from (the
    // caller puts r_refdef back after this).
    const int side = path[za::min(drawDepth + 1, kMaxDepth)];
    Con_Printf("VR portal shot %d: side %d eye %d, %dx%d, eye (%.2f %.2f %.2f)\n", shotCount, side, stereo::eye(), width,
        height, chosenOrigin.x, chosenOrigin.y, chosenOrigin.z);
    Con_Printf("  drawn from (%.2f %.2f %.2f), angles (%.2f %.2f %.2f)\n", r_refdef.vieworg[0], r_refdef.vieworg[1],
        r_refdef.vieworg[2], r_refdef.viewangles[0], r_refdef.viewangles[1], r_refdef.viewangles[2]);
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
    // Its float scene too (as vr_eyeshot 2's .pfm: bottom row first), to compare with the eye's over the same pixels.
    const za::String pfm = dir + "/" + va("%s_%03d.pfm", cl.mapname[0] ? cl.mapname : "none", shotCount);
    if(FILE* f = fopen(pfm.cStr(), "wb"))
    {
        fprintf(f, "PF\n%d %d\n-1.0\n", width, height);
        for(za::SizeT i = 0; i < n; i++)
        {
            fwrite(&rgb[i * 4], sizeof(float), 3, f);
        }
        fclose(f);
    }
    ++shotCount;
}

} // namespace qvr::portals

using namespace qvr;

// R_RenderView, before R_SetupView: the view through the gate moved there (the eye's own, set up with its entities,
// carried through the gate: the same rigid mapping as movement and shots, see carriedView).
extern "C" void VR_PortalView(void)
{
    if(!portals::enabled() || portals::drawnSide() < 0)
    {
        return;
    }
    const portals::Side& sd = portals::sides[static_cast<za::SizeT>(portals::drawnSide())];
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
    if(!portals::enabled() || portals::drawnSide() < 0)
    {
        return leaf;
    }
    const portals::Side& sd = portals::sides[static_cast<za::SizeT>(portals::drawnSide())];
    const glm::vec3 p = sd.to + sd.turn * -sd.normal * 8.f;
    vec3_t q{p.x, p.y, p.z};
    mleaf_t* dest = Mod_PointInLeaf(q, cl.worldmodel);
    return dest && dest->contents != CONTENTS_SOLID ? dest : leaf;
}

// R_MarkSurfaces: a view leaf with a liquid's or a gate's face in it takes the PVS round the view's origin (SV_FatPVS,
// for seeing through the surface). In the view through the gate that origin is the camera carried behind the
// destination (in a wall, or another room: its PVS empty or elsewhere, the world through the gate black: vrteleporters'
// loop, its turns and its platform gate, whose destinations' leaves touch a gate's face); there it is the destination's
// point, the one VR_PortalViewLeaf finds the leaf by.
extern "C" void VR_PortalPVSOrigin(float origin[3])
{
    if(!portals::enabled() || portals::drawnSide() < 0)
    {
        return;
    }
    const portals::Side& sd = portals::sides[static_cast<za::SizeT>(portals::drawnSide())];
    const glm::vec3 p = sd.to + sd.turn * -sd.normal * 8.f;
    vec3_t q{p.x, p.y, p.z};
    const mleaf_t* dest = Mod_PointInLeaf(q, cl.worldmodel);
    if(dest && dest->contents != CONTENTS_SOLID)
    {
        geom::store(p, origin);
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
    if(!portals::enabled() || portals::drawnSide() < 0 || !gl_clipcontrol_able)
    {
        return;
    }
    const portals::Side& sd = portals::sides[static_cast<za::SizeT>(portals::drawnSide())];
    const glm::vec3 n = sd.turn * sd.normal;
    // (Half a unit beyond the exit's plane: the exit gate's own teleport faces lie in it, and their copy facing back (a
    // liquid's faces are made both ways) would cover the view, once views within views draw teleport faces.)
    const glm::vec4 world{-n, glm::dot(n, sd.to) - 0.5f};
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
    if(!portals::enabled() || !portals::viewing() || portals::rectAll[portals::drawDepth] || !gl_clipcontrol_able)
    {
        return;
    }
    const glm::vec4 r = portals::rects[portals::drawDepth];
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

// VR_WaterView (each view's frame data): the sides shown on the teleport faces in the view being drawn (gl_shaders.h,
// LiquidPortal; within a view through a gate, the views drawn for it: vr_portals_recursion): each one's plane (normal
// towards the eye, distance), its box (w: how much of the view is shown, 0 none).
extern "C" void VR_PortalFrameData(float plane[8][4], float mins[8][4], float maxs[8][4])
{
    memset(plane, 0, 8 * 4 * sizeof(float));
    memset(mins, 0, 8 * 4 * sizeof(float));
    memset(maxs, 0, 8 * 4 * sizeof(float));
    const portals::Level& level = portals::levels[portals::drawDepth];
    if(!portals::enabled() || !level.texture) { return; }
    for(int layer = 0; layer < level.renderedViews; ++layer)
    {
        const portals::Side& sd = portals::sides[level.renderedSides[layer]];
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

// R_DrawBrushModels_Water: the views through the gates for this view's teleport faces (unit 17, PortalScene; 0: none).
extern "C" unsigned VR_PortalTexture(void)
{
    const portals::Level& level = portals::levels[portals::drawDepth];
    return portals::enabled() && level.renderedViews > 0 ? level.texture : 0u;
}

extern "C" int VR_PortalDrawing(void) { return portals::viewing() ? 1 : 0; }

// R_DrawBrushModels_Water, R_EntHasWater: in a view through a gate, the teleport faces are left out unless views
// within views are on (vr_portals_recursion): then they show the views drawn for them, and the deepest views' faces
// their shimmer.
extern "C" int VR_PortalHideTeleport(void)
{
    return portals::viewing() && portals::maxDepth() <= 1 ? 1 : 0;
}

extern "C" float VR_TeleportOpacity(void)
{
    return portals::enabled() ? za::clamp(vr_teleporter_surface_opacity.value, 0.f, 1.f) : 1.f;
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
    auto& seen = portals::scratch.seen;
    seen.clear();
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
            seen.pushBack(static_cast<int>(&sd - portals::sides.data()));
        }
        if(glm::distance(sd.to, o) < portals::kRange && straddles(portals::reverseSide(sd)))
        {
            const glm::vec3 c = (sd.mins + sd.maxs) * 0.5f;
            add(c - sd.normal * (glm::dot(sd.normal, c) - sd.dist) + sd.normal * 8.f); // just in front of the entry
        }
    }
    // Views within views (vr_portals_recursion): the rooms of the gates seen from a destination sent (in its PVS, in
    // front of them, beyond its exit, near), a gate deeper each round, at most 16 rooms in all.
    auto& next = portals::scratch.nextSeen;
    auto& hopPvs = portals::scratch.hopPvs;
    for(int hop = 1; hop < portals::maxDepth() && !seen.empty() && added.size() < 16; hop++)
    {
        next.clear();
        for(const int s : seen)
        {
            const portals::Side& from = portals::sides[static_cast<za::SizeT>(s)];
            const glm::vec3 p = from.to + from.turn * -from.normal * 8.f;
            vec3_t q{p.x, p.y, p.z};
            mleaf_t* leaf = Mod_PointInLeaf(q, sv.worldmodel);
            if(!leaf || leaf->contents == CONTENTS_SOLID) { continue; }
            hopPvs.resize((sv.worldmodel->numleafs + 7) >> 3);
            memcpy(hopPvs.data(), Mod_LeafPVS(leaf, sv.worldmodel), hopPvs.size()); // (SV_AddToFatPVS reuses its row)
            const glm::vec3 n = from.turn * from.normal;
            for(const portals::Side& sd : portals::sides)
            {
                bool beyond = false;
                for(int c = 0; c < 8 && !beyond; c++)
                {
                    const glm::vec3 corner{(c & 1) ? sd.maxs.x : sd.mins.x, (c & 2) ? sd.maxs.y : sd.mins.y,
                        (c & 4) ? sd.maxs.z : sd.mins.z};
                    beyond = glm::dot(n, corner) < glm::dot(n, from.to) - 1.f;
                }
                if(beyond && glm::dot(sd.normal, from.to) - sd.dist >= 0.f && portals::inPvs(hopPvs.data(), sd.leaf) &&
                    glm::distance(portals::nearestPoint(sd, from.to), from.to) <= portals::kRange && added.size() < 16)
                {
                    add(sd.dest);
                    next.pushBack(static_cast<int>(&sd - portals::sides.data()));
                }
            }
        }
        seen.swap(next);
    }
}

// ----------------------------------------------------------------------------
// Walking and shooting through (vr_portals_walk): the server carries a player through a teleporter as the torso enters
// its plane, and what flies (missiles, grenades, gibs) as its path crosses it, by the side's own mapping: where it is,
// how it moves and where it looks are kept, turned and shifted as the view through the gate shows them, so the view
// does not jump. QuakeC's teleport_touch leaves players to it (portal_handles), including at a frame or sill;
// what the teleport does besides moving him is
// QuakeC's (VR_Portal_Crossed: the trigger's targets, what the hands carry). Monsters too at paired gates
// (vr_portals_monsters: VR_PortalMonsterCross), else Quake's teleport.


namespace qvr::portals
{
}


namespace qvr::portals
{

// vr_portals_info (Debug > Teleporters): the gates built for this map, and where the local player's body is against each
// of them - his box, his torso's middle plane, its distance from the gate's plane, whether it is over the aperture and
// how near his box can bring it. For checking a crossing by hand, and for finding a gate's geometry in a map.
void infoBody()
{
    Con_Printf("VR portals: %d sides (vr_teleporters %g, vr_portals %g, vr_portals_walk %g)\n", static_cast<int>(sides.size()),
        vr_teleporters.value, vr_portals.value, vr_portals_walk.value);
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
        Con_Printf("VR portals: off (vr_teleporters 0): no gates built, nothing goes through one, Quake's plain teleporters.\n");
        return;
    }
    if(!current())
    {
        build();
    }
    progs::ServerVm serverVm; // (the console's qcvm is not the server's: the sides name the server's edicts)
    infoBody();
}

// vr_portals_view (Debug > Teleporters): why this frame looks through a gate or through none - every side of every gate
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
        Con_Printf("VR portals: off (vr_teleporters 0): nothing is looked through.\n");
        return;
    }
    if(!current())
    {
        build();
    }
    Con_Printf("VR portals: looking through side %d (%d sides, range %g, vr_portals %g)\n", chosen,
        static_cast<int>(sides.size()), kRange, vr_portals.value);
    Con_Printf("  last camera: %d gate views rendered (limit %d)\n", levels[0].renderedViews, viewLimit(0));
    for(int d = 1; d < maxDepth(); d++) // views within views: the last view of each depth's own
    {
        Con_Printf("  within the last view %d gate%s deep: %d views rendered (limit %d), the first through side %d\n", d,
            d > 1 ? "s" : "", levels[d].renderedViews, viewLimit(d), levels[d].renderedViews ? levels[d].renderedSides[0] : -1);
    }
    Con_Printf("  last frame: %s\n",
        levels[0].texture ? "the scene through the gate, drawn for that camera" : "nothing drawn through a gate (its faces are dark)");
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
    progs::ServerVm serverVm;
    if(!current()) { build(); }
    edict_t* player = progs::playerEdict();
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
}

// vr_portals_lightviews [<x> <y> <z>] (Debug > Teleporters > Torch Light Views): the views whose torches light this
// frame (prepareLightViews from the eye: each one's depth, the gate it looks out of, its eye carried through), what
// making them costs, and with a point its distance for a torch there (lightDistance; none: not in any view's PVS).
void lightViews_f()
{
    if(!cl.worldmodel)
    {
        return;
    }
    const glm::vec3 eye = vec(r_refdef.vieworg);
    const double t0 = Sys_DoubleTime();
    constexpr int reps = 200;
    for(int i = 0; i < reps; i++)
    {
        prepareLightViews(eye);
    }
    const double us = (Sys_DoubleTime() - t0) * 1e6 / reps;
    int deepest = 0;
    for(int i = 0; i < lightViewCount; i++)
    {
        deepest = za::max(deepest, lightViews.views[i].depth);
    }
    Con_Printf("light views: %d (deepest %d; vr_portals_recursion %g) from %.0f %.0f %.0f, %.1f us to make\n", lightViewCount,
        deepest, vr_portals_recursion.value, eye.x, eye.y, eye.z, us);
    for(int i = 0; i < lightViewCount; i++)
    {
        const LightView& v = lightViews.views[i];
        Con_Printf("  view %d: depth %d, side %d, eye %.0f %.0f %.0f\n", i, v.depth, v.side, v.eye.x, v.eye.y, v.eye.z);
    }
    if(Cmd_Argc() >= 4)
    {
        vec3_t o{static_cast<float>(Q_atof(Cmd_Argv(1))), static_cast<float>(Q_atof(Cmd_Argv(2))), static_cast<float>(Q_atof(Cmd_Argv(3)))};
        const int leaf = static_cast<int>(Mod_PointInLeaf(o, cl.worldmodel) - cl.worldmodel->leafs) - 1;
        const float d = lightDistance(vec(o), leaf);
        if(d >= 1e8f)
        {
            Con_Printf("  point %.0f %.0f %.0f: in no view's PVS\n", o[0], o[1], o[2]);
        }
        else
        {
            Con_Printf("  point %.0f %.0f %.0f: %.0f units for a torch there\n", o[0], o[1], o[2], d);
        }
    }
}

void reachTest_f();
void stuck_f();
void rebuild_f() { if(sv.active) { build(); } }
void registerCommands()
{
    Cmd_AddCommand("vr_portals_rebuild", rebuild_f);
    Cmd_AddCommand("vr_portals_info", info_f);
    Cmd_AddCommand("vr_portals_view", viewInfo_f);
    Cmd_AddCommand("vr_portals_lightviews", lightViews_f);
    Cmd_AddCommand("vr_portals_shot", shot_f);
    Cmd_AddCommand("vr_portals_pulltest", pullTest_f);
    Cmd_AddCommand("vr_portals_reachtest", reachTest_f);
    Cmd_AddCommand("vr_portals_stuck", stuck_f);
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
bool traceFromImage = false; // the last MOVE_PORTALS traceline began at a player's image (a muzzle held through a gate)
// A player's shot beginning this far from his box (units) began through a gate (his muzzle reached through: reachAlong,
// carried to the far side); no arm and gun reach it.
constexpr float kImageReach = 72.f;
glm::vec3 traceEntry[kMaxTraceCrossings];
glm::vec3 traceExit[kMaxTraceCrossings];
glm::mat3 traceTurn{1.f};

} // namespace
} // namespace qvr::portals

extern "C" void VR_PortalTraceBegin(void)
{
    qvr::portals::traceCrossings = 0;
    qvr::portals::traceFromImage = false;
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
    // A player's shot from a muzzle held through a gate (carried to its far side, far from his body): it meets him too
    // (MOVE_HITPASS), as one come out of a gate does: the gun reached into the loop's gate shoots your back.
    const int passNum = passedict ? NUM_FOR_EDICT(passedict) : 0;
    if(passNum >= 1 && passNum <= svs.maxclients && !(type & MOVE_HITPASS))
    {
        const glm::vec3 nearest = glm::clamp(s, vec(passedict->v.absmin), vec(passedict->v.absmax));
        if(glm::distance(s, nearest) > kImageReach)
        {
            traceFromImage = true;
            type |= MOVE_HITPASS;
            *trace = SV_Move(const_cast<float*>(start), vec3_origin, vec3_origin, const_cast<float*>(end), type, passedict);
        }
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
        // Out of the gate it meets the shooter too (MOVE_HITPASS): your shot at your own image hits you.
        *trace = SV_Move(a, vec3_origin, vec3_origin, b, type | MOVE_HITPASS, passedict);
    }
    if(traceCrossings)
    {
        trace->fraction = za::min((before + trace->fraction * glm::distance(s, e)) / total, 1.f);
    }
}

// QuakeC (portal_from_image): whether the last MOVE_PORTALS traceline began at a player's image, his muzzle held
// through a gate (then what it strikes may be him: LightningDamage's MOVE_HITPASS).
extern "C" float VR_PortalFromImage(void)
{
    return qvr::portals::enabled() && qvr::portals::traceFromImage ? 1.f : 0.f;
}

// SV_MoveRun: a missile carried through a gate (VR_PortalToss) meets its owner, Quake's owner rule off for it.
extern "C" int VR_PortalHitsOwner(edict_t* missile)
{
    using namespace qvr;
    const int ofs = progs::fields().vr_gate_crossed;
    return missile && ofs >= 0 && missile->v.owner && progs::fieldFloat(missile, ofs) > 0.f ? 1 : 0;
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
        const trace_t tr = worldtrace::serverLine(root, on);
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
    if(mode == 0.f || viewing() || e < cl_entities || e >= cl_entities + cl_max_edicts) { return; }
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
    progs::ServerVm scope;
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
    const int field = qvr::progs::fields().carry_portal;
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
    progs::ServerVm serverVm;
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

bool crossedGate(const glm::vec3& from, const glm::vec3& to, LightGate& gate)
{
    if(!walkOn()) { return false; }
    if(!current()) { build(); }
    progs::ServerVm serverVm;
    for(const Side& sd : sides)
    {
        const float d0 = glm::dot(sd.normal, from) - sd.dist, d1 = glm::dot(sd.normal, to) - sd.dist;
        if(d0 < 0.f || d1 >= 0.f) { continue; }
        const glm::vec3 c = from + (to - from) * (d0 / (d0 - d1));
        const edict_t* trig = EDICT_NUM(sd.trigger);
        if(!onGate(sd, c, 2.f) || !triggerActive(trig) || (static_cast<int>(trig->v.spawnflags) & 1)) { continue; }
        gate = {sd.from, sd.to, sd.normal, sd.mins, sd.maxs, sd.turn, sd.dist};
        return true;
    }
    return false;
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
    progs::ServerVm serverVm;
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
    progs::ServerVm serverVm;
    edict_t* player = progs::playerEdict();
    const VrMove* move = server::clientMove(player);
    if(move)
    {
        const glm::vec3 raw = move->hands[1].pos;
        const Reach mapped = reach(vec(player->v.origin), raw);
        const int offset = qvr::progs::fields().handpos;
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
}
}
