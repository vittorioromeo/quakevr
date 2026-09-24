// vr_throw.cpp -- see vr_throw.hpp.
//
// Why not the hand's current velocity: a throw is released by opening the hand, which lags the
// moment of release by tens of milliseconds, and the hand is already slowing down by then. The
// old engine averaged the last 15 frames, which lagged even more (and depended on the frame
// rate): throws came out weak and aimed where the hand was going several frames earlier.
//
// Instead (vr_throw_algorithm 2) the release velocity is taken where the hand was fastest in
// the last vr_throw_window seconds, averaged over a few frames around that peak to reject
// noise. The held object is not at the grip: its centre is vr_throw_lever_arm metres along the
// hand, so a wrist flick adds angularVelocity x leverArm to it.

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
    glm::vec3 vel{0.f};    // grip
    glm::vec3 objVel{0.f}; // held object's centre
    glm::vec3 angVel{0.f};
};

// Enough for vr_throw_avg_frames (at most 50) or a quarter of a second at 144 Hz.
constexpr int capacity = 64;

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

[[nodiscard]] Estimate average(const History& h, int frames, bool lever)
{
    Estimate out;
    const int n = std::min(std::max(frames, 1), h.count);
    for(int i = 0; i < n; i++)
    {
        out.vel += lever ? h.at(i).objVel : h.at(i).vel;
        out.angVel += h.at(i).angVel;
    }
    out.vel /= static_cast<float>(n);
    out.angVel /= static_cast<float>(n);
    return out;
}

[[nodiscard]] Estimate peak(const History& h, double window, double span)
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

    Estimate out;
    int n = 0;
    const double peakTime = h.at(best).time;
    for(int i = 0; i < h.count; i++)
    {
        if(std::abs(h.at(i).time - peakTime) <= span)
        {
            out.vel += h.at(i).objVel;
            out.angVel += h.at(i).angVel;
            n++;
        }
    }
    out.vel /= static_cast<float>(n); // n >= 1: the peak itself
    out.angVel /= static_cast<float>(n);
    return out;
}

} // namespace

void sample(int hand, double time, const glm::vec3& vel, const glm::vec3& angVel, const glm::vec3& forward)
{
    History& h = histories[hand];

    Sample s;
    s.time = time;
    s.vel = vel;
    s.angVel = angVel;
    s.objVel = vel + glm::cross(angVel, forward * vr_throw_lever_arm.value);

    if(h.count > 0 && h.at(0).time == time)
    {
        // Resampled within the same frame (the hands were recomputed): replace.
        h.samples[(h.next - 1 + capacity) % capacity] = s;
        return;
    }

    if(h.count > 0 && time < h.at(0).time)
    {
        h = History{}; // time went backwards (new map, demo): start over
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
        default:
            return peak(h, std::max(vr_throw_window.value, 0.f), std::max(vr_throw_peak_span.value, 0.f));
    }
}

void reset()
{
    for(History& h : histories)
    {
        h = History{};
    }
}

} // namespace qvr::throwing
