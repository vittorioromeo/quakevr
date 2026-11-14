// vr_lagcomp.cpp -- lag compensation (vr_lagcomp.hpp; MP_PLAN.md, B8b).

#include "vr_lagcomp.hpp"

#include "vr_api.h"
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_hitmodel_internal.hpp"
#include "vr_netmove.hpp"
#include "vr_netsnap.hpp"
#include "vr_progs.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <string.h>

namespace qvr::lagcomp
{

namespace
{

constexpr int frames = 80; // server frames kept (1.1 s at 72 Hz: past vr_net_lagcomp_most's most, 1 s)

// A target as a frame left it.
struct Entry
{
    int num{0};
    float origin[3]{}, angles[3]{};
    float frame{0.f};
    bool step{false};
    bool tracked{false}; // (its hit model track then: `track`)
    hitmodel::detail::Track track;
    bool hands{false};   // a player's hands (B8c: a parry of his guard, a blow at what he holds): [0] off, [1] main
    glm::vec3 handPos[2]{glm::vec3{0.f}, glm::vec3{0.f}}, handRot[2]{glm::vec3{0.f}, glm::vec3{0.f}};
};
struct Frame
{
    double time{-1.0};
    za::Vector<Entry> of; // by entity number, ascending
};
struct History
{
    Frame ring[frames];
    int head{0}; // the next to write
};
History history;

struct Seen
{
    bool valid{false};
    double time{0.0}, back{0.0};
};
Seen seen[MAX_SCOREBOARD];

// begin's: each target moved, as it was before (`now`) and as put back (`then`): end puts it back, keeping what the
// scope did to it (a blow's push moved it, its death changed its frame).
struct Moved
{
    Entry now, then;
};
struct Saved
{
    bool on{false};
    za::Vector<Moved> of;
    double backMs{0.0}; // (the last scope's: how far back the others went, for its report)
};
Saved saved;

[[nodiscard]] bool isTarget(const edict_t* e)
{
    return !e->free && (static_cast<int>(e->v.flags) & (FL_MONSTER | FL_CLIENT)) &&
           static_cast<int>(e->v.solid) != SOLID_NOT && e->area.prev; // (linked: in the world's areas)
}

[[nodiscard]] bool anyRemote()
{
    for(int i = 0; i < svs.maxclients; i++)
    {
        if(svs.clients[i].active && netmove::serverSends(i))
        {
            return true;
        }
    }
    return false;
}

void take(const edict_t* e, Entry& out)
{
    out.num = NUM_FOR_EDICT(const_cast<edict_t*>(e));
    memcpy(out.origin, e->v.origin, sizeof(out.origin));
    memcpy(out.angles, e->v.angles, sizeof(out.angles));
    out.frame = e->v.frame;
    out.step = static_cast<int>(e->v.movetype) == MOVETYPE_STEP;
    out.tracked = out.num < static_cast<int>(hitmodel::detail::tracks.size());
    if(out.tracked)
    {
        out.track = hitmodel::detail::tracks[static_cast<za::SizeT>(out.num)];
    }
    edict_t* me = const_cast<edict_t*>(e);
    out.hands = (static_cast<int>(e->v.flags) & FL_CLIENT) && progs::bindings().isVrProgs;
    for(int h = 0; out.hands && h < 2; h++)
    {
        out.handPos[h] = progs::fieldVec(me, progs::handFields(h).pos);
        out.handRot[h] = progs::fieldVec(me, progs::handFields(h).rot);
    }
}

void put(edict_t* e, const Entry& in)
{
    memcpy(e->v.origin, in.origin, sizeof(in.origin));
    memcpy(e->v.angles, in.angles, sizeof(in.angles));
    e->v.frame = in.frame;
    if(in.tracked && in.num < static_cast<int>(hitmodel::detail::tracks.size()))
    {
        hitmodel::detail::tracks[static_cast<za::SizeT>(in.num)] = in.track;
    }
    for(int h = 0; in.hands && h < 2; h++)
    {
        progs::setFieldVec(e, progs::handFields(h).pos, in.handPos[h]);
        progs::setFieldVec(e, progs::handFields(h).rot, in.handRot[h]);
    }
    SV_LinkEdict(e, false);
}

[[nodiscard]] bool same(const float* a, const float* b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

// What end puts back for a target the scope may have changed: its place now moved by what the scope moved it (a push,
// a setorigin), its angles, frame and track the scope's when it changed them (a death's frame), else as they were now.
[[nodiscard]] Entry restored(edict_t* e, const Moved& m)
{
    Entry cur;
    take(e, cur);
    Entry out = m.now;
    for(int i = 0; i < 3; i++)
    {
        out.origin[i] = m.now.origin[i] + (cur.origin[i] - m.then.origin[i]);
    }
    if(!same(cur.angles, m.then.angles))
    {
        memcpy(out.angles, cur.angles, sizeof(out.angles));
    }
    if(cur.frame != m.then.frame)
    {
        out.frame = cur.frame;
        out.track = cur.track; // (posed by the scope's frame: the next frame's track follows it)
    }
    for(int h = 0; out.hands && cur.hands && h < 2; h++)
    {
        out.handPos[h] = m.now.handPos[h] + (cur.handPos[h] - m.then.handPos[h]);
        if(cur.handRot[h] != m.then.handRot[h])
        {
            out.handRot[h] = cur.handRot[h];
        }
    }
    return out;
}

[[nodiscard]] const Entry* entryOf(const Frame& f, int num)
{
    za::SizeT lo = 0, hi = f.of.size();
    while(lo < hi)
    {
        const za::SizeT mid = (lo + hi) / 2;
        if(f.of[mid].num < num)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }
    return lo < f.of.size() && f.of[lo].num == num ? &f.of[lo] : nullptr;
}

// The kept frames around time `t`: the newest at or before it (`a`; none: the oldest kept) and the one after (`b`, or
// null) -> false: nothing kept.
[[nodiscard]] bool framesAround(double t, const Frame*& a, const Frame*& b)
{
    a = b = nullptr;
    for(int k = 0; k < frames; k++)
    {
        const Frame& f = history.ring[(history.head - 1 - k + 2 * frames) % frames];
        if(f.time < 0.0)
        {
            break;
        }
        if(f.time <= t)
        {
            a = &f;
            return true;
        }
        b = &f;
        a = &f; // (older than every kept: the oldest)
    }
    if(a == b)
    {
        b = nullptr;
    }
    return a != nullptr;
}

// Target `num` at time `t` (between the frames around it: a player's place as the interpolation buffer draws it; a
// frame, angles and track as the frame before it left them) -> false: not kept then.
[[nodiscard]] bool entryAt(int num, double t, Entry& out)
{
    const Frame *fa = nullptr, *fb = nullptr;
    if(!framesAround(t, fa, fb))
    {
        return false;
    }
    const Entry* a = entryOf(*fa, num);
    if(!a)
    {
        return false;
    }
    out = *a;
    const Entry* b = fb ? entryOf(*fb, num) : nullptr;
    if(b && !a->step && fb->time > fa->time)
    {
        const float f = za::clamp(static_cast<float>((t - fa->time) / (fb->time - fa->time)), 0.f, 1.f);
        for(int i = 0; i < 3; i++)
        {
            out.origin[i] = a->origin[i] + (b->origin[i] - a->origin[i]) * f;
        }
        for(int h = 0; a->hands && b->hands && h < 2; h++)
        {
            out.handPos[h] = a->handPos[h] + (b->handPos[h] - a->handPos[h]) * f;
        }
    }
    return true;
}

} // namespace

void serverFrame()
{
    if(!sv.active || svs.maxclients <= 1 || vr_net_lagcomp.value == 0.f || !anyRemote())
    {
        return;
    }
    Frame& f = history.ring[history.head];
    history.head = (history.head + 1) % frames;
    f.time = qcvm->time;
    f.of.clear();
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        const edict_t* e = EDICT_NUM(i);
        if(isTarget(e))
        {
            Entry en;
            take(e, en);
            f.of.pushBack(en);
        }
    }
}

void noteMove(int slot, float time, float viewBack)
{
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        seen[slot] = Seen{true, static_cast<double>(time), static_cast<double>(za::clamp(viewBack, 0.f, 1.f))};
    }
}

