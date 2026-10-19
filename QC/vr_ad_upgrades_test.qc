// vr_ad_upgrades_test.qc -- AD's upgraded weapons' rules through our weapons, in any map while AD's data is installed:
// 140 the Shadow Axe's, 141 the Plasma Gun's cell (one "adplasma:" line per check).
//
// 140 (Debug > Tests > Arcane Dimensions: Shadow Axe Rules Test): AD's rules that gate on
// the Shadow Axe, through our VR axe's own path, in any map while AD's data is installed. AD's QC asks the Shadow Axe in
// five places: its blow (35 for the axe's 20: AD_AxeScale), the mark it leaves (.axhitme 2: a zombie in pain gibbed for
// good, ad_zombie_pain; a monster's long pain, ad_monster_pain_check), the bodies lying on the floor it chops
// (ad_monster_flrbody_gib) and the triggers' upgrade_axe key (the player carries it). 140 makes three AD zombies and an
// AD knight ahead; two seconds on (their AD start done) each check runs: W_FireAxe's blow as a melee contact lands it
// (the blow's contact set: VR_Blow_Contact), VR_Melee_Strike's chop as a swing through a downed zombie. One
// "adshadowaxe:" line per check (PASS or FAIL), then the count.

entity vr_adsa_z1, vr_adsa_z2, vr_adsa_z3, vr_adsa_k;
float vr_adsa_pass, vr_adsa_fail;

void(string xWhat, float xOk, string xGot) AD_ShadowAxeCheck =
{
    bprint(sprintf("adshadowaxe: %s: %s (%s)\n", xWhat, xOk ? "PASS" : "FAIL", xGot));
    if(xOk) { vr_adsa_pass = vr_adsa_pass + 1; }
    else { vr_adsa_fail = vr_adsa_fail + 1; }
};

void(string xWhat, float xOk, string xGot) AD_PlasmaCheck =
{
    bprint(sprintf("adplasma: %s: %s (%s)\n", xWhat, xOk ? "PASS" : "FAIL", xGot));
    if(xOk) { vr_adsa_pass = vr_adsa_pass + 1; }
    else { vr_adsa_fail = vr_adsa_fail + 1; }
};

// The axe in p's main hand, with AD's upgrade (the Shadow Axe) or without.
void(entity p, float xShadow) AD_ShadowAxeGive =
{
    if(p.weapon != WID_AXE)
    {
        VRGiveEntWeapon(p, cVR_MainHand, WID_AXE, 0, WeaponIdToBaseClipSize(WID_AXE));
    }
    p.weaponinst.wi_adup = xShadow ? ad_IT_UPGRADE_AXE : 0;
};

// W_FireAxe's blow of p's main hand landing on `e` (as a swing's contact: VR_Blow_Contact), self p.
void(entity p, entity e) AD_ShadowAxeBlow =
{
    entity os = self;
    self = p;
    entity b = VR_Blow(cVR_MainHand); // (p's)
    self = os;
    vector at = (e.absmin + e.absmax) * 0.5;
    vector dir = normalize(at - (p.origin + p.view_ofs));
    b.mh_c_ent = e;
    b.mh_c_pos = at - dir * 4;
    b.mh_c_dir = dir;
    b.mh_c_normal = dir * -1;
    b.mh_strength = 1;
    self = p;
    VRSetMeleeHitSoundPlayed(cVR_MainHand, FALSE);
    if(cvar("developer"))
    {
        bprint(sprintf("adshadowaxe: blow on %s: tracked %g human %g contact %g inpain %g\n", e.classname, VRHandsTracked(p),
            p.ishuman, VR_Blow_Contact(cVR_MainHand), e.ad_inpain));
    }
    W_FireAxe(cVR_MainHand);
    if(cvar("developer"))
    {
        bprint(sprintf("adshadowaxe: after: mark %g inpain %g health %g\n", e.ad_axhitme, e.ad_inpain, e.health));
    }
    self = os;
    b.mh_c_ent = world;
    b.mh_c_pos = '0 0 0';
};

float(entity e) AD_ShadowAxeGone =
{
    return e.health <= 0 || e.deadflag > 0 || e.model == "" || e.modelindex == 0;
};

