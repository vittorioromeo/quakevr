#ifdef QVR_TESTS
// vr_flatitems_test.qc -- pickups in flat-screen play (-flatscreenmode; docs/vr-port/FLATPLAY.md, "Pickups"), through the
// real paths: the map's own spawn functions, E (impulse 46), +attack, and +forward over the pickup's own place.
// `vr_flatitems_test 1`: each thing that can stay in the off hand, grabbed with E (it stays held: not better, or full up;
//   what the mode takes at once is noted), the main hand's shotgun fired through it: the shot goes, the hand keeps it.
//   Misc/quakevr/flatitems_test.sh fails the run on any "Assertion failure" in the log.
// `vr_flatitems_test 2`: walked over, each kind of pickup is taken (ammo, health, megahealth over 100, a monster's backpack
//   too), a box of what he is full of is left lying, and armour puts the plates on.
// `vr_flatitems_test 3`: E on armour that is better than what he wears: on him at once, the plates on, nothing held.
// `vr_flatitems_test 5`: as he played: armour in the left hand with E, 2 for the shotgun, `give n 40` on the console, the shot:
//   the one way a shot reaches VR_AmmoUtil_GetAmmoCountByAmmoId's assertion (FLATPLAY.md, "Pickups"): the engine's give
//   wrote a count where the game keeps the ammo id. The armour is only there as it was in his report.
// `vr_flatitems_test 4`: red armour ahead, to look at: walks over it and takes two pictures of him from the front (vr_flat_orbit),
//   before and after (`flatplates: before`, `flatplates: after`).
// `vr_flatitems_test 6`: E's palm first (vr_flatfgrab.cpp): two keys where he looks, E pressed twice a frame apart: the first
//   flies only after the palm's turn, never two at one hand, the first ends in the hand it flew to.
// `vr_flatitems_test 7`: a brick force-grabbed beside the shotgun: the gun one-handed, the brick in the off hand's guard,
//   V's overhead swing and the fist's punch with it each hurt a grunt; a wall torch caught turned flame up.
// `vr_flatitems_test 8`: E on a backpack: taken at once (vr_forcegrab_pack_collect); an empty backpack touched: taken without
//   a word; one with a negative amount: only what is more than none.
// `vr_flatitems_test 9`: the rocket launcher, then the axe, held in both hands: E, then the shove: the off hand lets
//   go of the grip and eases off it (E: before the brick flies), the axe as the gun (the release shared by guns and melee).
// Ends `flatitems: N passed, M failed`.

float vr_ffx_on;    // the running test's number
float vr_ffx_s;     // its state
float vr_ffx_case;  // which pickup
float vr_ffx_t0;    // when the state began (a stuck test: FAIL "timed out")
float vr_ffx_wait;  // frames left before the state goes on (the word to the engine, the console's +attack)
float vr_ffx_ammo;  // shells before the shot, the main hand's clip with them (flat reload: a shot takes its clip's)
vector vr_ffx_from; // where the walk starts
entity vr_ffx_item; // the pickup of this case

float VR_FFX_GIVEUP = 8; // seconds a state may last
float VR_FFX_HOLD_CASES = 6;
float VR_FFX_WALK_CASES = 20;

void(float s, float frames) VR_FFX_To =
{
    vr_ffx_s = s;
    vr_ffx_wait = frames;
    vr_ffx_t0 = time;
};

void(entity xItem) VR_FFX_Forget =
{
    if(xItem != world)
    {
        remove(xItem);
    }
    vr_ffx_item = world;
};

// The map's own spawn function of a kind, `xAt` its place.
entity(float xKind, vector xAt) VR_FFX_SpawnKind =
{
    entity oldself = self;
    entity it = spawn();
    setorigin(it, xAt);
    self = it;
    if(xKind == 0)       { it.classname = ITEM_NAME_ARMOR1;   item_armor1(); }
    else if(xKind == 1)  { it.classname = ITEM_NAME_ARMOR2;   item_armor2(); }
    else if(xKind == 2)  { it.classname = ITEM_NAME_ARMORINV; item_armorInv(); }
    else if(xKind == 3)  { it.classname = "item_key1";   item_key1(); }
    else if(xKind == 4)  { it.classname = "item_key2";   item_key2(); }
    else if(xKind == 5)  { it.classname = "item_artifact_envirosuit"; item_artifact_envirosuit(); }
    else if(xKind == 6)  { it.classname = "item_sigil"; it.spawnflags = 1; item_sigil(); }
    else if(xKind == 7)  { it.classname = "item_shells"; item_shells(); }
    else if(xKind == 8)  { it.classname = "item_health"; item_health(); }
    else if(xKind == 9)  { it.classname = "item_artifact_super_damage"; item_artifact_super_damage(); }
    else if(xKind == 10) { it.classname = "item_artifact_invulnerability"; item_artifact_invulnerability(); }
    else if(xKind == 11) { it.classname = "item_artifact_invisibility"; item_artifact_invisibility(); }
    else if(xKind == 12) { it.classname = "weapon_supershotgun"; weapon_supershotgun(); }
    else if(xKind == 13) { it.classname = "item_rockets"; item_rockets(); }
    else if(xKind == 14) { it.classname = "item_cells"; item_cells(); }
    else if(xKind == 15) { it.classname = "item_spikes"; item_spikes(); }
    else if(xKind == 16) { it.classname = "item_shells"; item_shells(); }  // (his shells full: it stays)
    else if(xKind == 17) { it.classname = "item_health"; item_health(); }  // (his health full: it stays)
    else if(xKind == 18) { it.classname = "item_health"; it.spawnflags = H_MEGA; item_health(); } // (over 100)
    else // a backpack, as a monster drops it (DropBackpack: the armed grunts' too), put on his way at rest
    {
        entity d = spawn();
        setorigin(d, xAt + '0 0 24');
        d.ammo_shells = 20;
        self = d;
        DropBackpack();
        self = oldself;
        remove(d);
        remove(it);
        for(entity e = nextent(world); e; e = nextent(e))
        {
            if(e.carry_use == BackpackTouch && e.ammo_shells == 20 && vlen(e.origin - xAt) < 64)
            {
                e.velocity = '0 0 0';
                setorigin(e, xAt);
                return e;
            }
        }
        return world;
    }
    self = oldself;
    return it;
};

string(float xKind) VR_FFX_KindName =
{
    if(xKind == 0)  { return "green armour"; }
    if(xKind == 1)  { return "yellow armour"; }
    if(xKind == 2)  { return "red armour"; }
    if(xKind == 3)  { return "silver key"; }
    if(xKind == 4)  { return "gold key"; }
    if(xKind == 5)  { return "biosuit"; }
    if(xKind == 6)  { return "rune"; }
    if(xKind == 7)  { return "shells"; }
    if(xKind == 8)  { return "health"; }
    if(xKind == 9)  { return "quad"; }
    if(xKind == 10) { return "pentagram"; }
    if(xKind == 11) { return "ring"; }
    if(xKind == 12) { return "super shotgun"; }
    if(xKind == 13) { return "rockets"; }
    if(xKind == 14) { return "cells"; }
    if(xKind == 15) { return "nails"; }
    if(xKind == 16) { return "shells, his full"; }
    if(xKind == 17) { return "health, his full"; }
    if(xKind == 18) { return "megahealth"; }
    return "backpack";
};

