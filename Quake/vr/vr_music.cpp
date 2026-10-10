// vr_music.cpp -- CD tracks read in place, per campaign.
//
// The original Steam Quake ships no music: its soundtrack and the mission packs' are in the 2021 rerelease's
// <game>/music folders (GOG's original keeps its tracks next to its game), and Quake VR mounts none of them, so the
// game was silent. A track is looked for in the active campaign's folder, then in Quake's (id1): first where the
// search path has that folder, then in the owned store/rerelease installs, which are read where they are (never
// mounted, copied or written: no base dir changes). A game folder that is no campaign's (a mod, a map package, the
// quakevr folder) that has the track plays it first, as the search path always did.
//
// A campaign's own track (target_vr_music's "music_folder": the tutorial's arena plays Scourge of Armagon's): a CD
// track number above 127 says whose, 128 + 16 * the folder's place in campaignFolders + the track (2 to 15), looked for
// in that folder only (the search path's, the owned installs'), whatever the active campaign; one byte as every track
// is, so svc_cdtrack, the serverinfo and a saved game carry it unchanged.

#include "vr_engine.hpp"

#include <cstdio>
#include <cstring>

namespace
{
constexpr const char* campaignFolders[] = {"id1", "hipnotic", "rogue", "dopa", "mg1", "mg3"};
constexpr int maxTracks = 256;
constexpr int campaignTrackBase = 128;
constexpr int campaignTrackSpan = 16;
constexpr const char* trackStems[] = {"track", "Track"};

// The GOG original's install (its tracks may be in <install>/music), found once; empty when it is not installed.
char gogOriginalRoot[MAX_OSPATH] = {};
bool gogOriginalKnown = false;
// A missing track is said once per track (a map reload, or every map of an episode, is not a new message).
bool missingSaid[maxTracks] = {};

bool isCampaignFolder(const char* name)
{
    for(const char* f : campaignFolders)
    {
        if(!q_strcasecmp(name, f)) { return true; }
    }
    return false;
}

const char* lastSeparator(const char* s)
{
    const char* slash = strrchr(s, '/');
    const char* backslash = strrchr(s, '\\');
    return slash > backslash ? slash : backslash;
}

// The game folder's name of a search path entry: its last component, or the folder its pak is in.
void folderOf(const searchpath_t* sp, char* out, size_t size)
{
    char dir[MAX_OSPATH];
    q_strlcpy(dir, sp->pack ? sp->pack->filename : sp->filename, sizeof(dir));
    if(sp->pack)
    {
        if(char* cut = const_cast<char*>(lastSeparator(dir))) { *cut = '\0'; }
    }
    const char* sep = lastSeparator(dir);
    q_strlcpy(out, sep ? sep + 1 : dir, size);
}

// A loose file's size, or -1.
long looseFileSize(const char* path)
{
    if(Sys_FileType(path) != FS_ENT_FILE) { return -1; }
    FILE* file = fopen(path, "rb");
    if(!file) { return -1; }
    long size = -1;
    if(!fseek(file, 0, SEEK_END)) { size = ftell(file); }
    fclose(file);
    return size;
}

void setName(vr_musicfile_t* out, int track, const char* ext)
{
    q_snprintf(out->name, sizeof(out->name), "music/track%02d.%s", track, ext);
    q_strlcpy(out->ext, ext, sizeof(out->ext));
}

// The track in one search path entry (a pak, or a folder's loose files), the first extension that has it.
bool findInSearchPath(const searchpath_t* sp, int track, const char* const* exts, int numExts, vr_musicfile_t* out)
{
    for(int e = 0; e < numExts; ++e)
    {
        char name[MAX_QPATH];
        q_snprintf(name, sizeof(name), "music/track%02d.%s", track, exts[e]);
        if(sp->pack)
        {
            for(int i = 0; i < sp->pack->numfiles; ++i)
            {
                const packfile_t& f = sp->pack->files[i];
                if(q_strcasecmp(f.name, name) || f.filelen <= 0) { continue; }
                q_strlcpy(out->path, sp->pack->filename, sizeof(out->path));
                out->offset = f.filepos;
                out->length = f.filelen;
                out->pak = 1;
                setName(out, track, exts[e]);
                return true;
            }
            continue;
        }
        char path[MAX_OSPATH];
        q_snprintf(path, sizeof(path), "%s/%s", sp->filename, name);
        const long size = looseFileSize(path);
        if(size <= 0) { continue; }
        q_strlcpy(out->path, path, sizeof(out->path));
        out->offset = 0;
        out->length = size;
        out->pak = 0;
        setName(out, track, exts[e]);
        return true;
    }
    return false;
}

// The track in an owned install: <root>/<folder>/music/trackNN.ext (the rerelease's layout), or for Quake's own
// tracks <root>/music/trackNN.ext (the GOG original's). Both spellings of the name ("Track02" on a case-sensitive disk).
bool findInRoot(const char* root, const char* folder, int track, const char* const* exts, int numExts, vr_musicfile_t* out)
{
    const bool quake = !q_strcasecmp(folder, "id1");
    for(int e = 0; e < numExts; ++e)
    {
        for(int layout = 0; layout < (quake ? 2 : 1); ++layout)
        {
            for(const char* stem : trackStems)
            {
                char path[MAX_OSPATH];
                if(layout == 0) { q_snprintf(path, sizeof(path), "%s/%s/music/%s%02d.%s", root, folder, stem, track, exts[e]); }
                else { q_snprintf(path, sizeof(path), "%s/music/%s%02d.%s", root, stem, track, exts[e]); }
                const long size = looseFileSize(path);
                if(size <= 0) { continue; }
                q_strlcpy(out->path, path, sizeof(out->path));
                out->offset = 0;
                out->length = size;
                out->pak = 0;
                setName(out, track, exts[e]);
                q_strlcpy(out->source, root, sizeof(out->source));
                return true;
            }
        }
    }
    return false;
}

const char* gogOriginal()
{
    if(!gogOriginalKnown)
    {
        gogOriginalKnown = true;
        if(COM_CheckParm("-nogog") || !Sys_GetGOGQuakeDir(gogOriginalRoot, sizeof(gogOriginalRoot)))
        { gogOriginalRoot[0] = '\0'; }
    }
    return gogOriginalRoot;
}

// The track in one campaign folder: where the search path has it, then the owned installs (last, highest priority,
// first), the GOG original last of all.
bool findInFolder(const char* folder, int track, const char* const* exts, int numExts, vr_musicfile_t* out)
{
    for(const searchpath_t* sp = com_searchpaths; sp; sp = sp->next)
    {
        char name[MAX_OSPATH];
        folderOf(sp, name, sizeof(name));
        if(q_strcasecmp(name, folder) || !findInSearchPath(sp, track, exts, numExts, out)) { continue; }
        q_strlcpy(out->source, sp->pack ? sp->pack->filename : sp->filename, sizeof(out->source));
        return true;
    }
    int roots = 0;
    while(VR_OwnedReadRoot(roots)) { ++roots; }
    for(int r = roots - 1; r >= 0; --r)
    {
        if(findInRoot(VR_OwnedReadRoot(r), folder, track, exts, numExts, out)) { return true; }
    }
    return gogOriginal()[0] && findInRoot(gogOriginal(), folder, track, exts, numExts, out);
}

void sayMissing(int code, const char* what)
{
    if(missingSaid[code]) { return; }
    missingSaid[code] = true;
    Con_Printf("[skipnotify]VR music: no %s in the game folders or the owned installs\n", what);
}

} // namespace

