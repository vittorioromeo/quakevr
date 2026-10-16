# spacewarp-ref

A small standalone OpenXR + Vulkan reference app for the Steam Frame. It submits analytically exact motion vectors and
depth through `XR_EXT_frame_synthesis` or `XR_FB_space_warp`, with a controllable GPU load, so that SteamVR's frame
synthesis can be A/B tested against a known-good app (for example to check whether a problem is in the Quake port).

- Source: this folder. On the Frame it lives in `~/Games/spacewarp-ref/`.
- Binary: `~/Games/spacewarp-ref/spacewarp-ref`.
- Stack: C++17, `XR_KHR_vulkan_enable2`, Vulkan 1.1, one stereo projection layer, SteamVR OpenXR runtime.

## Build (on the Frame)

```
cd ~/Games/spacewarp-ref && ./build.sh
```

This needs cmake, g++, glslc, and the Vulkan and OpenXR headers, all of which are already on the Frame. It links
SteamVR's `libopenxr_loader.so` from `/opt/steamvr/bin/linuxarm64` through an RPATH. Shaders are compiled to
`build/shaders/*.inc` and embedded in the binary. If `XR_RUNTIME_JSON` is unset, the app sets it to
`/opt/steamvr/steamxr_linuxarm64.json`.

## Command line

```
spacewarp-ref [options]
  --api ext|fb|none          synthesis API (default ext = XR_EXT_frame_synthesis)
  --seconds S                run time after the session first becomes FOCUSED (default 15)
  --warmup-seconds W         first W of those seconds excluded from the summary (default 3)
  --load-ms N                auto-calibrate artificial GPU load to ~N ms app GPU time/frame (default 0)
  --load-iters K             fixed load: K loop iterations per pixel per eye (overrides --load-ms)
  --scale F                  colour eye size = recommended * F (default 1.0, 0.1..2)
  --motion-res-scale F       motion/depth size = recommended motion size * F (default 1.0)
  --mv-2d                    EXT: set USE_2D_MOTION_VECTOR bit (z written as 0)
  --mv-camera on|off         include head/eye motion in the vectors (default on)
  --mv-fov prev|cur          previous-frame projection: last frame's FOV (prev, default) or this frame's (cur)
  --mv-gain G                multiply submitted vectors by G (default 1 = exact)
  --relaxed                  EXT: set REQUEST_RELAXED_FRAME_INTERVAL bit
  --reversed-z               reversed depth (near=1, far=0; nearZ > farZ in the submission)
  --near M --far M           clip distances in metres (default 0.05 / 100)
  --synthesis-skip-every N   every Nth frame: EXT omits the struct, FB sets FRAME_SKIP
  --srgb-motion              force the sRGB8 motion encoding (Quake's experimental path)
  --motion-range R           sRGB8 encoding positive range in NDC (default 0.125)
  --dump-motion PREFIX       write eye-0 motion/depth + analysis after --dump-after seconds
  --dump-after S             (default 3, seconds after FOCUSED)
  --trace-views N            log the raw xrLocateViews result (full precision) for the first N rendered frames
  --space stage|local        app reference space (default stage, falls back to local)
  --focus-timeout S          give up (exit 4) if not FOCUSED within S seconds (default 30)
  --log FILE                 per-second log (default ./spacewarp-ref.log)
  --summary FILE             JSON summary (default ./spacewarp-ref-summary.json)
```

Exit codes:

| Code | Meaning |
|---|---|
| 0 | Completed. |
| 2 | Usage error. |
| 3 | A required extension is missing (`XR_KHR_vulkan_enable2`, `XR_EXT_frame_synthesis` for `ext`, `XR_FB_space_warp` for `fb`), or `xrEndFrame` rejected a frame carrying the synthesis struct. |
| 4 | Runtime failure, including a focus timeout and session loss. |

Start long runs detached and poll for the result, as below. Only run one instance at a time.

```
cd ~/Games/spacewarp-ref/runs && (setsid nohup sh -c "../spacewarp-ref --api ext --seconds 10 --log a.log --summary a.json > a.out 2>&1; echo \$? > a.done" > /dev/null 2>&1 < /dev/null &)
```

Use `--warmup-seconds 6 --seconds 12` or longer for clean numbers; see the standby notes below for why.

