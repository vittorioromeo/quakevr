#pragma once

// vr_selfshot.hpp -- shooting yourself in VR (vr_selfshot; ROUND24.md, "Shooting yourself"). The player's own drawn
// body and hands as capsules (the wounds' hit geometry, vr_wounds.cpp: where an enemy's hit is painted on you), and a
// segment's first meeting with them: a hand's shots (QC selfhit_segment) and its missiles' flights (SV_PushEntity:
// VR_SelfShotMove) meet the shooter's own torso, legs, forearms and hands, and his head (round the eyes, current()).

#include "vr_engine.hpp" // entity_t, edict_t, trace_t

#include "Zancle/Container/InPlaceVector.hpp"

namespace qvr::selfshot
{

// The parts (QC vr_selfshot.qc QVR_SS_PART_*): a skip mask is 1 << part.
enum Part : int
{
    PartTorso = 0,
    PartLegs = 1,
    PartOffForearm = 2,
    PartOffHand = 3,
    PartMainForearm = 4,
    PartMainHand = 5,
    PartHead = 6, // (your own shots only: current(); the wounds' capsules, build(), have none)
};

struct Capsule
{
    glm::vec3 a{0.f}, b{0.f};
    float r{1.f};
    int part{PartTorso};
};

using Capsules = za::InPlaceVector<Capsule, 7>;

// The capsules round the drawn body (own[0], its origin the pelvis) and hands (own[1 + hand]; the posed forearms when
// the body is posed): the torso and the legs, each forearm from the elbow to the wrist, each hand past the wrist.
[[nodiscard]] Capsules build(entity_t* const own[3]);

// As drawn now (view::woundTargets), and the head round the eyes (the view's place and angles: PartHead).
[[nodiscard]] Capsules current();

struct Hit
{
    float fraction{1.f}; // along a..b (0: it starts inside)
    glm::vec3 at{0.f};
    glm::vec3 normal{0.f};
    int part{-1};
};

// Where the segment a..b (a sphere of `radius` swept along it) first meets one of `caps` not in `skip` (1 << part).
[[nodiscard]] bool segment(const Capsules& caps, const glm::vec3& a, const glm::vec3& b, float radius, int skip, Hit& out);

// Whether `ent` is the player this listen server's own client draws (its body and hands are the capsules), in a
// headset (not flat-screen play).
[[nodiscard]] bool isLocalPlayer(const edict_t* ent);

// The last precise wound painted on you (QC selfhit_info 1..3): how many, how far it landed from its point, its part.
void notePreciseWound(float off, int part);

// QC builtins (vr_builtins.cpp): selfhit_segment, selfhit_info, selfhit_point.
void PF_selfhit_segment();
void PF_selfhit_info();
void PF_selfhit_point();

} // namespace qvr::selfshot
