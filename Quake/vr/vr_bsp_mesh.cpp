#include "vr_bsp_mesh.hpp"

#include "quakedef.h"
#include "gl_model.h"

#include <limits>

namespace qvr::bsp
{

namespace
{
bool modelSurfaces(const qmodel_t* world, za::Vector<SurfaceInput>& surfaces)
{
    if(!world || world->name[0] == '*' || world->firstmodelsurface != 0 || world->nummodelsurfaces < 0 ||
        world->firstmodelsurface > world->numsurfaces ||
        world->nummodelsurfaces > world->numsurfaces - world->firstmodelsurface ||
        (world->nummodelsurfaces > 0 && !world->surfaces))
    {
        return false;
    }

    for(int i = 0; i < world->nummodelsurfaces; ++i)
    {
        const int sourceIndex = world->firstmodelsurface + i;
        const msurface_t& source = world->surfaces[sourceIndex];
        if(source.vbo_firstvert < 0 || source.numedges < 0 || !source.texinfo ||
            source.texinfo->texnum < 0 || source.texinfo->texnum >= world->numtextures)
        {
            return false;
        }

        const bool opaque = !(source.flags & (SURF_DRAWTILED | SURF_DRAWTURB | SURF_DRAWFENCE |
            SURF_DRAWSPRITE | SURF_DRAWLAVA | SURF_DRAWSLIME | SURF_DRAWTELE | SURF_DRAWWATER));
        surfaces.pushBack({static_cast<za::U32>(source.vbo_firstvert),
            static_cast<za::U32>(source.numedges), static_cast<za::U32>(source.texinfo->texnum),
            static_cast<za::U32>(sourceIndex), opaque});
    }
    return true;
}
} // namespace

bool buildOpaqueWorldMesh(const SurfaceInput* surfaces, const za::SizeT surfaceCount,
    const za::SizeT vertexCount, OpaqueWorldMesh& out)
{
    if(surfaceCount != 0 && surfaces == nullptr)
        return false;
    if(vertexCount > std::numeric_limits<za::U32>::max())
        return false;

    OpaqueWorldMesh candidate;
    for(za::SizeT i = 0; i < surfaceCount; ++i)
    {
        const SurfaceInput& surface = surfaces[i];
        if(!surface.opaque)
            continue;
        if(surface.vertexCount < 3 || surface.firstVertex > vertexCount ||
            surface.vertexCount > vertexCount - surface.firstVertex)
            return false;

        const za::SizeT triangleCount = surface.vertexCount - 2;
        if(triangleCount > std::numeric_limits<za::U32>::max() / 3u ||
            candidate.indices.size() > std::numeric_limits<za::U32>::max() - triangleCount * 3u)
            return false;

        const za::U32 firstIndex = static_cast<za::U32>(candidate.indices.size());
        for(za::U32 corner = 1; corner + 1 < surface.vertexCount; ++corner)
        {
            candidate.indices.pushBack(surface.firstVertex);
            candidate.indices.pushBack(surface.firstVertex + corner);
            candidate.indices.pushBack(surface.firstVertex + corner + 1);
        }
        candidate.ranges.pushBack({firstIndex, static_cast<za::U32>(triangleCount * 3u),
            surface.material, surface.surfaceIndex});
        const za::U32 surfaceMax = surface.firstVertex + surface.vertexCount - 1;
        if(surfaceMax > candidate.maxVertexIndex)
            candidate.maxVertexIndex = surfaceMax;
    }

    out = static_cast<OpaqueWorldMesh&&>(candidate);
    return true;
}

bool buildOpaqueWorldMeshFromModel(const qmodel_s* rawWorld, const za::SizeT vertexCount,
    OpaqueWorldMesh& out)
{
    const qmodel_t* world = reinterpret_cast<const qmodel_t*>(rawWorld);
    za::Vector<SurfaceInput> surfaces;
    if(!modelSurfaces(world, surfaces)) return false;
    return buildOpaqueWorldMesh(surfaces.data(), surfaces.size(), vertexCount, out);
}

bool buildOpaqueWorldUpload(const ::glvert_s* sourceVertices, const za::SizeT vertexCount,
    const SurfaceInput* surfaces, const za::SizeT surfaceCount, OpaqueWorldUpload& out)
{
    if((surfaceCount != 0 && !surfaces) || (vertexCount != 0 && !sourceVertices) ||
        vertexCount > std::numeric_limits<za::U32>::max())
    {
        return false;
    }

    OpaqueWorldUpload candidate;
    for(za::SizeT i = 0; i < surfaceCount; ++i)
    {
        const SurfaceInput& surface = surfaces[i];
        if(!surface.opaque) continue;
        if(surface.vertexCount < 3 || surface.firstVertex > vertexCount ||
            surface.vertexCount > vertexCount - surface.firstVertex ||
            candidate.vertices.size() > std::numeric_limits<za::U32>::max() - surface.vertexCount)
        {
            return false;
        }
        const za::U32 baseVertex = static_cast<za::U32>(candidate.vertices.size());
        const za::U64 newIndexCount64 = static_cast<za::U64>(surface.vertexCount - 2) * 3u;
        if(newIndexCount64 > std::numeric_limits<za::U32>::max() ||
            candidate.indices.size() > std::numeric_limits<za::U32>::max() - newIndexCount64)
        {
            return false;
        }
        const za::U32 newIndexCount = static_cast<za::U32>(newIndexCount64);

        for(za::U32 vertex = 0; vertex < surface.vertexCount; ++vertex)
        {
            const ::glvert_s& source = sourceVertices[surface.firstVertex + vertex];
            candidate.vertices.pushBack({{source.pos[0], source.pos[1], source.pos[2]},
                {source.st[0], source.st[1]}});
        }
        const za::U32 firstIndex = static_cast<za::U32>(candidate.indices.size());
        for(za::U32 corner = 1; corner + 1 < surface.vertexCount; ++corner)
        {
            candidate.indices.pushBack(baseVertex);
            candidate.indices.pushBack(baseVertex + corner);
            candidate.indices.pushBack(baseVertex + corner + 1);
        }
        candidate.ranges.pushBack({firstIndex, static_cast<za::U32>(newIndexCount),
            surface.material, surface.surfaceIndex});
    }

    out = static_cast<OpaqueWorldUpload&&>(candidate);
    return true;
}

bool buildOpaqueWorldUploadFromModel(const qmodel_s* rawWorld, const ::glvert_s* sourceVertices,
    const za::SizeT vertexCount, OpaqueWorldUpload& out)
{
    const qmodel_t* world = reinterpret_cast<const qmodel_t*>(rawWorld);
    za::Vector<SurfaceInput> surfaces;
    if(!modelSurfaces(world, surfaces)) return false;
    return buildOpaqueWorldUpload(sourceVertices, vertexCount, surfaces.data(), surfaces.size(), out);
}

} // namespace qvr::bsp
