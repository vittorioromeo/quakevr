#ifdef QVR_TESTS
// dw_test.qc -- Dwell support (DWELL.md P2a): vr_ad_test 200, the map's Dwell boss fought to its end (d1end's Awoken,
// d2end's Sculptor): woken as the map would (the Sculptor used, as its fight_phase1 relay does; the Awoken shown its
// enemy), then a killing blow each time it can be hurt, through every phase, until it dies. The Sculptor rests 3 s
// between phases here (its `wait`: 40 in the map). Prints "dwbosstest: ..." lines and one verdict:
// "dwbosstest: <classname> phases <n> died <0|1>: PASS|FAIL".

.float dw_test_phases, dw_test_started;

void() DW_BossTest_Think =
{
    entity b = self.enemy;
    entity p = self.owner;
    self.nextthink = time + 0.1;
    float over = !b || b.classname != self.netname || b.health <= 0; // (gone, or another entity in its slot)
    if(over || time > self.dw_test_started + 600)
    {
        float died = over;
        if(b) { bprint(sprintf("dwbosstest: (%s health %g state %g)\n", b.classname, b.health, b.state)); }
        bprint(sprintf("dwbosstest: %s phases %g died %g: %s\n", self.netname, self.dw_test_phases, died,
            (died && self.dw_test_phases >= 3) ? "PASS" : "FAIL"));
        remove(self);
        return;
    }
    entity oself = self;
    self = b;
    activator = p;
    if(self.classname == "monster_d2boss" && self.state == 0)
    {
        bprint("dwbosstest: the Sculptor used\n");
        self.use();
    }
    else if(self.classname == "monster_d1boss" && !self.takedamage && self.frame >= 65 && self.frame <= 87)
    {
        self.enemy = p; // (shown its enemy: it stops channelling)
        self.th_run();
    }
    if(self.takedamage && self.health > 0)
    {
        float before = self.state & 15;
        T_Damage(self, p, p, self.health + 1);
        if(self.health > 0)
        {
            oself.dw_test_phases = oself.dw_test_phases + 1;
            bprint(sprintf("dwbosstest: phase %g over (state %g, was %g)\n", oself.dw_test_phases, self.state & 15, before));
        }
    }
    self = oself;
};

void(entity p) DW_BossTest =
{
    entity b = find(world, classname, "monster_d1boss");
    if(!b) { b = find(world, classname, "monster_d2boss"); }
    if(!b || !p)
    {
        bprint("dwbosstest: no boss (or no player) here: FAIL\n");
        return;
    }
    bprint(sprintf("dwbosstest: %s health %g fight\n", b.classname, b.health));
    if(b.classname == "monster_d2boss") { b.wait = 3; } // (after the map's killboss, 2 s after a phase)
    entity t = spawn();
    t.classname = "dw_bosstest";
    t.netname = b.classname;
    t.enemy = b;
    t.owner = p;
    t.dw_test_started = time;
    t.think = DW_BossTest_Think;
    t.nextthink = time + 0.5;
};

// ---------------------------------------------------------------------------------------------------------------------
// vr_ad_test 201-202 (DWELL.md P3): Dwell's weapons as ours. 201: the Rotary Shotgun in the main hand, the Crystal Lance
// in the off hand, the Rifle in holster 0 (with ammo). Then a level change, or a save and load (Misc/quakevr/
// dwell_weapons_test.sh). 202: each still Dwell's (its record's variant: parm76, or the save), then each fired once at
// grunts held on its line (health 1000, frozen): the Rifle 50 to one, the Rotary Shotgun some of its 20 pellets of 2 to
// one, the Crystal Lance 150 to each of two in a row (its pierce). The script levels the mock's hands (they point at
// the floor) and turns positional damage off (the numbers exact). 203: 202's checks of the three kept, no shots. "dwweapons: N passed, M failed".

entity dw_wt_a, dw_wt_b;
float dw_wt_frames, dw_wt_step;

