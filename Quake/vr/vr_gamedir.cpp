// vr_gamedir.cpp -- the "quakevr" game folder.
//
// The Quake VR progs contain the id1, Scourge of Armagon (hipnotic) and Dissolution of
// Eternity (rogue) campaigns in one. When the quakevr folder is added to the search path,
// the installed mission packs are layered underneath it automatically, and the engine is
// kept in standard-Quake mode: the mission-pack HUDs and weapon encodings do not apply
// to the VR progs, which use their own weapon IDs.

#include "vr_ambient.hpp"
#include "vr_anchor.hpp"
#include "vr_ao.hpp"
#include "vr_bigfont.hpp"
#include "vr_gadget.hpp"
#include "vr_bodyblood.hpp"
#include "vr_cvars.hpp"
#include "vr_detail.hpp"
#include "vr_emissive.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_walltorch.hpp"
#include "vr_gfx.hpp"
#include "vr_mapinstall.hpp"
#include "vr_mem.hpp"
#include "vr_modellight.hpp"
#include "vr_relight.hpp"
#include "vr_avatar.hpp"
#include "vr_flashlight.hpp"
#include "vr_sightalign.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"
#include "vr_wounds.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <sys/stat.h>
extern "C" {
#include "steam.h"
#include "json.h" // (the engine's C header: Epic's install manifests)
}


