// vr_bodytrack.cpp -- the tracked body (vr_bodytrack.hpp; ROUND22.md, "Body tracking"): a source's joints smoothed and
// placed in the world, the regions' blend weights, what the avatar, the torso direction and the lean detection take from
// it, the debug skeleton, vr_bodytrack_status and vr_bodytrack_stats, and the mock's synthetic body.
//
// Everything the body takes is scale-free (directions and angles) and laid on the avatar's own bones, so the player's
// body calibration (heights, arm lengths, the hands' calibration) keeps the avatar's proportions: the spine's tilt from
// the hips to the neck, the chest's twist from the hips (the shoulder line against the hip line), the hips' facing, the
// elbow's side of the shoulder-to-wrist line, the knee's side of the hip-to-ankle line and the ankle from the hip as a
// share of the leg's length.

#include "vr_bodytrack.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_lines.hpp"
#include "vr_profile.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/InitializerList.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Sqrt.hpp"

namespace qvr::bodytrack
{

namespace
{

constexpr glm::vec3 UP{0.f, 0.f, 1.f};
constexpr double SUBMIT_STALE = 0.25; // seconds (the wall clock) without a frame from the source: it stopped

BackendStatus backend;

// The source's last frame, and the joints smoothed (the tracking space) and placed (the world).
struct State
{
    Raw raw{};
    double submitted{-1.0}; // Sys_DoubleTime of the last submit (< 0: never)
    glm::vec3 smooth[JointCount]{};
    bool have[JointCount]{};
    glm::vec3 world[JointCount]{};
    bool placed{false};
    float m2u{1.f};
    double lastTime{-1.0}; // vr_gametime of the last place()
    float torso{0.f};      // the regions' weights now
    float arm[2]{};
    float leg[2]{};
    float legApplied[2]{};    // the legs' weight leg() last laid on (less climbing, swimming and the walk)
    double placeSeconds{0.0}; // place()'s and the synthetic body's time (vr_bodytrack_stats)
    int placeCalls{0};
    glm::vec3 placeBase{0.f}, placeFloor{0.f}; // place()'s mapping, the last (the synthetic body's way back)
    float placeYaw{0.f};
    glm::vec3 posedWrist[2]{}; // the avatar's wrists last posed (world; the synthetic body's wrists)
    bool havePosedWrist{false};
};
State st;

// vr_bodytrack_stats: each posed frame's errors against the tracked body, and the largest jump.
struct Stats
{
    int frames{0};
    double lean{0.0}, leanMax{0.0};         // degrees: the avatar's spine against the tracked tilt laid on it
    double tiltAvatar{0.0}, tiltTracked{0.0}; // degrees from upright
    double twist{0.0}, twistMax{0.0};       // degrees: the avatar's shoulder line against the tracked one
    double elbow[2]{}, elbowMax[2]{};       // degrees: the elbows' swivel about the shoulder-to-wrist line
    int elbowFrames[2]{};
    double elbowCm[2]{}, elbowCmMax[2]{};   // cm: the elbows from the tracked ones
    int elbowCmFrames[2]{};
    double knee[2]{};
    int kneeFrames[2]{};
    double hold{0.0};                       // the lean detection's hold
    double jumpMax{0.0};                    // cm a joint moved against the top of the neck in one frame, at most
    double weightTorso{0.0}, weightArm{0.0}, weightLeg{0.0}, legApplied{0.0};
    glm::vec3 last[10]{};
    bool haveLast{false};
};
Stats stats;
int debugFrame = -1;

[[nodiscard]] bool valid(int j)
{
    return (st.raw.flags[j] & POS_VALID) != 0 && st.have[j];
}

[[nodiscard]] glm::vec3 quakeFromTracking(const glm::vec3& v)
{
    return {-v.z, -v.x, v.y};
}

[[nodiscard]] glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback)
{
    const float l = glm::length(v);
    return l > 1e-5f ? v / l : fallback;
}

[[nodiscard]] float wrapYaw(float a)
{
    return za::remainder(a, 360.f);
}

[[nodiscard]] float yawOfLine(const glm::vec3& left)
{
    const glm::vec3 fwd = glm::cross(left, UP); // +x forward, +y left: forward is left x up
    return glm::degrees(za::atan2(fwd.y, fwd.x));
}

[[nodiscard]] float smooth01(float e0, float e1, float x)
{
    const float u = za::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
    return u * u * (3.f - 2.f * u);
}

// `v` without its part along unit `axis`.
[[nodiscard]] glm::vec3 across(const glm::vec3& v, const glm::vec3& axis)
{
    return v - axis * glm::dot(v, axis);
}

// The signed angle (radians) from `a` to `b` about unit `axis` (both across it).
[[nodiscard]] float angleAbout(const glm::vec3& a, const glm::vec3& b, const glm::vec3& axis)
{
    return za::atan2(glm::dot(glm::cross(a, b), axis), glm::dot(a, b));
}

[[nodiscard]] float angleBetween(const glm::vec3& a, const glm::vec3& b)
{
    return glm::degrees(za::acos(za::clamp(glm::dot(safeNormalize(a, UP), safeNormalize(b, UP)), -1.f, 1.f)));
}

// The tilt that takes the vertical to unit `dir` (about the horizontal axis across both).
[[nodiscard]] glm::quat tiltFromUp(const glm::vec3& dir)
{
    const glm::vec3 axis = glm::cross(UP, dir);
    const float s = glm::length(axis);
    if(s < 1e-6f)
    {
        return glm::quat{1.f, 0.f, 0.f, 0.f};
    }
    return glm::angleAxis(za::atan2(s, glm::dot(UP, dir)), axis / s);
}

constexpr int armUpper(int side) { return side == 0 ? ArmUpperL : ArmUpperR; }
constexpr int armLower(int side) { return side == 0 ? ArmLowerL : ArmLowerR; }
constexpr int wrist(int side) { return side == 0 ? WristL : WristR; }
constexpr int upperLeg(int side) { return side == 0 ? UpperLegL : UpperLegR; }
constexpr int lowerLeg(int side) { return side == 0 ? LowerLegL : LowerLegR; }
constexpr int ankle(int side) { return side == 0 ? AnkleL : AnkleR; }

[[nodiscard]] float ease(float now, float target, float dt)
{
    const float fade = vr_bodytrack_fade.value;
    float w = fade > 0.f ? now + (target - now) * (1.f - za::exp(-dt / fade)) : target;
    if(target <= 0.f && w < 1e-3f)
    {
        w = 0.f;
    }
    if(target >= 1.f && w > 1.f - 1e-4f)
    {
        w = 1.f;
    }
    return w;
}

// The spine (hips to neck) in the world, and its yaws.
[[nodiscard]] glm::vec3 spine()
{
    return st.world[Neck] - st.world[Hips];
}

[[nodiscard]] float chestYaw()
{
    return yawOfLine(st.world[ArmUpperL] - st.world[ArmUpperR]);
}

[[nodiscard]] float hipsYaw()
{
    return yawOfLine(st.world[UpperLegL] - st.world[UpperLegR]);
}

// Two bones from `root` to `target` (lengths a, b), the middle joint bent towards `pole`.
[[nodiscard]] glm::vec3 twoBone(const glm::vec3& root, const glm::vec3& target, float a, float b, const glm::vec3& pole)
{
    const glm::vec3 to = target - root;
    const float d = za::clamp(glm::length(to), 1e-3f, a + b - 1e-4f);
    const glm::vec3 dir = safeNormalize(to, {0.f, -1.f, 0.f});
    const float cosA = za::clamp((a * a + d * d - b * b) / (2.f * a * d), -1.f, 1.f);
    const float sinA = za::sqrt(za::max(0.f, 1.f - cosA * cosA));
    const glm::vec3 bend = safeNormalize(across(pole, dir), {0.f, 0.f, -1.f});
    return root + dir * (a * cosA) + bend * (a * sinA);
}

// vr_bodytrack_debug_bones 1 or 2 (with vr_bodytrack_debug on): our IK's bones drawn, the avatar untracked.
[[nodiscard]] bool debugUntracked()
{
    return vr_bodytrack_debug.value != 0.f && vr_bodytrack_debug_bones.value >= 1.f;
}

// Our IK's bones (the avatar as posed): orange, the spine from the pelvis to the top of the neck.
void drawOurs(const Posed& p, const glm::vec3& offset)
{
    constexpr glm::vec4 ORANGE{1.f, 0.6f, 0.1f, 0.9f};
    const auto bone = [&](const glm::vec3& a, const glm::vec3& b) { lines::line(a + offset, b + offset, 0.3f, ORANGE, ORANGE); };
    bone(p.pelvis, p.top);
    lines::point(p.pelvis + offset, 1.f, ORANGE);
    lines::point(p.top + offset, 1.f, ORANGE);
    for(int side = 0; side < 2; side++)
    {
        bone(p.top, p.shoulder[side]);
        bone(p.shoulder[side], p.elbow[side]);
        bone(p.elbow[side], p.wrist[side]);
        bone(p.pelvis, p.hip[side]);
        bone(p.hip[side], p.knee[side]);
        bone(p.knee[side], p.ankle[side]);
        for(const glm::vec3& j : {p.elbow[side], p.wrist[side], p.knee[side], p.ankle[side]})
        {
            lines::point(j + offset, 1.f, ORANGE);
        }
    }
}

void drawSkeleton(const glm::vec3& offset)
{
    constexpr int bones[][2] = {{Hips, SpineLower}, {SpineLower, SpineMiddle}, {SpineMiddle, SpineUpper}, {SpineUpper, Chest},
        {Chest, Neck}, {Neck, Head}, {Chest, ArmUpperL}, {ArmUpperL, ArmLowerL}, {ArmLowerL, WristL}, {Chest, ArmUpperR},
        {ArmUpperR, ArmLowerR}, {ArmLowerR, WristR}, {Hips, UpperLegL}, {UpperLegL, LowerLegL}, {LowerLegL, AnkleL},
        {AnkleL, BallL}, {Hips, UpperLegR}, {UpperLegR, LowerLegR}, {LowerLegR, AnkleR}, {AnkleR, BallR}};
    // Tracked (the camera saw it): cyan; valid only (inferred, the generated legs): magenta.
    const auto colour = [](int j) {
        return (st.raw.flags[j] & POS_TRACKED) ? glm::vec4{0.2f, 1.f, 1.f, 0.9f} : glm::vec4{1.f, 0.3f, 1.f, 0.9f};
    };
    for(const auto& b : bones)
    {
        if(valid(b[0]) && valid(b[1]))
        {
            lines::line(st.world[b[0]] + offset, st.world[b[1]] + offset, 0.3f, colour(b[0]), colour(b[1]));
        }
    }
    for(int j = 0; j < JointCount; j++)
    {
        if(valid(j) && (j <= Head || j == ArmLowerL || j == ArmLowerR || j == WristL || j == WristR || j == LowerLegL ||
                           j == LowerLegR || j == AnkleL || j == AnkleR))
        {
            lines::point(st.world[j] + offset, 1.f, colour(j));
        }
    }
}

void status_f()
{
    const BackendStatus& b = backend;
    const Raw& r = st.raw;
    int valids = 0, tracked = 0;
    for(int j = 0; j < JointCount; j++)
    {
        valids += (r.flags[j] & POS_VALID) ? 1 : 0;
        tracked += (r.flags[j] & POS_TRACKED) ? 1 : 0;
    }
    static constexpr const char* fidelity[] = {"unknown", "low", "high"};
    static constexpr const char* calibration[] = {"unknown", "valid", "calibrating", "invalid"};
    const bool fresh = st.submitted >= 0.0 && Sys_DoubleTime() - st.submitted < SUBMIT_STALE;
    Con_Printf("bodytrack: extensions fb %d fullbody %d fidelity %d calibration %d; system %d; tracker %d (%s)%s%s\n",
        b.fbExtension, b.fullBodyExtension, b.fidelityExtension, b.calibrationExtension, b.systemSupports, b.trackerCreated,
        b.fullBodySet ? "full body" : "upper body", b.highFidelityRequested ? ", high fidelity asked" : "",
        vr_bodytrack_synth.value != 0.f ? "; synthetic body (mock)" : "");
    Con_Printf("bodytrack: source %s, active %d, confidence %.2f, fidelity %s, calibration %s, joints %d valid %d tracked\n",
        fresh ? "live" : "none", r.active, r.confidence, fidelity[za::clamp(r.fidelity, 0, 2)],
        calibration[za::clamp(r.calibration, 0, 3)], valids, tracked);
    Con_Printf("bodytrack: weights torso %.2f arms %.2f %.2f legs %.2f %.2f; %.1f us a frame%s%s\n", st.torso, st.arm[0],
        st.arm[1], st.leg[0], st.leg[1], st.placeCalls > 0 ? st.placeSeconds * 1e6 / st.placeCalls : 0.0,
        b.error[0] ? "; last error " : "", b.error);
}

void stats_f()
{
    if(Cmd_Argc() >= 2 && !strcmp(Cmd_Argv(1), "reset"))
    {
        stats = Stats{};
        st.placeSeconds = 0.0;
        st.placeCalls = 0;
        return;
    }
    const char* label = Cmd_Argc() >= 2 ? Cmd_Argv(1) : "";
    const Stats& s = stats;
    const double n = za::max(1, s.frames);
    Con_Printf("btstats %s: frames %d w %.2f/%.2f/%.2f lean err %.2f max %.2f deg (avatar tilt %.1f, tracked %.1f) twist err "
               "%.1f max %.1f\n",
        label, s.frames, s.weightTorso / n, s.weightArm / n, s.weightLeg / n, s.lean / n, s.leanMax, s.tiltAvatar / n,
        s.tiltTracked / n, s.twist / n, s.twistMax);
    Con_Printf("btstats %s: elbow err L %.1f (max %.1f) R %.1f (max %.1f) knee err L %.1f R %.1f legs applied %.2f hold %.2f "
               "jump max %.2f cm; %.1f us a frame\n",
        label, s.elbow[0] / za::max(1, s.elbowFrames[0]), s.elbowMax[0], s.elbow[1] / za::max(1, s.elbowFrames[1]),
        s.elbowMax[1], s.knee[0] / za::max(1, s.kneeFrames[0]), s.knee[1] / za::max(1, s.kneeFrames[1]), s.legApplied / n,
        s.hold / n, s.jumpMax, st.placeCalls > 0 ? st.placeSeconds * 1e6 / st.placeCalls : 0.0);
    Con_Printf("btstats %s: elbow off L %.2f (max %.2f) R %.2f (max %.2f) cm from the tracked\n", label,
        s.elbowCm[0] / za::max(1, s.elbowCmFrames[0]), s.elbowCmMax[0], s.elbowCm[1] / za::max(1, s.elbowCmFrames[1]),
        s.elbowCmMax[1]);
}

} // namespace

