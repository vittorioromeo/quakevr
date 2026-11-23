// vr_quoth.cpp -- Quoth support, the engine's side (vr_quoth.hpp; docs/vr-port/QUOTH.md).

#include "vr_quoth.hpp"
#include "vr_engine.hpp"
#include "vr_sha256.hpp"
#include "vr_strings.hpp"

#include "vr_api.h"

#include <cstdio>

namespace
{

struct Shadowed
{
    const char* name;
    int owner; // 0 id1, 1 hipnotic, 2 rogue, 3 ad
};

#define VR_QUOTH_SHADOWED(name, owner) {name, owner},
constexpr Shadowed shadowedFiles[] = {
#include "vr_quoth_data.inc"
};
#undef VR_QUOTH_SHADOWED

constexpr const char* ownerStatus[] = {nullptr, "vr_hipnotic_status", "vr_rogue_status", "vr_ad_status"};

// Quoth's own files that are not content: its progs, its configs (they bind keys, set its cvars), its demos.
constexpr const char* notContent[] = {
    "quake.rc", "progs.dat", "csprogs.dat", "default.cfg", "optout.cfg", "quoth.cfg", "autoexec.cfg",
};

bool versionWarned = false; // (the "another version" line said once)

} // namespace

namespace qvr::quoth
{

int versionStatus(const char* folder)
{
    char path[MAX_OSPATH];
    q_snprintf(path, sizeof(path), "%s/pak2.pak", folder);
    Sys_MatchFileCase(path);
    const int status = qvr::sha256::fileMatches(path, VR_QUOTH_PIN_SIZE, VR_QUOTH_PIN_SHA) ? 1 : 3;
    if(status != 1 && !versionWarned)
    {
        versionWarned = true;
        Con_Printf("VR: Quoth in %s is not 2.2 (its pak2.pak differs). Quake VR plays Quoth 2.2 (quoth2pt2full_2.zip): "
                   "install it over this one.\n", folder);
    }
    return status;
}

bool skipInFolder(const char* filename)
{
    for(const char* n : notContent)
    {
        if(!q_strcasecmp(filename, n)) { return true; }
    }
    if(qvr::strings::endsWith(filename, ".dem")) { return true; }
    for(const Shadowed& s : shadowedFiles) // (87 names, sorted; a lookup per file search while Quoth is mounted)
    {
        if(q_strcasecmp(filename, s.name)) { continue; }
        return s.owner == 0 || Cvar_VariableValue(ownerStatus[s.owner]) == 1.f;
    }
    return false;
}

bool skipOutside(const char* filename)
{
    using qvr::strings::startsWith;
    return skipInFolder(filename) ||
           !(startsWith(filename, "progs/") || startsWith(filename, "sound/") || startsWith(filename, "gfx/env/"));
}

bool folderMounted()
{
    for(const searchpath_t* s = com_searchpaths; s; s = s->next)
    {
        char name[MAX_OSPATH];
        VR_GameFolderName(s->pack ? s->pack->filename : s->filename, name, sizeof(name));
        if(!q_strcasecmp(name, "quoth")) { return true; }
    }
    return false;
}

} // namespace qvr::quoth
