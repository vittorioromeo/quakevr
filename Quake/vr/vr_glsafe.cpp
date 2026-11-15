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
#include "vr_envmap.hpp"
#include "vr_lighting.hpp"
#include "vr_strings.hpp"
#include "vr_testkit.hpp"
#include "vr_tunables.hpp"

#include <cctype>
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
    unsigned esKeep = 0;    // -gleskeep: the VR_GLES_FORM_* whose PC form stays (a measurement of each one's cost)
    bool esNoExt = false;   // -glesnoext: as an ES driver without the optional extensions we have a fallback for
    int esShaderErrors = 0; // -glescompat: programs that failed as ES 3.2 GLSL (VR_GLESShaderError)
    za::String esDefineUsed; // vr_gles_define as the ES shaders last took it (VR_GLESDefineChanged)
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

// -gleskeep <list> (QVR_GLES_KEEP=<list>; comma-separated): under -glescompat, these switches keep their PC form, one
// run each, to measure what each ES form costs (QUEST_PLAN.md section 6b). The safe-mode features (the PC feature
// kept on) and -glescompat's own forms (VR_GLESForm).
struct EsKeepName
{
    const char* name;
    unsigned feature; // VR_GLSAFE_*: back on
    unsigned form;    // VR_GLES_FORM_*: the PC form kept
};
constexpr EsKeepName esKeepNames[] = {
    {"bindless", VR_GLSAFE_BINDLESS, 0},
    {"multibind", VR_GLSAFE_MULTI_BIND, 0},
    {"layer", VR_GLSAFE_VIEWPORT_LAYER, 0},
    {"clip", VR_GLSAFE_CLIP_CONTROL, 0},
    {"glsl", 0, VR_GLES_FORM_GLSL},
    {"foveation", 0, VR_GLES_FORM_FOVEATION},
    {"views", 0, VR_GLES_FORM_VIEWS},
};

// The -gleskeep list applied to the run's switches; what was kept logged.
void applyEsKeep(const char* list)
{
    char kept[256] = "";
    for(const EsKeepName& k : esKeepNames)
    {
        const size_t n = strlen(k.name);
        for(const char* at = strstr(list, k.name); at; at = strstr(at + 1, k.name))
        {
            if((at == list || at[-1] == ',') && (at[n] == 0 || at[n] == ','))
            {
                glsafe.vendorOff &= ~k.feature;
                glsafe.esKeep |= k.form;
                q_strlcat(kept, kept[0] ? ", " : "", sizeof(kept));
                q_strlcat(kept, k.name, sizeof(kept));
                break;
            }
        }
    }
    VR_GLStep("GLES compat: PC forms kept (-gleskeep): %s", kept[0] ? kept : "none");
    Con_Printf("GLES compat: PC forms kept (-gleskeep): %s\n", kept[0] ? kept : "none (names: bindless, multibind, "
                                                                                  "layer, clip, glsl, foveation, views)");
}

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
#ifdef __ANDROID__
    constexpr bool esContext = true; // (Android: always an OpenGL ES 3.2 context, gl_vidsdl.c)
#else
    constexpr bool esContext = false;
