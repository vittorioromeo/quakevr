// vr_coopsave.cpp -- coop saves (docs/vr-port/MP_PLAN.md, B4): a save made in a game of more than one player carries
// per-player blocks, versioned, as comment lines of its header (COM_Parse skips them; vr_progs.cpp's VR_ReadSaveInfo
// reads them):
//
//   // qvr_coop <format> <maxclients> <coop>
//   // qvr_coopslot <slot> <in> [<count> <parm1..parm16> <parm17..> <name>]   (count: the parms that follow)
//
// `in`: 1 for a player in the game when it was saved (his edict, slot + 1, is a live player's), 0 for an empty slot. The
// parms are his level-start spawn parms (what a coop respawn gives him: QC's setspawnparms; a single player save has
// player 0's alone, in its header). The load (Host_Loadgame_f) plays the save with its own number of players (the
// edicts after the players' are numbered from it), keeps the players connected when that is the game's number already
// (they reconnect, as at a changelevel, each in his slot), gives each slot its parms, and spawns a player into a slot
// that was empty in the save as a new one (ClientConnect, PutClientInServer) instead of into the blank edict there.
//
// A single player save (no `qvr_coop` line) loads as before: slot 0's edict is the loaded player's for as long as the
// loaded level lasts.

#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_progs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{

constexpr int coopFormat = 1; // the `qvr_coop` lines' format (goes up when they change; a newer one is refused)

struct Slot
{
    bool in{false};                                  // a player in the game when it was saved
    float parms[NUM_SPAWN_PARMS]{};                  // his level-start parm1..parm16
    float ext[qvr::progs::numExtSpawnParms]{};       // parm17..parm75
    char name[32]{};
};

// The save being loaded (read before the old game ends: VR_ReadSaveInfo), and the loaded game's slots not yet spawned.
struct Load
{
    int format{0};       // 0: a single player save (no `qvr_coop` line)
    int maxclients{1};
    int coop{0};
    Slot slots[MAX_SCOREBOARD];
    bool coopLoaded{false};               // the level now played was loaded from a coop save
    bool pending[MAX_SCOREBOARD]{};       // its slot's edict is a loaded player's not yet spawned into
};
Load load;

// The lines a save writes (valid until the next save's fill: SaveData_Fill waits for the last save written).
za::String saveText;

// (B15) A player who left a game in progress (his game quit, crashed or lost its link): what he had, given back when
// a player of his name and his game's key joins the same game again (vr_mp_rejoin_keep). Kept through level changes
// (what he had is what a level change carries: QC's SetChangeParms; his level-start parms become those,
// VR_CoopLevelChangeParms), forgotten when the server shuts down (a new game, a load) or a coop save is loaded; the
// oldest overwritten when the table is full.
struct Leaver
{
    bool used{false};
    char name[32]{};
    char key[24]{};                                  // his game's vr_mp_clientkey (net_defs.h qvrKey; "": an older client's)
    double left{0.0};                                // realtime when he left (the oldest goes first when the table is full)
    int slot{-1};                                    // the slot he left (his again if it is free)
    float startParms[NUM_SPAWN_PARMS]{};             // his level-start parms (a coop respawn's: setspawnparms)
    float startExt[qvr::progs::numExtSpawnParms]{};
    float nowParms[NUM_SPAWN_PARMS]{};               // SetChangeParms as he left: his weapons, ammo, health, armour
    float nowExt[qvr::progs::numExtSpawnParms]{};
};
Leaver leavers[MAX_SCOREBOARD];
int rejoining[MAX_SCOREBOARD]; // a slot's player back as a leaver: his leavers[] index + 1 until he spawns (0: none)

[[nodiscard]] bool keepsLeavers()
{
    return sv.active && svs.maxclients > 1 && deathmatch.value == 0.f && qvr::vr_mp_rejoin_keep.value != 0.f &&
           qvr::progs::bindings().isVrProgs;
}

