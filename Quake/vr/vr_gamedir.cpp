// vr_gamedir.cpp -- the "quakevr" game folder.
//
// The Quake VR progs contain the id1, Scourge of Armagon (hipnotic) and Dissolution of
// Eternity (rogue) campaigns in one. When the quakevr folder is added to the search path,
// the installed mission packs are layered underneath it automatically, and the engine is
// kept in standard-Quake mode: the mission-pack HUDs and weapon encodings do not apply
// to the VR progs, which use their own weapon IDs.

#include "vr_cvars.hpp"
#include "vr_engine.hpp"

#include <cstring>

namespace
{

constexpr const char* vrGameDir = "quakevr";
constexpr const char* missionPacks[] = {"hipnotic", "rogue"};

bool addingMissionPacks = false;

[[nodiscard]] bool gameDirExists(const char* game)
{
    for(int i = 0; i < com_numbasedirs; i++)
    {
        if(Sys_FileType(va("%s/%s", com_basedirs[i], game)) == FS_ENT_DIRECTORY)
        {
            return true;
        }
    }

    return false;
}

[[nodiscard]] bool gameDirAlreadyAdded(const char* game)
{
    // com_gamenames is "a;b;c" (without id1).
    const std::size_t len = strlen(game);
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

    addingMissionPacks = true;
    for(const char* pack : missionPacks)
    {
        if(gameDirExists(pack) && !gameDirAlreadyAdded(pack))
        {
            COM_AddGameDirectory(pack);
        }
    }
    addingMissionPacks = false;
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

    static const char* const campaigns[] = {"id1", "hipnotic", "rogue"};
    const int idx = (static_cast<int>(qvr::vr_activestartpaknameidx.value) % 3 + 3) % 3;
    const char* selected = campaigns[idx];
    if(idx > 0 && !gameDirAlreadyAdded(selected))
    {
        return 0; // not installed: leave the lookup alone
    }

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
    static char relit[MAX_QPATH * 2];
    q_snprintf(relit, sizeof(relit), "relit/%s/%s", game, name);
    return COM_FileExists(relit, nullptr) ? relit : name;
}

extern "C" void VR_AfterAddGameDirectory(const char* dir)
{
    if(q_strcasecmp(dir, vrGameDir))
    {
        return;
    }

    standard_quake = true;
    hipnotic = false;
    rogue = false;
}
