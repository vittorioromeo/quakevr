#pragma once

// The grab query (design review 5 (c)): what is under a player's hand for its grip to take, in one vocabulary and with
// one precedence. Every search that answers it is a provider of one Kind; a provider gives a Candidate (its kind, the
// thing, how far off it is in units). Kinds are numbered in precedence order: when two could take the same grip, the
// smaller number takes it. The order is today's, the frame's stages in turn (a stage's take swallows the grip or fills the
// hand for the stages after it):
//
//   client, before the move is sent (vr_input.cpp buttonChanged; the grip taken there never reaches the server):
//     ZoneDrag      a holster zone's drag (vr_holsterzones.cpp dragGrip; the editors and the arena cards before it)
//     ChainsawCord  the cord handle of the other hand's chainsaw (vr_chainsaw.cpp grip)
//     Lamp          the flashlight on the body, a gun or the other hand (vr_flashlight.cpp button, lateGrips)
//   server, before the move (sv_phys.c SV_Physics_Client):
//     Climb         a ledge, rung or tall prop's edge (vr_climb.cpp gripHand; hides the grip from the stages after)
//     Foe           a standing monster under the fist (vr_foegrab.cpp tryTake; hides the grip as climb's)
//   server, the hands' touches (vr_physics.cpp touchProviders: a thing's .handtouch runs for a hand on it, by its kind):
//     Ragdoll, Prop, Round, ArmedWeapon, Weapon, Box
//   server QC, W_Frame (QC/vr_grab.qc: the same numbers):
//     SecondHand    the other hand onto a carried prop (VR_Carry_TrySecondHand)
//     Torch         a torch on its wall near the hand (VR_WallTorch_GrabCandidate)
//     GrenadePouch  a grenade from the pouch (VR_HandGrenade_GrabCandidate)
//     AmmoPouch     a round or a magazine from the ammo pouch (VR_Reload_GrabCandidate)
//     Holster       a gun drawn from a holster, or the carried gun's grip (DoHandImpl)
//     ForceCatch    a force-grabbed thing caught as it arrives (VR_Forcegrab_FlightThink)
//
// The touch stage's providers are one table, tried in order: the first that claims a thing decides whether the hand is on
// it (a Weapon not touched by a fist that has none known passes it to the Box test). Between two things touched in one
// frame, the trace order decides (the main hand's reach first), as before.
// QC/vr_grab.qc's VR_GRAB_* are these numbers (build.sh checks nothing: keep them in step).

#include "vr_engine.hpp"

#include "Zancle/Base/IntTypes.hpp"

namespace grab
{

enum class Kind : za::U8
{
    None = 0,
    ZoneDrag = 1,
    ChainsawCord = 2,
    Lamp = 3,
    Climb = 4,
    Foe = 5,
    Ragdoll = 6,
    Prop = 7,
    Round = 8,
    ArmedWeapon = 9,
    Weapon = 10,
    Box = 11,
    SecondHand = 12,
    Torch = 13,
    GrenadePouch = 14,
    AmmoPouch = 15,
    Holster = 16,
    ForceCatch = 17,
};

struct Candidate
{
    Kind kind = Kind::None;
    edict_t* ent = nullptr;
    float distance = 0.f; // units from the hand (its fist) to the thing; 0 when the provider only tests touching

    [[nodiscard]] constexpr int priority() const { return static_cast<int>(kind); } // smaller takes the grip
    [[nodiscard]] constexpr explicit operator bool() const { return kind != Kind::None; }
};

[[nodiscard]] constexpr const char* name(Kind k)
{
    constexpr const char* names[] = {"none", "zone drag", "chainsaw cord", "lamp", "climb", "foe", "ragdoll", "prop",
        "round", "armed weapon", "weapon", "box", "second hand", "torch", "grenade pouch", "ammo pouch", "holster",
        "force catch"};
    const int i = static_cast<int>(k);
    return i >= 0 && i < static_cast<int>(sizeof(names) / sizeof(names[0])) ? names[i] : "?";
}

// The touch stage's query: whether `player`'s hand `which` (HAND_OFF, HAND_MAIN) is on `target`, and as what
// (vr_physics.cpp; physics::handOn's answer).
[[nodiscard]] Candidate touch(edict_t* target, edict_t* player, int which);

} // namespace grab
