// vr_ad_enemyguns.qc -- AD support: Arcane Dimensions' armed soldiers drop their own guns, and the player fires them
// (WID_ADROCKET..WID_ADFUMIGATOR; their tables: vr_ad_guns_defs.qc; after AD's files: AD's names, models and sounds).
//
// Who drops what (VR_ADGun_Drop, from VR_AD_Dying): the rocket soldier his launcher (Quake VR's rocket), the grenade
// soldier his (Quake VR's grenade: the launcher's model, size, bounce, fuse, blast; catchable as yours), the plasma
// soldier, the eliminator and the centurion their plasma guns (AD's plasma bolts, as the Plasma Gun's: a hit and a
// small blast; our impact sparks and prop push), the defender his shotgun (Quake VR's pellets: FireBullets' tracers,
// puffs, blood and decals; AD's projectile pellets where its rules apply), the pyro his
// flamethrower (a stream of flames: each flame's damage, and what it touches set on fire: Quake VR's burning) and the
// fumigator his poison sprayer (a stream of poison: each puff's damage, and what it touches poisoned for a while). AD's
// plain soldier and enforcer drop Quake VR's grunt's burst rifle and enforcer's laser rifle (their own kinds of gun).
//
// The player's use only: every number is a setting (vr_adgun_<gun>_<what>: VR Settings > Combat > Enemy Weapons >
// Arcane Dimensions' Guns), each gun's rounds and time between shots, the plasma guns' damage, blast and speed, the
// shotgun's pellets and spread, the flamethrowers' fuel, reach and what they leave burning or poisoned. The monsters'
// own attacks are AD's. vr_adgun_drops 0: the armed soldiers drop nothing.
//
// Single use, as the grunts' burst rifle (vr_enemyguns.qc): the clip is the gun's only ammo (the flamethrowers': their
// fuel, a unit a flame), never refilled; empty, it clicks, smokes and crackles (VR_EnemyGun_Spent) and is still a club.
// Damage is Quake VR's (T_Damage: AD's monsters take it through AD's damage, with their ammunition resistance: the
// rockets and grenades (ours, their own damage settings: Weapons' damage) as rockets, the flames' as rockets, the plasma's as cells, the pellets' as shells).
//
// `vr_ad_test 70` fires each gun from your main hand at a target ahead and reports what each did; `71` drops each from
// a soldier's place ahead and checks it; `72` prints the settings. developer 1: `adgun:` lines.

.float vr_adgun_dmg;     // a projectile's or flame's damage (direct)
.float vr_adgun_splash;  // ... its blast's (0: none)
.float vr_adgun_wid;     // ... the gun it came from
.float vr_adgun_burn;    // a flame's: seconds what it touches burns (0: none)
.float vr_adgun_poison;  // a poison puff's: damage a second of the poison it leaves
.float vr_adgun_ptime;   // ... and how long it lasts
.float vr_adgun_stream;  // a player's main hand's flamethrower stream: its last flame's time (0: not streaming)
.float vr_adgun_stream2; // ... the off hand's
.entity vr_adpoison;     // a poisoned thing's poison (a vr_adpoison: enemy the poisoned, owner who poisoned it)
float vr_adgun_hits;     // what the guns' shots dealt (the tests)

// ---------------------------------------------------------------------------------------------------------------------
// Drops

// The gun an AD soldier of class `t` drops (-1: none). Their own single-use guns (Vittorio, 2026-10-15: "their own
// single-use weapons", looking and feeling different, sharing our guns' projectiles where they have a kind of ours:
// VR_ADGun_Fire; a drop mapped to our weapons, 2026-10-14's vr_adgun_ours, was reverted).
float(float t) VR_ADGun_ForClass =
{
    float wid = -1;
    if(t == ad_CT_MONARMYROCKET)       { wid = WID_ADROCKET; }
    else if(t == ad_CT_MONARMYGRENADE) { wid = WID_ADGRENADE; }
    else if(t == ad_CT_MONARMYPLASMA)  { wid = WID_ADPLASMA; }
    else if(t == ad_CT_MONDEFENDER)    { wid = WID_ADDEFENDER; }
    else if(t == ad_CT_MONELIMATOR)    { wid = WID_ADELIMINATOR; }
    else if(t == ad_CT_MONCENTURION)   { wid = WID_ADCENTURION; }
    else if(t == ad_CT_MONPYRO)        { wid = WID_ADPYRO; }
    else if(t == ad_CT_MONFUMIGATOR)   { wid = WID_ADFUMIGATOR; }
    else if(t == ad_CT_MONARMY)        { wid = WID_GRUNTGUN; }  // (AD's plain soldier: the grunt's own kind of gun)
    else if(t == ad_CT_MONENFORCER)    { wid = WID_ENFRIFLE; }  // (AD's plain enforcer: the enforcer's laser rifle)
    return wid;
};

// A gun's model made and precached now (setmodel precaches one late: the gun model's making, from AD's monster model's
// file, a few milliseconds): as the soldier spawns, and before a gun is put in a hand (the hands' models are sent by
// their precache index).
void(float wid) VR_ADGun_Ensure =
{
    if(!VR_IsADGun(wid))
    {
        return;
    }
    entity e = spawn();
    setmodel(e, WeaponIdToModel(wid, 0));
    remove(e);
};

void(float xClass) VR_ADGun_Prepare =
{
    if(cvar("vr_adgun_drops"))
    {
        VR_ADGun_Ensure(VR_ADGun_ForClass(xClass));
    }
};

// AD's weapons in a map without AD's world setup (outside AD's rules: an id1 or a custom map; the toolgun's, a map's
// func_weapon_grabbable of one of these guns, an upgrade's pickup): the sounds their shots play, which AD's soldiers and
// world precache where they are (precache_sound may come late in Quake VR's progs; models setmodel precaches itself).
void() AD_WeaponsOnDemand =
{
    if(!cvar("vr_ad_mounted"))
    {
        return; // (Quake VR's stand-ins without AD's data: their own sounds)
    }
    precache_sound("soldier/rocket_fire.wav");
    precache_sound("soldier/rocket_hit.wav");
    precache_sound(ad_SOUND_PLASMA_FIRE);
    precache_sound(ad_SOUND_PLASMA_HIT);
    precache_sound(ad_SOUND_MISFIRE);
    precache_sound("enforcer/def_ssg_fire.wav");
    precache_sound("enforcer/pyro_flameon.wav");
    precache_sound("enforcer/pyro_flameoff.wav");
    precache_sound("enforcer/fum_poisonon.wav");
    precache_sound("enforcer/fum_poisonoff.wav");
};

// func_weapon_grabbable (a map's or the toolgun's) laying one of AD's soldiers' guns (`wid`): its model made and its
// sounds precached first (in any map: they need none of AD's rules).
void(float wid) VR_ADGun_Placed =
{
    if(VR_IsADGun(wid))
    {
        VR_ADGun_Ensure(wid);
        AD_WeaponsOnDemand();
    }
};

