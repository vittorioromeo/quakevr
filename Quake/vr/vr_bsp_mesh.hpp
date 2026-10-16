// CPU-side opaque BSP draw packet shared by future graphics backends.
#pragma once

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Base/IntTypes.hpp"

struct qmodel_s;
struct glvert_s;

namespace qvr::bsp
{

struct SurfaceInput
{
    za::U32 firstVertex{0};
    za::U32 vertexCount{0};
    za::U32 material{0};
    za::U32 surfaceIndex{0};
    bool opaque{false};
};

struct DrawRange
{
    za::U32 firstIndex{0};
    za::U32 indexCount{0};
    za::U32 material{0};
    za::U32 surfaceIndex{0};
};

struct OpaqueWorldMesh
{
    za::Vector<za::U32> indices;
    za::Vector<DrawRange> ranges;
    za::U32 maxVertexIndex{0};
};

// Minimal owned vertex format for a native renderer's initial BSP upload.
// UVs are the base texture coordinates from glvert_t::st[0..1].
struct UploadVertex
{
    float position[3]{};
    float uv[2]{};
};

struct OpaqueWorldUpload
{
    za::Vector<UploadVertex> vertices;
    za::Vector<za::U32> indices; // local to `vertices`, not the transient engine VBO array
    za::Vector<DrawRange> ranges;
};

// Emits a triangle fan for every eligible convex BSP face. Indices address the
// existing CPU vertex array; surface/material boundaries remain explicit.
// Returns false without changing `out` if any input range or 32-bit count is invalid.
[[nodiscard]] bool buildOpaqueWorldMesh(const SurfaceInput* surfaces, za::SizeT surfaceCount,
    za::SizeT vertexCount, OpaqueWorldMesh& out);

// Adapts the loaded world BSP after R_BuildBModelVertexData has assigned
// msurface_t::vbo_firstvert. Non-world inline brush models are never included.
[[nodiscard]] bool buildOpaqueWorldMeshFromModel(const qmodel_s* world,
    za::SizeT vertexCount, OpaqueWorldMesh& out);

// Copies only opaque world-face positions and base UVs into a compact upload
// packet. The packet no longer borrows the GL renderer's transient CPU array.
// Returns false transactionally for invalid surfaces, spans, or counts.
[[nodiscard]] bool buildOpaqueWorldUpload(const ::glvert_s* sourceVertices,
    za::SizeT vertexCount, const SurfaceInput* surfaces, za::SizeT surfaceCount,
    OpaqueWorldUpload& out);

[[nodiscard]] bool buildOpaqueWorldUploadFromModel(const qmodel_s* world,
    const ::glvert_s* sourceVertices, za::SizeT vertexCount, OpaqueWorldUpload& out);

} // namespace qvr::bsp
