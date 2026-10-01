// vr_timescale.hpp -- slow motion (bullet time) for recording: the whole simulation slowed by one time scale, the
// headset's view in real time. ROUND21.md, "Slow motion".
//
// vr_timescale (or the bindable vr_slowmo toggle) scales host_frametime (host.c, Host_AdvanceTime and the server's
// frames): the server (QC thinks, Quake's movement, Box3D), the client's time (lerping, particles, decals, effects) and
// vr_gametime, the slowed realtime VR's own client simulations step on. The headset's pose is never slowed (the camera
// follows the head at the full frame rate); the hands are (filterHands). Sounds play slower and lower with
// vr_timescale_sound (VR_SndRate: snd_mix.c, vr_audio.cpp). Single player only (a remote server keeps its own time).

#pragma once

namespace qvr
{
struct TrackingState;
}

namespace qvr::timescale
{

void init(); // the vr_slowmo command

// The time scale in effect (1 unless slow motion): vr_timescale's or bullet time's (vr_bullettime.cpp), eased by
// vr_timescale_ramp.
[[nodiscard]] float current();

// Whether the player runs in its own time in the slowed world (Sandevistan: vr_sandevistan, or bullet time's
// vr_bullettime_sandevistan): its moves, turns, hands, weapons' timing and (vr_sandevistan_missiles) its missiles at
// full speed; the hands not slowed; QC's melee on the player's clock (vr_player_time_offset).
[[nodiscard]] bool sandevistan();

// How many times faster than the world the smooth turn goes (1, or 1 over the scale with
// vr_timescale_turn_realtime or Sandevistan).
[[nodiscard]] float turnSpeedup();

// VR_BeginFrame, right after the tracking (and angvel::fix): in slow motion, the hands move with the slowed world.
// Each follows its controller, from the head (never slowed: walking, ducking and dodging carry the hands along), as the
// world's time allows, at most vr_timescale_hand_speed m/s and
// vr_timescale_hand_spin rad/s of the game's time (0: no limit), and their velocities are given in the game's time;
// the tracking's clock is slowed likewise (t.time). Nothing is changed at a time scale of 1 once the hands have caught
// up, nor while a menu is open (the hands are the controllers'), nor in Sandevistan. The head is never slowed.
void filterHands(TrackingState& t);

} // namespace qvr::timescale