void() AD_ShadowAxeTestRun =
{
    entity p = self.owner;
    entity os = self;
    vr_adsa_pass = 0;
    vr_adsa_fail = 0;

    // Its blow: AD's 35 for the axe's 20.
    self = p;
    AD_ShadowAxeGive(p, TRUE);
    float up = AD_AxeScale(cVR_MainHand);
    AD_ShadowAxeGive(p, FALSE);
    float plain = AD_AxeScale(cVR_MainHand);
    self = os;
    AD_ShadowAxeCheck("the blow (AD's 35 for 20)", up == ad_DAMAGE_PLAYAXE2 / ad_DAMAGE_PLAYAXE1 && plain == 1,
        sprintf("x%g, the axe's x%g", up, plain));

    // A zombie in pain, hit by the Shadow Axe: gibbed for good (ad_zombie_pain); by the plain axe: gets up again.
    AD_ShadowAxeGive(p, TRUE);
    vr_adsa_z1.ad_inpain = 1;
    AD_ShadowAxeBlow(p, vr_adsa_z1);
    AD_ShadowAxeCheck("a zombie in pain, the Shadow Axe's blow: gibbed", AD_ShadowAxeGone(vr_adsa_z1),
        sprintf("mark %g, health %g, deadflag %g", vr_adsa_z1.ad_axhitme, vr_adsa_z1.health, vr_adsa_z1.deadflag));
    AD_ShadowAxeGive(p, FALSE);
    vr_adsa_z2.ad_inpain = 1;
    AD_ShadowAxeBlow(p, vr_adsa_z2);
    AD_ShadowAxeCheck("a zombie in pain, the axe's blow: not gibbed", !AD_ShadowAxeGone(vr_adsa_z2),
        sprintf("health %g, deadflag %g", vr_adsa_z2.health, vr_adsa_z2.deadflag));

    // A monster's long pain (ad_monster_pain_check, its import hook): the Shadow Axe's mark, not the axe's.
    float longanim = vr_adsa_k.ad_pain_longanim;
    self = p;
    AD_ShadowAxeGive(p, TRUE);
    AD_AxeMark(cVR_MainHand, vr_adsa_k);
    self = vr_adsa_k;
    vr_adsa_k.ad_pain_finstate = time;
    ad_monster_pain_check(p, 10);
    float withShadow = vr_adsa_k.ad_pain_check;
    self = p;
    AD_ShadowAxeGive(p, FALSE);
    AD_AxeMark(cVR_MainHand, vr_adsa_k);
    self = vr_adsa_k;
    ad_monster_pain_check(p, 10);
    float withAxe = vr_adsa_k.ad_pain_check;
    self = os;
    AD_ShadowAxeCheck("a knight's long pain: the Shadow Axe's only", longanim && withShadow == 2 && withAxe != 2,
        sprintf("long animation %g, Shadow Axe %g, axe %g", longanim, withShadow, withAxe));

    // A downed zombie (on the floor: ad_bodyonflr), a swing of the Shadow Axe through it: chopped (VR_Melee_Strike).
    self = vr_adsa_z3;
    ad_zombie_onground(ad_MONAI_ZOMBIEUPB);
    self = os;
    float down = vr_adsa_z3.ad_bodyonflr == ad_MON_ONFLR;
    self = p;
    entity b = VR_Blow(cVR_MainHand); // (p's)
    self = os;
    float n = b.mh_n;
    vector pp = b.mh_pp[0], cp = b.mh_cp[0];
    b.mh_n = 1;
    b.mh_pp[0] = vr_adsa_z3.origin + '-24 0 -8';
    b.mh_cp[0] = vr_adsa_z3.origin + '24 0 -8';
    self = p;
    AD_ShadowAxeGive(p, FALSE);
    AD_ShadowAxeChop(cVR_MainHand, b); // the plain axe: nothing
    float keptByAxe = vr_adsa_z3.ad_bodyonflr == ad_MON_ONFLR;
    AD_ShadowAxeGive(p, TRUE);
    AD_ShadowAxeChop(cVR_MainHand, b);
    self = os;
    b.mh_n = n;
    b.mh_pp[0] = pp;
    b.mh_cp[0] = cp;
    AD_ShadowAxeCheck("a zombie on the floor, the Shadow Axe's swing through it: chopped",
        down && keptByAxe && vr_adsa_z3.ad_bodyonflr != ad_MON_ONFLR && AD_ShadowAxeGone(vr_adsa_z3),
        sprintf("down %g, the axe left it %g, health %g", down, keptByAxe, vr_adsa_z3.health));

    // The triggers' upgrade_axe key (triggers_states.qc's hook): the Shadow Axe carried, then not (the holsters' own
    // Shadow Axes set aside meanwhile: Arcane Dimensions: Shadow Axe in Hand holsters one).
    float holstered[8];
    for(float i = 1; i < 8; i++)
    {
        entity inst = AD_CarriedInst(p, i);
        holstered[i] = inst != world ? (inst.wi_adup & ad_IT_UPGRADE_AXE) : 0;
        if(inst != world) { inst.wi_adup = inst.wi_adup - holstered[i]; }
    }
    AD_ShadowAxeGive(p, TRUE);
    float carried = VR_AD_HasUpgrade(p, ad_IT_UPGRADE_AXE);
    AD_ShadowAxeGive(p, FALSE);
    float notCarried = VR_AD_HasUpgrade(p, ad_IT_UPGRADE_AXE);
    for(float j = 1; j < 8; j++)
    {
        entity back = AD_CarriedInst(p, j);
        if(back != world) { back.wi_adup = back.wi_adup | holstered[j]; }
    }
    AD_ShadowAxeCheck("a trigger's upgrade_axe key: the Shadow Axe carried", carried && !notCarried,
        sprintf("carried %g, without it %g", carried, notCarried));

    bprint(sprintf("adshadowaxe: %g passed, %g failed\n", vr_adsa_pass, vr_adsa_fail));
    AD_ShadowAxeGive(p, TRUE);
    if(vr_adsa_k != world) { remove(vr_adsa_k); } // (the second zombie stays: to try the Shadow Axe on)
    remove(self);
};

