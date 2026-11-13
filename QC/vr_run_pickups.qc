//
//
//
// ----------------------------------------------------------------------------
// Mutator: Randomized Pickups (vr_run_pickups; docs/vr-port/MUTATORS.md)
// ----------------------------------------------------------------------------
//
// The map's pickups swapped as it loads: 1 Balanced, 2 Anything Goes. Only the kinds of the table below (ammo, health,
// armour, weapons, powerups) and never one the map names or counts: an item with a targetname, target or killtarget
// (a trigger's, a door's), a secret item (HONEY's), one a trigger brings (trigger-spawned, Dawn of the Machine's
// SPAWNED, AD's STARTOFF: those never reach the hook). Keys, runes, sigils and every other kind are not in the table.
// - StartItem (VR_Run_ItemSpawned): an item of the table marked (vr_rpk_row), a weapon's ammo noted (vr_rpk_wa);
// - the engine, once the map's entities have all spawned (VR_Run_MapSpawned; not a saved game's load: its entities
//   are the save's): each marked item rolled, and when another kind comes up, that kind spawned in its place by its own
//   spawn function (its model, sound and touch as a map's) and the old one removed. A kind whose spawn function
//   refuses (its game's data not here: Rogue's, Hipnotic's) is not tried again on this map; the item stays.
// Balanced: ammo for ammo of a weapon kind you hold (the parms you enter with), one placed within RPK_NEAR units, or
// the shotgun's shells, the box's size kept; health for health of its tier (15 and 25; the megahealth alone); armour
// for armour a step away (green, yellow, red); a weapon for one of its tier; a powerup for one of its tier. AD's
// upgrades (the Widowmaker, the Plasma Gun) only with AD mounted and its rules on. Anything Goes: a kind
// (ammo, health, armour, weapon, powerup), then any of it. Melee Only (VR_RunMeleeOnly_Refuses): nothing becomes a
// gun that was not one.
// The rolls: a hash of the item's place and the run's seed (vr_run_seed, set at the run's start): the same map gets the
// same pickups all through a run (an Ironman restart too); seed 0: the server's random numbers (sv_random_seed).

.float vr_rpk_row;     // StartItem: the item's row + 1 when it may be swapped; -(row + 1): kept for the map; 0: not one
.float vr_rpk_wa;      // a weapon item's ammo bit (any weapon of the table, swappable or not: the ammo's "nearby")
.float vr_rpk_from;    // a swapped-in item: the row + 1 of the one it replaced (the test reads it; saved)
.float vr_rpk_mask;    // an ammo item: the ammo bits Balanced allows in its place (a swapped-in one keeps them: the test)
.entity vr_rpk_wnext;  // the pass's chain of the map's weapons
float vr_rpk_busy;     // a swap's spawn function runs (its StartItem leaves it unmarked)
float vr_rpk_misfit;   // the last VR_RPK_Make's item did not fit where the old one stood (its kind not refused)
float vr_rpk_seed;     // this map's pass's seed
float vr_rpk_heldmask; // the ammo bits of the weapons held as he enters (and the shells)
entity vr_rpk_weapons; // the chain's head
float vr_rpk_seen;     // the pass: items it rolled
float vr_rpk_swapped;  // and swapped
float vr_rpk_kept;     // items of the table kept for the map's sake (named, counted, triggered)
// The table's rows: the ammo's first (RPK_AMMO_ROWS: seven kinds, small and big), then health, armour, weapons,
// powerups (VR_RPK_Load; VR_RPK_Spawn the same rows' spawn functions).
#define RPK_ROWS 41
#define RPK_AMMO_ROWS 14
nosave float vr_rpk_bad[RPK_ROWS + 1]; // 1: a row whose spawn function refused it on this map
float RPK_NEAR = 1536;
float RPK_AMMO = 1;
float RPK_HEALTH = 2;
float RPK_ARMOUR = 3;
float RPK_WEAPON = 4;
float RPK_POWER = 5;

