// vr_update.cpp -- the in-game update notice's check: is there a newer Quake VR than this one?
//
// The source is the installer's own release feed (Installer: ReleaseFeed.cs; Misc/quakevr/make_release.py writes it):
// latest.json, from GitHub's latest release only (no mirror: the author's decision, 2026-10-08). Its
// "version" is the release's version text ("0.9.1 (2026-10-08 abcdef12)"), compared with the game's VERSION file
// (VR_Version) as semantic versions; its optional "page" is the release's page, opened by the notice (else GitHub's
// latest release page). docs/vr-port/RELEASING.md: game releases are marked Latest on GitHub, asset releases never.
//
// When: once at start-up (the first frame's end, as the map index: the config's vr_update_check read), on a thread of
// its own (the engine's Download: libcurl, its 20 s connect timeout, CURLOPT_QUICK_EXIT), and at most once an hour: an
// answer younger than that is read back from <base>/cache/update_check.txt instead (beside the map index's cache).
// vr_update_check 0: nothing is asked for and no notice shows. The kit's test runs (QVR_TEST_BACKGROUND) never check
// at start-up; vr_update_check_now does (the Debug menu's Check for Updates Now). Failures are silent: one line with
// `developer`, and vr_update_status says what happened.

#include "vr_update.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_zancle.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/ToString.hpp"

