// vr_gadget.hpp -- the wrist gadget (vr_hud_mode 1): a device strapped over the back of the off
// hand's forearm (progs/vrgadget.mdl, from Misc/quakevr/make_gadget.py) whose screen shows the
// HUD: the player's face, health and armour, ammo, keys, powerups and sigils, the level, kills
// and secrets. Raise the wrist and turn it to read it, like a watch.

#pragma once

#include <glm/glm.hpp>

namespace qvr::gadget
{

struct Pose
{
    bool valid{false};
    glm::vec3 origin{0.f};
    glm::mat3 axes{1.f}; // the screen's right, up and out
    float scale{1.f};    // world units per model unit
};

// Whether the HUD is the gadget's (vr_hud_mode 1, in the headset, in game).
[[nodiscard]] bool active();

// Set by the view every frame, read by the panel.
void setPose(const Pose& pose);
[[nodiscard]] const Pose& pose();

// The screen's corners (in model space: the lower-left one, and its extent along x and y), for
// the textured quad drawn over the model's screen.
void screenRect(glm::vec3& corner, glm::vec2& size);

// Draws the screen's contents into its texture, at the end of the 2D pass.
void renderScreen();

// The screen's texture (0 before the first render).
[[nodiscard]] unsigned screenTexture();

} // namespace qvr::gadget
