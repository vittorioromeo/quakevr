# View, HUD, menus, console & input — Quake VR functional changes

## Summary
- **View (`view.cpp`)**: the largest functional area. In VR `V_CalcRefdef` stops being "eye = origin + viewheight + bob": the eye comes from `vr_viewOffset`, and bob, roll, pitch drift, `V_BoundOffsets`, the death roll and (optionally) the view kick are all turned off. The weapon view model is placed at the tracked hand. `V_RenderView` also builds many new client-side entities every frame: the off-hand weapon, 4 holstered weapons, 4 holster slots, 2×6 finger/hand models, the torso, 2 weapon buttons and 2 weapon text entities.
- **HUD/2D**: QVR does not draw 2D in screen space in VR. `VR_Draw2D` (vr.cpp) draws the whole 2D pass (menu, console, centerprint and so on) on a 320×200 quad placed in the world. `VR_DrawSbar` attaches the status bar to a hand. `GL_SetCanvas` does nothing in VR. The sbar has a second ammo slot (off-hand) and a clip/reload counter. CSQC HUD hooks were **removed**.
- **Menus**: the whole QSS menu system was rewritten into a data-driven `quake::menu` class (`menu_util.*`). There are 3 new VR hubs with 30 sub-pages that expose about 200 cvars, plus a draggable in-world virtual keyboard (`menu_keyboard.*`).
- **Input**: the VR controllers do not touch `in_sdl` at all. `VR_DoInput` (vr.cpp) reads SteamVR actions and calls `Key_Event` with fixed keys (`'k' 'l' 'n' 'm' '1' '3' '4' '5' '7' '8'`, `K_MOUSE1/2`, `K_SPACE`, `K_ESCAPE`, the arrows and `K_ENTER`). The shipped `config.cfg` binds those keys to VR commands. The menu laser pointer is a ray/plane intersection that yields `vr_menu_mouse_x/y`.
- **Console/cmd/cvar**: mostly churn. The real changes are cvar handles (for the QC builtins `cvar_hmake`, `cvar_hget`, `cvar_hset` and `cvar_hclear`), `developerMode()` (forced on in debug builds), `NUM_CON_TIMES` 8, and the **removal** of the client's replies to `cmd protocols` and `cmd pext`.
- **Biggest Ironwail risks**:
  1. IW's 2D and 3D paths are core-profile (shaders, `drawtransform_t`, no `glBegin`/matrix stack). QVR's immediate-mode `glPushMatrix`/`glTranslatef` world-space 2D, and the `glBegin`-based keyboard, cannot be reused. The GUI must go to an FBO that is then drawn as a textured quad in each eye.
  2. IW's menu code is C, macro/table driven and has mouse support. The ~3,400 lines of C++ lambda menus must become a data table.
  3. `V_CalcRefdef` differs in IW (`cl.viewheight`, `cl_entities`, `punchtime` lerp, `LERP_FINISH`, `V_RestoreAngles`).
  4. IW's sbar has three HUD styles, and the ammo semantics change (`STAT_AMMO` becomes an ammo-type id) touches all three.

## Changes

### View: VR disables bob, roll and pitch drift
- **Where (QVR):** view.cpp `V_CalcRoll` (109), `V_CalcBob` (154), `V_StartPitchDrift` (202), `V_DriftPitch` (243)
- **Upstream (BASE):** roll by `cl_rollangle`; bob by `cl_bob*` (with a `cl_bobcycle==0` guard); pitch drift when lookspring is on.
- **Change:**
  - `V_CalcRoll`: value = 0 if `vr_enabled`.
  - `V_CalcBob`: returns 0 if `vr_enabled`. The BASE guard `if(!cl_bobcycle.value) cycle=0` was **removed**, so `cl_bobcycle 0` now divides by zero.
  - `V_DriftPitch`: returns early if `vr_enabled`.
  - `V_StartPitchDrift` (the `centerview` command): if `VR_EnabledAndNotFake()`, it calls `VR_ResetOrientation()` and returns. In VR, "centerview" therefore recenters the VR yaw.
- **Purpose:** prevent motion sickness; give `centerview` a VR meaning.
- **Ironwail:** view.c 80/111/147/184 are the same as BASE (IW keeps the cl_bobcycle guard).
- **Isolation idea:** add `if (VR_Active()) return 0;` at the top of the roll and bob functions and an early return in `V_DriftPitch`. `V_StartPitchDrift` gets `if (VR_Active()) { VR_ResetOrientation(); return; }`.
- **Tag:** VR-CORE

### View: damage kick gated by `vr_viewkick`; CSQC_Parse_Damage removed
- **Where (QVR):** view.cpp `V_ParseDamage` (336-416)
- **Upstream (BASE):** called `CSQC_Parse_Damage` (which could suppress the effect), then always applied the `v_dmg_roll/pitch` kick.
- **Change:**
  - The CSQC hook is **removed**.
  - The kick block runs only if `!vr_enabled || vr_viewkick`. The colour shift is still applied.
- **Purpose:** avoid forced head rotation in VR.
- **Ironwail:** view.c 283. IW has its own `V_ResetEffects` and keeps the CSQC hook.
- **Isolation idea:** wrap the kick block in `if (!VR_Active() || vr_viewkick.value)`. Keep CSQC.
- **Tag:** VR-CORE / REMOVED

### View: gun kick (punchangle) gated by `vr_viewkick`
- **Where (QVR):** view.cpp `V_CalcRefdef` (~1115-1150)
- **Change:** `v_gunkick` 1 and 2 are applied only when `!(vr_enabled && !vr_viewkick)`. The lerp is the old QSS per-axis delta lerp (`punch[]` static).
- **Ironwail:** IW's lerp uses `cl.punchtime` and `punchblend`. Keep IW's lerp and add the same gate.
- **Tag:** VR-CORE

### View: `V_BonusFlash_f` arguments removed
- **Where (QVR):** view.cpp 434
- **Upstream (BASE):** `bf r g b a` set a custom bonus-flash colour when `Cmd_Argc()>=5`.
- **Change:** that branch is **removed**; the flash is always the fixed colour.
- **Ironwail:** IW has its own version. Keep IW's.
- **Tag:** REMOVED

### View: `V_PolyBlend` saves and restores GL state
- **Where (QVR):** view.cpp 633-688
- **Change:**
  - Adds `glPushAttrib(GL_TRANSFORM_BIT)` and a push/pop of the projection and modelview matrices, so the blend can run inside a per-eye VR projection.
  - `premul_hud` handling **removed**. Always ends with `glDisable(GL_BLEND)` and `glColor3f(1,1,1)`.
- **Ironwail:** IW draws a full-screen triangle with the `viewblend` shader, or folds it into the scene-effects FBO blit (`GL_NeedsSceneEffects`). In VR, apply it per eye after the eye render.
- **Isolation idea:** handled by the render/VR agent (per-eye post pass). No change to view.c beyond calling it per eye.
- **Tag:** RENDER

### View: gun angle follows the hand in controller aim mode (`CalcGunAngle`)
- **Where (QVR):** view.cpp `CalcGunAngle(wpnCvarEntry, viewent, handrot, visual_handrot, horizFlip)` (706-813)
- **Upstream (BASE):** `CalcGunAngle(void)` lagged the gun behind `r_refdef.viewangles` and wrote `cl.viewent`.
- **Change:**
  - If `vr_enabled && vr_aimmode == e_CONTROLLER`: take the offsets `VR_GetWpnAngleOffsets(wpnCvarEntry)`, negating yaw and roll if `horizFlip` (off hand). Then set `viewent->angles = {-visual_handrot[PITCH]+oPitch, visual_handrot[YAW]+oYaw, visual_handrot[ROLL]+oRoll}` and return.
  - Otherwise the old lag code runs, but its source is `r_refdef.aimangles` (a new refdef field) instead of `viewangles`.
  - It now works on any entity passed in (main hand or off hand).
- **Purpose:** hand-tracked weapons.
- **Ironwail:** view.c 607 (void, `cl.viewent` only).
- **Isolation idea:** keep IW's `CalcGunAngle`. In `V_CalcRefdef`, `if (VR_Active()) VR_SetupWeaponViewEnt(&cl.viewent, cVR_MainHand); else CalcGunAngle();`. All the angle math lives in the VR module.
- **Tag:** VR-CORE

