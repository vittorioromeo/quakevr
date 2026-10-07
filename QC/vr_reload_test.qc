//
//
//
// ----------------------------------------------------------------------------
// Immersive reloading: test steps (impulse 125; vr_reload.qc)
// ----------------------------------------------------------------------------
//
// `vr_reload_test <step>; impulse 125` (Debug > Tests > Reloading) runs immersive reloading's own functions as the hands
// do, the main hand taking and the off hand holding the gun: 0 reports (your shells, each hand's weapon and magazine and
// what it holds, the rounds lying about), 1 the main hand takes a round from the pouch, 2 loads it into the off hand's gun
// (as at the port), 3 drops it (as letting go anywhere), 4 puts it back (as letting go at the pouch), 5 empties the off
// hand's gun into the reserve, 6 ejects the off hand's gun's magazine (as its B/Y), 9 the self-test: the steps in turn
// with every count checked (`reload: PASS|FAIL <what>`, then the totals): the shells, the magazines (VR_Reload_MagSelfTest),
// and the modes' rules (Off and the holsters' reload unchanged, Immersive's guns by hand only).

float vr_reload_test_pass;
float vr_reload_test_fail;

void(float xOk, string xWhat) VR_Reload_Check =
{
    if(xOk)
    {
        vr_reload_test_pass++;
        bprint(sprintf("reload: PASS %s\n", xWhat));
    }
    else
    {
        vr_reload_test_fail++;
        bprint(sprintf("reload: FAIL %s\n", xWhat));
    }
};

string(float xHand) VR_Reload_TestHeld =
{
    entity r = VR_Carry_Held(self, xHand);
    if(r == world)
    {
        return "nothing";
    }
    if(VR_Reload_IsRound(r))
    {
        return sprintf("a round of %g", r.vr_ammo_count);
    }
    return r.classname;
};

void() VR_Reload_TestReport =
{
    float loose = 0;
    entity e = find(world, classname, VR_RELOAD_CLASS);
    while(e)
    {
        loose = loose + (e.carry_player == world ? 1 : 0);
        e = find(e, classname, VR_RELOAD_CLASS);
    }
    bprint(sprintf("reload: mode %g (immersive %g), shells %g nails %g cells %g, %g lying about\n",
        cvar_hget(cvarh_vr_reload_mode), VR_Reload_Immersive(), self.ammo_shells, self.ammo_nails, self.ammo_cells, loose));
    float f = VRGetEntWeaponFlags(self, cVR_OffHand);
    // (The off hand's super shotgun on a line of its own: sprintf takes at most seven values. The report in two prints
    // on one line.)
    string ssg = VRGetEntWeapon(self, cVR_OffHand) == WID_SUPER_SHOTGUN ? sprintf("reload: the super shotgun open %g spent %g\n",
        !!(f & QVR_WPNFLAG_SSG_OPEN), VR_Reload_SsgSpentOf(f)) : "";
    bprint(sprintf("reload: off hand weapon %g clip %g mag %g holds %s | ",
        VRGetEntWeapon(self, cVR_OffHand), VRGetEntWeaponClip(self, cVR_OffHand),
        !(f & QVR_WPNFLAG_NOMAG), VR_Reload_TestHeld(cVR_OffHand)));
    bprint(sprintf("main hand weapon %g clip %g holds %s, main mag %g\n",
        VRGetEntWeapon(self, cVR_MainHand), VRGetEntWeaponClip(self, cVR_MainHand), VR_Reload_TestHeld(cVR_MainHand),
        !(VRGetEntWeaponFlags(self, cVR_MainHand) & QVR_WPNFLAG_NOMAG)));
    bprint(ssg);
};

// The off hand holding a shotgun (given if it holds another weapon or none), the main hand empty.
void() VR_Reload_TestSetup =
{
    if(VRGetEntWeapon(self, cVR_OffHand) != WID_SHOTGUN)
    {
        VR_WeaponUtil_EntAddWeapon(self, WID_SHOTGUN);
        VRGiveEntWeapon(self, cVR_OffHand, WID_SHOTGUN, 0, 0);
    }
    entity held = VR_Carry_Held(self, cVR_MainHand);
    if(held != world)
    {
        VR_Carry_SetHeld(self, cVR_MainHand, world);
        held.carry_player = world;
        remove(held);
    }
    if(VRGetEntWeapon(self, cVR_MainHand) != WID_FIST)
    {
        VRGiveEntWeapon(self, cVR_MainHand, WID_FIST, 0, 0);
    }
};

