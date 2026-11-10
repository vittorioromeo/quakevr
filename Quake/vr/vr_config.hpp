// vr_config.hpp -- the settings' registration and the config file (vr_cvars.cpp, vr_config_diag.cpp): what the module's
// start-up, the menus' close and the config's tools call. The settings by name: vr_cvars.hpp.

#pragma once

extern "C" {
#include "q_stdinc.h"
#include "cvar.h"
}

#include "Zancle/Container/Array.hpp"
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

// The data tables' values (design review 3; no cvars: the stand-ins of quakevr/data/weapons.txt's, props.txt's,
// monsters.txt's and groups.txt's values, vr_slottable.hpp, vr_monsterdata.cpp, vr_mgroups.cpp), each table's in turn:
// the config's writer, its tracking and tests, Propose Defaults.
[[nodiscard]] za::Array<za::Span<cvar_t>, 4> standInTables();

} // namespace qvr