## Scene and space

The app space is **STAGE**, or LOCAL if STAGE is not offered or `--space local` is given. The world is anchored once,
at the first rendered frame:

- The origin is on the floor below the midpoint of the two eye poses.
- -Z points along the initial view yaw.
- The floor is STAGE y = 0 when the eye height is between 0.8 and 2.4 m; otherwise it is 1.6 m below the eyes.

All sizes are in metres.

| Object | Placement and behaviour |
|---|---|
| Floor | Checkerboard at y = 0, x -10..10, z +5..-15. |
| Back wall | Checkerboard at z = -5, x -6..6, y 0..4. |
| Pillars | Four static pillars, 0.3 x 3 x 0.3, at (+-2, z -3) and (+-1.3, z -4.3). |
| Moving cube | Side 0.3 m, centre at eye height, 1.5 m ahead. It moves along x at a constant 0.6 m/s, as a triangle wave of amplitude 0.6 m and period 4 s, turning around at x = +-0.6. |
| Rotating cube | Side 0.3 m, at (0, eye height - 0.45, -1.5). It rotates about the vertical axis at 90 deg/s. |
| Sky | Not geometry. It is the colour-pass background and is reprojected with rotation only. |

Animation time comes from `predictedDisplayTime`, measured from the first rendered frame. It is never taken from the
CPU clock.

## Motion vectors and depth

**Definition.** Each pixel stores `mv = NDC_current - NDC_previous` as `(dx, dy, dz)`.

- The NDC convention is Vulkan's, as in `xr_linear.h`: +Y is down and depth runs 0..1.
- `NDC_previous` is the same surface point at the previously rendered frame's display time, projected with that eye's
  previous view-projection.
- The motion pass interpolates current and previous clip positions per vertex and divides per pixel, so values are
  exact (perspective correct) everywhere.
- The sky is rotation-only: `mv = ndc - project(prevVP_rot * inverse(curVP_rot) * ndc)`, with z = 0.
- `--mv-2d` (EXT) writes z = 0 and sets the 2D flag. `--mv-gain` scales the vectors.

**Previous-frame matrices.** These are handled per eye.

- After every frame that `xrEndFrame` accepts, the app keeps `P*V` and the rotation-only `P*R` that it actually
  rendered with, plus V. It also keeps that frame's animation time.
- Previous model matrices are rebuilt from the animation at that time.
- Previous means the previous **submitted** frame, so the vectors span exactly one app frame. That is 13.9 ms at 72 Hz
  and 27.8 ms when SteamVR runs the app at half rate.
- The first frame carries no synthesis struct.
- `--mv-camera off` uses the current view-projection as the previous one, so only object motion remains.
- `--mv-fov cur` keeps the previous pose but uses the current FOV. This removes the runtime's FOV jitter; see the
  findings below.

**Formats.** These are the formats chosen on the Frame. The selection order is the same as Quake's.

| Swapchain | Format | Notes |
|---|---|---|
| Colour | `R8G8B8A8_SRGB` | |
| Motion | `R16G16B16A16_SFLOAT` | Order: RGBA16F, then RG16F, then RGBA32F, then the sRGB8 encoding. RGBA = (dx, dy, dz, 1). |
| Depth | `D32_SFLOAT` | Order: D32, then D24S8, then D32S8, then D16. |

- All swapchains are 3 images.
- The motion and depth swapchains are sized by the EXT/FB recommended motion size, which is 1728x1728 on the Frame.
- For float motion formats, `motionVectorScale` is (1,1,1,1) and the offset is 0.
- `--srgb-motion` switches to Quake's sRGB8 encoding: `scale = range / (1 - 0.502886458)`,
  `offset = -0.502886458 * scale`, and the shader writes `(mv - offset) / scale`, clamped to 0..1.

**Depth.** Depth is the motion pass's own depth buffer, standard by default: 0 = near (0.05 m) and 1 = far (100 m).

- The struct gets `minDepth` 0, `maxDepth` 1, `nearZ` 0.05 and `farZ` 100.
- With `--reversed-z`, `nearZ` and `farZ` are swapped.
- `appSpaceDeltaPose` is always identity.

## GPU load and timing

