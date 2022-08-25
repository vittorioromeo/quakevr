/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2021 Vittorio Romeo

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "host.hpp"
#include "quakedef.hpp"
#include "net.hpp"
#include "zone.hpp"
#include "keys.hpp"
#include "protocol.hpp"
#include "input.hpp"
#include "sys.hpp"
#include "msg.hpp"
#include "vr.hpp"
#include "vr_cvars.hpp"
#include "util.hpp"
#include "worldtext.hpp"
#include "cmd.hpp"
#include "console.hpp"
#include "quakedef_macros.hpp"
#include "menu.hpp"
#include "screen.hpp"
#include "client.hpp"
#include "sys.hpp"
#include "saveutil.hpp"
#include "sizebuf.hpp"
#include "qcvm.hpp"
#include "glquake.hpp"
#include "crc.hpp"

#include <ctime>

#ifndef _WIN32
#include <dirent.h>
#endif

extern cvar_t pausable;

int current_skill;

void Mod_Print();

/*
==================
Host_Quit_f
==================
*/

void Host_Quit_f()
{
    if(key_dest != key_console && cls.state != ca_dedicated)
    {
        M_Menu_Quit_f();
        return;
    }
    CL_Disconnect();
    Host_ShutdownServer(false);

    Sys_Quit();
}

//==============================================================================
// johnfitz -- extramaps management
//==============================================================================

/*
==================
FileList_Add
==================
*/
void FileList_Add(const char* name, filelist_item_t** list)
{
    filelist_item_t* item;
    filelist_item_t* cursor;
    filelist_item_t* prev;

    // ignore duplicate
    for(item = *list; item; item = item->next)
    {
        if(!Q_strcmp(name, item->name))
        {
            return;
        }
    }

    item = (filelist_item_t*)Z_Malloc(sizeof(filelist_item_t));
    q_strlcpy(item->name, name, sizeof(item->name));

    // insert each entry in alphabetical order
    if(*list == nullptr ||
        q_strcasecmp(item->name, (*list)->name) < 0) // insert at front
    {
        item->next = *list;
        *list = item;
    }
    else // insert later
    {
        prev = *list;
        cursor = (*list)->next;
        while(cursor && (q_strcasecmp(item->name, cursor->name) > 0))
        {
            prev = cursor;
            cursor = cursor->next;
        }
        item->next = prev->next;
        prev->next = item;
    }
}

static void FileList_Clear(filelist_item_t** list)
{
    filelist_item_t* blah;

    while(*list)
    {
        blah = (*list)->next;
        Z_Free(*list);
        *list = blah;
    }
}

filelist_item_t* extralevels;

void ExtraMaps_Add(const char* name)
{
    FileList_Add(name, &extralevels);
}

void ExtraMaps_Init()
{
#ifdef _WIN32
    WIN32_FIND_DATA fdat;
    HANDLE fhnd;
#else
    DIR* dir_p;
    struct dirent* dir_t;
#endif
    char filestring[MAX_OSPATH];
    char mapname[32];
    char ignorepakdir[32];
    searchpath_t* search;
    pack_t* pak;
    int i;

    // we don't want to list the maps in id1 pakfiles,
    // because these are not "add-on" levels
    q_snprintf(ignorepakdir, sizeof(ignorepakdir), "/%s/", GAMENAME);

    for(search = com_searchpaths; search; search = search->next)
    {
        if(*search->filename) // directory
        {
#ifdef _WIN32
            q_snprintf(filestring, sizeof(filestring), "%s/maps/*.bsp",
                search->filename);
            fhnd = FindFirstFile(filestring, &fdat);
            if(fhnd == INVALID_HANDLE_VALUE)
            {
                continue;
            }

            do
            {
                COM_StripExtension(fdat.cFileName, mapname, sizeof(mapname));
                ExtraMaps_Add(mapname);
            } while(FindNextFile(fhnd, &fdat));
            FindClose(fhnd);
#else
            q_snprintf(
                filestring, sizeof(filestring), "%s/maps/", search->filename);
            dir_p = opendir(filestring);
            if(dir_p == nullptr) continue;
            while((dir_t = readdir(dir_p)) != nullptr)
            {
                if(q_strcasecmp(COM_FileGetExtension(dir_t->d_name), "bsp") !=
                    0)
                    continue;
                COM_StripExtension(dir_t->d_name, mapname, sizeof(mapname));
                ExtraMaps_Add(mapname);
            }
            closedir(dir_p);
#endif
        }
        else // pakfile
        {
            if(!strstr(search->pack->filename, ignorepakdir))
            {
                // don't list standard id maps
                for(i = 0, pak = search->pack; i < pak->numfiles; i++)
                {
                    if(!strcmp(COM_FileGetExtension(pak->files[i].name), "bsp"))
                    {
                        if(pak->files[i].filelen > 32 * 1024)
                        {
                            // don't list files under 32k (ammo boxes etc)
                            COM_StripExtension(pak->files[i].name + 5, mapname,
                                sizeof(mapname));
                            ExtraMaps_Add(mapname);
                        }
                    }
                }
            }
        }
    }
}

static void ExtraMaps_Clear()
{
    FileList_Clear(&extralevels);
}

void ExtraMaps_NewGame()
{
    ExtraMaps_Clear();
    ExtraMaps_Init();
}

/*
==================
Host_Maps_f
==================
*/
void Host_Maps_f()
{
    int i;
    filelist_item_t* level;

    for(level = extralevels, i = 0; level; level = level->next, i++)
    {
        Con_SafePrintf("   %s\n", level->name);
    }

    if(i)
    {
        Con_SafePrintf("%i map(s)\n", i);
    }
    else
    {
        Con_SafePrintf("no maps found\n");
    }
}

//==============================================================================
// johnfitz -- modlist management
//==============================================================================

filelist_item_t* modlist;

void Modlist_Add(const char* name)
{
    FileList_Add(name, &modlist);
}

#ifdef _WIN32
void Modlist_Init()
{
    WIN32_FIND_DATA fdat;
    HANDLE fhnd;
    DWORD attribs;
    char dir_string[MAX_OSPATH];
    char mod_string[MAX_OSPATH];

    q_snprintf(dir_string, sizeof(dir_string), "%s/*", com_basedir);
    fhnd = FindFirstFile(dir_string, &fdat);
    if(fhnd == INVALID_HANDLE_VALUE)
    {
        return;
    }

    do
    {
        if(!strcmp(fdat.cFileName, ".") || !strcmp(fdat.cFileName, ".."))
        {
            continue;
        }
        q_snprintf(mod_string, sizeof(mod_string), "%s/%s", com_basedir,
            fdat.cFileName);
        attribs = GetFileAttributes(mod_string);
        if(attribs != INVALID_FILE_ATTRIBUTES &&
            (attribs & FILE_ATTRIBUTE_DIRECTORY))
        {
            /* don't bother testing for pak files / progs.dat */
            Modlist_Add(fdat.cFileName);
        }
    } while(FindNextFile(fhnd, &fdat));

    FindClose(fhnd);
}
#else
void Modlist_Init()
{
    DIR *dir_p, *mod_dir_p;
    struct dirent* dir_t;
    char dir_string[MAX_OSPATH], mod_string[MAX_OSPATH];

    q_snprintf(dir_string, sizeof(dir_string), "%s/", com_basedir);
    dir_p = opendir(dir_string);
    if(dir_p == nullptr) return;

    while((dir_t = readdir(dir_p)) != nullptr)
    {
        if(!strcmp(dir_t->d_name, ".") || !strcmp(dir_t->d_name, ".."))
            continue;
        if(!q_strcasecmp(COM_FileGetExtension(dir_t->d_name),
               "app")) // skip .app bundles on macOS
            continue;
        q_snprintf(
            mod_string, sizeof(mod_string), "%s%s/", dir_string, dir_t->d_name);
        mod_dir_p = opendir(mod_string);
        if(mod_dir_p == nullptr) continue;
        /* don't bother testing for pak files / progs.dat */
        Modlist_Add(dir_t->d_name);
        closedir(mod_dir_p);
    }

    closedir(dir_p);
}
#endif

//==============================================================================
// ericw -- demo list management
//==============================================================================

filelist_item_t* demolist;

static void DemoList_Clear()
{
    FileList_Clear(&demolist);
}

void DemoList_Rebuild()
{
    DemoList_Clear();
    DemoList_Init();
}

// TODO: Factor out to a general-purpose file searching function
void DemoList_Init()
{
#ifdef _WIN32
    WIN32_FIND_DATA fdat;
    HANDLE fhnd;
#else
    DIR* dir_p;
    struct dirent* dir_t;
#endif
    char filestring[MAX_OSPATH];
    char demname[32];
    char ignorepakdir[32];
    searchpath_t* search;
    pack_t* pak;
    int i;

    // we don't want to list the demos in id1 pakfiles,
    // because these are not "add-on" demos
    q_snprintf(ignorepakdir, sizeof(ignorepakdir), "/%s/", GAMENAME);

    for(search = com_searchpaths; search; search = search->next)
    {
        if(*search->filename) // directory
        {
#ifdef _WIN32
            q_snprintf(
                filestring, sizeof(filestring), "%s/*.dem", search->filename);
            fhnd = FindFirstFile(filestring, &fdat);
            if(fhnd == INVALID_HANDLE_VALUE)
            {
                continue;
            }

            do
            {
                COM_StripExtension(fdat.cFileName, demname, sizeof(demname));
                FileList_Add(demname, &demolist);
            } while(FindNextFile(fhnd, &fdat));
            FindClose(fhnd);
#else
            q_snprintf(filestring, sizeof(filestring), "%s/", search->filename);
            dir_p = opendir(filestring);
            if(dir_p == nullptr) continue;
            while((dir_t = readdir(dir_p)) != nullptr)
            {
                if(q_strcasecmp(COM_FileGetExtension(dir_t->d_name), "dem") !=
                    0)
                    continue;
                COM_StripExtension(dir_t->d_name, demname, sizeof(demname));
                FileList_Add(demname, &demolist);
            }
            closedir(dir_p);
#endif
        }
        else // pakfile
        {
            if(!strstr(search->pack->filename, ignorepakdir))
            {
                // don't list standard id demos
                for(i = 0, pak = search->pack; i < pak->numfiles; i++)
                {
                    if(!strcmp(COM_FileGetExtension(pak->files[i].name), "dem"))
                    {
                        COM_StripExtension(
                            pak->files[i].name, demname, sizeof(demname));
                        FileList_Add(demname, &demolist);
                    }
                }
            }
        }
    }
}


