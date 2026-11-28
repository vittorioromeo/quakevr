// vr_move.cpp -- VR clc_move block serialization. Vectors are sent as floats: hand
// velocities and offsets need more precision than protocol coordinates give.

#include "vr_move.hpp"
#include "vr_engine.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/IsFinite.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sin.hpp"
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
    if(h.flags & VrDrawnHand::solveBit)
    {
        writeEighths(buf, h.handOrigin, origin);
        writeAngles16(buf, h.handAngles);
        MSG_WriteShort(buf, h.weaponModel);
        MSG_WriteByte(buf, h.weaponFrame);
        if(h.flags & VrDrawnHand::weaponBit)
        {
            // (bots3: its weapon where the server placed it, view::placeHeld: drawn there by every client)
            writeEighths(buf, h.weaponOrigin, origin);
            writeAngles16(buf, h.weaponAngles);
        }
        return;
    }
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

void writeGunLook(sizebuf_t* buf, const VrGunLook& l)
{
    // (the player's mode bits with or without a gun: his holstered guns are drawn by them)
    const za::U8 bits = (l.bits & VrGunLook::gunBit) ? l.bits : (l.bits & ~VrGunLook::revolverBit);
    MSG_WriteByte(buf, bits);
    if(!(bits & VrGunLook::gunBit))
    {
        return;
    }
    MSG_WriteShort(buf, l.flags);
    MSG_WriteByte(buf, l.clip);
    MSG_WriteByte(buf, l.clipSize);
    MSG_WriteByte(buf, l.attach);
    if(bits & VrGunLook::revolverBit)
    {
        const int rv = l.revolver & 0xFFFFFF; // (18 bits used: vr_protocol.hpp, STAT_QVR_REVOLVER)
        MSG_WriteByte(buf, rv & 255);
        MSG_WriteByte(buf, (rv >> 8) & 255);
        MSG_WriteByte(buf, (rv >> 16) & 255);
        MSG_WriteByte(buf, static_cast<int>(za::round(l.cylinder / 360.f * 256.f)) & 255);
    }
}

VrGunLook readGunLook()
{
    VrGunLook l;
    l.bits = static_cast<za::U8>(MSG_ReadByte());
    if(!(l.bits & VrGunLook::gunBit))
    {
        l.bits &= static_cast<za::U8>(~VrGunLook::revolverBit);
        return l;
    }
    l.flags = static_cast<za::U16>(MSG_ReadShort());
    l.clip = static_cast<za::U8>(MSG_ReadByte());
    l.clipSize = static_cast<za::U8>(MSG_ReadByte());
    l.attach = static_cast<za::U8>(MSG_ReadByte());
    if(l.bits & VrGunLook::revolverBit)
    {
        const int b0 = MSG_ReadByte();
        const int b1 = MSG_ReadByte();
        const int b2 = MSG_ReadByte();
        l.revolver = b0 | (b1 << 8) | (b2 << 16);
        l.cylinder = static_cast<float>(MSG_ReadByte()) * (360.f / 256.f);
    }
    return l;
}

void writeDrawnLamp(sizebuf_t* buf, const VrDrawnLamp& l, const glm::vec3& origin)
{
    const za::U8 flags = (l.flags & VrDrawnLamp::placedBit) ? l.flags : 0;
    MSG_WriteByte(buf, flags);
    if(!flags)
    {
        return;
    }
    writeEighths(buf, l.origin, origin);
    const glm::quat q = l.rot.w < 0.f ? -l.rot : l.rot;
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteShort(buf, za::clamp(static_cast<int>(za::round(q[i] * 32767.f)), -32767, 32767));
    }
    if(flags & VrDrawnLamp::litBit)
    {
        MSG_WriteShort(buf, za::clamp(static_cast<int>(za::round(l.range)), 0, 32767));
    }
}

