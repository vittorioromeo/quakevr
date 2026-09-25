// vr_gadget.hpp -- the wrist gadget (vr_hud_mode 1): a device strapped over the back of the off
// hand's forearm (progs/vrgadget.mdl, from Misc/quakevr/make_gadget.py) whose screen shows the
// HUD: the player's face, health and armour, ammo, keys, powerups and sigils, the level, kills
// and secrets. Raise the wrist and turn it to read it, like a watch. Its screen casts a faint
// light in its colour (vr_gadget_light), and the console's messages float over it in a small log
// facing the player (vr_notify_wrist), rather than at the edge of the view.

#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>

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

// Set by the view every frame (before each eye's scene), read by the panel. It also places the
// screen's light.
void setPose(const Pose& pose);
[[nodiscard]] const Pose& pose();

// The screen's corners (in model space: the lower-left one, and its extent along x and y), for
// the textured quad drawn over the model's screen.
void screenRect(glm::vec3& corner, glm::vec2& size);

// Draws the screen's contents into its texture, at the end of the 2D pass.
void renderScreen();

// The screen's texture (0 before the first render).
[[nodiscard]] unsigned screenTexture();

// The log floating over the gadget (vr_notify_wrist): the console's newest notify lines, wrapped
// to fit, oldest first, each with how faded in it is (1 .. 0); drawn by vr_text3d, only while the
// screen is turned towards the viewer (like a watch, checked by raising the wrist).
struct Log
{
    std::vector<std::string> lines;
    std::vector<float> alpha;
    glm::vec3 base{0.f}; // the screen's centre: the log's bottom edge is `lift` above it (the view's up)
    float lift{0.f};
    glm::vec3 normal{0.f}; // out of the screen
    float charSize{0.f}; // units
    glm::vec3 color{1.f}; // the screen's text colour
    glm::vec3 backColor{0.f};
};

// False when there is nothing to show.
[[nodiscard]] bool log(Log& out);

} // namespace qvr::gadget
