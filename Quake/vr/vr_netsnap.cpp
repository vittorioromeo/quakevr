// vr_netsnap.cpp -- snapshots down, acknowledged and delta-coded, and the entities' interpolation buffer (vr_netsnap.hpp;
// MP_PLAN.md, B7b).

#include "vr_netsnap.hpp"
#include "vr_netmove.hpp"
#include "vr_mpavatar.hpp"
#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_protocol.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <algorithm>
#include <string.h>

extern "C" void CL_ParseUpdate(int bits);  // cl_parse.c: an entity's update (its first byte read)
extern "C" void CL_ParseClientdata(void);  // cl_parse.c: svc_clientdata's body

namespace qvr::netsnap
{

void Snapshot::clear()
{
    seq = 0;
    records.clear();
    bytes.clear();
}

void Snapshot::add(int key, const za::U8* d, int len)
{
    records.emplaceBack(Record{key, static_cast<za::I32>(bytes.size()), len});
    if(len > 0)
    {
        bytes.emplaceBackRange(d, static_cast<za::SizeT>(len));
    }
}

void Snapshot::sortByKey()
{
    std::sort(records.begin(), records.end(), [](const Record& a, const Record& b) { return a.key < b.key; });
}

const za::U8* Snapshot::data(const Record& r) const
{
    return bytes.data() + r.at;
}

namespace
{

constexpr int opGone = 0;
constexpr int opWhole = 1;
constexpr int opDelta = 2;
constexpr int maxDeltaLen = 2048; // a longer record always goes whole
constexpr int headerMost = 2 + 5 * 3; // svc_quakevr, the sub-command and three varints

[[nodiscard]] int varintSize(za::U32 u)
{
    int n = 1;
    for(; u >= 0x80u; u >>= 7)
    {
        n++;
    }
    return n;
}

[[nodiscard]] za::U32 entryHead(int key, int prevKey, int op)
{
    return (static_cast<za::U32>(key - prevKey) << 2) | static_cast<za::U32>(op);
}

// `cur`'s bytes as a delta from `base`'s (the same length): a byte a 8 groups (any changed), a mask byte each changed
// group, then its changed bytes. Its size; written into `buf` unless nullptr.
int bytesDelta(sizebuf_t* buf, const za::U8* base, const za::U8* cur, int len)
{
    const int groups = (len + 7) / 8;
    const int tops = (groups + 7) / 8;
    za::U8 masks[maxDeltaLen / 8]{};
    za::U8 top[maxDeltaLen / 64]{};
    int size = tops;
    for(int i = 0; i < len; i++)
    {
        if(base[i] != cur[i])
        {
            masks[i / 8] = static_cast<za::U8>(masks[i / 8] | (1 << (i % 8)));
            size++;
        }
    }
    for(int g = 0; g < groups; g++)
    {
        if(masks[g])
        {
            top[g / 8] = static_cast<za::U8>(top[g / 8] | (1 << (g % 8)));
            size++;
        }
    }
    if(!buf)
    {
        return size;
    }
    for(int t = 0; t < tops; t++)
    {
        MSG_WriteByte(buf, top[t]);
    }
    for(int g = 0; g < groups; g++)
    {
        if(!masks[g])
        {
            continue;
        }
        MSG_WriteByte(buf, masks[g]);
        for(int k = 0; k < 8; k++)
        {
            if(masks[g] & (1 << k))
            {
                MSG_WriteByte(buf, cur[g * 8 + k]);
            }
        }
    }
    return size;
}

// bytesDelta's, read from net_message (no further than `end`) onto `out` (the base's bytes): false when malformed.
[[nodiscard]] bool readBytesDelta(za::U8* out, int len, int end)
{
    const int groups = (len + 7) / 8;
    const int tops = (groups + 7) / 8;
    za::U8 top[maxDeltaLen / 64]{};
    if(len > maxDeltaLen)
    {
        return false;
    }
    for(int t = 0; t < tops; t++)
    {
        if(msg_readcount >= end)
        {
            return false;
        }
        top[t] = static_cast<za::U8>(MSG_ReadByte());
    }
    for(int g = 0; g < groups; g++)
    {
        if(!(top[g / 8] & (1 << (g % 8))))
        {
            continue;
        }
        if(msg_readcount >= end)
        {
            return false;
        }
        const int mask = MSG_ReadByte();
        for(int k = 0; k < 8; k++)
        {
            if(!(mask & (1 << k)))
            {
                continue;
            }
            const int i = g * 8 + k;
            if(i >= len || msg_readcount >= end)
            {
                return false;
            }
            out[i] = static_cast<za::U8>(MSG_ReadByte());
        }
    }
    return !msg_badread;
}

void addFrom(Snapshot& to, const Snapshot& from, const Record& r)
{
    to.add(r.key, from.data(r), r.len);
}

} // namespace

int encode(const Snapshot* base, const Snapshot& cur, sizebuf_t* body, int budget, Snapshot& sent)
{
    sent.records.clear();
    sent.bytes.clear();
    budget = za::min(budget, body->maxsize);
    int written = 0, prevKey = -1;
    bool cut = false; // the budget reached: the rest as the base's (what the client keeps)
    za::SizeT bi = 0, ci = 0;
    const za::SizeT bn = base ? base->records.size() : 0, cn = cur.records.size();
    const auto fits = [&](int need) {
        if(!cut && body->cursize + need <= budget)
        {
            return true;
        }
        cut = true;
        return false;
    };
    while(bi < bn || ci < cn)
    {
        const Record* b = bi < bn ? &base->records[bi] : nullptr;
        const Record* c = ci < cn ? &cur.records[ci] : nullptr;
        if(c && (!b || c->key < b->key)) // new: whole
        {
            ci++;
            const za::U32 head = entryHead(c->key, prevKey, opWhole);
            if(fits(varintSize(head) + varintSize(static_cast<za::U32>(c->len)) + c->len))
            {
                netmove::writeVarint(body, head);
                netmove::writeVarint(body, static_cast<za::U32>(c->len));
                SZ_Write(body, cur.data(*c), c->len);
                addFrom(sent, cur, *c);
                prevKey = c->key;
                written++;
            }
            continue;
        }
        if(!c || b->key < c->key) // gone
        {
            bi++;
            const za::U32 head = entryHead(b->key, prevKey, opGone);
            if(fits(varintSize(head)))
            {
                netmove::writeVarint(body, head);
                prevKey = b->key;
                written++;
            }
            else
            {
                addFrom(sent, *base, *b);
            }
            continue;
        }
        bi++;
        ci++;
        if(b->len == c->len && memcmp(base->data(*b), cur.data(*c), static_cast<size_t>(c->len)) == 0)
        {
            addFrom(sent, cur, *c); // (unchanged: not sent)
            continue;
        }
        const int wholeSize = varintSize(static_cast<za::U32>(c->len)) + c->len;
        const int deltaSize = b->len == c->len && c->len <= maxDeltaLen ? bytesDelta(nullptr, base->data(*b), cur.data(*c), c->len)
                                                                        : wholeSize + 1;
        const int op = deltaSize < wholeSize ? opDelta : opWhole;
        const za::U32 head = entryHead(c->key, prevKey, op);
        if(!fits(varintSize(head) + za::min(deltaSize, wholeSize)))
        {
            addFrom(sent, *base, *b);
            continue;
        }
        netmove::writeVarint(body, head);
        if(op == opDelta)
        {
            bytesDelta(body, base->data(*b), cur.data(*c), c->len);
        }
        else
        {
            netmove::writeVarint(body, static_cast<za::U32>(c->len));
            SZ_Write(body, cur.data(*c), c->len);
        }
        addFrom(sent, cur, *c);
        prevKey = c->key;
        written++;
    }
    return written;
}

bool decode(const Snapshot* base, int bodyLen, Snapshot& out)
{
    out.records.clear();
    out.bytes.clear();
    const int end = msg_readcount + bodyLen;
    if(bodyLen < 0 || end > net_message.cursize)
    {
        return false;
    }
    za::SizeT bi = 0;
    const za::SizeT bn = base ? base->records.size() : 0;
    int prevKey = -1;
    while(msg_readcount < end)
    {
        // (a key's step is at most the last key + 1, from none: the predict record alone in a delta was refused
        // under a limit of 2 x avatarKey -- mp_loss's undecodable snapshots, B8b)
        za::U32 head;
        if(!netmove::readVarint(head) || (head >> 2) == 0 || (head & 3u) > static_cast<za::U32>(opDelta) ||
            (head >> 2) > static_cast<za::U32>(predictKey + 1))
        {
            return false;
        }
        const int key = prevKey + static_cast<int>(head >> 2);
        const int op = static_cast<int>(head & 3u);
        prevKey = key;
        while(bi < bn && base->records[bi].key < key)
        {
            addFrom(out, *base, base->records[bi++]); // (unchanged)
        }
        const Record* b = bi < bn && base->records[bi].key == key ? &base->records[bi] : nullptr;
        bi += b ? 1 : 0;
        if(op == opGone)
        {
            if(!b)
            {
                return false;
            }
            continue;
        }
        if(op == opWhole)
        {
            za::U32 len;
            if(!netmove::readVarint(len) || len > static_cast<za::U32>(end - msg_readcount))
            {
                return false;
            }
            out.add(key, net_message.data + msg_readcount, static_cast<int>(len));
            msg_readcount += static_cast<int>(len);
            continue;
        }
        if(!b)
        {
            return false;
        }
        const za::I32 at = static_cast<za::I32>(out.bytes.size());
        addFrom(out, *base, *b);
        if(!readBytesDelta(out.bytes.data() + at, b->len, end))
        {
            return false;
        }
    }
    while(bi < bn)
    {
        addFrom(out, *base, base->records[bi++]);
    }
    return msg_readcount == end && !msg_badread;
}

namespace
{

// ---- Server --------------------------------------------------------------------------------------------------------

// Per client slot: the snapshots sent (`ring` back, by sequence number), the newest it acknowledged, the counts.
struct ServerSlot
{
    za::U32 nextSeq{1}; // (kept over maps: a reset forgets the snapshots, so an old acknowledgement matches none)
    za::U32 acked{0};
    Snapshot snaps[ring];
    ServerCounts counts;
};
za::Vector<ServerSlot> serverSlots;

// The snapshot being made for the client whose datagram is being written (serverBegin .. serverEnd).
struct Building
{
    bool on{false};
    int slot{-1};
    Snapshot cur;
    za::U8 body[MAX_DATAGRAM];
};
Building building;

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

bool serverBegin(client_t* client)
{
    Building& b = building;
    b.on = false;
    if(vr_net_snapshots.value == 0.f || !client || !client->netconnection ||
        strcmp(NET_QSocketGetAddressString(client->netconnection), "LOCAL") == 0)
    {
        return false;
    }
    const int slot = static_cast<int>(client - svs.clients);
    if(!netmove::serverSends(slot) || !slotOf(slot, true))
    {
        return false; // (not a client of QVR_CLC_MOVES yet: NetQuake's datagram, as its moves are)
    }
    b.on = true;
    b.slot = slot;
    b.cur.clear();
    return true;
}

bool serverRecording()
{
    return building.on;
}

void serverTake(sizebuf_t* msg, int at, int key)
{
    if(!building.on || at < 0 || at > msg->cursize)
    {
        return;
    }
    building.cur.add(key, msg->data + at, msg->cursize - at);
    msg->cursize = at;
}

void serverRecord(int key, const za::U8* data, int len)
{
    if(building.on && len > 0)
    {
        building.cur.add(key, data, len);
    }
}

void serverEnd(sizebuf_t* msg)
{
    Building& bl = building;
    if(!bl.on)
    {
        return;
    }
    bl.on = false;
    ServerSlot* s = slotOf(bl.slot, true);
    const int room = msg->maxsize - msg->cursize - 1 - headerMost;
    if(!s || room <= 0)
    {
        return;
    }
    const za::U32 seq = s->nextSeq++;
    const Snapshot* base = nullptr;
    if(s->acked != 0 && seq - s->acked < static_cast<za::U32>(ring) && s->snaps[s->acked % ring].seq == s->acked)
    {
        base = &s->snaps[s->acked % ring];
    }
    bl.cur.sortByKey();
    Snapshot& sent = s->snaps[seq % ring];
    sizebuf_t body{};
    body.data = bl.body;
    body.maxsize = static_cast<int>(sizeof(bl.body));
    const za::SizeT before = bl.cur.records.size();
    encode(base, bl.cur, &body, room, sent);
    sent.seq = seq;
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_SNAPSHOT);
    const int at = msg->cursize;
    netmove::writeVarint(msg, seq);
    netmove::writeVarint(msg, base ? seq - base->seq : 0u);
    netmove::writeVarint(msg, static_cast<za::U32>(body.cursize));
    SZ_Write(msg, body.data, body.cursize);
    ServerCounts& c = s->counts;
    c.sent++;
    (base ? c.delta : c.full)++;
    c.cut += body.cursize >= room - 8 && sent.records.size() != before ? 1 : 0;
    c.bytes += msg->cursize - at + 2;
    c.seq = seq;
    c.acked = s->acked;
}

void serverAck(int slotNum, za::U32 seq)
{
    ServerSlot* s = slotOf(slotNum, false);
    if(s && seq == 0)
    {
        s->acked = 0; // (it has none: a new connection or map, or a demo begun; the next whole)
    }
    else if(s && seq > s->acked && seq < s->nextSeq && s->snaps[seq % ring].seq == seq)
    {
        s->acked = seq;
    }
}

void serverReset(int slotNum, bool keepCounts)
{
    if(ServerSlot* s = slotOf(slotNum, false))
    {
        s->acked = 0;
        for(Snapshot& sn : s->snaps)
        {
            sn.clear();
        }
        if(!keepCounts)
        {
            s->counts = {};
        }
    }
}

ServerCounts serverCounts(int slotNum)
{
    const ServerSlot* s = slotOf(slotNum, false);
    return s ? s->counts : ServerCounts{};
}

namespace
{

// ---- Client --------------------------------------------------------------------------------------------------------

struct ClientState
{
    Snapshot snaps[ring];
    za::U32 last{0}; // the newest read (acknowledged in our moves)
    bool wantsWhole{false}; // a demo begun: 0 acknowledged until a whole one comes (the demo's start, vr_netsnap.hpp)
    ClientCounts counts;
};
ClientState clientState;

// ---- The interpolation buffer ----------------------------------------------------------------------------------------

constexpr int historyLen = 8;           // messages kept per entity
constexpr double tick = 1.0 / 72.0;     // the server's frame
constexpr double delayMin = 1.5 * tick; // the delay's least ...
constexpr double delayMost = 0.25;      // ... and most
constexpr double delayMargin = 0.5 * tick;
constexpr double peakDecay = 0.01;      // s a second: the longest gap's hold on the delay fades this fast
constexpr double clockGain = 4.0;       // the delay's clock: this much faster a second behind (slower ahead) ...
constexpr double rateLeast = 0.85, rateMost = 1.15; // ... within these
constexpr double snapAfter = 0.25;      // s off its target: set there (a new map, a long stall)

struct Sample
{
    double t{0.0}; // the message's server time
    float origin[3]{};
    float angles[3]{};
};
struct History
{
    Sample s[historyLen];
    int n{0}, head{0};
    const qmodel_t* model{nullptr};
    za::U32 seq{0}; // the snapshot of its newest
};
struct Interp
{
    za::Vector<History> of; // by entity number
    bool valid{false};
    double est{0.0};      // the newest message's server time, as a clock (following the messages as they come)
    double rt{0.0};       // the time the others are drawn at: est - the delay, its rate eased
    double lastReal{0.0};
    double newest{0.0};   // the newest message's time
    double peak{0.0};     // the longest recent gap: how far past the newest the clock was when a message came
    double delay{0.0};
    int holds{0};         // frames the draw time passed the newest message (a gap longer than the delay)
};
Interp interp;

void interpArrive(double t)
{
    Interp& ip = interp;
    if(!ip.valid || za::abs(t - ip.est) > snapAfter)
    {
        ip.valid = true;
        ip.est = t;
        ip.newest = t;
        ip.peak = 2.0 * tick;
        ip.delay = ip.peak + delayMargin;
        ip.rt = t - ip.delay;
        ip.lastReal = realtime;
        return;
    }
    ip.peak = za::max(ip.peak, ip.est - ip.newest);
    ip.est += (t - ip.est) * 0.1; // (a nudge: the arrivals' jitter averaged)
    ip.newest = za::max(ip.newest, t);
}

// Entity `e`'s place in snapshot `seq` (the one read before it: `prevSeq`) into its history.
void interpNote(int e, za::U32 prevSeq, za::U32 seq)
{
    Interp& ip = interp;
    if(e <= 0 || e >= cl.num_entities)
    {
        return;
    }
    if(e >= static_cast<int>(ip.of.size()))
    {
        ip.of.resize(static_cast<za::SizeT>(e) + 1);
    }
    const entity_t& ent = cl_entities[e];
    History& h = ip.of[static_cast<za::SizeT>(e)];
    if(h.seq != prevSeq || prevSeq == 0 || ent.model != h.model)
    {
        h.n = 0; // (new, or back in sight, or another thing in its slot: its history starts again)
    }
    h.model = ent.model;
    h.seq = seq;
    Sample& s = h.s[h.head];
    s.t = cl.mtime[0];
    memcpy(s.origin, ent.msg_origins[0], sizeof(s.origin));
    memcpy(s.angles, ent.msg_angles[0], sizeof(s.angles));
    h.head = (h.head + 1) % historyLen;
    h.n = za::min(h.n + 1, historyLen);
}

// The snapshot's records parsed as the server's messages they are (net_message pointed at each); its entities' places
// into their histories when it is a new one (`prevSeq`: the one read before it; else 0 and seq 0).
void apply(const Snapshot& s, za::U32 prevSeq, za::U32 seq)
{
    ClientCounts& counts = clientState.counts;
    const sizebuf_t savedMsg = net_message;
    const int savedCount = msg_readcount;
    const qboolean savedBad = msg_badread;
    for(const Record& r : s.records)
    {
        net_message.data = const_cast<byte*>(s.data(r));
        net_message.cursize = r.len;
        net_message.maxsize = r.len;
        msg_readcount = 0;
        msg_badread = false;
        while(msg_readcount < r.len && !msg_badread)
        {
            const int cmd = MSG_ReadByte();
            if(cmd & U_SIGNAL)
            {
                CL_ParseUpdate(cmd & 127);
            }
            else if(cmd == svc_clientdata)
            {
                CL_ParseClientdata();
            }
            else if(!VR_ParseServerMessage(cmd))
            {
                msg_badread = true;
            }
        }
        counts.bad += msg_badread || msg_readcount != r.len ? 1 : 0;
    }
    net_message = savedMsg;
    msg_readcount = savedCount;
    msg_badread = savedBad;
    if(seq == 0)
    {
        return;
    }
    for(const Record& r : s.records)
    {
        if(r.key > clientdataKey && r.key < avatarKey)
        {
            interpNote(r.key, prevSeq, seq);
        }
    }
}

} // namespace

za::U32 clientAck()
{
    return clientState.wantsWhole ? 0u : clientState.last;
}

void clientWantWhole()
{
    clientState.wantsWhole = clientState.last != 0;
}

void parse()
{
    za::U32 seq, dist, len;
    if(!netmove::readVarint(seq) || !netmove::readVarint(dist) || !netmove::readVarint(len) ||
        len > static_cast<za::U32>(net_message.cursize - msg_readcount))
    {
        msg_badread = true; // (CL_ParseServerMessage: a bad server message)
        return;
    }
    ClientState& c = clientState;
    const int bodyStart = msg_readcount;
    bool ok = seq > c.last && dist < static_cast<za::U32>(ring);
    const Snapshot* base = nullptr;
    if(ok && dist != 0)
    {
        base = &c.snaps[(seq - dist) % ring];
        ok = seq > dist && base->seq == seq - dist;
    }
    Snapshot& out = c.snaps[seq % ring];
    ok = ok && decode(base, static_cast<int>(len), out);
    msg_readcount = bodyStart + static_cast<int>(len);
    msg_badread = false;
    if(!ok)
    {
        c.counts.undecodable++;
        Con_Printf("netsnap: snapshot %u undecodable (base %u back, newest read %u, base kept %u, len %u)\n", seq, dist, c.last,
            base ? base->seq : 0u, len);
        if(seq > c.last)
        {
            out.seq = 0;
        }
        // (the entities kept as the last read: none vanishes for a message)
        if(c.last != 0 && c.snaps[c.last % ring].seq == c.last)
        {
            apply(c.snaps[c.last % ring], 0, 0);
        }
        return;
    }
    out.seq = seq;
    const za::U32 prev = c.last;
    c.last = seq;
    c.wantsWhole = c.wantsWhole && dist != 0;
    c.counts.decoded++;
    c.counts.records += static_cast<int>(out.records.size());
    c.counts.bytes += static_cast<long long>(len);
    apply(out, prev, seq);
    interpArrive(cl.mtime[0]);
}

void clientClear()
{
    ClientState& c = clientState;
    c.last = 0;
    for(Snapshot& s : c.snaps)
    {
        s.clear();
    }
    interp.valid = false;
    for(History& h : interp.of)
    {
        h.n = 0;
        h.model = nullptr;
    }
}

ClientCounts clientCounts()
{
    return clientState.counts;
}

bool interpActive()
{
    return interp.valid && vr_net_interp.value != 0.f && clientState.last != 0 && cls.state == ca_connected &&
           !cls.demoplayback;
}

void interpFrame()
{
    Interp& ip = interp;
    if(!interpActive())
    {
        ip.lastReal = realtime;
        return;
    }
    const double dt = za::clamp(realtime - ip.lastReal, 0.0, 0.1);
    ip.lastReal = realtime;
    ip.est += dt;
    ip.peak = za::max(ip.peak - dt * peakDecay, tick);
    ip.delay = za::clamp(ip.peak + delayMargin, delayMin, delayMost);
    const double target = ip.est - ip.delay;
    const double err = target - ip.rt;
    if(za::abs(err) > snapAfter)
    {
        ip.rt = target;
    }
    else
    {
        ip.rt += dt * za::clamp(1.0 + err * clockGain, rateLeast, rateMost);
    }
    ip.holds += ip.rt > ip.newest ? 1 : 0;
}

bool interpEntity(int num, float origin[3], float angles[3])
{
    const Interp& ip = interp;
    if(!interpActive() || num == cl.viewentity || num <= 0 || num >= static_cast<int>(ip.of.size()))
    {
        return false;
    }
    const entity_t& ent = cl_entities[num];
    if(ent.model && ent.model->type == mod_brush)
    {
        return false; // (doors and lifts: in step with the player's own entity, NetQuake's lerp: he rides them)
    }
    const History& h = ip.of[static_cast<za::SizeT>(num)];
    if(h.n == 0 || h.model != ent.model)
    {
        return false;
    }
    const Sample* newer = nullptr;
    const Sample* older = nullptr;
    for(int k = 0; k < h.n; k++)
    {
        const Sample& s = h.s[(h.head - 1 - k + 2 * historyLen) % historyLen];
        if(s.t <= ip.rt)
        {
            older = &s;
            break;
        }
        newer = &s;
    }
    const Sample* only = !older ? newer : !newer ? older : nullptr; // (before the oldest, or past the newest: held)
    if(!only)
    {
        for(int j = 0; j < 3; j++)
        {
            if(za::abs(newer->origin[j] - older->origin[j]) > 100.f)
            {
                only = older; // (a teleport: no lerp across it; the newer when its time comes)
            }
        }
    }
    if(only)
    {
        memcpy(origin, only->origin, sizeof(only->origin));
        memcpy(angles, only->angles, sizeof(only->angles));
        return true;
    }
    const double span = newer->t - older->t;
    const float f = span > 0.0 ? static_cast<float>(za::clamp((ip.rt - older->t) / span, 0.0, 1.0)) : 1.f;
    for(int j = 0; j < 3; j++)
    {
        origin[j] = older->origin[j] + f * (newer->origin[j] - older->origin[j]);
    }
    const glm::vec3 a = mpavatar::slerpAngles(glm::vec3{older->angles[0], older->angles[1], older->angles[2]},
        glm::vec3{newer->angles[0], newer->angles[1], newer->angles[2]}, f);
    angles[0] = a.x;
    angles[1] = a.y;
    angles[2] = a.z;
    return true;
}

double interpTime()
{
    return interp.rt;
}

double interpDelayMs()
{
    return interp.delay * 1000.0;
}

int interpHolds()
{
    return interp.holds;
}

namespace
{

// vr_netsnap_selftest's scratch (off the stack).
struct SelftestScratch
{
    Snapshot base, cur, sent, read;
    byte data[4096];
};
SelftestScratch selftestScratch;

// A snapshot of `n` records (keys 0, 1, 3, 5...), record i `len` bytes from `seed`.
void fillSnapshot(Snapshot& s, int n, int len, int seed)
{
    s.clear();
    za::U8 buf[64]{};
    for(int i = 0; i < n; i++)
    {
        for(int k = 0; k < len && k < 64; k++)
        {
            buf[k] = static_cast<za::U8>((i * 31 + k * 7 + seed * (k == 3 ? 1 : 0)) & 0xFF);
        }
        s.add(i == 0 ? 0 : 2 * i - 1, buf, za::min(len, 64));
    }
    s.add(avatarKey + 2, buf, 40);
    s.sortByKey();
}

[[nodiscard]] bool sameSnapshot(const Snapshot& a, const Snapshot& b)
{
    if(a.records.size() != b.records.size())
    {
        return false;
    }
    for(za::SizeT i = 0; i < a.records.size(); i++)
    {
        const Record& x = a.records[i];
        const Record& y = b.records[i];
        if(x.key != y.key || x.len != y.len || memcmp(a.data(x), b.data(y), static_cast<size_t>(x.len)) != 0)
        {
            return false;
        }
    }
    return true;
}

// `cur` encoded against `base` (nullptr none) within `budget`, decoded back against it: the client's equals `sent`.
[[nodiscard]] bool roundTrip(const Snapshot* base, const Snapshot& cur, int budget, int& bytes)
{
    SelftestScratch& t = selftestScratch;
    sizebuf_t buf{};
    buf.data = t.data;
    buf.maxsize = sizeof(t.data);
    encode(base, cur, &buf, budget, t.sent);
    bytes = buf.cursize;
    net_message = buf;
    MSG_BeginReading();
    return decode(base, buf.cursize, t.read) && sameSnapshot(t.read, t.sent);
}

} // namespace

void selftest_f()
{
    SelftestScratch& t = selftestScratch;
    testkit::Tally tally;
    const auto check = [&](bool ok, const char* what) {
        tally.check(ok);
        Con_Printf("netsnap selftest: %s %s\n", ok ? "PASS" : "FAIL", what);
    };
    const sizebuf_t savedMsg = net_message;
    const int savedCount = msg_readcount;
    const qboolean savedBad = msg_badread;
    int fullBytes = 0, sameBytes = 0, deltaBytes = 0, cutBytes = 0;
    fillSnapshot(t.base, 40, 16, 0);
    const bool whole = roundTrip(nullptr, t.base, 4096, fullBytes) && sameSnapshot(t.sent, t.base);
    check(whole, va("a snapshot without a base: whole, read back the same (%d records, %d bytes)", 41, fullBytes));
    const bool same = roundTrip(&t.base, t.base, 4096, sameBytes) && sameSnapshot(t.sent, t.base);
    check(same && sameBytes == 0, va("the same again against it: read back the same in %d bytes", sameBytes));
    fillSnapshot(t.cur, 40, 16, 0);
    {
        // two records changed by a byte, one gone (key 5), one new (key 200), one longer (key 7)
        Snapshot& c = t.cur;
        c.bytes[c.records[1].at + 3] ^= 0x55;
        c.bytes[c.records[20].at + 9] ^= 0x01;
        Snapshot rebuilt;
        za::U8 buf[64]{};
        for(const Record& r : c.records)
        {
            if(r.key == 5)
            {
                continue;
            }
            rebuilt.add(r.key, r.key == 7 ? buf : c.data(r), r.key == 7 ? 20 : r.len);
        }
        rebuilt.add(200, buf, 12);
        rebuilt.sortByKey();
        c = rebuilt;
    }
    const bool changed = roundTrip(&t.base, t.cur, 4096, deltaBytes) && sameSnapshot(t.sent, t.cur);
    check(changed, va("changed, gone, new and resized records: read back the same in %d bytes (whole: %d)", deltaBytes,
                       fullBytes));
    check(deltaBytes > 0 && deltaBytes < 50, va("two one-byte changes, one gone, two whole (32 B): %d bytes", deltaBytes));
    {
        // the predict record (the last key) the only one changed: its step from none is the largest (B8b's undecodable)
        int lastBytes = 0;
        za::U8 buf[16]{};
        t.base.add(predictKey, buf, 16);
        t.base.sortByKey();
        t.cur = t.base;
        t.cur.bytes[t.cur.records[t.cur.records.size() - 1].at + 5] ^= 0x10;
        const bool lastOk = roundTrip(&t.base, t.cur, 4096, lastBytes) && sameSnapshot(t.sent, t.cur);
        check(lastOk, va("the predict record alone changed: read back the same in %d bytes", lastBytes));
    }
    const bool cutOk = roundTrip(nullptr, t.cur, 200, cutBytes);
    check(cutOk && cutBytes <= 200 && t.sent.records.size() < t.cur.records.size(),
        va("past a budget of 200 bytes: cut there (%d bytes, %d of %d records), the client's as the server keeps it", cutBytes,
            static_cast<int>(t.sent.records.size()), static_cast<int>(t.cur.records.size())));
    net_message = savedMsg;
    msg_readcount = savedCount;
    msg_badread = savedBad;
    tally.verdict("netsnap selftest");
}

void init()
{
    Cmd_AddCommand("vr_netsnap_selftest", selftest_f);
}

} // namespace qvr::netsnap

extern "C" void VR_InterpFrame()
{
    qvr::netsnap::interpFrame();
}

extern "C" int VR_InterpEntity(int num, float* origin, float* angles)
{
    return qvr::netsnap::interpEntity(num, origin, angles) ? 1 : 0;
}
