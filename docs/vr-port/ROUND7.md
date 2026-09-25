# Feedback round 7: plan and notes

| # | Request | Status |
|---|---|---|
| 1 | Commit and push everything | done (`origin/vr-ironwail` at 387466c1) |
| 2 | Lighting: rooms light up when shooting, glowing textures (buttons, lights) glow, darker ambient relying on light sources | |
| 3 | Blob shadows named as such; easy to turn off when real shadows are on | done |
| 4 | Ammo/health boxes: grab box much larger than the model; their real shadow much bigger than the model | done |
| 5 | Boxes consumed by releasing them at a holster (default); the trigger as an option | done |
| 6 | Designer-placed weapons: check their hit box | checked |
| 7 | Push-back (tweakable) on parries and melee hits, for the player and enemies | done |
| 8 | Swimming: stick penalty 20%; strokes stronger towards where the stick points, weaker against it | done |
| 9 | Knights' swords: handle, the knight's own texture, held by the handle and turning about it | done |
| 10 | Headshot sound not heard; the "Gameplay" menu missing | done |
| 11 | Gameplay menu: damage multipliers (player to enemies, enemies to player, headshots, ...) | done |
| 12 | Headbutt from the old port | done (reworked) |
| 13 | Immersion, VR interactions, "juice" | |

## Notes

(Filled in as each item is done.)

- **Changed defaults now reach existing configs.** Ironwail saves every archived setting, so a new default never
  reached anyone who had played before. `vr_migrate_config` runs right after the saved config (queued by `exec`):
  each changed default is listed with its old value in `vr_cvars.cpp`, and a config still holding the old value
  takes the new one, once (`vr_cfg_version` records it). Settings you changed yourself are kept. Tested with a
  config holding the old swimming stick speed: it became the new one.

3. **Blob shadows.** The menu says so now: Graphics > "Blob Shadows" (Off / Always / Auto), "Blob Shadows: You"
   (hands, body), "Blob Shadows: Things" (monsters and items). Auto, the default (`vr_blob_shadows` 2), draws them
   only where real shadows do not fall: none under monsters and items while the map lights cast moving things'
   shadows (`vr_shadow_maplights` > 0), none under you while they also cast yours (`vr_shadow_self` > 0).
4. **Boxes' grab box and shadow.**
   - **Grabbing:** hands were tested against the item's abs box, which Quake widens by 15 units on each side for
     items so that walking over them picks them up: a 6-unit box was grabbable from about 20 units away. Hands now
     test the entity's own box (`handTouch`, `vr_physics.cpp`), as the other hand-touch path already did.
     Tested on e1m1's shells box with force grab off: a hand beside the box takes it; one 0.6 m to the side no
     longer does.
   - **Shadow:** the shadow pass drew brush entities without the networked scale (boxes are drawn at a quarter
     of their size) and with the pitch uninverted (`vr_lighting.cpp`); it now draws them as they are rendered.
     Checked with a test light: the box's shadow is the box's size.
5. **Taking a carried box** (`vr_carry_take`; Throwing and Physics > Carrying Boxes > "Take a Box"): 0 (default)
   let go of it at a holster (hips, shoulders, upper holsters) and it goes into your pack; 1 the trigger (as
   before); 2 either. Full up, a box let go at a holster just drops there. Tested: carried to the right hip and
   to the right upper holster, let go: shells 25 -> 45.
6. **Designer-placed weapons.** Their box is the model's (square around the origin, as they turn), reaching
   down to the floor since they float. Checked with `r_showbboxes 1` and by taking e1m1's super shotgun with the
   hand at the model. Since grabbing now uses the true box rather than the 15-unit-wider one, a hand near but off
   the model no longer takes it; if one is still hard to take, `r_showbboxes 1` shows its box: tell me which.
7. **Push-back** (`VR_Push`, `combat.qc`; Gameplay > Push-back):
   - your melee blows and headbutts push what they hit, alive or dead (`vr_melee_push`);
   - monsters' unparried melee blows throw you back (`vr_melee_push_player`);
   - a parry pushes the attacker back and you a little (`vr_parry_push_enemy`, `vr_parry_push_player`).

   Walking monsters are thrown (lifted a little); flying and swimming ones are stepped aside. Monsters bigger
   than a grunt move less, smaller ones more. Checked that the pushes are applied (a grunt hit by the sword:
   about 660 units/s); how far things fly is for you to judge.
8. **Swimming.** The stick under water is 20% (`vr_swim_stick_speed` 0.2; your saved 0.1 is updated). Strokes
   push up to 50% more towards where the stick points and as much less against it (`vr_swim_stroke_assist`,
   Locomotion > Stroke Steering), so the stick steers the swim and a stray hand motion does not throw you back.
9. **Knights' swords remade** (`Misc/quakevr/make_swords.py`):
   - **Texture:** the knight's own skin again, unpainted (it was repainted as steel), so the sword looks as the
     knights carry it.
   - **Handle:** the knights' hands cover their grips, so the models have none. A leather grip and a pommel are
     added, and the hell knight's bare blade gets a crossguard.
   - **Held by the handle:** the sword is laid where the axe's handle is in `v_axe.mdl`: the grip along the
     handle's axis, the guard where the head starts, the blade continuing with its edges the way the axe's head
     points. Weapon slots 19 and 20 are the axe's settings. The hand holds the grip as it holds the axe's handle,
     and the sword turns about it.
   - **What was wrong:** weapon anchor indices are not a model's vertex numbers but the old engine's strip order,
     so "vertex 0" and "vertex 1" were two points by the guard: the hand was placed there and swings reached from
     there.
   - **Making weapon settings:** new commands print a model's anchors: `vr_anchor_info <model> <index>` and
     `vr_anchor_nearest <model> <x> <y> <z>` (model coordinates).
   - **Scaling:** the engine scales weapon models about their file's bounds corner, not their origin, so the
     slots' offsets correct for the swords' different bounds.
   - **Configs:** your config saved the first swords' settings; `vr_wofs_version` 2 resets slots 19 and 20 once
     more.

   Checked in the mock: the hand wraps the grip under the guard, where it holds the axe.
10. **Headshot sound and the Gameplay page.** The tick was `misc/menu1.wav`, a faint menu click lost under
    gunfire. It is now a generated crack over a thump (`sound/vr/headshot.wav`, `Misc/quakevr/make_sounds.py`).
    There was no Gameplay page (the docs were wrong: the setting was under Immersion); now there is one, under
    Advanced VR Options.
11. **Gameplay page:** damage to enemies, to you, self damage, melee damage; positional damage with headshot, arm
    and leg multipliers (`vr_headshot_mult` 1.5, `vr_limbshot_mult` 0.35, `vr_legshot_mult` 0.6) and the headshot
    sound; push-back; headbutt; the knights' swords.
12. **Headbutt.** It was there (the old port's code), but it needed a head speed of 2 m/s, traced from the chest,
    counted any fast head motion (a quick look round), and played a sword swoosh whenever the head moved fast.
    Now (`vr_headbutt`, `vr_headbutt_speed` 1.5 m/s, `vr_headbutt_damage` 32): only a lunge towards where you look
    counts, it hits what is within 18 units of your eyes as the head stops, for the damage times the lunge's speed
    over the threshold (up to three times), pushes it, and waits 0.4 s before the next. No sound unless it hits.
