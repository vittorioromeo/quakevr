// vr_cheats.cpp -- Debug > Cheats and Recording's commands: a scene cleaned for footage (vr_scene_clean,
// vr_scene_count), the monsters frozen (vr_freeze_monsters), noclip steered as a headset's stick walks, when each entity
// was made (.vr_born: the props made since the map loaded). See vr_cheats.hpp.

#include "vr_cheats.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_decals.hpp"
#include "vr_gore.hpp"
#include "vr_wounds.hpp"
#include "vr_bodyblood.hpp"
#include "vr_particles.hpp"
#include "vr_explosiondebris.hpp"
#include "vr_shells.hpp"
#include "vr_autopump.hpp"
#include "vr_smoulder.hpp"
#include "vr_fireparticles.hpp"
#include "vr_flamewobble.hpp"
#include "vr_weaponfx.hpp"
#include "vr_shock.hpp"
#include "vr_progs.hpp"
#include "vr_server.hpp"
#include "vr_move.hpp"
#include "vr_protocol.hpp"
#include "vr_api.h"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/Strcmp.hpp"


namespace qvr::cheats
{
namespace
{

// What vr_scene_clean takes: the server's parts (QC VR_SCENE_*, vr_cheats.qc), then the client's.
enum : int
{
    Gibs = 1,
    Corpses = 2,
    Props = 4,
    Fires = 8,
    ServerParts = Gibs | Corpses | Props | Fires,
    Decals = 16,
    Wounds = 32,
    Effects = 64,
    All = 127
};

struct Word
{
    const char* name;
    int bits;
};
constexpr Word words[] = {{"gibs", Gibs}, {"corpses", Corpses}, {"props", Props}, {"fires", Fires},
    {"decals", Decals}, {"wounds", Wounds}, {"effects", Effects}, {"all", All}};

// The parts named by the command's words; -1 for a word it doesn't know.
[[nodiscard]] int parseParts()
{
    int bits = 0;
    for(int i = 1; i < Cmd_Argc(); i++)
    {
        const char* a = Cmd_Argv(i);
        int found = -1;
        for(const Word& w : words)
        {
            if(!q_strcasecmp(a, w.name))
            {
                found = w.bits;
            }
        }
        if(found < 0)
        {
            return -1;
        }
        bits |= found;
    }
    return bits;
}

// The client's parts: cleared, or counted.
void clientParts(int bits, bool clear)
{
    if(cls.state == ca_dedicated)
    {
        return;
    }
    if(!clear)
    {
        Con_Printf("scene (client): decals %d, wound masks %d, particles %d, casings %d, explosion debris %d\n",
            decals::liveCount(), wounds::liveCount(), particles::liveCount(), shells::liveCount(), explosiondebris::liveCount());
        return;
    }
    if(bits & Decals)
    {
        decals::clear(); // (and the gore's drips, runs and pools: gore::clear)
    }
    if(bits & Wounds)
    {
        wounds::clear(); // every model's mask, the blood on your hands and gear
        bodyblood::clear();
    }
    if(bits & Fires)
    {
        smoulder::clear();
        fireparticles::clear();
        flamewobble::clear();
    }
    if(bits & Effects)
    {
        particles::clear();
        R_ClearParticles();
        explosiondebris::clear();
        explosiondebris::serverClear(); // (the server's chunks: a listen server's)
        shells::clear();
        autopump::clear();
        smoulder::clear();
        fireparticles::clear();
        flamewobble::clear();
        weaponfx::clear();
        shock::clear();
        ZA_MEMSET(cl_dlights, 0, sizeof(cl_dlights));
    }
    if(!(bits & (Decals | Wounds | Effects | Fires)))
    {
        return;
    }
    Con_Printf("scene (client): cleared%s%s%s%s\n", bits & Decals ? " decals" : "", bits & Wounds ? " wounds" : "",
        bits & Effects ? " effects" : "", bits & Fires ? " smoke" : "");
}

// The server's parts, by QC (VR_Scene_Clean: each removed as the game removes it), for sv_player.
void serverParts(int bits, bool clear)
{
    if(pr_global_struct->deathmatch)
    {
        SV_ClientPrintf("scene: not in deathmatch\n");
        return;
    }
    if(clear && !VR_GuestMay("vr_scene_clean"))
    {
        return; // (B28: it clears everyone's scene; counting is anyone's)
    }
    const func_t fn = progs::bindings().Scene_Clean;
    if(!fn)
    {
        SV_ClientPrintf("scene: these progs can't clean the scene\n");
        return;
    }
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(sv_player);
    pr_global_struct->other = EDICT_TO_PROG(qcvm->edicts);
    G_FLOAT(OFS_PARM0) = static_cast<float>(bits & ServerParts);
    G_FLOAT(OFS_PARM1) = clear ? 0.f : 1.f;
    PR_ExecuteProgram(fn);
}

void run(bool clear)
{
    int bits = clear ? parseParts() : All;
    if(clear && (bits <= 0))
    {
        Con_Printf("vr_scene_clean <gibs | corpses | props | fires | decals | wounds | effects | all>...: removes them "
                   "(props: the loose ones made since the map loaded)\n");
        return;
    }
    if(cmd_source == src_command)
    {
        clientParts(bits, clear);
        if(bits & ServerParts)
        {
            Cmd_ForwardToServer(); // (run by the server as src_client: serverParts)
        }
        return;
    }
    serverParts(bits, clear);
}

void clean_f()
{
    run(true);
}

void count_f()
{
    run(false);
}

// The living player a host's debug command acts on: the client in slot Cmd_Argv(`arg`) (none given: 0, you; hosting a
// coop game, another player), in the game; null (said why) otherwise. Not in deathmatch, but on a dedicated server
// (MP B11: its own console may, as it may kick; its players can't send these commands).
edict_t* slotPlayer(const char* command, int arg)
{
    const int slot = Cmd_Argc() > arg ? Q_atoi(Cmd_Argv(arg)) : 0;
    if(!sv.active || slot < 0 || slot >= svs.maxclients || !svs.clients[slot].active || !svs.clients[slot].spawned ||
        !svs.clients[slot].edict)
    {
        Con_Printf("%s: %s\n", command, sv.active ? "no player in that slot" : "not hosting a game");
        return nullptr;
    }
    if(deathmatch.value != 0.f && !isDedicated)
    {
        Con_Printf("%s: not in deathmatch (but on a dedicated server)\n", command);
        return nullptr;
    }
    edict_t* player = svs.clients[slot].edict;
    if(player->v.health <= 0.f)
    {
        Con_Printf("%s: that player is dead\n", command);
        return nullptr;
    }
    return player;
}

// vr_goto_exit [<slot>] [<map>]: the player put in the level's exit (its first trigger_changelevel's middle; with
// <map>, the one to that map: a hub's), touching it as if he walked in: the intermission, or the next level (Debug >
// Tests > Cheats; from agent/mp).
void gotoExit_f()
{
    edict_t* player = slotPlayer("vr_goto_exit", 1);
    if(!player)
    {
        return;
    }
    const progs::ServerVm vm;
    const char* wanted = Cmd_Argc() > 2 ? Cmd_Argv(2) : nullptr;
    edict_t* exit = nullptr;
    for(int i = svs.maxclients + 1; i < qcvm->num_edicts && !exit; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free || ZA_STRCMP(PR_GetString(e->v.classname), "trigger_changelevel"))
        {
            continue;
        }
        const eval_t* to = GetEdictFieldValueByName(e, "map");
        exit = !wanted || (to && to->string && !q_strcasecmp(PR_GetString(to->string), wanted)) ? e : nullptr;
    }
    if(!exit)
    {
        Con_Printf("vr_goto_exit: no exit (trigger_changelevel)%s%s in this level\n", wanted ? " to " : "", wanted ? wanted : "");
        return;
    }
    for(int k = 0; k < 3; k++)
    {
        player->v.origin[k] = (exit->v.absmin[k] + exit->v.absmax[k]) * 0.5f;
    }
    pr_global_struct->time = qcvm->time;
    SV_LinkEdict(player, true); // (its touch: changelevel_touch)
    const eval_t* map = GetEdictFieldValueByName(exit, "map");
    Con_Printf("vr_goto_exit: %s in the exit to %s at %.0f %.0f %.0f\n", PR_GetString(player->v.netname),
        map && map->string ? PR_GetString(map->string) : "?", player->v.origin[0], player->v.origin[1], player->v.origin[2]);
}

// vr_hurt <damage> [<slot>] [<attacker>]: the player takes that much damage from the world (QC's T_Damage, as a
// fall's): a real death at 1000, not `kill`'s respawn at once (Debug > Tests > Cheats; from agent/mp). Multiplayer's
// harness (mp_dm): the slot may be a bot's (FrikBot's bots are the edicts of slots no client is in; MP B14c) and the
// damage from entity number <attacker> (a player or a bot: his frag in deathmatch).
void hurt_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_hurt <damage> [<slot>] [<attacker entity>]\n");
        return;
    }
    if(!sv.active)
    {
        Con_Printf("vr_hurt: not hosting a game\n");
        return;
    }
    const progs::ServerVm vm;
    const int slot = Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) : 0;
    edict_t* bot = slot >= 0 && slot < svs.maxclients && !svs.clients[slot].active && (isDedicated || deathmatch.value == 0.f)
                       ? EDICT_NUM(slot + 1)
                       : nullptr;
    edict_t* player = bot && !bot->free && bot->v.health > 0.f && !ZA_STRCMP(PR_GetString(bot->v.classname), "player")
                          ? bot
                          : slotPlayer("vr_hurt", 2);
    if(!player)
    {
        return;
    }
    edict_t* attacker = qcvm->edicts;
    if(Cmd_Argc() > 3)
    {
        const int n = Q_atoi(Cmd_Argv(3));
        if(n <= 0 || n >= qcvm->num_edicts || EDICT_NUM(n)->free)
        {
            Con_Printf("vr_hurt: no entity %d\n", n);
            return;
        }
        attacker = EDICT_NUM(n);
    }
    const func_t fn = progs::findFunction("T_Damage");
    if(!fn)
    {
        Con_Printf("vr_hurt: no T_Damage in the game's progs\n");
        return;
    }
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(player);
    G_INT(OFS_PARM0) = EDICT_TO_PROG(player);
    G_INT(OFS_PARM1) = EDICT_TO_PROG(attacker);
    G_INT(OFS_PARM2) = EDICT_TO_PROG(attacker);
    G_FLOAT(OFS_PARM3) = static_cast<float>(Q_atof(Cmd_Argv(1)));
    // (its wound, sounds and gore are broadcast: a command runs before the server frame, whose SV_ClearDatagram would
    // drop them written to sv.datagram now; they open the next frame's)
    server::commandBroadcastBegin();
    PR_ExecuteProgram(fn);
    server::commandBroadcastEnd();
    Con_Printf("vr_hurt: %s at %.0f health%s\n", PR_GetString(player->v.netname), player->v.health,
        player->v.health <= 0.f ? ", dead" : "");
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand_ClientCommand("vr_scene_clean", clean_f);
    Cmd_AddCommand_ClientCommand("vr_scene_count", count_f);
    Cmd_AddCommand("vr_goto_exit", gotoExit_f);
    Cmd_AddCommand("vr_hurt", hurt_f);
}

