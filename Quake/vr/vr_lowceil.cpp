// vr_lowceil.cpp -- see vr_lowceil.hpp.

#include "vr_lowceil.hpp"
#include "vr_comfortfade.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_hull.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace qvr::lowceil
{
namespace
{

// The view's darkening and blur now (0..1), eased towards the hold's (1 while held with the eyes past the ceiling).
float share{0.f};
// The message was queued this frame (the status line).
bool messageShown{false};

// The blur's offset at vr_lowceil_blur 1, in shares of the eye's height (the underwater blur's four taps: 0.0008 a step
// of vr_water_underwater; this is five times its most, a smear rather than a softening).
constexpr float blurPerUnit = 0.004f;
// The message: this far ahead of the eyes along the gaze, its characters this many units (8 * scale).
constexpr float messageDistance = 0.6f; // metres
constexpr float messageScale = 0.06f;

void status_f()
{
    const hull::LocalHold h = hull::localHold();
    Con_Printf("lowceil: hold %d hidden %d dark %.2f blur %.4f message %d eye %.1f ceiling %.1f top %.0f\n", h.kind,
        h.hidden ? 1 : 0, static_cast<double>(share), static_cast<double>(blur()), messageShown ? 1 : 0,
        static_cast<double>(h.eye), static_cast<double>(h.ceiling), static_cast<double>(h.top));
}

} // namespace

void frame(const hands::State& s)
{
    const hull::LocalHold h = hull::localHold();
    const float want = (h.kind != 0 && h.hidden) ? 1.f : 0.f;
    const float seconds = za::clamp(vr_lowceil_fade.value, 0.f, 2.f);
    const float dt = static_cast<float>(za::clamp(host_frametime, 0.0, 0.1));
    const float step = seconds > 0.f ? dt / seconds : 1.f;
    share = want > share ? za::min(want, share + step) : za::max(want, share - step);
    comfortfade::hold(share * za::clamp(vr_lowceil_dark.value, 0.f, 1.f));

    messageShown = false;
    if(h.kind == 0 || vr_lowceil_message.value == 0.f || key_dest != key_game || s.head == glm::vec3{0.f})
    {
        return;
    }
    const char* text = h.kind == 1 ? "Lie back down\nno room to get up here" : "Crouch back down\nno room to stand here";
    const glm::vec3 angles{s.headAngles.x, s.headAngles.y, 0.f};
    const glm::vec3 at = s.head + hands::forward(angles) * (messageDistance * units::metresToUnits());
    text3d::queueOverlay(text, at, angles, messageScale);
    messageShown = true;
}

float blur()
{
    return share * za::clamp(vr_lowceil_blur.value, 0.f, 4.f) * blurPerUnit;
}

void clear()
{
    share = 0.f;
    messageShown = false;
    comfortfade::hold(0.f);
}

void registerCommands()
{
    Cmd_AddCommand("vr_lowceil_status", status_f);
}

} // namespace qvr::lowceil