// 141, two steps (the engine reports the drawn gun's well a frame on): the Plasma Gun in the off hand, the main hand free.
// Its cell's well (the engine's load point and the attached cell's box: vr_view.cpp magMounts, loadPorts: AdVrPlasma) is
// found; the way the cell leaves is out of the gun's inner side (the left hand's right: the model mirrored), not down; the
// cell ejected (as its button does); a cell taken from the pouch and brought to the load point goes in.
void() AD_PlasmaCellTestRun =
{
    entity p = self.owner;
    entity os = self;
    self = p;
    float h = cVR_OffHand;
    vr_adsa_pass = 0;
    vr_adsa_fail = 0;
    float found = p.offloadportpos != '0 0 0' && VR_Reload_HasMagBox(p, h);
    AD_PlasmaCheck("its cell's well is found", found && p.weaponmodel2 == "progs/v_plasma.mdl#vr",
        sprintf("load point %v, box %g, model %s", p.offloadportpos, VR_Reload_HasMagBox(p, h), p.weaponmodel2));
    vector out = VR_Reload_MagOutWay(p, h);
    makevectors(VRGetEntHandRot(p, h));
    float side = out * v_right, down = out * v_up * -1, ahead = out * v_forward;
    AD_PlasmaCheck("the cell leaves out of its inner side", side > 0.8,
        sprintf("right %.2f, down %.2f, ahead %.2f", side, down, ahead));
    float was = VR_Reload_MagIn(p, h);
    entity cell = world;
    if(was)
    {
        cell = VR_Reload_Eject(p, h, out * 40, -1, "test 141");
    }
    AD_PlasmaCheck("the cell ejected", was && !VR_Reload_MagIn(p, h) && cell != world, sprintf("was in %g", was));
    VR_Reload_PlayerFrame();
    VR_Reload_Take(cVR_MainHand);
    entity r = VR_Carry_Held(p, cVR_MainHand);
    if(r != world)
    {
        setorigin(r, r.origin + p.offloadportpos - VR_Reload_RoundRef(r));
        r.vr_ammo_fresh = FALSE;
        VR_Reload_HeldFrame(cVR_MainHand, r);
    }
    AD_PlasmaCheck("a cell brought to its load point goes in", r != world && VR_Reload_MagIn(p, h),
        sprintf("taken %g", r != world));
    bprint(sprintf("adplasma: %g passed, %g failed\n", vr_adsa_pass, vr_adsa_fail));
    self = os;
    remove(self);
};

