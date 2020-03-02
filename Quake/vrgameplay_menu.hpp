#pragma once

enum class VRGameplayMenuOpt
{
    e_MeleeThreshold,
    e_RoomscaleJump,
    e_RoomscaleJumpThreshold,
    e_CalibrateHeight,

    k_Max
};

void M_Menu_VRGameplay_f(void);
void M_VRGameplay_Draw(void);
void M_VRGameplay_Key(int key);