namespace
{

constexpr const char* vrGameDir = "quakevr";
constexpr const char* missionPacks[] = {"hipnotic", "rogue"};

bool addingMissionPacks = false;

// 0 missing, 1 available, 2 incomplete/corrupt. Inspect the pack's own files, never
// the VR overrides: shipping a view model is not evidence of owning the campaign.
int packStatus[2] = {};

// What inspectPackCached remembers from one call to the next (main thread, from the campaign scan and
// VR_OnGameDirChanged): each pack's last file hash, what its files were inspected as, and whether either was recorded.
// The two mission packs are fixed, so plain arrays beside packStatus, which they feed.
za::U64 packSignatureMemo[2] = {};
int packStatusMemo[2] = {};
bool packMemoValid[2] = {};
constexpr const char* hipnoticResources[] = {
#include "vr_pack_hipnotic.inc"
};
constexpr const char* rogueResources[] = {
#include "vr_pack_rogue.inc"
};

struct PackEntry
{
    char name[56];
    int32_t offset;
    int32_t length;
};

// Pack inspection runs on the main thread during filesystem setup/switching. Each nested
// operation has its own buffer; capacity is reused during inspection and released on map change.
struct PackScratch
{
    za::Vector<unsigned char> resourceBytes;
    za::Vector<unsigned char> found;
    za::Vector<PackEntry> entries;
    auto members() { return qvr::mem::list(resourceBytes, found, entries); }
};
qvr::mem::Scratch<PackScratch> packScratch{"mission packs"};

// Structural validation accepts different owned releases and replacement assets, while
// rejecting truncated payloads before the model/sound loaders can consume them.
bool validResource(FILE* file, long offset, long length, const char* name)
{
    if(length < 12 || fseek(file, offset, SEEK_SET)) { return false; }
    za::Vector<unsigned char>& bytes = packScratch.resourceBytes;
    bytes.clear();
    bytes.resize(static_cast<za::SizeT>(length));
    if(fread(bytes.data(), 1, bytes.size(), file) != bytes.size()) { return false; }
    const auto integer = [&](size_t pos) -> int32_t
    {
        int32_t value = 0;
        if(pos <= bytes.size() && bytes.size() - pos >= 4) { memcpy(&value, bytes.data() + pos, 4); }
        return LittleLong(value);
    };
    size_t cursor = 0;
    const auto advance = [&](int64_t count) -> bool
    {
        if(count < 0 || static_cast<uint64_t>(count) > bytes.size() - cursor) { return false; }
        cursor += static_cast<size_t>(count);
        return true;
    };
    const char* ext = COM_FileGetExtension(name);
    if(!q_strcasecmp(ext, "mdl"))
    {
        if(length < 84 || memcmp(bytes.data(), "IDPO", 4) || integer(4) != 6) { return false; }
        const int skins = integer(48), width = integer(52), height = integer(56);
        const int vertices = integer(60), triangles = integer(64), frames = integer(68);
        if(skins <= 0 || width <= 0 || height <= 0 || vertices <= 0 || triangles <= 0 || frames <= 0)
        { return false; }
        cursor = 84;
        for(int i = 0; i < skins; ++i)
        {
            const int type = integer(cursor);
            if(!advance(4)) { return false; }
            int count = 1;
            if(type == 1)
            {
                count = integer(cursor);
                if(count <= 0 || !advance(4 + int64_t(count) * 4)) { return false; }
            }
            else if(type != 0) { return false; }
            // Check one image first: the following multiplication cannot exceed the file size.
            const int64_t image = int64_t(width) * height;
            if(image > length || !advance(image * count)) { return false; }
        }
        if(!advance(int64_t(vertices) * 12 + int64_t(triangles) * 16)) { return false; }
        for(int i = 0; i < frames; ++i)
        {
            const int type = integer(cursor);
            if(!advance(4)) { return false; }
            int count = 1;
            if(type == 1)
            {
                count = integer(cursor);
                if(count <= 0 || !advance(12 + int64_t(count) * 4)) { return false; }
            }
            else if(type != 0) { return false; }
            const int64_t frame = 24 + int64_t(vertices) * 4;
            if(frame > length || !advance(frame * count)) { return false; }
        }
        return true;
    }
    if(!q_strcasecmp(ext, "spr"))
    {
        if(length < 36 || memcmp(bytes.data(), "IDSP", 4) || integer(4) != 1 || integer(24) <= 0)
        { return false; }
        cursor = 36;
        for(int i = 0; i < integer(24); ++i)
        {
            const int type = integer(cursor);
            if(!advance(4)) { return false; }
            int count = 1;
            if(type == 1)
            {
                count = integer(cursor);
                if(count <= 0 || !advance(4 + int64_t(count) * 4)) { return false; }
            }
            else if(type != 0) { return false; }
            for(int j = 0; j < count; ++j)
            {
                const int width = integer(cursor + 8), height = integer(cursor + 12);
                if(width <= 0 || height <= 0 || !advance(16 + int64_t(width) * height)) { return false; }
            }
        }
        return true;
    }
    if(!q_strcasecmp(ext, "wav"))
    {
        if(length < 44 || memcmp(bytes.data(), "RIFF", 4) || memcmp(bytes.data() + 8, "WAVE", 4) ||
            integer(4) < 36 || int64_t(integer(4)) + 8 > length) { return false; }
        bool format = false, samples = false;
        cursor = 12;
        const size_t end = static_cast<size_t>(integer(4)) + 8;
        while(cursor < end)
        {
            if(end - cursor < 8) { return false; }
            const int size = integer(cursor + 4);
            if(size < 0 || static_cast<size_t>(size) > end - cursor - 8) { return false; }
            if(!memcmp(bytes.data() + cursor, "fmt ", 4)) { format = size >= 16; }
            if(!memcmp(bytes.data() + cursor, "data", 4)) { samples = size > 0; }
            // Original pack WAVs can contain junk LIST metadata or omit the final pad byte.
            // The engine needs the bounded fmt/data chunks; trailing metadata is not consumed.
            if(format && samples) { return true; }
            if(!advance(8 + int64_t(size) + (size & 1))) { return false; }
        }
        return format && samples;
    }
    if(length < 124 || (integer(0) != 29 && integer(0) != 30 &&
        memcmp(bytes.data(), "BSP2", 4) && memcmp(bytes.data(), "2PSB", 4))) { return false; }
    for(int i = 0; i < 15; ++i)
    {
        const int pos = integer(4 + i * 8), size = integer(8 + i * 8);
        if(pos < 0 || size < 0 || pos > length || size > length - pos) { return false; }
    }
    return integer(120) >= 64; // at least the world model
}

int inspectPack(const char* game, const char* const* resources, size_t count, const char* onlyRoot = nullptr)
{
    za::Vector<unsigned char>& found = packScratch.found;
    found.assign(count, 0);
    // Whether any of the pack's own data is there (a pak, or one of its listed files): a folder holding only what a
    // texture or music pack extracted into it (textures/, music/) is no installation, so it reads as missing, not
    // incomplete.
    bool gameData = false;
    for(int base = 0; base < (onlyRoot ? 1 : com_numbasedirs); ++base)
    {
        char folder[MAX_OSPATH];
        q_snprintf(folder, sizeof(folder), "%s/%s", onlyRoot ? onlyRoot : com_basedirs[base], game);
        for(int pak = 0;; ++pak)
        {
            char path[MAX_OSPATH];
            q_snprintf(path, sizeof(path), "%s/pak%d.pak", folder, pak);
            FILE* file = fopen(path, "rb");
            if(!file) { break; }
            gameData = true;
            fseek(file, 0, SEEK_END);
            const long size = ftell(file);
            rewind(file);
            struct { char id[4]; int32_t offset; int32_t length; } header{};
            bool valid = fread(&header, 1, sizeof(header), file) == sizeof(header) &&
                         !memcmp(header.id, "PACK", 4);
            const int offset = LittleLong(header.offset);
            const int length = LittleLong(header.length);
            valid &= offset >= 12 && length > 0 && length % sizeof(PackEntry) == 0 &&
                     length / sizeof(PackEntry) <= 2048 && offset <= size && length <= size - offset;
            za::Vector<PackEntry>& entries = packScratch.entries;
            entries.clear();
            if(valid)
            {
                entries.resize(length / sizeof(PackEntry));
                valid = !fseek(file, offset, SEEK_SET) && fread(entries.data(), 1, length, file) == size_t(length);
            }
            for(const PackEntry& entry : entries)
            {
                const int pos = LittleLong(entry.offset), len = LittleLong(entry.length);
                if(!memchr(entry.name, 0, sizeof(entry.name)) || pos < 0 || len < 0 || pos > size || len > size - pos)
                {
                    valid = false;
                    break;
                }
                for(size_t i = 0; i < count; ++i)
                {
                    if(!strcmp(entry.name, resources[i]))
                    {
                        if(!validResource(file, pos, len, entry.name))
                        {
                            Con_Printf("VR: %s: invalid/truncated resource %s in %s.\n", game, entry.name, path);
                            valid = false;
                        }
                        found[i] = true;
                    }
                }
            }
            fclose(file);
            if(!valid)
            {
                Con_Printf("VR: %s: corrupt archive %s; restore your owned mission-pack data.\n", game, path);
                return 2;
            }
        }
        // An extracted installation is also supported, but every required file must exist.
        for(size_t i = 0; i < count; ++i)
        {
            char path[MAX_OSPATH];
            q_snprintf(path, sizeof(path), "%s/%s", folder, resources[i]);
            if(FILE* file = fopen(path, "rb"))
            {
                fseek(file, 0, SEEK_END);
                const long size = ftell(file);
                const bool valid = validResource(file, 0, size, resources[i]);
                fclose(file);
                if(!valid) { return 2; }
                found[i] = true;
                gameData = true;
            }
        }
    }
    for(size_t i = 0; i < count; ++i)
    {
        if(!found[i])
        {
            if(gameData)
            {
                Con_Printf("VR: %s: incomplete installation (missing %s); restore your owned mission-pack data.\n",
                    game, resources[i]);
            }
            return gameData ? 2 : 0;
        }
    }
    return 1;
}


// What a pack's files are now, without reading them: each pak's and each listed loose file's size and time, in every
// base dir. The same signature: the same answer (inspectPack reads every listed model, sound and map whole, which is
// worth doing once, not at every rebuild of the game folders: a campaign or a map package switched).
za::U64 packSignature(const char* game, const char* const* resources, size_t count)
{
    za::U64 h = 1469598103934665603ull;
    const auto mix = [&](za::U64 v) { h = (h ^ v) * 1099511628211ull; };
    const auto file = [&](const char* path)
    {
        struct stat st{};
        if(stat(path, &st) == 0) { mix(static_cast<za::U64>(st.st_size)); mix(static_cast<za::U64>(st.st_mtime)); }
        else { mix(0x9e3779b97f4a7c15ull); }
    };
    for(int base = 0; base < com_numbasedirs; ++base)
    {
        char folder[MAX_OSPATH], path[MAX_OSPATH];
        q_snprintf(folder, sizeof(folder), "%s/%s", com_basedirs[base], game);
        mix(Sys_FileType(folder) == FS_ENT_DIRECTORY ? 1 : 2);
        for(int pak = 0; pak < 100; ++pak)
        {
            q_snprintf(path, sizeof(path), "%s/pak%d.pak", folder, pak);
            if(Sys_FileType(path) != FS_ENT_FILE) { break; }
            file(path);
        }
        for(size_t i = 0; i < count; ++i)
        {
            q_snprintf(path, sizeof(path), "%s/%s", folder, resources[i]);
            file(path);
        }
    }
    return h;
}

// inspectPack, again only when the pack's files changed since it was last read.
int inspectPackCached(int which, const char* game, const char* const* resources, size_t count)
{
    const za::U64 now = packSignature(game, resources, count);
    if(!packMemoValid[which] || packSignatureMemo[which] != now)
    {
        packStatusMemo[which] = inspectPack(game, resources, count);
        packSignatureMemo[which] = now;
        packMemoValid[which] = true;
    }
    return packStatusMemo[which];
}

constexpr const char* dopaResources[] = {
#include "vr_pack_dopa.inc"
};
constexpr const char* mg1Resources[] = {
#include "vr_pack_mg1.inc"
};
constexpr const char* mg3Resources[] = {
#include "vr_pack_mg3.inc"
};
struct Campaign
{
    const char* folder;
    const char* title;
    const char* start;
    int schema;
    bool nativeReady;
    const char* const* resources;
    size_t resourceCount;
    int status;
    char root[MAX_OSPATH];
};
// The installer mirrors nativeReady and soloOnly() (Installer/.../ExpansionDetector.cs, its "ready" labels); its
// self-test "expansions: readiness matches the engine" fails until both agree.
Campaign campaigns[] = {
    {"id1", "Quake", "start", 0, true, nullptr, 0, 1, {}},
    {"hipnotic", "Scourge of Armagon", "start", 1, true, nullptr, 0, 0, {}},
    {"rogue", "Dissolution of Eternity", "start", 2, true, nullptr, 0, 0, {}},
    {"dopa", "Dimension of the Past", "e5start", 3, true, dopaResources, countof(dopaResources), 0, {}},
    {"mg1", "Dimension of the Machine", "start", 4, true, mg1Resources, countof(mg1Resources), 0, {}},
    {"mg3", "Dawn of the Machine", "start", 5, true, mg3Resources, countof(mg3Resources), 0, {}},
};
int activeCampaign = 0;
bool discoveredCampaigns = false;
bool developerNative = false;
bool rebuildingCampaign = false;
bool nativeCampaignPaths = false;
double hubSelectTime = -10.0; // (realtime) when a hub's teleporter last ran the selector (VR_CanChangeCampaignMap)

int campaignIndex(const char* name)
{
    for(int i = 0; i < int(countof(campaigns)); ++i)
    {
        if(!q_strcasecmp(name, campaigns[i].folder)) { return i; }
    }
    return -1;
}

// The Epic Games Store's Quake (the 2021 release): the launcher's install manifests (<ProgramData>/Epic/
// EpicGamesLauncher/Data/Manifests/*.item, JSON; or the folder after -epicmanifests, for tests), each with its
// DisplayName and InstallLocation. A manifest counts when its name says Quake and its folder holds the rerelease's
// id1/pak0.pak, in rerelease/ (Steam's layout) or at its root (GOG's): that folder is the root. -noepic: none.
void addEpicRoots(za::Vector<za::String>& roots)
{
    za::String dir;
    if(const int arg = COM_CheckParm("-epicmanifests"); arg && arg + 1 < com_argc)
    {
        dir = com_argv[arg + 1];
    }
    else
    {
#ifdef _WIN32
        const char* programData = getenv("PROGRAMDATA");
        if(!programData || !*programData) { return; }
        dir = za::String{programData} + "/Epic/EpicGamesLauncher/Data/Manifests";
#else
        return;
#endif
    }
    if(!qvr::files::isDirectory(dir.cStr())) { return; }
    qvr::files::forEachEntry(dir.cStr(), [&](const char* name, bool isDirectory)
    {
        const za::SizeT n = strlen(name);
        if(isDirectory || n < 5 || q_strcasecmp(name + n - 5, ".item")) { return; }
        za::String text;
        if(!qvr::files::readText(qvr::files::join(dir.cStr(), name).cStr(), text)) { return; }
        json_t* json = JSON_Parse(text.cStr());
        if(!json) { return; }
        const char* title = json->root ? JSON_FindString(json->root, "DisplayName") : nullptr;
        const char* location = json->root ? JSON_FindString(json->root, "InstallLocation") : nullptr;
        if(title && location && *location && q_strcasestr(title, "quake"))
        {
            char path[MAX_OSPATH];
            q_snprintf(path, sizeof(path), "%s/rerelease/id1/pak0.pak", location);
            if(Sys_FileType(path) == FS_ENT_FILE)
            {
                q_snprintf(path, sizeof(path), "%s/rerelease", location);
                roots.pushBack(za::String{path});
            }
            else
            {
                q_snprintf(path, sizeof(path), "%s/id1/pak0.pak", location);
                if(Sys_FileType(path) == FS_ENT_FILE) { roots.pushBack(za::String{location}); }
            }
        }
        JSON_Free(json);
    });
}

za::Vector<za::String> ownedRoots()
{
    // Explicit bases win, last base highest. Never add store roots to com_basedirs:
    // configs, saves and community caches must remain under the writable VR root.
    // (on the heap: MAX_BASEDIRS * 3 + 2 paths of PATH_MAX bytes is most of a megabyte of stack on Linux)
    za::Vector<za::String> roots;
    char path[MAX_OSPATH];
    const auto addRoot = [&](const char* p) { roots.pushBack(za::String{p}); };
    if(!COM_CheckParm("-nosteam"))
    {
        steamgame_t owned{};
        char install[MAX_OSPATH];
        if(Steam_FindGame(&owned, QUAKE_STEAM_APPID) && Steam_ResolvePath(install, sizeof(install), &owned))
        { q_snprintf(path, sizeof(path), "%s/rerelease", install); addRoot(path); }
    }
    if(!COM_CheckParm("-nogog"))
    {
        char install[MAX_OSPATH];
        if(Sys_GetGOGQuakeEnhancedDir(install, sizeof(install)))
        { addRoot(install); }
    }
    if(!COM_CheckParm("-noepic"))
    {
        addEpicRoots(roots);
    }
    for(int i = 0; i < com_numbasedirs; ++i)
    {
        q_snprintf(path, sizeof(path), "%s/../rerelease", com_basedirs[i]);
        addRoot(path);
        q_snprintf(path, sizeof(path), "%s/rerelease", com_basedirs[i]);
        addRoot(path);
        addRoot(com_basedirs[i]);
    }
    return roots;
}

void discoverCampaigns()
{
    if(discoveredCampaigns) { return; }
    discoveredCampaigns = true;
    // Explicit bases win, last base highest. Never add store roots to com_basedirs:
    // configs, saves and community caches must remain under the writable VR root.
    const auto roots = ownedRoots();
    for(int i = 3; i < int(countof(campaigns)); ++i)
    {
        Campaign& c = campaigns[i];
        for(int r = int(roots.size()) - 1; r >= 0; --r)
        {
            char folder[MAX_OSPATH];
            q_snprintf(folder, sizeof(folder), "%s/%s", roots[r].cStr(), c.folder);
            if(Sys_FileType(folder) != FS_ENT_DIRECTORY) { continue; }
            c.status = inspectPack(c.folder, c.resources, c.resourceCount, roots[r].cStr());
            if(c.status == 0) { continue; } // none of its data (a texture pack's folder): the next root's copy counts
            q_strlcpy(c.root, roots[r].cStr(), sizeof(c.root));
            // A damaged higher-priority owned copy is explicit, never masked by another release.
            break;
        }
    }
}

// `syncHub`: the hub's choice (vr_activestartpaknameidx, archived) made the active campaign's too: when a campaign is
// chosen (the selector, a map or a changelevel that picks one), never when the game folders are only rebuilt (a map
// package mounted, a mission pack re-added): that would undo a choice made at the hub's buttons before its portal.
void publishCampaign(bool syncHub = false)
{
    if(!Cvar_FindVar("vr_campaign")) { return; }
    Cvar_SetROM("vr_honey_context", "0");
    // The archived legacy hub selector only describes the original three campaigns.
    if(syncHub)
    { Cvar_SetValueQuick(&qvr::vr_activestartpaknameidx, activeCampaign <= 2 ? activeCampaign : 0); }
    Cvar_SetROM("vr_campaign", va("%d", activeCampaign));
    Cvar_SetROM("vr_campaign_schema", va("%d", campaigns[activeCampaign].schema));
    for(int i = 3; i < int(countof(campaigns)); ++i)
    { Cvar_SetROM(va("vr_%s_status", campaigns[i].folder), va("%d", campaigns[i].status)); }
}

constexpr const char* dopaLanguageKeys[] = {
#include "vr_loc_dopa.inc"
};
constexpr const char* mg1LanguageKeys[] = {
#include "vr_loc_mg1.inc"
};
constexpr const char* mg3LanguageKeys[] = {
#include "vr_loc_mg3.inc"
};

bool campaignMultiplayerRequested()
{
    return Cvar_VariableValue("coop") || Cvar_VariableValue("deathmatch") || svs.maxclients > 1;
}

// The ready native campaigns accepted for single player only: Dimension of the Past, Dimension of the Machine
// (its Horde coop passed two-process tests, not yet a session with two headsets) and Dawn of the Machine (MG3_PLAN.md
// decision 6: single player first; its co-op and dm1 later). Their multiplayer stays on the developer path
// (vr_campaign_native).
[[nodiscard]] bool soloOnly(int index)
{
    return index == 3 || index == 4 || index == 5;
}

int missingLanguage(int index, const char** first = nullptr)
{
    const char* const* keys = index == 3 ? dopaLanguageKeys : index == 4 ? mg1LanguageKeys : mg3LanguageKeys;
    const size_t count = index == 3 ? countof(dopaLanguageKeys) : index == 4 ? countof(mg1LanguageKeys) : countof(mg3LanguageKeys);
    int missing = 0;
    for(size_t i = 0; i < count; ++i)
    {
        const char* text = LOC_GetRawString(keys[i]);
        if(!text || !*text || !strcmp(text, keys[i]))
        { if(first && !missing) { *first = keys[i]; } ++missing; }
    }
    return missing;
}

const char* campaignStatus(int i)
{
    return campaigns[i].status == 0 ? "missing" : campaigns[i].status == 2 ? "incomplete/corrupt" :
        campaigns[i].nativeReady ? (i >= 3 && Cvar_FindVar("language") && missingLanguage(i) ? "installed; language data incomplete" : "ready") : "installed; native support in progress";
}

void reportCampaigns()
{
    for(int i = 0; i < int(countof(campaigns)); ++i)
    {
        const Campaign& c = campaigns[i];
        Con_Printf("VR campaign %d %s: %s; start %s; schema %d; source %s\n",
            i, c.folder, campaignStatus(i), c.start, c.schema, c.root[0] ? c.root : "configured basedirs");
    }
    for(int i = 3; i < int(countof(campaigns)); ++i)
    {
        const char* first = "none";
        const int missing = missingLanguage(i, &first);
        Con_Printf("VR language %s: %d missing identifiers; first %s. Missing translations use owned English when available.\n", campaigns[i].folder, missing, first);
    }
    Con_Printf("VR active campaign: %s (%d), VR progs highest.\n", campaigns[activeCampaign].folder, activeCampaign);
}

void reportPackStatus()
{
    for(int i = 0; i < 2; ++i)
    {
        Con_Printf("VR: %s %s. %s\n", missionPacks[i], packStatus[i] == 1 ? "available" :
            packStatus[i] == 2 ? "incomplete/corrupt" : "missing",
            packStatus[i] == 1 ? "Campaign and pack resources enabled." :
            "Optional for Quake; copy the owned pack data into its folder to enable it.");
    }
}

[[nodiscard]] bool gameDirAlreadyAdded(const char* game)
{
    // com_gamenames is "a;b;c" (without id1).
    const za::SizeT len = strlen(game);
    for(const char* p = com_gamenames; *p;)
    {
        if(!q_strncasecmp(p, game, len) && (p[len] == ';' || p[len] == '\0'))
        {
            return true;
        }

        p = strchr(p, ';');
        if(!p)
        {
            break;
        }
        ++p;
    }

    return false;
}

char* lastSeparator(char* s)
{
    char* slash = strrchr(s, '/');
    char* backslash = strrchr(s, '\\');
    return slash > backslash ? slash : backslash;
}

// The game folder's name of a search path: its last component, or the folder a pak is in.
void gameFolderName(const char* path, char* out, size_t size)
{
    char dir[MAX_OSPATH];
    q_strlcpy(dir, path, sizeof(dir));
    const char* ext = COM_FileGetExtension(dir);
    if(!q_strcasecmp(ext, "pak") || !q_strcasecmp(ext, "pk3") || !q_strcasecmp(ext, "zip"))
    {
        if(char* cut = lastSeparator(dir))
        {
            *cut = '\0';
        }
    }
    const char* sep = lastSeparator(dir);
    q_strlcpy(out, sep ? sep + 1 : dir, size);
}

} // namespace

