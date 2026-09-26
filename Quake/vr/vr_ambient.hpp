// vr_ambient.hpp -- directional ambient light on models: an "ambient cube" (Half-Life 2 / Source).
//
// A model is lit by one brightness, the lightmap under it (R_LightPoint), so a monster beside a lit
// wall is as bright on both sides. Here rays from the model's middle (the hand, for held weapons)
// read the baked light of the surfaces around it (their lightmaps at the points hit, every light
// style, coloured; the sky and lava as bright), folded by cosine lobes into six colours, +X -X +Y -Y
// +Z -Z. They are divided by their average (so the model is as bright as before on the whole) and
// sent with the instance; the alias shader shades the (bumped) normal with them, n^2 weighted.
// Cached per entity: traced again when it moved or a moment passed, a few entities a frame, and
// faded from the last trace so that nothing pops; what the rays hit is kept and read again at the
// current light styles, so flickering lights flicker on the model as on the walls. docs/vr-port/ROUND17.md.

#pragma once

#include "vr_engine.hpp"

namespace qvr::ambient
{

// The six faces (+X -X +Y -Y +Z -Z) for the alias instance: xyz each face's light over the model's
// own (1 on average), [0].w how much the cube applies (0: off), [1].w how much of the model's
// directional shading (vr_modellight's) to keep.
void entityCube(const entity_t* e, const float modelMatrix[16], const void* aliashdr, bool enabled, float out[6][4]);

} // namespace qvr::ambient
