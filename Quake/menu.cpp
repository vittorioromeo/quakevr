/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
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
#include "menu.hpp"
#include "console.hpp"
#include "cvar.hpp"
#include "quakedef.hpp"
#include "bgmusic.hpp"
#include "menu_util.hpp"
#include "server.hpp"
#include "util.hpp"
#include "vr.hpp"
#include "vr_cvars.hpp"
#include "vr_showfn.hpp"
#include "cmd.hpp"
#include "qpic.hpp"
#include "net.hpp"
#include "keys.hpp"
#include "saveutil.hpp"
#include "draw.hpp"
#include "client.hpp"
#include "input.hpp"
#include "screen.hpp"
#include "q_sound.hpp"
#include "vid.hpp"
#include "view.hpp"
#include "gl_util.hpp"
#include "menu_keyboard.hpp"

#include <string>
#include <SDL2/SDL_mouse.h>
#include <string_view>
#include <cassert>
#include <array>
#include <tuple>
#include <cstddef>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <variant>

void (*vid_menucmdfn)(); // johnfitz
void (*vid_menudrawfn)();
void (*vid_menukeyfn)(int key);

enum m_state_e m_state;

void M_Menu_SinglePlayer_f();
void M_Menu_NewGame_f(const char* map);
void M_Menu_Load_f();
void M_Menu_Save_f();
void M_Menu_MultiPlayer_f();
void M_Menu_Setup_f();
void M_Menu_Net_f();
void M_Menu_LanConfig_f();
void M_Menu_GameOptions_f();
void M_Menu_Search_f(enum slistScope_e scope); // QSS
void M_Menu_ServerList_f();

void M_Menu_Keys_f();
void M_Menu_Video_f();
void M_Menu_Help_f();

void M_Main_Draw();
void M_SinglePlayer_Draw();
void M_Load_Draw();
void M_Save_Draw();
void M_MultiPlayer_Draw();
void M_Setup_Draw();
void M_Net_Draw();
void M_LanConfig_Draw();
void M_GameOptions_Draw();
void M_Search_Draw();
void M_ServerList_Draw();
void M_Options_Draw();
void M_Keys_Draw();
void M_Video_Draw();
void M_Help_Draw();
void M_Quit_Draw();

void M_Main_Key(int key);
void M_SinglePlayer_Key(int key);
void M_Load_Key(int key);
void M_Save_Key(int key);
void M_MultiPlayer_Key(int key);
void M_Setup_Key(int key);
void M_Net_Key(int key);
void M_LanConfig_Key(int key);
void M_GameOptions_Key(int key);
void M_Search_Key(int key);
void M_ServerList_Key(int key);
void M_Options_Key(int key);
void M_Keys_Key(int key);
void M_Video_Key(int key);
void M_Help_Key(int key);
void M_Quit_Key(int key);

bool m_entersound; // play after drawing a frame, so caching
                   // won't disrupt the sound
bool m_recursiveDraw;

enum m_state_e m_return_state;
bool m_return_onerror;
char m_return_reason[32];

// TODO VR: (P2) hackish
#define StartingGame ((multiPlayerMenu().cursor_idx()) == 1)
#define JoiningGame ((multiPlayerMenu().cursor_idx()) == 0)

void M_ConfigureNetSubsystem();

[[nodiscard]] static quake::menu& quakeVRQuickSettingsMenu();

/*
================
M_DrawCharacter

Draws one solid graphics character
================
*/
void M_DrawCharacter(int cx, int line, int num)
{
    Draw_Character(cx, line, num);
}

void M_Print(int cx, int cy, const char* str)
{
    while(*str)
    {
        M_DrawCharacter(cx, cy, (*str) + 128);
        str++;
        cx += 8;
    }
}


void M_PrintWithNewLine(int cx, int cy, const char* str)
{
    const int originalCx = cx;

    while(*str)
    {
        if(*str == '\n')
        {
            cx = originalCx;
            cy += 8;
            str++;

            continue;
        }

        M_DrawCharacter(cx, cy, (*str) + 128);
        str++;
        cx += 8;
    }
}

void M_PrintWhite(int cx, int cy, const char* str)
{
    while(*str)
    {
        M_DrawCharacter(cx, cy, *str);
        str++;
        cx += 8;
    }
}

void M_PrintWhiteWithNewLine(int cx, int cy, const char* str)
{
    const int originalCx = cx;

    while(*str)
    {
        if(*str == '\n')
        {
            cx = originalCx;
            cy += 8;
            str++;

            continue;
        }

        M_DrawCharacter(cx, cy, *str);
        str++;
        cx += 8;
    }
}

void M_PrintWhiteByWrapping(
    const int wrapCount, int cx, int cy, const char* str)
{
    const int originalCx = cx;
    int currPrint = 0;

    while(*str)
    {
        if(*str == '\n')
        {
            cx = originalCx;
            cy += 8;
            str++;
            currPrint = 0;

            continue;
        }

        // TODO VR: (P2) code repetition
        // TODO VR: (P2) improve wrapping logic (find word ends?)
        if(currPrint >= wrapCount)
        {
            cx = originalCx;
            cy += 8;
            currPrint = 0;

            continue;
        }

        M_DrawCharacter(cx, cy, *str);
        ++currPrint;

        str++;
        cx += 8;
    }
}

void M_DrawTransPic(int x, int y, qpic_t* pic)
{
    // johnfitz -- simplified becuase centering is handled elsewhere
    Draw_Pic(x, y, pic);
}

void M_DrawPic(int x, int y, qpic_t* pic)
{
    // johnfitz -- simplified becuase centering is handled elsewhere
    Draw_Pic(x, y, pic);
}

void M_DrawTransPicTranslate(int x, int y, qpic_t* pic, int top,
    int bottom) // johnfitz -- more parameters
{
    // johnfitz -- simplified becuase centering is handled elsewhere
    Draw_TransPicTranslate(x, y, pic, top, bottom);
}

void M_DrawTextBox(int x, int y, int width, int lines)
{
    int cx = x;
    int cy = y;

    // draw left side
    qpic_t* p = Draw_CachePic("gfx/box_tl.lmp");
    M_DrawTransPic(cx, cy, p);

    p = Draw_CachePic("gfx/box_ml.lmp");
    for(int n = 0; n < lines; n++)
    {
        cy += 8;
        M_DrawTransPic(cx, cy, p);
    }

    p = Draw_CachePic("gfx/box_bl.lmp");
    M_DrawTransPic(cx, cy + 8, p);

    // draw middle
    cx += 8;
    while(width > 0)
    {
        cy = y;

        p = Draw_CachePic("gfx/box_tm.lmp");
        M_DrawTransPic(cx, cy, p);

        p = Draw_CachePic("gfx/box_mm.lmp");
        for(int n = 0; n < lines; n++)
        {
            cy += 8;
            if(n == 1)
            {
                p = Draw_CachePic("gfx/box_mm2.lmp");
            }
            M_DrawTransPic(cx, cy, p);
        }

        p = Draw_CachePic("gfx/box_bm.lmp");
        M_DrawTransPic(cx, cy + 8, p);

        width -= 2;
        cx += 16;
    }

    // draw right side
    cy = y;

    p = Draw_CachePic("gfx/box_tr.lmp");
    M_DrawTransPic(cx, cy, p);

    p = Draw_CachePic("gfx/box_mr.lmp");
    for(int n = 0; n < lines; n++)
    {
        cy += 8;
        M_DrawTransPic(cx, cy, p);
    }

    p = Draw_CachePic("gfx/box_br.lmp");
    M_DrawTransPic(cx, cy + 8, p);
}

//=============================================================================

int m_save_demonum;

/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f()
{
    m_entersound = true;

    if(key_dest == key_menu)
    {
        if(m_state != m_main)
        {
            M_Menu_Main_f();
            return;
        }

        key_dest = key_game;
        m_state = m_none;

        IN_UpdateGrabs(); // QSS
        return;
    }

    if(key_dest == key_console)
    {
        Con_ToggleConsole_f();
    }
    else
    {
        M_Menu_Main_f();
    }
}

//=============================================================================
/* MAIN MENU */

[[nodiscard]] static quake::menu makeMainMenu()
{
    quake::menu m{"Main Menu", [] {}};

    m.add_action_entry("Single Player", &M_Menu_SinglePlayer_f);
    m.add_action_entry("Multi Player & Bots", &M_Menu_MultiPlayer_f);
    m.add_action_entry("Options", &M_Menu_Options_f);
    // m.add_action_entry("Quake VR - Quick Settings",
    // &M_Menu_QuakeVRQuickSettings_f);
    m.add_action_entry("Quake VR - Settings", &M_Menu_QuakeVRSettings_f);
    m.add_action_entry("Quake VR - Dev Tools", &M_Menu_QuakeVRDevTools_f);
    m.add_action_entry("Quake VR - Change Map", &M_Menu_QuakeVRChangeMap_f);
    m.add_action_entry("Help/Ordering", &M_Menu_Help_f);
    m.add_action_entry("Quit", &M_Menu_Quit_f);

    return m;
}

[[nodiscard]] static quake::menu& mainMenu()
{
    static quake::menu res = makeMainMenu();
    return res;
}

void M_Menu_Main_f()
{
    if(key_dest != key_menu)
    {
        m_save_demonum = cls.demonum;
        cls.demonum = -1;
    }

    key_dest = key_menu;
    m_state = m_main;
    m_entersound = true;

    IN_UpdateGrabs(); // QSS
}

void M_Main_Draw()
{
    mainMenu().draw();
}

void M_Main_Key(int key)
{
    mainMenu().key(key);

    switch(key)
    {
        case K_ESCAPE: [[fallthrough]];
        case K_BBUTTON:
        {
            key_dest = key_game;
            m_state = m_none;
            cls.demonum = m_save_demonum;

            IN_UpdateGrabs(); // QSS
            break;

            /*
            if(cls.demonum != -1 && !cls.demoplayback &&
                cls.state != ca_connected)
            {
                CL_NextDemo();
            }
            break;
            */
        }
    }
}

//=============================================================================
/* SINGLE PLAYER MENU */

[[nodiscard]] static quake::menu makeSinglePlayerMenu()
{
    quake::menu m{"Single Player", &M_Menu_Main_f};

    m.add_action_entry("Tutorial", [] { M_Menu_NewGame_f("vrtutorial"); });
    m.add_action_entry("Sandbox", [] { M_Menu_NewGame_f("vrfiringrange"); });

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_action_entry("New Game", [] { M_Menu_NewGame_f("start"); });
    m.add_action_entry("Load", &M_Menu_Load_f);
    m.add_action_entry("Save", &M_Menu_Save_f);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    {
        const std::vector<std::string>& loadedPakNames =
            VR_GetLoadedPakNamesWithStartMaps();

        auto e =
            m.add_cvar_entry<int>("Start map from:", vr_activestartpaknameidx,
                {1, 0, static_cast<int>(loadedPakNames.size()) - 1});

        e.tooltip(
            "When multiple '.pak' are placed in the 'Id1' folder, the starting "
            "map will be overwritten by the last '.pak' file. This option "
            "allows you to choose what '.pak' file to get the starting map "
            "from.");

        e->_printer = [&](char* buf, const int buf_size, const int x)
        {
            snprintf(buf, buf_size, "%s",
                loadedPakNames[x % loadedPakNames.size()].data());
        };
    }

    return m;
}

[[nodiscard]] static quake::menu& singlePlayerMenu()
{
    static quake::menu res = makeSinglePlayerMenu();
    return res;
}

void M_Menu_SinglePlayer_f()
{
    key_dest = key_menu;
    m_state = m_singleplayer;
    m_entersound = true;

    IN_UpdateGrabs(); // QSS
}

void M_SinglePlayer_Draw()
{
    singlePlayerMenu().draw();
}

void M_SinglePlayer_Key(int key)
{
    singlePlayerMenu().key(key);
}

//=============================================================================
/* LOAD/SAVE MENU */

std::size_t save_cursor; // 0 < save_cursor < MAX_SAVEGAMES
std::size_t load_cursor; // 0 < load_cursor < MAX_SAVEGAMES + MAX_AUTOSAVES

void M_Menu_NewGame_f(const char* map)
{
    key_dest = key_game;
    IN_UpdateGrabs(); // QSS

    if(sv.active)
    {
        Cbuf_AddText("disconnect\n");
    }

    Cbuf_AddText("maxplayers 1\n");
    Cbuf_AddText("deathmatch 0\n"); // johnfitz
    Cbuf_AddText("coop 0\n");       // johnfitz
    Cbuf_AddText(va("map %s\n", map));
}

void M_Menu_Load_f()
{
    m_entersound = true;
    m_state = m_load;

    key_dest = key_menu;

    quake::saveutil::scanSaves();

    IN_UpdateGrabs(); // QSS
}

void M_Menu_Save_f()
{
    if(!sv.active)
    {
        return;
    }

    if(cl.intermission)
    {
        return;
    }

    if(svs.maxclients != 1)
    {
        return;
    }

    m_entersound = true;
    m_state = m_save;

    key_dest = key_menu;

    quake::saveutil::scanSaves();

    IN_UpdateGrabs(); // QSS
}


void M_Load_Draw()
{
    qpic_t* p = Draw_CachePic("gfx/p_load.lmp");
    M_DrawPic((320 - p->width) / 2, 4, p);

    for(std::size_t i = 0; i < MAX_SAVEGAMES; i++)
    {
        M_Print(16, 32 + 8 * i, quake::saveutil::nthSaveFilename(i));
    }

    for(std::size_t i = 0; i < MAX_AUTOSAVES; i++)
    {
        M_PrintWhite(16, (8 * (MAX_SAVEGAMES + 1)) + 32 + 8 * i, "(AUTO)");

        M_Print(70, (8 * (MAX_SAVEGAMES + 1)) + 32 + 8 * i,
            quake::saveutil::nthAutosaveFilename(i));

        char buf[24]{'\0'};

        if(quake::saveutil::isNthAutosaveLoadable(i))
        {
            std::strftime(buf, sizeof(buf), "%F %T",
                std::localtime(&quake::saveutil::nthAutosaveTimestamp(i)));
        }

        M_PrintWhite(370, (8 * (MAX_SAVEGAMES + 1)) + 32 + 8 * i, buf);
    }

    // line cursor
    const int cursorX = (load_cursor < MAX_SAVEGAMES)
                            ? (32 + load_cursor * 8)
                            : (32 + (load_cursor + 1) * 8);

    M_DrawCharacter(8, cursorX, 12 + ((int)(realtime * 4) & 1));
}


void M_Save_Draw()
{
    qpic_t* p = Draw_CachePic("gfx/p_save.lmp");
    M_DrawPic((320 - p->width) / 2, 4, p);

    for(std::size_t i = 0; i < MAX_SAVEGAMES; i++)
    {
        M_Print(16, 32 + 8 * i, quake::saveutil::nthSaveFilename(i));
    }

    // line cursor
    M_DrawCharacter(8, 32 + save_cursor * 8, 12 + ((int)(realtime * 4) & 1));
}


void M_Load_Key(int k)
{
    switch(k)
    {
        case K_ESCAPE:
        case K_BBUTTON: M_Menu_SinglePlayer_f(); break;

        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON:
        {
            S_LocalSound("misc/menu2.wav");

            if(load_cursor < MAX_SAVEGAMES)
            {
                if(!quake::saveutil::isNthSaveLoadable(load_cursor))
                {
                    return;
                }
            }
            else
            {
                assert(load_cursor < MAX_SAVEGAMES + MAX_AUTOSAVES);

                if(!quake::saveutil::isNthAutosaveLoadable(
                       load_cursor - MAX_SAVEGAMES))
                {
                    return;
                }
            }

            m_state = m_none;
            key_dest = key_game;
            IN_UpdateGrabs(); // QSS

            // Host_Loadgame_f can't bring up the loading plaque because too
            // much stack space has been used, so do it now
            SCR_BeginLoadingPlaque();

            // issue the load command
            if(load_cursor < MAX_SAVEGAMES)
            {
                Cbuf_AddText(va("load s%i\n", load_cursor));
            }
            else
            {
                Cbuf_AddText(
                    va("load_autosave auto%i\n", load_cursor - MAX_SAVEGAMES));
            }

            return;
        }

        case K_UPARROW:
        case K_LEFTARROW:
        {
            S_LocalSound("misc/menu1.wav");

            if(load_cursor == 0)
            {
                load_cursor = (MAX_SAVEGAMES + MAX_AUTOSAVES) - 1;
            }
            else
            {
                --load_cursor;
            }

            break;
        }

        case K_DOWNARROW:
        case K_RIGHTARROW:
        {
            S_LocalSound("misc/menu1.wav");

            ++load_cursor;

            if(load_cursor >= (MAX_SAVEGAMES + MAX_AUTOSAVES))
            {
                load_cursor = 0;
            }

            break;
        }
    }
}


void M_Save_Key(int k)
{
    switch(k)
    {
        case K_ESCAPE:
        case K_BBUTTON: M_Menu_SinglePlayer_f(); break;

        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON:
        {
            m_state = m_none;
            key_dest = key_game;
            Cbuf_AddText(va("save s%i\n", save_cursor));

            IN_UpdateGrabs(); // QSS
            return;
        }

        case K_UPARROW:
        case K_LEFTARROW:
        {
            S_LocalSound("misc/menu1.wav");

            if(save_cursor == 0)
            {
                save_cursor = MAX_SAVEGAMES - 1;
            }
            else
            {
                --save_cursor;
            }

            break;
        }

        case K_DOWNARROW:
        case K_RIGHTARROW:
        {
            S_LocalSound("misc/menu1.wav");

            ++save_cursor;

            if(save_cursor >= MAX_SAVEGAMES)
            {
                save_cursor = 0;
            }
            break;
        }
    }
}

//=============================================================================
/* BOT CONTROL MENU */

[[nodiscard]] static quake::menu makeBotControlMenu()
{
    const auto runCmd = [](const char* cmd)
    {
        return [cmd]
        {
            quake::menu_util::playMenuSound("items/r_item2.wav", 0.5);
            Cmd_ExecuteString(cmd, cmd_source_t::src_command);
        };
    };

    quake::menu m{"Bot Control", &M_Menu_MultiPlayer_f};

    m.add_action_entry("Add Bot (Team 0)", runCmd("impulse 100"));
    m.add_action_entry("Add Bot (Team 1)", runCmd("impulse 101"));
    m.add_action_entry("Kick Bot", runCmd("impulse 102"));

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    extern cvar_t skill;
    m.add_cvar_getter_enum_entry<int>(        //
        "Skill",                              //
        [] { return &skill; },                //
        "Easy", "Normal", "Hard", "Nightmare" //
    );

    return m;
}

[[nodiscard]] static quake::menu& botControlMenu()
{
    static quake::menu res = makeBotControlMenu();
    return res;
}

void M_Menu_BotControl_f()
{
    key_dest = key_menu;
    m_state = m_botcontrol;
    m_entersound = true;

    IN_UpdateGrabs(); // QSS
}

void M_BotControl_Draw()
{
    botControlMenu().draw();
}

void M_BotControl_Key(int key)
{
    botControlMenu().key(key);
}

//=============================================================================
/* MULTIPLAYER MENU */