// VR_LoadOwnedLocalization's store/rerelease roots, found once (they do not change while the game runs).
static za::Vector<za::String> ownedLocalizationRoots;
static bool ownedLocalizationRootsKnown = false;

// The filesystem asks for roots here without changing its writable basedirs.
// Language tables are borrowed individually; this never mounts their maps/models or
// changes com_basedirs (and therefore cannot redirect saves/configs to a store).
extern "C" char* VR_LoadOwnedLocalization(const char* name)
{
    if(strncmp(name, "localization/loc_", 17) || strchr(name, ':') || strstr(name, "..")) { return nullptr; }
    // The store and rerelease roots only, found once (Steam's, GOG's and Epic's lookups are not free, and the roots do
    // not change while the game runs): a base dir itself is on the search path already, its table read by
    // LOC_ReadFile, and reading it here too parsed and kept every entry twice.
    za::Vector<za::String>& roots = ownedLocalizationRoots;
    if(!ownedLocalizationRootsKnown)
    {
        ownedLocalizationRootsKnown = true;
        for(const za::String& r : ownedRoots())
        {
            bool base = false;
            for(int i = 0; i < com_numbasedirs && !base; ++i) { base = !q_strcasecmp(r.cStr(), com_basedirs[i]); }
            if(!base) { roots.pushBack(r); }
        }
    }
    char* result = nullptr;
    size_t used = 0;
    const auto append = [&](FILE* file, long offset, long length, const char* source)
    {
        constexpr long maxTable = 8 * 1024 * 1024;
        if(length <= 0 || length > maxTable || fseek(file, offset, SEEK_SET)) { return; }
        char* buffer = static_cast<char*>(VR_HeapMalloc(static_cast<size_t>(length) + 1));
        if(fread(buffer, 1, static_cast<size_t>(length), file) != static_cast<size_t>(length))
        { VR_HeapFree(buffer); return; }
        // A separate line between tables preserves the final entry of files without a newline.
        char marker[MAX_OSPATH + 32];
        const size_t markerLength = q_snprintf(marker, sizeof(marker), "// vr-language-source %s\n", source);
        result = static_cast<char*>(VR_HeapRealloc(result, used + markerLength + static_cast<size_t>(length) + 2));
        memcpy(result + used, marker, markerLength);
        used += markerLength;
        memcpy(result + used, buffer, static_cast<size_t>(length));
        used += static_cast<size_t>(length);
        result[used++] = '\n';
        result[used] = 0;
        VR_HeapFree(buffer);
        Con_Printf("[skipnotify]VR language source: %s -> %s\n", name, source);
    };
    for(int r = int(roots.size()) - 1; r >= 0; --r)
    {
        char path[MAX_OSPATH];
        int last = -1;
        for(int pak = 0; pak < 2048; ++pak)
        {
            q_snprintf(path, sizeof(path), "%s/id1/pak%d.pak", roots[r].cStr(), pak);
            FILE* file = fopen(path, "rb");
            if(!file) { break; }
            fclose(file);
            last = pak;
        }
        for(int pak = last; pak >= 0; --pak)
        {
            q_snprintf(path, sizeof(path), "%s/id1/pak%d.pak", roots[r].cStr(), pak);
            FILE* file = fopen(path, "rb");
            if(!file) { continue; }
            fseek(file, 0, SEEK_END);
            const long size = ftell(file);
            rewind(file);
            struct { char id[4]; int32_t dirofs; int32_t dirlen; } header{};
            if(fread(&header, sizeof(header), 1, file) == 1 && !memcmp(header.id, "PACK", 4))
            {
                const int offset = LittleLong(header.dirofs), length = LittleLong(header.dirlen);
                if(offset >= 12 && length >= 0 && length % sizeof(PackEntry) == 0 &&
                    length / sizeof(PackEntry) <= 2048 && offset <= size && length <= size - offset)
                {
                    for(int entry = 0; entry < length / int(sizeof(PackEntry)); ++entry)
                    {
                        PackEntry item{};
                        fseek(file, offset + entry * sizeof(PackEntry), SEEK_SET);
                        if(fread(&item, sizeof(item), 1, file) != 1) { break; }
                        if(!memchr(item.name, 0, sizeof(item.name)) || strcmp(item.name, name)) { continue; }
                        const int pos = LittleLong(item.offset), len = LittleLong(item.length);
                        if(pos >= 12 && len > 0 && pos <= size && len <= size - pos) { append(file, pos, len, path); }
                        break;
                    }
                }
            }
            fclose(file);
        }
        q_snprintf(path, sizeof(path), "%s/id1/%s", roots[r].cStr(), name);
        FILE* loose = fopen(path, "rb");
        if(loose)
        {
            fseek(loose, 0, SEEK_END);
            const long size = ftell(loose);
            append(loose, 0, size, path);
            fclose(loose);
        }
    }
    return result;
}

