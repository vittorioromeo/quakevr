//
//
//
// ----------------------------------------------------------------------------
// Armed grunts (experimental; docs/vr-port/ROUND22.md, "Armed grunts")
// ----------------------------------------------------------------------------
//
// monster_vr_grunt: a grunt holding one of your weapons, drawn as the weapon lying about is (its model at the grunt's
// right hand: classname vr_ag_weapon, its record in .weaponinst), fired with your weapons' own code (W_FireShotgun,
// W_FireRocket, ... with `self` the grunt and its hand fields set from the held weapon: the same pellets, projectiles,
// sounds, smoke and damage path), its damage to players times vr_agrunt_damage_scale. Its classname is monster_army
// (.vr_ag 1): to everything else it is a grunt (ragdolls, knockdowns, hit reactions, decapitation, holding, shoves).
//
// - The model: progs/vrgrunt.mdl, the grunt without its gun (Misc/quakevr/make_vrgrunt.py: the gun's triangles dropped,
//   its vertices kept). Each server frame the weapon goes to the grunt's right hand (VR_AG_HAND_VERTS, as drawn now:
//   modelvertex), along the line the grunt's own gun had in that frame (its rear to its muzzle; its top for its roll):
//   the hands its animations close round the rifle hold it there. Aiming, it points at its target; a shot kicks it back;
//   a melee blow swings it.
// - Its weapon: the map's or the toolgun's "weapon" key (WID_*; -1 its fists; 0, unset: one from the pool, each weapon's
//   weight vr_agrunt_w_*), "weaponflags" its flags.
// - Ranged weapons: the grunt's own decision (SoldierCheckAttack's sight line and attack_finished), each weapon's chances
//   by range (VR_AG_Chance), its shots in the grunt's fire frames (shoot1..9: one shot, a burst, or one a frame for the
//   nail guns and the thunderbolt), aimed a little behind a moving target and up to vr_agrunt_aim_error degrees off.
//   With an explosive weapon it backs away from a target nearer than VR_AG_MinRange (its own blast) and clubs one at
//   arm's length.
// - Melee weapons and fists: it closes in and swings (the shove's frames; W_FireAxe, W_ChainsawMelee, HIP_FireMjolnir,
//   W_FistMelee from its hand towards the target).
// - Ammo: the weapon's clip (its record's wi_clip; its kind's clip size, VR_AG_ClipSize) and vr_agrunt_reserve clips more
//   (not for single-use enemy guns: their clip is all they have). Empty, it reloads (the load frames, vr_agrunt_reload_time
//   s); with nothing left it throws the gun away. Dying, it drops the weapon with what is left in it (gibbed too) and its
//   reserve in its backpack.
// - Disarming: a blow of at least vr_agrunt_disarm_damage on its gun hand (vr_agrunt_disarm), your empty hand grabbing
//   its weapon (vr_agrunt_disarm_yank: it is yours), a knockdown: the weapon falls (a lying weapon, its clip with it).
// - Re-arming (vr_agrunt_rearm): disarmed it fights with its fists and goes for a weapon lying within
//   vr_agrunt_rearm_range when its target is not nearer to that weapon than it is (one at its feet, within 64 units:
//   always), then fights with that one.

float VR_AG_CLASS_FISTS = 0;     // its fists
float VR_AG_CLASS_MELEE = 1;     // a melee weapon
float VR_AG_CLASS_GUN = 2;       // one shot an attack (the shotguns, the laser rifle)
float VR_AG_CLASS_EXPLOSIVE = 3; // one round an attack, kept away from its target (the launchers)
float VR_AG_CLASS_AUTO = 4;      // one a frame (the nail guns, the thunderbolt, the laser cannon)
float VR_AG_CLASS_BURST = 5;     // the grunts' burst rifle: three rounds

float VR_AG_GRIP_REACH = 52;     // a melee attack starts this near (the boxes' middles)
float VR_AG_PICKUP_REACH = 40;   // it takes a weapon this near
float VR_AG_DISARM_HAND = 10;    // a blow this near its gun hand (units) disarms it
float VR_AG_SWING_TIME = 0.4;    // a melee swing's drawn arc (seconds)
float VR_AG_EF_HELD = 128;       // the held weapon's effects bit: a weapon in a monster's hand (the engine's vr_armik.cpp,
                                 // vr_weaponfx.cpp: the arms' grips, its muzzle flash); no other effect uses it

// Its right hand's vertices (their middle: where it holds the weapon) and its hidden gun's: the rear, the muzzle, the
// top and the bottom (Quake VR's soldier.mdl, vertices 463..548 the gun: scratch analysis, ROUND22.md).
float VR_AG_HAND_V1 = 32;
float VR_AG_HAND_V2 = 26;
float VR_AG_HAND_V3 = 31;
float VR_AG_GUN_REAR1 = 497;
float VR_AG_GUN_REAR2 = 545;
float VR_AG_GUN_MUZZLE1 = 498;
float VR_AG_GUN_MUZZLE2 = 514;
float VR_AG_GUN_TOP = 529;
float VR_AG_GUN_BOTTOM = 496;

// Where vertex `v` of e's model is as drawn now (trace_fraction 0: none), and the muzzle of the weapon model e is drawn
// with (the engine's modelanchorpoint and weapondrawnpose: builtins.qc).
vector(entity e, float v) modelvertex =
{
    vector a = '0 0 0';
    a_x = -2 - v;
    return modelanchorpoint(e, a);
};
vector(entity e) weaponmuzzle = { return weapondrawnpose(e, 0, 6); };

.float vr_ag_code;      // its "weapon" key as spawned (a weapon id, or VR_AG_CODE_*: the tests' names)
.vector vr_ag_aim;      // the way it aims now (a unit vector; '0 0 0': as its animation holds the gun)
.float vr_ag_myrun;     // when its own run frames last ran (VR_AG_CheckAttack sends it back to them)
.float vr_ag_lastwid;   // the last weapon it held (its backpack's ammo: VR_AG_SoldierDrop)

// The pool's pick (VR_AG_PoolAdd: one pass, each weapon kept with its weight's share of the weights so far).
float vr_ag_pool_total;
float vr_ag_pool_pick;
float vr_ag_shots_fired; // every armed grunt's shots (the tests)
float vr_ag_player_dmg;  // the damage armed grunts dealt players, scaled (the tests)

void(string s) VR_AG_Log =
{
    if(cvar_hget(cvarh_vr_agrunt_debug))
    {
        dprint(sprintf("agrunt: %s\n", s));
    }
};

float(float wid) VR_AG_Class =
{
    if(wid < 0 || wid == WID_FIST) { return VR_AG_CLASS_FISTS; }
    if(wid == WID_AXE || wid == WID_SWORD || wid == WID_CROWBAR || wid == WID_CHAINSAW || wid == WID_MJOLNIR ||
       wid == WID_SUPERAXE)
    {
        return VR_AG_CLASS_MELEE;
    }
    if(wid == WID_GRENADE_LAUNCHER || wid == WID_ROCKET_LAUNCHER || wid == WID_PROXIMITY_GUN || wid == WID_ADROCKET ||
       wid == WID_ADGRENADE)
    {
        return VR_AG_CLASS_EXPLOSIVE;
    }
    if(wid == WID_NAILGUN || wid == WID_SUPER_NAILGUN || wid == WID_LIGHTNING || wid == WID_LASER_CANNON ||
       wid == WID_ADPYRO || wid == WID_ADFUMIGATOR)
    {
        return VR_AG_CLASS_AUTO;
    }
    if((wid == WID_GRUNTGUN && cvar_hget(cvarh_vr_grunt_burst)) || wid == WID_ADPLASMA || wid == WID_ADCENTURION)
    {
        return VR_AG_CLASS_BURST; // (AD's plasma guns: three bolts an attack)
    }
    return VR_AG_CLASS_GUN;
};

// A flamethrower's stream (AD's pyro's and fumigator's: two flames a fire frame, near only).
float(float wid) VR_AG_Stream = { return wid == WID_ADPYRO || wid == WID_ADFUMIGATOR; };

// Whether it can use `wid` (what it can pick up, what the pool and the keys may give it): your weapons, the enemy guns
// it knows, Arcane Dimensions' soldiers' guns while AD's data is loaded; not the grapple or the toolgun.
float(float wid) VR_AG_Usable =
{
    if(wid == WID_FIST) { return TRUE; }
    if(VR_IsADGun(wid)) { return wid != WID_ADELIMINATOR && VR_ADGun_Available(); }
    if(wid < WID_AXE || wid > WID_SUPERAXE || wid == WID_TOOLGUN) { return FALSE; }
    return VR_Pack_WeaponAvailable(wid);
};

