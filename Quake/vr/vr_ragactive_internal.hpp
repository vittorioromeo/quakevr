#pragma once

// vr_ragactive_internal.hpp -- what vr_ragactive.cpp (active ragdolls; see vr_ragactive.hpp) shares with its parts:
// vr_ragactive_swim.cpp (liquids: swimming, flailing in lava or slime), vr_ragactive_hold.cpp (grabbing others,
// climbing a ledge) and vr_ragactive_cut.cpp (dismemberment: stumps held, balance on one leg). Internal: included by
// those four alone. What was the file's anonymous namespace and more than one of them uses is namespace
// qvr::ragactive::detail: its constants here, its functions and state declared here and defined where they stood.

#include "vr_ragactive.hpp"

#include "vr_b3glm.hpp"
#include "vr_cvars.hpp"
#include "vr_geom.hpp"
#include "vr_lines.hpp"
#include "vr_ragpose.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <cstring>

#include "vr_ledges.hpp"

namespace qvr::ragactive
{

namespace detail
{

using b3glm::b3v;
using b3glm::fromB3;
using b3glm::glmv;
using b3glm::toB3;
using geom::smooth01;

constexpr float stumbleTau = 0.4f;   // s: a stumble's carried motion dies away so

constexpr float handThick = 2.f;     // units: a hand on a wound, off its surface

constexpr float liquidWindow = 6.f;   // s since the death: a body falling in later is a corpse, it doesn't swim

constexpr float twoPi = 6.2831853f;

// The parts the swim looks at besides the pelvis: the chest and the head (-1: none, or cut off).
struct SwimParts
{
    int chest{-1}, head{-1};
};

constexpr float slipTime = 0.25f;   // s: a hold at its most this long slips off

constexpr float hangBefore = 0.45f; // s: it hangs this (and up to as much again) before it pulls
constexpr float pullTime = 0.9f;    // s: its pelvis carried up under the lip over this,
constexpr float overUp = 0.5f;      // then up over the lip over this,
constexpr float overIn = 0.6f;      // and in onto the top over this
constexpr float climbPin = 3.f;     // body weights: its pelvis's pin at most
constexpr float climbHertz = 8.f;   // its spring (on its pelvis alone: its muscles hold its weight, climbLift)
constexpr float climbLift = 0.9f;   // its weight held up by its muscles as it pulls and goes over (a share, on every part)
constexpr float climbArms = 0.3f;   // its arms' strength as it pulls and goes over (holding: 0.9)
constexpr float climbChestPull = 0.8f; // body weights: its chest led along at most (climbChest)
constexpr float climbLag = 0.3f;    // (of its arm's length) its pelvis this far behind its way: the way slows to a quarter
constexpr float climbSlack = 1.5f;  // s: a step of it (up under the lip, over it, in) this much later than its time: it gives up

// vr_ragactive.cpp
[[nodiscard]] int sideOf(const char* name);
[[nodiscard]] uint32_t hash(uint32_t a, uint32_t b);
[[nodiscard]] float roll(uint32_t seed, uint32_t what);
[[nodiscard]] bool cut(const Parts& p, int b);
void pushPart(const Parts& p, int b, const glm::vec3& f);
[[nodiscard]] int chestOf(const Parts& p);
[[nodiscard]] bool onBone(const ragdoll::Rig& rig, int b, int top);
[[nodiscard]] bool drivable(const State& s, const Parts& p, int b);
[[nodiscard]] glm::quat bodyRot(const Parts& p, int b);
[[nodiscard]] glm::vec3 worldPoint(const Parts& p, int b, const glm::vec3& rest);
void dropPin(Pin& pin);
[[nodiscard]] glm::vec3 limbEnd(const State& s, const Parts& p, int limb);
[[nodiscard]] float limbLength(const State& s, const Parts& p, int limb);
[[nodiscard]] bool limbWhole(const State& s, const Parts& p, int limb);
[[nodiscard]] float floorAt(const Context& c, const glm::vec3& at, float above, float fallback);
[[nodiscard]] const char* armName(int arm);
void letGo(State& s, int arm, const Context* c, const char* why);
void holdOn(State& s, const Parts& p, const Context& c, int arm, const glm::vec3& at, float cap);
[[nodiscard]] glm::vec3 shoulderOf(const State& s, const Parts& p, int arm);
void livingLedge(State& s, const Parts& p, const Context& c);

// vr_ragactive_swim.cpp
[[nodiscard]] SwimParts swimParts(const Parts& p);
[[nodiscard]] bool liquidsOn();
[[nodiscard]] Liquid enteredLiquid(const Parts& p, const Context& c, const SwimParts& sp);
void startSwim(State& s, const Parts& p, const Context& c, Liquid l, float t);
void swimFrame(State& s, const Parts& p, const Context& c, float t);
[[nodiscard]] glm::vec3 massCentre(const Parts& p, glm::vec3* vel = nullptr);
[[nodiscard]] glm::vec3 level(const glm::vec3& v);
void turnWhole(const Parts& p, const Context& c, const glm::quat& want, float rate, float most, float k);
void holdLevel(const Parts& p, const Context& c, const glm::vec3& anchor, float k);

// vr_ragactive_hold.cpp
bool findGrab(State& s, const Parts& p, const Context& c, const glm::vec3& chestAt, const glm::vec3& v, float L);
void keepGrab(State& s, const Parts& p, const Context& c, int arm, float L);
void climbFrame(State& s, const Parts& p, const Context& c, float L);

// vr_ragactive_cut.cpp
void dismemberFrame(State& s, const Parts& p, const Context& c, float t);

} // namespace detail

using namespace detail;

} // namespace qvr::ragactive
