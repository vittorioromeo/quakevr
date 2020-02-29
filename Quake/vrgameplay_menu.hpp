#pragma once

enum class VRGameplayMenuOpt
{
    e_Melee_Threshold,

    k_Max
};

void M_Menu_VRGameplay_f(void);
void M_VRGameplay_Draw(void);
void M_VRGameplay_Key(int key);