// The leaver of this name and game key (another game under his name is a new player: REVIEW_MP_2026-11-18 finding 8;
// a client of an older build sends no key, "", and is found by his name alone, as before).
[[nodiscard]] int leaverOf(const char* name, const char* key)
{
    if(!name || !*name)
    {
        return -1;
    }
    for(int i = 0; i < MAX_SCOREBOARD; i++)
    {
        if(leavers[i].used && !strcmp(leavers[i].name, name) && !strcmp(leavers[i].key, key ? key : ""))
        {
            return i;
        }
    }
    return -1;
}

// A loaded coop game's slot still waiting for its saved player (his saved edict, his saved parms): not a leaver's.
[[nodiscard]] bool slotPending(int slot)
{
    return sv.loadgame && load.coopLoaded && slot >= 0 && slot < MAX_SCOREBOARD && load.pending[slot];
}

void appendFloat(za::String& out, float v)
{
    char buf[32];
    q_snprintf(buf, sizeof(buf), " %g", static_cast<double>(v));
    out += buf;
}

} // namespace

extern "C" const char* VR_SaveCoopLines(void)
{
    saveText.clear();
    if(svs.maxclients <= 1 || !svs.clients)
    {
        return "";
    }
    char buf[128];
    q_snprintf(buf, sizeof(buf), "// qvr_coop %d %d %d\n", coopFormat, svs.maxclients, coop.value != 0.f ? 1 : 0);
    saveText += buf;
    for(int i = 0; i < svs.maxclients; i++)
    {
        const client_t& c = svs.clients[i];
        const bool in = c.active && c.spawned;
        q_snprintf(buf, sizeof(buf), "// qvr_coopslot %d %d", i, in ? 1 : 0);
        saveText += buf;
        if(in)
        {
            q_snprintf(buf, sizeof(buf), " %d", NUM_SPAWN_PARMS + qvr::progs::numExtSpawnParms);
            saveText += buf;
            for(int j = 0; j < NUM_SPAWN_PARMS; j++)
            {
                appendFloat(saveText, c.spawn_parms[j]);
            }
            const float* ext = qvr::progs::clientExtParms(i);
            for(int j = 0; j < qvr::progs::numExtSpawnParms; j++)
            {
                appendFloat(saveText, ext[j]);
            }
            saveText += ' ';
            for(const char* p = c.name; *p; ++p)
            {
                saveText += (*p == '\n' || *p == '\r') ? ' ' : *p;
            }
        }
        saveText += '\n';
    }
    return saveText.cStr();
}

extern "C" int VR_ReadCoopLines(const char* text, const char* relname)
{
    load.format = 0;
    load.maxclients = 1;
    load.coop = 0;
    for(Slot& s : load.slots)
    {
        s = Slot{};
    }
    const char* line = text ? strstr(text, "\n// qvr_coop ") : nullptr;
    if(!line)
    {
        return 1;
    }
    int format = 0, maxclients = 0, isCoop = 0;
    if(sscanf(line + 1, "// qvr_coop %d %d %d", &format, &maxclients, &isCoop) != 3 || maxclients < 2 ||
        maxclients > MAX_SCOREBOARD)
    {
        Con_Printf("ERROR: %s has a broken \"// qvr_coop\" line: not loaded\n", relname);
        return 0;
    }
    if(format > coopFormat)
    {
        Con_Printf("ERROR: %s is a coop save of a newer format (%d; this build reads up to %d): not loaded\n", relname,
            format, coopFormat);
        return 0;
    }
    load.format = format;
    load.maxclients = maxclients;
    load.coop = isCoop;
    for(const char* p = strstr(line + 1, "\n// qvr_coopslot "); p; p = strstr(p + 1, "\n// qvr_coopslot "))
    {
        const char* at = p + 1 + 16; // ("// qvr_coopslot ")
        char* end = nullptr;
        const long slot = strtol(at, &end, 10);
        if(end == at || slot < 0 || slot >= maxclients)
        {
            continue;
        }
        at = end;
        const long in = strtol(at, &end, 10);
        if(end == at || in != 1)
        {
            continue;
        }
        Slot& s = load.slots[slot];
        at = end;
        const long count = strtol(at, &end, 10);
        bool ok = end != at && count >= NUM_SPAWN_PARMS && count < 1024;
        for(long j = 0; j < count && ok; j++)
        {
            at = end;
            const float v = strtof(at, &end);
            ok = end != at;
            if(j < NUM_SPAWN_PARMS)
            {
                s.parms[j] = v;
            }
            else if(j - NUM_SPAWN_PARMS < qvr::progs::numExtSpawnParms)
            {
                s.ext[j - NUM_SPAWN_PARMS] = v; // (a newer build's further parms are skipped; fewer: the rest 0)
            }
        }
        if(!ok)
        {
            Con_Printf("ERROR: %s: the coop save's slot %ld has broken parms: not loaded\n", relname, slot);
            return 0;
        }
        if(*end == ' ')
        {
            end++;
        }
        const size_t n = strcspn(end, "\r\n");
        q_strlcpy(s.name, end, za::min(n + 1, sizeof(s.name)));
        s.in = true;
    }
    return 1;
}

