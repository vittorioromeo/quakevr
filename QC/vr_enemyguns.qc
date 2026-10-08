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
    precache_sound("misc/power.wav");       // a spent gun's crackle (VR_EnemyGun_Spent: the shocked corpses')
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

// ----------------------------------------------------------------------------
// The cap on dropped single-use enemy weapons (ROUND21.md, "A cap on dropped enemy weapons")
// ----------------------------------------------------------------------------
//
// The grunts' burst rifles, the enforcers' laser rifles and the ogres' chainsaws (single use: their ammo or fuel their
// own) that monsters dropped, lying about: at most vr_enemy_weapon_drop_max (48; 0: no limit). Past it, as one more is
// made (a monster's drop, a player's drop or throw of one), the oldest (.vr_drop_born: saved, so the order survives a load) fades away; one in a hand
// (not lying about), being carried, pulled by a force grab or in flight is spared: the next oldest goes. Every other
// weapon, drop and prop stays.

float VR_DROPCAP_FLYING = 40; // units/s: one moving faster is in flight (thrown, falling), never taken away
float VR_DROPCAP_FADE = 0.4;  // seconds its fade takes

float(float xWid) VR_EnemyWeapons_Capped = { return VR_IsEnemyGun(xWid) || xWid == WID_CHAINSAW; };

// Counted under the cap: a single-use enemy weapon a monster dropped (not a map's or a dispenser's: its record's
// .vr_enemy_drop), lying about (or flying), not one fading.
float(entity e) VR_EnemyWeapons_Counted =
{
    return VR_EnemyWeapons_Capped(e.weapon) && e.weaponinst.vr_enemy_drop && !e.vr_drop_fade;
};

// One the cap never takes away: carried, pulled to a hand, in flight.
float(entity e) VR_EnemyWeapons_Spared =
{
    return e.carry_player != world || e.fg_state != 0 || vlen(e.velocity) > VR_DROPCAP_FLYING;
};

void() VR_EnemyWeapons_FadeThink =
{
    self.alpha = max(0.01, 1 - (time - self.vr_drop_fade) / VR_DROPCAP_FADE);
    if(time >= self.vr_drop_fade + VR_DROPCAP_FADE)
    {
        remove(self);
        return;
    }
    self.nextthink = time + 0.02;
};

// `e` fades away and is removed: nothing can take it meanwhile (no hand, no force grab), its engine (a chainsaw's)
// stopped.
void(entity e) VR_EnemyWeapons_Fade =
{
    VR_Saw_PropStop(e);
    e.vr_drop_fade = time;
    e.handtouch = SUB_Null;
    e.touch = SUB_Null;
    e.flags = e.flags - (e.flags & FL_FORCEGRABBABLE);
    e.fg_state = 2; // (VR_Forcegrab_IsEligible: none)
    e.alpha = 1;
    e.think = VR_EnemyWeapons_FadeThink;
    e.nextthink = time + 0.02;
};

