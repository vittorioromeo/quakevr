#pragma once

// vr_brushes_internal.hpp -- what vr_brushes.cpp (see vr_brushes.hpp) shares with its diagnostics, vr_brushes_diag.cpp.
// Internal: included by those two alone. What was the file's anonymous namespace and both use is namespace
// brushes::detail: its types here, its functions declared here and defined in the file that owns them.

#include "vr_brushes.hpp"

#include "vr_box3d.hpp"
#include "vr_brushbake.hpp"
#include "vr_brushlight.hpp"
#include "vr_cvars.hpp"
#include "vr_decals.hpp"
#include "vr_files.hpp"
#include "vr_held.hpp"
#include "vr_ledges.hpp"
#include "vr_mem.hpp"
#include "vr_modelcollide.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/InPlaceVector.hpp"
#include "Zancle/Container/SmallVector.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::brushes::detail
{

struct Built
{
    za::Vector<mplane_t> planes;
    za::Vector<mclipnode_t> clipnodes[3];
    za::Vector<mnode_t> nodes;
    mleaf_t leafs[2]{}; // solid, empty
    za::Vector<mvertex_t> vertexes;
    za::Vector<medge_t> edges;
    za::Vector<int> surfedges;
    za::Vector<mtexinfo_t> texinfo;
    za::Vector<msurface_t> surfaces;
    za::Vector<int> surfaceFace; // each surface's face in the brush
    za::Vector<texture_t*> textures;
    za::Vector<int> used;
    za::Vector<glm::vec4> clipPlanes; // the faces (hull 0's planes, outward) and the bevels a box swept at it needs
    za::Vector<glm::dvec4> hullPlanes[3]; // each hull's planes (outward, model space: its chain's; the bake's submodels)
    glm::vec3 origin{0.f};      // the entity's (model space = world - origin)
    glm::vec3 mins{0.f}, maxs{0.f};
    int texofs[TEXTYPE_COUNT + 1]{};
};

void build(const Brush* b, Built& out);
void apply(qmodel_t* mod, Built* b, int sortId);
bool parseVec(int first, glm::dvec3& v);
[[nodiscard]] bool serverReady();

} // namespace qvr::brushes::detail
