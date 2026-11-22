// vr_discord.cpp -- Discord Rich Presence (vr_discord.hpp; docs/vr-port/DISCORD.md): what the player plays, shown on
// their Discord profile, through the Discord client's own local IPC. No Discord SDK and no library.
//
// The protocol (Discord's RPC over IPC, as its open-source clients use it): a Windows named pipe \\.\pipe\discord-ipc-N
// (N 0 to 9: the first that opens), on Linux and macOS a unix socket <dir>/discord-ipc-N (<dir>: XDG_RUNTIME_DIR,
// TMPDIR, TMP, TEMP or /tmp; a Flatpak's app/com.discordapp.Discord and a Snap's snap.discord under it). Each frame:
// opcode (uint32, little endian), the payload's length (uint32), the payload (JSON). Opcodes: 0 HANDSHAKE (ours:
// {"v":1,"client_id":"<application id>"}), 1 FRAME (commands and answers; Discord's first is the READY dispatch), 2 CLOSE
// (Discord's: refused, an unknown application id says so in its "message"), 3 PING (answered by 4 PONG, its payload).
// The activity: FRAME {"cmd":"SET_ACTIVITY","args":{"pid":<ours>,"activity":{...}},"nonce":"<n>"}; without "activity":
// cleared.
//
// Threads: the main thread (frame) reads the game's state each frame into a small key (no allocation); when it changed,
// and no sooner than minGapSeconds after the last one handed over (Discord's rate limit; the changes in between are
// merged: the newest goes), it writes the activity's JSON and hands it to the thread under a lock held for a copy. The
// thread (a loop that blocks on its pipe for its whole life: CODE_STYLE.md, "Threads") connects, sends, reads Discord's
// answers, and reconnects with a back-off (2 s doubling to 60 s) when Discord is not running or went away; every read and
// write waits 1 s at most (a Discord that stopped reading never holds the game). It never prints: its state is read by
// vr_discord_status on the main thread. Off (vr_discord 0, or no vr_discord_appid): the activity cleared and the pipe
// closed, the thread idle. Quit (finish): every wait of the thread cut to quitBudgetMs (0.5 s; a wait already under way
// too), the activity cleared if it can be in that time, the thread joined. On by default (opt-out); never on Android.
//
// The kit's test runs (QVR_TEST_BACKGROUND) never reach the real Discord: only vr_discord_test's own pipe.
//
// A "join" button (multiplayer room codes, later): the activity's "party" ({"id", "size": [n, max]}) and "secrets"
// ({"join": the room code}), and Discord's ACTIVITY_JOIN event (a SUBSCRIBE frame, then the event's "secret" read in
// readFrames) -- the place is marked JOIN-HOOK below.

#include "vr_discord.hpp"
#include "vr_api.h"
#include "vr_arenawaves.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_main.hpp"
#include "vr_progs.hpp"
#include "vr_strings.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Chrono/Time.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/ToString.hpp"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0 // (macOS: SO_NOSIGPIPE below instead)
#endif
#endif

namespace qvr::discord
{
namespace
{

constexpr double minGapSeconds = 15.0; // between two activities handed over (Discord allows 5 a 20 s: kept well under)
constexpr int ioTimeoutMs = 1000;      // a read or write waits this long at most
constexpr int readyTimeoutMs = 3000;   // the handshake's READY
constexpr int quitBudgetMs = 500;      // quitting: every wait of the thread's ends by then (a hung Discord never holds the quit)
constexpr za::U32 maxPayload = 64 * 1024;
constexpr const char* defaultPipeBase = "discord-ipc-";
constexpr const char* testName = "vr_discord_test";

enum Op : za::U32
{
    opHandshake = 0,
    opFrame = 1,
    opClose = 2,
    opPing = 3,
    opPong = 4,
};

[[nodiscard]] za::I64 nowMs() { return za::Clock::nowNanoseconds() / 1000000; }

// Quitting (finish): the time every wait of the thread ends by (0: not quitting), and on Windows an event set with it
// that cuts a wait already under way short (manual reset: stays set until the next start).
za::Atomic<za::I64> quitDeadline{0};
#ifdef _WIN32
HANDLE quitEvent = nullptr;
#endif

// What is left of a wait that ends at `end`, at most what is left before the quit's deadline (0: none left).
[[nodiscard]] za::I64 waitLeft(za::I64 end)
{
    const za::I64 now = nowMs();
    za::I64 left = end - now;
    const za::I64 quit = quitDeadline.loadSeqCst();
    if(quit != 0)
    {
        left = za::min(left, quit - now);
    }
    return left > 0 ? left : 0;
}

[[nodiscard]] unsigned long processId()
{
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return static_cast<unsigned long>(getpid());
#endif
}

// A JSON string's text, without its quotes: quotes and backslashes escaped, control characters dropped (tabs and line
// breaks made spaces); Quake's coloured (high) characters made plain, so the payload is ASCII.
void appendJsonText(za::String& out, const char* s)
{
    for(; *s; ++s)
    {
        const char c = static_cast<char>(*s & 0x7f);
        if(c == '"' || c == '\\')
        {
            out += '\\';
            out += c;
        }
        else if(c == '\n' || c == '\r' || c == '\t')
        {
            out += ' ';
        }
        else if(c >= 0x20 && c != 0x7f)
        {
            out += c;
        }
    }
}

// A JSON string: appendJsonText's, quoted.
void appendJson(za::String& out, const char* s)
{
    out += '"';
    appendJsonText(out, s);
    out += '"';
}

// ---- The pipe (the platform's part) ----------------------------------------------------------------------------

struct Pipe
{
#ifdef _WIN32
    HANDLE h{INVALID_HANDLE_VALUE};
    HANDLE ev{nullptr}; // the overlapped I/O's event (made once)

