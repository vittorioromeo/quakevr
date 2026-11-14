// vr_glsafe.cpp -- the OpenGL start-up's breadcrumbs and safe mode (1.0.1: a Radeon RX 7900 XTX crashed inside AMD's
// driver, on one of its own threads, before any map; ROUND21.md "AMD start-up crash").
//
// Breadcrumbs (VR_GLStep): each start-up GL step and each shader compile and link, named before the call, kept in
// memory (the crash report's "Last GL steps", vr_crash.cpp) and written while the game starts to
// <game folder>/crash/gl_startup.log, unbuffered (the line is in the file before the call that may crash), and to
// qconsole.log (-condebug).
//
// Safe mode (VR_GLSafeOff): the optional GL features off for the run, each one logged: persistent mapped buffers
// (GL_ARB_buffer_storage), bindless textures, multi-bind, clip control (reversed Z, float depth), the shadow casters'
// layered draws (gl_ViewportIndex from a vertex shader), GL debug output, MSAA, and the shadow atlas at 4096.
//   - "-glsafe" or "vr_glsafe 1" (read before the window opens): this run.
//   - automatic: the last start's gl_startup.log has no end line (it died before its first map was drawn, or was
//     killed): this start is in safe mode, and says so. Kept on later starts while a start in it ends well (a release
//     build; a dev build tries the full renderer again next time) until "vr_glsafe_retry" (Debug > Startup). Never in
//     automated test runs (QVR_NO_ERROR_DIALOG, QVR_TEST_HIDDEN) unless "-glsafeauto"; "-noglsafe" turns it off.
//   - vendor workarounds (vr_gl_workarounds 1, default): on AMD/ATI, bindless textures off without safe mode
//     (amdDefaults: what crashed 1.0.0 there).
// A start ends well at the first map's 30th drawn frame, or at a quit or an error (Sys_Error: not a crash).

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_strings.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace qvr;

extern "C" char com_gamedir[];
extern "C" void Con_DebugLog(const char* msg);
extern "C" void Sys_mkdir(const char* path);

