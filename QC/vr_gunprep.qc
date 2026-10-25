//
//
//
// ----------------------------------------------------------------------------
// The held guns' models the map can hand the player (docs/vr-port/ROUND22.md, "Held guns' convex pieces at map load")
// ----------------------------------------------------------------------------
//
// VR_GunPrep_Collect and VR_GunPrep_Model: the engine (vr_box3d.cpp, prepareHeldGuns) asks once the map's entities exist, and makes each
// listed model's convex pieces for a hand holding it (from its cache file, cut only when the disk lacks it), so a
// weapon picked up in play finds them made. The models of: the weapons lying in the map (weapon_*, thrown_weapon,
// func_weapon_grabbable), the guns its monsters drop when they die (grunt, enforcer, knight, hell knight, ogre; AD's
// armed soldiers by VR_ADGun_ForClass), the backpacks' weapons, and the players' hands and holsters. The engine skips
// a name it has not precached; a gun's secondary-ammo model (lava nails, multi-rockets, plasma) is listed where the
// pack has it (WeaponIdToModel).

float vr_gp_seen[32];   // WID_* listed already
float vr_gp_order[32];  // the WID_*s in the order listed
float vr_gp_n;          // how many

void(float wid) VR_GunPrep_Wid =
{
    if(wid < 1 || wid > WID_LAST || vr_gp_seen[wid])
    {
        return;
    }
    vr_gp_seen[wid] = TRUE;
    vr_gp_order[vr_gp_n] = wid;
    vr_gp_n = vr_gp_n + 1;
};

float(string c) VR_GunPrep_ClassWid =
{
    if(c == "weapon_grapple")         { return WID_GRAPPLE; }
    if(c == "weapon_axe")             { return WID_AXE; }
    if(c == "weapon_upgrade_axe")     { return WID_AXE; }
    if(c == "weapon_mjolnir")         { return WID_MJOLNIR; }
    if(c == "weapon_superaxe")        { return WID_SUPERAXE; }
    if(c == "weapon_toolgun")         { return WID_TOOLGUN; }
    if(c == "weapon_nailpistol")      { return WID_NAILPISTOL; }
    if(c == "weapon_revolver")        { return WID_REVOLVER; }
    if(c == "weapon_shotgun")         { return WID_SHOTGUN; }
    if(c == "weapon_supershotgun")    { return WID_SUPER_SHOTGUN; }
    if(c == "weapon_upgrade_ssg")     { return WID_SUPER_SHOTGUN; }
    if(c == "weapon_nailgun")         { return WID_NAILGUN; }
    if(c == "weapon_supernailgun")    { return WID_SUPER_NAILGUN; }
    if(c == "weapon_grenadelauncher") { return WID_GRENADE_LAUNCHER; }
    if(c == "weapon_proximity_gun")   { return WID_PROXIMITY_GUN; }
    if(c == "weapon_rocketlauncher")  { return WID_ROCKET_LAUNCHER; }
    if(c == "weapon_lightning")       { return WID_LIGHTNING; }
    if(c == "weapon_upgrade_lg")      { return WID_LIGHTNING; }
    if(c == "weapon_laser_gun")       { return WID_LASER_CANNON; }
    if(c == "weapon_crowbar")         { return WID_CROWBAR; }
    return 0;
};

// The gun a monster of class `c` (AD's class type `t`) drops when it dies (0: none).
float(string c, float t) VR_GunPrep_MonsterWid =
{
    if(c == "monster_army")                                 { return WID_GRUNTGUN; }
    if(c == "monster_enforcer")                             { return WID_ENFRIFLE; }
    if(c == "monster_knight" || c == "monster_hell_knight") { return WID_SWORD; }
    if(c == "monster_ogre" || c == "monster_ogre_marksman") { return WID_CHAINSAW; }
    if(t) // AD support: an AD monster's gun by its classtype (vr_ad_enemyguns.qc)
    {
        return max(0, VR_ADGun_ForClass(t));
    }
    return 0;
};

float() VR_GunPrep_Collect =
{
    float i;
    for(i = 0; i < 32; i = i + 1)
    {
        vr_gp_seen[i] = FALSE;
    }
    vr_gp_n = 0;
    entity e = nextent(world);
    while(e)
    {
        string c = e.classname;
        if(c == "")
        {
            e = nextent(e);
            continue;
        }
        if(substring(c, 0, 7) == "weapon_")
        {
            VR_GunPrep_Wid(VR_GunPrep_ClassWid(c));
        }
        else if(c == "func_weapon_grabbable" || c == "thrown_weapon")
        {
            VR_GunPrep_Wid(e.weapon);
        }
        else if(substring(c, 0, 8) == "monster_")
        {
            VR_GunPrep_Wid(VR_GunPrep_MonsterWid(c, e.ad_classtype));
        }
        else if(c == "backpack")
        {
            if(e.items & IT_SHOTGUN)          { VR_GunPrep_Wid(WID_SHOTGUN); }
            if(e.items & IT_SUPER_SHOTGUN)    { VR_GunPrep_Wid(WID_SUPER_SHOTGUN); }
            if(e.items & IT_NAILGUN)          { VR_GunPrep_Wid(WID_NAILGUN); }
            if(e.items & IT_SUPER_NAILGUN)    { VR_GunPrep_Wid(WID_SUPER_NAILGUN); }
            if(e.items & IT_GRENADE_LAUNCHER) { VR_GunPrep_Wid(WID_GRENADE_LAUNCHER); }
            if(e.items & IT_ROCKET_LAUNCHER)  { VR_GunPrep_Wid(WID_ROCKET_LAUNCHER); }
            if(e.items & IT_LIGHTNING)        { VR_GunPrep_Wid(WID_LIGHTNING); }
        }
        else if(c == "player")
        {
            VR_GunPrep_Wid(e.weapon);
            VR_GunPrep_Wid(e.weapon2);
            VR_GunPrep_Wid(e.holsterweapon0);
            VR_GunPrep_Wid(e.holsterweapon1);
            VR_GunPrep_Wid(e.holsterweapon2);
            VR_GunPrep_Wid(e.holsterweapon3);
            VR_GunPrep_Wid(e.holsterweapon4);
            VR_GunPrep_Wid(e.holsterweapon5);
        }
        e = nextent(e);
    }
    return vr_gp_n;
};

// The k-th model of the list VR_GunPrep_Collect made: gun k / 2, its primary (even k) or secondary-ammo model (odd k).
string(float k) VR_GunPrep_Model =
{
    float h = floor(k / 2);
    if(h >= vr_gp_n)
    {
        return "";
    }
    return WeaponIdToModel(vr_gp_order[h], (k - h * 2) ? QVR_WPNFLAG_USE_SECONDARY_AMMO : 0);
};
