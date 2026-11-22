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
//   [a byte each changed group: its changed bytes][those bytes]), 3 an entity's update by its fields against the base's
//   (B12b, vr_netsnap.cpp fieldsEncode: its changed fields' mask, each one's change; sent only when it decodes exactly).
//
// The armed humanoids' poses in its sight (QVR_SVC_ARMPOSE, poseKey + the humanoid) are records too (B12): one sent this
// frame is in the set, one skipped (the pose's level of detail) is gone, so the client parses the same poses NetQuake's
// datagram would carry. A client's rate (vr_net_rate, his preference) caps the body: the changed records go by priority
// (the player's own state first, then the nearest and those ahead of his view; each one's wait adds to its priority), the
// rest stay as the base's on both sides and wait.
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
inline constexpr int predictKey = 2 << 16; // his walk's state (QVR_SVC_PREDICT, vr_predict.cpp)
inline constexpr int heldKey = predictKey + 1; // what his hands hold, and where (QVR_SVC_HELDPLACE, vr_held.cpp, B27)
inline constexpr int poseKey = 3 << 16;    // + an armed humanoid's entity: its pose this frame (QVR_SVC_ARMPOSE, B12)
inline constexpr int ragKey = 4 << 16;     // + a ragdoll's entity: its parts' places (QVR_SVC_RAGPOSE, vr_netragdoll.cpp)
inline constexpr int keyLimit = 5 << 16;   // every key below this

// Two kept places of an entity `span` seconds apart further than this on an axis: a teleport, not lerped across (100
// units at the frames' rate as before; a slower send rate or a lost message lets a body at twice sv_maxvelocity's
// default go further between two: mpreview B8). The drawing (interpEntity) and lag compensation (drawnAt) share it.
[[nodiscard]] constexpr float teleportCut(double span)
{
    return span * 4000.0 > 100.0 ? static_cast<float>(span * 4000.0) : 100.f;
}

// How far back from the newest message the interpolation clock may be drawing (s): its longest delay, the most it lags
// its target before it snaps there (catching up after a hitch), and three ticks. A history of places or poses kept by a
// client (the entities' here, the ragdolls' in vr_netragdoll.cpp) reaches back this far, or what it keeps is drawn held
// at its oldest: ahead of the clock (raginterp; mpopen, the ragdolls' poses).
inline constexpr double interpReach = 0.25 + 0.25 + 3.0 / 72.0;