extern "C" const char* VR_GameDirectoryRoot(const char* dir, int index)
{
    const int i = campaignIndex(dir);
    if(i >= 3 && (nativeCampaignPaths || rebuildingCampaign) && discoveredCampaigns && campaigns[i].root[0])
    { return index == 0 ? campaigns[i].root : nullptr; }
    return index < com_numbasedirs ? com_basedirs[index] : nullptr;
}

extern "C" void VR_PrepareCampaignDirectories(const char* paths)
{
    discoverCampaigns();
    if(rebuildingCampaign) { return; }
    char copy[1024];
    q_strlcpy(copy, paths, sizeof(copy));
    bool vr = false;
    int selected = 0;
    for(char* p = strtok(copy, ";"); p; p = strtok(nullptr, ";"))
    {
        if(!q_strcasecmp(p, vrGameDir)) { vr = true; }
        const int i = campaignIndex(p);
        if(i > 0) { selected = i; }
    }
    nativeCampaignPaths = vr;
    if(vr && selected >= 3 && campaigns[selected].status != 1) // (the packs' status is read when quakevr is added)
    {
        // Its data missing or damaged: nothing of it would be mounted, and its start map (isolated per campaign in
        // VR_SkipSearchPath) found nowhere. Quake instead, said once.
        Con_Printf("VR: %s is %s; starting with Quake. Supply its complete owned data in <basedir>/%s.\n",
            campaigns[selected].folder, campaignStatus(selected), campaigns[selected].folder);
        selected = 0;
    }
    if(vr) { activeCampaign = selected; developerNative = selected >= 3 && !campaigns[selected].nativeReady; }
    publishCampaign();
}

extern "C" void VR_InitCampaignDirectories()
{
    char paths[1024] = {};
    for(int i = 0; i < com_argc - 1; ++i)
    {
        if(!q_strcasecmp(com_argv[i], "-game"))
        { q_strlcat(paths, com_argv[++i], sizeof(paths)); q_strlcat(paths, ";", sizeof(paths)); }
    }
    VR_PrepareCampaignDirectories(paths);
}

