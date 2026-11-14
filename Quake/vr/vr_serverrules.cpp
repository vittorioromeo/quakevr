// vr_serverrules.cpp -- see vr_serverrules.hpp.

#include "vr_serverrules.hpp"
#include "vr_clientprefs.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gameplaypresets.hpp"
#include "vr_menu.hpp"
#include "vr_presets.hpp"
#include "vr_protocol.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <stdlib.h>
#include <string.h>

namespace qvr::serverrules
{
namespace
{

// The melee timing settings: what decides whether a blow lands (the server's QC reads each: vr_melee.qc, combat.qc,
// vr_carry.qc, vr_crates.qc, vr_juice.qc, client.qc).
cvar_t* const meleeTiming[] = {
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

// The balance pages: every setting under them (Search's paths: menu::forEachSettingPath) is a rule, but for those below.
// (Not Locomotion: its comfort, turning and controls are each player's own.)
constexpr const char* rulePages[] = {
    "Advanced VR Options > Combat",
    "Advanced VR Options > Carrying and Throwing",
    "Advanced VR Options > Arcane Dimensions",
    "Advanced VR Options > World",
    "Advanced VR Options > Movement > Climbing",
    "Advanced VR Options > Movement > Grappling Hook",
    "Advanced VR Options > Movement > Swimming",
    "Advanced VR Options > Movement > Player Hitbox",
    "Advanced VR Options > Movement > Monster Hitbox",
    "VR Settings > Gameplay",
};

// A player's own on those pages: what only his senses get (sounds, haptics, colours, what is shown or printed) and how
// his own hands and weapons are posed and held (his weapons' offsets, the climbing hand's pose, the grip's fit, his stick):
// cvar names containing one of these.
constexpr const char* ownParts[] = {"sound", "haptic", "_show", "debug", "_viz", "vignette", "_hud", "_hue", "colour",
    "color", "_saturation", "_lightness", "_alpha", "_glow", "_text", "_print", "_draw", "vr_wofs_", "vr_climb_hand_",
    "vr_grip_fit", "_stick_", "anygrip"};

struct State
{
    bool built{false};
    za::Vector<cvar_t*> list; // the rules, by address (isRule's search)
    // Server: the values the clients were last sent; serverFrame's changed ones (kept: no allocation a frame).
    za::Vector<float> sent;
    za::Vector<int> changed;
    // Client: playing by a remote server's rules (shadowed), each rule's own value kept meanwhile; the config's write
    // swaps them in (swapped: the cvars' strings while it writes).
    bool shadowed{false};
    za::Vector<za::String> own;
    za::Vector<const char*> live;
    bool swapped{false};
};
State state;

[[nodiscard]] bool pageIsRules(const char* path)
{
    for(const char* p : rulePages)
    {
        const size_t n = strlen(p);
        if(!strncmp(path, p, n) && (path[n] == '\0' || path[n] == ' '))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool ownByName(const char* name)
{
    for(const char* part : ownParts)
    {
        if(strstr(name, part))
        {
            return true;
        }
    }
    return false;
}

void consider(cvar_t* var)
{
    if(!var || !(var->flags & CVAR_ARCHIVE) || clientprefs::indexOf(var) >= 0 || ownByName(var->name))
    {
        return;
    }
    for(const cvar_t* r : state.list)
    {
        if(r == var)
        {
            return;
        }
    }
    state.list.pushBack(var);
}

// The rules, gathered once (the menus' pages built for it: Search's index), sorted by address.
void build()
{
    if(state.built)
    {
        return;
    }
    state.built = true;
    for(cvar_t* var : meleeTiming)
    {
        consider(var);
    }
    const presetcore::Table& t = gameplaypresets::kind().table();
    for(int i = 0; i < t.rowCount; i++)
    {
        consider(t.rows[i].var);
    }
    menu::forEachSettingPath([](const cvar_t& var, const char* page, const char*) {
        if(pageIsRules(page))
        {
            consider(Cvar_FindVar(var.name));
        }
    });
    za::quickSort(state.list.begin(), state.list.end());
    state.sent.resize(state.list.size());
    state.own.resize(state.list.size());
    state.live.resize(state.list.size());
}

[[nodiscard]] int indexOf(const cvar_t* var)
{
    if(!state.built || !var)
    {
        return -1;
    }
    size_t lo = 0, hi = state.list.size();
    while(lo < hi)
    {
        const size_t mid = (lo + hi) / 2;
        if(state.list[mid] == var)
        {
            return static_cast<int>(mid);
        }
        if(state.list[mid] < var)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }
    return -1;
}

[[nodiscard]] float defaultOf(const cvar_t* var)
{
    return var->default_string ? static_cast<float>(atof(var->default_string)) : 0.f;
}

// QVR_SVC_RULES: [byte flags (1: all -- every rule not in these messages is at its default)][byte count]
// ([string cvar][string value]) x count, a message at most chunkBytes long (the first of several carries the flag).
constexpr int flagAll = 1;
constexpr int chunkBytes = 1000;
constexpr int chunkMost = 255;

void writeRules(sizebuf_t* msg, const int* which, int count, bool all)
{
    int k = 0;
    bool first = true;
    while(k < count || first)
    {
        // As many as fit in a chunk.
        int n = 0;
        int bytes = 4;
        while(k + n < count && n < chunkMost)
        {
            const cvar_t* var = state.list[which[k + n]];
            const int size = static_cast<int>(strlen(var->name) + strlen(var->string)) + 2;
            if(n > 0 && bytes + size > chunkBytes)
            {
                break;
            }
            bytes += size;
            n++;
        }
        MSG_WriteByte(msg, protocol::svc_quakevr);
        MSG_WriteByte(msg, protocol::QVR_SVC_RULES);
        MSG_WriteByte(msg, all && first ? flagAll : 0);
        MSG_WriteByte(msg, n);
        for(int j = 0; j < n; j++)
        {
            const cvar_t* var = state.list[which[k + j]];
            MSG_WriteString(msg, var->name);
            MSG_WriteString(msg, var->string);
        }
        k += n;
        first = false;
    }
}

[[nodiscard]] cvar_t* ruleNamed(const char* name)
{
    cvar_t* var = Cvar_FindVar(name);
    return indexOf(var) >= 0 ? var : nullptr;
}

// Client: the server's rules govern from now on: each rule's own value kept.
void shadow()
{
    if(state.shadowed)
    {
        return;
    }
    for(size_t i = 0; i < state.list.size(); i++)
    {
        state.own[i] = state.list[i]->string;
    }
    state.shadowed = true;
}

// vr_serverrules [cvar...]: how many rules and whose govern; each cvar named: whether a rule, its value here, the
// player's own (while a remote server's govern) and whether locked ("mp_rule:" lines, the harness's mp_rules). With
// none named, the rules off their defaults.
void list_f()
{
    build();
    const bool isRemote = remote();
    Con_Printf("mp_rules: count=%d remote=%d shadowed=%d (%s)\n", static_cast<int>(state.list.size()), isRemote ? 1 : 0,
        state.shadowed ? 1 : 0, state.shadowed ? "the remote server's govern" : sv.active ? "this server's govern" : "your own");
    if(Cmd_Argc() > 1)
    {
        for(int a = 1; a < Cmd_Argc(); a++)
        {
            const cvar_t* var = Cvar_FindVar(Cmd_Argv(a));
            const int i = indexOf(var);
            if(!var)
            {
                Con_Printf("mp_rule: name=%s none\n", Cmd_Argv(a));
                continue;
            }
            Con_Printf("mp_rule: name=%s rule=%d value=%s own=%s locked=%d\n", var->name, i >= 0 ? 1 : 0, var->string,
                i >= 0 && state.shadowed ? state.own[i].cStr() : var->string, locked(var) ? 1 : 0);
        }
        return;
    }
    for(size_t i = 0; i < state.list.size(); i++)
    {
        const cvar_t* var = state.list[i];
        const bool mine = state.shadowed && strcmp(state.own[i].cStr(), var->string);
        if(var->value != defaultOf(var) || mine)
        {
            Con_Printf("  %-32s %s%s%s%s\n", var->name, var->string, mine ? "  (yours " : "", mine ? state.own[i].cStr() : "",
                mine ? ")" : "");
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
    return state.shadowed && remote() && isRule(var);
}

float shown(const cvar_t* var)
{
    return var->value;
}

float own(const cvar_t* var)
{
    const int i = indexOf(var);
    return i >= 0 && state.shadowed ? static_cast<float>(atof(state.own[i].cStr())) : var->value;
}

const char* lockedBecause()
{
    return state.shadowed && remote() ? "The server's rules govern while you play on it (a preset sets them): its host picks "
                                        "them. Yours come back when you leave."
                                      : nullptr;
}

void serverWriteAll(sizebuf_t* msg)
{
    if(svs.maxclients <= 1)
    {
        return; // (single player: its own rules)
    }
    build();
    za::Vector<int> which;
    for(size_t i = 0; i < state.list.size(); i++)
    {
        if(state.list[i]->value != defaultOf(state.list[i]))
        {
            which.pushBack(static_cast<int>(i));
        }
    }
    writeRules(msg, which.data(), static_cast<int>(which.size()), true);
}

void serverFrame()
{
    if(!state.built || svs.maxclients <= 1)
    {
        return; // (serverReset builds them with the map)
    }
    za::Vector<int>& which = state.changed;
    which.clear();
    for(size_t i = 0; i < state.list.size(); i++)
    {
        if(state.list[i]->value != state.sent[i])
        {
            state.sent[i] = state.list[i]->value;
            which.pushBack(static_cast<int>(i));
        }
    }
    if(which.size() > 0)
    {
        writeRules(&sv.reliable_datagram, which.data(), static_cast<int>(which.size()), false);
    }
}

void serverReset()
{
    if(svs.maxclients <= 1)
    {
        return; // (single player: nothing to send, nothing gathered)
    }
    build();
    for(size_t i = 0; i < state.list.size(); i++)
    {
        state.sent[i] = state.list[i]->value;
    }
}

void clientParse()
{
    const int flags = MSG_ReadByte();
    const int count = MSG_ReadByte();
    const bool apply = remote(); // (a demo's, a listen host's own: read past)
    if(apply)
    {
        build();
        shadow();
        if(flags & flagAll)
        {
            for(cvar_t* var : state.list)
            {
                if(var->default_string && strcmp(var->string, var->default_string))
                {
                    Cvar_SetQuick(var, var->default_string);
                }
            }
        }
    }
    for(int k = 0; k < count; k++)
    {
        char name[64];
        q_strlcpy(name, MSG_ReadString(), sizeof(name));
        const char* value = MSG_ReadString();
        if(cvar_t* var = apply ? ruleNamed(name) : nullptr) // (a rule this build does not know: skipped)
        {
            if(strcmp(var->string, value))
            {
                Cvar_SetQuick(var, value);
            }
        }
    }
}

void clientLeft()
{
    if(!state.shadowed)
    {
        return;
    }
    state.shadowed = false;
    for(size_t i = 0; i < state.list.size(); i++)
    {
        if(strcmp(state.list[i]->string, state.own[i].cStr()))
        {
            Cvar_SetQuick(state.list[i], state.own[i].cStr());
        }
    }
}

void registerCommands()
{
    Cmd_AddCommand("vr_serverrules", list_f);
}

} // namespace qvr::serverrules

// Host_WriteConfigurationToFile, around Cvar_WriteVariables: while a remote server's rules govern, the player's own
// values written (each rule's string swapped for his own while the file is written, no callback run).
extern "C" void VR_ServerRulesConfigWrite(int begin)
{
    using namespace qvr::serverrules;
    if((begin != 0) == state.swapped || (begin && !state.shadowed))
    {
        return;
    }
    for(size_t i = 0; i < state.list.size(); i++)
    {
        if(begin)
        {
            state.live[i] = state.list[i]->string;
            state.list[i]->string = state.own[i].cStr();
        }
        else
        {
            state.list[i]->string = state.live[i];
        }
    }
    state.swapped = begin != 0;
}

// CL_Disconnect: left the server: the player's own values back.
extern "C" void VR_ServerRulesLeft(void)
{
    qvr::serverrules::clientLeft();
}