// One take of a magazine and its counts: `xLeft` of its ammo left, a magazine of `xCount` in the main hand.
entity(float xLeft, float xCount, string xWhat) VR_Reload_TestTakeMag =
{
    VR_Reload_Take(cVR_MainHand);
    entity r = VR_Carry_Held(self, cVR_MainHand);
    float count = (r != world && r.vr_ammo_wid) ? r.vr_ammo_count : -1;
    float left = r != world ? VR_AmmoUtil_GetAmmoCountByAmmoId(self, r.vr_ammo_aid) : -1;
    VR_Reload_Check(left == xLeft && count == xCount,
        sprintf("%s: %g left (want %g), a magazine of %g in the hand (want %g)", xWhat, left, xLeft, count, xCount));
    return r;
};

// One take and its counts: `xShells` left, a round of `xCount` in the main hand (0: none).
entity(float xShells, float xCount, string xWhat) VR_Reload_TestTake =
{
    VR_Reload_Take(cVR_MainHand);
    entity r = VR_Carry_Held(self, cVR_MainHand);
    float count = (r != world && VR_Reload_IsRound(r)) ? r.vr_ammo_count : 0;
    VR_Reload_Check(self.ammo_shells == xShells && count == xCount,
        sprintf("%s: %g shells left (want %g), a round of %g in the hand (want %g)", xWhat, self.ammo_shells, xShells,
            count, xCount));
    return r;
};

// How many rounds lie about (not in a hand).
float() VR_Reload_TestLoose =
{
    float n = 0;
    entity e = VR_Reload_NextRound(world);
    while(e)
    {
        n = n + (e.carry_player == world ? 1 : 0);
        e = VR_Reload_NextRound(e);
    }
    return n;
};

