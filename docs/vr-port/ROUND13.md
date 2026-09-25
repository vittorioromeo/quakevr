# Feedback round 13: plan and notes

From the third batch of voice notes, the first headset profile, and the VDXR question.

| # | Note | Request | Status |
|---|---|---|---|
| 1 | e1m1 19-58-43, 20-01-56, vrfiringrange 20-14-55 | Parallax on pickups too deep, a black box; expansion ammo without it | done |
| 2 | vrfiringrange 20-15-32 | Parallax on weapons and the body, with its own depth | done |
| 3 | e1m1 20-00-31 | Ammo screen light directional and tight; the CRT look on ammo screens | done |
| 4 | e1m1 20-03-50, start 20-16-22 | Flashlight doesn't light models; flickers over stairs (a single ray) | done: a real spotlight |
| 5 | e1m1 20-05-06 | Shoot or punch a gib held in the other hand | done |
| 6 | e1m1 20-09-35, 20-10-07 | Melee: whips still hit; backward pulls count | done |
| 7 | e1m1 20-11-57 | A profile to look at | done (below) |
| 8 | e4m1 20-24-55 | Floating objects jitter at the water's surface | done |
| 9 | e4m1 20-25-38 | Shove monsters off ledges | done |
| 10 | e4m1 20-26-44 | Mantling and hanging from ledges (experimental) | done, off by default |
| 11 | end 20-22-30 | Swimming backwards (the reverse frog stroke) | done |
| 12 | end 20-24-03, start 20-17-58 | Liquids: waves, caustics, an underwater view | done |
| 13 | vrtutorial 20-28-00 | The tutorial map looks fullbright | done |
| 14 | vrtutorial 20-28-18 | VR-friendly menus, by changing the drawing primitives | done |
| — | (question) | VDXR as an alternative to SteamVR | done: a runtime choice |

## Notes

