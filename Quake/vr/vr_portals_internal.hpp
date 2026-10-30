#pragma once

// vr_portals_internal.hpp -- what vr_portals.cpp (see vr_portals.hpp) shares with its part vr_portals_gates.cpp
// (crossing a gate: players, monsters, thrown things and their pulls, the AI's images through gates). Internal:
// included by those two alone. What was the file's first anonymous namespace is portals::detail: its types here, its
// functions and variables declared here and defined in the file that owns them.

#include "vr_portals.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_gfx.hpp"
#include "vr_hands.hpp"
#include "vr_held.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_move.hpp"
#include "vr_physics.hpp"
#include "vr_progs.hpp"
#include "vr_trace.hpp"
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_stereo.hpp"
#include "vr_view.hpp"
#include "vr_geom.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Tan.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string.h>

namespace qvr::portals::detail
{

inline constexpr float kReach = 24.f;    // how far round a trigger's brush its teleporter's faces may be

inline constexpr float kStand = 24.f;    // a standing player's origin over the floor

inline constexpr float kRange = 1536.f;  // how far from the head a gate is looked through

inline constexpr float kOblique = 0.25f; // the oblique near plane's slope in the depth (see VR_PortalClip)

// A body this near a gate's plane (either side, over its opening) straddles it. The server carries a player at the start
// of a tick (VR_PortalClientCross, before the move), so the tick whose move takes his torso past the plane sends him
// still behind it, not yet carried, while the client's eye is already through (eyeThrough: drawn from the
// destination); and once carried, an eye still behind the exit's face is drawn from the source. VR_PortalAddPVS sends
// both rooms' entities while he straddles a gate: else the room he sees lost its doors, lifts and floors (start's
// pentagram floor, func_bossgate) for that one frame, seen through.
inline constexpr float kStraddle = 32.f;

// A side of a teleporter: its faces in one plane, seen from in front of it, and where they lead.
struct Side
{
    glm::vec3 normal{0.f}; // towards where it is seen from
    float dist = 0.f;
    glm::vec3 mins{0.f}, maxs{0.f};
    float area = 0.f;
    int leaf = -1;         // the leaf in front of it, in the PVS's bits (-1: none found: no PVS test)
    glm::vec3 from{0.f};   // its point carried onto `to`
    glm::vec3 to{0.f};
    glm::vec3 dest{0.f};   // the destination's point (where a player arrives)
    float yaw = 0.f;       // degrees a view's yaw turns going through
    glm::mat3 turn{1.f};   // the same turn
    int trigger = 0;       // its trigger_teleport's edict
    int sourceTarget = 0;  // Native authored retargeting invalidates the cached destination immediately.
    bool paired = false;   // it comes out of another gate's face (pairExits): its exit (reverseSide) is a real aperture
    int pair = -1;         // and that gate's side (the stealth AI's way back: aiGateFace)
};

struct PortalScratch
{
    za::Vector<gfx::Vertex> mask;
    za::Vector<byte> sourcePvs;
    za::Vector<glm::vec3> added;
    za::Vector<int> seen, nextSeen; // VR_PortalAddPVS: the sides whose rooms are sent, a gate deeper each round
    za::Vector<byte> hopPvs;
    auto members() { return mem::list(mask, sourcePvs, added, seen, nextSeen, hopPvs); }
};

struct ViewCandidate { int side = -1; float score = 0.f; };

// Views within views (vr_portals_recursion): a camera's depth is how many gates it looks through - 0 the eye's own
// camera (or the flat or spectator one), 1 a view through a gate, 2 a gate seen in that view, and so on. A camera's
// views are drawn before it (stereo::renderPortal: each view's own views first, deepest first), each into its depth's
// texture array, which its teleport faces then show.
inline constexpr int kMaxDepth = 4; // the deepest view: the first gate and three more within it (vr_portals_recursion 3)

// The views seen by one camera: its candidates, ranked, and those drawn (their array's layers in order).
struct Level
{
    za::Array<ViewCandidate, 8> candidates;
    za::Array<int, 8> renderedSides;
    int candidateViews = 0, renderedViews = 0;
    unsigned texture = 0; // their texture array (0: none drawn)
    int textureEye = -1;  // the eye it was drawn for (diagnostics)
};

using geom::vec;

// This frame's view test (update() and vr_portals_view share it, so the reasons printed are the ones acted on):
// the head, the leaf it is in and its PVS, and where it looks. Invalid when the head is not in the world.
struct ViewTest
{
    const byte* vis = nullptr;
    glm::vec3 head{0.f}, forward{0.f};
    bool valid = false;
};

inline constexpr float kAperture = 0.f;  // how far outside a gate's faces its aperture still counts as its opening

// (No cooldown after a crossing: one of 0.5 s let a player who stepped straight back into the gate he had just come out
// of, or walked on into the one he had just backed out of, take his torso past its plane uncarried - the split body
// lets it go there - and at 24 units past it the split ended, leaving him inside the wall behind the gate: the
// softlock pushing a crate in, ROUND21.md "Teleporters: shooting yourself, stuck behind a gate". A carried body comes out
// in front of the exit's plane moving away from it, so nothing bounces back; one crossing a tick at most.)

// Each client's last seamless crossing (and each monster's: vr_portals_monsters).
struct ClientState
{
    double crossed = -1e9;
    int generation = -1;
    int leavingSide = -1; // until the trailing half has cleared the destination plane
    glm::vec3 lastTorso{0.f}; // a monster's torso at its last look (VR_PortalMonsterCross: which way it went)
    bool torsoKnown = false;
};


// gates (vr_portals_gates.cpp)
inline constexpr float kCross = 24.f;    // how near the plane the body is watched for its crossing
} // namespace qvr::portals::detail

