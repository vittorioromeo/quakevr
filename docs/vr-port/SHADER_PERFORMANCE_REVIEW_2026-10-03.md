# Quake VR / Ironwail shader performance review

Date: 2026-10-03. Source snapshot: `d960645a1c3db20716177efcabd315a3595fb84c`. Final verification at `a9a9979c`: intervening commits changed gameplay/physics/documentation, but none of the shader sources or render call sites cited below.

This is a static shader audit for a separate agent to benchmark. No shaders were changed, compiled, or timed for this review. Rankings describe likely GPU work saved in scenes that exercise the feature; they are hypotheses, not measured FPS improvements. The previous general performance report is complementary: this report focuses on work inside vertex, fragment, and compute shaders and on the data changes necessary to remove that work.

At 120 Hz the total frame interval is 8.33 ms. Savings in an individual shader only improve frame delivery when they reduce work on the frame's critical path. Measure both eyes and the complete GPU frame, including the window view, alongside individual passes.

## Coverage and current settings

The engine embeds GLSL in C/C++ strings and macros. The shader-source inventory covers all engine-owned shader definitions found in `Quake`, their program assembly, and the relevant AMD FSR 1 / NVIDIA NIS vendor paths. The generated vendor `.glsl.inc` files are embedded copies of those headers, not additional independent shader algorithms.

| Source | Stages and functions reviewed |
| --- | --- |
| [gl_shaders.h](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.h:42) | GUI, view blend, warp/scale, postprocess, world, water, sky stencil/layers/cubemap/box sides, alias models, wound painting, sprites, particles, debug, OIT output/resolve; indirect clear/gather, visibility culling, light clustering, palette initialization/postprocess compute |
| [vr_glsl.h](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:32) | Shared shadows, dynamic/baked lighting, normal maps, AO, parallax, specular AA, detail, liquid waves/ripples/foam/refraction/caustics, model ambient/reflections, wounds/morphs, soft sprites |
| [vr_retro.h](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_retro.h:54), [vr_retrolight.h](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_retrolight.h:40), [vr_tonemap.h](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_tonemap.h:39) | Texture blocks/palette filtering, lighting grids/quantization, tone curve and grading |
| [vr_gfx_gl.cpp](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_gfx_gl.cpp:31) | Generic effect modes, CRT/hologram effects, particle vertices, tube vertices, bent mesh vertices/fragments |
| [vr_bloom.cpp](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_bloom.cpp:53) | Bright extraction, downsample, upsample, mean brightness |
| [vr_haze.cpp](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_haze.cpp:47) | Lava/ellipsoid heat-haze projection and sampling |
| [vr_envmap.cpp](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_envmap.cpp:74) | Simplified environment-cube world/lightmap shaders |
| [vr_stereo.cpp](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_stereo.cpp:181) | Mirror projection, Catmull-Rom, underwater/bloom/tone/grade sampling |
| [vr_water.cpp](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_water.cpp:238) | Half-resolution scene-distance construction, including MSAA variant |
| [vr_upscale.cpp](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_upscale.cpp:46) | FSR EASU/RCAS wrappers, lens-region composition, NIS compute wrapper |
| [FSR header](C:/OHWorkspace/quakevr-iw/Quake/vr/external/fsr1/ffx_fsr1.h:315), [NIS header](C:/OHWorkspace/quakevr-iw/Quake/vr/external/nis/NIS_Scaler.h:594) | Active FP32 scaling paths, optional precision/gather paths, shared-memory and workgroup integration |
| [vr_lighting.cpp](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_lighting.cpp:85), [vr_foveated.cpp](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_foveated.cpp:194) | Minimal shadow-depth shader and diagnostic shading-rate overlay |
| [gl_shaders.c](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.c:330), [r_alias.c](C:/OHWorkspace/quakevr-iw/Quake/r_alias.c:362), [r_world.c](C:/OHWorkspace/quakevr-iw/Quake/r_world.c:604) | Compile-time variants, depth/shadow program selection, existing opaque-world depth prepass |

The saved [ironwail.cfg](C:/OHWorkspace/quakevr-iw/quakevr/ironwail.cfg:1171) has parallax enabled with 16 maximum steps, normal maps and baked bump lighting enabled, dynamic AO strength 1.5, and native render scale 1. It also has retro textures/lighting enabled, category bump blending set to 1, category detail contribution set to 0, and fine own wound masks disabled (`vr_wounds_own_res 0`). These are saved settings, not verified live state. Per-category overrides, AB switches, map assets and runtime changes can alter the active paths.

The saved retro shadow filter is 1. `ShadowFilter` overrides the ordinary shadow filter when that setting is active, selecting a single bilinear comparison fetch. Do not infer nine active fetches from the ordinary shadow-filter cvar alone. Native scale also means upscaling recommendations generally have no effect unless native sharpening is enabled or scale is reduced.

## Ranked opportunities

“High” means potentially substantial in its affected pass, not a promise of a large whole-frame gain. Exact-preserving changes can still differ slightly because of floating-point ordering. Approximate changes need moving-head stereo image comparisons, not just still screenshots.

