#include "vr_alloccount.h"
// vr_stealth.cpp -- the monsters' senses' engine side: the light at a point and on each player, each player's flashlight
// beam (QC vr_stealth.qc; docs/vr-port/STEALTH.md). Each client measures the light on its own player and sends it
// with its lamp's beam in its VR move (vr_move.hpp): a coop client's lamp and the light he stands in are his own.

#include "vr_stealth.hpp"
#include "vr_geom.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flashlight.hpp"
#include "vr_hands.hpp"
#include "vr_move.hpp"
#include "vr_physsound.hpp"
#include "vr_profile.h"
#include "vr_progs.hpp"
#include "vr_server.hpp"
#include "vr_text3d.hpp"

#include "Zancle/Container/Array.hpp"
#include "Zancle/Vocabulary/Span.hpp"

#include <glm/glm.hpp>
#include <string.h>

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

[[nodiscard]] float lightNow(const glm::vec3& point, float* dynamic = nullptr)
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
            const float k = add * (c > 0.f ? glm::min(c, 1.f) : 1.f);
            light += k;
            if(dynamic)
            {
                *dynamic += k;
            }
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

// ---- The see-through trace (traceseethrough)

// vr_stealth_test_fence: a texture named with one of its (space-separated) prefixes counts as a fence (the tests: the
// kit's maps have no solid '{' brush).
[[nodiscard]] bool testFence(const char* texture)
{
    const char* list = vr_stealth_test_fence.string;
    while(list && *list)
    {
        while(*list == ' ')
        {
            list++;
        }
        size_t n = 0;
        while(list[n] && list[n] != ' ')
        {
            n++;
        }
        if(n > 0 && q_strncasecmp(texture, list, n) == 0)
        {
            return true;
        }
        list += n;
    }
    return false;
}

// Where a trace that hit `hit` at `at` (going `dir`) comes out of it, when what it hit is see-through: a fence (an
// alpha-tested '{' texture: a grate, a fence, a web) on its brush model (the world's for world), or the whole entity
// drawn see-through (its alpha under 1: a glass func_wall). False: opaque, or no way out within 64 units.
[[nodiscard]] bool passThrough(edict_t* hit, const glm::vec3& at, const glm::vec3& dir, glm::vec3& out)
{
    if(!hit)
    {
        return false;
    }
    const bool world = hit == qcvm->edicts;
    const float alpha = world ? 0.f : progs::fieldFloatOr(hit, progs::fields().alpha, 0.f);
    const int index = world ? 1 : static_cast<int>(hit->v.modelindex);
    const qmodel_t* m = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const bool brush = m && m->type == mod_brush && m->nodes && m->surfaces;
    const glm::vec3 origin = world ? glm::vec3{0.f} : geom::vec(hit->v.origin);
    const glm::vec3 local = at - origin;
    bool see = alpha > 0.f && alpha < 1.f;
    if(!see && brush)
    {
        const msurface_t* surf = physsound::surfaceOnSegment(m, local - dir, local + dir * 2.f);
        see = surf && ((surf->flags & SURF_DRAWFENCE) || testFence(m->textures[surf->texinfo->texnum]->name));
    }
    if(!see)
    {
        return false;
    }
    if(brush)
    {
        hull_t* hull = const_cast<hull_t*>(&m->hulls[0]);
        for(float step = 1.f; step <= 64.f; step += 1.f)
        {
            const glm::vec3 p = local + dir * step;
            vec3_t v{p.x, p.y, p.z};
            if(SV_HullPointContents(hull, hull->firstclipnode, v) != CONTENTS_SOLID)
            {
                out = at + dir * (step + 0.25f);
                return true;
            }
        }
        return false;
    }
    // (a see-through model's box: out of its far side)
    float exitAt = 1e9f;
    for(int i = 0; i < 3; i++)
    {
        if(glm::abs(dir[i]) > 1e-6f)
        {
            const float face = dir[i] > 0.f ? hit->v.absmax[i] : hit->v.absmin[i];
            exitAt = glm::min(exitAt, (face - at[i]) / dir[i]);
        }
    }
    if(exitAt > 256.f || exitAt < 0.f)
    {
        return false;
    }
    out = at + dir * (exitAt + 0.25f);
    return true;
}

} // namespace

