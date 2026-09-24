# Rendering — Quake VR functional changes

## Summary
VR needs from the renderer: (1) one scene render per eye into its own FBO, using the HMD's projection matrix and a view
origin/orientation that includes the per-eye offset, then submit to the OpenVR compositor and mirror to the window;
(2) about 30 extra client-side "view" alias entities (both weapons, hands with separate finger entities, "ghost"
hands, holsters, holster slots, weapon buttons, torso) drawn in world space with no depth-range hack; (3) alias-model
extensions: per-entity non-uniform scale, offset, horizontal mirroring, light override/multiply, and a "zeroBlend" lerp
toward frame 0; weapon models are rescaled by mutating `aliashdr_t::scale` at runtime; (4) 3D text (QC-driven
world text plus floating ammo counters); (5) the HUD, menus and console drawn as world-space quads; (6) a laser/dot
crosshair and many debug helpers drawn as immediate-mode lines and points.
Size: gl_rmain about +600 lines, r_alias (rewrite), gl_screen/gl_draw (medium), r_part (heavy rewrite; see its section),
plus the render parts of vr.cpp and vr_showfn.cpp. The rest of the diff is C++ churn and switching to GLEW.
Biggest risks for Ironwail (IW): QVR relies on a **compatibility-profile GL 1.x/2.x pipeline** (glBegin/glEnd,
matrix stack, `gl_ModelViewProjectionMatrix`, `gl_Fog`, alpha test). IW is **core-profile 4.3** with shaders only,
per-frame UBOs, reversed-Z clip control and framebuffers sized to the window. Every VR draw path has to be rewritten
rather than copied. Alias "anchor vertex" cvars index QVR's **tri-strip-reordered** pose vertex array, and IW does not
have that array. QVR also removed QSS's MD3/IQM/multi-surface alias rendering and the FTE scripted particles.

## Changes

### Per-eye stereo rendering loop (FBO per eye, OpenVR submit, mirror)
- **Where (QVR):** vr.cpp `VR_UpdateScreenContent` 3394-3540 (eye loop 3510-3528, mirror blit 3530-3539);
  `RenderScreenForCurrentEye_OVR` 1544-1628; `CreateFBO`/`RecreateTextures`/`CreateMSAA` 295-398; `VR_Enable` 1423-1514
  (eye FBO creation 1474-1490, fov from `GetProjectionRaw`, `SDL_GL_SetSwapInterval(0)`); gl_screen.cpp `SCR_UpdateScreen`
  1428-1502 and new split-out `SCR_UpdateScreenContent` 1348-1418.
- **Upstream (BASE):** `SCR_UpdateScreen` renders once (V_RenderView + 2D) to the back buffer. `r_stereo` does anaglyph
  inside `R_RenderView`.
- **Change:** `SCR_UpdateScreen` calls `VR_UpdateFlick()`. If `vr_enabled && !con_forcedup` it switches to `sv.qcvm`
  (`PR_SwitchQCVM(nullptr)` then `&sv.qcvm`) and calls `VR_UpdateScreenContent()`. Otherwise it sets
  `cl.viewangles = r_refdef.viewangles = r_refdef.aimangles = cl.aimangles` and calls `SCR_UpdateScreenContent()`. If
  `vr_fakevr==1` it makes one more `SCR_UpdateScreenContent()` pass with viewangles = aimangles = `cl.viewangles`, so a
  non-VR fakevr frame renders twice. `VR_UpdateScreenContent` updates devices and aim modes, then for each
  of the two eyes:
  - it sets the global `current_eye`;
  - it computes `vr_viewOffset` from the eye position (OpenVR (-z,-x,y) × `meters_to_units`, rotated by yaw
    delta) and adds `vr_floor_offset`;
  - it forces `glwidth/glheight` to `GetRecommendedRenderTargetSize` and (re)creates the eye FBO (RGBA8 color texture +
    DEPTH_COMPONENT24 texture). An MSAA FBO is created when `vr_msaa` > 0 (default 4, clamped to the max MSAA level);
  - it binds the FBO, sets the viewport and clears color+depth;
  - it calls `srand(cl.time*1000)` so random effects match between eyes;
  - it sets `r_refdef.fov_x/fov_y` from the eye's asymmetric tangents (atan(-L)+atan(R)) and calls
    `SCR_UpdateScreenContent()` (full 3D view + 2D);
  - it resolves MSAA by blit, calls `GLSLGamma_GammaCorrect(eye.index)`, submits `fbo.texture` via
    `VRCompositor()->Submit` (`TextureType_OpenGL`, `ColorSpace_Gamma`), restores glwidth/glheight and binds FB 0.
  After the loop, eye 0's texture is blitted to the back buffer as the mirror, flipped vertically. The source rectangle
  arguments (`0, w_fbo, h_fbo, 0`) look swapped, which is probably a bug. `GL_EndRendering` then swaps.
- **Purpose:** VR stereo.
- **Ironwail:** IW has no stereo. The 3D path is `SCR_UpdateScreen` (gl_screen.c 2115) → `V_RenderView` → `R_RenderView`
  (gl_rmain.c 2037), which calls `R_SetupView` then `R_RenderScene` then `R_WarpScaleView`. The FBO chain is
  `framebufs.scene/composite/oit/resolved_scene` (gl_rmain.c 224-293). They are created at `vid.width×vid.height`
  (`GL_CreateFBOAttachment` 148) and `GL_PostProcess` (331) writes to FB 0. MSAA is already scene-FBO based
  (`vid_fsaa`, resolved in `R_WarpScaleView`). Gamma, contrast and dithering are already done in `GL_PostProcess`.
  `GL_EndRendering` (gl_vidsdl.c 1376) calls PostProcess + `GL_ReleaseFrameResources` + swap.
- **Isolation idea:** new `vr_render.c` with `VR_RenderEyes()`, called from `SCR_UpdateScreen` in place of
  `V_RenderView`+2D when VR is active. For each eye:
  - set a global `r_vr_eye` and eye pose;
  - use a second `framebufs` set sized to the HMD target. Generalise `GL_CreateFrameBuffers(width,height)` or keep
    one set per eye;
  - run `R_SetupView`/`R_RenderScene`/`R_WarpScaleView`, then the 2D-in-3D pass;
  - run `GL_PostProcess` with a destination-FBO parameter (the eye texture) instead of 0;
  - submit to OpenVR.
  Afterwards blit or mirror to FB 0 and swap once. `GL_ReleaseFrameResources` must run once per frame, not per eye.
  Check that IW's per-frame upload ring buffers (`GL_Upload`) are big enough for two scenes. Keep the
  `srand(cl.time*1000)` per eye (IW particles use `rand()`).
- **Tag:** VR-CORE / RENDER

### HMD projection and view matrices
- **Where (QVR):** gl_rmain.cpp `R_SetupGL` 603-653 (`VR_EnabledAndNotFake()` → `VR_SetMatrices()`); vr.cpp
  `VR_SetMatrices` 3542-3551; `VR_AddOrientationToViewAngles` 3572-3578 (used in view.cpp 929); view.cpp 1012-1013
  (`r_refdef.vieworg = ent->origin + vr_viewOffset`).
- **Upstream (BASE):** projection comes from `GL_SetFrustum(r_fovx,r_fovy)` (symmetric, near `NEARCLIP`), and the
  viewport from the refdef vrect with `r_scale`.
- **Change:** in real VR the projection is `TransposeMatrix(ovrHMD->GetProjectionMatrix(eye, 4.f /*near*/,
  gl_farclip.value))`, loaded with `glLoadMatrixf`. The viewport is left as set by the eye loop, so `r_scale` is ignored.
  The modelview is unchanged (Euler angles from `r_refdef.viewangles`, translated by `vieworg`). The per-eye pose is
  folded into `vieworg` (via `vr_viewOffset`) and the angles (via HMD orientation).
