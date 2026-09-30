//
//
//
// ----------------------------------------------------------------------------
// The grunts' burst rifles and the enforcers' laser rifles (WID_GRUNTGUN, WID_ENFRIFLE; docs/vr-port/ROUND21.md,
// "Enemy weapons: the grunts' shotguns and the enforcers' laser rifles", "The grunts' burst rifles")
// ----------------------------------------------------------------------------
//
// Every grunt drops its gun and every enforcer its laser rifle when it dies (gibbed or not; their death frames are
// drawn without it: vr_monstermods.cpp), a weapon lying there to be picked up (progs/v_gruntgun.mdl, v_enfrifle.mdl:
// Misc/quakevr/make_enemyguns.py), as the knights drop their swords and the ogres their chainsaws.
//
// - Single use: its ammo is its own, the weapon's clip, full as it drops (vr_gruntgun_ammo rounds, vr_enfrifle_ammo
//   shots); it goes with that gun from hand to hand, into a holster, onto the ground and back. Nothing refills it (its
//   ammo id is AID_NONE: shell boxes and cells fill your own guns, never it; no reloading). Empty, it clicks, and is still
//   a club (a gun's melee blow), or dropped.
// - The grunt's gun is a burst rifle (vr_grunt_burst 1, the default): a trigger pull fires VR_BURST_ROUNDS single
//   bullets, VR_BURST_INTERVAL apart (VR_EnemyGuns_Frame fires the rest, the trigger held or not), vr_gruntgun_damage a
//   round, a round of its ammo each (a multiple of 3). As the grunts' own (orig_mon_soldier.qc army_fire:
//   vr_grunt_burst_damage a round). vr_grunt_burst 0: id's shotgun again, the grunts' and the dropped guns' (six pellets
//   as your shotgun, 4 a pellet, a shell a shot: a third of vr_gruntgun_ammo).
// - The enforcer's rifle fires the enforcer's laser (progs/laser.mdl), faster (vr_enfrifle_speed; the enforcer's: 600),
//   vr_enfrifle_damage a laser (the enforcer's: 15); as your nails, it strikes corpses, gibs, props and breakables
//   (VR_RifleLaser_Touch).
// - The weapon's ammo counter shows what is left.

.float vr_laser_damage; // a laser's damage (Laser_Touch): 0 the enforcer's 15; a rifle's laser: vr_enfrifle_damage

.float vr_burst_left;      // the main hand's burst rifle: the rounds of this burst still to fire (VR_EnemyGuns_Frame)
.float vr_burst_left2;     // the off hand's
.float vr_burst_next;      // the main hand's: when the next one fires
.float vr_burst_next2;     // the off hand's

float VR_BURST_ROUNDS = 3;            // a burst: the rounds of a trigger pull (a grunt's attack)
float VR_BURST_INTERVAL = 0.08;       // the player's: seconds between its rounds (750 a minute)
float VR_BURST_CYCLE = 0.5;           // the player's: a burst's start to the next's (the shotgun's shot to shot)
vector VR_BURST_SPREAD = '0.006 0.006 0'; // the player's: a round's spread (the shotgun's pellets: 0.01)

void() VR_EnemyGuns_Precache =
{
    precache_model("progs/v_gruntgun.mdl");
    precache_model("progs/v_enfrifle.mdl");
    precache_model("progs/laser.mdl");      // (the enforcer's own precache is only where enforcers are)
    precache_sound("soldier/sattck1.wav");  // the grunt's shot (id's shotgun: vr_grunt_burst 0)
    precache_sound("vr/burst1.wav");        // a burst's round (Misc/quakevr/make_sounds.py)
    precache_sound("vr/burst2.wav");
    precache_sound("vr/burst3.wav");
    precache_sound("enforcer/enfire.wav");  // the enforcer's laser, and where it hits
    precache_sound("enforcer/enfstop.wav");
};

float(float xWid) VR_IsEnemyGun =
{
    return xWid == WID_GRUNTGUN || xWid == WID_ENFRIFLE;
};

string(float xWid) VR_EnemyGun_Name =
{
    if(xWid == WID_GRUNTGUN)
    {
        return cvar_hget(cvarh_vr_grunt_burst) ? "grunt's burst rifle" : "grunt's shotgun";
    }
    return "enforcer's rifle";
};

