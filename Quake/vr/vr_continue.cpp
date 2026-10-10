// vr_continue.cpp -- Continue and the autosaves (1.0.1: a player's "the game doesn't seem to save its progress; after
// a restart there is no 'continue'").
//
// The saves: the player's own (save/load, the Save menu's s0..s19 and any name), Ironwail's autosaves (sv_autosave 1,
// every sv_autosave_interval seconds of quiet play: host.c's Host_CheckAutosave) and, new, one as each level starts
// (vr_autosave_level_start). An autosave is one file per map: autosave/<map>.sav for Quake's campaign (Ironwail's
// name), autosave/<folder>/<map>.sav for the others' (hipnotic, rogue, dopa, mg1, mg3: their start and end maps share
// names). None on Quake VR's own maps (vr*: the hub, the tutorial, the calibration room, the firing range), none in
// multiplayer (host.c: svs.maxclients 1 only; Quake's save refuses multiplayer anyway).
//
// Continue (the main menu's first row, the hub's Continue button, vr_continue): the newest save of all, by its file's
// time, that is loadable here (its `// qvr_save` line, its campaign keys: VR_SaveCampaign) and of a campaign's map (not
// a vr* map's). Saves of every campaign are in the one game folder (quakevr), so the newest one is the campaign
// played last. Loading it makes its campaign the active one first (VR_CanLoadCampaignSave).

#include "vr_cvars.hpp"
#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace
{

// What Continue offers (the main thread: the menus, the hub's spawn, vr_continue).
struct ContinueSave
{
    bool found{false};
    char relname[MAX_QPATH]{};  // as load takes it: "s0", "autosave/e1m2", "autosave/mg1/start"
    char path[MAX_OSPATH]{};    // com_gamedir's
    char map[MAX_QPATH]{};
    char title[64]{};           // the save's comment's map title ("the Installation")
    int campaign{0};
    time_t when{0};             // the file's time
};
ContinueSave continueSave;

// What the last refresh read (the main thread): the candidate's path and time, so that the same file isn't read again
// at every menu opening; and each file found by the directory walk.
struct ContinueScan
{
    char path[MAX_OSPATH]{};
    time_t when{0};
};
ContinueScan continueScanned;

struct Candidate
{
    char relname[MAX_QPATH];
    char path[MAX_OSPATH];
    time_t when;
};

constexpr int maxCandidates = 256;
Candidate candidates[maxCandidates]; // (the refresh's list: written before it is read, too big for the stack)
int candidateCount = 0;

void addCandidates(const char* subdir)
{
    char dir[MAX_OSPATH];
    if(subdir[0]) { q_snprintf(dir, sizeof(dir), "%s/%s", com_gamedir, subdir); }
    else { q_strlcpy(dir, com_gamedir, sizeof(dir)); }
    for(findfile_t* find = Sys_FindFirst(dir, "sav"); find; find = Sys_FindNext(find))
    {
        if(find->attribs & FA_DIRECTORY || candidateCount >= maxCandidates) { continue; }
        Candidate& c = candidates[candidateCount];
        char base[MAX_QPATH];
        COM_StripExtension(find->name, base, sizeof(base));
        if(subdir[0]) { q_snprintf(c.relname, sizeof(c.relname), "%s/%s", subdir, base); }
        else { q_strlcpy(c.relname, base, sizeof(c.relname)); }
        q_snprintf(c.path, sizeof(c.path), "%s/%s", dir, find->name);
        c.when = 0;
        if(!Sys_GetFileTime(c.path, &c.when)) { continue; }
        ++candidateCount;
    }
}

// The save's text: up to its globals (the campaign keys), whole for a small file.
bool readSave(const char* path, za::Vector<char>& text)
{
    FILE* f = Sys_fopen(path, "rb");
    if(!f) { return false; }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(size <= 0 || size > 64L * 1024 * 1024) { fclose(f); return false; }
    text.resize(static_cast<za::SizeT>(size) + 1);
    const size_t got = fread(text.data(), 1, static_cast<size_t>(size), f);
    fclose(f);
    text[got] = 0;
    return got > 0;
}

// Line `n` (0: the version) of a save, into `out`.
void saveLine(const char* text, int n, char* out, size_t size)
{
    const char* p = text;
    for(int i = 0; i < n && p; ++i)
    {
        p = strchr(p, '\n');
        if(p) { ++p; }
    }
    out[0] = 0;
    if(!p) { return; }
    const size_t len = strcspn(p, "\r\n");
    q_strlcpy(out, p, za::min(size, len + 1));
}

// Whether a save would load here, and what it is: its map (line 19: after the version, the comment, 16 spawn parms and
// the skill), its title (the comment's first 22 characters, '_' for ' '), its campaign.
bool inspect(const Candidate& c, ContinueSave& out)
{
    za::Vector<char> text; // (a command's working buffer: once per menu opening at most)
    if(!readSave(c.path, text)) { return false; }
    char line[128];
    saveLine(text.data(), 0, line, sizeof(line));
    if(Q_atoi(line) != SAVEGAME_VERSION) { return false; }
    const char* qvr = strstr(text.data(), "\n// qvr_save ");
    int format = 0;
    if(!qvr || sscanf(qvr + 1, "// qvr_save %d", &format) != 1 || format > VR_SAVE_FORMAT) { return false; }
    saveLine(text.data(), 19, out.map, sizeof(out.map));
    if(!out.map[0] || !VR_AutosaveAllowed(out.map)) { return false; }
    out.campaign = VR_SaveCampaign(text.data());
    if(out.campaign < 0) { return false; }
    saveLine(text.data(), 1, line, sizeof(line));
    line[za::min<size_t>(22, sizeof(line) - 1)] = 0;
    for(char* p = line; *p; ++p) { if(*p == '_') { *p = ' '; } }
    for(size_t n = strlen(line); n > 0 && line[n - 1] == ' '; --n) { line[n - 1] = 0; }
    q_strlcpy(out.title, line[0] ? line : out.map, sizeof(out.title));
    q_strlcpy(out.relname, c.relname, sizeof(out.relname));
    q_strlcpy(out.path, c.path, sizeof(out.path));
    out.when = c.when;
    out.found = true;
    return true;
}

void refresh(bool verbose)
{
    candidateCount = 0;
    addCandidates("");
    addCandidates("autosave");
    for(int i = 1; i < VR_CampaignCount(); ++i)
    { addCandidates(va("autosave/%s", VR_CampaignFolder(i))); }
    // Newest first: the first one that would load is Continue's (the same file as last time: not read again).
    for(int tried = 0; tried < candidateCount; ++tried)
    {
        int best = -1;
        for(int i = 0; i < candidateCount; ++i)
        {
            if(candidates[i].when >= 0 && (best < 0 || candidates[i].when > candidates[best].when)) { best = i; }
        }
        if(best < 0) { break; }
        Candidate& c = candidates[best];
        if(continueSave.found && !strcmp(c.path, continueScanned.path) && c.when == continueScanned.when)
        {
            if(verbose) { Con_Printf("continue: %s (as before)\n", c.relname); }
            return;
        }
        ContinueSave found;
        if(inspect(c, found))
        {
            continueSave = found;
            q_strlcpy(continueScanned.path, c.path, sizeof(continueScanned.path));
            continueScanned.when = c.when;
            if(verbose) { Con_Printf("continue: %s\n", c.relname); }
            return;
        }
        if(verbose) { Con_Printf("continue: %s passed over (not loadable here, or a Quake VR map's)\n", c.relname); }
        c.when = -1; // (tried)
    }
    continueSave = ContinueSave{};
    continueScanned = ContinueScan{};
}

void publish()
{
    Cvar_SetROM("vr_continue_available", continueSave.found ? "1" : "0");
}

void continueCommand()
{
    VR_ContinueRefresh();
    if(!continueSave.found)
    {
        Con_Printf("No saved game to continue.\n");
        return;
    }
    VR_ContinueStart();
}

// vr_continue_info: what Continue would load, and the newer saves passed over (Debug > Tests).
void continueInfo()
{
    refresh(true);
    publish();
    if(!continueSave.found)
    {
        Con_Printf("Continue: none (no loadable save of a campaign's map in %s)\n", com_gamedir);
        return;
    }
    char title[96], detail[96];
    VR_ContinueDescribe(title, sizeof(title), detail, sizeof(detail));
    Con_Printf("Continue: %s, %s: %s (campaign %d)\n", title, detail, continueSave.relname, continueSave.campaign);
}

} // namespace

