#include "vr_alloccount.h"
// vr_voicerelay.cpp -- voice chat's server side (vr_voicechat.hpp; MP_PLAN.md, B10v): each speaker's frames kept as they
// come up (QVR_CLC_VOICE), passed on to every other client in his datagram (QVR_SVC_VOICE) under his voice budget.

#include "vr_voicechat.hpp"
#include "vr_clientprefs.hpp"
#include "vr_cvars.hpp"
#include "vr_protocol.hpp"
#include "vr_api.h"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include <stdlib.h>

namespace qvr::voicechat
{
namespace
{

constexpr int relayRing = 16;          // a speaker's frames kept (320 ms)
constexpr double maxAge = 0.3;         // s: an older frame not yet sent to a client is dropped for him
constexpr double voiceBurst = 0.1;     // s of his voice budget the bucket holds (at least one message of frames)
constexpr int inFramesPerSecond = 60;  // a speaker's frames taken a second at most (50 sent; a flood dropped)
constexpr int netShare = 2;            // his voice budget at most his vr_net_rate / this

struct RelayFrame
{
    Frame frame;
    double at{0.0}; // (realtime it was taken)
};

struct RelaySlot
{
    RelayFrame ring[relayRing];
    long long written = 0;            // frames taken from him (ring index = written % relayRing)
    double inTokens = 0.0, inFilledAt = -1.0;
    bool haveLast = false;
    za::U16 lastSeq = 0;              // the newest frame taken (its copies, and older ones, not again)
    // as a listener: the next of each speaker's frames to send him, his budget
    long long next[MAX_SCOREBOARD]{};
    long long againFrom[MAX_SCOREBOARD]{}, againTo[MAX_SCOREBOARD]{}; // the last datagram's, once more
    double tokens = 0.0, filledAt = -1.0;
    int firstSpeaker = 0;             // the slot whose new frames go first in his next datagram (it rotates)
    unsigned muted = 0;               // the speakers he muted, a bit a slot (his vr_voicechat_listen): none sent him
    bool voiceOff = false;            // his voice chat off (the same): nothing sent him
    double notedAt[MAX_SCOREBOARD]{}; // when he was last told a speaker he muted talks
    // counts
    long long in = 0, inDropped = 0, inCopies = 0, out = 0, outBytes = 0, outDropped = 0, outAgain = 0, outMuted = 0;
};
static_assert(MAX_SCOREBOARD <= 32, "a listener's muted speakers are a bit a slot in an unsigned");
constexpr unsigned allSlots = MAX_SCOREBOARD == 32 ? ~0u : (1u << (MAX_SCOREBOARD % 32)) - 1u;

struct Relay
{
    RelaySlot slot[MAX_SCOREBOARD];
};
Relay relay;

constexpr int messageHeaderBytes = 4; // [svc_quakevr][QVR_SVC_VOICE][byte slot][byte count]
constexpr double mutedNoteEvery = 0.1; // s: a listener told at most this often that a speaker he muted talks

[[nodiscard]] bool relaying()
{
    return (sv.protocolflags & PRFL_QUAKEVR) != 0 && vr_net_voice_bytes.value > 0.f && svs.maxclients > 1;
}

// A speaker's frame `in` taken at `now` (his flood guard refilled for the message first: refillIn): his copies and
// older frames not again, a frame over his frames a second dropped.
void refillIn(RelaySlot& r, double now)
{
    // (his own flood guard: frames a second, a bucket)
    const double most = inFramesPerSecond * 0.2;
    r.inTokens = r.inFilledAt < 0.0 || now < r.inFilledAt ? most : za::min(most, r.inTokens + inFramesPerSecond * (now - r.inFilledAt));
    r.inFilledAt = now;
}

void take(RelaySlot& r, const Frame& in, double now)
{
    if(r.haveLast && seqDiff(in.seq, r.lastSeq) <= 0)
    {
        r.inCopies++; // (the copy his next datagram carries, or one overtaken)
        return;
    }
    if(r.inTokens < 1.0)
    {
        r.inDropped++;
        return;
    }
    r.inTokens -= 1.0;
    r.haveLast = true;
    r.lastSeq = in.seq;
    RelayFrame& f = r.ring[r.written % relayRing];
    f.frame = in;
    f.at = now;
    r.written++;
    r.in++;
}

// Listener `to`'s voice in `msg`: the other speakers in `present` (a bit a slot, of the first `slots`), under his budget
// (`rate` B/s, a bucket; a quarter of the datagram). Two passes: every speaker's new frames first, starting at a slot
// that rotates a datagram (to the next speaker the budget cut short), so two or three talking at once share the budget;
// then, with what remains, the last datagram's frames again (NetQuake drops a datagram that comes after a newer one, so
// a frame in two survives most reordering and a loss). The speakers he muted, or all with his voice chat off, are not sent him: a speaker he muted
// who talks is only noted (a message of no frames, at most every mutedNoteEvery), for his Voice Chat page.
// [svc_quakevr][QVR_SVC_VOICE][byte slot][byte count] x ([short seq][byte flags][byte len][len bytes]) a speaker.
void relayTo(Relay& relay, sizebuf_t* msg, int to, unsigned present, int slots, double rate, double now)
{
    RelaySlot& l = relay.slot[to];
    slots = za::min(slots, MAX_SCOREBOARD);
    const double most = za::max(rate * voiceBurst, static_cast<double>(messageHeaderBytes + 2 * (frameHeaderBytes + maxFrameBytes)));
    l.tokens = l.filledAt < 0.0 || now < l.filledAt ? most : za::min(most, l.tokens + rate * (now - l.filledAt));
    l.filledAt = now;
    // (a quarter of the datagram at most)
    const double room = za::min(static_cast<double>(za::min(msg->maxsize - msg->cursize, msg->maxsize / 4)), l.tokens);
    double used = 0.0;
    int fresh[MAX_SCOREBOARD]{}, again[MAX_SCOREBOARD]{};
    bool note[MAX_SCOREBOARD]{}, sends[MAX_SCOREBOARD]{};
    for(int s = 0; s < slots; s++)
    {
        RelaySlot& sp = relay.slot[s];
        long long& next = l.next[s];
        long long& from = l.againFrom[s];
        long long& until = l.againTo[s];
        if(s == to || !(present & (1u << s)) || l.voiceOff)
        {
            next = from = until = sp.written;
            continue;
        }
        if(l.muted & (1u << s))
        {
            // (muted by him: skipped; noted while he talks)
            note[s] = next < sp.written && now - l.notedAt[s] >= mutedNoteEvery;
            l.outMuted += sp.written - next;
            next = from = until = sp.written;
            continue;
        }
        // (frames gone round the ring, or too old: dropped for him)
        if(next < sp.written - relayRing)
        {
            l.outDropped += sp.written - relayRing - next;
            next = sp.written - relayRing;
        }
        while(next < sp.written && now - sp.ring[next % relayRing].at > maxAge)
        {
            l.outDropped++;
            next++;
        }
        from = za::max(from, za::max(sp.written - relayRing, 0LL));
        while(from < until && now - sp.ring[from % relayRing].at > maxAge)
        {
            from++;
        }
        sends[s] = true;
    }
    // (the speakers in this datagram's order, from the rotating first)
    int order[MAX_SCOREBOARD];
    int count = 0;
    for(int i = 0; i < slots; i++)
    {
        const int s = (l.firstSpeaker + i) % za::max(slots, 1);
        if(sends[s] || note[s])
        {
            order[count++] = s;
        }
    }
    const auto fits = [&](int add) { return used + add <= room; };
    // (1: the notes and every speaker's new frames)
    for(int i = 0; i < count; i++)
    {
        const int s = order[i];
        const RelaySlot& sp = relay.slot[s];
        if(note[s])
        {
            note[s] = fits(messageHeaderBytes);
            used += note[s] ? messageHeaderBytes : 0;
            continue;
        }
        for(long long k = l.next[s]; k < sp.written && fresh[s] < maxFramesPerMessage; k++)
        {
            const int add = frameHeaderBytes + sp.ring[k % relayRing].frame.len + (fresh[s] == 0 ? messageHeaderBytes : 0);
            if(!fits(add))
            {
                break;
            }
            used += add;
            fresh[s]++;
        }
    }
    // (the next datagram's first: after this one's first with new frames, the next the budget cut short, else simply the
    // next slot)
    int first = 0;
    const auto waiting = [&](int s) { return sends[s] ? relay.slot[s].written - l.next[s] : 0LL; };
    while(first < count && waiting(order[first]) == 0)
    {
        first++;
    }
    if(first < count)
    {
        l.firstSpeaker = (order[first] + 1) % slots;
        for(int j = 1; j < count; j++)
        {
            const int s = order[(first + j) % count];
            if(fresh[s] < za::min(waiting(s), static_cast<long long>(maxFramesPerMessage)))
            {
                l.firstSpeaker = s;
                break;
            }
        }
    }
    // (2: the last datagram's frames again, with what remains)
    for(int i = 0; i < count; i++)
    {
        const int s = order[i];
        if(!sends[s])
        {
            continue;
        }
        const RelaySlot& sp = relay.slot[s];
        for(long long k = l.againFrom[s]; k < l.againTo[s] && fresh[s] + again[s] < maxFramesPerMessage; k++)
        {
            const int add = frameHeaderBytes + sp.ring[k % relayRing].frame.len + (fresh[s] + again[s] == 0 ? messageHeaderBytes : 0);
            if(!fits(add))
            {
                break;
            }
            used += add;
            again[s]++;
        }
    }
    for(int i = 0; i < count; i++)
    {
        const int s = order[i];
        const RelaySlot& sp = relay.slot[s];
        if(note[s])
        {
            MSG_WriteByte(msg, protocol::svc_quakevr);
            MSG_WriteByte(msg, protocol::QVR_SVC_VOICE);
            MSG_WriteByte(msg, s);
            MSG_WriteByte(msg, 0);
            l.notedAt[s] = now;
            l.outBytes += messageHeaderBytes;
            continue;
        }
        if(fresh[s] + again[s] == 0)
        {
            continue; // (his copies wait for the next datagram)
        }
        long long& next = l.next[s];
        long long& from = l.againFrom[s];
        long long& until = l.againTo[s];
        MSG_WriteByte(msg, protocol::svc_quakevr);
        MSG_WriteByte(msg, protocol::QVR_SVC_VOICE);
        MSG_WriteByte(msg, s);
        MSG_WriteByte(msg, fresh[s] + again[s]);
        int bytes = messageHeaderBytes;
        for(int k = 0; k < again[s]; k++)
        {
            writeFrame(msg, sp.ring[(from + k) % relayRing].frame);
            bytes += frameHeaderBytes + sp.ring[(from + k) % relayRing].frame.len;
        }
        for(int k = 0; k < fresh[s]; k++)
        {
            writeFrame(msg, sp.ring[(next + k) % relayRing].frame);
            bytes += frameHeaderBytes + sp.ring[(next + k) % relayRing].frame.len;
        }
        from = next;
        until = next + fresh[s];
        next += fresh[s];
        l.out += fresh[s];
        l.outAgain += again[s];
        l.outBytes += bytes;
    }
    l.tokens -= used;
}

// "vr_voicechat_listen <muted> <off>" (a client's, as his mutes or his voice chat change; voicechat.cpp tellListen):
// the speakers he muted, a bit a slot, and whether his voice chat is off. Kept for his slot (reset with it).
void listen_f()
{
    if(cmd_source != src_client || !sv.active || !host_client || Cmd_Argc() != 3)
    {
        return;
    }
    const int slot = static_cast<int>(host_client - svs.clients);
    if(slot < 0 || slot >= MAX_SCOREBOARD)
    {
        return;
    }
    relay.slot[slot].muted = static_cast<unsigned>(strtoul(Cmd_Argv(1), nullptr, 10)) & allSlots;
    relay.slot[slot].voiceOff = Q_atoi(Cmd_Argv(2)) != 0;
}

} // namespace

bool serverRead(client_s* client)
{
    // [byte count] x ([short seq][byte flags][byte len][len bytes])
    const int slot = static_cast<int>(client - svs.clients);
    const int count = MSG_ReadByte();
    if(count < 0 || count > maxFramesPerMessage)
    {
        return false;
    }
    RelaySlot* r = slot >= 0 && slot < MAX_SCOREBOARD ? &relay.slot[slot] : nullptr;
    const bool keep = r && relaying() && client->spawned;
    const double now = realtime; // (not the game's clock: it stands still while the server is paused)
    if(keep)
    {
        refillIn(*r, now);
    }
    for(int i = 0; i < count; i++)
    {
        Frame in;
        if(!readFrame(in))
        {
            return false;
        }
        if(keep)
        {
            take(*r, in, now);
        }
    }
    return true;
}

void serverWrite(sizebuf_t* msg, client_s* client)
{
    const int to = static_cast<int>(client - svs.clients);
    if(!relaying() || to < 0 || to >= MAX_SCOREBOARD || !client->spawned)
    {
        return;
    }
    const double now = realtime; // (as serverRead's)
    // (his voice budget: vr_net_voice_bytes, at most a share of his vr_net_rate; a bucket)
    double rate = vr_net_voice_bytes.value;
    const int netRate = client->edict ? static_cast<int>(clientprefs::value(clientprefs::indexOf(&vr_net_rate), &vr_net_rate, client->edict)) : 0;
    if(netRate > 0)
    {
        rate = za::min(rate, static_cast<double>(netRate) / netShare);
    }
    const int slots = za::min(svs.maxclients, MAX_SCOREBOARD);
    unsigned present = 0;
    for(int s = 0; s < slots; s++)
    {
        present |= svs.clients[s].active ? 1u << s : 0u;
    }
    relayTo(relay, msg, to, present, slots, rate, now);
}

void serverReset(int slot)
{
    if(slot < 0 || slot >= MAX_SCOREBOARD)
    {
        return;
    }
    RelaySlot& r = relay.slot[slot];
    const long long keep = r.written; // (the others' indices into his ring stay valid)
    r = RelaySlot{};
    r.written = keep;
    for(int s = 0; s < MAX_SCOREBOARD; s++)
    {
        r.next[s] = r.againFrom[s] = r.againTo[s] = relay.slot[s].written;
        relay.slot[s].muted &= ~(1u << slot); // (a new player in the slot: nobody's mute of the last one his)
        relay.slot[s].notedAt[slot] = 0.0;
    }
}

void serverReport()
{
    for(int s = 0; s < svs.maxclients && s < MAX_SCOREBOARD; s++)
    {
        const RelaySlot& r = relay.slot[s];
        if(!svs.clients[s].active)
        {
            continue;
        }
        Con_Printf("mp_voice_relay: slot=%d in=%lld in_dropped=%lld in_copies=%lld out=%lld out_again=%lld out_bytes=%lld "
                   "out_dropped=%lld out_muted=%lld muted=%u voice_off=%d\n", s, r.in, r.inDropped, r.inCopies, r.out, r.outAgain,
            r.outBytes, r.outDropped, r.outMuted, r.muted, r.voiceOff ? 1 : 0);
    }
    Con_Printf("mp_voice_relay: done\n");
}

namespace
{

// The relay self-test's run: speakers in slots 1..talkers (60-byte frames, 50 a second, each taken as it comes) and the
// listener in slot 0 at `rate` B/s, 72 datagrams a second of `datagram` bytes, for `seconds`; what he got.
struct RelayRun
{
    long long made[MAX_SCOREBOARD]{};   // frames each speaker sent
    long long fresh[MAX_SCOREBOARD]{};  // ... the listener got, the first time
    long long copies[MAX_SCOREBOARD]{}; // ... once more
    long long notes[MAX_SCOREBOARD]{};  // messages of no frames (a muted speaker talking)
    long long bytes = 0;                // the listener's voice bytes
    int firstServed[8]{};               // the speaker of each of the first datagrams' first new frame (-1 none)
    bool seen[MAX_SCOREBOARD][1024]{};  // the sequence numbers got
};

void relayRun(RelayRun& run, int talkers, double rate, double seconds, int datagram, unsigned muted, bool off)
{
    za::UniquePtr<Relay> relay = za::makeUnique<Relay>();
    relay->slot[0].muted = muted;
    relay->slot[0].voiceOff = off;
    const unsigned present = (2u << talkers) - 1u; // (slots 0..talkers; 8 slots: empty ones in the rotation)
    za::U8 bytes[1400];
    sizebuf_t buf{};
    buf.data = bytes;
    buf.maxsize = za::min(datagram, static_cast<int>(sizeof(bytes)));
    const double t0 = 1000.0;
    double nextFrame = t0;
    for(int d = 0; d * (1.0 / 72.0) < seconds; d++)
    {
        const double now = t0 + d * (1.0 / 72.0);
        for(; nextFrame <= now; nextFrame += 0.02) // (a frame is 20 ms)
        {
            for(int s = 1; s <= talkers; s++)
            {
                Frame f;
                f.seq = static_cast<za::U16>(run.made[s]++);
                f.len = 60;
                f.data[0] = static_cast<za::U8>(s);
                refillIn(relay->slot[s], nextFrame);
                take(relay->slot[s], f, nextFrame);
            }
        }
        buf.cursize = 0;
        relayTo(*relay, &buf, 0, present, 8, rate, now);
        run.bytes += buf.cursize;
        int first = -1;
        WireRead read{buf};
        while(msg_readcount < buf.cursize && !msg_badread)
        {
            MSG_ReadByte(); // (svc_quakevr, QVR_SVC_VOICE)
            MSG_ReadByte();
            const int slot = MSG_ReadByte();
            const int count = MSG_ReadByte();
            if(slot < 0 || slot >= MAX_SCOREBOARD)
            {
                break;
            }
            run.notes[slot] += count == 0 ? 1 : 0;
            for(int i = 0; i < count; i++)
            {
                Frame f;
                if(readFrame(f) && f.seq < 1024)
                {
                    (run.seen[slot][f.seq] ? run.copies : run.fresh)[slot]++;
                    first = first < 0 && !run.seen[slot][f.seq] ? slot : first;
                    run.seen[slot][f.seq] = true;
                }
            }
        }
        read.restore();
        if(d < 8)
        {
            run.firstServed[d] = first;
        }
    }
}

} // namespace

void serverSelftest(za::FunctionRef<void(bool ok, const char* what)> check)
{
    const double seconds = 3.0, budget = 8000.0;
    const double most = budget * seconds + budget * voiceBurst + 1400; // (the bucket's burst, a datagram's edge)
    {
        // two talking at once under the default budget: both get every new frame, the copies only with what remains
        za::UniquePtr<RelayRun> r = za::makeUnique<RelayRun>();
        relayRun(*r, 2, budget, seconds, 1400, 0, false);
        check(r->fresh[1] >= r->made[1] - 2 && r->fresh[2] >= r->made[2] - 2 && r->copies[1] + r->copies[2] > 0 && r->bytes <= most,
            va("the relay, two talking under %.0f B/s: new frames %lld and %lld of %lld each, copies %lld and %lld, %lld bytes "
               "(before: the higher slot got a third)", budget, r->fresh[1], r->fresh[2], r->made[1], r->copies[1], r->copies[2],
                r->bytes));
    }
    {
        // three: the budget shared, nobody silent; no copies while new frames wait (10 s: the bucket's first burst, the
        // first speaker's, a small part)
        za::UniquePtr<RelayRun> r = za::makeUnique<RelayRun>();
        const double longer = 10.0;
        relayRun(*r, 3, budget, longer, 1400, 0, false);
        double lo = 1.0, hi = 0.0;
        long long fresh = 0, copies = 0;
        for(int s = 1; s <= 3; s++)
        {
            const double share = static_cast<double>(r->fresh[s]) / za::max(1LL, r->made[s]);
            lo = za::min(lo, share);
            hi = za::max(hi, share);
            fresh += r->fresh[s];
            copies += r->copies[s];
        }
        check(lo >= 0.6 && hi - lo <= 0.05 && copies * 20 <= fresh && r->bytes <= budget * longer + budget * voiceBurst + 1400,
            va("the relay, three talking under %.0f B/s: each got %.2f..%.2f of his new frames, %lld copies to %lld new, %lld "
               "bytes (before: the third silent)", budget, lo, hi, copies, fresh, r->bytes));
    }
    {
        // the first speaker served rotates a datagram (a datagram with room for one frame: the two take turns)
        za::UniquePtr<RelayRun> r = za::makeUnique<RelayRun>();
        relayRun(*r, 2, 100000.0, 0.1, 280, 0, false);
        bool turns = true;
        for(int d = 1; d < 8; d++)
        {
            turns = turns && r->firstServed[d] > 0 && r->firstServed[d] != r->firstServed[d - 1];
        }
        check(turns, va("the relay's first speaker rotates a datagram: %d %d %d %d %d %d %d %d", r->firstServed[0], r->firstServed[1],
                           r->firstServed[2], r->firstServed[3], r->firstServed[4], r->firstServed[5], r->firstServed[6],
                           r->firstServed[7]));
    }
    {
        // a listener who muted slot 2: none of his frames, only a note while he talks (at most 10 a second); slot 1 whole
        za::UniquePtr<RelayRun> r = za::makeUnique<RelayRun>();
        relayRun(*r, 2, budget, seconds, 1400, 1u << 2, false);
        check(r->fresh[2] == 0 && r->copies[2] == 0 && r->notes[2] >= 1 && r->notes[2] <= static_cast<long long>(seconds / mutedNoteEvery) + 1 &&
                  r->fresh[1] >= r->made[1] - 2,
            va("the relay skips a speaker the listener muted: %lld of his frames sent, %lld notes; the other's %lld of %lld",
                r->fresh[2] + r->copies[2], r->notes[2], r->fresh[1], r->made[1]));
    }
    {
        // his voice chat off: nothing at all
        za::UniquePtr<RelayRun> r = za::makeUnique<RelayRun>();
        relayRun(*r, 2, budget, seconds, 1400, 0, true);
        check(r->bytes == 0, va("the relay sends nothing to a listener whose voice chat is off: %lld bytes", r->bytes));
    }
}

void serverInit()
{
    Cmd_AddCommand("vr_voicechat_relay", serverReport);
    Cmd_AddCommand_ClientCommand("vr_voicechat_listen", listen_f);
}

} // namespace qvr::voicechat
