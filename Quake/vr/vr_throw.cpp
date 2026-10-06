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
// there. The samples are the controller's own (its grip's velocity, the lever along a fixed frame on
// it: hands::throwFrame), so the hand calibration doesn't change a throw; vr_throw_pitch tilts it.
// The release itself (filterGrips) comes from the analog grip easing off during a throw, earlier
// than the runtime's grip button.

#include "vr_throw.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_timescale.hpp"
#include "vr_units.hpp"

#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Log.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr_zancle.hpp"


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
    double spinTime{0.0}; // when `angVel` is for: `time` less the spin's lag (Motion::spinLag)
};

// Over a second at 240 fps (the windows take under 0.4 s of it at their sliders' most).
constexpr int capacity = 256;

struct History
{
    za::Array<Sample, capacity> samples;
    int count{0};
    int next{0};

    // i = 0 is the newest sample.
    [[nodiscard]] const Sample& at(int i) const
    {
        return samples[(next - 1 - i + capacity * 2) % capacity];
    }

    void clear()
    {
        count = 0;
        next = 0;
    }
};

History histories[2];
// The controllers' own (timescale::controllerPose), in step with `histories` (the same samples unless the slowed hand
// lags its controller in slow motion): a throw's direction (vr_throw_slowmo_aim).
History ownHistories[2];
History bothHistory;    // estimateBothAt's samples (scratch: off the stack)
History ownBothHistory; // the same of ownHistories

// The estimate takes the samples as a signal in time, not as so many samples: linear between them (a velocity or spin
// between two frames), each window a span of seconds weighed by time, the peak looked for between the frames. Before
// (until 2026-10-06) it took the fastest sample, averaged the samples within so many seconds of it and the direction
// over the samples before it: the same throw moved by up to 14 degrees between 72 and 240 fps (the fastest sample can be
// any of several on a flat top, a frame either side of the true peak; 3 samples in the direction's 40 ms at 72 fps, 10
// at 240), the wrist's lever was the peak sample's (a flick turns the hand 11 degrees in a frame at 72 fps), and samples
// after the release were in the window when the move carrying it was built late enough to have them (at 90 fps and over,
// whose server frames are fewer than the frames) and not at 72. ROUND21.md, "Throws at any frame rate".

// A track of a history: its samples oldest first (k = 0 the oldest), as the velocity's (at the samples' times) or the
// spin's (at their spins' times).
struct Track
{
    const History& h;
    bool spin;

    [[nodiscard]] int size() const
    {
        return h.count;
    }
    [[nodiscard]] const Sample& at(int k) const
    {
        return h.at(h.count - 1 - k);
    }
    [[nodiscard]] double time(int k) const
    {
        return spin ? at(k).spinTime : at(k).time;
    }
    [[nodiscard]] glm::dvec3 value(int k) const
    {
        return glm::dvec3{spin ? at(k).angVel : at(k).vel};
    }

    // The first sample later than `t` (size() if none).
    [[nodiscard]] int after(double t) const
    {
        int lo = 0, hi = size();
        while(lo < hi)
        {
            const int mid = (lo + hi) / 2;
            if(time(mid) > t)
            {
                hi = mid;
            }
            else
            {
                lo = mid + 1;
            }
        }
        return lo;
    }

    // Linear between the samples, the nearest one's past either end.
    [[nodiscard]] glm::dvec3 valueAt(double t) const
    {
        const int k = after(t);
        if(k == 0)
        {
            return value(0);
        }
        if(k >= size())
        {
            return value(size() - 1);
        }
        const double t0 = time(k - 1), t1 = time(k);
        return t1 > t0 ? glm::mix(value(k - 1), value(k), (t - t0) / (t1 - t0)) : value(k);
    }
};

// Where a history's samples put the hand at `t` (world units; linear between them).
[[nodiscard]] glm::vec3 posAt(const History& h, double t)
{
    const Track tr{h, false};
    const int k = tr.after(t);
    if(k == 0 || k >= tr.size())
    {
        return tr.at(k == 0 ? 0 : tr.size() - 1).pos;
    }
    const double t0 = tr.time(k - 1), t1 = tr.time(k);
    return glm::mix(tr.at(k - 1).pos, tr.at(k).pos, static_cast<float>(t1 > t0 ? (t - t0) / (t1 - t0) : 1.0));
}

