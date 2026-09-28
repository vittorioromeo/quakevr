// vr_bodycal.cpp -- see vr_bodycal.hpp.

#include "vr_bodycal.hpp"
#include "vr_avatar.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_menuui.hpp"
#include "vr_still.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace qvr::bodycal
{
namespace
{

// ---------------------------------------------------------------------------------------------------------------------
// The steps

enum Step : int
{
    Stand,
    TPose,
    Forward,
    Up,
    Circles,
    Elbows,
    Wrists,
    StepCount
};

enum class Kind
{
    Static, // held still, captured once
    Motion, // recorded for a few seconds
};

struct StepInfo
{
    const char* name;  // in the session files
    const char* title; // on the page and above the pose
    const char* text;  // what to do (lines)
    const char* help;  // the page's help for its row
    Kind kind;
    float seconds; // Motion: how long it records (more while the moves don't cover enough)
};

constexpr StepInfo steps[StepCount] = {
    {"stand", "Stand Tall", "Stand up straight, look ahead.\nArms straight down at your sides,\nshoulders relaxed.",
        "Your eye height, and how high your shoulders are over your straight arms.", Kind::Static, 0.f},
    {"tpose", "T-Pose", "Arms straight out to the sides,\nlevel with your shoulders,\npalms down. Look ahead.",
        "Your arms' reach to the sides: how far apart your shoulders are.", Kind::Static, 0.f},
    {"forward", "Arms Forward", "Both arms straight out in front,\nlevel with your shoulders.\nShoulders relaxed: don't reach.",
        "Your arms' reach forward, and how far the shoulders come forward with them.", Kind::Static, 0.f},
    {"up", "Arms Up", "Both arms straight up above\nyour head. Look ahead.",
        "Your arms' reach up, and how far the shoulders rise with them.", Kind::Static, 0.f},
    {"circles", "Arm Circles",
        "Arms straight out in front: draw\nbig slow circles with both hands.\nKeep your elbows straight.",
        "Your hands on the sphere your straight arms sweep: where your shoulders turn, and your reach.", Kind::Motion,
        8.f},
    {"elbows", "Elbows Still",
        "Elbows at your sides, forearms\nforward. Keep the elbows still\nand wave the forearms: up and\ndown, in and out.",
        "Your wrists round your still elbows: the forearm's length and where the elbow is (the upper arm).", Kind::Motion,
        8.f},
    {"wrists", "Wrists",
        "Elbows at your sides, forearms\nforward and still. Bend your\nwrists up and down, and side\nto side.",
        "Your hands turning about your real wrists: checks where Hand Calibration puts the drawn wrist.", Kind::Motion,
        6.f},
};

constexpr double readSeconds = 2.5;      // the instructions before the countdown (the first step's longer)
constexpr double firstReadSeconds = 4.0;
constexpr double stillSeconds = 0.5;     // held this long within...
constexpr float stillHeadCm = 1.5f;      // the head
constexpr float stillWristCm = 2.0f;     // and each wrist
constexpr double hintAfter = 6.0;        // a static pose not taken this long: what's wrong with it
constexpr double giveUpSeconds = 40.0;   // then left out (Redo)
constexpr double extraSeconds = 6.0;     // a move recorded this much longer while it doesn't cover enough
constexpr double gotItSeconds = 1.0;     // "got it" shown

// ---------------------------------------------------------------------------------------------------------------------
// The samples

// One frame's tracking, in the world, for the body's sides (0 its left, 1 its right).
struct Raw
{
    double t{0.0};
    glm::vec3 head{0.f};
    glm::vec3 headAngles{0.f};
    float headHeight{0.f};
    glm::vec3 wrist[2]{};
    glm::mat3 hand[2]{glm::mat3{1.f}, glm::mat3{1.f}}; // the drawn empty hand's axes: towards the fingers, the thumb, the back
    glm::vec3 grip[2]{};
    glm::mat3 gripRot[2]{glm::mat3{1.f}, glm::mat3{1.f}}; // the controller's grip pose: forward, left, up
};

// The same in the chest's frame (forward, left, up; real metres from the chest joint): the body standing upright under
// the head (avatar::uprightChest), facing the step's yaw.
struct Local
{
    glm::vec3 wrist[2]{};
    glm::mat3 hand[2]{glm::mat3{1.f}, glm::mat3{1.f}};
    glm::vec3 grip[2]{};
    glm::mat3 gripRot[2]{glm::mat3{1.f}, glm::mat3{1.f}};
    bool hasGrip{false};
};

struct StepData
{
    bool taken{false};
    float yaw{0.f};          // the body's facing (degrees, world)
    std::vector<Raw> raw;    // Static: the capture (averaged); Motion: every frame
    std::vector<Local> local; // (a synthetic session file's, already in the chest's frame)
};

// ---------------------------------------------------------------------------------------------------------------------
// The result

enum Grade : int
{
    NotTaken,
    Good,
    Fair,
    Redo
};

struct StepQuality
{
    Grade grade{NotTaken};
    float value{0.f};   // cm: Static, the reach's residual (the worse arm); Motion, the rms of the samples kept
    float kept{1.f};    // Motion: the share of the samples kept
    float coverage{0.f}; // Motion: how much the moves spread (degrees or cm)
    std::string note;
};

struct Result
{
    bool valid{false};
    bool seated{false};
    float scale{1.f};
    float eyeHeight{0.f}, eyeHeightNow{0.f};
    float upper{0.f}, fore{0.f}, upperSe{0.f}, foreSe{0.f}; // cm
    float upperNow{0.f}, foreNow{0.f};
    float stretchNow{1.f};
    glm::vec3 offset{0.f}; // vr_body_shoulders_back, _up, _out
    glm::vec3 offsetSe{0.f}; // cm (real)
    float raise{0.f}, swing{0.f}, raiseSe{0.f}, swingSe{0.f};
    float raiseNow{0.f}, swingNow{0.f};
    glm::vec3 eyeToShoulder{0.f}, eyeToShoulderNow{0.f}; // cm: behind the eyes, below them, from the middle
    float reachSide[2]{};                                // cm
    StepQuality quality[StepCount];
    bool wristValid[2]{};
    float wristOff[2]{};    // cm: the drawn wrist from the real one
    glm::vec3 wristOffHand[2]{}; // cm, along the drawn hand: towards the fingers, the thumb, the back of the hand
    float wristRms[2]{};
    float rms{0.f};
    float dropped{0.f};
    int iterations{0};
    std::string when;
};

// ---------------------------------------------------------------------------------------------------------------------
// The session

enum class Sub
{
    Read,
    Countdown,
    Wait,   // Static: for the pose held still
    Record, // Motion
};

struct Session
{
    Phase phase{Phase::Idle};
    int returnPage{-1};
    int world{0};
    bool seated{false};
    std::vector<int> todo; // the steps still to take
    Sub sub{Sub::Read};
    double subStart{0.0};
    still::Countdown countdown;
    still::Window window{{{still::Window::Kind::Point}, {still::Window::Kind::Point}, {still::Window::Kind::Point},
        {still::Window::Kind::AveragedDirection}}};
    std::vector<Raw> recent; // the window's frames (the capture averages them)
    std::vector<Raw> recording;
    float yaw{0.f};
    double gotIt{-1.0};
    std::string hint;
    std::string message; // why it stopped
    int lastFrame{-1};
    StepData data[StepCount];
    float eyeHeight{0.f}; // the height the chest frames are for (the Stand capture's, or the setting)
};
Session ses;
Result result;
bool applied = false;
int versionCounter = 0;
std::string lineBuf[24];
std::string rowBuf[StepCount];
std::string savedFile;

void bump()
{
    versionCounter++;
}

[[nodiscard]] int currentStep()
{
    return ses.todo.empty() ? -1 : ses.todo.front();
}

[[nodiscard]] int sideHand(int side)
{
    const int leftHand = vr_lefthanded.value ? HAND_MAIN : HAND_OFF;
    return side == 0 ? leftHand : 1 - leftHand;
}

[[nodiscard]] float metres(float units)
{
    return units / units::metresToUnits();
}

[[nodiscard]] float angleBetween(const glm::vec3& a, const glm::vec3& b)
{
    return still::degreesBetween(a, b);
}

// Columns: forward, left, up, from Quake angles.
[[nodiscard]] glm::mat3 axesOf(const glm::vec3& angles)
{
    glm::vec3 f, r, u;
    hands::angleVectors(angles, f, r, u);
    return glm::mat3{f, -r, u};
}

// ---------------------------------------------------------------------------------------------------------------------
// The chest's frame

[[nodiscard]] hands::State stateOf(const Raw& r, float yaw)
{
    hands::State s{};
    s.valid = true;
    s.head = r.head;
    s.headAngles = r.headAngles;
    s.headHeight = r.headHeight;
    s.bodyYaw = yaw;
    s.standingHeight = r.headHeight;
    return s;
}

struct ChestFrame
{
    glm::vec3 pos{0.f};
    glm::mat3 toLocal{1.f}; // world directions to (forward, left, up)
    float m2u{1.f};

    [[nodiscard]] glm::vec3 point(const glm::vec3& p) const { return toLocal * (p - pos) / m2u; }
    [[nodiscard]] glm::mat3 axes(const glm::mat3& m) const { return toLocal * m; }
};

[[nodiscard]] ChestFrame chestFrame(const Raw& r, float yaw, float eyeHeight)
{
    const avatar::Frame c = avatar::uprightChest(stateOf(r, yaw), yaw, eyeHeight);
    // The chest bone's columns: up the spine, right, forward.
    const glm::mat3 flu{c.rot[2], -c.rot[1], c.rot[0]};
    return {c.pos, glm::transpose(flu), units::metresToUnits()};
}

[[nodiscard]] Local toLocal(const Raw& r, float yaw, float eyeHeight)
{
    const ChestFrame f = chestFrame(r, yaw, eyeHeight);
    Local l;
    for(int side = 0; side < 2; side++)
    {
        l.wrist[side] = f.point(r.wrist[side]);
        l.hand[side] = f.axes(r.hand[side]);
        l.grip[side] = f.point(r.grip[side]);
        l.gripRot[side] = f.axes(r.gripRot[side]);
    }
    l.hasGrip = true;
    return l;
}

[[nodiscard]] std::vector<Local> localsOf(const StepData& d, float eyeHeight)
{
    if(!d.local.empty())
    {
        return d.local;
    }
    std::vector<Local> out;
    out.reserve(d.raw.size());
    for(const Raw& r : d.raw)
    {
        out.push_back(toLocal(r, d.yaw, eyeHeight));
    }
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// The body as it is set now

struct Settings
{
    float scale{1.f};
    glm::vec3 offset{0.f};
    float upper{0.f}, fore{0.f}; // metres
    float raise{25.f}, swing{20.f};
    bool calibrated{false};
};

[[nodiscard]] Settings current(float eyeHeight)
{
    Settings s;
    s.scale = eyeHeight / units::modelEyeHeight;
    s.offset = {vr_body_shoulders_back.value, vr_body_shoulders_up.value, vr_body_shoulders_out.value};
    float bu = 0.f, bf = 0.f;
    avatar::armBones(bu, bf);
    const float length = CLAMP(0.5f, vr_body_arm_length.value, 2.f);
    s.upper = vr_body_upper_arm.value > 0.f ? CLAMP(10.f, vr_body_upper_arm.value, 60.f) * 0.01f : bu * s.scale * length;
    s.fore = vr_body_forearm.value > 0.f ? CLAMP(10.f, vr_body_forearm.value, 60.f) * 0.01f : bf * s.scale * length;
    s.raise = vr_body_shoulder_up.value;
    s.swing = vr_body_shoulder_forward.value;
    s.calibrated = vr_body_upper_arm.value > 0.f && vr_body_forearm.value > 0.f;
    return s;
}

[[nodiscard]] avatar::ShoulderModel modelOf(const Settings& s)
{
    avatar::ShoulderModel m;
    m.scale = s.scale;
    m.offset = s.offset;
    m.armLength = s.upper + s.fore;
    m.upDegrees = s.raise;
    m.forwardDegrees = s.swing;
    m.calibrated = s.calibrated;
    return m;
}

// The shoulder at rest (the arm hanging), in the chest's frame.
[[nodiscard]] glm::vec3 restShoulder(int side, const avatar::ShoulderModel& m)
{
    return avatar::shoulderInChest(side, glm::vec3{0.f, 0.f, -2.f}, m);
}

// ---------------------------------------------------------------------------------------------------------------------
// The fit: the shoulders' place, the reach and the forearm, the shoulders' rise and swing, and the elbows of the Elbows
// step, from all the samples at once (Levenberg-Marquardt, then Tukey's biweight on the recorded moves' samples, four
// times over). Each residual is in centimetres.

enum Param : int
{
    PBack,
    PUp,
    POut,
    PReach, // metres: the shoulder joint to the drawn wrist, the arm straight
    PFore,  // metres
    PRaise, // degrees
    PSwing,
    PElbowL, // 3: the left elbow in the Elbows step (the chest's frame)
    PElbowR = PElbowL + 3,
    ParamCount = PElbowR + 3
};

enum ObsKind : int
{
    ObsReach, // |wrist - shoulder(wrist)| = reach (the arm straight)
    ObsElbow, // |wrist - elbow| = forearm (the Elbows step)
    ObsLink,  // |elbow - shoulder| = reach - forearm (the upper arm)
};

struct Obs
{
    ObsKind kind;
    int step;
    int side;
    glm::vec3 w;
    double weight; // its fixed weight (a capture counts as many samples)
};

constexpr double resUnit = 0.01; // metres per unit of residual
constexpr double captureWeight = 4.47213595; // sqrt(20)
// Priors: the shoulders' rise and swing (degrees), their spread; the forearm's share of the reach, its spread (metres).
constexpr double priorRaise = 20.0, priorRaiseSd = 8.0, priorSwing = 15.0, priorSwingSd = 8.0;
constexpr double priorForeShare = 0.45, priorForeSd = 0.05;
constexpr double priorOffsetSd = 0.15; // metres of the model: vr_body_shoulders_*
constexpr double priorElbowSd = 1.0;   // metres: the elbows only where the Elbows step says

struct FitInput
{
    float scale{1.f};
    std::vector<Obs> obs;
    glm::vec3 elbow0[2]{};
};

struct FitOutput
{
    std::array<double, ParamCount> p{};
    std::array<double, ParamCount> se{};
    std::vector<double> residual; // per observation, cm
    std::vector<double> weight;   // the robust weight at the end (0: dropped)
    double upperSe{0.0};
    int iterations{0};
    bool ok{false};
};

[[nodiscard]] avatar::ShoulderModel modelOf(const std::array<double, ParamCount>& p, float scale)
{
    avatar::ShoulderModel m;
    m.scale = scale;
    m.offset = glm::vec3{static_cast<float>(p[PBack]), static_cast<float>(p[PUp]), static_cast<float>(p[POut])};
    m.armLength = static_cast<float>(p[PReach]);
    m.upDegrees = static_cast<float>(p[PRaise]);
    m.forwardDegrees = static_cast<float>(p[PSwing]);
    m.calibrated = true;
    return m;
}

// The residuals of the observations (not weighted), then the priors'.
void residuals(const FitInput& in, const std::array<double, ParamCount>& p, std::vector<double>& out)
{
    const avatar::ShoulderModel m = modelOf(p, in.scale);
    const glm::dvec3 elbow[2] = {{p[PElbowL], p[PElbowL + 1], p[PElbowL + 2]}, {p[PElbowR], p[PElbowR + 1], p[PElbowR + 2]}};
    out.resize(in.obs.size() + 5 + 6);
    for(size_t i = 0; i < in.obs.size(); i++)
    {
        const Obs& o = in.obs[i];
        const glm::dvec3 w{o.w};
        double r = 0.0;
        switch(o.kind)
        {
            case ObsReach:
            {
                const glm::dvec3 s{avatar::shoulderInChest(o.side, o.w, m)};
                r = glm::length(w - s) - p[PReach];
                break;
            }
            case ObsElbow: r = glm::length(w - elbow[o.side]) - p[PFore]; break;
            case ObsLink:
            {
                const glm::dvec3 s{avatar::shoulderInChest(o.side, o.w, m)};
                r = glm::length(elbow[o.side] - s) - (p[PReach] - p[PFore]);
                break;
            }
        }
        out[i] = r / resUnit;
    }
    size_t k = in.obs.size();
    out[k++] = (p[PRaise] - priorRaise) / priorRaiseSd;
    out[k++] = (p[PSwing] - priorSwing) / priorSwingSd;
    out[k++] = (p[PFore] - priorForeShare * p[PReach]) / priorForeSd;
    out[k++] = p[PBack] / priorOffsetSd + 0.0 * p[PUp];
    out[k++] = p[POut] / priorOffsetSd;
    for(int side = 0; side < 2; side++)
    {
        for(int c = 0; c < 3; c++)
        {
            out[k++] = (elbow[side][c] - in.elbow0[side][c]) / priorElbowSd;
        }
    }
}

// Solves A x = b (n x n, row-major) by Gaussian elimination with partial pivoting; false if singular.
[[nodiscard]] bool solve(std::vector<double> a, std::vector<double> b, int n, std::vector<double>& x)
{
    for(int c = 0; c < n; c++)
    {
        int best = c;
        for(int r = c + 1; r < n; r++)
        {
            if(std::abs(a[r * n + c]) > std::abs(a[best * n + c]))
            {
                best = r;
            }
        }
        if(std::abs(a[best * n + c]) < 1e-300)
        {
            return false;
        }
        if(best != c)
        {
            for(int k = 0; k < n; k++)
            {
                std::swap(a[c * n + k], a[best * n + k]);
            }
            std::swap(b[c], b[best]);
        }
        for(int r = c + 1; r < n; r++)
        {
            const double f = a[r * n + c] / a[c * n + c];
            if(f == 0.0)
            {
                continue;
            }
            for(int k = c; k < n; k++)
            {
                a[r * n + k] -= f * a[c * n + k];
            }
            b[r] -= f * b[c];
        }
    }
    x.assign(n, 0.0);
    for(int r = n - 1; r >= 0; r--)
    {
        double v = b[r];
        for(int k = r + 1; k < n; k++)
        {
            v -= a[r * n + k] * x[k];
        }
        x[r] = v / a[r * n + r];
    }
    return true;
}

[[nodiscard]] double stepOf(int j)
{
    return j == PRaise || j == PSwing ? 1e-2 : 1e-5;
}

// Levenberg-Marquardt on the weighted residuals; returns the last Jacobian's normal matrix (for the covariance).
void levenberg(const FitInput& in, const std::vector<double>& weights, std::array<double, ParamCount>& p, std::vector<double>& jtj,
    double& cost, int& iterations)
{
    const int n = ParamCount;
    const size_t no = in.obs.size();
    std::vector<double> r, rp, rm;
    const auto weighted = [&](const std::array<double, ParamCount>& q, std::vector<double>& out) {
        residuals(in, q, out);
        for(size_t i = 0; i < no; i++)
        {
            out[i] *= weights[i];
        }
    };
    const auto sumSq = [](const std::vector<double>& v) {
        double s = 0.0;
        for(double x : v)
        {
            s += x * x;
        }
        return s;
    };
    weighted(p, r);
    cost = sumSq(r);
    double lambda = 1e-3;
    const size_t m = r.size();
    std::vector<double> J(m * n);
    for(int it = 0; it < 80; it++)
    {
        iterations++;
        for(int j = 0; j < n; j++)
        {
            std::array<double, ParamCount> a = p, b = p;
            const double h = stepOf(j);
            a[j] += h;
            b[j] -= h;
            weighted(a, rp);
            weighted(b, rm);
            for(size_t i = 0; i < m; i++)
            {
                J[i * n + j] = (rp[i] - rm[i]) / (2.0 * h);
            }
        }
        jtj.assign(n * n, 0.0);
        std::vector<double> g(n, 0.0);
        for(size_t i = 0; i < m; i++)
        {
            const double* row = &J[i * n];
            for(int a = 0; a < n; a++)
            {
                g[a] += row[a] * r[i];
                for(int b = a; b < n; b++)
                {
                    jtj[a * n + b] += row[a] * row[b];
                }
            }
        }
        for(int a = 0; a < n; a++)
        {
            for(int b = 0; b < a; b++)
            {
                jtj[a * n + b] = jtj[b * n + a];
            }
        }
        bool improved = false;
        double largest = 0.0;
        for(int tries = 0; tries < 12; tries++)
        {
            std::vector<double> A = jtj, rhs(n), step;
            for(int a = 0; a < n; a++)
            {
                A[a * n + a] += lambda * (jtj[a * n + a] + 1e-9);
                rhs[a] = -g[a];
            }
            if(!solve(A, rhs, n, step))
            {
                lambda *= 4.0;
                continue;
            }
            std::array<double, ParamCount> q = p;
            largest = 0.0;
            for(int a = 0; a < n; a++)
            {
                q[a] += step[a];
                largest = std::max(largest, std::abs(step[a]) / stepOf(a));
            }
            q[PReach] = std::max(q[PReach], 0.2);
            q[PFore] = CLAMP(0.08, q[PFore], q[PReach] - 0.08);
            std::vector<double> rq;
            weighted(q, rq);
            const double c = sumSq(rq);
            if(c < cost)
            {
                p = q;
                r = std::move(rq);
                const double drop = cost - c;
                cost = c;
                lambda = std::max(lambda / 3.0, 1e-9);
                improved = drop > 1e-9 * cost;
                break;
            }
            lambda *= 4.0;
        }
        if(!improved || largest < 1e-3)
        {
            break;
        }
    }
}

[[nodiscard]] FitOutput fit(const FitInput& in, const std::array<double, ParamCount>& start)
{
    FitOutput out;
    out.p = start;
    const size_t no = in.obs.size();
    std::vector<double> weights(no);
    for(size_t i = 0; i < no; i++)
    {
        weights[i] = in.obs[i].weight;
    }
    std::vector<double> jtj;
    double cost = 0.0;
    std::vector<double> r;
    for(int round = 0; round < 4; round++)
    {
        levenberg(in, weights, out.p, jtj, cost, out.iterations);
        residuals(in, out.p, r);
        // Tukey's biweight on the moves' samples (weight 1), by kind: the spread from their median absolute deviation.
        for(int kind : {ObsReach, ObsElbow})
        {
            std::vector<double> vals;
            for(size_t i = 0; i < no; i++)
            {
                if(in.obs[i].kind == kind && in.obs[i].weight == 1.0)
                {
                    vals.push_back(r[i]);
                }
            }
            if(vals.size() < 10)
            {
                continue;
            }
            std::vector<double> sorted = vals;
            std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
            const double med = sorted[sorted.size() / 2];
            for(double& v : sorted)
            {
                v = std::abs(v - med);
            }
            std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
            const double spread = std::max(1.4826 * sorted[sorted.size() / 2], 0.3);
            for(size_t i = 0; i < no; i++)
            {
                if(in.obs[i].kind == kind && in.obs[i].weight == 1.0)
                {
                    const double u = r[i] / (4.685 * spread);
                    weights[i] = std::abs(u) < 1.0 ? (1.0 - u * u) * (1.0 - u * u) : 0.0;
                }
            }
        }
    }
    levenberg(in, weights, out.p, jtj, cost, out.iterations);
    residuals(in, out.p, r);
    out.residual.assign(r.begin(), r.begin() + static_cast<std::ptrdiff_t>(no));
    out.weight = weights;

    // The covariance: (J'J)^-1 times the residuals' variance.
    double used = 0.0;
    for(size_t i = 0; i < no; i++)
    {
        used += weights[i] > 0.0 ? 1.0 : 0.0;
    }
    const double dof = std::max(1.0, used + 11.0 - static_cast<double>(ParamCount));
    const double s2 = cost / dof;
    const int n = ParamCount;
    std::vector<double> cov(n * n, 0.0);
    bool okCov = true;
    for(int c = 0; c < n && okCov; c++)
    {
        std::vector<double> e(n, 0.0), col;
        e[c] = 1.0;
        okCov = solve(jtj, e, n, col);
        for(int k = 0; k < n && okCov; k++)
        {
            cov[k * n + c] = col[k] * s2;
        }
    }
    if(okCov)
    {
        for(int k = 0; k < n; k++)
        {
            out.se[k] = std::sqrt(std::max(0.0, cov[k * n + k]));
        }
        out.upperSe = std::sqrt(std::max(0.0, cov[PReach * n + PReach] + cov[PFore * n + PFore] - 2.0 * cov[PReach * n + PFore]));
    }
    out.ok = std::isfinite(out.p[PReach]) && out.p[PReach] > 0.3 && out.p[PReach] < 1.2 && out.p[PFore] > 0.1;
    return out;
}

// The wrist: the point that stays still in the controller's frame as the hand turns about it (the real wrist), from
// the grip poses (R p + t = c: p in the grip's frame, c in the chest's). The drawn wrist's place in the grip's frame, and
// its offset from the real one along the drawn hand's axes.
struct Pivot
{
    bool ok{false};
    glm::vec3 real{0.f}, drawn{0.f}; // in the grip's frame
    glm::vec3 offsetHand{0.f};       // drawn - real, along the hand (towards the fingers, the thumb, its back), metres
    float rms{0.f};                  // metres
    float spread{0.f};               // degrees: the smaller of the turns' two widest spreads
};

[[nodiscard]] Pivot pivot(const std::vector<Local>& ls, int side)
{
    Pivot pv;
    std::vector<double> A(36, 0.0), b(6, 0.0), x;
    int n = 0;
    for(const Local& l : ls)
    {
        if(!l.hasGrip)
        {
            continue;
        }
        const glm::mat3& R = l.gripRot[side];
        const glm::vec3& t = l.grip[side];
        // Rows [R | -I], right side -t.
        for(int row = 0; row < 3; row++)
        {
            double a[6];
            for(int c = 0; c < 3; c++)
            {
                a[c] = R[c][row];
                a[3 + c] = row == c ? -1.0 : 0.0;
            }
            for(int i = 0; i < 6; i++)
            {
                b[i] += a[i] * -t[row];
                for(int j = 0; j < 6; j++)
                {
                    A[i * 6 + j] += a[i] * a[j];
                }
            }
        }
        n++;
    }
    if(n < 30 || !solve(A, b, 6, x))
    {
        return pv;
    }
    pv.real = glm::vec3{static_cast<float>(x[0]), static_cast<float>(x[1]), static_cast<float>(x[2])};
    const glm::vec3 centre{static_cast<float>(x[3]), static_cast<float>(x[4]), static_cast<float>(x[5])};
    double sq = 0.0;
    glm::vec3 drawn{0.f}, offset{0.f};
    glm::mat3 ref = ls.front().gripRot[side];
    std::vector<glm::vec3> turns;
    for(const Local& l : ls)
    {
        const glm::mat3& R = l.gripRot[side];
        const glm::vec3 e = R * pv.real + l.grip[side] - centre;
        sq += glm::dot(e, e);
        const glm::vec3 d = glm::transpose(R) * (l.wrist[side] - l.grip[side]);
        drawn += d;
        const glm::mat3 handInGrip = glm::transpose(R) * l.hand[side];
        offset += glm::transpose(handInGrip) * (d - pv.real);
        const glm::quat q = glm::quat_cast(R * glm::transpose(ref));
        const float angle = 2.f * std::acos(std::fmin(1.f, std::abs(q.w)));
        const glm::vec3 axis = glm::length(glm::vec3{q.x, q.y, q.z}) > 1e-6f ? glm::normalize(glm::vec3{q.x, q.y, q.z}) * (q.w < 0.f ? -1.f : 1.f)
                                                                           : glm::vec3{0.f};
        turns.push_back(axis * glm::degrees(angle));
    }
    const float k = 1.f / static_cast<float>(ls.size());
    pv.drawn = drawn * k;
    pv.offsetHand = offset * k;
    pv.rms = static_cast<float>(std::sqrt(sq / static_cast<double>(ls.size())));
    // The turns' spread: the covariance's two largest eigenvalues (a 3x3 by power iteration, deflated).
    glm::vec3 mean{0.f};
    for(const glm::vec3& t : turns)
    {
        mean += t;
    }
    mean *= k;
    glm::mat3 C{0.f};
    for(const glm::vec3& t : turns)
    {
        C += glm::outerProduct(t - mean, t - mean) * k;
    }
    float eig[2]{};
    for(int e = 0; e < 2; e++)
    {
        glm::vec3 v{0.577f, 0.577f, 0.577f};
        for(int it = 0; it < 50; it++)
        {
            const glm::vec3 w = C * v;
            if(glm::length(w) < 1e-9f)
            {
                break;
            }
            v = glm::normalize(w);
        }
        eig[e] = glm::dot(v, C * v);
        C -= eig[e] * glm::outerProduct(v, v);
    }
    pv.spread = std::sqrt(std::max(0.f, eig[1]));
    pv.ok = true;
    return pv;
}

// The two largest spreads (standard deviations) of a set of points or directions.
void spreads(const std::vector<glm::vec3>& pts, float& first, float& second)
{
    first = second = 0.f;
    if(pts.size() < 3)
    {
        return;
    }
    glm::vec3 mean{0.f};
    for(const glm::vec3& p : pts)
    {
        mean += p;
    }
    mean /= static_cast<float>(pts.size());
    glm::mat3 C{0.f};
    for(const glm::vec3& p : pts)
    {
        C += glm::outerProduct(p - mean, p - mean) / static_cast<float>(pts.size());
    }
    float eig[2]{};
    for(int e = 0; e < 2; e++)
    {
        glm::vec3 v{0.6f, 0.5f, 0.62f};
        for(int it = 0; it < 60; it++)
        {
            const glm::vec3 w = C * v;
            if(glm::length(w) < 1e-12f)
            {
                break;
            }
            v = glm::normalize(w);
        }
        eig[e] = glm::dot(v, C * v);
        C -= eig[e] * glm::outerProduct(v, v);
    }
    first = std::sqrt(std::max(0.f, eig[0]));
    second = std::sqrt(std::max(0.f, eig[1]));
}

// How well a recorded move covers what the step needs (Circles: the arms' directions, degrees; Elbows: the wrists'
// sweep, cm; Wrists: the hands' turns, degrees), the least of both arms.
[[nodiscard]] float coverage(int step, const std::vector<Local>& ls, const avatar::ShoulderModel& m)
{
    float least = 1e9f;
    for(int side = 0; side < 2; side++)
    {
        std::vector<glm::vec3> pts;
        for(const Local& l : ls)
        {
            if(step == Circles)
            {
                const glm::vec3 d = l.wrist[side] - restShoulder(side, m);
                pts.push_back(glm::degrees(1.f) * glm::normalize(d));
            }
            else if(step == Elbows)
            {
                pts.push_back(l.wrist[side] * 100.f);
            }
        }
        float a = 0.f, b = 0.f;
        if(step == Wrists)
        {
            const Pivot pv = pivot(ls, side);
            b = pv.ok ? pv.spread : 0.f;
        }
        else
        {
            spreads(pts, a, b);
        }
        least = std::min(least, b);
    }
    return least;
}

constexpr float circlesNeed = 10.f; // degrees (a circle 30 degrees round has 21)
constexpr float elbowsNeed = 5.f;   // cm
constexpr float wristsNeed = 8.f;   // degrees

[[nodiscard]] float coverageNeeded(int step)
{
    return step == Circles ? circlesNeed : step == Elbows ? elbowsNeed : wristsNeed;
}

// ---------------------------------------------------------------------------------------------------------------------
// From the samples to the result

// The shoulders' place relative to the eyes of a body standing `eyeHeight` tall, the head level (cm: behind the eyes,
// below them, from the middle).
[[nodiscard]] glm::vec3 eyeToShoulder(const avatar::ShoulderModel& m, float eyeHeight)
{
    Raw r;
    r.head = glm::vec3{0.f, 0.f, eyeHeight * units::metresToUnits()};
    r.headAngles = glm::vec3{0.f};
    r.headHeight = eyeHeight;
    const avatar::Frame c = avatar::uprightChest(stateOf(r, 0.f), 0.f, eyeHeight);
    const glm::vec3 s = restShoulder(0, m); // forward, left, up
    const glm::vec3 world = c.pos + (c.rot[2] * s.x - c.rot[1] * s.y + c.rot[0] * s.z) * units::metresToUnits();
    const glm::vec3 rel = (world - r.head) / units::metresToUnits() * 100.f;
    return {-rel.x, -rel.z, std::abs(rel.y)};
}

[[nodiscard]] Grade gradeStatic(float cm)
{
    return cm <= 1.5f ? Good : cm <= 3.f ? Fair : Redo;
}

[[nodiscard]] Grade gradeMotion(float rms, float kept, bool covered)
{
    if(!covered || kept < 0.7f || rms > 2.f)
    {
        return Redo;
    }
    return rms <= 1.f && kept >= 0.85f ? Good : Fair;
}

void analyse()
{
    Result r;
    r.seated = ses.seated;
    r.eyeHeightNow = units::eyeHeight();
    // The eye height: standing, the Stand capture's.
    r.eyeHeight = r.eyeHeightNow;
    if(!ses.seated && ses.data[Stand].taken && !ses.data[Stand].raw.empty())
    {
        r.eyeHeight = ses.data[Stand].raw.front().headHeight;
    }
    else if(!ses.seated && ses.eyeHeight > 0.5f)
    {
        r.eyeHeight = ses.eyeHeight;
    }
    r.eyeHeight = CLAMP(0.8f, r.eyeHeight, 2.4f);
    r.scale = r.eyeHeight / units::modelEyeHeight;

    const Settings now = current(r.eyeHeightNow);
    r.upperNow = now.upper * 100.f;
    r.foreNow = now.fore * 100.f;
    r.stretchNow = std::max(1.f, vr_body_arm_stretch.value);
    r.raiseNow = now.raise;
    r.swingNow = now.swing;
    r.eyeToShoulderNow = eyeToShoulder(modelOf(now), r.eyeHeightNow);

    // The chest frames at the new height; the current body's shoulders there, for the samples' first checks.
    Settings ref = now;
    ref.scale = r.scale;
    const avatar::ShoulderModel refModel = modelOf(ref);
    const float reach0 = ref.upper + ref.fore;
    std::vector<Local> locals[StepCount];
    for(int st = 0; st < StepCount; st++)
    {
        if(ses.data[st].taken)
        {
            locals[st] = localsOf(ses.data[st], r.eyeHeight);
        }
    }

    FitInput in;
    in.scale = r.scale;
    glm::vec3 elbowMean[2]{};
    for(int st = 0; st < StepCount; st++)
    {
        const std::vector<Local>& ls = locals[st];
        if(ls.empty())
        {
            continue;
        }
        if(steps[st].kind == Kind::Static)
        {
            for(int side = 0; side < 2; side++)
            {
                in.obs.push_back({ObsReach, st, side, ls.front().wrist[side], captureWeight});
            }
            continue;
        }
        if(st == Wrists)
        {
            continue;
        }
        // At most 240 samples an arm, evenly.
        const size_t every = std::max<size_t>(1, ls.size() / 240);
        int counted[2]{};
        for(size_t i = 0; i < ls.size(); i += every)
        {
            for(int side = 0; side < 2; side++)
            {
                const glm::vec3 w = ls[i].wrist[side];
                const glm::vec3 d = w - restShoulder(side, refModel);
                if(st == Circles)
                {
                    // The arm out in front (the moves between left out).
                    if(glm::length(d) < 0.6f * reach0 || d.x <= 0.f)
                    {
                        continue;
                    }
                    in.obs.push_back({ObsReach, st, side, w, 1.0});
                }
                else
                {
                    // The wrist below the shoulder, the arm bent.
                    if(d.z >= 0.f || glm::length(d) > 0.9f * reach0)
                    {
                        continue;
                    }
                    in.obs.push_back({ObsElbow, st, side, w, 1.0});
                    elbowMean[side] += w;
                    counted[side]++;
                }
            }
        }
        if(st == Elbows)
        {
            for(int side = 0; side < 2; side++)
            {
                if(counted[side] > 20)
                {
                    elbowMean[side] /= static_cast<float>(counted[side]);
                    in.obs.push_back({ObsLink, st, side, elbowMean[side], captureWeight});
                }
            }
        }
    }

    // The start: the body as it is (at the new height), the elbows under the shoulders.
    std::array<double, ParamCount> p0{};
    p0[PBack] = now.offset.x;
    p0[PUp] = now.offset.y;
    p0[POut] = now.offset.z;
    p0[PReach] = std::max(0.35f, reach0);
    p0[PFore] = priorForeShare * p0[PReach];
    p0[PRaise] = priorRaise;
    p0[PSwing] = priorSwing;
    for(int side = 0; side < 2; side++)
    {
        const glm::vec3 e = restShoulder(side, refModel) - glm::vec3{0.f, 0.f, ref.upper};
        in.elbow0[side] = e;
        for(int c = 0; c < 3; c++)
        {
            p0[PElbowL + side * 3 + c] = e[c];
        }
    }

    int reachObs = 0;
    for(const Obs& o : in.obs)
    {
        reachObs += o.kind == ObsReach ? 1 : 0;
    }
    if(reachObs < 4)
    {
        r.valid = false;
        r.when = "too few poses to measure";
        result = r;
        return;
    }

    const FitOutput f = fit(in, p0);
    r.iterations = f.iterations;
    r.valid = f.ok;
    const auto& p = f.p;
    r.upper = static_cast<float>((p[PReach] - p[PFore]) * 100.0);
    r.fore = static_cast<float>(p[PFore] * 100.0);
    r.upperSe = static_cast<float>(f.upperSe * 100.0);
    r.foreSe = static_cast<float>(f.se[PFore] * 100.0);
    r.offset = glm::vec3{static_cast<float>(p[PBack]), static_cast<float>(p[PUp]), static_cast<float>(p[POut])};
    r.offsetSe = glm::vec3{static_cast<float>(f.se[PBack]), static_cast<float>(f.se[PUp]), static_cast<float>(f.se[POut])} *
                 r.scale * 100.f;
    r.raise = static_cast<float>(p[PRaise]);
    r.swing = static_cast<float>(p[PSwing]);
    r.raiseSe = static_cast<float>(f.se[PRaise]);
    r.swingSe = static_cast<float>(f.se[PSwing]);
    r.eyeToShoulder = eyeToShoulder(modelOf(p, r.scale), r.eyeHeight);

    // Each step's fit, each side's reach.
    double sq = 0.0, sw = 0.0;
    int dropped = 0, moves = 0;
    double sideSum[2]{}, sideW[2]{};
    for(int st = 0; st < StepCount; st++)
    {
        StepQuality& q = r.quality[st];
        if(!ses.data[st].taken)
        {
            q.grade = NotTaken;
            continue;
        }
        double s2 = 0.0, w2 = 0.0, worst = 0.0;
        int n = 0, kept = 0;
        for(size_t i = 0; i < in.obs.size(); i++)
        {
            const Obs& o = in.obs[i];
            if(o.step != st || o.kind == ObsLink)
            {
                continue;
            }
            n++;
            const double w = f.weight[i];
            if(w > 0.0)
            {
                kept++;
            }
            s2 += w * f.residual[i] * f.residual[i];
            w2 += w;
            worst = std::max(worst, std::abs(f.residual[i]));
            if(o.kind == ObsReach)
            {
                sideSum[o.side] += w * f.residual[i];
                sideW[o.side] += w;
            }
        }
        if(steps[st].kind == Kind::Static)
        {
            q.value = static_cast<float>(worst * resUnit * 100.0);
            q.grade = n ? gradeStatic(q.value) : Redo;
            if(q.grade == Redo)
            {
                q.note = "the arms' reach doesn't match the other poses: were they straight?";
            }
        }
        else if(st != Wrists)
        {
            q.value = w2 > 0.0 ? static_cast<float>(std::sqrt(s2 / w2) * resUnit * 100.0) : 99.f;
            q.kept = n ? static_cast<float>(kept) / static_cast<float>(n) : 0.f;
            q.coverage = coverage(st, locals[st], modelOf(p, r.scale));
            const bool covered = q.coverage >= coverageNeeded(st);
            q.grade = n ? gradeMotion(q.value, q.kept, covered) : Redo;
            q.note = !n            ? "no samples in the pose"
                     : !covered    ? (st == Circles ? "circles too small" : "forearms moved too little")
                     : q.kept < 0.7f ? (st == Circles ? "arms not kept straight" : "elbows didn't stay still")
                                      : "";
            dropped += n - kept;
            moves += n;
        }
        sq += s2;
        sw += w2;
    }
    r.rms = sw > 0.0 ? static_cast<float>(std::sqrt(sq / sw) * resUnit * 100.0) : 0.f;
    r.dropped = moves ? static_cast<float>(dropped) / static_cast<float>(moves) : 0.f;
    for(int side = 0; side < 2; side++)
    {
        r.reachSide[side] = static_cast<float>((p[PReach] + (sideW[side] > 0.0 ? sideSum[side] / sideW[side] * resUnit : 0.0)) * 100.0);
    }

    // The wrists: the hand calibration's check.
    if(ses.data[Wrists].taken)
    {
        StepQuality& q = r.quality[Wrists];
        float worstRms = 0.f;
        bool ok = true;
        for(int side = 0; side < 2; side++)
        {
            const Pivot pv = pivot(locals[Wrists], side);
            r.wristValid[side] = pv.ok;
            ok = ok && pv.ok;
            if(pv.ok)
            {
                r.wristOff[side] = glm::length(pv.offsetHand) * 100.f;
                r.wristOffHand[side] = pv.offsetHand * 100.f;
                r.wristRms[side] = pv.rms * 100.f;
                worstRms = std::max(worstRms, pv.rms * 100.f);
            }
        }
        q.value = worstRms;
        q.coverage = coverage(Wrists, locals[Wrists], modelOf(p, r.scale));
        const bool covered = q.coverage >= wristsNeed;
        q.grade = !ok ? Redo : !covered || worstRms > 2.f ? Redo : worstRms > 1.f ? Fair : Good;
        q.note = !ok ? "no samples" : !covered ? "wrists bent too little" : worstRms > 2.f ? "forearms moved" : "";
    }

    char stamp[32];
    const std::time_t t = std::time(nullptr);
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", std::localtime(&t));
    r.when = stamp;
    result = r;
}

// ---------------------------------------------------------------------------------------------------------------------
// Applying (and previewing) the result

struct Setting
{
    cvar_t* cvar;
    std::string value;
};

[[nodiscard]] std::vector<Setting> candidate()
{
    const Result& r = result;
    std::vector<Setting> list;
    const auto add = [&](cvar_t& c, const char* fmt, float v) { list.push_back({&c, va(fmt, v)}); };
    if(!r.seated)
    {
        add(vr_height_calibration, "%.4f", r.eyeHeight);
    }
    add(vr_body_upper_arm, "%.1f", r.upper);
    add(vr_body_forearm, "%.1f", r.fore);
    add(vr_body_shoulders_back, "%.3f", r.offset.x);
    add(vr_body_shoulders_up, "%.3f", r.offset.y);
    add(vr_body_shoulders_out, "%.3f", r.offset.z);
    add(vr_body_shoulder_up, "%.1f", r.raise);
    add(vr_body_shoulder_forward, "%.1f", r.swing);
    return list;
}

[[nodiscard]] std::vector<Setting> snapshot(const std::vector<Setting>& of)
{
    std::vector<Setting> list;
    for(const Setting& s : of)
    {
        list.push_back({s.cvar, s.cvar->string});
    }
    return list;
}

void write(const std::vector<Setting>& list)
{
    for(const Setting& s : list)
    {
        Cvar_SetQuick(s.cvar, s.value.c_str());
    }
}

std::vector<Setting> originals;   // the settings before the preview
bool previewOn = false;           // the result's values set for the preview
bool showNew = true;              // the preview shows the new values
float bodyDebugBefore = -1.f;     // vr_body_debug before the preview showed the body

void setPreview(bool on)
{
    if(on == previewOn)
    {
        return;
    }
    if(on)
    {
        originals = snapshot(candidate());
        write(candidate());
    }
    else
    {
        write(originals);
    }
    previewOn = on;
}

void showBody(bool on)
{
    if(on && bodyDebugBefore < 0.f)
    {
        bodyDebugBefore = vr_body_debug.value;
        Cvar_SetValueQuick(&vr_body_debug, vr_bodycal_preview.value >= 2.f ? 3.f : 2.f);
    }
    else if(!on && bodyDebugBefore >= 0.f)
    {
        Cvar_SetValueQuick(&vr_body_debug, bodyDebugBefore);
        bodyDebugBefore = -1.f;
    }
}

// vr_bodycal_undo: the settings before the last Apply ("name value;..."), kept with the config.
void saveUndo(const std::vector<Setting>& before)
{
    std::string s;
    for(const Setting& b : before)
    {
        s += std::string(b.cvar->name) + " " + b.value + ";";
    }
    Cvar_SetQuick(&vr_bodycal_undo, s.c_str());
}

// ---------------------------------------------------------------------------------------------------------------------
// The session's file (bodycal/<date>.txt): the settings, each step's frames, the result.

void saveSession()
{
    const std::string dir = std::string(com_gamedir) + "/bodycal";
    Sys_mkdir(dir.c_str());
    char stamp[32];
    const std::time_t t = std::time(nullptr);
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", std::localtime(&t));
    const std::string path = dir + "/" + stamp + ".txt";
    FILE* f = fopen(path.c_str(), "w");
    if(!f)
    {
        return;
    }
    fprintf(f, "# Quake VR body calibration (vr_bodycal_refit <this file> fits it again; docs/vr-port/ROUND21.md, \"Body "
               "calibration\")\n");
    fprintf(f, "seated %d\neye_height %.4f\n", ses.seated ? 1 : 0, ses.eyeHeight);
    fprintf(f, "settings");
    for(cvar_t* c : {&vr_height_calibration, &vr_world_scale, &vr_body_torso_back, &vr_body_eye_forward, &vr_body_eye_up,
            &vr_body_shoulders_back, &vr_body_shoulders_up, &vr_body_shoulders_out, &vr_body_upper_arm, &vr_body_forearm,
            &vr_body_arm_length, &vr_body_arm_stretch, &vr_body_shoulder_up, &vr_body_shoulder_forward, &vr_gunangle,
            &vr_gunyaw, &vr_handcal_x, &vr_handcal_y, &vr_handcal_z, &vr_handcal_roll, &vr_lefthanded})
    {
        fprintf(f, " %s=%s", c->name, c->string);
    }
    fprintf(f, "\n");
    const float m2u = units::metresToUnits();
    for(int st = 0; st < StepCount; st++)
    {
        const StepData& d = ses.data[st];
        if(!d.taken)
        {
            continue;
        }
        fprintf(f, "step %s yaw %.3f\n", steps[st].name, d.yaw);
        // r: time, head (metres, world / units per metre), head angles, head height, then per side: wrist, hand axes
        // (fingers, thumb, back), grip, grip axes (forward, left, up).
        for(const Raw& r : d.raw)
        {
            fprintf(f, "r %.4f %.5f %.5f %.5f %.3f %.3f %.3f %.4f", r.t, r.head.x / m2u, r.head.y / m2u, r.head.z / m2u,
                r.headAngles.x, r.headAngles.y, r.headAngles.z, r.headHeight);
            for(int side = 0; side < 2; side++)
            {
                const glm::vec3 w = r.wrist[side] / m2u, g = r.grip[side] / m2u;
                fprintf(f, "  %.5f %.5f %.5f", w.x, w.y, w.z);
                for(int c = 0; c < 3; c++)
                {
                    fprintf(f, " %.4f %.4f %.4f", r.hand[side][c].x, r.hand[side][c].y, r.hand[side][c].z);
                }
                fprintf(f, "  %.5f %.5f %.5f", g.x, g.y, g.z);
                for(int c = 0; c < 3; c++)
                {
                    fprintf(f, " %.4f %.4f %.4f", r.gripRot[side][c].x, r.gripRot[side][c].y, r.gripRot[side][c].z);
                }
            }
            fprintf(f, "\n");
        }
        // (A synthetic session's samples, already in the chest's frame.)
        for(const Local& l : d.local)
        {
            fprintf(f, "l");
            for(int side = 0; side < 2; side++)
            {
                fprintf(f, "  %.5f %.5f %.5f", l.wrist[side].x, l.wrist[side].y, l.wrist[side].z);
                if(l.hasGrip)
                {
                    for(int c = 0; c < 3; c++)
                    {
                        fprintf(f, " %.4f %.4f %.4f", l.hand[side][c].x, l.hand[side][c].y, l.hand[side][c].z);
                    }
                    fprintf(f, "  %.5f %.5f %.5f", l.grip[side].x, l.grip[side].y, l.grip[side].z);
                    for(int c = 0; c < 3; c++)
                    {
                        fprintf(f, " %.4f %.4f %.4f", l.gripRot[side][c].x, l.gripRot[side][c].y, l.gripRot[side][c].z);
                    }
                }
            }
            fprintf(f, "\n");
        }
    }
    const Result& r = result;
    fprintf(f, "result valid %d eye_height %.4f upper %.2f fore %.2f shoulders %.4f %.4f %.4f raise %.2f swing %.2f rms %.2f "
               "dropped %.3f reach_left %.2f reach_right %.2f wrist_off %.2f %.2f\n",
        r.valid ? 1 : 0, r.eyeHeight, r.upper, r.fore, r.offset.x, r.offset.y, r.offset.z, r.raise, r.swing, r.rms, r.dropped,
        r.reachSide[0], r.reachSide[1], r.wristOff[0], r.wristOff[1]);
    fclose(f);
    savedFile = path;
    Con_Printf("Body Calibration: saved %s\n", path.c_str());
}

[[nodiscard]] bool loadSession(const char* name, std::string& error)
{
    std::string path = name;
    std::ifstream in(path);
    if(!in)
    {
        path = std::string(com_gamedir) + "/" + name;
        in.open(path);
    }
    if(!in)
    {
        path = std::string(com_gamedir) + "/bodycal/" + name;
        in.open(path);
    }
    if(!in)
    {
        error = std::string("can't open ") + name;
        return false;
    }
    Session s;
    s.eyeHeight = units::eyeHeight();
    int st = -1;
    std::string line;
    const float m2u = units::metresToUnits();
    while(std::getline(in, line))
    {
        std::istringstream w(line);
        std::string key;
        if(!(w >> key) || key[0] == '#')
        {
            continue;
        }
        if(key == "seated")
        {
            int v = 0;
            w >> v;
            s.seated = v != 0;
        }
        else if(key == "eye_height")
        {
            w >> s.eyeHeight;
        }
        else if(key == "step")
        {
            std::string n, y;
            w >> n >> y;
            st = -1;
            for(int i = 0; i < StepCount; i++)
            {
                st = n == steps[i].name ? i : st;
            }
            if(st >= 0)
            {
                s.data[st] = StepData{};
                s.data[st].taken = true;
                if(y == "yaw")
                {
                    w >> s.data[st].yaw;
                }
            }
        }
        else if(key == "r" && st >= 0)
        {
            Raw r;
            glm::vec3 head;
            w >> r.t >> head.x >> head.y >> head.z >> r.headAngles.x >> r.headAngles.y >> r.headAngles.z >> r.headHeight;
            r.head = head * m2u;
            for(int side = 0; side < 2; side++)
            {
                glm::vec3 v;
                w >> v.x >> v.y >> v.z;
                r.wrist[side] = v * m2u;
                for(int c = 0; c < 3; c++)
                {
                    w >> r.hand[side][c].x >> r.hand[side][c].y >> r.hand[side][c].z;
                }
                w >> v.x >> v.y >> v.z;
                r.grip[side] = v * m2u;
                for(int c = 0; c < 3; c++)
                {
                    w >> r.gripRot[side][c].x >> r.gripRot[side][c].y >> r.gripRot[side][c].z;
                }
            }
            if(w)
            {
                s.data[st].raw.push_back(r);
            }
        }
        else if(key == "l" && st >= 0)
        {
            Local l;
            std::vector<float> v;
            float x;
            while(w >> x)
            {
                v.push_back(x);
            }
            if(v.size() == 6)
            {
                l.wrist[0] = {v[0], v[1], v[2]};
                l.wrist[1] = {v[3], v[4], v[5]};
            }
            else if(v.size() == 2 * 24)
            {
                for(int side = 0; side < 2; side++)
                {
                    const float* q = &v[static_cast<size_t>(side) * 24];
                    l.wrist[side] = {q[0], q[1], q[2]};
                    for(int c = 0; c < 3; c++)
                    {
                        l.hand[side][c] = {q[3 + c * 3], q[4 + c * 3], q[5 + c * 3]};
                    }
                    l.grip[side] = {q[12], q[13], q[14]};
                    for(int c = 0; c < 3; c++)
                    {
                        l.gripRot[side][c] = {q[15 + c * 3], q[16 + c * 3], q[17 + c * 3]};
                    }
                }
                l.hasGrip = true;
            }
            else
            {
                continue;
            }
            s.data[st].local.push_back(l);
        }
    }
    for(int i = 0; i < StepCount; i++)
    {
        if(s.data[i].taken && s.data[i].raw.empty() && s.data[i].local.empty())
        {
            s.data[i].taken = false;
        }
    }
    const int page = ses.returnPage;
    ses = std::move(s);
    ses.returnPage = page;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// The flow

void stopCapture(const char* why)
{
    ses.phase = result.valid && !applied ? Phase::Result : Phase::Idle;
    ses.message = why ? why : "";
    ses.todo.clear();
    if(why && *why)
    {
        Con_Printf("Body Calibration: %s\n", why);
    }
    bump();
    if(ses.returnPage >= 0 && key_dest != key_menu)
    {
        menu::reopen(ses.returnPage);
    }
}

void finish()
{
    analyse();
    applied = false;
    saveSession();
    ses.todo.clear();
    if(result.valid)
    {
        S_LocalSound("misc/talk.wav");
        ses.phase = Phase::Result;
        showNew = true;
        Con_Printf("Body Calibration: upper arm %.1f cm, forearm %.1f cm, reach %.1f cm; shoulders %.1f cm below the eyes, "
                   "%.1f behind, %.1f apart; eye height %.3f m; fit %.2f cm rms, %.0f%% dropped\n",
            result.upper, result.fore, result.upper + result.fore, result.eyeToShoulder.y, result.eyeToShoulder.x,
            2.f * result.eyeToShoulder.z, result.eyeHeight, result.rms, 100.f * result.dropped);
    }
    else
    {
        S_LocalSound("misc/menu3.wav");
        ses.phase = Phase::Idle;
        ses.message = "couldn't measure: redo the poses";
        Con_Printf("Body Calibration: couldn't measure (%s)\n", result.when.c_str());
    }
    bump();
    if(ses.returnPage >= 0 && key_dest != key_menu)
    {
        menu::reopen(ses.returnPage);
    }
}

void beginStep(double now)
{
    ses.sub = Sub::Read;
    ses.subStart = now;
    ses.window.clear();
    ses.recent.clear();
    ses.recording.clear();
    ses.hint.clear();
}

void nextStep(double now)
{
    if(!ses.todo.empty())
    {
        ses.todo.erase(ses.todo.begin());
    }
    bump();
    if(ses.todo.empty())
    {
        finish();
        return;
    }
    beginStep(now);
}

bool begin(std::vector<int> todo, int returnPage)
{
    if(!vrActive() || cls.state != ca_connected)
    {
        Con_Printf("Body Calibration: in a game, with the headset\n");
        return false;
    }
    if(ses.phase == Phase::Capturing)
    {
        return false;
    }
    setPreview(false);
    showBody(false);
    if(key_dest == key_menu)
    {
        menuui::backToGame(HAND_MAIN);
    }
    Cbuf_AddText("-attack\n-offhandattack\n");
    ses.returnPage = returnPage;
    ses.world = worldGeneration();
    ses.phase = Phase::Capturing;
    ses.todo = std::move(todo);
    ses.message.clear();
    ses.lastFrame = -1;
    beginStep(realtime);
    bump();
    const int step = currentStep();
    Con_Printf("Body Calibration: %s, %d pose%s from %s\n", ses.seated ? "seated" : "standing",
        static_cast<int>(ses.todo.size()), ses.todo.size() == 1 ? "" : "s", step >= 0 ? steps[step].title : "-");
    return true;
}

[[nodiscard]] std::vector<int> allSteps(bool seated)
{
    std::vector<int> v;
    for(int i = seated ? TPose : Stand; i < StepCount; i++)
    {
        v.push_back(i);
    }
    return v;
}

// Why the pose held isn't the step's (empty: it is), from the averaged frame.
[[nodiscard]] std::string wrongPose(int step, const Raw& r)
{
    const Local l = toLocal(r, ses.yaw, ses.eyeHeight);
    Settings now = current(ses.eyeHeight);
    const avatar::ShoulderModel m = modelOf(now);
    const float reach = now.upper + now.fore;
    const char* sides[2] = {"left", "right"};
    for(int side = 0; side < 2; side++)
    {
        const glm::vec3 d = l.wrist[side] - restShoulder(side, m);
        const float len = glm::length(d);
        const glm::vec3 out{0.f, side == 0 ? 1.f : -1.f, 0.f};
        switch(step)
        {
            case Stand:
                if(std::abs(r.headAngles.x) > 25.f)
                {
                    return "look ahead";
                }
                if(angleBetween(d, {0.f, 0.f, -1.f}) > 30.f || len < 0.7f * reach)
                {
                    return va("%s arm: straight down at your side", sides[side]);
                }
                break;
            case TPose:
                if(angleBetween(d, out) > 30.f || len < 0.6f * reach)
                {
                    return va("%s arm: straight out to the side", sides[side]);
                }
                break;
            case Forward:
                if(angleBetween(d, {1.f, 0.f, 0.f}) > 30.f || len < 0.6f * reach)
                {
                    return va("%s arm: straight out in front", sides[side]);
                }
                break;
            case Up:
                if(angleBetween(d, {0.f, 0.f, 1.f}) > 35.f || len < 0.6f * reach)
                {
                    return va("%s arm: straight up", sides[side]);
                }
                break;
            default: break;
        }
    }
    return {};
}

[[nodiscard]] Raw rawOf(const hands::State& s, double now, bool& ok)
{
    Raw r;
    r.t = now;
    r.head = s.head;
    r.headAngles = s.headAngles;
    r.headHeight = s.headHeight;
    ok = true;
    for(int side = 0; side < 2; side++)
    {
        const int hand = sideHand(side);
        view::EmptyHand e;
        ok = ok && view::emptyHandPose(s, hand, e);
        r.wrist[side] = e.wrist;
        r.hand[side] = glm::mat3{e.forward, e.up, e.back};
        r.grip[side] = s.gripPos[hand];
        r.gripRot[side] = axesOf(s.gripRot[hand]);
    }
    return r;
}

// The average of frames (positions averaged, axes of the middle one).
[[nodiscard]] Raw average(const std::vector<Raw>& rs)
{
    Raw m = rs[rs.size() / 2];
    const float k = 1.f / static_cast<float>(rs.size());
    m.head = glm::vec3{0.f};
    m.headHeight = 0.f;
    for(int side = 0; side < 2; side++)
    {
        m.wrist[side] = m.grip[side] = glm::vec3{0.f};
    }
    for(const Raw& r : rs)
    {
        m.head += r.head * k;
        m.headHeight += r.headHeight * k;
        for(int side = 0; side < 2; side++)
        {
            m.wrist[side] += r.wrist[side] * k;
            m.grip[side] += r.grip[side] * k;
        }
    }
    return m;
}

// ---------------------------------------------------------------------------------------------------------------------
// What the player sees: the text and the ghost

// The ghost's arm on `side` for `step` at `t` seconds: the upper arm's and the forearm's directions, and the hand's
// (forward, left, up of the player; the ghost is mirrored).
void ghostArm(int step, int side, float t, glm::vec3& upper, glm::vec3& fore, glm::vec3& hand)
{
    const float sg = side == 0 ? 1.f : -1.f;
    const glm::vec3 F{1.f, 0.f, 0.f}, L{0.f, 1.f, 0.f}, U{0.f, 0.f, 1.f};
    const auto rot = [](const glm::vec3& axis, float deg, const glm::vec3& v) {
        return glm::mat3_cast(glm::angleAxis(glm::radians(deg), glm::normalize(axis))) * v;
    };
    switch(step)
    {
        case Stand: upper = fore = hand = glm::normalize(-U + 0.08f * sg * L); break;
        case TPose: upper = fore = hand = sg * L; break;
        case Forward: upper = fore = hand = F; break;
        case Up: upper = fore = hand = glm::normalize(U + 0.05f * F); break;
        case Circles:
        {
            const float w = glm::two_pi<float>() / 2.6f;
            upper = fore = hand = glm::normalize(F + 0.45f * (std::cos(w * t) * U + std::sin(w * t) * sg * L));
            break;
        }
        case Elbows:
        {
            upper = -U;
            const float flex = 90.f + 35.f * std::sin(glm::two_pi<float>() * t / 2.2f);
            fore = rot(U, 30.f * std::sin(glm::two_pi<float>() * t / 3.1f) * sg, rot(-L, flex, -U));
            hand = fore;
            break;
        }
        default: // Wrists
        {
            upper = -U;
            fore = F;
            hand = rot(U, 20.f * std::sin(glm::two_pi<float>() * t / 2.7f) * sg,
                rot(-L, 40.f * std::sin(glm::two_pi<float>() * t / 1.9f), F));
            break;
        }
    }
}

void drawGhost(const hands::State& s, int step, float t)
{
    const float m2u = units::metresToUnits();
    const float k = units::eyeHeight() / units::modelEyeHeight;
    const glm::vec3 fwd = hands::forward(glm::vec3{0.f, ses.yaw, 0.f});
    const glm::vec3 left{-fwd.y, fwd.x, 0.f};
    const glm::vec3 floor{s.head.x, s.head.y, s.head.z - s.headHeight * m2u};
    const glm::vec3 centre = floor + fwd * (1.7f * m2u);
    // A point of the player's own body (forward, left, up; metres of the model) as the ghost facing them shows it: a
    // mirror's image.
    const auto at = [&](const glm::vec3& p) { return centre + (-fwd * p.x + left * p.y + glm::vec3{0.f, 0.f, p.z}) * (k * m2u); };
    const glm::vec4 body{0.35f, 0.85f, 1.f, 0.45f}, arm{0.4f, 1.f, 1.f, 0.9f};
    const float width = 0.012f * m2u;
    const auto seg = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec4& c) { lines::line(at(a), at(b), width, c, c); };

    const glm::vec3 neck{-0.01f, 0.f, 1.47f}, chest{-0.01f, 0.f, 1.28f}, pelvis{0.f, 0.f, 0.95f}, head{0.02f, 0.f, 1.6f};
    // The head: a ring facing the player.
    for(int i = 0; i < 12; i++)
    {
        const float a0 = glm::two_pi<float>() * static_cast<float>(i) / 12.f, a1 = glm::two_pi<float>() * static_cast<float>(i + 1) / 12.f;
        seg(head + glm::vec3{0.f, 0.1f * std::cos(a0), 0.11f * std::sin(a0)}, head + glm::vec3{0.f, 0.1f * std::cos(a1), 0.11f * std::sin(a1)}, body);
    }
    seg(neck, glm::vec3{0.f, 0.f, 1.49f}, body);
    seg(neck, chest, body);
    seg(chest, pelvis, body);
    for(int side = 0; side < 2; side++)
    {
        const float sg = side == 0 ? 1.f : -1.f;
        const glm::vec3 shoulder{-0.01f, 0.19f * sg, 1.42f}, hip{0.f, 0.09f * sg, 0.92f}, knee{0.01f, 0.09f * sg, 0.5f},
            ankle{-0.02f, 0.09f * sg, 0.08f}, toe{0.13f, 0.09f * sg, 0.02f};
        seg(neck, shoulder, body);
        seg(pelvis, hip, body);
        seg(hip, knee, body);
        seg(knee, ankle, body);
        seg(ankle, toe, body);
        glm::vec3 u, f, h;
        ghostArm(step, side, t, u, f, h);
        const glm::vec3 elbow = shoulder + u * 0.29f, wrist = elbow + f * 0.26f, tip = wrist + h * 0.09f;
        seg(shoulder, elbow, arm);
        seg(elbow, wrist, arm);
        seg(wrist, tip, arm);
    }
}

[[nodiscard]] std::string gold(const char* s)
{
    std::string out;
    for(const char* c = s; *c; c++)
    {
        out += static_cast<char>(*c | 0x80);
    }
    return out;
}

void drawText(const hands::State& s, int step, double now)
{
    int index = 1, total = static_cast<int>(ses.todo.size());
    for(int i = 0; i < StepCount; i++)
    {
        if(ses.data[i].taken && ses.todo.end() == std::find(ses.todo.begin(), ses.todo.end(), i))
        {
            index++;
            total++;
        }
    }
    std::string text = gold("BODY CALIBRATION") + va("  %d of %d\n", index, total);
    text += gold(steps[step].title);
    text += "\n";
    text += steps[step].text;
    text += "\n\n";
    if(ses.gotIt >= 0.0 && now - ses.gotIt < gotItSeconds)
    {
        text += "got it";
    }
    else if(ses.sub == Sub::Read)
    {
        text += "get ready";
    }
    else if(ses.sub == Sub::Countdown)
    {
        text += va("get into the pose: %d", ses.countdown.remaining(now));
    }
    else if(ses.sub == Sub::Wait)
    {
        text += ses.hint.empty() ? std::string("hold still") : ses.hint;
    }
    else
    {
        const double length = steps[step].seconds;
        const double t = now - ses.subStart;
        const int bars = CLAMP(0, static_cast<int>(20.0 * t / length), 20);
        text += std::string(static_cast<size_t>(bars), '=') + std::string(static_cast<size_t>(20 - bars), '.');
        if(!ses.hint.empty())
        {
            text += "\n" + ses.hint;
        }
    }
    text += "\nmenu button: stop";
    const float m2u = units::metresToUnits();
    const glm::vec3 fwd = hands::forward(glm::vec3{0.f, s.headAngles.y, 0.f});
    const glm::vec3 at = s.head + fwd * (0.9f * m2u) - glm::vec3{0.f, 0.f, 0.18f * m2u};
    text3d::queueOverlay(text, at, glm::vec3{0.f, s.headAngles.y, 0.f}, 0.045f);
}

// ---------------------------------------------------------------------------------------------------------------------
// Commands

void start_f()
{
    const bool seated = Cmd_Argc() > 1 ? !q_strcasecmp(Cmd_Argv(1), "seated") : vr_bodycal_seated.value != 0.f;
    Cvar_SetValueQuick(&vr_bodycal_seated, seated ? 1.f : 0.f);
    restart(-1);
}

void step_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_bodycal_step <1..%d>: that pose again (the others kept), then the fit\n", StepCount);
        return;
    }
    redo(atoi(Cmd_Argv(1)) - 1, -1);
}

void print_f()
{
    for(int i = 0; statusLine(i); i++)
    {
        Con_Printf("%s\n", statusLine(i));
    }
    for(int i = 0; i < StepCount; i++)
    {
        Con_Printf("%s\n", stepRow(i));
    }
    const Result& r = result;
    if(r.valid)
    {
        Con_Printf("bodycal: upper %.2f +-%.2f fore %.2f +-%.2f cm; shoulders back %.4f up %.4f out %.4f (+-%.1f %.1f %.1f cm); "
                   "raise %.1f +-%.1f swing %.1f +-%.1f; eye %.4f; reach L %.2f R %.2f; eye->shoulder behind %.2f below %.2f "
                   "half %.2f; rms %.2f dropped %.3f; wrist off L %.2f (%.2f %.2f %.2f, rms %.2f) R %.2f (%.2f %.2f %.2f, "
                   "rms %.2f); iterations %d\n",
            r.upper, r.upperSe, r.fore, r.foreSe, r.offset.x, r.offset.y, r.offset.z, r.offsetSe.x, r.offsetSe.y, r.offsetSe.z,
            r.raise, r.raiseSe, r.swing, r.swingSe, r.eyeHeight, r.reachSide[0], r.reachSide[1], r.eyeToShoulder.x,
            r.eyeToShoulder.y, r.eyeToShoulder.z, r.rms, r.dropped, r.wristOff[0], r.wristOffHand[0].x, r.wristOffHand[0].y,
            r.wristOffHand[0].z, r.wristRms[0], r.wristOff[1], r.wristOffHand[1].x, r.wristOffHand[1].y, r.wristOffHand[1].z,
            r.wristRms[1], r.iterations);
        for(int i = 0; i < StepCount; i++)
        {
            const StepQuality& q = r.quality[i];
            Con_Printf("bodycal step %s grade %d value %.2f kept %.3f coverage %.1f %s\n", steps[i].name, q.grade, q.value, q.kept,
                q.coverage, q.note.c_str());
        }
    }
}

void refit_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_bodycal_refit <file>: fits a saved session (bodycal/<date>.txt) again, the result to Apply\n");
        return;
    }
    if(ses.phase == Phase::Capturing)
    {
        return;
    }
    std::string error;
    if(!loadSession(Cmd_Argv(1), error))
    {
        Con_Printf("Body Calibration: %s\n", error.c_str());
        return;
    }
    setPreview(false);
    analyse();
    applied = false;
    ses.phase = result.valid ? Phase::Result : Phase::Idle;
    bump();
    print_f();
}

