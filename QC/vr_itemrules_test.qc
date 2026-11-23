#ifdef QVR_TESTS
// vr_pickup_test 19 (vr_itemrules_test.qc): an item's targets fire only when it is taken (itemTouch, honey_items.qc:
// item_taken). Each item is spawned ahead of you by its map spawn function, placed at once (VRPlaceItemNow), given a
// target that counts its uses, and touched through itemTouch (what a hand, the body and a box carried to a holster
// reach) first in a way that takes nothing, then in one that takes it: a health box at full health, a box of shells at
// the cap, the silver key already held, green armour under better armour, a super shotgun brushed by a hand that
// doesn't grip (then gripped). Not taken: the item is still there and its target has not fired; taken: it fired once.
// With Honey's extensions on (the start map with vr_honey_context 1) a secret health box also counts as found only
// when taken. Verdict "itemtargets: N passed, M failed". Single player, any map (e1m1).

float vr_itt_fired; // the counting target's uses

void() VR_ITT_Use =
{
    vr_itt_fired = vr_itt_fired + 1;
};

// The map's own spawn function of `xClass` at `xAt`, placed now, its target the counting one ("vr_itt_target").
entity(string xClass, vector xAt, float xFlags) VR_ITT_Spawn =
{
    entity oldself = self;
    entity it = spawn();
    setorigin(it, xAt);
    it.classname = xClass;
    it.spawnflags = xFlags;
    it.target = "vr_itt_target";
    self = it;
    if(xClass == "item_health") { item_health(); }
    else if(xClass == "item_shells") { item_shells(); }
    else if(xClass == "item_key1") { item_key1(); }
    else if(xClass == ITEM_NAME_ARMOR1) { item_armor1(); }
    else { weapon_supershotgun(); }
    VRPlaceItemNow();
    self = oldself;
    return it;
};

// `p`'s touch of `xItem` through itemTouch by the main hand (what its handtouch, or a box's carry_use, is just placed).
void(entity p, entity xItem) VR_ITT_Touch =
{
    entity oldself = self, oldother = other;
    p.touchinghand = cVR_MainHand;
    xItem.handtouch_hand = cVR_MainHand;
    xItem.handtouch_ent = p;
    other = p;
    self = xItem;
    itemTouch();
    self = oldself;
    other = oldother;
};

// One item: touched as `xLeave` sets the player (nothing taken), then as `xTake` does (taken).
float vr_itt_case;
void(entity p, float xStep) VR_ITT_Set =
{
    float k = vr_itt_case;
    if(k == 0) { p.health = xStep ? 50 : p.max_health; }
    else if(k == 1) { p.ammo_shells = xStep ? 0 : VR_MaxAmmo(p, AID_SHELLS); }
    else if(k == 2) { if(xStep) { VR_ItemUtil_EntDelItem(p, IID_KEY1); } else { VR_ItemUtil_EntAddItem(p, IID_KEY1); } }
    else if(k == 3) { p.armortype = xStep ? 0 : 0.8; p.armorvalue = xStep ? 0 : 200; }
    else { VRSetMainHandGrabbingBit(p, xStep); }
};

void(entity p, string xClass, string xWhat, float xFlags) VR_ITT_Case =
{
    makevectors(p.v_angle_y * '0 1 0');
    entity it = VR_ITT_Spawn(xClass, p.origin + v_forward * 64, xFlags);
    VR_Test_Check(it.handtouch == itemTouch || it.carry_use == itemTouch,
        sprintf("%s: its pickup goes through itemTouch", xClass));
    float secrets = found_secrets;
    vr_itt_fired = 0;
    VR_ITT_Set(p, 0);
    VR_ITT_Touch(p, it);
    VR_Test_Check(it.model != string_null && vr_itt_fired == 0,
        sprintf("%s touched %s: not taken (%s), its target fired %g times (0)", xClass, xWhat,
        it.model != string_null ? "still there" : "gone", vr_itt_fired));
    if(xFlags & HONEY_SPAWNFLAG_ITEM_SECRET)
    {
        VR_Test_Check(found_secrets == secrets, sprintf("%s: a secret not taken is not found (%g, was %g)", xClass,
            found_secrets, secrets));
    }
    VR_ITT_Set(p, 1);
    VR_ITT_Touch(p, it);
    VR_Test_Check(it.model == string_null && vr_itt_fired == 1,
        sprintf("%s taken: %s, its target fired %g times (1)", xClass, it.model == string_null ? "gone" : "still there",
        vr_itt_fired));
    if(xFlags & HONEY_SPAWNFLAG_ITEM_SECRET)
    {
        VR_Test_Check(found_secrets == secrets + 1, sprintf("%s: the secret taken is found (%g, was %g)", xClass,
            found_secrets, secrets));
    }
    vr_itt_case = vr_itt_case + 1;
};

