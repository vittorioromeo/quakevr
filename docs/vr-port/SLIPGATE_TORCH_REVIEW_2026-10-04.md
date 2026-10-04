# Review of torch, slipgate and PVS work — 2026-10-04

Reviewed HEAD `0110e221`, including `0ac23671`, `d3baeab3`, `ad20f2c1`, `89ca5735`, `ce1e1ed1`, the PVS investigation/probes through `b4767f23`, and `557232f3` / `477fc6bd`. Intent was checked against the transcripts in `quakevr/notes/`. The review was followed by the fixes authorized by the author, described below.

**Result: all five review findings corrected, and the actual staircase rendering regression diagnosed and fixed.**
The master slipgate switch had no defect identified in this review.

## Fixes and regression results

- Player entry now requires the torso to reach the gate plane, movement into the selected face, and the full effective collision box to fit its aperture. The old reach-short-of-plane rule, timed vanilla-teleport bypass and sideways destination-centre fallback were removed. Both frame-edge jumps remain on the source side. Centre jumps cross at y=1384. Off-centre x220/x244 jumps arrive at x532/x556. All three skill gates passed; the two-sided gate correctly crosses at y1732 from the front and y1740 from the back. Waiting at its opposite face does not cross. Walking without jumping still stops at the low sill, as intended.
- `vr_pvs_selfleaf` was removed entirely from client/server code, cvars and menu. The PVS reader, leaf numbering, world row size, PAK extractor and entity queries were repaired. Five diagnostic regression tests pass; independent and repaired readers both report zero missing self bits across all 1128 world leaves in each start map. Obsolete self-leaf A/B scripts and false completion claims were removed.
- The staircase cover is *38 (`func_bossgate`). Portal rendering culled it from the shared entity list before either eye could draw it. `R_RenderView` now restores its input entity list after each view. Temporary before/after probes confirmed portal culling is independent of both eyes; the probes were removed. Matched portals-on/off screenshots changed 91.83% of a 200x115 floor region before the fix, versus 0% afterward (mean RGB difference 77.508 -> 0.001).
- Portal views now use the same exact rigid transform as movement and shots at every distance. The camera-only 256-unit clamp was removed because it altered the targets shown by screen-space sampling. Diagnostics now report yaw in degrees correctly and distinguish geometry depth from the near-depth mask.
- Wall torch thrown contacts now respect `vr_walltorch_shot`. A fast throw with the switch off leaves the torch mounted; enabling it knocks the torch off. The laser diagnostic now uses the player's rifle missile behavior; torch test scripts reset throw speed per case, exercise disabled throws, and wait beyond a grenade's full fuse.

The engine and QuakeC were rebuilt. No real-headset, remote-client or all-map validation was performed.
The finding sections below preserve the original review evidence; shipped-source line numbers refer to the reviewed commits.


## 1. [P1] Jumping against the frame still teleports before the torso reaches the gate

Commit: `d3baeab3`. Locations: `Quake/vr/vr_portals.cpp:890`, `:972`, `:1102`, `:1211`.

**Author clarification during review:** stopping at the low sill until the player jumps is intended. The initial review incorrectly classified the no-jump result as a regression; that claim is withdrawn. The remaining defect is jumping against the frame, independently reproduced with ordinary collision enabled.

Reproduced with real collision, mock stick movement and the normal jump command in the original `start` map:

```text
map start; wait120; god; notarget; developer 1
setpos 211 1330 24 0 90 0; wait10; noclip 0; wait80
vr_mock_stick off 0 0.5; wait20; +jump; wait20; -jump
wait100; vr_mock_stick off 0 0
vr_portals_info
```

The player jumps from the actual floor. Before reaching the gate plane y=1384, the runtime prints:

```text
VR portal: carried edict 1 through side 0: 211 1367 8 -> 544 1536 28
```

That origin is about seventeen units short of the plane and the player's width overlaps the frame. The centre x=211 is inside the aperture x=208..256, but the collision box extends left of it. The point test accepts the midpoint; `reachOf` detects the obstruction; `d <= max(reach,0)+1` treats that obstruction as permission to teleport. The destination placement then falls back to the destination centre, producing the sideways snap as well.

A separate noclip-placement reproduction at `setpos 211 1372 24 0 90 0` also teleported twelve units short of the plane, but the physical jump above is the stronger evidence.

Walking without jumping stops at `(232,1367.969,-8)`, with torso midpoint z=-7, below the aperture's z=0. The engine claims the trigger and blocks the old teleport indefinitely. In light of the author's clarification, this is intended protection against entering without jumping, not an actionable finding.

