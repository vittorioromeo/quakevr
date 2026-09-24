# Throwing in Quake VR: current state, state of the art, recommendation

Repository: `C:\OHWorkspace\quakevr-iw`, branch `vr-ironwail`, HEAD `a7d92887`. Nothing in the repo was modified.
Conversions used below: `m2u = vr_world_scale / 0.0381` = 32.8 units/m at the default 1.25. Quake `sv_gravity 800`
is 24.4 m/s² at that scale, about 2.5 g.

---

## 1. How it works today, and what is likely wrong

### 1.1 Pipeline, from grip release to flight

1. **Pose and velocity source.** `vr_backend_openxr.cpp:155` locates every space at `frameState.predictedDisplayTime`.
   That time is 1–3 frames *in the future*, so the samples are predicted poses. `locate()` (`:559-581`) chains
   `XrSpaceVelocity`, so linear and angular velocity come from the runtime (IMU-fused, in world axes).
   With `vr_controller_legacy_pose 1` (the default), `toLegacyPose()` (`:520-556`) moves the hand about 10 cm forward
   of the grip pose (Touch: `{0.007, -0.0018, 0.102}`) and sets `v_raw = v_grip + ω × (p_raw − p_grip)`.
2. **History.** `vr_hands.cpp:134-146` converts the velocities to Quake axes (a proper rotation, so cross products stay
   valid) and turns them with the play-space yaw. Then `throwing::sample(h, realtime, vel, angVel, forward(rot))`
   stores one sample per host frame. The sample is timestamped with **`realtime`**, the host clock, but the pose it
   holds is for the predicted display time. `vr_throw.cpp:136` precomputes
   `objVel = vel + ω × (forward · vr_throw_lever_arm)`, where `forward` is the *aim* direction (including
   `vr_gunangle` 39.5°) and the lever arm is 0.1 m. Because the raw pose is already about 0.1 m ahead of the palm,
   the assumed object centre sits about **0.2 m from the palm**.
3. **Estimation (`vr_throw.cpp:93-124`, algorithm 2, the default).** Over the samples within `vr_throw_window`
   (0.1 s) of the *newest* sample, it finds the sample with the largest `|objVel|` (lever term included). It then
   averages `objVel` and `angVel` over samples within ±`vr_throw_peak_span` (±25 ms, so a 50 ms span) of that
   peak. This is almost exactly Valve's Half-Life: Alyx method (see §2.1).
4. **Transport.** `vr_client.cpp:134-138` calls `estimate()` on **every** move, not once at release, and sends
   `throwVel` and `angVel` with each move. The server copies the latest move's values into `.handthrowvel` and
   `.handavel` (`vr_server.cpp:128-141`). The grab bit is the runtime's **boolean** `squeeze` action
   (`vr_backend_openxr.cpp:828`, bound to `input/squeeze/value` at `:763`). That bool goes through the
   `RSHOULDER` key and `+grabmain` into `handButtons().grab`. The float `gripValue` is read (`:914`) but not used
   for releasing.
