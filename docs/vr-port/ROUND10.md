# Feedback round 10: plan and notes

| # | Request | Status |
|---|---|---|
| 1 | Held boxes lag behind a moving player (rubber band) | done |
| 2 | Knockback too strong; a base value and settings per source | done |
| 3 | A meaty headshot sound, like Counter-Strike's | done |
| 4 | Thrown boxes and gibs hurt too much; settings for it | done |
| 5 | Gibs grabbed by hand only, not force-grabbed | done |
| 6 | Heads can't be grabbed | done |
| 7 | Gibs: Quake VR blood particles, a lasting trail of blood, splats where they hit or bounce | done |
| 8 | Thrown gibs hurt monsters even lying still | done |
| 9 | The player's box keeps you far from walls and ledges | options below; not changed yet |
| 10 | Water isn't see-through | done (relit maps) |
| 11 | Wounded arms drip blood | done |
| 12 | Separate bloom for white and coloured lights | done |
| 13 | A training dummy in vrfiringrange that reports every hit | done |
| 14 | The DarkPlaces look (research items 1-6) and normal maps made at load | done |

## Notes

1. **Held objects.** The client placed the hands from where it last saw the player; the server applied them after
   moving the player on, so a held box trailed the hand by a step or two of running. The move now carries the
   origin the hands were placed from, and the server moves every hand field (hands, muzzles, throw points) along
   by how far the player has moved since (`server::rebaseHands`, before hand touches and before the per-frame
   QC). **Tested:** running with a box, it stays at the same offset from the player every frame. Shots also leave
   the gun while moving.
2. **Knockback** (Gameplay > Knockback): `vr_push` (0.5) scales all of it, and each source has its own setting:
   your melee hits (`vr_melee_push`), weapon hits (`vr_hit_push`), killing blows (`vr_kill_push`, new), parries
   (`vr_parry_push_enemy`, `vr_parry_push_player`), monsters' blows (`vr_melee_push_player`), bash and shove
   (`vr_bash_push`). Steps of 0.05.
3. **Headshot sound** (`Misc/quakevr/make_sounds.py`, synthesised): a crack, a short helmet dink, a low punch, a
   mid thwack and a wet crunch of bone, soft-clipped together.
4. **Thrown damage:** `vr_carry_throw_damage` 8 (was 25; configs on the old default move once) and a separate
   `vr_gib_throw_damage` 4, both at about 6 m/s and more the faster. Throwing page, steps of 1.
5. **Gibs:** `vr_grab_gibs` is now Left alone / Grab by hand (default) / Hand and force grab.
6. **Heads:** a head is the monster itself; the soldier's death code (and others') made it non-solid after it
   was made grabbable, and it kept its monster flag, so the killing blow's push threw it at over 1000 u/s. Its
   solid is restored after the death code and it is no longer a monster. **Tested:** a gibbed grunt's head picked up.
7. **Gib blood** (`vr_gib_blood`, `vr_gib_blood_trail`; Graphics): gibs trail Quake VR blood, leave drops on the
   floor every 20 units while moving (at most 64 a gib), and splat and spurt where they hit or bounce (seen by the
   client from their movement: rigid bodies and plain gibs have no touches).
8. **Thrown hits need speed** (`vr_throw_hit_min_speed` 200 u/s): slower, a thrown object touching a monster just
   ends its throw. Thrown rigid bodies never touched the world on landing, so they stayed "thrown" and hurt the
   next monster walking into them.
9. **The player's box.** Your head is always right above the middle of Quake's 32-unit player box, so your eyes
   stay 16 units (about half a metre) from any wall, railing or ledge lip, and are pushed back with the box. Quake
   maps only have hulls for three box sizes, fixed at compile time. Options:
   - **Lean within the box (recommended):** the head may move up to about 14 units from the box's middle before
     the body follows; the box is always in open space, so the head never enters a wall. You could put your
     face near a wall and lean over a railing. Client-side (the camera and hands are placed from the body plus
     the head's offset); roomscale walking sends only the part of the head's motion beyond the lean.
   - **Lean past the box**, with a head-sized trace so the head can't enter walls (more reach, a fade to black
     when it would).
   - **A smaller box against the world:** needs box traces against the map's polygons (as DarkPlaces'
     polygon collision) or brush data (BSPX BRUSHLIST from recompiled maps); a large engine change.
