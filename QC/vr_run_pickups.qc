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
// The table's rows: the ammo's first (RPK_AMMO_ROWS: seven kinds, small and big), then health, armour, weapons (the
// weapon data's: weapons.txt pickup_tier), powerups (VR_RPK_Build; VR_RPK_Spawn a row's spawn function by its class).
#define RPK_ROWS_MAX 48 // (the rows: 28 items' and the weapons' with a tier)
#define RPK_AMMO_ROWS 14
nosave float vr_rpk_bad[RPK_ROWS_MAX + 1]; // 1: a row whose spawn function refused it on this map
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

// The items that are not weapons: class, variant spawnflags, kind, tier, ammo. Balanced keeps a kind and its tier
// (armour: a step away). Powerups: 0 the strong (Quad, Pentagram), 2 the defensive (Ring, AD's shields), 1 the suits and
// the belt. A weapon's row is the weapon data's (VR_RPK_Build: weapons.txt pickup_tier).
typedef struct
{
    string cls;
    float sf, cat, tier, aid;
} vr_rpk_item;

vr_rpk_item VR_RPK_ITEMS[] =
{
    { "item_shells", 0, 1, 0, AID_SHELLS }, { "item_shells", 1, 1, 1, AID_SHELLS },
    { "item_spikes", 0, 1, 0, AID_NAILS }, { "item_spikes", 1, 1, 1, AID_NAILS },
    { "item_rockets", 0, 1, 0, AID_ROCKETS }, { "item_rockets", 1, 1, 1, AID_ROCKETS },
    { "item_cells", 0, 1, 0, AID_CELLS }, { "item_cells", 1, 1, 1, AID_CELLS },
    { "item_lava_spikes", 0, 1, 0, AID_NAILS }, { "item_lava_spikes", 1, 1, 1, AID_NAILS },
    { "item_multi_rockets", 0, 1, 0, AID_ROCKETS }, { "item_multi_rockets", 1, 1, 1, AID_ROCKETS },
    { "item_plasma", 0, 1, 0, AID_CELLS }, { "item_plasma", 1, 1, 1, AID_CELLS },
    { "item_health", H_ROTTEN, 2, 0, 0 }, { "item_health", 0, 2, 0, 0 }, { "item_health", H_MEGA, 2, 1, 0 },
    { "item_armor1", 0, 3, 1, 0 }, { "item_armor2", 0, 3, 2, 0 }, { "item_armorInv", 0, 3, 3, 0 },
    { "item_artifact_super_damage", 0, 5, 0, 0 }, { "item_artifact_invulnerability", 0, 5, 0, 0 },
    { "item_artifact_invisibility", 0, 5, 2, 0 }, { "item_artifact_empathy_shields", 0, 5, 2, 0 },
    { "item_powerup_shield", 0, 5, 2, 0 }, { "item_artifact_envirosuit", 0, 5, 1, 0 },
    { "item_artifact_wetsuit", 0, 5, 1, 0 }, { "item_powerup_belt", 0, 5, 1, 0 }
};
#define VR_RPK_ITEMS_N ((float)(sizeof(VR_RPK_ITEMS) / sizeof(VR_RPK_ITEMS[0])))
#define RPK_BEFORE_WEAPONS 20 // (the ammo, health and armour rows: the weapons' come after them, then the powerups)

// AD's weapon upgrades as map pickups (vr_ad_bridge.qc's AD_UPGRADES): row i's weapon, its pickup's classname, its model
// (its weapons.txt slot).
float(float i) AD_UpgradeWid;
string(float i) AD_UpgradeMapClass;
string(float i) AD_UpgradeVRModel;
#define RPK_AD_UPGRADES 3

// The rows, built from the tables (VR_RPK_Build): the items', the weapon data's in the middle, by tier then weapon id.
nosave string vr_rpk_rcls[RPK_ROWS_MAX];
nosave float vr_rpk_rsf[RPK_ROWS_MAX];
nosave float vr_rpk_rcat[RPK_ROWS_MAX];
nosave float vr_rpk_rtier[RPK_ROWS_MAX];
nosave float vr_rpk_rwid[RPK_ROWS_MAX];
nosave float vr_rpk_raid[RPK_ROWS_MAX];
nosave float vr_rpk_rad[RPK_ROWS_MAX];
nosave float vr_rpk_nrows; // (0: not built)

void(string c, float sf, float cat, float tier, float wid, float aid, float ad) VR_RPK_Add =
{
    if(vr_rpk_nrows >= RPK_ROWS_MAX) { ASSERT(FALSE); return; }
    vr_rpk_rcls[vr_rpk_nrows] = c;
    vr_rpk_rsf[vr_rpk_nrows] = sf;
    vr_rpk_rcat[vr_rpk_nrows] = cat;
    vr_rpk_rtier[vr_rpk_nrows] = tier;
    vr_rpk_rwid[vr_rpk_nrows] = wid;
    vr_rpk_raid[vr_rpk_nrows] = aid;
    vr_rpk_rad[vr_rpk_nrows] = ad;
    vr_rpk_nrows++;
};

