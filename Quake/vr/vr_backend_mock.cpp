// vr_backend_mock.cpp -- desktop stand-in for an XR runtime.
//
// Reports a standing head and two hands held in front of the body, and renders the eyes into
// its own textures (the left eye is mirrored to the window), so the VR code paths -- including
// stereo rendering -- can run and be tested without a headset.

#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"

#include <algorithm>
#include <cmath>

namespace qvr
{
namespace
{

// The pretend headset's recommended eye size (its images'), and its largest.
constexpr int imageWidth = 1024;
constexpr int imageHeight = 1024;
constexpr int maxImageSize = 2048;
constexpr float halfIpd = 0.032f;

// Controller input set from the console, for testing without a headset.
InputState mockInput;

[[nodiscard]] int mockHand(const char* name)
{
    if(!q_strcasecmp(name, "main"))
    {
        return HAND_MAIN;
    }
    if(!q_strcasecmp(name, "off"))
    {
        return HAND_OFF;
    }
    return -1;
}

// vr_mock_button <main|off> <trigger|grip|primary|secondary|stickclick|menu> <0|1>
void mockButton_f()
{
    const int hand = Cmd_Argc() == 4 ? mockHand(Cmd_Argv(1)) : -1;
    if(hand < 0)
    {
        Con_Printf("usage: vr_mock_button <main|off> <trigger|grip|primary|secondary|stickclick|menu> <0|1>\n");
        return;
    }

    struct Control
    {
        const char* name;
        bool HandInput::*button;
    };
    constexpr Control controls[] = {{"trigger", &HandInput::trigger}, {"grip", &HandInput::grip},
        {"primary", &HandInput::primary}, {"secondary", &HandInput::secondary},
        {"stickclick", &HandInput::stickClick}, {"menu", &HandInput::menu}};

    for(const Control& c : controls)
    {
        if(!q_strcasecmp(Cmd_Argv(2), c.name))
        {
            HandInput& in = mockInput.hands[hand];
            in.*c.button = Q_atoi(Cmd_Argv(3)) != 0;

            // Like a real controller: the fingers follow the trigger and grip, the thumb rests
            // on the face buttons.
            in.triggerValue = in.trigger ? 1.f : 0.f;
            in.gripValue = in.grip ? 1.f : 0.f;
            in.thumbTouch = in.primary || in.secondary || in.stickClick;
            return;
        }
    }
    Con_Printf("vr_mock_button: unknown control \"%s\"\n", Cmd_Argv(2));
}

// vr_mock_hand <main|off|head> <x> <y> <z> [<pitch> <yaw> <roll>]: tracking-space position
// (metres, +x right, +y up, -z forward) and, for a hand, its orientation (degrees: pitch up,
// yaw left, roll right side up); "vr_mock_hand <main|off|head>" alone restores
// the standing pose's.
constexpr int mockHead = HAND_COUNT;
glm::vec3 mockHandPos[HAND_COUNT + 1];
bool mockHandSet[HAND_COUNT + 1]{};
glm::quat mockHandRot[HAND_COUNT];
bool mockHandRotSet[HAND_COUNT]{};

void mockHand_f()
{
    int hand = Cmd_Argc() >= 2 ? mockHand(Cmd_Argv(1)) : -1;
    if(Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "head"))
    {
        hand = mockHead;
    }
    if(hand < 0 || (Cmd_Argc() != 2 && Cmd_Argc() != 5 && Cmd_Argc() != 8))
    {
        Con_Printf("usage: vr_mock_hand <main|off|head> [<x> <y> <z> [<pitch> <yaw> <roll>]]\n");
        return;
    }
    mockHandSet[hand] = Cmd_Argc() >= 5;
    mockHandPos[hand] = {Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4))};
    if(hand < HAND_COUNT)
    {
        mockHandRotSet[hand] = Cmd_Argc() == 8;
        mockHandRot[hand] = glm::angleAxis(glm::radians(Q_atof(Cmd_Argv(6))), glm::vec3{0.f, 1.f, 0.f}) *
                            glm::angleAxis(glm::radians(Q_atof(Cmd_Argv(5))), glm::vec3{1.f, 0.f, 0.f}) *
                            glm::angleAxis(glm::radians(-Q_atof(Cmd_Argv(7))), glm::vec3{0.f, 0.f, -1.f});
    }
}