void reset()
{
    for(Frame& f : history.ring)
    {
        f.time = -1.0;
        f.of.clear();
    }
    history.head = 0;
    for(Seen& s : seen)
    {
        s = Seen{};
    }
    saved.on = false;
    saved.of.clear();
}

bool begin(edict_t* shooter, double most)
{
    if(saved.on || !sv.active || vr_net_lagcomp.value == 0.f || !shooter)
    {
        return false;
    }
    const int slot = NUM_FOR_EDICT(shooter) - 1;
    if(slot < 0 || slot >= svs.maxclients || slot >= MAX_SCOREBOARD || !seen[slot].valid || !netmove::serverSends(slot))
    {
        return false;
    }
    const double now = qcvm->time;
    most = za::clamp(most >= 0.0 ? most : static_cast<double>(vr_net_lagcomp_most.value), 0.0, 1.0);
    // (a stepping monster drawn at the newest message's time; the rest at the interpolation buffer's)
    const double stepAt = za::clamp(seen[slot].time, now - most, now);
    const double bufferAt = za::clamp(seen[slot].time - seen[slot].back, now - most, now);
    if(now - stepAt < 1e-4 && now - bufferAt < 1e-4)
    {
        return false;
    }
    saved.of.clear();
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e == shooter || !isTarget(e))
        {
            continue;
        }
        Entry then;
        const bool step = static_cast<int>(e->v.movetype) == MOVETYPE_STEP;
        if(!entryAt(i, step ? stepAt : bufferAt, then))
        {
            continue;
        }
        Moved m;
        take(e, m.now);
        m.then = then;
        saved.of.pushBack(m);
        put(e, then);
    }
    saved.on = true;
    saved.backMs = (now - bufferAt) * 1000.0;
    hitmodel::detail::poseTimeOverride = stepAt; // (the monsters' poses as his client lerped them)
    return true;
}

