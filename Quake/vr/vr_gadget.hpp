// vr_gadget.hpp -- the wrist gadget (vr_hud_mode 1): a device strapped over the back of the off
// hand's forearm (progs/vrgadget.mdl, from Misc/quakevr/make_gadget.py) whose screen shows the
// HUD: the player's face, health and armour, ammo, keys, powerups and sigils, the level, kills
// and secrets. Raise the wrist and turn it to read it, like a watch. Its screen is a small
// monochrome CRT (vr_gadget_crt) that glows (vr_screen_glow) and casts a light in its colour the
// way it faces (vr_gadget_light), and the console's messages float over it in a small log facing
// the player (vr_notify_wrist), rather than at the edge of the view.

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

// Set by the view every frame (before each eye's scene). It also places the screen's lights.
void setPose(const Pose& pose);
[[nodiscard]] const Pose& pose();

// The screen's corners (in model space: the lower-left one, and its extent along x and y), where
// its texture is drawn over the model's screen.
void screenRect(glm::vec3& corner, glm::vec2& size);

// Draws the screen's contents into its texture, at the end of the 2D pass (and the weapons' ammo
// screens' into theirs: text3d::renderScreens).
void renderScreen();

// How much a CRT screen glitches at `time` (0..1, the CRT look's): now and then a short burst.
// The ammo screens' offset their time, so that they glitch at other moments.
[[nodiscard]] float glitch(double time);

// How much the text, numbers and icons on the CRT screens (the gadget's and the ammo screens') glow
// (vr_screen_text_glow, 0..3; Shade::Screen's glow).
[[nodiscard]] float textGlow();

// Draws the texture over the model's screen, in one phosphor colour and as a small CRT
// (vr_gadget_crt: scanlines, a flicker, faint static and now and then a glitch), in each eye's
// scene after the opaque entities (VR_DrawSceneOpaque), where the bloom catches its light.
void drawScreen();

// A soft glow round a screen's edge (vr_screen_glow): the screen's centre, right and up (unit
// vectors), its half width and height, how far out the glow reaches, its colour and strength.
struct Glow
{
    glm::vec3 centre{0.f};
    glm::vec3 right{1.f, 0.f, 0.f};
    glm::vec3 up{0.f, 1.f, 0.f};
    glm::vec2 halfSize{0.f};
    float spread{0.f};
    glm::vec4 color{0.f};
};

// The gadget screen's glow, drawn by vr_text3d with the weapons' ammo screens'; false when there
// is none.
[[nodiscard]] bool screenGlow(Glow& out);

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