BackendStatus& backendStatus()
{
    return backend;
}

void submit(const Raw& raw)
{
    st.raw = raw;
    st.submitted = Sys_DoubleTime();
}

void place(const glm::vec3& base, const glm::vec3& floorBelowHead, float turnYaw, float m2u)
{
    if(st.submitted < 0.0)
    {
        return; // never a source: as today
    }
    QVR_PROFILE("bodytrack");
    const double t0 = Sys_DoubleTime();
    const bool fresh = t0 - st.submitted < SUBMIT_STALE;
    const double dtRaw = st.lastTime >= 0.0 ? vr_gametime - st.lastTime : 0.0;
    const float dt = static_cast<float>(za::clamp(dtRaw, 0.0, 0.1));
    st.lastTime = vr_gametime;
    st.m2u = m2u;
    st.placeBase = base;
    st.placeFloor = floorBelowHead;
    st.placeYaw = turnYaw;

    // Smoothed in the room (the play space's turns and the locomotion move it all at once, unsmoothed).
    const float tau = vr_bodytrack_smooth.value;
    const float a = tau > 0.f ? 1.f - za::exp(-dt / tau) : 1.f;
    for(int j = 0; j < JointCount; j++)
    {
        if(fresh && (st.raw.flags[j] & POS_VALID))
        {
            st.smooth[j] = st.have[j] ? glm::mix(st.smooth[j], st.raw.pos[j], a) : st.raw.pos[j];
            st.have[j] = true;
        }
        if(st.have[j])
        {
            st.world[j] = base + hands::rotateYaw(quakeFromTracking(st.smooth[j] - floorBelowHead) * m2u, turnYaw);
        }
    }
    st.placed = true;

    // The regions: in while the tracking is good (on, active, confident, the region's joints valid), eased out otherwise
    // (the last joints held meanwhile).
    const Raw& r = st.raw;
    // (Debug: drawing our IK's untracked bones, the avatar ignores the tracking, which is still read and drawn.)
    const bool good = fresh && vr_bodytrack.value != 0.f && r.active && r.confidence >= vr_bodytrack_confidence.value &&
                      !debugUntracked();
    const auto all = [&](za::InitializerList<int> joints) {
        for(int j : joints)
        {
            if(!(r.flags[j] & POS_VALID) || !st.have[j])
            {
                return false;
            }
        }
        return true;
    };
    const float torsoTarget = good && all({Hips, Neck, ArmUpperL, ArmUpperR, UpperLegL, UpperLegR})
                                  ? za::clamp(vr_bodytrack_torso.value, 0.f, 1.f)
                                  : 0.f;
    st.torso = ease(st.torso, torsoTarget, dt);
    for(int side = 0; side < 2; side++)
    {
        const float armTarget = good && all({armUpper(side), armLower(side), wrist(side)}) ? za::clamp(vr_bodytrack_arms.value, 0.f, 1.f) : 0.f;
        st.arm[side] = ease(st.arm[side], armTarget, dt);
        const float legTarget =
            good && all({upperLeg(side), lowerLeg(side), ankle(side)}) ? za::clamp(vr_bodytrack_legs.value, 0.f, 1.f) : 0.f;
        st.leg[side] = ease(st.leg[side], legTarget, dt);
    }
    st.placeSeconds += Sys_DoubleTime() - t0;
    st.placeCalls++;
}

