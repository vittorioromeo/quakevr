# Feedback round 12: plan and notes

From the second batch of voice notes.

| # | Note | Request | Status |
|---|---|---|---|
| 1 | vrstart 18-31-24 | Flashlight: sits wrong in the hand; shadows on by default; a visible cone of light | done |
| 2 | start 18-34-15 | Surfaces look flat: the effect where geometry seems to come out (parallax) | done |
| 3 | vrfiringrange 18-36-13, 18-37-31 | One-hand shove; punches, slaps and overhead blows balanced | done |
| 4 | vrfiringrange 18-39-17, start 18-43-25 | Melee from wrist flicks and wiggles; too little force needed | done |
| 5 | vrfiringrange 18-40-20 | The firing range is too bright and saturated | done |
| 6 | start 18-41-37 | Old-style particles (lava balls): use Quake VR's | done |
| 7 | start 18-42-41 | Gadget light more directional, a little bloom on the gadget | done |
| 8 | e3m1 18-44-08 | A faint 1-pixel HUD outline in front of the eyes | done (check) |
| 9 | e3m1 18-45-39 | Ammo screen light: smaller, a bit stronger, a little bloom | done |
| 10 | e3m1 18-46-28 | Bullet holes pixelated; a dent look | done |
| 11 | e3m1 18-47-11 | A CRT/glitch look for the gadget; icons and numbers in its colour | done |
| 12 | e3m1 18-48-38 | The shotgun's sights like the double shotgun's | done |
| 13 | e3m1 18-49-23 | Gibs and heads destroyed by shots and throws, in a mist of blood | done |
| — | (question) | Profiling from the menu | done |

## Notes

1. **Flashlight:** held 5 cm further back and 4 cm lower (`vr_flashlight_hand_forward`, `_up`); shadows on by
   default (configs move once, version 6); a soft volumetric-looking beam (two open cones, additive, depth
   tested, faded at the edges by the view angle, pulled in where it meets walls; `vr_flashlight_beam` 0.35).
2. **Parallax occlusion mapping** (`vr_parallax`, `_depth` 3, `_distance` 512, `_steps` 16; Graphics): heights
   from each texture's luminance (darker deeper), blurred and normalised, in the normal maps' alpha; 8–16 steps
   plus refinement, each eye its own ray, faded with distance and at grazing angles; world and brush models,
   smooth-filtered textures only. About +0.1–0.2 ms a frame at the headset's size. Presets: on from Medium.
3. **Melee** (`vr_juice.qc`): a blow is measured at the wrist (estimated behind the controller), relative to the
   head: it must travel `vr_melee_distance` (0.2 m) along a steady line at `vr_melee_speed` (3.5 m/s, was 3;
   configs move once, version 7), and the hand must stop or pull back (and 0.2 s) before the next. Damage comes
   from the stroke's peak speed, weighted by its shape: straight 1.25, overhead 0.7, slap 0.6 (arcs move the hand
   about twice as fast). Measured on the dummy: punch 16.1, slap 14.6, overhead 18.8 (were 13.7, 25.8, 27.2);
   flicks and wiggles no hit. An open palm driven forward is a one-hand shove (half a two-hand shove).
4. **Firing range** (`Misc/quakevr/relight_firingrange.py`): its 17 lamps at 1200 put 87% of the lightmap at the
   maximum; now lit as outdoors: a sun, sky light, ambient occlusion, no bounce.
5. **Particles:** rocket, lava ball, grenade trails (fire, embers, smoke), scrag/knight/vore projectile ribbons
   in their lights' colours, the tarbaby explosion, lava splash, teleport, spike impacts: all Quake VR particles
   now (`VR_EntityTrail`); `vr_particles 0` brings Ironwail's back.
6. **Gadget and screens:** the gadget's screen has a CRT look (scanlines, grille, flicker, a rolling bar, faint
   static, a glitch burst every few seconds; `vr_gadget_crt`) and is monochrome in its colour (icons and big
   numbers too); it's drawn in the eye's scene so the bloom sees it; a soft edge glow on the gadget and the
   ammo screens (`vr_screen_glow`); the gadget's light is mostly where the screen faces (a light 14 units out,
   radius 36, and a faint one for the hand); ammo screen lights radius 34 (was 56), a bit stronger.
7. **HUD outline:** the head-following panel cut a hole where the (desktop) status bar is, and filtering blended
   the status bar's edge texels in: the hole is now a texel wider all round. Not visible in the mock: check.
8. **Bullet holes:** they were Hipnotic's low-resolution sprites, placed by the QC; with `vr_decals` they're now
   decals: 5 chip variants at 256 texels (a crater, a raised rim, cracks, dust), lit on the side the strongest
   map light comes from (the decal blend already brightens as well as darkens). Atlas 1024², mipmapped,
   anisotropic.
9. **Shotgun sights** (`Misc/quakevr/recolor_shotgun_sight.py`): the double shotgun's orange gradient.
10. **Gibs** (`vr_gib_destroy`, `vr_gib_health` 12, heads 18, `vr_gib_splat_speed` 250): shots (hitscan traces now
    stop at gibs: `MOVE_HITGIBS`), missiles, explosions, hand and gun strikes, and hard throws into walls or
    monsters burst them into a blood mist and splats. Not while held, not the first 0.2 s.
11. **Profiling:** Graphics > Performance Profile (Off / Record / Record and show).