**The load.** The load is a full-screen fragment pass per eye, drawn before the scene. It is a loop of dependent
`textureLod` fetches from a 256x256 noise texture plus a little ALU, split into draws of 16 iterations.

**Calibration with `--load-ms N`.**

1. Measure 20 frames with no load.
2. Probe 8 iterations.
3. Rescale the iteration count from the timestamp deltas. Each round uses 10 frames and the median of the last 8.
4. Stop when within 3% after 2 rounds, after 8 rounds, or after 2.5 s.

Calibration logs `CALIB` lines. On the Frame one iteration costs about 0.95-1.0 ms of GPU per frame in steady state
(both eyes, 1728x1728), so the load granularity is about 1 ms at scale 1.0.

The load is frozen after calibration. Because of GPU DVFS, compositor preemption and the standby blip during the first
seconds, the steady state can land up to about 12% off target. For example, one run calibrated 22 iterations at
20.0 ms and then ran at 22.4 ms. For reproducible A/B runs use `--load-iters K`.

**GPU time.** GPU ms comes from timestamp queries: the start of the command buffer, the end of the colour passes, and
the end of the motion passes. It covers the whole app frame on the GPU.

**Frame timing fields.**

| Field | Meaning |
|---|---|
| `frame_ms` | Interval between successive `xrWaitFrame` returns. |
| `wait_ms` | Time blocked in `xrWaitFrame`. |
| `cpu_busy_ms` | From the `xrWaitFrame` return to the `xrEndFrame` return. Includes `swapchain_wait_ms` (`xrWaitSwapchainImage`) and `end_frame_ms` (`xrEndFrame`). |
| `display_dt_ms` | Delta of `predictedDisplayTime`. |
| `period` | `predictedDisplayPeriod`. |

## Log format

The program prints the same lines to stdout and to `--log`. Every line starts with a tag:

| Tag | Content |
|---|---|
| `INFO` / `WARN` / `ERROR` | Setup and diagnostics: extensions offered/enabled, runtime and system, view sizes, swapchain formats, GPU, anchor, view-state changes, first eye poses/FOVs. |
| `STATE t=... A -> B` | Session state changes. `t` is seconds since process start. |
| `CALIB ...` | Load calibration rounds. |
| `SEC ...` | One line per second, described below. |
| `DUMP ...` | Motion dump analysis. |
| `VIEWTRACE ...` | Raw `xrLocateViews` output with `--trace-views`. |
| `SUMMARY ...` | One line at exit. |

Example `SEC` line:

```
SEC t=10.376 tf=9.337 state=FOCUSED frames=37 fps=36.00 frame_avg=27.778 frame_p95=27.844 frame_max=27.851
    wait_avg=27.565 cpu_avg=0.207 cpu_max=0.463 swapwait_avg=0.014 endframe_avg=0.045 gpu_avg=19.272 gpu_p95=19.323
    gpu_max=19.327 gpu_motion_avg=0.672 gpu_n=37 period=27.778 disp_dt_avg=27.778 disp_dt_max=27.803 rendered=37
    synth=37 iters=19 measuring=1
```

The real line is a single line. Its fields:

- `t` is seconds since process start; `tf` is seconds since the first FOCUSED.
- All times are in ms over the frames of that second.
- `gpu_*` covers the GPU results that arrived in that second, and `gpu_n` is how many there were.
- `rendered` and `synth` count frames that were rendered and frames that carried a synthesis struct.
- `iters` is the current load iteration count.
- `measuring=1` means the frame is inside the summary window.

## Summary JSON (`--summary`)

The summary covers the window `[first FOCUSED + warmup, first FOCUSED + seconds]`.

