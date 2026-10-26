// vr_rigseq.cpp -- the Rig Studio's animations in the game: see vr_rigseq.hpp.

#include "vr_rigseq.hpp"
#include "vr_agarms.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rigdata.hpp"
#include "vr_rigpose.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string.h>

extern "C" int PR_MakeTempString(const char* val); // pr_cmds.c

using namespace qvr;

namespace
{

// A sequence drawn on a client entity (QVR_SVC_RIGSEQ; the client's thread).
struct ClientPlay
{
    int ent{0};
    za::String name;
    double start{0.0};
    double stopAt{-1.0}; // its blend out began (the server stopped it); -1 playing
};

// The studio's subject this frame (vr_rigstudio.cpp: set each frame it is open).
struct StudioPose
{
    int ent{0};
    int numBones{0};
    bool arms{false};
    rigpose::Rigid F[ragdoll::maxBones];
};

// A sequence played on a server edict (the server's frame).
struct ServerPlay
{
    int ent{0};
    const qmodel_t* model{nullptr};
    za::String name;
    double start{0.0};
    float fps{10.f};
    int length{1};
    bool loop{false};
    bool hold{true};
    float heldFrame{0.f};
    int lastFrame{-1}; // the last whole frame whose events were sent
};

// QC's side, found by name each time the progs change: the fields and the event function.
struct QcSide
{
    const void* progs{nullptr};
    int req{-1}, len{-1}, frame{-1}, t0{-1};
    func_t event{0};
};

struct RigSeqState
{
    za::Vector<ClientPlay> client;
    za::Vector<ServerPlay> server;
    StudioPose studio;
    QcSide qc;
};
RigSeqState rs;

struct SvVm
{
    qcvm_t* old{nullptr};
    SvVm() { PR_PushQCVM(&sv.qcvm, &old); }
    ~SvVm() { PR_PopQCVM(old); }
};

[[nodiscard]] float smooth01(float x)
{
    const float t = za::clamp(x, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

[[nodiscard]] int numberOf(const entity_t* e)
{
    const int n = static_cast<int>(e - cl_entities);
    return (cl_entities && n > 0 && n < cl_max_edicts) ? n : -1;
}

[[nodiscard]] const rigdata::Sequence* findSeq(const rigdata::ModelData* m, const char* name)
{
    if(!m)
    {
        return nullptr;
    }
    for(const rigdata::Sequence& s : m->seqs)
    {
        if(!q_strcasecmp(s.name.cStr(), name))
        {
            return &s;
        }
    }
    return nullptr;
}

[[nodiscard]] ClientPlay* clientPlay(int ent)
{
    for(ClientPlay& p : rs.client)
    {
        if(p.ent == ent)
        {
            return &p;
        }
    }
    return nullptr;
}

// A play's weight now (its blends) and its frame; false: over (blended out).
bool playNow(const ClientPlay& p, const rigdata::Sequence& s, float& frame, float& w)
{
    const double now = cl.time;
    const float t = static_cast<float>(now - p.start);
    const float len = rigdata::seconds(s);
    float ft = t;
    if(s.loop)
    {
        ft = t - za::floor(t / len) * len;
    }
    else
    {
        ft = za::clamp(t, 0.f, len);
    }
    frame = ft * s.fps;
    if(!s.loop)
    {
        frame = za::min(frame, static_cast<float>(za::max(s.length - 1, 0)));
    }
    w = s.blendIn > 0.f ? smooth01(t / s.blendIn) : 1.f;
    if(p.stopAt >= 0.0)
    {
        const float out = static_cast<float>(now - p.stopAt);
        if(out >= s.blendOut)
        {
            return false;
        }
        w *= s.blendOut > 0.f ? 1.f - smooth01(out / s.blendOut) : 0.f;
    }
    return true;
}

// The frame (in its group, from 0) the poses drawn are at, and the group's name.
bool groupTime(const aliashdr_t* hdr, int pose1, int pose2, float blend, char* group, int size, float& t)
{
    const int f1 = rigdata::frameOfPose(hdr, pose1), f2 = rigdata::frameOfPose(hdr, pose2);
    rigdata::AnimGroup g1, g2;
    int i1 = 0, i2 = 0;
    const bool h1 = f1 >= 0 && rigdata::groupOfFrame(hdr, f1, g1, i1);
    const bool h2 = f2 >= 0 && rigdata::groupOfFrame(hdr, f2, g2, i2);
    if(!h1 && !h2)
    {
        return false;
    }
    if(h1 && h2 && !strcmp(g1.name, g2.name) && i2 == i1 + 1)
    {
        q_strlcpy(group, g1.name, static_cast<size_t>(size));
        t = static_cast<float>(i1) + blend;
        return true;
    }
    const bool second = !h1 || (h2 && blend >= 0.5f);
    q_strlcpy(group, second ? g2.name : g1.name, static_cast<size_t>(size));
    t = static_cast<float>(second ? i2 : i1);
    return true;
}

// ---- The server ----

void resolveQc()
{
    const void* progs = sv.qcvm.progs;
    if(progs == rs.qc.progs)
    {
        return;
    }
    rs.qc = QcSide{};
    rs.qc.progs = progs;
    rs.server.clear();
    if(!progs)
    {
        return;
    }
    rs.qc.req = ED_FindFieldOffset("vr_rigseq");
    rs.qc.len = ED_FindFieldOffset("vr_rigseq_len");
    rs.qc.frame = ED_FindFieldOffset("vr_rigseq_frame");
    rs.qc.t0 = ED_FindFieldOffset("vr_rigseq_t0");
    rs.qc.event = progs::findFunction("VR_RigSeq_Event");
}

void sendStart(const ServerPlay& p)
{
    if(sv.reliable_datagram.cursize + 64 > sv.reliable_datagram.maxsize)
    {
        return;
    }
    MSG_WriteByte(&sv.reliable_datagram, protocol::svc_quakevr);
    MSG_WriteByte(&sv.reliable_datagram, protocol::QVR_SVC_RIGSEQ);
    MSG_WriteShort(&sv.reliable_datagram, p.ent);
    MSG_WriteByte(&sv.reliable_datagram, 1);
    MSG_WriteString(&sv.reliable_datagram, p.name.cStr());
    MSG_WriteFloat(&sv.reliable_datagram, static_cast<float>(p.start));
}

void sendStop(int ent)
{
    if(sv.reliable_datagram.cursize + 16 > sv.reliable_datagram.maxsize)
    {
        return;
    }
    MSG_WriteByte(&sv.reliable_datagram, protocol::svc_quakevr);
    MSG_WriteByte(&sv.reliable_datagram, protocol::QVR_SVC_RIGSEQ);
    MSG_WriteShort(&sv.reliable_datagram, ent);
    MSG_WriteByte(&sv.reliable_datagram, 0);
    MSG_WriteFloat(&sv.reliable_datagram, static_cast<float>(sv.qcvm.time));
}

// QC's VR_RigSeq_Event(seq, ev, frame), self the monster.
void callEvent(edict_t* e, const char* seq, const char* ev, float frame)
{
    if(vr_rigseq_debug.value)
    {
        Con_Printf("rigseq: entity %d: %s: event %s at frame %g (time %.3f)\n", NUM_FOR_EDICT(e), seq, ev, frame, sv.qcvm.time);
    }
    if(!rs.qc.event)
    {
        return;
    }
    const int oldSelf = pr_global_struct->self, oldOther = pr_global_struct->other;
    pr_global_struct->time = sv.qcvm.time;
    pr_global_struct->self = EDICT_TO_PROG(e);
    pr_global_struct->other = EDICT_TO_PROG(sv.qcvm.edicts);
    G_INT(OFS_PARM0) = PR_MakeTempString(seq);
    G_INT(OFS_PARM1) = PR_MakeTempString(ev);
    G_FLOAT(OFS_PARM2) = frame;
    PR_ExecuteProgram(rs.qc.event);
    pr_global_struct->self = oldSelf;
    pr_global_struct->other = oldOther;
}

void setFields(edict_t* e, float len, float frame)
{
    progs::setFieldFloat(e, rs.qc.len, len);
    progs::setFieldFloat(e, rs.qc.frame, frame);
}

[[nodiscard]] za::SizeT serverIndex(int ent)
{
    for(za::SizeT i = 0; i < rs.server.size(); i++)
    {
        if(rs.server[i].ent == ent)
        {
            return i;
        }
    }
    return rs.server.size();
}

void stopOn(edict_t* e, const char* why)
{
    const int ent = NUM_FOR_EDICT(e);
    const za::SizeT i = serverIndex(ent);
    if(i == rs.server.size())
    {
        return;
    }
    const za::String name = rs.server[i].name;
    rs.server.erase(rs.server.begin() + static_cast<za::PtrDiffT>(i));
    sendStop(ent);
    setFields(e, 0.f, -1.f);
    callEvent(e, name.cStr(), why, -1.f);
}

void startOn(edict_t* e, const char* name)
{
    const int ent = NUM_FOR_EDICT(e);
    const int mi = static_cast<int>(e->v.modelindex);
    const qmodel_t* model = mi > 0 && mi < MAX_MODELS ? sv.models[mi] : nullptr;
    const rigdata::Sequence* s = model ? findSeq(rigdata::of(model), name) : nullptr;
    if(!s)
    {
        setFields(e, -1.f, -1.f);
        if(vr_rigseq_debug.value)
        {
            Con_Printf("rigseq: entity %d (%s): no sequence %s\n", ent, model ? model->name : "no model", name);
        }
        return;
    }
    const za::SizeT old = serverIndex(ent);
    if(old != rs.server.size())
    {
        rs.server.erase(rs.server.begin() + static_cast<za::PtrDiffT>(old)); // (another one on it: replaced)
    }
    ServerPlay p;
    p.ent = ent;
    p.model = model;
    p.name = s->name;
    // (from when QC asked, if it said: its events' times then exact whatever the server frame it was taken in)
    const float asked = progs::fieldFloatOr(e, rs.qc.t0, 0.f);
    p.start = asked > 0.f && asked <= sv.qcvm.time && asked > sv.qcvm.time - 0.5 ? static_cast<double>(asked) : sv.qcvm.time;
    p.fps = s->fps;
    p.length = za::max(s->length, 1);
    p.loop = s->loop;
    p.hold = s->hold;
    if(!s->base.empty() && s->hold)
    {
        const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
        const int f = hdr ? rigdata::frameOfName(hdr, s->base.cStr()) : -1;
        if(f >= 0)
        {
            e->v.frame = static_cast<float>(f);
        }
    }
    p.heldFrame = e->v.frame;
    rs.server.pushBack(p);
    sendStart(p);
    setFields(e, rigdata::seconds(*s), 0.f);
    if(vr_rigseq_debug.value)
    {
        Con_Printf("rigseq: entity %d: %s started (%d frames at %g fps, %.2f s%s)\n", ent, s->name.cStr(), s->length, s->fps,
            rigdata::seconds(*s), s->loop ? ", looped" : "");
    }
}

// Each play's frames since the last: its events sent, its end.
void advance()
{
    for(za::SizeT i = 0; i < rs.server.size();)
    {
        ServerPlay p = rs.server[i]; // (a copy: the events' QC may start or stop others)
        edict_t* e = p.ent > 0 && p.ent < sv.qcvm.num_edicts ? EDICT_NUM(p.ent) : nullptr;
        const int mi = e ? static_cast<int>(e->v.modelindex) : 0;
        if(!e || e->free || mi <= 0 || mi >= MAX_MODELS || sv.models[mi] != p.model)
        {
            rs.server.erase(rs.server.begin() + static_cast<za::PtrDiffT>(i));
            sendStop(p.ent);
            continue;
        }
        if(p.hold && e->v.frame != p.heldFrame)
        {
            stopOn(e, "stopped"); // (its own animation took over: a pain, a death)
            continue;
        }
        const rigdata::Sequence* s = findSeq(rigdata::of(p.model), p.name.cStr());
        const float fr = static_cast<float>(sv.qcvm.time - p.start) * p.fps;
        const int whole = static_cast<int>(za::floor(fr + 1e-3f)); // (0.3 s at 10 fps: frame 3, not 2.9999's 2)
        const int last = p.loop ? whole : za::min(whole, p.length - 1);
        for(int k = p.lastFrame + 1; s && k <= last; k++)
        {
            const int at = k % p.length;
            for(const rigdata::Event& ev : s->events)
            {
                if(ev.frame == at)
                {
                    callEvent(e, p.name.cStr(), ev.name.cStr(), static_cast<float>(at));
                }
            }
        }
        const za::SizeT j = serverIndex(p.ent);
        if(j == rs.server.size() || rs.server[j].start != p.start)
        {
            continue; // (stopped or replaced by its events' QC)
        }
        rs.server[j].lastFrame = za::max(p.lastFrame, last);
        if(!p.loop && fr + 1e-3f >= static_cast<float>(p.length))
        {
            rs.server.erase(rs.server.begin() + static_cast<za::PtrDiffT>(j));
            sendStop(p.ent);
            setFields(e, 0.f, -1.f);
            callEvent(e, p.name.cStr(), "end", static_cast<float>(p.length));
            continue;
        }
        setFields(e, s ? rigdata::seconds(*s) : 0.f, static_cast<float>(p.loop ? whole % p.length : last));
        i = j + 1;
    }
}

[[nodiscard]] edict_t* entityArg(const char* arg)
{
    if(arg && arg[0] >= '0' && arg[0] <= '9')
    {
        const int n = Q_atoi(arg);
        return n > 0 && n < sv.qcvm.num_edicts && !EDICT_NUM(n)->free ? EDICT_NUM(n) : nullptr;
    }
    // nearest: the monster with a rig nearest the player
    edict_t* p = EDICT_NUM(1);
    edict_t* best = nullptr;
    float bestD = 1e30f;
    for(int i = 2; i < sv.qcvm.num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        const int mi = static_cast<int>(e->v.modelindex);
        if(e->free || e->v.health <= 0.f || mi <= 0 || mi >= MAX_MODELS || !sv.models[mi] || !ragdoll::eligible(sv.models[mi]))
        {
            continue;
        }
        const glm::vec3 d{e->v.origin[0] - p->v.origin[0], e->v.origin[1] - p->v.origin[1], e->v.origin[2] - p->v.origin[2]};
        const float l = glm::length(d);
        if(l < bestD)
        {
            bestD = l;
            best = e;
        }
    }
    return best;
}

void play_f()
{
    if(Cmd_Argc() < 2 || !sv.active)
    {
        Con_Printf("usage: vr_rigseq_play <sequence> [entity | nearest] (a running game; as QC's .vr_rigseq)\n");
        return;
    }
    const SvVm vm;
    resolveQc();
    edict_t* e = entityArg(Cmd_Argc() > 2 ? Cmd_Argv(2) : "nearest");
    if(!e)
    {
        Con_Printf("rigseq: no such monster\n");
        return;
    }
    startOn(e, Cmd_Argv(1));
}

void stop_f()
{
    if(!sv.active)
    {
        return;
    }
    const SvVm vm;
    resolveQc();
    edict_t* e = entityArg(Cmd_Argc() > 1 ? Cmd_Argv(1) : "nearest");
    if(e)
    {
        stopOn(e, "stopped");
    }
}

void status_f()
{
    Con_Printf("rigseq: %d on the server, %d drawn; the studio's subject %d; any animation loaded: %s\n",
        static_cast<int>(rs.server.size()), static_cast<int>(rs.client.size()), rs.studio.ent, rigdata::anyAnimation() ? "yes" : "no");
    for(const ServerPlay& p : rs.server)
    {
        Con_Printf("rigseq:   server: entity %d %s from %.2f, frame events sent to %d\n", p.ent, p.name.cStr(), p.start, p.lastFrame);
    }
    for(const ClientPlay& p : rs.client)
    {
        Con_Printf("rigseq:   client: entity %d %s from %.2f%s\n", p.ent, p.name.cStr(), p.start, p.stopAt >= 0.0 ? " (blending out)" : "");
    }
}

} // namespace

namespace qvr::rigseq
{

bool wants(const entity_t* e)
{
    if(rs.studio.ent == 0 && rs.client.empty() && !rigdata::anyAnimation())
    {
        return false;
    }
    if(vr_rigseq.value == 0.f && rs.studio.ent == 0)
    {
        return false;
    }
    const int n = numberOf(e);
    if(n <= 0)
    {
        return false;
    }
    if(n == rs.studio.ent || clientPlay(n))
    {
        return true;
    }
    if(!rigdata::anyAnimation() || vr_rigseq.value == 0.f)
    {
        return false;
    }
    const rigdata::ModelData* m = rigdata::of(e->model);
    return m && !m->overrides.empty();
}

bool bones(const entity_t* e, const ragdoll::Rig& rig, const aliashdr_t* hdr, int pose1, int pose2, float blend,
    const rigpose::Rigid* posed, rigpose::Rigid* F, bool* arms)
{
    if(arms)
    {
        *arms = false;
    }
    const int n = numberOf(e);
    if(n <= 0)
    {
        return false;
    }
    if(n == rs.studio.ent && rs.studio.numBones == rig.numBones)
    {
        for(int b = 0; b < rig.numBones; b++)
        {
            F[b] = rs.studio.F[b];
        }
        if(arms)
        {
            *arms = rs.studio.arms;
        }
        return true;
    }
    if(vr_rigseq.value == 0.f)
    {
        return false;
    }
    ClientPlay* play = clientPlay(n);
    if(!play && !rigdata::anyAnimation())
    {
        return false;
    }
    const rigdata::ModelData* m = rigdata::of(e->model);
    if(!m)
    {
        return false;
    }
    glm::vec3 pivots[ragdoll::maxBones];
    rigdata::restOf(rig, nullptr, pivots, nullptr);
    // The frame's locals.
    rigpose::Rigid L[ragdoll::maxBones];
    for(int b = 0; b < rig.numBones; b++)
    {
        const int p = rig.bones[b].parent;
        L[b] = (p >= 0 && p < b) ? rigdata::compose(rigdata::inverse(posed[p]), posed[b]) : posed[b];
    }
    bool any = false;
    // Its own animation's override tracks.
    if(!m->overrides.empty())
    {
        char group[16];
        float t = 0.f;
        if(groupTime(hdr, pose1, pose2, blend, group, sizeof(group), t))
        {
            for(const rigdata::AnimOverride& o : m->overrides)
            {
                if(q_strcasecmp(o.anim.cStr(), group))
                {
                    continue;
                }
                for(const rigdata::Track& tr : o.tracks)
                {
                    rigdata::Local d;
                    if(tr.bone >= 0 && tr.bone < rig.numBones && rigdata::sample(tr, t, 0.f, d))
                    {
                        L[tr.bone] = rigdata::compose(L[tr.bone], rigdata::toRigid(d, pivots[tr.bone]));
                        any = true;
                        // (an arm's track: an armed humanoid's arms follow the animation's arms with it, their hands
                        // where its forearms are, not on the weapons' grips as the game poses them)
                        const char* bn = rig.bones[tr.bone].name;
                        if(arms && (!strncmp(bn, "upperarm", 8) || !strncmp(bn, "forearm", 7)))
                        {
                            *arms = true;
                        }
                    }
                }
            }
        }
    }
    // A sequence played on it.
    if(play)
    {
        const rigdata::Sequence* s = findSeq(m, play->name.cStr());
        float frame = 0.f, w = 0.f;
        if(!s || !playNow(*play, *s, frame, w))
        {
            const za::SizeT i = static_cast<za::SizeT>(play - rs.client.data());
            rs.client.erase(rs.client.begin() + static_cast<za::PtrDiffT>(i)); // (over, or not known here)
        }
        else
        {
            for(const rigdata::Track& tr : s->tracks)
            {
                rigdata::Local l;
                if(tr.bone >= 0 && tr.bone < rig.numBones &&
                    rigdata::sample(tr, frame, s->loop ? static_cast<float>(s->length) : 0.f, l))
                {
                    L[tr.bone] = rigdata::lerp(L[tr.bone], rigdata::toRigid(l, pivots[tr.bone]), w);
                }
            }
            if(arms)
            {
                *arms = *arms || (s->arms && w > 0.f);
            }
            any = true;
        }
    }
    if(!any)
    {
        return false;
    }
    for(int b = 0; b < rig.numBones; b++)
    {
        const int p = rig.bones[b].parent;
        F[b] = (p >= 0 && p < b) ? rigdata::compose(F[p], L[b]) : L[b];
    }
    return true;
}

bool layer(const entity_t* e, const ragdoll::Rig& rig, const aliashdr_t* hdr, int pose1, int pose2, float blend,
    const rigpose::Rigid* posed, rigpose::Rigid* kick)
{
    rigpose::Rigid F[ragdoll::maxBones];
    if(!bones(e, rig, hdr, pose1, pose2, blend, posed, F, nullptr))
    {
        return false;
    }
    for(int b = 0; b < rig.numBones; b++)
    {
        kick[b] = rigdata::compose(kick[b], rigdata::compose(F[b], rigdata::inverse(posed[b])));
    }
    return true;
}

void adjustTargets(const ragdoll::Rig& rig, const agarms::Spec& spec, const rigpose::Rigid* posed, const rigpose::Rigid* kick,
    agarms::Targets& t)
{
    for(int s = 0; s < 2; s++)
    {
        const int fore = spec.fore[s], upper = spec.upper[s];
        if(fore < 0 || upper < 0 || fore >= rig.numBones || upper >= rig.numBones)
        {
            continue;
        }
        const rigpose::Rigid H = kick ? rigdata::compose(kick[fore], posed[fore]) : posed[fore];
        const rigpose::Rigid U = kick ? rigdata::compose(kick[upper], posed[upper]) : posed[upper];
        t.fk[s] = true;
        t.fkHand[s] = H;
        const glm::vec3 elbow = rigpose::apply(H, rig.bones[fore].pivot);
        const glm::vec3 shoulder = rigpose::apply(U, rig.bones[upper].pivot);
        const glm::vec3 d = elbow - shoulder;
        t.pole[s] = glm::length(d) > 1e-3f ? glm::normalize(d) : glm::vec3{0.f};
    }
    t.second = false;
    t.cup = false;
    t.reaching = false;
}

void setStudio(int ent, const rigpose::Rigid* F, int numBones, bool arms)
{
    rs.studio.ent = ent;
    rs.studio.numBones = za::clamp(numBones, 0, ragdoll::maxBones);
    rs.studio.arms = arms;
    for(int b = 0; F && b < rs.studio.numBones; b++)
    {
        rs.studio.F[b] = F[b];
    }
}

int studioEnt()
{
    return rs.studio.ent;
}

void parseMessage()
{
    const int ent = MSG_ReadShort();
    const int kind = MSG_ReadByte();
    if(kind == 1)
    {
        const za::String name{MSG_ReadString()};
        const float start = MSG_ReadFloat();
        ClientPlay* p = clientPlay(ent);
        if(!p)
        {
            rs.client.pushBack(ClientPlay{});
            p = &rs.client.back();
        }
        p->ent = ent;
        p->name = name;
        p->start = start;
        p->stopAt = -1.0;
        return;
    }
    const float at = MSG_ReadFloat();
    if(ClientPlay* p = clientPlay(ent); p && p->stopAt < 0.0)
    {
        p->stopAt = at;
    }
}

void clientClear()
{
    rs.client.clear();
}

void serverFrame()
{
    if(!sv.active)
    {
        return;
    }
    const SvVm vm;
    resolveQc();
    if(rs.qc.req < 0)
    {
        return; // (the progs have no such field: nothing asks)
    }
    for(int i = 1; i < sv.qcvm.num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free)
        {
            continue;
        }
        int& req = progs::fieldInt(e, rs.qc.req);
        if(!req)
        {
            continue;
        }
        const za::String name{PR_GetString(req)};
        req = 0;
        if(name.empty() || !strcmp(name.cStr(), "-"))
        {
            stopOn(e, "stopped");
        }
        else if(vr_rigseq.value != 0.f)
        {
            startOn(e, name.cStr());
        }
        else
        {
            setFields(e, -1.f, -1.f);
        }
    }
    if(!rs.server.empty())
    {
        advance();
    }
}

void serverClear()
{
    rs.server.clear();
    rs.qc = QcSide{};
}

bool playingOn(int ent, const char*& name, float& time, float& weight)
{
    ClientPlay* p = clientPlay(ent);
    name = "";
    time = 0.f;
    weight = 0.f;
    if(!p)
    {
        return false;
    }
    name = p->name.cStr();
    time = static_cast<float>(cl.time - p->start);
    weight = p->stopAt >= 0.0 ? 0.5f : 1.f;
    return true;
}

void registerCommands()
{
    Cmd_AddCommand("vr_rigseq_play", play_f);
    Cmd_AddCommand("vr_rigseq_stop", stop_f);
    Cmd_AddCommand("vr_rigseq_status", status_f);
}

} // namespace qvr::rigseq
