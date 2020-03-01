// 2016 Dominic Szablewski - phoboslab.org

#include "quakedef.hpp"

#ifndef __R_VR_H
#define __R_VR_H

struct VrAimMode
{
    enum Enum
    {
        // Head Aiming; View YAW is mouse+head, PITCH is head
        e_HEAD_MYAW = 1,

        // Head Aiming; View YAW and PITCH is mouse+head
        e_HEAD_MYAW_MPITCH = 2,

        // Mouse Aiming; View YAW is mouse+head, PITCH is head
        e_MOUSE_MYAW = 3,

        // Mouse Aiming; View YAW and PITCH is mouse+head
        e_MOUSE_MYAW_MPITCH = 4,

        // Blended Aiming; Mouse aims, with YAW decoupled for limited area
        e_BLENDED = 5,

        // Blended Aiming; Mouse aims, with YAW decoupled for limited area,
        // pitch decoupled entirely
        e_BLENDED_NOPITCH = 6,

        // Controller Aiming
        e_CONTROLLER = 7
    };
};

struct VrCrosshair
{
    enum Enum
    {
        // No crosshair
        e_NONE = 0,

        // Point crosshair projected to depth of object it is in front of
        e_POINT = 1,

        // Line crosshair
        e_LINE = 2
    };
};

struct VrMovementMode
{
    enum Enum
    {
        e_FOLLOW_HAND = 0,
        e_RAW_INPUT = 1,
        k_Max
    };
};

enum class VrSbarMode : int
{
    MainHand = 0,
    OffHand = 1
};

void VID_VR_Init();
void VID_VR_Shutdown();
bool VR_Enable();
void VID_VR_Disable();

void VR_UpdateScreenContent();
void VR_ShowCrosshair();
void VR_Draw2D();
void VR_Move(usercmd_t* cmd);
void VR_InitGame();
void VR_PushYaw();
void VR_DrawSbar();
void VR_AddOrientationToViewAngles(vec3_t angles);
void VR_SetAngles(vec3_t angles);
void VR_ResetOrientation();
void VR_SetMatrices();
void VR_SetTrackingSpace(int n);

extern cvar_t vr_aimmode;
extern cvar_t vr_crosshair_alpha;
extern cvar_t vr_crosshair_depth;
extern cvar_t vr_crosshair_size;
extern cvar_t vr_crosshair;
extern cvar_t vr_crosshairy;
extern cvar_t vr_deadzone;
extern cvar_t vr_enabled;
extern cvar_t vr_floor_offset;
extern cvar_t vr_gun_z_offset;
extern cvar_t vr_gunangle;
extern cvar_t vr_gunmodelpitch;
extern cvar_t vr_gunmodelscale;
extern cvar_t vr_gunmodely;
extern cvar_t vr_gunyaw;
extern cvar_t vr_hud_scale;
extern cvar_t vr_melee_threshold;
extern cvar_t vr_menu_scale;
extern cvar_t vr_movement_mode;
extern cvar_t vr_msaa;
extern cvar_t vr_enable_joystick_turn;
extern cvar_t vr_snap_turn;
extern cvar_t vr_turn_speed;
extern cvar_t vr_viewkick;
extern cvar_t vr_world_scale;
extern cvar_t vr_sbar_mode;
extern cvar_t vr_sbar_offset_x;
extern cvar_t vr_sbar_offset_y;
extern cvar_t vr_sbar_offset_z;
extern cvar_t vr_sbar_offset_pitch;
extern cvar_t vr_sbar_offset_yaw;
extern cvar_t vr_sbar_offset_roll;

// TODO VR: not sure what this number should actually be...
#define MAX_WEAPONS 20
#define VARS_PER_WEAPON 8

extern cvar_t vr_weapon_offset[MAX_WEAPONS * VARS_PER_WEAPON];
extern int weaponCVarEntry;

#endif
