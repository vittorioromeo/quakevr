// vr_barrelmask.hpp -- each gun's barrel mask (ROUND23.md, "Barrel glow: the fire frames, the revolver, a painted
// barrel"): the triangles of its model that glow when its barrel is hot (vr_hotblade.cpp: the shader's Barrel), and the
// paint mode that marks them (Weapon Offsets > Effects > Paint Barrel Mask).
//
// The mask is the weapon key fx_gfx_mask (vr_weapons.inc: saved as the other per-weapon settings, vr_wofs_fx_gfx_mask_NN;
// its shipped value in quakevr/data/weapons.txt, pre-marked by Misc/quakevr/barrel_masks.py): "0" none (the glow's
// ramp alone, from the muzzle back Barrel Glow Length), else "<triangles>:<bits>", one bit a triangle of the gun's own
// model in its drawing order, six a character (A-Z a-z 0-9 - _, the lowest bit first). A model with another triangle
// count (a mod's) reads it as none. A part drawn in the gun's place (the auto pump's pieces, the open super shotgun's,
// the revolver's frame, crane and cylinder: view::gunPartHand) takes each triangle's bit from the gun's triangle in the
// same place (its rest pose's middle; the nearest within a unit).
//
// The paint mode (vr_barrelpaint [main|off]): the other hand points at the held gun; its trigger paints the triangles
// under the brush, its grip erases them; its stick up/down sizes the brush. The mask is shown flat (cyan) on the gun,
// the brush as a ring where the ray meets it. A panel ahead (touch a button, pull the trigger): Undo, Clear,
// Pre-marked (the shipped mask), Cancel, Done. B/Y undoes, the menu button is Done. Each stroke is written to the key
// at once (Cancel puts back the mask it started with).

#pragma once

#include "vr_engine.hpp"
#include "vr_posing.hpp"

namespace qvr::barrelmask
{

void init(); // the commands

// Instance data (vr_hotblade.cpp bladePlane): 1 + the first word in the alias shaders' BarrelMasks (SSBO 16) of the
// triangle bits of `drawn` for weapon slot `slot`'s mask on its gun model `gun`; 0: no mask (the ramp alone).
[[nodiscard]] float gpuMask(const qmodel_t* drawn, const qmodel_t* gun, int slot);

// R_FlushAliasInstances: the masks bound for the alias shaders (SSBO 16).
void bind();

// Paint mode.
[[nodiscard]] bool active();
[[nodiscard]] bool painting(int hand); // hand `hand`'s gun has its mask painted now (shown flat)
bool start(int hand, int returnPage);  // False (said why) if it can't
void finish(bool keep);
bool button(int hand, posing::Button b, bool down); // true: taken, not a key
void sticks(const glm::vec2& off, const glm::vec2& main);
void frame(); // once a frame (VR_BeginFrame, with the other editors)
[[nodiscard]] bool buttonPoint(int index, glm::vec3& out); // the panel's button `index` (vr_mock_hand_to H barrelbutton I)

} // namespace qvr::barrelmask