### View: `V_CalcRefdef` in VR (eye position, body yaw, weapon position, hand-model hiding)
- **Where (QVR):** view.cpp `V_CalcRefdef(cvarEntry, handpos, gunOffset)` (986-1173)
- **Upstream (BASE):** `V_CalcRefdef(void)`.
- **Change:**
  - `ent->angles[YAW] = VR_GetBodyYawAngle()`, so the player model faces the body yaw rather than the view.
  - Eye: if `VR_EnabledAndNotFake()`, `vieworg = ent->origin + vr_viewOffset`, with no `STAT_VIEWHEIGHT` and no bob. Otherwise as in BASE.
  - `V_BoundOffsets()` is skipped when `vr_enabled`.
  - Weapon (controller aim): `view->origin = handpos + cl.vmeshoffset + gunOffset`, with no bob. `gunOffset = VR_GetWpnGunOffsets(cvarEntry)`.
  - If the weapon model name is one of `progs/hand.mdl`, `hand_base.mdl`, or `finger_{thumb,index,middle,ring,pinky}.mdl`, then `view->hidden = true`. This relies on a new `entity_t::hidden` field. The hands are drawn separately, see below.
  - Stair smoothing moved to `StairSmoothView(playerOldZ, ent, view)` (945). It now adjusts **only** `view->origin[2]`; the `vieworg` adjustment is commented out. It is called for many entities with the same `oldz`, and each call advances `oldz` again, which is a quirk.
  - `Chase_UpdateForDrawing(r_refdef, view)` takes a refdef and entity. It is called once per entity, and each call rewrites `r_refdef`.
- **Purpose:** roomscale eye; weapon in hand.
- **Ironwail:** view.c 763. IW uses `cl.viewheight` (not `cl.stats[STAT_VIEWHEIGHT]`), `cl_entities[]`, `LERP_FINISH` propagation, `view->scale = ENTSCALE_DEFAULT`, and the `punchtime` lerp.
- **Isolation idea:**
  - Keep IW's `V_CalcRefdef`. After the vieworg computation add `if (VR_Active()) VR_OverrideEye(&r_refdef, ent);`.
  - Skip `V_BoundOffsets` under VR.
  - After the gun setup add `if (VR_Active()) VR_PlaceWeapon(view, cVR_MainHand);`.
  - Put the hand-model name check in `VR_PlaceWeapon`.
- **Tag:** VR-CORE

### View: intermission and death in VR
- **Where (QVR):** view.cpp `V_CalcViewRoll` (894), `V_CalcIntermissionRefdef` (909-938)
- **Change:**
  - The dead-view roll of 80 is skipped when `VR_EnabledAndNotFake()`.
  - Intermission in VR:
    1. `viewangles[PITCH]=0`
    2. `aimangles = viewangles`
    3. `viewangles = VR_AddOrientationToViewAngles(viewangles)`
    4. `VR_SetAngles(viewangles)`
- **Ironwail:** view.c 710/737.
- **Isolation idea:** two `if (VR_Active())` hooks.
- **Tag:** VR-CORE

### View: extra VR client entities built in `V_RenderView`
- **Where (QVR):** view.cpp 1175-1876:
  - `V_SetupOffHandWpnViewEnt` (1175)
  - `V_SetupVRTorsoViewEnt` (1231)
  - `V_SetupHolsterSlotViewEnt` (1260)
  - `V_SetupHolsterViewEnt` (1297)
  - `V_SetupHandViewEnt` (1421)
  - `V_SetupFixedHelpingHandViewEnt` (1513)
  - `V_SetupWpnButtonViewEnt` (1566)
  - `V_SetupWpnTextViewEnt` (1614)
  - the `V_RenderView_*` wrappers (1645-1876)
- **Upstream (BASE):** only `cl.viewent`.
- **Change:** each frame (when not in intermission and not paused) these run in order:
  - **WeaponModels**: main hand via `V_CalcRefdef`; off hand into `cl.offhand_viewent`. The off hand uses model `STAT_WEAPONMODEL2`, frame `STAT_WEAPONFRAME2`, `horizFlip=true`, and gunOffset.y negated.
  - **HolsteredWeaponModels**: `cl.left/right_hip_holster` and `cl.left/right_upper_holster`, models from `STAT_HOLSTERWEAPONMODEL2..5`, positions from `VR_GetLeftHipPos()` etc. The angles are fixed offsets from the body yaw (hip: pitch -90, roll ∓body yaw ±10; upper: pitch -20, yaw body+180). When hovered (`cl.hotspot[hand]==QVR_HS_*`), `lightmod=Multiply` with `{6,6,6}`. Hand models are replaced with null.
  - **HolsterModels** (if `vr_leg_holster_model_enabled`): `progs/legholster.mdl` in 4 `*_holster_slot` entities.
  - **HandModels**: 6 entities per hand (base + 5 fingers) in `cl.right_hand_entities` and `cl.left_hand_entities`.
    - Position: anchored to a weapon-model vertex through `VR_GetScaledAndAngledAliasVertexPosition(anchor, WpnCVar::HandAnchorVertex, …)`. With the fist it uses `cl.handpos`.
    - Finger offsets come from the `vr_finger*` cvars.
    - Frame = `vr_fingertracking_frame[hand][finger]`.
    - For the two-handed "Fixed" display mode it blends with `vr_2h_aim_transition`.
    - Sets `zeroBlend` on the other weapon from `WpnCVar::ZeroBlend` or `TwoHZeroBlend`.
    - `ApplyMod_Weapon(vr_hardcoded_wpn_cvar_fist, hdr)` rescales the hand mdl.
    - The ghost-hand code is behind `if(false)`.
  - **VRTorsoModel** (if `vr_vrtorso_enabled==1`): `progs/vrtorso.mdl` in `cl.vrtorso`.
    - Pitch = `vr_vrtorso_pitch - crouchRatio*35`.
    - Yaw = body yaw + `vr_vrtorso_yaw`.
    - Origin is offset by the x/y cvars, by `-crouch*14` forward, and z by `headZ*vr_vrtorso_head_z_mult + z_offset`.
  - **WeaponButtonModels**: `progs/wpnbutton.mdl` at `WpnButtonAnchorVertex`, in `cl.mainhand_wpn_button` and `cl.offhand_wpn_button`. Hidden if `WpnButtonMode::None`.
  - **WeaponText**: a new `textentity_t` (`cl.mainhand_wpn_text` and `cl.offhand_wpn_text`) with origin, angles, `horizFlip`, `scale=WpnTextScale` and `hidden`.
  - Then `R_RenderView()`.
  - BASE called `R_RenderView()` unconditionally. QVR calls it **only** inside the intermission and not-paused branches, so a paused game renders no 3D (behaviour change).
- **Purpose:** VR body and hands, holsters, the weapon button and ammo text.
- **Ironwail:** view.c 920. IW entities differ (no `hidden`, `horizFlip`, `lightmod`, `zeroBlend`, or `textentity_t`). Those fields belong to the render agent.
- **Isolation idea:** new `vr_view.c` with `VR_SetupClientEntities()`, called from IW `V_RenderView` just before `R_RenderView` (`if (VR_Active())`). IW adds visedicts itself (the `cl_visedicts` API), so these entities should be pushed through a `VR_AddEntities()` hook in `CL_RelinkEntities` or `R_RenderView`. Keep IW's unconditional `R_RenderView`.
- **Tag:** VR-CORE / RENDER

### Chase camera takes an explicit refdef and entity; new `TraceLineToEntity`
- **Where (QVR):** chase.cpp `TraceLine` (56, now returns `trace_t`), `TraceLineToEntity(start,end,ent)` (66, `SV_MoveTrace(..., MOVE_NORMAL, ent)`), `Chase_UpdateForDrawing(refdef_t&, entity_t*)` (97)
- **Change:** chase uses `viewent->origin` instead of `cl.viewent.origin` and writes the given refdef. `TraceLineToEntity` is used by `vr_showfn.cpp:318`. `TraceLine` is used by `cl_main.cpp:1503` and `vr_showfn.cpp:287`.
- **Ironwail:** chase.c 51/84 (void versions).
- **Isolation idea:** add a `trace_t VR_TraceLine(start,end,passent)` helper in the VR module. Leave chase.c alone.
- **Tag:** MISC

