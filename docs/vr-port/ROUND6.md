# Feedback round 6: plan and notes

The requests of 2026-09-25, in the order they are worked on: quick fixes first, then the bigger systems. Each item
gets a status, what was done, how it was tested, and anything to check on the headset.

| # | Request | Size | Status |
|---|---|---|---|
| 1 | Swing sounds back for weapons; suppressed only for downward motions | S | done |
| 2 | The body (IK) invisible while dead | S | done |
| 3 | Floating physics items: splash only above a speed | S | done |
| 4 | Green armour on the body closer to the real armour's colour | S | done |
| 5 | Shoulder offsets, independent of the torso | S | done |
| 6 | Blob shadows for ammo and health pickups | S | done |
| 7 | Designer-placed pickups (weapons, keys, armour, powerups) float at torso height | S–M | |
| 8 | Headshot detection: check rotations, fix; subtle headshot sound (toggle) | M | done |
| 9 | A force-grabbed ammo pickup, not collected, fell through the floor (twice) | M | done |
| 10 | A small 3D screen behind the ammo counter on weapons (programmatic) | M | |
| 11 | Parrying enemy melee: weapon held sideways in front; damage reduction, sound, sparks, arm wobble; one hand may drop the weapon, two hands never | L | |
| 12 | Pauldrons on the body (toggle, customisable), after the Quake ranger; improve the body with the same reference | L | |
| 13 | Swimming: walk in shallow water (small penalty); deep or feet off the floor: slow stick (10%), strokes move you | L | |
| 14 | Carrying and nudging physics items (ammo, health boxes): push with hands or weapons, hold and carry, no weapons while holding, no phasing through walls, improvised melee and throwing | XL | |
| 15 | Knights' swords: a usable melee weapon dropped on death; their death frames without the sword | XL | |
| 16 | Credits and attributions for everything used (docs/vr-port/CREDITS.md) | S | done, kept up to date |

## Notes

(Filled in as each item is done.)

1. **Swing sounds.** Guns whoosh again (`client.qc` `PlayerVRMeleeImpl`), except when the hand moves mostly
   downwards (`dir_z < -0.6`): reaching for a holster. Fists, axe and Mjolnir always whoosh. A downward swing still
   hits only things that bleed.
2. **Body while dead.** `setupBody` (`vr_view.cpp`) shows the body only while health > 0 and not at intermission.
3. **Splashes.** `VR_AllowWaterSplash` (`vr_physics.cpp`): anything but a player splashes only when moving faster
   than `vr_water_splash_speed` (150 units/s), besides the existing 0.2 s limit.
4. **Armour colours.** `make_vrbody.py` takes its armour ramps from `progs/armor.mdl`'s own skins (palette
   entries 182-186 green-grey, 200-204 yellow, 70-79 red); skins 4-15 regenerated. The green is a muted grey-green
   now, as on the armour pickup.
5. **Shoulders.** `vr_body_shoulders_back`, `_up`, `_out` (metres) move the clavicles' roots along the chest's
   axes (`solveArm`, `vr_avatar.cpp`); Body page ("Shoulders Back/Up/Width"), VR Settings ("Shoulders Offset").
6. **Pickup shadows.** Ammo and health boxes are brush models (`maps/b_*.bsp`), which the blob shadows skipped. They
   now get blob shadows, cast real shadows from dynamic lights, and count as moving casters for map lights.
   To check on the headset: the map lights also cast your hands' and body's shadows (`vr_shadow_self 2`); near a
   light behind you these are large. Set 1 (body only) or 0 if it is too much.
7. **Floating pickups.** `VRFloatPickup` (`items.qc`, from `PlaceItem`): items that are not force-grabbable
   physics boxes rise `vr_item_float_height` (26 units, about torso height; next map), stop falling
   (`MOVETYPE_NONE`), and their box reaches down to the floor so walking over them still picks them up. Checked on
   e1m1: the green armour's origin went from 76 to 102, its box from the floor; the nailgun from 2 to 28.
8. **Headshots.** The old test put the head on the vertical axis through the origin and pushed the entry point a
   fixed depth into the box along the shot. That only worked for shots square to the box, and heads are not on the
   axis: they sit forward, 3 units for a grunt and 14 for a shambler, so a turned monster was measured wrong. Now
   (`PositionalDamage`, `weapons.qc`) each target has a head sphere in its own frame (`PositionalHead`: forward, up,
   radius; measured on the models' standing frames); the shot is a ray, and it is a headshot if its closest
   approach to the sphere is within the radius. The same ray finds the body (its closest approach to the vertical
   axis): extremities and legs as before. Zombies, scrags and mummies have heads now.
   **Sound:** `vr_headshot_sound` (0.5, volume; 0 off; Advanced > Gameplay): a quiet `misc/menu1.wav` tick for
   the shooter, at most once a frame (shotgun pellets).
   **Tested** in the mock, shotgun on a grunt from 60 units, hand pitch swept: one volley passed 1.4-2.0 units from
   the head's centre (headshot), another 10.5-11.4 units (body).
   **For tests:** `impulse 150 + weapon id` puts a loaded weapon in the main hand, `170 + id` in the off hand
   (single player; hold `+grabright` or `+grableft` in the mock, since a hand that is not gripping drops its weapon).
9. **Force-grabbed box through the floor.** Reproduced in the mock: pull the shells box at e1m1 (672, -40), do not
   grip; it ended at z = -1660, still falling. Cause: ammo and health boxes are small (6 units at
   `vr_forcegrabbable_box_scale` 0.25), and Quake clips anything over 3 units with the *player* hull (32 x 32 x 56).
   The flight ends at the hand; near a wall or a low ceiling that tall box is buried, and Quake lets a trace that
   starts and ends in solid through, so it falls forever. The miss handler meant to put it somewhere it fits, but
   searched from a place where that box did not fit either and then gave up. Fixes:
   - `VR_Forcegrab_Miss` starts its search from the item's box laid on the player's own (which fits), and else puts
     it back where it flew from (`fg_origin`).
   - Engine safety net (`keepInWorld`, `vr_rigid.cpp`, from `SV_Physics_Toss`): every item (`FL_ITEM`,
     force-grabbable) and rigid body remembers the last place it was free; one found buried while moving (rigid
     bodies: centre inside solid, since they collide by their corners from it) goes back there, still.
     `developer 1` prints "buried". Items resting buried (some map boxes touch a low ceiling with that tall hull)
     are left alone: Quake does not move grounded items.
   Tested three times: the box flies, is missed, and ends at rest (z 51), not out of the level.