The shipped crossing tests use `setpos`, which unconditionally enables noclip (`Quake/host_cmd.c:1823`), and never restore `noclip 0`. Their z=24 fixtures also put the player above the real floor. They validate neither physical approach to the sill nor the full collision box fitting through the opening.

**Correction:** do not treat obstruction by the frame as permission to cross; test the player's width rather than only its centre. Add physical centre-jump and frame-jump checks with noclip explicitly disabled. Preserve the intended requirement to jump over the sill, and the protection against leaning or brushing the frame.

Evidence: `build-cmake/slipgate-review/jumping.log` (`CASE-edge-jump`), `crossing.log`, `crossing-physical.log`, `crossing-sill.log`, `crossing-controls.log`.

## 2. [P1] The missing-self-leaf diagnosis is based on a broken BSP reader

Locations: `Misc/quakevr/pvs/bspvis.py:71`, `:89`, `:103`; `Misc/quakevr/pvs/FINDINGS.md:41`; the comments/help/checklist associated with `557232f3` / `477fc6bd`.

Quake PVS compression writes each nonzero byte literally. Only a zero byte starts a run of zero bytes. The probe instead treats every nonzero byte as a count and copies that many subsequent bytes. It also sizes rows using all 1,568 leaf records instead of the world model's 1,128 visibility leaves. `BSP.bits()` returns bit indices, despite callers treating them as leaf numbers.

An independent decoder matching the engine's format gives:

| Map | World visibility leaves | Leaves missing their own bit |
| --- | ---: | ---: |
| Original `pak0.pak` `start.bsp` | 1,128 | **0** |
| Shipped relit `id1/maps/start.bsp` | 1,128 | **0** |

Leaf 595 already includes bit 594 in both maps. Its correctly decoded PVS contains 187 leaves in the original and 190 in the relit version. The shipped probe reports 591 and a missing self bit for the original; that output is corrupt.

Therefore the added `Mod_LeafPVS` self bit is redundant for these maps. The published missing-self diagnosis and claimed explanation for the staircase are invalid. Pixel differences between sequential screenshots do not establish that geometry was restored: animated flames, lights and other time-dependent rendering also change between them.

The investigation also identifies the wrong brush. The hidden staircase cover is `func_bossgate`, model `*38`, bounds `(353,1601,-15)` to `(735,2015,-1)`. Model `*41` is an episode entrance blocker (`func_episodegate`) at `(289,1681,1)` to `(303,1775,95)`. On a fresh game it does not become a rendered brush: `QC/misc.qc:834` returns unless the relevant episode has been completed. It is not a world marksurface that a self bit can restore, as the later findings claim.

**Correction:** fix the decoder, bit numbering and world row length; retract the erroneous comments, menu help and completion claim; investigate the actual `func_bossgate` using controlled captures and its entity visibility. Remove the workaround rather than retaining a defensive override unsupported by these maps.

Reproduction: `python build-cmake/slipgate-review/verify-pvs.py`. That independent script reads the original extracted BSP and the relit BSP without changing either.

## 3. [P2] The server self bit names the next leaf

Commit: `557232f3`. Location: `Quake/sv_main.c:613`.

The new code sets bit `node - worldmodel->leafs`. Both client and server use bit **leaf-1**. The server's stored `ent->leafnums[]` are already biased: `SV_FindTouchedLeafs` assigns `leaf - sv.worldmodel->leafs - 1` (`Quake/world.c:466`). Reading `ent->leafnums` directly in `SV_EdictInPVS` and `SV_WriteEntitiesToClient` does not imply a different convention.

The existing `Mod_LeafPVS` result, including the new correctly indexed client bit, is already ORed into `fatpvs`. The additional raw leaf bit is therefore unnecessary and grants visibility to the next leaf. `SV_FatPVS` also feeds client visibility near turbulent surfaces, so this can cause both unnecessary entity transmission and extra geometry submission. Numeric neighbours in the leaf array are not guaranteed to be spatial neighbours. In the original `start` map, 269 world PVS rows exclude their next leaf; the new write can add such a leaf.

**Correction:** remove the additional server write. If a separate server safeguard is retained, use leaf-1 and explain why the bit from `Mod_LeafPVS` is insufficient. Correct the two-conventions explanation in the source and findings.

## 4. [P2] Bounding the camera distance breaks the portal's visual ray mapping

