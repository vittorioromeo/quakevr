#pragma once

// vr_humlook.hpp -- the armed humanoids' recolours by the weapon they spawned with (the author's ask, 2026-11-21: "run-time
// conditional hue/tint recolors on their textures, and also hue/tint recolors on the backpacks"; ARMED_MONSTERS.md,
// "Recolours by weapon"). QC gives each armed humanoid its look as it is armed (vr_humrepl.qc VR_HUM_Look: .vr_hum_look,
// its row times 16 plus its weapon's category; its worn pack the same plus partPack); this sends each entity's look to
// the clients (QVR_SVC_HUMLOOK, reliable, on a change) and fills the alias instance's Recolor rows from the table
// (quakevr/humlook.txt, read at load: one row per row, category and part): the shader's QVR_ALIAS_FS_RECOLOR turns the
// skin's pixels in the row's colour range (hue, saturation, lightness, soft-edged) by its HSL change and tint, over
// whatever texture the model draws (Quake's skin, a replacement, a retro one): the mod's files are never changed. Off
// (vr_humlook 0) nothing is recoloured. Phase 2: a per-skin mask of our own, the Texture Studio editing the table.

#include "vr_engine.hpp"

struct entity_s;

namespace qvr::humlook
{

inline constexpr int maxKinds = 8;       // armed monster rows (QC VR_AM_*: grunt, enforcer, knight, death knight)
inline constexpr int maxCategories = 16; // weapon categories (QC VR_HUM_CAT_*; 0 none)
inline constexpr int partPack = 4096;    // a look's part bit: its worn backpack (else its body)

// VR_ServerFrameEnd: each entity's look sent to the clients where it changed.
void serverFrame();
// VR_WriteClientSpawnState: every look (after a clear), for a client coming in.
void serverWriteAll(struct sizebuf_s* msg);
// QVR_SVC_HUMLOOK: an entity's look set or cleared (-1: all cleared).
void parseEvent();
// The console commands (vr_humlook_reload, vr_humlook_info).
void registerCommands();
// VR_AliasInstance: `e`'s three Recolor rows (all zero: none).
void instanceRows(const struct entity_s* e, float rows[3][4]);

} // namespace qvr::humlook
