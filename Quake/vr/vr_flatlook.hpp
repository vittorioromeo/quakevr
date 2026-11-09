// vr_flatlook.hpp -- flatscreen play's look (ROUND24.md, "Flat-screen look"): the window's 3D view, once drawn and
// before the HUD, gets a glow (vr_flat_bloom, the eyes' bloom chain), a colour grade (vr_flat_grade_*: exposure, an S
// curve, shadows crushed or lifted, warm highlights and cool shadows, saturation) and a vignette (vr_flat_vignette), in
// one full-screen pass over a copy of the view. The eyes never run it: in a headset nothing changes.

#pragma once

namespace qvr::flatlook
{

// Whether the pass runs this frame: flatscreen play, and any of its parts on.
[[nodiscard]] bool active();

// The passes run so far (vr_flatlook_info; the tests check the eyes add none).
[[nodiscard]] long long passes();

void init();

} // namespace qvr::flatlook