extern "C" int VR_CoopSaveClients(void)
{
    return load.format ? load.maxclients : 0;
}

extern "C" int VR_CoopSaveIsCoop(void)
{
    return load.coop;
}

extern "C" void VR_CoopApplyLoad(void)
{
    load.coopLoaded = load.format != 0;
    for(int i = 0; i < MAX_SCOREBOARD; i++)
    {
        load.pending[i] = false;
    }
    if(!load.coopLoaded)
    {
        return;
    }
    // (a load with the players kept, no shutdown: what the leavers had belongs to the game left behind; each saved one
    // is back by his saved block, and the leaver path is open again in the loaded level)
    VR_CoopForgetLeavers();
    int players = 0;
    for(int i = 0; i < svs.maxclients && i < MAX_SCOREBOARD; i++)
    {
        const Slot& s = load.slots[i];
        load.pending[i] = s.in;
        if(!s.in)
        {
            continue;
        }
        players++;
        memcpy(svs.clients[i].spawn_parms, s.parms, sizeof(s.parms));
        float* ext = qvr::progs::clientExtParms(i);
        memcpy(ext, s.ext, sizeof(s.ext));
    }
    Con_Printf("coop save: %d players of %d slots, each back in his slot\n", players, svs.maxclients);
}

extern "C" int VR_CoopSlotLoaded(int slot)
{
    if(!sv.loadgame)
    {
        return 0;
    }
    if(!load.coopLoaded)
    {
        return 1; // (a single player save: as before)
    }
    return slot >= 0 && slot < MAX_SCOREBOARD && load.pending[slot] ? 1 : 0;
}

extern "C" int VR_CoopSlotDropped(int slot)
{
    if(!sv.loadgame || !load.coopLoaded || slot < 0 || slot >= MAX_SCOREBOARD || !load.pending[slot])
    {
        return 0;
    }
    // (his slot stays pending: back later, he takes his saved edict; the game runs once no other is still connecting)
    for(int i = 0; i < svs.maxclients && i < MAX_SCOREBOARD; i++)
    {
        if(load.pending[i] && svs.clients[i].active)
        {
            return 0;
        }
    }
    return 1;
}

extern "C" int VR_CoopSlotSpawned(int slot)
{
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        load.pending[slot] = false;
    }
    if(!load.coopLoaded)
    {
        return 1;
    }
    // (the loaded game paused until every saved player still connected is back in: none fights alone meanwhile)
    for(int i = 0; i < svs.maxclients && i < MAX_SCOREBOARD; i++)
    {
        if(load.pending[i] && svs.clients[i].active)
        {
            return 0;
        }
    }
    return 1;
}

namespace
{

// Slots a and b's saved blocks (name, level-start parms) swapped, each into its slot's parms; their saved edicts stay
// where they are (the bodies: what each had on him at the save).
void swapSaved(int a, int b)
{
    const Slot t = load.slots[a];
    load.slots[a] = load.slots[b];
    load.slots[b] = t;
    const bool p = load.pending[a];
    load.pending[a] = load.pending[b];
    load.pending[b] = p;
    for(const int i : {a, b})
    {
        memcpy(svs.clients[i].spawn_parms, load.slots[i].parms, sizeof(load.slots[i].parms));
        memcpy(qvr::progs::clientExtParms(i), load.slots[i].ext, sizeof(load.slots[i].ext));
    }
}

} // namespace

