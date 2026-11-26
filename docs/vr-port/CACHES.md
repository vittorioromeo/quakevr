# The derived-data cache

Work the loads would redo every session, kept on disk (`Quake/vr/vr_diskcache.hpp`, `.cpp`). Nothing in it is ever
committed: everything is derived from the player's own game files. A release ships a read-only part made for Quake VR's
own maps ("The shipped cache" below).

## Where

`<cache root>/<product>/<build>/<file>`

- **Root:** `vr_cache_dir` when set (tests: point it at an empty folder for a first-ever session), else `<base>/cache`
  (the last base directory's, beside the map index's files). One root for every game folder: a file's name is a hash of
  everything its maker reads, so id1's grunt read through hipnotic or AD is the same file. Until 2026-10-16 each game
  folder had its own `<gamedir>/cache/<kind>` (those old folders are no longer read and can be deleted;
  `<gamedir>/cache/prefetch.txt`, the texture look-ahead's per-game list, stays where it was).
- **Build:** the maker's own version (a constant in its source; see below). Made once a session at its first use; the
  product's other builds' folders are removed then, and its oldest files past its budget.
- **Writes:** beside their place, then renamed (`diskcache::writeAtomic`): another copy of the game never reads half a
  file. Small files are written on the pool (`diskcache::write`).

## Products

| product | folder | made by | key (hash of) | version constant | setting | budget |
|---|---|---|---|---|---|---|
| skins' normal maps | `normalmaps` | `vr_normalmaps.cpp` TexMgr_SkinToNormals | texels, size, heights, texels per unit, islands | `NORMALCACHE_VERSION` | `vr_normalmap_cache` | - |
| guns' convex pieces | `gunpieces` | `vr_box3d.cpp` cutGun / `vr_convex.cpp` | triangles, cut settings | `cutVersion` (+ Box3D's) | `vr_gun_pieces_cache` | - |
| compiled hull trees, and the map's brushes they are compiled from (`<world>.hbr`: the walk, clip brushes and holes; loadprof) | `hulls` | `vr_hull.cpp` | world's nodes and planes, the box (the brushes: `keyOf`, entities and the holes' settings too) | `treeVersion` (+ layout, brushes' `buildVersion`; the brushes' file `brushesVersion`) | `vr_hull_cache` | 1 GB |
| big maps' model light grids | `lightgrid` | `vr_ambient.cpp` | lightmaps, styles and planes, the grid's size, `vr_model_light_grid_near`, the lit liquids' colours | `GRID_CACHE_VERSION` | always (no setting) | 256 MB |
| occlusion bakes | `ao` | `vr_ao.cpp` | SHA-256 of poses, triangles, scale, origin, the bake's numbers | `BAKE_VERSION` | `vr_ao_cache` | 64 MB |
| dequantised poses | `dequant` | `vr_mdldequant.cpp` | poses, frames (poses, names), the progs' loops it takes | `dequantVersion` | `vr_mdl_dequant_cache` | 256 MB |
| monsters' navmeshes | `navmesh` | `vr_nav.cpp` | the map's solids as triangles (world, clip brushes, still brush entities), the build's settings, the size class | `navVersion` (+ Detour's `DT_NAVMESH_VERSION`) | `vr_nav_cache` | 256 MB |
| ragdoll rigs | `rigs` | `vr_ragdoll.cpp` derive | the seed table, poses, scale and origin, triangles' vertices | `rigVersion` | `vr_ragdoll_rig_cache` | - |

**Bump the version constant** whenever a change could make a different result from the same inputs (the maker, its
constants, a table it reads such as the ragdoll seed tables or Quake's normal table). A forgotten bump shows with the
product's setting at **2**: made anyway and compared with the file; `vr_cache_info` counts "checked N same M differed"
and each difference prints a warning. Setting 0: never read nor written (made every time).

Not cached, on purpose: limb models (built from the .mdl in well under a millisecond; their skins' normal maps, poses
and occlusion go through the products above), decoded textures (raw RGBA is gigabytes for a texture pack; the image
look-ahead decodes them on the pool instead, `vr_texture_prefetch`), runtime prop hulls (one per gib size and frame:
built ahead on the pool, `vr_box3d_hulls_ahead`).

Gun pieces are prepared at a map's load for every weapon the map can give (lying, its monsters' drops, backpacks, the
player's hands and holsters; `prepareHeldGuns`, QC `vr_gunprep.qc`), read from the file or cut on
the pool on a miss, so a first pickup in play is a hit (ROUND22.md, "Held guns' convex pieces at map load").

## The shipped cache

A release's `quakevr/cache_shipped` (made at release time, never committed: `quakevr/.gitignore`): the products made by
loading Quake VR's own first maps, read where the player's cache has no file (or one amiss). Never written nor pruned.

- **Lookup:** `cache_shipped/index.txt`, one line a file: the player's cache's own name `<product>/<build>/<key>.<ext>`
  (the same hash keys and builds: a shipped file is read only for exactly what it was made from), then its file in the
  folder, named by map (`hulls/vrstart_8_8_28.hul`, `lightgrid/vrstart.mlg`) so the next release replaces it in place:
  Setup's Quest step never deletes a file, and keyed names would pile up on the headset at every update (the PC
  installer removes what a release no longer ships). Read once, at the first lookup (main thread). `diskcache::read`
  looks there by itself; the hulls' own reader (`vr_hull.cpp` diskRead) takes `shippedFileOf`'s path from its job.