Commit: `ce1e1ed1`. Location: `Quake/vr/vr_portals.cpp:102`.

`carriedView` moves the virtual camera forward when stand-off exceeds 256 units, but leaves the eye's horizontal/vertical projection and screen-coordinate sampling unchanged. `LiquidPortal` samples `gl_FragCoord.xy`, while `VR_PortalTrace` and `carried` still use the exact rigid portal transform. Consequently the displayed distant scene and a shot through the same screen point follow different rays.

Concrete geometry from the reported position: head x=447, y=420; source gate centre x=544, plane y=1384; destination plane y=1536. A ray to the source centre has x/y slope `97/964`. The exact transported camera is `(447,572)`, so this ray reaches destination x=544. The clamped camera is `(447,1280)` and the same screen ray reaches destination x≈472.76 — a horizontal mismatch of **71.24 units** at the destination plane. This is a geometric/code finding; an actual aimed-shot capture of the discrepancy was not performed.

Runtime readbacks confirm the clamped position at the reported far spot. Both the initial far view and the far view after approaching the gate report the same camera `(447,1280,60)` and mean gate-box luma 8.0/255; the near view reports `(544,1492,60)` and 18.3/255. This test does not reproduce a history-dependent visibility change and does not establish that the author's original black-gate issue was caused by distance alone.

**Correction:** diagnose the missing/dark scene without changing the rigid mapping, or implement an explicit projection/texture remap consistent with the new camera. Check lateral views, both eyes and aiming through the gate.

Evidence: `build-cmake/slipgate-review/portal-views.log`, its screenshots and portal readbacks.

## 5. [P2] Turning torch knockoff off does not disable thrown-prop knockoff

Commit: `0ac23671` (implementation `5acf2412`). Location: `QC/vr_walltorch.qc:715`–`:735`.

`VR_WallTorch_Shot` checks `vr_walltorch_shot`, but the on-wall branch of `VR_WallTorch_Touch` calls `VR_WallTorch_KnockOff` without checking it. The setting's help describes both shooting and throwing, and says changes apply on the next map.

Reproduced even after setting `vr_walltorch_shot 0` **before** loading `vrfiringrange`:

```text
setpos -566 -760 40 0 180 0
vr_test_walltorch_shot 20
walltorch: knocked off its wall by vr_rock along 0 -1 0
```

The same disabled setting correctly lets the test hitscan shot pass through the torch to `func_wall`.

**Correction:** gate the on-wall thrown-contact branch using the same cvar, or provide an explicitly separate throwing setting and accurate help.

Evidence: `build-cmake/slipgate-review/torches.log`, `CASE-off-thrown` and `CASE-off-shot`.

## Checks completed and limits

- Release x64 engine build succeeded with the configured ClangCL toolset.
- QuakeC build succeeded, zero warnings. Static-storage, QC precedence and FGD coverage checks passed.
- Mock runs exited successfully after the test data setup was corrected to include mission-pack assets. An initial setup-only run failed to load `progs/playham.mdl`; that run was discarded.
- Torch knockoff passed the tested hitscan, nail, supernail, rocket, lightning, explosion and fast thrown-prop cases.
- The baseline laser diagnostic launched the monster's `enforcer_laser`, which missed the torch. The repaired wall-torch diagnostic uses the player's rifle missile behavior and passed (`final-torch-rifle-off.log`, `CASE-rifle-on`). Disabled thrown and hitscan cases also passed. The grenade fixture now waits through its fuse: it bounced away and exploded at (-380,-757,19), outside the torch's blast range, so that fixture is not positive proof of grenade knockoff. The direct explosion case passed in the original matrix.
- The master slipgate switch guards rendering, building, movement, traces and destination PVS. Same-map toggling disabled portal rendering/crossing and restored ordinary teleportation; turning it on rebuilt the sides and restored a through-gate trace. No defect found in `ad20f2c1`.
- `89ca5735` primarily shares the view-selection test with diagnostics; no separate selection regression identified.
- PVS picture inspection and far/near/far portal readbacks were performed. Raw scene luma is not the final headset appearance. The portal depth maximum can include the near-depth mask, so a maximum of 1 is not by itself proof of scene geometry.
- No real-headset, remote-client or all-map validation was performed. The staircase root cause was subsequently confirmed and fixed as described above.

Review harness, independent PVS verifier, logs and images are under the ignored `build-cmake/slipgate-review/` directory. The user's original game config and unrelated untracked files were preserved. Gameplay fixes are present in the working tree; no commit was made.
