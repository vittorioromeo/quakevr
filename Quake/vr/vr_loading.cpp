// vr_loading.cpp -- "Loading..." in the headset before a level change (vr_loading.hpp).

#include "vr_loading.hpp"

#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/Macros.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <string.h>

namespace qvr::loading
{

namespace
{

// A level change's command put off until the headset showed the notice.
struct Pending
{
    za::String command;  // the command line, run again when the notice was shown (the latest, if another came meanwhile)
    bool waiting{false}; // put off: the notice is showing
    bool going{false};   // its second run: not put off again
    int shown{0};        // headset frames submitted with the eye images since it was put off
    int frames{0};       // frames begun since (the way out when none can show it: the session stopped rendering)
    double since{0.0};   // realtime when it was put off
};
Pending pending;
// (A campaign switch's map, its longest load, is put off too: the switch left no world, as at start-up, not a cleared
// one: CL_ForgetModels; ROUND21.md, "The Loading... crash, root cause".)
double previewUntil = 0.0; // vr_loading_preview: the notice shown without a load until then (realtime)

constexpr int framesToShow = 2; // (one is enough when the runtime shows it; the second covers a frame it dropped)
constexpr int framesMost = 10;
constexpr float noticeDistance = 1.2f; // metres in front of the eyes
constexpr float noticeCharSize = 0.04f; // metres: a character's height (4 cm at 1.2 m: the word about 20 degrees wide)

// The headset is showing frames, the notice is on (and the mock headset's only with 2: test runs keep their timing).
[[nodiscard]] bool canShow()
{
    if(vr_loading_notice.value == 0.f || cls.state == ca_dedicated)
    {
        return false;
    }
    const Backend* const be = backend();
    if(!be || (vr_loading_notice.value < 2.f && !strcmp(be->name(), "mock")))
    {
        return false;
    }
    return frameState().shouldRender;
}

// Straight ahead of the eyes (pitch too: wherever you look), facing them.
void queueNotice()
{
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return;
    }
    const float m2u = units::metresToUnits();
    const glm::vec3 angles{s.headAngles.x, s.headAngles.y, 0.f};
    const glm::vec3 at = s.head + hands::forward(angles) * (noticeDistance * m2u);
    text3d::queueOverlay("Loading...", at, angles, noticeCharSize * m2u / 8.f, {}, 0.8f);
}

[[nodiscard]] bool ready()
{
    const bool shown = pending.shown >= framesToShow || pending.frames >= framesMost;
    return shown && realtime - pending.since >= static_cast<double>(vr_loading_hold.value);
}

// vr_loading_wait: put in the command buffer before the waiting command's text; the buffer stops at it (a wait) until
// the notice was shown, then the command runs (its second run, not put off).
void waitCommand()
{
    if(!pending.waiting)
    {
        return;
    }
    if(!ready())
    {
        Cbuf_InsertText("wait\nvr_loading_wait");
        return;
    }
    pending.waiting = false;
    pending.going = true;
    Cbuf_InsertText(pending.command.cStr());
}

// vr_loading_preview [seconds]: the notice shown for that long (default 3) without a load: to look at it (Debug > Tests).
void previewCommand()
{
    const float seconds = Cmd_Argc() > 1 ? Q_atof(Cmd_Argv(1)) : 3.f;
    previewUntil = realtime + static_cast<double>(seconds > 0.f ? seconds : 3.f);
    queueNotice();
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_loading_wait", waitCommand);
    Cmd_AddCommand("vr_loading_preview", previewCommand);
}

void frame()
{
    pending.going = false; // (a second run that never reached VR_LoadingDefer: refused before it)
    if(!pending.waiting)
    {
        if(realtime < previewUntil)
        {
            queueNotice();
        }
        return;
    }
    pending.frames++;
    queueNotice();
}

void submitted(bool eyes)
{
    if(pending.waiting && eyes)
    {
        pending.shown++;
    }
}

bool waiting()
{
    return pending.waiting;
}

} // namespace qvr::loading

using namespace qvr;

extern "C" int VR_LoadingDefer(void)
{
    if(loading::pending.going)
    {
        loading::pending.going = false;
        return 0;
    }
    const char* const args = Cmd_Args();
    za::String line{Cmd_Argv(0)};
    if(args && *args)
    {
        line += " ";
        line += args;
    }
    if(loading::pending.waiting)
    {
        loading::pending.command = ZA_MOVE(line); // another load meanwhile: the latest one runs (once)
        return 1;
    }
    if(!loading::canShow())
    {
        return 0;
    }
    Con_DPrintf("Loading...: \"%s\" put off until the headset shows the notice\n", line.cStr());
    loading::pending = loading::Pending{};
    loading::pending.command = ZA_MOVE(line);
    loading::pending.waiting = true;
    loading::pending.since = realtime;
    loading::queueNotice(); // (this frame's eyes have it too)
    Cbuf_InsertText("vr_loading_wait");
    return 1;
}

extern "C" int VR_LoadingPlaque(void)
{
    return loading::pending.waiting && !(cls.state == ca_connected && cls.signon == SIGNONS) ? 1 : 0;
}
