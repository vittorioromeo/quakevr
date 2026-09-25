# Feedback round 14: plan and notes

## Render scale

Voice note start 23-37-06: raising Render Scale made the headset's image stretched and wrongly projected.

**Cause.** Our side was consistent (the projection layer's `imageRect` always matched the swapchain; SteamVR logged
`bounds (0, 0, 1, 1)`), but the eye swapchains were recreated at the new size mid-session, and SteamVR's OpenGL
path does not follow that: it keeps copying into textures of the session's *first* eye size (its client has a
"Scene resize" path for D3D and Vulkan, none for OpenGL). SteamVR's log for this build
(`Steam/logs/xrclient_ironwail.txt`) shows it:

- The 23:24 session started at 0.5 (1646x1762): every later, larger size (up to 1.0) was copied without error, but
  into a 1646x1762 texture, so only the image's lower-left part reached the lenses, magnified to the whole field of
  view. That is the "stretched" picture of the voice note. This is inferred from the log; it wasn't seen in a
  debugger.
- The 23:20 session started at 1.0: sizes above 1 were cropped the same way; every size below 1 made the copy fail
  (`GL_INVALID_VALUE`, left pending), so SteamVR's next swapchain failed its own GL check
  (`SXR_GL_CHECK: error:0x501 ... glGenTextures`) and the engine fell back to 1.0. Going down seemed to do nothing.

**Fix.** The eye images keep the runtime's recommended size for the whole session. `vr_render_scale` now sets the
size the eyes are *rendered* at: the post-process pass writes into a texture of that size, and a linear blit
resamples it into the eye image. Above 1 this is supersampling (smoother edges, shimmer reduced); below 1 it's a
smooth upscale. There's no swapchain churn while dragging the slider, and it works the same on every runtime. Pending
GL errors are also cleared before `xrCreateSwapchain`, so an engine error can't fail SteamVR's swapchain again.
`vr_status` prints both sizes (rendered, and images/recommended/largest).

**Ironwail's `r_scale` ("Render Scale" in its video menu)** does something different: it renders the 3D scene at
1/2, 1/3 or 1/4 of the resolution into the same framebuffers and upscales it with *nearest* filtering, so you get
chunky retro pixels. It works in the eyes (tested with the mock backend at 2 and 3: pixelated, same framing) and
stacks with ours (it divides the size `vr_render_scale` gives). Both are kept:

- **VR Settings > Headset > Render Scale** (`vr_render_scale`, 0.5 to 1.5; the console allows 0.25 to 2): sharpness
  against GPU time. Use 1.2 to 1.5 when the GPU has headroom, and below 1 when frames are late.
- **Ironwail's Render Scale** (`r_scale` 2 to 4): only for the low-resolution pixel look.
- To hand the headset **larger images** (more detail in the lens centre, not only smoother edges), raise the
  resolution in SteamVR's per-application video settings (or Virtual Desktop's quality) before starting VR. The
  recommended size follows it and the swapchains are created at it.

**Check in the headset:** drag Render Scale between 0.5 and 1.5, in a session started at 1.0 and in one started
at 0.5. The picture should keep its scale and framing, with no zoom or stretch at any value. Edges get smoother
above 1 and blurrier below it. Check the frame timing at 1.5: at 120 Hz a 4090 was at about 7.3 ms at 1.0, and
1.5 renders 2.25 times the pixels. `xrclient_ironwail.txt` should show no more `SXR_GL_CHECK` errors, and only one
pair of eye swapchains per session.

## Glowing screen text

Voice note start 23-33-44: the text on the ammo screens and on the wrist gadget should glow too, with a bright,
whitish glow.

**What it does.** The glow happens in the CRT screen shader (`Shade::Screen` in `vr_gfx_gl.cpp`), so it's the
same in both eyes and doesn't depend on the bloom (which is weak with the author's settings). The shader does two
things:

- It pushes the cores of the lit strokes towards white, slightly above 1 so the bloom can catch them where it's on.
- It adds a halo from the screen texture's brightness, blurred. There's an inner ring (4 taps, about one pixel of
  the virtual screen, whitish) and an outer ring (8 taps, about 2.5 pixels, in the screen's colour). Each tap reads
  a mip level about as coarse as its ring is wide, and the face's own brightness is subtracted first so the face
  doesn't glow.

The screens' render targets (the gadget's 480x300 and the ammo screens' small ones) now have mipmap chains, rebuilt
in `gfx::end2D()`. Normal sampling still reads level 0, so the screens stay as sharp as before. The halo goes past
the glyph edges within the screen. It's added before the scanlines, grille, rim and flicker, so those run over it,
and it's sampled at the torn glitch coordinates, so it tears with the text. Text, big numbers, icons and the face
all glow, and so do the gadget's frame lines.

**Setting.** `vr_screen_text_glow` (default 1, 0 is off, up to 3) is in VR Settings > Wrist Gadget > Screen > "Text
Glow" and in Graphical Settings > "Screen Text Glow" (next to Screen Glow). The "Off (Quake)" look preset sets it
to 0. At 2 the halos start to merge into blobs on the ammo screens. The ammo screens glow only with their CRT look
on (`vr_weapon_screen_crt` > 0). Without it they're plain glyphs again.

**Cost.** 13 extra texture reads per screen fragment, on a few small quads, plus the mipmap generation of about five
small textures a frame. It's negligible.

**Check in the headset:** read the ammo screen (shotgun "8/8 25") and the gadget up close and at arm's length. The
digits should have white-ish cores and a soft green halo, with the scanlines still visible across the halo. Try Text
Glow 0, 1 and 2, and with your settings (brightness 1.5, background 0.6, Screen Glow 3). Also check that a glitch
burst still tears the glow along with the text, and that the text isn't blurred at an oblique angle.

## Menus

Voice notes vrfiringrange 23-31-12 (a way straight back to the game from deep in the menus, reopening where you
left, the right stick scrolling) and start 23-41-04 (live preview should keep the game running).

**Back to game.** A button in the menu panel's top-left corner, with an arrow and "Back to game", is drawn over every
menu (Ironwail's and the VR pages) when the VR menu style is on. It lights up under the laser with a tick in the hand;
the trigger on it closes the menu from any depth. **Holding the menu button for half a second** does the same. A
short press is still Escape (back a page), but it now happens when the button is let go rather than when it is
pressed. Quest 3 has only the one menu button (on the left controller), so the hold is the shortcut. The label is
shown when there's room to the left of Quake's plaque (a 16:9 mirror window at the default row spacing); on a
narrower canvas only the arrow is drawn.

**Reopening where you left** (`vr_menu_remember`, default 1; VR Settings > Advanced > Menu > "Reopen Where Left").
After Back to Game, the next time the menu opens (menu button, Escape, `togglemenu`) it opens on that page: the menu,
its selection, and a list's scroll (options pages, key bindings, maps, mods), or the VR page with its selection,
scroll and way back. This is one-shot. Other ways of closing (Escape from the main menu, starting or loading a game)
open the main menu next time, as Quake does. The page is opened over the main menu, so Escape from it leads back
normally. Where it can't be resumed, it falls back:

- Saving outside a single-player game goes to the main menu.
- The skill and quit dialogs go to the menu they came from.
- Mod details go to the mods list.
- Multiplayer setup, searches and server lists go to the multiplayer menu.
- Controller calibration goes to the controller options.

This only happens in VR; the flat screen behaves as before.

**Right stick scrolls.** On a page with a scrollbar (the VR pages, and Ironwail's options pages, key bindings, maps
and mods), pushing the main hand's stick up or down scrolls the list. It moves at 3 rows a second just past the dead
zone, up to 25 at full push, and the first row moves at once. There's a faint tick per row. The selection stays
where it is while it's in view; when it would scroll out, it moves to the nearest visible row. Left and right on that
stick still change values. The off-hand stick navigates as before, and on pages without a scrollbar both sticks
navigate as before.

**Live preview keeps the game running** (Ironwail's `ui_live_preview`, on by default; also in VR Settings > Advanced
> Menu). In VR, in single player, the game keeps running while the menu is on a settings page: Ironwail's options
and its Display/Graphics/Interface/Game/Controller pages, and the VR Settings pages. So water, lights, particles and
animations can be seen changing. The choices:

- The main menu and the others (load/save, maps, quit...) still pause, so Escape is still a pause.
- The settings pages pause too while a living monster is after you (its `enemy` is you). The menu stays a safe pause
  in a fight, and monsters don't attack you while you tweak.
- Everything else runs, monsters included, as in multiplayer. Lava, slime and drowning keep hurting while you're in
  the menu.

Tested with the mock backend: two screenshots 100 frames apart on a VR page differ (the world animates). With the
preview off, or on the main menu, they are identical (paused).

**Engine hooks (all marked QVR):**

- `menu.c`: `M_ToggleMenu_f` calls `VR_MenuReopen` before `M_Menu_Main_f`; `M_Draw` calls `VR_MenuDrawOverlay` (the
  button); `M_Keydown` gives `K_MOUSE1` to `VR_MenuClick` first. There are two new functions after `M_Mousemove`:
  `M_ScrollList` (scroll the current list by rows, keeping the selection in view) and `M_ListPosition` (read/restore a
  list's cursor and scroll).
- `host.c`: `Host_ServerFrame` also runs `SV_Physics` when `VR_MenuRunsGame()`.
- The declarations are in `vr_api.h`.

**Check in the headset:**

- From Advanced VR Options > Body (scrolled down), click Back to Game, then press the menu button: you should land on
  Body at the same row. Do the same by holding the menu button.
- A quick press of the menu button should still go back one page.
- Scroll Body, Gameplay and Customize (key bindings) with the right stick, gently and at full push.
- Open Graphics or VR Settings in the firing range or at the water in start: water and lights should animate behind
  the menu. Then do it with a monster chasing you: the game should pause.

## Melee

Voice note vrfiringrange 23-27-11: a hard overhead axe swing always came out as a bash (8 damage), while waving
the axe softly up and down like a flag still hit. Melee should reward fast, forceful, wide swings and straight
punches, and a guard pushed forward should be a bash.

**Why it failed.** The bash fired on any hand driving forward at 1.6 m/s while its weapon pointed "across" the
facing. An axe held up at the top of a swing (pointing up, or level above the head) counts as across, and the
forward part of the chop drove it. The bash also ran before the swing and blocked it. Separately, a blow only had
to reach 3.5 m/s at the wrist over 25 cm, and a blow at that minimum already did full damage (x1), so a soft wide
wave did a full axe hit.

**The model now** (`QC/vr_juice.qc`, "Blows"; one model for fists, clubs and swung weapons):

- Each hand's **wrist** (estimated behind the controller) and **striking point** (the fist, or the weapon's head)
  are followed relative to the head. A **stroke** is one run of the wrist in a steady direction. It starts from
  rest or a turn back (the wind-up), and it ends when the wrist slows down, turns away by more than ~65 degrees
  (arcs up to ~130 degrees go on), or lasts over 0.5 s. A frame with no new pose doesn't end it.
- A stroke is a **blow** when the wrist has travelled `vr_melee_distance` (0.25 m) and reached the minimum speed:
  `vr_melee_speed` (3.5 m/s) for a fist or a gun used as a club, 1.25x that for the axe, sword and Mjolnir. It must
  also get there in a snap (the wrist's mean acceleration to its peak is 25 m/s2 or more), still be moving hard,
  and not go back towards the body. Fists also reject whips, as before. Flicks, wiggles and whips fail on the
  wrist's travel, pull-backs on direction, and slow waves on speed and snap.
- **Strength** multiplies the weapon's base damage (fist 10, gun 12, axe 20, sword 20 x `vr_sword_damage_mult`,
  Mjolnir 25). It's the product of:
  - speed: linear, 1 at a good blow (1.6x `vr_melee_speed` for fists and clubs, 1.8x for swung weapons), 0.25 at
    a fist's minimum and 0.45 at a swing's;
  - reach: 1 to 1.2 as the wrist's travel goes from 1x to 2x `vr_melee_distance`;
  - shape: fists and clubs keep straight punch 1.25 (`vr_melee_punch_mult`), uppercut 1, slap or hook 0.6 and
    overhead 0.7. Swung weapons get 1 for arcs (overheads, side swings), 0.8 for straight thrusts, and 0.5 swung
    straight up;
  - for a swung weapon, the head's snap: its peak speed over the wrist's, from 0.85 (held stiffly) to 1.2 (x1.8,
    a wrist snap or a long arc).

  The strength is capped at 2, which is 20 for a fist and 40 for the axe.
- **Bash** (`VR_Bash`): the guard must be held **still** first, then pushed forward within 0.4 s, at
  `vr_bash_speed` (1.6 m/s) or more and within 45 degrees of straight ahead (so not a chop). The guards are the
  parry's poses (`combat.qc`). A weapon guard (`VR_Parry_Guard`) is held across within `vr_parry_angle`, about
  level (within 30 degrees), from 65 cm below the eyes to 15 cm above them. A hands guard
  (`VR_Parry_HandsTogether`) is both empty hands within 45 cm of each other, in front, at the same height. A raised
  axe is never a guard, and a downward chop is never a push. The one-hand palm shove, parry, headbutt and deflecting
  projectiles (which needs a real blow) are unchanged.
- The training dummy reports the kind, the wrist's (and a weapon head's) peak speed, the travel, the snap and the
  strength, for example `melee: Axe, overhead blow, main hand, wrist 7.8 m/s, head 11.4 m/s over 63 cm, 59 m/s2
  (x1.62)`. The kinds for swung weapons are "swing", "thrust", "overhead blow" and "rising swing".
- No cvar defaults changed, so there's no config migration. `vr_melee_speed` and `vr_melee_distance` are still
  minimums, and the menu help says what they scale.

**Measured** on the training dummy (scripted mock motions at `host_maxfps 90`, damage per hit; generator in the
session's scratchpad `melee_gen3.py`):

| Motion | Before | After |
|---|---|---|
| Hard straight punch (5.9 m/s) | 21 | 14.8 |
| Medium punch (4.2 m/s) | 15 | 6.3 |
| Soft punch (2.7 m/s) | - | - |
| Jab (5.3 m/s) | 18.8 | 11.3 |
| Hook (7.9 m/s) | 14.9 | 12.3 |
| Uppercut (6.4 m/s) | 19.9 | 14.8 |
| Slap, normal (7.4 m/s) / very fast (9.3) | 16.5 / 22.3 | 14.4 / 20 |
| Overhead fist (11.7 m/s) | 25.5 | 20 |
| Palm shove / two-hand shove / hands guard + push | 4 / 8 / 8 | 4 / 8 / 8 |
| Fist flick, fist pulled back | - | - |
| Axe overhead strong (wrist 10.1, head 15.2) | bash 8 | 40 |
| Axe overhead medium (7.8) | bash 8 | 32.5 |
| Axe overhead light (6.1) | bash 8 | 22.9 |
| Axe overhead soft (0.6 s, 4.7, 19 m/s2) | bash 8 | - |
| Axe held level across at the top, then chopped | bash 8 | 18.6 |
| Axe side swing strong (7.3) / soft (4.4) | bash 8 / bash 8 | 25.2 / - |
| Axe flag wave, slow (0.6 s a stroke, 4.5) | 3x bash 8 | - |
| Axe flag wave, brisk (0.4 s a stroke, 1.5 m arcs) | 3x bash 8 | 11.8 down, 13.9 up |
| Axe small fast wiggle / wrist flick / pulled back | - / - / - | - / - / - |
| Axe guard + push forward (fast / slow 1.6 m/s) | bash 8 / 8 | bash 8 / 8 |
| Axe guard, raise, overhead chop | bash 8 | 40 |
| Axe guard, straight into a quick chop | bash 8 | 17.8 |
| Sword overhead / side (x1.5 sword mult) | bash 8 / bash 8 | 60 / 48.2 |
| Gun as a club overhead / thrust | bash 8 / - | 24 / 11.5 |

The mock's "before" side swings were bashes too, once the swing's weapon lagged behind the arm (pointing across).
The brisk wave is a full-range arm swing at about 1.2 Hz with the wrist at 5-7 m/s. That's a real swing, so it
still hits, weakly, and weaker going up.

**Check in the headset** (firing range dummy, with `developer 1` for the bash lines):

- Axe: chop overhead hard, from above your head down to your waist. It should be an "overhead blow" of about
  30-40, never a bash. A relaxed but committed chop should do about 15-25. Wave the axe up and down softly in
  front of the dummy: no hits. Side swings should be about 20-30.
- Hold the axe level across your chest, pause, and push it forward: a bash (8). Then from that guard, raise the
  axe and chop: a hit, not a bash.
- Fists: a hard straight punch should do about 13-16, a jab about 10, a hook or slap about 12-15, a lazy punch 5-7
  or nothing. Put both hands together at your chest, pause, and push: a shove.
- If real swings read slower than the mock's, lower `vr_melee_speed` (everything scales with it). If lazy motions
  still hit, raise it. The dummy's line shows the wrist speed and the m/s2 of each hit to tune by.

## Model bumps and parallax

Voice note vrfiringrange 23-30-36: the parallax on models looked weird and unnatural. Maybe they were missing the
world's bump mapping.

**Why it looked wrong.** Under their own (static) light, models had no bumps. Their skins' normal maps were only
used by dynamic lights, so the parallax slid the skin with no shading to match it, and the texture seemed to swim.
Parallax on Quake's skins is also wrong in itself:

- The skins are 8-bit and drawn sharp. The shifts bend their square texels into curves (the same reason the world's
  8-bit textures get no heights when drawn sharp). In the screenshots, the grunt's face and the super nailgun's
  window frame come out sheared.
- Heights from luminance aren't the model's shape: bright paint stands up, dark paint sinks.
- Skins are small and stretched, with many seams, and the triangles are big.

**What changed.**

- **Bumps on the models' own light** (`vr_normalmap_models`, default 1; Graphics > "Bumps on Models"; multiplied by
  "Bumps in Map Light"). The alias shader shades the skin's normal map with the model's light direction (from the
  map's light entities, `vr_modellight`, or Quake's fixed one). A bump leaning towards the light gets brighter, one
  leaning away gets darker. Only the lean along the surface counts, and that averages out over a skin, so a model is
  as bright as before (the crops' mean brightness moves by under 2%). The effect is weaker on the side facing away
  from the light. Held weapons and hands get half, because their skins are a hand's width from the eyes. At full
  strength, the same bumps that look fine on a monster turn into grain up close.
- **Seams.** Before a skin's normal map is made, its luminance is grown out of the skin's islands, 4 texels
  (`TexMgr_DilateIslands`). The black background or a neighbouring part no longer makes a ridge along a seam, which
  would have shown as a bright or dark line.
- **Model parallax is off by default** (`vr_parallax_models` 0). Configs still holding the old 0.75 take 0 once
  (config version 9). The option is still there. When it's on, it now fades out from 50 to 70 degrees off the
  triangle (it was 70 to 83), because a model's sides are grazing all round.

**Verdict on model parallax.** With heights generated from Quake's skins, I don't think it can look good. The
texels bend, and the depth is invented from paint, not from shape. It might work for high-resolution replacement
skins drawn smooth, with authored `_norm`/`_bump` maps (0.5 to 0.75), but that's untested. Bumps give the relief
without moving the skin.

**Screenshots** (mock eyes; left: before, with parallax 0.75 and no bumps on the own light; right: now):
`model_bumps_before_after.png` in the scratchpad, showing the held super nailgun at the note's spot, a green armour
in the firing range, and an e1m1 grunt.

**Cost** (RTX 4090, mock eyes 1024 x 1024, a green armour filling the view and the super nailgun held): the alias
pass takes 0.014 ms a frame per eye with the bumps off and on alike. The bumps are one normal-map read (only when
bumps or dynamic lights are active) and about ten instructions per model pixel. Parallax at 0.75 took it to 0.022.

**Check in the headset:**

- Monsters (e1m1's grunts, the firing range's spawns) and pickups (armour, health) should show relief under the
  map's light, with no bright or dark lines along their seams.
- Look at the held weapons, close and turning your wrist: the bumps should be subtle, not noisy. If they're too
  strong or too weak, tell me and I'll change the held factor (0.5, `VIEWMODEL_BUMPS` in `vr_lighting.cpp`).
- Try "Bumps on Models" at 0, 1 and 2.
- If you want to judge parallax again, set Parallax Models Depth to 0.5 and move your head near a monster.

## Swimming

Voice note end 23-46-47: after a forward stroke, bringing the arms back for the next one swims you back as far,
so you make no progress. A relaxed return should do nothing, and a deliberate stroke back should still swim you
backwards. More knobs were asked for.

**The model now** (`vr_physics.cpp`, "Swimming"; the push per frame is as before: hand speed beyond
`vr_swim_stroke_min`, squared, times palm flatness, the look bias and the stick's steering):

- Each hand's motion is cut into **strokes**: a stroke starts from rest or a turn and follows the hand as it curves
  (a frog sweep out and back is one stroke). It ends when the hand stops or turns by more than 75 degrees.
- **Intent threshold:** a stroke propels only once its *peak* speed passes `vr_swim_power_threshold` (1.1 m/s). It
  fades in to full power over `vr_swim_power_knee` (0.5 m/s) above that. With `vr_swim_power_whole` (1) the whole
  stroke counts once it passes, including its slow start and the slowing tail. A relaxed return that never gets
  brisk pushes nothing.
- **Stroke memory:** each hand remembers its last full stroke (its direction and peak speed) for
  `vr_swim_intent_memory` (1.5 s; full for the first half, then fading). A stroke against it that is slower than
  `vr_swim_reverse_speed` (1.0) times the remembered peak is damped by `vr_swim_reverse_damp` (0.85). The damping
  is full at 0.3 below that ratio and gone at it. So a same-speed return is partly damped, and a deliberate reverse
  stroke that is as brisk isn't damped at all. Once a stroke passes, it becomes the new memory, so strokes back
  then keep going back.
- New shape knobs: `vr_swim_speed_exp` (2: push grows with speed squared; 1 linear; 3 steeper),
  `vr_swim_flat_exp` (1; 2 to 3 means only a really flat palm pushes fully), `vr_swim_palm_dir` (0; 1 pushes away
  from where the palm faces instead of against the motion, so a palms-in sweep pushes sideways, not back).
- Speeds over 8 m/s are ignored as tracking jumps.
- `vr_swim_debug 1` prints every stroke to the console and the wrist log, for example
  `swim main: peak 2.39 m/s, flat 0.90, power x1.00, push 167 of 167 (+138 ahead)`.
- Menu: **Advanced VR Options > Swimming** (new page) has all of them, plus "Reset Swimming to Defaults". The
  Locomotion page keeps only the Swimming on/off switch.

No existing default changed (no config migration). The author's own values (stroke 12, palm 0.8, recovery 0.05,
look 0.2, glide 0.7) are untouched. The new cvars start at the defaults above.

**Measured** (mock hands, both mirrored, at about 50 fps, 4 cycles each in the vrfiringrange pool, units forward
per cycle. The stroke takes 0.4 s over 0.6 m and peaks at 2.4 m/s. The "flat" returns keep the palm facing the
way it moves, the worst case.)

| Case | Old, author's values | New, author's values | Old, shipped | New, shipped |
|---|---|---|---|---|
| Frog stroke, return at 55% speed | +54 | **+73** | +38 | +51 |
| Frog stroke, return at 75% speed | +40 | **+68** | +28 | +48 |
| Frog stroke, return at the same speed, flat | +19 | +35 (+64 with `vr_swim_reverse_speed 1.2`) | +13 | +24 |
| Frog stroke, same-speed return edge-ish | +37 | +48 | +23 | +31 |
| Straight pull (palms back), return at 55% | +47 | **+65** | +32 | +45 |
| Straight pull, return at 75% | +31 | **+60** | +21 | +41 |
| Deliberate reverse stroke, relaxed return | −43 | **−61** | −29 | −42 |
| Frog, then a deliberate reverse (one of each) | +15 | +12 | +14 | +9 |
| Look up, push down (2.6 m/s), relaxed up: rise | +13 | +19 | −15 | −13 |
| Idle sink per second | −47 | −47 | −46 | −46 |

Stroke debug for the frog stroke with a 55% return: the stroke gives 167 of 167 and the return gives 1 of 47. For
a same-speed return: 101 of 142 (x0.71). Gentle treading (1.4 m/s pushes) now gives about 75%, and slower sculling
nothing. To tread water, push down briskly or lower the threshold.

**Try in the headset:**

1. Set `vr_swim_debug 1` and swim as you normally do. The wrist log shows each stroke's **peak**: compare the
   forward strokes with the returns. Set **Intent Threshold** just above your returns' peaks and below your
   strokes'. Leave Intent Fade-In at 0.3 to 0.5.
2. If your returns are as brisk as your strokes, raise **Reverse Stroke Speed** to 1.2 to 1.4: a stroke back then
   has to be clearly harder than the stroke it reverses, or come after a pause longer than Stroke Memory.
3. If the debug shows your forward strokes with a low **flat** (under about 0.4), your grip holds the palm edge-on
   while you pull. Set Palm Matters to 0.3 or lower, or Recovery Push to 0.3 or higher, so the stroke still pushes.
4. Ranges to explore: threshold 0.6 to 2.0, knee 0 to 1, memory 0.5 to 3 s, reverse damping 0.5 to 1, reverse
   speed 0.8 to 1.5, speed curve 1.5 to 3, palm sharpness 1 to 3, glide 0.4 to 0.9.

## Water fixes

Voice notes end 23-48-00 and 23-48-20 (the underwater wobble also bent the HUD, the log text and the menu) and
start 23-41-49 (a halo round objects, text and HUD elements seen over water).

**Wobble on the UI. Cause:** the underwater wobble and blur run in the eye's post-process pass, and the laser lines,
the HUD panel or menu, and the wrist log were drawn into the eye's scene colours before that pass, so they got
bent with the world. **Fix:** after the post-process (and the render-scale resample), each eye now draws its UI
straight into the eye's final image: lasers, the HUD panel or menu with its pointer, and the wrist gadget's floating
log (`vr_stereo.cpp`, `drawUi`; `text3d::drawOverlay`). The wrist gadget itself, the ammo screens, world and
floating texts stay in the scene and still wobble. The wobble is also back on with a menu open: before, it was
switched off then because the menu was in the pixels it read. Side effects: the UI is drawn at the eye image's full
size, so it stays sharp below Render Scale 1. The world's bloom no longer spills over it, and `vr_gamma` /
`vr_contrast` no longer apply to it (they're neutral at 1 by default). The wrist log is no longer depth tested, so
the other hand can't hide part of it. For the desktop mirror, the UI is drawn into the scene colours too after the
post-process, so the mirror shows it as before.

**Halo. Cause:** refraction reads the opaque scene at a wave-shifted pixel. Next to anything in front of the
water (a hand, a gun, a monster standing in it, a text), the shifted pixel landed on that object, so its colours
were smeared into the water beside it. **Fix:** refraction only reads what is behind the surface. Before the
first translucent liquid draws, a small pass turns the scene's depth into a half-size R32F texture: the distance
of the nearest of each 2x2 pixels (`vr_water.cpp`, `sceneDistances`, unit 8). It can't read the depth buffer
directly, because the translucent pass is testing against it and writing its stencil. If the shifted pixel is in
front of the water, the shift is cut to 30%, or to zero if that is still in front. The shift also fades in over the
first 12 units of depth along the view, so the edges of a pool, and things standing in it, don't smear either
(`LiquidRefract` in `gl_shaders.h`). Ironwail hooks, marked `// QVR`:

- `R_OpaqueSceneDepthTexture` and `R_RestoreTranslucentTarget` in `gl_rmain.c`
- one call and one bind in `R_DrawBrushModels_Water` (`r_world.c`)

**Cost:** nothing when no translucent liquid is in view or refraction is off (`r_oit 1`, no MSAA, as before).
Otherwise the mock at 2048x2048 per eye measured +0.022 ms of GPU time per eye (the pass plus three texel reads per
water pixel), which scales to roughly 0.06 ms per eye, or 0.12 ms a frame, at 3292x3524. Two guesses were
measured and removed along the way:

- a full copy of the depth buffer: slower
- `glGet` calls to restore the framebuffer: they cost 0.25 ms of CPU time a view

**Check in the headset:**

1. Under water, with the wrist raised: the log text, the HUD panel (with `vr_hud_mode 0`: the status bar on the
   hand) and the menu should be steady and sharp, while the world, including the wrist gadget and the hands,
   wobbles.
2. Open the menu under water: the menu is steady, and the world behind it still wobbles.
3. Over clear water (the start map's pool, `r_wateralpha` below 1): wave the gun and hand above the surface. There
   should be no smeared fringe beside them, and the refraction should still bend what is under the water.
4. Look along a pool's edge and at things standing in water: no bands of the wall above the waterline smeared into
   the water.
