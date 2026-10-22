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
// plain soldier and enforcer drop Quake VR's grunt's burst rifle and enforcer's laser rifle (their own kinds of gun). The
// crossbow knight drops its crossbow (WID_ADCROSSBOW: AD's w_dcrossbow.mdl; AD's bolts, vr_adgun_crossbow_*; vr_ad_test 93).
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
    // (the eliminator's and the centurion's plasma guns are one gun as the player's: Vittorio, 2026-10-18)
    else if(t == ad_CT_MONELIMATOR || t == ad_CT_MONCENTURION) { wid = WID_ADCENTURION; }
    else if(t == ad_CT_MONPYRO)        { wid = WID_ADPYRO; }
    else if(t == ad_CT_MONFUMIGATOR)   { wid = WID_ADFUMIGATOR; }
    else if(t == ad_CT_MONDCROSS)      { wid = WID_ADCROSSBOW; }   // (the crossbow knight: AD's w_dcrossbow.mdl)
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
    precache_sound("weapons/crossbow_fire.wav"); // (the crossbow knight's crossbow: its shot and its bolts' hits)
    precache_sound("weapons/bolt_hit1.wav");
    precache_sound("weapons/bolt_hit4.wav");
    precache_model(ad_MODEL_PROJ_BOLT1);
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
// An older save's (or its level parms') eliminator's plasma gun (WID_ADELIMINATOR, gone: Vittorio, 2026-10-18, the same
// gun as the centurion's): the centurion's, with its model. A slot's model left empty if it was (a hidden hand's).
string(float xWid, string xModel) VR_ADGun_LegacyModel =
{
    if(xWid != WID_ADELIMINATOR || xModel == "")
    {
        return xModel;
    }
    return VR_ADGun_Model(WID_ADCENTURION);
};

float(float xWid) VR_ADGun_LegacyId =
{
    return xWid == WID_ADELIMINATOR ? WID_ADCENTURION : xWid;
};

// `p`'s hands and holsters.
void(entity p) VR_ADGun_LegacyIds =
{
    p.weaponmodel = VR_ADGun_LegacyModel(p.weapon, p.weaponmodel);
    p.weapon = VR_ADGun_LegacyId(p.weapon);
    p.weaponmodel2 = VR_ADGun_LegacyModel(p.weapon2, p.weaponmodel2);
    p.weapon2 = VR_ADGun_LegacyId(p.weapon2);
    p.holsterweaponmodel0 = VR_ADGun_LegacyModel(p.holsterweapon0, p.holsterweaponmodel0);
    p.holsterweapon0 = VR_ADGun_LegacyId(p.holsterweapon0);
    p.holsterweaponmodel1 = VR_ADGun_LegacyModel(p.holsterweapon1, p.holsterweaponmodel1);
    p.holsterweapon1 = VR_ADGun_LegacyId(p.holsterweapon1);
    p.holsterweaponmodel2 = VR_ADGun_LegacyModel(p.holsterweapon2, p.holsterweaponmodel2);
    p.holsterweapon2 = VR_ADGun_LegacyId(p.holsterweapon2);
    p.holsterweaponmodel3 = VR_ADGun_LegacyModel(p.holsterweapon3, p.holsterweaponmodel3);
    p.holsterweapon3 = VR_ADGun_LegacyId(p.holsterweapon3);
    p.holsterweaponmodel4 = VR_ADGun_LegacyModel(p.holsterweapon4, p.holsterweaponmodel4);
    p.holsterweapon4 = VR_ADGun_LegacyId(p.holsterweapon4);
    p.holsterweaponmodel5 = VR_ADGun_LegacyModel(p.holsterweapon5, p.holsterweaponmodel5);
    p.holsterweapon5 = VR_ADGun_LegacyId(p.holsterweapon5);
};

// OnLoadGame: the guns lying about (thrown or dropped, a map's or the toolgun's func_weapon_grabbable not yet made)
// and the players'. Returns how many were changed (vr_ad_test 71).
float() VR_ADGun_LegacyWorld =
{
    float n = 0;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.classname == "player")
        {
            float before = (e.weapon == WID_ADELIMINATOR) + (e.weapon2 == WID_ADELIMINATOR);
            VR_ADGun_LegacyIds(e);
            n += before;
        }
        else if(e.weapon == WID_ADELIMINATOR && (e.classname == "thrown_weapon" || e.classname == "func_weapon_grabbable"))
        {
            e.weapon = WID_ADCENTURION;
            if(e.model != "")
            {
                vector mn = e.mins, mx = e.maxs;
                VR_ADGun_Ensure(WID_ADCENTURION);
                setmodel(e, VR_ADGun_Model(WID_ADCENTURION));
                setsize(e, mn, mx);
            }
            n++;
        }
    }
    return n;
};

