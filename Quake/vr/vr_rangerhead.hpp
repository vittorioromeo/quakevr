// vr_rangerhead.hpp -- the body's head (progs/vrbody*, vr_avatar.cpp) made of the Quake ranger's (vr_body_ranger_head).
//
// The body ships with a placeholder head (make_vrbody.py: a loft weighted wholly to the head joint). With the option on,
// the ranger's own head is taken from the player's progs/player.mdl as the body loads, nothing of it shipped or cached
// on disk: the first progs/player.mdl that is Quake's ranger (its vertex and triangle counts, its standing height, a
// frame "stand1", the head's size), the game folder's first, then id1's, then the other folders' (a mod's own player
// model, or Quake VR's quakevr/progs/player.mdl, is passed over: id1's pak's then), its triangles wholly above the
// neck in stand1 (the neck's opening capped). Then:
//   - the mesh (VR_RangerHeadMesh, Mod_LoadMD5MeshModel): the placeholder's head-only triangles dropped, the ranger's
//     added, weighted wholly to the head joint: hidden from your eyes as the placeholder was (the eye views collapse the
//     neck and head, vr_avatar.cpp solveTorso), seen in the mirror, the views from outside the head (vr_mock_camera,
//     a third-person camera, the body preview) and the shadows. 1.2 times as wide as the placeholder, centred over
//     it, its bottom 4 cm below the placeholder's; the neck's top ring narrowed to the neck's width and given the
//     neck's skin, inside it.
//   - the skins (VR_RangerHeadImage, Image_LoadImage: every load and reload of the files): the placeholder's head
//     block of the body's 16 shared skins (progs/vrbody_NN_00, above the neck's texels) painted with the ranger's
//     texels (Quake's palette, nearest: his front half left, his back half right); their normal maps flat there,
//     their skin masks (make_skin_masks.py) skin where his texels are skin colours (vr_skin_tone tints his face).
// Taken once a session, the option latched then (the skins are painted once): a change shows at the next start. No
// ranger found, or one unlike Quake's: the placeholder head, as with the option off. ROUND22.md, "The ranger's head".

// Placed live by vr_body_head_* (Appearance > Ranger Head): turned, sized and moved about the bottom of his head, the
// neck's top sunk with him; each body mesh's vertices and vertex buffer rewritten as a setting changes.

#pragma once

// VR_RangerHeadMesh and VR_RangerHeadImage (gl_model.c, image.c) are declared in vr_api_render.h.

namespace qvr::rangerhead
{

void init();   // the vr_body_head_* callbacks
void info_f(); // vr_body_head_info: the option, the meshes placed, where his head and the neck's top are

} // namespace qvr::rangerhead
