// vr_continue.cpp -- the saves: Continue, the autosaves' names, what each save was played as (1.0.1: a player's "the
// game doesn't seem to save its progress; after a restart there is no 'continue'"; then the author's one-folder design).
//
// The saves: the player's own (save/load, the Save menu's s0..s19 and any name), Ironwail's autosaves (sv_autosave 1,
// every sv_autosave_interval seconds of quiet play: host.c's Host_CheckAutosave) and one as each level starts
// (vr_autosave_level_start). All in the one game folder (quakevr/ and quakevr/autosave/), whatever the campaign or the
// map package. An autosave is one file per map, its name unique across the campaigns and the packages:
// autosave/<map>.sav for Quake's campaign (Ironwail's name: its Play Custom Map's Resume, and 1.0.0's autosaves),
// autosave/<campaign folder>~<map>.sav for the others' (mg1~start: their start and end maps share names), and
// autosave/<package>~<map>.sav for a Map Library package's map (<package>: the first 16 of its sha256, the name of its
// folder in qvr_addons). They are named by what is running (the active campaign, the mounted package), never by the
// map's name. Each kept in vr_autosave_slots rotating slots (player feedback: an autosave in an ambush trapped them):
// autosave/<name>.sav the newest (the name above, so Continue, Resume and older saves are unchanged), <name>@2.sav,
// <name>@3.sav... the older ones (VR_AutosaveRotate, by Host_Savegame_f before an autosave is written), listed newest
// first by the Load Autosave menu and the death menu (menu.c M_Autosaves). None on Quake VR's own maps (vr*: the hub, the tutorial, the calibration room, the firing range), none in
// multiplayer (host.c: svs.maxclients 1 only; Quake's save refuses multiplayer anyway), none in Setup's preparation.
//
// Every save says what it was played as, in comment lines after its model list (SaveData_WriteHeader; COM_Parse skips
// them, so 1.0.0 reads these saves): `// qvr_campaign <n>`, `// qvr_package <sha256>` (nothing after the key: none),
// `// qvr_package_title <title>`, `// qvr_map_title <title>`. A save from before them: the campaign from its
// vr_save_campaign global (world.qc, since 2026-10-05), the package from the <save>.addon beside it. Loading any save
// (the Load menu, `load`, Continue, Ironwail's autoload) makes its campaign the active one and mounts its package first
// (host_cmd.c's Host_Loadgame_f: VR_SavePackageReady, VR_CanLoadCampaignSave, VR_AddonForSave, VR_CanLoadSaveMap).
//
// Continue (the main menu's first row, the hub's Continue button, vr_continue): the newest save of all, by its file's
// time, that is loadable here (its `// qvr_save` line, its campaign keys: VR_SaveCampaign; its map package installed)
// and of a campaign's map (not a vr* map's).

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_mapindex.hpp"
#include "vr_mapinstall.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace
{

// What a save was played as (its context lines, else what an older save has), and its titles.
struct SaveContext
{
    char map[MAX_QPATH]{};
    int campaign{-1};          // -1: the save doesn't say
    bool packageKnown{false};  // the save says which map package (none included)
    char package[72]{};        // its sha256 ("": none)
    char packageTitle[64]{};
    char mapTitle[64]{};       // the level's title ("The Gateway"); the map's name when it has none
};

// What Continue offers (the main thread: the menus, the hub's spawn, vr_continue).
struct ContinueSave
{
    bool found{false};
    char relname[MAX_QPATH]{};  // as load takes it: "s0", "autosave/e1m2", "autosave/mg1~start"
    char path[MAX_OSPATH]{};    // com_gamedir's
    char map[MAX_QPATH]{};
    char title[64]{};           // the map's title ("the Installation")
    char context[64]{};         // its campaign's title, or its map package's
    int campaign{0};
    time_t when{0};             // the file's time
    bool package{false};        // a map package's save: its title is the index's (read again when that arrives)
    bool hasPlayer{false};      // its `// qvr_player` line (saves since the rotating autosaves): the three below
    int health{0};
    int armor{0};
    float played{0.f};          // seconds into the level
};
ContinueSave continueSave;

// The Load Autosave menu's list (menu.c M_Autosaves, the death menu; vr_autosave_list): the loadable autosaves, newest
// first (the main thread: read again at each opening).
constexpr int maxAutosaves = 24;
ContinueSave autosaves[maxAutosaves];
int autosaveCount = 0;

// What the last refresh read (the main thread): the candidate's path and time, so that the same file isn't read again
// at every menu opening; and each file found by the directory walk.
struct ContinueScan
{
    char path[MAX_OSPATH]{};
    time_t when{0};
    za::U32 indexGeneration{0}; // the map index's then (a package's title, "map package <sha>" until it has arrived)
};
ContinueScan continueScanned;

// The saves Continue passed over because their map package is not installed, already said in the console (each once,
// not at every menu opening).
constexpr int maxWarned = 16;
ContinueScan warnedMissing[maxWarned];
int warnedCount = 0;

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

// A save's text: whole, or its first `limit` bytes (0: whole). `whole`: all of it was read.
bool readSave(const char* path, za::Vector<char>& text, long limit = 0, bool* whole = nullptr)
{
    FILE* f = Sys_fopen(path, "rb");
    if(!f) { return false; }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(size <= 0 || size > 64L * 1024 * 1024) { fclose(f); return false; }
    const long want = limit > 0 && limit < size ? limit : size;
    text.resize(static_cast<za::SizeT>(want) + 1);
    const size_t got = fread(text.data(), 1, static_cast<size_t>(want), f);
    fclose(f);
    text[got] = 0;
    if(whole) { *whole = want == size; }
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

// A context line's value ("// <key> <value>": "" when nothing follows the key); false when the save has no such line.
bool contextLine(const char* text, const char* key, char* out, size_t size)
{
    char needle[64];
    q_snprintf(needle, sizeof(needle), "\n// %s", key);
    const size_t n = strlen(needle);
    for(const char* p = strstr(text, needle); p; p = strstr(p + 1, needle))
    {
        const char* v = p + n;
        if(*v != ' ' && *v != '\r' && *v != '\n' && *v != 0) { continue; } // (another key that starts the same)
        if(*v == ' ') { ++v; }
        const size_t len = strcspn(v, "\r\n");
        q_strlcpy(out, v, za::min(size, len + 1));
        for(size_t e = strlen(out); e > 0 && out[e - 1] == ' '; --e) { out[e - 1] = 0; }
        return true;
    }
    out[0] = 0;
    return false;
}

// A map package's title as the map index has it now ("" when the index doesn't know it).
void packageTitleNow(const char* sha, char* out, size_t size)
{
    out[0] = 0;
    if(!sha[0]) { return; }
    const qvr::mapindex::Entry* e = qvr::mapindex::find(za::String{sha});
    if(e) { q_strlcpy(out, qvr::mapindex::index().field(e->title), size); }
}

// What a save says it was played as, from its text (and its path: the <save>.addon of a save older than the package
// line). A save older than the campaign line needs its globals in `text` for its campaign.
void readContext(const char* text, const char* path, SaveContext& ctx)
{
    ctx = SaveContext{};
    saveLine(text, 19, ctx.map, sizeof(ctx.map)); // (after the version, the comment, 16 spawn parms and the skill)
    char line[128];
    if(contextLine(text, "qvr_campaign", line, sizeof(line))) { ctx.campaign = Q_atoi(line); }
    else if(const char* g = strstr(text, "\"vr_save_campaign\""))
    {
        g += strlen("\"vr_save_campaign\"");
        while(*g == ' ' || *g == '\t' || *g == '"') { ++g; }
        if(*g >= '0' && *g <= '9') { ctx.campaign = Q_atoi(g); }
    }
    if(contextLine(text, "qvr_package", ctx.package, sizeof(ctx.package))) { ctx.packageKnown = true; }
    else if(path)
    {
        za::String sha;
        if(qvr::files::readText((za::String{path} + ".addon").cStr(), sha))
        {
            while(sha.size() && (sha.back() == '\n' || sha.back() == '\r' || sha.back() == ' ')) { sha.popBack(); }
            q_strlcpy(ctx.package, sha.cStr(), sizeof(ctx.package));
            ctx.packageKnown = true;
        }
    }
    if(ctx.package[0])
    {
        packageTitleNow(ctx.package, ctx.packageTitle, sizeof(ctx.packageTitle));
        if(!ctx.packageTitle[0]) { contextLine(text, "qvr_package_title", ctx.packageTitle, sizeof(ctx.packageTitle)); }
        if(!ctx.packageTitle[0]) { q_snprintf(ctx.packageTitle, sizeof(ctx.packageTitle), "map package %.8s", ctx.package); }
    }
    if(!contextLine(text, "qvr_map_title", ctx.mapTitle, sizeof(ctx.mapTitle)) || !ctx.mapTitle[0])
    {
        // The comment's first 22 characters ('_' for ' ': Host_SavegameComment), the level's title cut short.
        saveLine(text, 1, line, sizeof(line));
        line[za::min<size_t>(22, sizeof(line) - 1)] = 0;
        for(char* p = line; *p; ++p) { if(*p == '_') { *p = ' '; } }
        for(size_t n = strlen(line); n > 0 && line[n - 1] == ' '; --n) { line[n - 1] = 0; }
        q_strlcpy(ctx.mapTitle, line[0] ? line : ctx.map, sizeof(ctx.mapTitle));
    }
}

// What a save was played as, for the player: its map package's title, else its campaign's.
const char* contextTitle(const SaveContext& ctx)
{
    if(ctx.package[0]) { return ctx.packageTitle; }
    return VR_CampaignTitle(ctx.campaign);
}

// A map's name as part of a file name: letters, digits, '_', '-' and '.' kept, anything else '_'.
void safeName(const char* in, char* out, size_t size)
{
    size_t i = 0;
    for(; in[i] && i + 1 < size; ++i)
    {
        const char c = in[i];
        const bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                          c == '-' || c == '.';
        out[i] = keep ? c : '_';
    }
    out[i] = 0;
}

// The autosave of `map` played in `campaign`, from the map package `sha` ("": none).
void autosaveName(const char* map, int campaign, const char* sha, char* out, size_t size)
{
    char safe[MAX_QPATH];
    safeName(map, safe, sizeof(safe));
    if(sha[0]) { q_snprintf(out, size, "autosave/%.16s~%s", sha, safe); }
    else if(campaign <= 0) { q_snprintf(out, size, "autosave/%s", map); } // (Ironwail's own name, unchanged)
    else { q_snprintf(out, size, "autosave/%s~%s", VR_CampaignFolder(campaign), safe); }
}

// contgame's autosaves (1.0.1 before this): autosave/<campaign folder>/<map>.sav moved to autosave/<folder>~<map>.sav,
// with the <save>.addon beside it. Once, at start-up; the emptied folder removed.
void migrateFolderAutosaves()
{
    for(int c = 1; c < VR_CampaignCount(); ++c)
    {
        const char* folder = VR_CampaignFolder(c);
        const za::String dir = za::String{com_gamedir} + "/autosave/" + folder;
        if(!qvr::files::isDirectory(dir.cStr())) { continue; }
        za::Vector<za::String> names;
        for(findfile_t* find = Sys_FindFirst(dir.cStr(), "sav"); find; find = Sys_FindNext(find))
        {
            if(!(find->attribs & FA_DIRECTORY)) { names.pushBack(za::String{find->name}); }
        }
        for(const za::String& name : names)
        {
            const za::String from = dir + "/" + name;
            const za::String to = za::String{com_gamedir} + "/autosave/" + folder + "~" + name;
            if(qvr::files::exists(to.cStr()))
            {
                Con_Printf("Saves: autosave/%s/%s left where it is: autosave/%s~%s is there already\n", folder,
                    name.cStr(), folder, name.cStr());
                continue;
            }
            if(!qvr::files::rename(from.cStr(), to.cStr()))
            {
                Con_Printf("Saves: autosave/%s/%s could not be moved to autosave/%s~%s\n", folder, name.cStr(), folder,
                    name.cStr());
                continue;
            }
            Con_Printf("Saves: autosave/%s/%s moved to autosave/%s~%s (one save folder)\n", folder, name.cStr(), folder,
                name.cStr());
            const za::String addon = from + ".addon";
            if(qvr::files::exists(addon.cStr())) { (void)qvr::files::rename(addon.cStr(), (to + ".addon").cStr()); }
        }
        qvr::files::remove(dir.cStr()); // (only when it is empty now)
    }
}

// Whether a save would load here, and what it is: its map, its title, its campaign, its map package.
bool inspect(const Candidate& c, ContinueSave& out, bool verbose)
{
    za::Vector<char> text; // (a command's working buffer: once per menu opening at most)
    if(!readSave(c.path, text)) { return false; }
    char line[128];
    saveLine(text.data(), 0, line, sizeof(line));
    if(Q_atoi(line) != SAVEGAME_VERSION) { return false; }
    const char* qvr = strstr(text.data(), "\n// qvr_save ");
    int format = 0;
    if(!qvr || sscanf(qvr + 1, "// qvr_save %d", &format) != 1 || format > VR_SAVE_FORMAT) { return false; }
    SaveContext ctx;
    readContext(text.data(), c.path, ctx);
    if(!ctx.map[0] || !VR_AutosaveAllowed(ctx.map)) { return false; }
    out.campaign = VR_SaveCampaign(text.data());
    if(out.campaign < 0) { return false; }
    if(ctx.package[0] && !qvr::mapinstall::installed(za::String{ctx.package}))
    {
        bool warned = false;
        for(int i = 0; i < warnedCount; ++i)
        { warned = warned || (!strcmp(warnedMissing[i].path, c.path) && warnedMissing[i].when == c.when); }
        if(verbose || !warned)
        {
            Con_Printf("Continue: %s passed over: it was made in the map package %s, which is not installed\n",
                c.relname, ctx.packageTitle);
        }
        if(!warned && warnedCount < maxWarned)
        {
            q_strlcpy(warnedMissing[warnedCount].path, c.path, sizeof(warnedMissing[warnedCount].path));
            warnedMissing[warnedCount++].when = c.when;
        }
        return false;
    }
    q_strlcpy(out.map, ctx.map, sizeof(out.map));
    q_strlcpy(out.title, ctx.mapTitle, sizeof(out.title));
    q_strlcpy(out.context, contextTitle(ctx), sizeof(out.context));
    out.package = ctx.package[0] != 0;
    char player[64];
    out.hasPlayer = contextLine(text.data(), "qvr_player", player, sizeof(player)) &&
                    sscanf(player, "%d %d %f", &out.health, &out.armor, &out.played) == 3;
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
        if(continueSave.found && !strcmp(c.path, continueScanned.path) && c.when == continueScanned.when &&
            (!continueSave.package || continueScanned.indexGeneration == qvr::mapindex::generation()))
        {
            if(verbose) { Con_Printf("continue: %s (as before)\n", c.relname); }
            return;
        }
        ContinueSave found;
        if(inspect(c, found, verbose))
        {
            continueSave = found;
            q_strlcpy(continueScanned.path, c.path, sizeof(continueScanned.path));
            continueScanned.when = c.when;
            continueScanned.indexGeneration = qvr::mapindex::generation();
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

// One save's line for vr_save_info.
void saveInfoLine(const char* relname)
{
    char path[MAX_OSPATH];
    q_snprintf(path, sizeof(path), "%s/%s", com_gamedir, relname);
    COM_AddExtension(path, ".sav", sizeof(path));
    za::Vector<char> text;
    if(!readSave(path, text))
    {
        Con_Printf("vr_save_info: %s not found\n", path);
        return;
    }
    SaveContext ctx;
    readContext(text.data(), path, ctx);
    char label[SAVEGAME_COMMENT_LENGTH + 1];
    VR_SaveMenuLabel(path, label, sizeof(label));
    Con_Printf("save %s: map %s, campaign %d (%s), package %s%s%s, map title \"%s\", menu \"%s\"\n", relname, ctx.map,
        ctx.campaign, VR_CampaignTitle(ctx.campaign), ctx.package[0] ? ctx.package : "none", ctx.package[0] ? " " : "",
        ctx.package[0] ? ctx.packageTitle : "", ctx.mapTitle, label);
}

// vr_save_info [save]: what a save says it was played as; without one, every save in the game folder and autosave/
// (Debug > Tests > Saves > List the Saves).
void saveInfo()
{
    if(Cmd_Argc() >= 2)
    {
        saveInfoLine(Cmd_Argv(1));
        return;
    }
    candidateCount = 0;
    addCandidates("");
    addCandidates("autosave");
    const int count = candidateCount; // (saveInfoLine doesn't walk the folders: the list stays)
    for(int i = 0; i < count; ++i) { saveInfoLine(candidates[i].relname); }
    Con_Printf("vr_save_info: %d saves in %s\n", count, com_gamedir);
}

// Each autosave file as last inspected (its path and time; whether it would load, and what it is): the list is read again
// at each opening of the menu, a file not changed since not read again.
struct InspectedAutosave
{
    char path[MAX_OSPATH]{};
    time_t when{0};
    bool loadable{false};
    ContinueSave save;
};
constexpr int maxInspected = 128;
InspectedAutosave inspected[maxInspected];
int inspectedNext = 0; // (the slot replaced next when none is free: round)

bool inspectAutosave(const Candidate& c, ContinueSave& out)
{
    for(InspectedAutosave& e : inspected)
    {
        if(e.when == c.when && !strcmp(e.path, c.path))
        {
            out = e.save;
            return e.loadable;
        }
    }
    InspectedAutosave& e = inspected[inspectedNext];
    inspectedNext = (inspectedNext + 1) % maxInspected;
    q_strlcpy(e.path, c.path, sizeof(e.path));
    e.when = c.when;
    e.save = ContinueSave{};
    e.loadable = inspect(c, e.save, false);
    out = e.save;
    return e.loadable;
}

// The loadable autosaves of autosave/ (not the relighting's own), newest first, at most maxAutosaves.
void refreshAutosaves()
{
    autosaveCount = 0;
    candidateCount = 0;
    addCandidates("autosave");
    for(int i = 0; i < candidateCount; ++i)
    {
        const Candidate& c = candidates[i];
        if(!strcmp(c.relname, "autosave/relight")) { continue; } // (vr_relight.cpp's batch save)
        ContinueSave found;
        if(!inspectAutosave(c, found)) { continue; }
        int at = autosaveCount;
        while(at > 0 && autosaves[at - 1].when < found.when) { --at; }
        if(at >= maxAutosaves) { continue; }
        for(int j = za::min(autosaveCount, maxAutosaves - 1); j > at; --j) { autosaves[j] = autosaves[j - 1]; }
        autosaves[at] = found;
        autosaveCount = za::min(autosaveCount + 1, maxAutosaves);
    }
}

// An autosave's two lines: "e1m3: The Necropolis" and "Health 65 Armor 50, 3:12 in, 5 min ago".
void describeAutosave(const ContinueSave& s, char* line1, size_t size1, char* line2, size_t size2)
{
    q_snprintf(line1, size1, "%s: %s", s.map, s.title);
    time_t now;
    time(&now);
    const double ago = difftime(now, s.when);
    char when[32];
    if(ago < 60.) { q_strlcpy(when, "just now", sizeof(when)); }
    else if(ago < 3600.) { q_snprintf(when, sizeof(when), "%d min ago", static_cast<int>(ago / 60.)); }
    else if(ago < 48. * 3600.) { q_snprintf(when, sizeof(when), "%d h ago", static_cast<int>(ago / 3600.)); }
    else { q_snprintf(when, sizeof(when), "%d days ago", static_cast<int>(ago / 86400.)); }
    if(!s.hasPlayer)
    {
        q_strlcpy(line2, when, size2);
        return;
    }
    char played[32];
    if(s.played < 10.f) { q_strlcpy(played, "level start", sizeof(played)); }
    else
    {
        const int secs = static_cast<int>(s.played);
        q_snprintf(played, sizeof(played), "%d:%02d in", secs / 60, secs % 60);
    }
    q_snprintf(line2, size2, "Health %d Armor %d, %s, %s", s.health, s.armor, played, when);
}

// vr_autosave_list: the Load Autosave menu's list, printed (Debug > Tests > Saves).
void autosaveList()
{
    refreshAutosaves();
    for(int i = 0; i < autosaveCount; ++i)
    {
        char line1[96], line2[96];
        describeAutosave(autosaves[i], line1, sizeof(line1), line2, sizeof(line2));
        Con_Printf("autosave %d: %s | %s | %s\n", i + 1, autosaves[i].relname, line1, line2);
    }
    Con_Printf("vr_autosave_list: %d autosaves\n", autosaveCount);
}

// vr_autosave_load [n]: the n-th newest autosave (1: the newest) loaded, as the Load Autosave menu's row n.
void autosaveLoad()
{
    refreshAutosaves();
    const int n = Cmd_Argc() >= 2 ? Q_atoi(Cmd_Argv(1)) : 1;
    if(n < 1 || n > autosaveCount)
    {
        Con_Printf("vr_autosave_load: no autosave %d (%d of them)\n", n, autosaveCount);
        return;
    }
    VR_AutosaveLoad(n - 1);
}

} // namespace

extern "C" void VR_AutosaveRotate(const char* path)
{
    const int slots = za::max(1, za::min(9, static_cast<int>(qvr::vr_autosave_slots.value)));
    char base[MAX_OSPATH];
    COM_StripExtension(path, base, sizeof(base));
    const auto slotPath = [&base](int k) -> za::String {
        char out[MAX_OSPATH];
        if(k == 1) { q_snprintf(out, sizeof(out), "%s.sav", base); }
        else { q_snprintf(out, sizeof(out), "%s@%d.sav", base, k); }
        return za::String{out};
    };
    // The oldest kept replaced, those past the slots too (vr_autosave_slots lowered); then each one a slot older (the
    // <save>.addon with it; the file's time kept: the newest is still Continue's).
    for(int k = 9; k >= za::max(2, slots); --k)
    {
        const za::String old = slotPath(k);
        if(qvr::files::exists(old.cStr())) { qvr::files::remove(old.cStr()); }
        if(qvr::files::exists((old + ".addon").cStr())) { qvr::files::remove((old + ".addon").cStr()); }
    }
    for(int k = slots - 1; k >= 1; --k)
    {
        const za::String from = slotPath(k);
        if(!qvr::files::exists(from.cStr())) { continue; }
        const za::String to = slotPath(k + 1);
        if(!qvr::files::rename(from.cStr(), to.cStr()))
        {
            Con_Printf("Autosave: %s could not be moved to %s\n", from.cStr(), to.cStr());
            continue;
        }
        if(qvr::files::exists((to + ".addon").cStr())) { qvr::files::remove((to + ".addon").cStr()); }
        if(qvr::files::exists((from + ".addon").cStr()))
        { (void)qvr::files::rename((from + ".addon").cStr(), (to + ".addon").cStr()); }
    }
}

extern "C" int VR_AutosaveAny(void)
{
    char dir[MAX_OSPATH];
    q_snprintf(dir, sizeof(dir), "%s/autosave", com_gamedir);
    int any = 0;
    for(findfile_t* find = Sys_FindFirst(dir, "sav"); find; find = Sys_FindNext(find))
    {
        if(!(find->attribs & FA_DIRECTORY) && q_strcasecmp(find->name, "relight.sav")) { any = 1; }
    }
    return any;
}

extern "C" int VR_AutosaveListRefresh(void)
{
    refreshAutosaves();
    return autosaveCount;
}

extern "C" void VR_AutosaveDescribe(int i, char* line1, size_t size1, char* line2, size_t size2)
{
    line1[0] = 0;
    line2[0] = 0;
    if(i < 0 || i >= autosaveCount) { return; }
    describeAutosave(autosaves[i], line1, size1, line2, size2);
}

extern "C" void VR_AutosaveLoad(int i)
{
    if(i < 0 || i >= autosaveCount) { return; }
    Con_Printf("Load Autosave: %s\n", autosaves[i].relname);
    Cbuf_InsertText(va("load \"%s\"\n", autosaves[i].relname)); // (next, before any command queued)
}

extern "C" void VR_AutosaveName(const char* map, char* out, size_t size)
{
    autosaveName(map, VR_ActiveCampaign(), qvr::mapinstall::active().cStr(), out, size);
}

extern "C" void VR_AutosaveNameForLaunch(const char* map, char* out, size_t size)
{
    // (the campaign the `map` command would run it in: its own campaign check, VR_CanLoadCampaignMap's)
    autosaveName(map, VR_CampaignForMap(map), qvr::mapinstall::packageFor(map).cStr(), out, size);
}

extern "C" int VR_AutosaveAllowed(const char* map)
{
    return q_strncasecmp(map, "vr", 2) != 0;
}

extern "C" void VR_SaveContext(char* out, size_t size)
{
    const za::String& sha = qvr::mapinstall::active();
    char packageTitle[64];
    packageTitleNow(sha.cStr(), packageTitle, sizeof(packageTitle));
    char mapTitle[64];
    q_strlcpy(mapTitle, cl.levelname[0] ? cl.levelname : sv.name, sizeof(mapTitle));
    for(char* p = mapTitle; *p; ++p) { if(*p == '\n' || *p == '\r') { *p = ' '; } }
    for(char* p = packageTitle; *p; ++p) { if(*p == '\n' || *p == '\r') { *p = ' '; } }
    // (the player's health, armor and time into the level: the Load Autosave menu's second line)
    const edict_t* player = svs.clients && svs.clients[0].edict ? svs.clients[0].edict : nullptr;
    q_snprintf(out, size,
        "// qvr_campaign %d\n// qvr_package%s%s\n// qvr_package_title%s%s\n// qvr_map_title %s\n// qvr_player %d %d %.1f\n",
        VR_ActiveCampaign(), sha.size() ? " " : "", sha.cStr(), packageTitle[0] ? " " : "", packageTitle, mapTitle,
        player ? static_cast<int>(player->v.health) : 0, player ? static_cast<int>(player->v.armorvalue) : 0,
        static_cast<double>(sv.qcvm.time));
}

extern "C" int VR_SavePackage(const char* text, const char* savepath, char* sha, size_t size)
{
    SaveContext ctx;
    readContext(text, savepath, ctx);
    q_strlcpy(sha, ctx.package, size);
    return ctx.packageKnown ? 1 : 0;
}

extern "C" int VR_SavePackageReady(const char* text, const char* savepath)
{
    SaveContext ctx;
    readContext(text, savepath, ctx);
    if(!ctx.package[0]) { return 1; }
    if(qvr::mapinstall::installed(za::String{ctx.package}) &&
        qvr::files::isDirectory(qvr::mapinstall::packageFolder(za::String{ctx.package}).cStr()))
    { return 1; }
    Con_Printf("Saves: this save was made in the map package %s (%.16s), which is not installed: not loaded. Install it "
               "again from Single Player > Map Library to load it.\n", ctx.packageTitle, ctx.package);
    SCR_CenterPrint(va("This save needs the map package\n%s\nwhich is not installed: not loaded", ctx.packageTitle));
    return 0;
}

extern "C" int VR_SaveMenuLabel(const char* path, char* out, size_t size)
{
    za::Vector<char> text;
    bool whole = false;
    if(!readSave(path, text, 64L * 1024, &whole)) { return 0; }
    // (a save older than the context lines: its campaign is in its globals, after the edicts' start; read it all)
    if(!whole && !strstr(text.data(), "\n// qvr_campaign") && !readSave(path, text)) { return 0; }
    SaveContext ctx;
    readContext(text.data(), path, ctx);
    const char* context = contextTitle(ctx);
    // (a map package named as its one map: "Base To Hell" once)
    if(context[0] && q_strcasecmp(context, ctx.mapTitle)) { q_snprintf(out, size, "%s - %s", context, ctx.mapTitle); }
    else { q_strlcpy(out, ctx.mapTitle, size); }
    return 1;
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
    q_snprintf(detail, detailsize, "%s, %s", continueSave.context, ago);
}

extern "C" void VR_ContinueStart()
{
    if(!continueSave.found) { return; }
    Con_Printf("Continue: %s\n", continueSave.relname);
    // Next, before any command queued (inserted: the load, then before it a multiplayer server's settings put back, as
    // the game's start does: a save loads as a single-player game). The load makes the save's campaign the active one
    // and mounts its map package (Host_Loadgame_f).
    Cbuf_InsertText(va("load \"%s\"\n", continueSave.relname));
    if(svs.maxclients != 1 || coop.value != 0.f || deathmatch.value != 0.f)
    { Cbuf_InsertText("disconnect\nmaxplayers 1\ndeathmatch 0\ncoop 0\n"); }
}

extern "C" void VR_RegisterContinue()
{
    Cmd_AddCommand("vr_continue", continueCommand);
    Cmd_AddCommand("vr_continue_info", continueInfo);
    Cmd_AddCommand("vr_save_info", saveInfo);
    Cmd_AddCommand("vr_autosave_list", autosaveList);
    Cmd_AddCommand("vr_autosave_load", autosaveLoad);
    migrateFolderAutosaves();
}
