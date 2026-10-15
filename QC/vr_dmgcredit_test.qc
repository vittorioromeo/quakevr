// vr_dmgcredit_test.qc -- the weapon each of a player's weapons is credited with (vr_dmgcredit.qc; Honey's
// FL_SPECIFICDAMAGE targets take damage only from the weapons in their .items).
//
// `vr_dmgcredit_test 1` (Debug > Tests > Damage Credit): every weapon there is (the expansions' when their data is) fired
// from the main hand at a target ahead that takes only one weapon's damage, twice: once taking only the weapon's own
// credit, once only the credit the player was last left with (set to another weapon's just before: as if he had just
// struck with it). The weapon credited is the one the target took: `dmgcredit: <weapon>: own <bit> hit|refused, last
// <bit> hit|refused: credited <bit>: PASS|FAIL`, then `dmgcredit: <n> passed, <m> failed`. PASS: the weapon's own bit.
// `vr_dmgcredit_test 100+i` tests weapon i alone (VR_DCTest_Name: 0 shotgun ... 21 thrown laser cannon). The hand
// aimed about level (the mock headset's: `vr_mock_hand main 0.2 1.0 -0.3 65 0 0`): the target goes short of the wall or
// floor the hand points at.

entity vr_dctest_target; // the target ahead
entity vr_dctest_player;
float vr_dctest_index;   // the weapon being tested (VR_DCTest_Wid)
float vr_dctest_pass;    // 0: the target takes its own credit; 1: the last one's
float vr_dctest_ticks;   // the pass's ticks (0.1 s) so far
float vr_dctest_hit;     // the target took damage in this pass
float vr_dctest_ownhit;  // ... in pass 0
string vr_dctest_by;     // what struck it last
float vr_dctest_passed;
float vr_dctest_failed;
float vr_dctest_godmode; // the player's FL_GODMODE as it was
float vr_dctest_grip;    // vr_weapon_grip_mode as it was (1 for the test: a weapon stays in a hand not gripping)
float vr_dctest_end;     // the test after the last to run
vector vr_dctest_home;   // where the player stood

float VR_DCTEST_COUNT = 22;

// Test `i`'s weapon id (-1: thrown), its weapon flags, its name and the credit it should get.
float(float i) VR_DCTest_Wid =
{
    if(i == 0)  { return WID_SHOTGUN; }
    if(i == 1)  { return WID_SUPER_SHOTGUN; }
    if(i == 2)  { return WID_NAILGUN; }
    if(i == 3)  { return WID_SUPER_NAILGUN; }
    if(i == 4)  { return WID_GRENADE_LAUNCHER; }
    if(i == 5)  { return WID_ROCKET_LAUNCHER; }
    if(i == 6)  { return WID_LIGHTNING; }
    if(i == 7)  { return WID_LASER_CANNON; }
    if(i == 8)  { return WID_PROXIMITY_GUN; }
    if(i == 9)  { return WID_NAILGUN; }          // lava nails
    if(i == 10) { return WID_SUPER_NAILGUN; }    // lava super nails
    if(i == 11) { return WID_GRENADE_LAUNCHER; } // multi grenades
    if(i == 12) { return WID_ROCKET_LAUNCHER; }  // multi rockets
    if(i == 13) { return WID_LIGHTNING; }        // plasma
    if(i == 14) { return WID_GRUNTGUN; }
    if(i == 15) { return WID_ENFRIFLE; }
    if(i == 16) { return WID_ADROCKET; }
    if(i == 17) { return WID_ADGRENADE; }
    if(i == 18) { return WID_ADPLASMA; }
    if(i == 19) { return WID_ADDEFENDER; }
    if(i == 20) { return WID_AXE; }              // thrown
    return WID_LASER_CANNON;                     // thrown
};

float(float i) VR_DCTest_Flags = { return (i >= 9 && i <= 13) ? QVR_WPNFLAG_USE_SECONDARY_AMMO : 0; };

