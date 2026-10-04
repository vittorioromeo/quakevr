# Latest commits, handoff, and portal follow-up review — 2026-10-04

Reviewed `vr-ironwail` at `cdc48f5f`, including the new gameplay change
`a7cbc990` and its measurement/checklist commits. The earlier review fixes are
present in `88c0f9fd` / `05d74fdf`. Fetch found no newer remote commits.

This is a review record. Production code was not changed. Diagnostic QC was
compiled in an ignored scratch copy, run in an isolated game directory, then
replaced there with the normal current progs. User configs and assets were not
used as test output locations. The runtime fixtures below test specific functions;
they do not replace headset acceptance of swing feel.

## New hitzones findings

### P2 — Apply positional damage before the zombie chainsaw threshold

`QC/vr_chainsaw.qc:430-431` first calls `VR_Zombie_SawHurt`, then multiplies
its answer by `VR_Melee_Positional`. The zombie helper accumulates ordinary ticks,
resets its sum when the required 25/60 damage is reached, and ensures the final
tick meets the knockdown/gib threshold. Reducing that final tick afterwards
invalidates the guarantee. Downed zombies reset their health to 60 in
`zombie_pain`, so this is not ordinary damage accumulating more slowly.

Runtime probe with default leg multiplier 0.6, enemy damage scale 1, 8-damage
ticks, and a downed zombie calling the actual helper and `T_DamageImpl`:

```text
REVIEW zombie tick 7 boosted 8 dealt 4.8 sum 56 health 60
REVIEW zombie tick 8 boosted 60 dealt 36 sum 0 health 60
REVIEW zombie tick 15 boosted 8 dealt 4.8 sum 56 health 60
REVIEW zombie tick 16 boosted 60 dealt 36 sum 0 health 60
```

The fixture uses the legacy leg classifier to isolate the ordering bug; a
model-mapped leg receives the same multiplier. This is a non-head cut, so it
does not depend on decapitation. Multiply each normal tick before accumulating
it; ensure the helper's final threshold damage is not reduced a second time.
Test standing knockdown and downed gibbing with leg/limb multipliers and enemy
damage scales below and above 1.

### P2 — Head priority compares the swing with an unanimated head sphere

`QC/weapons.qc:403` can replace the model-mapped body region with a head hit via
`VR_HeadZoneCrossing`. That helper (`292-326`) compares a world-space segment
against the standing head sphere, while `PositionalRegion` maps the contact
triangle back into the model's standing pose with `hitmodel_rest`.

The two classifiers disagree when the monster bends or moves its head. The
head-priority override can grant head damage to a torso contact whose incoming
segment never reaches the animated head.

Runtime probe: actual soldier model, exact zero-radius segment contacts, melee
tolerance 0, head priority on. For frame 8 at height 15 relative to its origin:

```text
contact -0.914551 0 15
rest 3.25684 -0.460938 7.9209
region 0 shot 1 melee 1.5
```

Other heights and frames reproduce it. This is not probe inflation. Preserve
the same model-space zone classification for both guns and melee. If priority
is intended to select among contacts along a swing, classify actual contacted
triangles in that same space; a standing sphere tested directly in world space
is not an animated head zone. Simply mapping arbitrary free-space segment ends
to the nearest triangle is also not a reliable swept-zone test.

### P2 — Blade contacts use the pommel's start point as their swing path

`QC/vr_melee.qc:1511` saves `mh_pp[0]` as the single remembered swing origin.
Every chosen contact, including a blade/tip contact, copies that point to
`mh_c_from` (`1616`, `1626`, `1661`). Point 0 is the sword pommel, axe handle
end, etc.; the contact may come from a different point's sweep. Joining them
creates a diagonal that no striking point followed. The 64-unit limit does
not make this path physically meaningful.

The helper probe demonstrates the consequence for a body contact: a horizontal
blade segment is not a head crossing, but changing only its start to an elevated
pommel position makes it a head crossing and changes its multiplier:

```text
REVIEW blade path head 0 pommel-to-blade chord head 1 blade mult 1 chord mult 1.5
```

This probe isolates the classifier; it is not a recorded real axe swing. The
incorrect start-point assignment is directly visible in the contact code.
Track the chosen striking point's path (or classify the actual swept contacts),
including curved paths. Reset history when tracking is lost, the held weapon
changes, or a new attack begins. The new `mh_sweep_live` flag is never cleared;
history currently restarts only through movement heuristics, including a
frame-dependent displacement threshold of 1 unit.