| Rank | Opportunity | Expected effect when active | Nature |
| --- | --- | --- | --- |
| 1 | Reduce parallax height sampling using better traversal or quality tiers | High on nearby detailed surfaces | Algorithm / quality tradeoff |
| 2 | Remove redundant retro auxiliary and zero-contribution detail samples | Medium–high across nearby world/models | Mostly exact, particularly relevant to saved settings |
| 3 | Cache or replace lightmap direction reconstruction | High on maps using the guessed-direction fallback | Data/precomputation; approximation needs validation |
| 4 | Tighten dynamic AO candidates and stop at its visibility floor | High in AO-heavy scenes | Conservative culling / exact floor exit |
| 5 | Reduce bloom sampling at full resolution and in upsampling | Medium–high across both eye images | Filter quality tradeoff |
| 6 | Share world-light vectors and reject outside radii before square roots | Medium with many lights | Exact intent; compiler-dependent savings |
| 7 | Precompute spotlight shadow frames and light constants | Medium with shadowed spotlights | Data/ALU tradeoff |
| 8 | Improve cluster precision to reduce downstream light loops | Medium–high with large overlapping lights | Conservative culling |
| 9 | Cache fine wound relief and remove unnecessary wound noise | High on large wounded own models with fine masks | Conditional; caching approximation |
| 10 | Skip disabled/faded liquid work and tighten ripple rejection | Medium on large liquid surfaces | Mostly exact |
| 11 | Add a small set of feature-specific world/model variants | Medium if register pressure limits occupancy | Compiler/variant experiment |
| 12 | Avoid repeated pose evaluation and optionally cache skinned vertices | Medium for skeletal models drawn in many passes | Mostly exact; bandwidth tradeoff |
| 13 | Use a dedicated alpha-tested shadow fragment shader | Medium in holey-caster shadow scenes | Specialized shader; driver-dependent |
| 14 | Reduce procedural liquid/haze/morph noise work | Medium on affected large surfaces | Quality tradeoff |
| 15 | Reuse surface frames or supply brush tangents directly | Low–medium on feature-rich materials | Data/ALU tradeoff |
| 16 | Precompute light-cluster geometry and transformed light bounds | Low–medium in clustering pass | Mostly exact |
| 17 | Test NIS gather/FP16/workgroups and FSR FP16 | Medium in active upscale pass | Hardware-dependent / precision tradeoff |
| 18 | Reject hidden weapon-morph pixels earlier | Medium during morphs | Requires derivative-safe restructuring |
| 19 | Cache screen/hologram glow and collapse zero-split reads | Low–medium for large/near displays | Exact special case / filter caching |
| 20 | Reduce visibility-compute atomics and serial index emission | Low–medium in large BSPs | Compute/data redesign |
| 21 | Optimize full-screen arithmetic and inactive features | Low–medium, broadly exercised | Compiler-dependent / LUT approximation |
| 22 | Gather depth and avoid unnecessary fragment sampling | Low–medium in selected auxiliary passes | Exact intent with state restrictions |
| 23 | Reduce repeated particle/tube/bent-mesh vertex work | Low normally, higher in stress scenes | Geometry/data tradeoff |

### 1. Parallax: reduce the number and cost of height lookups

Sources: [ParallaxUV](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:521), [world call](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.h:680), [model call](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:1953).

Each active march performs an initial `textureGrad` height lookup, a dependent lookup per step, and usually one additional secant-refinement lookup. A fragment using all 16 steps can therefore issue 18 height-sampling instructions before its final material samples. The loop's static upper bound is 64; actual steps depend on view angle and texture-space travel. Authored model relief uses the world's step budget, while generated model relief uses fewer steps.

Already present: distance/grazing fades, a projected-size rejection, a texel-travel step cap, early intersection exit, face-UV clamping, and secant refinement. Simply adding those features would duplicate existing work.

Test a smaller step budget and a projected-error-driven quality tier first. For preserving the current quality at fewer dependent reads, investigate a precomputed conservative height hierarchy or interval traversal. Additional hierarchy accesses can lose on shallow relief and short rays; evaluate generated and authored height maps separately. A dedicated compact height texture may reduce bandwidth, but adds another resource and may lose locality with the normal texture.

Test compile-time maximum-step variants such as 8/16/32/64 only if generated code shows a problem with the current bounded dynamic loop. Do not force full unrolling by default. Explicit scalar LOD is another experiment, but is not equivalent to `textureGrad` under anisotropic filtering.

**Benchmark:** nearby grazing walls, steep authored hand/weapon relief, 0/8/16/32 steps, fixed head path. Record average loop iterations, texture instruction/stall cost, and edge popping. The “off” comparison measures the whole feature's cost; it does not establish the benefit of a proposed traversal.

### 2. Retro filtering: avoid samples whose blend weight is zero

Sources: [RetroAux](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_retro.h:171), [RetroBlocks/RetroSample](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_retro.h:132), [DetailFactor call](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:1152).

`RetroAux` always samples the smooth texture before computing its blend weight. With full block contribution (`k == 1`), the result is entirely `RetroBlocks`, so that smooth lookup is redundant. Compute `k` first: use smooth only at zero, blocks only at one, and both inside the blend interval. Its lookups use explicit gradients/LOD, making this easier to restructure safely than implicit-gradient sampling.

The world detail path similarly computes one or two detail samples, then mixes the factor back to 1 when `RetroP2.z == 0`. The saved category settings select exactly this case. Gate the call on effective detail contribution before entering `DetailFactor`. A solid texel with `result.a == 0` also receives no detail modulation and can skip that work. Preserve the distinction between solid alpha-as-fullbright and alpha-tested coverage.

`RetroBlocks` uses one, two, or four base-texture reads at block interiors/edges. Palette quantization adds a LUT lookup for each quantized tap; transition to smooth sampling can add another texture and LUT lookup. Reuse block coordinates, edge weights and LOD calculations across color/fullbright/normal/specular textures only where their UVs and dimensions match. Do not assume differently sized auxiliary textures share the same LOD.

Changing to one palette lookup after block interpolation would save work but changes the result: quantization and interpolation do not commute. A baked palette/block texture cache is a larger alternative, invalidated by set, palette, block size, mip and dither changes.

**Benchmark:** current retro settings; then bump blending 0, 0.5 and 1, detail contribution 0 and 1, near block edges and minified textures. Confirm the exact endpoint paths reduce instructions and fetches without changing the endpoint images. This is one of the best initial implementation experiments.

### 3. Replace the guessed lightmap direction's gather workload

