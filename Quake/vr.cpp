#include "console.hpp"
#include "cvar.hpp"
#include "quakedef.hpp"
#include "server.hpp"
#include "vr.hpp"
#include "vr_cvars.hpp"
#include "util.hpp"
#include "render.hpp"
#include "openvr.hpp"
#include "quakeglm.hpp"
#include "quakeglm_qmat4.hpp"
#include "quakeglm_qvec2.hpp"
#include "quakeglm_qvec4.hpp"
#include "quakeglm_qquat.hpp"
#include "common.hpp"
#include "lerpdata.hpp"
#include "sbar.hpp"
#include "menu.hpp"
#include "keys.hpp"
#include "client.hpp"
#include "draw.hpp"
#include "gl_util.hpp"
#include "cmd.hpp"
#include "debugprint.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <deque>
#include <glm/geometric.hpp>
#include <iostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <SDL2/SDL.h>

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/projection.hpp>

//
//
//
// ----------------------------------------------------------------------------
// Utilities
// ----------------------------------------------------------------------------

using quake::util::cvarToEnum;
using quake::util::getAngledVectors;
using quake::util::getDirectionVectorFromPitchYawRoll;
using quake::util::getFwdVecFromPitchYawRoll;
using quake::util::hitSomething;
using quake::util::lerp;
using quake::util::mapRange;
using quake::util::pitchYawRollFromDirectionVector;
using quake::util::redirectVector;

//
//
//
// ----------------------------------------------------------------------------
// VR Rendering Structs
// ----------------------------------------------------------------------------

struct vr_eye_t
{
    int index;
    fbo_t fbo;
    vr::EVREye eye;
    vr::HmdVector3_t position;
    vr::HmdQuaternion_t orientation;
    float fov_x, fov_y;
};

//
//
//
// ----------------------------------------------------------------------------
// VR Input Structs
// ----------------------------------------------------------------------------

class VecHistory
{
private:
    std::size_t _bufCapacity;
    std::deque<qvec3> _data;

public:
    explicit VecHistory(const std::size_t bufCapacity) noexcept
        : _bufCapacity{bufCapacity}
    {
    }

    void add(const qvec3& v)
    {
        if(_data.size() == _bufCapacity)
        {
            _data.pop_front();
        }

        _data.push_back(v);
    }

    [[nodiscard]] qvec3 average() const noexcept
    {
        if(_data.empty())
        {
            return vec3_zero;
        }

        qvec3 res{};

        for(const auto& v : _data)
        {
            res += v;
        }

        return res / static_cast<qfloat>(_data.size());
    }
};

struct vr_controller
{
    vr::VRControllerState_t state;
    vr::VRControllerState_t lastState;
    qvec3 position;
    qvec3 orientation;
    qvec3 velocity;
    qvec3 a_velocity;
    vr::HmdVector3_t rawvector;
    vr::HmdQuaternion_t raworientation;
    bool active{false};

    VecHistory velocityHistory{15};
    VecHistory angularVelocityHistory{15};
};



//
//
//
// ----------------------------------------------------------------------------
// Extern Rendering and Screen Declarations
// ----------------------------------------------------------------------------

// main screen & 2D drawing
extern void SCR_SetUpToDrawConsole();
extern void SCR_UpdateScreenContent();
extern bool scr_drawdialog;
extern void SCR_DrawNotifyString();
extern bool scr_drawloading;
extern void SCR_DrawLoading();
extern void SCR_CheckDrawCenterString();
extern void SCR_DrawRam();
extern void SCR_DrawNet();
extern void SCR_DrawTurtle();
extern void SCR_DrawPause();
extern void SCR_DrawDevStats();
extern void SCR_DrawFPS();
extern void SCR_DrawClock();
extern void SCR_DrawConsole();

// rendering
extern void R_SetupView();
extern int glx, gly, glwidth, glheight;

//
//
//
// ----------------------------------------------------------------------------
// TODO VR: (P2) reorganize and document these globals
// ----------------------------------------------------------------------------

static float vrYaw;
static bool readbackYaw;

std::string vr_working_directory;

qvec3 vr_viewOffset;
qvec3 lastHudPosition{};
qvec3 lastMenuPosition{};
qvec3 vr_menu_target{};
qvec3 vr_menu_angles{};
qvec3 vr_menu_intersection_point{};

vr::IVRSystem* ovrHMD;
vr::TrackedDevicePose_t ovr_DevicePose[vr::k_unMaxTrackedDeviceCount];

static vr_eye_t eyes[2];
static vr_eye_t* current_eye = nullptr;
static vr_controller controllers[2];
static qvec3 lastOrientation{vec3_zero};
static qvec3 lastAim{vec3_zero};

static bool vr_initialized = false;

static qvec3 headOrigin{vec3_zero};
static qvec3 lastHeadOrigin{vec3_zero};
static vr::HmdVector3_t headVelocity;

qvec3 vr_roomscale_move{};

// Wolfenstein 3D, DOOM and QUAKE use the same coordinate/unit system:
// 8 foot (96 inch) height wall == 64 units, 1.5 inches per pixel unit
// 1.0 pixel unit / 1.5 inch == 0.666666 pixel units per inch
#define meters_to_units (vr_world_scale.value / (1.5f * 0.0254f))

extern cvar_t gl_farclip;

bool vr_was_teleporting{false};
bool vr_teleporting{false};
bool vr_teleporting_impact_valid{false};
bool vr_send_teleport_msg{false};
qvec3 vr_teleporting_impact{vec3_zero};
bool vr_left_grabbing{false};
bool vr_left_prevgrabbing{false};
bool vr_right_grabbing{false};
bool vr_right_prevgrabbing{false};
bool vr_left_reloading{false};
bool vr_left_prevreloading{false};
bool vr_right_reloading{false};
bool vr_right_prevreloading{false};
bool vr_left_reloadflicking{false};
bool vr_left_prevreloadflicking{false};
bool vr_right_reloadflicking{false};
bool vr_right_prevreloadflicking{false};

float flickreload_effect[2]{};

vr::VRSkeletalSummaryData_t vr_ss_lefthand;
vr::VRSkeletalSummaryData_t vr_ss_righthand;

std::array<std::array<float, 6>, 2> vr_lastfinger_frame{};
std::array<std::array<float, 6>, 2> vr_fingertracking_frame{};

float vr_menu_mouse_x{};
float vr_menu_mouse_y{};
bool vr_menu_mouse_click{};

VrGunWallCollision vr_gun_wall_collision[2];

float vr_2h_aim_transition[2]{0.f, 0.f};
float vr_2h_aim_stock_transition[2]{0.f, 0.f};
bool gotLastPlayerOrigin{false};
qvec3 lastPlayerOrigin;
float lastPlayerHeadYaw{};
float lastVrYawDiff{};
float vr_menu_mult{0.f};
bool vr_should_aim_2h[2]{};
bool vr_active_2h_helping_hand[2]{};
int vr_hardcoded_wpn_cvar_fist{16};
qvec3 lastMenuAngles;

struct WpnButtonState
{
    bool _hover{false};
    bool _prevHover{false};
    float _lastClTime{0.f};
};

WpnButtonState vr_wpnbutton_state[2];

// TODO VR: (P2) not sure what this number should actually be...
enum
{
    e_MAX_WEAPONS = 32
};

cvar_t
    vr_weapon_offset[e_MAX_WEAPONS * static_cast<std::size_t>(WpnCVar::k_Max)];

float vr_debug_max_handvelmag{0.f};
float vr_debug_max_handvelmag_timeout{0.f};

//
//
//
// ----------------------------------------------------------------------------
// VR Rendering
// ----------------------------------------------------------------------------

void RecreateTextures(
    fbo_t* const fbo, const int width, const int height) noexcept
{
    const GLuint oldDepth = fbo->depth_texture;
    const GLuint oldTexture = fbo->texture;

    glGenTextures(1, &fbo->depth_texture);
    glGenTextures(1, &fbo->texture);

    if(oldDepth)
    {
        glDeleteTextures(1, &oldDepth);
        glDeleteTextures(1, &oldTexture);
    }

    glBindTexture(GL_TEXTURE_2D, fbo->depth_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,
        GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

    glBindTexture(GL_TEXTURE_2D, fbo->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
        GL_UNSIGNED_BYTE, nullptr);

    fbo->size.width = width;
    fbo->size.height = height;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo->framebuffer);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo->texture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
        fbo->depth_texture, 0);
}

[[nodiscard]] fbo_t CreateFBO(const int width, const int height) noexcept
{
    fbo_t fbo;

    glGenFramebuffers(1, &fbo.framebuffer);

    fbo.depth_texture = 0;

    RecreateTextures(&fbo, width, height);

    fbo.msaa = 0;
    fbo.msaa_framebuffer = 0;
    fbo.msaa_texture = 0;

    return fbo;
}

[[nodiscard]] fbo_t& VR_GetEyeFBO(const int index) noexcept
{
    return eyes[index].fbo;
}

void CreateMSAA(fbo_t* const fbo, const int width, const int height,
    const int msaa) noexcept
{
    assert(msaa <= quake::util::getMaxMSAALevel());

    fbo->msaa = msaa;

    if(fbo->msaa_framebuffer)
    {
        glDeleteFramebuffers(1, &fbo->msaa_framebuffer);
        glDeleteTextures(1, &fbo->msaa_texture);
        glDeleteTextures(1, &fbo->msaa_depth_texture);
    }

    glGenFramebuffers(1, &fbo->msaa_framebuffer);
    glGenTextures(1, &fbo->msaa_texture);
    glGenTextures(1, &fbo->msaa_depth_texture);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, fbo->msaa_texture);
    glTexImage2DMultisample(
        GL_TEXTURE_2D_MULTISAMPLE, msaa, GL_RGBA8, width, height, false);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, fbo->msaa_depth_texture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaa,
        GL_DEPTH_COMPONENT24, width, height, false);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo->msaa_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D_MULTISAMPLE, fbo->msaa_texture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D_MULTISAMPLE, fbo->msaa_depth_texture, 0);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE)
    {
        Con_Printf("Framebuffer incomplete %x", status);
    }
}

// TODO VR: (P2) never called
void DeleteFBO(const fbo_t& fbo)
{
    glDeleteFramebuffers(1, &fbo.framebuffer);
    glDeleteTextures(1, &fbo.depth_texture);
    glDeleteTextures(1, &fbo.texture);
}

// TODO VR: (P2) move to util? This uses `vrYaw` inside
[[nodiscard]] qvec3 QuatToYawPitchRoll(const vr::HmdQuaternion_t& q) noexcept
{
    const auto sqw = q.w * q.w;
    const auto sqx = q.x * q.x;
    const auto sqy = q.y * q.y;
    const auto sqz = q.z * q.z;

    qvec3 out;

    out[PITCH] = -asin(-2 * (q.y * q.z - q.w * q.x)) / M_PI_DIV_180;
    out[YAW] = atan2(2 * (q.x * q.z + q.w * q.y), sqw - sqx - sqy + sqz) /
                   M_PI_DIV_180 +
               VR_GetTurnYawAngle();
    out[ROLL] = -atan2(2 * (q.x * q.y + q.w * q.z), sqw - sqx + sqy - sqz) /
                M_PI_DIV_180;

    return out;
}

[[nodiscard]] static qvec3 Vec3RotateZ(
    const qvec3& in, const qfloat angle) noexcept
{
    return {
        in[0] * std::cos(angle) - in[1] * std::sin(angle), //
        in[0] * std::sin(angle) + in[1] * std::cos(angle), //
        in[2]                                              //
    };
}

[[nodiscard]] vr::HmdMatrix44_t TransposeMatrix(
    const vr::HmdMatrix44_t& in) noexcept
{
    vr::HmdMatrix44_t out;

    for(int y = 0; y < 4; y++)
    {
        for(int x = 0; x < 4; x++)
        {
            out.m[x][y] = in.m[y][x];
        }
    }

    return out;
}

[[nodiscard]] vr::HmdVector3_t AddVectors(
    const vr::HmdVector3_t& a, const vr::HmdVector3_t& b) noexcept
{
    vr::HmdVector3_t out;

    out.v[0] = a.v[0] + b.v[0];
    out.v[1] = a.v[1] + b.v[1];
    out.v[2] = a.v[2] + b.v[2];

    return out;
}

// Rotates a vector by a quaternion and returns the results
// Based on math from
// https://gamedev.stackexchange.com/questions/28395/rotating-vector3-by-a-quaternion
[[nodiscard]] vr::HmdVector3_t RotateVectorByQuaternion(
    const vr::HmdVector3_t& v, const vr::HmdQuaternion_t& q) noexcept
{
    vr::HmdVector3_t u;
    u.v[0] = q.x;
    u.v[1] = q.y;
    u.v[2] = q.z;

    // Dot products of u,v and u,u
    const auto uvDot = u.v[0] * v.v[0] + u.v[1] * v.v[1] + u.v[2] * v.v[2];
    const auto uuDot = u.v[0] * u.v[0] + u.v[1] * u.v[1] + u.v[2] * u.v[2];

    // Calculate cross product of u, v
    vr::HmdVector3_t uvCross;
    uvCross.v[0] = u.v[1] * v.v[2] - u.v[2] * v.v[1];
    uvCross.v[1] = u.v[2] * v.v[0] - u.v[0] * v.v[2];
    uvCross.v[2] = u.v[0] * v.v[1] - u.v[1] * v.v[0];

    // Calculate each vectors' result individually because there aren't
    // arthimetic functions for HmdVector3_t dsahfkldhsaklfhklsadh
    const auto s = q.w;
    vr::HmdVector3_t result;

    result.v[0] = u.v[0] * 2.0f * uvDot + (s * s - uuDot) * v.v[0] +
                  2.0f * s * uvCross.v[0];
    result.v[1] = u.v[1] * 2.0f * uvDot + (s * s - uuDot) * v.v[1] +
                  2.0f * s * uvCross.v[1];
    result.v[2] = u.v[2] * 2.0f * uvDot + (s * s - uuDot) * v.v[2] +
                  2.0f * s * uvCross.v[2];

    return result;
}

// Transforms a HMD Matrix34 to a Vector3
// Math borrowed from https://github.com/Omnifinity/OpenVR-Tracking-Example
[[nodiscard]] vr::HmdVector3_t Matrix34ToVector(
    const vr::HmdMatrix34_t& in) noexcept
{
    vr::HmdVector3_t vector;

    vector.v[0] = in.m[0][3];
    vector.v[1] = in.m[1][3];
    vector.v[2] = in.m[2][3];

    return vector;
}

// Transforms a HMD Matrix34 to a Quaternion
// Function logic nicked from
// https://github.com/Omnifinity/OpenVR-Tracking-Example
[[nodiscard]] vr::HmdQuaternion_t Matrix34ToQuaternion(
    const vr::HmdMatrix34_t& in) noexcept
{
    vr::HmdQuaternion_t q;

    q.w = sqrt(fmax(0, 1.0 + in.m[0][0] + in.m[1][1] + in.m[2][2])) / 2.0;
    q.x = sqrt(fmax(0, 1.0 + in.m[0][0] - in.m[1][1] - in.m[2][2])) / 2.0;
    q.y = sqrt(fmax(0, 1.0 - in.m[0][0] + in.m[1][1] - in.m[2][2])) / 2.0;
    q.z = sqrt(fmax(0, 1.0 - in.m[0][0] - in.m[1][1] + in.m[2][2])) / 2.0;
    q.x = copysign(
        q.x, static_cast<double>(in.m[2][1]) - static_cast<double>(in.m[1][2]));
    q.y = copysign(
        q.y, static_cast<double>(in.m[0][2]) - static_cast<double>(in.m[2][0]));
    q.z = copysign(
        q.z, static_cast<double>(in.m[1][0]) - static_cast<double>(in.m[0][1]));

    return q;
}

[[nodiscard]] vr::HmdQuaternion_t Matrix34ToQuaternion2(
    const vr::HmdMatrix34_t& m)
{
    const auto m0 = m.m[0][0];
    const auto m1 = m.m[0][1];
    const auto m2 = m.m[0][2];
    const auto m4 = m.m[1][0];
    const auto m5 = m.m[1][1];
    const auto m6 = m.m[1][2];
    const auto m8 = m.m[2][0];
    const auto m9 = m.m[2][1];
    const auto m10 = m.m[2][2];

    vr::HmdQuaternion_t q;

    q.w = sqrt(1 + m0 + m5 + m10) / 2.0; // Scalar
    q.x = (m9 - m6) / (4 * q.w);
    q.y = (m2 - m8) / (4 * q.w);
    q.z = (m4 - m1) / (4 * q.w);

    return q;
}

static void HmdVec3RotateY(vr::HmdVector3_t& pos, const qfloat angle) noexcept
{
    const auto s = std::sin(angle);
    const auto c = std::cos(angle);
    const auto x = c * pos.v[0] - s * pos.v[2];
    const auto y = s * pos.v[0] + c * pos.v[2];

    pos.v[0] = x;
    pos.v[2] = y;
}

//
//
//
// ----------------------------------------------------------------------------
// VR CVar Callbacks
// ----------------------------------------------------------------------------

// TODO VR: (P2) consider removing and having vr always enabled
static void VR_Enabled_f(cvar_t* var) noexcept
{
    (void)var;

    VID_VR_Disable();

    if(!vr_enabled.value)
    {
        return;
    }

    if(!VR_Enable())
    {
        // TODO VR: (P2) what to do?
        // Cvar_SetValueQuick(&vr_enabled, 0);
    }
}

static void VR_Deadzone_f(cvar_t* var) noexcept
{
    (void)var;

    // clamp the mouse to a max of 0 - 70 degrees
    const auto deadzone = CLAMP(0.0f, vr_deadzone.value, 70.0f);
    if(deadzone != vr_deadzone.value)
    {
        Cvar_SetValueQuick(&vr_deadzone, deadzone);
    }
}

//
//
//
// ----------------------------------------------------------------------------
// Weapon CVars
// ----------------------------------------------------------------------------

[[nodiscard]] static qfloat VR_GetScaleCorrect() noexcept
{
    // Initial version had 0.75 default world scale, so weapons reflect
    // that.
    return (vr_world_scale.value / 0.75_qf) * vr_gunmodelscale.value;
}

void VR_ApplyModelMod(
    const qvec3& scale, const qvec3& offsets, aliashdr_t* const hdr) noexcept
{
    const auto scaleCorrect = VR_GetScaleCorrect();

    hdr->scale = hdr->original_scale * scale * scaleCorrect;
    hdr->scale_origin = hdr->original_scale_origin + offsets;
    hdr->scale_origin *= scaleCorrect;
}

[[nodiscard]] float VR_GetEasyHandTouchBonus() noexcept
{
    return 4.5f;
}

[[nodiscard]] int VR_OtherHand(const int hand) noexcept
{
    if(hand == cVR_OffHand)
    {
        return cVR_MainHand;
    }

    assert(hand == cVR_MainHand);
    return cVR_OffHand;
}

[[nodiscard]] bool VR_IsActive2HHelpingHand(const int helpingHand) noexcept
{
    const auto holdingHand = VR_OtherHand(helpingHand);

    return vr_should_aim_2h[holdingHand] &&
           vr_active_2h_helping_hand[helpingHand];
}

[[nodiscard]] bool VR_IsHandGrabbing(const int hand) noexcept
{
    if(hand == cVR_OffHand)
    {
        return vr_left_grabbing;
    }

    assert(hand == cVR_MainHand);
    return vr_right_grabbing;
}

[[nodiscard]] bool VR_IsHandReloadFlicking(const int hand) noexcept
{
    if(hand == cVR_OffHand)
    {
        return vr_left_reloadflicking;
    }

    assert(hand == cVR_MainHand);
    return vr_right_reloadflicking;
}

void ApplyMod_Weapon(const int cvarEntry, aliashdr_t* const hdr)
{
    const auto ofs = VR_GetWpnOffsets(cvarEntry);
    const auto scale = VR_GetWpnCVarValue(cvarEntry, WpnCVar::Scale);

    VR_ApplyModelMod({scale, scale, scale}, ofs, hdr);
}

void VR_SetHandtouchParams(int hand, edict_t* player, edict_t* target)
{
    player->v.touchinghand = hand;
    target->v.handtouch_hand = hand;
    target->v.handtouch_ent = EDICT_TO_PROG(player);
}

void VR_SetFakeHandtouchParams(edict_t* player, edict_t* target)
{
    VR_SetHandtouchParams(2, player, target);
}

[[nodiscard]] int VR_GetWpnCVarFromModelName(const char* name)
{
    for(int i = 0; i < e_MAX_WEAPONS; i++)
    {
        if(!strcmp(VR_GetWpnCVar(i, WpnCVar::ID).string, name))
        {
            return i;
        }
    }

    Con_Printf("No VR offset for weapon: %s\n", name);
    return -1;
}

[[nodiscard]] int VR_GetWpnCVarFromModel(qmodel_t* model)
{
    assert(model != nullptr);
    return VR_GetWpnCVarFromModelName(model->name);
}

char* CopyWithNumeral(const char* str, int i)
{
    auto len = strlen(str);
    char* ret = (char*)malloc(len + 1);
    assert(ret != nullptr);
    strcpy(ret, str);
    ret[len - 1] = '0' + (i % 10);
    ret[len - 2] = '0' + (i / 10);
    return ret;
}

[[nodiscard]] cvar_t& VR_GetWpnCVar(
    const int cvarEntry, WpnCVar setting) noexcept
{
    return vr_weapon_offset[cvarEntry *
                                static_cast<std::size_t>(WpnCVar::k_Max) +
                            static_cast<std::uint8_t>(setting)];
}

[[nodiscard]] float VR_GetWpnCVarValue(
    const int cvarEntry, WpnCVar setting) noexcept
{
    return VR_GetWpnCVar(cvarEntry, setting).value;
}

[[nodiscard]] int& VR_GetMainHandWpnCvarEntry() noexcept
{
    // TODO VR: (P2) document usages, why do we need this?
    static int entry = 0;
    return entry;
}