const char* stateLine()
{
    if(cls.state != ca_connected)
    {
        return "Not in a game.";
    }
    int flags = 0;
    int movetype = MOVETYPE_WALK;
    if(const edict_t* p = progs::playerEdict())
    {
        flags = static_cast<int>(p->v.flags);
        movetype = static_cast<int>(p->v.movetype);
    }
    const int items = cl.items;
    const char* on = va("%s%s%s%s%s%s%s%s", flags & FL_GODMODE ? ", God" : "", movetype == MOVETYPE_NOCLIP ? ", Noclip" : "",
        flags & FL_NOTARGET ? ", Notarget" : "", movetype == MOVETYPE_FLY ? ", Fly" : "", items & IT_QUAD ? ", Quad" : "",
        items & IT_INVULNERABILITY ? ", Pentagram" : "", items & IT_INVISIBILITY ? ", Ring" : "", items & IT_SUIT ? ", Biosuit" : "");
    return on[0] ? va("On: %s", on + 2) : "No cheats on.";
}

} // namespace qvr::cheats

// SV_Physics, for each entity past the clients: whether it is a living monster frozen (vr_freeze_monsters; not in
// deathmatch). Its thinking and moving are skipped this frame, and its next think put off as long, so it goes on where
// it stopped once let go.
extern "C" int VR_MonsterFrozen(edict_t* ent)
{
    if(!qvr::vr_freeze_monsters.value || pr_global_struct->deathmatch || !(static_cast<int>(ent->v.flags) & FL_MONSTER) ||
        ent->v.health <= 0.f)
    {
        return 0;
    }
    if(ent->v.nextthink > 0.f)
    {
        ent->v.nextthink += static_cast<float>(host_frametime);
    }
    return 1;
}