// Placed: PlaceItem has run and (an object's) VR_PickupObj_Place after it.
float(entity xItem) VR_FFX_Placed =
{
    if(xItem.think == PlaceItem) { return FALSE; }
    if(xItem.carry_use == BackpackTouch) { return TRUE; } // (dropped, not placed: nothing to wait for)
    if(xItem.vr_pickup_obj) { return !!(xItem.flags & FL_FORCEGRABBABLE); } // (VR_PickupObj_Place sets it, its last word)
    return xItem.nextthink2 <= time; // (the plain box's AfterPlacement, if it has one)
};

// What stays in the hand. Set up so it does: a worse armour under red, a key he has not got, full up of shells and health.
void(entity p) VR_FFX_MakeFull =
{
    p.armortype = 0.8;
    p.armorvalue = 200;
    VR_ItemUtil_EntAddItem(p, IID_ARMOR3);
    p.health = 100;
    p.ammo_shells = 100;
    p.items = p.items - (p.items & (IT_KEY1 | IT_KEY2));
};

// Puts it where E's grab looks: where the crosshair points, a short way out (VR_Forcegrab_FlatGrab searches from his eye along
// his view; a thing a player walks up to and presses E on).
void(entity p, entity xItem) VR_FFX_LookAt =
{
    makeforward(p.v_angle);
    vector at = VRGetHeadPos(p) + v_forward * 36 - (xItem.mins + xItem.maxs) * 0.5;
    setorigin(xItem, at);
    xItem.velocity = '0 0 0';
};

vector(entity p) VR_FFX_Ahead =
{
    makevectors('0 1 0' * p.v_angle_y);
    return v_forward;
};

// The shells he has, his main hand's clip with them: a shot takes one from the clip (the starting shotgun comes loaded,
// flat-screen play too; before 2026-11-10 it was empty and the shot was the empty clip's reload from the shells).
float(entity p) VR_FFX_Rounds =
{
    return p.ammo_shells + VRGetEntWeaponClip(p, cVR_MainHand);
};

// ---- 1: held in the off hand, the shotgun fired.
void(entity p) VR_FFX_Hold =
{
    if(vr_ffx_s == 0) // the shotgun in the main hand, the off hand free
    {
        if(vr_ffx_case >= (vr_ffx_on == 5 ? 1 : VR_FFX_HOLD_CASES))
        {
            localcmd("-attack\n");
            VR_Test_Verdict();
            vr_ffx_on = 0;
            cvar_set("vr_flatitems_test", "0");
            return;
        }
        if(vr_ffx_on != 5) { localcmd("impulse 154\n"); }
        VR_FFX_To(1, 10);
        return;
    }
    if(vr_ffx_s == 1)
    {
        if(vr_ffx_on != 5) { VR_FFX_MakeFull(p); }
        float kind = vr_ffx_case == 0 ? 0 : vr_ffx_case == 1 ? 3 : vr_ffx_case == 2 ? 5 : vr_ffx_case == 3 ? 6 : vr_ffx_case == 4 ? 7 : 8;
        vr_ffx_item = VR_FFX_SpawnKind(kind, p.origin + VR_FFX_Ahead(p) * 96);
        VR_FFX_To(2, 0);
        return;
    }
    if(vr_ffx_s == 2) // placed: look at it
    {
        if(VR_FFX_Placed(vr_ffx_item))
        {
            VR_FFX_LookAt(p, vr_ffx_item);
            VR_FFX_To(3, 2); // (two frames: the hands' frame has searched along the view since: the preview)
        }
        return;
    }
    if(vr_ffx_s == 3) // E
    {
        // The preview (vr_flat_grab_preview): the outline on what E takes (its own search: the suit, too near the eye's
        // line, is found by neither), on the hand E takes it with (the main if free); none on the other.
        float grabMain = VR_Hand_Free(p, cVR_MainHand, VR_HANDFREE_LOCKED);
        entity onGrab = grabMain ? p.mainhand_fgtarget : p.offhand_fgtarget;
        entity onOther = grabMain ? p.offhand_fgtarget : p.mainhand_fgtarget;
        makeforward(p.v_angle);
        entity oldSelf = self;
        self = p; // (the search's traces pass its self)
        entity eTakes = VR_Forcegrab_FindTargetAlong(VRGetHeadPos(p), v_forward);
        self = oldSelf;
        VR_Test_Check(onGrab == eTakes && !p.offhand_fglocked && !p.mainhand_fglocked && onOther == world,
            sprintf("the force grab preview is on what E takes (%s), the %s hand's, placed %s (off %s, main %s)",
            eTakes.classname, grabMain ? "main" : "off", vr_ffx_item.classname, p.offhand_fgtarget.classname,
            p.mainhand_fgtarget.classname));
        dprint(sprintf("flatitems: hands %g %g, off hand free %g, grabbing %s\n", VRGetEntWeapon(p, cVR_MainHand), VRGetEntWeapon(p, cVR_OffHand),
            VR_Hand_Free(p, cVR_OffHand, VR_HANDFREE_LOCKED), vr_ffx_item.classname));
        p.impulse = 46;
        VR_FFX_To(4, 0);
        return;
    }
    if(vr_ffx_s == 4) // it arrives: in the hand, or taken at once
    {
        if(vr_ffx_item.model == string_null || VR_Carry_Held(p, cVR_OffHand) == vr_ffx_item)
        {
            dprint(sprintf("flatitems: %s %s\n", vr_ffx_item.classname, vr_ffx_item.model == string_null ? "taken at once" : "in the off hand"));
            vr_ffx_ammo = VR_FFX_Rounds(p);
            if(vr_ffx_on == 5)
            {
                VR_FFX_To(10, 0); // (the shotgun after it, as he would: 2 cycles the right hand)
                return;
            }
            localcmd("+attack\n");
            VR_FFX_To(5, 0);
        }
        return;
    }
    if(vr_ffx_s == 11)
    {
        VR_Test_Check(p.currentammo == AID_SHELLS, sprintf("give n 40: the main hand's ammo id stays shells (%g)", p.currentammo));
        vr_ffx_ammo = VR_FFX_Rounds(p);
        localcmd("+attack\n");
        VR_FFX_To(5, 0);
        return;
    }
    if(vr_ffx_s == 10)
    {
        if(WeaponIdToAmmoId(VRGetEntWeapon(p, cVR_MainHand), VRGetEntWeaponFlags(p, cVR_MainHand)) == AID_SHELLS)
        {
            dprint(sprintf("flatitems: 2 gave the main hand weapon %g, off %g, ammo ids %g %g\n", VRGetEntWeapon(p, cVR_MainHand), VRGetEntWeapon(p, cVR_OffHand), p.currentammo, p.currentammo2));
            localcmd("give n 40\n"); // (the console's own: Host_Give_f, what the Debug menu's weapon rows use too)
            VR_FFX_To(11, 4);
            return;
        }
        cvar_set("vr_flat_holsters", "0"); // (2's cycle, as he played: the holsters off, vr_flatholster.qc)
        p.impulse = 2;
        vr_ffx_wait = 4;
        return;
    }
    if(vr_ffx_s == 5) // the shot
    {
        if(VR_FFX_Rounds(p) < vr_ffx_ammo)
        {
            localcmd("-attack\n");
            VR_Test_Check(TRUE, sprintf("%s: the shot goes with it %s (shells and clip %g to %g)", vr_ffx_item.classname,
                vr_ffx_item.model == string_null ? "taken" : "held", vr_ffx_ammo, VR_FFX_Rounds(p)));
            VR_FFX_To(6, 4);
        }
        return;
    }
    if(vr_ffx_s == 6) // RMB too: the off hand's own trigger on what it holds
    {
        localcmd("+offhandattack\n");
        VR_FFX_To(7, 6);
        return;
    }
    if(vr_ffx_s == 7)
    {
        localcmd("-offhandattack\n");
        if(vr_ffx_item.model != string_null)
        {
            VR_Test_Check(VR_Carry_Held(p, cVR_OffHand) == vr_ffx_item, sprintf("%s: the hand keeps it after the shots", vr_ffx_item.classname));
            VR_Hand_Release(p, cVR_OffHand, "test");
            VR_Carry_Loose(vr_ffx_item);
        }
        VR_FFX_Forget(vr_ffx_item);
        vr_ffx_case = vr_ffx_case + 1;
        VR_FFX_To(0, 2);
        return;
    }
};

