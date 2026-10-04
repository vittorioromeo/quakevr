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

## 4. 2026-11 (agent/pvs-selfbit): the head CAN be placed; the gate is not a static model; measured

**`setpos` does place the view in VR runs.** The §2 failure was the script, not the engine: `sweep_images.sh` runs
`skill 2` (which restarts the level) with **no `wait` before `setpos`**, so the restart landed after it and put the
player back at the spawn. With `map start;wait120;god;notarget;setpos 278 1728 24 7 -20 0;wait60` the temporary
`R_MarkSurfaces` probe printed, every sampled frame: `QVRVIS branch 2 leaf 595 nearwater 0 eye 278 1729 59
vieworg 278 1729 59` — leaf **595**, `branch 2` = `Mod_LeafPVS`, the fat PVS is not what the view reads here.
`vr_hull_leafdebug` (Debug) confirms it independently: at `278 1728 28` the leaf is the box `264 1672 -28 .. 280 1768
28` (the gate's leaf); at `278 1620 28` it is `232 1480 -28 .. 336 1656 36` (a different leaf). `setpos` sets the
server origin and the head follows him (`vr_hands.cpp`: the eyes are `cl_entities[cl.viewentity].origin` plus the
head's offset from the play-space floor; the eye lands 35 units over the origin). `vr_mock_look <pitch> <yaw>` turns
the view **without moving the eye**, so a pure yaw sweep cannot cross a leaf boundary in the mock — the flicker is
reproduced by moving the head (`vr_mock_hand head x 1.7 z`, 0.3 m ≈ 8 units) or by `setpos`, not by looking.
Both probes were removed before the commit; the numbers below are from the runs that had them.

**The episode gate is not in the client's static-model list.** A temporary probe in `R_AddStaticModels` printed
`QVRPVS leaf 595 selfleaf 1 statics 10 drawn 0` with no "own leaf 595" line at all: `start` has 10 static entities,
none has leaf 595 in its `cl_efrags` list, none is drawn from leaf 595. §3's model-`*41` leaf lookup is the BSP's own
leaf record, not a client visedict. What is culled is the world's own faces: `R_MarkVisSurfaces (vis)` marks the
marksurfaces of the leafs the PVS names, and leaf 595 is not among them, so the faces whose only non-solid neighbour
is leaf 595 (the gate brush's own faces) are dropped. Standing in a leaf you see it only through its neighbours' PVSs.

**STEP 3, measured** (`Misc/quakevr/pvs/selfleaf_shots.sh`, `diff_shots.sh`; 720×720 mirror shots, `pngdiff.py`
reports mean abs diff / % of pixels changed by >8):

| at `setpos 278 1728 24` (eye 278 1729 59, leaf 595) | head yaw −60 | −30 | 0 | +30 | +60 |
|---|---|---|---|---|---|
| `vr_pvs_selfleaf 0` vs `1`, % pixels changed | 0.84 | 4.55 | 5.17 | 5.23 | 3.99 |

The switch changes the picture at every yaw (the gate's faces enter the visible set; small at −60 where the brush is
edge-on). The sweep itself is unchanged by the switch — within-flag yaw-to-yaw deltas are 74–79% either way (off/on:
78.89/78.75, 74.10/74.09, 76.05/75.97, 79.04/79.05), i.e. it adds a stable set of faces, not a flicker.
The flicker is the leaf boundary, and it is gone: at the in-leaf spot off vs on = **3.91%** (the gate appears), at
`setpos 278 1620 24` (a different leaf, looking at the same gate) off vs on = 1.76% (only that leaf's own faces).
With the switch on the gate is drawn from both spots; with it off it is missing from one of them.

**STEP 4, e1m1.** At the spawn, `vr_pvs_selfleaf 0` vs `1` at the same moment: **0.000 mean abs diff, 0 pixels
changed (max 3)** — identical, early and late. The only 2.13% delta seen in the first run also appears in an
off-vs-off pair 200+ waits apart (luma 15.77 → 16.51), so it is a map-side time event, not the switch. Nothing that
should stay hidden became visible at the spot tested (one spot only).

**STEP 4, cost** (`vr_profile 1; vr_profile_gpu 1; …; vr_profile_dump`, `--exclusive`, fast mode, e1m1, warm windows):

| window | frames | ms apart | host frame | CPU busy | GPU |
|---|---|---|---|---|---|
| off (warm-up) | 399 | 4.17 | 0.59 | 0.55 | 0.74 (max 1.68) |
| **on** | 639 | 4.01 | 0.41 | 0.36 | 0.69 (max 1.52) |
| **off** | 639 | 4.00 | 0.39 | 0.35 | 0.70 (max 1.50) |

on vs off, matched windows: 4.01 vs 4.00 ms apart, host 0.41 vs 0.39, GPU 0.69 vs 0.70 — inside the noise. The cost
is one bit per leaf: `Mod_LeafPVS` sets one bit in a decompressed PVS (no extra work), `SV_AddToFatPVS` sets one bit
per leaf its sample reaches, so a fat PVS names one extra leaf and `SV_EdictInPVS` / `SV_WriteEntitiesToClient`
(line 669 / 755) can transmit the entities standing in the leaf the player is in — which is what the fix is for.

**The two conventions, and the one place they meet.** The server writes and reads raw leaf numbers
(`sv_main.c:613` sets bit `leafnum`; `:669` and `:755` test `pvs[leafnums[i] >> 3]`), so the server bit is `leaf`.
The client's lists are packed as `idx - 1` (`gl_refrag.c:71`) and tested against that, so the client bit is `leaf-1`
(`Mod_LeafPVS`). `SV_FatPVS` is handed to the client in the `nearwaterportal` branch and read there with the client's
convention — so the raw bit I add there names leaf `leaf+1` to the client, an off-by-one that vanilla already has for
that whole buffer. It can only ever name one extra leaf adjacent to the sample leaf (over-draw near water, never a
cull), and it cannot change entity transmission (the server's readers use raw bits, which is what is set). No separate
gate is needed; if exactness near water is ever wanted, add bit `leaf-1` in `SV_AddToFatPVS` too — at the cost of one
more leaf of entities transmitted.
