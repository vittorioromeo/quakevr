// CPU-only validation of the index packet used by the experimental Vulkan BSP path.
#include "vr_bsp_mesh.hpp"
#include "quakedef.h"
#include "gl_model.h"

#include <cstdio>
#include <cstdlib>

namespace
{
void require(bool condition, const char* message)
{
    if(!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
}

int main()
{
    using namespace qvr::bsp;
    const SurfaceInput surfaces[] = {
        {0, 4, 7, 12, true},   // convex quad -> two fan triangles
        {4, 3, 2, 13, false},  // sky/water/cutout omitted by the engine adapter
        {7, 5, 9, 14, true},   // convex pentagon -> three fan triangles
    };
    OpaqueWorldMesh mesh;
    require(buildOpaqueWorldMesh(surfaces, 3, 12, mesh), "valid opaque surfaces build");
    require(mesh.ranges.size() == 2, "one draw range remains for each opaque face");
    require(mesh.indices.size() == 15, "quad and pentagon become five triangles");
    require(mesh.maxVertexIndex == 11, "packet reports highest referenced vertex");
    require(mesh.indices[0] == 0 && mesh.indices[1] == 1 && mesh.indices[2] == 2 &&
        mesh.indices[3] == 0 && mesh.indices[4] == 2 && mesh.indices[5] == 3,
        "quad uses stable convex fan winding");
    require(mesh.ranges[0].firstIndex == 0 && mesh.ranges[0].indexCount == 6 &&
        mesh.ranges[0].material == 7 && mesh.ranges[0].surfaceIndex == 12,
        "range preserves first face's material and source surface");
    require(mesh.ranges[1].firstIndex == 6 && mesh.ranges[1].indexCount == 9 &&
        mesh.ranges[1].material == 9 && mesh.ranges[1].surfaceIndex == 14,
        "range preserves second face's material and source surface");

    const SurfaceInput outOfBounds[] = {{10, 3, 0, 0, true}};
    require(!buildOpaqueWorldMesh(outOfBounds, 1, 12, mesh), "out-of-bounds face is rejected");
    require(mesh.indices.size() == 15 && mesh.ranges.size() == 2,
        "failed build leaves prior output packet intact");
    const SurfaceInput degenerate[] = {{0, 2, 0, 0, true}};
    require(!buildOpaqueWorldMesh(degenerate, 1, 12, mesh), "degenerate face is rejected");
    const SurfaceInput unordered[] = {{7, 5, 9, 14, true}, {0, 4, 7, 12, true}};
    require(buildOpaqueWorldMesh(unordered, 2, 12, mesh) && mesh.maxVertexIndex == 11,
        "maximum vertex index is independent of surface ordering");
    require(buildOpaqueWorldMesh(nullptr, 0, 0, mesh) && mesh.indices.size() == 0 && mesh.ranges.size() == 0,
        "empty world is represented as an empty valid packet");
    require(!buildOpaqueWorldMesh(nullptr, 1, 0, mesh), "null surface input with nonzero count is rejected");

    mtexinfo_t texinfo[3]{};
    texinfo[0].texnum = 2;
    texinfo[1].texnum = 4;
    texinfo[2].texnum = 9;
    msurface_t modelSurfaces[3]{};
    modelSurfaces[0].vbo_firstvert = 0;
    modelSurfaces[0].numedges = 4;
    modelSurfaces[0].texinfo = &texinfo[0];
    modelSurfaces[1].vbo_firstvert = 4;
    modelSurfaces[1].numedges = 3;
    modelSurfaces[1].flags = SURF_DRAWTURB; // lightmapped liquid need not be DRAWTILED
    modelSurfaces[1].texinfo = &texinfo[1];
    modelSurfaces[2].vbo_firstvert = 7;
    modelSurfaces[2].numedges = 5;
    modelSurfaces[2].flags = SURF_DRAWFENCE;
    modelSurfaces[2].texinfo = &texinfo[2];
    qmodel_t world{};
    world.name[0] = 'm';
    world.numtextures = 10;
    world.numsurfaces = 3;
    world.nummodelsurfaces = 3;
    world.surfaces = modelSurfaces;
    require(buildOpaqueWorldMeshFromModel(&world, 12, mesh), "loaded qmodel surfaces adapt to packet");
    require(mesh.ranges.size() == 1 && mesh.indices.size() == 6 &&
        mesh.ranges[0].material == 2 && mesh.ranges[0].surfaceIndex == 0,
        "model adapter keeps opaque textured faces and skips lit liquid/fence surfaces");

    glvert_t sourceVertices[12]{};
    for(int i = 0; i < 12; ++i)
    {
        sourceVertices[i].pos[0] = static_cast<float>(i);
        sourceVertices[i].pos[1] = static_cast<float>(i + 20);
        sourceVertices[i].pos[2] = static_cast<float>(i + 40);
        sourceVertices[i].st[0] = static_cast<float>(i) / 12.f;
        sourceVertices[i].st[1] = static_cast<float>(11 - i) / 12.f;
    }
    OpaqueWorldUpload upload;
    require(buildOpaqueWorldUploadFromModel(&world, sourceVertices, 12, upload),
        "world adapter copies renderable attributes into an owned upload packet");
    require(upload.vertices.size() == 4 && upload.indices.size() == 6 && upload.ranges.size() == 1,
        "upload packet contains only the opaque world's vertices and indices");
    require(upload.indices[0] == 0 && upload.indices[1] == 1 && upload.indices[2] == 2 &&
        upload.indices[3] == 0 && upload.indices[4] == 2 && upload.indices[5] == 3,
        "upload indices address the compact packet, not the source CPU array");
    require(upload.vertices[2].position[0] == sourceVertices[2].pos[0] &&
        upload.vertices[2].position[1] == sourceVertices[2].pos[1] &&
        upload.vertices[2].uv[0] == sourceVertices[2].st[0] &&
        upload.vertices[2].uv[1] == sourceVertices[2].st[1],
        "upload packet preserves source position and base texture coordinates");
    require(upload.ranges[0].material == 2 && upload.ranges[0].surfaceIndex == 0,
        "upload draw range retains world material identity");
    qmodel_t inlineModel = world;
    inlineModel.name[0] = '*';
    require(!buildOpaqueWorldMeshFromModel(&inlineModel, 12, mesh),
        "inline brush models are rejected as static world geometry");
    qmodel_t shiftedWorld = world;
    shiftedWorld.firstmodelsurface = 1;
    require(!buildOpaqueWorldMeshFromModel(&shiftedWorld, 12, mesh),
        "nonzero firstmodelsurface is rejected for the world packet");
    qmodel_t truncatedWorld = world;
    truncatedWorld.nummodelsurfaces = 4;
    require(!buildOpaqueWorldMeshFromModel(&truncatedWorld, 12, mesh),
        "world surface range beyond the model is rejected");
    texinfo[0].texnum = 10;
    require(!buildOpaqueWorldMeshFromModel(&world, 12, mesh), "material outside model texture table is rejected");
    require(mesh.ranges.size() == 1 && mesh.ranges[0].material == 2,
        "failed model adapter leaves prior output packet intact");
    texinfo[0].texnum = 2;
    modelSurfaces[0].vbo_firstvert = 11;
    require(!buildOpaqueWorldUploadFromModel(&world, sourceVertices, 12, upload),
        "upload adapter rejects a source span that exceeds CPU vertex data");
    require(upload.vertices.size() == 4 && upload.indices.size() == 6 && upload.ranges[0].material == 2,
        "failed upload build leaves the previous packet intact");
    std::puts("PASS: BSP fan indices, opaque filtering, material ranges, bounds and transactional failure");
}
