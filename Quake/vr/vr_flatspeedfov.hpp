#pragma once

// vr_flatspeedfov.hpp -- flatscreen play's speed FOV (docs/vr-port/FLATPLAY.md, "The speed FOV"; his words 2026-11-09:
// "any speed above the run speed would increase the FOV gradually, up to a cap. This would also mean that bunny hopping
// benefits"). The view widens with the speed along the floor: not at all up to vr_flat_speed_fov_from (the run's 320),
// smoothly (smoothstep) up to vr_flat_speed_fov_max times the field of view at vr_flat_speed_fov_top and over; eased in
// over vr_flat_speed_fov_ease seconds, out over vr_flat_speed_fov_ease_out. A slide and a bunny hop chain both get it
// from their speed. The ironsight wins: the widening goes as the ironsight's travel comes (none while sighted).

namespace qvr::flatspeedfov
{

void init(); // vr_flat_speed_fov_test

// Once a frame from flatplay::frame (outside the fly camera): the eased widening towards the speed's. `on`: the mode is
// on; off, the widening is gone at once.
void frame(bool on);

// `fov` (the ironsight's, flatsight::fov) widened by the eased multiplier, the widening scaled by 1 - `adsShare` (the
// ironsight's travel, 0..1). Exactly `fov` when nothing widens.
[[nodiscard]] float widen(float fov, float adsShare);

// The eased multiplier now (1: none), and the speed's own target for it.
[[nodiscard]] float multiplier();

// `basefov` without widen's widening when it is widen's last answer (in_sdl.c IN_FovScale, through VR_FlatLookFov).
[[nodiscard]] float lookFov(float basefov);

} // namespace qvr::flatspeedfov
