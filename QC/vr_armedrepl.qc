//
//
//
// ----------------------------------------------------------------------------
// Armed humanoid replacements (docs/vr-port/ROUND22.md, "Armed humanoid replacements"; ARMED_MONSTERS.md)
// ----------------------------------------------------------------------------
//
// vr_agrunt_repl 1: every grunt and enforcer a map spawns (its own entities, its spawners, MG's and AD's waiting ones)
// is its armed version (vr_armedgrunt.qc VR_AM_Spawn), holding its own gun: the grunt's rifle, the enforcer's laser
// rifle. That gun is "native" (its prop's .vr_ag_native): it fires as the monster it replaced fires (its attack frames,
// chances and pauses, its own shots and damage: army_burst_round, army_fire, enforcer_fire's laser; not times
// vr_agrunt_damage_scale), never runs dry, and its backpack is that monster's (SoldierDrop, EnforcerDrop: the ammo and
// the chance of a weapon); dying it drops the gun as the monster would (full). So the balance is the monster's while it
// can be disarmed, tugged, parried, its gun shot out, its weapon held by its ragdoll, its gun taken.
//
// On top of it, each with its own chance (0 by default), times the skill's vr_agrunt_repl_skill<N>:
// - vr_agrunt_repl_common: a shotgun or a nail pistol;
// - vr_agrunt_repl_uncommon: a nailgun, a super nailgun or a super shotgun, once a player owns that weapon
//   (VR_WeaponUtil_EntIsEligibleForWeapon: the rule the monsters' weapon drops use);
// - vr_agrunt_repl_rare: the launchers, the thunderbolt, the melee weapons, the mission packs' guns, AD's soldiers'
//   guns in AD maps: owned as above, and only from the vr_agrunt_repl_rare_map'th map of an episode on (its name's
//   "<n>m<index>": e1m4, hip2m3, r1m5; a map with no index never);
// - vr_agrunt_repl_dual: a second weapon that pairs (VR_AG_Pairs: from the same rolls);
// - vr_agrunt_repl_gunaxe: a gun in its right hand and an axe in its left (VR_AGR_GunAxe: the gun, the axe up close).
// The rolls are seeded by the map's name, the monster's spawn point and kind (vr_agrunt_repl_seed shuffles them all):
// the same map gives the same loadouts every time, a restart and a reload too. Eligibility needs a player: a map's own
// monsters spawn before one is in, so theirs are rolled on the first frame with one (VR_AGR_Frame), spawned holding
// their own gun meanwhile.
// Where: vr_agrunt_repl_id (Quake's own campaigns, their custom maps), vr_agrunt_repl_mp (the mission packs),
// vr_agrunt_repl_ad (Arcane Dimensions' maps: its own encounters, off by default).

.vector vr_agr_seed;       // its rolls' seed (the map's name, its spawn point and kind)
.float vr_agr_pending;     // 1: its loadout not rolled yet (no player in yet: VR_AGR_Frame)
.float vr_ag_nstep;        // its native attack's step (VR_AGR_NatStep)
.float vr_agr_switches;    // a gun and an axe: its switches between them (the tests)
float vr_agr_npending;     // the monsters waiting for their rolls
float vr_agr_made;         // replacements made (the tests)

// The results of VR_AGR_Pick.
float vr_agr_code;         // its right hand's "weapon" code (vr_armedgrunt.qc VR_AG_Decode)
float vr_agr_code2;        // its left hand's (0: none)
float vr_agr_gunaxe;       // 1: a gun and an axe
float vr_agr_tier;         // the right hand's tier: 0 its own gun, 1 common, 2 uncommon, 3 rare

// ----------------------------------------------------------------------------
// Seeded rolls (Wichmann-Hill: products under 2^24, exact in QuakeC's floats)
// ----------------------------------------------------------------------------

float vr_agr_s1;
float vr_agr_s2;
float vr_agr_s3;

