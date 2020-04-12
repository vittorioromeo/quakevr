#pragma once // 2016 Dominic Szablewski - phoboslab.org

#include "quakedef.hpp"

//
//
//
// ----------------------------------------------------------------------------
// VR Constants
// ----------------------------------------------------------------------------

inline constexpr int cVR_OffHand = 0;
inline constexpr int cVR_MainHand = 1;

//
//
//
// ----------------------------------------------------------------------------
// VR Configuration Enums
// ----------------------------------------------------------------------------

struct VrAimMode
{
    enum Enum
    {
        // Head Aiming; View YAW is mouse+head, PITCH is head
        e_HEAD_MYAW = 0,

        // Head Aiming; View YAW and PITCH is mouse+head
        e_HEAD_MYAW_MPITCH = 1,

        // Mouse Aiming; View YAW is mouse+head, PITCH is head
        e_MOUSE_MYAW = 2,

        // Mouse Aiming; View YAW and PITCH is mouse+head
        e_MOUSE_MYAW_MPITCH = 3,

        // Blended Aiming; Mouse aims, with YAW decoupled for limited area
        e_BLENDED = 4,

        // Blended Aiming; Mouse aims, with YAW decoupled for limited area,
        // pitch decoupled entirely
        e_BLENDED_NOPITCH = 5,

        // Controller Aiming
        e_CONTROLLER = 6
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
        e_LINE = 2,

        // Smooth line crosshair
        e_LINE_SMOOTH = 3
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

enum class Vr2HMode : int
{
    Disabled = 0,
    Basic = 1,
    VirtualStock = 2
};

enum class VrOptionHandSelection : int
{
    Off = 0,
    MainHand = 1,
    OffHand = 2,
    BothHands = 3
};

//
//
//
// ----------------------------------------------------------------------------
// VR Public API
// ----------------------------------------------------------------------------

void VID_VR_Init();
void VID_VR_Shutdown();
bool VR_Enable();
void VID_VR_Disable();

void VR_UpdateScreenContent();

void VR_ShowCrosshair();
void VR_DrawAllShowHelpers();

void VR_DrawTeleportLine();
void VR_Draw2D();
void VR_Move(usercmd_t* cmd);
void VR_InitGame();
void VR_PushYaw();
void VR_DrawSbar();
[[nodiscard]] glm::vec3 VR_AddOrientationToViewAngles(
    const glm::vec3& angles) noexcept;
void VR_SetAngles(const glm::vec3& angles) noexcept;
void VR_ResetOrientation();
void VR_SetMatrices();
void VR_CalibrateHeight();
void VR_ModVRTorsoModel();
void VR_ModAllModels();

[[nodiscard]] const glm::vec3& VR_GetHeadOrigin() noexcept;

void VR_DoHaptic(const int hand, const float delay, const float duration,
    const float frequency, const float amplitude);

[[nodiscard]] float VR_GetMenuMult() noexcept;

[[nodiscard]] glm::vec3 VR_GetLastHeadOrigin() noexcept;
[[nodiscard]] float VR_GetCrouchRatio() noexcept;

[[nodiscard]] glm::vec3 VR_GetLeftHipPos() noexcept;
[[nodiscard]] glm::vec3 VR_GetRightHipPos() noexcept;
[[nodiscard]] glm::vec3 VR_GetLeftUpperPos() noexcept;
[[nodiscard]] glm::vec3 VR_GetRightUpperPos() noexcept;

[[nodiscard]] float VR_GetTurnYawAngle() noexcept;
[[nodiscard]] glm::vec3 VR_GetHeadAngles() noexcept;
[[nodiscard]] float VR_GetHeadYawAngle() noexcept;
[[nodiscard]] float VR_GetBodyYawAngle() noexcept;

// TODO VR: (P2) remove?
[[nodiscard]] int VR_GetWpnCVarFromModel(qmodel_t* model);
void ApplyMod_Weapon(const int cvarEntry, aliashdr_t* const hdr);

void VR_SetHandtouchParams(int hand, edict_t* player, edict_t* target);
void VR_SetFakeHandtouchParams(edict_t* player, edict_t* target);

void VR_ModAllWeapons();

[[nodiscard]] bool VR_EnabledAndNotFake() noexcept;

void VR_ApplyModelMod(const glm::vec3& scale, const glm::vec3& offsets,
    aliashdr_t* const hdr) noexcept;

//
//
//
// ----------------------------------------------------------------------------
// Weapon CVars
// ----------------------------------------------------------------------------

enum class Wpn2HMode : std::uint8_t
{
    // Allows two-hand aiming, and supports virtual stock.
    Default = 0,