// DecodeLevelParms: p's carried guns (hands, holsters) brought from the last level: their models made and precached and
// their sounds precached in this one, which may have no such soldier (or vr_adgun_drops off): the hands' and holsters'
// models are sent by their precache index (SV_ModelIndex fails on one not precached).
void(entity p) VR_ADGun_EnsureCarried =
{
    VR_ADGun_Placed(p.weapon);
    VR_ADGun_Placed(p.weapon2);
    VR_ADGun_Placed(p.holsterweapon0);
    VR_ADGun_Placed(p.holsterweapon1);
    VR_ADGun_Placed(p.holsterweapon2);
    VR_ADGun_Placed(p.holsterweapon3);
    VR_ADGun_Placed(p.holsterweapon4);
    VR_ADGun_Placed(p.holsterweapon5);
};

// Self, an armed AD soldier dying: its own gun, where it held it, full (VR_DropEnemyGun).
void() VR_ADGun_Drop =
{
    if(!cvar("vr_adgun_drops"))
    {
        return;
    }
    float wid = VR_ADGun_ForClass(self.ad_classtype);
    if(wid < 0)
    {
        return;
    }
    VR_DropEnemyGun(wid);
    if(cvar("developer"))
    {
        dprint(sprintf("adgun: %s drops its %s (%g in it)\n", self.classname,
            VR_IsADGun(wid) ? VR_EnemyGun_Name(wid) : WeaponIdToWeaponName(wid), WeaponIdToBaseClipSize(wid)));
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Rockets and grenades

// The rocket soldier's launcher fires Quake VR's rocket (VR_FireRocketMissile: the rocket launcher's speed, model, hit,
// blast and muzzle clearance); its own sound, kick and rate.
void(float xHand) VR_ADGun_FireRocket =
{
    sound(self, VRGetGunChannel(xHand), "soldier/rocket_fire.wav", 1, ATTN_NORM);
    self.punchangle_x = -2;
    VR_FireRocketMissile(xHand);
    VRGunHaptic(xHand, 0.45, 75, 0.9);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 3);
};

// The grenade soldier's launcher fires Quake VR's grenade (VR_FireGrenadeMissile: the grenade launcher's model, size,
// bounce, fuse, blast, muzzle clearance and catchability; not AD's oversized monster grenade); its own kick and rate.
void(float xHand) VR_ADGun_FireGrenade =
{
    sound(self, VRGetGunChannel(xHand), "weapons/grenade.wav", 1, ATTN_NORM);
    self.punchangle_x = -2;
    VR_FireGrenadeMissile(xHand);
    VRGunHaptic(xHand, 0.4, 75, 0.9);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 2);
};

// ---------------------------------------------------------------------------------------------------------------------
// Plasma

// A plasma bolt (AD's, launched as its soldier's: its model, light and sparks) strikes: its hit and its blast, Quake VR's
// damage (not AD's own for that monster's bolt).
void() VR_ADGun_PlasmaTouch =
{
    if(self.ad_touchedvoid || other == self.owner || other.solid == SOLID_TRIGGER || VR_OwnerSpared(other))
    {
        return;
    }
    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }
    // Quake VR's bolt hit (as the laser cannon's and the nails'): the prop it meets pushed, where on a body it struck
    // (PositionalDamage: a head more), our sparks with AD's flash.
    vector dir = normalize(self.velocity);
    VR_ShotPush_Laser(other, self.origin, dir);
    VR_TouchTrace(self.origin - dir * 8, self.origin + dir * 8);
    vr_ad_ammo = AD_AMMO_CELLS;
    if(self.vr_adgun_splash > 0)
    {
        T_RadiusDamage(self, self.owner, self.vr_adgun_splash, other);
    }
    if(other.takedamage)
    {
        float before = other.health;
        float dmg = self.vr_adgun_dmg * PositionalDamageFactor(PositionalDamage(other, trace_endpos, dir));
        spawn_touchblood(dmg);
        T_Damage(other, self, self.owner, dmg);
        vr_adgun_hits = vr_adgun_hits + max(0, before - other.health);
    }
    vr_ad_ammo = 0;
    particle2(self.origin, '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 8);
    ad_SpawnExplosion(random() < 0.3 ? ad_EXPLODE_PLASMA_SMALL : ad_EXPLODE_ELECT_SMALL, self.origin, ad_SOUND_PLASMA_HIT);
    ad_entity_remove(self, 1);
};

void(float xHand, float xWid) VR_ADGun_FirePlasma =
{
    sound(self, VRGetGunChannel(xHand), ad_SOUND_PLASMA_FIRE, 1, ATTN_NORM);
    self.punchangle_x = -1;
    float kind = ad_CT_MONARMYPLASMA;
    if(xWid == WID_ADELIMINATOR) { kind = ad_CT_MONELIMATOR; }
    else if(xWid == WID_ADCENTURION) { kind = ad_CT_MONCENTURION; }
    vector dir = VRGetWeaponFireRotFwd(xHand);
    float speed = max(100, VR_ADGun_Value(xWid, "speed"));
    ad_launch_plasma(VRGetMuzzlePos(xHand), dir, kind, speed);
    entity m = ad_newmis;
    m.touch = VR_ADGun_PlasmaTouch;
    m.velocity = dir * speed + vrProjectileVelocity(xHand, 0.4);
    m.angles = vectoangles(m.velocity);
    m.vr_adgun_wid = xWid;
    m.vr_adgun_dmg = VR_ADGun_Value(xWid, "damage");
    m.vr_adgun_splash = VR_ADGun_Value(xWid, "splash");
    VR_ShotPlace(m, xHand);
    VRGunHaptic(xHand, 0.3, 60, 0.7);
};

// ---------------------------------------------------------------------------------------------------------------------
// The defender's shotgun

void(float xHand) VR_ADGun_FireDefender =
{
    sound(self, VRGetGunChannel(xHand), "enforcer/def_ssg_fire.wav", 1, ATTN_NORM);
    self.punchangle_x = -3;
    float pellets = max(1, rint(VR_ADGun_Value(WID_ADDEFENDER, "pellets")));
    float spread = max(0, VR_ADGun_Value(WID_ADDEFENDER, "spread"));
    vr_pellet_damage = VR_ADGun_Value(WID_ADDEFENDER, "damage");
    FireBullets(xHand, pellets, VRGetWeaponFireRotFwd(xHand), [spread, spread * 0.55, 0]); // (AD's: 0.18 by 0.1)
    vr_pellet_damage = 0;
    VRGunHaptic(xHand, 0.4, 70, 0.9);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 2);
    self.damage_weapon = IT_SUPER_SHOTGUN; // HONEY
};

// ---------------------------------------------------------------------------------------------------------------------
// The flamethrowers: a stream of flames or poison puffs (AD's sprites), each striking what it meets once

