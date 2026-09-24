// vr_gamedir.cpp -- the "quakevr" game folder.
//
// The Quake VR progs contain the id1, Scourge of Armagon (hipnotic) and Dissolution of
// Eternity (rogue) campaigns in one. When the quakevr folder is added to the search path,
// the installed mission packs are layered underneath it automatically, and the engine is
// kept in standard-Quake mode: the mission-pack HUDs and weapon encodings do not apply
// to the VR progs, which use their own weapon IDs.

#include "vr_engine.hpp"

#include <cstring>

extern "C" char com_gamenames[];

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