float(float x, float m) VR_AGR_Mod =
{
    float r = x - m * floor(x / m);
    if(r < 0) { r = r + m; }
    if(r >= m) { r = r - m; }
    return r;
};

float() VR_AGR_Rand =
{
    vr_agr_s1 = VR_AGR_Mod(171 * vr_agr_s1, 30269);
    vr_agr_s2 = VR_AGR_Mod(172 * vr_agr_s2, 30307);
    vr_agr_s3 = VR_AGR_Mod(170 * vr_agr_s3, 30323);
    float r = vr_agr_s1 / 30269 + vr_agr_s2 / 30307 + vr_agr_s3 / 30323;
    return r - floor(r);
};

void(vector seed) VR_AGR_SeedSet =
{
    float c = VR_AGR_Mod(fabs(rint(cvar("vr_agrunt_repl_seed"))), 30011);
    vr_agr_s1 = 1 + VR_AGR_Mod(seed_x + 17 * c, 30268);
    vr_agr_s2 = 1 + VR_AGR_Mod(seed_y + 29 * c, 30306);
    vr_agr_s3 = 1 + VR_AGR_Mod(seed_x * 3 + seed_y + seed_z * 11 + c, 30322);
    VR_AGR_Rand();
    VR_AGR_Rand();
};

// `e`'s seed: its map's name hashed, its spawn point, its kind.
vector(entity e, float kind) VR_AGR_SeedOf =
{
    float h = 7;
    float n = strlen(mapname);
    float i;
    for(i = 0; i < n; i++)
    {
        h = VR_AGR_Mod(h * 31 + str2chr(mapname, i), 30011);
    }
    vector s;
    s_x = h;
    s_y = VR_AGR_Mod(fabs(rint(e.origin_x)) * 7 + fabs(rint(e.origin_y)) * 13 + fabs(rint(e.origin_z)) * 3, 30011);
    s_z = kind + 1;
    return s;
};

// ----------------------------------------------------------------------------
// Where, what, who may hold it
// ----------------------------------------------------------------------------

// This map is one the replacements play in (vr_agrunt_repl_id, _mp, _ad).
float() VR_AGR_InScope =
{
    float c = cvar("vr_campaign");
    if(AD_Rules() || c == 6)
    {
        return cvar("vr_agrunt_repl_ad") != 0;
    }
    if(c == 1 || c == 2)
    {
        return cvar("vr_agrunt_repl_mp") != 0;
    }
    return cvar("vr_agrunt_repl_id") != 0;
};

// The map's index in its episode: the number after the last "<digit>m" of its name (e1m4: 4, hip2m3: 3); 0: none.
float() VR_AGR_MapIndex =
{
    float n = strlen(mapname);
    float best = 0;
    float i;
    for(i = 1; i + 1 < n; i++)
    {
        float c = str2chr(mapname, i);
        float before = str2chr(mapname, i - 1);
        if(c == 109 && before >= 48 && before <= 57) // ('m' after a digit)
        {
            float v = 0;
            float j = i + 1;
            while(j < n && str2chr(mapname, j) >= 48 && str2chr(mapname, j) <= 57)
            {
                v = v * 10 + (str2chr(mapname, j) - 48);
                j = j + 1;
            }
            if(j > i + 1)
            {
                best = v;
            }
        }
    }
    return best;
};

// Its own gun (the monster's: the grunt's rifle, the enforcer's laser rifle).
float(float kind) VR_AGR_NativeOf = { return kind == VR_AM_ENFORCER ? WID_ENFRIFLE : WID_GRUNTGUN; };

// Any player may have `wid` (the weapon drops' rule: owned; the axe and the shotgun always).
float(float wid) VR_AGR_Owned =
{
    entity p = find(world, classname, "player");
    while(p)
    {
        if(VR_WeaponUtil_EntIsEligibleForWeapon(p, wid))
        {
            return TRUE;
        }
        p = find(p, classname, "player");
    }
    return FALSE;
};

