// vr_emissive.hpp -- things that glow light up what is round them, and look lit: monsters' glowing
// projectiles (hell knight flames, scrag spit, vore balls, enforcer and laser cannon lasers) carry
// coloured dynamic lights and flash where they hit (vr_projectile_lights); Rogue's lava nails burn
// bright, streak embers and light the room, the nearest vr_lavanail_lights of them; lightning beams
// light the room along their length (vr_beam_lights, a stream of flickering lights); torches and
// flames (static flame models) flicker a small warm light onto the room, the nearest
// vr_torch_lights in view (VR_TorchLights, round 17); the weapons' ammo screens
// cast a small light in their colour the way they face (vr_weapon_screen_light); the held weapons' dim fullbright
// texels (the shotgun's red sights) shine brighter, so that the bloom makes them glow
// (vr_weapon_glow). All small and unshadowed (lighting::dlightNoShadow); the "Off (Quake)" preset
// turns them off.

#pragma once

#include "vr_engine.hpp"

namespace qvr::emissive
{

// vr_view.cpp, as a weapon's ammo screen is queued: its light, in front of the screen centred at
// `pos` and facing along `angles` (Quake angles; the readable side faces right x up).
void weaponScreenLight(int hand, const glm::vec3& pos, const glm::vec3& angles);

} // namespace qvr::emissive
