float(float wid) IsValidWeaponId =
{
    return wid >= WID_FIST
        && wid <= WID_LASER_CANNON;
}

float(float wid, float wpnFlags) PlayerHasWeapon =
{
    if(!IsValidWeaponId(wid))
    {
        bprint("`PlayerHasWeapon`: invalid weapon id '");
        bprint(ftos(wid));
        bprint("'\n");

        return FALSE;
    }

    if(wid == WID_FIST) return TRUE;
    if(wid == WID_GRAPPLE) return cvar("vr_enable_grapple");

    return wpnFlags & WeaponIdToItemId(wid);
}

float(entity ent, float wid) PlayerEligibleForWeapon =
{
    if(wid == WID_FIST)             { return TRUE; }
    if(wid == WID_AXE)              { return TRUE; }
    if(wid == WID_SHOTGUN)          { return TRUE; }

    return PlayerHasWeapon(wid, ent.items);
}

float() getEnemyDrops =
{
    return cvar("vr_enemy_drops");
}

float() getAmmoBoxDrops =
{
    return cvar("vr_ammobox_drops");
}

entity() FindEntPlayerForDrop =
{
    if(getEnemyDrops() == QVR_ENEMYDROPS_DISABLE)
    {
        return world;
    }

    return find(world, classname, "player");
}

float(entity entPlayer, float xWeapon, float xChance) TryEnemyDrop =
{
    local float eligible, chance, roll;

    if(getEnemyDrops() == QVR_ENEMYDROPS_DISABLE)
    {
        return FALSE;
    }

    // TODO VR: (P2) bitwise or is the only one that works here, || does not work. Why?
    eligible = (getEnemyDrops() == QVR_ENEMYDROPS_ALWAYS) |
               PlayerEligibleForWeapon(entPlayer, xWeapon);

    chance = xChance * cvar("vr_enemy_drops_chance_mult");

    roll = random();

    // bprint("weapon: ");
    // bprint(ftos(xWeapon));
    // bprint(" | eligible ps: ");
    // bprint(ftos(getEnemyDrops() == QVR_ENEMYDROPS_ALWAYS));
    // bprint(" or ");
    // bprint(ftos(PlayerEligibleForWeapon(entPlayer, xWeapon)));
    // bprint(" | chance: ");
    // bprint(ftos(chance));
    // bprint(" | roll: ");
    // bprint(ftos(roll));
    // bprint(" | eligible: ");
    // bprint(ftos(eligible));
    // bprint(" | rollpass: ");
    // bprint(ftos(roll < chance));
    // bprint(" | cond: ");
    // bprint(ftos(eligible && (roll < chance)));
    // bprint("\n");

    if(eligible && (roll < chance))
    {
        CreateWeaponDrop(self, xWeapon, self.origin);
        return TRUE;
    }

    return FALSE;
}

// TODO VR: (P2) code repetition, but create ammo box uses player ent
float(entity entAmmoBox, entity entPlayer, float xWeapon, float xChance) TryAmmoBoxDrop =
{
    local float eligible, chance, roll;

    if(getAmmoBoxDrops() == QVR_AMMOBOXDROPS_DISABLE)
    {
        return FALSE;
    }

    // TODO VR: (P2) bitwise or is the only one that works here, || does not work. Why?
    eligible = (getAmmoBoxDrops() == QVR_AMMOBOXDROPS_ALWAYS) |
               PlayerEligibleForWeapon(entPlayer, xWeapon);

    chance = xChance * cvar("vr_ammobox_drops_chance_mult");

    roll = random();

    if(eligible && (roll < chance))
    {
        CreateAmmoBoxWeaponDrop(entPlayer, xWeapon, entAmmoBox.origin);
        return TRUE;
    }

    return FALSE;
}

// TODO VR: (P2) what to do with multiple `items` fields?
float(float wid) WeaponIdToItemId =
{
    if(wid == WID_FIST)             { return IT_FIST; }
    if(wid == WID_AXE)              { return IT_AXE; }
    if(wid == WID_MJOLNIR)          { return IT_MJOLNIR; }
    if(wid == WID_SHOTGUN)          { return IT_SHOTGUN; }
    if(wid == WID_SUPER_SHOTGUN)    { return IT_SUPER_SHOTGUN; }
    if(wid == WID_NAILGUN)          { return IT_NAILGUN; }
    if(wid == WID_SUPER_NAILGUN)    { return IT_SUPER_NAILGUN; }
    if(wid == WID_GRENADE_LAUNCHER) { return IT_GRENADE_LAUNCHER; }
    if(wid == WID_PROXIMITY_GUN)    { return IT_PROXIMITY_GUN; }
    if(wid == WID_ROCKET_LAUNCHER)  { return IT_ROCKET_LAUNCHER; }
    if(wid == WID_LIGHTNING)        { return IT_LIGHTNING; }
    if(wid == WID_LASER_CANNON)     { return IT_LASER_CANNON; }
    // TODO VR: (P1) fist and grapple

    bprint("`WeaponIdToItemId`: invalid weapon id '");
    bprint(ftos(wid));
    bprint("'\n");

    return -1;
}

