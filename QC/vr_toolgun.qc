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
    // Its Fire tab's torches and flames (vr_walltorch.qc, vr_spawn_live), with the sounds a wall torch taken makes: a
    // client hears no sound precached after it joined.
    precache_model("progs/flame.mdl");
    precache_model("progs/flame2.mdl");
    precache_model("progs/vrtorch.mdl");
    // (the Fire tab's candle and lantern, where their pak is: small, loaded at their first spawn otherwise, a 3-50 ms frame)
    if(VR_Has("progs/candle.mdl")) { precache_model("progs/candle.mdl"); }
    if(VR_Has("progs/lantern.mdl")) { precache_model("progs/lantern.mdl"); }
    precache_sound("vr/torch_pull.wav");
    precache_sound("vr/torch_out.wav");
    precache_sound("vr/torch_light.wav");
    precache_sound("vr/torch_hit.wav");
    precache_sound("vr/nail_sizzle1.wav"); // (also fire put out by water: vr_burning.qc VR_Burn_Doused)
    precache_sound("vr/nail_sizzle2.wav");
    precache_sound("misc/null.wav");
    // Its brush-model items and boxes (health, ammo, explosive boxes): a brush model's lightmaps and vertices are built
    // with the map, so one first loaded later is never drawn (vr_client.cpp parsePrecacheModel). Small files; the
    // expansions' only where their pak is. Everything else an entry needs is precached late, when it is chosen (the
    // ghost's probe: vr_toolgun.cpp) or made, and reaches the client before the thing (VR_WriteRecentPrecaches).
    precache_model("maps/b_bh10.bsp");
    precache_model("maps/b_bh25.bsp");
    precache_model("maps/b_bh100.bsp");
    precache_model("maps/b_shell0.bsp");
    precache_model("maps/b_shell1.bsp");
    precache_model("maps/b_nail0.bsp");
    precache_model("maps/b_nail1.bsp");
    precache_model("maps/b_rock0.bsp");
    precache_model("maps/b_rock1.bsp");
    precache_model("maps/b_batt0.bsp");
    precache_model("maps/b_batt1.bsp");
    precache_model("maps/b_explob.bsp");
    precache_model("maps/b_exbox2.bsp");
    if(VR_Has("maps/b_lnail0.bsp")) { precache_model("maps/b_lnail0.bsp"); }
    if(VR_Has("maps/b_lnail1.bsp")) { precache_model("maps/b_lnail1.bsp"); }
    if(VR_Has("maps/b_mrock0.bsp")) { precache_model("maps/b_mrock0.bsp"); }
    if(VR_Has("maps/b_mrock1.bsp")) { precache_model("maps/b_mrock1.bsp"); }
    if(VR_Has("maps/b_plas0.bsp")) { precache_model("maps/b_plas0.bsp"); }
    if(VR_Has("maps/b_plas1.bsp")) { precache_model("maps/b_plas1.bsp"); }
    // (AD's golems' rubble: the only brush models an entry's spawn precaches, ad_mon_bossicegolem.qc)
    if(VR_Has("maps/ad_brk/rock01.bsp")) { precache_model("maps/ad_brk/rock01.bsp"); }
    if(VR_Has("maps/ad_brk/rock02.bsp")) { precache_model("maps/ad_brk/rock02.bsp"); }
};

void() MG3_UpgradeStart;
void() VR_PickupObj_Place;
void() AD_SpawnLive_Finish; // (vr_ad_items.qc)

// box3d::spawnClass, right after a live spawn's function (self: what it made; the toolgun, physics spawns): what a map's
// item does over its first second, done now, so it is drawn and touched at its final size and place from its first
// frame (no big box shrinking a moment later): Dawn of the Machine's upgrade started (its half second), placed on the
// floor (StartItem's 0.2 s: PlaceItem; boxes shrunk to objects, pickups scaled and floated), made an object
// (VR_PickupObj_Mark's 0.3 s).
void() VR_SpawnLive_Finish =
{
    if(self.think == MG3_UpgradeStart)
    {
        MG3_UpgradeStart();
    }
    AD_SpawnLive_Finish(); // (AD's items: their delayed start)
    if(!self.classname)
    {
        return; // (removed by its start: an empty backpack, one stuck in a wall)
    }
    if(!VRPlaceItemNow())
    {
        return; // (fell out of the level: removed)
    }
    if(self.think2 == VR_PickupObj_Place)
    {
        VR_PickupObj_Place();
        if(self.think2 == VR_PickupObj_Place)
        {
            self.think2 = SUB_Null;
            self.nextthink2 = 0;
        }
    }
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

// vr_debug_spawn_toolgun (Debug > Toolgun, for tests in the headset): each spawn of `self` (a new level, a respawn), a
// toolgun in the first empty holster (the hips, the chest, the shoulders; right before left) unless one is carried
// already (a hand, a holster). Weapon Mode Immersive (quick slots' holsters keep copies).
void() VR_Toolgun_SpawnInHolster =
{
    if(!cvar_hget(cvarh_vr_debug_spawn_toolgun) || getWeaponMode() != QVR_WEAPONMODE_IMMERSIVE)
    {
        return;
    }
    if(self.weapon == WID_TOOLGUN || self.weapon2 == WID_TOOLGUN)
    {
        return;
    }
    float order0 = QVR_HS_RIGHT_HIP_HOLSTER, order1 = QVR_HS_LEFT_HIP_HOLSTER, order2 = QVR_HS_RIGHT_UPPER_HOLSTER;
    float order3 = QVR_HS_LEFT_UPPER_HOLSTER, order4 = QVR_HS_RIGHT_SHOULDER_HOLSTER, order5 = QVR_HS_LEFT_SHOULDER_HOLSTER;
    float empty = -1;
    for(float i = 0; i < 6; i++)
    {
        float hs = i == 0 ? order0 : (i == 1 ? order1 : (i == 2 ? order2 : (i == 3 ? order3 : (i == 4 ? order4 : order5))));
        float w = getHolsterWeapon(hs);
        if(w == WID_TOOLGUN)
        {
            return; // (carried already)
        }
        if(w == WID_FIST && empty < 0)
        {
            empty = hs;
        }
    }
    if(empty < 0)
    {
        dprint("toolgun: every holster taken: none put in one\n");
        return;
    }
    setHolsterWeapon(empty, WID_TOOLGUN);
    setHolsterWeaponModel(empty, WeaponIdToModel(WID_TOOLGUN, 0));
    setHolsterWeaponFlags(empty, 0);
    VRSetEntHolsterWeaponInst(self, empty, WeaponInst_New(0));
    dprint(sprintf("toolgun: put in holster %g (vr_debug_spawn_toolgun)\n", empty));
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