void() VR_ItemTargetsTest_Run =
{
    VR_Test_BeginFlags("itemtargets", VR_TEST_LOUD);
    entity p = self;
    entity counter = spawn();
    counter.classname = "vr_itt_counter";
    counter.targetname = "vr_itt_target";
    counter.use = VR_ITT_Use;
    float oldHealth = p.health, oldShells = p.ammo_shells, oldType = p.armortype, oldArmor = p.armorvalue;
    float hadKey = VR_ItemUtil_EntHasItem(p, IID_KEY1);
    VR_Test_Check(VRIsHandEmpty(p, cVR_MainHand), "the main hand is empty (for the weapon's grip)");

    vr_itt_case = 0;
    VR_ITT_Case(p, "item_health", "at full health", 0);
    VR_ITT_Case(p, "item_shells", "with shells at the cap", 0);
    VR_ITT_Case(p, "item_key1", "with the key held", 0);
    VR_ITT_Case(p, ITEM_NAME_ARMOR1, "under red armour", 0);
    VR_ITT_Case(p, "weapon_supershotgun", "by a hand that doesn't grip", 0);
    VRSetMainHandGrabbingBit(p, FALSE);
    if(VR_GameUtil_EnableHoneyExtensions())
    {
        vr_itt_case = 0;
        VR_ITT_Case(p, "item_health", "at full health (a Honey secret)", HONEY_SPAWNFLAG_ITEM_SECRET);
    }
    else
    {
        VR_Test_Print("itemtargets: (Honey's extensions off here: the secret case not run)\n");
    }

    remove(counter);
    VRSetEntWeapon(p, cVR_MainHand, WID_FIST);
    p.health = oldHealth;
    p.ammo_shells = oldShells;
    p.armortype = oldType;
    p.armorvalue = oldArmor;
    if(hadKey) { VR_ItemUtil_EntAddItem(p, IID_KEY1); } else { VR_ItemUtil_EntDelItem(p, IID_KEY1); }
    VR_Test_Verdict();
};

