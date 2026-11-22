// vr_netragdoll.cpp -- the server's ragdolls drawn by a remote client as the server has them (see the header; ragsync).

#include "vr_netragdoll.hpp"
#include "vr_api_render.h"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_mpavatar.hpp"
#include "vr_netsnap.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_ragdoll.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"

#include <cmath>
#include <cstring>

#include <glm/gtc/type_ptr.hpp>

namespace qvr::netrag
{
namespace
{

constexpr float posScale = 8.f;              // the parts' places: eighths of a unit
constexpr float quatMost = 0.70710678f;      // the smallest three's range (the largest left out)
constexpr float quatSteps = 16383.f;         // ... in 15 bits each
constexpr int ringLen = 40;                  // poses kept per ragdoll on the client: 0.55 s at 72 Hz, past the
                                             // interpolation clock's furthest lag (netsnap::interpReach). (mpopen: 24, a
                                             // third of a second, covered the longest delay but not a hitch's catch-up:
                                             // the clock before the oldest pose drew the parts held there, 3-4 ticks
                                             // ahead, mp_ragsync's parts p95 9.3 units in 1 run of 5)
static_assert(ringLen / 72.0 >= netsnap::interpReach, "the ragdolls' pose ring must outlast the clock's lag");
constexpr int recordMost = 18 + 18 + 24 + 12 * ragdoll::maxBones; // QVR_SVC_RAGPOSE's bytes at most (24: his hands)

// ---- The message ---------------------------------------------------------------------------------------------------

void writeQuat(sizebuf_t* m, const glm::quat& q)
{
    const float c[4]{q.w, q.x, q.y, q.z};
    int big = 0;
    for(int i = 1; i < 4; i++)
    {
        big = std::fabs(c[i]) > std::fabs(c[big]) ? i : big;
    }
    const float sign = c[big] < 0.f ? -1.f : 1.f;
    za::U64 bits = static_cast<za::U64>(big);
    int k = 0;
    for(int i = 0; i < 4; i++)
    {
        if(i == big)
        {
            continue;
        }
        const long v = za::clamp(std::lround(c[i] * sign / quatMost * quatSteps), -16383l, 16383l);
        bits |= (static_cast<za::U64>(v) & 0x7fffu) << (2 + 15 * k);
        k++;
    }
    for(int b = 0; b < 6; b++)
    {
        MSG_WriteByte(m, static_cast<int>((bits >> (8 * b)) & 255u));
    }
}

[[nodiscard]] glm::quat readQuat()
{
    za::U64 bits = 0;
    for(int b = 0; b < 6; b++)
    {
        bits |= static_cast<za::U64>(MSG_ReadByte() & 255) << (8 * b);
    }
    const int big = static_cast<int>(bits & 3u);
    float c[4]{};
    float sum = 0.f;
    int k = 0;
    for(int i = 0; i < 4; i++)
    {
        if(i == big)
        {
            continue;
        }
        int raw = static_cast<int>((bits >> (2 + 15 * k)) & 0x7fffu);
        raw = (raw & 0x4000) ? raw - 0x8000 : raw;
        c[i] = static_cast<float>(raw) / quatSteps * quatMost;
        sum += c[i] * c[i];
        k++;
    }
    c[big] = std::sqrt(sum < 1.f ? 1.f - sum : 0.f);
    return glm::normalize(glm::quat{c[0], c[1], c[2], c[3]});
}

// ---- Server --------------------------------------------------------------------------------------------------------

struct ServerScratch
{
    byte data[recordMost + 16];
};
ServerScratch serverScratch;

// ---- Client --------------------------------------------------------------------------------------------------------

struct Sample
{
    double time{0.0};
    int bodies{0};
    uint32_t cut{0};
    bool bleeds{false};
    float scale{1.f};
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    int heldBy{0}; // (ragsync2: his own hands on it, and where the server had them)
    glm::vec3 hand[2]{glm::vec3{0.f}, glm::vec3{0.f}};
};
struct Ring
{
    int num{0}; // (0: free)
    qmodel_t* model{nullptr};
    const ragdoll::Rig* rig{nullptr};
    int n{0}, head{0}; // its samples: s[(head + i) % ringLen], i 0 the oldest
    za::U32 seen{0};
    Sample s[ringLen];