namespace
{
bool selectCampaign(int selected, bool developer, bool start)
{
    if(selected < 0 || selected >= int(countof(campaigns))) { return false; }
    Campaign& c = campaigns[selected];
    if(c.status != 1)
    {
        Con_Printf("VR: %s is %s. Supply its complete owned data in <basedir>/%s; detected source %s.\n",
            c.folder, campaignStatus(selected), c.folder, c.root[0] ? c.root : "none");
        return false;
    }
    if(soloOnly(selected) && campaignMultiplayerRequested())
    {
        Con_Printf("VR: %s native readiness covers single-player. Multiplayer context/join/respawn behavior is not accepted; set coop 0, deathmatch 0 and maxplayers 1 before starting.\n", c.title);
        if(!developer) { return false; }
    }
    const char* firstMissing = "none";
    const int languageMissing = selected >= 3 ? missingLanguage(selected, &firstMissing) : 0;
    if(languageMissing)
    {
        Con_Printf("VR: %s language data incomplete: %d missing identifiers (first %s). Supply updated owned rerelease id1/pak*.pak via -basedir, or enable store discovery. Local translations override supplied strings.\n", c.folder, languageMissing, firstMissing);
        if(!developer) { return false; }
    }
    if(!c.nativeReady && !developer)
    {
        Con_Printf("VR: %s native support in progress; campaign play is not ready. Developer testing: vr_campaign_native %s.\n", c.folder, c.folder);
        return false;
    }
    if(!c.nativeReady)
    { Con_Printf("VR: developer native launch of %s: support in progress, gameplay/progression incomplete.\n", c.folder); }
    const bool changed = activeCampaign != selected;
    activeCampaign = selected;
    developerNative = developer && selected >= 3; // (only the native campaigns have gates it opens)
    publishCampaign(true);
    if(changed || !gameDirAlreadyAdded(vrGameDir))
    {
        rebuildingCampaign = true;
        COM_ReloadVRGame("quakevr");
        rebuildingCampaign = false;
    }
    if(start) { Cbuf_InsertText(va("map %s\n", c.start)); }
    return true;
}

void campaignProbeCommand()
{
    if(Cmd_Argc() != 2)
    {
        for(const char* name : {"progs.dat", "maps/start.bsp", "maps/hub.bsp", "maps/end.bsp", "maps/dm1.bsp"})
        {
            const bool exists = COM_FileExists(name, nullptr);
            Con_Printf("VR campaign source: %s -> %s; campaign %s\n", name,
                exists ? com_filesource : "missing", campaigns[activeCampaign].folder);
        }
        return;
    }
    const char* file = Cmd_Argv(1);
    const bool exists = COM_FileExists(file, nullptr);
    Con_Printf("VR campaign source: %s -> %s; campaign %s\n", file,
        exists ? com_filesource : "missing", campaigns[activeCampaign].folder);
    if(!strncmp(file, "maps/", 5))
    { Con_Printf("VR campaign relit: %s -> %s\n", file, VR_ModelFile(file)); }
}

void campaignSelectCommand()
{
    if(Cmd_Argc() != 2) { reportCampaigns(); return; }
    const int i = campaignIndex(Cmd_Argv(1));
    if(i < 0) { Con_Printf("VR: campaign must be id1, hipnotic, rogue, dopa, mg1 or mg3.\n"); return; }
    selectCampaign(i, !q_strcasecmp(Cmd_Argv(0), "vr_campaign_native"), true);
}

// vr_campaign_hub [vrstart|vrstart_old|vrtutorial|vrfiringrange]: Quake's campaign, then that VR map (the hub, vr_hub_map,
// by default). A command of its own: a changelevel there from another campaign cannot rebuild the game folders
// mid-spawn.
void campaignHubCommand()
{
    const char* map = Cmd_Argc() > 1 ? Cmd_Argv(1) : VR_HubMap();
    if(!VR_IsVrMap(map)) { map = VR_HubMap(); }
    if(selectCampaign(0, false, false)) { Cbuf_InsertText(va("map %s\n", map)); }
}
} // namespace

extern "C" const char* VR_CampaignLabel(int index)
{
    return index >= 0 && index < int(countof(campaigns)) ?
        va("%s: %s", campaigns[index].title, campaigns[index].status == 1 && !campaigns[index].nativeReady ?
            "native in progress" : campaignStatus(index)) : "";
}
extern "C" const char* VR_CampaignHelp(int index)
{
    if(index < 0 || index >= int(countof(campaigns))) { return ""; }
    const Campaign& c = campaigns[index];
    return va("%s. Data: %s. %s", c.title, c.root[0] ? c.root : "configured basedirs",
        c.status != 1 ? "Supply complete owned campaign files to play." :
        soloOnly(index) && campaignMultiplayerRequested() ? "Accepted for single-player: set coop 0, deathmatch 0, maxplayers 1; multiplayer context/join/respawn is not accepted." :
        index >= 3 && missingLanguage(index) ? "Language data incomplete: supply updated owned rerelease id1 tables, or enable store discovery." :
        c.nativeReady ? "Starts a new single-player campaign and resets level progress." : "Native gameplay is being ported; campaign play is unavailable.");
}
extern "C" void VR_SelectCampaign(int index)
{ selectCampaign(index, false, true); }
extern "C" int VR_CampaignUnavailable(int index)
{ return index < 0 || index >= int(countof(campaigns)) || campaigns[index].status != 1 || !campaigns[index].nativeReady || (index >= 3 && missingLanguage(index)) ||
    (soloOnly(index) && campaignMultiplayerRequested()); }

extern "C" void VR_BeforeAddGameDirectory(const char* dir)
{
    if(addingMissionPacks || q_strcasecmp(dir, vrGameDir))
    {
        return;
    }

    nativeCampaignPaths = true;
    discoverCampaigns();
    packStatus[0] = inspectPackCached(0, "hipnotic", hipnoticResources, countof(hipnoticResources));
    packStatus[1] = inspectPackCached(1, "rogue", rogueResources, countof(rogueResources));
    addingMissionPacks = true;
    for(int i = 0; i < 2; ++i)
    {
        const char* pack = missionPacks[i];
        if(packStatus[i] == 1 && !gameDirAlreadyAdded(pack))
        {
            COM_AddGameDirectory(pack);
        }
    }
    if(activeCampaign >= 3 && campaigns[activeCampaign].status == 1 &&
        !gameDirAlreadyAdded(campaigns[activeCampaign].folder))
    { COM_AddGameDirectory(campaigns[activeCampaign].folder); }
    // The active map package's own folder (vr_mapinstall.cpp), last before quakevr: over the stock game and the
    // packs, under every file of Quake VR's own.
    qvr::mapinstall::mountActive();
    addingMissionPacks = false;
    campaigns[1].status = packStatus[0];
    campaigns[2].status = packStatus[1];
    publishCampaign();
}

extern "C" int VR_QuakeVRMounted() { return gameDirAlreadyAdded(vrGameDir) ? 1 : 0; }

// The game folders rebuilt as they are (quakevr, its packs, the campaign, the map package now active), the selected
// campaign kept: COM_ReloadVRGame alone would read the campaign from its "quakevr" argument (Quake's).
extern "C" void VR_ReloadVRGameKeepCampaign()
{
    rebuildingCampaign = true;
    COM_ReloadVRGame("quakevr");
    rebuildingCampaign = false;
}

extern "C" void VR_RegisterPackStatus()
{
    Cvar_SetROM("vr_hipnotic_available", packStatus[0] == 1 ? "1" : "0");
    Cvar_SetROM("vr_rogue_available", packStatus[1] == 1 ? "1" : "0");
    Cvar_SetROM("vr_hipnotic_status", va("%d", packStatus[0]));
    Cvar_SetROM("vr_rogue_status", va("%d", packStatus[1]));
    Cmd_AddCommand("vr_pack_status", reportPackStatus);
    Cmd_AddCommand("vr_campaign_menu", VR_OpenCampaignSelector);
    Cmd_AddCommand("vr_campaign_status", reportCampaigns);
    Cmd_AddCommand("vr_campaign_probe", campaignProbeCommand);
    Cmd_AddCommand("vr_campaign_select", campaignSelectCommand);
    Cmd_AddCommand("vr_campaign_native", campaignSelectCommand);
    Cmd_AddCommand("vr_campaign_hub", campaignHubCommand);
    publishCampaign();
    Con_Printf("VR active campaign: %s (%d); schema %d; %s.\n", campaigns[activeCampaign].folder, activeCampaign, campaigns[activeCampaign].schema, campaignStatus(activeCampaign));
    reportPackStatus();
}