void(entity p) VR_ADGun_EnsureCarried =
{
    VR_ADGun_LegacyIds(p); // (an older save's eliminator's gun)
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
    if(wid == WID_ADCROSSBOW)
    {
        self.ad_gib1mdl = ad_string_null; // (its crossbow fell as ours: AD's gibs throw no second one)
    }
    if(cvar("developer"))
    {
        dprint(sprintf("adgun: %s drops its %s (%g in it)\n", self.classname,
            VR_IsADGun(wid) ? VR_EnemyGun_Name(wid) : WeaponIdToWeaponName(wid), WeaponIdToBaseClipSize(wid)));
    }
};

// Freddie's gun arm: his rig's left forearm (vr_ad_rigs.inc adFreddieSeeds: bone 4, forearm_l), the super nailgun's
// five barrels on it (mon_freddie.mdl vertices 88..182: the engine leaves them off his limbs' models, vr_limbmodel.cpp).
float VR_AD_FREDDIE_GUNBONE = 4;
float vr_adgun_freddie_drops; // the super nailguns his cut arms dropped (the tests)

// `e`'s limb just cut off or popped (VR_Limb_Cut; `xTakes` the bones it took): Freddie's gun arm (the forearm, or the
// whole arm) drops the super nailgun on it as Quake VR's, loaded as a dropped enemy gun is (VR_DropEnemyGun: a full
// clip), where the piece was, turned and moving as it (his gun's barrels along his model's +x, as the nailgun's).
void(entity e, float xTakes) VR_ADGun_LimbCut =
{
    if(e.ad_classtype != ad_CT_MONFREDDIE || !(xTakes & VR_Limb_Bit(VR_AD_FREDDIE_GUNBONE)) || !cvar("vr_adgun_drops"))
    {
        return;
    }
    vector at = ragdollcut(e, 0);
    vector vel = ragdollcut(e, 2);
    entity g = CreateThrownWeapon(e, WID_SUPER_NAILGUN, 0, WeaponInst_New(WeaponIdToBaseClipSize(WID_SUPER_NAILGUN)),
        at, ragdollcut(e, 1), vel, randomVec(60),
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(WID_SUPER_NAILGUN));
    if(g != world)
    {
        vr_adgun_freddie_drops++;
    }
    dprint(sprintf("adgun: %s's gun arm off: a super nailgun (%g in it) at %v\n", e.classname,
        WeaponIdToBaseClipSize(WID_SUPER_NAILGUN), at));
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

// The plasma rifles' bolts (the soldiers', eliminators' and centurions', AD's ad_launch_plasma: fired by those monsters
// or by the player with their dropped guns) are electric, one path for both: TRUE for such a bolt (not a poisonous one).
float(entity bolt) VR_ADGun_IsElectric =
{
    float k = bolt.ad_classproj;
    return !bolt.ad_poisonous && (k == ad_CT_MONARMYPLASMA || k == ad_CT_MONELIMATOR || k == ad_CT_MONCENTURION);
};

// The damage AD's radius damage throws props and ragdolls with (physicsblast, from ad_T_RadiusDamage: ad_import.py): a
// plasma bolt's blast (the Plasma Gun's, the plasma monsters') as a blast of vr_ad_plasma_push damage when that is more
// (its splash is 20: within 60 units, a tenth of a rocket's throw a step from it; the author's 2026-10-22 note: "very
// very subtle"), every other blast its own damage.
float(entity xInflictor, float xDamage) VR_ADGun_BlastPush =
{
    if(xInflictor.ad_classtype == ad_CT_PROJ_PLASMA)
    {
        return max(xDamage, cvar("vr_ad_plasma_push"));
    }
    return xDamage;
};

// An electric bolt leaving a muzzle at `org` going `dir`, or (`xHit`) striking there: blue-white sparks and crackling
// arcs (vr_adgun_bolt_fx; its trail is the engine's: vr_particles.cpp electricTrail).
void(vector org, vector dir, float xHit) VR_ADGun_BoltFx =
{
    if(cvar("vr_adgun_bolt_fx"))
    {
        dir = normalize(dir);
        particle2(xHit ? org - dir * 4 : org, xHit ? dir * -1 : dir, QVR_PARTICLE_PRESET_ELECTRICSPARKS, xHit ? 10 : 8);
    }
};

// An electric bolt has struck `t` (alive before: `xAlive`) at `xAt` going `xDir`: the lightning's electrocution, on the
// living and on the dead (vr_shock.qc: the lightning gun's short arcs over it, then its lasting shock: arcs, a corpse's
// convulsions, crackle and burns).
void(entity t, float xAlive, vector xAt, vector xDir) VR_ADGun_BoltShock =
{
    if(!(t.flags & FL_MONSTER) && !t.vr_corpse)
    {
        return;
    }
    bodyshock(t, 0.75);
    VR_Shock_Hit(t, xAlive, xAt, xDir);
    dprint(sprintf("adgun bolt shock: %s alive before %g, health %g, corpse %g, shock left %g\n", t.classname, xAlive,
        t.health, t.vr_corpse, max(0, t.vr_shock_until - time)));
};

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
    VR_Proj_Begin(QVR_PROJ_LASER, self.vr_adgun_dmg); // (every projectile's hit: weapons.qc)
    vr_ad_ammo = AD_AMMO_CELLS;
    if(self.vr_adgun_splash > 0)
    {
        T_RadiusDamage(self, self.owner, self.vr_adgun_splash, other);
    }
    if(other.takedamage)
    {
        float before = other.health;
        float dmg = self.vr_adgun_dmg * vr_proj_factor;
        spawn_touchblood(dmg);
        vector at = vr_proj_at;
        T_Damage(other, self, self.owner, dmg);
        vr_adgun_hits = vr_adgun_hits + max(0, before - other.health);
        VR_ADGun_BoltShock(other, before > 0, at, dir);
    }
    vr_ad_ammo = 0;
    VR_Proj_End();
    particle2(self.origin, '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 8);
    VR_ADGun_BoltFx(self.origin, dir, TRUE);
    ad_SpawnExplosion(random() < 0.3 ? ad_EXPLODE_PLASMA_SMALL : ad_EXPLODE_ELECT_SMALL, self.origin, ad_SOUND_PLASMA_HIT);
    ad_entity_remove(self, 1);
};

void(float xHand, float xWid) VR_ADGun_FirePlasma =
{
    sound(self, VRGetGunChannel(xHand), ad_SOUND_PLASMA_FIRE, 1, ATTN_NORM);
    self.punchangle_x = -1;
    float kind = ad_CT_MONARMYPLASMA;
    if(xWid == WID_ADCENTURION || xWid == WID_ADELIMINATOR) { kind = ad_CT_MONCENTURION; }
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
    VR_ShotLaunched(m, xHand); // (its muzzle's crackle: ad_launch_plasma's VR_ADGun_BoltFx, as the monsters')
    VRGunHaptic(xHand, 0.3, 60, 0.7);
};

// ---------------------------------------------------------------------------------------------------------------------
// The crossbow knight's crossbow

// The bolt in its groove (vr_adcrossbow.cpp, vr_adgun_crossbow_anim): its drawn size (the engine's boltScale). Where it
// lies comes with each move (.launchofs, .offlaunchofs).
#define VR_ADXBOW_BOLT_SCALE 0.75

// Its bolt (AD's, launched as the knight's: its model, speed and flight) strikes: Quake VR's damage (vr_adgun_crossbow_damage,
// positional: a head more), taken by AD's monsters as nails; AD's bolt sounds. Bare minimum (ROUND22 "AD crossbow"): it
// does not stick where it lands.
void() VR_ADGun_BoltTouch =
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
    // Every projectile's hit (weapons.qc VR_Proj_Begin): what it met pushed (a ragdoll, a prop), where it struck (a head
    // more), a corpse's head or limb worn, a kill at the head or a limb popped by chance (vr_decap_pop_projectile).
    VR_Proj_Begin(QVR_PROJ_BOLT, self.vr_adgun_dmg);
    sound(self, CHAN_WEAPON, random() < 0.3 ? "weapons/bolt_hit1.wav" : "weapons/bolt_hit4.wav", 1, ATTN_NORM);
    if(other.takedamage)
    {
        float before = other.health;
        float dmg = self.vr_adgun_dmg * vr_proj_factor;
        spawn_touchblood(dmg);
        vr_ad_ammo = AD_AMMO_NAILS;
        T_Damage(other, self, self.owner, dmg);
        vr_ad_ammo = 0;
        vr_adgun_hits = vr_adgun_hits + max(0, before - other.health);
        if(cvar_hget(cvarh_vr_debug_shots))
        {
            dprint(sprintf("adgun: crossbow bolt hit %s for %g (health %g -> %g)\n", other.classname, dmg, before,
                other.health));
        }
    }
    else
    {
        particle2(self.origin, '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 4);
        if(cvar_hget(cvarh_vr_debug_shots))
        {
            dprint(sprintf("adgun: crossbow bolt struck %s at %v\n", other == world ? "the world" : other.classname, self.origin));
        }
    }
    VR_Proj_End();
    ad_entity_remove(self, 1);
};

void(float xHand) VR_ADGun_FireCrossbow =
{
    sound(self, VRGetGunChannel(xHand), "weapons/crossbow_fire.wav", 1, ATTN_NORM);
    self.punchangle_x = -1;
    vector dir = VRGetWeaponFireRotFwd(xHand);
    float speed = max(100, VR_ADGun_Value(WID_ADCROSSBOW, "speed"));
    vector org = VRGetMuzzlePos(xHand);
    vector ofs = VR_IMPL_SELECT(xHand, self.offlaunchofs, self.launchofs);
    float drawn = 1;
    if(cvar("vr_adgun_crossbow_anim") && ofs != '0 0 0')
    {
        // The bolt the client draws in the crossbow's groove, along the shot (vr_adcrossbow.cpp), is the one that leaves:
        // from its middle as drawn (.launchofs: from the muzzle in the shot's frame, however the crossbow is posed), at
        // its size. Not through a wall (the muzzle then).
        makevectors(VRGetWeaponFireRot(xHand));
        vector groove = org + v_forward * ofs_x + v_right * ofs_y + v_up * ofs_z;
        traceline(org, groove, TRUE, self);
        if(trace_fraction == 1 && !trace_startsolid)
        {
            org = groove;
        }
        drawn = VR_ADXBOW_BOLT_SCALE;
    }
    ad_launch_projectile(org, dir, ad_CT_PROJ_BOLT1, speed);
    entity m = ad_newmis;
    m.scale = drawn;
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("adgun: crossbow bolt launched at %v way %v (from the muzzle %v)\n", org, dir, ofs));
    }
    m.touch = VR_ADGun_BoltTouch;
    m.velocity = dir * speed + vrProjectileVelocity(xHand, 0.4);
    m.angles = vectoangles(m.velocity);
    m.vr_adgun_wid = WID_ADCROSSBOW;
    m.vr_adgun_dmg = VR_ADGun_Value(WID_ADCROSSBOW, "damage");
    VR_ShotLaunched(m, xHand);
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
        dprint(sprintf("adpoison: %s's poison ends (puffs %g, health %g, %g s left)\n", v.classname, self.frags, v.health,
            max(0, self.attack_finished - time)));
        remove(self);
        return;
    }
    // Green smoke puffing off it all the while (every quarter second; vr_adgun_stream_fire 0: AD's few motes), its damage
    // every other time (half a second).
    if(cvar("vr_adgun_stream_fire") > 0)
    {
        particle2(v.origin + (v.mins + v.maxs) * 0.5, (v.maxs - v.mins) * (0.5 / 16), QVR_PARTICLE_PRESET_POISONPUFF, 2);
        self.frags = self.frags + 1; // (puffs sent: the tests)
    }
    else if(!self.cnt)
    {
        particle(v.origin + (v.mins + v.maxs) * 0.5 + randomVec(8), '0 0 10', 56 + rint(random() * 7), 6);
    }
    self.cnt = !self.cnt;
    if(self.cnt)
    {
        vr_ad_ammo = AD_AMMO_CELLS;
        float before = v.health;
        vr_hit_nobleed = TRUE; // (poison: no blood, no wound)
        T_Damage(v, self, self.owner, self.dmg * 0.5);
        vr_hit_nobleed = FALSE;
        vr_adgun_hits = vr_adgun_hits + max(0, before - v.health);
        vr_ad_ammo = 0;
    }
    self.nextthink = time + 0.25;
};

