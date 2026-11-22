// vr_netmove.cpp -- a remote client's moves up as quantized deltas with repeats (vr_netmove.hpp; MP_PLAN.md, B7).

#include "vr_netmove.hpp"
#include "vr_netsnap.hpp"
#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_protocol.hpp"
#include "vr_testkit.hpp"
#include "vr_melee_shared.h"
#include "vr_mem.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/IsFinite.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Container/Vector.hpp"

#include <string.h>

namespace qvr::netmove
{
namespace
{

// Quanta per unit of each kind of field.
constexpr float posScale = 64.f;             // places and offsets: 64ths of a unit
constexpr float drawnScale = 8.f;            // the drawn hands' and holsters' places: eighths (as the avatar sends them)
constexpr float angScale = 65536.f / 360.f;  // angles: 65536ths of a turn (not wrapped: the value as it was)
// (syncfix4) The worn gear's places and turns in their frames, coarser: quarters and 4096ths of a turn (0.09 deg); the
// finer steps changed with every move from the frames' own rounding (mp_loss's link up at its bound).
constexpr float gearScale = 4.f;
constexpr float gearAngScale = 4096.f / 360.f;
constexpr float velScale = 1024.f;           // speeds (metres or units a second), angular speeds
constexpr float unitScale = 4096.f;          // unit directions
constexpr float timeScale = 1000.f;          // the client's time, throw ages: milliseconds
constexpr float clockScale = 100000.f;       // the poses' clock (wraps at VR_POSE_CLOCK_WRAP, 1024 s): 10 microseconds
constexpr za::I32 quantMost = 1 << 30;

[[nodiscard]] za::I32 toQuant(float x, float scale)
{
    if(!ZA_ISFINITE(x))
    {
        return 0;
    }
    const float r = za::round(x * scale);
    return r >= static_cast<float>(quantMost) ? quantMost : (r <= -static_cast<float>(quantMost) ? -quantMost : static_cast<za::I32>(r));
}

// The move's fields in their order, each with its kind: one list for counting, quantizing and dequantizing. Its origin
// first (the places from it follow).
template <class V>
constexpr void visitMove(WireMove& m, V& v)
{
    VrMove& r = m.vr;
    v.abs(r.origin, posScale);
    const glm::vec3& o = r.origin;
    v.real(m.time, timeScale);
    v.real(m.frameTime, clockScale);
    v.real(m.viewBack, clockScale);
    v.abs(m.viewAngles, angScale);
    v.integer(m.forward);
    v.integer(m.side);
    v.integer(m.up);
    v.integer(m.buttons);
    v.integer(m.impulse);
    v.abs(r.headAngles, angScale);
    v.real(r.vrYaw, angScale);
    for(VrHandMove& h : r.hands)
    {
        v.rel(h.pos, o, posScale);
        v.abs(h.rot, angScale);
        v.abs(h.vel, velScale);
        v.abs(h.throwVel, velScale);
        v.real(h.velMag, velScale);
        v.abs(h.angVel, velScale);
        v.abs(h.throwFlick, velScale);
        v.abs(h.throwPos, posScale);
        v.real(h.throwAge, timeScale);
        v.rel(h.tracked, o, posScale);
        v.abs(h.ownVel, velScale);
    }
    v.abs(r.headVel, velScale);
    v.rel(r.muzzlePos[0], o, posScale);
    v.rel(r.muzzlePos[1], o, posScale);
    v.integer(r.vrBits0);
    v.abs(r.teleportTarget, posScale);
    v.integer(r.hotspots[0]);
    v.integer(r.hotspots[1]);
    v.abs(r.roomscaleMove, posScale);
    v.integer(r.buttons);
    v.integer(r.sawCord);
    v.integer(r.handDrop);
    v.integer(r.handSynth);
    v.rel(r.headPos, o, posScale);
    for(int h = 0; h < 2; h++)
    {
        v.abs(r.shotRot[h], angScale);
        v.rel(r.loadPort[h], o, posScale);
        v.abs(r.loadPortAxis[h], unitScale);
        v.abs(r.loadPortFace[h], unitScale);
        v.abs(r.launchOfs[h], posScale);
        for(glm::vec3& b : r.magBox[h])
        {
            v.abs(b, posScale); // (all zero: none, kept exactly: not from the origin)
        }
    }
    for(glm::vec3& p : r.ammoPouch)
    {
        v.abs(p, posScale);
    }
    v.integer(r.lampLit);
    v.rel(r.lampLens, o, posScale);
    v.abs(r.lampDir, unitScale);
    v.real(r.lampRange, 16.f);
    v.real(r.lampCos, unitScale);
    v.real(r.poseClock, clockScale);
    v.real(r.light, unitScale);
    for(VrDrawnHand& h : r.drawn)
    {
        v.integer(h.flags);
        v.rel(h.handOrigin, o, drawnScale);
        v.abs(h.handAngles, angScale);
        for(auto& finger : h.curl)
        {
            for(float& c : finger)
            {
                v.real(c, 16.f);
            }
        }
        v.thumb(h.thumb);
        v.integer(h.weaponModel);
        v.integer(h.weaponFrame);
        v.integer(h.weaponSkin);
        v.rel(h.weaponOrigin, o, drawnScale);
        v.abs(h.weaponAngles, angScale);
    }
    VrBodyPose& b = r.body;
    v.integer(b.valid);
    v.real(b.yaw, angScale);
    v.real(b.headHeight, 1000.f);
    v.real(b.standingHeight, 1000.f);
    v.real(b.leanHold, 255.f);
    VrDrawnHolsters& hs = r.holsters;
    v.integer(hs.valid);
    v.integer(hs.weapons);
    v.integer(hs.mirrored);
    for(int i = 0; i < VrDrawnHolsters::count; i++)
    {
        v.rel(hs.at[i], o, drawnScale);
        v.integer(hs.model[i]);
        v.integer(hs.frame[i]);
        v.integer(hs.skin[i]);
        v.rel(hs.origin[i], o, drawnScale);
        v.abs(hs.angles[i], angScale);
    }
    VrDrawnGear& gear = r.gear; // (his worn gear as drawn: the others draw it, syncfix)
    v.integer(gear.shown);
    v.integer(gear.mirrored);
    v.integer(gear.wrist);
    // (syncfix3) Each piece from what it is worn on: the gadget and its straps from the hand that wears them (place and
    // turn), the rest from his body (its yaw): unchanged while he walks, turns or swings his arms, so the delta from the
    // acknowledged move stays empty (the move up grew by half under 10% loss sent from his origin).
    const VrDrawnHand& wristHand = r.drawn[gear.wrist ? 1 : 0];
    const glm::vec3 bodyTurn{0.f, b.yaw, 0.f};
    for(int i = 0; i < VrDrawnGear::count; i++)
    {
        v.integer(gear.model[i]);
        v.integer(gear.frame[i]);
        v.integer(gear.skin[i]);
        v.integer(gear.scale[i]);
        const bool onWrist = i >= VrDrawnGear::gadget && i < VrDrawnGear::pouch;
        const glm::vec3& from = onWrist ? wristHand.handOrigin : o;
        const glm::vec3& turn = onWrist ? wristHand.handAngles : bodyTurn;
        // (a piece not drawn: zeros from its frame, not the world's origin seen from a moving frame -- that changed with
        // every move; read back, it is left as it was)
        const bool shown = (gear.shown & (1 << i)) != 0;
        glm::vec3 at = shown ? gear.origin[i] : from;
        glm::vec3 ang = shown ? gear.angles[i] : turn;
        v.framed(at, from, turn[1], gearScale);
        v.turned(ang, turn, gearAngScale);
        if(shown)
        {
            v.store(gear.origin[i], at); // (the dequantizer's: the quantizer only reads the move)
            v.store(gear.angles[i], ang);
        }
    }
    VrDrawnLamp& lamp = r.lamp; // (his torch as drawn: the others draw and light it, wpnsync)
    v.integer(lamp.flags);
    v.rel(lamp.origin, o, drawnScale);
    v.thumb(lamp.rot, 32767.f);
    v.real(lamp.range, 1.f);
}

struct Counter
{
    int n{0};
    constexpr void real(float&, float) { n++; }
    constexpr void abs(glm::vec3&, float) { n += 3; }
    constexpr void rel(glm::vec3&, const glm::vec3&, float) { n += 3; }
    constexpr void framed(glm::vec3&, const glm::vec3&, float, float) { n += 3; }
    constexpr void turned(glm::vec3&, const glm::vec3&, float) { n += 3; }
    constexpr void store(glm::vec3&, const glm::vec3&) {}
    template <class T>
    constexpr void integer(T&)
    {
        n++;
    }
    constexpr void thumb(glm::quat&, float = 127.f) { n += 3; }
};

[[nodiscard]] constexpr int countValues()
{
    WireMove m;
    Counter c;
    visitMove(m, c);
    return c.n;
}

// The move's values, counted at compile time: a move grown past maxValues would overflow Quant::v (quantize).
constexpr int valueCount = countValues();
static_assert(valueCount <= maxValues, "a move's values outgrew netmove::maxValues (Quant::v)");

struct Quantizer
{
    Quant& q;
    int i{0};
    void real(float& x, float scale) { q.v[i++] = toQuant(x, scale); }
    void abs(glm::vec3& x, float scale)
    {
        for(int k = 0; k < 3; k++)
        {
            q.v[i++] = toQuant(x[k], scale);
        }
    }
    void rel(glm::vec3& x, const glm::vec3& o, float scale)
    {
        for(int k = 0; k < 3; k++)
        {
            q.v[i++] = toQuant(x[k] - o[k], scale);
        }
    }
    void framed(glm::vec3& x, const glm::vec3& o, float yaw, float scale)
    {
        const glm::vec3 d = x - o;
        const float c = za::cos(glm::radians(yaw)), s = za::sin(glm::radians(yaw));
        q.v[i++] = toQuant(d.x * c + d.y * s, scale);
        q.v[i++] = toQuant(d.y * c - d.x * s, scale);
        q.v[i++] = toQuant(d.z, scale);
    }
    void turned(glm::vec3& a, const glm::vec3& ref, float scale)
    {
        for(int k = 0; k < 3; k++)
        {
            q.v[i++] = toQuant(a[k] - ref[k], scale);
        }
    }
    void store(glm::vec3&, const glm::vec3&) {}
    template <class T>
    void integer(T& x)
    {
        q.v[i++] = static_cast<za::I32>(x);
    }
    void thumb(glm::quat& t, float scale = 127.f)
    {
        // x, y, z with w >= 0 (w from them), in 127ths (`scale`ths): as writeDrawnHand
        const glm::quat u = t.w < 0.f ? -t : t;
        const int most = static_cast<int>(scale);
        for(int k = 0; k < 3; k++)
        {
            q.v[i++] = za::clamp(toQuant(u[k], scale), -most, most);
        }
    }
};

struct Dequantizer
{
    const Quant& q;
    int i{0};
    void real(float& x, float scale) { x = static_cast<float>(q.v[i++]) / scale; }
    void abs(glm::vec3& x, float scale)
    {
        for(int k = 0; k < 3; k++)
        {
            x[k] = static_cast<float>(q.v[i++]) / scale;
        }
    }
    void rel(glm::vec3& x, const glm::vec3& o, float scale)
    {
        for(int k = 0; k < 3; k++)
        {
            x[k] = o[k] + static_cast<float>(q.v[i++]) / scale;
        }
    }
    // (from `o` in the frame turned by `yaw` degrees: Quantizer::framed's inverse)
    void framed(glm::vec3& x, const glm::vec3& o, float yaw, float scale)
    {
        const float c = za::cos(glm::radians(yaw)), s = za::sin(glm::radians(yaw));
        const float fx = static_cast<float>(q.v[i++]) / scale, fy = static_cast<float>(q.v[i++]) / scale;
        const float fz = static_cast<float>(q.v[i++]) / scale;
        x = glm::vec3{o.x + fx * c - fy * s, o.y + fx * s + fy * c, o.z + fz};
    }
    void turned(glm::vec3& a, const glm::vec3& ref, float scale)
    {
        for(int k = 0; k < 3; k++)
        {
            a[k] = ref[k] + static_cast<float>(q.v[i++]) / scale;
        }
    }
    void store(glm::vec3& x, const glm::vec3& value) { x = value; }
    template <class T>
    void integer(T& x)
    {
        x = static_cast<T>(q.v[i++]);
    }
    void thumb(glm::quat& t, float scale = 127.f)
    {
        glm::vec3 v;
        for(int k = 0; k < 3; k++)
        {
            v[k] = static_cast<float>(q.v[i++]) / scale;
        }
        const float w2 = 1.f - glm::dot(v, v);
        t = glm::normalize(glm::quat{w2 > 0.f ? za::sqrt(w2) : 0.f, v.x, v.y, v.z});
    }
};

} // namespace

void writeVarint(sizebuf_t* buf, za::U32 u)
{
    while(u >= 0x80u)
    {
        MSG_WriteByte(buf, static_cast<int>((u & 0x7Fu) | 0x80u));
        u >>= 7;
    }
    MSG_WriteByte(buf, static_cast<int>(u));
}

bool readVarint(za::U32& u)
{
    u = 0;
    for(int shift = 0; shift < 35; shift += 7)
    {
        const int b = MSG_ReadByte();
        if(b < 0 || msg_badread)
        {
            return false;
        }
        u |= static_cast<za::U32>(b & 0x7F) << shift;
        if(!(b & 0x80))
        {
            return true;
        }
    }
    return false;
}

namespace
{

[[nodiscard]] za::U32 zigzag(za::I32 d)
{
    return (static_cast<za::U32>(d) << 1) ^ static_cast<za::U32>(d >> 31);
}

[[nodiscard]] za::I32 unzigzag(za::U32 u)
{
    return static_cast<za::I32>((u >> 1) ^ (0u - (u & 1u)));
}

const Quant zeroQuant{};

constexpr int maxGroups = (maxValues + 7) / 8;

// The client's moves sent (by sequence number, `ring` back) and the newest the server acknowledged.
struct ClientState
{
    za::U32 nextSeq{1}; // (kept over maps and connections: the server's slot is reset on a new connection)
    za::U32 acked{0};
    za::U32 first{0};   // the first move sent since clientClear (0: none yet)
    za::U32 seqs[ring]{};
    Quant moves[ring];
    double sentAt[ring]{}; // (B13) each move's send (Sys_DoubleTime): its acknowledgement's round trip, the net HUD's ping
    double rtt{0.0};       // ... smoothed (0: none measured on this connection yet)
    double rttLast{0.0};   // ... the newest
};
ClientState clientState;

// (B20) The ticks over which the move buffer measures a client's jitter (2 s at 72 Hz): a spike's depth lasts this long.
constexpr int lateWindow = 144;

// The server's per client slot: the moves read (`ring` back), the newest run.
struct ServerSlot
{
    za::U32 last{0};
    za::U32 low{0};  // the lockstep's and the buffer's first move: the oldest read before one ran (0: none read)
    za::U32 read{0}; // the move buffer's newest move read (B19: kept until a tick runs it; 0: none)
    za::U32 seqs[ring]{};
    Quant moves[ring];
    Came came[ring]; // (mpflakes) each kept move's first read and its quiet allowance (serverCame)
    double heardAt{-1.0}; // the server's time of the last packet read (-1: none yet)
    double quiet{0.0};    // the quiet allowance then
    ServerCounts counts;
    bool sends{false}; // it sent QVR_CLC_MOVES: acknowledged in its datagrams
    // (B20) The buffer's depth by his link's jitter: each move's lateness when it first came (the buffer's tick count
    // less its number), the lowest and highest of each tick's over the last `lateWindow` ticks.
    za::U32 tick{0};           // the buffer's ticks since the slot's reset
    int lateLo[lateWindow]{};  // ... each tick's lowest lateness (lateAny: any move came in it)
    int lateHi[lateWindow]{};
    bool lateAny[lateWindow]{};
    int curLo{0}, curHi{0};    // ... the coming tick's so far
    bool curAny{false};
};
za::Vector<ServerSlot> serverSlots;
Came applying; // (mpflakes) the move being run's (serverCame)

[[nodiscard]] ServerSlot* slotOf(int slot, bool make)
{
    if(slot < 0 || slot >= MAX_SCOREBOARD)
    {
        return nullptr;
    }
    if(slot >= static_cast<int>(serverSlots.size()))
    {
        if(!make)
        {
            return nullptr;
        }
        serverSlots.resize(static_cast<za::SizeT>(slot) + 1);
    }
    return &serverSlots[static_cast<za::SizeT>(slot)];
}

} // namespace

int numValues()
{
    return valueCount;
}

void quantize(const WireMove& m, Quant& q)
{
    Quantizer v{q};
    visitMove(const_cast<WireMove&>(m), v); // (the quantizer only reads)
}

void dequantize(const Quant& q, WireMove& m)
{
    Dequantizer v{q};
    visitMove(m, v);
}

void writeDelta(sizebuf_t* buf, const Quant& base, const Quant& q)
{
    const int n = valueCount;
    const int groups = (n + 7) / 8;
    za::U8 groupMask[maxGroups]{};
    za::U8 top[(maxGroups + 7) / 8]{};
    for(int i = 0; i < n; i++)
    {
        if(q.v[i] != base.v[i])
        {
            groupMask[i / 8] = static_cast<za::U8>(groupMask[i / 8] | (1 << (i % 8)));
        }
    }
    for(int g = 0; g < groups; g++)
    {
        if(groupMask[g])
        {
            top[g / 8] = static_cast<za::U8>(top[g / 8] | (1 << (g % 8)));
        }
    }
    for(int t = 0; t < (groups + 7) / 8; t++)
    {
        MSG_WriteByte(buf, top[t]);
    }
    for(int g = 0; g < groups; g++)
    {
        if(!groupMask[g])
        {
            continue;
        }
        MSG_WriteByte(buf, groupMask[g]);
        for(int k = 0; k < 8; k++)
        {
            if(groupMask[g] & (1 << k))
            {
                const int i = g * 8 + k;
                writeVarint(buf, zigzag(static_cast<za::I32>(static_cast<za::U32>(q.v[i]) - static_cast<za::U32>(base.v[i]))));
            }
        }
    }
}

bool readDelta(const Quant& base, Quant& q)
{
    const int n = valueCount;
    const int groups = (n + 7) / 8;
    q = base;
    za::U8 top[(maxGroups + 7) / 8]{};
    for(int t = 0; t < (groups + 7) / 8; t++)
    {
        const int b = MSG_ReadByte();
        if(b < 0 || msg_badread)
        {
            return false;
        }
        top[t] = static_cast<za::U8>(b);
    }
    for(int g = 0; g < groups; g++)
    {
        if(!(top[g / 8] & (1 << (g % 8))))
        {
            continue;
        }
        const int mask = MSG_ReadByte();
        if(mask < 0 || msg_badread)
        {
            return false;
        }
        for(int k = 0; k < 8; k++)
        {
            const int i = g * 8 + k;
            if(!(mask & (1 << k)))
            {
                continue;
            }
            za::U32 u;
            if(!readVarint(u) || i >= n)
            {
                return false;
            }
            q.v[i] = static_cast<za::I32>(static_cast<za::U32>(base.v[i]) + static_cast<za::U32>(unzigzag(u)));
        }
    }
    return true;
}

bool clientUses()
{
    // (not this game's own server: a client connected while it hosts is the loopback's. Until B11b this asked whether
    // the socket's address was "LOCAL", the loopback's server side -- its client side is "localhost" -- so single
    // player and a listen host's own client sent QVR_CLC_MOVES and predicted themselves too)
    return cls.state == ca_connected && cls.netcon && (cl.protocolflags & PRFL_QUAKEVR) && !sv.active;
}

void writeMoves(sizebuf_t* buf, const WireMove& m, bool record)
{
    if(!record)
    {
        return; // (dumped after a signon, or a demo's: never sent, so never a base)
    }
    ClientState& c = clientState;
    const za::U32 seq = c.nextSeq++;
    c.first = c.first != 0 ? c.first : seq;
    const int at = static_cast<int>(seq % ring);
    quantize(m, c.moves[at]);
    c.seqs[at] = seq;
    c.sentAt[at] = Sys_DoubleTime();
    const za::U32 base = c.acked;
    const bool haveBase = base > 0 && base < seq && seq - base < static_cast<za::U32>(ring) && c.seqs[base % ring] == base;
    za::U32 first = seq > static_cast<za::U32>(repeats) ? seq - repeats : 1;
    if(haveBase && first <= base)
    {
        first = base + 1;
    }
    while(first < seq && c.seqs[first % ring] != first)
    {
        first++;
    }
    MSG_WriteByte(buf, QVR_CLC_MOVES);
    writeVarint(buf, netsnap::clientAck()); // (the newest snapshot read: the base of the server's next, B7b)
    writeVarint(buf, haveBase ? base : 0);
    MSG_WriteByte(buf, static_cast<int>(seq - first + 1));
    writeVarint(buf, first);
    const Quant* prev = haveBase ? &c.moves[base % ring] : &zeroQuant;
    for(za::U32 s = first; s <= seq; s++)
    {
        const Quant& q = c.moves[s % ring];
        writeDelta(buf, *prev, q);
        prev = &q;
    }
}

void parseAck()
{
    const za::U32 seq = static_cast<za::U32>(MSG_ReadLong());
    ClientState& c = clientState;
    if(seq < c.nextSeq && seq > c.acked)
    {
        c.acked = seq;
        if(c.seqs[seq % ring] == seq)
        {
            // (B13) the move's round trip: the way there, the server's frame that ran it, the datagram back
            c.rttLast = za::max(0.0, Sys_DoubleTime() - c.sentAt[seq % ring]);
            c.rtt = c.rtt > 0.0 ? 0.875 * c.rtt + 0.125 * c.rttLast : c.rttLast;
        }
    }
}

void clientClear()
{
    ClientState& c = clientState;
    c.acked = 0;
    c.first = 0;
    c.rtt = c.rttLast = 0.0;
    memset(c.seqs, 0, sizeof(c.seqs));
}

namespace
{
// Forgotten when the client's state is cleared (vr_mem.hpp's resets): no acknowledged move: the next sent whole, B7.
mem::Reset clientStateReset{"netmove", mem::ResetEvent::ClientState, mem::ResetPhase::State, &clientClear};
} // namespace

double clientRoundTrip(double* newest)
{
    if(newest)
    {
        *newest = clientState.rttLast;
    }
    return clientState.rtt;
}

za::U32 clientNewest()
{
    return clientState.nextSeq - 1;
}

za::U32 clientSent()
{
    const ClientState& c = clientState;
    return c.first != 0 ? c.nextSeq - c.first : 0;
}

za::U32 clientPending()
{
    const ClientState& c = clientState;
    if(c.first == 0)
    {
        return 0;
    }
    const za::U32 done = c.acked >= c.first ? c.acked : c.first - 1;
    return c.nextSeq - 1 - done;
}

bool clientMoveAt(za::U32 seq, WireMove& m)
{
    const ClientState& c = clientState;
    if(seq == 0 || seq >= c.nextSeq || c.seqs[seq % ring] != seq)
    {
        return false;
    }
    dequantize(c.moves[seq % ring], m); // (the integers sent: the server's values exactly)
    return true;
}

namespace
{
// (B19) The move buffer: on unless the harness's lockstep runs the moves by number.
[[nodiscard]] bool buffering()
{
    return vr_net_movebuffer.value >= 1.f && vr_mp_lockstep.value < 1.f;
}

// The oldest move kept after the last one run, run (a gap before it: lost) -> false: none kept. (B20) !skipGap: a gap
// before it waits instead (-> false; *gap set): a move reordered on the way comes a tick or two after a newer one.
bool runOldest(client_t* client, ServerSlot& slot, ApplyFn apply, bool skipGap = true, bool* gap = nullptr)
{
    if(slot.read == 0 || slot.read <= slot.last)
    {
        return false;
    }
    za::U32 from = slot.last != 0 ? slot.last + 1 : slot.low;
    if(from == 0 || from > slot.read)
    {
        return false;
    }
    if(slot.read - from >= static_cast<za::U32>(ring))
    {
        from = slot.read - static_cast<za::U32>(ring) + 1; // (older ones gone from the ring)
    }
    za::U32 s = from;
    while(s <= slot.read && slot.seqs[s % ring] != s)
    {
        s++;
    }
    if(s > slot.read)
    {
        return false;
    }
    if(s != from && !skipGap)
    {
        if(gap)
        {
            *gap = true;
        }
        return false;
    }
    if(slot.last != 0)
    {
        slot.counts.lost += static_cast<int>(s - slot.last - 1);
        if(s != slot.last + 1 && vr_mp_watch.value >= 3.f)
        {
            Con_Printf("mp_netmove_skip: moves %u-%u never came (read %u, tick %u, %s)\n", slot.last + 1, s - 1, slot.read,
                slot.tick, gap ? "the tick's first" : "over the depth");
        }
    }
    slot.last = s;
    slot.counts.run++;
    WireMove m;
    dequantize(slot.moves[s % ring], m);
    applying = slot.came[s % ring];
    apply(client, m, s == slot.read);
    return true;
}

// The kept moves run until at most `keep` wait (by number: a lost one between counts).
void runWaiting(client_t* client, ServerSlot& slot, ApplyFn apply, za::U32 keep)
{
    while(slot.read > slot.last && slot.read - slot.last > keep && runOldest(client, slot, apply))
    {
    }
}

// (B20) A buffer tick begun: the moves' lateness read since the last one kept in the window -> the moves left waiting
// after this tick at most. vr_net_movebuffer at least; with vr_net_movebuffer_max over it, the spread of the lateness
// over the window (his jitter, a lost packet's move come with the next, a reordered one), up to vr_net_movebuffer_max:
// a move then runs a whole depth after the earliest of its kind would, never sooner than it may come, so a jittery
// link's moves still run one a tick (each meeting the movers where his replay met them); a quiet link's depth is the
// least again once the window has passed (the moves over it run together then: the latency back).
za::U32 bufferDepth(ServerSlot& slot)
{
    const int w = static_cast<int>(slot.tick % static_cast<za::U32>(lateWindow));
    slot.lateLo[w] = slot.curLo;
    slot.lateHi[w] = slot.curHi;
    slot.lateAny[w] = slot.curAny;
    slot.curAny = false;
    slot.tick++;
    const float most = static_cast<float>(ring / 2);
    const float least = za::clamp(vr_net_movebuffer.value, 1.f, most);
    const float cap = za::clamp(vr_net_movebuffer_max.value, least, most);
    if(cap <= least)
    {
        return static_cast<za::U32>(least);
    }
    bool any = false;
    int lo = 0, hi = 0;
    for(int k = 0; k < lateWindow; k++)
    {
        if(slot.lateAny[k])
        {
            lo = any ? za::min(lo, slot.lateLo[k]) : slot.lateLo[k];
            hi = any ? za::max(hi, slot.lateHi[k]) : slot.lateHi[k];
            any = true;
        }
    }
    const float spread = any ? static_cast<float>(hi - lo) : 0.f;
    slot.counts.spreadMax = za::max(slot.counts.spreadMax, hi - lo);
    return static_cast<za::U32>(za::clamp(spread, least, cap));
}
} // namespace

bool serverRead(client_t* client, ApplyFn apply, bool stale)
{
    const int slotNum = static_cast<int>(client - svs.clients);
    ServerSlot* slot = slotOf(slotNum, true);
    const int start = msg_readcount;
    za::U32 snapAck, base, first;
    if(!slot || !readVarint(snapAck) || !readVarint(base))
    {
        return false;
    }
    if(!stale)
    {
        netsnap::serverAck(slotNum, snapAck); // (B20: an overtaken one's is older)
    }
    const int count = MSG_ReadByte();
    if(count < 1 || count > repeats + 1 || !readVarint(first) || first == 0 || first > 0x7FFFFFFFu)
    {
        return false; // (a number past any game's: the sums below would wrap, and runOldest's search with them)
    }
    slot->sends = true;
    // (mpflakes) The quiet allowance: a gap before this packet (his frame hitch: none came) is allowed his moves' times
    // after it, spent as the server's time goes on (the burst of moves his frame's catch-up ticks make all name the
    // newest message from before the hitch; once he reads the server's again, his times are fresh).
    const double now = sv.qcvm.time;
    const double gap = slot->heardAt >= 0.0 ? za::max(now - slot->heardAt, 0.0) : 0.0;
    slot->quiet = za::max(gap, slot->quiet - gap);
    slot->heardAt = now;
    const Came came{now, slot->quiet};
    bool decodable = base == 0 || slot->seqs[base % ring] == base;
    Quant prev = base != 0 && decodable ? slot->moves[base % ring] : zeroQuant;
    if(!decodable)
    {
        slot->counts.undecodable++;
    }
    Quant cur;
    const bool lockstep = vr_mp_lockstep.value >= 1.f;
    const bool buffered = buffering();
    if(stale && (!buffered || lockstep))
    {
        return true; // (its moves run already or by number: nothing to keep)
    }
    if(!buffered && slot->read > slot->last)
    {
        runWaiting(client, *slot, apply, 0); // (the buffer just turned off: what it kept runs first, in order)
    }
    for(int i = 0; i < count; i++)
    {
        const za::U32 s = first + static_cast<za::U32>(i);
        if(!readDelta(prev, cur))
        {
            return false;
        }
        prev = cur;
        if(!decodable)
        {
            continue; // (read past: the message stays in step)
        }
        if(buffered && s > slot->last && slot->seqs[s % ring] != s)
        {
            // (B20) its lateness, first come: the buffer's ticks less its number (a steady link: the same each move)
            const int late = static_cast<int>(slot->tick - s);
            slot->curLo = slot->curAny ? za::min(slot->curLo, late) : late;
            slot->curHi = slot->curAny ? za::max(slot->curHi, late) : late;
            slot->curAny = true;
        }
        if(s + static_cast<za::U32>(ring) > slot->last)
        {
            if(slot->seqs[s % ring] != s)
            {
                slot->came[s % ring] = came; // (first read: a repeat keeps its first arrival)
            }
            slot->seqs[s % ring] = s;
            slot->moves[s % ring] = cur;
        }
        if(lockstep)
        {
            if(slot->last == 0 && (slot->low == 0 || s < slot->low))
            {
                slot->low = s;
            }
            continue; // (kept: the world's step runs it, serverRunNext)
        }
        if(buffered)
        {
            // (B19: kept for the ticks to run, serverRunBuffered; read = the newest, acknowledged)
            if(s > slot->last && s > slot->read)
            {
                if(slot->last == 0 && (slot->low == 0 || s < slot->low))
                {
                    slot->low = s;
                }
                slot->counts.recovered += i == count - 1 ? 0 : 1;
                slot->read = s;
            }
            continue;
        }
        if(s <= slot->last)
        {
            continue; // (run already: a repeat)
        }
        if(slot->last > 0 && s > slot->last + 1)
        {
            slot->counts.lost += static_cast<int>(s - slot->last - 1);
        }
        const bool newest = i == count - 1;
        slot->counts.recovered += newest ? 0 : 1;
        slot->counts.run++;
        slot->last = s;
        WireMove m;
        dequantize(cur, m);
        applying = came;
        apply(client, m, newest);
    }
    slot->counts.bytes += msg_readcount - start + 1;
    return true;
}

void serverWriteAck(sizebuf_t* msg, int slotNum)
{
    const ServerSlot* slot = slotOf(slotNum, false);
    if(!slot || !slot->sends || slot->last == 0 || msg->cursize + 6 > msg->maxsize)
    {
        return;
    }
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_MOVEACK);
    MSG_WriteLong(msg, static_cast<int>(slot->read > slot->last ? slot->read : slot->last)); // (kept: read, B19)
}

