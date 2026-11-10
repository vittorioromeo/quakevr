# Recast/Detour (vendored: Recast and Detour only)

Mikko Mononen's navigation-mesh toolset, zlib licence (`License.txt`).

- Upstream: https://github.com/recastnavigation/recastnavigation, tag `v1.6.0` (commit `6dc1667`)
- Archive: `recastnavigation-1.6.0.zip` (GitHub's tag archive, 1.3 MB), SHA-256
  `8B50C62177249554B226514B307E6C529934A62B3926822777BF6ABB8906A155`
- Vendored: `Recast/Include`, `Recast/Source` (without `RecastLayers.cpp`: DetourTileCache's layers, unused),
  `Detour/Include`, `Detour/Source`, `License.txt`. Not vendored: DetourCrowd, DetourTileCache, DebugUtils,
  RecastDemo, Tests, the CMake files. No local changes: the files are upstream's.

Used by `Quake/vr/vr_nav.cpp` (docs/vr-port/NAVMESH.md). Built as its own static library, always optimised without its
asserts (NDEBUG), Debug builds too, warnings off: `Windows/VisualStudio/recast.vcxproj` (in the solution, referenced by
`quakevr.props`), `Quake/vr/vr.cmake` (`qvr_recast`), `Quake/vr/vr.mk` (the Makefiles). Its allocations go through
`rcAlloc` / `dtAlloc`, whose default is malloc: the process's C heap, which is mimalloc (`vr_crtheap.c`), so no custom
allocator is installed.