extern "C"
{
#include "json.h"
}

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace qvr::update
{
namespace
{

constexpr za::I64 cacheSeconds = 60 * 60;          // an answer younger than this is not asked for again
constexpr za::SizeT maxFeedBytes = 256 * 1024;     // latest.json is under 2 KB: anything this big is not it
constexpr char cacheMagic[] = "#quakevr-update-1";
constexpr const char* defaultFeeds[] = {
    "https://github.com/vittorioromeo/quakevr/releases/latest/download/latest.json", // (the Latest release's asset)
};
constexpr const char* defaultPage = "https://github.com/vittorioromeo/quakevr/releases/latest";
const char* acceptHeader = "Accept: application/json"; // (file scope: download_t.headers wants a const char**)

// What a check found (the thread's answer, and the main thread's copy of the last one).
struct Result
{
    bool ok{false};        // a feed answered with a version
    bool fromCache{false}; // read back from the cache file, nothing asked for
    za::String version;    // the feed's "version", whole ("0.9.1 (2026-10-08 abcdef12)")
    za::String page;       // the feed's "page" ("" without one)
    za::String source;     // the feed that answered
    za::I64 fetchedAt{0};  // when it answered (time())
    za::String error;      // why none did (every feed's reason)
    za::String cacheNote;  // the cache's state, for vr_update_status
};

// The handoff: the thread fills `pending`, poll() takes it on the main thread.
za::AtomicMutex handoff;
Result pending;
za::Atomic<bool> pendingReady{false};

za::Thread worker;
za::Atomic<bool> running{false};
SDL_atomic_t cancel{};
za::Vector<za::String> runFeeds; // the pass's feeds, read on the main thread as it starts (a cvar's string may be freed)
bool runUseCache = true;         // the pass takes a fresh cached answer (not vr_update_check_now's)

// Main thread.
Result last;                       // the last answer (ok: a version; else why not)
bool checked = false;              // `last` is a pass's (not the start-up's "not checked")
bool askedPass = false;            // the pass in flight was asked for (vr_update_check_now): its result is printed
za::String startNote{"not checked yet"}; // why the start-up check did not run, or that it did
Notice current;                    // what notice() returns while `hasNotice`
bool hasNotice = false;

[[nodiscard]] za::String cachePath()
{
    return za::String{com_basedirs[com_numbasedirs - 1]} + "/cache/update_check.txt";
}

// vr_update_url's feeds (separated by ';' or spaces), else GitHub's.
void feedsNow(za::Vector<za::String>& out)
{
    out.clear();
    const char* s = vr_update_url.string ? vr_update_url.string : "";
    while(*s)
    {
        while(*s == ';' || *s == ' ')
        {
            s++;
        }
        const char* e = s;
        while(*e && *e != ';' && *e != ' ')
        {
            e++;
        }
        if(e > s)
        {
            out.pushBack(za::String{s, static_cast<za::SizeT>(e - s)});
        }
        s = e;
    }
    if(out.empty())
    {
        for(const char* f : defaultFeeds)
        {
            out.pushBack(za::String{f});
        }
    }
}

[[nodiscard]] za::String joined(const za::Vector<za::String>& feeds)
{
    za::String s;
    for(const za::String& f : feeds)
    {
        if(!s.empty())
        {
            s += ';';
        }
        s += f;
    }
    return s;
}

// ---------------------------------------------------------------- semantic versions

struct SemVer
{
    za::U64 core[3]{};
    const char* pre{nullptr}; // the prerelease's identifiers ("beta.1"), null without; ends at preEnd
    const char* preEnd{nullptr};
};

[[nodiscard]] bool isIdentChar(char c)
{
    return isalnum(static_cast<unsigned char>(c)) || c == '-';
}

// MAJOR.MINOR.PATCH[-prerelease][+build], a "v" before it and anything after a space allowed.
[[nodiscard]] bool parseSemVer(const char* s, SemVer& out)
{
    while(*s == ' ')
    {
        s++;
    }
    if(*s == 'v' || *s == 'V')
    {
        s++;
    }
    for(int i = 0; i < 3; i++)
    {
        if(!isdigit(static_cast<unsigned char>(*s)) || (s[0] == '0' && isdigit(static_cast<unsigned char>(s[1]))))
        {
            return false; // (no digits, or a leading zero)
        }
        za::U64 n = 0;
        int digits = 0;
        while(isdigit(static_cast<unsigned char>(*s)))
        {
            if(++digits > 18)
            {
                return false;
            }
            n = n * 10 + static_cast<za::U64>(*s - '0');
            s++;
        }
        out.core[i] = n;
        if(i < 2)
        {
            if(*s != '.')
            {
                return false;
            }
            s++;
        }
    }
    if(*s == '-')
    {
        s++;
        out.pre = s;
        // Dot-separated identifiers, none empty.
        bool empty = true;
        while(isIdentChar(*s) || *s == '.')
        {
            if(*s == '.')
            {
                if(empty)
                {
                    return false;
                }
                empty = true;
            }
            else
            {
                empty = false;
            }
            s++;
        }
        if(empty)
        {
            return false;
        }
        out.preEnd = s;
    }
    if(*s == '+')
    {
        s++;
        if(!isIdentChar(*s))
        {
            return false;
        }
        while(isIdentChar(*s) || *s == '.')
        {
            s++;
        }
    }
    return *s == '\0' || *s == ' ';
}

[[nodiscard]] bool allDigits(const char* a, const char* e)
{
    for(; a < e; a++)
    {
        if(!isdigit(static_cast<unsigned char>(*a)))
        {
            return false;
        }
    }
    return true;
}

// Prerelease identifiers in turn: numbers numerically, below words; words in ASCII order; a shorter list that matches
// a longer one's start is older (semver.org, rule 11).
[[nodiscard]] int comparePre(const SemVer& a, const SemVer& b)
{
    if(!a.pre || !b.pre)
    {
        return (a.pre ? -1 : 0) + (b.pre ? 1 : 0); // (a release is newer than its prereleases)
    }
    const char* x = a.pre;
    const char* y = b.pre;
    while(true)
    {
        const bool xEnd = x >= a.preEnd, yEnd = y >= b.preEnd;
        if(xEnd || yEnd)
        {
            return xEnd == yEnd ? 0 : (xEnd ? -1 : 1);
        }
        const char* xe = x;
        while(xe < a.preEnd && *xe != '.')
        {
            xe++;
        }
        const char* ye = y;
        while(ye < b.preEnd && *ye != '.')
        {
            ye++;
        }
        const bool xNum = allDigits(x, xe), yNum = allDigits(y, ye);
        int c = 0;
        if(xNum && yNum)
        {
            const za::SizeT xl = static_cast<za::SizeT>(xe - x), yl = static_cast<za::SizeT>(ye - y);
            c = xl != yl ? (xl < yl ? -1 : 1) : strncmp(x, y, xl); // (no leading zeros: longer is larger)
        }
        else if(xNum != yNum)
        {
            c = xNum ? -1 : 1;
        }
        else
        {
            const za::SizeT xl = static_cast<za::SizeT>(xe - x), yl = static_cast<za::SizeT>(ye - y);
            c = strncmp(x, y, xl < yl ? xl : yl);
            if(c == 0 && xl != yl)
            {
                c = xl < yl ? -1 : 1;
            }
        }
        if(c != 0)
        {
            return c < 0 ? -1 : 1;
        }
        x = xe < a.preEnd ? xe + 1 : xe;
        y = ye < b.preEnd ? ye + 1 : ye;
    }
}

// The version's own text: from the "v" (left out) to the first space.
[[nodiscard]] za::String versionToken(const char* s)
{
    while(*s == ' ')
    {
        s++;
    }
    if(*s == 'v' || *s == 'V')
    {
        s++;
    }
    const char* e = s;
    while(*e && *e != ' ')
    {
        e++;
    }
    return za::String{s, static_cast<za::SizeT>(e - s)};
}

// ---------------------------------------------------------------- the cache

// The cache file: the magic, the feeds asked ('#' before each), when, then the version, the page and the source.
void writeCache(const za::String& feeds, const Result& r)
{
    const za::String path = cachePath();
    files::createDirectories(za::String{files::parentPath(path)}.cStr());
    za::String text{cacheMagic};
    text += "\n#";
    text += feeds;
    text += "\n#";
    text += za::toString(static_cast<long long>(r.fetchedAt));
    text += '\n';
    text += r.version;
    text += '\n';
    text += r.page;
    text += '\n';
    text += r.source;
    text += '\n';
    if(!files::writeText(path.cStr(), text))
    {
        Con_DPrintf("update check: the cache could not be written (%s)\n", path.cStr());
    }
}

// The cached answer, if it is of these feeds; `age` its age in seconds (-1 without one).
[[nodiscard]] bool readCache(const za::String& feeds, Result& r, za::I64& age, za::String& note)
{
    age = -1;
    za::String text;
    if(!files::readText(cachePath().cStr(), text))
    {
        note = "none";
        return false;
    }
    za::String lines[6];
    int n = 0;
    files::forLines(text, [&](za::StringView l)
    {
        if(n < 6)
        {
            lines[n] = za::String{l};
        }
        n++;
    });
    if(n < 4 || lines[0] != za::String{cacheMagic} || lines[1].size() < 1)
    {
        note = "unreadable (left alone; the next answer replaces it)";
        return false;
    }
    if(za::String{lines[1].cStr() + 1} != feeds)
    {
        note = za::String{"of other feeds ("} + (lines[1].cStr() + 1) + ")";
        return false;
    }
    r.fetchedAt = static_cast<za::I64>(strtoll(lines[2].cStr() + 1, nullptr, 10));
    r.version = lines[3];
    r.page = n > 4 ? lines[4] : za::String{};
    r.source = n > 5 ? lines[5] : za::String{};
    r.ok = !r.version.empty();
    r.fromCache = true;
    age = static_cast<za::I64>(time(nullptr)) - r.fetchedAt;
    note = za::String{"from "} + za::toString(static_cast<long long>(age)) + " s ago";
    return r.ok;
}

// ---------------------------------------------------------------- the fetch

struct Body
{
    za::Vector<char> bytes;
    bool tooBig{false};
};

size_t writeChunk(void* buffer, size_t size, size_t nmemb, void* stream)
{
    Body& body = *static_cast<Body*>(stream);
    const za::SizeT n = size * nmemb;
    if(SDL_AtomicGet(&cancel) || body.bytes.size() + n > maxFeedBytes)
    {
        body.tooBig = !SDL_AtomicGet(&cancel);
        return 0; // (the transfer stops)
    }
    body.bytes.reserveMore(n);
    body.bytes.unsafeEmplaceBackRange(static_cast<const char*>(buffer), n);
    return nmemb;
}

char httpError[32]; // fetchFeed's "HTTP <code>" (the thread's own; one pass runs at a time)

// One feed: its version (and page). False: `error` says why.
[[nodiscard]] bool fetchFeed(const za::String& url, Result& r, za::String& error)
{
    Body body;
    download_t dl{};
    dl.headers = &acceptHeader;
    dl.num_headers = 1;
    dl.write_fn = writeChunk;
    dl.write_data = &body;
    dl.abort = &cancel;
    if(!Download(url.cStr(), &dl))
    {
        q_snprintf(httpError, sizeof(httpError), "HTTP %d", static_cast<int>(dl.response));
        error = body.tooBig ? za::String{"over 256 KiB: not a release feed"}
                            : za::String{dl.error ? dl.error : (dl.response ? httpError : "no response")};
        return false;
    }
    body.bytes.pushBack('\0');
    json_t* json = JSON_Parse(body.bytes.data());
    if(!json)
    {
        error = "not JSON";
        return false;
    }
    bool ok = false;
    const jsonentry_t* root = json->root;
    const double* schema = root && root->type == JSON_OBJECT ? JSON_FindNumber(root, "schema") : nullptr;
    const char* version = root && root->type == JSON_OBJECT ? JSON_FindString(root, "version") : nullptr;
    if(schema && *schema != 1.0)
    {
        error = za::String{"schema "} + za::toString(static_cast<int>(*schema)) + ", not 1";
    }
    else if(!version || !version[0] || strchr(version, '\n'))
    {
        error = "no version";
    }
    else
    {
        const char* page = JSON_FindString(root, "page");
        r.version = za::String{version};
        // Only a web page (never a file or a program to run), on one line.
        r.page = page && (!strncmp(page, "https://", 8) || !strncmp(page, "http://", 7)) && !strpbrk(page, "\r\n ")
                     ? za::String{page}
                     : za::String{};
        r.source = url;
        ok = true;
    }
    JSON_Free(json);
    return ok;
}

void run() noexcept
{
    struct Done
    {
        ~Done() { running.storeSeqCst(false); }
    } done;
    const za::Vector<za::String> feeds = runFeeds;
    const za::String key = joined(feeds);
    Result r;
    za::I64 age = -1;
    Result cached;
    const bool haveCache = readCache(key, cached, age, r.cacheNote);
    if(runUseCache && haveCache && age >= 0 && age < cacheSeconds)
    {
        cached.cacheNote = r.cacheNote + " (fresh: nothing asked for)";
        r = ZA_MOVE(cached);
    }
    else
    {
        if(haveCache)
        {
            r.cacheNote += runUseCache ? " (stale: asked again)" : " (asked again: vr_update_check_now)";
        }
        za::String errors;
        for(const za::String& url : feeds)
        {
            if(SDL_AtomicGet(&cancel))
            {
                errors += "cancelled (the game quit)";
                break;
            }
            za::String error;
            if(fetchFeed(url, r, error))
            {
                r.ok = true;
                r.fetchedAt = static_cast<za::I64>(time(nullptr));
                writeCache(key, r);
                r.cacheNote = za::String{"written ("} + cachePath() + ")";
                break;
            }
            if(!errors.empty())
            {
                errors += "; ";
            }
            errors += url + ": " + error;
        }
        if(!r.ok && haveCache)
        {
            // An older answer rather than none (of these feeds, however old).
            cached.cacheNote = r.cacheNote + "; used: no feed answered";
            r = ZA_MOVE(cached);
        }
        r.error = ZA_MOVE(errors);
    }
    za::LockGuard lock{handoff};
    pending = ZA_MOVE(r);
    pendingReady.storeSeqCst(true);
}

void startPass(bool useCache)
{
    if(running.loadSeqCst())
    {
        return;
    }
    if(worker.joinable())
    {
        worker.join(); // (a pass that finished)
    }
    SDL_AtomicSet(&cancel, 0);
    feedsNow(runFeeds);
    runUseCache = useCache;
    running.storeSeqCst(true);
    worker = za::Thread(run);
}

// ---------------------------------------------------------------- the notice

// What refreshNotice last worked from: it works again only when one changes (every frame otherwise: no allocation).
int lastGeneration = 0; // poll() took an answer this many times
int seenGeneration = -1;
float seenCheck = -1.f;
za::String seenTestVersion;

[[nodiscard]] const char* testVersion()
{
    return vr_update_test_version.string ? vr_update_test_version.string : "";
}

// `current` and `hasNotice` from the cvars and the last answer: vr_update_test_version's version, else the feed's.
void refreshNotice()
{
    const char* test = testVersion();
    if(seenGeneration == lastGeneration && seenCheck == vr_update_check.value && seenTestVersion == test)
    {
        return;
    }
    seenGeneration = lastGeneration;
    seenCheck = vr_update_check.value;
    seenTestVersion = za::String{test};
    hasNotice = false;
    const char* latest = test[0] ? test : last.ok ? last.version.cStr() : "";
    bool ok = false;
    if(!vr_update_check.value || !latest[0] || compareVersions(latest, VR_Version(), ok) <= 0 || !ok)
    {
        return;
    }
    current.version = versionToken(latest);
    current.page = za::String{last.page.empty() ? defaultPage : last.page.cStr()};
    hasNotice = true;
}

// ---------------------------------------------------------------- the commands

void status_f()
{
    Con_Printf("update check: %s; the start-up check: %s\n",
        vr_update_check.value ? "on (vr_update_check 1)" : "off (vr_update_check 0: no check, no notice)", startNote.cStr());
    Con_Printf("update check: this game %s (build %s)\n", VR_Version(), VR_BuildVersion());
    if(running.loadSeqCst())
    {
        Con_Printf("update check: checking now\n");
    }
    else if(!checked)
    {
        Con_Printf("update check: no answer yet\n");
    }
    else if(last.ok)
    {
        Con_Printf("update check: latest \"%s\" from %s (%s, %lld s ago)%s\n", last.version.cStr(), last.source.cStr(),
            last.fromCache ? "the cache" : "asked", static_cast<long long>(time(nullptr) - last.fetchedAt),
            last.page.empty() ? "" : va(", page %s", last.page.cStr()));
        if(!last.error.empty())
        {
            Con_Printf("update check: (the cached answer: no feed answered: %s)\n", last.error.cStr());
        }
    }
    else
    {
        Con_Printf("update check: failed (%s)\n", last.error.cStr());
    }
    if(testVersion()[0])
    {
        Con_Printf("update check: vr_update_test_version \"%s\" stands in for the feed's\n", testVersion());
    }
    za::Vector<za::String> feeds;
    feedsNow(feeds);
    Con_Printf("update check: feeds %s%s\n", joined(feeds).cStr(), vr_update_url.string[0] ? " (vr_update_url)" : "");
    Result cached;
    za::I64 age = -1;
    za::String note;
    (void)readCache(joined(feeds), cached, age, note);
    Con_Printf("update check: cache %s: %s%s\n", cachePath().cStr(), note.cStr(),
        age < 0 ? "" : (age < cacheSeconds ? " (fresh: the start-up check takes it)" : " (stale: the start-up check asks again)"));
    refreshNotice();
    if(hasNotice)
    {
        Con_Printf("update check: notice \"Update available: Quake VR: Unleashed %s\" -> %s\n", current.version.cStr(),
            current.page.cStr());
    }
    else
    {
        Con_Printf("update check: no notice\n");
    }
}

void checkNow_f()
{
    // "startup": the start-up check as it runs outside the tests (vr_update_check, the cache honoured); else asked
    // now, whatever vr_update_check and the cache say.
    const bool startup = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "startup");
    if(running.loadSeqCst())
    {
        Con_Printf("vr_update_check_now: a check is already under way\n");
        return;
    }
    if(startup && !vr_update_check.value)
    {
        startNote = "off (vr_update_check 0)";
        Con_Printf("vr_update_check_now startup: off (vr_update_check 0): nothing asked for\n");
        return;
    }
    askedPass = !startup; // (the start-up check's answer is silent but with developer, as at start-up)
    startPass(startup);
    if(startup)
    {
        startNote = "run (vr_update_check_now startup)";
    }
    Con_Printf("vr_update_check_now: checking %s%s\n", joined(runFeeds).cStr(),
        startup ? " (a fresh cached answer is taken)" : "");
}

// vr_update_compare <a> <b>: the comparison the notice makes (a test aid).
void compare_f()
{
    if(Cmd_Argc() != 3)
    {
        Con_Printf("vr_update_compare <a> <b>: whether version a is older, the same or newer than b (semver)\n");
        return;
    }
    bool ok = false;
    const int c = compareVersions(Cmd_Argv(1), Cmd_Argv(2), ok);
    Con_Printf("vr_update_compare: %s %s %s\n", Cmd_Argv(1), !ok ? "?" : c < 0 ? "<" : c > 0 ? ">" : "==", Cmd_Argv(2));
}

} // namespace

