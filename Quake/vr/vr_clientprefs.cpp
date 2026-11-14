// vr_clientprefs.cpp -- see vr_clientprefs.hpp.

#include "vr_clientprefs.hpp"

#include "vr_cvars.hpp"
#include "vr_tunables_api.hpp"
#include "Zancle/Math/MinMax.hpp"

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

struct State
{
    const cvar_t* vars[prefCount]{}; // (found by name on first use: cvars and tuning constants)
    bool resolved{false};
    // Client: what went up last.
    float lastSent[prefCount]{};
    bool sent{false};
    double nextSend{0.0};
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

// vr_mp_prefs: this client's preferences (the ones off the defaults: "mp_prefs_own:") and, hosting, each client's as the
// server has them (those that differ from the server's own: "mp_prefs_sv:").
void mpPrefs_f()
{
    resolve();
    if(cls.state == ca_connected && cl.viewentity > 0)
    {
        Con_Printf("mp_prefs_own: ent=%d sent=%d", cl.viewentity, state.sent ? 1 : 0);
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
            Con_Printf("mp_prefs_sv: ent=%d has=%d", slot + 1, state.slotHas[slot] ? 1 : 0);
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
    MSG_WriteByte(&cls.message, clc_stringcmd);
    MSG_WriteString(&cls.message, text);
    memcpy(state.lastSent, now, sizeof(now));
    state.sent = true;
    state.nextSend = realtime + static_cast<double>(za::max(vr_net_avatarcal_interval.value, 0.f));
}

void clientClear()
{
    state.sent = false;
}

void serverReset(int slot)
{
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        state.slotHas[slot] = false;
    }
}

void init()
{
    Cmd_AddCommand("vr_mp_prefs", mpPrefs_f);
    Cmd_AddCommand_ClientCommand("vr_prefs", prefs_f);
}

} // namespace qvr::clientprefs