// The super shotgun broken open (phase 2b): in the off hand, the main hand taking pairs; fired shells stay in as spent,
// breaking it open throws them out (and a live one as a round), the pouch always gives a pair, it loads only open, a pair
// with one chamber free loads one (the other stays in the hand), closed it fires; Break Open off: as before.
void() VR_Reload_SsgSelfTest =
{
    entity r;
    cvar_set("vr_reload_ssg_break", "1");
    VR_WeaponUtil_EntAddWeapon(self, WID_SUPER_SHOTGUN);
    VRGiveEntWeapon(self, cVR_OffHand, WID_SUPER_SHOTGUN, 0, 0);
    VRSetEntWeaponClip(self, cVR_OffHand, 2);
    self.ammo_shells = 10;
    VR_Reload_AmmoChanged(self);
    VR_Reload_Check(VR_Reload_ManualFor(WID_SUPER_SHOTGUN) && VR_Reload_AmmoFor(self, cVR_OffHand) == AID_SHELLS &&
        !VR_Reload_SsgIsOpen(self, cVR_OffHand), "immersive: the super shotgun loads by hand (shut)");
    VRTryReloadWeapon(self, cVR_OffHand, WID_SUPER_SHOTGUN, 0);
    VRSetEntWeaponClip(self, cVR_OffHand, 0);
    VR_Reload_SsgFired(cVR_OffHand, 2);
    VR_Reload_Check(VR_Reload_SsgSpentOf(VRGetEntWeaponFlags(self, cVR_OffHand)) == 2 && self.ammo_shells == 10,
        "both barrels fired: two spent shells stay in (no reload)");
    r = VR_Reload_TestTake(8, 2, "the super shotgun: always a taped pair");
    VR_Reload_Check(!VR_Reload_Load(cVR_MainHand, r) && VRGetEntWeaponClip(self, cVR_OffHand) == 0 && r.vr_ammo_full,
        "shut, its chambers take nothing (the can't click)");
    r.vr_ammo_full = FALSE;
    // One live, one spent: broken open, the spent one out (the casings), the live one a round lying about.
    VRSetEntWeaponClip(self, cVR_OffHand, 1);
    float f = VRGetEntWeaponFlags(self, cVR_OffHand);
    VRSetEntWeaponFlags(self, cVR_OffHand, (f - (f & QVR_WPNFLAG_SSG_SPENT)) | QVR_WPNFLAG_SSG_SPENT_ONE);
    float loose = VR_Reload_TestLoose();
    VR_Reload_SsgBreak(self, cVR_OffHand, "the test");
    f = VRGetEntWeaponFlags(self, cVR_OffHand);
    VR_Reload_Check(VR_Reload_SsgIsOpen(self, cVR_OffHand) && VR_Reload_SsgSpentOf(f) == 0 &&
        VRGetEntWeaponClip(self, cVR_OffHand) == 0 && VR_Reload_TestLoose() == loose + 1,
        sprintf("broken open: open, empty, the live shell thrown out (%g lying about, want %g)", VR_Reload_TestLoose(),
            loose + 1));
    VR_Reload_Check(VR_Reload_Load(cVR_MainHand, r) && VRGetEntWeaponClip(self, cVR_OffHand) == 2 &&
        VR_Carry_Held(self, cVR_MainHand) == world, "open, the pair in the chambers: both loaded");
    VR_Reload_SsgClose(self, cVR_OffHand, "the test");
    VR_Reload_Check(!VR_Reload_SsgIsOpen(self, cVR_OffHand) && VRGetEntWeaponClip(self, cVR_OffHand) == 2,
        "closed: two loaded");
    // One chamber free (a live shell taken back from the floor loaded first): a pair loads one, the other stays in hand.
    f = VRGetEntWeaponFlags(self, cVR_OffHand);
    VRSetEntWeaponFlags(self, cVR_OffHand, f | QVR_WPNFLAG_SSG_OPEN);
    VRSetEntWeaponClip(self, cVR_OffHand, 1);
    // (The author: one chamber loaded, the pouch gives a single shell; a pair only for two empty ones.)
    r = VR_Reload_TestTake(7, 1, "one chamber loaded: the pouch gives a single shell");
    VR_Reload_Check(VR_Reload_Load(cVR_MainHand, r) && VRGetEntWeaponClip(self, cVR_OffHand) == 2,
        "the single shell into the free chamber: both loaded");
    // A pair brought anyway (shell pairs, one taken off the floor): one in, the other stays in the hand.
    VRSetEntWeaponClip(self, cVR_OffHand, 1);
    r = VR_Reload_MakeRound(0, AID_SHELLS, 2, VRGetEntHandPos(self, cVR_MainHand), VRGetEntHandRot(self, cVR_MainHand));
    vr_carry_quiet = TRUE;
    VR_Carry_Start(self, cVR_MainHand, r);
    vr_carry_quiet = FALSE;
    VR_Reload_Check(!VR_Reload_Load(cVR_MainHand, r) && VRGetEntWeaponClip(self, cVR_OffHand) == 2 &&
        r.vr_ammo_count == 1 && VR_Carry_Held(self, cVR_MainHand) == r,
        sprintf("one chamber free, a pair: one in, the other stays in the hand (a round of %g)", r.vr_ammo_count));
    VR_Reload_Refund(self, r, cVR_MainHand);
    VR_Reload_SsgClose(self, cVR_OffHand, "the test");
    // Break Open off: the super shotgun as before (the flick's or the holsters' reload; spent shells not kept).
    cvar_set("vr_reload_ssg_break", "0");
    VRSetEntWeaponClip(self, cVR_OffHand, 0);
    self.ammo_shells = 10;
    VRTryReloadWeapon(self, cVR_OffHand, WID_SUPER_SHOTGUN, 0);
    VR_Reload_Check(!VR_Reload_ManualFor(WID_SUPER_SHOTGUN) && VRGetEntWeaponClip(self, cVR_OffHand) == 2 &&
        self.ammo_shells == 8, sprintf("Break Open off: it reloads as before (magazine %g, %g shells; want 2, 8)",
            VRGetEntWeaponClip(self, cVR_OffHand), self.ammo_shells));
    VRSetEntWeaponFlags(self, cVR_OffHand, 0);
    VRGiveEntWeapon(self, cVR_OffHand, WID_SHOTGUN, 0, 0);
    self.ammo_shells = 10;
    VR_Reload_AmmoChanged(self);
};

