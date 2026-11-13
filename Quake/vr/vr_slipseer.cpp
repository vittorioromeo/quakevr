// vr_slipseer.cpp -- Slipseer as the Map Library's second source (vr_slipseer.hpp says what and how). Three parts:
// the polite fetch (one request at a time, seconds apart, a browser's headers, a cache on disk revalidated rather than
// fetched again, robots.txt, a back-off that stops rather than retries); the HTML read into Items and Details (pure
// functions, tested on saved pages: maps_slipseer_test); and the thread that runs the listing pass and the picked map's
// page, whose entries go to mapindex::setSlipseer (one model with Quaddicted's, `source` Slipseer).
//
// The cache: <base>/cache/slipseer/<16 hex of the URL's hash>.<html|img> and a .meta beside each (its URL, when it
// was fetched, its ETag and Last-Modified). A listing page is revalidated after listingSeconds, a map page after
// detailSeconds, a screenshot never (a new one has a new URL). The back-off is kept in backoff.txt there, so that a
// restart does not knock again before it ends.

#include "vr_slipseer.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_mapindex.hpp"
#include "vr_mapinstall.hpp"
#include "vr_random.hpp"
#include "vr_sha256.hpp"
#include "vr_strings.hpp"
#include "vr_testkit.hpp"
#include "vr_tunables.hpp"
#include "vr_zancle.hpp"

#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/ToString.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

extern "C"
{
#include "json.h"
#include "q_ctype.h"
}