void(entity p) DW_WeaponsTest_Give =
{
    entity os = self;
    self = p;
    VR_WeaponUtil_EntAddWeapon(p, WID_SUPER_SHOTGUN);
    VR_WeaponUtil_EntAddWeapon(p, WID_LIGHTNING);
    VR_WeaponUtil_EntAddWeapon(p, WID_SHOTGUN);
    VRGiveEntWeapon(p, cVR_MainHand, WID_SUPER_SHOTGUN, 0, WeaponIdToBaseClipSize(WID_SUPER_SHOTGUN));
    p.weaponinst.wi_dwvar = DW_VAR_ROTARY;
    VRGiveEntWeapon(p, cVR_OffHand, WID_LIGHTNING, 0, WeaponIdToBaseClipSize(WID_LIGHTNING));
    p.weaponinst2.wi_dwvar = DW_VAR_LANCE;
    VR_Scene_HoldGiven(p, cVR_MainHand);
    VR_Scene_HoldGiven(p, cVR_OffHand);
    entity inst = WeaponInst_New(WeaponIdToBaseClipSize(WID_SHOTGUN));
    inst.wi_dwvar = DW_VAR_RIFLE;
    p.holsterweapon0 = WID_SHOTGUN; p.holsterweaponflags0 = 0; p.holsterweaponinst0 = inst;
    p.holsterweaponmodel0 = WeaponIdToModel(WID_SHOTGUN, 0);
    p.ammo_shells = 100;
    p.ammo_cells = 100;
    self = os;
    bprint(sprintf("dwweapons: given: main %g var %g, off %g var %g, holster0 %g var %g, parms %g\n", p.weapon,
        p.weaponinst.wi_dwvar, p.weapon2, p.weaponinst2.wi_dwvar, p.holsterweapon0, inst.wi_dwvar, DW_PackWeaponVariants(p)));
};

// A target grunt held at `at` (its middle), frozen and at 1000 health.
void(entity g, vector at) DW_WeaponsTest_Hold =
{
    setorigin(g, at - '0 0 8');
    g.velocity = '0 0 0';
    g.movetype = MOVETYPE_NONE;
    g.think = SUB_Null; // (frozen: no animation, no pain's steps off the line)
    g.th_pain = SUB_NullPain;
    g.nextthink = time + 5;
    g.health = 1000;
};

// The weapon in `xHand`'s line: the grunts held 64 and 128 units ahead of its muzzle (a frame before its shot: the
// monsters' precise hit models follow a move a frame later).
void(entity p, float xHand) DW_WeaponsTest_Place =
{
    entity os = self;
    self = p;
    vector src = VRGetMuzzlePos(xHand);
    vector dir = VRGetWeaponFireRotFwd(xHand);
    self = os;
    DW_WeaponsTest_Hold(dw_wt_a, src + dir * 64);
    DW_WeaponsTest_Hold(dw_wt_b, src + dir * 128);
};

// One shot of the weapon in `xHand` (Dwell's variant).
void(entity p, float xHand) DW_WeaponsTest_Shoot =
{
    entity os = self;
    self = p;
    dw_wt_a.health = dw_wt_b.health = 1000;
    float fired = DW_FireVariant(xHand, &self.attack_finished);
    self = os;
    VR_Test_Check(fired, sprintf("hand %g's variant %g fired", xHand, VRGetEntWeaponInst(p, xHand).wi_dwvar));
};

// Main hand <-> holster 0 (the records with their weapons: a draw from the holster, as the holster's own swap).
void(entity p) DW_WeaponsTest_SwapHolster =
{
    entity keep = p.weaponinst;
    float w = p.weapon;
    p.weaponinst = p.holsterweaponinst0;
    p.weapon = p.holsterweapon0;
    p.holsterweaponinst0 = keep;
    p.holsterweapon0 = w;
    VRSetEntWeaponClip(p, cVR_MainHand, 1);
};

// Fresh targets for the next weapon (a grunt hit hard goes into our knockdown and off the line, frozen or not).
void(entity p) DW_WeaponsTest_Targets =
{
    if(dw_wt_a) { remove(dw_wt_a); }
    if(dw_wt_b) { remove(dw_wt_b); }
    makevectors('0 1 0' * p.angles_y);
    dw_wt_a = VR_Test_Spawn(world, 0, p.origin + v_forward * 200 + '0 0 64', p.angles_y + 180);
    dw_wt_b = VR_Test_Spawn(world, 0, p.origin + v_forward * 260 + '0 0 64', p.angles_y + 180);
};

