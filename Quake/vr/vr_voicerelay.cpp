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
    // counts
    long long in = 0, inDropped = 0, inCopies = 0, out = 0, outBytes = 0, outDropped = 0, outAgain = 0;
};
RelaySlot relay[MAX_SCOREBOARD];

[[nodiscard]] bool relaying()
{
    return (sv.protocolflags & PRFL_QUAKEVR) != 0 && vr_net_voice_bytes.value > 0.f && svs.maxclients > 1;
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
    RelaySlot* r = slot >= 0 && slot < MAX_SCOREBOARD ? &relay[slot] : nullptr;
    const bool take = r && relaying() && client->spawned;
    const double now = realtime; // (not the game's clock: it stands still while the server is paused)
    if(take)
    {
        // (his own flood guard: frames a second, a bucket)
        const double most = inFramesPerSecond * 0.2;
        r->inTokens = r->inFilledAt < 0.0 || now < r->inFilledAt ? most
                                                                 : za::min(most, r->inTokens + inFramesPerSecond * (now - r->inFilledAt));
        r->inFilledAt = now;
    }
    for(int i = 0; i < count; i++)
    {
        Frame in;
        if(!readFrame(in))
        {
            return false;
        }
        if(!take)
        {
            continue;
        }
        if(r->haveLast && seqDiff(in.seq, r->lastSeq) <= 0)
        {
            r->inCopies++; // (the copy his next datagram carries, or one overtaken)
            continue;
        }
        if(r->inTokens < 1.0)
        {
            r->inDropped++;
            continue;
        }
        r->inTokens -= 1.0;
        r->haveLast = true;
        r->lastSeq = in.seq;
        RelayFrame& f = r->ring[r->written % relayRing];
        f.frame = in;
        f.at = now;
        r->written++;
        r->in++;
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
    RelaySlot& l = relay[to];
    const double now = realtime; // (as serverRead's)
    // (his voice budget: vr_net_voice_bytes, at most a share of his vr_net_rate; a bucket)
    double rate = vr_net_voice_bytes.value;
    const int netRate = client->edict ? static_cast<int>(clientprefs::value(clientprefs::indexOf(&vr_net_rate), &vr_net_rate, client->edict)) : 0;
    if(netRate > 0)
    {
        rate = za::min(rate, static_cast<double>(netRate) / netShare);
    }
    const double most = za::max(rate * voiceBurst, static_cast<double>(4 + 2 * (frameHeaderBytes + maxFrameBytes)));
    l.tokens = l.filledAt < 0.0 || now < l.filledAt ? most : za::min(most, l.tokens + rate * (now - l.filledAt));
    l.filledAt = now;
    // (a quarter of the datagram at most)
    const int room = za::min(msg->maxsize - msg->cursize, msg->maxsize / 4);
    int used = 0;
    for(int s = 0; s < svs.maxclients && s < MAX_SCOREBOARD; s++)
    {
        RelaySlot& sp = relay[s];
        long long& next = l.next[s];
        if(s == to || !svs.clients[s].active)
        {
            next = l.againFrom[s] = l.againTo[s] = sp.written;
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
        // [svc_quakevr][QVR_SVC_VOICE][byte slot][byte count] x ([short seq][byte flags][byte len][len bytes]): the new
        // frames first, then (budget left) the last datagram's again: NetQuake drops a datagram that comes after a newer
        // one, so a frame in two survives most reordering and a loss
        long long& from = l.againFrom[s];
        long long& until = l.againTo[s];
        from = za::max(from, za::max(sp.written - relayRing, 0LL));
        while(from < until && now - sp.ring[from % relayRing].at > maxAge)
        {
            from++;
        }
        int fresh = 0, again = 0, bytes = 4;
        for(long long k = next; k < sp.written && fresh < maxFramesPerMessage; k++, fresh++)
        {
            const int add = frameHeaderBytes + sp.ring[k % relayRing].frame.len;
            if(used + bytes + add > room || bytes + add > l.tokens)
            {
                break;
            }
            bytes += add;
        }
        for(long long k = from; k < until && fresh + again < maxFramesPerMessage; k++, again++)
        {
            const int add = frameHeaderBytes + sp.ring[k % relayRing].frame.len;
            if(used + bytes + add > room || bytes + add > l.tokens)
            {
                break;
            }
            bytes += add;
        }
        if(fresh + again == 0)
        {
            continue;
        }
        MSG_WriteByte(msg, protocol::svc_quakevr);
        MSG_WriteByte(msg, protocol::QVR_SVC_VOICE);
        MSG_WriteByte(msg, s);
        MSG_WriteByte(msg, fresh + again);
        for(int i = 0; i < again; i++)
        {
            writeFrame(msg, sp.ring[(from + i) % relayRing].frame);
        }
        for(int i = 0; i < fresh; i++)
        {
            writeFrame(msg, sp.ring[(next + i) % relayRing].frame);
        }
        from = next;
        until = next + fresh;
        next += fresh;
        used += bytes;
        l.tokens -= bytes;
        l.out += fresh;
        l.outAgain += again;
        l.outBytes += bytes;
    }
}

void serverReset(int slot)
{
    if(slot < 0 || slot >= MAX_SCOREBOARD)
    {
        return;
    }
    const long long keep = relay[slot].written; // (the others' indices into his ring stay valid)
    relay[slot] = RelaySlot{};
    relay[slot].written = keep;
    for(int s = 0; s < MAX_SCOREBOARD; s++)
    {
        relay[slot].next[s] = relay[slot].againFrom[s] = relay[slot].againTo[s] = relay[s].written;
    }
}

void serverReport()
{
    for(int s = 0; s < svs.maxclients && s < MAX_SCOREBOARD; s++)
    {
        const RelaySlot& r = relay[s];
        if(!svs.clients[s].active)
        {
            continue;
        }
        Con_Printf("mp_voice_relay: slot=%d in=%lld in_dropped=%lld in_copies=%lld out=%lld out_again=%lld out_bytes=%lld "
                   "out_dropped=%lld\n", s, r.in, r.inDropped, r.inCopies, r.out, r.outAgain, r.outBytes, r.outDropped);
    }
    Con_Printf("mp_voice_relay: done\n");
}

void serverInit()
{
    Cmd_AddCommand("vr_voicechat_relay", serverReport);
}

} // namespace qvr::voicechat
