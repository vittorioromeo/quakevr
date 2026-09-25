# Feedback round 11: plan and notes

The first round collected as voice notes (`vr_notes`; transcribed with `Misc/quakevr/transcribe_notes.py`).

| # | Note | Request | Status |
|---|---|---|---|
| 1 | e1m1 16-59-38 | Held objects run ahead of the hand when moving or turning with the stick | done |
| 2 | e1m1 16-57-08 | Backpacks taken by the trigger after a force grab; should go to a holster | done |
| 3 | e1m1 17-05-52, 17-09-03 | The force grab line aims above axes and backpacks | done |
| 4 | e1m1 17-10-16 | Gibs pushed into the floor when grabbed from above | done |
| 5 | vrfiringrange 16-54-59 | A two-handed shove also counts as two punches | done |
| 6 | end 17-14-55, 17-15-52 | Swimming: the recovery stroke cancels the stroke | done |
| 7 | e1m1 17-00-47 | Bump mapping doesn't show | done |
| 8 | e1m1 17-05-13 | The red button doesn't light its room | done |
| 9 | e3m5 17-19-22 | Monster projectiles should light up rooms | done |
| 10 | e1m1 17-01-47 | Ammo screens should give light; the shotgun's sights should glow | done |
| 11 | e4m8 17-17-21 | A chest flashlight (The Walking Dead VR style) | done |
| 12 | e4m8 17-18-08 | The wrist gadget should glow in its screen colour | done |
| 13 | vrfiringrange 16-53-59 | Console messages readable above the wrist gadget | done |
| 14 | vrfiringrange 16-55-43 | Floating damage numbers only readable against a background | done |
| 15 | e1m1 17-03-09, 17-03-47 | Legs: sidestep when turning, slower walk, a little thicker | done |
| 16 | r1m1 17-21-32, 17-22-32 | Sword hilt off-centre, out of style, see-through | done |
| 17 | e1m1 17-08-07, 17-08-47 | The dog's head leaves no blood trail; gib blood too small | done |
| 18 | e1m1 17-12-41 | GPU/CPU profiling | done |

## Notes

1. **Held objects** (`vr_held.cpp`): the client draws what your hands hold in the hand, every frame, at the
   hand's position and turn (the server says which entity each hand holds: `STAT_QVR_CARRYMAIN/OFF`), easing
   back to the server's position over 0.2 s when let go. The server's copy was measured running 2–8 units
   ahead or behind while walking. Other players see the server's position. `vr_carry_local` 0 compares.
2. **Backpacks** carry like boxes: gripped or caught, taken by letting go at a holster.
3. **Force grab aim:** the beam and the server's pull aim at the middle of the model as drawn (a
   `modelcentre` builtin), not the origin plus the model's height (wrong for a backpack's roll, the weapons'
   model offsets).
4. **Floor:** rigid-body traces started inside the player's (wide) box and missed the floor under objects
   near your feet; they now ignore players (hits on players and monsters are still found). Gibs become rigid
   bodies on their first landing and settle on their turned box (they sank up to 11 units before).
5. **Shove:** a two-handed shove (or a bash with a weapon held across) blocks the hands' punches for that
   motion: one shove, 8 damage. The bash also judged "forward" by the main hand's aim, so a weapon bash almost
   never triggered; it now uses the head.
6. **Swimming:** strokes drive you where you look (`vr_swim_look` 0.6 for sideways parts), a hand reaching
   forward again pushes only `vr_swim_recovery` (0.1), and you glide between strokes (`vr_swim_glide` 0.5).
   Stroke and recover moved 143 units instead of 5. `vr_swim_look 0; vr_swim_recovery 1; vr_swim_glide 0` is
   the old swimming. Treading water now means looking up and pushing down.
7. **Bumps:** round 10's bumps only shaded dynamic lights, and QRP textures' normal maps were built from
   image data the upload had already changed. Now they also shade the map's own light (a direction guessed
   from the lightmap's gradient, as DarkPlaces' fake deluxemaps; `vr_normalmap_baked`), deeper (dark
   textures more), and capped at 2 texels a unit so QRP's grain doesn't read as noise.
8. **Glowing panels** (relight): a glowing texture's light budget is shared per room, not per map; small
   glowing faces get their own light; strongly coloured glows get up to twice the light and reach. e1m1's red
   button now tints its room. The relit maps were re-made (both folders, water-vis kept).
9. **Projectile lights** (`vr_projectile_lights`): hell knight flames orange, scrag spit green, vore balls
   purple, lasers tinted; spike impacts flash. Unshadowed.
10. **Weapon glows:** ammo screens cast a small light in their colour (`vr_weapon_screen_light`); weapon
    glowing texels are boosted (`vr_weapon_glow`), so the shotgun's red sights glow and bloom.
11. **Chest flashlight** (`vr_flashlight`; `vr_flashlight.cpp`): a torch clipped to the chest on the off-hand
    side, pointing where the torso faces; trigger near it toggles; grip takes it into the hand; let go and it
    springs back (0.4 s). The beam: a pool of light where it lands, a spill light, a glow at the lamp;
    unshadowed unless `vr_flashlight_shadows`. `vr_flashlight_toggle` for a key.
12. **Gadget light** (`vr_gadget_light`): its screen's colour, faint, unshadowed.
13. **Wrist log** (`vr_notify_wrist`): console messages float over the gadget while its screen faces you
    (and not at the edge of the view).
14. **Floating numbers:** they were drawn before the sky, which painted over them; now drawn after.
15. **Legs:** step rate capped (`vr_body_step_rate` 2.2 steps a second running, was about 11); feet stay
    planted turning on the spot and step round past `vr_body_turn_step` (40°); legs and boots 15–20% thicker.
16. **Swords:** only the blade is kept from the knights' models (the knight's own guard was open where its
    hand covered it: the see-through part, and lopsided: the off-centre grip); a new hilt, crossguard, grip and
    pommel, low-poly, centred on the blade and textured from each knight's skin. Weapon settings reset once
    (`vr_wofs_version` 4).
17. **Gib blood:** heads without a trail flag in Quake VR's models (dog, fiend, shambler, zombie) now bleed;
    gib drops, splats and trail particles are about twice as big.
18. **Profiling** (`vr_profile`; TESTING.md "Profiling"): CPU scopes and GPU timer queries per pass, per eye,
    written to `quakevr/profile/*.csv` every 5 s with the graphics settings. Its first find: the wrist
    gadget's screen asked the driver for its framebuffer and viewport every frame (glGetIntegerv), which
    stalls the CPU on a threaded driver: about 0.45 ms. Now set instead of read: the mock's CPU time per frame
    went from about 0.7 to 0.26 ms. Bloom costs about 80% of the world's GPU time; worth a look with a real
    profile from the headset.

Also: voice notes trigger within 30 cm of the mouth and drop taps under 0.8 s.