    [[nodiscard]] bool isOpen() const { return h != INVALID_HANDLE_VALUE; }

    bool open(const char* name)
    {
        if(!ev)
        {
            ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        }
        char path[256];
        snprintf(path, sizeof(path), "\\\\.\\pipe\\%s", name);
        h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        return isOpen() && ev;
    }

    void close()
    {
        if(isOpen())
        {
            CloseHandle(h);
            h = INVALID_HANDLE_VALUE;
        }
    }

    // One overlapped read or write, waited for `ms` at most (cancelled past it). The bytes moved, or -1.
    long transfer(void* data, DWORD size, bool write, int ms)
    {
        OVERLAPPED o{};
        o.hEvent = ev;
        ResetEvent(ev);
        DWORD done = 0;
        const BOOL ok = write ? WriteFile(h, data, size, nullptr, &o) : ReadFile(h, data, size, nullptr, &o);
        if(!ok && GetLastError() != ERROR_IO_PENDING && GetLastError() != ERROR_MORE_DATA)
        {
            return -1;
        }
        // (the quit's event too: past it, the rest of the wait is capped by the quit's deadline)
        const za::I64 end = nowMs() + ms;
        HANDLE waits[2] = {ev, quitEvent};
        DWORD count = quitEvent ? 2 : 1;
        DWORD r = WAIT_TIMEOUT;
        for(;;)
        {
            r = WaitForMultipleObjects(count, waits, FALSE, static_cast<DWORD>(waitLeft(end)));
            if(r != WAIT_OBJECT_0 + 1)
            {
                break;
            }
            count = 1;
        }
        if(r != WAIT_OBJECT_0)
        {
            CancelIoEx(h, &o);
            GetOverlappedResult(h, &o, &done, TRUE);
            return -1;
        }
        if(!GetOverlappedResult(h, &o, &done, FALSE) && GetLastError() != ERROR_MORE_DATA)
        {
            return -1;
        }
        return static_cast<long>(done);
    }

    // Bytes waiting to be read; -1: the pipe is broken.
    [[nodiscard]] long available()
    {
        DWORD avail = 0;
        return PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr) ? static_cast<long>(avail) : -1;
    }
#else
    int fd{-1};

    [[nodiscard]] bool isOpen() const { return fd >= 0; }