float torsoWeight()
{
    return st.torso;
}

float armWeight(int side)
{
    return st.arm[side & 1];
}

float legWeight(int side)
{
    return st.leg[side & 1];
}

Trunk trunk()
{
    Trunk t;
    if(st.torso <= 0.f)
    {
        return t;
    }
    const glm::vec3 s = spine();
    const float len = glm::length(s);
    if(len < 1e-3f)
    {
        return t;
    }
    t.weight = st.torso;
    t.tilt = za::acos(za::clamp(s.z / len, -1.f, 1.f));
    const glm::vec3 h{s.x, s.y, 0.f};
    t.dir = glm::length(h) > 1e-4f ? glm::normalize(h) : glm::vec3{0.f};
    t.hipsYaw = hipsYaw();
    t.twist = wrapYaw(chestYaw() - t.hipsYaw);
    return t;
}

float bodyYaw(float estimated)
{
    if(st.torso <= 0.f)
    {
        return estimated;
    }
    return estimated + st.torso * wrapYaw(hipsYaw() - estimated);
}

float leanSure(float estimated, const glm::vec2& offMetres)
{
    if(st.torso <= 0.f)
    {
        return estimated;
    }
    // How far the spine's top went the head's way, as a share of the head's offset: a lean about 0.7 (the head is
    // further up the same tilt), a step or a neck bend about none. The head about over the box's middle: whether the
    // spine is tilted at all (3 to 10 cm off over the hips).
    const glm::vec3 s = spine() / za::max(st.m2u, 1e-3f);
    const glm::vec2 sh{s.x, s.y};
    const float offLen = glm::length(offMetres);
    const float went = offLen > 1e-3f ? glm::dot(sh, offMetres / offLen) / offLen : 0.f;
    const float offWeight = smooth01(0.01f, 0.03f, offLen);
    const float tracked = glm::mix(smooth01(0.03f, 0.1f, glm::length(sh)), smooth01(0.15f, 0.5f, went), offWeight);
    return estimated + (tracked - estimated) * st.torso;
}

