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
//
// Living monsters (the same classes; ROUND22.md, "Active ragdolls: stagger, judo, get-up"):
// - A shove's stagger (beginStagger): held up by its muscles (a share of its weight), its pelvis pinned and carried on
//   along the shove, kept upright (turnWhole: the whole body turned towards its stance), its feet planted and stepping
//   to where its middle comes to rest (capture steps); all as strong as its balance (the knockdown's chance against
//   the roll). Its middle leaning too far over its feet: it falls; steady: Recovered (vr_box3d.cpp hands it back).
// - A fall (beginFall: a stagger gone over, a shove's knockdown, a two-hand throw): its arms brace towards the floor the
//   way it goes, its torso and head keep some tone (a throw's curl; slack where the hands held it), a flinch as it lands.
// - A get-up (beginRise): rolled over if it lies the other way up, pulled into its get-up's first frame where it will
//   stand, then risen through the get-up (pin, muscles, the whole body turned towards the frames' pelvis), held at its
//   last frame for vr_box3d.cpp to hand it back.

#pragma once

#include "vr_ragdoll.hpp"

#include "Zancle/Container/Array.hpp"

#include <box3d/box3d.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace qvr::ledges
{
class Map;
}

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
// `start` (a frame and a share of the next) at time `t0` (held at `start` until then), held at `last` (or looped back to
// `first`). first > last: played backwards (a death backwards: a get-up). first < 0: none (each joint holds its own turn;
// only the reaches move limbs).
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

// A monster losing limbs as he dies (cutReaction; vr_ragactive.cpp, "Dismemberment"; vr_ragdoll_active_dismember): his
// free hand to the stump, a writhe; on one leg a balance beat before he falls; with no legs a crawl on his arms.
struct Dismember
{
    bool on{false};
    uint32_t lost{0};   // the limbs lost (bits of Limb)
    int liveFrame{-1};  // the frame he stood in, killed by the cut (his ragdoll made then): the balance beat's pose
    uint32_t seed{0};
    // The stump a hand holds: the part it is on, its end there (rest units) and outward; the hand (Limb) and when.
    int stumpPart{-1};
    glm::vec3 stumpAt{0.f}, stumpOut{0.f};
    int holdArm{-1};
    float holdFrom{0.f}, holdUntil{0.f}; // s since the drive began (then let go over 0.8 s)
    // The writhe (s since the drive began): the torso and legs stirring a little, eased in and out; each joint's turn
    // this frame (rad/s, its parent's rest space: fed forward to its motor, which would hold it still).
    float writheFrom{-1.f}, writheUntil{-1.f};
    za::Array<glm::vec3, ragdoll::maxBones> spin{};
    bool pitch{false}; // no legs, killed standing: pitched forward at the next drive
    // The balance beat on one leg: the leg left, the way to the side lost (level, world), his line from that foot to his
    // pelvis at the start (unit), the pin's anchor's origin then, where that foot is held (world, metres); until when at
    // most, and when it broke (his lean from that line past Balance Lean, or the time up) or -1.
    int standLeg{-1};
    glm::vec3 lostSide{0.f}, line0{0.f}, origin0{0.f};
    glm::vec3 plant{0.f};
    float balanceUntil{-1.f}, brokeAt{-1.f};
    Clip deathClip;                       // his death animation, played once the beat breaks
    za::Array<Fade, channelCount> deathFade{}; // and its strengths (shifted to the break)
    // The traces: the hand from the stump (units), his lean (deg).
    float handGap{-1.f}, lean{0.f};
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
    float turnHertz{-1.f}; // its turn's (-1: hertz)
    float turnShare{0.25f}; // its turn's torque at most: this times its force's (N m per N)
};