    [[nodiscard]] const Sample& at(int i) const { return s[(head + i) % ringLen]; }
};
struct ClientRags
{
    za::Vector<Ring> rings;
    za::U32 stamp{0};
    long long parsed{0}, rejected{0}; // records read, and those whose rig did not fit (drawn animated)
};
ClientRags clientRags;

[[nodiscard]] Ring* ringOf(int num, bool make)
{
    Ring* free = nullptr;
    for(Ring& r : clientRags.rings)
    {
        if(r.num == num)
        {
            return &r;
        }
        free = !free && r.num == 0 ? &r : free;
    }
    if(!make)
    {
        return nullptr;
    }
    if(!free)
    {
        clientRags.rings.emplaceBack();
        free = &clientRags.rings.back();
    }
    *free = Ring{};
    free->num = num;
    return free;
}

void dropRing(Ring& r)
{
    ragdoll::unpublish(r.num);
    r = Ring{};
}

[[nodiscard]] ragdoll::NetPose poseOf(const Ring& r, const Sample& s)
{
    ragdoll::NetPose p;
    p.rig = r.rig;
    p.bodies = s.bodies;
    p.cut = s.cut;
    p.bleeds = s.bleeds;
    p.scale = s.scale;
    p.time = s.time;
    p.rot = s.rot.data();
    p.pos = s.pos.data();
    p.heldBy = s.heldBy;
    p.hand[0] = s.hand[0];
    p.hand[1] = s.hand[1];
    return p;
}

// ---- vr_mp_bodylog ---------------------------------------------------------------------------------------------------

struct BodyLog
{
    double seconds{0.0}; // (0: off) the log's length in its side's game time ...
    double start{-1.0};  // ... from its first frame's
    float radius{400.f};
    double lastClock{-1.0};
};
BodyLog bodyLog;

void printPart(const char* side, double t, int num, int b, const glm::vec3& p, const glm::quat& q, const char* src)
{
    Con_Printf("mp_bp: side=%s t=%.4f n=%d b=%d p=%.2f,%.2f,%.2f q=%.5f,%.5f,%.5f,%.5f src=%s\n", side, t, num, b, p.x, p.y,
        p.z, q.w, q.x, q.y, q.z, src);
}

// The ragdoll pose `p` (world: a point p_rest at rot * (scale * p_rest) + pos): each part's joint (its pivot) and turn.
void printPose(const char* side, double t, int num, const ragdoll::Rig& rig, int bodies, const glm::quat* rot,
    const glm::vec3* pos, float scale)
{
    for(int b = 0; b < bodies && b < rig.numBones; b++)
    {
        printPart(side, t, num, b, rot[b] * (scale * rig.bones[b].pivot) + pos[b], rot[b], "rag");
    }
}

void bodylog_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_mp_bodylog <seconds> [radius]: the physics things near you each tick of game time (mp_bl, "
                   "mp_bp lines; a server's as it has them, a remote client's as it draws them)\n");
        return;
    }
    bodyLog.seconds = Q_atof(Cmd_Argv(1));
    bodyLog.start = -1.0;
    bodyLog.radius = Cmd_Argc() > 2 ? static_cast<float>(Q_atof(Cmd_Argv(2))) : 400.f;
    bodyLog.lastClock = -1.0;
    Con_Printf("mp_bodylog: seconds=%.1f radius=%.0f side=%s\n", bodyLog.seconds, bodyLog.radius, sv.active ? "sv" : "cl");
}

