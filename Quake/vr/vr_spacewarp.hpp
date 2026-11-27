#pragma once

// Application SpaceWarp (the Quest's XR_FB_space_warp; vr_xr_spacewarp, docs/vr-port/ROUND26.md "spacewarp"): each eye's
// motion vectors and depth go to the runtime with its colour, and the runtime draws a made-up frame between two of the
// game's from them, so the game runs at half the display's rate (the runtime halves it once the frames carry them).
//
// The motion vectors are temporal AA's (vr_temporal.cpp): the eye's scene writes the models' and moving brush models'
// own motion into the motion target, and the camera's motion (head, locomotion) comes from the depth and the eye's
// view-projection one frame ago. Each eye's pass (eyePass, from VR_TemporalResolve) turns them into the runtime's form
// at its size: xyz this frame's NDC minus the last frame's (Meta's GLES sample), and the depth as the projection wrote
// it. The play space's own motion in the world (locomotion, turning) goes with the frame (appSpaceDelta), and a jump
// (teleport, snap turn, a new map) marks it skipped.

namespace qvr::spacewarp
{

// VR_RenderView, once a frame before the eyes: whether this frame's eyes make SpaceWarp's images (vr_xr_spacewarp with
// a runtime that has it, or vr_spacewarp_test; never while a menu or the console is up: the UI over the eyes has no
// motion of its own). Read by temporal::wanted (the motion target collected) and the eyes' path (the post-process's).
void beginFrame();
[[nodiscard]] bool frameOn();

// One eye after its scene (VR_TemporalResolve): the scene's depth texture and the motion target (texels with this
// stamp hold a model's own motion: uv a frame ago minus uv now), the eye's unjittered view-projection's inverse and the
// one a frame ago, the drawn size (the textures' lower left part), the projection's planes (Quake units), and whether
// the view jumped since the last frame.
struct EyeInputs
{
    int eye{0};
    unsigned depthTex{0};
    unsigned motionTex{0};
    const float* invViewProj{nullptr};
    const float* prevViewProj{nullptr};
    float stamp{0.f};
    int drawnWidth{0};
    int drawnHeight{0};
    float nearUnits{0.f};
    float farUnits{0.f};
    bool reversed{false};
    bool jump{false};
    const float* origin{nullptr}; // the eye's place and angles in the world (r_refdef's)
    const float* angles{nullptr};
};
void eyePass(const EyeInputs& in);

// VR_RenderView, after both eyes, before the backend's endFrame: the frame's information (the play space's delta, the
// depth's planes, skipped) given to the runtime.
void endFrame();

void registerCommands();

// VR stopped (stereo::releaseTargets): the test's images and the framebuffers freed.
void releaseTargets();

} // namespace qvr::spacewarp