void swivelElbow(int side, const glm::vec3& shoulder, const glm::vec3& wristPos, glm::vec3& elbow, glm::vec3& bend)
{
    const float w = st.arm[side & 1];
    if(w <= 0.f)
    {
        return;
    }
    const glm::vec3 ts = st.world[armUpper(side)], te = st.world[armLower(side)], tw = st.world[wrist(side)];
    const glm::vec3 tAxis = safeNormalize(tw - ts, -UP);
    const glm::vec3 tOut = across(te - ts, tAxis);
    const glm::vec3 axis = safeNormalize(wristPos - shoulder, -UP);
    const glm::vec3 want = across(tOut, axis);
    const glm::vec3 now = across(elbow - shoulder, axis);
    // A straight tracked arm says nothing of its swivel: none taken (2 to 6 cm off the line, eased in).
    const float sure = smooth01(0.02f, 0.06f, glm::length(tOut) / za::max(st.m2u, 1e-3f));
    if(sure <= 0.f || glm::length(want) < 1e-4f || glm::length(now) < 1e-4f)
    {
        return;
    }
    const float angle = angleAbout(now, want, axis) * w * sure;
    const glm::quat q = glm::angleAxis(angle, axis);
    const glm::vec3 centre = shoulder + axis * glm::dot(elbow - shoulder, axis);
    elbow = centre + q * (elbow - centre);
    bend = q * bend;
}

