#pragma once

// vr_box3d_internal.hpp -- what the parts of vr_box3d.cpp share (the rigid bodies in Box3D: see vr_box3d.cpp's
// comment). Internal: included by vr_box3d*.cpp alone. What was one file's anonymous namespace and more than one
// part uses is namespace box3d::detail (the world, its slots and categories, the helpers): its types and variables
// here (each variable one object: inline), its functions declared here and defined in the part that owns them.
// What one part alone uses stays in that part's anonymous namespace.
//
// The parts: vr_box3d.cpp (the world: helpers, step tasks, settings, building and destroying it, tool joints),
// _mesh (world mesh, hulls), _props, _corpses, _ragdoll, _knockdown, _ragparts (cuts, grabs, limbs),
// _hands (throws, hands, reach bodies), _step (before and after a step, VR_PhysicsFrameEnd), _contacts (filters,
// portal copies, pushes), _api (loads, queries), _debugdraw, _standing (players on props, the SV_ hooks), _climb
// and _diag (the commands, benches and tests). docs/vr-port/SOURCE_MAP.md has the map.

#include "vr_alloccount.h"

#include "vr_modelmetadata.hpp"
#include "vr_box3d.hpp"
#include "vr_b3glm.hpp"
#include "vr_geom.hpp"
#include "vr_brushes.hpp"
#include "vr_ad_monsters.hpp" // AD support
#include "vr_convex.hpp"
#include "vr_diskcache.hpp"
#include "vr_main.hpp"
#include "vr_portals.hpp"
#include "vr_axestick.hpp"
#include "vr_climb.hpp"
#include "vr_hitmodel.hpp"
#include "vr_jobs.hpp"
#include "vr_cvars.hpp"
#include "vr_grip.hpp"
#include "vr_held.hpp"
#include "vr_ledges.hpp"
#include "vr_lines.hpp"
#include "vr_mem.hpp"
#include "vr_physics.hpp"
#include "vr_physsound.hpp"
#include "vr_profile.hpp"
#include "vr_tracy.h"
#include "vr_progs.hpp"
#include "vr_props.hpp"
#include "vr_limbmodel.hpp"
#include "vr_ragdoll.hpp"
#include "vr_ragactive.hpp"
#include "vr_ragpose.hpp"
#include "vr_ragtest.hpp"
#include "vr_server.hpp"
#include "vr_weapons.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Erase.hpp"
#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/LowerBound.hpp"
#include "Zancle/Algorithm/Rotate.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Algorithm/UpperBound.hpp"
#include "Zancle/Base/BitCast.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Vocabulary/Pair.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"
#include "vr_zancle.hpp"

#include <box3d/box3d.h>

#include <atomic>
#include <string.h>

using namespace qvr;
using namespace qvr::progs;

namespace box3d::detail
{

using qvr::b3glm::b3v;
using qvr::b3glm::fromB3;
using qvr::b3glm::glmv;
using qvr::b3glm::toB3;

using qvr::geom::smooth01;

// The sync's buffers (the server's frame: the main thread; Box3D steps with one worker, the caller's).
struct Box3dScratch
{
    za::Vector<glm::vec3> propVerts;   // a prop's drawn vertices, its hull made (propHull)
    za::Vector<glm::vec3> actorVerts;  // an actor's (actorHull)
    za::Vector<glm::vec3> corners;     // a body's shapes' corners (floorDepth)
    za::Vector<uint8_t> carried;       // by edict: carried by a player (syncEntities)
    za::Vector<b3ContactData> pushContacts; // a pushed prop's touching contacts, all of them (limitPushes)
    za::Vector<glm::vec4> palmPatch;   // a hand's palm, points on its skin (handFit: a limb taken; in the world, w: -radius)
    za::Vector<glm::vec4> fistPatch;   // and its closed hand's spheres (held::fist), the same
    za::Vector<glm::vec3> magVerts;    // a gun's seated magazine's drawn vertices (magazineHull)
    auto members() { return qvr::mem::list(propVerts, actorVerts, corners, carried, pushContacts, palmPatch, fistPatch, magVerts); }
};
inline mem::Scratch<Box3dScratch> scratch{"box3d"};

// Collision categories: props collide with everything; the rest only with props.
constexpr uint64_t catWorld = 1;
constexpr uint64_t catMover = 2;
constexpr uint64_t catActor = 4;
constexpr uint64_t catPlayer = 8;
constexpr uint64_t catProp = 16;
constexpr uint64_t catHeld = 32;
constexpr uint64_t catFixture = 64;
constexpr uint64_t catHand = 128;  // the players' hands (vr_box3d_hand_push): only against solid props
constexpr uint64_t catSolid = 256; // a solid prop's (.vr_rigid 2: an explosive box) shapes, as well as catProp
constexpr uint64_t catReachHand = 512;    // an empty hand's body (vr_box3d_hand_props): the loose props but grenades
constexpr uint64_t catReachWeapon = 1024; // a held weapon's body (vr_box3d_weapon_push): the loose props
constexpr uint64_t catReach = catReachHand | catReachWeapon;
constexpr uint64_t catCorpse = 2048; // a corpse's body (vr_corpse_collide): what meets it is its own mask's (corpseMask)
constexpr uint64_t propMask = catWorld | catMover | catActor | catPlayer | catProp | catHeld | catFixture | catCorpse;
// An explosion's chunk (vr_explosiondebris.cpp; isChunk), as well as catProp: it meets the level, doors and lifts,
// monsters, players, props and corpses (its few grams knock them, barely), never the hands or what they hold; a contact
// with one makes no sound, touch or impact (chunkPair).
constexpr uint64_t catChunk = 4096;
constexpr uint64_t chunkMask = catWorld | catMover | catActor | catPlayer | catProp | catFixture | catCorpse;

enum class Kind : uint8_t
{
    None,
    Prop,   // a rigid body: dynamic
    Held,   // a prop carried in a hand (or two): kinematic, following it
    Mover,  // a brush entity: kinematic
    Actor,  // a monster or another solid box: kinematic
    Player, // a player's body: a kinematic capsule
    Fixture, // a pickup that is not a rigid body (hanging in the air, on a rack): kinematic, its drawn hull
    Corpse,  // a dead monster lying still (vr_corpse_collide): kinematic (Fixed) or a heavy dynamic body (Pushable)
};
[[nodiscard]] const char* kindName(Kind k);
[[nodiscard]] glm::vec3 vec(const float* v);
void store(const glm::vec3& g, float* v);
[[nodiscard]] qmodel_t* modelOf(edict_t* ent);
[[nodiscard]] bool hasFlag(edict_t* ent, int flag);
void setFlag(edict_t* ent, int flag, bool on);
[[nodiscard]] float gravityScaleOf(edict_t* ent);
[[nodiscard]] bool isSolidProp(edict_t* ent);
void localBox(edict_t* ent, qmodel_t* model, glm::vec3& lo, glm::vec3& hi);
[[nodiscard]] float waterDensity(edict_t* ent);
[[nodiscard]] float submerged(const glm::vec3& c, float lo, float hi);
[[nodiscard]] bool isWeaponLike(edict_t* ent);
[[nodiscard]] bool isGrenade(const qmodel_t* model);

constexpr float smallPropSleepThreshold = 0.15f; // m/s: a prop with its own mass (a small gib) sleeps under it (Box3D's: 0.05)

// A small gib (vr_smallgibs.qc): a prop with its own mass (.vr_prop_mass).
[[nodiscard]] bool isSmallGib(edict_t* ent);
// A weapon or backpack a monster dropped (.vr_monster_drop): s since; -1 for anything else.
[[nodiscard]] float monsterDropAge(edict_t* ent);

constexpr float grenadeRestitution = 0.45f; // (Quake's bounce: 0.5; a steel ball on stone)
[[nodiscard]] float densityOf(edict_t* ent, const qmodel_t* model);
[[nodiscard]] float propMassSetting(const qmodel_t* model);
[[nodiscard]] float weaponMassSetting(edict_t* ent, const qmodel_t* model);
[[nodiscard]] float massSetting(edict_t* ent, const qmodel_t* model);
[[nodiscard]] bool isSoft(edict_t* ent, const qmodel_t* model);

// ---------------------------------------------------------------------------------------------------------------------
// Box3D's world

struct Slot // what one edict is in the world (by its number)
{
    Kind kind{Kind::None};
    b3BodyId body{b3_nullBodyId};

    // What the body was made from: made again when it changes.
    const qmodel_t* model{nullptr};
    za::U32 modelRev{0}; // its model's revision (a runtime brush's, made again in place: brushes::revision; else 0)
    int frame{0};
    za::Array<float, 9> scale{};   // props: model_scale, model_scale_origin, model_offset
    glm::vec3 mins{0.f}, maxs{0.f}; // actors: Quake's box; props: the drawn box
    float radius{0.f};              // players: the capsule's