bool serverSends(int slotNum)
{
    const ServerSlot* slot = slotOf(slotNum, false);
    return slot && slot->sends;
}

bool serverTakesStale(int slotNum)
{
    return serverSends(slotNum) && buffering();
}

za::U32 serverLast(int slotNum)
{
    const ServerSlot* slot = slotOf(slotNum, false);
    return slot ? slot->last : 0;
}

namespace
{
[[nodiscard]] za::U32 nextOf(const ServerSlot& slot)
{
    const za::U32 next = slot.last != 0 ? slot.last + 1 : slot.low;
    return next != 0 && slot.seqs[next % ring] == next ? next : 0;
}

// vr_mp_lockstep_movelog's scratch (big: off the stack).
Quant moveLogQuant;

// (B14e) A move the lockstep runs, every value as the world gets it (its pose clock the server's), 64 a line: two runs'
// moves compared (Misc/quakevr/mp: mp_test.py lock_moves_diff).
void logMove(int slotNum, za::U32 rel, const WireMove& m)
{
    quantize(m, moveLogQuant);
    const int n = numValues();
    for(int part = 0; part * 64 < n; part++)
    {
        char line[1024];
        int len = q_snprintf(line, sizeof(line), "mp_movelog: slot=%d rel=%u part=%d", slotNum, rel, part);
        for(int i = part * 64; i < n && i < part * 64 + 64 && len < static_cast<int>(sizeof(line)) - 16; i++)
        {
            len += q_snprintf(line + len, sizeof(line) - static_cast<za::SizeT>(len), " %d", moveLogQuant.v[i]);
        }
        Con_Printf("%s\n", line);
    }
}
} // namespace

