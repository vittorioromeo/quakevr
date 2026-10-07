#pragma once
// vr_stealth.hpp -- the monsters' senses' engine side (QC vr_stealth.qc; docs/vr-port/STEALTH_PLAN.md): the light at a
// point and the host's flashlight beam, as QC builtins.

namespace qvr::stealth
{

// float(vector at) stealthlight: the light at `at` as Quake's lightmaps give it (R_LightPoint: 128 is full light) plus
// the dynamic lights there (Quake's falloff), the host's flashlight's own lights left out; -1 with no client world.
void PF_stealthlight();

// vector(entity player, float what) flashlightbeam: the player's flashlight beam (only the host's lamp is known to the
// server): what 0 its lens, 1 its axis, 2 '<on> <range> <cosine of its cone's half angle>' ('0 0 0' while off).
void PF_flashlightbeam();

// float(vector a, vector b) pvsvisible: whether b is in a's potentially visible set (the world's leaves): a noise's way
// round a corner or through a doorway, as against through solid walls.
void PF_pvsvisible();

} // namespace qvr::stealth