// A full clip of `wid` (your guns' reloading clip; one that has none: 20; AD's guns' rounds or fuel).
float(float wid) VR_AG_ClipSize =
{
    if(VR_AG_Class(wid) == VR_AG_CLASS_MELEE || VR_AG_Class(wid) == VR_AG_CLASS_FISTS) { return 0; }
    if(VR_IsADGun(wid)) { return VR_ADGun_ClipSize(wid); }
    float c = WeaponIdToBaseClipSize(wid);
    return c > 0 ? c : 20;
};

// ... of the weapon `wid` whose record is `inst` (the Widowmaker's: its whole tube).
float(float wid, entity inst) VR_AG_ClipOf =
{
    if(wid == WID_SUPER_SHOTGUN && inst && AD_Widowmaker(inst))
    {
        return VR_ClipSizeOf(wid, inst);
    }
    return VR_AG_ClipSize(wid);
};

// Its ammo is a clip of its own only (single use): the enemy guns.
float(float wid) VR_AG_SingleUse = { return VR_IsEnemyGun(wid); };

// Nearer than this, it doesn't fire an explosive weapon (its own blast).
float(float wid) VR_AG_MinRange =
{
    if(wid == WID_ROCKET_LAUNCHER || wid == WID_ADROCKET) { return 190; }
    if(wid == WID_GRENADE_LAUNCHER || wid == WID_PROXIMITY_GUN || wid == WID_ADGRENADE) { return 160; }
    return 0;
};

// The chance it fires this frame at a target in sight at range `r` (RANGE_*): each weapon's best ranges.
float(float wid, float r) VR_AG_Chance =
{
    float c = VR_AG_Class(wid);
    if(c == VR_AG_CLASS_EXPLOSIVE)
    {
        if(r == RANGE_NEAR) { return 0.35; }
        if(r == RANGE_MID) { return 0.25; }
        return (wid == WID_ROCKET_LAUNCHER && r == RANGE_FAR) ? 0.05 : 0;
    }
    if(wid == WID_LIGHTNING || VR_AG_Stream(wid))
    {
        return r == RANGE_MELEE ? 0.9 : r == RANGE_NEAR ? 0.5 : 0;
    }
    if(wid == WID_SUPER_SHOTGUN)
    {
        return r == RANGE_MELEE ? 0.9 : r == RANGE_NEAR ? 0.5 : r == RANGE_MID ? 0.03 : 0;
    }
    if(c == VR_AG_CLASS_AUTO)
    {
        return r == RANGE_MELEE ? 0.9 : r == RANGE_NEAR ? 0.45 : r == RANGE_MID ? 0.15 : 0;
    }
    return r == RANGE_MELEE ? 0.9 : r == RANGE_NEAR ? 0.4 : r == RANGE_MID ? 0.08 : 0; // (the grunt's own)
};

// ----------------------------------------------------------------------------
// Its weapon: the pool, the held prop
// ----------------------------------------------------------------------------

// The "weapon" keys beyond the weapon ids (the toolgun's and the pool's): AD's upgrades (the weapon it upgrades, its
// record's upgrade) and AD's melee weapons (the knight's sword of that kind: VR_ADMelee_Flags).
float VR_AG_CODE_SHADOWAXE = 101;
float VR_AG_CODE_WIDOWMAKER = 102;
float VR_AG_CODE_PLASMAGUN = 103;
float VR_AG_CODE_ADMELEE = 110; // 111..116: kind 1..6 (VR_ADMELEE_*)

float vr_ag_dec_wid;   // VR_AG_Decode's: the weapon,
float vr_ag_dec_flags; // ... its flags,
float vr_ag_dec_up;    // ... its record's AD upgrade (0: none)

// A "weapon" key `code` as a weapon, its flags and its upgrade (vr_ag_dec_*); FALSE: none it can hold.
float(float code) VR_AG_Decode =
{
    vr_ag_dec_wid = code;
    vr_ag_dec_flags = 0;
    vr_ag_dec_up = 0;
    if(code == VR_AG_CODE_SHADOWAXE || code == VR_AG_CODE_WIDOWMAKER || code == VR_AG_CODE_PLASMAGUN)
    {
        vr_ag_dec_wid = code == VR_AG_CODE_SHADOWAXE ? WID_AXE : code == VR_AG_CODE_WIDOWMAKER ? WID_SUPER_SHOTGUN : WID_LIGHTNING;
        vr_ag_dec_up = AD_UpgradeFor(vr_ag_dec_wid);
        return vr_ad_data && VR_AD_FeatureScope(AD_UpgradeScope()); // (elsewhere a plain one: not this)
    }
    if(code > VR_AG_CODE_ADMELEE && code <= VR_AG_CODE_ADMELEE + VR_ADMELEE_LAST)
    {
        vr_ag_dec_wid = WID_SWORD;
        vr_ag_dec_flags = VR_ADMelee_Flags(code - VR_AG_CODE_ADMELEE);
        return cvar("vr_ad_mounted") != 0;
    }
    return VR_AG_Usable(code);
};

// Its name (the tests, the log).
string(float wid, float flags, entity inst) VR_AG_NameOf =
{
    if(wid < 0) { return "fists"; }
    if(inst && inst.wi_adup && vr_ad_data)
    {
        if(wid == WID_AXE) { return "Shadow Axe"; }
        if(wid == WID_SUPER_SHOTGUN) { return "Widowmaker"; }
        if(wid == WID_LIGHTNING) { return "Plasma Gun"; }
    }
    if(VR_IsADGun(wid)) { return VR_ADGun_Name(wid); }
    return WeaponNameOf(wid, flags);
};

void(float code, float w) VR_AG_PoolAdd =
{
    if(w <= 0 || !VR_AG_Decode(code))
    {
        return;
    }
    vr_ag_pool_total = vr_ag_pool_total + w;
    if(random() * vr_ag_pool_total < w)
    {
        vr_ag_pool_pick = code;
    }
};

// A weapon from the pool (-1: its fists).
float() VR_AG_PickWeapon =
{
    vr_ag_pool_total = 0;
    vr_ag_pool_pick = -1;
    VR_AG_PoolAdd(WID_FIST, cvar_hget(cvarh_vr_agrunt_w_fists));
    VR_AG_PoolAdd(WID_AXE, cvar_hget(cvarh_vr_agrunt_w_axe));
    VR_AG_PoolAdd(WID_SWORD, cvar_hget(cvarh_vr_agrunt_w_sword));
    VR_AG_PoolAdd(WID_CROWBAR, cvar_hget(cvarh_vr_agrunt_w_crowbar));
    VR_AG_PoolAdd(WID_CHAINSAW, cvar_hget(cvarh_vr_agrunt_w_chainsaw));
    VR_AG_PoolAdd(WID_SHOTGUN, cvar_hget(cvarh_vr_agrunt_w_shotgun));
    VR_AG_PoolAdd(WID_SUPER_SHOTGUN, cvar_hget(cvarh_vr_agrunt_w_ssg));
    VR_AG_PoolAdd(WID_NAILGUN, cvar_hget(cvarh_vr_agrunt_w_nailgun));
    VR_AG_PoolAdd(WID_SUPER_NAILGUN, cvar_hget(cvarh_vr_agrunt_w_sng));
    VR_AG_PoolAdd(WID_GRENADE_LAUNCHER, cvar_hget(cvarh_vr_agrunt_w_gl));
    VR_AG_PoolAdd(WID_ROCKET_LAUNCHER, cvar_hget(cvarh_vr_agrunt_w_rl));
    VR_AG_PoolAdd(WID_LIGHTNING, cvar_hget(cvarh_vr_agrunt_w_lg));
    VR_AG_PoolAdd(WID_GRUNTGUN, cvar_hget(cvarh_vr_agrunt_w_gruntgun));
    VR_AG_PoolAdd(WID_ENFRIFLE, cvar_hget(cvarh_vr_agrunt_w_enfrifle));
    VR_AG_PoolAdd(WID_MJOLNIR, cvar_hget(cvarh_vr_agrunt_w_packs));
    VR_AG_PoolAdd(WID_LASER_CANNON, cvar_hget(cvarh_vr_agrunt_w_packs));
    VR_AG_PoolAdd(WID_PROXIMITY_GUN, cvar_hget(cvarh_vr_agrunt_w_packs));
    // Arcane Dimensions' (VR_AG_Decode: only with its data; the upgrades where AD's upgrades play).
    VR_AG_PoolAdd(WID_ADROCKET, cvar("vr_agrunt_w_adrocket"));
    VR_AG_PoolAdd(WID_ADGRENADE, cvar("vr_agrunt_w_adgrenade"));
    VR_AG_PoolAdd(WID_ADPLASMA, cvar("vr_agrunt_w_adplasma"));
    VR_AG_PoolAdd(WID_ADDEFENDER, cvar("vr_agrunt_w_addefender"));
    VR_AG_PoolAdd(WID_ADCENTURION, cvar("vr_agrunt_w_adcenturion"));
    VR_AG_PoolAdd(WID_ADPYRO, cvar("vr_agrunt_w_adpyro"));
    VR_AG_PoolAdd(WID_ADFUMIGATOR, cvar("vr_agrunt_w_adfumigator"));
    VR_AG_PoolAdd(WID_ADCROSSBOW, cvar("vr_agrunt_w_adcrossbow"));
    VR_AG_PoolAdd(VR_AG_CODE_SHADOWAXE, cvar("vr_agrunt_w_shadowaxe"));
    VR_AG_PoolAdd(VR_AG_CODE_WIDOWMAKER, cvar("vr_agrunt_w_widowmaker"));
    VR_AG_PoolAdd(VR_AG_CODE_PLASMAGUN, cvar("vr_agrunt_w_plasmagun"));
    for(float k = 1; k <= VR_ADMELEE_LAST; k++)
    {
        VR_AG_PoolAdd(VR_AG_CODE_ADMELEE + k, cvar(VR_ADMelee_Heavy(k) ? "vr_agrunt_w_adheavy" : "vr_agrunt_w_adsword"));
    }
    if(vr_ag_pool_pick == WID_FIST)
    {
        return -1;
    }
    return vr_ag_pool_pick;
};

