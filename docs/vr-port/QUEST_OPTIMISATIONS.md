# Quest renderer optimisations: a ledger

Every optimisation made so far for the standalone Quest (OpenGL ES / `-glescompat`) renderer and its frame cost, in one
place, so that we can check later which of them would also help PC VR or the flatscreen game. Sources: QUEST_PLAN.md
sections 6b to 6zm, ROUND25.md's Quest sections, `Quake/vr/vr_gfxpresets.cpp` (`questRows`) and the ES "lite"
switches in `Quake/gl_shaders.c`. How to read the columns: **Quest gain** is the number QUEST_PLAN states, with its
scene and section (GPU ms a frame on the Quest 3 unless said; launches vary by +-0.3 to 2 ms, so small figures are
loose); "not measured" means the docs give none. **Cvar / switch** gives the name and the Quest preset's value against
the PC default where the docs state it. **ES only?** says whether the code path exists only under the ES profile
(`-glescompat`, Android). **Would it help PC VR / flat?** is a judgement of the ledger's author, not a measurement
(Likely / Maybe / No). Updated each Quest round: new rows go at the end of their group. Presets are Quest 3 72 Hz / Quest 3
90 Hz / Quest 2 72 Hz, written a/b/c where they differ.

## 1. Resolution and targets

| Optimisation | What it does | Quest gain measured | Cvar / switch | ES only? | Would it help PC VR / flat? |
|---|---|---|---|---|---|
| Render scale below the runtime's size | Eyes drawn at a share of the recommended eye size; every pass scales with it | render scale 0.7 gave 29 ms against 25.6 ms at the preset's scale on e1m1 (not pixel-bound then; 6l); native 1.0 at 72 Hz estimated ~15 ms for a still view against ~12.4 ms at 0.9 (6zl) | `vr_render_scale` 0.9 / 0.75 / 0.65 (PC: per graphics preset) | no | Maybe: a plain user setting already; a PC GPU rarely needs it |
| Dynamic resolution with a low floor | Scale follows the GPU frame time down to a floor | floor 0.65 -> 0.55: live fight frames over 1.5 periods 7.4-16.6% -> 4.6-6.7%, a 3-light frame's world 12.2-13.2 -> 9.5-10.0 ms (6v); e1m1 still sat at the floor in 86% of his frames (6zl) | `vr_dynres` 1, `vr_dynres_min` 0.65 / 0.5 / 0.5 (C5i, his call: the Quest 3 72 Hz floor back to 0.65 once the lights went Quake's way; C4o had 0.55) | no | Likely: PC VR with dynres on and a floor tuned to the headset |
| No upscale pass (compositor scales) | Bilinear `vr_upscale 0`: the runtime's compositor resamples the smaller image | "upscale" scope 0.21 ms GPU at 0.9 on the PC (6d); no Quest number | `vr_upscale` 0 (PC: per preset) | no | Maybe: only when the runtime resamples anyway |
| Eye sub-image | The eye is post-processed straight into the lower-left part of the image and `imageRect` shows that part: the resample pass disappears | 0.07 + 0.03 ms GPU of the "upscale" scope gone on a PC mock; bandwidth saving on the tiler not measured (6f) | `vr_eye_subimage` 1 on ES, 0 on PC (2 = always) | yes (default) | Maybe: on PC it frees a full-screen read and write if the runtime accepts `imageRect` |
| Scene straight into the headset's images | Each eye's scene drawn into its swapchain image, no post-process (no tone curve) | post-process 1.0 ms gone, GPU span 13.2 -> 12.3 at 0.9, single pass off (6w); default off, foveation through it no effect | `vr_xr_direct` 0 (not saved) | yes | No: needs the tone curve moved into the shaders; PC has no tiler reason |

## 2. Render passes and formats

