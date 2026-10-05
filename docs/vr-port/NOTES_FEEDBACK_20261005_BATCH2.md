# October 5 follow-up voice notes and slipgate reach

Transcribed 27 new recordings with `Misc/quakevr/transcribe_notes.py --device cpu`; the ignored local transcript now contains 37 recordings. The checked checklist entries without spoken comments remain accepted as verified. No player configuration was overwritten.

## Authorized slipgate reach

Hands, held guns, alias props and external brush props render as two copies clipped at the aperture. Both alias instances share the current animation and skeletal pose. The original half stays in its room; the transformed half appears in the destination. Own models can reach fully beyond the plane while the player's body remains in the source room. Inline map geometry is not duplicated.

The server keeps raw tracking coordinates and maps hand positions, muzzle positions, orientations and linear/angular throw motion into the reached room. Hand/world obstruction traces check both halves. Gun touches follow the folded reach rather than sweeping across the distance between rooms. Carried props change coordinate frames when the grip crosses, including a crossing on the release frame; withdrawal reverses that mapping. Two-hand solves use a common coordinate frame for both grips. Apertures must be active and admit props, with a clear path from the player's room.

Physical hand/weapon and prop shapes get a temporary collision copy in the other room while straddling. Contact points outside each half are discarded. Dynamic copies return their contact velocity changes to the original body; kinematic copies can push props in the far room. Carried-object level checks clip the convex bounds and query each room independently. Loose rigid props transfer at their centre crossing without the old placement jump. Debug commands: `vr_portals_reachtest`, `vr_physics_portals`.

This is one-aperture reach. Dynamic collision copies exchange impulses but do not share a persistent solver contact cache across rooms. Shadow-depth passes still use the original model. Headless checks establish coordinate mapping, pose sharing and copy creation, not headset perception or every contact arrangement.

## Other feedback addressed

- Gadget hologram and recording text: prevent portal subviews from choosing the gadget's main-view layout or drawing local recording overlays; rebuild text facing and billboard geometry for each camera. This addresses the yaw-dependent disappearance and mirrored timer caused by the first portal view's cached geometry.
- Retro Textures: add **Liquids** for water, slime and lava, with the existing texture-picker path identifying that category. Teleport textures retain their previous category.
- Wall torches: a grip takes the torch immediately; remove the obsolete Pull to Take menu slider. Melee damage now knocks them off. Blood helpers recognize a torch after it becomes a loose prop too. Enemy lasers use missile collision, so they meet mounted shot targets. Lightning, nails, lasers and melee knock down a torch without blood. A grenade contact knocks it down through both supported contact paths and keeps the original fuse.
- Small gibs: melee fragments follow the actual swing direction, including upward/downward motion, with modest spread. Other damage retains its existing outward scatter.
- Held-object blood: project the splatter contact into the striking object's bounds, then place its source past the outgoing face. This handles contact points outside the prop as well as inside it, so the face that hit takes blood.
- Heavy props: include held weapon reach shapes in solid-prop collision filters. Guns and crowbars can push crates and explosive boxes as carried props already did.
- Defaults: low-poly flashlight chain; handheld torch motion setting 3 from the current config; burning another hand enabled. These values already match the current local config.
- Nonblocking corpses: gentle, capped impulses along the walking direction when a living player's body overlaps their parts. Includes controller movement and physical room-scale displacement, with large teleport jumps excluded. Existing blocking/damage rules remain unchanged.
- Lightning on bodies: short arcs for 0.75 seconds over the currently posed enemy/corpse mesh, using the existing glowing arc renderer. Consecutive hits extend one effect on that entity rather than stacking copies.
- Intermittent force grabs after many map loads: invalidate the physics world by map generation as well as model pointer. A reused map-model address no longer preserves the old bodies; stale ragdoll lookups are rejected before rebuilding. Repeated loads verify the reset. The intermittent headset failure itself was not reproduced, so this remains a plausible underlying fix requiring play confirmation.

## AI seeing and shooting through slipgates: research only

The current AI uses ordinary room coordinates for visibility (`QC/ai.qc`: `visible`, `infront`, `ai_run`), attack clearance and range (`QC/fight.qc`: `CheckAttack`), and each monster's aim direction. Portal-aware projectiles alone cannot correct those decisions.

A practical first scope is one-hop ranged perception: expose a mapped image of the player in the monster's room, check visibility through the aperture and both room segments, use folded range and yaw, then aim at that image. Hitscan attacks need the existing portal trace flags. Projectile launch paths need to use the same mapped aim point; recheck the aperture when the shot actually fires so closing gates cancel invalid shots. Enemy acquisition also needs the portal visibility path, not just an adjustment after the enemy has already been selected.

This is a moderate change across perception plus the monster-specific attack routines, including mission packs. It is much smaller than full portal-aware navigation. Keep melee decisions local and discuss whether enemies should walk through gates separately; otherwise an enemy able to see a portal image may still try walking towards its physical target. No AI behavior has been changed in this patch.

## Verification

Release engine build succeeds and QuakeC compiles with zero warnings. Static checks and `git diff --check` pass.

- `reach_review.ps1` / `check_reach_feedback.py`: zero mapping error, split traces, two animated hand instances with 33 bone poses, straddling prop collision copy, carry across/back/release, and three map-generation resets.
- `torch_hits_review.ps1` / `check_reach_feedback.py`: real lightning, enforcer laser, nail, pellet, close grenade contact, melee damage route and immediate grip. All detach the torch without blood; the grenade remains alive one second later, before its fuse.
- `body_shock_review.ps1` / `check_reach_feedback.py`: lightning on a surviving grunt and a corpse uses 810 posed triangles and expires after 0.75 seconds. `vr_shock_info` exposes active body effects.
- `notes_oct5_review.ps1` / `check_notes_oct5.py`: room-scale portal traversal, force-grab selection/crossing/catch/cancel, blunt head/body outcomes, small-gib button exclusion, gremlin corpse interaction.
- `notes_feedback_review.ps1` / `check_notes_feedback.py`: flashlight saves, actual nail death, ragdoll capacity/replacement, drowning, struggling controls, weapon pickups and shortened upward torch flame.

Appearance of the hologram, blood coverage, body arcs, corpse nudge strength and split models still benefits from headset review. The tested Release executable and matching QuakeC are installed in the usual local paths.
