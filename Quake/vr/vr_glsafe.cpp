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
//   - automatic: the last start's gl_startup.log has no end line (it died before its first map was drawn) and it
//     crashed (the crash handler's "== crashed"), or died within 20 s of its shaders without a report (a driver killing
//     the process); a start stopped during a shader compile, after >= 10 s of them or after living on is not counted
//     (judgeLastStart; said in the log): this start is in safe mode, and says so. Kept on later starts while a start in it ends well (a release
//     build; a dev build tries the full renderer again next time) until "vr_glsafe_retry" (Debug > Startup). Never in
//     automated test runs (QVR_NO_ERROR_DIALOG, QVR_TEST_HIDDEN) unless "-glsafeauto"; "-noglsafe" turns it off.
//   - vendor workarounds (vr_gl_workarounds 1, default): on AMD/ATI, bindless textures off without safe mode
//     (amdDefaults: what crashed 1.0.0 there).
// A start ends well at the first map's 30th drawn frame, or at a quit or an error (Sys_Error: not a crash).

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_startlog.hpp"
#include "vr_api.h"
#include "vr_envmap.hpp"
#include "vr_lighting.hpp"
#include "vr_strings.hpp"
#include "vr_testkit.hpp"
#include "vr_tunables.hpp"
#include "Zancle/Base/Memcmp.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strstr.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

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
    int shaderSets = 0;        // GL_CreateShaders runs this session (VR_GLShadersBegin)
    double shaderSetMs = 0.0;  // the running one's start (nowMs)
    double aliveMs = 0.0;      // the last "== alive" line (VR_GLFrame)
    bool liteFirstTaken = false; // VR_GLESWorldLiteFirst gave the first set's form
    bool keptBuffersLogged = false;
};
State glsafe;

constexpr double aliveEveryMs = 5000.0; // "== alive at" in gl_startup.log while the start runs (how long a stopped start lived)
// A start that never reached its first map counts against the full renderer (automatic safe mode next time) only when
// it crashed (the crash handler's "== crashed"), or died quickly after its shaders without a report (a driver killing
// the process): never when stopped during a shader compile, after a slow compile, or after living on a while (the
// Quest's long first compiles were stopped by hand, and the next start's safe mode recompiled everything at 1/4 of the
// frame rate: his adb session of 2026-11-17).
constexpr double slowShadersMs = 10000.0; // a start whose shader sets took this long: stopped by hand, not counted
constexpr double quickDeathMs = 20000.0;  // counted only when it died within this of its shaders' end

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
    bool autoSafe = false;  // it ran in automatic safe mode
    bool crashed = false;   // "== crashed" (the crash handler)
    bool inShaders = false; // its last step inside a shader set (begun, not done; or a compile, link or binary step)
    bool inSet = false;     // a set begun, not done
    double shaderMs = 0.0;  // its shader sets' time (a running one's up to its last step)
    double shadersDoneMs = 0.0; // its last set's end
    double shaderBeganMs = 0.0; // the running set's start
    double lastMs = 0.0;    // its last step's or "alive" line's time
    char lastStep[stepLength]{};
};

constexpr const char* setBegun = "shaders: set ";  // "shaders: set N begun (why)" / "shaders: set N done in X ms ..."
constexpr const char* aliveLine = "== alive at ";
constexpr const char* crashedLine = "== crashed";