Sources: [LightmapGreen / LightmapLumDerivs](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:1089), [baked bump fallback](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:1178).

When a valid deluxemap direction is unavailable, baked normal lighting reconstructs a smooth lightmap slope from a 4×4 luxel neighborhood. Four calls to `LightmapGreen` issue 4 gathers for one style, 8 for two styles, or 16 for the three/four-style packed path. This is additional to the ordinary lightmap reads. Enabling deluxemaps does not remove the fallback on faces with no valid `.lux` data. Alpha-tested faces already avoid this derivative reconstruction.

Prefer valid authored deluxemaps where available. Otherwise test cached slope/direction data. An exact option for static style values is a prepass generating matching per-cell slope coefficients after style blending; update only affected atlas regions when styles change. Preserve the cell's corner coefficients and seam choices: simply filtering one gradient per luxel is not necessarily equivalent at a rejected seam. Prebaking per-style gradients and blending them is cheaper to maintain, but the current `LuxelSlope` seam rejection is nonlinear in blended values. It will not reproduce all animated-style and atlas-border cases exactly.

The contrast/luminance scaling at the end of `LightmapLumDerivs` also needs preserving. Padding face regions is preferable to allowing a cached derivative to mix neighboring atlas faces.

**Benchmark:** no `.lux`, valid `.lux`, animated two-style faces, three/four-style faces, close atlas seams. Compare auxiliary texture bandwidth and any slope-update pass against the removed gathers. Count how much visible screen area actually takes the fallback.

### 4. Dynamic AO: reduce candidates and exploit the existing floor

Source: [DynamicAO / AOBox / AOEllipsoid](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:400).

The shader intersects a screen-tile mask with a global depth-slice mask and visits its set bits. This is already much better than scanning all 64 occluders. However, independent tile/slice masks form a conservative cross-product and can leave many irrelevant candidates. A true per-cluster mask can remove candidates before loading/transformation and form-factor work. Compare its larger storage and binning cost with fragment savings.

Boxes are expensive: up to three facing polygons, each with four direction normalizations and four fitted edge terms. Test ellipsoid approximations for small/distant boxes or an AO quality tier. A lower-resolution AO pass is a larger alternative, but must preserve how AO modifies baked/model-own lighting rather than indiscriminately darkening specular, emission and dynamic light.

A simpler exact-intent opportunity is terminating when `vis <= 0.1`. The final output is already `max(vis, 0.1)`, and valid nonnegative occlusion strengths only reduce visibility. Exit the entire function, not only one inner mask loop. This does not help bright pixels and may increase divergence; measure dense occlusion separately.

**Benchmark:** hands over body, ragdoll pile, moving brush props, mostly empty view. Record candidates after mask intersection, candidates passing reach/self tests, box-face evaluations and pixels reaching the floor. Validate self-group exclusion and moving-object contact.

### 5. Bloom: prioritize full-resolution taps

Sources: [postprocess bloom](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:66), [upsample shader](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_bloom.cpp:109), [mirror bloom](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_stereo.cpp:269).

The final eye postprocess samples the quarter-size bloom texture four times for every affected full-resolution pixel. Test one bilinear sample, or a two-sample compromise, with the blur adjusted upstream. This is a quality experiment: one sample does not reproduce the current extra smoothing. Its broad coverage can make it more valuable than a larger instruction saving in a tiny effect.

Each pyramid upsample uses nine samples of the smaller image plus an own-level fetch, with a mean fetch on the final level. Test a four-tap upsample kernel or a separable alternative. A separable pass adds a target write/read and another draw, so fewer texture instructions alone do not guarantee improvement. Weighted bilinear tap pairing is only exact when the desired discrete kernel and sample alignment permit it; it is not automatically exact for arbitrary `Params.x` spread and subtexel coordinates.

Bright extraction already uses four bilinear taps and starts at quarter resolution. Mean brightness's 64 reads run for a single output fragment per eye; it is much lower priority than four reads across millions of final pixels.

**Benchmark:** glowing HUD text, lamps against dark walls, bright outdoor maps, moving head. Time final postprocess and pyramid separately; check small bright specks, halo shape and adaptation stability.

### 6. Share the world's per-light geometry calculations

Sources: [world dynamic-light loop](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.h:779), [world DarkPlaces path](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:1230), [light helpers](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:237).

The world shader computes distance for attenuation and calls helpers that normalize light direction again for cone, diffuse and specular terms. Compute squared distance, reciprocal length, length, normalized direction and `N·L` once where they refer to the same receiver position. Compute the eye direction once per fragment. Reject `distance² >= radius²` before a square root in the DarkPlaces path.

The [model dynamic-light shader](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:1634) already implements this approach, including a squared radius reject and one eye vector for all lights. It is a useful reference, not an additional optimization target.

Preserve world Quake falloff's plane/tangent-distance formula; replacing it with the model's radial attenuation changes lighting. Retro lighting also deliberately uses different `rl_pos`, `rl_spos` and `rl_mpos` positions for light evaluation and shadow lookup. Share vectors only when those positions agree. Keep map-light shadow iteration order: each update depends on the current baked light.

Point lights have a zero spot vector, yet `SpotCone` still contains reciprocal-length arithmetic. A coherent point-light specialization or explicit type branch may avoid it. Whether the compiler already shares expressions or hoists the eye normalization must be checked in generated code.

**Benchmark:** 1/8/32/64 unshadowed and shadowed lights, both falloff modes, retro grids on/off; compare instruction count, register count and total light-loop time.

### 7. Precompute spotlight shadow bases and constants

Source: [SpotShadow](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:245).

Every spotlight receiver currently normalizes the spot direction, selects an auxiliary axis, normalizes a cross product for `right`, and computes `up`. These values are invariant for that light. Store the frame or a compact projection representation with the light, calculated alongside the CPU shadow view. Also consider storing reciprocal radius, reciprocal usable tile size, border scale and atlas inverse dimensions.

