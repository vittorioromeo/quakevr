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
    float frameTime{0.f};        // the client frame it covers, seconds (B8: the server runs his walk for it, as his prediction does)
    float viewBack{0.f};         // how far behind `time` the others were drawn, seconds (his interpolation buffer: B8b's lag compensation)
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
// (B13) The moves' acknowledgements' round trip in seconds, smoothed (0: none yet on this connection); `newest`: the last.
[[nodiscard]] double clientRoundTrip(double* newest = nullptr);
// The newest move sent (0: none yet) and a sent move by number, as the server reads it (false: gone from the ring).
[[nodiscard]] za::U32 clientNewest();
[[nodiscard]] bool clientMoveAt(za::U32 seq, WireMove& m);
// The moves sent on this connection the server has not acknowledged (the harness's lockstep: vr_mp_lockstep_ahead).
[[nodiscard]] za::U32 clientPending();
[[nodiscard]] za::U32 clientSent(); // the moves sent since clientClear
[[nodiscard]] int clientPeakBytes(); // (mpoverflow) the largest QVR_CLC_MOVES written (vr_net_stats)

// Server: QVR_CLC_MOVES's body from client slot `slot`; `apply` runs each move it has not run yet, in order. False: a
// malformed message (the client dropped).
using ApplyFn = void (*)(client_t* client, const WireMove& m, bool newest);
// (B20) stale: a datagram overtaken on the way (net_stalemessage): its snapshot acknowledgement ignored (older), its
// moves only kept for the buffer (one it no longer waits for: nothing).
[[nodiscard]] bool serverRead(client_t* client, ApplyFn apply, bool stale = false);
// (B20) Its overtaken datagrams are read (net_dgrm.c): it sends QVR_CLC_MOVES and the move buffer is on.
[[nodiscard]] bool serverTakesStale(int slot);
// The acknowledgement into the client's datagram (QVR_SVC_MOVEACK: the newest move read, kept by the buffer or run),
// once it sent QVR_CLC_MOVES.
void serverWriteAck(sizebuf_t* msg, int slot);
void serverReset(int slot);
[[nodiscard]] bool serverSends(int slot); // it sent QVR_CLC_MOVES since its connection or map began (its snapshots too)
[[nodiscard]] za::U32 serverLast(int slot); // the newest move run (0: none)
// The harness's lockstep (vr_mp_lockstep: MP_PLAN B14b): serverRead keeps the moves without running them; each world
// step runs the next one by number (the first: the oldest read since the slot's reset). serverHasNext: it was read.
[[nodiscard]] bool serverHasNext(int slot);
bool serverRunNext(client_t* client, ApplyFn apply);
// (B19) The move buffer (vr_net_movebuffer; not under the lockstep): serverRead keeps the moves (acknowledging them) and
// each server tick runs the oldest kept, in order, then more while over vr_net_movebuffer wait: his moves meet the
// movers a tick apart as his replay of them did, however his packets bunch on the way. Off: what it kept runs at once.
void serverRunBuffered(client_t* client, ApplyFn apply);
// (mpflakes) The move `apply` is running: when the server first read it (sv.qcvm.time; a move the buffer kept runs
// ticks later) and the quiet allowance it came with: the silence before its client's packets resumed after a gap (his
// frame hitch: the moves made after it name his newest message from before it), spent as the server's time goes on.
struct Came
{
    double at{0.0};
    double quiet{0.0};
};
[[nodiscard]] Came serverCame();
// vr_mp_status: moves run, lost (never came, repeats included), recovered (run from a repeat), undecodable (the base
// gone), bytes read.
// (B19) The move buffer's ticks: those that ran one move, more than one (a backlog over vr_net_movebuffer), none (none
// kept: a late move, or his moves slower than the ticks), and the moves left waiting after them, summed.
struct ServerCounts
{
    int run{0}, lost{0}, recovered{0}, undecodable{0};
    long long bytes{0};
    int ticksOne{0}, ticksMore{0}, ticksNone{0};
    long long waitSum{0};
    // (B20) the ticks of ticksNone that waited for a gap (a reordered move), the buffer's depth now and at most, the
    // moves' lateness spread at most (vr_net_movebuffer_max)
    int ticksGap{0}, depth{0}, depthMax{0}, spreadMax{0};
};
[[nodiscard]] ServerCounts serverCounts(int slot);

// vr_netmove_selftest (the codec: values, round trips, deltas read back, an idle move's bytes; ends "netmove selftest:
// N passed, M failed").
void selftest_f();
void init();

} // namespace qvr::netmove