[[nodiscard]] bool near(const float* a, const float* b, float r)
{
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz <= r * r;
}

void logServer()
{
    const double t = qcvm->time;
    const edict_t* me = progs::playerEdict();
    if(!me || t <= bodyLog.lastClock)
    {
        return; // (once a server frame)
    }
    bodyLog.lastClock = t;
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts; num++)
    {
        const edict_t* e = EDICT_NUM(num);
        const int mi = static_cast<int>(e->v.modelindex);
        if(e->free || mi <= 0 || mi >= MAX_MODELS || !sv.model_precache[mi] || sv.model_precache[mi][0] == '*' ||
            !near(e->v.origin, me->v.origin, bodyLog.radius))
        {
            continue;
        }
        ragdoll::NetPose p;
        const bool rag = ragdoll::serverPose(num, p);
        Con_Printf("mp_bl: side=sv t=%.4f n=%d m=%s o=%.2f,%.2f,%.2f a=%.2f,%.2f,%.2f rag=%d\n", t, num, sv.model_precache[mi],
            e->v.origin[0], e->v.origin[1], e->v.origin[2], e->v.angles[0], e->v.angles[1], e->v.angles[2], rag ? 1 : 0);
        if(rag)
        {
            printPose("sv", p.time, num, *p.rig, p.bodies, p.rot, p.pos, p.scale); // (its step's time: the record's)
            Con_Printf("mp_blpose: n=%d t=%.4f pose=%.4f\n", num, t, p.time);
        }
    }
}

void logClient()
{
    const double t = netsnap::interpActive() ? netsnap::interpTime() : cl.time;
    if(t - bodyLog.lastClock < 1.0 / 144.0)
    {
        return;
    }
    bodyLog.lastClock = t;
    for(const Ring& r : clientRags.rings)
    {
        if(r.num != 0 && r.n > 0)
        {
            Con_Printf("mp_blring: n=%d t=%.4f oldest=%.4f newest=%.4f samples=%d interp=%d\n", r.num, t, r.at(0).time,
                r.at(r.n - 1).time, r.n, netsnap::interpActive() ? 1 : 0);
        }
    }
    const float* me = cl_entities[cl.viewentity].origin;
    for(int num = cl.maxclients + 1; num < cl.num_entities; num++)
    {
        entity_t* e = &cl_entities[num];
        if(!e->model || e->model->name[0] == '*' || e->model->type != mod_alias || !near(e->origin, me, bodyLog.radius))
        {
            continue;
        }
        const bool rag = ragdoll::published(num);
        char name[MAX_QPATH];
        q_strlcpy(name, e->model->name, sizeof(name));
        if(char* hash = strchr(name, '#'))
        {
            *hash = 0; // (its skinned copy drawn: the .mdl's name)
        }
        int way = 4, samples = 0, restarts = 0, why = 0; // (mpopen: how it was interpolated, after rag=)
        static_cast<void>(netsnap::interpWay(num, way, samples, restarts, why));
        Con_Printf("mp_bl: side=cl t=%.4f n=%d m=%s o=%.2f,%.2f,%.2f a=%.2f,%.2f,%.2f rag=%d way=%d h=%d rs=%d why=%d\n", t, num,
            name, e->origin[0], e->origin[1], e->origin[2], e->angles[0], e->angles[1], e->angles[2], rag ? 1 : 0, way, samples,
            restarts, why);
        if(rag)
        {
            for(int b = 0; b < ragdoll::maxBones; b++)
            {
                glm::quat rot;
                glm::vec3 pos;
                float scale = 1.f;
                const ragdoll::Rig* rig = nullptr;
                if(!ragdoll::drawnPart(num, b, rot, pos, scale, &rig) || !rig || b >= rig->numBones)
                {
                    break;
                }
                printPart("cl", t, num, b, rot * (scale * rig->bones[b].pivot) + pos, rot, "rag");
            }
            continue;
        }
        // Drawn animated (its frame): each rig bone as that frame places it (vr_drawnpose.cpp livePlacement's matrix).
        const ragdoll::Rig* rig = strchr(e->model->name, '#') ? nullptr : ragdoll::rigFor(e->model);
        if(!rig)
        {
            continue;
        }
        float m16[16];
        R_EntityMatrix(m16, e->origin, e->angles, e->scale);
        VR_AliasPreTransform(e, m16);
        VR_AliasPostTransform(e, m16);
        const glm::mat4 m = glm::make_mat4(m16);
        const glm::mat3 a{glm::normalize(glm::vec3{m[0]}), glm::normalize(glm::vec3{m[1]}), glm::normalize(glm::vec3{m[2]})};
        const glm::quat turn = glm::quat_cast(a);
        const int pose = ragdoll::poseOfFrame(e->model, e->frame);
        for(int b = 0; b < rig->numBones; b++)
        {
            glm::quat rot;
            glm::vec3 pos;
            ragdoll::bonePose(*rig, pose, b, rot, pos);
            const glm::vec3 pivot = glm::vec3{m * glm::vec4{rot * rig->bones[b].pivot + pos, 1.f}};
            printPart("cl", t, num, b, pivot, turn * rot, "anim");
        }
    }
}

