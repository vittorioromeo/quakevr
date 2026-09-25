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
| 10 | A small 3D screen behind the ammo counter on weapons (programmatic) | M | done |
| 11 | Parrying enemy melee: weapon held sideways in front; damage reduction, sound, sparks, arm wobble; one hand may drop the weapon, two hands never | L | done |
| 12 | Pauldrons on the body (toggle, customisable), after the Quake ranger; improve the body with the same reference | L | |
| 13 | Swimming: walk in shallow water (small penalty); deep or feet off the floor: slow stick (10%), strokes move you | L | done |
| 14 | Carrying and nudging physics items (ammo, health boxes): push with hands or weapons, hold and carry, no weapons while holding, no phasing through walls, improvised melee and throwing | XL | done |
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
10. **Ammo screen.** `text3d::queue(..., screen)` (`vr_text3d.cpp`): behind a weapon's ammo counter, a dark bezel
    box (shaded per face) and a lit screen face in the wrist gadget's palette (`vr_gadget_screen_hue`,
    `_brightness`, `_background`); the text is tinted the gadget's text colour. All geometry is made in code, sized
    to the text (`vr_weapon_screen_padding`); no model changed. `vr_weapon_screen` (1) toggles it; menu: Advanced
    > HUD, "Weapon Ammo Screen", "Ammo Screen Margin". The font's cutout shade now multiplies by the vertex colour
    (white elsewhere, so unchanged). Checked on the shotgun: a small green screen on its back, "8/8" over "100".
11. **Parrying.** `VR_Parry` (`combat.qc`, in `T_Damage`): a blow from a melee monster itself (knight, hell knight,
    ogre, fiend, dog, shambler up close, fish, gremlin, mummy; the inflictor is the attacker, within 150 units) on a
    VR player is parried by a hand holding a weapon (not the fist) that is:
    - square to the blow within `vr_parry_angle` (50 degrees);
    - ahead of the body towards the attacker (2-44 units);
    - between the hips and above the head.

    A parry takes `vr_parry_reduction` (0.75) off the damage, clangs (`player/axhit2` + `weapons/tink1`), throws
    sparks from the weapon, gives a strong haptic, and knocks the hand. The knock is a new
    `QVR_SVC_HANDIMPACT` message (`handimpact` builtin): the client offsets the drawn hand, and with it the weapon and
    the IK arm, along the blow with a shaking, decaying spring (`vr_parry_wobble`); the tracked hand the game uses is
    untouched. With two hands on the weapon (2H aiming) both hands are knocked and it is never dropped. With one,
    `vr_parry_drop_chance` (0.5) knocks it out of the hand. The v_forward/right/up globals are restored for the
    monster's code. Menu: Advanced > Melee.

    **Tested** in the mock: a dog biting a player holding the shotgun sideways, three bites parried; with the drop
    chance at 1, the gun fell. Not tested: the two-handed case (the mock cannot hold a weapon two-handed), and how
    the wobble looks; please check both.
13. **Swimming** (`vr_swim`, `vr_physics.cpp`, hooks in `SV_ClientThink`). The stick's speed in water is scaled:
    - feet in water: `vr_swim_shallow_speed` (0.85);
    - waist deep standing on the bottom: `vr_swim_wade_speed` (0.6);
    - under water or off the bottom: `vr_swim_stick_speed` (0.1).

    Strokes (`VR_AfterWaterMove`, after Quake's `SV_WaterMove`): each hand under water faster than
    `vr_swim_stroke_min` (0.4 m/s, relative to the body) pushes the body the opposite way. The push is
    (speed - threshold) x `vr_swim_stroke` (10/s), scaled by how squarely the palm meets the water
    (`vr_swim_palm`, 0.6: a hand slicing edge-first on the recovery pushes 40%). It is capped at
    `vr_swim_max_speed` (400). Quake's water friction slows you between strokes. The direction is the stroke's: pull
    down to rise, back to go forward, sideways to turn aside. Menu: Advanced > Locomotion.

    **Tested** in e1m1's pool (720, 900; submerged, on the bottom): two scripted breaststroke pulls (palm back) with
    slow edge-on recoveries moved the player 34 units forward (about 0.65 m a stroke); with the strength at 0, not
    at all. Along the way:
    - the move's hand velocities turned out to be in m/s, not world units (comment fixed);
    - the mock backend now reports velocities for hands moved by `vr_mock_hand`, measured between moves and held
      40 ms, so strokes, throws and swings can be scripted.

    To tune on the headset: stroke strength and threshold, and whether the palm weighting feels right.
14. **Carrying and nudging boxes** (`QC/vr_carry.qc`, `vr_carry`). Ammo and health boxes (the force-grabbable
    ones) are rigid bodies now, and their hand touch (`handtouch`) and gun touch (`vr_wpntouch`) go to carrying.
    The box's own pickup is kept as `carry_use`.
    - **Nudge:** a hand or a gun touching a box without gripping pushes it: along the hand's motion it takes up at
      least the hand's speed (`vr_carry_nudge`).
    - **Hold:** gripping a box holds it, keeping where it was in the hand's frame, and turning with the hand. Each
      frame it moves along a line from where it is to where the hand wants it, stopping at walls and monsters, so
      it never passes through them. Left 32 units behind (stuck), it drops. The box's own think (water floating,
      respawn repositioning) is paused while held.
    - **Take:** the trigger while holding runs the box's pickup (the ammo or health); if full up, it stays held.
      With `vr_carry 0`, or body interactions, a touch takes it as before.
    - **No weapons while holding:** unholstering, weapons lying about, catching thrown weapons and force grab are
      all refused for that hand.
    - **Improvised melee:** punches with a box in hand hurt `vr_carry_melee_mult` (1.5) more.
    - **Throw:** letting go leaves it with the hand's throw estimate (the same as thrown weapons), as a rigid body
      with the hand's spin. Over 250 units/s it hurts what bleeds (`vr_carry_throw_damage` 25, scaled by speed as
      thrown weapons are), not the thrower.

    Menu: Advanced > Throwing and Physics > Carrying Boxes.

    **Tested** in the mock on e1m1's shells box:
    - a hand sweeping through it pushed it 18 units its way;
    - gripping it held it, and it followed the hand up and back;
    - the trigger took it (shells 25 -> 45, the box gone);
    - a forward swing and release threw it at 246 units/s forward.

    **Not tested:** carrying into a wall at speed, the improvised punch, thrown damage on a monster.
    **Known limit:** a held box moves on the server's frames and is interpolated on the client, so it can trail the
    hand a little (as the grapple rope did); if it shows, the next step is to draw held boxes at the hand on the
    client.