    // The entity as last written (props) or seen (kinematic ones): QC's changes are the differences.
    glm::vec3 origin{0.f}, angles{0.f}, velocity{0.f}, spin{0.f};
    glm::vec3 arrival{0.f}; // props: the velocity this frame's step began with (0 asleep): what a touch after it sees
    float gravityScale{1.f};
    float massSetting{0.f}; // props: the Mass set for its model when it was made (vr_props.inc; 0: none)
    float massScale{1.f};   // props: its estimate's Mass x when it was made (props::massScale)
    float size{1.f};        // props: its model's Size when it was made (Held Object Offsets: props::drawnSize)
    bool asleep{false};
    bool wet{false};      // in water (its lift and drag: beforeStep)
    bool sleepless{false}; // Box3D's sleep off: floating (it bobs), or sinking through the water (beforeStep)
    float lift{0.f};       // N up: its lift in water this frame (beforeStep), given again in each piece of the step
    bool bullet{false};   // fast: continuous collision against other props too
    bool chunk{false};    // an explosion's chunk (isChunk)
    bool soft{false};     // isSoft
    bool brush{false};    // angles as a brush model's
    bool spins{false};    // a fixture drawn spinning (an EF_ROTATE model: the map's pickups): its shape turns with it
    const qmodel_t* mag{nullptr}; // props: a gun's seated magazine its body was made with (seatedMagazine; nullptr none)
    physsound::Material sound{physsound::Material::None}; // what its knocks and scrapes sound like (vr_physsound.cpp)
    double born{0.0};     // the server's time its body was made (a prop: thrown, let go of, launched)
    int pushedStep{-100}; // props: the last step a hand's body pushed it (limitPushes: a hit, then a shove)
    bool flight{false};   // props: thrown by a hand and not yet touched anything (noteThrows; spinAlign)
    const b3HullData* hull{nullptr}; // actors: the hull at rest (actorHull), nullptr for Quake's box
    // Corpses (vr_corpse_collide): the setting and the mask (corpseMask) its body was made with, and whether it is
    // dynamic (Pushable) and fitted to its pose (its hulls: corpseHulls) or a box.
    int corpseMode{0};
    uint64_t corpseMask{0};
    bool corpseDynamic{false};
    bool corpseFitted{false};
    float corpseFriction{0.f};
    int ragdoll{-1}; // a ragdoll (vr_ragdoll): its parts in World::ragdolls (body: its pelvis); -1 none
    // A pushable corpse's middle after the last step (carryCorpses: through a teleporter; its body made again as its
    // frame changes is the same corpse).
    glm::vec3 lastMid{0.f};
    bool midKnown{false};