void end()
{
    if(!saved.on)
    {
        return;
    }
    for(const Moved& m : saved.of)
    {
        edict_t* e = EDICT_NUM(m.now.num);
        if(!e->free) // (removed by the scope: gibbed)
        {
            put(e, restored(e, m));
        }
    }
    saved.of.clear();
    saved.on = false;
    hitmodel::detail::poseTimeOverride = -1.0;
}

namespace
{

// "vr_lagcomp_shot <x y z> <x y z> <time> <back>" from a client (vr_mp_shoot): a shot's line traced as his shots are
// (MOVE_HITMODEL), without and with the targets put back where he saw them -> "mp_lagcomp_shot:" (the harness's
// mp_lagcomp). Its time and back are what his move of that frame said (a reliable command may come a round trip after
// that move); move_ms: how far back his newest move put them. Nothing hurt.
// A punch's segment (vr_lagcomp_punch): the target it meets first (a player's or monster's hit model, as melee's
// hitmodel_segment judges a blow; another's box without one), 0 none.
[[nodiscard]] int punchMeets(edict_t* self, const glm::vec3& a, const glm::vec3& b)
{
    constexpr float radius = 4.f; // (a fist's line)
    int best = 0;
    float bestT = 2.f;
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e == self || !isTarget(e))
        {
            continue;
        }
        float t = -1.f;
        hitmodel::Hit h;
        if(hitmodel::target(e))
        {
            const float r = radius + hitmodel::tolerance(hitmodel::Class::Melee);
            t = hitmodel::segment(e, a, b, r, 1.f, hitmodel::Class::Melee, h) ? h.t : -1.f;
        }
        else
        {
            // (its box grown by the line's thickness, as melee's VR_SegmentBox: a player)
            float t0 = 0.f, t1 = 1.f;
            for(int k = 0; k < 3 && t0 <= t1; k++)
            {
                const float lo = e->v.origin[k] + e->v.mins[k] - radius, hi = e->v.origin[k] + e->v.maxs[k] + radius;
                const float d = b[k] - a[k];
                if(za::abs(d) < 1e-6f)
                {
                    t1 = a[k] < lo || a[k] > hi ? -1.f : t1;
                    continue;
                }
                const float u = (lo - a[k]) / d, v = (hi - a[k]) / d;
                t0 = za::max(t0, za::min(u, v));
                t1 = za::min(t1, za::max(u, v));
            }
            t = t0 <= t1 ? t0 : -1.f;
        }
        if(t >= 0.f && t < bestT)
        {
            best = i;
            bestT = t;
        }
    }
    return best;
}