extern "C" unsigned char* Image_DecodeMemory(const unsigned char* bytes, int length, int* width, int* height); // image.c
extern "C" int Image_InfoMemory(const unsigned char* bytes, int length, int* width, int* height);            // image.c

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace qvr::slipseer
{
namespace
{

constexpr const char* siteRoot = "https://www.slipseer.com";
constexpr const char* quaddictedRoot = "https://www.quaddicted.com"; // (only its screenshots are fetched from it)
constexpr za::I64 firstPageSeconds = 2 * 24 * 60 * 60; // the listing's first page revalidated after two days
constexpr za::I64 listingSeconds = 10 * 24 * 60 * 60; // its other pages after ten
constexpr za::I64 detailSeconds = 30 * 24 * 60 * 60;  // a map's page after thirty
constexpr za::I64 robotsSeconds = 7 * 24 * 60 * 60;
constexpr za::I64 assetSeconds = 365ll * 24 * 60 * 60; // a screenshot: its URL changes when it does
constexpr za::I64 firstBackoff = 15 * 60;             // a 429 / 5xx: this long without a request, doubled each time
constexpr za::I64 longestBackoff = 24 * 60 * 60;
constexpr int maxListingPages = 60;                   // a guard (the site's listing is a few dozen pages)
constexpr int maxScreenshotBytes = 8 * 1024 * 1024;

// Realistic desktop browsers' User-Agent strings: one is picked per session, at random, and its family's usual
// headers sent with it (as that browser would).
struct Agent
{
    const char* userAgent;
    const char* acceptLanguage;
    bool firefox; // (its Accept header differs from Chromium's)
};
constexpr Agent agents[] = {
    {"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36",
        "en-US,en;q=0.9", false},
    {"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36 "
     "Edg/140.0.0.0",
        "en-US,en;q=0.9", false},
    {"Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:144.0) Gecko/20100101 Firefox/144.0", "en-US,en;q=0.5", true},
    {"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/141.0.0.0 "
     "Safari/537.36",
        "en-GB,en;q=0.9", false},
    {"Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:143.0) Gecko/20100101 Firefox/143.0", "en-US,en;q=0.5", true},
    {"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36",
        "en-US,en;q=0.9", false},
};
constexpr int agentCount = static_cast<int>(sizeof(agents) / sizeof(agents[0]));

// ---------------------------------------------------------------- the session (any thread; one request at a time)

int agentIndex = -1;           // agents[]: picked on the first request
za::U32 rngState = 0;          // vrand::xorshift01's (seeded from the clock once)
za::Atomic<int> requests{0};   // live requests made this session
za::U32 lastRequestAt[2] = {}; // SDL ticks of the last one, per site (0 Slipseer, 1 Quaddicted; 0: none yet)

// How soon a request may go (the thread's own: set by what it is doing, PaceScope).
enum class Pace
{
    Now,        // the player just asked for it (his pick: its page and screenshot): at once, like a click
    Auto,       // following another automatic request he asked for (maps_fetch, maps_slipseer_get): 1-3 s apart
    Background, // the start-up listing pass: vr_maps_slipseer_delay to twice it apart
};
Pace pace = Pace::Background;
struct PaceScope
{
    Pace before;
    explicit PaceScope(Pace p) : before{pace} { pace = p; }
    ~PaceScope() { pace = before; }
    PaceScope(const PaceScope&) = delete;
    PaceScope& operator=(const PaceScope&) = delete;
};

// What the thread is fetching, for the library's status line (activity()): set by ActivityScope, the bytes by
// writeBody / headerLine. Read on the main thread.
za::AtomicMutex activityLock;
za::String activityText;          // "" : idle
za::Atomic<int> activityBytes{0}; // received so far
za::Atomic<int> activityTotal{0}; // its Content-Length (0: not given)
za::Atomic<int> activityWaitUntil{0}; // SDL ticks: waiting for its turn until then (0: not waiting)
za::String lastError;             // the last request's failure, in one line ("" : none since)
za::U32 lastErrorAt = 0;
struct ActivityScope
{
    za::String before;
    explicit ActivityScope(const za::String& what)
    {
        za::LockGuard lock{activityLock};
        before = activityText;
        activityText = what;
        activityBytes.storeSeqCst(0);
        activityTotal.storeSeqCst(0);
    }
    ~ActivityScope()
    {
        za::LockGuard lock{activityLock};
        activityText = before;
    }
    ActivityScope(const ActivityScope&) = delete;
    ActivityScope& operator=(const ActivityScope&) = delete;
};
void failed(const za::String& what)
{
    za::LockGuard lock{activityLock};
    lastError = what;
    lastErrorAt = za::max(SDL_GetTicks(), za::U32{1});
}
za::I64 backoffUntil = 0;      // unix seconds: no request before then
za::I64 backoffSeconds = 0;    // the last back-off's length (the next is twice it)
bool backoffRead = false;      // backoff.txt read
// robots.txt's Disallow prefixes for every agent, per site: 0 Slipseer, 1 Quaddicted (its screenshots).
struct Robots
{
    za::Vector<za::String> disallowed;
    double crawlDelay = 0.0; // its Crawl-delay (seconds) for every agent: the least gap between automatic requests
    bool known = false;
};
Robots robotsOf[2];
SDL_atomic_t cancel{};          // the thread's stop (quitting)

void seedRng()
{
    if(!rngState)
    {
        rngState = static_cast<za::U32>(SDL_GetPerformanceCounter()) | 1u;
    }
}

[[nodiscard]] const Agent& agent()
{
    if(agentIndex < 0)
    {
        seedRng();
        agentIndex = za::min(static_cast<int>(vrand::xorshift01(rngState) * agentCount), agentCount - 1);
    }
    return agents[agentIndex];
}

[[nodiscard]] za::I64 now()
{
    return static_cast<za::I64>(time(nullptr));
}

[[nodiscard]] za::String cacheDir()
{
    return files::baseCacheDir() + "/slipseer";
}

[[nodiscard]] za::U64 hashUrl(const za::String& url)
{
    za::U64 h = 1469598103934665603ull; // FNV-1a
    for(za::SizeT i = 0; i < url.size(); i++)
    {
        h ^= static_cast<unsigned char>(url.cStr()[i]);
        h *= 1099511628211ull;
    }
    return h;
}

[[nodiscard]] za::String cachePath(const za::String& url, bool asset)
{
    char name[40];
    q_snprintf(name, sizeof(name), "/%016llx.%s", static_cast<unsigned long long>(hashUrl(url)), asset ? "img" : "html");
    return cacheDir() + name;
}

struct Meta
{
    za::String url;
    za::String etag;
    za::String modified;
    za::I64 fetchedAt{0};
    bool missing{false}; // the site answered 404 / 410 (a screenshot that is not there): not asked again for a week
};
constexpr za::I64 missingSeconds = 7 * 24 * 60 * 60;

bool readMeta(const za::String& path, Meta& m)
{
    za::String text;
    if(!files::readText((path + ".meta").cStr(), text))
    {
        return false;
    }
    files::forLines(text, [&](za::StringView l) {
        const za::String line{l};
        const char* tab = strchr(line.cStr(), '\t');
        if(!tab)
        {
            return;
        }
        const za::String key{line.cStr(), static_cast<za::SizeT>(tab - line.cStr())};
        const za::String value{tab + 1};
        if(key == "url")
        {
            m.url = value;
        }
        else if(key == "etag")
        {
            m.etag = value;
        }
        else if(key == "modified")
        {
            m.modified = value;
        }
        else if(key == "fetched")
        {
            m.fetchedAt = static_cast<za::I64>(strtoll(value.cStr(), nullptr, 10));
        }
        else if(key == "missing")
        {
            m.missing = value == "1";
        }
    });
    return m.fetchedAt > 0;
}

void writeMeta(const za::String& path, const Meta& m)
{
    za::String text;
    text += za::String{"url\t"} + m.url + "\n";
    text += za::String{"fetched\t"} + za::toString(m.fetchedAt) + "\n";
    if(m.etag.size())
    {
        text += za::String{"etag\t"} + m.etag + "\n";
    }
    if(m.modified.size())
    {
        text += za::String{"modified\t"} + m.modified + "\n";
    }
    if(m.missing)
    {
        text += "missing\t1\n";
    }
    (void)files::writeText((path + ".meta").cStr(), text);
}

void readBackoff()
{
    if(backoffRead)
    {
        return;
    }
    backoffRead = true;
    za::String text;
    if(files::readText((cacheDir() + "/backoff.txt").cStr(), text))
    {
        sscanf(text.cStr(), "%lld %lld", reinterpret_cast<long long*>(&backoffUntil),
            reinterpret_cast<long long*>(&backoffSeconds));
    }
}

void startBackoff(za::I64 retryAfter)
{
    backoffSeconds = retryAfter > 0 ? retryAfter
                                    : (backoffSeconds > 0 ? za::min(backoffSeconds * 2, longestBackoff) : firstBackoff);
    backoffUntil = now() + backoffSeconds;
    files::createDirectories(cacheDir().cStr());
    char text[64];
    q_snprintf(text, sizeof(text), "%lld %lld\n", static_cast<long long>(backoffUntil),
        static_cast<long long>(backoffSeconds));
    (void)files::writeText((cacheDir() + "/backoff.txt").cStr(), za::StringView{text});
}

// The response's headers (the last response's, after a redirect).
struct Received
{
    za::String etag;
    za::String modified;
    za::I64 retryAfter{-1};
};

size_t headerLine(char* buffer, size_t size, size_t nitems, void* userdata)
{
    Received& r = *static_cast<Received*>(userdata);
    const size_t n = size * nitems;
    za::String line{buffer, static_cast<za::SizeT>(n)};
    while(line.size() && (line.cStr()[line.size() - 1] == '\r' || line.cStr()[line.size() - 1] == '\n'))
    {
        line.erase(line.size() - 1, 1);
    }
    if(!q_strncasecmp(line.cStr(), "HTTP/", 5))
    {
        r = Received{}; // (a redirect's next response)
        return n;
    }
    const char* colon = strchr(line.cStr(), ':');
    if(!colon)
    {
        return n;
    }
    const za::SizeT keyLen = static_cast<za::SizeT>(colon - line.cStr());
    const char* value = colon + 1;
    while(*value == ' ')
    {
        value++;
    }
    const auto is = [&](const char* key) { return strlen(key) == keyLen && !q_strncasecmp(line.cStr(), key, keyLen); };
    if(is("etag"))
    {
        r.etag = za::String{value};
    }
    else if(is("last-modified"))
    {
        r.modified = za::String{value};
    }
    else if(is("retry-after"))
    {
        r.retryAfter = isdigit(static_cast<unsigned char>(*value)) ? static_cast<za::I64>(atoll(value)) : -1;
    }
    else if(is("content-length"))
    {
        activityTotal.storeSeqCst(static_cast<int>(za::min(atoll(value), 1ll << 30)));
    }
    return n;
}

size_t writeBody(void* buffer, size_t size, size_t nmemb, void* stream)
{
    if(SDL_AtomicGet(&cancel))
    {
        return 0;
    }
    za::Vector<char>& body = *static_cast<za::Vector<char>*>(stream);
    const za::SizeT n = size * nmemb;
    if(body.size() + n > static_cast<za::SizeT>(maxScreenshotBytes) * 2)
    {
        return 0; // (no page or picture of ours is this big)
    }
    body.reserveMore(n);
    body.unsafeEmplaceBackRange(static_cast<const char*>(buffer), n);
    activityBytes.storeSeqCst(static_cast<int>(body.size()));
    return nmemb;
}

bool servePendingPick(); // (the module, below: a pick the player made meanwhile, served at once)

// Waits until `gapMs` has passed since site `host`'s last request (false: the thread was told to stop meanwhile).
// An automatic request's wait gives way to a pick the player makes meanwhile: it goes first, at once, and the wait
// then counts from it.
bool waitTurn(double gapMs, int host)
{
    if(!lastRequestAt[host] || gapMs <= 0.0)
    {
        return true;
    }
    while(static_cast<double>(SDL_GetTicks() - lastRequestAt[host]) < gapMs)
    {
        if(SDL_AtomicGet(&cancel))
        {
            activityWaitUntil.storeSeqCst(0);
            return false;
        }
        activityWaitUntil.storeSeqCst(static_cast<int>(lastRequestAt[host] + static_cast<za::U32>(gapMs)));
        if(pace != Pace::Now && servePendingPick())
        {
            continue;
        }
        SDL_Delay(20);
    }
    activityWaitUntil.storeSeqCst(0);
    return true;
}

[[nodiscard]] bool pathOf(const za::String& url, za::String& path)
{
    const char* p = strstr(url.cStr(), "://");
    if(!p)
    {
        return false;
    }
    p = strchr(p + 3, '/');
    path = za::String{p ? p : "/"};
    return true;
}

[[nodiscard]] bool sameSite(const za::String& url)
{
    return !q_strncasecmp(url.cStr(), siteRoot, strlen(siteRoot)) ||
           !q_strncasecmp(url.cStr(), "https://slipseer.com", 20);
}

// Quaddicted's: one screenshot of a picked map (vr_mapindex's shot URL), under the same rules as Slipseer's.
[[nodiscard]] bool quaddictedSite(const za::String& url)
{
    return !q_strncasecmp(url.cStr(), quaddictedRoot, strlen(quaddictedRoot));
}

enum class Kind
{
    Page,   // a page a person opens (seconds apart)
    Asset,  // what a page shows (its screenshot): fetched just after it, as a browser would
    Robots, // robots.txt
};

bool fetch(const za::String& url, Kind kind, za::I64 ttl, const char* referer, bool network, za::Vector<char>& body,
    za::String& error, bool* fresh = nullptr);

// The Disallow lines of `text`'s `User-agent: *` groups, and their Crawl-delay (seconds; 0: none).
void parseRobots(const char* text, za::Vector<za::String>& out, double* crawlDelay = nullptr)
{
    bool forAll = false, inRules = false;
    files::forLines(za::StringView{text}, [&](za::StringView l) {
        za::String line{l};
        if(const char* hash = strchr(line.cStr(), '#'))
        {
            line = za::String{line.cStr(), static_cast<za::SizeT>(hash - line.cStr())};
        }
        const char* colon = strchr(line.cStr(), ':');
        if(!colon)
        {
            return;
        }
        za::String key{line.cStr(), static_cast<za::SizeT>(colon - line.cStr())};
        const char* value = colon + 1;
        while(*value == ' ' || *value == '\t')
        {
            value++;
        }
        za::String v{value};
        while(v.size() && (v.cStr()[v.size() - 1] == ' ' || v.cStr()[v.size() - 1] == '\r'))
        {
            v.erase(v.size() - 1, 1);
        }
        if(!q_strcasecmp(key.cStr(), "user-agent"))
        {
            if(inRules)
            {
                forAll = false; // (a new group)
                inRules = false;
            }
            forAll = forAll || v == "*";
        }
        else if(!q_strcasecmp(key.cStr(), "disallow") || !q_strcasecmp(key.cStr(), "allow"))
        {
            inRules = true;
            if(forAll && !q_strcasecmp(key.cStr(), "disallow") && v.size())
            {
                out.pushBack(v);
            }
        }
        else if(!q_strcasecmp(key.cStr(), "crawl-delay"))
        {
            inRules = true;
            if(forAll && crawlDelay)
            {
                *crawlDelay = za::max(*crawlDelay, za::min(atof(v.cStr()), 60.0)); // (capped: a typo is not an hour)
            }
        }
    });
}

// robots.txt of site `host` (0 Slipseer, 1 Quaddicted) read (fetched at most once a week).
void readRobots(bool network, int host)
{
    Robots& r = robotsOf[host];
    if(r.known)
    {
        return;
    }
    za::Vector<char> body;
    za::String error;
    if(!fetch(za::String{host ? quaddictedRoot : siteRoot} + "/robots.txt", Kind::Robots, robotsSeconds, nullptr, network,
           body, error))
    {
        r.known = network; // (none: nothing disallowed; not known yet when only the cache was read)
        return;
    }
    r.known = true;
    body.pushBack('\0');
    parseRobots(body.data(), r.disallowed, &r.crawlDelay);
}

[[nodiscard]] bool robotsAllow(const za::String& url, int host)
{
    za::String path;
    if(!pathOf(url, path))
    {
        return false;
    }
    for(const za::String& prefix : robotsOf[host].disallowed)
    {
        if(!strncmp(path.cStr(), prefix.cStr(), prefix.size()))
        {
            return false;
        }
    }
    return true;
}

// `url`'s body: the cached copy while it is fresh; else (when `network`) fetched, conditionally when a copy is there
// (a 304 keeps it); else the cached copy however old. False: none (`error` says why).
bool fetch(const za::String& url, Kind kind, za::I64 ttl, const char* referer, bool network, za::Vector<char>& body,
    za::String& error, bool* fresh)
{
    body.clear();
    const bool asset = kind == Kind::Asset;
    const za::String path = cachePath(url, asset);
    Meta meta;
    const bool known = readMeta(path, meta) && meta.url == url;
    if(known && meta.missing && now() - meta.fetchedAt < missingSeconds)
    {
        error = za::String{"not on the site (it answered 404 lately)"};
        return false;
    }
    const bool cached = known && !meta.missing && files::isFile(path.cStr());
    const auto fromCache = [&](const char* why) {
        if(!cached || !files::readBytes(path.cStr(), body))
        {
            error = za::String{why};
            return false;
        }
        return true;
    };
    if(fresh)
    {
        *fresh = false;
    }
    if(cached && now() - meta.fetchedAt < ttl)
    {
        return fromCache("the cached copy could not be read");
    }
    const bool quad = quaddictedSite(url);
    if(!network || (!vr_maps_slipseer.value && !quad))
    {
        return fromCache("not in the cache (the network is off for it: a test run, or vr_maps_fetch 0)");
    }
    readBackoff();
    if(now() < backoffUntil)
    {
        return fromCache("Slipseer asked us to wait (backing off); not in the cache meanwhile");
    }
    if(!sameSite(url) && kind != Kind::Asset)
    {
        error = za::String{"not a Slipseer page: "} + url;
        return false;
    }
    if(kind != Kind::Robots)
    {
        readRobots(network, quad ? 1 : 0);
        if((quad || sameSite(url)) && !robotsAllow(url, quad ? 1 : 0))
        {
            return fromCache("robots.txt disallows it");
        }
    }

    // A person's pace. What the player just asked for goes at once, as his click would (still one request at a time).
    // Between automatic requests: a page 1-3 s after the last when he asked for the run (maps_fetch), else
    // vr_maps_slipseer_delay to twice it (the start-up pass); what a page shows, a moment after it; never under the
    // site's robots.txt Crawl-delay.
    const int host = quad ? 1 : 0;
    double gap = 0.0;
    if(pace != Pace::Now)
    {
        seedRng();
        const double delay = za::max(static_cast<double>(vr_maps_slipseer_delay.value), 1.0) * 1000.0;
        gap = kind == Kind::Asset ? static_cast<double>(vrand::xorshiftRange(rngState, 300.f, 1200.f))
              : pace == Pace::Auto ? static_cast<double>(vrand::xorshiftRange(rngState, 1000.f, 3000.f))
                                   : delay * static_cast<double>(vrand::xorshiftRange(rngState, 1.f, 2.f));
        gap = za::max(gap, robotsOf[host].crawlDelay * 1000.0);
    }
    if(!waitTurn(gap, host))
    {
        error = "cancelled";
        return false;
    }

    const Agent& a = agent();
    za::Vector<za::String> lines;
    lines.pushBack(za::String{"User-Agent: "} + a.userAgent);
    lines.pushBack(za::String{asset ? (a.firefox ? "Accept: image/avif,image/webp,image/png,image/svg+xml,image/*;q=0.8,*/*;q=0.5"
                                                  : "Accept: image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8")
                                    : (a.firefox ? "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"
                                                 : "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,"
                                                   "image/webp,image/apng,*/*;q=0.8")});
    lines.pushBack(za::String{"Accept-Language: "} + a.acceptLanguage);
    if(!asset)
    {
        lines.pushBack(za::String{"Upgrade-Insecure-Requests: 1"});
    }
    if(referer && *referer)
    {
        lines.pushBack(za::String{"Referer: "} + referer);
    }
    if(cached && meta.etag.size())
    {
        lines.pushBack(za::String{"If-None-Match: "} + meta.etag);
    }
    if(cached && meta.modified.size())
    {
        lines.pushBack(za::String{"If-Modified-Since: "} + meta.modified);
    }
    za::Vector<const char*> headers;
    for(const za::String& l : lines)
    {
        headers.pushBack(l.cStr());
    }

    Received got;
    download_t dl{};
    dl.headers = headers.data();
    dl.num_headers = static_cast<int>(headers.size());
    dl.write_fn = writeBody;
    dl.write_data = &body;
    dl.abort = &cancel;
    dl.header_fn = headerLine;
    dl.header_data = &got;
    const bool ok = Download(url.cStr(), &dl);
    lastRequestAt[host] = za::max(SDL_GetTicks(), za::U32{1});
    requests.fetchAddSeqCst(1);

    if(dl.response == 304 && cached)
    {
        meta.fetchedAt = now();
        writeMeta(path, meta);
        return fromCache("the cached copy could not be read");
    }
    if(ok && body.size())
    {
        backoffSeconds = 0;
        files::createDirectories(cacheDir().cStr());
        (void)files::writeBytes(path.cStr(), body.data(), body.size());
        Meta m;
        m.url = url;
        m.fetchedAt = now();
        m.etag = got.etag;
        m.modified = got.modified;
        writeMeta(path, m);
        if(fresh)
        {
            *fresh = true;
        }
        return true;
    }
    const za::String what = dl.error ? za::String{dl.error} : za::String{"HTTP "} + za::toString(dl.response);
    if(dl.response == 429 || dl.response == 403 || dl.response >= 500)
    {
        startBackoff(got.retryAfter);
        error = what + ": waiting " + za::toString(static_cast<int>(backoffSeconds / 60)) + " min before the next request";
    }
    else
    {
        error = what;
    }
    if(asset && (dl.response == 404 || dl.response == 410) && !cached)
    {
        // (a screenshot the site does not have: remembered, so picking the map again asks no more for a week)
        files::createDirectories(cacheDir().cStr());
        Meta m;
        m.url = url;
        m.fetchedAt = now();
        m.missing = true;
        writeMeta(path, m);
    }
    if(!SDL_AtomicGet(&cancel))
    {
        failed(error);
    }
    za::String why = error;
    if(fromCache(why.cStr()))
    {
        return true; // (an older copy rather than none)
    }
    error = why;
    return false;
}

} // namespace

// ---------------------------------------------------------------- the HTML

namespace
{

// Entities a page's text uses, decoded (numeric ones to UTF-8).
void appendDecoded(za::String& out, const char* s, za::SizeT n)
{
    for(za::SizeT i = 0; i < n; i++)
    {
        if(s[i] != '&')
        {
            out += s[i];
            continue;
        }
        const char* semi = static_cast<const char*>(memchr(s + i, ';', za::min(n - i, za::SizeT{12})));
        if(!semi)
        {
            out += '&';
            continue;
        }
        const za::String name{s + i + 1, static_cast<za::SizeT>(semi - (s + i + 1))};
        unsigned long code = 0;
        if(name.size() > 1 && name.cStr()[0] == '#')
        {
            code = name.cStr()[1] == 'x' || name.cStr()[1] == 'X' ? strtoul(name.cStr() + 2, nullptr, 16)
                                                                  : strtoul(name.cStr() + 1, nullptr, 10);
        }
        else if(name == "amp")
        {
            code = '&';
        }
        else if(name == "lt")
        {
            code = '<';
        }
        else if(name == "gt")
        {
            code = '>';
        }
        else if(name == "quot")
        {
            code = '"';
        }
        else if(name == "apos")
        {
            code = '\'';
        }
        else if(name == "nbsp")
        {
            code = ' ';
        }
        else if(name == "ndash" || name == "mdash")
        {
            code = '-';
        }
        else if(name == "rsquo" || name == "lsquo")
        {
            code = '\'';
        }
        else if(name == "hellip")
        {
            code = 0x2026;
        }
        if(!code)
        {
            out += '&';
            continue;
        }
        if(code < 0x20 || code == 0x7F)
        {
            out += ' '; // (a control code: never into the text drawn)
        }
        else if((code >= 0xD800 && code <= 0xDFFF) || code > 0xFFFF)
        {
            out += '?'; // (a surrogate is no character; past 3 bytes of UTF-8, an emoji: none in the font)
        }
        else if(code < 0x80)
        {
            out += static_cast<char>(code);
        }
        else if(code < 0x800)
        {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xE0 | ((code >> 12) & 0x0F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        i = static_cast<za::SizeT>(semi - s);
    }
}

// The text of s..e: tags dropped (a <br>, </p>, </li> or </div> as a space), entities decoded, whitespace folded.
[[nodiscard]] za::String plainText(const char* s, const char* e)
{
    za::String raw;
    for(const char* p = s; p < e;)
    {
        if(*p == '<')
        {
            const char* close = static_cast<const char*>(memchr(p, '>', static_cast<size_t>(e - p)));
            if(!close)
            {
                break;
            }
            if(!q_strncasecmp(p, "<script", 7) || !q_strncasecmp(p, "<style", 6))
            {
                const char* closer = !q_strncasecmp(p, "<script", 7) ? "</script>" : "</style>";
                const char* end = strstr(close, closer);
                p = end && end < e ? end + strlen(closer) : e;
                continue;
            }
            raw += ' ';
            p = close + 1;
            continue;
        }
        const char* next = static_cast<const char*>(memchr(p, '<', static_cast<size_t>(e - p)));
        const char* stop = next ? next : e;
        appendDecoded(raw, p, static_cast<za::SizeT>(stop - p));
        p = stop;
    }
    za::String out;
    bool space = true;
    for(za::SizeT i = 0; i < raw.size(); i++)
    {
        const unsigned char c = static_cast<unsigned char>(raw.cStr()[i]);
        if(c <= ' ')
        {
            space = true;
            continue;
        }
        if(space && out.size())
        {
            out += ' ';
        }
        space = false;
        out += static_cast<char>(c);
    }
    return out;
}

// An attribute's value in the tag starting at `tag` ("" : none).
[[nodiscard]] za::String attribute(const char* tag, const char* name)
{
    const char* end = strchr(tag, '>');
    if(!end)
    {
        return za::String{};
    }
    const za::SizeT n = strlen(name);
    for(const char* p = tag; p + n < end; p++)
    {
        if(!q_strncasecmp(p, name, n) && (p[n] == '=' ) && (p == tag || isspace(static_cast<unsigned char>(p[-1]))))
        {
            const char* v = p + n + 1;
            const char quote = *v == '"' || *v == '\'' ? *v++ : 0;
            const char* ve = v;
            while(ve < end && (quote ? *ve != quote : !isspace(static_cast<unsigned char>(*ve))))
            {
                ve++;
            }
            za::String out;
            appendDecoded(out, v, static_cast<za::SizeT>(ve - v));
            return out;
        }
    }
    return za::String{};
}

// The element whose start tag is at `tag`: its inner text's range (to its matching close tag of the same name, nested
// ones counted). False: not closed.
bool innerRange(const char* tag, const char*& s, const char*& e)
{
    char name[16];
    int n = 0;
    for(const char* p = tag + 1; n < 15 && isalnum(static_cast<unsigned char>(*p)); p++)
    {
        name[n++] = static_cast<char>(tolower(static_cast<unsigned char>(*p)));
    }
    name[n] = 0;
    const char* open = strchr(tag, '>');
    if(!open || !n)
    {
        return false;
    }
    s = open + 1;
    int depth = 1;
    for(const char* p = s; *p; p++)
    {
        if(*p != '<')
        {
            continue;
        }
        const bool closing = p[1] == '/';
        const char* q = p + (closing ? 2 : 1);
        if(q_strncasecmp(q, name, static_cast<size_t>(n)) || isalnum(static_cast<unsigned char>(q[n])))
        {
            continue;
        }
        depth += closing ? -1 : 1;
        if(!depth)
        {
            e = p;
            return true;
        }
    }
    return false;
}

// The next start tag at or after `from`, before `limit`, whose class attribute holds `cls` as a word ("" : any
// class) and whose name is `tagName` ("" : any). Null: none.
[[nodiscard]] const char* findTag(const char* from, const char* limit, const char* tagName, const char* cls)
{
    const za::SizeT tn = strlen(tagName);
    for(const char* p = from; p && *p && (!limit || p < limit); p++)
    {
        if(*p != '<' || p[1] == '/' || p[1] == '!')
        {
            continue;
        }
        if(tn && (q_strncasecmp(p + 1, tagName, tn) || isalnum(static_cast<unsigned char>(p[1 + tn]))))
        {
            continue;
        }
        if(!*cls)
        {
            return p;
        }
        const za::String c = attribute(p, "class");
        const za::SizeT cn = strlen(cls);
        for(const char* w = c.cStr(); (w = strstr(w, cls)) != nullptr; w += cn)
        {
            const bool startOk = w == c.cStr() || w[-1] == ' ';
            const bool endOk = !w[cn] || w[cn] == ' ';
            if(startOk && endOk)
            {
                return p;
            }
        }
    }
    return nullptr;
}

} // namespace

za::String absoluteUrl(const char* base, const za::String& href)
{
    if(href.empty())
    {
        return za::String{};
    }
    if(strstr(href.cStr(), "://"))
    {
        return href;
    }
    if(href.cStr()[0] == '/' && href.size() > 1 && href.cStr()[1] == '/')
    {
        return za::String{"https:"} + href;
    }
    const char* scheme = strstr(base, "://");
    const char* hostEnd = scheme ? strchr(scheme + 3, '/') : nullptr;
    const za::String origin = hostEnd ? za::String{base, static_cast<za::SizeT>(hostEnd - base)} : za::String{base};
    if(href.cStr()[0] == '/')
    {
        return origin + href;
    }
    // Relative to the base's folder (its query left out).
    za::String folder{base};
    if(const char* q = strchr(folder.cStr(), '?'))
    {
        folder = za::String{folder.cStr(), static_cast<za::SizeT>(q - folder.cStr())};
    }
    const char* slash = strrchr(folder.cStr(), '/');
    if(slash && hostEnd && slash >= folder.cStr() + (hostEnd - base))
    {
        folder = za::String{folder.cStr(), static_cast<za::SizeT>(slash - folder.cStr() + 1)};
    }
    else
    {
        folder = origin + "/";
    }
    return folder + href;
}

za::String idFor(const za::String& page)
{
    const za::String key = za::String{"slipseer:"} + page;
    const sha256::Digest d = sha256::of(key.cStr(), key.size());
    char hex[65];
    sha256::toHex(d, hex);
    return za::String{hex};
}

void requirementFields(const za::String& needs, za::String& extract, za::String& theme)
{
    const char* r = needs.cStr();
    const auto has = [&](const char* word) {
        for(const char* p = r; *p; p++)
        {
            if(!q_strncasecmp(p, word, strlen(word)))
            {
                return true;
            }
        }
        return false;
    };
    theme = za::String{};
    if(has("arcane") || !q_strcasecmp(r, "ad"))
    {
        extract = "{base}/ad/";
        theme = "arcane_dimensions";
    }
    else if(has("quoth"))
    {
        extract = "{base}/quoth/";
        theme = "quoth";
    }
    else if(has("copper"))
    {
        extract = "{base}/copper/";
    }
    else if(has("alkaline"))
    {
        extract = "{base}/alkaline/";
    }
    else if(has("hipnotic") || has("armagon"))
    {
        extract = "{base}/hipnotic/";
    }
    else if(has("rogue") || has("dissolution"))
    {
        extract = "{base}/rogue/";
    }
    else
    {
        extract = "{base}/id1/";
    }
}

// The site is XenForo's Resource Manager. A listing page (a category's, e.g. Maps) holds one
// `div.structItem--resource` per map: its title link (the map's page), data-author, the first <time datetime=...>
// (first uploaded), its category link (what it needs: Vanilla, Copper, Arcane Dimensions, Alkaline, Quoth, Other,
// Multiplayer), its icon, its tag line and its stars; the next page is `a.pageNav-jump--next`. A map's page carries a
// JSON-LD block (schema.org CreativeWork: headline, description, dateCreated, author, thumbnailUrl, aggregateRating),
// its description as HTML (`article.resourceBody-main .bbWrapper`, images as `img.bbImage`), and its Download button
// (`a.button--icon--download`, the page's URL + "download").

namespace
{

// A category's slug ("arcane-dimensions") as what the map needs, and its game mode.
void categoryNeeds(const za::String& slug, za::String& needs, za::String& mode)
{
    mode = "singleplayer";
    needs = za::String{};
    if(slug == "copper")
    {
        needs = "Copper";
    }
    else if(slug == "arcane-dimensions")
    {
        needs = "Arcane Dimensions";
    }
    else if(slug == "alkaline")
    {
        needs = "Alkaline";
    }
    else if(slug == "quoth")
    {
        needs = "Quoth";
    }
    else if(slug == "multiplayer")
    {
        mode = "deathmatch";
    }
}

// "4.00 star(s)" as x100 (0: none).
[[nodiscard]] int starsOf(const char* from, const char* limit)
{
    const char* r = findTag(from, limit, "span", "ratingStars");
    if(!r)
    {
        return 0;
    }
    const za::String title = attribute(r, "title");
    const double v = atof(title.cStr());
    return v > 0.0 && v <= 5.0 ? static_cast<int>(v * 100.0 + 0.5) : 0;
}

[[nodiscard]] za::String innerText(const char* tag)
{
    const char* s = nullptr;
    const char* e = nullptr;
    return tag && innerRange(tag, s, e) ? plainText(s, e) : za::String{};
}

// A category link's slug ("vanilla" from ".../categories/vanilla.3/"), the first in from..limit.
[[nodiscard]] za::String categorySlug(const char* from, const char* limit)
{
    const char* c = strstr(from, "resources/categories/");
    if(!c || (limit && c >= limit))
    {
        return za::String{};
    }
    c += strlen("resources/categories/");
    const char* end = c;
    while(*end && *end != '.' && *end != '/' && *end != '"')
    {
        end++;
    }
    return za::String{c, static_cast<za::SizeT>(end - c)};
}

} // namespace

void parseListing(const char* html, const char* pageUrl, za::Vector<Item>& out, za::String& next)
{
    next = za::String{};
    const char* p = findTag(html, nullptr, "div", "structItem--resource");
    while(p)
    {
        const char* end = findTag(p + 1, nullptr, "div", "structItem--resource");
        const char* limit = end ? end : p + strlen(p);
        Item it;
        it.author = attribute(p, "data-author");
        if(const char* t = findTag(p, limit, "div", "structItem-title"))
        {
            if(const char* a = findTag(t, limit, "a", ""))
            {
                it.page = absoluteUrl(pageUrl, attribute(a, "href"));
                it.title = innerText(a);
            }
        }
        if(const char* time = findTag(p, limit, "time", ""))
        {
            const za::String when = attribute(time, "datetime");
            it.date = za::String{when.cStr(), za::min(when.size(), za::SizeT{10})};
        }
        if(const char* minor = findTag(p, limit, "div", "structItem-minor"))
        {
            za::String mode;
            it.category = categorySlug(minor, limit);
            categoryNeeds(it.category, it.needs, mode);
        }
        if(const char* icon = findTag(p, limit, "div", "structItem-iconContainer"))
        {
            if(const char* img = findTag(icon, limit, "img", ""))
            {
                it.thumb = absoluteUrl(pageUrl, attribute(img, "src"));
            }
        }
        it.tagline = innerText(findTag(p, limit, "div", "structItem-resourceTagLine"));
        it.rating = starsOf(p, limit);
        if(it.page.size() && it.title.size())
        {
            out.pushBack(ZA_MOVE(it));
        }
        p = end;
    }
    if(const char* n = findTag(html, nullptr, "a", "pageNav-jump--next"))
    {
        next = absoluteUrl(pageUrl, attribute(n, "href"));
    }
}

bool parseDetails(const char* html, const char* pageUrl, Details& out)
{
    // The JSON-LD block: what the page says of itself.
    bool any = false;
    for(const char* ld = strstr(html, "application/ld+json"); ld; ld = strstr(ld + 1, "application/ld+json"))
    {
        const char* open = strchr(ld, '>');
        const char* close = open ? strstr(open, "</script>") : nullptr;
        if(!close)
        {
            break;
        }
        const za::String text{open + 1, static_cast<za::SizeT>(close - open - 1)};
        json_t* json = JSON_Parse(text.cStr());
        if(!json)
        {
            continue;
        }
        const jsonentry_t* main = JSON_Find(json->root, "mainEntity", JSON_OBJECT);
        if(main)
        {
            any = true;
            const auto str = [&](const jsonentry_t* e, const char* name) {
                const char* v = e ? JSON_FindString(e, name) : nullptr;
                return v ? za::String{v} : za::String{};
            };
            out.title = str(main, "headline");
            out.description = str(main, "description");
            const za::String created = str(main, "dateCreated");
            out.date = za::String{created.cStr(), za::min(created.size(), za::SizeT{10})};
            out.author = str(JSON_Find(main, "author", JSON_OBJECT), "name");
            out.screenshot = str(main, "thumbnailUrl");
            if(const jsonentry_t* rating = JSON_Find(main, "aggregateRating", JSON_OBJECT))
            {
                if(const double* v = JSON_FindNumber(rating, "ratingValue"))
                {
                    out.rating = *v > 0.0 && *v <= 5.0 ? static_cast<int>(*v * 100.0 + 0.5) : 0;
                }
            }
        }
        JSON_Free(json);
        if(main)
        {
            break;
        }
    }

    // The description as the page shows it (whole, where the JSON-LD's may be cut), and its first picture.
    if(const char* body = findTag(html, nullptr, "article", "resourceBody-main"))
    {
        const char* s = nullptr;
        const char* e = nullptr;
        if(innerRange(body, s, e))
        {
            if(const char* bb = findTag(s, e, "div", "bbWrapper"))
            {
                const za::String text = innerText(bb);
                if(text.size())
                {
                    out.description = text;
                    any = true;
                }
            }
            if(const char* img = findTag(s, e, "img", "bbImage"))
            {
                za::String src = attribute(img, "data-url");
                if(src.empty())
                {
                    src = attribute(img, "src");
                }
                if(src.size() && strncmp(src.cStr(), "data:", 5))
                {
                    out.screenshot = absoluteUrl(pageUrl, src);
                }
            }
        }
    }
    // No picture in the description: the page's own large one (og:image, an attachment: ".../attachments/" alone is
    // none), else the JSON-LD's icon (kept above).
    if(out.screenshot.empty() || strstr(out.screenshot.cStr(), "resource_icons"))
    {
        for(const char* m = strstr(html, "og:image"); m; m = strstr(m + 1, "og:image"))
        {
            const char* tag = m;
            while(tag > html && *tag != '<')
            {
                tag--;
            }
            const za::String content = attribute(tag, "content");
            const char* att = strstr(content.cStr(), "attachments/");
            if(att && att[strlen("attachments/")])
            {
                out.screenshot = absoluteUrl(pageUrl, content);
                break;
            }
        }
    }
    // (a map hosted elsewhere has a "redirect" button instead: the site's /download link sends the browser to its host;
    // what comes back is checked to be a zip at install time)
    const char* dl = findTag(html, nullptr, "a", "button--icon--download");
    if(!dl)
    {
        dl = findTag(html, nullptr, "a", "button--icon--redirect");
    }
    if(dl)
    {
        out.download = absoluteUrl(pageUrl, attribute(dl, "href"));
        any = true;
    }
    if(out.title.empty())
    {
        out.title = innerText(findTag(html, nullptr, "h1", "p-title-value"));
    }
    za::String mode;
    const char* crumbs = findTag(html, nullptr, "ul", "p-breadcrumbs");
    za::String slug;
    // (the last category link in the breadcrumbs: the map's own category)
    for(const char* c = crumbs; c && (c = strstr(c, "resources/categories/")) != nullptr; c++)
    {
        const char* crumbsEnd = strstr(crumbs, "</ul>");
        if(crumbsEnd && c > crumbsEnd)
        {
            break;
        }
        slug = categorySlug(c, nullptr);
    }
    if(slug.size())
    {
        categoryNeeds(slug, out.needs, mode);
    }
    return any && out.download.size();
}

// ---------------------------------------------------------------- the thread

namespace
{

za::Thread worker;
za::Atomic<bool> running{false};

// Main thread -> the thread (under `handoff`).
za::AtomicMutex handoff;
bool passWanted = false;   // the listing pass
bool passNetwork = false;  // it may use the network
za::String wantedPage;     // a map's page wanted (the picked one)
bool wantedNetwork = false;
za::String wantedShotKey;  // a screenshot wanted on its own (Quaddicted's: no page to read): its key, as shown, and URL
za::String wantedShotUrl;
bool wantedShotNetwork = false;
za::U32 wantedAt = 0;      // SDL ticks of the latest pick (servePick's timings)
za::String wantedTitle;    // its title (the status line's)
bool passAsked = false;    // the pass was asked for (maps_fetch): Pace::Auto, else Pace::Background
za::String getUrl;         // maps_slipseer_get's URL (development: one page fetched and kept)
bool getAsset = false;     // ... a picture (its cache name: .img)

// The thread -> main thread (under `handoff`).
za::Atomic<int> served{0}; // picks served (maps_slipseer_time issues the next when this moves)
za::UniquePtr<mapindex::Index> pendingPart;
za::Vector<za::String> pendingLog;
za::String progress;
za::String workingOn; // the page being fetched now ("" : none)
struct Picture
{
    za::String page;
    unsigned char* rgba{nullptr}; // (VR_HeapMalloc'd by stb_image; the main thread takes it)
    int width{0}, height{0};
};
Picture pendingPicture;

// The thread's own (only one runs at a time: each start joins the last).
za::Vector<Item> items;
ankerl::unordered_dense::map<za::String, Details> details; // by page
bool itemsRead = false;

// The main thread's.
za::String lastStatus;
za::String shownPage; // the screenshot held (its page)
za::Vector<unsigned char> shownPic; // Draw_PicBytes
za::Vector<unsigned> shownTexels;
int shownWidth = 0, shownHeight = 0;
ankerl::unordered_dense::map<za::String, bool> detailedPages; // pages whose details reached the index

void note(const char* fmt, ...)
{
    char line[512];
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    za::LockGuard lock{handoff};
    pendingLog.pushBack(za::String{line});
    progress = za::String{line};
}

[[nodiscard]] za::String listingUrl()
{
    return za::String{siteRoot} + "/index.php?resources/categories/maps.1/"; // (page 1; the rest are its "next" links)
}

// The Items and Details as mapindex entries (their own arena; mapindex::setSlipseer appends them to Quaddicted's).
za::UniquePtr<mapindex::Index> buildPart()
{
    auto idx = za::makeUnique<mapindex::Index>();
    idx->source = listingUrl();
    idx->fetchedAt = now();
    const auto add = [&](const za::String& s) {
        mapindex::Text t;
        t.off = static_cast<za::U32>(idx->text.size());
        for(za::SizeT i = 0; i < s.size(); i++)
        {
            const unsigned char c = static_cast<unsigned char>(s.cStr()[i]);
            idx->text.pushBack(c < ' ' ? ' ' : static_cast<char>(c));
        }
        t.len = static_cast<za::U32>(s.size());
        idx->text.pushBack('\0');
        return t;
    };
    for(const Item& it : items)
    {
        mapindex::Entry e;
        const auto d = details.find(it.page);
        const Details* det = d != details.end() ? &d->second : nullptr;
        const za::String& needs = det && det->needs.size() ? det->needs : it.needs;
        za::String extract, theme;
        requirementFields(needs, extract, theme);
        e.source = mapindex::Source::Slipseer;
        e.sha256 = add(idFor(it.page));
        e.title = add(it.title);
        e.author = add(det && det->author.size() ? det->author : it.author);
        e.date = add(det && det->date.size() ? det->date : it.date);
        e.types = add(za::String{"map"});
        e.modes = add(za::String{it.category == "multiplayer" ? "deathmatch" : "singleplayer"});
        e.extract = add(extract);
        e.themes = add(theme);
        e.page = add(it.page);
        e.needs = add(needs);
        if(det)
        {
            e.description = add(det->description.size() ? det->description : it.tagline);
            e.urls = add(det->download);
            e.shot = add(det->screenshot.size() ? det->screenshot : it.thumb);
        }
        else
        {
            e.description = add(it.tagline);
            e.shot = add(it.thumb);
        }
        const int stars = det && det->rating ? det->rating : it.rating;
        e.userRating = static_cast<za::U16>(za::min(za::max(stars, 0), 500));
        idx->entries.pushBack(e);
    }
    return idx;
}

void publishPart()
{
    auto part = buildPart();
    za::LockGuard lock{handoff};
    pendingPart = ZA_MOVE(part);
}

// A map page's details (cache or network), into `details`. False: none.
bool readDetails(const za::String& page, bool network, const char* referer, bool* fresh = nullptr)
{
    za::Vector<char> body;
    za::String error;
    {
        za::LockGuard lock{handoff};
        workingOn = page;
    }
    const bool got = fetch(page, Kind::Page, detailSeconds, referer, network, body, error, fresh);
    {
        za::LockGuard lock{handoff};
        workingOn = za::String{};
    }
    if(!got)
    {
        if(network)
        {
            note("slipseer: %s: %s", page.cStr(), error.cStr());
        }
        return false;
    }
    body.pushBack('\0');
    Details d;
    if(!parseDetails(body.data(), page.cStr(), d))
    {
        note("slipseer: %s does not read as a map's page (%d bytes)", page.cStr(), static_cast<int>(body.size()) - 1);
        return false;
    }
    details[page] = ZA_MOVE(d);
    return true;
}

// The listing pages, first to last (each from the cache while fresh). Stops at the first failure.
void listingPass(bool network)
{
    za::Vector<Item> found;
    za::String url = listingUrl();
    za::String referer = za::String{siteRoot} + "/";
    int pages = 0, fetched = 0;
    for(; url.size() && pages < maxListingPages; pages++)
    {
        if(SDL_AtomicGet(&cancel))
        {
            return;
        }
        za::Vector<char> body;
        za::String error;
        bool fresh = false;
        {
            za::LockGuard lock{handoff};
            workingOn = url;
        }
        // The first page (where new and updated maps come) revalidated after firstPageSeconds, the rest after
        // listingSeconds.
        char doing[96];
        q_snprintf(doing, sizeof(doing), "Slipseer's list of maps: page %d (%d maps so far)", pages + 1,
            static_cast<int>(found.size()));
        const ActivityScope activity{za::String{doing}};
        const bool got = fetch(url, Kind::Page, pages == 0 ? firstPageSeconds : listingSeconds, referer.cStr(), network,
            body, error, &fresh);
        {
            za::LockGuard lock{handoff};
            workingOn = za::String{};
        }
        if(!got)
        {
            note("slipseer: listing page %d: %s (%s)", pages + 1, error.cStr(), url.cStr());
            break;
        }
        fetched += fresh ? 1 : 0;
        body.pushBack('\0');
        za::String next;
        const za::SizeT before = found.size();
        parseListing(body.data(), url.cStr(), found, next);
        if(fresh)
        {
            note("slipseer: listing page %d: %d maps", pages + 1, static_cast<int>(found.size() - before));
        }
        if(found.size() == before)
        {
            break;
        }
        referer = url;
        url = next;
    }
    // Every map listed once (a map can move between pages while they are read).
    za::Vector<Item> unique;
    ankerl::unordered_dense::map<za::String, bool> seen;
    for(Item& it : found)
    {
        if(seen.find(it.page) == seen.end())
        {
            seen[it.page] = true;
            unique.pushBack(ZA_MOVE(it));
        }
    }
    if(!unique.empty() || items.empty())
    {
        items = ZA_MOVE(unique);
    }
    // The details already in the cache (no network): a map picked once keeps its description and link.
    for(const Item& it : items)
    {
        if(details.find(it.page) == details.end())
        {
            (void)readDetails(it.page, false, nullptr);
        }
    }
    itemsRead = true;
    publishPart();
    char status[200];
    q_snprintf(status, sizeof(status), "slipseer: %d maps from %d listing pages (%d fetched now, the rest cached)",
        static_cast<int>(items.size()), pages, fetched);
    za::LockGuard lock{handoff};
    pendingLog.pushBack(za::String{status});
    progress = za::String{status};
}

// A map's screenshot, decoded for the main thread. Its size in bytes (-1: none).
// A downloaded picture's size read from its header before it is decoded: a small file can claim a huge picture
// (gigabytes decoded); anything past 8192 a side or 32 Mpixels is refused.
[[nodiscard]] bool pictureSizeOk(const char* bytes, za::SizeT size, int& w, int& h)
{
    w = h = 0;
    if(!Image_InfoMemory(reinterpret_cast<const unsigned char*>(bytes), static_cast<int>(size), &w, &h))
    {
        return false;
    }
    return w > 0 && h > 0 && w <= 8192 && h <= 8192 && static_cast<za::I64>(w) * h <= za::I64{32} * 1024 * 1024;
}

int readPicture(const za::String& key, const za::String& url, const char* referer, bool network, bool* fresh = nullptr)
{
    if(url.empty())
    {
        return -1;
    }
    const za::String& page = key;
    za::Vector<char> body;
    za::String error;
    if(!fetch(url, Kind::Asset, assetSeconds, referer, network, body, error, fresh) || body.empty() ||
        body.size() > static_cast<za::SizeT>(maxScreenshotBytes))
    {
        if(network && error.size())
        {
            note("slipseer: screenshot %s: %s", url.cStr(), error.cStr());
        }
        return -1;
    }
    int w = 0, h = 0;
    if(!pictureSizeOk(body.data(), body.size(), w, h))
    {
        note("slipseer: screenshot %s: %dx%d, not shown", url.cStr(), w, h);
        return -1;
    }
    unsigned char* rgba = Image_DecodeMemory(reinterpret_cast<const unsigned char*>(body.data()),
        static_cast<int>(body.size()), &w, &h);
    if(!rgba)
    {
        return -1;
    }
    za::LockGuard lock{handoff};
    if(pendingPicture.rgba)
    {
        VR_HeapFree(pendingPicture.rgba);
    }
    pendingPicture.page = page;
    pendingPicture.rgba = rgba;
    pendingPicture.width = w;
    pendingPicture.height = h;
    return static_cast<int>(body.size());
}

// A Slipseer map's screenshot: from its page's details, else its listing's icon. Its size in bytes (-1: none).
int readScreenshot(const za::String& page, bool network, bool* fresh = nullptr)
{
    const auto d = details.find(page);
    za::String url = d != details.end() ? d->second.screenshot : za::String{};
    if(url.empty())
    {
        for(const Item& it : items)
        {
            if(it.page == page)
            {
                url = it.thumb;
            }
        }
    }
    return readPicture(page, url, page.cStr(), network, fresh);
}

// The player's pick: a Slipseer map's page and screenshot, or a Quaddicted map's screenshot alone; how long each took
// since the pick (`at`, SDL ticks) goes to the console ("slipseer: picked ...": maps_slipseer_time reads it).
void servePick(const za::String& page, bool pageNet, const za::String& shotKey, const za::String& shotUrl, bool shotNet,
    za::U32 at, const za::String& title)
{
    const PaceScope now{Pace::Now};
    char line[400];
    const char* name = title.size() ? title.cStr() : "the map";
    if(page.size())
    {
        if(!itemsRead)
        {
            listingPass(false); // (the items from the cache, to build the entry)
        }
        bool pageFresh = false, shotFresh = false;
        bool got = false;
        {
            const ActivityScope activity{za::String{"Slipseer: reading "} + name + "'s page"};
            got = readDetails(page, pageNet, listingUrl().cStr(), &pageFresh);
        }
        if(got)
        {
            publishPart();
        }
        const za::U32 pageMs = SDL_GetTicks() - at;
        const ActivityScope activity{za::String{"Slipseer: "} + name + "'s screenshot"};
        const int bytes = readScreenshot(page, pageNet, &shotFresh);
        char took[64]; // (not va(): its buffers are the main thread's too)
        q_snprintf(took, sizeof(took), bytes < 0 ? "none" : "%d KB, %s", bytes / 1024, shotFresh ? "fetched" : "cached");
        q_snprintf(line, sizeof(line), "slipseer: picked %s: page %u ms (%s), screenshot %u ms (%s)", page.cStr(), pageMs,
            got ? (pageFresh ? "fetched" : "cached") : "none", SDL_GetTicks() - at, took);
    }
    else
    {
        bool shotFresh = false;
        const ActivityScope activity{za::String{"Quaddicted: "} + name + "'s screenshot"};
        const int bytes = readPicture(shotKey, shotUrl, nullptr, shotNet, &shotFresh);
        char took[64];
        q_snprintf(took, sizeof(took), bytes < 0 ? "none" : "%d KB, %s", bytes / 1024, shotFresh ? "fetched" : "cached");
        q_snprintf(line, sizeof(line), "slipseer: picked %s: screenshot %u ms (%s)", shotUrl.cStr(), SDL_GetTicks() - at, took);
    }
    {
        za::LockGuard lock{handoff};
        pendingLog.pushBack(za::String{line});
    }
    served.fetchAddSeqCst(1);
}

bool inPick = false; // (the thread's: servePendingPick runs one at a time, never inside itself)

bool servePendingPick()
{
    if(inPick)
    {
        return false;
    }
    za::String page, shotKey, shotUrl, title;
    bool pageNet = false, shotNet = false;
    za::U32 at = 0;
    {
        za::LockGuard lock{handoff};
        if(wantedPage.empty() && wantedShotKey.empty())
        {
            return false;
        }
        page = ZA_MOVE(wantedPage);
        wantedPage = za::String{};
        pageNet = wantedNetwork;
        shotKey = ZA_MOVE(wantedShotKey);
        shotUrl = ZA_MOVE(wantedShotUrl);
        wantedShotKey = za::String{};
        wantedShotUrl = za::String{};
        shotNet = wantedShotNetwork;
        at = wantedAt;
        title = wantedTitle;
    }
    inPick = true;
    servePick(page, pageNet, shotKey, shotUrl, shotNet, at, title);
    inPick = false;
    return true;
}

void run() noexcept
{
    struct Done
    {
        ~Done() { running.storeSeqCst(false); }
    } done;
    for(;;)
    {
        if(SDL_AtomicGet(&cancel))
        {
            return;
        }
        if(servePendingPick()) // (the player waits for it: before anything else)
        {
            continue;
        }
        bool pass = false, passNet = false, asked = false, asset = false;
        za::String get;
        {
            za::LockGuard lock{handoff};
            if(getUrl.empty() && !passWanted && wantedPage.empty() && wantedShotKey.empty())
            {
                // Nothing left: stop here, under the lock, so a request handed over after this sees running false
                // and starts a new worker (ensureThread); one handed over before it is served by this one.
                running.storeSeqCst(false);
                return;
            }
            get = ZA_MOVE(getUrl);
            getUrl = za::String{};
            asset = getAsset;
            pass = passWanted;
            passNet = passNetwork;
            asked = passAsked;
            passWanted = false;
        }
        if(get.size())
        {
            const PaceScope paced{Pace::Auto};
            const ActivityScope activity{za::String{"Slipseer: "} + get};
            za::Vector<char> body;
            za::String error;
            bool fresh = false;
            const bool ok =
                fetch(get, asset ? Kind::Asset : Kind::Page, asset ? assetSeconds : detailSeconds, nullptr, true, body, error, &fresh);
            note("slipseer: %s: %s (%d bytes, %s) -> %s", get.cStr(), ok ? "ok" : error.cStr(),
                static_cast<int>(body.size()), fresh ? "fetched" : "cached", cachePath(get, asset).cStr());
            continue;
        }
        if(pass)
        {
            const PaceScope paced{asked ? Pace::Auto : Pace::Background};
            listingPass(passNet);
            continue;
        }
        // (a pick handed over since servePendingPick looked: served next time round; else the check above stops)
    }
}

void ensureThread()
{
    if(running.loadSeqCst())
    {
        return;
    }
    if(worker.joinable())
    {
        worker.join();
    }
    SDL_AtomicSet(&cancel, 0);
    // (the agent picked and the back-off read here, before the worker starts: the main thread's downloads and status
    // then only read them, never set them as the worker does)
    (void)agent();
    readBackoff();
    running.storeSeqCst(true);
    worker = za::Thread(run);
}

[[nodiscard]] bool networkOk(bool asked)
{
    return asked || (vr_maps_fetch.value && !getenv("QVR_TEST_BACKGROUND"));
}

} // namespace

namespace
{
void testStep(); // (maps_slipseer_test's install, each frame: below)
void timeStep(); // (maps_slipseer_time's next pick, each frame: below)
} // namespace

// ---------------------------------------------------------------- the API

void start(bool asked)
{
    if(!vr_maps_slipseer.value || (!asked && COM_CheckParm("-nomapindex")))
    {
        return;
    }
    {
        za::LockGuard lock{handoff};
        passWanted = true;
        passNetwork = networkOk(asked);
        passAsked = asked;
    }
    ensureThread();
}

void finish()
{
    if(!worker.joinable())
    {
        return;
    }
    SDL_AtomicSet(&cancel, 1);
    const za::U32 t0 = SDL_GetTicks();
    while(running.loadSeqCst() && SDL_GetTicks() - t0 < 3000)
    {
        SDL_Delay(10);
    }
    if(running.loadSeqCst())
    {
        Sys_Printf("slipseer: the fetch did not stop in 3 s; quitting without it\n");
        Download_KeepGlobalState();
        worker.detach();
    }
    else
    {
        worker.join();
    }
    running.storeSeqCst(false);
}

void poll()
{
    za::Vector<za::String> log;
    za::UniquePtr<mapindex::Index> part;
    Picture picture;
    {
        za::LockGuard lock{handoff};
        log = ZA_MOVE(pendingLog);
        pendingLog = za::Vector<za::String>{};
        part = ZA_MOVE(pendingPart);
        picture = pendingPicture;
        pendingPicture = Picture{};
    }
    for(const za::String& line : log)
    {
        Con_SafePrintf("%s\n", line.cStr());
        lastStatus = line;
    }
    if(part)
    {
        detailedPages.clear();
        for(const mapindex::Entry& e : part->entries)
        {
            if(e.urls.len)
            {
                detailedPages[za::String{part->field(e.page)}] = true;
            }
        }
        mapindex::setSlipseer(ZA_MOVE(*part));
    }
    testStep();
    timeStep();
    if(picture.rgba)
    {
        // The texels kept while the picture is drawn (a video restart reads them again).
        shownTexels.clear();
        shownTexels.resize(static_cast<za::SizeT>(picture.width) * static_cast<za::SizeT>(picture.height));
        memcpy(shownTexels.data(), picture.rgba, shownTexels.size() * 4);
        VR_HeapFree(picture.rgba);
        if(shownPic.empty())
        {
            shownPic.resize(Draw_PicBytes(), 0);
        }
        Draw_ReplacePicRGBA(reinterpret_cast<qpic_t*>(shownPic.data()), "slipseer:screenshot", picture.width,
            picture.height, picture.width, picture.height, shownTexels.data(), true);
        shownPage = picture.page;
        shownWidth = picture.width;
        shownHeight = picture.height;
    }
}

namespace
{
// A pick's title for the status line: the index entry whose page (or sha256, `bySha`) is `key`. Main thread.
[[nodiscard]] za::String titleOf(const za::String& key, bool bySha)
{
    const mapindex::Index& idx = mapindex::index();
    for(const mapindex::Entry& e : idx.entries)
    {
        const mapindex::Text& t = bySha ? e.sha256 : e.page;
        if(idx.length(t) == key.size() && !memcmp(idx.field(t), key.cStr(), key.size()))
        {
            return za::String{idx.field(e.title)};
        }
    }
    return za::String{};
}
} // namespace

za::String activity()
{
    za::String what, error;
    za::U32 errorAt = 0;
    {
        za::LockGuard lock{activityLock};
        what = activityText;
        error = lastError;
        errorAt = lastErrorAt;
    }
    char line[256];
    if(what.size())
    {
        const int bytes = activityBytes.loadSeqCst(), total = activityTotal.loadSeqCst();
        const int waitUntil = activityWaitUntil.loadSeqCst();
        const int waitMs = waitUntil - static_cast<int>(SDL_GetTicks());
        if(waitUntil && waitMs > 0)
        {
            q_snprintf(line, sizeof(line), "%s: in %d s (the site's pace)", what.cStr(), (waitMs + 999) / 1000);
        }
        else if(total > 0)
        {
            q_snprintf(line, sizeof(line), "%s: %d%% (%s of %s)", what.cStr(),
                static_cast<int>(za::min(static_cast<long long>(bytes) * 100ll / total, 100ll)),
                mapinstall::formatBytes(static_cast<za::U64>(bytes)).cStr(),
                mapinstall::formatBytes(static_cast<za::U64>(total)).cStr());
        }
        else if(bytes > 0)
        {
            q_snprintf(line, sizeof(line), "%s: %s", what.cStr(), mapinstall::formatBytes(static_cast<za::U64>(bytes)).cStr());
        }
        else
        {
            q_snprintf(line, sizeof(line), "%s...", what.cStr());
        }
        return za::String{line};
    }
    if(error.size() && SDL_GetTicks() - errorAt < 10000)
    {
        q_snprintf(line, sizeof(line), "Fetch failed: %s", error.cStr());
        return za::String{line};
    }
    return za::String{};
}

void want(const za::String& page)
{
    if(page.empty() || !vr_maps_slipseer.value)
    {
        return;
    }
    {
        za::LockGuard lock{handoff};
        wantedPage = page; // (only the latest: a map picked and left before its turn is never fetched)
        wantedNetwork = networkOk(false);
        wantedAt = SDL_GetTicks();
        wantedTitle = titleOf(page, false);
    }
    ensureThread();
}

void wantShot(const za::String& key, const za::String& url)
{
    if(key.empty() || url.empty())
    {
        return;
    }
    {
        za::LockGuard lock{handoff};
        wantedShotKey = key; // (only the latest, as want())
        wantedShotUrl = url;
        wantedShotNetwork = networkOk(false);
        wantedAt = SDL_GetTicks();
        wantedTitle = titleOf(key, true);
    }
    ensureThread();
}

bool detailed(const za::String& page)
{
    return detailedPages.find(page) != detailedPages.end();
}

bool fetching(const za::String& page)
{
    za::LockGuard lock{handoff};
    return workingOn == page || wantedPage == page || wantedShotKey == page;
}

za::String state()
{
    za::LockGuard lock{handoff};
    if(!vr_maps_slipseer.value)
    {
        return za::String{"slipseer: off (vr_maps_slipseer 0)"};
    }
    return progress.size() ? progress : za::String{"slipseer: not started"};
}

bool screenshotSize(const za::String& page, int& width, int& height)
{
    if(shownPage != page || shownPic.empty())
    {
        return false;
    }
    width = shownWidth;
    height = shownHeight;
    return true;
}

void drawScreenshot(float x, float y, float w, float h)
{
    if(shownPic.empty())
    {
        return;
    }
    // True colours: the menu hue tint (vr_menu_recolor) is for the menu's browns, not for a map's picture.
    const qboolean recolor = Draw_SetMenuRecolor(false);
    Draw_SubPic(x, y, w, h, reinterpret_cast<qpic_t*>(shownPic.data()), 0.f, 0.f, 1.f, 1.f, nullptr, 1.f);
    Draw_SetMenuRecolor(recolor);
}

void downloadHeaders(const char* referer, za::Vector<za::String>& out)
{
    const Agent& a = agent();
    out.pushBack(za::String{"User-Agent: "} + a.userAgent);
    out.pushBack(za::String{"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"});
    out.pushBack(za::String{"Accept-Language: "} + a.acceptLanguage);
    if(referer && *referer)
    {
        out.pushBack(za::String{"Referer: "} + referer);
    }
}

int requestsMade()
{
    return requests.loadSeqCst();
}

namespace
{

void slipseer_f()
{
    readBackoff();
    Con_Printf("%s\n", state().cStr());
    Con_Printf("slipseer: %d live requests this session; agent %d; cache %s", requestsMade(), agentIndex,
        cacheDir().cStr());
    if(backoffUntil > now())
    {
        Con_Printf("; backing off for %lld s more", static_cast<long long>(backoffUntil - now()));
    }
    Con_Printf("\n");
}

void get_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("maps_slipseer_get <url> [asset]: one Slipseer page (or picture) fetched politely into the cache "
                   "(development)\n");
        return;
    }
    {
        za::LockGuard lock{handoff};
        getUrl = za::String{Cmd_Argv(1)};
        getAsset = Cmd_Argc() >= 3 && !q_strcasecmp(Cmd_Argv(2), "asset");
    }
    ensureThread();
}

// maps_slipseer_time: picks made one after another as the player makes them, with the network (asked: a development
// command), each timed by servePick; the next goes when the last is served. Main thread.
struct TimeRun
{
    za::Vector<za::String> picks; // a Slipseer page's URL, or "shot:<url>" (a Quaddicted screenshot)
    int next{0};
    int target{-1}; // `served` once the issued pick is done (-1: none issued)
    za::U32 started{0};
};
TimeRun timeRun;

void time_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("maps_slipseer_time <page URL | shot:<picture URL> | pass>...: each picked in turn, live, and timed "
                   "(development: real requests, at the site's pace; pass starts the listing pass alongside)\n");
        return;
    }
    timeRun = TimeRun{};
    for(int i = 1; i < Cmd_Argc(); i++)
    {
        timeRun.picks.pushBack(za::String{Cmd_Argv(i)});
    }
    timeRun.started = SDL_GetTicks();
}