bool serverHasNext(int slotNum)
{
    const ServerSlot* slot = slotOf(slotNum, false);
    return slot && nextOf(*slot) != 0;
}

bool serverRunNext(client_t* client, ApplyFn apply)
{
    ServerSlot* slot = slotOf(static_cast<int>(client - svs.clients), false);
    const za::U32 next = slot ? nextOf(*slot) : 0;
    if(next == 0)
    {
        return false;
    }
    slot->last = next;
    slot->counts.run++;
    WireMove m;
    dequantize(slot->moves[next % ring], m);
    if(m.vr.poseClock >= 0.f)
    {
        // (B14c) The poses' clock from the first move the world ran, a step a move: the client's own count starts at
        // its connection, and the first move the server runs (slot->low, the oldest read since the slot's reset) is a
        // matter of timing; the melee's speeds over the clock's 10-microsecond steps (.vr_posetime, .mh_hdt) went
        // apart between runs by one of them.
        m.vr.poseClock = static_cast<float>(
            za::fmod(static_cast<double>(next - slot->low) * host_frametime, static_cast<double>(VR_POSE_CLOCK_WRAP)));
    }
    if(vr_mp_lockstep_movelog.value >= 1.f)
    {
        logMove(static_cast<int>(client - svs.clients), next - slot->low, m);
    }
    applying = Came{sv.qcvm.time, 0.0}; // (the lockstep's: judged as it runs, as before -- the same every run)
    apply(client, m, true);
    return true;
}

