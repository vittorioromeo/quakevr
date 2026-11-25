// vr_playerhit.hpp -- shots meet the other players' bodies as they are drawn (hitzones, 2026-11-23; his flat-screen
// deathmatch notes: a crosshair squarely on a bot's helmet missed, a shot well beside its arm hit, and a shot at its weapon
// alone hurt it). A player (a human, a bot, a splitscreen seat) is drawn as his VR body (vr_mpavatar.cpp: posed under his
// head, his arms reaching to his hands, his feet on the floor), not as his box: the box (24 wide, axis-aligned, its top at
// his box's eyes + 5) stood beside and under that body -- a 34-unit-wide target seen across its corner, a helmet above it.
//
// His hit shape: capsules laid out from what every client poses his body from (the avatar QVR_SVC_AVATAR sends: his head
// as drawn, his body's yaw, his hands; his body's size), in the drawn body's proportions (vrbody.mdl's skeleton, measured
// posed: vr_mp_avatars' mp_body line): a head sphere, the torso, each arm (shoulder, elbow, hand) and each leg (hip, knee,
// ankle). Shots, missiles and thrown things (SV_Move with MOVE_HITMODEL's guns or thrown class, MOVE_MISSILE, MOVE_HITGIBS)
// meet these instead of his box (grown by vr_hit_tolerance_guns and the mover's own radius); positional damage takes the
// zone of the capsule met (QC playerhitzone: head, body, arms the extremities, legs). Bodies moving, melee and the
// grappling hook keep his box. Show Hit Zones draws the same capsules (the host the server's, a client its rebuild from
// the same avatar). vr_hit_player_shape 0: the box, as before.

#pragma once

#include "vr_engine.hpp"

#include <glm/vec3.hpp>

namespace qvr::playerhit
{

// What his body is posed from, as offsets from his entity's origin (the avatar's: a lag-compensated shot moves them
// with his origin).
struct Pose
{
    glm::vec3 head{0.f};                // his drawn eyes
    float yaw{0.f};                     // his body's facing, degrees
    glm::vec3 hand[2]{};                // [0] off hand, [1] main hand
    bool hasHand[2]{false, false};
    float m2w{1.f / 0.0381f * 1.2f};    // world units a metre of the modelled body (units::perMetre * his scale)
    float floor{-24.f};                 // his feet (his box's bottom)
};

struct Capsule
{
    glm::vec3 a{0.f}, b{0.f}; // world
    float r{0.f};
    int zone{0};              // a VR_REGION_* (vr_region_shared.h): 0 body, 1 head, 2 an arm, 3 a leg
};

struct Shape
{
    static constexpr int most = 10;
    Capsule c[most];
    int n{0};
};

// His shape at `origin` (his entity's) from `pose`.
void build(const Pose& pose, const glm::vec3& origin, Shape& out);

// Whether shots meet player `ent`'s shape (vr_hit_player_shape, a multiplayer server's living player with an avatar); his
// shape now (the server's avatar data: vr_server.cpp avatarPose). False: his box.
[[nodiscard]] bool serverShape(edict_t* ent, Shape& out);

// The segment a..b swept by a sphere of `radius` against the shape: the first hit's fraction (0 inside at a), its point
// on the grown capsule, its normal and zone. False: none before `maxT`.
bool segment(const Shape& s, const glm::vec3& a, const glm::vec3& b, float radius, float maxT, float& t, glm::vec3& point,
    glm::vec3& normal, int& zone);

// Client entity `num`'s shape as this client draws him (no server here: rebuilt from his avatar as received, sized as his
// body is drawn), at his entity as drawn. False: none drawn.
[[nodiscard]] bool clientShape(int num, Shape& out);

void init(); // VR_Init: vr_hitzones_aim (the test: mp_dm_hitzones)

} // namespace qvr::playerhit

// world.c (SV_ClipToLinks): a shot's, missile's or thrown thing's move against player `touch`'s shape. -1: not his shape's
// to decide (his box then); 0: missed him; 1: met him (`trace` filled in).
extern "C" int VR_PlayerShotClip(edict_t* touch, const float* start, const float* mins, const float* maxs, const float* end,
    int hittype, float hitmodel, int hitgibs, trace_t* trace);
// QC float playerhitzone(entity e, vector p, vector dir): the zone (VR_REGION_*) of e's shape that a shot along `dir`
// entering at p met; -1: e has no shape (QC then decides by his box).
void PF_playerhitzone();
// QC vector playerbodypoint(entity e, float i): sample point i of e's drawn body (the shape above, world): 0 the head's
// centre, 1 the neck, 2 the torso's middle, 3 and 4 the shoulders (off, main), 5 and 6 the hands; '0 0 0': no shape or
// no such point (monsters' sight and aim at a leaning player: QC vr_leanaim.qc).
void PF_playerbodypoint();
