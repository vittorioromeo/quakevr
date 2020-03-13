#pragma once

enum class VRGameplayMenuOpt
{
    e_MeleeThreshold,
    e_RoomscaleJump,
    e_RoomscaleJumpThreshold,
    e_CalibrateHeight,
    e_MeleeDmgMultiplier,
    e_MeleeRangeMultiplier,
    e_BodyInteractions,
    e_ForwardSpeed,
    e_SpeedBtnMultiplier,
    e_RoomScaleMoveMult,
    e_TeleportEnabled,
    e_TeleportRange,
    e_2HMode,
    e_2HAngleThreshold,
    e_2HVirtualStockThreshold,

    k_Max
};

void M_Menu_VRGameplay_f(void);
void M_VRGameplay_Draw(void);
void M_VRGameplay_Key(int key);