// The `i`th weapon code of tier `t` (1 common, 2 uncommon, 3 rare; -1 past the last).
float(float t, float i) VR_AGR_TierCode =
{
    if(t == 1)
    {
        return i == 0 ? WID_SHOTGUN : i == 1 ? WID_NAILPISTOL : -1;
    }
    if(t == 2)
    {
        return i == 0 ? WID_SUPER_SHOTGUN : i == 1 ? WID_NAILGUN : i == 2 ? WID_SUPER_NAILGUN : -1;
    }
    if(i == 0) { return WID_GRENADE_LAUNCHER; }
    if(i == 1) { return WID_ROCKET_LAUNCHER; }
    if(i == 2) { return WID_LIGHTNING; }
    if(i == 3) { return WID_AXE; }
    if(i == 4) { return WID_SWORD; }
    if(i == 5) { return WID_CROWBAR; }
    if(i == 6) { return WID_CHAINSAW; }
    if(i == 7) { return WID_MJOLNIR; }
    if(i == 8) { return WID_LASER_CANNON; }
    if(i == 9) { return WID_PROXIMITY_GUN; }
    if(i == 10) { return WID_ADROCKET; }
    if(i == 11) { return WID_ADGRENADE; }
    if(i == 12) { return WID_ADPLASMA; }
    if(i == 13) { return WID_ADDEFENDER; }
    if(i == 14) { return WID_ADCENTURION; }
    if(i == 15) { return WID_ADCROSSBOW; }
    return -1;
};

// A code of tier `t` a replacement may hold now: one it can hold here (VR_AG_Decode) a player owns (common: always;
// AD's soldiers' guns: in AD's maps).
float(float t, float code) VR_AGR_Eligible =
{
    if(!VR_AG_Decode(code))
    {
        return FALSE;
    }
    if(t == 1)
    {
        return TRUE;
    }
    if(VR_IsADGun(code))
    {
        return AD_Rules();
    }
    return VR_AGR_Owned(vr_ag_dec_wid);
};

float(float t) VR_AGR_TierCount =
{
    float n = 0;
    float i;
    for(i = 0; VR_AGR_TierCode(t, i) >= 0; i++)
    {
        if(VR_AGR_Eligible(t, VR_AGR_TierCode(t, i)))
        {
            n = n + 1;
        }
    }
    return n;
};

// Tier `t`'s eligible weapon at the roll `r` (0..1); -1: none.
float vr_agr_n1; // VR_AGR_Pick's tiers' eligible weapons now (VR_AGR_TierCount: once a pick)
float vr_agr_n2;
float vr_agr_n3;
float(float t, float r) VR_AGR_TierPick =
{
    float n = t == 1 ? vr_agr_n1 : t == 2 ? vr_agr_n2 : vr_agr_n3;
    if(n < 1)
    {
        return -1;
    }
    float k = min(n - 1, floor(r * n));
    float i;
    for(i = 0; VR_AGR_TierCode(t, i) >= 0; i++)
    {
        float code = VR_AGR_TierCode(t, i);
        if(VR_AGR_Eligible(t, code))
        {
            if(k < 1)
            {
                return code;
            }
            k = k - 1;
        }
    }
    return -1;
};

float(float x) VR_AGR_Unit = { return min(1, max(0, x)); };

// The chances times the skill's (vr_agrunt_repl_skill0..3).
float() VR_AGR_SkillMult = { return max(0, cvar(sprintf("vr_agrunt_repl_skill%g", min(3, max(0, rint(skill)))))); };

// Any variety on (VR_AGR_Pick has anything to roll).
float() VR_AGR_Varied =
{
    return cvar("vr_agrunt_repl_common") > 0 || cvar("vr_agrunt_repl_uncommon") > 0 || cvar("vr_agrunt_repl_rare") > 0 ||
           cvar("vr_agrunt_repl_dual") > 0 || cvar("vr_agrunt_repl_gunaxe") > 0;
};