| Field | Content |
|---|---|
| `api`, `exit_reason`, `exit_code` | Run identity and outcome. `exit_reason` is one of `completed`, `signal`, `focus_timeout`, `xrEndFrame_rejected_XrFrameSynthesisInfoEXT` (or `..._XrCompositionLayerSpaceWarpInfoFB`), `runtime_requested_exit`, `session_loss_pending`, `instance_loss_pending`, or the name of a failed setup step. |
| `frames`, `seconds`, `fps` | Frame count, window length and frame rate. |
| `rendered_frames`, `synthesis_frames`, `unfocused_frames` | `unfocused_frames` counts window frames that were not FOCUSED. |
| `frame_ms`, `wait_ms`, `cpu_busy_ms`, `swapchain_wait_ms`, `end_frame_ms`, `gpu_ms`, `gpu_motion_ms`, `display_dt_ms` | Each is `{avg, p50, p95, p99, max, min, n}`. |
| `display_period_ms` | Average `predictedDisplayPeriod`. |
| `load_ms_target`, `load_iters`, `load_calibrated`, `load_base_gpu_ms` | Load settings and calibration result. |
| `scale`, `eye_w`, `eye_h`, `motion_w`, `motion_h` | Image sizes. |
| `color_format`, `motion_format`, `depth_format`, `motion_encoding` | Formats chosen. |
| `mv_fov`, `mv_2d`, `mv_camera`, `mv_gain`, `relaxed`, `reversed_z`, `near_z`, `far_z`, `synthesis_skip_every`, `space` | Motion and projection options in effect. |
| `focused_at_s`, `warmup_seconds`, `run_seconds`, `focus_lost`, `states[]` | Session timing and state history. |
| `rejected_end_frames`, `dump` | Rejected frames and whether a dump was taken. |
| `runtime`, `system`, `gpu` | Names reported by the runtime and the driver. |

## Motion dump (`--dump-motion PREFIX`)

**When it is taken.** The dump is the eye-0 motion and depth of one frame. The first frame that meets all of these is
used:

- at least `--dump-after` seconds after FOCUSED
- the moving cube is on a straight leg
- its front-face centre is inside the central 80% of the eye-0 image
- the camera moved less than 0.02 NDC since the previous frame

The last condition avoids the pose teleport when the headset enters or leaves standby. The frame is copied on the GPU
and analysed after the run, so it does not stall the frame loop.

**Output files.**

| File | Content |
|---|---|
| `PREFIX.motion.f32` | `w*h*3` little-endian float32 values: decoded NDC (dx, dy, dz). Row 0 is the top, which is NDC y = -1. |
| `PREFIX.depth.f32` | `w*h` float32 window depth. |
| `PREFIX.motion.ppm` | Visualisation: R/G = 128 + mv * 2540, B = depth. |
| `PREFIX.json` | The numbers below. |

**`DUMP` log lines.**

| Line | Content |
|---|---|
| `DUMP eye=0` | Frame dt, cube x and velocity, face depth, P00, and the pixel of the cube's front-face centre. |
| `DUMP moving_cube` | Expected values (`expected_full`, and `expected_simple`) against measured values (`measured_center` and the 9x9 median), plus measured vs expected depth. `expected_full` is the CPU double-precision projection of the face centre, current vs previous. `expected_simple` is a pinhole estimate `P00 * v * dt / depth` that ignores camera motion. |
| `DUMP static_wall` / `static_floor` | Fixed static points: expected vs measured. |
| `DUMP pixel_check class=static\|moving_cube\|rotating_cube\|sky` | A whole-image check. Every pixel is unprojected with its stored depth, assigned to the object it lies on, moved back with that object's previous model, and reprojected with the previous VP. The line reports the maximum and RMS error per component. |

## Verification results (2026-10-16)

`verify.sh` runs the whole sequence below, one run at a time, and writes to `runs/verify/`. The numbers come from the
final binary, using the default 3 s warmup, so the summary window still contains the standby blip.

| Run | FPS | GPU avg ms (app timestamps) | Compositor stats for the pid |
|---|---|---|---|
| `--api none --seconds 10` | 72.00 | 1.10 | 783 presents, 0 dropped, 0 reprojected; ApplicationTime GPU 3.09 ms |
| `--api ext --seconds 10` | 68.0 (p50 frame 13.90 ms) | 1.38 | 770 presents, 12 dropped, 121 reprojected; AppMVs log count +1 |
| `--api fb --seconds 10` | 68.0 | 1.36 | 772 presents, 11 dropped, 66 reprojected; AppMVs +1 |
| `--api ext --load-ms 20 --seconds 10` | 34.2 | 22.4 | 427 reprojected; ApplicationTime GPU 19.8 ms |

- Every run exited 0 with `focus_lost` 1.
- In the ext and fb runs, the 68 fps against 72 is the standby blip (80 unfocused and half-rate frames in the window).
  In the `none` run the same blip did not lower the frame rate.