// Quake, Scourge of Armagon and Dissolution of Eternity each have a maps/start.bsp; with all
// three layered the last one would always win. vr_activestartpaknameidx (0 Quake, 1 SoA, 2 DoE,
// set by the vrstart hub's buttons) picks the campaign whose start map "start" loads, as the old
// engine's COM_FindFile did for its paks: the other campaigns' folders are skipped for it.
extern "C" int VR_SkipSearchPath(const char* filename, const char* path)
{
    if(qvr::mapinstall::skipSearchPath(path)) // (a map package's folder, while the stock game alone is asked)
    {
        return 1;
    }
    if(!gameDirAlreadyAdded(vrGameDir))
    {
        return 0;
    }

    // The maps more than one campaign has under the same name: start for Quake and its mission packs (they have no end,
    // hub or dm1 of their own: isolating those would hide Quake's while one of them is active); start, end, hub and
    // dm1 while one of the newer campaigns is active (theirs in place of Quake's).
    const bool start = strncmp(filename, "maps/start.", 11) == 0 ||
        (activeCampaign >= 3 && (strncmp(filename, "maps/end.", 9) == 0 || strncmp(filename, "maps/hub.", 9) == 0 ||
                                    strncmp(filename, "maps/dm1.", 9) == 0));
    char name[MAX_OSPATH];
    gameFolderName(path, name, sizeof(name));
    const int i = campaignIndex(name);
    if(i < 0) { return 0; }
    if(!strcmp(filename, "progs.dat") && i >= 3) { return 1; }
    if(i >= 3) { return i != activeCampaign; }
    if(start) { return i != activeCampaign; }
    // Isolate campaign maps and sidecars, while retaining optional pack brush models
    // (e.g. maps/b_explob.bsp) used by VR's global resource precaches.
    if(activeCampaign >= 3 && !strncmp(filename, "maps/", 5) && i != activeCampaign)
    {
        char map[MAX_QPATH];
        COM_StripExtension(filename, map, sizeof(map));
        if(!strcmp(map, "maps/start") || !strcmp(map, "maps/hub") ||
            !strcmp(map, "maps/end") || !strcmp(map, "maps/dm1")) { return 1; }
        const Campaign& c = campaigns[activeCampaign];
        for(size_t r = 0; r < c.resourceCount; ++r)
        {
            if(strncmp(c.resources[r], "maps/", 5)) { continue; }
            char owned[MAX_QPATH];
            COM_StripExtension(c.resources[r], owned, sizeof(owned));
            if(!strcmp(owned, map)) { return 1; }
        }
    }
    return 0;
}

// Relit maps (Misc/quakevr/relight_maps.py, vr_relit_maps): relit/<game>/maps/<map>.bsp, found
// in any game folder (the script writes into quakevr), replaces maps/<map>.bsp when that comes
// from <game>; its .lit sits next to it. A map relit in the game (vr_relight.cpp, vr_relight_use) is in
// relit_custom/<game>/maps/ instead, and wins. Per game, since id1, hipnotic and rogue all have a
// start.bsp and an end.bsp; a mod's own version of a map is left alone.
namespace
{
char relitPath[MAX_QPATH * 2]; // VR_ModelFile's answer: valid until its next call (the main thread)
} // namespace

extern "C" const char* VR_ModelFile(const char* name)
{
    if(!qvr::vr_relit_maps.value || strncmp(name, "maps/", 5) != 0)
    {
        return name;
    }

    unsigned int pathId = 0;
    if(!COM_FileExists(name, &pathId))
    {
        return name;
    }
    const char* folder = com_filesource;
    if(!folder[0]) { return name; }

    char game[MAX_OSPATH];
    gameFolderName(folder, game, sizeof(game));
    char(&relit)[MAX_QPATH * 2] = relitPath;
    // Relit in the game (vr_relight.cpp: relit_custom/<game>/maps/), over relight_maps.py's.
    q_snprintf(relit, sizeof(relit), "relit_custom/%s/%s", game, name);
    // (only while it was made from the map the game finds now: a stale one is moved aside, vr_relight.cpp)
    if(qvr::vr_relight_use.value && COM_FileExists(relit, nullptr) && qvr::relight::customCurrent(game, name))
    {
        return relit;
    }
    q_snprintf(relit, sizeof(relit), "relit/%s/%s", game, name);
    return COM_FileExists(relit, nullptr) ? relit : name;
}

// The game folder a file comes from, as relit/ names it ("maps/e1m1.bsp": id1; a map package's folder name): false
// if there is no such file (vr_relight.cpp).
extern "C" int VR_MapGameFolder(const char* name, char* out, size_t size)
{
    if(!COM_FileExists(name, nullptr) || !com_filesource[0])
    {
        return 0;
    }
    gameFolderName(com_filesource, out, size);
    return out[0] != 0;
}

// COM_SwitchGame, after Mod_ResetAll and the renderer's reload: the caches that hold models' pointers (their slots are
// reused for other models), other games' models by name, or files read from the game folders, emptied. (The ones
// kept per map are emptied at the next map anyway: VR_NewMap's generation, VR_OnClientClearState.)
extern "C" void VR_OnGameDirChanged()
{
    qvr::ao::onGameDirChanged();
    qvr::anchor::onGameDirChanged();
    qvr::detail::onGameDirChanged();
    qvr::emissive::onGameDirChanged();
    qvr::walltorch::onGameDirChanged();
    qvr::ambient::onGameDirChanged();
    qvr::modellight::onGameDirChanged();
    qvr::gfx::onGameDirChanged();
    qvr::bodyblood::clear();
    qvr::wounds::clear();
    qvr::view::resetCaches(); // the view models (the missing ones too), clip sizes, the jointed hand's check, grasp shapes
    qvr::weapons::resetCaches();
    qvr::sightalign::resetCaches(); // the sight lines (a mod's own guns)
    qvr::avatar::reset();
    qvr::flashlight::onGameDirChanged();
    qvr::bigfont::onGameDirChanged();
    qvr::gadget::onGameDirChanged();
    qvr::mem::on(qvr::mem::GameDirChange); // the registered caches that name it (vr_mem.hpp)
    Con_DPrintf("VR: game directory changed: model and game file caches emptied\n");
}

extern "C" void VR_AfterAddGameDirectory(const char* dir)
{
    if(q_strcasecmp(dir, vrGameDir))
    {
        return;
    }

    if(Cvar_FindVar("vr_hipnotic_available"))
    {
        Cvar_SetROM("vr_hipnotic_available", packStatus[0] == 1 ? "1" : "0");
        Cvar_SetROM("vr_rogue_available", packStatus[1] == 1 ? "1" : "0");
        Cvar_SetROM("vr_hipnotic_status", va("%d", packStatus[0]));
        Cvar_SetROM("vr_rogue_status", va("%d", packStatus[1]));
        reportPackStatus();
    }
    standard_quake = true;
    hipnotic = false;
    rogue = false;
}

namespace
{
// The campaign a map's name says it belongs to (e1m1: Quake, hip1m1: Scourge of Armagon, r1m1: Dissolution of Eternity,
// e5m1: Dimension of the Past), or `current`. A name is only a hint: when that campaign's data is not available and the
// map is found where the game looks now (a map package, the user's own id1/quakevr), it is that map, not the
// campaign's: `current` (a package's hipside, or an e5m1 of one's own, is never refused for a campaign not installed).
int campaignForMap(const char* map, int current)
{
    int named = current;
    if((map[0] == 'e' && map[1] >= '1' && map[1] <= '4') || !strcmp(map, "vrtest")) { named = 0; }
    if(!strncmp(map, "hip", 3)) { named = 1; }
    if(!strncmp(map, "r1m", 3) || !strncmp(map, "r2m", 3)) { named = 2; }
    if(!strncmp(map, "e5", 2)) { named = 3; }
    if(named != current && campaigns[named].status != 1 && COM_FileExists(va("maps/%s.bsp", map), nullptr))
    { return current; }
    return named;
}
} // namespace

// Quake VR's own maps: they run in Quake's campaign (a map or a changelevel there from another campaign first
// switches back to it). vrstart is the island hub (Misc/quakevr/maps/vrstart_gen.py; it was vrstart2 until
// 2026-10-07), vrstart_old the old hub (loadable, Debug > Tests > Hubs; nothing goes to it by default).
extern "C" int VR_IsVrMap(const char* map)
{
    return !strcmp(map, "vrstart") || !strcmp(map, "vrstart_old") || !strcmp(map, "vrstart2") || !strcmp(map, "vrtutorial") ||
           !strcmp(map, "vrtutorial2") || !strcmp(map, "vrfiringrange");
}

