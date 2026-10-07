// vr_cheats.qc -- Debug > Cheats and Recording (the engine's vr_menu.cpp, vr_cheats.cpp; docs/vr-port/ROUND21.md,
// "Cheats and Recording"): the powerups switched on and off, full health and armour, infinite ammunition
// (vr_cheat_ammo), and a scene cleaned for footage (vr_scene_clean, vr_scene_count: VR_Scene_Clean). Cheats where
// Quake's are (god, noclip, impulse 9): not in deathmatch.

// A powerup (impulses 190 to 193): on for an hour, or, while on, off at once (CheckPowerups ends it the next frame,
// without the "almost burned out" warning and flashes).
void(.float xFinished, .float xTime, float xIid, string xName) VR_Cheat_TogglePowerup =
{
    if(deathmatch)
    {
        return;
    }
    if(self.xFinished > time)
    {
        self.xFinished = time - 0.1;
        self.xTime = time + 10;
        sprint(self, xName);
        sprint(self, " OFF\n");
        return;
    }
    self.xTime = 1;
    self.xFinished = time + 3600;
    VR_ItemUtil_EntAddItem(self, xIid);
    sprint(self, xName);
    sprint(self, " ON\n");
};

// Impulse 194: full health and the red armour.
void() VR_Cheat_HealthArmor =
{
    if(deathmatch)
    {
        return;
    }
    self.health = max(self.health, 100);
    self.armortype = 0.8;
    self.armorvalue = 200;
    VR_ItemUtil_EntDelItem(self, IID_ARMOR1);
    VR_ItemUtil_EntDelItem(self, IID_ARMOR2);
    VR_ItemUtil_EntAddItem(self, IID_ARMOR3);
    sprint(self, "Full health and red armour\n");
};

// Infinite ammunition (vr_cheat_ammo; PlayerPostThink): the ammunition kept as impulse 9 fills it (the mission packs'
// where their projectiles are there), and the magazine of a weapon in either hand full. The enemy guns' own magazines
// and the chainsaws' fuel are not.
void(float xHand) VR_Cheat_FillClip =
{
    if(!VRIsWeaponReloadingEnabledFor(self, xHand) || VRIsAmmolessWeapon(xHand))
    {
        return;
    }
    float size = WeaponIdToBaseClipSize(VRGetWeapon(xHand));
    if(size > 0 && VRGetWeaponClip(xHand) < size)
    {
        VRSetWeaponClip(xHand, size);
    }
};

void() VR_Cheat_AmmoFrame =
{
    if(deathmatch || !cvar_hget(cvarh_vr_cheat_ammo))
    {
        return;
    }
    float before = self.ammo_shells + self.ammo_nails + self.ammo_rockets + self.ammo_cells;
    self.ammo_shells = max(self.ammo_shells, 100);
    self.ammo_nails = max(self.ammo_nails, 200);
    self.ammo_rockets = max(self.ammo_rockets, 100);
    self.ammo_cells = max(self.ammo_cells, 200);
    if(VR_Pack_AmmoAvailable(AID_LAVA_NAILS)) { self.ammo_lava_nails = max(self.ammo_lava_nails, 200); }
    self.ammo_multi_rockets = max(self.ammo_multi_rockets, 100);
    if(VR_Pack_AmmoAvailable(AID_PLASMA)) { self.ammo_plasma = max(self.ammo_plasma, 100); }
    VR_Cheat_FillClip(cVR_MainHand);
    VR_Cheat_FillClip(cVR_OffHand);
    if(self.ammo_shells + self.ammo_nails + self.ammo_rockets + self.ammo_cells != before)
    {
        updateAmmoCounters();
    }
};

// ----------------------------------------------------------------------------
// The scene cleaned (vr_scene_clean; vr_cheats.cpp calls VR_Scene_Clean for the player who asked)
// ----------------------------------------------------------------------------

float VR_SCENE_GIBS = 1;    // gibs, heads, severed limbs, small gibs and brains, the stumps' fountains
float VR_SCENE_CORPSES = 2; // dead monsters (and their ragdolls), the players' old bodies
float VR_SCENE_PROPS = 4;   // the loose props made since the map loaded (dropped weapons, backpacks, crate pieces...)
float VR_SCENE_FIRES = 8;   // the fires on bodies and props (vr_burning.qc)

// Made since the map loaded (.vr_born, set by the engine as each entity is made): the map's own entities and those placed
// as it loads (the crates, the rocks and bricks) were all made in its first moments (time starts at 1).
float VR_SCENE_LOADED = 1.5;

