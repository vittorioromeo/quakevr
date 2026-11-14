// vr_mpavatar.hpp -- other players' VR state (QVR_SVC_AVATAR): their heads, and their hands and held weapons as their own
// clients drew them (the jointed hand's place, its fingers' curls and thumb; the weapon's model, frame and place), kept per
// player and drawn on his entity (MP_PLAN.md, B2b). The server writes them (vr_server.cpp writeAvatars) to each client
// that sees that player. The body under them (vr_avatar.cpp's, posed from them) is the next phase's.

#pragma once

#include "vr_engine.hpp"
#include "vr_move.hpp"

namespace qvr::mpavatar
{

struct Avatar
{
    bool valid{false};
    double received{0.0}; // realtime it came
    bool flat{false};     // a flatscreen player
    glm::vec3 head{0.f};  // from his entity's origin
    glm::vec3 headAngles{0.f};
    VrDrawnHand hands[2]; // [0] off hand, [1] main hand; places from his entity's origin
    int weapon{0};
    int weapon2{0};
};

// vr_client.cpp's dispatch: QVR_SVC_AVATAR's body (after its sub-command byte).
void parse();

// Player entity `ent`'s avatar (1..MAX_SCOREBOARD), or nullptr: none received.
[[nodiscard]] const Avatar* of(int ent);

// VR_AddMpAvatars (view.c, after the view's entities): each other player's hands and weapons in sight to cl_visedicts
// (vr_mp_avatar_draw; multiplayer only).
void addEntities();

// VR_AliasBonePoses: an avatar's jointed hand's skinning matrices (their count; 0: not one).
[[nodiscard]] int bonePoses(const entity_t* e, const float** matrices);
// VR_AliasMirrored: an avatar's entity drawn mirrored (an off hand, a left-handed weapon).
[[nodiscard]] bool mirrored(const entity_t* e);

// A new connection: no avatars.
void clear();

// vr_mp_avatars (Debug > Reports > Network: Player Avatars; the harness's mp_avatar): the avatars this client has and
// draws, its own hands as drawn and, hosting, the server's view of each player and the avatars sent.
void init();

} // namespace qvr::mpavatar
