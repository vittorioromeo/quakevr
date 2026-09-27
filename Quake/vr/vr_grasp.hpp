// vr_grasp.hpp -- fitted hands (round 21): the jointed hand's fingers (vr_handrig.hpp) wrap what the hand holds --
// a weapon's grip, the other hand's weapon's foregrip or blade, a carried box, backpack, gib, head or armour, the
// flashlight -- instead of closing into their fixed fist.
//
// The solve (round 21, second pass: on the main thread, every frame the held thing moves in the hand; tens of
// microseconds): each finger closes along its own curl path from open (curl 0) to the tightest fist (curl 4), all its
// joints together; as soon as one of its segments touches the held thing's triangles, the joints that move that
// segment stop there, and the joints past it go on closing (Miller et al., GraspIt!'s "auto-grasp"): the finger wraps
// round what it meets, joint by joint. The segments are spheres along each finger bone (fitted to the rig's vertices),
// closed by conservative advancement: each step closes as far as no sphere can reach the nearest triangle (its
// distance over how fast the joints can move it), so no step passes through anything, and a contact is found in a
// few steps. What the solve keeps, per joint, is the curl it stopped at.
//
// Each frame: a finger closes as far as the controller says (its curl), but never past where it stopped; the more
// the controller closes it (grip, trigger, thumb), the more it is drawn to the stop even past the controller's curl
// (a gripping hand holds on), up to wrapping fully at a full press. With nothing met, the curl is the controller's.

#pragma once

#include "vr_handrig.hpp"

#include <memory>
#include <vector>

namespace qvr::grasp
{

struct Triangle
{
    glm::vec3 p[3];
};

// The triangles `e` is drawn with, in world space: an alias model's pose (`frame`, or the entity's if < 0) as the
// renderer places it (vr_render.cpp's transforms, mirrored or not), a brush model's faces. False if it has none.
bool worldTriangles(const entity_t& e, bool mirrored, int frame, std::vector<Triangle>& out);

// A held thing's shape: its model's triangles in the model's own coordinates (an alias model's raw vertices of one
// pose, a brush model's), and what the solve queries them with (vr_grasp.cpp): made once per model and pose and kept.
struct Shape
{
    struct Space;
    std::vector<Triangle> tris;
    std::unique_ptr<Space> space;

    Shape();
    ~Shape();
    Shape(Shape&&) noexcept;
    Shape& operator=(Shape&&) noexcept;
};

// Forgets the shapes made (a game directory change reuses their models' slots).
void reset();

// The shape of `e`'s model at `frame` (or the entity's own if < 0); nullptr if it has none.
[[nodiscard]] const Shape* shapeOf(const entity_t& e, int frame);

// The matrix `e`'s model is drawn with: its shape's coordinates to the world (vr_render.cpp's, mirrored or not).
[[nodiscard]] glm::mat4 shapeToWorld(const entity_t& e, bool mirrored);

// What the solve found for a finger: per joint, the curl it stops at (4, the tightest fist, if nothing stops it),
// and whether anything was met at all.
struct FingerStop
{
    float stop[handrig::jointsPerFinger]{4.f, 4.f, 4.f};
    bool met{false};
    bool startsInside{false}; // in it at every curl (left as the controller has it)
    bool fromClosed{false};   // in it open: closed from the tightest curl it is clear at
    bool leastInside{false};  // the thumb in it at every turn and curl: where it is least in it
};

struct Solution
{
    FingerStop finger[handrig::FingerCount];
    glm::vec3 palm{0.f};  // the hand's move (rig space) to hold it flush, the fingers solved there
    glm::quat palmTurn{1.f, 0.f, 0.f, 0.f}; // and its turn about palmCentre, before the move
    glm::vec3 palmCentre{0.f};
    glm::quat thumbTurn{1.f, 0.f, 0.f, 0.f}; // the thumb's metacarpal turn (Pose::metacarpal) it closes at
    int thumbChoice{-1};  // which of the thumb's turns (for the next solve's preference)
    int triangles{0};     // the held thing's, within the hand's reach
    int probes{0};        // the fingers' places tested
    int places{0};        // the palm's places tried (a grip through the hand)
    double seconds{0.0};
};

struct Settings
{
    float palmLimit{0.f};     // hand units the palm may move to sit flush (0: not at all)
    float palmTurnLimit{0.f}; // degrees it may turn to face the surface in front of it
    float overlap{0.f};       // hand units the hand may sink into what it holds (snug, no gap)
    bool thenar{false};       // the ball of the thumb meets it too (a thing held against the palm; not a weapon's grip)
    bool thumbTop{false};     // the thumb along the top of what it holds, not wrapped round it
};

// Solves the hand of `pose` (its shifts; its curls and metacarpal are ignored) holding `shape`, placed in the hand's
// rig space by `shapeToRig`: the palm turned and moved flush (as the settings allow), then the fingers there.
// `previous`: the solve before for the same thing: among thumb turns nearly as good, its own wins (no flips).
// `extra`: another thing in the way (the other hand, when this one cups it), its coordinates to the rig by
// `extraToRig`, the hand allowed `extraOverlap` hand units into it (at least as much as into what it holds).
void solve(const handrig::Pose& pose, const Shape& shape, const glm::mat4& shapeToRig, const Settings& settings,
    const Solution* previous, Solution& out, Shape* extra = nullptr, const glm::mat4& extraToRig = glm::mat4{1.f},
    float extraOverlap = 0.f);

// A shape of triangles as they are given (their own coordinates: no model), for solve's `extra`.
void makeShape(const std::vector<Triangle>& tris, Shape& out);

// A finger's joint curls this frame: `curl` the controller's (0..5, vr_view.cpp's), `engage` how much it grips
// (0..1: drawn to the stops past `curl`).
void curls(const FingerStop& stop, float curl, float engage, float out[handrig::jointsPerFinger]);

// The middle of the palm's side (rig space): what the palm turns about.
[[nodiscard]] glm::vec3 palmCentre();

// The curl path's place for the controller's curl: 0..4 closing, then 5 back to 3's shape (the old frames').
[[nodiscard]] float pathCurl(float curl);

// The hand's grip channel (rig space): where a handle held in the half-closed fingers lies, the line through the
// middles of the circles the fingers curl round (`point` on it, `dir` from the little finger's side to the index's),
// and how thick a handle it is (`radius`). False if it can't be found.
bool gripChannel(const handrig::Pose& pose, glm::vec3& point, glm::vec3& dir, float& radius);

// Whether the world point `p` is inside `shape` (drawn with `shapeToWorld`) within `reach` world units of its surface
// (the nearest triangle faces away from it); `out` the move out to that surface.
bool inside(const Shape& shape, const glm::mat4& shapeToWorld, const glm::vec3& p, float reach, glm::vec3& out);

// A finger at `curls`: its joints (the knuckle, the two between) and its tip's end, rig space.
void fingerPoints(const handrig::Pose& pose, int finger, const float curls[handrig::jointsPerFinger], glm::vec3 out[4]);

// The fingertips of the hand at `pose` (rig space: each last segment's last sphere).
void fingertips(const handrig::Pose& pose, glm::vec3 out[handrig::FingerCount]);

// The spheres the hand at `pose` is tested as (rig space; w the radius): the fingers', the palm's.
void posedSpheres(const handrig::Pose& pose, std::vector<glm::vec4>& out);

// vr_grasp_spheres: prints the spheres the fingers and palm are tested as.
void spheres_f();

} // namespace qvr::grasp