// One line of gl_startup.log into r (the file's text a line at a time; the decision test's lines too).
void parseLastStartLine(LastStart& r, const char* line)
{
    r.exists = true;
    if(strings::startsWith(line, endLine))
    {
        r.ended = true;
        return;
    }
    if(strings::startsWith(line, retryLine))
    {
        r.retry = true;
        return;
    }
    if(strings::startsWith(line, crashedLine))
    {
        r.crashed = true;
        return;
    }
    if(strings::startsWith(line, aliveLine))
    {
        r.lastMs = za::max(r.lastMs, atof(line + ZA_STRLEN(aliveLine)));
        return;
    }
    if(!line[0] || strings::startsWith(line, "=="))
    {
        return;
    }
    // a step: "%9.1f ms  <text>"
    char* end = nullptr;
    const double ms = strtod(line, &end);
    const char* text = end && strings::startsWith(end, " ms  ") ? end + 5 : line;
    r.lastMs = za::max(r.lastMs, ms);
    if(ZA_STRSTR(text, "safe mode: on (automatic"))
    {
        r.autoSafe = true;
    }
    if(strings::startsWith(text, setBegun) && ZA_STRSTR(text, " begun"))
    {
        r.inSet = true;
        r.shaderBeganMs = ms;
    }
    else if(r.inSet && strings::startsWith(text, setBegun) && ZA_STRSTR(text, " done"))
    {
        r.inSet = false;
        r.shaderMs += ms - r.shaderBeganMs;
        r.shadersDoneMs = ms;
    }
    // inside a set, or a compile, link or binary step last (VR_PrecompilePrograms' after the set, a log from before
    // the sets' lines)
    const bool compileStep = strings::startsWith(text, "compile ") || strings::startsWith(text, "link ") || strings::startsWith(text, "program ");
    r.inShaders = r.inSet || compileStep;
    q_strlcpy(r.lastStep, text, sizeof(r.lastStep));
}

void parseLastStartText(LastStart& r, const char* text)
{
    char line[512];
    while(*text)
    {
        const za::SizeT n = strcspn(text, "\r\n");
        q_strlcpy(line, text, za::min(n + 1, sizeof(line)));
        parseLastStartLine(r, line);
        text += n;
        text += strspn(text, "\r\n");
    }
}

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
        line[strcspn(line, "\r\n")] = 0;
        parseLastStartLine(r, line);
    }
    fclose(f);
    return r;
}

