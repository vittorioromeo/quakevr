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
#include "vr_testkit.hpp"
#include "vr_strings.hpp"
#include "vr_api.h"

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
    unsigned vendorOff = 0; // the vendor workarounds' features
    unsigned logged = 0;    // features whose "off" line was printed
    char why[256]{};        // why safe mode is on
    bool mapSpawned = false;
    int framesSinceMap = 0;
    int frames = 0;
    int shaderSets = 0;       // GL_CreateShaders runs this session (VR_GLShadersBegin)
    double shaderSetMs = 0.0; // the running set's start (nowMs)
    double aliveMs = 0.0;     // the last "== alive at" line (VR_GLFrame)
};
State glsafe;

// A start that never reached its first map counts against the full renderer (automatic safe mode next time) only when
// it crashed, or died quickly after its shaders without a report: never when stopped during a shader compile, after a
// slow compile (AMD's cold ones take 6-9 s), or after living on a while (closed by hand: the next start's safe mode
// would compile a different set again; Quest, 2026-11-17).
constexpr double aliveEveryMs = 5000.0;   // "== alive at" in gl_startup.log while the start runs (how long it lived)
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

// The last start's log: whether it exists and ended well (its end line), vr_glsafe_retry asked, it crashed; its last
// step, and when its shader sets ran.
struct LastStart
{
    bool exists = false;
    bool ended = false;
    bool retry = false;
    bool autoSafe = false;      // it ran in automatic safe mode
    bool crashed = false;       // "== crashed" (the crash handler)
    bool inShaders = false;     // its last step inside a shader set (begun, not done), or a compile or link
    bool inSet = false;         // a set begun, not done
    double shaderMs = 0.0;      // its finished shader sets' time
    double shadersDoneMs = 0.0; // its last set's end
    double shaderBeganMs = 0.0; // the running set's start
    double lastMs = 0.0;        // its last step's or "alive" line's time
    char lastStep[stepLength]{};
};

constexpr const char* setLine = "shaders: set "; // "shaders: set N begun" / "shaders: set N done in X ms ..."
constexpr const char* aliveLine = "== alive at ";
constexpr const char* crashedLine = "== crashed";

// One line of gl_startup.log into r (the file a line at a time; the judge test's texts too).
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
        r.lastMs = za::max(r.lastMs, atof(line + strlen(aliveLine)));
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
    if(strstr(text, "safe mode: on (automatic"))
    {
        r.autoSafe = true;
    }
    if(strings::startsWith(text, setLine))
    {
        if(strstr(text, " begun"))
        {
            r.inSet = true;
            r.shaderBeganMs = ms;
        }
        else if(r.inSet && strstr(text, " done"))
        {
            r.inSet = false;
            r.shaderMs += ms - r.shaderBeganMs;
            r.shadersDoneMs = ms;
        }
    }
    // (a compile or link step last: a log from before the sets' lines)
    r.inShaders = r.inSet || strings::startsWith(text, "compile ") || strings::startsWith(text, "link ");
    q_strlcpy(r.lastStep, text, sizeof(r.lastStep));
}

