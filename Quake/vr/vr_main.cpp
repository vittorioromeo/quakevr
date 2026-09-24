// vr_main.cpp -- Quake VR module lifetime, core cvars and per-frame update.

#include "vr_engine.hpp"
#include "vr_backend.hpp"

#include <cstring>
#include <memory>

namespace
{

cvar_t vr_enabled = {"vr_enabled", "0", CVAR_ARCHIVE};
cvar_t vr_backend = {"vr_backend", "mock", CVAR_ARCHIVE};
cvar_t vr_world_scale = {"vr_world_scale", "1", CVAR_ARCHIVE};

struct State
{
    std::unique_ptr<qvr::Backend> backend;
    qvr::TrackingState tracking;
    bool restartRequested{false};
};

State* state = nullptr;

[[nodiscard]] std::unique_ptr<qvr::Backend> createBackend(const char* name)
{
    if(!strcmp(name, "mock"))
    {
        return qvr::makeMockBackend();
    }

    Con_Warning("VR: unknown backend \"%s\" (available: mock)\n", name);
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
        Con_Warning("VR: failed to start %s backend\n", backend->name());
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
}

} // namespace

extern "C" void VR_Init()
{
    state = new State{};

    Cvar_RegisterVariable(&vr_enabled);
    Cvar_RegisterVariable(&vr_backend);
    Cvar_RegisterVariable(&vr_world_scale);
    Cvar_SetCallback(&vr_enabled, onBackendSettingChanged);
    Cvar_SetCallback(&vr_backend, onBackendSettingChanged);

    Cmd_AddCommand("vr_status", VR_Status_f);

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
    if(!state)
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

    if(state->backend && !state->backend->update(state->tracking))
    {
        Con_Warning("VR: %s session lost\n", state->backend->name());
        stopBackend();
    }
}

extern "C" int VR_IsActive()
{
    return state && state->backend;
}
