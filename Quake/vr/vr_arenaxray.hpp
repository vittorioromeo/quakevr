#pragma once

// vr_arenaxray.hpp -- a Survival Arena wave's last foes, seen through walls (the author's ask, 2026-11-01: "an option to
// see the enemies glow through walls ... when they are far enough from the player ... if there's the last or last two
// enemies"). QC marks them (vr_arena.qc VR_ARENA_XrayMark: .vr_xray, once no more than vr_arena_xray_left of the wave
// are left, up and still to come); this sends the marks to the clients (QVR_SVC_XRAY, reliable, on a change), sends a
// marked foe wherever it stands (past the PVS: VR_SV_SendPastPVS), and draws each marked foe farther from the eyes than
// vr_arena_xray_distance a second time, after the scene, as its rim glow only, added on where something stands in front
// of it (r_alias.c R_DrawAliasModels_Xray: the alias shader's coloured rim, the depth test turned round). Off
// (vr_arena_xray 0) nothing is marked or drawn. vr_arena_xray_shown reads back how many the last view outlined.

#include "vr_engine.hpp"

namespace qvr::arenaxray
{

// VR_ServerFrameEnd: each entity's mark sent to the clients where it changed.
void serverFrame();
// VR_WriteClientSpawnState: every mark (after a clear), for a client coming in.
void serverWriteAll(struct sizebuf_s* msg);
// QVR_SVC_XRAY: a mark set or cleared (-1: all cleared).
void parseEvent();

} // namespace qvr::arenaxray
