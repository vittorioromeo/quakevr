#include "quakedef.hpp"
#include "vr.hpp"
#include "vr_menu.hpp"
#include "util.hpp"
#include "openvr.hpp"

#define GLM_FORCE_INLINE
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <gtx/quaternion.hpp>
#include <gtx/euler_angles.hpp>

#include <algorithm>
#include <cassert>
#include <vector>
#include <tuple>
#include <string>

#ifdef WIN32
#define UNICODE 1
#include <mmsystem.h>
#undef UNICODE
#endif

//
//
//
// ----------------------------------------------------------------------------
// Utilities
// ----------------------------------------------------------------------------

using quake::util::lerp;
using quake::util::toQuakeVec3;
using quake::util::toVec3;
using quake::util::vec3lerp;

//
//
//
// ----------------------------------------------------------------------------
// VR Rendering Structs
// ----------------------------------------------------------------------------

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

struct vr_controller
{
    vr::VRControllerState_t state;
    vr::VRControllerState_t lastState;
    vec3_t position;
    vec3_t orientation;
    vec3_t velocity;
    vr::HmdVector3_t rawvector;
    vr::HmdQuaternion_t raworientation;
};

//
//
//
// ----------------------------------------------------------------------------
// OpenGL Extensions
// ----------------------------------------------------------------------------

#define GL_READ_FRAMEBUFFER_EXT 0x8CA8
#define GL_DRAW_FRAMEBUFFER_EXT 0x8CA9
#define GL_FRAMEBUFFER_SRGB_EXT 0x8DB9

typedef void(APIENTRYP PFNGLBLITFRAMEBUFFEREXTPROC)(
    GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
typedef bool(APIENTRYP PFNWGLSWAPINTERVALEXTPROC)(int);

static PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT;
static PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glCheckFramebufferStatusEXT;
static PFNGLBLITFRAMEBUFFEREXTPROC glBlitFramebufferEXT;
static PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffersEXT;
static PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT;
static PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT;
static PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT;
static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
static PFNGLTEXIMAGE2DMULTISAMPLEPROC glTexImage2DMultisampleEXT;

struct
{
    void* func;
    const char* name;
} gl_extensions[] = {
    {&glBindFramebufferEXT, "glBindFramebufferEXT"},
    {&glBlitFramebufferEXT, "glBlitFramebufferEXT"},
    {&glDeleteFramebuffersEXT, "glDeleteFramebuffersEXT"},
    {&glGenFramebuffersEXT, "glGenFramebuffersEXT"},
    {&glTexImage2DMultisampleEXT, "glTexImage2DMultisample"},
    {&glFramebufferTexture2DEXT, "glFramebufferTexture2DEXT"},
    {&glFramebufferRenderbufferEXT, "glFramebufferRenderbufferEXT"},
    {&glCheckFramebufferStatusEXT, "glCheckFramebufferStatusEXT"},
    {&wglSwapIntervalEXT, "wglSwapIntervalEXT"},
    {nullptr, nullptr},
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
// TODO VR
// ----------------------------------------------------------------------------

static float vrYaw;
static bool readbackYaw;

std::string vr_working_directory;

vec3_t vr_viewOffset;
glm::vec3 lastHudPosition{};
glm::vec3 lastMenuPosition{};

vr::IVRSystem* ovrHMD;
vr::TrackedDevicePose_t ovr_DevicePose[vr::k_unMaxTrackedDeviceCount];

static vr_eye_t eyes[2];
static vr_eye_t* current_eye = nullptr;
static vr_controller controllers[2];
static vec3_t lastOrientation = {0, 0, 0};
static vec3_t lastAim = {0, 0, 0};

static bool vr_initialized = false;

static vec3_t headOrigin;
static vec3_t lastHeadOrigin;
static vr::HmdVector3_t headPos;
static vr::HmdVector3_t headVelocity;

vec3_t vr_room_scale_move;

// Wolfenstein 3D, DOOM and QUAKE use the same coordinate/unit system:
// 8 foot (96 inch) height wall == 64 units, 1.5 inches per pixel unit
// 1.0 pixel unit / 1.5 inch == 0.666666 pixel units per inch
#define meters_to_units (vr_world_scale.value / (1.5f * 0.0254f))

extern cvar_t gl_farclip;


//
//
//
// ----------------------------------------------------------------------------
// VR CVar ition and Registration
// ----------------------------------------------------------------------------

static std::vector<cvar_t*> cvarsToRegister;

#define DEFINE_CVAR(name, defaultValue, type)         \
    cvar_t name = {#name, #defaultValue, type};       \
    static struct _cvar_registrar##name##__LINE__##_t \
    {                                                 \
        _cvar_registrar##name##__LINE__##_t()         \
        {                                             \
            cvarsToRegister.emplace_back(&name);      \
        }                                             \
    } _cvar_registrar##name##__LINE__

DEFINE_CVAR(vr_enabled, 0, CVAR_NONE);
DEFINE_CVAR(vr_viewkick, 0, CVAR_NONE);
DEFINE_CVAR(vr_lefthanded, 0, CVAR_NONE);
DEFINE_CVAR(vr_crosshair, 1, CVAR_ARCHIVE);
DEFINE_CVAR(vr_crosshair_depth, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_crosshair_size, 3.0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_crosshair_alpha, 0.25, CVAR_ARCHIVE);
DEFINE_CVAR(vr_aimmode, 7, CVAR_ARCHIVE);
DEFINE_CVAR(vr_deadzone, 30, CVAR_ARCHIVE);
DEFINE_CVAR(vr_gunangle, 32, CVAR_ARCHIVE);
DEFINE_CVAR(vr_gunmodelpitch, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_gunmodelscale, 1.0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_gunmodely, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_crosshairy, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_world_scale, 1.0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_floor_offset, -16, CVAR_ARCHIVE);
DEFINE_CVAR(vr_snap_turn, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_enable_joystick_turn, 1, CVAR_ARCHIVE);
DEFINE_CVAR(vr_turn_speed, 1, CVAR_ARCHIVE);
DEFINE_CVAR(vr_msaa, 4, CVAR_ARCHIVE);
DEFINE_CVAR(vr_movement_mode, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_hud_scale, 0.025, CVAR_ARCHIVE);
DEFINE_CVAR(vr_menu_scale, 0.13, CVAR_ARCHIVE);
DEFINE_CVAR(vr_melee_threshold, 7, CVAR_ARCHIVE);
DEFINE_CVAR(vr_gunyaw, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_gun_z_offset, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_sbar_mode, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_sbar_offset_x, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_sbar_offset_y, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_sbar_offset_z, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_sbar_offset_pitch, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_sbar_offset_yaw, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_sbar_offset_roll, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_roomscale_jump, 1, CVAR_ARCHIVE);
DEFINE_CVAR(vr_height_calibration, 1.6, CVAR_ARCHIVE);
DEFINE_CVAR(vr_roomscale_jump_threshold, 1.0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_menu_distance, 76, CVAR_ARCHIVE);
DEFINE_CVAR(vr_melee_dmg_multiplier, 1.0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_melee_range_multiplier, 1.0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_body_interactions, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_room_scale_move_mult, 1.0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_teleport_enabled, 1, CVAR_ARCHIVE);
DEFINE_CVAR(vr_teleport_range, 400, CVAR_ARCHIVE);

//
//
//
// ----------------------------------------------------------------------------
// VR Rendering
// ----------------------------------------------------------------------------

[[nodiscard]] static bool InitOpenGLExtensions()
{
    static bool extensions_initialized;

    if(extensions_initialized)
    {
        return true;
    }

    for(int i = 0; gl_extensions[i].func; ++i)
    {
        void* func = SDL_GL_GetProcAddress(gl_extensions[i].name);
        if(!func)
        {
            return false;
        }

        *((void**)gl_extensions[i].func) = func;
    }

    extensions_initialized = true;
    return extensions_initialized;
}

void RecreateTextures(fbo_t* const fbo, const int width, const int height)
{
    GLuint oldDepth = fbo->depth_texture;
    GLuint oldTexture = fbo->texture;

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

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo->framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
        GL_TEXTURE_2D, fbo->texture, 0);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
        GL_TEXTURE_2D, fbo->depth_texture, 0);
}

[[nodiscard]] fbo_t CreateFBO(const int width, const int height)
{
    fbo_t fbo;

    glGenFramebuffersEXT(1, &fbo.framebuffer);

    fbo.depth_texture = 0;

    RecreateTextures(&fbo, width, height);

    fbo.msaa = 0;
    fbo.msaa_framebuffer = 0;
    fbo.msaa_texture = 0;

    return fbo;
}

void CreateMSAA(
    fbo_t* const fbo, const int width, const int height, const int msaa)
{
    fbo->msaa = msaa;

    if(fbo->msaa_framebuffer)
    {
        glDeleteFramebuffersEXT(1, &fbo->msaa_framebuffer);
        glDeleteTextures(1, &fbo->msaa_texture);
        glDeleteTextures(1, &fbo->msaa_depth_texture);
    }

    glGenFramebuffersEXT(1, &fbo->msaa_framebuffer);
    glGenTextures(1, &fbo->msaa_texture);
    glGenTextures(1, &fbo->msaa_depth_texture);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, fbo->msaa_texture);
    glTexImage2DMultisampleEXT(
        GL_TEXTURE_2D_MULTISAMPLE, msaa, GL_RGBA8, width, height, false);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, fbo->msaa_depth_texture);
    glTexImage2DMultisampleEXT(GL_TEXTURE_2D_MULTISAMPLE, msaa,
        GL_DEPTH_COMPONENT24, width, height, false);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo->msaa_framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
        GL_TEXTURE_2D_MULTISAMPLE, fbo->msaa_texture, 0);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
        GL_TEXTURE_2D_MULTISAMPLE, fbo->msaa_depth_texture, 0);

    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if(status != GL_FRAMEBUFFER_COMPLETE)
    {
        Con_Printf("Framebuffer incomplete %x", status);
    }
}

