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
| ES struct copy unrolled | Struct copies out of an SSBO and `gateinverse[3]` rewritten member by member (Adreno copied whole 240-byte `Light` to scratch) | frame 383 -> 140 ms (6l) | `vr_glsafe.cpp expandStructCopies / appendMemberCopies`; `-glesdebug structcopy` undoes | yes | No: a driver bug workaround, not a PC cost |
| No `centroid` | Dropped from the ES shader text (Adreno interpolated other inputs wrong) | a correctness fix, no timing | `-glesdebug centroid` keeps it | yes | No: PC driver fine |
| World textures in arrays | World textures grouped by size in `sampler2DArray`s, one bind per group | world draws per eye 118 -> 25 (idle e1m1, mock on PC; GPU/CPU no change on PC); Quest CPU gain "not measured" (6e) | `vr_world_texarrays` 1 on ES, 0 PC (2 = any bound path) | yes (default) | No: the PC has bindless; useful only where draws are costly |
| Bindless off (side result) | The bound path in place of bindless | -0.1 to -0.37 ms where many textures draw, on the PC under ES (6b) | `-glesdebug keep:bindless` to undo | yes | No: it is the fallback, not a gain |
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
| Lit textures' walk from a leaf grid (questperf) | R_WorldLitTextures (the split's skip, C4w) tests only the leaves in grid cells round each light, once, and reuses the answer for the second eye | 0 on the Quest at Standalone (the lights go into the lightmap: no split view walks); PC mock under ES: the hub 0.865 -> 0.080 ms CPU | `vr_gl_world_lit_reuse` 1 (0 the old walk, 2 both compared) | no (the split is ES's) | No: the PC has no split |

## 7. Runtime and OpenXR

| Optimisation | What it does | Quest gain measured | Cvar / switch | ES only? | Would it help PC VR / flat? |
|---|---|---|---|---|---|
| Refresh rate choice | `XR_FB_display_refresh_rate`: 72 Hz default, 90 Hz as a harder level | 72 Hz held at e1m1's start with a 13.89 ms frame (6o); 90 Hz level lowers scale to 0.75 | `vr_xr_refresh_rate` 0 = 72 (`vid_refreshrate` is ignored on the Quest) | yes | No: PC runtimes set their own rate |
| CPU / GPU performance levels | `XR_EXT_performance_settings` levels | `vr_xr_gpu_level 4` + `vr_xr_cpu_level 4`: 11.21 against 11.40 ms, noise (6zl); not in the preset | `vr_xr_cpu_level`, `vr_xr_gpu_level` | yes | No: Quest runtime concept |
| Fixed foveation (runtime) | `XR_FB_foveation` on the eye swapchains, the eyes drawn straight into them (form 2), one render pass an eye (`vr_gl_one_pass`, qfov3) | the gib fight at 0.65: Low 13.6-14.1 ms vs 15.9-17.3 through the post-process (3 launches); qfov4, clean: High 12.9-14.1, Medium 14.4-14.9, Low 14.2-15.2, none 16.6-18.2; at 0.9 High 14.6-15.2, Low 23.9-25.0; no crash at launch, level changes or mid-game changes (qfov2/qfov3) | `vr_xr_foveation` 3 in Standalone on the Quest 3, 1 on the Quest 2 (0 elsewhere; no `-xrfoveation` guard since qfov3) | yes | No: the PC's own foveation (`vr_foveated`) already exists; not applicable here |
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

## PC results (pcopt, 2026-11-22)

Method: the PC's real GPU (desktop GPU, the author's game closed: no low-impact runs), the dev Release exe (the GPU
times do not depend on the CPU build), one launch per scene and mode, every config of a set measured 3 times
interleaved (5 at 3072), each `vr_profile` over 120 frames after a 30-frame settle and a warm-up of every config's
shaders; still scenes (`sv_freezenonclients 1`, `vr_particle_freeze 1`, seeds 7): **fight** 16 grunts on vrfiringrange,
**lights** e1m1's start with `vr_test_dlights 3`, **e1m1** its start, **torch** e1m2's start. VR: the mock headset's
eyes at 2048 (and 3072, the second pass); flat: `-flatscreenmode` at its window. Pictures: the mirror's screenshot per
config against the base's (pixels differing by more than 8 in a channel; VR still scenes repeat to 0.00%, the flat
view does not: its diffs are noise). `scratch/pcopt_ab.py`, `pcopt_sum.py`, `pcopt_png.py`. The A/B switch for the
ES-only program forms is new: **`vr_gl_pc_forms`** (bits: 1 models' plain/gore/skin, 2 plain lights, 4 unlit world
and split, 8 lite world; default 0; Debug > Tunables "Quest Shader Forms on PC"). GPU frame ms (both eyes), mean of the
rounds (rounds spread +-0.3 to 1.0 ms at 2048: this GPU runs these scenes in 1.2-3.4 ms, so small figures are noise):

