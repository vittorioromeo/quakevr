// vr_bodyclone.hpp -- a mirror image of your body while you tweak what is worn on it (vr_menu_body_clone; ROUND22.md,
// "A clone of the body while tweaking").
//
// The author: "It's really hard to tweak the flashlight head zone because you can't really see it from the first person
// perspective." While a VR menu page tweaks something attached to the body (menu::bodyClonePage: the holsters, the
// pouches, the flashlight's place on the belt and on the head and its zones, the wrist gadget's mount, the virtual stock,
// the pauldrons), the body as drawn this frame (with its head), the hands, the holsters and what is in them, the pouches,
// the gadget and the torch are drawn a second time beside the menu panel, vr_menu_body_clone_distance metres off, facing
// you: reflected about a vertical plane half way, a mirror (your right hand moves the image's hand on your right). The
// zones drawn on the body for the page (body::queueDebug's holsters and pouches, the flashlight's zones) are drawn on the
// image too, at the same places on it, as the sliders move. The mirror stays where it was put while you stand about the
// same place (moved again 0.6 m off, or with the menu panel). Drawn only (no collision, no shadows); the window's
// spectator view sees it too. Nothing is done while no such page is shown.

#pragma once

#include <glm/glm.hpp>

namespace qvr::bodyclone
{

// This frame's mirror: the world to the image's place (a reflection: its determinant is -1). False while no image is
// drawn (vr_menu_body_clone 0, no menu page that tweaks the body, no hands or no menu panel).
[[nodiscard]] bool mirror(glm::mat4& out);

// The view's count of the entities it drew reflected this frame and where the body's went (vr_menu_body_clone_info).
void noteDrawn(int entities, bool body, const glm::vec3& bodyOrigin);

// vr_menu_body_clone_info: whether the image is shown, its mirror, and what was drawn in it last frame.
void info_f();

} // namespace qvr::bodyclone
