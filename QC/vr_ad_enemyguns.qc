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
.float vr_adgun_scorched; // a flame's: its scorch made (VR_ADGun_FlameScorch: once; AD's slide along a wall)
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
    precache_sound("traps/steam_loop.wav"); // (a poison tank's burst's cloud: VR_TankField_Loop)
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

// ---------------------------------------------------------------------------------------------------------------------
// Fuel tanks
//
// The pyro's flamethrower and the fumigator's poison sprayer lying about (Vittorio, 2026-10-23: dropped, thrown, a
// monster's) always have their tank beside them: a prop of its own ("vr_fueltank", "progs/vr_fueltank.mdl#6" red, "#7"
// green: vr_adgunart.cpp), held to the gun by a rope (the engine's, vr_box3d.cpp syncFuelRopes: Box3D's distance joint
// no longer than vr_adgun_tank_rope m) and drawn joined to it by a hose (vr_adtank.cpp, as the back tank's). A hand
// carries the tank (no force grab: it has no FL_FORCEGRABBABLE); either drags the other by the rope. In a hand or a
// holster the gun has no tank lying about: it is the one on the back (vr_adtank.cpp). Dropped by a player, the tank falls
// off his back (spared his body for a second); by a monster, from behind it. The gun gone any way (taken, removed, its
// cap), its tank goes with it (VR_FuelTank_Think); a gun found without one (an older save, a test's) gets one
// (VR_FuelTank_Frame). gun.vr_fuellink is its tank, tank.vr_fuellink its gun. vr_adgun_tank_world 0: none.

// A tank lying about takes damage (Vittorio, 2026-10-23, vrfiringrange_2026-10-23_17-45-47): vr_adgun_tank_health (a
// grunt's 30; 0: it never bursts), shots, blasts, blows, flames. Enough of it bursts it (VR_FuelTank_Burst): with fuel
// in its gun, an explosion as big as the fuel left (its reach vr_adgun_tank_burst_radius at a full tank, 40% of it near
// empty; its blast, burning and poison as much): the flamethrower's sets on fire everything it reaches (monsters,
// corpses, props, the player), the poison sprayer's poisons them (the fumigator's poison); empty, it breaks with a puff
// of smoke. Its gun is spent from then (QVR_WPNFLAG_TANK_BURST, its fuel 0, .vr_depleted: no force grab, no loot
// pillar), a hand still takes it; it never has a tank again (none lying about, none on the back, no hose: vr_adtank.cpp).
// A tank caught in another's burst bursts a moment after (a chain of traps).
.float vr_fueltank_burstat; // when it bursts (a neighbour's burst set it off: VR_FuelTank_Think); 0 not
float vr_fueltank_bursts;   // tanks burst (the tests)
float vr_fueltank_burst_hit; // what the last burst reached (the tests)
void() VR_FuelTank_Burst;
void(entity t, entity xBy) VR_FuelTank_BurstNow;

.float vr_fueltank_grace; // a tank dropped off a player's back: until when it spares his body (.owner him, then its gun)
float vr_fueltank_next;   // VR_FuelTank_Frame's next look
float vr_fueltank_made;   // tanks made (the tests)

float(float xWid) VR_FuelTank_Fuelled = { return xWid == WID_ADPYRO || xWid == WID_ADFUMIGATOR; };

// An armed monster's back tank (Vittorio, 2026-10-23, vrfiringrange_2026-10-23_21-24-32, 21-25-15): a monster holding a
// flamethrower or poison sprayer wears its tank on its back as you do (classname vr_fueltank_worn, the same model; placed
// on its back each frame by its weapon's think, VR_FuelTank_Wear in vr_adtankworn.qc, from its row's back vertices in the
// armed monsters' table; drawn on the drawn body and hosed to the gun by the engine, vr_adtank.cpp). It takes damage as a
// tank lying about does and bursts on the monster (VR_FuelTank_BurstNow): its gun spent and dropped. Its .owner the
// monster, its .vr_fuellink the weapon it holds (vr_ag_weapon).
void(entity xProp) VR_FuelTank_Wear;
float vr_fueltank_worn_made; // worn tanks made (the tests)

// A tank, lying about or worn by a monster (hits on it: metal, never blood: vr_crates.qc VR_Prop_Metal).
float(entity e) VR_FuelTank_Is = { return e.classname == "vr_fueltank" || e.classname == "vr_fueltank_worn"; };

// Its loops stopped (a tank gone, burst or taken: Vittorio, 2026-10-23, its hiss never stopped). Its burst's own sounds
// are its field's (VR_TankField_*), which stops them as it goes.
void(entity t) VR_FuelTank_Hush =
{
    sound(t, CHAN_WEAPON, "misc/null.wav", 0.1, ATTN_NORM);
    sound(t, CHAN_BURN, "misc/null.wav", 0.1, ATTN_NORM);
};

// `g`'s tank lying about (world: none).
entity(entity g) VR_FuelTank_Of =
{
    entity t = g.vr_fuellink;
    if(!t || !t.modelindex || t.classname != "vr_fueltank" || t.vr_fuellink != g)
    {
        return world;
    }
    return t;
};

// `t`'s gun, while it is still its gun lying about (world: not).
entity(entity t) VR_FuelTank_Gun =
{
    entity g = t.vr_fuellink;
    if(!g || !g.modelindex || g.vr_fuellink != t || g.classname != "thrown_weapon" || !VR_FuelTank_Fuelled(g.weapon))
    {
        return world;
    }
    return g;
};

// A worn tank `t`'s gun: the fuelled weapon its wearer holds (world: no longer).
entity(entity t) VR_FuelTank_WornGun =
{
    entity m = t.owner;
    entity p = t.vr_fuellink;
    if(!p || !m || p.classname != "vr_ag_weapon" || p.owner != m || m.vr_ag_prop != p || !VR_FuelTank_Fuelled(p.weapon) ||
       (p.weaponflags & QVR_WPNFLAG_TANK_BURST))
    {
        return world;
    }
    return p;
};
entity(entity g, vector vel) VR_AG_DropHeld; // (vr_armedgrunt.qc: a worn tank's burst drops its gun)

void(float xSpeed) VR_FuelTank_Impact;

// A tank from a save made before it had its hit (none then) gets it.
void(entity t) VR_FuelTank_ImpactFix =
{
    if(t.vr_impact != VR_FuelTank_Impact)
    {
        t.vr_impact = VR_FuelTank_Impact;
        t.throwhit = QVR_THROWHIT_HIT;
    }
};

void() VR_FuelTank_Think =
{
    VR_FuelTank_ImpactFix(self);
    entity g = VR_FuelTank_Gun(self);
    if(!g || !cvar("vr_adgun_tank_world"))
    {
        if(g)
        {
            g.vr_fuellink = world;
        }
        if(cvar("developer"))
        {
            dprint(sprintf("adgun: a tank at %v goes with its gun\n", self.origin));
        }
        VR_FuelTank_Hush(self);
        remove(self);
        return;
    }
    if(self.vr_fueltank_grace && time >= self.vr_fueltank_grace)
    {
        self.vr_fueltank_grace = 0;
        self.owner = g; // (it meets its dropper again; never its own gun: the rope holds them)
    }
    if(self.vr_fueltank_burstat && time >= self.vr_fueltank_burstat)
    {
        VR_FuelTank_BurstNow(self, self.enemy); // (set off by a neighbour's burst)
        return;
    }
    self.nextthink = time + 0.01;
};