// What the walk should give: how much of it he has afterwards is the check.
float(entity p, float xKind) VR_FFX_WalkGot =
{
    if(xKind <= 2) { return p.armorvalue > 0; }
    if(xKind == 3) { return !!(p.items & IT_KEY1); }
    if(xKind == 4) { return !!(p.items & IT_KEY2); }
    if(xKind == 5) { return !!(p.items & IT_SUIT); }
    if(xKind == 6) { return !!(serverflags & 1); }
    if(xKind == 7) { return p.ammo_shells > 10; }
    if(xKind == 8) { return p.health > 50; }
    if(xKind == 9) { return !!(p.items & IT_QUAD); }
    if(xKind == 10) { return !!(p.items & IT_INVULNERABILITY); }
    if(xKind == 11) { return !!(p.items & IT_INVISIBILITY); }
    if(xKind == 12) { return !!(p.items & IT_SUPER_SHOTGUN); }
    if(xKind == 13) { return p.ammo_rockets > 0; }
    if(xKind == 14) { return p.ammo_cells > 0; }
    if(xKind == 15) { return p.ammo_nails > 0; }
    if(xKind == 16) { return p.ammo_shells > 100; }
    if(xKind == 17 || xKind == 18) { return p.health > 100; }
    return p.ammo_shells > 10;
};

// Nothing he could already be given by the walk.
void(entity p) VR_FFX_MakeBare =
{
    p.armortype = 0;
    p.armorvalue = 0;
    VR_ItemUtil_EntDelItem(p, IID_ARMOR1);
    VR_ItemUtil_EntDelItem(p, IID_ARMOR2);
    VR_ItemUtil_EntDelItem(p, IID_ARMOR3);
    p.items = p.items - (p.items & (IT_KEY1 | IT_KEY2 | IT_SUIT | IT_QUAD | IT_INVULNERABILITY | IT_INVISIBILITY | IT_SUPER_SHOTGUN));
    p.health = 50;
    p.ammo_shells = 10;
    p.ammo_rockets = 0;
    p.ammo_cells = 0;
    p.ammo_nails = 0;
    serverflags = serverflags - (serverflags & 1);
};

// ---- 2: walked over.
void(entity p) VR_FFX_Walk =
{
    if(vr_ffx_s == 0) // the next: bare, at the start of the same stretch, the pickup ahead
    {
        if(vr_ffx_case >= VR_FFX_WALK_CASES)
        {
            VR_Test_Verdict();
            vr_ffx_on = 0;
            cvar_set("vr_flatitems_test", "0");
            return;
        }
        if(vr_ffx_case == 0)
        {
            vr_ffx_from = p.origin;
        }
        VR_FFX_MakeBare(p);
        if(vr_ffx_case == 16) { p.ammo_shells = 100; }
        if(vr_ffx_case == 17 || vr_ffx_case == 18) { p.health = 100; }
        localcmd(sprintf("setpos %g %g %g 0 %g 0\n", vr_ffx_from_x, vr_ffx_from_y, vr_ffx_from_z, p.v_angle_y));
        VR_FFX_To(1, 4);
        return;
    }
    if(vr_ffx_s == 1)
    {
        vr_ffx_item = VR_FFX_SpawnKind(vr_ffx_case, vr_ffx_from + VR_FFX_Ahead(p) * 120);
        VR_FFX_To(2, 0);
        return;
    }
    if(vr_ffx_s == 2)
    {
        if(VR_FFX_Placed(vr_ffx_item))
        {
            localcmd("+forward\n");
            VR_FFX_To(3, 0);
        }
        return;
    }
    if(vr_ffx_s == 3) // on until it is gone, or he has walked past it
    {
        vector d = p.origin - vr_ffx_from;
        d_z = 0;
        float past = vlen(d) > 200;
        float gone = vr_ffx_item.model == string_null;
        if(gone || past)
        {
            localcmd("-forward\n");
            float got = VR_FFX_WalkGot(p, vr_ffx_case);
            // (Full up of it, it stays: the box's own rules, as a headset hand's; his words 2026-11-05: "walk over all".)
            if(vr_ffx_case == 16 || vr_ffx_case == 17)
            {
                VR_Test_Check(!gone && !got, sprintf("walked over the %s: left lying (taken %g)", VR_FFX_KindName(vr_ffx_case), gone));
            }
            else
            {
                VR_Test_Check(gone && got, sprintf("walked over the %s: taken %g, his now %g", VR_FFX_KindName(vr_ffx_case), gone, got));
            }
            if(vr_ffx_case <= 2)
            {
                VR_Test_Check(p.vr_pp_mask != 0, sprintf("... the plates respond (mask %g, armour %g, type %g)", p.vr_pp_mask, p.armorvalue, p.armortype));
            }
            VR_FFX_Forget(gone ? world : vr_ffx_item);
            vr_ffx_case = vr_ffx_case + 1;
            VR_FFX_To(0, 2);
        }
        return;
    }
};