5. **QC (`QC/weapons.qc:3993-4062`, `DropWeaponInHand`).** Two-handed throws (2H aim within the last 0.3 s) use
   `(v_this + v_other) / 2 × vr_2h_throw_velocity_mult (1.4)` (`:4004-4009`). Then:
   `throwVelUnits = throwVel × m2u × WeaponIdToThrowMult(w) × vr_weapon_throw_velocity_mult` (`:4021`).
   `WeaponIdToThrowMult` (`vr_weaponutil.qc:241-260`) is **0.4–1.0** (0.6 for RL/GL/SNG/LG, 0.4 for the laser cannon).
   The weapon spawns at the **current** `VRGetHandPos` and `VRGetHandRot` (`:4029`, `:4038`). The thrower's
   velocity is added (`weapons.qc:3881`). Spin comes from `VRThrowSpin` (the hand's ω mapped to `.avelocity`).
   Gravity is `vr_throw_gravity × m2u / sv_gravity` (`:4052`, 9.81 m/s², so `.gravity ≈ 0.40`) until the first
   touch, which resets it (`:3456`). Damage is `|v| / throwdmgspeed × base`.
6. **No aim assist.** `MOVETYPE_TOSS` with a tiny box of half-size 1–2.4 units, i.e. 3–8 cm
   (`vr_weaponutil.qc:304-318`). The builtin `calcthrowangle` (`vr_builtins.cpp:57-84`) exists but no QC calls it,
   and its gravity is multiplied by `host_frametime` (`:65`), which looks wrong for a closed-form ballistic solution.

### 1.2 Likely problems, most likely first

| # | Problem | Where | Why it feels "off" |
|---|---|---|---|
| P1 | **Throws are much weaker than the hand.** True-scale speed × `WeaponIdToThrowMult` 0.6 (0.4 for the laser cannon), with real gravity. | `weapons.qc:4021`, `vr_weaponutil.qc:241` | A 6 m/s release at 20° from 1.5 m lands about **4.5 m** away. The same throw with the ×0.6 weight lands about **2.3 m** away. Quake rooms and monster distances are 5–20 m. The weight multiplier was tuned for the old ×120 scaling, where it was harmless; on a 1:1 base it roughly halves the range (range ∝ v²). |
| P2 | **The time bases don't match, and the look-back is short.** Samples are timestamped with `realtime` but hold *predicted* poses, so the newest 1–3 samples are extrapolations past "now". The 0.1 s window is measured from that newest sample at *move-build time*. In front of that sit the finger-opening lag, the runtime's bool threshold, the key/command path and up to one 72 Hz net interval. | `vr_hands.cpp:146`, `vr_throw.cpp:99`, `vr_backend_openxr.cpp:155` | If the real release (peak speed) happened more than ~60–80 ms before the move that carries the bool edge, the peak is *out of the window*. The estimator then returns a decelerating sample, so throws come out randomly weak, low or sideways. Nothing timestamps the release itself. (Needs measuring, see §4.) |
| P3 | **The estimate isn't frozen at release.** Each move carries a fresh estimate; the server keeps the last move's. | `vr_client.cpp:134` | The move QC actually uses can be one or more net frames after the edge. The window then slides further past the peak (made worse by P2). |
| P4 | **The peak is chosen on `objVel` with a 0.2 m effective lever.** At wrist speeds of 15–25 rad/s, ω×r adds 3–5 m/s, and the angular velocity is the noisiest signal at the end of a throw. | `vr_throw.cpp:101,136`, `toLegacyPose` | The peak sample gets picked by wrist-snap noise, so direction and speed jitter from throw to throw. Alyx picks the peak on *controller* speed. Hurricane VR only adds the angular term above a threshold. |
| P5 | **Release uses the runtime's boolean squeeze.** | `vr_backend_openxr.cpp:828` | The bool's threshold and hysteresis are chosen by the runtime and usually fire late (near full release). Charlie Deck (Rescuties) releases once pressure drops a tuned amount (~20%) *below the peak since pickup* (§2.4). |
| P6 | **Spawned at the current hand position.** The velocity comes from ~40–100 ms earlier. | `weapons.qc:4029,4038` | The weapon starts from the follow-through position (lower, further along the arc) instead of from where it "left the hand". The arc then looks disconnected from the motion. |
| P7 | **Small hit box, no assist.** | `vr_weaponutil.qc:304`, no assist code | A thrown weapon has to pass its 3–8 cm box through a monster's box. With VR direction noise of several degrees that misses at 10 m, even when the throw "looked right". |
| P8 | **Two-handed ×1.4 is arbitrary.** Averaging both hand velocities is the midpoint's velocity, which is correct. | `weapons.qc:4004-4009` | Two-handed throws are 40% hotter than one-handed throws at the same motion, so muscle memory doesn't carry over. |

---

## 2. What others do (sources opened)

### 2.1 Valve: SteamVR Interaction System and Half-Life: Alyx
- **Throwable.cs** ([source](https://raw.githubusercontent.com/ValveSoftware/steamvr_unity_plugin/master/Assets/SteamVR/InteractionSystem/Core/Scripts/Throwable.cs)).
  The default is `releaseVelocityStyle = GetFromHand`, which takes the runtime velocity at `releaseVelocityTimeOffset = -0.011`
  (one 90 Hz frame back), with `scaleReleaseVelocity = 1.1`. There is an optional speed-dependent gain:
  `scaleFactor = clamp01(curve(|v| / scaleReleaseVelocityThreshold))`, with a default curve `EaseInOut(0, 0.1, 1, 1)`.
  It is disabled by default (`threshold = -1`). Its tooltip says it allows "greater differentiation between a drop, toss, and throw".
  The styles are `NoChange`, `GetFromHand`, `ShortEstimation` and `AdvancedEstimation`.
- **AdvancedEstimation.** `SteamVR_Behaviour_Pose.GetEstimatedPeakVelocities`
  ([source](https://raw.githubusercontent.com/ValveSoftware/steamvr_unity_plugin/master/Assets/SteamVR/Input/SteamVR_Behaviour_Pose.cs))
  runs `historyBuffer.GetTopVelocity(10, 1)` and then `GetAverageVelocities(…, 2, top)`
  ([SteamVR_RingBuffer.cs](https://raw.githubusercontent.com/ValveSoftware/steamvr_unity_plugin/master/Assets/SteamVR/Scripts/SteamVR_RingBuffer.cs)).
  That means: find the top **controller** speed among the last 10 frames, then average 2 frames around it. The history stores runtime velocities, not differenced ones.
- **ShortEstimation.** `VelocityEstimator.cs` differences the object's transform (`velocityAverageFrames = 5`, `angularVelocityAverageFrames = 11`).
- **Half-Life: Alyx developer commentary, "Throwing" (Tony Cox)**
  ([transcript](https://combineoverwiki.net/wiki/Developer_commentary/Half-Life:_Alyx)):
  - *The Lab* differenced positions and averaged "over several frames before and after the object release". It was inconsistent, especially below 90 fps.
  - *Moondust* biased the estimate towards higher velocities and switched to the hardware IMU velocity (computed every 2 ms), so throwing no longer depended on frame rate.
  - *Alyx* "takes ten frames that preceded the release … and averages the three frames bracketing the peak controller velocity". At 90 Hz that is a ~111 ms window and a ~33 ms span.
  - **The current algorithm 2 is essentially this.** The remaining problems are therefore in the inputs, timing and gameplay scaling, not in the core estimator.

### 2.2 Unity XR Interaction Toolkit (XRGrabInteractable)
([source mirror](https://raw.githubusercontent.com/needle-mirror/com.unity.xr.interaction.toolkit/master/Runtime/Interaction/Interactables/XRGrabInteractable.cs),
[API](https://docs.unity3d.com/Packages/com.unity.xr.interaction.toolkit@3.0/api/UnityEngine.XR.Interaction.Toolkit.Interactables.XRGrabInteractable.html))
- It differences the **object's target pose** (so the tangential velocity at the object is included implicitly) into a 20-frame ring buffer.
- It takes a weighted mean over `throwSmoothingDuration = 0.25 s`, weighted by `throwSmoothingCurve` (flat by default).
- Defaults: `throwVelocityScale = 1.5`, `throwAngularVelocityScale = 1.0`. Frames with dt < 1 ms are skipped (a Quest timing quirk).
- A source comment admits the two-handed case is imperfect: frames after the first hand releases should be ignored.

### 2.3 Meta Interaction SDK
- The `StandardVelocityCalculator` ([API](https://developers.meta.com/horizon/reference/interaction/v71/class_oculus_interaction_throw_standard_velocity_calculator),
  [docs](https://developers.meta.com/horizon/documentation/unity/unity-isdk-using-with-physics/)) buffers timed poses with runtime velocities.
  It blends **instant**, **trend** (history), **tangential** (from ω about an estimated axis of rotation, with
  `AxisOfRotationOrigin`) and **external** velocity through `…Influence` weights. It also has `ReferenceOffset` (the point
  whose velocity is estimated) and `StepBackTime` (looks back in time from the release). The formula and default values are not
  in the public docs (*not verified*).
- `RANSACVelocity` ([API](https://developers.meta.com/horizon/reference/interaction/v71/class_oculus_interaction_throw_r_a_n_s_a_c_velocity))
  selects "the best pair of linear and angular velocities from a buffer of recent timed poses" by random-sample consensus.
  Defaults: `samplesCount = 10`, `samplesDeadZone = 2`, `minHighConfidenceSamples = 2`. It also has a
  `MaxSyntheticSpeed` clamp. Both calculators are now superseded by `Grabbable`'s built-in throw.

### 2.4 Developer write-ups
- **Charlie Deck, "Why Throwing in VR Sucks – and How to Make it Better"** (Rescuties)
  ([Game Developer](https://www.gamedeveloper.com/design/why-throwing-in-vr-sucks--and-how-to-make-it-better)):
  - Low-pass averaging feels "underwater".
  - Picking the peak velocity gave directional error from noise, so he fits a **linear regression** ("trendline") through the last few frames (the debug view shows 4).
  - Velocity is measured at the **controller**, where the user feels the mass, not at the virtual object.
  - **Release is triggered when grip pressure eases ~20% below its peak since pickup**, not at 0%.
- **Wilco Schoneveld, web VR throwing game** ([dev.to](https://dev.to/wilcoschoneveld/challenges-of-a-web-vr-throwing-game-1c66)):
  quadratic regression over the last 6 positions (`p = a + b·t + c·t²`, velocity = the `b` term with t centred on release), plus a `ω × r` "centrifuge" term.
- **NormalVR, "Throwing Throw Down"** ([blog](https://www.normalvr.com/blog/throwing-throw-down/)):
  use the runtime's controller velocity and angular velocity and evaluate the velocity at any point rigidly attached to the controller (`GetPointVelocity`, i.e. `v + ω × r`). Custom estimators were less consistent than the runtime's values.
- **Hurricane VR** ([HVRHandGrabber API](https://cloudwalker2020.github.io/HurricaneVR-Docs/api/HurricaneVR.Framework.Core.Grabbers.HVRHandGrabber.html)).
  Tooltips only; the defaults are not public:
  - `ThrowLookback` ("frames to average") and `ThrowLookbackStart` ("frames to skip", i.e. drop the frames just before release)
  - `TakePeakVelocities`
  - `ReleasedVelocityFactor`
  - `ReleasedAngularConversionFactor` ("angular to linear") and `ReleasedAngularThreshold` ("hand angular velocity must exceed this to add linear velocity based on angular velocity")
  - `HVRThrowingCenterOfMass`: a per-controller-model centre-of-mass point used for the angular-to-linear conversion.
- **Godot XR Tools** ([function_pickup.gd](https://raw.githubusercontent.com/GodotVR/godot-xr-tools/master/addons/godot-xr-tools/functions/function_pickup.gd),
  [velocity_averager.gd](https://raw.githubusercontent.com/GodotVR/godot-xr-tools/master/addons/godot-xr-tools/misc/velocity_averager.gd)):
  `velocity_samples = 5`, a time-weighted average of the held object's transform deltas, `impulse_factor = 1.0`.
- **Auto Hand** ([docs](https://earnest-robot.gitbook.io/auto-hand-docs/auto-hand/hand)): a single "Throw Power" multiplier on release (the default is not stated).

### 2.5 Aim assist: Rec Room patent
US10990169B2, "Assisting virtual gestures based on viewing frustum", Rec Room Inc
([Google Patents](https://patents.google.com/patent/US10990169B2/en)):
- The target comes from **gaze** (head or eye direction, or the view frustum), compared with the throw's motion vector.
- The gaze gets high weight when the angle is under an acute threshold ("e.g. 20 degrees or 30 degrees") and is ignored beyond a larger one ("such as 90 degrees").
- The object leaves the hand on its natural trajectory for a short first stage ("on the order of 10 milliseconds or less"). It is then "slightly accelerated" onto a newly computed trajectory that lands on or near the target. This keeps the release looking right.

### 2.6 Research
- **OpenXR spec, Prediction Time Limits**
  ([fundamentals.adoc](https://raw.githubusercontent.com/KhronosGroup/OpenXR-Docs/main/specification/sources/chapters/fundamentals.adoc)):
  "The runtime must retain and return at least 50 milliseconds of historical data … preceding the most recently received pose."
  Locating in the past is supported, so the release time can be looked up exactly (within 50 ms).
  For a longer look-back we must keep our own history, which we already do.
- **Butkus & Ceponis 2019, "Accuracy of throwing distance perception in VR"** ([CEUR](https://ceur-ws.org/Vol-2470/p33.pdf)).
  Vive tracker; n = 6, so it is weak evidence. The throw was released automatically when tracker speed began to drop after rising.
  Participants used "3 to 5 % more power" in VR, were more accurate at longer distances, and tended to under-throw overall.
  So users don't grossly under-throw: large multipliers are a *gameplay* choice, not a perceptual correction.
- **Yamac & O'Sullivan 2022, "FauxThrow"** ([arXiv 2208.02166](https://arxiv.org/pdf/2208.02166)).
  A perceptual study of animated throws with a shifted point of release: late release in overarm throws is noticed more easily than early release.
  It cites Zindulka et al. (CHI 2020) for VR throws being less accurate than real ones, "mainly due to lower accuracy in distance and height dimensions",
  which is consistent with release-timing error. The Zindulka paper itself (ACM) could not be opened: *unverified beyond this citation*.
- Not opened (blocked or unavailable), so not used: Karl Lewis "How to Improve Throwing Physics in VR",
  OpenReview "Towards Better Throwing … Point of Release Mechanics", Meta forum thread on Grabbable smoothing.
  No primary Owlchemy source on throwing was found. The Game Developer article on their Vision Pro port only covers hand-tracking pose extrapolation.

### 2.7 Technique summary with numbers

| Technique | Values seen |
|---|---|
| Velocity source | Runtime IMU velocity (Valve Moondust/Alyx, NormalVR) instead of differenced positions (The Lab, XRI, Godot) |
| Window and peak | Alyx: 10 frames before release (~110 ms), mean of the 3 frames around peak *controller* speed. SteamVR Advanced: 10 frames, 2-frame mean |
| Plain averaging | XRI 0.25 s (weighted); Godot 5 frames; Valve ShortEstimation 5 frames (angular 11) |
| Regression | Deck: linear, ~4 frames; Schoneveld: quadratic, 6 frames |
| Robust fit | Meta RANSAC: 10 samples |
| Look-back / time offset | Valve −11 ms; HVR `ThrowLookbackStart`; Meta `StepBackTime` |
| Release detection | Deck: grip −20% from peak since pickup; Butkus: automatic on deceleration |
| Tangential term | NormalVR / Schoneveld `ω × r`; HVR only above an ω threshold, with a factor; XRI/Godot implicitly (they track the object) |
| Linear gain | Valve 1.1 (+ optional speed curve, 0.1 → 1.0); XRI 1.5; Godot 1.0 |
| Angular gain | XRI 1.0 |
| Aim assist | Rec Room: gaze cone 20–30° full weight, 90° none; natural flight ≤ 10 ms, then steer onto a computed ballistic path |

---

## 3. Recommendation

The estimator core (Alyx-style peak) is fine. Fix the **inputs** (a single time base, release timestamp, no noisy lever
in peak picking), **freeze** the estimate at release, **backdate** the spawn, replace the heavy weight multiplier
with a **speed-dependent gain**, and add an **optional aim assist**.

### 3.1 Engine (C++): history, release, estimate

```
// ---- per frame, per hand (vr_hands / vr_throw) ----
// Timestamp every sample with the XrTime its pose is FOR (predictedDisplayTime),
// converted to seconds; not realtime.  Keep ~0.3 s (capacity 64 is fine at <=144 Hz, raise to 128).
Sample { t_xr; p_ctrl; q; v_ctrl; w; grip; }        // controller point = raw pose (as now), world-turned
// Optional, better: after xrSyncActions, ALSO locate the grip space at t_now
// (XR_KHR_win32_convert_performance_counter_time: QueryPerformanceCounter -> XrTime) and store that
// non-predicted sample instead; fall back to predictedDisplayTime when the extension is missing.

// ---- release detection (vr_throw_release_mode) ----
// mode 0: runtime bool (as today).  t_rel = XrActionStateBoolean.lastChangeTime of the falling edge.
// mode 1 (default proposal): float squeeze with drop-from-peak (Deck):
//   while grabbed: gPeak = max(gPeak, grip)
//   release when grip < gPeak * (1 - vr_throw_release_drop)   // default 0.25
//            or  grip < vr_throw_release_floor                 // default 0.35
//   t_rel = XrActionStateFloat.lastChangeTime (same XrTime base as the samples)
//   re-grab needs grip > vr_throw_grab_press (0.7) (hysteresis); reset gPeak on grab.
//   This drives the existing grab bit, so QC needs no change.

// ---- estimate, computed ONCE at the release edge, then frozen ----
Estimate estimateAtRelease(hand, t_rel):
  W  = samples with t in [t_rel - vr_throw_window, t_rel + vr_throw_lookahead]   // 0.12 s, 0.01 s
  pk = argmax over W of |v_ctrl|                     // CONTROLLER speed, no lever term (Alyx)
  S  = samples with |t - t_pk| <= vr_throw_peak_span // 0.017 s: ~3 frames at 90 Hz
  v  = mean(v_ctrl over S)
  w  = mean(w over S')   where S' uses 2*span       // angular velocity is noisier; average more
  // tangential term at the object's centre, gated and scaled (HVR-style)
  r  = R(q_pk) * comOffset[weapon]                   // metres from controller point; default 0.05 m along the barrel
  if |w| > vr_throw_ang_threshold (6 rad/s):
      v += vr_throw_ang_factor (0.7) * cross(w, r)
  p  = p_ctrl(pk) + r                                // release position of the object's centre
  age = t_now_sample - t_pk                          // seconds since that release point
  return { v, w, p, age }

// Transport: once the grab bit falls, every move carries the frozen {v, w, p, age(+elapsed)} until
// the hand grabs again (fixes P3). age is refreshed per move = (now - t_pk).
// Debug (vr_debug_throw 1/2): see section 4.
```

Notes:
- If `t_rel` is not available (mock backend, flat mode), use the newest sample time. With a consistent XrTime base,
  the window is anchored at the *release* and not at "whenever the move was built" (fixes P2).
- Keep algorithms 0/1/2 as they are for A/B comparisons. Add this as **algorithm 3**.
- *Optional* direction refinement (Deck/Schoneveld): keep the peak-average *magnitude*, but take the *direction* from a
  least-squares line fit of `p_ctrl(t)` over `[t_pk − 0.04, t_pk + 0.01]`. Do this only if the logs show direction jitter
  (`vr_throw_dir_fit 0/1`).

### 3.2 QC: gameplay scaling, spawn, gravity

```
// DropWeaponInHand, replacing weapons.qc:4021 and the spawn origin
s     = vlen(v)                                         // m/s
gain  = 1 + (vr_throw_gain_max - 1) * smoothstep(vr_throw_gain_lo, vr_throw_gain_hi, s)
        // defaults: max 1.5 (XRI default), lo 1.5 m/s, hi 6 m/s -> drops/handing stay 1:1, real throws get up to 1.5x
weight = mix(1, WeaponIdToThrowMult(w), vr_throw_weight_influence)   // default 0.25 -> 0.85..1.0 (was 0.4..1.0)
vU    = v * m2u * gain * weight * vr_weapon_throw_velocity_mult
two-handed: v = (vA + vB) / 2 at the SAME t_rel (the first hand's release); vr_2h_throw_velocity_mult default 1.0

// Backdated spawn (P6): where the object would be now had it left at t_pk
g     = vr_throw_gravity * m2u
pNow  = pU + vU * age + '0 0 -0.5' * g * age*age       // pU = p * m2u relative to the player, as the hand pos
traceline(VRGetHandPos(h), pNow, MOVE_NOMONSTERS, self) // never spawn in a wall
origin = trace_endpos (pulled back 1 unit along the trace); velocity = vU + '0 0 -1' * g * age + thrower.velocity
if age > 0.15: age = 0.15                               // clamp, in case of a stale estimate
```

- **Gravity.** Keep `vr_throw_gravity` (9.81 true scale). With P1 fixed, a 7 m/s throw with a 1.3× gain at 20°
  (9.1 m/s from 1.5 m) reaches about 8 m, which is room scale in Quake. If playtests still find arcs too short, 7–8 m/s² is a gentler
  arcade compromise before raising the gain further. The gain and gravity cvars already let the user tune this.
- **Spin.** Keep the hand's ω (XRI's angular scale is 1.0). Cap it at ~20 rad/s (≈1150°/s) so wrist-flick noise
  doesn't produce helicopter weapons.
- **Hit box (P7).** Against monsters, test contact with an inflated box (e.g. 8 units half-size, via a
  `traceline`/`tracebox` from the old to the new origin in the think, or the existing
  `vr_gameplayfix_missilesize` style). Keep the small box for world collision.

### 3.3 Optional aim assist (QC, server side)

```
// cvars: vr_throw_assist 0/1 (default 0; opt-in like the other rules),
//        vr_throw_assist_cone 12 (deg, max angle between the throw dir and the target),
//        vr_throw_assist_full 4 (deg, full strength inside),
//        vr_throw_assist_strength 0.8, vr_throw_assist_range 1200 (units),
//        vr_throw_assist_gaze 30 (deg; the target must also be within this of head forward, 0 = off; Rec Room)
//        vr_throw_assist_speed 0.15 (max relative speed change allowed to reach the target)
d = normalize(vU); s = vlen(vU)
best = world
for e in findradius(origin, range):
    if !(e.flags & FL_MONSTER && e.health > 0) && !(e.takedamage && e.classname in {breakaway, breakablewallwithrubble, …}): continue
    c = centre(e) + e.velocity * tFlightEstimate(e)        // one-step lead: t ≈ |c - origin|_xy / |vU|_xy
    a = angle(d, normalize(c - origin))
    if a > cone: continue
    if gaze && angle(v_forward(head), normalize(c - origin)) > gaze: continue
    traceline(origin, c, MOVE_NOMONSTERS, self); if trace_fraction < 1: continue
    score = a / cone + 0.2 * |c - origin| / range         // prefer the smallest angle, then nearer
    keep lowest score
if best:
    // ballistic solution at speed s (low arc); if none, allow s *= up to 1 + assist_speed; if still none, yaw-only assist
    theta = lowArcAngle(s, g, dx_horizontal, dz)            // a new closed form; don't reuse calcthrowangle (g * frametime bug?)
    d* = horizontal dir to c rotated up by theta
    k  = strength * (a <= full ? 1 : (cone - a) / (cone - full))
    d' = slerp(d, d*, k); vU = d' * s'
```

- **Where.** In QC, at spawn time. Rec Room's "natural flight for ≤ 10 ms, then steer" refinement is optional. It can be
  done by storing `d*` and nudging `.velocity` in the first think (0.02 s), so the first frames match the hand. With a
  cone of 12° the difference is not noticeable, so a direct blend at spawn is acceptable.
- **Targets.** Monsters (`FL_MONSTER`, alive) plus `takedamage` breakables (`hip_brk.qc` classnames). Exclude the
  player, other players in co-op, and corpses.
- **Visibility.** Draw nothing by default. Optionally, with `developer 1`, draw a particle at the chosen target.

### 3.4 New and changed cvars (engine `vr_cvars.inc` / QC `vr_cvars.qc`)

| Cvar | Default | Side |
|---|---|---|
| `vr_throw_algorithm` | 3 (new) | C++ |
| `vr_throw_window` / `vr_throw_lookahead` / `vr_throw_peak_span` | 0.12 / 0.01 / 0.017 s | C++ |
| `vr_throw_release_mode` / `_drop` / `_floor` / `vr_throw_grab_press` | 1 / 0.25 / 0.35 / 0.7 | C++ |
| `vr_throw_ang_threshold` / `vr_throw_ang_factor` / `vr_throw_lever_arm` (now the CoM offset from the controller point) | 6 rad/s / 0.7 / 0.05 m | C++ |
| `vr_throw_gain_max` / `_lo` / `_hi` | 1.5 / 1.5 / 6 m/s | QC |
| `vr_throw_weight_influence` | 0.25 | QC |
| `vr_2h_throw_velocity_mult` | 1.0 (was 1.4) | QC |
| `vr_throw_assist*` | see §3.3 | QC |

All these defaults are *starting points* taken from the sources where available (window, span, gain) and otherwise
chosen by me (thresholds, influence). Confirm them with the logging below.

---

## 4. Test plan

1. **Log the timing chain** (`vr_debug_throw 2`), printing per throw:
   - `t_rel − t_pk`: release lag after peak; expected 20–120 ms
   - `t_moveSent − t_rel`: pipeline lag
   - how many samples in the window are predicted rather than "now"
   - `gPeak` and grip at release
   - controller speed at peak versus at release
   - `|ω|` and the tangential term's size
   - final speed, gain and weight
   - elevation and azimuth of `v` relative to head forward
   - `age`

   If `t_rel − t_pk` is often close to or above today's effective look-back (~60–80 ms), P2 is confirmed.
2. **Dump the raw history** (`vr_debug_throw_dump 1`: the last 0.3 s of samples, grip value and the release event as CSV
   in the game dir). Replay the same throws offline through algorithms 0/1/2/3 (the estimator is a pure function of
   the history) to compare throw-to-throw spread of speed and direction without re-throwing.
3. **Mock backend.** `vr_mock_swing` gives an analytic arc (`vr_backend_mock.cpp:142`). Check that algorithm 3 returns
   `r·θ̇` at the release angle within ~2%, at host frame rates of 72, 90, 120 and 144 (frame-rate independence).
4. **Range test.** Put targets at 5, 10 and 15 m (start.bsp or e1m1 corridor). For each configuration (current defaults;
   algorithm 3 without gain; with gain; with assist), throw 20 weapons per distance. QC logs the first-impact point's
   distance to the target (`dprint` in `forcegrabbable_touch`). Compare mean error (bias: short/long) and spread.
   Also compare subjective "it went where I threw it".
5. **Regression checks.**
   - Gentle drops and hand-to-hand passes still land at your feet (gain ≈ 1 below 1.5 m/s).
   - Holstering doesn't trigger throws.
   - Release mode 1 doesn't drop weapons when the grip is merely relaxed (log false releases; tune `_drop`).
   - Sticky grip mode (`vr_weapon_grip_mode 1`) still works.
   - Two-handed throws match one-handed ones at equal motion.