// `v` rotated about the unit axis `k` by `deg` degrees (Rodrigues; vr_axestick.qc has the same).
vector(vector v, vector k, float deg) VR_AG_Rotate =
{
    float c = cos(deg * (3.14159265 / 180));
    float s = sin(deg * (3.14159265 / 180));
    vector kv;
    kv_x = k_y * v_z - k_z * v_y;
    kv_y = k_z * v_x - k_x * v_z;
    kv_z = k_x * v_y - k_y * v_x;
    return v * c + kv * s + k * ((k * v) * (1 - c));
};

// The held weapon to the grunt's right hand as drawn now, along its hidden gun's line (a melee weapon upright),
// pointing where it aims, kicked back by a shot, swung by a blow. Its view angles (VR_AxeStick_Angles') in .v_angle:
// the shots' and a drop's.
void(entity g, entity p) VR_AG_PlaceProp =
{
    vector hand = (modelvertex(g, VR_AG_HAND_V1) + modelvertex(g, VR_AG_HAND_V2) + modelvertex(g, VR_AG_HAND_V3)) * (1 / 3);
    if(trace_fraction == 0)
    {
        return; // (no such model now: a ragdoll's, another's; it stays where it was)
    }
    vector rear = (modelvertex(g, VR_AG_GUN_REAR1) + modelvertex(g, VR_AG_GUN_REAR2)) * 0.5;
    vector muzzle = (modelvertex(g, VR_AG_GUN_MUZZLE1) + modelvertex(g, VR_AG_GUN_MUZZLE2)) * 0.5;
    vector up = modelvertex(g, VR_AG_GUN_TOP) - modelvertex(g, VR_AG_GUN_BOTTOM);
    vector fw = normalize(muzzle - rear);
    if(VR_AG_Class(g.vr_ag_wid) == VR_AG_CLASS_MELEE && g.vr_ag_wid != WID_CHAINSAW)
    {
        // A blade or a club over its shoulder (the rifle's line the other way: its rear rests there), its edge ahead.
        fw = -fw;
        up = -up;
    }
    if(g.vr_ag_aim != '0 0 0')
    {
        fw = g.vr_ag_aim;
    }
    up = normalize(up - fw * (up * fw));
    if(up == '0 0 0')
    {
        up = '0 0 1';
    }
    vector right;
    right_x = fw_y * up_z - fw_z * up_y;
    right_y = fw_z * up_x - fw_x * up_z;
    right_z = fw_x * up_y - fw_y * up_x;

    // A melee blow's arc: raised back, then down through the target.
    float s = (time - g.vr_ag_swing_at) / VR_AG_SWING_TIME;
    if(g.vr_ag_swing_at > 0 && s >= 0 && s < 1)
    {
        float deg = 60 - 150 * (s * s * (3 - 2 * s));
        fw = VR_AG_Rotate(fw, right, deg);
        up = VR_AG_Rotate(up, right, deg);
        // The hand goes with it (the client's arm follows the grip: vr_agrunt_ik): round its right shoulder (the rig's
        // upperarm_r joint, about), most of the blade's turn.
        makevectors([0, g.angles_y, 0]);
        vector shoulder = g.origin + v_up * 12 + v_right * 9.7;
        hand = shoulder + VR_AG_Rotate(hand - shoulder, v_right, deg * 0.6);
    }

    // A shot's kick: back along it and its muzzle up, dying out in 0.15 s.
    float k = 1 - (time - g.vr_ag_kick_at) / 0.15;
    if(g.vr_ag_kick_at > 0 && k > 0)
    {
        hand = hand - fw * (2.5 * k);
        fw = VR_AG_Rotate(fw, right, 9 * k);
        up = VR_AG_Rotate(up, right, 9 * k);
    }

    vector a = VR_AxeStick_Angles(fw, up);
    p.v_angle = a;
    p.angles = a;
    p.angles_x = -a_x; // (a model's pitch)
    setorigin(p, hand);
};

void() VR_AG_PropHandTouch;
void(entity g) VR_AG_KnockedDown;

// Every server frame: to its hand; gone with its grunt.
void() VR_AG_PropThink =
{
    entity g = self.owner;
    if(g.vr_ag_prop != self || !g.vr_ag || g.health <= 0)
    {
        self.weaponinst = world; // (its record goes with a drop, if any: never freed here)
        remove(self);
        return;
    }
    if(g.vr_knockdown && cvar_hget(cvarh_vr_agrunt_disarm))
    {
        VR_AG_KnockedDown(g);
        return;
    }
    VR_AG_PlaceProp(g, self);
    self.nextthink = time;
};

// `g` takes up a weapon (its record `inst`: its magazine).
void(entity g, float wid, float flags, entity inst) VR_AG_Arm =
{
    g.vr_ag_wid = wid;
    g.vr_ag_wflags = flags;
    g.vr_ag_lastwid = wid;
    g.vr_ag_aim = '0 0 0';
    g.vr_ag_seek = world;
    if(wid < 0 || wid == WID_FIST)
    {
        g.vr_ag_wid = -1;
        g.vr_ag_prop = world;
        return;
    }
    entity p = spawn();
    p.classname = "vr_ag_weapon";
    p.owner = g;
    p.weapon = wid;
    p.weaponflags = flags;
    p.weaponinst = inst ? inst : WeaponInst_New(VR_AG_ClipSize(wid));
    p.netname = VR_AG_NameOf(wid, flags, p.weaponinst);
    p.movetype = MOVETYPE_NONE;
    p.solid = SOLID_TRIGGER;
    p.effects = VR_AG_EF_HELD; // (the client finds it so: its muzzle flash, the arms' grips)
    VR_ADGun_Placed(wid); // (AD's guns' and melee weapons' models made, their sounds precached: any map)
    VR_ADMelee_Placed(wid, flags);
    string m = AD_WeaponModel(wid, flags, p.weaponinst);
    precache_model(m);
    setmodel(p, m);
    setsize(p, '-10 -10 -10', '10 10 10');
    p.handtouch = VR_AG_PropHandTouch;
    p.think = VR_AG_PropThink;
    p.nextthink = time;
    g.vr_ag_prop = p;
    VR_AG_PlaceProp(g, p);
};