10. **Water.** id's maps were vised with liquids as walls, so the engine keeps them opaque. The relit maps now get
    VisPatch's water-vised visibility (`relight_maps.py --vis-dir`, or `vis_maps.py`; see GRAPHICS.md "See-through
    water"). Done for your relit maps. `quakevr.cfg` keeps lava opaque (`r_lavaalpha 1`); `r_wateralpha` sets
    the rest. With Relit Maps off the originals load, and water is opaque. A later engine-side option would merge
    visibility across liquids at map load, for any map.
11. **Wounded arms** (`vr_body_blood`; Body > Wounds Drip Blood): where the wound skins show (below 75 health),
    drops swell on the forearms and hands and fall, more at lower health and just after a hit, flung off by a
    hard swing; small marks where they land. Not in front of your eyes. The drops are drawn over everything,
    your arms included (no depth test yet).
12. **Bloom** (Graphics): `vr_bloom_white` (0.5) and `vr_bloom_color` (1.5) times Bloom, by how saturated the
    light is; and `vr_bloom_adapt` (4): bloom weakens the more of the view glows, so a brightly lit map (the
    firing range) is not washed out while lamps in dark rooms keep their glow.
13. **Training dummy** (vrfiringrange, ahead-left of the start): a grunt that can't be hurt, stays put and flinches.
    Every hit prints to your console and floats a number up from the hit point (yellow head, orange melee, cyan
    thrown, red explosion, green deflected), e.g. `Dummy: 28 damage - melee: punch, main hand, 6.7 m/s (x2.24), at
    the body [2 hits: 55.9]` or `Dummy: 14.4 damage - weapon fire: Shotgun, 6 pellets, 6 legs (x0.60)`. Kinds:
    melee (fist, punch, box in hand, axe, sword, Mjolnir, a gun as a club, with speed and multiplier), headbutt,
    bash or shove, weapon fire (pellets by body part, projectiles by type), thrown weapons, boxes, gibs and heads
    (with speed), explosions, deflected projectiles, monsters' attacks. **Tested:** punches and a shotgun blast.
    Its sign reads right from both sides (world texts no longer show mirrored from behind).
14. **DarkPlaces look** (details in LIGHTING.md "DarkPlaces look (round 10)"; Graphics page; each part switchable):
    - `vr_gamma`/`vr_contrast` (1) for the headset: your desktop gamma 0.95 / contrast 1.2 no longer brighten it;
    - the relight: no bounced light, stronger ambient occlusion, half the glow light (`--bright`: the old one);
      your relit maps were re-made;
    - smooth filtering for replacement textures (`vr_texture_smooth`);
    - models as bright as the floor they stand on (`vr_model_light_parity`), hands' minimum light 8
      (`vr_viewmodel_minlight`, was 24);
    - DarkPlaces' dynamic light falloff and colours (`vr_dlight_falloff`): strong white flashes fading in 0.05 s,
      orange explosions; `vr_flash_scale`/`vr_explosion_light_scale` now scale those (defaults 1; your config
      has `vr_flash_scale 1.7`);
    - a sheen under dynamic lights (`vr_specular` 0.125);
    - normal maps made from each texture and skin at load (or a `_norm`/`_bump` image beside a replacement),
      for dynamic light and sheen only (`vr_normalmaps`, `vr_normalmap_strength`; next map); about 24 MB on e1m1
      with QRP textures.

    Dark areas are much darker now; if too dark, Light Contrast between 1 and 1.5 (you play at 1.5).