// TODO VR: never called
void DeleteFBO(const fbo_t& fbo)
{
    glDeleteFramebuffersEXT(1, &fbo.framebuffer);
    glDeleteTextures(1, &fbo.depth_texture);
    glDeleteTextures(1, &fbo.texture);
}

[[nodiscard]] glm::vec3 QuatToYawPitchRoll(vr::HmdQuaternion_t q)
{
    const auto sqw = q.w * q.w;
    const auto sqx = q.x * q.x;
    const auto sqy = q.y * q.y;
    const auto sqz = q.z * q.z;

    glm::vec3 out;

    out[ROLL] = -atan2(2 * (q.x * q.y + q.w * q.z), sqw - sqx + sqy - sqz) /
                M_PI_DIV_180;
    out[PITCH] = -asin(-2 * (q.y * q.z - q.w * q.x)) / M_PI_DIV_180;
    out[YAW] = atan2(2 * (q.x * q.z + q.w * q.y), sqw - sqx - sqy + sqz) /
                   M_PI_DIV_180 +
               vrYaw;

    return out;
}

void Vec3RotateZ(vec3_t in, float angle, vec3_t out)
{
    out[0] = in[0] * cos(angle) - in[1] * sin(angle);
    out[1] = in[0] * sin(angle) + in[1] * cos(angle);
    out[2] = in[2];
}

[[nodiscard]] vr::HmdMatrix44_t TransposeMatrix(const vr::HmdMatrix44_t& in)
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
    const vr::HmdVector3_t& a, const vr::HmdVector3_t& b)
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
    const vr::HmdVector3_t& v, const vr::HmdQuaternion_t& q)
{
    vr::HmdVector3_t u;

    vr::HmdVector3_t result;
    u.v[0] = q.x;
    u.v[1] = q.y;
    u.v[2] = q.z;
    float s = q.w;

    // Dot products of u,v and u,u
    float uvDot = (u.v[0] * v.v[0] + u.v[1] * v.v[1] + u.v[2] * v.v[2]);
    float uuDot = (u.v[0] * u.v[0] + u.v[1] * u.v[1] + u.v[2] * u.v[2]);

    // Calculate cross product of u, v
    vr::HmdVector3_t uvCross;
    uvCross.v[0] = u.v[1] * v.v[2] - u.v[2] * v.v[1];
    uvCross.v[1] = u.v[2] * v.v[0] - u.v[0] * v.v[2];
    uvCross.v[2] = u.v[0] * v.v[1] - u.v[1] * v.v[0];

    // Calculate each vectors' result individually because there aren't
    // arthimetic functions for HmdVector3_t dsahfkldhsaklfhklsadh
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
[[nodiscard]] vr::HmdVector3_t Matrix34ToVector(const vr::HmdMatrix34_t& in)
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
    const vr::HmdMatrix34_t& in)
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

void HmdVec3RotateY(vr::HmdVector3_t* const pos, const float angle)
{
    const float s = sin(angle);
    const float c = cos(angle);
    const float x = c * pos->v[0] - s * pos->v[2];
    const float y = s * pos->v[0] + c * pos->v[2];

    pos->v[0] = x;
    pos->v[2] = y;
}

//
//
//
// ----------------------------------------------------------------------------
// VR CVar Callbacks
// ----------------------------------------------------------------------------

static void VR_Enabled_f(cvar_t* var)
{
    (void)var;

    VID_VR_Disable();

    if(!vr_enabled.value)
    {
        return;
    }

    if(!VR_Enable())
    {
        Cvar_SetValueQuick(&vr_enabled, 0);
    }
}

static void VR_Deadzone_f(cvar_t* var)
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

// TODO VR: not sure what this number should actually be...
enum
{
    e_MAX_WEAPONS = 20
};

cvar_t
    vr_weapon_offset[e_MAX_WEAPONS * static_cast<std::size_t>(WpnCVar::k_Max)];

aliashdr_t* lastWeaponHeader;
int weaponCVarEntry;

[[nodiscard]] float VR_GetScaleCorrect() noexcept
{
    // Initial version had 0.75 default world scale, so weapons reflect that.
    return (vr_world_scale.value / 0.75f) * vr_gunmodelscale.value;
}

void ApplyMod_Weapon(const int cvarEntry, aliashdr_t* const hdr)
{
    const float scaleCorrect = VR_GetScaleCorrect();

    VectorScale(hdr->original_scale,
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::Scale) * scaleCorrect,
        hdr->scale);

    const auto [ox, oy, oz] = VR_GetWpnOffsets(cvarEntry);
    vec3_t ofs{ox, oy, oz};

    VectorAdd(hdr->original_scale_origin, ofs, hdr->scale_origin);
    VectorScale(hdr->scale_origin, scaleCorrect, hdr->scale_origin);
}

void Mod_Weapon(const char* name, aliashdr_t* hdr)
{
    if(lastWeaponHeader != hdr)
    {
        lastWeaponHeader = hdr;
        weaponCVarEntry = -1;

        for(int i = 0; i < e_MAX_WEAPONS; i++)
        {
            if(!strcmp(VR_GetWpnCVar(i, WpnCVar::ID).string, name))
            {
                weaponCVarEntry = i;
                break;
            }
        }

        if(weaponCVarEntry == -1)
        {
            Con_Printf("No VR offset for weapon: %s\n", name);
        }
    }

    if(weaponCVarEntry != -1)
    {
        ApplyMod_Weapon(weaponCVarEntry, hdr);
    }
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

[[nodiscard]] int VR_GetOffHandFistCvarEntry() noexcept
{
    // TODO VR: hardcoded hand/fist cvar number
    return 16;
}

[[nodiscard]] WeaponOffsets VR_GetWpnOffsets(const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::OffsetX),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::OffsetY),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::OffsetZ) + vr_gunmodely.value};
}

[[nodiscard]] WeaponAngleOffsets VR_GetWpnAngleOffsets(
    const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::Pitch) + vr_gunmodelpitch.value,
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::Yaw),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::Roll)};
}

[[nodiscard]] WeaponAngleOffsets VR_GetWpnMuzzleOffsets(
    const int cvarEntry) noexcept
{
    return {//
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::MuzzleOffsetX),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::MuzzleOffsetY),
        VR_GetWpnCVarValue(cvarEntry, WpnCVar::MuzzleOffsetZ)};
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


