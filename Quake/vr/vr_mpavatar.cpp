// vr_mpavatar.cpp -- other players' VR state (QVR_SVC_AVATAR; vr_mpavatar.hpp).

#include "vr_mpavatar.hpp"

#include "vr_move.hpp"
#include "vr_server.hpp"

namespace qvr::mpavatar
{

namespace
{

Avatar avatars[MAX_SCOREBOARD + 1]; // by player entity (0 unused)

glm::vec3 readEighths()
{
    glm::vec3 v{0.f};
    for(int i = 0; i < 3; i++)
    {
        v[i] = static_cast<float>(MSG_ReadShort()) / 8.f;
    }
    return v;
}

glm::vec3 readAngles16()
{
    glm::vec3 a{0.f};
    for(int i = 0; i < 3; i++)
    {
        a[i] = MSG_ReadAngle16(cl.protocolflags);
    }
    return a;
}

void printVec(const char* name, const glm::vec3& v)
{
    Con_Printf(" %s=%.1f %.1f %.1f", name, v.x, v.y, v.z);
}

void avatars_f()
{
    if(cls.state == ca_connected && cl.viewentity > 0)
    {
        for(int e = 1; e <= MAX_SCOREBOARD; e++)
        {
            const Avatar& a = avatars[e];
            if(!a.valid)
            {
                continue;
            }
            Con_Printf("mp_avatar: ent=%d self=%d flat=%d", e, e == cl.viewentity ? 1 : 0, a.flat ? 1 : 0);
            printVec("head", a.head);
            printVec("hand0", a.hands[0]);
            printVec("hand1", a.hands[1]);
            printVec("rot1", a.handAngles[1]);
            Con_Printf(" wpn=%d %d age=%.0f\n", a.weapon, a.weapon2, (realtime - a.received) * 1000.0);
        }
    }
    if(sv.active)
    {
        PR_SwitchQCVM(&sv.qcvm); // (server::clientMove: the edicts' numbers)
        for(int i = 0; i < svs.maxclients; i++)
        {
            const client_t& c = svs.clients[i];
            const VrMove* m = c.active && c.spawned && c.edict ? server::clientMove(c.edict) : nullptr;
            if(!m)
            {
                continue;
            }
            Con_Printf("mp_avatar_sv: ent=%d", i + 1);
            printVec("head", m->headPos);
            printVec("hand0", m->hands[0].pos);
            printVec("hand1", m->hands[1].pos);
            printVec("rot1", m->hands[1].rot);
            Con_Printf(" wpn=%d\n", static_cast<int>(c.edict->v.weapon));
        }
        PR_SwitchQCVM(nullptr);
    }
    Con_Printf("mp_avatars: done\n");
}

} // namespace

void parse()
{
    const int ent = MSG_ReadShort();
    Avatar a;
    a.valid = true;
    a.received = realtime;
    a.flat = (MSG_ReadByte() & 1) != 0;
    // (his entity's origin in this message: the entities come before the messages about them)
    const glm::vec3 origin = ent > 0 && ent < cl.num_entities
                                 ? glm::vec3{cl_entities[ent].msg_origins[0][0], cl_entities[ent].msg_origins[0][1],
                                       cl_entities[ent].msg_origins[0][2]}
                                 : glm::vec3{0.f};
    a.head = origin + readEighths();
    a.headAngles = readAngles16();
    for(int h = 0; h < 2; h++)
    {
        a.hands[h] = origin + readEighths();
        a.handAngles[h] = readAngles16();
    }
    a.weapon = MSG_ReadLong();
    a.weapon2 = MSG_ReadLong();
    if(ent > 0 && ent <= MAX_SCOREBOARD && ent != cl.viewentity)
    {
        avatars[ent] = a;
    }
}

const Avatar* of(int ent)
{
    return ent > 0 && ent <= MAX_SCOREBOARD && avatars[ent].valid ? &avatars[ent] : nullptr;
}

void init()
{
    Cmd_AddCommand("vr_mp_avatars", avatars_f);
}

} // namespace qvr::mpavatar
