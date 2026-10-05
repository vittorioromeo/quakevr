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
    if(length < 124 || (integer(0) != 29 && integer(0) != 30)) { return false; }
    for(int i = 0; i < 15; ++i)
    {
        const int pos = integer(4 + i * 8), size = integer(8 + i * 8);
        if(pos < 0 || size < 0 || pos > length || size > length - pos) { return false; }
    }
    return integer(120) >= 64; // at least the world model
}

int inspectPack(const char* game, const char* const* resources, size_t count)
{
    za::Vector<unsigned char>& found = packScratch.found;
    found.assign(count, 0);
    bool directory = false;
    for(int base = 0; base < com_numbasedirs; ++base)
    {
        char folder[MAX_OSPATH];
        q_snprintf(folder, sizeof(folder), "%s/%s", com_basedirs[base], game);
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

extern "C" void VR_BeforeAddGameDirectory(const char* dir)
{
    if(addingMissionPacks || q_strcasecmp(dir, vrGameDir))
    {
        return;
    }

    packStatus[0] = inspectPack("hipnotic", hipnoticResources, sizeof(hipnoticResources) / sizeof(*hipnoticResources));
    packStatus[1] = inspectPack("rogue", rogueResources, sizeof(rogueResources) / sizeof(*rogueResources));
    addingMissionPacks = true;
    for(int i = 0; i < 2; ++i)
    {
        const char* pack = missionPacks[i];
        if(packStatus[i] == 1 && !gameDirAlreadyAdded(pack))
        {
            COM_AddGameDirectory(pack);
        }
    }
    addingMissionPacks = false;
}

extern "C" void VR_RegisterPackStatus()
{
    Cvar_SetROM("vr_hipnotic_available", packStatus[0] == 1 ? "1" : "0");
    Cvar_SetROM("vr_rogue_available", packStatus[1] == 1 ? "1" : "0");
    Cvar_SetROM("vr_hipnotic_status", va("%d", packStatus[0]));
    Cvar_SetROM("vr_rogue_status", va("%d", packStatus[1]));
    Cmd_AddCommand("vr_pack_status", reportPackStatus);
    reportPackStatus();
}

// Quake, Scourge of Armagon and Dissolution of Eternity each have a maps/start.bsp; with all
// three layered the last one would always win. vr_activestartpaknameidx (0 Quake, 1 SoA, 2 DoE,
// set by the vrstart hub's buttons) picks the campaign whose start map "start" loads, as the old
// engine's COM_FindFile did for its paks: the other campaigns' folders are skipped for it.
extern "C" int VR_SkipSearchPath(const char* filename, const char* path)
{
    if(strncmp(filename, "maps/start.", 11) != 0 || !gameDirAlreadyAdded(vrGameDir))
    {
        return 0;
    }

    static constexpr const char* campaigns[] = {"id1", "hipnotic", "rogue"};
    const int idx = (static_cast<int>(qvr::vr_activestartpaknameidx.value) % 3 + 3) % 3;
    const char* selected = campaigns[(idx > 0 && packStatus[idx - 1] != 1) ? 0 : idx];

    char name[MAX_OSPATH];
    gameFolderName(path, name, sizeof(name));

    for(const char* campaign : campaigns)
    {
        if(!q_strcasecmp(name, campaign))
        {
            return q_strcasecmp(name, selected) != 0;
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
    const char* folder = nullptr;
    for(const searchpath_t* search = com_searchpaths; search; search = search->next)
    {
        if(search->path_id == pathId && !search->pack)
        {
            folder = search->filename;
            break;
        }
    }
    if(!folder)
    {
        return name;
    }

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

extern "C" int VR_CanLoadCampaignMap(const char* map)
{
    if(!gameDirAlreadyAdded(vrGameDir)) { return 1; }
    int pack = -1;
    if(!strncmp(map, "hip", 3)) { pack = 0; }
    if(!strncmp(map, "r1m", 3) || !strncmp(map, "r2m", 3)) { pack = 1; }
    if(!strcmp(map, "start"))
    {
        const int idx = (static_cast<int>(qvr::vr_activestartpaknameidx.value) % 3 + 3) % 3;
        if(idx > 0) { pack = idx - 1; }
    }
    if(pack >= 0 && packStatus[pack] != 1)
    {
        Con_Printf("VR: cannot load %s: %s is unavailable; restore its owned data or select Quake in the VR Hub.\n",
            map, missionPacks[pack]);
        return 0;
    }
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
    return 1;
}