// Its weapon leaves its hand as a weapon lying about (its record and magazine with it), flying at `vel`; world if it
// held none.
entity(entity g, vector vel) VR_AG_DropHeld =
{
    entity p = g.vr_ag_prop;
    if(!p)
    {
        return world;
    }
    entity inst = p.weaponinst;
    entity w = CreateThrownWeapon(g, p.weapon, p.weaponflags, inst, p.origin, p.v_angle, vel, randomVec(90),
        FALSE /* disappear on hit */, FALSE /* deal damage */, WeaponIdToThrowDamage(p.weapon));
    VR_AG_Log(sprintf("%s dropped (%g in its clip, %g in reserve)", p.netname, inst.wi_clip, g.vr_ag_reserve));
    p.weaponinst = world;
    remove(p);
    g.vr_ag_prop = world;
    g.vr_ag_wid = -1;
    g.vr_ag_wflags = 0;
    g.vr_ag_aim = '0 0 0';
    g.vr_ag_seek_next = time + 0.5 + random() * 0.5;
    return w;
};

// Disarmed: the weapon knocked out of its hand (`dir`: the blow's way).
void(entity g, vector dir, float speed, string why) VR_AG_Disarm =
{
    if(!g.vr_ag_prop)
    {
        return;
    }
    dir_z = 0;
    vector vel = normalize(dir) * (speed * (0.8 + random() * 0.4)) + '0 0 1' * (speed * 0.9);
    VR_AG_DropHeld(g, vel);
    g.vr_ag_disarms = g.vr_ag_disarms + 1;
    g.vr_ag_reserve = 0; // (its spare clips are no use without the gun)
    sound(g, CHAN_AUTO, "weapons/tink1.wav", 1, ATTN_NORM);
    VR_AG_Log(sprintf("disarmed (%s)", why));
};

void(entity g) VR_AG_KnockedDown =
{
    makevectors(g.angles);
    VR_AG_Disarm(g, v_forward + v_right * crandom(), 60, "knocked down");
};

// Your hand grabbing its weapon takes it: the weapon is yours, in that hand (the grab started now).
void() VR_AG_PropHandTouch =
{
    entity pl = self.handtouch_ent;
    if(!cvar_hget(cvarh_vr_agrunt_disarm_yank) || !(pl.flags & FL_CLIENT) || pl.health <= 0)
    {
        return;
    }
    float h = VRGetHandtouchHand(self);
    if(cvar_hget(cvarh_vr_agrunt_debug) >= 2)
    {
        VR_AG_Log(sprintf("a hand (%g) on its weapon: empty %g, grabbing %g", h, VRIsHandEmpty(pl, h),
            VR_HandGrabUtil_StartedHandGrabbing(pl, h)));
    }
    if(!VRIsHandEmpty(pl, h) || !VR_HandGrabUtil_StartedHandGrabbing(pl, h))
    {
        return;
    }
    entity g = self.owner;
    entity w = VR_AG_DropHeld(g, '0 0 0');
    if(!w)
    {
        return;
    }
    g.vr_ag_disarms = g.vr_ag_disarms + 1;
    g.vr_ag_reserve = 0;
    w.handtouch_ent = pl;
    w.handtouch_hand = h;
    wpnthrow_handtouch_ent(w);
    VR_AG_Log(sprintf("yanked out of its hand by %s (hand %g)", pl.netname, h));
};

// ----------------------------------------------------------------------------
// Firing and swinging: your weapons' own functions, `self` the grunt
// ----------------------------------------------------------------------------

// Where it aims: at its target's middle, a little behind it as it moves (the grunt's), up to vr_agrunt_aim_error off.
vector(vector from) VR_AG_AimDir =
{
    entity t = self.enemy;
    vector at = VR_AI_Origin(t) - t.velocity * 0.15;
    if(t.flags & FL_CLIENT)
    {
        at_z = at_z + 6; // (a player's chest, not his origin's waist)
    }
    vector dir = normalize(at - from);
    float err = cvar_hget(cvarh_vr_agrunt_aim_error);
    if(err > 0)
    {
        vector a = vectoangles(dir);
        a_x = -a_x + crandom() * err; // (a view's pitch for makevectors)
        a_y = a_y + crandom() * err;
        makevectors(a);
        dir = v_forward;
    }
    return dir;
};

// Its hand fields as the player's would be holding the weapon `p` aimed along `dir` (VRGetHandPos, VRGetMuzzlePos,
// VRGetWeaponFireRot, VRGetWeapon, VRGetEntWeaponInst read them).
void(entity p, vector dir) VR_AG_SetHand =
{
    self.vr_ag_aim = dir;
    VR_AG_PlaceProp(self, p);
    self.handpos = p.origin;
    vector a = vectoangles(dir);
    a_x = -a_x; // (a view's pitch, as the hands')
    self.handrot = a;
    self.shotrot = '0 0 0';
    self.handvel = '0 0 0';
    vector m = weaponmuzzle(p);
    if(m == p.origin)
    {
        m = p.origin + dir * 16;
    }
    self.muzzlepos = m;
    self.weapon = p.weapon;
    self.weaponflags = p.weaponflags;
    self.weaponinst = p.weaponinst;
    makevectors(self.handrot);
};

// ----------------------------------------------------------------------------
// Its launchers fired safely (vr_agrunt_blast_safe): the round's burst predicted, kept clear of itself
// ----------------------------------------------------------------------------

.float vr_ag_dodge_until; // stepping aside (a launcher's shot not clear) until then
.float vr_ag_held;        // the launcher shots it held back (the tests)
float vr_ag_self_kills;   // armed grunts killed by their own weapon (the tests)
float vr_ag_self_dmg;     // ... the damage they dealt themselves
float vr_ag_held_fire;    // ... the launcher shots held back
vector vr_ag_blast_at;    // VR_AG_BlastPoint's last

// Where a round of `wid` fired from `from` along `dir` (unit) bursts, as the launchers' code flies it: a rocket at what
// its line meets; a grenade along its arc (600 ahead, 200 up, gravity; bounced as MOVETYPE_BOUNCE, 1.5 off a surface,
// resting on a floor once slow) at its fuse or at the first thing it may hurt; a mine where it first lands (it sits there
// and blows at anything near).
vector(float wid, vector from, vector dir) VR_AG_BlastPoint =
{
    if(wid == WID_ROCKET_LAUNCHER || wid == WID_ADROCKET)
    {
        traceline(from, from + dir * 4096, FALSE, self);
        return trace_endpos;
    }
    vector a = vectoangles(dir);
    a_x = -a_x;
    makevectors(a);
    vector vel = v_forward * 600 + v_up * 200;
    vector pos = from;
    float g = cvar("sv_gravity");
    float mine = (wid == WID_PROXIMITY_GUN);
    float dt = 0.05;
    float t;
    for(t = 0; t < 2.5; t = t + dt)
    {
        vector next = pos + vel * dt;
        vel_z = vel_z - g * dt;
        traceline(pos, next, FALSE, self);
        if(trace_fraction == 1)
        {
            pos = next;
            continue;
        }
        pos = trace_endpos + trace_plane_normal * 0.5;
        if(mine || (trace_ent != world && trace_ent.takedamage == DAMAGE_AIM))
        {
            return trace_endpos; // (a mine lands; a grenade bursts on what it may hurt)
        }
        vel = vel - trace_plane_normal * ((vel * trace_plane_normal) * 1.5);
        if(trace_plane_normal_z > 0.7 && vel_z < 60)
        {
            return pos; // (at rest on the floor till its fuse)
        }
    }
    return pos;
};

// The least distance from itself its round of `wid` may burst at (its blast's reach and a margin: the grenades' and the
// mines' a little more, their path less sure).
float(float wid) VR_AG_SafeDist =
{
    if(wid == WID_PROXIMITY_GUN) { return 230; }
    if(wid == WID_ROCKET_LAUNCHER || wid == WID_ADROCKET) { return 180; }
    return 200;
};

// Whether its launcher's round fired along `dir` from the held weapon `p` would burst far enough from it (TRUE for any
// other weapon, or vr_agrunt_blast_safe 0).
float(entity p, vector dir) VR_AG_BlastSafe =
{
    float wid = p.weapon;
    if(VR_AG_Class(wid) != VR_AG_CLASS_EXPLOSIVE || !cvar_hget(cvarh_vr_agrunt_blast_safe))
    {
        return TRUE;
    }
    vector from = weaponmuzzle(p);
    vr_ag_blast_at = VR_AG_BlastPoint(wid, from, dir);
    return vlen(vr_ag_blast_at - self.origin) >= VR_AG_SafeDist(wid);
};

// Its own (or any) proximity mine near it (within 200 units, in sight), to keep away from (world: none).
entity() VR_AG_NearMine =
{
    entity e = findradius(self.origin, 200);
    while(e)
    {
        if(e.classname == "proximity_grenade")
        {
            traceline(self.origin, e.origin, TRUE, self);
            if(trace_fraction == 1)
            {
                return e;
            }
        }
        e = e.chain;
    }
    return world;
};

