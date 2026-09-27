// vr_move.cpp -- VR clc_move block serialization. Vectors are sent as floats: hand
// velocities and offsets need more precision than protocol coordinates give.

#include "vr_move.hpp"
#include "vr_engine.hpp"

#include <cmath>

namespace qvr
{
namespace
{

void writeVec3(sizebuf_t* buf, const glm::vec3& v)
{
    MSG_WriteFloat(buf, v.x);
    MSG_WriteFloat(buf, v.y);
    MSG_WriteFloat(buf, v.z);
}

// Reads the block's floats, noting whether each is finite: a NaN or an infinity (a broken or hostile client) would
// pass every "reject if x > limit" test downstream and end up in positions and velocities.
struct Reader
{
    bool finite{true};

    [[nodiscard]] float real()
    {
        const float f = MSG_ReadFloat();
        finite = finite && std::isfinite(f);
        return f;
    }

    [[nodiscard]] glm::vec3 vec3()
    {
        glm::vec3 v;
        v.x = real();
        v.y = real();
        v.z = real();
        return v;
    }
};

} // namespace

void writeVrMove(sizebuf_t* buf, const VrMove& move)
{
    writeVec3(buf, move.headAngles);
    MSG_WriteFloat(buf, move.vrYaw);

    for(const VrHandMove& hand : move.hands)
    {
        writeVec3(buf, hand.pos);
        writeVec3(buf, hand.rot);
        writeVec3(buf, hand.vel);
        writeVec3(buf, hand.throwVel);
        MSG_WriteFloat(buf, hand.velMag);
        writeVec3(buf, hand.angVel);
        writeVec3(buf, hand.throwPos);
        MSG_WriteFloat(buf, hand.throwAge);
    }

    writeVec3(buf, move.headVel);
    writeVec3(buf, move.muzzlePos[0]);
    writeVec3(buf, move.muzzlePos[1]);
    MSG_WriteShort(buf, move.vrBits0);
    writeVec3(buf, move.teleportTarget);
    MSG_WriteByte(buf, move.hotspots[0]);
    MSG_WriteByte(buf, move.hotspots[1]);
    writeVec3(buf, move.roomscaleMove);
    MSG_WriteByte(buf, move.buttons);
    writeVec3(buf, move.origin);
    writeVec3(buf, move.headPos);
}

std::optional<VrMove> readVrMove()
{
    Reader in;
    VrMove move;

    move.headAngles = in.vec3();
    move.vrYaw = in.real();

    for(VrHandMove& hand : move.hands)
    {
        hand.pos = in.vec3();
        hand.rot = in.vec3();
        hand.vel = in.vec3();
        hand.throwVel = in.vec3();
        hand.velMag = in.real();
        hand.angVel = in.vec3();
        hand.throwPos = in.vec3();
        hand.throwAge = in.real();
    }

    move.headVel = in.vec3();
    move.muzzlePos[0] = in.vec3();
    move.muzzlePos[1] = in.vec3();
    move.vrBits0 = static_cast<std::uint16_t>(MSG_ReadShort());
    move.teleportTarget = in.vec3();
    move.hotspots[0] = static_cast<std::uint8_t>(MSG_ReadByte());
    move.hotspots[1] = static_cast<std::uint8_t>(MSG_ReadByte());
    move.roomscaleMove = in.vec3();
    move.buttons = static_cast<std::uint8_t>(MSG_ReadByte());
    move.origin = in.vec3();
    move.headPos = in.vec3();

    if(!in.finite)
    {
        return std::nullopt;
    }
    return move;
}

} // namespace qvr