// The magazines (phase 2): the nailgun in the off hand, the main hand taking, seating, pulling and bumping; the super
// nailgun's and the thunderbolt's.
void() VR_Reload_MagSelfTest =
{
    VR_WeaponUtil_EntAddWeapon(self, WID_NAILGUN);
    VRGiveEntWeapon(self, cVR_OffHand, WID_NAILGUN, 0, 24);
    self.ammo_nails = 50;
    VR_Reload_Check(VR_Reload_MagIn(self, cVR_OffHand) && VR_Reload_ManualFor(WID_NAILGUN),
        "nailgun: a magazine in, loaded by hand");
    // Out (as B/Y does): the gun empty, no magazine; a magazine of 24 lying about.
    entity r = VR_Reload_Eject(self, cVR_OffHand, '0 0 0', -1, "test");
    VR_Reload_Check(!VR_Reload_MagIn(self, cVR_OffHand) && VRGetEntWeaponClip(self, cVR_OffHand) == 0 &&
                    (VRGetEntWeaponFlags(self, cVR_OffHand) & QVR_WPNFLAG_NOMAG) && r.classname == VR_RELOAD_MAGCLASS &&
                    r.vr_ammo_count == 24 && r.vr_ammo_wid == WID_NAILGUN && r.model == "progs/vr_mag_nail.mdl",
        sprintf("ejected: the gun empty, no magazine; a magazine of %g lying about", r.vr_ammo_count));
    VR_Reload_Check(VR_Forcegrab_IsEligible(r), "the ejected magazine can be force grabbed");
    remove(r);
    // From the pouch: a full one (24 of 50).
    r = VR_Reload_TestTakeMag(26, 24, "take a magazine");
    VR_Reload_Check(VR_Reload_MagAtWell(cVR_MainHand, r) && VR_Reload_MagIn(self, cVR_OffHand) &&
                    VRGetEntWeaponClip(self, cVR_OffHand) == 24 && VR_Carry_Held(self, cVR_MainHand) == world,
        sprintf("seated: the gun holds %g (want 24), the hand empty", VRGetEntWeaponClip(self, cVR_OffHand)));
    // Partly used: out into the main hand (the pull), its 10 back into the pouch.
    VRSetEntWeaponClip(self, cVR_OffHand, 10);
    r = VR_Reload_Eject(self, cVR_OffHand, '0 0 0', cVR_MainHand, "test pull");
    VR_Reload_Check(VR_Carry_Held(self, cVR_MainHand) == r && r.vr_ammo_count == 10, "pulled out into the hand: 10 in it");
    VR_Reload_Refund(self, r, cVR_MainHand);
    VR_Reload_Check(self.ammo_nails == 36, sprintf("a part-used magazine put back: %g nails (want 36)", self.ammo_nails));
    // A short reserve: the magazine holds what there is.
    self.ammo_nails = 5;
    r = VR_Reload_TestTakeMag(0, 5, "the reserve short: a magazine of 5");
    VR_Reload_Check(VR_Reload_MagAtWell(cVR_MainHand, r) && VRGetEntWeaponClip(self, cVR_OffHand) == 5, "seated: 5");
    // The bump, in two steps: a new magazine meeting the one in slowly does nothing; hard, it only knocks it out (the new
    // one still in the hand); it seats once brought back to the well, not in the same touch.
    self.ammo_nails = 30;
    r = VR_Reload_TestTakeMag(6, 24, "take a magazine for the bump");
    self.vr_mag_seated0 = time - 1; // (seated a while ago: a hit just after one seats does nothing)
    self.offloadportpos = VR_Reload_MagRef(r); // (its top at the well)
    VR_Reload_HitFrame(cVR_OffHand);
    VR_Reload_Check(!VR_Reload_HeldFrame(cVR_MainHand, r) && VRGetEntWeaponClip(self, cVR_OffHand) == 5 &&
                    VR_Carry_Held(self, cVR_MainHand) == r, "too slow for a bump: nothing changes");
    self.handvel = '0 0 3'; // (the main hand meets the gun at 3 m/s; the engine sets it again next frame)
    VR_Reload_HitFrame(cVR_OffHand);
    self.handvel = '0 0 0';
    VR_Reload_Check(!VR_Reload_MagIn(self, cVR_OffHand) && VRGetEntWeaponClip(self, cVR_OffHand) == 0 &&
                    VR_Carry_Held(self, cVR_MainHand) == r, "a hard bump only knocks the old one out (the new one still in the hand)");
    VR_Reload_Check(!VR_Reload_HeldFrame(cVR_MainHand, r) && !VR_Reload_MagIn(self, cVR_OffHand),
        "not seated in the same touch");
    self.offloadportpos = VR_Reload_MagRef(r) + '0 0 40'; // (taken away from the well)
    VR_Reload_HeldFrame(cVR_MainHand, r);
    self.offloadportpos = VR_Reload_MagRef(r); // (and back)
    VR_Reload_Check(VR_Reload_HeldFrame(cVR_MainHand, r) && VRGetEntWeaponClip(self, cVR_OffHand) == 24,
        sprintf("brought back to the well: seated (the gun holds %g, want 24)", VRGetEntWeaponClip(self, cVR_OffHand)));
    entity old = world;
    entity e = find(world, classname, VR_RELOAD_MAGCLASS);
    while(e)
    {
        if(e.carry_player == world && e.vr_ammo_count == 5)
        {
            old = e;
        }
        e = find(e, classname, VR_RELOAD_MAGCLASS);
    }
    VR_Reload_Check(old != world, "the old magazine (5) knocked out, lying about");
    if(old)
    {
        remove(old);
    }
    // The super nailgun's magazine doesn't fit the nailgun; the thunderbolt's holds cells.
    VR_WeaponUtil_EntAddWeapon(self, WID_SUPER_NAILGUN);
    VRGiveEntWeapon(self, cVR_OffHand, WID_SUPER_NAILGUN, QVR_WPNFLAG_NOMAG, 0);
    self.ammo_nails = 100;
    r = VR_Reload_TestTakeMag(64, 36, "super nailgun: a magazine of 36");
    VR_Reload_Check(r.model == "progs/vr_mag_snail.mdl", "its own magazine");
    VR_WeaponUtil_EntAddWeapon(self, WID_NAILGUN);
    VRGiveEntWeapon(self, cVR_OffHand, WID_NAILGUN, QVR_WPNFLAG_NOMAG, 0);
    VR_Reload_Check(!VR_Reload_MagFits(self, cVR_OffHand, r), "it doesn't fit the nailgun");
    VR_Reload_Refund(self, r, cVR_MainHand);
    VR_WeaponUtil_EntAddWeapon(self, WID_LIGHTNING);
    VRGiveEntWeapon(self, cVR_OffHand, WID_LIGHTNING, QVR_WPNFLAG_NOMAG, 0);
    self.ammo_cells = 40;
    r = VR_Reload_TestTakeMag(4, 36, "thunderbolt: a cell of 36");
    VR_Reload_Check(r.vr_ammo_aid == AID_CELLS && r.model == "progs/vr_mag_light.mdl" &&
                    VR_Reload_MagAtWell(cVR_MainHand, r) && VRGetEntWeaponClip(self, cVR_OffHand) == 36,
        "the cell seated: 36 cells in the thunderbolt");
};