// One shot (or round, or nail, or bolt) of the gun it holds, from its clip.
void() VR_AG_Shoot =
{
    entity p = self.vr_ag_prop;
    if(!p || !self.enemy)
    {
        return;
    }
    entity inst = p.weaponinst;
    if(inst.wi_clip < 1)
    {
        return;
    }
    float wid = p.weapon;
    vector dir = VR_AG_AimDir(p.origin);
    VR_AG_SetHand(p, dir);
    if(!VR_AG_BlastSafe(p, dir))
    {
        // Its own blast would reach it (the target came near, a wall ahead): held back, a step aside.
        self.vr_ag_held = self.vr_ag_held + 1;
        vr_ag_held_fire = vr_ag_held_fire + 1;
        self.vr_ag_dodge_until = time + 0.6 + random() * 0.6;
        self.lefty = !self.lefty;
        self.weaponinst = world;
        VR_AG_Log(sprintf("%s held back: it would burst %.0f from it", p.netname, vlen(vr_ag_blast_at - self.origin)));
        return;
    }
    makevectors(self.handrot); // (VR_AG_BlastSafe's arc used the globals)
    float aid = WeaponIdToAmmoId(wid, p.weaponflags);
    float single = VR_AG_SingleUse(wid) || aid == AID_NONE;
    if(!single)
    {
        VR_AmmoUtil_SetAmmoCountByAmmoId(self, aid, inst.wi_clip);
    }

    float h = cVR_MainHand;
    float rounds = 1;
    self.button0 = 1; // (its trigger held: the laser cannon fires only so)
    if(wid == WID_SHOTGUN)              { W_FireShotgun(h); }
    else if(wid == WID_SUPER_SHOTGUN && AD_Widowmaker(inst)) { AD_FireWidowmaker(h); } // (AD's upgrades: vr_ad_bridge.qc)
    else if(wid == WID_LIGHTNING && AD_PlasmaGun(h, p.weaponflags)) { AD_FirePlasma(h); }
    else if(wid == WID_ADROCKET)        { VR_ADGun_FireRocket(h); } // (AD's soldiers' guns: vr_ad_enemyguns.qc)
    else if(wid == WID_ADGRENADE)       { VR_ADGun_FireGrenade(h); }
    else if(wid == WID_ADDEFENDER)      { VR_ADGun_FireDefender(h); }
    else if(wid == WID_ADCROSSBOW)      { VR_ADGun_FireCrossbow(h); }
    else if(wid == WID_ADPLASMA || wid == WID_ADCENTURION) { VR_ADGun_FirePlasma(h, wid); }
    else if(VR_AG_Stream(wid))
    {
        // Two flames a fire frame (the player's stream is one a tick), from its fuel.
        rounds = min(2, inst.wi_clip);
        VR_ADGun_FireFlame(h, wid);
        if(rounds > 1)
        {
            VR_ADGun_FireFlame(h, wid);
        }
    }
    else if(wid == WID_SUPER_SHOTGUN)   { W_FireSuperShotgun(h); }
    else if(wid == WID_NAILGUN)         { W_FireSpikes(h, (self.vr_ag_shots & 1) ? -2.35 : 2.35); }
    else if(wid == WID_SUPER_NAILGUN)   { W_FireSuperSpikes(h); }
    else if(wid == WID_GRENADE_LAUNCHER)
    {
        if(p.weaponflags & QVR_WPNFLAG_USE_SECONDARY_AMMO) { W_FireMultiGrenade(h); } else { W_FireGrenade(h); }
    }
    else if(wid == WID_ROCKET_LAUNCHER)
    {
        if(p.weaponflags & QVR_WPNFLAG_USE_SECONDARY_AMMO) { W_FireMultiRocket(h); } else { W_FireRocket(h); }
    }
    else if(wid == WID_PROXIMITY_GUN)   { W_FireProximityGrenade(h); }
    else if(wid == WID_LASER_CANNON)    { HIP_FireLaser(h, self.vr_ag_shots & 1); }
    else if(wid == WID_LIGHTNING)
    {
        if(self.waterlevel > 1)
        {
            self.button0 = 0;
            self.weaponinst = world;
            return; // (it would discharge: never under water)
        }
        W_FireLightning(h);
        // The beam: the client draws yours from your gun; a monster's is Quake's temporary beam.
        traceline(self.muzzlepos, self.muzzlepos + dir * 600, TRUE, self);
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_LIGHTNING2);
        WriteEntity(MSG_BROADCAST, p);
        WriteByte(MSG_BROADCAST, 0); // (the disambiguator: no hand's)
        WriteVec3(MSG_BROADCAST, self.muzzlepos);
        WriteVec3(MSG_BROADCAST, trace_endpos);
    }
    else if(wid == WID_GRUNTGUN)
    {
        if(cvar_hget(cvarh_vr_grunt_burst)) { VR_FireBurstRound(h); } else { VR_FireGruntGun(h); }
    }
    else if(wid == WID_ENFRIFLE)        { VR_FireEnforcerRifle(h); }
    self.button0 = 0;
    self.weaponinst = world; // (the record is its weapon's: nothing else may take the grunt for its holder)

    if(VR_AG_Class(wid) != VR_AG_CLASS_MELEE && !VR_AG_Stream(wid))
    {
        weaponfired(p, -1); // its muzzle flash on the held weapon, as yours (the engine's vr_weaponfx.cpp)
    }
    if(single)
    {
        inst.wi_clip = max(0, inst.wi_clip - rounds);
    }
    else
    {
        inst.wi_clip = max(0, VR_AmmoUtil_GetAmmoCountByAmmoId(self, aid));
        VR_AmmoUtil_SetAmmoCountByAmmoId(self, aid, 0);
    }
    self.vr_ag_shots = self.vr_ag_shots + 1;
    self.vr_ag_total_shots = self.vr_ag_total_shots + 1;
    vr_ag_shots_fired = vr_ag_shots_fired + 1;
    self.vr_ag_kick_at = time;
    self.effects = self.effects | EF_MUZZLEFLASH;
    self.show_hostile = time + 1; // wakes monsters up, as your shots do
    VR_AG_Log(sprintf("%s fired at %s (%g left in the clip), muzzle %v", p.netname, self.enemy.classname, inst.wi_clip,
        self.muzzlepos));
};

// A melee blow (its weapon's, or a punch) from its hand towards its target.
void() VR_AG_Strike =
{
    float h = cVR_MainHand;
    entity p = self.vr_ag_prop;
    vector from = p ? p.origin : self.origin + '0 0 16';
    if(!p)
    {
        // A punch from its right hand.
        from = (modelvertex(self, VR_AG_HAND_V1) + modelvertex(self, VR_AG_HAND_V2)) * 0.5;
        if(trace_fraction == 0) { from = self.origin + '0 0 16'; }
    }
    vector dir = normalize(VR_AI_Origin(self.enemy) + '0 0 8' - from);
    if(p)
    {
        VR_AG_SetHand(p, dir);
    }
    else
    {
        self.handpos = from;
        vector a = vectoangles(dir);
        a_x = -a_x;
        self.handrot = a;
        self.shotrot = '0 0 0';
        self.handvel = '0 0 0';
        self.muzzlepos = from + dir * 4;
        self.weapon = WID_FIST;
        self.weaponflags = 0;
        self.weaponinst = world;
        makevectors(a);
    }
    VRSetMeleeHitSoundPlayed(h, FALSE);
    float wid = p ? p.weapon : WID_FIST;
    float cells = self.ammo_cells;
    self.ammo_cells = 0; // (Mjolnir as the hammer: no lightning)
    if(wid == WID_CHAINSAW)            { W_ChainsawMelee(h); }
    else if(wid == WID_MJOLNIR)        { HIP_FireMjolnir(h); }
    else if(wid == WID_FIST)           { W_FistMelee(h); }
    else if(VR_AG_Class(wid) == VR_AG_CLASS_MELEE) { W_FireAxe(h); }
    else                               { W_GunMelee(h); } // (a gun's butt: an explosive one at arm's length)
    self.ammo_cells = cells;
    self.weaponinst = world;
    self.vr_ag_aim = '0 0 0';
    self.vr_ag_strikes = self.vr_ag_strikes + 1;
    VR_AG_Log(sprintf("struck at %s with %s", self.enemy.classname, p ? p.netname : "its fists"));
};

