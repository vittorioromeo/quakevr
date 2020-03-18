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
    e_ShoulderOffsetX,
    e_ShoulderOffsetY,
    e_ShoulderOffsetZ,
    e_2HVirtualStockFactor,
    e_WpnPosWeight,
    e_WpnPosWeightOffset,
    e_WpnPosWeightMult,
    e_WpnPosWeight2HHelpOffset,
    e_WpnPosWeight2HHelpMult,
    e_WpnDirWeight,
    e_WpnDirWeightOffset,
    e_WpnDirWeightMult,
    e_WpnDirWeight2HHelpOffset,
    e_WpnDirWeight2HHelpMult,

    k_Max
};

void M_Menu_VRGameplay_f(void);
void M_VRGameplay_Draw(void);
void M_VRGameplay_Key(int key);
