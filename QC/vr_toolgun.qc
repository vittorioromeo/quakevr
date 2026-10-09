// The toolgun (WID_TOOLGUN; docs/vr-port/TOOLGUN.md): a debug and sandbox tool held as a pistol, as Garry's Mod's. Its
// model is our own (Misc/quakevr/make_toolgun.py: progs/v_toolgun.mdl), its weapon settings slot 26 (Quake/vr/
// vr_weapons.inc). It has no ammo and fires nothing here (weapons.qc W_AttackImpl): the engine takes its hand's buttons
// (Quake/vr/vr_toolgun.cpp): the trigger uses the chosen tool on what it points at, B/Y opens its menu, X/A held turns
// the sticks into the tool's offsets. It is a weapon as any other otherwise: held, holstered, dropped, thrown, taken.
//
// Placing one: a `weapon_toolgun` entity in a map, `func_weapon_grabbable` with weapon 19, `vr_physics_spawn
// weapon_toolgun`, impulse 169 (main hand) or 189 (off hand); vrfiringrange has one in the prop area.

void() VR_Toolgun_Precache =
{
    precache_model("progs/v_toolgun.mdl");
};

/*QUAKED weapon_toolgun (0 .5 .8) (-16 -16 0) (16 16 32)
The toolgun lying ready to be picked up (Quake VR): a debug and sandbox tool (docs/vr-port/TOOLGUN.md). It falls from its
origin and settles on the floor. "angle" or "angles": how it lies (none: any way).
*/
void() weapon_toolgun =
{
    vector a = self.angles;
    if(a == '0 0 0')
    {
        a = randomVec(180);
    }
    CreateThrownWeapon(self, WID_TOOLGUN, 0 /* weaponflags */, WeaponInst_New(0),
        self.origin, a, '0 0 0', '0 0 0',
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(WID_TOOLGUN));
    remove(self);
};

void(entity e) VR_Scene_Remove; // (vr_cheats.qc)

// The toolgun's remover (vr_toolgun.cpp, its trigger on what glows red): `e` gone as the scene's cleanup removes its kind
// (a fire put out, a head's flies silenced, a player's old body emptied). Never the world nor a player.
void(entity e) VR_Toolgun_Remove =
{
    if(e == world || (e.flags & FL_CLIENT))
    {
        return;
    }
    VR_Scene_Remove(e);
};