namespace
{

constexpr int stepCount = 24;
constexpr int stepLength = 200;
constexpr const char* endLine = "== ended: ";          // the line a start that ended well writes last
constexpr const char* retryLine = "== next start: the full renderer"; // vr_glsafe_retry
constexpr int framesAfterMap = 30;

struct Step
{
    double ms; // since the first step
    char text[stepLength];
};

struct State
{
    Step steps[stepCount]{};
    long total = 0; // steps recorded (steps[total % stepCount] is the next)
    double t0 = -1.0;
    FILE* log = nullptr; // gl_startup.log while the game starts
    char logPath[1024]{};
    bool started = false; // VR_GLStartupBegin ran
    bool ended = false;
    int safe = 0;         // 1: safe mode this run
    bool safeAuto = false;
    bool keepAuto = false; // the last start was in automatic safe mode and ended well
    unsigned vendorOff = 0; // the vendor workarounds' features (and -glescompat's)
    bool esCompat = false;  // -glescompat: the features OpenGL ES 3.2 lacks off (QUEST_PLAN.md, C2)
    int esShaderErrors = 0; // -glescompat: programs that failed as ES 3.2 GLSL (VR_GLESShaderError)
    unsigned logged = 0;    // features whose "off" line was printed
    char why[256]{};        // why safe mode is on
    bool mapSpawned = false;
    int framesSinceMap = 0;
    int frames = 0;
};
State glsafe;

double nowMs()
{
    const double t = Sys_DoubleTime() * 1000.0;
    if(glsafe.t0 < 0.0)
    {
        glsafe.t0 = t;
    }
    return t - glsafe.t0;
}

void writeLog(const char* line)
{
    if(glsafe.log)
    {
        fputs(line, glsafe.log); // (unbuffered: in the file now)
    }
}

bool testRun()
{
    return getenv("QVR_NO_ERROR_DIALOG") || getenv("QVR_TEST_HIDDEN");
}

// The last start's log: "" none; its end line ("" when it has none: a crash); whether vr_glsafe_retry asked.
struct LastStart
{
    bool exists = false;
    bool ended = false;
    bool retry = false;
    bool autoSafe = false; // it ran in automatic safe mode
    char lastStep[stepLength]{};
};

LastStart readLastStart(const char* path)
{
    LastStart r;
    FILE* f = fopen(path, "rb");
    if(!f)
    {
        return r;
    }
    char line[512];
    while(fgets(line, sizeof(line), f))
    {
        r.exists = true;
        line[strcspn(line, "\r\n")] = 0;
        if(!strncmp(line, endLine, strlen(endLine)))
        {
            r.ended = true;
        }
        else if(!strncmp(line, retryLine, strlen(retryLine)))
        {
            r.retry = true;
        }
        else if(strstr(line, "safe mode: on (automatic"))
        {
            r.autoSafe = true;
        }
        if(line[0] && strncmp(line, "==", 2))
        {
            q_strlcpy(r.lastStep, line, sizeof(r.lastStep));
        }
    }
    fclose(f);
    return r;
}

const char* featureName(int feature)
{
    switch(feature)
    {
        case VR_GLSAFE_BUFFER_STORAGE: return "persistent mapped buffers (GL_ARB_buffer_storage)";
        case VR_GLSAFE_BINDLESS: return "bindless textures (GL_ARB_bindless_texture)";
        case VR_GLSAFE_MULTI_BIND: return "multi-bind (GL_ARB_multi_bind)";
        case VR_GLSAFE_CLIP_CONTROL: return "clip control (reversed Z, float depth)";
        case VR_GLSAFE_VIEWPORT_LAYER: return "layered shadow casters (gl_ViewportIndex in vertex shaders)";
        case VR_GLSAFE_DEBUG_OUTPUT: return "GL debug output";
        case VR_GLSAFE_MSAA: return "MSAA";
        case VR_GLSAFE_SHADOW_ATLAS: return "the 8192 shadow atlas (4096)";
        default: return "?";
    }
}

// The features off on AMD/ATI without safe mode. Bindless textures: 1.0.0 crashed in AMD's driver (an RX 7900 XTX,
// Adrenalin 26.8.1, on a driver thread while GL_CreateShaders built the programs) and started with -nobindless; plain
// Ironwail's bindless runs there, so it is Quake VR's world shader on that path (its normal and specular maps' handles,
// the parallax walk, decals and retro textures sampling handles passed through functions, in loops), not pinned down
// without the GPU. The layered shadow casters (gl_ViewportIndex from a vertex shader) compiled there: kept.
constexpr unsigned amdDefaults = VR_GLSAFE_BINDLESS;

// -glescompat (QUEST_PLAN.md, C2): the PC renderer limited to what OpenGL ES 3.2 on the Quest's Adreno has, to run and
// measure the Quest's paths on the PC before an Android build exists. Off: the optional features ES has no form of
// (bindless textures, multi-bind, gl_ViewportIndex from a vertex shader) and clip control (EXT_clip_control: not known
// on the Quest's driver; checked on the device in C4), and NVIDIA's shading-rate image (vr_foveated.cpp). Kept:
// persistent mapped buffers (EXT_buffer_storage), MSAA, debug output (core in ES 3.2). vr_gles_report lists the rest.
constexpr unsigned esCompatOff =
    VR_GLSAFE_BINDLESS | VR_GLSAFE_MULTI_BIND | VR_GLSAFE_CLIP_CONTROL | VR_GLSAFE_VIEWPORT_LAYER;

} // namespace