// A row's record (VR_RPK_Load): its class, the spawnflags its variant takes, its kind, its tier (ammo: 0 small, 1 big),
// its weapon (WID_) and its ammo's base kind (AID_: Rogue's lava nails are nails), and 1 when only AD's maps have it.
string vr_rpk_cls;
float vr_rpk_sf;
float vr_rpk_cat;
float vr_rpk_tier;
float vr_rpk_wid;
float vr_rpk_aid;
float vr_rpk_ad;

void(string c, float sf, float cat, float tier, float wid, float aid) VR_RPK_Set =
{
    vr_rpk_cls = c;
    vr_rpk_sf = sf;
    vr_rpk_cat = cat;
    vr_rpk_tier = tier;
    vr_rpk_wid = wid;
    vr_rpk_aid = aid;
    vr_rpk_ad = FALSE;
};

// A weapon's row: its ammo is the weapon table's (vr_weaponutil.qc); `ad`: only AD's maps have it.
void(string c, float tier, float wid, float ad) VR_RPK_Weapon =
{
    VR_RPK_Set(c, 0, RPK_WEAPON, tier, wid, WeaponIdToAmmoId(wid, 0));
    vr_rpk_ad = ad;
};

void(float r) VR_RPK_Load =
{
    if(r < RPK_AMMO_ROWS)
    {
        float t = r - 2 * floor(r / 2); // (even: the small box, odd: the big one)
        float k = floor(r / 2);
        if(k == 0) VR_RPK_Set("item_shells", t, RPK_AMMO, t, 0, AID_SHELLS);
        else if(k == 1) VR_RPK_Set("item_spikes", t, RPK_AMMO, t, 0, AID_NAILS);
        else if(k == 2) VR_RPK_Set("item_rockets", t, RPK_AMMO, t, 0, AID_ROCKETS);
        else if(k == 3) VR_RPK_Set("item_cells", t, RPK_AMMO, t, 0, AID_CELLS);
        else if(k == 4) VR_RPK_Set("item_lava_spikes", t, RPK_AMMO, t, 0, AID_NAILS);
        else if(k == 5) VR_RPK_Set("item_multi_rockets", t, RPK_AMMO, t, 0, AID_ROCKETS);
        else VR_RPK_Set("item_plasma", t, RPK_AMMO, t, 0, AID_CELLS);
        return;
    }
    if(r == 14) VR_RPK_Set("item_health", H_ROTTEN, RPK_HEALTH, 0, 0, 0);
    else if(r == 15) VR_RPK_Set("item_health", 0, RPK_HEALTH, 0, 0, 0);
    else if(r == 16) VR_RPK_Set("item_health", H_MEGA, RPK_HEALTH, 1, 0, 0);
    else if(r == 17) VR_RPK_Set("item_armor1", 0, RPK_ARMOUR, 1, 0, 0);
    else if(r == 18) VR_RPK_Set("item_armor2", 0, RPK_ARMOUR, 2, 0, 0);
    else if(r == 19) VR_RPK_Set("item_armorInv", 0, RPK_ARMOUR, 3, 0, 0);
    else if(r == 20) VR_RPK_Weapon("weapon_shotgun", 0, WID_SHOTGUN, FALSE);
    else if(r == 21) VR_RPK_Weapon("weapon_revolver", 0, WID_REVOLVER, FALSE);
    else if(r == 22) VR_RPK_Weapon("weapon_supershotgun", 1, WID_SUPER_SHOTGUN, FALSE);
    else if(r == 23) VR_RPK_Weapon("weapon_nailgun", 1, WID_NAILGUN, FALSE);
    else if(r == 24) VR_RPK_Weapon("weapon_upgrade_ssg", 1, WID_SUPER_SHOTGUN, TRUE);
    else if(r == 25) VR_RPK_Weapon("weapon_supernailgun", 2, WID_SUPER_NAILGUN, FALSE);
    else if(r == 26) VR_RPK_Weapon("weapon_grenadelauncher", 2, WID_GRENADE_LAUNCHER, FALSE);
    else if(r == 27) VR_RPK_Weapon("weapon_proximity_gun", 2, WID_PROXIMITY_GUN, FALSE);
    else if(r == 28) VR_RPK_Weapon("weapon_rocketlauncher", 3, WID_ROCKET_LAUNCHER, FALSE);
    else if(r == 29) VR_RPK_Weapon("weapon_lightning", 3, WID_LIGHTNING, FALSE);
    else if(r == 30) VR_RPK_Weapon("weapon_laser_gun", 3, WID_LASER_CANNON, FALSE);
    else if(r == 31) VR_RPK_Weapon("weapon_mjolnir", 3, WID_MJOLNIR, FALSE);
    else if(r == 32) VR_RPK_Weapon("weapon_upgrade_lg", 3, WID_LIGHTNING, TRUE);
    else if(r == 33) VR_RPK_Set("item_artifact_super_damage", 0, RPK_POWER, 0, 0, 0);
    else if(r == 34) VR_RPK_Set("item_artifact_invulnerability", 0, RPK_POWER, 0, 0, 0);
    else if(r == 35) VR_RPK_Set("item_artifact_invisibility", 0, RPK_POWER, 0, 0, 0);
    else if(r == 36) VR_RPK_Set("item_artifact_empathy_shields", 0, RPK_POWER, 0, 0, 0);
    else if(r == 37) VR_RPK_Set("item_powerup_shield", 0, RPK_POWER, 0, 0, 0);
    else if(r == 38) VR_RPK_Set("item_artifact_envirosuit", 0, RPK_POWER, 1, 0, 0);
    else if(r == 39) VR_RPK_Set("item_artifact_wetsuit", 0, RPK_POWER, 1, 0, 0);
    else VR_RPK_Set("item_powerup_belt", 0, RPK_POWER, 1, 0, 0);
};