float vr_agr_cc; // VR_AGR_Pick's tiers' chances now (common, uncommon, rare: scaled to a sum of at most 1)
float vr_agr_cu;
float vr_agr_cr;

// The tier at the roll `r` (VR_AGR_Pick's chances): 0 its own gun.
float(float r) VR_AGR_TierAt =
{
    return r < vr_agr_cr ? 3 : r < vr_agr_cr + vr_agr_cu ? 2 : r < vr_agr_cr + vr_agr_cu + vr_agr_cc ? 1 : 0;
};

// A replacement of the row `kind` with the seed `seed`: its loadout (vr_agr_code, _code2, _gunaxe, _tier).
void(float kind, vector seed) VR_AGR_Pick =
{
    VR_AGR_SeedSet(seed);
    float native = VR_AGR_NativeOf(kind);
    vr_agr_code = native;
    vr_agr_code2 = 0;
    vr_agr_gunaxe = 0;
    vr_agr_tier = 0;
    float m = VR_AGR_SkillMult();
    vr_agr_n1 = VR_AGR_TierCount(1);
    vr_agr_n2 = VR_AGR_TierCount(2);
    vr_agr_n3 = VR_AGR_TierCount(3);
    float rareOk = (VR_AGR_MapIndex() >= max(1, cvar("vr_agrunt_repl_rare_map"))) || (cvar("vr_agrunt_repl_rare_map") <= 0);
    vr_agr_cr = (rareOk && (vr_agr_n3 > 0)) ? VR_AGR_Unit(cvar("vr_agrunt_repl_rare") * m) : 0;
    vr_agr_cu = (vr_agr_n2 > 0) ? VR_AGR_Unit(cvar("vr_agrunt_repl_uncommon") * m) : 0;
    vr_agr_cc = (vr_agr_n1 > 0) ? VR_AGR_Unit(cvar("vr_agrunt_repl_common") * m) : 0;
    float sum = vr_agr_cr + vr_agr_cu + vr_agr_cc;
    if(sum > 1)
    {
        vr_agr_cr = vr_agr_cr / sum;
        vr_agr_cu = vr_agr_cu / sum;
        vr_agr_cc = vr_agr_cc / sum;
    }
    float t = VR_AGR_TierAt(VR_AGR_Rand());
    float rw = VR_AGR_Rand();
    float rd = VR_AGR_Rand();
    float rg = VR_AGR_Rand();
    if(t > 0)
    {
        vr_agr_code = VR_AGR_TierPick(t, rw);
        vr_agr_tier = t;
    }
    if(!VR_AG_Decode(vr_agr_code))
    {
        vr_agr_code = native;
        vr_agr_tier = 0;
        VR_AG_Decode(native);
    }
    float w1 = vr_ag_dec_wid;
    float f1 = vr_ag_dec_flags;
    if(rd < VR_AGR_Unit(cvar("vr_agrunt_repl_dual") * m) && VR_AG_Pairable(w1, f1) && !VR_AG_Big(w1, f1))
    {
        // A second weapon that pairs, from the same rolls (its own gun among them; a few tries, else one weapon).
        float k;
        for(k = 0; k < 12 && vr_agr_code2 == 0; k++)
        {
            float t2 = VR_AGR_TierAt(VR_AGR_Rand());
            float c2 = t2 > 0 ? VR_AGR_TierPick(t2, VR_AGR_Rand()) : native;
            if(c2 > 0 && VR_AG_Decode(c2) && VR_AG_Pairs(w1, f1, vr_ag_dec_wid, vr_ag_dec_flags))
            {
                vr_agr_code2 = c2;
            }
        }
    }
    else if(rg < VR_AGR_Unit(cvar("vr_agrunt_repl_gunaxe") * m) && VR_AG_DualGun(w1, f1))
    {
        vr_agr_code2 = WID_AXE;
        vr_agr_gunaxe = 1;
    }
};

// ----------------------------------------------------------------------------
// The replacement
// ----------------------------------------------------------------------------

