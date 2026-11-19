// vr_config.hpp -- the settings' registration and the config file (vr_cvars.cpp, vr_config_diag.cpp): what the module's
// start-up, the menus' close and the config's tools call. The settings by name: vr_cvars.hpp.

#pragma once

extern "C" {
#include "q_stdinc.h"
#include "cvar.h"
}

#include "Zancle/Container/Array.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/Span.hpp"

namespace qvr
{

void registerCvars();

// Two settings' values the same (as numbers when both are): what a sparse config's "changed from the default" means.
[[nodiscard]] bool sameSettingValue(const char* a, const char* b);
// vr_config_test: the sparse config's round trip (vr_config_diag.cpp).
void registerConfigDiag();

// The game folder's config written now if an archived setting changed since it was last read or written (not in a copy
// started with -noconfigwrite); configFrame: so, each time the menu closes. The config was written only when the game
// quit, and a game ended otherwise (stopped from the debugger, killed) lost the session's settings.
void saveConfigNow();
void configFrame();

// The config's tracking (vr_cvars.cpp ConfigTrack), for vr_config_test (vr_config_diag.cpp): saveConfigNow's count of
// the archived settings changed since the config was read or written; the same counted as it was until 2026-11-19 (a
// map of every archived setting made, then compared: the reference, and the cost it had); the tracking against the
// config file and the settings read afresh (as VR_ConfigWritten did after every write until then); the tracking read
// from the file again, as at the config's load.
[[nodiscard]] int configChangedCount();
[[nodiscard]] int configChangedCountByMap();
struct ConfigTrackCheck
{
    bool on = false;      // tracking the game folder's config
    int fileDiffers = 0;  // names whose tracked file value differs from the file read now (or only one side has)
    int hereDiffers = 0;  // and whose tracked value here differs from the setting's
    int sharedNames = 0;  // archived settings that share a name with another
    za::String firstFile; // (the first of each)
    za::String firstHere;
};
[[nodiscard]] ConfigTrackCheck configTrackCheck(const char* path);
void configTrackAgain(const char* path);

// The data tables' values (design review 3; no cvars: the stand-ins of quakevr/data/weapons.txt's, props.txt's,
// monsters.txt's and groups.txt's values, vr_slottable.hpp, vr_monsterdata.cpp, vr_mgroups.cpp), each table's in turn:
// the config's writer, its tracking and tests, Propose Defaults.
[[nodiscard]] za::Array<za::Span<cvar_t>, 4> standInTables();

} // namespace qvr