- In the load run, calibration chose 22 iterations. SteamVR switched the app to half rate: `predictedDisplayPeriod`
  went to 27.78 ms and the steady state was 36.0 fps.

**Motion dump with `--api ext`, default `--mv-fov prev`.**

- The dump was taken at a focus age of 4.07 s with dt 13.888 ms. The cube was moving at +0.6 m/s, with its front face
  1.35 m from the eye and P00 = 0.69616.
- Expected (CPU, double precision): dx = +0.004876, dy = +0.000579. That is the object's 0.004297 plus the runtime's
  0.000579 FOV-jitter shift.
- Measured: (+0.004875, +0.000578). The x error is 6.5e-7 (0.013%), which is half-float quantisation.
- Expected depth 0.963445 matched measured depth 0.963445.
- In the whole-image check, the maximum absolute error is below 2e-6 for static, both cubes and sky. Static and sky
  show a uniform 1/1728 NDC per frame from the FOV jitter.

**The same dump with `--mv-fov cur`.**

- Expected 0.004297 = 0.6 m/s x 13.8875 ms x 0.69616 / 1.35 m, and measured 0.004295.
- Static and sky are exactly 0.

## Findings on the Frame (SteamVR/OpenXR 2.18.2, Turnip Adreno 750)

These were measured on 2026-10-16.

### Extensions and formats

- Both `XR_EXT_frame_synthesis` v1 and `XR_FB_space_warp` v2 are offered and accepted.
- Each run logs one `Using motion encoding AppMVs.` line in `~/.local/share/Steam/logs/vrcompositor.txt`.
- Recommended eye size is 1728x1728. The recommended motion size is 1728x1728 for both EXT and FB.

### Runtime FOV jitter

Even with synthesis off, SteamVR returns a different FOV every frame from `xrLocateViews`.

- It alternates between two FOVs, each about +-0.25 px from a centre FOV.
- It returns the centre FOV on the first frame and whenever it runs the app at half rate.
- Values seen for eye 0, in radians (L, R, U, D):
  - centre: (-1.02620733, 0.88493711, 0.86231160, -1.04904664)
  - first alternate: (-1.02609575, 0.88510382, 0.86213350, -1.04915106)
  - second alternate: (-1.02631891, 0.88477033, 0.86248958, -1.04894221)
- With exact `--mv-fov prev` vectors, a static scene therefore shows a uniform +-1/1728 NDC (half a pixel) per frame in
  x and y.
- `--mv-fov cur` removes it.
- Whether the runtime expects the jitter inside the app's vectors is unknown, so A/B both settings.
- The jitter is not caused by the implicit `XR_APILAYER_VALVE_fdm_injection` layer. It persists with
  `DISABLE_VULKAN_FDM_INJECTION_LAYER=1`.

### Standby

The headset is in standby when nobody wears it.

- SteamVR leaves standby when the app connects and re-enters standby about 5 s later. See `vrserver.txt`:
  `Set displays on` / `Set displays off`.
- This costs a FOCUSED -> VISIBLE -> FOCUSED blip from about 2.1 s to about 4.1 s after the first FOCUSED.
- During the blip, tracking switches to the real headset pose (`tracked=1/1`).
- With synthesis on, SteamVR runs the app at half rate for about 0.6 s after each transition.
- With the default warmup of 3 s these frames land in the window (`unfocused_frames` counts them). Use a warmup of at
  least 6 s for clean numbers.

### Standby view pose

In standby, `xrLocateSpace(VIEW, STAGE)` reports the head at y = 1.002, but `xrLocateViews` returns the eyes at
y = 0. The anchor uses the eye midpoint.

### Pacing under GPU load

- With a GPU load beyond one period, SteamVR switches the app to half rate: `predictedDisplayPeriod` becomes 27.78 ms
  and `xrWaitFrame` paces it.
- With an extreme load, for example a 125 ms GPU frame, the blocking moves into `xrEndFrame`.

### Implicit layer and compositor log

- The implicit `XR_APILAYER_VALVE_fdm_injection` layer prints `fdm_injection: XR layer loaded` on stdout.
- `vrcompositor.txt` logs `Failed to signal gpu fence` once per app connect.
