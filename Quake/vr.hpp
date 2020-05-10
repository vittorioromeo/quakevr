#pragma once

#include "quakedef.hpp"
#include "openvr.hpp"

#include <vector>
#include <string>

//
//
//
// ----------------------------------------------------------------------------
// VR Typedef
// ----------------------------------------------------------------------------

using HandIdx = int; // TODO VR: (P2) should be strong typedef?
using WpnCvarEntry = int;
using VertexIdx = int;

//
//
//
// ----------------------------------------------------------------------------
// VR Constants
// ----------------------------------------------------------------------------

inline constexpr HandIdx cVR_OffHand = 0;
inline constexpr HandIdx cVR_MainHand = 1;
inline constexpr HandIdx cVR_FakeHand = 2;

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

void VR_Draw2D();
void VR_Move(usercmd_t* cmd);
void VR_InitGame();
void VR_PushYaw();
void VR_DrawSbar();
[[nodiscard]] qvec3 VR_AddOrientationToViewAngles(const qvec3& angles) noexcept;
void VR_SetAngles(const qvec3& angles) noexcept;
void VR_ResetOrientation();
void VR_SetMatrices();
void VR_CalibrateHeight();
void VR_ModVRTorsoModel();
void VR_ModVRLegHolsterModel();
void VR_ModAllModels();

void VR_OnSpawnServer();
void VR_OnClientClearState();



struct VrGunWallCollision
{
    bool _colliding{false};
    bool _normals[3]{};
    edict_t* _ent{nullptr};
};

[[nodiscard]] qvec3 VR_GetAdjustedPlayerOrigin(qvec3 playerOrigin) noexcept;

[[nodiscard]] qvec3 VR_GetWorldHandPos(
    const int handIndex, const qvec3& playerOrigin) noexcept;

[[nodiscard]] qvec3 VR_GetResolvedHandPos(edict_t* edict,
    const qvec3& worldHandPos, const qvec3& adjPlayerOrigin) noexcept;

qvec3 VR_UpdateGunWallCollisions(edict_t* edict, const int handIndex,
    VrGunWallCollision& out, qvec3 resolvedHandPos) noexcept;

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

[[nodiscard]] const qvec3& VR_GetHeadOrigin() noexcept;

void VR_DoHaptic(const int hand, const float delay, const float duration,
    const float frequency, const float amplitude);

[[nodiscard]] float VR_GetMenuMult() noexcept;

[[nodiscard]] qvec3 VR_GetLastHeadOrigin() noexcept;
[[nodiscard]] qfloat VR_GetCrouchRatio() noexcept;

[[nodiscard]] qvec3 VR_GetLeftHipPos() noexcept;
[[nodiscard]] qvec3 VR_GetRightHipPos() noexcept;
[[nodiscard]] qvec3 VR_GetLeftUpperPos() noexcept;
[[nodiscard]] qvec3 VR_GetRightUpperPos() noexcept;

[[nodiscard]] qfloat VR_GetTurnYawAngle() noexcept;
[[nodiscard]] qvec3 VR_GetHeadAngles() noexcept;
[[nodiscard]] qfloat VR_GetHeadYawAngle() noexcept;
[[nodiscard]] qfloat VR_GetBodyYawAngle() noexcept;

[[nodiscard]] qvec3 VR_GetAliasVertexOffsets(
    entity_t* const anchor, const int anchorVertex) noexcept;

[[nodiscard]] qvec3 VR_GetScaledAliasVertexOffsets(entity_t* const anchor,
    const int anchorVertex, const qvec3& extraOffsets,
    const bool horizFlip) noexcept;

[[nodiscard]] qvec3 VR_GetScaledAndAngledAliasVertexOffsets(
    entity_t* const anchor, const int anchorVertex, const qvec3& extraOffsets,
    const qvec3& rotation, const bool horizFlip) noexcept;

[[nodiscard]] qvec3 VR_GetScaledAndAngledAliasVertexPosition(
    entity_t* const anchor, const int anchorVertex, const qvec3& extraOffsets,
    const qvec3& rotation, const bool horizFlip) noexcept;

[[nodiscard]] qvec3 VR_GetWpnFixed2HFinalPosition(entity_t* const anchor,
    const int cvarEntry, const bool horizflip, const qvec3& extraOffset,
    const qvec3& handRot) noexcept;

// TODO VR: (P2) remove?
[[nodiscard]] int VR_GetWpnCVarFromModel(qmodel_t* model);
void ApplyMod_Weapon(const int cvarEntry, aliashdr_t* const hdr);

void VR_SetHandtouchParams(int hand, edict_t* player, edict_t* target);
void VR_SetFakeHandtouchParams(edict_t* player, edict_t* target);

void VR_ModAllWeapons();

[[nodiscard]] bool VR_EnabledAndNotFake() noexcept;

void VR_ApplyModelMod(
    const qvec3& scale, const qvec3& offsets, aliashdr_t* const hdr) noexcept;

[[nodiscard]] float VR_GetEasyHandTouchBonus() noexcept;
[[nodiscard]] int VR_OtherHand(const int hand) noexcept;

[[nodiscard]] bool VR_IsActive2HHelpingHand(const int hand) noexcept;

[[nodiscard]] bool VR_IsHandGrabbing(const int hand) noexcept;

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

enum class WpnButtonMode : int
{
    // The weapon does not have a button.
    None = 0,