extern "C" int VR_MusicCampaignTrack(const char* folder, int track)
{
    if(!folder || track < 2 || track >= campaignTrackSpan) { return 0; }
    for(int i = 0; i < int(countof(campaignFolders)); ++i)
    {
        if(!q_strcasecmp(folder, campaignFolders[i])) { return campaignTrackBase + campaignTrackSpan * i + track; }
    }
    return 0;
}

extern "C" int VR_FindMusicTrack(int track, const char* const* exts, int numExts, vr_musicfile_t* out)
{
    if(track < 0 || track >= maxTracks || numExts <= 0) { return 0; }
    *out = vr_musicfile_t{};

    if(track >= campaignTrackBase) // a campaign's own track: that folder's only
    {
        const int folder = (track - campaignTrackBase) / campaignTrackSpan, n = (track - campaignTrackBase) % campaignTrackSpan;
        if(folder >= int(countof(campaignFolders))) { return 0; }
        if(findInFolder(campaignFolders[folder], n, exts, numExts, out)) { return 1; }
        char what[64];
        q_snprintf(what, sizeof(what), "%s track %d (track %d)", campaignFolders[folder], n, track);
        sayMissing(track, what);
        return 0;
    }

    // A mod's or map package's own track wins, as the search path says.
    vr_musicfile_t top{};
    char topFolder[MAX_OSPATH] = {};
    bool haveTop = false;
    for(const searchpath_t* sp = com_searchpaths; sp && !haveTop; sp = sp->next)
    {
        if(!findInSearchPath(sp, track, exts, numExts, &top)) { continue; }
        haveTop = true;
        folderOf(sp, topFolder, sizeof(topFolder));
        q_strlcpy(top.source, sp->pack ? sp->pack->filename : sp->filename, sizeof(top.source));
    }
    if(haveTop && !isCampaignFolder(topFolder)) { *out = top; return 1; }

    // The active campaign's track, then Quake's. Without the quakevr folder (plain Quake, a mod), every campaign
    // folder on the search path in its order.
    const char* chain[countof(campaignFolders) + 1] = {};
    int chainSize = 0;
    const auto addFolder = [&](const char* f)
    {
        for(int i = 0; i < chainSize; ++i)
        {
            if(!q_strcasecmp(chain[i], f)) { return; }
        }
        if(chainSize < int(countof(chain))) { chain[chainSize++] = f; }
    };
    if(const char* active = VR_ActiveCampaignFolder()) { addFolder(active); }
    else
    {
        for(const searchpath_t* sp = com_searchpaths; sp; sp = sp->next)
        {
            char folder[MAX_OSPATH];
            folderOf(sp, folder, sizeof(folder));
            for(const char* f : campaignFolders)
            {
                if(!q_strcasecmp(folder, f)) { addFolder(f); }
            }
        }
    }
    addFolder("id1");

    // Where the search path has that folder (the user's own data, a rerelease mounted as a base dir), then the owned
    // installs.
    for(int c = 0; c < chainSize; ++c)
    {
        if(findInFolder(chain[c], track, exts, numExts, out)) { return 1; }
    }

    // Another campaign's track on the search path rather than silence (what the search path alone chose before).
    if(haveTop) { *out = top; return 1; }

    // (tracks 0 and 1 are no music: a map without any, the CD's data track)
    if(track >= 2)
    {
        char what[32];
        q_snprintf(what, sizeof(what), "track %d", track);
        sayMissing(track, what);
    }
    return 0;
}