This saves work in both world and model fragments. Extra light-buffer bytes can increase bandwidth and register pressure. Compare a separate spotlight/shadow-data buffer against expanding every point light's record, and preserve the exact frame convention, border, reversed depth and bias behavior. Merely computing the frame once per fragment outside a helper still leaves it repeated over all fragments.

**Benchmark:** flashlight held close to detailed walls, multiple spotlights, mixed point/spot lights. Inspect buffer traffic and shader occupancy, not just ALU instruction count.

### 8. Tighter lighting clusters can remove expensive fragment iterations

Source: [LightTouchesCluster](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.h:1965).

Current membership tests use a sphere against the cluster's enclosing AABB; the six cluster-plane tests are compiled out. AABB corners admit false positives that downstream fragments reject or shade. Add conservative frustum-plane tests after the cheap AABB reject. For spotlights, test the cone against the cluster or use a tighter conservative bound after the already-present cone bounding sphere.

The benefit is not only faster clustering: it is fewer repeated shadow, cone, diffuse and specular computations over receiver pixels. Separate map-light and dynamic-light masks can avoid per-iteration type branches while preserving map-light order. All-empty masks already skip their fragment loops.

**Benchmark:** broad overlapping lights and spotlights crossing cluster corners. Compare cluster-pass overhead, light-mask popcount weighted by visible pixels, and world/model pass savings. Verify asymmetric eye projections, near/far slices and cone edges have no missing light.

### 9. Fine wounds: cache relief and evaluate only needed noise channels

Sources: [WoundHeight / WoundsAt](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:1829), [WoundSheen](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:1913).

With fine masks and relief enabled, a visible nonempty wound texel samples its own mask and other-blood mask, then evaluates four neighboring `WoundHeight` calls. Each neighbor samples the own mask and evaluates two four-corner sine-hash noises. The main fine path also evaluates three four-corner noises. This can reach six mask lookup instructions and 44 source-level sine-hash evaluations, before normal reconstruction and sheen. These are source counts, not guaranteed executed hardware instruction counts.

Generate a cached height/slope field when wounds change rather than regenerating it per eye fragment. Separate burned and blood contributions if their strengths remain adjustable without regeneration. Preserve the deliberate exclusion of other-blood spatter from relief. Cached bilinear slopes approximate the current nonlinear ramps applied to bilinearly sampled masks, so compare edges carefully and include update-pass cost.

More surgical: avoid crust noise where there is no burn contribution, and avoid `spot`/`flick` ember noise when heat/burn cannot produce emission. Wet-only texels need neither char texture variation nor blood relief. Endpoint branches may diverge; mask-wide feature flags or variants can help.

The saved configuration has fine own masks disabled, so the expensive four-neighbor path is inactive there. Chunky wounds already have a zero-mask early return and one own-mask fetch, optionally another other-blood fetch.

**Benchmark:** fine resolution 512/1024/2048, burning and bleeding hands close to the eye, wet-only hands, healing/washing/painting. Include dirty-region updates and both eyes. Compare steady cost and spike cost.

### 10. Liquids: avoid zero-contribution waves/glints and irrelevant ripples

Sources: [LiquidDisplace / LiquidRipples](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:685), [LiquidWaves](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:760), [LiquidShade](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:803).

`LiquidWaves` evaluates its five sine/cosine wave terms before multiplying by `Water.x`. Gate only that block when its strength is zero; long swells and splash ripples have independent controls and must still contribute.

`LiquidDisplace` computes swells/ripples even when its distance fade or pin is zero. Return the original position once either is zero, before calling them. When only height is required, specialized swell/ripple height functions can omit slope accumulation; the compiler may already remove dead vector components, so inspect first.

`LiquidShade` computes half vectors and two variable-exponent glints even if `Water.w == 0`, or the underwater multiplier makes glints vanish. Skip glint arithmetic in those cases while preserving fresnel/opacity behavior. Keep required derivatives outside divergent branches, or use coherent per-kind variants.

`LiquidRipples` computes `length(d)` before rejecting ripples on another liquid height. Move the height test earlier. Reject rings with squared inner/outer radial bounds before square root/trigonometry, and consider per-surface/tile event lists. Up to 32 globally active ripple events otherwise visit unrelated water pixels and geometric-wave vertices. Per-event front/phase constants are also candidates for CPU precomputation.

**Benchmark:** waves off with swells/ripples on; distant and rim-pinned meshes; above/below water; 0/8/32 ripples on the same and different heights. Do not remove the base ripple merely because its fine harmonic fades out.

### 11. Reduce feature-induced register pressure with targeted variants

Sources: [program creation](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.c:344), [world shader](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.h:607), [alias shader](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.h:1335).

Existing variants cover OIT, dither/mode, alpha test and pose format, but most VR material features remain runtime branches. A general shader can retain a larger register footprint even when a uniform disables work. Test a small number of common combinations: basic lit, normal/parallax lit, and full feature set, with optional dedicated retro or wound-heavy paths only if worthwhile.

Exact guards can also skip external spec-map sampling/specular AA when all specular contributions are disabled, and world normal-map sampling when neither baked nor dynamic lighting consumes the bumped normal. Do not apply the same guards to models without accounting for ambient, wounds, reflections and rim effects.

The generic VR effect shader already uses compile-time `MODE`, `BLENDED` and `RETRO` variants. Do not reintroduce a runtime mode switch there. Link-time removal of unused vertex outputs should also be inspected before manually splitting every depth or paint vertex shader.

**Benchmark:** generated register count, spills, occupancy and GPU time for common material batches. Warm programs before timing. Count extra draw/batch splits and compilation cost; avoid combinatorial variant growth without evidence.