// Its own gun's props marked native (VR_AG_ShootProp: its monster's own shots).
void(entity g) VR_AGR_MarkNative =
{
    float native = VR_AGR_NativeOf(g.vr_ag_kind);
    if(g.vr_ag_prop && g.vr_ag_prop.weapon == native)
    {
        g.vr_ag_prop.vr_ag_native = 1;
    }
    if(g.vr_ag_prop2 && g.vr_ag_prop2.weapon == native)
    {
        g.vr_ag_prop2.vr_ag_native = 1;
    }
};

// monster_army (`kind` 0), monster_enforcer (1): its armed version in its place. FALSE: not replaced (off, out of
// scope, a dummy, MG3's infected, Honey's trigger-spawned copies' maker, VR_AM_Spawn's own call).
float(float kind) VR_AGR_Replace =
{
    if(vr_am_spawning || vr_agr_hold > 0 || !cvar("vr_agrunt_repl"))
    {
        return FALSE;
    }
    if(self.classname != (kind == VR_AM_ENFORCER ? "monster_enforcer" : "monster_army") || self.vr_mg3_infected ||
       (!VR_OtherFlags() && (self.spawnflags & (SPAWNFLAG_TRIGGER_SPAWN | SPAWNFLAG_TRIGGER_SPAWN_MULTIPLE))) ||
       !VR_AGR_InScope())
    {
        return FALSE;
    }
    self.vr_agr = 1;
    self.vr_agr_seed = VR_AGR_SeedOf(self, kind);
    self.weapon = VR_AGR_NativeOf(kind);
    self.weaponflags = 0;
    self.weapon2 = 0;
    self.weaponflags2 = 0;
    if(VR_AGR_Varied())
    {
        if(find(world, classname, "player"))
        {
            VR_AGR_Pick(kind, self.vr_agr_seed); // (a spawner's, a player in: rolled now)
            self.weapon = vr_agr_code;
            self.weapon2 = vr_agr_code2;
            self.vr_ag_gunaxe = vr_agr_gunaxe;
        }
        else
        {
            self.vr_agr_pending = 1; // (the map's own: before any player, its own gun meanwhile)
            vr_agr_npending = vr_agr_npending + 1;
        }
    }
    entity me = self;
    VR_AM_Spawn(kind);
    if(me.vr_ag)
    {
        VR_AGR_MarkNative(me);
        vr_agr_made = vr_agr_made + 1;
    }
    return TRUE;
};

// A held weapon gone with nothing left (its record too: a loadout changed before anyone saw it).
void(entity p) VR_AGR_Unmake =
{
    if(!p)
    {
        return;
    }
    if(p.weaponinst)
    {
        remove(p.weaponinst);
    }
    p.weaponinst = world;
    remove(p);
};

// `g`'s loadout rolled and taken up (its own gun meanwhile: VR_AGR_Replace).
void(entity g) VR_AGR_Resolve =
{
    g.vr_agr_pending = 0;
    VR_AGR_Pick(g.vr_ag_kind, g.vr_agr_seed);
    if(vr_agr_code == VR_AGR_NativeOf(g.vr_ag_kind) && vr_agr_code2 == 0)
    {
        return; // (its own gun, as it spawned)
    }
    entity oldself = self;
    self = g;
    VR_AGR_Unmake(self.vr_ag_prop);
    VR_AGR_Unmake(self.vr_ag_prop2);
    self.vr_ag_prop = world;
    self.vr_ag_prop2 = world;
    self.vr_ag_wid = -1;
    self.vr_ag_wid2 = -1;
    self.vr_ag_dual = 0;
    self.vr_ag_gunaxe = vr_agr_gunaxe;
    float want = vr_agr_code;
    float want2 = vr_agr_code2;
    entity inst = world;
    VR_AG_Decode(want);
    want = vr_ag_dec_wid;
    float flags = vr_ag_dec_flags;
    if(vr_ag_dec_up)
    {
        inst = WeaponInst_New(0);
        inst.wi_adup = vr_ag_dec_up;
        inst.wi_clip = VR_AG_ClipOf(want, inst);
    }
    self.vr_ag_code = vr_agr_code;
    vr_ag_arm_rolled = want2 == 0; // (a flare gun too: its seed's next number, after its loadout's)
    vr_ag_arm_roll = VR_AGR_Rand();
    VR_AM_ArmAll(want, flags, inst, want2, 0);
    VR_AGR_MarkNative(self);
    VR_AG_Log(sprintf("replacement rolled: %s%s%s", self.vr_ag_prop ? self.vr_ag_prop.netname : "fists",
        self.vr_ag_prop2 ? " and " : "", self.vr_ag_prop2 ? self.vr_ag_prop2.netname : ""));
    self = oldself;
};

