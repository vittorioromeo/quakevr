#ifdef QVR_TESTS
// dw_test.qc -- Dwell support (DWELL.md P2a): vr_ad_test 200, the map's Dwell boss fought to its end (d1end's Awoken,
// d2end's Sculptor): woken as the map would (the Sculptor used, as its fight_phase1 relay does; the Awoken shown its
// enemy), then a killing blow each time it can be hurt, through every phase, until it dies. The Sculptor rests 3 s
// between phases here (its `wait`: 40 in the map), and must go through all its motion's steps (flying, flat and
// shielded, teleporting, resting at its target2). Where the map has a target_bullethell (d2end), first the pattern
// sweep, once he has landed: its styles 1-8 fired one after another next to him (in god mode meanwhile; each when the
// last one's shots, tasks and marks are gone), each to spawn shots, the aimed line (1) to hit him, the edicts to stay
// within 400 of where they were and come back there.
// Prints "dwbosstest: ..." and "dwhelltest: ..." lines and one verdict:
// "dwbosstest: <classname> phases <n> died <0|1> motion <ok> patterns <ok>: PASS|FAIL".

.float dw_test_phases, dw_test_started, dw_test_seen;
.float dw_test_style, dw_test_emstyle, dw_test_god, dw_test_base, dw_test_peak, dw_test_fired, dw_test_shots0;
.float dw_test_hits, dw_test_hellok, dw_test_restok, dw_test_linehits;
.entity dw_test_em;
.vector dw_test_emorg;

float() DW_Test_Edicts =
{
    float n = 0;
    entity e = nextent(world);
    while(e)
    {
        n = n + 1;
        e = nextent(e);
    }
    return n;
};

// The sweep's emitter next to the player where he is now, at his height: the clearest of 8 ways from him (up to 320
// units, things in the way counted).
void(entity em, entity p) DW_HellSweep_Place =
{
    float k, best = -1;
    vector at = p.origin;
    for(k = 0; k < 8; k++)
    {
        traceline(p.origin, p.origin + DW_Yaw(k * 45) * 320, FALSE, p);
        if(trace_fraction > best)
        {
            best = trace_fraction;
            at = trace_endpos - DW_Yaw(k * 45) * 32;
        }
    }
    setorigin(em, at);
    bprint(sprintf("dwhelltest: emitter %g units from the player (at %v)\n", vlen(at - p.origin), p.origin));
};

// The pattern sweep's step (every 0.1 s); FALSE once it is over (or there is none).
float() DW_HellSweep_Think =
{
    entity em = self.dw_test_em;
    entity p = self.owner;
    if(!em || self.dw_test_style > 8) { return FALSE; }
    self.dw_test_peak = max(self.dw_test_peak, DW_Test_Edicts());
    if(find(world, classname, "dw_helltask") || find(world, classname, "dw_hellshot") || find(world, classname, "dw_hellmark"))
    {
        return TRUE;
    }
    if(!self.dw_test_style && !(p.flags & FL_ONGROUND)) { return TRUE; } // (d2end drops him in: on his feet first)
    if(self.dw_test_style > 0)
    {
        float n = dw_hell_shots - self.dw_test_shots0;
        if(self.dw_test_style == 1) { self.dw_test_linehits = dw_hell_hits - self.dw_test_hits; } // (aimed at him)
        bprint(sprintf("dwhelltest: style %g shots %g hits %g (edicts peak +%g)\n", self.dw_test_style, n,
            dw_hell_hits - self.dw_test_hits, self.dw_test_peak - self.dw_test_base));
        if(n > 0) { self.dw_test_fired = self.dw_test_fired + 1; }
    }
    self.dw_test_style = self.dw_test_style + 1;
    if(self.dw_test_style > 8)
    {
        float left = DW_Test_Edicts() - self.dw_test_base;
        float peak = self.dw_test_peak - self.dw_test_base;
        setorigin(em, self.dw_test_emorg);
        em.style = self.dw_test_emstyle;
        p.flags = (p.flags - (p.flags & FL_GODMODE)) | self.dw_test_god;
        float fine = self.dw_test_fired == 8 && self.dw_test_linehits > 0;
        float tidy = left <= 2 && peak < 400;
        self.dw_test_hellok = (fine && tidy);
        string word = "BAD";
        if(self.dw_test_hellok) { word = "ok"; }
        bprint(sprintf("dwhelltest: patterns %g/8 fired, the line's hits %g, hits %g, edicts peak +%g, left %g: %s\n",
            self.dw_test_fired, self.dw_test_linehits, dw_hell_hits, peak, left, word));
        return FALSE;
    }
    self.dw_test_shots0 = dw_hell_shots;
    self.dw_test_hits = dw_hell_hits;
    em.style = self.dw_test_style;
    DW_HellSweep_Place(em, p);
    entity oself = self;
    self = em;
    activator = p;
    em.use();
    self = oself;
    return TRUE;
};