// A burst's round's sound (three, a little apart: the same round again and again sounds mechanical).
string() VR_BurstSound =
{
    float r = random();
    return r < 0.34 ? "vr/burst1.wav" : r < 0.67 ? "vr/burst2.wav" : "vr/burst3.wav";
};

// A grunt's dropped gun's full clip: vr_gruntgun_ammo rounds, a multiple of 3 (a burst's); id's shotgun (vr_grunt_burst
// 0) a third as many shells.
float() VR_GruntGun_ClipSize =
{
    float rounds = max(VR_BURST_ROUNDS, rint(cvar_hget(cvarh_vr_gruntgun_ammo) / VR_BURST_ROUNDS) * VR_BURST_ROUNDS);
    return cvar_hget(cvarh_vr_grunt_burst) ? rounds : rounds / VR_BURST_ROUNDS;
};

float(float xHand) VR_GetBurstLeft = { return xHand == cVR_OffHand ? self.vr_burst_left2 : self.vr_burst_left; };
float(float xHand) VR_GetBurstNext = { return xHand == cVR_OffHand ? self.vr_burst_next2 : self.vr_burst_next; };
void(float xHand, float xLeft, float xNext) VR_SetBurst =
{
    if(xHand == cVR_OffHand)
    {
        self.vr_burst_left2 = xLeft;
        self.vr_burst_next2 = xNext;
    }
    else
    {
        self.vr_burst_left = xLeft;
        self.vr_burst_next = xNext;
    }
};

// A grunt or an enforcer dies: its gun falls where it held it (by its right side as it stands), full.
void(float xWid) VR_DropEnemyGun =
{
    makevectors(self.angles);
    vector from = self.origin + '0 0 8';
    vector at = self.origin + v_forward * 8 - v_right * 10 + '0 0 8';
    traceline(from, at, TRUE, self);
    at = trace_endpos + (from - trace_endpos) * 0.2 * (trace_fraction < 1);
    vector vel = v_forward * 25 - v_right * 20 + v_right * (crandom() * 15) + '0 0 70';
    CreateThrownWeapon(self, xWid, 0, WeaponIdToBaseClipSize(xWid) /* its ammo */,
        at, self.angles, vel, randomVec(60),
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(xWid));
};

// The grunt's burst rifle: a round, a single bullet from its muzzle along the aim.
void(float xHand) VR_FireBurstRound =
{
    sound(self, CHAN_AUTO, VR_BurstSound(), 1, ATTN_NORM); // (not the gun's channel: each round's tail rings on)
    self.punchangle_x = -1;
    vr_pellet_damage = cvar_hget(cvarh_vr_gruntgun_damage);
    FireBullets(xHand, 1, VRGetWeaponFireRotFwd(xHand), VR_BURST_SPREAD);
    vr_pellet_damage = 0;
    VRGunHaptic(xHand, 0.08, 110, 0.6);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);
    self.damage_weapon = IT_SHOTGUN; // HONEY
};

// id's shotgun (vr_grunt_burst 0): as W_FireShotgun, from its own shells.
void(float xHand) VR_FireGruntGun =
{
    sound(self, VRGetGunChannel(xHand), "soldier/sattck1.wav", 1, ATTN_NORM);
    self.punchangle_x = -2;
    vr_pellet_damage = 4; // your shotgun's pellet
    FireBullets(xHand, 6, VRGetWeaponFireRotFwd(xHand), '0.01 0.01 0');
    vr_pellet_damage = 0;
    VRGunHaptic(xHand, 0.25, 70, 0.75);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);
    self.damage_weapon = IT_SHOTGUN; // HONEY
};

// A rifle's laser strikes: as your nails (spike_touch), what takes damage takes vr_enfrifle_damage (positional: a head
// more), a corpse, a gib or a breakable too; a prop is pushed (VR_ShotPush_Touch); the enforcer's sounds and sparks.
void() VR_RifleLaser_Touch =
{
    if(other == self.owner || other.solid == SOLID_TRIGGER)
    {
        return;
    }
    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    sound(self, CHAN_WEAPON, "enforcer/enfstop.wav", 1, ATTN_STATIC);
    VR_ShotPush_Touch(); // the prop it hit (or flew through) pushed

    makeforward_mdl(self.angles);
    VR_TouchTrace(self.origin - v_forward * 8, self.origin + v_forward * 8);

    float bleed = FALSE;
    if(other.takedamage)
    {
        float before = other.health;
        float dmg = self.vr_laser_damage * PositionalDamageFactor(PositionalDamage(trace_ent, trace_endpos, v_forward));
        if(T_Damage(other, self, self.owner, dmg))
        {
            spawn_touchblood(dmg);
            bleed = TRUE;
        }
        if(cvar_hget(cvarh_vr_debug_shots))
        {
            dprint(sprintf("rifle laser: hit %s (%s) at %g at %v, %g damage, health %g -> %g\n", other.classname,
                other.model, time, self.origin, dmg, before, other.health));
        }
    }
    else if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("rifle laser: hit %s (%s, solid %g) at %g at %v (no damage)\n",
            other == world ? "world" : other.classname, other.model, other.solid, time, self.origin));
    }
    if(!bleed)
    {
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_GUNSHOT);
        WriteVec3(MSG_BROADCAST, self.origin - 8 * normalize(self.velocity));
    }
    remove(self);
};