// "vr_lagcomp_punch <x y z> <x y z> <time> <back>": as vr_lagcomp_shot, a test punch's segment (B8c: melee's sweeps run
// in the same scope, QC lagcompbegin) -> "mp_lagcomp_punch:".
void punch_f()
{
    if(cmd_source != src_client || !sv.active || !host_client || !host_client->edict || Cmd_Argc() != 9)
    {
        return;
    }
    glm::vec3 from, to;
    for(int i = 0; i < 3; i++)
    {
        from[i] = Q_atof(Cmd_Argv(1 + i));
        to[i] = Q_atof(Cmd_Argv(4 + i));
    }
    edict_t* self = host_client->edict;
    const int slot = static_cast<int>(host_client - svs.clients);
    if(slot < 0 || slot >= MAX_SCOREBOARD)
    {
        return;
    }
    const Seen moved = seen[slot];
    const int plain = punchMeets(self, from, to);
    seen[slot] = Seen{true, static_cast<double>(Q_atof(Cmd_Argv(7))), za::clamp(static_cast<double>(Q_atof(Cmd_Argv(8))), 0.0, 1.0)};
    const bool back = begin(self, 1.0);
    const int then = punchMeets(self, from, to);
    end();
    seen[slot] = moved;
    Con_Printf("mp_lagcomp_punch: slot=%d plain=%d rewound=%d back=%d back_ms=%.1f\n", slot, plain, then, back ? 1 : 0,
        back ? saved.backMs : 0.0);
}

void shot_f()
{
    if(cmd_source != src_client || !sv.active || !host_client || !host_client->edict || Cmd_Argc() != 9)
    {
        return;
    }
    vec3_t from, to;
    for(int i = 0; i < 3; i++)
    {
        from[i] = Q_atof(Cmd_Argv(1 + i));
        to[i] = Q_atof(Cmd_Argv(4 + i));
    }
    edict_t* self = host_client->edict;
    const int slot = static_cast<int>(host_client - svs.clients);
    if(slot < 0 || slot >= MAX_SCOREBOARD)
    {
        return;
    }
    const Seen moved = seen[slot];
    const double moveMs = moved.valid ? (qcvm->time - (moved.time - moved.back)) * 1000.0 : 0.0;
    const int type = MOVE_NORMAL | MOVE_HITMODEL;
    const trace_t plain = SV_Move(from, vec3_origin, vec3_origin, to, type, self);
    seen[slot] = Seen{true, static_cast<double>(Q_atof(Cmd_Argv(7))), za::clamp(static_cast<double>(Q_atof(Cmd_Argv(8))), 0.0, 1.0)};
    const bool back = begin(self, 1.0); // (his command comes later than his move would: at most 1 s back, not the cvar's)
    const trace_t then = SV_Move(from, vec3_origin, vec3_origin, to, type, self);
    end();
    seen[slot] = moved;
    Con_Printf("mp_lagcomp_shot: slot=%d plain=%d rewound=%d back=%d back_ms=%.1f move_ms=%.1f\n", slot,
        plain.ent ? NUM_FOR_EDICT(plain.ent) : 0, then.ent ? NUM_FOR_EDICT(then.ent) : 0, back ? 1 : 0,
        back ? saved.backMs : 0.0, moveMs);
}