    // Props, held and fixtures: the settings (shapeGeneration) and the entity's box its drawn box and Mass were last
    // found the same at (stale): looked at again only when one of them changes.
    unsigned checkedGeneration{0};
    glm::vec3 checkedMins{0.f}, checkedMaxs{0.f};
    bool checkedSolid{false};
};

// A ragdoll's parts (vr_ragdoll; "Ragdolls" below): one body per bone of its rig. num 0: a free entry.
struct RagdollBodies
{
    int num{0};
    const ragdoll::Rig* rig{nullptr};
    float scale{1.f};
    int count{0};
    za::Array<b3BodyId, ragdoll::maxBones> body{};
    glm::quat turn{1.f, 0.f, 0.f, 0.f}; // its entity's yaw when it was made (the frames' bones turned by it)
    double born{0.0};
    // Its head cut off (cutHead; ROUND21.md, "Decapitation"): those bones (bits) have no body nor joint now; their
    // entries in `body` name the part they were cut from (the loops over the parts stay valid: those that add up or push
    // per part skip them, partCut). And the head as it was cut (ragdollCut): its middle, turn, launch and spin (world).
    // Limb gore (cutLimb): any limb so; the head* fields are the last piece cut, `lastCut` its joint, `lastCutBones` what it
    // took.
    uint32_t cut{0};
    glm::vec3 headMid{0.f}, headVel{0.f}, headSpin{0.f};
    glm::quat headRot{1.f, 0.f, 0.f, 0.f};
    int lastCut{-1};
    uint32_t lastCutBones{0};
    // Its parts' motion times this at its next step (feedRagdoll, after the frame's knocks): a head pop's body barely
    // moves (ragdollDecap's settle, vr_decap_pop_body_speed); 1 none pending. Only the knock is scaled: each part's own
    // motion (metres, radians: the monster's walk and its animation's, or a corpse's as it lay; vr_decap_own_motion) is
    // kept.
    float settle{1.f};
    za::Array<glm::vec3, ragdoll::maxBones> ownLin{}, ownAng{};
    // A living monster knocked down (vr_knockdown; "Knockdowns" below): its loose pieces (a weapon) welded to the part
    // that held them, until it gets up or dies.
    bool knocked{false};
    za::Array<glm::quat, ragdoll::maxBones> struggleRest{};
    bool struggleReady{false};
    int shockJerk{-1}; // shocked (shockRagdoll): the jerk it last bucked or not at
    // Its pelvis after the last step (carryRagdolls: the step it goes in through a teleporter carries it whole).
    glm::vec3 lastPelvis{0.f};
    bool pelvisKnown{false};
    // The two-hand throw's topple (ragdollTopple): these parts (bits, its feet) kept at pinVel (level, m/s: swept back
    // against the throw, or still) until pinUntil.
    uint32_t pinned{0}, feet{0}; // (feet: the parts it toppled over, for the tests' ragdollStance)
    double pinUntil{0.0};
    glm::vec2 pinVel{0.f};
    // Its middle's speed down (m/s, its parts' by their mass, a loose weapon not counted) before the step's piece
    // (noteFallSpeeds): a landing is no harder than its whole body comes down (touches: a fall's damage).
    float fallDown{0.f};
    // Its middle's speed (m/s, any way) and its mass (kg) then, the same parts': an impact's measure (touches, Impacts).
    float bodySpeed{0.f}, bodyMass{0.f};
    bool warnedUnstable{false}; // a part of it went faster than unstableSpeed (noteFallSpeeds said so)
    // Each part's lift in a liquid this frame (N, up; ragdollsInLiquids), given again for the step's later pieces.
    za::Array<float, ragdoll::maxBones> lift{};
    // Its active drive's forces this frame (N, world; ragactive::Parts::force), given again for the step's later pieces
    // (liftAgain), only in the frame they were set (`driveForceAt`, the server's time).
    za::Array<glm::vec3, ragdoll::maxBones> driveForce{};
    double driveForceAt{-1.0};
    // A shove's knockdown (ragdollShove; ROUND21.md, "A shove's knockdown: travel and a quarter turn"): driven each frame
    // (driveShove) while `on`, its travel apart from its turn.
    struct ShoveDrive
    {
        bool on{false};
        bool ledge{false};      // over a ledge: only kept going at `speed` until it has gone `reach` (past the edge)
        double start{0.0};
        glm::vec3 dir{0.f};     // level, along the shove
        float speed{0.f};       // m/s: its travel's speed at the start, slowing evenly (decel) to none
        float decel{0.f};       // m/s/s
        float duration{0.f};    // s it travels
        glm::vec3 pelvis0{0.f}; // its pelvis at the start, and how high above its feet's floor (metres)
        float pelvisUp{0.f};
        float reach{0.f};       // metres (over a ledge)
        glm::vec3 from{0.f};    // its middle (centre of mass) at the start, metres
        float angle{0.f};       // rad: the turn about its feet, reached at `time` s
        float time{0.f};
        float lag{0.f};         // share of the travel its feet lack as it turns (its top as much ahead), its middle none
        float maxSpin{0.f};     // rad/s: its turn never faster
        float rate{0.f};        // rad/s: over a ledge, its turn's (until it has turned `angle`)
        float floor{0.f};       // over a ledge: the floor it stood on (metres up): its middle below it, it went over
        float meanShare{0.5f};  // its parts' mean height share (mass-weighted)
        za::Array<float, ragdoll::maxBones> share{}; // each part's height share at the start (its feet 0, its top 1)
    } shove;
    // Active (vr_ragdoll_active; vr_ragactive.hpp): its joints driven towards a pose (a death: his death animation; a
    // living one's stagger, fall or get-up), the strengths fading; off: limp. Its joints (each part's to its parent,
    // null none) and their limp friction and stiffness, kept for a drive begun later (a knockdown's get-up).
    ragactive::State active;
    za::Array<b3JointId, ragdoll::maxBones> joints{};
    float jointFriction{0.f}, jointStiffness{0.f};
    double staggerStuck{-1.0}; // a recovered stagger with no room to stand since (it falls after a while)
    // Its get-up as its body rises (ragdollGetUp): the frame the QC chose (1, 2), where it will stand, its yaw (rad).
    int riseWhich{0};
    glm::vec3 riseSpot{0.f};
    float riseYaw{0.f};
    // A rise begun and still asked for (ragdollGetUp): if it lapses (lost its pose, or no room left at its spot), the
    // next ask waits vr_knockdown_retry (returns 0) before trying again; after riseLapsesMost the old blended get-up.
    bool riseAsked{false};
    int riseLapses{0};
    double riseStuck{-1.0}; // risen, its spot taken since (it slumps after a while)
    double swimGoalAt{-1.0}; // a living knockdown swimming (livingSwim): when its goal (where it can stand) was looked for
    bool drowning{false};    // knocked down drowning (ragdollDrowning): its living swim begins frantic
    bool heldWet{false};     // its get-up asked while a hand holds it in a liquid (ragdollGetUp: it stays down; printed once)
    bool neckSnapped{false}; // its neck snapped (ragdollNeckSnapped): its head's joint loose, never driven
};

// A knocked-down monster getting up ("Knockdowns"): its ragdoll's last pose blended into its animation as it plays.
struct Recovery
{
    int num{0};
    const ragdoll::Rig* rig{nullptr};
    float scale{1.f};
    int count{0};
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    double start{0.0};
    float duration{0.35f};
    int frame{-1}, prevFrame{-1};
    double changed{0.0}; // the server time its frame changed (the think that changed it: due then)
    double due{0.0};     // its next think, as of the last step
};
[[nodiscard]] bool partCut(const RagdollBodies& r, int b);
[[nodiscard]] int partsLeft(const RagdollBodies& r);

// The most shapes a body has (a gun's convex pieces: vr_box3d_gun_pieces, at most this).
constexpr int maxBodyShapes = 32;

// The hulls made for models, by what they were made from (and the world's scale).
struct PropHullKey
{
    const qmodel_t* model;
    int frame;
    za::Array<float, 6> box;
    bool operator==(const PropHullKey&) const = default;
};

struct PropHullKeyHash
{
    [[nodiscard]] za::U64 operator()(const PropHullKey& k) const
    {
        za::U64 h = ankerl::unordered_dense::hash<const void*>{}(k.model) ^ (static_cast<za::U64>(static_cast<za::U32>(k.frame)) * 0x9E3779B97F4A7C15ull);
        for(const float f : k.box)
        {
            // (+ 0.f: -0 and +0 are one key, as they are equal)
            h = (h ^ ZA_BIT_CAST(za::U32, f + 0.f)) * 0x100000001B3ull;
        }
        return h;
    }
};

// The guns' convex pieces (gunPieces) as made, in model units, kept from map to map (made again only when the models'
// slots or the VR models change: their pointers are the keys): a gun first picked up on a map took 35 to 60 ms to cut
// (vr_convex.cpp). The main thread's.
struct GunShapeCache
{
    struct Entry
    {
        PropHullKey key;
        int most{0};       // vr_box3d_gun_pieces as made
        int firstPiece{0}; // in pieceEnds
        int pieces{0};     // 0: none made (one hull)
        convex::Report report;
    };
    za::Vector<Entry> entries;
    za::Vector<int> pieceEnds;      // each piece's end in points
    za::Vector<glm::vec3> points;   // the pieces' hulls' points, model units
    auto members() { return mem::list(entries, pieceEnds, points); }
};
inline mem::Cache<GunShapeCache> gunShapes{"box3d gun shapes", mem::GameDirChange | mem::ModelReload};

// A prop's hardest hit in a frame, for its .vr_impact (an explosive box dropped or thrown).
struct Shock
{
    int num{0};
    int other{0}; // what it hit (0: the level)
    float speed{0.f}; // m/s, the contact points' approach
};

// A ragdoll's part striking the level hard, the part's hardest in a frame (touches; Impacts: QC VR_RagImpact_Hit).
struct Impact
{
    int num{0};
    glm::vec3 at{0.f};  // the contact point (units)
    float speed{0.f};   // m/s: the part's approach, no faster than vr_ragimpact_whip times its whole body's speed
    float raw{0.f};     // m/s: the part's approach
    float mass{0.f};    // kg behind it: the part's, vr_ragimpact_body of the rest of the body (and a hand's, held)
    float partMass{0.f};
    float bodyMass{0.f};
    bool held{false};   // a hand holds the part (it drives it)
    int part{-1};       // which part (its ragdoll's body): each part's hardest strike kept
};

// A prop near a hand's body before a step, and its motion then (notePushed, limitPushes).
struct Pushed
{
    int num{0};
    glm::vec3 velocity{0.f}, spin{0.f}; // m/s, rad/s
};

// Box3D on the pool (vr_box3d_threads; ROUND21.md, "Box3D on the pool"): a task of Box3D's step
// (b3EnqueueTaskCallback) as a jobs::Task, run by a worker of the game's pool or by the stepping thread as it waits for
// it (finishStepTask). The worker borrows the stepping thread's QuakeC VM (qcvm and pr_global_struct are thread-local):
// the callbacks it may call (shouldCollide, preSolve) read the entities.
struct StepTask
{
    jobs::Task task;
    b3TaskCallback* fn{nullptr};
    void* context{nullptr};
    qcvm_t* vm{nullptr};
    globalvars_t* globals{nullptr};
};

// A world's (its b3WorldDef's userTaskContext): its workers and the tasks of the step under way.
struct StepTasks
{
    int workers{1}; // Box3D's (stepWorkers; 1: the step on the stepping thread alone)
    za::Array<StepTask, B3_MAX_TASKS + 8> tasks; // a step's (Box3D enqueues at most B3_MAX_TASKS)
    za::Atomic<int> next{0};                      // the step's next in `tasks` (0 before each step)
};

// vr_physics_frametime's phases of a server frame's physics (VR_PhysicsFrameEnd's, and SV_Physics's round them).
enum FramePhase
{
    PhaseServerPhysics, // SV_Physics from the world's turn to the end of Box3D's frame: the entities' thinks and moves too
    PhaseBox3D,         // VR_PhysicsFrameEnd, all of it
    PhaseSync,          // the entities into their bodies (syncEntities, hands, reach)
    PhaseBefore,        // the water and the hit boxes before the step (beforeStep, standing, shoves)
    PhaseStep,          // the step's pieces with their touches (notePushed .. traceGibContacts)
    PhaseSolver,        // b3World_Step alone
    PhaseWrite,         // the bodies into their entities (writeProp, writeRagdoll, writeCorpse)
    PhaseAfter,         // the touches' QC (SV_Impact), the shocks, watchInside, the sounds
    framePhases
};
constexpr const char* framePhaseNames[framePhases] = {"server physics", "box3d", "sync", "before", "step", "solver", "write", "after"};

struct World
{
    b3WorldId id{};
    StepTasks tasks;
    // vr_physics_steptime: the steps' time since it last printed.
    double stepTime{0.0}, stepTimeMax{0.0}; // s, a frame's
    int stepFrames{0};
    za::U64 stepAwake{0}; // the awake bodies, summed over the frames
    // vr_physics_steptime: each frame's step since it last printed (its median, 95th percentile; by awake bodies).
    struct StepSample
    {
        float ms{0.f};
        int awake{0}; // the awake bodies as the step began (what stepWorkers saw)
    };
    za::Vector<StepSample> stepSamples;
    b3Profile stepProfile{}; // Box3D's own (ms), summed over the steps (vr_physics_steptime bins)
    // vr_physics_frametime: each server frame's physics by phase (ms; framePhaseNames) since it last printed.
    struct FrameSample
    {
        float ms[framePhases]{};
        int awake{0}; // the awake bodies after the step
    };
    za::Vector<FrameSample> frameSamples;
    const qmodel_t* map{nullptr};
    int generation{-1};
    float m2u{1.f};      // units a metre
    float gravity{0.f};  // sv_gravity at the last update
    float friction{-1.f}, restitution{-1.f};
    b3MeshData* mesh{nullptr}; // (meshCache's)
    b3ShapeId worldShape{b3_nullShapeId};
    za::Vector<za::Pair<int, int>> impacts; // the step's touches (kept: no allocation a frame)
    za::Vector<Shock> shocks; // props with a .vr_impact hitting something this frame (the hardest hit each)
    za::Vector<Shock> falls;  // knocked-down monsters' ragdolls hitting the level this frame (the hardest, vertically)
    za::Vector<Impact> ragImpacts; // ragdolls' parts striking the level this frame (the hardest each: callRagImpacts)
    za::Vector<Pushed> pushed; // the props near the hands' bodies before this step (kept: no allocation a frame)
    struct PortalCopy
    {
        b3BodyId original{b3_nullBodyId}, copy{b3_nullBodyId};
        glm::mat3 turn{1.f};
        glm::vec3 shift{0.f}, velocity{0.f}, spin{0.f};
        glm::vec4 here{0.f}, there{0.f};
        bool dynamic{false};
    };
    za::Vector<PortalCopy> portalCopies;
    za::Vector<Slot> slots; // by edict number
    // The players' hands (by client, [0] off, [1] main): kinematic spheres at their fists that push solid props; and
    // their reach bodies (syncReach): the empty hand's, or the held weapon's.
    struct ReachKey
    {
        enum What : uint8_t
        {
            None,
            Open,    // an open hand: its box (the palm, the fingers)
            Fist,    // a fist (the grip held)
            Weapon,  // the held weapon's drawn hull (view::drawnWeapon)
            Capsule, // a held weapon not drawn here (another player's): the hand to its muzzle
        };
        What what{None};
        const qmodel_t* model{nullptr};
        bool mirrored{false};
        unsigned generation{0};
        float scale{0.f};
        glm::vec3 muzzle{0.f}; // (Capsule) in the hand's frame
        const qmodel_t* mag{nullptr}; // (Weapon) its seated magazine, solid with it (view::DrawnWeapon::mag)
        bool operator==(const ReachKey&) const = default;
    };
    struct HandBody
    {
        b3BodyId body{b3_nullBodyId};
        bool armed{false};
        glm::vec3 at{0.f};
        b3BodyId reach{b3_nullBodyId};
        ReachKey key{};
        za::Vector<int> ignore; // props the reach body passes through until clear of them (inside it as it was made)
        int held{0};             // the prop the hand carried last frame (0: none)
        float weaponMass{0.f};   // the held weapon's own mass (kg: Weapon Weights' Mass; 0: none, or not a weapon)
        za::Vector<glm::vec4> fist; // the push body's spheres (the drawn fist, in the hand's frame; empty: the one sphere)
        za::Vector<b3Vec3> outline; // a weapon in convex pieces: their hull's points (its frame, m: sweepReach's sweep)
    };
    za::Vector<za::Array<HandBody, 2>> hands;
    za::Vector<glm::vec3> walkOrigins; // physical walking also nudges nonblocking corpses
    // Throws (vr_box3d_throw_grace, noteThrows): what was thrown passes through its thrower's hands' bodies a moment.
    struct Grace
    {
        int num{0};             // the thing thrown
        int player{0};          // its thrower (client)
        double born{0.0};       // its body's (Slot::born): the same body still
        double until{0.0};      // the server's time it meets the hands again
        bool skips{true};       // false: only watched (vr_debug_box3d with the grace off: its numbers, for comparing)
        bool ragdoll{false};    // a ragdoll's (ragdollGrace: a judo throw's enemy): `born` its RagdollBodies'
        glm::vec3 velocity{0.f}; // m/s, rad/s: as it left the hand
        glm::vec3 spin{0.f};
        // The hands' contacts it passed through (steps): counted by preSolve, on Box3D's workers (relaxed; copied with
        // the grace).
        struct Count
        {
            za::Atomic<int> n{0};
            Count() = default;
            Count(const Count& o) noexcept : n{o.n.loadRelaxed()} {}
            Count& operator=(const Count& o) noexcept
            {
                n.storeRelaxed(o.n.loadRelaxed());
                return *this;
            }
        } skipped;
    };
    za::Vector<Grace> graces;
    za::Vector<int> made; // the props whose bodies were made this frame (createBody): what may have been thrown
    // Small gibs and monster drops whose bodies were made inside a monster's or a player's (noteBornInside): they pass
    // through it until clear of it (shouldCollide), as a hand's reach body passes through what was inside it
    // (HandBody::ignore).
    struct Inside
    {
        int prop{0};      // the small gib or drop
        int other{0};     // the monster or player
        double born{0.0}; // the small gib's body's (Slot::born): the same body still
    };
    za::Vector<Inside> inside;
    // The players standing on solid props (by client; vr_box3d_player_stand, "Standing on props" below).
    struct Stand
    {
        int ground{0};          // the prop stood on at the frame's end (0: none): this frame's, then the last's
        glm::vec3 feet{0.f};    // the point stood on (units): under the player's origin, at its feet, in the prop's box
        b3Vec3 local{};         // that point in the prop's body before this frame's step
        float top{0.f};         // the top of the prop's Quake box then (units): what his box stands on
        // A jump's push on the prop jumped from, spread over jumpPushTime (a force, not a blow: see beforeStanding).
        int pushed{0};          // the prop (0: none)
        b3Vec3 pushLocal{};     // where, in its body
        float pushForce{0.f};   // N, down
        float pushLeft{0.f};    // s
    };
    za::Vector<Stand> stands;
    // The climbing hands' pulls on light props this frame (climbPull; vr_climb.cpp, "Holds on props"): a force through
    // each piece of the step (pressStanding's), then forgotten.
    struct ClimbPull
    {
        int num{0};
        b3BodyId body{b3_nullBodyId};
        b3Vec3 local{}; // where, in its body
        b3Vec3 force{}; // N (Box3D's: its gravity's)
    };
    za::Vector<ClimbPull> climbPulls;
    // A player's move blocked by a solid prop's side this frame (VR_PlayerBumps): he shoves it (shoveBumped).
    struct Bump
    {
        int player{0};
        int num{0};
        glm::vec3 dir{0.f};     // horizontal, into the prop
        float speed{0.f};       // units/s, his speed into it
    };
    za::Vector<Bump> bumps;
    ankerl::unordered_dense::map<PropHullKey, b3HullData*, PropHullKeyHash> propHulls; // nullptr: no hull (a box instead)
    // The hulls of the props made this frame, posted to the pool before syncEntities' loop (prebuildPropHulls; propHull
    // takes each; what is left goes into propHulls at the loop's end).
    ankerl::unordered_dense::map<PropHullKey, jobs::Future<b3HullData*>, PropHullKeyHash> pendingHulls;
    ankerl::unordered_dense::map<const qmodel_t*, za::Vector<b3HullData*>> moverHulls;
    // A runtime brush's model made again in place (vr_brushes.cpp): the revision its hulls were made from, and the hulls
    // let go of (a body made with them may still be about until its slot is made again: freed once no mover body of
    // that model and revision is left, freeRetiredHulls, or with the world).
    ankerl::unordered_dense::map<const qmodel_t*, za::U32> moverHullRevs;
    struct RetiredHull
    {
        const qmodel_t* model;
        za::U32 rev;
        b3HullData* hull;
    };
    za::Vector<RetiredHull> retiredHulls;
    // The guns' convex pieces (gunPieces; empty: one hull), by model and frame (a held gun's: frame -2, -3 mirrored).
    ankerl::unordered_dense::map<PropHullKey, za::Vector<b3HullData*>, PropHullKeyHash> gunPieces;
    ankerl::unordered_dense::map<PropHullKey, convex::Report, PropHullKeyHash> gunReports; // (vr_physics_shapes)
    int gunPiecesMost{-1}; // the vr_box3d_gun_pieces they were made with
    // Corpses' hulls fitted to their pose (corpseHulls): by model, frame and scale.
    ankerl::unordered_dense::map<PropHullKey, za::Vector<b3HullData*>, PropHullKeyHash> corpseHulls;
    // By edict: a dead monster's frame and model, and since when (the server's time) it has had them (watchCorpses: a
    // death animation over, for a corpse the QC doesn't say lies still); -1: not a dead monster.
    struct CorpseWatch
    {
        float frame{-1.f};
        float model{0.f};
        double since{-1.0};
        glm::vec3 origin{0.f}, last{0.f}; // where it is, and where it was the server frame before (createRagdoll)
        double changed{-1.0};              // when its frame last changed from one seen (not as it was first seen: a load)
        bool dying{false}; // seen dead but still solid (its frame and origin kept): the frame it stops being solid in
                           // is known to be new or not (the fiend's death6: SOLID_NOT and the frame together)
        bool settled{false}; // put at its death's last frame, once (settleCorpses: left still mid-death)
    };
    za::Vector<CorpseWatch> corpseWatch;
    bool corpseFrameSaid{false}; // settleCorpses said its first in this world (the rest: developer)
    // By edict: a monster's (live or dying) last three places its origin changed to, and when (watchCorpses; its own
    // motion: ownMotion), and its velocity the frame before (what the frame's knock added to it); n how many are known.
    struct MonsterMotion
    {
        za::Array<glm::vec3, 3> at{};
        za::Array<double, 3> when{};
        int n{0};
        glm::vec3 vel{0.f};
        // Its own motion the last frame it was alive (units/s), and when (an active death's stumble: deathMotion).
        glm::vec3 alive{0.f};
        double aliveAt{-1.0};
    };
    za::Vector<MonsterMotion> motion;
    za::Vector<RagdollBodies> ragdolls; // (Slot::ragdoll)
    // A full ragdoll pool (vr_ragdoll_max): the corpses waiting for QC to make room (VR_Knockdown_MakeRoom), run after
    // syncEntities' edict loop rather than inside it; and the ones it refused, left as corpses until they are gone.
    za::Vector<int> roomWanted, roomRefused;
    bool syncingEntities{false};
    za::Vector<Recovery> recoveries;    // knocked-down monsters getting up (Knockdowns)
    // Hands holding a ragdoll's limb (vr_ragdoll_grab; "Ragdolls"): a kinematic body at the hand and a motor joint to
    // the limb.
    struct RagdollGrab
    {
        int player{0};
        int hand{0}; // QC's: 0 the off hand, 1 the main
        int num{0};  // the ragdoll's entity
        int part{0};
        b3BodyId anchor{b3_nullBodyId};
        b3JointId joint{b3_nullJointId};
        double farSince{-1.0}; // since when the limb is far from the hand (stuck behind something: let go of)
        // A soft hold's (ragdollHoldSoft: a judo throw's or a lift's enemy held on): its spring's most force (N; < 0:
        // vr_ragdoll_grab_force's), its rate (Hz; 0: grabHertz) and how far the limb may lag before it is let go of
        // (units; 0: grabStuck).
        float force{-1.f};
        float hertz{0.f};
        float stuck{0.f};
    };
    za::Vector<RagdollGrab> ragdollGrabs;
    // Ragdolls made in each other (two grunts dying on one spot, vr_ragdoll_collide_each): they pass through each other
    // until no part of one is in the other (pruneRagdollsInside), as a small gib made in a body does.
    struct RagdollPair
    {
        int a{0}, b{0};
    };
    za::Vector<RagdollPair> ragdollsInside;
    // The last blasts (box3d::blast), for the ragdolls made just after them (createRagdoll: a grunt a blast killed).
    struct RecentBlast
    {
        glm::vec3 at{0.f};
        float damage{0.f};
        double time{-1e9};
    };
    za::Array<RecentBlast, 8> recentBlasts{};
    int nextBlast{0};
    bool gunsPrepared{false};   // the map's held guns' pieces made (prepareHeldGuns, at its first frame)
    int gunsLoadout{-1};        // the first player's hands and holsters when they were (made again when they change)
    bool ragdollsWarmed{false}; // the map's ragdoll models rigged (syncEntities: at its first frame with vr_ragdoll on)
    int steps{0};