// vr_mock_look <pitch> <yaw>: the head's orientation in degrees (pitch down positive).
glm::quat mockHeadOrientation{1.f, 0.f, 0.f, 0.f};

void mockLook_f()
{
    if(Cmd_Argc() != 3)
    {
        Con_Printf("usage: vr_mock_look <pitch> <yaw>\n");
        return;
    }
    const float pitch = glm::radians(Q_atof(Cmd_Argv(1)));
    const float yaw = glm::radians(Q_atof(Cmd_Argv(2)));
    mockHeadOrientation =
        glm::angleAxis(yaw, glm::vec3{0.f, 1.f, 0.f}) * glm::angleAxis(-pitch, glm::vec3{1.f, 0.f, 0.f});
}

// vr_mock_stick <main|off> <x> <y>
void mockStick_f()
{
    const int hand = Cmd_Argc() == 4 ? mockHand(Cmd_Argv(1)) : -1;
    if(hand < 0)
    {
        Con_Printf("usage: vr_mock_stick <main|off> <x> <y>\n");
        return;
    }
    mockInput.hands[hand].stick = {Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3))};
}

// vr_mock_swing: the main hand swings on a 60cm arm around the shoulder, from behind the head
// to in front of the chest and back, with exact velocities (as a runtime reports them).
void swing(Pose& hand, double time, float period)
{
    const glm::vec3 shoulder{0.2f, 1.45f, 0.f};
    constexpr float radius = 0.6f;
    constexpr float centre = 0.6f; // radians forward from straight up
    constexpr float amplitude = 0.9f;

    const float w = 2.f * 3.14159265f / period;
    const float phase = static_cast<float>(std::fmod(time, static_cast<double>(period))) * w;
    const float theta = centre + amplitude * std::sin(phase);
    const float thetaRate = amplitude * w * std::cos(phase);

    // theta 0 is straight up; increasing theta brings the hand forward (-z) and down.
    hand.position = shoulder + radius * glm::vec3{0.f, std::cos(theta), -std::sin(theta)};
    hand.orientation = glm::angleAxis(-(theta - 1.2f), glm::vec3{1.f, 0.f, 0.f});
    hand.linearVelocity = radius * thetaRate * glm::vec3{0.f, -std::sin(theta), -std::cos(theta)};
    hand.angularVelocity = glm::vec3{-thetaRate, 0.f, 0.f};
    hand.velocityValid = true;
}

// The velocity of a pose moved from the console: a scripted move is a jump in one frame, so the
// velocity is measured between moves and held for 40 ms (the engine may run several frames per
// server frame): strokes, throws and swings can be scripted with a move every frame or few.
struct ScriptedMotion
{
    glm::vec3 lastPos{0.f};
    glm::vec3 velocity{0.f};
    double lastMove = 0.0;

    [[nodiscard]] glm::vec3 update(const glm::vec3& pos, double now)
    {
        if(pos != lastPos)
        {
            velocity = lastMove > 0.0 ? (pos - lastPos) / static_cast<float>(std::clamp(now - lastMove, 0.004, 0.05))
                                      : glm::vec3{0.f};
            lastPos = pos;
            lastMove = now;
        }
        else if(now - lastMove > 0.04)
        {
            velocity = glm::vec3{0.f};
        }
        return velocity;
    }
};

class MockBackend final : public Backend
{
public:
    [[nodiscard]] const char* name() const override
    {
        return "mock";
    }

    [[nodiscard]] bool start() override
    {
        ensureTextures();
        return true;
    }

    void stop() override
    {
        for(gfx::Texture& tex : textures)
        {
            gfx::destroyTexture(tex);
            tex = 0;
        }
        width_ = height_ = 0;
    }