    // Allows two-hand aiming, ignores virtual stock.
    NoVirtualStock = 1,

    // Disallows two-hand aiming.
    Forbidden = 2,
};

enum class WpnCrosshairMode : std::uint8_t
{
    // Shows crosshair, if enabled.
    Default = 0,

    // Never shows crosshair. Useful for melee weapons.
    Forbidden = 1,
};

enum class WpnCVar : std::uint8_t
{
    OffsetX = 0,
    OffsetY = 1,
    OffsetZ = 2,
    Scale = 3,
    ID = 4,
    Pitch = 5,
    Roll = 6,
    Yaw = 7,
    MuzzleOffsetX = 8,
    MuzzleOffsetY = 9,
    MuzzleOffsetZ = 10,
    TwoHOffsetX = 11,
    TwoHOffsetY = 12,
    TwoHOffsetZ = 13,
    TwoHPitch = 14,
    TwoHYaw = 15,
    TwoHRoll = 16,
    TwoHMode = 17,
    Length = 18,
    Weight = 19,
    HandOffsetX = 20,
    HandOffsetY = 21,
    HandOffsetZ = 22,
    HandAnchorVertex = 23,
    OffHandOffsetX = 24,
    OffHandOffsetY = 25,
    OffHandOffsetZ = 26,
    CrosshairMode = 27,