// The poison on `xVictim` (self, a vr_adpoison): its damage every half second while it lasts, a few green motes.
void() VR_ADGun_PoisonThink =
{
    entity v = self.enemy;
    if(v == world || v.vr_adpoison != self || v.health <= 0 || !v.takedamage || time >= self.attack_finished)
    {
        if(v != world && v.vr_adpoison == self)
        {
            v.vr_adpoison = world;
        }
        remove(self);
        return;
    }
    vr_ad_ammo = AD_AMMO_CELLS;
    float before = v.health;
    T_Damage(v, self, self.owner, self.dmg * 0.5);
    vr_adgun_hits = vr_adgun_hits + max(0, before - v.health);
    vr_ad_ammo = 0;
    particle(v.origin + (v.mins + v.maxs) * 0.5 + randomVec(8), '0 0 10', 56 + rint(random() * 7), 6);
    self.nextthink = time + 0.5;
};

// `xVictim` poisoned by `xBy` for `xTime` s, `xDps` a second (again: from now on, the stronger of the two).
void(entity xVictim, entity xBy, float xDps, float xTime) VR_ADGun_Poison =
{
    if(xDps <= 0 || xTime <= 0 || !xVictim.takedamage || xVictim.health <= 0)
    {
        return;
    }
    entity p = xVictim.vr_adpoison;
    if(p == world || p.classname != "vr_adpoison" || p.enemy != xVictim)
    {
        p = spawn();
        p.classname = "vr_adpoison";
        p.enemy = xVictim;
        p.movetype = MOVETYPE_NONE;
        p.solid = SOLID_NOT;
        p.think = VR_ADGun_PoisonThink;
        p.nextthink = time + 0.5;
        setorigin(p, xVictim.origin);
        xVictim.vr_adpoison = p;
        p.dmg = 0;
    }
    p.owner = xBy;
    p.dmg = max(p.dmg, xDps);
    p.attack_finished = max(p.attack_finished, time + xTime);
};

void() VR_ADGun_FlameThink =
{
    self.cnt = self.cnt + 1;
    if(self.cnt > 6 || time >= self.attack_finished)
    {
        remove(self);
        return;
    }
    self.frame = self.cnt;
    self.velocity_z = self.velocity_z + 10 + random() * 20; // (drifting up, slowing down: AD's flames)
    self.velocity = self.velocity - self.movedir * 0.125;
    self.nextthink = time + 0.05;
};

// A flame or puff meets something: what takes damage takes its damage once, then catches fire or is poisoned.
void() VR_ADGun_FlameTouch =
{
    if(other == self.owner || VR_OwnerSpared(other) || other.solid == SOLID_TRIGGER)
    {
        return;
    }
    if(!other.takedamage)
    {
        if(other.solid == SOLID_BSP || other == world)
        {
            remove(self); // (spent on the wall)
        }
        return;
    }
    vector dir = normalize(self.velocity);
    vr_ad_ammo = self.vr_adgun_poison > 0 ? AD_AMMO_CELLS : AD_AMMO_ROCKETS;
    float before = other.health;
    VR_SetHitKind(QVR_HITKIND_NONE, -1, self.origin, dir);
    T_Damage(other, self, self.owner, self.vr_adgun_dmg);
    VR_ClearHitKind();
    vr_adgun_hits = vr_adgun_hits + max(0, before - other.health);
    vr_ad_ammo = 0;
    if(self.vr_adgun_burn > 0 && pointcontents(self.origin) > CONTENT_WATER)
    {
        vr_burn_len = self.vr_adgun_burn;
        VR_Burn_Ignite(other, self.owner, self.origin, dir, VR_BURN_NAIL);
        vr_burn_len = 0;
    }
    if(self.vr_adgun_poison > 0)
    {
        VR_ADGun_Poison(other, self.owner, self.vr_adgun_poison, self.vr_adgun_ptime);
    }
    remove(self);
};

float(float xHand) VR_ADGun_GetStream = { return xHand == cVR_OffHand ? self.vr_adgun_stream2 : self.vr_adgun_stream; };
void(float xHand, float xTime) VR_ADGun_SetStream =
{
    if(xHand == cVR_OffHand) { self.vr_adgun_stream2 = xTime; }
    else { self.vr_adgun_stream = xTime; }
};

void(float xHand, float xWid) VR_ADGun_FireFlame =
{
    float poison = (xWid == WID_ADFUMIGATOR);
    if(!VR_ADGun_GetStream(xHand))
    {
        sound(self, VRGetGunChannel(xHand), poison ? "enforcer/fum_poisonon.wav" : "enforcer/pyro_flameon.wav", 1, ATTN_NORM);
    }
    VR_ADGun_SetStream(xHand, time);
    vector dir = VRGetWeaponFireRotFwd(xHand);
    vector org = VRGetMuzzlePos(xHand);
    entity f = spawn();
    f.classname = "vr_adflame";
    f.owner = self;
    f.movetype = MOVETYPE_FLYMISSILE;
    f.solid = SOLID_TRIGGER;
    float water = (pointcontents(org) <= CONTENT_WATER);
    setmodel(f, poison ? ad_SBURST_POISON : (water ? ad_SBURST_SMOKE : ad_SBURST_FLAME));
    setorigin(f, org);
    setsize(f, '0 0 0', '0 0 0');
    float speed = max(100, VR_ADGun_Value(xWid, "speed"));
    f.movedir = dir * speed + randomVec(10);
    f.velocity = f.movedir + vrProjectileVelocity(xHand, 0.5);
    f.angles_z = anglemod(350 + random() * 20);
    f.cnt = 0;
    f.attack_finished = time + max(0.1, VR_ADGun_Value(xWid, "range") / speed); // (its reach: units)
    f.vr_adgun_wid = xWid;
    f.vr_adgun_dmg = (water && !poison) ? 0 : VR_ADGun_Value(xWid, "damage");
    f.vr_adgun_burn = (poison || water) ? 0 : VR_ADGun_Value(xWid, "burn");
    f.vr_adgun_poison = poison ? VR_ADGun_Value(xWid, "poison") : 0;
    f.vr_adgun_ptime = poison ? VR_ADGun_Value(xWid, "poisontime") : 0;
    f.think = VR_ADGun_FlameThink;
    f.nextthink = time + 0.05;
    f.touch = VR_ADGun_FlameTouch;
    VRGunHaptic(xHand, 0.1, 40, 0.4);
};