// ---------------------------------------------------------------- the API

int compareVersions(const char* a, const char* b, bool& ok)
{
    SemVer x, y;
    ok = a && b && parseSemVer(a, x) && parseSemVer(b, y);
    if(!ok)
    {
        return 0;
    }
    for(int i = 0; i < 3; i++)
    {
        if(x.core[i] != y.core[i])
        {
            return x.core[i] < y.core[i] ? -1 : 1;
        }
    }
    return comparePre(x, y);
}

void start()
{
    if(running.loadSeqCst() || checked)
    {
        if(startNote == "not checked yet")
        {
            startNote = "not run (vr_update_check_now came first)"; // (the config's or a test script's)
        }
        return;
    }
    if(!vr_update_check.value)
    {
        startNote = "off (vr_update_check 0)";
        return;
    }
    if(getenv("QVR_TEST_BACKGROUND"))
    {
        startNote = "not run (a test run: vr_update_check_now asks)";
        return;
    }
    startNote = "run";
    startPass(true);
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
        Sys_Printf("update check: did not stop in 3 s; quitting without it\n");
        Download_KeepGlobalState(); // (its transfer still reads libcurl's global state: not freed under it)
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
    if(pendingReady.loadSeqCst())
    {
        {
            za::LockGuard lock{handoff};
            last = ZA_MOVE(pending);
            pending = Result{};
            pendingReady.storeSeqCst(false);
        }
        checked = true;
        lastGeneration++;
        bool ok = false;
        const int c = last.ok ? compareVersions(last.version.cStr(), VR_Version(), ok) : 0;
        char line[512];
        if(last.ok)
        {
            q_snprintf(line, sizeof(line), "update check: latest %s from %s (%s): %s%s%s\n", last.version.cStr(),
                last.source.cStr(), last.fromCache ? "cached" : "asked",
                !ok ? "not a version this game reads" : c > 0 ? "newer than this game's" : "not newer than this game's",
                last.error.empty() ? "" : "; no feed answered: ", last.error.cStr());
        }
        else
        {
            q_snprintf(line, sizeof(line), "update check: no answer (%s)\n", last.error.cStr());
        }
        if(askedPass)
        {
            Con_Printf("%s", line);
        }
        else
        {
            Con_DPrintf("%s", line); // (silent: only with developer)
        }
        askedPass = false;
    }
    refreshNotice();
}

const Notice* notice()
{
    return hasNotice ? &current : nullptr;
}

void registerCommands()
{
    Cmd_AddCommand("vr_update_status", status_f);
    Cmd_AddCommand("vr_update_check_now", checkNow_f);
    Cmd_AddCommand("vr_update_compare", compare_f);
}

} // namespace qvr::update
