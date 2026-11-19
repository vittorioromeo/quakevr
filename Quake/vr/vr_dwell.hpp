#pragma once

// vr_dwell.hpp -- Dwell support, the engine's side (docs/vr-port/DWELL.md, sections 3, 5 and P1b): Dwell's release
// checked (v2.2, by its progs.dat), its folder supplying only its own files. Its maps play by Copper's rules
// (vr_rules.cpp: the campaign's ruleset); the campaign row is vr_gamedir.cpp's, like the others.

namespace qvr::dwell
{

// The campaign table's index of Dwell (vr_gamedir.cpp campaigns[]).
inline constexpr int campaignIndex = 7;

// 1 when <folder> (a Dwell installation inspectPack accepted) is v2.2 (its progs.dat's size and SHA-256); 3 when it
// is another version (said once in the console).
[[nodiscard]] int versionStatus(const char* folder);

// True when Dwell's folder must not supply `filename` (vr_gamedir.cpp VR_SkipSearchPath): a file an official pak has
// too (id1's always; Hipnotic's and Rogue's while that pack is installed: our VR models, heads and sounds stay
// Quake's), Dwell's progs, its quake.rc and default.cfg.
[[nodiscard]] bool skipInFolder(const char* filename);

} // namespace qvr::dwell