// vr_netrag_selftest: the message written and read back (a quaternion's smallest three, the places' eighths).
void selftest_f()
{
    testkit::Tally tally;
    const auto check = [&](const char* what, bool ok, float got) {
        tally.check(ok);
        Con_Printf("netrag selftest: %s %s (%.5f)\n", what, ok ? "ok" : "FAILED", got);
    };
    byte data[64];
    float worst = 0.f;
    for(int i = 0; i < 200; i++)
    {
        const float a = static_cast<float>(i) * 0.37f, b = static_cast<float>(i) * 1.13f, c = static_cast<float>(i) * 0.71f;
        const glm::quat q = glm::normalize(glm::quat{glm::vec3{a, b, c}} * glm::quat{std::cos(a), std::sin(b), -std::sin(c), 0.3f});
        sizebuf_t m{};
        m.data = data;
        m.maxsize = sizeof(data);
        writeQuat(&m, q);
        const sizebuf_t saved = net_message;
        const int savedCount = msg_readcount;
        net_message = m;
        msg_readcount = 0;
        const glm::quat r = readQuat();
        net_message = saved;
        msg_readcount = savedCount;
        const float d = glm::degrees(2.f * std::acos(za::clamp(std::fabs(glm::dot(q, r)), 0.f, 1.f)));
        worst = d > worst ? d : worst;
    }
    check("200 turns read back within 0.02 degrees", worst < 0.02f, worst);
    tally.verdict("netrag selftest");
}

} // namespace

// ---- Server --------------------------------------------------------------------------------------------------------