// ---- 3: E on armour better than his: on him at once.
void(entity p) VR_FFX_Better =
{
    if(vr_ffx_s == 0)
    {
        VR_FFX_MakeBare(p);
        vr_ffx_item = VR_FFX_SpawnKind(vr_ffx_case, p.origin + VR_FFX_Ahead(p) * 96);
        VR_FFX_To(1, 0);
        return;
    }
    if(vr_ffx_s == 1)
    {
        if(VR_FFX_Placed(vr_ffx_item))
        {
            VR_FFX_LookAt(p, vr_ffx_item);
            VR_FFX_To(2, 0);
        }
        return;
    }
    if(vr_ffx_s == 2)
    {
        p.impulse = 46;
        VR_FFX_To(3, 0);
        return;
    }
    if(vr_ffx_s == 3)
    {
        if(vr_ffx_item.model == string_null || VR_Carry_Held(p, cVR_OffHand) == vr_ffx_item || VR_Carry_Held(p, cVR_MainHand) == vr_ffx_item)
        {
            VR_FFX_To(4, 2);
        }
        return;
    }
    if(vr_ffx_s == 4)
    {
        VR_Test_Check(vr_ffx_item.model == string_null && p.armorvalue > 0, sprintf("E on the %s: worn at once (armour %g)", VR_FFX_KindName(vr_ffx_case), p.armorvalue));
        VR_Test_Check(VR_Carry_Held(p, cVR_OffHand) == world && VR_Carry_Held(p, cVR_MainHand) == world, "... and nothing is left in a hand");
        VR_Test_Check(p.vr_pp_mask != 0, sprintf("... the plates respond (mask %g)", p.vr_pp_mask));
        VR_Test_Verdict();
        vr_ffx_on = 0;
        cvar_set("vr_flatitems_test", "0");
        return;
    }
};

// ---- 6: E's palm first (vr_flatfgrab.cpp; his note 2026-11-10): two keys where he looks, E pressed through the engine's
// own path (vr_flat_fg_press) and again a frame later. The first key flies no sooner than the palm's turn
// (vr_flat_fg_turn_time); no two objects ever fly at one hand at once; the first one ends in the hand it flew to.
float vr_ffx_pressAt;   // when E was pressed (the first time)
float vr_ffx_flewAt;    // when the first key began to fly
float vr_ffx_flewHand;  // the hand it flew to
float vr_ffx_doubled;   // frames two objects flew at one hand
entity vr_ffx_item2;    // the second key

float(entity p, entity xKey) VR_FFX_InHand =
{
    return xKey.model == string_null || VR_Carry_Held(p, cVR_OffHand) == xKey || VR_Carry_Held(p, cVR_MainHand) == xKey;
};

void(entity p) VR_FFX_PalmFirst =
{
    if(vr_ffx_s == 0)
    {
        VR_FFX_MakeFull(p);
        vr_ffx_item = VR_FFX_SpawnKind(3, p.origin + VR_FFX_Ahead(p) * 96);
        vr_ffx_item2 = VR_FFX_SpawnKind(4, p.origin + VR_FFX_Ahead(p) * 96 + '0 0 16');
        vr_ffx_flewAt = 0;
        vr_ffx_doubled = 0;
        VR_FFX_To(1, 0);
        return;
    }
    if(vr_ffx_s == 1)
    {
        if(VR_FFX_Placed(vr_ffx_item) && VR_FFX_Placed(vr_ffx_item2))
        {
            VR_FFX_LookAt(p, vr_ffx_item);
            VR_FFX_LookAt(p, vr_ffx_item2);
            setorigin(vr_ffx_item2, vr_ffx_item2.origin + '0 0 8');
            VR_FFX_To(2, 0);
        }
        return;
    }
    if(vr_ffx_s == 2)
    {
        localcmd("vr_flat_fg_press\n");
        vr_ffx_pressAt = time;
        VR_FFX_To(3, 1);
        return;
    }
    // Watching: the second press once, the first flight's time and hand, two at one hand never.
    if(vr_ffx_s == 3 || vr_ffx_s == 4)
    {
        if(vr_ffx_s == 3)
        {
            localcmd("vr_flat_fg_press\n");
            vr_ffx_s = 4;
        }
        float a = vr_ffx_item.fg_state == 1 && vr_ffx_item.fg_player == p;
        float b = vr_ffx_item2.fg_state == 1 && vr_ffx_item2.fg_player == p;
        if(a && b && vr_ffx_item.fg_hand == vr_ffx_item2.fg_hand)
        {
            vr_ffx_doubled = vr_ffx_doubled + 1;
        }
        if(!vr_ffx_flewAt && (a || b))
        {
            vr_ffx_flewAt = time;
            vr_ffx_flewHand = a ? vr_ffx_item.fg_hand : vr_ffx_item2.fg_hand;
            if(!a)
            {
                entity swap = vr_ffx_item;
                vr_ffx_item = vr_ffx_item2;
                vr_ffx_item2 = swap;
            }
        }
        if(vr_ffx_flewAt && !a && !b && vr_ffx_item.fg_state != 1)
        {
            VR_FFX_To(5, 36); // (half a second more: a second grab's flight, if one went, ends and is watched no more)
        }
        return;
    }
    if(vr_ffx_s == 5)
    {
        float turn = cvar_hget(cvarh_vr_flat_fg_turn_time);
        float took = vr_ffx_flewAt - vr_ffx_pressAt;
        VR_Test_Check(vr_ffx_flewAt > 0 && took >= turn - 0.02,
            sprintf("E: the palm turns out first: the key flew %g s after the press (turn %g s)", took, turn));
        VR_Test_Check(took <= turn + cvar_hget(cvarh_vr_flat_fg_detach_time) + 0.15, sprintf("... and not long after it (%g s)", took));
        VR_Test_Check(vr_ffx_doubled == 0, sprintf("two quick presses: never two objects flying at one hand (%g frames)", vr_ffx_doubled));
        VR_Test_Check(VR_FFX_InHand(p, vr_ffx_item) && (vr_ffx_item.model == string_null || VR_Carry_Held(p, vr_ffx_flewHand) == vr_ffx_item),
            sprintf("the first key ends in the hand it flew to (%g)", vr_ffx_flewHand));
        VR_Test_Verdict();
        vr_ffx_on = 0;
        cvar_set("vr_flatitems_test", "0");
        return;
    }
};

// ---- 4: to look at: pictures looking down, before and after the walk.
void(entity p) VR_FFX_Pictures =
{
    if(vr_ffx_s == 0)
    {
        VR_FFX_MakeBare(p);
        vr_ffx_from = p.origin;
        vr_ffx_item = VR_FFX_SpawnKind(2, p.origin + VR_FFX_Ahead(p) * 110);
        localcmd(sprintf("setpos %g %g %g 0 %g 0\nvr_flat_orbit 180 70\n", p.origin_x, p.origin_y, p.origin_z, p.v_angle_y)); // (from the front)
        VR_FFX_To(1, 0);
        return;
    }
    if(vr_ffx_s == 1)
    {
        if(VR_FFX_Placed(vr_ffx_item))
        {
            VR_FFX_To(2, 12);
        }
        return;
    }
    if(vr_ffx_s == 2)
    {
        dprint(sprintf("flatplates: before, mask %g armour %g\n", p.vr_pp_mask, p.armorvalue));
        localcmd("screenshot\n");
        VR_FFX_To(3, 8);
        return;
    }
    if(vr_ffx_s == 3)
    {
        localcmd("+forward\n");
        VR_FFX_To(4, 0);
        return;
    }
    if(vr_ffx_s == 4)
    {
        if(vr_ffx_item.model == string_null || vlen(p.origin - vr_ffx_from) > 200)
        {
            localcmd("-forward\n");
            VR_Test_Check(p.armorvalue > 0 && p.vr_pp_mask != 0, sprintf("walked over red armour: on him (armour %g, mask %g)", p.armorvalue, p.vr_pp_mask));
            VR_FFX_To(5, 150); // the morph's time, as a player sees the plates grow (counted: VR_PP's own duration is under 2 s)
        }
        return;
    }
    if(vr_ffx_s == 5)
    {
        dprint(sprintf("flatplates: after, mask %g armour %g\n", p.vr_pp_mask, p.armorvalue));
        localcmd("screenshot\n");
        VR_FFX_To(6, 8);
        return;
    }
    if(vr_ffx_s == 6)
    {
        VR_Test_Verdict();
        vr_ffx_on = 0;
        cvar_set("vr_flatitems_test", "0");
        return;
    }
};

