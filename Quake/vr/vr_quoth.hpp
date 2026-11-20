#pragma once

// vr_quoth.hpp -- Quoth support, the engine's side (docs/vr-port/QUOTH.md): Quoth 2.2's release checked (its pak2.pak,
// the pak with its progs), its folder supplying only its own files. Quoth has no source and its licence forbids
// modifying or bundling it: the player installs Quaddicted's zip, unmodified; Quake VR never runs its progs. Its maps
// (its own, as a campaign, and the Map Library's packages made for it) play by Arcane Dimensions' rules
// (vr_rules.cpp), whose code re-implements nearly every Quoth classname; the rest is QC's vr_quoth.qc.

namespace qvr::quoth
{

// The campaign table's index of Quoth (vr_gamedir.cpp campaigns[]).
inline constexpr int campaignIndex = 8;

// 1 when <folder> (a Quoth installation inspectPack accepted) is 2.2 (its pak2.pak's size and SHA-256); 3 when it is
// another version (said once in the console).
[[nodiscard]] int versionStatus(const char* folder);

// True when Quoth's folder must not supply `filename` (vr_gamedir.cpp VR_SkipSearchPath): a file an official pak or
// Arcane Dimensions has too (id1's always; Hipnotic's, Rogue's, AD's while installed: our VR models and AD's stay;
// Quoth's knight, ogre, grunt... meshes never replace id's), its progs, quake.rc and configs.
[[nodiscard]] bool skipInFolder(const char* filename);

// In another campaign (a Map Library package made for Quoth, any map naming Quoth's files), Quoth's folder supplies
// only its models, sprites, sounds and skies (after skipInFolder): never its maps, textures, menu art.
[[nodiscard]] bool skipOutside(const char* filename);

// Quoth's folder is on the search path (its own campaign, or its content in another: vr_gamedir.cpp).
[[nodiscard]] bool folderMounted();

} // namespace qvr::quoth