// ----------------------------------------------------------------------------
// Animations: its own run, attacks, reload and pickup (the grunt's frames); stand, walk, pain, death are the grunt's
// ----------------------------------------------------------------------------

void(float d) VR_AG_Run;
void() vr_ag_reload1;
void() vr_ag_take1;

void() vr_ag_run1 = [73, vr_ag_run2] { VR_AG_Run(11); };
void() vr_ag_run2 = [74, vr_ag_run3] { VR_AG_Run(15); };
void() vr_ag_run3 = [75, vr_ag_run4] { VR_AG_Run(10); };
void() vr_ag_run4 = [76, vr_ag_run5] { VR_AG_Run(10); };
void() vr_ag_run5 = [77, vr_ag_run6] { VR_AG_Run(8); };
void() vr_ag_run6 = [78, vr_ag_run7] { VR_AG_Run(15); };
void() vr_ag_run7 = [79, vr_ag_run8] { VR_AG_Run(10); };
void() vr_ag_run8 = [80, vr_ag_run1] { VR_AG_Run(8); };

// After an attack: a reload if its clip is empty, else on running.
void() VR_AG_AfterAttack =
{
    self.vr_ag_aim = '0 0 0';
    entity p = self.vr_ag_prop;
    if(self.vr_adgun_stream)
    {
        // A flamethrower's stream ends with its sound (the player's: VR_ADGun_Frame).
        self.vr_adgun_stream = 0;
        sound(self, CHAN_WEAPON, self.vr_ag_wid == WID_ADFUMIGATOR ? "enforcer/fum_poisonoff.wav" : "enforcer/pyro_flameoff.wav",
            1, ATTN_NORM);
    }
    if(p && p.weaponinst.wi_clip < 1 && VR_AG_ClipOf(p.weapon, p.weaponinst) > 0)
    {
        if(self.vr_ag_reserve > 0)
        {
            vr_ag_reload1();
            return;
        }
        // Spent, nothing to reload: thrown away; it fights on with its fists.
        makevectors(self.angles);
        VR_AG_DropHeld(self, v_right * 90 + v_forward * 40 + '0 0 120');
        VR_AG_Log("its weapon spent: thrown away");
    }
    vr_ag_run1();
};

// A fire frame `n` (1..9: shoot1..9) of its attack.
void(float n) VR_AG_AtkFrame =
{
    ai_face();
    entity p = self.vr_ag_prop;
    if(!p || !self.enemy)
    {
        return;
    }
    float c = VR_AG_Class(p.weapon);
    if(n >= 3)
    {
        self.vr_ag_aim = VR_AG_AimDir(p.origin); // (pointing at its target from the frame it raises the gun)
    }
    if(c == VR_AG_CLASS_AUTO && n >= 4 && n <= 8)
    {
        VR_AG_Shoot();
    }
    else if(c == VR_AG_CLASS_BURST && n >= 5 && n <= 7)
    {
        VR_AG_Shoot();
    }
    else if(n == 5 && (c == VR_AG_CLASS_GUN || c == VR_AG_CLASS_EXPLOSIVE))
    {
        VR_AG_Shoot();
    }
};

void() vr_ag_atk1 = [81, vr_ag_atk2] { self.vr_ag_shots = 0; VR_AG_AtkFrame(1); };
void() vr_ag_atk2 = [82, vr_ag_atk3] { VR_AG_AtkFrame(2); };
void() vr_ag_atk3 = [83, vr_ag_atk4] { VR_AG_AtkFrame(3); };
void() vr_ag_atk4 = [84, vr_ag_atk5] { VR_AG_AtkFrame(4); };
void() vr_ag_atk5 = [85, vr_ag_atk6] { VR_AG_AtkFrame(5); };
void() vr_ag_atk6 = [86, vr_ag_atk7] { VR_AG_AtkFrame(6); };
void() vr_ag_atk7 = [87, vr_ag_atk8]
{
    VR_AG_AtkFrame(7);
    if(self.vr_ag_prop.weaponinst.wi_clip > 0 && VR_AG_Class(self.vr_ag_wid) == VR_AG_CLASS_AUTO)
    {
        SUB_CheckRefire(vr_ag_atk4); // (a nail gun's or the thunderbolt's stream goes on while it sees its target)
    }
};
void() vr_ag_atk8 = [88, vr_ag_atk9] { VR_AG_AtkFrame(8); };
void() vr_ag_atk9 = [89, vr_ag_atk9] { VR_AG_AtkFrame(9); VR_AG_AfterAttack(); };

// A melee blow (the shove's frames: a step in, the arm out); its swing drawn from the second.
void() vr_ag_swing1 = [114, vr_ag_swing2] { ai_face(); ai_charge(4); };
void() vr_ag_swing2 = [115, vr_ag_swing3] { ai_face(); ai_charge(4); self.vr_ag_swing_at = time;
    VR_SoundVaried(self, CHAN_WEAPON, self.vr_ag_prop ? "weapons/ax1.wav" : "knight/sword1.wav", 1, ATTN_NORM); };
void() vr_ag_swing3 = [116, vr_ag_swing4] { ai_face(); ai_charge(2); };
void() vr_ag_swing4 = [117, vr_ag_swing5] { ai_face(); VR_AG_Strike(); };
void() vr_ag_swing5 = [118, vr_ag_swing6] {};
void() vr_ag_swing6 = [119, vr_ag_swing6] { VR_AG_AfterAttack(); };

// The reload: the grunt's load frames (unused by id's), over vr_agrunt_reload_time; a clip out of its reserve at the end.
void() VR_AG_ReloadFrame =
{
    self.nextthink = time + max(0.02, cvar_hget(cvarh_vr_agrunt_reload_time)) / 11;
};
void() vr_ag_reload1 = [29, vr_ag_reload2] { VR_AG_ReloadFrame(); VR_AG_Log("reloading"); };
void() vr_ag_reload2 = [30, vr_ag_reload3] { VR_AG_ReloadFrame(); };
void() vr_ag_reload3 = [31, vr_ag_reload4] { VR_AG_ReloadFrame(); };
void() vr_ag_reload4 = [32, vr_ag_reload5] { VR_AG_ReloadFrame(); };
void() vr_ag_reload5 = [33, vr_ag_reload6] { VR_AG_ReloadFrame(); sound(self, CHAN_WEAPON, "weapons/lock4.wav", 1, ATTN_NORM); };
void() vr_ag_reload6 = [34, vr_ag_reload7] { VR_AG_ReloadFrame(); };
void() vr_ag_reload7 = [35, vr_ag_reload8] { VR_AG_ReloadFrame(); };
void() vr_ag_reload8 = [36, vr_ag_reload9] { VR_AG_ReloadFrame(); };
void() vr_ag_reload9 = [37, vr_ag_reload10] { VR_AG_ReloadFrame(); };
void() vr_ag_reload10 = [38, vr_ag_reload11] { VR_AG_ReloadFrame(); };
void() vr_ag_reload11 = [39, vr_ag_run1]
{
    VR_AG_ReloadFrame();
    entity p = self.vr_ag_prop;
    if(p)
    {
        float n = min(VR_AG_ClipOf(p.weapon, p.weaponinst) - p.weaponinst.wi_clip, self.vr_ag_reserve);
        p.weaponinst.wi_clip = p.weaponinst.wi_clip + n;
        self.vr_ag_reserve = self.vr_ag_reserve - n;
        self.vr_ag_reloads = self.vr_ag_reloads + 1;
        VR_AG_Log(sprintf("reloaded: %g in the clip, %g in reserve", p.weaponinst.wi_clip, self.vr_ag_reserve));
    }
};

// Taking up a weapon lying at its feet: bent down (the load frames' first half), up with it.
void() VR_AG_TakeIt =
{
    entity w = self.vr_ag_seek;
    self.vr_ag_seek = world;
    if(!w || w.classname != "thrown_weapon" || vlen(w.origin - self.origin) > VR_AG_PICKUP_REACH * 1.5 || self.vr_ag_prop)
    {
        return;
    }
    entity inst = w.weaponinst;
    float wid = w.weapon;
    float flags = w.weaponflags;
    w.weaponinst = world;
    remove(w);
    VR_AG_Arm(self, wid, flags, inst);
    self.vr_ag_reserve = 0;
    self.vr_ag_pickups = self.vr_ag_pickups + 1;
    sound(self, CHAN_ITEM, "weapons/pkup.wav", 1, ATTN_NORM);
    VR_AG_Log(sprintf("picked up %s (%g in its clip)", self.vr_ag_prop.netname, inst.wi_clip));
};
void() vr_ag_take1 = [29, vr_ag_take2] { self.nextthink = time + 0.06; };
void() vr_ag_take2 = [30, vr_ag_take3] { self.nextthink = time + 0.06; };
void() vr_ag_take3 = [31, vr_ag_take4] { self.nextthink = time + 0.06; };
void() vr_ag_take4 = [32, vr_ag_take5] { self.nextthink = time + 0.06; VR_AG_TakeIt(); };
void() vr_ag_take5 = [31, vr_ag_take6] { self.nextthink = time + 0.06; };
void() vr_ag_take6 = [30, vr_ag_run1] { self.nextthink = time + 0.06; };