- **Purpose:** correct asymmetric per-eye frusta.
- **Ironwail:** gl_rmain.c `R_SetFrustum` 829-875 builds `r_matproj` with `GL_FrustumMatrix` (symmetric, with
  **reversed-Z when `gl_clipcontrol_able`** and the Quake→GL axis swap baked in) and `r_matview` from viewangles/vieworg.
  It extracts the 4 frustum planes from `r_matviewproj`, copies it into `r_framedata.viewproj` and sets log-Z light-cluster
  params from znear/zfar. znear is FOV dependent (0.5–4).
- **Isolation idea:** hook in `R_SetFrustum`. If VR is active, get the four raw tangents (`GetProjectionRaw`) and build an
  off-axis version of `GL_FrustumMatrix` (reversed-Z or standard, same axis-swap layout). Do not use OpenVR's GL matrix
  directly, because it has the wrong depth convention. Use near=4 to match QVR, and still feed zlogscale/bias. Because IW
  derives culling planes from the real matrix, QVR's "fovx += 25" culling hack (below) is not needed. `R_SetupView` (it
  also runs `R_MarkSurfaces` compute culling, sorting, dlights) must run per eye, or once with a merged frustum.
- **Tag:** VR-CORE / RENDER

### Frustum culling widened in VR
- **Where (QVR):** gl_rmain.cpp `R_SetFrustum` 551-580 (`if(vr_enabled.value) fovx += 25;`).
- **Upstream (BASE):** only `r_stereo` adds 10.
- **Change:** CPU frustum planes are built with a horizontal FOV 25° wider whenever `vr_enabled`, to hide culling errors
  caused by the asymmetric projection.
- **Ironwail:** planes come from the view-projection matrix (gl_rmain.c 862-865), so this is not needed if the VR
  projection is used in `R_SetFrustum`.
- **Isolation idea:** drop it.
- **Tag:** VR-CORE

### Viewmodel rendering: many view entities, no depth hack, invisibility becomes translucent
- **Where (QVR):** gl_rmain.cpp `R_DrawViewModel(entity_t*)` 919-963; `R_RenderScene` 1459-1596 (view entity draws
  1497-1586); client.hpp `anyViewmodel`/`forAllViewmodels` 377-427 (list of 37 entities); r_alias.cpp
  `R_SetupAliasLighting` 1086-1101.
- **Upstream (BASE):** `R_DrawViewModel(void)` draws `cl.viewent` with `glDepthRange(0,0.3)`. It is skipped when
  `cl.items & IT_INVISIBILITY`, health ≤ 0, chase_active or r_drawviewmodel 0. The gun minimum light of 72 applies only
  to `cl.viewent`.
- **Change:**
  - `R_DrawViewModel` takes an entity and skips it if `viewent->hidden` (new `entity_t::hidden`).
  - Invisibility no longer hides the model: it sets `viewent->alpha = 128` (255 otherwise) for every view entity.
  - `glDepthRange(0,0.3)` is applied only when `!vr_enabled`.
  - `R_RenderScene` draws view entities in this order, after particles and `Fog_DisableGFog`, and after the VR crosshair:
    - `cl.viewent`, `cl.offhand_viewent`, `cl.mainhand_wpn_button`, `cl.offhand_wpn_button`;
    - ammo text (see world text);
    - if `vr_leg_holster_model_enabled`: `left/right_hip_holster_slot`, `left/right_upper_holster_slot`;
    - `left/right_hip_holster`, `left/right_upper_holster`;
    - for left/right hand and left/right ghost hand: `.base, .f_thumb, .f_index, .f_middle, .f_ring, .f_pinky`;
    - `cl.vrtorso` if `vr_vrtorso_enabled == 1`.
  - After `R_ShowTris` and `R_ShowBoundingBoxes` it calls `quake::vr::showfn::draw_all_show_helpers()` when vr_enabled.
  - The minimum-light-72 rule applies to every entity in `anyViewmodel`.
  - `R_ShowTris` iterates all view entities (`forAllViewmodels`).
  - All these entities are in world coordinates, positioned by view.cpp and vr.cpp. They are culled with
    `R_CullModelForEntity`.
- **Purpose:** VR hands, dual weapons, holsters, body.
- **Ironwail:** gl_rmain.c `R_IsViewModelVisible` 1130 and `R_DrawViewModel` 1154 draw only `cl.viewent`, using
  `GL_DepthRange(ZRANGE_VIEWMODEL)`, after `R_EndTranslucency`. r_alias.c `R_DrawAliasModel_Real` 564 adds
  `cl_gun_x/y/z` offsets and `cl_gun_fovscale` for `cl.viewent` (must be disabled in VR). `R_SetupAliasLighting` 248
  applies min-light to `cl.viewent` only. `R_SetupEntityTransform` 184 skips lerpmove for `cl.viewent` only (QVR is the
  same).
- **Isolation idea:** `VR_GetViewEntities(entity_t **out)` returns the non-hidden, model-bearing list. In
  `R_DrawViewModel`, when VR is active, call `R_DrawAliasModels(list,n)` with `ZRANGE_FULL` and skip the cl_gun
  adjustments. The invisibility alpha of 128 needs a translucent pass: IW handles translucency via OIT between
  `R_BeginTranslucency`/`R_EndTranslucency`, so translucent view entities should go into that pass, or be appended to
  `cl_visedicts` before `R_SortEntities`. Replace the `e == &cl.viewent` checks with `VR_IsViewEntity(e)` for min-light
  and lerpmove.
- **Tag:** VR-CORE / RENDER

### Alias model: per-instance scale/offset, horizontal flip, light modifier
- **Where (QVR):** r_alias.cpp `R_DrawAliasModel` 1169-1522 (transform 1205-1226, flip 1208-1212 + 1518-1521),
  `R_SetupAliasLighting` 1154-1161; entity.hpp new fields `horizFlip`, `msg_scales[2]`, `model_scale`,
  `model_scale_origin`, `model_offset`, `hidden`, `zeroBlend`, `lightmod` (enum `EntityLightModifier
  {None=0,Override=1,Multiply=2}`), `lightmodvalue`. `textentity_t {origin, angles, scale, hidden, horizFlip}`.
- **Upstream (BASE):** transform is `R_RotateForEntity(origin, angles, netstate.scale)` → `scale_origin` → `scale`.
  Lighting is multiplied by `netstate.colormod/32`.
- **Change:** the matrix order after `R_RotateForEntity` is:
  1. if `horizFlip`: `glScalef(1,-1,1)` and `glFrontFace(GL_CCW)` (restored to CW after);
  2. `translate(-model_scale_origin)`, `scale(model_scale + 1)` (note the **+1**: 0 means unscaled),
     `translate(+model_scale_origin)`;
  3. `translate(hdr->scale_origin)`, `scale(hdr->scale)`;
  4. `translate(model_offset)` (in model byte space, after hdr scale).
  Lighting: after the /200 normalisation, `Override` gives `lightcolor = lightmodvalue` and `Multiply` gives
  `lightcolor *= lightmodvalue`. view.cpp uses Multiply {6,6,6} to highlight hovered holsters. The QSS `colormod`
  multiply was **removed**. `GL_DrawAliasShadow` and `R_DrawAliasModel_ShowTris` ignore flip/scale/offset.
  `R_CullModelForEntity` ignores model_scale.
  The fields are filled from the network in cl_parse.cpp 1860-1865 and 1959, and from cl_main.cpp 760 (lerp of
  `msg_scales`), for server entities with `model_scale*` edict fields (sv_main.cpp 3917) — see the protocol inventory.
- **Purpose:** left-hand mirroring of right-hand models; QC-controlled model scaling; highlighting.
- **Ironwail:** r_alias.c `R_DrawAliasModel_Real` 609-611 builds `model_matrix` via `R_EntityMatrix(origin, angles,
  e->scale)` + `ApplyTranslation/ApplyScale`, then packs it into `aliasinstance_t.worldmatrix` (instanced, batched,
  `R_FlushAliasInstances`). Culling uses `R_GetEntityBounds` (gl_rmain.c 391) with `e->scale`. The lighting ends in
  `R_SetupAliasLighting` 225-289 (IW also has no colormod).
