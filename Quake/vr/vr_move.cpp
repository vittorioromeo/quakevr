// vr_move.cpp -- VR clc_move block serialization. Vectors are sent as floats: hand
// velocities and offsets need more precision than protocol coordinates give.

#include "vr_move.hpp"
#include "vr_engine.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/IsFinite.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Vocabulary/Optional.hpp"


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
        finite = finite && ZA_ISFINITE(f);
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

// (writeDrawnHand: eighths of a unit from `origin`, a short each; angles in 16 bits; whatever the protocol's flags)
void writeEighths(sizebuf_t* buf, const glm::vec3& v, const glm::vec3& origin)
{
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteShort(buf, za::clamp(static_cast<int>(za::round((v[i] - origin[i]) * 8.f)), -32767, 32767));
    }
}

[[nodiscard]] glm::vec3 readEighths(const glm::vec3& origin)
{
    glm::vec3 v;
    for(int i = 0; i < 3; i++)
    {
        v[i] = origin[i] + static_cast<float>(MSG_ReadShort()) / 8.f;
    }
    return v;
}

void writeAngles16(sizebuf_t* buf, const glm::vec3& a)
{
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteShort(buf, static_cast<int>(za::round(a[i] * 65536.f / 360.f)) & 65535);
    }
}

[[nodiscard]] glm::vec3 readAngles16()
{
    glm::vec3 a;
    for(int i = 0; i < 3; i++)
    {
        a[i] = static_cast<float>(MSG_ReadShort()) * (360.f / 65536.f);
    }
    return a;
}

[[nodiscard]] int curlByte(float c)
{
    return za::clamp(static_cast<int>(za::round(c * 16.f)), -128, 127);
}

} // namespace

float quantizedCurl(float c)
{
    return static_cast<float>(curlByte(c)) / 16.f;
}

void writeDrawnHand(sizebuf_t* buf, const VrDrawnHand& h, const glm::vec3& origin)
{
    MSG_WriteByte(buf, h.flags);
    if(h.flags & VrDrawnHand::handBit)
    {
        writeEighths(buf, h.handOrigin, origin);
        writeAngles16(buf, h.handAngles);
        for(const auto& finger : h.curl)
        {
            for(const float c : finger)
            {
                MSG_WriteChar(buf, curlByte(c));
            }
        }
        // The thumb's turn: x, y, z of the quaternion with w >= 0 (w from them), in 127ths.
        const glm::quat q = h.thumb.w < 0.f ? -h.thumb : h.thumb;
        for(int i = 0; i < 3; i++)
        {
            MSG_WriteChar(buf, za::clamp(static_cast<int>(za::round(q[i] * 127.f)), -127, 127));
        }
    }
    if(h.flags & VrDrawnHand::weaponBit)
    {
        MSG_WriteShort(buf, h.weaponModel);
        MSG_WriteByte(buf, h.weaponFrame);
        MSG_WriteByte(buf, h.weaponSkin);
        writeEighths(buf, h.weaponOrigin, origin);
        writeAngles16(buf, h.weaponAngles);
    }
}

