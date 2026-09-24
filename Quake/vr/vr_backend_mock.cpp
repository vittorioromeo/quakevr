// vr_backend_mock.cpp -- desktop stand-in for an XR runtime.
//
// Reports a standing head and two hands held in front of the body, and renders the eyes into
// its own textures (the left eye is mirrored to the window), so the VR code paths -- including
// stereo rendering -- can run and be tested without a headset.

#include "vr_backend.hpp"
#include "vr_engine.hpp"

namespace qvr
{
namespace
{

constexpr int eyeWidth = 1024;
constexpr int eyeHeight = 1024;
constexpr float halfIpd = 0.032f;

class MockBackend final : public Backend
{
public:
    [[nodiscard]] const char* name() const override
    {
        return "mock";
    }

    [[nodiscard]] bool start() override
    {
        glGenTextures(2, textures);
        for(GLuint tex : textures)
        {
            glBindTexture(GL_TEXTURE_2D, tex);
            GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_RGBA8, eyeWidth, eyeHeight);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        GL_ClearBindings();
        return true;
    }

    void stop() override
    {
        glDeleteTextures(2, textures);
        GL_ClearBindings();
    }

    [[nodiscard]] bool beginFrame(TrackingState& tracking, FrameState& frame) override
    {
        tracking = standingPose();

        frame.shouldRender = true;
        for(int eye = 0; eye < 2; eye++)
        {
            frame.eyes[eye].pose = tracking.head;
            frame.eyes[eye].pose.position.x += eye == 0 ? -halfIpd : halfIpd;
            frame.eyes[eye].fov = Fov{};
        }

        return true;
    }

    void eyeResolution(int& width, int& height) const override
    {
        width = eyeWidth;
        height = eyeHeight;
    }

    [[nodiscard]] unsigned acquireEyeImage(int eye) override
    {
        return textures[eye];
    }

    void releaseEyeImage(int /* eye */) override
    {
    }

    void endFrame(bool /* rendered */) override
    {
    }

private:
    GLuint textures[2]{};
};

} // namespace

TrackingState standingPose()
{
    constexpr float eyeHeight = 1.7f;

    TrackingState out;
    out.head.position = {0.f, eyeHeight, 0.f};
    out.head.valid = true;

    // Hands at chest height, 40cm forward, 20cm to each side.
    out.hands[HAND_OFF].position = {-0.2f, eyeHeight - 0.4f, -0.4f};
    out.hands[HAND_MAIN].position = {0.2f, eyeHeight - 0.4f, -0.4f};
    for(Pose& hand : out.hands)
    {
        hand.valid = true;
    }

    return out;
}

std::unique_ptr<Backend> makeMockBackend()
{
    return std::make_unique<MockBackend>();
}

} // namespace qvr