// `xNew` made (CreateThrownWeapon): if it is a single-use enemy weapon, the oldest of them go past the cap.
void(entity xNew) VR_EnemyWeapons_MakeRoom =
{
    float most = cvar_hget(cvarh_vr_enemy_weapon_drop_max);
    if(most <= 0 || !VR_EnemyWeapons_Counted(xNew))
    {
        return;
    }
    float n = 0;
    entity e;
    // The count and the first oldest in one walk (nothing changes between them: the tests only read; MG3_PLAN.md
    // M3-28); after a fade, walked again as before.
    entity oldest = world;
    for(e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
    {
        n = n + VR_EnemyWeapons_Counted(e);
        if(e != xNew && VR_EnemyWeapons_Counted(e) && !VR_EnemyWeapons_Spared(e) &&
           (oldest == world || e.vr_drop_born < oldest.vr_drop_born))
        {
            oldest = e;
        }
    }
    float first = TRUE;
    while(n > most)
    {
        if(!first)
        {
            oldest = world;
            for(e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
            {
                if(e != xNew && VR_EnemyWeapons_Counted(e) && !VR_EnemyWeapons_Spared(e) &&
                   (oldest == world || e.vr_drop_born < oldest.vr_drop_born))
                {
                    oldest = e;
                }
            }
        }
        first = FALSE;
        if(oldest == world)
        {
            return; // (all the rest spared: over the cap a moment)
        }
        dprint(sprintf("dropcap: the oldest %s (made at %.2f) fades away (%g lying about, the most %g)\n", oldest.netname,
            oldest.vr_drop_born, n, most));
        VR_EnemyWeapons_Fade(oldest);
        n = n - 1;
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
    CreateThrownWeapon(self, xWid, 0, WeaponInst_New(WeaponIdToBaseClipSize(xWid)) /* its ammo */,
        at, self.angles, vel, randomVec(60),
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(xWid));
};

// The grunt's burst rifle: a round, a single bullet from its muzzle along the aim.
void(float xHand) VR_FireBurstRound =
{
    sound(self, VRGetHandChannel(xHand), VR_BurstSound(), 1, ATTN_NORM); // (any free one of the hand's, not the gun's: each round's tail rings on)
    self.punchangle_x = -1;
    vr_pellet_damage = cvar_hget(cvarh_vr_gruntgun_damage);
    FireBullets(xHand, 1, VRGetWeaponFireRotFwd(xHand), VR_BURST_SPREAD);
    vr_pellet_damage = 0;
    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp)
    VRGunHaptic(xHand, 0.08, 110, 0.6);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);
    self.damage_weapon = IT_SHOTGUN; // HONEY
};

// id's shotgun (vr_grunt_burst 0): as W_FireShotgun, from its own shells.
void(float xHand) VR_FireGruntGun =
{
    sound(self, VRGetGunChannel(xHand), "soldier/sattck1.wav", 1, ATTN_NORM);
    self.punchangle_x = -2;
    vr_pellet_damage = cvar_hget(cvarh_vr_dmg_shotgun); // your shotgun's pellet (Weapon Damage)
    FireBullets(xHand, 6, VRGetWeaponFireRotFwd(xHand), '0.01 0.01 0');
    vr_pellet_damage = 0;
    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp)
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
        vector hitAt = trace_ent == other ? trace_endpos : self.origin;
        vector hitDir = v_forward;
        float dmg = self.vr_laser_damage * PositionalDamageFactor(PositionalDamage(trace_ent, trace_endpos, v_forward));
        // (A kill at the head or a limb pops it by chance, as an enemy's bolt: vr_decap_pop_laser, vr_limbs.qc.)
        float pop = VR_Decap_LaserArm(other, self.owner, hitAt, hitDir, dmg);
        if(T_Damage(other, self, self.owner, dmg))
        {
            spawn_touchblood(dmg);
            bleed = TRUE;
        }
        if(pop)
        {
            VR_Decap_After(other);
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
    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp)
    VRGunHaptic(xHand, 0.2, 90, 0.6);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1); // its smoke, as your other guns'
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("rifle laser: fired at %g from %v, %g u/s\n", time, laser.origin, vlen(laser.velocity)));
    }
};

// ----------------------------------------------------------------------------
// A spent gun's cues (the author's note grenedin_2026-10-08_00-07-22)
// ----------------------------------------------------------------------------
//
// The shot that empties a grunt's burst rifle or an enforcer's laser rifle sets it smoking for vr_enemygun_spent_smoke
// seconds (wisps off its muzzle in the hand, off its middle lying about; holstered, none) and crackling all over with
// lightning's lasting arcs, as a corpse the lightning struck does, for vr_enemygun_spent_crackle seconds, with the
// corpses' crackle (misc/power.wav) at vr_enemygun_spent_volume: it is done, drop it. Kept on the gun's record (its
// weapon_inst, which goes with it from hand to ground and back), thinking every tenth of a second while they last.

.float wi_spent;        // a record's: when its gun's own ammo ran out (its cues from then; 0: none)
.float wi_spent_where;  // ... where its arcs were last sent: 1 the off hand, 2 the main hand, 3 lying about; 0 nowhere
.entity wi_spent_owner; // ... the player who emptied it (it is looked for in his hands)
.float wi_spent_puffs;  // ... its wisps of smoke so far (the log)
float vr_enemygun_spent_arcs; // the arcs sent (the tests)

// The spent gun's arcs for `xLeft` s: over the gun in a hand of `p` (`xWhere` 1 off, 2 main: its client draws them on
// the gun as drawn), or over `xLying`, the gun lying about (every client).
void(entity p, float xWhere, entity xLying, float xLeft) VR_EnemyGun_SpentArcs =
{
    vr_enemygun_spent_arcs++;
    if(xWhere == 3)
    {
        bodyshockdeath(xLying, xLeft);
        return;
    }
    entity oldSelf = self;
    self = p;
    watershock(QVR_SHOCK_GUN, p.origin, xWhere == 2 ? cVR_MainHand : cVR_OffHand, xLeft);
    self = oldSelf;
};

