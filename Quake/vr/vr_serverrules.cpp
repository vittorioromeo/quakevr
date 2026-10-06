// vr_serverrules.cpp -- see vr_serverrules.hpp.

#include "vr_serverrules.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_protocol.hpp"

#include <string.h>

namespace qvr::serverrules
{
namespace
{

// The melee timing settings: what decides whether a blow lands (the server's QC reads each: vr_melee.qc, combat.qc,
// vr_carry.qc, vr_crates.qc, vr_juice.qc, client.qc).
cvar_t* const rules[] = {
    &vr_melee_speed,         // a blow's least speed
    &vr_melee_wrist_speed,   // and the hand's own (its wrist's) over the last 0.12 s
    &vr_melee_pommel_wait,   // how long a pommel's touch waits for a speed
    &vr_melee_butt_run,      // how far a gun is carried at something for a butt strike
    &vr_bash_speed,          // a bash's least speed
    &vr_bash_deflect_window, // and how long it deflects
    &vr_headbutt_speed,      // a headbutt's least speed
    &vr_deflect_speed,       // a batting swing's speed (times vr_melee_speed)
    &vr_deflect_window,      // and how long it stays a bat
};
constexpr int ruleCount = static_cast<int>(sizeof(rules) / sizeof(rules[0]));

// Server: the values the clients were last sent.
float sent[ruleCount];

// Client: the server's values, and which were received.
float received[ruleCount];
bool known[ruleCount];

[[nodiscard]] int indexOf(const cvar_t* var)
{
    for(int i = 0; i < ruleCount; i++)
    {
        if(rules[i] == var)
        {
            return i;
        }
    }
    return -1;
}

[[nodiscard]] int indexOfName(const char* name)
{
    for(int i = 0; i < ruleCount; i++)
    {
        if(!strcmp(rules[i]->name, name))
        {
            return i;
        }
    }
    return -1;
}

void writeRules(sizebuf_t* msg, const int* which, int count)
{
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_RULES);
    MSG_WriteByte(msg, count);
    for(int k = 0; k < count; k++)
    {
        MSG_WriteString(msg, rules[which[k]]->name);
        MSG_WriteFloat(msg, rules[which[k]]->value);
    }
}

void list_f()
{
    const bool isRemote = remote();
    Con_Printf("Server rules (%s):\n", isRemote ? "the remote server's govern" : sv.active ? "this server's govern" : "not connected");
    for(int i = 0; i < ruleCount; i++)
    {
        if(isRemote)
        {
            if(known[i])
            {
                Con_Printf("  %-24s server %g (yours %g)\n", rules[i]->name, received[i], rules[i]->value);
            }
            else
            {
                Con_Printf("  %-24s server ? (yours %g)\n", rules[i]->name, rules[i]->value);
            }
        }
        else
        {
            Con_Printf("  %-24s %g\n", rules[i]->name, rules[i]->value);
        }
    }
}

} // namespace

bool isRule(const cvar_t* var)
{
    return indexOf(var) >= 0;
}

bool remote()
{
    return cls.state == ca_connected && !sv.active && !cls.demoplayback;
}

bool locked(const cvar_t* var)
{
    return var && remote() && isRule(var);
}

float shown(const cvar_t* var)
{
    const int i = indexOf(var);
    return i >= 0 && known[i] && remote() ? received[i] : var->value;
}

void serverWriteAll(sizebuf_t* msg)
{
    int which[ruleCount];
    for(int i = 0; i < ruleCount; i++)
    {
        which[i] = i;
    }
    writeRules(msg, which, ruleCount);
}

void serverFrame()
{
    int which[ruleCount];
    int count = 0;
    for(int i = 0; i < ruleCount; i++)
    {
        if(rules[i]->value != sent[i])
        {
            sent[i] = rules[i]->value;
            which[count++] = i;
        }
    }
    if(count > 0)
    {
        writeRules(&sv.reliable_datagram, which, count);
    }
}

void serverReset()
{
    for(int i = 0; i < ruleCount; i++)
    {
        sent[i] = rules[i]->value;
    }
}

void clientParse()
{
    const int count = MSG_ReadByte();
    for(int k = 0; k < count; k++)
    {
        char name[64];
        q_strlcpy(name, MSG_ReadString(), sizeof(name));
        const float value = MSG_ReadFloat();
        const int i = indexOfName(name);
        if(i >= 0) // (a rule this build does not know: skipped)
        {
            received[i] = value;
            known[i] = true;
        }
    }
}

void clientReset()
{
    for(int i = 0; i < ruleCount; i++)
    {
        known[i] = false;
    }
}

void registerCommands()
{
    Cmd_AddCommand("vr_serverrules", list_f);
}

} // namespace qvr::serverrules