- **Isolation idea:** add the fields to IW `entity_t` and a `VR_ApplyAliasEntityTransform(e, model_matrix)` call between
  `R_EntityMatrix` and the scale_origin translation, plus the post-scale `model_offset` translation. A negative
  determinant flips winding. IW sets culling through `GL_SetState` (`GLS_CULL_*`), so break the batch in
  `R_Alias_CanAddToBatch` when the flip changes and use front-face culling for flipped instances, or flip
  `glFrontFace` around the flush. Apply lightmod at the end of `R_SetupAliasLighting`. Expand the bounds in
  `R_GetEntityBounds` by model_scale.
- **Tag:** RENDER / GAMEPLAY

### Alias model: "zeroBlend" (blend current animation toward frame 0)
- **Where (QVR):** r_alias.cpp `GLAliasBlended_CreateShaders` 241-359; `GL_DrawBlendedAliasFrame_GLSL` 452-530;
  `GL_DrawBlendedAliasFrame` (immediate mode) 593-709; `R_SetupAliasFrameZero` 966-985;
  `getDrawAliasFrameData`/`getFinalVertexPos*` 532-590 (exported in render.hpp); `R_DrawAliasModel` 1179-1192, 1352-1363.
  Set in view.cpp 1737/1750 from weapon cvars `WpnCVar::ZeroBlend` and `TwoHZeroBlend`.
- **Upstream (BASE):** none.
- **Change:** the zero pose is `frames[0].firstpose` (+ `(int)(cl.time/0.1) % numposes` for grouped frames), with
  blend 1 and no lerp. The vertex shader does
  `pos = mix(mix(Pose1, Pose2, Blend), ZeroPoseVert, ZeroBlend)`, where ZeroPoseVert is the xyz of the zero pose
  (attribute index 5). Normals are not zero-blended. The GLSL path always uses the blended program: the
  `zeroBlend<=0.001` fast path is disabled with `if(false && …)`.
- **Purpose:** keep weapon models (and the hand anchor vertices on them) near the rest pose so firing and reload
  animations don't pull the gun away from the tracked hand; separate value when 2-hand aiming.
- **Ironwail:** alias instances go through `ALIAS_INSTANCE_BUFFER` (gl_shaders.h 1039-1058: `WorldMatrix[3], LightColor,
  Pose1, Pose2, Blend, Padding`) and `alias_vertex_shader` (1061-1160), which reads poses from the `PackedPosNor` SSBO by
  `pose + gl_VertexID`. r_alias.c 660-678 fills the instance and scales the pose index by the vertex count.
- **Isolation idea:** extend `InstanceData` with `int Pose0; float ZeroBlend` (use `Padding` plus 8 more bytes; keep
  std430 alignment). In the shader: `lerped = mix(lerped, GetPoseVertex(inst.Pose0).pos, inst.ZeroBlend)`, applied
  before the world matrix, for PV_QUAKE1/MD3 only. On the CPU, compute Pose0 like `R_SetupAliasFrameZero` and multiply by
  totalverts. The CPU helper used for anchor positions must use the same formula (next section).
- **Tag:** RENDER / GAMEPLAY

### CPU access to posed alias vertices (anchor vertices) — tri-strip order dependency
- **Where (QVR):** vr.cpp `VR_GetAliasVertexOffsets` 2093-2119, `VR_GetScaledAliasVertexOffsets` 2121-2168,
  `VR_GetScaledAndAngledAliasVertexPosition` 2186-2211; users view.cpp 1447-1456 (`HandAnchorVertex`) and the weapon
  cvars `MuzzleAnchorVertex`, `HandAnchorVertex`, `TwoHHandAnchorVertex`, `WpnButtonAnchorVertex`,
  `WpnTextAnchorVertex` (menu.cpp 2634-3286); debug display vr_showfn.cpp 636-698.
- **Upstream (BASE):** n/a.
- **Change:** runs `R_SetupAliasFrame` and `R_SetupAliasFrameZero` on the CPU for an entity. It reads
  `trivertx_t` from `hdr->posedata + pose*hdr->poseverts + anchorVertex`, lerps and zero-blends it, then applies
  flip/`scale`/`scale_origin`/angles/origin. **`posedata` is the array reordered by `BuildTris`/`vertexorder[]`**
  (gl_mesh.cpp 39-440): strip/fan order, with duplicated vertices. So cvar vertex indices refer to that order, not to
  the MDL vertex order. The index is clamped to `numverts` rather than `poseverts`, a minor bug.
- **Ironwail:** IW keeps `hdr->vertexes` in original MDL order (gl_mesh.c 56-59) and has no BuildTris, `commands`
  or `posedata`.
- **Isolation idea:** port `BuildTris` (StripLength/FanLength/BuildTris) into the VR module and compute
  `vertexorder[]` per model at load, storing it in extradata or a side table. Map anchor index → original vertex index
  and read IW `hdr->vertexes` (numverts per pose). The alternative is to re-author every weapon's anchor cvars. Do this
  before any weapon tuning is ported.
- **Tag:** VR-CORE / RISK

### Weapon, torso and holster model rescaling by mutating aliashdr
- **Where (QVR):** gl_model.hpp `aliashdr_t::original_scale`, `original_scale_origin` (new); gl_model.cpp
  `Mod_LoadAliasModel` 4152-4153 (saves originals); vr.cpp `VR_ApplyModelMod` 623-631, `ApplyMod_Weapon` 679,
  `VR_ModVRTorsoModel` 1137, `VR_ModVRLegHolsterModel` 1148, `VR_ModAllWeapons` 1163, `VR_ModAllModels` 1189 (called from
  common.cpp 2421, host.cpp 1300, menu.cpp).
- **Change:** `hdr->scale = original_scale * scale * scaleCorrect` and
  `hdr->scale_origin = (original_scale_origin + offsets) * scaleCorrect`, where
  `scaleCorrect = (vr_world_scale/0.75) * vr_gunmodelscale`. Scale and offsets come from per-weapon cvars
  (`vr_wofs_*`), `vr_vrtorso_[xyz]_scale`, and `vr_leg_holster_model_scale/_[xyz]_offset`. The data is modified in the
  cache (`Mod_Extradata`), so a cache flush reverts it. Model bounds are not recomputed.
- **Ironwail:** IW reads `paliashdr->scale/scale_origin` per draw (r_alias.c 610-611) and the GPU pose data is unscaled
  bytes, so mutating the header works the same way. `aliashdr_t` is in gl_model.h.
- **Isolation idea:** add the two `original_*` fields to IW `aliashdr_t`, set them in `Mod_LoadAliasModel`, and keep
  `VR_ApplyModelMod` in the VR module. A cleaner option is a per-model side table applied in the entity-transform hook,
  which avoids cache mutation.
- **Tag:** GAMEPLAY / RENDER

### Alias pipeline reverted to legacy QuakeSpasm (MD3/IQM/multi-surface/QF16 rendering removed)
- **Where (QVR):** gl_mesh.cpp 39-760 (`StripLength`, `FanLength`, `BuildTris`, `GL_MakeAliasModelDisplayLists`,
  `GL_MakeAliasModelDisplayLists_VBO`, single-surface `GLMesh_LoadVertexBuffer` 555-687); r_alias.cpp whole file;
  gl_model.hpp adds `meshxyz_t`, `aliashdr_t::poseverts/posedata/commands`, `qmodel_t::vboindexofs/vboxyzofs/vbostofs`.
- **Upstream (BASE):**
  - The VBO builder handles PV_QUAKE1, PV_QUAKEFORGE (16-bit), PV_QUAKE3 (MD3) and PV_IQM, across multiple surfaces
    (`nextsurface`), with CPU fallbacks (`meshvboptr`).
  - `R_DrawAliasModel` loops over surfaces and uses skeletal GLSL for IQM (`r_alias_glsl[ALIAS_GLSL_SKELETAL]`).
  - `EFLAGS_VIEWMODEL` entities are drawn relative to the view (identity modelview, depth range 0-0.3).
  - `numskins<=0` gives the checkerboard.
  - Lighting samples `r_refdef.vieworg` for viewmodel-eflag entities and applies colormod.
