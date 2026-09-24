// vr_backend.hpp -- abstraction over the XR runtime (OpenXR, or a desktop mock for testing).
//
// A backend reports tracking, paces frames and owns the images the eyes are rendered into.
// All game logic stays in the VR module, so that it can run without a headset (mock backend).

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>

namespace qvr
{

// Tracking-space pose. Units are metres, in the runtime's convention: +X right, +Y up,
// -Z forward, origin on the play-space floor.
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
    Pose hands[HAND_COUNT]; // grip poses, [0] off hand, [1] main hand
};

// Field of view of an eye, as tangent-space angles in radians (left and down negative).
struct Fov
{
    float left{-0.8f};
    float right{0.8f};
    float up{0.8f};
    float down{-0.8f};
};

struct EyeView
{
    Pose pose;
    Fov fov;
};

struct FrameState
{
    bool shouldRender{false}; // the runtime wants this frame's eyes rendered
    EyeView eyes[2];          // [0] left, [1] right
};

class Backend
{
public:
    virtual ~Backend() = default;

    [[nodiscard]] virtual const char* name() const = 0;

    // Creates the runtime session. Called with the engine's GL context current.
    [[nodiscard]] virtual bool start() = 0;
    virtual void stop() = 0;

    // Once per host frame: pumps runtime events, waits for the next frame and updates
    // tracking. Returns false if the session was lost.
    [[nodiscard]] virtual bool beginFrame(TrackingState& tracking, FrameState& frame) = 0;

    // Render target size of each eye.
    virtual void eyeResolution(int& width, int& height) const = 0;

    // GL texture to render an eye into, between acquire and release.
    [[nodiscard]] virtual unsigned acquireEyeImage(int eye) = 0;
    virtual void releaseEyeImage(int eye) = 0;

    // Finishes the frame begun by beginFrame; `rendered` if the eye images were rendered.
    virtual void endFrame(bool rendered) = 0;
};

[[nodiscard]] std::unique_ptr<Backend> makeMockBackend();
[[nodiscard]] std::unique_ptr<Backend> makeOpenXrBackend(); // null if not built in

// A standing player holding both hands in front of the chest (what the mock backend reports).
[[nodiscard]] TrackingState standingPose();

} // namespace qvr
