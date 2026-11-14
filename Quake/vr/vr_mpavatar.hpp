// vr_mpavatar.hpp -- other players' VR state (QVR_SVC_AVATAR): their heads and hands as their own clients placed them,
// and their weapons, kept per player for drawing them (MP_PLAN.md, B2). The server writes them (vr_server.cpp
// writeAvatars) to each client that sees that player; nothing draws them yet (the next phase: the player's own body,
// vr_avatar.cpp's, posed from them on the remote player).

#pragma once

#include "vr_engine.hpp"

namespace qvr::mpavatar
{

struct Avatar
{
    bool valid{false};
    double received{0.0}; // realtime it came
    bool flat{false};     // a flatscreen player
    glm::vec3 head{0.f};  // world, from his entity's origin in the same message
    glm::vec3 headAngles{0.f};
    glm::vec3 hands[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // [0] off hand, [1] main hand; world
    glm::vec3 handAngles[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    int weapon{0};
    int weapon2{0};
};

// vr_client.cpp's dispatch: QVR_SVC_AVATAR's body (after its sub-command byte).
void parse();

// Player entity `ent`'s avatar (1..MAX_SCOREBOARD), or nullptr: none received.
[[nodiscard]] const Avatar* of(int ent);

// vr_mp_avatars (Debug > Reports > Network: Player Avatars; the harness's mp_avatar): the avatars this client has and,
// hosting, the server's view of each player.
void init();

} // namespace qvr::mpavatar