### 2D pass is world-space in VR (`SCR_UpdateScreen` split)
- **Where (QVR):** gl_screen.cpp `SCR_UpdateScreenContent` (1348-1414), `SCR_UpdateScreen` (1429-1502); vr.cpp `VR_Draw2D` (3598-3752), `VR_DrawSbar` (3754-3849)
- **Upstream (BASE):** `V_RenderView`, `GL_Set2D`, draw the 2D items, `GLSLGamma_GammaCorrect`, `GL_EndRendering`.
- **Change:**
  - `SCR_UpdateScreen` calls `VR_UpdateFlick()`. If `vr_enabled && !con_forcedup`, it switches the QCVM to `sv.qcvm` and calls `VR_UpdateScreenContent()` (per-eye loop, VR-core). Otherwise it sets `cl.viewangles = r_refdef.viewangles = r_refdef.aimangles = cl.aimangles` and calls `SCR_UpdateScreenContent()`. With `vr_fakevr==1` it sets view=aim=`cl.viewangles` and renders again. `GLSLGamma_GammaCorrect` is **removed** (commented out).
  - `SCR_UpdateScreenContent`: `V_RenderView()`, then `VR_Draw2D()` when in VR, otherwise the normal 2D list plus `M_DrawKeyboard()` after `M_Draw()`, then `V_UpdateBlend()`.
  - `VR_Draw2D`:
    - Temporarily sets `glwidth/glheight/vid.conwidth/conheight` to 320×200.
    - Anchor by `vr_menumode`:
      - `LastHeadAngles`: head angles captured when Escape was pressed.
      - `FollowHead`.
      - `FollowOffHand` or `FollowMainHand`: needs an active controller.
    - Target = anchor + `vr_menu_distance`·fwd. Pitch is zeroed for the `HEAD_MYAW*` aim modes. Position is lerped 0.9 toward the target.
    - Transform: rotate yaw-90 and pitch+90, centre with `-160*s` and `-100*s`, scale by `vr_menu_scale`.
    - Draws the same list as flat mode (no crosshair) plus `M_DrawKeyboard`, then `VR_DrawSbar` if the sbar is due.
  - `VR_DrawSbar`:
    - Controller aim: attached to the main or off hand per `vr_sbar_mode`. For the off hand, a quatLookAt is rotated by `vr_sbar_offset_{pitch,yaw,roll}` and translated by `vr_sbar_offset_{x,y,z}`.
    - Other aim modes: `viewent.origin + 1·fwd`, tilted 135°.
    - Scale `vr_hud_scale`. Then `Sbar_Draw()`.
- **Purpose:** 2D must be a quad in 3D in a headset.
- **Ironwail:** gl_screen.c 2115. Draws are batched (`Draw_Flush`) with `glcanvas.transform` (2D NDC transforms). There is no matrix stack.
- **Isolation idea:**
  - In VR, render the entire IW 2D block into a GUI FBO sized for example 640×480, with `vid.guiwidth/guiheight` set to the FBO size. Render the sbar into a second small FBO (320×48 via `CANVAS_SBAR`).
  - In each eye pass, draw the two textures as world-space quads with the matrices computed by `VR_Draw2D`/`VR_DrawSbar` math, in a new `vr_hud.c`.
  - Hook: in `SCR_UpdateScreen` add `if (VR_Active()) { VR_RenderFrame(); GL_EndRendering(); return; }` after `SCR_SetUpToDrawConsole`. The VR module calls a refactored `SCR_Draw2DContents()`, which is IW's existing if/else block extracted into a function.
- **Tag:** VR-CORE / UI / RENDER

### Canvas changes
- **Where (QVR):** gl_draw.cpp `GL_SetCanvas` (919-1000); screen.hpp adds `CANVAS_NOTIFY`
- **Change:**
  - `GL_SetCanvas` returns immediately (no projection change) when `VR_EnabledAndNotFake() && !con_forcedup`, so every 2D draw lands in `VR_Draw2D`'s matrix at raw 320×200 coordinates.
  - `CANVAS_MENU` and `CANVAS_CONSOLE` are now an 800×600 ortho (BASE: menu 640×200 in a 320×200 fit; console `conwidth`-based). This makes room for the wide QVR menus: labels right-aligned to x≈216, values at x=240, tooltips at x=340.
  - The new `CANVAS_NOTIFY` is the old `CANVAS_CONSOLE` (vid.conwidth-based), used by `Con_DrawNotify`.
  - Non-DM `CANVAS_SBAR` is ortho `-200..420 × 48` with viewport `600*s` wide, to fit the off-hand ammo at x=-100.
- **Ironwail:** gl_draw.c `Draw_GetCanvasTransform` (1198). IW's menu is 320×200 but uses the dynamic `m_left/m_width` (up to 960 px wide, `M_UpdateBounds` 7206). The console uses `conwidth`.
- **Isolation idea:** do not change IW canvases. The IW menus already get the extra width through `m_width`. VR pages should lay out within `m_width`. For the sbar second-ammo slot, draw it inside the 320 width (for example as a small number) or use `CANVAS_SBAR2`.
- **Tag:** UI

### Draw_Character / Draw_String get a `scale` parameter; Scrap_Upload on draw
- **Where (QVR):** gl_draw.cpp `Draw_CharacterQuad` (594), `Draw_Character`, `Draw_String` (641); draw.hpp
- **Change:**
  - Optional `float scale=1`: quads are `8*scale` in size and the advance is `8*scale`. Used by `Sbar_DrawString(..., 1.4f)`.
  - `Draw_Pic`, `Draw_SubPic` and `Draw_PicPolygon` call `Scrap_Upload()` if `scrap_dirty`.
- **Ironwail:** IW gl_draw.c has no scale parameter. IW handles the scrap in its own atlas path.
- **Isolation idea:** add `Draw_StringScaled` or `Draw_CharacterScaled` as new functions rather than changing the signatures.
- **Tag:** UI

### Sbar: second (off-hand) ammo display and reload clip counter; ammo stat semantics changed
- **Where (QVR):** sbar.cpp `Sbar_Draw` (1159-1428, ammo part 1307-1420), `Sbar_DrawNum` (453, now returns the next x), `Sbar_DrawString(x,y,str,scale)` (368)
- **Upstream (BASE):** ammo icon from the `IT_SHELLS..` item bits at x=224; number `STAT_AMMO` at x=248.
- **Change:**
  - Non-rogue:
    - The ammo icon comes from `cl.stats[STAT_AMMO]` read as an **ammo-type id**: `AID_NONE=0`, `AID_SHELLS..AID_CELLS=1..4`, drawn as `sb_ammo[aid-1]`. The main hand is at x=224; the off hand uses `STAT_AMMO2` at x=-124.
    - The count comes from `STAT_AMMOCOUNTER` (27) and `STAT_AMMOCOUNTER2` at x=248 and x=-100. Nothing is drawn if the aid is `AID_NONE`.
  - If `quake::vr::get_weapon_reloading_enabled()` (`vr_reload_mode!=0 && vr_holster_mode==0`) and `STAT_WEAPONCLIPSIZE[2]!=0`:
    - Draw the big number `STAT_WEAPONCLIP[2]`.
    - Then `"/%d"` of the clip size at (nextX+4, 2).
    - Then the total ammo counter at (nextX+4, 10), scale 1.4.
  - Rogue keeps the item-bit icons, but the numbers follow the new stats.
  - `Sbar_DrawNum` now truncates to the last `digits` digits (`ptr += l-digits`) and returns x.
  - Stat numbers are in quakedef_macros.hpp: `STAT_AMMO2 26`, `STAT_AMMOCOUNTER 27`, `STAT_WEAPONCLIP 59`, ….
