// vr_dwell.cpp -- Dwell support, the engine's side (vr_dwell.hpp; docs/vr-port/DWELL.md).

#include "vr_dwell.hpp"
#include "vr_engine.hpp"
#include "vr_sha256.hpp"
#include "vr_strings.hpp"

#include "Zancle/Container/Vector.hpp"
#include "vr_api.h"

#include <cstdio>

namespace
{

struct Shadowed
{
    const char* name;
    int owner; // 0 id1, 1 hipnotic, 2 rogue
};

#define VR_DWELL_SHADOWED(name, owner) {name, owner},
constexpr Shadowed shadowedFiles[] = {
#include "vr_dwell_data.inc"
};
#undef VR_DWELL_SHADOWED

bool versionWarned = false; // (the "another version" line said once)

} // namespace

namespace qvr::dwell
{

int versionStatus(const char* folder)
{
    char path[MAX_OSPATH];
    q_snprintf(path, sizeof(path), "%s/progs.dat", folder);
    const int status = qvr::sha256::fileMatches(path, VR_DWELL_PROGS_SIZE, VR_DWELL_PROGS_SHA) ? 1 : 3;
    if(status != 1 && !versionWarned)
    {
        versionWarned = true;
        Con_Printf("VR: Dwell in %s is not v2.2 (its progs.dat differs). Quake VR plays Dwell v2.2 (dwellv2p2.zip): "
                   "install it over this one.\n", folder);
    }
    return status;
}

bool skipInFolder(const char* filename)
{
    if(!q_strcasecmp(filename, "quake.rc") || !q_strcasecmp(filename, "progs.dat") ||
        !q_strcasecmp(filename, "csprogs.dat") || !q_strcasecmp(filename, "default.cfg"))
    {
        return true;
    }
    for(const Shadowed& s : shadowedFiles) // (64 names, sorted; a lookup per file search while Dwell is active)
    {
        if(q_strcasecmp(filename, s.name)) { continue; }
        return s.owner == 0 || Cvar_VariableValue(s.owner == 1 ? "vr_hipnotic_status" : "vr_rogue_status") == 1.f;
    }
    return false;
}

bool skipOutside(const char* filename)
{
    return skipInFolder(filename) ||
           !(qvr::strings::startsWith(filename, "progs/") || qvr::strings::startsWith(filename, "sound/"));
}

bool folderMounted()
{
    for(const searchpath_t* s = com_searchpaths; s; s = s->next)
    {
        char name[MAX_OSPATH];
        VR_GameFolderName(s->pack ? s->pack->filename : s->filename, name, sizeof(name));
        if(!q_strcasecmp(name, "dwell") || !q_strcasecmp(name, "dwellv2p2")) { return true; }
    }
    return false;
}

} // namespace qvr::dwell