// ---- 7: a prop force-grabbed beside a two-handed gun (his notes 2026-11-10, vr_flatfgrab.cpp, vr_flatplay.cpp): the
// shotgun in the main hand, its fore-end in the off hand. E on a brick: the off hand lets go of the fore-end for good
// (no two-handed aim, the hand out at its side, in the guard: `vr_flat_fg_report` says "fist prop 1", the script reads
// it), V strikes with the brick (vr_flat_prop_v): the overhead swing over the head (vr_flat_prop_melee 1), then the
// fist's punch below it (0), each hurting a grunt in front. H drops it; E on a wall torch lying on its side: with
// vr_flat_torch_upright 0 it is held as the grip took it, with 1 it is turned flame up.
float vr_ffx_gruntHp;  // the grunt's health before the blow
float vr_ffx_top;      // the highest the off hand went over the eye in the blow
float vr_ffx_frames;   // frames counted in a state
float vr_ffx_twoHand;  // frames the gun was aimed two-handed with the brick in the off hand
float vr_ffx_d0;       // the torch off upright with vr_flat_torch_upright 0
vector vr_ffx_rel;     // the off hand off the main hand, on the fore-end
float vr_ffx_swingTop; // the overhead swing's vr_ffx_top
float vr_ffx_near;     // the nearest the off hand came to the grunt's box in the blow
entity vr_ffx_grunt;

// How far the stick of torch `t` is off upright, degrees (its +x its head; an alias model's pitch is inverted).
float(entity t) VR_FFX_TorchOff =
{
    vector a = t.angles;
    a_x = -a_x;
    makevectors(a);
    vector up = vectoangles(v_forward); // (its pitch: 90 straight up)
    return 90 - up_x;
};

void(entity p, float xMelee) VR_FFX_Blow =
{
    // A fresh grunt for each blow, standing still (one already hit turns on him and moves: the next blow's a lottery).
    // The punch's nearer: it reaches less than the swing.
    if(vr_ffx_grunt != world)
    {
        remove(vr_ffx_grunt);
    }
    entity oldself = self;
    vr_ffx_grunt = spawn();
    makevectors('0 1 0' * p.v_angle_y);
    setorigin(vr_ffx_grunt, p.origin + v_forward * (xMelee ? 36 : 28));
    vr_ffx_grunt.angles_y = p.v_angle_y + 180;
    self = vr_ffx_grunt;
    monster_army();
    self = oldself;
    vr_ffx_grunt.health = 1000;
    vr_ffx_gruntHp = vr_ffx_grunt.health;
    vr_ffx_top = -1000;
    vr_ffx_near = 1000;
    vr_ffx_frames = 0;
    localcmd(sprintf("vr_flat_prop_melee %g\nvr_flat_punch v\n", xMelee));
};