string(float i) VR_DCTest_Name =
{
    if(i == 0)  { return "shotgun"; }
    if(i == 1)  { return "super shotgun"; }
    if(i == 2)  { return "nails"; }
    if(i == 3)  { return "super nails"; }
    if(i == 4)  { return "grenade"; }
    if(i == 5)  { return "rocket"; }
    if(i == 6)  { return "lightning"; }
    if(i == 7)  { return "laser cannon"; }
    if(i == 8)  { return "proximity grenade"; }
    if(i == 9)  { return "lava nails"; }
    if(i == 10) { return "lava super nails"; }
    if(i == 11) { return "multi grenades"; }
    if(i == 12) { return "multi rockets"; }
    if(i == 13) { return "plasma"; }
    if(i == 14) { return "grunt's shotgun"; }
    if(i == 15) { return "enforcer's rifle"; }
    if(i == 16) { return "AD rocket soldier's launcher"; }
    if(i == 17) { return "AD grenade soldier's launcher"; }
    if(i == 18) { return "AD plasma rifle"; }
    if(i == 19) { return "AD defender's shotgun"; }
    if(i == 20) { return "thrown axe"; }
    return "thrown laser cannon";
};

float(float i) VR_DCTest_Expect =
{
    float wid = VR_DCTest_Wid(i);
    if(wid == WID_SHOTGUN || wid == WID_GRUNTGUN)                         { return IT_SHOTGUN; }
    if(wid == WID_SUPER_SHOTGUN || wid == WID_ADDEFENDER)                 { return IT_SUPER_SHOTGUN; }
    if(wid == WID_NAILGUN)                                                { return IT_NAILGUN; }
    if(wid == WID_SUPER_NAILGUN)                                          { return IT_SUPER_NAILGUN; }
    if(wid == WID_GRENADE_LAUNCHER || wid == WID_PROXIMITY_GUN || wid == WID_ADGRENADE) { return IT_GRENADE_LAUNCHER; }
    if(wid == WID_ROCKET_LAUNCHER || wid == WID_ADROCKET)                 { return IT_ROCKET_LAUNCHER; }
    if(wid == WID_LIGHTNING || wid == WID_ADPLASMA)                       { return IT_LIGHTNING; }
    if(wid == WID_LASER_CANNON || wid == WID_ENFRIFLE)                    { return IT_LASER_CANNON; }
    return IT_AXE;
};

// The credit the player is left with before each shot: another weapon's.
float(float i) VR_DCTest_Last = { return VR_DCTest_Expect(i) == IT_AXE ? IT_SHOTGUN : IT_AXE; };

// Whether test `i`'s weapon is there (an expansion's data, AD's guns).
float(float i) VR_DCTest_Available =
{
    if(!vr_pack_refreshed) { VR_Pack_Refresh(); }
    if(i == 9 || i == 10) { return vr_have_lava; }
    if(i == 12) { return vr_have_multirockets; }
    if(i == 13) { return vr_have_plasma; }
    return VR_Pack_WeaponAvailable(VR_DCTest_Wid(i));
};

void(entity attacker, float damage) VR_DCTest_TargetPain =
{
    vr_dctest_hit = TRUE;
    vr_dctest_by = damage_inflictor.classname;
};
void() VR_DCTest_TargetDie = { self.health = 100000; };

// The player's shots still about (between passes: a late blast would count in the next).
void(entity p) VR_DCTest_ClearShots =
{
    entity e = nextent(world);
    while(e)
    {
        entity n = nextent(e);
        if(e.owner == p && e.touch && e.touch != SUB_Null &&!(e.flags & FL_CLIENT) &&
           (e.movetype == MOVETYPE_FLYMISSILE || e.movetype == MOVETYPE_BOUNCE || e.movetype == MOVETYPE_FLY ||
            e.movetype == MOVETYPE_TOSS || e.movetype == MOVETYPE_BOUNCEMISSILE))
        {
            remove(e);
        }
        e = n;
    }
};

// The target `xDist` units out along the main hand's aim, taking only `xItems`' damage (self the player).
void(float xDist, float xItems) VR_DCTest_Place =
{
    entity t = vr_dctest_target;
    if(t == world)
    {
        t = spawn();
        t.classname = "vr_dmgcredit_target";
        t.solid = SOLID_SLIDEBOX;
        t.movetype = MOVETYPE_NONE;
        t.takedamage = DAMAGE_AIM;
        t.th_pain = VR_DCTest_TargetPain;
        t.th_die = VR_DCTest_TargetDie;
        setmodel(t, "progs/player.mdl");
        setsize(t, '-16 -16 -24', '16 16 32');
        vr_dctest_target = t;
    }
    t.health = 100000;
    t.flags = FL_SPECIFICDAMAGE; // (not a monster: the monsters' hooks would want its AI)
    t.items = xItems;
    vector from = VRGetMuzzlePos(cVR_MainHand);
    vector dir = VRGetWeaponFireRotFwd(cVR_MainHand);
    traceline(from, from + dir * (xDist + 40), TRUE, self); // (no monsters; short of the wall or floor it points at)
    setorigin(t, from + dir * max(48, (xDist + 40) * trace_fraction - 40) - '0 0 4');
};