#endif
    if(esContext || COM_CheckParm("-glescompat") || getenv("QVR_GLES_COMPAT")) // (the variable: bench.sh runs, which take no arguments)
    {
        glsafe.esCompat = true;
        glsafe.vendorOff |= esCompatOff;
        // -glesclipcontrol (QVR_GLES_CLIP_CONTROL=1): as an ES driver with EXT_clip_control (the Quest's: [?], C4),
        // which the shadows, reflections and portals need (without it they are off).
        // Android: kept unless -glesnoclipcontrol (the driver's EXT_clip_control, gl_vidsdl.c; the Quest 3 has it: C4c).
        if(COM_CheckParm("-glesclipcontrol") || getenv("QVR_GLES_CLIP_CONTROL") || (esContext && !COM_CheckParm("-glesnoclipcontrol")))
        {
            glsafe.vendorOff &= ~static_cast<unsigned>(VR_GLSAFE_CLIP_CONTROL);
        }
        // -glesnoext (QVR_GLES_NOEXT=1): as an ES driver without the optional extensions that have a fallback
        // (NV_shader_noperspective_interpolation: the w-multiplied form, VR_GLShaderSource), to test the fallbacks.
        if(const int define = COM_CheckParm("-glesdefine"); define && define < com_argc - 1)
        {
            Con_Printf("GLES compat: every shader with -glesdefine %s (VR_GLShaderSource)\n", com_argv[define + 1]);
        }
        if(COM_CheckParm("-glesnoext") || getenv("QVR_GLES_NOEXT"))
        {
            glsafe.esNoExt = true;
            VR_GLStep("GLES compat: optional extensions treated as absent (-glesnoext)");
            Con_Printf("GLES compat: optional extensions treated as absent (-glesnoext): noperspective w-multiplied\n");
        }
        const int keep = COM_CheckParm("-gleskeep");
        const char* keepList = keep && keep < com_argc - 1 ? com_argv[keep + 1] : getenv("QVR_GLES_KEEP");
        if(keepList && keepList[0])
        {
            applyEsKeep(keepList);
        }
        VR_GLStep("GLES compat (-glescompat): the features OpenGL ES 3.2 lacks off");
        Con_Printf("GLES compat (-glescompat): the features OpenGL ES 3.2 lacks off (vr_gles_report)%s\n",
            glsafe.vendorOff & VR_GLSAFE_CLIP_CONTROL ? "" : "; clip control kept (-glesclipcontrol: EXT_clip_control)");
    }
}

extern "C" int VR_GLESCompat(void)
{
    return glsafe.esCompat ? 1 : 0;
}

extern "C" int VR_GLESForm(int form)
{
    return glsafe.esCompat && !(glsafe.esKeep & static_cast<unsigned>(form)) ? 1 : 0;
}

// C4g: the world programs' lite form under the ES profile: the parts of the world's fragment shader the settings turn
// off compiled out (Adreno pays for their mere presence: QUEST_PLAN.md 6n, e1m1 GPU 18.7 -> 15.7 ms a frame).
// Bits: 1 no parallax mapping (vr_parallax or vr_normalmaps 0), 2 no carved decals (vr_decal_carve
// 0). The programs are made again when this changes (GL_WorldLiteCheck). -glesfull: never (the whole shader, as the PC's).
// C4m (QUEST_PLAN.md 6t): vr_gles_define no longer what the ES shaders were made with (GL_WorldLiteCheck makes them again).
extern "C" int VR_GLESDefineChanged(void)
{
    return VR_GLESForm(VR_GLES_FORM_GLSL) && strcmp(vr_gles_define.string, glsafe.esDefineUsed.cStr()) != 0;
}