extern "C" void VR_GLStep(const char* fmt, ...)
{
    Step& s = glsafe.steps[glsafe.total % stepCount];
    s.ms = nowMs();
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(s.text, sizeof(s.text), fmt, args);
    va_end(args);
    glsafe.total++;
    if(glsafe.log)
    {
        char line[stepLength + 32];
        q_snprintf(line, sizeof(line), "%9.1f ms  %s\n", s.ms, s.text);
        writeLog(line);
        Con_DebugLog(line);
    }
}

extern "C" int VR_GLRecentSteps(char* out, int outSize, int count)
{
    int len = 0;
    out[0] = 0;
    const long first = glsafe.total > count ? glsafe.total - count : 0;
    const long oldest = glsafe.total > stepCount ? glsafe.total - stepCount : 0;
    for(long i = first > oldest ? first : oldest; i < glsafe.total && len < outSize - 1; i++)
    {
        const Step& s = glsafe.steps[i % stepCount];
        const int n = q_snprintf(out + len, outSize - len, "  %9.1f ms  %.*s\n", s.ms, stepLength, s.text);
        if(n < 0)
        {
            break;
        }
        len += n < outSize - len ? n : outSize - 1 - len;
    }
    return static_cast<int>(glsafe.total - (first > oldest ? first : oldest));
}

// VID_Init, before the window: the last start's log read, this one's begun, safe mode decided (vr_glsafe is read
// from the config by then).
extern "C" void VR_GLStartupBegin(void)
{
    if(glsafe.started)
    {
        return;
    }
    glsafe.started = true;
    char dir[1024];
    q_snprintf(dir, sizeof(dir), "%s/crash", com_gamedir);
    Sys_mkdir(dir);
    q_snprintf(glsafe.logPath, sizeof(glsafe.logPath), "%s/gl_startup.log", dir);
    const LastStart last = readLastStart(glsafe.logPath);

    const bool autoAllowed = !COM_CheckParm("-noglsafe") && (!testRun() || COM_CheckParm("-glsafeauto"));
    if(COM_CheckParm("-glsafe"))
    {
        glsafe.safe = 1;
        q_strlcpy(glsafe.why, "on (-glsafe)", sizeof(glsafe.why));
    }
    else if(vr_glsafe.value != 0.f)
    {
        glsafe.safe = 1;
        q_strlcpy(glsafe.why, "on (vr_glsafe 1)", sizeof(glsafe.why));
    }
    else if(autoAllowed && last.exists && !last.ended)
    {
        glsafe.safe = 1;
        glsafe.safeAuto = true;
        q_snprintf(glsafe.why, sizeof(glsafe.why), "on (automatic: the last start ended before its first map, after \"%s\")",
            last.lastStep[0] ? last.lastStep : "?");
    }
    else if(autoAllowed && last.ended && last.autoSafe && !last.retry && !VR_VersionIsDev())
    {
        glsafe.safe = 1;
        glsafe.safeAuto = true;
        q_strlcpy(glsafe.why, "on (automatic, kept: a start crashed in full mode; vr_glsafe_retry tries it again)",
            sizeof(glsafe.why));
    }
    else
    {
        q_strlcpy(glsafe.why, "off", sizeof(glsafe.why));
    }

    glsafe.log = fopen(glsafe.logPath, "wb");
    if(glsafe.log)
    {
        setvbuf(glsafe.log, nullptr, _IONBF, 0);
    }
    char head[512];
    q_snprintf(head, sizeof(head), "== Quake VR %s GL start-up log (the steps, each written before it runs)\n", VR_BuildVersion());
    writeLog(head);
    VR_GLStep("safe mode: %s", glsafe.why);
    if(glsafe.safe)
    {
        Con_Printf("\x02GL safe mode %s: optional GL features off (gl_startup.log; Debug > Startup)\n", glsafe.why + 3);
    }
}