- **Purpose:** dual wielding and reloading.
- **Ironwail:** sbar.c `Sbar_Draw` 1608. Ammo appears at 1763 (classic) and 1802 (modern/`Sbar_DrawInventory2`). `Sbar_AmmoPic` (1479) uses item bits. There is also a QW HUD. All three read `STAT_AMMO` as a count.
- **Isolation idea:** add a helper `Sbar_VR_Ammo(slot, &pic, &count, &clip, &clipsize)`. In VR mode (or when the QVR stat layout is detected), each HUD style calls it in place of `cl.stats[STAT_AMMO]` and `Sbar_AmmoPic`. The stat definitions and parse come from the protocol agent.
- **Tag:** GAMEPLAY / UI

### Sbar: CSQC HUD, hudtype autodetect, ping and fitzmode removed; misc
- **Where (QVR):** sbar.cpp `Sbar_Draw`, `Sbar_IntermissionOverlay` (1691), `Sbar_LoadPics` (158), `Sbar_DeathmatchOverlay` (1479), `Sbar_SoloScoreboard` (586), `Sbar_Voice` (1117)
- **Change (all regressions from QSS):**
  - `CSQC_DrawHud`/`CSQC_DrawScores` calls **removed** from `Sbar_Draw` and the intermission.
  - The spike `hudtype` autodetect (`Sbar_CheckPicFromWad`) is `#if 0`. Back to the global `hipnotic`/`rogue`.
  - Ping column and `ping` request **removed** from the DM overlay.
  - The `fitzmode` branch of the solo scoreboard is removed (always the QS layout).
  - `Sbar_Voice` uses a local dummy cvar with value 1, so the voice meter is always on.
- **Ironwail:** IW keeps CSQC, autodetect and ping. **Keep IW's behaviour.**
- **Isolation idea:** none needed.
- **Tag:** REMOVED

### Menu framework rewrite (`quake::menu`, menu_util)
- **Where (QVR):** menu_util.hpp/.cpp (new, 366+577 lines); mstate.hpp (new, `m_state_e` moved out of menu.hpp)
- **Change:**
  - Generic menu object: title, escape callback, entries vector, cursor, optional per-menu `on_key(key, entry)` hook, and an optional two-column mode (25 per column, used by Change Map).
  - Entry kinds:
    - `cvar<float|int|bool>` with `menu_bounds{inc,min,max}` and an optional printer.
    - Labelled enum on a cvar or a plain int (`add_cvar_getter_enum_entry` with label strings).
    - Plain bool value.
    - Action (shows `(X)`).
    - Action slider (−1/+1 with a range function drawn by `M_DrawSlider`).
    - Separator (skipped by the cursor).
    - Pointer to another menu's entry (used by Quick Settings).
  - Per-entry `tooltip` (drawn wrapped at 28 columns at x=340, y=50) and a `hover(bool)` callback. The callback fires on cursor enter and leave and on menu `enter()`/`leave()`. It drives the `quake::vr::showfn::vr_impl_draw_*` debug gizmos.
  - Keys: Esc runs `VID_SyncCvars()`, the escape callback and `leave()`. Up/Down move the cursor. Left/Right adjust the value, or jump a column in two-column mode. Enter adjusts/activates.
  - Value adjustment uses `quake::util::makeMenuCVarAdjuster` (util.hpp 175): increment × `VR_GetMenuMult()`, where a mult ≥3 counts as 6. The mult comes from the VR "MultiplierHalf/PlusOne/PlusOne2" actions: 0.5, 1, 2 or 3.
  - Sounds: `setMenuState` plays `items/r_item1.wav` through `S_StartSound` at `cl.viewentity`; actions play `items/r_item2.wav`.
  - Layout: title centred on 320; labels right-aligned to a width of 26 chars; values at x=240.
  - No mouse support.
- **Purpose:** quickly author hundreds of VR options.
- **Ironwail:** menu.c 7728 lines of C with its own infrastructure: `menulist_t` + `M_List_*` (scroll, search, mouse, 551-1047), the `OPTIONS_LIST` X-macro for Options/Display/Graphics/Interface/Game/Controller submenus (3084), `M_GetBaseState` (1129), live preview, sliders with mouse click (`M_SliderClick`), and `ui_mouse`.
- **Isolation idea:** see "Porting the VR menus to Ironwail" below.
- **Tag:** UI

### Menu: vanilla menus converted, reorganised or simplified
- **Where (QVR):** menu.cpp:
  - `makeMainMenu` (370)
  - `makeSinglePlayerMenu` (445)
  - Load/Save (536-766)
  - `makeBotControlMenu` (771)
  - `makeMultiPlayerMenu` (830)
  - `makeOptionsMenu` (1191)
  - Keys (3789-4007)
  - Quit (4087-4200)
  - LanConfig/GameOptions (4202-4905)
- **Change:**
  - **Main menu** is a text list (no `gfx/mainmenu.lmp`): Single Player, Multi Player & Bots, Options, Quake VR - Settings, Quake VR - Dev Tools, Quake VR - Change Map, Help/Ordering, Quit. Quick Settings is commented out. Esc returns to the game and restores `cls.demonum`.
  - **Single Player**:
    - "VR Hub" runs `map vrstart`, "Tutorial" runs `vrtutorial`, "Sandbox" runs `vrfiringrange`.
    - New Game runs `map start` directly. There is no BASE "are you sure" or `SCR_ModalMessage`. `M_Menu_NewGame_f(map)` issues `disconnect; maxplayers 1; deathmatch 0; coop 0; map X`.
    - Load, Save.
    - "Start map from:" (cvar `vr_activestartpaknameidx`, printer = `VR_GetLoadedPakNamesWithStartMaps()`, tooltip).
  - **Load menu** lists `MAX_SAVEGAMES` normal saves plus `MAX_AUTOSAVES` autosaves with timestamps (`quake::saveutil`). An autosave loads with `load_autosave autoN`. **Save** writes `save sN`.
  - **Bot Control** (new): "Add Bot (Team 0)" runs `impulse 100`, "Add Bot (Team 1)" `impulse 101`, "Kick Bot" `impulse 102`, plus Skill (`skill`).
  - **Multi Player**: Join a Game, New Game (both go to LanConfig), Setup, Bot Control. The Net/IPX selection is removed and LanConfig goes back to MultiPlayer. "Search for public games" is **removed**.
  - **Game Options**:
    - The `sv_public` item is **removed**.
    - One merged level table covers id1 + SoA + DoE (70 maps), with episodes "Vanilla E1..DM", "SoA E1..DM" and "DoE E1..DM". There is no hipnotic/rogue switch.
  - **Options** (text list):
    - Controls
    - Goto Console
    - Reset Config (`SCR_ModalMessage`, then `resetcfg; exec default.cfg`)
    - Scale (sets `scr_conscale`, `scr_menuscale` and `scr_sbarscale` together)
    - Screen Size (`viewsize`)
    - Brightness (`gamma`)
    - Contrast (`contrast`)
    - Mouse Speed (`sensitivity`)
    - Statusbar Alpha (`scr_sbaralpha`)
    - Sound Volume (`volume`)
    - Music Volume (`bgmvolume`)
    - External Music (`bgm_extmusic`)
    - Always Run (`cl_alwaysrun`)
    - Toggle Invert Mouse (`m_pitch`)
    - Toggle Mouse Look (`+mlook`/`-mlook`)
    - Lookspring
    - Lookstrafe
    - The BASE video submenu entry is gone.
  - **Keys**:
    - Static `bindnames` table. It adds `impulse 13` ("cycle off-hand weapons") and keeps `+voip`.
    - The BASE `bindlist.lst` support, `keys_first` scrolling and "-" section headers are **removed**.
  - **Quit**:
    - Any key other than Esc quits (VR controllers cannot type 'y').
    - The message box moved to x=260.
    - Credits read "by Vittorio Romeo, Ozkan Sezer, …".
    - The version string is "QuakeSpasm " `QUAKESPASM_VER_STRING`.
    - The `cl_confirmquit`/fitzmode instant quit is disabled (`if(false)`).
  - Load/Save/Quit/etc. accept `K_BBUTTON` and `K_ABUTTON` (already in BASE).
