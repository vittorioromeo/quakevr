// vr_angvel.cpp -- see vr_angvel.hpp.
//
// The controller's frame (vr_angvel_frame 1) is the grip pose turned vr_angvel_local_pitch degrees about its x axis:
// fitted on the author's takes of 2026-09-29 (VirtualDesktopXR 1.0.10, Quest Touch; 10 000 samples above 6 rad/s,
// the runtime's against the pose's turn two samples earlier): median cosine 0.98 (0.50 taken as the tracking space).
//
// vr_angvel_frame -1 (auto) starts from the runtime's name (VirtualDesktopXR: the controller's frame) and then counts,
// on samples where the pose turns faster than 4 rad/s, which reading of the runtime's vector points along that turn
// (when they differ by over 0.3 in cosine): 60 net samples against the current reading (about a second of fast wrist
// turns) switch it, so a runtime that follows OpenXR (or a later VirtualDesktopXR that does) is used as it comes.

#include "vr_angvel.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"

#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Fabs.hpp"

#include <cstring>


namespace qvr::angvel
{
namespace
{

constexpr int voteDecide = 30; // the detection's lead either way at its start
constexpr int voteMax = 60;    // how much evidence it keeps
constexpr float evidenceSpin = 4.f;    // rad/s of the pose's turn
constexpr float evidenceRuntime = 2.f; // rad/s of the runtime's
constexpr float evidenceMargin = 0.3f; // cosine

struct HandPrev
{
    glm::quat rot{1.f, 0.f, 0.f, 0.f};
    double time{-1.0};
    glm::vec3 spin{0.f};
    bool spinValid{false};
};

HandPrev prev[HAND_COUNT];
int votes = -voteDecide; // > 0: the controller's frame
Frame detected = Frame::Tracking;
Frame inUse = Frame::Tracking;
char seededFor[256]{}; // the runtime the detection started from (a change starts it over)
bool seeded = false;

[[nodiscard]] bool reportsLocal(const char* runtime)
{
    return runtime && strstr(runtime, "VirtualDesktopXR") != nullptr;
}

void seed(const char* runtime)
{
    reset();
    q_strlcpy(seededFor, runtime ? runtime : "", sizeof(seededFor));
    seeded = true;
    const bool local = reportsLocal(runtime);
    votes = local ? voteDecide : -voteDecide;
    detected = local ? Frame::Controller : Frame::Tracking;
}

// The controller's spin (tracking space, rad/s) from its turn since its previous sample; none after a gap of over
// 0.1 s; the same frame's again when the time hasn't moved.
void sampleSpin(int h, const Pose& p, double time)
{
    HandPrev& s = prev[h];
    if(!p.valid)
    {
        s.time = -1.0;
        s.spinValid = false;
        return;
    }
    const double dt = time - s.time;
    if(s.time >= 0.0 && dt == 0.0)
    {
        return;
    }
    s.spin = glm::vec3{0.f};
    s.spinValid = false;
    if(s.time >= 0.0 && dt > 0.0 && dt < 0.1)
    {
        glm::quat d = p.orientation * glm::inverse(s.rot);
        if(d.w < 0.f)
        {
            d = -d;
        }
        const glm::vec3 v{d.x, d.y, d.z};
        if(const float len = glm::length(v); len > 1e-7f)
        {
            s.spin = v * (2.f * za::atan2(len, d.w) / (len * static_cast<float>(dt)));
        }
        s.spinValid = true;
    }
    s.rot = p.orientation;
    s.time = time;
}

void vote(const glm::vec3& spin, const glm::vec3& asTracking, const glm::vec3& asLocal)
{
    if(glm::length(spin) < evidenceSpin || glm::length(asTracking) < evidenceRuntime)
    {
        return;
    }
    const glm::vec3 n = glm::normalize(spin);
    const float c0 = glm::dot(n, glm::normalize(asTracking));
    const float c1 = glm::dot(n, glm::normalize(asLocal));
    if(za::fabs(c1 - c0) > evidenceMargin)
    {
        votes = glm::clamp(votes + (c1 > c0 ? 1 : -1), -voteMax, voteMax);
    }
}

[[nodiscard]] const char* frameName(Frame f)
{
    return f == Frame::Controller ? "the controller's frame" : f == Frame::Pose ? "the controller's turn" : "the tracking space";
}

} // namespace

glm::quat controllerFrame(const Pose& hand, const GripInRaw& gripInHand)
{
    return hand.orientation * gripInHand.turn *
           glm::angleAxis(glm::radians(vr_angvel_local_pitch.value), glm::vec3{1.f, 0.f, 0.f});
}

void reset()
{
    for(HandPrev& p : prev)
    {
        p = HandPrev{};
    }
    votes = detected == Frame::Controller ? voteDecide : -voteDecide;
}

Frame current()
{
    return inUse;
}

void fix(TrackingState& t, const char* runtime)
{
    if(!seeded || strcmp(seededFor, runtime ? runtime : "") != 0)
    {
        seed(runtime);
    }
    const double time = t.time >= 0.0 ? t.time : realtime;

    glm::vec3 local[HAND_COUNT]{glm::vec3{0.f}, glm::vec3{0.f}}; // the runtime's vector read in the controller's frame
    for(int h = 0; h < HAND_COUNT; h++)
    {
        const Pose& p = t.hands[h];
        sampleSpin(h, p, time);
        if(!p.valid || !p.velocityValid)
        {
            continue;
        }
        local[h] = controllerFrame(p, t.gripInHand[h]) * p.angularVelocity;
        if(prev[h].spinValid)
        {
            vote(prev[h].spin, p.angularVelocity, local[h]);
        }
    }
    const Frame was = detected;
    if(votes >= voteDecide)
    {
        detected = Frame::Controller;
    }
    else if(votes <= -voteDecide)
    {
        detected = Frame::Tracking;
    }
    const int mode = static_cast<int>(vr_angvel_frame.value);
    if(detected != was && mode < 0)
    {
        Con_Printf("VR: the controllers' angular velocity: %s (detected, %s)\n", frameName(detected), runtime ? runtime : "");
    }
    inUse = mode < 0 ? detected : mode == 1 ? Frame::Controller : mode >= 2 ? Frame::Pose : Frame::Tracking;
    if(inUse == Frame::Tracking && !vr_debug_angvel.value)
    {
        return; // as it came (vr_angvel_frame 0: as before)
    }

    for(int h = 0; h < HAND_COUNT; h++)
    {
        Pose& p = t.hands[h];
        if(!p.valid || !p.velocityValid)
        {
            continue;
        }
        const glm::vec3 w0 = p.angularVelocity, v0 = p.linearVelocity;
        if(inUse != Frame::Tracking)
        {
            p.angularVelocity = inUse == Frame::Controller ? local[h] : prev[h].spin;
        }
        if(inUse != Frame::Tracking && p.gripVelocityValid)
        {
            // The legacy pose's point swings about the grip with the turn (the backend's toLegacyPose, redone).
            p.linearVelocity = p.gripVelocity + glm::cross(p.angularVelocity, -(p.orientation * t.gripInHand[h].offset));
        }
        if(vr_debug_angvel.value && prev[h].spinValid && glm::length(prev[h].spin) > evidenceSpin)
        {
            const glm::vec3 n = glm::normalize(prev[h].spin);
            const auto cosTo = [&](const glm::vec3& w) { return glm::length(w) > 1e-4f ? glm::dot(n, glm::normalize(w)) : 0.f; };
            Con_Printf("angvel: %s hand, %s (evidence %+d): turn %.1f rad/s, runtime's %.1f (cos %.2f), fixed %.1f (cos %.2f); "
                       "speed %.2f -> %.2f m/s\n",
                h == HAND_MAIN ? "main" : "off", frameName(inUse), votes, glm::length(prev[h].spin), glm::length(w0), cosTo(w0),
                glm::length(p.angularVelocity), cosTo(p.angularVelocity), glm::length(v0), glm::length(p.linearVelocity));
        }
    }
}

} // namespace qvr::angvel