// 149 (Debug > Tests > Arcane Dimensions: Widowmaker Tube Test): the Widowmaker in the off hand, empty, the main hand
// free. Its shell port (vr_view_internal.hpp loadPorts: AdVrWidowmaker, on the receiver's inner side) is found; it loads by
// hand; its tube holds vr_ad_widowmaker_tube; a pouch handful is vr_ad_widowmaker_handful shells, all going in at the
// port, until the tube is full; then the trigger held two seconds (W_OffHandAttack each frame its shot is due) fires a
// shell every vr_ad_widowmaker_refire; last, a Widowmaker pickup touched comes with its tube full. (Its spent shells' way out: the engine's "shells out" lines, vr_debug_weaponfx 1.)
float vr_adwm_tube, vr_adwm_end, vr_adwm_reserve;
void(string xWhat, float xOk, string xGot) AD_WidowCheck =
{
    bprint(sprintf("adwidow: %s: %s (%s)\n", xWhat, xOk ? "PASS" : "FAIL", xGot));
    if(xOk) { vr_adsa_pass = vr_adsa_pass + 1; }
    else { vr_adsa_fail = vr_adsa_fail + 1; }
};

float vr_adwm_t0, vr_adwm_first, vr_adwm_phase, vr_adwm_clip, vr_adwm_rest;

void() AD_WidowFireRun =
{
    entity p = self.owner;
    entity os = self;
    float spinup = max(0.01, cvar("vr_ad_widowmaker_spinup"));
    if(vr_adwm_phase == 1)
    {
        // The trigger let go 0.3 s (the barrels slowing), then pressed again: the first shot comes sooner than from rest.
        self = p;
        if(time < vr_adwm_rest)
        {
            AD_WidowmakerSpinFrame(cVR_OffHand, FALSE);
        }
        else
        {
            if(vr_adwm_t0 < 0)
            {
                vr_adwm_t0 = time;
                vr_adwm_clip = VRGetEntWeaponClip(p, cVR_OffHand);
            }
            AD_WidowmakerSpinFrame(cVR_OffHand, TRUE);
            if(time >= p.offhand_attack_finished)
            {
                W_OffHandAttack();
            }
            if(VRGetEntWeaponClip(p, cVR_OffHand) < vr_adwm_clip)
            {
                float d = time - vr_adwm_t0;
                AD_WidowCheck("pressed again while still spinning, it fires sooner", d > 0.02 && d < spinup * 0.7,
                    sprintf("first shot %g s after the press; from rest %g s (spin-up %g)", d, vr_adwm_first, spinup));
                vr_adwm_phase = 2;
            }
        }
        self = os;
        self.nextthink = time + 0.01;
        if(vr_adwm_phase != 2)
        {
            return;
        }
    }
    else if(time < vr_adwm_end)
    {
        self = p;
        AD_WidowmakerSpinFrame(cVR_OffHand, TRUE);
        if(time >= p.offhand_attack_finished)
        {
            W_OffHandAttack();
        }
        if(vr_adwm_first < 0 && VRGetEntWeaponClip(p, cVR_OffHand) < vr_adwm_tube)
        {
            vr_adwm_first = time - vr_adwm_t0;
        }
        self = os;
        self.nextthink = time + 0.01;
        return;
    }
    if(vr_adwm_phase == 0)
    {
        float shots = vr_adwm_tube - VRGetEntWeaponClip(p, cVR_OffHand);
        float rate = max(0.02, cvar("vr_ad_widowmaker_refire"));
        float expect = floor((2 - spinup) / rate) + 1;
        AD_WidowCheck("first shot waits for the barrels: from rest about the spin-up time", vr_adwm_first >= spinup - 0.05 && vr_adwm_first <= spinup + 0.1,
            sprintf("%g s after the press, spin-up %g", vr_adwm_first, spinup));
        AD_WidowCheck("full auto: the trigger held 2 s fires a shell each refire after the spin-up", shots >= expect - 1 && shots <= expect + 1,
            sprintf("%g shots, %g expected at %g s; %g left in the tube", shots, expect, rate, VRGetEntWeaponClip(p, cVR_OffHand)));
        vr_adwm_phase = 1;
        vr_adwm_t0 = -1;
        vr_adwm_rest = time + 0.3;
        self.nextthink = time + 0.01;
        return;
    }
    AD_WidowCheck("its shots come from the tube, not the reserve", p.ammo_shells == vr_adwm_reserve,
        sprintf("reserve %g, was %g", p.ammo_shells, vr_adwm_reserve));
    // Last, a Widowmaker pickup (weapon_touch as the body's touch, both hands free): it comes with its tube full.
    self = p;
    VRGiveEntWeapon(p, cVR_MainHand, WID_FIST, 0, 0);
    VRGiveEntWeapon(p, cVR_OffHand, WID_FIST, 0, 0);
    p.ammo_shells = 20;
    entity pk = spawn();
    pk.classname = "weapon_supershotgun";
    pk.netname = "Widowmaker";
    pk.vr_ad_upgrade = ad_IT_UPGRADE_SSG;
    float touching = p.touchinghand;
    p.touchinghand = cVR_FakeHand;
    self = pk;
    other = p;
    weapon_touch();
    p.touchinghand = touching;
    remove(pk);
    self = os;
    float tube = rint(cvar("vr_ad_widowmaker_tube"));
    float got = AD_WidowmakerIn(p, cVR_MainHand) ? VRGetEntWeaponClip(p, cVR_MainHand) : -1;
    AD_WidowCheck("a pickup comes with its tube full", got == tube,
        sprintf("%g in its tube of %g (-1: no Widowmaker in hand); the reserve 20 -> %g", got, tube, p.ammo_shells));
    bprint(sprintf("adwidow: %g passed, %g failed\n", vr_adsa_pass, vr_adsa_fail));
    vr_ad_wspin_test = FALSE;
    remove(self);
};

