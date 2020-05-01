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
inline constexpr int cVR_FakeHand = 2;

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

enum class VrPlayerShadows : int
{
    Off = 0,
    ViewEntities = 1,
    ThirdPerson = 2,
    Both = 3
};

enum class VrHolsterMode : int
{
    // No weapon cycling. Weapons must be carried in holsters and hands.
    Immersive = 0,

    // Player always carries all obtained weapons. Holsters act as quick
    // selection slots.
    QuickSlotHolsters = 1,
};

enum class VrWeaponThrowMode : int
{
    // Thrown weapons can be picked back up.
    Immersive = 0,

    // Thrown weapons disappear on hit.
    DisappearOnHit = 1,

    // Weapons cannot be thrown, they will be discarded.
    Discard = 2
};

enum class VrWeaponCycleMode : int
{
    // Weapon cycling disabled (immersive option).
    Disabled = 0,

    // Can always cycle between all owned weapons.
    Allowed = 1
};

enum class VrHolsterHaptics : int
{
    // Never activate haptics when hovering holster slots.
    Off = 0,

    // Continuous activation.
    Continuous = 1,

    // Single buzz.
    Once = 2
};

enum class VrMeleeBloodlust : int
{
    // VR melee attacks restore some of the player's health.
    Enabled = 0,

    // VR melee attacks do not restore health.
    Disabled = 1
};

enum class VrEnemyDrops : int
{
    // Enemies drop weapons the player is eligible for.
    WhenEligible = 0,

    // Enemies drop weapons without checking for eligibility.
    Always = 1,

    // Enemies never drop weapons.
    Disabled = 2
};

enum class VrAmmoBoxDrops : int
{
    // Ammo boxes spawn weapons the player is eligible for.
    WhenEligible = 0,

    // Ammo boxes spawn weapons without checking for eligibility.
    Always = 1,

    // Ammo boxes never spawn weapons.
    Disabled = 2
};

enum class VrMenuMode : int
{
    LastHeadAngles = 0,
    FollowHead = 1,
    FollowOffHand = 2,
    FollowMainHand = 3
};

enum class VrForceGrabMode : int
{
    Disabled = 0,
    Parabola = 1,
    Linear = 2,
    Instant = 3,
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
void VR_ModVRLegHolsterModel();
void VR_ModAllModels();

struct VrGunWallCollision
{
    bool _colliding{false};
    bool _normals[3]{};
    edict_t* _ent{nullptr};
};

[[nodiscard]] glm::vec3 VR_GetAdjustedPlayerOrigin(
    glm::vec3 playerOrigin) noexcept;

[[nodiscard]] glm::vec3 VR_GetWorldHandPos(
    const int handIndex, const glm::vec3& playerOrigin) noexcept;

[[nodiscard]] glm::vec3 VR_GetResolvedHandPos(
    const glm::vec3& worldHandPos, const glm::vec3& adjPlayerOrigin) noexcept;

glm::vec3 VR_UpdateGunWallCollisions(const int handIndex,
    VrGunWallCollision& out, glm::vec3 resolvedHandPos) noexcept;

// TODO VR: (P2) move

struct fbo_t
{
    GLuint framebuffer, depth_texture, texture;
    GLuint msaa_framebuffer, msaa_texture, msaa_depth_texture;
    int msaa;
    struct
    {
        float width, height;
    } size;
};

[[nodiscard]] fbo_t& VR_GetEyeFBO(const int index) noexcept;

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

[[nodiscard]] glm::vec3 VR_GetAliasVertexOffsets(
    entity_t* const anchor, const int anchorVertex) noexcept;

[[nodiscard]] glm::vec3 VR_GetScaledAliasVertexOffsets(entity_t* const anchor,
    const int anchorVertex, const glm::vec3& extraOffsets) noexcept;

[[nodiscard]] glm::vec3 VR_GetScaledAndAngledAliasVertexOffsets(
    entity_t* const anchor, const int anchorVertex,
    const glm::vec3& extraOffsets, const glm::vec3& rotation) noexcept;

[[nodiscard]] glm::vec3 VR_GetScaledAndAngledAliasVertexPosition(
    entity_t* const anchor, const int anchorVertex,
    const glm::vec3& extraOffsets, const glm::vec3& rotation) noexcept;

[[nodiscard]] glm::vec3 VR_GetWpnFixed2HFinalPosition(entity_t* const anchor,
    const int cvarEntry, const bool horizflip, const glm::vec3& extraOffset,
    const glm::vec3& handRot) noexcept;

// TODO VR: (P2) remove?
[[nodiscard]] int VR_GetWpnCVarFromModel(qmodel_t* model);
void ApplyMod_Weapon(const int cvarEntry, aliashdr_t* const hdr);

void VR_SetHandtouchParams(int hand, edict_t* player, edict_t* target);
void VR_SetFakeHandtouchParams(edict_t* player, edict_t* target);

void VR_ModAllWeapons();

[[nodiscard]] bool VR_EnabledAndNotFake() noexcept;

void VR_ApplyModelMod(const glm::vec3& scale, const glm::vec3& offsets,
    aliashdr_t* const hdr) noexcept;

[[nodiscard]] float VR_GetEasyHandTouchBonus() noexcept;
[[nodiscard]] int VR_OtherHand(const int hand) noexcept;

[[nodiscard]] bool VR_IsActive2HHelpingHand(const int hand) noexcept;

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

enum class Wpn2HDisplayMode : std::uint8_t
{
    // Off-hand model moves freely.
    Dynamic = 0,