    k_Max
};

// ----------------------------------------------------------------------------

struct WeaponOffsets
{
    float _x;
    float _y;
    float _z;
};

[[nodiscard]] WeaponOffsets VR_GetWpnOffsets(const int cvarEntry) noexcept;
[[nodiscard]] WeaponOffsets VR_GetWpn2HOffsets(const int cvarEntry) noexcept;

// ----------------------------------------------------------------------------

struct WeaponAngleOffsets
{
    float _pitch;
    float _yaw;
    float _roll;
};

[[nodiscard]] WeaponAngleOffsets VR_GetWpnAngleOffsets(
    const int cvarEntry) noexcept;

[[nodiscard]] WeaponAngleOffsets VR_GetWpn2HAngleOffsets(
    const int cvarEntry) noexcept;

// ----------------------------------------------------------------------------

struct WeaponMuzzleOffsets
{
    float _x;
    float _y;
    float _z;
};

[[nodiscard]] WeaponAngleOffsets VR_GetWpnMuzzleOffsets(
    const int cvarEntry) noexcept;

[[nodiscard]] glm::vec3 VR_CalcWeaponMuzzlePosImpl(
    const int index, const int cvarEntry) noexcept;
[[nodiscard]] glm::vec3 VR_CalcMainHandWpnMuzzlePos() noexcept;

// ----------------------------------------------------------------------------

[[nodiscard]] glm::vec3 VR_GetWpnHandOffsets(const int cvarEntry) noexcept;
[[nodiscard]] glm::vec3 VR_GetWpnOffHandOffsets(const int cvarEntry) noexcept;

[[nodiscard]] cvar_t& VR_GetWpnCVar(
    const int cvarEntry, WpnCVar setting) noexcept;

[[nodiscard]] float VR_GetWpnCVarValue(
    const int cvarEntry, WpnCVar setting) noexcept;

[[nodiscard]] int& VR_GetMainHandWpnCvarEntry() noexcept;

[[nodiscard]] int& VR_GetOffHandWpnCvarEntry() noexcept;

[[nodiscard]] Wpn2HMode VR_GetWpn2HMode(const int cvarEntry) noexcept;

//
//
//
// ----------------------------------------------------------------------------
// CVar Declarations
// ----------------------------------------------------------------------------

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
extern cvar_t vr_roomscale_jump;
extern cvar_t vr_height_calibration;
extern cvar_t vr_roomscale_jump_threshold;
extern cvar_t vr_menu_distance;
extern cvar_t vr_melee_dmg_multiplier;
extern cvar_t vr_melee_range_multiplier;
extern cvar_t vr_body_interactions;
extern cvar_t vr_roomscale_move_mult;
extern cvar_t vr_teleport_enabled;
extern cvar_t vr_teleport_range;
extern cvar_t vr_2h_mode;
extern cvar_t vr_2h_angle_threshold;
extern cvar_t vr_virtual_stock_thresh;
extern cvar_t vr_show_virtual_stock;
extern cvar_t vr_shoulder_offset_x;
extern cvar_t vr_shoulder_offset_y;
extern cvar_t vr_shoulder_offset_z;
extern cvar_t vr_2h_virtual_stock_factor;
extern cvar_t vr_wpn_pos_weight;
extern cvar_t vr_wpn_pos_weight_offset;
extern cvar_t vr_wpn_pos_weight_mult;
extern cvar_t vr_wpn_pos_weight_2h_help_offset;
extern cvar_t vr_wpn_pos_weight_2h_help_mult;
extern cvar_t vr_wpn_dir_weight;
extern cvar_t vr_wpn_dir_weight_offset;
extern cvar_t vr_wpn_dir_weight_mult;
extern cvar_t vr_wpn_dir_weight_2h_help_offset;
extern cvar_t vr_wpn_dir_weight_2h_help_mult;
extern cvar_t vr_offhandpitch;
extern cvar_t vr_offhandyaw;
extern cvar_t vr_show_hip_holsters;
extern cvar_t vr_hip_offset_x;
extern cvar_t vr_hip_offset_y;
extern cvar_t vr_hip_offset_z;
extern cvar_t vr_hip_holster_thresh;
extern cvar_t vr_show_shoulder_holsters;
extern cvar_t vr_shoulder_holster_offset_x;
extern cvar_t vr_shoulder_holster_offset_y;
extern cvar_t vr_shoulder_holster_offset_z;
extern cvar_t vr_shoulder_holster_thresh;
extern cvar_t vr_show_upper_holsters;
extern cvar_t vr_upper_holster_offset_x;
extern cvar_t vr_upper_holster_offset_y;
extern cvar_t vr_upper_holster_offset_z;
extern cvar_t vr_upper_holster_thresh;
extern cvar_t vr_fakevr;
extern cvar_t vr_vrtorso_debuglines_enabled;
extern cvar_t vr_vrtorso_enabled;
extern cvar_t vr_vrtorso_x_offset;
extern cvar_t vr_vrtorso_y_offset;
extern cvar_t vr_vrtorso_z_offset;
extern cvar_t vr_vrtorso_head_z_mult;
extern cvar_t vr_vrtorso_x_scale;
extern cvar_t vr_vrtorso_y_scale;
extern cvar_t vr_vrtorso_z_scale;
extern cvar_t vr_vrtorso_pitch;
extern cvar_t vr_vrtorso_yaw;
extern cvar_t vr_vrtorso_roll;
extern cvar_t vr_holster_haptics;

// TODO VR: (P2) what to do with this?
extern int vr_hardcoded_wpn_cvar_fist;

// TODO VR: (P2) encapsulate nicely
extern int vr_impl_draw_wpnoffset_helper_offset;
extern int vr_impl_draw_wpnoffset_helper_muzzle;
extern int vr_impl_draw_wpnoffset_helper_2h_offset;