void() DW_BossTest_Think =
{
    entity b = self.enemy;
    entity p = self.owner;
    self.nextthink = time + 0.1;
    if(DW_HellSweep_Think()) { return; }
    float over = !b || b.classname != self.netname || b.health <= 0; // (gone, or another entity in its slot)
    if(over || time > self.dw_test_started + 600)
    {
        float died = over;
        // the Sculptor: flying (4), flat and shielded (2), teleporting (3), resting (5) at its target2
        float motion = self.netname != "monster_d2boss" || ((self.dw_test_seen & 60) == 60 && self.dw_test_restok);
        float hell = !self.dw_test_em || self.dw_test_hellok;
        if(b) { bprint(sprintf("dwbosstest: (%s health %g state %g)\n", b.classname, b.health, b.state)); }
        bprint(sprintf("dwbosstest: states seen %g, rested at target2 %g\n", self.dw_test_seen, self.dw_test_restok));
        bprint(sprintf("dwbosstest: %s phases %g died %g motion %g patterns %g: %s\n", self.netname, self.dw_test_phases,
            died, motion, hell, (died && self.dw_test_phases >= 3 && motion && hell) ? "PASS" : "FAIL"));
        remove(self);
        return;
    }
    if(b.classname == "monster_d2boss" && b.state < 14)
    {
        float bit = 1;
        float k;
        for(k = 0; k < b.state; k++) { bit = bit * 2; }
        self.dw_test_seen = self.dw_test_seen | bit;
        entity dest = find(world, targetname, b.cu_target2);
        if(b.state == 5 && dest && vlen(b.origin - dest.origin) < 1) { self.dw_test_restok = TRUE; }    }
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
    entity em = find(world, classname, "target_bullethell");
    if(em) // (the pattern sweep first: its emitter next to the player, at his height)
    {
        t.dw_test_em = em;
        t.dw_test_emorg = em.origin;
        t.dw_test_emstyle = em.style;
        t.dw_test_god = p.flags & FL_GODMODE;
        p.flags = p.flags | FL_GODMODE;
        t.dw_test_base = DW_Test_Edicts();
        t.dw_test_peak = t.dw_test_base;
    }
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

// vr_ad_test 204 (DWELL.md P3): an armed grunt arming a Dwell weapon's record, as it takes one you threw (VR_AG_TakeUp
// hands the record to VR_AG_Arm): its prop named as Dwell's, the variant kept, and its shot Dwell's (DW_FireVariant
// through VR_AG_ShootProp: a shell for the Rotary Shotgun's, not the Widowmaker's; 6 cells for the Crystal Lance's), at a
// grunt held 128 units ahead of it. Frames: 0 spawned (fists), armed; 4 fired, checked; then the powerups' screen
// tints (Haste yellow, Haste and Berserk orange, none when they end). "dwgrunt: N passed, M failed".
entity dw_gt_g[2], dw_gt_t[2];
void() DW_GruntTest_Think =
{
    self.nextthink = time + 0.05;
    self.cnt = self.cnt + 1;
    if(self.cnt < 4) { return; }
    float k;
    for(k = 0; k < 2; k++)
    {
        entity g = dw_gt_g[k];
        entity pr = g.vr_ag_prop;
        entity inst = pr.weaponinst;
        float before = dw_variant_shots, clip = inst.wi_clip;
        entity oldself = self;
        self = g;
        g.enemy = dw_gt_t[k];
        pr.vr_ag_hk = 0;
        VR_AG_ShootProp(pr);
        self = oldself;
        VR_Test_Check(dw_variant_shots == before + 1 && inst.wi_clip == clip - (k ? 6 : 1),
            sprintf("%s: Dwell's shot fired (%g shots, clip %g -> %g)", pr.netname, dw_variant_shots - before, clip, inst.wi_clip));
        remove(dw_gt_t[k]);
    }
    // The powerups' screen tints (dw_powerup_tint: Dwell's colours through AD's tint): Haste, both, none.
    entity p = self.owner;
    entity os = self;
    self = p;
    p.dw_haste_finished = time + 5;
    DW_PowerupFrame();
    float t1 = p.vr_ad_tint, d1 = p.vr_ad_tintden;
    p.dw_berserk_finished = time + 5;
    DW_PowerupFrame();
    float t2 = p.vr_ad_tint, d2 = p.vr_ad_tintden;
    p.dw_haste_finished = p.dw_berserk_finished = time - 1;
    DW_PowerupFrame();
    DW_PowerupFrame();
    self = os;
    VR_Test_Check(t1 == 255 * 65536 + 255 * 256 && d1 == 10, sprintf("Haste tints yellow (%g, %g)", t1, d1));
    VR_Test_Check(t2 == 255 * 65536 + 128 * 256 && d2 == 20, sprintf("Haste and Berserk tint orange (%g, %g)", t2, d2));
    VR_Test_Check(!p.vr_ad_tintden && !p.vr_dw_tinted, sprintf("both ended: no tint (%g)", p.vr_ad_tintden));
    VR_Test_Verdict();
    remove(self);
};

void(entity p) DW_GruntTest =
{
    VR_Test_BeginFlags("dwgrunt", VR_TEST_LOUD);
    p.flags = p.flags | FL_GODMODE | FL_NOTARGET;
    vr_agt_player = p;
    float k;
    for(k = 0; k < 2; k++)
    {
        float wid = k ? WID_LIGHTNING : WID_SUPER_SHOTGUN;
        makevectors(p.angles);
        entity g = VR_AGT_Spawn(-1, 220 + 140 * k);
        entity inst = WeaponInst_New(0);
        inst.wi_dwvar = DW_VariantFor(wid);
        inst.wi_clip = 30;
        VR_AG_Arm(g, wid, 0, inst);
        entity pr = g.vr_ag_prop;
        VR_Test_Check(pr && pr.weaponinst == inst && inst.wi_dwvar == DW_VariantFor(wid),
            sprintf("%s armed: its record and variant kept (variant %g)", DW_VariantName(DW_VariantFor(wid)), inst.wi_dwvar));
        VR_Test_Check(pr.netname == DW_VariantName(inst.wi_dwvar), sprintf("named %s", pr.netname));
        makevectors(g.angles);
        dw_gt_g[k] = g;
        dw_gt_t[k] = VR_Test_Spawn(world, 0, g.origin + v_forward * 128 + '0 0 8', g.angles_y + 180);
        DW_WeaponsTest_Hold(dw_gt_t[k], dw_gt_t[k].origin);
    }
    entity t = spawn();
    t.classname = "dw_grunttest";
    t.owner = p;
    t.think = DW_GruntTest_Think;
    t.nextthink = time;
};

// vr_ad_test 205 (DWELL.md 9.6): Dwell's weapons drawn as themselves: 201's three given, then (a frame later: the models
// synced in PostThink) each hand's and the holster's model Dwell's VR model (with Dwell's data; without it ours), and the
// Rotary's model, moved to the off hand, still its own. "dwmodels: N passed, M failed". 206-208: the Rotary Shotgun, the
// Crystal Lance, the Rifle alone in the main hand (the screenshot harness: Misc/quakevr/dwell_vrmodels_test.sh).
float dw_mt_frames;
void() DW_ModelsTest_Think =
{
    entity p = self.owner;
    dw_mt_frames = dw_mt_frames + 1;
    if(dw_mt_frames < 3)
    {
        self.nextthink = time;
        return;
    }
    float data = DW_Data();
    VR_Test_BeginFlags("dwmodels", VR_TEST_LOUD);
    VR_Test_Check(data, "Dwell's data found (its folder)");
    VR_Test_Check(p.weaponmodel == (data ? "progs/dw_shot3.mdl#vr" : WeaponIdToModel(WID_SUPER_SHOTGUN, 0)),
        sprintf("the Rotary Shotgun drawn as itself (%s)", p.weaponmodel));
    VR_Test_Check(p.weaponmodel2 == (data ? "progs/dw_rail.mdl#vr" : WeaponIdToModel(WID_LIGHTNING, 0)),
        sprintf("the Crystal Lance drawn as itself (%s)", p.weaponmodel2));
    VR_Test_Check(p.holsterweaponmodel0 == (data ? "progs/dw_rifle.mdl#vr" : WeaponIdToModel(WID_SHOTGUN, 0)),
        sprintf("the Rifle holstered drawn as itself (%s)", p.holsterweaponmodel0));
    VR_Test_Check(AD_WeaponModel(WID_SUPER_SHOTGUN, 0, world) == WeaponIdToModel(WID_SUPER_SHOTGUN, 0),
        "a plain super shotgun drawn as ours");
    entity widow = WeaponInst_New(WeaponIdToBaseClipSize(WID_SUPER_SHOTGUN));
    widow.wi_adup = ad_IT_UPGRADE_SSG;
    float adOn = vr_ad_data && AD_WeaponUpgraded(world, widow, ad_IT_UPGRADE_SSG);
    VR_Test_Check(!adOn || AD_WeaponModel(WID_SUPER_SHOTGUN, 0, widow) == "progs/v_shot3.mdl#vr",
        sprintf("AD's Widowmaker still its own (%s)", AD_WeaponModel(WID_SUPER_SHOTGUN, 0, widow)));
    remove(widow);
    VR_Test_Verdict();
    remove(self);
};

void(entity p, float xRequest) DW_ModelsTest =
{
    if(xRequest == 205)
    {
        DW_WeaponsTest_Give(p);
        // (as a hand's draw sets them: our models, which AD_SyncWeaponModels swaps for Dwell's)
        p.weaponmodel = WeaponIdToModel(WID_SUPER_SHOTGUN, 0);
        p.weaponmodel2 = WeaponIdToModel(WID_LIGHTNING, 0);
        dw_mt_frames = 0;
        entity t = spawn();
        t.classname = "dw_modelstest";
        t.owner = p;
        t.think = DW_ModelsTest_Think;
        t.nextthink = time;
        return;
    }
    // 206-208: one of them alone in the main hand (the off hand empty).
    float wid = xRequest == 206 ? WID_SUPER_SHOTGUN : xRequest == 207 ? WID_LIGHTNING : WID_SHOTGUN;
    entity os = self;
    self = p;
    VR_WeaponUtil_EntAddWeapon(p, wid);
    VRGiveEntWeapon(p, cVR_MainHand, wid, 0, WeaponIdToBaseClipSize(wid));
    p.weaponinst.wi_dwvar = DW_VariantFor(wid);
    VR_Scene_HoldGiven(p, cVR_MainHand);
    p.weaponmodel = WeaponIdToModel(wid, 0); // (as a hand's draw sets it: swapped for Dwell's by AD_SyncWeaponModels)
    p.ammo_shells = 100;
    p.ammo_cells = 100;
    self = os;
    bprint(sprintf("dwmodels: %s in the main hand\n", DW_VariantName(DW_VariantFor(wid))));
};

void(entity p, float xRequest) DW_WeaponsTest =
{
    if(xRequest >= 205 && xRequest <= 208)
    {
        DW_ModelsTest(p, xRequest);
        return;
    }
    if(xRequest == 204)
    {
        DW_GruntTest(p);
        return;
    }
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
