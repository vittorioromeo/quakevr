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
#include <cstdio>
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

bool setMockButton(int hand, const char* control, bool on);

// vr_mock_button <main|off> <trigger|grip|primary|secondary|stickclick|menu> <0|1>
void mockButton_f()
{
    const int hand = Cmd_Argc() == 4 ? mockHand(Cmd_Argv(1)) : -1;
    if(hand < 0)
    {
        Con_Printf("usage: vr_mock_button <main|off> <trigger|grip|primary|secondary|stickclick|menu> <0|1>\n");
        return;
    }
    if(!setMockButton(hand, Cmd_Argv(2), Q_atoi(Cmd_Argv(3)) != 0))
    {
        Con_Printf("vr_mock_button: unknown control \"%s\"\n", Cmd_Argv(2));
    }
}

bool setMockButton(int hand, const char* control, bool on)
{
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
        if(!q_strcasecmp(control, c.name))
        {
            HandInput& in = mockInput.hands[hand];
            in.*c.button = on;

            // Like a real controller: the fingers follow the trigger and grip, the thumb rests
            // on the face buttons.
            in.triggerValue = in.trigger ? 1.f : 0.f;
            in.gripValue = in.grip ? 1.f : 0.f;
            in.thumbTouch = in.primary || in.secondary || in.stickClick;
            return true;
        }
    }
    return false;
}

// vr_mock_hand <main|off|head> <x> <y> <z> [<pitch> <yaw> <roll>]: tracking-space position
// (metres, +x right, +y up, -z forward) and, for a hand, its orientation (degrees: pitch up,
// yaw left, roll right side up); "vr_mock_hand <main|off|head>" alone restores
// the standing pose's.
[[nodiscard]] glm::quat mockRotation(float pitch, float yaw, float roll)
{
    return glm::angleAxis(glm::radians(yaw), glm::vec3{0.f, 1.f, 0.f}) *
           glm::angleAxis(glm::radians(pitch), glm::vec3{1.f, 0.f, 0.f}) *
           glm::angleAxis(glm::radians(-roll), glm::vec3{0.f, 0.f, -1.f});
}

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
        mockHandRot[hand] = mockRotation(Q_atof(Cmd_Argv(5)), Q_atof(Cmd_Argv(6)), Q_atof(Cmd_Argv(7)));
    }
}

// vr_mock_play <file>: plays a scripted motion on the clock, so that it runs the same at any frame
// rate (vr_mock_hand moves once per command: scripts paced by "wait" run at the server's rate). The
// file has one keyframe per line, times in seconds from the start:
//   <t> <main|off|head> <x> <y> <z> [<pitch> <yaw> <roll>]   (as vr_mock_hand)
//   <t> button <main|off> <control> <0|1>                     (as vr_mock_button)
//   <t> cmd <console command>                                (e.g. +grabright, -grabright)
// Poses in between are interpolated (positions linearly, orientations by slerp), and the hands
// report the motion's velocities between their keyframes, as a runtime would. At the end the last
// poses stay, as vr_mock_hand leaves them. "vr_mock_play" alone stops it.
struct PlayKey
{
    double t;
    glm::vec3 pos;
    bool hasRot;
    glm::quat rot;
};
struct PlayButton
{
    double t;
    int hand; // -1: `control` is a console command
    char control[64];
    bool on;
};
std::vector<PlayKey> playKeys[HAND_COUNT + 1];
std::vector<PlayButton> playButtons;
std::size_t playNextButton = 0;
double playStart = -1.0;
double playEnd = 0.0;