[[nodiscard]] double det3(const double m[3][3])
{
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
           m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

// The value at `centre` of the polynomial of `degree` (0: the mean, 1: a line, 2: a parabola) in the time from `centre`
// fitted (least squares) to the track over [a, b], each moment weighed by its time, whatever the frames (the signal
// linear between the samples: three-point Gauss-Legendre on each piece between them is exact for the parabola's sums).
// `speed`: fitted to the signal's length (the speed) instead, in x. A line is the mean in the middle of a window, but
// unbiased at its end, where the window is cut at the release while the hand speeds up.
[[nodiscard]] glm::dvec3 localFit(const Track& tr, double centre, double a, double b, int degree, bool speed = false)
{
    const auto valueAt = [&](double t) {
        const glm::dvec3 v = tr.valueAt(t);
        return speed ? glm::dvec3{glm::length(v), 0.0, 0.0} : v;
    };
    if(tr.size() == 0)
    {
        return glm::dvec3{0.0};
    }
    if(b - a < 1e-6)
    {
        return valueAt(centre);
    }

    constexpr double gaussX[3] = {-0.7745966692414834, 0.0, 0.7745966692414834};
    constexpr double gaussW[3] = {5.0 / 9.0, 8.0 / 9.0, 5.0 / 9.0};
    double m[5]{};
    glm::dvec3 y[3]{glm::dvec3{0.0}, glm::dvec3{0.0}, glm::dvec3{0.0}};
    const auto piece = [&](double p, double q) {
        if(q <= p)
        {
            return;
        }
        const double half = (q - p) * 0.5, mid = (p + q) * 0.5;
        for(int g = 0; g < 3; g++)
        {
            const double t = mid + half * gaussX[g];
            const double w = half * gaussW[g];
            const glm::dvec3 v = valueAt(t);
            const double x = t - centre;
            double px = 1.0;
            for(int k = 0; k < 5; k++)
            {
                m[k] += w * px;
                if(k < 3)
                {
                    y[k] += v * (w * px);
                }
                px *= x;
            }
        }
    };
    double p = a;
    for(int k = tr.after(a); k < tr.size() && tr.time(k) < b; k++)
    {
        piece(p, tr.time(k));
        p = tr.time(k);
    }
    piece(p, b);

    if(degree >= 2)
    {
        const double mm[3][3] = {{m[0], m[1], m[2]}, {m[1], m[2], m[3]}, {m[2], m[3], m[4]}};
        if(const double d = det3(mm); za::abs(d) > 1e-30)
        {
            glm::dvec3 out{0.0};
            for(int c = 0; c < 3; c++)
            {
                const double mc[3][3] = {{y[0][c], m[1], m[2]}, {y[1][c], m[2], m[3]}, {y[2][c], m[3], m[4]}};
                out[c] = det3(mc) / d;
            }
            return out;
        }
    }
    if(degree >= 1)
    {
        if(const double d = m[0] * m[2] - m[1] * m[1]; d > 1e-30)
        {
            return (y[0] * m[2] - y[1] * m[1]) / d;
        }
    }
    return y[0] / m[0];
}

// The speeds the peak is looked for at (releasePeak: one a millisecond of the window).
constexpr double peakGrid = 0.001;
constexpr int peakGridMax = 1024;
za::Array<double, peakGridMax + 1> peakSpeeds;
// The peak: the middle of where the smoothed speed is within this share of its top (weighed by how far within), not
// its very top, which a flat-topped or noisy peak moves by tens of milliseconds for a hundredth of its speed.
constexpr double peakTopShare = 0.02;


// The speed at the peak: a parabola fitted (least squares) to the speeds within peakFit seconds of it, kept within 20%
// of the smoothed velocity's; not at the window's end (it would go on past the release).
constexpr double peakFit = 0.03;

// The samples' clock in slow motion (not Sandevistan) is slowed with the world (timescale::filterHands: t.time, and
// the velocities in the game's time), so a window of the release's (vr_throw_window, _peak_span, _dir_lookback,
// peakFit, peakGrid) in its seconds would cover 1/scale times as much of the real throw: at 0.3, the peak's
// averages and the direction took in a third of a second of the arm's arc, the spin was halved and the throw went up
// to 30 degrees off (voice note start 16:55: "the wrist snapping action feels way too strong"). The windows are taken
// in the player's real seconds instead (times this), and vr_throw_ang_threshold in real rad/s.
// vr_throw_slowmo_real_time 0: as before.
[[nodiscard]] double clockRate()
{
    return vr_throw_slowmo_real_time.value != 0.f ? static_cast<double>(za::clamp(timescale::handScale(), 0.05f, 1.f)) : 1.0;
}

// The windows' clock for a throw released at `releaseTime`, from its controller's own samples `own` (in step with the
// hand's): clockRate(), the player's real seconds, for a throw made at real speed; but one made slowly, with the slowed
// world (the voice note's "the same motion but slowly"), is the same motion as at full speed in the game's time, and
// its windows are taken there (1), or the same arc came out up to 5 degrees lower than at full speed (the windows a
// third of it at 0.3x). Which it is, from the controller's fastest speed around the release in the game's time: within
// what the slowed hand follows (vr_timescale_hand_speed, 8 m/s if 0) it moved with the world (1), from 1.5 times that
// it moved faster than the world (clockRate()), between them a blend (in the log). vr_throw_slowmo_tempo 0: always
// clockRate().
[[nodiscard]] double motionRate(const History& own, double releaseTime)
{
    const double real = clockRate();
    if(real >= 1.0 || vr_throw_slowmo_tempo.value == 0.f)
    {
        return real;
    }
    const double from = releaseTime - za::max(vr_throw_window.value, 0.f);
    float fastest = 0.f;
    for(int i = 0; i < own.count; i++)
    {
        if(const Sample& s = own.at(i); s.time >= from && s.time <= releaseTime)
        {
            fastest = za::max(fastest, glm::length(s.vel));
        }
    }
    const float follows = vr_timescale_hand_speed.value > 0.f ? vr_timescale_hand_speed.value : 8.f;
    const double past = za::clamp(za::log(static_cast<double>(za::max(fastest, 1e-3f) / follows)) / za::log(1.5), 0.0, 1.0);
    return za::pow(real, past);
}

// vr_throw_pitch: `vel` tilted up (down if negative) by that many degrees, about the level line square to it, its speed
// kept; not past straight up or down, and a throw with no level part (straight up or down) is left as it is.
[[nodiscard]] glm::vec3 pitched(const glm::vec3& vel)
{
    const float level = glm::length(glm::vec2{vel.x, vel.y});
    if(vr_throw_pitch.value == 0.f || level < 1e-4f)
    {
        return vel;
    }
    const float most = glm::radians(89.9f);
    const float up = CLAMP(-most, za::atan2(vel.z, level) + glm::radians(vr_throw_pitch.value), most);
    const float speed = glm::length(vel);
    const glm::vec2 way = glm::vec2{vel.x, vel.y} / level;
    return glm::vec3{way * (speed * za::cos(up)), speed * za::sin(up)};
}


// The peak of the controller's speed in the window before the release (vr_throw_window, up to the release: the
// samples after it are there or not depending on when the move carrying it is built, so never; vr_throw_lookahead is
// no longer used). The speed is smoothed (a line fitted over vr_throw_peak_span either side), its peak the middle of
// its top (peakTopShare); the throw's velocity is the smoothed one there, as fast as the speeds' parabola there says
// (peakFit), the way the hand went over vr_throw_dir_lookback before it; the spin the mean over twice the span.
// `lever`: metres along the hand's forward to the held object's centre (vr_throw_lever_arm): a clear wrist flick adds
// its spin's velocity there (none for two hands: their samples are the object's own). `wrist`: the estimate's flick, the
// part of it the hand's turn gives (none for two hands).
// `rate`: the samples' clock's seconds in one of the windows' (motionRate).
[[nodiscard]] Estimate releasePeak(const History& h, double releaseTime, float leverArm, bool wrist, double rate)
{
    if(h.count == 0)
    {
        return {};
    }
    const Track vel{h, false};
    const Track spin{h, true};
    const double lo = vel.time(0);
    const double newest = vel.time(vel.size() - 1);
    const double to = za::clamp(releaseTime, lo, newest);
    const double from = za::max(lo, to - static_cast<double>(za::max(vr_throw_window.value, 0.f)) * rate);
    const double span = static_cast<double>(za::max(vr_throw_peak_span.value, 0.f)) * rate;
    const auto smoothed = [&](double t) { return localFit(vel, t, za::max(t - span, lo), za::min(t + span, to), 1); };

    // The smoothed speed a millisecond apart over the window, and the middle of its top.
    const int n = za::clamp(static_cast<int>((to - from) / (peakGrid * rate) + 0.5), 0, peakGridMax);
    const double step = n > 0 ? (to - from) / n : 0.0;
    int best = 0;
    for(int i = 0; i <= n; i++)
    {
        peakSpeeds[i] = glm::length(smoothed(from + step * i));
        if(peakSpeeds[i] > peakSpeeds[best])
        {
            best = i;
        }
    }
    const double floor = peakSpeeds[best] * (1.0 - peakTopShare);
    int first = best, last = best;
    while(first > 0 && peakSpeeds[first - 1] >= floor)
    {
        first--;
    }
    while(last < n && peakSpeeds[last + 1] >= floor)
    {
        last++;
    }
    double weight = 0.0, sum = 0.0;
    for(int i = first; i <= last; i++)
    {
        weight += peakSpeeds[i] - floor;
        sum += (peakSpeeds[i] - floor) * (from + step * i);
    }
    const double peak = weight > 0.0 ? sum / weight : from + step * best;

    glm::dvec3 v = smoothed(peak);
    // As fast as the speeds' parabola about the peak says (a line's average of a peak is a little under its top); not
    // at the release (the parabola would run on past it).
    if(const double speed = glm::length(v); speed > 1e-4 && peak < to - step * 0.5)
    {
        const double fit = peakFit * rate;
        const double top = localFit(vel, peak, za::max(peak - fit, lo), za::min(peak + fit, to), 2, true).x;
        if(top > speed)
        {
            v *= za::min(top, 1.2 * speed) / speed;
        }
    }

    // The direction from the hand's way over the lookback before the peak: at the peak itself an overarm throw is
    // already curving down, and throws went lower than meant.
    if(const double lookback = static_cast<double>(vr_throw_dir_lookback.value) * rate; lookback > 0.0)
    {
        const glm::dvec3 dir = localFit(vel, peak, za::max(peak - lookback, lo), peak, 0);
        if(glm::length(dir) > 1e-4 && glm::length(v) > 1e-4)
        {
            v = glm::normalize(dir) * glm::length(v);
        }
    }
    glm::vec3 velocity{v};

    // The spin, the mean over twice the span (noisier).
    const glm::vec3 angVel{localFit(spin, peak, za::max(peak - span * 2.0, spin.time(0)), za::min(peak + span * 2.0, to), 0)};

    // Where the hand was and pointed at the peak (between the samples: a flick turns it 11 degrees in a frame at 72 fps).
    glm::vec3 forward = vel.at(vel.size() - 1).forward;
    glm::vec3 pos = vel.at(vel.size() - 1).pos;
    if(const int k = vel.after(peak); k == 0)
    {
        forward = vel.at(0).forward;
        pos = vel.at(0).pos;
    }
    else if(k < vel.size())
    {
        const double t0 = vel.time(k - 1), t1 = vel.time(k);
        const float s = static_cast<float>(t1 > t0 ? (peak - t0) / (t1 - t0) : 1.0);
        const glm::vec3 f = glm::mix(vel.at(k - 1).forward, vel.at(k).forward, s);
        forward = glm::length(f) > 1e-4f ? glm::normalize(f) : vel.at(k).forward;
        pos = glm::mix(vel.at(k - 1).pos, vel.at(k).pos, s);
    }

    // The object's centre, and the velocity a clear wrist flick adds there.
    const glm::vec3 lever = forward * leverArm; // metres
    glm::vec3 flick{0.f};
    if(glm::length(angVel) * static_cast<float>(rate) > vr_throw_ang_threshold.value) // (real rad/s)
    {
        flick = glm::cross(angVel, lever) * vr_throw_ang_factor.value;
        velocity += flick;
    }
    // The part of it the hand's turn about the wrist gives (vr_throw_wrist_dist behind the controller's point): a heavy
    // thing keeps less of it (weight::throwVelocity), the wrist being too weak to flick it. Not pitched (vr_throw_pitch
    // turns the whole throw a little).
    if(wrist)
    {
        flick += glm::cross(angVel, forward * za::max(vr_throw_wrist_dist.value, 0.f));
    }
    else
    {
        flick = glm::vec3{0.f};
    }

    return {pitched(velocity), angVel, flick, pos + lever * units::metresToUnits(), peak};
}

void push(History& h, const Sample& s)
{
    const double time = s.time;
    if(h.count > 0 && h.at(0).time == time)
    {
        // Resampled within the same frame (the hands were recomputed): replace.
        h.samples[(h.next - 1 + capacity) % capacity] = s;
        return;
    }

    if(h.count > 0 && time < h.at(0).time)
    {
        h.clear(); // time went backwards (new map, demo, another runtime clock): start over
    }

    h.samples[h.next] = s;
    h.next = (h.next + 1) % capacity;
    h.count = za::min(h.count + 1, capacity);
}

// vr_throw_slowmo_aim: in slow motion the slowed hand (timescale::filterHands) moves at most vr_timescale_hand_speed and
// turns at most _spin (of the game's time) and lags its controller; its velocity is then its catch-up towards where the
// controller is, not the way the arm moves (an overhand throw at full real speed went 21 to 31 degrees up at 0.3x, the
// same throw made slowly went as at full speed). `e` is the hand's estimate from `hand`; the throw keeps its speed (at
// most the controller's), its spin's size and its release point and time (the object leaves the hand as drawn), but
// goes the way the controller's own estimate (the same windows on `own`, in step with `hand`) does: its velocity, the
// wrist's flick scaled with it and its spin's axis. Nothing changes when `own` is `hand`'s (not slowed, Sandevistan).
[[nodiscard]] Estimate withOwnAim(const Estimate& e, const History& hand, const History& own, double releaseTime,
    float leverArm, bool wrist, double rate)
{
    if(vr_throw_slowmo_aim.value == 0.f || own.count == 0)
    {
        return e;
    }
    const Estimate o = releasePeak(own, releaseTime, leverArm, wrist, rate);
    if(o.vel == e.vel && o.angVel == e.angVel && o.flick == e.flick)
    {
        return e; // the controller's own motion: the hand didn't lag
    }

    Estimate out = e;
    const float speed = glm::length(e.vel);
    if(const float ownSpeed = glm::length(o.vel); ownSpeed > 1e-4f)
    {
        const float k = za::min(speed, ownSpeed) / ownSpeed;
        out.vel = o.vel * k;
        out.flick = o.flick * k;
        if(speed > 1e-4f)
        {
            out.aimTurn = glm::degrees(za::acos(za::clamp(glm::dot(e.vel, o.vel) / (speed * ownSpeed), -1.f, 1.f)));
        }
    }
    if(const float ownSpin = glm::length(o.angVel); ownSpin > 1e-4f)
    {
        out.angVel = o.angVel * (za::min(glm::length(e.angVel), ownSpin) / ownSpin);
    }
    // How far the hand was behind its controller at the peak (the samples are in step).
    if(hand.count > 0)
    {
        out.lag = glm::length(posAt(own, e.time) - posAt(hand, e.time)) / units::metresToUnits();
    }
    return out;
}

// estimateBothAt: the hands' samples `h0` and `h1` of the same frames, as the held object's (`centre` metres from the
// middle of the hands), into `both`.
void bothSamples(const History& h0, const History& h1, const glm::vec3& centre, History& both)
{
    // The hands' samples of the same frames, as the held object's: its centre's velocity (the middle's, and its spin
    // about the middle) and its spin (the hands' own about the line between them, and the line's turn: a rigid
    // body's). Not across a hand's gap (a frame one hand missed).
    both.clear();
    const float m2u = units::metresToUnits();
    int j = h1.count - 1;
    for(int i = h0.count - 1; i >= 0; i--) // oldest first
    {
        const Sample& a = h0.at(i);
        while(j >= 0 && h1.at(j).time < a.time)
        {
            j--;
        }
        if(j < 0 || h1.at(j).time != a.time)
        {
            continue;
        }
        const Sample& b = h1.at(j);
        const glm::vec3 line = (b.pos - a.pos) / m2u; // metres, from the off hand to the main
        const float d = glm::length(line);
        glm::vec3 spin = (a.angVel + b.angVel) * 0.5f;
        if(d > 0.03f)
        {
            const glm::vec3 u = line / d;
            const glm::vec3 dv = b.vel - a.vel;
            const glm::vec3 across = dv - u * glm::dot(dv, u);
            const glm::vec3 rigid = u * glm::dot(spin, u) + glm::cross(u, across) / d;
            spin = glm::mix(spin, rigid, glm::clamp((d - 0.03f) / 0.03f, 0.f, 1.f));
        }
        Sample s;
        s.time = a.time;
        s.pos = (a.pos + b.pos) * 0.5f + centre * m2u;
        s.vel = (a.vel + b.vel) * 0.5f + glm::cross(spin, centre);
        s.angVel = spin;
        s.forward = (a.forward + b.forward) * 0.5f;
        s.spinTime = (a.spinTime + b.spinTime) * 0.5;
        both.samples[both.next] = s;
        both.next = (both.next + 1) % capacity;
        both.count = za::min(both.count + 1, capacity);
    }
}

// Grip state per hand, for the release detection.
struct Grip
{
    bool held{false};
    float peak{0.f};
    double released{-1.0};
    float lastValue{0.f}; // the analog grip as of the frame before, and when (< 0: none)
    double lastTime{-1.0};
};

Grip grips[2];

} // namespace