### 12. Evaluate a pose once when interpolation is unnecessary

Sources: [GetPoseVertex/main](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.h:1227), [CPU pose equality](C:/OHWorkspace/quakevr-iw/Quake/r_alias.c:650).

Alias vertices fetch/evaluate both poses unconditionally. If pose indices match or blend is zero, reuse the first pose. This can remove a second skeletal matrix blend, or the second MD3 spherical-normal decode (four trigonometric calls), as well as duplicate packed-position reads. Frozen/static models and externally posed skeletons are useful cases to count. CPU equality currently sets blend to zero but does not by itself make runtime buffer accesses disappear from shader source.

Quake pose AO reads the same packed pose record used by `GetPoseVertex`; carry its AO value with the decoded pose to enable explicit reuse where needed. Keep muzzle-flash/recoil `ZeroBlend` behavior intact.

For high-vertex-count skeletons rendered in both eyes and multiple shadow faces, compare a once-per-frame compute skinning cache against repeated vertex-stage skinning. Cache object-space positions/normals and retain per-view projection. Extra writes, reads, synchronization and invalidation can outweigh savings for small Quake meshes.

**Benchmark:** stationary monsters, interpolated animation, hands/body, ragdolls, 0/1/many shadowed lights; measure vertex invocations and bone-buffer reads. Depth variants already allow the linker to remove unused normal/shading outputs.

### 13. Specialize the holey-model shadow fragment shader

Sources: [shadow program selection](C:/OHWorkspace/quakevr-iw/Quake/r_alias.c:362), [shadow metadata zeroing](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_render.cpp:239), [alias fragment main](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.h:1356).

Opaque alias casters already use a vertex-only depth program. Holey casters retain the general alias fragment shader. CPU shadow setup zeros their VR instance metadata, so parallax, wounds, morphs and reflections are already largely gated off; nevertheless the linked shader still contains general lighting/AO/fog/fullbright paths and alpha handling.

A dedicated depth fragment variant needs only the skin alpha lookup and the current cutoff/coverage behavior required for that pass. Preserve affine/noperspective UV modes where applicable. Driver specialization for disabled color outputs may already eliminate color-only work, so capture the active shadow shader before assuming the entire general shader executes.

Avoid enabling forced early fragment tests on a depth-writing alpha-test shader: fragments rejected by `discard` must not leave opaque depth holes filled in. The existing vertex-only opaque path needs no additional fragment shader.

**Benchmark:** holey MDL/MD3/IQM casters under shadowed point lights, correct holes at multiple mips, actual fragment invocations and shader instructions.

### 14. Procedural effects: exchange ALU/SFU work for filtered data selectively

Sources: [LiquidNoise/foam/caustics](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:884), [Shimmer](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_haze.cpp:76), [MorphNoise](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:1762).

Foam evaluates three value-noise samples after useful shoreline/distance rejects. Caustics evaluate two warped sine lattices after a 3D wet-volume fetch and zero-wet reject. Haze evaluates a vector warp plus six sine terms. Morphing evaluates two eight-corner 3D noises. Large affected surfaces can make special-function or integer arithmetic throughput significant.

Test filtered tiled noise/gradient textures, fewer octaves at projected-small scales, and cached world-space fields. These are visual changes: maintain world-space anchoring and common time in both eyes. Do not replace them with unrelated screen noise, which can swim during head motion.

For caustics, a conservative per-draw/face “potentially wet” flag can avoid the 3D lookup across definitely dry geometry. Include the volume's expanded border and normal offset; objects crossing water need dynamic bounds. There is already a return when caustics are disabled and another when the sampled volume is dry.

For haze, test per-mode variants and reuse the projected displacement calculation (`c1 = c0 + ViewProj * vec4(w, 0)`) rather than a second position projection. The compiler may do this algebra already. Its contribution and chord rejects are already before shimmer and final scene fetch.

**Benchmark:** shore closeups, broad wet walls, lava heat viewed across a room, explosions near the eye, morph transitions. Check SFU/ALU stalls versus the extra texture traffic and aliasing.

### 15. Reuse a surface frame across parallax, normals, lux and retro setup

Sources: [BumpedNormalFrame](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:321), [ParallaxUV](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:531), [LuxDirection](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:1108), [RetroBegin](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_retro.h:83).

These functions reconstruct related tangent/gradient frames from `dpdx`, `dpdy`, UV derivatives and a normal. Explicitly reuse cross products, determinant and raw tangent vectors. Verify compiler common-subexpression elimination first and avoid extending live ranges so much that occupancy drops.

Flat BSP faces can carry a constant face normal and texture gradients computed during loading, transformed per brush instance. This trades derivative arithmetic for attributes/varyings or buffer loads. Models require their distinct axis-normalized tangent convention and changing geometry, so a brush optimization cannot simply be copied to them. Wound relief and retro lighting grids have related but different unit/scaling requirements.

**Benchmark:** normal-only versus parallax+lux+retro materials, varying bandwidth and registers, rotated/scaled/mirrored brush entities, UV seams and skinny triangles.

### 16. Avoid reconstructing invariant cluster geometry and light bounds

Sources: [cluster planes/extents/main](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.h:1924), [spot bounding sphere](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:2041).

The 32×16×32 grid rebuilds tile planes, logarithmic depth boundaries and AABB extents for each invocation. X/Y geometry repeats across depth slices; depth geometry repeats across tiles. Precompute tile rays/planes and slice depths per eye projection, or cache full cluster AABBs until that projection changes. Compare buffer reads with removed normalization/cross products/exponentials.

Each 8×8 workgroup independently transforms all active light bounding spheres into view space, including spotlight sphere construction. Compute those once per eye in CPU preparation or a small preceding compute dispatch. Shared staging within a group is already present; precomputation targets duplication between groups.

