# Navmesh pathfinding for walking monsters (Recast/Detour)

Decision log for the navmesh work (brief: arena enemies stuck on the tower stairs and the centerpiece's corners;
Quake's `movetogoal` only heads at the goal, tries diagonals/random directions when blocked and never steps off
ledges).

## Step 1: does Recast/Detour suit us? (2026-11-10) — yes

Checked https://github.com/recastnavigation/recastnavigation (README and releases page):

- License: zlib (License.txt). Compatible with our GPL engine and with shipping binaries; only the notice must stay
  in the source copy.
- Language/platform: plain C++98, no dependencies, builds with any C++ compiler (MSVC/clang-cl/gcc), so it fits our
  clang-cl Windows build and the Linux/flatpak build alike.
- Components we need: Recast (navmesh generation), Detour (runtime mesh, `findPath`, `findStraightPath`, poly
  flags/areas, off-mesh connections). Maybe later: DetourCrowd (local avoidance), DetourTileCache (dynamic
  obstacles). Not needed: RecastDemo, Tests, DebugUtils (our own debug draw is simpler than adapting its interface).
- Maintenance: active (1,184 commits, open PRs, CI). Latest release **v1.6.0** (tag commit `6dc1667`).
- Allocator hooks: `rcAllocSetCustom` / `dtAllocSetCustom` exist in the sources (RecastAlloc.h, DetourAlloc.h), so
  mimalloc can be plugged in.

Verdict: suits our purposes.

## Step 2: getting the source — blocked (worker can't download)

The worker rules forbid downloads, and no copy of the library exists on this machine (searched C: to G: for
`DetourNavMesh.h`). Needed from the coordinator/author: the official tag archive
`https://github.com/recastnavigation/recastnavigation/archive/refs/tags/v1.6.0.zip`, extracted under
`C:/OHWorkspace/qvr-kit/external/recastnavigation-1.6.0` (like the tracy archives there). The worker then vendors only
`Recast/` + `Detour/` (sources and headers) and `License.txt` into `Quake/vr/external/recastnavigation`, records the
version and commit here.

## Plan (to carry out once the source is available)

### Build integration
- Own static-library vcxproj (like the box3d/zancle projects), clang-cl, /O2 and the same runtime library as the
  game in every configuration (Debug too), warnings off for the vendored code only; added to the solution, to
  build.sh-built trees, ReleaseLTO and the release packaging; CMakeLists.txt for the Linux/flatpak build.

### Input geometry: vr_hull's convex brushes (coordinator correction, 2026-11-10)
- Monsters do not move with Quake's hull1/hull2 here: with `vr_mhull 1` (default) each monster class moves with a
  compiled hull of its own width (`vr_mhull_<class>`, -1 = its box) and Quake's height (56 or 88). See HULLS.md,
  "Monsters (round 3: their own widths against walls)", and `Quake/vr/vr_hull.cpp`.
- Recast input = vr_hull's convex-brush rebuild of the world (it already recovers clip brushes from the clipping
  hulls). Each brush is turned into triangles (clip a base winding by the brush planes, one face per plane) and
  rasterised as a solid; Recast then erodes by each class's real radius. No second brush reconstruction is written.
  This answers the clip-brush question: clip brushes are real solids in that rebuild.
- One navmesh per (radius bucket, height) class present on the level: heights 56 / 88, radius quantised to buckets
  (e.g. 12, 14, 20, 32 for the id classes: 24x56 grunt/knight/dog..., 28x56 enforcer, 40x88 ogre, 64x88 vore/shambler).
  A new size seen in play (AD monsters) adds a class built on the pool, like `vr_hull_async`.
- `vr_mhull 0`: Quake's two hull sizes (32x56, 64x88) as the two classes.

### Runtime
- Built on the thread pool at map load, tiled (tiles in parallel); cached to disk (key: BSP hash + settings version
  + class), atomic write, validated on load.
- Doors: polys under a closed door flagged off; plats/trains: off-mesh links; survivable drops: one-way off-mesh links.
- Chase: QC builtin for the next steering corner (findPath + findStraightPath, corridor per monster, staggered
  repaths), stepped with Quake's movestep; fallback to `movetogoal` / `info_arena_nav`. `vr_nav 1` default.
- Debug view `vr_nav_debug 1/2`.
