// vr_main.cpp -- Quake VR module lifetime, core cvars and per-frame update.

#include "vr_engine.hpp"
#include "vr_backend.hpp"
#include "vr_throw.hpp"
#include "vr_client.hpp"
#include "vr_hands.hpp"
#include "vr_input.hpp"
#include "vr_lines.hpp"
#include "vr_text3d.hpp"
#include "vr_twohand.hpp"
#include "vr_cvars.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_server.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include <cstring>
#include <memory>

using namespace qvr;

namespace
{

struct State
{
    std::unique_ptr<qvr::Backend> backend;
    qvr::TrackingState tracking;
    qvr::FrameState frame;
    bool restartRequested{false};
};

State* state = nullptr;

[[nodiscard]] std::unique_ptr<qvr::Backend> createBackend(const char* name)
{
    if(!strcmp(name, "mock"))
    {
        return qvr::makeMockBackend();
    }

    if(!strcmp(name, "openxr"))
    {
        if(auto backend = qvr::makeOpenXrBackend())
        {
            return backend;
        }

        Con_Warning("VR: this build has no OpenXR support\n");
        return nullptr;
    }

    Con_Warning("VR: unknown backend \"%s\" (available: openxr, mock)\n", name);
    return nullptr;
}

void stopBackend()
{
    if(!state->backend)
    {
        return;
    }

    Con_Printf("VR: stopping %s backend\n", state->backend->name());
    state->backend->stop();
    state->backend.reset();
}

void startBackend()
{
    std::unique_ptr<qvr::Backend> backend = createBackend(vr_backend.string);
    if(!backend)
    {
        return;
    }

    if(!backend->start())
    {
        Con_Warning("VR: failed to start %s backend (vr_restart to retry)\n", backend->name());
        backend->stop();
        return;
    }

    Con_Printf("VR: started %s backend\n", backend->name());
    state->backend = std::move(backend);
}

void onBackendSettingChanged(cvar_t* /* var */)
{
    // Applied at the start of the next frame, not from inside the cvar callback, so that
    // settings loaded from config.cfg before video init are handled uniformly.
    if(state)
    {
        state->restartRequested = true;
    }
}

void VR_Restart_f()
{
    if(state)
    {
        state->restartRequested = true;
    }
}

// quake.rc's last command: with VR enabled, start in the vrstart hub (tutorial, settings and
// the mission packs' portals) as the old engine did; otherwise play the attract demos. A map
// or demo started from the command line runs instead of either.
void VR_StartGame_f()
{
    if(cls.state == ca_dedicated)
    {
        return;
    }

    if(vr_enabled.value && !sv.active && !cls.demoplayback && cls.state != ca_connected)
    {
        Cbuf_InsertText("maxplayers 1; deathmatch 0; coop 0; map vrstart\n");
        return;
    }

    Cbuf_InsertText("startdemos demo1 demo2 demo3\n");
}

void printPose(const char* label, const qvr::Pose& pose)
{
    Con_Printf("  %-5s %s pos (%.2f %.2f %.2f) rot (%.2f %.2f %.2f %.2f)\n", label,
        pose.valid ? "valid  " : "invalid", pose.position.x, pose.position.y,
        pose.position.z, pose.orientation.w, pose.orientation.x,
        pose.orientation.y, pose.orientation.z);
}

void VR_Status_f()
{
    if(!state->backend)
    {
        Con_Printf("VR: inactive (vr_enabled %s, vr_backend \"%s\")\n",
            vr_enabled.string, vr_backend.string);
        return;
    }

    Con_Printf("VR: active, backend \"%s\", world scale %g\n",
        state->backend->name(), vr_world_scale.value);
    printPose("head", state->tracking.head);
    printPose("off", state->tracking.hands[qvr::HAND_OFF]);
    printPose("main", state->tracking.hands[qvr::HAND_MAIN]);

    const hands::State& hs = hands::current();
    if(hs.valid)
    {
        for(int h : {qvr::HAND_OFF, qvr::HAND_MAIN})
        {
            Con_Printf("  %-5s angles (%.1f %.1f %.1f), hotspot %d%s%s\n", h == qvr::HAND_MAIN ? "main" : "off",
                hs.rot[h].x, hs.rot[h].y, hs.rot[h].z, hs.hotspot[h], client::grabbing(h) ? ", grabbing" : "",
                twohand::helping(h) ? ", helping two-handed" : "");
        }
        Con_Printf("  two-handed aiming: %s\n", twohand::aiming() ? "yes" : "no");
    }
}

} // namespace

namespace qvr
{

const TrackingState& tracking()
{
    static const TrackingState fallback = standingPose();
    return state && state->backend ? state->tracking : fallback;
}

bool vrActive()
{
    return state && state->backend;
}

Backend* backend()
{
    return state ? state->backend.get() : nullptr;
}

const FrameState& frameState()
{
    static const FrameState none;
    return state && state->backend ? state->frame : none;
}

} // namespace qvr

extern "C" void VR_Init()
{
    state = new State{};

    registerCvars();
    weapons::registerCvars();
    Cvar_SetCallback(&vr_enabled, onBackendSettingChanged);
    Cvar_SetCallback(&vr_backend, onBackendSettingChanged);

    Cmd_AddCommand("vr_status", VR_Status_f);
    Cmd_AddCommand("vr_restart", VR_Restart_f);
    Cmd_AddCommand("menu_vr", menu::command_f);
    Cmd_AddCommand("vr_startgame", VR_StartGame_f);
    registerMockCommands();
    input::init();
    client::init();
    server::init();
    Cmd_AddCommand("vr_dumpview", view::dumpView_f);

    state->restartRequested = true;
}

extern "C" void VR_Shutdown()
{
    if(!state)
    {
        return;
    }

    stopBackend();
    delete state;
    state = nullptr;
}

extern "C" void VR_BeginFrame()
{
    if(!state || cls.state == ca_dedicated)
    {
        return;
    }

    if(state->restartRequested)
    {
        state->restartRequested = false;
        stopBackend();

        if(vr_enabled.value)
        {
            startBackend();
        }
    }

    if(state->backend && !state->backend->beginFrame(state->tracking, state->frame))
    {
        Con_Warning("VR: %s session lost\n", state->backend->name());
        stopBackend();
    }

    lines::clear(); // queued anew every frame (teleport aim, crosshairs)
    text3d::clear();
    throwing::filterGrips(state->tracking); // the analog grip's release, before it becomes a key
    input::update(state->tracking.input); // releases held keys when VR is off

    // Update the hands now, before the move is built (it carries the aim in the view angles).
    input::roomscaleJump(hands::current());
}

extern "C" int VR_IsActive()
{
    return state && state->backend;
}

extern "C" int VR_ModalMessageFrame()
{
    if(!VR_IsActive())
    {
        return 0;
    }

    // Keep the headset's frames going, showing the dialog, and the controllers' keys coming.
    VR_BeginFrame();
    scr_drawdialog = true;
    SCR_UpdateScreen();
    scr_drawdialog = false;
    return 1;
}
