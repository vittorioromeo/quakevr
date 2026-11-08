// vr_skinedge.hpp -- skin edge fill (vr_skin_edge_fill). A model's triangles map their corners to texel middles
// (gl_mesh.c's + 0.5), so a triangle's slanted edge on the skin runs through texels whose middles lie outside it: a
// nearest gl_texturemode reads them along that edge, and where the skin's painted part steps back from the edge (its
// background there, usually black) they showed as thin black slivers on the model (the shotgun's receiver behind its
// rear sight; ROUND24.md, "Black slivers on the shotgun"). A linear mode blends them in, as Blender does. As a model
// loads, every texel its triangles touch only at an edge (no triangle has its middle) gets the texel next to it that
// the touching triangle has inside; its skins (8-bit, their fullbright masks, a full-colour replacement of the same
// size) upload with those texels filled. A see-through skin (MF_HOLEY) is left as it is (its edges cut by alpha).
//

#pragma once

// VR_SkinEdgesModel (gl_model.c) and VR_SkinEdgeFill (gl_texmgr.c) are declared in vr_api_render.h.

namespace qvr::skinedge
{

void init();   // vr_skin_edge_fill's callback (the filled skins uploaded again), the command
void list_f(); // vr_skin_edges: the models with edge texels, how many, the uploads filled

} // namespace qvr::skinedge