// The row's spawn function, on self.
void(float r) VR_RPK_Spawn =
{
    if(r < RPK_AMMO_ROWS)
    {
        float k = floor(r / 2);
        if(k == 0) item_shells();
        else if(k == 1) item_spikes();
        else if(k == 2) item_rockets();
        else if(k == 3) item_cells();
        else if(k == 4) item_lava_spikes();
        else if(k == 5) item_multi_rockets();
        else item_plasma();
    }
    else if(r <= 16) item_health();
    else if(r == 17) item_armor1();
    else if(r == 18) item_armor2();
    else if(r == 19) item_armorInv();
    else if(r == 20) weapon_shotgun();
    else if(r == 21) weapon_revolver();
    else if(r == 22) weapon_supershotgun();
    else if(r == 23) weapon_nailgun();
    else if(r == 24) weapon_upgrade_ssg();
    else if(r == 25) weapon_supernailgun();
    else if(r == 26) weapon_grenadelauncher();
    else if(r == 27) weapon_proximity_gun();
    else if(r == 28) weapon_rocketlauncher();
    else if(r == 29) weapon_lightning();
    else if(r == 30) weapon_laser_gun();
    else if(r == 31) weapon_mjolnir();
    else if(r == 32) weapon_upgrade_lg();
    else if(r == 33) item_artifact_super_damage();
    else if(r == 34) item_artifact_invulnerability();
    else if(r == 35) item_artifact_invisibility();
    else if(r == 36) item_artifact_empathy_shields();
    else if(r == 37) item_powerup_shield();
    else if(r == 38) item_artifact_envirosuit();
    else if(r == 39) item_artifact_wetsuit();
    else item_powerup_belt();
};

// The row of item `e` (its class and its variant: a big box, a rotten or mega health), or -1: not one of the table.
float(entity e) VR_RPK_RowOf =
{
    string c = e.classname;
    if(c == "item_armorinv") c = "item_armorInv"; // (both spellings spawn it)
    float vari = 0;
    if(c == "item_health") vari = (e.spawnflags & H_ROTTEN) ? H_ROTTEN : ((e.spawnflags & H_MEGA) ? H_MEGA : 0);
    else vari = e.spawnflags & WEAPON_BIG2;
    for(float r = 0; r < RPK_ROWS; r++)
    {
        VR_RPK_Load(r);
        if(vr_rpk_cls == c && (vr_rpk_cat == RPK_WEAPON || vr_rpk_cat == RPK_ARMOUR || vr_rpk_cat == RPK_POWER ||
                               vr_rpk_sf == vari))
        {
            return r;
        }
    }
    return -1;
};