namespace qvr::portals::detail
{

// vr_portals.cpp
[[nodiscard]] bool enabled();
extern mem::Scratch<PortalScratch> scratch;
extern za::Vector<Side> sides;
extern za::Vector<Side> exits;
extern const qmodel_t* builtFor;
extern int builtGeneration;
extern float builtPairExits;
extern int chosen;
extern int lastChosen;
extern za::Array<Level, kMaxDepth + 1> levels;
extern za::Array<int, kMaxDepth + 1> path;
extern za::Array<glm::vec4, kMaxDepth + 1> rects;
extern za::Array<bool, kMaxDepth + 1> rectAll;
extern int rankDepth;
extern int nestedViews;
extern int drawDepth;
extern glm::vec3 chosenOrigin;
extern glm::vec4 eyeRect;
extern bool eyeRectAll;
void build();
[[nodiscard]] bool current();
[[nodiscard]] glm::vec3 nearestPoint(const Side& sd, const glm::vec3& p);

// vr_portals_gates.cpp
ClientState& crossingState(edict_t* ent);
[[nodiscard]] bool walkOn();
[[nodiscard]] bool monsterWalks(edict_t* ent);
[[nodiscard]] bool triggerActive(const edict_t* trig);
[[nodiscard]] bool overlaps(const edict_t* a, const edict_t* b);
[[nodiscard]] glm::vec3 carried(const Side& sd, const glm::vec3& p);
[[nodiscard]] Side reverseSide(const Side& entry);
[[nodiscard]] bool onGate(const Side& sd, const glm::vec3& onPlane, float margin);
[[nodiscard]] glm::vec3 headOf(edict_t* ent);
[[nodiscard]] glm::vec3 torsoOf(edict_t* ent);
[[nodiscard]] bool engineCarries(int t);
[[nodiscard]] float reachOf(edict_t* ent, const Side& sd, const glm::vec3& origin, float d);
[[nodiscard]] bool bodyFitsGateAt(edict_t* ent, const Side& sd, const glm::vec3& origin);
[[nodiscard]] bool bodyFitsGate(edict_t* ent, const Side& sd);
[[nodiscard]] bool inFrontOfOpposite(const Side& sd, const glm::vec3& p);
void setVec(float* out, const glm::vec3& v);
void portalCrossed(edict_t* ent, const Side& sd, edict_t* trig, const glm::vec3& from, int returnSide);
void crossPlayer(edict_t* ent, const Side& sd, edict_t* trig, ClientState& cs, int returnSide = -1);
[[nodiscard]] bool solidAt(edict_t* ent, const glm::vec3& p);
bool unstick(edict_t* ent, ClientState& cs);
void crossMonster(edict_t* ent, const Side& sd, edict_t* trig, ClientState& cs, int returnSide = -1);
[[nodiscard]] bool aiGateSide(int gate, Side& out);

} // namespace qvr::portals::detail

namespace qvr::portals
{

// vr_portals_gates.cpp
void stuck_f();
glm::vec3 aiMap(int gate, const glm::vec3& value, bool direction);
int aiGateCount();
int aiGateFlags(int gate);
int aiGateFace(int gate);
glm::vec3 aiGateVec(int gate, int what, const glm::vec3& p);
int aiImage(edict_t* observer, edict_t* target, const glm::vec3& from, const glm::vec3& point, int gate, glm::vec3& image);
void pullSearchOrigins(const glm::vec3& from, za::Vector<glm::vec3>& out, float range);
glm::vec3 pullImage(const glm::vec3& from, const glm::vec3& point, int* gate);
glm::vec3 pullImageSeen(const glm::vec3& from, const glm::vec3& point);
int aiRoute(edict_t* entity);

} // namespace qvr::portals