void InitWeaponCVars(int i, const char* id, const char* offsetX,
    const char* offsetY, const char* offsetZ, const char* scale,
    const char* roll = "0.0", const char* pitch = "0.0",
    const char* yaw = "0.0", const char* muzzleOffsetX = "0.0",
    const char* muzzleOffsetY = "0.0", const char* muzzleOffsetZ = "0.0")
{
    // clang-format off
    constexpr const char* nameOffsetX = "vr_wofs_x_nn";
    constexpr const char* nameOffsetY = "vr_wofs_y_nn";
    constexpr const char* nameOffsetZ = "vr_wofs_z_nn";
    constexpr const char* nameScale = "vr_wofs_scale_nn";
    constexpr const char* nameID = "vr_wofs_id_nn";
    constexpr const char* nameRoll = "vr_wofs_roll_nn";
    constexpr const char* namePitch = "vr_wofs_pitch_nn";
    constexpr const char* nameYaw = "vr_wofs_yaw_nn";
    constexpr const char* nameMuzzleOffsetX = "vr_wofs_muzzle_x_nn";
    constexpr const char* nameMuzzleOffsetY = "vr_wofs_muzzle_y_nn";
    constexpr const char* nameMuzzleOffsetZ = "vr_wofs_muzzle_z_nn";

    InitWeaponCVar(VR_GetWpnCVar(i, WpnCVar::OffsetX), nameOffsetX, i, offsetX);
    InitWeaponCVar(VR_GetWpnCVar(i, WpnCVar::OffsetY), nameOffsetY, i, offsetY);
    InitWeaponCVar(VR_GetWpnCVar(i, WpnCVar::OffsetZ), nameOffsetZ, i, offsetZ);
    InitWeaponCVar(VR_GetWpnCVar(i, WpnCVar::Scale), nameScale, i, scale);
    InitWeaponCVar(VR_GetWpnCVar(i, WpnCVar::ID), nameID, i, id);
    InitWeaponCVar(VR_GetWpnCVar(i, WpnCVar::Pitch), nameRoll, i, roll); // TODO VR: mismatch
    InitWeaponCVar(VR_GetWpnCVar(i, WpnCVar::Roll), namePitch, i, pitch); // TODO VR: mismatch
    InitWeaponCVar(VR_GetWpnCVar(i, WpnCVar::Yaw), nameYaw, i, yaw); // TODO VR: mismatch
    InitWeaponCVar(VR_GetWpnCVar(i, WpnCVar::MuzzleOffsetX), nameMuzzleOffsetX, i, muzzleOffsetX);
    InitWeaponCVar(VR_GetWpnCVar(i, WpnCVar::MuzzleOffsetY), nameMuzzleOffsetY, i, muzzleOffsetY);
    InitWeaponCVar(VR_GetWpnCVar(i, WpnCVar::MuzzleOffsetZ), nameMuzzleOffsetZ, i, muzzleOffsetZ);
    // clang-format on
}

// TODO VR: get rid of this
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
    InitWeaponCVars(i++, "progs/hand.mdl", "0.0", "0.0", "0.0", "0.0");

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

void VID_VR_Init()
{
    // This is only called once at game start
    Cvar_SetCallback(&vr_enabled, VR_Enabled_f);
    Cvar_SetCallback(&vr_deadzone, VR_Deadzone_f);

    for(cvar_t* c : cvarsToRegister)
    {
        Cvar_RegisterVariable(c);
    }

    InitAllWeaponCVars();

    // VR: Fix grenade model flags to enable smoke trail.
    qmodel_t* test = Mod_ForName("progs/grenade.mdl", true);
    test->flags |= EF_GRENADE;

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

//
//
//
// ----------------------------------------------------------------------------
// VR Action Handles
// ----------------------------------------------------------------------------

vr::VRActiveActionSet_t vrActiveActionSet;

vr::VRActionSetHandle_t vrashDefault;

vr::VRActionHandle_t vrahLocomotion;
vr::VRActionHandle_t vrahTurn;
vr::VRActionHandle_t vrahFire;
vr::VRActionHandle_t vrahJump;
vr::VRActionHandle_t vrahPrevWeapon;
vr::VRActionHandle_t vrahNextWeapon;
vr::VRActionHandle_t vrahEscape;
vr::VRActionHandle_t vrahSpeed;
vr::VRActionHandle_t vrahTeleport;
vr::VRActionHandle_t vrahLeftHaptic;
vr::VRActionHandle_t vrahRightHaptic;

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
            Con_Printf(
                "Failed to read Steam VR action set handle, rc = %d", (int)rc);
        }
    }

    // -----------------------------------------------------------------------
    // VR: Read all action handles.
    const auto readHandle = [](const char* name, vr::VRActionHandle_t& handle) {
        const auto rc = vr::VRInput()->GetActionHandle(name, &handle);

        if(rc != vr::EVRInputError::VRInputError_None)
        {
            Con_Printf(
                "Failed to read Steam VR action handle, rc = %d", (int)rc);
        }
    };

    readHandle("/actions/default/in/Locomotion", vrahLocomotion);
    readHandle("/actions/default/in/Turn", vrahTurn);
    readHandle("/actions/default/in/Fire", vrahFire);
    readHandle("/actions/default/in/Jump", vrahJump);
    readHandle("/actions/default/in/PrevWeapon", vrahPrevWeapon);
    readHandle("/actions/default/in/NextWeapon", vrahNextWeapon);
    readHandle("/actions/default/in/Escape", vrahEscape);
    readHandle("/actions/default/in/Speed", vrahSpeed);
    readHandle("/actions/default/in/Teleport", vrahTeleport);
    readHandle("/actions/default/out/LeftHaptic", vrahLeftHaptic);
    readHandle("/actions/default/out/RightHaptic", vrahRightHaptic);

    vrActiveActionSet.ulActionSet = vrashDefault;
    vrActiveActionSet.ulRestrictedToDevice = vr::k_ulInvalidInputValueHandle;
    vrActiveActionSet.nPriority = 0;

    // -----------------------------------------------------------------------
    // VR: Get handles to the controllers.
    const auto readInputSourceHandle = [](const char* name,
                                           vr::VRInputValueHandle_t& handle) {
        const auto rc = vr::VRInput()->GetInputSourceHandle(name, &handle);

        if(rc != vr::EVRInputError::VRInputError_None)
        {
            Con_Printf("Failed to read Steam VR input source handle, rc = %d",
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
// TODO VR
// ----------------------------------------------------------------------------

bool VR_Enable()
{
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
                "Failed to read Steam VR action manifest, rc = %d", (int)rc);
        }
        else
        {
            VR_InitActionHandles();
        }
    }

    if(!InitOpenGLExtensions())
    {
        Con_Printf("Failed to initialize OpenGL extensions");
        return false;
    }

    eyes[0].eye = vr::Eye_Left;
    eyes[1].eye = vr::Eye_Right;

    for(int i = 0; i < 2; i++)
    {
        uint32_t vrwidth;

        uint32_t vrheight;
        float LeftTan;

        float RightTan;

        float UpTan;

        float DownTan;

        ovrHMD->GetRecommendedRenderTargetSize(&vrwidth, &vrheight);
        ovrHMD->GetProjectionRaw(
            eyes[i].eye, &LeftTan, &RightTan, &UpTan, &DownTan);

        eyes[i].index = i;
        eyes[i].fbo = CreateFBO(vrwidth, vrheight);
        eyes[i].fov_x = (atan(-LeftTan) + atan(RightTan)) / float(M_PI_DIV_180);
        eyes[i].fov_y = (atan(-UpTan) + atan(DownTan)) / float(M_PI_DIV_180);
    }

    // TODO VR: seated mode?
    // Put us into standing tracking position
    vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseStanding);

    VR_ResetOrientation(); // Recenter the HMD

    wglSwapIntervalEXT(0); // Disable V-Sync

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
    cl.viewheight = DEFAULT_VIEWHEIGHT;

    // TODO: Cleanup frame buffers

    vr_initialized = false;
}

