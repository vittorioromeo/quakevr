# Permanent decals (vr_permdecals)

Blood, scorches and chips baked for good into a layer of the map's own faces (the author, 2026-10-14: "make blood and
bullet hole decals permanently 'embedded/mapped' in the level's textures, both to make them never disappear but also
avoid performance issues with multiple static decals"). Spec: `C:/OHWorkspace/qvr-kit/scratch/PERMDECALS.md`.
Branch `agent/permdecals`, pushed to `feature/permdecals`.

## Status (keep current: another agent resumes from here)

- Phase 1 (world + brush models layer, atlas, GPU bake, shader, cvar): **done and tested** (see Tests).
- Phase 2 (saves): **done and tested**: the stamps go into the save as comment lines, baked again after the load.
- Phase 3 (instanced b_*.bsp models, alias props): not started. Idea: a per-entity page set keyed by the entity
  (stamps with an entity key instead of a model), the instance's flag choosing it in the shader; props (alias) keep
  the live decals or the wound masks.
- Phase 4 (coop): not started. The stamps are the events: a server message per stamp (or the clients bake from the
  replicated decal effects as they do now, each its own layer).
- Open: retro textures read the layer smoothly (the live marks' retro path quantises them); fresh marks with a delay
  (gore's flying blood) show at once on doors (baked at once there).

## How it works

- `Quake/vr/vr_permdecals.cpp` (the header has the overview). Each lit world-model face (the world and submodels *n:
  doors, lifts) gets a planar layout at map load (`VR_PermDecalsLayout`, from `GL_BuildBModelVertexBuffer`): axes
  from its texture's s axis, `vr_permdecal_density` units a texel (0.5), pages of 30 x 30 texels, one page table entry
  each (vrclimb: 27143 entries; e1m1 at density 1: 15388).
- Vertex attribute 6 (`glvert_t.grime`): the texel on the face, the face's first page, its pages (across + 4096 down);
  enabled only in the solid pass while the layer has pages (`VR_PermDecalsOn`).
- A page takes a 32 x 32 slot (a texel of rim for the bilinear filter) of an RGB10_A2 atlas (`vr_permdecal_atlas`
  4096: 16384 slots, 85 MB with 4 mip levels, made at the first bake) on first use; full: the least recently baked
  page gives its slot up (`takeSlot`).
- Baking: `bakeSettled` (vr_decals.cpp, from `VR_DecalsFrame`, once a frame) bakes each live mark on the static world
  once it has settled (0.1 s at least, for a hit's second effect to find the first) and drops it from the ring; a mark
  the ring lets go of earlier (more marks in a frame than `vr_decal_max`) is baked then (`bakeEarly`). Marks on brush
  entities are baked at once in the entity's model space (`bakeEntities`; `hitWorld` traces brush entities while the
  layer is on). `flush` draws the queued quads into every mip level with `GL_DST_COLOR, GL_SRC_COLOR` (a texel is half
  the factor; 512/1023 is 1), saving and restoring the draw framebuffer, viewport and scissor.
- Shader: `QVR_PERMDECAL_FUNCTIONS` / `QVR_WORLD_FS_DECALS` (vr_glsl.h): page lookup (SSBO 10), one `textureLod` (unit
  18, the footprint's mip clamped inside the slot), multiplied into the texture, fullbrights and sheen before the
  light, following the parallax shift. `PermDecals` (frame data, after `AmbientLight`): on, slots a row, 1/size, show.
- Saves: `// qvrdecals 1 map <model> n <count>` then `// qvrd <base64>` lines of 35-byte records (`saveLines`), read by
  `VR_ReadSaveInfo` (`readSaveLines`), baked again on the next map load of that map (`restore`, from `bakeSettled`).
- Off (`vr_permdecals 0`): no bake, `hitWorld` as before, attribute 6 not enabled, `PermDecals.x` 0: the old paths
  (bit-identical frames, below).

## Commands and menus

- `vr_permdecal_info` (Debug > Reports "Permanent Decals"): faces laid out, pages, atlas size and memory, slots used,
  stamps kept, baked, given up, dropped.
- `vr_permdecal_show 1` (Debug > Tools "Show Permanent Decal Pages"): the pages drawn unlit and green, the marks on them.
- `vr_decal_at x y z dx dy dz [blood|hole|scorch|pool] [size]`: a mark where the line meets a surface (deterministic:
  rand seeded from its arguments); prints the entity it met.
- Settings > Gore > Marks: "Permanent Decals" (`vr_permdecals`, default 1).

## Tests (headless)

`Misc/quakevr/permdecal_test.sh <agent>` is the regression (marks past a live cap of 16 all baked, two on brush
entities, the same stamps after a load in the same game and in a new one). The rest were one-off scripts in the
worktree's scratch/ (git-ignored):

- Live vs baked (`ab.sh`, vrclimb wall, 8 marks): same shapes and colours; baked a little softer (0.5 units a texel; at
  1 visibly blurrier, at 2 a smudge).
- Off == before (`offeq.sh`, exact setup: `vr_fixed_frames 1; vr_dither 0; r_drawentities 0; ...`, 64 live marks):
  base build a9946d491 vs this build with `vr_permdecals 0`: mean 0.000, peak 0 (bit-identical).
- Lift (`lift.sh`, vrclimb's func_train, entity 10): marks baked on it (`entity 10`), shown with `vr_permdecal_show`
  riding up with it over 6 s.
- Live cap (`cap.sh`, `vr_decal_max 16`, 79 marks laid in bursts): on: 79 stamps baked, all on screen, 0 live; off:
  16 left.
- Save/load (`saveload.sh`): 49 stamps, 479 pages before the save; after `load` in the same game and in a new game:
  49 stamps, 479 pages, the same marks on screen. The save grew by ~2.3 KB (49 records).
- Frame time (`perf.sh`, `--exclusive`, vrclimb, world pass GPU ms, mock eyes): none 0.220 (base build 0.218); 1100
  baked marks 0.227; 1024 live marks (old path) 0.364. CPU unchanged (the world's 0.006).
- Note: a CPU hitch alone shifts the mock's view slightly (`vr_decal_atlas` in an unmarked run: mean 0.8 over the
  frame); whole-frame comparisons need the exact setup above.

## Decisions

- Density 0.5 units a texel, not the spec's ~2 (bullet chips are 6 units: 3 texels at 2 was a smudge); pages of 30
  (not 62) so that scattered small marks take less of the atlas.
- Saves keep the stamps (each mark's record), not the texels: 35 bytes a mark, deterministic re-bake, and the same
  records will serve coop.
- Marks on the boxes made in the game (vr_brushes.cpp) stay live (they go when the box changes).
