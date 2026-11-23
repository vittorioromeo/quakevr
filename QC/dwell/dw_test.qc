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
    string rotary = DW_RotaryGauntlet() ? "progs/dw_rotary.mdl#vr" : "progs/dw_shot3.mdl#vr"; // (vr_dw_rotary_gauntlet)
    VR_Test_Check(p.weaponmodel == (data ? rotary : WeaponIdToModel(WID_SUPER_SHOTGUN, 0)),
        sprintf("the Rotary Shotgun drawn as itself (%s)", p.weaponmodel));
    string lance = DW_LancePole() ? "progs/dw_lance.mdl#vr" : "progs/dw_rail.mdl#vr"; // (vr_dw_lance_pole)
    VR_Test_Check(p.weaponmodel2 == (data ? lance : WeaponIdToModel(WID_LIGHTNING, 0)),
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

// vr_ad_test 213 (his notes 2026-11-21): Dwell's map pickups taken by the body's touch (weapon_touch: they asserted on
// their classnames and were removed), and the toolgun's Dwell weapons and bloody shotguns laid as props (not floating
// pickups), with their variant or bloody bit, a bloody one's taken by the hand. "dwpickup: N passed, M failed"; the
// script fails on any "Assertion failure".
entity(entity p, string xClass, void() xSpawn) DW_PickupTest_Make =
{
    entity os = self;
    makevectors('0 1 0' * p.angles_y);
    self = spawn();
    self.classname = xClass;
    setorigin(self, p.origin + v_forward * 64 + '0 0 16');
    vr_grab_made = world;
    xSpawn();
    entity e = self;
    self = os;
    return e;
};

void(entity p) DW_PickupTest =
{
    VR_Test_BeginFlags("dwpickup", VR_TEST_LOUD);
    entity os = self;
    self = p;
    VRGiveEntWeapon(p, cVR_MainHand, WID_FIST, 0, 0);
    VRGiveEntWeapon(p, cVR_OffHand, WID_FIST, 0, 0);
    self = os;
    float touching = p.touchinghand;
    p.touchinghand = cVR_FakeHand; // (the body's touch: the first free hand)
    entity pk = DW_PickupTest_Make(p, "weapon_rifle", dw_weapon_rifle);
    self = pk; other = p; weapon_touch(); self = os;
    VR_Test_Check(pk.model == "" && p.weapon == WID_SHOTGUN && p.weaponinst.wi_dwvar == DW_VAR_RIFLE,
        sprintf("the map's Rifle taken: weapon %g variant %g, pickup %s", p.weapon, p.weaponinst.wi_dwvar,
        pk.model == "" ? "gone" : "still there"));
    pk = DW_PickupTest_Make(p, "weapon_railgun", dw_weapon_railgun);
    self = pk; other = p; weapon_touch(); self = os;
    VR_Test_Check(pk.model == "" && p.weapon2 == WID_LIGHTNING && p.weaponinst2.wi_dwvar == DW_VAR_LANCE,
        sprintf("the map's Crystal Lance taken: off hand weapon %g variant %g", p.weapon2, p.weaponinst2.wi_dwvar));
    p.touchinghand = touching;

    float k;
    for(k = 0; k < 3; k++)
    {
        float wid = k == 0 ? WID_SUPER_SHOTGUN : k == 1 ? WID_LIGHTNING : WID_SHOTGUN;
        float vr = k == 0 ? DW_VAR_ROTARY : k == 1 ? DW_VAR_LANCE : DW_VAR_RIFLE;
        DW_PickupTest_Make(p, "toolgun", k == 0 ? toolgun_dw_rotary : k == 1 ? toolgun_dw_lance : toolgun_dw_rifle);
        entity w = vr_grab_made;
        VR_Test_Check(w != world && w.classname == "thrown_weapon" && w.weapon == wid && w.weaponinst.wi_dwvar == vr &&
            w.model == DW_VRModelOf(wid), sprintf("the toolgun's %s laid as a prop: %s, weapon %g variant %g, model %s",
            DW_VariantName(vr), w.classname, w.weapon, w.weaponinst.wi_dwvar, w.model));
        if(k == 0 && w != world)
        {
            VR_Test_Check(w.weaponinst.wi_clip == VR_ClipSizeOf(WID_SUPER_SHOTGUN, w.weaponinst),
                sprintf("the toolgun's Rotary Shotgun comes loaded: %g", w.weaponinst.wi_clip));
        }
    }

    float bits = MG3_BloodyBits();
    MG3_SetBloodyBits(0);
    for(k = 0; k < 2; k++)
    {
        DW_PickupTest_Make(p, "toolgun", k == 0 ? toolgun_bloody_sg : toolgun_bloody_ssg);
        entity b = vr_grab_made;
        float bwid = k == 0 ? WID_SHOTGUN : WID_SUPER_SHOTGUN;
        float bit = k == 0 ? MG3_BLOODY_SG : MG3_BLOODY_SSG;
        VR_Test_Check(b != world && b.classname == "thrown_weapon" && b.weapon == bwid && b.vr_bloody_bit == bit &&
            b.model == WeaponIdToModel(bwid, 0), sprintf("the toolgun's %s laid as a prop: %s, weapon %g, bit %g, model %s",
            b.netname, b.classname, b.weapon, b.vr_bloody_bit, b.model));
        if(b == world) { continue; }
        float hand = k == 0 ? cVR_MainHand : cVR_OffHand;
        self = p;
        VRGiveEntWeapon(p, hand, WID_FIST, 0, 0);
        self = os;
        float shells = p.ammo_shells;
        float* wp = VRGetEntWeaponPtr(p, hand);
        float* fp = VRGetEntWeaponFlagsPtr(p, hand);
        wpnthrow_handtouch_impl(hand, b, p, wp, fp);
        self = os;
        VR_Test_Check(VRGetEntWeapon(p, hand) == bwid && (MG3_BloodyBits() & bit) && p.ammo_shells > shells,
            sprintf("taken: the hand holds %g, bloody bits %g, shells %g -> %g", VRGetEntWeapon(p, hand), MG3_BloodyBits(),
            shells, p.ammo_shells));
    }
    MG3_SetBloodyBits(bits);
    VR_Test_Verdict();
};

// vr_ad_test 214 (Dwell's start, his note 2026-11-21: two runes placed, the gates looked open, walking through teleported
// nowhere): the spawn point's targets fired (start1_targets unlocks the pedestals' relays: Copper's PutClientInServer),
// both working pedestals touched by the player, then (once the relays' 2 s delay has passed: the walls toggled) each
// episode's corridor walkable from the portal sheet to its teleporter (the player's hull traced), and its teleporter
// taking the player to its tunnel. "dwgate: N passed, M failed"; gives up after 10 s.
float dw_gt_until;
entity(string xField, string xValue, string xClass) DW_GateTest_Find =
{
    entity e = world;
    while((e = find(e, classname, xClass)))
    {
        if((xField == "target" ? e.target : e.targetname) == xValue) { return e; }
    }
    return world;
};

void() DW_GateTest_Think =
{
    entity p = self.owner;
    entity w1 = DW_GateTest_Find("targetname", "ep1_fakewall", "func_wall");
    entity w2 = DW_GateTest_Find("targetname", "ep2_fakewall", "func_wall");
    // (gone: not solid, its model and its solid stand-in, Copper's "buddy", gone: cu_bmodel_vanish)
    float open = w1 && w2 && w1.solid == SOLID_NOT && w2.solid == SOLID_NOT && !w1.modelindex && !w2.modelindex &&
                 !w1.cu_buddy && !w2.cu_buddy;
    if(!open && time < dw_gt_until)
    {
        self.nextthink = time + 0.1;
        return;
    }
    VR_Test_Check(open, sprintf("the corridors' walls toggled away (solid %g, %g)", w1.solid, w2.solid));
    float k;
    for(k = 1; k <= 2; k++)
    {
        string dest = k == 1 ? "ep1_tunneltele" : "ep2_tunneltele";
        entity t = DW_GateTest_Find("target", dest, "trigger_teleport");
        entity sheet = DW_GateTest_Find("targetname", k == 1 ? "ep1_portal" : "ep2_portal", "func_fade");
        entity d = find(world, targetname, dest);
        if(!t || !sheet || !d)
        {
            VR_Test_Check(FALSE, sprintf("episode %g's teleporter, portal sheet and destination found", k));
            continue;
        }
        vector mid = (t.absmin + t.absmax) * 0.5;
        vector from = (sheet.absmin + sheet.absmax) * 0.5;
        from_z = mid_z = t.absmin_z + 24 + 2; // (standing height: the hull's bottom on the floor)
        tracebox(from, VEC_HULL_MIN, VEC_HULL_MAX, mid, FALSE, p);
        VR_Test_Check(trace_fraction >= 1 || trace_ent == t, sprintf("episode %g: walkable from the portal to its "
            "teleporter (%v to %v: blocked at %v by '%s', its owner '%s')", k, from, mid, trace_endpos,
            trace_ent.classname, trace_ent.owner.classname));
        VR_Test_Check(!(t.cu_customflags & cu_CFL_LOCKED), sprintf("episode %g's teleporter unlocked", k));
        setorigin(p, mid);
        p.velocity = '0 0 0';
        entity os = self;
        self = t; other = p;
        t.touch();
        self = os;
        VR_Test_Check(vlen(p.origin - d.origin) < 64, sprintf("episode %g's teleporter took the player to %v (%v)", k,
            d.origin, p.origin));
    }
    VR_Test_Verdict();
    remove(self);
};

void(entity p) DW_GateTest =
{
    VR_Test_BeginFlags("dwgate", VR_TEST_LOUD);
    entity r1 = DW_GateTest_Find("targetname", "rune1_set", "trigger_relay");
    entity r2 = DW_GateTest_Find("targetname", "rune2_set", "trigger_relay");
    VR_Test_Check(r1 && r2 && !(r1.cu_customflags & cu_CFL_LOCKED) && !(r2.cu_customflags & cu_CFL_LOCKED),
        "the pedestals' relays unlocked by the spawn point's targets (start1_targets)");
    float k;
    for(k = 1; k <= 2; k++)
    {
        entity ped = DW_GateTest_Find("target", k == 1 ? "rune1_set" : "rune2_set", "trigger_once");
        VR_Test_Check(ped != world, sprintf("rune %g's pedestal found", k));
        if(!ped) { continue; }
        entity os = self;
        self = ped; other = p;
        ped.touch();
        self = os;
    }
    dw_gt_until = time + 10;
    entity t = spawn();
    t.classname = "dw_gatetest";
    t.owner = p;
    t.think = DW_GateTest_Think;
    t.nextthink = time + 0.1;
};

// vr_ad_test 215 (his note 2026-11-21: the Kamikaze burst at once, without chasing him, and with no explosion drawn): one
// made up to 500 units away down the clearest line, facing the player (not notarget); it must wake, run at him and burst
// near him, its explosion drawn (DW_Boom). "dwkami: N passed, M failed"; gives up after 15 s.
entity dw_kt_kami;
float dw_kt_born, dw_kt_booms, dw_kt_last, dw_kt_far;
void() DW_KamiTest_Think =
{
    entity p = self.owner;
    entity k = dw_kt_kami;
    // (alive until its death: dw_kami_die takes its damage away; freed, an entity keeps its classname)
    if(k.classname == "monster_kamikaze" && k.takedamage != DAMAGE_NO && time < dw_kt_born + 15)
    {
        dw_kt_last = vlen(VR_AI_Origin(p) - k.origin);
        self.nextthink = time + 0.05;
        return;
    }
    if(!self.cnt) // (dead: its burst follows 0.05 s later, dw_kami_die; the verdict 0.3 s on)
    {
        self.cnt = 1;
        self.ltime = time;
        self.nextthink = time + 0.3;
        return;
    }
    VR_Test_Check(self.ltime >= dw_kt_born + 0.5, sprintf("it chased first: burst %.2f s after it was made, %g units away",
        self.ltime - dw_kt_born, dw_kt_far));
    VR_Test_Check(self.ltime < dw_kt_born + 15, "it burst within 15 s");
    VR_Test_Check(dw_kt_last < 100, sprintf("it burst near the player: %.0f units from him", dw_kt_last));
    VR_Test_Check(dw_boom_count > dw_kt_booms, sprintf("its explosion drawn (%g explosions)", dw_boom_count - dw_kt_booms));
    VR_Test_Verdict();
    remove(self);
};

void(entity p) DW_KamiTest =
{
    VR_Test_BeginFlags("dwkami", VR_TEST_LOUD);
    float yaw, best = 0, bestYaw = 0;
    for(yaw = 0; yaw < 360; yaw += 45)
    {
        tracebox(p.origin, '-16 -16 -24', '16 16 24', p.origin + DW_Yaw(yaw) * 600, TRUE, p);
        if(trace_fraction * 600 > best) { best = trace_fraction * 600; bestYaw = yaw; }
    }
    dw_kt_far = min(500, best - 32);
    VR_Test_Check(dw_kt_far >= 250, sprintf("room for the chase: %g units clear at yaw %g", best, bestYaw));
    entity os = self;
    self = spawn();
    setorigin(self, p.origin + DW_Yaw(bestYaw) * dw_kt_far);
    self.angles = '0 1 0' * (bestYaw + 180);
    dw_monster_kamikaze();
    dw_kt_kami = self;
    self = os;
    dw_kt_born = time;
    dw_kt_booms = dw_boom_count;
    dw_kt_last = dw_kt_far;
    entity t = spawn();
    t.classname = "dw_kamitest";
    t.owner = p;
    t.think = DW_KamiTest_Think;
    t.nextthink = time + 0.5; // (its start, walkmonster_start's, has given it its damage by then)
};

// vr_ad_test 216 (d2end; his note 2026-11-21: stuck inside the Sculptor, moving with it): the map's Sculptor with its
// fighting box and controller, the player put inside its box (60 units from its middle); its failsafe must put him just
// outside, where he is free (his box clear of everything). "dwstuck: N passed, M failed"; gives up after 3 s.
float dw_st_until;
vector dw_st_from;
void() DW_StuckTest_Think =
{
    entity p = self.owner;
    entity b = self.enemy;
    float inside = p.absmin_x < b.absmax_x && p.absmax_x > b.absmin_x && p.absmin_y < b.absmax_y &&
                   p.absmax_y > b.absmin_y && p.absmin_z < b.absmax_z && p.absmax_z > b.absmin_z;
    if(inside && time < dw_st_until)
    {
        self.nextthink = time + 0.05;
        return;
    }
    VR_Test_Check(!inside, sprintf("the player put out of its box: at %v, its box %v to %v", p.origin, b.absmin, b.absmax));
    tracebox(p.origin, p.mins, p.maxs, p.origin, FALSE, p);
    VR_Test_Check(!trace_startsolid, sprintf("he is free there (in %s)", trace_ent.classname));
    VR_Test_Check(vlen(p.origin - dw_st_from) < 240, sprintf("moved %.0f units: just outside it", vlen(p.origin - dw_st_from)));
    VR_Test_Verdict();
    remove(self);
};

void(entity p) DW_StuckTest =
{
    VR_Test_BeginFlags("dwstuck", VR_TEST_LOUD);
    entity b = find(world, classname, "monster_d2boss");
    VR_Test_Check(b != world, "the map's Sculptor found");
    if(!b) { VR_Test_Verdict(); return; }
    DW_D2_FightBox(b);
    b.think = dw_d2_think;
    b.nextthink = time + 0.1;
    dw_st_from = b.origin + '60 0 0';
    setorigin(p, dw_st_from);
    p.velocity = '0 0 0';
    dw_st_until = time + 3;
    entity t = spawn();
    t.classname = "dw_stucktest";
    t.owner = p;
    t.enemy = b;
    t.think = DW_StuckTest_Think;
    t.nextthink = time + 0.05;
};

// vr_ad_test 217 (dwellwpn2): the Crystal Lance as a lance (vr_dw_lance_pole): in the main hand drawn with its lance model
// (Dwell's world model's, slot 55) and dealt its crystal head's blow (DW_LanceHeld: DW_LanceMelee, not the gun's club);
// with vr_dw_lance_pole 0 the pistol (dw_rail) and the gun's club again; the setting put back. "dwlance: N passed, M failed".
float dw_lt_frames;
float dw_lt_was;
void() DW_LanceTest_Think =
{
    entity p = self.owner;
    dw_lt_frames = dw_lt_frames + 1;
    entity os = self;
    if(dw_lt_frames == 3)
    {
        VR_Test_BeginFlags("dwlance", VR_TEST_LOUD);
        VR_Test_Check(DW_Data(), "Dwell's data found (its folder)");
        VR_Test_Check(p.weaponmodel == "progs/dw_lance.mdl#vr", sprintf("the Lance drawn as a lance (%s)", p.weaponmodel));
        self = p;
        VR_Test_Check(DW_LanceHeld(cVR_MainHand), "its blow the crystal head's (DW_LanceMelee)");
        self = os;
        cvar_set("vr_dw_lance_pole", "0");
        p.weaponmodel = WeaponIdToModel(WID_LIGHTNING, 0); // (as a hand's draw sets it: swapped by AD_SyncWeaponModels)
    }
    else if(dw_lt_frames == 6)
    {
        VR_Test_Check(p.weaponmodel == "progs/dw_rail.mdl#vr", sprintf("vr_dw_lance_pole 0: the pistol (%s)", p.weaponmodel));
        self = p;
        VR_Test_Check(!DW_LanceHeld(cVR_MainHand), "vr_dw_lance_pole 0: the gun's club");
        self = os;
        cvar_set("vr_dw_lance_pole", ftos(dw_lt_was));
        VR_Test_Verdict();
        remove(self);
        return;
    }
    self.nextthink = time;
};

void(entity p) DW_LanceTest =
{
    dw_lt_was = cvar("vr_dw_lance_pole");
    cvar_set("vr_dw_lance_pole", "1");
    DW_ModelsTest(p, 207);
    dw_lt_frames = 0;
    entity t = spawn();
    t.classname = "dw_lancetest";
    t.owner = p;
    t.think = DW_LanceTest_Think;
    t.nextthink = time;
};

// vr_ad_test 224 (lancecharge): the Crystal Lance's charged shot (dw_items.qc DW_LanceChargeFrame), in the main hand
// (207's), a grunt held 64 units ahead on its line, the trigger held by the test (dw_lc_test: through W_WeaponFrameImpl
// as the real one). With vr_dw_lance_charge 1: held to half charge (in 0.5 s), let go: no shot, no damage, no cells, a
// charge let go short; nothing later either. With 0.5: full in 0.5 s, held on charged 30 frames without a shot, let go:
// the shot (150 to the grunt, 6 cells). Without its 6 cells (4): no charge begins, its cells kept. The setting (a
// tunable: `tune`, taken the next frame) put back.
// "dwcharge: N passed, M failed".
float dw_ct_step, dw_ct_frames, dw_ct_wait, dw_ct_was;
float dw_ct_cells, dw_ct_shots, dw_ct_starts, dw_ct_cancels, dw_ct_fires;
entity dw_ct_a;

void(entity p) DW_ChargeTest_Place =
{
    entity os = self;
    self = p;
    vector src = VRGetMuzzlePos(cVR_MainHand);
    vector dir = VRGetWeaponFireRotFwd(cVR_MainHand);
    self = os;
    DW_WeaponsTest_Hold(dw_ct_a, src + dir * 64);
};

void(entity p) DW_ChargeTest_End =
{
    dw_lc_test = dw_lc_test_press = 0;
    localcmd(sprintf("tune vr_dw_lance_charge %g\n", dw_ct_was)); // (a tunable: set by `tune`)
    cvar_set("vr_dw_lance_pole", ftos(dw_lt_was));
    VR_Test_Verdict();
    remove(dw_ct_a);
    remove(self);
};

void() DW_ChargeTest_Think =
{
    entity p = self.owner;
    self.nextthink = time;
    dw_ct_frames++;
    if(dw_ct_frames > 1500)
    {
        VR_Test_Check(FALSE, sprintf("timed out (step %g)", dw_ct_step));
        DW_ChargeTest_End(p);
        return;
    }
    if(!dw_ct_a.takedamage) { return; } // (its monster start: a frame or two)
    float dmg = 1000 - dw_ct_a.health;
    float shots = dw_variant_shots - dw_ct_shots;
    if(dw_ct_step == 0)
    {
        VR_Test_BeginFlags("dwcharge", VR_TEST_LOUD);
        entity os = self;
        self = p;
        VR_Test_Check(DW_LanceCharged(cVR_MainHand), "the Lance in the main hand charges its shots");
        self = os;
        localcmd("tune vr_dw_lance_charge 1\n");
        dw_lc_test = 1;
        dw_lc_test_press = 0;
        DW_ChargeTest_Place(p);
        dw_ct_cells = p.ammo_cells;
        dw_ct_shots = dw_variant_shots;
        dw_ct_starts = dw_lc_starts;
        dw_ct_cancels = dw_lc_cancels;
        dw_ct_fires = dw_lc_fires;
        dw_ct_step = 1;
    }
    else if(dw_ct_step == 1) // (its refire over: it may attack)
    {
        if(time < p.attack_finished || cvar_hget(cvarh_vr_dw_lance_charge) != 1) { return; }
        dw_lc_test_press = 1;
        dw_ct_step = 2;
    }
    else if(dw_ct_step == 2)
    {
        if(p.dw_lc1 < 0.5) { return; }
        VR_Test_Check(dw_lc_starts == dw_ct_starts + 1 && fabs(time - p.dw_lc_s1 - 0.5) <= 0.03,
            sprintf("charge time 1: half charged %.3f s after it began (0.5; charges begun %g)", time - p.dw_lc_s1,
            dw_lc_starts - dw_ct_starts));
        dw_lc_test_press = 0;
        dw_ct_step = 3;
    }
    else if(dw_ct_step == 3)
    {
        if(p.dw_lc1 > 0) { return; }
        VR_Test_Check(dw_lc_cancels == dw_ct_cancels + 1 && dw_lc_fires == dw_ct_fires,
            sprintf("let go at half charge: a charge let go short (%g), none fired (%g)", dw_lc_cancels - dw_ct_cancels,
            dw_lc_fires - dw_ct_fires));
        VR_Test_Check(dmg == 0 && shots == 0 && p.ammo_cells == dw_ct_cells,
            sprintf("let go at half charge: no shot (%g), no damage (%g), no cells (%g of %g)", shots, dmg, p.ammo_cells, dw_ct_cells));
        dw_ct_wait = 0;
        dw_ct_step = 4;
    }
    else if(dw_ct_step == 4)
    {
        if(++dw_ct_wait < 20) { return; }
        VR_Test_Check(dmg == 0 && shots == 0, sprintf("no shot later (%g frames on: %g shots, %g damage)", dw_ct_wait, shots, dmg));
        localcmd("tune vr_dw_lance_charge 0.5\n");
        DW_ChargeTest_Place(p);
        dw_ct_step = 10;
    }
    else if(dw_ct_step == 10) // (the setting taken: the next frame's commands)
    {
        if(cvar_hget(cvarh_vr_dw_lance_charge) != 0.5) { return; }
        dw_lc_test_press = 1;
        dw_ct_step = 5;
    }
    else if(dw_ct_step == 5)
    {
        if(p.dw_lc1 < 1) { return; }
        VR_Test_Check(fabs(time - p.dw_lc_s1 - 0.5) <= 0.03, sprintf("charge time 0.5: full %.3f s after it began", time - p.dw_lc_s1));
        dw_ct_wait = 0;
        dw_ct_step = 6;
    }
    else if(dw_ct_step == 6)
    {
        if(++dw_ct_wait < 30) { return; }
        VR_Test_Check(dmg == 0 && shots == 0 && p.ammo_cells == dw_ct_cells && p.dw_lc1 >= 1,
            sprintf("held charged 30 frames: no shot yet (%g), no cells (%g), still charged (%.2f)", shots, p.ammo_cells, p.dw_lc1));
        dw_lc_test_press = 0;
        dw_ct_wait = 0;
        dw_ct_step = 7;
    }
    else if(dw_ct_step == 7)
    {
        if(!shots && ++dw_ct_wait < 20) { return; }
        VR_Test_Check(shots == 1 && dw_lc_fires == dw_ct_fires + 1, sprintf("let go charged: the shot (%g, a charge fired %g)",
            shots, dw_lc_fires - dw_ct_fires));
        // (150 through our damage: 188 to an unaware target, as test 202's)
        VR_Test_Check(dmg >= 150 && dmg <= 188 && p.ammo_cells == dw_ct_cells - 6,
            sprintf("let go charged: 150 to the grunt (%g), 6 cells (%g of %g)", dmg, p.ammo_cells, dw_ct_cells));
        dw_ct_step = 8;
    }
    else if(dw_ct_step == 8) // (its refire over)
    {
        if(time < p.attack_finished) { return; }
        p.ammo_cells = 4;
        dw_ct_starts = dw_lc_starts;
        dw_ct_shots = dw_variant_shots;
        dw_lc_test_press = 1;
        dw_ct_wait = 0;
        dw_ct_step = 9;
    }
    else if(dw_ct_step == 9)
    {
        if(++dw_ct_wait < 40) { return; }
        VR_Test_Check(dw_lc_starts == dw_ct_starts && p.dw_lc1 == 0 && shots == 0 && p.ammo_cells == 4,
            sprintf("4 cells: no charge begun (%g), no shot (%g), the cells kept (%g)", dw_lc_starts - dw_ct_starts, shots,
            p.ammo_cells));
        DW_ChargeTest_End(p);
    }
};

void(entity p) DW_ChargeTest =
{
    dw_ct_was = cvar_hget(cvarh_vr_dw_lance_charge);
    dw_lt_was = cvar("vr_dw_lance_pole");
    cvar_set("vr_dw_lance_pole", "1");
    DW_ModelsTest(p, 207);
    dw_ct_step = dw_ct_frames = 0;
    makevectors('0 1 0' * p.angles_y);
    dw_ct_a = VR_Test_Spawn(world, 0, p.origin + v_forward * 200 + '0 0 64', p.angles_y + 180);
    entity t = spawn();
    t.classname = "dw_chargetest";
    t.owner = p;
    t.think = DW_ChargeTest_Think;
    t.nextthink = time;
};

// vr_ad_test 218 (dwellwpn3): the Rotary Shotgun as a gauntlet (vr_dw_rotary_gauntlet), its reload found. In the off
// hand, its tube empty, the main hand free: drawn with its gauntlet model (slot 56, the super nailgun's hold); its shell
// port found (the engine's load point for it: vr_view_internal.hpp loadPorts, DwVrRotary; waited for a frame at a time,
// up to a second); it loads by hand, never broken open; the pouch's handfuls held at the port fill its tube (as the
// Widowmaker's, test 149); then with the setting 0 the super shotgun's grip again (dw_shot3). "dwrotary: N passed, M
// failed".
float dw_rt_frames;
float dw_rt_was; // the setting's old value; -1 - it once the gauntlet's checks are done
void() DW_RotaryTest_Think =
{
    entity p = self.owner;
    entity os = self;
    float h = cVR_OffHand;
    dw_rt_frames = dw_rt_frames + 1;
    if(dw_rt_frames < 72 && p.offloadportpos == '0 0 0' && dw_rt_was >= 0)
    {
        self.nextthink = time; // (the engine's load point comes with a move: a frame or two)
        return;
    }
    if(dw_rt_was >= 0)
    {
        VR_Test_BeginFlags("dwrotary", VR_TEST_LOUD);
        VR_Test_Check(DW_Data(), "Dwell's data found (its folder)");
        VR_Test_Check(p.weaponmodel2 == "progs/dw_rotary.mdl#vr", sprintf("the Rotary drawn as a gauntlet (%s)", p.weaponmodel2));
        VR_Test_Check(p.offloadportpos != '0 0 0', sprintf("its shell port found (load point %v after %g frames)",
            p.offloadportpos, dw_rt_frames));
        self = p;
        VR_Test_Check(VR_Reload_ManualInHand(p, h) && !VR_Reload_SsgBreaks(p, h), "it loads by hand (never broken open)");
        float tube = VR_ClipSizeIn(p, h);
        VR_Test_Check(tube == rint(cvar_hget(cvarh_vr_ad_widowmaker_tube)), sprintf("its tube the Widowmaker's (%g)", tube));
        float handful = rint(cvar_hget(cvarh_vr_ad_widowmaker_handful));
        float loads = 0, most = 0;
        for(float i = 0; i < 12 && VRGetEntWeaponClip(p, h) < tube; i++)
        {
            VR_Reload_PlayerFrame();
            VR_Reload_Take(cVR_MainHand);
            entity r = VR_Carry_Held(p, cVR_MainHand);
            if(r == world) { break; }
            setorigin(r, r.origin + p.offloadportpos - VR_Reload_RoundRef(r));
            r.vr_ammo_fresh = FALSE;
            float was = VRGetEntWeaponClip(p, h);
            VR_Reload_HeldFrame(cVR_MainHand, r);
            if(VRGetEntWeaponClip(p, h) > was)
            {
                loads = loads + 1;
                most = max(most, VRGetEntWeaponClip(p, h) - was);
            }
        }
        VR_Test_Check(most == handful, sprintf("a pouch handful at its port goes in at once (%g, a handful %g)", most, handful));
        VR_Test_Check(VRGetEntWeaponClip(p, h) == tube && loads == ceil(tube / handful),
            sprintf("its tube fills at the port (%g of %g after %g inserts)", VRGetEntWeaponClip(p, h), tube, loads));
        self = os;
        cvar_set("vr_dw_rotary_gauntlet", "0");
        p.weaponmodel2 = WeaponIdToModel(WID_SUPER_SHOTGUN, 0); // (as a hand's draw sets it: swapped by AD_SyncWeaponModels)
        dw_rt_was = -1 - dw_rt_was;
        dw_rt_frames = 0;
        self.nextthink = time;
        return;
    }
    if(dw_rt_frames < 3)
    {
        self.nextthink = time;
        return;
    }
    VR_Test_Check(p.weaponmodel2 == "progs/dw_shot3.mdl#vr", sprintf("vr_dw_rotary_gauntlet 0: the super shotgun's grip (%s)",
        p.weaponmodel2));
    cvar_set("vr_dw_rotary_gauntlet", ftos(-1 - dw_rt_was));
    VR_Test_Verdict();
    remove(self);
};

void(entity p) DW_RotaryTest =
{
    dw_rt_was = cvar("vr_dw_rotary_gauntlet");
    cvar_set("vr_dw_rotary_gauntlet", "1");
    entity os = self;
    self = p;
    VR_WeaponUtil_EntAddWeapon(p, WID_SUPER_SHOTGUN);
    VRGiveEntWeapon(p, cVR_OffHand, WID_SUPER_SHOTGUN, 0, 0);
    p.weaponinst2.wi_dwvar = DW_VAR_ROTARY;
    p.weaponmodel2 = WeaponIdToModel(WID_SUPER_SHOTGUN, 0); // (as a hand's draw sets it: swapped by AD_SyncWeaponModels)
    VRGiveEntWeapon(p, cVR_MainHand, WID_FIST, 0, 0);
    p.ammo_shells = 100;
    VR_Scene_HoldGiven(p, cVR_OffHand); // (held, the mock's grip unpressed)
    self = os;
    bprint("dwrotary: the Rotary in the off hand, its tube empty\n");
    dw_rt_frames = 0;
    entity t = spawn();
    t.classname = "dw_rotarytest";
    t.owner = p;
    t.think = DW_RotaryTest_Think;
    t.nextthink = time;
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

// vr_ad_test 210 (d2end, where the Sculptor's models are): the Sculptor's ring takes shots past its old 96-unit box and
// shots between its stones fly on (DW_D2_FightBox, the precise hits on its model). A fighting Sculptor where the map's
// waits, shot at from up to 400 units, turned three ways: a 17 x 17 grid of shots (MOVE_HITMODEL, as the guns) at the
// plane through its middle, 16 units apart. Verdict `dwring: N passed, M failed`.
void(entity p) DW_RingHitTest =
{
    VR_Test_BeginFlags("dwring", VR_TEST_LOUD);
    entity e = spawn();
    e.classname = "dw_ringtest";
    e.flags = FL_MONSTER;
    e.movetype = MOVETYPE_NOCLIP;
    e.takedamage = DAMAGE_AIM;
    setmodel(e, "progs/mon_d2bossb.mdl");
    DW_D2_FightBox(e);
    // where the map's Sculptor waits (its arena's open air), shot at from the clearest of 8 headings
    entity boss = find(world, classname, "monster_d2boss");
    vector at = boss ? boss.origin : p.origin + p.view_ofs;
    float yaw, best = 0, bestYaw = 0;
    for(yaw = 0; yaw < 360; yaw += 45)
    {
        makevectors('0 1 0' * yaw);
        traceline(at, at - v_forward * 900, TRUE, p); // (no monsters)
        if(trace_fraction * 900 > best) { best = trace_fraction * 900; bestYaw = yaw; }
    }
    makevectors('0 1 0' * bestYaw);
    vector fw = v_forward, rt = v_right, up = '0 0 1';
    float dist = min(400, best - 16);
    VR_Test_Check(dist >= 200, sprintf("room for the ring: %g units clear at yaw %g", best, bestYaw));
    vector eye = at - fw * dist;
    setorigin(e, at);
    float k;
    for(k = 0; k < 3; k++)
    {
        e.angles = k == 0 ? '0 0 0' : k == 1 ? '0 90 0' : '-60 0 45';
        float outer = 0, holes = 0, hits = 0, i, j;
        for(i = -8; i <= 8; i++)
        {
            for(j = -8; j <= 8; j++)
            {
                vector to = e.origin + rt * (i * 16) + up * (j * 16);
                traceline(eye, eye + (to - eye) * 1.5, MOVE_HITMODEL, p);
                if(trace_ent == e)
                {
                    hits = hits + 1;
                    if(fabs(i * 16) > 48 || fabs(j * 16) > 48) outer = outer + 1;
                }
                else if((trace_endpos - eye) * fw > dist + 8)
                {
                    holes = holes + 1; // (inside the box's reach, past the ring's plane: between its stones)
                }
            }
        }
        VR_Test_Check(outer > 0, sprintf("turned %v: %g of %g hits past the old box", e.angles, outer, hits));
        VR_Test_Check(holes > 0, sprintf("turned %v: %g shots flew on through the box between its stones (its model, not "
            "its box, is hit)", e.angles, holes));
    }
    remove(e);
    VR_Test_Verdict();
};
#endif
