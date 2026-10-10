// vr_doom.cpp -- see vr_doom.hpp; docs/vr-port/DOOM.md.

#include "vr_doom.hpp"

#include "vr_bullettime.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_gadget.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_touch.hpp"
#include "vr_units.hpp"

#include "doom/qvr_doom_api.h"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <stdio.h>
#include <string.h>


#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace qvr::doom
{
namespace
{

// ---- The DLL ---------------------------------------------------------------------------------------------------------

struct Api
{
#ifdef _WIN32
    HMODULE library{nullptr};
#endif
    qvr_doom_version_fn version{nullptr};
    qvr_doom_init_fn init{nullptr};
    qvr_doom_tick_fn tick{nullptr};
    qvr_doom_key_fn key{nullptr};
    qvr_doom_frame_fn frame{nullptr};
    qvr_doom_get_status_fn status{nullptr};
    qvr_doom_set_fn set{nullptr};
    qvr_doom_type_fn type{nullptr};
    qvr_doom_menu_tap_fn menuTap{nullptr};
    qvr_doom_error_fn error{nullptr};
    qvr_doom_shutdown_fn shutdown{nullptr};
};
Api api;

bool loadLibrary()
{
#ifdef _WIN32
    if(api.library)
    {
        return true;
    }
    api.library = LoadLibraryA("qvr_doom.dll");
    if(!api.library)
    {
        Con_Printf("Doom: qvr_doom.dll not found beside the game (error %lu)\n", GetLastError());
        return false;
    }
    const auto get = [](const char* name) { return reinterpret_cast<void*>(GetProcAddress(api.library, name)); };
    api.version = reinterpret_cast<qvr_doom_version_fn>(get("qvr_doom_version"));
    api.init = reinterpret_cast<qvr_doom_init_fn>(get("qvr_doom_init"));
    api.tick = reinterpret_cast<qvr_doom_tick_fn>(get("qvr_doom_tick"));
    api.key = reinterpret_cast<qvr_doom_key_fn>(get("qvr_doom_key"));
    api.frame = reinterpret_cast<qvr_doom_frame_fn>(get("qvr_doom_frame"));
    api.status = reinterpret_cast<qvr_doom_get_status_fn>(get("qvr_doom_get_status"));
    api.set = reinterpret_cast<qvr_doom_set_fn>(get("qvr_doom_set"));
    api.type = reinterpret_cast<qvr_doom_type_fn>(get("qvr_doom_type"));
    api.menuTap = reinterpret_cast<qvr_doom_menu_tap_fn>(get("qvr_doom_menu_tap"));
    api.error = reinterpret_cast<qvr_doom_error_fn>(get("qvr_doom_error"));
    api.shutdown = reinterpret_cast<qvr_doom_shutdown_fn>(get("qvr_doom_shutdown"));
    if(!api.version || api.version() != QVR_DOOM_API_VERSION || !api.init || !api.tick || !api.key || !api.frame ||
        !api.status || !api.set || !api.type || !api.menuTap || !api.error || !api.shutdown)
    {
        Con_Printf("Doom: qvr_doom.dll is another version than this game's\n");
        FreeLibrary(api.library);
        api = Api{};
        return false;
    }
    return true;
#else
    Con_Printf("Doom: only on Windows\n");
    return false;
#endif
}

void unloadLibrary()
{
#ifdef _WIN32
    if(api.library)
    {
        FreeLibrary(api.library);
    }
#endif
    api = Api{};
}

// ---- Folders -----------------------------------------------------------------------------------------------------

// quakevr/doom/ (where the Freedoom WADs ship; Doom's settings and saves): the last base folder that has it, else the
// first's (made).
za::String doomFolder()
{
    for(int i = com_numbasedirs - 1; i >= 0; i--)
    {
        char path[MAX_OSPATH];
        q_snprintf(path, sizeof(path), "%s/quakevr/doom", com_basedirs[i]);
        if(files::isDirectory(path))
        {
            return za::String{path};
        }
    }
    char path[MAX_OSPATH];
    q_snprintf(path, sizeof(path), "%s/quakevr/doom", com_numbasedirs > 0 ? com_basedirs[0] : ".");
    (void)files::createDirectories(path);
    return za::String{path};
}

// ---- MD5 (the official IWADs are known by theirs) ----------------------------------------------------------------

struct Md5
{
    uint32_t h[4]{0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};
    unsigned char block[64]{};
    uint64_t length{0};
    int used{0};

    static uint32_t rotl(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

    void compress(const unsigned char* p)
    {
        static constexpr uint32_t k[64] = {0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
            0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e,
            0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
            0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942,
            0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
            0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97, 0xab9423a7,
            0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
            0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
        static constexpr int r[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14,
            20, 5, 9, 14, 20, 5, 9, 14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6,
            10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
        uint32_t w[16];
        for(int i = 0; i < 16; i++)
        {
            w[i] = static_cast<uint32_t>(p[i * 4]) | static_cast<uint32_t>(p[i * 4 + 1]) << 8 |
                   static_cast<uint32_t>(p[i * 4 + 2]) << 16 | static_cast<uint32_t>(p[i * 4 + 3]) << 24;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        for(int i = 0; i < 64; i++)
        {
            uint32_t f;
            int g;
            if(i < 16) { f = (b & c) | (~b & d); g = i; }
            else if(i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if(i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
            else { f = c ^ (b | ~d); g = (7 * i) % 16; }
            const uint32_t t = d;
            d = c;
            c = b;
            b = b + rotl(a + f + k[i] + w[g], r[i]);
            a = t;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
    }

    void add(const unsigned char* p, size_t n)
    {
        length += n;
        while(n > 0)
        {
            const size_t take = za::min(n, static_cast<size_t>(64 - used));
            memcpy(block + used, p, take);
            used += static_cast<int>(take);
            p += take;
            n -= take;
            if(used == 64)
            {
                compress(block);
                used = 0;
            }
        }
    }

    void hex(char (&out)[33])
    {
        const uint64_t bits = length * 8;
        const unsigned char one = 0x80, zero = 0;
        add(&one, 1);
        while(used != 56)
        {
            add(&zero, 1);
        }
        unsigned char tail[8];
        for(int i = 0; i < 8; i++)
        {
            tail[i] = static_cast<unsigned char>(bits >> (8 * i));
        }
        add(tail, 8);
        for(int i = 0; i < 16; i++)
        {
            q_snprintf(out + i * 2, 3, "%02x", (h[i / 4] >> (8 * (i % 4))) & 0xff);
        }
    }
};

// ---- The WADs ----------------------------------------------------------------------------------------------------

enum Game : int
{
    GameDoom,        // DOOM.WAD, DOOM1.WAD (the shareware episode), Freedoom Phase 1
    GameDoom2,       // DOOM2.WAD, Freedoom Phase 2
    GameFinal,       // TNT.WAD, PLUTONIA.WAD
};

struct Known
{
    const char* md5;
    const char* name;
    Game game;
};
constexpr Known knownWads[] = {
    {"f0cefca49926d00903cf57551d901abe", "DOOM Shareware 1.9", GameDoom},
    {"1cd63c5ddff1bf8ce844237f580e9cf3", "DOOM 1.9", GameDoom},
    {"c4fe9fd920207691a9f493668e0a2083", "The Ultimate DOOM 1.9", GameDoom},
    {"fb35c4a5a9fd49ec29ab6e900572c524", "The Ultimate DOOM (BFG Edition)", GameDoom},
    {"25e1459ca71d321525f84628f45ca8cd", "DOOM II 1.9", GameDoom2},
    {"c3bea40570c23e511a7ed3ebcd9865f7", "DOOM II (BFG Edition)", GameDoom2},
    {"4e158d9953c79ccf97bd0663244cc6b6", "Final DOOM: TNT Evilution", GameFinal},
    {"1d39e405bf6ee3df69a8d2646c8d5c49", "Final DOOM: TNT Evilution (id Anthology)", GameFinal},
    {"75c8cf89566741fa9d22447604053bd7", "Final DOOM: The Plutonia Experiment", GameFinal},
    {"3493be7e1e2588bc9c8b31eab2587a04", "Final DOOM: The Plutonia Experiment (id Anthology)", GameFinal},
};

// The IWADs' file names Doom knows (the stores' folders are searched for these only).
constexpr const char* iwadNames[] = {"doom.wad", "doom1.wad", "doom2.wad", "doomu.wad", "tnt.wad", "plutonia.wad",
    "freedoom1.wad", "freedoom2.wad"};

struct Wad
{
    za::String path;
    za::String name;   // "The Ultimate DOOM 1.9", "Freedoom: Phase 1"
    za::String source; // "bundled", "drop-in folder", "Steam", "GOG", "vr_doom_wad_dir"
    Game game{GameDoom};
    bool official{false}; // id's (known by its MD5, or an IWAD with id's file name)
    bool known{false};    // its MD5 is a known release's
    bool freedoom{false};
    bool failed{false};   // Doom stopped with an error on it this session (an automatic pick skips it)
};

struct Catalog
{
    za::Vector<Wad> wads;
    bool scanned{false};
};
Catalog catalog;

[[nodiscard]] bool endsWithWad(const char* name)
{
    const size_t n = strlen(name);
    return n > 4 && !q_strcasecmp(name + n - 4, ".wad");
}

[[nodiscard]] bool knownName(const char* name)
{
    for(const char* k : iwadNames)
    {
        if(!q_strcasecmp(name, k))
        {
            return true;
        }
    }
    return false;
}

// An IWAD's header, and whether it has MAP01 (DOOM II's levels) rather than E1M1.
[[nodiscard]] bool readIwad(const char* path, bool& map01)
{
    FILE* f = fopen(path, "rb");
    if(!f)
    {
        return false;
    }
    unsigned char header[12];
    bool ok = fread(header, 1, 12, f) == 12 && !memcmp(header, "IWAD", 4);
    map01 = false;
    if(ok)
    {
        const int count = header[4] | header[5] << 8 | header[6] << 16 | header[7] << 24;
        const long dir = header[8] | header[9] << 8 | header[10] << 16 | static_cast<long>(header[11]) << 24;
        ok = count > 0 && count < 100000 && fseek(f, dir, SEEK_SET) == 0;
        for(int i = 0; ok && i < count; i++)
        {
            unsigned char entry[16];
            if(fread(entry, 1, 16, f) != 16)
            {
                break;
            }
            if(!memcmp(entry + 8, "MAP01", 5))
            {
                map01 = true;
                break;
            }
        }
    }
    fclose(f);
    return ok;
}

void md5File(const char* path, char (&out)[33])
{
    out[0] = 0;
    FILE* f = fopen(path, "rb");
    if(!f)
    {
        return;
    }
    Md5 m;
    unsigned char buffer[1 << 16];
    for(;;)
    {
        const size_t n = fread(buffer, 1, sizeof(buffer), f);
        if(n == 0)
        {
            break;
        }
        m.add(buffer, n);
    }
    fclose(f);
    m.hex(out);
}

void addWad(const char* path, const char* fileName, const char* source, bool anyName)
{
    if(!endsWithWad(fileName) || (!anyName && !knownName(fileName)))
    {
        return;
    }
    const za::String full = files::generic(files::absolute(path));
    for(const Wad& w : catalog.wads)
    {
        if(!q_strcasecmp(w.path.cStr(), full.cStr()))
        {
            return;
        }
    }
    bool map01 = false;
    if(!readIwad(full.cStr(), map01))
    {
        return; // (a PWAD, or not a WAD)
    }
    Wad w;
    w.path = full;
    w.source = source;
    w.game = map01 ? GameDoom2 : GameDoom;
    if(!q_strcasecmp(fileName, "freedoom1.wad") || !q_strcasecmp(fileName, "freedoom2.wad"))
    {
        w.freedoom = true;
        w.name = map01 ? "Freedoom: Phase 2" : "Freedoom: Phase 1";
    }
    else
    {
        char md5[33];
        md5File(full.cStr(), md5);
        for(const Known& k : knownWads)
        {
            if(!strcmp(md5, k.md5))
            {
                w.known = w.official = true;
                w.name = k.name;
                w.game = k.game;
            }
        }
        if(!w.known)
        {
            // An IWAD of another version (the 2024 re-release's, a German edition, a mod's own): by its file name.
            w.official = knownName(fileName);
            char name[160];
            q_snprintf(name, sizeof(name), "%s (%s, an unlisted version)", fileName,
                map01 ? "DOOM II's levels" : "DOOM's levels");
            w.name = name;
        }
    }
    catalog.wads.pushBack(w);
}

// The WADs in `dir` (and its subfolders, `depth` deep).
void scanFolder(const char* dir, const char* source, bool anyName, int depth)
{
    if(!files::isDirectory(dir))
    {
        return;
    }
    files::forEachEntry(dir, [&](const char* name, bool isDirectory) {
        char path[MAX_OSPATH];
        q_snprintf(path, sizeof(path), "%s/%s", dir, name);
        if(isDirectory)
        {
            if(depth > 0 && name[0] != '.')
            {
                scanFolder(path, source, anyName, depth - 1);
            }
        }
        else
        {
            addWad(path, name, source, anyName);
        }
    });
}

#ifdef _WIN32
// GOG's games (its registry: each game's "path"), whose name has DOOM in it (not DOOM 3, 64 or later: their files are
// no IWADs of this Doom's anyway, and only the known IWAD names are taken).
void scanGog()
{
    HKEY games;
    if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\GOG.com\\Games", 0, KEY_READ, &games) != ERROR_SUCCESS)
    {
        return;
    }
    for(DWORD i = 0;; i++)
    {
        char id[128];
        DWORD idLength = sizeof(id);
        if(RegEnumKeyExA(games, i, id, &idLength, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
        {
            break;
        }
        HKEY game;
        if(RegOpenKeyExA(games, id, 0, KEY_READ, &game) != ERROR_SUCCESS)
        {
            continue;
        }
        char name[256] = "", path[MAX_PATH] = "";
        DWORD nameLength = sizeof(name) - 1, pathLength = sizeof(path) - 1;
        const bool hasName = RegQueryValueExA(game, "gameName", nullptr, nullptr, reinterpret_cast<BYTE*>(name),
                                 &nameLength) == ERROR_SUCCESS;
        const bool hasPath = RegQueryValueExA(game, "path", nullptr, nullptr, reinterpret_cast<BYTE*>(path),
                                 &pathLength) == ERROR_SUCCESS;
        RegCloseKey(game);
        if(hasName && hasPath && q_strcasestr(name, "doom"))
        {
            scanFolder(path, "GOG", false, 3);
        }
    }
    RegCloseKey(games);
}
#endif

// Steam's: The Ultimate DOOM / DOOM + DOOM II (app 2280, "Ultimate Doom": base/, rerelease/), DOOM II (2300, "Doom 2"),
// Final DOOM (2290, "Final Doom"), in each of its libraries (the registry's SteamPath, its
// steamapps/libraryfolders.vdf's "path"s).
void scanSteam()
{
#ifdef _WIN32
    char steam[MAX_PATH] = "";
    DWORD length = sizeof(steam) - 1;
    if(RegGetValueA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath", RRF_RT_REG_SZ, nullptr, steam, &length) !=
        ERROR_SUCCESS)
    {
        return;
    }
    za::Vector<za::String> libraries;
    libraries.pushBack(za::String{steam});
    char vdf[MAX_OSPATH];
    q_snprintf(vdf, sizeof(vdf), "%s/steamapps/libraryfolders.vdf", steam);
    za::String text;
    if(files::readText(vdf, text))
    {
        const char* p = text.cStr();
        while((p = strstr(p, "\"path\"")) != nullptr)
        {
            p += 6;
            const char* open = strchr(p, '"');
            if(!open)
            {
                break;
            }
            za::String path;
            const char* c = open + 1;
            for(; *c && *c != '"'; c++)
            {
                if(c[0] == '\\' && c[1] == '\\')
                {
                    c++;
                }
                path += *c;
            }
            libraries.pushBack(path);
            p = *c ? c + 1 : c;
        }
    }
    constexpr const char* folders[] = {"Ultimate Doom", "Doom 2", "Final Doom"};
    for(const za::String& library : libraries)
    {
        for(const char* folder : folders)
        {
            char path[MAX_OSPATH];
            q_snprintf(path, sizeof(path), "%s/steamapps/common/%s", library.cStr(), folder);
            scanFolder(path, "Steam", false, 3);
        }
    }
#endif
}

// The order an automatic pick takes: id's known releases (DOOM before DOOM II before Final DOOM; the full games before
// the shareware episode), then id's other versions, then Freedoom (Phase 1 first).
[[nodiscard]] int rank(const Wad& w)
{
    const bool shareware = q_strcasestr(w.name.cStr(), "Shareware") != nullptr;
    const int game = static_cast<int>(w.game);
    if(w.known)
    {
        return (shareware ? 10 : 0) + game;
    }
    if(w.official)
    {
        return 20 + game;
    }
    return w.freedoom ? 40 + game : 60;
}

void scan()
{
    catalog.wads.clear();
    const za::String folder = doomFolder();
    char path[MAX_OSPATH];
    q_snprintf(path, sizeof(path), "%s/wads", folder.cStr());
    (void)files::createDirectories(path);
    scanFolder(path, "drop-in folder", true, 1);
    if(vr_doom_wad_dir.string && vr_doom_wad_dir.string[0])
    {
        scanFolder(vr_doom_wad_dir.string, "vr_doom_wad_dir", true, 1);
    }
    scanSteam();
#ifdef _WIN32
    scanGog();
#endif
    scanFolder(folder.cStr(), "bundled", false, 0);
    // (Sorted by rank, stable: insertion.)
    for(za::SizeT i = 1; i < catalog.wads.size(); i++)
    {
        for(za::SizeT j = i; j > 0 && rank(catalog.wads[j]) < rank(catalog.wads[j - 1]); j--)
        {
            Wad t = catalog.wads[j];
            catalog.wads[j] = catalog.wads[j - 1];
            catalog.wads[j - 1] = t;
        }
    }
    catalog.scanned = true;
}

void ensureScanned()
{
    if(!catalog.scanned)
    {
        scan();
    }
}

// The WAD to start: vr_doom_wad's (a path, or a file name in the catalogue) when it is there, else the best ranked
// that hasn't failed. -1: none.
[[nodiscard]] int chosenWad()
{
    ensureScanned();
    const char* want = vr_doom_wad.string;
    if(want && want[0])
    {
        for(za::SizeT i = 0; i < catalog.wads.size(); i++)
        {
            const Wad& w = catalog.wads[i];
            if(!q_strcasecmp(w.path.cStr(), want) || !q_strcasecmp(files::fileName(w.path).data(), want))
            {
                return static_cast<int>(i);
            }
        }
    }
    for(za::SizeT i = 0; i < catalog.wads.size(); i++)
    {
        if(!catalog.wads[i].failed)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// ---- The session -------------------------------------------------------------------------------------------------

constexpr int doomEntity = -0x5C20; // its sounds' (the gadget's own keys are -0x5C1x)
constexpr int screenHeight = 200;   // the overlay's virtual screen: 200 high, as wide as the screen's aspect

struct Session
{
    bool running{false};      // Doom started (the DLL loaded)
    bool open{false};         // shown (the gadget's screen, the panel)
    bool focused{false};
    int wad{-1};              // catalog index running
    double startedAt{0.0};    // realtime
    double lastTick{-1.0};    // realtime of the last frame counted
    float lookTime{0.f}, awayTime{0.f};
    qvr_doom_status status{};
    unsigned frameShown{~0u};
    float tickMs{0.f}, tickMsMax{0.f}; // the DLL's tick (smoothed), and the most lately
    float renderMs{0.f};
    // Keys: what is held in Doom now, and what this frame wants held (the controller's, the touch's, pulses).
    bool held[256]{};
    bool want[256]{};
    double pulseUntil[256]{};
    // The controller (the gadget hand's), as last seen while it had the focus.
    HandInput pad{};
    bool padValid{false};
    double repeatAt[4]{}; // the stick's menu arrows' next repeat
    bool stickDir[4]{};   // up, down, left, right held
    // Touch.
    touch::Ui ui;
    bool fireLock{false};
    double lastTouch{-10.0};
    // The screen.
    float aspect{3.f / 1.86f}; // the screen's (width / height)
    // The floating panel: its place in the play space (from the player's origin and lean, turned by turnYaw then).
    bool panelPlaced{false};
    glm::vec3 panelOffset{0.f};
    float panelYaw{0.f};
    float panelTurn{0.f};
};
Session session;

gfx::Picture picture;   // Doom's 320 x 200 frame (nearest filtering)
gfx::Target screen;     // the overlay drawn over it, the screen's texture (3x)

// ---- Sounds: Doom's effects as sounds of the engine at the gadget -------------------------------------------------

struct SoundCache
{
    za::Vector<za::String> names;               // "doom/<lump>.wav" names made
    za::Vector<za::Vector<unsigned char>> wavs; // and their WAV files
    int generation{0};                          // a new Doom start's sounds are new sounds (another WAD's)
    int channelSep[16]{};
    int channelVolume[16]{};
};
SoundCache sounds;

// The DMX lump as a WAV file (8-bit unsigned mono, its rate).
bool makeWav(const unsigned char* dmx, int length, za::Vector<unsigned char>& out)
{
    if(length < 8 + 32 || dmx[0] != 3 || dmx[1] != 0)
    {
        return false;
    }
    const int rate = dmx[2] | dmx[3] << 8;
    int count = dmx[4] | dmx[5] << 8 | dmx[6] << 16 | dmx[7] << 24;
    if(count > length - 8 || count <= 32)
    {
        return false;
    }
    count -= 32; // (16 bytes of padding each end)
    const unsigned char* samples = dmx + 8 + 16;
    out.resize(static_cast<za::SizeT>(44 + count));
    unsigned char* p = out.data();
    const auto u32 = [&](uint32_t v) {
        for(int i = 0; i < 4; i++)
        {
            *p++ = static_cast<unsigned char>(v >> (8 * i));
        }
    };
    const auto u16 = [&](uint32_t v) {
        *p++ = static_cast<unsigned char>(v);
        *p++ = static_cast<unsigned char>(v >> 8);
    };
    memcpy(p, "RIFF", 4), p += 4;
    u32(static_cast<uint32_t>(36 + count));
    memcpy(p, "WAVEfmt ", 8), p += 8;
    u32(16);
    u16(1);
    u16(1);
    u32(static_cast<uint32_t>(rate));
    u32(static_cast<uint32_t>(rate));
    u16(1);
    u16(8);
    memcpy(p, "data", 4), p += 4;
    u32(static_cast<uint32_t>(count));
    memcpy(p, samples, static_cast<size_t>(count));
    return true;
}

// Where Doom's sound on a channel plays: the screen (the gadget's, or the panel), a little to the side by its
// separation (Doom's stereo: a small speaker's width).
bool soundOrigin(int channel, glm::vec3& out)
{
    touch::Surface z;
    bool have = false;
    if(vr_doom_panel.value != 0.f && session.panelPlaced)
    {
        have = touch::lastSurface(z);
    }
    if(!have)
    {
        bullettime::Screen s;
        if(bullettime::screen(s))
        {
            z.centre = s.centre;
            z.right = s.right;
            have = true;
        }
    }
    if(!have)
    {
        const hands::State& h = hands::current();
        if(!h.valid)
        {
            return false;
        }
        z.centre = h.pos[hands::gadgetHand()];
        z.right = glm::vec3{0.f};
    }
    const float sep = (static_cast<float>(sounds.channelSep[channel & 15]) - 128.f) / 128.f;
    out = z.centre + z.right * (sep * 0.06f * units::metresToUnits());
    return true;
}

[[nodiscard]] float soundVolume(int volume)
{
    return za::clamp(static_cast<float>(volume) / 127.f, 0.f, 1.f) * za::clamp(vr_doom_volume.value, 0.f, 1.f);
}

void hostPrint(void*, const char* line)
{
    if(vr_debug_doom.value || !session.running)
    {
        Con_Printf("doom: %s\n", line);
    }
    else
    {
        Con_DPrintf("doom: %s\n", line);
    }
}

void hostSoundStart(void*, int channel, const char* lump, const void* dmx, int length, int volume, int separation)
{
    if(!shm || channel < 0 || channel >= 16)
    {
        return;
    }
    char lumpName[9] = {};
    memcpy(lumpName, lump, 8);
    for(char& c : lumpName)
    {
        c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    }
    char name[MAX_QPATH];
    q_snprintf(name, sizeof(name), "doom/%d/%s.wav", sounds.generation, lumpName);
    bool have = false;
    for(const za::String& n : sounds.names)
    {
        have = have || n == za::StringView{name};
    }
    if(!have)
    {
        za::Vector<unsigned char> wav;
        if(!makeWav(static_cast<const unsigned char*>(dmx), length, wav))
        {
            return;
        }
        sounds.names.pushBack(za::String{name});
        sounds.wavs.pushBack(wav);
    }
    sfx_t* sfx = S_PrecacheSound(name);
    sounds.channelSep[channel] = separation;
    sounds.channelVolume[channel] = volume;
    glm::vec3 at;
    if(!sfx || !soundOrigin(channel, at))
    {
        return;
    }
    vec3_t org{at.x, at.y, at.z};
    S_StartSound(doomEntity, channel + 1, sfx, org, soundVolume(volume), za::max(0.f, vr_doom_attenuation.value));
}

channel_t* findChannel(int channel)
{
    for(int i = 0; i < total_channels && i < MAX_CHANNELS; i++)
    {
        channel_t& ch = snd_channels[i];
        if(ch.sfx && ch.entnum == doomEntity && ch.entchannel == channel + 1)
        {
            return &ch;
        }
    }
    return nullptr;
}

void hostSoundStop(void*, int channel)
{
    S_StopSound(doomEntity, channel + 1);
}

int hostSoundPlaying(void*, int channel)
{
    channel_t* ch = findChannel(channel);
    return ch && ch->end > paintedtime ? 1 : 0;
}

void hostSoundUpdate(void*, int channel, int volume, int separation)
{
    if(channel < 0 || channel >= 16)
    {
        return;
    }
    sounds.channelSep[channel] = separation;
    sounds.channelVolume[channel] = volume;
    if(channel_t* ch = findChannel(channel))
    {
        ch->master_vol = static_cast<int>(soundVolume(volume) * 255.f);
    }
}

// Doom's sounds follow the screen (the arm moves).
void moveSounds()
{
    for(int c = 0; c < 16; c++)
    {
        channel_t* ch = findChannel(c);
        glm::vec3 at;
        if(ch && soundOrigin(c, at))
        {
            ch->origin[0] = at.x;
            ch->origin[1] = at.y;
            ch->origin[2] = at.z;
        }
    }
}

void stopSounds()
{
    for(int c = 0; c < 16; c++)
    {
        S_StopSound(doomEntity, c + 1);
    }
}

// ---- Starting and stopping ---------------------------------------------------------------------------------------

void releaseKeys()
{
    for(int k = 0; k < 256; k++)
    {
        if(session.held[k] && api.key)
        {
            api.key(0, k);
        }
        session.held[k] = false;
        session.want[k] = false;
        session.pulseUntil[k] = 0.0;
    }
    for(bool& d : session.stickDir)
    {
        d = false;
    }
}

void applyOptions()
{
    if(!session.running)
    {
        return;
    }
    // joyb_speed past the joystick's buttons: always running (Chocolate Doom's way).
    (void)api.set("joyb_speed", vr_doom_always_run.value != 0.f ? "29" : "2");
}

void stop(const char* why)
{
    if(!session.running)
    {
        return;
    }
    releaseKeys();
    stopSounds();
    api.shutdown();
    unloadLibrary();
    session.running = false;
    session.focused = false;
    session.frameShown = ~0u;
    if(why)
    {
        Con_Printf("Doom: %s\n", why);
    }
}

bool start(int index)
{
    if(index < 0 || index >= static_cast<int>(catalog.wads.size()))
    {
        Con_Printf("Doom: no WAD (quakevr/doom/freedoom1.wad is missing; vr_doom_wads lists what was found)\n");
        return false;
    }
    if(!loadLibrary())
    {
        return false;
    }
    const Wad& w = catalog.wads[index];
    za::String data = doomFolder();
    data += "/";
    qvr_doom_host host{};
    host.version = QVR_DOOM_API_VERSION;
    host.print = hostPrint;
    host.sound_start = hostSoundStart;
    host.sound_stop = hostSoundStop;
    host.sound_playing = hostSoundPlaying;
    host.sound_update = hostSoundUpdate;
    sounds.generation++;
    za::Vector<const char*> args;
    if(vr_doom_args.string && vr_doom_args.string[0])
    {
        Cmd_TokenizeString(vr_doom_args.string);
        for(int i = 0; i < Cmd_Argc(); i++)
        {
            args.pushBack(Cmd_Argv(i));
        }
    }
    session.running = true; // (its startup text printed as it comes)
    const double t0 = Sys_DoubleTime();
    const int result = api.init(&host, w.path.cStr(), data.cStr(), static_cast<int>(args.size()), args.data());
    if(result != 0)
    {
        Con_Printf("Doom: could not start %s: %s\n", w.name.cStr(), api.error());
        catalog.wads[index].failed = true;
        session.running = true;
        stop(nullptr);
        return false;
    }
    session.wad = index;
    session.startedAt = realtime;
    session.lastTick = -1.0;
    session.frameShown = ~0u;
    applyOptions();
    Con_Printf("Doom: %s (%s, %s) started in %.0f ms\n", w.name.cStr(), w.source.cStr(), w.path.cStr(),
        (Sys_DoubleTime() - t0) * 1000.0);
    return true;
}

// Doom stopped by itself: its own Quit (closed), or an error (another WAD tried when it was an automatic pick, early).
void stopped(int result)
{
    const bool early = realtime - session.startedAt < 20.0;
    const int wad = session.wad;
    za::String why;
    if(result == QVR_DOOM_ERROR)
    {
        why = "stopped with an error: ";
        why += api.error();
    }
    else
    {
        why = "quit";
    }
    stop(why.cStr());
    if(result == QVR_DOOM_ERROR && wad >= 0)
    {
        catalog.wads[wad].failed = true;
        const bool automatic = !vr_doom_wad.string || !vr_doom_wad.string[0];
        if(early && automatic && session.open)
        {
            const int next = chosenWad();
            if(next >= 0 && next != wad)
            {
                Con_Printf("Doom: trying %s instead\n", catalog.wads[next].name.cStr());
                if(start(next))
                {
                    return;
                }
            }
        }
    }
    session.open = false;
}

void setOpen(bool open)
{
    if(open == session.open)
    {
        return;
    }
    if(open)
    {
        // (An automatic pick that fails to start is marked failed: the next one is tried.)
        const bool automatic = !vr_doom_wad.string || !vr_doom_wad.string[0];
        while(!session.running)
        {
            const int wad = chosenWad();
            if(start(wad))
            {
                break;
            }
            if(!automatic || wad < 0 || chosenWad() == wad)
            {
                return;
            }
            Con_Printf("Doom: trying %s instead\n", catalog.wads[chosenWad()].name.cStr());
        }
        session.open = true;
        session.lastTick = -1.0;
        if(vr_hud_mode.value != 1.f && vr_doom_panel.value == 0.f)
        {
            Con_Printf("Doom: the HUD is not the wrist gadget (vr_hud_mode 1): shown on the floating panel\n");
            Cvar_SetValueQuick(&vr_doom_panel, 1.f);
        }
        return;
    }
    session.open = false;
    session.focused = false;
    releaseKeys();
    stopSounds();
}

// ---- Focus -------------------------------------------------------------------------------------------------------

// Whether Quake runs (Doom runs with it): no menu, no console, the runtime's menu not up, not paused.
[[nodiscard]] bool quakeRunning()
{
    return key_dest == key_game && !runtimeMenuPaused() && !(sv.active && sv.paused) && !cl.paused;
}

// The screen Doom is on now (the panel, or the gadget's), and whether the viewer looks at it, raised and facing.
[[nodiscard]] bool screenNow(touch::Surface& out)
{
    if(vr_doom_panel.value != 0.f && session.panelPlaced)
    {
        return touch::lastSurface(out);
    }
    bullettime::Screen s;
    if(!bullettime::screen(s))
    {
        return false;
    }
    out.centre = s.centre;
    out.right = s.right;
    out.up = s.up;
    out.normal = s.normal;
    out.halfSize = s.halfSize;
    return true;
}

[[nodiscard]] bool lookedAt(const hands::State& s)
{
    touch::Surface z;
    if(!s.valid || !screenNow(z))
    {
        return false;
    }
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.headAngles, fwd, right, up);
    const glm::vec3 toScreen = z.centre - s.head;
    const float d = glm::length(toScreen);
    if(d < 1.f)
    {
        return false;
    }
    const glm::vec3 dir = toScreen / d;
    const float facing = -glm::dot(z.normal, dir); // the screen's face towards the eye
    return facing > 0.5f && glm::dot(fwd, dir) > za::cos(glm::radians(za::clamp(vr_doom_focus_angle.value, 5.f, 60.f)));
}

void updateFocus(float dt)
{
    const hands::State& s = hands::current();
    const bool controls = vr_doom_controls.value >= 1.f;
    if(!session.open || !quakeRunning() || !controls)
    {
        session.focused = false;
        session.lookTime = session.awayTime = 0.f;
        return;
    }
    const bool look = lookedAt(s);
    const touch::Contact& c = touch::contact();
    const bool touching = c.valid && (c.down || c.hover);
    session.lookTime = look ? session.lookTime + dt : 0.f;
    session.awayTime = look || touching ? 0.f : session.awayTime + dt;
    if(!session.focused)
    {
        const float lookFor = vr_doom_focus_look.value;
        if((c.valid && c.pressed) || (lookFor > 0.f && session.lookTime >= lookFor))
        {
            session.focused = true;
            if(vr_debug_doom.value)
            {
                Con_Printf("Doom: focus (%s)\n", c.pressed ? "a touch" : "looked at");
            }
        }
    }
    else if(session.awayTime >= za::max(0.05f, vr_doom_focus_grace.value))
    {
        session.focused = false;
        if(vr_debug_doom.value)
        {
            Con_Printf("Doom: focus back to Quake (the screen out of view)\n");
        }
    }
}

// ---- Controls ----------------------------------------------------------------------------------------------------

void hold(int key)
{
    if(key > 0 && key < 256)
    {
        session.want[key] = true;
    }
}

// A key held for a moment (two tics: a tap that Doom's game sees, as its menus see a press).
void pulse(int key)
{
    if(key > 0 && key < 256)
    {
        session.pulseUntil[key] = realtime + 0.065;
    }
}

// The off hand's stick as Doom's arrows: held past 0.5 (let go under 0.35); in Doom's menu repeated while held.
void stickArrows(glm::vec2 v, bool menu)
{
    const int keys[4] = {QVR_DOOM_KEY_UPARROW, QVR_DOOM_KEY_DOWNARROW, QVR_DOOM_KEY_LEFTARROW, QVR_DOOM_KEY_RIGHTARROW};
    const float amount[4] = {v.y, -v.y, -v.x, v.x};
    for(int i = 0; i < 4; i++)
    {
        const bool was = session.stickDir[i];
        const bool now = amount[i] > (was ? 0.35f : 0.5f);
        session.stickDir[i] = now;
        if(!now)
        {
            continue;
        }
        if(!menu)
        {
            hold(keys[i]);
        }
        else if(!was || realtime >= session.repeatAt[i])
        {
            pulse(keys[i]);
            session.repeatAt[i] = realtime + (was ? 0.14 : 0.4);
        }
    }
}

void controllerKeys()
{
    if(!session.focused || !session.padValid || vr_doom_controls.value < 1.f)
    {
        for(bool& d : session.stickDir)
        {
            d = false;
        }
        return;
    }
    const HandInput& p = session.pad;
    const qvr_doom_status& st = session.status;
    const bool menu = st.menu_active != 0;
    stickArrows(p.stick, menu);
    if(p.trigger)
    {
        hold(menu ? QVR_DOOM_KEY_ENTER : QVR_DOOM_KEY_FIRE);
        if(st.message_input)
        {
            hold('y');
        }
    }
    if(p.grip)
    {
        hold(menu ? QVR_DOOM_KEY_BACKSPACE : QVR_DOOM_KEY_USE);
    }
    if(p.primary)
    {
        hold(menu ? 'n' : QVR_DOOM_KEY_NEXTWEAPON);
    }
    if(p.secondary)
    {
        hold(QVR_DOOM_KEY_ESCAPE);
    }
    if(p.stickClick)
    {
        hold(QVR_DOOM_KEY_TAB);
    }
}

// The image's box on the overlay's screen (its pixels: aspect x 200): the 4:3 Doom was shown at (vr_doom_aspect 1),
// or the whole screen.
touch::Rect imageBox()
{
    const float w = session.aspect * screenHeight;
    if(vr_doom_aspect.value != 0.f)
    {
        const float iw = za::min(w, screenHeight * 4.f / 3.f);
        return {(w - iw) * 0.5f, 0.f, iw, static_cast<float>(screenHeight)};
    }
    return {0.f, 0.f, w, static_cast<float>(screenHeight)};
}

// The touch overlay (docs/vr-port/DOOM.md, "Touch controls"): laid out on the screen's own pixels (about 322 x 200 on
// the gadget: 35 to a centimetre), for a fingertip: no button under 1.3 cm.
void touchKeys()
{
    const touch::Contact& c = touch::contact();
    const float w = session.aspect * screenHeight;
    const int mode = static_cast<int>(vr_doom_controls.value); // 0 touch only, 1 both, 2 the controller only
    float alpha = 0.f;
    if(mode != 2)
    {
        const bool close = c.valid && (c.hover || c.down || (c.height < za::max(4.f, vr_touch_hover.value * 2.f) && c.height > -4.f));
        if(c.down || close)
        {
            session.lastTouch = realtime;
        }
        const int overlay = static_cast<int>(vr_doom_overlay.value); // 0 automatic, 1 always, 2 never
        const bool recent = realtime - session.lastTouch < 2.0;
        alpha = overlay == 2 ? 0.f : overlay == 1 || mode == 0 ? (recent ? 1.f : 0.55f) : recent ? 1.f : 0.f;
    }
    session.ui.begin(c, {w, static_cast<float>(screenHeight)}, alpha);
    if(mode == 2)
    {
        return; // (touches only give the focus)
    }
    touch::Ui& ui = session.ui;
    const qvr_doom_status& st = session.status;
    const touch::Rect box = imageBox();

    // The top bar: Doom's menu, the map, the weapons, always run, the fire lock, close.
    const float top = 34.f;
    const float bw = w / 7.f;
    int id = 1;
    const auto bar = [&](int i, const char* label, touch::Look look) {
        return ui.button(id++, {bw * i + 1.f, 0.f, bw - 2.f, top}, label, look);
    };
    if(bar(0, "MENU", touch::Look::Normal).pressed)
    {
        pulse(QVR_DOOM_KEY_ESCAPE);
    }
    if(bar(1, "MAP", st.automap ? touch::Look::On : touch::Look::Normal).pressed)
    {
        pulse(QVR_DOOM_KEY_TAB);
    }
    if(bar(2, "W-", touch::Look::Normal).pressed)
    {
        pulse(QVR_DOOM_KEY_PREVWEAPON);
    }
    if(bar(3, "W+", touch::Look::Normal).pressed)
    {
        pulse(QVR_DOOM_KEY_NEXTWEAPON);
    }
    if(bar(4, "RUN", vr_doom_always_run.value != 0.f ? touch::Look::On : touch::Look::Normal).pressed)
    {
        Cvar_SetValueQuick(&vr_doom_always_run, vr_doom_always_run.value != 0.f ? 0.f : 1.f);
        applyOptions();
    }
    if(bar(5, "LOCK", session.fireLock ? touch::Look::On : touch::Look::Normal).pressed)
    {
        session.fireLock = !session.fireLock;
    }
    if(bar(6, "X", touch::Look::Normal).clicked)
    {
        setOpen(false);
        return;
    }

    const float lower = top + 4.f;
    const float side = za::max(70.f, w * 0.22f);
    if(st.menu_active && st.message_input)
    {
        // A question: yes or no.
        if(ui.button(id++, {w * 0.5f - 120.f, 120.f, 110.f, 70.f}, "YES", touch::Look::Accent).pressed)
        {
            pulse('y');
        }
        if(ui.button(id++, {w * 0.5f + 10.f, 120.f, 110.f, 70.f}, "NO", touch::Look::Normal).pressed)
        {
            pulse('n');
        }
        return;
    }
    if(st.menu_active)
    {
        // Doom's menu: up, down, select, back at the sides; a tap on an item selects it.
        const float h = (screenHeight - lower) * 0.5f - 2.f;
        if(ui.button(id++, {0.f, lower, side, h}, "UP", touch::Look::Normal).pressed)
        {
            pulse(QVR_DOOM_KEY_UPARROW);
        }
        if(ui.button(id++, {0.f, lower + h + 4.f, side, h}, "DOWN", touch::Look::Normal).pressed)
        {
            pulse(QVR_DOOM_KEY_DOWNARROW);
        }
        if(ui.button(id++, {w - side, lower, side, h}, "OK", touch::Look::Accent).pressed)
        {
            pulse(QVR_DOOM_KEY_ENTER);
        }
        if(ui.button(id++, {w - side, lower + h + 4.f, side, h}, "BACK", touch::Look::Normal).pressed)
        {
            pulse(st.save_name ? QVR_DOOM_KEY_ESCAPE : QVR_DOOM_KEY_BACKSPACE);
        }
        touch::Button tap;
        if(ui.loose(tap) && tap.pressed)
        {
            const int x = static_cast<int>((c.downPos.x - box.x) * QVR_DOOM_WIDTH / box.w);
            const int y = static_cast<int>((c.downPos.y - box.y) * QVR_DOOM_HEIGHT / box.h);
            const int key = api.menuTap(x, y);
            pulse(key);
            if(vr_debug_doom.value)
            {
                Con_Printf("Doom: menu tap at %d %d: key %d\n", x, y, key);
            }
        }
        return;
    }

    // Playing: a drag stick on the left (move and turn; a hard press fires), fire and use on the right; a tap on the
    // middle uses, a drag across it turns.
    const touch::Stick move = ui.stick(id++, {0.f, lower, side * 1.6f, screenHeight - lower}, 40.f, "MOVE");
    if(move.held)
    {
        if(move.value.y < -0.3f) { hold(QVR_DOOM_KEY_UPARROW); }
        if(move.value.y > 0.3f) { hold(QVR_DOOM_KEY_DOWNARROW); }
        if(move.value.x < -0.35f) { hold(QVR_DOOM_KEY_LEFTARROW); }
        if(move.value.x > 0.35f) { hold(QVR_DOOM_KEY_RIGHTARROW); }
        if(move.hard && vr_doom_hard_fire.value != 0.f) { hold(QVR_DOOM_KEY_FIRE); }
    }
    const float fh = (screenHeight - lower) * 0.5f - 2.f;
    if(ui.button(id++, {w - side, lower, side, fh}, "USE", touch::Look::Normal).held)
    {
        hold(QVR_DOOM_KEY_USE);
    }
    if(ui.button(id++, {w - side, lower + fh + 4.f, side, fh}, "FIRE", touch::Look::Accent).held || session.fireLock)
    {
        hold(QVR_DOOM_KEY_FIRE);
    }
    touch::Button middle;
    if(ui.loose(middle))
    {
        const float dx = c.pos.x - c.downPos.x;
        if(middle.held && za::fabs(dx) > 8.f)
        {
            hold(dx < 0.f ? QVR_DOOM_KEY_LEFTARROW : QVR_DOOM_KEY_RIGHTARROW);
        }
        if(middle.released && realtime - c.downAt < 0.35 && glm::length(c.pos - c.downPos) < 8.f)
        {
            pulse(QVR_DOOM_KEY_USE);
        }
        if(middle.held && c.hard && vr_doom_hard_fire.value != 0.f)
        {
            hold(QVR_DOOM_KEY_FIRE);
        }
    }
}

// The keys wanted this frame sent as changes.
void sendKeys()
{
    for(int k = 1; k < 256; k++)
    {
        const bool now = session.want[k] || realtime < session.pulseUntil[k];
        if(now != session.held[k])
        {
            api.key(now ? 1 : 0, k);
            session.held[k] = now;
            if(vr_debug_doom.value >= 2.f)
            {
                Con_Printf("Doom: key %d %s\n", k, now ? "down" : "up");
            }
        }
        session.want[k] = false;
    }
}

// ---- The floating panel --------------------------------------------------------------------------------------------

void placePanel()
{
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return;
    }
    glm::vec3 fwd, right, up;
    hands::angleVectors(glm::vec3{0.f, s.headAngles.y, 0.f}, fwd, right, up);
    const float m = units::metresToUnits();
    const glm::vec3 at = s.head + fwd * (za::clamp(vr_doom_panel_distance.value, 0.2f, 3.f) * m) -
                         glm::vec3{0.f, 0.f, 0.12f * m};
    session.panelOffset = at - (s.playerOrigin + s.lean);
    session.panelYaw = s.headAngles.y;
    session.panelTurn = s.turnYaw;
    session.panelPlaced = true;
}

// The panel in the world now: placed in the play space, it moves and turns with it.
bool panelSurface(touch::Surface& out)
{
    const hands::State& s = hands::current();
    if(!session.panelPlaced || !s.valid)
    {
        return false;
    }
    const float turn = glm::radians(s.turnYaw - session.panelTurn);
    const float c = za::cos(turn), sn = za::sin(turn);
    const glm::vec3 o = session.panelOffset;
    const glm::vec3 offset{o.x * c - o.y * sn, o.x * sn + o.y * c, o.z};
    glm::vec3 fwd, right, up;
    hands::angleVectors(glm::vec3{0.f, session.panelYaw + (s.turnYaw - session.panelTurn), 0.f}, fwd, right, up);
    const float width = za::clamp(vr_doom_panel_size.value, 0.1f, 3.f) * units::metresToUnits();
    out.centre = s.playerOrigin + s.lean + offset;
    out.right = right;
    out.up = glm::vec3{0.f, 0.f, 1.f};
    out.normal = -fwd;
    out.halfSize = {width * 0.5f, width * 0.5f / session.aspect};
    return true;
}

// ---- Commands ----------------------------------------------------------------------------------------------------

[[nodiscard]] int keyNamed(const char* name)
{
    struct Named
    {
        const char* name;
        int key;
    };
    static constexpr Named names[] = {{"up", QVR_DOOM_KEY_UPARROW}, {"down", QVR_DOOM_KEY_DOWNARROW},
        {"left", QVR_DOOM_KEY_LEFTARROW}, {"right", QVR_DOOM_KEY_RIGHTARROW}, {"fire", QVR_DOOM_KEY_FIRE},
        {"use", QVR_DOOM_KEY_USE}, {"escape", QVR_DOOM_KEY_ESCAPE}, {"enter", QVR_DOOM_KEY_ENTER},
        {"tab", QVR_DOOM_KEY_TAB}, {"strafeleft", QVR_DOOM_KEY_STRAFE_L}, {"straferight", QVR_DOOM_KEY_STRAFE_R},
        {"nextweapon", QVR_DOOM_KEY_NEXTWEAPON}, {"prevweapon", QVR_DOOM_KEY_PREVWEAPON},
        {"backspace", QVR_DOOM_KEY_BACKSPACE}};
    for(const Named& n : names)
    {
        if(!q_strcasecmp(name, n.name))
        {
            return n.key;
        }
    }
    if(name[0] && !name[1])
    {
        return static_cast<unsigned char>(name[0]);
    }
    return atoi(name);
}

void doom_f()
{
    const bool open = Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) != 0 : !session.open;
    setOpen(open);
}

void key_f()
{
    if(Cmd_Argc() < 2 || !session.running)
    {
        Con_Printf("vr_doom_key <up|down|left|right|fire|use|escape|enter|tab|nextweapon|...|letter|number> [<1 held|0 "
                   "let go|tap>]: needs Doom started (vr_doom 1)\n");
        return;
    }
    const int key = keyNamed(Cmd_Argv(1));
    const char* how = Cmd_Argc() > 2 ? Cmd_Argv(2) : "tap";
    if(!strcmp(how, "tap"))
    {
        pulse(key);
        return;
    }
    // (Held by the command: sent at once, outside the frame's own keys; "0" lets go.)
    const bool down = atoi(how) != 0;
    api.key(down ? 1 : 0, key);
    session.held[key] = false;
}

void type_f()
{
    if(Cmd_Argc() < 2 || !session.running)
    {
        Con_Printf("vr_doom_type <text>: typed into Doom (cheats: iddqd, idkfa, idclev11)\n");
        return;
    }
    api.type(Cmd_Args());
}

void wads_f()
{
    scan();
    const int chosen = chosenWad();
    Con_Printf("Doom's WADs (drop DOOM.WAD, DOOM2.WAD... into quakevr/doom/wads; vr_doom_wad picks one):\n");
    for(za::SizeT i = 0; i < catalog.wads.size(); i++)
    {
        const Wad& w = catalog.wads[i];
        Con_Printf("%c %d: %s [%s%s] %s\n", static_cast<int>(i) == chosen ? '>' : ' ', static_cast<int>(i), w.name.cStr(),
            w.source.cStr(), w.failed ? ", failed" : "", w.path.cStr());
    }
}

void info_f()
{
    const qvr_doom_status& st = session.status;
    Con_Printf("Doom: %s, %s, %s; %s\n", session.running ? "running" : "not started", session.open ? "open" : "closed",
        session.focused ? "focused" : "not focused",
        session.wad >= 0 && session.wad < static_cast<int>(catalog.wads.size()) ? catalog.wads[session.wad].name.cStr() : "no WAD");
    if(session.running)
    {
        Con_Printf("  state %d (menu %d, automap %d), E%dM%d, health %d armor %d weapon %d ammo %d/%d/%d/%d\n",
            st.game_state, st.menu_active, st.automap, st.episode, st.map, st.health, st.armor, st.ready_weapon,
            st.ammo[0], st.ammo[1], st.ammo[2], st.ammo[3]);
        Con_Printf("  at %d %d facing %d, kills %d items %d secrets %d; %d tics, %d frames, %d sounds\n", st.x, st.y,
            st.angle, st.kills, st.items, st.secrets, st.tics, st.frames, st.sounds);
        Con_Printf("  tick %.3f ms (most %.3f), screen %.3f ms\n", session.tickMs, session.tickMsMax, session.renderMs);
        int playing = 0;
        for(int c = 0; c < 16; c++)
        {
            playing += findChannel(c) ? 1 : 0;
        }
        const glm::vec3 o = hands::current().playerOrigin;
        Con_Printf("  Quake's time %.2f s (%s), the player at %.0f %.0f; Doom's sounds playing now %d\n", cl.time,
            quakeRunning() ? "running" : "paused", o.x, o.y, playing);
    }
    touch::Surface z;
    const hands::State& hs = hands::current();
    if(hs.valid && screenNow(z))
    {
        glm::vec3 fwd, right, up;
        hands::angleVectors(hs.headAngles, fwd, right, up);
        const glm::vec3 dir = glm::normalize(z.centre - hs.head);
        Con_Printf("  screen %.1f x %.1f cm, %.0f cm from the eye, facing it %.2f, %.0f degrees off the view's middle: %s\n",
            z.halfSize.x * 200.f / units::metresToUnits(), z.halfSize.y * 200.f / units::metresToUnits(),
            glm::length(z.centre - hs.head) * 100.f / units::metresToUnits(), -glm::dot(z.normal, dir),
            glm::degrees(za::acos(za::clamp(glm::dot(fwd, dir), -1.f, 1.f))), lookedAt(hs) ? "looked at" : "not looked at");
    }
}

void focus_f()
{
    session.focused = Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) != 0 && session.open : !session.focused && session.open;
    session.lookTime = session.awayTime = 0.f;
}

void restart_f()
{
    const bool wasOpen = session.open;
    stop("restarting");
    session.open = false;
    for(Wad& w : catalog.wads)
    {
        w.failed = false;
    }
    catalog.scanned = false;
    if(wasOpen || (Cmd_Argc() > 1 && atoi(Cmd_Argv(1))))
    {
        setOpen(true);
    }
}

void openFolder_f()
{
    char path[MAX_OSPATH];
    q_snprintf(path, sizeof(path), "%s/wads", doomFolder().cStr());
    (void)files::createDirectories(path);
#ifdef _WIN32
    char full[MAX_PATH];
    if(GetFullPathNameA(path, sizeof(full), full, nullptr) && !getenv("QVR_TEST_HIDDEN"))
    {
        ShellExecuteA(nullptr, "open", full, nullptr, nullptr, SW_SHOWNORMAL);
    }
#endif
    Con_Printf("Doom's WAD folder: %s\n", path);
}

void panelPlace_f()
{
    placePanel();
}

} // namespace

// ---- The module ----------------------------------------------------------------------------------------------------

void init()
{
    Cmd_AddCommand("vr_doom", doom_f);
    Cmd_AddCommand("vr_doom_key", key_f);
    Cmd_AddCommand("vr_doom_type", type_f);
    Cmd_AddCommand("vr_doom_wads", wads_f);
    Cmd_AddCommand("vr_doom_info", info_f);
    Cmd_AddCommand("vr_doom_focus", focus_f);
    Cmd_AddCommand("vr_doom_restart", restart_f);
    Cmd_AddCommand("vr_doom_open_folder", openFolder_f);
    Cmd_AddCommand("vr_doom_panel_place", panelPlace_f);
}

void shutdown()
{
    stop(nullptr);
}

void frame()
{
    if(!session.running)
    {
        session.open = false;
        return;
    }
    const double now = realtime;
    const float dt = session.lastTick < 0.0 ? 0.f : static_cast<float>(za::clamp(now - session.lastTick, 0.0, 0.25));
    session.lastTick = now;
    if(!session.open)
    {
        return; // (closed: Doom waits)
    }
    // The screen's shape (the gadget's; the panel the same), and the panel placed when first wanted.
    if(vr_doom_panel.value != 0.f && !session.panelPlaced)
    {
        placePanel();
    }
    else if(vr_doom_panel.value == 0.f)
    {
        session.panelPlaced = false;
    }
    {
        glm::vec3 corner;
        glm::vec2 size;
        gadget::screenRect(corner, size);
        session.aspect = size.x / za::max(0.01f, size.y);
    }
    touch::Request req;
    req.pixels = {session.aspect * screenHeight, static_cast<float>(screenHeight)};
    if(vr_doom_panel.value != 0.f && panelSurface(req.panel))
    {
        req.gadget = false;
    }
    touch::request(req);

    updateFocus(dt);
    const bool runs = quakeRunning() && (vr_doom_pause_unfocused.value == 0.f || session.focused);
    if(runs)
    {
        touchKeys();
        controllerKeys();
    }
    else
    {
        session.ui.begin(touch::contact(), {session.aspect * screenHeight, static_cast<float>(screenHeight)}, 0.f);
    }
    sendKeys();
    if(runs)
    {
        const double t0 = Sys_DoubleTime();
        const int result = api.tick(static_cast<unsigned>(dt * 1000.f + 0.5f));
        const float ms = static_cast<float>((Sys_DoubleTime() - t0) * 1000.0);
        session.tickMs += (ms - session.tickMs) * 0.05f;
        session.tickMsMax = za::max(session.tickMsMax * 0.995f, ms);
        if(result != QVR_DOOM_RUNNING)
        {
            stopped(result);
            return;
        }
        api.status(&session.status);
    }
    moveSounds();
}

bool onGadget()
{
    return session.running && session.open && vr_doom_panel.value == 0.f;
}

gfx::Texture gadgetTexture()
{
    return session.running && session.open ? screen.texture : 0;
}

const gfx::Target& screenTarget()
{
    return screen;
}

void renderScreen()
{
    if(!session.running || !session.open)
    {
        return;
    }
    // Only while it can be seen: the gadget shown, or the panel.
    if(!(gadget::active() && vr_doom_panel.value == 0.f) && !(vr_doom_panel.value != 0.f && session.panelPlaced))
    {
        return;
    }
    const double t0 = Sys_DoubleTime();
    unsigned frameNow = 0;
    const unsigned char* rgba = api.frame(&frameNow);
    gfx::ensurePicture(picture, QVR_DOOM_WIDTH, QVR_DOOM_HEIGHT, "doom screen");
    if(frameNow != session.frameShown)
    {
        gfx::updatePicture(picture, rgba);
        session.frameShown = frameNow;
    }
    const int w = static_cast<int>(session.aspect * screenHeight + 0.5f);
    gfx::ensureTarget(screen, w * 3, screenHeight * 3, false, "doom screen");
    gfx::begin2D(screen, w, screenHeight);
    gfx::draw2D::fill(0.f, 0.f, static_cast<float>(w), static_cast<float>(screenHeight), glm::vec3{0.f});
    const touch::Rect box = imageBox();
    gfx::draw2D::picture(picture, box.x, box.y, box.w, box.h);
    session.ui.draw();
    gfx::end2D();
    session.renderMs += (static_cast<float>((Sys_DoubleTime() - t0) * 1000.0) - session.renderMs) * 0.05f;
}

void drawPanel()
{
    touch::Surface z;
    if(!session.running || !session.open || vr_doom_panel.value == 0.f || !screen.texture || !panelSurface(z))
    {
        return;
    }
    const glm::vec3 x = z.right * (z.halfSize.x * 2.f), y = z.up * (z.halfSize.y * 2.f);
    const glm::vec3 o = z.centre - x * 0.5f - y * 0.5f;
    const glm::vec4 white{1.f};
    const gfx::Vertex c[4] = {{o, {0.f, 0.f}, white}, {o + x, {1.f, 0.f}, white}, {o + x + y, {1.f, 1.f}, white},
        {o + y, {0.f, 1.f}, white}};
    const gfx::Vertex quad[6] = {c[0], c[1], c[2], c[0], c[2], c[3]};
    const float k = za::clamp(vr_gadget_crt.value * vr_doom_crt.value, 0.f, 2.f) * 0.5f;
    gfx::draw(quad, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::Screen, .blend = gfx::Blend::Opaque, .depthTest = true, .depthWrite = true,
            .params = {static_cast<float>(realtime - 1000.0 * static_cast<int>(realtime / 1000.0)), k, 0.f, 0.f},
            .screen = {session.aspect * screenHeight, static_cast<float>(screenHeight), 1.f}, .trueColor = true},
        screen.texture);
}

bool takesOffHand(const InputState& in)
{
    const int g = hands::gadgetHand();
    session.pad = in.hands[g];
    session.padValid = true;
    return session.running && session.open && session.focused && vr_doom_controls.value >= 1.f && quakeRunning();
}

} // namespace qvr::doom

// S_LoadSound, no file of the name (snd_mem.c): Doom's effects, made into WAV files as they were first played.
extern "C" byte* VR_SndMemory(const char* name, int* size)
{
    using namespace qvr::doom;
    if(strncmp(name, "doom/", 5) != 0)
    {
        return nullptr;
    }
    for(za::SizeT i = 0; i < sounds.names.size(); i++)
    {
        if(sounds.names[i] == za::StringView{name})
        {
            const za::Vector<unsigned char>& wav = sounds.wavs[i];
            byte* copy = static_cast<byte*>(VR_HeapMalloc(wav.size()));
            memcpy(copy, wav.data(), wav.size());
            *size = static_cast<int>(wav.size());
            return copy;
        }
    }
    return nullptr;
}