    bool open(const char* path)
    {
        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if(fd < 0)
        {
            return false;
        }
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        if(strlen(path) >= sizeof(addr.sun_path))
        {
            close();
            return false;
        }
        strcpy(addr.sun_path, path);
        if(connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        {
            close();
            return false;
        }
#ifdef SO_NOSIGPIPE
        int one = 1;
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        return true;
    }

    void close()
    {
        if(fd >= 0)
        {
            ::close(fd);
            fd = -1;
        }
    }

    long transfer(void* data, size_t size, bool write, int ms)
    {
        const za::I64 end = nowMs() + ms;
        for(;;)
        {
            const ssize_t n = write ? send(fd, data, size, MSG_NOSIGNAL) : recv(fd, data, size, 0);
            if(n > 0)
            {
                return static_cast<long>(n);
            }
            if(n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
            {
                return -1;
            }
            // (in slices: a quit's deadline cuts the wait short)
            for(;;)
            {
                const za::I64 left = waitLeft(end);
                if(left <= 0)
                {
                    return -1;
                }
                pollfd p{fd, static_cast<short>(write ? POLLOUT : POLLIN), 0};
                const int r = poll(&p, 1, static_cast<int>(za::min(left, za::I64{50})));
                if(r < 0 && errno != EINTR)
                {
                    return -1;
                }
                if(r > 0)
                {
                    break;
                }
            }
        }
    }

    [[nodiscard]] long available()
    {
        pollfd p{fd, POLLIN, 0};
        if(poll(&p, 1, 0) < 0 || (p.revents & (POLLERR | POLLHUP | POLLNVAL)))
        {
            return -1;
        }
        int n = 0;
        return ioctl(fd, FIONREAD, &n) == 0 ? n : -1;
    }
#endif

    bool all(void* data, size_t size, bool write, int ms)
    {
        auto* p = static_cast<char*>(data);
        while(size > 0)
        {
            const long n = transfer(p, static_cast<za::U32>(size), write, ms);
            if(n <= 0)
            {
                return false;
            }
            p += n;
            size -= static_cast<size_t>(n);
        }
        return true;
    }

    bool sendFrame(za::U32 op, const za::String& json)
    {
        za::Vector<char> buf;
        buf.resize(8 + json.size());
        const za::U32 header[2] = {op, static_cast<za::U32>(json.size())}; // (every platform built is little endian)
        memcpy(buf.data(), header, 8);
        memcpy(buf.data() + 8, json.data(), json.size());
        return all(buf.data(), buf.size(), true, ioTimeoutMs);
    }

    // A whole frame read (its header waited for `ms`, its payload ioTimeoutMs). False: broken or timed out.
    bool readFrame(za::U32& op, za::String& json, int ms)
    {
        za::U32 header[2]{};
        if(!all(header, 8, false, ms) || header[1] > maxPayload)
        {
            return false;
        }
        op = header[0];
        json.resize(header[1]);
        return header[1] == 0 || all(json.data(), header[1], false, ioTimeoutMs);
    }
};

// ---- The state shared with the thread ----------------------------------------------------------------------------

struct Wanted
{
    bool on{false};
    za::String appId;
    za::String pipeBase;
    za::String activity; // the activity object's JSON; empty: cleared
    int serial{0};       // bumped by each handover
};

struct Status
{
    bool connected{false};
    int attempts{0}; // connections tried
    int connects{0}; // made (READY)
    int sent{0};     // SET_ACTIVITY frames sent (clears too)
    za::String error; // the last failure or Discord's last error message
    za::String pipe;  // the pipe connected to
};

za::AtomicMutex mutex;
Wanted wanted;
Status status;
za::Thread worker;
za::Atomic<bool> running{false};
za::Atomic<bool> wakeFlag{false};
#ifdef _WIN32
HANDLE wakeEvent = nullptr;
#endif

void wake()
{
    wakeFlag.storeSeqCst(true);
#ifdef _WIN32
    if(wakeEvent)
    {
        SetEvent(wakeEvent);
    }
#endif
}

// The thread's sleep, cut short by wake().
void sleepMs(int ms)
{
#ifdef _WIN32
    WaitForSingleObject(wakeEvent, static_cast<DWORD>(ms));
#else
    for(int left = ms; left > 0 && !wakeFlag.loadSeqCst(); left -= 50)
    {
        za::ThisThread::sleepFor(za::milliseconds(za::min(left, 50)));
    }
#endif
    wakeFlag.storeSeqCst(false);
}

void setError(const char* text)
{
    za::LockGuard lock{mutex};
    status.error = text;
}

// A string field's value in Discord's JSON ("message":"..."), for the status; not a parser (no escapes undone).
[[nodiscard]] za::String jsonField(const za::String& json, const char* field)
{
    char key[64];
    snprintf(key, sizeof(key), "\"%s\":\"", field);
    const char* at = strstr(json.cStr(), key);
    if(!at)
    {
        return za::String{};
    }
    at += strlen(key);
    const char* end = strchr(at, '"');
    return end ? za::String{at, static_cast<za::SizeT>(end - at)} : za::String{};
}

// The pipes' names to try, in order.
[[nodiscard]] za::Vector<za::String> pipeNames(const za::String& base)
{
    za::Vector<za::String> names;
#ifdef _WIN32
    for(int i = 0; i < 10; ++i)
    {
        names.pushBack(base + za::toString(i));
    }
#else
    static constexpr const char* vars[] = {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"};
    static constexpr const char* subs[] = {"", "/app/com.discordapp.Discord", "/snap.discord"};
    const char* dir = nullptr;
    for(const char* var : vars)
    {
        if(!dir || !*dir)
        {
            dir = getenv(var);
        }
    }
    const za::String root = dir && *dir ? za::String{dir} : za::String{"/tmp"};
    for(const char* sub : subs)
    {
        for(int i = 0; i < 10; ++i)
        {
            names.pushBack(root + sub + "/" + base + za::toString(i));
        }
    }
#endif
    return names;
}

struct Connection
{
    Pipe pipe;
    int nonce{0};

    // Discord's frames waiting, read: a PING answered, a CLOSE or a broken pipe false, an error's message kept.
    bool readFrames()
    {
        for(;;)
        {
            const long avail = pipe.available();
            if(avail < 0)
            {
                setError("Discord closed the connection");
                return false;
            }
            if(avail < 8)
            {
                return true;
            }
            za::U32 op = 0;
            za::String json;
            if(!pipe.readFrame(op, json, ioTimeoutMs))
            {
                setError("a read from Discord failed");
                return false;
            }
            if(op == opPing)
            {
                if(!pipe.sendFrame(opPong, json))
                {
                    return false;
                }
            }
            else if(op == opClose)
            {
                const za::String message = jsonField(json, "message");
                setError(message.empty() ? "Discord closed the connection" : message.cStr());
                return false;
            }
            else if(op == opFrame && strstr(json.cStr(), "\"evt\":\"ERROR\""))
            {
                setError(jsonField(json, "message").cStr());
            }
            // JOIN-HOOK: an ACTIVITY_JOIN dispatch's "secret" (the room code) is read here.
        }
    }

    bool open(const za::String& base, const za::String& appId)
    {
        for(const za::String& name : pipeNames(base))
        {
            if(!pipe.open(name.cStr()))
            {
                pipe.close();
                continue;
            }
            za::String hello = "{\"v\":1,\"client_id\":";
            appendJson(hello, appId.cStr());
            hello += '}';
            za::U32 op = 0;
            za::String json;
            if(pipe.sendFrame(opHandshake, hello) && pipe.readFrame(op, json, readyTimeoutMs) && op == opFrame
                && strstr(json.cStr(), "\"evt\":\"READY\""))
            {
                za::LockGuard lock{mutex};
                status.connected = true;
                status.connects++;
                status.pipe = name;
                status.error.clear();
                return true;
            }
            const za::String message = op == opClose ? jsonField(json, "message") : za::String{};
            setError(message.empty() ? "no READY from Discord" : message.cStr());
            pipe.close();
            return false; // (a Discord that answered: the other pipes are no better)
        }
        setError("Discord is not running (no IPC pipe)");
        return false;
    }

    bool sendActivity(const za::String& activity)
    {
        za::String frame = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":";
        frame += za::toString(static_cast<za::U64>(processId()));
        if(!activity.empty())
        {
            frame += ",\"activity\":";
            frame += activity;
        }
        frame += "},\"nonce\":\"";
        frame += za::toString(++nonce);
        frame += "\"}";
        if(!pipe.sendFrame(opFrame, frame))
        {
            setError("a write to Discord failed");
            return false;
        }
        za::LockGuard lock{mutex};
        status.sent++;
        return true;
    }

    void close()
    {
        pipe.close();
        za::LockGuard lock{mutex};
        status.connected = false;
    }
};

void run()
{
    Connection c;
    int sentSerial = -1;
    za::I64 nextTry = 0;
    int backoffMs = 2000;
    while(running.loadSeqCst())
    {
        Wanted w;
        {
            za::LockGuard lock{mutex};
            w = wanted;
        }
        if(!w.on || w.appId.empty())
        {
            if(c.pipe.isOpen())
            {
                c.sendActivity(za::String{}); // (cleared, then closed)
                c.close();
            }
            sentSerial = -1;
            nextTry = 0;
            backoffMs = 2000;
            sleepMs(500);
            continue;
        }
        if(!c.pipe.isOpen())
        {
            if(nowMs() < nextTry)
            {
                sleepMs(static_cast<int>(za::min(nextTry - nowMs(), za::I64{500})));
                continue;
            }
            {
                za::LockGuard lock{mutex};
                status.attempts++;
            }
            if(!c.open(w.pipeBase, w.appId))
            {
                c.close();
                nextTry = nowMs() + backoffMs;
                backoffMs = za::min(backoffMs * 2, 60000);
                continue;
            }
            backoffMs = 2000;
            sentSerial = -1;
        }
        bool ok = true;
        if(sentSerial != w.serial)
        {
            ok = c.sendActivity(w.activity);
            sentSerial = w.serial;
        }
        ok = ok && c.readFrames();
        if(!ok)
        {
            c.close();
            nextTry = nowMs() + backoffMs;
            continue;
        }
        sleepMs(250);
    }
    if(c.pipe.isOpen())
    {
        c.sendActivity(za::String{}); // quit: cleared
        c.close();
    }
}

void startThread()
{
    if(running.exchangeSeqCst(true))
    {
        return;
    }
    quitDeadline.storeSeqCst(0);
#ifdef _WIN32
    if(!wakeEvent)
    {
        wakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr); // (auto-reset)
    }
    if(!quitEvent)
    {
        quitEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr); // (manual reset)
    }
    ResetEvent(quitEvent);
#endif
    worker = za::Thread(run);
}

// ---- The game's state (main thread) --------------------------------------------------------------------------------

enum class Mode
{
    Menus,
    Demo,
    Single,
    Coop,
    Deathmatch,
    Arena,
};

// What the activity says, read each frame without allocating; a change is handed over.
struct Key
{
    bool on{false};
    Mode mode{Mode::Menus};
    int wave{0};
    bool vr{false};
    int campaign{0};
    char map[64]{};