// "vr_mp_punch <entity>": on a remote server, a test punch (vr_lagcomp_punch): a fist's 32-unit line ending 8 units
// into entity's middle as drawn now, from your eyes' side.
void punchAt_f()
{
    if(Cmd_Argc() != 2 || cls.state != ca_connected || cls.signon != SIGNONS)
    {
        Con_Printf("vr_mp_punch <entity>: a test punch at it as drawn (the server reports what it met, lag compensated)\n");
        return;
    }
    const int ent = Q_atoi(Cmd_Argv(1));
    if(ent <= 0 || ent >= cl.num_entities || cl.viewentity <= 0)
    {
        return;
    }
    const float* me = cl_entities[cl.viewentity].origin;
    const float* at = cl_entities[ent].origin;
    vec3_t dir{at[0] - me[0], at[1] - me[1], 0.f};
    VectorNormalize(dir);
    const vec3_t mid{at[0], at[1], at[2] + 8.f};
    const double back = netsnap::interpActive() ? za::clamp(cl.mtime[0] - netsnap::interpTime(), 0.0, 1.0) : 0.0;
    char text[200];
    const int n = q_snprintf(text, sizeof(text), "vr_lagcomp_punch %.2f %.2f %.2f %.2f %.2f %.2f %.4f %.4f",
        mid[0] - dir[0] * 24.f, mid[1] - dir[1] * 24.f, mid[2], mid[0] + dir[0] * 8.f, mid[1] + dir[1] * 8.f, mid[2],
        cl.mtime[0], back);
    if(n > 0 && n < static_cast<int>(sizeof(text)) && client::roomForCommand(n))
    {
        MSG_WriteByte(&cls.message, clc_stringcmd);
        MSG_WriteString(&cls.message, text);
        Con_Printf("mp_punch: ent=%d at=%.1f %.1f %.1f\n", ent, at[0], at[1], at[2]);
    }
}

// "vr_mp_shoot <entity>": on a remote server, a test shot (vr_lagcomp_shot) from your eyes at entity's middle as drawn now.
void shoot_f()
{
    if(Cmd_Argc() != 2 || cls.state != ca_connected || cls.signon != SIGNONS)
    {
        Con_Printf("vr_mp_shoot <entity>: a test shot at it as drawn (the server reports what it hit, lag compensated)\n");
        return;
    }
    const int ent = Q_atoi(Cmd_Argv(1));
    if(ent <= 0 || ent >= cl.num_entities || cl.viewentity <= 0)
    {
        return;
    }
    const float* me = cl_entities[cl.viewentity].origin;
    const float* at = cl_entities[ent].origin;
    vec3_t from{me[0], me[1], me[2] + 22.f}, dir{at[0] - from[0], at[1] - from[1], at[2] - from[2]};
    VectorNormalize(dir);
    const double back = netsnap::interpActive() ? za::clamp(cl.mtime[0] - netsnap::interpTime(), 0.0, 1.0) : 0.0;
    char text[200];
    const int n = q_snprintf(text, sizeof(text), "vr_lagcomp_shot %.2f %.2f %.2f %.2f %.2f %.2f %.4f %.4f", from[0], from[1],
        from[2], from[0] + dir[0] * 2048.f, from[1] + dir[1] * 2048.f, from[2] + dir[2] * 2048.f, cl.mtime[0], back);
    if(n > 0 && n < static_cast<int>(sizeof(text)) && client::roomForCommand(n))
    {
        MSG_WriteByte(&cls.message, clc_stringcmd);
        MSG_WriteString(&cls.message, text);
        Con_Printf("mp_shoot: ent=%d at=%.1f %.1f %.1f\n", ent, at[0], at[1], at[2]);
    }
}

} // namespace

void init()
{
    Cmd_AddCommand_ClientCommand("vr_lagcomp_shot", shot_f);
    Cmd_AddCommand_ClientCommand("vr_lagcomp_punch", punch_f);
    Cmd_AddCommand("vr_mp_shoot", shoot_f);
    Cmd_AddCommand("vr_mp_punch", punchAt_f);
}

// lagcompbegin(entity e): QC's scope (B8c: melee's sweeps, a bash's find, a headbutt's, a player's parry of another's
// blow), as a traceline's: 1 when e is a remote client and the others went back where he saw them (lagcompend after).
void PF_lagcompbegin()
{
    G_FLOAT(OFS_RETURN) = begin(G_EDICT(OFS_PARM0)) ? 1.f : 0.f;
}

void PF_lagcompend()
{
    end();
}

} // namespace qvr::lagcomp

extern "C" int VR_LagCompBegin(edict_t* shooter)
{
    return qvr::lagcomp::begin(shooter) ? 1 : 0;
}

extern "C" void VR_LagCompEnd(void)
{
    qvr::lagcomp::end();
}