- **Purpose:** VR start maps; controller-friendly.
- **Ironwail:**
  - IW has a much richer vanilla menu set: a Maps menu (`m_maps`, which supersedes Change Map), Skill select, Mods and ModInfo, a Controller/gamepad page, a 'bindlist.lst'-aware key setup with sections, and quit confirm with `K_ABUTTON`.
  - **Keep IW's vanilla menus.** Port only:
    - (a) SP entries for vrstart / vrtutorial / vrfiringrange;
    - (b) the "Start map from" pak selector;
    - (c) autosave slots in Load;
    - (d) Bot Control;
    - (e) extra key-setup entries, which can come entirely from a `bindlist.lst` in the QVR game dir.
- **Isolation idea:**
  - (a) Add 3 items to IW's SP menu, or put them on a "Quake VR" page.
  - (c) Add rows to `M_Load_*` behind `if (vr_autosave slots)`, or put them on a separate "Load Autosave" VR page.
  - (e) Ship `bindlist.lst` with `+grableft`, `+grabright`, `+reloadleft`, `+reloadright`, `+flickreloadleft`, `+flickreloadright`, `+offhandattack`, `impulse 13/15/16/42/43`, and so on.
- **Tag:** UI / REMOVED

### Menu: "Quake VR - Settings" hub and its 15 pages
- **Where (QVR):** menu.cpp `forQVRSMenus` (2440), `makeQuakeVRSettingsMenu` (2459); the hub auto-generates one action per page title. States `m_qvrs_*` are in mstate.hpp.
- **Change (page, function and line, cvars exposed):**

| Page | QVR fn (line) | cvars |
|---|---|---|
| Menu Settings | makeQVRSMenuMenu (1400) | vr_menumode (enum: Fixed Head / Follow Head / Follow Off-Hand / Follow Main Hand), vr_menu_scale, vr_menu_distance, vr_menu_mouse_pointer_hand (Off-Hand / Main Hand) |
| Crosshair Settings | makeQVRSCrosshairMenu (1440) | vr_crosshair (Off/Point/Line/Smooth line), vr_crosshair_depth, crosshair, vr_crosshair_size, vr_crosshair_alpha, vr_crosshairy |
| Particle Settings | makeQVRSParticleMenu (1485) | r_particles, r_particle_mult |
| Locomotion Settings | makeQVRSLocomotionMenu (1510) | vr_movement_mode, vr_deadzone, vr_enable_joystick_turn, vr_snap_turn, vr_turn_speed, vr_teleport_enabled, vr_teleport_range, vr_roomscale_jump, vr_roomscale_jump_threshold, vr_roomscale_move_mult, cl_forwardspeed, cl_movespeedkey |
| Hand/Gun Calibration | makeQVRSHandGunCalibrationMenu (1618) | vr_gunangle, vr_gunmodelpitch, vr_gunmodelscale, vr_gunmodely, vr_gunyaw, vr_gun_z_offset, vr_offhandpitch, vr_offhandyaw, vr_finger_grip_bias, vr_finger_auto_close_thumb |
| Player Calibration | makeQVRSPlayerCalibrationMenu (1682) | vr_world_scale, vr_floor_offset (plus a calibrate-height action) |
| Melee Settings | makeQVRSMeleeMenu (1716) | vr_melee_threshold, vr_melee_dmg_multiplier, vr_melee_range_multiplier, vr_melee_bloodlust, vr_melee_bloodlust_mult |
| Aiming Settings | makeQVRSAimingMenu (1760) | vr_2h_mode, vr_2h_angle_threshold, vr_2h_disable_angle_threshold, vr_2h_virtual_stock_factor, vr_wpn_pos_weight, vr_wpn_pos_weight_offset, vr_wpn_pos_weight_mult, vr_wpn_pos_weight_2h_help_offset, vr_wpn_pos_weight_2h_help_mult, vr_wpn_dir_weight, vr_wpn_dir_weight_offset, vr_wpn_dir_weight_mult, vr_wpn_dir_weight_2h_help_offset, vr_wpn_dir_weight_2h_help_mult |
| Immersion Settings | makeQVRSImmersionMenu (1851) | vr_show_weapon_text, vr_positional_damage, vr_body_interactions, vr_disablehaptics, vr_holster_mode, vr_reload_mode, vr_holster_haptics, vr_weapon_cycle_mode, vr_weapon_throw_mode, vr_weapon_throw_damage_mult, vr_weapon_throw_velocity_mult, vr_weapondrop_particles, vr_enemy_drops, vr_enemy_drops_chance_mult, vr_ammobox_drops, vr_ammobox_drops_chance_mult, vr_forcegrab_mode, vr_forcegrab_range, vr_forcegrab_radius, vr_forcegrab_powermult, vr_forcegrab_eligible_particles, vr_forcegrab_eligible_haptics |
| Graphical Settings | makeQVRSGraphicalMenu (2038) | vr_msaa, r_shadows, vr_player_shadows |
| Hud Configuration | makeQVRSHudConfigurationMenu (2074) | vr_sbar_mode (Main Hand / Off Hand), vr_hud_scale, vr_sbar_offset_x/y/z, vr_sbar_offset_roll/pitch/yaw |
| Hotspot Settings | makeQVRSHotspotMenu (2133) | vr_show_virtual_stock (Off/Main/Off/Both), vr_shoulder_offset_x/y/z, vr_virtual_stock_thresh, vr_show_shoulder_holsters, vr_shoulder_holster_offset_x/y/z, vr_shoulder_holster_thresh, vr_show_hip_holsters, vr_hip_offset_x/y/z, vr_hip_holster_thresh, vr_show_upper_holsters, vr_upper_holster_offset_x/y/z, vr_upper_holster_thresh |
| Torso Settings | makeQVRSTorsoMenu (2212) | vr_vrtorso_enabled, vr_vrtorso_x/y/z_offset, vr_vrtorso_head_z_mult, vr_vrtorso_x/y/z_scale, vr_vrtorso_pitch/yaw/roll, vr_leg_holster_model_enabled, vr_leg_holster_model_scale, vr_leg_holster_model_x/y/z_offset. `on_key` calls `VR_ModVRTorsoModel()` + `VR_ModVRLegHolsterModel()` after every key. |
| Transparency Options | makeQVRSTransparencyOptionsMenu (2286) | r_novis, r_wateralpha, r_lavaalpha, r_telealpha, r_slimealpha. `on_key`: pressing 'p' adds the hovered entry to Quick Settings (hack). |
| Voip Options | makeQVRSVoipMenu (2337) | sv_voip, sv_voip_echo, cl_voip_send, cl_voip_test, cl_voip_vad_threshhold, cl_voip_vad_delay, cl_voip_capturingvol, cl_voip_showmeter, cl_voip_play, cl_voip_micamp, cl_voip_ducking, cl_voip_codec, cl_voip_noisefilter, cl_voip_autogain, cl_voip_opus_bitrate |

- **"Quake VR - Quick Settings"** (`makeQuakeVRQuickSettingsMenu` 2405, `m_quakevrquicksettings`): empty at start; filled by the 'p' hack. Unused (commented out of Main).
- **Purpose:** user configuration of VR.
- **Ironwail:** nothing equivalent.
- **Isolation idea:** see below.
- **Tag:** UI

