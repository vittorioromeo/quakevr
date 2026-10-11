// vr_ragactive.hpp -- active ragdolls (experimental; vr_ragdoll_active; ROUND22.md, "Active ragdolls: core and deaths"):
// a ragdoll's joints driven towards a pose (a powered ragdoll, as PuppetMaster's muscles or Euphoria's), and what a
// dying monster does with them.
//
// The core (later stages build on it: a shove's stagger and get-up, liquids, holds on the world, cut limbs, living hit
// reactions):
// - Each joint is driven towards a target: a ball's Box3D spring to its target rotation (frame B relative to frame A),
//   a hinge's to its target angle, at vr_ragdoll_active_hertz times the square root of its strength (a spring's
//   stiffness goes with the hertz squared: its pull goes with the strength), damped near critically; and its motor (the
//   limp ragdoll's joint friction) turning it at the pose's own speed (the next frame's turn, fed forward), at most its
//   limb's weight's lever times vr_ragdoll_active_torque times its strength: a blow or a hand overpowers it. Strength 0
//   is today's limp joint: its friction, and its stiffness towards how he stood.
// - The target: a clip of the model's frames (Clip: the rig's bone transforms in every pose, vr_ragdoll.cpp), played
//   at its frame rate and blended between frames, each joint's turn in its parent; a limb's Reach (two-bone IK: the
//   lower bone a hinge bent by the distance, the upper turned the least way to point at the goal) blended over it.
// - Strength: one per body channel (legs, torso, arms, head), each a Fade over time; times `base` (a headshot: almost
//   none) and each part's own (a limb a hand holds: none, easing back over half a second once let go of).
// - The pelvis pin: a kinematic anchor following the clip's pelvis (and a stumble's carried motion), a Box3D motor joint
//   pulling the pelvis to it with at most `force` body weights (the hand grabs' pattern), fading.
// - Strength 0 everywhere and the pin gone: the drive stops, the ragdoll is today's limp one.
//
// Deaths (beginDeath; the grunt, enforcer, knight, death knight, ogre and zombie): his ragdoll is made at the first frame
// his death leaves him not solid, driven by his own death animation, the legs fading first, then the torso, the arms
// and the head. Dying on the move, he stumbles (the pin carries his motion on a moment, a leg steps ahead); hit in the
// body or a limb, a free hand goes to the wound and holds it; lying face down, a humanoid may crawl a few seconds on his
// arms before going limp. A headshot or beheading: an almost pure collapse.

#pragma once

#include "vr_ragdoll.hpp"

#include "Zancle/Container/Array.hpp"