- **Change:** only PV_QUAKE1 layout (`meshxyz_t` byte xyz + byte normal) for a single surface. The fixed-function path uses
  tri-strip command lists (`commands`) again. The MD3/IQM loaders are still present (gl_mesh.cpp 850+, 1095+;
  gl_model.cpp 477/482) but produce no VBO or command list and are not rendered correctly. The GLSL shaders are GLSL 110
  with fixed-function fog (`gl_Fog`) and `gl_ModelViewProjectionMatrix`. `r_drawflat` per-vertex random colors are
  supported again in the legacy path. The QuakeForge 16-bit loader flag is still parsed, but only the high byte is used.
- **Purpose:** probably to keep the author's zeroBlend and anchor code simple.
- **Ironwail:** IW supports MDL/MD3/IQM, multi-surface and GPU skinning (gl_mesh.c, r_alias.c).
- **Isolation idea:** do not port; keep IW's pipeline and add only the zeroBlend, flip, scale and lightmod hooks.
  VR content only uses MDL.
- **Tag:** REMOVED

### Alias shadows enabled, including for VR view entities
- **Where (QVR):** gl_rmain.cpp cvar `r_shadows` default "0"→"1" (93); `R_DrawShadows` 1380-1453; r_alias.cpp
  `GL_DrawAliasShadow` 1538-1599 (the viewent early-out is commented out); cvar `vr_player_shadows` (vr_cvars.cpp 136,
  default 2; enum `VrPlayerShadows {Off=0, ViewEntities=1, ThirdPerson=2, Both=3}`).