    [[nodiscard]] bool operator==(const Key& o) const
    {
        return on == o.on && mode == o.mode && wave == o.wave && vr == o.vr && campaign == o.campaign
            && !strcmp(map, o.map);
    }
};

// The test's stand-ins (vr_discord_test): on with its application id and its own pipe, whatever the cvars say.
struct TestForce
{
    bool on{false};
    za::String pipeBase;
};

Key current;           // the key last read in a map (kept while one loads)
Key handed;            // the key last handed over
bool handedAny{false}; // anything handed over yet
double handedAt{-1e9}; // realtime of the last handover
long long mapStart{0}; // unix time when this map began (the activity's elapsed time)
char mapStartFor[64]{};
TestForce testForce;
constexpr const char* testAppId = "123456789012345678";

[[nodiscard]] bool enabledNow()
{
    if(testForce.on)
    {
        return true;
    }
    if(!vr_discord.value || !vr_discord_appid.string[0])
    {
        return false;
    }
    return getenv("QVR_TEST_BACKGROUND") == nullptr; // (the kit's test runs never reach the real Discord)
}

[[nodiscard]] Key readKey()
{
    Key k = current;
    k.on = enabledNow();
    k.vr = vrActive();
    k.campaign = VR_ActiveCampaign();
    if(cls.state != ca_connected)
    {
        k.mode = Mode::Menus;
        k.wave = 0;
        k.map[0] = 0;
        return k;
    }
    if(cls.signon != SIGNONS || !cl.mapname[0])
    {
        return k; // (a map loading: the last one's kept)
    }
    q_strlcpy(k.map, cl.mapname, sizeof(k.map));
    k.wave = 0;
    if(cls.demoplayback)
    {
        k.mode = Mode::Demo;
    }
    else if(arenawaves::isArenaMap(cl.mapname))
    {
        k.mode = Mode::Arena;
        const progs::Bindings& b = progs::bindings();
        if(sv.active && b.arenaWave)
        {
            k.wave = static_cast<int>(*b.arenaWave);
        }
    }
    else if(cl.gametype == GAME_DEATHMATCH)
    {
        k.mode = Mode::Deathmatch;
    }
    else
    {
        k.mode = cl.maxclients > 1 ? Mode::Coop : Mode::Single;
    }
    return k;
}

// The activity's JSON object for a key (empty: cleared).
[[nodiscard]] za::String activityFor(const Key& k)
{
    if(!k.on)
    {
        return za::String{};
    }
    za::String title = strings::trim(k.mode == Mode::Menus ? "In the menus" : cl.levelname);
    if(title.empty())
    {
        title = k.map;
    }
    constexpr const char* dot = " \\u00b7 "; // (a middle dot between the state's parts)
    za::String a = "{\"details\":";
    appendJson(a, title.cStr());
    a += ",\"state\":\"";
    if(k.mode != Mode::Menus)
    {
        const char* campaign = VR_CampaignTitle(VR_CampaignForMap(k.map));
        appendJsonText(a, campaign && *campaign ? campaign : "Quake");
        a += dot;
        switch(k.mode)
        {
            case Mode::Demo: a += "Watching a demo"; break;
            case Mode::Single: a += "Single player"; break;
            case Mode::Coop: a += "Co-op"; break;
            case Mode::Deathmatch: a += "Deathmatch"; break;
            case Mode::Arena:
                a += "Survival Arena";
                if(k.wave > 0)
                {
                    strings::appendf(a, " wave %d", k.wave);
                }
                break;
            case Mode::Menus: break;
        }
        a += dot;
    }
    a += k.vr ? "VR\"" : "Flat screen\"";
    if(k.mode != Mode::Menus && mapStart > 0)
    {
        strings::appendf(a, ",\"timestamps\":{\"start\":%lld}", mapStart);
    }
    if(vr_discord_image.string[0])
    {
        a += ",\"assets\":{\"large_image\":";
        appendJson(a, vr_discord_image.string);
        a += ",\"large_text\":";
        appendJson(a, k.mode == Mode::Menus ? "Quake VR" : k.map);
        a += '}';
    }
    // JOIN-HOOK: ",\"party\":{\"id\":...,\"size\":[n,max]},\"secrets\":{\"join\":<room code>}" once rooms exist.
    a += '}';
    return a;
}

void handOver(const Key& k)
{
    za::String activity = activityFor(k);
    {
        za::LockGuard lock{mutex};
        wanted.on = k.on;
        wanted.appId = testForce.on ? za::String{testAppId} : za::String{vr_discord_appid.string};
        wanted.pipeBase = testForce.on ? testForce.pipeBase : za::String{defaultPipeBase};
        wanted.activity = ZA_MOVE(activity);
        wanted.serial++;
    }
    handed = k;
    handedAny = true;
    handedAt = realtime;
    wake();
}

void presenceFrame()
{
    const Key k = readKey();
    current = k;
    if(k.mode != Mode::Menus && strcmp(mapStartFor, k.map) != 0)
    {
        q_strlcpy(mapStartFor, k.map, sizeof(mapStartFor));
        mapStart = static_cast<long long>(time(nullptr));
    }
    if(!k.on && !handedAny)
    {
        return; // never on: no thread
    }
    if(handedAny && k == handed)
    {
        return;
    }
    // Turned off: at once (cleared). Otherwise no sooner than minGapSeconds after the last.
    if(k.on && handedAny && handed.on && realtime - handedAt < minGapSeconds)
    {
        return;
    }
    startThread();
    handOver(k);
}

// ---- vr_discord_test: a fake Discord on a pipe of the test's own -------------------------------------------------------

#ifdef _WIN32
struct FakeFrame
{
    za::U32 op{0};
    za::String json;
    double at{0.0}; // realtime when read
};

struct FakeDiscord
{
    HANDLE h{INVALID_HANDLE_VALUE};
    HANDLE ev{nullptr};
    OVERLAPPED connectOv{};
    bool connected{false};
    bool silent{false}; // a hung Discord: it takes the connection and reads, never answers
    za::Vector<char> in;
    za::Vector<FakeFrame> frames;
    int clients{0}; // connections accepted