    [[nodiscard]] b3Vec3 toM(const glm::vec3& u) const { return b3Vec3{u.x / m2u, u.y / m2u, u.z / m2u}; }
    [[nodiscard]] glm::vec3 toU(const b3Vec3& m) const { return glm::vec3{m.x, m.y, m.z} * m2u; }
};

inline za::UniquePtr<World> world;

// The toolgun's (vr_toolgun.cpp; docs/vr-port/TOOLGUN.md): the props it pinned (frozen, or held by the physgun) and the
// joints it made, by edict number. Outside the world: a world made again (a setting changed) keeps them, its joints made
// again from them (syncToolJoints); a new server or the entity's removal forgets them (reset, toolForget). The main thread.
struct ToolJointRecord
{
    int a{0}, b{0};
    qvr::box3d::ToolJoint kind{qvr::box3d::ToolJoint::Weld};
    b3Transform frameA{}, frameB{}; // in each body's frame (metres), as Box3D's joint definitions take them
    float length{0.f};              // rope and spring: the rest length (metres)
    b3JointId id{b3_nullJointId};   // the joint now (null: made at the next frame both bodies are there)
};
struct ToolRecords
{
    za::Vector<int> pinned;
    za::Vector<ToolJointRecord> joints;
};
inline ToolRecords toolRecords;
[[nodiscard]] bool toolPinned(int num);

constexpr za::SizeT stepSamplesMax = 1u << 16; // vr_physics_steptime's frames kept (15 minutes at 72 Hz)
inline double serverPhysicsStart = 0.0; // SV_Physics's world's turn this frame (Sys_DoubleTime; noteServerPhysicsStart)
inline bool frameTiming = false;        // vr_physics_frametime asked for once: its samples kept from then on
inline double frameLastWall = 0.0;      // vr_physics_frametime: the wall clock when it last printed
inline int frameLastHostFrame = 0;      // and at which host frame
void* enqueueStepTask(b3TaskCallback* fn, void* context, void* user, const char* /*name*/);
void finishStepTask(void* task, void* /*user*/);

// Box3D's wide kernels (the contact solver, hull-hull manifolds; external/box3d/README.md): AVX2's 8 lanes where the
// CPU has AVX2 and vr_box3d_avx2 is on, SSE2's 4 otherwise. Set on a world before each step (it may change between two).
inline const bool cpuHasAvx2 = b3IsAVX2Available(); // (a CPUID: once, before main)
void applySimdWidth(b3WorldId id);
[[nodiscard]] glm::quat turnOf(const float* angles, bool brush);
[[nodiscard]] void* userOf(int num);
[[nodiscard]] int numOf(b3ShapeId shape);
[[nodiscard]] Slot& slotOf(int num);

void dropGrabsOf(int num); // (below: "Ragdolls")
void destroyBody(Slot& s);

// The world model's faces as a triangle mesh (metres), sky and liquids left out, wound counter-clockwise seen from
// the open side (a face's plane, flipped for SURF_PLANEBACK). Vertices shared by the BSP's faces stay shared, so
// Box3D finds the triangles' neighbours (identifyEdges: no bumps at inner edges).
//
// T-junctions: the BSP splits faces where they meet others, and a face's edge often has another face's corner in its
// middle (one of the BSP's shared vertices, but not the face's). Two triangles then share no edge there, and a box
// sliding over the seam can catch on it. Such corners are put into the edge (every drawn face's edges are checked
// against the corners near them), and a face that got any is fanned from a new vertex in its middle, so that every
// piece of its outline is a triangle's edge, shared with the neighbour's.
struct MeshStats
{
    int faces{0}, triangles{0}, junctions{0}, junctionFaces{0}, middleFans{0};
    double ms{0.0};
};
[[nodiscard]] b3MeshData* worldMesh(const qmodel_t* map, float m2u, bool junctions, MeshStats& stats);

// The world's mesh is made once per map and world scale and kept while the map is (a saved game loaded, a restart:
// Box3D's world is made again, not the mesh). A few scales are kept: a take's replay (vr_motion_play) sets the world
// scale it was recorded at and puts the user's back, and each eval take loads the map at the user's scale first; the
// mesh (in metres) isn't made again for either (35 ms each, twice per take). Other maps' meshes are kept too
// (vr_box3d_mesh_keep_maps: a return to the hub, 47-50 ms of its load), each by all that worldMesh reads of its map
// (MeshInputs: the faces it makes, in order, with their corners' numbers and points), compared whole: an edited map
// of the same name and counts is a new mesh.
struct MeshCache
{
    char name[MAX_QPATH]{};
    int vertexes{0}, surfaces{0};
    float m2u{0.f};
    bool junctions{true};
    b3MeshData* mesh{nullptr};
    MeshStats stats;
    za::U64 inputsKey{0};          // MeshInputs' hash
    za::Vector<za::U32> inputs;    // MeshInputs' words
    za::U32 serial{0};             // (MeshInputs' check: this entry's inputs found the same as the map's)
};
inline za::Vector<MeshCache> meshCache; // the latest used last

// What worldMesh reads of a map (beside the scale and vr_box3d_mesh_junctions), as words: the faces it makes (not sky
// or liquid, 3 corners or more), each its corner count, its normal and its corners (the vertex's number and point).
// Made once a map load (beforeLoad: a few ms on vrstart; the map's model is freed and loaded again at each), kept for
// the map's frames; `same` lists the cache entries found equal to it (their serials), so a frame's check compares no
// words.
struct MeshInputs
{
    const qmodel_t* map{nullptr};
    za::U64 key{0};
    za::Vector<za::U32> words;
    za::Vector<za::U32> same;
};
inline MeshInputs meshInputs;
const MeshInputs& inputsOf(const qmodel_t* map);

void settleMesh();
[[nodiscard]] MeshCache* findMesh(const qmodel_t* map, float m2u);
[[nodiscard]] bool meshCurrent(const qmodel_t* map, float m2u, const b3MeshData* mesh);

// The map's mesh made on the game's thread pool while the map spawns (beforeLoad), for the cache: put there at the
// first look at the cache after (findMesh), or when the map's memory is about to go (finishLoads).
struct PendingMesh
{
    jobs::Future<b3MeshData*> job;
    MeshCache key; // its map, scale and settings (its mesh: the job's)
    const qmodel_t* map{nullptr};
};
inline PendingMesh pendingMesh;
void settleMesh();
[[nodiscard]] b3MeshData* cachedWorldMesh(const qmodel_t* map, float m2u);

// A convex region as the intersection of half-spaces dot(n, p) <= d: its corners (triples of planes meeting inside
// all of them).
struct HalfSpace
{
    glm::dvec3 n;
    double d;
};
[[nodiscard]] za::Vector<b3Vec3> corners(const za::Vector<HalfSpace>& planes, float m2u);
[[nodiscard]] const za::Vector<b3HullData*>& moverHulls(qmodel_t* model);
[[nodiscard]] b3HullData* fittedHull(const za::Vector<b3Vec3>& points, int budget, float m2u);

struct GunPiecesDisk
{
    std::atomic<int> read{0};     // pieces read from a file (atomics: prepareHeldGuns cuts on the pool)
    std::atomic<int> cut{0};      // cut (no file, or checked)
    std::atomic<int> written{0};  // files written
    std::atomic<int> checked{0};  // vr_gun_pieces_cache 2: read and cut, compared
    std::atomic<int> differed{0}; // of those, not the same
};
inline GunPiecesDisk gunPiecesDisk;
[[nodiscard]] bool cutGun(const za::Vector<glm::vec3>& triangles, const convex::Settings& settings,
    za::Vector<convex::Piece>& pieces, convex::Report& report, const char* name);

// A gun in convex pieces that follow its drawn shape (vr_convex.hpp: its shell port, its well, the gaps between its
// parts left open; one hull spanned them, and a round lying on it rested on air), from its drawn triangles (`corners`,
// units), as hulls in metres; empty: one hull instead (vr_box3d_gun_pieces 0 or 1, or no pieces made). Made once for
// each key; `corners(out)` gives the triangles, asked only then.
template <typename Corners>
[[nodiscard]] const za::Vector<b3HullData*>& gunPieces(const PropHullKey& key, Corners&& corners)
{
    const int most = za::min(static_cast<int>(vr_box3d_gun_pieces.value), maxBodyShapes);
    if(most != world->gunPiecesMost) // (the setting changed: made again, for the guns made from now on)
    {
        for(auto& [made, hulls] : world->gunPieces)
        {
            for(b3HullData* hull : hulls)
            {
                b3DestroyHull(hull);
            }
        }
        world->gunPieces.clear();
        world->gunReports.clear();
        world->gunPiecesMost = most;
    }
    auto it = world->gunPieces.find(key);
    if(it != world->gunPieces.end())
    {
        return it->second;
    }
    za::Vector<b3HullData*> hulls;
    if(most < 2)
    {
        return world->gunPieces.emplace(key, ZA_MOVE(hulls)).first->second;
    }
    // Made once a session (gunShapes), its hulls once a world (in metres).
    const GunShapeCache::Entry* made = nullptr;
    for(const GunShapeCache::Entry& e : gunShapes.entries)
    {
        if(e.key == key && e.most == most)
        {
            made = &e;
        }
    }
    za::Vector<glm::vec3>& triangles = scratch.propVerts;
    if(!made && corners(triangles))
    {
        convex::Settings settings;
        settings.maxPieces = most;
        settings.verbose = vr_debug_box3d.value >= 2.f;
        za::Vector<convex::Piece> pieces;
        GunShapeCache::Entry entry{key, most, static_cast<int>(gunShapes.pieceEnds.size()), 0, {}};
        const bool fromDisk = cutGun(triangles, settings, pieces, entry.report, key.model->name);
        for(const convex::Piece& piece : pieces)
        {
            gunShapes.points.emplaceBackRange(piece.points.data(), piece.points.size());
            gunShapes.pieceEnds.pushBack(static_cast<int>(gunShapes.points.size()));
        }
        entry.pieces = static_cast<int>(pieces.size());
        gunShapes.entries.pushBack(entry);
        made = &gunShapes.entries.back();
        if(vr_debug_box3d.value)
        {
            Con_Printf("box3d: %s: %d convex pieces in %.1f ms%s; the most a hull lies off the gun %.2f units (one hull: "
                       "%.2f)\n",
                key.model->name, entry.pieces, entry.report.ms, fromDisk ? " (read from the disk)" : "", entry.report.gap,
                entry.report.wholeGap);
        }
    }
    if(made)
    {
        za::Vector<b3Vec3> points;
        for(int i = made->firstPiece; i < made->firstPiece + made->pieces; i++)
        {
            points.clear();
            for(int k = i > 0 ? gunShapes.pieceEnds[static_cast<size_t>(i - 1)] : 0; k < gunShapes.pieceEnds[static_cast<size_t>(i)]; k++)
            {
                points.pushBack(world->toM(gunShapes.points[static_cast<size_t>(k)]));
            }
            if(b3HullData* hull = convex::hull(points.data(), static_cast<int>(points.size()), static_cast<int>(points.size())))
            {
                hulls.pushBack(hull);
            }
        }
        world->gunReports[key] = made->report;
    }
    return world->gunPieces.emplace(key, ZA_MOVE(hulls)).first->second;
}
[[nodiscard]] const za::Vector<b3HullData*>& mirroredPieces(const PropHullKey& baseKey, const za::Vector<b3HullData*>& base);
[[nodiscard]] const b3HullData* magazineHull(const qmodel_t* mag, int code, const za::Vector<glm::vec3>& vertices);
[[nodiscard]] bool isGun(edict_t* ent);

// The props' and actors' hulls made this session (vr_physics_hull_stats): how many, their time on the calling thread,
// the slowest ones (VRTRACE_20261016: 13 frames of 6-7.5 ms building them in the sync).
struct HullBuildStats
{
    int built{0};
    int posted{0}, ahead{0};   // built on the pool ahead (prebuildPropHulls), and taken by propHull
    int same{0}, differed{0}; // vr_box3d_hulls_ahead 2: built here too, compared
    double ms{0.0}, mostMs{0.0};
    struct Slow
    {
        char model[48]{};
        int frame{0};
        double ms{0.0};
        double time{0.0}; // the server's
    };
    za::Array<Slow, 8> slowest{}; // by ms, the slowest first
};
inline HullBuildStats hullBuilds;
[[nodiscard]] bool propHullPoints(edict_t* ent, za::Vector<b3Vec3>& points);
[[nodiscard]] b3HullData* propHullOf(const za::Vector<b3Vec3>& points, int budget, float m2u);
[[nodiscard]] b3HullData* propHull(edict_t* ent, qmodel_t* model, const glm::vec3& lo, const glm::vec3& hi);
[[nodiscard]] b3HullData* actorHull(edict_t* ent, qmodel_t* model);
[[nodiscard]] b3ShapeDef shapeDef(int num, uint64_t category, uint64_t mask);
[[nodiscard]] bool isChunk(const qmodel_t* model);
[[nodiscard]] bool chunkPair(b3ShapeId a, b3ShapeId b);
void addChunkShape(edict_t* ent, int num, b3BodyId body);
[[nodiscard]] const qmodel_t* seatedMagazine(edict_t* ent, const qmodel_t* model);
[[nodiscard]] bool isRigid(edict_t* ent);

[[nodiscard]] bool isCorpse(edict_t* ent, int num); // (below)
[[nodiscard]] int corpseMode();
[[nodiscard]] bool wantsRagdoll(edict_t* ent, int num); // (below: "Ragdolls")
[[nodiscard]] Kind kindOf(edict_t* ent, int num, const za::Vector<uint8_t>& carried);
[[nodiscard]] za::Array<float, 9> scaleFields(edict_t* ent);
constexpr float corpseTop = 24.f;    // units: a corpse's box no taller (VR_Corpse_Arm's lying box's most)
constexpr float corpseStep = 16.f;   // units: a corpse a player or monster steps over is this high to them (Quake's step: 18)
[[nodiscard]] int corpseMode();
[[nodiscard]] bool deadMonster(edict_t* ent);
[[nodiscard]] bool knockedDown(edict_t* ent);

[[nodiscard]] glm::vec3 ownMotion(int num); // (below)
void watchCorpses();
[[nodiscard]] glm::vec3 ownMotion(int num);
[[nodiscard]] glm::vec3 deathMotion(int num);
[[nodiscard]] bool isCorpse(edict_t* ent, int num);
void corpseBox(edict_t* ent, glm::vec3& lo, glm::vec3& hi);
[[nodiscard]] uint64_t corpseMask();
[[nodiscard]] const za::Vector<b3HullData*>& corpseHulls(edict_t* ent, qmodel_t* model);
[[nodiscard]] bool corpsePushable(edict_t* ent);
int shrunkCorners(b3ShapeId shape, const b3WorldTransform& xf, float sink, za::Array<b3Vec3, B3_MAX_SHAPE_CAST_POINTS>& points);
void overlapShrunk(b3BodyId body, float sink, b3QueryFilter filter, b3OverlapResultFcn* fcn, void* context);
void noteCorpseInside(int num, const Slot& s);
void addCorpseShapes(edict_t* ent, int num, qmodel_t* model, Slot& s);

// Its settings: the global ones (Gibs and Corpses > Ragdolls), each replaced for a monster class by its own when that is
// not -1 (Ragdolls > Grunt: vr_ragdoll_army_*; > Knight: vr_ragdoll_knight_*). A row a class.
enum class Tune : uint8_t
{
    Start,
    Mass,
    Friction,
    JointFriction,
    JointStiffness,
    Limits,
    Damping,
    Blast,
    Inherit,
    Count
};

struct RagdollClass
{
    const char* classname;
    // The head its QC throws (ThrowHead): its prop's Mass -1 weighs what this class's head cut off its ragdoll does
    // (headPropMass). The mummy's is the zombie's model, weighed as the zombie's.
    const char* head;
    za::Array<cvar_t*, static_cast<za::SizeT>(Tune::Count)> own;
};

inline const za::Array<cvar_t*, static_cast<za::SizeT>(Tune::Count)> ragdollGlobals{&vr_ragdoll_start, &vr_ragdoll_mass,
    &vr_ragdoll_friction, &vr_ragdoll_joint_friction, &vr_ragdoll_joint_stiffness, &vr_ragdoll_limits, &vr_ragdoll_damping,
    &vr_ragdoll_blast, &vr_ragdoll_inherit};

inline const RagdollClass ragdollClasses[] = {
    {"monster_army", "progs/h_guard.mdl",
        {&vr_ragdoll_army_start, &vr_ragdoll_army_mass, &vr_ragdoll_army_friction, &vr_ragdoll_army_joint_friction,
            &vr_ragdoll_army_joint_stiffness, &vr_ragdoll_army_limits, &vr_ragdoll_army_damping, &vr_ragdoll_army_blast,
            &vr_ragdoll_army_inherit}},
    {"monster_knight", "progs/h_knight.mdl",
        {&vr_ragdoll_knight_start, &vr_ragdoll_knight_mass, &vr_ragdoll_knight_friction,
            &vr_ragdoll_knight_joint_friction, &vr_ragdoll_knight_joint_stiffness, &vr_ragdoll_knight_limits,
            &vr_ragdoll_knight_damping, &vr_ragdoll_knight_blast, &vr_ragdoll_knight_inherit}},
    {"monster_ogre", "progs/h_ogre.mdl",
        {&vr_ragdoll_ogre_start, &vr_ragdoll_ogre_mass, &vr_ragdoll_ogre_friction, &vr_ragdoll_ogre_joint_friction,
            &vr_ragdoll_ogre_joint_stiffness, &vr_ragdoll_ogre_limits, &vr_ragdoll_ogre_damping, &vr_ragdoll_ogre_blast,
            &vr_ragdoll_ogre_inherit}},
    {"monster_ogre_marksman", "progs/h_ogre.mdl",
        {&vr_ragdoll_ogre_start, &vr_ragdoll_ogre_mass, &vr_ragdoll_ogre_friction, &vr_ragdoll_ogre_joint_friction,
            &vr_ragdoll_ogre_joint_stiffness, &vr_ragdoll_ogre_limits, &vr_ragdoll_ogre_damping, &vr_ragdoll_ogre_blast,
            &vr_ragdoll_ogre_inherit}},
    {"monster_enforcer", "progs/h_mega.mdl",
        {&vr_ragdoll_enforcer_start, &vr_ragdoll_enforcer_mass, &vr_ragdoll_enforcer_friction,
            &vr_ragdoll_enforcer_joint_friction, &vr_ragdoll_enforcer_joint_stiffness, &vr_ragdoll_enforcer_limits,
            &vr_ragdoll_enforcer_damping, &vr_ragdoll_enforcer_blast, &vr_ragdoll_enforcer_inherit}},
    {"monster_hell_knight", "progs/h_hellkn.mdl",
        {&vr_ragdoll_hknight_start, &vr_ragdoll_hknight_mass, &vr_ragdoll_hknight_friction,
            &vr_ragdoll_hknight_joint_friction, &vr_ragdoll_hknight_joint_stiffness, &vr_ragdoll_hknight_limits,
            &vr_ragdoll_hknight_damping, &vr_ragdoll_hknight_blast, &vr_ragdoll_hknight_inherit}},
    {"monster_dog", "progs/h_dog.mdl",
        {&vr_ragdoll_dog_start, &vr_ragdoll_dog_mass, &vr_ragdoll_dog_friction, &vr_ragdoll_dog_joint_friction,
            &vr_ragdoll_dog_joint_stiffness, &vr_ragdoll_dog_limits, &vr_ragdoll_dog_damping, &vr_ragdoll_dog_blast,
            &vr_ragdoll_dog_inherit}},
    {"monster_wizard", "progs/h_wizard.mdl",
        {&vr_ragdoll_wizard_start, &vr_ragdoll_wizard_mass, &vr_ragdoll_wizard_friction,
            &vr_ragdoll_wizard_joint_friction, &vr_ragdoll_wizard_joint_stiffness, &vr_ragdoll_wizard_limits,
            &vr_ragdoll_wizard_damping, &vr_ragdoll_wizard_blast, &vr_ragdoll_wizard_inherit}},
    {"monster_zombie", "progs/h_zombie.mdl",
        {&vr_ragdoll_zombie_start, &vr_ragdoll_zombie_mass, &vr_ragdoll_zombie_friction,
            &vr_ragdoll_zombie_joint_friction, &vr_ragdoll_zombie_joint_stiffness, &vr_ragdoll_zombie_limits,
            &vr_ragdoll_zombie_damping, &vr_ragdoll_zombie_blast, &vr_ragdoll_zombie_inherit}},
    {"monster_demon1", "progs/h_demon.mdl",
        {&vr_ragdoll_demon_start, &vr_ragdoll_demon_mass, &vr_ragdoll_demon_friction, &vr_ragdoll_demon_joint_friction,
            &vr_ragdoll_demon_joint_stiffness, &vr_ragdoll_demon_limits, &vr_ragdoll_demon_damping,
            &vr_ragdoll_demon_blast, &vr_ragdoll_demon_inherit}},
    {"monster_shambler", "progs/h_shams.mdl",
        {&vr_ragdoll_shambler_start, &vr_ragdoll_shambler_mass, &vr_ragdoll_shambler_friction,
            &vr_ragdoll_shambler_joint_friction, &vr_ragdoll_shambler_joint_stiffness, &vr_ragdoll_shambler_limits,
            &vr_ragdoll_shambler_damping, &vr_ragdoll_shambler_blast, &vr_ragdoll_shambler_inherit}},
    {"monster_gremlin", "progs/h_grem.mdl",
        {&vr_ragdoll_gremlin_start, &vr_ragdoll_gremlin_mass, &vr_ragdoll_gremlin_friction,
            &vr_ragdoll_gremlin_joint_friction, &vr_ragdoll_gremlin_joint_stiffness, &vr_ragdoll_gremlin_limits,
            &vr_ragdoll_gremlin_damping, &vr_ragdoll_gremlin_blast, &vr_ragdoll_gremlin_inherit}},
    // Dawn of the Machine's ranged knight (QC vr_mg3_rknight.qc): a death knight's build, the death knight's settings.
    {"monster_ranged_knight", "progs/h_hellkn.mdl",
        {&vr_ragdoll_hknight_start, &vr_ragdoll_hknight_mass, &vr_ragdoll_hknight_friction,
            &vr_ragdoll_hknight_joint_friction, &vr_ragdoll_hknight_joint_stiffness, &vr_ragdoll_hknight_limits,
            &vr_ragdoll_hknight_damping, &vr_ragdoll_hknight_blast, &vr_ragdoll_hknight_inherit}},
    {"monster_mummy", nullptr,
        {&vr_ragdoll_mummy_start, &vr_ragdoll_mummy_mass, &vr_ragdoll_mummy_friction, &vr_ragdoll_mummy_joint_friction,
            &vr_ragdoll_mummy_joint_stiffness, &vr_ragdoll_mummy_limits, &vr_ragdoll_mummy_damping,
            &vr_ragdoll_mummy_blast, &vr_ragdoll_mummy_inherit}},
    {"monster_shalrath", "progs/h_shal.mdl",
        {&vr_ragdoll_vore_start, &vr_ragdoll_vore_mass, &vr_ragdoll_vore_friction, &vr_ragdoll_vore_joint_friction,
            &vr_ragdoll_vore_joint_stiffness, &vr_ragdoll_vore_limits, &vr_ragdoll_vore_damping, &vr_ragdoll_vore_blast,
            &vr_ragdoll_vore_inherit}},
    // Dawn of the Machine's super shambler: the shambler's settings (Ragdoll Settings > Shambler) and head gib.
    {"monster_super_shambler", "progs/h_shams.mdl",
        {&vr_ragdoll_shambler_start, &vr_ragdoll_shambler_mass, &vr_ragdoll_shambler_friction,
            &vr_ragdoll_shambler_joint_friction, &vr_ragdoll_shambler_joint_stiffness, &vr_ragdoll_shambler_limits,
            &vr_ragdoll_shambler_damping, &vr_ragdoll_shambler_blast, &vr_ragdoll_shambler_inherit}},
    {"monster_scourge", "progs/h_scourg.mdl",
        {&vr_ragdoll_centroid_start, &vr_ragdoll_centroid_mass, &vr_ragdoll_centroid_friction,
            &vr_ragdoll_centroid_joint_friction, &vr_ragdoll_centroid_joint_stiffness, &vr_ragdoll_centroid_limits,
            &vr_ragdoll_centroid_damping, &vr_ragdoll_centroid_blast, &vr_ragdoll_centroid_inherit}},
};
[[nodiscard]] float tune(const edict_t* ent, Tune t);

void blastRagdoll(RagdollBodies& r, const glm::vec3& at, float damage); // (below)

// Active deaths (vr_ragdoll_active; vr_ragactive.hpp; ROUND22.md, "Active ragdolls: core and deaths"): the classes whose
// death animation their ragdoll plays (the most common, most humanoid ones: their rigs' arms and legs are two-bone
// limbs), and whether they may crawl. The others go limp as before (Go Limp At).
struct ActiveClass
{
    const char* classname;
    bool crawls;
};
[[nodiscard]] const ActiveClass* activeClassOf(const edict_t* ent);
[[nodiscard]] int ragdollCount();
[[nodiscard]] bool wantsRagdoll(edict_t* ent, int num);
[[nodiscard]] glm::quat zTo(const glm::vec3& d);

[[nodiscard]] RagdollBodies* ragdollOf(int num); // (below)
[[nodiscard]] bool isRagdollSlot(int num);
[[nodiscard]] bool ragdollsInside(int a, int b);
void pruneRagdollsInside();

bool cutHead(RagdollBodies& r, edict_t* ent, const glm::vec3& blade, bool launch); // (below)
bool cutLimb(RagdollBodies& r, edict_t* ent, int bone, const glm::vec3& blade, bool launch);

// The QC globals a call into QC from the engine's middle (a builtin, the physics) may clobber and its caller may still
// read: the parameters and return value, self, other, msg_entity and the last trace's results. (Not the rest: Killed's
// counters, killed_monsters and the like, are meant to change.)
struct QcCallGuard
{
    globalvars_t saved;
    QcCallGuard() { memcpy(&saved, pr_global_struct, sizeof saved); }
    ~QcCallGuard()
    {
        globalvars_t& g = *pr_global_struct;
        memcpy(g.pad, saved.pad, sizeof g.pad);
        g.self = saved.self;
        g.other = saved.other;
        g.msg_entity = saved.msg_entity;
        g.trace_allsolid = saved.trace_allsolid;
        g.trace_startsolid = saved.trace_startsolid;
        g.trace_fraction = saved.trace_fraction;
        VectorCopy(saved.trace_endpos, g.trace_endpos);
        VectorCopy(saved.trace_plane_normal, g.trace_plane_normal);
        g.trace_plane_dist = saved.trace_plane_dist;
        g.trace_ent = saved.trace_ent;
        g.trace_inopen = saved.trace_inopen;
        g.trace_inwater = saved.trace_inwater;
    }
};
[[nodiscard]] bool roomRefused(int num);
[[nodiscard]] int heldLoadout();
void prepareHeldGuns();
bool makeRagdollRoom(edict_t* ent);
[[nodiscard]] b3HullData* boneHull(const ragdoll::Bone& bone, float k, float& volume);
[[nodiscard]] uint32_t neckBones(const RagdollBodies& r);
[[nodiscard]] ragactive::Parts activeParts(const RagdollBodies& r);

// Active ragdolls' dismemberment (ROUND22.md, "Active ragdolls: dismemberment"; vr_ragactive.cpp cutReaction): the joint
// ragdollCutLimb is cutting, while its ragdoll is made (-1 none).
inline int cutDeathJoint = -1;
bool createRagdoll(edict_t* ent, int num, Slot& s, bool now = false);
void driveShove(RagdollBodies& r, float dt);
void feedRagdoll(edict_t* ent, Slot& s);
float activeFloorBelow(const glm::vec3& at, float depth);

[[nodiscard]] int liquidAt(const glm::vec3& p); // (below: "Ragdolls and corpses in water")

// The liquid a drive of entity `num` feels (activeLiquidAt; a test's swim on dry land, vr_ragdoll_act: that liquid
// everywhere for its drive, none for its buoyancy).
using LiquidFn = ragactive::Liquid (*)(const glm::vec3&);
[[nodiscard]] LiquidFn liquidFor(int num);

[[nodiscard]] bool standSpot(edict_t* ent, const glm::vec3& from, const glm::vec3& at, const glm::vec3& mins,
    const glm::vec3& maxs, float range, glm::vec3& out, bool anywhere); // (below)
void driveActive(Slot& s, float dt);
[[nodiscard]] float shockLeft(edict_t* ent);
void shockRagdoll(edict_t* ent, Slot& s, float dt);
void writeRagdoll(edict_t* ent, Slot& s);
void updateRecoveries();
[[nodiscard]] bool standSpot(edict_t* ent, const glm::vec3& from, const glm::vec3& at, const glm::vec3& mins,
    const glm::vec3& maxs, float range, glm::vec3& out, bool anywhere = false);
void blastRagdoll(RagdollBodies& r, const glm::vec3& at, float damage);
[[nodiscard]] b3BodyId ragdollPartNear(int num, const glm::vec3& at);
bool cutLimb(RagdollBodies& r, edict_t* ent, int bone, const glm::vec3& blade, bool launch);
bool cutHead(RagdollBodies& r, edict_t* ent, const glm::vec3& blade, bool launch);
void dropGrabsOf(int num);
[[nodiscard]] RagdollBodies* ragdollOf(int num);
[[nodiscard]] int nearestPart(const RagdollBodies& r, const glm::vec3& at, float& distance);
void syncRagdollGrabs(float dt);
[[nodiscard]] float grabReach(int player, int hand);
inline unsigned shapeGeneration = 1;

void writeProp(edict_t* ent, Slot& s);
[[nodiscard]] b3Quat rotationOf(edict_t* ent, const Slot& s);
[[nodiscard]] bool graced(int player, int num);
bool isSmallGib(edict_t* ent);
float monsterDropAge(edict_t* ent);
[[nodiscard]] bool smallGibSpared(int num);
void traceGibContacts();
[[nodiscard]] bool bornInside(int a, int b);
void noteThrows();
void syncHands(float dt);
constexpr float restUp = 0.3f;         // a prop rests on a hand where their contact's normal (up into it) is this steep (restsOnHand)
[[nodiscard]] World::HandBody* reachOf(b3BodyId body, int player);
[[nodiscard]] bool reachSkips(const World::HandBody& hb, int player, int num);
[[nodiscard]] float pushKeep(Slot& s, float pusher, float m, float gain, float dt);
[[nodiscard]] float reachMass(const World::HandBody& hb);
void syncReach(float dt);
void feedProp(edict_t* ent, Slot& s);
void feedCorpse(edict_t* ent, Slot& s);
void writeCorpse(edict_t* ent, Slot& s);
void settleCorpses();
void keepPendingHulls();
void syncEntities(float dt);
[[nodiscard]] int liquidAt(const glm::vec3& p);
[[nodiscard]] edict_t* supportOf(const Slot& s);
void writeProp(edict_t* ent, Slot& s);
[[nodiscard]] bool reachMeets(b3ShapeId reach, b3ShapeId other);
[[nodiscard]] bool passesGun(int round, int gun);
[[nodiscard]] int partsOverlapping(const RagdollBodies& r, int& pairs, int& deep);
[[nodiscard]] int armsInsideChest(const RagdollBodies& r, int& samples);
bool shouldCollide(b3ShapeId a, b3ShapeId b, void*);
bool preSolve(b3ShapeId a, b3ShapeId b, b3Pos point, b3Vec3 normal, void*);
void syncPortalCopies(float dt);
void carryRagdolls();
void carryCorpses();
void feedPortalCopies();
void finishPortalCopies();
[[nodiscard]] float pusherMass(b3ShapeId shape);
void notePushed(float dt);
void limitPushes(float dt);
void updateSettings();
void destroyWorld();
void buildWorld();

struct VmScope
{
    qcvm_t* old{nullptr};
    VmScope() { PR_PushQCVM(&sv.qcvm, &old); }
    ~VmScope() { PR_PopQCVM(old); }
};
void addProfile(b3Profile& a, const b3Profile& b);

// vr_physics_player [onto <number | classname>]: the first player's origin, velocity and ground (FL_ONGROUND,
// .groundentity) and what is under his feet, for tests of standing on props; with onto, first put on top of that
// entity's box (the first of them), still, noclip off; with near, on the floor beside it (see below).
void playerOverProp(edict_t* p); // (below, with the players' shape against props)
void watchInside();
void registerCommands();
[[nodiscard]] bool wanted();
[[nodiscard]] b3BodyId toolJointBody(int num);
void syncToolJoints();
void beforeStanding();
[[nodiscard]] bool solidProp(int num);
void nudgeWalkedRagdolls(float dt);
void shoveBumped(float dt);
void unstickProps();
void pressStanding(float dt);
void rideStanding();

// Players against solid props' shapes (vr_box3d_player_shape; ROUND21.md, "Standing on props 3"). Quake met a solid
// prop with the upright box round it (solidBox): a tilted box was taller and wider than drawn (landing on it, the player
// stood in the air over its face; jumping onto it, he met its box's top corner), and walking into a box turned 45
// degrees stopped him further from its face than from one lying with the grid. Now the player's own box meets the prop's
// drawn box as it is turned, and as a round column (a 16-sided prism: his width across its flats, his box's height,
// flat-bottomed as his box is), so he stops as far from a box's face turned any way as from a wall's (his box's
// half-width). The trace is the column's origin against the Minkowski sum of the prop's box and the column, as Quake
// traces its hulls: the half-spaces of every face normal and edge-pair normal of the two, entered and left along the
// move, stopped DIST_EPSILON short (Quake 3's brush trace).

constexpr int columnSides = 16;
constexpr double traceEpsilon = 0.03125; // DIST_EPSILON (world.c): how far short of the surface a trace stops

struct PropShape
{
    glm::dvec3 centre{0.0};      // the prop's drawn box: its centre, its axes, half its size
    za::Array<glm::dvec3, 3> axes{};
    glm::dvec3 half{0.0};
    za::Array<glm::dvec2, columnSides> ring{}; // the player's column round his origin: its corners across ...
    double bottom{0.0}, top{0.0};               // ... from his feet to his head
    za::Array<glm::dvec3, 2 * (4 + columnSides / 2 + 3 * (1 + columnSides / 2))> normals{};
    int count{0};
};

// Shots and missiles against solid props' shapes (vr_box3d_shot_shape; ROUND21.md, "Missiles meet a turned prop's
// shape"): a shot's or a missile's box (a point, mostly) swept against the prop's drawn box as it is turned, as Quake
// traces a box against a hull: the half-spaces of their Minkowski sum (the prop's 3 axes; with a box, the world's 3 and
// the 9 edge pairs too), exact and cheap (a point: 6 planes).
struct ShotShape
{
    glm::dvec3 centre{0.0}; // the prop's drawn box (less the mover's box's offset from its origin): its centre, axes, half its size
    za::Array<glm::dvec3, 3> axes{};
    glm::dvec3 half{0.0};
    glm::dvec3 ext{0.0}; // half the mover's box
    za::Array<glm::dvec3, 2 * 15> normals{};
    int count{0};
};
[[nodiscard]] PropShape propShape(edict_t* touch, const Slot& s, const float* mins, const float* maxs);
[[nodiscard]] double propSupport(const PropShape& p, const glm::dvec3& n);
[[nodiscard]] double propSupport(const ShotShape& p, const glm::dvec3& n);

// Quake 3's brush trace of the player's origin (or a shot's) from start to end against the sum's half-spaces. False:
// missed.
template <typename Shape>
[[nodiscard]] bool traceProp(const Shape& p, const glm::dvec3& start, const glm::dvec3& end, trace_t& trace)
{
    double enter = -1.0, leave = 1.0;
    bool startOut = false, getOut = false;
    glm::dvec3 normal{0.0, 0.0, 1.0};
    double dist = 0.0;
    for(int k = 0; k < p.count; k++)
    {
        const glm::dvec3& n = p.normals[static_cast<size_t>(k)];
        const double h = propSupport(p, n);
        const double d1 = glm::dot(n, start) - h, d2 = glm::dot(n, end) - h;
        getOut = getOut || d2 > 0.0;
        startOut = startOut || d1 > 0.0;
        if(d1 > 0.0 && (d2 >= traceEpsilon || d2 >= d1))
        {
            return false; // (in front of this face all along)
        }
        if(d1 <= 0.0 && d2 <= 0.0)
        {
            continue;
        }
        if(d1 > d2)
        {
            const double f = (d1 - traceEpsilon) / (d1 - d2);
            if(f > enter)
            {
                enter = f;
                normal = n;
                dist = h;
            }
        }
        else
        {
            leave = za::min(leave, (d1 + traceEpsilon) / (d1 - d2));
        }
    }
    if(!startOut)
    {
        // Started inside: as Quake's hull trace, the move isn't cut (VR_PropLetsOut may let him out).
        trace.startsolid = true;
        trace.allsolid = !getOut;
        trace.fraction = 1.f;
        return true;
    }
    if(enter >= leave || enter <= -1.0)
    {
        return false;
    }
    const double f = za::max(enter, 0.0);
    trace.fraction = static_cast<float>(f);
    for(int k = 0; k < 3; k++)
    {
        trace.endpos[k] = static_cast<float>(start[k] + f * (end[k] - start[k]));
        trace.plane.normal[k] = static_cast<float>(normal[k]);
    }
    trace.plane.dist = static_cast<float>(dist);
    return true;
}
[[nodiscard]] double columnGap(const PropShape& p, const glm::dvec3& origin);
void playerOverProp(edict_t* p);

// vr_physics_inside [1 | 0]: a test's watch on players passing into solid props (ROUND21.md, "Phasing through a
// toppled box"). Each frame, a player's origin inside a solid prop's drawn box grown by his column (where none of his
// moves may go, vr_box3d_player_shape) is counted, and the first frame of each time in is printed.
struct InsideWatch
{
    bool on{false};
    int frames{0}, inside{0}, times{0}, longest{0};
    double deepest{0.0};
    za::Array<int, MAX_SCOREBOARD + 1> in{}, run{}; // (the prop each player is in, 0: none; for how many frames)
};
inline InsideWatch insideWatch;
void watchInside();
void pressClimbing();
[[nodiscard]] ShotShape shotShape(const edict_t* touch, const Slot& s, const float* mins, const float* maxs);
} // namespace box3d::detail

using namespace ::box3d::detail;

// What the API part's anonymous namespace shared with other parts (as it was: visible in qvr::box3d).
namespace qvr::box3d
{
namespace detail
{
void ownBox(const Slot& s, glm::vec3& lo, glm::vec3& hi);
[[nodiscard]] bool boxInLevel(const glm::vec3& lo, const glm::vec3& hi, const glm::vec3& at, const glm::quat& rot, float in = 1.f);
} // namespace detail
using namespace detail;
} // namespace qvr::box3d

// The API's functions that one part defines and another calls (not in vr_box3d.hpp).
namespace qvr::box3d
{
bool canRagdoll(edict_t* ent);
float headPropMass(const char* model);
int nearestProp();
float propMass(edict_t* ent);
bool push(edict_t* ent, const glm::vec3& at, const glm::vec3& velocity, float pusherMass);
edict_t* spawnClass(const char* classname, const glm::vec3& origin, const glm::vec3& angles, const char* model,
    const char* key, float value, bool probe);
} // namespace qvr::box3d