// ----------------------------------------------------------------------------
// Its AI
// ----------------------------------------------------------------------------

// A weapon lying about it would go for: usable, not spent, not in a hand or flying, within vr_agrunt_rearm_range and in
// sight, and its target (if any) not nearer to it than it is. world: none.
entity() VR_AG_FindWeapon =
{
    float range = cvar_hget(cvarh_vr_agrunt_rearm_range);
    entity best = world;
    float bestDist = range + 1;
    entity e = findradius(self.origin, range);
    while(e)
    {
        if(e.classname == "thrown_weapon" && VR_AG_Usable(e.weapon) && !e.vr_depleted && !VR_EnemyWeapons_Spared(e) &&
           (VR_AG_Class(e.weapon) == VR_AG_CLASS_MELEE || e.weaponinst.wi_clip > 0))
        {
            float d = vlen(e.origin - self.origin);
            float safe = TRUE;
            if(self.enemy && self.enemy.health > 0 && d > 64)
            {
                safe = vlen(e.origin - self.enemy.origin) > d; // (one at its feet it takes up whatever)
            }
            if(safe && d < bestDist && fabs(e.origin_z - self.origin_z) < 64)
            {
                traceline(self.origin + '0 0 16', e.origin, TRUE, self);
                if(trace_fraction == 1 || trace_ent == e)
                {
                    best = e;
                    bestDist = d;
                }
            }
        }
        e = e.chain;
    }
    return best;
};

// Disarmed: whether to go for a weapon (it looks now and then).
float() VR_AG_Seeking =
{
    if(self.vr_ag_prop || !cvar_hget(cvarh_vr_agrunt_rearm))
    {
        self.vr_ag_seek = world;
        return FALSE;
    }
    entity w = self.vr_ag_seek;
    if(w && (w.classname != "thrown_weapon" || VR_EnemyWeapons_Spared(w)))
    {
        self.vr_ag_seek = world; // (taken by someone else, or flying)
    }
    if(!self.vr_ag_seek && time >= self.vr_ag_seek_next)
    {
        self.vr_ag_seek_next = time + 0.5;
        self.vr_ag_seek = VR_AG_FindWeapon();
        if(self.vr_ag_seek)
        {
            VR_AG_Log(sprintf("going for %s, %.0f away", self.vr_ag_seek.netname, vlen(self.vr_ag_seek.origin - self.origin)));
        }
    }
    return self.vr_ag_seek != world;
};

// Its run frames: to a weapon it goes for, away from a target too near its explosive weapon, else the grunt's run.
void(float d) VR_AG_Run =
{
    self.vr_ag_myrun = time;
    if(VR_AG_Seeking())
    {
        entity w = self.vr_ag_seek;
        if(vlen(w.origin - self.origin) <= VR_AG_PICKUP_REACH)
        {
            vr_ag_take1();
            return;
        }
        self.ideal_yaw = vectoyaw(w.origin - self.origin);
        ChangeYaw();
        if(!walkmove(self.ideal_yaw, d))
        {
            entity oldGoal = self.goalentity;
            self.goalentity = w;
            movetogoal(d);
            self.goalentity = oldGoal;
        }
        return;
    }
    if(!self.enemy)
    {
        // Out of a fight (it went for a weapon: VR_AG_Idle): looking about as it stands.
        if(!FindTarget())
        {
            self.th_stand();
        }
        return;
    }
    float safe = cvar_hget(cvarh_vr_agrunt_blast_safe);
    entity mine = safe ? VR_AG_NearMine() : world;
    if(mine)
    {
        // Away from a mine (its own blows at anything near).
        self.ideal_yaw = vectoyaw(self.origin - mine.origin);
        ChangeYaw();
        if(!walkmove(self.ideal_yaw, d))
        {
            walkmove(self.ideal_yaw + (self.lefty ? 60 : -60), d);
        }
        return;
    }
    float explosive = self.vr_ag_prop && VR_AG_MinRange(self.vr_ag_wid) > 0;
    if(explosive && safe && time < self.vr_ag_dodge_until)
    {
        // A step aside (its last shot wasn't clear), facing its target.
        ai_face();
        if(!walkmove(self.angles_y + (self.lefty ? 90 : -90), d))
        {
            self.lefty = !self.lefty;
            walkmove(self.angles_y + 180, d * 0.5);
        }
        return;
    }
    if(explosive && visible(self.enemy) && vlen(self.enemy.origin - self.origin) < VR_AG_MinRange(self.vr_ag_wid))
    {
        // Backing off: its own blast would reach it (cornered: along the wall, either way).
        ai_face();
        if(!walkmove(self.angles_y + 180 + (self.lefty ? 30 : -30), d) && safe &&
           !walkmove(self.angles_y + 180 + (self.lefty ? -30 : 30), d))
        {
            self.lefty = !self.lefty;
            walkmove(self.angles_y + (self.lefty ? 90 : -90), d);
        }
        if(vlen(self.enemy.origin - self.origin) < 60 && time >= self.attack_finished)
        {
            self.th_melee();
            SUB_AttackFinished(1);
        }
        return;
    }
    ai_run(d);
};

// ai_stand, ai_walk (vr_agrunt_rearm_idle): unarmed and out of a fight, off to a weapon lying near (its run frames:
// VR_AG_Run walks it there and takes it up; then back to standing).
float() VR_AG_Idle =
{
    if(self.vr_ag_prop || self.health <= 0 || !cvar_hget(cvarh_vr_agrunt_rearm_idle) || !VR_AG_Seeking())
    {
        return FALSE;
    }
    vr_ag_run1();
    return TRUE;
};

// CheckAnyAttack (ai.qc): an armed grunt's attack by its weapon; its own run frames for going for a weapon or backing
// off (the grunt's pains end in the grunt's run).
float() VR_AG_CheckAttack =
{
    entity targ = self.enemy;
    float c = VR_AG_Class(self.vr_ag_wid);
    float d = vlen(VR_AI_Origin(targ) - self.origin);
    float mine = time - self.vr_ag_myrun < 0.15;
    float explosive = VR_AG_MinRange(self.vr_ag_wid) > 0;
    float blastSafe = cvar_hget(cvarh_vr_agrunt_blast_safe);
    if(!mine && (VR_AG_Seeking() || (explosive && d < VR_AG_MinRange(self.vr_ag_wid)) ||
                 (explosive && blastSafe && (time < self.vr_ag_dodge_until || VR_AG_NearMine()))))
    {
        vr_ag_run1();
        return TRUE;
    }
    if(explosive && blastSafe && (time < self.vr_ag_dodge_until || VR_AG_NearMine()))
    {
        return FALSE; // (stepping aside, or away from a mine: VR_AG_Run)
    }
    if(VR_AG_Stream(self.vr_ag_wid) && d > 0.8 * VR_ADGun_Value(self.vr_ag_wid, "range"))
    {
        return FALSE; // (a flamethrower's stream reaches so far)
    }
    if(self.vr_ag_seek)
    {
        return FALSE; // (on its way to a weapon)
    }
    if(time < self.attack_finished)
    {
        return FALSE;
    }
    if(c == VR_AG_CLASS_FISTS || c == VR_AG_CLASS_MELEE)
    {
        if(d < VR_AG_GRIP_REACH + (c == VR_AG_CLASS_MELEE ? 10 : 0))
        {
            self.th_melee();
            SUB_AttackFinished(0.6 + random() * 0.4);
            return TRUE;
        }
        return FALSE;
    }

    // The grunt's: a clear shot.
    traceline(self.origin + self.view_ofs, VR_AI_Origin(targ) + targ.view_ofs, self.vr_ai_gate > 0 ? MOVE_PORTALS : FALSE, self);
    if(trace_inopen && trace_inwater)
    {
        return FALSE;
    }
    if(trace_ent != targ)
    {
        return FALSE;
    }
    if(VR_AG_MinRange(self.vr_ag_wid) > 0 && d < VR_AG_MinRange(self.vr_ag_wid))
    {
        return FALSE; // (backing off: VR_AG_Run)
    }
    if(explosive && blastSafe && self.vr_ag_prop &&
       !VR_AG_BlastSafe(self.vr_ag_prop, normalize(VR_AI_Origin(targ) - weaponmuzzle(self.vr_ag_prop))))
    {
        // Its round wouldn't burst clear of it from here (a wall, a ledge ahead): a step aside first.
        self.vr_ag_dodge_until = time + 0.5 + random() * 0.5;
        self.lefty = !self.lefty;
        return FALSE;
    }
    if(random() < VR_AG_Chance(self.vr_ag_wid, enemy_range))
    {
        self.th_missile();
        SUB_AttackFinished(1 + random());
        if(random() < 0.3)
        {
            self.lefty = !self.lefty;
        }
        return TRUE;
    }
    return FALSE;
};

