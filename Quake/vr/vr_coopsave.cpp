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

#include "vr_engine.hpp"
#include "vr_progs.hpp"

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

extern "C" int VR_CoopPickSlot(const char* name)
{
    if(!sv.loadgame || !load.coopLoaded || !svs.clients)
    {
        return -1; // (the first free slot, as ever)
    }
    const int slots = za::min(svs.maxclients, MAX_SCOREBOARD);
    // His saved player, by the name his connect request carried (net_dgrm.c), in whichever slot it was saved in.
    if(name && *name)
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