// vr_pickup_test 20 (vr_itemrules_test.qc): the ammo boxes' weapon roll (items.qc SpawnAmmoBoxWeapons): Rogue's three
// boxes are ammo boxes; the map's roll ran at the first PutClientInServer, every ammo box of the map rolled exactly once
// (.vr_ammobox_rolls) and against the player; a box placed now rolls once at its placement against him; the map's roll
// asked again (a coop respawn's PutClientInServer) rolls nothing. Verdict "ammoroll: N passed, M failed". Single
// player, a map with ammo boxes (e1m1).
void() VR_AmmoRollTest_Run =
{
    VR_Test_BeginFlags("ammoroll", VR_TEST_LOUD);
    entity p = self;
    entity probe = spawn();
    float ok = TRUE;
    string names[8] = { "item_shells", "item_spikes", "item_rockets", "item_cells", "item_lava_spikes",
                        "item_multi_rockets", "item_plasma", "item_health" };
    for(float i = 0; i < 8; i++)
    {
        probe.classname = names[i];
        if(IsAmmoBox(probe) != (i < 7))
        {
            ok = FALSE;
            VR_Test_Print(sprintf("ammoroll: IsAmmoBox(%s) is %g\n", names[i], IsAmmoBox(probe)));
        }
    }
    remove(probe);
    VR_Test_Check(ok, "the seven ammo boxes (Rogue's lava nails, multi-rockets and plasma too) are ammo boxes, health isn't");
    VR_Test_Check(vr_ammobox_rolled_map, "the map's roll ran (the first PutClientInServer)");
    VR_Test_Check(vr_ammobox_roller == p, sprintf("the last roll was against the player (%s)", vr_ammobox_roller.classname));

    float boxes = 0, once = 0;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(IsAmmoBox(e))
        {
            boxes = boxes + 1;
            if(e.vr_ammobox_rolls == 1) { once = once + 1; }
            else { VR_Test_Print(sprintf("ammoroll: %s at %v rolled %g times\n", e.classname, e.origin, e.vr_ammobox_rolls)); }
        }
    }
    VR_Test_Check(boxes > 0 && once == boxes, sprintf("each of the map's %g ammo boxes rolled exactly once (%g)", boxes, once));

    vr_ammobox_roller = world;
    makevectors(p.v_angle_y * '0 1 0');
    entity oldself = self;
    entity box = spawn();
    setorigin(box, p.origin + v_forward * 64);
    box.classname = "item_shells";
    self = box;
    item_shells();
    VRPlaceItemNow();
    self = oldself;
    VR_Test_Check(box.vr_ammobox_rolls == 1 && vr_ammobox_roller == p,
        sprintf("a box placed now rolled once at its placement (%g), against the player (%s)", box.vr_ammobox_rolls,
        vr_ammobox_roller.classname));

    SpawnAllAmmoBoxWeapons(p); // (a coop respawn's PutClientInServer)
    float again = 0;
    for(entity e2 = nextent(world); e2; e2 = nextent(e2))
    {
        if(IsAmmoBox(e2) && e2.vr_ammobox_rolls != 1) { again = again + 1; }
    }
    VR_Test_Check(again == 0, sprintf("the map's roll asked again rolls nothing (%g boxes rolled more than once)", again));
    remove(box);
    VR_Test_Verdict();
};
// vr_pickup_test 21 (vr_itemrules_test.qc): CTF's level change (client.qc SetChangeParms, teamplay 4): nothing carried, as
// a dead player's: the weapons' flags, magazines and ids are a new game's (0; the hip shotgun's own magazine, parm36, as
// SetNewParms fills it), the team kept (parm22), the next weapon id SetNewParms's. The player's weapons are given
// flags and a magazine first (their records have ids); teamplay is put back. Verdict "ctfparms: N passed, M failed". Single player (e1m1).
void() VR_CtfParmsTest_Run =
{
    VR_Test_BeginFlags("ctfparms", VR_TEST_LOUD);
    entity p = self;
    float oldTeamplay = cvar_hget(cvarh_teamplay);
    p.weaponflags = 1; p.weaponflags2 = 1; p.holsterweaponflags2 = 1;
    if(p.holsterweaponinst2) { p.holsterweaponinst2.wi_clip = 7; } // (the axe's record; the holsters' ids are theirs)
    p.steam = 2;
    cvar_set("teamplay", ftos(TEAM_CTF));
    SetChangeParms();
    cvar_set("teamplay", ftos(oldTeamplay));
    float bad = 0;
    if(parm23 || parm24 || parm25 || parm26 || parm27 || parm28 || parm29 || parm30) { bad = bad + 1; }
    if(parm31 || parm32 || parm33 || parm34 || parm35 || parm37 || parm38) { bad = bad + 2; }
    if(parm41 || parm42 || parm43 || parm44 || parm45 || parm46 || parm47 || parm48) { bad = bad + 4; }
    VR_Test_Check(bad == 0, sprintf("the weapons' flags, magazines and ids are a new game's (wrong groups %g)", bad));
    VR_Test_Check(parm8 == WID_FIST && parm14 == WID_SHOTGUN && parm13 == WID_AXE,
        sprintf("the loadout is a new game's (hand %g, holsters %g %g)", parm8, parm13, parm14));
    VR_Test_Check(parm22 == 2, sprintf("the team is kept (parm22 %g)", parm22));
    VR_Test_Check(parm49 == qvr_weaponinst_next, sprintf("the next weapon id is kept (parm49 %g, %g)", parm49,
        qvr_weaponinst_next));
    VR_Test_Verdict();
};
// vr_pickup_test 22 (vr_itemrules_test.qc): each player's readiness after his spawn is his own (.vr_readytime, set by
// PutClientInServer; weapons.qc W_WeaponFrameImpl reads self's). Two players need coop, which a headless run here
// can't make: another player's spawn is stood in for by another entity's vr_readytime 30 s ahead (the global it
// replaced was set so by any client's spawn). With a loaded shotgun in the main hand and the mock trigger held 20
// frames: he fires (another's spawn doesn't hold him); with his own vr_readytime 30 s ahead he doesn't. Verdict
// "readytime: N passed, M failed". Single player, mock headset (e1m1).
float vr_rdt_step;
float vr_rdt_frames;
float vr_rdt_clip;
entity vr_rdt_other;