**Benchmark:** clustering separately at 0/8/64 lights, including precompute/dispatch costs. Current X/Y dimensions are multiples of 8; if changed later, do not let out-of-range invocations return before a workgroup barrier while their peers reach it.

### 17. Upscaling: use supported vendor options before rewriting the kernel

Sources: [integration](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_upscale.cpp:54), [NIS gather switch](C:/OHWorkspace/quakevr-iw/Quake/vr/external/nis/NIS_Scaler.h:235), [NIS architecture choices](C:/OHWorkspace/quakevr-iw/Quake/vr/external/nis/NIS_Config.h:99), [FSR half path](C:/OHWorkspace/quakevr-iw/Quake/vr/external/fsr1/ffx_fsr1.h:445).

FSR EASU/RCAS use FP32 here. EASU already uses the vendor's texture-gather path (12 RGB-component gather calls for its 12-tap kernel), and RCAS uses five pixel loads. Test the vendor's packed FP16 versions only with the required OpenGL shader extensions/capability checks. `mediump` or `packHalf2x16` alone does not establish native FP16 arithmetic.

NIS defaults `NIS_TEXTURE_GATHER` to 0; the wrapper does not override it. Test its supplied gather path: three component gathers replace four RGBA samples while filling a 2×2 luma tile. Hardware/cache behavior determines whether that is faster. `NIS_USE_HALF_PRECISION` is also off; enabling it reduces precision of designated `NVH` data, including shared data, rather than making every `NVF` computation half precision.

The current 32×24/128-thread NIS layout matches the vendored optimizer's generic NVIDIA choices. AMD/Intel choices use 256 threads; the NVIDIA FP16 choice uses 32×32/128. Update CPU dispatch block dimensions alongside shader defines when experimenting. Do not label the current layout universally wrong.

EASU already skips outside its lens circle and is scissored to a box. NIS only dispatches the enclosing block rectangle; whole workgroups proven outside the required circle could uniformly return before entering the vendor kernel. Include filter/RCAS support margin and never return only some lanes before shared barriers.

**Benchmark:** native scale, representative reduced scales, FSR/NIS, gather on/off, supported FP16 on/off, 128/256 threads. Validate lens seams, text, dark gradients and sharpness. Measure upscale+composition as a unit.

### 18. Move morph rejection earlier only after resolving derivative dependencies

Sources: [Morph](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:1936), [alias main](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.h:1367), [late discard](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:2021).

The shader knows whether a morph pixel is shown near the start, but discards near the end after texture, wound, normal, AO, lighting and reflection work. On overlapping incoming/outgoing weapon models, much of that work can be wasted.

Early rejection must retain derivative-valid execution. Normal/specular AA and retro lighting's `fwidth` of the computed light are later dependencies. Computing only position/UV derivatives first is insufficient. A staged shader can finish needed derivative inputs, then discard before independent expensive work; variants with retro lighting AA disabled permit more movement. Alternatively retain derivative-producing work and move rejection before the remaining fullbright/reflection/output work.

**Benchmark:** repeated morphs with normal/specular AA and retro lighting on/off. Check seam stability, specular crawling and mip selection during head motion. Avoid a blanket “move discard to the top” edit.

### 19. Screen/hologram glow: cache repeated text filtering

Source: [phosphor/glow/screen/hologram](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_gfx_gl.cpp:90).

A screen samples phosphor brightness three times for color splitting, then potentially samples 12 taps for its two glow rings. Hologram text also adds a haze sample. These kernels repeat in both eyes despite sharing the display texture.

Precompute a glow texture when display content changes, then sample it with the same torn/glitched coordinates. Preserve the brightness function: `max` plus luminance before averaging is not generally interchangeable with averaging RGB first. For displays updating every frame, compare the update pass against saved eye shading; small display coverage can make caching lose.

When split is exactly zero, one phosphor sample suffices for all three channels. The compiler may CSE this only if it knows the uniform-derived split is zero; an explicit coherent endpoint branch makes that condition visible. Precompute time-only flicker/seed constants and fixed ring offsets if generated code retains repeated calculations. Constant-loop sine/cosine calls are likely folded already.

**Benchmark:** wrist close to eye, large map board, hologram text and beam, glitch off/on, display updates versus static content. The generic shader already specializes screen and hologram by mode.

### 20. Visibility compute: aggregate append work if atomics dominate

Source: [cull_mark_compute_shader](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.h:1832).

Visible surfaces perform a deduplication `atomicExchange`, a per-texture `atomicAdd`, then serial triangle-fan index writes in one invocation. Repeated leaf references can contend on the same surface; many faces of one texture can contend on its command count.

Test marking unique visible surfaces followed by compact/append work, or workgroup/subgroup aggregation for destinations sharing a texture. Prebuilt surface index ranges with compacted draw metadata are a larger alternative to regenerating indices. Do not remove the current deduplication: BSP surfaces can occur in multiple leaves. Extra dispatches, scans and buffers may lose on original Quake maps.

The indirect clear and gather shaders are already simple 64-thread bounded writes/copies; changing their arithmetic is unlikely to matter. Draw-remap division/modulus use a compile-time constant and should be checked in generated code before hand optimization.

**Benchmark:** large BSPs with repeated marksurfaces and large polygon faces, clustering of textures, atomic contention, index bytes emitted, total compute+draw cost.

### 21. Full-screen arithmetic: inactive paths, curve constants and precision

Sources: [postprocess](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.h:242), [underwater/grade/dither](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_glsl.h:50), [tone function](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_tonemap.h:39), [mirror](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_stereo.cpp:231).

The underwater branch replaces the initial `GammaTexture` fetch with four `WaterScene` samples. Select the base scene source first so the original fetch is explicitly absent underwater. The compiler may already sink/eliminate it; verify the active program. A small dry/underwater postprocess variant split can also reduce register pressure.

