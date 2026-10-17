// vr_renderdoc.cpp -- RenderDoc's in-application API (vr_renderdoc.hpp).
//
// Detection: only when renderdoc.dll is already loaded in this process (the game was launched from RenderDoc, or
// injected into): GetModuleHandleA, then its RENDERDOC_GetAPI asked for the 1.x API (1.1.2, else 1.1.0, else 1.0.0).
// The DLL is never loaded from here. The structure is declared here as renderdoc_app.h (MIT) has it, up to 1.1.2.
//
// The frame RenderDoc captures ends at the present of the window it hooked: the desktop window's, which the mirror
// presents once a frame (GL_EndRendering), after both eyes were rendered (VR_RenderView): one capture holds both eyes.
// Presents the mirror skips (the unfocused window's vr_mirror_unfocused_hz, the unpaced test frames) end no frame:
// the capture waits for the next one. vr_renderdoc_status counts the presents against the game's frames.

#include "vr_renderdoc.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_menu.hpp"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace qvr::renderdoc
{
namespace
{

#ifdef _WIN32
#define QVR_RDC_CC __cdecl
#else
#define QVR_RDC_CC
#endif

// RENDERDOC_API_1_1_2, in renderdoc_app.h's order.
struct Api
{
    void(QVR_RDC_CC* GetAPIVersion)(int* major, int* minor, int* patch);
    int(QVR_RDC_CC* SetCaptureOptionU32)(int opt, unsigned value);
    int(QVR_RDC_CC* SetCaptureOptionF32)(int opt, float value);
    unsigned(QVR_RDC_CC* GetCaptureOptionU32)(int opt);
    float(QVR_RDC_CC* GetCaptureOptionF32)(int opt);
    void(QVR_RDC_CC* SetFocusToggleKeys)(void* keys, int num);
    void(QVR_RDC_CC* SetCaptureKeys)(void* keys, int num);
    unsigned(QVR_RDC_CC* GetOverlayBits)();
    void(QVR_RDC_CC* MaskOverlayBits)(unsigned andMask, unsigned orMask);
    void(QVR_RDC_CC* RemoveHooks)();
    void(QVR_RDC_CC* UnloadCrashHandler)();
    int(QVR_RDC_CC* SetCaptureFilePathTemplate)(const char* path);
    const char*(QVR_RDC_CC* GetCaptureFilePathTemplate)();
    unsigned(QVR_RDC_CC* GetNumCaptures)();
    unsigned(QVR_RDC_CC* GetCapture)(unsigned idx, char* filename, unsigned* pathlength, unsigned long long* timestamp);
    void(QVR_RDC_CC* TriggerCapture)();
    unsigned(QVR_RDC_CC* IsTargetControlConnected)();
    unsigned(QVR_RDC_CC* LaunchReplayUI)(unsigned connectTargetControl, const char* cmdline);
    void(QVR_RDC_CC* SetActiveWindow)(void* device, void* wndHandle);
    void(QVR_RDC_CC* StartFrameCapture)(void* device, void* wndHandle);
    unsigned(QVR_RDC_CC* IsFrameCapturing)();
    unsigned(QVR_RDC_CC* EndFrameCapture)(void* device, void* wndHandle);
    void(QVR_RDC_CC* TriggerMultiFrameCapture)(unsigned numFrames); // 1.1.0
};

constexpr int versionNone = 0;
constexpr int version1_0_0 = 10000;
constexpr int version1_1_0 = 10100;
constexpr int version1_1_2 = 10102;

enum class Step
{
    Idle,
    Waiting,   // the delay
    MenuShut,  // the menu closed: a couple of frames drawn without it
    Capturing, // triggered: until the capture file is written
};

struct State
{
    Api* api{nullptr};
    int version{versionNone};
    double nextLook{0.0}; // the next search for the DLL (not every frame)
    Step step{Step::Idle};
    int frames{1};
    double deadline{0.0};
    int shutFrame{0};
    bool hideMenu{false};
    bool menuWasShut{false}; // we closed the menu (the page it was on is kept)
    bool menuWasVr{false};
    int menuPage{0};
    unsigned capturesBefore{0};
    double triggeredAt{0.0};
    int triggerFrame{0};
    unsigned presents{0};
    int presentsFrameStart{0};
    unsigned presentsAtTrigger{0};
    char lastPath[512]{};
};
State rd;

void look()
{
    if(rd.api)
    {
        return;
    }
    const double now = Sys_DoubleTime();
    if(now < rd.nextLook)
    {
        return;
    }
    rd.nextLook = now + 1.0;
    using GetApiFn = int(QVR_RDC_CC*)(int, void**);
#ifdef _WIN32
    HMODULE dll = GetModuleHandleA("renderdoc.dll"); // (never LoadLibrary: only what RenderDoc itself put here)
    if(!dll)
    {
        return;
    }
    const FARPROC proc = GetProcAddress(dll, "RENDERDOC_GetAPI");
#else
    // Linux: librenderdoc.so only if RenderDoc put it here (RTLD_NOLOAD: never loaded from here).
    void* dll = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD);
    if(!dll)
    {
        return;
    }
    void* const proc = dlsym(dll, "RENDERDOC_GetAPI");
#endif
    if(!proc)
    {
        return;
    }
    const GetApiFn getApi = reinterpret_cast<GetApiFn>(proc);
    for(const int v : {version1_1_2, version1_1_0, version1_0_0})
    {
        void* out = nullptr;
        if(getApi(v, &out) == 1 && out)
        {
            rd.api = static_cast<Api*>(out);
            rd.version = v;
            rd.presentsFrameStart = host_framecount;
            Con_Printf("renderdoc: attached (API %d.%d.%d)\n", v / 10000, v / 100 % 100, v % 100);
            return;
        }
    }
}

void finish(const char* why)
{
    // The menu back where it was left.
    if(rd.menuWasShut)
    {
        rd.menuWasShut = false;
        if(key_dest != key_menu)
        {
            if(rd.menuWasVr)
            {
                menu::reopen(rd.menuPage);
            }
            else
            {
                M_ToggleMenu_f();
            }
        }
    }
    rd.step = Step::Idle;
    if(why)
    {
        Con_Printf("renderdoc: %s\n", why);
    }
}

void trigger()
{
    rd.capturesBefore = rd.api->GetNumCaptures();
    if(rd.frames > 1 && rd.version >= version1_1_0 && rd.api->TriggerMultiFrameCapture)
    {
        rd.api->TriggerMultiFrameCapture(static_cast<unsigned>(rd.frames));
    }
    else
    {
        rd.api->TriggerCapture();
    }
    rd.triggeredAt = Sys_DoubleTime();
    rd.triggerFrame = host_framecount;
    rd.presentsAtTrigger = rd.presents;
    rd.step = Step::Capturing;
}

bool lastCapturePath(char* out, unsigned size)
{
    const unsigned n = rd.api ? rd.api->GetNumCaptures() : 0;
    if(n == 0)
    {
        return false;
    }
    unsigned len = 0;
    unsigned long long stamp = 0;
    rd.api->GetCapture(n - 1, nullptr, &len, &stamp);
    if(len == 0 || len > size)
    {
        return false;
    }
    return rd.api->GetCapture(n - 1, out, &len, &stamp) != 0;
}

void capture_f()
{
    const int frames = Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : static_cast<int>(vr_renderdoc_frames.value);
    const float delay = Cmd_Argc() > 2 ? static_cast<float>(atof(Cmd_Argv(2))) : 0.f;
    capture(frames, delay);
}

void status_f()
{
    look();
    if(!rd.api)
    {
        Con_Printf("renderdoc: not attached (renderdoc.dll is not in this process; launch the game from RenderDoc)\n");
        return;
    }
    const unsigned n = rd.api->GetNumCaptures();
    char path[512];
    Con_Printf("renderdoc: attached, API %d.%d.%d, %u captures%s\n", rd.version / 10000, rd.version / 100 % 100,
        rd.version % 100, n, rd.step == Step::Idle ? "" : " (a capture is under way)");
    Con_Printf("renderdoc: last capture %s\n", lastCapturePath(path, sizeof(path)) ? path : "none yet");
    if(rd.api->GetCaptureFilePathTemplate)
    {
        Con_Printf("renderdoc: files go to %s_<n>.rdc\n", rd.api->GetCaptureFilePathTemplate());
    }
    Con_Printf("renderdoc: %u window presents in %d frames of the game (one a frame: both eyes in each)\n", rd.presents,
        host_framecount - rd.presentsFrameStart);
    Con_Printf("renderdoc: button delay %g s, %g frame(s), menu %s for the capture\n", vr_renderdoc_delay.value,
        vr_renderdoc_frames.value, vr_renderdoc_hide_menu.value ? "closed" : "kept");
}

} // namespace