// A hit that leaves it whole: a few sparks off its metal.
void(entity attacker, float damage) VR_FuelTank_Pain =
{
    particle2(self.origin, '0 0 0', QVR_PARTICLE_PRESET_CONTACTSPARKS, 2);
};

// Its Box3D hit (.vr_impact, at `xSpeed` m/s; Vittorio, 2026-10-25: "a thrown fuel tank deals no damage to enemies": it
// had no hit at all, its touch SUB_Null): as a crate's or an explosive box's (vr_crates.qc VR_SolidProp_Impact): thrown
// hard into a monster it hurts it as a thrown prop of its mass does (VR_Thrown_Damage), flung into one (on its rope,
// batted) as a flung prop (VR_Prop_Flung). Box3D's speed of the hit, not the touch's (the touch, after the step, saw an
// ogre's bounce: half the speed). Its own hits never hurt it, on a monster no more than on a wall: shots, blasts, blows
// and flames burst it, not a throw. Dropped off a player's back, it still spares his body (.owner him) until its grace
// is over, whatever it lands on.
void(float xSpeed) VR_FuelTank_Impact =
{
    entity was = self.owner;
    float spare = self.vr_fueltank_grace > 0 && self.throwhit != QVR_THROWHIT_NEVER_HIT;
    VR_SolidProp_Impact(xSpeed);
    if(spare && self.owner != was)
    {
        self.owner = was;
    }
};

// `xGun`, a gun lying about, given its tank if it is a fuelled one without one: off `xFrom`'s back (a player dropping it:
// where his back tank was; a monster: behind it), else beside the gun.
void(entity xGun, entity xFrom) VR_FuelTank_Attach =
{
    if(!xGun || xGun.classname != "thrown_weapon" || !VR_FuelTank_Fuelled(xGun.weapon) || !cvar("vr_adgun_tank_world") ||
       VR_FuelTank_Of(xGun) || (xGun.weaponflags & QVR_WPNFLAG_TANK_BURST))
    {
        return; // (a gun whose tank burst never has one again)
    }
    entity t = spawn();
    t.classname = "vr_fueltank";
    t.netname = "fuel tank";
    t.movetype = MOVETYPE_TOSS;
    t.solid = SOLID_NOT_BUT_TOUCHABLE;
    setmodel(t, xGun.weapon == WID_ADPYRO ? "progs/vr_fueltank.mdl#6" : "progs/vr_fueltank.mdl#7");
    setsize(t, modelbounds(t, 0), modelbounds(t, 1));
    vector from = xGun.origin, at;
    t.owner = xGun;
    if(xFrom && xFrom != xGun && (xFrom.flags & (FL_CLIENT | FL_MONSTER)))
    {
        makevectors('0 1 0' * xFrom.angles_y);
        from = xFrom.origin;
        at = from - v_forward * ((xFrom.flags & FL_CLIENT) ? 10 : 14) + '0 0 6';
        t.velocity = xFrom.velocity * ((xFrom.flags & FL_CLIENT) ? 1 : 0.5);
        t.angles = '0 1 0' * xFrom.angles_y;
        if(xFrom.flags & FL_CLIENT)
        {
            t.owner = xFrom; // (out of his body before it meets it)
            t.vr_fueltank_grace = time + 1;
        }
    }
    else
    {
        makevectors('0 1 0' * xGun.angles_y);
        at = xGun.origin + v_right * 14 + '0 0 4';
        t.velocity = xGun.velocity;
        t.angles = '90 0 0' + '0 1 0' * xGun.angles_y; // (lying on its side)
    }
    traceline(from, at, TRUE, xGun);
    if(trace_fraction < 1)
    {
        at = from + (trace_endpos - from) * 0.7; // (short of a wall)
    }
    setorigin(t, at);
    t.vr_fuellink = xGun;
    xGun.vr_fuellink = t;
    t.vr_prop_mass = max(0.2, cvar("vr_adgun_tank_mass"));
    t.touch = SUB_Null;
    t.vr_impact = VR_FuelTank_Impact;
    t.throwhit = QVR_THROWHIT_HIT; // (not thrown yet: QVR_THROWHIT_NEVER_HIT, 0, is a throw in flight)
    t.handtouch = SUB_Null; // (nothing to take: a hand carries it, VR_Carry_Setup)
    VR_Carry_Setup(t);
    t.think = VR_FuelTank_Think;
    t.nextthink = time + 0.01;
    float hp = cvar("vr_adgun_tank_health");
    if(hp > 0)
    {
        t.takedamage = DAMAGE_YES; // (shots and missiles stop at it: a touchable that takes damage, the engine's MOVE_HITGIBS)
        t.health = hp;
        t.max_health = hp;
        t.th_die = VR_FuelTank_Burst;
        t.th_pain = VR_FuelTank_Pain;
    }
    vr_fueltank_made++;
    if(cvar("developer"))
    {
        dprint(sprintf("adgun: %s's tank made at %v (%s)\n", xGun.netname, t.origin,
            xFrom.classname != "" ? xFrom.classname : "beside it"));
    }
};