// Every player frame, each hand (VR_EnemyGuns_Frame): a stream let go of (or the gun gone, empty) ends with its sound.
void(float xHand) VR_ADGun_Frame =
{
    float last = VR_ADGun_GetStream(xHand);
    if(!last)
    {
        return;
    }
    float wid = VRGetWeapon(xHand);
    float gap = max(0.05, VR_ADGun_Value(WID_ADPYRO, "rate")) * 2 + 0.05;
    if(wid == WID_ADPYRO || wid == WID_ADFUMIGATOR)
    {
        gap = max(0.05, VR_ADGun_Value(wid, "rate")) * 2 + 0.05;
    }
    if(time - last < gap && (wid == WID_ADPYRO || wid == WID_ADFUMIGATOR))
    {
        return;
    }
    VR_ADGun_SetStream(xHand, 0);
    sound(self, VRGetGunChannel(xHand), wid == WID_ADFUMIGATOR ? "enforcer/fum_poisonoff.wav" : "enforcer/pyro_flameoff.wav", 1,
        ATTN_NORM);
};

// ---------------------------------------------------------------------------------------------------------------------
// The trigger

// VR_EnemyGun_Attack for an AD soldier's gun with a round (or fuel) in it: one shot (a flame), the next after its time.
// FALSE (nothing done): not one of AD's guns, or empty (the enemy guns' empty click).
float(float xHand, float xWeapon, float xAttackFinished, float* xOutAttackFinished) VR_ADGun_Attack =
{
    if(!VR_IsADGun(xWeapon) || VRGetWeaponClip(xHand) < 1)
    {
        return FALSE;
    }
    RuneApplyBlackNoise(self);
    SuperDamageSound();
    makevectors(VRGetWeaponFireRot(xHand));
    self.show_hostile = time + 1; // wake monsters up
    VR_Stealth_Shot(self, xWeapon == WID_ADDEFENDER ? WID_SUPER_SHOTGUN : (xWeapon == WID_ADGRENADE ? WID_GRENADE_LAUNCHER :
        WID_ROCKET_LAUNCHER)); // QVR: heard (vr_stealth.qc)
    VRSetWeaponClip(xHand, VRGetWeaponClip(xHand) - 1);
    float stream = (xWeapon == WID_ADPYRO || xWeapon == WID_ADFUMIGATOR);
    if(!stream)
    {
        player_shot1(xHand);
    }
    if(xWeapon == WID_ADROCKET)          { VR_ADGun_FireRocket(xHand); }
    else if(xWeapon == WID_ADGRENADE)    { VR_ADGun_FireGrenade(xHand); }
    else if(xWeapon == WID_ADDEFENDER)   { VR_ADGun_FireDefender(xHand); }
    else if(stream)                      { VR_ADGun_FireFlame(xHand, xWeapon); }
    else                                 { VR_ADGun_FirePlasma(xHand, xWeapon); }
    if(!stream)
    {
        weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp)
    }
    float next = VR_ADGun_Value(xWeapon, stream ? "rate" : "refire");
    *xOutAttackFinished = time + RuneApplyHell(max(0.05, next), self);
    updateAmmoCounters();
    if(VRGetWeaponClip(xHand) < 1)
    {
        VR_EnemyGun_Spent(xHand);
    }
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("adgun: %s fired (hand %g), %g left\n", VR_ADGun_Name(xWeapon), xHand, VRGetWeaponClip(xHand)));
    }
    return TRUE;
};

// ---------------------------------------------------------------------------------------------------------------------
// Tests (vr_ad_test 70..72)

entity vr_adgun_test_target; // the target ahead
float vr_adgun_test_index;   // the gun being tested (0..7)
float vr_adgun_test_step;    // its step: 0 equip and fire, 1..5 fire on (the streams), 6 read
float vr_adgun_test_pass;
float vr_adgun_test_fail;
float vr_adgun_test_clip;    // the clip as the gun was given
float vr_adgun_test_grip;    // vr_weapon_grip_mode as it was (1 for the test: the gun stays in a hand not gripping)

void(entity attacker, float damage) VR_ADGun_TestTargetPain = {};
void() VR_ADGun_TestTargetDie = { self.health = 100000; };

// The target, `xDist` units out along the main hand's aim.
void(entity p, float xDist) VR_ADGun_TestPlace =
{
    entity t = vr_adgun_test_target;
    if(t == world)
    {
        t = spawn();
        t.classname = "vr_adgun_target";
        t.solid = SOLID_SLIDEBOX;
        t.movetype = MOVETYPE_NONE;
        t.takedamage = DAMAGE_AIM;
        t.th_pain = VR_ADGun_TestTargetPain;
        t.th_die = VR_ADGun_TestTargetDie;
        t.flags = 0; // (not a monster: the monsters' hooks would want its AI)
        setmodel(t, "progs/player.mdl");
        setsize(t, '-16 -16 -24', '16 16 32');
        vr_adgun_test_target = t;
    }
    t.health = 100000;
    entity oldSelf = self;
    self = p;
    vector at = VRGetMuzzlePos(cVR_MainHand) + VRGetWeaponFireRotFwd(cVR_MainHand) * xDist;
    self = oldSelf;
    setorigin(t, at - '0 0 4');
};

void() VR_ADGun_TestThink =
{
    entity p = self.owner;
    float wid = WID_ADROCKET + vr_adgun_test_index;
    entity oldSelf = self;
    if(vr_adgun_test_step == 0)
    {
        self = p;
        VR_ADGun_Ensure(wid);
        VR_WeaponUtil_EntAddWeapon(self, wid);
        VRGiveEntWeapon(self, cVR_MainHand, wid, 0, WeaponIdToBaseClipSize(wid));
        W_SetCurrentAmmoFor(cVR_MainHand);
        self = oldSelf;
        vr_adgun_test_clip = VRGetEntWeaponClip(p, cVR_MainHand);
        VR_ADGun_TestPlace(p, (wid == WID_ADPYRO || wid == WID_ADFUMIGATOR) ? 90 : 160);
        vr_adgun_hits = 0;
    }
    if(vr_adgun_test_step <= 5 && (vr_adgun_test_step == 0 || wid == WID_ADPYRO || wid == WID_ADFUMIGATOR))
    {
        self = p;
        VR_EnemyGun_Attack(cVR_MainHand, wid, 0, &self.attack_finished);
        self = oldSelf;
    }
    vr_adgun_test_step = vr_adgun_test_step + 1;
    if(vr_adgun_test_step < 6)
    {
        self.nextthink = time + 0.1;
        return;
    }
    if(vr_adgun_test_step < 20)
    {
        self.nextthink = time + 0.1; // (time for it to strike: 1.4 s)
        return;
    }
    float clip = VRGetEntWeaponClip(p, cVR_MainHand);
    float dealt = 100000 - vr_adgun_test_target.health;
    float ok = (clip < vr_adgun_test_clip && dealt > 0 && VRGetEntWeapon(p, cVR_MainHand) == wid);
    string model = WeaponIdToModel(wid, 0);
    bprint(sprintf("adgun test: %s (%s): rounds %g -> %g, dealt %g (hits %g): %s\n", VR_ADGun_Name(wid), model,
        vr_adgun_test_clip, clip, dealt, vr_adgun_hits, ok ? "PASS" : "FAIL"));
    if(ok) { vr_adgun_test_pass++; } else { vr_adgun_test_fail++; }
    vr_adgun_test_index = vr_adgun_test_index + 1;
    vr_adgun_test_step = 0;
    if(vr_adgun_test_index > WID_ADFUMIGATOR - WID_ADROCKET)
    {
        bprint(sprintf("adgun test: %g passed, %g failed\n", vr_adgun_test_pass, vr_adgun_test_fail));
        cvar_set("vr_weapon_grip_mode", ftos(vr_adgun_test_grip));
        remove(vr_adgun_test_target);
        vr_adgun_test_target = world;
        remove(self);
        return;
    }
    self.nextthink = time + 0.3;
};