// The weapons of the weapon table (VR_WEAPONS, vr_weaponutil.qc) with a map pickup that the table leaves out: melee
// weapons, the enemies' own guns, tools, the flare gun, the nail pistol. A new weapon with a pickup is either a row above
// or named here: vr_runpickups_test fails otherwise (VR_RPK_Unclassified).
float(string c) VR_RPK_WeaponLeftOut =
{
    return c == "weapon_grapple" || c == "weapon_axe" || c == "weapon_knight_sword" || c == "weapon_chainsaw" ||
           c == "weapon_gruntgun" || c == "weapon_enfrifle" || c == "weapon_superaxe" || c == "weapon_toolgun" ||
           c == "weapon_nailpistol" || c == "weapon_flaregun";
};

// The weapons with a map pickup neither in the table nor left out by name (each printed): their count.
float() VR_RPK_Unclassified =
{
    float n = 0;
    for(float wid = WID_FIST; wid <= WID_LAST; wid++)
    {
        string c = VR_WEAPONS[wid].classname;
        if(c == "" || VR_RPK_WeaponLeftOut(c)) continue;
        float found = FALSE;
        for(float r = 0; r < RPK_ROWS && !found; r++)
        {
            VR_RPK_Load(r);
            found = (vr_rpk_cls == c);
        }
        if(!found)
        {
            n++;
            dprint(sprintf("Randomized pickups: %s (weapon %g) is not classified: a row of vr_run_pickups.qc's table, "
                           "or VR_RPK_WeaponLeftOut\n", c, wid));
        }
    }
    return n;
};

float(float aid) VR_RPK_AmmoBit =
{
    return (aid >= AID_SHELLS && aid <= AID_CELLS) ? VRUTIL_POWER_OF_TWO((aid - 1)) : 0;
};

// The ammo bits of the weapons in `items` (IT_ bits), the shells always (the start's shotgun or revolver).
float(float items) VR_RPK_HeldMask =
{
    float m = VR_RPK_AmmoBit(AID_SHELLS);
    if(items & (IT_NAILGUN | IT_SUPER_NAILGUN)) m = m | VR_RPK_AmmoBit(AID_NAILS);
    if(items & (IT_GRENADE_LAUNCHER | IT_ROCKET_LAUNCHER)) m = m | VR_RPK_AmmoBit(AID_ROCKETS);
    if(items & (IT_LIGHTNING | IT_LASER_CANNON)) m = m | VR_RPK_AmmoBit(AID_CELLS);
    return m;
};

// An item the map needs as it is: named, aimed at something, counted or brought by a trigger.
float(entity e) VR_RPK_Protected =
{
    if(e.targetname != "" || e.target != "" || e.killtarget != "") return TRUE;
    if(e.spawnflags & (HONEY_SPAWNFLAG_ITEM_SECRET | SPAWNFLAG_TRIGGER_SPAWN)) return TRUE;
    if(MG3_Campaign() && (e.spawnflags & MG3_ITEM_SPAWNED)) return TRUE;
    return FALSE;
};

// StartItem (VR_Run_ItemSpawned): a map's item of the table marked for the pass.
void() VR_RunPickups_ItemSpawned =
{
    if(vr_rpk_busy || vr_spawn_live) return;
    float r = VR_RPK_RowOf(self);
    if(r < 0) return;
    VR_RPK_Load(r);
    if(vr_rpk_cat == RPK_WEAPON) self.vr_rpk_wa = VR_RPK_AmmoBit(vr_rpk_aid);
    if(!cvar_hget(cvarh_vr_run_pickups) || deathmatch) return;
    self.vr_rpk_row = VR_RPK_Protected(self) ? -(r + 1) : r + 1;
};

