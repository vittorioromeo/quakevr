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

#pragma once

#include "mstate.hpp"

#include <ctime>

extern bool m_entersound;

//
// menus
//
void M_Init();
void M_Keydown(int key, const bool fromVirtualKeyboard = false);
void M_Charinput(int key);
bool M_TextEntry();
void M_ToggleMenu_f();

void M_Menu_Main_f();
void M_Menu_Options_f();
void M_Menu_QuakeVRQuickSettings_f();
void M_Menu_QuakeVRSettings_f();
void M_Menu_QuakeVRDevTools_f();
void M_Menu_QuakeVRChangeMap_f();
void M_Menu_Quit_f();

void M_Print(int cx, int cy, const char* str);
void M_PrintWithNewLine(int cx, int cy, const char* str);
void M_PrintWhite(int cx, int cy, const char* str);
void M_PrintWhiteWithNewLine(int cx, int cy, const char* str);
void M_PrintWhiteByWrapping(
    const int wrapCount, int cx, int cy, const char* str);

void M_Draw();
void M_DrawKeyboard();
void M_DrawCharacter(int cx, int line, int num);

struct qpic_t;

void M_DrawSlider(int x, int y, float range);
void M_DrawPic(int x, int y, qpic_t* pic);
void M_DrawTransPic(int x, int y, qpic_t* pic);
void M_DrawCheckbox(int x, int y, int on);