[[nodiscard]] static quake::menu makeMultiPlayerMenu()
{
    quake::menu m{"Multi Player", &M_Menu_Main_f};

    m.add_action_entry("Join a Game", &M_Menu_LanConfig_f);
    m.add_action_entry("New Game", &M_Menu_LanConfig_f);
    m.add_action_entry("Setup", &M_Menu_Setup_f);
    m.add_action_entry("Bot Control", &M_Menu_BotControl_f);

    return m;
}

[[nodiscard]] static quake::menu& multiPlayerMenu()
{
    static quake::menu res = makeMultiPlayerMenu();
    return res;
}

void M_Menu_MultiPlayer_f()
{
    key_dest = key_menu;
    m_state = m_multiplayer;
    m_entersound = true;

    IN_UpdateGrabs(); // QSS
}

void M_MultiPlayer_Draw()
{
    // QSS
    if(!ipv4Available && !ipv6Available)
    {
        M_PrintWhite(
            (320 / 2) - ((27 * 8) / 2), 148, "No Communications Available");
        return;
    }

    multiPlayerMenu().draw();
}

void M_MultiPlayer_Key(int key)
{
    multiPlayerMenu().key(key);
}

//=============================================================================
/* SETUP MENU */

int setup_cursor = 4;
int setup_cursor_table[] = {40, 56, 80, 104, 140};

char setup_hostname[16];
char setup_myname[16];
int setup_oldtop;
int setup_oldbottom;
int setup_top;
int setup_bottom;

#define NUM_SETUP_CMDS 5

void M_Menu_Setup_f()
{
    key_dest = key_menu;
    m_state = m_setup;
    m_entersound = true;
    Q_strcpy(setup_myname, cl_name.string);
    Q_strcpy(setup_hostname, hostname.string);
    setup_top = setup_oldtop = ((int)cl_color.value) >> 4;
    setup_bottom = setup_oldbottom = ((int)cl_color.value) & 15;

    IN_UpdateGrabs(); // QSS
}

void M_Setup_Draw()
{
    qpic_t* p;

    M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
    p = Draw_CachePic("gfx/p_multi.lmp");
    M_DrawPic((320 - p->width) / 2, 4, p);

    M_Print(64, 40, "Hostname");
    M_DrawTextBox(160, 32, 16, 1);
    M_Print(168, 40, setup_hostname);

    M_Print(64, 56, "Your name");
    M_DrawTextBox(160, 48, 16, 1);
    M_Print(168, 56, setup_myname);

    M_Print(64, 80, "Shirt color");
    M_Print(64, 104, "Pants color");

    M_DrawTextBox(64, 140 - 8, 14, 1);
    M_Print(72, 140, "Accept Changes");

    p = Draw_CachePic("gfx/bigbox.lmp");
    M_DrawTransPic(160, 64, p);
    p = Draw_CachePic("gfx/menuplyr.lmp");
    M_DrawTransPicTranslate(172, 72, p, setup_top, setup_bottom);

    M_DrawCharacter(
        56, setup_cursor_table[setup_cursor], 12 + ((int)(realtime * 4) & 1));

    if(setup_cursor == 0)
    {
        M_DrawCharacter(168 + 8 * strlen(setup_hostname),
            setup_cursor_table[setup_cursor], 10 + ((int)(realtime * 4) & 1));
    }

    if(setup_cursor == 1)
    {
        M_DrawCharacter(168 + 8 * strlen(setup_myname),
            setup_cursor_table[setup_cursor], 10 + ((int)(realtime * 4) & 1));
    }
}


void M_Setup_Key(int k)
{
    switch(k)
    {
        case K_ESCAPE:
        case K_BBUTTON: M_Menu_MultiPlayer_f(); break;

        case K_UPARROW:
            S_LocalSound("misc/menu1.wav");
            setup_cursor--;
            if(setup_cursor < 0)
            {
                setup_cursor = NUM_SETUP_CMDS - 1;
            }
            break;

        case K_DOWNARROW:
            S_LocalSound("misc/menu1.wav");
            setup_cursor++;
            if(setup_cursor >= NUM_SETUP_CMDS)
            {
                setup_cursor = 0;
            }
            break;

        case K_LEFTARROW:
            if(setup_cursor < 2)
            {
                return;
            }
            S_LocalSound("misc/menu3.wav");
            if(setup_cursor == 2)
            {
                setup_top -= 1;
            }
            if(setup_cursor == 3)
            {
                setup_bottom -= 1;
            }
            break;
        case K_RIGHTARROW:
            if(setup_cursor < 2)
            {
                return;
            }
        forward:
            S_LocalSound("misc/menu3.wav");
            if(setup_cursor == 2)
            {
                setup_top += 1;
            }
            if(setup_cursor == 3)
            {
                setup_bottom += 1;
            }
            break;

        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON:
            if(setup_cursor == 0 || setup_cursor == 1)
            {
                return;
            }

            if(setup_cursor == 2 || setup_cursor == 3)
            {
                goto forward;
            }

            // setup_cursor == 4 (OK)
            if(Q_strcmp(cl_name.string, setup_myname) != 0)
            {
                Cbuf_AddText(va("name \"%s\"\n", setup_myname));
            }
            if(Q_strcmp(hostname.string, setup_hostname) != 0)
            {
                Cvar_Set("hostname", setup_hostname);
            }
            if(setup_top != setup_oldtop || setup_bottom != setup_oldbottom)
            {
                Cbuf_AddText(va("color %i %i\n", setup_top, setup_bottom));
            }
            m_entersound = true;
            M_Menu_MultiPlayer_f();
            break;

        case K_BACKSPACE:
            if(setup_cursor == 0)
            {
                if(strlen(setup_hostname))
                {
                    setup_hostname[strlen(setup_hostname) - 1] = 0;
                }
            }

            if(setup_cursor == 1)
            {
                if(strlen(setup_myname))
                {
                    setup_myname[strlen(setup_myname) - 1] = 0;
                }
            }
            break;
    }

    if(setup_top > 13)
    {
        setup_top = 0;
    }
    if(setup_top < 0)
    {
        setup_top = 13;
    }
    if(setup_bottom > 13)
    {
        setup_bottom = 0;
    }
    if(setup_bottom < 0)
    {
        setup_bottom = 13;
    }
}


void M_Setup_Char(int k)
{
    int l;

    switch(setup_cursor)
    {
        case 0:
            l = strlen(setup_hostname);
            if(l < 15)
            {
                setup_hostname[l + 1] = 0;
                setup_hostname[l] = k;
            }
            break;
        case 1:
            l = strlen(setup_myname);
            if(l < 15)
            {
                setup_myname[l + 1] = 0;
                setup_myname[l] = k;
            }
            break;
    }
}


bool M_Setup_TextEntry()
{
    return (setup_cursor == 0 || setup_cursor == 1);
}

//=============================================================================
/* NET MENU */

int m_net_cursor;
int m_net_items;

const char* net_helpMessage[] = {
    /* .........1.........2.... */
    " Novell network LANs    ", " or Windows 95 DOS-box. ",
    "                        ", "(LAN=Local Area Network)",

    " Commonly used to play  ", " over the Internet, but ",
    " also used on a Local   ", " Area Network.          "};

void M_Menu_Net_f()
{
    key_dest = key_menu;
    m_state = m_net;
    m_entersound = true;
    m_net_items = 1;

    if(m_net_cursor >= m_net_items)
    {
        m_net_cursor = 0;
    }
    m_net_cursor--;
    M_Net_Key(K_DOWNARROW);

    IN_UpdateGrabs(); // QSS
}


void M_Net_Draw()
{
    int f;
    qpic_t* p;

    M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
    p = Draw_CachePic("gfx/p_multi.lmp");
    M_DrawPic((320 - p->width) / 2, 4, p);

    f = 32;
    p = Draw_CachePic("gfx/netmen4.lmp");
    M_DrawTransPic(72, f, p);

    f = (320 - 26 * 8) / 2;
    M_DrawTextBox(f, 96, 24, 4);
    f += 8;
    M_Print(f, 104, net_helpMessage[m_net_cursor * 4 + 0]);
    M_Print(f, 112, net_helpMessage[m_net_cursor * 4 + 1]);
    M_Print(f, 120, net_helpMessage[m_net_cursor * 4 + 2]);
    M_Print(f, 128, net_helpMessage[m_net_cursor * 4 + 3]);

    f = (int)(realtime * 10) % 6;
    M_DrawTransPic(54, 32 + m_net_cursor * 20,
        Draw_CachePic(va("gfx/menudot%i.lmp", f + 1)));
}


void M_Net_Key(int k)
{
    switch(k)
    {
        case K_ESCAPE:
        case K_BBUTTON: M_Menu_MultiPlayer_f(); break;

        case K_DOWNARROW:
            S_LocalSound("misc/menu1.wav");
            m_net_cursor = 0;
            break;

        case K_UPARROW:
            S_LocalSound("misc/menu1.wav");
            m_net_cursor = 0;
            break;

        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON:
            m_entersound = true;
            M_Menu_LanConfig_f();
            break;
    }
}

//=============================================================================
/* OPTIONS MENU */

[[nodiscard]] static quake::menu makeOptionsMenu()
{
    namespace qmu = ::quake::menu_util;

    quake::menu m{"Options", &M_Menu_Main_f};

    m.add_action_entry("Controls", &M_Menu_Keys_f);
    m.add_action_entry("Goto Console",
        []
        {
            m_state = m_none;
            Con_ToggleConsole_f();
        });
    m.add_action_entry("Reset Config",
        []
        {
            if(SCR_ModalMessage("This will reset all controls\n"
                                "and stored cvars. Continue? (y/n)\n",
                   15.0f))
            {
                Cbuf_AddText("resetcfg\n");
                Cbuf_AddText("exec default.cfg\n");
            }
        });
    m.add_action_slider_entry(
        "Scale",
        [](int dir)
        {
            const float l = ((vid.width + 31) / 32) / 10.0;
            const float f = std::clamp(scr_conscale.value + dir * 0.1f, 1.f, l);

            Cvar_SetValue("scr_conscale", f);
            Cvar_SetValue("scr_menuscale", f);
            Cvar_SetValue("scr_sbarscale", f);
        },
        []
        {
            const float l = (vid.width / 320.0) - 1;
            return l > 0 ? (scr_conscale.value - 1) / l : 0;
        });
    m.add_action_slider_entry(
        "Screen Size",
        [](int dir)
        {
            const float f =
                std::clamp(scr_viewsize.value + dir * 10, 30.f, 120.f);
            Cvar_SetValue("viewsize", f);
        },
        [] { return (scr_viewsize.value - 30) / (120 - 30); });

    // TODO VR: (P2) changing these makes the screen black in VR
    if(false)
    {
        m.add_action_slider_entry(
            "Brightness",
            [](int dir)
            {
                const float f =
                    std::clamp(vid_gamma.value - dir * 0.05f, 0.5f, 1.f);
                Cvar_SetValue("gamma", f);
            },
            [] { return (1.0 - vid_gamma.value) / 0.5; });

        m.add_action_slider_entry(
            "Contrast",
            [](int dir)
            {
                const float f =
                    std::clamp(vid_contrast.value + dir * 0.1f, 1.f, 2.f);
                Cvar_SetValue("contrast", f);
            },
            [] { return vid_contrast.value - 1.0; });
    }

    m.add_action_slider_entry(
        "Mouse Speed",
        [](int dir)
        {
            const float f =
                std::clamp(sensitivity.value + dir * 0.5f, 1.f, 11.f);
            Cvar_SetValue("sensitivity", f);
        },
        [] { return (sensitivity.value - 1) / 10; });
    m.add_action_slider_entry(
        "Statusbar Alpha",
        [](int dir)
        {
            const float f =
                std::clamp(scr_sbaralpha.value - dir * 0.05f, 0.f, 1.f);
            Cvar_SetValue("scr_sbaralpha", f);
        },
        [] { return (1.0 - scr_sbaralpha.value); });
    m.add_action_slider_entry(
        "Sound Volume",
        [](int dir)
        {
            const float f = std::clamp(sfxvolume.value + dir * 0.1f, 0.f, 1.f);
            Cvar_SetValue("volume", f);
        },
        [] { return sfxvolume.value; });
    m.add_action_slider_entry(
        "Music Volume",
        [](int dir)
        {
            const float f = std::clamp(bgmvolume.value + dir * 0.1f, 0.f, 1.f);
            Cvar_SetValue("bgmvolume", f);
        },
        [] { return bgmvolume.value; });
    m.add_cvar_entry<bool>("External Music", bgm_extmusic);
    m.add_cvar_entry<bool>("Always Run", cl_alwaysrun);
    m.add_action_entry("Toggle Invert Mouse",
        [] { Cvar_SetValue("m_pitch", -m_pitch.value); });
    m.add_action_entry("Toggle Mouse Look",
        []
        {
            if(in_mlook.state & 1)
            {
                Cbuf_AddText("-mlook");
            }
            else
            {
                Cbuf_AddText("+mlook");
            }
        });
    m.add_cvar_entry<bool>("Lookspring", lookspring);
    m.add_cvar_entry<bool>("Lookstrafe", lookstrafe);

    return m;
}

[[nodiscard]] static quake::menu& optionsMenu()
{
    static quake::menu res = makeOptionsMenu();
    return res;
}

void M_Menu_Options_f()
{
    key_dest = key_menu;
    m_state = m_options;
    m_entersound = true;

    IN_UpdateGrabs(); // QSS
}

void M_DrawSlider(int x, int y, float range)
{
    const int SLIDER_RANGE = 10;

    int i;

    if(range < 0)
    {
        range = 0;
    }
    if(range > 1)
    {
        range = 1;
    }
    M_DrawCharacter(x - 8, y, 128);
    for(i = 0; i < SLIDER_RANGE; i++)
    {
        M_DrawCharacter(x + i * 8, y, 129);
    }
    M_DrawCharacter(x + i * 8, y, 130);
    M_DrawCharacter(x + (SLIDER_RANGE - 1) * 8 * range, y, 131);
}

void M_DrawCheckbox(int x, int y, int on)
{
#if 0
	if (on)
		M_DrawCharacter (x, y, 131);
	else
		M_DrawCharacter (x, y, 129);
#endif
    if(on)
    {
        M_Print(x, y, "on");
    }
    else
    {
        M_Print(x, y, "off");
    }
}

void M_Options_Draw()
{
    optionsMenu().draw();
}

void M_Options_Key(int k)
{
    optionsMenu().key(k);

    /*
    if(options_cursor == OPTIONS_ITEMS - 1 && vid_menudrawfn == nullptr)
    {
        if(k == K_UPARROW)
        {
            options_cursor = OPTIONS_ITEMS - 2;
        }
        else
        {
            options_cursor = 0;
        }
    }
   */
}

//=============================================================================
/* QUAKE VR SETTINGS MENU - MENU SETTINGS */

[[nodiscard]] static quake::menu makeQVRSMenuMenu()
{
    quake::menu m{"Menu Settings", &M_Menu_QuakeVRSettings_f};

    m.add_cvar_getter_enum_entry<VrMenuMode>( //
         "Menu Mode",                         //
         [] { return &vr_menumode; },         //
         "Fixed Head",                        //
         "Follow Head",                       //
         "Follow Off-Hand",                   //
         "Follow Main Hand"                   //
         )
        .tooltip("Control where the menu is anchored/displayed.");

    m.add_cvar_entry<float>("Menu Scale", vr_menu_scale, {0.01f, 0.05f, 0.6f})
        .tooltip("Scale multiplier for the menu.");

    m.add_cvar_entry<float>("Menu Distance", vr_menu_distance, {1, 24, 256})
        .tooltip("Distance of the menu from the anchor point.");

    m.add_cvar_getter_enum_entry<int>(              //
        "Pointer Hand",                             //
        [] { return &vr_menu_mouse_pointer_hand; }, //
        "Off-Hand",                                 //
        "Main Hand");

    // TODO VR: (P1) menu lerp amount, menu distance follow hand?

    return m;
}

[[nodiscard]] static quake::menu& qvrsMenuMenu()
{
    static quake::menu res = makeQVRSMenuMenu();
    return res;
}

//=============================================================================
/* QUAKE VR SETTINGS MENU - CROSSHAIR SETTINGS */

[[nodiscard]] static quake::menu makeQVRSCrosshairMenu()
{
    quake::menu m{"Crosshair Settings", &M_Menu_QuakeVRSettings_f};

    m.add_cvar_getter_enum_entry<int>(        //
        "Crosshair",                          //
        [] { return &vr_crosshair; },         //
        "Off", "Point", "Line", "Smooth line" //
    );

    m.add_cvar_entry<float>(
         "Crosshair Depth", vr_crosshair_depth, {16.f, 0.f, 4096.f})
        .tooltip(
            "When zero, the crosshair will be projected to the closest wall. "
            "Otherwise, the crosshair will be projected at a fixed depth.");

    m.add_cvar_entry<float>(
         "Crosshair Size", vr_crosshair_size, {0.5f, 0.f, 32.f})
        .tooltip("Size of the crosshair.");

    m.add_cvar_entry<float>(
         "Crosshair Alpha", vr_crosshair_alpha, {0.05f, 0.f, 1.f})
        .tooltip(
            "Transparency of the crosshair. '1' means fully visible, '0' means "
            "fully transparent.");

    m.add_cvar_entry<float>(
         "Crosshair Z Offset", vr_crosshairy, {0.05f, -10.0f, 10.f})
        .tooltip(
            "Z offset applied to the crosshair. Does not affect aiming. Leave "
            "to zero unless you changed other weapon options and your aim is "
            "off.");

    return m;
}

[[nodiscard]] static quake::menu& qvrsCrosshairMenu()
{
    static quake::menu res = makeQVRSCrosshairMenu();
    return res;
}

//=============================================================================
/* QUAKE VR SETTINGS MENU - PARTICLE SETTINGS */

[[nodiscard]] static quake::menu makeQVRSParticleMenu()
{
    quake::menu m{"Particle Settings", &M_Menu_QuakeVRSettings_f};

    extern cvar_t r_particles;
    m.add_cvar_entry<bool>("Particle Effects", r_particles)
        .tooltip("Enable or disable all particle effects.");

    extern cvar_t r_particle_mult;
    m.add_cvar_entry<float>(
         "Particle Multiplier", r_particle_mult, {0.25f, 0.25f, 10.f})
        .tooltip("Multiplier for the number of spawned particles.");

    return m;
}

[[nodiscard]] static quake::menu& qvrsParticleMenu()
{
    static quake::menu res = makeQVRSParticleMenu();
    return res;
}

//=============================================================================
/* QUAKE VR SETTINGS MENU - LOCOMOTION SETTINGS */