// A roll in [0, 1) of item `e`: its place, its row and the seed (the same each time the map loads in a run).
float(entity e, float salt) VR_RPK_Roll =
{
    float x = e.origin_x * 0.1213 + e.origin_y * 0.3377 + e.origin_z * 0.7741 + vr_rpk_seed * 0.0917 + salt * 1.618 +
              e.vr_rpk_row * 0.271;
    x = sin(x) * 43758.5453;
    return x - floor(x);
};

// Row `r` may come in the place of row `r0`'s item: its game's data here (not refused on this map), AD's only in AD,
// and under Melee Only no gun where there was none.
float(float r, float r0) VR_RPK_Allowed =
{
    if(vr_rpk_bad[r]) return FALSE;
    VR_RPK_Load(r0);
    float gun0 = (vr_rpk_cat == RPK_WEAPON) && VR_RunMeleeOnly_Refuses(vr_rpk_wid);
    VR_RPK_Load(r);
    if(vr_rpk_ad && !VR_AD_FeatureScope(1)) return FALSE;
    if(vr_rpk_cat == RPK_WEAPON && !gun0 && VR_RunMeleeOnly_Refuses(vr_rpk_wid)) return FALSE;
    return TRUE;
};

float vr_rpk_cand[RPK_ROWS + 1];
float vr_rpk_ncand;

// The rows that may replace item `e` (row r0) in `mode`, in vr_rpk_cand; `mask`: the ammo bits Balanced allows.
void(entity e, float r0, float mode, float mask, float salt) VR_RPK_Candidates =
{
    vr_rpk_ncand = 0;
    VR_RPK_Load(r0);
    float cat = vr_rpk_cat;
    float tier = vr_rpk_tier;
    if(mode >= 2)
    {
        cat = 1 + floor(VR_RPK_Roll(e, salt + 7) * 5); // (Anything Goes: a kind first, then any of it)
        if(cat > RPK_POWER) cat = RPK_POWER;
    }
    for(float r = 0; r < RPK_ROWS; r++)
    {
        if(r != r0 && !VR_RPK_Allowed(r, r0)) continue;
        VR_RPK_Load(r);
        if(vr_rpk_cat != cat) continue;
        if(mode < 2)
        {
            if(vr_rpk_cat == RPK_ARMOUR) { if(fabs(vr_rpk_tier - tier) > 1) continue; }
            else if(vr_rpk_tier != tier) continue;
            if(vr_rpk_cat == RPK_AMMO && r != r0 && !(mask & VR_RPK_AmmoBit(vr_rpk_aid))) continue;
        }
        vr_rpk_cand[vr_rpk_ncand] = r;
        vr_rpk_ncand++;
    }
};

// Whether row `r`'s item has its origin at its box's corner (ammo and health boxes: '0 0 0' to '32 32 h'), not its
// middle (weapons, armour, powerups).
float(float r) VR_RPK_Corner =
{
    VR_RPK_Load(r);
    return vr_rpk_cat == RPK_AMMO || vr_rpk_cat == RPK_HEALTH;
};

// Row `r` spawned in the place of `e` (row r0), by its spawn function (as a map's item); world: its spawn function
// refused it. A box for a centred item or the reverse: moved half a box so it stands where the old one stood (a box at
// a weapon's place, by a wall, was in the wall and fell out of the level).
entity(entity e, float r0, float r) VR_RPK_Make =
{
    vector at = e.origin;
    float corner0 = VR_RPK_Corner(r0);
    float corner = VR_RPK_Corner(r);
    if(corner0 && !corner) at = at + '16 16 0';
    else if(corner && !corner0) at = at - '16 16 0';
    VR_RPK_Load(r);
    entity n = spawn();
    n.classname = vr_rpk_cls;
    n.spawnflags = (e.spawnflags - (e.spawnflags & 3)) | vr_rpk_sf; // (its vari's bits; the rest as the map's)
    n.angles = e.angles;
    setorigin(n, at);
    entity oldself = self;
    self = n;
    vr_rpk_busy = TRUE;
    vr_rpk_misfit = FALSE;
    VR_RPK_Spawn(r);
    vr_rpk_busy = FALSE;
    if(n.nextthink == -1)
    {
        self = oldself;
        return world; // (removed: its game's data missing, or not in this campaign)
    }
    if(n.think != PlaceItem)
    {
        self = oldself;
        remove(n);
        return world;
    }
    // Its box dropped to the floor there as PlaceItem will (the same test, put back): one that would fall out of the
    // level (a bigger box by a wall) is not made.
    vector o = n.origin;
    setorigin(n, o + '0 0 6');
    float fits = droptofloor();
    setorigin(n, o);
    self = oldself;
    if(!fits)
    {
        vr_rpk_misfit = TRUE;
        remove(n);
        return world;
    }
    return n;
};