// vr_ad_test 90: the shots the launchers and a plasma gun fire, each from the main hand (aim it level: TESTING.md), and
// how each leaves the muzzle: the grenade soldier's launcher's grenade against the grenade launcher's (phase 1: model,
// box, movement, touch, blast, fuse and speed the same), the rocket soldier's rocket ours, the plasma soldier's bolt AD's
// with our hit; each 0.3 s on still in flight, its own, at least 90 units from the muzzle, touches of its firer counted.
float vr_adgun_t90_phase;    // 0 the AD grenade launcher, 1 ours, 2 the AD rocket launcher, 3 the plasma rifle
entity vr_adgun_t90_shot;    // the shot followed
string vr_adgun_t90_model;   // ... its model as fired
vector vr_adgun_t90_from;    // ... the muzzle it left
vector vr_adgun_t90_gsize;   // the AD grenade's (phase 0): box, movement, fuse, speed, function checks
float vr_adgun_t90_gmove;
float vr_adgun_t90_gsolid;
float vr_adgun_t90_gfuse;
float vr_adgun_t90_gspeed;
float vr_adgun_t90_gfuncs;

void() VR_ADGun_ShotsTestThink =
{
    entity p = self.owner;
    entity oldSelf = self;
    float ph = vr_adgun_t90_phase;
    if(vr_adgun_test_step == 0)
    {
        self = p;
        float wid = (ph == 0 || ph == 4) ? WID_ADGRENADE : (ph == 1 ? WID_GRENADE_LAUNCHER : (ph == 2 ? WID_ADROCKET : WID_ADPLASMA));
        if(VR_IsADGun(wid)) { VR_ADGun_Ensure(wid); }
        VR_WeaponUtil_EntAddWeapon(self, wid);
        VRGiveEntWeapon(self, cVR_MainHand, wid, 0, WeaponIdToBaseClipSize(wid));
        self.ammo_rockets = max(self.ammo_rockets, 10);
        W_SetCurrentAmmoFor(cVR_MainHand);
        vr_adgun_t90_from = VRGetMuzzlePos(cVR_MainHand);
        vr_owner_touches = 0;
        ad_newmis = world;
        if(ph == 1) { W_FireGrenade(cVR_MainHand); }
        else if(ph == 4)
        {
            // (Not counted: the gun's grenade before 2026-10-15, AD's monster grenade model, launched as ours.)
            entity og = spawn();
            og.owner = self;
            og.movetype = MOVETYPE_BOUNCE;
            og.solid = SOLID_BBOX;
            og.classname = "grenade";
            og.velocity = VRGetWeaponFireRotFwd(cVR_MainHand) * 600 + vrProjectileVelocity(cVR_MainHand, 0.6);
            og.avelocity = '300 300 300';
            og.angles = vectoangles(og.velocity);
            og.touch = GrenadeTouch;
            og.nextthink = time + 2.5;
            og.think = GrenadeExplode;
            setmodel(og, ad_MODEL_PROJ_GRENADE);
            setsize(og, '0 0 0', '0 0 0');
            setorigin(og, VRGetMuzzlePos(cVR_MainHand));
            VR_ShotPlace(og, cVR_MainHand);
            VR_Grenade_Make(og, GrenadeExplode, 2.5, TRUE);
        }
        else { VR_EnemyGun_Attack(cVR_MainHand, wid, 0, &self.attack_finished); }
        self = oldSelf;
        entity s = world;
        if(ph == 3)
        {
            s = ad_newmis;
        }
        else
        {
            string cn = (ph <= 1 || ph == 4) ? "grenade" : "missile";
            for(entity g = find(world, classname, cn); g != world; g = find(g, classname, cn))
            {
                if(g.owner == p && g.vr_adgun_wid == 0) { s = g; }
            }
        }
        vr_adgun_t90_shot = s;
        if(s == world)
        {
            bprint(sprintf("adgun shots: %g: nothing fired: FAIL\n", ph));
            vr_adgun_test_fail++;
            vr_adgun_test_step = 20;
        }
        else
        {
            s.vr_adgun_wid = -1 - ph; // (marked: this test's)
            vr_adgun_t90_model = s.model;
            float fuse = (s.vr_gren_fuse ? s.vr_gren_fuse : s.nextthink) - time;
            float spd = vlen(s.velocity);
            float funcs = 0;
            if(ph <= 1)
            {
                funcs = ((s.touch == VR_Grenade_Touch ? s.vr_gren_touch : s.touch) == GrenadeTouch &&
                         (s.vr_gren_boom ? s.vr_gren_boom : s.think) == GrenadeExplode);
            }
            else if(ph == 2) { funcs = s.touch == T_MissileTouch; }
            else { funcs = s.touch == VR_ADGun_PlasmaTouch; }
            bprint(sprintf("adgun shots: %g: %s, box %v, move %g, solid %g, ", ph, s.model, s.size, s.movetype, s.solid));
            bprint(sprintf("fuse %.2f, speed %.0f, ours %g\n", fuse, spd, funcs));
            if(ph == 0)
            {
                vr_adgun_t90_gsize = s.size; vr_adgun_t90_gmove = s.movetype; vr_adgun_t90_gsolid = s.solid;
                vr_adgun_t90_gfuse = fuse; vr_adgun_t90_gspeed = spd; vr_adgun_t90_gfuncs = funcs;
            }
            else if(ph == 1)
            {
                float same = vr_adgun_t90_gfuncs && funcs && s.model == "progs/grenade.mdl" && s.size == vr_adgun_t90_gsize &&
                             s.movetype == vr_adgun_t90_gmove && s.solid == vr_adgun_t90_gsolid &&
                             fabs(fuse - vr_adgun_t90_gfuse) < 0.06 && fabs(spd - vr_adgun_t90_gspeed) < 0.05 * spd;
                bprint(sprintf("adgun shots: the grenade soldier's grenade is the grenade launcher's: %s\n",
                    same ? "PASS" : "FAIL"));
                if(same) { vr_adgun_test_pass++; } else { vr_adgun_test_fail++; }
            }
            else if(ph != 4)
            {
                float own = funcs && (ph == 3 || s.model == "progs/missile.mdl");
                bprint(sprintf("adgun shots: %g: its projectile %s: %s\n", ph, ph == 2 ? "our rocket" : "AD's bolt, our hit",
                    own ? "PASS" : "FAIL"));
                if(own) { vr_adgun_test_pass++; } else { vr_adgun_test_fail++; }
            }
        }
    }
    vr_adgun_test_step = vr_adgun_test_step + 1;
    if(vr_adgun_test_step == 7)
    {
        // 0.3 s on: still flying (not caught, not gone off), well clear of the muzzle.
        entity t = vr_adgun_t90_shot;
        float alive = t.vr_adgun_wid == -1 - ph && t.model == vr_adgun_t90_model;
        float d = alive ? vlen(t.origin - vr_adgun_t90_from) : 0;
        float ok = alive && t.carry_player == world && d >= 90;
        bprint(sprintf("adgun shots: %g: 0.3 s on: %s, %.0f from the muzzle, touches of its firer %g: %s\n", ph,
            alive ? (t.carry_player != world ? "held" : "flying") : "gone", d, vr_owner_touches,
            ph == 4 ? "(before, not counted)" : (ok ? "PASS" : "FAIL")));
        if(ph != 4)
        {
            if(ok) { vr_adgun_test_pass++; } else { vr_adgun_test_fail++; }
        }
    }
    if(vr_adgun_test_step < 20)
    {
        self.nextthink = time + 0.05;
        return;
    }
    vr_adgun_test_step = 0;
    vr_adgun_t90_phase = ph + 1;
    if(vr_adgun_t90_phase > 4)
    {
        bprint(sprintf("adgun shots: %g passed, %g failed\n", vr_adgun_test_pass, vr_adgun_test_fail));
        cvar_set("vr_weapon_grip_mode", ftos(vr_adgun_test_grip));
        remove(self);
        return;
    }
    self.nextthink = time + 0.5;
};

