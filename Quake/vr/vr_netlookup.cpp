#include "vr_alloccount.h"
// vr_netlookup.cpp -- host names looked up on a worker for the datagram layer (net_dgrm.c: the room service's name, the
// server browser's favourites and recent servers), never on the game thread (review 2026-11-18, finding 6: with no
// working DNS -- a LAN party, a captive portal, a headset on a hotspot -- each gethostbyname held the host's frame for
// the resolver's timeout, several seconds, every minute, and a dead favourite's name at every browser refresh).
//
// VR_NetLookup(name) answers from what it has and starts what it lacks: getaddrinfo (IPv4) as a jobs::async task, its
// result taken by a later call on the main thread. A failed name waits before its next try, the wait doubling from
// firstWait to mostWait; a found one is kept keepFor, then looked up again while its old address is still given out. The
// caller says whether a lookup may start now (net_dgrm.c: not while remote clients are connected).
//
// Test knobs (the multiplayer harness, mp_test.py lookup): -netlookupdelay <s> each lookup waits that long on its worker
// first (a slow resolver); -netlookupfail each lookup fails. net_lookups lists the names, their state, tries and waits.

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "vr_engine.hpp"
#include "vr_jobs.hpp"
#include "vr_api.h"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Chrono/Time.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/MinMax.hpp"


namespace qvr::netlookup
{
namespace
{

constexpr int maxNames = 32;       // names kept (the browser's 24 at most, the room service's, a few changed since)
constexpr int nameLength = 64;     // (a longer name is not looked up)
constexpr double firstWait = 2.0;  // s after a first failure before the next try
constexpr double mostWait = 300.0; // s between tries at most
constexpr double keepFor = 600.0;  // s a found address is given out before it is looked up again

struct Found
{
    bool ok{false};
    za::U32 ipv4{0}; // (network order)
};

struct Entry
{
    char name[nameLength]{};
    jobs::Future<Found> lookup; // (valid: under way)
    bool found{false};
    za::U32 ipv4{0};
    double foundAt{0.0};
    double nextTry{0.0}; // (0: now)
    double wait{0.0};    // the next failure's wait
    int tries{0};
    int failures{0};
    double usedAt{0.0}; // (the slot taken by a new name: the least recently used)
};

// The names (the main thread's: net_dgrm.c's callers); the test knobs, read once.
struct Lookups
{
    za::Array<Entry, maxNames> entries;
    bool knobsRead{false};
    double testDelay{0.0};
    bool testFail{false};
};
Lookups lookups;

void readKnobs()
{
    if(lookups.knobsRead)
    {
        return;
    }
    lookups.knobsRead = true;
    const int delay = COM_CheckParm("-netlookupdelay");
    if(delay && delay + 1 < com_argc)
    {
        lookups.testDelay = za::max(0.0, static_cast<double>(Q_atof(com_argv[delay + 1])));
    }
    lookups.testFail = COM_CheckParm("-netlookupfail") != 0;
}

// getaddrinfo on the worker (no engine state touched: the name copied in, the address returned)
Found resolve(const za::Array<char, nameLength>& name, double delay, bool fail) noexcept
{
    if(delay > 0.0)
    {
        za::ThisThread::sleepFor(za::milliseconds(static_cast<za::I64>(delay * 1000.0)));
    }
    Found out;
    if(fail)
    {
        return out;
    }
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* list = nullptr;
    if(getaddrinfo(name.data(), nullptr, &hints, &list) != 0 || !list)
    {
        return out;
    }
    for(const addrinfo* a = list; a; a = a->ai_next)
    {
        if(a->ai_family == AF_INET && a->ai_addr)
        {
            out.ipv4 = reinterpret_cast<const sockaddr_in*>(a->ai_addr)->sin_addr.s_addr;
            out.ok = true;
            break;
        }
    }
    freeaddrinfo(list);
    return out;
}

// A finished lookup's result taken
void take(Entry& e, double now)
{
    if(!e.lookup.valid() || !e.lookup.ready())
    {
        return;
    }
    const Found f = e.lookup.get();
    if(f.ok)
    {
        e.found = true;
        e.ipv4 = f.ipv4;
        e.foundAt = now;
        e.wait = 0.0;
        e.failures = 0;
        return;
    }
    e.failures++;
    e.wait = e.wait <= 0.0 ? firstWait : za::min(mostWait, e.wait * 2.0);
    e.nextTry = now + e.wait;
}

void start(Entry& e, double now)
{
    za::Array<char, nameLength> name{};
    q_strlcpy(name.data(), e.name, nameLength);
    e.tries++;
    e.nextTry = now; // (a failure sets the next)
    e.lookup = jobs::async([name, delay = lookups.testDelay, fail = lookups.testFail]() noexcept
                           { return resolve(name, delay, fail); });
}

// The name's entry; a new name takes an empty slot, else the least recently asked one with no lookup under way
Entry& slotFor(const char* name, double now)
{
    Entry* pick = nullptr;
    for(Entry& e : lookups.entries)
    {
        if(e.name[0] && !ZA_STRCMP(e.name, name))
        {
            return e;
        }
        if(!pick || (!e.name[0] && pick->name[0]) ||
           (e.name[0] && pick->name[0] && (pick->lookup.valid() || (!e.lookup.valid() && e.usedAt < pick->usedAt))))
        {
            pick = &e;
        }
    }
    Entry& e = *pick;
    e = Entry{}; // (every slot's lookup under way, 32 names at once: this waits for one; never in practice)
    q_strlcpy(e.name, name, nameLength);
    e.usedAt = now;
    return e;
}

void lookups_f()
{
    const double now = Sys_DoubleTime();
    int n = 0;
    for(Entry& e : lookups.entries)
    {
        if(!e.name[0])
        {
            continue;
        }
        take(e, now);
        const unsigned char* ip = reinterpret_cast<const unsigned char*>(&e.ipv4);
        Con_Printf("net_lookup: name=%s state=%s ip=%d.%d.%d.%d tries=%d failures=%d wait=%.1f next=%.1f\n", e.name,
                   e.lookup.valid() ? "looking" : e.found ? "found" : e.failures ? "failed" : "idle", ip[0], ip[1], ip[2],
                   ip[3], e.tries, e.failures, e.wait, e.lookup.valid() ? 0.0 : za::max(0.0, e.nextTry - now));
        n++;
    }
    Con_Printf("net_lookups: %d names\n", n);
}

} // namespace
} // namespace qvr::netlookup

using namespace qvr;
using namespace qvr::netlookup;

extern "C" int VR_NetLookup(const char* name, unsigned int* netorder_ipv4, int mayStart)
{
    if(!name || !name[0] || ZA_STRLEN(name) >= static_cast<size_t>(nameLength))
    {
        return -1;
    }
    readKnobs();
    const double now = Sys_DoubleTime();
    Entry& e = slotFor(name, now);
    e.usedAt = now;
    take(e, now);
    const bool stale = e.found && now - e.foundAt > keepFor;
    if(!e.lookup.valid() && mayStart && (!e.found || stale) && now >= e.nextTry)
    {
        start(e, now);
    }
    if(e.found)
    {
        *netorder_ipv4 = e.ipv4;
        return 1;
    }
    return e.lookup.valid() || (e.failures == 0 && !mayStart) ? 0 : -1;
}

extern "C" void VR_NetLookupInit(void)
{
    Cmd_AddCommand("net_lookups", lookups_f);
}

extern "C" void VR_NetLookupShutdown(void)
{
    for(Entry& e : lookups.entries)
    {
        e = Entry{}; // (a lookup under way waited for: its Future's)
    }
}