    [[nodiscard]] bool beginFrame(TrackingState& tracking, FrameState& frame) override
    {
        tracking = standingPose();
        tracking.input = mockInput;
        tracking.time = realtime;
        for(int h = 0; h < HAND_COUNT; h++)
        {
            if(mockHandSet[h])
            {
                tracking.hands[h].position = mockHandPos[h];
            }
            if(mockHandRotSet[h])
            {
                tracking.hands[h].orientation = mockHandRot[h];
            }
        }
        if(mockHandSet[mockHead])
        {
            tracking.head.position = mockHandPos[mockHead];
        }
        tracking.head.orientation = mockHeadOrientation;
        if(vr_mock_swing.value > 0.f)
        {
            swing(tracking.hands[HAND_MAIN], realtime, vr_mock_swing.value);
        }

        // Hands moved by vr_mock_hand report the velocity of the motion, as a runtime would.
        for(int h = 0; h < HAND_COUNT; h++)
        {
            Pose& hand = tracking.hands[h];
            if(h == HAND_MAIN && vr_mock_swing.value > 0.f)
            {
                continue; // exact velocities
            }
            hand.linearVelocity = handMotion[h].update(hand.position, realtime);
            hand.angularVelocity = glm::vec3{0.f};
            hand.velocityValid = true;
        }

        // The head likewise (a lunge scripted with vr_mock_hand head).
        tracking.head.linearVelocity = headMotion.update(tracking.head.position, realtime);
        tracking.head.velocityValid = true;

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
        width = width_;
        height = height_;
    }

    [[nodiscard]] EyeSizes eyeSizes() const override
    {
        EyeSizes s;
        s.recommendedWidth = imageWidth;
        s.recommendedHeight = imageHeight;
        s.maxWidth = s.maxHeight = maxImageSize;
        s.width = width_;
        s.height = height_;
        return s;
    }

    // vr_mock_hidden_area 1: the corners outside a circle a little wider than the image (about
    // 17% of it, as a headset's lenses hide), to test vr_visibility_mask.
    [[nodiscard]] const HiddenArea* hiddenArea(int /* eye */) const override
    {
        if(vr_mock_hidden_area.value == 0.f)
        {
            return nullptr;
        }
        if(hidden_.vertices.empty())
        {
            makeHiddenArea();
        }
        return &hidden_;
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
    gfx::Texture textures[2]{};
    int width_{0};
    int height_{0};
    mutable HiddenArea hidden_;

    // The eye images, at the recommended size like a runtime's (vr_render_scale does not change
    // them: the eyes are resampled into them).
    void ensureTextures()
    {
        if(textures[0])
        {
            return;
        }
        for(gfx::Texture& tex : textures)
        {
            tex = gfx::createTexture(imageWidth, imageHeight);
        }
        width_ = imageWidth;
        height_ = imageHeight;
    }

    // Between a circle of radius 1.04 (the image's half-width 1) and the image's edge, in quads
    // from the circle out to the edge along rays from the middle (the corners among them), scaled
    // to the mock's field of view (Fov{}: 0.8 radians each way).
    void makeHiddenArea() const
    {
        constexpr int segments = 64; // a multiple of 8: the corners' rays are among them
        constexpr float radius = 1.04f;
        const float tangent = std::tan(Fov{}.right);
        for(int i = 0; i < segments; i++)
        {
            const float a = 2.f * 3.14159265f * static_cast<float>(i) / segments;
            const glm::vec2 dir{std::cos(a), std::sin(a)};
            const float toEdge = 1.f / std::max(std::fabs(dir.x), std::fabs(dir.y));
            hidden_.vertices.push_back(dir * std::min(radius, toEdge) * tangent); // inner
            hidden_.vertices.push_back(dir * toEdge * tangent);                      // outer
        }
        for(int i = 0; i < segments; i++)
        {
            const std::uint32_t in0 = 2 * i, out0 = 2 * i + 1;
            const std::uint32_t in1 = 2 * ((i + 1) % segments), out1 = in1 + 1;
            for(std::uint32_t v : {in0, out0, out1, in0, out1, in1})
            {
                hidden_.indices.push_back(v);
            }
        }
    }
    ScriptedMotion handMotion[HAND_COUNT];
    ScriptedMotion headMotion; // headbutts
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

void registerMockCommands()
{
    Cmd_AddCommand("vr_mock_button", mockButton_f);
    Cmd_AddCommand("vr_mock_stick", mockStick_f);
    Cmd_AddCommand("vr_mock_hand", mockHand_f);
    Cmd_AddCommand("vr_mock_look", mockLook_f);
}

std::unique_ptr<Backend> makeMockBackend()
{
    return std::make_unique<MockBackend>();
}

} // namespace qvr