void apply_f()
{
    apply();
}

void cancel_f()
{
    cancel();
}

void undo_f()
{
    undo();
}

// vr_bodycal_debug: each empty hand's wrist (tracking metres from the head, as vr_mock_hand takes them) and the drawn
// hand's, and the chest frame's view of both wrists.
bool debugNext = false;
void debug_f()
{
    debugNext = true;
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------

void init()
{
    Cmd_AddCommand("vr_bodycal", start_f);
    Cmd_AddCommand("vr_bodycal_step", step_f);
    Cmd_AddCommand("vr_bodycal_refit", refit_f);
    Cmd_AddCommand("vr_bodycal_print", print_f);
    Cmd_AddCommand("vr_bodycal_apply", apply_f);
    Cmd_AddCommand("vr_bodycal_cancel", cancel_f);
    Cmd_AddCommand("vr_bodycal_undo", undo_f);
    Cmd_AddCommand("vr_bodycal_debug", debug_f);
}

Phase phase()
{
    return ses.phase;
}

bool start(int returnPage)
{
    ses.seated = vr_bodycal_seated.value != 0.f;
    std::vector<int> todo;
    for(int i : allSteps(ses.seated))
    {
        if(!ses.data[i].taken)
        {
            todo.push_back(i);
        }
    }
    if(todo.empty())
    {
        return restart(returnPage);
    }
    if(ses.eyeHeight <= 0.f)
    {
        ses.eyeHeight = units::eyeHeight();
    }
    return begin(todo, returnPage);
}

bool restart(int returnPage)
{
    if(ses.phase == Phase::Capturing)
    {
        return false;
    }
    setPreview(false);
    const bool seated = vr_bodycal_seated.value != 0.f;
    for(StepData& d : ses.data)
    {
        d = StepData{};
    }
    ses.seated = seated;
    ses.eyeHeight = units::eyeHeight();
    result = Result{};
    return begin(allSteps(seated), returnPage);
}

bool redo(int step, int returnPage)
{
    if(step < 0 || step >= StepCount || (ses.seated && step == Stand))
    {
        return false;
    }
    setPreview(false);
    if(ses.eyeHeight <= 0.f)
    {
        ses.eyeHeight = units::eyeHeight();
    }
    return begin({step}, returnPage);
}

void apply()
{
    if(ses.phase != Phase::Result || !result.valid)
    {
        return;
    }
    setPreview(false);
    const std::vector<Setting> list = candidate();
    saveUndo(snapshot(list));
    write(list);
    applied = true;
    ses.phase = Phase::Idle;
    S_LocalSound("misc/menu2.wav");
    Con_Printf("Body Calibration: applied (upper arm %.1f cm, forearm %.1f cm)\n", result.upper, result.fore);
    bump();
}

void cancel()
{
    if(ses.phase != Phase::Result)
    {
        return;
    }
    setPreview(false);
    ses.phase = Phase::Idle;
    result.valid = false;
    bump();
}

bool canUndo()
{
    return vr_bodycal_undo.string[0] != 0 && ses.phase != Phase::Capturing;
}

void undo()
{
    if(!canUndo())
    {
        return;
    }
    setPreview(false);
    std::istringstream in(vr_bodycal_undo.string);
    std::string item;
    while(std::getline(in, item, ';'))
    {
        std::istringstream w(item);
        std::string name, value;
        if(w >> name >> value)
        {
            Cvar_Set(name.c_str(), value.c_str());
        }
    }
    Cvar_SetQuick(&vr_bodycal_undo, "");
    applied = false;
    Con_Printf("Body Calibration: the settings from before it are back\n");
    bump();
}

bool partial()
{
    int taken = 0, all = 0;
    for(int i : allSteps(ses.seated))
    {
        all++;
        taken += ses.data[i].taken ? 1 : 0;
    }
    return taken > 0 && taken < all && ses.phase == Phase::Idle;
}

bool showingNew()
{
    return showNew;
}

void switchShown()
{
    showNew = !showNew;
    bump();
}

int version()
{
    return versionCounter;
}

const char* statusLine(int i)
{
    int n = 0;
    const auto add = [&](const std::string& s) {
        if(n < 24)
        {
            lineBuf[n++] = s;
        }
    };
    if(ses.phase == Phase::Capturing)
    {
        const int step = currentStep();
        add(va("Calibrating: %s", step >= 0 ? steps[step].title : "-"));
    }
    else if(result.valid && (ses.phase == Phase::Result || applied))
    {
        const Result& r = result;
        add(applied ? va("Applied (%s):", r.when.c_str()) : std::string("Measured:        now     new"));
        if(!r.seated)
        {
            add(va("Eye height    %6.2f  %6.2f m ", r.eyeHeightNow, r.eyeHeight));
        }
        add(va("Upper arm     %6.1f  %6.1f cm", r.upperNow, r.upper));
        add(va("Forearm       %6.1f  %6.1f cm", r.foreNow, r.fore));
        add(va("Reach         %6.1f  %6.1f cm", r.upperNow + r.foreNow, r.upper + r.fore));
        add(va("Shoulders:                     "));
        add(va(" below eyes   %6.1f  %6.1f cm", r.eyeToShoulderNow.y, r.eyeToShoulder.y));
        add(va(" behind eyes  %6.1f  %6.1f cm", r.eyeToShoulderNow.x, r.eyeToShoulder.x));
        add(va(" apart        %6.1f  %6.1f cm", 2.f * r.eyeToShoulderNow.z, 2.f * r.eyeToShoulder.z));
        add(va(" rise, swing %3.0f/%-3.0f %3.0f/%-3.0f deg", r.raiseNow, r.swingNow, r.raise, r.swing));
        add(va("Left, right reach %.1f, %.1f cm", r.reachSide[0], r.reachSide[1]));
        if(r.wristValid[0] && r.wristValid[1])
        {
            add(va("Drawn wrists %.1f, %.1f cm off", r.wristOff[0], r.wristOff[1]));
        }
        add(va("Fit: %.1f cm, %.0f%% of samples dropped", r.rms, 100.f * r.dropped));
        if(!applied)
        {
            add(va("Old arms reached %.1f cm stretched", (r.upperNow + r.foreNow) * r.stretchNow));
        }
    }
    else if(!ses.message.empty())
    {
        add("Stopped: " + ses.message);
    }
    return i >= 0 && i < n ? lineBuf[i].c_str() : nullptr;
}

int stepCount()
{
    return StepCount;
}

bool stepUsed(int i)
{
    return i >= 0 && i < StepCount && !(i == Stand && vr_bodycal_seated.value != 0.f);
}

const char* stepRow(int i)
{
    if(i < 0 || i >= StepCount)
    {
        return "";
    }
    const StepQuality& q = result.quality[i];
    std::string state;
    if(!ses.data[i].taken)
    {
        state = "not taken";
    }
    else if(!result.valid)
    {
        state = "taken";
    }
    else
    {
        const char* grade = q.grade == Good ? "good" : q.grade == Fair ? "fair" : q.grade == Redo ? "REDO" : "";
        state = va("%.1f cm %s", q.value, grade);
    }
    rowBuf[i] = va("Redo %d %-13s %s", i + 1, steps[i].title, state.c_str());
    return rowBuf[i].c_str();
}

const char* stepHelp(int i)
{
    if(i < 0 || i >= StepCount)
    {
        return "";
    }
    const StepQuality& q = result.quality[i];
    static std::string buf;
    buf = std::string(steps[i].help) + (q.note.empty() ? "" : std::string(" Last time: ") + q.note + ".") +
          " Takes this pose again; the others are kept.";
    return buf.c_str();
}

void frame()
{
    // The preview: the result's values on the body shown in front, while its page is open.
    const bool pageOpen = key_dest == key_menu && ses.returnPage >= 0 && menu::currentPage() == ses.returnPage;
    setPreview(ses.phase == Phase::Result && pageOpen && showNew);
    showBody(ses.phase == Phase::Result && pageOpen && vr_bodycal_preview.value > 0.f);

    if(ses.phase != Phase::Capturing)
    {
        return;
    }
    if(key_dest == key_menu || ses.world != worldGeneration() || cls.state != ca_connected || cl.intermission || !vrActive() ||
        cl.stats[STAT_HEALTH] <= 0)
    {
        S_LocalSound("misc/menu3.wav");
        stopCapture("stopped (the menu, the map or the game changed): Continue takes the rest");
        return;
    }
    const hands::State& s = hands::current();
    const int step = currentStep();
    if(!s.valid || step < 0)
    {
        return;
    }
    const double now = realtime;
    if(ses.sub == Sub::Read)
    {
        const bool first = ses.todo.size() == static_cast<size_t>(StepCount) ||
                           (ses.seated && ses.todo.size() == static_cast<size_t>(StepCount - 1));
        if(now - ses.subStart >= (first ? firstReadSeconds : readSeconds))
        {
            ses.sub = Sub::Countdown;
            ses.subStart = now;
            ses.countdown.start(now);
        }
    }
    else if(ses.sub == Sub::Countdown)
    {
        if(ses.countdown.update(now))
        {
            ses.sub = steps[step].kind == Kind::Static ? Sub::Wait : Sub::Record;
            ses.subStart = now;
            ses.yaw = s.headAngles.y; // the body faces where the head looks now ("look ahead")
            ses.window.clear();
            ses.recent.clear();
            ses.recording.clear();
            ses.hint.clear();
        }
    }
    else if(ses.sub == Sub::Wait && now - ses.subStart > giveUpSeconds)
    {
        S_LocalSound("misc/menu3.wav");
        Con_Printf("Body Calibration: %s not taken (%s)\n", steps[step].title, ses.hint.c_str());
        nextStep(now);
        return;
    }
    drawText(s, step, now);
    drawGhost(s, step, static_cast<float>(now - ses.subStart));
}

void viewFrame(const hands::State& s)
{
    if(debugNext)
    {
        debugNext = false;
        bool ok = false;
        const Raw r = rawOf(s, realtime, ok);
        const Local l = toLocal(r, s.headAngles.y, units::eyeHeight());
        for(int side = 0; side < 2; side++)
        {
            const glm::vec3 d = (r.wrist[side] - s.head) / units::metresToUnits();
            glm::vec3 hf, hr, hu;
            hands::angleVectors(glm::vec3{0.f, s.headAngles.y, 0.f}, hf, hr, hu);
            Con_Printf("bodycal debug %s: empty wrist (from the head: fwd %.4f left %.4f up %.4f) chest-local %.4f %.4f %.4f; "
                       "grip chest-local %.4f %.4f %.4f; jointed %d\n",
                side == 0 ? "L" : "R", glm::dot(d, hf), -glm::dot(d, hr), d.z, l.wrist[side].x, l.wrist[side].y, l.wrist[side].z,
                l.grip[side].x, l.grip[side].y, l.grip[side].z, ok ? 1 : 0);
        }
    }

    if(ses.phase != Phase::Capturing || host_framecount == ses.lastFrame)
    {
        return;
    }
    ses.lastFrame = host_framecount;
    const int step = currentStep();
    if(step < 0 || (ses.sub != Sub::Wait && ses.sub != Sub::Record))
    {
        return;
    }
    const double now = realtime;
    bool ok = false;
    const Raw r = rawOf(s, now, ok);
    if(!ok)
    {
        return;
    }

    if(ses.sub == Sub::Record)
    {
        ses.recording.push_back(r);
        const double t = now - ses.subStart;
        const double length = steps[step].seconds;
        if(t < length)
        {
            return;
        }
        // Enough of the move? (Checked twice a second past its length.)
        if(static_cast<int>(t * 2.0) == static_cast<int>((t - host_frametime) * 2.0) && t < length + extraSeconds)
        {
            return;
        }
        StepData d;
        d.taken = true;
        d.yaw = ses.yaw;
        d.raw = ses.recording;
        const Settings now0 = current(ses.eyeHeight);
        const float cov = coverage(step, localsOf(d, ses.eyeHeight), modelOf(now0));
        if(cov < coverageNeeded(step) && t < length + extraSeconds)
        {
            ses.hint = step == Circles ? "bigger circles" : step == Elbows ? "wave the forearms further" : "bend the wrists further";
            return;
        }
        ses.data[step] = std::move(d);
        ses.gotIt = now;
        S_LocalSound(cov >= coverageNeeded(step) ? "weapons/pkup.wav" : "misc/menu3.wav");
        Con_Printf("Body Calibration: %s recorded, %d frames, coverage %.1f (%.1f needed)\n", steps[step].title,
            static_cast<int>(ses.data[step].raw.size()), cov, coverageNeeded(step));
        nextStep(now);
        return;
    }

    // Wait: the pose held still.
    const float m2u = units::metresToUnits();
    ses.window.tolerance(0, stillHeadCm * 0.01f * m2u);
    ses.window.tolerance(1, stillWristCm * 0.01f * m2u);
    ses.window.tolerance(2, stillWristCm * 0.01f * m2u);
    const glm::vec3 channels[] = {r.head, r.wrist[0], r.wrist[1], hands::forward(glm::vec3{0.f, r.headAngles.y, 0.f})};
    ses.window.add(now, channels, stillSeconds + 0.25);
    ses.recent.push_back(r);
    while(!ses.recent.empty() && now - ses.recent.front().t > stillSeconds + 1e-3)
    {
        ses.recent.erase(ses.recent.begin());
    }
    std::vector<glm::vec3> mean;
    const bool still = ses.window.still(now, stillSeconds, mean);
    if(!still)
    {
        if(now - ses.subStart > hintAfter)
        {
            ses.hint = "hold still";
        }
        return;
    }
    Raw capture = average(ses.recent);
    // The body faces where the head looked while held (averaged).
    const glm::vec3 look = mean[3];
    ses.yaw = glm::degrees(std::atan2(look.y, look.x));
    const std::string wrong = wrongPose(step, capture);
    if(!wrong.empty())
    {
        if(now - ses.subStart > hintAfter)
        {
            ses.hint = wrong;
        }
        return;
    }
    StepData& d = ses.data[step];
    d = StepData{};
    d.taken = true;
    d.yaw = ses.yaw;
    d.raw.push_back(capture);
    if(step == Stand)
    {
        ses.eyeHeight = capture.headHeight; // the chest frames of the steps after it
    }
    ses.gotIt = now;
    S_LocalSound("weapons/pkup.wav");
    Con_Printf("Body Calibration: %s taken\n", steps[step].title);
    nextStep(now);
}

} // namespace qvr::bodycal