// StartFrame: the map's replacements waiting for a player are rolled once one is in.
void() VR_AGR_Frame =
{
    if(vr_agr_npending <= 0 || !find(world, classname, "player"))
    {
        return;
    }
    vr_agr_npending = 0;
    entity e = findflags(world, vr_agr_pending, 1);
    while(e)
    {
        entity next = findflags(e, vr_agr_pending, 1);
        if(e.vr_ag && e.health > 0)
        {
            VR_AGR_Resolve(e);
        }
        e.vr_agr_pending = 0;
        e = next;
    }
};

// A weapon code's model and sounds made ready (as VR_AG_MakeProp's).
void(float code) VR_AGR_PrecacheCode =
{
    if(!VR_AG_Decode(code))
    {
        return;
    }
    VR_ADGun_Placed(vr_ag_dec_wid);
    VR_ADMelee_Placed(vr_ag_dec_wid, vr_ag_dec_flags);
    precache_model(AD_WeaponModel(vr_ag_dec_wid, vr_ag_dec_flags, world));
};

// worldspawn: with replacements on, their bodies, their sounds, their own guns and every weapon the rolls may give
// precached at the level's load (none late: a spawner's later ones too).
void() VR_AGR_Precache =
{
    if(!cvar("vr_agrunt_repl"))
    {
        return;
    }
    float k;
    for(k = 0; k < VR_AM_COUNT; k++)
    {
        VR_AM_Row(k);
        precache_model(vr_am_model);
        VR_AGR_PrecacheCode(VR_AGR_NativeOf(k));
    }
    precache_sound("weapons/lock4.wav");
    precache_sound("weapons/pkup.wav");
    precache_sound("weapons/ax1.wav");
    precache_sound("weapons/tink1.wav");
    VR_AG_PlayPrecache();
    if(!VR_AGR_Varied())
    {
        return;
    }
    float t;
    float i;
    for(t = 1; t <= 3; t++)
    {
        for(i = 0; VR_AGR_TierCode(t, i) >= 0; i++)
        {
            float code = VR_AGR_TierCode(t, i);
            if(!VR_IsADGun(code) || AD_Rules())
            {
                VR_AGR_PrecacheCode(code);
            }
        }
    }
};

// ----------------------------------------------------------------------------
// Its own gun: its monster's attack (soldier's army_atk1..9, enforcer's enf_atk1..14)
// ----------------------------------------------------------------------------

// Its one weapon is its own gun (two held: the armed layer's dual attacks, each native gun's shots its monster's).
float(entity g) VR_AGR_NativeAlone = { return g.vr_ag_prop && g.vr_ag_prop.vr_ag_native && !g.vr_ag_prop2; };

// Its monster's chance to fire at range `r` (SoldierCheckAttack's; the enforcer's CheckAttack's, no melee attack).
float(float r) VR_AGR_NativeChance =
{
    if(self.vr_ag_kind == VR_AM_ENFORCER)
    {
        return r == RANGE_MELEE ? 0.9 : r == RANGE_NEAR ? 0.4 : r == RANGE_MID ? 0.1 : 0;
    }
    return r == RANGE_MELEE ? 0.9 : r == RANGE_NEAR ? 0.4 : r == RANGE_MID ? 0.05 : 0;
};