void sample(int hand, double time, const Motion& handMotion, const Motion& controller)
{
    push(histories[hand], {time, handMotion.pos, handMotion.vel, handMotion.angVel, handMotion.forward,
                              time - static_cast<double>(handMotion.spinLag)});
    push(ownHistories[hand], {time, controller.pos, controller.vel, controller.angVel, controller.forward,
                                 time - static_cast<double>(controller.spinLag)});
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

    const History& own = ownHistories[hand];
    const double rate = motionRate(own, releaseTime);
    Estimate e = releasePeak(h, releaseTime, vr_throw_lever_arm.value, true, rate);
    e.rate = static_cast<float>(rate);
    return withOwnAim(e, h, own, releaseTime, vr_throw_lever_arm.value, true, rate);
}

Estimate estimateBothAt(double releaseTime, const glm::vec3& centre)
{
    bothSamples(histories[0], histories[1], centre, bothHistory);
    if(bothHistory.count == 0)
    {
        return {};
    }
    bothSamples(ownHistories[0], ownHistories[1], centre, ownBothHistory);
    const double rate = motionRate(ownBothHistory, releaseTime);
    Estimate e = releasePeak(bothHistory, releaseTime, 0.f, false, rate);
    e.rate = static_cast<float>(rate);
    return withOwnAim(e, bothHistory, ownBothHistory, releaseTime, 0.f, false, rate);
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
        double released = now; // (a button's: the frame it reads let go)

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
            g.peak = za::max(g.peak, in.gripValue);

            const Pose& pose = t.hands[hand];
            const bool throwing = pose.velocityValid && glm::length(pose.linearVelocity) * static_cast<float>(clockRate()) >
                                                            vr_throw_release_speed.value; // (real m/s)
            const float easedBelow = g.peak * (1.f - CLAMP(0.f, vr_throw_release_drop.value, 1.f));
            const bool eased = in.gripValue < easedBelow;
            if(in.gripValue < vr_throw_release_floor.value || (throwing && eased))
            {
                g.held = false;
                released = now;
                // When the grip crossed the line that let go, between this frame and the one before (linear): not up
                // to a frame late, as many milliseconds as the frame rate says (14 at 72 fps, 4 at 240).
                const float line = throwing && eased ? za::max(easedBelow, vr_throw_release_floor.value) : vr_throw_release_floor.value;
                if(g.lastTime >= 0.0 && now > g.lastTime && g.lastValue >= line && g.lastValue > in.gripValue)
                {
                    const double s = za::clamp(static_cast<double>((g.lastValue - line) / (g.lastValue - in.gripValue)), 0.0, 1.0);
                    released = g.lastTime + (now - g.lastTime) * s;
                }
            }
        }

        if(wasHeld && !g.held)
        {
            g.released = released;
        }
        g.lastValue = in.gripValue;
        g.lastTime = analog ? now : -1.0;
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
        h.clear();
    }
    for(History& h : ownHistories)
    {
        h.clear();
    }
    for(Grip& g : grips)
    {
        g = Grip{};
    }
}

} // namespace qvr::throwing
