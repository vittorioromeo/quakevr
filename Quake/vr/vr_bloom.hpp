// vr_bloom.hpp -- glow around bright things (vr_bloom): lamps, fullbright buttons and panels,
// muzzle flashes and explosions. Each eye's scene is reduced to a quarter, keeping what is brighter
// than vr_bloom_threshold (white and coloured light weighted apart), then halved three more times
// (dual-filter taps) and brought back up to the quarter (3 x 3 tent taps), each level adding its own
// glow on the way: a tight halo from the quarter, a wide soft glow from the smallest. How much of
// the view glows (one texel, from the smallest level) weakens it (vr_bloom_adapt). The result is
// added by the eye's post-processing (GL_PostProcess, VR_PostProcessBloom), so there is no pass at
// the scene's full size: eight small passes an eye, all at a sixteenth of the pixels or less.

#pragma once

#include "vr_engine.hpp"

namespace qvr::bloom
{

// Makes the glow of the scene `sceneTex` (`width` x `height`) for this eye's post-processing.
void apply(GLuint sceneTex, int width, int height);

// This eye's glow (a quarter of the scene's size), if there is one.
[[nodiscard]] bool result(unsigned& texture);

// Frees the GL objects (when the GL context goes away).
void shutdown();

} // namespace qvr::bloom