[[nodiscard]] int& VR_GetOffHandWpnCvarEntry() noexcept
{
    // TODO VR: (P2) hardcoded hand/fist cvar number
    static int entry = vr_hardcoded_wpn_cvar_fist;
    return entry;
}

[[nodiscard]] int& VR_GetWpnCvarEntry(const int hand) noexcept
{
    if(hand == cVR_OffHand)
    {
        return VR_GetOffHandWpnCvarEntry();
    }

    assert(hand == cVR_MainHand);
    return VR_GetMainHandWpnCvarEntry();
}

[[nodiscard]] qvec3 VR_GetWpnOffsets(const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::OffsetX),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::OffsetY),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::OffsetZ) + vr_gunmodely.value};
}

[[nodiscard]] qvec3 VR_GetWpn2HOffsets(const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHOffsetX),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHOffsetY),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHOffsetZ)};
}

[[nodiscard]] qvec3 VR_GetWpnFixed2HOffsets(const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHFixedOffsetX),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHFixedOffsetY),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHFixedOffsetZ)};
}

[[nodiscard]] qvec3 VR_GetWpnGunOffsets(const int cvarEntry) noexcept
{
    // TODO VR: (P1) bugged at the moment, need to use angles and stop hand from
    // moving

    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::GunOffsetX),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::GunOffsetY),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::GunOffsetZ)};
}

[[nodiscard]] qvec3 VR_GetWpnFixed2HHandAngles(const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHFixedHandPitch),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHFixedHandYaw),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHFixedHandRoll)};
}

[[nodiscard]] qvec3 VR_GetWpnFixed2HMainHandOffsets(
    const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHFixedMainHandOffsetX),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHFixedMainHandOffsetY),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHFixedMainHandOffsetZ)};
}

[[nodiscard]] qvec3 VR_GetWpnAngleOffsets(const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::Pitch) + vr_gunmodelpitch.value,
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::Yaw),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::Roll)};
}

[[nodiscard]] qvec3 VR_GetWpnMuzzleOffsets(const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::MuzzleOffsetX),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::MuzzleOffsetY),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::MuzzleOffsetZ)};
}

[[nodiscard]] qvec3 VR_GetWpnButtonOffsets(const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::WpnButtonX),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::WpnButtonY),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::WpnButtonZ)};
}

[[nodiscard]] qvec3 VR_GetWpnButtonAngles(const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::WpnButtonPitch),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::WpnButtonYaw),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::WpnButtonRoll)};
}


[[nodiscard]] qvec3 VR_GetWpnTextOffsets(const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::WpnTextX),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::WpnTextY),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::WpnTextZ)};
}

[[nodiscard]] qvec3 VR_GetWpnTextAngles(const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::WpnTextPitch),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::WpnTextYaw),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::WpnTextRoll)};
}

[[nodiscard]] qvec3 VR_GetWpn2HAngleOffsets(const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHPitch),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHYaw),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHRoll)};
}

[[nodiscard]] qvec3 VR_GetWpnHandOffsets(const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::HandOffsetX),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::HandOffsetY),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::HandOffsetZ)};
}

[[nodiscard]] Wpn2HMode VR_GetWpn2HMode(const int cvarEntry) noexcept
{
    return cvarToEnum<Wpn2HMode>(VR_GetWpnCVar(cvarEntry, WpnCVar::TwoHMode));
}

[[nodiscard]] WpnCrosshairMode VR_GetWpnCrosshairMode(
    const int cvarEntry) noexcept
{
    return cvarToEnum<WpnCrosshairMode>(
        VR_GetWpnCVar(cvarEntry, WpnCVar::CrosshairMode));
}

[[nodiscard]] float VR_GetWpnLength(const int cvarEntry) noexcept
{
    // TODO VR: (P2) unused
    return VR_GetWpnCVarValue(cvarEntry, WpnCVar::Length);
}

void InitWeaponCVar(cvar_t& cvar, const char* name, int i, const char* value)
{
    const char* cvarname = CopyWithNumeral(name, i + 1);
    if(!Cvar_FindVar(cvarname))
    {
        cvar.name = cvarname;
        cvar.string = value;
        cvar.flags = CVAR_ARCHIVE;
        Cvar_RegisterVariable(&cvar);
    }
    else
    {
        Cvar_SetQuick(&cvar, value);
    }
}


int InitWeaponCVars(int i, const char* id, const char* offsetX,
    const char* offsetY, const char* offsetZ, const char* scale,
    const char* roll = "0.0", const char* pitch = "0.0",
    const char* yaw = "0.0", const char* muzzleOffsetX = "0.0",
    const char* muzzleOffsetY = "0.0", const char* muzzleOffsetZ = "0.0",
    const char* twoHOffsetX = "0.0", const char* twoHOffsetY = "0.0",
    const char* twoHOffsetZ = "0.0", const char* twoHPitch = "0.0",
    const char* twoHYaw = "0.0", const char* twoHRoll = "0.0",
    const char* twoHMode = "0.0", const char* length = "0.0",
    const char* weight = "0.0", const char* handOffsetX = "0.0",
    const char* handOffsetY = "0.0", const char* handOffsetZ = "0.0",
    const char* handAnchorVertex = "0.0", const char* offHandOffsetX = "0.0",
    const char* offHandOffsetY = "0.0", const char* offHandOffsetZ = "0.0",
    const char* crosshairMode = "0.0", const char* hideHand = "0.0",
    const char* twoHDisplayMode = "0.0",
    const char* twoHHandAnchorVertex = "0.0")
{
    (void)offHandOffsetX;
    (void)offHandOffsetY;
    (void)offHandOffsetZ;

    const auto init =
        [&](const WpnCVar wpnCVar, const char* name, const char* defaultVal)
    { InitWeaponCVar(VR_GetWpnCVar(i, wpnCVar), name, i, defaultVal); };

    // clang-format off
    init(WpnCVar::OffsetX,                  "vr_wofs_x_nn",            offsetX);
    init(WpnCVar::OffsetY,                  "vr_wofs_y_nn",            offsetY);
    init(WpnCVar::OffsetZ,                  "vr_wofs_z_nn",            offsetZ);
    init(WpnCVar::Scale,                    "vr_wofs_scale_nn",        scale);
    init(WpnCVar::ID,                       "vr_wofs_id_nn",           id);
    init(WpnCVar::Roll,                     "vr_wofs_roll_nn",         roll);
    init(WpnCVar::Yaw,                      "vr_wofs_yaw_nn",          yaw);
    init(WpnCVar::Pitch,                    "vr_wofs_pitch_nn",        pitch);
    init(WpnCVar::MuzzleOffsetX,            "vr_wofs_muzzle_x_nn",     muzzleOffsetX);
    init(WpnCVar::MuzzleOffsetY,            "vr_wofs_muzzle_y_nn",     muzzleOffsetY);
    init(WpnCVar::MuzzleOffsetZ,            "vr_wofs_muzzle_z_nn",     muzzleOffsetZ);
    init(WpnCVar::TwoHOffsetX,              "vr_wofs_2h_x_nn",         twoHOffsetX);
    init(WpnCVar::TwoHOffsetY,              "vr_wofs_2h_y_nn",         twoHOffsetY);
    init(WpnCVar::TwoHOffsetZ,              "vr_wofs_2h_z_nn",         twoHOffsetZ);
    init(WpnCVar::TwoHPitch,                "vr_wofs_2h_pitch_nn",     twoHPitch);
    init(WpnCVar::TwoHYaw,                  "vr_wofs_2h_yaw_nn",       twoHYaw);
    init(WpnCVar::TwoHRoll,                 "vr_wofs_2h_roll_nn",      twoHRoll);
    init(WpnCVar::TwoHMode,                 "vr_wofs_2h_mode_nn",      twoHMode);
    init(WpnCVar::Length,                   "vr_wofs_length_nn",       length);
    init(WpnCVar::Weight,                   "vr_wofs_weight_nn",       weight);
    init(WpnCVar::HandOffsetX,              "vr_wofs_hand_x_nn",       handOffsetX);
    init(WpnCVar::HandOffsetY,              "vr_wofs_hand_y_nn",       handOffsetY);
    init(WpnCVar::HandOffsetZ,              "vr_wofs_hand_z_nn",       handOffsetZ);
    init(WpnCVar::HandAnchorVertex,         "vr_wofs_hand_av_nn",      handAnchorVertex);

    // TODO VR: (P1) deprecated
    // init(WpnCVar::OffHandOffsetX,           "vr_wofs_offhand_x_nn",    offHandOffsetX);
    // init(WpnCVar::OffHandOffsetY,           "vr_wofs_offhand_y_nn",    offHandOffsetY);
    // init(WpnCVar::OffHandOffsetZ,           "vr_wofs_offhand_z_nn",    offHandOffsetZ);

    init(WpnCVar::CrosshairMode,            "vr_wofs_ch_mode_z_nn",    crosshairMode);
    init(WpnCVar::HideHand,                 "vr_wofs_hide_hand_nn",    hideHand);
    init(WpnCVar::TwoHDisplayMode,          "vr_wofs_2h_dispmd_nn",    twoHDisplayMode);
    init(WpnCVar::TwoHHandAnchorVertex,     "vr_wofs_2h_hand_av_nn",   twoHHandAnchorVertex);
    init(WpnCVar::TwoHFixedOffsetX,         "vr_wofs_2h_fxd_ox_nn",    "0.0");
    init(WpnCVar::TwoHFixedOffsetY,         "vr_wofs_2h_fxd_oy_nn",    "0.0");
    init(WpnCVar::TwoHFixedOffsetZ,         "vr_wofs_2h_fxd_oz_nn",    "0.0");
    init(WpnCVar::GunOffsetX,               "vr_wofs_gunoff_x_nn",     "0.0");
    init(WpnCVar::GunOffsetY,               "vr_wofs_gunoff_y_nn",     "0.0");
    init(WpnCVar::GunOffsetZ,               "vr_wofs_gunoff_z_nn",     "0.0");
    init(WpnCVar::TwoHFixedHandPitch,       "vr_wofs_2h_fxd_hp_nn",    "0.0");
    init(WpnCVar::TwoHFixedHandYaw,         "vr_wofs_2h_fxd_hy_nn",    "0.0");
    init(WpnCVar::TwoHFixedHandRoll,        "vr_wofs_2h_fxd_hr_nn",    "0.0");
    init(WpnCVar::TwoHFixedMainHandOffsetX, "vr_wofs_2h_fxd_mh_ox_nn", "0.0");
    init(WpnCVar::TwoHFixedMainHandOffsetY, "vr_wofs_2h_fxd_mh_oy_nn", "0.0");
    init(WpnCVar::TwoHFixedMainHandOffsetZ, "vr_wofs_2h_fxd_mh_oz_nn", "0.0");
    init(WpnCVar::WeightPosMult,            "vr_wofs_w_posmult_nn",    "1.0");
    init(WpnCVar::WeightDirMult,            "vr_wofs_w_dirmult_nn",    "1.0");
    init(WpnCVar::WeightHandVelMult,        "vr_wofs_w_hvelmult_nn",   "1.0");
    init(WpnCVar::WeightHandThrowVelMult,   "vr_wofs_w_htvelmult_nn",  "1.0");
    init(WpnCVar::Weight2HPosMult,          "vr_wofs_w_2hposmult_nn",  "1.0");
    init(WpnCVar::Weight2HDirMult,          "vr_wofs_w_2hdirmult_nn",  "1.0");
    init(WpnCVar::WpnButtonMode,            "vr_wofs_wpnbtnmode_nn",   "0.0");
    init(WpnCVar::WpnButtonX,               "vr_wofs_wpnbtn_x_nn",     "0.0");
    init(WpnCVar::WpnButtonY,               "vr_wofs_wpnbtn_y_nn",     "0.0");
    init(WpnCVar::WpnButtonZ,               "vr_wofs_wpnbtn_z_nn",     "0.0");
    init(WpnCVar::WpnButtonAnchorVertex,    "vr_wofs_wpnbtn_av_nn",    "0.0");

    // TODO VR: (P1) deprecated
    // init(WpnCVar::WpnButtonOffHandX,        "vr_wofs_wpnbtn_oh_x_nn",  "0.0");
    // init(WpnCVar::WpnButtonOffHandY,        "vr_wofs_wpnbtn_oh_y_nn",  "0.0");
    // init(WpnCVar::WpnButtonOffHandZ,        "vr_wofs_wpnbtn_oh_z_nn",  "0.0");

    init(WpnCVar::WpnButtonPitch,           "vr_wofs_wpnbtn_pitch_nn", "0.0");
    init(WpnCVar::WpnButtonYaw,             "vr_wofs_wpnbtn_yaw_nn",   "0.0");
    init(WpnCVar::WpnButtonRoll,            "vr_wofs_wpnbtn_roll_nn",  "0.0");
    init(WpnCVar::MuzzleAnchorVertex,       "vr_wofs_muzzle_av_nn",    "0.0");
    init(WpnCVar::ZeroBlend,                "vr_wofs_zb_nn",           "0.0");
    init(WpnCVar::TwoHZeroBlend,            "vr_wofs_zb_2h_nn",        "0.0");
    init(WpnCVar::WpnTextMode,              "vr_wofs_wpntxtmode_nn",   "0.0");
    init(WpnCVar::WpnTextX,                 "vr_wofs_wpntxt_x_nn",     "0.0");
    init(WpnCVar::WpnTextY,                 "vr_wofs_wpntxt_y_nn",     "0.0");
    init(WpnCVar::WpnTextZ,                 "vr_wofs_wpntxt_z_nn",     "0.0");
    init(WpnCVar::WpnTextAnchorVertex,      "vr_wofs_wpntxt_av_nn",    "0.0");
    init(WpnCVar::WpnTextPitch,             "vr_wofs_wpntxt_pitch_nn", "0.0");
    init(WpnCVar::WpnTextYaw,               "vr_wofs_wpntxt_yaw_nn",   "0.0");
    init(WpnCVar::WpnTextRoll,              "vr_wofs_wpntxt_roll_nn",  "0.0");
    init(WpnCVar::WpnTextScale,             "vr_wofs_wpntxt_scale_nn", "1.0");
    // clang-format on

    return i;
}

// TODO VR: (P2) get rid of this or update with new values
void InitAllWeaponCVars()
{
    // clang-format off

    int i = 0;
    if(!strcmp(COM_SkipPath(com_gamedir), "ad"))
    {
        // weapons for Arcane Dimensions mod; initially made for v1.70 + patch1
        InitWeaponCVars(i++, "progs/v_shadaxe0.mdl", "-1.5", "43.1", "41", "0.25"); // shadow axe
        InitWeaponCVars(i++, "progs/v_shadaxe3.mdl", "-1.5", "43.1", "41", "0.25"); // shadow axe upgrade, same numbers
        InitWeaponCVars(i++, "progs/v_shot.mdl", "1.5", "1.7", "17.5", "0.33"); // shotgun
        InitWeaponCVars(i++, "progs/v_shot2.mdl", "-3.5", "0.4", "8.5", "0.8"); // double barrel shotgun
        InitWeaponCVars(i++, "progs/v_shot3.mdl", "-3.5", "0.4", "8.5", "0.8"); // triple barrel shotgun ("Widowmaker")
        InitWeaponCVars(i++, "progs/v_nail.mdl", "-9.5", "3", "17", "0.5"); // nailgun
        InitWeaponCVars(i++, "progs/v_nail2.mdl", "-6", "3.5", "20", "0.4"); // supernailgun
        InitWeaponCVars(i++, "progs/v_rock.mdl", "-3", "1.25", "17", "0.5"); // grenade
        InitWeaponCVars(i++, "progs/v_rock2.mdl", "0", "5.55", "22.5", "0.45"); // rocket
        InitWeaponCVars(i++, "progs/v_light.mdl", "-4", "3.1", "13", "0.5"); // lightning
        InitWeaponCVars(i++, "progs/v_plasma.mdl", "2.8", "1.8", "22.5", "0.5"); // plasma
    }
    else
    {
        // weapons for vanilla Quake, Scourge of Armagon, Dissolution of Eternity

        // vanilla quake weapons
        InitWeaponCVars(i++, "progs/v_axe.mdl", "-4", "24", "37", "0.33");
        InitWeaponCVars(i++, "progs/v_shot.mdl", "1.5", "1", "10", "0.5"); // gun
        InitWeaponCVars(i++, "progs/v_shot2.mdl", "-3.5", "1", "8.5", "0.8"); // shotgun
        InitWeaponCVars(i++, "progs/v_nail.mdl", "-5", "3", "15", "0.5"); // nailgun
        InitWeaponCVars(i++, "progs/v_nail2.mdl", "0", "3", "19", "0.5"); // supernailgun
        InitWeaponCVars(i++, "progs/v_rock.mdl", "10", "1.5", "13", "0.5"); // grenade
        InitWeaponCVars(i++, "progs/v_rock2.mdl", "10", "7", "19", "0.5"); // rocket
        InitWeaponCVars(i++, "progs/v_light.mdl", "3", "4", "13", "0.5"); // lightning

        // hipnotic weapons
        InitWeaponCVars(i++, "progs/v_hammer.mdl", "-4", "18", "37", "0.33"); // mjolnir hammer
        InitWeaponCVars(i++, "progs/v_laserg.mdl", "65", "3.7", "17", "0.33"); // laser
        InitWeaponCVars(i++, "progs/v_prox.mdl", "10", "1.5", "13", "0.5"); // proximity - same as grenade

        // rogue weapons
        InitWeaponCVars(i++, "progs/v_lava.mdl", "-5", "3", "15", "0.5"); // lava nailgun - same as nailgun
        InitWeaponCVars(i++, "progs/v_lava2.mdl", "0", "3", "19", "0.5"); // lava supernailgun - same as supernailgun
        InitWeaponCVars(i++, "progs/v_multi.mdl", "10", "1.5", "13", "0.5"); // multigrenade - same as grenade
        InitWeaponCVars(i++, "progs/v_multi2.mdl", "10", "7", "19", "0.5"); // multirocket - same as rocket
        InitWeaponCVars(i++, "progs/v_plasma.mdl", "3", "4", "13", "0.5"); // plasma - same as lightning
    }

    // empty hand
    vr_hardcoded_wpn_cvar_fist =
        InitWeaponCVars(i++, "progs/hand.mdl", "0.0", "0.0", "0.0", "0.0");

    // grapple gun
    InitWeaponCVars(i++, "progs/v_grpple.mdl", "0.0", "0.0", "0.0", "0.0");

    // clang-format on

    while(i < e_MAX_WEAPONS)
    {
        InitWeaponCVars(i++, "-1", "1.5", "1", "10", "0.5");
    }
}

//
//
//
// ----------------------------------------------------------------------------
// VR Initialization
// ----------------------------------------------------------------------------

void VR_InitCvars()
{
    // This is only called once at game start
    Cvar_SetCallback(&vr_enabled, VR_Enabled_f);
    Cvar_SetCallback(&vr_deadzone, VR_Deadzone_f);

    // TODO VR: (P2) what to do here?
    Cvar_SetValueQuick(&vr_enabled, 1);

    quake::vr::register_all_cvars();
    InitAllWeaponCVars();
}

void VID_VR_Init()
{
    // VR: Fix grenade model flags to enable smoke trail.
    Mod_ForName("progs/grenade.mdl", true)->flags |= EF_GRENADE;
    Mod_ForName("progs/proxbomb.mdl", true)->flags |= EF_GRENADE;
    Mod_ForName("progs/mervup.mdl", true)->flags |= EF_GRENADE;

    // VR: Disable rotation of backpack model.
    Mod_ForName("progs/backpack.mdl", true)->flags &= ~EF_ROTATE;

    // Set the cvar if invoked from a command line parameter
    {
        // int i = COM_CheckParm("-vr");
        // if (i && i < com_argc - 1) {
        Cvar_SetQuick(&vr_enabled, "1");
        //}
    }
}

void VR_InitGame()
{
    InitAllWeaponCVars();
}

void VR_ModVRTorsoModel()
{
    const qvec3 vrTorsoScale{vr_vrtorso_x_scale.value, vr_vrtorso_y_scale.value,
        vr_vrtorso_z_scale.value};

    auto* model = Mod_ForName("progs/vrtorso.mdl", true);
    auto* hdr = (aliashdr_t*)Mod_Extradata(model);

    VR_ApplyModelMod(vrTorsoScale, vec3_zero, hdr);
}

void VR_ModVRLegHolsterModel()
{
    const auto factor = vr_leg_holster_model_scale.value;
    const qvec3 legHolsterScale{factor, factor, factor};

    auto* model = Mod_ForName("progs/legholster.mdl", true);
    auto* hdr = (aliashdr_t*)Mod_Extradata(model);

    VR_ApplyModelMod(legHolsterScale,
        {vr_leg_holster_model_x_offset.value,
            vr_leg_holster_model_y_offset.value,
            vr_leg_holster_model_z_offset.value},
        hdr);
}

void VR_ModAllWeapons()
{
    for(int i = 0; i < e_MAX_WEAPONS; ++i)
    {
        const char* cvarname = CopyWithNumeral("vr_wofs_id_nn", i + 1);

        const cvar_t* cvar = Cvar_FindVar(cvarname);
        if(cvar == nullptr)
        {
            Con_Printf("null cvar %s\n", cvarname);
            continue;
        }

        const char* mdlName = cvar->string;

        if(strcmp(mdlName, "-1") == 0)
        {
            continue;
        }

        auto* model = Mod_ForName(mdlName, true);
        auto* hdr = (aliashdr_t*)Mod_Extradata(model);
        ApplyMod_Weapon(i, hdr);
    }
}

void VR_ModAllModels()
{
    VR_ModVRTorsoModel();
    VR_ModVRLegHolsterModel();
    VR_ModAllWeapons();
}

[[nodiscard]] bool VR_EnabledAndNotFake() noexcept
{
    return vr_enabled.value == 1 && vr_fakevr.value == 0;
}