void timeStep()
{
    if(timeRun.picks.empty() || (timeRun.target >= 0 && served.loadSeqCst() < timeRun.target))
    {
        return;
    }
    if(timeRun.next >= static_cast<int>(timeRun.picks.size()))
    {
        Con_Printf("slipseer_time: done, %d picks in %u ms\n", static_cast<int>(timeRun.picks.size()),
            SDL_GetTicks() - timeRun.started);
        timeRun = TimeRun{};
        return;
    }
    const za::String& pick = timeRun.picks[static_cast<za::SizeT>(timeRun.next++)];
    if(pick == "pass")
    {
        start(true); // (the listing pass, live, alongside the picks that follow: they must not wait for it)
        timeRun.target = -1;
        return;
    }
    timeRun.target = served.loadSeqCst() + 1;
    {
        za::LockGuard lock{handoff};
        if(!strncmp(pick.cStr(), "shot:", 5))
        {
            wantedShotUrl = za::String{pick.cStr() + 5};
            wantedShotKey = wantedShotUrl;
            wantedShotNetwork = true;
        }
        else
        {
            wantedPage = pick;
            wantedNetwork = true;
        }
        wantedAt = SDL_GetTicks();
    }
    ensureThread();
}

} // namespace

// ---------------------------------------------------------------- maps_slipseer_test

