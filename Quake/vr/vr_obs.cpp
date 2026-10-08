// vr_obs.cpp -- OBS Studio's recording from the game (vr_obs.hpp): a small obs-websocket v5 client.
//
// OBS 28 and later ship obs-websocket: Tools > WebSocket Server Settings > Enable WebSocket server (port 4455; with
// Enable Authentication, its password: Show Connect Info). The game connects to vr_obs_host:vr_obs_port (127.0.0.1:4455)
// with vr_obs_password (archived: the config keeps it in plain text; nothing here ever prints it), and asks for the
// recording's state (GetRecordStatus), follows it (the Outputs events: RecordStateChanged) and starts or stops it
// (ToggleRecord). The protocol: github.com/obsproject/obs-websocket, docs/generated/protocol.md: a WebSocket (RFC 6455,
// subprotocol obswebsocket.json) of JSON messages {"op": n, "d": {...}}: Hello (0), Identify (1; its "authentication":
// base64(sha256(base64(sha256(password + salt)) + challenge))), Identified (2), Event (5), Request (6),
// RequestResponse (7); a failed authentication closes with 4009.
//
// When: the thread (a loop that blocks on its socket for its whole life: CODE_STYLE.md, "Threads") starts the first
// time a menu opens with vr_obs on, and asks while a menu is open: at once, then every 5 s until OBS answers. On
// Windows it first looks for OBS's process (obs64.exe, obs32.exe, obs.exe: Toolhelp) on this PC: none, no connection
// tried (a refused one on Windows takes a second or two); OBS running and nothing listening: the WebSocket server is
// off, which the row says. A connection stays open while OBS keeps it (menus closed too): the row knows the state at
// once. A refused password (or none where OBS asks for one) is not tried again on its own (OBS logs each failure):
// a new vr_obs_password, a press on the row, vr_obs_connect or the menu opened again tries once more. The kit's test runs (QVR_TEST_BACKGROUND) never reach for OBS on their own: vr_obs_connect opts in.
// The main thread never waits: poll() hands over the cvars and takes the thread's last state, under a lock held for a
// copy.

#include "vr_obs.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_sha256.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"

extern "C"
{
#include "json.h"
}

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>
using Socket = SOCKET;
constexpr Socket noSocket = INVALID_SOCKET;
#else
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
constexpr Socket noSocket = -1;
#endif