    // The weapon has a button to change ammunition type.
    ChangeAmmo = 1,
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
    // OffHandOffsetX = 24, // TODO VR: (P2) deprecated, free index
    // OffHandOffsetY = 25, // TODO VR: (P2) deprecated, free index
    // OffHandOffsetZ = 26, // TODO VR: (P2) deprecated, free index
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
    WpnButtonMode = 49,
    WpnButtonX = 50,
    WpnButtonY = 51,
    WpnButtonZ = 52,
    WpnButtonAnchorVertex = 53,
    // WpnButtonOffHandX = 54, // TODO VR: (P2) deprecated, free index
    // WpnButtonOffHandY = 55, // TODO VR: (P2) deprecated, free index
    // WpnButtonOffHandZ = 56, // TODO VR: (P2) deprecated, free index
    WpnButtonPitch = 57,
    WpnButtonYaw = 58,
    WpnButtonRoll = 59,
    MuzzleAnchorVertex = 60,

    ZeroBlend = 61,
    TwoHZeroBlend = 62,

    k_Max
};

// ----------------------------------------------------------------------------

[[nodiscard]] qvec3 VR_GetWpnOffsets(const int cvarEntry) noexcept;
[[nodiscard]] qvec3 VR_GetWpn2HOffsets(const int cvarEntry) noexcept;
[[nodiscard]] qvec3 VR_GetWpnFixed2HOffsets(const int cvarEntry) noexcept;
[[nodiscard]] qvec3 VR_GetWpnGunOffsets(const int cvarEntry) noexcept;
[[nodiscard]] qvec3 VR_GetWpnFixed2HHandAngles(const int cvarEntry) noexcept;
[[nodiscard]] qvec3 VR_GetWpnFixed2HMainHandOffsets(
    const int cvarEntry) noexcept;

// ----------------------------------------------------------------------------

[[nodiscard]] qvec3 VR_GetWpnAngleOffsets(const int cvarEntry) noexcept;
[[nodiscard]] qvec3 VR_GetWpn2HAngleOffsets(const int cvarEntry) noexcept;

// ----------------------------------------------------------------------------

[[nodiscard]] qvec3 VR_GetWpnMuzzleOffsets(const int cvarEntry) noexcept;

[[nodiscard]] qvec3 VR_CalcWeaponMuzzlePosImpl(
    const int index, const int cvarEntry) noexcept;
[[nodiscard]] qvec3 VR_CalcMainHandWpnMuzzlePos() noexcept;

// ----------------------------------------------------------------------------

[[nodiscard]] qvec3 VR_GetWpnButtonOffsets(const int cvarEntry) noexcept;
[[nodiscard]] qvec3 VR_GetWpnButtonAngles(const int cvarEntry) noexcept;


// ----------------------------------------------------------------------------

[[nodiscard]] qvec3 VR_GetWpnHandOffsets(const int cvarEntry) noexcept;

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
// Extern Declarations
// ----------------------------------------------------------------------------

// TODO VR: (P2) what to do with this?
extern int vr_hardcoded_wpn_cvar_fist;

// TODO VR: (P2) encapsulate nicely
extern float vr_2h_aim_transition[2];
extern bool vr_teleporting;
extern qvec3 vr_teleporting_impact;
extern bool vr_teleporting_impact_valid;
extern vr::VRSkeletalSummaryData_t vr_ss_lefthand;
extern vr::VRSkeletalSummaryData_t vr_ss_righthand;

// TODO VR: (P1) used by show fns
[[nodiscard]] qvec3 VR_Get2HHoldingHandPos(
    const int holdingHand, const int helpingHand) noexcept;

[[nodiscard]] qvec3 VR_Get2HHelpingHandPos(
    const int holdingHand, const int helpingHand) noexcept;

[[nodiscard]] qvec3 VR_GetShoulderStockPos(
    const int holdingHand, const int helpingHand) noexcept;

[[nodiscard]] qvec3 VR_Get2HVirtualStockMix(
    const qvec3& viaHand, const qvec3& viaShoulder) noexcept;

[[nodiscard]] bool VR_InStockDistance(const int holdingHand,
    const int helpingHand, const qvec3& shoulderPos) noexcept;

[[nodiscard]] std::tuple<qvec3, qvec3, qvec3, qvec3, qvec3, qvec3, qvec3, qvec3>
VR_GetBodyYawAngleCalculations() noexcept;

[[nodiscard]] bool VR_InHipHolsterDistance(
    const qvec3& hand, const qvec3& holster);

[[nodiscard]] bool VR_InShoulderHolsterDistance(
    const qvec3& hand, const qvec3& holster);

[[nodiscard]] bool VR_InUpperHolsterDistance(
    const qvec3& hand, const qvec3& holster);

[[nodiscard]] qvec3 VR_GetLeftShoulderHolsterPos() noexcept;

[[nodiscard]] qvec3 VR_GetRightShoulderHolsterPos() noexcept;

[[nodiscard]] WpnCrosshairMode VR_GetWpnCrosshairMode(
    const int cvarEntry) noexcept;

[[nodiscard]] qvec3 VR_CalcFinalWpnMuzzlePos(const int index) noexcept;

[[nodiscard]] bool svPlayerActive() noexcept;
[[nodiscard]] edict_t* getPlayerEdict() noexcept;

//
//
// PAK Stuff

[[nodiscard]] const std::string& VR_GetActiveStartPakName();
[[nodiscard]] std::vector<std::string>& VR_GetLoadedPakNames();
[[nodiscard]] std::vector<std::string>& VR_GetLoadedPakNamesWithStartMaps();

[[nodiscard]] std::string VR_ExtractPakName(std::string_view sv);
[[nodiscard]] std::string VR_ExtractPakName(const pack_t& pak);

void VR_OnLoadedPak(pack_t& pak);