extern "C" int VR_CoopPickSlot(const char* name, const char* key, int isLocal)
{
    if(!svs.clients)
    {
        return -1;
    }
    const int slots = za::min(svs.maxclients, MAX_SCOREBOARD);
    // The host's own client (the loopback) always takes slot 0: every "own player" path reads his edict and hands there
    // (progs::hostSlot). In a loaded coop game whose slot 0 was saved for another name (the host renamed, or the save
    // made by another host or a dedicated server), his own saved block, if he has one, is swapped into slot 0 first:
    // his name and level-start parms are his, the body in slot 0 is the one saved there (and the other player, back by
    // his name, takes the host's saved slot and body). With no block of his he takes slot 0's saved player.
    if(isLocal && !svs.clients[0].active)
    {
        if(sv.loadgame && load.coopLoaded && load.pending[0] && name && strcmp(load.slots[0].name, name) != 0)
        {
            int own = -1;
            for(int i = 1; i < slots && own < 0 && *name; i++)
            {
                own = !svs.clients[i].active && load.pending[i] && !strcmp(load.slots[i].name, name) ? i : -1;
            }
            Con_Printf("coop save: the host %s takes slot 0, saved for %s (%s)\n", name, load.slots[0].name,
                own >= 0 ? va("his own saved slot %d swapped with it", own) : "he has no saved slot of his own");
            if(own >= 0)
            {
                swapSaved(0, own);
            }
        }
        return 0;
    }
    const bool loaded = sv.loadgame && load.coopLoaded;
    // His saved player, by the name his connect request carried (net_dgrm.c), in whichever slot it was saved in.
    if(loaded && name && *name)
    {
        for(int i = 0; i < slots; i++)
        {
            if(!svs.clients[i].active && load.pending[i] && !strcmp(load.slots[i].name, name))
            {
                Con_Printf("coop save: %s back in his saved slot %d\n", name, i);
                return i;
            }
        }
    }
    // (B15) a player back in a game he left (a loaded level too, once his slot no longer waits for a saved player): his
    // old slot if it is free (his entity number and colours again)
    const int k = keepsLeavers() ? leaverOf(name, key) : -1;
    const int slot = k >= 0 ? leavers[k].slot : -1;
    if(slot >= 0 && slot < slots && !svs.clients[slot].active && !slotPending(slot))
    {
        return slot;
    }
    if(!loaded)
    {
        return -1; // (the first free)
    }
    // Not a saved player still to come: a free slot no saved player waits for (a new player there), else the first free.
    for(int i = 0; i < slots; i++)
    {
        if(!svs.clients[i].active && !load.pending[i])
        {
            return i;
        }
    }
    return -1;
}