// TODO VR: (P2) what to do with multiple `items` fields?
float(float it) ItemIdToWeaponId =
{
    if(it == IT_FIST)             { return WID_FIST; }
    if(it == IT_AXE)              { return WID_AXE; }
    if(it == IT_MJOLNIR)          { return WID_MJOLNIR; }
    if(it == IT_SHOTGUN)          { return WID_SHOTGUN; }
    if(it == IT_SUPER_SHOTGUN)    { return WID_SUPER_SHOTGUN; }
    if(it == IT_NAILGUN)          { return WID_NAILGUN; }
    if(it == IT_SUPER_NAILGUN)    { return WID_SUPER_NAILGUN; }
    if(it == IT_GRENADE_LAUNCHER) { return WID_GRENADE_LAUNCHER; }
    if(it == IT_PROXIMITY_GUN)    { return WID_PROXIMITY_GUN; }
    if(it == IT_ROCKET_LAUNCHER)  { return WID_ROCKET_LAUNCHER; }
    if(it == IT_LIGHTNING)        { return WID_LIGHTNING; }
    if(it == IT_LASER_CANNON)     { return WID_LASER_CANNON; }
    // TODO VR: (P1) fist and grapple

    bprint("`ItemIdToWeaponId`: invalid item id '");
    bprint(ftos(it));
    bprint("'\n");

    return -1;
}

string(float wid) WeaponIdToModel =
{
    if(wid == WID_FIST)             { return "progs/hand.mdl"; }
    if(wid == WID_GRAPPLE)          { return "progs/v_grpple.mdl"; }
    if(wid == WID_AXE)              { return "progs/v_axe.mdl"; }
    if(wid == WID_MJOLNIR)          { return "progs/v_hammer.mdl"; }
    if(wid == WID_SHOTGUN)          { return "progs/v_shot.mdl"; }
    if(wid == WID_SUPER_SHOTGUN)    { return "progs/v_shot2.mdl"; }
    if(wid == WID_NAILGUN)          { return "progs/v_nail.mdl"; }
    if(wid == WID_SUPER_NAILGUN)    { return "progs/v_nail2.mdl"; }
    if(wid == WID_GRENADE_LAUNCHER) { return "progs/v_rock.mdl"; }
    if(wid == WID_PROXIMITY_GUN)    { return "progs/v_prox.mdl"; }
    if(wid == WID_ROCKET_LAUNCHER)  { return "progs/v_rock2.mdl"; }
    if(wid == WID_LIGHTNING)        { return "progs/v_light.mdl"; }
    if(wid == WID_LASER_CANNON)     { return "progs/v_laserg.mdl"; }

    bprint("`WeaponIdToModel`: invalid weapon id '");
    bprint(ftos(wid));
    bprint("'\n");

    return "";
}

float(float wid) WeaponIdToAmmoId =
{
    if(wid == WID_FIST)             { return AID_NONE; }
    if(wid == WID_GRAPPLE)          { return AID_NONE; }
    if(wid == WID_AXE)              { return AID_NONE; }
    if(wid == WID_MJOLNIR)          { return AID_CELLS; }
    if(wid == WID_SHOTGUN)          { return AID_SHELLS; }
    if(wid == WID_SUPER_SHOTGUN)    { return AID_SHELLS; }
    if(wid == WID_NAILGUN)          { return AID_NAILS; }
    if(wid == WID_SUPER_NAILGUN)    { return AID_NAILS; }
    if(wid == WID_GRENADE_LAUNCHER) { return AID_ROCKETS; }
    if(wid == WID_PROXIMITY_GUN)    { return AID_ROCKETS; }
    if(wid == WID_ROCKET_LAUNCHER)  { return AID_ROCKETS; }
    if(wid == WID_LIGHTNING)        { return AID_CELLS; }
    if(wid == WID_LASER_CANNON)     { return AID_CELLS; }

    bprint("`WeaponIdToAmmoId`: invalid weapon id '");
    bprint(ftos(wid));
    bprint("'\n");

    return AID_NONE;
}