// ---------------------------------------------------------------------------------------------------------------------
// AD's weapons in any map (vr_ad_test 81..84; ROUND22.md "AD weapons everywhere"): the debug spawner's 309..319 (the
// three upgrades' pickups, the eight soldiers' guns as func_weapon_grabbable), as the toolgun and the Debug menu make
// them.

// The spawner's AD weapon `choice` (309..319) made `xDist` ahead of `p` (a pickup, or a gun lying about): it, or world
// (not made: outside its scope).
entity(entity p, float choice, float xDist) VR_ADWeapons_Make =
{
    makevectors(p.angles_y * '0 1 0');
    entity e = spawn();
    setorigin(e, p.origin + v_forward * xDist + '0 0 8');
    entity was = self;
    self = e;
    VR_EnemyDispenser_Spawn(choice);
    self = was;
    if(choice < 312)
    {
        return e.model != "" ? e : world; // (an upgrade's pickup: removed outside Weapon Upgrades' scope)
    }
    float wid = WID_ADROCKET + choice - 312;
    for(entity t = find(world, classname, "thrown_weapon"); t; t = find(t, classname, "thrown_weapon"))
    {
        if(t.weapon == wid && t.vr_drop_born == time)
        {
            return t;
        }
    }
    return world;
};

// `w` (a pickup or a weapon lying about) taken into `p`'s empty main hand, as a grip closing on it does.
void(entity p, entity w) VR_ADWeapons_Take =
{
    VRGiveEntWeapon(p, cVR_MainHand, WID_FIST, 0, 0);
    entity was = self;
    entity wasOther = other;
    p.touchinghand = cVR_MainHand;
    w.handtouch_hand = cVR_MainHand;
    w.handtouch_ent = p;
    other = p;
    self = w;
    float catching = forcegrabCatching;
    forcegrabCatching = TRUE;
    w.handtouch();
    forcegrabCatching = catching;
    self = was;
    other = wasOther;
};

// `p`'s main hand's rounds: its magazine and the reserve of its ammo.
float(entity p) VR_ADWeapons_Rounds =
{
    float wid = VRGetEntWeapon(p, cVR_MainHand);
    float ammo = WeaponIdToAmmoId(wid, 0);
    float reserve = (VR_IsADGun(wid) || ammo == AID_NONE) ? 0 : VR_AmmoUtil_GetAmmoCountByAmmoId(p, ammo);
    return VRGetEntWeaponClip(p, cVR_MainHand) + reserve;
};

// `p`'s main hand fires once (W_AttackImpl, as its trigger): a report line; TRUE when a round went (the Shadow Axe: its
// blow's scale).
float(entity p, string xWhat) VR_ADWeapons_Fire =
{
    float wid = VRGetEntWeapon(p, cVR_MainHand);
    entity inst = VRGetEntWeaponInst(p, cVR_MainHand);
    float before = VR_ADWeapons_Rounds(p);
    float code = AD_UpgradeCode(inst);
    entity was = self;
    self = p;
    W_SetCurrentAmmoFor(cVR_MainHand);
    float ok;
    float spent = 0;
    if(wid == WID_AXE)
    {
        float scale = AD_AxeScale(cVR_MainHand);
        ok = scale > 1;
        self = was;
        bprint(sprintf("adweapons: %s: hand weapon %g (%s), upgrade %g, blow x%g: %s\n", xWhat, wid,
            WeaponIdToWeaponName(wid), code, scale, ok ? "PASS" : "FAIL"));
        return ok;
    }
    self.attack_finished = 0;
    W_AttackImpl(cVR_MainHand, wid, self.weaponflags, 0, &self.weapon, &self.attack_finished);
    self = was;
    spent = before - VR_ADWeapons_Rounds(p);
    float want = 1;
    if(wid == WID_SUPER_SHOTGUN) { want = (code & 2) ? 3 : 2; } // (the Widowmaker: a third shell from the reserve)
    ok = ((spent == want) && (VRGetEntWeapon(p, cVR_MainHand) == wid));
    bprint(sprintf("adweapons: %s: hand weapon %g (%s), upgrade %g, model %s, ", xWhat, wid, WeaponIdToWeaponName(wid), code,
        WeaponIdToModel(wid, 0)));
    bprint(sprintf("fired: rounds %g -> %g (%g, want %g): %s\n", before, before - spent, spent, want, ok ? "PASS" : "FAIL"));
    return ok;
};

// vr_ad_test 83: what lies about and is held, for a save/load check.
void(entity p) VR_ADWeapons_Report =
{
    float guns = 0, gunsOk = 0, pickups = 0;
    for(entity t = find(world, classname, "thrown_weapon"); t; t = find(t, classname, "thrown_weapon"))
    {
        if(VR_IsADGun(t.weapon))
        {
            guns++;
            gunsOk += (t.modelindex > 0 && t.model == WeaponIdToModel(t.weapon, 0) && t.weaponinst.wi_clip > 0);
        }
    }
    for(entity e = nextent(world); e; e = nextent(e))
    {
        pickups += (e.vr_ad_upgrade != 0 && e.model != "" && e.modelindex > 0);
    }
    float wid = VRGetEntWeapon(p, cVR_MainHand);
    bprint(sprintf("adweapons report: AD guns lying %g (%g with their model and rounds), upgrade pickups %g, ", guns, gunsOk,
        pickups));
    bprint(sprintf("main hand %g (%s) upgrade %g, magazine %g, rounds with the reserve %g\n", wid, WeaponIdToWeaponName(wid),
        AD_UpgradeCode(VRGetEntWeaponInst(p, cVR_MainHand)), VRGetEntWeaponClip(p, cVR_MainHand), VR_ADWeapons_Rounds(p)));
};