//
//
//
// ----------------------------------------------------------------------------
// VR Action Handles
// ----------------------------------------------------------------------------

vr::VRActiveActionSet_t vrActiveActionSet;

vr::VRActionSetHandle_t vrashDefault;
vr::VRActionSetHandle_t vrashMenu;

vr::VRActionHandle_t vrahLeftHandAnim;
vr::VRActionHandle_t vrahRightHandAnim;
vr::VRActionHandle_t vrahLocomotion;
vr::VRActionHandle_t vrahTurn;
vr::VRActionHandle_t vrahFireMainHand;
vr::VRActionHandle_t vrahFireOffHand;
vr::VRActionHandle_t vrahJump;
vr::VRActionHandle_t vrahPrevWeaponMainHand;
vr::VRActionHandle_t vrahNextWeaponMainHand;
vr::VRActionHandle_t vrahEscape;
vr::VRActionHandle_t vrahSpeed;
vr::VRActionHandle_t vrahTeleport;
vr::VRActionHandle_t vrahLeftGrab;
vr::VRActionHandle_t vrahRightGrab;
vr::VRActionHandle_t vrahLeftReload;
vr::VRActionHandle_t vrahRightReload;
vr::VRActionHandle_t vrahPrevWeaponOffHand;
vr::VRActionHandle_t vrahNextWeaponOffHand;
vr::VRActionHandle_t vrahBMoveForward;
vr::VRActionHandle_t vrahBMoveBackward;
vr::VRActionHandle_t vrahBMoveLeft;
vr::VRActionHandle_t vrahBMoveRight;
vr::VRActionHandle_t vrahBTurnLeft;
vr::VRActionHandle_t vrahBTurnRight;
vr::VRActionHandle_t vrahLeftHaptic;
vr::VRActionHandle_t vrahRightHaptic;

vr::VRActionHandle_t vrahMenuNavigation;
vr::VRActionHandle_t vrahMenuUp;
vr::VRActionHandle_t vrahMenuDown;
vr::VRActionHandle_t vrahMenuLeft;
vr::VRActionHandle_t vrahMenuRight;
vr::VRActionHandle_t vrahMenuEnter;
vr::VRActionHandle_t vrahMenuBack;
vr::VRActionHandle_t vrahMenuAddToShortcuts;
vr::VRActionHandle_t vrahMenuMultiplierHalf;
vr::VRActionHandle_t vrahMenuMultiplierPlusOne;
vr::VRActionHandle_t vrahMenuMultiplierPlusOne2;

vr::VRInputValueHandle_t vrivhLeft;
vr::VRInputValueHandle_t vrivhRight;

static void VR_InitActionHandles()
{
    // -----------------------------------------------------------------------
    // VR: Read "default" action set handle.
    {
        const auto rc = vr::VRInput()->GetActionSetHandle(
            "/actions/default", &vrashDefault);

        if(rc != vr::EVRInputError::VRInputError_None)
        {
            Con_Printf("Failed to read Steam VR action set handle, rc = %d\n",
                (int)rc);
        }
    }

    // -----------------------------------------------------------------------
    // VR: Read "menu" action set handle.
    {
        const auto rc =
            vr::VRInput()->GetActionSetHandle("/actions/menu", &vrashMenu);

        if(rc != vr::EVRInputError::VRInputError_None)
        {
            Con_Printf("Failed to read Steam VR action set handle, rc = %d\n",
                (int)rc);
        }
    }

    // -----------------------------------------------------------------------
    // VR: Read all action handles.
    const auto readHandle = [](const char* name, vr::VRActionHandle_t& handle)
    {
        const auto rc = vr::VRInput()->GetActionHandle(name, &handle);

        if(rc != vr::EVRInputError::VRInputError_None)
        {
            Con_Printf(
                "Failed to read Steam VR action handle, rc = %d\n", (int)rc);
        }
    };

    // Skeleton options.
    readHandle("/actions/default/in/LeftHandAnim", vrahLeftHandAnim);
    readHandle("/actions/default/in/RightHandAnim", vrahRightHandAnim);

    // Analog joystick options.
    readHandle("/actions/default/in/Locomotion", vrahLocomotion);
    readHandle("/actions/default/in/Turn", vrahTurn);

    // Boolean options.
    readHandle("/actions/default/in/FireMainHand", vrahFireMainHand);
    readHandle("/actions/default/in/FireOffHand", vrahFireOffHand);
    readHandle("/actions/default/in/Jump", vrahJump);
    readHandle(
        "/actions/default/in/PrevWeaponMainHand", vrahPrevWeaponMainHand);
    readHandle(
        "/actions/default/in/NextWeaponMainHand", vrahNextWeaponMainHand);
    readHandle("/actions/default/in/Escape", vrahEscape);
    readHandle("/actions/default/in/Speed", vrahSpeed);
    readHandle("/actions/default/in/Teleport", vrahTeleport);
    readHandle("/actions/default/in/LeftGrab", vrahLeftGrab);
    readHandle("/actions/default/in/RightGrab", vrahRightGrab);
    readHandle("/actions/default/in/LeftReload", vrahLeftReload);
    readHandle("/actions/default/in/RightReload", vrahRightReload);
    readHandle("/actions/default/in/PrevWeaponOffHand", vrahPrevWeaponOffHand);
    readHandle("/actions/default/in/NextWeaponOffHand", vrahNextWeaponOffHand);

    // Boolean locomotion options (useful for accessibility and legacy HMDs).
    readHandle("/actions/default/in/BMoveForward", vrahBMoveForward);
    readHandle("/actions/default/in/BMoveBackward", vrahBMoveBackward);
    readHandle("/actions/default/in/BMoveLeft", vrahBMoveLeft);
    readHandle("/actions/default/in/BMoveRight", vrahBMoveRight);
    readHandle("/actions/default/in/BTurnLeft", vrahBTurnLeft);
    readHandle("/actions/default/in/BTurnRight", vrahBTurnRight);

    // Haptics.
    readHandle("/actions/default/out/LeftHaptic", vrahLeftHaptic);
    readHandle("/actions/default/out/RightHaptic", vrahRightHaptic);

    // Menu.
    readHandle("/actions/menu/in/Navigation", vrahMenuNavigation);
    readHandle("/actions/menu/in/Up", vrahMenuUp);
    readHandle("/actions/menu/in/Down", vrahMenuDown);
    readHandle("/actions/menu/in/Left", vrahMenuLeft);
    readHandle("/actions/menu/in/Right", vrahMenuRight);
    readHandle("/actions/menu/in/Enter", vrahMenuEnter);
    readHandle("/actions/menu/in/Back", vrahMenuBack);
    readHandle("/actions/menu/in/AddToShortcuts", vrahMenuAddToShortcuts);
    readHandle("/actions/menu/in/MultiplierHalf", vrahMenuMultiplierHalf);
    readHandle("/actions/menu/in/MultiplierPlusOne", vrahMenuMultiplierPlusOne);
    readHandle(
        "/actions/menu/in/MultiplierPlusOne2", vrahMenuMultiplierPlusOne2);

    vrActiveActionSet.ulActionSet = vrashDefault;
    vrActiveActionSet.ulRestrictedToDevice = vr::k_ulInvalidInputValueHandle;
    vrActiveActionSet.nPriority = 0;

    // -----------------------------------------------------------------------
    // VR: Get handles to the controllers.
    const auto readInputSourceHandle =
        [](const char* name, vr::VRInputValueHandle_t& handle)
    {
        const auto rc = vr::VRInput()->GetInputSourceHandle(name, &handle);

        if(rc != vr::EVRInputError::VRInputError_None)
        {
            Con_Printf("Failed to read Steam VR input source handle, rc = %d\n",
                (int)rc);
        }
    };

    readInputSourceHandle("/user/hand/left", vrivhLeft);
    readInputSourceHandle("/user/hand/right", vrivhRight);
}

//
//
//
// ----------------------------------------------------------------------------
// TODO VR: (P2) reorganize
// ----------------------------------------------------------------------------

[[nodiscard]] static bool inMenu() noexcept
{
    return key_dest == key_menu;
}

[[nodiscard]] static bool inConsole() noexcept
{
    return key_dest == key_console;
}

[[nodiscard]] static bool inMenuOrConsole() noexcept
{
    return inMenu() || inConsole();
}

[[nodiscard]] static bool inGame() noexcept
{
    return key_dest == key_game;
}

[[nodiscard]] edict_t* getPlayerEdictUnchecked() noexcept
{
    return svs.clients[0].edict;
    // return sv_player;
}

[[nodiscard]] bool svPlayerActive() noexcept
{
    // TODO VR: (P2) document, this is because of Host_ClearMemory and map
    // change callback
    // return getPlayerEdictUnchecked() != nullptr && cls.signon == SIGNONS;

    // sv.active == true && sv.state == ss_active

    return svs.clients != nullptr && svs.clients[0].active &&
           svs.clients[0].spawned && getPlayerEdictUnchecked() != nullptr &&
           sv.active == true && sv.state == ss_active && cls.signon == SIGNONS;
}

// TODO VR: (P1) grep for invocations of the fn below for multiplayer issues
[[nodiscard]] edict_t* getPlayerEdict() noexcept
{
    assert(svPlayerActive());
    return getPlayerEdictUnchecked();
}

bool VR_Enable()
{
    if(COM_CheckParm("-fakevr"))
    {
        Cvar_SetValueQuick(&vr_fakevr, 1);
    }

    if(COM_CheckParm("-novr"))
    {
        Cvar_SetValueQuick(&vr_fakevr, 1);
        Cvar_SetValueQuick(&vr_novrinit, 1);
    }

    if(vr_fakevr.value && vr_novrinit.value)
    {
        return true;
    }

    if(vr_initialized)
    {
        return true;
    }

    vr::EVRInitError eInit = vr::VRInitError_None;
    ovrHMD = vr::VR_Init(&eInit, vr::VRApplication_Scene);

    if(eInit != vr::VRInitError_None)
    {
        Con_Printf("%s\nFailed to initialize Steam VR",
            VR_GetVRInitErrorAsEnglishDescription(eInit));
        return false;
    }

    {
        static std::string manifestPath =
            vr_working_directory + "/actions.json";

        Con_Printf(
            "Set Steam VR action manifest path to: '%s'", manifestPath.c_str());

        const auto rc =
            vr::VRInput()->SetActionManifestPath(manifestPath.c_str());

        if(rc != vr::EVRInputError::VRInputError_None)
        {
            Con_Printf(
                "Failed to read Steam VR action manifest, rc = %d\n", (int)rc);
        }
        else
        {
            VR_InitActionHandles();
        }
    }

    eyes[0].eye = vr::Eye_Left;
    eyes[1].eye = vr::Eye_Right;

    for(int i = 0; i < 2; i++)
    {
        uint32_t vrwidth, vrheight;
        ovrHMD->GetRecommendedRenderTargetSize(&vrwidth, &vrheight);

        float LeftTan, RightTan, UpTan, DownTan;
        ovrHMD->GetProjectionRaw(
            eyes[i].eye, &LeftTan, &RightTan, &UpTan, &DownTan);

        eyes[i].index = i;
        eyes[i].fbo = CreateFBO(vrwidth, vrheight);
        eyes[i].fov_x =
            (std::atan(-LeftTan) + std::atan(RightTan)) / float(M_PI_DIV_180);
        eyes[i].fov_y =
            (std::atan(-UpTan) + std::atan(DownTan)) / float(M_PI_DIV_180);
    }

    // TODO VR: (P2) seated mode?
    // Put us into standing tracking position
    vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseStanding);

    VR_ResetOrientation(); // Recenter the HMD

    if(const int rc = SDL_GL_SetSwapInterval(0); rc != 0) // Disable V-Sync
    {
        Con_Printf("Error disabling VSync, rc=%d\n", rc);
    }

    Cbuf_AddText(
        "exec vr_autoexec.cfg\n"); // Load the vr autosec config file incase
                                   // the user has settings they want

    vr_initialized = true;
    return true;
}

void VR_PushYaw()
{
    readbackYaw = true;
}

void VID_VR_Shutdown()
{
    VID_VR_Disable();
}

void VID_VR_Disable()
{
    if(!vr_initialized)
    {
        return;
    }

    vr::VR_Shutdown();
    ovrHMD = nullptr;

    // Reset the view height
    cl.stats[STAT_VIEWHEIGHT] = DEFAULT_VIEWHEIGHT;

    // TODO: Cleanup frame buffers

    vr_initialized = false;
}

static void RenderScreenForCurrentEye_OVR(vr_eye_t& eye)
{
    if(isDedicated || vr_fakevr.value || vr_novrinit.value)
    {
        return;
    }

    // Remember the current glwidth/height; we have to modify it here for
    // each eye
    const int oldglheight = glheight;
    const int oldglwidth = glwidth;

    uint32_t cglwidth = glwidth;
    uint32_t cglheight = glheight;
    ovrHMD->GetRecommendedRenderTargetSize(&cglwidth, &cglheight);
    glwidth = cglwidth;
    glheight = cglheight;

    const bool newTextures =
        glwidth != eye.fbo.size.width || glheight != eye.fbo.size.height;

    if(newTextures)
    {
        RecreateTextures(&eye.fbo, glwidth, glheight);
    }

    if(const int maxMsaa = quake::util::getMaxMSAALevel();
        vr_msaa.value > maxMsaa)
    {
        Con_Printf(
            "Unsupported MSAA level. Changing to supported max '%d'.", maxMsaa);

        Cvar_SetValueQuick(&vr_msaa, maxMsaa);
    }

    if(newTextures || vr_msaa.value != eye.fbo.msaa)
    {
        CreateMSAA(&eye.fbo, glwidth, glheight, vr_msaa.value);
    }

    // Set up current FBO
    if(eye.fbo.msaa > 0)
    {
        glEnable(GL_MULTISAMPLE);
        glBindFramebuffer(GL_FRAMEBUFFER, eye.fbo.msaa_framebuffer);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, eye.fbo.framebuffer);
    }

    glViewport(0, 0, eye.fbo.size.width, eye.fbo.size.height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw everything
    srand((int)(cl.time * 1000)); // sync random stuff between eyes

    r_refdef.fov_x = eye.fov_x;
    r_refdef.fov_y = eye.fov_y;

    SCR_UpdateScreenContent();

    // Generate the eye texture and send it to the HMD

    if(eye.fbo.msaa > 0)
    {
        glDisable(GL_MULTISAMPLE);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, eye.fbo.framebuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, eye.fbo.msaa_framebuffer);
        glBlitFramebuffer(0, 0, glwidth, glheight, 0, 0, glwidth, glheight,
            GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    vr::Texture_t eyeTexture = {
        reinterpret_cast<void*>(uintptr_t(eye.fbo.texture)),
        vr::TextureType_OpenGL, vr::ColorSpace_Gamma};
    vr::VRCompositor()->Submit(eye.eye, &eyeTexture);

    // Reset
    glwidth = oldglwidth;
    glheight = oldglheight;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Get a reasonable height around where hands should be when aiming a gun.
[[nodiscard]] float VR_GetHandZOrigin(const qvec3& playerOrigin) noexcept
{
    return playerOrigin[2] + vr_floor_offset.value + vr_gun_z_offset.value;
}

// Get the player origin vector, but adjusted to the upper torso on the Z axis.
[[nodiscard]] qvec3 VR_GetAdjustedPlayerOrigin(qvec3 playerOrigin) noexcept
{
    playerOrigin[2] = VR_GetHandZOrigin(playerOrigin) + 40;
    return playerOrigin;
}

[[nodiscard]] static qfloat VR_GetWeaponWeightFactorImpl(const int cvarEntry,
    const float aiming2H, const float weightOffset, const float weightMult,
    const float twoHHelpOffset, const float twoHHelpMult,
    const float wpnTwoHMult)
{
    assert(aiming2H >= 0.f && aiming2H <= 1.f);

    const auto initial = 1.f - VR_GetWpnCVarValue(cvarEntry, WpnCVar::Weight);
    const auto withOffset = initial + weightOffset;
    const auto withMult = withOffset * weightMult;
    const auto with2HHelpOffset = withOffset + twoHHelpOffset;
    const auto with2HHelpMult = (with2HHelpOffset * twoHHelpMult) * wpnTwoHMult;

    const float finalFactor = lerp(withMult, with2HHelpMult, aiming2H);
    return std::clamp(finalFactor, 0.f, 1.f);
}

[[nodiscard]] qfloat VR_GetWeaponWeightPosFactor(
    const int cvarEntry, const float aiming2H)
{
    return VR_GetWeaponWeightFactorImpl(cvarEntry, aiming2H,    //
        vr_wpn_pos_weight_offset.value,                         //
        vr_wpn_pos_weight_mult.value,                           //
        vr_wpn_pos_weight_2h_help_offset.value,                 //
        vr_wpn_pos_weight_2h_help_mult.value,                   //
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::Weight2HPosMult) //
    );
}

[[nodiscard]] qfloat VR_GetWeaponWeightDirFactor(
    const int cvarEntry, const float aiming2H)
{
    return VR_GetWeaponWeightFactorImpl(cvarEntry, aiming2H,    //
        vr_wpn_dir_weight_offset.value,                         //
        vr_wpn_dir_weight_mult.value,                           //
        vr_wpn_dir_weight_2h_help_offset.value,                 //
        vr_wpn_dir_weight_2h_help_mult.value,                   //
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::Weight2HDirMult) //
    );
}

template <auto TFactorFn>
[[nodiscard]] static qfloat VR_CalcWeaponWeight(const int handIndex) noexcept
{
    return (handIndex == 0 && !vr_should_aim_2h[handIndex])
               ? (*TFactorFn)(VR_GetOffHandWpnCvarEntry(), 0.f)
               : (*TFactorFn)(VR_GetMainHandWpnCvarEntry(),
                     vr_2h_aim_transition[handIndex]);
}

template <auto TFactorFn>
[[nodiscard]] static qfloat VR_CalcWeaponWeightFTAdjusted(
    const int handIndex) noexcept
{
    const auto weaponWeight = VR_CalcWeaponWeight<TFactorFn>(handIndex);
    const auto frametime = cl.time - cl.oldtime;
    return (weaponWeight * frametime) * 100.f;
}

[[nodiscard]] const qvec3& VR_GetHeadOrigin() noexcept
{
    return lastHeadOrigin;
}

void debugPrintHandvel(const int index, const float linearity)
{
    if(index == cVR_MainHand && vr_debug_print_handvel.value)
    {
        if(cl.handvelmag[index] > vr_debug_max_handvelmag)
        {
            vr_debug_max_handvelmag = cl.handvelmag[index];
        }

        if(vr_debug_max_handvelmag_timeout > 0.f)
        {
            const auto frametime = cl.time - cl.oldtime;
            vr_debug_max_handvelmag_timeout -= frametime;
        }
        else
        {
            vr_debug_max_handvelmag = 0.f;
        }

        if(cl.handvelmag[index] > 3.f)
        {
            vr_debug_max_handvelmag_timeout = 2.5f;

            Con_Printf("handvelmag: %.2f (max: %.2f) (linearity: %.2f)\n",
                cl.handvelmag[index], vr_debug_max_handvelmag, linearity);
        }
    }
}

void debugPrintHeadvel()
{
    if(!vr_debug_print_headvel.value)
    {
        return;
    }

    const auto len = glm::length(cl.headvel);
    if(len < 2.f)
    {
        return;
    }

    Con_Printf("headvel: {%.2f, %.2f, %.2f} | len: %.2f\n", cl.headvel[0],
        cl.headvel[1], cl.headvel[2], len);
}

[[nodiscard]] entity_t* VR_GetAnchorEntity(const HandIdx handIdx) noexcept
{
    return handIdx == cVR_MainHand ? &cl.viewent : &cl.offhand_viewent;
}

[[nodiscard]] bool VR_GetHorizFlip(const HandIdx handIdx) noexcept
{
    return handIdx == cVR_OffHand;
}

[[nodiscard]] qvec3 VR_CalcLocalWpnMuzzlePos(const int index) noexcept
{
    entity_t* const anchor = VR_GetAnchorEntity(index);

    if(anchor->model == nullptr)
    {
        return vec3_zero;
    }

    return VR_CalcFinalWpnMuzzlePos(index) - anchor->origin;
}

qvec3 VR_UpdateGunWallCollisions(edict_t* edict, const int handIndex,
    VrGunWallCollision& out, qvec3 resolvedHandPos) noexcept
{
    constexpr qvec3 handMins{-1.f, -1.f, -1.f};
    constexpr qvec3 handMaxs{1.f, 1.f, 1.f};

    // Local position of the gun's muzzle. Takes orientation into
    // account.
    const auto localMuzzlePos = VR_CalcLocalWpnMuzzlePos(handIndex);

    // World position of the gun's muzzle.
    const auto muzzlePos = resolvedHandPos + localMuzzlePos;

    // Check for collisions between the muzzle and geometry/entities.
    const trace_t gunTrace = SV_Move(
        resolvedHandPos, handMins, handMaxs, muzzlePos, MOVE_NORMAL, edict);

    // Position of the hand after resolving collisions with the gun
    // muzzle.
    const auto resolvedHandMuzzlePos = gunTrace.endpos - localMuzzlePos;

    if(hitSomething(gunTrace))
    {
        // TODO VR: (P2) haptics cancel each other
        // VR_DoHaptic(index, 0.f, 0.1f, 50, 1.f - gunTrace.fraction);

        out._colliding = true;
        out._ent = gunTrace.ent;

        for(int i = 0; i < 3; ++i)
        {
            out._normals[i] = gunTrace.plane.normal[i] != 0;
            resolvedHandPos[i] = resolvedHandMuzzlePos[i];
        }
    }
    else
    {
        out._colliding = false;
        out._ent = nullptr;
    }

    return resolvedHandPos;
}