float lightAt(const glm::vec3& point)
{
    if(!cl.worldmodel || cls.state != ca_connected || cls.demoplayback || cls.signon != SIGNONS)
    {
        ownLight = {};
        return -1.f;
    }
    if(vr_mp_lockstep_ahead.value >= 1.f)
    {
        // (the harness's lockstep, B14c: each move's own, read afresh -- the tenth of a second's samples fell at the
        // client's clock since its load, and the lightmap's cached spot moved with them: two runs' .stl_light apart)
        lightCache = {};
        ownLight = {lightNow(point), cl.time, cl.worldmodel};
        return ownLight.light;
    }
    if(ownLight.world != cl.worldmodel || cl.time >= ownLight.at + 0.1 || cl.time < ownLight.at)
    {
        ownLight = {lightNow(point), cl.time, cl.worldmodel};
    }
    return ownLight.light;
}

float lightFresh(const glm::vec3& point, float* dynamic)
{
    if(!cl.worldmodel || cls.state != ca_connected || cls.signon != SIGNONS)
    {
        return -1.f;
    }
    return lightNow(point, dynamic);
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

void PF_traceseethrough()
{
    const float* a = G_VECTOR(OFS_PARM0);
    const float* b = G_VECTOR(OFS_PARM1);
    const int type = static_cast<int>(G_FLOAT(OFS_PARM2)) & ~MOVE_PORTALS;
    edict_t* ignore = G_EDICT(OFS_PARM3);
    const glm::vec3 from{a[0], a[1], a[2]}, to{b[0], b[1], b[2]};
    const float length = glm::length(to - from);
    const glm::vec3 dir = length > 0.f ? (to - from) / length : glm::vec3{0.f, 0.f, 1.f};
    vec3_t start{a[0], a[1], a[2]}, end{b[0], b[1], b[2]};
    trace_t tr = SV_Move(start, vec3_origin, vec3_origin, end, type, ignore);
    bool inOpen = tr.inopen, inWater = tr.inwater;
    int passed = 0;
    // (at most 8 see-through things in a line: each one stepped out of, the trace on from there)
    while(tr.fraction < 1.f && !tr.allsolid && !tr.startsolid && passed < 8)
    {
        glm::vec3 out;
        if(!passThrough(tr.ent, geom::vec(tr.endpos), dir, out))
        {
            break;
        }
        passed++;
        if(glm::dot(out - from, dir) >= length)
        {
            tr.fraction = 1.f;
            tr.ent = nullptr;
            VectorCopy(end, tr.endpos);
            break;
        }
        vec3_t next{out.x, out.y, out.z};
        tr = SV_Move(next, vec3_origin, vec3_origin, end, type, ignore);
        inOpen = inOpen || tr.inopen;
        inWater = inWater || tr.inwater;
    }
    if(passed > 0 && tr.fraction < 1.f)
    {
        const glm::vec3 e{tr.endpos[0], tr.endpos[1], tr.endpos[2]};
        tr.fraction = length > 0.f ? glm::clamp(glm::length(e - from) / length, 0.f, 1.f) : 0.f;
        if(tr.fraction >= 1.f)
        {
            tr.fraction = 0.9999f;
        }
    }
    pr_global_struct->trace_allsolid = tr.allsolid;
    pr_global_struct->trace_startsolid = tr.startsolid;
    pr_global_struct->trace_fraction = tr.fraction;
    pr_global_struct->trace_inwater = inWater;
    pr_global_struct->trace_inopen = inOpen;
    VectorCopy(tr.endpos, pr_global_struct->trace_endpos);
    VectorCopy(tr.plane.normal, pr_global_struct->trace_plane_normal);
    pr_global_struct->trace_plane_dist = tr.plane.dist;
    edict_t* const hitEnt = tr.ent ? tr.ent : qcvm->edicts;
    pr_global_struct->trace_ent = EDICT_TO_PROG(hitEnt);
    G_FLOAT(OFS_RETURN) = static_cast<float>(passed);
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


// ---- The drawn moves' log (vr_debug_drawn_moves): a monster drawn jumping between frames (the dogs' turns)

namespace
{

struct DrawnTrack
{
    int num{-1};
    double time{-1.0};    // the client time of the last frame it was drawn at
    glm::vec3 drawn{0.f}; // and where
    float usual{0.f};     // its usual step a frame (smoothed)
    int restarts{0};      // move lerps begun since that frame: with a move, and with only a turn
    int turnRestarts{0};
    float blendAtRestart{0.f}; // how far along its move it was drawn when the last one began
    float restartTurn{0.f};    // and its yaw's change then, its origin's, the old move's length (s) and age (s)
    float restartMove{0.f};
    float oldLength{0.f};
    float oldAge{0.f};
};

struct DrawnLog
{
    za::Array<DrawnTrack, 16> tracks{};
    int jumps{0};
    int frames{0};
    int snaps{0};          // new moves begun with over 4 units of the last one left to draw (Quake's rule snaps them)
    float worstSnap{0.f};
    float worstStep{0.f}; // the farthest it was drawn to move in a frame (a leap's flight: ~5 units at 90 fps)
    int worstStepFrame{0}; // its model frame then
};
DrawnLog drawnLog;

[[nodiscard]] DrawnTrack* drawnTrack(const entity_t* e)
{
    const int num = static_cast<int>(e - cl_entities);
    if(num <= 0 || num >= cl.num_entities || !e->model || !strstr(e->model->name, vr_debug_drawn_moves.string))
    {
        return nullptr;
    }
    DrawnTrack* free = nullptr;
    for(DrawnTrack& t : drawnLog.tracks)
    {
        if(t.num == num)
        {
            return &t;
        }
        if(!free && (t.num < 0 || cl.time - t.time > 1.0))
        {
            free = &t;
        }
    }
    if(free)
    {
        *free = DrawnTrack{};
        free->num = num;
    }
    return free;
}

} // namespace

} // namespace qvr::stealth

int VR_MoveLerpContinuous()
{
    return qvr::vr_monster_lerp_continue.value != 0.f;
}

int VR_DebugDrawnOn()
{
    return qvr::vr_debug_drawn_moves.string[0] != 0;
}

void VR_DebugDrawnRestart(const entity_t* e, int moved)
{
    using namespace qvr::stealth;
    DrawnTrack* t = drawnTrack(e);
    if(!t)
    {
        return;
    }
    // (Quake's rule would have drawn it from its last move's end: the snap that was, whichever rule is on)
    const float left = (1.f - R_MoveLerpBlend(e)) * glm::length(glm::vec3{e->currentorigin[0] - e->previousorigin[0],
                                                         e->currentorigin[1] - e->previousorigin[1],
                                                         e->currentorigin[2] - e->previousorigin[2]});
    if(left > 4.f)
    {
        drawnLog.snaps++;
        drawnLog.worstSnap = glm::max(drawnLog.worstSnap, left);
    }
    t->restarts++;
    t->turnRestarts += moved ? 0 : 1;
    t->blendAtRestart = R_MoveLerpBlend(e);
    float turn = 0.f;
    for(int i = 0; i < 3; i++)
    {
        float d = e->angles[i] - e->currentangles[i];
        d = d > 180.f ? d - 360.f : d < -180.f ? d + 360.f : d;
        turn += glm::abs(d) * (i == 1 ? 1.f : 1000.f); // (a pitch or roll change counted in thousands)
    }
    t->restartTurn = turn;
    t->restartMove = glm::length(glm::vec3{e->origin[0] - e->currentorigin[0], e->origin[1] - e->currentorigin[1],
        e->origin[2] - e->currentorigin[2]});
    t->oldLength = e->movelerpfinish > e->movelerpstart ? static_cast<float>(e->movelerpfinish - e->movelerpstart) : 0.1f;
    t->oldAge = static_cast<float>(cl.time - e->movelerpstart);
}

void VR_DebugDrawnMove(const entity_t* e, const float* drawn)
{
    using namespace qvr::stealth;
    DrawnTrack* t = drawnTrack(e);
    if(!t || t->time == cl.time)
    {
        return; // (drawn again this frame: the other eye, a shadow)
    }
    const glm::vec3 at{drawn[0], drawn[1], drawn[2]};
    if(t->time >= 0.0 && cl.time - t->time < 0.1)
    {
        const float step = glm::length(at - t->drawn);
        drawnLog.frames++;
        // (a snap: a new move begun, and drawn farther in the frame than its own lerp goes: a fast step alone is not)
        const glm::vec3 prev{e->previousorigin[0], e->previousorigin[1], e->previousorigin[2]};
        const glm::vec3 cur{e->currentorigin[0], e->currentorigin[1], e->currentorigin[2]};
        const double length = e->movelerpfinish > e->movelerpstart ? e->movelerpfinish - e->movelerpstart : 0.1;
        const float own = glm::length(cur - prev) * static_cast<float>((cl.time - t->time) / length);
        if(t->restarts > 0 && step > glm::max(6.f, 3.f * t->usual) && step > 1.5f * own + 2.f)
        {
            drawnLog.jumps++;
            Con_Printf("drawnmove: %.3f ent %d (%s) jumped %.1f units in a frame (usual %.1f); lerps begun %d (%d by a turn "
                       "alone), the last at %.2f of its move (%.3f s old of %.3f), turning %.1f, moving %.1f; frame %d\n",
                cl.time, t->num, e->model->name, step, t->usual, t->restarts, t->turnRestarts, t->blendAtRestart, t->oldAge,
                t->oldLength, t->restartTurn, t->restartMove, static_cast<int>(e->frame));
        }
        if(step > drawnLog.worstStep)
        {
            drawnLog.worstStep = step;
            drawnLog.worstStepFrame = static_cast<int>(e->frame);
        }
        t->usual = t->usual * 0.9f + glm::min(step, 3.f * t->usual + 1.f) * 0.1f;
        if(drawnLog.frames % 900 == 0)
        {
            Con_Printf("drawnmove: %d frames drawn, %d jumps; %d moves begun early (Quake's drawing snaps them; the worst %.1f "
                       "units); the farthest drawn in a frame %.1f units (frame %d)\n", drawnLog.frames, drawnLog.jumps,
                drawnLog.snaps, drawnLog.worstSnap, drawnLog.worstStep, drawnLog.worstStepFrame);
        }
    }
    t->time = cl.time;
    t->drawn = at;
    t->restarts = 0;
    t->turnRestarts = 0;
}

// ---- Debug > Views > Lift Riders (vr_debug_lift): how far the player and monsters standing on a moving lift (a
// MOVETYPE_PUSH brush they stand on, moving up or down) sink into it: their boxes on the server, their drawn models and
// (the player) the drawn eye against the drawn lift. A local game only (it reads the server's edicts). Each ride's
// worst printed as the lift stops ("liftdebug: ride ..."): his notes of 2026-11-21 (riders "compressed by the lift").
namespace qvr::stealth
{
namespace
{
struct LiftWorst
{
    float box{-999.f};   // the box's sinking into the lift's top (units; under 0: above it)
    float drawn{-999.f}; // the drawn model's (its drawn feet under the drawn lift's top)
    float eye{-999.f};   // the player's drawn eye's height over the drawn lift short of his eye's over it on the server
    int frames{0};
};

struct LiftLog
{
    double frameTime{-1.0}; // the frame last looked at (the first model drawn in a frame does it)
    int lift{0};            // the lift under the player or a monster this ride, and its way (1 up, -1 down)
    int way{0};
    LiftWorst player;
    LiftWorst monsters;
};

LiftLog liftLog;

[[nodiscard]] edict_t* liftSvEdict(int num)
{
    if(num <= 0 || num >= sv.qcvm.num_edicts)
    {
        return nullptr;
    }
    edict_t* ed = reinterpret_cast<edict_t*>(reinterpret_cast<byte*>(sv.qcvm.edicts) + num * sv.qcvm.edict_size);
    return ed->free ? nullptr : ed;
}

// The moving lift `ed` stands on (its edict number), 0 none.
[[nodiscard]] int liftUnder(const edict_t* ed)
{
    if(!(static_cast<int>(ed->v.flags) & FL_ONGROUND) || ed->v.groundentity <= 0 ||
        ed->v.groundentity % sv.qcvm.edict_size != 0)
    {
        return 0;
    }
    const int num = ed->v.groundentity / sv.qcvm.edict_size;
    const edict_t* g = liftSvEdict(num);
    if(!g || static_cast<int>(g->v.movetype) != MOVETYPE_PUSH || g->v.velocity[2] == 0.f || num >= cl.num_entities)
    {
        return 0;
    }
    return num;
}

// The drawn top of lift `num` (its client entity's place as drawn this frame plus its model's top).
[[nodiscard]] float liftDrawnTop(int num)
{
    const entity_t& le = cl_entities[num];
    return le.model ? le.origin[2] + le.model->maxs[2] : le.origin[2];
}

void liftNote(LiftWorst& w, float box, float drawn, float eye)
{
    w.box = glm::max(w.box, box);
    w.drawn = glm::max(w.drawn, drawn);
    w.eye = glm::max(w.eye, eye);
    w.frames++;
}

void liftReport()
{
    if(liftLog.lift && (liftLog.player.frames || liftLog.monsters.frames))
    {
        Con_Printf("liftdebug: ride %s of lift %d: player %d frames, box %.2f drawn %.2f eye %.2f; monsters %d frames, box "
                   "%.2f drawn %.2f (units sunk into it, the worst)\n", liftLog.way > 0 ? "up" : "down", liftLog.lift,
            liftLog.player.frames, liftLog.player.box, liftLog.player.drawn, liftLog.player.eye, liftLog.monsters.frames,
            liftLog.monsters.box, liftLog.monsters.drawn);
    }
    liftLog.player = {};
    liftLog.monsters = {};
    liftLog.lift = 0;
    liftLog.way = 0;
}

// Once a frame: the ride's end (the lift stopped or turned), then the player on it.
void liftFrame()
{
    if(liftLog.lift)
    {
        const edict_t* g = liftSvEdict(liftLog.lift);
        const int way = !g ? 0 : g->v.velocity[2] > 0.f ? 1 : g->v.velocity[2] < 0.f ? -1 : 0;
        if(way != liftLog.way)
        {
            liftReport();
        }
    }
    const edict_t* p = liftSvEdict(cl.viewentity);
    const int lift = p ? liftUnder(p) : 0;
    if(!lift)
    {
        return;
    }
    const edict_t* g = liftSvEdict(lift);
    liftLog.lift = lift;
    liftLog.way = g->v.velocity[2] > 0.f ? 1 : -1;
    const float top = g->v.origin[2] + g->v.maxs[2];
    const float drawnTop = liftDrawnTop(lift);
    const float drawnFeet = cl_entities[cl.viewentity].origin[2] + p->v.mins[2];
    const float eyeOver = p->v.origin[2] + p->v.view_ofs[2] - top;
    const float drawnEyeOver = r_refdef.vieworg[2] - drawnTop;
    liftNote(liftLog.player, top - (p->v.origin[2] + p->v.mins[2]), drawnTop - drawnFeet, eyeOver - drawnEyeOver);
}
} // namespace
} // namespace qvr::stealth

int VR_DebugLiftOn()
{
    return qvr::vr_debug_lift.value != 0.f && sv.active && sv.qcvm.edicts && !cls.demoplayback;
}

void VR_DebugLiftDrawn(const entity_t* e, const float* drawn)
{
    using namespace qvr::stealth;
    if(liftLog.frameTime != cl.time)
    {
        liftLog.frameTime = cl.time;
        liftFrame();
    }
    const int num = static_cast<int>(e - cl_entities);
    if(num <= 0 || num >= cl.num_entities || num == cl.viewentity)
    {
        return;
    }
    const edict_t* m = liftSvEdict(num);
    if(!m || !(static_cast<int>(m->v.flags) & FL_MONSTER))
    {
        return;
    }
    const int lift = liftUnder(m);
    if(!lift)
    {
        return;
    }
    const edict_t* g = liftSvEdict(lift);
    if(!liftLog.lift)
    {
        liftLog.lift = lift;
        liftLog.way = g->v.velocity[2] > 0.f ? 1 : -1;
    }
    const float top = g->v.origin[2] + g->v.maxs[2];
    liftNote(liftLog.monsters, top - (m->v.origin[2] + m->v.mins[2]), liftDrawnTop(lift) - (drawn[2] + m->v.mins[2]), -999.f);
    if(qvr::vr_debug_lift.value >= 2.f)
    {
        Con_Printf("liftdebug: %.3f ent %d on lift %d (top %.2f, drawn %.2f, vz %.0f): feet %.2f, drawn %.2f\n", cl.time, num,
            lift, top, liftDrawnTop(lift), g->v.velocity[2], m->v.origin[2] + m->v.mins[2], drawn[2] + m->v.mins[2]);
    }
}

namespace qvr::stealth
{

// ---- Debug > Tests > Stealth AI > Meters Over Monsters (vr_stealth_debug_meters)

namespace
{

// A monster's label: its state (and an Alert one's phase, a Hostile one's time out of sight), its meter as a bar.
void meterLabel(edict_t* e, int stateOfs, int phaseOfs, int meterOfs, int lostOfs, int trailOfs, int classOfs,
    const glm::vec3& head)
{
    const auto read = [e](int ofs) {
        const eval_t* v = ofs >= 0 ? GetEdictFieldValue(e, ofs) : nullptr;
        return v ? v->_float : 0.f;
    };
    const glm::vec3 top{(e->v.absmin[0] + e->v.absmax[0]) * 0.5f, (e->v.absmin[1] + e->v.absmax[1]) * 0.5f, e->v.absmax[2]};
    const float dist = glm::length(top - head);
    if(dist > vr_stealth_debug_meters_range.value || dist < 1.f)
    {
        return;
    }
    const float meter = glm::clamp(read(meterOfs), 0.f, 1.f);
    const bool hostile = e->v.enemy != 0;
    const char* state = "idle";
    char line[64];
    if(read(classOfs) < 0.f)
    {
        state = "quake's ai";
    }
    else if(hostile)
    {
        const float lost = read(lostOfs);
        const float hidden = lost > 0.f ? static_cast<float>(qcvm->time) - lost : 0.f;
        // (out of sight: the step of his trail it follows, vr_stealth.qc VR_Stealth_Trail: 1 his last spot, 2 ahead, 3 at him)
        if(hidden > 0.2f)
        {
            q_snprintf(line, sizeof(line), "HOSTILE  trail %d  unseen %.1fs", static_cast<int>(read(trailOfs)), hidden);
        }
        else
        {
            q_snprintf(line, sizeof(line), "HOSTILE");
        }
        state = line;
    }
    else if(read(stateOfs) > 0.f)
    {
        static constexpr const char* phases[] = {"ALERT", "ALERT turn", "ALERT walk", "ALERT search", "ALERT return"};
        const int phase = glm::clamp(static_cast<int>(read(phaseOfs)), 0, 4);
        state = phases[phase];
    }
    char text[160];
    q_snprintf(text, sizeof(text), "%s  %.2f\n%20s", state, meter, "");
    const float cells = 20.f;
    const glm::vec4 color = hostile ? glm::vec4{1.f, 0.25f, 0.15f, 0.95f}
                            : read(stateOfs) > 0.f ? glm::vec4{1.f, 0.8f, 0.2f, 0.95f}
                                                   : glm::vec4{0.35f, 0.9f, 0.45f, 0.95f};
    // (the bar under the state, its 20 cells the meter's 0..1; the glimpse's cell marked by the track's end)
    const text3d::OverlayBar bar{1, 0.f, meter * cells, cells, color};
    const float scale = glm::max(0.12f, 0.0022f * dist);
    const glm::vec3 at = top + glm::vec3{0.f, 0.f, 10.f + 16.f * scale};
    const glm::vec3 to = at - head; // (the text faces the way it is looked at, as the head's yaw does)
    const float yaw = glm::degrees(glm::atan(to.y, to.x));
    text3d::queueOverlay(text, at, glm::vec3{0.f, yaw, 0.f}, scale, za::Span<const text3d::OverlayBar>{&bar, 1}, 0.45f);
}

} // namespace

void debugFrame()
{
    if(!vr_stealth_debug_meters.value || !sv.active || !cl.worldmodel)
    {
        return;
    }
    qcvm_t* const old = qcvm;
    if(old != &sv.qcvm)
    {
        if(old)
        {
            PR_SwitchQCVM(nullptr);
        }
        PR_SwitchQCVM(&sv.qcvm);
    }
    const int stateOfs = progs::fields().stl_state;
    const int phaseOfs = progs::fields().stl_phase;
    const int meterOfs = progs::fields().stl_meter;
    const int lostOfs = progs::fields().stl_lost_time;
    const int trailOfs = progs::fields().stl_trail;
    const int classOfs = progs::fields().stl_class;
    const glm::vec3 head = hands::current().head;
    if(meterOfs >= 0)
    {
        progs::forEachEdict(1, [&](edict_t* e) {
            if((static_cast<int>(e->v.flags) & FL_MONSTER) && e->v.health > 0.f)
            {
                meterLabel(e, stateOfs, phaseOfs, meterOfs, lostOfs, trailOfs, classOfs, head);
            }
        });
    }
    if(old != &sv.qcvm)
    {
        PR_SwitchQCVM(nullptr);
        if(old)
        {
            PR_SwitchQCVM(old);
        }
    }
}

} // namespace qvr::stealth