bool trackedArm(int side, glm::vec3& shoulder, glm::vec3& elbow, float& weight)
{
    weight = st.arm[side & 1] * za::clamp(vr_bodytrack_arm_fit.value, 0.f, 1.f);
    if(weight <= 0.f)
    {
        weight = 0.f;
        return false;
    }
    shoulder = st.world[armUpper(side)];
    elbow = st.world[armLower(side)];
    return true;
}

void leg(int side, const glm::vec3& hip, float reach, float floorZ, float keep, glm::vec3& target, glm::vec3& kneeDir)
{
    const float w = st.leg[side & 1] * za::clamp(keep, 0.f, 1.f);
    st.legApplied[side & 1] = w;
    if(w <= 0.f)
    {
        return;
    }
    const glm::vec3 th = st.world[upperLeg(side)], tk = st.world[lowerLeg(side)], ta = st.world[ankle(side)];
    const float tracked = glm::distance(th, tk) + glm::distance(tk, ta);
    if(tracked < 1e-3f)
    {
        return;
    }
    glm::vec3 foot = hip + (ta - th) * (reach / tracked);
    foot.z = za::max(foot.z, floorZ);
    target = glm::mix(target, foot, w);
    const glm::vec3 out = across(tk - th, safeNormalize(ta - th, -UP));
    if(glm::length(out) > 1e-3f)
    {
        kneeDir = safeNormalize(glm::mix(kneeDir, glm::normalize(out), w), kneeDir);
    }
}