// ---- What is round him (vr_ragdoll_active_env; vr_ragactive.cpp, "Environment"; ROUND22.md, "Active ragdolls:
// environment") ----
// A dying body senses what is round it (a few rays from the chest every few frames: along its fall, behind it, to its
// sides; the ledges near its hands), each hand picks a goal, and acts on it (Overgrowth's and Euphoria's sense, goal,
// act):
// - Catch: a surface he falls onto soon within reach (the floor ahead, a wall, a crate): the arms out to it, landing
//   with a strength that gives (a capped push: never held).
// - Lean: a wall within reach behind or beside his chest as he dies: a hand on it at shoulder height, held by a capped
//   hand-to-world joint that slides down as he weakens, the knees buckling, the pelvis drawn weakly to the wall: he
//   slumps down it. The hand lets go on its load, its timer, a hit, at the floor.
// - Ledge: falling past a lip (vr_ledges) near a hand: it grabs the lip (the same joint, at the hold), hangs a moment,
//   drops on its timer, its load or a hit.
// - Curl: tumbling with nothing to catch: the arms in to the chest, the knees up.
// A loose prop (a crate) is braced against, never held.
enum class Grip : uint8_t
{
    None,
    Catch, // reaching for a surface he falls onto, then pushing off it (giving)
    Brace, // a hand on a loose prop (no joint)
    Lean,  // a hand on a wall, held, sliding down
    Ledge, // a hand on a lip, hanging
    Curl   // the arms in (tumbling)
};

struct EnvHand
{
    Grip mode{Grip::None};
    glm::vec3 goal{0.f};   // world, metres: where the hand goes (on the surface)
    glm::vec3 normal{0.f}; // out of that surface
    float weight{0.f};     // the reach now (eased towards `want`)
    float want{0.f};
    float strength{0.f};
    double since{0.0};     // when this mode began
    double touched{-1.0};  // when the hand reached the surface (-1: not yet)
    // Its hold (Lean, Ledge): a capped motor joint from the world's (or a mover's) body to the hand.
    b3JointId joint{b3_nullJointId};
    b3BodyId on{b3_nullBodyId};
    double holdUntil{0.0};
    float load{0.f};       // N: the hold's force, the last frame
    float cap{0.f};        // N: its most
    double overSince{-1.0}; // its load at the cap since (slipping)
    float slid{0.f};       // metres the hold slid down
    bool struck{false};    // hit since the last frame (struck())
};

// One probe ray (the debug overlay): from, to (where it stopped), whether it met something.
struct EnvProbe
{
    glm::vec3 from{0.f}, to{0.f};
    bool hit{false};
};