## Reported rendering and crossing issues

These are existing portal/CRT integration problems, not all introduced by the
new hitzones commit. The author confirms that portal surfaces and banner text
render in VR, but not on the flat screen; portals also fail in the spectator
camera.

### P2 — Flat-screen and spectator portal views are excluded

`portals::update()` and `stereo::renderPortal()` are scheduled in the eye render
loop (`Quake/vr/vr_stereo.cpp:716`, `751`). Portal frame data and texture access
also require an eye and explicitly reject the spectator
(`Quake/vr/vr_portals.cpp:737-758`). Removing only the rejection would reuse a
texture rendered from a different camera and cause projection/parallax errors.

Give each scene view a portal pass using that view's camera, projection,
viewport, and render target: both eyes, the ordinary flat-screen view, and the
spectator. Keep the exact rigid mapping used by movement and shots, the oblique
exit clip plane, and entity-list restoration. Draw a visible gate from the
spectator's position even when neither eye is looking at it. Respect the existing
off switches and avoid recursive portal rendering.

### P2 — CRT banner images are generated only inside the VR canvas path

`VR_End2D` returns when `drawingToCanvas` is false
(`Quake/vr/vr_panel.cpp:389-393`). The call that generates board textures is
after that return, through `gadget::renderScreen` → `text3d::renderScreens` →
`renderBoards`. Flat-screen scenes still lay out the boards and draw their
frames, but never generate their text images.

Reproduced with `vr_enabled 0` in `vrfiringrange`, at
`setpos -460 -672 17 0 180 0`. At `vr_worldtext_crt 1`, the soldier/ogre boards
are blank. Changing only to `vr_worldtext_crt 0` renders their labels. Images:

- `build-cmake/slipgate-review/quakevr/screenshots/vrfiringrange_2026-10-04_16-05-55.png`
- `build-cmake/slipgate-review/quakevr/screenshots/vrfiringrange_2026-10-04_16-05-56.png`

Generate world-board textures in a shared frame path independent of wrist
gadget/canvas activity, preserving GL state and avoiding duplicate work in VR.
Verify fresh flat-screen startup, page changes, map changes, and VR/flat mode
switching. Temporary workaround: `vr_worldtext_crt 0`.

### P2 — Destination torch lights are filtered against the player's source PVS

`VR_TorchLights` builds candidates from the ordinary view's leaf/PVS and physical
distance (`Quake/vr/vr_emissive.cpp:541-566`). Dynamic/taken torches must pass
that PVS test before they are considered (`641-647`). A thrown torch can already
be present on the client and visible in the portal scene but have no generated
dynamic light because its leaf is invisible from the source room. Crossing
changes the viewer's PVS and makes the light appear, matching the author's test.

The server already unions destination PVS into entity transmission
(`VR_PortalAddPVS`); do not misdiagnose this as another missing-self-bit problem.
Include visible portal destinations in torch light candidate selection and
rank by their contribution to the relevant view rather than physical distance
from the source room alone. Portal shadow preparation is also currently skipped
(`Quake/vr/vr_lighting.cpp:973-975`), reusing the main view's chosen lights/shadow
cache. Destination lighting needs appropriate shadow selection/data too.

Light transmission through a gate is a separate missing capability: nearby
source lights and flashlight cones are not transformed into lights affecting
the destination. A bounded one-hop virtual-light implementation should use the
same rigid transform, constrain light to the gate aperture, preserve folded
travel distance/falloff, and account for blockers on each side. Merely copying a
point light to the other room leaks around the gate and lets backing geometry
incorrectly block its shadow rays. Implement destination-side lights first,
then transmitted lights; profile actual GPU time before setting shadow budgets.

### Backed gates — recommended solutions without early frame teleportation

Requiring the torso to reach the surface exposes a map-geometry conflict: the
leading half of the collision box can hit a wall behind the surface before its
centre reaches it. The requirement to jump over a sill and clear the frame
remains intentional.