void(float request, entity p) AD_GunTest =
{
    if(request == 81 || request == 82)
    {
        // 81: each AD weapon made ahead (the spawner's 309..319), taken into the main hand and fired once.
        // 82: each laid ahead (left there), then a Widowmaker taken into the main hand (save, load, then 83 and 84).
        float grip = cvar("vr_weapon_grip_mode");
        cvar_set("vr_weapon_grip_mode", "1"); // (a weapon stays in a hand not gripping)
        p.ammo_shells = max(p.ammo_shells, 50);
        p.ammo_cells = max(p.ammo_cells, 50);
        float pass = 0, fail = 0;
        for(float c = 309; c <= 319; c++)
        {
            entity w = VR_ADWeapons_Make(p, c, request == 81 ? 64 : 112 + (c - 309) * 24);
            if(w == world)
            {
                bprint(sprintf("adweapons: %g not made (its scope: vr_ad_upgrades %g, vr_ad_everywhere %g, vr_ad_rules %g): FAIL\n",
                    c, cvar("vr_ad_upgrades"), cvar("vr_ad_everywhere"), cvar("vr_ad_rules")));
                fail++;
                continue;
            }
            if(request == 82)
            {
                continue;
            }
            string what = sprintf("%g %s", c, w.netname);
            VR_ADWeapons_Take(p, w);
            if(VR_ADWeapons_Fire(p, what)) { pass++; } else { fail++; }
            VRGiveEntWeapon(p, cVR_MainHand, WID_FIST, 0, 0);
        }
        if(request == 82)
        {
            entity wm = VR_ADWeapons_Make(p, 310, 400);
            if(wm != world)
            {
                VR_ADWeapons_Take(p, wm);
            }
            VR_ADWeapons_Report(p);
        }
        else
        {
            bprint(sprintf("adweapons: %g passed, %g failed\n", pass, fail));
        }
        cvar_set("vr_weapon_grip_mode", ftos(grip));
        return;
    }
    if(request == 83)
    {
        VR_ADWeapons_Report(p);
        return;
    }
    if(request == 85)
    {
        // A plain super shotgun in the main hand, loaded, 53 shells in reserve (83's comparison, no AD weapon).
        VRGiveEntWeapon(p, cVR_MainHand, WID_SUPER_SHOTGUN, 0, 2);
        p.ammo_shells = 53;
        VR_ADWeapons_Report(p);
        return;
    }
    if(request == 86)
    {
        // What the Debug menu's AD pickup row (vr_test_spawn N with impulse 241) just made: the pickup or weapon with a
        // model nearest where it was made (vr_test_spawn_dist ahead), where it lies from you (ahead, within reach) and its
        // model (adspawn: one line, PASS or FAIL); then removed, for the next row's. A monster the spawner made too.
        makevectors('0 1 0' * p.v_viewangle_y);
        vector at = p.origin + v_forward * cvar("vr_test_spawn_dist");
        entity n = world;
        float best = 4096;
        for(entity e3 = nextent(world); e3; e3 = nextent(e3))
        {
            string k = substring(e3.classname, 0, 5);
            if(e3.modelindex > 0 && (k == "item_" || k == "weapo" || e3.classname == "thrown_weapon" ||
               ((e3.flags & FL_MONSTER) && e3.health > 0)) &&
               vlen(e3.origin - at) < best)
            {
                n = e3;
                best = vlen(e3.origin - at);
            }
        }
        if(n == world)
        {
            bprint(sprintf("adspawn: %g: nothing made: FAIL\n", cvar("vr_test_spawn")));
            return;
        }
        vector mid = (n.absmin + n.absmax) * 0.5;
        vector flat = mid - p.origin;
        float dz = flat_z;
        flat_z = 0;
        float dist = vlen(flat);
        float ahead = dist > 0 ? (flat * v_forward) / dist : 0;
        float near = (dist < max(128, cvar("vr_test_spawn_dist") + 64) && ahead > 0.7 && fabs(dz) < 64);
        bprint(sprintf("adspawn: %g: %s (%s) at %g ahead x%g, height %g: %s\n", cvar("vr_test_spawn"), n.classname, n.model,
            rint(dist), rint(ahead * 100) / 100, rint(dz), near ? "PASS" : "FAIL"));
        if(near)
        {
            remove(n);
        }
        return;
    }
    if(request == 84)
    {
        float grip2 = cvar("vr_weapon_grip_mode");
        cvar_set("vr_weapon_grip_mode", "1");
        bprint(sprintf("adweapons: the main hand fires: %s\n", VR_ADWeapons_Fire(p, "held") ? "PASS" : "FAIL"));
        cvar_set("vr_weapon_grip_mode", ftos(grip2));
        return;
    }
    if(request == 90)
    {
        if(!cvar("vr_ad_mounted"))
        {
            bprint("adgun shots: Arcane Dimensions' data is not loaded\n");
            return;
        }
        AD_WeaponsOnDemand();
        entity e90 = spawn();
        e90.classname = "vr_adgun_test";
        e90.owner = p;
        vr_adgun_t90_phase = 0;
        vr_adgun_test_step = 0;
        vr_adgun_test_pass = vr_adgun_test_fail = 0;
        vr_adgun_test_grip = cvar("vr_weapon_grip_mode");
        cvar_set("vr_weapon_grip_mode", "1");
        e90.think = VR_ADGun_ShotsTestThink;
        e90.nextthink = time + 0.1;
        bprint("adgun shots: the launchers' and plasma guns' shots from your main hand\n");
        return;
    }
    if(request == 70)
    {
        if(!cvar("vr_ad_mounted"))
        {
            bprint("adgun test: Arcane Dimensions' data is not loaded (its guns' models are its monsters')\n");
            return;
        }
        AD_WeaponsOnDemand(); // (the sounds AD's soldiers precache where they are: none here)
        entity e = spawn();
        e.classname = "vr_adgun_test";
        e.owner = p;
        vr_adgun_test_index = 0;
        vr_adgun_test_step = 0;
        vr_adgun_test_pass = vr_adgun_test_fail = 0;
        vr_adgun_test_grip = cvar("vr_weapon_grip_mode");
        cvar_set("vr_weapon_grip_mode", "1"); // (as it was again at the end)
        e.think = VR_ADGun_TestThink;
        e.nextthink = time + 0.1;
        bprint("adgun test: each gun fired from your main hand at a target ahead\n");
        return;
    }
    if(request == 71)
    {
        // Each gun dropped from a soldier's place ahead (VR_DropEnemyGun as VR_ADGun_Drop calls it): its model and clip.
        float pass = 0, fail = 0;
        makevectors(p.angles);
        entity s = spawn();
        setorigin(s, p.origin + v_forward * 96);
        s.angles = p.angles;
        entity oldSelf = self;
        for(float ci = 0; ci < 8; ci++)
        {
            // (each armed soldier's drop)
            float wid = VR_ADGun_ForClass(ci == 0 ? ad_CT_MONARMYROCKET : (ci == 1 ? ad_CT_MONARMYGRENADE :
                (ci == 2 ? ad_CT_MONARMYPLASMA : (ci == 3 ? ad_CT_MONDEFENDER : (ci == 4 ? ad_CT_MONELIMATOR :
                (ci == 5 ? ad_CT_MONCENTURION : (ci == 6 ? ad_CT_MONPYRO : ad_CT_MONFUMIGATOR)))))));
            self = s;
            VR_DropEnemyGun(wid);
            self = oldSelf;
            entity w = world;
            for(entity e2 = find(world, classname, "thrown_weapon"); e2; e2 = find(e2, classname, "thrown_weapon"))
            {
                if(e2.weapon == wid && e2.vr_drop_born == time)
                {
                    w = e2;
                }
            }
            float ok = (w != world && w.model == WeaponIdToModel(wid, 0) && w.weaponinst.wi_clip == WeaponIdToBaseClipSize(wid) &&
                        w.modelindex > 0);
            bprint(sprintf("adgun drop: %s (%g): %s, %g rounds: %s\n", VR_IsADGun(wid) ? VR_ADGun_Name(wid) :
                WeaponIdToWeaponName(wid), wid, w != world ? w.model : "none",
                w != world ? w.weaponinst.wi_clip : 0, ok ? "PASS" : "FAIL"));
            if(ok) { pass++; } else { fail++; }
        }
        remove(s);
        bprint(sprintf("adgun drop: %g passed, %g failed\n", pass, fail));
        return;
    }
    if(request >= 73 && request <= 80)
    {
        // The gun `request - 73` (in WID_ADROCKET's order) in your main hand, full (with vr_weapon_grip_mode 1, or the
        // grip held, else it drops at once): a look at it held.
        float gw = WID_ADROCKET + request - 73;
        entity was = self;
        self = p;
        VR_ADGun_Ensure(gw);
        VR_WeaponUtil_EntAddWeapon(self, gw);
        VRGiveEntWeapon(self, cVR_MainHand, gw, 0, WeaponIdToBaseClipSize(gw));
        W_SetCurrentAmmoFor(cVR_MainHand);
        self = was;
        bprint(sprintf("adgun: %s in your main hand\n", VR_ADGun_Name(gw)));
        return;
    }
    if(request == 87)
    {
        // Carried through a level change: the rocket soldier's launcher in the main hand (with vr_weapon_grip_mode 1)
        // and the plasma soldier's rifle in the left shoulder holster; then `changelevel` to a map without those
        // soldiers (or vr_adgun_drops 0) and `vr_ad_test 88`.
        entity was2 = self;
        self = p;
        VR_ADGun_Ensure(WID_ADROCKET);
        VR_WeaponUtil_EntAddWeapon(self, WID_ADROCKET);
        VRGiveEntWeapon(self, cVR_MainHand, WID_ADROCKET, 0, WeaponIdToBaseClipSize(WID_ADROCKET));
        W_SetCurrentAmmoFor(cVR_MainHand);
        self = was2;
        VR_ADGun_Ensure(WID_ADPLASMA);
        p.holsterweapon0 = WID_ADPLASMA;
        p.holsterweaponflags0 = 0;
        p.holsterweaponinst0 = WeaponInst_Carried(WID_ADPLASMA, WeaponIdToBaseClipSize(WID_ADPLASMA), 0);
        p.holsterweaponmodel0 = WeaponIdToModel(WID_ADPLASMA, 0);
        bprint(sprintf("adgun carry: armed %s (%s), holster %s (%s)\n", VR_ADGun_Name(p.weapon), p.weaponmodel,
            VR_ADGun_Name(p.holsterweapon0), p.holsterweaponmodel0));
        return;
    }
    if(request == 88)
    {
        float okHand = p.weapon == WID_ADROCKET && p.weaponmodel == WeaponIdToModel(WID_ADROCKET, 0);
        float okHolster = p.holsterweapon0 == WID_ADPLASMA && p.holsterweaponmodel0 == WeaponIdToModel(WID_ADPLASMA, 0);
        bprint(sprintf("adgun carry: %s: hand %g (%s), holster %g (%s), drops %g: %s\n", mapname, p.weapon,
            p.weaponmodel, p.holsterweapon0, p.holsterweaponmodel0, cvar("vr_adgun_drops"),
            (okHand && okHolster) ? "PASS" : "FAIL"));
        return;
    }
    if(request == 89)
    {
        // A gun model the engine cannot make (as another release of AD's model, or a replacement pack): id's soldier's
        // model named as an AD gun. Quake VR's stand-in's file loads under the name (before: a Host_Error).
        entity g = spawn();
        setmodel(g, sprintf("%s#gun0", "progs/soldier.mdl")); // (made: setmodel precaches it late)
        bprint(sprintf("adgun standin: %s index %g size %v: %s\n", g.model, g.modelindex, g.size,
            (g.modelindex > 0 && vlen(g.size) > 0) ? "PASS" : "FAIL"));
        remove(g);
        return;
    }
    if(request == 72)
    {
        for(float w2 = WID_ADROCKET; w2 <= WID_ADFUMIGATOR; w2++)
        {
            float st = (w2 == WID_ADPYRO || w2 == WID_ADFUMIGATOR);
            if(w2 == WID_ADROCKET || w2 == WID_ADGRENADE)
            {
                bprint(sprintf("%s (%s): Quake VR's %s, ", VR_ADGun_Name(w2), WeaponIdToModel(w2, 0),
                    w2 == WID_ADROCKET ? "rocket" : "grenade"));
            }
            else
            {
                bprint(sprintf("%s (%s): damage %g, splash %g, speed %g, ", VR_ADGun_Name(w2), WeaponIdToModel(w2, 0),
                    VR_ADGun_Value(w2, "damage"), VR_ADGun_Value(w2, "splash"), VR_ADGun_Value(w2, "speed")));
            }
            bprint(sprintf("%s %g, %s %g\n", st ? "fuel" : "ammo", VR_ADGun_ClipSize(w2), st ? "rate" : "refire",
                VR_ADGun_Value(w2, st ? "rate" : "refire")));
        }
        return;
    }
};