// GL_Init, the context made: the vendor's workarounds.
extern "C" void VR_GLStartupVendor(const char* vendor, const char* renderer, const char* version)
{
    // Tests: "-glfakevendor <vendor>" (e.g. "ATI Technologies Inc."): the workarounds of a GPU we lack.
    const int fake = COM_CheckParm("-glfakevendor");
    if(fake && fake < com_argc - 1)
    {
        vendor = com_argv[fake + 1];
        VR_GLStep("vendor taken as '%s' (-glfakevendor)", vendor);
    }
    VR_GLStep("GL_VENDOR %s; GL_RENDERER %s; GL_VERSION %s", vendor ? vendor : "?", renderer ? renderer : "?",
        version ? version : "?");
    const bool amd = (vendor && (strstr(vendor, "ATI") || strstr(vendor, "AMD"))) ||
                     (renderer && (strstr(renderer, "Radeon") || strstr(renderer, "AMD")));
    glsafe.vendorOff = amd && vr_gl_workarounds.value != 0.f ? amdDefaults : 0u;
    if(glsafe.vendorOff)
    {
        VR_GLStep("vendor workarounds (AMD; vr_gl_workarounds 1): %s off", featureName(VR_GLSAFE_BINDLESS));
    }
    if(COM_CheckParm("-glescompat") || getenv("QVR_GLES_COMPAT")) // (the variable: bench.sh runs, which take no arguments)
    {
        glsafe.esCompat = true;
        glsafe.vendorOff |= esCompatOff;
        VR_GLStep("GLES compat (-glescompat): the features OpenGL ES 3.2 lacks off");
        Con_Printf("GLES compat (-glescompat): the features OpenGL ES 3.2 lacks off (vr_gles_report)\n");
    }
}

extern "C" int VR_GLESCompat(void)
{
    return glsafe.esCompat ? 1 : 0;
}

// -glescompat: every shader as OpenGL ES 3.2 GLSL, so the PC's driver (ARB_ES3_2_compatibility) checks each program
// the way the Quest's will. The source's "#version 430" line becomes "#version 320 es", the ES extensions our gates
// use are enabled where the driver has them (sources test GL_ES and the extension's macro), and precision statements
// go before the first line of code (ES has no default float precision in fragment shaders, none for most sampler and
// image types anywhere). Without -glescompat the strings are given unchanged.
namespace
{
constexpr const char* esHeader = "#version 320 es\n"
                                 "#ifdef GL_EXT_clip_cull_distance\n#extension GL_EXT_clip_cull_distance : enable\n#endif\n"
                                 "#ifdef GL_NV_shader_noperspective_interpolation\n"
                                 "#extension GL_NV_shader_noperspective_interpolation : enable\n#endif\n";

constexpr const char* esPrecisions =
    "precision highp float; precision highp int;\n"
    "precision highp sampler2D; precision highp sampler3D; precision highp samplerCube; precision highp sampler2DArray;\n"
    "precision highp sampler2DShadow; precision highp samplerCubeShadow; precision highp sampler2DArrayShadow;\n"
    "precision highp samplerCubeArray; precision highp samplerCubeArrayShadow; precision highp samplerBuffer;\n"
    "precision highp sampler2DMS; precision highp sampler2DMSArray;\n"
    "precision highp isampler2D; precision highp isampler3D; precision highp isamplerCube; precision highp isampler2DArray;\n"
    "precision highp isamplerBuffer; precision highp isampler2DMS;\n"
    "precision highp usampler2D; precision highp usampler3D; precision highp usamplerCube; precision highp usampler2DArray;\n"
    "precision highp usamplerBuffer; precision highp usampler2DMS;\n"
    "precision highp image2D; precision highp image3D; precision highp imageCube; precision highp image2DArray;\n"
    "precision highp imageBuffer; precision highp iimage2D; precision highp iimage2DArray; precision highp iimageBuffer;\n"
    "precision highp uimage2D; precision highp uimage3D; precision highp uimage2DArray; precision highp uimageBuffer;\n";

// The offset of the line after the one at i (a directive's "\" continuations followed).
[[nodiscard]] size_t nextLine(const char* s, size_t i)
{
    while(s[i] && s[i] != '\n')
    {
        i += s[i] == '\\' && s[i + 1] == '\n' ? 2 : 1;
    }
    return s[i] ? i + 1 : i;
}

// Where the precision statements go: the first line of code past the leading directives, comments and blank lines
// (after every leading #extension, as ES requires), or the outermost #if around that line (a false branch never
// hides them).
[[nodiscard]] size_t codeStart(const char* s)
{
    size_t i = 0;
    size_t outerIf = 0;
    int depth = 0;
    while(s[i])
    {
        size_t j = i;
        while(s[j] == ' ' || s[j] == '\t' || s[j] == '\r')
        {
            j++;
        }
        if(s[j] == '#')
        {
            j++;
            while(s[j] == ' ' || s[j] == '\t')
            {
                j++;
            }
            if(strings::startsWith(s + j, "if"))
            {
                outerIf = depth == 0 ? i : outerIf;
                depth++;
            }
            else if(strings::startsWith(s + j, "endif") && depth > 0)
            {
                depth--;
            }
        }
        else if(s[j] && s[j] != '\n' && !(s[j] == '/' && s[j + 1] == '/'))
        {
            return depth > 0 ? outerIf : i;
        }
        i = nextLine(s, i);
    }
    return i;
}
} // namespace