void() VR_AG_Missile = { vr_ag_atk1(); };
void() VR_AG_Melee = { vr_ag_swing1(); };

// T_DamageDeal: its weapon's damage to a player.
float(entity targ, entity attacker, float damage) VR_AG_DamageScale =
{
    if(attacker == targ && targ.vr_ag && targ.health > 0)
    {
        vr_ag_self_dmg = vr_ag_self_dmg + damage; // (the tests: its own blast)
    }
    if(attacker.vr_ag && (targ.flags & FL_CLIENT) && attacker != targ)
    {
        damage = damage * max(0, cvar_hget(cvarh_vr_agrunt_damage_scale));
        vr_ag_player_dmg = vr_ag_player_dmg + damage;
        return damage;
    }
    return damage;
};

// T_DamageDeal: a blow on it, near its gun hand and hard enough, knocks its weapon out of it.
void(entity targ, entity inflictor, entity attacker, float take) VR_AG_Hit =
{
    if(!targ.vr_ag || !targ.vr_ag_prop || targ.health <= 0 || !cvar_hget(cvarh_vr_agrunt_disarm) ||
       take < cvar_hget(cvarh_vr_agrunt_disarm_damage) || inflictor == world || inflictor.classname == "vr_burn")
    {
        return;
    }
    VR_HitReact_Where(targ, inflictor, attacker);
    if(vr_hr_at == (targ.absmin + targ.absmax) * 0.5 || vr_hitkind == QVR_HITKIND_SPLASH)
    {
        return; // (where it struck isn't known, or a blast's: its box's nearest point)
    }
    float near = vlen(vr_hr_at - targ.vr_ag_prop.origin);
    if(cvar_hget(cvarh_vr_agrunt_debug))
    {
        VR_AG_Log(sprintf("hit %g at %.1f from its gun hand (part %g)", take, near, vr_hr_part));
    }
    if(near <= VR_AG_DISARM_HAND || (vr_hr_part == 2 && near <= VR_AG_DISARM_HAND * 1.6))
    {
        VR_AG_Disarm(targ, vr_hr_dir, 150, sprintf("a blow of %g on its gun hand", take));
    }
};

// SoldierDrop: an armed grunt's backpack holds its reserve (none: a grunt's 5 shells).
float() VR_AG_SoldierDrop =
{
    if(!self.vr_ag)
    {
        return FALSE;
    }
    self.ammo_shells = 0;
    self.ammo_nails = 0;
    self.ammo_rockets = 0;
    self.ammo_cells = 0;
    float aid = self.vr_ag_lastwid >= 0 ? WeaponIdToAmmoId(self.vr_ag_lastwid, 0) : AID_NONE;
    if(aid != AID_NONE && self.vr_ag_reserve > 0)
    {
        VR_AmmoUtil_SetAmmoCountByAmmoId(self, aid, self.vr_ag_reserve);
    }
    else
    {
        self.ammo_shells = 5;
    }
    self.vr_ag_reserve = 0;
    DropBackpack();
    return TRUE;
};

void() VR_AG_Die =
{
    if(damage_attacker == self)
    {
        vr_ag_self_kills = vr_ag_self_kills + 1; // (the tests)
        VR_AG_Log(sprintf("killed by its own %s", self.vr_ag_prop ? self.vr_ag_prop.netname : "weapon"));
    }
    // Its weapon falls from its hand, with what is left in it (gibbed too).
    makevectors(self.angles);
    VR_AG_DropHeld(self, v_forward * 25 - v_right * 20 + v_right * (crandom() * 15) + '0 0 70');

    if(self.health < -35)
    {
        sound(self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
        ThrowHead("progs/h_guard.mdl", self.health);
        ThrowGib("progs/gib1.mdl", self.health);
        ThrowGib("progs/gib2.mdl", self.health);
        ThrowGib("progs/gib3.mdl", self.health);
        SoldierDrop();
        return;
    }
    sound(self, CHAN_VOICE, "soldier/death1.wav", 1, ATTN_NORM);
    if(random() < 0.5)
        army_die1();
    else
        army_cdie1();
};

/*QUAKED monster_vr_grunt (1 0 0) (-16 -16 -24) (16 16 40) Ambush
An armed grunt (experimental): a grunt holding one of your weapons. "weapon": its weapon (WID_*: 2 axe, 4 shotgun,
5 super shotgun, 6 nailgun, 7 super nailgun, 8 grenade launcher, 10 rocket launcher, 11 thunderbolt, 13 sword,
14 chainsaw, 15 grunt's rifle, 16 laser rifle, 17 crowbar; with Arcane Dimensions' data 20 AD rocket launcher, 21 AD
grenade launcher, 22 plasma rifle, 23 defender's shotgun, 25 centurion's plasma gun, 26 flamethrower, 27 poison sprayer,
28 crossbow, 101 Shadow Axe, 102 Widowmaker, 103 Plasma Gun, 111..116 AD's swords, hammer and mace; -1 its fists; 0 or
unset: one from the pool). "weaponflags": that weapon's flags.
*/
void() monster_vr_grunt =
{
    if(MG_MonsterInhibited()) return;
    if(deathmatch)
    {
        remove(self);
        return;
    }
    float want = self.weapon;
    float flags = self.weaponflags;
    self.weapon = 0;
    self.weaponflags = 0;
    if(want == 0)
    {
        want = VR_AG_PickWeapon();
        flags = 0;
    }
    entity inst = world;
    self.vr_ag_code = want;
    if(want > 0)
    {
        if(!VR_AG_Decode(want))
        {
            dprint(sprintf("monster_vr_grunt: weapon %g can't be held here: its fists\n", want));
            want = -1;
        }
        else
        {
            // (AD's upgrades: on its record; its melee weapons: the sword's kind in its flags)
            want = vr_ag_dec_wid;
            flags = flags | vr_ag_dec_flags;
            if(vr_ag_dec_up)
            {
                inst = WeaponInst_New(0);
                inst.wi_adup = vr_ag_dec_up;
                inst.wi_clip = VR_AG_ClipOf(want, inst);
            }
        }
    }
    if(!VR_OtherFlags())
    {
        self.spawnflags = self.spawnflags - (self.spawnflags & (SPAWNFLAG_TRIGGER_SPAWN | SPAWNFLAG_TRIGGER_SPAWN_MULTIPLE));
    }

    self.classname = "monster_army";
    monster_army();
    precache_model("progs/vrgrunt.mdl");
    precache_sound("weapons/lock4.wav");
    precache_sound("weapons/pkup.wav");
    precache_sound("weapons/ax1.wav");
    precache_sound("weapons/tink1.wav");
    setmodel(self, "progs/vrgrunt.mdl");
    setsize(self, '-12 -12 -24', '12 12 27');
    self.vr_ag = 1;
    self.th_run = vr_ag_run1;
    self.th_missile = VR_AG_Missile;
    self.th_melee = VR_AG_Melee;
    self.th_die = VR_AG_Die;
    VR_AG_Arm(self, want, flags, inst);
    self.vr_ag_reserve = VR_AG_SingleUse(self.vr_ag_wid) ? 0 :
                         VR_AG_ClipOf(self.vr_ag_wid, self.vr_ag_prop.weaponinst) * max(0, rint(cvar_hget(cvarh_vr_agrunt_reserve)));
    VR_AG_Log(sprintf("spawned with %s (%g in reserve)", self.vr_ag_prop ? self.vr_ag_prop.netname : "its fists",
        self.vr_ag_reserve));
};