void(entity p) VR_FFX_PropFist =
{
    entity held = VR_Carry_Held(p, cVR_OffHand);
    if(vr_ffx_s == 0) // the shotgun in the main hand, the off hand free, the view level
    {
        localcmd("impulse 154\n");
        localcmd(sprintf("setpos %g %g %g 0 %g 0\n", p.origin_x, p.origin_y, p.origin_z, p.v_angle_y));
        vr_ffx_grunt = world;
        VR_FFX_To(1, 72); // (a second: the off hand onto the fore-end, eased)
        return;
    }
    if(vr_ffx_s == 1)
    {
        // (Whether the off hand was on the fore-end: `vr_flat_fg_report`'s "from the grip", the script reads it.)
        vr_ffx_rel = VRGetEntHandPos(p, cVR_OffHand) - VRGetEntHandPos(p, cVR_MainHand);
        entity oldself = self;
        vr_ffx_item = spawn();
        setorigin(vr_ffx_item, p.origin + VR_FFX_Ahead(p) * 64);
        vr_ffx_item.model = "progs/vr_brick1.mdl";
        self = vr_ffx_item;
        vr_debris_piece();
        self = oldself;
        VR_FFX_LookAt(p, vr_ffx_item);
        VR_FFX_To(2, 2);
        return;
    }
    if(vr_ffx_s == 2)
    {
        localcmd("vr_flat_fg_press\n");
        VR_FFX_To(3, 0);
        return;
    }
    if(vr_ffx_s == 3) // caught
    {
        if(held == vr_ffx_item)
        {
            vr_ffx_twoHand = 0;
            vr_ffx_frames = 0;
            VR_FFX_To(4, 0);
        }
        return;
    }
    if(vr_ffx_s == 4) // a second in the off hand: never two-handed; then where it ended
    {
        vr_ffx_twoHand = vr_ffx_twoHand + (VRGet2HAiming(p) ? 1 : 0);
        vr_ffx_frames = vr_ffx_frames + 1;
        if(vr_ffx_frames >= 72)
        {
            float d1 = vlen(VRGetEntHandPos(p, cVR_OffHand) - VRGetEntHandPos(p, cVR_MainHand) - vr_ffx_rel);
            VR_Test_Check(vr_ffx_twoHand == 0, sprintf("the brick in the off hand: the shotgun one-handed (%g frames two-handed)", vr_ffx_twoHand));
            VR_Test_Check(d1 > 8, sprintf("... the off hand off the fore-end: %g units from where it held it", d1));
            localcmd("vr_flat_fg_report\nscreenshot\n");
            VR_FFX_To(5, 4);
        }
        return;
    }
    if(vr_ffx_s == 5 || vr_ffx_s == 7) // V: the overhead swing (5), the fist's punch (7)
    {
        VR_FFX_Blow(p, vr_ffx_s == 5);
        VR_FFX_To(vr_ffx_s + 1, 0);
        return;
    }
    if(vr_ffx_s == 6 || vr_ffx_s == 8) // the blow: watched until it hurts the grunt, then until the hand is back
    {
        vr_ffx_frames = vr_ffx_frames + 1;
        if(vr_ffx_s == 6 && vr_ffx_frames == 22)
        {
            localcmd("screenshot\n"); // (the swing at its top, to look at)
        }
        vector hp = VRGetEntHandPos(p, cVR_OffHand);
        vector ep = VRGetHeadPos(p);
        vr_ffx_top = max(vr_ffx_top, hp_z - ep_z);
        vector gap = '0 0 0';
        gap_x = max(0, max(vr_ffx_grunt.absmin_x - hp_x, hp_x - vr_ffx_grunt.absmax_x));
        gap_y = max(0, max(vr_ffx_grunt.absmin_y - hp_y, hp_y - vr_ffx_grunt.absmax_y));
        gap_z = max(0, max(vr_ffx_grunt.absmin_z - hp_z, hp_z - vr_ffx_grunt.absmax_z));
        vr_ffx_near = min(vr_ffx_near, vlen(gap));
        float hurt = vr_ffx_grunt.health < vr_ffx_gruntHp;
        if(vr_ffx_frames >= 72) // (a second: the swing is 0.79 s, the punch 0.6, with their ways back)
        {
            string how = vr_ffx_s == 6 ? "the overhead swing" : "the fist's punch";
            dprint(sprintf("flatitems: %s: the off hand came %g units from the grunt's box\n", how, vr_ffx_near));
            VR_Test_Check(hurt, sprintf("V with the brick in the off hand, %s: the grunt hurt (%g)", how, vr_ffx_gruntHp - vr_ffx_grunt.health));
            if(vr_ffx_s == 6)
            {
                VR_Test_Check(vr_ffx_top > 0, sprintf("... the hand up over the eye (%g units over it)", vr_ffx_top));
                vr_ffx_swingTop = vr_ffx_top;
            }
            else
            {
                VR_Test_Check(vr_ffx_top < 0, sprintf("... thrown straight, the hand under the eye all the way (%g units over it; the swing %g)", vr_ffx_top, vr_ffx_swingTop));
            }
            VR_Test_Check(VR_Carry_Held(p, cVR_OffHand) == vr_ffx_item, "... the brick still in the off hand");
            VR_FFX_To(vr_ffx_s + 1, 36); // (the hand settled in the guard again)
        }
        return;
    }
    if(vr_ffx_s == 9) // H drops it; a wall torch on its side where he looks
    {
        localcmd("impulse 47\nvr_flat_torch_upright 0\n");
        remove(vr_ffx_grunt);
        vr_ffx_grunt = world;
        // (Made live, as the toolgun makes one: vr_spawn_live.)
        entity oldself = self;
        vr_ffx_item2 = spawn();
        setorigin(vr_ffx_item2, p.origin + VR_FFX_Ahead(p) * 64 + '0 0 40');
        vr_ffx_item2.classname = "light_torch_small_walltorch"; // (as the map's, the toolgun's: what makes it a torch, VR_WallTorch_IsTorch)
        self = vr_ffx_item2;
        float oldLive = vr_spawn_live;
        vr_spawn_live = 1;
        light_torch_small_walltorch();
        vr_spawn_live = oldLive;
        self = oldself;
        VR_Test_Check(vr_ffx_item2.model == "progs/vrtorch.mdl", sprintf("a wall torch made (%s)", vr_ffx_item2.model));
        if(vr_ffx_item2.model != "progs/vrtorch.mdl")
        {
            VR_FFX_To(20, 0);
            return;
        }
        VR_FFX_To(10, 10);
        return;
    }
    if(vr_ffx_s == 10)
    {
        VR_FFX_LookAt(p, vr_ffx_item2);
        vr_ffx_item2.angles = '0 90 0'; // (on its side)
        VR_FFX_To(11, 2);
        return;
    }
    if(vr_ffx_s == 11)
    {
        localcmd("vr_flat_fg_press\n");
        VR_FFX_To(12, 0);
        return;
    }
    if(vr_ffx_s == 12)
    {
        if(held == vr_ffx_item2 || vr_ffx_item2.carry_player == p)
        {
            VR_FFX_To(13, 72);
        }
        return;
    }
    if(vr_ffx_s == 13)
    {
        vr_ffx_d0 = VR_FFX_TorchOff(vr_ffx_item2);
        dprint(sprintf("flatitems: torch in the off hand, upright off: %g deg off upright\n", vr_ffx_d0));
        localcmd("vr_flat_fg_report\nscreenshot\nvr_flat_torch_upright 1\n");
        VR_FFX_To(14, 72);
        return;
    }
    if(vr_ffx_s == 14)
    {
        float off = VR_FFX_TorchOff(vr_ffx_item2);
        dprint(sprintf("flatitems: torch in the off hand, upright on: %g deg off upright\n", off));
        VR_Test_Check(off < 15, sprintf("the torch in the off hand stands flame up: %g deg off upright (%g with vr_flat_torch_upright 0)", off, vr_ffx_d0));
        localcmd("vr_flat_fg_report\nscreenshot\n");
        VR_FFX_To(20, 4);
        return;
    }
    if(vr_ffx_s == 20)
    {
        VR_Test_Verdict();
        vr_ffx_on = 0;
        cvar_set("vr_flatitems_test", "0");
        return;
    }
};

// ---- 8: backpacks (his notes 2026-11-10). A monster's backpack (20 shells) where he looks, E (impulse 46): the force grab
// brings it in and it is taken at once (vr_forcegrab_pack_collect), nothing left in a hand. Then an empty one touched:
// taken without a word (no bare "You get"), his ammo as it was; and one with a negative amount: only the positive taken.
float vr_ffx_shells0;
void(entity p) VR_FFX_Packs =
{
    if(vr_ffx_s == 0)
    {
        VR_FFX_MakeBare(p);
        vr_ffx_item = VR_FFX_SpawnKind(19, p.origin + VR_FFX_Ahead(p) * 96);
        VR_FFX_To(1, 0);
        return;
    }
    if(vr_ffx_s == 1)
    {
        VR_FFX_LookAt(p, vr_ffx_item);
        vr_ffx_shells0 = p.ammo_shells;
        p.impulse = 46;
        VR_FFX_To(2, 0);
        return;
    }
    if(vr_ffx_s == 2)
    {
        if(vr_ffx_item.model == string_null || VR_Carry_Held(p, cVR_OffHand) == vr_ffx_item || VR_Carry_Held(p, cVR_MainHand) == vr_ffx_item)
        {
            VR_FFX_To(3, 2);
        }
        return;
    }
    if(vr_ffx_s == 3)
    {
        VR_Test_Check(vr_ffx_item.model == string_null && p.ammo_shells == min(vr_ffx_shells0 + 20, VR_MaxAmmo(p, AID_SHELLS)),
            sprintf("E on a backpack: the force grab's catch takes it at once (shells %g to %g)", vr_ffx_shells0, p.ammo_shells));
        VR_Test_Check(VR_Carry_Held(p, cVR_OffHand) == world && VR_Carry_Held(p, cVR_MainHand) == world, "... and nothing is left in a hand");
        // An empty one, and one with a negative amount, touched as walking into it touches it.
        entity oself = self;
        for(float k = 0; k < 2; k++)
        {
            entity pk = spawn();
            setorigin(pk, p.origin + VR_FFX_Ahead(p) * 64);
            Backpack_Setup(pk, 0.75);
            if(k == 1) { pk.ammo_shells = -3; pk.ammo_nails = 5; }
            float sh = p.ammo_shells, na = p.ammo_nails;
            other = p;
            self = pk;
            BackpackTouch();
            self = oself;
            if(k == 0)
            {
                VR_Test_Check(vr_backpack_last_empty && pk.model == string_null && p.ammo_shells == sh && p.ammo_nails == na,
                    sprintf("an empty backpack: taken silently (silent %g, gone %g, shells %g to %g)", vr_backpack_last_empty,
                    pk.model == string_null, sh, p.ammo_shells));
            }
            else
            {
                VR_Test_Check(!vr_backpack_last_empty && p.ammo_shells == sh && p.ammo_nails == min(na + 5, VR_MaxAmmo(p, AID_NAILS)),
                    sprintf("a backpack of -3 shells and 5 nails: only the nails (shells %g to %g, nails %g to %g)", sh,
                    p.ammo_shells, na, p.ammo_nails));
            }
        }
        VR_Test_Verdict();
        vr_ffx_on = 0;
        cvar_set("vr_flatitems_test", "0");
        return;
    }
};

