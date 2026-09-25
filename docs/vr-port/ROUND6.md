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
| 8 | Headshot detection: check rotations, fix; subtle headshot sound (toggle) | M | |
| 9 | A force-grabbed ammo pickup, not collected, fell through the floor (twice) | M | |
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