// SV_NoclipMove: a headset's noclip flies as its stick walks: the move (VR_AdjustMove: the stick turned by the moving
// hand, then into the head's yaw) along the head's yaw, level, and up or down by its upmove (the moving hand's pitch), as
// swimming does. Quake's (where the view looks, .v_angle) was the aiming hand's: the flight went where the gun pointed.
// 0 for a flat screen's client: Quake's stands.
extern "C" int VR_NoclipAngles(edict_t* ent, float* out)
{
    const qvr::VrMove* move = qvr::server::clientMove(ent);
    if(!move || !(move->buttons & qvr::protocol::QVR_BUTTON_HANDSTRACKED))
    {
        return 0;
    }
    out[0] = 0.f;
    out[1] = move->headAngles.y;
    out[2] = 0.f;
    return 1;
}

// ED_Alloc: when the server's entity was made (.vr_born, QC): the map's own and those placed as it loads (the crates,
// the rocks and bricks) are made before its first second is out; vr_scene_clean props takes those made later.
extern "C" void VR_OnEdictAlloc(edict_t* ed)
{
    if(qcvm != &sv.qcvm)
    {
        return;
    }
    const int ofs = qvr::progs::fields().vr_born;
    if(ofs >= 0)
    {
        qvr::progs::fieldFloat(ed, ofs) = static_cast<float>(qcvm->time);
    }
}
