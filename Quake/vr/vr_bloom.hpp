// vr_bloom.hpp -- glow around bright things (vr_bloom): lamps, fullbright buttons and panels,
// muzzle flashes and explosions. Each eye's scene is reduced to a quarter, keeping what is brighter
// than vr_bloom_threshold (white and coloured light weighted apart), then halved twice more
// (dual-filter taps) and brought back up to the quarter (3 x 3 tent taps), each level adding its own
// glow on the way: a tight halo from the quarter, a wide soft glow from the smallest. How much of
// the view glows (one texel, from the smallest level) weakens it (vr_bloom_adapt). The result is
// added by the eye's post-processing (GL_PostProcess, VR_PostProcessBloom), so there is no pass at
// the scene's full size: six small passes an eye, all at a sixteenth of the pixels or less, the result
// added by one bilinear tap (ROUND22.md, "Cheaper bloom": once four levels and four taps).

#pragma once

#include "vr_engine.hpp"
#include "vr_stereo.hpp"

#include <glm/glm.hpp>

namespace qvr::bloom
{

// Makes the glow of the scene `sceneTex` for this eye's post-processing: of its texture (`size`'s allocated size) the
// drawn part (stereo::viewSize: smaller under dynamic resolution, the chain then drawing a part of its own targets).
void apply(GLuint sceneTex, const stereo::ViewSize& size);

// Flatscreen play's window (vr_flatlook.cpp): the glow of `sceneTex` at `strength` (vr_bloom's other settings), in its
// own chain; its texture (a quarter of the scene's size, the strength in it), 0 for none. The eyes' result is untouched.
[[nodiscard]] GLuint applyFlat(GLuint sceneTex, int width, int height, float strength);

// This eye's glow (a quarter of the scene's size), if there is one; `view`: its drawn part's width and height, its
// texture's (the post-process's BloomView, the mirror's).
[[nodiscard]] bool result(unsigned& texture, glm::vec4& view);

// Frees the GL objects (when the GL context goes away).
void shutdown();

// VR stopped (stereo::releaseTargets): the eyes' and the spectator camera's chains freed (made again at their next use).
void releaseTargets();

} // namespace qvr::bloom
