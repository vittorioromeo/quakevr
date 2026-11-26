// vr_lancecharge.hpp -- Dwell's Crystal Lance's charged shot as drawn and heard (ROUND25.md, "lancecharge"; his note
// 2026-11-23). The server (QC dw_items.qc DW_LanceChargeFrame) charges it while the trigger is held and fires it as it is
// let go fully charged; it tells every client of each charge begun, let go short or fired (QVR_SVC_LANCECHARGE). Here, at
// the player's Lance's crystal tip (its drawn muzzle: yours, or another player's copy's, mpavatar): violet sparks
// gathering in, denser as it charges; a violet light growing with the charge (pulsing once full); a hum rising in pitch
// and volume (vr/lance_charge.wav, looped: Misc/quakevr/make_lance_sounds.py). Let go short, they fade out over
// vr_dw_lance_charge_fade s; fired, they go out at once and its bolt (the beam) lingers vr_dw_lance_bolt s (beamLife).
// The same code for flat and VR play, yours and the other players'. vr_debug_gunfx 1 prints "lancecharge: ..." lines.

#pragma once

#include "vr_view.hpp"

namespace qvr::lancecharge
{

// QVR_SVC_LANCECHARGE.
void parse();

// Once a frame (vr_weaponfx.cpp frame), your hands' drawn weapons `own`: every charge drawn and heard.
void frame(const view::ViewEntity (&own)[2]);

// CL_ParseBeam: seconds a new beam keyed `ent` (an entity | (beam id + 1) << 16, vr_client.cpp VR_ParseBeamEntity) is
// drawn: vr_dw_lance_bolt for a Crystal Lance's (a player's hand holding one, a Lance prop's), Quake's 0.2 else.
[[nodiscard]] float beamLife(int ent);

// VR_UpdateBeam: the start of player `ent`'s bolt from `hand` (beam ids 0, 1) as drawn: his Crystal Lance's (yours, your
// hand's drawn weapon; another player's copy's, mpavatar), spear or gun, its middle crystal's tip, the charge's origin
// (his decision 2026-11-23), in flat and VR play; false: no Lance drawn there (the beam's start kept). `muzzle`: the start
// it replaces (vr_debug_gunfx's print). `ent` a prop (beyond the players): the Lance stuck in a body that your hand holds
// and fires (embedfire, QC vr_propfire.qc), its tip as drawn. The charge (frame) is drawn on that stuck Lance too.
[[nodiscard]] bool boltStart(int ent, int hand, const glm::vec3& muzzle, glm::vec3& out);

// Your Lance in `hand` shaking (lance3; his note vrtesthall_2026-11-23_23-25-08): a little as it charges (by the charge,
// vr_dw_lance_shake_charge), more as it fires (vr_dw_lance_shake_fire, dying away over vr_dw_lance_shake_time), in its
// shot's kicks as vr_weaponfx.cpp recoilOffset sums them (`kick` back and up, `sideways`); added to, never set.
void shake(int hand, float& kick, float& sideways);

// The hum's sound loaded (vr_main.cpp's map preparation).
void prepare();

// Everything forgotten (the client's state cleared).
void clear();

} // namespace qvr::lancecharge
