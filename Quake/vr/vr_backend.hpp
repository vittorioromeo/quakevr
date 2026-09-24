// vr_backend.hpp -- abstraction over the XR runtime (OpenXR, or a desktop mock for testing).
//
// A backend only reports tracking state and (later) owns the eye swapchains. All game logic
// stays in the VR module, so that it can be exercised without a headset via the mock backend.

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>

namespace qvr
{

// Tracking-space pose. Units are metres, in the runtime's convention: +Y up, -Z forward.
struct Pose
{
    glm::vec3 position{0.f};
    glm::quat orientation{1.f, 0.f, 0.f, 0.f};
    bool valid{false};
};

enum Hand : int
{
    HAND_OFF = 0,
    HAND_MAIN = 1,
    HAND_COUNT = 2
};

struct TrackingState
{
    Pose head;
    Pose hands[HAND_COUNT];
};

class Backend
{
public:
    virtual ~Backend() = default;

    [[nodiscard]] virtual const char* name() const = 0;

    // Creates the runtime session. Called with the engine's GL context current.
    [[nodiscard]] virtual bool start() = 0;
    virtual void stop() = 0;

    // Pumps runtime events and updates tracking. Returns false if the session was lost.
    [[nodiscard]] virtual bool update(TrackingState& out) = 0;
};

[[nodiscard]] std::unique_ptr<Backend> makeMockBackend();

} // namespace qvr