void() AD_WidowTubeTestRun =
{
    entity p = self.owner;
    entity os = self;
    self = p;
    float h = cVR_OffHand;
    vr_adsa_pass = 0;
    vr_adsa_fail = 0;
    AD_WidowCheck("its shell port is found", p.offloadportpos != '0 0 0' && p.weaponmodel2 == "progs/v_shot3.mdl#vr",
        sprintf("load point %v, model %s", p.offloadportpos, p.weaponmodel2));
    AD_WidowCheck("it loads by hand (never broken open)", VR_Reload_ManualInHand(p, h) && !VR_Reload_SsgBreaks(p, h),
        sprintf("manual %g", VR_Reload_ManualInHand(p, h)));
    vr_adwm_tube = VR_ClipSizeIn(p, h);
    AD_WidowCheck("its tube's size", vr_adwm_tube == rint(cvar("vr_ad_widowmaker_tube")), sprintf("%g", vr_adwm_tube));
    float handful = rint(cvar("vr_ad_widowmaker_handful"));
    float loads = 0, first = -1, lastClip = 0, steps = 0;
    for(float i = 0; i < 12 && VRGetEntWeaponClip(p, h) < vr_adwm_tube; i++)
    {
        VR_Reload_PlayerFrame();
        VR_Reload_Take(cVR_MainHand);
        entity r = VR_Carry_Held(p, cVR_MainHand);
        if(r == world)
        {
            break;
        }
        if(first < 0)
        {
            first = r.vr_ammo_count;
        }
        setorigin(r, r.origin + p.offloadportpos - VR_Reload_RoundRef(r));
        r.vr_ammo_fresh = FALSE;
        float was = VRGetEntWeaponClip(p, h);
        VR_Reload_HeldFrame(cVR_MainHand, r);
        if(VRGetEntWeaponClip(p, h) > was)
        {
            loads = loads + 1;
            steps = max(steps, VRGetEntWeaponClip(p, h) - was);
        }
        lastClip = VRGetEntWeaponClip(p, h);
    }
    AD_WidowCheck("the pouch gives a handful for it", first == handful, sprintf("%g shells, a handful %g", first, handful));
    AD_WidowCheck("a handful at its port goes in at once", steps == handful, sprintf("most in one insert %g", steps));
    AD_WidowCheck("its tube fills", lastClip == vr_adwm_tube && loads == ceil(vr_adwm_tube / handful),
        sprintf("%g of %g after %g inserts", lastClip, vr_adwm_tube, loads));
    vr_adwm_tube = VRGetEntWeaponClip(p, h);
    vr_adwm_reserve = p.ammo_shells;
    self = os;
    vr_adwm_end = time + 2;
    vr_adwm_t0 = time;
    vr_adwm_first = -1;
    vr_adwm_phase = 0;
    vr_ad_wspin_test = TRUE;
    self.think = AD_WidowFireRun;
    self.nextthink = time + 0.01;
};