### Menu: "Quake VR - Dev Tools" hub and its 7 pages
- **Where (QVR):** menu.cpp `forQVRDTMenus` (3506), `makeQuakeVRDevToolsMenu` (3517)
- **Change:**
  - **Weapon Configuration (1)–(5)** (2501, 2683, 2904, 3007, 3161):
    - These edit the per-weapon "WpnCVar" table through `VR_GetWpnCVar(idx, WpnCVar::X)`, where `idx = VR_GetMainHandWpnCvarEntry()` or `VR_GetOffHandWpnCvarEntry()`, chosen by the first entry, the bool "Off-Hand".
    - Each page's `on_key` calls `VR_ModAllWeapons()`.
    - The fields are:
      - (1) OffsetX/Y/Z, Scale, Roll/Pitch/Yaw, MuzzleOffsetX/Y/Z, MuzzleAnchorVertex, TwoHOffsetX/Y/Z, TwoHPitch/Yaw/Roll
      - (2) HideHand, HandAnchorVertex, TwoHDisplayMode, TwoHHandAnchorVertex, HandOffsetX/Y/Z, TwoHMode, TwoHFixedOffsetX/Y/Z, TwoHFixedHandPitch/Yaw/Roll, TwoHFixedMainHandOffsetX/Y/Z, GunOffsetX/Y/Z
      - (3) Weight, WeightPosMult, WeightDirMult, WeightHandVelMult, WeightHandThrowVelMult, Weight2HPosMult, Weight2HDirMult
      - (4) WpnButtonMode, WpnButtonX/Y/Z, WpnButtonPitch/Yaw/Roll, WpnButtonAnchorVertex, ZeroBlend, TwoHZeroBlend
      - (5) WpnTextMode, WpnTextX/Y/Z, WpnTextPitch/Yaw/Roll, WpnTextAnchorVertex, WpnTextScale
    - Hover callbacks set `quake::vr::showfn::vr_impl_draw_wpnoffset_helper_offset`, `_2h_offset`, `_muzzle`, `vr_impl_draw_hand_anchor_vertex`, `vr_impl_draw_2h_hand_anchor_vertex`, `vr_impl_draw_wpnbutton_anchor_vertex` and `vr_impl_draw_wpntext_anchor_vertex` to 1 (main hand) or 2 (off hand), which draws gizmos.
  - **Finger Configuration** (3304): X/Y/Z of vr_fingers_and_base, vr_fingers_and_base_offhand, vr_fingers, vr_finger_thumb, vr_finger_index, vr_finger_middle, vr_finger_ring, vr_finger_pinky, vr_finger_base (each `_x`,`_y`,`_z`), plus vr_finger_blending and vr_finger_blending_speed.
  - **Debug Utilities** (3356):
    - cvars: vr_forcegrabbable_ammo_boxes, vr_forcegrabbable_health_boxes, vr_forcegrabbable_return_time_deathmatch, vr_forcegrabbable_return_time_singleplayer, vr_throw_up_center_of_mass, vr_throw_avg_frames, vr_throw_angvel_avg_frames, vr_throw_algorithm (Basic/CrossAngVel), vr_autosave_seconds, vr_autosave_on_changelevel, vr_autosave_show_message, skill, r_showbboxes, r_showbboxes_player, vr_vrtorso_debuglines_enabled, vr_fakevr, host_timescale, vr_debug_print_handvel, vr_debug_print_headvel, vr_debug_show_hand_pos_and_rot, vr_aimmode (HEAD_MYAW, HEAD_MYAW_MPITCH, MOUSE_MYAW, MOUSE_MYAW_MPITCH, BLENDED, BLENDED_NOPITCH, CONTROLLER), vr_viewkick, vr_player_stepsize.
    - Actions: "Refresh Throw Avg Frames" calls `VR_ResetThrowAvgFrames()`. The impulses 9, 11, 14, 17, 254, 255 and the god, noclip and fly commands run through `Cmd_ExecuteString(src_command)`.
- **Tag:** UI

### Menu: "Quake VR - Change Map" hub and its 6 pages
- **Where (QVR):** menu.cpp 3556-3787
- **Change:**
  - First entry "Preserve Equipment": a local int, "No (map)" or "Yes (changelevel)".
  - Six two-column pages run `map X` or `changelevel X`:
    - Vanilla (e1m1…end)
    - Scourge of Armagon (hip*)
    - Dissolution of Eternity (r*m*, ctf1)
    - Dimensions of the Past (e5m1…e5dm)
    - Honey (saint, honey, h_hub1, h_hub2, h_end, credits)
    - Custom Maps (the `extralevels` file list)
  - No cvars.
- **Ironwail:** IW's Maps menu (`M_Menu_Maps_f` 1756) already lists every map with titles and search, and has mouse support.
- **Isolation idea:** drop these pages. Add a "preserve equipment (changelevel)" toggle to IW's Maps menu, or leave it out.
- **Tag:** UI

### Menu: virtual keyboard (`menu_keyboard`)
- **Where (QVR):** menu_keyboard.hpp/.cpp (new); menu.cpp `mkb()` (5091, initial pos {200,400}), `M_DrawKeyboard` (5097-5169); called from gl_screen.cpp 1404 and vr.cpp 3735
- **Change:**
  - Drawn while `key_dest` is menu or console.
  - Layout:
    - Rows "1234567890'", "qwertyuiop", "asdfghjkl", "zxcvbnm,.-", with a 28 px pitch and ±8 px hit boxes.
    - Special keys: escape, backspace, enter, console, caps, tab, space and four arrows at fixed offsets.
    - A 256×16 drag bar at `pos-(8,8)`, with its hit box grown by 16 when hovered.
  - Input: the pointer `vr_menu_mouse_x/y` and click `vr_menu_mouse_click`. Rising-edge click on a key produces:
    - Characters: `Char_Event(c or c-32 when caps)`.
    - Keys: `M_Keydown(key, fromVirtualKeyboard=true)` in the menu, or `Key_Console(key, true)` in the console. Escape in the console calls `M_ToggleMenu_f`.
    - "console" sets `m_state=m_none` and calls `Con_ToggleConsole_f`.
  - Draws a red 8 px GL point at the pointer.
  - `M_Keydown` (5283) and `Key_Console` (keys.cpp 555) ignore a physical `K_ENTER` while the pointer hovers the keyboard, unless `vr_fakevr`, so the trigger's Enter does not double-fire.
- **Purpose:** text entry (console, player name, IP) in VR.
- **Ironwail:** nothing equivalent. IW menus already have mouse hover and click. IW has `M_Charinput` and `Key_Console(int)`.
- **Isolation idea:**
  - New `vr_keyboard.c` drawn with IW `Draw_Fill`, `Draw_Character` and `Draw_String` inside the GUI FBO.
  - Instead of adding a parameter to `M_Keydown` and `Key_Console`, add a global `vr_kb_suppress_enter` checked in the VR input code before it sends `K_ENTER`. That needs no edits in keys.c or menu.c.
- **Tag:** UI / VR-CORE

### Menu: `M_Draw` / `M_Keydown` dispatch for nested VR menus
- **Where (QVR):** menu.cpp `M_Draw` (5171-5281), `M_Keydown(int key, bool fromVirtualKeyboard=false)` (5283-5389), `M_Init` (5068)
- **Change:**
  - Before the `m_state` switch, iterate `forQVRSMenus`, `forQVRDTMenus` and `forQVRCMMenus` and dispatch draw or key to the menu whose state matches.
  - New cases for `m_botcontrol` and `m_quakevr*`.
  - `M_Init` adds the command `autosave` (`quake::saveutil::doAutosave`). No `menu_*` commands for the VR menus.
- **Ironwail:** menu.c `M_Draw` 7230, `M_Keydown(int, qboolean repeat)` 7355, `M_Mousemove` 7481, `M_GetBaseState` 1129. Add the `autosave` command in the save-util port instead.
- **Tag:** UI

### VR controller → Quake input mapping
- **Where (QVR):**
  - vr.cpp: `VR_DoInput` (4021-4429), `VR_DoInput_UpdateVRMouse` (3945-4019), `VR_DoInput_UpdateFakeMouse` (3935), weapon-button hits (3100-3129), action handles (1261-1346), `VR_Move` (4431, called from cl_main.cpp 1451 after `IN_Move`)
  - `ReleaseFiles/actions.json` + `bindings_*.json`
  - `ReleaseFiles/Id1/config.cfg`