void posed(const Posed& p)
{
    if(!st.placed)
    {
        return;
    }
    st.posedWrist[0] = p.wrist[0];
    st.posedWrist[1] = p.wrist[1];
    st.havePosedWrist = true;
    if(vr_bodytrack_debug.value != 0.f && debugFrame != host_framecount)
    {
        debugFrame = host_framecount;
        const glm::vec3 right{p.fwd.y, -p.fwd.x, 0.f};
        const glm::vec3 offset = vr_bodytrack_debug.value >= 2.f ? right * (0.6f * st.m2u) : glm::vec3{0.f};
        const int bones = static_cast<int>(vr_bodytrack_debug_bones.value);
        if(bones != 1)
        {
            drawSkeleton(offset); // the tracked
        }
        if(bones >= 1)
        {
            drawOurs(p, offset); // our IK's, untracked
        }
    }

    // The largest jump of a joint against the top of the neck (the head's own motion taken out), the tracking valid or
    // not (its fallback must not pop).
    Stats& s = stats;
    const glm::vec3 joints[9] = {p.pelvis, p.elbow[0], p.elbow[1], p.knee[0], p.knee[1], p.ankle[0], p.ankle[1], p.shoulder[0],
        p.shoulder[1]};
    for(int i = 0; i < 9; i++)
    {
        const glm::vec3 rel = joints[i] - p.top;
        if(s.haveLast)
        {
            s.jumpMax = za::max(s.jumpMax, static_cast<double>(glm::distance(rel, s.last[i])) * 100.0 / st.m2u);
        }
        s.last[i] = rel;
    }
    s.haveLast = true;

    // The errors: against the tracked body (the synthetic one's is exact).
    if(!valid(Hips) || !valid(Neck))
    {
        return;
    }
    s.frames++;
    s.weightTorso += st.torso;
    s.weightArm += 0.5 * (st.arm[0] + st.arm[1]);
    s.weightLeg += 0.5 * (st.leg[0] + st.leg[1]);
    s.legApplied += 0.5 * (st.legApplied[0] + st.legApplied[1]);
    const glm::vec3 avatar = safeNormalize(p.top - p.pelvis, UP);
    const glm::vec3 tracked = safeNormalize(spine(), UP);
    const glm::vec3 expected = tiltFromUp(tracked) * glm::normalize(p.neutral); // the tracked tilt on the avatar's spine
    const double lean = angleBetween(avatar, expected);
    s.lean += lean;
    s.leanMax = za::max(s.leanMax, lean);
    s.tiltAvatar += angleBetween(avatar, p.neutral);
    s.tiltTracked += angleBetween(tracked, UP);
    s.hold += p.leanHold;
    if(valid(ArmUpperL) && valid(ArmUpperR))
    {
        const double twist = za::fabs(wrapYaw(yawOfLine(p.shoulder[0] - p.shoulder[1]) - chestYaw()));
        s.twist += twist;
        s.twistMax = za::max(s.twistMax, twist);
    }
    for(int side = 0; side < 2; side++)
    {
        if(valid(armUpper(side)) && valid(armLower(side)) && valid(wrist(side)))
        {
            const glm::vec3 ts = st.world[armUpper(side)];
            const glm::vec3 tOut = across(st.world[armLower(side)] - ts, safeNormalize(st.world[wrist(side)] - ts, -UP));
            const glm::vec3 axis = safeNormalize(p.wrist[side] - p.shoulder[side], -UP);
            const glm::vec3 a = across(p.elbow[side] - p.shoulder[side], axis), b = across(tOut, axis);
            if(glm::length(tOut) > 0.04f * st.m2u && glm::length(a) > 1e-3f && glm::length(b) > 1e-3f)
            {
                const double e = za::fabs(glm::degrees(angleAbout(a, b, axis)));
                s.elbow[side] += e;
                s.elbowMax[side] = za::max(s.elbowMax[side], e);
                s.elbowFrames[side]++;
            }
            const double cm = glm::distance(p.elbow[side], st.world[armLower(side)]) / za::max(st.m2u, 1e-3f) * 100.0;
            s.elbowCm[side] += cm;
            s.elbowCmMax[side] = za::max(s.elbowCmMax[side], cm);
            s.elbowCmFrames[side]++;
        }
        if(valid(upperLeg(side)) && valid(lowerLeg(side)) && valid(ankle(side)))
        {
            const glm::vec3 th = st.world[upperLeg(side)];
            const glm::vec3 tOut = across(st.world[lowerLeg(side)] - th, safeNormalize(st.world[ankle(side)] - th, -UP));
            const glm::vec3 axis = safeNormalize(p.ankle[side] - p.hip[side], -UP);
            const glm::vec3 a = across(p.knee[side] - p.hip[side], axis), b = across(tOut, axis);
            if(glm::length(tOut) > 0.02f * st.m2u && glm::length(a) > 1e-3f && glm::length(b) > 1e-3f)
            {
                s.knee[side] += za::fabs(glm::degrees(angleAbout(a, b, axis)));
                s.kneeFrames[side]++;
            }
        }
    }
}