1. **Parallax on items:** the boxes are drawn at a quarter size, so a depth in world units was a third of a box;
   items now use `vr_parallax_items` (1.5, in the box's own units). The black box: the shells' and nails' side
   textures use only their lower three quarters (the top is black); the ray wrapped into it. The shader now
   shrinks the shift towards the edges of the part of the texture a face shows. Expansion ammo (8-bit only)
   gets heights too.
2. **Parallax on models** (`vr_parallax_models` 0.75): weapons held and lying, pickups, monsters, the body and
   hands; skins carry heights (RGBA8, about 8.5 MB more), which rise to the surface near the skin's seams.
3. **Ammo screens:** the light is a spot (the renderer has them now, see 4) 1 unit in front of the screen,
   radius 28, full to 45° and nothing past 85°: the gun's sides stay dark. The screens have the gadget's CRT
   look (`vr_weapon_screen_crt`), each glitching at its own times. Their tilt is now composed with the hand's
   turn (added angles tilted the screen the wrong way on a gun pointing up or down).
4. **Spotlights:** any dynamic light can be a spot (a direction and inner/outer cone; `lighting::dlightSpot`),
   in the world and model shaders, with a single square shadow tile. The flashlight is one spot at the lens
   (full to 10°, none past 22°, a 25% wrap) plus a faint spill and a glow at the lamp; no traces place lights, so
   sweeping stairs doesn't flicker, and models in the beam are lit and cast shadows.
5. **Held gibs:** the other hand's shots, nails, rockets, lightning, fist and swung weapon hit a gib held in one
   hand (it's dropped and takes the hit); the hand that let go of one is blocked for 0.4 s, not both.
6. **Melee:** strokes pointing backwards (more than ~117° from the head's facing) are never blows, and 90–117°
   need to travel further; fists turning more than 2.6 rad per metre of travel (whips) are rejected;
   `vr_melee_distance` 0.25 (configs move once, version 8). Whips, pull-backs and wiggles: no hit; punches,
   slaps, overheads, uppercuts as before. A swing starting inside a monster counts.
7. **The profile** (e1m1, RTX 4090): eyes 3292x3524 (SteamVR's supersampling), world+brush 3.3 ms, bloom 0.12,
   CPU work ~1.5 ms. The "3.2 ms" of GPU time outside the eyes is not work: the profiler's closing timestamp
   waits for the CPU to return from xrEndFrame (the runtime pacing the frame), so it's mostly idle GPU; the real
   GPU load is about 4–6 ms, 2–4 ms of headroom; late frames come from spikes. New: **Render Scale**
   (`vr_render_scale`, VR Settings > Headset) and **Hide Lens Corners** (`vr_visibility_mask`: the lenses' hidden
   area drawn into the depth buffer first: about 17% fewer pixels shaded; 13% less eye GPU time in the mock);
   the runtime's calls are GPU scopes (`xr acquire/release/submit`); the CSV records the runtime, render scale
   and mask.
8. **Buoyancy** (`vr_rigid.cpp`): the lift grows with how deep the box is, with near-critical vertical drag;
   objects settle in ~1.3 s and bob a fraction of a unit. It was QC setting a rise speed whenever the origin
   point was under water.
9. **Ledge shoves** (`combat.qc`): a hard push also slides the monster along the floor after the hop (its feet
   just off the floor so its AI can't walk it back); with nothing under it, it falls; fall damage past 150 units.
   A one-hand shove pushes at 0.75 of a two-hand one.
10. **Ledge grab** (`vr_climb` 0; Locomotion > Ledge Grab (Experimental); `vr_climb.cpp`): an empty hand's grip
    at the top edge of a ledge with a drop below and room above hangs you there; the body follows the hand
    inversely; the other hand can take a ledge at most 16 units higher (shimmy, not climb); pull up to mantle
    (the box must fit on top); letting go falls with a capped fling. Walls, floors and steps can't be held.
11. **Swimming:** the palm or the back of a hand meeting the water flat pushes, edge-first hardly; the push grows
    with the square of the hand's speed, so a brisk stroke outweighs a slower return whichever way; the look bias
    is symmetric. Frog +61 units a cycle, the reverse stroke −59 (was +10).
12. **Liquids** (`vr_water.cpp`, `vr_water_*`, Graphics > Water and Liquids): waves in the shading (sums of sines),
    a fresnel term, glints, refraction of what's under translucent water (with `r_oit 1`, no MSAA), murky
    slime, glowing lava, a teleporter shimmer; caustics on surfaces under water or slime (a coarse grid of the
    map's liquids); under water: fog and a tint in the liquid's colour, a slow world-anchored wobble and a slight
    blur in the eyes' post-processing. Real waves would need tessellated liquid faces.
13. **Tutorial** (`Misc/quakevr/relight_quakevr_maps.py`, which replaces `relight_firingrange.py`): the map had
    never been lit (an empty lightmap draws fullbright); now its own lamps light it (the 16 fill lamps at 1200
    dropped), with coloured glow lights, dark corners, a faint cool sky.
14. **VR menus** (`vr_menuui.cpp`; `vr_menu_vr_style`, `vr_menu_spacing` 1.5): the menu canvas fills the panel and
    is stretched vertically, glyphs keep their size, so rows are spaced; sliders, toggles, text boxes,
    scrollbars and the highlight are modern widgets; a laser from the hand drives Ironwail's own menu mouse
    (hover selects, trigger clicks and drags). Source-port hooks: five in `menu.c`, a few in `gl_draw.c`,
    marked `// QVR`.
15. **VDXR:** it runs our OpenGL backend as it is (D3D11 interop, zero copy). **OpenXR Runtime** (VR Settings >
    Headset; `vr_xr_runtime` 0 system / 1 VDXR / 2 SteamVR / 3 `vr_xr_runtime_json`) sets the loader's
    XR_RUNTIME_JSON from the installed runtimes and restarts VR (switching works without restarting the game:
    tested loading VDXR 1.0.10 and SteamVR 2.17.10); `vr_status` and the profile show the runtime; with VDXR,
    VD's "Emulate Index controllers" is treated as Touch. Keep that option off anyway; check weapon alignment on
    VDXR once (its grip pose follows Meta's runtime).