Precompute tone-curve constants involving knee/white point and common inverse image/projection sizes. Test a gamma==1 path rather than always `pow(rgb, gamma)`. Retro light spacing defaults to 1, yet quantization contains `pow(m,g)` and its inverse; specialized spacing 1 and 0.5 paths can use identity and square-root/square, preserving antialiasing and level logic. Uniform branches or variants are preferable to changing arbitrary exponents.

A combined tone/gamma/grade LUT trades arithmetic for bandwidth and approximation; its HDR input domain, hue-preserving max-channel shoulder, bullet-time order, and final dither require careful handling. Existing gamma/tone/grade operations are already in a common final pass, so “fuse all postprocess shaders” overstates the remaining opportunity.

Mirror Catmull-Rom already combines the middle coefficients into nine bilinear reads instead of 16 texel reads. Bilinear mirror sampling is a quality tradeoff affecting the window, not eye image quality. Its cost can still delay the GPU frame if ordered on the critical path.

**Benchmark:** dry/underwater, tone/grade/gamma endpoints, retro spacing variants, window resolutions. Keep deterministic dither fixed when doing image differences.

### 22. Auxiliary sampling: gather depth and reject invisible contributions carefully

Sources: [scene distance shader](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_water.cpp:246), [world alpha test](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.h:693), [particle fragment](C:/OHWorkspace/quakevr-iw/Quake/gl_shaders.h:1551), [particle states](C:/OHWorkspace/quakevr-iw/Quake/r_part.c:734).

The non-MSAA distance shader fetches four neighboring depth texels and reduces min/max. One precisely aligned `textureGather` can retrieve the four depths. Preserve clamping for odd image dimensions and reversed-Z conversion. The MSAA shader fetches sample 0 from each pixel; a gather is not a drop-in replacement for `sampler2DMS`, and changing to all samples changes both work and depth semantics.

The world alpha-test path samples fullbright before rejecting albedo alpha. Move the fullbright read after discard, preserving explicit precomputed gradients/mip bias rather than introducing implicit derivatives in divergent flow. This saves only pixels on fullbright-enabled holey materials that are rejected; it is not a major opaque-world optimization.

Classic particle shading can compute coverage before fog/palette/dither and discard zero-contribution pixels in a no-depth-write blended/OIT variant. It also has an opaque, depth-writing mode, where a zero-alpha discard changes behavior. Do not apply the same rejection indiscriminately. The generic VR blended shader already discards all-zero results before reading soft depth.

**Benchmark:** soft-particle stress, OIT and opaque particle modes, fence foliage/fullbright skins, depth edges and odd render dimensions. OIT resolve already uses early stencil tests to avoid empty pixels; adding a second revealage discard is not the first optimization.

### 23. Particle, tube and bent-mesh vertex work

Sources: [particle VS](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_gfx_gl.cpp:285), [tube VS](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_gfx_gl.cpp:378), [bent VS](C:/OHWorkspace/quakevr-iw/Quake/vr/vr_gfx_gl.cpp:443).

VR particles expand six nonindexed vertices per quad. Two corners are duplicated, and per-particle streak/pull frame calculations repeat. Test an indexed four-corner expansion or a per-eye compute-generated frame cache only at high particle counts; the current GPU expansion already avoids CPU quad construction. Eye-dependent streak/pull geometry must remain per eye.

Tubes also emit six vertices per quad, recompute ring frames and side sine/cosine values, and repeat positions along neighboring quads. Indexed geometry or a cached side-angle table can remove duplicates. Flat shading needs separate face-normal handling, so indexing cannot blindly merge all side vertices.

Bent meshes perform a binary search over curve samples for every vertex. A lookup table mapping arc length to segment, or uniformly spaced samples allowing direct indexing, can remove searches. Preserve extrapolation beyond curve ends and interpolation of local frames. This is usually a small chain/rope/display workload, not the first whole-frame target.

**Benchmark:** particle storms and many long chains/ropes, record counts and vertex invocations, then compare added cache/dispatch traffic. Ordinary GUI/debug/simple-depth shaders have too little arithmetic to justify similar machinery.

## Already optimized or low-priority areas

- Opaque world/brush rendering already has a depth prepass using invariant vertex positions. Do not propose another general opaque-world prepass without identifying coverage the existing one misses.
- Opaque alias shadows and simple brush shadows already use vertex-only depth programs. Link-time dead-code removal can eliminate unused general vertex outputs in those programs.
- Shadow PCF modes already combine separable tap weights: nominal 3×3 uses four bilinear comparison reads and nominal 4×4 uses nine. The available filter modes therefore use 1/4/4/9 actual shader lookup calls; active retro filtering can override them to one. Further reductions change filter quality unless a new equivalent kernel is derived.
- Map-light shadows skip faces with no moving casters and skip the static shadow lookup if the moving lookup is fully lit. Filtering both maps unconditionally would increase cost.
- Model dynamic lighting already shares distance/direction and view-vector calculations, and radius rejection uses squared distance.
- Light loops and AO loops already enumerate mask set bits. Empty masks skip work; AO already rejects reach, self and back-facing boxes before expensive face evaluation.
- Detail already has distance and fine-octave exits. Foam already rejects distant and irrelevant shoreline pixels before noise. Haze rejects absent chord/contribution before shimmer.
- Liquid foam/refraction share the center scene-distance fetch, and foam can reuse the already computed swell height. These caches are already present.
- Environment-map rendering already uses simplified average face albedo/emission and lightmap shading. Adding the full world material shader to that pass would be a regression. Its remaining style reads/contrast pow are conditional and relatively small.
- GUI is one texture sample/multiply; view blend and debug fragments output a supplied color; the simple depth vertex transforms position. These are primarily coverage/geometry/state costs, not meaningful arithmetic targets.
- Palette initialization does a costly 256-color nearest search per LUT voxel, but [GLPalette_UpdateLookupTable](C:/OHWorkspace/quakevr-iw/Quake/gl_texmgr.c:2426) caches palette/metric state. It is not steady per-frame gameplay shading. Cached generated LUTs may reduce startup/settings-change hitches. Palette postprocess operates on only 256 colors and already moves palettized gamma/contrast out of full-resolution shading.
- The foveated debug overlay is diagnostic. Keep it disabled in performance captures; optimizing its ring distance/color branches does not improve ordinary gameplay.
- OIT resolve already uses early fragment tests and stencil coverage, and MSAA uses `gl_SampleID`. Collapsing per-sample resolve to one pixel invocation is a quality/coverage change, not an equivalent shader rewrite.

