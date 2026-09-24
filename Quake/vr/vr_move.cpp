// vr_move.cpp -- VR clc_move block serialization. Vectors are sent as floats: hand
// velocities and offsets need more precision than protocol coordinates give.

#include "vr_move.hpp"
#include "vr_engine.hpp"

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

[[nodiscard]] glm::vec3 readVec3()
{
    glm::vec3 v;
    v.x = MSG_ReadFloat();
    v.y = MSG_ReadFloat();
    v.z = MSG_ReadFloat();
    return v;
}

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
}

VrMove readVrMove()
{
    VrMove move;

    move.headAngles = readVec3();
    move.vrYaw = MSG_ReadFloat();

    for(VrHandMove& hand : move.hands)
    {
        hand.pos = readVec3();
        hand.rot = readVec3();
        hand.vel = readVec3();
        hand.throwVel = readVec3();
        hand.velMag = MSG_ReadFloat();
        hand.angVel = readVec3();
        hand.throwPos = readVec3();
        hand.throwAge = MSG_ReadFloat();
    }

    move.headVel = readVec3();
    move.muzzlePos[0] = readVec3();
    move.muzzlePos[1] = readVec3();
    move.vrBits0 = static_cast<std::uint16_t>(MSG_ReadShort());
    move.teleportTarget = readVec3();
    move.hotspots[0] = static_cast<std::uint8_t>(MSG_ReadByte());
    move.hotspots[1] = static_cast<std::uint8_t>(MSG_ReadByte());
    move.roomscaleMove = readVec3();
    move.buttons = static_cast<std::uint8_t>(MSG_ReadByte());

    return move;
}

} // namespace qvr