void serverRunBuffered(client_t* client, ApplyFn apply)
{
    ServerSlot* slot = slotOf(static_cast<int>(client - svs.clients), false);
    if(!slot || !slot->sends || vr_mp_lockstep.value >= 1.f)
    {
        return;
    }
    if(!buffering())
    {
        runWaiting(client, *slot, apply, 0); // (turned off: what it kept, at once)
        return;
    }
    const za::U32 keep = bufferDepth(*slot);
    slot->counts.depth = static_cast<int>(keep);
    slot->counts.depthMax = za::max(slot->counts.depthMax, slot->counts.depth);
    // (B20) a gap before the oldest kept (a move reordered on the way) waits while the moves kept are within the depth
    bool gap = false;
    if(!runOldest(client, *slot, apply, slot->read - slot->last > keep, &gap))
    {
        slot->counts.ticksNone++;
        slot->counts.ticksGap += gap ? 1 : 0;
        slot->counts.waitSum += slot->read > slot->last ? slot->read - slot->last : 0;
        return;
    }
    const int before = slot->counts.run;
    runWaiting(client, *slot, apply, keep);
    if(slot->counts.run != before)
    {
        slot->counts.ticksMore++;
    }
    else
    {
        slot->counts.ticksOne++;
    }
    slot->counts.waitSum += slot->read > slot->last ? slot->read - slot->last : 0;
}