void synthesize(TrackingState& t)
{
    if(vr_bodytrack_synth.value == 0.f)
    {
        return;
    }
    const double t0 = Sys_DoubleTime();
    // A body standing where the mock's head stands (its eyes at the head's height), facing -z: forward f, right r, up u.
    const glm::vec3 f{0.f, 0.f, -1.f}, r{1.f, 0.f, 0.f}, u{0.f, 1.f, 0.f};
    const float eye = za::max(1.f, t.head.position.y);
    const glm::vec3 floor{t.head.position.x, 0.f, t.head.position.z};
    const float scale = units::bodyScale();
    const float eyeFwd = vr_body_eye_forward.value * scale, eyeUp = vr_body_eye_up.value * scale;
    const float hipH = 0.52f * eye, neckLen = 0.06f * eye;
    const float spineLen = eye - hipH - neckLen - eyeUp; // the eyes at `eye` standing upright

    const glm::quat tilt = glm::angleAxis(glm::radians(vr_bodytrack_synth_roll.value), glm::cross(u, r)) *
                           glm::angleAxis(glm::radians(vr_bodytrack_synth_pitch.value), glm::cross(u, f));
    const glm::quat chest = tilt * glm::angleAxis(glm::radians(vr_bodytrack_synth_twist.value), u);
    const glm::quat head = chest * glm::angleAxis(glm::radians(vr_bodytrack_synth_neck.value), glm::cross(u, f));

    Raw raw;
    raw.active = vr_bodytrack_synth_active.value != 0.f;
    raw.confidence = vr_bodytrack_synth_confidence.value;
    raw.fidelity = 2;
    raw.calibration = 1;
    const auto set = [&](int j, const glm::vec3& p, bool trackedJoint) {
        raw.pos[j] = p;
        raw.flags[j] = raw.active ? POS_VALID | ROT_VALID | (trackedJoint ? POS_TRACKED | ROT_TRACKED : 0u) : 0u;
    };

    const glm::vec3 hips = floor + u * (hipH - za::max(0.f, vr_bodytrack_synth_crouch.value));
    const glm::vec3 up = tilt * u;
    set(Hips, hips, true);
    set(SpineLower, hips + up * (0.15f * spineLen), true);
    set(SpineMiddle, hips + up * (0.35f * spineLen), true);
    set(SpineUpper, hips + up * (0.6f * spineLen), true);
    set(Chest, hips + up * (0.8f * spineLen), true);
    const glm::vec3 neck = hips + up * spineLen;
    set(Neck, neck, true);
    const glm::vec3 top = neck + head * (u * neckLen);
    set(Head, top + head * (u * 0.05f * eye), true);
    const glm::vec3 eyes = top + head * (f * eyeFwd + u * eyeUp);
    t.head.position = eyes;
    t.head.orientation = head * t.head.orientation;

    // The arms: the shoulders on the chest, the wrists where the avatar's were last posed (a tracked wrist is the real
    // one, behind the controller; before the first pose, or without one, the controllers), the elbows bent towards the
    // pole.
    const float out = za::clamp(vr_bodytrack_synth_elbows.value, 0.f, 1.f);
    for(int side = 0; side < 2; side++)
    {
        const float sgn = side == 0 ? -1.f : 1.f;
        const glm::vec3 shoulder = hips + up * (0.8f * spineLen) + chest * (r * (sgn * 0.11f * eye) + u * (0.03f * eye));
        set(armUpper(side), shoulder, true);
        const Pose& hand = t.hands[side == 0 ? HAND_OFF : HAND_MAIN];
        glm::vec3 w = hand.valid ? hand.position : shoulder - u * (0.3f * eye);
        if(hand.valid && st.havePosedWrist && st.m2u > 0.f)
        {
            const glm::vec3 q = hands::rotateYaw((st.posedWrist[side] - st.placeBase) / st.m2u, -st.placeYaw);
            w = glm::vec3{-q.y, q.z, -q.x} + st.placeFloor; // (quakeFromTracking undone)
        }
        set(wrist(side), w, true);
        const glm::vec3 pole = glm::mix(-u - f * 0.3f, chest * (r * sgn) - u * 0.2f, out);
        set(armLower(side), twoBone(shoulder, w, 0.175f * eye, 0.16f * eye, pole), true);

        // The legs (generated: valid, not tracked): the feet under the hips, the knees forward, turned out.
        const glm::vec3 hip = hips + r * (sgn * 0.055f * eye) - u * (0.03f * eye);
        const glm::vec3 ankleAt = floor + r * (sgn * 0.07f * eye) + u * (0.045f * eye);
        const glm::vec3 kneePole = glm::angleAxis(glm::radians(-sgn * vr_bodytrack_synth_knees.value), u) * f;
        set(upperLeg(side), hip, false);
        set(lowerLeg(side), twoBone(hip, ankleAt, 0.245f * eye, 0.245f * eye, kneePole), false);
        set(ankle(side), ankleAt, false);
        set(side == 0 ? BallL : BallR, ankleAt + f * (0.08f * eye) - u * (0.035f * eye), false);
    }
    submit(raw);
    st.placeSeconds += Sys_DoubleTime() - t0;
}

void registerCommands()
{
    Cmd_AddCommand("vr_bodytrack_status", status_f);
    Cmd_AddCommand("vr_bodytrack_stats", stats_f);
}

} // namespace qvr::bodytrack
