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
| 12 | Pauldrons on the body (toggle, customisable), after the Quake ranger; improve the body with the same reference | L | done |
| 13 | Swimming: walk in shallow water (small penalty); deep or feet off the floor: slow stick (10%), strokes move you | L | done |
| 14 | Carrying and nudging physics items (ammo, health boxes): push with hands or weapons, hold and carry, no weapons while holding, no phasing through walls, improvised melee and throwing | XL | done |
| 15 | Knights' swords: a usable melee weapon dropped on death; their death frames without the sword | XL | done |
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

    - carried into a wall: the hand jumped 1.5, 2.5, 3.5 and 6 m ahead in single frames; the box stopped against
      the wall and stayed in the hand, never passing through.

    **Not tested:** the improvised punch, thrown damage on a monster.
    **Known limit:** a held box moves on the server's frames and is interpolated on the client, so it can trail the
    hand a little (as the grapple rope did); if it shows, the next step is to draw held boxes at the hand on the
    client.
12. **Pauldrons and the ranger's clothes.** The reference is the Quake ranger (`progs/player.mdl`, rendered from its
    skin and frames): a sleeveless olive vest, padded and laced down the front; a dark belt with a red-brown buckle;
    red-brown camouflage trousers with ridged olive plates on the thighs; tall dark boots; bare, muscular arms; and
    big quilted brown pauldrons. The colours are his skin's own palette entries.
    - **Pauldrons** (`Misc/quakevr/make_pauldron.py`, drawn by `setupPauldrons` in `vr_view.cpp`). Each shoulder
      has two parts, since a single rigid shell cannot both sit on the shoulder and wrap the arm:
      - a cap over the shoulder (`progs/vrpauldron.mdl`), carried by the clavicle and turning partly with the
        upper arm (`vr_body_pauldron_follow`, 0.35);
      - two lames round the top of the upper arm (`progs/vrpauldron_arm.mdl`), which follow it.

      Both are quilted leather plates with stitched seams, a light lower edge and rivets, closed with an inside
      and a rim. They are modelled in the body's bind pose about the left shoulder joint; the right side is the
      same model mirrored. The avatar now reports each shoulder's joint and the rotations its clavicle and upper
      arm apply to the bind pose (`avatar::shoulder`).
      - `vr_body_pauldrons` (1) toggles them.
      - `vr_body_pauldron_style`: 0 the ranger's leather, 1 the colour of the armour you wear (leather without
        armour), 2 steel.
      - `vr_body_pauldron_size` (1) sets the size; they also scale with the build.
      - `vr_body_pauldron_forward`, `_up` and `_out` offset them (metres).

      All are in Advanced > Body.
    - **The body** (`make_vrbody.py`):
      - **Skins:** the vest has lacing, side seams, padded belly folds, a chest seam and a back yoke. The belt and
        buckle sit at the hips, with the trousers below them. The trousers are red-brown camouflage, with the thigh
        plates in front. The armour plates (green, yellow, red) still cover the vest when worn, now from above the
        belt.
      - **Boots:** new shafts over the calves, from the ankle to below the knee, with a strap and a turned-down top.
      - **Geometry:** the torso is rounder (12 sides, was 8).
      - **Texture seams:** every loft's texture now wraps round once, with a duplicated seam vertex. Before, the
        last face of each ring squeezed the whole texture backwards. The engine welds normals by position, so
        shading is unchanged. The texture pad is 2 texels (was 5), so narrow blocks keep their detail.

    **Tested** in the mock with the body preview (`vr_body_debug 2`, `3`) and in first person:
    - the pauldrons sit on both shoulders; with an arm raised forward, the lames stay on it and the cap on the
      shoulder;
    - looking towards a shoulder shows its pauldron at the edge of view, leather and steel;
    - the body shows the vest, belt, trousers with thigh plates and boots.

    **To check on the headset:** whether the pauldrons are the right size and height for you, and whether the cap
    follows the arm enough when you aim.
15. **Knights' swords.** Quake's sword wielders are the knight and the hell knight (the death knight in Quake's
    code). Each now drops its sword when it dies, gibbed or not (not statues), with a chance of `vr_sword_drop`
    (1; Advanced > Melee). The sword is a new melee weapon, `WID_SWORD` (13; item `IID_SWORD`, 43): the hell
    knight's is the same weapon with the secondary-ammo weapon flag.
    - **Models** (`Misc/quakevr/make_swords.py`): `progs/v_ksword.mdl` (61 vertices) and `v_hksword.mdl` (9) are
      cut out of Quake VR's own knight models.
      - The swords' vertices were picked by hand, since Quake VR's knights are not id's; the script also handles
        id's models with `--id`.
      - Each blade is laid along +z with the pommel at the origin (vertex 0, the hand's anchor) and the tip at
        vertex 1 (the "muzzle", a swing's reach).
      - The blade is repainted as clean steel in the sword's own copy of the skin: the knights' bloody strip read
        as noise up close in the hand.
    - **Dead knights without the sword** (`vr_monstermods.cpp`): when the models load, the sword's own vertices in
      every death frame are collapsed onto the hilt, so its triangles vanish. The knight's death frames are known
      by index (Quake VR's model numbers its frames), the hell knight's by name. `developer 1` prints "the sword
      (N vertices) hidden in M death frames". Nothing changes on disk.
    - **In the hand:** weapon slots 19 and 20 (`vr_weapons.inc`) copy the axe's settings with the pommel as the
      anchor. The swing is the axe's with a longer reach (`W_SwordMelee`): 20 x `vr_sword_damage_mult` (1.5), the
      hell knight's 25% more. It hits with the knight's own sounds (`knight/sword1`, `sword2`) and sparks off
      walls. It can be holstered, thrown and force-grabbed like any weapon.
    - **Configs:** Ironwail archives every weapon slot's settings, so configs had slots 19 and 20's old placeholder
      values ("-1"), which hid the swords' settings. Fixed with `vr_wofs_version`: on the first run, slots whose
      defaults changed are reset to them once, and the version is saved.
    - **Precaches:** the swords' models and sounds are precached by `worldspawn`, so a sword carried into a map
      without knights still works. This also fixed the headshot tick (item 8): `misc/menu1.wav` was precached only
      in `main`, which the server never runs, so it did not play ("not precached").

    **Tested** in the mock:
    - e1m2: a knight shot dead lay without its sword, and the sword lay by its head;
    - a hand gripping it on the floor picked it up;
    - in the hand it is about 80 cm, held below the guard, next to the axe for scale;
    - e2m3: a hell knight, likewise.

    **Not tested:** how swings feel and their damage, on the headset.