// Frames (each once the targets can be hurt): 0 the Rifle drawn, placed; 1 fired; 2 checked, new targets; 3 the
// Rotary Shotgun drawn back, placed; 4 fired; 5 checked, new targets; 6 the Lance's line placed; 7 fired; 8 checked.
void() DW_WeaponsTest_Think =
{
    entity p = self.owner;
    self.nextthink = time;
    dw_wt_frames++;
    if(dw_wt_frames > 400)
    {
        VR_Test_Check(FALSE, "timed out");
        VR_Test_Verdict();
        remove(self);
        return;
    }
    if(!dw_wt_a.takedamage || !dw_wt_b.takedamage) { return; } // (their monster start: a frame or two)
    float da = 1000 - dw_wt_a.health, db = 1000 - dw_wt_b.health;
    if(dw_wt_step == 0 || dw_wt_step == 3) { DW_WeaponsTest_SwapHolster(p); DW_WeaponsTest_Place(p, cVR_MainHand); }
    else if(dw_wt_step == 1 || dw_wt_step == 4) { DW_WeaponsTest_Shoot(p, cVR_MainHand); }
    else if(dw_wt_step == 2)
    {
        // (50 through our damage: an unaware target's bonus makes it 63)
        VR_Test_Check(da >= 50 && da <= 75 && db == 0, sprintf("the Rifle: one 50 bullet to the first (%g), none past it (%g)", da, db));
        DW_WeaponsTest_Targets(p);
    }
    else if(dw_wt_step == 5)
    {
        // (20 pellets of 2 at most: 40, 50 into an unaware target)
        VR_Test_Check(da >= 2 && da <= 50 && db <= 50, sprintf("the Rotary Shotgun: pellets of 2, 20 at most (%g, %g)", da, db));
        DW_WeaponsTest_Targets(p);
    }
    else if(dw_wt_step == 6) { VRSetEntWeaponClip(p, cVR_OffHand, 6); DW_WeaponsTest_Place(p, cVR_OffHand); }
    else if(dw_wt_step == 7) { DW_WeaponsTest_Shoot(p, cVR_OffHand); }
    else
    {
        // (150 each through our damage: 188 to an unaware target, as the Rifle's 63)
        VR_Test_Check(da >= 150 && da <= 188 && db == da, sprintf("the Crystal Lance: 150 to each, through the first (%g, %g)", da, db));
        VR_Test_Verdict();
        remove(dw_wt_a);
        remove(dw_wt_b);
        remove(self);
        return;
    }
    dw_wt_step++;
};

void(entity p, float xRequest) DW_WeaponsTest =
{
    if(xRequest == 201)
    {
        DW_WeaponsTest_Give(p);
        return;
    }
    VR_Test_BeginFlags("dwweapons", VR_TEST_LOUD);
    VR_Test_Check(p.weapon == WID_SUPER_SHOTGUN && p.weaponinst.wi_dwvar == DW_VAR_ROTARY,
        sprintf("the Rotary Shotgun still in the main hand (weapon %g, variant %g)", p.weapon, p.weaponinst.wi_dwvar));
    VR_Test_Check(p.weapon2 == WID_LIGHTNING && p.weaponinst2.wi_dwvar == DW_VAR_LANCE,
        sprintf("the Crystal Lance still in the off hand (weapon %g, variant %g)", p.weapon2, p.weaponinst2.wi_dwvar));
    VR_Test_Check(p.holsterweapon0 == WID_SHOTGUN && p.holsterweaponinst0.wi_dwvar == DW_VAR_RIFLE,
        sprintf("the Rifle still in holster 0 (weapon %g, variant %g)", p.holsterweapon0, p.holsterweaponinst0.wi_dwvar));
    if(xRequest == 203)
    {
        VR_Test_Verdict(); // (kept only: e1m2's start has no room for the shots)
        return;
    }
    p.flags = p.flags | FL_GODMODE | FL_NOTARGET;
    p.ammo_shells = 100;
    p.ammo_cells = 100;
    dw_wt_a = dw_wt_b = world;
    DW_WeaponsTest_Targets(p);
    dw_wt_frames = 0;
    dw_wt_step = 0;
    entity t = spawn();
    t.classname = "dw_weaponstest";
    t.owner = p;
    t.think = DW_WeaponsTest_Think;
    t.nextthink = time;
};
#endif