## Implementation rules that matter for this code

1. Preserve derivatives. `dFdx/dFdy/fwidth`, implicit `texture` LOD and `textureQueryLod` need appropriate quad execution. Several shaders deliberately compute derivatives before discard or disable derivative-based AA in alpha-tested paths. Explicit `textureGrad` permits some safe movement, but does not fix a later derivative of newly computed lighting or normals.
2. Do not enable `layout(early_fragment_tests)` globally. Discarding alpha/morph fragments can then leave depth/stencil effects different from current behavior. It is already used where appropriate in OIT resolve.
3. Preserve world/model position conventions. World `in_pos` is world space; alias `in_pos` is relative to the eye. Liquid shading deliberately retains the flat surface position while geometric waves displace rasterized vertices. Replacing refraction's projected flat position with `gl_FragCoord` depth/coordinates is only valid when those projections agree; displaced liquids require separate data or the existing projection.
4. Preserve stereo coherence. Surface grids, wounds, water and haze are anchored to the world/skin. Precomputations can share view-independent data between eyes, while projection, parallax, shading derivatives and some billboards remain view-dependent.
5. Compare executable shader code. Source-level multiplication chains, cached repeated normalizations, constant-loop trigonometry and packed half values may already optimize away. Conversely, a uniform zero does not guarantee removal of all its upstream work. Record actual register allocation, spills and texture/SFU instructions where available.
6. Keep FP32 for world positions, projection, distance/bias and derivative determinants unless targeted precision tests prove otherwise. Restrict initial FP16 experiments to vendor-supported upscaling and bounded color/weight data. Ordinary desktop GLSL precision qualifiers are not proof of half arithmetic.
7. Do not reorder baked map-light shadow multiplication or change Quake attenuation while claiming an equivalent speedup. Preserve atlas borders, reversed depth, normal bias, normal-map handedness, model axis normalization and self-AO groups.

## Benchmarking handoff

Capture the exact commit, generated shader variant/defines, driver/GPU, headset resolution and refresh, render scale, MSAA, shading rates, shader cvars and map/texture assets. Log live settings; the saved configuration is only a starting clue. Use repeatable head/controller paths because parallax, retro edges, AO and periphery shading rates all depend on view position and direction.

Start with these implementation experiments, one at a time:

1. `RetroAux` endpoint paths plus zero-retro-detail guard. This is small, directly supported by the saved settings, and easy to image-compare.
2. World-light shared vectors plus squared radius rejection. Use the existing model loop as the reference without copying its different attenuation formula.
3. Spotlight basis/constants precomputation. Test flashlight scenes and mixed light types.
4. AO floor exit; then evaluate true cluster masks independently.
5. Liquid zero/fade gates and ripple height-first rejection.
6. One/two/four-tap final bloom A/B, explicitly treating it as a filter-quality experiment.

Then investigate the larger POM, lightmap-slope, fine-wound and compute-skinning changes only where captures show they dominate. Disabling those features provides upper bounds on their current cost, not promised savings from a replacement algorithm.

| Scenario | Features it isolates | Evidence to collect |
| --- | --- | --- |
| Close grazing world surface | POM, retro filtering, detail, guessed/lux baked bumps | Height iterations, gathers, texture stalls, image stability |
| Flashlight plus many local lights | World/model loops, spot frames, PCF, cluster precision | Light-mask popcount, shadow fetches, registers, cluster+receiver time |
| Hands over body and ragdoll pile | AO, skeletal vertices, fine/chunky wounds | AO candidates/faces, visibility floor frequency, skinning reads |
| Large pool with many splashes | Waves, ripple scans, foam/refraction, depth downsample | Active ring hits versus scanned rings, SFU/texture time |
| HUD/glowing displays against dark wall | Bloom, screen/hologram glow, postprocess | Full-resolution fetches, halo/text quality, display update cost |
| Reduced eye scale, window on/off | FSR/NIS, lens composition, mirror | Upscale+compose time, workgroup occupancy, window contribution |
| Large BSP with many visible faces | Cull/mark, index generation | Atomic contention, unique faces, generated index bytes |

Use warmed programs and assets for steady-state comparisons; separately record first-use shader compilation and effect-update spikes. Pair scene/pass timestamps with whole-frame GPU and CPU timelines. Report median, p95/p99 and worst recurrent frame intervals rather than FPS alone. Avoid timestamp scopes that include CPU-induced GPU idle being misinterpreted as shader work.

For shader counters, keep intrusive instrumentation out of the final timings: extra atomics or output writes can change occupancy and bottlenecks. Use a separate diagnostic capture or sampling. Make moving-head comparisons with both eyes and both original and changed features together; preserve deterministic seeds/time when comparing outputs. Repeat under current foveation and an unfoveated diagnostic run so changes in fragment invocation density do not hide the actual shader cost.

For every candidate, the useful result is: affected runtime variant and screen coverage; before/after GPU time for the pass and full frame; register/spill/instruction or fetch changes; added precompute/update cost; and any visual difference. Reject changes that merely move cost to another pass or create worse recurrent frame spikes.
