# Enemy perception and ranged attacks through teleporters

Enabled by default with `vr_portals_ai 1`. The toggle is **Graphics → Teleporters → Enemies See and Shoot Through**. Seamless teleporters (`vr_teleporters`, `vr_portals`, `vr_portals_walk`) must also be enabled.

Ranged enemies can acquire a player through one active aperture even when the ordinary room PVS excludes the player. Both room segments must be unobstructed. Range, facing and aim use the player position mapped into the enemy's room; prediction velocities rotate into that room too. Ordinary local sight takes priority. Notarget, invisibility, ordinary facing and range restrictions still apply. Player-only teleports do not provide enemy sight routes.

Perception works from the entrance and the virtual exit: enemies visible in the destination room can fire back through a one-way Quake teleport. An AI projectile retains its selected route until crossing, including this reverse route. Player and prop traversal retain their existing rules.

Hitscan attacks use the folded shot trace. Projectile attacks use mapped targets, and their movement rotates and translates at the gate. Lightning draws and damages separate segments in each room. Delayed wizard attacks retain their selected route. At the firing frame, an exact muzzle check cancels shots through a closed, blocked or missed aperture. Homing attacks update their aim across the gate. Original Quake, Hipnotic and Rogue ranged routines use these helpers, including custom boss attack loops and armed gremlins.

Under the stealth AI (`vr_stealth_gates`, STEALTH_PLAN.md "Teleporters") every monster also sees through paired gates, a melee one with a remote target runs through the gate to it, and Alert monsters walk through paired gates to what they saw or heard there. Otherwise melee attacks and navigation remain local. A remote target cannot enter the melee range class. An enemy with a visible remote target faces the opening and attacks without walking towards the player's physical coordinates in another room. This is one-hop perception, not navigation or recursive multi-gate targeting. Existing spread, fan attacks, grenade gravity and ballistic selection remain in effect, so a launched projectile can still miss or hit the edge of the opening.

## Validation

The mock reviews that checked this from a disposable game base (`portal_ai_review.ps1`,
`portal_ai_reverse_review.ps1` and `check_portal_ai.py`) were removed on 2026-10-09; they are in git history.
`Misc/quakevr/teleporters/teleporters_test.sh` (its `chase` case, in the release suite `Misc/release/run_test_suite.py`) covers monsters following through gates.

Those fixtures checked acquisition outside the ordinary PVS, folded range/facing, a muzzle outside the aperture, solid blockers in each room, actual hitscan/laser/lightning/wizard damage, delayed cancellation, feature disabling, native grunt attack callbacks without locomotion, and a 90-degree destination rotation. They use `vr_portals_ai_test` and `vr_portals_rebuild` only in the disposable game directory and never write the user's configuration. Mission-pack paths compile but are not each individually exercised by these fixtures. Headset visual testing remains useful for beam/tracer presentation.