extern "C" void VR_GLShaderSource(unsigned int shader, int count, const char* const* strings)
{
    if(!glsafe.esCompat)
    {
        GL_ShaderSourceFunc(shader, count, strings, nullptr);
        return;
    }
    za::String joined;
    for(int k = 0; k < count; k++)
    {
        joined += strings[k];
    }
    const char* s = joined.cStr();
    const size_t body = strings::startsWith(s, "#version") ? nextLine(s, 0) : 0;
    const size_t code = body + codeStart(s + body);
    za::String es{esHeader};
    es += za::String{s + body, static_cast<za::SizeT>(code - body)};
    es += esPrecisions;
    es += s + code;
    const char* text = es.cStr();
    GL_ShaderSourceFunc(shader, 1, &text, nullptr);
    if(COM_CheckParm("-glesverbose")) // (a check run: each error with its source line; the compile waited for)
    {
        GL_CompileShaderFunc(shader);
        GLint ok = 0;
        GL_GetShaderivFunc(shader, GL_COMPILE_STATUS, &ok);
        if(ok)
        {
            return;
        }
        char log[4096];
        log[0] = 0;
        GL_GetShaderInfoLogFunc(shader, sizeof(log), nullptr, log);
        for(const char* l = log; *l; l += strcspn(l, "\n") + (l[strcspn(l, "\n")] ? 1 : 0))
        {
            int line = 0;
            if(sscanf(l, "0(%d)", &line) != 1 || line < 1)
            {
                continue;
            }
            size_t at = 0;
            for(int k = 1; k < line && text[at]; k++)
            {
                at += strcspn(text + at, "\n") + (text[at + strcspn(text + at, "\n")] ? 1 : 0);
            }
            Con_Printf("GLES src: %.*s | %.*s\n", static_cast<int>(strcspn(l, "\n")), l,
                static_cast<int>(strcspn(text + at, "\n")), text + at);
        }
    }
}

extern "C" unsigned VR_GLESImageFormat(unsigned format)
{
    if(!glsafe.esCompat)
    {
        return format;
    }
    switch(format)
    {
        case GL_RG32UI: return GL_RGBA32UI;
        case GL_R8UI: return GL_R32UI;
        case GL_R8: return GL_RGBA8;
        default: return format;
    }
}

extern "C" int VR_GLESShaderError(const char* message)
{
    if(!glsafe.esCompat)
    {
        return 0;
    }
    glsafe.esShaderErrors++;
    if(message)
    {
        Con_Printf("GLES compat: shader error %d:\n%s\n", glsafe.esShaderErrors, message);
    }
    return 1;
}