void serverReset(int slotNum)
{
    if(ServerSlot* slot = slotOf(slotNum, false))
    {
        slot->last = 0;
        slot->low = 0;
        slot->read = 0;
        memset(slot->seqs, 0, sizeof(slot->seqs));
        slot->counts = {};
        slot->tick = 0;
        memset(slot->lateAny, 0, sizeof(slot->lateAny));
        slot->curAny = false;
        slot->sends = false;
    }
    if(ServerSlot* slot = slotOf(slotNum, true))
    {
        // (his silence counted from his connection's or map's start: the moves after his load name its messages)
        slot->heardAt = sv.active ? sv.qcvm.time : -1.0;
        slot->quiet = 0.0;
    }
}

Came serverCame()
{
    return applying;
}

ServerCounts serverCounts(int slotNum)
{
    const ServerSlot* slot = slotOf(slotNum, false);
    return slot ? slot->counts : ServerCounts{};
}

namespace
{

// vr_netmove_selftest's scratch (big: off the stack).
struct SelftestScratch
{
    WireMove m, back;
    Quant q, q2, base, read;
    byte data[4096];
};
SelftestScratch selftestScratch;

void fillMove(WireMove& m, int seed)
{
    m = WireMove{};
    const float s = static_cast<float>(seed);
    VrMove& r = m.vr;
    r.origin = glm::vec3{512.f + s * 3.1f, -1024.5f + s, 64.f};
    m.time = 1234.567f + s * 0.0139f;
    m.frameTime = 0.01389f + s * 0.0001f;
    m.viewBack = 0.04f + s * 0.001f;
    m.viewAngles = glm::vec3{-10.f + s, 271.3f, 0.5f};
    m.forward = 200;
    m.impulse = seed & 1;
    for(int h = 0; h < 2; h++)
    {
        r.hands[h].pos = r.origin + glm::vec3{10.f + s * 0.37f, -5.f - static_cast<float>(h), 40.f};
        r.hands[h].rot = glm::vec3{12.345f, -170.f + s, 33.3f};
        r.hands[h].vel = glm::vec3{0.123f * s, -1.5f, 2.25f};
        r.drawn[h].flags = VrDrawnHand::handBit;
        r.drawn[h].handOrigin = r.hands[h].pos;
        r.drawn[h].curl[1][2] = 0.4f + 0.01f * s;
    }
    r.poseClock = 1000.f + s * 0.0139f;
}

} // namespace