bool serverWrite(int num, sizebuf_t* m)
{
    ragdoll::NetPose p;
    if(!ragdoll::serverPose(num, p) || !p.rig || p.bodies <= 0 || p.bodies > ragdoll::maxBones)
    {
        return false;
    }
    // (ragsync2) The snapshot's own player's hands holding it, and where the server has them (his sent hands, the
    // holds' targets): his client leads its drawing with his hands since (vr_ragdoll_held_local), as the listen host's.
    const int player = netsnap::serverPlayer();
    int heldBy = 0;
    glm::vec3 hand[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    for(int h = 0; h < 2 && player > 0 && player < qcvm->num_edicts; h++)
    {
        box3d::RagdollHold hold;
        if(box3d::ragdollHold(player, h, hold) && hold.num == num)
        {
            heldBy |= 1 << h;
            hand[h] = progs::fieldVec(EDICT_NUM(player), progs::handFields(h).pos);
        }
    }
    MSG_WriteByte(m, protocol::svc_quakevr);
    MSG_WriteByte(m, protocol::QVR_SVC_RAGPOSE);
    MSG_WriteShort(m, num);
    MSG_WriteByte(m, p.bodies | (p.rig->authored ? 128 : 0));
    MSG_WriteLong(m, static_cast<int>(p.cut));
    MSG_WriteByte(m, (p.bleeds ? 1 : 0) | heldBy << 1);
    MSG_WriteFloat(m, p.scale);
    MSG_WriteLong(m, static_cast<int>(std::llround(p.time * 1000.0))); // (ms: a float's would lose them in hours)
    const glm::vec3 root = p.pos[0];
    for(int k = 0; k < 3; k++)
    {
        MSG_WriteLong(m, static_cast<int>(std::lround(root[k] * posScale)));
    }
    writeQuat(m, p.rot[0]);
    for(int b = 1; b < p.bodies; b++)
    {
        for(int k = 0; k < 3; k++)
        {
            MSG_WriteShort(m, static_cast<int>(za::clamp(std::lround((p.pos[b][k] - root[k]) * posScale), -32767l, 32767l)));
        }
        writeQuat(m, p.rot[b]);
    }
    for(int h = 0; h < 2; h++)
    {
        for(int k = 0; k < 3 && (heldBy & (1 << h)); k++)
        {
            MSG_WriteLong(m, static_cast<int>(std::lround(hand[h][k] * posScale)));
        }
    }
    return true;
}

void serverRecord(int num)
{
    if(vr_net_ragdolls.value == 0.f || !ragdoll::published(num))
    {
        return;
    }
    sizebuf_t m{};
    m.data = serverScratch.data;
    m.maxsize = static_cast<int>(sizeof(serverScratch.data));
    if(serverWrite(num, &m) && !m.overflowed)
    {
        netsnap::serverRecord(netsnap::ragKey + num, m.data, m.cursize);
    }
}

// ---- Client --------------------------------------------------------------------------------------------------------

void clientParse()
{
    Sample in;
    const int num = MSG_ReadShort();
    const int nb = MSG_ReadByte();
    in.bodies = nb & 63;
    const bool authored = (nb & 128) != 0;
    in.cut = static_cast<uint32_t>(MSG_ReadLong());
    const int flags = MSG_ReadByte();
    in.bleeds = (flags & 1) != 0;
    in.heldBy = (flags >> 1) & 3;
    in.scale = MSG_ReadFloat();
    in.time = static_cast<double>(MSG_ReadLong()) / 1000.0;
    if(in.bodies <= 0 || in.bodies > ragdoll::maxBones)
    {
        msg_badread = true;
        return;
    }
    glm::vec3 root;
    for(int k = 0; k < 3; k++)
    {
        root[k] = static_cast<float>(MSG_ReadLong()) / posScale;
    }
    in.pos[0] = root;
    in.rot[0] = readQuat();
    for(int b = 1; b < in.bodies; b++)
    {
        glm::vec3 d;
        for(int k = 0; k < 3; k++)
        {
            d[k] = static_cast<float>(MSG_ReadShort()) / posScale;
        }
        in.pos[static_cast<za::SizeT>(b)] = root + d;
        in.rot[static_cast<za::SizeT>(b)] = readQuat();
    }
    for(int h = 0; h < 2; h++)
    {
        for(int k = 0; k < 3 && (in.heldBy & (1 << h)); k++)
        {
            in.hand[h][k] = static_cast<float>(MSG_ReadLong()) / posScale;
        }
    }
    if(msg_badread || num <= 0 || num >= cl_max_edicts || sv.active)
    {
        return; // (a listen host draws its own server's: never a record's)
    }
    clientRags.parsed++;
    qmodel_t* model = cl_entities[num].model;
    const ragdoll::Rig* rig = model ? (authored ? ragdoll::rigFor(model) : ragdoll::derivedRigFor(model)) : nullptr;
    rig = authored ? mpavatar::clientDollRig(rig, &cl_entities[num], in.scale) : rig; // (a dead player's doll: his proportions, as the server's)
    if(!rig || rig->authored != authored || in.bodies > rig->numBones)
    {
        clientRags.rejected++; // (its rig not this client's: drawn animated, as before)
        if(Ring* r = ringOf(num, false))
        {
            dropRing(*r);
        }
        return;
    }
    Ring* r = ringOf(num, true);
    if(r->model != model || r->rig != rig)
    {
        const int keep = r->num;
        ragdoll::unpublish(keep);
        *r = Ring{};
        r->num = keep;
        r->model = model;
        r->rig = rig;
    }
    r->seen = clientRags.stamp;
    if(r->n > 0 && !(in.time > r->at(r->n - 1).time))
    {
        return; // (the same pose: its record unchanged, read again with each snapshot)
    }
    if(r->n < ringLen)
    {
        r->s[(r->head + r->n) % ringLen] = in;
        r->n++;
    }
    else
    {
        r->s[r->head] = in;
        r->head = (r->head + 1) % ringLen;
    }
}

void clientSnapshotBegin()
{
    clientRags.stamp++;
}

void clientSnapshotEnd()
{
    for(Ring& r : clientRags.rings)
    {
        if(r.num != 0 && r.seen != clientRags.stamp)
        {
            dropRing(r); // (out of his sight, gibbed, risen: none in the snapshot)
        }
    }
}

void clientFrame()
{
    if(sv.active || cls.state != ca_connected || clientRags.rings.empty())
    {
        return;
    }
    const double clock = netsnap::interpActive() ? netsnap::interpTime() : cl.time;
    for(Ring& r : clientRags.rings)
    {
        if(r.num == 0 || r.n == 0)
        {
            continue;
        }
        if(r.num >= cl.num_entities || cl_entities[r.num].model != r.model)
        {
            ragdoll::unpublish(r.num); // (the entity shows another model now: a head; drawn as it is)
            continue;
        }
        // The two poses around the clock (before the oldest: the oldest; past the newest: the newest, held still).
        int lo = 0, hi = 0;
        for(int i = 0; i < r.n; i++)
        {
            if(r.at(i).time <= clock)
            {
                lo = hi = i;
            }
        }
        if(r.at(lo).time <= clock && lo + 1 < r.n)
        {
            hi = lo + 1;
        }
        ragdoll::publishNet(r.num, poseOf(r, r.at(lo)), poseOf(r, r.at(hi)), clock);
    }
}

void clientClear()
{
    for(Ring& r : clientRags.rings)
    {
        if(r.num != 0)
        {
            dropRing(r);
        }
    }
    clientRags.rings.clear();
}

int clientCount()
{
    int n = 0;
    for(const Ring& r : clientRags.rings)
    {
        n += r.num != 0 && r.n > 0 ? 1 : 0;
    }
    return n;
}

void logFrame()
{
    if(bodyLog.seconds <= 0.0)
    {
        return;
    }
    double t = -1.0;
    if(sv.active)
    {
        const progs::ServerVm vm;
        t = qcvm->time;
    }
    else if(cls.state == ca_connected && cl.viewentity > 0)
    {
        t = netsnap::interpActive() ? netsnap::interpTime() : cl.time;
    }
    if(t < 0.0)
    {
        return;
    }
    bodyLog.start = bodyLog.start < 0.0 ? t : bodyLog.start;
    if(t - bodyLog.start > bodyLog.seconds)
    {
        bodyLog.seconds = 0.0;
        Con_Printf("mp_bodylog: done\n");
        return;
    }
    if(sv.active)
    {
        const progs::ServerVm vm;
        logServer();
    }
    else
    {
        logClient();
    }
}

void init()
{
    Cmd_AddCommand("vr_mp_bodylog", bodylog_f);
    Cmd_AddCommand("vr_netrag_selftest", selftest_f);
}

} // namespace qvr::netrag
