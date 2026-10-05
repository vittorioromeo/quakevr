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
#include "vr_bodyblood.hpp"
#include "vr_cvars.hpp"
#include "vr_detail.hpp"
#include "vr_emissive.hpp"
#include "vr_engine.hpp"
#include "vr_walltorch.hpp"
#include "vr_gfx.hpp"
#include "vr_mapinstall.hpp"
#include "vr_mem.hpp"
#include "vr_modellight.hpp"
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
}


namespace
{

constexpr const char* vrGameDir = "quakevr";
constexpr const char* missionPacks[] = {"hipnotic", "rogue"};

bool addingMissionPacks = false;

// 0 missing, 1 available, 2 incomplete/corrupt. Inspect the pack's own files, never
// the VR overrides: shipping a view model is not evidence of owning the campaign.
int packStatus[2] = {};
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
    bool directory = false;
    for(int base = 0; base < (onlyRoot ? 1 : com_numbasedirs); ++base)
    {
        char folder[MAX_OSPATH];
        q_snprintf(folder, sizeof(folder), "%s/%s", onlyRoot ? onlyRoot : com_basedirs[base], game);
        directory |= Sys_FileType(folder) == FS_ENT_DIRECTORY;
        for(int pak = 0;; ++pak)
        {
            char path[MAX_OSPATH];
            q_snprintf(path, sizeof(path), "%s/pak%d.pak", folder, pak);
            FILE* file = fopen(path, "rb");
            if(!file) { break; }
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
            }
        }
    }
    for(size_t i = 0; i < count; ++i)
    {
        if(!found[i])
        {
            if(directory)
            {
                Con_Printf("VR: %s: incomplete installation (missing %s); restore your owned mission-pack data.\n",
                    game, resources[i]);
            }
            return directory ? 2 : 0;
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
    static za::U64 signatures[2] = {};
    static int statuses[2] = {};
    static bool known[2] = {};
    const za::U64 now = packSignature(game, resources, count);
    if(!known[which] || signatures[which] != now)
    {
        statuses[which] = inspectPack(game, resources, count);
        signatures[which] = now;
        known[which] = true;
    }
    return statuses[which];
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
Campaign campaigns[] = {
    {"id1", "Quake", "start", 0, true, nullptr, 0, 1, {}},
    {"hipnotic", "Scourge of Armagon", "start", 1, true, nullptr, 0, 0, {}},
    {"rogue", "Dissolution of Eternity", "start", 2, true, nullptr, 0, 0, {}},
    {"dopa", "Dimension of the Past", "e5start", 3, false, dopaResources, countof(dopaResources), 0, {}},
    {"mg1", "Dimension of the Machine", "start", 4, false, mg1Resources, countof(mg1Resources), 0, {}},
    {"mg3", "Dawn of the Machine", "start", 5, false, mg3Resources, countof(mg3Resources), 0, {}},
};
int activeCampaign = 0;
bool discoveredCampaigns = false;
bool developerNative = false;
bool rebuildingCampaign = false;
bool nativeCampaignPaths = false;

int campaignIndex(const char* name)
{
    for(int i = 0; i < int(countof(campaigns)); ++i)
    {
        if(!q_strcasecmp(name, campaigns[i].folder)) { return i; }
    }
    return -1;
}

void discoverCampaigns()
{
    if(discoveredCampaigns) { return; }
    discoveredCampaigns = true;
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
    for(int i = 0; i < com_numbasedirs; ++i)
    {
        q_snprintf(path, sizeof(path), "%s/../rerelease", com_basedirs[i]);
        addRoot(path);
        q_snprintf(path, sizeof(path), "%s/rerelease", com_basedirs[i]);
        addRoot(path);
        addRoot(com_basedirs[i]);
    }
    for(int i = 3; i < int(countof(campaigns)); ++i)
    {
        Campaign& c = campaigns[i];
        for(int r = int(roots.size()) - 1; r >= 0; --r)
        {
            char folder[MAX_OSPATH];
            q_snprintf(folder, sizeof(folder), "%s/%s", roots[r].cStr(), c.folder);
            if(Sys_FileType(folder) != FS_ENT_DIRECTORY) { continue; }
            c.status = inspectPack(c.folder, c.resources, c.resourceCount, roots[r].cStr());
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

const char* campaignStatus(int i)
{
    return campaigns[i].status == 0 ? "missing" : campaigns[i].status == 2 ? "incomplete/corrupt" :
        campaigns[i].nativeReady ? "ready" : "installed; native support in progress";
}

void reportCampaigns()
{
    for(int i = 0; i < int(countof(campaigns)); ++i)
    {
        const Campaign& c = campaigns[i];
        Con_Printf("VR campaign %d %s: %s; start %s; schema %d; source %s\n",
            i, c.folder, campaignStatus(i), c.start, c.schema, c.root[0] ? c.root : "configured basedirs");
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

// The filesystem asks for roots here without changing its writable basedirs.
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
    if(vr) { activeCampaign = selected; developerNative = selected >= 3; }
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
    if(!c.nativeReady && !developer)
    {
        Con_Printf("VR: %s native support in progress; campaign play is not ready. Developer testing: vr_campaign_native %s.\n", c.folder, c.folder);
        return false;
    }
    if(!c.nativeReady)
    { Con_Printf("VR: developer native launch of %s: support in progress, gameplay/progression incomplete.\n", c.folder); }
    const bool changed = activeCampaign != selected;
    activeCampaign = selected;
    developerNative = developer;
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

void campaignHubCommand()
{
    if(selectCampaign(0, false, false)) { Cbuf_InsertText("map vrstart\n"); }
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
        c.nativeReady ? "Starts a new campaign and resets level progress." : "Native gameplay is being ported; campaign play is unavailable.");
}
extern "C" void VR_SelectCampaign(int index)
{ selectCampaign(index, false, true); }
extern "C" int VR_CampaignUnavailable(int index)
{ return index < 0 || index >= int(countof(campaigns)) || campaigns[index].status != 1 || !campaigns[index].nativeReady; }

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

    const bool start = strncmp(filename, "maps/start.", 11) == 0 ||
        strncmp(filename, "maps/end.", 9) == 0 || strncmp(filename, "maps/hub.", 9) == 0 ||
        strncmp(filename, "maps/dm1.", 9) == 0;
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
// from <game>; its .lit sits next to it. Per game, since id1, hipnotic and rogue all have a
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
    q_snprintf(relit, sizeof(relit), "relit/%s/%s", game, name);
    return COM_FileExists(relit, nullptr) ? relit : name;
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

extern "C" int VR_CanLoadCampaignMap(const char* map)
{
    if(!gameDirAlreadyAdded(vrGameDir)) { return 1; }
    if(!strcmp(map, "vrstart") || !strcmp(map, "vrtutorial") || !strcmp(map, "vrfiringrange"))
    { return activeCampaign == 0 || selectCampaign(0, false, false); }
    int requested = campaignForMap(map, activeCampaign);
    if(!strcmp(map, "start") && activeCampaign <= 2)
    {
        const int legacy = static_cast<int>(qvr::vr_activestartpaknameidx.value);
        if(legacy < 0 || legacy >= int(countof(campaigns)))
        { Con_Printf("VR: invalid campaign index %d; choose a campaign first.\n", legacy); return 0; }
        requested = legacy;
    }
    if(campaigns[requested].status != 1 || requested != activeCampaign)
    { return selectCampaign(requested, developerNative, false); }
    char source[MAX_OSPATH] = {};
    if(COM_FileExists(va("maps/%s.bsp", map), nullptr))
    { gameFolderName(com_filesource, source, sizeof(source)); }
    Cvar_SetROM("vr_honey_context", !q_strcasecmp(source, "honey") ? "1" : "0");
    return 1;
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
        if(savedCampaign != activeCampaign && !selectCampaign(savedCampaign, true, false)) { return 0; }
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
    if(!strcmp(map, "vrstart") && activeCampaign != 0)
    { Cbuf_InsertText("vr_campaign_hub\n"); return 0; }
    int requested = activeCampaign;
    if(!strcmp(map, "start") && activeCampaign <= 2)
    { requested = static_cast<int>(qvr::vr_activestartpaknameidx.value); }
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
