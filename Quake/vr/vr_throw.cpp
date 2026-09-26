// vr_throw.cpp -- see vr_throw.hpp, and docs/vr-port/THROWING.md for the research behind it.
//
// Why not the hand's current velocity: a throw is released by opening the hand, which lags the
// moment of release by tens of milliseconds, and the hand is already slowing down by then. The
// old engine averaged the last 15 frames, which lagged even more (and depended on the frame
// rate): throws came out weak and aimed where the hand was going several frames earlier.
//
// Half-Life: Alyx's approach: the release velocity is the controller's
// own velocity where it was fastest in a window around the moment of release, averaged over a
// few samples around that peak to reject noise. It is computed once, when the hand lets go, from
// samples timed on the runtime's clock, so neither the network rate nor the time the release
// takes to arrive moves the window. The held object's centre sits vr_throw_lever_arm metres along
// the hand: a clear wrist flick (above vr_throw_ang_threshold) adds some of its spin's velocity
// there. The release itself (filterGrips) comes from the analog grip easing off during a throw,
// earlier than the runtime's grip button.

#include "vr_throw.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_units.hpp"

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

// The peak of a quadratic fitted (least squares) to the speeds within peakFit seconds of `peakTime`
// (the fastest sample, `bestSpeed`), kept within 20% of it; 0 when it doesn't fit (fewer than three
// samples, or not a peak).
constexpr double peakFit = 0.03;

[[nodiscard]] float peakSpeedFit(const History& h, double peakTime, float bestSpeed)
{
    // Sums for s = a + b x + c x^2, x in seconds from the peak sample.
    double sx[5]{}, sy[3]{};
    int n = 0;
    for(int i = 0; i < h.count; i++)
    {
        const double x = h.at(i).time - peakTime;
        if(std::abs(x) > peakFit)
        {
            continue;
        }
        const double y = glm::length(h.at(i).vel);
        double p = 1.0;
        for(int k = 0; k < 5; k++)
        {
            sx[k] += p;
            if(k < 3)
            {
                sy[k] += p * y;
            }
            p *= x;
        }
        n++;
    }
    if(n < 3)
    {
        return 0.f;
    }

    // The normal equations (3x3), by Cramer's rule.
    const double m[3][3] = {{sx[0], sx[1], sx[2]}, {sx[1], sx[2], sx[3]}, {sx[2], sx[3], sx[4]}};
    const auto det3 = [](const double a[3][3]) {
        return a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
               a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    };
    const double d = det3(m);
    if(std::abs(d) < 1e-18)
    {
        return 0.f;
    }
    double coef[3];
    for(int c = 0; c < 3; c++)
    {
        double mc[3][3];
        for(int r = 0; r < 3; r++)
        {
            for(int k = 0; k < 3; k++)
            {
                mc[r][k] = k == c ? sy[r] : m[r][k];
            }
        }
        coef[c] = det3(mc) / d;
    }
    const double a = coef[0], b = coef[1], c = coef[2];
    if(c >= 0.0)
    {
        return 0.f; // not a peak
    }
    const double x = std::clamp(-b / (2.0 * c), -peakFit, peakFit);
    const double top = a + b * x + c * x * x;
    return static_cast<float>(std::clamp(top, 0.8 * bestSpeed, 1.2 * bestSpeed));
}

// The peak of the controller's speed around the release.
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

    // The speed at the true peak, between the samples: a quadratic fitted to the speeds within
    // peakFit of the fastest sample. At a low frame rate the samples are far apart and the fastest
    // one can be well off the peak (throws came out up to 8% slower at 45 fps than at 72).
    if(const float fitted = peakSpeedFit(h, peak.time, bestSpeed); fitted > 0.f && glm::length(vel) > 1e-4f)
    {
        vel = glm::normalize(vel) * std::max(glm::length(vel), fitted);
    }

    // The direction from the samples leading up to the peak: at the peak itself an overarm throw
    // is already curving down, and throws went lower than meant.
    if(const float lookback = vr_throw_dir_lookback.value; lookback > 0.f)
    {
        glm::vec3 dir{0.f};
        for(int i = 0; i < h.count; i++)
        {
            const Sample& s = h.at(i);
            if(s.time <= peak.time && s.time >= peak.time - lookback)
            {
                dir += s.vel;
            }
        }
        if(glm::length(dir) > 1e-4f && glm::length(vel) > 1e-4f)
        {
            vel = glm::normalize(dir) * glm::length(vel);
        }
    }

    // The object's centre, and the velocity a clear wrist flick adds there.
    const glm::vec3 lever = peak.forward * vr_throw_lever_arm.value; // metres
    if(glm::length(angVel) > vr_throw_ang_threshold.value)
    {
        vel += glm::cross(angVel, lever) * vr_throw_ang_factor.value;
    }

    return {vel, angVel, peak.pos + lever * units::metresToUnits(), peak.time};
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
    const Sample s{time, pos, vel, angVel, forward};

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
    return estimateAt(hand, latestTime(hand));
}

Estimate estimateAt(int hand, double releaseTime)
{
    const History& h = histories[hand];
    if(h.count == 0)
    {
        return {};
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