namespace
{
// (syncfix3) What a gear piece is placed from: the gadget and its straps from the hand that wears them (its place and
// turn), the rest from his body (`origin`, its yaw). Unchanged while he walks, turns or swings his arms: the avatar's
// bytes stay the same, so its snapshot record's delta stays small (the move's own: vr_netmove.cpp's visitMove).
struct GearFrame
{
    glm::vec3 from;
    glm::vec3 turn;
};

[[nodiscard]] GearFrame gearFrame(int piece, int wrist, const glm::vec3& origin, float bodyYaw, const VrDrawnHand* hands)
{
    if(piece >= VrDrawnGear::gadget && piece < VrDrawnGear::pouch)
    {
        const VrDrawnHand& h = hands[wrist ? 1 : 0];
        return GearFrame{h.handOrigin, h.handAngles};
    }
    return GearFrame{origin, glm::vec3{0.f, bodyYaw, 0.f}};
}

[[nodiscard]] glm::vec3 gearLocal(const glm::vec3& x, const GearFrame& f)
{
    const glm::vec3 d = x - f.from;
    const float c = za::cos(glm::radians(f.turn[1])), s = za::sin(glm::radians(f.turn[1]));
    return glm::vec3{d.x * c + d.y * s, d.y * c - d.x * s, d.z};
}

[[nodiscard]] glm::vec3 gearWorld(const glm::vec3& l, const GearFrame& f)
{
    const float c = za::cos(glm::radians(f.turn[1])), s = za::sin(glm::radians(f.turn[1]));
    return f.from + glm::vec3{l.x * c - l.y * s, l.x * s + l.y * c, l.z};
}
} // namespace

void writeDrawnGear(sizebuf_t* buf, const VrDrawnGear& g, const glm::vec3& origin, float bodyYaw, const VrDrawnHand* hands)
{
    MSG_WriteShort(buf, g.shown);
    MSG_WriteShort(buf, g.mirrored);
    MSG_WriteByte(buf, g.wrist);
    for(int i = 0; i < VrDrawnGear::count; i++)
    {
        if(g.shown & (1 << i))
        {
            const GearFrame f = gearFrame(i, g.wrist, origin, bodyYaw, hands);
            MSG_WriteShort(buf, g.model[i]);
            MSG_WriteByte(buf, g.frame[i]);
            MSG_WriteByte(buf, g.skin[i]);
            MSG_WriteByte(buf, g.scale[i]);
            writeEighths(buf, gearLocal(g.origin[i], f), glm::vec3{0.f});
            writeAngles16(buf, g.angles[i] - f.turn);
        }
    }
    if(g.shown & (1 << VrDrawnGear::ammoPouch))
    {
        MSG_WriteByte(buf, g.pouchKind);
        MSG_WriteByte(buf, g.pouchCount);
        MSG_WriteByte(buf, g.pouchKind2);
        MSG_WriteByte(buf, g.pouchCount2);
    }
}

VrDrawnGear readDrawnGear(const glm::vec3& origin, float bodyYaw, const VrDrawnHand* hands)
{
    VrDrawnGear g;
    g.shown = static_cast<za::U16>(MSG_ReadShort());
    g.mirrored = static_cast<za::U16>(MSG_ReadShort());
    g.wrist = static_cast<za::U8>(MSG_ReadByte() ? 1 : 0);
    for(int i = 0; i < VrDrawnGear::count; i++)
    {
        if(g.shown & (1 << i))
        {
            const GearFrame f = gearFrame(i, g.wrist, origin, bodyYaw, hands);
            g.model[i] = static_cast<za::U16>(MSG_ReadShort());
            g.frame[i] = static_cast<za::U8>(MSG_ReadByte());
            g.skin[i] = static_cast<za::U8>(MSG_ReadByte());
            g.scale[i] = static_cast<za::U8>(MSG_ReadByte());
            g.origin[i] = gearWorld(readEighths(glm::vec3{0.f}), f);
            const glm::vec3 a = readAngles16() + f.turn; // (in -180..180, as a whole turn read back is)
            g.angles[i] = a - 360.f * glm::floor((a + 180.f) / 360.f);
        }
    }
    if(g.shown & (1 << VrDrawnGear::ammoPouch))
    {
        g.pouchKind = static_cast<za::U8>(MSG_ReadByte());
        g.pouchCount = static_cast<za::U8>(MSG_ReadByte());
        g.pouchKind2 = static_cast<za::U8>(MSG_ReadByte());
        g.pouchCount2 = static_cast<za::U8>(MSG_ReadByte());
    }
    return g;
}

VrDrawnLamp readDrawnLamp(const glm::vec3& origin)
{
    VrDrawnLamp l;
    l.flags = static_cast<za::U8>(MSG_ReadByte());
    if(!(l.flags & VrDrawnLamp::placedBit))
    {
        l.flags = 0;
        return l;
    }
    l.origin = readEighths(origin);
    glm::vec3 v;
    for(int i = 0; i < 3; i++)
    {
        v[i] = static_cast<float>(MSG_ReadShort()) / 32767.f;
    }
    const float w2 = 1.f - glm::dot(v, v);
    l.rot = glm::normalize(glm::quat{w2 > 0.f ? za::sqrt(w2) : 0.f, v.x, v.y, v.z});
    if(l.flags & VrDrawnLamp::litBit)
    {
        l.range = static_cast<float>(MSG_ReadShort());
    }
    return l;
}