extern "C" int VR_GLSafeOff(int feature)
{
    const bool off = glsafe.safe || (glsafe.vendorOff & static_cast<unsigned>(feature));
    if(off && !(glsafe.logged & static_cast<unsigned>(feature)))
    {
        glsafe.logged |= static_cast<unsigned>(feature);
        const char* why = glsafe.safe                                                         ? "safe mode"
                          : glsafe.esCompat && (esCompatOff & static_cast<unsigned>(feature)) ? "GLES compat"
                                                                                              : "vendor workaround";
        VR_GLStep("%s: %s off", why, featureName(feature));
        Con_SafePrintf("GL %s: %s off\n", why, featureName(feature));
    }
    return off ? 1 : 0;
}

extern "C" int VR_GLSafeMode(void)
{
    return glsafe.safe;
}

extern "C" const char* VR_GLSafeDescribe(void)
{
    return glsafe.why[0] ? glsafe.why : "not decided yet";
}

namespace
{
void endStartup(const char* how)
{
    if(glsafe.ended || !glsafe.log)
    {
        glsafe.ended = true;
        return;
    }
    VR_GLStep("start-up over: %s", how);
    char line[512];
    q_snprintf(line, sizeof(line), "%s%s; safe mode: %s\n", endLine, how, glsafe.why);
    writeLog(line);
    fclose(glsafe.log);
    glsafe.log = nullptr;
    glsafe.ended = true;
}
} // namespace

// GL_EndRendering, before the swap: the first frames named; the start-up's end, frames after the first map spawned.
extern "C" void VR_GLFrame(void)
{
    if(glsafe.ended)
    {
        return;
    }
    glsafe.frames++;
    if(glsafe.frames <= 3)
    {
        VR_GLStep("frame %d: swap", glsafe.frames);
    }
    if(glsafe.mapSpawned && ++glsafe.framesSinceMap >= framesAfterMap)
    {
        endStartup("the first map drawn");
    }
}

extern "C" void VR_GLMapSpawned(const char* map)
{
    if(!glsafe.ended && !glsafe.mapSpawned)
    {
        glsafe.mapSpawned = true;
        VR_GLStep("map %s spawned", map ? map : "?");
    }
}

// Host_Shutdown: a quit or an error (Sys_Error, reported: not a crash) ends the start-up well.
extern "C" void VR_GLStartupShutdown(int error)
{
    endStartup(error ? "an error (Sys_Error)" : "a quit before the first map");
}