extern "C" void VR_CoopNoteLeave(int slot)
{
    if(!keepsLeavers() || slot < 0 || slot >= svs.maxclients || slot >= MAX_SCOREBOARD)
    {
        return;
    }
    client_t& c = svs.clients[slot];
    if(!c.active || !c.spawned || !c.edict || !c.name[0] || !strcmp(c.name, "unconnected"))
    {
        return;
    }
    const char* key = c.netconnection ? NET_QSocketGetQvrKey(c.netconnection) : "";
    int k = leaverOf(c.name, key);
    for(int i = 0; i < MAX_SCOREBOARD && k < 0; i++)
    {
        k = leavers[i].used ? -1 : i;
    }
    if(k < 0)
    {
        // Full: the one who left longest ago makes room (not one back in a slot and still to spawn: rejoining[]).
        for(int i = 0; i < MAX_SCOREBOARD; i++)
        {
            bool spawning = false;
            for(int j = 0; j < MAX_SCOREBOARD; j++)
            {
                spawning = spawning || rejoining[j] == i + 1;
            }
            if(!spawning && (k < 0 || leavers[i].left < leavers[k].left))
            {
                k = i;
            }
        }
        if(k < 0)
        {
            return;
        }
        Con_Printf("coop: what %s had (left %.0f s ago) is forgotten for %s's: the leavers' table is full\n",
            leavers[k].name, realtime - leavers[k].left, c.name);
    }
    Leaver& l = leavers[k];
    l = Leaver{};
    l.used = true;
    l.slot = slot;
    l.left = realtime;
    q_strlcpy(l.name, c.name, sizeof(l.name));
    q_strlcpy(l.key, key, sizeof(l.key));
    memcpy(l.startParms, c.spawn_parms, sizeof(l.startParms));
    float* ext = qvr::progs::clientExtParms(slot);
    memcpy(l.startExt, ext, sizeof(l.startExt));

    // What he has now, as a level change carries it (QC's SetChangeParms on his player; dead: a new player's).
    const qvr::progs::ServerVm vm;
    const int self = pr_global_struct->self;
    const float healthBefore = c.edict->v.health; // (SetChangeParms raises it to its floor in place)
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(c.edict);
    PR_ExecuteProgram(pr_global_struct->SetChangeParms);
    pr_global_struct->self = self;
    for(int j = 0; j < NUM_SPAWN_PARMS; j++)
    {
        l.nowParms[j] = (&pr_global_struct->parm1)[j];
    }
    // A return is not a level change: no floors (B17). A level change carries at least 25 shells and half the health
    // capacity (id's SetChangeParms), which here would be a top-up for leaving and coming back (B15's "a magazine more":
    // 17 shells back as 25). The cap above (the health capacity) stays; a dead player's new parms are left alone.
    if(healthBefore > 0.f)
    {
        if(l.nowParms[1] > healthBefore)
        {
            l.nowParms[1] = healthBefore;
        }
        const float shells = c.edict->v.ammo_shells; // (after SetChangeParms: a shell in his hand back in the reserve)
        if(l.nowParms[3] > shells && l.nowParms[3] <= 25.f)
        {
            l.nowParms[3] = shells;
        }
    }
    VR_StoreSpawnParms(slot); // (the globals parm17.. into his slot's store, read back here, then his own again)
    memcpy(l.nowExt, ext, sizeof(l.nowExt));
    memcpy(ext, l.startExt, sizeof(l.startExt));
    Con_Printf("coop: %s left; what he had is kept for his return (health %g, items %d)\n", l.name,
        static_cast<double>(l.nowParms[1]), static_cast<int>(l.nowParms[0]));
}

extern "C" void VR_CoopRejoinParms(int slot, const char* name, const char* key)
{
    if(slot < 0 || slot >= MAX_SCOREBOARD)
    {
        return;
    }
    rejoining[slot] = 0;
    // (not into a loaded coop game's slot still waiting for its saved player: his saved parms and edict are his)
    const int k = keepsLeavers() && !slotPending(slot) ? leaverOf(name, key) : -1;
    if(k < 0)
    {
        return;
    }
    // His level-start parms for a respawn (setspawnparms); what he had goes to his spawn (VR_CoopRejoinSpawn).
    memcpy(svs.clients[slot].spawn_parms, leavers[k].startParms, sizeof(leavers[k].startParms));
    memcpy(qvr::progs::clientExtParms(slot), leavers[k].startExt, sizeof(leavers[k].startExt));
    rejoining[slot] = k + 1;
}

extern "C" void VR_CoopRejoinSpawn(int slot)
{
    if(slot < 0 || slot >= MAX_SCOREBOARD || !rejoining[slot])
    {
        return;
    }
    Leaver& l = leavers[rejoining[slot] - 1];
    rejoining[slot] = 0;
    if(!l.used)
    {
        return;
    }
    for(int j = 0; j < NUM_SPAWN_PARMS; j++)
    {
        (&pr_global_struct->parm1)[j] = l.nowParms[j];
    }
    float* ext = qvr::progs::clientExtParms(slot);
    memcpy(ext, l.nowExt, sizeof(l.nowExt));
    VR_RestoreSpawnParms(slot); // (parm17.. from his slot's store into the globals)
    memcpy(ext, l.startExt, sizeof(l.startExt));
    Con_Printf("coop: %s is back with what he had when he left (health %g, items %d)\n", l.name,
        static_cast<double>(l.nowParms[1]), static_cast<int>(l.nowParms[0]));
    l = Leaver{};
}