[[nodiscard]] qvec3 VR_GetWorldHandPos(
    const int handIndex, const qvec3& playerOrigin) noexcept
{
    // Position of the hand relative to the head.
    const auto headLocalPreRot = controllers[handIndex].position - headOrigin;
    const auto headLocal =
        Vec3RotateZ(headLocalPreRot, VR_GetTurnYawAngle() * M_PI_DIV_180) +
        headOrigin;

    // Position of the hand in the game world, prior to any collision
    // detection or resolution.
    const qvec3 worldHandPos{
        -headLocal[0] + playerOrigin[0],               //
        -headLocal[1] + playerOrigin[1],               //
        headLocal[2] + VR_GetHandZOrigin(playerOrigin) //
    };

    return worldHandPos;
}

[[nodiscard]] qvec3 VR_GetResolvedHandPos(edict_t* edict,
    const qvec3& worldHandPos, const qvec3& adjPlayerOrigin) noexcept
{
    // Size of hand hitboxes.
    constexpr qvec3 mins{-1.f, -1.f, -1.f};
    constexpr qvec3 maxs{1.f, 1.f, 1.f};

    // Trace from upper torso to desired final location. `SV_Move` detects
    // entities as well, not just geometry.
    const trace_t trace =
        SV_Move(adjPlayerOrigin, mins, maxs, worldHandPos, MOVE_NORMAL, edict);

    // Compute final collision resolution position, starting from the
    // desired position and resolving only against the collision plane's
    // normal vector.
    if(!hitSomething(trace))
    {
        return worldHandPos;
    }

    // Resolve collision along trace normals.
    qvec3 res = worldHandPos;

    for(int i = 0; i < 3; ++i)
    {
        if(trace.plane.normal[i] != 0)
        {
            res[i] = trace.endpos[i];
        }
    }

    return res;
}

template <typename T>
[[nodiscard]] static qvec3 openVRCoordsToQuakeCoords(const T& v)
{
    return qvec3{-v[2], v[0], v[1]};
}

[[nodiscard]] static qvec3 redirectVectorByYaw(
    const qvec3& input, const qfloat exemplarYaw)
{
    return redirectVector(input, {0.f, exemplarYaw, 0.f});
}

void SetHandPos(int index, entity_t& player)
{
    // -----------------------------------------------------------------------
    // VR: Figure out position of hand controllers in the game world.

    const auto worldHandPos = VR_GetWorldHandPos(index, player.origin);

    // -----------------------------------------------------------------------
    // VR: Detect & resolve hand collisions against the world or entities.

    // Start around the upper torso, not actual center of the player.
    const qvec3 adjPlayerOrigin = VR_GetAdjustedPlayerOrigin(player.origin);

    // TODO VR: (P2) cvar to enable/disable muzzle collisions
    qvec3 finalVec = worldHandPos;

    // TODO VR: (P2) reintroduce as modifier
    // const float gunLength = index == 0 ? 0.f :
    // VR_GetWpnLength(VR_GetMainHandWpnCvarEntry());

    if(svPlayerActive())
    {
        edict_t* playerEdict = getPlayerEdict();

        const auto resolvedHandPos =
            VR_GetResolvedHandPos(playerEdict, worldHandPos, adjPlayerOrigin);

        finalVec = resolvedHandPos;

        finalVec = VR_UpdateGunWallCollisions(
            playerEdict, index, vr_gun_wall_collision[index], finalVec);
    }

    const auto oldHandpos = cl.handpos[index];

    const qvec3 lastPlayerTranslation =
        gotLastPlayerOrigin ? player.origin - lastPlayerOrigin : vec3_zero;

    // ------------------------------------------------------------------------
    // VR: Interpolate hand position depending on weapon weight.
    if(!inMenuOrConsole() && vr_wpn_pos_weight.value == 1)
    {
        const auto wpnCvarEntry = VR_GetWpnCvarEntry(index);

        const auto ftw =
            VR_CalcWeaponWeightFTAdjusted<VR_GetWeaponWeightPosFactor>(index) *
            VR_GetWpnCVarValue(wpnCvarEntry, WpnCVar::WeightPosMult);

        const auto rotate_point =
            [](const qvec2& center, const qfloat angle, qvec2 p)
        {
            // translate point back to origin:
            p -= center;

            // rotate point
            const qfloat s = std::sin(angle);
            const qfloat c = std::cos(angle);
            const qvec2 rotated{p.x * c - p.y * s, p.x * s + p.y * c};

            // translate point back
            return rotated + center;
        };

        const auto oldadjxy = rotate_point(
            player.origin.xy, glm::radians(-lastVrYawDiff), oldHandpos.xy);

        const qvec3 oldadj{oldadjxy[0], oldadjxy[1], oldHandpos[2]};

        const auto diffWithOld = oldHandpos - oldadj;

        const auto newPos = glm::mix(oldHandpos, finalVec, ftw);

        const auto diffWithNew = newPos - oldHandpos;

        // TODO VR: (P1) seems good now. Cvar everything

        cl.handpos[index] += diffWithNew +
                             (lastPlayerTranslation * (1.f - ftw)) -
                             (diffWithOld * (1.f - ftw));
    }
    else
    {
        cl.handpos[index] = finalVec;
    }

    // ------------------------------------------------------------------------
    // If hands get too far, bring them closer to the player.
    const auto currHandPos = cl.handpos[index];
    constexpr qfloat maxHandPlayerDiff = 50.f;
    const auto handPlayerDiff = currHandPos - adjPlayerOrigin;
    if(glm::length(handPlayerDiff) > maxHandPlayerDiff)
    {
        const auto dir = safeNormalize(handPlayerDiff);
        cl.handpos[index] = adjPlayerOrigin + dir * maxHandPlayerDiff;
    }

    // handrot is set with AngleVectorFromRotMat

    // handvel
    // Redirect controller velocity to world velocity (based on thumbstick yaw
    // turn angle).
    cl.handvel[index] = redirectVectorByYaw(
        openVRCoordsToQuakeCoords(controllers[index].velocity),
        VR_GetTurnYawAngle());

    // handavel
    cl.handavel[index] = controllers[index].a_velocity;

    // handthrowvel
    {
        cl.handthrowvel[index] = controllers[index].velocityHistory.average();

        // TODO VR: (P1) throwing an item up with a small flick feels too strong
        const auto up =
            std::get<2>(getAngledVectors(controllers[index].orientation));

        cl.handthrowvel[index] +=
            glm::cross(controllers[index].angularVelocityHistory.average(),
                up * vr_throw_up_center_of_mass.value);

        cl.handthrowvel[index] = redirectVectorByYaw(
            openVRCoordsToQuakeCoords(cl.handthrowvel[index]),
            VR_GetTurnYawAngle());
    }

    // When positional weight is enabled, scale the hand velocity and throw
    // velocity depending on the weight.
    if(vr_wpn_pos_weight.value == 1)
    {
        const auto wpnCvarEntry = VR_GetWpnCvarEntry(index);

        const auto weaponWeight =
            VR_CalcWeaponWeight<VR_GetWeaponWeightPosFactor>(index);

        const auto clampedWeight = std::clamp(weaponWeight, 0._qf, 0.4_qf);

        const auto handvelFactor =
            mapRange(clampedWeight, 0.0_qf, 0.4_qf, 0.55_qf, 1._qf) *
            VR_GetWpnCVarValue(wpnCvarEntry, WpnCVar::WeightHandVelMult);

        const auto handthrowvelFactor =
            mapRange(clampedWeight, 0.0_qf, 0.4_qf, 0.45_qf, 1._qf) *
            VR_GetWpnCVarValue(wpnCvarEntry, WpnCVar::WeightHandThrowVelMult);

        cl.handvel[index] *= std::pow(handvelFactor, 0.4f);
        cl.handthrowvel[index] *= std::pow(handthrowvelFactor, 0.8f);
    }

    // TODO VR: (P2) doesn't work due to collision resolution with wall, it
    // jumps up cl.handvel[index] = (cl.handpos[index] - lastPlayerTranslation)
    // - oldHandpos;

    if(vr_gun_wall_collision[index]._colliding)
    {
        for(int i = 0; i < 3; ++i)
        {
            if(vr_gun_wall_collision[index]._normals[i])
            {
                cl.handpos[index][i] = finalVec[i];
            }
        }
    }

    // handvelmag
    cl.handvelmag[index] = glm::length(cl.handvel[index]);

    const auto xdFwd =
        getFwdVecFromPitchYawRoll({0.f, VR_GetBodyYawAngle(), 0.f});

    const auto linearity =
        glm::dot(glm::normalize(cl.handvel[index]), glm::normalize(xdFwd));

    // VR: This helps direct punches (in line with the body) being registered.
    if(linearity > 0.f)
    {
        cl.handvelmag[index] *= std::pow(linearity, 4.f) * 2.f;
    }

    debugPrintHandvel(index, linearity);
}

[[nodiscard]] static bool VR_ClientEntitiesAvailable() noexcept
{
    return cl.entities != nullptr;
}

[[nodiscard]] static entity_t& VR_GetPlayerEntity() noexcept
{
    assert(VR_ClientEntitiesAvailable());
    return cl.entities[cl.viewentity];
}

// TODO VR: (P1) code repetition with r_alias
[[nodiscard]] qvec3 VR_GetAliasVertexOffsets(
    entity_t* const anchor, const int anchorVertex) noexcept
{
    const auto anchorHdr = (aliashdr_t*)Mod_Extradata(anchor->model);

    lerpdata_t zeroLerpdata;
    R_SetupAliasFrameZero(*anchorHdr, 0, &zeroLerpdata);

    lerpdata_t lerpdata;
    R_SetupAliasFrame(anchor, *anchorHdr, anchor->frame, &lerpdata);

    auto fd = getDrawAliasFrameData(*anchorHdr, lerpdata, zeroLerpdata);

    const int clampedAnchorVertex =
        std::clamp(anchorVertex, 0, anchorHdr->numverts);

    fd.verts1 += clampedAnchorVertex;
    fd.verts2 += clampedAnchorVertex;
    fd.zeroverts1 += clampedAnchorVertex;

    if(fd.lerping)
    {
        return getFinalVertexPosLerped(fd, anchor->zeroBlend);
    }

    return getFinalVertexPosNonLerped(fd, anchor->zeroBlend);
}

[[nodiscard]] qvec3 VR_GetScaledAliasVertexOffsets(entity_t* const anchor,
    const int anchorVertex, const qvec3& extraOffsets,
    const bool horizFlip) noexcept
{
    (void)extraOffsets;

    if(anchor->model == nullptr)
    {
        return vec3_zero;
    }

    const auto anchorHdr = (aliashdr_t*)Mod_Extradata(anchor->model);

    qvec3 result = VR_GetAliasVertexOffsets(anchor, anchorVertex);

    if(horizFlip)
    {
        result[1] *= -1.f;
    }

    // const auto scaleCorrect = VR_GetScaleCorrect();
    //
    // auto scale = anchorHdr->original_scale * scale * scaleCorrect;
    // auto scale_origin = anchorHdr->original_scale_origin + offsets;
    // scale_origin *= scaleCorrect;

    const auto offsets = anchorHdr->scale_origin;

    result *= anchorHdr->scale;

    // const auto scaleratio = anchorHdr->scale / anchorHdr->original_scale;

    result += offsets; // * scaleratio;
    // result += offsets * 3.f;

    // result[0] -=
    //     anchorHdr->scale_origin[0] - anchorHdr->original_scale_origin[0];
    // result[1] -=
    //     anchorHdr->scale_origin[1] - anchorHdr->original_scale_origin[1];
    // result[2] -=
    //     anchorHdr->scale_origin[2] - anchorHdr->original_scale_origin[2];


    //  result += extraOffsets;
    // result *= 0.f;

    return result;
}

[[nodiscard]] qvec3 VR_GetScaledAndAngledAliasVertexOffsets(
    entity_t* const anchor, const int anchorVertex, const qvec3& extraOffsets,
    const qvec3& rotation, const bool horizFlip) noexcept
{
    if(anchor == nullptr || anchor->model == nullptr)
    {
        return vec3_zero;
    }

    const auto finalVertexOffsets = VR_GetScaledAliasVertexOffsets(
        anchor, anchorVertex, extraOffsets, horizFlip);
    // const auto anchorHdr = (aliashdr_t*)Mod_Extradata(anchor->model);

    return quake::util::redirectVector(finalVertexOffsets, rotation);
}

[[nodiscard]] qvec3 VR_GetScaledAndAngledAliasVertexPosition(
    entity_t* const anchor, const int anchorVertex, const qvec3& extraOffsets,
    const qvec3& rotation, const bool horizFlip) noexcept
{
    (void)rotation;

    const auto anchorHdr = (aliashdr_t*)Mod_Extradata(anchor->model);
    qvec3 c = VR_GetAliasVertexOffsets(anchor, anchorVertex);

    qmat4 trMat = glm::translate(anchor->origin);

    trMat *= glm::rotate(glm::radians(anchor->angles[YAW]), qvec3{0, 0, 1});
    trMat *= glm::rotate(glm::radians(-anchor->angles[PITCH]), qvec3{0, 1, 0});
    trMat *= glm::rotate(glm::radians(anchor->angles[ROLL]), qvec3{1, 0, 0});

    if(horizFlip)
    {
        trMat *= glm::scale(qvec3{1._qf, -1._qf, 1._qf});
    }

    trMat *= glm::translate(extraOffsets);
    trMat *= glm::translate(anchorHdr->scale_origin);
    trMat *= glm::scale(anchorHdr->scale);

    return trMat * qvec4(c, 1.f);
}

[[nodiscard]] qvec3 VR_GetWpnFixed2HFinalPosition(entity_t* const anchor,
    const int cvarEntry, const bool horizFlip, const qvec3& extraOffset,
    const qvec3& handRot) noexcept
{
    if(anchor == nullptr || anchor->model == nullptr)
    {
        return vec3_zero;
    }

    const qvec3 extraOffsets = VR_GetWpnFixed2HOffsets(cvarEntry) + extraOffset;

    const auto anchorVertex = static_cast<VertexIdx>(
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::TwoHHandAnchorVertex));

    return VR_GetScaledAndAngledAliasVertexPosition(
        anchor, anchorVertex, extraOffsets, handRot, horizFlip);
}

[[nodiscard]] qvec3 VR_GetBodyAnchor(const qvec3& offsets) noexcept
{
    if(!VR_ClientEntitiesAvailable())
    {
        return vec3_zero;
    }

    const auto heightRatio = std::clamp(VR_GetCrouchRatio(), 0._qf, 0.8_qf);

    const auto [vFwd, vRight, vUp] =
        getAngledVectors({heightRatio * -35._qf, VR_GetBodyYawAngle(), 0._qf});

    const auto& [ox, oy, oz] = offsets;

    auto origin = VR_GetPlayerEntity().origin;
    origin[2] += 2._qf;
    origin[2] -= VR_GetCrouchRatio() * 18._qf;

    return origin + vRight * oy + vFwd * ox +
           vUp * qfloat(vr_height_calibration.value) * oz;
}

// Relative to player.
[[nodiscard]] static qvec3 VR_GetShoulderOffsets() noexcept
{
    return {
        vr_shoulder_offset_x.value, //
        vr_shoulder_offset_y.value, //
        vr_shoulder_offset_z.value  //
    };
}

// Relative to shoulder offset.
[[nodiscard]] static qvec3 VR_GetShoulderHolsterOffsets() noexcept
{
    return {
        vr_shoulder_holster_offset_x.value, //
        vr_shoulder_holster_offset_y.value, //
        vr_shoulder_holster_offset_z.value  //
    };
}

// Relative to player.
[[nodiscard]] static qvec3 VR_GetHipOffsets() noexcept
{
    return {
        vr_hip_offset_x.value, //
        vr_hip_offset_y.value, //
        vr_hip_offset_z.value  //
    };
}

// Relative to player.
[[nodiscard]] static qvec3 VR_GetUpperOffsets() noexcept
{
    return {
        vr_upper_holster_offset_x.value, //
        vr_upper_holster_offset_y.value, //
        vr_upper_holster_offset_z.value  //
    };
}

[[nodiscard]] static qvec3 VR_NegateY(qvec3 v) noexcept
{
    v.y = -v.y;
    return v;
}


[[nodiscard]] static qvec3 VR_GetLeftShoulderStockPos() noexcept
{
    return VR_GetBodyAnchor(VR_NegateY(VR_GetShoulderOffsets()));
}

[[nodiscard]] static qvec3 VR_GetRightShoulderStockPos() noexcept
{
    return VR_GetBodyAnchor(VR_GetShoulderOffsets());
}

[[nodiscard]] qvec3 VR_GetShoulderStockPos(
    const int holdingHand, const int helpingHand) noexcept
{
    (void)helpingHand;

    if(holdingHand == cVR_OffHand)
    {
        return VR_GetLeftShoulderStockPos();
    }

    assert(holdingHand == cVR_MainHand);
    return VR_GetRightShoulderStockPos();
}

[[nodiscard]] qvec3 VR_GetHolsterXCrouchAdjustment(const float mult) noexcept
{
    // TODO VR: (P2) abstract this, used in view as well
    const auto heightRatio =
        std::clamp(VR_GetCrouchRatio() - 0.2_qf, 0._qf, 0.6_qf);
    const auto crouchXOffset = heightRatio * mult;

    const auto vFwd =
        getFwdVecFromPitchYawRoll({0.f, VR_GetBodyYawAngle(), 0.f});

    return vFwd * crouchXOffset;
}


[[nodiscard]] qvec3 VR_GetShoulderHolsterCrouchAdjustment() noexcept
{
    // TODO VR: (P2) cvar + menu entry
    return VR_GetHolsterXCrouchAdjustment(1.5f);
}

[[nodiscard]] qvec3 VR_GetLeftShoulderHolsterPos() noexcept
{
    const auto shoulderPos = VR_NegateY(VR_GetShoulderOffsets());
    const auto holsterOff = VR_NegateY(VR_GetShoulderHolsterOffsets());

    return VR_GetBodyAnchor(shoulderPos + holsterOff) +
           VR_GetShoulderHolsterCrouchAdjustment();
}

[[nodiscard]] qvec3 VR_GetRightShoulderHolsterPos() noexcept
{
    const auto shoulderPos = VR_GetShoulderOffsets();
    const auto holsterOff = VR_GetShoulderHolsterOffsets();

    return VR_GetBodyAnchor(shoulderPos + holsterOff) +
           VR_GetShoulderHolsterCrouchAdjustment();
}


[[nodiscard]] qvec3 VR_GetHipHolsterCrouchAdjustment() noexcept
{
    // TODO VR: (P2) cvar + menu entry
    return VR_GetHolsterXCrouchAdjustment(-9.5f);
}

[[nodiscard]] qvec3 VR_GetLeftHipPos() noexcept
{
    return VR_GetBodyAnchor(VR_NegateY(VR_GetHipOffsets())) +
           VR_GetHipHolsterCrouchAdjustment();
}

[[nodiscard]] qvec3 VR_GetRightHipPos() noexcept
{
    return VR_GetBodyAnchor(VR_GetHipOffsets()) +
           VR_GetHipHolsterCrouchAdjustment();
}

[[nodiscard]] qvec3 VR_GetUpperHolsterCrouchAdjustment() noexcept
{
    // TODO VR: (P2) cvar + menu entry
    return VR_GetHolsterXCrouchAdjustment(-1.5f);
}

[[nodiscard]] qvec3 VR_GetLeftUpperPos() noexcept
{
    return VR_GetBodyAnchor(VR_NegateY(VR_GetUpperOffsets())) +
           VR_GetUpperHolsterCrouchAdjustment();
}

[[nodiscard]] qvec3 VR_GetRightUpperPos() noexcept
{
    return VR_GetBodyAnchor(VR_GetUpperOffsets()) +
           VR_GetUpperHolsterCrouchAdjustment();
}

[[nodiscard]] qfloat VR_GetTurnYawAngle() noexcept
{
    return vrYaw;
}

[[nodiscard]] static qvec3 VR_GetEyesOrientation() noexcept
{
    return QuatToYawPitchRoll(eyes[0].orientation);
}

[[nodiscard]] qvec3 VR_GetHeadAngles() noexcept
{
    return cl.viewangles;
}

[[nodiscard]] qfloat VR_GetHeadYawAngle() noexcept
{
    return VR_GetHeadAngles()[YAW];
}

[[nodiscard]] static auto VR_GetHeadDirs() noexcept
{
    return getAngledVectors(VR_GetHeadAngles());
}

[[nodiscard]] static auto VR_GetHeadYawDirs() noexcept
{
    return getAngledVectors({0.f, VR_GetHeadYawAngle(), 0.f});
}

[[nodiscard]] static float VR_GetHeadFwdAngleBlended() noexcept
{
    const auto [eyePitch, eyeYaw, eyeRoll] = VR_GetEyesOrientation();
    const auto [headFwd, headRight, headUp] = VR_GetHeadDirs();

    const auto isBetween = [](const auto x, const auto min, const auto max)
    { return x >= min && x <= max; };

    if(isBetween(eyePitch, -50.f, 50.f))
    {
        return eyeYaw;
    }

    if(eyePitch > 0.f)
    {
        const float factor = eyePitch / 90.f;
        const auto dir = glm::mix(headFwd, headUp, factor);

        return pitchYawRollFromDirectionVector({0.f, 0.f, 1.f}, dir)[YAW];
    }

    const float factor = -eyePitch / 90.f;
    const auto dir = glm::mix(headFwd, -headUp, factor);

    return pitchYawRollFromDirectionVector({0.f, 0.f, 1.f}, dir)[YAW];
}

[[nodiscard]] static auto VR_GetBodyYawAdjPlayerOrigins(
    const qvec3& headFwdDir, const qvec3& headRightDir) noexcept
{
    if(!VR_ClientEntitiesAvailable())
    {
        return std::tuple{vec3_zero, vec3_zero, vec3_zero};
    }

    const auto adjPlayerOrigin =
        VR_GetPlayerEntity().origin - headFwdDir * 10._qf;
    const auto adjPlayerOriginLeft = adjPlayerOrigin - headRightDir * 6.5_qf;
    const auto adjPlayerOriginRight = adjPlayerOrigin + headRightDir * 6.5_qf;

    return std::tuple{
        adjPlayerOrigin, adjPlayerOriginLeft, adjPlayerOriginRight};
}

