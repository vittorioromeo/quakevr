// vr_bloom.hpp -- glow around bright things (vr_bloom): lamps, fullbright buttons and panels,
// muzzle flashes and explosions. Each eye's scene is reduced to a quarter, what is brighter than
// vr_bloom_threshold kept (with a soft knee), blurred twice (a Gaussian, the second pass twice as
// wide, vr_bloom_radius), and added back onto the scene before post-processing. Costs a few
// fullscreen passes at a sixteenth of the pixels.

#pragma once

#include "vr_engine.hpp"

namespace qvr::bloom
{

// Adds the glow onto the scene in `sceneFbo` (its colour texture `sceneTex`, `width` x `height`).
void apply(GLuint sceneFbo, GLuint sceneTex, int width, int height);

// Frees the GL objects (when the GL context goes away).
void shutdown();

} // namespace qvr::bloom