float() VR_RDT_Clip = { return VRGetEntWeaponClip(VR_Player(), cVR_MainHand); };

void() VR_ReadyTest_Start =
{
    VR_Test_BeginFlags("readytime", VR_TEST_LOUD);
    entity p = self;
    VRGiveEntWeapon(p, cVR_MainHand, WID_SHOTGUN, 0, WeaponIdToBaseClipSize(WID_SHOTGUN));
    p.ammo_shells = max(p.ammo_shells, 25);
    VR_Test_Check(p.vr_readytime > 0 && p.vr_readytime <= time,
        sprintf("his spawn set his own vr_readytime (%g, now %g)", p.vr_readytime, time));
    vr_rdt_other = spawn();
    vr_rdt_other.classname = "vr_rdt_other";
    vr_rdt_other.vr_readytime = time + 30; // (another player just spawned)
    vr_rdt_clip = VR_RDT_Clip();
    localcmd("vr_mock_button main trigger 1\n");
    vr_rdt_frames = 0;
    vr_rdt_step = 1;
};

void() VR_ReadyTest_Frame =
{
    entity p = VR_Player();
    vr_rdt_frames = vr_rdt_frames + 1;
    if(vr_rdt_step == 1 && vr_rdt_frames >= 20)
    {
        localcmd("vr_mock_button main trigger 0\n");
        VR_Test_Check(VR_RDT_Clip() < vr_rdt_clip, sprintf("another player's spawn doesn't hold him: he fired (magazine %g -> %g)",
            vr_rdt_clip, VR_RDT_Clip()));
        vr_rdt_step = 2;
        vr_rdt_frames = 0;
    }
    else if(vr_rdt_step == 2 && vr_rdt_frames >= 40)
    {
        p.vr_readytime = time + 30; // (his own spawn)
        VRGiveEntWeapon(p, cVR_MainHand, WID_SHOTGUN, 0, WeaponIdToBaseClipSize(WID_SHOTGUN)); // (loaded again)
        vr_rdt_clip = VR_RDT_Clip();
        VR_Test_Check(vr_rdt_clip > 0, sprintf("the shotgun is loaded again (%g)", vr_rdt_clip));
        localcmd("vr_mock_button main trigger 1\n");
        vr_rdt_step = 3;
        vr_rdt_frames = 0;
    }
    else if(vr_rdt_step == 3 && vr_rdt_frames >= 20)
    {
        localcmd("vr_mock_button main trigger 0\n");
        VR_Test_Check(VR_RDT_Clip() == vr_rdt_clip, sprintf("his own spawn holds him: he didn't fire (magazine %g -> %g)",
            vr_rdt_clip, VR_RDT_Clip()));
        p.vr_readytime = time;
        remove(vr_rdt_other);
        vr_rdt_step = 0;
        VR_Test_Verdict();
    }
};
// vr_pickup_test 23 (vr_itemrules_test.qc): a player holding Mjolnir is player.mdl, as with any weapon (Hipnotic's
// playham.mdl, the third-person player holding it, is not used: without Hipnotic its model index was 0, the player
// invisible to others). Mjolnir given to the main hand, CheckPowerups (which picks the model each frame) and the pain
// frames run: his model index is player.mdl's, not 0. Verdict "mjolnirmodel: N passed, M failed". Single player, with or
// without Hipnotic's data (e1m1).
void() VR_MjolnirModelTest_Run =
{
    VR_Test_BeginFlags("mjolnirmodel", VR_TEST_LOUD);
    entity p = self;
    VRGiveEntWeapon(p, cVR_MainHand, WID_MJOLNIR, 0, 0);
    p.weapon = WID_MJOLNIR;
    CheckPowerups();
    VR_Test_Check(p.modelindex != 0 && p.modelindex == modelindex_player,
        sprintf("holding Mjolnir: model index %g (player.mdl's %g)", p.modelindex, modelindex_player));
    float hadHealth = p.health;
    p.pain_finished = 0;
    player_pain(world, 1);
    p.think();
    VR_Test_Check(p.modelindex == modelindex_player, sprintf("in pain holding Mjolnir: model index %g", p.modelindex));
    p.health = hadHealth;
    VRSetEntWeapon(p, cVR_MainHand, WID_FIST);
    p.weapon = WID_FIST;
    VR_Test_Verdict();
};

