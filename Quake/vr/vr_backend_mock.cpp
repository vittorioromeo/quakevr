// vr_backend_mock.cpp -- desktop stand-in for an XR runtime.
//
// Reports a standing head and two hands held in front of the body, so the VR code paths can
// run (and be tested) without a headset. Later phases will let the mouse drive the hands.

#include "vr_backend.hpp"

namespace qvr
{
namespace
{

class MockBackend final : public Backend
{
public:
    [[nodiscard]] const char* name() const override
    {
        return "mock";
    }

    [[nodiscard]] bool start() override
    {
        return true;
    }

    void stop() override
    {
    }

    [[nodiscard]] bool update(TrackingState& out) override
    {
        out = standingPose();
        return true;
    }
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