// ... its pause after an attack.
void() VR_AGR_NativeFinished =
{
    if(self.vr_ag_kind == VR_AM_ENFORCER)
    {
        SUB_AttackFinished(2 * random());
        return;
    }
    SUB_AttackFinished(1 + random());
};

// Its attack's step `s` (1..): the row's aiming frame drawn (VR_AM_A_ATK's index; -1: the attack is over). The
// enforcer's attack1..8 then attack5..10 again (its row's frames are attack2..10); the grunt's shoot1..9.
float(float kind, float s) VR_AGR_NatFrame =
{
    if(kind == VR_AM_ENFORCER)
    {
        if(s > 14) { return -1; }
        if(s <= 8) { return max(0, s - 2); }
        return s - 6;
    }
    return s > 9 ? -1 : s - 1;
};

// ... it fires then (the grunt: shoot5, its burst's two more in shoot6, 7; the enforcer: both attack6s).
float(float kind, float s) VR_AGR_NatFires =
{
    if(kind == VR_AM_ENFORCER)
    {
        return s == 6 || s == 10;
    }
    return s == 5 || ((s == 6 || s == 7) && cvar_hget(cvarh_vr_grunt_burst));
};

// ... a nightmare's refire is checked then (SUB_CheckRefire: army_atk7, enf_atk14).
float(float kind, float s) VR_AGR_NatRefire = { return kind == VR_AM_ENFORCER ? s == 14 : s == 7; };

void() VR_AGR_NatStep;
void() VR_AGR_NatRetry = { self.vr_ag_nstep = self.vr_ag_nstep - 1; VR_AGR_NatStep(); }; // (its step again)

void() VR_AGR_NatStep =
{
    float s = self.vr_ag_nstep + 1;
    self.vr_ag_nstep = s;
    float idx = VR_AGR_NatFrame(self.vr_ag_kind, s);
    entity p = self.vr_ag_prop;
    if(idx < 0 || !VR_AGR_NativeAlone(self))
    {
        VR_AG_AfterAttack(); // (over, or its gun gone or a second taken mid-attack)
        return;
    }
    VR_AM_Anim(VR_AM_A_ATK, idx, VR_AGR_NatStep);
    ai_face();
    if(!self.enemy)
    {
        return;
    }
    if(s >= 3)
    {
        vr_ag_aim_mult = -1; // (its gun drawn on its target: no aim error, the monster's own shots have their own)
        self.vr_ag_aim = VR_AG_AimDir(p.origin);
        vr_ag_aim_mult = 0;
    }
    if(VR_AGR_NatFires(self.vr_ag_kind, s))
    {
        // Far off its facing it waits, turning, as its monster does (vr_aimcone.qc: army_atk5's, enforcer_fire's;
        // VR_AGR_NatRetry fires this step when the wait ends).
        if((self.vr_ag_kind == VR_AM_ENFORCER || s == 5) && VR_AimCone_Wait(VR_AGR_NatRetry, VR_AIM_NARROW))
        {
            return;
        }
        VR_AG_Shoot();
    }
    if(VR_AGR_NatRefire(self.vr_ag_kind, s) && skill == 3 && self.cnt != 1 && visible(self.enemy))
    {
        self.cnt = 1; // (SUB_CheckRefire's: once more on nightmare)
        self.vr_ag_nstep = 0;
    }
};

void() VR_AGR_NativeAttack =
{
    self.vr_ag_nstep = 0;
    self.vr_ag_shots = 0;
    VR_AGR_NatStep();
};

