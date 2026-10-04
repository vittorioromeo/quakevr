# start: hidden staircase / slipgate PVS — measurements and what they rule out

Probe position reported by the author: `setpos 278 1728 24 7 -20 0` in `start`, game time ~748 s.

## 1. The `nearwaterportal` / `SV_FatPVS` explanation is DISPROVED

Temporary probe in `Quake/r_world.c R_MarkSurfaces`, just after the `vis` selection:

```c
	if ((r_visframecount & 63) == 0)
		Con_DPrintf ("QVRVIS branch %d leaf %d nearwater %d eye %.0f %.0f %.0f\n", branch,
			(int) (r_viewleaf - cl.worldmodel->leafs), (int) nearwaterportal,
			r_origin[0], r_origin[1], r_origin[2]);
```
(`branch` 0 = `Mod_NoVisPVS`, 1 = `SV_FatPVS`, 2 = `Mod_LeafPVS`. Run with `developer 1`.)

Result, every sampled frame, before and after `setpos`:

```
QVRVIS branch 2 leaf 1084 nearwater 0 eye 810 24 31
```

- **branch 2 — `Mod_LeafPVS` is the vis the view reads; `nearwater 0`.** So the fat-PVS branch is not taken,
  and `R_AddStaticModels` (which is passed this same `vis`) culls static brush entities with it.
- The self-bit change in `Mod_LeafPVS` was therefore live, and it still changed nothing measurable
  (gates-off vis vs `r_novis 1` at yaw -20: 17.65% px after vs 17.79% before — noise). Reverted.

## 2. The bigger finding: `setpos` does not move the view in these VR runs

`setpos` is a client command forwarded to the server (`host_cmd.c:3781` → `sv_user.c:578`), but the probe shows
the view origin at `810 24 31` (leaf 1084) both before and after `setpos 278 1728 24 7 -20 0`. In the VR build the
view origin comes from the tracked head pose, not from the server player origin, so `setpos` does not place the
head. **All six eyeshots in `sweep_images.sh`, and therefore the 17.8% / 16.1% pixel deltas, were measured at the
default spawn position — not at the reported spot.** They are not evidence about the episode gate.

Next step (not mine to decide): place the *head* at the reported spot — the `QVRVIS eye` probe above is the way to
confirm it landed — then re-run the 6-phase sweep and only then re-open the PVS question.

## 3. Offline BSP facts (still valid; `q.py` / `bspvis.py` on `start.bsp`)

- 1568 leaf records (index 0 = common solid leaf), 1128 carry vis data; **392 of them do not name themselves**.
- `278 1728 24` (and z 48/64/75/96/120/150) is leaf **595**; its PVS names 591 leafs and not itself.
- The `func_episodegate` brush (model `*41`, `289,1681,1`-`303,1775,95`) resolves to leaf **595** alone.
- 699 leafs' PVS do name 595.
- Bit convention, from the client code: `Mod_LoadLeafs` sets `numleafs = count` (all records, leaf 0 included);
  `r_brush.c` packs `leafs[i+1]` as bit `i`; `gl_refrag.c R_AddStaticModels` tests `idx - 1` → **bit k names leaf
  k+1**. `sv_main.c:659,:745` test the raw 0-based leaf number — a different convention (pre-existing, unchanged).
  The visdata itself is not self-consistent with either (bit 0 set in 104 blobs, bit 1567 in 471), and a
  portal-adjacency test was inconclusive (1192 vs 1205 of 1859 pairs), so the convention is proven from code only.