namespace
{

// The test's state: the parsing and the filters are checked at once; the install (when a zip's URL is given) runs over
// frames (poll() steps it) until the job ends.
struct SelfTest
{
    testkit::Tally tally;
    int stage{0}; // 0 none; 1 the install running; 2 done
    int frames{0};
    za::String id;
};
SelfTest selfTest;

void expect(bool ok, const char* what, const za::String& got = za::String{})
{
    const za::String line = za::String{what} + (got.size() ? za::String{" - got "} + got : za::String{});
    if(selfTest.tally.check(ok, "slipseer_test", line.cStr()))
    {
        Con_Printf("slipseer_test: PASS %s\n", line.cStr());
    }
}

void verdict()
{
    selfTest.tally.verdict("slipseer_test");
    selfTest.stage = 2;
}

void test_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("maps_slipseer_test <fixture dir> [zip url]: the Slipseer parser on saved pages, the source filter and "
                   "tag, and (with a zip's URL) a Slipseer map installed through the Map Library's path and removed\n");
        return;
    }
    finish(); // (the start-up's cache-only pass joined: the test owns the items now)
    selfTest = SelfTest{};
    const za::String dir{Cmd_Argv(1)};
    const auto load = [&](const char* name, za::String& text) {
        const bool ok = files::readText((dir + "/" + name).cStr(), text);
        expect(ok, name);
        return ok;
    };

    // The listing page.
    za::String listing, drain, violacea;
    za::Vector<Item> found;
    za::String next;
    if(load("listing.html", listing))
    {
        parseListing(listing.cStr(), "https://www.slipseer.com/index.php?resources/categories/maps.1/", found, next);
    }
    expect(found.size() == 3, "listing: 3 maps", za::toString(static_cast<int>(found.size())));
    expect(next == "https://www.slipseer.com/index.php?resources/categories/maps.1/&page=2", "listing: next page", next);
    const Item* d = nullptr;
    const Item* v = nullptr;
    const Item* c = nullptr;
    for(const Item& it : found)
    {
        d = it.title == "Drain" ? &it : d;
        v = it.title == "Violacea" ? &it : v;
        c = it.title == "Caliber" ? &it : c;
    }
    expect(d && v && c, "listing: Drain, Violacea, Caliber");
    if(d && v && c)
    {
        expect(d->page == "https://www.slipseer.com/index.php?resources/drain.672/", "Drain: page", d->page);
        expect(d->author == "Snarg", "Drain: author", d->author);
        expect(d->date == "2026-11-11", "Drain: date", d->date);
        expect(d->category == "vanilla" && d->needs.empty(), "Drain: vanilla, needs nothing", d->category);
        expect(d->rating == 400, "Drain: rated 4.00", za::toString(d->rating));
        expect(strstr(d->thumb.cStr(), "resource_icons/0/672.jpg") != nullptr, "Drain: icon", d->thumb);
        expect(d->tagline == "short vanilla style base map", "Drain: tag line", d->tagline);
        expect(v->needs == "Arcane Dimensions", "Violacea: needs Arcane Dimensions", v->needs);
        expect(c->needs == "Copper", "Caliber: needs Copper", c->needs);
    }

    // A picture's size checked from its header (a PNG's signature and IHDR: width, height, 8-bit RGBA).
    {
        char png[] = {'\x89', 'P', 'N', 'G', '\r', '\n', '\x1a', '\n', 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 0, 0, 0, 0,
            0, 0, 8, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'I', 'D', 'A', 'T'}; // (an IDAT next: stb reads on for a tRNS)
        const auto sized = [&png](za::U32 w, za::U32 h) {
            for(int b = 0; b < 4; b++)
            {
                png[16 + b] = static_cast<char>((w >> (24 - 8 * b)) & 0xFF);
                png[20 + b] = static_cast<char>((h >> (24 - 8 * b)) & 0xFF);
            }
            int pw = 0, ph = 0;
            return pictureSizeOk(png, sizeof(png), pw, ph) && pw == static_cast<int>(w) && ph == static_cast<int>(h);
        };
        expect(sized(640, 360), "picture: a 640x360 PNG header is read and kept");
        expect(!sized(6000, 6000), "picture: a 6000x6000 PNG header (36 Mpixels) is refused before decoding");
        expect(!sized(9000, 100), "picture: a 9000-wide PNG header is refused before decoding");
        int nw = 0, nh = 0;
        expect(!pictureSizeOk("not a picture", 13, nw, nh), "picture: a web page is no picture");
    }

    // The map pages.
    Details dd, vd;
    expect(load("drain.html", drain) && parseDetails(drain.cStr(), "https://www.slipseer.com/index.php?resources/drain.672/", dd),
        "drain.html reads as a map's page");
    expect(dd.download == "https://www.slipseer.com/index.php?resources/drain.672/download", "Drain: download link", dd.download);
    expect(dd.author == "Snarg" && dd.date == "2026-11-11", "Drain: author and date", dd.author + " " + dd.date);
    expect(strings::startsWith(dd.description.cStr(), "Drain is a fairly short vanilla style base map"), "Drain: description");
    expect(strstr(dd.description.cStr(), "that's your thing") != nullptr, "Drain: entities decoded");
    expect(strstr(dd.screenshot.cStr(), "resource_icons/0/672.jpg") != nullptr, "Drain: picture (its icon: none other)",
        dd.screenshot);
    expect(dd.rating == 400, "Drain: rating", za::toString(dd.rating));
    expect(load("violacea.html", violacea) &&
               parseDetails(violacea.cStr(), "https://www.slipseer.com/index.php?resources/violacea.655/", vd),
        "violacea.html reads as a map's page");
    expect(strstr(vd.screenshot.cStr(), "attachments/v4-png.8539") != nullptr, "Violacea: screenshot (og:image)",
        vd.screenshot);
    expect(vd.needs == "Arcane Dimensions", "Violacea: needs Arcane Dimensions (breadcrumbs)", vd.needs);

    // Quaddicted's screenshots: the URL from the id, and its robots.txt (as measured on the site).
    const char* qsha = "582bfaa3049c18416a276ee40597c22e91a3362be12d585b9491d1c1d90257de";
    expect(mapindex::quaddictedShotUrl(za::StringView{qsha}, za::String{"ne_ruins.zip"}) ==
               "https://www.quaddicted.com/files/quaddicted-images/by-sha256/58/582bfaa3049c18416a276ee40597c22e91a3362be12d585b9491d1c1d90257de/ne_ruins.jpg",
        "Quaddicted shot: URL from sha and zip name");
    expect(mapindex::quaddictedShotUrl(za::StringView{qsha}, za::String{"a/../b.zip"}).empty() &&
               mapindex::quaddictedShotUrl(za::StringView{qsha}, za::String{"x.txt"}).empty() &&
               mapindex::quaddictedShotUrl(za::StringView{"12"}, za::String{"x.zip"}).empty(),
        "Quaddicted shot: unsafe id, no zip, bad sha: none");
    // A cached index keeps no filename tag: the shot comes from the download URLs' zip name (Quaddicted's own first).
    const za::String mirror = za::String{"https://x901.net/quaddictedfiles/mirror_name.zip"} + mapindex::partSep +
                              "https://www.quaddicted.com/files/by-sha256/58/" + qsha + "/ne_ruins.zip";
    expect(mapindex::zipNameFromUrls(za::StringView{mirror}) == "ne_ruins.zip" &&
               mapindex::zipNameFromUrls(za::StringView{"https://x901.net/q/abc.zip"}) == "abc.zip" &&
               mapindex::zipNameFromUrls(za::StringView{"https://example.com/page.html"}).empty(),
        "Quaddicted shot: zip name from the URLs (Quaddicted's, else the first zip, else none)");
    {
        // One cached package line (17 fields, as cacheText writes them, before the shot existed): it shows its shot.
        za::String line{qsha};
        const char* fields[16] = {"The Altar of Storms", "necros", "2001-01-01", "map", "single", "", "", "1000", "start", "",
            "0", mirror.cStr(), "", "1", "0", "0"};
        for(const char* f : fields)
        {
            line += mapindex::fieldSep;
            line += f;
        }
        const za::String text = za::String{"#quakevr-mapindex-3\n#test:\n#1\n"} + line + "\n";
        mapindex::Index cached;
        const bool read = mapindex::parseCacheText(cached, text, za::String{"test:"});
        const za::String shot = read && cached.entries.size() == 1 ? za::String{cached.field(cached.entries[0].shot)} : za::String{};
        expect(shot == mapindex::quaddictedShotUrl(za::StringView{qsha}, za::String{"ne_ruins.zip"}),
            "Quaddicted shot: a cached index's package has its shot", shot);
    }
    expect(quaddictedSite(za::String{"https://www.quaddicted.com/files/x.jpg"}) && !sameSite(za::String{"https://www.quaddicted.com/x"}),
        "Quaddicted shot: its own site");
    za::Vector<za::String> dis;
    parseRobots("User-agent: sogou\nDisallow: /\n\nUser-agent: *\nDisallow: /stuff/\n\nUser-agent: *\nDisallow: /temp/\n"
                "Disallow: /reviews/random_map.php\n",
        dis);
    expect(dis.size() == 3 && dis[0] == "/stuff/", "Quaddicted robots.txt: the * groups' Disallow lines",
        za::toString(static_cast<int>(dis.size())));
    {
        // A map hosted elsewhere (Herzblut, Kadath: measured 2026-11-13): its Download is a "redirect" button.
        Details hd;
        const bool ok = parseDetails(
            "<script type=\"application/ld+json\">{\"mainEntity\":{\"headline\":\"Herzblut\"}}</script>"
            "<a href=\"/index.php?resources/herzblut.666/download\" class=\"button button--icon button--icon--redirect "
            "button--cta\" target=\"_blank\">",
            "https://www.slipseer.com/index.php?resources/herzblut.666/", hd);
        expect(ok && hd.download == "https://www.slipseer.com/index.php?resources/herzblut.666/download",
            "map page: an external host's redirect button is its download link", hd.download);
    }
    {
        za::Vector<za::String> d2;
        double crawl = 0.0;
        parseRobots("User-agent: bingbot\nCrawl-delay: 30\n\nUser-agent: *\nCrawl-delay: 4\nDisallow: /x/\n", d2, &crawl);
        expect(crawl == 4.0 && d2.size() == 1, "robots.txt: the * group's Crawl-delay (another agent's ignored)",
            za::toString(static_cast<int>(crawl)));
    }
    za::Vector<za::String> saved = robotsOf[1].disallowed;
    robotsOf[1].disallowed = dis;
    expect(robotsAllow(za::String{"https://www.quaddicted.com/files/quaddicted-images/by-sha256/58/x/y.jpg"}, 1) &&
               !robotsAllow(za::String{"https://www.quaddicted.com/stuff/a.jpg"}, 1),
        "Quaddicted robots.txt: a screenshot allowed, /stuff/ not");
    robotsOf[1].disallowed = saved;

    // Identity and requirements.
    expect(mapindex::validSha(za::StringView{idFor(za::String{"https://www.slipseer.com/index.php?resources/drain.672/"})}),
        "id: 64 hex digits");
    za::String extract, theme;
    requirementFields(za::String{"Arcane Dimensions"}, extract, theme);
    expect(extract == "{base}/ad/" && theme == "arcane_dimensions", "requirement: AD's folder and tag", extract);
    requirementFields(za::String{}, extract, theme);
    expect(extract == "{base}/id1/" && theme.empty(), "requirement: none is the stock game's", extract);

    // The entries in the shared index: the source filter, the tag.
    items = found;
    details.clear();
    if(d)
    {
        details[d->page] = dd;
    }
    if(v)
    {
        details[v->page] = vd;
    }
    const za::String zipUrl = Cmd_Argc() >= 3 ? za::String{Cmd_Argv(2)} : za::String{};
    if(zipUrl.size())
    {
        Item t;
        t.page = "https://www.slipseer.com/index.php?resources/qvr-install-test.1/";
        t.title = "Quake VR Install Test";
        t.author = "Quake VR";
        t.date = "2026-11-13";
        t.category = "vanilla";
        items.pushBack(t);
        Details td;
        td.download = zipUrl;
        td.description = "A test map installed from a local server.";
        details[t.page] = td;
        selfTest.id = idFor(t.page);
    }
    itemsRead = true;
    const int quaddicted = mapindex::countFrom(mapindex::Source::Quaddicted);
    mapindex::setSlipseer(ZA_MOVE(*buildPart()));
    const int slip = mapindex::countFrom(mapindex::Source::Slipseer);
    expect(slip == static_cast<int>(items.size()), "index: every Slipseer map in it", za::toString(slip));
    expect(mapindex::countFrom(mapindex::Source::Quaddicted) == quaddicted, "index: Quaddicted's kept",
        za::toString(quaddicted));
    za::Vector<const mapindex::Entry*> out;
    mapindex::Query q;
    q.allowProgs = true;
    mapindex::search(q, out);
    expect(static_cast<int>(out.size()) == quaddicted + slip, "filter All: both sources", za::toString(static_cast<int>(out.size())));
    q.source = static_cast<int>(mapindex::Source::Slipseer);
    mapindex::search(q, out);
    bool onlySlip = static_cast<int>(out.size()) == slip;
    for(const mapindex::Entry* e : out)
    {
        onlySlip = onlySlip && e->source == mapindex::Source::Slipseer;
    }
    expect(onlySlip, "filter Slipseer: only Slipseer's", za::toString(static_cast<int>(out.size())));
    q.source = static_cast<int>(mapindex::Source::Quaddicted);
    mapindex::search(q, out);
    bool onlyQuad = static_cast<int>(out.size()) == quaddicted;
    for(const mapindex::Entry* e : out)
    {
        onlyQuad = onlyQuad && e->source == mapindex::Source::Quaddicted;
    }
    expect(onlyQuad, "filter Quaddicted: only Quaddicted's", za::toString(static_cast<int>(out.size())));
    q.source = -1;
    q.text = "violacea";
    mapindex::search(q, out);
    expect(out.size() == 1 && out[0]->source == mapindex::Source::Slipseer &&
               za::String{mapindex::sourceName(out[0]->source)} == "Slipseer",
        "search finds Violacea, tagged Slipseer");
    if(out.size() == 1)
    {
        expect(mapinstall::madeFor(*out[0]) == "Arcane Dimensions", "Violacea: made for Arcane Dimensions",
            mapinstall::madeFor(*out[0]));
        expect(mapindex::index().length(out[0]->author) > 0, "Violacea: its author credited",
            za::String{mapindex::index().field(out[0]->author)});
    }
    expect(requestsMade() == 0, "no live request in the test", za::toString(requestsMade()));

    if(zipUrl.empty())
    {
        verdict();
        return;
    }
    // The install: through mapinstall::begin, as the page's Install.
    const mapindex::Entry* e = mapindex::find(selfTest.id);
    za::String why;
    const bool begun = e && mapinstall::begin(e, true, &why);
    expect(begun, "install: begun", why);
    if(!begun)
    {
        verdict();
        return;
    }
    selfTest.stage = 1;
}