void parseLastStartText(LastStart& r, const char* text)
{
    char line[512];
    while(*text)
    {
        const size_t n = strcspn(text, "\r\n");
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

// What the last start says about this one: true automatic safe mode, false not (`why` says which and why; "" when there
// is nothing to say: no log, or it ended well).
bool judgeLastStart(const LastStart& last, char* why, size_t whySize)
{
    why[0] = 0;
    if(!last.exists || last.ended)
    {
        return false;
    }
    const char* step = last.lastStep[0] ? last.lastStep : "?";
    if(last.crashed)
    {
        q_snprintf(why, whySize, "on (automatic: the last start crashed before its first map, in \"%s\")", step);
        return true;
    }
    if(last.inShaders)
    {
        q_snprintf(why, whySize, "off (the last start was stopped during a shader compile, %.0f s in, in \"%s\": not counted)",
            last.lastMs / 1000.0, step);
        return false;
    }
    if(last.shaderMs >= slowShadersMs)
    {
        q_snprintf(why, whySize, "off (the last start was stopped before its first map after %.0f s of shader compiles: a slow "
                                 "start, not counted)", last.shaderMs / 1000.0);
        return false;
    }
    if(last.lastMs - last.shadersDoneMs >= quickDeathMs)
    {
        q_snprintf(why, whySize, "off (the last start lived %.0f s past its shaders and was stopped before its first map, "
                                 "without a crash: not counted)", (last.lastMs - last.shadersDoneMs) / 1000.0);
        return false;
    }
    q_snprintf(why, whySize, "on (automatic: the last start ended %.0f s in, before its first map, without a crash report, "
                             "after \"%s\")", last.lastMs / 1000.0, step);
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
        Con_Printf("\x02GL safe mode %s: optional GL features off (gl_startup.log; Debug > Startup)\n", glsafe.why + 3);
    }
    else if(strcmp(glsafe.why, "off") != 0)
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
    const bool amd = (vendor && (strstr(vendor, "ATI") || strstr(vendor, "AMD"))) ||
                     (renderer && (strstr(renderer, "Radeon") || strstr(renderer, "AMD")));
    glsafe.vendorOff = amd && vr_gl_workarounds.value != 0.f ? amdDefaults : 0u;
    if(glsafe.vendorOff)
    {
        VR_GLStep("vendor workarounds (AMD; vr_gl_workarounds 1): %s off", featureName(VR_GLSAFE_BINDLESS));
    }
}

extern "C" int VR_GLSafeOff(int feature)
{
    const bool off = glsafe.safe || (glsafe.vendorOff & static_cast<unsigned>(feature));
    if(off && !(glsafe.logged & static_cast<unsigned>(feature)))
    {
        glsafe.logged |= static_cast<unsigned>(feature);
        VR_GLStep("%s: %s off", glsafe.safe ? "safe mode" : "vendor workaround", featureName(feature));
        Con_SafePrintf("GL %s: %s off\n", glsafe.safe ? "safe mode" : "vendor workaround", featureName(feature));
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

extern "C" void VR_GLShadersBegin(void)
{
    glsafe.shaderSets++;
    glsafe.shaderSetMs = nowMs();
    VR_GLStep("%s%d begun", setLine, glsafe.shaderSets);
}

extern "C" void VR_GLShadersEnd(int programs)
{
    const double ms = nowMs() - glsafe.shaderSetMs;
    VR_GLStep("%s%d done in %.0f ms (%d programs)", setLine, glsafe.shaderSets, ms, programs);
    Con_DPrintf("GL shaders: set %d this run, %d programs in %.0f ms\n", glsafe.shaderSets, programs, ms);
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
// vr_glsafe_judge_test (Debug > Profiling): judgeLastStart on made-up gl_startup.log texts: a crash counts, a start
// stopped during a shader compile or after a slow one does not, a quick death after the shaders does.
void judgeTest_f()
{
    struct Case
    {
        const char* name;
        const char* log;
        bool counted;
    };
    const Case cases[] = {
        {"ended well", "    0.0 ms  safe mode: off\n  900.0 ms  shaders: set 1 begun\n== ended: the first map drawn\n", false},
        {"crashed in the shaders", "    0.0 ms  safe mode: off\n  500.0 ms  shaders: set 1 begun\n  600.0 ms  compile world vertex shader\n== crashed\n", true},
        {"stopped in a compile", "    0.0 ms  safe mode: off\n  500.0 ms  shaders: set 1 begun\n 61000.0 ms  compile world fragment shader\n", false},
        {"stopped in the second set", "  500.0 ms  shaders: set 1 begun\n 3000.0 ms  shaders: set 1 done in 2500 ms\n 9000.0 ms  shaders: set 2 begun\n 9100.0 ms  link world\n", false},
        {"stopped after a slow compile", "  500.0 ms  shaders: set 1 begun\n 50500.0 ms  shaders: set 1 done in 50000 ms\n 50600.0 ms  framebuffers (GL_CreateFrameBuffers)\n", false},
        {"lived on, then stopped", "  500.0 ms  shaders: set 1 begun\n 2500.0 ms  shaders: set 1 done in 2000 ms\n 2600.0 ms  frame 1: swap\n== alive at 40000.0 ms\n", false},
        {"died quickly after the shaders", "  500.0 ms  shaders: set 1 begun\n 2500.0 ms  shaders: set 1 done in 2000 ms\n 2600.0 ms  framebuffers (GL_CreateFrameBuffers)\n", true},
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
        t.check(counted == c.counted, "vr_glsafe_judge_test",
            va("%s: %s, not %s", c.name, counted ? "counted" : "not counted", c.counted ? "counted" : "not counted"));
    }
    t.verdict("vr_glsafe_judge_test");
}
} // namespace

extern "C" void VR_GLSafeInit(void)
{
    Cmd_AddCommand("vr_glsafe_retry", retry_f);
    Cmd_AddCommand("vr_glsafe_status", status_f);
    Cmd_AddCommand("vr_gl_sync_report", syncReport_f);
    Cmd_AddCommand("vr_glsafe_judge_test", judgeTest_f);
}