- **Upstream (BASE):** none (in_sdl.cpp has **no** VR code).
- **Change:**
  - SteamVR action sets `/actions/default` and `/actions/menu`.
    - The set switches to menu when `inMenuOrConsole()` and Escape is not held. It switches back when not in the menu and MenuBack is not held.
  - Each frame the following are read:
    - Analog: Locomotion, Turn.
    - Digital: FireMainHand, FireOffHand, Jump, Prev/NextWeaponMainHand, Prev/NextWeaponOffHand, Escape, Speed, Teleport, LeftGrab, RightGrab, LeftReload, RightReload, BMoveForward/Backward/Left/Right, BTurnLeft/Right.
    - Skeletal: LeftHandAnim, RightHandAnim.
    - Menu: Navigation (analog), Up, Down, Left, Right, Enter, Back, AddToShortcuts (unused), MultiplierHalf, MultiplierPlusOne, MultiplierPlusOne2.
  - Always (when not `vr_fakevr`):
    - `Key_Event('k', LeftGrab)` and `Key_Event('l', RightGrab)`; then `vr_left/right_grabbing = in_grableft/right.state&1`.
    - `Key_Event('n', LeftReload)` and `Key_Event('m', RightReload)`; then `vr_*_reloading = in_reload*.state&1`.
    - `in_speed.state = Speed`.
    - `vr_menu_mult = (Half?0.5:1) + PlusOne + PlusOne2`.
  - **In game**:

    | Action | Quake key |
    |---|---|
    | FireMainHand | K_MOUSE1 (held) |
    | FireOffHand | K_MOUSE2 (held) |
    | Jump rising edge, or roomscale jump (`headVelocity.y > vr_roomscale_jump_threshold` and head above `vr_height_calibration`) | K_SPACE |
    | Escape (with haptic; stores `lastMenuAngles`) | K_ESCAPE |
    | NextWeaponMainHand | '1' |
    | NextWeaponOffHand | '3' |
    | PrevWeaponMainHand | '4' |
    | PrevWeaponOffHand | '5' |
    | Teleport | `vr_teleporting` (not a key) |

  - **Weapon buttons** (other hand touching the `wpnbutton` entity within 2.7 units, at most once per 0.2 s): '7' for the off hand, '8' for the main hand. In the shipped config these are impulse 42 and 43.
  - **In menu or console**:

    | Action | Quake key |
    |---|---|
    | MenuEnter | K_ENTER, and also `vr_menu_mouse_click` |
    | MenuBack | K_ESCAPE |
    | MenuLeft / MenuRight | K_LEFTARROW / K_RIGHTARROW |
    | Navigation Y axis (deadzone 0.025) | K_UPARROW / K_DOWNARROW (edge-triggered) |
    | MenuUp / MenuDown | K_UPARROW / K_DOWNARROW |

    Each press gives a haptic pulse on both hands (0.1 s, 50 Hz, 0.5 amplitude).
  - **Pointer**: a ray from the `vr_menu_mouse_pointer_hand` controller (1 = main hand first) is intersected with the menu plane (`vr_menu_target`, `vr_menu_angles`).
    - `vr_menu_mouse_x = proj_right/scale + 160`
    - `vr_menu_mouse_y = -proj_up/scale + 120` (note: 120, not 100)
  - Fake-VR/novrinit mode: grab = **NOT** `in_grab*` (inverted), reload and flick-reload from the kbuttons, pointer = `SDL_GetMouseState`.
  - Movement axes go back to `VR_Move`, where the locomotion and turn math runs (VR-core agent).
  - Required config bindings (from `ReleaseFiles/Id1/config.cfg`):

    ```
    bind k +grableft; bind l +grabright; bind n +reloadleft; bind m +reloadright
    bind i +flickreloadright; bind o +flickreloadleft
    bind 1 "impulse 10"; bind 3 "impulse 12"; bind 4 "impulse 15"; bind 5 "impulse 16"
    bind 7 "impulse 42"; bind 8 "impulse 43"; bind MOUSE1 +attack; bind MOUSE2 +offhandattack
    bind SPACE +jump; bind ESCAPE togglemenu
    ```

  - New kbuttons and commands in cl_input.cpp (794-805, client agent): `+/-grableft`, `+/-grabright`, `+/-reloadleft`, `+/-reloadright`, `+/-flickreloadleft`, `+/-flickreloadright`, `+/-offhandattack`.
- **Purpose:** reuse Quake's binding and impulse plumbing for VR controllers.
- **Ironwail:** in_sdl.c is heavily extended (gamepad, gyro, flick stick, `Key_EventWithKeycode`, key repeat, `K_DPAD_*` remapped to arrows in `M_Keydown`). `Key_Event(int, qboolean)` exists (keys.c 1176). `M_Keydown` has a `repeat` argument and ignores mouse keys unless `ui_mouse`.
- **Isolation idea:**
  - Keep the "synthesise `Key_Event`" design, but in a new `vr_input.c` called from `IN_Commands`/`IN_Move` or from `CL_SendCmd`.
  - Better: execute the commands directly (`KeyDown(&in_grableft)` through small exported helpers) instead of relying on the user's bindings to 'k'/'l'/'n'/'m'. That removes the fragile config dependency.
  - Menu navigation: send K_UPARROW, K_DOWNARROW, K_LEFTARROW, K_RIGHTARROW, K_ENTER and K_ESCAPE, which IW menus accept. For Quit, IW needs `K_ABUTTON` or 'y'.
  - Pointer: feed IW's existing mouse path with canvas coordinates. Add `M_MousemoveCanvas(float x, float y)` split out of `M_Mousemove` 7481, and send `K_MOUSE1` for the click. IW menus then get hover and click for free (`ui_mouse` must be 1).
- **Tag:** VR-CORE / UI

### in_sdl: SDL1 and macOS paths dropped; no functional VR change
- **Where (QVR):** in_sdl.cpp throughout
- **Change:** removes `USE_SDL2` conditionals, SDL1 keysym mapping, the disabled mouse-accel block and `IN_Activate`/`IN_Deactivate`, which were already `#if 0`. `IN_JoyMove` (759) uses `(in_speed.state&1) ^ (cl_alwaysrun.value == 0.0)`. The same inversion appears in cl_input.cpp 464/557 and vr.cpp 4639. With `cl_alwaysrun 0` (the QVR default) the player **runs** by default, and +speed walks.
- **Ironwail:** IW defaults `cl_alwaysrun "1"` with the normal `!= 0` test. The default behaviour is identical (run). No port needed, just note the flipped meaning of existing QVR configs.
- **Tag:** MISC

### keys.cpp
- **Where (QVR):** keys.cpp `Key_Console(int key, bool fromVirtualKeyboard)` (555), `Key_Event` (1432, `case key_console: Key_Console(key,false)`)
- **Change:** the virtual-keyboard Enter suppression (see above). History indexing uses `& (CMDLINES-1)` instead of `& 31`, which is a BASE bug fix for `CMDLINES=64`. Everything else is churn.
- **Ironwail:** keys.c 307. IW already uses `CMDLINES-1`.
- **Tag:** UI / BUGFIX (already in IW)

### Console
- **Where (QVR):** console.cpp:
  - `NUM_CON_TIMES` 8 (84; BASE 4)
  - `Con_DrawNotify` uses `CANVAS_NOTIFY` (1432)
  - `Con_DPrintf`/`Con_DPrintf2` use `quake::vr::developerMode()`; new `Con_DPrintf3` (≥3) (815-870)
  - version string "QuakeSpasm " `QUAKESPASM_VER_STRING` (BASE `ENGINE_NAME_AND_VER`)
- **Change:** `developerMode()` (developer.cpp) returns 1 in `!NDEBUG` builds, otherwise `developer.value`. `cmd.cpp:1017` "Unknown command" uses it too.
- **Ironwail:** IW console.c `NUM_CON_TIMES 4`.
- **Isolation idea:** optional. 8 notify lines are nicer in VR because the notify area is small. `Con_DPrintf3` is only needed if the VR code uses it.
- **Tag:** MISC

### cmd.cpp
- **Where (QVR):** cmd.cpp `Cmd_Exec_f` (314), `Cmd_ForwardToServer` (1034)
- **Change:**
  - `exec` inserts an extra `"\n"` before the file text (`Cbuf_InsertText("\n")`).
  - **Removed**: the client's special answers to server `cmd protocols` (it listed RMQ, FITZ, BJP3, DP7, NQ) and `cmd pext` (FTE PEXT2).
- **Ironwail:** IW's cmd.c keeps its own version.
- **Isolation idea:** no port. The protocol agent decides on pext.
- **Tag:** REMOVED / PROTOCOL

### cvar.cpp: cvar handles for QC
- **Where (QVR):** cvar.cpp `cvar_handles` vector (reserve 128, 29/291) with `Cvar_MakeHandle` (820), `Cvar_GetValueFromHandle`, `Cvar_SetValueFromHandle` and `Cvar_ClearAllHandles`; cvar.hpp 157-160
- **Change:** the index into a `std::vector<cvar_t*>`, with range-checked get and set. Used by the QC builtins `cvar_hmake(string)`, `cvar_hget(float)`, `cvar_hset(float,float)` and `cvar_hclear()` (pr_cmds.cpp 1146-1172, 1445).
- **Ironwail:** none.
- **Isolation idea:** a static `cvar_t *vr_cvar_handles[256]` in the builtins file. No change to cvar.c.
- **Tag:** VR-CORE (QC)