// ---- 9: the off hand let go of a weapon held in both hands (his note 2026-11-10: "when holding a melee weapon with two
// hands and force-grabbing, the off hand doesn't seem to detach first. It should animate like when holding a gun with two
// hands"). The rocket launcher (its fore-end) and the axe (its grip up the handle, which a swing's end gives the free hand),
// in the main hand, the off hand on its two-handed grip; then what lets go of it: E on a brick where he looks
// (vr_flat_fg_press), then the shove (vr_flat_shove_now) (the swap, H and G: flatfg2h_test.sh, by their keys). Case = action * 3 +
// weapon (0 the rocket launcher, 1 the axe, 2 the crowbar -- Two-Handed: Sword, his note 2026-11-11: the force grab ran with
// the off hand still on its grip -- the gun first, its numbers the others' reference). From the action each frame: when the
// grip lets go (weaponanygrip), the off hand's place off the main hand and its most per frame (no jump).
// Checks: the grip let go; its most a frame within VR_FFX_STEP_MOST, or the gun's own for that action and a unit; E: the
// off hand's grab, the grip let go before the brick flies, the hand well off the grip by then, the flight no sooner than the
// palm's turn and the grip's detach (vr_flat_fg_turn_time + vr_flat_fg_detach_time) and no sooner than a palm's turn after the
// grip let go (the pull waits for the hand off the grip), screenshots every 4 frames from the
// press (the release, as a strip: run.sh -Out). `flatfg2h:` lines with the numbers.
float VR_FFX_STEP_MOST = 4;  // units a frame the off hand may move as it leaves the grip (a snap off it is 10 and more)
float VR_FFX_2H_ACTIONS = 2; // E, the shove
float VR_FFX_2H_WEAPONS = 3; // the rocket launcher, the axe, the crowbar (2026-11-11: a sword-mode grip, vr_twohand's applySword)
float VR_FFX_2H_WINDOW = 36; // frames watched after the shove (its push and the hand's way back)
float vr_ffx_gripGone;       // when the grip let go after the action (0: not yet)
float vr_ffx_stepMost;       // the off hand's biggest move in a frame from the action on
float vr_ffx_offAtFlight;    // the off hand's distance from its place on the grip when the brick began to fly
float vr_ffx_shotsLeft;      // screenshots still to take in the strip
float vr_ffx_actAt;          // when the action was asked for
float vr_ffx_gunStep[2];     // the launcher's most a frame, per action
vector vr_ffx_relPrev;       // the off hand off the main hand, last frame

string(float xAction) VR_FFX_2HActionName =
{
    return xAction == 0 ? "E" : "shove";
};

string(float xWeapon) VR_FFX_2HWeaponName =
{
    return xWeapon == 2 ? "crowbar" : xWeapon ? "axe" : "launcher";
};