// One snapshot: its records (sorted by key), each its bytes in `bytes`.
struct Record
{
    za::I32 key{0};
    za::I32 at{0};
    za::I32 len{0};
    bool fresh{false}; // its entry was in this snapshot (new or changed; not the base's kept: B12c's samples): decode's,
                       // and encode's `sent` (B13c: the lag compensation's places he was sent)
};
struct Snapshot
{
    za::U32 seq{0};
    za::U32 baseSeq{0}; // the snapshot it was coded against (0: none; B12b's trend)
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
// Past the budget (`soft`, a rate's, when given: the infinite priorities to `budget`): with `prio` (one a `cur` record),
// the gone entries and the highest priorities that fit go (B12's rate), else in key order until one does not fit;
// `unsent` (one a `cur` record, zeroed by the caller) marks the changed or new records that waited.
// `pflags`: the protocol's flags (its coordinates' and angles' sizes: an entity's update by its fields, B12b). `trend`
// (nullptr none): the base's own base, which both sides keep: a moving thing's origins and angles coded against where
// their last change carries them (`ahead` frames past the base, the base `span` past the older).
struct Trend
{
    const Snapshot* older{nullptr};
    int ahead{0}, span{0};
};
// `cosmetic` (one a `cur` record, or nullptr) marks the cosmetic fragments' records, whose changed entries take at most
// `cosmeticBudget` bytes (by priority, with `prio`; B12c), the rest waiting as a rate's do; with `cosmeticFill` (> 0)
// only what the other entries leave under that many bytes of body (B18). With `prio`, the ragdolls' changed records
// (keyClass 5) go only in the room every other entry leaves, under `ragSoft` bytes of body too (>= 0; ragsync2).
int encode(const Snapshot* base, const Snapshot& cur, sizebuf_t* body, int budget, Snapshot& sent, unsigned pflags,
    const Trend* trend, const float* prio = nullptr, za::U8* unsent = nullptr, int soft = -1, const za::U8* cosmetic = nullptr,
    int cosmeticBudget = -1, int cosmeticFill = -1, int ragSoft = -1);
// `tally` (keyClasses counts, or nullptr): each entry's bytes added by its key's class (keyClass).
[[nodiscard]] bool decode(const Snapshot* base, int bodyLen, Snapshot& out, unsigned pflags, const Trend* trend,
    long long* tally = nullptr);
inline constexpr int keyClasses = 6; // 0 the player state, 1 an entity, 2 an avatar, 3 the walk's state, 4 a pose, 5 a ragdoll
[[nodiscard]] constexpr int keyClass(int key)
{
    return key == clientdataKey ? 0 : key < avatarKey ? 1 : key < predictKey ? 2 : key < poseKey ? 3 : key < ragKey ? 4 : 5;
}

// Server (sv_main.c through vr_api.h). begin: whether this datagram's records go into a snapshot (a remote client that
// sends QVR_CLC_MOVES, vr_net_snapshots on); then take() cuts each record off the end of `msg`; end() writes the snapshot.
[[nodiscard]] bool serverBegin(client_t* client);
// (B13c) vr_net_sendhz: whether a remote snapshot client gets his datagram this server frame (on his beat), else held.
[[nodiscard]] bool serverSendsNow(client_t* client);
[[nodiscard]] bool serverRecording();
// (ragsync2) The player entity of the client whose snapshot is being made (0: none): his own hands' holds in it.
[[nodiscard]] int serverPlayer();
// (review 2026-11-18, finding 12) What a remote client may be sent a second, B/s (0: no cap): his vr_net_rate under the
// server's vr_net_rate_max (his 0: the server's). The snapshot's bucket and the voice relay's share of it.
[[nodiscard]] int clientRate(const client_t* client);
void serverTake(sizebuf_t* msg, int at, int key);
void serverRecord(int key, const za::U8* data, int len);
void serverEnd(sizebuf_t* msg);
void serverCharge(int bytes); // the datagram whose snapshot serverEnd wrote: its size, off his rate's bucket
void serverAck(int slot, za::U32 seq); // QVR_CLC_MOVES's acknowledgement
void serverReset(int slot, bool keepCounts = false); // a new connection's (or, counts kept, a new map's spawn)
// (B13c) What his client drew a record from: the server times of his kept snapshots made at or before `upTo` that
// carried record `key` new or changed (his interpolation buffer took a place from each), newest first, at most `most`,
// back to one without the record (out of his sight: his buffer starts again after it) -> their count; `oldest`: the
// oldest time walked (-1: no snapshots kept, NetQuake's datagrams); `sightBegan`: the walk ended at one without it.
int serverSentTimes(int slot, int key, double upTo, double* out, int most, double& oldest, bool& sightBegan);
struct ServerCounts
{
    int sent{0}, full{0}, delta{0}, cut{0};
    int rate{0};           // his rate (B/s; 0 none)
    long long waited{0};   // changed records that waited (his rate, or a full datagram)
    int waitFrames{0};     // snapshots in which any waited
    long long bytes{0};
    long long cosmeticBytes{0}; // (B12c) the cosmetic fragments' entries' bytes ...
    long long cosmeticWaited{0}; // ... and their changed records that waited for their budget (or his rate)
    long long ragHeld{0};  // (ragsync) ragdolls' changed records held by vr_net_ragdoll_bytes
    long long ragWaited{0}; // (ragsync2) ragdolls' records that waited (held, or no room left by the others)
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

// What this client read from its server, by kind (B12's byte table: `vr_net_bytes`, vr_mp_status's "mp_netbytes:").
// noteMessage: CL_ReadFromServer read a message (CL_GetMessage's 1 reliable, 2 unreliable); noteCommand:
// CL_ParseServerMessage's next command begins at `at` (the message's size: it ended).
void noteMessage(int kind);
void noteCommand(int at);
void bytesReport();
void bytesReset();

// The interpolation buffer. active: a remote server's snapshots read, vr_net_interp on (not a demo). frame: once a frame
// before the entities are placed (its clock). entity: entity `num`'s place and angles a delay behind; false: as before.
// time: the server time the others are drawn at (active only). delayMs, holds: vr_mp_status.
[[nodiscard]] bool interpActive();
void interpFrame();
[[nodiscard]] bool interpEntity(int num, float origin[3], float angles[3]);
[[nodiscard]] double interpTime();
[[nodiscard]] double interpDelayMs();
[[nodiscard]] int interpHolds();
[[nodiscard]] double interpNewest(); // the newest message's server time (vr_mp_trace)
// Entity `num`'s sample `back` messages before its newest (0: the newest): its server time and place (vr_mp_trace).
[[nodiscard]] bool interpSample(int num, int back, double& t, float origin[3]);
// (mpopen) Entity `num`'s interpolation as last drawn: `way` 0 between two messages, 1 held at its oldest (the clock before
// it), 2 held at its newest (past it), 3 a teleport's cut, 4 not interpolated; its messages kept; how often its history
// started again and why the last time (1 its record missing from the snapshot before, 2 a whole snapshot, 3 its model).
[[nodiscard]] bool interpWay(int num, int& way, int& samples, int& restarts, int& why);

// vr_netsnap_selftest (the codec: full, delta, gone, unchanged, a budget's cut; ends "netsnap selftest: N passed, M
// failed").
void selftest_f();
void init();

} // namespace qvr::netsnap