void selftest_f()
{
    SelftestScratch& t = selftestScratch;
    testkit::Tally tally;
    const auto check = [&](bool ok, const char* what) {
        tally.check(ok);
        Con_Printf("netmove selftest: %s %s\n", ok ? "PASS" : "FAIL", what);
    };
    check(valueCount > 300 && valueCount <= maxValues, va("%d values a move (at most %d)", valueCount, maxValues));
    // Quantized, read back, quantized again: the same integers (client and server bases agree).
    fillMove(t.m, 7);
    quantize(t.m, t.q);
    dequantize(t.q, t.back);
    quantize(t.back, t.q2);
    check(memcmp(t.q.v, t.q2.v, sizeof(t.q.v)) == 0, "a move quantized, read back and quantized again: the same");
    const float posErr = glm::distance(t.back.vr.hands[1].pos, t.m.vr.hands[1].pos);
    const float angErr = glm::distance(t.back.vr.hands[1].rot, t.m.vr.hands[1].rot);
    check(posErr <= 0.014f && angErr <= 0.006f && t.back.vr.magBox[1][0] == glm::vec3{0.f},
        va("its hand within %.4f units and %.4f degrees, a zero box still zero", posErr, angErr));
    // Deltas written and read: the same integers; an idle move's few.
    const sizebuf_t savedMsg = net_message;
    const int savedCount = msg_readcount;
    const qboolean savedBad = msg_badread;
    bool same = true;
    int idleBytes = 0, fullBytes = 0;
    for(int k = 0; k < 3; k++)
    {
        fillMove(t.m, 0);
        quantize(t.m, t.base);
        fillMove(t.m, k == 0 ? 0 : k == 1 ? 1 : 40);
        if(k == 0)
        {
            t.m.time += 0.0139f;
            t.m.vr.poseClock += 0.0139f;
        }
        quantize(t.m, t.q);
        sizebuf_t buf{};
        buf.data = t.data;
        buf.maxsize = sizeof(t.data);
        writeDelta(&buf, k == 2 ? zeroQuant : t.base, t.q);
        (k == 0 ? idleBytes : fullBytes) = buf.cursize;
        net_message = buf;
        MSG_BeginReading();
        same = same && readDelta(k == 2 ? zeroQuant : t.base, t.read) && msg_readcount == buf.cursize &&
               memcmp(t.read.v, t.q.v, sizeof(t.q.v)) == 0;
    }
    net_message = savedMsg;
    msg_readcount = savedCount;
    msg_badread = savedBad;
    check(same, "deltas (none, small, from zeros) read back the same");
    check(idleBytes <= 16, va("an idle move (its time and pose clock only) in %d bytes; one from zeros %d", idleBytes,
                               fullBytes));
    tally.verdict("netmove selftest");
}

void init()
{
    Cmd_AddCommand("vr_netmove_selftest", selftest_f);
}

} // namespace qvr::netmove