[[nodiscard]] static auto VR_GetFixedZHeadHandDiffs(
    const qvec3& adjPlayerOriginLeft,
    const qvec3& adjPlayerOriginRight) noexcept
{
    const auto fixZ = [&](qvec3 v)
    {
        v[2] = adjPlayerOriginLeft[2];
        return v;
    };

    return std::array{
        fixZ(cl.handpos[0]) - adjPlayerOriginLeft,  //
        fixZ(cl.handpos[1]) - adjPlayerOriginRight, //
    };
}

[[nodiscard]] static qvec3 VR_GetBodyYawMixHandDir(
    const std::array<qvec3, 2>& headHandDiffs, const qvec3& headFwdDir) noexcept
{
    auto result = glm::mix(headHandDiffs[0], headHandDiffs[1], 0.5f);
    result /= 10.f;

    if(glm::dot(result, headFwdDir) < 0.f)
    {
        // result *= -glm::dot(result, headDir);

        if(glm::length(result) > 0.1f)
        {
            result /= glm::length(result);
            result *= 0.1f;
        }
    }

    return result;
}

[[nodiscard]] static qvec3 VR_GetBodyYawMixFinalDir(
    const qvec3& headFwdDir, const qvec3& mixHandDir) noexcept
{
    return glm::normalize(glm::mix(headFwdDir, mixHandDir, 0.8f));
}

[[nodiscard]] std::tuple<qvec3, qvec3, qvec3, qvec3, qvec3, qvec3, qvec3, qvec3>
VR_GetBodyYawAngleCalculations() noexcept
{
    // const auto [headFwdDir, headRightDir, headUpDir] = VR_GetHeadYawDirs();
    const auto [headFwdDir, headRightDir, headUpDir] =
        getAngledVectors({0.f, VR_GetHeadFwdAngleBlended(), 0.f});

    const auto [adjPlayerOrigin, adjPlayerOriginLeft, adjPlayerOriginRight] =
        VR_GetBodyYawAdjPlayerOrigins(headFwdDir, headRightDir);

    const auto headHandDiffs =
        VR_GetFixedZHeadHandDiffs(adjPlayerOriginLeft, adjPlayerOriginRight);

    const auto mixHandDir = VR_GetBodyYawMixHandDir(headHandDiffs, headFwdDir);
    const auto mixFinalDir = VR_GetBodyYawMixFinalDir(headFwdDir, mixHandDir);

    return std::tuple{adjPlayerOrigin, adjPlayerOriginLeft,
        adjPlayerOriginRight, headFwdDir, headRightDir, headUpDir, mixHandDir,
        mixFinalDir};
}

[[nodiscard]] qfloat VR_GetBodyYawAngle() noexcept
{
    if(!controllers[0].active || !controllers[1].active)
    {
        // If any controller is off or player is inactive, return head yaw.
        return VR_GetHeadFwdAngleBlended();
    }

    const auto [adjPlayerOrigin, adjPlayerOriginLeft, adjPlayerOriginRight,
        headFwdDir, headRightDir, headUpDir, mixHandDir, mixFinalDir] =
        VR_GetBodyYawAngleCalculations();

    return quake::util::pitchYawRollFromDirectionVector(
        headUpDir, mixFinalDir)[YAW];
}


[[nodiscard]] qvec3 VR_Get2HVirtualStockMix(
    const qvec3& viaHand, const qvec3& viaShoulder) noexcept
{
    return glm::mix(viaHand, viaShoulder, vr_2h_virtual_stock_factor.value);
}

[[nodiscard]] qvec3 VR_Get2HHoldingHandPos(
    const int holdingHand, const int helpingHand) noexcept
{
    (void)helpingHand;
    return cl.handpos[holdingHand];
}

[[nodiscard]] qvec3 VR_Get2HHelpingHandPos(
    const int holdingHand, const int helpingHand) noexcept
{
    const auto [thox, thoy, thoz] =
        VR_GetWpn2HOffsets(VR_GetWpnCvarEntry(holdingHand));

    const auto [forward, right, up] = getAngledVectors(cl.handrot[holdingHand]);

    // Mirror Y axis when holding with the off-hand.
    const auto mirroredRight = helpingHand == cVR_OffHand ? right : -right;

    // TODO VR: (P2) maybe add off hand offset here if holding hand is offhand?
    // test?
    return cl.handpos[helpingHand] + forward * thox + mirroredRight * thoy +
           up * thoz;
}

[[nodiscard]] bool VR_InStockDistance(const int holdingHand,
    const int helpingHand, const qvec3& shoulderPos) noexcept
{
    (void)helpingHand;

    return glm::distance(shoulderPos, cl.handpos[holdingHand]) <
           vr_virtual_stock_thresh.value;
}

static void VR_DoTeleportation()
{
    if(!vr_teleport_enabled.value || !svPlayerActive() ||
        !VR_ClientEntitiesAvailable())
    {
        return;
    }

    entity_t& player = VR_GetPlayerEntity();

    if(vr_teleporting)
    {
        constexpr float oh = 6.f;
        constexpr float oy = 12.f;
        const qvec3 mins{-oh, -oh, -oy};
        const qvec3 maxs{oh, oh, oy};

        const auto fwd = getFwdVecFromPitchYawRoll(cl.handrot[cVR_OffHand]);
        const auto target =
            cl.handpos[cVR_OffHand] + qfloat(vr_teleport_range.value) * fwd;

        const auto adjPlayerOrigin = VR_GetAdjustedPlayerOrigin(player.origin);

        const trace_t trace = SV_Move(
            adjPlayerOrigin, mins, maxs, target, MOVE_NORMAL, getPlayerEdict());

        const auto between =
            [](const float value, const float min, const float max)
        { return value >= min && value <= max; };

        // Allow slopes, but not walls or ceilings.
        const bool goodNormal = between(trace.plane.normal[2], 0.75f, 1.f);

        vr_teleporting_impact_valid = hitSomething(trace) && goodNormal;
        vr_teleporting_impact = trace.endpos;
        vr_teleporting_impact[2] += 12.f; // Compensate for player height.

        if(vr_teleporting_impact_valid && inGame())
        {
            R_RunParticle2Effect(trace.endpos, vec3_zero, 7, 2);
        }
    }
    else if(vr_was_teleporting && vr_teleporting_impact_valid)
    {
        vr_send_teleport_msg = true;
        player.origin = vr_teleporting_impact;
    }

    vr_was_teleporting = vr_teleporting;
}

#ifndef WIN32
// TODO VR: (P2) linux hack
__attribute__((no_sanitize_address))
#endif
static void
VR_UpdateDevicesOrientationPosition() noexcept
{
    if(isDedicated || (vr_fakevr.value && vr_novrinit.value))
    {
        return;
    }

    controllers[0].active = false;
    controllers[1].active = false;

    // Update poses
    const auto rc = vr::VRCompositor()->WaitGetPoses(
        ovr_DevicePose, vr::k_unMaxTrackedDeviceCount, nullptr, 0);

    if(rc != vr::EVRCompositorError::VRCompositorError_None)
    {
        Con_Printf("Failed to wait for Steam VR poses, rc = %d\n", (int)rc);
        return;
    }

    const auto turnYaw = VR_GetTurnYawAngle();

    for(uint32_t iDevice = 0; iDevice < vr::k_unMaxTrackedDeviceCount;
        iDevice++)
    {
        // HMD vectors update
        if(ovr_DevicePose[iDevice].bPoseIsValid &&
            ovrHMD->GetTrackedDeviceClass(iDevice) ==
                vr::TrackedDeviceClass_HMD)
        {
            headVelocity = ovr_DevicePose[iDevice].vVelocity;

            vr::HmdVector3_t headPos = Matrix34ToVector(
                ovr_DevicePose[iDevice].mDeviceToAbsoluteTracking);
            headOrigin = {headPos.v[2], headPos.v[0], headPos.v[1]};

            // TODO VR: (P2) this should use the player's appoximated body
            // origin instead of the head origin, taking controllers into
            // account. See comment below for more info.
            qvec3 moveInTracking = headOrigin - lastHeadOrigin;
            moveInTracking[0] *= -meters_to_units;
            moveInTracking[1] *= -meters_to_units;
            moveInTracking[2] = 0;

            vr_roomscale_move =
                Vec3RotateZ(moveInTracking, turnYaw * M_PI_DIV_180);

            // ----------------------------------------------------------------
            // VR: Scale room-scale movement scaling for easier dodging and
            // to improve teleportation-based gameplay experience.
            vr_roomscale_move *= vr_roomscale_move_mult.value;
            // ----------------------------------------------------------------

            lastHeadOrigin = headOrigin;
            headOrigin -= lastHeadOrigin;

            // TODO VR: (P2) these two lines are what keep the head position
            // stable (attached to the player, instead of to the hmd). Should
            // add some leeway for neck length, so player can look at their body
            // without moving
            headPos.v[0] = 0;
            headPos.v[2] = 0;

            vr::HmdQuaternion_t headQuat = Matrix34ToQuaternion(
                ovr_DevicePose[iDevice].mDeviceToAbsoluteTracking);
            vr::HmdVector3_t leyePos =
                Matrix34ToVector(ovrHMD->GetEyeToHeadTransform(eyes[0].eye));
            vr::HmdVector3_t reyePos =
                Matrix34ToVector(ovrHMD->GetEyeToHeadTransform(eyes[1].eye));

            leyePos = RotateVectorByQuaternion(leyePos, headQuat);
            reyePos = RotateVectorByQuaternion(reyePos, headQuat);

            HmdVec3RotateY(headPos, -turnYaw * M_PI_DIV_180);

            HmdVec3RotateY(leyePos, -turnYaw * M_PI_DIV_180);
            HmdVec3RotateY(reyePos, -turnYaw * M_PI_DIV_180);

            eyes[0].position = AddVectors(headPos, leyePos);
            eyes[1].position = AddVectors(headPos, reyePos);
            eyes[0].orientation = headQuat;
            eyes[1].orientation = headQuat;
        }
        // Controller vectors update
        else if(ovr_DevicePose[iDevice].bPoseIsValid &&
                ovrHMD->GetTrackedDeviceClass(iDevice) ==
                    vr::TrackedDeviceClass_Controller)
        {
            vr::HmdVector3_t rawControllerPos = Matrix34ToVector(
                ovr_DevicePose[iDevice].mDeviceToAbsoluteTracking);
            vr::HmdQuaternion_t rawControllerQuat = Matrix34ToQuaternion(
                ovr_DevicePose[iDevice].mDeviceToAbsoluteTracking);
            const vr::HmdVector3_t& rawControllerVel =
                ovr_DevicePose[iDevice].vVelocity;
            const vr::HmdVector3_t& rawControllerAVel =
                ovr_DevicePose[iDevice].vAngularVelocity;

            int controllerIndex = -1;

            if(ovrHMD->GetControllerRoleForTrackedDeviceIndex(iDevice) ==
                vr::TrackedControllerRole_LeftHand)
            {
                // Swap controller values for our southpaw players
                controllerIndex = vr_lefthanded.value ? 1 : 0;
            }
            else if(ovrHMD->GetControllerRoleForTrackedDeviceIndex(iDevice) ==
                    vr::TrackedControllerRole_RightHand)
            {
                // Swap controller values for our southpaw players
                controllerIndex = vr_lefthanded.value ? 0 : 1;
            }

            if(controllerIndex != -1)
            {
                vr_controller* controller = &controllers[controllerIndex];
                controller->active = true;

                controller->lastState = controller->state;
                vr::VRSystem()->GetControllerState(
                    iDevice, &controller->state, sizeof(controller->state));
                controller->rawvector = rawControllerPos;
                controller->raworientation = rawControllerQuat;
                controller->position[0] =
                    (rawControllerPos.v[2] - lastHeadOrigin[0]) *
                    meters_to_units;
                controller->position[1] =
                    (rawControllerPos.v[0] - lastHeadOrigin[1]) *
                    meters_to_units;
                controller->position[2] =
                    (rawControllerPos.v[1]) * meters_to_units;

                controller->velocity[0] = rawControllerVel.v[0];
                controller->velocity[1] = rawControllerVel.v[1];
                controller->velocity[2] = rawControllerVel.v[2];
                controller->velocityHistory.add(controller->velocity);

                controller->a_velocity[0] = rawControllerAVel.v[0];
                controller->a_velocity[1] = rawControllerAVel.v[1];
                controller->a_velocity[2] = rawControllerAVel.v[2];
                controller->angularVelocityHistory.add(controller->a_velocity);

                controller->orientation = QuatToYawPitchRoll(rawControllerQuat);
            }
        }
    }
}

static void VR_DoWeaponDirSlerp()
{
    if(inMenuOrConsole() || vr_wpn_dir_weight.value != 1)
    {
        return;
    }

    for(int index = 0; index < 2; ++index)
    {
        const auto [oldFwd, oldRight, oldUp] =
            getAngledVectors(cl.prevhandrot[index]);
        const auto [newFwd, newRight, newUp] =
            getAngledVectors(cl.handrot[index]);

        const auto nOldFwd = safeNormalize(oldFwd);
        const auto nOldUp = safeNormalize(oldUp);
        const auto nNewFwd = safeNormalize(newFwd);
        const auto nNewUp = safeNormalize(newUp);

        const auto wpnCvarEntry = VR_GetWpnCvarEntry(index);

        const auto ftw =
            VR_CalcWeaponWeightFTAdjusted<VR_GetWeaponWeightDirFactor>(index) *
            VR_GetWpnCVarValue(wpnCvarEntry, WpnCVar::WeightDirMult);

        const auto slerpFwd = glm::slerp(nOldFwd, nNewFwd, ftw);
        const auto slerpUp = glm::slerp(nOldUp, nNewUp, ftw);

        const auto anyNan = [](const qvec3& v)
        { return std::isnan(v[0]) || std::isnan(v[1]) || std::isnan(v[2]); };

        const auto mixFwd = anyNan(slerpFwd) ? nNewFwd : slerpFwd;
        const auto mixUp = anyNan(slerpUp) ? nNewUp : slerpUp;

        const auto [p, y, r] = pitchYawRollFromDirectionVector(mixUp, mixFwd);

        const auto yawDiff = lastVrYawDiff;

        // VR_GetHeadYawAngle() - lastPlayerHeadYaw;
        // if(yawDiff != 0.f)
        //     Con_Printf(
        //         "yawDiff: %.2f | withftw: %2.f \n", yawDiff, yawDiff *
        //         (1.f - ftw));

        cl.handrot[index][PITCH] = p;
        cl.handrot[index][YAW] = y - (yawDiff * (1.f - ftw));
        cl.handrot[index][ROLL] = r;
    }
}

static void VR_DoUpdatePrevAnglesAndPlayerYaw()
{
    lastPlayerHeadYaw = VR_GetHeadYawAngle();
    cl.prevhandrot[cVR_OffHand] = cl.handrot[cVR_OffHand];
    cl.prevhandrot[cVR_MainHand] = cl.handrot[cVR_MainHand];
    cl.aimangles = cl.handrot[cVR_MainHand]; // Sets the shooting angle

    // Con_Printf("rt: %.2f, %.2f, %.2f\n", cl.handrot[cVR_MainHand][0],
    // cl.handrot[cVR_MainHand][1],
    //     cl.handrot[cVR_MainHand][2]);
    // Con_Printf("av: %.2f, %.2f, %.2f\n", cl.handavel[1][0],
    // cl.handavel[1][1],
    //    cl.handavel[1][2]);
    //
    // const auto [f, r, u] = getAngledVectors(cl.handrot[cVR_MainHand]);
    //
    // Con_Printf("rf: %.2f, %.2f, %.2f\n", f[0], f[1], f[2]);
    // Con_Printf("rr: %.2f, %.2f, %.2f\n", r[0], r[1], r[2]);
    // Con_Printf("ru: %.2f, %.2f, %.2f\n", u[0], u[1], u[2]);
}

static bool VR_GoodDistanceForDynamic2HGrabImpl(
    const qvec3& holdingHandPos, const qvec3& helpingHandPos)
{
    // TODO VR: (P1) weapon cvar for this!
    const auto handDist = glm::distance(holdingHandPos, helpingHandPos);
    const bool goodDistance = handDist > 5.f && handDist < 25.f;

    return goodDistance;
}

static bool VR_GoodDistanceFor2HGrab(
    const int holdingHand, const int helpingHand)
{
    // TODO VR: (P1) repetition with view.cpp

    const auto wpnCvarEntry = VR_GetWpnCvarEntry(holdingHand);

    const bool twoHDisplayModeFixed =
        quake::util::cvarToEnum<Wpn2HDisplayMode>(VR_GetWpnCVar(
            wpnCvarEntry, WpnCVar::TwoHDisplayMode)) == Wpn2HDisplayMode::Fixed;

    if(!twoHDisplayModeFixed)
    {
        return VR_GoodDistanceForDynamic2HGrabImpl(
            VR_Get2HHoldingHandPos(holdingHand, helpingHand),
            VR_Get2HHelpingHandPos(holdingHand, helpingHand));
    }

    const bool horizFlip = VR_GetHorizFlip(holdingHand);

    entity_t* const anchor = VR_GetAnchorEntity(holdingHand);

    const qvec3 pos = VR_GetWpnFixed2HFinalPosition(
        anchor, wpnCvarEntry, horizFlip, vec3_zero, cl.handrot[holdingHand]);

    const bool alreadyAiming = vr_should_aim_2h[holdingHand];
    const float threshold = alreadyAiming ? 20.f : 5.5f;

    return glm::distance(cl.handpos[helpingHand], pos) < threshold;
}

static bool VR_GoodDistanceForMainHand2HGrab()
{
    return VR_GoodDistanceFor2HGrab(cVR_MainHand, cVR_OffHand);
}

static bool VR_GoodDistanceForOffHand2HGrab()
{
    return VR_GoodDistanceFor2HGrab(cVR_OffHand, cVR_MainHand);
}

static bool VR_GoodDistanceForHandSwitch(const qvec3& a, const qvec3& b)
{
    return glm::distance(a, b) < 5.f;
}

[[nodiscard]] qvec3 VR_CalcFinalWpnMuzzlePos(const int index) noexcept
{
    entity_t* const anchor = VR_GetAnchorEntity(index);

    if(anchor->model == nullptr)
    {
        return vec3_zero;
    }

    const WpnCvarEntry anchorWpnCvar = VR_GetWpnCvarEntry(index);

    const auto anchorVertex = static_cast<VertexIdx>(
        VR_GetWpnCVarValue(anchorWpnCvar, WpnCVar::MuzzleAnchorVertex));

    const bool horizFlip = VR_GetHorizFlip(index);

    return VR_GetScaledAndAngledAliasVertexPosition(anchor, anchorVertex,
        VR_GetWpnMuzzleOffsets(anchorWpnCvar), cl.handrot[index], horizFlip);
}


static void VR_Do2HAimingImpl(Vr2HMode vr2HMode, const qvec3 (&originalRots)[2],
    const int holdingHand, const int helpingHand, const qvec3& shoulderStockPos)
{
    const auto helpingHandPos =
        VR_Get2HHelpingHandPos(holdingHand, helpingHand);

    const auto holdingHandPos =
        VR_Get2HHoldingHandPos(holdingHand, helpingHand);

    const auto handDiff = helpingHandPos - holdingHandPos;
    const auto handDir = safeNormalize(handDiff);

    const auto shoulderDiff = helpingHandPos - shoulderStockPos;

    const auto averageDiff = VR_Get2HVirtualStockMix(handDiff, shoulderDiff);
    const auto averageDir = safeNormalize(averageDiff);

    const auto [origDir, right, up] =
        getAngledVectors(originalRots[holdingHand]);

    const auto diffDot = glm::dot(handDir, origDir);

    const bool goodDistanceOrAlreadyAiming =
        VR_GoodDistanceFor2HGrab(holdingHand, helpingHand);

    const auto frametime = cl.time - cl.oldtime;

    const auto transitionVar =
        [&frametime](float& var, const bool predicate, const float speed)
    {
        var += frametime * (predicate ? speed : -speed);
        var = std::clamp(var, 0.f, 1.f);
    };

    const auto wpn2HMode = VR_GetWpn2HMode(VR_GetWpnCvarEntry(holdingHand));
    const bool useStock =
        VR_InStockDistance(holdingHand, helpingHand, shoulderStockPos) &&
        vr2HMode == Vr2HMode::VirtualStock &&
        wpn2HMode != Wpn2HMode::NoVirtualStock;

    transitionVar(vr_2h_aim_stock_transition[holdingHand], useStock, 5.f);

    const bool helpingHandGrabbing =
        helpingHand == cVR_OffHand ? vr_left_grabbing : vr_right_grabbing;

    const auto helpingHandStatIdx =
        helpingHand == cVR_OffHand ? STAT_OFFHAND_WID : STAT_MAINHAND_WID;

    const bool helpingHandIsFist = cl.stats[helpingHandStatIdx] == WID_FIST;

    const bool beforeMuzzle = [&]
    {
        const auto muzzlePos = VR_CalcFinalWpnMuzzlePos(holdingHand);

        const auto gunLength = glm::distance(holdingHandPos, muzzlePos);

        // Bias is needed because muzzle now moves with the gun animation.
        const auto bias = 7.5_qf;

        return glm::distance(holdingHandPos, helpingHandPos) <=
               gunLength + bias;
    }();

    const bool canGrabWith2H = helpingHandGrabbing &&
                               wpn2HMode != Wpn2HMode::Forbidden &&
                               !vr_gun_wall_collision[holdingHand]._colliding &&
                               helpingHandIsFist && beforeMuzzle;

    const bool goodDot = (diffDot > vr_2h_angle_threshold.value) ||
                         vr_2h_disable_angle_threshold.value;

    vr_should_aim_2h[holdingHand] =
        canGrabWith2H && goodDistanceOrAlreadyAiming && goodDot;

    vr_active_2h_helping_hand[helpingHand] = vr_should_aim_2h[holdingHand];

    transitionVar(
        vr_2h_aim_transition[holdingHand], vr_should_aim_2h[holdingHand], 5.f);

    const auto mixStockDir =
        glm::mix(handDir, averageDir, vr_2h_aim_stock_transition[holdingHand]);

    const auto mixDir =
        glm::mix(origDir, mixStockDir, vr_2h_aim_transition[holdingHand]);

    const auto [pitch, yaw, roll] = pitchYawRollFromDirectionVector(up, mixDir);

    auto [oP, oY, oR] =
        VR_GetWpn2HAngleOffsets(VR_GetWpnCvarEntry(holdingHand));

    const bool horizFlip = VR_GetHorizFlip(holdingHand);

    if(horizFlip)
    {
        oY *= -1;
        oR *= -1;
    }

    cl.handrot[holdingHand][PITCH] =
        pitch + oP * vr_2h_aim_transition[holdingHand];
    cl.handrot[holdingHand][YAW] = yaw + oY * vr_2h_aim_transition[holdingHand];
    cl.handrot[holdingHand][ROLL] =
        roll + oR * vr_2h_aim_transition[holdingHand];
}

