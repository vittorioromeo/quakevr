// vr_toolgun_brush.hpp -- the toolgun's Brush tool (docs/vr-port/BRUSH_TOOL.md, TOOLGUN.md): boxes of the map's own
// kind (vr_brushes.cpp) made, moved, resized, painted, copied and removed with the gun, everything on a grid
// (vr_toolgun_brush_grid). Its choice is vr_toolgun_brush_mode:
//   Create  the trigger on a surface starts a box's base there (its corner on the grid, on the surface's plane), held
//           and pulled the base follows the aim on that plane; let go, the box rises from it as the aim rises (a grid
//           step at least); the trigger again makes it;
//   Move    the trigger held on a box drags it at its distance (the sticks with X/A held: nearer, further), in grid
//           steps; let go, it is there;
//   Resize  a box's six faces have handles: the trigger held on one pulls that face in or out along its axis;
//   Paint   the trigger on a box gives it the texture chosen on the panel (the whole box, or the face shot: Whole
//           Brush); Pick: the next shot takes the texture of the surface it hits (the world's or a box's);
//   Copy    the trigger on a box takes a copy of it; then a ghost of it sits where the gun points and each trigger
//           puts one there;
//   Delete  the trigger on a box removes it.
// A box is never made, moved or grown over the player or a monster (it would hold them inside).

#pragma once

#include "vr_engine.hpp"

namespace qvr::toolgun::brushtool
{

enum class Mode : int
{
    Create,
    Move,
    Resize,
    Paint,
    Copy,
    Delete,
    Count
};

// What the toolgun sees this frame (vr_toolgun.cpp's aim).
struct Input
{
    bool valid{false};
    glm::vec3 from{0.f}, dir{0.f}; // the gun's muzzle and its way (unit)
    float worldDistance{0.f};      // the first solid along it (the world or a brush entity: Quake's point trace)
    glm::vec3 normal{0.f};         // ... its surface (zero: nothing within range)
    int hitEntity{0};              // ... the entity it is (0: the world)
    int target{0};                 // the entity whose box the aim meets first (0: none)
    float targetDistance{0.f};
    bool trigger{false};           // held
    bool fire{false};              // pressed since the last frame
    glm::vec3 head{0.f};           // where the texts face
};

// Once a host frame while the Brush tool is in hand (the server's VM pushed): its work and its ghosts.
void frame(const Input& in);
// X/A held, the gun's stick forward and back (units this frame): what Move holds nearer or further.
void push(float units);
// Its holds let go (another tool, the gun put down, a new world).
void reset();
// The entity the tool lights up (0: none), and how (the toolgun's glow: 0..1 a hue, 2..3 red).
[[nodiscard]] int glowEntity(float& glow);

[[nodiscard]] int modeCount();
[[nodiscard]] const char* modeName(int mode);
// The next shot (any mode) takes the texture it hits as the one boxes are made and painted with.
void pickTexture();
[[nodiscard]] bool picking();
// The gun's screen ("BRUSH CREATE") and the panel's line.
[[nodiscard]] const char* screenText();
[[nodiscard]] const char* stateLine();

void registerCommands();

} // namespace qvr::toolgun::brushtool