// vr_pickup_test 24 (vr_itemrules_test.qc; fbmisc1123, his note 2026-11-23: ad_crucial's lift at -640 -189 -296 "is
// raised and can't be brought down"): the map's own design. Its lift (func_plat "lift1", AD's ad_func_plat: with a
// targetname it starts raised and locked, ad_STATE_DISABLED) comes down once, when the super shotgun in the hall east of
// it (-320 80 -328, target lift1; three enforcers spawn too) is taken; then it is an ordinary lift. Neither a monster
// nor vr_nav's call moves a locked lift (AD's trigger answers clients only; VR_NAV_CallLift calls one at its bottom). Here: the lift raised and waiting, the map's gun taken
// through itemTouch by the main hand, the lift at its bottom (by the lift's own state, its travel 256 at speed 80:
// ~3.2 s; a give-up at 10 s). Verdict "crucial_lift: N passed, M failed". On ad_crucial only.
void() VR_CLTest_Wait =
{
    entity lift = self.enemy;
    if(lift.state != STATE_BOTTOM && time < self.ltime)
    {
        self.nextthink = time;
        return;
    }
    VR_Test_Check(lift.state == STATE_BOTTOM, sprintf("the lift came down: state %g (%g: its bottom), its top at z %g%s",
        lift.state, STATE_BOTTOM, lift.absmax_z, lift.state == STATE_BOTTOM ? "" : " (timed out)"));
    remove(self);
    VR_Test_Verdict();
};

void() VR_CrucialLiftTest_Run =
{
    VR_Test_BeginFlags("crucial_lift", VR_TEST_LOUD);
    entity p = self;
    entity lift = find(world, targetname, "lift1");
    while(lift && lift.classname != "func_plat")
    {
        lift = find(lift, targetname, "lift1");
    }
    entity gun = find(world, target, "lift1");
    while(gun && gun.classname != "weapon_supershotgun")
    {
        gun = find(gun, target, "lift1");
    }
    if(mapname != "ad_crucial" || !lift || !gun)
    {
        VR_Test_Check(FALSE, sprintf("ad_crucial's lift1 and its super shotgun (map %s)", mapname));
        VR_Test_Verdict();
        return;
    }
    // (AD's plat, ad_func_plats.qc: raised and locked, ad_STATE_DISABLED, until used; its trigger answers clients only)
    VR_Test_Check(lift.state == ad_STATE_DISABLED, sprintf("the lift waits raised and locked for its target (state %g, top z %g)",
        lift.state, lift.absmax_z));
    VRSetMainHandGrabbingBit(p, TRUE);
    VR_ITT_Touch(p, gun);
    VRSetMainHandGrabbingBit(p, FALSE);
    VR_Test_Check(gun.model == string_null && VR_WeaponUtil_EntHasWeapon(p, WID_SUPER_SHOTGUN),
        "the map's super shotgun taken");
    VR_Test_Check(lift.state == STATE_DOWN, sprintf("its pickup sent the lift down (state %g)", lift.state));
    entity w = spawn();
    w.classname = "vr_cltest_wait";
    w.enemy = lift;
    w.ltime = time + 10;
    w.think = VR_CLTest_Wait;
    w.nextthink = time;
};
#endif