void(entity p) VR_FFX_TwoHandGrab =
{
    vector rel = VRGetEntHandPos(p, cVR_OffHand) - VRGetEntHandPos(p, cVR_MainHand);
    float weapon = vr_ffx_case - floor(vr_ffx_case / VR_FFX_2H_WEAPONS) * VR_FFX_2H_WEAPONS; // (0 the launcher, 1 the axe, 2 the crowbar)
    float action = floor(vr_ffx_case / VR_FFX_2H_WEAPONS);
    string wname = sprintf("%s, %s", VR_FFX_2HWeaponName(weapon), VR_FFX_2HActionName(action));
    if(vr_ffx_s == 0) // both hands emptied, the weapon in the main hand, the view level
    {
        VR_FFX_MakeBare(p);
        p.ammo_rockets = 10; // (the quick grenade's)
        localcmd("impulse 150\nimpulse 170\n");
        // (The rocket launcher: its grip ahead, FLATPLAY.md; the axe; the crowbar: Two-Handed Sword, vr_twohand's applySword.)
        localcmd(weapon == 2 ? "impulse 167\n" : weapon ? "impulse 152\n" : "impulse 160\n");
        localcmd(sprintf("setpos %g %g %g 0 %g 0\n", p.origin_x, p.origin_y, p.origin_z, p.v_angle_y));
        vr_ffx_frames = 0;
        if(weapon)
        {
            // (The axe's grip stands up the handle, where no pose carries the free hand: at rest it takes it only now and
            // then, at a swing's end. The test lets the pose carry it there for the take -- the hands' line held good any
            // way round, vr_2h_angle_threshold -1 -- and resets the threshold once it holds: the hold is then the one a
            // swing leaves, vr_twohand's own, kept with no pose on the grip.)
            localcmd("vr_2h_angle_threshold -1\n");
        }
        VR_FFX_To(1, 10);
        return;
    }
    if(vr_ffx_s == 1) // until the off hand holds its grip, then (the threshold back) a second to settle
    {
        if(weaponanygrip(p, cVR_OffHand) != 0)
        {
            if(weapon)
            {
                localcmd("reset vr_2h_angle_threshold\n");
            }
            VR_FFX_To(2, weapon ? 4 : 72);
        }
        return;
    }
    if(vr_ffx_s == 2) // the action
    {
        if(weaponanygrip(p, cVR_OffHand) == 0)
        {
            VR_Test_Check(FALSE, sprintf("%s: the off hand let go of the grip by itself before the action", wname));
            VR_FFX_To(6, 0);
            return;
        }
        VR_Test_Check(TRUE, sprintf("%s: the off hand holds the two-handed grip (kind %g)", wname, weaponanygrip(p, cVR_OffHand)));
        vr_ffx_rel = rel;
        vr_ffx_relPrev = rel;
        vr_ffx_gripGone = 0;
        vr_ffx_stepMost = 0;
        vr_ffx_frames = 0;
        vr_ffx_flewAt = 0;
        vr_ffx_shotsLeft = 0;
        vr_ffx_actAt = time;
        if(action == 0)
        {
            entity oldself = self;
            vr_ffx_item = spawn();
            setorigin(vr_ffx_item, p.origin + VR_FFX_Ahead(p) * 64);
            vr_ffx_item.model = "progs/vr_brick1.mdl";
            self = vr_ffx_item;
            vr_debris_piece();
            self = oldself;
            VR_FFX_LookAt(p, vr_ffx_item);
            localcmd("vr_flat_fg_press\nscreenshot\n");
            vr_ffx_shotsLeft = 8;
        }
        else { localcmd("vr_flat_shove_now\n"); }
        localcmd("vr_flat_fg_report\n"); // (its drawn hand's most a frame from here: the next report's)
        VR_FFX_To(3, 0);
        return;
    }
    if(vr_ffx_s == 3) // each frame: E until the brick flies, the others over the window
    {
        vr_ffx_frames = vr_ffx_frames + 1;
        vr_ffx_stepMost = max(vr_ffx_stepMost, vlen(rel - vr_ffx_relPrev));
        vr_ffx_relPrev = rel;
        if(!vr_ffx_gripGone && weaponanygrip(p, cVR_OffHand) == 0)
        {
            vr_ffx_gripGone = time;
        }
        if(vr_ffx_shotsLeft > 0 && vr_ffx_frames % 4 == 0)
        {
            localcmd("screenshot\n");
            vr_ffx_shotsLeft = vr_ffx_shotsLeft - 1;
        }
        if(action == 0 && vr_ffx_item.fg_state == 1 && vr_ffx_item.fg_player == p)
        {
            vr_ffx_flewAt = time;
            vr_ffx_flewHand = vr_ffx_item.fg_hand;
            vr_ffx_offAtFlight = vlen(rel - vr_ffx_rel);
            VR_FFX_To(4, 0);
        }
        if(action != 0 && vr_ffx_frames >= VR_FFX_2H_WINDOW)
        {
            VR_FFX_To(4, 0);
        }
        return;
    }
    if(vr_ffx_s == 4)
    {
        float gone = vr_ffx_gripGone ? vr_ffx_gripGone - vr_ffx_actAt : -1;
        bprint(sprintf("flatfg2h: %s: grip let go %g s after it, the off hand at most %g units a frame", wname, gone, vr_ffx_stepMost));
        if(action == 0)
        {
            bprint(sprintf(", the brick flew at %g s, the hand %g units off the grip then", vr_ffx_flewAt - vr_ffx_actAt, vr_ffx_offAtFlight));
        }
        bprint("\n");
        localcmd("vr_flat_fg_report\n"); // (the drawn off hand's most a frame since the action: the script reads it)
        if(!weapon)
        {
            vr_ffx_gunStep[action] = vr_ffx_stepMost;
        }
        // (E: within VR_FFX_STEP_MOST. The shove's push moves the hand fast whatever it held: the axe's no more than the gun's.)
        float most = weapon ? max(VR_FFX_STEP_MOST, vr_ffx_gunStep[action] + 1) : action == 0 ? VR_FFX_STEP_MOST : 1000;
        VR_Test_Check(vr_ffx_gripGone > 0, sprintf("%s: the off hand lets go of the grip (%g s)", wname, gone));
        VR_Test_Check(vr_ffx_stepMost <= most, sprintf("%s: ... eased off it, no jump (%g units a frame at most, %g allowed)", wname, vr_ffx_stepMost, most));
        if(action == 0)
        {
            float need = cvar_hget(cvarh_vr_flat_fg_turn_time) + cvar_hget(cvarh_vr_flat_fg_detach_time);
            float took = vr_ffx_flewAt - vr_ffx_actAt;
            VR_Test_Check(vr_ffx_flewHand == cVR_OffHand, sprintf("%s: the grab goes to the off hand (%g)", wname, vr_ffx_flewHand));
            VR_Test_Check(vr_ffx_gripGone > 0 && vr_ffx_gripGone < vr_ffx_flewAt,
                sprintf("%s: the grip let go before the grab starts (%g s before)", wname, vr_ffx_gripGone ? vr_ffx_flewAt - vr_ffx_gripGone : 0));
            VR_Test_Check(vr_ffx_offAtFlight > 8, sprintf("%s: ... and the hand off it when the brick flies (%g units)", wname, vr_ffx_offAtFlight));
            VR_Test_Check(took >= need - 0.02, sprintf("%s: the brick flies after the palm's turn and the grip's detach (%g s, %g)", wname, took, need));
            // (2026-11-11, the crowbar: its grip stayed held through the pull. The pull starts only once the off hand has
            // left the grip, a whole palm's turn later, as a gun's.)
            float turn = cvar_hget(cvarh_vr_flat_fg_turn_time);
            float offFor = vr_ffx_gripGone ? vr_ffx_flewAt - vr_ffx_gripGone : -1;
            VR_Test_Check(offFor >= turn - 0.02, sprintf("%s: the pull starts %g s after the hand left the grip (at least the turn, %g)", wname, offFor, turn));
        }
        VR_FFX_To(5, 0);
        return;
    }
    if(vr_ffx_s == 5) // the action finished (the brick caught and dropped, impulse 47), then a second to settle
    {
        if(action == 0)
        {
            if(VR_Carry_Held(p, cVR_OffHand) != vr_ffx_item && VR_Carry_Held(p, cVR_MainHand) != vr_ffx_item)
            {
                return;
            }
            localcmd("impulse 47\n");
        }
        VR_FFX_To(6, 72);
        return;
    }
    if(vr_ffx_s == 6)
    {
        if(vr_ffx_case + 1 < VR_FFX_2H_ACTIONS * VR_FFX_2H_WEAPONS)
        {
            vr_ffx_case = vr_ffx_case + 1;
            VR_FFX_To(0, 0);
            return;
        }
        localcmd("impulse 150\nimpulse 170\n");
        VR_Test_Verdict();
        vr_ffx_on = 0;
        cvar_set("vr_flatitems_test", "0");
        return;
    }
};

void() VR_FlatItemsTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_flatitems_test);
    if(!ask)
    {
        return;
    }
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    if(vr_ffx_on != ask)
    {
        VR_Test_Begin("flatitems");
        vr_ffx_on = ask;
        vr_ffx_case = 0;
        VR_FFX_To(0, 0);
        if(!VR_FlatPlay(p))
        {
            bprint("flatitems: flat play off: run with -flatscreenmode\n");
        }
    }
    if(vr_ffx_wait > 0)
    {
        vr_ffx_wait = vr_ffx_wait - 1;
        return;
    }
    if(time - vr_ffx_t0 > VR_FFX_GIVEUP && vr_ffx_s != 0)
    {
        VR_Test_Check(FALSE, sprintf("timed out: test %g, case %g, state %g", ask, vr_ffx_case, vr_ffx_s));
        localcmd("-attack\n-forward\n-offhandattack\n");
        VR_Test_Verdict();
        vr_ffx_on = 0;
        cvar_set("vr_flatitems_test", "0");
        return;
    }
    if(ask == 1 || ask == 5) { VR_FFX_Hold(p); }
    if(ask == 2) { VR_FFX_Walk(p); }
    if(ask == 3) { VR_FFX_Better(p); }
    if(ask == 4) { VR_FFX_Pictures(p); }
    if(ask == 6) { VR_FFX_PalmFirst(p); }
    if(ask == 7) { VR_FFX_PropFist(p); }
    if(ask == 8) { VR_FFX_Packs(p); }
    if(ask == 9) { VR_FFX_TwoHandGrab(p); }
};

#endif