// A native gun's shot: its monster's own (army_burst_round, army_fire; enforcer_fire's laser), its own damage
// (VR_AG_DamageScale passes it), never spent; drawn on the held gun (its flash, its kick).
void(entity p) VR_AGR_NativeShot =
{
    makevectors(self.angles);
    vr_agr_native_fire = 1;
    if(p.weapon == WID_ENFRIFLE)
    {
        // enforcer_fire's (not its aim cone's wait: the attack's frames face the target already)
        vector org = self.origin + v_forward * 30 + v_right * 8.5 + '0 0 16';
        if(VR_AI_ShotReady(org))
        {
            entity laser = LaunchLaser(org, VR_AI_Origin(self.enemy) - self.origin);
            laser.vr_ag_native = 1;
            MG3_EnforcerAltLaser(org);
            vr_agr_shots_enf = vr_agr_shots_enf + 1;
        }
    }
    else if(cvar_hget(cvarh_vr_grunt_burst))
    {
        army_burst_round();
    }
    else
    {
        army_fire();
    }
    vr_agr_native_fire = 0;
    weaponfired(p, -1); // its muzzle flash on the held weapon (vr_weaponfx.cpp)
    self.effects = self.effects | EF_MUZZLEFLASH;
    p.vr_ag_kick_at = time;
    self.vr_ag_kick_at = time;
    self.vr_ag_shots = self.vr_ag_shots + 1;
    self.vr_ag_total_shots = self.vr_ag_total_shots + 1;
    p.vr_ag_total_shots = p.vr_ag_total_shots + 1;
    vr_ag_shots_fired = vr_ag_shots_fired + 1;
};

// ----------------------------------------------------------------------------
// A gun and an axe (vr_agrunt_repl_gunaxe; vr_ag_gunaxe): the gun, the axe up close or with the gun spent
// ----------------------------------------------------------------------------

float VR_AGR_AXE_NEAR = 84;  // its target nearer than this: the axe (a swing starts within VR_AG_GRIP_REACH + 10)
float VR_AGR_GUN_FAR = 150;  // ... further than this, the gun again while it has rounds (in it or in reserve)

// VR_AG_CheckAttack: the weapon its layer acts on (the first slot) the gun or the axe, its target `d` away.
void(float d) VR_AGR_GunAxe =
{
    entity a = self.vr_ag_prop;
    entity b = self.vr_ag_prop2;
    if(!a || !b || self.vr_ag_tug_pl || self.vr_ag_took_at > 0)
    {
        return;
    }
    float onAxe = VR_AG_Class(a.weapon) == VR_AG_CLASS_MELEE;
    if(onAxe == (VR_AG_Class(b.weapon) == VR_AG_CLASS_MELEE))
    {
        return; // (two guns or two blades: not a gun and an axe any more)
    }
    entity gun = onAxe ? b : a;
    float rounds = gun.vr_ag_native || gun.weaponinst.wi_clip > 0 || (onAxe ? self.vr_ag_reserve2 : self.vr_ag_reserve) > 0;
    if(!rounds)
    {
        // Spent, nothing to reload: thrown away (VR_AG_AfterAttack's), the axe in hand; out of its target's reach it
        // goes for a gun lying near (VR_AG_WantsSecond, VR_AG_PairsFor), else it fights on with the axe.
        if(onAxe)
        {
            VR_AG_Swap(self);
        }
        makevectors(self.angles);
        VR_AG_DropHeld(self, v_right * 90 + v_forward * 40 + '0 0 120');
        self.vr_agr_switches = self.vr_agr_switches + 1;
        VR_AG_Log("gun and axe: its gun spent: thrown away, the axe");
        return;
    }
    if(!onAxe && d < VR_AGR_AXE_NEAR)
    {
        VR_AG_Swap(self);
        self.vr_agr_switches = self.vr_agr_switches + 1;
        VR_AG_Log(sprintf("gun and axe: the axe (%.0f away)", d));
    }
    else if(onAxe && d > VR_AGR_GUN_FAR && rounds)
    {
        VR_AG_Swap(self);
        self.vr_agr_switches = self.vr_agr_switches + 1;
        VR_AG_Log(sprintf("gun and axe: the gun (%.0f away)", d));
    }
};