bool attached()
{
    look();
    return rd.api != nullptr;
}

bool capture(int frames, float delay)
{
    if(!attached())
    {
        Con_Printf("vr_renderdoc_capture: RenderDoc is not attached (launch the game from RenderDoc)\n");
        return false;
    }
    if(rd.step != Step::Idle)
    {
        Con_Printf("vr_renderdoc_capture: a capture is already under way\n");
        return false;
    }
    rd.frames = frames < 1 ? 1 : frames > 30 ? 30 : frames;
    rd.deadline = Sys_DoubleTime() + (delay > 0.f ? delay : 0.f);
    rd.hideMenu = vr_renderdoc_hide_menu.value != 0.f;
    rd.step = Step::Waiting;
    Con_Printf("vr_renderdoc_capture: %d frame(s) in %g s\n", rd.frames, delay > 0.f ? delay : 0.f);
    return true;
}

bool hidingPanel()
{
    return rd.hideMenu && (rd.step == Step::MenuShut || rd.step == Step::Capturing);
}

void presented()
{
    rd.presents++;
}

bool captureFromCvars()
{
    return capture(static_cast<int>(vr_renderdoc_frames.value), vr_renderdoc_delay.value);
}

void registerCommands()
{
    Cmd_AddCommand("vr_renderdoc_capture", capture_f);
    Cmd_AddCommand("vr_renderdoc_status", status_f);
}

