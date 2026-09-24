# Research: a full-body avatar with IK

Status: steps 1–4 implemented on 2026-09-24 (see *Implemented* at the end). Steps 5–7 are still open.

Today's body is `progs/vrtorso.mdl`: one rigid vertex-animated model, placed below the head and turned to
`bodyYaw` (vr_view.cpp, "Body"). It floats, has no arms, and does not bend when you crouch or lean. This note
covers what a skinned body driven by inverse kinematics would take in this port.

## Summary

It is much less work than it would have been in the old engine, because **Ironwail 0.8.2 already has GPU-skinned
skeletal models**. It loads MD5 meshes as "enhanced" replacements for `.mdl` files, and a vertex shader blends up to four
bone matrices per vertex. The engine side comes down to one new ability: letting an entity supply **its own bone
matrices each frame** instead of the model's baked animation poses. That is about 30 lines of `// QVR` hooks in
`r_alias.c`. Everything else lives in `Quake/vr/`.

The real costs are:
1. **the rigged body asset**, which is the biggest unknown;
2. **tuning the IK in the headset**, since elbows and shoulders that look wrong feel worse than no arms.

Recommended path:
1. Engine hook.
2. Upper body: torso and arms, with two-bone arm IK using the heuristics of Parger et al.
3. Optionally, runtime body tracking (`XR_FB_body_tracking`, which Virtual Desktop forwards from Quest 3), used
   instead of IK when present.
4. Legs last, behind an option.

## What the engine already has

- **MD5 loading** (`gl_model.c`, `Mod_LoadMD5MeshModel`):
  - `progs/foo.md5mesh` plus `progs/foo.md5anim` replace `progs/foo.mdl` when `r_enhancedmodels` is 1, which is the
    default (`loadMd5Replacement`).
  - An `.md5anim` is required. A one-frame anim holding the bind pose is enough for us.
  - The `.mdl` must also exist. A tiny placeholder is fine; it is also what flat or vanilla setups would show.
- **Skinning** (`gl_shaders.h`, `POSEVERTTYPE == 1`, PV_IQM):
  - Vertices carry four bone indices and four weights.
  - The shader reads `mat3x4 BonePoses[]` from the SSBO at binding 2, starting at the instance's `pose` index.
  - `r_alias.c` binds that range to the model's baked poses in the mesh VBO.
  - Each stored matrix is the **final skinning matrix**, `bonePose * inverseBind` (`MD5Anim_Load`:
    `R_ConcatTransforms(frameposes[j], bones[j].inverse, out)`). So an IK solver only has to produce the same 3×4
    matrices in model space; the entity matrix is applied after them, as usual.
- **Bone names and parents** (`boneinfo_t`: `name`, `parent`, `inverse`). The solver can find "upperarm_r" and the
  other bones by name, so the asset only has to follow a naming convention.
- **A skeleton debug view**: `R_DrawAliasModels_ShowSkel` draws the bones, which is useful for tuning if the hook
  also feeds it.

## Engine hook (small)

In `r_alias.c`, for an entity the VR module owns:

1. `R_Alias_CanAddToBatch`: never batch a VR-posed entity with another one. It is one entity, drawn once per eye, so
   batching doesn't matter.
2. Where `buffers[1]` is chosen for `PV_IQM`: if `VR_BonePoses(ent, &matrices, &count)` returns matrices, upload them
   with `GL_Upload(GL_SHADER_STORAGE_BUFFER, ...)`. Bind that range instead of the model's poses, and set the
   instance's `pose1 = pose2 = 0`.
3. Optionally, `R_DrawSkeleton` uses the same matrices.

The matrices are computed once per frame in the VR module, before the eyes are rendered, and reused for both
eyes.

A **hidden head** falls out of skinning: set the head bone's matrix to a zero scale and its vertices collapse to a
point, so the camera is never inside the head. The same trick hides the upper arms or legs per option.

## The skeleton and solver

A minimal skeleton, about 17 bones:
- pelvis, spine, chest, neck, head;
- per side: clavicle, upper arm, forearm, hand, thigh, calf, foot.

The hands stay the existing `hand.mdl` with its finger frames, anchored as today. The forearm's end must meet the
wrist, which is the one place the two models have to agree.

**Inputs we already have every frame** (vr_hands / vr_body):
- head pose;
- both hand poses (after collision, 2H, weight smoothing);
- `bodyYaw` (head and hands blended);
- `headHeight`, `vr_height_calibration`, world scale;
- player origin and velocity.

**Per frame:**
1. **Neck and chest from the head.** Place the neck at a fixed offset behind and below the eyes, rotated by the
   head's pitch and roll (damped). The chest faces `bodyYaw`.