- **Upstream (BASE):** `r_shadows 0`. The loop `return`s at `cl.viewent`.
- **Change:** the visedict loop skips view entities (`continue`). Then: ViewEntities or Both draws the shadow for
  every `forAllViewmodels` entity that has a model. ThirdPerson or Both draws the shadow of
  `cl.entities[cl.viewentity]` (the player body, which isn't otherwise drawn). The shadow uses the stencil (MarkV)
  with the fixed-function projected matrix.
- **Ironwail:** IW **removed r_shadows entirely** (no `GL_DrawAliasShadow`).
- **Isolation idea:** new `vr_shadows.c`. Draw each shadow-casting alias entity as an extra alias instance with a
  flattened world matrix (shadowmatrix skew −0.7, lheight from `R_LightPoint` lightspot), black at alpha 0.5×entalpha,
  with a stencil state. IW's `GL_SetState` has no stencil flags, so raw GL is needed. This could be deferred or dropped
  (low priority).
- **Tag:** RENDER

### World text (QC-driven 3D text) and weapon ammo text
- **Where (QVR):** gl_rmain.cpp `R_DrawWorldText` 831-913 (called in `R_RenderScene` after the opaque entities, 1477),
  `R_DrawString` 1030-1183 (ammo text, `R_RenderScene` 1506-1541); cvar `r_drawworldtext` "1" (87, registered
  gl_rmisc.cpp 238); worldtext.hpp (`WorldText{_text,_pos,_angles,_hAlign(Left/Center/Right),_scale}`,
  `WorldTextHandle=uint16_t`, max 65535); client state `cl.worldTexts` (std::vector) filled by svc messages in
  client.cpp; stats `STAT_WEAPONCLIP/CLIPSIZE/AMMOCOUNTER(2)`; cvar `vr_show_weapon_text` (default 1).
- **Upstream (BASE):** none.
- **Change:** conchars glyph quads (16×16 atlas, UV = col/row × 0.0625) are drawn with alpha test, no blend, no cull,
  white, in one `GL_QUADS` batch.
  - Each string is split on '\n'. `charSize = 8*scale`, `hInc = right*charSize`.
  - World text: `zInc = (0,0,-charSize) * up` (a **component-wise** multiply, so only the z of `up` is used).
    Ammo text: `zInc = up*8*scale`.
  - The block is centred on the position (`center = pos − (hInc*maxLen + zInc*nLines)/2`), and each line is offset by
    alignment (Center: +sizeDiff/2·hInc; Right: +sizeDiff·hInc). Spaces are skipped.
  - Ammo text (per hand, if `!textEnt.hidden`): the angles are `pitch −= 180`, `roll = −roll`. The text is
    `"%d/%d\n%d"` (clip/clipsize/ammo) when weapon reloading is enabled and clipsize ≠ 0, else `"%d"` (ammo). The scale
    is `0.10*textEnt.scale`, centred.
- **Ironwail:** nothing similar. IW `char_texture` is a **160×160 padded atlas** (gl_draw.c 457-480; UV =
  `col*0.0625 + 1/160`, size `8/160`, see `Draw_CharacterQuadEx` 721). Core profile has no immediate mode.
- **Isolation idea:** new `r_worldtext.c`. Build textured quads in world space using the IW UV formula, and draw them with a
  small textured shader, e.g. reuse the `sprites` program (gl_shaders.h 1232: pos, uv, color, `ViewProj` from framedata)
  with alpha-test/discard. Call it from `R_RenderScene` after `R_DrawEntitiesOnList(false)`, and draw ammo text in the
  view-entity pass.
- **Tag:** RENDER / GAMEPLAY

### VR crosshair (laser/dot) and debug "show" helpers
- **Where (QVR):** gl_rmain.cpp `R_RenderScene` 1491-1494 (`show_crosshair()` before view entities) and 1592-1595
  (`draw_all_show_helpers()`); vr_showfn.cpp 275-400 (crosshair), 777-795 (helpers), 797-815; gl_util.hpp/.cpp (new:
  `gl_beginend_guard`, `gl_showfn_guard` = no depth test, GL_LINE polygon mode, polygon offset SHOWTRIS, alpha blend, no
  texture, no cull; `draw_points_with_size`, `draw_line_strip`). Cvars (vr_cvars.cpp): `vr_crosshair` 1
  (0 off, point, line, smooth line; enum VrCrosshair), `vr_crosshair_depth` 0, `vr_crosshair_size` 3.0,
  `vr_crosshair_alpha` 0.25, `vr_crosshairy` 0.
- **Change:** a crosshair is drawn per hand (skipped when the weapon's `WpnCrosshairMode::Forbidden`) from
  `VR_CalcFinalWpnMuzzlePos(hand)` along the hand's forward vector.
  - **Point mode:** trace to 4096 (`vr_crosshair_depth<=0`) with the server-side `TraceLine`, or use a fixed depth. Draw a
    red `GL_POINTS` point with `GL_POINT_SMOOTH`, size `size*glwidth/vid.width`.
  - **Line modes:** `TraceLineToEntity` against the player edict. Draw a red line strip with `GL_LINE_SMOOTH` and line
    width `size*glwidth/vid.width`. Smooth mode fades to alpha×0.01 at both ends (points at 0.15 and 0.70).
  - Only drawn if `svPlayerActive()`. It uses **server-side traces from the render loop**, which is why
    `SCR_UpdateScreen` switches to `sv.qcvm`.
  - The helpers draw virtual stock, holsters, torso lines, teleport arc, weapon offset/muzzle/2H helpers, hand
    pos/rot, anchor vertices (points in several sizes, vertex index ±500) and the menu intersection point.
- **Ironwail:** IW has a debug line batcher: gl_rmain.c `R_EmitLine` 1256, `R_EmitWirePoint` 1276, `R_EmitWireBox`,
  `R_EmitArrow`, `R_FlushDebugGeometry` 1188 (`glprogs.debug3d`, GL_LINES, alpha blend, optional z-test). Core profile
  has no point smooth, no wide lines and no per-vertex alpha fade issues.
- **Isolation idea:** port the helpers to `R_EmitLine`, with points as small camera-facing quads or wire crosses. For
  thick or soft lasers, add a camera-facing ribbon (quad strip) using the particle or sprite shader. Keep the traces in
  the VR module (it could use the client-side `CL_TraceLine` equivalent instead of `sv`).
- **Tag:** UI / RENDER

### HUD, menus and console in 3D (2D pass redirected into world space)
- **Where (QVR):** gl_screen.cpp `SCR_UpdateScreenContent` 1352-1355; vr.cpp `VR_Draw2D` 3598-3752, `VR_DrawSbar`
  3754-3849; gl_draw.cpp `GL_SetCanvas` 932-935 (early return in VR), `Draw_FadeScreen` 886-889 (no-op when
  vr_enabled). Cvars: `vr_menu_scale` 0.13, `vr_menu_distance` 76, `vr_menumode` 0 (LastHeadAngles, FollowHead,
  FollowOffHand, FollowMainHand), `vr_hud_scale` 0.025, `vr_sbar_mode` 0 (MainHand, OffHand), `vr_sbar_offset_{x,y,z,
  pitch,yaw,roll}`.
- **Upstream (BASE):** `GL_Set2D` + orthographic canvases.
- **Change:**
  - In real VR, and when `!con_forcedup`, the 2D pass runs inside the eye's 3D modelview/projection. `glwidth/glheight`
    and `vid.conwidth/conheight` are forced to 320×200.
  - A matrix is pushed with depth test off and blend on. The anchor is either `vieworg + vr_menu_distance*fwd(angles)`,
    where angles = the frozen menu angles while in the menu, else `cl.viewangles` (pitch zeroed in head aim modes), or
    `handpos + dist*fwd(handrot)`.
  - The anchor is smoothed with `mix(last, target, 0.9)`. Then: rotate yaw−90 around Z, pitch+90 around −X, translate
    by `(−160*scale, −100*scale)`, scale by `vr_menu_scale`.
  - It then draws the dialog/loading/intermission/menus/console/`M_DrawKeyboard` (the flat crosshair is skipped).
  - Sbar: `VR_DrawSbar` attaches to a hand in controller aim mode. MainHand: `handpos − 5*right`, rotated like the
    other modes. OffHand: `quatLookAt(fwd,up)` × pitch/yaw/roll offsets + xyz offsets. Other aim modes use
    `viewent.origin + fwd` with a 135° tilt and 10 units down. Scale is `vr_hud_scale`.
  - `GL_SetCanvas` does nothing in this mode, so the canvas ortho/viewport never override the eye matrices.
- **Ironwail:** IW GUI vertices are transformed on the CPU to **NDC** (`Draw_SetVertex` with
  `glcanvas.transform`, gl_draw.c 703) and drawn by `gui_vertex_shader` (gl_shaders.h 40-53: `gl_Position =
  vec4(in_pos,0,1)`, no matrix) in batches (`Draw_Flush` 584). Canvases are `Draw_GetCanvasTransform` 1198.
- **Isolation idea:** two options.
  - (a) Render the whole 2D pass once per frame into an offscreen "UI texture" (320×200 or larger) with the normal IW
    canvases. Then draw that texture as world-space quads per eye: menu quad, sbar quad, using the sprite or debug
    program with `ViewProj`. This keeps gl_draw.c untouched and is recommended.
  - (b) Add a `mat4 GuiMVP` uniform to the gui shader and set it per eye.
  The anchor math goes in the VR module.
- **Tag:** UI / VR-CORE

### 2D canvas changes (menu 800×600, console canvas, notify canvas, wider sbar) and scaled text
- **Where (QVR):** gl_draw.cpp `GL_SetCanvas` 938-1009; `Draw_CharacterQuad/Draw_Character/Draw_String` 592-660
  (new `float scale=1.f` parameter, draw.hpp 37/52); console.cpp 1432 uses new `CANVAS_NOTIFY`.
- **Upstream (BASE):**
  - `CANVAS_CONSOLE` is ortho `(0,conwidth, conheight+lines, lines)`.
  - `CANVAS_MENU` is ortho 640×200 with a 320×200·s viewport.
  - `CANVAS_SBAR` (non-DM) is ortho 320×48.
  - `Draw_Character` returns early if `y <= -8`.
- **Change:**
  - `CANVAS_CONSOLE` becomes the same as the new menu canvas: ortho 800×600, `s = clamp(1, scr_menuscale,
    min(glw/800, glh/600))`, centred 800s×600s viewport.
  - New `CANVAS_NOTIFY` keeps the old console ortho.
  - `CANVAS_MENU` is ortho 800×600 with the same viewport.
  - Non-DM `CANVAS_SBAR` is ortho `(-200,420,48,0)` with viewport x `(glw−420s)/2`, width 600s.
  - The character quad size is `8*scale`. The `y<=-8` cull was removed from `Draw_Character`.
- **Purpose:** larger VR menus (menu and UI inventory); extra HUD space left of the status bar.
- **Ironwail:** gl_draw.c `Draw_GetCanvasTransform` 1198-1270 (MENU 320×200, CONSOLE uses conwidth, SBAR 320×48 plus
  QW/`CANVAS_SBAR2` styles), `Draw_CharacterEx`/`Draw_StringEx` 756-808 already take a size.
- **Isolation idea:** add `CANVAS_VRMENU` (800×600) and use it from the VR menus only. Map
  `Draw_Character(x,y,n,scale)` to `Draw_CharacterEx(x,y,8*scale,8*scale,n)`. Keep IW's console canvas. Revisit the
  sbar width together with the sbar inventory.
- **Tag:** UI

### GLSL gamma per eye (and no longer applied in flat mode)
- **Where (QVR):** gl_rmain.cpp `GLSLGamma_GammaCorrect(int eyeIndex)` 229-321; gl_screen.cpp 1411-1415 (call
  commented out); vr.cpp 1616.
- **Upstream (BASE):** called once at the end of `SCR_UpdateScreen` on the back buffer.
- **Change:**
  - The copy source is `glBindFramebuffer(GL_READ_FRAMEBUFFER, VR_GetEyeFBO(eye).framebuffer)`.
  - The texture is re-created when it is smaller than glwidth/glheight (bug fix for eye sizes above the window), with
    format `GL_UNSIGNED_BYTE` instead of `_8_8_8_8_REV`.
  - `GL_CULL_FACE` is disabled during the full-screen quad.
  - Only called from the VR eye path, so **in non-VR/fakevr mode vid_gamma/vid_contrast have no GLSL effect**
    (regression).
- **Ironwail:** gamma, contrast and dither are in `GL_PostProcess` (gl_rmain.c 331) → FB 0.
- **Isolation idea:** use IW `GL_PostProcess` with a target-FBO parameter per eye (see the stereo loop).
- **Tag:** RENDER / BUGFIX

### GL loading: GLEW instead of manual function pointers; compatibility profile; debug output
- **Where (QVR):** gl_vidsdl.cpp `GL_Init` 1203-1277 (`glewExperimental=GL_TRUE; glewInit()`, debug callback
  `MessageCallback` 1185-1203 enabled when `!NDEBUG`, `GLAliasBlended_CreateShaders()` in place of
  `GLAlias_CreateShaders()`); `GL_CheckExtensions` 930-1156 (`GLEW_ARB_*` checks; compressed texture support via
  `GLEW_EXT_texture_compression_s3tc/ARB_rgtc/EXT_bptc/ARB_ES3_compatibility/KHR_astc_ldr`); all `GL_*Func` pointers
  and `GL_CompressedTexImage2D` removed from glquake.hpp; `#include <GL/glew.h>` in many files; gl_rmisc.cpp removes
  `GL_CreateProgram`/`R_DeleteShaders` (replaced by shader.cpp `quake::make_gl_program` / `gl_program_builder`, which
  also supports geometry shaders, used by r_part 2373). No context version or profile attributes are set, so it is a
  legacy compatibility context.
- **Change:** behaviour is mostly unchanged. Side effects:
  - GLSL programs are never deleted on `vid_restart` (leak).
  - A GL 3.2+ compatibility context is assumed (geometry shaders, `glTexImage2DMultisample`, `glBlitFramebuffer`,
    `glDebugMessageCallback`).
  - `VID_Restart` calls `VID_VR_Disable()` before the mode change and `VR_Enable()` after it (gl_vidsdl.cpp 741-744,
    805-808).
  - `VR_Enable` disables vsync.
- **Ironwail:** IW loads functions itself through the `QGL_*_FUNCTIONS` X-macros (glquake.h 130-244, gl_vidsdl.c 98-137,
  992) and creates a **core 4.3** context (gl_vidsdl.c 466-468). It already has `glDebugMessageCallback` plumbing
  (`GL_BeginGroup`/`ObjectLabel`).
- **Isolation idea:** do not add GLEW. Any extra GL entry points the VR module needs (all core in 4.3: blit, multisample,
  framebuffers) are already in IW's list or can be added to `QGL_CORE_FUNCTIONS`. OpenVR only needs the GL texture name.
  Keep the VR disable/enable around `vid_restart`, and do the vsync-off in the VR module.
- **Tag:** MISC

### R_SetupView: QSS FTE_ENT_SKIN_CONTENTS view-contents loop removed
- **Where (QVR):** gl_rmain.cpp `R_SetupView` 699-714 and 721-725.
- **Upstream (BASE):** 587-624: a loop over brush `cl.entities` computes the view contents from moving brush models
  (skin <0 = contents) for `V_SetContentsColor` and waterwarp.
- **Change:** it only uses `r_viewleaf->contents`. Waterwarp re-queries the world leaf.
- **Ironwail:** IW `R_SetupView` (gl_rmain.c 965-1063) also uses only the world leaf, plus
  `cl.forceunderwater`/`M_ForcedUnderwater`.
- **Isolation idea:** nothing to port.
- **Tag:** REMOVED

### Removed QSS render features: EFLAGS_EXTERIORMODEL skip, EFLAGS_VIEWMODEL, colormod, scripted particles
- **Where (QVR):** gl_rmain.cpp `R_DrawEntitiesOnList` 791-829 (the `EFLAGS_EXTERIORMODEL` continue was removed);
  `R_RenderScene` 1487 (`PScript_DrawParticles` removed); r_alias.cpp (see the alias sections); r_part_fte.cpp deleted
  (declarations still in glquake.hpp 101-131); `R_RenderScene` no longer sets `currententity = &r_worldentity`.
- **Change:** entities flagged exterior-model (CSQC/QSS player body) are drawn in first person. Viewmodel-flagged
  entities are drawn in world space. Colormod is ignored. There are no FTE particle scripts (see Particles).
- **Ironwail:** IW has none of these (no eflags, no PScript) either, so there is nothing to port.
- **Tag:** REMOVED

### Skyroom camera loses roll
- **Where (QVR):** gl_rmain.cpp `R_RenderView` 1791-1803 (`VectorAngles(axis[0])`, "TODO VR: (P0) QSS Merge").
- **Upstream (BASE):** `VectorAngles(axis[0], axis[2], …)` (with up vector).
- **Change:** the rotating skyroom orientation ignores the up vector.
- **Ironwail:** IW has no skyroom.
- **Tag:** REMOVED / MISC

### Debug and minor cvars
- **Where (QVR):** gl_rmain.cpp:
  - `r_showbboxes_player` "0" (122, registered gl_rmisc.cpp 274): `R_ShowBoundingBoxes` 1241 draws the player's own
    bbox when set; it now does `PR_SwitchQCVM(nullptr)` before switching (QVR's switch asserts).
  - `R_EmitWirePoint` cross size 8→4 (1185).
  - `r_lavaalpha/r_telealpha/r_slimealpha` now `CVAR_ARCHIVE` (140-142).
  - `r_shadows` default 1 (see shadows).
- **Ironwail:** IW has a richer `R_ShowBoundingBoxes` (gl_rmain.c 1478) and `gl_zfix` etc.
- **Isolation idea:** optional; `r_showbboxes_player` is a trivial filter in `R_ShowBoundingBoxesFilter`.
- **Tag:** MISC

### Liquid alpha: map "vised" checks bypassed at map load
- **Where (QVR):** gl_rmisc.cpp `R_ParseWorldspawn` 417-420.
- **Upstream (BASE):** `map_*alpha` = cvar only if `cl.worldmodel->contentstransparent` has the corresponding
  SURF_DRAW* bit, else 1. `map_fallbackalpha = r_wateralpha`.
- **Change:** `map_wateralpha/lavaalpha/telealpha/slimealpha` are set directly from the cvars regardless of the map's
  water-vis state. `map_fallbackalpha` is no longer reset. Worldspawn keys still override.
- **Ironwail:** IW `R_ParseWorldspawn` (gl_rmisc.c) handles this in its own way.
- **Isolation idea:** ignore; preference-level change.
- **Tag:** MISC

### Stereo/anaglyph (`r_stereo`) retained but unused by VR
- **Where (QVR):** gl_rmain.cpp `R_RenderView` 1821-1851 (only style changes).
- **Ironwail:** removed in IW; not needed.
- **Tag:** MISC

### refdef_t gains `aimangles`
- **Where (QVR):** refdef.hpp (new header, split from render.hpp) adds `qvec3 aimangles`; set in gl_screen.cpp 1487/1496
  and vr.cpp 3503; read in view.cpp 739/751 (gun/aim calc) and 927.
- **Ironwail:** IW `refdef_t` (render.h 96-106) is slimmer (vrect, vieworg, viewangles, basefov, fov_x/y, scale).
- **Isolation idea:** add `aimangles` to IW refdef_t, or keep it in a VR global.
- **Tag:** VR-CORE

### Particles: rewritten system (SoA pool, texture atlas, geometry-shader billboards, presets)
- **Where (QVR):** r_part.cpp (whole file, 2632 lines).
  - Pool: `MAX_PARTICLES` 48, `ParticleSOA` 84-102, `ParticleBufferSOA::cleanup` 243-264, `ParticleManager` 443-515,
    spawn helpers 544-577.
  - Textures: `R_InitParticleTextures` 702-743, `stitchImages` 357-396.
  - Update: `CL_RunParticles` 2045-2231.
  - Rendering: shaders 2233-2374, `R_DrawParticles` 2409-2593.
  - Presets 1178-1661, explosions 961-1176, `R_TeleportSplash` 1707, `R_RocketTrail` 1837-2037.
  - render.hpp declarations 83-95.
- **Upstream (BASE):** classic Fitz particles: linked free list, 2048 default, palette colour, `pt_grav/slowgrav`,
  `r_particles` 1/2 texture switch, `r_quadparticles`, immediate-mode quads/tris.
- **Change:** pool, data layout and cvars:
  - Default pool is 409,600 (`4096*100`), `-particles` still overrides it (minimum 512).
  - Structure-of-arrays storage: org, vel, **acc** (new per-particle acceleration), float RGBA colour, angle, scale,
    atlasIdx, `{ramp, die, type, param0}`. Stable compaction kills particles at alpha≤0, scale≤0 or die.
  - `pt_grav/pt_slowgrav` removed; gravity is now `acc = (0,0,-sv_gravity*mult)` with default mult **0.5**
    (vanilla was 0.05, so 10× faster falling), and it also applies to `pt_static`.
  - New types: `pt_txexplode, pt_txsmoke, pt_lightning, pt_teleport, pt_rock, pt_gunsmoke, pt_gunpickup,
    pt_txbigsmoke`, with per-type alpha/scale/angle rates in `CL_RunParticles`.
  - Cvars: `r_particles` "1" (ARCHIVE; 0 also freezes simulation), new `r_particle_mult` "1" (ARCHIVE; multiplies
    every spawn count; menu range 0.25-10). `r_quadparticles` removed.

  Textures:
  - A 1026×128 RGBA atlas stitched from procedural circle/square/blob textures plus 8 TGAs
    (`textures/particle_explosion, _smoke, _blood, _blood_mist, _lightning, _spark, _rock, _gun_smoke`; `Sys_Error` if
    one is missing; shipped in ReleaseFiles/Id1).
  - Atlas indices: 0 circle, 3 explosion, 4 smoke, 5 blood, 6 blood mist, 7 lightning, 8 spark, 9 rock, 10 gun smoke.

  Rendering:
  - GLSL 430 vertex, geometry and fragment program. There is one VBO per SoA field, re-uploaded every frame, drawn with
    `GL_POINTS`.
  - The geometry shader expands each point to a camera-facing quad rotated by `angle`, with side `scale*1.5` world
    units (no depth-scale hack).
  - The fragment shader samples an atlas sub-rect (uniform `vec4 atlasBuf[11]` at location 12), outputs `tex*color`
    and discards when a<0.01.
  - Blend state is SRC_ALPHA/ONE_MINUS_SRC_ALPHA with depth mask off, no sorting and no fog. Matrices are read back
    via `glGetFloatv`. `R_DrawParticles_ShowTris` is an empty stub.
  - Drawn per eye (billboards face each eye); simulated once per frame.

  Effects:
  - `R_ParticleExplosion`/`R_ParticleExplosion2`/`R_TeleportSplash`/`R_LavaSplash`/`R_EntityParticles` are
    rewritten with sprites, rocks, sparks and smoke. `R_BlobExplosion` is removed (TE_TAREXPLOSION uses
    `R_ParticleExplosion`).
  - The generic `R_RunParticleEffect` is replaced by `R_RunParticleEffect_{BulletPuff(org,dir,color,count), Blood,
    Lightning, Smoke, BigSmoke, Sparks, GunSmoke, Teleport, GunPickup, GunForceGrab, LavaSpike}`.
  - `R_ParseParticleEffect` (svc_particle) always does BulletPuff, so the vanilla "count 255 = explosion" rule is lost.
  - New `R_ParseParticle2Effect`/`R_RunParticle2Effect(org, dir, preset, count)` with presets 0 BulletPuff,
    1 Blood, 2 Explosion, 3 Lightning, 4 Smoke, 5 Sparks, 6 GunSmoke, 7 Teleport, 8 GunPickup, 9 GunForceGrab,
    10 LavaSpike, 11 BigSmoke.

  Trails (`R_RocketTrail`):
  - A single **global** rate limiter (~90 Hz) is shared by all entities, which is quirky.
  - Fire trails fall.
  - New trail type 7 is the mini-rocket.
  - Type 4 has a `len -= 3` inside the per-particle lambda (bug).

  Callers:
  - cl_tent maps spike and gunshot TEs to BulletPuff with colours (0/20/226).
  - cl_main 884-931: `EF_MINIROCKET` selects trail 7, and new `EF_LAVATRAIL` spawns LavaSpike(4) every frame.
  - vr.cpp 2631 spawns teleport particles at the teleport target every frame.
- **Purpose:** better-looking VR effects; QC-driven presets (weapons, blood, buttons, grapple, force-grab).
- **Ironwail:** r_part.c. `particle_t` is in glquake.h:53 (`org, byte color, byte type, spawn, die, vel, ramp`),
  compacted in `CL_RunParticles` 566-648, default 16384. The instance vertex is `particlevert_t {vec3 pos; ubyte
  color[4]}`, uploaded via `GL_Upload` and drawn with `glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,4,n)` (655-671). The
  vertex shader (gl_shaders.h 1286-1321) billboards in clip space (`Params` = ProjScale, UVScale). The fragment shader
  (1325-1352) draws an analytic circle with no texture, and handles fog, OIT and dither. Opaque (`r_particles 2`) and
  alpha passes are at gl_rmain.c 1927/1939. IW's trail limiter is per entity (`ent->traildelay`, cl_main.c 481-497).
- **Isolation idea:**
  - Extend `particle_t` with `acc, alpha, scale, angle, atlas, param0` and add `vel += acc*dt`.
  - Port the preset functions as C in a new `r_part_vr.c` built on `R_AllocParticle`, and add `r_particle_mult`.
  - Extend `particlevert_t` with scale (loc 2), angle (loc 3) and atlas (loc 4, integer attribute), and bump
    `GLS_ATTRIBS`.
  - In the vertex shader, rotate `corner` in 2D by `angle` and size it by world scale (×0.75 half-size).
  - In the fragment shader, sample an atlas: prefer a `GL_TEXTURE_2D_ARRAY` of 128² layers over the 1026-wide strip
    to avoid linear-filter bleeding. Keep the analytic circle for index 0.
  - Route textured particles only through the alpha/OIT pass.
  - Raise `MAX_PARTICLES` (the draw batch can still flush).
  - Keep IW's per-entity trail limiter and IW's extra TE handling.
  - Protocol, QC and effect-bit parts (`svc_particle2 = 45`, builtin #79 `particle2`, `EF_MINIROCKET`/`EF_LAVATRAIL`
    **clash with IW's EF_QEX_* bits 16/32/64**) belong to the protocol/QC inventories.
- **Tag:** RENDER / GAMEPLAY

### FTE scripted particles (r_part_fte.cpp) removed
- **Where (QVR):** r_part_fte.cpp deleted. `PSET_SCRIPT`/`PSET_SCRIPT_EFFECTINFO` are commented out
  (quakedef_macros.hpp 61-64), and glquake.hpp 118-129 stubs `PScript_*` to macros.
- **Change:** everything that depended on scripted particles is gone:
  - Loading: `r_particledesc` and DP effectinfo.
  - Cvars: `r_bouncysparks, r_part_rain*, r_part_sparks*, r_part_beams, r_part_density, r_part_maxparticles,
    r_part_maxdecals, r_lightflicker, r_decal_noperpendicular, r_particle_tracelimit, r_part_contentswitch`.
  - Commands: `r_partredirect, r_partinfo, r_beaminfo`.
  - Network: `svcdp_trailparticles/pointparticles(1)` parsing.
  - Effects: model trail/emit effects, static-entity emitters (gl_refrag), sky-surface particles and
    `PScript_DrawParticles`.
  - Builtins `particleeffectnum/trailparticles/pointparticles` become stubs (pr_ext.cpp 5526-5531); `te_particlerain/snow`
    are stubbed; FTE_PART_* and DP_TE_PARTICLERAIN/SNOW extension strings are removed.
  - Several extended TEs (TEDP_*QUAD, TEFTE_*, TENEH_*, TEDP_CUSTOMFLASH, BLOOD, SPARK, …) now hit
    `Sys_Error("bad type")` in `CL_ParseTEnt`.
- **Ironwail:** IW has no scripted particles either, but it keeps many of these TEs.
- **Isolation idea:** nothing to port; keep IW's TE handling.
- **Tag:** REMOVED

### Brush models: per-entity scale/offset/horizontal flip (+ backface-cull bypass)
- **Where (QVR):** r_brush.cpp `R_DrawBrushModel` 536-630 (transform 595-613, cull bypass 615-628);
  `R_DrawBrushModel_ShowTris` 639-720 (transform 682-697).
- **Upstream (BASE):** only `R_RotateForEntity`.
- **Change:** the same transform as alias models. If `horizFlip`: `glScalef(1,-1,1)`, `glFrontFace(GL_CCW)` (never
  restored to CW, a bug). Then `T(-model_scale_origin)·S(model_scale+1)·T(model_scale_origin)·T(model_offset)`. If all
  three `model_scale` components are ≠0, every surface is chained without the CPU plane-side test against `modelorg`
  ("TODO VR: (P2)"). Culling ignores the scale.
- **Purpose:** QC-scaled/mirrored bmodels (same entity fields as alias).
- **Ironwail:** r_world.c `R_InitBModelInstance` 199 (matrix from `R_EntityMatrix`), `R_DrawBrushModels_Real` ~458 and
  `R_DrawBrushModels_Water` ~549 (use `GLS_CULL_BACK`); culling in gl_rmain.c `R_GetEntityBounds` ~391. IW has no CPU
  per-surface backface culling for bmodels.
- **Isolation idea:** apply the same `VR_ApplyEntityTransform` in `R_InitBModelInstance`. Put flipped instances in a
  separate batch with `GLS_CULL_FRONT` (glquake.h 281), and expand the bounds. The cull bypass is not needed.
- **Tag:** RENDER / GAMEPLAY

### Sprites: QSS scale/colormod dropped, VP_PARALLEL_UPRIGHT bug
- **Where (QVR):** r_sprite.cpp `R_DrawSpriteModel` 100-214 (109-116, 174).
- **Upstream (BASE):** sprite scale `netstate.scale/16` (149); colormod colour (154).
- **Change:** scale is always 1 and colour is white. For `SPR_VP_PARALLEL_UPRIGHT` the right vector is never assigned
  (uninitialised qvec3), so the sprite width is undefined.
- **Ironwail:** r_sprite.c `R_DrawSpriteModel_Real` 188 handles scale and computes right correctly.
- **Isolation idea:** nothing to port.
- **Tag:** REMOVED

### r_oldwater default 1 (warp render-to-texture broken with eye FBOs)
- **Where (QVR):** gl_warp.cpp 40-41 (`r_oldwater` "0"→"1", ARCHIVE, "TODO VR: (P1) oldwater 0 is broken");
  r_world.cpp 816.
- **Change:** `R_UpdateWarpTextures` renders into the corner of the bound (eye, possibly MSAA) FBO and copies it back
  with `glCopyTexSubImage2D`, which is invalid on MSAA and wrong at VR sizes. So QVR defaults to the old subdivided
  water.
- **Ironwail:** water warp is done in shaders (no warp textures, no `r_oldwater`), so the problem does not exist.
- **Isolation idea:** nothing.
- **Tag:** VR-CORE / MISC

### Static entities in sky leaves dropped (accidental)
- **Where (QVR):** r_world.cpp `R_MarkSurfaces` 164-178: `R_StoreEfrags` moved inside
  `if(r_oldskyleaf || leaf->contents != CONTENTS_SKY)`.
- **Upstream (BASE):** efrags are stored for every visible leaf.
- **Ironwail:** gl_refrag.c `R_AddStaticModels` 157 has the BASE behaviour.
- **Isolation idea:** do not port (likely a bug from adding braces).
- **Tag:** MISC

### gl_texmgr: imagedump failure message
- **Where (QVR):** gl_texmgr.cpp `TexMgr_Imagedump_f` 378-400 (prints "Failure to write image %s").
- **Ironwail:** gl_texmgr.c 481/488. Optional.
- **Tag:** MISC

(Unchanged apart from style: gl_sky, gl_fog, gl_rlight, gl_refrag, gl_texmgr otherwise, r_world `GLWorld_CreateShaders`
now uses raw strings and `gl_program_builder`. None of these files was reverted to older QuakeSpasm: skyroom, BSPX
lmshift, compressed formats, r_skyfog etc. are all still present in QVR.)

### Per-eye pipeline cost/state assumptions (reference for the IW design)
- **QVR:** everything under `SCR_UpdateScreenContent` runs twice per frame:
  - `R_SetupView`: fog, viewleaf, frustum, `R_MarkSurfaces` (cached by viewleaf), `R_CullSurfaces`,
    `R_UpdateWarpTextures`, `R_Clear`.
  - `R_SetupScene`: `R_PushDlights`, `R_AnimateLight`, `r_framecount++`. Because `r_framecount` advances, dynamic
    lightmaps are rebuilt and uploaded twice.
  - The 2D pass and particle drawing.
  Simulation (`CL_RunParticles`, entity lerp state) runs once. Lerp state is mutated during drawing
  (`R_SetupAliasFrame`/`R_SetupEntityTransform` write `e->lerpstart` etc.), and this is idempotent within the same
  `cl.time`.
- **Ironwail:**
  - `R_SetupView` (gl_rmain.c 965-1063) holds `r_framecount++`, `R_AnimateLight`, `Sky_SetupFrame`,
    `Fog_SetupFrame`, `R_SetFrustum`, `R_MarkSurfaces` (compute cull into the single shared
    `gl_bmodel_indirect_buffer`, per eye's planes and vieworg for backface culling), `R_SortEntities` and
    `R_PushDlights` (compute light clustering from `r_matproj`/`r_matview`).
  - The order must be L-setup, L-draw, R-setup, R-draw. Marking both eyes first would overwrite the indirect buffer.
  - `R_UploadFrameData` (UBO) must be issued per eye.
  - `r_framecount` advancing per eye is harmless in IW (no CPU lightmap rebuild).
- **Tag:** VR-CORE

## Dependencies on other subsystems
- **View (view.cpp):** positions all VR view entities (`V_RenderView_HandModels`, holsters, torso, weapon buttons and
  text), sets `hidden`, `horizFlip`, `lightmod`, `zeroBlend`, `vr_viewOffset` → `r_refdef.vieworg`,
  `VR_AddOrientationToViewAngles`, and uses `r_refdef.aimangles`.
- **VR core (vr.cpp):** eye poses, FBOs, OpenVR submit, `VR_Draw2D`/`VR_DrawSbar`, weapon cvars (`vr_wofs_*`: scale,
  offsets, anchor vertices, zeroblend, crosshair mode), `VR_ModAllModels`, finger tracking frames, teleport particles.
  `SCR_UpdateScreen` switches to the server QCVM because crosshair and helpers use server traces.
- **Protocol/client:** entity fields `model_scale`, `model_scale_origin`, `model_offset` (baseline + updates,
  `msg_scales` lerp); world text svc messages → `cl.worldTexts`; stats `STAT_WEAPONCLIP[2]`, `STAT_WEAPONCLIPSIZE[2]`,
  `STAT_AMMOCOUNTER[2]`; `svc_particle2 = 45`; `EF_MINIROCKET`/`EF_LAVATRAIL` (clash with IW `EF_QEX_*`).
- **QC/progs:** builtin #79 `particle2(vector o, vector d, float preset, float count)`; world text builtins; edict
  fields `model_scale*`.
- **Menu/UI:** 800×600 menu canvas, `M_DrawKeyboard` (drawn in the 2D pass, including in VR), `r_particle_mult` menu
  entry, weapon-offset editor using the showfn helpers; `Draw_Character/Draw_String` scale parameter.
- **Assets:** `textures/particle_*.tga` (8 files), `progs/vrtorso.mdl`, `progs/legholster.mdl`, hand/finger models.
- **Video:** `VID_Restart` must disable and re-enable VR; vsync is off in VR.

## Open questions
- Anchor vertices: confirm that all shipped weapon anchor cvar values assume BuildTris order. If so, port BuildTris only
  to compute a remap table (deterministic given the same triangle list and algorithm).
- 2D-in-VR: render the UI to a texture once per frame (recommended) or keep per-eye 3D-transformed GUI batches? The
  first is simpler with IW's NDC-space GUI batching.
- Should view entities be merged into `cl_visedicts` (automatic OIT/translucency, sorting, showtris) or kept in a
  separate pass like QVR? QVR draws them after everything else with no depth hack.
- Particle gravity ×10 (0.5·sv_gravity) and the global trail rate limiter: are these intentional behaviours to
  keep?
- Alias shadows: IW removed `r_shadows`; is the VR player/hand shadow (`vr_player_shadows`, default 2 = third-person
  body) important enough to reimplement?
- Near plane: QVR uses 4 units in VR; IW computes 0.5–4 from FOV. Pick a fixed VR near value that suits IW's
  reversed-Z and light clustering.
- The mirror-window blit source rectangle in QVR looks wrong (width/height swapped); decide on the desired mirror view
  (single eye, cropped).
- QVR's non-VR mode no longer applies GLSL gamma. IW will apply it via postprocess in both modes; confirm that
  `ColorSpace_Gamma` submission after IW's gamma postprocess gives the expected brightness in the HMD.