| Optimisation | What it does | Quest gain measured | Cvar / switch | ES only? | Would it help PC VR / flat? |
|---|---|---|---|---|---|
| MSAA off | The float scene's 2x MSAA resolved through memory on the tiler, so it is dropped | e1m1 frame 138 -> 41 ms (6l) | `vid_fsaa` 0 (preset a/b/c: 0; draft was 2) | no | No: a look change; the cost is the tiler's (no `EXT_multisampled_render_to_texture` path) |
| 32-bit float scene | R11F_G11F_B10F in place of RGBA16F for the scene | -1.4 ms GPU, same picture (0.24% pixels differ, noise 0.16%) (6o) | `vr_tonemap` 2 (PC default per preset) | no | Likely: halves scene bandwidth; check banding and bright-highlight range |
| OIT off | Translucent things sorted, not order-independent: OIT's own target is a render pass more per eye | e1m1 start -0.65 ms; frozen fight -0.6 to -1.0 ms (6q) | `r_oit` 0 (PC: per preset) | no | Maybe: a look change on PC; the render-pass cost is a tiler effect |
| Bloom off | Drops bloom's chain of half-size passes (the tone curve and grade are kept) | not measured as a gain; also the bloom fault (squares, black view) found and clamped (6m, 6p) | `vr_bloom` 0 (PC: per preset) | no | No: a look change; its cost is bandwidth-bound |
| TAA off | No history reads and writes | not measured | `vr_taa` 0 | no | No: look and quality choice |
| Heat haze, reflections, rim light off | The envmap and refraction extras left out | not measured | `vr_heat_haze` 0, `vr_water_reflections` 0, `vr_rim_light` 0 | no | No: look features |
| Window mirror off | The desktop mirror is not drawn when there is no window (`VR_NoWindow`) | -3.4 ms GPU (6o); the later audit finds the empty mirror passes cost 0.01 ms (6zl) | automatic under `VR_NoWindow` | yes (Android) | No: the PC wants its mirror; a toggle already exists |
| Framebuffer invalidation | `glInvalidateFramebuffer` on the eye's depth/stencil after the scene and on the image's colours before the post-process | -0.1 to -0.5 ms (6o) | `vr_gl_invalidate` -1 (ES on) | yes | Maybe: free on tilers and some integrated GPUs; moot on a desktop GPU |
| Permanent decals' bake under a scissor | Each bake touches only the bins its slots cover, not the whole layer | not measured (6p) | none (`vr_permdecals.cpp scissorOf`) | no (also on PC) | Likely: already shipped for PC; it is shared code |
| Right eye's own scene targets | Separate scene targets per eye | no gain, left off (6o) | `vr_eye_targets_split` 0 | no | No: measured nothing |

## 3. World shading

