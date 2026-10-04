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


## Fixes applied after the review

All actionable findings above are addressed in the engine/QC changes from this
session. The original findings remain above as the review record.

- Melee classifies the actual model contact after mapping it to the standing
  pose, as shots do. Head priority resolves overlap at that contact; it does not
  search a historical swing or the pommel-to-blade chord. The chainsaw applies
  positional damage before the downed-zombie gib threshold helper.
- Each headset eye, flat view and spectator camera selects and renders its own
  portal after camera setup. The exact rigid mapping is unchanged. Flat portal
  scenes always resolve to their colour target, and sampling accounts for the
  viewport, view size and reduced render resolution. Flat CRT banners execute
  the screen text pass.
- A straddling player collides as two clipped body portions in the respective
  rooms. Source backing geometry no longer blocks the portion already through;
  frame, sill and destination obstacles still collide. Torso entry transfers
  the exact position, heading and velocity, with no centre/forward snap. The
  split persists until the trailing portion clears; backing out uses the inverse
  mapping and does not fire trigger targets twice. Map changes reset crossing
  state. The intended sill jump remains required.
- Destination flame/torch candidates use owned destination PVS rows and folded
  camera distance, so their light is selected before the player crosses.
  Dynamic/spot light transmission is one traversal, bounded to two virtual
  contributors per view, with paired entry/exit cube shadows (tiles at most
  256 pixels). Aperture and source/exit obstacles clip the light path. Native
  dynamic-light slots are retained; virtual contributors do not recurse.
- Graphics > Slipgates has Portal Stars Size and Opacity. Size defaults to
  1.12x (range 1..2); opacity defaults to 1 (0..1), scaling the original 12%
  entrance shimmer. Following the author's clarification, there is one star
  layer on the entrance plane and no teleport/star quads in the destination
  render. The draft Destination Stars sliders/cvars were removed. Teleports
  use the original BSP surface vertices, separately from the liquid wave mesh;
  both required attributes are enabled on the liquid surface draw pass. Visual
  scale does not enlarge the collision aperture.
- Teleport-textured BSP liquid leaves are treated as empty for liquid interaction.
  Explicit splash/ripple/sound paths also reject slipgate faces. Ordinary pools
  retain their liquid contents, effects and sounds. Portal surfaces already omit
  the liquid ripple/foam shader paths. This applies with VR enabled or disabled.
- The offline reader also now uses Quake's 40-byte texinfo records (not 48).
  Its self-bit decoder repair remains intact; `BSP.bits()` returns BSP world
  leaf numbers. Both client/server PVS bit k name BSP leaf k+1.

### Verification of the fixes

Release/x64 engine build and shipping QC compilation passed; QC reported zero
warnings. Static-local and QC precedence checks passed. BSP/PVS suite: six tests
passed, including a two-record texinfo fixture. Isolated engine runs exited 0.

Physical `start` tests with `noclip 0` verified both blocked frame edges (x211,
x253), offset retention (x220 -> x532, x244 -> x556), all three skill gates,
and torso entry at y1384. A blocked destination entity stopped the source player
at y1376 before entry. An immediate reversal returned from (544,1536,28) to
(232,1384,24). The no-jump sill case remains blocked intentionally.

The maintained isolated QC fixture in `Misc/quakevr/scratch/` verified a downed
zombie gibs on tick 13 with an x0.6 leg multiplier, body contact stays x1, and
shot/melee classification disagrees at zero sampled real contacts across 102
soldier frames and heights 13..33. Gate point contents report empty; forced gate
splashes produce no particle/sound event. A real-water control reports water and
produces splash plus `vr/plip2.wav` at volume 0.60.

Flat screenshots show readable CRT and plain banner text. A spectator-camera
window screenshot shows the destination rather than a starry solid surface.
Flat portal tests also exercise r_scale 2 and viewsize 80. Gate-region mean
brightness was 20.4 baseline, 36.9 with a transmitted point light, 39.6 with an
injected destination flame, and 41.8 with that flame plus a source spotlight.

One RTX 4090 flat fixture reported 39 shadow faces / 296 model draws, GPU 0.13 ms,
CPU 0.20 ms for the measured pass (baseline CPU 0.07 ms). This is a limited
per-pass measurement, not a hardware VR performance acceptance. Full Quest 3
acceptance after the final composition correction, many simultaneous lights,
varied custom gate angles and subjective size/opacity tuning remain in the in-game checklist. Non-cardinal turns use a
conservative axis-aligned destination body bound and can block narrow exits.

Logs and screenshots: `build-cmake/slipgate-review/fixes-*.log`, `portalshots/`
and `screenshots/` under its isolated quakevr game. Reproduction commands are
in TESTING.md. External handoff corrections are tracked separately from Git;
verify their live branch/agent state at the next session.


### Author feedback during implementation

The author correctly reported that the first composed screenshots showed the
entrance backing wall, despite valid destination FBO readbacks. The added size
pivot attribute was initially enabled on the wrong draw pass; additionally,
the geometric liquid mesh also contained teleport faces and supplied no pivot.
The 1.12x scale therefore moved those faces about the world origin. The final
change enables the attribute on the proper pass and routes teleport surfaces
through their BSP vertices even when the wave mesh is active. Acceptance must
inspect the final window and both eye images, not just a valid portal FBO.

The author then clarified that exactly one set of stars should sit on the
portal plane. Destination star quads are suppressed rather than separately
adjustable. This supersedes the earlier request for four sliders.


Final composition checks after both corrections: flat opacity 1/0, r_scale 2 /
viewsize 80, liquid mesh on/off, both mock headset eyes, and the spectator
window all visibly show the destination's two doorways and torch. At opacity 0,
only the destination is visible; at 1, one star layer sits over it. The author
provided a reference image of the same destination to compare. Final captures:
`start_2026-10-04_18-00-56*.png`, `eyeshots/start_000_{L,R}.png` and
`start_2026-10-04_18-02-54.png` in the isolated game's capture directories.

### Multiple visible slipgates

The author's simultaneous-gate report exposed the original one-view budget.
Each camera now renders up to four visible gates by default, configurable from
one to eight through Graphics > Slipgates > Visible Gates (`vr_portals_maxviews`).
Each gate has its own texture-array layer; offscreen gates do not consume the
budget. Shadows are prepared independently for each destination view. Server
entity visibility includes all directly visible nearby gate destinations and
tests the original source PVS, avoiding recursive visibility expansion or an
arbitrary first-eight ordering that could omit a rendered destination's entities.

The maintained `make_portal_views.py` fixture places three gates side by side
with distinct rooms containing one, two and three columns. Final composed flat,
both mock-eye and spectator captures show the distinct destinations. Live changes
from limit 4 to 1 and then 8 render three, one and three views respectively.
Headset hardware performance with several gate/shadow passes remains an
acceptance item; the limit provides a direct performance control.

The final maintained QC probe also passed: zero animated-contact disagreements,
body multiplier 1, reduced leg chainsaw hits gib the downed zombie on tick 13,
no gate splash/sound and an intact real-water splash/sound control.

Final multi-view production captures: `portalviews_2026-10-04_18-37-22*.png`
(flat limits 4/1/8 and entrance stars), `portalviews_2026-10-04_18-35-57.png`
(spectator), `eyeshots/portalviews_000_{L,R}.png` (both eyes). Each of the three
destination rooms also contains a visible flame entity. Final production
`start` movement still transfers to (544,1542,28) through the backed skill gate.
Release/x64 MSBuild and final static/precedence/diff checks passed.