// `xVictim` poisoned by `xBy` with `xFrom` (the spray that struck: its damage credit) for `xTime` s, `xDps` a second
// (again: from now on, the stronger of the two).
void(entity xVictim, entity xFrom, entity xBy, float xDps, float xTime) VR_ADGun_Poison =
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
        p.cnt = 0;
        if(cvar("vr_adgun_stream_fire") > 0) // (green smoke puffing off it from the first: VR_ADGun_PoisonThink)
        {
            particle2(xVictim.origin + (xVictim.mins + xVictim.maxs) * 0.5, (xVictim.maxs - xVictim.mins) * (0.5 / 16),
                QVR_PARTICLE_PRESET_POISONPUFF, 3);
        }
    }
    p.owner = xBy;
    VR_DamageCredit_Inherit(p, xFrom);
    p.dmg = max(p.dmg, xDps);
    p.attack_finished = max(p.attack_finished, time + xTime);
};

// A flamethrower's flame or poison puff (`flame`: AD's sprite, the player's or a pyro's or fumigator's own), each think
// (`xShare` of a tenth of a second): AD's sprite see-through (vr_adgun_stream_alpha) and our torches' fire tongues
// carried with it (vr_adgun_stream_fire a tenth of a second; green: poison). Not as smoke under water.
void(entity flame, float poison) VR_ADGun_StreamFx =
{
    float a = cvar("vr_adgun_stream_alpha");
    flame.alpha = (a > 0 && a < 1) ? max(0.1, a) : 0; // (0: opaque)
    float n = cvar("vr_adgun_stream_fire") * (flame.classname == "vr_adflame" ? 0.5 : 1);
    if(n <= 0 || (!poison && flame.model != ad_SBURST_FLAME))
    {
        return;
    }
    n = floor(n + random()); // (a half: every other think on average)
    if(n > 0)
    {
        float sp = vlen(flame.velocity);
        vector v = sp > 750 ? flame.velocity * (750 / sp) : flame.velocity; // (as sent: under 8 a unit, by 1/16)
        particle2(flame.origin, v * 0.01, poison ? QVR_PARTICLE_PRESET_ADPOISON : QVR_PARTICLE_PRESET_ADFLAME, n);
    }
};

