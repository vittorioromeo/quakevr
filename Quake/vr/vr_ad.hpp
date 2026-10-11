#pragma once

// vr_ad.hpp -- Arcane Dimensions support, the engine's side (docs/vr-port/ARCANE_DIMENSIONS.md, sections 5 and 15):
// AD's release checked (the latest only), AD's folder supplying only its own files, the "AD rules" switch set before
// each map spawns, and which version of a shared classname spawns in a map played by AD's rules. Everything AD-specific
// in the engine is here; the engine's own files call it at a few places marked "AD support".

namespace qvr::ad
{

// The campaign table's index of Arcane Dimensions (vr_gamedir.cpp campaigns[]).
inline constexpr int campaignIndex = 6;

// 1 when <folder> (an AD installation that inspectPack accepted) is the latest release, AD 1.80 patch 1 ("1.81");
// 3 when it is another version (said once in the console, with what to install instead).
[[nodiscard]] int versionStatus(const char* folder);

// True when AD's folder must not supply `filename` (vr_gamedir.cpp VR_SkipSearchPath): a file an official pak has
// too (id1's always; Hipnotic's and Rogue's while that pack is installed), AD's progs and its own quake.rc.
[[nodiscard]] bool skipInFolder(const char* filename);

// The same while AD is not the active campaign (its folder mounted under the active one's, decision 16): only its
// models, sounds, HUD icons and brush models, never its maps, skies, textures, effect files or menu art.
[[nodiscard]] bool skipOutside(const char* filename);

// SV_SpawnServer, before the map's entities spawn (VR_OnSpawnServerBeforeLoad): vr_ad_rules for this map.
void beforeLoad();

// vr_ad_info: the data, the rules and the spawns of the map now running.
void registerCommands();

} // namespace qvr::ad
