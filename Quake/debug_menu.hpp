#pragma once

enum class DebugMenuOpt
{
    e_Showbboxes,
    e_Impulse9,
    e_Impulse11,
    e_God,
    e_Noclip,
    e_Fly,
    e_HostTimescale,

    k_Max
};

void M_Menu_Debug_f(void);
void M_Debug_Draw(void);
void M_Debug_Key(int key);