    bool listen(const char* name)
    {
        char path[256];
        snprintf(path, sizeof(path), "\\\\.\\pipe\\%s", name);
        h = CreateNamedPipeA(path, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                             1, 65536, 65536, 0, nullptr);
        if(h == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        if(!ev)
        {
            ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        }
        return accept();
    }

    bool accept()
    {
        connectOv = OVERLAPPED{};
        connectOv.hEvent = ev;
        ResetEvent(ev);
        connected = false;
        in.clear();
        if(ConnectNamedPipe(h, &connectOv))
        {
            connected = true;
        }
        else if(GetLastError() == ERROR_PIPE_CONNECTED)
        {
            connected = true;
        }
        else if(GetLastError() != ERROR_IO_PENDING)
        {
            return false;
        }
        if(connected)
        {
            clients++;
        }
        return true;
    }

    void close()
    {
        if(h != INVALID_HANDLE_VALUE)
        {
            CancelIoEx(h, nullptr);
            CloseHandle(h);
            h = INVALID_HANDLE_VALUE;
        }
        connected = false;
    }

    void reply(za::U32 op, const char* json)
    {
        const za::U32 header[2] = {op, static_cast<za::U32>(strlen(json))};
        za::Vector<char> buf;
        buf.resize(8 + header[1]);
        memcpy(buf.data(), header, 8);
        memcpy(buf.data() + 8, json, header[1]);
        OVERLAPPED o{};
        o.hEvent = ev;
        ResetEvent(ev);
        DWORD done = 0;
        if(WriteFile(h, buf.data(), static_cast<DWORD>(buf.size()), nullptr, &o) || GetLastError() == ERROR_IO_PENDING)
        {
            GetOverlappedResult(h, &o, &done, TRUE); // (a few bytes into an empty pipe: at once)
        }
    }

    // Each frame: the client's connection accepted, its bytes read, its frames recorded and answered.
    void poll()
    {
        if(h == INVALID_HANDLE_VALUE)
        {
            return;
        }
        if(!connected)
        {
            DWORD dummy = 0;
            if(!HasOverlappedIoCompleted(&connectOv) || !GetOverlappedResult(h, &connectOv, &dummy, FALSE))
            {
                return;
            }
            connected = true;
            clients++;
        }
        DWORD avail = 0;
        if(!PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr))
        {
            DisconnectNamedPipe(h); // the client went: wait for the next
            accept();
            return;
        }
        if(avail > 0)
        {
            const za::SizeT old = in.size();
            in.resize(old + avail);
            OVERLAPPED o{};
            o.hEvent = ev;
            ResetEvent(ev);
            DWORD got = 0;
            if(ReadFile(h, in.data() + old, avail, nullptr, &o) || GetLastError() == ERROR_IO_PENDING)
            {
                GetOverlappedResult(h, &o, &got, TRUE); // (the bytes are there: at once)
            }
            in.resize(old + got);
        }
        while(in.size() >= 8)
        {
            za::U32 header[2];
            memcpy(header, in.data(), 8);
            if(in.size() < 8 + header[1])
            {
                break;
            }
            FakeFrame f;
            f.op = header[0];
            f.json = za::String{in.data() + 8, static_cast<za::SizeT>(header[1])};
            f.at = realtime;
            in.erase(in.begin(), in.begin() + 8 + header[1]);
            if(silent)
            {
                // (no answer)
            }
            else if(f.op == opHandshake)
            {
                reply(opFrame, "{\"cmd\":\"DISPATCH\",\"evt\":\"READY\",\"data\":{\"v\":1}}");
            }
            else if(f.op == opFrame)
            {
                const za::String nonce = jsonField(f.json, "nonce");
                char ack[128];
                snprintf(ack, sizeof(ack), "{\"cmd\":\"SET_ACTIVITY\",\"evt\":null,\"nonce\":\"%s\",\"data\":{}}", nonce.cStr());
                reply(opFrame, ack);
            }
            frames.pushBack(ZA_MOVE(f));
        }
    }

