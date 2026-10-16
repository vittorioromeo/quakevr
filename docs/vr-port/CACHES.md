# The derived-data cache

Work the loads would redo every session, kept on disk (`Quake/vr/vr_diskcache.hpp`, `.cpp`). Nothing in it is ever
committed or shipped: everything is derived from the player's own game files.

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
| compiled hull trees | `hulls` | `vr_hull.cpp` | world's nodes and planes, the box | `treeVersion` (+ layout, brushes' `buildVersion`) | `vr_hull_cache` | 1 GB |
| occlusion bakes | `ao` | `vr_ao.cpp` | SHA-256 of poses, triangles, scale, origin, the bake's numbers | `BAKE_VERSION` | `vr_ao_cache` | 64 MB |
| dequantised poses | `dequant` | `vr_mdldequant.cpp` | poses, frames (poses, names), the progs' loops it takes | `dequantVersion` | `vr_mdl_dequant_cache` | 256 MB |
| ragdoll rigs | `rigs` | `vr_ragdoll.cpp` derive | the seed table, poses, scale and origin, triangles' vertices | `rigVersion` | `vr_ragdoll_rig_cache` | - |

**Bump the version constant** whenever a change could make a different result from the same inputs (the maker, its
constants, a table it reads such as the ragdoll seed tables or Quake's normal table). A forgotten bump shows with the
product's setting at **2**: made anyway and compared with the file; `vr_cache_info` counts "checked N same M differed"
and each difference prints a warning. Setting 0: never read nor written (made every time).

Not cached, on purpose: limb models (built from the .mdl in well under a millisecond; their skins' normal maps, poses
and occlusion go through the products above), decoded textures (raw RGBA is gigabytes for a texture pack; the image
look-ahead decodes them on the pool instead, `vr_texture_prefetch`), runtime prop hulls (one per gib size and frame:
built ahead on the pool, `vr_box3d_hulls_ahead`).

## Counts and gaps

`vr_cache_info` (Debug > Profiling and Memory > Cache Info): per product its build's files and size on disk, and this
session's reads (MB, ms), misses, writes, rejected files and checks; then the warm-up's progress. A miss while a map
is in play (not during a load) counts as "in play" and, with `developer 1`, prints `cache miss in play: <product>
<what>`: a first use the loads didn't prepare. (A load's occlusion bakes finish on their task after the load, so the
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