// Test `i`'s weapon fired from the main hand (self the player).
void(float i) VR_DCTest_Fire =
{
    float h = cVR_MainHand;
    float wid = VR_DCTest_Wid(i);
    if(i >= 20)
    {
        vector dir = VRGetWeaponFireRotFwd(h);
        CreateThrownWeapon(self, wid, 0, WeaponInst_New(WeaponIdToBaseClipSize(wid)), VRGetMuzzlePos(h),
            vectoangles(dir), dir * 700, '0 0 0', FALSE, TRUE, WeaponIdToThrowDamage(wid));
        return;
    }
    if(VR_IsADGun(wid)) { VR_ADGun_Ensure(wid); }
    VR_WeaponUtil_EntAddWeapon(self, wid);
    VRGiveEntWeapon(self, h, wid, VR_DCTest_Flags(i), WeaponIdToBaseClipSize(wid));
    self.ammo_shells = self.ammo_nails = self.ammo_rockets = self.ammo_cells = 100;
    self.ammo_lava_nails = self.ammo_multi_rockets = self.ammo_plasma = 100;
    W_SetCurrentAmmoFor(h);
    VRSetAttackFinished(h, 0);
    if(wid == WID_GRUNTGUN || wid == WID_ENFRIFLE || VR_IsADGun(wid)) { VR_EnemyGun_Attack(h, wid, 0, &self.attack_finished); }
    else if(i == 0)  { W_FireShotgun(h); }
    else if(i == 1)  { W_FireSuperShotgun(h); }
    else if(i == 2)  { W_FireSpikes(h, 0); }
    else if(i == 3)  { W_FireSuperSpikes(h); }
    else if(i == 4)  { W_FireGrenade(h); }
    else if(i == 5)  { W_FireRocket(h); }
    else if(i == 6)  { W_FireLightning(h); }
    else if(i == 7) // (its trigger held: it fires only then)
    {
        float trigger = self.button0;
        self.button0 = 1;
        HIP_FireLaser(h, 0);
        self.button0 = trigger;
    }
    else if(i == 8)  { W_FireProximityGrenade(h); }
    else if(i == 9)  { W_FireLavaSpikes(h, 0); }
    else if(i == 10) { W_FireSuperLavaSpikes(h); }
    else if(i == 11) { W_FireMultiGrenade(h); }
    else if(i == 12) { W_FireMultiRocket(h); }
    else if(i == 13) { W_FirePlasma(h); }
};

string(float b) VR_DCTest_Hit = { return b ? "hit" : "refused"; };