2. **Spine and pelvis.** Compare the head's height with the calibrated standing height to get crouch (and lean).
   The spine bends between neck and pelvis. The pelvis sits under the neck, pushed back a little when leaning
   forward.
3. **Shoulders.** The clavicle rotates up or forward (up to about 20–30°) when the hand is above the shoulder or
   reaching far, which gives the reach extra length.
4. **Arms.** Use an analytic two-bone IK from the shoulder to the wrist target (controller grip pose to wrist
   offset). The elbow's swivel angle is the hard part. Use the heuristics of Parger et al. (VRST 2018): the elbow
   goes down, outward and back, with a swivel that depends on the shoulder-to-hand distance and the hand's roll,
   clamped to joint limits. When the controller is out of reach, stretch slightly and then let the hand leave the
   arm; the hand must stay exactly where the controller is.
5. **Legs, optional.** Feet stay planted in the world. When the pelvis is too far from a foot, that foot steps: an
   arc over a short time, landing on a downward trace for stairs and slopes. The knee pole points forward. Crouching
   bends the knees by itself.

   Quake's movement is fast (about 10 m/s), so smooth locomotion needs a blended run cycle rather than steps.
   Untracked legs are a known source of discomfort in VR ("that's not where my legs are"). Keep them behind an
   option, possibly shown only when looking down or only in third person.

