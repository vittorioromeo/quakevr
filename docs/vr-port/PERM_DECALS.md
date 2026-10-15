# Permanent decals (vr_permdecals)

Blood, scorches and chips baked for good into a layer of the map's own faces (the author, 2026-10-14: "make blood and
bullet hole decals permanently 'embedded/mapped' in the level's textures, both to make them never disappear but also
avoid performance issues with multiple static decals"). Spec: `C:/OHWorkspace/qvr-kit/scratch/PERMDECALS.md`.
Branch `agent/permdecals`, pushed to `feature/permdecals`; the follow-up (slowdowns, atlas full, on/off) on
`agent/permdecals3` (2026-10-15).

## Status (keep current: another agent resumes from here)

- Phase 1 (world + brush models layer, atlas, GPU bake, shader, cvar): **done and tested** (see Tests).
- Phase 2 (saves): **done and tested**: the stamps go into the save as comment lines, baked again after the load; the
  live ring's marks not baked yet go in too (2026-10-15).
- Slowdowns, atlas full, on/off (2026-10-15): **done and tested**: no glGet in the bake (the slowdown), the page table
  sent in changed runs, emptied slots in one draw, a bake budget; the atlas grows by layers; one on/off rule.
- Phase 3 (instanced b_*.bsp models, alias props): not started. Idea: a per-entity page set keyed by the entity
  (stamps with an entity key instead of a model), the instance's flag choosing it in the shader; props (alias) keep
  the live decals or the wound masks.
- Phase 4 (coop): not started. The stamps are the events: a server message per stamp (or the clients bake from the
  replicated decal effects as they do now, each its own layer).
- Open: retro textures read the layer smoothly (the live marks' retro path quantises them); fresh marks with a delay
  (gore's flying blood) show at once on doors (baked at once there); a stamp whose pages were all dropped (the atlas
  full at vr_permdecal_atlas_max) is not kept for the saves.

## How it works

- `Quake/vr/vr_permdecals.cpp` (the header has the overview). Each lit world-model face (the world and submodels *n:
  doors, lifts) gets a planar layout at map load (`VR_PermDecalsLayout`, from `GL_BuildBModelVertexBuffer`): axes
  from its texture's s axis, `vr_permdecal_density` units a texel (0.5), pages of 30 x 30 texels, one page table entry
  each (vrclimb: 27143 entries; vrfiringrange 82811; e1m1 140588).
- Vertex attribute 6 (`glvert_t.grime`): the texel on the face, the face's first page, its pages (across + 4096 down);
  enabled only in the solid pass while the layer has pages (`VR_PermDecalsOn`).
- A page takes a 32 x 32 slot (a texel of rim for the bilinear filter) of an RGB10_A2 2D array texture on first use:
  `vr_permdecal_atlas` (4096) texels a side a layer, 16384 slots and 85 MB (4 mip levels) a layer. The first layer is
  made in the map's load (with the layer on), with the bake program. Full: it grows by half its layers (at least one;
  the texture made again by the next flush, the old layers blitted over), up to `vr_permdecal_atlas_max` MB (512: 6
  layers at 4096) and no more than the map's pages; full at that, the least recently baked page gives its slot up (an
  O(1) list through the slots; it was a scan of every slot for each page taken) and its marks are lost (counted).
  A new map shrinks it to one layer again.
- Baking: see "When a mark is baked". `flush` (once a frame, from `bakeSettled`) draws, for each mip level and layer
  with work, the emptied slots (one draw, no blending: they were a scissored clear each) then the marks' quads with
  `GL_DST_COLOR, GL_SRC_COLOR` (a texel is half the factor; 512/1023 is 1), and sends the page table's changed entries
  (runs, gaps under 64 entries sent along; all of it when it was made again). It asks GL nothing (no glGet: see
  Performance) and leaves framebuffer 0 bound: it runs in `R_SetupView`, before `R_SetupGL` binds the scene's target
  and viewport (as `VR_RenderShadowMaps` after it does).
- Shader: `QVR_PERMDECAL_FUNCTIONS` / `QVR_WORLD_FS_DECALS` (vr_glsl.h): page lookup (SSBO 10), the slot's layer and
  place (`PermDecals.y` slots a row, its square a layer), one `textureLod` (unit 18, a `sampler2DArray`, the footprint's
  mip clamped inside the slot), multiplied into the texture, fullbrights and sheen before the light, following the
  parallax shift. `PermDecals` (frame data, after `AmbientLight`): on, slots a row, 1/size, show.