void() VR_DCTest_Think =
{
    entity p = vr_dctest_player;
    entity oldSelf = self;
    float i = vr_dctest_index;
    self.nextthink = time + 0.1;
    if(vr_dctest_ticks == 0) // (where it started, a blast's push undone; the hands there by the next tick)
    {
        setorigin(p, vr_dctest_home);
        p.velocity = '0 0 0';
        VR_DCTest_ClearShots(p);
    }
    else if(vr_dctest_ticks == 1)
    {
        self = p;
        float wid = VR_DCTest_Wid(i);
        // (a rocket's blast well away from the player; a grenade's arc not yet over the target)
        float dist = 120;
        if(wid == WID_ROCKET_LAUNCHER || wid == WID_ADROCKET) { dist = 200; }
        else if(wid == WID_GRENADE_LAUNCHER || wid == WID_ADGRENADE || wid == WID_PROXIMITY_GUN) { dist = 72; }
        VR_DCTest_Place(dist, vr_dctest_pass ? VR_DCTest_Last(i) : VR_DCTest_Expect(i));
        p.damage_weapon = VR_DCTest_Last(i); // (the last blow another weapon's)
        vr_dctest_hit = FALSE;
        if(!vr_dctest_pass) { vr_dctest_by = ""; } // (what struck it in either pass)
        float clip = VRGetEntWeaponClip(p, cVR_MainHand);
        VR_DCTest_Fire(i);
        if(cvar("developer"))
        {
            dprint(sprintf("dmgcredit: %s pass %g: muzzle %v aim %v target %v, clip %g -> %g\n", VR_DCTest_Name(i),
                vr_dctest_pass, VRGetMuzzlePos(cVR_MainHand), VRGetWeaponFireRotFwd(cVR_MainHand),
                vr_dctest_target.origin, clip, VRGetEntWeaponClip(p, cVR_MainHand)));
        }
        self = oldSelf;
    }
    vr_dctest_ticks = vr_dctest_ticks + 1;
    if(vr_dctest_ticks < 30) // (3 s: a grenade's fuse)
    {
        return;
    }
    vr_dctest_ticks = 0;
    if(vr_dctest_pass == 0)
    {
        vr_dctest_ownhit = vr_dctest_hit;
        vr_dctest_pass = 1;
        return;
    }
    float own = VR_DCTest_Expect(i), last = VR_DCTest_Last(i);
    float credited = -1; // (-1: neither or both: not one weapon)
    if(vr_dctest_ownhit && !vr_dctest_hit) { credited = own; }
    else if(!vr_dctest_ownhit && vr_dctest_hit) { credited = last; }
    else if(!vr_dctest_ownhit && !vr_dctest_hit) { credited = 0; }
    float ok = credited == own;
    bprint(sprintf("dmgcredit: %s: own %g %s, last %g %s", VR_DCTest_Name(i), own, VR_DCTest_Hit(vr_dctest_ownhit), last,
        VR_DCTest_Hit(vr_dctest_hit)));
    bprint(sprintf(" (by %s): credited %g: %s\n", vr_dctest_by, credited, ok ? "PASS" : "FAIL"));
    if(ok) { vr_dctest_passed++; } else { vr_dctest_failed++; }
    vr_dctest_pass = 0;
    vr_dctest_index = vr_dctest_index + 1;
    while(vr_dctest_index < vr_dctest_end && !VR_DCTest_Available(vr_dctest_index))
    {
        bprint(sprintf("dmgcredit: %s: not there, skipped\n", VR_DCTest_Name(vr_dctest_index)));
        vr_dctest_index = vr_dctest_index + 1;
    }
    if(vr_dctest_index >= vr_dctest_end)
    {
        bprint(sprintf("dmgcredit: %g passed, %g failed\n", vr_dctest_passed, vr_dctest_failed));
        VR_DCTest_ClearShots(p);
        p.flags = p.flags - (p.flags & FL_GODMODE) + vr_dctest_godmode;
        cvar_set("vr_weapon_grip_mode", ftos(vr_dctest_grip));
        remove(vr_dctest_target);
        vr_dctest_target = world;
        remove(self);
    }
};

// Every server frame (StartFrame): the test asked for (vr_dmgcredit_test).
void() VR_DCTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_dmgcredit_test);
    if(!ask)
    {
        return;
    }
    cvar_set("vr_dmgcredit_test", "0");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    vr_dctest_player = p;
    vr_dctest_pass = vr_dctest_ticks = vr_dctest_passed = vr_dctest_failed = 0;
    vr_dctest_index = 0;
    vr_dctest_end = VR_DCTEST_COUNT;
    if(ask >= 100 && ask < 100 + VR_DCTEST_COUNT) // (one weapon's)
    {
        vr_dctest_index = ask - 100;
        vr_dctest_end = vr_dctest_index + 1;
    }
    if(!VR_DCTest_Available(vr_dctest_index))
    {
        bprint(sprintf("dmgcredit: %s: not there, skipped\n", VR_DCTest_Name(vr_dctest_index)));
        return;
    }
    vr_dctest_home = p.origin;
    vr_dctest_godmode = p.flags & FL_GODMODE;
    vr_dctest_grip = cvar("vr_weapon_grip_mode");
    cvar_set("vr_weapon_grip_mode", "1");
    p.flags = p.flags | FL_GODMODE; // (the blasts at the target: the player unhurt)
    entity d = spawn();
    d.classname = "vr_dmgcredit_test";
    d.think = VR_DCTest_Think;
    d.nextthink = time + 0.1;
};