void poll()
{
    if(!rd.api)
    {
        look();
        return;
    }
    switch(rd.step)
    {
        case Step::Idle: break;
        case Step::Waiting:
            if(Sys_DoubleTime() < rd.deadline)
            {
                break;
            }
            if(rd.hideMenu)
            {
                if(key_dest == key_menu && m_state != m_none)
                {
                    rd.menuWasVr = m_state == m_vr;
                    rd.menuPage = rd.menuWasVr ? menu::currentPage() : 0;
                    rd.menuWasShut = true;
                    M_ToggleMenu_f();
                }
                rd.shutFrame = host_framecount; // (the toolgun's panel is left out too: hidingPanel)
                rd.step = Step::MenuShut;
            }
            else
            {
                trigger();
            }
            break;
        case Step::MenuShut:
            if(host_framecount >= rd.shutFrame + 2)
            {
                trigger();
            }
            break;
        case Step::Capturing:
        {
            const unsigned n = rd.api->GetNumCaptures();
            if(n > rd.capturesBefore && !rd.api->IsFrameCapturing())
            {
                char path[512];
                if(!lastCapturePath(path, sizeof(path)))
                {
                    q_strlcpy(path, "(path unknown)", sizeof(path));
                }
                q_strlcpy(rd.lastPath, path, sizeof(rd.lastPath));
                Con_Printf("renderdoc: captured %d frame(s) in %d game frame(s), %u present(s): %s\n", rd.frames,
                    host_framecount - rd.triggerFrame, rd.presents - rd.presentsAtTrigger, path);
                char toast[640];
                q_snprintf(toast, sizeof(toast), "RenderDoc capture saved:\n%s", path);
                finish(nullptr);
                SCR_CenterPrintFor(toast, 6.f);
            }
            else if(Sys_DoubleTime() > rd.triggeredAt + 10.0)
            {
                finish("no capture arrived within 10 s (is the desktop window presenting? an unfocused mirror skips presents)");
            }
            break;
        }
    }
}

} // namespace qvr::renderdoc

extern "C" void VR_RenderDocPresented()
{
    qvr::renderdoc::presented();
}