- **Setting:** `vr_cache_shipped` 1 (Debug > Profiling and Memory, "Read the Shipped Cache"); 0 never read (the
  generator's runs, a first load's timing). `vr_cache_info`: "N read (M shipped, ...)" per product and a last line
  with the shipped folder, its files and size.
- **Made by** `Misc/release/make_shipped_cache.py` (make_release.ps1's "Shipped cache" step, RELEASING.md): the
  packaged game, in a copy of the package's `quakevr` (no player config: the shipped defaults), loads each map alone
  (`-prepare <file> -preparemaps <map>`: the installer's preparation run limited to those maps, then quit;
  `+vr_setup_pending 0`, or a first start's calibration room replaces the map) with an empty `vr_cache_dir` and
  `vr_cache_shipped 0`, and copies the products' files of that run.
- **What ships** (2026-11-25, loadprof2): vrstart's 8 hull trees (130.6 MB, 61 MB deflated: the player's and the
  monsters' boxes) and light grid (47.7 MB, 21 MB), vrtutorial's light grid (4.7 MB) and 2 small trees (0.4 MB), and the
  three maps' brushes (`.hbr`, the hull's walk, clip brushes and holes: vrstart 15.0 MB, 4.8 MB deflated; vrtutorial
  0.9 MB; vrcalibration 0.1 MB; ~10 s of the Quest 3's hub load): 15 files, 199.4 MB, 84.8 MB deflated. The generator's
  summary lists the brushes apart; no default map builds a navmesh (`navmesh` is in the product list: one made
  would ship, ~240 KB). Only products of our maps' geometry, lightmaps and entities: nothing keyed by a model, skin or
  texture (occlusion, normal maps, poses, rigs, gun pieces), which may be id Software's, and no id map's.
- **Platforms:** the files are the same bytes on the PC (x64) and the Quest (arm64): little-endian both; the layouts
  are plain ints, floats and shorts (`mclipnode_t`, `mplane_t`: their sizes are in the hulls' build; `GridEntry`);
  the keys are wyhash/FNV over the same loaded bytes; both builds compile without FP contraction
  (`-ffp-contract=off`: quakevr.props, Android/android.cmake) and the makers use no libm transcendental. A key that
  differs on a platform (another texture's liquid colour in the light grid's key, a setting) is only a miss.
- **Measured (PC, cold `vr_cache_dir`, vrstart):** without (vr_cache_shipped 0) the load 17.4 s (trees 5.4 s, light
  grid 5.8 s); with it 6.1 s (trees 0.14 s, grid 0.04 s: 8 + 1 shipped reads, 0 builds). `vr_hull_cache 2` with the
  shipped trees: "checked 8 same" (a fresh compile gives the same bytes). A stale index (other keys: a changed map)
  or a file amiss (counted rejected): built and written to the player's cache as before.

## Counts and gaps

`vr_cache_info` (Debug > Profiling and Memory > Cache Info): per product its build's files and size on disk, and this
session's reads (MB, ms), misses, writes, rejected files and checks; then the warm-up's progress. A miss while a map
is in play (not during a load) counts as "in play" and, with `developer 1`, prints `cache miss in play: <product>
<what>`: a first use the loads didn't prepare. Its last line counts the models loaded from their files in play
("models loaded in play"; `developer 1`: `model loaded in play: <name>, <ms>`), each a frame's hitch. (A load's occlusion bakes finish on their task after the load, so the
VR-derived models' bakes show there on a first visit.)

## The warm-up

`vr_cache_warmup 1` (Debug > Profiling and Memory, with its status line; `vr_cache_warmup_info`): five seconds after
the start-up, every installed game's `.mdl` files (each base directory's game folders: their `.pak` entries and loose
`progs/**.mdl`) are listed on the pool, then their occlusion bakes are made one model at a time on one pool thread at
idle priority. Paused while a map loads and for 2 s after a frame over 14 ms; the model under way finishes. Done pak
files and loose files go into `<root>/warmup/done_w<warm-up version>_ao<bake version>.txt`, so the next session goes
on from there (and a finished warm-up lists nothing). Off in test runs (`QVR_TEST_HIDDEN`) unless
`vr_cache_warmup_debug 2`, and in the installer's preparation run (`-prepare`), which loads maps and every model with
the real loaders and so fills every product.

Only the occlusion bake can be made from a model's file alone; the other products need the loaded model (normal maps:
the texture manager's skin conversion and islands; dequantised poses: the progs' loops; rigs: the loaded header) or the
loaded map (hulls). They come from the loads (and the map load's look-ahead: the probes of what can appear later,
`vr_probe_kinds`, and the limbs prebuilt, `vr_limbs_prebuild`) and from the installer's preparation run.