float(entity xPlayer, float xAid) GetAmmoCountByAmmoId =
{
    if(xAid == AID_SHELLS)  { return xPlayer.ammo_shells; }
    if(xAid == AID_NAILS)   { return xPlayer.ammo_nails; }
    if(xAid == AID_ROCKETS) { return xPlayer.ammo_rockets; }
    if(xAid == AID_CELLS)   { return xPlayer.ammo_cells; }

    bprint("`GetAmmoCountByAmmoId`: invalid ammo id '");
    bprint(ftos(xAid));
    bprint("'\n");

    return 0;
}

void(entity xPlayer, float xAid, float v) SetAmmoCountByAmmoId =
{
    if(xAid == AID_SHELLS)  { xPlayer.ammo_shells = v; return; }
    if(xAid == AID_NAILS)   { xPlayer.ammo_nails = v; return; }
    if(xAid == AID_ROCKETS) { xPlayer.ammo_rockets = v; return; }
    if(xAid == AID_CELLS)   { xPlayer.ammo_cells = v; return; }

    bprint("`SetAmmoCountByAmmoId`: invalid ammo id '");
    bprint(ftos(xAid));
    bprint("'\n");
}

float(entity xPlayer, float wid) PlayerHasAmmoForWeapon =
{
    local float ammoId;

    ammoId = WeaponIdToAmmoId(wid);

    if(ammoId == AID_NONE)
    {
        return TRUE;
    }

    return GetAmmoCountByAmmoId(xPlayer, ammoId) > 0;
}

float(float wid) WeaponIdToThrowMult =
{
    // TODO VR: (P2) used for velocity, but weight is used on the calculations
    // on the C++ side of things

    if(wid == WID_FIST)             { return 1; }
    if(wid == WID_GRAPPLE)          { return 0.9; }
    if(wid == WID_AXE)              { return 0.9; }
    if(wid == WID_MJOLNIR)          { return 0.7; }
    if(wid == WID_SHOTGUN)          { return 0.8; }
    if(wid == WID_SUPER_SHOTGUN)    { return 0.8; }
    if(wid == WID_NAILGUN)          { return 0.8; }
    if(wid == WID_SUPER_NAILGUN)    { return 0.6; }
    if(wid == WID_GRENADE_LAUNCHER) { return 0.6; }
    if(wid == WID_PROXIMITY_GUN)    { return 0.6; }
    if(wid == WID_ROCKET_LAUNCHER)  { return 0.6; }
    if(wid == WID_LIGHTNING)        { return 0.6; }
    if(wid == WID_LASER_CANNON)     { return 0.4; }

    bprint("`WeaponIdToThrowMult`: invalid weapon id '");
    bprint(ftos(wid));
    bprint("'\n");

    return 1;
}

float(float wid) WeaponIdToThrowStabilizeDim =
{
    if(wid == WID_FIST)             { return 0; }
    if(wid == WID_GRAPPLE)          { return 0; }
    if(wid == WID_AXE)              { return 2; }
    if(wid == WID_MJOLNIR)          { return 2; }
    if(wid == WID_SHOTGUN)          { return 0; }
    if(wid == WID_SUPER_SHOTGUN)    { return 0; }
    if(wid == WID_NAILGUN)          { return 0; }
    if(wid == WID_SUPER_NAILGUN)    { return 0; }
    if(wid == WID_GRENADE_LAUNCHER) { return 0; }
    if(wid == WID_PROXIMITY_GUN)    { return 0; }
    if(wid == WID_ROCKET_LAUNCHER)  { return 0; }
    if(wid == WID_LIGHTNING)        { return 0; }
    if(wid == WID_LASER_CANNON)     { return 0; }

    bprint("`WeaponIdToThrowStabilizeDim`: invalid weapon id '");
    bprint(ftos(wid));
    bprint("'\n");

    return 0;
}

float(float wid) WeaponIdToThrowDamage =
{
    if(wid == WID_FIST)             { return 20; }
    if(wid == WID_GRAPPLE)          { return 20; }
    if(wid == WID_AXE)              { return 35; }
    if(wid == WID_MJOLNIR)          { return 30; }
    if(wid == WID_SHOTGUN)          { return 20; }
    if(wid == WID_SUPER_SHOTGUN)    { return 20; }
    if(wid == WID_NAILGUN)          { return 20; }
    if(wid == WID_SUPER_NAILGUN)    { return 25; }
    if(wid == WID_GRENADE_LAUNCHER) { return 30; }
    if(wid == WID_PROXIMITY_GUN)    { return 30; }
    if(wid == WID_ROCKET_LAUNCHER)  { return 30; }
    if(wid == WID_LIGHTNING)        { return 25; }
    if(wid == WID_LASER_CANNON)     { return 45; }

    bprint("`WeaponIdToThrowDamage`: invalid weapon id '");
    bprint(ftos(wid));
    bprint("'\n");

    return 0;
}

