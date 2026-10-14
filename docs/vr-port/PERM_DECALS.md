# Permanent decals (vr_permdecals)

Blood, scorches and chips baked for good into a layer of the map's own faces (the author, 2026-10-14: "make blood and
bullet hole decals permanently 'embedded/mapped' in the level's textures, both to make them never disappear but also
avoid performance issues with multiple static decals"). Spec: `C:/OHWorkspace/qvr-kit/scratch/PERMDECALS.md`.
Branch `agent/permdecals`, pushed to `feature/permdecals`.

## Status (keep current: another agent resumes from here)

- Phase 1 (world + brush models layer, atlas, GPU bake, shader, cvar): **implemented, first tests pass** (see Tests).
  Next: door test (vrclimb lift), cap test, frame-time and memory numbers, off == before proof against the base build.
- Phase 2 (saves): not started. Plan: the stamp list (`permdecals::stamps`, each mark in its model's space) saved with
  the game and baked again on load (simpler and smaller than the atlas's texels; the bake is deterministic).
- Phase 3 (instanced b_*.bsp models, props): not started. Phase 4 (coop): not started; the stamps are the events.

## How it works

- `Quake/vr/vr_permdecals.cpp` (header has the overview). Each lit world-model face (world and submodels *n: doors,
  lifts) gets a planar layout at map load (`VR_PermDecalsLayout`, from `GL_BuildBModelVertexBuffer`): axes from its
  texture's s axis, `vr_permdecal_density` units a texel (0.5), pages of 30 x 30 texels, one page table entry each.
- Vertex attribute 6 (`glvert_t.grime`): the texel on the face, the face's first page, its pages (across + 4096 down).
- A page takes a 32 x 32 slot (a texel of rim for the bilinear filter) of an RGB10_A2 atlas (`vr_permdecal_atlas`
  4096: 16384 slots, 85 MB with 4 mip levels) on first use; full: the least recently baked page gives its slot up.
- Baking: `bakeSettled` (vr_decals.cpp, from `VR_DecalsFrame`, once a frame) bakes each live mark on the static world
  once it has settled (0.1 s at least) and drops it from the ring; marks on brush entities are baked at once in the
  entity's model space (`bakeEntities`; `hitWorld` traces brush entities while the layer is on). `flush` draws the
  queued quads into every mip level with `GL_DST_COLOR, GL_SRC_COLOR` (texel = half the factor; 512/1023 is 1).
- Shader: `QVR_PERMDECAL_FUNCTIONS` / `QVR_WORLD_FS_DECALS` (vr_glsl.h): page lookup (SSBO 10), one `textureLod` (unit
  18), multiplied into the texture and fullbrights before the light, following the parallax shift.
- Off (`vr_permdecals 0`): no bake, `hitWorld` as before, attribute 6 not enabled, `PermDecals.x` 0: the old paths.

## Commands

- `vr_permdecal_info`: faces laid out, pages, atlas size and memory, slots used, stamps, evictions.
- `vr_decal_at x y z dx dy dz [blood|hole|scorch|pool] [size]`: a mark where the line meets a surface (deterministic).

## Tests (headless)

- `scratch/ab.sh <0|1> <out.png>` (vrclimb wall, 8 marks): live vs baked look alike (shape, colour); baked is a
  little softer (0.5 units a texel; at 1 it was visibly blurrier, at 2 a smudge: the default is 0.5).
- Note: a CPU hitch alone shifts the mock's view slightly (`vr_decal_atlas` in an unmarked run: mean 0.8 over the
  frame), so whole-frame pixel comparisons need runs with the same hitches.

## Decisions

- Density 0.5 units a texel, not the spec's ~2 (bullet chips are 6 units: 3 texels at 2 was a smudge).
- Saves keep the stamps, not the texels (phase 2 plan above).
- Marks on the boxes made in the game (vr_brushes.cpp) stay live (they go when the box changes).
- Retro textures: the baked layer is read smoothly (not quantised like the live marks' retro path).