static void RenderScreenForCurrentEye_OVR()
{
    assert(current_eye != nullptr);

    // Remember the current glwidht/height; we have to modify it here for
    // each eye
    int oldglheight = glheight;
    int oldglwidth = glwidth;

    uint32_t cglwidth = glwidth;
    uint32_t cglheight = glheight;
    ovrHMD->GetRecommendedRenderTargetSize(&cglwidth, &cglheight);
    glwidth = cglwidth;
    glheight = cglheight;

    bool newTextures = glwidth != current_eye->fbo.size.width ||
                       glheight != current_eye->fbo.size.height;
    if(newTextures)
    {
        RecreateTextures(&current_eye->fbo, glwidth, glheight);
    }

    if(newTextures || vr_msaa.value != current_eye->fbo.msaa)
    {
        CreateMSAA(&current_eye->fbo, glwidth, glheight, vr_msaa.value);
    }

    // Set up current FBO
    if(current_eye->fbo.msaa > 0)
    {
        glEnable(GL_MULTISAMPLE);
        glBindFramebufferEXT(
            GL_FRAMEBUFFER_EXT, current_eye->fbo.msaa_framebuffer);
    }
    else
    {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, current_eye->fbo.framebuffer);
    }

    glViewport(0, 0, current_eye->fbo.size.width, current_eye->fbo.size.height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw everything
    srand((int)(cl.time * 1000)); // sync random stuff between eyes

    r_refdef.fov_x = current_eye->fov_x;
    r_refdef.fov_y = current_eye->fov_y;

    SCR_UpdateScreenContent();

    // Generate the eye texture and send it to the HMD

    if(current_eye->fbo.msaa > 0)
    {
        glDisable(GL_MULTISAMPLE);
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, current_eye->fbo.framebuffer);
        glBindFramebufferEXT(
            GL_READ_FRAMEBUFFER, current_eye->fbo.msaa_framebuffer);
        glDrawBuffer(GL_BACK);
        glBlitFramebufferEXT(0, 0, glwidth, glheight, 0, 0, glwidth, glheight,
            GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    vr::Texture_t eyeTexture = {
        reinterpret_cast<void*>(uintptr_t(current_eye->fbo.texture)),
        vr::TextureType_OpenGL, vr::ColorSpace_Gamma};
    vr::VRCompositor()->Submit(current_eye->eye, &eyeTexture);

    // Reset
    glwidth = oldglwidth;
    glheight = oldglheight;

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

// Get a reasonable height around where hands should be when aiming a gun.
[[nodiscard]] float VR_GetHandZOrigin(entity_t* player) noexcept
{
    return player->origin[2] + vr_floor_offset.value + vr_gun_z_offset.value;
}

// Get the player origin vector, but adjusted to the upper torso on the Z axis.
void VR_GetAdjustedPlayerOrigin(vec3_t out, entity_t* player) noexcept
{
    VectorCopy(player->origin, out);
    out[2] = VR_GetHandZOrigin(player) + 40;
}

void SetHandPos(int index, entity_t* player)
{
    // -----------------------------------------------------------------------
    // VR: Figure out position of hand controllers in the game world.

    vec3_t headLocalPreRot;
    _VectorSubtract(controllers[index].position, headOrigin, headLocalPreRot);

    vec3_t headLocal;
    Vec3RotateZ(headLocalPreRot, vrYaw * M_PI_DIV_180, headLocal);
    _VectorAdd(headLocal, headOrigin, headLocal);

    vec3_t finalPre, finalVec;

    const auto handZOrigin = VR_GetHandZOrigin(player);

    finalPre[0] = -headLocal[0] + player->origin[0];
    finalPre[1] = -headLocal[1] + player->origin[1];
    finalPre[2] = headLocal[2] + handZOrigin;

    // -----------------------------------------------------------------------
    // VR: Detect & resolve hand collisions against the world or entities.

    // Size of hand hitboxes.
    vec3_t mins{-1.f, -1.f, -1.f};
    vec3_t maxs{1.f, 1.f, 1.f};

    // Start around the upper torso, not actual center of the player.
    vec3_t adjPlayerOrigin;
    VR_GetAdjustedPlayerOrigin(adjPlayerOrigin, player);

    // Trace from upper torso to desired final location. `SV_Move` detects
    // entities as well, not just geometry.
    const trace_t trace =
        SV_Move(adjPlayerOrigin, mins, maxs, finalPre, MOVE_NORMAL, sv_player);

    // Origin of the trace.
    const auto orig = quake::util::toVec3(adjPlayerOrigin);

    // Final position before collision resolution.
    const auto pre = quake::util::toVec3(finalPre);

    // Final position after full collision resolution.
    const auto crop = quake::util::toVec3(trace.endpos);

    // Compute final collision resolution position, starting from the desired
    // position and resolving only against the collision plane's normal vector.
    VectorCopy(finalPre, finalVec);
    if(glm::length(pre - orig) >= glm::length(crop - orig))
    {
        VectorCopy(finalPre, finalVec);
        for(int i = 0; i < 3; ++i)
        {
            if(trace.plane.normal[i] != 0)
            {
                finalVec[i] = trace.endpos[i];
            }
        }
    }

    // handpos
    const auto oldx = cl.handpos[index][0];
    const auto oldy = cl.handpos[index][1];
    const auto oldz = cl.handpos[index][2];
    VectorCopy(finalVec, cl.handpos[index]);

    // TODO VR: adjust weight and add cvar, fix movement
    if(false)
    {
        cl.handpos[index][0] = lerp(oldx, finalVec[0], 0.05f);
        cl.handpos[index][1] = lerp(oldy, finalVec[1], 0.05f);
        cl.handpos[index][2] = lerp(oldz, finalVec[2], 0.05f);
    }

    // handrot is set with AngleVectorFromRotMat

    // handvel
    VectorCopy(controllers[index].velocity, cl.handvel[index]);

    // handvelmag
    // VR: This helps direct punches being registered. This calculation works
    // because the controller velocity is always absolute (not oriented where
    // the player is looking).
    // TODO VR: this still needs to be oriented to the headset's rotation,
    // otherwise diagonal punches will still not register.
    const auto length = VectorLength(controllers[index].velocity);
    const auto bestSingle = std::max({std::abs(controllers[index].velocity[0]),
                                std::abs(controllers[index].velocity[1]),
                                std::abs(controllers[index].velocity[2])}) *
                            1.75f;
    cl.handvelmag[index] = std::max(length, bestSingle);
}

// TODO VR:
bool vr_was_teleporting = false;
bool vr_teleporting = false;
bool vr_teleporting_impact_valid = false;
bool vr_send_teleport_msg = false;
vec3_t vr_teleporting_impact{0, 0, 0};

void VR_DoTeleportation()
{
    if(!vr_teleport_enabled.value)
    {
        return;
    }

    entity_t* player = &cl_entities[cl.viewentity];

    if(vr_teleporting)
    {
        constexpr float oh = 6.f;
        constexpr float oy = 12.f;
        vec3_t mins{-oh, -oh, -oy};
        vec3_t maxs{oh, oh, oy};

        vec3_t forward, right, up;
        AngleVectors(cl.handrot[0], forward, right, up);

        vec3_t target;
        VectorMA(cl.handpos[0], vr_teleport_range.value, forward, target);

        vec3_t adjPlayerOrigin;
        VR_GetAdjustedPlayerOrigin(adjPlayerOrigin, player);

        const trace_t trace = SV_Move(
            adjPlayerOrigin, mins, maxs, target, MOVE_NORMAL, sv_player);

        const auto between = [](const float value, const float min,
                                 const float max) {
            return value >= min && value <= max;
        };

        // TODO VR:
        // Con_Printf("%.2f, %.2f, %.2f\n", trace.plane.normal[0],
        //    trace.plane.normal[1], trace.plane.normal[2]);

        // Allow slopes, but not walls or ceilings.
        const bool
            goodNormal = //
                         // between(trace.plane.normal[0], -0.15f, 0.15f) && //
                         // between(trace.plane.normal[1], -0.15f, 0.15f) && //
            between(trace.plane.normal[2], 0.75f, 1.f);

        vr_teleporting_impact_valid = trace.fraction < 1.0 && goodNormal;
        VectorCopy(trace.endpos, vr_teleporting_impact);
        // TODO VR: sometimes this goes into the floor!
        // Try checking other trace properties (e.g. allsolid) or increasing the
        // offsets

        if(vr_teleporting_impact_valid)
        {
            // TODO VR:
            extern void R_RunParticle2Effect(
                vec3_t org, vec3_t dir, int preset, int count);

            vec3_t dir{0, 0, 0};
            R_RunParticle2Effect(vr_teleporting_impact, dir, 7, 4);
        }
    }
    else if(vr_was_teleporting && vr_teleporting_impact_valid)
    {
        vr_send_teleport_msg = true;
        VectorCopy(vr_teleporting_impact, sv_player->v.origin);
    }

    vr_was_teleporting = vr_teleporting;
}

void VR_UpdateScreenContent()
{
    GLint w;

    GLint h;

    // Last chance to enable VR Mode - we get here when the game already
    // start up with vr_enabled 1 If enabling fails, unset the cvar and
    // return.
    if(!vr_initialized && !VR_Enable())
    {
        Cvar_Set("vr_enabled", "0");
        return;
    }

    w = glwidth;
    h = glheight;

    entity_t* player = &cl_entities[cl.viewentity];

    // Update poses
    vr::VRCompositor()->WaitGetPoses(
        ovr_DevicePose, vr::k_unMaxTrackedDeviceCount, nullptr, 0);

    // Get the VR devices' orientation and position
    for(uint32_t iDevice = 0; iDevice < vr::k_unMaxTrackedDeviceCount;
        iDevice++)
    {
        // HMD vectors update
        if(ovr_DevicePose[iDevice].bPoseIsValid &&
            ovrHMD->GetTrackedDeviceClass(iDevice) ==
                vr::TrackedDeviceClass_HMD)
        {
            headVelocity = ovr_DevicePose->vVelocity;

            headPos =
                Matrix34ToVector(ovr_DevicePose->mDeviceToAbsoluteTracking);
            headOrigin[0] = headPos.v[2];
            headOrigin[1] = headPos.v[0];
            headOrigin[2] = headPos.v[1];

            vec3_t moveInTracking;
            _VectorSubtract(headOrigin, lastHeadOrigin, moveInTracking);
            moveInTracking[0] *= -meters_to_units;
            moveInTracking[1] *= -meters_to_units;
            moveInTracking[2] = 0;
            Vec3RotateZ(
                moveInTracking, vrYaw * M_PI_DIV_180, vr_room_scale_move);

            // VR: Scale room-scale movement for easier dodging and improve
            // teleportation-based gameplay experience.
            vr_room_scale_move[0] *= vr_room_scale_move_mult.value;
            vr_room_scale_move[1] *= vr_room_scale_move_mult.value;
            vr_room_scale_move[2] *= vr_room_scale_move_mult.value;

            _VectorCopy(headOrigin, lastHeadOrigin);
            _VectorSubtract(headOrigin, lastHeadOrigin, headOrigin);
            headPos.v[0] -= lastHeadOrigin[1];
            headPos.v[2] -= lastHeadOrigin[0];

            vr::HmdQuaternion_t headQuat =
                Matrix34ToQuaternion(ovr_DevicePose->mDeviceToAbsoluteTracking);
            vr::HmdVector3_t leyePos =
                Matrix34ToVector(ovrHMD->GetEyeToHeadTransform(eyes[0].eye));
            vr::HmdVector3_t reyePos =
                Matrix34ToVector(ovrHMD->GetEyeToHeadTransform(eyes[1].eye));

            leyePos = RotateVectorByQuaternion(leyePos, headQuat);
            reyePos = RotateVectorByQuaternion(reyePos, headQuat);

            HmdVec3RotateY(&headPos, -vrYaw * M_PI_DIV_180);

            HmdVec3RotateY(&leyePos, -vrYaw * M_PI_DIV_180);
            HmdVec3RotateY(&reyePos, -vrYaw * M_PI_DIV_180);

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
            vr::HmdVector3_t rawControllerVel =
                ovr_DevicePose[iDevice].vVelocity;

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

                // TODO VR: make prettier
                const auto [x, y, z] = QuatToYawPitchRoll(rawControllerQuat);
                controller->orientation[0] = x;
                controller->orientation[1] = y;
                controller->orientation[2] = z;
            }
        }
    }

    // Reset the aim roll value before calculation, incase the user switches
    // aimmode from 7 to another.
    cl.aimangles[ROLL] = 0.0;

    const auto orientation = QuatToYawPitchRoll(eyes[1].orientation);
    if(readbackYaw)
    {
        vrYaw = cl.viewangles[YAW] - (orientation[YAW] - vrYaw);
        readbackYaw = false;
    }

    switch((int)vr_aimmode.value)
    {
            // 1: (Default) Head Aiming; View YAW is mouse+head, PITCH is
            // head
        default:
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
            float diffHMDYaw = orientation[YAW] - lastOrientation[YAW];
            float diffHMDPitch = orientation[PITCH] - lastOrientation[PITCH];
            float diffAimYaw = cl.aimangles[YAW] - lastAim[YAW];
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

        // 7: Controller Aiming;
        case VrAimMode::e_CONTROLLER:
        {
            cl.viewangles[PITCH] = orientation[PITCH];
            cl.viewangles[YAW] = orientation[YAW];

            vec3_t mat[3];
            vec3_t matTmp[3];

            vec3_t rotOfs = {vr_gunangle.value, vr_gunyaw.value, 0};

            vec3_t gunMatPitch[3];
            CreateRotMat(0, rotOfs[0], gunMatPitch); // pitch

            vec3_t gunMatYaw[3];
            CreateRotMat(1, rotOfs[1], gunMatYaw); // yaw

            vec3_t gunMatRoll[3];
            CreateRotMat(2, rotOfs[2], gunMatRoll); // roll

            for(int i = 0; i < 2; i++)
            {
                RotMatFromAngleVector(controllers[i].orientation, mat);

                R_ConcatRotations(gunMatRoll, mat, matTmp);
                for(int j = 0; j < 3; ++j)
                {
                    VectorCopy(matTmp[j], mat[j]);
                }

                R_ConcatRotations(gunMatPitch, mat, matTmp);
                for(int j = 0; j < 3; ++j)
                {
                    VectorCopy(matTmp[j], mat[j]);
                }

                R_ConcatRotations(gunMatYaw, mat, matTmp);
                for(int j = 0; j < 3; ++j)
                {
                    VectorCopy(matTmp[j], mat[j]);
                }

                vec3_t handrottemp;
                AngleVectorFromRotMat(mat, handrottemp);
                VectorCopy(handrottemp, cl.handrot[i]);
            }

            if(cl.viewent.model)
            {
                auto* hdr = (aliashdr_t*)Mod_Extradata(cl.viewent.model);
                Mod_Weapon(cl.viewent.model->name, hdr);

                // auto* testhdr = (aliashdr_t*)Mod_Extradata(test);
                // testhdr->flags |= EF_GRENADE;
                // VectorScale(testhdr->scale_origin, 0.5f,
                // testhdr->scale_origin);

                // BModels cannot be scaled, doesnt work
                // qmodel_t* test = Mod_ForName("maps/b_shell1.bsp", true);
                // auto* testhdr = (aliashdr_t*)Mod_Extradata(test);
                // VectorScale(testhdr->scale_origin, 0.5f,
                // testhdr->scale_origin);
            }

            if(cl.offhand_viewent.model)
            {
                // aliashdr_t* hdr =
                // (aliashdr_t*)Mod_Extradata(cl.offhand_viewent.model);
                // Mod_Weapon(cl.offhand_viewent.model->name, hdr);

                ApplyMod_Weapon(VR_GetOffHandFistCvarEntry(),
                    (aliashdr_t*)Mod_Extradata(cl.offhand_viewent.model));
            }

            SetHandPos(0, player);
            SetHandPos(1, player);

            // TODO VR: interpolate based on weapon weight?
            VectorCopy(cl.handrot[1], cl.aimangles); // Sets the shooting angle
            // TODO VR: what sets the shooting origin?

            // TODO VR: teleportation stuff
            VR_DoTeleportation();
            break;
        }
    }
    cl.viewangles[ROLL] = orientation[ROLL];

    VectorCopy(orientation, lastOrientation);
    VectorCopy(cl.aimangles, lastAim);

    VectorCopy(cl.viewangles, r_refdef.viewangles);
    VectorCopy(cl.aimangles, r_refdef.aimangles);

    // Render the scene for each eye into their FBOs
    for(int i = 0; i < 2; i++)
    {
        current_eye = &eyes[i];

        vec3_t temp;

        // We need to scale the view offset position to quake units and
        // rotate it by the current input angles (viewangle - eye
        // orientation)
        const auto orientation = QuatToYawPitchRoll(current_eye->orientation);
        temp[0] = -current_eye->position.v[2] * meters_to_units; // X
        temp[1] = -current_eye->position.v[0] * meters_to_units; // Y
        temp[2] = current_eye->position.v[1] * meters_to_units;  // Z
        Vec3RotateZ(temp,
            (r_refdef.viewangles[YAW] - orientation[YAW]) * M_PI_DIV_180,
            vr_viewOffset);
        vr_viewOffset[2] += vr_floor_offset.value;

        RenderScreenForCurrentEye_OVR();
    }

    // Blit mirror texture to backbuffer
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, eyes[0].fbo.framebuffer);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
    glBlitFramebufferEXT(0, eyes[0].fbo.size.width, eyes[0].fbo.size.height, 0,
        0, h, w, 0, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
}

void VR_SetMatrices()
{
    vr::HmdMatrix44_t projection;

    // Calculate HMD projection matrix and view offset position
    projection = TransposeMatrix(
        ovrHMD->GetProjectionMatrix(current_eye->eye, 4.f, gl_farclip.value));

    // Set OpenGL projection and view matrices
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((GLfloat*)projection.m);
}

void VR_CalibrateHeight()
{
    const auto height = headPos.v[1];
    Cvar_SetValue("vr_height_calibration", height);
    Con_Printf("Calibrated height to %.2f\n", height);
}

void VR_AddOrientationToViewAngles(vec3_t angles)
{
    const auto orientation = QuatToYawPitchRoll(current_eye->orientation);

    angles[PITCH] = angles[PITCH] + orientation[PITCH];
    angles[YAW] = angles[YAW] + orientation[YAW];
    angles[ROLL] = orientation[ROLL];
}

[[nodiscard]] glm::vec3 VR_CalcWeaponMuzzlePos() noexcept
{
    const auto [ox, oy, oz] = VR_GetWpnOffsets(weaponCVarEntry);
    glm::vec3 finalOffsets{ox, -oy, oz};
    finalOffsets /= VR_GetWpnCVarValue(weaponCVarEntry, WpnCVar::Scale);

    const auto [moX, moY, moZ] = VR_GetWpnMuzzleOffsets(weaponCVarEntry);
    const glm::vec3 muzzleOfs{moX, moY, moZ};

    using namespace quake::util;
    finalOffsets += muzzleOfs;

    vec3_t forward, right, up;
    AngleVectors(cl.handrot[1], forward, right, up);

    const auto glmForward = toVec3(forward);
    const auto glmRight = toVec3(right);
    const auto glmUp = toVec3(up);

    finalOffsets *= VR_GetWpnCVarValue(weaponCVarEntry, WpnCVar::Scale) *
                    VR_GetScaleCorrect();

    const auto fFwd = glmForward * finalOffsets[0];
    const auto fRight = glmRight * finalOffsets[1];
    const auto fUp = glmUp * finalOffsets[2];

    return toVec3(cl.handpos[1]) + fFwd + fRight + fUp;
}

void VR_CalcWeaponMuzzlePos(vec3_t out) noexcept
{
    quake::util::toQuakeVec3(out, VR_CalcWeaponMuzzlePos());
}

void VR_ShowCrosshair()
{
    if(!sv_player || (int)(sv_player->v.weapon) == WID_AXE ||
        (int)(sv_player->v.weapon) == WID_MJOLNIR)
    {
        return;
    }

    const float size = CLAMP(0.0, vr_crosshair_size.value, 32.0);
    const float alpha = CLAMP(0.0, vr_crosshair_alpha.value, 1.0);

    if(size <= 0 || alpha <= 0)
    {
        return;
    }

    // setup gl
    glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    GL_PolygonOffset(OFFSET_SHOWTRIS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    vec3_t forward, right, up;
    vec3_t start;

    // calc the line and draw
    if(vr_aimmode.value == VrAimMode::e_CONTROLLER)
    {
        AngleVectors(cl.handrot[1], forward, right, up);
        VR_CalcWeaponMuzzlePos(start);
    }
    else
    {
        VectorCopy(cl.viewent.origin, start);
        start[2] -= cl.viewheight - 10;
        AngleVectors(cl.aimangles, forward, right, up);
    }

    vec3_t end;
    vec3_t impact;

    switch((int)vr_crosshair.value)
    {
        default:
        case VrCrosshair::e_POINT:
        {
            if(vr_crosshair_depth.value <= 0)
            {
                // trace to first wall
                VectorMA(start, 4096, forward, end);

                end[2] += vr_crosshairy.value;
                TraceLine(start, end, impact);
            }
            else
            {
                // fix crosshair to specific depth
                VectorMA(start, vr_crosshair_depth.value, forward, impact);
            }

            glEnable(GL_POINT_SMOOTH);
            glColor4f(1, 0, 0, alpha);
            glPointSize(size * glwidth / vid.width);

            glBegin(GL_POINTS);
            glVertex3f(impact[0], impact[1], impact[2]);
            glEnd();
            glDisable(GL_POINT_SMOOTH);
            break;
        }

        case VrCrosshair::e_LINE: [[fallthrough]];
        case VrCrosshair::e_LINE_SMOOTH:
        {
            const float depth =
                vr_crosshair_depth.value <= 0 ? 4096 : vr_crosshair_depth.value;

            // trace to first entity
            VectorMA(start, depth, forward, end);
            const trace_t trace =
                TraceLineToEntity(start, end, impact, sv_player);

            if(trace.fraction >= 1.0)
            {
                VectorCopy(end, impact);
            }

            impact[2] += vr_crosshairy.value * 10.f;

            glLineWidth(size * glwidth / vid.width);
            glEnable(GL_LINE_SMOOTH);
            glShadeModel(GL_SMOOTH);
            glBegin(GL_LINE_STRIP);

            if((int)vr_crosshair.value == VrCrosshair::e_LINE)
            {
                glColor4f(1, 0, 0, alpha);
                glVertex3f(start[0], start[1], start[2]);
                glVertex3f(impact[0], impact[1], impact[2]);
            }
            else
            {
                vec3_t midA, midB;
                vec3lerp(midA, start, impact, 0.15);
                vec3lerp(midB, start, impact, 0.85);

                glColor4f(1, 0, 0, alpha * 0.01f);
                glVertex3f(start[0], start[1], start[2]);

                glColor4f(1, 0, 0, alpha);
                glVertex3f(midA[0], midA[1], midA[2]);
                glVertex3f(midB[0], midB[1], midB[2]);

                glColor4f(1, 0, 0, alpha * 0.01f);
                glVertex3f(impact[0], impact[1], impact[2]);
            }

            glEnd();
            glShadeModel(GL_FLAT);
            glDisable(GL_LINE_SMOOTH);
            break;
        }
    }

    // cleanup gl
    glColor3f(1, 1, 1);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    GL_PolygonOffset(OFFSET_NONE);
    glEnable(GL_DEPTH_TEST);
}

void VR_DrawTeleportLine()
{
    if(!vr_teleport_enabled.value || !sv_player || !vr_teleporting ||
        vr_aimmode.value != VrAimMode::e_CONTROLLER)
    {
        return;
    }

    const float size = 2.f;
    const float alpha = 0.5f;

    if(size <= 0 || alpha <= 0)
    {
        return;
    }

    // setup gl
    glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    GL_PolygonOffset(OFFSET_SHOWTRIS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    // calc angles
    vec3_t forward, right, up;
    vec3_t start;

    AngleVectors(cl.handrot[0], forward, right, up);
    VectorCopy(cl.handpos[0], start);

    // calc line
    vec3_t impact;
    VectorCopy(vr_teleporting_impact, impact);

    // draw line
    if(vr_teleporting_impact_valid)
    {
        glColor4f(0, 0, 1, alpha);
    }
    else
    {
        glColor4f(1, 0, 0, alpha);
    }

    glLineWidth(size * glwidth / vid.width);
    glBegin(GL_LINES);
    glVertex3f(start[0], start[1], start[2]);
    glVertex3f(impact[0], impact[1], impact[2]);
    glEnd();

    // cleanup gl
    glColor3f(1, 1, 1);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    GL_PolygonOffset(OFFSET_NONE);
    glEnable(GL_DEPTH_TEST);
}

void VR_Draw2D()
{
    bool draw_sbar = false;
    vec3_t menu_angles;

    vec3_t forward;

    vec3_t right;

    vec3_t up;

    vec3_t target;
    float scale_hud = vr_menu_scale.value;

    int oldglwidth = glwidth;

    int oldglheight = glheight;

    int oldconwidth = vid.conwidth;

    int oldconheight = vid.conheight;

    glwidth = 320;
    glheight = 200;

    vid.conwidth = 320;
    vid.conheight = 200;

    // draw 2d elements 1m from the users face, centered
    glPushMatrix();
    glDisable(GL_DEPTH_TEST); // prevents drawing sprites on sprites from
                              // interferring with one another
    glEnable(GL_BLEND);

    // TODO VR: control with cvar
    if(false && vr_aimmode.value == VrAimMode::e_CONTROLLER)
    {
        AngleVectors(cl.handrot[1], forward, right, up);

        VectorCopy(cl.handrot[1], menu_angles);

        AngleVectors(menu_angles, forward, right, up);

        VectorMA(cl.handpos[1], 48, forward, target);
    }
    else
    {
        // TODO: Make the menus' position sperate from the right hand.
        // Centered on last view dir?
        VectorCopy(cl.viewangles, menu_angles);

        // TODO VR: ?
        if(vr_aimmode.value == VrAimMode::e_HEAD_MYAW ||
            vr_aimmode.value == VrAimMode::e_HEAD_MYAW_MPITCH)
        {
            menu_angles[PITCH] = 0;
        }

        AngleVectors(menu_angles, forward, right, up);
        VectorMA(r_refdef.vieworg, vr_menu_distance.value, forward, target);
    }

    // TODO VR: control smoothing with cvar
    const auto smoothedTarget = glm::mix(lastMenuPosition, toVec3(target), 0.9);
    lastMenuPosition = smoothedTarget;

    glTranslatef(smoothedTarget[0], smoothedTarget[1], smoothedTarget[2]);

    glRotatef(menu_angles[YAW] - 90, 0, 0, 1); // rotate around z
    glRotatef(90 + menu_angles[PITCH], -1, 0,
        0); // keep bar at constant angled pitch towards user
    glTranslatef(-(320.0 * scale_hud / 2), -(200.0 * scale_hud / 2),
        0); // center the status bar
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
    else if(cl.intermission == 1 && key_dest == key_game) // end of level
    {
        Sbar_IntermissionOverlay();
    }
    else if(cl.intermission == 2 && key_dest == key_game) // end of episode
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
    vec3_t sbar_angles;

    vec3_t forward;

    vec3_t right;

    vec3_t up;

    vec3_t target;
    float scale_hud = vr_hud_scale.value;

    glPushMatrix();
    glDisable(GL_DEPTH_TEST); // prevents drawing sprites on sprites from
                              // interferring with one another

    if(vr_aimmode.value == VrAimMode::e_CONTROLLER)
    {
        const auto mode = static_cast<int>(vr_sbar_mode.value);

        if(mode == static_cast<int>(VrSbarMode::MainHand))
        {
            AngleVectors(cl.handrot[1], forward, right, up);
            VectorCopy(cl.handrot[1], sbar_angles);

            AngleVectors(sbar_angles, forward, right, up);
            VectorMA(cl.handpos[1], -5, right, target);
        }
        else
        {
            AngleVectors(cl.handrot[0], forward, right, up);
            VectorCopy(cl.handrot[0], sbar_angles);

            AngleVectors(sbar_angles, forward, right, up);
            VectorMA(cl.handpos[0], 0.f, right, target);
        }
    }
    else
    {
        VectorCopy(cl.aimangles, sbar_angles);

        if(vr_aimmode.value == VrAimMode::e_HEAD_MYAW ||
            vr_aimmode.value == VrAimMode::e_HEAD_MYAW_MPITCH)
        {
            sbar_angles[PITCH] = 0;
        }

        AngleVectors(sbar_angles, forward, right, up);

        VectorMA(cl.viewent.origin, 1.0, forward, target);
    }

    // TODO VR: 1.0? Attach to off hand?
    const auto smoothedTarget = glm::mix(lastHudPosition, toVec3(target), 1.0);
    lastHudPosition = smoothedTarget;

    glTranslatef(smoothedTarget[0], smoothedTarget[1], smoothedTarget[2]);

    if(vr_aimmode.value == VrAimMode::e_CONTROLLER &&
        static_cast<int>(vr_sbar_mode.value) ==
            static_cast<int>(VrSbarMode::OffHand))
    {
        glm::fquat m;
        m = glm::quatLookAt(toVec3(forward), toVec3(up));
        m = glm::rotate(m, vr_sbar_offset_pitch.value, glm::vec3(1, 0, 0));
        m = glm::rotate(m, vr_sbar_offset_yaw.value, glm::vec3(0, 1, 0));
        m = glm::rotate(m, vr_sbar_offset_roll.value, glm::vec3(0, 0, 1));
        m = glm::normalize(m);

        glMultMatrixf(&glm::mat4_cast(m)[0][0]);

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

void VR_SetAngles(vec3_t angles)
{
    VectorCopy(angles, cl.aimangles);
    VectorCopy(angles, cl.viewangles);
    VectorCopy(angles, lastAim);
}

void VR_ResetOrientation()
{
    cl.aimangles[YAW] = cl.viewangles[YAW];
    cl.aimangles[PITCH] = cl.viewangles[PITCH];
    if(vr_enabled.value)
    {
        // IVRSystem_ResetSeatedZeroPose(ovrHMD);
        VectorCopy(cl.aimangles, lastAim);
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

void VR_DoHaptic(const int hand, const float delay, const float duration,
    const float frequency, const float amplitude)
{
    const auto hapticTarget = hand == 0 ? vrahLeftHaptic : vrahRightHaptic;

    vr::VRInput()->TriggerHapticVibrationAction(hapticTarget, delay, duration,
        frequency, amplitude, vr::k_ulInvalidInputValueHandle);
}

[[nodiscard]] static VRAxisResult VR_DoInput()
{
    {
        const auto rc = vr::VRInput()->UpdateActionState(
            &vrActiveActionSet, sizeof(vr::VRActiveActionSet_t), 1);

        if(rc != vr::EVRInputError::VRInputError_None)
        {
            Con_Printf(
                "Failed to update Steam VR action state, rc = %d", (int)rc);
        }
    }

    const auto readAnalogAction = [](const vr::VRActionHandle_t& actionHandle) {
        vr::InputAnalogActionData_t out;

        const auto rc = vr::VRInput()->GetAnalogActionData(actionHandle, &out,
            sizeof(vr::InputAnalogActionData_t),
            vr::k_ulInvalidInputValueHandle);

        if(rc != vr::EVRInputError::VRInputError_None)
        {
            Con_Printf(
                "Failed to read Steam VR analog action data, rc = %d", (int)rc);
        }

        return out;
    };

    const auto readDigitalAction =
        [](const vr::VRActionHandle_t& actionHandle) {
            vr::InputDigitalActionData_t out;

            const auto rc = vr::VRInput()->GetDigitalActionData(actionHandle,
                &out, sizeof(vr::InputDigitalActionData_t),
                vr::k_ulInvalidInputValueHandle);

            if(rc != vr::EVRInputError::VRInputError_None)
            {
                Con_Printf(
                    "Failed to read Steam VR digital action data, rc = %d",
                    (int)rc);
            }

            return out;
        };

    const auto inpLocomotion = readAnalogAction(vrahLocomotion);
    const auto inpTurn = readAnalogAction(vrahTurn);
    const auto inpFire = readDigitalAction(vrahFire);
    const auto inpJump = readDigitalAction(vrahJump);
    const auto inpPrevWeapon = readDigitalAction(vrahPrevWeapon);
    const auto inpNextWeapon = readDigitalAction(vrahNextWeapon);
    const auto inpEscape = readDigitalAction(vrahEscape);
    const auto inpSpeed = readDigitalAction(vrahSpeed);
    const auto inpTeleport = readDigitalAction(vrahTeleport);

    const auto isRisingEdge = [](const vr::InputDigitalActionData_t& data) {
        return data.bState && data.bChanged;
    };

    const bool mustFire = inpFire.bState;

    const bool isRoomscaleJump =
        vr_roomscale_jump.value &&
        headVelocity.v[1] > vr_roomscale_jump_threshold.value &&
        headPos.v[1] > vr_height_calibration.value;

    // TODO VR: make nice `Menu` class with declarative syntax
    const bool mustJump = isRisingEdge(inpJump) || isRoomscaleJump;
    const bool mustPrevWeapon = isRisingEdge(inpPrevWeapon);
    const bool mustNextWeapon = isRisingEdge(inpNextWeapon);
    const bool mustEscape = isRisingEdge(inpEscape);
    const bool mustSpeed = inpSpeed.bState;

    in_speed.state = mustSpeed;

    const auto doMenuHaptic = [&](const vr::VRInputValueHandle_t& origin) {
        vr::VRInput()->TriggerHapticVibrationAction(
            vrahLeftHaptic, 0, 0.1, 50, 0.5, origin);

        vr::VRInput()->TriggerHapticVibrationAction(
            vrahRightHaptic, 0, 0.1, 50, 0.5, origin);
    };

    const auto doMenuKeyEventWithHaptic =
        [&](const int key, const vr::InputDigitalActionData_t& i) {
            const bool pressed = isRisingEdge(i);

            if(pressed)
            {
                doMenuHaptic(i.activeOrigin);
            }

            Key_Event(key, pressed);
        };

    if(key_dest == key_menu)
    {
        doMenuKeyEventWithHaptic(K_ENTER, inpJump);
        doMenuKeyEventWithHaptic(K_ESCAPE, inpEscape);
        doMenuKeyEventWithHaptic(K_LEFTARROW, inpPrevWeapon);
        doMenuKeyEventWithHaptic(K_RIGHTARROW, inpNextWeapon);

        const auto doAxis = [&](const int quakeKeyNeg, const int quakeKeyPos) {
            const float lastVal = inpLocomotion.y - inpLocomotion.deltaY;
            const float val = inpLocomotion.y;

            const bool posWasDown = lastVal > 0.0f;
            const bool posDown = val > 0.0f;
            if(posDown != posWasDown)
            {
                if(posDown)
                {
                    doMenuHaptic(inpLocomotion.activeOrigin);
                }

                Key_Event(quakeKeyNeg, posDown);
            }

            const bool negWasDown = lastVal < 0.0f;
            const bool negDown = val < 0.0f;
            if(negDown != negWasDown)
            {
                if(negDown)
                {
                    doMenuHaptic(inpLocomotion.activeOrigin);
                }

                Key_Event(quakeKeyPos, negDown);
            }
        };

        doAxis(K_UPARROW, K_DOWNARROW);
    }
    else
    {
        Key_Event(K_MOUSE1, mustFire);
        Key_Event(K_SPACE, mustJump);
        doMenuKeyEventWithHaptic(K_ESCAPE, inpEscape);
        Key_Event('3', mustPrevWeapon);
        Key_Event('1', mustNextWeapon);

        vr_teleporting = inpTeleport.bState;
    }

    return VR_GetInputAxes(inpLocomotion, inpTurn);
}

void VR_Move(usercmd_t* cmd)
{
    if(!vr_enabled.value)
    {
        return;
    }

    // VR: Main hand: `handpos`, `handrot`, `handvel`, `handvelmag`.
    VectorCopy(cl.handpos[1], cmd->handpos);
    VectorCopy(cl.handrot[1], cmd->handrot);
    VectorCopy(cl.handvel[1], cmd->handvel);
    cmd->handvelmag = cl.handvelmag[1];

    // VR: Off hand: `offhandpos`, `offhandrot`, `offhandvel`, `offhandvelmag`.
    VectorCopy(cl.handpos[0], cmd->offhandpos);
    VectorCopy(cl.handrot[0], cmd->offhandrot);
    VectorCopy(cl.handvel[0], cmd->offhandvel);
    cmd->offhandvelmag = cl.handvelmag[0];

    // VR: Weapon muzzle position.
    VR_CalcWeaponMuzzlePos(cmd->muzzlepos);

    // VR: Buttons and instant controller actions.
    // VR: Query state of controller axes.
    const auto [fwdMove, sideMove, yawMove] = VR_DoInput();

    // VR: Teleportation.
    if(std::exchange(vr_send_teleport_msg, false))
    {
        cmd->teleporting = 1;
        VectorCopy(vr_teleporting_impact, cmd->teleport_target);
    }
    else
    {
        cmd->teleporting = 0;
    }

    if(key_dest == key_menu)
    {
        return;
    }

    vec3_t lfwd;
    vec3_t lright;
    vec3_t lup;
    AngleVectors(cl.handrot[0], lfwd, lright, lup);

    if(vr_movement_mode.value == VrMovementMode::e_RAW_INPUT)
    {
        cmd->forwardmove += cl_forwardspeed.value * fwdMove;
        cmd->sidemove += cl_forwardspeed.value * sideMove;
    }
    else
    {
        vec3_t playerYawOnly = {0, sv_player->v.v_viewangle[YAW], 0};

        vec3_t vfwd;
        vec3_t vright;
        vec3_t vup;
        AngleVectors(playerYawOnly, vfwd, vright, vup);

        // avoid gimbal by using up if we are point up/down
        if(fabsf(lfwd[2]) > 0.8f)
        {
            if(lfwd[2] < -0.8f)
            {
                lfwd[0] *= -1;
                lfwd[1] *= -1;
                lfwd[2] *= -1;
            }
            else
            {
                lup[0] *= -1;
                lup[1] *= -1;
                lup[2] *= -1;
            }

            VectorSwap(lup, lfwd);
        }

        // Scale up directions so tilting doesn't affect speed
        float fac = 1.0f / lup[2];
        for(int i = 0; i < 3; i++)
        {
            lfwd[i] *= fac;
            lright[i] *= fac;
        }

        vec3_t move = {0, 0, 0};
        VectorMA(move, fwdMove, lfwd, move);
        VectorMA(move, sideMove, lright, move);

        const float fwd = DotProduct(move, vfwd);
        const float right = DotProduct(move, vright);

        // Quake run doesn't affect the value of cl_sidespeed.value, so
        // just use forward speed here for consistency
        cmd->forwardmove += cl_forwardspeed.value * fwd;
        cmd->sidemove += cl_forwardspeed.value * right;
    }

    AngleVectors(cl.handrot[0], lfwd, lright, lup);
    cmd->upmove += cl_upspeed.value * fwdMove * lfwd[2];

    if((in_speed.state & 1) ^ (cl_alwaysrun.value == 0.0))
    {
        cmd->forwardmove *= cl_movespeedkey.value;
        cmd->sidemove *= cl_movespeedkey.value;
        cmd->upmove *= cl_movespeedkey.value;
    }

    if(vr_enable_joystick_turn.value == 1)
    {
        if(vr_snap_turn.value != 0)
        {
            static int lastSnap = 0;
            int snap = yawMove > 0.0f ? 1 : yawMove < 0.0f ? -1 : 0;
            if(snap != lastSnap)
            {
                vrYaw -= snap * vr_snap_turn.value;
                lastSnap = snap;
            }
        }
        else
        {
            vrYaw -= (yawMove * host_frametime * 100.0f) * vr_turn_speed.value;
        }
    }
}