void() VR_Reload_SelfTest =
{
    entity r;
    vr_reload_test_pass = 0;
    vr_reload_test_fail = 0;
    float mode = cvar_hget(cvarh_vr_reload_mode);
    float pairs = cvar_hget(cvarh_vr_reload_shell_pairs);
    float ssgBreak = cvar_hget(cvarh_vr_reload_ssg_break);
    cvar_set("vr_reload_mode", "3");
    cvar_set("vr_reload_shell_pairs", "0");
    cvar_set("vr_reload_ssg_break", "0"); // (phases 1 and 2: the super shotgun as before; its own section below)
    VR_Reload_TestSetup();
    VR_Reload_Check(VR_Reload_Immersive() && VR_Reload_ManualFor(WID_SHOTGUN) && !VR_Reload_ManualFor(WID_SUPER_SHOTGUN),
        "immersive: the shotgun loads by hand, the super shotgun does not");
    VR_Reload_Check(VR_Reload_AmmoFor(self, cVR_OffHand) == AID_SHELLS, "the pouch gives shells for the off hand's shotgun");
    VRSetEntWeaponClip(self, cVR_OffHand, 0);
    self.ammo_shells = 20;
    VR_Reload_AmmoChanged(self);

    // Take, load, until full.
    r = VR_Reload_TestTake(19, 1, "take");
    VR_Reload_Check(VR_Reload_Load(cVR_MainHand, r) && VRGetEntWeaponClip(self, cVR_OffHand) == 1 &&
                    VR_Carry_Held(self, cVR_MainHand) == world && self.ammo_shells == 19,
        sprintf("load: magazine %g (want 1), the hand empty, 19 shells", VRGetEntWeaponClip(self, cVR_OffHand)));
    float k;
    for(k = 0; k < 7; k++)
    {
        VR_Reload_Take(cVR_MainHand);
        r = VR_Carry_Held(self, cVR_MainHand);
        if(r != world)
        {
            VR_Reload_Load(cVR_MainHand, r);
        }
    }
    VR_Reload_Check(VRGetEntWeaponClip(self, cVR_OffHand) == 8 && self.ammo_shells == 12,
        sprintf("loaded until full: magazine %g (want 8), %g shells (want 12)", VRGetEntWeaponClip(self, cVR_OffHand),
            self.ammo_shells));
    r = VR_Reload_TestTake(11, 1, "take with the gun full");
    VR_Reload_Check(!VR_Reload_Load(cVR_MainHand, r) && VRGetEntWeaponClip(self, cVR_OffHand) == 8 &&
                    VR_Carry_Held(self, cVR_MainHand) == r,
        "the full gun takes nothing: the shell stays in the hand");

    // Refund.
    VR_Reload_Check(VR_Reload_Refund(self, r, cVR_MainHand) && self.ammo_shells == 12 &&
                    VR_Carry_Held(self, cVR_MainHand) == world,
        sprintf("put back: %g shells (want 12), the hand empty", self.ammo_shells));

    // Waste (dropped), taken again, put back.
    r = VR_Reload_TestTake(11, 1, "take to drop");
    VR_Carry_Release(cVR_MainHand, r);
    VR_Reload_Check(VR_Carry_Held(self, cVR_MainHand) == world && r.classname == VR_RELOAD_CLASS &&
                    r.carry_player == world && self.ammo_shells == 11 && (r.flags & FL_FORCEGRABBABLE) && r.solid != SOLID_NOT,
        sprintf("dropped: lying about (force grabbable), %g shells (want 11: it is out of the pouch)", self.ammo_shells));
    VR_Reload_Check(VR_Forcegrab_IsEligible(r), "the force grab finds it eligible");
    VR_Carry_Start(self, cVR_MainHand, r);
    VR_Reload_Check(VR_Carry_Held(self, cVR_MainHand) == r, "taken again by hand");
    VR_Reload_Refund(self, r, cVR_MainHand);
    VR_Reload_Check(self.ammo_shells == 12, sprintf("and put back: %g shells (want 12)", self.ammo_shells));

    // The taped pair.
    cvar_set("vr_reload_shell_pairs", "1");
    VRSetEntWeaponClip(self, cVR_OffHand, 7);
    r = VR_Reload_TestTake(10, 2, "take a taped pair");
    VR_Reload_Check(r.model == VR_RELOAD_PAIR, "the pair's model");
    VR_Reload_Check(!VR_Reload_Load(cVR_MainHand, r) && VRGetEntWeaponClip(self, cVR_OffHand) == 8 &&
                    r.vr_ammo_count == 1 && r.model == VR_RELOAD_SHELL,
        sprintf("a pair into a gun with one place: magazine %g (want 8), one shell left in the hand",
            VRGetEntWeaponClip(self, cVR_OffHand)));
    VR_Reload_Refund(self, r, cVR_MainHand);
    VR_Reload_Check(self.ammo_shells == 11, sprintf("the pair's other shell put back: %g shells (want 11)", self.ammo_shells));
    VRSetEntWeaponClip(self, cVR_OffHand, 0);
    r = VR_Reload_TestTake(9, 2, "another pair");
    VR_Reload_Check(VR_Reload_Load(cVR_MainHand, r) && VRGetEntWeaponClip(self, cVR_OffHand) == 2,
        sprintf("both into the gun: magazine %g (want 2)", VRGetEntWeaponClip(self, cVR_OffHand)));
    self.ammo_shells = 1;
    r = VR_Reload_TestTake(0, 1, "one shell left: a single one, not a pair");
    VR_Reload_Load(cVR_MainHand, r);
    cvar_set("vr_reload_shell_pairs", "0");

    // The empty pouch.
    VR_Reload_TestTake(0, 0, "the empty pouch gives nothing");

    // By ammo, not gun: the super shotgun (no immersive rules) gets shells all the same; it doesn't take them.
    VRSetEntWeaponClip(self, cVR_OffHand, 0);
    self.ammo_shells = 10;
    VR_WeaponUtil_EntAddWeapon(self, WID_SUPER_SHOTGUN);
    VRGiveEntWeapon(self, cVR_OffHand, WID_SUPER_SHOTGUN, 0, 0);
    r = VR_Reload_TestTake(9, 1, "the super shotgun: shells all the same");
    VR_Reload_Check(!VR_Reload_HeldFrame(cVR_MainHand, r) && VRGetEntWeaponClip(self, cVR_OffHand) == 0,
        "the super shotgun doesn't take them by hand");
    VR_Reload_Refund(self, r, cVR_MainHand);
    // The last kind: the nailgun held last, then the off hand emptied (or an axe in it): its magazines still.
    VR_WeaponUtil_EntAddWeapon(self, WID_NAILGUN);
    VRGiveEntWeapon(self, cVR_OffHand, WID_NAILGUN, 0, 0);
    self.ammo_nails = 30;
    VR_Reload_PlayerFrame();
    VR_Reload_Check(self.vr_pouch_kind == 2 && self.vr_pouch_count == 30,
        sprintf("the pouch shows nailgun magazines (kind %g, want 2), 30 nails (%g)", self.vr_pouch_kind,
            self.vr_pouch_count));
    VRGiveEntWeapon(self, cVR_OffHand, WID_FIST, 0, 0);
    VR_Reload_PlayerFrame();
    r = VR_Reload_TestTakeMag(6, 24, "empty hands: the last kind, a nailgun magazine");
    VR_Reload_Check(r.vr_ammo_wid == WID_NAILGUN, "a nailgun's magazine");
    VR_Reload_Refund(self, r, cVR_MainHand);
    VRGiveEntWeapon(self, cVR_OffHand, WID_AXE, 0, 0);
    VR_Reload_PlayerFrame();
    r = VR_Reload_TestTakeMag(6, 24, "an axe in the other hand: the last kind still");
    VR_Reload_Refund(self, r, cVR_MainHand);
    VRGiveEntWeapon(self, cVR_OffHand, WID_SUPER_SHOTGUN, 0, 0);
    VR_Reload_PlayerFrame();
    VR_Reload_Check(self.vr_pouch_kind == 1 && self.vr_pouch_count == 10, sprintf("back to shells: kind %g, %g shells",
        self.vr_pouch_kind, self.vr_pouch_count));
    VRGiveEntWeapon(self, cVR_OffHand, WID_FIST, 0, 0);
    VR_Reload_PlayerFrame();
    r = VR_Reload_TestTake(9, 1, "empty hands after the super shotgun: a shell");
    VR_Reload_Refund(self, r, cVR_MainHand);
    self.ammo_shells = 10;

    VR_Reload_MagSelfTest();
    VR_Reload_SsgSelfTest();
    cvar_set("vr_reload_ssg_break", "0");

    // The modes: the holsters' reload (2) fills the shotgun from the reserve, Immersive (3) doesn't, Off (0) has no
    // magazines.
    VRGiveEntWeapon(self, cVR_OffHand, WID_SHOTGUN, 0, 0);
    cvar_set("vr_reload_mode", "2");
    VR_Reload_Check(!VR_Reload_ManualFor(WID_SHOTGUN) && VR_Reload_AmmoFor(self, cVR_OffHand) < 0,
        "hip holsters: the shotgun doesn't load by hand");
    VRTryReloadWeapon(self, cVR_OffHand, WID_SHOTGUN, 0);
    VR_Reload_Check(VRGetEntWeaponClip(self, cVR_OffHand) == 8 && self.ammo_shells == 2,
        sprintf("hip holsters: the holster reload fills it (magazine %g, %g shells; want 8, 2)",
            VRGetEntWeaponClip(self, cVR_OffHand), self.ammo_shells));
    cvar_set("vr_reload_mode", "3");
    VRSetEntWeaponClip(self, cVR_OffHand, 0);
    self.ammo_shells = 10;
    VRTryReloadWeapon(self, cVR_OffHand, WID_SHOTGUN, 0);
    VR_Reload_Check(VRGetEntWeaponClip(self, cVR_OffHand) == 0 && self.ammo_shells == 10,
        "immersive: no holster or button reload for the shotgun");
    VRGiveEntWeapon(self, cVR_OffHand, WID_SUPER_SHOTGUN, 0, 0);
    VRTryReloadWeapon(self, cVR_OffHand, WID_SUPER_SHOTGUN, 0);
    VR_Reload_Check(VRGetEntWeaponClip(self, cVR_OffHand) == 2, sprintf("immersive: the super shotgun reloads as "
        "before (magazine %g, want 2)", VRGetEntWeaponClip(self, cVR_OffHand)));
    VRGiveEntWeapon(self, cVR_OffHand, WID_NAILGUN, 0, 0);
    VRSetEntWeaponClip(self, cVR_OffHand, 0);
    self.ammo_nails = 50;
    VRTryReloadWeapon(self, cVR_OffHand, WID_NAILGUN, 0);
    VR_Reload_Check(VRGetEntWeaponClip(self, cVR_OffHand) == 0 && self.ammo_nails == 50,
        "immersive: no holster or button reload for the nailgun (magazines)");
    cvar_set("vr_reload_mode", "2");
    VRTryReloadWeapon(self, cVR_OffHand, WID_NAILGUN, 0);
    VR_Reload_Check(VRGetEntWeaponClip(self, cVR_OffHand) == 24 && !VR_Reload_MagIn(self, cVR_OffHand),
        sprintf("hip holsters: the nailgun reloads at a holster as before (magazine %g, want 24), no magazines",
            VRGetEntWeaponClip(self, cVR_OffHand)));
    cvar_set("vr_reload_mode", "0");
    VR_Reload_Check(!VRIsWeaponReloadingEnabledFor(self, cVR_OffHand) && !VR_Reload_Immersive(), "off: no magazines");

    VRGiveEntWeapon(self, cVR_OffHand, WID_SHOTGUN, 0, 0);
    VRSetEntWeaponClip(self, cVR_OffHand, 0);
    self.ammo_shells = 20;
    VR_Reload_AmmoChanged(self);
    cvar_set("vr_reload_mode", ftos(mode));
    cvar_set("vr_reload_shell_pairs", ftos(pairs));
    cvar_set("vr_reload_ssg_break", ftos(ssgBreak));
    bprint(sprintf("reload: %g passed, %g failed\n", vr_reload_test_pass, vr_reload_test_fail));
};