// A weapon's Randomized Pickups tier (weapons.txt pickup_tier, by its model's slot): 0..3, -1 never, -2 not classified.
float(float wid) VR_RPK_WeaponTier = { return weaponvalue(VR_WEAPONS[wid].model, "pickup_tier"); };

void() VR_RPK_Build =
{
    vr_rpk_nrows = 0;
    float i;
    for(i = 0; i < RPK_BEFORE_WEAPONS; i++)
    {
        VR_RPK_Add(VR_RPK_ITEMS[i].cls, VR_RPK_ITEMS[i].sf, VR_RPK_ITEMS[i].cat, VR_RPK_ITEMS[i].tier, 0,
                   VR_RPK_ITEMS[i].aid, FALSE);
    }
    for(float t = 0; t <= 3; t++)
    {
        for(float wid = WID_FIST; wid <= WID_LAST; wid++)
        {
            if(VR_WEAPONS[wid].classname != "" && VR_RPK_WeaponTier(wid) == t)
            {
                VR_RPK_Add(VR_WEAPONS[wid].classname, 0, RPK_WEAPON, t, wid, WeaponIdToAmmoId(wid, 0), FALSE);
            }
        }
        for(i = 0; i < RPK_AD_UPGRADES; i++)
        {
            if(weaponvalue(AD_UpgradeVRModel(i), "pickup_tier") == t)
            {
                float uw = AD_UpgradeWid(i);
                VR_RPK_Add(AD_UpgradeMapClass(i), 0, RPK_WEAPON, t, uw, WeaponIdToAmmoId(uw, 0), TRUE);
            }
        }
    }
    for(i = RPK_BEFORE_WEAPONS; i < VR_RPK_ITEMS_N; i++)
    {
        VR_RPK_Add(VR_RPK_ITEMS[i].cls, VR_RPK_ITEMS[i].sf, VR_RPK_ITEMS[i].cat, VR_RPK_ITEMS[i].tier, 0,
                   VR_RPK_ITEMS[i].aid, FALSE);
    }
};

void(float r) VR_RPK_Load =
{
    if(!vr_rpk_nrows) VR_RPK_Build();
    vr_rpk_cls = vr_rpk_rcls[r];
    vr_rpk_sf = vr_rpk_rsf[r];
    vr_rpk_cat = vr_rpk_rcat[r];
    vr_rpk_tier = vr_rpk_rtier[r];
    vr_rpk_wid = vr_rpk_rwid[r];
    vr_rpk_aid = vr_rpk_raid[r];
    vr_rpk_ad = vr_rpk_rad[r];
};

// The row's spawn function, on self (by its classname: the map's own way).
void(float r) VR_RPK_Spawn =
{
    VR_RPK_Load(r);
    callspawn(vr_rpk_cls);
};

// The row of item `e` (its class and its variant: a big box, a rotten or mega health), or -1: not one of the table.
float(entity e) VR_RPK_RowOf =
{
    if(!vr_rpk_nrows) VR_RPK_Build();
    string c = e.classname;
    if(c == "item_armorinv") c = "item_armorInv"; // (both spellings spawn it)
    float vari = 0;
    if(c == "item_health") vari = (e.spawnflags & H_ROTTEN) ? H_ROTTEN : ((e.spawnflags & H_MEGA) ? H_MEGA : 0);
    else vari = e.spawnflags & WEAPON_BIG2;
    for(float r = 0; r < vr_rpk_nrows; r++)
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

// The weapons with a map pickup (the weapon table's, AD's upgrades) whose weapons.txt slot has no pickup_tier (each
// printed): their count. A new weapon with a pickup gets a tier there (0..3) or -1 (never swapped).
float() VR_RPK_Unclassified =
{
    float n = 0;
    for(float wid = WID_FIST; wid <= WID_LAST; wid++)
    {
        if(VR_WEAPONS[wid].classname == "" || VR_RPK_WeaponTier(wid) != -2) continue;
        n++;
        dprint(sprintf("Randomized pickups: %s (weapon %g) is not classified: pickup_tier in its quakevr/data/weapons.txt "
                       "slot (%s)\n", VR_WEAPONS[wid].classname, wid, VR_WEAPONS[wid].model));
    }
    for(float i = 0; i < RPK_AD_UPGRADES; i++)
    {
        if(weaponvalue(AD_UpgradeVRModel(i), "pickup_tier") != -2) continue;
        n++;
        dprint(sprintf("Randomized pickups: %s is not classified: pickup_tier in its weapons.txt slot (%s)\n",
                       AD_UpgradeMapClass(i), AD_UpgradeVRModel(i)));
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

float vr_rpk_cand[RPK_ROWS_MAX + 1];
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
    for(float r = 0; r < vr_rpk_nrows; r++)
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
    for(float r = 0; r < vr_rpk_nrows; r++) vr_rpk_bad[r] = FALSE;
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