// Each frame while the install runs (poll()).
void testStep()
{
    if(selfTest.stage != 1)
    {
        return;
    }
    selfTest.frames++;
    if(mapinstall::running())
    {
        if(selfTest.frames > 72 * 600)
        {
            expect(false, "install: timed out");
            verdict();
        }
        return;
    }
    const bool have = mapinstall::installed(selfTest.id);
    if(!have && mapinstall::job().phase != mapinstall::Phase::Failed && selfTest.frames < 72 * 600)
    {
        return; // (its record is taken by mapinstall::poll)
    }
    expect(have, "install: installed", mapinstall::job().message);
    za::String why;
    const za::String start = mapinstall::startMap(selfTest.id, &why);
    expect(start == "qvrslip", "install: its map found (Play starts it)", start.size() ? start : why);
    expect(mapinstall::uninstall(selfTest.id) && !mapinstall::installed(selfTest.id), "uninstall: removed");
    expect(requestsMade() == 0, "no live request (the zip came from the local server)", za::toString(requestsMade()));
    verdict();
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("maps_slipseer_test", test_f);
    Cmd_AddCommand("maps_slipseer", slipseer_f);
    Cmd_AddCommand("maps_slipseer_get", get_f);
    Cmd_AddCommand("maps_slipseer_time", time_f);
}

} // namespace qvr::slipseer