// The hub: vrstart, or vrstart_old when vr_hub_map names it (vrstart2, the island's old name, is vrstart).
extern "C" const char* VR_HubMap()
{
    return !strcmp(qvr::vr_hub_map.string, "vrstart_old") ? "vrstart_old" : "vrstart";
}

// A map's current name: vrstart2 (the island hub's name until 2026-10-07) is vrstart, vrslipgates (the teleporter
// test map's until 2026-10-08) vrteleporters, so an old save made there, a bind or a script's "map vrstart2" still
// load it (SV_SpawnServer).
extern "C" const char* VR_MapAlias(const char* map)
{
    if(!strcmp(map, "vrstart2")) { return "vrstart"; }
    if(!strcmp(map, "vrslipgates")) { return "vrteleporters"; }
    return map;
}

extern "C" int VR_CanLoadCampaignMap(const char* map)
{
    if(!gameDirAlreadyAdded(vrGameDir)) { return 1; }
    if(VR_IsVrMap(map))
    { return activeCampaign == 0 || selectCampaign(0, false, false); }
    int requested = campaignForMap(map, activeCampaign);
    if(!strcmp(map, "start") && activeCampaign <= 2)
    {
        const int legacy = static_cast<int>(qvr::vr_activestartpaknameidx.value);
        if(legacy < 0 || legacy >= int(countof(campaigns)))
        { Con_Printf("VR: invalid campaign index %d; choose a campaign first.\n", legacy); return 0; }
        // (3 and over: the hub's choice of a campaign with a game folder of its own, which only its teleporter starts:
        // VR_CanChangeCampaignMap; "start" is then the running campaign's)
        requested = legacy <= 2 ? legacy : activeCampaign;
    }
    if(campaigns[requested].status != 1 || requested != activeCampaign ||
        (requested >= 3 && !developerNative && (!campaigns[requested].nativeReady || missingLanguage(requested) || (soloOnly(requested) && campaignMultiplayerRequested()))))
    { return selectCampaign(requested, developerNative, false); }
    char source[MAX_OSPATH] = {};
    if(COM_FileExists(va("maps/%s.bsp", map), nullptr))
    { gameFolderName(com_filesource, source, sizeof(source)); }
    Cvar_SetROM("vr_honey_context", !q_strcasecmp(source, "honey") ? "1" : "0");
    return 1;
}

// SV_SpawnServer (every spawn: map, changelevel, restart, load): the map against the campaign already chosen, never a
// switch. A switch rebuilds the game folders and shuts the server down, which cannot happen inside a spawn (the
// server's progs are switched in: PR_SwitchQCVM's "already active"); map and load choose the campaign before they
// disconnect (VR_CanLoadCampaignMap), changelevel within the campaigns that share their folders (VR_CanChangeCampaignMap).
// Quake, Scourge of Armagon and Dissolution of Eternity share theirs, so a mismatch among them is only bookkeeping;
// any other is refused with a Host_Error (a spawn skipped silently would leave the old server running, its player
// dead or stuck).
extern "C" void VR_CheckSpawnCampaignMap(const char* map)
{
    if(!gameDirAlreadyAdded(vrGameDir)) { return; }
    int requested = campaignForMap(map, activeCampaign);
    if(!strcmp(map, "start") && activeCampaign <= 2)
    {
        const int legacy = static_cast<int>(qvr::vr_activestartpaknameidx.value);
        requested = legacy >= 0 && legacy <= 2 ? legacy : activeCampaign;
    }
    if(VR_IsVrMap(map)) { requested = 0; }
    if(requested != activeCampaign)
    {
        if(requested <= 2 && activeCampaign <= 2 && campaigns[requested].status == 1)
        {
            activeCampaign = requested;
            publishCampaign(true);
        }
        else
        {
            Host_Error("VR: %s belongs to %s, not the campaign now running (%s); choose it in Official Campaigns",
                map, campaigns[requested].title, campaigns[activeCampaign].title);
        }
    }
    if(soloOnly(activeCampaign) && !developerNative && campaignMultiplayerRequested())
    { Host_Error("VR: %s is single-player only; set coop 0, deathmatch 0 and maxplayers 1", campaigns[activeCampaign].title); }
    char source[MAX_OSPATH] = {};
    if(COM_FileExists(va("maps/%s.bsp", map), nullptr))
    { gameFolderName(com_filesource, source, sizeof(source)); }
    Cvar_SetROM("vr_honey_context", !q_strcasecmp(source, "honey") ? "1" : "0");
}

// Entity model indices in a save refer to the original map's precache order. Refuse a
// different installation before disconnecting, instead of restoring wrong/missing models.
extern "C" int VR_CanLoadCampaignSave(const char* text)
{
    if(!gameDirAlreadyAdded(vrGameDir)) { return 1; }
    constexpr const char* key = "\"vr_save_packmask\"";
    const char* marker = strstr(text, key);
    int saved = 4; // Legacy merged VR progs precached both packs on every map.
    if(marker)
    {
        COM_Parse(marker + strlen(key));
        saved = Q_atoi(com_token);
    }
    const int installed = 1 + (packStatus[0] == 1) + 2 * (packStatus[1] == 1);
    if(saved != installed)
    {
        Con_Printf("VR: save uses a different mission-pack installation (saved mask %d, installed %d). "
            "Restore the same owned packs before loading; start a new game to use this installation.\n", saved, installed);
        return 0;
    }
    if(const char* context = strstr(text, "\"vr_save_campaign\""))
    {
        COM_Parse(context + strlen("\"vr_save_campaign\""));
        const int savedCampaign = Q_atoi(com_token);
        if(savedCampaign < 0 || savedCampaign >= int(countof(campaigns)))
        { Con_Printf("VR: save has an invalid campaign context.\n"); return 0; }
        // Ready campaign saves obey ordinary data/solo readiness, including when already active.
        // Keep unfinished developer-save behavior for the campaigns still being ported.
        if(savedCampaign >= 3 && campaigns[savedCampaign].nativeReady)
        { if(!selectCampaign(savedCampaign, false, false)) { return 0; } }
        else if(savedCampaign != activeCampaign &&
                !selectCampaign(savedCampaign, savedCampaign >= 3 && !campaigns[savedCampaign].nativeReady, false))
        { return 0; }
    }
    return 1;
}

extern "C" int VR_IsNewCampaignDirectory(const char* dir) { return campaignIndex(dir) >= 3; }
extern "C" int VR_IsNativeCampaignLaunch()
{
    for(int i = 0; i < com_argc - 1; ++i)
    { if(!q_strcasecmp(com_argv[i], "-game") && !q_strcasecmp(com_argv[i + 1], vrGameDir)) { return 1; } }
    return 0;
}

extern "C" int VR_ShouldMountCampaignDirectory(const char* dir)
{
    const int i = campaignIndex(dir);
    // Native explicit -game/game pack tokens are represented by context only. The
    // VR hook mounts the selected pack after optional packs, before VR overrides.
    return !nativeCampaignPaths || i < 3 ||
        (addingMissionPacks && i == activeCampaign && campaigns[i].status == 1);
}