// The enforcer's rifle: its laser, from the muzzle along the aim.
void(float xHand) VR_FireEnforcerRifle =
{
    sound(self, VRGetGunChannel(xHand), "enforcer/enfire.wav", 1, ATTN_NORM);
    self.punchangle_x = -1;
    vector dir = VRGetWeaponFireRotFwd(xHand);
    entity laser = LaunchLaser(VRGetMuzzlePos(xHand), dir);
    laser.vr_laser_damage = cvar_hget(cvarh_vr_enfrifle_damage);
    // As your nails (launch_spike): a missile's move, which meets corpses and gibs too (SV_Move's MOVE_MISSILE), from
    // behind what the muzzle is inside, the hand's swing carried; VR_RifleLaser_Touch deals its damage.
    laser.movetype = MOVETYPE_FLYMISSILE;
    laser.velocity = dir * max(100, cvar_hget(cvarh_vr_enfrifle_speed)) + vrProjectileVelocity(xHand, 1.0);
    laser.angles = vectoangles(laser.velocity);
    laser.touch = VR_RifleLaser_Touch;
    laser.classname = "rifle_laser";
    VR_ShotPlace(laser, xHand);
    VRGunHaptic(xHand, 0.2, 90, 0.6);
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("rifle laser: fired at %g from %v, %g u/s\n", time, laser.origin, vlen(laser.velocity)));
    }
};

// A burst's round from the gun in that hand, its clip less one (the trigger pull's first, VR_EnemyGun_Attack, or the
// burst's next, VR_EnemyGuns_Frame).
void(float xHand) VR_BurstRound =
{
    makevectors(VRGetWeaponFireRot(xHand));
    self.show_hostile = time + 1; // wake monsters up
    VRSetWeaponClip(xHand, VRGetWeaponClip(xHand) - 1);
    player_shot1(xHand);
    VR_FireBurstRound(xHand);
    updateAmmoCounters();
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("burst round: hand %g, %g more in the burst, %g in the clip, at %g\n", xHand,
            VR_GetBurstLeft(xHand), VRGetWeaponClip(xHand), time));
    }
};

// Every player frame (PlayerPostThink), each hand: a burst's next rounds, VR_BURST_INTERVAL apart, the trigger held or
// let go; the burst ends with the clip, or with the gun leaving the hand.
void(float xHand) VR_EnemyGuns_Frame =
{
    float left = VR_GetBurstLeft(xHand);
    if(left < 1)
    {
        return;
    }
    if(VRGetWeapon(xHand) != WID_GRUNTGUN || VRGetWeaponClip(xHand) < 1 || self.health <= 0)
    {
        VR_SetBurst(xHand, 0, 0);
        return;
    }
    if(time < VR_GetBurstNext(xHand))
    {
        return;
    }
    VR_SetBurst(xHand, left - 1, VR_GetBurstNext(xHand) + VR_BURST_INTERVAL);
    VR_BurstRound(xHand);
};