1. **Recommended: collision split at the portal plane.** While entering a valid,
   active gate, test the portion of the body before the plane against the source
   world and the transformed portion beyond it against the destination world.
   Backing geometry then cannot obstruct the half already through the portal,
   while the source frame/sill and destination obstructions still collide.
   Transfer the body when its torso crosses, retaining offset, velocity and
   heading. This is the most seamless option and requires a portal-aware sweep,
   not a global noclip exception or skipping whole brushes that contain a frame.
2. **Map-author fix:** provide sufficient clearance behind the portal surface
   for the body to straddle it. Simple for editable maps, but does not solve
   arbitrary stock/custom maps automatically.
3. **Compatibility fallback, only with an explicit mechanic decision:** transfer
   on leading-body contact after verifying the full aperture and destination
   clearance. This may feel smooth, but changes the author's torso-at-plane rule
   and can restore the short-of-plane behavior rejected in the earlier review.

Do not revive the timed vanilla bypass, destination-centre snap, or camera-only
stand-off clamp. Acceptance should cover backing walls, sill jumps, border
contacts, angled approaches, blocked exits, and the same cases in flat/VR modes.

## HANDOFF.md and QUEUE.md corrections for future sessions

Reviewed the actual files in `C:/OHWorkspace/qvr-kit`; no external file edits
were made. Their summaries of the decoder repair, removed camera clamp,
entity-list restoration, and intended sill jump are sound. Correct these points:

- **PVS indexing:** both server and client PVS bit `k` refer to BSP leaf `k+1`.
  The server stores `leaf - worldmodel->leafs - 1` in `ent->leafnums`
  (`Quake/world.c:466`), then indexes those already adjusted numbers directly in
  `sv_main.c`. The handoff's "server bit k = leaf k" is misleading and risks
  recreating the exact off-by-one bug removed by the earlier review.
- **Flat mode:** only the rendering is stereo-restricted. `walkOn()`,
  `VR_PortalClientCross`, and `VR_PortalHandles` do not require a chosen eye
  portal or active VR. Do not describe `vr_slipgates` as inert in flat mode or
  prescribe ordinary Quake crossing as its intended implementation. The author
  also reports successful central crossings there.
- **New requests supersede the flat-task gate:** `slipgate-flat` has now been
  requested explicitly; include the spectator camera in that work. The
  author's latest lighting report adds a concrete destination-torch bug to the
  existing `slipgate-lights` request. Add the banner regression and backed-gate
  collision decision to the open items.
- **Hit zones:** explain that contacts on the animated model map back to
  canonical zones in the standing pose. "Zones do not follow animation" is
  ambiguous. Do not label the new head-priority implementation correct/validated
  without addressing the three findings above. The stored dummy acceptance run
  has different hit counts for priority on/off, so it is not a clean paired
  replay proving only the classification changed.
- **Mock movement:** inability to move using `vr_mock_stick main` does not mean
  input-driven headless movement is impossible. The backend supports
  `vr_mock_stick off`; `vr_input.cpp:433` takes locomotion from that stick.
  Keep `noclip 0`, settle the map before `setpos`, and exercise actual collision.
- **Ship exit 4:** this means `git merge --ff-only` failed. Dirty changes that
  would be overwritten, divergent history, or conflicting untracked files can
  cause it; arbitrary unrelated dirty files do not necessarily prevent a
  fast-forward. Diagnose status and graph before choosing a repair. Do not
  automatically stash/commit the author's unrelated work.
- **Session state:** `82ebe9c5` was the recorded handoff state; current reviewed
  HEAD is `cdc48f5f`. Always verify branches/remote state at session start. The
  handoff recommends 5–8 concurrent workers while the queue says one at a time;
  make the current limit consistent. Retain risk-based review even when a ship
  smoke test passes; the new QC compiled despite these behavioral bugs.

## Validation

- Engine Release/x64 MSBuild: passed.
- Current production QC with shipping flags: passed, 0 warnings.
- Static-local, FGD coverage, QC precedence checks: passed.
- Repaired BSP/PVS unit suite: 5 tests passed.
- Isolated QC runtime probes: reproduced zombie threshold loss, animated
  head-priority disagreement, and the false pommel-to-blade chord.
- Fresh flat-screen CRT/plain banner screenshot comparison: reproduced.
- Portal/spectator gating and torch PVS filtering: direct code-path review,
  consistent with the author's observations. No GPU cost measurements or full
  hardware VR acceptance were performed in this review.

Probe and build logs: `build-cmake/slipgate-review/round2-*.log`.