- Saves: `// qvrdecals 1 map <model> n <count>` then `// qvrd <base64>` lines of 35-byte records (`saveLines`: the
  stamps, then the live ring's world marks not among them, `decals::liveStamps`), read by `VR_ReadSaveInfo`
  (`readSaveLines`), baked again on the next map load of that map (`restore`, from `bakeSettled`; kept, not baked, with
  the layer off).

## When a mark is baked

The author asked (23:07:45) exactly when marks become baked, and whether baking waits for the bucket limit. It does
not: `vr_decal_bucket_max` only decides which live marks the world shader lists in a 32-unit spot; baking is by time.
Once a frame (`bakeSettled`, oldest first), a live mark on the static world is baked and leaves the live ring when it
has settled:

- bullet chips, scorches, plain blood: 0.1 s after it shows (for a hit's second effect to find the first: `add`);
- marks that spread (`grow`): when spread;
- the gore's marks that dry darker (sprays, runs, pools, splotches: `darken` > 0): after 8 s (or their spread if
  longer), so the layer gets the dried colour.

Sooner: a mark the ring lets go of before then (more than `vr_decal_max` at once, the drops' quarter) is baked on the
spot (`bakeEarly`); marks on doors, lifts and plats (brush entities) are baked at once in the entity's space. Later:
past `vr_permdecal_bake_max` pages (1024) in a frame the rest wait (live) for the next frames. Never: marks on the
boxes made in the game (vr_brushes.cpp) and marks with no face of the layer under them stay live (and fade as before).
So a fresh pool under a corpse stays a live mark for 8 s, then becomes part of the wall; a live mark left out of a full
bucket (a pile of over `vr_decal_bucket_max`, 24, in one spot) is unseen until it bakes.

## On and off

One rule (`permdecals::kept`, vr_permdecals.hpp): **every mark goes into the layer's stamps, on or off. On, they are
all shown, baked. Off (`vr_permdecals 0`), the newest `vr_decal_max` of them show live, exactly as without the layer
(fading after `vr_decal_life`). `vr_decals 0` hides both.** Before, switching off hid every baked mark and left the
live ones not baked yet (the last 0.1-8 s of marks, door marks lost): a random-looking mix.

- Switched off: `showRecord` (vr_decals.cpp) lays the newest world stamps live again (settled, a whole life ahead, older
  than the live marks there were), as many as the ring has room for; door marks stay hidden (the live ring never had
  any). The marks the ring lets go of while off (cap, life) are kept as stamps, not baked (`record`).
- Switched on again: the marks shown from the layer leave the ring (in it already); the stamps kept while off, and the
  live marks as they settle, are baked (`rebake`: only those, the atlas untouched while off).
- A save made off keeps the stamps and the live marks; loaded off, the newest show live; on, all are baked.
- The texture and its memory stay while off (made again only at a new map, and then only with the layer on).

## Performance (2026-10-15; `scratch/burst.sh`: vrfiringrange, gore 2, 32 monsters, four rounds of 4 blasts)

The slowdown: `flush` asked GL for the bound framebuffer, viewport and scissor (glGetIntegerv, glIsEnabled) in every
frame that baked. A glGet waits for the driver's thread and the GPU; with the GPU busy (other runs on the machine, 98%:
a headset's compositor does the same) one flush took up to 325 ms of CPU, 230 of 711 flushes over 0.5 ms. Also: the
whole page table (82811 entries, 324 KB) went up in every frame a page was taken (152 MB in a 6-second fight), and each
emptied slot was a scissored clear in each mip level (up to 145 x 4 a frame).

| the 6 s after the blasts (1460 frames, ~5500 stamps) | before (base build) | after |
| --- | --- | --- |
| flush CPU, average / worst frame (profile scope) | 0.129 / 34.8 ms | 0.003 / 0.09 ms |
| flush CPU worst (own timer), flushes over 0.5 ms | 325 ms, 230 of 711 (glGets kept) | 0.09 ms, 0 of 702 |
| flush GPU, average / worst | 0.045 / 8.1 ms | 0.015 / 0.07 ms |
| page table sent | 152 MB | 0.43 MB |
| emptied slots, GPU worst | 1.18 ms (scissored clears) | 0.05 ms (one draw) |
| first mark's frame | 5-10 ms (program, texture) | in the load |

The marks' quads were never the cost (842 pages in one frame: 0.03 ms GPU). Other numbers: `decals baked` (the CPU
bakes) 0.01 ms average, 0.1-0.6 worst; the live ring's grid rebuild 0.04 / 0.37 ms. Switching off with 5000 stamps:
`showRecord` 0.64 ms (768 marks laid live); on again: 0.46 ms CPU (the 2574 pages of the stamps kept while off; it was a
rebake of everything, 10.5 ms CPU + 23 ms GPU, before it baked only those). Budget: 128 marks settling at once with
`vr_permdecal_bake_max 64` bake over 21 frames, 84 pages the most in one (64 + one mark's).

Off (`vr_permdecals 0`) against the base build, 64 live marks, exact setup: 33 pixels of 518400 differ by 1 (the world
shader built with the array sampler), mean 0.00.

## Commands and menus

- `vr_permdecal_info` (Debug > Reports "Permanent Decals"): faces laid out, pages, the atlas's layers and memory (and
  its most), slots used, growths, stamps kept, baked, given up, dropped; the flushes' quads, the most in one, the table
  sent, their CPU time at most and how many over 0.5 ms; "kept while off" when some wait to bake.
- `vr_permdecal_show 1` (Debug > Tools "Show Permanent Decal Pages"): the pages drawn unlit and green, the marks on them.
- `vr_decal_at x y z dx dy dz [blood|hole|scorch|pool] [size]`: a mark where the line meets a surface (deterministic:
  rand seeded from its arguments); prints the entity it met.
- Settings > Gore > Marks: "Permanent Decals" (`vr_permdecals`, default 1; its help says what off shows).
- `vr_permdecal_atlas` (4096, a layer's side), `vr_permdecal_atlas_max` (512 MB), `vr_permdecal_bake_max` (1024 pages a
  frame; not archived): console only.

## Tests (headless)

`Misc/quakevr/permdecal_test.sh <agent>` is the regression (7 runs, ~3 min): (1) marks past a live cap of 16 all baked,
two on brush entities, the same stamps after a load in the same game and in a new one; (2) the burst budget (above);
(3) at density 0.25 a 1024 atlas grows to 4 layers with none given up, its frame the same as one 4096 layer's (peak 1);
(4) the on/off rule in exact frames (`permdecal_shots.py`, peaks): on -> off -> on the same frame (1), off the same as
the layer off from the start (1), vr_decals 0 the same as no marks at all (1) and 1 the same as before (1), marks made
while off kept and baked once on. One-off scripts in the worktree's scratch/ (git-ignored):

- Live vs baked (`ab.sh`, vrclimb wall, 8 marks): same shapes and colours; baked a little softer (0.5 units a texel; at
  1 visibly blurrier, at 2 a smudge).
- Off == before (`offeq.sh`): above.
- Lift (`lift.sh`, vrclimb's func_train, entity 10): marks baked on it (`entity 10`), shown with `vr_permdecal_show`
  riding up with it over 6 s.
- Save/load (`saveload.sh`): 49 stamps, 479 pages before the save; after `load` in the same game and in a new game:
  49 stamps, 479 pages, the same marks on screen. The save grew by ~2.3 KB (49 records). Off (`saveoff.sh`): 31 stamps
  + 16 live saved (47); loaded off, 16 live; on, 47 baked, 0 live (the same 479 pages as on all along).
- Atlas capped at one 1024 layer (`atlas.sh`, density 0.25, 158 marks): 2922 pages given up, 9589 dropped, 97 stamps
  left: what growth avoids.
- Frame time (`perf.sh`, `--exclusive`, vrclimb, world pass GPU ms, mock eyes): none 0.220 (base build 0.218); 1100
  baked marks 0.227; 1024 live marks (old path) 0.364. CPU unchanged (the world's 0.006).
- Note: a CPU hitch alone shifts the mock's view slightly; whole-frame comparisons need the exact setup above, and the
  view still turns for a while after the start (compare shots taken after the same frames).

## To try in VR

- A long fight (e1m1, gore 2): marks never fade; no stutter when a lot of blood lands at once (it used to wait on the
  GPU in every frame that baked); the frame rate where blood piles up stays as it was with none.
- Up close: baked marks a little softer than fresh ones (0.5 units a texel); `vr_permdecal_density 0.25` (next map)
  sharper, at four times the atlas's use.
- Permanent Decals off in the middle of a bloody room: the newest marks stay (as many as Max Decals), the rest go; on
  again: all back, nothing doubled darker. Marks made while off are there too once on.
- Shoot or bleed on a door or lift, then ride or open it: the marks move with it (hidden while the layer is off).
- Save, load (also after quitting): the marks come back, the freshest ones too.
- Debug > Tools > Show Permanent Decal Pages: where the layer has texels.

## Decisions

- Density 0.5 units a texel, not the spec's ~2 (bullet chips are 6 units: 3 texels at 2 was a smudge); pages of 30
  (not 62) so that scattered small marks take less of the atlas.
- Saves keep the stamps (each mark's record), not the texels: 35 bytes a mark, deterministic re-bake, and the same
  records will serve coop.
- Marks on the boxes made in the game (vr_brushes.cpp) stay live (they go when the box changes).
- Atlas full: grow (layers of an array texture) rather than downsample or merge old pages: the same look, a shader
  change of three lines, memory only when used; a map's pages bound it (vrfiringrange's four rounds of blasts: 7350
  pages, under half a layer). Downsampling would lose detail on the oldest marks and need mixed slot sizes in the page
  table. The copy when growing doubles the memory for a moment (blit, ~1 ms GPU a layer).
- Off shows the old live ring rather than nothing: switching never empties a room, and off looks exactly as the game
  did before the layer (tested frame for frame). Door marks stay hidden while off (the live ring has none).
- The flush leaves framebuffer 0 bound instead of restoring: no glGet; it runs before `R_SetupGL` (the scene's target)
  as the shadow maps do.
