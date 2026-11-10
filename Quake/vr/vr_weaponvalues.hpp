// vr_weaponvalues.hpp -- the named per-weapon values (vr_weaponvalues.inc): include it only where one is read, so an
// edit of the table recompiles those files alone. The weapon table itself: vr_weapons.hpp.

#pragma once

#include "vr_cvars.hpp" // weaponSlotCount (kept there: gen_vr_cvars_qc reads it from that file)

namespace qvr
{

// The named per-weapon values (design review 3, step 5: quakevr/data/weapons.txt's reload_* keys, vr_weaponvalues.inc):
// no cvars, each the stand-in of its slot's key in the weapon table (weaponStandIns: vr_weapons.cpp's DataTable keeps
// its values there, slot by slot) named as its old cvar (vr_reload_port_nail_x, ...), read as one (name.value, &name).
enum WeaponKeyId : int
{
#define QVR_WEAPON_KEY(e, k) weaponKeyId_##e,
#include "vr_weapons.inc"
#undef QVR_WEAPON_KEY
    weaponKeyCount
};
extern cvar_t weaponStandIns[weaponSlotCount * weaponKeyCount]; // (vr_weapons.cpp)
#define QVR_WEAPON_VALUE(name, slot, key) \
    inline constexpr cvar_t& name = weaponStandIns[(slot) * weaponKeyCount + weaponKeyId_##key];
#include "vr_weaponvalues.inc"
#undef QVR_WEAPON_VALUE

} // namespace qvr