// A flamethrower's muzzle at `org` (`dir` the way it sprays) as a flame leaves it, a pyro's or fumigator's or the
// player's with their dropped gun: a few slow tongues of fire licking out of it and its smoke (the fumigator's: green, no
// smoke; under water: nothing).
void(vector org, vector dir, float poison, float water) VR_ADGun_StreamMuzzle =
{
    if(cvar("vr_adgun_stream_fire") <= 0 || water)
    {
        return;
    }
    particle2(org, normalize(dir) * 0.6, poison ? QVR_PARTICLE_PRESET_ADPOISON : QVR_PARTICLE_PRESET_ADFLAME, 1);
    if(!poison && random() < 0.3)
    {
        particle2(org, '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);
    }
};

// A flame's or puff's flight: AD's flames' (ad_pyroflame_think: a tenth of a second a frame, drifting up, an eighth of
// its speed lost a frame, gone after seven), at twice their thinks (the fire carried with it: VR_ADGun_StreamFx), so
// each think half of theirs. (It once took a whole frame's slowing and a frame a think: it died at half AD's reach, a
// third of the range set, short of a big monster held off at a distance, the minotaur: ROUND22.md.)
void() VR_ADGun_FlameThink =
{
    self.cnt = self.cnt + 1;
    if(self.cnt > 13 || time >= self.attack_finished)
    {
        remove(self);
        return;
    }
    self.frame = min(6, floor(self.cnt / 2));
    self.velocity_z = self.velocity_z + 5 + random() * 10;
    self.velocity = self.velocity - self.movedir * 0.0625;
    self.nextthink = time + 0.05;
    VR_ADGun_StreamFx(self, self.vr_adgun_poison > 0 || self.model == ad_SBURST_POISON);
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
    VR_Proj_Begin(QVR_PROJ_BLAST, 0); // VR: every projectile's hit (weapons.qc VR_Proj_Begin)
    vr_hit_nobleed = TRUE; // (it burns or poisons: no blood, no wound, no gore: combat.qc VR_HitBloodless)
    T_Damage(other, self, self.owner, self.vr_adgun_dmg);
    vr_hit_nobleed = FALSE;
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
        VR_ADGun_Poison(other, self, self.owner, self.vr_adgun_poison, self.vr_adgun_ptime);
    }
    remove(self);
};