// W_AttackImpl for an enemy gun: a shot from its clip, or the empty click.
void(float xHand, float xWeapon, float xAttackFinished, float* xOutAttackFinished) VR_EnemyGun_Attack =
{
    if(VRGetWeaponClip(xHand) < 1)
    {
        if(time > xAttackFinished)
        {
            sound(self, CHAN_AUTO, "gunclick.wav", 1, ATTN_NORM);
            *xOutAttackFinished = time + 0.5;
            if(cvar_hget(cvarh_vr_debug_shots))
            {
                dprint(sprintf("empty click: hand %g, %s (its own ammo spent)\n", xHand, VR_EnemyGun_Name(xWeapon)));
            }
        }
        return;
    }

    RuneApplyBlackNoise(self);
    SuperDamageSound();
    if(xWeapon == WID_GRUNTGUN && cvar_hget(cvarh_vr_grunt_burst))
    {
        // The burst's first round now, the rest from VR_EnemyGuns_Frame; the next pull a cycle after this one.
        VR_SetBurst(xHand, min(VR_BURST_ROUNDS, VRGetWeaponClip(xHand)) - 1, time + VR_BURST_INTERVAL);
        VR_BurstRound(xHand);
        *xOutAttackFinished = time + RuneApplyHell(VR_BURST_CYCLE, self);
        return;
    }

    makevectors(VRGetWeaponFireRot(xHand));
    self.show_hostile = time + 1; // wake monsters up
    VRSetWeaponClip(xHand, VRGetWeaponClip(xHand) - 1);
    player_shot1(xHand);
    if(xWeapon == WID_GRUNTGUN)
    {
        VR_FireGruntGun(xHand);
        *xOutAttackFinished = time + RuneApplyHell(0.5, self);
    }
    else
    {
        VR_FireEnforcerRifle(xHand);
        *xOutAttackFinished = time + RuneApplyHell(0.3, self);
    }
    updateAmmoCounters();
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("%s fired: hand %g, %g left\n", VR_EnemyGun_Name(xWeapon), xHand, VRGetWeaponClip(xHand)));
    }
};

// updateAmmoCounter: an enemy gun's counter is its clip (its only ammo).
float(float xHand) VR_EnemyGun_Counter =
{
    if(!VR_IsEnemyGun(VRGetWeapon(xHand)))
    {
        return FALSE;
    }
    VRSetAmmoCounter(xHand, VRGetWeaponClip(xHand));
    VRSetWeaponClipSize(xHand, 0);
    return TRUE;
};

// impulse 213 (Debug > Tests): the enemy guns in your hands and their ammo; 214 (a test aid): left with one shot.
void(float xOneLeft) VR_EnemyGuns_Impulse =
{
    for(float h = 0; h < 2; h++)
    {
        float hand = h ? cVR_MainHand : cVR_OffHand;
        if(!VR_IsEnemyGun(VRGetWeapon(hand)))
        {
            continue;
        }
        if(xOneLeft)
        {
            VRSetWeaponClip(hand, 1);
            updateAmmoCounters();
        }
        bprint(sprintf("enemy gun: %s hand: %s, %g left (a full one: %g; your shells %g, cells %g)\n",
            hand == cVR_OffHand ? "off" : "main", VR_EnemyGun_Name(VRGetWeapon(hand)), VRGetWeaponClip(hand),
            WeaponIdToBaseClipSize(VRGetWeapon(hand)), self.ammo_shells, self.ammo_cells));
    }
};

// impulse 212 (a test aid; Debug > Tests): the enemy gun lying nearest you taken into an empty hand, as a hand closing
// on it does (its handtouch, as a force grab's catch).
void() VR_EnemyGuns_TakeNearest =
{
    entity best = world;
    float bestDist = 1000;
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        if(VR_IsEnemyGun(e.weapon) && vlen(e.origin - self.origin) < bestDist)
        {
            best = e;
            bestDist = vlen(e.origin - self.origin);
        }
        e = find(e, classname, "thrown_weapon");
    }
    float hand = VRIsHandEmpty(self, cVR_MainHand) ? cVR_MainHand : VRIsHandEmpty(self, cVR_OffHand) ? cVR_OffHand : -1;
    if(best == world || hand < 0 || !best.handtouch)
    {
        bprint("enemy gun: none lying within 1000 units, or no empty hand\n");
        return;
    }
    bprint(sprintf("enemy gun: taking the %s %g units away (%g left) into the %s hand\n", VR_EnemyGun_Name(best.weapon),
        bestDist, best.weaponclip, hand == cVR_OffHand ? "off" : "main"));
    entity oldSelf = self;
    entity oldOther = other;
    self.touchinghand = hand;
    best.handtouch_hand = hand;
    best.handtouch_ent = self;
    other = self;
    self = best;
    float oldCatching = forcegrabCatching;
    forcegrabCatching = TRUE;
    best.handtouch();
    forcegrabCatching = oldCatching;
    self = oldSelf;
    other = oldOther;
};