namespace qvr::obs
{
namespace
{

constexpr za::U32 retryMs = 5000;         // between attempts while a menu is open
constexpr za::U32 connectMs = 1500;       // a TCP connection's wait
constexpr za::U32 answerMs = 3000;        // the handshake's and each step's of the identification
constexpr za::U32 statusEveryMs = 2000;   // GetRecordStatus again while a menu is open (the time resynced)
constexpr za::SizeT maxMessage = 1 << 20; // a message bigger than this is not OBS's answer to us: the connection dropped
constexpr double pressRepeat = 1.5;       // seconds a second press does nothing (as the menus' links: vr_menubrand.cpp)
constexpr int closeAuthFailed = 4009;     // obs-websocket's WebSocketCloseCode::AuthenticationFailed

enum class State : int
{
    Off,           // vr_obs 0, or not asked yet
    Absent,        // nothing answers (OBS not running, or not here)
    NoServer,      // OBS is running, its WebSocket server is not (or not on this port)
    NeedPassword,  // the server asks for a password and vr_obs_password is empty
    WrongPassword, // OBS refused vr_obs_password
    Connected,     // identified: the recording's state follows
};

// The cvars, as the thread reads them (copied on the main thread: a cvar's string may be freed).
struct Config
{
    bool enabled{false};
    char host[128]{};
    int port{4455};
    char password[256]{};
    int processCheck{1};
};

// What the thread knows (its own copy, and the main thread's).
struct Status
{
    State state{State::Off};
    bool known{false};  // a GetRecordStatus answered (the recording's state below is OBS's)
    bool active{false}; // recording (paused or not)
    bool paused{false};
    bool starting{false};
    bool stopping{false};
    za::U32 durationMs{0}; // the recording's length when `at` (SDL_GetTicks) read it
    za::U32 at{0};
    char version[24]{}; // obs-websocket's (Hello)
    char detail[160]{}; // the last thing that went wrong, for vr_obs_status
    int attempts{0};
};

// ---------------------------------------------------------------- shared

za::AtomicMutex handoff;
Config sharedConfig;
int sharedConfigSeq = 0;
Status sharedStatus;
int sharedStatusSeq = 0;

za::Thread worker;
za::Atomic<bool> running{false};
za::Atomic<bool> quit{false};
za::Atomic<bool> wanted{false}; // a menu is open (attempts every retryMs)
za::Atomic<bool> kick{false};   // try now (vr_obs_connect, a hint's row pressed)
za::Atomic<int> toggles{0};     // ToggleRecords asked for

// ---------------------------------------------------------------- main thread

Config mainConfig;
int mainConfigSeq = 0;
Status mainStatus;
int seenStatusSeq = 0;
State printedState = State::Off;
bool printedActive = false;
double pressedAt = -10.0;
char statusText[192];

// ---------------------------------------------------------------- the thread's helpers

void base64(const za::U8* data, za::SizeT n, char* out, za::SizeT outSize)
{
    constexpr char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    za::SizeT o = 0;
    for(za::SizeT i = 0; i < n && o + 5 < outSize; i += 3)
    {
        const za::U32 a = data[i], b = i + 1 < n ? data[i + 1] : 0u, c = i + 2 < n ? data[i + 2] : 0u;
        const za::U32 v = (a << 16) | (b << 8) | c;
        out[o++] = digits[(v >> 18) & 63];
        out[o++] = digits[(v >> 12) & 63];
        out[o++] = i + 1 < n ? digits[(v >> 6) & 63] : '=';
        out[o++] = i + 2 < n ? digits[v & 63] : '=';
    }
    out[o] = '\0';
}

// obs-websocket v5's: base64(sha256(base64(sha256(password + salt)) + challenge)).
void authentication(const char* password, const char* salt, const char* challenge, char (&out)[48])
{
    sha256::Hasher h1;
    h1.update(password, strlen(password));
    h1.update(salt, strlen(salt));
    const sha256::Digest d1 = h1.finish();
    char secret[48];
    base64(d1.bytes, sizeof(d1.bytes), secret, sizeof(secret));
    sha256::Hasher h2;
    h2.update(secret, strlen(secret));
    h2.update(challenge, strlen(challenge));
    const sha256::Digest d2 = h2.finish();
    base64(d2.bytes, sizeof(d2.bytes), out, sizeof(out));
}

za::U32 rngState = 0;

[[nodiscard]] za::U32 random32()
{
    if(rngState == 0)
    {
        rngState = static_cast<za::U32>(SDL_GetPerformanceCounter()) | 1u;
    }
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}

[[nodiscard]] bool isLocal(const char* host)
{
    return !q_strcasecmp(host, "127.0.0.1") || !q_strcasecmp(host, "localhost") || !q_strcasecmp(host, "::1");
}

// Whether OBS runs on this PC: 1 yes, 0 no, -1 unknown (not Windows, or the snapshot failed).
[[nodiscard]] int obsRunning()
{
#ifdef _WIN32
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(snap == INVALID_HANDLE_VALUE)
    {
        return -1;
    }
    PROCESSENTRY32W e;
    e.dwSize = sizeof(e);
    int found = 0;
    for(BOOL ok = Process32FirstW(snap, &e); ok && !found; ok = Process32NextW(snap, &e))
    {
        found = !_wcsicmp(e.szExeFile, L"obs64.exe") || !_wcsicmp(e.szExeFile, L"obs32.exe") || !_wcsicmp(e.szExeFile, L"obs.exe");
    }
    CloseHandle(snap);
    return found;
#else
    return -1;
#endif
}

void closeSocket(Socket& s)
{
    if(s == noSocket)
    {
        return;
    }
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
    s = noSocket;
}

[[nodiscard]] bool wouldBlock()
{
#ifdef _WIN32
    const int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
#else
    return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINPROGRESS;
#endif
}

// select() on one socket: 1 ready, 0 timed out, -1 failed.
[[nodiscard]] int waitFor(Socket s, bool write, za::U32 ms)
{
    fd_set set, err;
    FD_ZERO(&set);
    FD_ZERO(&err);
    FD_SET(s, &set);
    FD_SET(s, &err);
    timeval tv;
    tv.tv_sec = static_cast<long>(ms / 1000);
    tv.tv_usec = static_cast<long>((ms % 1000) * 1000);
    const int n = select(static_cast<int>(s) + 1, write ? nullptr : &set, write ? &set : nullptr, &err, &tv);
    if(n < 0 || FD_ISSET(s, &err))
    {
        return -1;
    }
    return n > 0 ? 1 : 0;
}

// ---------------------------------------------------------------- the connection (the thread's)

enum class Got
{
    Message, // a text message: `message`
    Closed,  // the server closed (`closeCode`) or the connection broke
    Timeout,
};

struct Connection
{
    Socket s{noSocket};
    za::Vector<char> in;      // bytes received, not yet taken as frames
    za::Vector<char> partial; // a fragmented message's pieces
    za::Vector<char> message; // the last whole message, NUL-terminated
    za::Vector<za::U8> out;   // a frame being sent
    int closeCode{0};
    int nextRequest{1};
    za::U32 lastStatusAsk{0};
};

Connection conn;

void disconnect()
{
    closeSocket(conn.s);
    conn.in.clear();
    conn.partial.clear();
    conn.closeCode = 0;
}

[[nodiscard]] bool sendAll(const void* data, za::SizeT n)
{
    const char* p = static_cast<const char*>(data);
    const za::U32 t0 = SDL_GetTicks();
    while(n > 0)
    {
        const int sent = static_cast<int>(send(conn.s, p, static_cast<int>(n), 0));
        if(sent > 0)
        {
            p += sent;
            n -= static_cast<za::SizeT>(sent);
            continue;
        }
        if(sent < 0 && wouldBlock() && SDL_GetTicks() - t0 < answerMs && waitFor(conn.s, true, 100) >= 0)
        {
            continue;
        }
        return false;
    }
    return true;
}

// A frame, masked (a client's must be).
[[nodiscard]] bool sendFrame(int opcode, const void* payload, za::SizeT n)
{
    za::Vector<za::U8>& f = conn.out;
    f.clear();
    f.pushBack(static_cast<za::U8>(0x80 | opcode));
    if(n < 126)
    {
        f.pushBack(static_cast<za::U8>(0x80 | n));
    }
    else if(n < 65536)
    {
        f.pushBack(0x80 | 126);
        f.pushBack(static_cast<za::U8>(n >> 8));
        f.pushBack(static_cast<za::U8>(n));
    }
    else
    {
        f.pushBack(0x80 | 127);
        for(int i = 7; i >= 0; i--)
        {
            f.pushBack(static_cast<za::U8>(static_cast<za::U64>(n) >> (8 * i)));
        }
    }
    const za::U32 mask = random32();
    const za::U8 key[4]{static_cast<za::U8>(mask), static_cast<za::U8>(mask >> 8), static_cast<za::U8>(mask >> 16),
        static_cast<za::U8>(mask >> 24)};
    for(const za::U8 k : key)
    {
        f.pushBack(k);
    }
    const za::U8* p = static_cast<const za::U8*>(payload);
    for(za::SizeT i = 0; i < n; i++)
    {
        f.pushBack(static_cast<za::U8>(p[i] ^ key[i & 3]));
    }
    return sendAll(f.data(), f.size());
}

[[nodiscard]] bool sendText(const char* json)
{
    return sendFrame(0x1, json, strlen(json));
}

// Bytes from the socket into conn.in: 1 some, 0 none within `ms`, -1 closed or broken.
[[nodiscard]] int receive(za::U32 ms)
{
    const int ready = waitFor(conn.s, false, ms);
    if(ready <= 0)
    {
        return ready;
    }
    char buf[4096];
    const int n = static_cast<int>(recv(conn.s, buf, sizeof(buf), 0));
    if(n < 0 && wouldBlock())
    {
        return 0;
    }
    if(n <= 0)
    {
        return -1;
    }
    const za::SizeT old = conn.in.size();
    conn.in.resize(old + static_cast<za::SizeT>(n));
    memcpy(conn.in.data() + old, buf, static_cast<size_t>(n));
    return 1;
}

void consume(za::SizeT n)
{
    const za::SizeT rest = conn.in.size() - n;
    if(rest > 0)
    {
        memmove(conn.in.data(), conn.in.data() + n, rest);
    }
    conn.in.resize(rest);
}

// One frame off conn.in: 1 taken (`opcode`, `fin`, its payload appended to `payload`), 0 not all here yet, -1 not a
// frame we take.
[[nodiscard]] int takeFrame(int& opcode, bool& fin, za::Vector<char>& payload)
{
    const za::SizeT have = conn.in.size();
    const za::U8* b = reinterpret_cast<const za::U8*>(conn.in.data());
    if(have < 2)
    {
        return 0;
    }
    fin = (b[0] & 0x80) != 0;
    opcode = b[0] & 0x0f;
    const bool masked = (b[1] & 0x80) != 0;
    za::U64 len = b[1] & 0x7f;
    za::SizeT at = 2;
    if(len == 126)
    {
        if(have < 4)
        {
            return 0;
        }
        len = (static_cast<za::U64>(b[2]) << 8) | b[3];
        at = 4;
    }
    else if(len == 127)
    {
        if(have < 10)
        {
            return 0;
        }
        len = 0;
        for(int i = 0; i < 8; i++)
        {
            len = (len << 8) | b[2 + i];
        }
        at = 10;
    }
    if(len > maxMessage)
    {
        return -1;
    }
    za::U8 key[4]{};
    if(masked)
    {
        if(have < at + 4)
        {
            return 0;
        }
        memcpy(key, b + at, 4);
        at += 4;
    }
    if(have < at + len)
    {
        return 0;
    }
    const za::SizeT old = payload.size();
    payload.resize(old + static_cast<za::SizeT>(len));
    for(za::SizeT i = 0; i < len; i++)
    {
        payload[old + i] = static_cast<char>(masked ? b[at + i] ^ key[i & 3] : b[at + i]);
    }
    consume(at + static_cast<za::SizeT>(len));
    return 1;
}

// The next text message (pings answered, fragments joined), within `ms`.
[[nodiscard]] Got nextMessage(za::U32 ms)
{
    const za::U32 t0 = SDL_GetTicks();
    za::Vector<char> control;
    for(;;)
    {
        for(;;)
        {
            int opcode = 0;
            bool fin = false;
            const bool data = conn.in.size() >= 1 && (conn.in[0] & 0x08) == 0; // (not a control frame)
            za::Vector<char>& into = data ? conn.partial : control;
            control.clear();
            const int r = takeFrame(opcode, fin, into);
            if(r < 0)
            {
                conn.closeCode = 1009;
                return Got::Closed;
            }
            if(r == 0)
            {
                break;
            }
            if(opcode == 0x8)
            {
                const za::U8* p = reinterpret_cast<const za::U8*>(control.data());
                conn.closeCode = control.size() >= 2 ? (p[0] << 8) | p[1] : 1005;
                (void)sendFrame(0x8, control.data(), control.size() >= 2 ? 2 : 0); // (the close answered)
                return Got::Closed;
            }
            if(opcode == 0x9)
            {
                if(!sendFrame(0xA, control.data(), control.size()))
                {
                    return Got::Closed;
                }
                continue;
            }
            if(opcode == 0xA)
            {
                continue;
            }
            if(conn.partial.size() > maxMessage)
            {
                conn.closeCode = 1009;
                return Got::Closed;
            }
            if(fin)
            {
                conn.message.clear();
                conn.message.resize(conn.partial.size() + 1);
                if(!conn.partial.empty())
                {
                    memcpy(conn.message.data(), conn.partial.data(), conn.partial.size());
                }
                conn.message[conn.partial.size()] = '\0';
                conn.partial.clear();
                return Got::Message;
            }
        }
        const za::U32 spent = SDL_GetTicks() - t0;
        if(spent >= ms)
        {
            return Got::Timeout;
        }
        const int r = receive(za::min(ms - spent, 100u));
        if(r < 0)
        {
            conn.closeCode = 1006;
            return Got::Closed;
        }
        if(quit.loadSeqCst())
        {
            return Got::Timeout;
        }
    }
}

// The TCP connection and the WebSocket handshake.
[[nodiscard]] bool openConnection(const Config& cfg, char* detail, za::SizeT detailSize)
{
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char port[16];
    q_snprintf(port, sizeof(port), "%d", cfg.port);
    addrinfo* found = nullptr;
    if(getaddrinfo(cfg.host, port, &hints, &found) != 0 || !found)
    {
        q_snprintf(detail, detailSize, "can't resolve the host \"%s\"", cfg.host);
        return false;
    }
    conn.s = socket(found->ai_family, found->ai_socktype, found->ai_protocol);
    if(conn.s == noSocket)
    {
        freeaddrinfo(found);
        q_snprintf(detail, detailSize, "no socket");
        return false;
    }
#ifdef _WIN32
    u_long nonBlocking = 1;
    ioctlsocket(conn.s, FIONBIO, &nonBlocking);
#else
    fcntl(conn.s, F_SETFL, fcntl(conn.s, F_GETFL, 0) | O_NONBLOCK);
#endif
    int noDelay = 1;
    setsockopt(conn.s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
    const int c = connect(conn.s, found->ai_addr, static_cast<int>(found->ai_addrlen));
    freeaddrinfo(found);
    if(c != 0 && !wouldBlock())
    {
        q_snprintf(detail, detailSize, "nothing listens on %s:%d", cfg.host, cfg.port);
        return false;
    }
    if(c != 0)
    {
        int error = 0;
        socklen_t len = sizeof(error);
        if(waitFor(conn.s, true, connectMs) <= 0 ||
            getsockopt(conn.s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len) != 0 || error != 0)
        {
            q_snprintf(detail, detailSize, "nothing listens on %s:%d", cfg.host, cfg.port);
            return false;
        }
    }

    // The upgrade.
    za::U8 nonce[16];
    for(int i = 0; i < 16; i += 4)
    {
        const za::U32 r = random32();
        memcpy(nonce + i, &r, 4);
    }
    char key[32];
    base64(nonce, sizeof(nonce), key, sizeof(key));
    char request[512];
    q_snprintf(request, sizeof(request),
        "GET / HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\nSec-WebSocket-Protocol: obswebsocket.json\r\n\r\n",
        cfg.host, cfg.port, key);
    if(!sendAll(request, strlen(request)))
    {
        q_snprintf(detail, detailSize, "the connection broke (handshake)");
        return false;
    }
    const za::U32 t0 = SDL_GetTicks();
    for(;;)
    {
        // The response's head: up to its blank line (what follows it is the first frame's).
        const za::SizeT n = conn.in.size();
        for(za::SizeT i = 3; i < n; i++)
        {
            if(conn.in[i - 3] == '\r' && conn.in[i - 2] == '\n' && conn.in[i - 1] == '\r' && conn.in[i] == '\n')
            {
                const bool upgraded = n >= 12 && !strncmp(conn.in.data(), "HTTP/1.1 101", 12);
                consume(i + 1);
                if(!upgraded)
                {
                    q_snprintf(detail, detailSize, "%s:%d answered, but not as a WebSocket server", cfg.host, cfg.port);
                    return false;
                }
                return true;
            }
        }
        if(n > 8192 || SDL_GetTicks() - t0 > answerMs || quit.loadSeqCst())
        {
            q_snprintf(detail, detailSize, "%s:%d did not answer the WebSocket handshake", cfg.host, cfg.port);
            return false;
        }
        if(receive(100) < 0)
        {
            q_snprintf(detail, detailSize, "%s:%d closed the connection (handshake)", cfg.host, cfg.port);
            return false;
        }
    }
}

// The message's {"op": n, "d": {...}}: false if it is not one.
[[nodiscard]] bool parseMessage(json_t*& json, int& op, const jsonentry_t*& d)
{
    json = JSON_Parse(conn.message.data());
    const jsonentry_t* root = json ? json->root : nullptr;
    if(!root || root->type != JSON_OBJECT)
    {
        return false;
    }
    const double* o = JSON_FindNumber(root, "op");
    d = JSON_Find(root, "d", JSON_OBJECT);
    if(!o || !d)
    {
        return false;
    }
    op = static_cast<int>(*o);
    return true;
}

// The thread's state, published to the main thread.
Status status;

void publish()
{
    za::LockGuard lock{handoff};
    sharedStatus = status;
    sharedStatusSeq++;
}

void fail(State s)
{
    disconnect();
    status.state = s;
    status.known = false;
    publish();
}

[[nodiscard]] bool ask(const char* requestType)
{
    char json[160];
    q_snprintf(json, sizeof(json), "{\"op\":6,\"d\":{\"requestType\":\"%s\",\"requestId\":\"qvr%d\"}}", requestType,
        conn.nextRequest++);
    return sendText(json);
}

// Hello, Identify, Identified: the connection's state after (Connected, or why not).
void identify(const Config& cfg, bool obsSeen)
{
    const State unreachable = obsSeen ? State::NoServer : State::Absent;
    if(nextMessage(answerMs) != Got::Message)
    {
        q_snprintf(status.detail, sizeof(status.detail), "no Hello from %s:%d (not obs-websocket 5?)", cfg.host, cfg.port);
        fail(unreachable);
        return;
    }
    json_t* json = nullptr;
    int op = -1;
    const jsonentry_t* d = nullptr;
    char auth[48] = "";
    bool needsAuth = false;
    if(!parseMessage(json, op, d) || op != 0)
    {
        JSON_Free(json);
        q_snprintf(status.detail, sizeof(status.detail), "%s:%d is not obs-websocket 5 (no Hello)", cfg.host, cfg.port);
        fail(unreachable);
        return;
    }
    const char* version = JSON_FindString(d, "obsWebSocketVersion");
    q_strlcpy(status.version, version ? version : "?", sizeof(status.version));
    if(const jsonentry_t* a = JSON_Find(d, "authentication", JSON_OBJECT))
    {
        needsAuth = true;
        const char* challenge = JSON_FindString(a, "challenge");
        const char* salt = JSON_FindString(a, "salt");
        if(cfg.password[0] && challenge && salt)
        {
            authentication(cfg.password, salt, challenge, auth);
        }
    }
    JSON_Free(json);
    if(needsAuth && !cfg.password[0])
    {
        q_snprintf(status.detail, sizeof(status.detail),
            "OBS asks for a password: vr_obs_password (OBS: Tools > WebSocket Server Settings > Show Connect Info)");
        fail(State::NeedPassword);
        return;
    }

    char identify[192];
    if(needsAuth)
    {
        q_snprintf(identify, sizeof(identify),
            "{\"op\":1,\"d\":{\"rpcVersion\":1,\"authentication\":\"%s\",\"eventSubscriptions\":64}}", auth);
    }
    else
    {
        q_snprintf(identify, sizeof(identify), "{\"op\":1,\"d\":{\"rpcVersion\":1,\"eventSubscriptions\":64}}");
    }
    if(!sendText(identify))
    {
        q_snprintf(status.detail, sizeof(status.detail), "the connection broke (Identify)");
        fail(unreachable);
        return;
    }
    const Got got = nextMessage(answerMs);
    if(got == Got::Closed && conn.closeCode == closeAuthFailed)
    {
        q_snprintf(status.detail, sizeof(status.detail), "OBS refused the password (vr_obs_password)");
        fail(State::WrongPassword);
        return;
    }
    op = -1;
    const bool identified = got == Got::Message && parseMessage(json, op, d) && op == 2;
    JSON_Free(json);
    if(!identified)
    {
        q_snprintf(status.detail, sizeof(status.detail), "not identified (%s, close code %d)",
            got == Got::Timeout ? "no answer" : got == Got::Closed ? "closed" : "another message", conn.closeCode);
        fail(unreachable);
        return;
    }
    status.state = State::Connected;
    status.known = false;
    status.detail[0] = '\0';
    publish();
    conn.lastStatusAsk = SDL_GetTicks();
    (void)ask("GetRecordStatus");
}

void attempt(const Config& cfg, int processCheck)
{
    status.attempts++;
    const int seen = processCheck == 2 ? 1 : (processCheck == 1 && isLocal(cfg.host)) ? obsRunning() : -1;
    if(seen == 0)
    {
        q_snprintf(status.detail, sizeof(status.detail), "OBS is not running on this PC (obs64.exe)");
        fail(State::Absent);
        return;
    }
    if(!openConnection(cfg, status.detail, sizeof(status.detail)))
    {
        fail(seen == 1 ? State::NoServer : State::Absent);
        return;
    }
    identify(cfg, seen == 1);
}

// An answer or an event, while connected.
void handle()
{
    json_t* json = nullptr;
    int op = -1;
    const jsonentry_t* d = nullptr;
    if(!parseMessage(json, op, d))
    {
        JSON_Free(json);
        return;
    }
    bool changed = false;
    bool askAgain = false;
    if(op == 5)
    {
        const char* type = JSON_FindString(d, "eventType");
        const jsonentry_t* data = JSON_Find(d, "eventData", JSON_OBJECT);
        if(type && data && !strcmp(type, "RecordStateChanged"))
        {
            const qboolean* active = JSON_FindBoolean(data, "outputActive");
            const char* state = JSON_FindString(data, "outputState");
            if(active)
            {
                status.active = *active;
            }
            if(state)
            {
                status.starting = !strcmp(state, "OBS_WEBSOCKET_OUTPUT_STARTING");
                status.stopping = !strcmp(state, "OBS_WEBSOCKET_OUTPUT_STOPPING");
                if(!strcmp(state, "OBS_WEBSOCKET_OUTPUT_PAUSED"))
                {
                    status.paused = true;
                }
                else if(!strcmp(state, "OBS_WEBSOCKET_OUTPUT_RESUMED") || !strcmp(state, "OBS_WEBSOCKET_OUTPUT_STARTED") ||
                        !strcmp(state, "OBS_WEBSOCKET_OUTPUT_STOPPED"))
                {
                    status.paused = false;
                }
                if(!strcmp(state, "OBS_WEBSOCKET_OUTPUT_STARTED"))
                {
                    status.durationMs = 0;
                    status.at = SDL_GetTicks();
                }
                askAgain = !status.starting && !status.stopping; // (settled: its time and pause read again)
            }
            changed = true;
        }
    }
    else if(op == 7)
    {
        const char* type = JSON_FindString(d, "requestType");
        const jsonentry_t* rs = JSON_Find(d, "requestStatus", JSON_OBJECT);
        const qboolean* ok = rs ? JSON_FindBoolean(rs, "result") : nullptr;
        const jsonentry_t* data = JSON_Find(d, "responseData", JSON_OBJECT);
        if(type && !strcmp(type, "GetRecordStatus") && ok && *ok && data)
        {
            const qboolean* active = JSON_FindBoolean(data, "outputActive");
            const qboolean* paused = JSON_FindBoolean(data, "outputPaused");
            const double* duration = JSON_FindNumber(data, "outputDuration");
            status.active = active && *active;
            status.paused = paused && *paused;
            status.durationMs = duration && *duration > 0.0 ? static_cast<za::U32>(*duration) : 0u;
            status.at = SDL_GetTicks();
            if(!status.active)
            {
                status.starting = status.stopping = false;
            }
            status.known = true;
            changed = true;
        }
        else if(type && !strcmp(type, "ToggleRecord"))
        {
            if(!(ok && *ok))
            {
                const char* comment = rs ? JSON_FindString(rs, "comment") : nullptr;
                q_snprintf(status.detail, sizeof(status.detail), "ToggleRecord failed: %s", comment ? comment : "?");
                changed = true;
                askAgain = true; // (else the events tell)
            }
        }
    }
    JSON_Free(json);
    if(askAgain)
    {
        conn.lastStatusAsk = SDL_GetTicks();
        (void)ask("GetRecordStatus");
    }
    if(changed)
    {
        publish();
    }
}

// Connected: the socket's messages for a moment, the toggles asked for, the status asked for again now and then.
void serve(bool menuOpen)
{
    for(int t = toggles.exchangeSeqCst(0); t > 0; t--)
    {
        if(!ask("ToggleRecord"))
        {
            break;
        }
    }
    if(menuOpen && SDL_GetTicks() - conn.lastStatusAsk >= statusEveryMs)
    {
        conn.lastStatusAsk = SDL_GetTicks();
        (void)ask("GetRecordStatus");
    }
    for(;;)
    {
        const Got got = nextMessage(conn.in.empty() ? 50 : 0);
        if(got == Got::Message)
        {
            handle();
            continue;
        }
        if(got == Got::Closed)
        {
            q_snprintf(status.detail, sizeof(status.detail), "OBS closed the connection (code %d)", conn.closeCode);
            fail(State::Absent); // (OBS quit, or its server stopped: the next attempt says which)
        }
        return;
    }
}

void run()
{
#ifdef _WIN32
    WSADATA wsa;
    const bool wsaUp = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#endif
    const bool tests = getenv("QVR_TEST_BACKGROUND") != nullptr;
    bool optedIn = !tests;
    Config cfg;
    int cfgSeq = -1;
    za::U32 nextAttempt = 0;
    bool held = false;         // a refused password: not tried again on its own (OBS logs, and may announce, each)
    bool menuWasOpen = false;
    while(!quit.loadSeqCst())
    {
        bool reconnect = false;
        {
            za::LockGuard lock{handoff};
            if(cfgSeq != sharedConfigSeq)
            {
                reconnect = cfgSeq >= 0 && (strcmp(cfg.host, sharedConfig.host) || cfg.port != sharedConfig.port ||
                                               strcmp(cfg.password, sharedConfig.password));
                cfg = sharedConfig;
                cfgSeq = sharedConfigSeq;
            }
        }
        if(reconnect)
        {
            disconnect();
            status.known = false;
            held = false;
            nextAttempt = SDL_GetTicks(); // (at once, with the new address or password)
        }
        if(!cfg.enabled)
        {
            if(conn.s != noSocket || status.state != State::Off)
            {
                status.detail[0] = '\0';
                fail(State::Off);
            }
            kick.storeSeqCst(false);
            toggles.storeSeqCst(0);
            SDL_Delay(50);
            continue;
        }
        const bool menuOpen = wanted.loadSeqCst();
        if(menuOpen && !menuWasOpen)
        {
            held = false; // (a menu opened again: one more try)
        }
        menuWasOpen = menuOpen;
        if(conn.s == noSocket)
        {
            const bool kicked = kick.exchangeSeqCst(false);
            optedIn = optedIn || kicked;
            const za::U32 now = SDL_GetTicks();
            if(!kicked && (held || !(optedIn && menuOpen && static_cast<za::I32>(now - nextAttempt) >= 0)))
            {
                SDL_Delay(50);
                continue;
            }
            attempt(cfg, cfg.processCheck);
            held = status.state == State::WrongPassword || status.state == State::NeedPassword;
            nextAttempt = SDL_GetTicks() + retryMs;
            continue;
        }
        kick.storeSeqCst(false);
        serve(menuOpen);
        if(conn.s == noSocket)
        {
            nextAttempt = SDL_GetTicks() + 1000; // (lost: soon again)
        }
    }
    disconnect();
#ifdef _WIN32
    if(wsaUp)
    {
        WSACleanup();
    }
#endif
    running.storeSeqCst(false);
}

// ---------------------------------------------------------------- the main thread's

void startWorker()
{
    if(running.loadSeqCst() || worker.joinable())
    {
        return;
    }
    quit.storeSeqCst(false);
    running.storeSeqCst(true);
    worker = za::Thread(run);
}

// The cvars to the thread, when one changed.
void syncConfig()
{
    const bool enabled = vr_obs.value != 0.f;
    const char* host = vr_obs_host.string && vr_obs_host.string[0] ? vr_obs_host.string : "127.0.0.1";
    const char* password = vr_obs_password.string ? vr_obs_password.string : "";
    const int port = static_cast<int>(vr_obs_port.value);
    const int check = static_cast<int>(vr_obs_process_check.value);
    Config& c = mainConfig;
    if(c.enabled == enabled && c.port == port && c.processCheck == check && !strcmp(c.host, host) &&
        !strcmp(c.password, password) && mainConfigSeq > 0)
    {
        return;
    }
    c.enabled = enabled;
    c.port = port;
    c.processCheck = check;
    q_strlcpy(c.host, host, sizeof(c.host));
    q_strlcpy(c.password, password, sizeof(c.password));
    mainConfigSeq++;
    za::LockGuard lock{handoff};
    sharedConfig = c;
    sharedConfigSeq = mainConfigSeq;
}

[[nodiscard]] bool connectedKnown()
{
    return mainStatus.state == State::Connected && mainStatus.known;
}

// The recording's length now (its last reading, plus the time since while it runs).
[[nodiscard]] za::U32 elapsedMs()
{
    const Status& s = mainStatus;
    za::U32 ms = s.durationMs;
    if(s.active && !s.paused && !s.starting)
    {
        ms += SDL_GetTicks() - s.at;
    }
    return ms;
}

void formatTime(za::U32 ms, char (&out)[16])
{
    const za::U32 t = ms / 1000;
    q_snprintf(out, sizeof(out), "%02u:%02u:%02u", t / 3600, (t / 60) % 60, t % 60);
}

[[nodiscard]] const char* stateName(State s)
{
    switch(s)
    {
        case State::Off: return "off";
        case State::Absent: return "not found";
        case State::NoServer: return "OBS running, its WebSocket server off";
        case State::NeedPassword: return "password needed";
        case State::WrongPassword: return "wrong password";
        case State::Connected: return "connected";
    }
    return "?";
}

void status_f()
{
    const Status& s = mainStatus;
    Con_Printf("obs: %s (vr_obs %g), %s:%d, password %s, process check %d\n", stateName(s.state), vr_obs.value,
        mainConfig.host, mainConfig.port, mainConfig.password[0] ? "set" : "not set", mainConfig.processCheck);
    if(s.state == State::Connected)
    {
        char t[16];
        formatTime(elapsedMs(), t);
        Con_Printf("obs: obs-websocket %s; %s%s%s %s\n", s.version, !s.known ? "state not known yet" : !s.active ? "not recording" : "recording",
            s.paused ? " (paused)" : "", s.starting ? " (starting)" : s.stopping ? " (stopping)" : "", s.active ? t : "");
    }
    if(s.detail[0])
    {
        Con_Printf("obs: %s\n", s.detail);
    }
    const Banner b = banner();
    Con_Printf("obs: menu row %s%s%s (%d attempts)\n", b.shown ? "\"" : "hidden", b.shown ? b.text : "", b.shown ? "\"" : "",
        s.attempts);
}

void toggle_f()
{
    if(!vr_obs.value)
    {
        Con_Printf("vr_obs_toggle: vr_obs is 0\n");
        return;
    }
    if(!connectedKnown())
    {
        Con_Printf("vr_obs_toggle: not connected to OBS (%s): connecting\n", stateName(mainStatus.state));
        startWorker();
        kick.storeSeqCst(true);
        return;
    }
    toggles.fetchAddSeqCst(1);
    Con_Printf("vr_obs_toggle: %s recording\n", mainStatus.active ? "stopping" : "starting");
}

void connect_f()
{
    if(!vr_obs.value)
    {
        Con_Printf("vr_obs_connect: vr_obs is 0\n");
        return;
    }
    syncConfig();
    startWorker();
    kick.storeSeqCst(true);
    Con_Printf("vr_obs_connect: %s %s:%d\n", mainStatus.state == State::Connected ? "connected to" : "connecting to",
        mainConfig.host, mainConfig.port);
}

} // namespace

// ---------------------------------------------------------------- the API

void registerCommands()
{
    Cmd_AddCommand("vr_obs_status", status_f);
    Cmd_AddCommand("vr_obs_toggle", toggle_f);
    Cmd_AddCommand("vr_obs_connect", connect_f);
}

void poll()
{
    const bool menuOpen = key_dest == key_menu && m_state != m_none;
    if(!vr_obs.value && !worker.joinable())
    {
        return; // (never started: nothing to tell it)
    }
    syncConfig();
    wanted.storeSeqCst(menuOpen);
    if(menuOpen && vr_obs.value)
    {
        startWorker();
    }
    {
        za::LockGuard lock{handoff};
        if(seenStatusSeq != sharedStatusSeq)
        {
            seenStatusSeq = sharedStatusSeq;
            mainStatus = sharedStatus;
        }
    }
    const Status& s = mainStatus;
    if(s.state != printedState || (connectedKnown() && s.active != printedActive))
    {
        Con_DPrintf("obs: %s%s\n", stateName(s.state), !connectedKnown() ? "" : s.active ? ", recording" : ", not recording");
        printedState = s.state;
        printedActive = s.active;
    }
}

void finish()
{
    if(!worker.joinable())
    {
        return;
    }
    quit.storeSeqCst(true);
    const za::U32 t0 = SDL_GetTicks();
    while(running.loadSeqCst() && SDL_GetTicks() - t0 < 3000)
    {
        SDL_Delay(10);
    }
    if(running.loadSeqCst())
    {
        Sys_Printf("obs: the connection's thread did not stop in 3 s; quitting without it\n");
        worker.detach();
    }
    else
    {
        worker.join();
    }
}

Banner banner()
{
    Banner b;
    const Status& s = mainStatus;
    if(!vr_obs.value)
    {
        return b;
    }
    switch(s.state)
    {
        case State::Off:
        case State::Absent: return b;
        case State::NoServer:
            q_strlcpy(b.text, "OBS found: enable its WebSocket server", sizeof(b.text));
            q_strlcpy(b.shortText, "OBS: WebSocket off", sizeof(b.shortText));
            break;
        case State::NeedPassword:
            q_strlcpy(b.text, "OBS: password needed", sizeof(b.text));
            q_strlcpy(b.shortText, "OBS: password needed", sizeof(b.shortText));
            break;
        case State::WrongPassword:
            q_strlcpy(b.text, "OBS: wrong password", sizeof(b.text));
            q_strlcpy(b.shortText, "OBS: wrong password", sizeof(b.shortText));
            break;
        case State::Connected:
        {
            if(!s.known)
            {
                return b;
            }
            char t[16];
            formatTime(elapsedMs(), t);
            if(s.starting)
            {
                q_strlcpy(b.text, "OBS: Starting...", sizeof(b.text));
            }
            else if(s.stopping)
            {
                q_strlcpy(b.text, "OBS: Stopping...", sizeof(b.text));
            }
            else if(s.active && s.paused)
            {
                q_snprintf(b.text, sizeof(b.text), "OBS: Paused %s", t);
            }
            else if(s.active)
            {
                q_snprintf(b.text, sizeof(b.text), "OBS: Recording %s", t);
            }
            else
            {
                q_strlcpy(b.text, "OBS: Not recording", sizeof(b.text));
            }
            if(s.active && !s.paused && !s.starting && !s.stopping)
            {
                q_snprintf(b.shortText, sizeof(b.shortText), "OBS: REC %s", t);
            }
            else
            {
                q_strlcpy(b.shortText, b.text, sizeof(b.shortText));
            }
            b.recording = s.active;
            b.paused = s.paused;
            break;
        }
    }
    b.shown = true;
    return b;
}

bool press()
{
    if(!banner().shown)
    {
        return false;
    }
    if(realtime - pressedAt < pressRepeat)
    {
        return true; // (taken: the last press is on its way)
    }
    pressedAt = realtime;
    S_LocalSound("misc/menu2.wav");
    if(connectedKnown())
    {
        toggles.fetchAddSeqCst(1);
        Con_DPrintf("obs: the menu's row pressed: %s recording\n", mainStatus.active ? "stopping" : "starting");
    }
    else
    {
        kick.storeSeqCst(true); // (a hint's row: asked again now)
        Con_DPrintf("obs: the menu's row pressed: asking OBS again\n");
    }
    return true;
}

const char* statusLine()
{
    const Status& s = mainStatus;
    if(!vr_obs.value)
    {
        q_strlcpy(statusText, "OBS: off (OBS Recording Control above)", sizeof(statusText));
    }
    else if(connectedKnown())
    {
        const Banner b = banner();
        q_snprintf(statusText, sizeof(statusText), "%s (obs-websocket %s)", b.text, s.version);
    }
    else if(s.state == State::Connected)
    {
        q_strlcpy(statusText, "OBS: connected", sizeof(statusText));
    }
    else if(s.state == State::Off)
    {
        q_strlcpy(statusText, "OBS: not asked yet (asked while a menu is open)", sizeof(statusText));
    }
    else if(s.state == State::Absent)
    {
        q_strlcpy(statusText, "OBS: not found (not running, or its WebSocket server off)", sizeof(statusText));
    }
    else
    {
        q_snprintf(statusText, sizeof(statusText), "OBS: %s", stateName(s.state));
    }
    return statusText;
}

} // namespace qvr::obs
