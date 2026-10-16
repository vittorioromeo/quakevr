// vr_rangerhead.hpp -- the body's head (progs/vrbody*, vr_avatar.cpp) made of the Quake ranger's (vr_body_ranger_head).
//
// The body ships with a placeholder head (make_vrbody.py: a loft weighted wholly to the head joint). With the option on,
// the ranger's own head is taken from the player's progs/player.mdl as the body loads, nothing of it shipped or cached
// on disk: the first player.mdl in the search path that is Quake's ranger (its vertex and triangle counts, its standing
// height, a frame "stand1": a mod's own player model, or Quake VR's quakevr/progs/player.mdl, is passed over for the
// next one, id1's pak), its triangles wholly above the neck in that frame (the neck's opening capped). Then:
//   - the mesh (VR_RangerHeadMesh, Mod_LoadMD5MeshModel): the placeholder's head-only triangles dropped, the ranger's
//     added, weighted wholly to the head joint: hidden from your eyes as the placeholder was (the eye views collapse the
//     neck and head, vr_avatar.cpp solveTorso), seen in the mirror, the third-person and body-tracking views and the
//     shadows. As wide as the placeholder, centred over it, its bottom just below the placeholder's (the neck's top
//     ring inside it).
//   - the skins (VR_RangerHeadImage, Image_LoadImage: every load and reload of the files): the placeholder's head
//     block of the body's 16 shared skins (progs/vrbody_NN_00, above the neck's texels) painted with the ranger's
//     texels (Quake's palette, nearest: his front half left, his back half right); their normal maps flat there,
//     their skin masks (make_skin_masks.py) skin where his texels are skin colours (vr_skin_tone tints his face).
// Taken once a session, the option latched then (the skins are painted once): a change shows at the next start. No
// ranger found, or one unlike Quake's: the placeholder head, as with the option off. ROUND22.md, "The ranger's head".

#pragma once

// VR_RangerHeadMesh and VR_RangerHeadImage (gl_model.c, image.c) are declared in vr_api_render.h.
