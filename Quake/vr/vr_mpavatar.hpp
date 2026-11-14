// vr_mpavatar.hpp -- other players' VR state (QVR_SVC_AVATAR): their heads, and their hands and held weapons as their own
// clients drew them (the jointed hand's place, its fingers' curls and thumb; the weapon's model, frame and place), kept per
// player and drawn on his entity (MP_PLAN.md, B2b). The server writes them (vr_server.cpp writeAvatars) to each client
// that sees that player. His body under them is vr_avatar.cpp's, posed from them by the code that poses your own (B2d:
// avatar::poseOther), sized and dressed by his calibration; it takes his player model's place (vr_mp_body).

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
    VrBodyPose body;      // how his client poses his body (its yaw, the head's height, the lean)
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

// The avatar calibration (MP_PLAN.md, B2c): what others need of a player's body and see of it, from his own settings --
// height, seated height, arm length, world scale, handedness and the gadget's arm, gloves (kind, colour), nails, skin
// tone, body (mode, build, bracers, pauldrons). Each as an integer (calFields in vr_mpavatar.cpp: metres in mm, shares
// in thousandths...), so what is sent, kept and compared is exact. The client sends his up when it changes (a
// "vr_avatarcal" client command, reliable, at most every vr_net_avatarcal_interval); the server keeps one per client
// slot and passes it on to every client (QVR_SVC_AVATARCAL, reliable), and all of them to a joining client.
// Multiplayer only.
inline constexpr int numCalFields = 16;
struct Calibration
{
    int v[numCalFields]{};
    [[nodiscard]] bool operator==(const Calibration& o) const;
};

// Player entity `ent`'s calibration as this client has it, or nullptr: none (yet, or a client that sends none).
[[nodiscard]] const Calibration* calibrationOf(int ent);
// VR_WriteMoveExtras: this client's calibration up, when changed (multiplayer only).
void sendCalibration();
// vr_client.cpp's dispatch: QVR_SVC_AVATARCAL's body.
void parseCalibration();
// The server's side: a client slot reset (connect, drop): its calibration gone, and the clients told.
void serverReset(int slot);
// The server's side: every slot's calibration into a spawning client's message (VR_WriteClientSpawnState).
void serverWriteAll(sizebuf_t* msg);

// vr_mp_avatars (Debug > Reports > Network: Player Avatars; the harness's mp_avatar): the avatars this client has and
// draws, its own hands as drawn and, hosting, the server's view of each player and the avatars sent.
void init();

} // namespace qvr::mpavatar
