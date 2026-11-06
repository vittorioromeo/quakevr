// vr_animrec_internal.hpp -- the animation recorder's data (docs/vr-port/ANIMATIONS.md): a recording in memory, its
// .anim file (vr_animrec_data.cpp: written, read), its smoothing (keyframes kept within a tolerance, a jitter filter)
// and its sampling between frames (a cubic Hermite spline). Misc/quakevr/anim_smooth.py does the same smoothing offline.
//
// Frames: positions in metres, rotations as quaternions here (Quake angles in the file: pitch, yaw, roll, as
// hands::basisOf takes them). Axes of every frame: forward, left, up.
// - "head": relative to the head (its eye position and its full turn);
// - "body": relative to the body: hands::bodyAnchor(s, 0) (under the head, lowered as he crouches) turned by the body's
//   yaw alone (no pitch: a look straight up or down does not move it).

#pragma once

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <glm/gtc/quaternion.hpp>


namespace qvr::animrec
{

inline constexpr int fingerCount = 5;
inline constexpr int handCount = 2; // HAND_OFF 0, HAND_MAIN 1

struct Pose
{
    glm::vec3 pos{0.f};
    glm::quat rot{1.f, 0.f, 0.f, 0.f};
};

struct HandFrame
{
    Pose head;                     // the hand relative to the head
    Pose body;                     // and relative to the body
    float curl[fingerCount]{};     // thumb .. little, 0 open .. 1 curled (view::fingerCurl: as drawn)
    float grip{0.f};               // the controller's grip and trigger, 0..1
    float trigger{0.f};
    int wid{0};                    // the held weapon's id (STAT_QVR_WEAPON / 2; QC WID_*), 0: none
};

struct Frame
{
    double t{0.0};    // seconds from the start
    Pose head;        // the head relative to the body
    float crouch{0.f}; // 0 standing .. 1 crouched
    HandFrame hands[handCount];
};

// What happened, when: a shot, a pump, a reload's step (magazine out and in, a round taken from the pouch, ...).
// `spot`: the recorder's own hotspot the hand was nearest (Spot), and the hand's place from it (body axes, metres):
// so that a playback can take this player's own hotspot instead (his pouch may be elsewhere) or do the step at `t`.
struct Event
{
    double t{0.0};
    int hand{-1}; // 0 off, 1 main, -1 both or neither
    za::String kind;
    za::String spot;          // "-": none near
    glm::vec3 fromSpot{0.f};  // the hand's place from that spot
    glm::vec3 body{0.f};      // the hand's place relative to the body
    za::String detail;
};

// A weapon taken into a hand (at the start: each hand's).
struct WeaponChange
{
    double t{0.0};
    int hand{0};
    int wid{0};
    za::String model; // its model ("-": none)
};

// The recorder's hotspots at the start (body axes, metres): the ammo and grenade pouches, the holsters.
struct Spot
{
    za::String name;
    glm::vec3 pos{0.f};
};

struct Anim
{
    za::String name;
    za::String date;
    za::String map;
    za::String source;
    za::String note;
    int version{1};
    int rawFrames{0};       // the recording's frames (a smoothed file keeps fewer)
    bool smoothed{false};
    za::String smoothInfo;  // the smoothing's parameters and its errors (a smoothed file's)
    za::Vector<Spot> spots;
    za::Vector<WeaponChange> weapons;
    za::Vector<Event> events;
    za::Vector<Frame> frames;

    [[nodiscard]] double duration() const { return frames.empty() ? 0.0 : frames.back().t; }
    // The weapon `hand` holds at `t` (the last change at or before it): its model ("" none) and id.
    [[nodiscard]] const WeaponChange* weaponAt(int hand, double t) const;
};

// The file. write: false if it could not be written. read: false with `error` said.
[[nodiscard]] za::String toText(const Anim& a);
[[nodiscard]] bool fromText(za::StringView text, Anim& a, za::String& error);

// Quake angles <-> quaternions (hands::basisOf: forward, left, up).
[[nodiscard]] glm::quat quatOf(const glm::vec3& angles);
[[nodiscard]] glm::vec3 anglesOf(const glm::quat& q);

// The pose at `t` (clamped to the recording): a cubic Hermite spline through the frames (Catmull-Rom tangents over the
// frames' times; the rotations' quaternions likewise, kept on one side and normalised); the weapon ids of the frame
// before.
[[nodiscard]] Frame sample(const Anim& a, double t);

struct SmoothParams
{
    float tau{0.02f};      // s: the jitter filter's time constant (forward and backward: no lag); 0: none
    float posTol{0.004f};  // m: the most a kept-keys spline may stray from the filtered motion
    float angTol{1.5f};    // degrees, likewise for the rotations
    float curlTol{0.04f};  // the fingers, grip and trigger (0..1)
};

struct SmoothResult
{
    int before{0};
    int after{0};
    float maxPos{0.f};   // m: the spline through the keys against the filtered motion, at every frame
    float maxAng{0.f};   // degrees
    float maxCurl{0.f};
    float rawPos{0.f};   // m: the filtered motion against the raw one (what the filter took out: the jitter)
};

// `in` filtered and reduced to the keys a spline needs to stay within the tolerances; the events' frames, the weapon
// changes and the ends are always kept.
[[nodiscard]] SmoothResult smooth(const Anim& in, const SmoothParams& p, Anim& out);

} // namespace qvr::animrec