VrDrawnHand readDrawnHand(const glm::vec3& origin)
{
    VrDrawnHand h;
    h.flags = static_cast<za::U8>(MSG_ReadByte());
    if(h.flags & VrDrawnHand::handBit)
    {
        h.handOrigin = readEighths(origin);
        h.handAngles = readAngles16();
        for(auto& finger : h.curl)
        {
            for(float& c : finger)
            {
                c = static_cast<float>(MSG_ReadChar()) / 16.f;
            }
        }
        glm::vec3 v;
        for(int i = 0; i < 3; i++)
        {
            v[i] = static_cast<float>(MSG_ReadChar()) / 127.f;
        }
        const float w2 = 1.f - glm::dot(v, v);
        h.thumb = glm::normalize(glm::quat{w2 > 0.f ? za::sqrt(w2) : 0.f, v.x, v.y, v.z});
    }
    if(h.flags & VrDrawnHand::weaponBit)
    {
        h.weaponModel = static_cast<za::U16>(MSG_ReadShort());
        h.weaponFrame = static_cast<za::U8>(MSG_ReadByte());
        h.weaponSkin = static_cast<za::U8>(MSG_ReadByte());
        h.weaponOrigin = readEighths(origin);
        h.weaponAngles = readAngles16();
    }
    return h;
}

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
        writeVec3(buf, hand.throwFlick);
        writeVec3(buf, hand.throwPos);
        MSG_WriteFloat(buf, hand.throwAge);
        writeVec3(buf, hand.tracked);
        writeVec3(buf, hand.ownVel);
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
    MSG_WriteByte(buf, move.sawCord);
    MSG_WriteByte(buf, move.handDrop);
    MSG_WriteByte(buf, move.handSynth);
    writeVec3(buf, move.origin);
    writeVec3(buf, move.headPos);
    writeVec3(buf, move.shotRot[0]);
    writeVec3(buf, move.shotRot[1]);
    writeVec3(buf, move.loadPort[0]);
    writeVec3(buf, move.loadPort[1]);
    writeVec3(buf, move.loadPortAxis[0]);
    writeVec3(buf, move.loadPortAxis[1]);
    writeVec3(buf, move.loadPortFace[0]);
    writeVec3(buf, move.loadPortFace[1]);
    writeVec3(buf, move.launchOfs[0]);
    writeVec3(buf, move.launchOfs[1]);
    for(const auto& box : move.magBox)
    {
        for(const glm::vec3& v : box)
        {
            writeVec3(buf, v);
        }
    }
    for(const glm::vec3& v : move.ammoPouch)
    {
        writeVec3(buf, v);
    }
    MSG_WriteByte(buf, move.lampLit ? 1 : 0);
    if(move.lampLit)
    {
        writeVec3(buf, move.lampLens);
        writeVec3(buf, move.lampDir);
        MSG_WriteFloat(buf, move.lampRange);
        MSG_WriteFloat(buf, move.lampCos);
    }
    MSG_WriteFloat(buf, move.poseClock);
    MSG_WriteFloat(buf, move.light);
    for(const VrDrawnHand& h : move.drawn)
    {
        writeDrawnHand(buf, h, move.origin);
    }
}

za::Optional<VrMove> readVrMove()
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
        hand.throwFlick = in.vec3();
        hand.throwPos = in.vec3();
        hand.throwAge = in.real();
        hand.tracked = in.vec3();
        hand.ownVel = in.vec3();
    }

    move.headVel = in.vec3();
    move.muzzlePos[0] = in.vec3();
    move.muzzlePos[1] = in.vec3();
    move.vrBits0 = static_cast<za::U16>(MSG_ReadShort());
    move.teleportTarget = in.vec3();
    move.hotspots[0] = static_cast<za::U8>(MSG_ReadByte());
    move.hotspots[1] = static_cast<za::U8>(MSG_ReadByte());
    move.roomscaleMove = in.vec3();
    move.buttons = static_cast<za::U8>(MSG_ReadByte());
    move.sawCord = static_cast<za::U8>(MSG_ReadByte());
    move.handDrop = static_cast<za::U8>(MSG_ReadByte());
    move.handSynth = static_cast<za::U8>(MSG_ReadByte());
    move.origin = in.vec3();
    move.headPos = in.vec3();
    move.shotRot[0] = in.vec3();
    move.shotRot[1] = in.vec3();
    move.loadPort[0] = in.vec3();
    move.loadPort[1] = in.vec3();
    move.loadPortAxis[0] = in.vec3();
    move.loadPortAxis[1] = in.vec3();
    move.loadPortFace[0] = in.vec3();
    move.loadPortFace[1] = in.vec3();
    move.launchOfs[0] = in.vec3();
    move.launchOfs[1] = in.vec3();
    for(auto& box : move.magBox)
    {
        for(glm::vec3& v : box)
        {
            v = in.vec3();
        }
    }
    for(glm::vec3& v : move.ammoPouch)
    {
        v = in.vec3();
    }
    move.lampLit = MSG_ReadByte() == 1;
    if(move.lampLit)
    {
        move.lampLens = in.vec3();
        move.lampDir = in.vec3();
        move.lampRange = in.real();
        move.lampCos = in.real();
    }
    move.poseClock = in.real();
    move.light = in.real();
    for(VrDrawnHand& h : move.drawn)
    {
        h = readDrawnHand(in.finite ? move.origin : glm::vec3{0.f});
    }

    if(!in.finite)
    {
        return za::nullOpt;
    }
    return za::makeOptional(ZA_MOVE(move));
}

} // namespace qvr
