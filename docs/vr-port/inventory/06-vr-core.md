# VR core: Quake VR architecture and interface inventory

All the files covered here are new in QVR and have no BASE counterpart. So instead of a "Changes" list, this file describes the module's architecture, its interface, every hook it needs in the engine, and the engine state it depends on.

Files (QVR `C:\OHWorkspace\quakevr\Quake\`):

| File | Lines | Role |
|---|---|---|
| `vr.hpp` / `vr.cpp` | 647 / 4942 | Everything: OpenVR init, poses, eye FBOs, projection, aim modes, hand tracking and physics, 2H aiming, holsters, teleport, input to keys/usercmd, haptics, 3D menu/HUD placement, weapon-offset cvar table, PAK list helpers. The last ~270 lines are TODO comments only. |
| `vr_cvars.hpp` / `vr_cvars.cpp` | 250 / 341 | Static definition of about 190 `vr_*` cvars through a self-registering macro, plus `quake::vr::register_all_cvars()` and a few vec3 getters. |
| `vr_showfn.hpp` / `vr_showfn.cpp` | 49 / 817 | Debug/visual helper drawing (crosshair, teleport arc, holster lines, anchor vertices) using immediate-mode GL. |
| `vr_macros.hpp` | 3 | `VRUTIL_POWER_OF_TWO(x)` = `1<<x` (used for the `QVR_VRBITS0_*` bits in `quakedef_macros.hpp:299-312`). |
| `worldtext.hpp` | 32 | `WorldText {string _text; qvec3 _pos,_angles; HAlign; float _scale}`, `WorldTextHandle = uint16_t`. Used by the world-text subsystem and `R_DrawString`. Has no logic. |
| `util.hpp` / `util.cpp` | 376 / 20 | `quake::util::` math helpers (`getAngledVectors`, `pitchYawRollFromDirectionVector`, `redirectVector`, `mapRange`, `lerp`, `cvarToEnum`, `hitSomething`, `traceHitGround`, flag helpers, `canBeHandTouched`, `checkGroundCollision`, `makeMenuCVarAdjuster` which reads `VR_GetMenuMult()`) and `getMaxMSAALevel()` (`glGetIntegerv(GL_MAX_SAMPLES)`). |
| `quakeglm*.hpp` | about 200 | glm aliases: `qvec2/3/4`, `qmat3/4`, `qquat`, `qfloat=float`, `_qf` literal, `vec3_zero`, `toGlVec`, `toGlMat(qquat)`. This is style churn with no functional content. |
| `variantutil.hpp` | 35 | `quake::util::overload_set` / `match()` (std::visit helper). This is style churn. |
| `openvr.hpp` | ~7000 | Vendored `openvr.h` from SteamVR SDK 1.23.7 (`IVRSystem_022`, `IVRCompositor_027`, `IVRInput_010`). Linked against `openvr_api.lib` (`CMakeLists.txt:215,224,259`). |
| `ReleaseFiles\actions.json`, `bindings_*.json` | | SteamVR Input manifest plus 6 default binding profiles. Described below. |

Related files that are not in this inventory but are used by it: `gl_util.hpp/.cpp` (`quake::vr::gl_util::gl_showfn_guard`, `gl_vertex`, all immediate-mode), `menu_keyboard.hpp` (the in-game virtual keyboard), `developer.hpp` (`quake::vr::developerMode()`, which is only used for console/cmd warnings and is unrelated to VR).

## Summary

- The VR module is one monolithic client-side file. It also runs **server-side traces** (`SV_Move`) on the local server's edicts every render frame. Because of that, `SCR_UpdateScreen` switches the QCVM to `sv.qcvm` around `VR_UpdateScreenContent()`. This means VR hand placement only works correctly with a local listen server.
- **OpenVR surface used:** `VR_Init(Scene)`, `IVRSystem` (render target size, projection raw/matrix, eye-to-head, device class/role, legacy `GetControllerState`, which is dead data), `IVRCompositor` (`SetTrackingSpace(Standing)`, `WaitGetPoses`, `Submit` of GL textures) and `IVRInput` (manifest, 2 action sets, 36 actions, skeletal summary for finger curls, and vibration). It does not use overlays, the SteamVR keyboard or the chaperone.
- **Rendering model:** the whole scene and the 2D layer are rendered twice by calling `SCR_UpdateScreenContent()` per eye into a per-eye FBO (optional MSAA). Projection is loaded with `glLoadMatrixf`. The 2D menu, console and HUD are drawn as world-space quads using `glPushMatrix/glTranslatef/glRotatef` from `VR_Draw2D`/`VR_DrawSbar`. **Ironwail is a GL core-profile renderer (no `glMatrixMode`/`glBegin` anywhere; `r_matproj`/`r_matviewproj` built in `R_SetFrustum`, gl_rmain.c:829-867).** All of `VR_SetMatrices`, `VR_Draw2D`, `VR_DrawSbar`, `vr_showfn.cpp` and `gl_util` must therefore be rewritten, not ported. This is the biggest risk.
- **Input model:** SteamVR actions are turned into synthetic `Key_Event`s (`'k'`, `'l'`, `'n'`, `'m'`, `'1'`, `'3'`, `'4'`, `'5'`, `'7'`, `'8'`, `K_MOUSE1/2`, `K_SPACE`, `K_ESCAPE`, menu arrows). These depend on the default binds in `ReleaseFiles/Id1/config.cfg`. Analog locomotion goes directly into `usercmd_t`. `VR_Move()` also fills the many VR `usercmd_t` fields (hand pos/rot/vel, muzzle positions, teleport, `vrbits0`, hotspots, `roomscalemove`). Those fields are owned by the protocol inventory.
- **Hook count:** about 60 call sites in 18 engine files outside `menu.cpp`. Most are one-line `if(vr_enabled.value)` guards. `menu.cpp` has about 200 references, almost all cvar pointers for the VR option menus.
- `vr_enabled` is effectively always 1: `VR_InitCvars` and `VID_VR_Init` both force it. Pancake mode is `vr_fakevr 1`, and a full no-HMD mode is `-novr` (fakevr plus novrinit).
- **Builtin conflict:** QVR `PF_haptic` is **#81**, which is `stof` in IW (pr_cmds.c:3362). QVR also puts `pow` at #80, which is `localsound` in IW.

---

## 1. Architecture

### 1.1 Init and shutdown sequence

| Order | Where (QVR) | VR call | What it does |
|---|---|---|---|
| 0 | `main_sdl.cpp:main` 97-114 | writes `vr_working_directory` (extern `std::string`, vr.cpp:189) | Stores the directory of `argv[0]` (Windows: substring up to the last `\`). It is used to locate `actions.json`. |
| 1 | `host.cpp:Host_Init` 1232 (right after `Cvar_Init`) | `VR_InitCvars()` (vr.cpp:1100) | Sets callbacks `VR_Enabled_f` (disable, then re-enable) and `VR_Deadzone_f` (clamp 0..70). Calls `Cvar_SetValueQuick(&vr_enabled,1)` *before* registration. Then `quake::vr::register_all_cvars()`, then `InitAllWeaponCVars()` (registers 32×69 `vr_wofs_*` cvars). |
| 2 | `gl_rmisc.cpp:R_Init` 306 | `VID_VR_Init()` (vr.cpp:1113) | Model flag fixes: `progs/grenade.mdl`, `proxbomb.mdl` and `mervup.mdl` get `EF_GRENADE` (smoke trail); `progs/backpack.mdl` loses `EF_ROTATE`. Then sets `vr_enabled "1"`, and the callback calls `VR_Enable()`. |
| 3 | `host.cpp:Host_Init` 1298-1301 (after `exec quake.rc`) | `VR_ModAllModels()` if `vr_enabled==1` | Rescales `progs/vrtorso.mdl`, `progs/legholster.mdl` and every weapon model (see 1.9). |
| 4 | `VR_Enable()` (vr.cpp:1423) | | `-fakevr` sets `vr_fakevr=1`. `-novr` sets `vr_fakevr=1` and `vr_novrinit=1`. If both are set it returns true without touching OpenVR. Otherwise: `vr::VR_Init(VRApplication_Scene)`; `VRInput()->SetActionManifestPath(vr_working_directory+"/actions.json")` then `VR_InitActionHandles()`; for each eye it creates the FBO (`GetRecommendedRenderTargetSize`) and computes FOV from `GetProjectionRaw` (`fov_x = atan(-L)+atan(R)`, `fov_y = atan(-U)+atan(D)`, in degrees); `VRCompositor()->SetTrackingSpace(TrackingUniverseStanding)`; `VR_ResetOrientation()`; `SDL_GL_SetSwapInterval(0)` (vsync off, because `WaitGetPoses` paces the frame); `Cbuf_AddText("exec vr_autoexec.cfg\n")`; sets `vr_initialized=true`. |
| 5 | `VR_UpdateScreenContent()` 3399 | lazy re-init | If VR is not initialized and `VR_Enable()` fails, it sets `vr_enabled 0` and returns. |
| 6 | `gl_vidsdl.cpp:VID_Restart` 741-743 / 805-807 | `VID_VR_Disable()` before the mode change, `VR_Enable()` after | Needed because the GL context and its objects are recreated. |
| 7 | `common.cpp:COM_Game_f` 2326 | `VR_InitGame()` | Calls `InitAllWeaponCVars()` again, which **resets all `vr_wofs_*` to per-game defaults** (checks `COM_SkipPath(com_gamedir)=="ad"`). |
| 8 | `common.cpp:COM_Game_f` 2418-2421 | `VR_ModAllModels()` plus `map vrstart` | Runs after `exec quake.rc` on a game change. |
| 9 | `host.cpp:Host_Shutdown` 1370 | `VID_VR_Shutdown()` → `VID_VR_Disable()` | `vr::VR_Shutdown()`, `ovrHMD=nullptr`, `cl.stats[STAT_VIEWHEIGHT]=DEFAULT_VIEWHEIGHT`. FBOs are never freed (`DeleteFBO` is unused). |
| 10 | `sv_main.cpp:SV_SpawnServer` 4265 / `cl_parse.cpp:CL_ParseServerInfo` 1508 | `VR_OnSpawnServer()` / `VR_OnClientClearState()` | Both only reset `vr_wpnbutton_state[2]`. |

### 1.2 Per-frame flow

The frame has two halves: input/cmd happens in `CL_SendCmd`, and poses, aiming and rendering happen in `SCR_UpdateScreen`. A usercmd is built from hand state computed during the previous render frame.

```
Host_Frame
 ├─ Host_FilterTime: maxfps cap disabled when vr_enabled (host.cpp:795)
 ├─ CL_SendCmd (cl_main.cpp:1435)
 │    CL_BaseMove → IN_Move → VR_Move(&cmd) (vr.cpp:4431)
 │       fill cmd VR fields from cl.hand* ; VR_CalcFinalWpnMuzzlePos x2
 │       VR_DoInput(): UpdateActionState, read actions, Key_Event injection,
 │                     action-set switch, menu pointer, returns axes
 │       vrbits0, hotspots (holster/2H/hand-switch), locomotion → forward/side/upmove,
 │       roomscalemove = vr_roomscale_move / host_frametime, snap/smooth turn → vrYaw
 └─ SCR_UpdateScreen (gl_screen.cpp:1429)
      VR_UpdateFlick()                        (visual_handrot for spin-reload anim)
      if vr_enabled && !con_forcedup:
        PR_SwitchQCVM(&sv.qcvm)
        VR_UpdateScreenContent() (vr.cpp:3394)
          VR_UpdateFingerTracking()            (skeletal curls → finger frames 0..5)
          VR_UpdateDevicesOrientationPosition() (WaitGetPoses — blocks)
          readbackYaw handling (VR_PushYaw)
          aim-mode switch → VR_ControllerAiming (default mode 6)
             cl.viewangles ← HMD pitch/yaw ; cl.handrot ← controller + gun angle cvars
             map viewent model → wpn cvar entry ; cl.headvel
             SetHandPos(0/1)  (SV_Move traces vs world+ents, weight smoothing, velocities)
             VR_Do2HAiming ; VR_DoWeaponDirSlerp ; (fakevr: VR_FakeVRControllerAiming)
             cl.aimangles = handrot[main] ; VR_DoTeleportation ; VR_DoWpnButton ; flick-reload
          cl.viewangles[ROLL] = HMD roll ; r_refdef.viewangles/aimangles
          (fakevr/novrinit/dedicated: return here)
          for each eye: vr_viewOffset = rotZ(eye pos * m2u) + floor_offset
             RenderScreenForCurrentEye_OVR(eye):
               bind (msaa) FBO, glViewport, clear, srand(cl.time*1000)
               r_refdef.fov_x/y = eye fov ; glwidth/glheight = rec. size
               SCR_UpdateScreenContent()  → V_RenderView → … R_SetupGL → VR_SetMatrices()
                                            R_RenderScene → showfn::show_crosshair / draw_all_show_helpers
                                          → VR_Draw2D() → VR_DrawSbar()
               MSAA blit-resolve ; GLSLGamma_GammaCorrect(eye.index) ; VRCompositor()->Submit
          mirror: glBlitFramebuffer(eye0 → backbuffer)   (src rect args look wrong, see Open questions)
        PR_SwitchQCVM(old)
      else: cl.viewangles = cl.aimangles; SCR_UpdateScreenContent()
      if vr_fakevr: r_refdef angles = cl.viewangles; SCR_UpdateScreenContent()  (pancake view)
      GL_EndRendering
```

`SCR_UpdateScreenContent()` is new in QVR. It holds the body of BASE `SCR_UpdateScreen` from `V_RenderView` onward (gl_screen.cpp:1348). BASE's final `GLSLGamma_GammaCorrect()` was moved per eye as `GLSLGamma_GammaCorrect(int eyeIndex)`. It reads `VR_GetEyeFBO(eyeIndex).framebuffer` (gl_rmain.cpp:229/281). The desktop path now does no gamma correction at all (the call is commented out, gl_screen.cpp:1411).

### 1.3 Tracking math, coordinates and globals

- `meters_to_units = vr_world_scale / (1.5*0.0254)`, about 26.25 at scale 1 (vr.cpp:218).
- OpenVR to Quake axes: position `{-z, x, y}` for velocities (`openVRCoordsToQuakeCoords`, 1874). The head is `{z, x, y}` in `VR_UpdateDevicesOrientationPosition`.
- `QuatToYawPitchRoll(q)` (408): pitch = `-asin(-2(yz-wx))`, yaw = `atan2(2(xz+wy), w²-x²-y²+z²)` **plus `vrYaw`** (turn angle), roll = `-atan2(2(xy+wz), w²-x²+y²-z²)`. The results are in degrees. `Matrix34ToQuaternion` uses the copysign method.
- Head (per pose update, 2674-2730):
  - `vr_roomscale_move` is the horizontal HMD delta × m2u, rotated by `vrYaw`, × `vr_roomscale_move_mult`. It goes to the server as `cmd.roomscalemove`.
  - `lastHeadOrigin` is the raw HMD position in metres (Quake axis order). Only its Z is used later, for crouch, height calibration and roomscale jump.
  - Eye positions are the head position with **X and Z zeroed** (only height is kept), plus the eye-to-head offset rotated by the head quaternion and then by `-vrYaw`.
  - Both eyes use the same `orientation = headQuat`.
- Controllers (2732-2790): the device role (`TrackedControllerRole_Left/RightHand`) maps to index 0 (off-hand) or 1 (main hand). `vr_lefthanded` swaps them.
  - `position = ((raw.z-lastHead.x), (raw.x-lastHead.y), raw.y) * m2u`.
  - `velocity` / `a_velocity` are raw. They are pushed into `VecHistory` ring buffers of size 15 and 5; `VR_ResetThrowAvgFrames` resizes them from `vr_throw_avg_frames` / `vr_throw_angvel_avg_frames`.
  - `orientation = QuatToYawPitchRoll`.
- Hand world position (`VR_GetWorldHandPos`, 1819): `headLocal = rotZ(ctrl.pos - headOrigin, vrYaw) + headOrigin`; `world = (-hl.x+org.x, -hl.y+org.y, hl.z + org.z + vr_floor_offset + vr_gun_z_offset)`.
- Collision (`VR_GetResolvedHandPos`, 1839): `SV_Move` a ±1 box from `VR_GetAdjustedPlayerOrigin` (z = hand z origin + 40) to the target, using `MOVE_NORMAL` and ignoring the player edict. Only the axes whose plane normal is non-zero are clamped. `VR_UpdateGunWallCollisions` (1775) then traces from the hand to the muzzle position and pulls the hand back.
- `SetHandPos` (1885):
  - Positional weight smoothing (weight cvars and `WpnCVar::WeightPosMult`).
  - Hands are clamped to 50 units from the torso.
  - `cl.handvel` is the controller velocity redirected by yaw.
  - `cl.handthrowvel` uses one of two algorithms (`vr_throw_algorithm`): Basic is the average velocity; CrossAngVel adds `angvel × up*vr_throw_up_center_of_mass`.
  - The weight scales handvel by `pow(map(w,0..0.4,0.55..1),0.4)` and throwvel by `pow(map(w,0..0.4,0.45..1),0.8)`.
  - `cl.handvelmag` is multiplied by `linearity⁴·2` when the hand moves along the body forward direction (this helps punches register).
- Body yaw (`VR_GetBodyYawAngle`, 2536): if either controller is inactive it uses the head yaw blended by pitch. Otherwise it mixes the head-forward direction with the average hand direction (factor 0.8). This drives the player model yaw, the torso and the holster anchors (`VR_GetBodyAnchor`, 2231: crouch pitch `-35·ratio`, z `+2 - crouch·18`, up × `vr_height_calibration`).
- `VR_GetCrouchRatio() = clamp(vr_height_calibration / lastHeadOrigin.z - 1, 0, 1)`. `VR_CalibrateHeight()` sets `vr_height_calibration` to the current head Z (menu action "Calibrate Height").
- Aim modes (`vr_aimmode`, 3422-3492): 0-5 are legacy head/mouse/blended modes carried over from Quakespasm-Rift. 6 (`e_CONTROLLER`, the default) is the only mode the gameplay uses.
- Turning: `vrYaw` is changed in `VR_Move` (snap by `vr_snap_turn` degrees on a sign change, or smooth `yaw·frametime·100·vr_turn_speed`). `lastVrYawDiff` feeds the weight-smoothing compensation. `VR_PushYaw()` sets `readbackYaw`; on the next frame `vrYaw = cl.viewangles.yaw - (hmdYaw - vrYaw)`. This keeps the server-set view yaw on teleports, `svc_setview`, `svc_setangle` and the intermission.
- Teleport (2592): while the Teleport action is held, it traces a ±6×±12 hull from the torso along the off-hand forward direction for `vr_teleport_range`. A hit is valid if `normal.z ∈ [0.75,1]`; the impact gets z+12 and particle effect `R_RunParticle2Effect(pos,0,7,2)`. On release it sets `player.origin` (client entity) and sends `cmd.teleport_target` with bit `QVR_VRBITS0_TELEPORTING`.
- Weapon button (3096): a hand within 2.7 units of `cl.{off,main}hand_wpn_button.origin` (hand position offset by fwd·2 and up·-2.5) gives a rising edge, which sends `Key_Event('7'/'8')` (impulse 42/43, change ammo). This is throttled to 0.2 s.
- Flick (spin) reload (3206): only for the super shotgun (`WID_SUPER_SHOTGUN`) when the clip is not full. It triggers when `|avg angvel| ≥ vr_spinreload_x_angular_threshold` and `dot(fwd, restUp) > 0.6`. This sets the `*_reloadflicking` bits and `flickreload_effect=360`, which `VR_UpdateFlick` animates into `cl.visual_handrot` at `vr_spinreload_pitch_speed` degrees per second.
- 2H aiming (2943-3070): see the weapons inventory. Its outputs are `vr_should_aim_2h[2]`, `vr_active_2h_helping_hand[2]`, `vr_2h_aim_transition[2]` (0..1, speed 5/s, extern, read by view.cpp) and `vr_2h_aim_stock_transition[2]`. It overwrites `cl.handrot[holding]`.
- Hotspots (`VR_Move` 4520): for each hand, the first match in order: L/R shoulder, L/R hip, L/R upper holster, 2H grab, hand switch (<5 units), otherwise none. The results go to `cl.hotspot[]` and `cmd.{off,main}hand_hotspot` (`QVR_HS_*`, quakedef_macros.hpp:286-295).

### 1.4 Render targets and eye matrices

- `struct fbo_t {framebuffer, depth_texture(GL_DEPTH_COMPONENT24), texture(GL_RGBA8), msaa_framebuffer, msaa_texture, msaa_depth_texture, msaa, size}` (vr.hpp:302). The textures are recreated when the recommended size changes. MSAA uses `glTexImage2DMultisample(vr_msaa samples)`, clamped to `GL_MAX_SAMPLES`.
- Per eye: `glwidth/glheight` are temporarily overwritten with the render-target size, so everything that reads `glwidth` (the viewport in `GLSLGamma`, line widths in showfn) renders at eye resolution.
- Projection: `VR_SetMatrices()` (3542) calls `GetProjectionMatrix(eye, near=4, far=gl_farclip)`, transposes it, and does `glMatrixMode(GL_PROJECTION); glLoadMatrixf`. It is called from `R_SetupGL` (gl_rmain.cpp:607-610) instead of `GL_SetFrustum`/viewport. The rest of `R_SetupGL` (the modelview built from `r_refdef.vieworg/viewangles`) is unchanged.
- The view position comes from `V_CalcRefdef` (view.cpp:1010-1013): `r_refdef.vieworg = player.origin + vr_viewOffset`, with no viewheight and no bob. `r_refdef.viewangles = cl.viewangles`, which are the HMD angles. `vr_viewOffset` is per eye and comes from `VR_UpdateScreenContent`.
- Frustum culling: `R_SetFrustum` widens `fovx` by 25 when `vr_enabled` (gl_rmain.cpp:561), as a hack against culling at asymmetric FOVs.
- 2D and HUD in 3D (`VR_Draw2D`, 3598):
  - It forces `glwidth/glheight/vid.conwidth/conheight` to 320×200.
  - It translates to a smoothed target that is `vr_menu_distance` in front of either the frozen head angles (`LastHeadAngles`), the live head (`FollowHead`), or an off-hand/main-hand ray (`vr_menumode`).
  - It rotates by yaw-90 and pitch+90, centres the page, and scales by `vr_menu_scale`.
  - It then draws the same list as BASE's 2D pass, minus the crosshair and plus `M_DrawKeyboard`. The status bar is drawn by `VR_DrawSbar` (3754): it is attached to a hand in controller mode (main hand: -5 right; off hand: quatLookAt plus the `vr_sbar_offset_*` cvars) or in front of the gun otherwise, scaled by `vr_hud_scale`.
  - `GL_SetCanvas` becomes a no-op in VR (gl_draw.cpp:932), so every 2D draw lands in the pushed 3D matrix. `Draw_FadeScreen` is also a no-op (887).
- Menu pointer (`VR_DoInput_UpdateVRMouse`, 3945): intersects the hand ray with the menu plane and projects it to 320×240 menu coordinates in `vr_menu_mouse_x/y`. `vr_menu_mouse_click` is the menu Enter action. menu.cpp:5113 (`M_DrawKeyboard`) and keys.cpp:561 (`Key_Console`) consume them. In fake VR the SDL mouse position is used instead.

### 1.5 OpenVR input (IVRInput)

- Action sets: `/actions/default` and `/actions/menu`, one active at a time. The switch happens in `VR_DoInput` 4229: menu when in menu/console and Escape is not held; default otherwise when menu Back is not held.
- Per frame: `UpdateActionState(1 set)`. Then digital, analog and skeletal reads with `k_ulInvalidInputValueHandle` (any device).

| Action | Type | Effect (in game) | Menu set |
|---|---|---|---|
| LeftHandAnim / RightHandAnim | skeleton | `GetSkeletalSummaryData(FromDevice)` → `vr_ss_lefthand/righthand.flFingerCurl[5]` → finger frames `clamp(curl+vr_finger_grip_bias)*5`. The thumb is auto-closed if the average curl of the other fingers is >0.5 (`vr_finger_auto_close_thumb`). Blending speed is `vr_finger_blending_speed`. | |
| Locomotion | vector2 | `(y,x)` → forward/side move | |
| Turn | vector2 | `.x` → yaw | |
| FireMainHand / FireOffHand | bool | `Key_Event(K_MOUSE1/K_MOUSE2)` (+attack / +offhandattack) | |
| Jump | bool (rising) | `K_SPACE`. Also a roomscale jump when `headVel.y > vr_roomscale_jump_threshold` and head Z > calibration. | |
| Next/PrevWeaponMainHand | bool (rising) | `'1'` / `'4'` (impulse 10 / 15) | |
| Next/PrevWeaponOffHand | bool (rising) | `'3'` / `'5'` (impulse 12 / 16) | |
| Escape | bool (rising) | `K_ESCAPE` plus haptic. Stores `lastMenuAngles`. | |
| Speed | bool | `in_speed.state` | |
| Teleport | bool | `vr_teleporting` | |
| LeftGrab / RightGrab | bool | `Key_Event('k'/'l')` → `in_grableft/right` (cl_input.cpp:794) → `vr_left/right_grabbing` | |
| LeftReload / RightReload | bool | `'n'/'m'` → `in_reloadleft/right`. There is **no default binding in any profile.** | |
| BMove{Forward,Backward,Left,Right}, BTurn{Left,Right} | bool | Boolean locomotion. When any of them is set it overrides the analog axes. | |
| LeftHaptic / RightHaptic | vibration | `VR_DoHaptic` | |
| Navigation | vector2 | | Y axis → `K_UPARROW/K_DOWNARROW` (deadzone 0.025, uses `deltaY` for edges) |
| Up/Down/Left/Right/Enter/Back | bool | | arrows / `K_ENTER` / `K_ESCAPE`, each with a menu haptic |
| AddToShortcuts | bool | Read but unused | |
| MultiplierHalf / PlusOne / PlusOne2 | bool | | `vr_menu_mult` = 0.5 or 1, plus 1 each → `VR_GetMenuMult()` used by the menu adjusters in `util.hpp` (≥3 becomes ×6) |

- Fake-VR input (`vr_fakevr`): grab is `!(in_grab*.state&1)`, meaning **inverted**: hands grab by default and a key releases. Reload and flick-reload come from the `+reload*` / `+flickreload*` key buttons. The mouse is the SDL mouse.
- Haptics: `VR_DoHaptic(hand, delay, duration, freq, amp)` (3910) calls `TriggerHapticVibrationAction(Left/RightHaptic, …, invalid handle)`. It is suppressed if dedicated, fake VR, novrinit or `vr_disablehaptics`. The only external caller is QC builtin **#81 `PF_haptic(float hand, float delay, float duration, float frequency, float amplitude)`** (pr_cmds.cpp:712, table entry 2391). The menu haptic is fixed at 0, 0.1 s, 50 Hz, 0.5 on both hands, restricted to `activeOrigin`.
- Legacy `IVRSystem::GetControllerState` is still called per controller (2766), but `state`/`lastState` are never read. It is dead.
- `vrivhLeft/vrivhRight` input-source handles are read (`/user/hand/left|right`) and never used.

### 1.6 Fake VR (pancake) mode

`vr_fakevr 1`, set by `-fakevr` or `-novr`:
- No compositor submit. `VR_UpdateDevicesOrientationPosition` still runs `WaitGetPoses` unless `vr_novrinit` is also set.
- `VR_FakeVRControllerAiming` places the hands at `origin + fwd·16.5 ± right·5.5 + up·15.5`, with `handrot = viewangles` and roll = `vr_fakevr_handroll`. `vrYaw` follows the mouse yaw.
- `SCR_UpdateScreen` renders a normal desktop frame using `cl.viewangles`.
- `VR_EnabledAndNotFake()` (1196) is the guard for anything stereo-specific (projection, 2D-in-3D, view offset, death roll, `V_StartPitchDrift`).

### 1.7 Overlays and keyboard

There are none from OpenVR. The on-screen keyboard is QVR's own `quake::vr::menu_keyboard` (menu_keyboard.hpp), drawn by `M_DrawKeyboard` inside the 2D pass and driven by the VR menu pointer.

### 1.8 PAK helpers (multi-`start.bsp`)

- `VR_OnLoadedPak(pak)` is called from `COM_AddGameDirectory` (common.cpp:2220). It records the pak's base name, and also records it in a separate list if the pak contains `maps/start.bsp`.
- `COM_FindFile` (common.cpp:1436-1444) skips `maps/start.bsp` from any pak other than `VR_GetActiveStartPakName()` (indexed by `vr_activestartpaknameidx`) or `pak0`.
- The single-player menu (menu.cpp:467) lists `VR_GetLoadedPakNamesWithStartMaps()`.

### 1.9 Weapon offset cvar table

- `cvar_t vr_weapon_offset[32*72]` (vr.cpp:282). The entry index is the weapon slot 0..31 and the setting is `WpnCVar` 0..71 (vr.hpp:438-516; indices 24-26 and 54-56 are deprecated and unregistered).
- The names are `vr_wofs_<key>_NN` with NN = 01..32 (`CopyWithNumeral` replaces the trailing `nn`; the strings are malloc'd and leaked). There are 69 keys. The full list is at vr.cpp:947-1024, e.g. `vr_wofs_x_nn`, `_scale_`, `_id_` (the model name), `_muzzle_x_`, `_2h_*`, `_hand_av_`, `_ch_mode_z_` (sic), `_wpnbtn_*`, `_wpntxt_*`, `_zb_`, `_zb_2h_`, `_w_*mult_`. All are `CVAR_ARCHIVE`.
- Defaults are set in `InitAllWeaponCVars` (1031). One set is for `ad` (Arcane Dimensions); another covers id1, hipnotic and rogue. After those come `progs/hand.mdl` (its index is stored in `vr_hardcoded_wpn_cvar_fist`, default 16) and `progs/v_grpple.mdl`. The rest get `"-1"`. Most real per-weapon values come from `config.cfg`.
- Lookup: `VR_GetWpnCVarFromModel(model)` does a strcmp of `model->name` against each `vr_wofs_id_NN`. It is done each frame for `cl.viewent` and `cl.offhand_viewent` in `VR_ControllerAiming`.
- `VR_ApplyModelMod(scale, ofs, hdr)` (623) modifies alias models in place: `hdr->scale = original_scale*scale*k`, `scale_origin = (original_scale_origin+ofs)*k`, `k = vr_world_scale/0.75 * vr_gunmodelscale`. This needs the new `aliashdr_t::original_scale/original_scale_origin` fields (model inventory).

---

## 2. Public interface

### 2.1 `vr.hpp`

Lifecycle and frame:
- `void VR_InitCvars()`: once, in Host_Init.
- `void VID_VR_Init()`: in R_Init.
- `void VID_VR_Shutdown()`.
- `bool VR_Enable()`.
- `void VID_VR_Disable()`.
- `void VR_UpdateScreenContent()`: the whole VR frame.
- `void VR_Draw2D()`.
- `void VR_DrawSbar()`.
- `void VR_SetMatrices()`: loads the eye projection.
- `void VR_Move(usercmd_t*)`.
- `void VR_InitGame()`.
- `void VR_OnSpawnServer()`.
- `void VR_OnClientClearState()`.
- `void VR_UpdateFlick()`.
- `void VR_ResetThrowAvgFrames()`.

Angles and head:
- `void VR_PushYaw()`.
- `qvec3 VR_AddOrientationToViewAngles(const qvec3&)`: uses the current eye.
- `void VR_SetAngles(const qvec3&)`: sets aim, view and lastAim.
- `void VR_ResetOrientation()`.
- `void VR_CalibrateHeight()`.
- `const qvec3& VR_GetHeadOrigin()`.
- `qvec3 VR_GetLastHeadOrigin()`.
- `qfloat VR_GetCrouchRatio()`.
- `qfloat VR_GetTurnYawAngle()`.
- `qvec3 VR_GetHeadAngles()` (= `cl.viewangles`).
- `qfloat VR_GetHeadYawAngle()`.
- `qfloat VR_GetBodyYawAngle()`.
- `std::tuple<qvec3×8> VR_GetBodyYawAngleCalculations()`.
- `float VR_GetMenuMult()`.
- `bool VR_EnabledAndNotFake()`.

Hands, collision and physics (these are also called server-side):
- `qvec3 VR_GetAdjustedPlayerOrigin(qvec3)`.
- `qvec3 VR_GetWorldHandPos(int hand, const qvec3& playerOrigin)`.
- `qvec3 VR_GetResolvedHandPos(edict_t*, const qvec3& worldHandPos, const qvec3& adjOrigin)`.
- `qvec3 VR_UpdateGunWallCollisions(edict_t*, int hand, VrGunWallCollision& out, qvec3 resolvedHandPos)`.
- `struct VrGunWallCollision {bool _colliding; bool _normals[3]; edict_t* _ent;}`.
- `void VR_SetHandtouchParams(int hand, edict_t* player, edict_t* target)`: writes `player->v.touchinghand`, `target->v.handtouch_hand` and `target->v.handtouch_ent`.
- `void VR_SetFakeHandtouchParams(edict_t*, edict_t*)` (hand=2).
- `float VR_GetEasyHandTouchBonus()` (=4.5).
- `int VR_OtherHand(int)`.
- `bool VR_IsActive2HHelpingHand(int)`.
- `bool VR_IsHandGrabbing(int)`.
- `bool VR_IsHandReloadFlicking(int)`.
- `void VR_DoHaptic(int hand, float delay, float duration, float freq, float amp)`.

Holsters and body:
- `VR_GetLeftHipPos`, `VR_GetRightHipPos`.
- `VR_GetLeftUpperPos`, `VR_GetRightUpperPos`.
- `VR_GetLeftShoulderHolsterPos`, `VR_GetRightShoulderHolsterPos`.
- `VR_GetShoulderStockPos(holding, helping)`.
- `VR_InHipHolsterDistance(hand, holster)`, `VR_InShoulderHolsterDistance(hand, holster)`, `VR_InUpperHolsterDistance(hand, holster)`.

2H:
- `VR_Get2HHoldingHandPos`, `VR_Get2HHelpingHandPos`.
- `VR_Get2HVirtualStockMix`.
- `VR_InStockDistance`.

Model and anchor math (uses r_alias internals):
- `VR_GetAliasVertexOffsets(entity_t*, int vertex)`.
- `VR_GetScaledAliasVertexOffsets(…)`.
- `VR_GetScaledAndAngledAliasVertexOffsets(…)`.
- `VR_GetScaledAndAngledAliasVertexPosition(entity_t*, int vertex, const qvec3& extraOfs, const qvec3& rot, bool horizFlip)`.
- `VR_GetWpnFixed2HFinalPosition(…)`.
- `VR_CalcFinalWpnMuzzlePos(int hand)`.
- `VR_CalcMainHandWpnMuzzlePos()` (declared only).
- `entity_t* VR_GetAnchorEntity(hand)` (`&cl.viewent` / `&cl.offhand_viewent`).
- `bool VR_GetHorizFlip(hand)` (off-hand = true).

Weapon cvars:
- `cvar_t& VR_GetWpnCVar(entry, WpnCVar)`.
- `float VR_GetWpnCVarValue(entry, WpnCVar)`.
- `int& VR_GetWpnCvarEntry(hand)`, `int& VR_GetMainHandWpnCvarEntry()`, `int& VR_GetOffHandWpnCvarEntry()`.
- `int VR_GetWpnCVarFromModel(qmodel_t*)`.
- Vec3 getters: `VR_GetWpnOffsets`, `VR_GetWpn2HOffsets`, `VR_GetWpnFixed2HOffsets`, `VR_GetWpnGunOffsets`, `VR_GetWpnFixed2HHandAngles`, `VR_GetWpnFixed2HMainHandOffsets`, `VR_GetWpnAngleOffsets`, `VR_GetWpn2HAngleOffsets`, `VR_GetWpnMuzzleOffsets`, `VR_GetWpnButtonOffsets`, `VR_GetWpnButtonAngles`, `VR_GetWpnTextOffsets`, `VR_GetWpnTextAngles`, `VR_GetWpnHandOffsets`.
- `Wpn2HMode VR_GetWpn2HMode(entry)`.
- `WpnCrosshairMode VR_GetWpnCrosshairMode(entry)`.
- `void ApplyMod_Weapon(entry, aliashdr_t*)`.
- `void VR_ApplyModelMod(scale, ofs, hdr)`.
- `VR_ModAllWeapons()`, `VR_ModVRTorsoModel()`, `VR_ModVRLegHolsterModel()`, `VR_ModAllModels()`.

Misc:
- `fbo_t& VR_GetEyeFBO(int)`.
- `bool svPlayerActive()`: true when `svs.clients[0]` is active and spawned, `sv.active`, `ss_active`, and `cls.signon==SIGNONS`.
- `edict_t* getPlayerEdict()` (= `svs.clients[0].edict`).
- PAK: `VR_GetActiveStartPakName`, `VR_GetLoadedPakNames`, `VR_GetLoadedPakNamesWithStartMaps`, `VR_ExtractPakName(string_view | pack_t)`, `VR_OnLoadedPak(pack_t&)`.

Enums (the values are persisted in cvars and must stay stable):
- `VrAimMode` 0-6.
- `VrCrosshair` 0-3.
- `VrMovementMode` 0-1.
- `VrSbarMode`, `Vr2HMode`, `VrOptionHandSelection`, `VrPlayerShadows`, `VrHolsterMode`, `VrReloadMode`, `VrWeaponThrowMode`, `VrWeaponCycleMode`, `VrHolsterHaptics`, `VrMeleeBloodlust`, `VrEnemyDrops`, `VrAmmoBoxDrops`, `VrMenuMode`, `VrForceGrabMode`, `VrThrowAlgorithm`.
- `Wpn2HMode`, `WpnCrosshairMode`, `Wpn2HDisplayMode`, `WpnButtonMode`, `WpnTextMode`, `WpnCVar`, `FingerIdx`.
- Constants: `cVR_OffHand=0`, `cVR_MainHand=1`, `cVR_FakeHand=2`.

Extern globals:
- `vr_hardcoded_wpn_cvar_fist`.
- `vr_2h_aim_transition[2]`.
- `vr_teleporting`, `vr_teleporting_impact`, `vr_teleporting_impact_valid`.
- `vr_ss_lefthand/righthand` (`vr::VRSkeletalSummaryData_t`, which leaks OpenVR types into the header).
- `vr_fingertracking_frame[2][6]`.
- `vr_menu_mouse_x/y`, `vr_menu_mouse_click`.
- `vr_menu_target`, `vr_menu_angles`, `vr_menu_intersection_point`.
- Declared locally by users: `vr_viewOffset` (view.cpp:1012) and `vr_working_directory` (main_sdl.cpp:95).

### 2.2 `vr_showfn.hpp`

- `quake::vr::showfn::draw_all_show_helpers()`: draws the virtual stock, hip/shoulder/upper holster lines, torso debug lines, teleport line (a colour-faded line strip, blue if valid and red otherwise), weapon offset/muzzle/2H helpers, hand pos/rot, and the anchor-vertex point clouds for hand/2H/button/text/muzzle.
- `show_crosshair()`: requires `svPlayerActive()`. It is drawn per hand from the muzzle along `handrot`. The point mode uses `TraceLine` to 4096 or `vr_crosshair_depth`; the line mode uses `TraceLineToEntity` (both are server traces).
- Externs `vr_impl_draw_*` ints (7). The menu sets them while an entry is hovered.

### 2.3 `vr_cvars.hpp`

- `quake::vr::register_all_cvars()`.
- `get_fingers_and_base_xyz()`, `get_fingers_and_base_offhand_xyz()`, `get_fingers_xyz()`, `get_finger_{thumb,index,middle,ring,pinky,base}_xyz()`.
- `get_weapon_reloading_enabled()` = `vr_reload_mode!=0 && vr_holster_mode==0`.

---

## 3. Call sites in the rest of the engine (the hook list)

Format: QVR file:line (function) → VR API. The IW column gives the equivalent location in Ironwail 0.8.2.

### host.cpp
| QVR | Call | Purpose | IW |
|---|---|---|---|
| 795 `Host_FilterTime` | `!vr_enabled.value` | Disables the `host_maxfps` cap in VR (`WaitGetPoses` paces the frame) | host.c:782 `Host_GetFrameInterval`, a different design |
| 1232 `Host_Init` | `VR_InitCvars()` | Registers cvars (after `Cvar_Init`, before `COM_Init`) | host.c:1382 |
| 1298-1300 `Host_Init` | `if(vr_enabled==1) VR_ModAllModels()` | Scales models after `quake.rc` | host.c |
| 1370 `Host_Shutdown` | `VID_VR_Shutdown()` | Before `VID_Shutdown` | host.c:1490 |

### gl_rmisc.cpp
| 306 `R_Init` | `VID_VR_Init()` | Model flag fixes, sets `vr_enabled 1` (triggers VR_Enable) | gl_rmisc.c:287 |

### gl_vidsdl.cpp
| 741-743 `VID_Restart` | `VID_VR_Disable()` | Before the GL context is recreated | gl_vidsdl.c:709 |
| 805-807 `VID_Restart` | `VR_Enable()` | After | |

### main_sdl.cpp
| 95-114 `main` | writes `vr_working_directory` | Directory for `actions.json` | main_sdl.c:127 |

### gl_screen.cpp
| 1348-1355 `SCR_UpdateScreenContent` (new, split out of `SCR_UpdateScreen`) | `if(VR_EnabledAndNotFake() && !con_forcedup) VR_Draw2D(); else <BASE 2D pass>` | 2D in 3D | IW 2D pass is inline in `SCR_UpdateScreen` gl_screen.c:2115+. The SCR_Draw* helpers are **static** in IW (`SCR_DrawFPS` 728, `SCR_DrawClock` 846, `SCR_DrawDevStats` 1017, `SCR_DrawTurtle` 1066, `SCR_DrawNet` 1093, `SCR_DrawPause` 1110, `SCR_DrawLoading` 1146, `SCR_DrawConsole` 1595, `SCR_DrawNotifyString` 1968), and there is no `SCR_DrawRam`. vr.cpp:159-173 externs all of these. |
| 1411 | commented out `GLSLGamma_GammaCorrect` x2 | | |
| 1467 `SCR_UpdateScreen` | `VR_UpdateFlick()` | | |
| 1469-1482 | `if(vr_enabled && !con_forcedup){ PR_SwitchQCVM(&sv.qcvm); VR_UpdateScreenContent(); restore }` | The VR frame | IW has qcvm (progs.h:308-311) |
| 1483-1490 | else `cl.viewangles = cl.aimangles; r_refdef.viewangles/aimangles = cl.aimangles; SCR_UpdateScreenContent()` | The desktop path relies on the split aim/view angles | |
| 1493-1499 | `if(vr_fakevr==1)` render the desktop again with `cl.viewangles` | Pancake | |

### gl_rmain.cpp
| 229/281 `GLSLGamma_GammaCorrect(int eyeIndex)` | `VR_GetEyeFBO(eyeIndex)` | Gamma per eye (reads the eye FBO) | IW has no GLSLGamma; gamma is in `GL_PostProcess` gl_rmain.c:331 |
| 561 `R_SetFrustum` | `if(vr_enabled) fovx += 25` | Culling hack | gl_rmain.c:829 (matrix-based planes) |
| 607-610 `R_SetupGL` | `if(VR_EnabledAndNotFake()) VR_SetMatrices(); else <BASE projection/viewport>` | Eye projection | gl_rmain.c:844 `GL_FrustumMatrix(r_matproj…)` / 897 `R_SetupGL` |
| 1016-1026 `R_DrawViewModel` | `if(!vr_enabled) glDepthRange(0,0.3)` | The viewmodel is a world object in VR | gl_rmain.c:1154 |
| 1433 `R_RenderScene` | `vr_player_shadows` (`VrPlayerShadows`) | View-entity shadows | gl_rmain.c:1915 |
| 1491-1494 `R_RenderScene` | `if(vr_enabled) showfn::show_crosshair()` | | |
| 1522 / 1537 | `get_weapon_reloading_enabled()`, `vr_show_weapon_text` | Weapon ammo text | |
| 1546 | `vr_leg_holster_model_enabled` | Holster slot models | |
| 1583 | `vr_vrtorso_enabled` | Torso | |
| 1592-1594 | `if(vr_enabled) showfn::draw_all_show_helpers()` | Debug helpers | |

### gl_draw.cpp
| 887 `Draw_FadeScreen` | `if(vr_enabled) return` | | gl_draw.c:1110 (takes alpha) |
| 932 `GL_SetCanvas` | `if(VR_EnabledAndNotFake() && !con_forcedup) return` (after setting `currentcanvas`) | Keeps the 3D menu matrix | gl_draw.c:1288, which computes its own ortho matrix |

### view.cpp
| 122 `V_CalcRoll` | `vr_enabled` → no roll | | view.c:80 |
| 160 `V_CalcBob` | `vr_enabled` → 0 | | view.c:111 |
| 204-206 `V_StartPitchDrift` | `VR_EnabledAndNotFake()` → `VR_ResetOrientation(); return` | `centerview` recentres | view.c:147 |
| 249 `V_DriftPitch` | `vr_enabled` → no drift | | view.c:184 |
| 392 `V_ParseDamage` | `!vr_enabled \|\| vr_viewkick` → damage kick | | view.c:283 |
| 710-733 `CalcGunAngle` | controller mode → `VR_GetWpnAngleOffsets(entry)` plus `visual_handrot` | Viewmodel angles | view.c:607 (void signature) |
| 896 `V_CalcViewRoll` | death roll only if `!VR_EnabledAndNotFake()` | | view.c:710 |
| 924-930 `V_CalcIntermissionRefdef` | `VR_AddOrientationToViewAngles`, `VR_SetAngles` | | view.c:737 |
| 1002 `V_CalcRefdef` | `ent->angles[YAW] = VR_GetBodyYawAngle()` | Player model yaw | view.c:763 |
| 1010-1013 | `r_refdef.vieworg = ent->origin + vr_viewOffset` (when `VR_EnabledAndNotFake`) | **Eye position** | |
| 1053 | `!vr_enabled` → `V_BoundOffsets` | | |
| 1061/1065-1069 | `CalcGunAngle(…cl.handrot[main], visual_handrot…)`; controller mode `view->origin = handpos + vmeshoffset + gunOffset` | Viewmodel attached to the hand | |
| 1134/1140 | gunkick disabled unless `vr_viewkick` | | |
| 1175-1187 `V_SetupOffHandWpnViewEnt` | same for the off-hand | | new |
| 1231-1255 `V_SetupVRTorsoViewEnt` | `VR_GetBodyYawAngle`, `VR_GetCrouchRatio`, `VR_GetHeadOrigin`, `vr_vrtorso_*` | | new |
| 1264/1301 | `cl.hotspot[]` highlights holsters | | new |
| 1380-1415 `fingerIdxToOffset` | `quake::vr::get_finger*_xyz()` | | new |
| 1421-1504 `V_SetupHandViewEnt` | `VR_GetWpnAngleOffsets(fist)`, `VR_GetWpnHandOffsets`, `VR_GetWpnCVarValue(HandAnchorVertex/HideHand/TwoHDisplayMode)`, `VR_GetScaledAndAngledAliasVertexPosition`, `VR_OtherHand`, `VR_GetWpnCvarEntry`, `vr_2h_aim_transition`, `VR_GetWpnFixed2HFinalPosition`, `ApplyMod_Weapon(fist)`, `vr_fingertracking_frame` | Hand models | new |
| 1513-1557 `V_SetupFixedHelpingHandViewEnt` | `VR_GetWpnFixed2HHandAngles`, `VR_GetWpnFixed2HFinalPosition`, `vr_2h_aim_transition`, `ApplyMod_Weapon`, `vr_fingertracking_frame` | | new |
| 1566-1610 `V_SetupWpnButtonViewEnt` | `VR_GetWpnButtonOffsets/Angles`, `VR_GetWpnCVarValue`, `VR_GetScaledAndAngledAliasVertexPosition` | | new |
| 1614-1642 `V_SetupWpnTextViewEnt` | `VR_GetWpnTextOffsets/Angles/Scale` … | | new |
| 1645-1663 `V_RenderView_WeaponModels` | `VR_GetMain/OffHandWpnCvarEntry`, `VR_GetWpnGunOffsets` → `V_CalcRefdef(entry, handpos, gunOffset)` | | new |
| 1667-1710 `V_RenderView_Holster*` | `VR_GetBodyYawAngle`, `VR_GetLeft/RightHipPos`, `VR_GetLeft/RightUpperPos`, `vr_leg_holster_model_enabled` | | new |
| 1715-1806 `V_RenderView_HandModels` | `VR_GetWpnCVarFromModel`, `VR_GetWpnCVar(TwoHDisplayMode/ZeroBlend/TwoHZeroBlend)`, `VR_IsActive2HHelpingHand`, `VR_OtherHand` | | new |
| 1810-1866 | torso, weapon buttons, weapon text | | new |

view.cpp is the largest consumer. It is covered in detail by the view/render inventory.

### cl_main.cpp
| 778 `CL_RelinkEntities` | `VR_PushYaw()` when the view entity teleports (delta >100) | | cl_main.c:506 |
| 1451 `CL_SendCmd` | `VR_Move(&cmd)` after `IN_Move` | **Main input hook** | cl_main.c:812 |

### cl_parse.cpp
| 1508 `CL_ParseServerInfo` | `VR_OnClientClearState()` after `CL_ClearState` | | cl_parse.c:285 |
| 2942 `svc_setangle` | `VR_SetAngles(cl.viewangles)` | | cl_parse.c:1181 |
| 2954 `svc_setview` | `VR_PushYaw()` | | cl_parse.c:1187 |

### keys.cpp / menu.cpp
| keys.cpp 558-564 `Key_Console(int, bool fromVirtualKeyboard)` | Swallows `K_ENTER` if the pointer hovers the virtual keyboard (`vr_menu_mouse_x/y`, `vr_fakevr`) | | keys.c:307 |
| menu.cpp 5097-5115 `M_DrawKeyboard` | `vr_menu_mouse_x/y/click` | Virtual keyboard | none in IW |
| menu.cpp 5286 `M_Keydown` | same hover check | | |
| menu.cpp 467 `makeSinglePlayerMenu` | `VR_GetLoadedPakNamesWithStartMaps()` | Choose the start map pak | |
| menu.cpp 1688 `makeQVRSPlayerCalibrationMenu` | `VR_CalibrateHeight` | | |
| menu.cpp 2225-2226 `makeQVRSTorsoMenu` | `VR_ModVRTorsoModel`, `VR_ModVRLegHolsterModel` | Live re-scale | |
| menu.cpp 2501-3283 `makeQVRDTWeaponConfiguration{1..5}Menu` | `VR_GetOff/MainHandWpnCvarEntry`, `VR_GetWpnCVar`, `VR_ModAllWeapons`, `showfn::vr_impl_draw_*` | Weapon offset dev tool | |
| menu.cpp 3400 `makeQVRDTDebugUtilitiesMenu` | `VR_ResetThrowAvgFrames` | | |
| menu.cpp (throughout) | pointers to about 170 `vr_*` cvars | VR option menus | IW menu is plain C, so these need re-creating |
| util.hpp 179/196 `makeMenuCVarAdjuster/ValueAdjuster` | `VR_GetMenuMult()` | | |

### common.cpp
| 1438-1443 `COM_FindFile` | `VR_ExtractPakName`, `VR_GetActiveStartPakName` | Multi start.bsp | common.c:1996 |
| 2220 `COM_AddGameDirectory` | `VR_OnLoadedPak(*pak)` | | common.c:2496 |
| 2326 `COM_Game_f` | `VR_InitGame()` | Resets weapon cvars | common.c:2736 |
| 2418-2421 `COM_Game_f` | `if(vr_enabled){ map vrstart; VR_ModAllModels(); }` | | |

### host_cmd.cpp
| 3106-3113 `Host_Startdemos_f` | `if(vr_enabled)` → `maxplayers 1; deathmatch 0; coop 0; map vrstart; centerview` instead of demos | Boot into the VR start map | host_cmd.c:3657 |

### sv_main.cpp
| 3362-3383 `SV_WriteClientdataToMessage` | `#if 0` block using `VR_GetWorldHandPos/…/VR_UpdateGunWallCollisions` | Dead code | |
| 4264-4265 `SV_SpawnServer` | `VR_OnSpawnServer()` at the end | | sv_main.c:1908 |

### sv_phys.cpp
| 1015 `SV_WalkMove` | `stepsize = vr_player_stepsize.value` (replaces `STEPSIZE` 18) | | sv_phys.c:845 |
| 1076-1148 `SV_Handtouch` (new) | `VR_GetEasyHandTouchBonus`, `VR_SetHandtouchParams`, `cVR_*` | Hand touch | new |
| 1180-1206 `SV_VRWpntouch` (new) | `VR_GetWorldHandPos`, `VR_GetAdjustedPlayerOrigin`, `VR_GetResolvedHandPos`, `VR_UpdateGunWallCollisions`, `VR_SetHandtouchParams`, then `SV_Impact(…, &entvars_t::vr_wpntouch)` | Weapon-touches-entity callback | new |

The server-side code calls `VR_GetWorldHandPos`, which reads the **client** `controllers[]` and `vrYaw`. It only works for the local player.

### world.cpp
| 438-440 `SV_TouchLinks` | `vr_body_interactions`, `vr_fakevr` → `VR_SetFakeHandtouchParams` | Body touch counts as hand touch | world.c:336 |
| 464-496 | `VR_GetEasyHandTouchBonus`, `vr_enabled`, `VR_SetHandtouchParams` | | |
| 647 `SV_LinkEdict` | `VR_GetEasyHandTouchBonus()` | Enlarges the hand-touch box | world.c:467 |

### pr_cmds.cpp
| 712-725, table 2391 | `PF_haptic` #81 → `VR_DoHaptic` | QC haptics | IW #81 = `stof` (pr_cmds.c:3362): **conflict** |

### sbar.cpp / saveutil.cpp
| sbar.cpp 1380 `Sbar_Draw` | `get_weapon_reloading_enabled()` → clip counters | | sbar.c:1608 |
| saveutil.cpp 192-237 | `vr_autosave_show_message`, `vr_autosave_seconds`, `vr_autosave_on_changelevel` | Autosave (new file) | none |

### Headers
- `serverdefines.hpp:3`, `server.hpp:26`, `modeleffects.hpp:3` and `gl_model.hpp:33` include `vr_macros.hpp`, for the `VRUTIL_POWER_OF_TWO` flag macros.
- `progdefs_generated.hpp:186-188`: `vr_itemId`, `vr_wpntouch` entity fields (QC/progs inventory).

### Indirect hooks (no VR_ call, but they exist only for the VR module)
- `cl_input.cpp:69-71, 794-805`: `in_grableft/right`, `in_reloadleft/right`, `in_flickreloadleft/right` kbuttons, read by vr.cpp.
- `ReleaseFiles/Id1/config.cfg` binds: `k +grableft`, `l +grabright`, `n +reloadleft`, `m +reloadright`, `1/3/4/5` impulse 10/12/15/16, `7/8` impulse 42/43, `MOUSE1 +attack`, `MOUSE2 +offhandattack`, `SPACE +jump`. The VR input path breaks if these binds are missing.

---

## 4. Reverse dependencies (engine state the VR module reads or writes)

- `client_state_t cl`:
  - Writes: `viewangles`, `aimangles`, `handpos[2]`, `handrot[2]`, `prevhandrot[2]`, `visual_handrot[2]`, `handvel[2]`, `handthrowvel[2]`, `handvelmag[2]`, `handavel[2]`, `headvel`, `hotspot[2]`, `stats[STAT_VIEWHEIGHT]` (on disable).
  - Reads: `time`, `oldtime`, `entities`, `viewentity`, `viewent`, `offhand_viewent`, `mainhand_wpn_button`, `offhand_wpn_button` (`.hidden`, `.origin`), `intermission`, `stats[STAT_{MAIN,OFF}HAND_WID, STAT_WEAPONCLIP(2), STAT_WEAPONCLIPSIZE(2)]`.
  - Writes client entity `player.origin` on teleport.
- `usercmd_t` (VR_Move writes): `vryaw`, `handpos`, `handrot`, `handvel`, `handthrowvel`, `handvelmag`, `handavel`, `offhandpos`, `offhandrot`, `offhandvel`, `offhandthrowvel`, `offhandvelmag`, `offhandavel`, `headvel`, `muzzlepos`, `offmuzzlepos`, `teleport_target`, `vrbits0`, `offhand_hotspot`, `mainhand_hotspot`, `roomscalemove`, `forwardmove`, `sidemove`, `upmove`.
- `r_refdef`: writes `fov_x`, `fov_y`, `viewangles`, `aimangles` (a new field); reads `vieworg`.
- Renderer globals: `glx`, `gly`, `glwidth`, `glheight` (overwritten temporarily); `vid.conwidth/conheight`; `gl_farclip`; `GLSLGamma_GammaCorrect(int)`; `R_SetupView` (extern'd, unused).
- 2D/UI: `SCR_UpdateScreenContent`, `SCR_DrawNotifyString`, `SCR_DrawLoading`, `SCR_CheckDrawCenterString`, `SCR_DrawRam/Net/Turtle/Pause/DevStats/FPS/Clock/Console`, `scr_drawdialog`, `scr_drawloading`, `con_forcedup`, `Draw_ConsoleBackground`, `Draw_FadeScreen`, `Sbar_Draw`, `Sbar_IntermissionOverlay`, `Sbar_FinaleOverlay`, `M_Draw`, `M_DrawKeyboard`, `key_dest` (`key_menu`/`key_console`/`key_game`), `Key_Event`.
- Input: `in_grableft/right`, `in_reloadleft/right`, `in_flickreloadleft/right`, `in_speed`; `cl_forwardspeed`, `cl_upspeed`, `cl_movespeedkey`, `cl_alwaysrun`; `host_frametime`; SDL `SDL_GetMouseState`, `SDL_GL_SetSwapInterval`.
- Server: `svs.clients[0].{active,spawned,edict}`, `sv.active`, `sv.state`, `cls.signon`; `SV_Move(start,mins,maxs,end,MOVE_NORMAL,passedict)`; `trace_t`; edict fields `v.touchinghand`, `v.handtouch_hand`, `v.handtouch_ent`; `EDICT_TO_PROG`; `isDedicated`.
- Models: `Mod_ForName`, `Mod_Extradata`, `aliashdr_t::{scale, scale_origin, original_scale, original_scale_origin, numverts}`, `qmodel_t::{name,flags}`, `EF_GRENADE`, `EF_ROTATE`; alias frame internals `R_SetupAliasFrameZero`, `R_SetupAliasFrame`, `getDrawAliasFrameData`, `getFinalVertexPosLerped/NonLerped`, `lerpdata_t`, `entity_t::zeroBlend` (these come from the r_alias zero-blend changes).
- Math: `CreateRotMat`, `RotMatFromAngleVector`, `R_ConcatRotations`, `AngleVectorFromRotMat`, `VectorAngles`, `TurnVector`, `AngleVectorsOnlyFwd`, `safeNormalize`, `DotProduct`.
- Effects: `R_RunParticle2Effect`.
- Filesystem and cvars: `Cvar_RegisterVariable`, `Cvar_SetCallback`, `Cvar_SetQuick/SetValueQuick/Set/SetValue/FindVar`, `Cbuf_AddText`, `COM_CheckParm`, `COM_SkipPath`, `com_gamedir`, `pack_t::{filename,numfiles,files[].name}`, `Con_Printf`.
- Weapon IDs and stats: `WID_FIST`, `WID_SUPER_SHOTGUN`, `STAT_*` from the stats/QC inventory; `QVR_HS_*` and `QVR_VRBITS0_*` (quakedef_macros.hpp:286-312).

---

## 5. VR cvars

All are defined in `vr_cvars.cpp` (line numbers given). The six in the first block are not archived; everything else is `CVAR_ARCHIVE`. "QC" means the cvar is also read by QuakeC through `cvar_hmake`/`cvar_hget` (#86, QC `vr_cvars.qc:35-70`).

| Cvar | Default | Line | Meaning |
|---|---|---|---|
| vr_enabled | 0 (forced to 1 at init) | 34 | VR master switch. The callback disables and re-enables OpenVR. QC. |
| vr_viewkick | 0 | 35 | Allow damage/gun view kick in VR |
| vr_lefthanded | 0 | 36 | Swap controller roles |
| vr_fakevr | 0 | 37 | Pancake/debug mode (mouse aims, synthetic hands). QC. |
| vr_novrinit | 0 | 38 | Do not initialise OpenVR (with fakevr) |
| vr_fakevr_handroll | 0 | 39 | Hand roll in fake VR |
| vr_crosshair | 1 | 48 | 0 none, 1 point, 2 line, 3 smooth line |
| vr_crosshair_depth | 0 | 49 | Fixed depth (0 = trace) |
| vr_crosshair_size | 3.0 | 50 | Point size / line width |
| vr_crosshair_alpha | 0.25 | 51 | |
| vr_aimmode | 6 | 52 | `VrAimMode` (6 = controller) |
| vr_deadzone | 30 | 53 | Blended-aim yaw deadzone (clamped 0..70 by the callback) |
| vr_gunangle | 32 | 54 | Main-hand pitch offset of the controller to the gun |
| vr_gunmodelpitch | 0 | 55 | Added to the weapon pitch offset |
| vr_gunmodelscale | 1.0 | 56 | Global weapon model scale |
| vr_gunmodely | 0 | 57 | Added to the weapon Z offset |
| vr_crosshairy | 0 | 58 | Crosshair vertical bias |
| vr_world_scale | 1.0 | 59 | Metres to units multiplier (also scales the models) |
| vr_floor_offset | -16 | 60 | Z offset of the tracking floor relative to the player origin |
| vr_snap_turn | 0 | 61 | Snap angle in degrees (0 = smooth) |
| vr_enable_joystick_turn | 1 | 62 | |
| vr_turn_speed | 1 | 63 | Smooth turn speed |
| vr_msaa | 4 | 64 | Eye FBO MSAA samples |
| vr_movement_mode | 0 | 65 | 0 follow off-hand, 1 raw input |
| vr_hud_scale | 0.025 | 66 | Status bar scale |
| vr_menu_scale | 0.13 | 67 | 2D menu quad scale |
| vr_melee_threshold | 7 | 68 | Hand velocity magnitude that counts as melee. QC. |
| vr_gunyaw | 0 | 69 | Main-hand yaw offset |
| vr_gun_z_offset | 0 | 70 | Hand Z offset |
| vr_sbar_mode | 0 | 71 | HUD on main hand / off-hand |
| vr_sbar_offset_{x,y,z,pitch,yaw,roll} | 0 | 72-77 | Off-hand HUD placement |
| vr_roomscale_jump | 1 | 78 | Jump when the head moves up quickly |
| vr_height_calibration | 1.6 | 79 | Standing head height (m) |
| vr_roomscale_jump_threshold | 1.0 | 80 | Head vertical speed (m/s) |
| vr_menu_distance | 76 | 81 | Menu distance (units) |
| vr_melee_dmg_multiplier | 1.0 | 82 | QC |
| vr_melee_range_multiplier | 1.0 | 83 | QC |
| vr_body_interactions | 0 | 84 | Body touch picks up items (world.cpp). QC. |
| vr_roomscale_move_mult | 1.0 | 85 | Scale of physical movement |
| vr_teleport_enabled | 1 | 86 | |
| vr_teleport_range | 400 | 87 | |
| vr_2h_mode | 2 | 88 | 0 off, 1 basic, 2 virtual stock |
| vr_2h_angle_threshold | 0.65 | 89 | Dot threshold for 2H |
| vr_virtual_stock_thresh | 10 | 90 | Shoulder distance for the stock |
| vr_show_virtual_stock | 0 | 91 | Debug (hand selection 0-3) |
| vr_shoulder_offset_{x,y,z} | -1.5, 1.75, 16.0 | 92-94 | Shoulder anchor |
| vr_2h_virtual_stock_factor | 0.5 | 95 | Hand/shoulder mix |
| vr_wpn_pos_weight | 1 | 96 | Enable positional weight lag |
| vr_wpn_pos_weight_offset / _mult / _2h_help_offset / _2h_help_mult | 0, 1, 0.3, 1 | 97-100 | |
| vr_wpn_dir_weight | 1 | 101 | Enable rotational weight lag |
| vr_wpn_dir_weight_offset / _mult / _2h_help_offset / _2h_help_mult | 0, 1, 0.3, 1 | 102-105 | |
| vr_offhandpitch / vr_offhandyaw | 0, 0 | 106-107 | Off-hand angle offsets |
| vr_show_hip_holsters | 0 | 108 | Debug |
| vr_hip_offset_{x,y,z} | -1, 7, 4.5 | 109-111 | |
| vr_hip_holster_thresh | 6.0 | 112 | |
| vr_show_shoulder_holsters | 0 | 113 | |
| vr_shoulder_holster_offset_{x,y,z} | 5, 1.5, 0 | 114-116 | |
| vr_shoulder_holster_thresh | 8.0 | 117 | |
| vr_show_upper_holsters | 0 | 118 | |
| vr_upper_holster_offset_{x,y,z} | 2.5, 6.5, 2.5 | 119-121 | |
| vr_upper_holster_thresh | 6.0 | 122 | |
| vr_vrtorso_debuglines_enabled | 0 | 123 | |
| vr_vrtorso_enabled | 1 | 124 | Draw the torso model |
| vr_vrtorso_{x,y,z}_offset | -3.25, 0, -21 | 125-127 | |
| vr_vrtorso_head_z_mult | 32 | 128 | |
| vr_vrtorso_{x,y,z}_scale | 1, 1, 1 | 129-131 | |
| vr_vrtorso_{pitch,yaw,roll} | 0 | 132-134 | |
| vr_holster_haptics | 1 | 135 | `VrHolsterHaptics`. QC. |
| vr_player_shadows | 2 | 136 | `VrPlayerShadows` |
| vr_positional_damage | 1 | 137 | Headshots etc. QC. |
| vr_debug_print_handvel / _headvel | 0 | 138-139 | |
| vr_debug_show_hand_pos_and_rot | 0 | 140 | |
| vr_leg_holster_model_enabled | 1 | 141 | |
| vr_leg_holster_model_scale | 1 | 142 | |
| vr_leg_holster_model_{x,y,z}_offset | 0 | 143-145 | |
| vr_holster_mode | 0 | 146 | `VrHolsterMode`. QC. |
| vr_weapon_throw_mode | 0 | 147 | QC |
| vr_weapon_throw_damage_mult | 1.0 | 148 | QC |
| vr_weapon_throw_velocity_mult | 1.0 | 149 | QC |
| vr_weapon_cycle_mode | 0 | 150 | QC |
| vr_melee_bloodlust | 0 | 151 | QC |
| vr_melee_bloodlust_mult | 1.0 | 152 | QC |
| vr_enemy_drops | 0 | 153 | QC |
| vr_enemy_drops_chance_mult | 1.0 | 154 | QC |
| vr_ammobox_drops | 0 | 155 | QC |
| vr_ammobox_drops_chance_mult | 1.0 | 156 | QC |
| vr_menumode | 0 | 157 | `VrMenuMode` |
| vr_forcegrab_powermult | 0.75 | 158 | QC |
| vr_forcegrab_mode | 1 | 159 | QC |
| vr_forcegrab_range | 150 | 160 | QC |
| vr_forcegrab_radius | 18 | 161 | QC |
| vr_forcegrab_eligible_particles | 1 | 162 | QC |
| vr_forcegrab_eligible_haptics | 1 | 163 | QC |
| vr_weapondrop_particles | 1 | 164 | QC |
| vr_2h_spread_reduction | 0.5 | 165 | QC |
| vr_2h_throw_velocity_mult | 1.4 | 166 | QC |
| vr_headbutt_velocity_threshold | 2.02 | 168 | QC |
| vr_headbutt_damage_mult | 32 | 170 | QC |
| vr_activestartpaknameidx | 0 | 171 | Which pak's start.bsp to use |
| vr_verbosebots | 0 | 172 | QC (frikbot) |
| vr_finger_grip_bias | 0.0 | 173 | Added to the finger curl |
| vr_2h_disable_angle_threshold | 0 | 174 | |
| vr_autosave_seconds | 240 | 175 | saveutil |
| vr_autosave_on_changelevel | 1 | 176 | saveutil |
| vr_throw_up_center_of_mass | 0.1 | 177 | CrossAngVel algorithm |
| vr_throw_avg_frames | 15 | 178 | Velocity history size (applied by the menu action) |
| vr_throw_angvel_avg_frames | 5 | 179 | |
| vr_forcegrabbable_ammo_boxes | 1 | 180 | QC |
| vr_forcegrabbable_health_boxes | 1 | 181 | QC |
| vr_forcegrabbable_return_time_deathmatch | 4 | 182 | QC |
| vr_forcegrabbable_return_time_singleplayer | 0 | 183 | QC |
| vr_finger_auto_close_thumb | 1 | 184 | |
| vr_autosave_show_message | 0 | 185 | |
| vr_finger_blending | 1 | 186 | |
| vr_finger_blending_speed | 50 | 187 | |
| vr_menu_mouse_pointer_hand | 1 | 188 | 1 main hand first |
| vr_reload_mode | 2 | 189 | `VrReloadMode`. QC and engine. |
| vr_show_weapon_text | 1 | 190 | Ammo text on weapons |
| vr_disablehaptics | 0 | 191 | |
| vr_spinreload_pitch_speed | 1100 | 192 | Spin animation speed (deg/s) |
| vr_spinreload_x_angular_threshold | 6.5 | 193 | rad/s |
| vr_throw_algorithm | 0 | 194 | `VrThrowAlgorithm` |
| vr_player_stepsize | 18.0 | 195 | Replaces STEPSIZE in `SV_WalkMove` |
| vr_fingers_and_base_{x,y,z} | 0 | 205-207 | Finger model offsets (view.cpp) |
| vr_fingers_and_base_offhand_{x,y,z} | 0 | 210-212 | |
| vr_fingers_{x,y,z} | 0 | 215-217 | |
| vr_finger_{thumb,index,middle,ring,pinky,base}_{x,y,z} | 0 | 220-247 | |

There are also 32 × 69 dynamic weapon cvars (`vr_wofs_*_01..32`, vr.cpp:947-1024, archived), described in 1.9.

**Removed/overridden upstream behaviour:** the `host_maxfps` cap is ignored in VR, and vsync is forced off (`SDL_GL_SetSwapInterval(0)`).

---

## 6. OpenVR-specific vs backend-agnostic parts (OpenXR plan)

Backend-agnostic, and can stay in a portable `vr.c` / `vr_game.c`:
- Weapon cvar table, model mods, anchor-vertex math
- Hand collision and positional/rotational weight
- Throw velocity
- 2H aiming, holsters, body yaw, crouch
- Teleport, hotspots, flick reload
- The whole `VR_Move` usercmd filling, apart from where the raw input comes from
- Menu pointer ray math
- 2D-in-3D placement math
- showfn geometry (the GL calls themselves must be rewritten for core profile)
- PAK helpers
- Fake VR mode

OpenVR-specific parts, which belong behind a backend interface. Suggested interface: `vr_backend_init/shutdown`, `begin_frame(&predicted poses)`, `get_eye(i, &fov, &pose, &rt size)`, `acquire/submit eye image`, `poll_actions(&struct)`, `haptic(hand, …)`, `get_finger_curls(hand, float[5])`.

| OpenVR feature (vr.cpp line) | Use | OpenXR equivalent |
|---|---|---|
| `VR_Init(VRApplication_Scene)` 1447, `VR_Shutdown` 1533 | Runtime/session | `xrCreateInstance` (+`XR_KHR_opengl_enable`), `xrGetSystem(HEAD_MOUNTED_DISPLAY)`, `xrCreateSession` with `XrGraphicsBindingOpenGLWin32KHR`, `xrBeginSession` on `READY`, and an event loop via `xrPollEvent` (session state, which OpenVR does not need) |
| `GetRecommendedRenderTargetSize` 1483/1558 | Eye RT size | `xrEnumerateViewConfigurationViews(PRIMARY_STEREO).recommendedImageRectWidth/Height` |
| Own GL FBO textures plus `Submit(eye, Texture_t{GL tex, Gamma})` 1618-1621 | Frame submission | `xrCreateSwapchain` (GL_SRGB8_ALPHA8 or RGBA8), `xrAcquire/Wait/ReleaseSwapchainImage`, render into the swapchain image (or blit from the MSAA FBO), then `xrEndFrame` with an `XrCompositionLayerProjection` of 2 views. The colour-space difference (OpenVR `ColorSpace_Gamma` vs sRGB swapchain) affects gamma. |
| `WaitGetPoses` 2659 (blocking frame pacing) | Poses and pacing | `xrWaitFrame` → `predictedDisplayTime`, `xrBeginFrame`, `xrLocateViews` (eye poses and FOV) plus `xrLocateSpace` for the hand/grip/aim spaces |
| `SetTrackingSpace(TrackingUniverseStanding)` 1499 | Floor-origin space | `xrCreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_STAGE)`, or `LOCAL_FLOOR` (`XR_EXT_local_floor`), falling back to LOCAL plus a height offset |
| `GetProjectionRaw` 1486 (FOV degrees) / `GetProjectionMatrix(eye, 4, farclip)` 3546 | FOV and projection | `XrView.fov` (angleLeft/Right/Up/Down). Build an asymmetric frustum matrix yourself; this fits IW's `GL_FrustumMatrix`/`r_matproj` path well. |
| `GetEyeToHeadTransform` 2714 plus head pose 2680-2729 | Per-eye position (IPD) | `XrView.pose` per eye directly, in the reference space. The head is `xrLocateSpace(VIEW space)`. |
| `GetTrackedDeviceClass`, `GetControllerRoleForTrackedDeviceIndex` 2675/2747 | Controller discovery and handedness | Subaction paths `/user/hand/left` and `/user/hand/right` on a pose action, which removes device enumeration. Keep `vr_lefthanded` as an index swap. |
| `TrackedDevicePose_t.vVelocity/vAngularVelocity` 2740-2743, HMD `vVelocity` 2678 | Throw/melee/headbutt/roomscale jump | `XrSpaceVelocity` chained into `xrLocateSpace` (check `XR_SPACE_VELOCITY_*_VALID_BIT`). The frame of the angular velocity may differ (OpenVR gives it in tracking space). |
| `GetControllerState` 2766 | Dead | Drop |
| `IVRInput::SetActionManifestPath` + `actions.json` 1464 | Action definitions | `xrCreateActionSet` ("default", "menu"), `xrCreateAction` per action. Also `xrSuggestInteractionProfileBindings` per profile (`/interaction_profiles/valve/index_controller`, `oculus/touch_controller`, `htc/vive_controller`, `microsoft/motion_controller`, `khr/simple_controller`, `htc/vive_cosmos_controller`), replacing `bindings_*.json`, then `xrAttachSessionActionSets` |
| `GetActionSetHandle` / `GetActionHandle` 1255-1368 | Handles | `XrActionSet` / `XrAction` objects |
| `UpdateActionState(1 set)` 4063, switching the active set | Poll | `xrSyncActions`. OpenXR can sync both sets with priority, but to match the behaviour sync only the current set. |
| `GetDigitalActionData` (bState, bChanged, activeOrigin) | Buttons | `xrGetActionStateBoolean` (currentState, changedSinceLastSync). For the origin, use per-subaction queries. |
| `GetAnalogActionData` (x, y, deltaX/Y) | Sticks | `xrGetActionStateVector2f`. There is no delta, so keep the last value yourself (the menu navigation edge detection uses deltaY). |
| Skeleton actions plus `GetSkeletalSummaryData(FromDevice).flFingerCurl[5]` 4107-4150 | Finger curls | No direct equivalent. Options: `XR_EXT_hand_tracking` joint curls (compute from joints); `XR_FB_hand_tracking_aim`; or, most portable, derive curls from float actions (`/input/trigger/value`, `/input/squeeze/value`, the Index `/input/{index,middle,ring,pinky}/curl` via `XR_VALVE_index_controller`-style paths if exposed, `thumb` touch). Keep `vr_finger_auto_close_thumb`. |
| `TriggerHapticVibrationAction(action, delay, duration, freq, amp, origin)` 3925/4293 | Haptics | `xrApplyHapticFeedback` with `XrHapticVibration{duration ns, frequency, amplitude}`. There is **no delay parameter**, so schedule it yourself (QC passes a delay). |
| `GetInputSourceHandle(/user/hand/*)` 1366 | Unused | Subaction paths |
| `VR_GetVRInitErrorAsEnglishDescription` | Errors | `xrResultToString` |
| `vr::VRSkeletalSummaryData_t` in `vr.hpp` externs | Type leak | Replace with `float vr_finger_curl[2][5]` |

Frame structure change for OpenXR: `xrWaitFrame`/`xrBeginFrame` must bracket rendering, and pose prediction uses the display time. The QVR split (`VR_Move` in `CL_SendCmd` using last frame's poses, rendering in `SCR_UpdateScreen` after the pose wait) maps to `xrWaitFrame` + `xrSyncActions` + locate at the start of `SCR_UpdateScreen`, then `xrEndFrame` after both eyes. Session-state handling (focus loss or visibility changes pause input) is new work with no QVR equivalent.

---

## 7. SteamVR input manifests (`C:\OHWorkspace\quakevr\ReleaseFiles`)

- `actions.json`:
  - Action sets `/actions/default` and `/actions/menu`, both with usage `leftright`.
  - 36 actions (listed in 1.5). The skeleton actions `LeftHandAnim`/`RightHandAnim` are "suggested" and bound to `/skeleton/hand/left|right`.
  - Default bindings are listed for `knuckles`, `oculus_touch`, `generic`, `vive_cosmos_controller`, `vive_controller` and `holographic_controller`.
  - Has en_US localisation.
- Default layout (knuckles/vive/cosmos/generic are the same):

| Action | Binding |
|---|---|
| Locomotion | L stick |
| Turn | R stick |
| Speed | L stick click |
| FireMainHand | R trigger |
| FireOffHand | L trigger |
| Jump | R A |
| NextWeaponMainHand | R B |
| NextWeaponOffHand | L B |
| Escape | L A |
| Teleport | L trackpad touch |
| Grab | grip (grab mode) |
| Haptics | per hand |
| Skeleton | per hand |

- Menu set: navigation on the L stick; L B / R B are left/right; R A enter; L A back; multiplier-half on the L trackpad click; plus-one on the L trigger; plus-one-2 on the R trigger.
- Touch: Escape is L X, off-hand next weapon is L Y, grab is the grip trigger click, and there is no teleport binding.
- Holographic (WMR): uses `joystick`, `trackpad` click for jump/up/down, and `application_menu` for escape/back.
- Issues to fix when writing OpenXR suggested bindings:
  - `bindings_cosmos.json` declares `controller_type: vive_controller` (copy-paste bug).
  - The Vive/Cosmos/generic files reference `thumbstick`/`a`/`b`, which Vive wands do not have.
  - `LeftReload`/`RightReload`, `PrevWeapon*`, the `BMove*/BTurn*` actions and `AddToShortcuts` are unbound in every profile.
  - `vr_autoexec.cfg` is executed but not shipped in ReleaseFiles.

---

## Dependencies on other subsystems

- **Protocol / usercmd:** the VR `usercmd_t` fields, `clc_move` encoding, `vrbits0`, hotspots, `roomscalemove`, `teleport_target`. `cmd.vryaw` is sent to the server.
- **Client state (`client.hpp`):** the `cl.hand*` arrays, `visual_handrot`, `headvel`, `hotspot`, and extra view entities (`offhand_viewent`, `*_wpn_button`, `*_wpn_text`, holster slots, hand/finger entities, `vrtorso`), plus `STAT_*` additions.
- **View/render (`view.cpp`, `gl_rmain.cpp`, `r_alias.cpp`):** `V_CalcRefdef` with arguments, hand/finger/torso/holster view entities, zero-blend alias frames (`entity_t::zeroBlend`, `getDrawAliasFrameData`), `R_DrawString`/world text, `aliashdr_t::original_scale*`.
- **Server physics (`sv_phys.cpp`, `world.cpp`):** `SV_Handtouch`, `SV_VRWpntouch`, hand-touch box enlargement, the new edict fields `handtouch*`, `touchinghand`, `vr_wpntouch`, `vr_itemId`.
- **QC builtins:** #81 `haptic` and #86 `cvar_hmake`/`cvar_hget`, plus the other VR builtins.
- **Menu (`menu.cpp`, `menu_keyboard`):** the VR option pages, the weapon dev tool and the virtual keyboard.
- **Input (`cl_input.cpp`):** the `+grab*`/`+reload*`/`+flickreload*`/`+offhandattack` buttons.
- **Filesystem (`common.cpp`):** multi-start.bsp paks, `vrstart` map. **Saves (`saveutil.cpp`):** autosave.

## Ironwail porting risks and notes

1. **Core-profile GL:** every fixed-function call in `VR_SetMatrices`, `VR_Draw2D`, `VR_DrawSbar`, `vr_showfn.cpp` and `gl_util` must be replaced. The recommended approach:
   - Inject the eye projection and view into `R_SetFrustum`/`r_matproj`/`r_matview` (gl_rmain.c:829-867).
   - Render the 2D layer (IW `GL_Set2D` + canvas batching) into an offscreen 320×200-ish texture once per frame, then draw it as a textured world-space quad per eye. Alternatively use an OpenXR quad layer (`XrCompositionLayerQuad`), which is ideal for the menu and HUD.
   - Rewrite showfn with IW's debug-line facility or a small VBO line shader.
2. **Frame structure:** IW has `framebufs.scene`/`composite`/OIT FBOs, `GL_PostProcess`, `r_scale` and `R_WarpScaleView`. Per-eye rendering must reuse these, either by pointing the final composite at the eye swapchain image or by blitting. `glwidth/glheight` overriding may conflict with `vid.maxscale` and `r_refdef.scale`.
3. **Static SCR helpers in IW:** `VR_Draw2D` duplicates the 2D list. It is better to reuse IW's list by rendering it to a texture than to un-static the helpers.
4. **Server traces from the client** (`SV_Move` in `SetHandPos`, `TraceLine` in the crosshair) need `PR_SwitchQCVM(&sv.qcvm)` and a local server. This does not work for remote clients (see the TODOs about MP). IW keeps the same qcvm model, so the approach can be kept but should be guarded with `sv.active`.
5. **IW frame pacing** (`Host_GetFrameInterval`, host.c:776): it must return 0 in VR, and vsync must be off.
6. **Builtin #81 clash** with IW `stof`. Also #80.
7. `vr_enabled` defaults to 0 but is forced to 1. For the port, prefer a single `vr_enabled` plus a `-vr` flag and make fake VR explicit.
8. `VR_InitGame` overwriting the user's `vr_wofs_*` on a game change is probably a bug, or intentional because config.cfg is re-executed afterwards.

## Open questions

- The mirror blit (vr.cpp:3537) passes `srcX0=0, srcY0=eyeW, srcX1=eyeH, srcY1=0 → dst 0,h,w,0`. That looks like a transposed/flipped bug, which matches the TODO "desktop display has a bit of it cut off". Should the port simply blit the eye with the correct aspect?
- Head X/Z are zeroed for the eye position and fed back as `roomscalemove`, so the player entity follows the HMD. Should that be kept under OpenXR, where STAGE space makes 6DoF straightforward? It is required for server-side collision.
- `current_eye` is static, but its comment claims it is used in view.cpp/gl_rmain.cpp. It is only used via `VR_SetMatrices`/`VR_AddOrientationToViewAngles`. Also, `VR_AddOrientationToViewAngles` dereferences `current_eye` in `V_CalcIntermissionRefdef`, which is null before the first eye render.
- The `headOrigin -= lastHeadOrigin` sequence makes `headOrigin` always zero. Controller X/Y are relative to `lastHeadOrigin` and Z is absolute. Confirm this is intended before re-deriving the math for OpenXR spaces.
- Aim modes 0-5 and `VrMovementMode::e_RAW_INPUT` look unmaintained. Could they be dropped in the port?
- Is `SV_WriteClientdataToMessage`'s `#if 0` hand-collision block a planned MP fix? It is ignored here.