    // Off-hand model is fixed to a particular vertex.
    Fixed = 1,
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
    HideHand = 28,
    TwoHDisplayMode = 29,
    TwoHHandAnchorVertex = 30,
    TwoHFixedOffsetX = 31,
    TwoHFixedOffsetY = 32,
    TwoHFixedOffsetZ = 33,
    GunOffsetX = 34,
    GunOffsetY = 35,
    GunOffsetZ = 36,
    TwoHFixedHandPitch = 37,
    TwoHFixedHandYaw = 38,
    TwoHFixedHandRoll = 39,
    TwoHFixedMainHandOffsetX = 40,
    TwoHFixedMainHandOffsetY = 41,
    TwoHFixedMainHandOffsetZ = 42,
    WeightPosMult = 43,
    WeightDirMult = 44,
    WeightHandVelMult = 45,
    WeightHandThrowVelMult = 46,
    Weight2HPosMult = 47,
    Weight2HDirMult = 48,

    k_Max
};

// ----------------------------------------------------------------------------

[[nodiscard]] glm::vec3 VR_GetWpnOffsets(const int cvarEntry) noexcept;
[[nodiscard]] glm::vec3 VR_GetWpn2HOffsets(const int cvarEntry) noexcept;
[[nodiscard]] glm::vec3 VR_GetWpnFixed2HOffsets(const int cvarEntry) noexcept;
[[nodiscard]] glm::vec3 VR_GetWpnGunOffsets(const int cvarEntry) noexcept;
[[nodiscard]] glm::vec3 VR_GetWpnFixed2HHandAngles(
    const int cvarEntry) noexcept;
[[nodiscard]] glm::vec3 VR_GetWpnFixed2HMainHandOffsets(
    const int cvarEntry) noexcept;

// ----------------------------------------------------------------------------

[[nodiscard]] glm::vec3 VR_GetWpnAngleOffsets(const int cvarEntry) noexcept;
[[nodiscard]] glm::vec3 VR_GetWpn2HAngleOffsets(const int cvarEntry) noexcept;

// ----------------------------------------------------------------------------

[[nodiscard]] glm::vec3 VR_GetWpnMuzzleOffsets(const int cvarEntry) noexcept;

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

[[nodiscard]] int& VR_GetWpnCvarEntry(const int hand) noexcept;
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
extern cvar_t vr_novrinit;
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
extern cvar_t vr_player_shadows;
extern cvar_t vr_positional_damage;
extern cvar_t vr_debug_print_handvel;
extern cvar_t vr_debug_show_hand_pos_and_rot;
extern cvar_t vr_leg_holster_model_enabled;
extern cvar_t vr_leg_holster_model_scale;
extern cvar_t vr_leg_holster_model_x_offset;
extern cvar_t vr_leg_holster_model_y_offset;
extern cvar_t vr_leg_holster_model_z_offset;
extern cvar_t vr_holster_mode;
extern cvar_t vr_weapon_throw_mode;
extern cvar_t vr_weapon_throw_damage_mult;
extern cvar_t vr_weapon_throw_velocity_mult;
extern cvar_t vr_weapon_cycle_mode;
extern cvar_t vr_melee_bloodlust;
extern cvar_t vr_melee_bloodlust_mult;
extern cvar_t vr_enemy_drops;
extern cvar_t vr_enemy_drops_chance_mult;
extern cvar_t vr_ammobox_drops;
extern cvar_t vr_ammobox_drops_chance_mult;
extern cvar_t vr_menumode;
extern cvar_t vr_forcegrab_powermult;
extern cvar_t vr_forcegrab_mode;
extern cvar_t vr_forcegrab_range;
extern cvar_t vr_forcegrab_radius;
extern cvar_t vr_weapondrop_particles;
extern cvar_t vr_forcegrab_eligible_particles;
extern cvar_t vr_forcegrab_eligible_haptics;
extern cvar_t vr_enable_grapple;

// TODO VR: (P2) what to do with this?
extern int vr_hardcoded_wpn_cvar_fist;

// TODO VR: (P2) encapsulate nicely
extern int vr_impl_draw_wpnoffset_helper_offset;
extern int vr_impl_draw_wpnoffset_helper_muzzle;
extern int vr_impl_draw_wpnoffset_helper_2h_offset;
extern int vr_impl_draw_hand_anchor_vertex;
extern int vr_impl_draw_2h_hand_anchor_vertex;
extern float vr_2h_aim_transition[2];