void() VR_FuelTank_Frame =
{
    if(time < vr_fueltank_next)
    {
        return;
    }
    vr_fueltank_next = time + 0.5;
    if(!cvar("vr_adgun_tank_world"))
    {
        return;
    }
    for(entity g = find(world, classname, "thrown_weapon"); g; g = find(g, classname, "thrown_weapon"))
    {
        if(VR_FuelTank_Fuelled(g.weapon) && !VR_FuelTank_Of(g))
        {
            VR_FuelTank_Attach(g, world);
        }
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Cutting the hose
//
// Vittorio, 2026-10-24: a blade swung across the hose between a gun and its tank lying about (a sword's, an axe's, a
// knife's, the chainsaw's bar; not a fist, a club, the hammer or mace, the crowbar, a gun, nor a pommel or a hilt) cuts
// it: the blade proper's swept points (VR_Melee_Sweep's lines, vr_rib_*) passing within their reach of the hose as the
// engine draws it (hosecross; vr_adtank.cpp), at vr_adgun_hosecut_speed (m/s) or faster, across the blade (a stab along
// it doesn't). The gun is spent from then, as a burst tank's (QVR_WPNFLAG_TANK_BURST, its fuel 0, .vr_depleted: it never
// fires, no tank again); the tank a prop of its own (force grabbed, carried, thrown, shot: it bursts as big as the fuel
// it kept, .vr_fueltank_fuel, its field as a tank's). Both keep a stub of the hose at the cut (.vr_hosecut, its time:
// the engine draws a short free end at the gun's inlet and the tank's outlet). A hiss and a snap, sparks and a puff of
// fuel vapour or poison where it was cut (no field). Only a hose lying about: a monster's worn tank's isn't cut, and in
// your hands the gun's tank is on your back. vr_adgun_hosecut 0: never.
.float vr_hosecut;        // a gun lying about whose hose was cut, the tank cut free of it: when (0: not)
.float vr_fueltank_fuel;  // a tank cut free of its gun: the fuel it kept, a share of a full tank
float vr_hosecut_count;   // hoses cut (the tests)
float vr_hosecut_refused; // cuts refused for the blade's speed (the tests)
float vr_hosecut_speed_test; // the tests: the blade's speed at the hose, m/s (0: the hand's)
entity vr_hosecut_tank;   // the last tank cut free (the tests)
entity vr_hosecut_gun;    // and its gun
float VR_HOSECUT_ALONG = 0.87; // cosine: the blade moving within 30 degrees of its own line is a stab, not a cut

// A tank cut free: it bursts when set off by a neighbour's burst (VR_FuelTank_Think's, without its gun).
void() VR_FuelTank_FreeThink =
{
    VR_FuelTank_ImpactFix(self);
    if(self.vr_fueltank_burstat && time >= self.vr_fueltank_burstat)
    {
        VR_FuelTank_BurstNow(self, self.enemy);
        return;
    }
    self.nextthink = time + 0.05;
};

// The hose between gun `g` and its tank `t` lying about cut at `xAt` by `xBy`.
void(entity g, entity t, vector xAt, entity xBy) VR_HoseCut_Cut =
{
    float full = WeaponIdToBaseClipSize(g.weapon);
    float fuel = g.weaponinst ? g.weaponinst.wi_clip : 0;
    float poison = g.weapon == WID_ADFUMIGATOR;
    t.vr_fueltank_fuel = full > 0 ? clamp(fuel / full, 0, 1) : 0;
    // The gun spent for good, as a burst tank's: no tank, no fuel, no force grab, no loot pillar (a hand still takes it).
    g.weaponflags = g.weaponflags | QVR_WPNFLAG_TANK_BURST;
    if(g.weaponinst)
    {
        g.weaponinst.wi_clip = 0;
    }
    g.vr_depleted = TRUE;
    g.vr_fuellink = world;
    g.vr_hosecut = time;
    // The tank a prop of its own: no rope (the engine's goes with the link), meeting its gun again, force grabbed.
    t.vr_fuellink = world;
    t.owner = world;
    t.vr_fueltank_grace = 0;
    t.vr_hosecut = time;
    t.flags = t.flags | FL_FORCEGRABBABLE;
    t.think = VR_FuelTank_FreeThink;
    t.nextthink = time + 0.05;
    // A hiss (the stream's end) and a snap; sparks and a puff of fuel vapour or poison.
    sound(t, CHAN_WEAPON, poison ? "enforcer/fum_poisonoff.wav" : "enforcer/pyro_flameoff.wav", 0.7, ATTN_NORM);
    sound(g, CHAN_AUTO, "weapons/tink1.wav", 0.6, ATTN_NORM);
    particle2(xAt, '0 0 0', QVR_PARTICLE_PRESET_CONTACTSPARKS, 3);
    if(poison)
    {
        particle2(xAt, '0 0 0', QVR_PARTICLE_PRESET_POISONPUFF, 1);
    }
    else
    {
        particle2(xAt, '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 3);
    }
    vr_hosecut_count++;
    vr_hosecut_tank = t;
    vr_hosecut_gun = g;
    if(cvar("developer"))
    {
        dprint(sprintf("adgun: %s's hose cut at %v by %s, its tank free with %.2f of its fuel\n", g.netname, xAt,
            xBy.classname, t.vr_fueltank_fuel));
    }
};

// Whether `b`'s swept lines (the blade proper's, from `xFrom` of the way from the grip to the tip) met the hose of tank
// `t` to gun `g`: then cut, if fast enough across the blade (else refused). TRUE: met (cut or refused).
float(entity b, entity t, entity g, float xFrom) VR_HoseCut_Ribs =
{
    vector line = b.mh_far - b.mh_grip;
    float len2 = line * line;
    float least = max(0, cvar("vr_adgun_hosecut_speed"));
    for(float k = 0; k < vr_rib_n; ++k)
    {
        float i = vr_rib_i[k];
        vector p0 = vr_rib_p0[k];
        vector p1 = vr_rib_p1[k];
        float f = ((p1 - b.mh_grip) * line) / len2; // (the grip 0, the tip 1)
        if(vr_mpt_kind[i] != VR_MPT_EDGE || p0 == p1 || f < xFrom)
        {
            continue; // (the blade proper, an axe's head, the saw's bar)
        }
        if(hitmodel_segment(t, p0, p1, vr_rib_r[k], -1) < 0)
        {
            continue;
        }
        vector at = trace_endpos;
        vector vp = VR_Melee_PointVel(b, min(1, f));
        float speed = vlen(vp);
        vector axis = normalize(b.mh_rfar - b.mh_rgrip);
        float along = speed > 0 ? fabs(vp * axis) / speed : 0;
        if(vr_hosecut_speed_test > 0)
        {
            speed = vr_hosecut_speed_test;
            along = 0;
        }
        if(speed < least || along > VR_HOSECUT_ALONG)
        {
            vr_hosecut_refused++;
            if(cvar("developer"))
            {
                dprint(sprintf("adgun: %s's hose not cut: %.1f m/s, %.2f along the blade\n", g.netname, speed, along));
            }
            return TRUE;
        }
        VR_HoseCut_Cut(g, t, at, self);
        return TRUE;
    }
    return FALSE;
};

// What `b` (hand `xHand`'s blow, VR_Melee_Sweep's lines just made: vr_rib_*) holds cuts a hose lying about it swept
// across (the first met), at its speed there. VR_Melee_Strike's, after the sweep.
void(float xHand, entity b) VR_HoseCut_Melee =
{
    float thing = b.mh_thing;
    if(b.mh_free || b.mh_n <= 0 || vr_rib_n <= 0 || (thing != VR_MTHING_SWORD && thing != VR_MTHING_AXE && thing != VR_MTHING_SAW) ||
       (thing == VR_MTHING_AXE && b.mh_wid != WID_AXE && b.mh_wid != WID_SUPERAXE))
    {
        return; // (not a blade: a fist, a club, a gun, the crowbar, Mjolnir, the hammer, the mace; one held off its handle)
    }
    if(!cvar("vr_adgun_hosecut"))
    {
        return;
    }
    vector line = b.mh_far - b.mh_grip;
    float len2 = line * line;
    if(len2 < 1)
    {
        return;
    }
    // The tanks lying about with their gun whose pair's box (widened by its rope) the swept lines' box meets.
    vector lo = vr_rib_lo - '1 1 1' * (vr_rib_rmax + 2);
    vector hi = vr_rib_hi + '1 1 1' * (vr_rib_rmax + 2);
    float slack = max(0.4, cvar("vr_adgun_tank_rope")) * VRMetersToUnits() * 0.6;
    float from = thing == VR_MTHING_SAW ? VR_SAW_BAR_START : (thing == VR_MTHING_AXE ? VR_DECAP_BLADE_AXE : VR_DECAP_BLADE_SWORD);
    for(entity e = find(world, classname, "vr_fueltank"); e; e = find(e, classname, "vr_fueltank"))
    {
        entity eg = VR_FuelTank_Gun(e);
        if(!eg)
        {
            continue;
        }
        vector plo = '0 0 0';
        vector phi = '0 0 0';
        plo_x = min(e.origin_x, eg.origin_x) - slack;
        plo_y = min(e.origin_y, eg.origin_y) - slack;
        plo_z = min(e.origin_z, eg.origin_z) - slack;
        phi_x = max(e.origin_x, eg.origin_x) + slack;
        phi_y = max(e.origin_y, eg.origin_y) + slack;
        phi_z = max(e.origin_z, eg.origin_z) + slack;
        if(plo_x <= hi_x && plo_y <= hi_y && plo_z <= hi_z && phi_x >= lo_x && phi_y >= lo_y && phi_z >= lo_z)
        {
            if(VR_HoseCut_Ribs(b, e, eg, from))
            {
                return;
            }
        }
    }
};

// Freddie's gun arm: his rig's left forearm (vr_ad_rigs.inc adFreddieSeeds: bone 4, forearm_l), the super nailgun's
// five barrels on it (mon_freddie.mdl vertices 88..182: the engine leaves them off his limbs' models, vr_limbmodel.cpp).
float VR_AD_FREDDIE_GUNBONE = 4;
float vr_adgun_freddie_drops; // the super nailguns his cut arms or gibbed bodies dropped (the tests)
.float vr_adgun_freddie_off;  // his gun arm cut off or his body gibbed: its super nailgun gone (dropped once)

// `e`'s limb just cut off or popped (VR_Limb_Cut; `xTakes` the bones it took): Freddie's gun arm (the forearm, or the
// whole arm) drops the super nailgun on it as Quake VR's, loaded as a dropped enemy gun is (VR_DropEnemyGun: a full
// clip), where the piece was, turned and moving as it (his gun's barrels along his model's +x, as the nailgun's).
void(entity e, float xTakes) VR_ADGun_LimbCut =
{
    if(e.ad_classtype != ad_CT_MONFREDDIE || !(xTakes & VR_Limb_Bit(VR_AD_FREDDIE_GUNBONE)) || e.vr_adgun_freddie_off)
    {
        return;
    }
    e.vr_adgun_freddie_off = TRUE;
    if(!cvar("vr_adgun_drops"))
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

// Self gibbed by AD's code (VR_AD_Gibbed: killed past his gib health, or his body gibbed later): Freddie's super nailgun,
// unless his gun arm was cut off before (it dropped then), thrown from where his left forearm was, up and out to his
// left, loaded as VR_ADGun_LimbCut's.
void() VR_ADGun_FreddieGibbed =
{
    if(self.ad_classtype != ad_CT_MONFREDDIE || self.vr_adgun_freddie_off)
    {
        return;
    }
    self.vr_adgun_freddie_off = TRUE;
    if(!cvar("vr_adgun_drops"))
    {
        return;
    }
    makevectors('0 1 0' * self.angles_y);
    vector from = self.origin + '0 0 24';
    vector at = from + v_forward * 16 - v_right * 24;
    traceline(from, at, TRUE, self);
    at = trace_endpos + (from - trace_endpos) * 0.2 * (trace_fraction < 1);
    vector vel = v_forward * 40 - v_right * 120 + randomVec(40) + '0 0 200';
    entity g = CreateThrownWeapon(self, WID_SUPER_NAILGUN, 0, WeaponInst_New(WeaponIdToBaseClipSize(WID_SUPER_NAILGUN)),
        at, '0 1 0' * self.angles_y, vel, randomVec(120),
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(WID_SUPER_NAILGUN));
    if(g != world)
    {
        vr_adgun_freddie_drops++;
    }
    dprint(sprintf("adgun: %s gibbed: a super nailgun (%g in it) at %v\n", self.classname,
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

// A burst's field (Vittorio, 2026-10-23, vrfiringrange_2026-10-23_21-25-15): where a tank burst with fuel in it, a field
// lingers (vr_adgun_tank_field_time at a full tank, 40% of it near empty): flames on the ground (the flamethrower's)
// setting on fire everything that enters it (monsters, players, corpses, crates: its burning as the burst's, and the
// flames' own vr_burn_damage a second on the living, times vr_adgun_tank_field_damage), or a green cloud (the sprayer's)
// poisoning everything that enters it (the burst's poison a second times vr_adgun_tank_field_damage, for its time). Its
// reach vr_adgun_tank_field_radius at a full tank (40% near empty). Its look and its sound (a fire's crackle, a hiss)
// fade out with it over its second half. It plays the burst's sounds too, and stops every loop of them as it goes
// (they never outlive it: the hiss that never stopped). A burst with no field (time 0) still has one, silent and empty,
// for its sounds' 0.7 s. It spreads from the tank out to its reach over vr_adgun_tank_field_spread (VR_TankField_Reach),
// its look and its damage together; its look: VR_TankField_Look, its 3D flames VR_TankField_Flames (2026-10-24).
float VR_TANKFIELD_TICK = 0.5;   // s between its damages
float VR_TANKFIELD_HUSH = 0.7;   // s the burst's whoosh plays (its "off" then: the loop's end)
.float vr_tf_kind;   // 1 fire, 2 poison
.float vr_tf_start;  // when it began,
.float vr_tf_end;    // ... when it goes
.float vr_tf_radius; // its reach (units)
.float vr_tf_s;      // the fuel's share of the burst (0.4 .. 1): its burning's and poison's
.float vr_tf_level;  // its loop's volume step now (0..4; 0 silent)
.float vr_tf_tick;   // its next damage
.float vr_tf_hush;   // when the burst's whoosh stops (0: stopped)
.float vr_tf_spread; // s it takes to spread from the tank out to its reach (vr_adgun_tank_field_spread as it was made)
.float vr_tf_looked; // its last look's time (VR_TankField_Look: as many as the time since)
float vr_tankfield_made;  // fields made (the tests)
float vr_tankfield_hits;  // what the fields reached, tick by tick (the tests)
float vr_tankfield_loops; // fields' loops playing now (the tests: 0 once they are gone)
float vr_tankfield_whoosh; // bursts' whooshes playing now (the tests: 0 a moment after each)

// How strong it is now: 1 over its first half, fading to 0 over the second.
float(entity f) VR_TankField_Strength =
{
    float life = max(0.01, f.vr_tf_end - f.vr_tf_start);
    return clamp((f.vr_tf_end - time) / (life * 0.5), 0, 1);
};

// Its reach now (Vittorio, 2026-10-24, vrtesthall_2026-10-24_01-05-18: not popping into its whole circle at once): from
// the tank out to its full reach over its spread time, quick at first and slowing (its look and its damage together).
float(entity f) VR_TankField_Reach =
{
    if(f.vr_tf_spread <= 0)
    {
        return f.vr_tf_radius;
    }
    float k = clamp((time - f.vr_tf_start) / f.vr_tf_spread, 0, 1);
    return max(8, f.vr_tf_radius * (1 - (1 - k) * (1 - k)));
};

// Its loop at volume step `xLevel` (0..4: silent .. full), started again only when the step changes.
void(entity f, float xLevel) VR_TankField_Loop =
{
    if(xLevel == f.vr_tf_level)
    {
        return;
    }
    string loop = f.vr_tf_kind == 1 ? "ambience/fire1.wav" : (cvar("vr_ad_mounted") ? "traps/steam_loop.wav" : "");
    if(xLevel > 0 && loop != "")
    {
        sound(f, CHAN_BURN, loop, (f.vr_tf_kind == 1 ? 0.9 : 0.6) * xLevel / 4, ATTN_NORM);
        vr_tankfield_loops += (f.vr_tf_level <= 0);
    }
    else if(f.vr_tf_level > 0)
    {
        sound(f, CHAN_BURN, "misc/null.wav", 0.1, ATTN_NORM);
        vr_tankfield_loops = max(0, vr_tankfield_loops - 1);
    }
    f.vr_tf_level = loop != "" ? xLevel : 0;
};

// The burst's whoosh stopped (its "off": the loop's end, its tail).
void(entity f) VR_TankField_Unwhoosh =
{
    if(!f.vr_tf_hush)
    {
        return;
    }
    f.vr_tf_hush = 0;
    if(cvar("vr_ad_mounted"))
    {
        sound(f, CHAN_WEAPON, f.vr_tf_kind == 2 ? "enforcer/fum_poisonoff.wav" : "enforcer/pyro_flameoff.wav", 0.6, ATTN_NORM);
    }
    vr_tankfield_whoosh = max(0, vr_tankfield_whoosh - 1);
};

// What it reaches now: in its circle, its body near the ground there (fire 48 units up, a cloud 72), in sight of it.
void(entity f) VR_TankField_Damage =
{
    float fire = f.vr_tf_kind == 1;
    float scale = max(0, cvar("vr_adgun_tank_field_damage"));
    float top = f.origin_z + (fire ? 48 : 72);
    float burn = max(0, cvar("vr_adgun_tank_burst_burn")) * f.vr_tf_s;
    float dps = max(0, cvar("vr_adgun_tank_burst_poison")) * f.vr_tf_s * scale;
    float ptime = max(0, cvar("vr_adgun_tank_burst_poisontime")) * f.vr_tf_s;
    vector eye = f.origin + '0 0 16';
    float reach = VR_TankField_Reach(f);
    for(entity e = findradius(f.origin, reach + 24); e; e = e.chain)
    {
        if(e == f || (!e.takedamage && !(fire && VR_Burn_Can(e))) || e.absmin_z > top || e.absmax_z < f.origin_z - 12)
        {
            continue;
        }
        vector mid = e.origin + (e.mins + e.maxs) * 0.5;
        vector flat = mid - f.origin;
        flat_z = 0;
        if(vlen(flat) > reach + max(e.maxs_x, 8))
        {
            continue; // (outside its circle: findradius's ball is wider)
        }
        traceline(eye, mid, TRUE, f);
        if(trace_fraction < 1 && vlen(trace_endpos - mid) > 24)
        {
            continue; // (behind a wall)
        }
        entity by = e == f.owner ? world : f.owner; // (the one who burst it: as anyone, not by himself)
        if(VR_FuelTank_Is(e))
        {
            if(fire && !e.vr_fueltank_burstat && e.takedamage)
            {
                e.vr_fueltank_burstat = time + 0.15 + random() * 0.15; // (another tank in the flames: it bursts too)
                e.enemy = by;
            }
            continue;
        }
        vr_tankfield_hits++;
        vector at = mid;
        at_z = max(e.absmin_z + 4, f.origin_z + 4); // (its feet in the flames)
        if(!fire)
        {
            VR_ADGun_Poison(e, f, by, dps, ptime);
            continue;
        }
        if(pointcontents(at) <= CONTENT_WATER)
        {
            continue;
        }
        vr_burn_len = burn;
        VR_Burn_Ignite(e, by, at, '0 0 1', VR_BURN_TOUCH);
        vr_burn_len = 0;
        float d = cvar_hget(cvarh_vr_burn_damage) * VR_TANKFIELD_TICK * scale;
        if(d > 0 && e.takedamage && e.health > 0 && (e.flags & (FL_CLIENT | FL_MONSTER)))
        {
            VR_SetHitKind(QVR_HITKIND_NONE, -1, at, '0 0 1');
            T_Damage(e, f, by, d);
            VR_ClearHitKind();
        }
    }
};

// A spot on the ground in its circle out to `xReach`, in sight of its middle (not through a wall), not past a ledge:
// vr_tankfield_spot, vr_tankfield_out its share of the way out (0 the middle, 1 the edge). FALSE: none this time.
vector vr_tankfield_spot;
float vr_tankfield_out;
float vr_tankfield_fx_fire;  // its look's effects sent (the tests): fire, smoke, the cloud's puffs
float vr_tankfield_fx_smoke;
float vr_tankfield_fx_cloud;
float(entity f, float xReach) VR_TankField_Spot =
{
    float a = random() * 6.2831853;
    vr_tankfield_out = sqrt(random());
    float r = vr_tankfield_out * xReach;
    vector p = f.origin + '1 0 0' * (cos(a) * r) + '0 1 0' * (sin(a) * r);
    traceline(f.origin + '0 0 16', p + '0 0 16', TRUE, f);
    if(trace_fraction < 1)
    {
        return FALSE; // (a wall in the way: not through it)
    }
    traceline(p + '0 0 16', p - '0 0 48', TRUE, f);
    if(trace_fraction >= 1)
    {
        return FALSE; // (no ground there: a ledge)
    }
    vr_tankfield_spot = trace_endpos + '0 0 1';
    return TRUE;
};

// `x` rounded up or down at random (its fraction's share of the time up): a few a second from a fraction a think.
float(float x) VR_TankField_SomeOf = { return floor(max(0, x) + random()); };

// Its look over the last `xDt` s (Vittorio, 2026-10-24, vrtesthall_2026-10-24_01-04-12, 01-05-18: a strong area denial):
// spots through its circle as far as it reaches now (VR_TankField_Reach), as many as its area. The fire: tongues of fire
// rising straight up off the ground (vr_adgun_tank_field_flames), dark smoke rising off their tops
// (vr_adgun_tank_field_smoke); the flames thinning and shrinking as it fades. The poison: a big green cloud
// (vr_adgun_tank_field_cloud) low over it, thinner at its edge and as it fades, thick at once as it spreads (a smoke
// bomb's), with a few green wisps of the sprayer's flame low in it. Its 3D flames are their own (VR_TankFlame_*).
void(entity f, float xStrength, float xDt) VR_TankField_Look =
{
    float reach = VR_TankField_Reach(f);
    float area = clamp(reach * reach / 1600, 0.25, 9); // (a full tank's 96: 5.8)
    float per = xDt / 0.1;
    float i = 0, n = 0;
    vector look = '1 0 0' + '0 1 0' * (0.55 + 0.45 * xStrength) + '0 0 1' * (0.3 + 0.7 * xStrength);
    if(f.vr_tf_kind == 1)
    {
        n = VR_TankField_SomeOf(max(0, cvar("vr_adgun_tank_field_flames")) * area * 3 * per * (0.25 + 0.75 * xStrength));
        for(i = 0; i < n; i++)
        {
            if(VR_TankField_Spot(f, reach) && (pointcontents(vr_tankfield_spot + '0 0 4') > CONTENT_WATER))
            {
                particle2(vr_tankfield_spot, look, QVR_PARTICLE_PRESET_TANKFIRE, 2 + floor(2 * xStrength * random() + 0.5));
                vr_tankfield_fx_fire++;
            }
        }
        n = VR_TankField_SomeOf(max(0, cvar("vr_adgun_tank_field_smoke")) * clamp(area * 0.45, 0.5, 4) * per * xStrength);
        for(i = 0; i < n; i++)
        {
            if(VR_TankField_Spot(f, reach) && (pointcontents(vr_tankfield_spot + '0 0 4') > CONTENT_WATER))
            {
                particle2(vr_tankfield_spot, look, QVR_PARTICLE_PRESET_TANKSMOKE, 1);
                vr_tankfield_fx_smoke++;
            }
        }
        return;
    }
    float burst = (time - f.vr_tf_start < f.vr_tf_spread + 0.3) ? 4 : 1; // (thick at once: a smoke bomb going off)
    n = VR_TankField_SomeOf(max(0, cvar("vr_adgun_tank_field_cloud")) * clamp(area, 0.5, 9) * per * burst);
    for(i = 0; i < n; i++)
    {
        if(VR_TankField_Spot(f, reach))
        {
            float edge = clamp((1 - vr_tankfield_out) * 3, 0.3, 1); // (thinner toward its edge)
            particle2(vr_tankfield_spot, '1.5 0 0' + '0 1 0' * (0.7 + 0.3 * reach / max(1, f.vr_tf_radius)) +
                '0 0 1' * (edge * xStrength), QVR_PARTICLE_PRESET_TANKCLOUD, 1);
            vr_tankfield_fx_cloud++;
        }
    }
    n = VR_TankField_SomeOf(max(0, cvar("vr_adgun_tank_field_flames")) * area * 0.5 * per * xStrength);
    for(i = 0; i < n; i++)
    {
        if(VR_TankField_Spot(f, reach))
        {
            particle2(vr_tankfield_spot + '0 0 4', '0 0 0.15' + randomVec(0.08), QVR_PARTICLE_PRESET_ADPOISON, 1);
        }
    }
};

// Its 3D flames (vrtesthall_2026-10-24_01-04-12): id's large torch flame (progs/flame2.mdl's second frame, as the
// burning bodies' and the torches'), each thrown out from the tank to a random spot in its circle over its spread time,
// large on the way and settling to its size there; it burns on, then fades out and shrinks, each at its own time over
// the field's life (the last before the field fades out). The first lights its place as a torch does (vr_emissive.cpp:
// drawn with no alpha); the others are drawn opaque, no light of their own.
.vector vr_tfm_spot;   // where it goes
.float vr_tfm_born;    // when it was thrown
.float vr_tfm_arrive;  // when it is there
.float vr_tfm_fade;    // when it starts fading out,
.float vr_tfm_end;     // ... gone
.float vr_tfm_size;    // its scale there
float vr_tankflame_made; // (the tests)

void() VR_TankFlame_Think =
{
    if(time >= self.vr_tfm_end)
    {
        remove(self);
        return;
    }
    float s = self.vr_tfm_size;
    if(time < self.vr_tfm_arrive)
    {
        float u = clamp((time - self.vr_tfm_born) / max(0.01, self.vr_tfm_arrive - self.vr_tfm_born), 0, 1);
        s = s * (1.6 - 0.6 * u * u); // (large on its way out)
    }
    else if(self.velocity != '0 0 0')
    {
        self.velocity = '0 0 0';
        setorigin(self, self.vr_tfm_spot);
    }
    if(time > self.vr_tfm_fade)
    {
        float v = clamp((self.vr_tfm_end - time) / max(0.01, self.vr_tfm_end - self.vr_tfm_fade), 0, 1);
        self.alpha = max(0.03, v);
        s = s * (0.45 + 0.55 * v);
    }
    self.scale = s;
    self.nextthink = time + 0.05;
};

// The fire field `f`'s 3D flames (vr_adgun_tank_field_models at a full tank, the fuel's share `s` of it), its life `xLife`.
void(entity f, float s, float xLife) VR_TankField_Flames =
{
    float n = floor(max(0, cvar("vr_adgun_tank_field_models")) * s + 0.5);
    float spread = max(0.05, f.vr_tf_spread);
    float lit = FALSE;
    for(float i = 0; i < n; i++)
    {
        float tries = 0;
        float ok = FALSE;
        while(!ok && tries < 4)
        {
            ok = (VR_TankField_Spot(f, f.vr_tf_radius * 0.9) && (pointcontents(vr_tankfield_spot + '0 0 4') > CONTENT_WATER));
            tries++;
        }
        if(!ok)
        {
            continue;
        }
        entity m = spawn();
        m.classname = "vr_tankflame";
        m.owner = f;
        m.movetype = MOVETYPE_NOCLIP;
        m.solid = SOLID_NOT;
        setmodel(m, "progs/flame2.mdl");
        setsize(m, '0 0 0', '0 0 0');
        m.frame = 1; // (the large flame)
        m.angles_y = random() * 360;
        setorigin(m, f.origin);
        m.vr_tfm_spot = vr_tankfield_spot;
        m.vr_tfm_born = time;
        m.vr_tfm_arrive = time + spread * (0.6 + 0.4 * vr_tankfield_out);
        m.velocity = (m.vr_tfm_spot - f.origin) * (1 / (m.vr_tfm_arrive - time));
        m.vr_tfm_end = time + xLife * (0.55 + 0.4 * random());
        m.vr_tfm_fade = m.vr_tfm_end - min(1.2, xLife * 0.25);
        m.vr_tfm_size = (0.75 + 0.45 * random()) * (0.7 + 0.3 * s);
        m.scale = m.vr_tfm_size * 1.6;
        m.alpha = lit ? 1 : 0; // (the first lights its place)
        lit = TRUE;
        m.think = VR_TankFlame_Think;
        m.nextthink = time + 0.05;
        vr_tankflame_made++;
    }
};

void() VR_TankField_Think =
{
    if(self.vr_tf_hush && time >= self.vr_tf_hush)
    {
        VR_TankField_Unwhoosh(self);
    }
    if(time >= self.vr_tf_end)
    {
        VR_TankField_Loop(self, 0);
        if(!self.vr_tf_hush)
        {
            if(cvar("developer"))
            {
                dprint(sprintf("adgun: a tank's %s field at %v gone after %.1f s\n", self.vr_tf_kind == 1 ? "fire" : "poison",
                    self.origin, time - self.vr_tf_start));
            }
            remove(self);
            return;
        }
        self.nextthink = self.vr_tf_hush;
        return;
    }
    float k = VR_TankField_Strength(self);
    VR_TankField_Loop(self, ceil(k * 4));
    VR_TankField_Look(self, k, clamp(time - self.vr_tf_looked, 0.01, 0.2));
    self.vr_tf_looked = time;
    if(time >= self.vr_tf_tick)
    {
        self.vr_tf_tick = time + VR_TANKFIELD_TICK;
        VR_TankField_Damage(self);
    }
    self.nextthink = time + (time - self.vr_tf_start < self.vr_tf_spread ? 0.05 : 0.1); // (spreading: smoothly)
};

// The field of a tank burst at `at` (its fuel's share `s`: 0.4 .. 1; `xFuel` 0: empty, no field), `xBy` who burst it:
// it plays the burst's sounds (the whoosh, the blast) and lingers there as long as its time.
entity(vector at, float poison, float s, float xFuel, entity xBy) VR_TankField_Make =
{
    entity f = spawn();
    f.classname = "vr_tankfield";
    f.movetype = MOVETYPE_NONE;
    f.solid = SOLID_NOT;
    traceline(at, at - '0 0 1024', TRUE, world);
    setorigin(f, trace_endpos + '0 0 1'); // (on the ground under it: a tank thrown up by another's blast too)
    f.owner = xBy;
    f.vr_tf_kind = poison ? 2 : 1;
    f.vr_tf_s = s;
    f.vr_tf_start = time;
    float life = xFuel > 0 ? max(0, cvar("vr_adgun_tank_field_time")) * s : 0;
    f.vr_tf_end = time + life;
    f.vr_tf_radius = max(16, cvar("vr_adgun_tank_field_radius")) * s;
    f.vr_tf_tick = time + 0.2; // (the burst's own reach first)
    f.vr_tf_spread = max(0, cvar("vr_adgun_tank_field_spread"));
    f.vr_tf_looked = time;
    f.think = VR_TankField_Think;
    f.nextthink = time + (f.vr_tf_spread > 0 ? 0.05 : 0.1);
    if(xFuel > 0)
    {
        if(cvar("vr_ad_mounted"))
        {
            AD_WeaponsOnDemand(); // (the whoosh, its end, the hiss: precached as a burst comes, any map)
            sound(f, CHAN_WEAPON, poison ? "enforcer/fum_poisonon.wav" : "enforcer/pyro_flameon.wav", 1, ATTN_NORM);
            f.vr_tf_hush = time + VR_TANKFIELD_HUSH;
            vr_tankfield_whoosh++;
        }
        sound(f, CHAN_BODY, "weapons/r_exp3.wav", 0.4 + 0.6 * (s - 0.4) / 0.6, ATTN_NORM);
    }
    if(life > 0)
    {
        vr_tankfield_made++;
        VR_TankField_Loop(f, 4);
        VR_TankField_Look(f, 1, 0.05); // (its first flames or puffs at the tank at once)
        if(!poison)
        {
            VR_TankField_Flames(f, s, life);
        }
        if(cvar("developer"))
        {
            dprint(sprintf("adgun: a tank's %s field at %v: %.1f s, reach %.0f\n", poison ? "poison" : "fire", f.origin, life,
                f.vr_tf_radius));
        }
    }
    else if(!f.vr_tf_hush)
    {
        f.nextthink = time + 0.1; // (nothing to play: gone at its first think)
    }
    return f;
};

// Fuel tanks bursting (above, "Fuel tanks"). Self: a tank its damage killed (Killed: .enemy who did it).
void() VR_FuelTank_Burst =
{
    VR_FuelTank_BurstNow(self, self.enemy);
};

// Tank `t` bursts now, `xBy` the one who did it (world: none known).
void(entity t, entity xBy) VR_FuelTank_BurstNow =
{
    if(!VR_FuelTank_Is(t) || !t.modelindex)
    {
        return;
    }
    float worn = t.classname == "vr_fueltank_worn";
    entity m = worn ? t.owner : world; // (its wearer)
    entity g = worn ? VR_FuelTank_WornGun(t) : VR_FuelTank_Gun(t);
    float poison = g ? (g.weapon == WID_ADFUMIGATOR) : (t.model == "progs/vr_fueltank.mdl#7");
    float full = g ? WeaponIdToBaseClipSize(g.weapon) : 0;
    float fuel = (g && g.weaponinst) ? g.weaponinst.wi_clip : 0;
    float f = full > 0 ? clamp(fuel / full, 0, 1) : 0;
    if(!g && t.vr_hosecut)
    {
        f = clamp(t.vr_fueltank_fuel, 0, 1); // (cut free of its gun: the fuel it kept; "Cutting the hose")
        fuel = f;
    }
    vector at = t.origin + (t.mins + t.maxs) * 0.5;
    if(!worn)
    {
        VR_Carry_Forget(t); // (a hand holding it lets go)
    }
    // Its gun spent for good: no tank, no fuel, no force grab, no loot pillar (a hand still takes it).
    if(g)
    {
        g.weaponflags = g.weaponflags | QVR_WPNFLAG_TANK_BURST;
        if(g.weaponinst)
        {
            g.weaponinst.wi_clip = 0;
        }
        g.vr_depleted = TRUE;
        g.vr_fuellink = world;
    }
    if(worn && g)
    {
        // Its wearer's gun spent: it lets go of it (lying about spent: no tank) and fights on with its fists.
        m.vr_ag_wflags = m.vr_ag_wflags | QVR_WPNFLAG_TANK_BURST;
        m.vr_ag_reserve = 0;
        makevectors('0 1 0' * m.angles_y);
        entity dropped = VR_AG_DropHeld(m, v_forward * 120 + v_right * crandom() * 60 + '0 0 140');
        if(dropped)
        {
            dropped.vr_depleted = TRUE;
        }
    }
    vr_fueltank_bursts++;
    vr_fueltank_burst_hit = 0;
    // The tank itself gone (Killed carries on after its th_die: no model, nothing more done to it).
    VR_FuelTank_Hush(t);
    t.takedamage = DAMAGE_NO;
    t.th_die = SUB_Null;
    t.solid = SOLID_NOT;
    t.vr_fuellink = world;
    setmodel(t, "");
    t.think = SUB_Remove;
    t.nextthink = time + 0.1;
    if(f <= 0)
    {
        // Empty: it breaks with a puff of smoke and a clank.
        particle2(at, '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 4);
        particle2(at, '0 0 0', QVR_PARTICLE_PRESET_SMOKE, 1);
        sound(t, CHAN_AUTO, "weapons/tink1.wav", 0.8, ATTN_NORM);
        if(cvar("developer"))
        {
            dprint(sprintf("adgun: a tank bursts empty at %v (%s)\n", at, xBy.classname));
        }
        return;
    }
    float s = 0.4 + 0.6 * f; // (its reach, burning and poison: 40% near empty, all of it full)
    float radius = max(16, cvar("vr_adgun_tank_burst_radius")) * s;
    float blast = max(0, cvar("vr_adgun_tank_burst_damage")) * f * (poison ? 0.5 : 1);
    float burn = max(0, cvar("vr_adgun_tank_burst_burn")) * s;
    float dps = max(0, cvar("vr_adgun_tank_burst_poison")) * s;
    float ptime = max(0, cvar("vr_adgun_tank_burst_poisontime")) * s;
    // The look: fire or green poison thrown out through its reach, as much as the fuel; a flame's burst its explosion.
    float n = 4 + floor(14 * f);
    for(float i = 0; i < n; i++)
    {
        vector d = normalize(randomVec(1) + '0 0 0.4');
        traceline(at, at + d * radius * random() * 0.8, TRUE, t);
        particle2(trace_endpos, d * 2, poison ? QVR_PARTICLE_PRESET_ADPOISON : QVR_PARTICLE_PRESET_ADFLAME, 2 + floor(3 * f));
        if(poison)
        {
            particle2(trace_endpos, '0 0 0', QVR_PARTICLE_PRESET_POISONPUFF, 1);
        }
    }
    if(poison)
    {
        particle2(at, '0 0 0', QVR_PARTICLE_PRESET_POISONPUFF, 3 + floor(5 * f));
    }
    else
    {
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_EXPLOSION);
        WriteCoord(MSG_BROADCAST, at_x);
        WriteCoord(MSG_BROADCAST, at_y);
        WriteCoord(MSG_BROADCAST, at_z);
    }
    // Its sounds (the whoosh, stopped a moment after; the blast) and the field it leaves: its field's (VR_TankField_Make).
    entity field = VR_TankField_Make(at, poison, s, fuel, xBy);
    // What it reaches (in sight of it): its blast, then set on fire or poisoned. Another tank: it bursts a moment after.
    for(entity e = findradius(at, radius); e; e = e.chain)
    {
        if(e == t || e == g || !e.takedamage)
        {
            continue;
        }
        vector mid = e.origin + (e.mins + e.maxs) * 0.5;
        traceline(at, mid, TRUE, t);
        if(trace_fraction < 1 && vlen(trace_endpos - mid) > 24)
        {
            continue; // (behind a wall)
        }
        if(VR_FuelTank_Is(e))
        {
            if(!e.vr_fueltank_burstat)
            {
                e.vr_fueltank_burstat = time + 0.15 + random() * 0.15;
                e.enemy = xBy;
            }
            continue;
        }
        float k = 1 - 0.6 * clamp(vlen(mid - at) / radius, 0, 1); // (weaker at the edge)
        entity by = e == xBy ? world : xBy; // (the one who burst it is set on fire or poisoned as anyone: not by himself)
        vector dir = normalize(mid - at);
        vr_fueltank_burst_hit++;
        if(blast > 0)
        {
            VR_SetHitKind(QVR_HITKIND_NONE, -1, mid, dir);
            T_Damage(e, t, by, blast * k);
            VR_ClearHitKind();
        }
        if(!e.takedamage)
        {
            continue; // (gone: gibbed)
        }
        if(poison)
        {
            VR_ADGun_Poison(e, t, by, dps * k, ptime);
        }
        else if(burn > 0 && pointcontents(mid) > CONTENT_WATER)
        {
            vr_burn_len = burn;
            VR_Burn_Ignite(e, by, mid - dir * 8, dir, VR_BURN_TOUCH);
            vr_burn_len = 0;
        }
    }
    if(cvar("developer"))
    {
        dprint(sprintf("adgun: a %s tank bursts at %v (%s): fuel %g of %g, reach %.0f, ", poison ? "poison" : "fuel", at,
            xBy.classname, fuel, full, radius));
        dprint(sprintf("blast %.0f, %s, %g reached; ", blast,
            poison ? sprintf("poison %.1f/s for %.1f s", dps, ptime) : sprintf("burning %.1f s", burn), vr_fueltank_burst_hit));
        dprint(sprintf("its field %.1f s, reach %.0f%s\n", field.vr_tf_end - time, field.vr_tf_radius,
            worn ? sprintf(" (worn by %s)", m.classname) : ""));
    }
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
// A flame (AD's fire sprite: the player's, a pyro's) each think, `xStep` s since its last: gone into water or slime, out
// at the surface in a puff of steam with a hiss (vr_burn_water_douse; vr_burning.qc VR_Burn_Doused): TRUE, its caller
// removes it. Its last think's place is its oldorigin (a pyro's first think: where its speed had it a step before).
// (The player's flames: VR_ADGun_FlameThink; a pyro's: AD's pyroflame_think, ad_import.py's patch.)
float(entity flame, float xStep) VR_ADGun_FlameDoused =
{
    vector before = flame.oldorigin;
    flame.oldorigin = flame.origin;
    if(flame.model != ad_SBURST_FLAME || !VR_Burn_InWater(flame.origin) || cvar("vr_burn_water_douse") <= 0)
    {
        return FALSE;
    }
    if(before == '0 0 0')
    {
        before = flame.origin - flame.velocity * xStep;
    }
    vector at = VR_Burn_InWater(before) ? flame.origin : VR_Burn_WaterSurface(before, flame.origin);
    VR_Burn_Doused(at, 2, flame);
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("adgun: a flame (%s's) into water at %v: put out\n", flame.owner.classname, at));
    }
    return TRUE;
};

// A flame spent on a wall, the floor or a brush: a scorch where it struck (vr_decals.cpp fromEffect: as
// vr_adgun_fx_scorch says) and a tongue of fire licking over it (ROUND22.md, "Flamethrower scorches on every hit"). Once
// a flame (a pyro's slides along the wall, touching it each frame). Fire only (not poison, not smoke).
void(entity flame) VR_ADGun_FlameScorch =
{
    if(flame.vr_adgun_scorched || flame.model != ad_SBURST_FLAME || cvar("vr_adgun_fx_scorch") <= 0)
    {
        return;
    }
    flame.vr_adgun_scorched = TRUE;
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("adgun: a flame (%s's) scorches at %v\n", flame.owner.classname, flame.origin));
    }
    particle2(flame.origin, normalize(flame.velocity) * 2, QVR_PARTICLE_PRESET_ADFLAMEHIT,
        cvar("vr_adgun_stream_fire") > 0 ? 1 : 0);
};

void() VR_ADGun_FlameThink =
{
    if(VR_ADGun_FlameDoused(self, 0.05))
    {
        remove(self);
        return;
    }
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
    if(other.solid == SOLID_BSP || other == world)
    {
        VR_ADGun_FlameScorch(self); // (spent on a wall, the floor or a brush: a scorch where it struck)
        if(!other.takedamage)
        {
            remove(self);
            return;
        }
    }
    if(!other.takedamage)
    {
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
    f.oldorigin = org; // (where it was at its last think: VR_ADGun_FlameThink's surface, going into water)
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

// An armed monster's stream ended (its flamethrower or sprayer firing, vr_armedgrunt.qc: `m`.vr_adgun_stream its last
// flame's time): its loop's "off" on the channel it plays on, CHAN_WEAPON (`xWid` its gun). Vittorio, 2026-10-24
// (vrtesthall_2026-10-24_01-07-34): killed while firing, the hiss went on for ever; now its attack's end, its gun leaving
// its hand any way (VR_AG_DropHeld: dying, gibbed, disarmed, its tank burst, thrown away), its gun's think finding it
// gone or not firing for a moment (a pain cutting the attack short), `xGone` the ones it loses its gun or goes by.
float vr_ag_stream_ends;      // (the tests)
float vr_ag_stream_ends_gone;
float vr_fueltank_worn_alerts; // (the tests: wearers alerted by a hit on their tank alone, vr_adtankworn.qc)
void(entity m, float xWid, float xGone) VR_ADGun_MonsterStreamEnd =
{
    if(!m.vr_adgun_stream)
    {
        return;
    }
    m.vr_adgun_stream = 0;
    vr_ag_stream_ends++;
    vr_ag_stream_ends_gone += (xGone != 0);
    sound(m, CHAN_WEAPON, xWid == WID_ADFUMIGATOR ? "enforcer/fum_poisonoff.wav" : "enforcer/pyro_flameoff.wav", 1, ATTN_NORM);
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
#ifdef QVR_TESTS
float vr_adgun_test_index;   // the gun being tested (0..7)
float vr_adgun_test_step;    // its step: 0 equip and fire, 1..5 fire on (the streams), 6 read
float vr_adgun_test_pass;
float vr_adgun_test_fail;
float vr_adgun_test_clip;    // the clip as the gun was given
float vr_adgun_test_grip;    // vr_weapon_grip_mode as it was (1 for the test: the gun stays in a hand not gripping)
#endif

// vr_ad_test 90: the shots the launchers and a plasma gun fire, each from the main hand (aim it level: TESTING.md), and
// how each leaves the muzzle: the grenade soldier's launcher's grenade against the grenade launcher's (phase 1: model,
// box, movement, touch, blast, fuse and speed the same), the rocket soldier's rocket ours, the plasma soldier's bolt AD's
// with our hit; each 0.3 s on still in flight, its own, at least 90 units from the muzzle, touches of its firer counted.
#ifdef QVR_TESTS
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
#endif

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