float(float wid) WeaponIdToThrowBounds =
{
    if(wid == WID_FIST)             { return 1; }
    if(wid == WID_GRAPPLE)          { return 1; }
    if(wid == WID_AXE)              { return 1; }
    if(wid == WID_MJOLNIR)          { return 1.1; }
    if(wid == WID_SHOTGUN)          { return 1.25; }
    if(wid == WID_SUPER_SHOTGUN)    { return 1.25; }
    if(wid == WID_NAILGUN)          { return 1.25; }
    if(wid == WID_SUPER_NAILGUN)    { return 1.5; }
    if(wid == WID_GRENADE_LAUNCHER) { return 1.5; }
    if(wid == WID_PROXIMITY_GUN)    { return 1.5; }
    if(wid == WID_ROCKET_LAUNCHER)  { return 1.7; }
    if(wid == WID_LIGHTNING)        { return 1.5; }
    if(wid == WID_LASER_CANNON)     { return 2.4; }

    bprint("`WeaponIdToThrowBounds`: invalid weapon id '");
    bprint(ftos(wid));
    bprint("'\n");

    return 0;
}

float(string wpnClassname) WeaponClassNameToWeaponId =
{
    if(wpnClassname == "weapon_nailgun")         { return WID_NAILGUN; }
    if(wpnClassname == "weapon_supernailgun")    { return WID_SUPER_NAILGUN; }
    if(wpnClassname == "weapon_supershotgun")    { return WID_SUPER_SHOTGUN; }
    if(wpnClassname == "weapon_rocketlauncher")  { return WID_ROCKET_LAUNCHER; }
    if(wpnClassname == "weapon_grenadelauncher") { return WID_GRENADE_LAUNCHER; }
    if(wpnClassname == "weapon_lightning")       { return WID_LIGHTNING; }
    if(wpnClassname == "weapon_laser_gun")       { return WID_LASER_CANNON; }
    if(wpnClassname == "weapon_mjolnir")         { return WID_MJOLNIR; }
    if(wpnClassname == "weapon_proximity_gun")   { return WID_PROXIMITY_GUN; }

    bprint("`WeaponClassNameToWeaponId`: invalid weapon class name '");
    bprint(wpnClassname);
    bprint("'\n");

    return WID_FIST;
}

float(float wid) WeaponIdToInitialAmmoCount =
{
    if(wid == WID_FIST)             { return 0; }
    if(wid == WID_AXE)              { return 0; }
    if(wid == WID_MJOLNIR)          { return 30; }
    if(wid == WID_SHOTGUN)          { return 5; }
    if(wid == WID_SUPER_SHOTGUN)    { return 5; }
    if(wid == WID_NAILGUN)          { return 30; }
    if(wid == WID_SUPER_NAILGUN)    { return 30; }
    if(wid == WID_GRENADE_LAUNCHER) { return 5; }
    if(wid == WID_PROXIMITY_GUN)    { return 6; }
    if(wid == WID_ROCKET_LAUNCHER)  { return 5; }
    if(wid == WID_LIGHTNING)        { return 15; }
    if(wid == WID_LASER_CANNON)     { return 30; }
    // TODO VR: (P1) fist and grapple

    bprint("`WeaponIdToInitialAmmoCount`: invalid weapon id '");
    bprint(ftos(wid));
    bprint("'\n");

    return -1;
}

// TODO VR: (P1) use for DropBackpack, GremlinDropBackpack, netname, etc
string(float wid) WeaponIdToWeaponName =
{
    if(wid == WID_AXE)              { return "Axe"; }
    if(wid == WID_SHOTGUN)          { return "Shotgun"; }
    if(wid == WID_SUPER_SHOTGUN)    { return "Double-barrelled Shotgun"; }
    if(wid == WID_NAILGUN)          { return "Nailgun"; }
    if(wid == WID_SUPER_NAILGUN)    { return "Super Nailgun"; }
    if(wid == WID_GRENADE_LAUNCHER) { return "Grenade Launcher"; }
    if(wid == WID_ROCKET_LAUNCHER)  { return "Rocket Launcher"; }
    if(wid == WID_LIGHTNING)        { return "Thunderbolt"; }
    if(wid == WID_LASER_CANNON)     { return "Laser Cannon"; }
    if(wid == WID_PROXIMITY_GUN)    { return "Proximity Gun"; }
    if(wid == WID_MJOLNIR)          { return "Mjolnir"; }

    bprint("`WeaponIdToWeaponName`: invalid weapon id '");
    bprint(ftos(wid));
    bprint("'\n");

    return "";
}

string(float it) ItemIdToWeaponName =
{
    // TODO VR: (P2) why? stop using item ids
}
