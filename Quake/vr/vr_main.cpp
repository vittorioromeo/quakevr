// vr_main.cpp -- Quake VR module lifetime, core cvars and per-frame update.

#include "vr_engine.hpp"
#include "vr_anchor.hpp"
#include "vr_decals.hpp"
#include "vr_lighting.hpp"
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
#include "vr_profile.hpp"
#include "vr_server.hpp"
#include "vr_view.hpp"
#include "vr_voicenotes.hpp"
#include "vr_flashlight.hpp"
#include "vr_weapons.hpp"

#include <cmath>
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

    Con_Printf("VR: active, backend \"%s\" (%s), world scale %g\n",
        state->backend->name(), state->backend->runtimeName(), vr_world_scale.value);

    // The eyes: the size rendered at (vr_render_scale times the image's), and the images handed to
    // the runtime (its recommended size, SteamVR's resolution included; its largest).
    const qvr::EyeSizes sizes = state->backend->eyeSizes();
    const int renderWidth = qvr::scaledEyeSize(sizes.width, sizes.maxWidth);
    const int renderHeight = qvr::scaledEyeSize(sizes.height, sizes.maxHeight);
    const double pixels = static_cast<double>(renderWidth) * renderHeight;
    const double imagePixels = static_cast<double>(sizes.width) * sizes.height;
    Con_Printf("  eyes  rendered %dx%d (vr_render_scale %g: %.0f%% of the image's pixels), images %dx%d (recommended "
               "%dx%d, largest %dx%d)\n",
        renderWidth, renderHeight, vr_render_scale.value, imagePixels > 0.0 ? 100.0 * pixels / imagePixels : 0.0,
        sizes.width, sizes.height, sizes.recommendedWidth, sizes.recommendedHeight, sizes.maxWidth, sizes.maxHeight);

    // The lenses' hidden area (vr_visibility_mask): its share of the image, and its bounds in the
    // eye's tangent space against the eye's field of view (they should lie within it).
    for(int eye = 0; eye < 2; eye++)
    {
        const qvr::HiddenArea* h = state->backend->hiddenArea(eye);
        if(!h)
        {
            Con_Printf("  hidden area: not given by the runtime (no XR_KHR_visibility_mask)\n");
            break;
        }
        const qvr::Fov& fov = state->frame.eyes[eye].fov;
        const float l = std::tan(fov.left), r = std::tan(fov.right), u = std::tan(fov.up), d = std::tan(fov.down);
        double area = 0.0;
        glm::vec2 lo{1e9f}, hi{-1e9f};
        for(std::size_t i = 0; i + 2 < h->indices.size(); i += 3)
        {
            const glm::vec2 a = h->vertices[h->indices[i]];
            const glm::vec2 b = h->vertices[h->indices[i + 1]];
            const glm::vec2 c = h->vertices[h->indices[i + 2]];
            area += 0.5 * std::fabs(static_cast<double>((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)));
        }
        for(const glm::vec2& v : h->vertices)
        {
            lo = glm::min(lo, v);
            hi = glm::max(hi, v);
        }
        const double image = static_cast<double>(r - l) * static_cast<double>(u - d);
        Con_Printf("  hidden area %c: %zu triangles, %.1f%% of the image (%s); x %.2f..%.2f y %.2f..%.2f, view %.2f..%.2f "
                   "%.2f..%.2f\n",
            eye == 0 ? 'L' : 'R', h->indices.size() / 3, image > 0.0 ? 100.0 * area / image : 0.0,
            vr_visibility_mask.value != 0.f ? "masked" : "vr_visibility_mask 0", h->vertices.empty() ? 0.f : lo.x,
            h->vertices.empty() ? 0.f : hi.x, h->vertices.empty() ? 0.f : lo.y, h->vertices.empty() ? 0.f : hi.y, l, r, d, u);
    }
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

int scaledEyeSize(int image, int max)
{
    const float scale = CLAMP(0.25f, vr_render_scale.value, 2.f);
    int size = static_cast<int>(std::lround(static_cast<double>(image) * scale));
    if(max > 0)
    {
        size = q_min(size, max);
    }
    return q_max(size, 16);
}

} // namespace qvr

extern "C" void VR_Init()
{
    state = new State{};

    registerCvars();
    weapons::registerCvars();
    Cvar_SetCallback(&vr_enabled, onBackendSettingChanged);
    Cvar_SetCallback(&vr_backend, onBackendSettingChanged);
    Cvar_SetCallback(&vr_xr_runtime, onBackendSettingChanged);
    Cvar_SetCallback(&vr_xr_runtime_json, onBackendSettingChanged);

    Cmd_AddCommand("vr_status", VR_Status_f);
    Cmd_AddCommand("vr_restart", VR_Restart_f);
    Cmd_AddCommand("menu_vr", menu::command_f);
    Cmd_AddCommand("vr_startgame", VR_StartGame_f);
    registerMockCommands();
    input::init();
    voicenotes::init();
    flashlight::init();
    client::init();
    server::init();
    Cmd_AddCommand("vr_dumpview", view::dumpView_f);
    anchor::registerCommands();
    Cmd_AddCommand("vr_decal_count", decals::count_f);
    lighting::init();
    profile::init();

    state->restartRequested = true;
}

extern "C" void VR_Shutdown()
{
    if(!state)
    {
        return;
    }

    voicenotes::shutdown();
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

    profile::begin("xr wait", false); // the runtime's pacing (xrWaitFrame) and the tracking
    const bool began = !state->backend || state->backend->beginFrame(state->tracking, state->frame);
    profile::end();
    if(!began)
    {
        Con_Warning("VR: %s session lost\n", state->backend->name());
        stopBackend();
    }

    lines::clear(); // queued anew every frame (teleport aim, crosshairs)
    text3d::clear();
    voicenotes::frame(); // after the clear: its indicator is queued anew each frame
    profile::overlay();  // vr_profile 2
    throwing::filterGrips(state->tracking); // the analog grip's release, before it becomes a key
    input::update(state->tracking.input); // releases held keys when VR is off

    // Update the hands now, before the move is built (it carries the aim in the view angles).
    input::roomscaleJump(hands::current());
}

extern "C" int VR_IsActive()
{
    return vrActive();
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