static void VR_Do2HAiming(const qvec3 (&originalRots)[2])
{
    const auto vr2HMode = cvarToEnum<Vr2HMode>(vr_2h_mode);

    if(vr2HMode == Vr2HMode::Disabled)
    {
        return;
    }

    const bool bothControllersActive =
        controllers[0].active && controllers[1].active;

    if(!bothControllersActive)
    {
        return;
    }

    VR_Do2HAimingImpl(vr2HMode, originalRots, cVR_MainHand, cVR_OffHand,
        VR_GetRightShoulderStockPos());

    VR_Do2HAimingImpl(vr2HMode, originalRots, cVR_OffHand, cVR_MainHand,
        VR_GetLeftShoulderStockPos());
}

static void VR_FakeVRControllerAiming()
{
    if(!VR_ClientEntitiesAvailable())
    {
        return;
    }

    const auto [vfwd, vright, vup] = getAngledVectors(cl.viewangles);
    const auto& playerOrigin = VR_GetPlayerEntity().origin;

    vrYaw = cl.viewangles[YAW];

    cl.handpos[cVR_MainHand] =
        playerOrigin + vfwd * 16.5_qf + vright * 5.5_qf + vup * 15.5_qf;

    cl.handpos[cVR_OffHand] =
        playerOrigin + vfwd * 16.5_qf - vright * 5.5_qf + vup * 15.5_qf;

    cl.handrot[cVR_MainHand] = cl.viewangles;
    cl.handrot[cVR_OffHand] = cl.viewangles;
    cl.handrot[cVR_MainHand][ROLL] = vr_fakevr_handroll.value;
    cl.handrot[cVR_OffHand][ROLL] = vr_fakevr_handroll.value;
}

static void VR_DoWpnButton()
{
    const auto doWpnButton =
        [](const int handIdx, const entity_t& wpnButtonEntity, const char key)
    {
        const bool hasWpnButton = !wpnButtonEntity.hidden;

        WpnButtonState& state = vr_wpnbutton_state[handIdx];

        if(hasWpnButton && (cl.time - state._lastClTime) > 0.2f)
        {
            // TODO VR: (P1) improve this collision detection
            const auto otherHandIdx = VR_OtherHand(handIdx);

            const auto handPos = cl.handpos[otherHandIdx];

            const auto [fwd, right, up] =
                getAngledVectors(cl.handrot[otherHandIdx]);

            const auto adjHandPos = handPos + fwd * 2._qf + up * -2.5_qf;

            const auto dist = glm::distance(adjHandPos, wpnButtonEntity.origin);

            state._prevHover = state._hover;
            state._hover = dist < 2.7_qf;
            state._lastClTime = cl.time;

            const bool risingEdge = !state._prevHover && state._hover;
            Key_Event(key, risingEdge);
        }
    };

    doWpnButton(cVR_OffHand, cl.offhand_wpn_button, '7');
    doWpnButton(cVR_MainHand, cl.mainhand_wpn_button, '8');
}

static void VR_ControllerAiming(const qvec3& orientation)
{
    if(!VR_ClientEntitiesAvailable())
    {
        return;
    }

    // In fake VR mode, aim is controlled with the mouse.
    if(vr_fakevr.value == 0)
    {
        cl.viewangles[PITCH] = orientation[PITCH];
        cl.viewangles[YAW] = orientation[YAW];
    }

    qvec3 originalRots[2];

    for(int i = 0; i < 2; i++)
    {
        const auto rotOfs =
            i == 0 ? qvec3{vr_offhandpitch.value, vr_offhandyaw.value, 0.f}
                   : qvec3{vr_gunangle.value, vr_gunyaw.value, 0.f};

        const auto gunMatPitch = CreateRotMat(0, rotOfs[0]);
        const auto gunMatYaw = CreateRotMat(1, rotOfs[1]);
        const auto gunMatRoll = CreateRotMat(2, rotOfs[2]);

        auto mat = RotMatFromAngleVector(controllers[i].orientation);
        mat = R_ConcatRotations(gunMatRoll, mat);
        mat = R_ConcatRotations(gunMatPitch, mat);
        mat = R_ConcatRotations(gunMatYaw, mat);

        originalRots[i] = cl.handrot[i] = AngleVectorFromRotMat(mat);
    }

    // TODO VR: (P1) this is weird, the weapon model name is being used as a
    // key. Should switch to the WID instead
    const auto setHeldWeaponCVar = [](int& cvarEntry, entity_t& viewEntity)
    {
        if(viewEntity.model)
        {
            cvarEntry = VR_GetWpnCVarFromModel(viewEntity.model);
        }
    };

    setHeldWeaponCVar(VR_GetOffHandWpnCvarEntry(), cl.offhand_viewent);
    setHeldWeaponCVar(VR_GetMainHandWpnCvarEntry(), cl.viewent);

    entity_t& player = VR_GetPlayerEntity();

    // headvel
    cl.headvel = redirectVectorByYaw(
        openVRCoordsToQuakeCoords(headVelocity.v), VR_GetTurnYawAngle());
    debugPrintHeadvel();

    // handpos and offhandpos
    SetHandPos(0, player);
    SetHandPos(1, player);

    lastPlayerOrigin = player.origin;
    gotLastPlayerOrigin = true;

    VR_Do2HAiming(originalRots);
    VR_DoWeaponDirSlerp();

    if(vr_fakevr.value == 1)
    {
        VR_FakeVRControllerAiming();
    }

    VR_DoUpdatePrevAnglesAndPlayerYaw();
    VR_DoTeleportation();
    VR_DoWpnButton();

    // flick reload
    const auto doFlickReload = [](const int handIdx)
    {
        bool& prev = handIdx == cVR_MainHand ? vr_right_prevreloadflicking
                                             : vr_left_prevreloadflicking;

        bool& curr = handIdx == cVR_MainHand ? vr_right_reloadflicking
                                             : vr_left_reloadflicking;

        if(!controllers[handIdx].active)
        {
            prev = curr = false;
            return;
        }

        const qvec3 rawAngVel = openVRCoordsToQuakeCoords(
            controllers[handIdx].angularVelocityHistory.average());

        prev = curr;
        curr =
            glm::length(rawAngVel) >= vr_spinreload_x_angular_threshold.value;

        if(flickreload_effect[handIdx] == 0.f)
        {
            if(!prev && curr)
            {
                flickreload_effect[handIdx] = 360.f;
            }
        }
    };

    doFlickReload(cVR_MainHand);
    doFlickReload(cVR_OffHand);
}

static void VR_ResetGlobals()
{
    for(int handIdx = 0; handIdx < 2; ++handIdx)
    {
        vr_wpnbutton_state[handIdx] = WpnButtonState{};
    }
}

void VR_OnSpawnServer()
{
    VR_ResetGlobals();
}

void VR_OnClientClearState()
{
    VR_ResetGlobals();
}


[[nodiscard]] static float handSkeletalToFrame(
    const FingerIdx fingerIdx, const vr::VRSkeletalSummaryData_t& ss)
{
    // Close thumb in pancake mode.
    if(fingerIdx == FingerIdx::Thumb && vr_fakevr.value)
    {
        return 5.f;
    }

    // Automatically close thumb if most other fingers are curled.
    if(fingerIdx == FingerIdx::Thumb && vr_finger_auto_close_thumb.value)
    {
        const auto avg = [](const auto... xs)
        { return (xs + ...) / sizeof...(xs); };

        const auto avgCurl = [&](const auto... xs)
        { return avg(ss.flFingerCurl[(int)xs]...); };

        if(avgCurl(FingerIdx::Index, FingerIdx::Middle, FingerIdx::Ring,
               FingerIdx::Pinky) > 0.5)
        {
            return 5.f;
        }
    }

    return std::clamp(
               ss.flFingerCurl[(int)fingerIdx] + vr_finger_grip_bias.value, 0.f,
               1.f) *
           5.f;
}

[[nodiscard]] static float handSkeletalToFrameForHand(
    const FingerIdx fingerIdx, const HandIdx handIdx)
{
    return handSkeletalToFrame(
        fingerIdx, handIdx == cVR_OffHand ? vr_ss_lefthand : vr_ss_righthand);
}

[[nodiscard]] static float handSkeletalToFrameForHandWithBlending(
    const FingerIdx fingerIdx, const HandIdx handIdx)
{
    const float targetFrame = handSkeletalToFrameForHand(fingerIdx, handIdx);

    if(!vr_finger_blending.value)
    {
        return targetFrame;
    }
    const float frametime = cl.time - cl.oldtime;
    auto& ff = vr_fingertracking_frame[handIdx][(int)fingerIdx];
    const float speed = frametime * vr_finger_blending_speed.value;

    if(ff > targetFrame)
    {
        ff -= speed;

        if(ff < targetFrame)
        {
            ff = targetFrame;
        }
    }
    else if(ff < targetFrame)
    {
        ff += speed;

        if(ff > targetFrame)
        {
            ff = targetFrame;
        }
    }

    return ff;
}

void VR_UpdateFingerTracking()
{
    for(const HandIdx handIdx : {cVR_OffHand, cVR_MainHand})
    {
        for(const FingerIdx fingerIdx : {FingerIdx::Thumb, FingerIdx::Index,
                FingerIdx::Middle, FingerIdx::Ring, FingerIdx::Pinky})
        {
            vr_fingertracking_frame[(int)handIdx][(int)fingerIdx] =
                handSkeletalToFrameForHandWithBlending(fingerIdx, handIdx);
        }
    }
}

void VR_UpdateFlick()
{
    for(int i = 0; i < 2; ++i)
    {
        int handIdx = i;

        if(flickreload_effect[handIdx] <= 0.f)
        {
            flickreload_effect[handIdx] = 0.f;
            cl.visual_handrot[handIdx] = cl.handrot[handIdx];
            continue;
        }

        if(flickreload_effect[handIdx] > 0.f)
        {
            const auto frametime = cl.time - cl.oldtime;

            flickreload_effect[handIdx] -=
                static_cast<float>(frametime) * vr_spinreload_pitch_speed.value;
        }

        const auto [fwd, right, up] = getAngledVectors(cl.handrot[handIdx]);

        cl.visual_handrot[handIdx] = VectorAngles(
            TurnVector(fwd, up, 360.f - flickreload_effect[handIdx]), up);
    }
}

