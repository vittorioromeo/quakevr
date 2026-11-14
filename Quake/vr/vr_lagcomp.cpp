// vr_lagcomp.cpp -- lag compensation (vr_lagcomp.hpp; MP_PLAN.md, B8b).

#include "vr_lagcomp.hpp"

#include "vr_api.h"
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_hitmodel_internal.hpp"
#include "vr_netmove.hpp"
#include "vr_netsnap.hpp"

#include "Zancle/Container/Vector.hpp"
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

// begin's: each target moved, as it was before, to put back.
struct Saved
{
    bool on{false};
    za::Vector<Entry> of;
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
    SV_LinkEdict(e, false);
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
        Entry now_;
        take(e, now_);
        saved.of.pushBack(now_);
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
    for(const Entry& en : saved.of)
    {
        put(EDICT_NUM(en.num), en);
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
    Cmd_AddCommand("vr_mp_shoot", shoot_f);
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
