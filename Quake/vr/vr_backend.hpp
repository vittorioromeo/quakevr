// vr_backend.hpp -- abstraction over the XR runtime (OpenXR, or a desktop mock for testing).
//
// A backend reports tracking, paces frames and owns the images the eyes are rendered into.
// All game logic stays in the VR module, so that it can run without a headset (mock backend).

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace qvr
{

// Tracking-space pose. Units are metres, in the runtime's convention: +X right, +Y up,
// -Z forward, origin on the play-space floor.
struct Pose
{
    glm::vec3 position{0.f};
    glm::quat orientation{1.f, 0.f, 0.f, 0.f};
    bool valid{false};

    // Velocities reported by the runtime (IMU-fused, far better than differencing positions):
    // metres per second and radians per second, in tracking space.
    glm::vec3 linearVelocity{0.f};
    glm::vec3 angularVelocity{0.f};
    bool velocityValid{false};

    // Of the hand's grip (the palm) when the pose was moved off it (vr_controller_legacy_pose):
    // what a held object moves with. A wrist flick swings the controller's tip much faster.
    glm::vec3 gripVelocity{0.f};
    bool gripVelocityValid{false};
};

enum Hand : int
{
    HAND_OFF = 0,
    HAND_MAIN = 1,
    HAND_COUNT = 2
};

// One controller's physical controls. What they do is up to the key bindings (vr_input.cpp).
struct HandInput
{
    bool trigger{false};
    bool grip{false};
    bool primary{false};   // A / X
    bool secondary{false}; // B / Y
    bool stickClick{false};
    bool menu{false};
    glm::vec2 stick{0.f};  // x right, y forward

    // For the fingers (0 open .. 1 pressed): the index finger follows the trigger, the other
    // three the grip, the thumb whether it rests on a button, stick or thumb rest.
    float triggerValue{0.f};
    float gripValue{0.f};
    bool thumbTouch{false};
};

// Controller input, [0] off hand, [1] main hand.
struct InputState
{
    HandInput hands[HAND_COUNT];
};

struct TrackingState
{
    Pose head;
    Pose hands[HAND_COUNT]; // grip poses, [0] off hand, [1] main hand
    InputState input;
    double time{-1.0};      // seconds on the runtime's clock that the poses are for (< 0: unknown)
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

// The part of an eye's image the lenses never show (XR_KHR_visibility_mask's hidden triangle
// mesh): triangles in the eye's tangent space (x = tan of the angle right, y = tan up, the plane
// z = -1 of the eye's view), mapped to the image by the eye's Fov.
struct HiddenArea
{
    std::vector<glm::vec2> vertices;
    std::vector<std::uint32_t> indices; // three per triangle
};

// Eye sizes: the runtime's recommended and largest, and the eye images' (the recommended; they
// keep it for the whole session: SteamVR's OpenGL path does not follow a swapchain created at
// another size mid-session, it crops or drops the new images).
struct EyeSizes
{
    int recommendedWidth{0}, recommendedHeight{0};
    int maxWidth{0}, maxHeight{0};
    int width{0}, height{0};
};

// The size the eyes are rendered at for an eye image size (vr_render_scale times it, within a
// largest size): resampled into the image when it differs (vr_stereo.cpp).
[[nodiscard]] int scaledEyeSize(int image, int max);

class Backend
{
public:
    virtual ~Backend() = default;

    [[nodiscard]] virtual const char* name() const = 0;

    // The runtime behind it, with its version (OpenXR's runtimeName), or the backend's name.
    [[nodiscard]] virtual const char* runtimeName() const
    {
        return name();
    }

    // Creates the runtime session. Called with the engine's GL context current.
    [[nodiscard]] virtual bool start() = 0;
    virtual void stop() = 0;

    // Once per host frame: pumps runtime events, waits for the next frame and updates
    // tracking. Returns false if the session was lost.
    [[nodiscard]] virtual bool beginFrame(TrackingState& tracking, FrameState& frame) = 0;

    // Whether a frame begun by beginFrame is waiting to be finished (eyes may be rendered).
    [[nodiscard]] virtual bool frameActive() const
    {
        return true;
    }

    // Size of each eye's image (what acquireEyeImage returns).
    virtual void eyeResolution(int& width, int& height) const = 0;

    // The runtime's recommended and largest eye sizes, and the images'.
    [[nodiscard]] virtual EyeSizes eyeSizes() const
    {
        EyeSizes s;
        eyeResolution(s.width, s.height);
        s.recommendedWidth = s.maxWidth = s.width;
        s.recommendedHeight = s.maxHeight = s.height;
        return s;
    }

    // The eye's hidden area (the lenses' blind corners): null if the runtime has none to give
    // (no XR_KHR_visibility_mask); an empty mesh if it gave none for this eye.
    [[nodiscard]] virtual const HiddenArea* hiddenArea(int /* eye */) const
    {
        return nullptr;
    }

    // GL texture to render an eye into, between acquire and release.
    [[nodiscard]] virtual unsigned acquireEyeImage(int eye) = 0;
    virtual void releaseEyeImage(int eye) = 0;

    // Finishes the frame begun by beginFrame; `rendered` if the eye images were rendered.
    virtual void endFrame(bool rendered) = 0;

    // A flat panel the runtime shows in front of the player while the world is not rendered
    // (menus before a map, the console, loading): the GL texture to copy the 2D layer into, or
    // 0 without one. At most once per frame; shown with the frame endFrame finishes.
    [[nodiscard]] virtual unsigned acquirePanelImage(int /* width */, int /* height */)
    {
        return 0;
    }
    virtual void releasePanelImage()
    {
    }

    // Vibrates a controller.
    virtual void haptic(int /* hand */, float /* seconds */, float /* frequency */, float /* amplitude */)
    {
    }
};

[[nodiscard]] std::unique_ptr<Backend> makeMockBackend();
[[nodiscard]] std::unique_ptr<Backend> makeOpenXrBackend(); // null if not built in

// A standing player holding both hands in front of the chest (what the mock backend reports).
[[nodiscard]] TrackingState standingPose();

// Console commands driving the mock backend's tracking and controllers: vr_mock_button,
// vr_mock_stick, vr_mock_hand, vr_mock_look.
void registerMockCommands();

} // namespace qvr