void VR_UpdateScreenContent()
{
    // Last chance to enable VR Mode - we get here when the game already
    // start up with vr_enabled 1 If enabling fails, unset the cvar and
    // return.
    if(!vr_initialized && !VR_Enable())
    {
        Cvar_Set("vr_enabled", "0");
        return;
    }

    // Finger tracking blending information.
    VR_UpdateFingerTracking();

    // Get and update the VR devices' orientation and position
    VR_UpdateDevicesOrientationPosition();

    // Reset the aim roll value before calculation, incase the user switches
    // aimmode from 7 to another.
    cl.aimangles[ROLL] = 0.0;

    const auto orientation = VR_GetEyesOrientation();

    if(vr_fakevr.value == 0 && std::exchange(readbackYaw, false))
    {
        vrYaw = cl.viewangles[YAW] - (orientation[YAW] - vrYaw);
    }

    switch((int)vr_aimmode.value)
    {

        default:
        // 7: (Default) Controller Aiming;
        case VrAimMode::e_CONTROLLER:
        {
            VR_ControllerAiming(orientation);
            break;
        }

            // 1:Head Aiming; View YAW is mouse+head, PITCH is
            // head
        case VrAimMode::e_HEAD_MYAW:
            cl.viewangles[PITCH] = cl.aimangles[PITCH] = orientation[PITCH];
            cl.aimangles[YAW] = cl.viewangles[YAW] =
                cl.aimangles[YAW] + orientation[YAW] - lastOrientation[YAW];
            break;

            // 2: Head Aiming; View YAW and PITCH is mouse+head (this is
            // stupid)
        case VrAimMode::e_HEAD_MYAW_MPITCH:
            cl.viewangles[PITCH] = cl.aimangles[PITCH] = cl.aimangles[PITCH] +
                                                         orientation[PITCH] -
                                                         lastOrientation[PITCH];
            cl.aimangles[YAW] = cl.viewangles[YAW] =
                cl.aimangles[YAW] + orientation[YAW] - lastOrientation[YAW];
            break;

            // 3: Mouse Aiming; View YAW is mouse+head, PITCH is head
        case VrAimMode::e_MOUSE_MYAW:
            cl.viewangles[PITCH] = orientation[PITCH];
            cl.viewangles[YAW] = cl.aimangles[YAW] + orientation[YAW];
            break;

            // 4: Mouse Aiming; View YAW and PITCH is mouse+head
        case VrAimMode::e_MOUSE_MYAW_MPITCH:
            cl.viewangles[PITCH] = cl.aimangles[PITCH] + orientation[PITCH];
            cl.viewangles[YAW] = cl.aimangles[YAW] + orientation[YAW];
            break;

        case VrAimMode::e_BLENDED: [[fallthrough]];
        case VrAimMode::e_BLENDED_NOPITCH:
        {
            const auto diffHMDYaw = orientation[YAW] - lastOrientation[YAW];
            const auto diffHMDPitch =
                orientation[PITCH] - lastOrientation[PITCH];
            const auto diffAimYaw = cl.aimangles[YAW] - lastAim[YAW];
            float diffYaw;

            // find new view position based on orientation delta
            cl.viewangles[YAW] += diffHMDYaw;

            // find difference between view and aim yaw
            diffYaw = cl.viewangles[YAW] - cl.aimangles[YAW];

            if(fabs(diffYaw) > vr_deadzone.value / 2.0f)
            {
                // apply the difference from each set of angles to the other
                cl.aimangles[YAW] += diffHMDYaw;
                cl.viewangles[YAW] += diffAimYaw;
            }
            if((int)vr_aimmode.value == VrAimMode::e_BLENDED)
            {
                cl.aimangles[PITCH] += diffHMDPitch;
            }
            cl.viewangles[PITCH] = orientation[PITCH];

            break;
        }
    }

    if(vr_fakevr.value == 0)
    {
        cl.viewangles[ROLL] = orientation[ROLL];
    }

    lastOrientation = orientation;
    lastAim = cl.aimangles;

    r_refdef.viewangles = cl.viewangles;
    r_refdef.aimangles = cl.aimangles;

    if(isDedicated || vr_fakevr.value || vr_novrinit.value)
    {
        return;
    }

    // Render the scene for each eye into their FBOs
    for(vr_eye_t& eye : eyes)
    {
        // TODO VR: (P2) this global is problematic, remove it and pass args
        // around It is used in view.cpp and gl_rmain.cpp
        current_eye = &eye;

        // We need to scale the view offset position to quake units and
        // rotate it by the current input angles (viewangle - eye
        // orientation)
        const auto orientation = QuatToYawPitchRoll(eye.orientation);
        qvec3 temp{-eye.position.v[2], -eye.position.v[0], eye.position.v[1]};
        temp *= meters_to_units;
        vr_viewOffset = Vec3RotateZ(
            temp, (r_refdef.viewangles[YAW] - orientation[YAW]) * M_PI_DIV_180);

        vr_viewOffset[2] += vr_floor_offset.value;

        RenderScreenForCurrentEye_OVR(eye);
    }

    // Blit mirror texture to backbuffer
    const GLint w = glwidth;
    const GLint h = glheight;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, eyes[0].fbo.framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, eyes[0].fbo.size.width, eyes[0].fbo.size.height, 0, 0,
        h, w, 0, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

void VR_SetMatrices()
{
    // Calculate HMD projection matrix and view offset position
    vr::HmdMatrix44_t projection = TransposeMatrix(
        ovrHMD->GetProjectionMatrix(current_eye->eye, 4.f, gl_farclip.value));

    // Set OpenGL projection and view matrices
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((GLfloat*)projection.m);
}

[[nodiscard]] qvec3 VR_GetLastHeadOrigin() noexcept
{
    return lastHeadOrigin;
}

[[nodiscard]] qfloat VR_GetCrouchRatio() noexcept
{
    const auto maxHeight = vr_height_calibration.value;
    const auto currHeight = VR_GetLastHeadOrigin()[2];
    return std::clamp((maxHeight / currHeight) - 1._qf, 0._qf, 1._qf);
}

void VR_CalibrateHeight()
{
    const auto height = lastHeadOrigin[2];
    Cvar_SetValue("vr_height_calibration", height);
    Con_Printf("Calibrated height to %.2f\n", height);
}

[[nodiscard]] qvec3 VR_AddOrientationToViewAngles(const qvec3& angles) noexcept
{
    const auto [pitch, yaw, roll] =
        QuatToYawPitchRoll(current_eye->orientation);

    return {angles[PITCH] + pitch, angles[YAW] + yaw, roll};
}

[[nodiscard]] bool VR_InHipHolsterDistance(
    const qvec3& hand, const qvec3& holster)
{
    return glm::distance(hand, holster) < vr_hip_holster_thresh.value;
}

[[nodiscard]] bool VR_InShoulderHolsterDistance(
    const qvec3& hand, const qvec3& holster)
{
    return glm::distance(hand, holster) < vr_shoulder_holster_thresh.value;
}

[[nodiscard]] bool VR_InUpperHolsterDistance(
    const qvec3& hand, const qvec3& holster)
{
    return glm::distance(hand, holster) < vr_upper_holster_thresh.value;
}

void VR_Draw2D()
{
    bool draw_sbar = false;

    const float scale_hud = vr_menu_scale.value;

    const int oldglwidth = glwidth;
    const int oldglheight = glheight;
    const int oldconwidth = vid.conwidth;
    const int oldconheight = vid.conheight;

    glwidth = 320;
    glheight = 200;

    vid.conwidth = 320;
    vid.conheight = 200;

    // draw 2d elements 1m from the users face, centered
    glPushMatrix();
    glDisable(GL_DEPTH_TEST); // prevents drawing sprites on sprites from
                              // interferring with one another
    glEnable(GL_BLEND);

    const VrMenuMode menuMode = [&]
    {
        const auto v =
            static_cast<VrMenuMode>(static_cast<int>(vr_menumode.value));

        if((v == VrMenuMode::LastHeadAngles || v == VrMenuMode::FollowHead) ||
            (v == VrMenuMode::FollowOffHand && controllers[0].active) ||
            (v == VrMenuMode::FollowMainHand && controllers[1].active))
        {
            return v;
        }

        return VrMenuMode::LastHeadAngles;
    }();

    const bool headMenuMode = menuMode == VrMenuMode::LastHeadAngles ||
                              menuMode == VrMenuMode::FollowHead;

    if(headMenuMode)
    {
        if(menuMode == VrMenuMode::LastHeadAngles)
        {
            vr_menu_angles = inMenuOrConsole() ? lastMenuAngles : cl.viewangles;
        }
        else
        {
            vr_menu_angles = cl.viewangles;
        }

        if(vr_aimmode.value == VrAimMode::e_HEAD_MYAW ||
            vr_aimmode.value == VrAimMode::e_HEAD_MYAW_MPITCH)
        {
            vr_menu_angles[PITCH] = 0;
        }

        const auto fwd = getFwdVecFromPitchYawRoll(vr_menu_angles);
        vr_menu_target =
            r_refdef.vieworg + qfloat(vr_menu_distance.value) * fwd;
    }
    else
    {
        const auto hand =
            menuMode == VrMenuMode::FollowOffHand ? cVR_OffHand : cVR_MainHand;

        vr_menu_angles = cl.handrot[hand];

        const auto fwd = getFwdVecFromPitchYawRoll(vr_menu_angles);
        vr_menu_target =
            cl.handpos[hand] + qfloat(vr_menu_distance.value) * fwd;
    }

    // TODO VR: (P2) control smoothing with cvar
    const auto smoothedTarget = glm::mix(lastMenuPosition, vr_menu_target, 0.9);
    lastMenuPosition = smoothedTarget;

    glTranslatef(smoothedTarget[0], smoothedTarget[1], smoothedTarget[2]);

    vr_menu_angles[YAW] -= 90;
    vr_menu_angles[PITCH] += 90;

    // rotate around z
    glRotatef(vr_menu_angles[YAW], 0, 0, 1);

    // keep bar at constant angled pitch towards user
    glRotatef(vr_menu_angles[PITCH], -1, 0, 0);

    // center the status bar
    glTranslatef(-(320.0 * scale_hud / 2), -(200.0 * scale_hud / 2), 0);

    glScalef(scale_hud, scale_hud, scale_hud);

    if(scr_drawdialog) // new game confirm
    {
        if(con_forcedup)
        {
            Draw_ConsoleBackground();
        }
        else
        {
            draw_sbar = true; // Sbar_Draw ();
        }
        Draw_FadeScreen();
        SCR_DrawNotifyString();
    }
    else if(scr_drawloading) // loading
    {
        SCR_DrawLoading();
        draw_sbar = true; // Sbar_Draw ();
    }
    else if(cl.intermission == 1 && inGame()) // end of level
    {
        Sbar_IntermissionOverlay();
    }
    else if(cl.intermission == 2 && inGame()) // end of episode
    {
        Sbar_FinaleOverlay();
        SCR_CheckDrawCenterString();
    }
    else
    {
        // SCR_DrawCrosshair (); //johnfitz
        SCR_DrawRam();
        SCR_DrawNet();
        SCR_DrawTurtle();
        SCR_DrawPause();
        SCR_CheckDrawCenterString();
        draw_sbar = true;   // Sbar_Draw ();
        SCR_DrawDevStats(); // johnfitz
        SCR_DrawFPS();      // johnfitz
        SCR_DrawClock();    // johnfitz
        SCR_DrawConsole();
        M_Draw();
        M_DrawKeyboard();
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();

    if(draw_sbar)
    {
        VR_DrawSbar();
    }

    glwidth = oldglwidth;
    glheight = oldglheight;
    vid.conwidth = oldconwidth;
    vid.conheight = oldconheight;
}


void VR_DrawSbar()
{
    qvec3 sbar_angles;
    qvec3 forward;
    qvec3 right;
    qvec3 up;
    qvec3 target;

    const float scale_hud = vr_hud_scale.value;

    glPushMatrix();
    glDisable(GL_DEPTH_TEST); // prevents drawing sprites on sprites from
                              // interferring with one another

    if(vr_aimmode.value == VrAimMode::e_CONTROLLER)
    {
        const auto mode = cvarToEnum<VrSbarMode>(vr_sbar_mode);

        if(mode == VrSbarMode::MainHand)
        {
            std::tie(forward, right, up) =
                getAngledVectors(cl.handrot[cVR_MainHand]);
            sbar_angles = cl.handrot[cVR_MainHand];

            std::tie(forward, right, up) = getAngledVectors(sbar_angles);
            target = cl.handpos[cVR_MainHand] + -5._qf * right;
        }
        else
        {
            std::tie(forward, right, up) =
                getAngledVectors(cl.handrot[cVR_OffHand]);
            sbar_angles = cl.handrot[cVR_OffHand];

            std::tie(forward, right, up) = getAngledVectors(sbar_angles);
            target = cl.handpos[cVR_OffHand] + 0._qf * right;
        }
    }
    else
    {
        sbar_angles = cl.aimangles;

        if(vr_aimmode.value == VrAimMode::e_HEAD_MYAW ||
            vr_aimmode.value == VrAimMode::e_HEAD_MYAW_MPITCH)
        {
            sbar_angles[PITCH] = 0;
        }

        std::tie(forward, right, up) = getAngledVectors(sbar_angles);

        target = cl.viewent.origin + 1._qf * forward;
    }

    // TODO VR: (P2) 1.0? Attach to off hand?
    const auto smoothedTarget = glm::mix(lastHudPosition, target, 1.0);
    lastHudPosition = smoothedTarget;

    glTranslatef(smoothedTarget[0], smoothedTarget[1], smoothedTarget[2]);

    const auto sbarmode = cvarToEnum<VrSbarMode>(vr_sbar_mode);

    if(vr_aimmode.value == VrAimMode::e_CONTROLLER &&
        sbarmode == VrSbarMode::OffHand)
    {
        qquat m;
        m = glm::quatLookAt(forward, up);
        m = glm::rotate(m, qfloat(vr_sbar_offset_pitch.value), qvec3{1, 0, 0});
        m = glm::rotate(m, qfloat(vr_sbar_offset_yaw.value), qvec3{0, 1, 0});
        m = glm::rotate(m, qfloat(vr_sbar_offset_roll.value), qvec3{0, 0, 1});
        m = glm::normalize(m);

        glMultMatrixf(&toGlMat(m)[0][0]);

        const auto ox = vr_sbar_offset_x.value;
        const auto oy = vr_sbar_offset_y.value;
        const auto oz = vr_sbar_offset_z.value;

        glTranslatef(ox, oy, oz);
    }
    else
    {
        glRotatef(sbar_angles[YAW] - 90, 0, 0, 1); // rotate around z
        glRotatef(90 + 45 + sbar_angles[PITCH], -1, 0,
            0); // keep bar at constant angled pitch towards user

        glTranslatef(-(320.0 * scale_hud / 2), 0, 0); // center the status bar
        glTranslatef(0, 0, 10);                       // move hud down a bit
    }

    glScalef(scale_hud, scale_hud, scale_hud);

    Sbar_Draw();

    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
}

void VR_SetAngles(const qvec3& angles) noexcept
{
    cl.aimangles = angles;
    cl.viewangles = angles;
    lastAim = angles;
}

void VR_ResetOrientation()
{
    cl.aimangles[YAW] = cl.viewangles[YAW];
    cl.aimangles[PITCH] = cl.viewangles[PITCH];

    if(vr_enabled.value)
    {
        // IVRSystem_ResetSeatedZeroPose(ovrHMD);
        lastAim = cl.aimangles;
    }
}

struct VRAxisResult
{
    float fwdMove;
    float sideMove;
    float yawMove;
};

[[nodiscard]] static VRAxisResult VR_GetInputAxes(
    const vr::InputAnalogActionData_t& locomotion,
    const vr::InputAnalogActionData_t& turn)
{
    return {locomotion.y, locomotion.x, turn.x};
}

[[nodiscard]] static VRAxisResult VR_GetInputAxesFromBooleanInputs(
    const bool bMoveForward, const bool bMoveBackward, const bool bMoveLeft,
    const bool bMoveRight, const bool bTurnLeft, const bool bTurnRight)
{
    const auto getAxis = [](const bool left, const bool right)
    {
        if(left)
        {
            return -1.f;
        }

        if(right)
        {
            return 1.f;
        }

        return 0.f;
    };

    const float x = getAxis(bMoveForward, bMoveBackward);
    const float y = getAxis(bMoveRight, bMoveLeft);
    const float turn = getAxis(bTurnLeft, bTurnRight);

    return {x, y, turn};
}

void VR_DoHaptic(const int hand, const float delay, const float duration,
    const float frequency, const float amplitude)
{
    if(isDedicated || vr_fakevr.value == 1 || vr_novrinit.value == 1 ||
        vr_disablehaptics.value == 1)
    {
        // No haptics at all in fake VR mode.
        return;
    }

    const auto hapticTarget =
        hand == cVR_OffHand ? vrahLeftHaptic : vrahRightHaptic;

    vr::VRInput()->TriggerHapticVibrationAction(hapticTarget, delay, duration,
        frequency, amplitude, vr::k_ulInvalidInputValueHandle);
}

[[nodiscard]] float VR_GetMenuMult() noexcept
{
    return vr_menu_mult;
}

static void VR_DoInput_UpdateFakeMouse()
{
    int mx, my;
    const auto mouseState = SDL_GetMouseState(&mx, &my);

    vr_menu_mouse_x = mx;
    vr_menu_mouse_y = my;
    vr_menu_mouse_click = mouseState & SDL_BUTTON(SDL_BUTTON_LEFT);
}

static void VR_DoInput_UpdateVRMouse()
{
    if(!controllers[0].active && !controllers[1].active)
    {
        vr_menu_mouse_x = vr_menu_mouse_y = 0;
        vr_menu_mouse_click = false;
        return;
    }

    const auto updateWith = [&](const HandIdx handIdx)
    {
        const auto& orig = cl.handpos[handIdx];
        const auto dir = getFwdVecFromPitchYawRoll(cl.handrot[handIdx]);

        const auto& planeOrig = vr_menu_target;

        auto adjMenuAngles = vr_menu_angles;
        adjMenuAngles[PITCH] -= 90.f;
        adjMenuAngles[YAW] += 90.f;

        const auto [fwd, right, up] = getAngledVectors(adjMenuAngles);

        qfloat intersectionDist{};
        glm::intersectRayPlane(orig, dir, planeOrig, fwd, intersectionDist);

        const auto intersectionPoint = orig + intersectionDist * dir;
        const auto res = intersectionPoint - planeOrig;

        vr_menu_intersection_point = intersectionPoint;

        const float scale_hud = vr_menu_scale.value;

        const auto sign = [](const float x)
        {
            if(x >= 0)
            {
                return 1.f;
            }
            return -1.f;
        };

        const auto xProj = glm::proj(res, right);
        const float xSign = sign(glm::dot(xProj, right));
        vr_menu_mouse_x = glm::length(xProj) * xSign / scale_hud + 320 / 2;

        const auto yProj = glm::proj(res, up);
        const float ySign = -sign(glm::dot(yProj, up));
        vr_menu_mouse_y = glm::length(yProj) * ySign / scale_hud + 240 / 2;
    };

    const auto doControllersInOrder =
        [&](const HandIdx first, const HandIdx second)
    {
        if(controllers[first].active)
        {
            updateWith(first);
            return;
        }

        if(controllers[second].active)
        {
            updateWith(second);
            return;
        }
    };

    if(vr_menu_mouse_pointer_hand.value == 1)
    {
        doControllersInOrder(cVR_MainHand, cVR_OffHand);
    }
    else
    {
        doControllersInOrder(cVR_OffHand, cVR_MainHand);
    }
}

[[nodiscard]] static VRAxisResult VR_DoInput()
{
    if(isDedicated)
    {
        return {0.f, 0.f, 0.f};
    }

    if(vr_fakevr.value && vr_novrinit.value)
    {
        vr_menu_mult = 1.f;

        vr_left_prevgrabbing = vr_left_grabbing;
        vr_right_prevgrabbing = vr_right_grabbing;
        vr_left_grabbing = !(in_grableft.state & 1);
        vr_right_grabbing = !(in_grabright.state & 1);

        vr_left_prevreloading = vr_left_reloading;
        vr_right_prevreloading = vr_right_reloading;
        vr_left_reloading = in_reloadleft.state & 1;
        vr_right_reloading = in_reloadright.state & 1;

        vr_left_prevreloadflicking = vr_left_reloadflicking;
        vr_right_prevreloadflicking = vr_right_reloadflicking;
        vr_left_reloadflicking = in_flickreloadleft.state & 1;
        vr_right_reloadflicking = in_flickreloadright.state & 1;

        if(vr_left_reloadflicking)
        {
            flickreload_effect[cVR_OffHand] = 360.f;
        }

        if(vr_right_reloadflicking)
        {
            flickreload_effect[cVR_MainHand] = 360.f;
        }

        VR_DoInput_UpdateFakeMouse();

        return {0.f, 0.f, 0.f};
    }

    {
        const auto rc = vr::VRInput()->UpdateActionState(
            &vrActiveActionSet, sizeof(vr::VRActiveActionSet_t), 1);

        if(rc != vr::EVRInputError::VRInputError_None)
        {
            Con_Printf(
                "Failed to update Steam VR action state, rc = %d", (int)rc);
        }
    }

    const auto readAnalogAction = [](const vr::VRActionHandle_t& actionHandle)
    {
        vr::InputAnalogActionData_t out;

        const auto rc = vr::VRInput()->GetAnalogActionData(actionHandle, &out,
            sizeof(vr::InputAnalogActionData_t),
            vr::k_ulInvalidInputValueHandle);

        if(rc != vr::EVRInputError::VRInputError_None)
        {
            Con_Printf("Failed to read Steam VR analog action data, rc = %d\n",
                (int)rc);
        }

        return out;
    };

    const auto readDigitalAction = [](const vr::VRActionHandle_t& actionHandle)
    {
        vr::InputDigitalActionData_t out;

        const vr::EVRInputError rc = vr::VRInput()->GetDigitalActionData(
            actionHandle, &out, sizeof(vr::InputDigitalActionData_t),
            vr::k_ulInvalidInputValueHandle);

        if(rc != vr::EVRInputError::VRInputError_None)
        {
            Con_Printf("Failed to read Steam VR digital action data, rc = %d\n",
                (int)rc);
        }

        return out;
    };

    const auto readSkeletalSummary =
        [](const vr::VRActionHandle_t& actionHandle)
    {
        vr::InputSkeletalActionData_t outActionData;

        {
            const auto rc = vr::VRInput()->GetSkeletalActionData(actionHandle,
                &outActionData, sizeof(vr::InputSkeletalActionData_t));

            if(rc != vr::EVRInputError::VRInputError_None)
            {
                Con_Printf(
                    "Failed to read Steam VR skeletal action data, rc = %d\n",
                    (int)rc);
            }
        }

        if(!outActionData.bActive)
        {
            // Con_Printf(
            //     "Failed to read Steam VR inactive skeletal action data\n");

            return vr::VRSkeletalSummaryData_t{};
        }

        vr::VRSkeletalSummaryData_t outSummary;

        {
            const int rc = vr::VRInput()->GetSkeletalSummaryData(actionHandle,
                vr::EVRSummaryType::VRSummaryType_FromDevice, &outSummary);

            if(rc != vr::EVRInputError::VRInputError_None)
            {
                Con_Printf(
                    "Failed to read Steam VR skeletal summary data, rc = %d\n",
                    (int)rc);
            }
        }

        return outSummary;
    };

    const auto ssLeftHand = readSkeletalSummary(vrahLeftHandAnim);
    const auto ssRightHand = readSkeletalSummary(vrahRightHandAnim);

    vr_ss_lefthand = ssLeftHand;
    vr_ss_righthand = ssRightHand;

    const auto inpLocomotion = readAnalogAction(vrahLocomotion);
    const auto inpTurn = readAnalogAction(vrahTurn);

    const auto inpFireMainHand = readDigitalAction(vrahFireMainHand);
    const auto inpFireOffHand = readDigitalAction(vrahFireOffHand);
    const auto inpJump = readDigitalAction(vrahJump);
    const auto inpPrevWeaponMainHand =
        readDigitalAction(vrahPrevWeaponMainHand);
    const auto inpNextWeaponMainHand =
        readDigitalAction(vrahNextWeaponMainHand);
    const auto inpEscape = readDigitalAction(vrahEscape);
    const auto inpSpeed = readDigitalAction(vrahSpeed);
    const auto inpTeleport = readDigitalAction(vrahTeleport);
    const auto inpLeftGrab = readDigitalAction(vrahLeftGrab);
    const auto inpRightGrab = readDigitalAction(vrahRightGrab);
    const auto inpLeftReload = readDigitalAction(vrahLeftReload);
    const auto inpRightReload = readDigitalAction(vrahRightReload);
    const auto inpPrevWeaponOffHand = readDigitalAction(vrahPrevWeaponOffHand);
    const auto inpNextWeaponOffHand = readDigitalAction(vrahNextWeaponOffHand);

    const auto inpBMoveForward = readDigitalAction(vrahBMoveForward);
    const auto inpBMoveBackward = readDigitalAction(vrahBMoveBackward);
    const auto inpBMoveLeft = readDigitalAction(vrahBMoveLeft);
    const auto inpBMoveRight = readDigitalAction(vrahBMoveRight);
    const auto inpBTurnLeft = readDigitalAction(vrahBTurnLeft);
    const auto inpBTurnRight = readDigitalAction(vrahBTurnRight);

    const auto isRisingEdge = [](const vr::InputDigitalActionData_t& data)
    { return data.bState && data.bChanged; };

    const bool mustFireMainHand = inpFireMainHand.bState;
    const bool mustFireOffHand = inpFireOffHand.bState;

    const bool isRoomscaleJump =
        vr_roomscale_jump.value &&
        headVelocity.v[1] > vr_roomscale_jump_threshold.value &&
        lastHeadOrigin[2] > vr_height_calibration.value;

    const bool mustJump = isRisingEdge(inpJump) || isRoomscaleJump;
    const bool mustPrevWeaponMainHand = isRisingEdge(inpPrevWeaponMainHand);
    const bool mustNextWeaponMainHand = isRisingEdge(inpNextWeaponMainHand);
    // const bool mustEscape = isRisingEdge(inpEscape);
    const bool mustSpeed = inpSpeed.bState;
    const bool mustTeleport = inpTeleport.bState;
    const bool mustPrevWeaponOffHand = isRisingEdge(inpPrevWeaponOffHand);
    const bool mustNextWeaponOffHand = isRisingEdge(inpNextWeaponOffHand);

    const auto mustBMoveForward = inpBMoveForward.bState;
    const auto mustBMoveBackward = inpBMoveBackward.bState;
    const auto mustBMoveLeft = inpBMoveLeft.bState;
    const auto mustBMoveRight = inpBMoveRight.bState;
    const auto mustBTurnLeft = inpBTurnLeft.bState;
    const auto mustBTurnRight = inpBTurnRight.bState;

    const auto inpMenuNavigation = readAnalogAction(vrahMenuNavigation);
    const auto inpMenuUp = readDigitalAction(vrahMenuUp);
    const auto inpMenuDown = readDigitalAction(vrahMenuDown);
    const auto inpMenuLeft = readDigitalAction(vrahMenuLeft);
    const auto inpMenuRight = readDigitalAction(vrahMenuRight);
    const auto inpMenuEnter = readDigitalAction(vrahMenuEnter);
    const auto inpMenuBack = readDigitalAction(vrahMenuBack);

    // TODO VR: (P1) what to do with this?
    const auto inpMenuAddToShortcuts =
        readDigitalAction(vrahMenuAddToShortcuts);

    const auto inpMenuMultiplierHalf =
        readDigitalAction(vrahMenuMultiplierHalf);
    const auto inpMenuMultiplierPlusOne =
        readDigitalAction(vrahMenuMultiplierPlusOne);
    const auto inpMenuMultiplierPlusOne2 =
        readDigitalAction(vrahMenuMultiplierPlusOne2);

    // VR: Active action set transition.
    if(inMenuOrConsole() && !inpEscape.bState)
    {
        vrActiveActionSet.ulActionSet = vrashMenu;
    }
    else if(!inMenuOrConsole() && !inpMenuBack.bState)
    {
        vrActiveActionSet.ulActionSet = vrashDefault;
    }

    // TODO VR: (P2) global state mutation here, could be source of bugs
    vr_left_prevgrabbing = vr_left_grabbing;
    vr_right_prevgrabbing = vr_right_grabbing;

    // TODO VR: (P2) global state mutation here, could be source of bugs
    vr_left_prevreloading = vr_left_reloading;
    vr_right_prevreloading = vr_right_reloading;

    if(vr_fakevr.value == 0)
    {
        Key_Event('k', inpLeftGrab.bState);
        Key_Event('l', inpRightGrab.bState);
        vr_left_grabbing = (in_grableft.state & 1);
        vr_right_grabbing = (in_grabright.state & 1);

        Key_Event('n', inpLeftReload.bState);
        Key_Event('m', inpRightReload.bState);
        vr_left_reloading = (in_reloadleft.state & 1);
        vr_right_reloading = (in_reloadright.state & 1);

        VR_DoInput_UpdateVRMouse();
    }
    else
    {
        vr_left_grabbing = !(in_grableft.state & 1);
        vr_right_grabbing = !(in_grabright.state & 1);

        vr_left_reloading = in_reloadleft.state & 1;
        vr_right_reloading = in_reloadright.state & 1;

        vr_left_reloadflicking = in_flickreloadleft.state & 1;
        vr_right_reloadflicking = in_flickreloadright.state & 1;

        if(vr_left_reloadflicking)
        {
            flickreload_effect[cVR_OffHand] = 360.f;
        }

        if(vr_right_reloadflicking)
        {
            flickreload_effect[cVR_MainHand] = 360.f;
        }

        VR_DoInput_UpdateFakeMouse();
    }

    in_speed.state = mustSpeed;

    // Menu multipliers to fine-tune values.
    vr_menu_mult = inpMenuMultiplierHalf.bState ? 0.5f : 1.f;
    vr_menu_mult += static_cast<int>(inpMenuMultiplierPlusOne.bState);
    vr_menu_mult += static_cast<int>(inpMenuMultiplierPlusOne2.bState);

    const auto doMenuHaptic = [&](const vr::VRInputValueHandle_t& origin)
    {
        vr::VRInput()->TriggerHapticVibrationAction(
            vrahLeftHaptic, 0, 0.1, 50, 0.5, origin);

        vr::VRInput()->TriggerHapticVibrationAction(
            vrahRightHaptic, 0, 0.1, 50, 0.5, origin);
    };

    /*
    const auto doMenuKeyEvent = [&](const int key,
                                    const vr::InputDigitalActionData_t& i) {
        const bool pressed = isRisingEdge(i);

        Key_Event(key, pressed);
        return pressed;
    };
    */

    const auto doMenuKeyEventWithHaptic =
        [&](const int key, const vr::InputDigitalActionData_t& i)
    {
        const bool pressed = isRisingEdge(i);

        if(pressed)
        {
            doMenuHaptic(i.activeOrigin);
        }

        Key_Event(key, pressed);
        return pressed;
    };

    if(inMenuOrConsole())
    {
        const auto doAxis = [&](const int axis, const auto& inp,
                                const int quakeKeyNeg, const int quakeKeyPos)
        {
            const float lastVal =
                axis == 0 ? (inp.x - inp.deltaX) : (inp.y - inp.deltaY);

            const float val = axis == 0 ? inp.x : inp.y;

            const float deadzone = 0.025f;

            const bool posWasDown = lastVal > deadzone;
            const bool posDown = val > deadzone;
            if(posDown != posWasDown)
            {
                if(posDown)
                {
                    doMenuHaptic(inp.activeOrigin);
                }

                Key_Event(quakeKeyNeg, posDown);
            }

            const bool negWasDown = lastVal < -deadzone;
            const bool negDown = val < -deadzone;
            if(negDown != negWasDown)
            {
                if(negDown)
                {
                    doMenuHaptic(inp.activeOrigin);
                }

                Key_Event(quakeKeyPos, negDown);
            }
        };

        const auto doBooleanInput = [&](const auto& inp, const int quakeKey)
        {
            if(inp.bChanged)
            {
                if(inp.bState)
                {
                    doMenuHaptic(inp.activeOrigin);
                }

                Key_Event(quakeKey, inp.bState);
            }
        };

        if(vr_fakevr.value == 0)
        {
            vr_menu_mouse_click = inpMenuEnter.bState;

            doMenuKeyEventWithHaptic(K_ENTER, inpMenuEnter);
            doMenuKeyEventWithHaptic(K_ESCAPE, inpMenuBack);
            doMenuKeyEventWithHaptic(K_LEFTARROW, inpMenuLeft);
            doMenuKeyEventWithHaptic(K_RIGHTARROW, inpMenuRight);

            doAxis(1 /* Y axis */, inpMenuNavigation, K_UPARROW, K_DOWNARROW);
            // doAxis(0 /* X axis */, inpTurn, K_RIGHTARROW, K_LEFTARROW);

            doBooleanInput(inpMenuUp, K_UPARROW);
            doBooleanInput(inpMenuDown, K_DOWNARROW);
        }
    }
    else
    {
        // VR: We don't want these keypresses to override the mouse buttons
        // in fake VR mode.
        if(vr_fakevr.value == 0)
        {
            Key_Event(K_MOUSE1, mustFireMainHand);
            Key_Event(K_MOUSE2, mustFireOffHand);
        }

        Key_Event(K_SPACE, mustJump);

        if(doMenuKeyEventWithHaptic(K_ESCAPE, inpEscape))
        {
            lastMenuAngles = cl.viewangles;
        }

        Key_Event('1', mustNextWeaponMainHand); // impulse 10
        Key_Event('3', mustNextWeaponOffHand);  // impulse 12
        Key_Event('4', mustPrevWeaponMainHand); // impulse 15
        Key_Event('5', mustPrevWeaponOffHand);  // impulse 16

        vr_teleporting = mustTeleport;
    }

    const auto anyBooleanLocomotion = mustBMoveForward || mustBMoveBackward ||
                                      mustBMoveLeft || mustBMoveRight ||
                                      mustBTurnLeft || mustBTurnRight;

    if(anyBooleanLocomotion)
    {
        return VR_GetInputAxesFromBooleanInputs(mustBMoveForward,
            mustBMoveBackward, mustBMoveLeft, mustBMoveRight, mustBTurnLeft,
            mustBTurnRight);
    }
    else
    {
        return VR_GetInputAxes(inpLocomotion, inpTurn);
    }
}

void VR_Move(usercmd_t* cmd)
{
    if(isDedicated || !vr_enabled.value)
    {
        return;
    }

    // VR: VR yaw.
    cmd->vryaw = vrYaw;

    // VR: Main hand values.
    cmd->handpos = cl.handpos[cVR_MainHand];
    cmd->handrot = cl.handrot[cVR_MainHand];
    cmd->handvel = cl.handvel[cVR_MainHand];
    cmd->handthrowvel = cl.handthrowvel[cVR_MainHand];
    cmd->handvelmag = cl.handvelmag[cVR_MainHand];
    cmd->handavel = cl.handavel[cVR_MainHand];

    // VR: Off hand values.
    cmd->offhandpos = cl.handpos[cVR_OffHand];
    cmd->offhandrot = cl.handrot[cVR_OffHand];
    cmd->offhandvel = cl.handvel[cVR_OffHand];
    cmd->offhandthrowvel = cl.handthrowvel[cVR_OffHand];
    cmd->offhandvelmag = cl.handvelmag[cVR_OffHand];
    cmd->offhandavel = cl.handavel[cVR_OffHand];

    // VR: Head velocity.
    cmd->headvel = cl.headvel;

    // VR: Weapon muzzle position.
    cmd->muzzlepos = VR_CalcFinalWpnMuzzlePos(cVR_MainHand);
    cmd->offmuzzlepos = VR_CalcFinalWpnMuzzlePos(cVR_OffHand);

    // VR: Teleportation.
    const bool teleporting = std::exchange(vr_send_teleport_msg, false);
    if(teleporting)
    {
        cmd->teleport_target = vr_teleporting_impact;
    }

    // VR: Buttons and instant controller actions.
    // VR: Query state of controller axes.
    const auto [fwdMove, sideMove, yawMove] = VR_DoInput();

    // VR: VR-related bits.
    {
        const auto setBit =
            [](unsigned int& flags, const int bit, const bool value)
        {
            if(value)
            {
                flags |= bit;
            }
            else
            {
                flags &= ~bit;
            }
        };

        const bool twoHAiming = (vr_2h_aim_transition[cVR_OffHand] >= 0.5f) ||
                                (vr_2h_aim_transition[cVR_MainHand] >= 0.5f);

        cmd->vrbits0 = 0;
        setBit(cmd->vrbits0, QVR_VRBITS0_TELEPORTING, teleporting);
        setBit(cmd->vrbits0, QVR_VRBITS0_OFFHAND_GRABBING, vr_left_grabbing);
        setBit(cmd->vrbits0, QVR_VRBITS0_OFFHAND_PREVGRABBING,
            vr_left_prevgrabbing);
        setBit(cmd->vrbits0, QVR_VRBITS0_MAINHAND_GRABBING, vr_right_grabbing);
        setBit(cmd->vrbits0, QVR_VRBITS0_MAINHAND_PREVGRABBING,
            vr_right_prevgrabbing);
        setBit(cmd->vrbits0, QVR_VRBITS0_2H_AIMING, twoHAiming);
        setBit(cmd->vrbits0, QVR_VRBITS0_OFFHAND_RELOADING, vr_left_reloading);
        setBit(cmd->vrbits0, QVR_VRBITS0_OFFHAND_PREVRELOADING,
            vr_left_prevreloading);
        setBit(
            cmd->vrbits0, QVR_VRBITS0_MAINHAND_RELOADING, vr_right_reloading);
        setBit(cmd->vrbits0, QVR_VRBITS0_MAINHAND_PREVRELOADING,
            vr_right_prevreloading);
        setBit(cmd->vrbits0, QVR_VRBITS0_OFFHAND_RELOADFLICKING,
            vr_left_reloadflicking);
        setBit(cmd->vrbits0, QVR_VRBITS0_OFFHAND_PREVRELOADFLICKING,
            vr_left_prevreloadflicking);
        setBit(cmd->vrbits0, QVR_VRBITS0_MAINHAND_RELOADFLICKING,
            vr_right_reloadflicking);
        setBit(cmd->vrbits0, QVR_VRBITS0_MAINHAND_PREVRELOADFLICKING,
            vr_right_prevreloadflicking);
    }

    // VR: Hands.
    const auto computeHotSpot = [](const qvec3& hand)
    {
        if(VR_InShoulderHolsterDistance(hand, VR_GetLeftShoulderHolsterPos()))
        {
            return QVR_HS_LEFT_SHOULDER_HOLSTER;
        }

        if(VR_InShoulderHolsterDistance(hand, VR_GetRightShoulderHolsterPos()))
        {
            return QVR_HS_RIGHT_SHOULDER_HOLSTER;
        }

        if(VR_InHipHolsterDistance(hand, VR_GetLeftHipPos()))
        {
            return QVR_HS_LEFT_HIP_HOLSTER;
        }

        if(VR_InHipHolsterDistance(hand, VR_GetRightHipPos()))
        {
            return QVR_HS_RIGHT_HIP_HOLSTER;
        }

        if(VR_InUpperHolsterDistance(hand, VR_GetLeftUpperPos()))
        {
            return QVR_HS_LEFT_UPPER_HOLSTER;
        }

        if(VR_InUpperHolsterDistance(hand, VR_GetRightUpperPos()))
        {
            return QVR_HS_RIGHT_UPPER_HOLSTER;
        }

        if(hand == cl.handpos[cVR_OffHand] && VR_GoodDistanceForOffHand2HGrab())
        {
            return QVR_HS_OFFHAND_2H_GRAB;
        }

        if(hand == cl.handpos[cVR_MainHand] &&
            VR_GoodDistanceForMainHand2HGrab())
        {
            return QVR_HS_MAINHAND_2H_GRAB;
        }

        if(VR_GoodDistanceForHandSwitch(
               cl.handpos[cVR_OffHand], cl.handpos[cVR_MainHand]))
        {
            return QVR_HS_HAND_SWITCH;
        }

        return QVR_HS_NONE;
    };

    cl.hotspot[cVR_OffHand] = cmd->offhand_hotspot =
        computeHotSpot(cl.handpos[cVR_OffHand]);

    cl.hotspot[cVR_MainHand] = cmd->mainhand_hotspot =
        computeHotSpot(cl.handpos[cVR_MainHand]);

    if(inMenuOrConsole())
    {
        return;
    }

    auto [lfwd, lright, lup] = getAngledVectors(cl.handrot[cVR_OffHand]);

    if(vr_fakevr.value == 0)
    {
        if(vr_movement_mode.value == VrMovementMode::e_RAW_INPUT)
        {
            cmd->forwardmove += cl_forwardspeed.value * fwdMove;
            cmd->sidemove += cl_forwardspeed.value * sideMove;

            // TODO VR: (P1) avoid gimbal by using up if we are point up/down
            // like below, this is actually raw input but label in menu is
            // "Follow Head"
        }
        else
        {
            const auto [vfwd, vright, vup] = VR_GetHeadYawDirs();

            // avoid gimbal by using up if we are point up/down
            if(fabs(lfwd[2]) > 0.8f)
            {
                if(lfwd[2] < -0.8f)
                {
                    lfwd *= -1.f;
                }
                else
                {
                    lup *= -1.f;
                }

                std::swap(lup, lfwd);
            }

            // Scale up directions so tilting doesn't affect speed
            const float fac = 1.0f / lup[2];
            lfwd *= fac;
            lright *= fac;

            const qvec3 move =
                (qfloat(fwdMove) * lfwd) + (qfloat(sideMove) * lright);
            const auto fwd = DotProduct(move, vfwd);
            const auto right = DotProduct(move, vright);

            // Quake run doesn't affect the value of cl_sidespeed.value, so
            // just use forward speed here for consistency
            cmd->forwardmove += cl_forwardspeed.value * fwd;
            cmd->sidemove += cl_forwardspeed.value * right;
        }

        // roomscalemove:
        cmd->roomscalemove =
            vr_roomscale_move * static_cast<qfloat>(1._qf / host_frametime);

        std::tie(lfwd, lright, lup) = getAngledVectors(cl.handrot[cVR_OffHand]);
        cmd->upmove += cl_upspeed.value * fwdMove * lfwd[2];

        if((in_speed.state & 1) ^ (cl_alwaysrun.value == 0.0))
        {
            cmd->forwardmove *= cl_movespeedkey.value;
            cmd->sidemove *= cl_movespeedkey.value;
            cmd->upmove *= cl_movespeedkey.value;
        }
    }

    lastVrYawDiff = 0.f;

    if(vr_enable_joystick_turn.value == 1)
    {
        if(vr_snap_turn.value != 0)
        {
            static int lastSnap = 0;
            const int snap = yawMove > 0.0f ? 1 : yawMove < 0.0f ? -1 : 0;
            if(snap != lastSnap)
            {
                lastVrYawDiff = snap * vr_snap_turn.value;
                vrYaw -= lastVrYawDiff;
                lastSnap = snap;
            }
        }
        else
        {
            lastVrYawDiff =
                (yawMove * host_frametime * 100.0f) * vr_turn_speed.value;
            vrYaw -= lastVrYawDiff;
        }
    }
}

//
//
// PAK Stuff

[[nodiscard]] const std::string& VR_GetActiveStartPakName()
{
    const std::size_t idx = vr_activestartpaknameidx.value;
    const auto& vec = VR_GetLoadedPakNamesWithStartMaps();
    return vec[idx % vec.size()];
}

[[nodiscard]] std::vector<std::string>& VR_GetLoadedPakNames()
{
    static std::vector<std::string> res;
    return res;
}

[[nodiscard]] std::vector<std::string>& VR_GetLoadedPakNamesWithStartMaps()
{
    static std::vector<std::string> res;
    return res;
}

[[nodiscard]] std::string VR_ExtractPakName(std::string_view sv)
{
    const auto afterSlash = sv.find_last_of("/\\") + 1;
    sv = sv.substr(afterSlash, sv.size() - afterSlash);
    sv.remove_suffix(4);

    return std::string(sv.data(), sv.size());
}

[[nodiscard]] std::string VR_ExtractPakName(const pack_t& pak)
{
    return VR_ExtractPakName(pak.filename);
}

void VR_OnLoadedPak(pack_t& pak)
{
    const auto extractedName = VR_ExtractPakName(pak);

    VR_GetLoadedPakNames().emplace_back(extractedName);
    Con_Printf("Added pakfile to search paths: '%s'\n", extractedName.data());

    for(int i = 0; i < pak.numfiles; i++)
    {
        if(std::strcmp(pak.files[i].name, "maps/start.bsp") == 0)
        {
            VR_GetLoadedPakNamesWithStartMaps().emplace_back(extractedName);
            continue;
        }
    }
}

void VR_ResetThrowAvgFrames()
{
    controllers[0].velocityHistory = controllers[0].angularVelocityHistory =
        controllers[1].velocityHistory = controllers[1].angularVelocityHistory =
            VecHistory{static_cast<std::size_t>(vr_throw_avg_frames.value)};
}

// TODO VR: (P1): "seems like for the custom map a2 i can add bots with no
// problem, but on ab2 if i rapidly add them then the game crashes" - can
// reproduce, seems a problem with waypoint `._next`?

// TODO VR: (P1): "Quicksave would be better | A hotkey for quicksaving | Maybe
// a button combo"

// TODO VR: (P1): (MP) "I seem to be inheriting the server settings for a number
// of client side options, for instance I have immersive mode on and body-item
// interactions off, but these options were not applied whilst I was a client. I
// assume server has those settings set according to what I experienced? "

// TODO VR: (P1): (MP) "My axe had no physics collisions, was passing through
// walls etc, where as server reported he still had collsions"

// TODO VR: (P1): (MP) "Would be cool to render the full scoreboard on the
// wrist, or maybe the bottom of the hand? I think easy access to ping at least
// would be very useful to understand the context of MP bugs. Though my ping
// generally didn't feel too bad, likely under 100ms"

// TODO VR: (P1): (MP) "Jumping seemed very unreliable at times and fairly often
// just didn't trigger at all, much worse than the latency I was experiencing
// with weapons and general movement. "

// TODO VR: (P1): (MP) "An easy way to join games would be cool, for instance a
// numpad on the MP menu? Or a "Paste from clipboard" button, etc. "

// TODO VR: (P1): "In MP, would it be possible if when spawning and grab is
// already held, to automatically grab the weapon assigned to the corresponding
// leg slot?"

// TODO VR: (P1): "In general, would it be possible for touching weapon pickups
// to immediately give the ammo, so that it is not necessary to pickup the
// weapon itself?"

// TODO VR: (P1): When fullauto firing a nailgun in left hand, right hand
// weapon animations stop being lerped.

// TODO VR: (P1): "Ammo switching should not be possible in non-DOE levels
// since no ammo can be found?" "I might consider randomly spawning some,
// controlling the spawn rate with a CVar. Why not"

// TODO VR: (P1): plasma gun needs vrprojectilevelocity

// TODO VR: (P1) "Some weapons are so long that they physics collide with
// the floor to prevent your hand from touching health pickups, so you need
// to do an awkward wrist movement to allow your hand to near the health
// pickup. Seems most obvious with the rocket launcher. In my opinion held
// weapons should just collect pickups when colliding instead."

// TODO VR: (P1) "Perhaps the VR Body Interaction can be split into items /
// weapons? I much prefer the weapon pickup by hand, due to the inventory
// management aspect, where as items have no such concern"

// TODO VR: (P1) visual feedback when hovering holster, e.g. weapon comes
// out a bit, or glow, or bloom effect on vision edge (complementary to
// haptics)

// TODO VR: (P1) melee doesn't work with laser cannon - intended? test

// TODO VR: (P1): "I had let go of weapon grabs between a level end and next
// level start and upon next level spawn the main hand weapon was nowhere to
// be found, the offhand weapon was on the floor"

// TODO VR: (P1) recoil system, 2H will reduce it, or accuracy change for
// shotgun/ssg, reduce spread

// TODO VR: (P1) "Is there an option to not drop the weapon I'm holding when
// I release the grip button?"

// TODO VR: (P1) add system to load multiple folders at once (like paks)

// TODO VR: (P1) Swimming direction - what controller to follow?

// TODO VR: (P1) "no one wants to be faced with giant space shuttle like
// control panels full of semi-cryptic technical parameters to tweak right
// before one can start playing. That's just generally means the developer
// offloads the burden of setting the right defaults for many variables to
// the user, and the user experience suffers in consequence. "
//
// "These games are best enjoyed as plug and play experiences. And having to
// tweak various multipliers for all sorts of obscure parameters usually
// ruins that."

// TODO VR: (P1) "hey i just was wondering how to use the soundtrack for
// mission pack 1? do i need to swap out the original soundtrack before
// launching? "

// TODO VR: (P1): "If you're standing on a ledge and try to force grab an
// item below, it seems like you need to position your body over the ledge
// specifically so that a specific point of your body has a direct line of
// sight to the weapon" - this might be related to water

// TODO VR: (P1): lightning bolt tent beam is lagging behind the player,
// why?

// TODO VR: (P1): "also config is being rewritten
//
// Yes this is a bit annoying, though unavoidable in the case that new
// variables are being added I guess. Unless there's a way to either:
//
// A) preserve user overrides in a file separate to config.cfg, I think this
// might be possible with autoexec.cfg or something though I don't think
// that is integrated into the game itself at all and we need to manually
// maintain it? B) could we stop shipping config.cfg with each release and
// instead it populates missing variables on load with a sane default
// instead? Debatable depending on how mature @vee feels the project is
//
// I guess. I personally need to go and alter about 3-4 variables each
// release so it's manage-able for now."

// TODO VR: (P1): "- Is it normal for the HUD in DOE to not show alternate ammo
// counts unless the weapon is specifically selected and the alt ammo active?"

// TODO VR: (P1): joining a multiplayer match with mismatched `start` map causes
// weird glitches as the BSP geometry is incorrect

// TODO VR: (P1): are things like holster placement affected by server configs
// in MP? Check

// TODO VR: (P1): "I had a feeling that my controllers had also vibrations from
// my colleages game activity" in online mp

// TODO VR: (P1): "I found that if you force-grab a BSP item (ammo, health) and
// you have more than you can carry, when it drops to the floor it becomes solid
// and you can walk on it."

// TODO VR: (P1) Loading inexistent map crashes the game!



// TODO VR: (P2) add tooltip to off-hand option menu in wpn config

// TODO VR: (P2) "it seems to be a bit strange to me that I can hold down
// the trigger on the shotguns"

// TODO VR: (P2) consider adding ghost hands

// TODO VR: (P2) scourge of armagon music?

// TODO VR: (P2) consider new particle effect for shootable weapons and
// walls instead of blood

// TODO VR: (P2) add general cvars for health and damage multipliers

// TODO VR: (P2) add option to disable ogre mirvs?

// TODO VR: (P2) add option to pause game on SteamVR dash open

// TODO VR: (P2) immersive swimming

// TODO VR: (P2): "the problem with turning up the QuakeVR particle
// system is that beyond a certain point it all starts to overlap. It would
// be nice if the value also somewhat increased the spread or distance of
// the particles too, just to make it a bit messier"

// TODO VR: (P2) "The force grab seems a bit strange to me though, requiring
// me to press both the trigger and grip at once, then it doesn't quite come
// right to my hand, and I have to then release the buttons and then press
// grip again to finally grab it. Could do with a bit of a tweak, and
// obviously Half-Life: Alyx is the perfect example to rip off here."

// TODO VR: (P2) "Also, snap turning seems to have a tiny bit of lag and/or
// not work some of the time. And I don't mean because of any kind of dead
// zone as I've tried that at different settings already. Maybe just double
// check there's not a bit of a code conflict, like maybe if you flick once
// you can't quite flick immediately again in some cases or something."

// TODO VR: (P2) "Unfortunately, since the game doesn't differentiate from
// the mission packs, it uses the same soundtrack for all of them (eg if you
// were playing SOA, it would still play the original quake soundtrack
// unless you replaced the files manually)"

// TODO VR: (P2) "Hopefully that can be fixed, along with merging the start
// map together into one giant hubworld"

// TODO VR: (P2) "The lava splash noise just seems plain wrong, too watery.
// Even silent would be better, in the case of the molten rocks"

// TODO VR: (P2) "Perhaps its just VR thats making me notice it, but the
// stopping friction after running just feels too light and floaty"

// TODO VR: (P2) "In some situations force grab linear is being very
// unreliable, whereas others its fine. Is force grab code unchanged in
// 0.0.5 or is it helpful to try and replicate? I was having most issues
// when holding with right hand and trying to force grab with left."

// TODO VR: (P2): " hint messages for each of the weapons when they are held for
// the first time in a new game?"

// TODO VR: (P2): " I think making it work pretty much just like in Half-Life:
// Alyx would be ideal: So, you would press the controller's grip button while
// pointing at the object to activate the "force" (indicate this is happening to
// the player somehow), do a flick/pull motion toward you to flick the object to
// you (if you don't flick and instead release the button then it would
// deactivate the "force"), and then, in the case of this particular game, it
// could probably automatically grab the object once it reaches your hand (no
// additional press of the grip button required). Something like that would
// probably work: Press grip, flick/pull motion toward you, and you auto-grab it
// once it reaches your hand."

// TODO VR: (P2): "On thing that I think that would be a quality of life
// improvement are quicksave/quickload entries at the top of the pause menu.
// When I was streaming the game, I meant to save the game, but when I went to
// go do that, my itchy button thumb accidentally started a new game."

// TODO VR: (P2): "the desktop display has a bit of it cut off from what looks
// like the console"