struct Env
{
    bool started{false}; // looked at once (its first drive)
    bool on{false};     // this death senses (vr_ragdoll_active_env, not a collapse)
    uint32_t seed{0};
    uint8_t reached{0}; // the limbs whose reach it set the last frame (bits: Limb)
    int frame{0};       // frames since it began (the probes run every few)
    za::Array<EnvHand, 2> hand{};
    float baseAt{1.f};  // the State's base when it began: a blast since (weaken) lets go
    // Lean: the wall, when, and whether he leaned this death.
    bool lean{false};
    double leanFrom{-1.0};
    glm::vec3 wallAt{0.f}, wallN{0.f};
    bool leaned{false};
    float curl{0.f};    // the curl's weight now
    double nextCatch{0.0}; // no new catch before this (one landing at a time)
    bool hung{false};   // a hand grabbed a ledge this death (once)
    za::Array<EnvProbe, 4> probe{};
    int probes{0};
    // The tests: catches, holds made and let go.
    int catches{0}, holds{0};
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

// A living monster's stagger from a shove (beginStagger; vr_ragactive.cpp, "Staggers"), how it ends.
enum class Outcome : uint8_t
{
    Going,     // still staggering
    Recovered, // back on its feet: ready to be the animated monster again (vr_box3d.cpp hands it back)
    Fell       // it went over: a fall now (a knockdown)
};

struct Stagger
{
    bool on{false};
    Outcome outcome{Outcome::Going};
    double decided{-1.0};   // when it recovered or fell
    glm::vec3 dir{1.f, 0.f, 0.f}; // level: the shove's way
    float travel{0.f};      // metres the pin carries its pelvis along dir
    float time{0.4f};       // s over which (slowing to none)
    float balance{0.5f};    // 0 about to fall .. 1 steady
    float height{1.f};      // m: its pelvis over its feet as it stood
    glm::vec3 pelvis0{0.f}; // its pelvis then (world, metres)
    glm::vec3 lift{0.f};    // its pin's anchor from the stand frame's pelvis to where its pelvis was (metres)
    float lean{0.f}, leanMost{0.f}; // deg: its middle over its feet now, at most
    float over{-1.f};       // s after the drive began: leaning past its limit since (-1: not)
    float sag{1.f};         // its pelvis's height over the floor now over `overFloor`, as it stood (its lowest)
    float overFloor{0.f};   // m (0: not yet measured)
    // Its feet: each leg's place on the floor (planted), its stance (the foot from the pelvis, level), a step's way.
    za::Array<glm::vec3, 2> planted{}, stance{};
    int stepLeg{-1}; // Limb
    double stepFrom{0.0};
    glm::vec3 stepA{0.f}, stepB{0.f};
    int steps{0};
};

// A fall (beginFall: a stagger gone over, a shove's knockdown, a throw; vr_ragactive.cpp, "Falls"): the arms braced
// towards the floor the way it goes, the torso and head keeping some tone (a curl), the legs giving; landed, a flinch,
// then limp.
struct Fall
{
    bool on{false};
    float since{0.f};       // s after the drive began
    glm::vec3 dir{1.f, 0.f, 0.f}; // level: its way (the shove's, the throw's)
    float landed{-1.f};     // s after the drive began (-1: not yet)
    za::Array<bool, 2> put{};         // each arm's hand put down
    za::Array<glm::vec3, 2> hand{};   // where
    float height{1.f};      // m: its chest over the floor as it began
};

// A knocked-down monster getting up (beginRise; vr_ragactive.cpp, "Get-ups"): its body pulled into its get-up's first
// frame where it will stand, then the get-up played by it, risen by its pelvis's pin; done at its last frame (held),
// for vr_box3d.cpp to hand it back to the animated monster.
struct Rise
{
    bool on{false};
    float prep{0.4f};    // s: pulled into the first frame
    // Lying the other way up from its first frame (on its back, the frame on its front): it rolls over first, until then,
    // about a level axis (its spine's), that way; its front as the frame has it.
    double rollUntil{0.0};
    glm::vec3 rollAxis{0.f}, front{0.f};
    float rollSign{1.f};
    double end{0.0};     // the clip's end (then held)
    double polled{0.0};  // the QC's last ask (it stops asking: hit meanwhile, it lapses)
    bool done{false};
    float error{0.f};    // m: its pelvis from the clip's now
    float errorMost{0.f};
};

// An active ragdoll's drive (in vr_box3d.cpp's RagdollBodies).
struct State
{
    bool on{false};
    double begun{0.0};
    const qmodel_t* model{nullptr};
    glm::vec3 origin{0.f}; // world, metres: where the clip's model space is (his entity's origin when it was made)
    glm::quat turn{1.f, 0.f, 0.f, 0.f}; // the clip's model space -> the world (Parts::turn; a get-up's: where it stands)
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
    Dismember dismember;
    Pin pin;
    // Its weight held up by its muscles (a living one standing or rising: a force up on each part, this share of its
    // weight), over time.
    Fade support{0.f, 0.f, 0.f, 0.f};
    // Parts slack until then (with their limbs, as held: a throw's hands just let go).
    uint32_t slack{0};
    double slackUntil{0.0};
    Death death;
    Swim swim;
    Stagger stagger;
    Fall fall;
    Rise rise;
    // The last frame's (the debug overlay, the traces): each joint's target (its child's turn in its parent's rest
    // space) and the clip's alone, and its strength.
    za::Array<glm::quat, ragdoll::maxBones> target{}, animTarget{};
    za::Array<float, ragdoll::maxBones> strength{};
    // What is round him (vr_ragdoll_active_env).
    Env env;
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
    // The world (and the movers) along `from` -> `to` (world, metres): the share of the way to the first hit (1 none),
    // where it hit and its normal.
    float (*rayHit)(const glm::vec3& from, const glm::vec3& to, glm::vec3& at, glm::vec3& normal){nullptr};
    int num{0};       // its entity (the traces)
    double born{0.0}; // when its ragdoll was made (the traces' time)
    bool trace{false}; // a `ragtrace` line (driven or not: vr_ragdoll_active_trace)
    // What is round him (vr_ragdoll_active_env): the shapes his probes see (their category bits: the world, movers,
    // props; 0 none) and the world's ledge map (null: no ledges).
    uint64_t envMask{0};
    const ledges::Map* ledges{nullptr};
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

// A shove's stagger (a living monster's ragdoll just made, standing; vr_box3d.cpp ragdollStagger): its frames
// `first`..`last` (the flinch its pain began) held by its joints, its pelvis pinned up (at most a force by `balance`)
// and carried `travel` metres along `dir` (level), its feet planted and stepping after it, its top pushed by the shove
// (`push`: 1 a fresh two-hand shove). Its middle leaning past vr_ragdoll_active_stagger_lean over its feet (or its
// pelvis sagging low): it falls (a Fall); steady again: Recovered.
struct StaggerInfo
{
    glm::vec3 dir{1.f, 0.f, 0.f};
    float travel{0.f};
    float push{1.f};
    float balance{0.5f};
    int first{-1}, last{-1};
};
void beginStagger(State& s, const Parts& p, const StaggerInfo& info, double now);

// A fall (the drive begun or going): towards level `dir`, the arms bracing, the torso and head keeping some tone, the
// legs giving; parts `slack` (with their limbs) for `slackTime` s (a throw's held ones); `curl`: a throw (more tone in
// the torso: it curls as it goes over).
void beginFall(State& s, const Parts& p, const glm::vec3& dir, uint32_t slack, float slackTime, bool curl, double now);

// A get-up (the drive begun on a knocked-down monster's ragdoll): its frames `first` to `last` (backwards if first >
// last) played at `fps` after `prep` s held at `first`, the model standing at `origin` (world, metres) turned `yaw`
// (rad); its pelvis pinned to the clip's (vr_ragdoll_active_getup_pin body weights), its muscles holding its weight.
struct RiseInfo
{
    int first{-1}, last{-1};
    float fps{10.f};
    float prep{0.4f};
    glm::vec3 origin{0.f};
    float yaw{0.f};
};
void beginRise(State& s, const Parts& p, const RiseInfo& info, double now);

// The clip's frame now (its position: a frame and a share of the next; -1 none).
[[nodiscard]] float clipFrame(const State& s, double now);

// A stagger or a get-up let go (the QC stopped asking, no room to stand, he died): its pin dropped, a fall from here.
void lapse(State& s, const Parts& p, double now);

// Each frame before the step: the targets for now, the strengths, the joints set, the pin moved. Stops (on false)
// once nothing is driven.
void drive(State& s, const Parts& p, const Context& c);

// Whether a death's drive, limp now, may still be woken by a liquid (vr_ragdoll_active_liquids: not swum yet, within
// liquidWindow of its death, not a collapse): drive() is to be called for it each frame.
[[nodiscard]] bool liquidPending(const State& s, double now);

// A blast or a hard knock: every strength times `keep` from now on (a hit lowers the muscles).
void weaken(State& s, float keep);

// Dismemberment (vr_ragactive.cpp, "Dismemberment"): `joint` cut off just now (vr_box3d.cpp ragdollCutLimb; `p.cut` has
// it). A driven death reacts: the head, a collapse; a limb, a hand to the stump and a writhe, a balance beat on one leg
// (killed standing by this cut: `dismember.liveFrame` set as his death began), a crawl with no legs (`crawler`: his kind
// crawls). Off (vr_ragdoll_active_dismember 0): only the collapse.
void cutReaction(State& s, const Parts& p, int joint, bool crawler, const Context& c);
// The joint of limb `limb` (Limb), its upper bone (`lower` false: the shoulder, the hip) or its lower (the elbow, the
// knee), by its bones' names; -1 none (the tests' cuts: QC ragdolllimbs 6).
[[nodiscard]] int limbJointOf(const ragdoll::Rig& rig, int limb, bool lower);

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

// Its part `part` struck (a shot, a blow: box3d::push): a hand holding on there (or its forearm) lets go.
void struck(State& s, const Parts& p, b3BodyId part);

} // namespace qvr::ragactive