| Item | Setting tried | VR 2048: fight / lights / e1m1 / torch | VR 3072: lights / fight | Flat: fight / lights | Same picture? | Verdict |
|---|---|---|---|---|---|---|
| A1 models' forms | `vr_gl_pc_forms 1` | +0.06 / -0.67 / +0.30 / +0.33 (alias scope 0.11 -> 0.10-0.11) | +0.05 / +0.00 | -0.10 / +0.09 | yes (0.00%; fight 0.28%, its ragdoll noise) | off: no gain (the models cost 0.1 ms here) |
| A2 plain lights (shadows off, Low) | `vr_gl_pc_forms 2` | lights -0.11, torch +0.08, fight +0.02 (world scope the same) | - | lights 0.00 | yes | off: no gain; the run-time form (shadows on, none casting) not built |
| A3 unlit world + split | `vr_gl_pc_forms 4` | -0.02 / -0.43 / +0.23 / +0.01 (world scope the same; CPU busy within noise) | +0.07 / +0.01 | -0.13 / +0.09 | yes | off: no gain; the cull's CPU cost unmeasurable here |
| A4 32-bit float scene | `vr_tonemap 2` | -0.18 / -1.00 / -0.02 / +0.16 | -0.33 / -0.33 (2.59 -> 2.25, rounds within 0.01) | -0.10 / +0.21 | yes (max 4-8 levels, no banding seen at that threshold) | **on for PC**: `vr_tonemap` default 2 (the Low-Ultra presets' D); Tone Mapping rows now Off / On (64-bit) / On |
| A5 lite world (Low) | `vr_gl_pc_forms 8` | lights -0.09, torch +0.08, fight +0.03 (world -0.03 to -0.04) | - | lights 0.00 | yes | off: no gain; would remake the shaders per setting |
| A1-5 all forms (Low) | `vr_gl_pc_forms 15` | lights -0.24, torch +0.03, fight -0.14 (world -0.07, alias -0.04) | - | lights -0.02 | **no**: 0.9-9.7% of pixels differ at Low (not with any one bit alone): to look into before any use | off |
| A6 framebuffer invalidation | `vr_gl_invalidate 1` | +0.03 / -0.28 / +0.37 / +0.54 | -0.05 / +0.08 | -0.04 / +0.09 | yes | off: harmless, no gain on a desktop GPU; no integrated GPU here to try |
| A7 eye sub-image | `vr_eye_subimage 2` (render scale 0.8, bilinear) | - | +0.04 against 0 / -0.10 against 0 (1.98 -> 1.88) | - | yes (the same as sub-image 0) | off: a small gain on the mock (fight -0.10, lights noise); whether a real runtime honours `imageRect` not tried (no headset runs) |
| B Low trims | particle screen 0.08, torch/lava nail/debris lights 2 | fight +0.10, torch +0.01 | - | - | fight 0.29% (particles) | not added: no gain on this GPU |
| C dynres floor per preset | not run (needs a real headset's frame timing: the mock's fixed frames never miss) | - | - | - | - | defaults unchanged |
| D already shared | decal bake scissor, posed batching, alias sort key fix, visibility mask, forearm mesh, loads past a throttling runtime, perf recorder | on for PC since the merge (questmerge) | | | | confirmed in the code: shared paths, no ES gate |

Headline: on this desktop GPU none of the Quest's GPU optimisations wins clearly at the same look: the scenes cost
1.2-3.4 ms a frame and the differences sit inside the rounds' spread. The one exception is the 32-bit float scene (`vr_tonemap 2`): -0.33 ms (-13%) in both GPU-bound 3072 scenes, the same picture, so it is now the PC default (a saved config keeps its 1). Nothing else is turned on for PC;
`vr_gl_pc_forms` stays as the A/B switch for a weaker GPU.

## Quest gib fight (questperf, 2026-11-23)

The frozen e1m1 gib fight up close (questgore's scene: six grunts 56-90 units ahead, `vr_limb_test 10`, frozen), on
the Quest 3 under the Standalone preset (72 Hz) at a fixed 0.65, the dynres floor where questgore's frames sat; this
branch's APK (the quest tree's key) and test progs. GPU ms a frame, both eyes, medians of 2 interleaved parts of 240
frames each (one launch); "missed" is the share of frames the runtime reports late (`xr_missed`). Leaving things out
uses `vr_debug_hide_models` (new, Debug > Profiling: the Quest's GPU timer scopes inside one render pass mean nothing,
so a cost is found by its absence). Launch-to-launch the base moved 15.4 -> 18.1 ms over the session (the headset
warming), and parts of one setting within a launch by +-1.5 ms (draws 183-244 from the gibs' random cuts; fixed seeds,
`sv_random_seed 7; vr_particle_seed 7; vr_map_seed 7`, from the third A/B on): only gaps of ~2 ms or more read.

| Left out / changed | GPU (base) | Missed (base) | Draws | Reading |
|---|---|---|---|---|
| nothing (base) | 15.77 | 21.4% | 215 | 72 Hz misses one frame in five |
| every entity (`r_drawentities 0`) | 10.60 | 0.2% | 114 | the models ~5.2 ms |
| the world (`r_drawworld 0`) | 7.83 | 1.6% | 188 | **the world ~8 ms**: the top cost |
| your rig (body, hands, held guns, gear) | 13.27 | 2.1% | 168 | **your own models ~2.5 ms**: the second |
| the gore (gibs, heads, limbs) | 13.87 | 6.6% | 198 | **~1.9 ms** (round 2 split: gibs alone, limbs+heads alone each within the noise) |
| dropped guns, packs, the grunts' rig and armour | 15.22 | 15.8% | 213 | ~0.55 ms |
| particles (`r_particles 0`) | 16.13 | 22.0% | 217 | nothing (questgore: blood ~1 ms at most) |
| render scale 0.55 (round 2, base 15.38) | 12.04 | 1.1% | 219 | **holds 72 Hz** (both parts 12.0) |
| render scale 0.6 (third launch, base 17.22) | 17.46 | 31.4% | 233 | nothing: as 0.65 |
| render scale 0.58 (fourth launch, base 18.08) | 16.66 | 27.9% | 231 | -1.4 ms |
| anisotropy 1 or 4 (round 2) | 17.85 / 17.40 | 36.5% / 29.7% | 221 / 217 | nothing seen (the world's arrays took the value of the part before: not clean) |
| no world depth pre-pass (`vr_world_prepass 0`, new) | 15.78 vs 18.03, then 17.39 vs 17.28, 18.61 vs 17.22 | - | - | **no gain** over three launches (first launch's -2.2 was the gibs' random cuts): not in the preset |
| straight into the headset's images with fixed foveation high, form 1 (qfov: `+vr_xr_foveation 3 -xrfoveation` at launch, `vr_retro 0; vr_retrolight 0`; tone curve in the shaders, in-tile grade) | 14.62 vs 15.01 (P90 14.96 vs 16.30) | 12.1% vs 21.4% | 206 vs 210 | -0.4 ms: the post-process's cost, not foveation's (6w's form 1 the same); form 2 (own depth renderbuffer) with foveated images **crashed the runtime in xrEndFrame at once** (SIGSEGV 0x30 in libGLESv2_adreno under libvrapiimpl), with or without the in-tile pass; without foveation it runs. No default changed |
| (qworld) retro textures + retro lighting on (`vr_retro 1; vr_retrolight 1`, a fresh install's defaults), the world's unlit decal-retro twin (new) | 28.91 vs his config's (both 0) 13.28 | 98.7% vs 2.6% | 296 vs 189 | **retro costs ~15 ms here**: Standalone now sets `vr_retro 0`, `vr_retrolight 0` (his own Quest config's) |
| (qworld) the same, the old lit "world decal retro" program (`vr_gl_world_unlit 0` restores it) | 39.07 vs 28.91 | 98.9% | 211 | **-10.2 ms**: a retro blood mark no longer sends the world to the lights' loop (`GL_WorldUnlitDecalRetroProgram`) |
| (qworld) light contrast 2.5 as `x*x*sqrt(x)`, decals read member by member (old: probes `QVR_PROBE_OLDPOW,OLDDECAL`) | retro off 14.11 vs 14.22; retro on 28.91 vs 29.69 | 9.9% vs 10.0% | 213 | -0.1 / -0.8 ms: kept (exact) |
| (qworld) colours and light terms at mediump (`vr_gl_mediump 1`; world alone: probe `QVR_PROBE_MPNOALIAS`) | 17.20 (world alone 17.09) vs 14.11; launch 1: 17.11 vs 15.61 | 31.9% | 215 | **+3 ms worse** on Adreno (two launches): default 0 |
| qtune (retro off in both): your rig plain (`vr_rig_plain` 1), no fullbright read (`vr_gl_alias_nofb`), gore flat-lit and no lights' loop (`vr_gl_alias_flat`), vs none | 14.77 vs 15.84; 15.36 vs 16.17 | 15.2% vs 28.2%; 21.6% vs 23.7% | 215 / 214 | **-0.8 to -1.1 ms**: in the preset (the look on the headset not yet checked) |
| qtune: the no-fullbright and no-loop variants alone (rig plain in both) | 15.11 vs 15.59 | 18.3% vs 21.4% | 211 / 214 | ~-0.5 ms (so the rig's plain form ~-0.3 to -0.5) |
| qtune: dynres on (0.9 ceiling, 0.65 floor), the fight floor 0.55 (`vr_dynres_min_fight`) vs none | 13.36 vs 15.5-18.1 | 4.7% (20% with a load hitch) vs 19-37% | - | **near 72 Hz in the fight**; the e1m1 walk never left the 0.9 ceiling (0% of frames below 0.65) |
| qtune: settled gore trimmed (`vr_gore_trim_max` 24, 5 s, none within 160) | not measured: by design it leaves the close-up fight alone | - | - | far gore after a fight; the gore was already instanced (gib1 76 instances in 2 batches) |
| qfov2: form 2 rebuilt as Meta's samples (a framebuffer per eye image, the image attached for good, depth at the image's size), one launch, 2 parts each interleaved, level changed mid-game (new swapchains): P post-process, D straight in no foveation, L/M/H levels 1-3, R level 3 refocused | P 16.34, D 17.00, L 17.31, M 17.98, H 17.43, R 17.87 | 95-99% (a warm headset) | 208-213 | **no crash** (form 2 + foveation at launch and every mid-game change), **no gain**: the trace shows each eye as 6 render passes (the brush models' compute gathers between draws end the pass); only the first (the clear, hidden area) has scaled bins, the main pass (6-9 ms) loads colour and depth and runs every bin at 1/1. Form 2 also bins the whole 1680x1760 image while 0.65 of it is drawn. No default changed |
| qfov3: one render pass an eye (`vr_gl_one_pass` 1: ES's brush models drawn from the cull's own commands, no gather; the arrays' merges dispatched before the eye's clear from the last frame's plans), 3 launches x 2 interleaved parts each: Pg post-process + gather (today), Po post-process one pass, Dg/Do straight in, no foveation, gather / one pass; L/M/H levels 1-3 one pass; R H refocused; Y H + dynamic; E H with eye images at 0.65 (`vr_xr_eye_scale`, now live on the Quest) drawn whole | Pg 17.33/15.86/16.31, Po 15.98/15.98/16.29, Dg 17.00/17.19/17.11, Do 16.90/16.89/16.61, **L 13.61/13.78/14.06**, M 14.33/14.24/14.05, H 15.39/15.47/15.53, R 15.98/17.30/15.49, Y 13.74/14.06/13.48, E 15.45/15.58/15.56 | Pg 95-98%, L 67-76%, Y 63-73% | 205-213 (all) | the trace: each eye one render pass (no LoadColor/LoadDepth, no depth store), its bins foveated (was 6 passes, the main one unfoveated); only the drawn part's bins rendered (5 of 8), so no scissored clear was needed. One pass alone: -0.0 to -1.4 ms. Never merging (`vr_gl_one_pass 2`) doubled the draws (192 -> 429) and cost 1.2-3 ms. **Standalone now sets `vr_xr_foveation` 1** (Low: the fastest and the mildest; High slower than Low in all three launches). Eyeshots: the periphery's floor at L and M shows no visible blocks; CPU (busy) unchanged ~9.7-10.3 ms |
| qfov4: the levels measured clean (every part on new swapchains; 3 launches x 2 interleaved parts): D straight in, no foveation; L/M/H; Y H + dynamic; LR/MR/HR refocused by the game (removed after); then H and L at 0.9, H at 0.78, H without the in-tile pass (G), H without particles (Q) | D 18.15/16.64/17.54, L 14.87/15.19/14.21, M 14.37/14.70/14.94, **H 14.10/12.90/13.33**, Y 14.47/13.89/13.58, LR 18.16/17.84/17.07, MR 17.25/16.59/16.34, HR 17.16/16.01/16.33; block 2: L 14.09/14.12/13.78, H 13.26/13.21/13.13, **H at 0.78 13.00/13.14/12.76**, H at 0.9 14.58/15.16/14.68, **L at 0.9 25.04/24.74/23.91**, G 12.75/12.88/12.98, Q 12.99/13.10/13.23 | L 75-82%, H 60-74%; block 2: L 63-72%, H 57-60%, H at 0.78 53-57%, H at 0.9 80-89%, L at 0.9 99% | 186-213 (all) | qfov3's "High slower" was High measured on images still carrying the refocus part's focal points. The driver foveates by whole 864 x 448 bins (1/1, 2/1, 2/2); the runtime centres them on the rect submitted. At 0.65 Low = Medium (the same bins), High two bins more of the left eye; at 0.78-0.9 High reduces the bottom bin row and Low next to nothing. `vr_xr_foveation_refocus` removed. **Standalone now sets `vr_xr_foveation` 3 on the Quest 3** (the Quest 2's column stays 1). LRZ kills nothing in any path (open) |
| qlrz: LRZ ("LRZ Pixels Killed" 0 in every pass): a depth-writing draw with GL_BLEND on ends it for the pass, and Ironwail never disables GL_BLEND (opaque = ONE, ZERO). `vr_gl_opaque_noblend` 1: opaque draws unblended (the same pixels on the PC). 3 launches x 2 interleaved parts: A blended, B unblended, C unblended without the world pre-pass; fixed 0.65, then dynres as shipped | A 13.84/13.49/13.34, B 13.63/14.69/13.21, C 13.29/13.61/13.52; dynres A 12.58/12.72/12.60, B 12.78/12.44/12.39, C 12.06/12.22/12.34 | - | 200-206 | **no gain yet**: with it LRZ lives through the hidden area and the world (the probe's quad killed: 693k / 846k pixels an eye) but some id model other than a grunt ends it again in the opaque entity pass (open: the discarding forms suspected), and the world's own draws write none. Default 0; neither clip control, the pre-passes, the hidden area, particles, the in-tile pass, the invalidate, the depth format nor sample shading is a cause |
| qeye 1: the eye's part placed in the middle of its image (`vr_eye_rect_place` 1) vs the corner (0), High, his look (grade 0, dither 1); 3 launches x 2 interleaved parts: B/C 0.65 corner/middle, D/E 0.78, F/G 0.9 | B 12.72/13.04/12.96, **C 13.99/15.88/13.99**, D 12.66/12.91/13.15, **E 15.94/15.19/15.39**, F 14.75/14.51/14.37, **G 17.21/16.55/16.86** | B 54-57%, C 71-88%; D 53-57%, E 90-95%; F 79-86%, G 97-98% | 199-204 | **the middle is slower everywhere (+1.0..2.8 ms)**: the trace shows the runtime's full bins round the rect's centre, which then sits on the grid's middle lines (x 864, y 896): 4 full bins an eye at 0.65 (2 in the corner) and 2.8-3.3 M fragments vs 2.3-2.7 M. The bins (864 x 448, 2 x 4) are too coarse for a placement to help; the corner stays (default 0; the cvar kept for another runtime) |
| qeye 2: the scene's shaders dither straight-in eyes (`vr_dither_in_scene` 1: the post-process's noise in QvrScreenDither), the in-tile pass skipped, vs the in-tile pass dithering (A), High, 0.65, corner | A 13.15/13.00/12.74 (one valid part a launch: each launch's first part is its warm-up), B 12.72/13.04/12.96 | A 51-58%, B 54-57% | 199-205 | **no measurable change** (within the launches' noise; qfov4's no-in-tile G was 0.15-0.5 ms under H): the hash in every scene fragment (~4 a pixel) costs about what the pass saved. Kept on (one pass fewer; the look the same: PC eyeshots mean 0.076/255 from the post-process's dither). The pass still runs for bullet time's look (checked on the headset), the comfort vignette and the grade (`vr_grade` 1, a new player's) |
| qlrz2: what ends LRZ in the opaque entity pass (blending off; traces, not timings): depth clamp off, your rig, the body, gibs, heads or guns hidden alone, the clamped models drawn last, no motion-vector store; only every alias model hidden keeps it | - | - | - | **not found**: LRZ pixels killed 0 in each (2.7-3.3M with every model hidden); the world feeds LRZ through its pre-pass only (1.05M vs 34). No timing run, no default changed |

Counts (PC mock, `-glescompat +vr_standalone_preset 0`, `vr_gl_passes`): 240 draws a frame, 88 of them alias (48
batches, 290 instances, 72k alias triangles both eyes); the Quest's own frame 192-244 draws. Gibs 76 + 76 + 12
instances, limbs 4 x 12, heads 12 (both eyes; 28-110 triangles each): the gore is many tiny draws, not triangles. The
render stage trace (`ovrgpuprofiler -t`, 0.65): each eye's scene is 4 bins of 576x576 (colour 32-bit, D24S8); the
left eye's main pass 5.2 ms, the right's 6.7 (0.45 of it the compositor's preemption, ~0.5 ms three times a frame);
fragments shaded ~1.8 a pixel in the main pass, which includes the world's depth pre-pass (so no overdraw problem left
to take). Verdict: the scene is fill-bound at the preset's floor (0.55 holds 72 Hz, 0.6 and 0.58 do not reliably); no
look-keeping cut found that reaches 72 Hz. No default changed.

## CPU in fights (qcpu, 2026-11-23)

Where a big fight's CPU goes and what was cut, keeping gameplay identical. Scenes (qcombined's): **C** 48 mixed
monsters on vrfiringrange (grunts, ogres, knights, scrags; god mode), **E** the live e1m1 fight (two waves of six
grunts, gibbed). PC: Release (no LTO), mock headset (eyes 2048) or `-flatscreenmode`, the Standalone preset, unpaced
fixed frames, `vr_profile` trees and `vr_qcprofile` (600 frames), a Release-Tracy capture of C. Quest 3: Standalone as
a new player gets it, the perf recorder (`c_` phases), this branch's APK with a temporary A/B switch, the two orders
interleaved in each of 2 launches (a third skipped: another app was in front).

Top CPU costs, ms a frame (main thread unless said):

| # | PC VR, C (busy 5.4) | PC flat, C (busy 5.2) | PC VR, E (busy 3.5) | Quest, C (busy 25-27; qcombined's CSV) |
|---|---|---|---|---|
| 1 | QuakeC 1.70 (armed humanoids' weapon props 1.12: pose 0.59, the shot/throw scans 0.30, parry 0.16) | QuakeC 1.38 | QuakeC 1.15 (props 0.96) | SV_Physics 8.6 (QuakeC most of it, by the PC's share ~75%) |
| 2 | alias set-up 1.07 (eye L 0.87, R 0.20) | SV_Physics' own 1.10 (monster moves, traces) | Box3D 0.40 | alias set-up 4.0 |
| 3 | view entities 0.46 (holsters, ground spots, blood streams) | alias 0.62 | alias 0.33 | eye L's rest ~2 (world, lights, particles) |
| 4 | relink: armed arms 0.45 (the solves on jobs: 0.62 a frame over the threads) | view entities 0.58 | relink: armed arms 0.23 | view entities 2.2 |
| 5 | Box3D 0.29 | relink: armed arms 0.38 | view entities 0.28 | eye R 2.2 |
| 6 | findradius 0.24 (108 calls) | hands 0.35 | particle light 0.14 | sound 1.45 |
| 7 | vr particles 0.15 | Box3D 0.20 | box3d water and hits 0.12 | client read (relink) 1.36 |
| 8 | SV_Physics' own 0.15 | vr particles 0.16 | gib trail 0.03 | vr particles 0.66 |
| 9 | setup view (decals) 0.06 | send 0.09 | send 0.06 | lights 0.57 |
| 10 | send 0.10 | client send 0.07 | client send 0.04 | xr submit 0.59 |

QuakeC's builtins, C: findradiusmoving 0.19 (the props' two scans, 90 calls), traceline 0.04, findradius 0.04,
modelanchorpoint 0.03, movetogoal 0.02; no other QuakeC function over 0.13 ms self. The Quest runs the same QuakeC ~4x
slower (SV_Physics 8.6-10.5 vs 2.2) and the client ~5x (screen 11.3 vs 2.1).

| Change | PC (C, 600 frames) | Quest C: old / new (2 launches) | Quest E: old / new | Same gameplay? |
|---|---|---|---|---|
| findradiusmoving's mover test in the search, before the grid's sort (the props' scans found ~52 a call, nearly all standing monsters and props, sorted only to be dropped) | findradius 2.20 -> 1.15 us a call, 0.238 -> 0.124 ms a frame | findradius 5.21, 5.36 -> 3.46, 3.31 us a call; 0.585 -> 0.358 ms a frame; SV_Physics 10.50 -> 9.82, CPU busy 27.36 -> 26.30 (the launches' spread ~2 ms) | 3.05, 3.01 -> 2.24, 2.24 us; 0.445 -> 0.326 ms; busy 17.66 -> 17.20 | yes: the same entities in the same order (a subset of a sorted set); vr_agrunt_test 50-60, 93 pass |
| (qcpu2) the armed humanoids' weapon pose (VR_AGP_Pose and its ~45 helpers) in the engine: `agpose`, vr_builtins_agpose.inc, the same float steps on QC's globals and fields, its gameplay hooks (blows, parries, the chainsaw's engine) still QuakeC's; `vr_agpose_native` 1 (0: QuakeC, 2: both compared) | the pose (incl. its QC hooks) 0.559 -> 0.103 ms; QuakeC 1.575 -> 1.019 ms; SV_Physics 1.55 -> 1.17; CPU busy 3.72 -> 3.29 (VR mock). Flat: pose 0.544 -> 0.120, QuakeC 1.537 -> 1.153, busy 4.62 -> 4.08. E (VR): pose 0.378 -> 0.071, QuakeC 1.392 -> 1.039, busy 3.16 -> 3.04 | 3 launches, interleaved (means; per launch in ROUND25): c_server 11.09 -> 9.67 ms, CPU busy (median) 25.20 -> 23.35, period median 26.52 -> 24.73, late ~100% both (still CPU-bound), GPU 19.0 -> 18.0 | c_server 6.54 -> 5.75, busy 15.95 -> 15.11, period 17.96 -> 16.87, late 80% -> 75%, GPU 12.0 -> 12.0 | yes, bit for bit: vr_agpose_native 2 over C, E, C flat and 8 extra grunt waves (100,318 poses: guns, melee, two weapons, gun and blade, pickups, reloads, kicks, parries met, shoves) and the vr_agrunt_test group (213 cases): every vr_*/v_*/trace_* global, the humanoid's and its weapons' fields and the message equal; MP armed/disarm compared on the server too |

Left (by size, for the next CPU job): alias set-up on the Quest (4 ms: per-model state and uploads), the view entities
(2.2 ms Quest), sound (1.45 ms Quest); the props' other QuakeC (the shot and throw scans VR_AGS_Projectiles and
VR_AG_WeaponStruck ~0.3 ms PC, the parry's frame VR_AGPar_Frame 0.16).
## Standalone status, 2026-11-23 (qcombined)

`next` at 7df6f7ec8 (retro off, the unlit decal twin, rig plain, models without fullbright reads or a lights' loop,
the fight floor 0.55, settled gore trimmed, dynres) on the Quest 3, as a new player gets it: `resetcfg` then
`vr_standalone_preset 0` (Android's first run), only the test's seeds and freeze set by hand (and the fixed scale where
named). This tree's APK (the quest tree's key) and test progs; 3 launches with the scenes in changed orders (1, 2, 4),
a 4th his config vs the new player's, a 5th leaving things out between base parts. GPU span ms (both eyes), CPU = the
main thread's busy ms, period ms (13.9 a frame at 72 Hz); "missed" = frames the runtime reports late (`xr_missed`);
">15.3" = periods past 1.1 frames (">13.9" reads ~55% even where nothing is missed: the period's jitter around 13.9).

| scene | GPU med / p90 | CPU | period med / p90 | missed | >15.3 | dynres scale (share of frames) | 72 Hz? |
|---|---|---|---|---|---|---|---|
| e1m1 gib fight, frozen, 0.65 fixed | 17.85 / 18.62 | 11.9 | 18.70 / 21.03 | 35% | 96% | 0.65 fixed | no: GPU |
| the same, dynres as shipped | 14.77 / 15.42 | 10.8 | 15.69 / 17.82 | 13.5% | 62% | 0.55 96%, 0.65 4% | no: GPU |
| the same, 3 big lights (`vr_test_dlights 3`) | 14.77 / 15.52 | 10.7 | 15.78 / 18.06 | 14% | 63% | 0.55 93%, 0.65 7% | no: GPU (the lights cost ~0) |
| live e1m1 fight (6 + 6 grunts, gibbed live) | 11.62 / 13.96 | 16.2 | 17.46 / 26.95 | 31% | 66% | 0.55 56%, 0.65 28%, 0.7-0.85 16% | no: CPU (server 5.4) |
| firing range, 48 mixed monsters (combat_48) | 14.72 / 20.12 | 25.1 | 26.15 / 36.51 | 76% | 99.6% | 0.55 93%, 0.65 7% | no: CPU (SV_Physics 9.8) |
| e2m2, every monster awake (48) | 10.54 / 11.49 | 9.3 | 14.12 / 16.74 | 0.2% | 28% | 0.65-0.8 43%, 0.85 34%, 0.9 23% | yes |
| the hub (vrstart's campaign terrace) | 11.41 / 11.61 | 7.8 | 14.07 / 16.00 | 0.2% | 23% | 0.55 100% | yes, at the fight floor |

What dominates the gib fight (0.55 fixed, each part between two base parts, launch 5; base 13.5-14.3): the world
**~8 ms** (`r_drawworld 0`: 5.65-5.81), the gore ~1.0-1.4 (gibs, heads, limbs hidden: 12.87-12.97), your rig ~1.0
(12.52-12.76), the grunts' bodies ~0 (13.48). Dynres's floor costs more than the same scale fixed: 14.8 vs 13.8 ms
side by side (the eye drawn into the lower part of 0.9-sized images). Anisotropy (`gl_texture_anisotropy`, 16 from
vr_defaults.cfg; the preset leaves it): 1 / 4 / 16 = 16.90 / 17.18 / 17.85 at 0.65 (2 launches; 1 vs 16 ~0.5-0.9 ms,
near the noise). His config vs the new player's, same launch, 0.65 fixed, 3 pairs: 17.01 vs 16.96 GPU (CPU 10.4 vs
11.6; 210 vs 222 draws). The 0.65 base is 2-3 ms above qtune's 14.8-15.4 of the morning (same scene and settings): the
headset's warmth or something merged since; not split (no older build in the same launch). Scripts: posebake
`scratch/qc/qc_*` (not committed).