// The ammo bits of the weapons within RPK_NEAR of `e`.
float(entity e) VR_RPK_NearMask =
{
    float m = 0;
    for(entity w = vr_rpk_weapons; w; w = w.vr_rpk_wnext)
    {
        if(vlen(w.origin - e.origin) <= RPK_NEAR) m = m | w.vr_rpk_wa;
    }
    return m;
};

void(entity e, float mode) VR_RPK_One =
{
    float r0 = e.vr_rpk_row - 1;
    vr_rpk_seen++;
    VR_RPK_Load(r0);
    float mask = e.vr_rpk_mask;
    for(float tries = 0; tries < 4; tries++)
    {
        VR_RPK_Candidates(e, r0, mode, mask, tries * 3);
        if(!vr_rpk_ncand) return;
        float pick = vr_rpk_cand[min(vr_rpk_ncand - 1, floor(VR_RPK_Roll(e, tries * 3 + 1) * vr_rpk_ncand))];
        if(pick == r0) return;
        entity n = VR_RPK_Make(e, r0, pick);
        if(n)
        {
            n.vr_rpk_from = r0 + 1;
            n.vr_rpk_mask = mask;
            remove(e);
            vr_rpk_swapped++;
            return;
        }
        if(!vr_rpk_misfit) vr_rpk_bad[pick] = TRUE;
    }
};

// The engine (VR_Run_MapSpawned): the map's marked items swapped. `held`: the IT_ bits he enters with (0: a new game).
void(float held) VR_RunPickups_MapSpawned =
{
    vr_rpk_seen = 0;
    vr_rpk_swapped = 0;
    float mode = cvar_hget(cvarh_vr_run_pickups);
    if(!mode || deathmatch || substring(mapname, 0, 2) == "vr") return; // (Quake VR's own maps are no run's)
    vr_rpk_seed = cvar_hget(cvarh_vr_run_seed);
    if(!vr_rpk_seed) vr_rpk_seed = 1 + floor(random() * 65535);
    for(float r = 0; r < RPK_ROWS; r++) vr_rpk_bad[r] = FALSE;
    vr_rpk_heldmask = VR_RPK_HeldMask(held);
    vr_rpk_weapons = world;
    vr_rpk_kept = 0;
    entity e;
    for(e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_rpk_wa)
        {
            e.vr_rpk_wnext = vr_rpk_weapons;
            vr_rpk_weapons = e;
        }
        if(e.vr_rpk_row < 0) vr_rpk_kept++;
    }
    // (The ammo's allowed kinds before any swap: a swap frees a weapon's entity, and the next one made may take it.)
    for(e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_rpk_row > 0 && e.vr_rpk_row <= RPK_AMMO_ROWS) e.vr_rpk_mask = vr_rpk_heldmask | VR_RPK_NearMask(e);
    }
    e = nextent(world);
    while(e)
    {
        entity next = nextent(e);
        if(e.vr_rpk_row > 0)
        {
            VR_RPK_One(e, mode);
        }
        e = next;
    }
    dprint(sprintf("Randomized pickups (%s): %g of %g items swapped, %g kept for the map (seed %g)\n",
        mode >= 2 ? "anything goes" : "balanced", vr_rpk_swapped, vr_rpk_seen, vr_rpk_kept, vr_rpk_seed));
};
