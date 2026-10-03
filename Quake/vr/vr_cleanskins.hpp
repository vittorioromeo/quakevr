// vr_cleanskins.hpp -- clean weapon skins (vr_gore_clean_skins; Gore > Blood on You and Your Gear > Clean Weapon
// Skins). Some held weapons have blood painted into their skins (the axe, the knights' swords, the ogre's chainsaw,
// the grunt's shotgun); the blood gear takes is dynamic now (vr_wounds.cpp), so they start clean and are clean again
// after a wash. A skin's clean version is built as it loads, from the player's own file, by a patch beside it:
// progs/<model>_<skin>.clean (an 8-bit skin) or progs/<model>_<skin>_hq.clean (a full-colour replacement), a list of
// (texel, texel to copy into it) pairs for the pixels whose hash it names (Misc/quakevr/make_clean_skins.py makes
// them). It holds no pixel of the art; a file other than the one it was made for is left as it is. ROUND21.md,
// "Clean weapon skins".
//

#pragma once

// VR_CleanSkin (gl_texmgr.c calls it) is declared in vr_api_render.h.

namespace qvr::cleanskins
{

void init();   // vr_gore_clean_skins' callback (the skins with a patch uploaded again), the commands
void list_f(); // vr_cleanskins: the patches looked for, found, applied

} // namespace qvr::cleanskins