VrDrawnHand readDrawnHand(const glm::vec3& origin)
{
    VrDrawnHand h;
    h.flags = static_cast<za::U8>(MSG_ReadByte());
    if(h.flags & VrDrawnHand::solveBit)
    {
        h.handOrigin = readEighths(origin);
        h.handAngles = readAngles16();
        h.weaponModel = static_cast<za::U16>(MSG_ReadShort());
        h.weaponFrame = static_cast<za::U8>(MSG_ReadByte());
        if(h.flags & VrDrawnHand::weaponBit)
        {
            h.weaponOrigin = readEighths(origin);
            h.weaponAngles = readAngles16();
        }
        return h;
    }
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

void writeBodyPose(sizebuf_t* buf, const VrBodyPose& p)
{
    MSG_WriteByte(buf, p.valid ? 1 : 0);
    if(p.valid)
    {
        MSG_WriteShort(buf, static_cast<int>(za::round(p.yaw * 65536.f / 360.f)) & 65535);
        MSG_WriteShort(buf, za::clamp(static_cast<int>(za::round(p.headHeight * 1000.f)), -32767, 32767));
        MSG_WriteShort(buf, za::clamp(static_cast<int>(za::round(p.standingHeight * 1000.f)), -32767, 32767));
        MSG_WriteByte(buf, za::clamp(static_cast<int>(za::round(p.leanHold * 255.f)), 0, 255));
    }
}

VrBodyPose readBodyPose()
{
    VrBodyPose p;
    p.valid = MSG_ReadByte() == 1;
    if(p.valid)
    {
        p.yaw = static_cast<float>(MSG_ReadShort()) * (360.f / 65536.f);
        p.headHeight = static_cast<float>(MSG_ReadShort()) / 1000.f;
        p.standingHeight = static_cast<float>(MSG_ReadShort()) / 1000.f;
        p.leanHold = static_cast<float>(MSG_ReadByte()) / 255.f;
    }
    return p;
}

void writeDrawnHolsters(sizebuf_t* buf, const VrDrawnHolsters& h, const glm::vec3& origin, bool points)
{
    MSG_WriteByte(buf, h.valid ? 1 : 0);
    if(!h.valid)
    {
        return;
    }
    if(points)
    {
        for(const glm::vec3& p : h.at)
        {
            writeEighths(buf, p, origin);
        }
    }
    MSG_WriteByte(buf, h.weapons);
    MSG_WriteByte(buf, h.mirrored);
    for(int i = 0; i < VrDrawnHolsters::count; i++)
    {
        if(h.weapons & (1 << i))
        {
            MSG_WriteShort(buf, h.model[i]);
            MSG_WriteByte(buf, h.frame[i]);
            MSG_WriteByte(buf, h.skin[i]);
            writeEighths(buf, h.origin[i], origin);
            writeAngles16(buf, h.angles[i]);
        }
    }
}

VrDrawnHolsters readDrawnHolsters(const glm::vec3& origin, bool points)
{
    VrDrawnHolsters h;
    h.valid = MSG_ReadByte() == 1;
    if(!h.valid)
    {
        return h;
    }
    if(points)
    {
        for(glm::vec3& p : h.at)
        {
            p = readEighths(origin);
        }
    }
    h.weapons = static_cast<za::U8>(MSG_ReadByte());
    h.mirrored = static_cast<za::U8>(MSG_ReadByte());
    for(int i = 0; i < VrDrawnHolsters::count; i++)
    {
        if(h.weapons & (1 << i))
        {
            h.model[i] = static_cast<za::U16>(MSG_ReadShort());
            h.frame[i] = static_cast<za::U8>(MSG_ReadByte());
            h.skin[i] = static_cast<za::U8>(MSG_ReadByte());
            h.origin[i] = readEighths(origin);
            h.angles[i] = readAngles16();
        }
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
    MSG_WriteShort(buf, move.handSynth);
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
    writeBodyPose(buf, move.body);
    writeDrawnHolsters(buf, move.holsters, move.origin, true);
    writeDrawnLamp(buf, move.lamp, move.origin);
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
    move.handSynth = static_cast<za::U16>(MSG_ReadShort()); // (a short since flatmelee6: 256 the two-fisted blow)
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
    move.body = readBodyPose();
    move.holsters = readDrawnHolsters(in.finite ? move.origin : glm::vec3{0.f}, true);
    move.lamp = readDrawnLamp(in.finite ? move.origin : glm::vec3{0.f});

    if(!in.finite)
    {
        return za::nullOpt;
    }
    return za::makeOptional(ZA_MOVE(move));
}

} // namespace qvr