// What the last start says about this one: 1 automatic safe mode, 0 not (`why` says which and why; "" nothing to say).
bool judgeLastStart(const LastStart& last, char* why, za::SizeT whySize)
{
    why[0] = 0;
    if(!last.exists || last.ended)
    {
        return false;
    }
    if(last.crashed)
    {
        q_snprintf(why, whySize, "on (automatic: the last start crashed before its first map, in \"%s\")",
            last.lastStep[0] ? last.lastStep : "?");
        return true;
    }
    const double shaderMs = last.shaderMs + (last.inSet ? last.lastMs - last.shaderBeganMs : 0.0);
    if(last.inShaders)
    {
        q_snprintf(why, whySize, "off (the last start was stopped during a shader compile, %.0f s in, in \"%s\": not counted)",
            last.lastMs / 1000.0, last.lastStep[0] ? last.lastStep : "?");
        return false;
    }
    if(shaderMs >= slowShadersMs)
    {
        q_snprintf(why, whySize, "off (the last start was stopped before its first map after %.0f s of shader compiles: a slow "
                                 "start, not counted)", shaderMs / 1000.0);
        return false;
    }
    if(last.lastMs - last.shadersDoneMs >= quickDeathMs)
    {
        q_snprintf(why, whySize, "off (the last start lived %.0f s past its shaders and was stopped before its first map, "
                                 "without a crash: not counted)", (last.lastMs - last.shadersDoneMs) / 1000.0);
        return false;
    }
    q_snprintf(why, whySize, "on (automatic: the last start ended %.0f s in, before its first map, without a crash report, "
                             "after \"%s\")", last.lastMs / 1000.0, last.lastStep[0] ? last.lastStep : "?");
    return true;
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
        const za::SizeT n = ZA_STRLEN(k.name);
        for(const char* at = ZA_STRSTR(list, k.name); at; at = ZA_STRSTR(at + 1, k.name))
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
    qvr::startlog::glStep(s.text); // the start-up trace's heartbeat: the last GL step (vr_startlog.cpp)
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
    char judged[sizeof(glsafe.why)];
    const bool counted = judgeLastStart(last, judged, sizeof(judged));
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
    else if(autoAllowed && counted)
    {
        glsafe.safe = 1;
        glsafe.safeAuto = true;
        q_strlcpy(glsafe.why, judged, sizeof(glsafe.why));
    }
    else if(autoAllowed && !last.ended && last.autoSafe && !last.retry && !VR_VersionIsDev())
    {
        // The last start ran in automatic safe mode and was stopped before its end (Task Manager, Steam's Stop, the OS
        // closing it): not counted against the full renderer (judgeLastStart), but no evidence the full renderer works
        // either: safe mode kept, as after a safe start that ended (below).
        glsafe.safe = 1;
        glsafe.safeAuto = true;
        q_strlcpy(glsafe.why, "on (automatic, kept: the last start, in safe mode, was stopped before it ended; "
                              "vr_glsafe_retry tries the full renderer again)",
            sizeof(glsafe.why));
    }
    else if(autoAllowed && judged[0])
    {
        q_strlcpy(glsafe.why, judged, sizeof(glsafe.why)); // "off (...: not counted)"
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
        Con_Printf("\x02GL safe mode %s: optional GL features off (gl_startup.log; Debug > Startup; vr_glsafe_retry, "
                   "or delete gl_startup.log, tries the full renderer again)\n", glsafe.why + 3);
    }
    else if(ZA_STRCMP(glsafe.why, "off") != 0)
    {
        Con_Printf("GL safe mode %s\n", glsafe.why); // (a stopped start that was not counted: said)
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
    const bool amd = (vendor && (ZA_STRSTR(vendor, "ATI") || ZA_STRSTR(vendor, "AMD"))) ||
                     (renderer && (ZA_STRSTR(renderer, "Radeon") || ZA_STRSTR(renderer, "AMD")));
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
    return VR_GLESForm(VR_GLES_FORM_GLSL) && ZA_STRCMP(vr_gles_define.string, glsafe.esDefineUsed.cStr()) != 0;
}

// The lite form's bits the settings give, ES or not (VR_GLESWorldLite: the PC's share of them, vr_gl_pc_forms).
static int worldLiteBits(void)
{
    if(vr_gl_world_lite.value <= 0.f || COM_CheckParm("-glesfull"))
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
    if(vr_gl_mediump.value != 0.f)
    {
        lite |= 1 << 29; // qworld: colours and light terms at medium precision (gl_shaders.c QVR_MP; the PC's mask never keeps it)
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
        // (C5f's bit 29, your body's own flat-lit form, gone in C5h: the usual form skips the loop, r_alias.c)
        // qtune: the plain, gore and skin forms' variants (gl_shaders.c GL_AliasFormVariants): bit 29 without the
        // fullbright read (a batch whose skins have none), bit 30 without the dynamic lights' loop (a batch whose lights
        // are all added flat on the CPU: your body and, with vr_gl_alias_flat, the gore; Quake's lights only)
        if(vr_gl_alias_nofb.value != 0.f)
        {
            lite |= 1 << 29;
        }
        if(vr_gl_alias_flat.value != 0.f && (vr_gl_quake_dlights.value != 0.f || vr_gl_alias_body.value >= 2.f))
        {
            lite |= 1 << 30;
        }
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
        (vr_portals.value <= 0.f || vr_teleporters.value <= 0.f))
    {
        // C4p (QUEST_PLAN.md 6w): every light plain (no shadows, portal lights, DarkPlaces falloff): the world's lights'
        // loop without their code, 1 << 26 (gl_shaders.c GL_LightsLiteDefines). The Quest 3 paid ~3.5 ms of the world's
        // shading for each light covering the view with that code merely compiled in (Adreno: registers), ~0.3 without.
        // C4q (6x): DarkPlaces' falloff in the plain loops (vr_dlight_falloff 1, the Quest preset's: the bit was never set
        // there before), and the models' loop the same (vr_glsl_alias.h ModelDynamicLights; QVR_PROBE_NOAPLAIN undoes it).
        lite |= 1 << 26;
        if(vr_rig_lights_world.value == 0.f)
        {
            lite |= 1 << 27; // C4v: the rig's small lights skipped by the world's plain loop (QVR_LITE_RIGNOWORLD)
        }
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
    if(vr_gl_alias_vlight.value != 0.f && (lite & (1 << 26)) != 0 && (lite & 8) != 0)
    {
        // C4y (QUEST_PLAN.md 6ze): the models' plain forms lit at their vertices (gl_shaders.c GL_AliasPlainDefines:
        // QVR_APLAIN_VLIGHT): only with every light plain and no dynamic occlusion, which they leave out (retro lighting keeps
        // its levels, not its blocks)
        lite |= 1 << 28;
    }
    return lite;
}

// pcopt (QUEST_OPTIMISATIONS.md "PC results"): the ES profile's program forms the PC's own GLSL takes too, by
// vr_gl_pc_forms' bits (VR_GL_PCFORM_*): the models' plain, gore and skin forms; the plain lights' loop; the unlit world
// and its split by surface; the lite world's dead features compiled out. Under the ES profile: every one (as before).
extern "C" int VR_GLLiteForms(int part)
{
    if(VR_GLESForm(VR_GLES_FORM_GLSL))
    {
        return 1;
    }
    return (static_cast<int>(vr_gl_pc_forms.value) & part) != 0 ? 1 : 0;
}

extern "C" int VR_GLESWorldLite(void)
{
    if(VR_GLESForm(VR_GLES_FORM_GLSL))
    {
        return worldLiteBits();
    }
    int keep = 0; // the PC: only the parts vr_gl_pc_forms takes (the lights' lite form and the vertex-lit probe never)
    if(VR_GLLiteForms(VR_GL_PCFORM_MODELS))
    {
        keep |= 256 | (1023 << 9); // the models' lite and plain forms (gl_shaders.c GL_AliasLiteDefines, GL_AliasPlainDefines)
    }
    if(VR_GLLiteForms(VR_GL_PCFORM_PLAINLIGHTS))
    {
        keep |= (1 << 26) | (1 << 27);
    }
    if(VR_GLLiteForms(VR_GL_PCFORM_LITEWORLD))
    {
        keep |= 1 | 2 | 4 | 8 | 16 | 32 | 128;
    }
    const int unlit = VR_GLLiteForms(VR_GL_PCFORM_UNLIT) ? VR_GL_PCFORM_UNLIT_BIT : 0; // (the programs made: gl_shaders.c)
    return keep || unlit ? (worldLiteBits() & keep) | unlit : 0;
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
[[nodiscard]] za::SizeT nextLine(const char* s, za::SizeT i)
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
[[nodiscard]] za::SizeT codeStart(const char* s)
{
    za::SizeT i = 0;
    za::SizeT outerIf = 0;
    int depth = 0;
    while(s[i])
    {
        za::SizeT j = i;
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

[[nodiscard]] za::SizeT skipBlanks(const char* s, za::SizeT i)
{
    while(s[i] == ' ' || s[i] == '\t')
    {
        i++;
    }
    return i;
}

[[nodiscard]] za::SizeT identEnd(const char* s, za::SizeT i)
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
    za::SizeT nameLen = 0;
    const char* body = nullptr; // between its braces
    za::SizeT bodyLen = 0;
};
constexpr int maxGlslStructs = 32;

// The structs a source declares ("struct Name" at a line's start, its members up to the closing brace).
[[nodiscard]] int findStructs(const char* s, GlslStruct* out)
{
    int n = 0;
    for(za::SizeT i = 0; s[i] && n < maxGlslStructs; i = nextLine(s, i))
    {
        za::SizeT j = skipBlanks(s, i);
        if(!strings::startsWith(s + j, "struct") || isIdent(s[j + 6]))
        {
            continue;
        }
        j = skipBlanks(s, j + 6);
        const za::SizeT nameEnd = identEnd(s, j);
        const char* open = strchr(s + nameEnd, '{');
        const char* close = open ? strchr(open, '}') : nullptr;
        if(nameEnd == j || !close || strcspn(s + nameEnd, ";{") != static_cast<za::SizeT>(open - (s + nameEnd)))
        {
            continue;
        }
        out[n++] = GlslStruct{s + j, nameEnd - j, open + 1, static_cast<za::SizeT>(close - open - 1)};
    }
    return n;
}

// The member-by-member copy of the struct st from `src` (b[i]) into v: its #if lines kept, its declarations split.
void appendMemberCopies(za::String& out, const GlslStruct& st, const za::String& v, const za::String& src)
{
    const za::String body{st.body, static_cast<za::SizeT>(st.bodyLen)};
    const char* b = body.cStr();
    for(za::SizeT i = 0; b[i]; i = nextLine(b, i))
    {
        const za::SizeT lineEnd = i + strcspn(b + i, "\n");
        za::SizeT end = i;
        while(end < lineEnd && !(b[end] == '/' && b[end + 1] == '/'))
        {
            end++;
        }
        za::SizeT j = skipBlanks(b, i);
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
            const za::SizeT nameEnd = identEnd(b, j);
            const za::String name{b + j, static_cast<za::SizeT>(nameEnd - j)};
            za::SizeT k = skipBlanks(b, nameEnd);
            if(b[k] == '[')
            {
                const za::SizeT sizeEnd = k + strcspn(b + k, "]");
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
    za::SizeT copied = 0; // s up to here is in out
    for(za::SizeT i = 0; count > 0 && s[i]; i = nextLine(s, i))
    {
        const za::SizeT t = skipBlanks(s, i);
        const za::SizeT typeEnd = identEnd(s, t);
        const GlslStruct* st = nullptr;
        for(int k = 0; k < count && typeEnd > t; k++)
        {
            if(structs[k].nameLen == typeEnd - t && ZA_MEMCMP(structs[k].name, s + t, typeEnd - t) == 0)
            {
                st = &structs[k];
            }
        }
        if(!st)
        {
            continue;
        }
        const za::SizeT v = skipBlanks(s, typeEnd);
        const za::SizeT vEnd = identEnd(s, v);
        za::SizeT at = skipBlanks(s, vEnd);
        if(vEnd == v || s[at] != '=')
        {
            continue;
        }
        const za::SizeT from = skipBlanks(s, at + 1);
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
        const za::SizeT semi = skipBlanks(s, at);
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
    constexpr za::SizeT wordLen = sizeof(word) - 1;
    za::String out;
    za::SizeT copied = 0;
    for(const char* p = ZA_STRSTR(s, word); p; p = ZA_STRSTR(p + wordLen, word))
    {
        const za::SizeT at = static_cast<za::SizeT>(p - s);
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
    const za::SizeT body = strings::startsWith(s, "#version") ? nextLine(s, 0) : 0;
    const za::SizeT code = body + codeStart(s + body);
    za::String es{glsafe.esNoExt ? esHeaderNoExt : esHeader};
    // -glesdefine A,B,...: "#define A 1" (and B ...) in every ES shader's header: the shaders' QVR_PROBE_* switches
    // (gl_shaders.h; QUEST_PLAN.md 6j: the world and model programs split on a device)
    // and vr_gles_define's (C4m: the same in a session, the shaders made again when it changes: GL_WorldLiteCheck)
    const auto addDefines = [&es](const char* list) {
        for(const char* d = list; *d; d += strcspn(d, ",") + (d[strcspn(d, ",")] ? 1 : 0))
        {
            const za::SizeT len = strcspn(d, ",");
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
            za::SizeT at = 0;
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

// Safe mode under OpenGL ES (the Quest; -glescompat) keeps persistent mapped buffers: without them every GL_Upload is
// a synchronous copy into a buffer the GPU still reads (Adreno: 34% of the main thread, 19 fps of 72 in his adb session
// of 2026-11-17), and the safety they'd buy is a desktop driver's (the AMD crash). Safe mode never costs 4x.
namespace
{
bool safeKeeps(int feature)
{
    return glsafe.esCompat && feature == VR_GLSAFE_BUFFER_STORAGE;
}
} // namespace

extern "C" int VR_GLSafeOff(int feature)
{
    if(glsafe.safe && safeKeeps(feature) && !glsafe.keptBuffersLogged)
    {
        glsafe.keptBuffersLogged = true;
        VR_GLStep("safe mode: %s kept (OpenGL ES: every upload would wait on the GPU)", featureName(feature));
        Con_SafePrintf("GL safe mode: %s kept (OpenGL ES: without them every upload waits on the GPU)\n", featureName(feature));
    }
    const bool off = (glsafe.safe && !safeKeeps(feature)) || (glsafe.vendorOff & static_cast<unsigned>(feature));
    if(off && !(glsafe.logged & static_cast<unsigned>(feature)))
    {
        glsafe.logged |= static_cast<unsigned>(feature);
        const char* why = glsafe.safe && !safeKeeps(feature)                                  ? "safe mode"
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

// The GL calls that make the CPU wait for the GPU (VR_GLSyncNote: glquake.h's wrappers), counted
// between reports (vr_gl_sync_count, vr_gl_sync_report): the Quest's frames serialised CPU and GPU (2026-11-17).
namespace
{
constexpr const char* syncNames[VR_GLSYNC_KINDS] = {
    "finish", "fence wait", "query result", "pixel read", "texture read", "synced map", "link status"};
struct GlSyncs
{
    unsigned long long counts[VR_GLSYNC_KINDS]{};
    int frames = 0;
    double since = 0.0;
    bool asked = false;    // a vr_gl_sync_report started a count
    bool counting = false; // the queries' availability checked (VR_GLQueryWaits): asked, or vr_gl_sync_count on
} glSyncs;

void syncReport(const char* who)
{
    unsigned long long all = 0;
    for(unsigned long long c : glSyncs.counts)
    {
        all += c;
    }
    const double frames = static_cast<double>(za::max(1, glSyncs.frames));
    char parts[256];
    parts[0] = '\0';
    for(int k = 0; k < VR_GLSYNC_KINDS; k++)
    {
        if(glSyncs.counts[k])
        {
            q_strlcat(parts, va(" %s %.2f", syncNames[k], static_cast<double>(glSyncs.counts[k]) / frames), sizeof(parts));
        }
    }
    Con_Printf("%s: %d frames, %.2f GL syncs a frame%s%s\n", who, glSyncs.frames, static_cast<double>(all) / frames,
        parts[0] ? ":" : "", parts);
    const bool asked = glSyncs.asked;
    glSyncs = GlSyncs{};
    glSyncs.since = Sys_DoubleTime();
    glSyncs.asked = asked;
    glSyncs.counting = true;
}

void syncReport_f()
{
    glSyncs.asked = true;
    syncReport("vr_gl_sync_report");
}
} // namespace

extern "C" void VR_GLSyncNote(int kind)
{
    glSyncs.counts[za::clamp(kind, 0, VR_GLSYNC_KINDS - 1)]++;
}

// A query's result read waits only when it is not there yet (the profiler's reads come after an availability check);
// checked only while counting (vr_gl_sync_count, or after a vr_gl_sync_report): one more query call a read.
extern "C" int VR_GLQueryWaits(GLuint id)
{
    if(!glSyncs.counting)
    {
        return 0;
    }
    GLint available = 0;
    GL_GetQueryObjectivFunc(id, GL_QUERY_RESULT_AVAILABLE, &available);
    return available ? 0 : 1;
}

// GL_EndRendering, before the swap: the first frames named; the start-up's end, frames after the first map spawned.
extern "C" void VR_GLFrame(void)
{
    const bool wasCounting = glSyncs.counting;
    glSyncs.counting = glSyncs.asked || qvr::vr_gl_sync_count.value > 0.f; // (no longer sticky once the cvar is 0)
    if(glSyncs.counting && !wasCounting)
    {
        // vr_gl_sync_count set just now: its first report counts from here (not the frames since the game started, without
        // the query waits counted then)
        const bool asked = glSyncs.asked;
        glSyncs = GlSyncs{};
        glSyncs.since = Sys_DoubleTime();
        glSyncs.asked = asked;
        glSyncs.counting = true;
    }
    glSyncs.frames++;
    if(qvr::vr_gl_sync_count.value > 0.f && Sys_DoubleTime() - glSyncs.since >= qvr::vr_gl_sync_count.value)
    {
        syncReport("vr_gl_sync_count");
    }
    if(glsafe.ended)
    {
        return;
    }
    glsafe.frames++;
    if(glsafe.frames <= 3)
    {
        VR_GLStep("frame %d: swap", glsafe.frames);
    }
    if(const double ms = nowMs(); glsafe.log && ms - glsafe.aliveMs >= aliveEveryMs)
    {
        glsafe.aliveMs = ms; // (how long a start stopped before its first map lived: judgeLastStart)
        char line[64];
        q_snprintf(line, sizeof(line), "%s%.1f ms\n", aliveLine, ms);
        writeLog(line);
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

extern "C" void VR_GLStartupCrashed(void)
{
    if(glsafe.log && !glsafe.ended)
    {
        writeLog("== crashed\n");
    }
}

namespace
{
// gl_shader_set.txt (beside gl_startup.log): the lite form the last set was made with, for the next start's first set
// (VID_Init's, before the config: made with the form the config's settings will ask for, not the defaults', so that
// the config's exec makes nothing again). OpenGL ES only (the PC's lite form is always 0).
za::String shaderSetPath()
{
    char path[1024];
    q_snprintf(path, sizeof(path), "%s/crash/gl_shader_set.txt", com_gamedir);
    return za::String{path};
}
} // namespace

extern "C" int VR_GLESWorldLiteFirst(void)
{
    const int now = VR_GLESWorldLite();
    if(glsafe.liteFirstTaken || !VR_GLESForm(VR_GLES_FORM_GLSL) || (testRun() && !COM_CheckParm("-glesshaderset")))
    {
        glsafe.liteFirstTaken = true;
        return now;
    }
    glsafe.liteFirstTaken = true;
    int kept = now;
    if(FILE* f = fopen(shaderSetPath().cStr(), "rb"))
    {
        if(fscanf(f, "lite %d", &kept) != 1)
        {
            kept = now;
        }
        fclose(f);
    }
    if(kept != now)
    {
        VR_GLStep("shaders: the first set as the last session's (lite form %d; the settings so far %d)", kept, now);
        Con_DPrintf("GL shaders: the first set made with the last session's lite form %d (the defaults' %d)\n", kept, now);
    }
    return kept;
}

extern "C" void VR_GLShadersBegin(const char* why)
{
    glsafe.shaderSets++;
    glsafe.shaderSetMs = nowMs();
    VR_GLStep("%s%d begun (%s)", setBegun, glsafe.shaderSets, why ? why : "?");
}

extern "C" void VR_GLShadersEnd(int lite, int programs)
{
    const double ms = nowMs() - glsafe.shaderSetMs;
    int hits = 0, misses = 0;
    char cache[96] = "the program cache off";
    if(VR_ProgCacheOn())
    {
        VR_ProgCacheCounts(&hits, &misses);
        q_snprintf(cache, sizeof(cache), "%d from the program cache, %d compiled", hits, misses);
    }
    VR_GLStep("%s%d done in %.0f ms (%d programs; %s; lite form %d)", setBegun, glsafe.shaderSets, ms, programs, cache, lite);
    VR_StepNote("GL shaders: set %d this run, %d programs in %.0f ms (%s)", glsafe.shaderSets, programs, ms, cache);
    // (the console's under the ES profile, whose sets and lite forms are the Quest's to watch; the PC's developer only,
    // as before the Quest work)
    const char* const shadersLine = va("GL shaders: set %d this run, %d programs in %.0f ms (%s; lite form %d)\n",
        glsafe.shaderSets, programs, ms, cache, lite);
    if(VR_GLESForm(VR_GLES_FORM_GLSL))
    {
        Con_SafePrintf("%s", shadersLine);
    }
    else
    {
        Con_DPrintf("%s", shadersLine);
    }
    if(VR_GLESForm(VR_GLES_FORM_GLSL) && (!testRun() || COM_CheckParm("-glesshaderset")))
    {
        if(FILE* f = fopen(shaderSetPath().cStr(), "wb"))
        {
            fprintf(f, "lite %d\n", lite);
            fclose(f);
        }
    }
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
        const bool bySafe = glsafe.safe && !safeKeeps(bit);
        const bool off = bySafe || (glsafe.vendorOff & static_cast<unsigned>(bit));
        Con_Printf("  %-60s %s\n", featureName(bit), off ? (bySafe ? "off (safe mode)" : "off (vendor workaround)")
                                                          : glsafe.safe ? "kept (safe mode under OpenGL ES)" : "allowed");
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

} // namespace
extern "C" const char* R_WorldSolidProgramName(void); // (r_world.c)
namespace
{
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
    Con_Printf("vr_gles_report: the opaque world drew last with \"%s\"; vr_retro %g, vr_retrolight %g\n",
        R_WorldSolidProgramName(), vr_retro.value, vr_retrolight.value); // (qworld)
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

// vr_glsafe_judge_test (a test: Debug > Tests): judgeLastStart on made-up gl_startup.log texts: a crash counts, a
// start stopped during a shader compile or after a slow one does not, a quick death after the shaders does.
void judgeTest_f()
{
    struct Case
    {
        const char* name;
        const char* log;
        bool counted;
    };
    const Case cases[] = {
        {"ended well", "    0.0 ms  safe mode: off\n  900.0 ms  shaders: set 1 begun (start-up)\n== ended: the first map drawn\n", false},
        {"crashed in the shaders", "    0.0 ms  safe mode: off\n  500.0 ms  shaders: set 1 begun (start-up)\n  600.0 ms  compile world vertex shader\n== crashed\n", true},
        {"stopped in a compile", "    0.0 ms  safe mode: off\n  500.0 ms  shaders: set 1 begun (start-up)\n 61000.0 ms  compile world fragment shader\n", false},
        {"stopped in the second set", "  500.0 ms  shaders: set 1 begun (start-up)\n 3000.0 ms  shaders: set 1 done in 2500 ms\n 9000.0 ms  shaders: set 2 begun (the settings changed the lite form)\n 9100.0 ms  link world\n", false},
        {"stopped after a slow compile", "  500.0 ms  shaders: set 1 begun (start-up)\n 50500.0 ms  shaders: set 1 done in 50000 ms\n 50600.0 ms  framebuffers (GL_CreateFrameBuffers)\n", false},
        {"lived on, then stopped", "  500.0 ms  shaders: set 1 begun (start-up)\n 2500.0 ms  shaders: set 1 done in 2000 ms\n 2600.0 ms  frame 1: swap\n== alive at 40000.0 ms\n", false},
        {"died quickly after the shaders", "  500.0 ms  shaders: set 1 begun (start-up)\n 2500.0 ms  shaders: set 1 done in 2000 ms\n 2600.0 ms  framebuffers (GL_CreateFrameBuffers)\n", true},
        {"died before the shaders", "    0.0 ms  safe mode: off\n   80.0 ms  GL_VENDOR x; GL_RENDERER y; GL_VERSION z\n", true},
        {"an old log stopped in a compile", "    0.0 ms  safe mode: off\n 45000.0 ms  compile alias fragment shader\n", false},
        {"an old log, a quick death", "    0.0 ms  safe mode: off\n  700.0 ms  link world\n  800.0 ms  framebuffers (GL_CreateFrameBuffers)\n", true},
        {"no log", "", false},
    };
    testkit::Tally t;
    for(const Case& c : cases)
    {
        LastStart r;
        parseLastStartText(r, c.log);
        char why[256];
        const bool counted = judgeLastStart(r, why, sizeof(why));
        Con_Printf("  %-32s %s\n", c.name, why[0] ? why : "(nothing to say)");
        t.check(counted == c.counted, "vr_glsafe_judge_test", va("%s: %s, not %s", c.name, counted ? "counted" : "not counted",
                                                                  c.counted ? "counted" : "not counted"));
    }
    t.verdict("vr_glsafe_judge_test");
}
} // namespace

extern "C" void VR_GLSafeInit(void)
{
    Cmd_AddCommand("vr_glsafe_retry", retry_f);
    Cmd_AddCommand("vr_glsafe_status", status_f);
    Cmd_AddCommand("vr_gles_report", glesReport_f);
    Cmd_AddCommand("vr_gles_readback_test", readbackTest_f);
    Cmd_AddCommand("vr_glsafe_judge_test", judgeTest_f);
    Cmd_AddCommand("vr_gl_sync_report", syncReport_f);
}
