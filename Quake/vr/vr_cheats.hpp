// vr_cheats.hpp -- Debug > Cheats and Recording's commands (docs/vr-port/ROUND21.md, "Cheats and Recording"): a scene
// cleaned for footage, and what is left to clean counted.
//
// vr_scene_clean <gibs | corpses | props | fires | decals | wounds | effects | all>...: the server's part (gibs, heads,
// limbs, small gibs, the stumps' fountains; dead monsters, their ragdolls, the players' old bodies; the loose props made
// since the map loaded; the fires) is QC's VR_Scene_Clean (vr_cheats.qc), which removes each as the game does; the
// client's (the decals and the gore's pools, the wounds painted on the models and on you, the particles, casings,
// explosion debris and smoke) is cleared here. Allowed where Quake's cheats are (god, noclip): not in deathmatch.
// vr_scene_count: both counted, nothing removed.
//
// vr_freeze_monsters 1: the living monsters stand frozen (their thinking and moving skipped, their next think kept as
// far off: SV_Physics); vr_shot_hide (vr_view.cpp): the gadget, the hands, the body hidden for a clean shot.
#pragma once

namespace qvr::cheats
{

// vr_scene_clean, vr_scene_count (client commands: forwarded to the server, run by it as god is).
void registerCommands();

// One line for the menu: the cheats on (god, noclip, notarget, fly: a local server's player) and the powerups held.
[[nodiscard]] const char* stateLine();

} // namespace qvr::cheats
