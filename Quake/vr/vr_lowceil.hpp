// vr_lowceil.hpp -- standing up under a low ceiling, the headset player's side (vr_lowceil; ROUND24.md, "Standing up
// under a low ceiling"; his notes vrteleporters_2026-11-10_01-54-06 and _01-54-40). Lying in a vent or crouched in a low
// tunnel, he gets up for real where there is no room: the server keeps his box as it is and holds him where he is
// (vr_hull.cpp updateCrouch's hold: no stick, crawl, room-scale step or teleport), and here, while it holds, the view
// follows his head but darkens and blurs while his eyes are past the ceiling seen from his box (vr_lowceil_dark,
// vr_lowceil_blur, over vr_lowceil_fade seconds; the comfort fade's black, the underwater blur), and a message in front
// of his eyes says to lie or crouch back down (vr_lowceil_message). Back down, it all clears. The local server's word
// (hull::localHold): a client of another server sees nothing of it (he is still held there).
//
// Flat-screen play is never held: its view is held under the box's top already (hull::localEyeCap), the same box code.

#pragma once

namespace qvr::hands
{
struct State;
}

namespace qvr::lowceil
{

// Once a frame in the view's setup, before comfortfade::frame(): the darkness eased towards the hold's, given to the
// comfort fade, and the message queued in front of the head.
void frame(const hands::State& s);

// The eyes' post-processing (water::viewWobble): the blur's offset now (0: none).
[[nodiscard]] float blur();

// A new map, a disconnect: nothing left of it.
void clear();

// vr_lowceil_status: the hold as the server and this client see it (a test's line).
void registerCommands();

} // namespace qvr::lowceil