/*
==================
Host_Mods_f -- johnfitz

list all potential mod directories (contain either a pak file or a progs.dat)
==================
*/
void Host_Mods_f()
{
    int i;
    filelist_item_t* mod;

    for(mod = modlist, i = 0; mod; mod = mod->next, i++)
    {
        Con_SafePrintf("   %s\n", mod->name);
    }

    if(i)
    {
        Con_SafePrintf("%i mod(s)\n", i);
    }
    else
    {
        Con_SafePrintf("no mods found\n");
    }
}

//==============================================================================

/*
=============
Host_Mapname_f -- johnfitz
=============
*/
void Host_Mapname_f()
{
    if(sv.active)
    {
        Con_Printf("\"mapname\" is \"%s\"\n", sv.name);
        return;
    }

    if(cls.state == ca_connected)
    {
        Con_Printf("\"mapname\" is \"%s\"\n", cl.mapname);
        return;
    }

    Con_Printf("no map loaded\n");
}

/*
==================
Host_Status_f
==================
*/
void Host_Status_f()
{
    void (*print_fn)(const char* fmt, ...) FUNCP_PRINTF(1, 2);
    client_t* client;
    int seconds;
    int minutes;
    int hours = 0;
    int j, i /* QSS */;

    if(cmd_source == src_command)
    {
        if(!sv.active)
        {
            Cmd_ForwardToServer();
            return;
        }
        print_fn = Con_Printf;
    }
    else
    {
        print_fn = SV_ClientPrintf;
    }

    print_fn("host:    %s\n", Cvar_VariableString("hostname"));
    print_fn("version: %4.2f\n", VERSION);

    // QSS
    if(ipv4Available)
    {
        print_fn("tcp/ip:  %s\n",
            my_ipv4_address); // Spike -- FIXME: we should really have ports
                              // displayed here or something
    }

    // QSS
    if(ipv6Available)
    {
        print_fn("ipv6:    %s\n", my_ipv6_address);
    }

    if(ipxAvailable)
    {
        print_fn("ipx:     %s\n", my_ipx_address);
    }
    print_fn("map:     %s\n", sv.name);

    // QSS
    for(i = 1, j = 0; i < MAX_MODELS; i++)
    {
        if(sv.model_precache[i])
        {
            j++;
        }
    }
    print_fn("models:  %i/%i\n", j, MAX_MODELS - 1);

    for(i = 1, j = 0; i < MAX_SOUNDS; i++)
    {
        if(sv.sound_precache[i])
        {
            j++;
        }
    }
    print_fn("sounds:  %i/%i\n", j, MAX_SOUNDS - 1);
    for(i = 0, j = 0; i < MAX_PARTICLETYPES; i++)
    {
        if(sv.particle_precache[i])
        {
            j++;
        }
    }

    if(j)
    {
        print_fn("effects: %i/%i\n", j, MAX_PARTICLETYPES - 1);
    }

    for(i = 1, j = 1; i < sv.qcvm.num_edicts; i++)
    {
        if(!sv.qcvm.edicts[i].free)
        {
            j++;
        }
    }
    print_fn("entities:%i/%i\n", j, sv.qcvm.max_edicts);

    print_fn("players: %i active (%i max)\n\n", net_activeconnections,
        svs.maxclients);

    for(j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
    {
        if(!client->active)
        {
            continue;
        }

        // QSS
        if(client->netconnection)
        {
            seconds =
                (int)(net_time - NET_QSocketGetTime(client->netconnection));
        }
        else
        {
            seconds = 0;
        }

        minutes = seconds / 60;
        if(minutes)
        {
            seconds -= (minutes * 60);
            hours = minutes / 60;
            if(hours)
            {
                minutes -= (hours * 60);
            }
        }
        else
        {
            hours = 0;
        }
        print_fn("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n", j + 1, client->name,
            (int)client->edict->v.frags, hours, minutes, seconds);

        // QSS
        if(cmd_source == src_command)
        {
            print_fn("   %s\n",
                client->netconnection
                    ? NET_QSocketGetTrueAddressString(client->netconnection)
                    : "botclient");
        }
        else
        {
            print_fn("   %s\n",
                client->netconnection
                    ? NET_QSocketGetMaskedAddressString(client->netconnection)
                    : "botclient");
        }
    }
}

/*
==================
Host_God_f

Sets client to godmode
==================
*/
void Host_God_f()
{
    if(cmd_source == src_command)
    {
        Cmd_ForwardToServer();
        return;
    }

    if(pr_global_struct->deathmatch)
    {
        return;
    }

    // johnfitz -- allow user to explicitly set god mode to on or off
    switch(Cmd_Argc())
    {
        case 1:
            quake::util::toggleFlag(sv_player, FL_GODMODE);
            if(!(quake::util::hasFlag(sv_player, FL_GODMODE)))
            {
                SV_ClientPrintf("godmode OFF\n");
            }
            else
            {
                SV_ClientPrintf("godmode ON\n");
            }
            break;
        case 2:
            if(Q_atof(Cmd_Argv(1)))
            {
                quake::util::addFlag(sv_player, FL_GODMODE);
                SV_ClientPrintf("godmode ON\n");
            }
            else
            {
                quake::util::removeFlag(sv_player, FL_GODMODE);
                SV_ClientPrintf("godmode OFF\n");
            }
            break;
        default:
            Con_Printf(
                "god [value] : toggle god mode. values: 0 = off, 1 = on\n");
            break;
    }
    // johnfitz
}

/*
==================
Host_Notarget_f
==================
*/
void Host_Notarget_f()
{
    if(cmd_source == src_command)
    {
        Cmd_ForwardToServer();
        return;
    }

    if(pr_global_struct->deathmatch)
    {
        return;
    }

    // johnfitz -- allow user to explicitly set notarget to on or off
    switch(Cmd_Argc())
    {
        case 1:
            quake::util::toggleFlag(sv_player, FL_NOTARGET);
            if(!(quake::util::hasFlag(sv_player, FL_NOTARGET)))
            {
                SV_ClientPrintf("notarget OFF\n");
            }
            else
            {
                SV_ClientPrintf("notarget ON\n");
            }
            break;
        case 2:
            if(Q_atof(Cmd_Argv(1)))
            {
                quake::util::addFlag(sv_player, FL_NOTARGET);
                SV_ClientPrintf("notarget ON\n");
            }
            else
            {
                quake::util::removeFlag(sv_player, FL_NOTARGET);
                SV_ClientPrintf("notarget OFF\n");
            }
            break;
        default:
            Con_Printf(
                "notarget [value] : toggle notarget mode. values: 0 = off, 1 = "
                "on\n");
            break;
    }
    // johnfitz
}

bool noclip_anglehack;

/*
==================
Host_Noclip_f
==================
*/
void Host_Noclip_f()
{
    if(cmd_source == src_command)
    {
        Cmd_ForwardToServer();
        return;
    }

    if(pr_global_struct->deathmatch)
    {
        return;
    }

    // johnfitz -- allow user to explicitly set noclip to on or off
    switch(Cmd_Argc())
    {
        case 1:
            if(sv_player->v.movetype != MOVETYPE_NOCLIP)
            {
                noclip_anglehack = true;
                sv_player->v.movetype = MOVETYPE_NOCLIP;
                SV_ClientPrintf("noclip ON\n");
            }
            else
            {
                noclip_anglehack = false;
                sv_player->v.movetype = MOVETYPE_WALK;
                SV_ClientPrintf("noclip OFF\n");
            }
            break;
        case 2:
            if(Q_atof(Cmd_Argv(1)))
            {
                noclip_anglehack = true;
                sv_player->v.movetype = MOVETYPE_NOCLIP;
                SV_ClientPrintf("noclip ON\n");
            }
            else
            {
                noclip_anglehack = false;
                sv_player->v.movetype = MOVETYPE_WALK;
                SV_ClientPrintf("noclip OFF\n");
            }
            break;
        default:
            Con_Printf(
                "noclip [value] : toggle noclip mode. values: 0 = off, 1 = "
                "on\n");
            break;
    }
    // johnfitz
}

/*
====================
Host_SetPos_f

adapted from fteqw, originally by Alex Shadowalker
====================
*/
void Host_SetPos_f()
{
    if(cmd_source == src_command)
    {
        Cmd_ForwardToServer();
        return;
    }

    if(pr_global_struct->deathmatch)
    {
        return;
    }

    if(Cmd_Argc() != 7 && Cmd_Argc() != 4)
    {
        SV_ClientPrintf("usage:\n");
        SV_ClientPrintf("   setpos <x> <y> <z>\n");
        SV_ClientPrintf("   setpos <x> <y> <z> <pitch> <yaw> <roll>\n");
        SV_ClientPrintf("current values:\n");
        SV_ClientPrintf("   %i %i %i %i %i %i\n", (int)sv_player->v.origin[0],
            (int)sv_player->v.origin[1], (int)sv_player->v.origin[2],
            (int)sv_player->v.v_angle[0], (int)sv_player->v.v_angle[1],
            (int)sv_player->v.v_angle[2]);
        return;
    }

    if(sv_player->v.movetype != MOVETYPE_NOCLIP)
    {
        noclip_anglehack = true;
        sv_player->v.movetype = MOVETYPE_NOCLIP;
        SV_ClientPrintf("noclip ON\n");
    }

    // make sure they're not going to whizz away from it
    sv_player->v.velocity[0] = 0;
    sv_player->v.velocity[1] = 0;
    sv_player->v.velocity[2] = 0;

    sv_player->v.origin[0] = atof(Cmd_Argv(1));
    sv_player->v.origin[1] = atof(Cmd_Argv(2));
    sv_player->v.origin[2] = atof(Cmd_Argv(3));

    if(Cmd_Argc() == 7)
    {
        sv_player->v.angles[0] = atof(Cmd_Argv(4));
        sv_player->v.angles[1] = atof(Cmd_Argv(5));
        sv_player->v.angles[2] = atof(Cmd_Argv(6));
        sv_player->v.fixangle = 1;
    }

    SV_LinkEdict(sv_player, false);
}

/*
==================
Host_Fly_f

Sets client to flymode
==================
*/
void Host_Fly_f()
{
    if(cmd_source == src_command)
    {
        Cmd_ForwardToServer();
        return;
    }

    if(pr_global_struct->deathmatch)
    {
        return;
    }

    // johnfitz -- allow user to explicitly set noclip to on or off
    switch(Cmd_Argc())
    {
        case 1:
            if(sv_player->v.movetype != MOVETYPE_FLY)
            {
                sv_player->v.movetype = MOVETYPE_FLY;
                SV_ClientPrintf("flymode ON\n");
            }
            else
            {
                sv_player->v.movetype = MOVETYPE_WALK;
                SV_ClientPrintf("flymode OFF\n");
            }
            break;
        case 2:
            if(Q_atof(Cmd_Argv(1)))
            {
                sv_player->v.movetype = MOVETYPE_FLY;
                SV_ClientPrintf("flymode ON\n");
            }
            else
            {
                sv_player->v.movetype = MOVETYPE_WALK;
                SV_ClientPrintf("flymode OFF\n");
            }
            break;
        default:
            Con_Printf(
                "fly [value] : toggle fly mode. values: 0 = off, 1 = on\n");
            break;
    }
    // johnfitz
}


/*
==================
Host_Ping_f

==================
*/
void Host_Ping_f()
{
    int i;
    int j;
    float total;
    client_t* client;

    if(cmd_source == src_command)
    {
        Cmd_ForwardToServer();
        return;
    }

    SV_ClientPrintf("Client ping times:\n");
    for(i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
    {
        if(!client->spawned || !client->netconnection) // QSS
        {
            continue;
        }

        total = 0;
        for(j = 0; j < NUM_PING_TIMES; j++)
        {
            total += client->ping_times[j];
        }
        total /= NUM_PING_TIMES;
        SV_ClientPrintf("%4i %s\n", (int)(total * 1000), client->name);
    }
}

/*
===============================================================================

SERVER TRANSITIONS

===============================================================================
*/


/*
======================
Host_Map_f

handle a
map <servername>
command from the console.  Active clients are kicked off.
======================
*/
void Host_Map_f()
{
    if(Cmd_Argc() < 2) // no map name given
    {
        if(cls.state == ca_dedicated)
        {
            if(sv.active)
            {
                Con_Printf("Current map: %s\n", sv.name);
            }
            else
            {
                Con_Printf("Server not active\n");
            }
        }
        else if(cls.state == ca_connected)
        {
            Con_Printf("Current map: %s ( %s )\n", cl.levelname, cl.mapname);
        }
        else
        {
            Con_Printf("map <levelname>: start a new server\n");
        }

        return;
    }

    if(cmd_source != src_command)
    {
        return;
    }

    cls.demonum = -1; // stop demo loop in case this fails

    CL_Disconnect();
    Host_ShutdownServer(false);

    key_dest = key_game; // remove console or menu

    if(cls.state != ca_dedicated)
    {
        IN_UpdateGrabs();
    }

    SCR_BeginLoadingPlaque();

    svs.serverflags = 0; // haven't completed an episode yet

    char name[MAX_QPATH];
    q_strlcpy(name, Cmd_Argv(1), sizeof(name));

    // remove (any) trailing ".bsp" from mapname -- S.A.
    char* p = strstr(name, ".bsp");
    if(p && p[4] == '\0')
    {
        *p = '\0';
    }

    PR_SwitchQCVM(&sv.qcvm);
    SV_SpawnServer(name, SpawnServerSrc::FromMapCmd);
    PR_SwitchQCVM(nullptr);

    if(!sv.active)
    {
        return;
    }

    if(cls.state != ca_dedicated)
    {
        memset(cls.spawnparms, 0, MAX_MAPSTRING);
        for(int i = 2; i < Cmd_Argc(); i++)
        {
            q_strlcat(cls.spawnparms, Cmd_Argv(i), MAX_MAPSTRING);
            q_strlcat(cls.spawnparms, " ", MAX_MAPSTRING);
        }

        Cmd_ExecuteString("connect local", src_command);
    }
}

/*
======================
Host_Randmap_f

Loads a random map from the "maps" list.
======================
*/
void Host_Randmap_f()
{
    int i;
    int randlevel;
    int numlevels;
    filelist_item_t* level;

    if(cmd_source != src_command)
    {
        return;
    }

    for(level = extralevels, numlevels = 0; level; level = level->next)
    {
        numlevels++;
    }

    if(numlevels == 0)
    {
        Con_Printf("no maps\n");
        return;
    }

    randlevel = (rand() % numlevels);

    for(level = extralevels, i = 0; level; level = level->next, i++)
    {
        if(i == randlevel)
        {
            Con_Printf("Starting map %s...\n", level->name);
            Cbuf_AddText(va("map %s\n", level->name));
            return;
        }
    }
}

/*
==================
Host_Changelevel_f

Goes to a new map, taking all clients along
==================
*/
void Host_Changelevel_f()
{
    if(Cmd_Argc() != 2)
    {
        Con_Printf("changelevel <levelname> : continue game on a new level\n");
        return;
    }

    if(!sv.active || cls.demoplayback)
    {
        Con_Printf("Only the server may changelevel\n");
        return;
    }

    // johnfitz -- check for client having map before anything else
    char level[MAX_QPATH];
    q_snprintf(level, sizeof(level), "maps/%s.bsp", Cmd_Argv(1));

    if(!COM_FileExists(level, nullptr))
    {
        Host_Error("cannot find map %s", level);
    }
    // johnfitz

    key_dest = key_game; // remove console or menu

    if(cls.state != ca_dedicated)
    {
        IN_UpdateGrabs(); // -- S.A.
    }

    PR_SwitchQCVM(&sv.qcvm);
    SV_SaveSpawnparms();

    q_strlcpy(level, Cmd_Argv(1), sizeof(level));
    SV_SpawnServer(level, SpawnServerSrc::FromChangelevelCmd);

    PR_SwitchQCVM(nullptr);

    // also issue an error if spawn failed -- O.S.
    if(!sv.active)
    {
        Host_Error("cannot run map %s", level);
    }
}

/*
==================
Host_Restart_f

Restarts the current server for a dead player
==================
*/
void Host_Restart_f()
{
    char mapname[MAX_QPATH];

    // QSS
    if(cls.demoplayback)
    {
        return;
    }

    if(cmd_source != src_command)
    {
        return;
    }

    // QSS
    if(!sv.active)
    {
        if(*sv.name)
        {
            Cmd_ExecuteString(va("map \"%s\"\n", sv.name), src_command);
        }

        return;
    }

    // mapname gets cleared in spawnserver
    q_strlcpy(mapname, sv.name, sizeof(mapname));

    PR_SwitchQCVM(&sv.qcvm);
    SV_SpawnServer(mapname, SpawnServerSrc::FromRestart);
    PR_SwitchQCVM(nullptr);

    if(!sv.active)
    {
        Host_Error("cannot restart map %s", mapname);
    }
}

/*
==================
Host_Reconnect_f

This command causes the client to wait for the signon messages again.
This is sent just before a server changes levels

QSS:
for compatibility with quakeworld et al, we also allow this as a user-command to
reconnect to the last server we tried, but we can only reliably do that when
we're not already connected
==================
*/
// QSS
void Host_Reconnect_Con_f()
{
    CL_Disconnect_f();
    cls.demonum = -1; // stop demo loop in case this fails

    if(cls.demoplayback)
    {
        CL_StopPlayback();
        CL_Disconnect();
    }

    CL_EstablishConnection(nullptr);
}

// QSS
void Host_Reconnect_Sv_f()
{
    if(cls.demoplayback) // cross-map demo playback fix from Baker
    {
        return;
    }

    SCR_BeginLoadingPlaque();

    cl.protocol_dpdownload = false;
    cls.signon = 0; // need new connection messages
}



// QSS
void Host_Lightstyle_f()
{
    CL_UpdateLightstyle(atoi(Cmd_Argv(1)), Cmd_Argv(2));
}


/*
=====================
Host_Connect_f

User command to connect to server
=====================
*/
void Host_Connect_f()
{
    char name[MAX_QPATH];

    cls.demonum = -1; // stop demo loop in case this fails
    if(cls.demoplayback)
    {
        CL_StopPlayback();
        CL_Disconnect();
    }
    q_strlcpy(name, Cmd_Argv(1), sizeof(name));
    CL_EstablishConnection(name);

    // QSS
    Host_Reconnect_Sv_f();
}


/*
===============================================================================

LOAD / SAVE GAME

===============================================================================
*/

#define SAVEGAME_VERSION 5

/*
===============
Host_SavegameComment

Writes a SAVEGAME_COMMENT_LENGTH character comment describing the current
===============
*/
void Host_SavegameComment(char* text)
{
    int i;
    char kills[20];

    for(i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
    {
        text[i] = ' ';
    }
    memcpy(text, cl.levelname,
        q_min(strlen(cl.levelname), 22)); // johnfitz -- only copy 22 chars.
    sprintf(kills, "kills:%3i/%3i", cl.stats[STAT_MONSTERS],
        cl.stats[STAT_TOTALMONSTERS]);
    memcpy(text + 22, kills, strlen(kills));
    // convert space to _ to make stdio happy
    for(i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
    {
        if(text[i] == ' ')
        {
            text[i] = '_';
        }
    }
    text[SAVEGAME_COMMENT_LENGTH] = '\0';
}

bool Host_MakeSavegame(
    const char* filename, const std::time_t* timestamp, const bool printMessage)
{
    if(!sv.active)
    {
        Con_Printf("Not playing a local game.\n");
        return false;
    }

    if(cl.intermission)
    {
        Con_Printf("Can't save in intermission.\n");
        return false;
    }

    if(svs.maxclients != 1)
    {
        Con_Printf("Can't save multiplayer games.\n");
        return false;
    }

    if(strstr(filename, ".."))
    {
        Con_Printf("Relative pathnames are not allowed.\n");
        return false;
    }

    for(int i = 0; i < svs.maxclients; i++)
    {
        if(svs.clients[i].active && (svs.clients[i].edict->v.health <= 0))
        {
            Con_Printf("Can't savegame with a dead player\n");
            return false;
        }
    }

    char name[MAX_OSPATH];
    q_snprintf(name, sizeof(name), "%s/%s", com_gamedir, filename);
    COM_AddExtension(name, ".sav", sizeof(name));

    if(printMessage)
    {
        Con_Printf("Saving game to %s...\n", name);
    }

    FILE* f = fopen(name, "w");
    if(!f)
    {
        Con_Printf("ERROR: couldn't open.\n");
        return false;
    }

    if(timestamp != nullptr)
    {
        std::tm* ptm = gmtime(timestamp);

        char buf[256];
        std::strftime(buf, sizeof(buf), "%F %T", ptm);

        fprintf(f, "%s\n", buf);
    }

    // QSS
    PR_SwitchQCVM(&sv.qcvm);

    fprintf(f, "%i\n", SAVEGAME_VERSION);
    char comment[SAVEGAME_COMMENT_LENGTH + 1];
    Host_SavegameComment(comment);
    fprintf(f, "%s\n", comment);

    for(int i = 0; i < NUM_BASIC_SPAWN_PARMS; i++) // QSS
    {
        fprintf(f, "%f\n", svs.clients->spawn_parms[i]);
    }

    fprintf(f, "%d\n", current_skill);
    fprintf(f, "%s\n", sv.name);
    fprintf(f, "%f\n", qcvm->time); // QSS

    // write the light styles

    for(int i = 0; i < MAX_LIGHTSTYLES_VANILLA; i++) // QSS
    {
        if(sv.lightstyles[i])
        {
            fprintf(f, "%s\n", sv.lightstyles[i]);
        }
        else
        {
            fprintf(f, "m\n");
        }
    }

    ED_WriteGlobals(f);
    for(int i = 0; i < qcvm->num_edicts; i++) // QSS
    {
        ED_Write(f, EDICT_NUM(i));
        fflush(f);
    }

    // QSS
    // add extra info (lightstyles, precaches, etc) in a way that's supposed to
    // be compatible with DP. sidenote - this provides extended lightstyles and
    // support for late precaches it does NOT protect against spawnfunc precache
    // changes - we would need to include makestatics here too (and optionally
    // baselines, or just recalculate those).
    fprintf(f, "/*\n");
    fprintf(f, "// QuakeSpasm extended savegame\n");
    int i = 0;
    for(i = MAX_LIGHTSTYLES_VANILLA; i < MAX_LIGHTSTYLES; i++)
    {
        if(sv.lightstyles[i])
        {
            fprintf(f, "sv.lightstyles %i \"%s\"\n", i, sv.lightstyles[i]);
        }
    }
    for(i = 1; i < MAX_MODELS; i++)
    {
        if(sv.model_precache[i])
        {
            fprintf(
                f, "sv.model_precache %i \"%s\"\n", i, sv.model_precache[i]);
        }
    }
    for(i = 1; i < MAX_SOUNDS; i++)
    {
        if(sv.sound_precache[i])
        {
            fprintf(
                f, "sv.sound_precache %i \"%s\"\n", i, sv.sound_precache[i]);
        }
    }
    for(i = 1; i < MAX_PARTICLETYPES; i++)
    {
        if(sv.particle_precache[i])
        {
            fprintf(f, "sv.particle_precache %i \"%s\"\n", i,
                sv.particle_precache[i]);
        }
    }

    for(i = NUM_BASIC_SPAWN_PARMS; i < NUM_TOTAL_SPAWN_PARMS; i++)
    {
        if(svs.clients->spawn_parms[i])
        {
            fprintf(
                f, "spawnparm %i \"%f\"\n", i + 1, svs.clients->spawn_parms[i]);
        }
    }

    fprintf(f, "*/\n");

    fclose(f);

    if(printMessage)
    {
        Con_Printf("done.\n");
    }

    PR_SwitchQCVM(nullptr);
    return true;
}


/*
===============
Host_Savegame_f
===============
*/
void Host_Savegame_f()
{
    if(cmd_source != src_command)
    {
        return;
    }

    if(Cmd_Argc() != 2)
    {
        Con_Printf("save <savename> : save a game\n");
        return;
    }

    Host_MakeSavegame(Cmd_Argv(1), nullptr, true);
}


bool Host_Loadgame(const char* filename, const bool hasTimestamp)
{
    if(strstr(filename, ".."))
    {
        Con_Printf("Relative pathnames are not allowed.\n");
        return false;
    }

    cls.demonum = -1; // stop demo loop in case this fails

    char name[MAX_OSPATH];
    q_snprintf(name, sizeof(name), "%s/%s", com_gamedir, filename);
    COM_AddExtension(name, ".sav", sizeof(name));

    // we can't call SCR_BeginLoadingPlaque, because too much stack space has
    // been used.  The menu calls it before stuffing loadgame command
    //	SCR_BeginLoadingPlaque ();

    Con_Printf("Loading game from %s...\n", name);

    // avoid leaking if the previous Host_Loadgame_f failed with a Host_Error
    static char* start;
    if(start != nullptr)
    {
        free(start);
    }

    start = (char*)COM_LoadMallocFile_TextMode_OSPath(name, nullptr);
    if(start == nullptr)
    {
        Con_Printf("ERROR: couldn't open.\n");
        return false;
    }

    const char* data;
    data = start;

    if(hasTimestamp)
    {
        data = COM_ParseTimestampNewline(data);
        if(data == nullptr)
        {
            return false;
        }
    }

    int version;
    data = COM_ParseIntNewline(data, &version);

    if(version != SAVEGAME_VERSION)
    {
        free(start);
        start = nullptr;
        Con_Printf(
            "Savegame is version %i, not %i\n", version, SAVEGAME_VERSION);
        return false;
    }

    float spawn_parms[NUM_TOTAL_SPAWN_PARMS]; // QSS

    data = COM_ParseStringNewline(data);

    int i;
    for(i = 0; i < NUM_BASIC_SPAWN_PARMS; i++) // QSS
    {
        data = COM_ParseFloatNewline(data, &spawn_parms[i]);
    }

    for(; i < NUM_TOTAL_SPAWN_PARMS; i++) // QSS
    {
        spawn_parms[i] = 0;
    }

    // this silliness is so we can load 1.06 save files, which have float skill
    // values
    float tfloat;
    data = COM_ParseFloatNewline(data, &tfloat);
    current_skill = (int)(tfloat + 0.1);
    Cvar_SetValue("skill", (float)current_skill);

    data = COM_ParseStringNewline(data);

    char mapname[MAX_QPATH];
    q_strlcpy(mapname, com_token, sizeof(mapname));

    float time;
    data = COM_ParseFloatNewline(data, &time);

    CL_Disconnect_f();

    PR_SwitchQCVM(&sv.qcvm);
    SV_SpawnServer(mapname, SpawnServerSrc::FromSaveFile);

    if(!sv.active)
    {
        PR_SwitchQCVM(nullptr);

        free(start);
        start = nullptr;
        Con_Printf("Couldn't load map\n");
        return false;
    }

    sv.paused = true; // pause until all clients connect
    sv.loadgame = true;

    // load the light styles

    for(i = 0; i < MAX_LIGHTSTYLES_VANILLA; i++)
    {
        data = COM_ParseStringNewline(data);
        sv.lightstyles[i] = (const char*)Hunk_Strdup(com_token, "lightstyles");
    }

    for(; i < MAX_LIGHTSTYLES; i++)
    {
        sv.lightstyles[i] = nullptr;
    }

    // load the edicts out of the savegame file
    int entnum = -1; // -1 is the globals
    while(*data)
    {
        while(*data == ' ' || *data == '\r' || *data == '\n')
        {
            data++;
        }

        if(data[0] == '/' && data[1] == '*' &&
            (data[2] == '\r' || data[2] == '\n'))
        { // looks like an extended saved game
            const char* end;
            const char* ext;
            ext = data + 2;
            while((end = strchr(ext, '\n')))
            {
                end = "\0";
                ext = COM_Parse(ext);
                if(!strcmp(com_token, "sv.lightstyles"))
                {
                    int idx;
                    ext = COM_Parse(ext);
                    idx = atoi(com_token);
                    ext = COM_Parse(ext);
                    if(idx >= 0 && idx < MAX_LIGHTSTYLES)
                    {
                        if(*com_token)
                            sv.lightstyles[idx] = (const char*)Hunk_Strdup(
                                com_token, "lightstyles");
                        else
                            sv.lightstyles[idx] = nullptr;
                    }
                }
                else if(!strcmp(com_token, "sv.model_precache"))
                {
                    int idx;
                    ext = COM_Parse(ext);
                    idx = atoi(com_token);
                    ext = COM_Parse(ext);
                    if(idx >= 1 && idx < MAX_MODELS)
                    {
                        sv.model_precache[idx] = (const char*)Hunk_Strdup(
                            com_token, "model_precache");
                        sv.models[idx] =
                            Mod_ForName(sv.model_precache[idx], idx == 1);
                        // if (idx == 1)
                        //	qcvm->worldmodel = sv.models[idx];
                    }
                }
                else if(!strcmp(com_token, "sv.sound_precache"))
                {
                    int idx;
                    ext = COM_Parse(ext);
                    idx = atoi(com_token);
                    ext = COM_Parse(ext);
                    if(idx >= 1 && idx < MAX_MODELS)
                        sv.sound_precache[idx] = (const char*)Hunk_Strdup(
                            com_token, "sound_precache");
                }
                else if(!strcmp(com_token, "sv.particle_precache"))
                {
                    int idx;
                    ext = COM_Parse(ext);
                    idx = atoi(com_token);
                    ext = COM_Parse(ext);
                    if(idx >= 1 && idx < MAX_PARTICLETYPES)
                        sv.particle_precache[idx] = (const char*)Hunk_Strdup(
                            com_token, "particle_precache");
                }
                else if(!strcmp(com_token, "spawnparm"))
                {
                    int idx;
                    ext = COM_Parse(ext);
                    idx = atoi(com_token);
                    ext = COM_Parse(ext);
                    if(idx >= 1 && idx <= NUM_TOTAL_SPAWN_PARMS)
                        spawn_parms[idx - 1] = atof(com_token);
                }
                end = "\n";
                ext = end + 1;
            }
        }

        data = COM_Parse(data);
        if(!com_token[0])
        {
            break; // end of file
        }
        if(strcmp(com_token, "{"))
        {
            Sys_Error("First token isn't a brace");
        }

        if(entnum == -1)
        {
            // parse the global vars
            data = ED_ParseGlobals(data);
        }
        else
        {
            // parse an edict
            edict_t* ent = EDICT_NUM(entnum);

            if(entnum < qcvm->num_edicts)
            {
                ent->free = false;
                memset(&ent->v, 0, qcvm->progs->entityfields * 4);
            }
            else
            {
                memset(ent, 0, qcvm->edict_size);
            }

            data = ED_ParseEdict(data, ent);

            // link it into the bsp tree
            if(!ent->free)
            {
                SV_LinkEdict(ent, false);
            }
        }

        entnum++;
    }

    qcvm->num_edicts = entnum;
    qcvm->time = time;

    free(start);
    start = nullptr;

    for(int i = 0; i < NUM_TOTAL_SPAWN_PARMS; i++)
    {
        svs.clients->spawn_parms[i] = spawn_parms[i];
    }

    if(cls.state != ca_dedicated)
    {
        CL_EstablishConnection("local");
        Host_Reconnect_Sv_f();
    }

    Con_DPrintf("Calling QC 'OnLoadGame'.\n");
    PR_ExecuteProgram(pr_global_struct->OnLoadGame);

    PR_SwitchQCVM(nullptr);
    return true;
}

/*
===============
Host_Loadgame_f
===============
*/
void Host_Loadgame_f()
{
    if(cmd_source != src_command)
    {
        return;
    }

    if(Cmd_Argc() != 2)
    {
        Con_Printf("load <savename> : load a game\n");
        return;
    }

    if(strstr(Cmd_Argv(1), ".."))
    {
        Con_Printf("Relative pathnames are not allowed.\n");
        return;
    }

    if(!Host_Loadgame(Cmd_Argv(1), false /* hasTimestamp */))
    {
        Con_Printf("Failed to load '%s'\n", Cmd_Argv(1));
    }
}

void Host_LoadAutosave_f()
{
    if(cmd_source != src_command)
    {
        return;
    }

    if(Cmd_Argc() != 2)
    {
        Con_Printf("load_autosave <savename> : load an autosave\n");
        return;
    }

    if(strstr(Cmd_Argv(1), ".."))
    {
        Con_Printf("Relative pathnames are not allowed.\n");
        return;
    }

    if(!Host_Loadgame(Cmd_Argv(1), true /* hasTimestamp */))
    {
        Con_Printf("Failed to load '%s'\n", Cmd_Argv(1));
    }
}

//============================================================================

/*
======================
Host_Name_f
======================
*/
void Host_Name_f()
{
    char newName[32];

    if(Cmd_Argc() == 1)
    {
        Con_Printf("\"name\" is \"%s\"\n", cl_name.string);
        return;
    }
    if(Cmd_Argc() == 2)
    {
        q_strlcpy(newName, Cmd_Argv(1), sizeof(newName));
    }
    else
    {
        q_strlcpy(newName, Cmd_Args(), sizeof(newName));
    }
    newName[15] = 0; // client_t structure actually says name[32].

    if(cmd_source == src_command)
    {
        if(Q_strcmp(cl_name.string, newName) == 0)
        {
            return;
        }
        Cvar_Set("_cl_name", newName);
        if(cls.state == ca_connected)
        {
            Cmd_ForwardToServer();
        }
        return;
    }

    if(host_client->name[0] && strcmp(host_client->name, "unconnected"))
    {
        if(Q_strcmp(host_client->name, newName) != 0)
        {
            Con_Printf("%s renamed to %s\n", host_client->name, newName);
        }
    }
    Q_strcpy(host_client->name, newName);
    host_client->edict->v.netname = PR_SetEngineString(host_client->name);

    // send notification to all clients

    MSG_WriteByte(&sv.reliable_datagram, svc_updatename);
    MSG_WriteByte(&sv.reliable_datagram, host_client - svs.clients);
    MSG_WriteString(&sv.reliable_datagram, host_client->name);
}

void Host_Say(bool teamonly)
{
    int j;
    client_t* client;
    client_t* save;
    const char* p;
    char text[MAXCMDLINE];
    char* p2;
    bool quoted;
    bool fromServer = false;

    if(cmd_source == src_command)
    {
        if(cls.state != ca_dedicated)
        {
            Cmd_ForwardToServer();
            return;
        }
        fromServer = true;
        teamonly = false;
    }

    if(Cmd_Argc() < 2)
    {
        return;
    }

    save = host_client;

    p = Cmd_Args();
    // remove quotes if present
    quoted = false;
    if(*p == '\"')
    {
        p++;
        quoted = true;
    }
    // turn on color set 1
    if(!fromServer)
    {
        q_snprintf(text, sizeof(text), "\001%s: %s", save->name, p);
    }
    else
    {
        q_snprintf(text, sizeof(text), "\001<%s> %s", hostname.string, p);
    }

    // check length & truncate if necessary
    j = (int)strlen(text);
    if(j >= (int)sizeof(text) - 1)
    {
        text[sizeof(text) - 2] = '\n';
        text[sizeof(text) - 1] = '\0';
    }
    else
    {
        p2 = text + j;
        while((const char*)p2 > (const char*)text &&
              (p2[-1] == '\r' || p2[-1] == '\n' || (p2[-1] == '\"' && quoted)))
        {
            if(p2[-1] == '\"' && quoted)
            {
                quoted = false;
            }
            p2[-1] = '\0';
            p2--;
        }
        p2[0] = '\n';
        p2[1] = '\0';
    }

    for(j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
    {
        if(!client || !client->active || !client->spawned)
        {
            continue;
        }
        if(teamplay.value && teamonly &&
            client->edict->v.team != save->edict->v.team)
        {
            continue;
        }
        host_client = client;
        SV_ClientPrintf("%s", text);
    }
    host_client = save;

    if(cls.state == ca_dedicated)
    {
        Sys_Printf("%s", &text[1]);
    }
}


void Host_Say_f()
{
    Host_Say(false);
}


void Host_Say_Team_f()
{
    Host_Say(true);
}


void Host_Tell_f()
{
    int j;
    client_t* client;
    client_t* save;
    const char* p;
    char text[MAXCMDLINE];
    char* p2;
    bool quoted;

    if(cmd_source == src_command)
    {
        Cmd_ForwardToServer();
        return;
    }

    if(Cmd_Argc() < 3)
    {
        return;
    }

    p = Cmd_Args();
    // remove quotes if present
    quoted = false;
    if(*p == '\"')
    {
        p++;
        quoted = true;
    }
    q_snprintf(text, sizeof(text), "%s: %s", host_client->name, p);

    // check length & truncate if necessary
    j = (int)strlen(text);
    if(j >= (int)sizeof(text) - 1)
    {
        text[sizeof(text) - 2] = '\n';
        text[sizeof(text) - 1] = '\0';
    }
    else
    {
        p2 = text + j;
        while((const char*)p2 > (const char*)text &&
              (p2[-1] == '\r' || p2[-1] == '\n' || (p2[-1] == '\"' && quoted)))
        {
            if(p2[-1] == '\"' && quoted)
            {
                quoted = false;
            }
            p2[-1] = '\0';
            p2--;
        }
        p2[0] = '\n';
        p2[1] = '\0';
    }

    save = host_client;
    for(j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
    {
        if(!client->active || !client->spawned)
        {
            continue;
        }
        if(q_strcasecmp(client->name, Cmd_Argv(1)))
        {
            continue;
        }
        host_client = client;
        SV_ClientPrintf("%s", text);
        break;
    }
    host_client = save;
}


/*
==================
Host_Color_f
==================
*/
void Host_Color_f()
{
    int top;
    int bottom;
    int playercolor;

    if(Cmd_Argc() == 1)
    {
        Con_Printf("\"color\" is \"%i %i\"\n", ((int)cl_color.value) >> 4,
            ((int)cl_color.value) & 0x0f);
        Con_Printf("color <0-13> [0-13]\n");
        return;
    }

    if(Cmd_Argc() == 2)
    {
        top = bottom = atoi(Cmd_Argv(1));
    }
    else
    {
        top = atoi(Cmd_Argv(1));
        bottom = atoi(Cmd_Argv(2));
    }

    top &= 15;
    if(top > 13)
    {
        top = 13;
    }
    bottom &= 15;
    if(bottom > 13)
    {
        bottom = 13;
    }

    playercolor = top * 16 + bottom;

    if(cmd_source == src_command)
    {
        Cvar_SetValue("_cl_color", playercolor);
        if(cls.state == ca_connected)
        {
            Cmd_ForwardToServer();
        }
        return;
    }

    host_client->colors = playercolor;
    host_client->edict->v.team = bottom + 1;

    // send notification to all clients
    MSG_WriteByte(&sv.reliable_datagram, svc_updatecolors);
    MSG_WriteByte(&sv.reliable_datagram, host_client - svs.clients);
    MSG_WriteByte(&sv.reliable_datagram, host_client->colors);
}

/*
==================
Host_Kill_f
==================
*/
void Host_Kill_f()
{
    if(cmd_source == src_command)
    {
        Cmd_ForwardToServer();
        return;
    }

    if(sv_player->v.health <= 0)
    {
        SV_ClientPrintf("Can't suicide -- allready dead!\n");
        return;
    }

    pr_global_struct->time = qcvm->time; // QSS

    pr_global_struct->self = EDICT_TO_PROG(sv_player);
    PR_ExecuteProgram(pr_global_struct->ClientKill);
}


/*
==================
Host_Pause_f
==================
*/
void Host_Pause_f()
{
    // ericw -- demo pause support (inspired by MarkV)
    if(cls.demoplayback)
    {
        cls.demopaused = !cls.demopaused;
        cl.paused = cls.demopaused;
        return;
    }

    if(cmd_source == src_command)
    {
        Cmd_ForwardToServer();
        return;
    }
    if(!pausable.value)
    {
        SV_ClientPrintf("Pause not allowed.\n");
    }
    else
    {
        sv.paused ^= 1;

        if(sv.paused)
        {
            SV_BroadcastPrintf(
                "%s paused the game\n", PR_GetString(sv_player->v.netname));
        }
        else
        {
            SV_BroadcastPrintf(
                "%s unpaused the game\n", PR_GetString(sv_player->v.netname));
        }

        // send notification to all clients
        MSG_WriteByte(&sv.reliable_datagram, svc_setpause);
        MSG_WriteByte(&sv.reliable_datagram, sv.paused);
    }
}

//===========================================================================


/*
==================
Host_PreSpawn_f
==================
*/
void Host_PreSpawn_f()
{
    if(cmd_source == src_command)
    {
        Con_Printf("prespawn is not valid from the console\n");
        return;
    }

    if(host_client->spawned)
    {
        Con_Printf("prespawn not valid -- already spawned\n");
        return;
    }

    // QSS
    // will start splurging out prespawn data
    host_client->sendsignon = 2;
    host_client->signonidx = 0;
}

/*
==================
Host_Spawn_f
==================
*/
void Host_Spawn_f()
{
    int i;
    client_t* client;
    edict_t* ent;

    if(cmd_source == src_command)
    {
        Con_Printf("spawn is not valid from the console\n");
        return;
    }

    if(host_client->spawned)
    {
        Con_Printf("Spawn not valid -- already spawned\n");
        return;
    }

    host_client->knowntoqc = true;

    // run the entrance script
    if(sv.loadgame)
    {
        // loaded games are fully inited allready
        // if this is the last client to be connected, unpause
        sv.paused = false;
    }
    else
    {
        // set up the edict
        ent = host_client->edict;

        memset(&ent->v, 0, qcvm->progs->entityfields * 4); // QSS

        ent->v.colormap = NUM_FOR_EDICT(ent);
        ent->v.team = (host_client->colors & 15) + 1;
        ent->v.netname = PR_SetEngineString(host_client->name);

        // copy spawn parms out of the client_t

        for(i = 0; i < NUM_BASIC_SPAWN_PARMS; i++) // QSS
        {
            (&pr_global_struct->parm1)[i] = host_client->spawn_parms[i];
        }

        // extended spawn parms
        for(; i < NUM_TOTAL_SPAWN_PARMS; i++)
        {
            ddef_t* g = ED_FindGlobal(va("parm%i", i + 1));
            if(g)
            {
                qcvm->globals[g->ofs] = host_client->spawn_parms[i];
            }
        }

        // call the spawn function
        pr_global_struct->time = qcvm->time; // QSS

        pr_global_struct->self = EDICT_TO_PROG(sv_player);
        PR_ExecuteProgram(pr_global_struct->ClientConnect);

        if((Sys_DoubleTime() -
               NET_QSocketGetTime(host_client->netconnection)) <=
            qcvm->time) // QSS
        {
            Sys_Printf("%s entered the game\n", host_client->name);
        }

        PR_ExecuteProgram(pr_global_struct->PutClientInServer);

        // VR: Force autosave on client spawn.
        if(!pr_global_struct->deathmatch && !pr_global_struct->coop)
        {
            qcvm_t* oldvm = qcvm;
            PR_SwitchQCVM(nullptr);

            quake::saveutil::doChangelevelAutosave();

            PR_SwitchQCVM(oldvm);
        }
    }


    // send all current names, colors, and frag counts
    SZ_Clear(&host_client->message);

    // send time of update
    MSG_WriteByte(&host_client->message, svc_time);
    MSG_WriteFloat(&host_client->message, qcvm->time);

    if(host_client->protocol_pext2 & PEXT2_PREDINFO)
    {
        MSG_WriteShort(
            &host_client->message, (host_client->lastmovemessage & 0xffff));
    }

    for(i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
    {
        if(!client->knowntoqc)
        {
            continue;
        }
        MSG_WriteByte(&host_client->message, svc_updatename);
        MSG_WriteByte(&host_client->message, i);
        MSG_WriteString(&host_client->message, client->name);

        MSG_WriteByte(&host_client->message, svc_updatefrags);
        MSG_WriteByte(&host_client->message, i);
        MSG_WriteShort(&host_client->message, client->old_frags);

        MSG_WriteByte(&host_client->message, svc_updatecolors);
        MSG_WriteByte(&host_client->message, i);
        MSG_WriteByte(&host_client->message, client->colors);
    }

    // send all current light styles
    for(i = 0; i < MAX_LIGHTSTYLES; i++)
    {
        // CL_ClearState should have cleared all lightstyles, so don't send
        // irrelevant ones
        if(sv.lightstyles[i])
        {
            if(i > 0xff)
            {
                MSG_WriteByte(&host_client->message, svc_stufftext);
                MSG_WriteString(&host_client->message,
                    va("//ls %i \"%s\"\n", i, sv.lightstyles[i]));
            }
            else
            {
                MSG_WriteByte(&host_client->message, svc_lightstyle);
                MSG_WriteByte(&host_client->message, i);
                MSG_WriteString(&host_client->message, sv.lightstyles[i]);
            }
        }
    }

    //
    // send some stats
    //
    MSG_WriteByte(&host_client->message, svc_updatestat);
    MSG_WriteByte(&host_client->message, STAT_TOTALSECRETS);
    MSG_WriteLong(&host_client->message, pr_global_struct->total_secrets);

    MSG_WriteByte(&host_client->message, svc_updatestat);
    MSG_WriteByte(&host_client->message, STAT_TOTALMONSTERS);
    MSG_WriteLong(&host_client->message, pr_global_struct->total_monsters);

    MSG_WriteByte(&host_client->message, svc_updatestat);
    MSG_WriteByte(&host_client->message, STAT_SECRETS);
    MSG_WriteLong(&host_client->message, pr_global_struct->found_secrets);

    MSG_WriteByte(&host_client->message, svc_updatestat);
    MSG_WriteByte(&host_client->message, STAT_MONSTERS);
    MSG_WriteLong(&host_client->message, pr_global_struct->killed_monsters);

    //
    // send a fixangle
    // Never send a roll angle, because savegames can catch the server
    // in a state where it is expecting the client to correct the angle
    // and it won't happen if the game was just loaded, so you wind up
    // with a permanent head tilt
    ent = EDICT_NUM(1 + (host_client - svs.clients));
    MSG_WriteByte(&host_client->message, svc_setangle);
    for(i = 0; i < 2; i++)
    {
        MSG_WriteAngle(
            &host_client->message, ent->v.angles[i], sv.protocolflags);
    }
    MSG_WriteAngle(&host_client->message, 0, sv.protocolflags);

    if(!(host_client->protocol_pext2 & PEXT2_REPLACEMENTDELTAS))
    {
        SV_WriteClientdataToMessage(host_client, &host_client->message);
    }

    MSG_WriteByte(&host_client->message, svc_signonnum);
    MSG_WriteByte(&host_client->message, 3);
    host_client->sendsignon = true;

    {
        WorldTextHandle wth = 0;
        for(const WorldText& wt : sv.worldTexts)
        {
            sv.SendMsg_WorldTextHMake(*host_client, wth);
            sv.SendMsg_WorldTextHSetText(*host_client, wth, wt._text.data());
            sv.SendMsg_WorldTextHSetPos(*host_client, wth, wt._pos);
            sv.SendMsg_WorldTextHSetAngles(*host_client, wth, wt._angles);
            sv.SendMsg_WorldTextHSetHAlign(*host_client, wth, wt._hAlign);

            ++wth;
        }
    }
}

/*
==================
Host_Begin_f
==================
*/
void Host_Begin_f()
{
    if(cmd_source == src_command)
    {
        Con_Printf("begin is not valid from the console\n");
        return;
    }

    host_client->spawned = true;
}

//===========================================================================


/*
==================
Host_Kick_f

Kicks a user off of the server
==================
*/
void Host_Kick_f()
{
    const char* who;
    const char* message = nullptr;
    client_t* save;
    int i;
    bool byNumber = false;

    if(cmd_source == src_command)
    {
        if(!sv.active)
        {
            Cmd_ForwardToServer();
            return;
        }
    }
    else if(pr_global_struct->deathmatch)
    {
        return;
    }

    save = host_client;

    if(Cmd_Argc() > 2 && Q_strcmp(Cmd_Argv(1), "#") == 0)
    {
        i = Q_atof(Cmd_Argv(2)) - 1;
        if(i < 0 || i >= svs.maxclients)
        {
            return;
        }
        if(!svs.clients[i].active)
        {
            return;
        }
        host_client = &svs.clients[i];
        byNumber = true;
    }
    else
    {
        for(i = 0, host_client = svs.clients; i < svs.maxclients;
            i++, host_client++)
        {
            if(!host_client->active)
            {
                continue;
            }
            if(q_strcasecmp(host_client->name, Cmd_Argv(1)) == 0)
            {
                break;
            }
        }
    }

    if(i < svs.maxclients)
    {
        if(cmd_source == src_command)
        {
            if(cls.state == ca_dedicated)
            {
                who = "Console";
            }
            else
            {
                who = cl_name.string;
            }
        }
        else
        {
            who = save->name;
        }

        // can't kick yourself!
        if(host_client == save)
        {
            return;
        }

        if(Cmd_Argc() > 2)
        {
            message = COM_Parse(Cmd_Args());
            if(byNumber)
            {
                message++; // skip the #
                while(*message == ' ')
                {
                    // skip white space
                    message++;
                }
                message += strlen(Cmd_Argv(2)); // skip the number
            }
            while(*message && *message == ' ')
            {
                message++;
            }
        }
        if(message)
        {
            SV_ClientPrintf("Kicked by %s: %s\n", who, message);
        }
        else
        {
            SV_ClientPrintf("Kicked by %s\n", who);
        }
        SV_DropClient(false);
    }

    host_client = save;
}

/*
===============================================================================

DEBUGGING TOOLS

===============================================================================
*/

/*
==================
Host_Give_f
==================
*/
void Host_Give_f()
{
    const char* t;
    int v;
    eval_t* val;

    if(cmd_source == src_command)
    {
        Cmd_ForwardToServer();
        return;
    }

    if(pr_global_struct->deathmatch)
    {
        return;
    }

    t = Cmd_Argv(1);
    v = atoi(Cmd_Argv(2));

    switch(t[0])
    {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            // MED 01/04/97 added hipnotic give stuff
            if(hipnotic)
            {
                if(t[0] == '6')
                {
                    if(t[1] == 'a')
                    {
                        sv_player->v.items =
                            (int)sv_player->v.items | HIT_PROXIMITY_GUN;
                    }
                    else
                    {
                        sv_player->v.items =
                            (int)sv_player->v.items | IT_GRENADE_LAUNCHER;
                    }
                }
                else if(t[0] == '9')
                {
                    sv_player->v.items =
                        (int)sv_player->v.items | HIT_LASER_CANNON;
                }
                else if(t[0] == '0')
                {
                    sv_player->v.items = (int)sv_player->v.items | HIT_MJOLNIR;
                }
                else if(t[0] >= '2')
                {
                    sv_player->v.items =
                        (int)sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
                }
            }
            else
            {
                if(t[0] >= '2')
                {
                    sv_player->v.items =
                        (int)sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
                }
            }
            break;

        case 's':
            if(rogue)
            {
                val = GetEdictFieldValue(
                    sv_player, ED_FindFieldOffset("ammo_shells1"));
                if(val)
                {
                    val->_float = v;
                }
            }
            sv_player->v.ammo_shells = v;
            break;

        case 'n':
            if(rogue)
            {
                val = GetEdictFieldValue(
                    sv_player, ED_FindFieldOffset("ammo_nails1"));
                if(val)
                {
                    val->_float = v;
                    if(sv_player->v.weapon <= IT_LIGHTNING)
                    {
                        sv_player->v.ammo_nails = v;
                    }
                }
            }
            else
            {
                sv_player->v.ammo_nails = v;
            }
            break;

        case 'l':
            if(rogue)
            {
                val = GetEdictFieldValue(
                    sv_player, ED_FindFieldOffset("ammo_lava_nails"));
                if(val)
                {
                    val->_float = v;
                    if(sv_player->v.weapon > IT_LIGHTNING)
                    {
                        sv_player->v.ammo_nails = v;
                    }
                }
            }
            break;

        case 'r':
            if(rogue)
            {
                val = GetEdictFieldValue(
                    sv_player, ED_FindFieldOffset("ammo_rockets1"));
                if(val)
                {
                    val->_float = v;
                    if(sv_player->v.weapon <= IT_LIGHTNING)
                    {
                        sv_player->v.ammo_rockets = v;
                    }
                }
            }
            else
            {
                sv_player->v.ammo_rockets = v;
            }
            break;

        case 'm':
            if(rogue)
            {
                val = GetEdictFieldValue(
                    sv_player, ED_FindFieldOffset("ammo_multi_rockets"));
                if(val)
                {
                    val->_float = v;
                    if(sv_player->v.weapon > IT_LIGHTNING)
                    {
                        sv_player->v.ammo_rockets = v;
                    }
                }
            }
            break;

        case 'h': sv_player->v.health = v; break;

        case 'c':
            if(rogue)
            {
                val = GetEdictFieldValue(
                    sv_player, ED_FindFieldOffset("ammo_cells1"));
                if(val)
                {
                    val->_float = v;
                    if(sv_player->v.weapon <= IT_LIGHTNING)
                    {
                        sv_player->v.ammo_cells = v;
                    }
                }
            }
            else
            {
                sv_player->v.ammo_cells = v;
            }
            break;

        case 'p':
            if(rogue)
            {
                val = GetEdictFieldValue(
                    sv_player, ED_FindFieldOffset("ammo_plasma"));
                if(val)
                {
                    val->_float = v;
                    if(sv_player->v.weapon > IT_LIGHTNING)
                    {
                        sv_player->v.ammo_cells = v;
                    }
                }
            }
            break;

        // johnfitz -- give armour
        case 'a':
            if(v > 150)
            {
                sv_player->v.armortype = 0.8;
                sv_player->v.armorvalue = v;
                sv_player->v.items =
                    sv_player->v.items -
                    ((int)(sv_player->v.items) &
                        (int)(IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) +
                    IT_ARMOR3;
            }
            else if(v > 100)
            {
                sv_player->v.armortype = 0.6;
                sv_player->v.armorvalue = v;
                sv_player->v.items =
                    sv_player->v.items -
                    ((int)(sv_player->v.items) &
                        (int)(IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) +
                    IT_ARMOR2;
            }
            else if(v >= 0)
            {
                sv_player->v.armortype = 0.3;
                sv_player->v.armorvalue = v;
                sv_player->v.items =
                    sv_player->v.items -
                    ((int)(sv_player->v.items) &
                        (int)(IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) +
                    IT_ARMOR1;
            }
            break;
            // johnfitz
    }

    // TODO VR: (P2) docs
    const auto updateAmmoCounter =
        [&](const float currentAmmo, float& ammoCounter)
    {
        switch((int)(currentAmmo))
        {
            case AID_NONE:
            {
                ammoCounter = 0;
                break;
            }

            case AID_SHELLS:
            {
                ammoCounter = sv_player->v.ammo_shells;
                break;
            }

            case AID_NAILS:
            {
                ammoCounter = sv_player->v.ammo_nails;
                break;
            }

            case AID_ROCKETS:
            {
                ammoCounter = sv_player->v.ammo_rockets;
                break;
            }

            case AID_CELLS:
            {
                ammoCounter = sv_player->v.ammo_cells;
                break;
            }

            case AID_LAVA_NAILS:
            {
                ammoCounter = GetEdictFieldValue(
                    sv_player, ED_FindFieldOffset("ammo_lava_nails"))
                                  ->_float;
                break;
            }

            case AID_MULTI_ROCKETS:
            {
                ammoCounter = GetEdictFieldValue(
                    sv_player, ED_FindFieldOffset("ammo_multi_rockets"))
                                  ->_float;
                break;
            }

            case AID_PLASMA:
            {
                ammoCounter = GetEdictFieldValue(
                    sv_player, ED_FindFieldOffset("ammo_plasma"))
                                  ->_float;
                break;
            }

            default:
            {
                assert(false);
            }
        }
    };

    updateAmmoCounter(sv_player->v.currentammo, sv_player->v.ammocounter);
    updateAmmoCounter(sv_player->v.currentammo2, sv_player->v.ammocounter2);
}

edict_t* FindViewthing()
{
    int i;
    edict_t* e = nullptr;

    // QSS
    PR_SwitchQCVM(&sv.qcvm);
    i = qcvm->num_edicts;

    if(i == qcvm->num_edicts)
    {
        for(i = 0; i < qcvm->num_edicts; i++)
        {
            e = EDICT_NUM(i);
            if(!strcmp(PR_GetString(e->v.classname), "viewthing"))
            {
                break;
            }
        }
    }

    if(i == qcvm->num_edicts)
    {
        for(i = 0; i < qcvm->num_edicts; i++)
        {
            e = EDICT_NUM(i);
            if(!strcmp(PR_GetString(e->v.classname), "info_player_start"))
            {
                break;
            }
        }
    }

    if(i == qcvm->num_edicts)
    {
        e = nullptr;
        Con_Printf("No viewthing on map\n");
    }

    PR_SwitchQCVM(nullptr);
    return e;
}

/*
==================
Host_Viewmodel_f
==================
*/
void Host_Viewmodel_f()
{
    edict_t* e;
    qmodel_t* m;

    e = FindViewthing();
    if(!e)
    {
        return;
    }

    if(!*Cmd_Argv(1)) // QSS
    {
        m = nullptr;
    }
    else
    {
        m = Mod_ForName(Cmd_Argv(1), false);
        if(!m)
        {
            Con_Printf("Can't load %s\n", Cmd_Argv(1));
            return;
        }
    }

    // QSS
    PR_SwitchQCVM(&sv.qcvm);
    e->v.modelindex = m ? SV_Precache_Model(m->name) : 0;
    e->v.model = PR_SetEngineString(sv.model_precache[(int)e->v.modelindex]);
    e->v.frame = 0;
    PR_SwitchQCVM(nullptr);
}

/*
==================
Host_Viewframe_f
==================
*/
void Host_Viewframe_f()
{
    edict_t* e;
    int f;
    qmodel_t* m;

    e = FindViewthing();
    if(!e)
    {
        return;
    }
    m = cl.model_precache[(int)e->v.modelindex];

    if(m) // QSS
    {
        f = atoi(Cmd_Argv(1));
        if(f >= m->numframes)
        {
            f = m->numframes - 1;
        }

        e->v.frame = f;
    }
}


void PrintFrameName(qmodel_t* m, int frame)
{
    aliashdr_t* hdr;
    maliasframedesc_t* pframedesc;

    hdr = (aliashdr_t*)Mod_Extradata(m);
    if(!hdr || m->type != mod_alias /* QSS */)
    {
        return;
    }
    pframedesc = &hdr->frames[frame];

    Con_Printf("frame %i: %s\n", frame, pframedesc->name);
}

/*
==================
Host_Viewnext_f
==================
*/
void Host_Viewnext_f()
{
    edict_t* e;
    qmodel_t* m;

    e = FindViewthing();
    if(!e)
    {
        return;
    }
    m = cl.model_precache[(int)e->v.modelindex];

    if(m) // QSS
    {
        e->v.frame += 1;
        if(e->v.frame >= m->numframes)
        {
            e->v.frame = m->numframes - 1;
        }

        PrintFrameName(m, e->v.frame);
    }
}

/*
==================
Host_Viewprev_f
==================
*/
void Host_Viewprev_f()
{
    edict_t* e = FindViewthing();
    if(!e)
    {
        return;
    }

    qmodel_t* m = cl.model_precache[(int)e->v.modelindex];

    if(m) // QSS
    {
        e->v.frame -= 1;
        if(e->v.frame < 0)
        {
            e->v.frame = 0;
        }

        PrintFrameName(m, e->v.frame);
    }
}

/*
===============================================================================

DEMO LOOP CONTROL

===============================================================================
*/


/*
==================
Host_Startdemos_f
==================
*/
void Host_Startdemos_f()
{
    int i;
    int c;

    if(cls.state == ca_dedicated)
    {
        return;
    }

    c = Cmd_Argc() - 1;
    if(c > MAX_DEMOS)
    {
        Con_Printf("Max %i demos in demoloop\n", MAX_DEMOS);
        c = MAX_DEMOS;
    }
    Con_Printf("%i demo(s) in loop\n", c);

    for(i = 1; i < c + 1; i++)
    {
        q_strlcpy(cls.demos[i - 1], Cmd_Argv(i), sizeof(cls.demos[0]));
    }

    if(!sv.active && cls.demonum != -1 && !cls.demoplayback)
    {
        cls.demonum = 0;

        if(vr_enabled.value)
        {
            // Start a new game when vr_enabled
            Cbuf_AddText("maxplayers 1\n");
            Cbuf_AddText("deathmatch 0\n");
            Cbuf_AddText("coop 0\n");
            Cbuf_AddText("map start\n");
            Cbuf_AddText("centerview\n");
        }

        /*
        else if(!fitzmode)
        {
            // QuakeSpasm customization:
            // go straight to menu, no CL_NextDemo
            cls.demonum = -1;
            Cbuf_InsertText("menu_main\n");
            return;
        }
        */

        CL_NextDemo();
    }
    else
    {
        cls.demonum = -1;
    }
}


/*
==================
Host_Demos_f

Return to looping demos
==================
*/
void Host_Demos_f()
{
    if(cls.state == ca_dedicated)
    {
        return;
    }
    if(cls.demonum == -1)
    {
        cls.demonum = 1;
    }
    CL_Disconnect_f();
    CL_NextDemo();
}

/*
==================
Host_Stopdemo_f

Return to looping demos
==================
*/
void Host_Stopdemo_f()
{
    if(cls.state == ca_dedicated)
    {
        return;
    }
    if(!cls.demoplayback)
    {
        return;
    }
    CL_StopPlayback();
    CL_Disconnect();
}

//=============================================================================
// QSS
// download stuff

static void Host_Download_f()
{
    const char* fname = Cmd_Argv(1);
    int fsize;
    if(cmd_source == src_command)
    {
        // FIXME: add some sort of queuing thing
        //		if (cls.state == ca_connected)
        //			Cmd_ForwardToServer ();
        return;
    }
    else if(cmd_source == src_client)
    {
        if(host_client->download.file)
        { // abort the current download if the previous didn't terminate
          // properly.
            SV_ClientPrintf("cancelling previous download\n");
            MSG_WriteByte(&host_client->message, svc_stufftext);
            MSG_WriteString(&host_client->message, "\nstopdownload\n");
            fclose(host_client->download.file);
            host_client->download.file = nullptr;
        }

        host_client->download.size = 0;
        host_client->download.started = false;
        host_client->download.sendpos = 0;
        host_client->download.ackpos = 0;

        fsize = -1;
        if(!COM_DownloadNameOkay(fname))
            SV_ClientPrintf(
                "refusing download of %s - restricted filename\n", fname);
        else
        {
            fsize = COM_FOpenFile(fname, &host_client->download.file, nullptr);
            if(!host_client->download.file)
                SV_ClientPrintf("server does not have file %s\n", fname);
            else if(file_from_pak)
            {
                SV_ClientPrintf(
                    "refusing download of %s from inside pak\n", fname);
                fclose(host_client->download.file);
                host_client->download.file = nullptr;
            }
            else if(fsize < 0 || fsize > 50 * 1024 * 1024)
            {
                SV_ClientPrintf("refusing download of large file %s\n", fname);
                fclose(host_client->download.file);
                host_client->download.file = nullptr;
            }
        }

        host_client->download.size = (unsigned int)fsize;
        if(host_client->download.file)
        {
            host_client->download.startpos = ftell(host_client->download.file);
            Con_Printf("downloading %s to %s\n", fname, host_client->name);
            MSG_WriteByte(&host_client->message, svc_stufftext);
            MSG_WriteString(
                &host_client->message, va("\ncl_downloadbegin %u \"%s\"\n",
                                           host_client->download.size, fname));
            q_strlcpy(host_client->download.name, fname,
                sizeof(host_client->download.name));
        }
        else
        {
            Con_Printf(
                "refusing download of %s to %s\n", fname, host_client->name);
            MSG_WriteByte(&host_client->message, svc_stufftext);
            MSG_WriteString(&host_client->message, "\nstopdownload\n");
        }
        host_client->sendsignon = true; // override any keepalive issues.
    }
}

static void Host_StartDownload_f()
{
    if(cmd_source != src_client) return;
    if(host_client->download.file)
        host_client->download.started = true;
    else
        SV_ClientPrintf("no download started\n");
}
// just writes download data onto the end of the outgoing unreliable buffer
void Host_AppendDownloadData(client_t* client, sizebuf_t* buf)
{
    if(buf->cursize + 7 > buf->maxsize) return; // no space for anything
    if(client->download.file && client->download.started)
    {
        byte tbuf[1400]; // don't be too aggressive, ethernet mtu is about 1450
        unsigned int size = client->download.size - client->download.sendpos;
        // size might be 0 at eof, and that's needed to avoid failure if we drop
        // the last few packets
        if(size > sizeof(tbuf)) size = sizeof(tbuf);
        if((int)size > buf->maxsize - (buf->cursize + 7))
            size = (int)(buf->maxsize - (buf->cursize + 7)); // don't overflow

        if(size && fread(tbuf, 1, size, host_client->download.file) < size)
            client->download.ackpos = client->download.sendpos =
                client->download.size; // some kind of error...
        else
        {
            MSG_WriteByte(buf, svcdp_downloaddata);
            MSG_WriteLong(buf, client->download.sendpos);
            MSG_WriteShort(buf, size);
            SZ_Write(buf, tbuf, size);
            client->download.sendpos += size;
        }
    }
}
// parses incoming acks from the client, so we know which parts of the file the
// client actually received.
void Host_DownloadAck(client_t* client)
{
    unsigned int start = MSG_ReadLong();
    unsigned int size = (unsigned short)MSG_ReadShort();

    if(!client->download.started || !client->download.file) return;

    if(client->download.ackpos < start)
    {
        client->download.sendpos =
            client->download.ackpos; // there was a gap, rewind to the known gap
        fseek(client->download.file,
            host_client->download.startpos + client->download.sendpos,
            SEEK_SET);
    }
    else if(client->download.ackpos < start + size)
        client->download.ackpos = start + size; // no loss yet.
    // else FIXME: build a log of parts known to be acked to avoid resending
    // them later, skip past them in acks

    if(client->download.ackpos == client->download.size)
    {
        unsigned int hash = 0;
        byte* data;
        client->download.started = false;

        data = (byte*)malloc(client->download.size);
        if(data)
        {
            fseek(client->download.file, host_client->download.startpos,
                SEEK_SET);
            fread(data, 1, host_client->download.size, client->download.file);
            hash = CRC_Block(data, host_client->download.size);
            free(data);
        }
        fclose(client->download.file);
        client->download.file = nullptr;

        MSG_WriteByte(&host_client->message, svc_stufftext);
        MSG_WriteString(&host_client->message,
            va("cl_downloadfinished %u %u \"%s\"\n", client->download.size,
                hash, client->download.name));
        *client->download.name = 0;
        host_client->sendsignon = true; // override any keepalive issues.
    }
}

//=============================================================================

/*
==================
Host_InitCommands
==================
*/
void Host_InitCommands()
{
    Cmd_AddCommand("maps", Host_Maps_f);       // johnfitz
    Cmd_AddCommand("mods", Host_Mods_f);       // johnfitz
    Cmd_AddCommand("games", Host_Mods_f);      // alias to "mods"
    Cmd_AddCommand("mapname", Host_Mapname_f); // johnfitz
    Cmd_AddCommand("randmap", Host_Randmap_f); // ericw

    Cmd_AddCommand_ClientCommand("status", Host_Status_f); // QSS
    Cmd_AddCommand("quit", Host_Quit_f);
    Cmd_AddCommand_ClientCommand("god", Host_God_f);           // QSS
    Cmd_AddCommand_ClientCommand("notarget", Host_Notarget_f); // QSS
    Cmd_AddCommand_ClientCommand("fly", Host_Fly_f);           // QSS
    Cmd_AddCommand("map", Host_Map_f);
    Cmd_AddCommand("restart", Host_Restart_f);
    Cmd_AddCommand("changelevel", Host_Changelevel_f);
    Cmd_AddCommand("connect", Host_Connect_f);
    Cmd_AddCommand_Console("reconnect", Host_Reconnect_Con_f);      // QSS
    Cmd_AddCommand_ServerCommand("reconnect", Host_Reconnect_Sv_f); // QSS
    Cmd_AddCommand_ServerCommand("ls", Host_Lightstyle_f);          // QSS
    Cmd_AddCommand_ClientCommand("name", Host_Name_f);              // QSS
    Cmd_AddCommand_ClientCommand("noclip", Host_Noclip_f);          // QSS
    Cmd_AddCommand_ClientCommand("setpos", Host_SetPos_f); // QuakeSpasm // QSS

    Cmd_AddCommand_ClientCommand("say", Host_Say_f);           // QSS
    Cmd_AddCommand_ClientCommand("say_team", Host_Say_Team_f); // QSS
    Cmd_AddCommand_ClientCommand("tell", Host_Tell_f);         // QSS
    Cmd_AddCommand_ClientCommand("color", Host_Color_f);       // QSS
    Cmd_AddCommand_ClientCommand("kill", Host_Kill_f);         // QSS
    Cmd_AddCommand_ClientCommand("pause", Host_Pause_f);       // QSS
    Cmd_AddCommand_ClientCommand("spawn", Host_Spawn_f);       // QSS
    Cmd_AddCommand_ClientCommand("begin", Host_Begin_f);       // QSS
    Cmd_AddCommand_ClientCommand("prespawn", Host_PreSpawn_f); // QSS
    Cmd_AddCommand_ClientCommand("kick", Host_Kick_f);         // QSS
    Cmd_AddCommand_ClientCommand("ping", Host_Ping_f);         // QSS
    Cmd_AddCommand("load", Host_Loadgame_f);
    Cmd_AddCommand("load_autosave", Host_LoadAutosave_f);
    Cmd_AddCommand("save", Host_Savegame_f);
    Cmd_AddCommand_ClientCommand("give", Host_Give_f);         // QSS
    Cmd_AddCommand_ClientCommand("download", Host_Download_f); // QSS
    Cmd_AddCommand_ClientCommand(
        "sv_startdownload", Host_StartDownload_f); // QSS

    Cmd_AddCommand("startdemos", Host_Startdemos_f);
    Cmd_AddCommand("demos", Host_Demos_f);
    Cmd_AddCommand("stopdemo", Host_Stopdemo_f);

    Cmd_AddCommand("viewmodel", Host_Viewmodel_f);
    Cmd_AddCommand("viewframe", Host_Viewframe_f);
    Cmd_AddCommand("viewnext", Host_Viewnext_f);
    Cmd_AddCommand("viewprev", Host_Viewprev_f);

    Cmd_AddCommand("mcache", Mod_Print);
}