#include <box3d/box3d.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace qvr::ragactive
{

// The body channels a strength is set for (by the bone's name: thigh/shin/foot, pelvis/chest/tail, arm/hand, head/jaw).
enum class Channel : uint8_t
{
    Legs,
    Torso,
    Arms,
    Head
};
inline constexpr int channelCount = 4;
[[nodiscard]] Channel channelOf(const ragdoll::Bone& bone);
[[nodiscard]] const char* channelName(Channel c);

// The limbs a Reach moves (each found by its bones' names: upperarm/forearm, arm/hand, thigh/shin; _l and _r).
enum class Limb : uint8_t
{
    ArmL,
    ArmR,
    LegL,
    LegR
};
inline constexpr int limbCount = 4;

// A ragdoll as the drive sees it (vr_box3d.cpp's RagdollBodies).
struct Parts
{
    const ragdoll::Rig* rig{nullptr};
    const b3BodyId* body{nullptr}; // [count]: each bone's part (a cut one's: the part it was cut from)
    int count{0};
    uint32_t cut{0};                     // the bones cut off (bits)
    float k{1.f};                        // rest units -> metres (its scale over the world's units a metre)
    float m2u{1.f};                      // the world's units a metre
    glm::quat turn{1.f, 0.f, 0.f, 0.f}; // its model space -> the world (its yaw when it was made)
};

// A strength over time: `from` until `t0` (s since the drive began), eased (smoothstep) to `to` at `t1`.
struct Fade
{
    float from{1.f}, to{0.f};
    float t0{0.f}, t1{0.f};
};
[[nodiscard]] float fadeAt(const Fade& f, float t);

// The pose the joints are driven towards: the model's frames `first`..`last` played at `fps` from frame position
// `start` (a frame and a share of the next) at time `t0`, held at `last` (or looped back to `first`). first < 0: none
// (each joint holds its own turn; only the reaches move limbs).
struct Clip
{
    int first{-1}, last{-1};
    float start{0.f};
    double t0{0.0};
    float fps{10.f};
    bool loop{false};
};

// A limb reaching for a point (world, metres), blended over the clip's pose by `weight` (0..1); its bones' strength at
// least `strength` while it does.
struct Reach
{
    glm::vec3 goal{0.f};
    float weight{0.f};
    float strength{0.f};
};

// The pelvis pin: `force` body weights at most (0: none), `offset` (world, metres) added to the clip's pelvis.
struct Pin
{
    b3BodyId anchor{b3_nullBodyId};
    b3JointId joint{b3_nullJointId};
    float force{0.f};
    Fade fade{0.f, 0.f, 0.f, 0.f};
    glm::vec3 offset{0.f};
    float hertz{4.f};
};

// What a death does (beginDeath; vr_ragactive.cpp, "Deaths").
struct Death
{
    bool on{false};
    bool collapse{false}; // a headshot, beheaded: almost no strength
    float length{1.f};    // s: his death animation from the frame he went limp in
    // Stumble: dying on the move (his own motion, level, m/s).
    bool stumble{false};
    glm::vec3 walk{0.f};
    int stepLeg{-1};      // Limb: the leg that steps
    glm::vec3 stepFrom{0.f}, stepTo{0.f}; // its foot's way (world, metres)
    // Holding his wound: the part hit, the point (its body's space, metres) and outward there; the hand that holds it.
    int woundPart{-1};
    glm::vec3 woundAt{0.f}, woundOut{0.f};
    int woundArm{-1};     // Limb
    // Crawling: chosen when he died (its chance), begun once he lies face down after his death; its way (level, world),
    // the arm reaching now and where its hand was put down.
    bool crawl{false};
    double crawlFrom{-1.0}, crawlUntil{-1.0};
    glm::vec3 crawlDir{0.f};
    int crawlArm{-1};
    double strokeFrom{0.0};
    glm::vec3 planted{0.f};
    bool pulling{false};
    float crawled{0.f}; // metres its pelvis went while crawling (tests)
    // Rolling onto his front first (on his back or side): since when, about which level axis, which way.
    double rollFrom{-1.0};
    glm::vec3 rollAxis{0.f};
    float rollSign{1.f};
    glm::vec3 crawlStart{0.f};
};

// Liquids (vr_ragdoll_active_liquids; vr_ragactive.cpp, "Liquids"; ROUND22.md, "Active ragdolls: liquids"): a dying
// body in water (dying in it, drowned, or falling in within liquidWindow of its death) paddles and kicks, its head kept
// up, weakening as it drowns; in lava or slime it flails, short and violent; then limp. Once per death.
enum class Liquid : uint8_t
{
    None,
    Water,
    Slime,
    Lava
};
struct Swim
{
    Liquid liquid{Liquid::None}; // what it is in (None: not swimming)
    double from{-1.0};           // when it began (-1: not yet; it never begins twice)
    float length{0.f};           // s: its strokes last
    double outSince{-1.0};       // out of every liquid since (it stops after half a second)
    uint32_t seed{0};
    float phase{0.f};            // rad: the strokes' (advanced slower as it weakens)
    float headLift{0.f};         // N: the last frame's lift on the head (the traces)
};

// An active ragdoll's drive (in vr_box3d.cpp's RagdollBodies).
struct State
{
    bool on{false};
    double begun{0.0};
    const qmodel_t* model{nullptr};
    glm::vec3 origin{0.f}; // world, metres: where the clip's model space is (his entity's origin when it was made)
    // Each part's joint to its parent (null: the pelvis, a loose piece), its frame (ragpose::jointFrame) and its range.
    za::Array<b3JointId, ragdoll::maxBones> joint{};
    za::Array<float, ragdoll::maxBones> lower{}, upper{}; // a hinge's range (rad); a ball's cone and twist
    za::Array<float, ragdoll::maxBones> cap{};             // N m: its motor's most at strength 1
    float friction{0.f}, limpHertz{0.f};                   // the limp joint's (Joint Friction, Joint Stiffness)
    float totalMass{0.f};
    za::Array<za::Array<int8_t, 2>, limbCount> chain{};   // each limb's upper and lower bone (-1: none)
    Clip clip;
    float base{1.f};
    za::Array<Fade, channelCount> fade{};
    za::Array<float, ragdoll::maxBones> partScale{};
    za::Array<Reach, limbCount> reach{};
    Pin pin;
    Death death;
    Swim swim;
    // The last frame's (the debug overlay, the traces): each joint's target (its child's turn in its parent's rest
    // space) and the clip's alone, and its strength.
    za::Array<glm::quat, ragdoll::maxBones> target{}, animTarget{};
    za::Array<float, ragdoll::maxBones> strength{};
};

// What the drive needs of the world each frame.
struct Context
{
    b3WorldId world{b3_nullWorldId};
    double now{0.0};
    float dt{0.f};
    float gravity{9.8f};
    uint32_t held{0}; // the parts a hand holds or a throw pins: no strength (with their limb)
    // The floor's height (world, metres) below `at` (within `depth` metres), or -1e30 none.
    float (*floorBelow)(const glm::vec3& at, float depth){nullptr};
    // The liquid at `at` (world, metres); null: liquids not looked at.
    Liquid (*liquidAt)(const glm::vec3& at){nullptr};
    int num{0};      // its entity (the traces)
    double born{0.0}; // when its ragdoll was made (the traces' time)
    bool trace{false}; // a `ragtrace` line (driven or not: vr_ragdoll_active_trace)
};

// The drive started on a ragdoll just made (vr_box3d.cpp createRagdoll, its joints made): `joints` each part's
// (null: none), the limp joint's friction and stiffness. Strength none, no clip: set one, its fades, reaches, a pin.
void begin(State& s, const Parts& p, const qmodel_t* model, const glm::vec3& originMetres, const b3JointId* joints,
    float friction, float limpHertz, double now);

// A death (the frame he went limp in, `frame`, of death animation `death` of his rig).
struct DeathInfo
{
    int death{0};
    float frame{0.f};
    bool collapse{false};
    glm::vec3 walk{0.f};    // m/s: his own motion as he died
    bool wound{false};
    glm::vec3 woundAt{0.f}; // world, metres: where the blow that killed him struck
    uint32_t seed{0};
    bool crawler{false};    // his kind crawls (a humanoid)
};
void beginDeath(State& s, const Parts& p, const DeathInfo& d, double now);

// Each frame before the step: the targets for now, the strengths, the joints set, the pin moved. Stops (on false)
// once nothing is driven.
void drive(State& s, const Parts& p, const Context& c);

// Whether a death's drive, limp now, may still be woken by a liquid (vr_ragdoll_active_liquids: not swum yet, within
// liquidWindow of its death, not a collapse): drive() is to be called for it each frame.
[[nodiscard]] bool liquidPending(const State& s, double now);

// A blast or a hard knock: every strength times `keep` from now on (a hit lowers the muscles).
void weaken(State& s, float keep);

// Its pin's anchor (and joint) gone, the drive off (the ragdoll's end).
void end(State& s);
// The same, its joints made limp first (vr_ragdoll_active 0 while it is driven).
void release(State& s, const Parts& p);

// vr_ragdoll_active_debug: the drive's targets drawn over the parts (a bone's target in green as strong as it is,
// grey limp), the pin (yellow) and the reaches' goals (red).
void debugDraw(const State& s, const Parts& p);

// vr_ragdoll_active_list's line for it.
void describe(const State& s, const Parts& p, int num, double now);

// The drive's cost: milliseconds over the frames since the last call, and how many ragdolls driven.
void costSince(double& ms, int& frames, int& driven);

} // namespace qvr::ragactive
