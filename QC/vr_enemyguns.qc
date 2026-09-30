//
//
//
// ----------------------------------------------------------------------------
// The grunts' shotguns and the enforcers' laser rifles (WID_GRUNTGUN, WID_ENFRIFLE; docs/vr-port/ROUND21.md,
// "Enemy weapons: the grunts' shotguns and the enforcers' laser rifles")
// ----------------------------------------------------------------------------
//
// Every grunt drops its shotgun and every enforcer its laser rifle when it dies (gibbed or not; their death frames are
// drawn without it: vr_monstermods.cpp), a weapon lying there to be picked up (progs/v_gruntgun.mdl, v_enfrifle.mdl:
// Misc/quakevr/make_enemyguns.py), as the knights drop their swords and the ogres their chainsaws.
//
// - Single use: its ammo is its own, the weapon's clip, full as it drops (vr_gruntgun_ammo shells, vr_enfrifle_ammo
//   shots); it goes with that gun from hand to hand, into a holster, onto the ground and back. Nothing refills it (its
//   ammo id is AID_NONE: shell boxes and cells fill your own guns, never it; no reloading). Empty, it clicks, and is still
//   a club (a gun's melee blow), or dropped.
// - The grunt's shotgun fires as yours (six pellets, your shotgun's spread), vr_gruntgun_damage a pellet (yours: 4).
// - The enforcer's rifle fires the enforcer's laser (progs/laser.mdl at 600 units a second), vr_enfrifle_damage a laser
//   (the enforcer's: 15).
// - The weapon's ammo counter shows what is left.

.float vr_laser_damage; // a laser's damage (Laser_Touch): 0 the enforcer's 15; a rifle's laser: vr_enfrifle_damage

void() VR_EnemyGuns_Precache =
{
    precache_model("progs/v_gruntgun.mdl");
    precache_model("progs/v_enfrifle.mdl");
    precache_model("progs/laser.mdl");      // (the enforcer's own precache is only where enforcers are)
    precache_sound("soldier/sattck1.wav");  // the grunt's shot
    precache_sound("enforcer/enfire.wav");  // the enforcer's laser, and where it hits
    precache_sound("enforcer/enfstop.wav");
};

float(float xWid) VR_IsEnemyGun =
{
    return xWid == WID_GRUNTGUN || xWid == WID_ENFRIFLE;
};

string(float xWid) VR_EnemyGun_Name =
{
    return xWid == WID_GRUNTGUN ? "grunt's shotgun" : "enforcer's rifle";
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

// The grunt's shotgun: as W_FireShotgun, from its own shells.
void(float xHand) VR_FireGruntGun =
{
    sound(self, VRGetGunChannel(xHand), "soldier/sattck1.wav", 1, ATTN_NORM);
    self.punchangle_x = -2;
    vr_pellet_damage = cvar_hget(cvarh_vr_gruntgun_damage);
    FireBullets(xHand, 6, VRGetWeaponFireRotFwd(xHand), '0.01 0.01 0');
    vr_pellet_damage = 0;
    VRGunHaptic(xHand, 0.25, 70, 0.75);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);
    self.damage_weapon = IT_SHOTGUN; // HONEY
};

// The enforcer's rifle: its laser, from the muzzle along the aim.
void(float xHand) VR_FireEnforcerRifle =
{
    sound(self, VRGetGunChannel(xHand), "enforcer/enfire.wav", 1, ATTN_NORM);
    self.punchangle_x = -1;
    entity laser = LaunchLaser(VRGetMuzzlePos(xHand), VRGetWeaponFireRotFwd(xHand));
    laser.vr_laser_damage = cvar_hget(cvarh_vr_enfrifle_damage);
    VRGunHaptic(xHand, 0.2, 90, 0.6);
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

    makevectors(VRGetWeaponFireRot(xHand));
    self.show_hostile = time + 1; // wake monsters up
    RuneApplyBlackNoise(self);
    SuperDamageSound();
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