float(float xHand) VR_ADGun_GetStream = { return xHand == cVR_OffHand ? self.vr_adgun_stream2 : self.vr_adgun_stream; };
void(float xHand, float xTime) VR_ADGun_SetStream =
{
    if(xHand == cVR_OffHand) { self.vr_adgun_stream2 = xTime; }
    else { self.vr_adgun_stream = xTime; }
};

// A flame's life (s) for its reach `xRange` (units) at `xSpeed`: slowing as AD's (VR_ADGun_FlameThink: its speed less
// 1.25 times it a second, it covers xSpeed * (t - 0.625 t^2)) and gone after AD's 0.7 s, so it reaches at most about
// 0.39 times its speed (the pyro's 600: 236 units).
float(float xRange, float xSpeed) VR_ADGun_FlameLife =
{
    float q = 1 - 2.5 * xRange / xSpeed;
    if(q <= 0.015625) // ((1 - 1.25 * 0.7)^2: farther than it flies in 0.7 s)
    {
        return 0.7;
    }
    return max(0.1, (1 - sqrt(q)) / 1.25);
};

void(float xHand, float xWid) VR_ADGun_FireFlame =
{
    float poison = (xWid == WID_ADFUMIGATOR);
    if(!VR_ADGun_GetStream(xHand))
    {
        // (Its sounds, on and off, precached as the stream starts: a gun from wherever, in any map or a loaded game.)
        AD_WeaponsOnDemand();
        sound(self, VRGetGunChannel(xHand), poison ? "enforcer/fum_poisonon.wav" : "enforcer/pyro_flameon.wav", 1, ATTN_NORM);
    }
    VR_ADGun_SetStream(xHand, time);
    vector dir = VRGetWeaponFireRotFwd(xHand);
    vector org = VRGetMuzzlePos(xHand);
    entity f = spawn();
    f.classname = "vr_adflame";
    VR_DamageCredit_FromHand(f, self, xHand);
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
    f.attack_finished = time + VR_ADGun_FlameLife(VR_ADGun_Value(xWid, "range"), speed);
    f.vr_adgun_wid = xWid;
    f.vr_adgun_dmg = (water && !poison) ? 0 : VR_ADGun_Value(xWid, "damage");
    f.vr_adgun_burn = (poison || water) ? 0 : VR_ADGun_Value(xWid, "burn");
    f.vr_adgun_poison = poison ? VR_ADGun_Value(xWid, "poison") : 0;
    f.vr_adgun_ptime = poison ? VR_ADGun_Value(xWid, "poisontime") : 0;
    f.think = VR_ADGun_FlameThink;
    f.nextthink = time + 0.05;
    f.touch = VR_ADGun_FlameTouch;
    VR_ADGun_StreamFx(f, poison);
    VR_ADGun_StreamMuzzle(org, dir, poison, water);
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
    float heardAs = WID_ROCKET_LAUNCHER;
    if(xWeapon == WID_ADDEFENDER)       { heardAs = WID_SUPER_SHOTGUN; }
    else if(xWeapon == WID_ADGRENADE)   { heardAs = WID_GRENADE_LAUNCHER; }
    else if(xWeapon == WID_ADCROSSBOW)  { heardAs = WID_NAILGUN; } // (a crossbow: as quiet as the nailgun)
    VR_Stealth_Shot(self, heardAs); // QVR: heard (vr_stealth.qc)
    VRSetWeaponClip(xHand, VRGetWeaponClip(xHand) - 1);
    float stream = (xWeapon == WID_ADPYRO || xWeapon == WID_ADFUMIGATOR);
    if(!stream)
    {
        player_shot1(xHand);
    }
    if(xWeapon == WID_ADROCKET)          { VR_ADGun_FireRocket(xHand); }
    else if(xWeapon == WID_ADGRENADE)    { VR_ADGun_FireGrenade(xHand); }
    else if(xWeapon == WID_ADDEFENDER)   { VR_ADGun_FireDefender(xHand); }
    else if(xWeapon == WID_ADCROSSBOW)   { VR_ADGun_FireCrossbow(xHand); }
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