    // The SET_ACTIVITY frames recorded.
    [[nodiscard]] za::Vector<const FakeFrame*> activities() const
    {
        za::Vector<const FakeFrame*> out;
        for(const FakeFrame& f : frames)
        {
            if(f.op == opFrame && strstr(f.json.cStr(), "\"cmd\":\"SET_ACTIVITY\""))
            {
                out.pushBack(&f);
            }
        }
        return out;
    }
};
#endif

// The test's steps, one a frame until its condition holds (or its frame budget runs out: a FAIL).
enum class Step
{
    Off,
    NoPipe,       // on, no Discord: the map loads and plays, connections tried, none made
    FirstConnect, // the fake Discord up: the handshake, then the single-player activity
    Unchanged,    // no change for longer than the gap: nothing sent
    Flat,         // VR off: a new activity, no sooner than the gap after the last
    Coop,         // a co-op game
    Arena,        // the Survival Arena: its wave in the activity
    Restart,      // Discord gone and back: reconnected, the activity sent again
    Hung,         // a Discord that takes the connection and never answers: the frames go on, a quit waits < 0.6 s
    Resume,       // the thread started again, Discord answering: the activity sent again
    Disable,      // off: the activity cleared, the pipe closed
};

struct Test
{
    Step step{Step::Off};
    int frames{0};      // frames in this step
    int mark{0};        // the activities seen when the step began
    bool entered{false};
    double lastAt{0.0};  // Hung: the last frame's realtime
    double maxGap{0.0};  // Hung: the longest frame since the handshake came
    double hungAt{0.0};  // Hung: when the unanswered handshake came
    testkit::Tally t;
    za::String pipeBase;
#ifdef _WIN32
    FakeDiscord fake;
#endif
};

Test test;

void check(bool ok, const char* what)
{
    if(ok)
    {
        Con_Printf("%s: PASS %s\n", testName, what);
    }
    test.t.check(ok, testName, what);
}

void finishTest()
{
    test.t.verdict(testName);
#ifdef _WIN32
    test.fake.close();
#endif
    testForce = TestForce{};
    test.step = Step::Off;
}

void next(Step s)
{
    test.step = s;
    test.frames = 0;
    test.entered = false;
}

[[nodiscard]] bool inMap(const char* map) { return cls.state == ca_connected && cls.signon == SIGNONS && !strcmp(cl.mapname, map); }

#ifdef _WIN32
// The newest activity's frame, when more than `than` were recorded.
[[nodiscard]] const FakeFrame* newestAfter(int than)
{
    const za::Vector<const FakeFrame*> a = test.fake.activities();
    return static_cast<int>(a.size()) > than ? a.back() : nullptr;
}

[[nodiscard]] bool has(const FakeFrame* f, const char* text) { return f && strstr(f->json.cStr(), text) != nullptr; }

[[nodiscard]] int activityCount() { return static_cast<int>(test.fake.activities().size()); }

[[nodiscard]] Status statusCopy()
{
    za::LockGuard lock{mutex};
    return status;
}
#endif

void testFrame()
{
#ifdef _WIN32
    test.fake.poll();
    test.frames++;
    const bool enter = !test.entered;
    test.entered = true;
    if(enter)
    {
        test.mark = activityCount();
    }
    // A safety net: every step's condition comes in far fewer frames (the back-off is real time: 2 to 4 s).
    if(test.frames > 30000)
    {
        check(false, "timed out");
        finishTest();
        return;
    }
    switch(test.step)
    {
        case Step::Off: break;
        case Step::NoPipe:
            if(enter)
            {
                Cbuf_AddText("disconnect; maxplayers 1; coop 0; deathmatch 0; map e1m1\n");
            }
            else if(inMap("e1m1") && statusCopy().attempts >= 1 && ++test.mark > 150) // (150 frames played after)
            {
                const Status s = statusCopy();
                check(!s.connected && s.connects == 0, "no Discord: no connection, the game plays on");
                check(test.fake.frames.empty(), "no Discord: nothing received");
                if(!test.fake.listen((test.pipeBase + "0").cStr()))
                {
                    check(false, "the fake Discord's pipe made");
                    finishTest();
                    return;
                }
                next(Step::FirstConnect);
            }
            break;
        case Step::FirstConnect:
            if(const FakeFrame* f = newestAfter(0))
            {
                const FakeFrame& hello = test.fake.frames[0];
                check(hello.op == opHandshake && strstr(hello.json.cStr(), "\"v\":1")
                          && strstr(hello.json.cStr(), "\"client_id\":\"123456789012345678\""),
                      "the handshake: version 1, the application id");
                check(has(f, "\"details\":\"the Slipgate Complex\""), "single player: the map's title");
                check(has(f, "Quake \\u00b7 Single player \\u00b7 VR\""), "single player: campaign, mode, VR");
                check(has(f, "\"timestamps\":{\"start\":") && has(f, "\"pid\":"), "single player: the map's start time, the pid");
                Con_Printf("%s: activity %s\n", testName, f->json.cStr());
                next(Step::Unchanged);
            }
            break;
        case Step::Unchanged:
            // The gap and a second more of game time with nothing changed.
            if(test.frames > static_cast<int>((minGapSeconds + 1.0) * 72.0))
            {
                check(activityCount() == test.mark, "nothing changed: nothing sent");
                next(Step::Flat);
            }
            break;
        case Step::Flat:
            if(enter)
            {
                Cbuf_AddText("vr_enabled 0\n");
            }
            else if(const FakeFrame* f = newestAfter(test.mark))
            {
                const za::Vector<const FakeFrame*> a = test.fake.activities();
                check(has(f, "Single player \\u00b7 Flat screen\""), "flat screen: said");
                check(a.size() >= 2 && f->at - a[a.size() - 2]->at >= minGapSeconds - 0.5, "flat screen: no sooner than the gap");
                next(Step::Coop);
            }
            break;
        case Step::Coop:
            if(enter)
            {
                Cbuf_AddText("disconnect; maxplayers 4; coop 1; map e1m2\n");
            }
            else if(const FakeFrame* f = newestAfter(test.mark); f && has(f, "Co-op"))
            {
                check(has(f, "\"details\":\"Castle of the Damned\"") && has(f, "Quake \\u00b7 Co-op \\u00b7 Flat screen\""),
                      "co-op: the map, the mode");
                next(Step::Arena);
            }
            break;
        case Step::Arena:
            if(enter)
            {
                Cbuf_AddText("disconnect; maxplayers 1; coop 0; map vrarena\n");
            }
            else if(const FakeFrame* f = newestAfter(test.mark); f && has(f, "Survival Arena wave "))
            {
                check(true, "arena: its wave");
                Con_Printf("%s: activity %s\n", testName, f->json.cStr());
                next(Step::Restart);
            }
            break;
        case Step::Restart:
            if(enter)
            {
                test.fake.close(); // Discord quit
            }
            else if(test.fake.h == INVALID_HANDLE_VALUE)
            {
                if(!statusCopy().connected)
                {
                    test.fake.frames.clear();
                    test.mark = 0;
                    test.fake.listen((test.pipeBase + "0").cStr()); // Discord back
                }
            }
            else if(const FakeFrame* f = newestAfter(0))
            {
                check(test.fake.frames[0].op == opHandshake, "Discord restarted: a new handshake");
                check(has(f, "Survival Arena"), "Discord restarted: the activity sent again");
                next(Step::Hung);
            }
            break;
        case Step::Hung:
            if(enter)
            {
                test.fake.close(); // Discord gone, then back hung: the connection taken, nothing ever answered
                test.fake.silent = true;
                test.hungAt = 0.0;
                test.maxGap = 0.0;
            }
            else if(test.fake.h == INVALID_HANDLE_VALUE)
            {
                if(!statusCopy().connected)
                {
                    test.fake.frames.clear();
                    test.fake.listen((test.pipeBase + "0").cStr());
                }
            }
            else if(test.hungAt == 0.0)
            {
                if(!test.fake.frames.empty() && test.fake.frames[0].op == opHandshake)
                {
                    test.hungAt = realtime; // the thread now waits for a READY that never comes (readyTimeoutMs)
                    test.lastAt = realtime;
                }
            }
            else
            {
                test.maxGap = za::max(test.maxGap, realtime - test.lastAt);
                test.lastAt = realtime;
                if(realtime - test.hungAt > 1.0) // (mid-wait: the READY's wait is 3 s)
                {
                    check(test.maxGap < 0.25, "a hung Discord: the game's frames go on (the longest under 0.25 s)");
                    const za::I64 t0 = nowMs();
                    finish(); // the quit's path
                    const za::I64 took = nowMs() - t0;
                    Con_Printf("%s: the quit with a hung Discord took %lld ms (longest frame %.3f s)\n", testName,
                               static_cast<long long>(took), test.maxGap);
                    check(took < 600, "a hung Discord: the quit waits under 0.6 s");
                    test.fake.close();
                    test.fake.silent = false;
                    test.fake.frames.clear();
                    test.fake.listen((test.pipeBase + "0").cStr());
                    startThread();
                    handOver(current);
                    next(Step::Resume);
                }
            }
            break;
        case Step::Resume:
            if(const FakeFrame* f = newestAfter(0))
            {
                check(test.fake.frames[0].op == opHandshake && has(f, "Survival Arena"),
                      "after the quit's path: the thread started again, the activity sent again");
                next(Step::Disable);
            }
            break;
        case Step::Disable:
            if(enter)
            {
                testForce.on = false; // (vr_discord 0's path: the cvar is 0 in test runs)
            }
            else if(const FakeFrame* f = newestAfter(test.mark); f && !statusCopy().connected)
            {
                check(!has(f, "\"activity\""), "off: the activity cleared, the pipe closed");
                Cbuf_AddText("vr_enabled 1\n");
                finishTest();
            }
            break;
    }
#endif
}

void test_f()
{
#ifdef _WIN32
    if(test.step != Step::Off)
    {
        Con_Printf("%s: already running\n", testName);
        return;
    }
    test = Test{};
    test.pipeBase = "qvr-discord-test-" + za::toString(static_cast<za::U64>(processId())) + "-";
    testForce.on = true;
    testForce.pipeBase = test.pipeBase;
    next(Step::NoPipe);
#else
    Con_Printf("%s: SKIP the fake Discord is Windows only\n", testName);
    testkit::verdict(testName, 0, 0);
#endif
}

void status_f()
{
    za::String activity;
    Status s;
    int serial = 0;
    {
        za::LockGuard lock{mutex};
        s = status;
        activity = wanted.activity;
        serial = wanted.serial;
    }
    Con_Printf("Discord Rich Presence: %s%s\n", vr_discord.value ? "on" : "off (vr_discord 0)",
               vr_discord.value && !vr_discord_appid.string[0] ? ", but no vr_discord_appid" : "");
    Con_Printf("  thread %s; %s%s; tries %d, connections %d, activities sent %d (handed over %d)\n",
               running.loadSeqCst() ? "running" : "not started", s.connected ? "connected to " : "not connected",
               s.connected ? s.pipe.cStr() : "", s.attempts, s.connects, s.sent, serial);
    if(!s.error.empty())
    {
        Con_Printf("  last error: %s\n", s.error.cStr());
    }
    Con_Printf("  activity: %s\n", activity.empty() ? "(none)" : activity.cStr());
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_discord_status", status_f);
    Cmd_AddCommand("vr_discord_test", test_f);
}

void frame()
{
#ifdef __ANDROID__
    return; // (no Discord app on a headset: no thread, nothing tried)
#endif
    if(cls.state == ca_dedicated)
    {
        return;
    }
    if(test.step != Step::Off)
    {
        testFrame();
    }
    presenceFrame();
}

void finish()
{
    if(!running.loadSeqCst())
    {
        return;
    }
    // Every wait of the thread ends within quitBudgetMs (a Discord that hung mid-handshake or stopped reading included);
    // it clears the activity if it can in that time (Discord clears it itself when the pipe closes).
    quitDeadline.storeSeqCst(nowMs() + quitBudgetMs);
#ifdef _WIN32
    if(quitEvent)
    {
        SetEvent(quitEvent);
    }
#endif
    running.storeSeqCst(false);
    wake();
    if(worker.joinable())
    {
        worker.join();
    }
}

} // namespace qvr::discord