void() VR_Reload_Test =
{
    float step = cvar_hget(cvarh_vr_reload_test);
    entity r = VR_Carry_Held(self, cVR_MainHand);
    if(step == 1)
    {
        if(r != world || !VRIsHandEmpty(self, cVR_MainHand))
        {
            bprint("reload: the main hand isn't empty\n");
        }
        else if(VR_Reload_AmmoFor(self, cVR_OffHand) < 0)
        {
            bprint("reload: the off hand holds no gun that loads by hand (immersive reloading on?)\n");
        }
        else
        {
            VR_Reload_Take(cVR_MainHand);
        }
    }
    else if(step == 2 || step == 3 || step == 4)
    {
        if(r == world || !VR_Reload_IsRound(r))
        {
            bprint("reload: the main hand holds no round\n");
        }
        else if(step == 2)
        {
            if(VR_Reload_AmmoFor(self, cVR_OffHand) != r.vr_ammo_aid)
            {
                bprint("reload: the off hand holds no gun for it\n");
            }
            else
            {
                VR_Reload_Load(cVR_MainHand, r);
            }
        }
        else if(step == 3)
        {
            VR_Carry_Release(cVR_MainHand, r);
        }
        else
        {
            VR_Reload_Refund(self, r, cVR_MainHand);
        }
    }
    else if(step == 5)
    {
        float w = VRGetEntWeapon(self, cVR_OffHand);
        if(w != WID_FIST)
        {
            VR_AmmoUtil_AddAmmoCountByAmmoId(self, WeaponIdToAmmoId(w, VRGetEntWeaponFlags(self, cVR_OffHand)),
                VRGetEntWeaponClip(self, cVR_OffHand));
            VRSetEntWeaponClip(self, cVR_OffHand, 0);
            VR_Reload_AmmoChanged(self);
        }
    }
    else if(step == 6)
    {
        if(VR_Reload_MagIn(self, cVR_OffHand))
        {
            VR_Reload_Eject(self, cVR_OffHand, '0 0 -40', -1, "test step 6");
        }
        else
        {
            bprint("reload: the off hand's gun has no magazine in (immersive reloading on?)\n");
        }
    }
    else if(step == 9)
    {
        VR_Reload_SelfTest();
    }
    VR_Reload_TestReport();
};
