// vr_netsnap.hpp -- snapshots down to a remote client, acknowledged and delta-coded (QVR_SVC_SNAPSHOT), and the entities'
// interpolation buffer (MP_PLAN.md, B7b).
//
// A remote client (one that sends QVR_CLC_MOVES: vr_netmove.hpp) acknowledges in each packet of moves the newest snapshot
// it read. Each server frame its datagram's player state (svc_clientdata), entities (their NetQuake updates, U_SIGNAL) and
// the other players' avatars (QVR_SVC_AVATAR) are records of a snapshot, each the bytes NetQuake's writers wrote, keyed
// (0 the player state, an entity's number, avatarKey + a player's entity). The snapshot goes as one message, coded against
// the one the client acknowledged (the server keeps `ring` back per client, the client the same):
//
//   [byte svc_quakevr][byte QVR_SVC_SNAPSHOT][varint seq][varint seq - base, 0 none][varint body bytes] then each record
//   that differs from the base's, by key: [varint (key - previous key) << 2 | op] (the first key's previous -1); op 0
//   gone, 1 whole ([varint length][bytes]), 2 the base's changed (its length; [a byte a 8 groups of 8 bytes: any changed]
//   [a byte each changed group: its changed bytes][those bytes]).
//
// A record the same as the base's is not sent: an idle player and what he sees nearly free. The client puts the base's
// records and the changed ones together and parses every one, as if the server had sent NetQuake's whole datagram: the
// rest of the client (its entities, lerps, avatars) is unchanged. The loopback client and single player get NetQuake's
// messages as before.
//
// The interpolation buffer: a remote client keeps each entity's places from the last messages (their server times) and
// draws the others a delay behind the newest (adaptive: the longest recent gap between messages, a loss's), between the
// two messages around that time, turns slerped; the delay's clock speeds up or slows down a little to follow (never a
// jump). NetQuake's two-message lerp clamped its clock to the newest message: under loss a body stopped, then jumped.

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"

namespace qvr::netsnap
{

inline constexpr int ring = 32;           // snapshots each side keeps (an acknowledgement older than this: sent whole)
inline constexpr int avatarKey = 1 << 16; // + a player's entity: his avatar's record
inline constexpr int clientdataKey = 0;   // the player's own state (svc_clientdata)

// One snapshot: its records (sorted by key), each its bytes in `bytes`.
struct Record
{
    za::I32 key{0};
    za::I32 at{0};
    za::I32 len{0};
};
struct Snapshot
{
    za::U32 seq{0};
    za::Vector<Record> records;
    za::Vector<za::U8> bytes;

    void clear();
    void add(int key, const za::U8* data, int len); // (keys in any order: sortByKey after)
    void sortByKey();
    [[nodiscard]] const za::U8* data(const Record& r) const;
};

// Codec. encode: `cur`'s records (sorted) against `base` (nullptr: none) as a QVR_SVC_SNAPSHOT body into `body`, at most
// `budget` bytes; `sent` becomes what the client will have (records past the budget: the base's). Returns the records
// written. decode: a body (from net_message's read position, `bodyLen` bytes) against `base` into `out`; false: malformed.
int encode(const Snapshot* base, const Snapshot& cur, sizebuf_t* body, int budget, Snapshot& sent);
[[nodiscard]] bool decode(const Snapshot* base, int bodyLen, Snapshot& out);

// Server (sv_main.c through vr_api.h). begin: whether this datagram's records go into a snapshot (a remote client that
// sends QVR_CLC_MOVES, vr_net_snapshots on); then take() cuts each record off the end of `msg`; end() writes the snapshot.
[[nodiscard]] bool serverBegin(client_t* client);
[[nodiscard]] bool serverRecording();
void serverTake(sizebuf_t* msg, int at, int key);
void serverRecord(int key, const za::U8* data, int len);
void serverEnd(sizebuf_t* msg);
void serverAck(int slot, za::U32 seq); // QVR_CLC_MOVES's acknowledgement
void serverReset(int slot, bool keepCounts = false); // a new connection's (or, counts kept, a new map's spawn)
struct ServerCounts
{
    int sent{0}, full{0}, delta{0}, cut{0};
    long long bytes{0};
    za::U32 seq{0}, acked{0};
};
[[nodiscard]] ServerCounts serverCounts(int slot);

// Client. ack: the newest snapshot read (QVR_CLC_MOVES), 0 none. parse: QVR_SVC_SNAPSHOT's body (its records parsed).
[[nodiscard]] za::U32 clientAck();
void parse();
void clientClear();
// A demo begun mid-game (VR_WriteDemoState): none acknowledged until a whole snapshot comes, so the demo has one to start
// from (the deltas before it are kept as they are read).
void clientWantWhole();
struct ClientCounts
{
    int decoded{0}, undecodable{0}, bad{0}, records{0};
    long long bytes{0};
};
[[nodiscard]] ClientCounts clientCounts();

// The interpolation buffer. active: a remote server's snapshots read, vr_net_interp on (not a demo). frame: once a frame
// before the entities are placed (its clock). entity: entity `num`'s place and angles a delay behind; false: as before.
// time: the server time the others are drawn at (active only). delayMs, holds: vr_mp_status.
[[nodiscard]] bool interpActive();
void interpFrame();
[[nodiscard]] bool interpEntity(int num, float origin[3], float angles[3]);
[[nodiscard]] double interpTime();
[[nodiscard]] double interpDelayMs();
[[nodiscard]] int interpHolds();

// vr_netsnap_selftest (the codec: full, delta, gone, unchanged, a budget's cut; ends "netsnap selftest: N passed, M
// failed").
void selftest_f();
void init();

} // namespace qvr::netsnap