void(float request, entity p) AD_UpgradesTest =
{
    if(request == 149)
    {
        entity os2 = self;
        self = p;
        VR_WeaponUtil_EntAddWeapon(p, WID_SUPER_SHOTGUN);
        VRGiveEntWeapon(p, cVR_OffHand, WID_SUPER_SHOTGUN, 0, 0);
        p.weaponinst2.wi_adup = ad_IT_UPGRADE_SSG;
        p.weaponmodel2 = WeaponIdToModel(WID_SUPER_SHOTGUN, 0);
        VRGiveEntWeapon(p, cVR_MainHand, WID_FIST, 0, 0);
        p.ammo_shells = 100;
        AD_SyncWeaponModels(p);
        self = os2;
        entity t2 = spawn();
        t2.classname = "vr_adsa_timer";
        t2.owner = p;
        t2.think = AD_WidowTubeTestRun;
        t2.nextthink = time + 0.5;
        bprint("adwidow: the Widowmaker in the off hand, empty; the checks in 0.5 s\n");
        return;
    }
    if(request == 141)
    {
        entity os = self;
        self = p;
        VR_WeaponUtil_EntAddWeapon(p, WID_LIGHTNING);
        VRGiveEntWeapon(p, cVR_OffHand, WID_LIGHTNING, 0, WeaponIdToBaseClipSize(WID_LIGHTNING));
        p.weaponinst2.wi_adup = ad_IT_UPGRADE_LG;
        p.weaponmodel2 = WeaponIdToModel(WID_LIGHTNING, 0); // (AD_SyncWeaponModels: the upgraded one)
        VRGiveEntWeapon(p, cVR_MainHand, WID_FIST, 0, 0);
        p.ammo_cells = 100;
        AD_SyncWeaponModels(p);
        self = os;
        entity t = spawn();
        t.classname = "vr_adsa_timer";
        t.owner = p;
        t.think = AD_PlasmaCellTestRun;
        t.nextthink = time + 0.5;
        bprint("adplasma: the Plasma Gun in the off hand; the checks in 0.5 s\n");
        return;
    }
    if(request != 140)
    {
        return;
    }
    makevectors('0 1 0' * p.angles_y);
    vector flat = v_forward;
    vector side = v_right;
    vr_adsa_z1 = AD_TestSpawn("monster_zombie", ad_monster_zombie, p.origin + flat * 96 - side * 48);
    vr_adsa_z2 = AD_TestSpawn("monster_zombie", ad_monster_zombie, p.origin + flat * 96 + side * 48);
    vr_adsa_z3 = AD_TestSpawn("monster_zombie", ad_monster_zombie, p.origin + flat * 160);
    vr_adsa_k = AD_TestSpawn("monster_knight", ad_monster_knight, p.origin + flat * 224);
    entity t = spawn();
    t.classname = "vr_adsa_timer";
    t.owner = p;
    t.think = AD_ShadowAxeTestRun;
    t.nextthink = time + 2;
    bprint("adshadowaxe: three AD zombies and an AD knight ahead; the checks in 2 s\n");
};