| Optimisation | What it does | Quest gain measured | Cvar / switch | ES only? | Would it help PC VR / flat? |
|---|---|---|---|---|---|
| Lite world (parallax, carved decals) | Parallax mapping and decal carving compiled out while their settings are off | e1m1 start world GPU 21.0 / 21.1 / 21.5 -> 18.6 / 18.4 / 17.7 ms (6n) | `vr_gl_world_lite` 2 (bits; 1 = C4g's first form); QVR_LITE_NOPARALLAX / NOCARVE | yes | Maybe: Adreno register pressure; a desktop GPU shades the dead code for free |
| Lite world, every dead feature | Bump, AO, caustics, retro light, detail, contrast code the Low/Quest settings turn off becomes constants (retro texture bit never set) | world+brush 12.9 -> 8.2 ms at e1m1's start (6p); their code alone cost ~4.6 ms | `vr_gl_world_lite` 2 default; QVR_LITE_NOBUMP/NOAO/NOCAUSTICS/NORETROLIGHT/NODETAIL | yes | Maybe: Low-preset PC users on weak GPUs; code specialisation rebuilds shaders per setting |
| NORETRO compiled out | Retro texture code left out | -0.8 ms in the fight, -0.4 at the start, but walls flat on Adreno (driver derivative bug): bit 64 never set (6t) | `QVR_LITE_NORETRO` (probe only) | yes | No: only a driver bug blocks it; on PC it would be a plain specialisation, measured on Adreno only |
| ES struct copy unrolled | Struct copies out of an SSBO and `gateinverse[3]` rewritten member by member (Adreno copied whole 240-byte `Light` to scratch) | frame 383 -> 140 ms (6l) | `vr_glsafe.cpp expandStructCopies / appendMemberCopies`; `-glesstructcopy` undoes | yes | No: a driver bug workaround, not a PC cost |
| No `centroid` | Dropped from the ES shader text (Adreno interpolated other inputs wrong) | a correctness fix, no timing | `-glescentroid` keeps it | yes | No: PC driver fine |
| World textures in arrays | World textures grouped by size in `sampler2DArray`s, one bind per group | world draws per eye 118 -> 25 (idle e1m1, mock on PC; GPU/CPU no change on PC); Quest CPU gain "not measured" (6e) | `vr_world_texarrays` 1 on ES, 0 PC (2 = any bound path) | yes (default) | No: the PC has bindless; useful only where draws are costly |
| Bindless off (side result) | The bound path in place of bindless | -0.1 to -0.37 ms where many textures draw, on the PC under ES (6b) | `-gleskeep bindless` to undo | yes | No: it is the fallback, not a gain |
| Textures, culling and PVS audited | PVS per eye, per-eye frustum and backface culling, visibility mask (lenses) checked | `r_novis 1` +0.6 to +2.0 ms; `vr_visibility_mask 0` +0.3 to +1.0 ms: already in place (6zl); no cut made | existing | no | No: nothing to take, shared with PC already |
| Menu sharpness | Eyes drawn at least at a given scale while a menu is open | not a gain (quality) | `vr_menu_render_scale` -1 = 1 on ES | yes | No: quality, not cost |

## 4. Dynamic lights

| Optimisation | What it does | Quest gain measured | Cvar / switch | ES only? | Would it help PC VR / flat? |
|---|---|---|---|---|---|
| No shadowed dynamic lights | Preset turns shadow lights and map light shadows off, atlas a quarter of Low's, so the plain loop is allowed | run-time switch alone only -0.6 ms with 3 big lights; the point is the plain loop below (6w) | `vr_shadow_dlights` 0, `vr_shadow_maplights` 0, `vr_shadow_atlas` 2048, `vr_shadow_distance` 768 / 640 / 512 | no | Maybe: a look change on PC; memory saving of 16 vs 64 MB depth |
| Plain lights loop | When no light can need the shadow, portal, map-shadow or falloff code, the world's loop is Quake's falloff, a spot cone, angle and sheen alone | 3 big lights: world 14.7 -> 7.1-7.6 ms, GPU span 16.7 -> 8.8-9.3 (6w); with DarkPlaces falloff 14.3-14.4 -> 7.7-8.3 world (6x); live fight 3-light frames' world 8.7-9.3 -> 6.1-6.2 ms (6x) | `vr_gl_lights_plain` 1; QVR_LITE_PLAINLIGHT (lite bit 26) | yes | Maybe: Adreno pays for code it never runs (registers); a desktop GPU is far less sensitive; worth an A/B with shadows off |
| Models' plain lights loop | The same plain loop in the models' shader | no measurable change (+-0.5) (6x) | `vr_glsl_alias.h ModelDynamicLights`; `QVR_PROBE_NOAPLAIN` undoes | yes | No: nothing measured |
| Unlit world | When no dynamic light reaches the world or brush entities (sphere against the BSP and the view's PVS), the world's programs without the lights' loop are used | GPU span -1.7 to -2.9 ms at 0.55 in the 6 of 10 places it applied (6z); scripted e1m1 period p50 16.9 -> 15.1, GPU 15.1 -> 12.4, world 12.0 -> 9.1 (6z) | `vr_gl_world_unlit` 1 (later 2, default 2); `glprogs.world_unlit` | yes | Maybe: skipping the lights' loop is cheap on any GPU when lights are few; check the cull's CPU cost |
| Unlit/lit split by surface | A second indirect command per texture: surfaces touched by a light's sphere draw lit, the rest unlit, in one flush | e1m2 start (0.55): all lit 5.76 / 4.08 -> split 5.17 / 3.41 ms (span / world), +18 draws (6zb) | `vr_gl_world_unlit` 2 (default) | yes | Maybe: the same lights test could serve PC clustered lights; a GPU cull change |
| Lit-skip of empty lit draws | Only the textures with a reached surface draw their lit command | e1m2 start draws 192 -> 180, GPU 5.40 -> 5.34 ms (6zc); PC mock 174 -> 158 draws | `vr_gl_world_lit_skip` 1 | yes | No: the empty draws cost something only on ES drivers |
| Lights reach the PVS | The reach test also needs the sphere to touch a leaf in the view's PVS | no change at e1m2's start; kept as a cheap cut (6za) | in `VR_WorldLightsCheck` | yes | Maybe: only meaningful with the unlit world |
| Rig lights off the walls | The guns' screen lights and wrist gadget lights skip the world's loop (they still light hands and gear) | ~0.34 ms at e1m2's start; makes most of e1m1's views unlit; not turned on (his look call) (6zb); his session: rig alone showed nothing (6zm) | `vr_rig_lights_world` 1 in the preset (0 saves) | no | Maybe: a PC look call; worth it only if the unlit world helps there |
| Dynamic lights in the lightmap | The frame's dynamic lights added into the lightmap texels on the CPU, the world drawn without the lights' loop | not measured on the headset (in the dark); PC mock 17 blocks 0.15-0.3 ms CPU at spawn, 141 blocks 1.35 ms in the hall (6zm) | `vr_gl_world_dlmap` 0 (not in the preset) | yes | Maybe: moves cost from GPU to CPU; a look change (smooth blob instead of rings) |
| Light-casting things trimmed | Torch, lava nail and explosion debris lights lowered | not measured | `vr_torch_lights` 2 / 2 / 1, `vr_lavanail_lights` 2 / 2 / 1, `vr_explosion_debris_lights` 2 / 2 / 0 | no | Maybe: a user setting for weak PC GPUs |
| Dynamic AO off in models | Models' programs compile `DynamicAO` out (the preset has it off) | no measurable gain; kept as free cut (6zd) | `vr_ao_dynamic` 0, `vr_ao_brush` 0; QVR_ALITE_NOAO | yes (the compile-out) | No: nothing measured |

## 5. Models

| Optimisation | What it does | Quest gain measured | Cvar / switch | ES only? | Would it help PC VR / flat? |
|---|---|---|---|---|---|
| Opaque models without `discard` | An opaque solid non-morphing model uses a form with the morph and gate discards compiled out, so Adreno rejects hidden pixels before shading | eye near the body: 17.7 -> 16.8 ms (body off 16.6) (6p) | `vr_gl_alias_opaque` -1 (ES on); `glprogs.alias_opaque` | yes | No: a tiler's early-z behaviour; a desktop GPU does early-z with discard (late) already tolerable; low priority |
| Models' lite form | Models' programs take the lite world's parallax and retro-light bits | frozen fight -0.2 to -0.5 ms (27.5 vs 28.2, 27.7) (6q) | `vr_gl_alias_lite` 1; QVR_LITE_NOPARALLAX / NORETROLIGHT | yes | Maybe: small |
| Models' plain form | An opaque non-morphing batch with no per-model features uses a form with that code compiled out (skin tone, glows, wounds, dents, stumps, normal map, morphs, rim) | frozen fight 19.8-19.9 -> 17.9 ms (6r); later: switching it off costs +3.1 ms in the 9-monster fight (6zb) | `vr_gl_alias_plain` -1 (ES on), `vr_gl_alias_plain_mask` 255; `glprogs.alias_plain` | yes | Likely: per-batch feature specialisation could cut ALU on any GPU with many monsters; test on PC |
| Gore form | A batch that needs only wounds and dents uses the plain form keeping those | live fight 12.90 -> 11.76 ms (6 rounds), period 15.1 -> 14.8 (6zd) | `vr_gl_alias_gore` 1 (ES); `glprogs.alias_gore` | yes | Likely: same reasoning as the plain form |
| Skin form | A batch needing only skin tone (gloves, nails) besides wounds and dents uses the plain form keeping them | +0.02 ms, nothing measurable (6ze) | `vr_gl_alias_skin` 1 | yes | No: nothing measured |
| World models' pre-pass off | Alias world models shaded as drawn with no depth pre-pass (the tiler's own early depth test) | frozen e1m1 fight -0.49 ms GPU, -20 draws, same picture (6zc) | `vr_alias_prepass_world` 0 in the preset (PC default 1) | no | No: tiler-specific (on-chip tiles; a desktop GPU's early-z makes it moot) |
| Posed models batched | Posed models of one model/skin/bone count join one batch; bones go into one buffer | -0.11 ms and -20 draws in the lifted fight (6zc); PC eye 0.000% over 8 | `vr_alias_batch_posed` 1 (all platforms) | no | Likely: fewer draws and binds for PC too; already on |
| Alias sort key fix | `Mod_AliasSortKey` (15 bits of the name hash) replaces a 16-key mask so models sharing a key batch together (PC bug in Ironwail's code) | with batching: fight 4.81 / 219 draws -> 4.00 / 149 (all three, 6zc); not measured apart | none (code fix) | no | Likely: already shipped for PC |
| View entities hidden in foreign demos | In id's demos the player's rig is hidden (head still placed), with its lights | demo1: period p50 21.5 -> 14.0 ms, GPU 20.9 -> 6.5 (6za) | `hands::demoViewOnly` (Quest or `-glescompat`) | yes | No: a Quest-specific demo path |
| Models lit at the vertex | Models' light worked out in the vertex shader | no gain on the headset (8.37 vs 8.37 ms) (6ze) | `vr_gl_alias_vlight` 0 (probe) | yes | No: measured nothing |

## 6. CPU side

| Optimisation | What it does | Quest gain measured | Cvar / switch | ES only? | Would it help PC VR / flat? |
|---|---|---|---|---|---|
| Persistent mapped buffers | `GL_EXT_buffer_storage` (Android) gives the frame buffers the PC's persistent mapped path instead of a `glBufferSubData` Adreno copies whole | "models" CPU 370 -> under 1 ms; frame 2 FPS -> ~39 FPS with the world fix (6l) | automatic (`GL_Upload`); safe mode keeps them under ES (6zh) | yes | No: the PC already has `ARB_buffer_storage` |
| Particle and decal caps | Particle screen share, half-resolution particles, decal buckets lowered for fill rate and the decal walk | not measured | `vr_particle_max_screen` 0.08 / 0.06 / 0.05, `vr_particle_halfres` 1, `vr_decal_max` 256 / 192 / 128, `vr_decal_bucket_max` 48 / 32 / 32 | no | Maybe: user settings; half-res particles may help weak PC GPUs |
| Physics and gore caps | Fewer ragdolls, gibs, limbs, debris, dents | not measured | `vr_ragdoll_max` 4 / 3 / 3, `vr_smallgibs_max` 32 / 24 / 16, `vr_limbs_max` 12 / 8 / 8, `vr_explosion_debris_max` 32 / 24 / 16, `vr_gore_dents_max` 6 / 4 / 4 | no | Maybe: user settings for low-end CPUs |
| Compile-time and program cache | Programs' binaries cached per driver; queued compile with a loading panel | cold 65.1 s vs warm 8.3 s to e1m1's first frame; 103 programs from binaries in 126 ms (6k); later one set, 70 from the cache, first frame 5.3-6.7 s (6zk) | `vr_program_cache` 1 on ES (2 also on PC); `-shadersasync` | yes | Maybe: PC saved ~20 ms warm (off by default); of little value there |
| Armed humanoids' CPU | Investigated: QuakeC props and relink ~1.5 ms of a 4.4 ms fight frame; nothing cut | not an optimisation (6d) | none | no | Likely: a real cost on PC too (its own item, not yet done) |

## 7. Runtime and OpenXR

| Optimisation | What it does | Quest gain measured | Cvar / switch | ES only? | Would it help PC VR / flat? |
|---|---|---|---|---|---|
| Refresh rate choice | `XR_FB_display_refresh_rate`: 72 Hz default, 90 Hz as a harder level | 72 Hz held at e1m1's start with a 13.89 ms frame (6o); 90 Hz level lowers scale to 0.75 | `vr_xr_refresh_rate` 0 = 72 (`vid_refreshrate` is ignored on the Quest) | yes | No: PC runtimes set their own rate |
| CPU / GPU performance levels | `XR_EXT_performance_settings` levels | `vr_xr_gpu_level 4` + `vr_xr_cpu_level 4`: 11.21 against 11.40 ms, noise (6zl); not in the preset | `vr_xr_cpu_level`, `vr_xr_gpu_level` | yes | No: Quest runtime concept |
| Fixed foveation (runtime) | `XR_FB_foveation` on the eye swapchains | no gain through the post-process path (world 9.1 ms at 0.9, 6y; 9.7 ms at launch, 6x); crash guarded behind `-xrfoveation`; "direct" path: no effect before the crash (6w) | `vr_xr_foveation` 0 (needs `-xrfoveation`) | yes | No: the PC's own foveation (`vr_foveated`) already exists; not applicable here |
| Hidden-area (visibility) mask | The lenses' hidden area is not drawn | `vr_visibility_mask 0` costs +0.3 to +1.0 ms at 3 of 4 places, so it helps (6zl) | `vr_visibility_mask` (existing) | no | Likely: already on; keep it enabled for PC VR |
| Perf recorder | Per-frame CSV of phases, events and hitch causes | not an optimisation (6u) | `vr_perf_record` 0 | no | Likely: a tool that would find PC hitches too |

## 8. Tried and left off (no measured gain)

- QCOM texture foveation on our scene targets: no effect (bins, GPU, image the same) (6o, 6v); depth foveation made framebuffers incomplete.
- Separate right-eye scene targets: no gain (6o).
- The lights' lite form (`vr_gl_lights_lite`): costs -0.6 to -0.9 ms in the fight, default 0; C4l's -3.7 / -2.5 / -3.2 were launch variance (6t).
- `interpolateAtOffset` workaround for the derivative bug: +5.4 / +7.8 ms, dropped (6t). NORETRO: never (6t).
- Models lit at the vertex (`vr_gl_alias_vlight`): no gain (6ze). Skin form: +0.02 ms (6ze).
- PBR off on the world: within noise (6zl). CPU/GPU level 4: -0.2 ms noise (6zl).
- A fast-light path inside the full loop (`FASTLIGHT`): 12.3 with 3 lights against 11.8 for the lite form, slower with it (6w).

## Candidates for PC

- **Models' plain / gore / skin forms** (per-batch feature specialisation): the largest model-side Quest gain (19.8 -> 17.9 ms, 12.90 -> 11.76 ms live); an A/B on a many-monster scene on a mid-range PC GPU shows whether ALU or registers matter there.
- **Plain lights loop with shadows off** (`vr_gl_lights_plain`): world 14.7 -> 7.1-7.6 ms with 3 big lights on Adreno; test the same big-explosion scene on the PC with the shadow code compiled out.
- **Unlit world and the unlit/lit split**: skipping the lights' loop where no light reaches is cheap logic; check explosion and torch scenes on PC and the cull's CPU cost.
- **R11F_G11F_B10F float scene (`vr_tonemap 2`)**: -1.4 ms on the Quest at the same picture; scene bandwidth is also a PC VR cost at high render scales.
- **Dynamic resolution with a low floor**: the Quest's biggest practical lever (e1m1 at the floor 86% of the time); a PC VR preset with dynres on and a tuned floor is a cheap experiment.
- **Lite world compile-out of dead features** (bump, AO, caustics, retro light, detail): world 12.9 -> 8.2 ms on Adreno; on PC a weak GPU on the Low preset could try it, at the price of shader remakes per setting.

## This round (C5c, 2026-11-21)

| Optimisation | What it does | Quest gain measured | Cvar / switch | ES only? | Would it help PC VR / flat? |
|---|---|---|---|---|---|
| Bare forearms 144 sides only at the wrist | the body's bare arms 24-sided up to the last forearm ring, 144 at the wrist and lip (stitched): 10,874 -> 4,154 triangles | frozen fight span 3.71 -> 3.55 ms (unworn view) | none (the mesh) | no | Yes: PC body 0.099 -> 0.072 ms a pass, same look |
| Body pre-pass switch | the body's depth pre-pass off: skinned once a view | +0.33 ms (worse): stays on | `vr_alias_prepass_body` 1 | no | No: PC 0.091 vs 0.072 ms |
| Batched dynamic-lightmap uploads | the frame's lightmap changes uploaded per 16-row band from a CPU copy (141 -> 16 calls) | 3 big lights, dlmap on: GPU 5.47 -> 3.96 ms, CPU 6.61 -> 1.81 ms | `vr_gl_world_dlmap_batch` 1 | yes (dlmap is ES only) | No (the PC keeps the loop) |
| Dynamic lights in the lightmap (C5b), measured | the world without the lights' loop | 3 big lights: GPU span 9.79 -> 3.96 ms (+1.8 ms CPU) | `vr_gl_world_dlmap` 0 (his look call) | yes | Maybe for weak PC GPUs; the look is Quake's 16-unit grid |
| Loads past a throttling runtime | host frames run between throttled `xrWaitFrame`s while signing on | (a load, not a frame cost) | none | no | Yes: the same OpenXR backend |

## This round (C5d, 2026-11-21)

| Optimisation | What it does | Quest gain measured | Cvar / switch | ES only? | Would it help PC VR / flat? |
|---|---|---|---|---|---|
| (finding) the body's worn cost | its fragment shading, by the pixels it covers: pitch 35 +1.9 ms, pitch 70 +11.9 ms span at 1.0; its light ~4 of ~5 ms at 0.7 (dynamic lights on it 1.85, ambient/bumps 0.9), its look 0.15 | (a cause, no cut yet: C5e) | `vr_debug_head_pose` (test aid) | no | No: PC body 0.07 ms a pass, the eyes within 0.02 ms |
| Model dynamic lights per pixel off | `vr_dlight_models 0`: Quake's flat add for every model | -1.85 ms with the torso in view (pitch 70, 0.7) | `vr_dlight_models` (his config 1; not in the Quest preset) | no | No (a look change) |
| Body lit at its vertices (C5e) | the body's skin form with the own light and dynamic lights per vertex | **slower**: pitch 35 8.09 -> 11.24 ms, pitch 70 13.31 -> 16.80 (0.7) | `vr_gl_alias_body` 0 (a probe) | yes | No |

## This round (C5g, 2026-11-22)

| Optimisation | What it does | Quest gain measured | Cvar / switch | ES only? | Would it help PC VR / flat? |
|---|---|---|---|---|---|
| Dynamic lights in a light texture | the world's lights summed on the CPU once a frame at the lightmap's grid, added by the unlit world form as bright as the loop | frozen fight, 3 big lights: 11.33 -> 7.00 ms GPU, 72 Hz held, +1.5 ms CPU | `vr_gl_world_dlmap 1` (Quest preset) | yes (the unlit form) | no (the PC's world has the loop's look) |
| Body lights Quake's way | the body's dynamic lights added flat on the CPU | **slower**: +2.6 ms (pitch 70), +2.8 (35) | `vr_gl_alias_body 2`, `vr_gl_quake_dlights` | yes | - |
| vr_menu.cpp at -Os (build) | the NDK's post-RA scheduler skipped on the menus' file | APK 18+ min -> 5.6 min | android.cmake | Android | - |

## This round (C5h, 2026-11-22)

| Optimisation | What it does | Quest gain measured | Cvar / switch | ES only? | Would it help PC VR / flat? |
|---|---|---|---|---|---|
| Body lights Quake's way, its usual form | the body's dynamic lights added flat on the CPU, its batch's light loop skipped in the form it takes anyway (C5f's own form ran the same code ~3.3 ms slower) | posed head 0.7: pitch 70 11.65 -> 10.63, pitch 35 8.71 -> 7.98; fight +0.1 | `vr_gl_quake_dlights 1` (Quest preset) / `vr_gl_alias_body 2` | yes (the CPU add's switch) | Maybe: one loop fewer per pixel on your body; small on a desktop GPU |
| 24-bit eye depth | D24S8 instead of D32F_S8 for the eyes | pitch 0 -0.37 ms, pitch 35 -1.19 (in-session, 4 rounds) | `vr_depth_float 0` (Quest preset; PC 1) | no | No: the PC keeps the float depth's small near plane |
