// vr_clientprefs.cpp -- see vr_clientprefs.hpp.

#include "vr_clientprefs.hpp"

#include "vr_cvars.hpp"
#include "vr_held.hpp"
#include "vr_tunables_api.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <math.h>
#include <string.h>

namespace qvr::clientprefs
{
namespace
{

// The per-player preferences: settings of how a player's own hands and controls work that the server's QC reads (each
// of them through cvar_hget). Balance stays the server's (server rules: vr_serverrules.cpp, B6); what only his own
// client reads needs no sending. The list's order is the "vr_prefs" command's: QVR_NET_VERSION changes with it.
constexpr const char* prefNames[] = {
    // how he holds the guns (each hand's aim)
    "vr_gunangle", "vr_gunyaw", "vr_offhandpitch", "vr_offhandyaw", "vr_handcal_off_mirror",
    // grips, holsters, two hands
    "vr_weapon_grip_mode", "vr_weapon_cycle_mode", "vr_holster_mode", "vr_holster_haptics", "vr_2h_handoff",
    "vr_ammo_switch_empty_hand",
    // reloading
    "vr_reload_mode", "vr_reload_mode_shot", "vr_reload_mode_sshot", "vr_reload_mode_nail", "vr_reload_mode_snail",
    "vr_reload_mode_gl", "vr_reload_mode_prox", "vr_reload_mode_rl", "vr_reload_mode_light", "vr_reload_mode_widow",
    "vr_reload_mode_adplasma", "vr_reload_mode_npistol", "vr_reload_mode_revolver",
    // grabbing and grenades
    "vr_forcegrab_mode", "vr_handgrenade_arm",
    // flat-screen play's
    "vr_flat_autoreload", "vr_flat_reload_hold", "vr_flat_grenade_arm_mode", "vr_flat_holster_drop_ahead",
    "vr_flat_holsters", "vr_flat_saw_autostart"};
constexpr int prefCount = static_cast<int>(sizeof(prefNames) / sizeof(prefNames[0]));
constexpr int prefsCommandVersion = 1; // "vr_prefs <version> <count> <values>": another version's or count's is ignored

// The rig (B3): each hand's fist as this client poses it (held::fist: spheres in the hand's frame, world units), in
// 64ths of a unit, up as "vr_rig <version> <hand> <total> <first> <hex>" (4 shorts a sphere, at most rigChunk spheres
// a command, one command a frame) when a sphere moved more than a step.
constexpr int rigCommandVersion = 2;
constexpr int maxFistSpheres = 128;
constexpr int rigChunk = 24;
constexpr float rigStep = 64.f;

struct Fist
{
    int n{0};
    short v[maxFistSpheres * 4]{};
};

struct State
{
    const cvar_t* vars[prefCount]{}; // (found by name on first use: cvars and tuning constants)
    bool resolved{false};
    // Client: what went up last.
    float lastSent[prefCount]{};
    bool sent{false};
    double nextSend{0.0};
    Fist fistSent[2];
    bool fistWasSent[2]{};
    double nextRigSend{0.0};
    Fist fistGoing[2];             // (being sent, a chunk a frame)
    int nextChunk[2]{-1, -1};      // (its next sphere; -1 none going)
    // Server: each slot's fists as they come in (whole: held::setPlayerFist).
    glm::vec4 slotFist[MAX_SCOREBOARD][2][maxFistSpheres]{};
    // Server: each slot's.
    float slot[MAX_SCOREBOARD][prefCount]{};
    bool slotHas[MAX_SCOREBOARD]{};
};
State state;

void resolve()
{
    if(state.resolved)
    {
        return;
    }
    for(int i = 0; i < prefCount; i++)
    {
        const cvar_t* var = Cvar_FindVar(prefNames[i]);
        state.vars[i] = var ? var : tunableFind(prefNames[i]);
    }
    state.resolved = true;
}

[[nodiscard]] float ownValue(int i)
{
    return state.vars[i] ? state.vars[i]->value : 0.f;
}

// The slot of player edict number `num` whose preferences the server has, or -1.
[[nodiscard]] int slotWithPrefs(int num)
{
    const int slot = num - 1;
    return slot >= 0 && slot < svs.maxclients && slot < MAX_SCOREBOARD && state.slotHas[slot] ? slot : -1;
}

// "vr_prefs <version> <count> <value>...": a client's preferences (send), kept for his slot.
void prefs_f()
{
    if(cmd_source != src_client || !sv.active || !host_client)
    {
        return;
    }
    const int slot = static_cast<int>(host_client - svs.clients);
    if(slot < 0 || slot >= MAX_SCOREBOARD || Q_atoi(Cmd_Argv(1)) != prefsCommandVersion ||
        Q_atoi(Cmd_Argv(2)) != prefCount || Cmd_Argc() != 3 + prefCount)
    {
        return;
    }
    for(int i = 0; i < prefCount; i++)
    {
        const float v = Q_atof(Cmd_Argv(3 + i));
        state.slot[slot][i] = v == v ? v : 0.f; // (not a number: 0)
    }
    state.slotHas[slot] = true;
}

[[nodiscard]] int hexDigit(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

// "vr_rig <version> <hand> <total> <first> <hex>|-": spheres `first`... of a client's fist of that hand (sendRig); with
// the last, the whole fist kept for his slot (held::setPlayerFist).
void rig_f()
{
    if(cmd_source != src_client || !sv.active || !host_client || Cmd_Argc() != 6 || Q_atoi(Cmd_Argv(1)) != rigCommandVersion)
    {
        return;
    }
    const int slot = static_cast<int>(host_client - svs.clients);
    const int hand = Q_atoi(Cmd_Argv(2));
    if(slot < 0 || slot >= MAX_SCOREBOARD || (hand != 0 && hand != 1))
    {
        return;
    }
    const int total = Q_atoi(Cmd_Argv(3));
    const int first = Q_atoi(Cmd_Argv(4));
    const char* hex = Cmd_Argv(5);
    const int len = !strcmp(hex, "-") ? 0 : static_cast<int>(strlen(hex));
    if(total < 0 || total > maxFistSpheres || first < 0 || len % 16 != 0 || first + len / 16 > total)
    {
        return;
    }
    {
        for(int s = 0; s < len / 16; s++)
        {
            float c[4];
            for(int k = 0; k < 4; k++)
            {
                int u = 0;
                for(int d = 0; d < 4; d++)
                {
                    const int x = hexDigit(hex[s * 16 + k * 4 + d]);
                    if(x < 0)
                    {
                        return;
                    }
                    u = u * 16 + x;
                }
                c[k] = static_cast<float>(static_cast<short>(static_cast<unsigned short>(u))) / rigStep;
            }
            state.slotFist[slot][hand][first + s] = glm::vec4{c[0], c[1], c[2], za::max(c[3], 0.f)};
        }
    }
    if(first + len / 16 == total)
    {
        za::Vector<glm::vec4> spheres;
        for(int s = 0; s < total; s++)
        {
            spheres.pushBack(state.slotFist[slot][hand][s]);
        }
        held::setPlayerFist(slot, hand, spheres);
    }
}

[[nodiscard]] Fist quantized(const za::Vector<glm::vec4>& spheres)
{
    Fist q;
    for(const glm::vec4& s : spheres)
    {
        if(q.n >= maxFistSpheres)
        {
            break;
        }
        for(int k = 0; k < 4; k++)
        {
            q.v[q.n * 4 + k] = static_cast<short>(za::clamp(lroundf(s[k] * rigStep), -32767L, 32767L));
        }
        q.n++;
    }
    return q;
}

// Off by more than a step anywhere (a hand's settings changed), or another number of spheres.
[[nodiscard]] bool moved(const Fist& a, const Fist& b)
{
    if(a.n != b.n)
    {
        return true;
    }
    for(int i = 0; i < a.n * 4; i++)
    {
        if(za::abs(static_cast<int>(a.v[i]) - static_cast<int>(b.v[i])) > 1)
        {
            return true;
        }
    }
    return false;
}

// The sum of a fist's coordinates and radii (vr_mp_prefs: the server's and the client's compared).
[[nodiscard]] float fistSum(const za::Vector<glm::vec4>& spheres)
{
    float sum = 0.f;
    for(const glm::vec4& s : spheres)
    {
        sum += za::abs(s.x) + za::abs(s.y) + za::abs(s.z) + s.w;
    }
    return sum;
}

// One hand's fist up a frame at most, when it moved, at most every vr_net_avatarcal_interval; only with room in the
// reliable buffer (else the next frame).
void sendRig()
{
    if(realtime < state.nextRigSend)
    {
        return;
    }
    for(int hand = 0; hand < 2; hand++)
    {
        if(state.nextChunk[hand] <= 0)
        {
            const Fist now = quantized(held::fist(hand));
            if(state.fistWasSent[hand] && !moved(now, state.fistSent[hand]))
            {
                continue;
            }
            state.fistGoing[hand] = now;
            state.nextChunk[hand] = 0;
        }
        const Fist& now = state.fistGoing[hand];
        const int first = za::max(state.nextChunk[hand], 0);
        const int last = za::min(first + rigChunk, now.n);
        char text[64 + rigChunk * 16];
        int n = q_snprintf(text, sizeof(text), "vr_rig %d %d %d %d ", rigCommandVersion, hand, now.n, first);
        if(last <= first)
        {
            text[n++] = '-';
        }
        for(int i = first * 4; i < last * 4; i++)
        {
            n += q_snprintf(text + n, sizeof(text) - static_cast<size_t>(n), "%04x", static_cast<unsigned short>(now.v[i]));
        }
        text[n] = 0;
        if(cls.message.cursize + n + 2 > cls.message.maxsize)
        {
            return; // (no room: the next frame)
        }
        MSG_WriteByte(&cls.message, clc_stringcmd);
        MSG_WriteString(&cls.message, text);
        state.nextChunk[hand] = last;
        if(last >= now.n)
        {
            state.fistSent[hand] = now;
            state.fistWasSent[hand] = true;
            state.nextChunk[hand] = -1;
            state.nextRigSend = realtime + static_cast<double>(za::max(vr_net_avatarcal_interval.value, 0.f));
        }
        return;
    }
}

// vr_mp_prefs: this client's preferences (the ones off the defaults: "mp_prefs_own:") and, hosting, each client's as the
// server has them (those that differ from the server's own: "mp_prefs_sv:").
void mpPrefs_f()
{
    resolve();
    if(cls.state == ca_connected && cl.viewentity > 0)
    {
        Con_Printf("mp_prefs_own: ent=%d sent=%d fist0=%d fist1=%d fistsum0=%.3f fistsum1=%.3f", cl.viewentity,
            state.sent ? 1 : 0, static_cast<int>(held::fist(0).size()), static_cast<int>(held::fist(1).size()),
            fistSum(held::fist(0)), fistSum(held::fist(1)));
        for(int i = 0; i < prefCount; i++)
        {
            const cvar_t* var = state.vars[i];
            if(var && var->default_string && strcmp(var->string, var->default_string) != 0)
            {
                Con_Printf(" %s=%g", prefNames[i], var->value);
            }
        }
        Con_Printf("\n");
    }
    if(sv.active)
    {
        for(int slot = 0; slot < svs.maxclients && slot < MAX_SCOREBOARD; slot++)
        {
            if(!svs.clients[slot].active)
            {
                continue;
            }
            const edict_t* player = (const edict_t*)((const byte*)sv.qcvm.edicts + (slot + 1) * sv.qcvm.edict_size);
            Con_Printf("mp_prefs_sv: ent=%d has=%d fist0=%d fist1=%d fistsum0=%.3f fistsum1=%.3f fistgrabs=%d", slot + 1,
                state.slotHas[slot] ? 1 : 0, static_cast<int>(held::fistOf(player, 0).size()),
                static_cast<int>(held::fistOf(player, 1).size()), fistSum(held::fistOf(player, 0)),
                fistSum(held::fistOf(player, 1)), held::playerFistGrabs(slot));
            int same = 0;
            for(int i = 0; i < prefCount && state.slotHas[slot]; i++)
            {
                if(state.slot[slot][i] != ownValue(i))
                {
                    Con_Printf(" %s=%g", prefNames[i], state.slot[slot][i]);
                }
                else
                {
                    same++;
                }
            }
            Con_Printf(" same=%d\n", same);
        }
    }
}

} // namespace

int indexOf(const cvar_t* var)
{
    resolve();
    for(int i = 0; i < prefCount; i++)
    {
        if(var && state.vars[i] == var)
        {
            return i;
        }
    }
    return -1;
}

float value(int pref, const cvar_t* var, const edict_t* player)
{
    if(pref >= 0 && pref < prefCount && player && svs.maxclients > 1 && sv.active && qcvm == &sv.qcvm)
    {
        const int slot = slotWithPrefs(static_cast<int>(((const byte*)player - (const byte*)qcvm->edicts) / qcvm->edict_size));
        if(slot >= 0)
        {
            return state.slot[slot][pref];
        }
    }
    return var ? var->value : 0.f;
}

float forQc(int pref, const cvar_t* var)
{
    if(pref >= 0 && pref < prefCount && svs.maxclients > 1 && sv.active && qcvm == &sv.qcvm)
    {
        for(const int e : {pr_global_struct->self, pr_global_struct->other})
        {
            const int slot = slotWithPrefs(e / qcvm->edict_size);
            if(slot >= 0)
            {
                return state.slot[slot][pref];
            }
        }
    }
    return var ? var->value : 0.f;
}

void send()
{
    if(cls.state != ca_connected || cls.signon != SIGNONS || cls.demoplayback || cl.maxclients <= 1)
    {
        return;
    }
    resolve();
    float now[prefCount];
    bool changed = !state.sent;
    for(int i = 0; i < prefCount; i++)
    {
        now[i] = ownValue(i);
        changed = changed || now[i] != state.lastSent[i];
    }
    sendRig();
    if(!changed || (state.sent && realtime < state.nextSend))
    {
        return;
    }
    char text[1024];
    int n = q_snprintf(text, sizeof(text), "vr_prefs %d %d", prefsCommandVersion, prefCount);
    for(int i = 0; i < prefCount && n > 0 && n < static_cast<int>(sizeof(text)); i++)
    {
        n += q_snprintf(text + n, sizeof(text) - static_cast<size_t>(n), " %g", now[i]);
    }
    if(cls.message.cursize + n + 2 > cls.message.maxsize)
    {
        return; // (no room: the next frame)
    }
    MSG_WriteByte(&cls.message, clc_stringcmd);
    MSG_WriteString(&cls.message, text);
    memcpy(state.lastSent, now, sizeof(now));
    state.sent = true;
    state.nextSend = realtime + static_cast<double>(za::max(vr_net_avatarcal_interval.value, 0.f));
}

void clientClear()
{
    state.sent = false;
    state.fistWasSent[0] = state.fistWasSent[1] = false;
    state.nextChunk[0] = state.nextChunk[1] = -1;
}

void serverReset(int slot)
{
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        state.slotHas[slot] = false;
        held::setPlayerFist(slot, 0, {});
        held::setPlayerFist(slot, 1, {});
    }
}

void init()
{
    Cmd_AddCommand("vr_mp_prefs", mpPrefs_f);
    Cmd_AddCommand_ClientCommand("vr_prefs", prefs_f);
    Cmd_AddCommand_ClientCommand("vr_rig", rig_f);
}

} // namespace qvr::clientprefs