// Hub portals use changelevel, which preserves spawn parms. Stock selections need
// only context changes; changing the mounted expansion starts a fresh map instead.
extern "C" int VR_CanChangeCampaignMap(const char* map)
{
    if(!gameDirAlreadyAdded(vrGameDir)) { return 1; }
    if(VR_IsVrMap(map) && activeCampaign != 0)
    { Cbuf_InsertText(va("vr_campaign_hub %s\n", map)); return 0; }
    int requested = activeCampaign;
    if(!strcmp(map, "start") && activeCampaign <= 2)
    {
        requested = static_cast<int>(qvr::vr_activestartpaknameidx.value);
        // The hub's choice of a campaign with a game folder of its own (vrstart's lecterns 3, 4 and 5: Dimension of
        // the Past, Dimension of the Machine, Dawn of the Machine): its teleporter runs the selector, which rebuilds the
        // folders and starts the campaign's first map (a changelevel can't). From any other map such a choice is stale:
        // "start" stays the running campaign's.
        if(requested >= 3 && requested < int(countof(campaigns)))
        {
            if(VR_IsVrMap(sv.name))
            {
                // (once in 2 s: a campaign that can't start says why, not every frame its teleporter is touched)
                if(realtime - hubSelectTime > 2.0)
                {
                    hubSelectTime = realtime;
                    Cbuf_InsertText(va("vr_campaign_select %s\n", campaigns[requested].folder));
                }
                return 0;
            }
            requested = activeCampaign;
        }
    }
    else { requested = campaignForMap(map, activeCampaign); }
    if(requested < 0 || requested >= int(countof(campaigns)) || campaigns[requested].status != 1)
    { Con_Printf("VR: campaign unavailable; choose an installed campaign in Official Campaigns.\n"); return 0; }
    if(requested != activeCampaign && (requested >= 3 || activeCampaign >= 3))
    {
        Con_Printf("VR: use the campaign selector to change expansion paths before starting a new campaign.\n");
        return 0;
    }
    activeCampaign = requested;
    publishCampaign(true);
    char source[MAX_OSPATH] = {};
    if(COM_FileExists(va("maps/%s.bsp", map), nullptr))
    { gameFolderName(com_filesource, source, sizeof(source)); }
    Cvar_SetROM("vr_honey_context", !q_strcasecmp(source, "honey") ? "1" : "0");
    return 1;
}

extern "C" int VR_HasNativeCampaignDirectory(const char* paths)
{
    const size_t length = strlen(vrGameDir);
    for(const char* p = paths; p && *p;)
    {
        if(!q_strncasecmp(p, vrGameDir, length) && (p[length] == ';' || !p[length])) { return 1; }
        p = strchr(p, ';');
        if(p) { ++p; }
    }
    return 0;
}
extern "C" int VR_CampaignDataAvailable(const char* dir)
{
    const int i = campaignIndex(dir);
    return i >= 3 && campaigns[i].status == 1;
}

// A file of an owned pack read in place without mounting it (MG3_PLAN.md, "Decisions": an expansion's weapons usable
// in any campaign when its data is there): "owned/<folder>/<path>" names <path> in that discovered Dopa/MG1/MG3 folder
// (its loose file, else its highest pak that has it), whichever campaign is active; the pack's own files never shadow
// Quake VR's (its progs/v_hammer.mdl is the Super Axe, Quake VR's the Hipnotic Mjolnir). Nothing is copied or written.
// Returns 0 when `name` is not such a name, -1 when it is but the file is not there, 1 when found: `out` the pak or
// the loose file, `*offset`/`*length` the file inside it, `*packed` whether `out` is a pak. Any thread (a worker's
// image lookups): it reads only what discovery found at startup, never discovering itself. A sound's name is the
// same under sound/ ("sound/owned/mg3/rogre/ogwake.wav": precache_sound("owned/mg3/rogre/ogwake.wav"), as S_LoadSound
// puts sound/ before it): its pack's sound/rogre/ogwake.wav.
extern "C" int VR_OwnedFile(const char* name, char* out, int size, int* offset, int* length, int* packed)
{
    constexpr char soundPrefix[] = "sound/owned/";
    const bool sound = !q_strncasecmp(name, soundPrefix, sizeof(soundPrefix) - 1);
    if(sound) { name += sizeof("sound/") - 1; }
    constexpr char prefix[] = "owned/";
    if(q_strncasecmp(name, prefix, sizeof(prefix) - 1)) { return 0; }
    const char* folderStart = name + sizeof(prefix) - 1;
    const char* slash = strchr(folderStart, '/');
    if(!slash || slash == folderStart || !slash[1] || strstr(name, "..") || strchr(name, ':') || strchr(name, '\\'))
    { return -1; }
    char folder[MAX_QPATH];
    const size_t folderLength = static_cast<size_t>(slash - folderStart);
    if(folderLength >= sizeof(folder)) { return -1; }
    q_strlcpy(folder, folderStart, folderLength + 1);
    const int c = campaignIndex(folder);
    if(c < 3 || !discoveredCampaigns || campaigns[c].status != 1 || !campaigns[c].root[0]) { return -1; }
    char soundFile[MAX_QPATH];
    const char* file = slash + 1;
    if(sound)
    {
        q_snprintf(soundFile, sizeof(soundFile), "sound/%s", slash + 1);
        file = soundFile;
    }

    char path[MAX_OSPATH];
    q_snprintf(path, sizeof(path), "%s/%s/%s", campaigns[c].root, campaigns[c].folder, file);
    if(Sys_FileType(path) == FS_ENT_FILE)
    {
        FILE* loose = fopen(path, "rb");
        if(loose)
        {
            fseek(loose, 0, SEEK_END);
            const long n = ftell(loose);
            fclose(loose);
            if(n >= 0)
            {
                q_strlcpy(out, path, size);
                *offset = 0;
                *length = static_cast<int>(n);
                *packed = 0;
                return 1;
            }
        }
    }
    int last = -1;
    for(int pak = 0; pak < 64; ++pak)
    {
        q_snprintf(path, sizeof(path), "%s/%s/pak%d.pak", campaigns[c].root, campaigns[c].folder, pak);
        if(Sys_FileType(path) != FS_ENT_FILE) { break; }
        last = pak;
    }
    za::Vector<PackEntry> entries; // (a few lookups a model: not kept)
    for(int pak = last; pak >= 0; --pak)
    {
        q_snprintf(path, sizeof(path), "%s/%s/pak%d.pak", campaigns[c].root, campaigns[c].folder, pak);
        FILE* f = fopen(path, "rb");
        if(!f) { continue; }
        fseek(f, 0, SEEK_END);
        const long fileSize = ftell(f);
        rewind(f);
        struct { char id[4]; int32_t dirofs; int32_t dirlen; } header{};
        bool found = false;
        if(fread(&header, sizeof(header), 1, f) == 1 && !memcmp(header.id, "PACK", 4))
        {
            const long dirOffset = LittleLong(header.dirofs), dirLength = LittleLong(header.dirlen);
            const long count = dirLength / static_cast<long>(sizeof(PackEntry));
            if(dirOffset >= 12 && dirLength >= 0 && dirLength % static_cast<long>(sizeof(PackEntry)) == 0 && count <= 65536 &&
                dirOffset <= fileSize && dirLength <= fileSize - dirOffset && !fseek(f, dirOffset, SEEK_SET))
            {
                entries.resize(static_cast<za::SizeT>(count));
                if(count > 0 && fread(entries.data(), sizeof(PackEntry), static_cast<size_t>(count), f) == static_cast<size_t>(count))
                {
                    for(const PackEntry& e : entries)
                    {
                        if(!memchr(e.name, 0, sizeof(e.name)) || q_strcasecmp(e.name, file)) { continue; }
                        const long pos = LittleLong(e.offset), len = LittleLong(e.length);
                        if(pos >= 12 && len >= 0 && pos <= fileSize && len <= fileSize - pos)
                        {
                            q_strlcpy(out, path, size);
                            *offset = static_cast<int>(pos);
                            *length = static_cast<int>(len);
                            *packed = 1;
                            found = true;
                        }
                        break;
                    }
                }
            }
        }
        fclose(f);
        if(found) { return 1; }
    }
    return -1;
}

// Read-in-place sources for vr_music.cpp. The active campaign's folder while the quakevr folder is mounted (its music
// is chosen per campaign, though Quake's and the two mission packs' folders are mounted together), else null.
extern "C" const char* VR_ActiveCampaignFolder()
{
    return gameDirAlreadyAdded(vrGameDir) ? campaigns[activeCampaign].folder : nullptr;
}

// The owned store/rerelease roots that are not base dirs (a base dir's folders are on the search path already),
// lowest priority first as ownedRoots lists them, found once (Steam's and GOG's lookups are not free); null past the end.
static za::Vector<za::String> ownedReadRoots;
static bool ownedReadRootsKnown = false;
extern "C" const char* VR_OwnedReadRoot(int index)
{
    if(!ownedReadRootsKnown)
    {
        ownedReadRootsKnown = true;
        for(const za::String& r : ownedRoots())
        {
            bool base = false;
            for(int i = 0; i < com_numbasedirs && !base; ++i) { base = !q_strcasecmp(r.cStr(), com_basedirs[i]); }
            if(!base) { ownedReadRoots.pushBack(r); }
        }
    }
    return index >= 0 && index < int(ownedReadRoots.size()) ? ownedReadRoots[index].cStr() : nullptr;
}