void mockPlay_f()
{
    for(auto& keys : playKeys)
    {
        keys.clear();
    }
    playButtons.clear();
    playNextButton = 0;
    playStart = -1.0;
    playEnd = 0.0;
    if(Cmd_Argc() != 2)
    {
        return;
    }

    FILE* file = fopen(Cmd_Argv(1), "r");
    if(!file)
    {
        Con_Printf("vr_mock_play: can't open \"%s\"\n", Cmd_Argv(1));
        return;
    }
    char line[256];
    while(fgets(line, sizeof(line), file))
    {
        double t;
        char what[16], a[16];
        float x, y, z, pitch, yaw, roll;
        int on;
        char command[64];
        if(sscanf(line, "%lf cmd %63[^\r\n]", &t, command) == 2)
        {
            PlayButton b{t, -1, {}, true};
            q_strlcpy(b.control, command, sizeof(b.control));
            playButtons.push_back(b);
            playEnd = std::max(playEnd, t);
            continue;
        }
        if(sscanf(line, "%lf button %15s %15s %d", &t, what, a, &on) == 4)
        {
            PlayButton b{t, mockHand(what), {}, on != 0};
            q_strlcpy(b.control, a, sizeof(b.control));
            if(b.hand >= 0)
            {
                playButtons.push_back(b);
                playEnd = std::max(playEnd, t);
            }
            continue;
        }
        const int n = sscanf(line, "%lf %15s %f %f %f %f %f %f", &t, what, &x, &y, &z, &pitch, &yaw, &roll);
        if(n != 5 && n != 8)
        {
            continue;
        }
        const int target = !q_strcasecmp(what, "head") ? mockHead : mockHand(what);
        if(target < 0)
        {
            continue;
        }
        playKeys[target].push_back({t, {x, y, z}, n == 8 && target < HAND_COUNT,
            n == 8 ? mockRotation(pitch, yaw, roll) : glm::quat{1.f, 0.f, 0.f, 0.f}});
        playEnd = std::max(playEnd, t);
    }
    fclose(file);
    std::stable_sort(playButtons.begin(), playButtons.end(),
        [](const PlayButton& l, const PlayButton& r) { return l.t < r.t; });
    for(auto& keys : playKeys)
    {
        std::stable_sort(keys.begin(), keys.end(), [](const PlayKey& l, const PlayKey& r) { return l.t < r.t; });
    }
    playStart = realtime;
}

// The played motion at realtime `now`: poses into mockHandPos/mockHandRot, and each played hand's
// velocities (tracking space) into `vel`/`angVel` with `played` set.
void playFrame(double now, glm::vec3* vel, glm::vec3* angVel, bool* played)
{
    if(playStart < 0.0)
    {
        return;
    }
    const double t = now - playStart;
    for(int target = 0; target <= HAND_COUNT; target++)
    {
        const std::vector<PlayKey>& keys = playKeys[target];
        if(keys.empty())
        {
            continue;
        }
        std::size_t i = 0;
        while(i + 1 < keys.size() && keys[i + 1].t <= t)
        {
            i++;
        }
        const PlayKey& k0 = keys[i];
        const PlayKey& k1 = i + 1 < keys.size() ? keys[i + 1] : keys[i];
        const double span = k1.t - k0.t;
        const float s = span > 0.0 ? static_cast<float>(std::clamp((t - k0.t) / span, 0.0, 1.0)) : 1.f;
        mockHandPos[target] = glm::mix(k0.pos, k1.pos, s);
        mockHandSet[target] = true;
        if(target == mockHead)
        {
            continue;
        }
        if(k0.hasRot && k1.hasRot)
        {
            mockHandRot[target] = glm::slerp(k0.rot, k1.rot, s);
            mockHandRotSet[target] = true;
        }
        played[target] = true;
        vel[target] = glm::vec3{0.f};
        angVel[target] = glm::vec3{0.f};
        if(span > 0.0 && t >= k0.t && t <= k1.t)
        {
            vel[target] = (k1.pos - k0.pos) / static_cast<float>(span);
            if(k0.hasRot && k1.hasRot)
            {
                glm::quat d = k1.rot * glm::inverse(k0.rot);
                if(d.w < 0.f)
                {
                    d = -d;
                }
                angVel[target] = glm::axis(d) * (glm::angle(d) / static_cast<float>(span));
            }
        }
    }
    while(playNextButton < playButtons.size() && playButtons[playNextButton].t <= t)
    {
        const PlayButton& b = playButtons[playNextButton++];
        if(b.hand < 0)
        {
            Cbuf_InsertText(va("%s\n", b.control)); // ahead of a script waiting in the buffer
        }
        else
        {
            setMockButton(b.hand, b.control, b.on);
        }
    }
    if(t > playEnd)
    {
        playStart = -1.0;
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
        glm::vec3 playVel[HAND_COUNT + 1], playAngVel[HAND_COUNT + 1];
        bool played[HAND_COUNT + 1]{};
        playFrame(realtime, playVel, playAngVel, played);

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
            if(played[h])
            {
                hand.linearVelocity = playVel[h]; // vr_mock_play: the motion's own
                hand.angularVelocity = playAngVel[h];
            }
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
    Cmd_AddCommand("vr_mock_play", mockPlay_f);
}

std::unique_ptr<Backend> makeMockBackend()
{
    return std::make_unique<MockBackend>();
}

} // namespace qvr