// A loose prop made since the map loaded: a physics object, a weapon, a backpack or a piece lying about (not a
// monster, a player, a missile in flight, a mover, nor anything without a model: the triggers' helpers).
float(entity e) VR_Scene_IsRuntimeProp =
{
    if(e.vr_born <= VR_SCENE_LOADED || !e.model)
    {
        return FALSE;
    }
    if((e.flags & (FL_MONSTER | FL_CLIENT)) || e.solid == SOLID_BSP)
    {
        return FALSE;
    }
    if(e.movetype == MOVETYPE_FLYMISSILE || e.movetype == MOVETYPE_PUSH || e.movetype == MOVETYPE_NOCLIP)
    {
        return FALSE;
    }
    if(e.vr_rigid || e.classname == "thrown_weapon" || e.classname == "vr_crate_piece" || e.model == "progs/backpack.mdl")
    {
        return TRUE;
    }
    return e.movetype == MOVETYPE_TOSS || e.movetype == MOVETYPE_BOUNCE;
};

// Which of VR_SCENE_* `e` is part of (0: none).
float(entity e) VR_Scene_Kind =
{
    if(e.flags & FL_CLIENT)
    {
        return 0;
    }
    if(e.classname == "vr_burn")
    {
        return VR_SCENE_FIRES;
    }
    if(e.classname == "vr_limb_fountain" || e.classname == "vr_decap_fountain")
    {
        // (A stump's: with the gibs, and with the corpse it spouts from.)
        if((e.owner.flags & FL_MONSTER) && e.owner.health <= 0)
        {
            return VR_SCENE_GIBS | VR_SCENE_CORPSES;
        }
        return VR_SCENE_GIBS;
    }
    if(e.vr_gib || e.classname == "vr_smallgib" || e.classname == "vr_limb")
    {
        return VR_SCENE_GIBS;
    }
    if((e.flags & FL_MONSTER) && e.health <= 0)
    {
        return VR_SCENE_CORPSES;
    }
    if(e.classname == "bodyque" && e.modelindex)
    {
        return VR_SCENE_CORPSES;
    }
    if(VR_Scene_IsRuntimeProp(e))
    {
        return VR_SCENE_PROPS;
    }
    return 0;
};

// `e` gone as the game removes its kind: its fire out, a head's flies silenced, a small gib off the ring's count; a
// player's old body (one of the four bodyque entities: reused) only emptied.
void(entity e) VR_Scene_Remove =
{
    if(e.classname == "vr_burn")
    {
        VR_Burn_Out(e, "cleared");
        return;
    }
    if(e.burn_fire)
    {
        VR_Burn_Out(e.burn_fire, "cleared");
    }
    VR_Head_StopFlies(e);
    if(e.classname == "vr_smallgib")
    {
        VR_SmallGib_Remove(e);
        return;
    }
    if(e.classname == "bodyque")
    {
        e.model = string_null;
        e.modelindex = 0;
        e.frame = 0;
        return;
    }
    e.nextthink2 = 0;
    remove(e);
};

// vr_scene_clean / vr_scene_count for `self`: what `xWhat` (VR_SCENE_* added up) names, counted, and unless
// `xCountOnly`, removed (a thing held in a hand is kept); the counts printed to him.
void(float xWhat, float xCountOnly) VR_Scene_Clean =
{
    float gibs = 0, heads = 0, limbs = 0, sgibs = 0, brains = 0, fountains = 0, corpses = 0, props = 0;
    float fires = 0, held = 0, removed = 0;
    entity e = nextent(world);
    while(e)
    {
        entity n = nextent(e);
        float kind = VR_Scene_Kind(e);
        if(kind)
        {
            if(kind & VR_SCENE_FIRES)        { fires++; }
            else if((kind & VR_SCENE_CORPSES) && !(kind & VR_SCENE_GIBS)) { corpses++; }
            else if(e.classname == "vr_smallgib") { sgibs++; if(e.vr_sgib_brain) { brains++; } }
            else if(e.classname == "vr_limb") { limbs++; }
            else if((kind & VR_SCENE_GIBS) && e.classname != "vr_limb_fountain" && e.classname != "vr_decap_fountain")
            {
                gibs++;
                if(e.vr_was_monster || e.vr_flies) { heads++; }
            }
            else if(kind & VR_SCENE_GIBS) { fountains++; }
            else                             { props++; }

            if(!xCountOnly && (kind & xWhat))
            {
                if(e.carry_player != world)
                {
                    held++;
                }
                else
                {
                    VR_Scene_Remove(e);
                    removed++;
                }
            }
        }
        e = n;
    }
    sprint(self, sprintf("scene%s: gibs %g (heads %g), limbs %g, small gibs %g (brains %g), ", xCountOnly ? "" : " before",
                         gibs, heads, limbs, sgibs, brains));
    sprint(self, sprintf("fountains %g, corpses %g, props made since the map loaded %g, fires %g\n", fountains, corpses,
                         props, fires));
    if(!xCountOnly)
    {
        sprint(self, sprintf("scene: %g removed%s\n", removed, held ? sprintf(", %g held in a hand kept", held) : ""));
    }
};
