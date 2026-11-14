// vr_netmove.hpp -- a remote client's moves up as quantized deltas with repeats (QVR_CLC_MOVES; MP_PLAN.md, B7).
//
// The loopback client (single player, the listen host's own) still sends NetQuake's clc_move with the VR block of floats
// (vr_move.cpp): nothing of single player changes. A remote client (a Quake VR server of QVR_NET_VERSION 17 or later)
// sends each packet's moves as one QVR_CLC_MOVES message instead:
//
//   [byte QVR_CLC_MOVES][varint the newest snapshot read, 0 none: vr_netsnap.hpp][varint base seq, 0 none][byte count][varint first seq] then `count` moves, each the delta of its
//   quantized values (quantize: every field an integer, places in 64ths of a unit from the move's origin, angles in
//   65536ths of a turn, speeds in 1024ths...) against the one before it (the first against the base: the newest move
//   the server acknowledged, QVR_SVC_MOVEACK; none: against zeros).
//
// A delta is a group mask (a bit a group of 8 values: any changed), each changed group's mask byte, then its changed
// values as zigzag varints. Each packet repeats the moves after the acknowledged one, at most `repeats` before the new one:
// a lost packet's moves come in the next one (the server runs each move once, in order, by its sequence number).
// The client keeps the moves it sent and the server those it read (`ring` moves back), so both decode the same values.

#pragma once

#include "vr_engine.hpp"
#include "vr_move.hpp"

#include "Zancle/Base/IntTypes.hpp"

namespace qvr::netmove
{

// A whole move as the client sends it: NetQuake's part (clc_move's) and the VR block.
struct WireMove
{
    float time{0.f};             // the client's cl.mtime[0] (the server's ping)
    glm::vec3 viewAngles{0.f};
    int forward{0}, side{0}, up{0};
    int buttons{0};              // clc_move's bits (1 attack, 2 jump)
    int impulse{0};
    VrMove vr;
};

inline constexpr int maxValues = 512; // (a move's integers: numValues() <= this)
inline constexpr int ring = 64;       // moves each side keeps (a base older than this: the move sent whole)
inline constexpr int repeats = 2;     // older moves repeated in a packet besides the new one

// An unsigned varint (7 bits a byte, low first) written; read from net_message (false: it ran out). (QVR_SVC_SNAPSHOT too.)
void writeVarint(sizebuf_t* buf, za::U32 u);
[[nodiscard]] bool readVarint(za::U32& u);

// The integers a move is sent as.
struct Quant
{
    za::I32 v[maxValues]{};
};
[[nodiscard]] int numValues();
void quantize(const WireMove& m, Quant& q);
void dequantize(const Quant& q, WireMove& m);
// `q` as a delta from `base` (the bytes after the move's place in the message); readDelta: false when the message ran out.
void writeDelta(sizebuf_t* buf, const Quant& base, const Quant& q);
[[nodiscard]] bool readDelta(const Quant& base, Quant& q);

// Client: whether this connection sends QVR_CLC_MOVES (a remote Quake VR server); then CL_SendMove calls writeMoves
// instead of writing clc_move. `record`: this move is sent (not one of the two dumped after a signon, not a demo's).
[[nodiscard]] bool clientUses();
void writeMoves(sizebuf_t* buf, const WireMove& m, bool record);
// QVR_SVC_MOVEACK's body; a new connection's or map's state (VR_OnClientClearState).
void parseAck();
void clientClear();

// Server: QVR_CLC_MOVES's body from client slot `slot`; `apply` runs each move it has not run yet, in order. False: a
// malformed message (the client dropped).
using ApplyFn = void (*)(client_t* client, const WireMove& m, bool newest);
[[nodiscard]] bool serverRead(client_t* client, ApplyFn apply);
// The acknowledgement into the client's datagram (QVR_SVC_MOVEACK: the newest move read), once it sent QVR_CLC_MOVES.
void serverWriteAck(sizebuf_t* msg, int slot);
void serverReset(int slot);
[[nodiscard]] bool serverSends(int slot); // it sent QVR_CLC_MOVES since its connection or map began (its snapshots too)
// vr_mp_status: moves run, lost (never came, repeats included), recovered (run from a repeat), undecodable (the base
// gone), bytes read.
struct ServerCounts
{
    int run{0}, lost{0}, recovered{0}, undecodable{0};
    long long bytes{0};
};
[[nodiscard]] ServerCounts serverCounts(int slot);

// vr_netmove_selftest (the codec: values, round trips, deltas read back, an idle move's bytes; ends "netmove selftest:
// N passed, M failed").
void selftest_f();
void init();

} // namespace qvr::netmove