### cfgfile.cpp
- **Where (QVR):** cfgfile.cpp
- **Change:** churn only.
- **Tag:** —

### host.cpp (parts relevant here)
- **Where (QVR):** host.cpp `Host_FilterTime` (786), `_Host_Frame` (~1103), `Host_Init` (~1230-1310)
- **Change:**
  - The `host_maxfps` throttle is skipped when `vr_enabled` (the HMD compositor paces frames).
  - After `CL_ReadFromServer`, if `!deathmatch`, call `quake::saveutil::doAutomaticAutosave()`.
  - `VR_InitCvars()` runs before `COM_Init`.
  - After `exec quake.rc`, it adds `cl_warncmd 1`, calls `Cbuf_Execute()` immediately, then runs `VR_ModAllModels()` if `vr_enabled==1`.
  - `Host_Shutdown` calls `VID_VR_Shutdown()`.
- **Ironwail:** host.c.
- **Isolation idea:** hooks in the VR module; the host agent covers details.
- **Tag:** VR-CORE

## Porting the VR menus to Ironwail (minimal intrusion)
1. **New file `vr_menu.c`**, with no C++ lambdas. Define a table-driven page type:
   ```c
   typedef struct { const char *label; vrmitem_type_t type; const char *cvar; float inc, min, max;
                    const char *const *enum_labels; void (*action)(void); const char *tooltip;
                    int *showfn_var; } vrmitem_t;
   typedef struct { const char *title; const vrmitem_t *items; int numitems; void (*on_change)(void); } vrmpage_t;
   ```
   - Look cvars up by name (`Cvar_FindVar`) so the table needs no externs.
   - The WpnCVar pages need a getter callback type (`float *(*getter)(void)` or `cvar_t *(*getcvar)(void)`) because they index the current weapon's cvar block.
   - Pages: 15 Settings pages + 7 Dev Tools pages + 2 hubs. Change Map is replaced by IW's Maps menu.
2. **Reuse IW infrastructure**:
   - Each page embeds a `menulist_t` and calls `M_List_Key`, `M_List_Mousemove`, `M_List_Update`, `M_List_DrawScrollbar` and `M_List_GetVisibleRange`. This gives scrolling for long pages such as Immersion (22 items) and Weapon Config 2 (≈30), plus mouse hover, wheel, type-to-search and separators (via an `IsSelectable` callback).
   - Sliders: IW's `M_DrawSlider` and `M_SliderClick`, which need to be made non-static or duplicated.
   - Tooltips: `M_PrintWordWrap` (menu.c 372) at the bottom or right of `m_width`.
   - `VR_GetMenuMult()` is honoured in the adjust function.
3. **Hooks into menu.c (about 15 lines)**:
   - Add one state `m_vr` to `enum m_state_e` (menu.h). Keep the current page index inside vr_menu.c instead of adding 25 states.
   - `M_Draw`, `M_Keydown` and `M_Mousemove` get `case m_vr: VR_Menu_Draw(); / VR_Menu_Key(key); / VR_Menu_Mousemove(x,y);`.
   - Entry point: either add `item (OPT_VR, "Quake VR")` to `OPTIONS_LIST` (menu.c 3084, next to `OPT_MODS`) with its Enter handler calling `VR_Menu_Open()`, or add a main-menu item. The main menu is picture-based with an index enum (1147), so a text row under the pic, as `MAIN_MODS` does with `M_PrintEx` (1195), is the least intrusive.
   - `M_Init`: `Cmd_AddCommand("menu_vr", VR_Menu_Open_f)` so it can also be bound to a controller button.
4. **Things to keep from QVR**: per-page `on_change` callbacks (`VR_ModAllWeapons`, `VR_ModVRTorsoModel`, `VR_ModVRLegHolsterModel`); the hover showfn toggles for the weapon gizmos; Escape goes back to the hub. Wire the SP extras (vrstart/vrtutorial/vrfiringrange, "Start map from", Bot Control) as a "Quake VR" hub page rather than editing IW's SP/MP menus.
5. **Draw target**: in VR the menu is drawn into the GUI FBO (see "2D pass is world-space"). IW's `M_UpdateBounds` then sees a fixed FBO canvas. With `CANVAS_MENU` at 320×200 scale 1 and a 640×480 FBO, `m_width` becomes 640, so QVR's wide label/value layout fits without canvas hacks.

## Dependencies on other subsystems
- **VR core (vr.cpp)**: `VR_Draw2D`, `VR_DrawSbar`, `VR_DoInput`, `VR_Move`, `VR_UpdateFlick`, `VR_UpdateScreenContent`, `VR_ResetOrientation`, `VR_SetAngles`, `VR_AddOrientationToViewAngles`, `VR_GetBodyYawAngle`, `VR_GetCrouchRatio`, `VR_GetHeadOrigin`, `vr_viewOffset`, the hip/upper holster positions, the `VR_GetWpn*` accessors, the `WpnCVar` table, `vr_fingertracking_frame`, `vr_2h_aim_transition`, `VR_GetMenuMult`, `vr_menu_*` globals, and the cvar list in vr_cvars.
- **Client/protocol**:
  - `cl.handpos`, `handrot`, `visual_handrot` and `vmeshoffset`.
  - `cl.offhand_viewent`, the holster entities and slots, the hand entities, `vrtorso`, the weapon buttons and texts.
  - `cl.hotspot[]`.
  - Stats `STAT_WEAPONMODEL2`, `STAT_WEAPONFRAME2`, `STAT_HOLSTERWEAPONMODEL2..5`, `STAT_AMMO2`, `STAT_AMMOCOUNTER[2]`, `STAT_WEAPONCLIP[2]`, `STAT_WEAPONCLIPSIZE[2]`, and the `AID_*` values.
  - `r_refdef.aimangles`, `cl.aimangles`.
  - The kbuttons in cl_input.cpp.
- **Render**:
  - `entity_t` fields `hidden`, `horizFlip`, `lightmod`/`lightmodvalue`, `zeroBlend`.
  - `textentity_t` and its rendering (gl_rmain.cpp 1540 draws the ammo text).
  - `ApplyMod_Weapon`, `VR_GetScaledAndAngledAliasVertexPosition`, per-eye `V_PolyBlend`, the GUI FBO quad rendering, and the removed GLSL gamma.
- **Save util**: autosave slots and the `load_autosave` and `autosave` commands (`saveutil.cpp`).
- **QC builtins**: `cvar_hmake`, `cvar_hget`, `cvar_hset`, `cvar_hclear` (cvar handles).
- **Assets**: `progs/vrtorso.mdl`, `legholster.mdl`, `wpnbutton.mdl`, `hand_base.mdl`, `finger_*.mdl`; the maps vrstart, vrtutorial and vrfiringrange; `ReleaseFiles/actions.json`, `bindings_*.json` and `Id1/config.cfg`.

## Open questions
- Is QVR skipping `R_RenderView` while paused deliberate (in VR, pausing shows only 2D), or a regression? IW always renders. The recommendation is to keep IW's behaviour.
- `StairSmoothView` is called once per VR entity against one shared `oldz`, so the smoothing speed depends on how many entities exist, and the eye itself is no longer smoothed. Should the port smooth once per frame and apply the same delta to every VR entity?
- The VR pointer's y centre is 120 while the canvas is 200 high. Is this an intentional calibration fudge? It needs revisiting with the FBO approach anyway.
- Should the VR controller layer keep synthesising keyboard keys ('k', 'l', 'n', 'm', '1' to '8'), which breaks if the user rebinds them, or call the kbutton and impulse functions directly? The QC mod only sees the commands, so direct calls look safe.
- Is Quick Settings (the 'p' hack) or the `m_vr*`/`m_map`/`m_debug` states in mstate.hpp (unused leftovers) worth porting? The recommendation is no.
- CSQC HUD and damage hooks and the sbar ping were removed in QVR, presumably because they conflicted with the VR HUD path. Keep them in IW but bypass CSQC HUD drawing in VR mode?