extern "C" void VR_CoopLevelChangeParms(void)
{
    // A level change: each leaver's level-start parms are what he carries into the new level (what he had when he left,
    // as a level change carries it), as the players' own become theirs (SV_SaveSpawnparms): a respawn after his return
    // gives those, not the level he left from's start.
    for(Leaver& l : leavers)
    {
        if(l.used)
        {
            memcpy(l.startParms, l.nowParms, sizeof(l.startParms));
            memcpy(l.startExt, l.nowExt, sizeof(l.startExt));
        }
    }
}

// (MP bots phase 1) A coop game's bots follow a level change by the leavers' rules: FrikBot puts them back on the next
// level (bot_return, its scratch cvars), each a new player; what each had (QC's SetChangeParms on it, as on a player at
// the change) is kept here by its name and given back as it connects (VR_CoopBotParms). A bot's slot is never active (its
// player is FrikBot's), so SV_SaveSpawnparms' loop leaves it out.
extern "C" void VR_CoopNoteBots(void)
{
    if(!keepsLeavers())
    {
        return;
    }
    for(int slot = 0; slot < svs.maxclients && slot < MAX_SCOREBOARD; slot++)
    {
        edict_t* e = EDICT_NUM(slot + 1);
        if(svs.clients[slot].active || e->free || strcmp(PR_GetString(e->v.classname), "player") != 0 || !e->v.netname)
        {
            continue;
        }
        const char* name = PR_GetString(e->v.netname);
        int k = name[0] ? leaverOf(name, "") : -2; // (a bot's record has no game key: found by its name alone)
        for(int i = 0; i < MAX_SCOREBOARD && k == -1; i++)
        {
            k = leavers[i].used ? -1 : i;
        }
        if(k < 0)
        {
            continue;
        }
        Leaver& l = leavers[k];
        l = Leaver{};
        l.used = true;
        l.slot = slot;
        l.left = realtime; // (its age in the table: the oldest makes room when it is full)
        q_strlcpy(l.name, name, sizeof(l.name));
        const int self = pr_global_struct->self;
        pr_global_struct->self = EDICT_TO_PROG(e);
        PR_ExecuteProgram(pr_global_struct->SetChangeParms);
        pr_global_struct->self = self;
        for(int j = 0; j < NUM_SPAWN_PARMS; j++)
        {
            l.nowParms[j] = l.startParms[j] = (&pr_global_struct->parm1)[j];
        }
        float* ext = qvr::progs::clientExtParms(slot);
        float keep[qvr::progs::numExtSpawnParms];
        memcpy(keep, ext, sizeof(keep));
        VR_StoreSpawnParms(slot); // (the globals parm17.. into its slot's store, read back here, then the store as it was)
        memcpy(l.nowExt, ext, sizeof(l.nowExt));
        memcpy(l.startExt, ext, sizeof(l.startExt));
        memcpy(ext, keep, sizeof(keep));
        Con_Printf("coop: bot %s's things kept for the next level (health %g, items %d)\n", l.name,
            static_cast<double>(l.nowParms[1]), static_cast<int>(l.nowParms[0]));
    }
}

extern "C" int VR_CoopBotParms(int slot, const char* name)
{
    const int k = keepsLeavers() && slot >= 0 && slot < MAX_SCOREBOARD ? leaverOf(name, "") : -1;
    if(k < 0)
    {
        return 0;
    }
    Leaver& l = leavers[k];
    for(int j = 0; j < NUM_SPAWN_PARMS; j++)
    {
        (&pr_global_struct->parm1)[j] = l.nowParms[j];
    }
    float* ext = qvr::progs::clientExtParms(slot);
    float keep[qvr::progs::numExtSpawnParms];
    memcpy(keep, ext, sizeof(keep));
    memcpy(ext, l.nowExt, sizeof(l.nowExt));
    VR_RestoreSpawnParms(slot); // (parm17.. from its slot's store into the globals)
    memcpy(ext, keep, sizeof(keep));
    Con_Printf("coop: bot %s is back with what it had (health %g, items %d)\n", l.name, static_cast<double>(l.nowParms[1]),
        static_cast<int>(l.nowParms[0]));
    l = Leaver{};
    return 1;
}

extern "C" void VR_CoopForgetLeavers(void)
{
    for(int i = 0; i < MAX_SCOREBOARD; i++)
    {
        leavers[i] = Leaver{};
        rejoining[i] = 0;
    }
}