// Every tenth of a second while its cues last (self: the spent gun's record): where the gun is now, its smoke there,
// its arcs sent again as it moves between a hand and the ground.
void() VR_EnemyGun_SpentThink =
{
    entity inst = self;
    float t = time - inst.wi_spent;
    float smoke = max(0, cvar_hget(cvarh_vr_enemygun_spent_smoke));
    float crackle = max(0, cvar_hget(cvarh_vr_enemygun_spent_crackle));
    if(t >= smoke && t >= crackle)
    {
        if(cvar_hget(cvarh_vr_debug_shots))
        {
            dprint(sprintf("spent gun: its cues over after %.1f s: %g wisps, arcs sent %g times\n", t, inst.wi_spent_puffs,
                vr_enemygun_spent_arcs));
        }
        inst.wi_spent = 0;
        inst.wi_spent_where = 0;
        return;
    }
    inst.nextthink = time + 0.1;
    entity p = inst.wi_spent_owner;
    float where = 0;
    entity lying = world;
    if(p != world && p.weaponinst2 == inst)
    {
        where = 1;
    }
    else if(p != world && p.weaponinst == inst)
    {
        where = 2;
    }
    else
    {
        for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
        {
            if(e.weaponinst == inst)
            {
                lying = e;
                where = 3;
                break;
            }
        }
    }
    if(where != inst.wi_spent_where)
    {
        inst.wi_spent_where = where;
        if(where && t < crackle)
        {
            VR_EnemyGun_SpentArcs(p, where, lying, crackle - t);
        }
        if(cvar_hget(cvarh_vr_debug_shots))
        {
            dprint(sprintf("spent gun: %s at %.1f s\n", where == 1 ? "in the off hand" : (where == 2 ? "in the main hand" :
                (where == 3 ? "lying about" : "out of sight (holstered)")), t));
        }
    }
    if(!where || t >= smoke || random() >= 0.35 + 0.65 * (smoke - t) / max(0.1, smoke))
    {
        return;
    }
    vector at = where == 1 ? p.offmuzzlepos : (where == 2 ? p.muzzlepos : lying.origin + '0 0 2');
    if(where != 3 && vlen(at - VRGetEntHandPos(p, where == 2 ? cVR_MainHand : cVR_OffHand)) > 64)
    {
        at = VRGetEntHandPos(p, where == 2 ? cVR_MainHand : cVR_OffHand); // (no muzzle reported: a bot's)
    }
    particle2(at, '0 0 0', QVR_PARTICLE_PRESET_BATTERYSMOKE, 1);
    inst.wi_spent_puffs = inst.wi_spent_puffs + 1;
};

// The gun in that hand of self has just fired its last round: its cues start (none if both times are 0).
void(float xHand) VR_EnemyGun_Spent =
{
    entity inst = VRGetEntWeaponInst(self, xHand);
    float smoke = max(0, cvar_hget(cvarh_vr_enemygun_spent_smoke));
    float crackle = max(0, cvar_hget(cvarh_vr_enemygun_spent_crackle));
    if(inst == world || (smoke <= 0 && crackle <= 0))
    {
        return;
    }
    inst.wi_spent = time;
    inst.wi_spent_where = 0;
    inst.wi_spent_owner = self;
    inst.wi_spent_puffs = 0;
    inst.think = VR_EnemyGun_SpentThink;
    inst.nextthink = time + 0.05;
    float vol = cvar_hget(cvarh_vr_enemygun_spent_volume);
    if(crackle > 0 && vol > 0)
    {
        sound(self, CHAN_AUTO, "misc/power.wav", min(1, vol), ATTN_NORM);
    }
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("spent gun: %s (hand %g) empty: smoking %g s, crackling %g s, volume %g\n",
            VR_EnemyGun_Name(VRGetWeapon(xHand)), xHand, smoke, crackle, vol));
    }
};

// A burst's round from the gun in that hand, its clip less one (the trigger pull's first, VR_EnemyGun_Attack, or the
// burst's next, VR_EnemyGuns_Frame).
void(float xHand) VR_BurstRound =
{
    makevectors(VRGetWeaponFireRot(xHand));
    self.show_hostile = time + 1; // wake monsters up
    VR_Stealth_Shot(self, WID_GRUNTGUN); // QVR: heard (vr_stealth.qc)
    VRSetWeaponClip(xHand, VRGetWeaponClip(xHand) - 1);
    player_shot1(xHand);
    VR_FireBurstRound(xHand);
    updateAmmoCounters();
    if(VRGetWeaponClip(xHand) < 1)
    {
        VR_EnemyGun_Spent(xHand);
    }
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
            sound(self, VRGetHandChannel(xHand), "gunclick.wav", 1, ATTN_NORM);
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
    VR_Stealth_Shot(self, WID_GRUNTGUN); // QVR: heard (vr_stealth.qc)
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
    if(VRGetWeaponClip(xHand) < 1)
    {
        VR_EnemyGun_Spent(xHand);
    }
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
        bestDist, best.weaponinst.wi_clip, hand == cVR_OffHand ? "off" : "main"));
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