extern "C" void VR_AutosaveName(const char* map, char* out, size_t size)
{
    const int campaign = VR_CampaignForMap(map);
    if(campaign <= 0) { q_snprintf(out, size, "autosave/%s", map); }
    else { q_snprintf(out, size, "autosave/%s/%s", VR_CampaignFolder(campaign), map); }
}

extern "C" int VR_AutosaveAllowed(const char* map)
{
    return q_strncasecmp(map, "vr", 2) != 0;
}

extern "C" void VR_ContinueRefresh()
{
    refresh(false);
    publish();
}

extern "C" int VR_ContinueAvailable()
{
    return continueSave.found ? 1 : 0;
}

extern "C" void VR_ContinueDescribe(char* title, size_t titlesize, char* detail, size_t detailsize)
{
    if(!continueSave.found)
    {
        title[0] = 0;
        detail[0] = 0;
        return;
    }
    q_snprintf(title, titlesize, "%s (%s)", continueSave.title, continueSave.map);
    time_t now;
    time(&now);
    char ago[64];
    if(continueSave.when > now - 60) { q_strlcpy(ago, "just now", sizeof(ago)); }
    else
    {
        COM_DescribeDuration(ago, sizeof(ago), difftime(now, continueSave.when));
        q_strlcat(ago, " ago", sizeof(ago));
    }
    q_snprintf(detail, detailsize, "%s, %s", VR_CampaignTitle(continueSave.campaign), ago);
}

extern "C" void VR_ContinueStart()
{
    if(!continueSave.found) { return; }
    Con_Printf("Continue: %s\n", continueSave.relname);
    // Next, before any command queued (inserted: the load, then before it a multiplayer server's settings put back, as
    // the game's start does: a save loads as a single-player game).
    Cbuf_InsertText(va("load \"%s\"\n", continueSave.relname));
    if(svs.maxclients != 1 || coop.value != 0.f || deathmatch.value != 0.f)
    { Cbuf_InsertText("disconnect\nmaxplayers 1\ndeathmatch 0\ncoop 0\n"); }
}

extern "C" void VR_RegisterContinue()
{
    Cmd_AddCommand("vr_continue", continueCommand);
    Cmd_AddCommand("vr_continue_info", continueInfo);
}