extern "C" int VR_GLESWorldLite(void)
{
    if(!VR_GLESForm(VR_GLES_FORM_GLSL) || vr_gl_world_lite.value <= 0.f || COM_CheckParm("-glesfull"))
    {
        return 0;
    }
    int lite = 0;
    if(vr_parallax.value == 0.f || vr_normalmaps.value == 0.f)
    {
        lite |= 1;
    }
    if(vr_decal_carve.value == 0.f)
    {
        lite |= 2;
    }
    if(vr_gl_alias_lite.value != 0.f)
    {
        lite |= 256; // C4j: the models' programs take bits 1 and 32 (gl_shaders.c GL_AliasLiteDefines)
    }
    if(vr_gl_alias_plain.value != 0.f)
    {
        // C4k: the models' plain form, its features from bit 9 (gl_shaders.c GL_AliasPlainDefines; vr_render.cpp
        // VR_AliasPlainNeeds)
        lite |= (static_cast<int>(vr_gl_alias_plain_mask.value) & 1023) << 9;
    }
    if(vr_gl_world_lite.value < 2.f)
    {
        return lite;
    }
    // C4i (QUEST_PLAN.md 6p): the other switches off in the Quest preset or his settings, each a uniform the shader
    // tests (gl_shaders.h QVR_WORLD_FS_LITE): 4 normal maps, 8 the dynamic ambient occlusion, 16 caustics, 32 retro
    // lighting, 64 retro textures, 128 detail textures.
    const struct
    {
        bool off;
        int bit;
    } more[] = {
        {vr_normalmaps.value == 0.f, 4},
        {vr_ao_dynamic.value <= 0.f && vr_ao_brush.value <= 0.f, 8},
        {vr_water_caustics.value == 0.f, 16},
        {vr_retrolight.value == 0.f, 32},
        // (64, retro textures, never: with RetroBegin compiled out Adreno's compiler (Quest 3, driver 0837) can read every
        // world texture at its smallest mip level, the walls flat in their average colours (C4j, QUEST_PLAN.md 6q). It
        // depends on the rest of the shader: C4m's eyeshots had them textured with the lights' lite form on, flat with it
        // off (6t), so it is no safe cut (-0.8 ms in a fight, -0.4 at the start). -glesdefine/vr_gles_define
        // QVR_LITE_NORETRO, QVR_ALITE_NORETRO (the models') still try it.)
        {vr_detail.value == 0.f, 128},
    };
    for(const auto& m : more)
    {
        lite |= m.off ? m.bit : 0;
    }
    if(vr_gl_lights_plain.value != 0.f && vr_shadow_dlights.value <= 0.f && vr_shadow_maplights.value <= 0.f &&
        vr_dlight_falloff.value == 0.f && (vr_portals.value <= 0.f || vr_teleporters.value <= 0.f))
    {
        // C4p (QUEST_PLAN.md 6w): every light plain (no shadows, portal lights, DarkPlaces falloff): the world's lights'
        // loop without their code, 1 << 26 (gl_shaders.c GL_LightsLiteDefines). The Quest 3 paid ~3.5 ms of the world's
        // shading for each light covering the view with that code merely compiled in (Adreno: registers), ~0.3 without.
        lite |= 1 << 26;
    }
    if(vr_gl_lights_lite.value != 0.f)
    {
        // C4l (QUEST_PLAN.md 6s; off by default since C4m, 6t: measured in one launch it cost 0.6 ms of a fight, its
        // parts' gains below were launch-to-launch variance): the dynamic lights' code the settings leave unused, out of
        // the lights' loops: 1 << 19 the map
        // lights' shadows of moving things (vr_shadow_maplights 0: no light carries one), 1 << 20 DarkPlaces' falloff
        // (vr_dlight_falloff 0; the models' loop too), 1 << 21 the shadow filter's kernel as a constant (vr_shadow_filter
        // in bits 22-23), with retro lighting off (its hard shadows pick their own), 1 << 24 the portal lights' path
        // (vr_portals 0), 1 << 25 one shadow filter's code for spot and point lights: gl_shaders.c GL_LightsLiteDefines.
        if(vr_shadow_maplights.value <= 0.f)
        {
            lite |= 1 << 19;
        }
        if(vr_dlight_falloff.value == 0.f)
        {
            lite |= 1 << 20;
        }
        if(vr_portals.value <= 0.f || vr_teleporters.value <= 0.f)
        {
            lite |= 1 << 24; // the portal lights' path (vr_portals.cpp lightGates makes none): 0 in C4m's one launch
        }
        lite |= 1 << 25; // a spot's and a point light's shadow map read by one filter's code (the same picture): -0.3 ms (C4m)
        if(lite & 32)
        {
            const int filter = static_cast<int>(vr_shadow_filter.value);
            lite |= (1 << 21) | ((filter < 0 ? 0 : filter > 3 ? 3 : filter) << 22);
        }
    }
    return lite;
}

namespace
{
using DrawElementsIndirectFn = void(APIENTRY*)(GLenum, GLenum, const void*);
DrawElementsIndirectFn esDrawElementsIndirect = nullptr; // (the driver's, under -glescompat)

// glMultiDrawElementsIndirect as ES has it (EXT_multi_draw_indirect is not in ES 3.2, the Quest's: [?]): one
// glDrawElementsIndirect a command. (The callers are the bindless world path and the layered shadow casters, both
// off under -glescompat: a safety net for a new caller.)
void APIENTRY esMultiDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect, GLsizei count, GLsizei stride)
{
    const uintptr_t step = stride ? static_cast<uintptr_t>(stride) : 5u * sizeof(GLuint);
    for(GLsizei i = 0; i < count; i++)
    {
        esDrawElementsIndirect(mode, type, reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(indirect) + i * step));
    }
}
} // namespace

