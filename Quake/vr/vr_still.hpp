// vr_still.hpp -- holding still for a capture: the countdown with its beeps, and a window of the last moments' samples
// that says when they have all stayed within their tolerances round their average (and gives that average). Shared by
// the guided captures: Align Sights to My Aim (vr_sightalign.cpp) and the body calibration (vr_bodycal.cpp).

#pragma once

#include <glm/glm.hpp>

#include <vector>

namespace qvr::still
{

// Three beeps a second apart (misc/menu1.wav), then a higher one (misc/menu2.wav): go.
class Countdown
{
public:
    void start(double now, double seconds = 3.0);
    // Plays the beeps due by `now`; true once the go beep has played (then stays true until the next start).
    bool update(double now);
    // Whole seconds left (1 .. seconds; 0 once over).
    [[nodiscard]] int remaining(double now) const;
    [[nodiscard]] bool running() const { return active; }

private:
    double started{0.0};
    double length{3.0};
    int beeps{0};
    bool active{false};
    bool done{false};
};

// The samples of the last moments, each a fixed set of channels: points (any units: a tolerance each, round the
// average), directions (a tolerance in degrees round the normalised average), or values only averaged (no tolerance).
class Window
{
public:
    enum class Kind
    {
        Point,
        Direction,
        Averaged, // a point averaged, never checked
        AveragedDirection,
    };
    struct Channel
    {
        Kind kind{Kind::Point};
        float tolerance{0.f}; // units, or degrees for a direction
    };

    explicit Window(std::vector<Channel> channels);

    void clear();
    // Changes channel `c`'s tolerance (units that change with the world's scale).
    void tolerance(int c, float t) { spec[static_cast<size_t>(c)].tolerance = t; }
    [[nodiscard]] bool empty() const { return times.empty(); }
    [[nodiscard]] double lastTime() const { return times.empty() ? -1.0 : times.back(); }
    // A sample (one value per channel, in order). A second one at the same time (the other eye's pass) is left out;
    // those older than `keep` seconds are dropped.
    void add(double time, const glm::vec3* values, double keep);

    // Whether the samples of the last `seconds` (at least `minSamples` of them, reaching back that far) all stayed
    // within their channels' tolerances: then `mean` holds each channel's average (directions normalised).
    [[nodiscard]] bool still(double now, double seconds, std::vector<glm::vec3>& mean, int minSamples = 3);
    // The largest deviation of each channel found by the last still() (units or degrees), to show or debug.
    [[nodiscard]] const std::vector<float>& deviations() const { return devs; }
    // The last sample's value of channel `c` (none: zero).
    [[nodiscard]] glm::vec3 last(int c) const;

private:
    std::vector<Channel> spec;
    std::vector<double> times;
    std::vector<glm::vec3> values; // times.size() * spec.size()
    std::vector<float> devs;
};

[[nodiscard]] float degreesBetween(const glm::vec3& a, const glm::vec3& b);

} // namespace qvr::still