[[nodiscard]] static quake::menu makeQVRSLocomotionMenu()
{
    quake::menu m{"Locomotion Settings", &M_Menu_QuakeVRSettings_f};

    m.add_cvar_getter_enum_entry<VrMovementMode>( //
         "Movement Mode",                         //
         [] { return &vr_movement_mode; },        //
         "Follow Hand", "Follow Head"             //
         )
        .tooltip("Movement options for smooth locomotion.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<float>("Deadzone", vr_deadzone, {5.f, 0.f, 180.f})
        .tooltip("Controller thumbstick deadzone.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<bool>("Enable Joystick Turn", vr_enable_joystick_turn)
        .tooltip(
            "Allows turning using the controller. Keep enabled unless you want "
            "to force yourself to turn in real life.");

    {
        auto e = m.add_cvar_entry<int>("Turn", vr_snap_turn, {5, 0, 90});
        e.tooltip("Joystick turn smooth mode or snap turn degrees.");
        e->_printer = [](char* buf, const int buf_size, const int x)
        {
            if(x == 0)
            {
                snprintf(buf, buf_size, "Smooth");
            }
            else
            {
                snprintf(buf, buf_size, "%d Degrees", x);
            }
        };
    }

    constexpr float max_turn_speed = 10.0f;
    m.add_cvar_entry<float>(
         "Turn Speed", vr_turn_speed, {0.25f, 0.f, max_turn_speed})
        .tooltip("How quickly you turn with the joystick.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<bool>("Teleportation", vr_teleport_enabled)
        .tooltip(
            "Enables or disables teleportation. A controller binding is "
            "required to use teleportation.");

    m.add_cvar_entry<float>(
         "Teleport Range", vr_teleport_range, {10.f, 100.f, 800.f})
        .tooltip("How far you can teleport.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<bool>("Roomscale Jump", vr_roomscale_jump)
        .tooltip(
            "When enabled and after calibrating your height, you'll be able to "
            "jump in-game by jumping in real life.");

    m.add_cvar_entry<float>("Roomscale Jump Threshold",
         vr_roomscale_jump_threshold, {0.05f, 0.05f, 3.f})
        .tooltip(
            "Z velocity threshold to trigger a roomscale jump. Increase if you "
            "find yourself jumping when looking up or standing on your toes.");

    m.add_cvar_entry<float>(
         "Roomscale Move Mult.", vr_roomscale_move_mult, {0.25f, 0.25f, 5.f})
        .tooltip(
            "Roomscale movement multiplier. Allows you to cover more or less "
            "distance in-game compared to real life. Consider increasing if "
            "playing with teleportation and/or have limited space.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<int>("Movement Speed", cl_forwardspeed, {25, 100, 400})
        .tooltip("In-game movement speed.");

    m.add_cvar_entry<float>(
         "Speed Button Multiplier", cl_movespeedkey, {0.05f, 0.1f, 1.f})
        .tooltip(
            "In-game movement speed multiplier applied when pressing the "
            "'speed button', which needs to be bound to your controllers.");

    return m;
}

[[nodiscard]] static quake::menu& qvrsLocomotionMenu()
{
    static quake::menu res = makeQVRSLocomotionMenu();
    return res;
}

//=============================================================================
/* QUAKE VR SETTINGS MENU - HAND/GUN CALIBRATION */

[[nodiscard]] static quake::menu makeQVRSHandGunCalibrationMenu()
{
    quake::menu m{"Hand/Gun Calibration", &M_Menu_QuakeVRSettings_f};

    constexpr float max_gunangle = 180.0f;

    m.add_cvar_entry<float>(
         "Gun Angle", vr_gunangle, {0.75f, -max_gunangle, max_gunangle})
        .tooltip(
            "Pitch offset for the guns/hands. Affects aiming. Change if your "
            "controller is not 'aligned' to your gun.");

    m.add_cvar_entry<float>(
         "Gun Model Pitch", vr_gunmodelpitch, {0.25f, -90.f, 90.f})
        .tooltip("Pitch offset for gun models. Does NOT affect aiming.");

    m.add_cvar_entry<float>(
         "Gun Model Scale", vr_gunmodelscale, {0.05f, 0.1f, 2.f})
        .tooltip(
            "Scale multiplier for gun models. Does NOT affect aiming, but does "
            "affect muzzle position (where bullets and projectiles come "
            "from).");

    m.add_cvar_entry<float>(
         "Gun Model Z Offset", vr_gunmodely, {0.1f, -5.0f, 5.f})
        .tooltip("Z offset for gun models. Does NOT affect aiming.");

    m.add_cvar_entry<float>("Gun Yaw", vr_gunyaw, {0.25f, -90.f, 90.f})
        .tooltip(
            "Yaw offset for the guns/hands. Affects aiming. Change if your "
            "controller is not 'aligned' to your gun.");

    m.add_cvar_entry<float>(
         "Gun Z Offset", vr_gun_z_offset, {0.25f, -30.f, 30.f})
        .tooltip("Z offset for guns/hands. Affects aiming.");

    m.add_cvar_entry<float>(
         "Off-Hand Pitch", vr_offhandpitch, {0.25f, -90.f, 90.f})
        .tooltip(
            "Pitch offset for the guns/hands. Only for off-hand. Affects "
            "aiming.");

    m.add_cvar_entry<float>("Off-Hand Yaw", vr_offhandyaw, {0.25f, -90.f, 90.f})
        .tooltip(
            "Yaw offset for the guns/hands. Only for off-hand. Affects "
            "aiming.");

    m.add_cvar_entry<float>(
        "Finger Grip Bias", vr_finger_grip_bias, {0.05f, 0.f, 1.f});

    m.add_cvar_entry<bool>("Auto close thumb", vr_finger_auto_close_thumb);

    return m;
}

[[nodiscard]] static quake::menu& qvrsHandGunCalibrationMenu()
{
    static quake::menu res = makeQVRSHandGunCalibrationMenu();
    return res;
}

//=============================================================================
/* QUAKE VR SETTINGS MENU - PLAYER CALIBRATION */

[[nodiscard]] static quake::menu makeQVRSPlayerCalibrationMenu()
{
    constexpr float max_floor_offset = 200.0f;

    quake::menu m{"Player Calibration", &M_Menu_QuakeVRSettings_f};

    m.add_action_entry("Calibrate Height", &VR_CalibrateHeight)
        .tooltip(
            "When clicked, the game will calibrate your standing height. "
            "Please stand straight and look ahead.");

    m.add_cvar_entry<float>("World Scale", vr_world_scale, {0.05f, 0.f, 2.f})
        .tooltip(
            "Scale of the world. Affects movement, height, and hand "
            "positions.");

    m.add_cvar_entry<float>("Floor Offset", vr_floor_offset,
         {2.5f, -max_floor_offset, max_floor_offset})
        .tooltip(
            "Offset from the floor. Change if you feel like you're floating or "
            "stuck in the floor.");

    return m;
}

[[nodiscard]] static quake::menu& qvrsPlayerCalibrationMenu()
{
    static quake::menu res = makeQVRSPlayerCalibrationMenu();
    return res;
}

//=============================================================================
/* QUAKE VR SETTINGS MENU - MELEE SETTINGS */

[[nodiscard]] static quake::menu makeQVRSMeleeMenu()
{
    quake::menu m{"Melee Settings", &M_Menu_QuakeVRSettings_f};

    m.add_cvar_entry<float>(
        "Melee Threshold", vr_melee_threshold, {0.5f, 4.f, 18.f});

    m.add_cvar_entry<float>("Melee Damage Multiplier", vr_melee_dmg_multiplier,
         {0.25f, 0.25f, 15.f})
        .tooltip("Damage multiplier for melee attacks.");

    m.add_cvar_entry<float>("Melee Range Multiplier", vr_melee_range_multiplier,
         {0.25f, 0.25f, 15.f})
        .tooltip("Range multiplier for melee attacks.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<VrMeleeBloodlust>( //
         "Melee Bloodlust",                         //
         [] { return &vr_melee_bloodlust; },        //
         "Enabled", "Disabled"                      //
         )
        .tooltip(
            "When enabled, restores the player's health when performing melee "
            "attacks on enemies.");

    m.add_cvar_entry<float>("Melee Bloodlust Mult.", vr_melee_bloodlust_mult,
         {0.05f, 0.05f, 5.f}) //
        .tooltip("Multiplier for melee bloodlust health restoration.");

    return m;
}

[[nodiscard]] static quake::menu& qvrsMeleeMenu()
{
    static quake::menu res = makeQVRSMeleeMenu();
    return res;
}

//=============================================================================
/* QUAKE VR SETTINGS MENU - AIMING SETTINGS */

[[nodiscard]] static quake::menu makeQVRSAimingMenu()
{
    quake::menu m{"Aiming Settings", &M_Menu_QuakeVRSettings_f};

    m.add_cvar_getter_enum_entry<Vr2HMode>(   //
         "2H Aiming",                         //
         [] { return &vr_2h_mode; },          //
         "Disabled", "Basic", "Virtual Stock" //
         )
        .tooltip(
            "Two-handed aiming mode. 'Basic' does not provide a virtual stock, "
            "it only considers the two hands for interpolation. 'Virtual "
            "Stock' considers the shoulder's position for interpolation as "
            "well, if the weapon is close enough. The shoulder position can be "
            "tweaked in 'Hotspot Settings'.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<float>(
         "2H Aiming Threshold", vr_2h_angle_threshold, {0.05f, -1.f, 1.f})
        .tooltip(
            "How much your hands have to be out of line with each other before "
            "two-handed aiming stops. Increase the value for a more strict "
            "two-handed aiming experience.");

    m.add_cvar_entry<bool>(
        "Disable 2H Aiming Threshold", vr_2h_disable_angle_threshold);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<float>("2H Virtual Stock Factor",
         vr_2h_virtual_stock_factor, {0.05f, 0.f, 1.f})
        .tooltip(
            "How much the virtual stock matters in the final angle calculation "
            "for two-handed aiming. Decrease if you want your hands to matter "
            "more.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<bool>("Weighted Weapon Move", vr_wpn_pos_weight)
        .tooltip("Enable weight simulation for weapon movement.");

    m.add_cvar_entry<float>(
        "W. Weapon Move Off.", vr_wpn_pos_weight_offset, {0.05f, 0.05f, 1.f});

    m.add_cvar_entry<float>(
        "W. Weapon Move Mult", vr_wpn_pos_weight_mult, {0.1f, -5.f, 5.f});

    m.add_cvar_entry<float>("W. W. Move 2H Help Off.",
        vr_wpn_pos_weight_2h_help_offset, {0.05f, 0.05f, 1.f});

    m.add_cvar_entry<float>("W. W. Move 2H Help Mult",
        vr_wpn_pos_weight_2h_help_mult, {0.1f, -5.f, 5.f});

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<bool>("Weighted Weapon Turn", vr_wpn_dir_weight)
        .tooltip("Enable weight simulation for weapon turning.");

    m.add_cvar_entry<float>(
        "W. Weapon Turn Off.", vr_wpn_dir_weight_offset, {0.05f, 0.05f, 1.f});

    m.add_cvar_entry<float>(
        "W. Weapon Turn Mult", vr_wpn_dir_weight_mult, {0.1f, -5.f, 5.f});

    m.add_cvar_entry<float>("W. W. Turn 2H Help Off.",
        vr_wpn_dir_weight_2h_help_offset, {0.05f, 0.05f, 1.f});

    m.add_cvar_entry<float>("W. W. Turn 2H Help Mult",
        vr_wpn_dir_weight_2h_help_mult, {0.1f, -5.f, 5.f});

    return m;
}

[[nodiscard]] static quake::menu& qvrsAimingMenu()
{
    static quake::menu res = makeQVRSAimingMenu();
    return res;
}

//=============================================================================
/* QUAKE VR SETTINGS MENU - IMMERSION SETTINGS */

[[nodiscard]] static quake::menu makeQVRSImmersionMenu()
{
    quake::menu m{"Immersion Settings", &M_Menu_QuakeVRSettings_f};

    m.add_cvar_entry<bool>("Weapon Text", vr_show_weapon_text)
        .tooltip("Show floating ammunition text attached to weapons");

    m.add_cvar_entry<bool>("Positional Damage", vr_positional_damage)
        .tooltip(
            "Enables positional damage multipliers for headshots and leg "
            "shots. Only applies to humanoid enemies.");

    m.add_cvar_entry<bool>("Body-Item Interactions", vr_body_interactions)
        .tooltip(
            "When enabled, items and level weapons can be picked up by walking "
            "over them (instead of using your hands). Thrown and dropped "
            "weapons are not affected - they still need to be grabbed with "
            "your hand.");

    m.add_cvar_entry<bool>("Disable Haptics", vr_disablehaptics)
        .tooltip("Disable all haptics (vibration).");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<VrHolsterMode>( //
         "Weapon Mode",                          //
         [] { return &vr_holster_mode; },        //
         "Immersive", "Cycle + Quick Slots"      //
         )
        .tooltip(
            "Immersive mode holsters behave like real world holsters. They can "
            "be empty or full, and unholstering a weapon empties a holster. "
            "Immersive mode should be used without weapon cycling to limit the "
            "amount of weapons the player can carry. Quick slot holsters never "
            "become empty. They are the recommended way to play if you allow "
            "weapon cycling and want to carry infinite weapons.");

    m.add_cvar_getter_enum_entry<VrReloadMode>(      //
         "Weapon Reloading Mode",                    //
         [] { return &vr_reload_mode; },             //
         "None", "All Holsters", "Hip Holsters Only" //
         )
        .tooltip(
            "Enables weapon reloading mechanics. Weapons can be reloaded by "
            "moving them towards an holster. Reloading only works with "
            "'immersive' weapon mode.");

    m.add_cvar_getter_enum_entry<VrHolsterHaptics>( //
         "Holster Haptics",                         //
         [] { return &vr_holster_haptics; },        //
         "Off",                                     //
         "Continuous",                              //
         "Once"                                     //
         )
        .tooltip("Haptic feedback when hovering a usable holster slot.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<VrWeaponCycleMode>( //
         "Weapon Cycle Mode",                        //
         [] { return &vr_weapon_cycle_mode; },       //
         "Immersive (Disabled)", "Enabled"           //
         )
        .tooltip(
            "Allows or disallows cycling weapons using controller buttons.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<VrWeaponThrowMode>( //
         "Weapon Throw Mode",                        //
         [] { return &vr_weapon_throw_mode; },       //
         "Immersive", "Disappear on Hit", "Discard"  //
         )
        .tooltip(
            "Immersive mode allows throwing and picking weapons back up. It is "
            "the recommended way to play if immersive holster mode is enabled. "
            "With quick slot mode enabled, it makes more sense to use other "
            "options to avoid infinite weapons being spawned on the ground. "
            "Throw damage is affected by the weapon and the throw velocity.");

    m.add_cvar_entry<float>("Weapon Throw Damage Mult.",
         vr_weapon_throw_damage_mult, {0.05f, 0.05f, 5.f})
        .tooltip("Multiplier for weapon throw damage.");

    m.add_cvar_entry<float>("Weapon Throw Velocity Mult.",
         vr_weapon_throw_velocity_mult, {0.05f, 0.05f, 5.f})
        .tooltip("Multiplier for weapon throw velocity.");

    m.add_cvar_entry<bool>("Dropped Weapon Particles", vr_weapondrop_particles)
        .tooltip(
            "Enable particle effects for weapons dropped/thrown on the "
            "ground.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<VrEnemyDrops>( //
         "Enemy Weapon Drops",                  //
         [] { return &vr_enemy_drops; },        //
         "When Eligible", "Always", "Disabled"  //
         )
        .tooltip(
            "Controls random enemy weapon drops. 'Eligible' means that the "
            "player has obtained a weapon before through a level weapon "
            "pickup.");

    m.add_cvar_entry<float>("Enemy W. Drops Chance Mult.",
         vr_enemy_drops_chance_mult, {0.05f, 0.05f, 5.f}) //
        .tooltip("Multiplier for enemy weapon drops.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<VrAmmoBoxDrops>( //
         "Ammo Box Weapon Drops",                 //
         [] { return &vr_ammobox_drops; },        //
         "When Eligible", "Always", "Disabled"    //
         )
        .tooltip(
            "Controls random ammo box weapon drops. 'Eligible' means that the "
            "player has obtained a weapon before through a level weapon "
            "pickup.");

    m.add_cvar_entry<float>("Ammo Box W. Drops Chance Mult.",
         vr_ammobox_drops_chance_mult, {0.05f, 0.05f, 5.f}) //
        .tooltip("Multiplier for ammo box weapon drops.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<VrForceGrabMode>(   //
         "Force Grab",                               //
         [] { return &vr_forcegrab_mode; },          //
         "Disabled", "Linear", "Parabola", "Instant" //
         )
        .tooltip(
            "When enabled, allows the player to force grab thrown weapons from "
            "a distance.");

    m.add_cvar_entry<float>(
         "Force Grab Max Range", vr_forcegrab_range, {1.f, 1.f, 512.f}) //
        .tooltip("Maximum range for a force grab.");

    m.add_cvar_entry<float>(
         "Force Grab Radius", vr_forcegrab_radius, {0.1f, 0.1f, 64.f}) //
        .tooltip(
            "Search radius for weapon force grab (applied at trace end "
            "position based on max range).");

    m.add_cvar_entry<float>(
         "Force Grab Power Mult.", vr_forcegrab_powermult, {0.05f, 0.f, 10.f})
        .tooltip(
            "Multiplier for the strength of a weapon force grab. Tweak this if "
            "your grabs are either too strong or too weak.");

    m.add_cvar_entry<bool>(
         "Force Grab Particles", vr_forcegrab_eligible_particles)
        .tooltip(
            "Enable particle effects when aiming towards weapons that can be "
            "force grabbed.");

    m.add_cvar_entry<bool>("Force Grab Haptics", vr_forcegrab_eligible_haptics)
        .tooltip(
            "Enable haptic effects when aiming towards weapons that can be "
            "force grabbed.");

    return m;
}

[[nodiscard]] static quake::menu& qvrsImmersionMenu()
{
    static quake::menu res = makeQVRSImmersionMenu();
    return res;
}

//=============================================================================
/* QUAKE VR SETTINGS MENU - GRAPHICAL SETTINGS */

[[nodiscard]] static quake::menu makeQVRSGraphicalMenu()
{
    // ------------------------------------------------------------------------

    quake::menu m{"Graphical Settings", &M_Menu_QuakeVRSettings_f};

    const int max_msaa = quake::util::getMaxMSAALevel();
    m.add_cvar_entry<int>("MSAA Samples", vr_msaa, {1, 0, max_msaa});

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    extern cvar_t r_shadows;
    m.add_cvar_entry<bool>("Show Shadows", r_shadows);

    m.add_cvar_getter_enum_entry<VrPlayerShadows>( //
        "Player Shadows",                          //
        [] { return &vr_player_shadows; },         //
        "Off",                                     //
        "View Entities",                           //
        "Third Person",                            //
        "Both");

    return m;
}

[[nodiscard]] static quake::menu& qvrsGraphicalMenu()
{
    static quake::menu res = makeQVRSGraphicalMenu();
    return res;
}

//=============================================================================
/* QUAKE VR SETTINGS MENU - HUD CONFIGURATION */

[[nodiscard]] static quake::menu makeQVRSHudConfigurationMenu()
{
    constexpr float oInc = 1.f;
    constexpr float oBound = 200.f;

    constexpr float rInc = 0.1f;
    constexpr float rBound = 90.f;

    const quake::menu_bounds<float> oBounds{oInc, -oBound, oBound};
    const quake::menu_bounds<float> rBounds{rInc, -rBound, rBound};

    quake::menu m{"Hud Configuration", &M_Menu_QuakeVRSettings_f};

    m.add_cvar_getter_enum_entry<int>( //
        "Status Bar Mode",             //
        [] { return &vr_sbar_mode; },  //
        "Main Hand", "Off Hand"        //
    );

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<float>("HUD Scale", vr_hud_scale, {0.005f, 0.01f, 0.1f});

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<float>("Offset X", vr_sbar_offset_x, oBounds);
    m.add_cvar_entry<float>("Offset Y", vr_sbar_offset_y, oBounds);
    m.add_cvar_entry<float>("Offset Z", vr_sbar_offset_z, oBounds);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<float>("Scale", vr_hud_scale, {0.005f, 0.01f, 2.f});

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<float>("Roll", vr_sbar_offset_roll, rBounds);
    m.add_cvar_entry<float>("Pitch", vr_sbar_offset_pitch, rBounds);
    m.add_cvar_entry<float>("Yaw", vr_sbar_offset_yaw, rBounds);

    return m;
}

[[nodiscard]] static quake::menu& qvrsHudConfigurationMenu()
{
    static quake::menu res = makeQVRSHudConfigurationMenu();
    return res;
}

//=============================================================================
/* QUAKE VR SETTINGS MENU - HOTSPOT SETTINGS */

[[nodiscard]] static quake::menu makeQVRSHotspotMenu()
{
    constexpr quake::menu_bounds<float> bPos{0.5f, -50.f, 50.f};
    constexpr quake::menu_bounds<float> bDst{0.1f, 0.f, 30.f};

    quake::menu m{"Hotspot Settings", &M_Menu_QuakeVRSettings_f};

    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<int>(
        "Show Shoulder (Virtual Stock)",
        [] { return &vr_show_virtual_stock; },       //
        "Off", "Main Hand", "Off Hand", "Both Hands" //
    );

    m.add_cvar_entry<float>("Shoulder X", vr_shoulder_offset_x, bPos);
    m.add_cvar_entry<float>("Shoulder Y", vr_shoulder_offset_y, bPos);
    m.add_cvar_entry<float>("Shoulder Z", vr_shoulder_offset_z, bPos);
    m.add_cvar_entry<float>(
        "Virtual Stock Threshold", vr_virtual_stock_thresh, bDst);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<VrOptionHandSelection>( //
        "Show Shoulder Holsters",                        //
        [] { return &vr_show_shoulder_holsters; },       //
        "Off", "Main Hand", "Off Hand", "Both Hands"     //
    );

    m.add_cvar_entry<float>("Shoulder X", vr_shoulder_holster_offset_x, bPos);
    m.add_cvar_entry<float>("Shoulder Y", vr_shoulder_holster_offset_y, bPos);
    m.add_cvar_entry<float>("Shoulder Z", vr_shoulder_holster_offset_z, bPos);
    m.add_cvar_entry<float>(
        "Shoulder Threshold", vr_shoulder_holster_thresh, bDst);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<VrOptionHandSelection>( //
        "Show Hip Holsters",                             //
        [] { return &vr_show_hip_holsters; },            //
        "Off", "Main Hand", "Off Hand", "Both Hands"     //
    );

    m.add_cvar_entry<float>("Hip X", vr_hip_offset_x, bPos);
    m.add_cvar_entry<float>("Hip Y", vr_hip_offset_y, bPos);
    m.add_cvar_entry<float>("Hip Z", vr_hip_offset_z, bPos);
    m.add_cvar_entry<float>("Hip Threshold", vr_hip_holster_thresh, bDst);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<VrOptionHandSelection>( //
        "Show Upper Holsters",                           //
        [] { return &vr_show_upper_holsters; },          //
        "Off", "Main Hand", "Off Hand", "Both Hands"     //
    );

    m.add_cvar_entry<float>("Upper X", vr_upper_holster_offset_x, bPos);
    m.add_cvar_entry<float>("Upper Y", vr_upper_holster_offset_y, bPos);
    m.add_cvar_entry<float>("Upper Z", vr_upper_holster_offset_z, bPos);
    m.add_cvar_entry<float>("Upper Threshold", vr_upper_holster_thresh, bDst);

    return m;
}

[[nodiscard]] static quake::menu& qvrsHotspotMenu()
{
    static quake::menu res = makeQVRSHotspotMenu();
    return res;
}

//=============================================================================
/* QUAKE VR SETTINGS MENU - TORSO SETTINGS */

[[nodiscard]] static quake::menu makeQVRSTorsoMenu()
{
    constexpr quake::menu_bounds<float> vrtOBounds{0.5f, -100.f, 100.f};
    constexpr quake::menu_bounds<float> vrtMBounds{1.f, 0.f, 250.f};
    constexpr quake::menu_bounds<float> vrtSBounds{0.05f, 0.1f, 2.f};
    constexpr quake::menu_bounds<float> vrtRBounds{1.f, -180.f, 180.f};

    quake::menu m{"Torso Settings", &M_Menu_QuakeVRSettings_f};

    m.on_key(
        [](int, quake::impl::menu_entry&)
        {
            // TODO VR: (P2) hackish
            VR_ModVRTorsoModel();
            VR_ModVRLegHolsterModel();
        });

    // ------------------------------------------------------------------------

    m.add_cvar_entry<bool>("Show VR Torso", vr_vrtorso_enabled);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<float>("VR Torso X", vr_vrtorso_x_offset, vrtOBounds);
    m.add_cvar_entry<float>("VR Torso Y", vr_vrtorso_y_offset, vrtOBounds);
    m.add_cvar_entry<float>("VR Torso Z", vr_vrtorso_z_offset, vrtOBounds);
    m.add_cvar_entry<float>(
        "VR Torso Head Z Mult", vr_vrtorso_head_z_mult, vrtMBounds);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<float>("VR Torso Scale X", vr_vrtorso_x_scale, vrtSBounds);
    m.add_cvar_entry<float>("VR Torso Scale Y", vr_vrtorso_y_scale, vrtSBounds);
    m.add_cvar_entry<float>("VR Torso Scale Z", vr_vrtorso_z_scale, vrtSBounds);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<float>("VR Torso Pitch", vr_vrtorso_pitch, vrtRBounds);
    m.add_cvar_entry<float>("VR Torso Yaw", vr_vrtorso_yaw, vrtRBounds);
    m.add_cvar_entry<float>("VR Torso Roll", vr_vrtorso_roll, vrtRBounds);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<bool>("Show Holster Slots", vr_leg_holster_model_enabled);
    m.add_cvar_entry<float>(
        "Holster Slot Scale", vr_leg_holster_model_scale, vrtSBounds);

    m.add_cvar_entry<float>(
        "Holster Slot X", vr_leg_holster_model_x_offset, vrtOBounds);
    m.add_cvar_entry<float>(
        "Holster Slot Y", vr_leg_holster_model_y_offset, vrtOBounds);
    m.add_cvar_entry<float>(
        "Holster Slot Z", vr_leg_holster_model_z_offset, vrtOBounds);

    return m;
}

[[nodiscard]] static quake::menu& qvrsTorsoMenu()
{
    static quake::menu res = makeQVRSTorsoMenu();
    return res;
}

//=============================================================================
/* QUAKE VR SETTINGS MENU - TRANSPARENCY OPTIONS */

[[nodiscard]] static quake::menu makeQVRSTransparencyOptionsMenu()
{
    quake::menu m{"Transparency Options", &M_Menu_QuakeVRSettings_f};

    // TODO VR: (P1) what about this?
    m.on_key(
        [](const int key, quake::impl::menu_entry& entry)
        {
            if(key == 'p')
            {
                quakeVRQuickSettingsMenu().add_entry_ptr(entry);
            }
        });

    extern cvar_t r_novis;
    m.add_cvar_entry<bool>("(!) No Vis", r_novis)
        .tooltip(
            "NOT RECOMMENDED. Allows water transparency to work in "
            "non-vispatched maps, but will render the entire map every frame. "
            "Will likely kill your FPS. Prefer 'vispatching' your maps "
            "instead. Also, entities won't still be visible below water level "
            "without vispatched maps.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    extern cvar_t r_wateralpha;
    m.add_cvar_entry<float>("Water Alpha", r_wateralpha, {0.1f, 0.f, 1.f});

    extern cvar_t r_lavaalpha;
    m.add_cvar_entry<float>("Lava Alpha", r_lavaalpha, {0.1f, 0.f, 1.f});

    extern cvar_t r_telealpha;
    m.add_cvar_entry<float>("Tele Alpha", r_telealpha, {0.1f, 0.f, 1.f});

    extern cvar_t r_slimealpha;
    m.add_cvar_entry<float>("Slime Alpha", r_slimealpha, {0.1f, 0.f, 1.f});

    return m;
}

[[nodiscard]] static quake::menu& qvrsTransparencyOptionsMenu()
{
    static quake::menu res = makeQVRSTransparencyOptionsMenu();
    return res;
}

//=============================================================================
/* QUAKE VR SETTINGS MENU - VOIP OPTIONS */

[[nodiscard]] static quake::menu makeQVRSVoipMenu()
{
    extern cvar_t sv_voip;
    extern cvar_t sv_voip_echo;

    extern cvar_t cl_voip_send;
    extern cvar_t cl_voip_test;
    extern cvar_t cl_voip_vad_threshhold;
    extern cvar_t cl_voip_vad_delay;
    extern cvar_t cl_voip_capturingvol;
    extern cvar_t cl_voip_showmeter;
    extern cvar_t cl_voip_play;
    extern cvar_t cl_voip_micamp;
    extern cvar_t cl_voip_ducking;
    extern cvar_t cl_voip_codec;
    extern cvar_t cl_voip_noisefilter;
    extern cvar_t cl_voip_autogain;
    extern cvar_t cl_voip_opus_bitrate;

    quake::menu m{"Voip Options", &M_Menu_QuakeVRSettings_f};

    m.add_cvar_entry<bool>("Server - Enable Voip", sv_voip);
    m.add_cvar_entry<bool>("Server - Enable Voip Echo", sv_voip_echo);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<int>(          //
        "Send Mode",                            //
        [] { return &cl_voip_send; },           //
        "Off", "Voice Activation", "Continuous" //
    );

    m.add_cvar_entry<bool>("Test Mode", cl_voip_test);

    m.add_cvar_entry<float>("Vad Delay", cl_voip_vad_delay, {0.1f, 0.0f, 1.f});

    m.add_cvar_entry<float>(
        "Capturing Volume", cl_voip_capturingvol, {0.1f, 0.0f, 1.f});

    m.add_cvar_entry<bool>("Show Meter", cl_voip_showmeter);

    m.add_cvar_entry<bool>("Play Voip", cl_voip_play);

    m.add_cvar_entry<float>("Mic Amp.", cl_voip_micamp, {0.1f, 0.0f, 10.f});

    m.add_cvar_entry<float>("Ducking", cl_voip_ducking, {0.1f, 0.0f, 10.f});

    m.add_cvar_entry<bool>("Noise Filter", cl_voip_noisefilter);

    m.add_cvar_entry<bool>("Autogain", cl_voip_autogain);

    m.add_cvar_entry<float>(
        "Opus Bitrate", cl_voip_opus_bitrate, {500.f, 500.0f, 512000.f});

    return m;
}

[[nodiscard]] static quake::menu& qvrsVoipMenu()
{
    static quake::menu res = makeQVRSVoipMenu();
    return res;
}

//=============================================================================
/* QUAKE VR QUICK SETTINGS MENU */

[[nodiscard]] static quake::menu makeQuakeVRQuickSettingsMenu()
{
    quake::menu m{"Quake VR - Quick Settings", &M_Menu_Main_f};
    return m;
}

[[nodiscard]] static quake::menu& quakeVRQuickSettingsMenu()
{
    static quake::menu res = makeQuakeVRQuickSettingsMenu();
    return res;
}

void M_Menu_QuakeVRQuickSettings_f()
{
    key_dest = key_menu;
    m_state = m_quakevrquicksettings;
    m_entersound = true;

    IN_UpdateGrabs(); // QSS
}

void M_QuakeVRQuickSettings_Draw()
{
    quakeVRQuickSettingsMenu().draw();
}

void M_QuakeVRQuickSettings_Key(int k)
{
    quakeVRQuickSettingsMenu().key(k);
}

//=============================================================================
/* QUAKE VR SETTINGS MENU */

template <typename F>
static void forQVRSMenus(F&& f)
{
    f(qvrsMenuMenu(), m_qvrs_menu);
    f(qvrsCrosshairMenu(), m_qvrs_crosshair);
    f(qvrsParticleMenu(), m_qvrs_particle);
    f(qvrsLocomotionMenu(), m_qvrs_locomotion);
    f(qvrsHandGunCalibrationMenu(), m_qvrs_handguncalibration);
    f(qvrsPlayerCalibrationMenu(), m_qvrs_playercalibration);
    f(qvrsMeleeMenu(), m_qvrs_melee);
    f(qvrsAimingMenu(), m_qvrs_aiming);
    f(qvrsImmersionMenu(), m_qvrs_immersion);
    f(qvrsGraphicalMenu(), m_qvrs_graphical);
    f(qvrsHudConfigurationMenu(), m_qvrs_hudconfiguration);
    f(qvrsHotspotMenu(), m_qvrs_hotspot);
    f(qvrsTorsoMenu(), m_qvrs_torso);
    f(qvrsTransparencyOptionsMenu(), m_qvrs_transparencyoptions);
    f(qvrsVoipMenu(), m_qvrs_voip);
}

[[nodiscard]] static quake::menu makeQuakeVRSettingsMenu()
{
    quake::menu m{"Quake VR - Settings", &M_Menu_Main_f};

    const auto makeGotoMenu = [&](quake::menu& xm, m_state_e s)
    {
        m.add_action_entry(
            xm.title(), [&xm, s] { quake::menu_util::setMenuState(xm, s); });
    };

    forQVRSMenus(makeGotoMenu);
    return m;
}

[[nodiscard]] static quake::menu& quakeVRSettingsMenu()
{
    static quake::menu res = makeQuakeVRSettingsMenu();
    return res;
}

void M_Menu_QuakeVRSettings_f()
{
    key_dest = key_menu;
    m_state = m_quakevrsettings;
    m_entersound = true;

    IN_UpdateGrabs(); // QSS
}

void M_QuakeVRSettings_Draw()
{
    quakeVRSettingsMenu().draw();
}

void M_QuakeVRSettings_Key(int k)
{
    quakeVRSettingsMenu().key(k);
}

//=============================================================================
/* QUAKE VR DEV TOOLS MENU - WEAPON CONFIGURATION (1) */

[[nodiscard]] static quake::menu makeQVRDTWeaponConfiguration1Menu()
{
    static bool wpnoff_offhand = false;

    const auto getIdx = []
    {
        return wpnoff_offhand ? VR_GetOffHandWpnCvarEntry()
                              : VR_GetMainHandWpnCvarEntry();
    };

    const float oInc = 0.1f;
    constexpr float oBound = 100.f;

    const float rInc = 0.2f;
    constexpr float rBound = 90.f;

    const quake::menu_bounds<float> oBounds{oInc, -oBound, oBound};
    const quake::menu_bounds<float> rBounds{rInc, -rBound, rBound};

    // ------------------------------------------------------------------------

    quake::menu m{"Weapon Configuration (1) - Basics, Muzzle, 2H",
        &M_Menu_QuakeVRDevTools_f};

    m.on_key(
        [](int, quake::impl::menu_entry&)
        {
            // TODO VR: (P2) hackish
            VR_ModAllWeapons();
        });

    // ------------------------------------------------------------------------

    const auto o_wpncvar = [&](const char* title, const WpnCVar c)
    {
        return m.add_cvar_getter_entry<float>(                   //
            title,                                               //
            [getIdx, c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            oBounds                                              //
        );
    };

    const auto r_wpncvar = [&](const char* title, const WpnCVar c)
    {
        return m.add_cvar_getter_entry<float>(                   //
            title,                                               //
            [getIdx, c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            rBounds                                              //
        );
    };

    const auto makeHoverFn = [&](int& implVar)
    {
        return [&](const bool x)
        {
            if(!x)
            {
                implVar = 0;
                return;
            }

            implVar = wpnoff_offhand ? 2 : 1;
        };
    };

    // ------------------------------------------------------------------------

    m.add_getter_entry<bool>(          //
        "Off-Hand",                    //
        [] { return &wpnoff_offhand; } //
    );

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const auto hoverOffset =
        makeHoverFn(quake::vr::showfn::vr_impl_draw_wpnoffset_helper_offset);
    const auto hover2HOffset =
        makeHoverFn(quake::vr::showfn::vr_impl_draw_wpnoffset_helper_2h_offset);

    // ------------------------------------------------------------------------

    const char* offsetTooltip =
        "Offset of the weapon relative to the center of its model. Does not "
        "affect aiming.";

    o_wpncvar("X", WpnCVar::OffsetX).hover(hoverOffset).tooltip(offsetTooltip);
    o_wpncvar("Y", WpnCVar::OffsetY).hover(hoverOffset).tooltip(offsetTooltip);
    o_wpncvar("Z", WpnCVar::OffsetZ).hover(hoverOffset).tooltip(offsetTooltip);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    o_wpncvar("Scale", WpnCVar::Scale).tooltip("Scale of the weapon model.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const char* rotationTooltip =
        "Rotation of the weapon model. Does not affect aiming.";

    r_wpncvar("Roll", WpnCVar::Roll).tooltip(rotationTooltip);
    r_wpncvar("Pitch", WpnCVar::Pitch).tooltip(rotationTooltip);
    r_wpncvar("Yaw", WpnCVar::Yaw).tooltip(rotationTooltip);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const auto hoverMuzzle =
        makeHoverFn(quake::vr::showfn::vr_impl_draw_wpnoffset_helper_muzzle);

    const char* muzzleTooltip =
        "Position of the weapon muzzle. Relative to the XYZ offsets above. "
        "Affected by the weapon model scale. DOES affect aiming. Bullets and "
        "projectiles spawn from this position.";

    o_wpncvar("Muzzle X", WpnCVar::MuzzleOffsetX)
        .hover(hoverMuzzle)
        .tooltip(muzzleTooltip);
    o_wpncvar("Muzzle Y", WpnCVar::MuzzleOffsetY)
        .hover(hoverMuzzle)
        .tooltip(muzzleTooltip);
    o_wpncvar("Muzzle Z", WpnCVar::MuzzleOffsetZ)
        .hover(hoverMuzzle)
        .tooltip(muzzleTooltip);

    m.add_cvar_getter_entry<int>( //
         "Muzzle Anchor Vertex",  //
         [getIdx]
         { return &VR_GetWpnCVar(getIdx(), WpnCVar::MuzzleAnchorVertex); }, //
         {1, 0, 4096}                                                       //
         )
        .hover(hoverMuzzle)
        .tooltip("Index of the mesh vertex where the muzzle will be attached.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const char* twoHXYZTooltip =
        "Offset applied to the off-hand when aiming with two hands. Allows "
        "tweaking of the weapon's position and angle, and how close the "
        "off-hand appears to the model. DOES affect aiming.";

    o_wpncvar("2H X", WpnCVar::TwoHOffsetX)
        .hover(hover2HOffset)
        .tooltip(twoHXYZTooltip);
    o_wpncvar("2H Y", WpnCVar::TwoHOffsetY)
        .hover(hover2HOffset)
        .tooltip(twoHXYZTooltip);
    o_wpncvar("2H Z", WpnCVar::TwoHOffsetZ)
        .hover(hover2HOffset)
        .tooltip(twoHXYZTooltip);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const char* twoHRotTooltip =
        "Angle offset applied to the weapon when aiming with two hands. "
        "Allows tweaking of the weapon's angle. DOES affect aiming.";

    r_wpncvar("2H Pitch", WpnCVar::TwoHPitch).tooltip(twoHRotTooltip);
    r_wpncvar("2H Yaw", WpnCVar::TwoHYaw).tooltip(twoHRotTooltip);
    r_wpncvar("2H Roll", WpnCVar::TwoHRoll).tooltip(twoHRotTooltip);

    return m;
}

[[nodiscard]] static quake::menu& qvrdtWeaponConfiguration1Menu()
{
    static quake::menu res = makeQVRDTWeaponConfiguration1Menu();
    return res;
}

//=============================================================================
/* QUAKE VR DEV TOOLS MENU - WEAPON CONFIGURATION (2) */

[[nodiscard]] static quake::menu makeQVRDTWeaponConfiguration2Menu()
{
    static bool wpnoff_offhand = false;

    const auto getIdx = []
    {
        return wpnoff_offhand ? VR_GetOffHandWpnCvarEntry()
                              : VR_GetMainHandWpnCvarEntry();
    };

    const float oInc = 0.1f;
    constexpr float oBound = 100.f;

    const float rInc = 0.2f;
    constexpr float rBound = 180.f;

    const quake::menu_bounds<float> oBounds{oInc, -oBound, oBound};
    const quake::menu_bounds<float> rBounds{rInc, -rBound, rBound};

    // ------------------------------------------------------------------------

    quake::menu m{
        "Weapon Configuration (2) - Hand, 2H", &M_Menu_QuakeVRDevTools_f};

    m.on_key(
        [](int, quake::impl::menu_entry&)
        {
            // TODO VR: (P2) hackish
            VR_ModAllWeapons();
        });

    // ------------------------------------------------------------------------

    const auto o_wpncvar = [&](const char* title, const WpnCVar c)
    {
        return m.add_cvar_getter_entry<float>(                   //
            title,                                               //
            [getIdx, c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            oBounds                                              //
        );
    };

    const auto r_wpncvar = [&](const char* title, const WpnCVar c)
    {
        return m.add_cvar_getter_entry<float>(                   //
            title,                                               //
            [getIdx, c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            rBounds                                              //
        );
    };

    const auto b_wpncvar = [&](const char* title, const WpnCVar c)
    {
        return m.add_cvar_getter_entry<bool>(                   //
            title,                                              //
            [getIdx, c] { return &VR_GetWpnCVar(getIdx(), c); } //
        );
    };

    const auto makeHoverFn = [&](int& implVar)
    {
        return [&](const bool x)
        {
            if(!x)
            {
                implVar = 0;
                return;
            }

            implVar = wpnoff_offhand ? 2 : 1;
        };
    };

    // ------------------------------------------------------------------------

    m.add_getter_entry<bool>(          //
        "Off-Hand",                    //
        [] { return &wpnoff_offhand; } //
    );

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const auto hoverHandAnchorVertex =
        makeHoverFn(quake::vr::showfn::vr_impl_draw_hand_anchor_vertex);
    const auto hover2HHandAnchorVertex =
        makeHoverFn(quake::vr::showfn::vr_impl_draw_2h_hand_anchor_vertex);

    // ------------------------------------------------------------------------

    b_wpncvar("Hide Hand", WpnCVar::HideHand)
        .tooltip("Hide the hand model when this weapon is wielded.");

    m.add_cvar_getter_entry<int>( //
         "Hand Anchor Vertex",    //
         [getIdx]
         { return &VR_GetWpnCVar(getIdx(), WpnCVar::HandAnchorVertex); }, //
         {1, 0, 4096}                                                     //
         )
        .hover(hoverHandAnchorVertex)
        .tooltip(
            "Index of the mesh vertex where the hand will be attached. Useful "
            "to ensure that the hand follows the weapon animations "
            "properly.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<Wpn2HMode>( //
         "2H Display Mode",                  //
         [getIdx]
         { return &VR_GetWpnCVar(getIdx(), WpnCVar::TwoHDisplayMode); }, //
         "Dynamic", "Fixed"                                              //
         )
        .tooltip(
            "Display mode for the 2H aiming helping hand. When 'dynamic', the "
            "helping hand can move freely. When 'fixed', the helping hand "
            "stays locked to a particular vertex.");

    m.add_cvar_getter_entry<int>( //
         "2H Hand Anchor Vertex", //
         [getIdx]
         { return &VR_GetWpnCVar(getIdx(), WpnCVar::TwoHHandAnchorVertex); }, //
         {1, 0, 4096}                                                         //
         )
        .hover(hover2HHandAnchorVertex)
        .tooltip(
            "Index of the mesh vertex where the 2H aiming helping hand will be "
            "attached. Useful ensure that the hand follows the weapon "
            "animations properly. Only enabled when 2H Display Mode is set to "
            "'fixed'.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const char* handOffsetTooltip =
        "Visual offset of the hand, relative to the anchor vertex.";

    o_wpncvar("Hand X", WpnCVar::HandOffsetX).tooltip(handOffsetTooltip);
    o_wpncvar("Hand Y", WpnCVar::HandOffsetY).tooltip(handOffsetTooltip);
    o_wpncvar("Hand Z", WpnCVar::HandOffsetZ).tooltip(handOffsetTooltip);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<Wpn2HMode>(                               //
         "2H Mode",                                                        //
         [getIdx] { return &VR_GetWpnCVar(getIdx(), WpnCVar::TwoHMode); }, //
         "Default", "Ignore Virtual Stock", "Forbidden"                    //
         )
        .tooltip(
            "Defines whether the weapon is eligible for 2H aiming. "
            "Virtual stock can be ignored for weapons like the laser "
            "cannon.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const char* fixed2HTooltip =
        "Visual offset of the helping hand when aiming two-handed in fixed "
        "mode.";

    o_wpncvar("Fixed 2H X", WpnCVar::TwoHFixedOffsetX).tooltip(fixed2HTooltip);
    o_wpncvar("Fixed 2H Y", WpnCVar::TwoHFixedOffsetY).tooltip(fixed2HTooltip);
    o_wpncvar("Fixed 2H Z", WpnCVar::TwoHFixedOffsetZ).tooltip(fixed2HTooltip);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const char* fixed2HHandRotTooltip =
        "Visual rotation of the helping hand when aiming two-handed in fixed "
        "mode.";

    r_wpncvar("Fixed 2H Hand Pitch", WpnCVar::TwoHFixedHandPitch)
        .tooltip(fixed2HHandRotTooltip);
    r_wpncvar("Fixed 2H Hand Yaw", WpnCVar::TwoHFixedHandYaw)
        .tooltip(fixed2HHandRotTooltip);
    r_wpncvar("Fixed 2H Hand Roll", WpnCVar::TwoHFixedHandRoll)
        .tooltip(fixed2HHandRotTooltip);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const char* fixed2HMainHandTooltip =
        "Visual offset of the helping hand when aiming two-handed in fixed "
        "mode, only appliyed when the helping hand is the right one.";

    o_wpncvar("Fixed 2H Right Hand X", WpnCVar::TwoHFixedMainHandOffsetX)
        .tooltip(fixed2HMainHandTooltip);
    o_wpncvar("Fixed 2H Right Hand Y", WpnCVar::TwoHFixedMainHandOffsetY)
        .tooltip(fixed2HMainHandTooltip);
    o_wpncvar("Fixed 2H Right Hand Z", WpnCVar::TwoHFixedMainHandOffsetZ)
        .tooltip(fixed2HMainHandTooltip);

    // TODO VR: (P1) bugged
    // const char* gunOffsetTooltip =
    //     "Visual offset of the gun model. Does not affect hand positioning.";
    //
    // o_wpncvar("Gun X", WpnCVar::GunOffsetX).tooltip(gunOffsetTooltip);
    // o_wpncvar("Gun Y", WpnCVar::GunOffsetY).tooltip(gunOffsetTooltip);
    // o_wpncvar("Gun Z", WpnCVar::GunOffsetZ).tooltip(gunOffsetTooltip);

    return m;
}

[[nodiscard]] static quake::menu& qvrdtWeaponConfiguration2Menu()
{
    static quake::menu res = makeQVRDTWeaponConfiguration2Menu();
    return res;
}

//=============================================================================
/* QUAKE VR DEV TOOLS MENU - WEAPON CONFIGURATION (3) */

[[nodiscard]] static quake::menu makeQVRDTWeaponConfiguration3Menu()
{
    static bool wpnoff_offhand = false;

    const auto getIdx = []
    {
        return wpnoff_offhand ? VR_GetOffHandWpnCvarEntry()
                              : VR_GetMainHandWpnCvarEntry();
    };

    const float wInc = 0.01f;
    constexpr float wBound = 1.f;

    const quake::menu_bounds<float> wBounds{wInc, 0.f, wBound};
    const quake::menu_bounds<float> wmBounds{wInc, 0.f, 10.f};

    // ------------------------------------------------------------------------

    quake::menu m{
        "Weapon Configuration (3) - Weight", &M_Menu_QuakeVRDevTools_f};

    m.on_key(
        [](int, quake::impl::menu_entry&)
        {
            // TODO VR: (P2) hackish
            VR_ModAllWeapons();
        });

    // ------------------------------------------------------------------------

    const auto w_wpncvar = [&](const char* title, const WpnCVar c)
    {
        return m.add_cvar_getter_entry<float>(                   //
            title,                                               //
            [getIdx, c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            wBounds                                              //
        );
    };

    const auto wm_wpncvar = [&](const char* title, const WpnCVar c)
    {
        return m.add_cvar_getter_entry<float>(                   //
            title,                                               //
            [getIdx, c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            wmBounds                                             //
        );
    };

    // ------------------------------------------------------------------------

    m.add_getter_entry<bool>(          //
        "Off-Hand",                    //
        [] { return &wpnoff_offhand; } //
    );

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    w_wpncvar("Weight", WpnCVar::Weight)
        .tooltip(
            "How heavy the weapon 'feels'. Values closer to '1' are heavier. "
            "'1' itself is 'infinite' weight. Affects weapon movement and "
            "rotation speed, and also throwing distance and damage.");

    wm_wpncvar("Weight Pos. Mult.", WpnCVar::WeightPosMult)
        .tooltip("Weight multiplier for movement only.");

    wm_wpncvar("Weight Dir. Mult.", WpnCVar::WeightDirMult)
        .tooltip("Weight multiplier for direction only.");

    wm_wpncvar("Weight Hand Vel. Mult.", WpnCVar::WeightHandVelMult)
        .tooltip(
            "Weight multiplier for hand velocity calculations (e.g. melee "
            "attacks) only.");

    wm_wpncvar("Weight Hand Throw Vel. Mult.", WpnCVar::WeightHandThrowVelMult)
        .tooltip(
            "Weight multiplier for hand throw velocity calculations (e.g. "
            "throwing weapons) only.");

    wm_wpncvar("Weight 2H Pos. Mult.", WpnCVar::Weight2HPosMult)
        .tooltip(
            "Weight multiplier for two-handed movement (how much it helps to "
            "use two hands).");

    wm_wpncvar("Weight 2H Dir. Mult.", WpnCVar::Weight2HDirMult)
        .tooltip(
            "Weight multiplier for two-handed direction (how much it helps to "
            "use two hands).");

    return m;
}

[[nodiscard]] static quake::menu& qvrdtWeaponConfiguration3Menu()
{
    static quake::menu res = makeQVRDTWeaponConfiguration3Menu();
    return res;
}

//=============================================================================
/* QUAKE VR DEV TOOLS MENU - WEAPON CONFIGURATION (4) */

[[nodiscard]] static quake::menu makeQVRDTWeaponConfiguration4Menu()
{
    static bool wpnoff_offhand = false;

    const auto getIdx = []
    {
        return wpnoff_offhand ? VR_GetOffHandWpnCvarEntry()
                              : VR_GetMainHandWpnCvarEntry();
    };

    const float oInc = 0.1f;
    constexpr float oBound = 100.f;

    const float rInc = 0.2f;
    constexpr float rBound = 180.f;

    const quake::menu_bounds<float> oBounds{oInc, -oBound, oBound};
    const quake::menu_bounds<float> rBounds{rInc, -rBound, rBound};
    const quake::menu_bounds<float> zbBounds{0.05f, 0.f, 1.f};

    // ------------------------------------------------------------------------

    quake::menu m{
        "Weapon Configuration (4) - Button, Blend", &M_Menu_QuakeVRDevTools_f};

    m.on_key(
        [](int, quake::impl::menu_entry&)
        {
            // TODO VR: (P2) hackish
            VR_ModAllWeapons();
        });

    // ------------------------------------------------------------------------

    const auto o_wpncvar = [&](const char* title, const WpnCVar c)
    {
        return m.add_cvar_getter_entry<float>(                   //
            title,                                               //
            [getIdx, c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            oBounds                                              //
        );
    };

    const auto r_wpncvar = [&](const char* title, const WpnCVar c)
    {
        return m.add_cvar_getter_entry<float>(                   //
            title,                                               //
            [getIdx, c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            rBounds                                              //
        );
    };

    const auto zb_wpncvar = [&](const char* title, const WpnCVar c)
    {
        return m.add_cvar_getter_entry<float>(                   //
            title,                                               //
            [getIdx, c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            zbBounds                                             //
        );
    };

    const auto makeHoverFn = [&](int& implVar)
    {
        return [&](const bool x)
        {
            if(!x)
            {
                implVar = 0;
                return;
            }

            implVar = wpnoff_offhand ? 2 : 1;
        };
    };

    // ------------------------------------------------------------------------

    m.add_getter_entry<bool>(          //
        "Off-Hand",                    //
        [] { return &wpnoff_offhand; } //
    );

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<WpnButtonMode>( //
         "Button Mode",                          //
         [getIdx]
         { return &VR_GetWpnCVar(getIdx(), WpnCVar::WpnButtonMode); }, //
         "Disabled", "Ammo Type Change"                                //
         )
        .tooltip("Type of button.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const char* btnOffsetTooltip = "Offset of the weapon button.";

    o_wpncvar("Button X", WpnCVar::WpnButtonX).tooltip(btnOffsetTooltip);
    o_wpncvar("Button Y", WpnCVar::WpnButtonY).tooltip(btnOffsetTooltip);
    o_wpncvar("Button Z", WpnCVar::WpnButtonZ).tooltip(btnOffsetTooltip);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const char* btnAngleTooltip = "Angle offset of the weapon button.";

    r_wpncvar("Button Pitch", WpnCVar::WpnButtonPitch).tooltip(btnAngleTooltip);
    r_wpncvar("Button Yaw", WpnCVar::WpnButtonYaw).tooltip(btnAngleTooltip);
    r_wpncvar("Button Roll", WpnCVar::WpnButtonRoll).tooltip(btnAngleTooltip);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const auto hoverWpnButtonAnchorVertex =
        makeHoverFn(quake::vr::showfn::vr_impl_draw_wpnbutton_anchor_vertex);

    m.add_cvar_getter_entry<int>( //
         "Button Anchor Vertex",  //
         [getIdx] {
             return &VR_GetWpnCVar(getIdx(), WpnCVar::WpnButtonAnchorVertex);
         },           //
         {1, 0, 4096} //
         )
        .hover(hoverWpnButtonAnchorVertex)
        .tooltip("Index of the mesh vertex where the button will be attached.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const char* zbTooltip =
        "Blending factor of the weapon animation with the zeroth frame. Useful "
        "to adjust recoil animations.";

    zb_wpncvar("Zero Blend", WpnCVar::ZeroBlend).tooltip(zbTooltip);
    zb_wpncvar("2H Zero Blend", WpnCVar::TwoHZeroBlend).tooltip(zbTooltip);

    return m;
}

[[nodiscard]] static quake::menu& qvrdtWeaponConfiguration4Menu()
{
    static quake::menu res = makeQVRDTWeaponConfiguration4Menu();
    return res;
}

//=============================================================================
/* QUAKE VR DEV TOOLS MENU - WEAPON CONFIGURATION (5) */

[[nodiscard]] static quake::menu makeQVRDTWeaponConfiguration5Menu()
{
    static bool wpnoff_offhand = false;

    const auto getIdx = []
    {
        return wpnoff_offhand ? VR_GetOffHandWpnCvarEntry()
                              : VR_GetMainHandWpnCvarEntry();
    };

    const float oInc = 0.1f;
    constexpr float oBound = 100.f;

    const float rInc = 0.2f;
    constexpr float rBound = 180.f;

    const quake::menu_bounds<float> oBounds{oInc, -oBound, oBound};
    const quake::menu_bounds<float> rBounds{rInc, -rBound, rBound};
    const quake::menu_bounds<float> zbBounds{0.05f, 0.f, 1.f};

    // ------------------------------------------------------------------------

    quake::menu m{"Weapon Configuration (5) - Text", &M_Menu_QuakeVRDevTools_f};

    m.on_key(
        [](int, quake::impl::menu_entry&)
        {
            // TODO VR: (P2) hackish
            VR_ModAllWeapons();
        });

    // ------------------------------------------------------------------------

    const auto o_wpncvar = [&](const char* title, const WpnCVar c)
    {
        return m.add_cvar_getter_entry<float>(                   //
            title,                                               //
            [getIdx, c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            oBounds                                              //
        );
    };

    const auto r_wpncvar = [&](const char* title, const WpnCVar c)
    {
        return m.add_cvar_getter_entry<float>(                   //
            title,                                               //
            [getIdx, c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            rBounds                                              //
        );
    };

    const auto zb_wpncvar = [&](const char* title, const WpnCVar c)
    {
        return m.add_cvar_getter_entry<float>(                   //
            title,                                               //
            [getIdx, c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            zbBounds                                             //
        );
    };

    const auto makeHoverFn = [&](int& implVar)
    {
        return [&](const bool x)
        {
            if(!x)
            {
                implVar = 0;
                return;
            }

            implVar = wpnoff_offhand ? 2 : 1;
        };
    };

    // ------------------------------------------------------------------------

    m.add_getter_entry<bool>(          //
        "Off-Hand",                    //
        [] { return &wpnoff_offhand; } //
    );

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<WpnTextMode>(                                //
         "Text Mode",                                                         //
         [getIdx] { return &VR_GetWpnCVar(getIdx(), WpnCVar::WpnTextMode); }, //
         "Disabled", "Ammo"                                                   //
         )
        .tooltip("Type of text.");

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const char* btnOffsetTooltip = "Offset of the weapon text.";

    o_wpncvar("Text X", WpnCVar::WpnTextX).tooltip(btnOffsetTooltip);
    o_wpncvar("Text Y", WpnCVar::WpnTextY).tooltip(btnOffsetTooltip);
    o_wpncvar("Text Z", WpnCVar::WpnTextZ).tooltip(btnOffsetTooltip);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const char* btnAngleTooltip = "Angle offset of the weapon text.";

    r_wpncvar("Text Pitch", WpnCVar::WpnTextPitch).tooltip(btnAngleTooltip);
    r_wpncvar("Text Yaw", WpnCVar::WpnTextYaw).tooltip(btnAngleTooltip);
    r_wpncvar("Text Roll", WpnCVar::WpnTextRoll).tooltip(btnAngleTooltip);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    const auto hoverWpnTextAnchorVertex =
        makeHoverFn(quake::vr::showfn::vr_impl_draw_wpntext_anchor_vertex);

    m.add_cvar_getter_entry<int>( //
         "Text Anchor Vertex",    //
         [getIdx]
         { return &VR_GetWpnCVar(getIdx(), WpnCVar::WpnTextAnchorVertex); }, //
         {1, 0, 4096}                                                        //
         )
        .hover(hoverWpnTextAnchorVertex)
        .tooltip("Index of the mesh vertex where the text will be attached.");

    zb_wpncvar("Text Scale", WpnCVar::WpnTextScale)
        .tooltip("Scale of the text.");

    return m;
}

[[nodiscard]] static quake::menu& qvrdtWeaponConfiguration5Menu()
{
    static quake::menu res = makeQVRDTWeaponConfiguration5Menu();
    return res;
}

//=============================================================================
/* QUAKE VR DEV TOOLS MENU - FINGER CONFIGURATION */

[[nodiscard]] static quake::menu makeQVRDTFingerConfigurationMenu()
{
    const float oInc = 0.025f;
    constexpr float oBound = 100.f;

    const quake::menu_bounds<float> oBounds{oInc, -oBound, oBound};

    // ------------------------------------------------------------------------

    quake::menu m{"Finger Configuration", &M_Menu_QuakeVRDevTools_f};

    // ------------------------------------------------------------------------

#define MAKE_XYZ_CONTROLS(name, cvar_family_prefix)                          \
    {                                                                        \
        m.add_cvar_entry<float>(name " X", cvar_family_prefix##_x, oBounds); \
        m.add_cvar_entry<float>(name " Y", cvar_family_prefix##_y, oBounds); \
        m.add_cvar_entry<float>(name " Z", cvar_family_prefix##_z, oBounds); \
        m.add_separator();                                                   \
    }

    MAKE_XYZ_CONTROLS("All Fingers And Base", vr_fingers_and_base);
    MAKE_XYZ_CONTROLS(
        "All Fingers And Base (Off-Hand)", vr_fingers_and_base_offhand);
    MAKE_XYZ_CONTROLS("All Fingers", vr_fingers);
    MAKE_XYZ_CONTROLS("Thumb", vr_finger_thumb);
    MAKE_XYZ_CONTROLS("Index", vr_finger_index);
    MAKE_XYZ_CONTROLS("Middle", vr_finger_middle);
    MAKE_XYZ_CONTROLS("Ring", vr_finger_ring);
    MAKE_XYZ_CONTROLS("Pinky", vr_finger_pinky);
    MAKE_XYZ_CONTROLS("Base", vr_finger_base);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<bool>("Finger Blending", vr_finger_blending);
    m.add_cvar_entry<float>(
        "Finger Blending Speed", vr_finger_blending_speed, {0.5f, 0.f, 100.f});

    return m;
}

[[nodiscard]] static quake::menu& qvrdtFingerConfigurationMenu()
{
    static quake::menu res = makeQVRDTFingerConfigurationMenu();
    return res;
}

//=============================================================================
/* QUAKE VR DEV TOOLS MENU - DEBUG UTILITIES */

[[nodiscard]] static quake::menu makeQVRDTDebugUtilitiesMenu()
{
    const auto runCmd = [](const char* cmd)
    {
        return [cmd]
        {
            quake::menu_util::playMenuSound("items/r_item2.wav", 0.5);
            Cmd_ExecuteString(cmd, cmd_source_t::src_command);
        };
    };

    // ------------------------------------------------------------------------

    quake::menu m{"Debug Utilities", &M_Menu_QuakeVRDevTools_f};

    m.add_cvar_entry<bool>(
        "Force-grabbable ammo items", vr_forcegrabbable_ammo_boxes);

    m.add_cvar_entry<bool>(
        "Force-grabbable health items", vr_forcegrabbable_health_boxes);

    m.add_cvar_entry<float>("Return item time (DM)",
        vr_forcegrabbable_return_time_deathmatch, {0.5f, 0.f, 50.f});

    m.add_cvar_entry<float>("Return item time (SP)",
        vr_forcegrabbable_return_time_singleplayer, {0.5f, 0.f, 50.f});

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<float>("Throw Up Center Of Mass",
        vr_throw_up_center_of_mass, {0.01f, 0.f, 10.f});

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<int>("Throw Avg Frames", vr_throw_avg_frames, {1, 1, 50});

    m.add_action_entry("Reset Throw Avg Frames", &VR_ResetThrowAvgFrames);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_entry<int>("Autosave Period", vr_autosave_seconds, {5, 5, 2400});
    m.add_cvar_entry<bool>(
        "Autosave On Changelevel", vr_autosave_on_changelevel);
    m.add_cvar_entry<bool>("Autosave Messages", vr_autosave_show_message);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_action_entry("Impulse 9 (Give All)", runCmd("impulse 9"));
    m.add_action_entry("Impulse 11 (Rune)", runCmd("impulse 11"));
    m.add_action_entry("Impulse 14 (Spawn All)", runCmd("impulse 14"));
    m.add_action_entry("Impulse 17 (Spawn Grapple)", runCmd("impulse 17"));
    m.add_action_entry("Impulse 254 (Invisibility)", runCmd("impulse 254"));
    m.add_action_entry("Impulse 255 (Quad)", runCmd("impulse 255"));
    m.add_action_entry("God Mode", runCmd("god"));
    m.add_action_entry("Noclip", runCmd("noclip"));
    m.add_action_entry("Fly", runCmd("fly"));

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    extern cvar_t skill;
    m.add_cvar_getter_enum_entry<int>(        //
        "Skill",                              //
        [] { return &skill; },                //
        "Easy", "Normal", "Hard", "Nightmare" //
    );

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    extern cvar_t r_showbboxes;
    m.add_cvar_entry<bool>("Show BBoxes", r_showbboxes);

    extern cvar_t r_showbboxes_player;
    m.add_cvar_entry<bool>("Show Player BBoxes", r_showbboxes_player);

    m.add_cvar_entry<bool>(
        "Show VR Torso Debug Lines", vr_vrtorso_debuglines_enabled);

    m.add_cvar_entry<bool>("Fake VR Mode", vr_fakevr);

    extern cvar_t host_timescale;
    m.add_cvar_entry<float>("Timescale", host_timescale, {0.05f, 0.f, 5.f});

    m.add_cvar_entry<bool>("Print Handvel", vr_debug_print_handvel);
    m.add_cvar_entry<bool>("Print Headvel", vr_debug_print_headvel);
    m.add_cvar_entry<bool>("Show Hand Pos/Rot", vr_debug_show_hand_pos_and_rot);

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<VrAimMode>( //
        "Aim Mode",                          //
        [] { return &vr_aimmode; },          //
        "HEAD_MYAW",                         //
        "HEAD_MYAW_MPITCH",                  //
        "MOUSE_MYAW",                        //
        "MOUSE_MYAW_MPITCH",                 //
        "BLENDED",                           //
        "BLENDED_NOPITCH",                   //
        "CONTROLLER"                         //
    );

    m.add_cvar_entry<bool>("Viewkick", vr_viewkick);

    return m;
}

[[nodiscard]] static quake::menu& qvrdtDebugUtilitiesMenu()
{
    static quake::menu res = makeQVRDTDebugUtilitiesMenu();
    return res;
}

//=============================================================================
/* QUAKE VR DEV TOOLS MENU */

template <typename F>
static void forQVRDTMenus(F&& f)
{
    f(qvrdtWeaponConfiguration1Menu(), m_qvrdt_weaponconfiguration1);
    f(qvrdtWeaponConfiguration2Menu(), m_qvrdt_weaponconfiguration2);
    f(qvrdtWeaponConfiguration3Menu(), m_qvrdt_weaponconfiguration3);
    f(qvrdtWeaponConfiguration4Menu(), m_qvrdt_weaponconfiguration4);
    f(qvrdtWeaponConfiguration5Menu(), m_qvrdt_weaponconfiguration5);
    f(qvrdtFingerConfigurationMenu(), m_qvrdt_fingerconfiguration);
    f(qvrdtDebugUtilitiesMenu(), m_qvrdt_debugutilities);
}

[[nodiscard]] static quake::menu makeQuakeVRDevToolsMenu()
{
    quake::menu m{"Quake VR - Dev Tools", &M_Menu_Main_f};

    const auto makeGotoMenu = [&](quake::menu& xm, m_state_e s)
    {
        m.add_action_entry(
            xm.title(), [&xm, s] { quake::menu_util::setMenuState(xm, s); });
    };

    forQVRDTMenus(makeGotoMenu);
    return m;
}

[[nodiscard]] static quake::menu& quakeVRDevToolsMenu()
{
    static quake::menu res = makeQuakeVRDevToolsMenu();
    return res;
}

void M_Menu_QuakeVRDevTools_f()
{
    key_dest = key_menu;
    m_state = m_quakevrdevtools;
    m_entersound = true;

    IN_UpdateGrabs(); // QSS
}

void M_QuakeVRDevTools_Draw()
{
    quakeVRDevToolsMenu().draw();
}

void M_QuakeVRDevTools_Key(int k)
{
    quakeVRDevToolsMenu().key(k);
}

//=============================================================================
/* QUAKE VR CHANGE MAP - IMPL */

enum class ChangeMapCommand : int
{
    Map = 0,
    Changelevel = 1,
};

[[nodiscard]] static int& getChangeMapCommand()
{
    static int cmd{static_cast<int>(ChangeMapCommand::Map)};
    return cmd;
}

template <typename Range>
[[nodiscard]] static quake::menu makeQVRCMChangeMapMenuImpl(
    const std::string_view name, const Range& maps)
{
    const auto changeMap = [&maps](const int option)
    {
        return [&maps, option]
        {
            quake::menu_util::playMenuSound("items/r_item2.wav", 0.5);

            const char* cmd =
                static_cast<ChangeMapCommand>(getChangeMapCommand()) ==
                        ChangeMapCommand::Map
                    ? "map"
                    : "changelevel";

            Cmd_ExecuteString(
                va("%s %s", cmd, maps[option].data()), src_command);
        };
    };

    // ------------------------------------------------------------------------

    quake::menu m{name, &M_Menu_QuakeVRChangeMap_f, true};

    int idx{0};
    for(const auto& map : maps)
    {
        m.add_action_entry(map, changeMap(idx));
        ++idx;
    }

    return m;
}

using namespace std::literals;

constexpr std::array mapsVanilla{"e1m1"sv, "e1m2"sv, "e1m3"sv, "e1m4"sv,
    "e1m5"sv, "e1m6"sv, "e1m7"sv, "e1m8"sv, "e2m1"sv, "e2m2"sv, "e2m3"sv,
    "e2m4"sv, "e2m5"sv, "e2m6"sv, "e2m7"sv, "e3m1"sv, "e3m2"sv, "e3m3"sv,
    "e3m4"sv, "e3m5"sv, "e3m6"sv, "e3m7"sv, "e4m1"sv, "e4m2"sv, "e4m3"sv,
    "e4m4"sv, "e4m5"sv, "e4m6"sv, "e4m7"sv, "e4m8"sv, "end"sv};

constexpr std::array mapsSoa{"hip1m1"sv, "hip1m2"sv, "hip1m3"sv, "hip1m4"sv,
    "hip1m5"sv, "hip2m1"sv, "hip2m2"sv, "hip2m3"sv, "hip2m4"sv, "hip2m5"sv,
    "hip2m6"sv, "hip3m1"sv, "hip3m2"sv, "hip3m3"sv, "hip3m4"sv, "hipdm1"sv,
    "hipend"sv};

constexpr std::array mapsDoe{"r1m1"sv, "r1m2"sv, "r1m3"sv, "r1m4"sv, "r1m5"sv,
    "r1m6"sv, "r1m7"sv, "r2m1"sv, "r2m2"sv, "r2m3"sv, "r2m4"sv, "r2m5"sv,
    "r2m6"sv, "r2m7"sv, "r2m8"sv, "ctf1"sv};

constexpr std::array mapsDopa{"e5m1"sv, "e5m2"sv, "e5m3"sv, "e5m4"sv, "e5m5"sv,
    "e5m6"sv, "e5m7"sv, "e5m8"sv, "e5end"sv, "e5dm"sv};

constexpr std::array mapsHoney{
    "saint"sv, "honey"sv, "h_hub1"sv, "h_hub2"sv, "h_end"sv, "credits"sv};

//=============================================================================
/* QUAKE VR CHANGE MAP - VANILLA */

[[nodiscard]] static quake::menu makeQVRCMVanillaMenu()
{
    return makeQVRCMChangeMapMenuImpl("Vanilla", mapsVanilla);
}

[[nodiscard]] static quake::menu& qvrcmVanillaMenu()
{
    static quake::menu res = makeQVRCMVanillaMenu();
    return res;
}

//=============================================================================
/* QUAKE VR CHANGE MAP - SOA */

[[nodiscard]] static quake::menu makeQVRCMSoaMenu()
{
    return makeQVRCMChangeMapMenuImpl("Scourge of Armagon", mapsSoa);
}

[[nodiscard]] static quake::menu& qvrcmSoaMenu()
{
    static quake::menu res = makeQVRCMSoaMenu();
    return res;
}

//=============================================================================
/* QUAKE VR CHANGE MAP - DOE */

[[nodiscard]] static quake::menu makeQVRCMDoeMenu()
{
    return makeQVRCMChangeMapMenuImpl("Dissolution of Eternity", mapsDoe);
}

[[nodiscard]] static quake::menu& qvrcmDoeMenu()
{
    static quake::menu res = makeQVRCMDoeMenu();
    return res;
}

//=============================================================================
/* QUAKE VR CHANGE MAP - DOPA */

[[nodiscard]] static quake::menu makeQVRCMDopaMenu()
{
    return makeQVRCMChangeMapMenuImpl("Dimensions of the Past", mapsDopa);
}

[[nodiscard]] static quake::menu& qvrcmDopaMenu()
{
    static quake::menu res = makeQVRCMDopaMenu();
    return res;
}

//=============================================================================
/* QUAKE VR CHANGE MAP - HONEY */

[[nodiscard]] static quake::menu makeQVRCMHoneyMenu()
{
    return makeQVRCMChangeMapMenuImpl("Honey (czg)", mapsHoney);
}

[[nodiscard]] static quake::menu& qvrcmHoneyMenu()
{
    static quake::menu res = makeQVRCMHoneyMenu();
    return res;
}

//=============================================================================
/* QUAKE VR CHANGE MAP - EXTRA */

[[nodiscard]] static quake::menu makeQVRCMExtraMenu()
{
    static std::vector<std::string_view> mapsExtra;

    int i;
    filelist_item_t* level;

    for(level = extralevels, i = 0; level; level = level->next, i++)
    {
        mapsExtra.emplace_back(level->name);
    }

    return makeQVRCMChangeMapMenuImpl("Custom Maps", mapsExtra);
}

[[nodiscard]] static quake::menu& qvrcmExtraMenu()
{
    static quake::menu res = makeQVRCMExtraMenu();
    return res;
}


//=============================================================================
/* QUAKE VR CHANGE MAP MENU */

template <typename F>
static void forQVRCMMenus(F&& f)
{
    f(qvrcmVanillaMenu(), m_qvrs_changemap_vanilla);
    f(qvrcmSoaMenu(), m_qvrs_changemap_soa);
    f(qvrcmDoeMenu(), m_qvrs_changemap_doe);
    f(qvrcmDopaMenu(), m_qvrs_changemap_dopa);
    f(qvrcmHoneyMenu(), m_qvrs_changemap_honey);
    f(qvrcmExtraMenu(), m_qvrs_changemap_extra);
}

[[nodiscard]] static quake::menu makeQuakeVRChangeMap()
{
    quake::menu m{"Quake VR - Change Map", &M_Menu_Main_f};

    const auto makeGotoMenu = [&](quake::menu& xm, m_state_e s)
    {
        m.add_action_entry(
            xm.title(), [&xm, s] { quake::menu_util::setMenuState(xm, s); });
    };

    m.add_getter_enum_entry<ChangeMapCommand>( //
        "Preserve Equipment",                  //
        [] { return &getChangeMapCommand(); }, //
        "No (map)",                            //
        "Yes (changelevel)"                    //
    );

    // ------------------------------------------------------------------------
    m.add_separator();
    // ------------------------------------------------------------------------

    forQVRCMMenus(makeGotoMenu);

    return m;
}

[[nodiscard]] static quake::menu& quakeVRChangeMapMenu()
{
    static quake::menu res = makeQuakeVRChangeMap();
    return res;
}

void M_Menu_QuakeVRChangeMap_f()
{
    key_dest = key_menu;
    m_state = m_quakevrchangemap;
    m_entersound = true;

    IN_UpdateGrabs(); // QSS
}

void M_QuakeVRChangeMap_Draw()
{
    quakeVRChangeMapMenu().draw();
}

void M_QuakeVRChangeMap_Key(int k)
{
    quakeVRChangeMapMenu().key(k);
}

//=============================================================================
/* KEYS MENU */

const char* bindnames[][2] = {{"+attack", "attack"},
    {"impulse 10", "next weapon"}, {"impulse 12", "prev weapon"},
    {"impulse 13", "cycle off-hand weapons"}, {"+jump", "jump / swim up"},
    {"+forward", "walk forward"}, {"+back", "backpedal"},
    {"+left", "turn left"}, {"+right", "turn right"}, {"+speed", "run"},
    {"+moveleft", "step left"}, {"+moveright", "step right"},
    {"+strafe", "sidestep"}, {"+lookup", "look up"}, {"+lookdown", "look down"},
    {"centerview", "center view"}, {"+mlook", "mouse look"},
    {"+klook", "keyboard look"}, {"+moveup", "swim up"},
    {"+movedown", "swim down"}, {"+voip", "Voice Chat"}};

#define NUMCOMMANDS (sizeof(bindnames) / sizeof(bindnames[0]))

static int keys_cursor;
bool bind_grab; // QSS

void M_Menu_Keys_f()
{
    key_dest = key_menu;
    m_state = m_keys;
    m_entersound = true;

    IN_UpdateGrabs(); // QSS
}


void M_FindKeysForCommand(const char* command, int* threekeys)
{
    int count;
    int j;
    int l;
    char* b;
    int bindmap = 0;

    threekeys[0] = threekeys[1] = threekeys[2] = -1;
    l = strlen(command);
    count = 0;

    for(j = 0; j < MAX_KEYS; j++)
    {
        b = keybindings[bindmap][j];
        if(!b)
        {
            continue;
        }
        if(!strncmp(b, command, l))
        {
            threekeys[count] = j;
            count++;
            if(count == 3)
            {
                break;
            }
        }
    }
}

void M_UnbindCommand(const char* command)
{
    int j;
    int l;
    char* b;
    int bindmap = 0;

    l = strlen(command);

    for(j = 0; j < MAX_KEYS; j++)
    {
        b = keybindings[bindmap][j];
        if(!b)
        {
            continue;
        }
        if(!strncmp(b, command, l))
        {
            Key_SetBinding(j, nullptr, bindmap);
        }
    }
}

extern qpic_t *pic_up, *pic_down;

void M_Keys_Draw()
{
    int i;
    int x;
    int y;
    int keys[3];
    const char* name;
    qpic_t* p;

    p = Draw_CachePic("gfx/ttl_cstm.lmp");
    M_DrawPic((320 - p->width) / 2, 4, p);

    if(bind_grab)
    {
        M_Print(12, 32, "Press a key or button for this action");
    }
    else
    {
        M_Print(18, 32, "Enter to change, backspace to clear");
    }

    // search for known bindings
    for(i = 0; i < (int)NUMCOMMANDS; i++)
    {
        y = 48 + 8 * i;

        M_Print(16, y, bindnames[i][1]);

        M_FindKeysForCommand(bindnames[i][0], keys);

        if(keys[0] == -1)
        {
            M_Print(140, y, "???");
        }
        else
        {
            name = Key_KeynumToString(keys[0]);
            M_Print(140, y, name);
            x = strlen(name) * 8;
            if(keys[1] != -1)
            {
                name = Key_KeynumToString(keys[1]);
                M_Print(140 + x + 8, y, "or");
                M_Print(140 + x + 32, y, name);
                x += 32 + strlen(name) * 8;
                if(keys[2] != -1)
                {
                    M_Print(140 + x + 8, y, "or");
                    M_Print(140 + x + 32, y, Key_KeynumToString(keys[2]));
                }
            }
        }
    }

    if(bind_grab)
    {
        M_DrawCharacter(130, 48 + keys_cursor * 8, '=');
    }
    else
    {
        M_DrawCharacter(
            130, 48 + keys_cursor * 8, 12 + ((int)(realtime * 4) & 1));
    }
}


void M_Keys_Key(int k)
{
    char cmd[80];
    int keys[3];

    if(bind_grab)
    {
        // defining a key
        S_LocalSound("misc/menu1.wav");
        if((k != K_ESCAPE) && (k != '`'))
        {
            sprintf(cmd, "bind \"%s\" \"%s\"\n", Key_KeynumToString(k),
                bindnames[keys_cursor][0]);
            Cbuf_InsertText(cmd);
        }

        bind_grab = false;

        IN_UpdateGrabs(); // QSS
        return;
    }

    switch(k)
    {
        case K_ESCAPE:
        case K_BBUTTON: M_Menu_Options_f(); break;

        case K_LEFTARROW:
        case K_UPARROW:
            S_LocalSound("misc/menu1.wav");
            keys_cursor--;
            if(keys_cursor < 0)
            {
                keys_cursor = NUMCOMMANDS - 1;
            }
            break;

        case K_DOWNARROW:
        case K_RIGHTARROW:
            S_LocalSound("misc/menu1.wav");
            keys_cursor++;
            if(keys_cursor >= (int)NUMCOMMANDS)
            {
                keys_cursor = 0;
            }
            break;

        case K_ENTER: // go into bind mode
        case K_KP_ENTER:
        case K_ABUTTON:
            M_FindKeysForCommand(bindnames[keys_cursor][0], keys);
            S_LocalSound("misc/menu2.wav");
            if(keys[2] != -1)
            {
                M_UnbindCommand(bindnames[keys_cursor][0]);
            }
            bind_grab = true;
            IN_UpdateGrabs(); // QSS
            break;

        case K_BACKSPACE: // delete bindings
        case K_DEL:
            S_LocalSound("misc/menu2.wav");
            M_UnbindCommand(bindnames[keys_cursor][0]);
            break;
    }
}

//=============================================================================
/* VIDEO MENU */

void M_Menu_Video_f()
{
    (*vid_menucmdfn)(); // johnfitz
}


void M_Video_Draw()
{
    (*vid_menudrawfn)();
}


void M_Video_Key(int key)
{
    (*vid_menukeyfn)(key);
}

//=============================================================================
/* HELP MENU */

int help_page;
#define NUM_HELP_PAGES 6


void M_Menu_Help_f()
{
    key_dest = key_menu;
    m_state = m_help;
    m_entersound = true;
    help_page = 0;

    IN_UpdateGrabs(); // QSS
}



void M_Help_Draw()
{
    M_DrawPic(0, 0, Draw_CachePic(va("gfx/help%i.lmp", help_page)));
}


void M_Help_Key(int key)
{
    switch(key)
    {
        case K_ESCAPE:
        case K_BBUTTON: M_Menu_Main_f(); break;

        case K_UPARROW:
        case K_RIGHTARROW:
            m_entersound = true;
            if(++help_page >= NUM_HELP_PAGES)
            {
                help_page = 0;
            }
            break;

        case K_DOWNARROW:
        case K_LEFTARROW:
            m_entersound = true;
            if(--help_page < 0)
            {
                help_page = NUM_HELP_PAGES - 1;
            }
            break;
    }
}

//=============================================================================
/* QUIT MENU */

int msgNumber;
enum m_state_e m_quit_prevstate;
bool wasInMenus;

void M_Menu_Quit_f()
{
    if(m_state == m_quit)
    {
        return;
    }
    wasInMenus = (key_dest == key_menu);
    key_dest = key_menu;
    m_quit_prevstate = m_state;
    m_state = m_quit;
    m_entersound = true;
    msgNumber = rand() & 7;

    IN_UpdateGrabs(); // QSS
}


void M_Quit_Key(int key)
{
    if(key == K_ESCAPE)
    {
        if(wasInMenus)
        {
            m_state = m_quit_prevstate;
            m_entersound = true;
        }
        else
        {
            key_dest = key_game;
            m_state = m_none;
            IN_UpdateGrabs(); // QSS
        }
    }
    else
    {
        key_dest = key_console;
        Host_Quit_f();
        IN_UpdateGrabs(); // QSS
    }
}


void M_Quit_Char(int key)
{
    switch(key)
    {
        case 'n':
        case 'N':
            if(wasInMenus)
            {
                m_state = m_quit_prevstate;
                m_entersound = true;
            }
            else
            {
                key_dest = key_game;
                m_state = m_none;
                IN_UpdateGrabs(); // QSS
            }
            break;

        case 'y':
        case 'Y':
            key_dest = key_console;
            Host_Quit_f();
            IN_UpdateGrabs(); // QSS
            break;

        default: break;
    }
}


bool M_Quit_TextEntry()
{
    return true;
}


void M_Quit_Draw() // johnfitz -- modified for new quit message
{
    char msg1[64];
    char msg2[] =
        "by Vittorio Romeo, Ozkan Sezer, Eric Wasylishen, others"; /* msg2/msg3
                                                      are mostly [40] */
    char msg3[] = "Press y to quit";
    int boxlen;

    if(wasInMenus)
    {
        m_state = m_quit_prevstate;
        m_recursiveDraw = true;
        M_Draw();
        m_state = m_quit;
    }

    sprintf(msg1, "QuakeSpasm " QUAKESPASM_VER_STRING);

    // okay, this is kind of fucked up.  M_DrawTextBox will always act as if
    // width is even. Also, the width and lines values are for the interior
    // of the box, but the x and y values include the border.
    boxlen =
        q_max(strlen(msg1), q_max((sizeof(msg2) - 1), (sizeof(msg3) - 1))) + 1;
    if(boxlen & 1)
    {
        boxlen++;
    }
    M_DrawTextBox(260 - 4 * (boxlen + 2), 76, boxlen, 4);

    // now do the text
    M_Print(260 - 4 * strlen(msg1), 88, msg1);
    M_Print(260 - 4 * (sizeof(msg2) - 1), 96, msg2);
    M_PrintWhite(260 - 4 * (sizeof(msg3) - 1), 104, msg3);
}

//=============================================================================
/* LAN CONFIG MENU */

int lanConfig_cursor = -1;
int lanConfig_cursor_table[] = {72, 92, 124};
#define NUM_LANCONFIG_CMDS 3

int lanConfig_port;
char lanConfig_portname[6];
char lanConfig_joinname[22];

void M_Menu_LanConfig_f()
{
    key_dest = key_menu;
    m_state = m_lanconfig;
    m_entersound = true;
    if(lanConfig_cursor == -1)
    {
        if(JoiningGame)
        {
            lanConfig_cursor = 2;
        }
        else
        {
            lanConfig_cursor = 1;
        }
    }
    if(StartingGame && lanConfig_cursor == 2)
    {
        lanConfig_cursor = 1;
    }
    lanConfig_port = DEFAULTnet_hostport;
    sprintf(lanConfig_portname, "%u", lanConfig_port);

    m_return_onerror = false;
    m_return_reason[0] = 0;

    IN_UpdateGrabs(); // QSS
}


void M_LanConfig_Draw()
{
    qpic_t* p;
    int basex;
    const char* startJoin;
    const char* protocol;

    M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
    p = Draw_CachePic("gfx/p_multi.lmp");
    basex = (320 - p->width) / 2;
    M_DrawPic(basex, 4, p);

    if(StartingGame)
    {
        startJoin = "New Game";
    }
    else
    {
        startJoin = "Join Game";
    }

    protocol = "TCP/IP";

    M_Print(basex, 32, va("%s - %s", startJoin, protocol));
    basex += 8;

    M_Print(basex, 52, "Address:");

    // QSS
    int y = 52;
    qhostaddr_t addresses[16];
    int numaddresses =
        NET_ListAddresses(addresses, sizeof(addresses) / sizeof(addresses[0]));
    if(!numaddresses)
    {
        M_Print(basex + 9 * 8, y, "NONE KNOWN");
        y += 8;
    }
    else
    {
        for(int i = 0; i < numaddresses; i++)
        {
            M_Print(basex + 9 * 8, y, addresses[i]);
            y += 8;
        }
    }

    M_Print(basex, lanConfig_cursor_table[0], "Port");
    M_DrawTextBox(basex + 8 * 8, lanConfig_cursor_table[0] - 8, 6, 1);
    M_Print(basex + 9 * 8, lanConfig_cursor_table[0], lanConfig_portname);

    if(JoiningGame)
    {
        M_Print(basex, lanConfig_cursor_table[1], "Search for local games...");
        M_Print(basex, 108, "Join game at:");
        M_DrawTextBox(basex + 8, lanConfig_cursor_table[2] - 8, 22, 1);
        M_Print(basex + 16, lanConfig_cursor_table[2], lanConfig_joinname);
    }
    else
    {
        M_DrawTextBox(basex, lanConfig_cursor_table[1] - 8, 2, 1);
        M_Print(basex + 8, lanConfig_cursor_table[1], "OK");
    }

    M_DrawCharacter(basex - 8, lanConfig_cursor_table[lanConfig_cursor],
        12 + ((int)(realtime * 4) & 1));

    if(lanConfig_cursor == 0)
    {
        M_DrawCharacter(basex + 9 * 8 + 8 * strlen(lanConfig_portname),
            lanConfig_cursor_table[0], 10 + ((int)(realtime * 4) & 1));
    }

    if(lanConfig_cursor == 2)
    {
        M_DrawCharacter(basex + 16 + 8 * strlen(lanConfig_joinname),
            lanConfig_cursor_table[2], 10 + ((int)(realtime * 4) & 1));
    }

    if(*m_return_reason)
    {
        M_PrintWhite(basex, 148, m_return_reason);
    }
}


void M_LanConfig_Key(int key)
{
    int l;

    switch(key)
    {
        case K_ESCAPE:
        case K_BBUTTON: M_Menu_MultiPlayer_f(); break;

        case K_UPARROW:
            S_LocalSound("misc/menu1.wav");
            lanConfig_cursor--;
            if(lanConfig_cursor < 0)
            {
                lanConfig_cursor = NUM_LANCONFIG_CMDS - 1;
            }
            break;

        case K_DOWNARROW:
            S_LocalSound("misc/menu1.wav");
            lanConfig_cursor++;
            if(lanConfig_cursor >= NUM_LANCONFIG_CMDS)
            {
                lanConfig_cursor = 0;
            }
            break;

        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON:
            if(lanConfig_cursor == 0)
            {
                break;
            }

            m_entersound = true;

            M_ConfigureNetSubsystem();

            if(lanConfig_cursor == 1)
            {
                if(StartingGame)
                {
                    M_Menu_GameOptions_f();
                    break;
                }
                M_Menu_Search_f(SLIST_INTERNET);
                break;
            }

            if(lanConfig_cursor == 2)
            {
                m_return_state = m_state;
                m_return_onerror = true;
                key_dest = key_game;
                m_state = m_none;
                Cbuf_AddText(va("connect \"%s\"\n", lanConfig_joinname));

                IN_UpdateGrabs(); // QSS
                break;
            }

            break;

        case K_BACKSPACE:
            if(lanConfig_cursor == 0)
            {
                if(strlen(lanConfig_portname))
                {
                    lanConfig_portname[strlen(lanConfig_portname) - 1] = 0;
                }
            }

            if(lanConfig_cursor == 2)
            {
                if(strlen(lanConfig_joinname))
                {
                    lanConfig_joinname[strlen(lanConfig_joinname) - 1] = 0;
                }
            }
            break;
    }

    if(StartingGame && lanConfig_cursor == 2)
    {
        if(key == K_UPARROW)
        {
            lanConfig_cursor = 1;
        }
        else
        {
            lanConfig_cursor = 0;
        }
    }

    l = Q_atoi(lanConfig_portname);
    if(l > 65535)
    {
        l = lanConfig_port;
    }
    else
    {
        lanConfig_port = l;
    }
    sprintf(lanConfig_portname, "%u", lanConfig_port);
}


void M_LanConfig_Char(int key)
{
    int l;

    switch(lanConfig_cursor)
    {
        case 0:
            if(key < '0' || key > '9')
            {
                return;
            }
            l = strlen(lanConfig_portname);
            if(l < 5)
            {
                lanConfig_portname[l + 1] = 0;
                lanConfig_portname[l] = key;
            }
            break;
        case 2:
            l = strlen(lanConfig_joinname);
            if(l < 21)
            {
                lanConfig_joinname[l + 1] = 0;
                lanConfig_joinname[l] = key;
            }
            break;
    }
}


bool M_LanConfig_TextEntry()
{
    return (lanConfig_cursor == 0 || lanConfig_cursor == 2);
}

//=============================================================================
/* GAME OPTIONS MENU */

typedef struct
{
    const char* name;
    const char* description;
} level_t;

level_t levels[] = {

    {"e1m1", "Slipgate Complex"},     // 0
    {"e1m2", "Castle of the Damned"}, // 1
    {"e1m3", "The Necropolis"},       // 2
    {"e1m4", "The Grisly Grotto"},    // 3
    {"e1m5", "Gloom Keep"},           // 4
    {"e1m6", "The Door To Chthon"},   // 5
    {"e1m7", "The House of Chthon"},  // 6
    {"e1m8", "Ziggurat Vertigo"},     // 7

    {"e2m1", "The Installation"},     // 8
    {"e2m2", "Ogre Citadel"},         // 9
    {"e2m3", "Crypt of Decay"},       // 10
    {"e2m4", "The Ebon Fortress"},    // 11
    {"e2m5", "The Wizard's Manse"},   // 12
    {"e2m6", "The Dismal Oubliette"}, // 13
    {"e2m7", "Underearth"},           // 14

    {"e3m1", "Termination Central"},  // 15
    {"e3m2", "The Vaults of Zin"},    // 16
    {"e3m3", "The Tomb of Terror"},   // 17
    {"e3m4", "Satan's Dark Delight"}, // 18
    {"e3m5", "Wind Tunnels"},         // 19
    {"e3m6", "Chambers of Torment"},  // 20
    {"e3m7", "The Haunted Halls"},    // 21

    {"e4m1", "The Sewage System"},    // 22
    {"e4m2", "The Tower of Despair"}, // 23
    {"e4m3", "The Elder God Shrine"}, // 24
    {"e4m4", "The Palace of Hate"},   // 25
    {"e4m5", "Hell's Atrium"},        // 26
    {"e4m6", "The Pain Maze"},        // 27
    {"e4m7", "Azure Agony"},          // 28
    {"e4m8", "The Nameless City"},    // 29

    {"end", "Shub-Niggurath's Pit"}, // 30

    {"dm1", "Place of Two Deaths"}, // 31
    {"dm2", "Claustrophobopolis"},  // 32
    {"dm3", "The Abandoned Base"},  // 33
    {"dm4", "The Bad Place"},       // 34
    {"dm5", "The Cistern"},         // 35
    {"dm6", "The Dark Zone"},       // 36

    {"hip1m1", "The Pumping Station"}, // 37
    {"hip1m2", "Storage Facility"},    // 38
    {"hip1m3", "The Lost Mine"},       // 39
    {"hip1m4", "Research Facility"},   // 40
    {"hip1m5", "Military Complex"},    // 41

    {"hip2m1", "Ancient Realms"},       // 42
    {"hip2m2", "The Black Cathedral"},  // 43
    {"hip2m3", "The Catacombs"},        // 44
    {"hip2m4", "The Crypt"},            // 45
    {"hip2m5", "Mortum's Keep"},        // 46
    {"hip2m6", "The Gremlin's Domain"}, // 47

    {"hip3m1", "Tur Torment"},  // 48
    {"hip3m2", "Pandemonium"},  // 49
    {"hip3m3", "Limbo"},        // 50
    {"hip3m4", "The Gauntlet"}, // 51

    {"hipend", "Armagon's Lair"}, // 52

    {"hipdm1", "The Edge of Oblivion"}, // 53

    {"r1m1", "Deviant's Domain"},     // 54
    {"r1m2", "Dread Portal"},         // 55
    {"r1m3", "Judgement Call"},       // 56
    {"r1m4", "Cave of Death"},        // 57
    {"r1m5", "Towers of Wrath"},      // 58
    {"r1m6", "Temple of Pain"},       // 59
    {"r1m7", "Tomb of the Overlord"}, // 60

    {"r2m1", "Tempus Fugit"},      // 61
    {"r2m2", "Elemental Fury I"},  // 62
    {"r2m3", "Elemental Fury II"}, // 63
    {"r2m4", "Curse of Osiris"},   // 64
    {"r2m5", "Wizard's Keep"},     // 65
    {"r2m6", "Blood Sacrifice"},   // 66
    {"r2m7", "Last Bastion"},      // 67
    {"r2m8", "Source of Evil"},    // 68

    {"ctf1", "Division of Change"} // 69
};

typedef struct
{
    const char* description;
    int firstLevel;
    int lastLevel;
} episode_t;

episode_t episodes[] = {
    //
    {"Vanilla E1", 0, 7},   //
    {"Vanilla E2", 8, 14},  //
    {"Vanilla E3", 15, 21}, //
    {"Vanilla E4", 22, 30}, //
    {"Vanilla DM", 31, 36}, //
    {"SoA E1", 37, 41},     //
    {"SoA E2", 42, 47},     //
    {"SoA E3", 48, 52},     //
    {"SoA DM", 53, 53},     //
    {"DoE E1", 54, 60},     //
    {"DoE E2", 61, 68},     //
    {"DoE DM", 69, 69},     //
};

int startepisode;
int startlevel;
int maxplayers;

void M_Menu_GameOptions_f()
{
    key_dest = key_menu;
    m_state = m_gameoptions;
    m_entersound = true;

    if(maxplayers == 0)
    {
        maxplayers = svs.maxclients;
    }

    if(maxplayers < 2)
    {
        maxplayers = svs.maxclientslimit;
    }

    IN_UpdateGrabs(); // QSS
}


int gameoptions_cursor_table[] = {40, 56, 64, 72, 80, 88, 96, 112, 120};
#define NUM_GAMEOPTIONS 9
int gameoptions_cursor;

void M_GameOptions_Draw()
{
    qpic_t* p;

    M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
    p = Draw_CachePic("gfx/p_multi.lmp");
    M_DrawPic((320 - p->width) / 2, 4, p);

    M_DrawTextBox(152, 32, 10, 1);
    M_Print(160, 40, "begin game");

    M_Print(0, 56, "      Max players");
    M_Print(160, 56, va("%i", maxplayers));

    M_Print(0, 64, "        Game Type");
    if(coop.value)
    {
        M_Print(160, 64, "Cooperative");
    }
    else
    {
        M_Print(160, 64, "Deathmatch");
    }

    M_Print(0, 72, "        Teamplay");
    if(rogue)
    {
        const char* msg;

        switch((int)teamplay.value)
        {
            case 1: msg = "No Friendly Fire"; break;
            case 2: msg = "Friendly Fire"; break;
            case 3: msg = "Tag"; break;
            case 4: msg = "Capture the Flag"; break;
            case 5: msg = "One Flag CTF"; break;
            case 6: msg = "Three Team CTF"; break;
            default: msg = "Off"; break;
        }
        M_Print(160, 72, msg);
    }
    else
    {
        const char* msg;

        switch((int)teamplay.value)
        {
            case 1: msg = "No Friendly Fire"; break;
            case 2: msg = "Friendly Fire"; break;
            default: msg = "Off"; break;
        }
        M_Print(160, 72, msg);
    }

    M_Print(0, 80, "            Skill");
    if(skill.value == 0)
    {
        M_Print(160, 80, "Easy difficulty");
    }
    else if(skill.value == 1)
    {
        M_Print(160, 80, "Normal difficulty");
    }
    else if(skill.value == 2)
    {
        M_Print(160, 80, "Hard difficulty");
    }
    else
    {
        M_Print(160, 80, "Nightmare difficulty");
    }

    M_Print(0, 88, "       Frag Limit");
    if(fraglimit.value == 0)
    {
        M_Print(160, 88, "none");
    }
    else
    {
        M_Print(160, 88, va("%i frags", (int)fraglimit.value));
    }

    M_Print(0, 96, "       Time Limit");
    if(timelimit.value == 0)
    {
        M_Print(160, 96, "none");
    }
    else
    {
        M_Print(160, 96, va("%i minutes", (int)timelimit.value));
    }

    M_Print(0, 112, "         Episode");
    M_Print(160, 112, episodes[startepisode].description);

    M_Print(0, 120, "           Level");
    M_Print(160, 120,
        levels[episodes[startepisode].firstLevel + startlevel].description);
    M_Print(
        160, 128, levels[episodes[startepisode].firstLevel + startlevel].name);

    // line cursor
    M_DrawCharacter(144, gameoptions_cursor_table[gameoptions_cursor],
        12 + ((int)(realtime * 4) & 1));
}


void M_NetStart_Change(int dir)
{
    int count;
    float f;

    switch(gameoptions_cursor)
    {
        case 1:
            maxplayers += dir;
            if(maxplayers > svs.maxclientslimit)
            {
                maxplayers = svs.maxclientslimit;
            }
            if(maxplayers < 2)
            {
                maxplayers = 2;
            }
            break;

        case 2: Cvar_Set("coop", coop.value ? "0" : "1"); break;

        case 3:
            count = (rogue) ? 6 : 2;
            f = teamplay.value + dir;
            if(f > count)
            {
                f = 0;
            }
            else if(f < 0)
            {
                f = count;
            }
            Cvar_SetValue("teamplay", f);
            break;

        case 4:
            f = skill.value + dir;
            if(f > 3)
            {
                f = 0;
            }
            else if(f < 0)
            {
                f = 3;
            }
            Cvar_SetValue("skill", f);
            break;

        case 5:
            f = fraglimit.value + dir * 10;
            if(f > 100)
            {
                f = 0;
            }
            else if(f < 0)
            {
                f = 100;
            }
            Cvar_SetValue("fraglimit", f);
            break;

        case 6:
            f = timelimit.value + dir * 5;
            if(f > 60)
            {
                f = 0;
            }
            else if(f < 0)
            {
                f = 60;
            }
            Cvar_SetValue("timelimit", f);
            break;

        case 7:
            startepisode += dir;
            count = sizeof(episodes) / sizeof(episode_t);

            if(startepisode < 0)
            {
                startepisode = count - 1;
            }

            if(startepisode >= count)
            {
                startepisode = 0;
            }

            startlevel = 0;
            break;

        case 8:
            startlevel += dir;
            count = episodes[startepisode].lastLevel -
                    episodes[startepisode].firstLevel + 1;

            if(startlevel < 0)
            {
                startlevel = count - 1;
            }

            if(startlevel >= count)
            {
                startlevel = 0;
            }
            break;
    }
}

void M_GameOptions_Key(int key)
{
    switch(key)
    {
        case K_ESCAPE:
        case K_BBUTTON: M_Menu_LanConfig_f(); break;

        case K_UPARROW:
            S_LocalSound("misc/menu1.wav");
            gameoptions_cursor--;
            if(gameoptions_cursor < 0)
            {
                gameoptions_cursor = NUM_GAMEOPTIONS - 1;
            }
            break;

        case K_DOWNARROW:
            S_LocalSound("misc/menu1.wav");
            gameoptions_cursor++;
            if(gameoptions_cursor >= NUM_GAMEOPTIONS)
            {
                gameoptions_cursor = 0;
            }
            break;

        case K_LEFTARROW:
            if(gameoptions_cursor == 0)
            {
                break;
            }
            S_LocalSound("misc/menu3.wav");
            M_NetStart_Change(-1);
            break;

        case K_RIGHTARROW:
            if(gameoptions_cursor == 0)
            {
                break;
            }
            S_LocalSound("misc/menu3.wav");
            M_NetStart_Change(1);
            break;

        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON:
            S_LocalSound("misc/menu2.wav");
            if(gameoptions_cursor == 0)
            {
                if(sv.active)
                {
                    Cbuf_AddText("disconnect\n");
                }
                Cbuf_AddText(
                    "listen 0\n"); // so host_netport will be re-examined
                Cbuf_AddText(va("maxplayers %u\n", maxplayers));
                SCR_BeginLoadingPlaque();

                Cbuf_AddText(va("map %s\n",
                    levels[episodes[startepisode].firstLevel + startlevel]
                        .name));

                return;
            }

            M_NetStart_Change(1);
            break;
    }
}

//=============================================================================
/* SEARCH MENU */

bool searchComplete = false;
double searchCompleteTime;
enum slistScope_e searchLastScope = SLIST_LAN; // QSS

void M_Menu_Search_f(enum slistScope_e scope)
{
    key_dest = key_menu;
    m_state = m_search;
    m_entersound = false;
    slistSilent = true;
    slistScope = searchLastScope = scope; // QSS
    searchComplete = false;

    NET_Slist_f();
    IN_UpdateGrabs(); // QSS
}


void M_Search_Draw()
{
    qpic_t* p;
    int x;

    p = Draw_CachePic("gfx/p_multi.lmp");
    M_DrawPic((320 - p->width) / 2, 4, p);
    x = (320 / 2) - ((12 * 8) / 2) + 4;
    M_DrawTextBox(x - 8, 32, 12, 1);
    M_Print(x, 40, "Searching...");

    if(slistInProgress)
    {
        NET_Poll();
        return;
    }

    if(!searchComplete)
    {
        searchComplete = true;
        searchCompleteTime = realtime;
    }

    if(hostCacheCount)
    {
        M_Menu_ServerList_f();
        return;
    }

    M_PrintWhite((320 / 2) - ((22 * 8) / 2), 64, "No Quake servers found");
    if((realtime - searchCompleteTime) < 3.0)
    {
        return;
    }

    M_Menu_LanConfig_f();
}


void M_Search_Key(int key)
{
    (void)key;
}

//=============================================================================
/* SLIST MENU */

int slist_cursor;
bool slist_sorted;

void M_Menu_ServerList_f()
{
    key_dest = key_menu;
    m_state = m_slist;
    m_entersound = true;
    slist_cursor = 0;
    m_return_onerror = false;
    m_return_reason[0] = 0;
    slist_sorted = false;

    IN_UpdateGrabs(); // QSS
}


void M_ServerList_Draw()
{
    int n;
    qpic_t* p;

    if(!slist_sorted)
    {
        slist_sorted = true;
        NET_SlistSort();
    }

    p = Draw_CachePic("gfx/p_multi.lmp");
    M_DrawPic((320 - p->width) / 2, 4, p);
    for(n = 0; n < hostCacheCount; n++)
    {
        M_Print(16, 32 + 8 * n, NET_SlistPrintServer(n));
    }
    M_DrawCharacter(0, 32 + slist_cursor * 8, 12 + ((int)(realtime * 4) & 1));

    if(*m_return_reason)
    {
        M_PrintWhite(16, 148, m_return_reason);
    }
}


void M_ServerList_Key(int k)
{
    switch(k)
    {
        case K_ESCAPE:
        case K_BBUTTON: M_Menu_LanConfig_f(); break;

        case K_SPACE: M_Menu_Search_f(searchLastScope); break;

        case K_UPARROW:
        case K_LEFTARROW:
            S_LocalSound("misc/menu1.wav");
            slist_cursor--;
            if(slist_cursor < 0)
            {
                slist_cursor = hostCacheCount - 1;
            }
            break;

        case K_DOWNARROW:
        case K_RIGHTARROW:
            S_LocalSound("misc/menu1.wav");
            slist_cursor++;
            if(slist_cursor >= hostCacheCount)
            {
                slist_cursor = 0;
            }
            break;

        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON:
            S_LocalSound("misc/menu2.wav");
            m_return_state = m_state;
            m_return_onerror = true;
            slist_sorted = false;
            key_dest = key_game;
            m_state = m_none;
            Cbuf_AddText(
                va("connect \"%s\"\n", NET_SlistPrintServerName(slist_cursor)));

            IN_UpdateGrabs(); // QSS
            break;

        default: break;
    }
}

//=============================================================================
/* Menu Subsystem */


void M_Init()
{
    Cmd_AddCommand("togglemenu", M_ToggleMenu_f);

    Cmd_AddCommand("menu_main", M_Menu_Main_f);
    Cmd_AddCommand("menu_singleplayer", M_Menu_SinglePlayer_f);
    Cmd_AddCommand("menu_load", M_Menu_Load_f);
    Cmd_AddCommand("menu_save", M_Menu_Save_f);
    Cmd_AddCommand("menu_multiplayer", M_Menu_MultiPlayer_f);
    Cmd_AddCommand("menu_setup", M_Menu_Setup_f);
    Cmd_AddCommand("menu_options", M_Menu_Options_f);
    Cmd_AddCommand("menu_keys", M_Menu_Keys_f);
    Cmd_AddCommand("menu_video", M_Menu_Video_f);

    // TODO VR: (P2) cmds for new menu here?

    Cmd_AddCommand("help", M_Menu_Help_f);
    Cmd_AddCommand("menu_quit", M_Menu_Quit_f);

    Cmd_AddCommand("autosave", quake::saveutil::doAutosave);
}

[[nodiscard]] quake::vr::menu_keyboard& mkb()
{
    static quake::vr::menu_keyboard mkb{{200, 400}};
    return mkb;
}

// TODO VR: (P1) cleanup
void M_DrawKeyboard()
{
    using quake::vr::menu_keyboard;

    if(key_dest != key_menu && key_dest != key_console)
    {
        return;
    }

    mkb().draw();

    {
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_POINT_SMOOTH);
        glPointSize(8);

        const float mx = vr_menu_mouse_x;
        const float my = vr_menu_mouse_y;
        const bool click = vr_menu_mouse_click;

        const auto doKey = [&](const int key)
        {
            if(key_dest == key_menu)
            {
                M_Keydown(key, true /* fromVirtualKeyboard */);
            }
            else if(key_dest == key_console)
            {
                if(key == K_ESCAPE)
                {
                    M_ToggleMenu_f();
                }
                else
                {
                    void Key_Console(int key, const bool fromVirtualKeyboard);
                    Key_Console(key, true /* fromVirtualKeyboard */);
                }
            }
        };

        const auto actions = quake::util::make_overload_set([&](const char c)
            { Char_Event(mkb().caps_lock() ? c - 32 : c); },
            [&](menu_keyboard::ka_backspace) { doKey(K_BACKSPACE); },      //
            [&](menu_keyboard::ka_enter) { doKey(K_ENTER); },              //
            [&](menu_keyboard::ka_back) { doKey(K_ESCAPE); },              //
            [&](menu_keyboard::ka_up) { doKey(K_UPARROW); },               //
            [&](menu_keyboard::ka_down) { doKey(K_DOWNARROW); },           //
            [&](menu_keyboard::ka_left) { doKey(K_LEFTARROW); },           //
            [&](menu_keyboard::ka_right) { doKey(K_RIGHTARROW); },         //
            [&](menu_keyboard::ka_space) { Char_Event(' '); },             //
            [&](menu_keyboard::ka_tab) { doKey(K_TAB); },                  //
            [&](menu_keyboard::ka_capslock) { mkb().toggle_caps_lock(); }, //,
            [&](menu_keyboard::ka_console)
            {
                m_state = m_none;
                Con_ToggleConsole_f();
            });

        mkb().update_click(mx, my, click);
        mkb().update_letters(mx, my, click, actions);
        mkb().update_drag(mx, my, click);

        {
            const quake::vr::gl_util::gl_beginend_guard guard{GL_POINTS};

            glColor3f(1, 0, 0);
            quake::vr::gl_util::gl_vertex(qvec2{mx, my});
        }

        glDisable(GL_POINT_SMOOTH);
        glEnable(GL_TEXTURE_2D);
    }
}

void M_Draw()
{
    if(m_state == m_none || key_dest != key_menu)
    {
        return;
    }

    if(!m_recursiveDraw)
    {
        if(scr_con_current)
        {
            Draw_ConsoleBackground();
            S_ExtraUpdate();
        }

        Draw_FadeScreen(); // johnfitz -- fade even if console fills screen
    }
    else
    {
        m_recursiveDraw = false;
    }

    GL_SetCanvas(CANVAS_MENU); // johnfitz

    // -----------------------------------------------------------------------
    // VR: Process nested "Quake VR Settings" menus.
    forQVRSMenus(
        [&](quake::menu& xm, m_state_e s)
        {
            if(m_state == s)
            {
                xm.draw();
            }
        });

    // -----------------------------------------------------------------------
    // VR: Process nested "Quake VR Dev Tools" menus.
    forQVRDTMenus(
        [&](quake::menu& xm, m_state_e s)
        {
            if(m_state == s)
            {
                xm.draw();
            }
        });

    // -----------------------------------------------------------------------
    // VR: Process nested "Quake VR Change mMap" menus.
    forQVRCMMenus(
        [&](quake::menu& xm, m_state_e s)
        {
            if(m_state == s)
            {
                xm.draw();
            }
        });

    switch(m_state)
    {
        case m_none: break;
        case m_main: M_Main_Draw(); break;
        case m_singleplayer: M_SinglePlayer_Draw(); break;
        case m_load: M_Load_Draw(); break;
        case m_save: M_Save_Draw(); break;
        case m_multiplayer: M_MultiPlayer_Draw(); break;
        case m_setup: M_Setup_Draw(); break;
        case m_net: M_Net_Draw(); break;
        case m_options: M_Options_Draw(); break;
        case m_keys: M_Keys_Draw(); break;
        case m_video: M_Video_Draw(); break;
        // -------------------------------------------------------------------
        // VR: New menus.
        case m_botcontrol: M_BotControl_Draw(); break;
        case m_quakevrquicksettings: M_QuakeVRQuickSettings_Draw(); break;
        case m_quakevrsettings: M_QuakeVRSettings_Draw(); break;
        case m_quakevrdevtools: M_QuakeVRDevTools_Draw(); break;
        case m_quakevrchangemap: M_QuakeVRChangeMap_Draw(); break;
        // -------------------------------------------------------------------
        case m_help: M_Help_Draw(); break;
        case m_lanconfig: M_LanConfig_Draw(); break;
        case m_gameoptions: M_GameOptions_Draw(); break;
        case m_search: M_Search_Draw(); break;
        case m_slist: M_ServerList_Draw(); break;

        case m_quit:
            // if(!fitzmode)
            if(false)
            {
                /* QuakeSpasm customization: */
                /* Quit now! S.A. */
                key_dest = key_console;
                Host_Quit_f();
            }
            M_Quit_Draw();
            break;

        default:
        {
            // Nested menus are handled above.
            break;
        }
    }

    if(m_entersound)
    {
        S_LocalSound("misc/menu2.wav");
        m_entersound = false;
    }

    S_ExtraUpdate();
}

void M_Keydown(int key, const bool fromVirtualKeyboard)
{
    if(!fromVirtualKeyboard && !vr_fakevr.value &&
        mkb().hovered(vr_menu_mouse_x, vr_menu_mouse_y) && key == K_ENTER)
    {
        return;
    }

    // -----------------------------------------------------------------------
    // VR: Process nested "Quake VR Settings" menus.
    {
        bool processedAny = false;

        forQVRSMenus(
            [&](quake::menu& xm, m_state_e s)
            {
                if(m_state == s)
                {
                    xm.key(key);
                    processedAny = true;
                }
            });

        if(processedAny)
        {
            return;
        }
    }

    // -----------------------------------------------------------------------
    // VR: Process nested "Quake VR Dev Tools" menus.
    {
        bool processedAny = false;

        forQVRDTMenus(
            [&](quake::menu& xm, m_state_e s)
            {
                if(m_state == s)
                {
                    xm.key(key);
                    processedAny = true;
                }
            });

        if(processedAny)
        {
            return;
        }
    }

    // -----------------------------------------------------------------------
    // VR: Process nested "Quake VR Change Map" menus.
    {
        bool processedAny = false;

        forQVRCMMenus(
            [&](quake::menu& xm, m_state_e s)
            {
                if(m_state == s)
                {
                    xm.key(key);
                    processedAny = true;
                }
            });

        if(processedAny)
        {
            return;
        }
    }

    switch(m_state)
    {
        case m_none: return;
        case m_main: M_Main_Key(key); return;
        case m_singleplayer: M_SinglePlayer_Key(key); return;
        case m_load: M_Load_Key(key); return;
        case m_save: M_Save_Key(key); return;
        case m_multiplayer: M_MultiPlayer_Key(key); return;
        case m_setup: M_Setup_Key(key); return;
        case m_net: M_Net_Key(key); return;
        case m_options: M_Options_Key(key); return;
        case m_keys: M_Keys_Key(key); return;
        case m_video: M_Video_Key(key); return;
        // -------------------------------------------------------------------
        // VR: New menus.
        case m_botcontrol: M_BotControl_Key(key); break;
        case m_quakevrquicksettings: M_QuakeVRQuickSettings_Key(key); return;
        case m_quakevrsettings: M_QuakeVRSettings_Key(key); return;
        case m_quakevrdevtools: M_QuakeVRDevTools_Key(key); return;
        case m_quakevrchangemap: M_QuakeVRChangeMap_Key(key); return;
        // -------------------------------------------------------------------
        case m_help: M_Help_Key(key); return;
        case m_quit: M_Quit_Key(key); return;
        case m_lanconfig: M_LanConfig_Key(key); return;
        case m_gameoptions: M_GameOptions_Key(key); return;
        case m_search: M_Search_Key(key); return;
        case m_slist: M_ServerList_Key(key); return;

        default:
        {
            // Nested menus are handled above.
            break;
        }
    }
}


void M_Charinput(int key)
{
    switch(m_state)
    {
        case m_setup: M_Setup_Char(key); return;
        case m_quit: M_Quit_Char(key); return;
        case m_lanconfig: M_LanConfig_Char(key); return;
        default: return;
    }
}


bool M_TextEntry()
{
    switch(m_state)
    {
        case m_setup: return M_Setup_TextEntry();
        case m_quit: return M_Quit_TextEntry();
        case m_lanconfig: return M_LanConfig_TextEntry();
        default: return false;
    }
}


void M_ConfigureNetSubsystem()
{
    // enable/disable net systems to match desired config
    Cbuf_AddText("stopdemo\n");
    net_hostport = lanConfig_port;
}
