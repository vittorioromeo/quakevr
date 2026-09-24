// vr_throw.cpp -- see vr_throw.hpp, and docs/vr-port/THROWING.md for the research behind it.
//
// Why not the hand's current velocity: a throw is released by opening the hand, which lags the
// moment of release by tens of milliseconds, and the hand is already slowing down by then. The
// old engine averaged the last 15 frames, which lagged even more (and depended on the frame
// rate): throws came out weak and aimed where the hand was going several frames earlier.
//
// vr_throw_algorithm 3 (Half-Life: Alyx's approach): the release velocity is the controller's
// own velocity where it was fastest in a window around the moment of release, averaged over a
// few samples around that peak to reject noise. It is computed once, when the hand lets go, from
// samples timed on the runtime's clock, so neither the network rate nor the time the release
// takes to arrive moves the window. The held object's centre sits vr_throw_lever_arm metres along
// the hand: a clear wrist flick (above vr_throw_ang_threshold) adds some of its spin's velocity
// there. The release itself (filterGrips) comes from the analog grip easing off during a throw,
// earlier than the runtime's grip button.
//
// Algorithms 0..2 (the older ones: averages, and the peak of the newest samples) stay for comparison.

#include "vr_throw.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace qvr::throwing
{
namespace
{

struct Sample
{
    double time{0.0};
    glm::vec3 pos{0.f};    // world units
    glm::vec3 vel{0.f};    // controller point
    glm::vec3 objVel{0.f}; // held object's centre, with the whole lever term (algorithms 1, 2)
    glm::vec3 angVel{0.f};
    glm::vec3 forward{1.f, 0.f, 0.f};
};

// A third of a second at 144 Hz.
constexpr int capacity = 128;

struct History
{
    std::array<Sample, capacity> samples;
    int count{0};
    int next{0};

    // i = 0 is the newest sample.
    [[nodiscard]] const Sample& at(int i) const
    {
        return samples[(next - 1 - i + capacity * 2) % capacity];
    }
};

History histories[2];

[[nodiscard]] float metersToUnits()
{
    return vr_world_scale.value / 0.0381f;
}

[[nodiscard]] Estimate fromSample(const Sample& s, const glm::vec3& vel, const glm::vec3& angVel)
{
    Estimate out;
    out.vel = vel;
    out.angVel = angVel;
    out.pos = s.pos;
    out.time = s.time;
    return out;
}

[[nodiscard]] Estimate average(const History& h, int frames, bool lever)
{
    glm::vec3 vel{0.f}, angVel{0.f};
    const int n = std::min(std::max(frames, 1), h.count);
    for(int i = 0; i < n; i++)
    {
        vel += lever ? h.at(i).objVel : h.at(i).vel;
        angVel += h.at(i).angVel;
    }
    return fromSample(h.at(0), vel / static_cast<float>(n), angVel / static_cast<float>(n));
}

// Algorithm 2: the peak of the object's speed in the newest `window` seconds.
[[nodiscard]] Estimate newestPeak(const History& h, double window, double span)
{
    const double now = h.at(0).time;

    int best = 0;
    float bestSpeed = -1.f;
    for(int i = 0; i < h.count && now - h.at(i).time <= window; i++)
    {
        const float speed = glm::length(h.at(i).objVel);
        if(speed > bestSpeed)
        {
            bestSpeed = speed;
            best = i;
        }
    }

    glm::vec3 vel{0.f}, angVel{0.f};
    int n = 0;
    const double peakTime = h.at(best).time;
    for(int i = 0; i < h.count; i++)
    {
        if(std::abs(h.at(i).time - peakTime) <= span)
        {
            vel += h.at(i).objVel;
            angVel += h.at(i).angVel;
            n++;
        }
    }
    return fromSample(h.at(best), vel / static_cast<float>(n), angVel / static_cast<float>(n)); // n >= 1
}

// Algorithm 3: the peak of the controller's speed around the release.
[[nodiscard]] Estimate releasePeak(const History& h, double releaseTime)
{
    const double from = releaseTime - std::max(vr_throw_window.value, 0.f);
    const double to = releaseTime + std::max(vr_throw_lookahead.value, 0.f);
    const double span = std::max(vr_throw_peak_span.value, 0.f);

    int best = -1;
    float bestSpeed = -1.f;
    for(int i = 0; i < h.count; i++)
    {
        const Sample& s = h.at(i);
        if(s.time < from || s.time > to)
        {
            continue;
        }
        if(const float speed = glm::length(s.vel); speed > bestSpeed)
        {
            bestSpeed = speed;
            best = i;
        }
    }
    if(best < 0)
    {
        best = 0; // nothing in the window (a very late release): the newest sample
    }

    // The velocity over the samples within the span of the peak, the (noisier) spin over twice it.
    const Sample& peak = h.at(best);
    glm::vec3 vel{0.f}, angVel{0.f};
    int nVel = 0, nAng = 0;
    for(int i = 0; i < h.count; i++)
    {
        const double dt = std::abs(h.at(i).time - peak.time);
        if(dt <= span)
        {
            vel += h.at(i).vel;
            nVel++;
        }
        if(dt <= span * 2.0)
        {
            angVel += h.at(i).angVel;
            nAng++;
        }
    }
    vel /= static_cast<float>(nVel);
    angVel /= static_cast<float>(nAng);

    // The object's centre, and the velocity a clear wrist flick adds there.
    const glm::vec3 lever = peak.forward * vr_throw_lever_arm.value; // metres
    if(glm::length(angVel) > vr_throw_ang_threshold.value)
    {
        vel += glm::cross(angVel, lever) * vr_throw_ang_factor.value;
    }

    Estimate out = fromSample(peak, vel, angVel);
    out.pos = peak.pos + lever * metersToUnits();
    return out;
}

// Grip state per hand, for the release detection.
struct Grip
{
    bool held{false};
    float peak{0.f};
    double released{-1.0};
};

Grip grips[2];

} // namespace

void sample(int hand, double time, const glm::vec3& pos, const glm::vec3& vel, const glm::vec3& angVel,
    const glm::vec3& forward)
{
    History& h = histories[hand];

    Sample s;
    s.time = time;
    s.pos = pos;
    s.vel = vel;
    s.angVel = angVel;
    s.forward = forward;
    s.objVel = vel + glm::cross(angVel, forward * vr_throw_lever_arm.value);

    if(h.count > 0 && h.at(0).time == time)
    {
        // Resampled within the same frame (the hands were recomputed): replace.
        h.samples[(h.next - 1 + capacity) % capacity] = s;
        return;
    }

    if(h.count > 0 && time < h.at(0).time)
    {
        h = History{}; // time went backwards (new map, demo, another runtime clock): start over
    }

    h.samples[h.next] = s;
    h.next = (h.next + 1) % capacity;
    h.count = std::min(h.count + 1, capacity);
}

Estimate estimate(int hand)
{
    const History& h = histories[hand];
    if(h.count == 0)
    {
        return {};
    }

    switch(static_cast<int>(vr_throw_algorithm.value))
    {
        case 0: return average(h, static_cast<int>(vr_throw_avg_frames.value), false);
        case 1: return average(h, static_cast<int>(vr_throw_avg_frames.value), true);
        case 2:
            return newestPeak(h, std::max(vr_throw_window.value, 0.f), std::max(vr_throw_peak_span.value, 0.f));
        default: return releasePeak(h, h.at(0).time);
    }
}

Estimate estimateAt(int hand, double releaseTime)
{
    const History& h = histories[hand];
    if(h.count == 0 || static_cast<int>(vr_throw_algorithm.value) < 3)
    {
        return estimate(hand);
    }

    return releasePeak(h, releaseTime);
}

double latestTime(int hand)
{
    const History& h = histories[hand];
    return h.count > 0 ? h.at(0).time : 0.0;
}

void filterGrips(TrackingState& t)
{
    for(int hand = 0; hand < HAND_COUNT; hand++)
    {
        HandInput& in = t.input.hands[hand];
        Grip& g = grips[hand];
        const double now = t.time >= 0.0 ? t.time : realtime;
        const bool wasHeld = g.held;

        // Controllers whose grip is only a button report no value while it is pressed.
        const bool analog = in.gripValue > 0.01f || !in.grip;
        if(!vr_throw_release.value || !analog)
        {
            g.held = in.grip;
        }
        else if(!g.held)
        {
            g.held = in.gripValue >= vr_throw_grab_press.value;
            g.peak = in.gripValue;
        }
        else
        {
            g.peak = std::max(g.peak, in.gripValue);

            const Pose& pose = t.hands[hand];
            const bool throwing = pose.velocityValid && glm::length(pose.linearVelocity) > vr_throw_release_speed.value;
            const bool eased = in.gripValue < g.peak * (1.f - CLAMP(0.f, vr_throw_release_drop.value, 1.f));
            if(in.gripValue < vr_throw_release_floor.value || (throwing && eased))
            {
                g.held = false;
            }
        }

        if(wasHeld && !g.held)
        {
            g.released = now;
        }
        in.grip = g.held;
    }
}

double releaseTime(int hand)
{
    return grips[hand].released;
}

void reset()
{
    for(History& h : histories)
    {
        h = History{};
    }
    for(Grip& g : grips)
    {
        g = Grip{};
    }
}

} // namespace qvr::throwing