namespace
{
void retry_f()
{
    FILE* f = fopen(glsafe.logPath, "ab");
    if(!f || !glsafe.logPath[0])
    {
        Con_Printf("vr_glsafe_retry: %s cannot be written\n", glsafe.logPath);
        if(f)
        {
            fclose(f);
        }
        return;
    }
    fprintf(f, "%s (vr_glsafe_retry)\n", retryLine);
    fclose(f);
    Con_Printf("The next start tries the full renderer (automatic safe mode off; vr_glsafe is %s)\n", vr_glsafe.string);
}

void status_f()
{
    Con_Printf("GL safe mode: %s\n", VR_GLSafeDescribe());
    for(int bit = 1; bit <= VR_GLSAFE_SHADOW_ATLAS; bit <<= 1)
    {
        const bool off = glsafe.safe || (glsafe.vendorOff & static_cast<unsigned>(bit));
        Con_Printf("  %-60s %s\n", featureName(bit), off ? (glsafe.safe ? "off (safe mode)" : "off (vendor workaround)") : "allowed");
    }
    Con_Printf("Start-up log: %s\n", glsafe.logPath);
    char recent[stepCount * (stepLength + 24)];
    VR_GLRecentSteps(recent, sizeof(recent), 8);
    Con_Printf("Last GL steps:\n%s", recent);
}

// The desktop-GL uses not gated yet by -glescompat, from the audit (QUEST_PLAN.md, "Renderer audit"; the script
// Misc/quakevr/gles_audit.py makes it): each one's place and its OpenGL ES form. C2 gates them one by one.
struct EsPending
{
    const char* what;
    const char* where;
    const char* es;
};
constexpr EsPending esPending[] = {
    {"#version 430 in 52 shader sources", "gl_shaders.c, vr_gfx_gl.cpp, vr_bloom.cpp...", "one prefix: #version 320 es + precision"},
    {"MultiDrawElementsIndirect (bindless path)", "r_world.c, vr_lighting.cpp", "the bound path's DrawElementsIndirect loop"},
    {"glGetTexImage", "gl_texmgr.c, vr_envmap.cpp, vr_pbr.cpp, vr_lighting.cpp", "FBO + glReadPixels"},
    {"glGetBufferSubData", "vr_gfx_gl.cpp, vr_temporal.cpp, vr_water.cpp", "glMapBufferRange(GL_MAP_READ_BIT)"},
    {"glClearTexImage", "vr_temporal.cpp", "clear through an FBO"},
    {"glTextureView", "vr_envmap.cpp, vr_stereo_single.cpp", "render to the array layers directly"},
    {"GL_TEXTURE_LOD_BIAS", "gl_texmgr.c, vr_gfx_gl.cpp", "the bias in the shader (texture(s, uv, bias))"},
    {"GL_DEPTH_CLAMP", "r_alias.c", "EXT_depth_clamp, or the view model's depth range"},
    {"gl_ClipDistance", "gl_shaders.h, r_world.c, r_alias.c, vr_lighting.cpp", "EXT_clip_cull_distance, or a discard"},
    {"noperspective", "gl_shaders.h, vr_glsl_alias.h", "NV_shader_noperspective_interpolation, or w-multiplied"},
    {"textureQueryLevels", "vr_gfx_gl.cpp", "the level count as a uniform"},
    {"timer queries (GL_TIMESTAMP, 64-bit results)", "vr_profile.cpp, vr_tracy.cpp, vr_lighting.cpp", "EXT_disjoint_timer_query"},
    {"glDrawBuffer, glClearDepth, glDepthRange, glPolygonMode", "vr_gfx_gl.cpp, vr_glutil.cpp, gl_rmain.c, gl_vidsdl.c", "glDrawBuffers, ...f, debug only"},
    {"GL_FRAMEBUFFER_SRGB, GL_TEXTURE_CUBE_MAP_SEAMLESS", "vr_imgui.cpp, gl_vidsdl.c", "EXT_sRGB_write_control; always on"},
};

void glesReport_f()
{
    Con_Printf("GLES compat: %s\n", glsafe.esCompat ? "on (-glescompat)" : "off (start with -glescompat)");
    Con_Printf("  bindless %d, multi-bind %d, clip control %d, layered casters %d, buffer storage %d, NV foveation %s\n",
        gl_bindless_able ? 1 : 0, gl_multi_bind_able ? 1 : 0, gl_clipcontrol_able ? 1 : 0, gl_viewport_layer_able,
        gl_buffer_storage_able ? 1 : 0, glsafe.esCompat ? "off" : "when the GPU has it");
    Con_Printf("Not gated yet (C2), with their ES form:\n");
    for(const EsPending& p : esPending)
    {
        Con_Printf("  %s (%s): %s\n", p.what, p.where, p.es);
    }
    Con_Printf("vr_gles_report: %d pending, %d shader errors\n", static_cast<int>(sizeof(esPending) / sizeof(esPending[0])),
        glsafe.esShaderErrors);
}
} // namespace

extern "C" void VR_GLSafeInit(void)
{
    Cmd_AddCommand("vr_glsafe_retry", retry_f);
    Cmd_AddCommand("vr_glsafe_status", status_f);
    Cmd_AddCommand("vr_gles_report", glesReport_f);
}
