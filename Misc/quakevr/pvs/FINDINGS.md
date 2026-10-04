# start: corrected hidden staircase / slipgate visibility measurements

The 2026-10-04 review invalidated the self-leaf diagnosis and removed `vr_pvs_selfleaf`
from the client, server, cvars and debug menu. Quake's original PVS behavior is restored.

## Decoder corrections

`bspvis.py` previously read a nonzero visibility byte as a run length and copied that
many subsequent bytes. Quake RLE instead copies the nonzero byte itself; only a zero
byte introduces a run of zero bytes. The old tool also sized rows using every BSP leaf
record, including submodels. Rows must use world model `visleafs`, excluding solid leaf 0.
PVS bit N names world leaf N+1. Both client and server follow this convention;
`SV_FindTouchedLeafs` stores leaf indices minus one before the server tests them.

The repaired reader is import-safe, rejects malformed rows, omits padding bits and parses
whole entity records. `extract_pak.py` now reads the declared PAK directory offset and
64-byte records; its old fixed-offset/count interpretation was invalid too.

## Verified map facts

| Map | World visibility leaves | Total leaf records | Missing self bits | Leaves visible from leaf 595 |
|---|---:|---:|---:|---:|
| Original id1/pak0.pak start.bsp | 1128 | 1568 | 0 | 187 |
| quakevr/relit/id1/maps/start.bsp | 1128 | 1568 | 0 | 190 |

An independent decoder and the corrected tool agree. The former claims that 392 leaves
lack their self bit, that leaf 595 sees 591 leaves, and that the client/server use different
bit conventions are false. The server workaround added the next leaf's bit unnecessarily.

The staircase floor/cover is `func_bossgate`, model `*38`, with bounds approximately
(353,1601,-15)..(735,2015,-1). `func_episodegate` model `*41` is an upright episode
barrier at (289,1681,1)..(303,1775,95), spawned with a model only after that episode is
completed. A fresh game does not render it. Brush entity faces are not world marksurfaces.

Previous screenshot deltas do not establish a PVS fix: samples were taken at different
times, and the server change added unrelated visibility. The first sweep also placed
`setpos` before an asynchronous skill restart; those images were taken at spawn.
After waiting for map/skill loading, setpos does move the tracked view with the player.
It also enables noclip, so collision tests must explicitly issue `noclip 0` afterward.

## Reproduction and confirmed rendering regression

Reported position: `map start; wait120; setpos 278 1728 24 7 -20 0; wait60`.
A pure mock yaw change keeps the eye position fixed and cannot change its leaf. Test head
translation separately, and compare portal rendering on/off and `r_novis` at matched
positions. `sweep_images.sh` contains these controls; obsolete self-leaf A/B scripts were removed.
The real regression is cross-view entity culling in `R_RenderView`. `R_MarkSurfaces`
appends view-dependent static entities; `R_SortEntities` then removes brushes outside
that view's frustum from the shared `cl_visedicts` list. Portal and eye views reused
that already-filtered list. At yaw -20, the portal eye at (810,-1,28) culled *38;
the main eyes at (278,1729,56) and (278,1727,56) consequently received no cover.
With portals off, both main eyes received *38 and passed the frustum test.

The fix preserves and restores the input entity list around each rendered view. The
same temporary probe after the fix confirms that the portal still correctly culls *38,
while both main eyes receive it and pass the frustum test. Matched screenshots confirm
the cover remains present with portals on and off. The probe was removed after validation.
In a 200x115-pixel floor region, portals-on/off mean RGB difference was 77.508 before
the fix (91.83% of pixels changed by more than 8) and 0.001 afterward (0% over 8).
The PVS self-bit hypothesis is ruled out; no PVS override is needed.

```sh
python extract_pak.py /path/to/id1/pak0.pak /temporary/output
python bspvis.py /temporary/output/start.bsp audit
python bspvis.py quakevr/relit/id1/maps/start.bsp audit
python q.py --bsp /temporary/output/start.bsp leaf "278 1728 59"
python -m unittest discover -s Misc/quakevr/pvs
```