// GL_Init, the functions loaded: under -glescompat the GL calls ES lacks replaced by their ES forms.
extern "C" void VR_GLESInstallForms(void)
{
    if(!glsafe.esCompat)
    {
        return;
    }
    esDrawElementsIndirect = GL_DrawElementsIndirectFunc;
    (GL_MultiDrawElementsIndirectFunc) = esMultiDrawElementsIndirect; // (the parentheses: not the counting macro)
}

// -glescompat: every shader as OpenGL ES 3.2 GLSL, so the PC's driver (ARB_ES3_2_compatibility) checks each program
// the way the Quest's will. The source's "#version 430" line becomes "#version 320 es", the ES extensions our gates
// use are enabled where the driver has them (sources test GL_ES and the extension's macro), and precision statements
// go before the first line of code (ES has no default float precision in fragment shaders, none for most sampler and
// image types anywhere). Without -glescompat the strings are given unchanged.
namespace
{
// Without NV_shader_noperspective_interpolation (or under -glesnoext) QVR_NOPERSP_W is defined: the shaders' four
// noperspective values (the light tile coordinate in the world's and the models' shaders, the retro affine texture
// coordinate) go out times gl_Position.w and perspective-interpolated, and come in times gl_FragCoord.w (the
// interpolated 1/w): the screen-linear interpolation, exact up to rounding.
constexpr const char* esHeader = "#version 320 es\n"
                                 "#ifdef GL_EXT_clip_cull_distance\n#extension GL_EXT_clip_cull_distance : enable\n#endif\n"
                                 "#ifdef GL_NV_shader_noperspective_interpolation\n"
                                 "#extension GL_NV_shader_noperspective_interpolation : enable\n"
                                 "#else\n#define QVR_NOPERSP_W 1\n#endif\n";
constexpr const char* esHeaderNoExt = "#version 320 es\n"
                                      "#ifdef GL_EXT_clip_cull_distance\n#extension GL_EXT_clip_cull_distance : enable\n#endif\n"
                                      "#define QVR_NOPERSP_W 1\n";

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

// Adreno (the Quest 3's GL ES driver, 0837.0.9) reads a struct copied whole out of a buffer wrong: `Instance instance =
// instance_data[i];` gives a matrix that drops every vertex (QUEST_PLAN.md 6j, C4d: the world's and the models' black
// eyes), while the same members read one by one are right. So under the ES form every `T v = b[i];` of a struct T
// declared in the source becomes `T v; v.m = b[i].m; ...`, member by member (an array member element by element), the
// struct's own #if lines kept around its members. The PC's -glescompat compiles the same rewrite (gles_render_test).
[[nodiscard]] bool isIdent(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

[[nodiscard]] size_t skipBlanks(const char* s, size_t i)
{
    while(s[i] == ' ' || s[i] == '\t')
    {
        i++;
    }
    return i;
}

[[nodiscard]] size_t identEnd(const char* s, size_t i)
{
    while(isIdent(s[i]))
    {
        i++;
    }
    return i;
}

struct GlslStruct
{
    const char* name = nullptr;
    size_t nameLen = 0;
    const char* body = nullptr; // between its braces
    size_t bodyLen = 0;
};
constexpr int maxGlslStructs = 32;

// The structs a source declares ("struct Name" at a line's start, its members up to the closing brace).
[[nodiscard]] int findStructs(const char* s, GlslStruct* out)
{
    int n = 0;
    for(size_t i = 0; s[i] && n < maxGlslStructs; i = nextLine(s, i))
    {
        size_t j = skipBlanks(s, i);
        if(!strings::startsWith(s + j, "struct") || isIdent(s[j + 6]))
        {
            continue;
        }
        j = skipBlanks(s, j + 6);
        const size_t nameEnd = identEnd(s, j);
        const char* open = strchr(s + nameEnd, '{');
        const char* close = open ? strchr(open, '}') : nullptr;
        if(nameEnd == j || !close || strcspn(s + nameEnd, ";{") != static_cast<size_t>(open - (s + nameEnd)))
        {
            continue;
        }
        out[n++] = GlslStruct{s + j, nameEnd - j, open + 1, static_cast<size_t>(close - open - 1)};
    }
    return n;
}

// The member-by-member copy of the struct st from `src` (b[i]) into v: its #if lines kept, its declarations split.
void appendMemberCopies(za::String& out, const GlslStruct& st, const za::String& v, const za::String& src)
{
    const za::String body{st.body, static_cast<za::SizeT>(st.bodyLen)};
    const char* b = body.cStr();
    for(size_t i = 0; b[i]; i = nextLine(b, i))
    {
        const size_t lineEnd = i + strcspn(b + i, "\n");
        size_t end = i;
        while(end < lineEnd && !(b[end] == '/' && b[end + 1] == '/'))
        {
            end++;
        }
        size_t j = skipBlanks(b, i);
        if(j >= end)
        {
            continue;
        }
        if(b[j] == '#')
        {
            out += "\n";
            out += za::String{b + j, static_cast<za::SizeT>(end - j)};
            out += "\n";
            continue;
        }
        j = skipBlanks(b, identEnd(b, j)); // (the type)
        while(j < end && isIdent(b[j]))
        {
            const size_t nameEnd = identEnd(b, j);
            const za::String name{b + j, static_cast<za::SizeT>(nameEnd - j)};
            size_t k = skipBlanks(b, nameEnd);
            if(b[k] == '[')
            {
                const size_t sizeEnd = k + strcspn(b + k, "]");
                const za::String size{b + k + 1, static_cast<za::SizeT>(sizeEnd - k - 1)};
                // A literal size: element by element, unrolled (C4e: a loop's index into the local struct's array made
                // Adreno keep the whole struct in scratch memory, every pixel of the world's light loop paying for
                // it: 270 ms a frame on the Quest 3); else a loop
                const char* sz = size.cStr();
                const int literal = sz[strspn(sz, "0123456789")] == '\0' ? atoi(sz) : 0;
                for(int e = 0; e < (literal > 0 && literal <= 16 ? literal : 0); e++)
                {
                    out += va(" %s.%s[%d] = %s.%s[%d];", v.cStr(), name.cStr(), e, src.cStr(), name.cStr(), e);
                }
                if(literal <= 0 || literal > 16)
                {
                    out += " for (int qvr_k = 0; qvr_k < ";
                    out += size;
                    out += "; qvr_k++) ";
                    out += v;
                    out += ".";
                    out += name;
                    out += "[qvr_k] = ";
                    out += src;
                    out += ".";
                    out += name;
                    out += "[qvr_k];";
                }
                k = skipBlanks(b, sizeEnd + 1);
            }
            else
            {
                out += " ";
                out += v;
                out += ".";
                out += name;
                out += " = ";
                out += src;
                out += ".";
                out += name;
                out += ";";
            }
            j = b[k] == ',' ? skipBlanks(b, k + 1) : end; // (the next declarator, or the line's end)
        }
    }
}

// The source with every `T v = b[i];` of a struct T it declares made member by member.
[[nodiscard]] za::String expandStructCopies(const char* s)
{
    GlslStruct structs[maxGlslStructs];
    const int count = findStructs(s, structs);
    za::String out;
    size_t copied = 0; // s up to here is in out
    for(size_t i = 0; count > 0 && s[i]; i = nextLine(s, i))
    {
        const size_t t = skipBlanks(s, i);
        const size_t typeEnd = identEnd(s, t);
        const GlslStruct* st = nullptr;
        for(int k = 0; k < count && typeEnd > t; k++)
        {
            if(structs[k].nameLen == typeEnd - t && memcmp(structs[k].name, s + t, typeEnd - t) == 0)
            {
                st = &structs[k];
            }
        }
        if(!st)
        {
            continue;
        }
        const size_t v = skipBlanks(s, typeEnd);
        const size_t vEnd = identEnd(s, v);
        size_t at = skipBlanks(s, vEnd);
        if(vEnd == v || s[at] != '=')
        {
            continue;
        }
        const size_t from = skipBlanks(s, at + 1);
        at = identEnd(s, from);
        if(at == from || s[at] != '[')
        {
            continue;
        }
        int depth = 0; // the index's closing bracket (nested ones inside it)
        do
        {
            depth += s[at] == '[' ? 1 : s[at] == ']' ? -1 : 0;
            at++;
        } while(s[at] && s[at] != '\n' && depth > 0);
        const size_t semi = skipBlanks(s, at);
        if(depth != 0 || s[semi] != ';')
        {
            continue;
        }
        const za::String var{s + v, static_cast<za::SizeT>(vEnd - v)};
        out += za::String{s + copied, static_cast<za::SizeT>(t - copied)};
        out += za::String{st->name, static_cast<za::SizeT>(st->nameLen)};
        out += " ";
        out += var;
        out += ";";
        appendMemberCopies(out, *st, var, za::String{s + from, static_cast<za::SizeT>(at - from)});
        copied = semi + 1; // (the rest of its line after it)
    }
    out += s + copied;
    return out;
}
// The source with the `centroid` qualifier dropped (the word as a whole, before an `in`/`out`). C4g: Adreno (Quest 3,
// driver 0837.0.9) interpolates the world's other inputs wrong once its centroid lightmap coordinates are read (the
// texture coordinates garbage: the world's textures stretched and crawling). Without MSAA (the Quest preset's) centroid
// and centre sampling are the same; with it, a lightmap can bleed a little at a polygon's edge.
[[nodiscard]] za::String dropCentroid(const char* s)
{
    constexpr const char word[] = "centroid";
    constexpr size_t wordLen = sizeof(word) - 1;
    za::String out;
    size_t copied = 0;
    for(const char* p = strstr(s, word); p; p = strstr(p + wordLen, word))
    {
        const size_t at = static_cast<size_t>(p - s);
        const bool startsWord = at == 0 || !(isalnum(static_cast<unsigned char>(s[at - 1])) || s[at - 1] == '_');
        const char after = s[at + wordLen];
        if(!startsWord || !(after == ' ' || after == '\t'))
        {
            continue;
        }
        out += za::String{s + copied, static_cast<za::SizeT>(at - copied)};
        copied = at + wordLen + 1; // (and its blank)
    }
    out += s + copied;
    return out;
}
// The ES 3.2 form of a shader's strings: the text VR_GLShaderSource gives the driver under VR_GLES_FORM_GLSL.
[[nodiscard]] za::String esShaderText(int count, const char* const* strings)
{
    za::String whole;
    for(int k = 0; k < count; k++)
    {
        whole += strings[k];
    }
    if(!COM_CheckParm("-glescentroid")) // (-glescentroid: kept, as the PC's: a driver update's check)
    {
        whole = dropCentroid(whole.cStr());
    }
    const za::String joined = COM_CheckParm("-glesstructcopy") ? whole : expandStructCopies(whole.cStr()); // (-glesstructcopy: the copies left whole, as the PC's)
    const char* s = joined.cStr();
    const size_t body = strings::startsWith(s, "#version") ? nextLine(s, 0) : 0;
    const size_t code = body + codeStart(s + body);
    za::String es{glsafe.esNoExt ? esHeaderNoExt : esHeader};
    // -glesdefine A,B,...: "#define A 1" (and B ...) in every ES shader's header: the shaders' QVR_PROBE_* switches
    // (gl_shaders.h; QUEST_PLAN.md 6j: the world and model programs split on a device)
    // and vr_gles_define's (C4m: the same in a session, the shaders made again when it changes: GL_WorldLiteCheck)
    const auto addDefines = [&es](const char* list) {
        for(const char* d = list; *d; d += strcspn(d, ",") + (d[strcspn(d, ",")] ? 1 : 0))
        {
            const size_t len = strcspn(d, ",");
            if(len > 0)
            {
                es += "#define ";
                es += za::String{d, static_cast<za::SizeT>(len)};
                es += " 1\n";
            }
        }
    };
    const int define = COM_CheckParm("-glesdefine");
    if(define && define < com_argc - 1)
    {
        addDefines(com_argv[define + 1]);
    }
    glsafe.esDefineUsed = vr_gles_define.string;
    addDefines(vr_gles_define.string);
    es += za::String{s + body, static_cast<za::SizeT>(code - body)};
    es += esPrecisions;
    es += s + code;
    return es;
}
} // namespace

extern "C" unsigned long long VR_GLShaderHash(unsigned long long h, int count, const char* const* strings)
{
    if(!VR_GLESForm(VR_GLES_FORM_GLSL))
    {
        for(int k = 0; k < count; k++)
        {
            h = VR_ProgCacheKey(h, strings[k]);
        }
        return h;
    }
    return VR_ProgCacheKey(h, esShaderText(count, strings).cStr());
}

extern "C" void VR_GLShaderSource(unsigned int shader, int count, const char* const* strings)
{
    if(!VR_GLESForm(VR_GLES_FORM_GLSL))
    {
        GL_ShaderSourceFunc(shader, count, strings, nullptr);
        return;
    }
    const za::String es = esShaderText(count, strings);
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
            if((sscanf(l, "0(%d)", &line) != 1 && sscanf(l, "ERROR: 0:%d:", &line) != 1) || line < 1) // (NVIDIA's, Adreno's)
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
    if(!VR_GLESForm(VR_GLES_FORM_GLSL)) // (the GLSL declares the image formats by GL_ES: the two go together)
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

extern "C" void VR_GLPolygonMode(unsigned mode)
{
#ifdef __ANDROID__
    (void)mode; // (no glPolygonMode in OpenGL ES: the wireframe views draw filled)
#else
    if(!glsafe.esCompat)
    {
        glPolygonMode(GL_FRONT_AND_BACK, mode);
    }
#endif
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

// The desktop-GL uses from the audit (QUEST_PLAN.md, "Renderer audit"; the script Misc/quakevr/gles_audit.py makes
// it): each one's place and its OpenGL ES form; esDone: the ones -glescompat takes the ES form of (C2).
struct EsPending
{
    const char* what;
    const char* where;
    const char* es;
};
constexpr EsPending esDone[] = {
    {"#version 430 in every program", "VR_GLShaderSource (all five compile sites)", "#version 320 es, precisions, ES image formats"},
    {"glGetBufferSubData", "gfx::readBuffer", "glMapBufferRange(GL_MAP_READ_BIT)"},
    {"glClearTexImage", "gfx::clearTexture", "a clear through a framebuffer"},
    {"textureQueryLevels", "(no use left)", "-"},
    {"glDrawBuffer, glClearDepth, glDepthRange, glPolygonMode", "gfx::drawNoColour, ...f forms, VR_GLPolygonMode", "glDrawBuffers, float forms, no wireframe"},
    {"GL_TEXTURE_CUBE_MAP_SEAMLESS", "gl_vidsdl.c", "always on in ES (not enabled)"},
    {"NVIDIA Image Scaling (vr_upscale 2)", "vr_upscale.cpp", "off: FSR instead"},
    {"glGetTexImage", "VR_GLGetTexImage (gl_texmgr.c, vr_envmap.cpp, vr_pbr.cpp, vr_lighting.cpp)", "FBO + glReadPixels; depth through an R32UI copy"},
    {"MultiDrawElementsIndirect", "VR_GLESInstallForms (r_world.c, vr_lighting.cpp: bindless and layered, both off)", "a DrawElementsIndirect loop"},
    {"glTextureView", "vr_envmap.cpp, vr_stereo_single.cpp", "off: the whole cube's mipmaps; two-pass stereo (C4: multiview into the swapchain)"},
    {"noperspective (NV_shader_noperspective_interpolation)", "gl_shaders.h, vr_glsl_alias.h (QVR_NOPERSP_W)", "the extension where the driver has it, else (and -glesnoext) w-multiplied"},
    {"GL_TEXTURE_LOD_BIAS", "gl_texmgr.c, vr_gfx_gl.cpp", "not set (gl_lodbias, vr_menu_sharpen: none)"},
    {"pipeline-statistics and GL_SAMPLES_PASSED queries", "gl_rmain.c, r_world.c", "vr_model_fragments, vr_world_fragments refuse"},
    {"GL_FRAMEBUFFER_SRGB, GL_BGRA uploads", "vr_imgui.cpp, r_brush.c", "not read; lightmaps RGBA already"},
};
constexpr EsPending esPending[] = {
    {"GL_DEPTH_CLAMP", "r_alias.c", "EXT_depth_clamp on the Quest [?], else the held item cut at the near plane"},
    {"gl_ClipDistance (compiles with EXT_clip_cull_distance)", "gl_shaders.h, r_world.c, r_alias.c, vr_lighting.cpp", "the extension on the Quest [?], else a discard"},
    {"timer queries", "vr_profile.cpp, vr_tracy.cpp, vr_lighting.cpp, gl_rmain.c", "EXT_disjoint_timer_query: the EXT names, GL_GPU_DISJOINT_EXT (C3)"},
    {"clip control (shadows, reflections, portals need it)", "gl_rmain.c, vr_lighting.cpp, vr_envmap.cpp, vr_portals.cpp", "EXT_clip_control on the Quest [?] (-glesclipcontrol), else a standard-depth path"},
};

void glesReport_f()
{
    Con_Printf("GLES compat: %s%s\n", glsafe.esCompat ? "on (-glescompat)" : "off (start with -glescompat)",
        glsafe.esNoExt ? "; optional extensions treated as absent (-glesnoext): noperspective w-multiplied" : "");
    Con_Printf("  bindless %d, multi-bind %d, clip control %d, layered casters %d, buffer storage %d, NV foveation %s\n",
        gl_bindless_able ? 1 : 0, gl_multi_bind_able ? 1 : 0, gl_clipcontrol_able ? 1 : 0, gl_viewport_layer_able,
        gl_buffer_storage_able ? 1 : 0, VR_GLESForm(VR_GLES_FORM_FOVEATION) ? "off" : "when the GPU has it");
    // the interface limits (C4g: the world's ~30 vertex outputs against Adreno's)
    GLint vout = 0, fin = 0, vvec = 0, vcomp = 0;
    glGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS, &vout);
    glGetIntegerv(GL_MAX_FRAGMENT_INPUT_COMPONENTS, &fin);
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &vvec);
    glGetIntegerv(GL_MAX_VARYING_COMPONENTS, &vcomp);
    Con_Printf("  limits: vertex output components %d, fragment input components %d, varying vectors %d (components %d)\n",
        vout, fin, vvec, vcomp);
    Con_Printf("ES forms taken under -glescompat:\n");
    for(const EsPending& p : esDone)
    {
        Con_Printf("  %s (%s): %s\n", p.what, p.where, p.es);
    }
    Con_Printf("Not gated yet, with their ES form:\n");
    for(const EsPending& p : esPending)
    {
        Con_Printf("  %s (%s): %s\n", p.what, p.where, p.es);
    }
    Con_Printf("vr_gles_report: %d pending, %d shader errors\n", static_cast<int>(sizeof(esPending) / sizeof(esPending[0])),
        glsafe.esShaderErrors);
}

// vr_gles_readback_test (a test: Debug > Tests): every texture read back by glGetTexImage and by its OpenGL ES form
// (gfx::readTexImage: a framebuffer and glReadPixels; depth as bits through an R32UI copy), byte for byte: the
// loaded textures as RGBA bytes, the envmap's faces and levels as floats, the shadow atlases' depth. Runs with or
// without -glescompat (the PC has both); integer and compressed textures are skipped (no ES read).
void readbackTest_f()
{
    int passed = 0, failed = 0, skipped = 0;
    TexMgr_ReadbackTest(&passed, &failed, &skipped);
    const int textures = passed + failed;
    qvr::envmap::readbackTest(passed, failed);
    const int envmap = passed + failed - textures;
    qvr::lighting::readbackTest(passed, failed);
    Con_Printf("Read back: %d textures (%d skipped), %d envmap levels, %d atlases\n", textures, skipped, envmap,
        passed + failed - textures - envmap);
    testkit::verdict("vr_gles_readback_test", passed, failed);
}
} // namespace

extern "C" void VR_GLSafeInit(void)
{
    Cmd_AddCommand("vr_glsafe_retry", retry_f);
    Cmd_AddCommand("vr_glsafe_status", status_f);
    Cmd_AddCommand("vr_gles_report", glesReport_f);
    Cmd_AddCommand("vr_gles_readback_test", readbackTest_f);
}