**Reference implementation:** [VRArmIK](https://github.com/dabeschte/VRArmIK) (MIT, Unity C#), by the paper's
authors. It covers shoulder estimation, arm IK, elbow heuristics and T-pose calibration. It is small enough to port
to glm. MIT is compatible with the GPL.

**Calibration.** `vr_height_calibration` already exists. Arm span, from a T-pose, would improve arm lengths. Without
it, use proportions from the height.

## Runtime body tracking, a better input when available

[Virtual Desktop's OpenXR runtime (VDXR)](https://github.com/mbucchia/VirtualDesktop-OpenXR) forwards Quest body
tracking to PC games through **`XR_FB_body_tracking`** (and `XR_META_body_tracking_full_body`, generative legs). On
Quest 3 the upper body comes from inside-out body tracking, which sees the elbows and shoulders, so it beats any
IK estimate. It is exactly the user's setup.

Integration:
1. Enable the extension if it is listed (the same way as `XR_META_touch_controller_plus`).
2. Create a body tracker.
3. `xrLocateBodyJointsFB` each frame.
4. Map its joints onto our skeleton.
5. Fall back to IK when the extension or the tracking is absent.

This comes after the IK. The skeleton, asset and hook are shared, so it is mostly a joint-mapping table.

## The asset

This is the main open question: we need a first-person-friendly skinned body in MD5 format, in Quake's style.

| Option | Pros | Cons |
|---|---|---|
| Re-rig the Ranger: `player.mdl`'s stand frame converted to a mesh, skinned to our skeleton | Looks like the Quake player; bind pose can be any pose once the skeleton is fitted to it; low-poly, so automatic weights are acceptable | The stand pose holds a gun, so arms need care; id asset (mods routinely ship derivatives, as Quake VR already does) |
| New low-poly body authored in Blender, exported with an MD5 exporter add-on | Clean topology, proper arms, designed to match `hand.mdl` at the wrists | Needs modelling and texturing time from someone |
| Generated in code (capsule limbs with a Quake-palette texture) | No art needed; fine for a prototype and for tuning the IK | Looks like a prototype |

I can do the first and third without Blender. A script can:
1. read the MDL;
2. fit the skeleton;
3. compute weights from distance to the bone segments;
4. write `.md5mesh` and a one-frame `.md5anim`.

That gives a working prototype, from which a hand-made asset can replace it later.

## Other places that should follow the skeleton

- **Holster anchors** (vr_body): hips and shoulders should come from the pelvis and clavicle bones, so holstered
  weapons sit on the body.
- **Virtual stock:** the shoulder position comes from the clavicle.
- **The "upper torso" point** that hand and barrel collisions sweep from (vr_handpose) comes from the chest.
- **`vr_vrtorso_*` cvars:** kept for the old torso as a fallback mode.
- **Other players in multiplayer:** they currently see `player.mdl`. Head and hand poses reach the server through
  the VR move. Sending them to other clients and running the same solver there is a later step.

## Phases and rough effort

| Step | Work | Estimate |
|---|---|---|
| 1 | Engine hook (per-entity bone matrices, no batching, skeleton debug) plus loading `progs/vrbody.*` | 1 day |
| 2 | Prototype asset (generated capsules or re-rigged Ranger) and the MD5 writer script | 1–2 days |
| 3 | Upper-body solver: neck, chest, spine, crouch, clavicles, two-bone arms with the VRArmIK elbow heuristics; `vr_body_mode` (0 off, 1 old torso, 2 torso and arms, 3 full); hidden head | 2–3 days, plus headset tuning |
| 4 | Holsters, virtual stock and collision origin taken from the skeleton | 0.5 day |
| 5 | `XR_FB_body_tracking` when the runtime offers it | 1–2 days |
| 6 | Procedural legs (planted feet, stepping, run blend) | 2–4 days, most uncertain |
| 7 | Bodies for other players | 1–2 days |

Steps 1–3 can be verified with the mock backend and the skeleton debug view (screenshots and `vr_dumpview`), as
the rest of the port was. Only the feel needs the headset.

## Implemented (steps 1–4)

- **Engine hook** (`r_alias.c`, 5 `// QVR` lines):
  - `VR_AliasBonePoses` gives an entity its own skinning matrices, uploaded per draw.
  - Such entities are never batched with others.
  - They are never culled: posed limbs reach past the model's bounds.
- **Body scale:** carried by the entity's model matrix (`VR_AliasPostTransform`). The bone matrices stay at the
  model's scale, because the shader uses the skinned normals without renormalising them.
- **Asset:** `Misc/quakevr/make_vrbody.py` writes `quakevr/progs/vrbody.{md5mesh,md5anim,mdl}` and
  `vrbody_00_00.tga`. It is a 19-bone, 648-triangle low-poly body in Quake palette colours: skin, leather, cloth
  and boots.
  - The skeleton's bind pose is duplicated in `vr_avatar.cpp` (`bind()`). With `developer 1`, a mismatch is
    reported when the model loads.
  - Ironwail reads only the animated components of an `.md5anim`, so the one bind frame marks all of them animated.
- **Solver** (`Quake/vr/vr_avatar.cpp`):
  - The top of the neck is found behind and below the eyes. The torso sits `vr_body_torso_back` (0.1 m) behind
    it, so looking down shows the chest rather than the top of the shoulders.
  - A crouch drops the pelvis by `vr_body_crouch_legs` (0.7) of the head's drop. The back leans to absorb the
    rest.
  - The clavicles rise and swing forward when reaching up or far forward.
  - Each arm is a two-bone chain to the drawn hand's wrist (the centre of `hand_base.mdl`'s wrist). The elbow
    points down, `vr_body_elbow_out` outward, `vr_body_elbow_back` backward, and `vr_body_elbow_hand` away from the
    back of the hand. Arms stretch up to `vr_body_arm_stretch` (1.1) to reach.
  - Legs (mode 3) stand with the feet under the head, knees forward. They have no stepping (step 6).
  - The head and neck are collapsed, so the eyes are never inside them.
- **Modes:** `vr_body_mode` (Options > VR Settings > Body):
  - 0: off;
  - 1: the old torso;
  - 2: torso and arms (the default);
  - 3: full body.

  Modes 2 and 3 fall back to 1 if the model is not usable (for example with `r_enhancedmodels 0`).
- **Anchors (step 4):** with `vr_body_anchors` 1, these follow the body's lean and crouch:
  - The holsters are placed as before for the standing body, then carried by the pelvis (hips) or the chest (the
    upper and shoulder holsters).
  - The same goes for the virtual stock's shoulders.
  - Hand and barrel collisions sweep from the chest.
  - When standing upright, everything is where it was.
- **Tuning aids:** `vr_body_debug`:
  - 1 draws the skeleton, with each bone's hint axis in blue;
  - 2 also shows the body in front of you, facing you;
  - 3 shows it from its left.

  In a headset, this shows the whole pose while you move.

## Sources

- Parger, Mueller, Schmalstieg, Steinberger. [Human upper-body inverse kinematics for increased embodiment in
  consumer-grade virtual reality](https://dl.acm.org/doi/10.1145/3281505.3281529), VRST 2018;
  [author's post](https://dabeschte.github.io/paper/2018/12/01/vrarmik.html) and
  [code](https://github.com/dabeschte/VRArmIK).
- [Virtual Desktop: body, hand, face and eye tracking forwarded through VDXR](https://mixed-news.com/en/virtual-desktop-update-enables-quest-hand-tracking-on-pc-via-vdxr/).
- Ironwail sources: `gl_model.c` (`Mod_LoadMD5MeshModel`, `MD5Anim_Load`), `r_alias.c`, `gl_shaders.h`.
