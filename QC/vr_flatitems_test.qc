#ifdef QVR_TESTS
// vr_flatitems_test.qc -- pickups in flat-screen play (-flatscreenmode; docs/vr-port/FLATPLAY.md, "Pickups"), through the
// real paths: the map's own spawn functions, E (impulse 46), +attack, and +forward over the pickup's own place.
// `vr_flatitems_test 1`: each thing that can stay in the off hand, grabbed with E (it stays held: not better, or full up;
//   what the mode takes at once is noted), the main hand's shotgun fired through it: the shot goes, the hand keeps it.
//   Misc/quakevr/flatitems_test.sh fails the run on any "Assertion failure" in the log.
// `vr_flatitems_test 2`: walked over, each kind of pickup is taken, and armour puts the plates on (`flatwalk: ...`).
// `vr_flatitems_test 3`: E on armour that is better than what he wears: on him at once, the plates on, nothing held.
// `vr_flatitems_test 5`: as he played: armour in the left hand with E, 2 for the shotgun, `give n 40` on the console, the shot:
//   the one way a shot reaches VR_AmmoUtil_GetAmmoCountByAmmoId's assertion (FLATPLAY.md, "Pickups"): the engine's give
//   wrote a count where the game keeps the ammo id. The armour is only there as it was in his report.
// `vr_flatitems_test 4`: red armour ahead, to look at: walks over it and takes two pictures of him from the front (vr_flat_orbit),
//   before and after (`flatplates: before`, `flatplates: after`).
// Ends `flatitems: N passed, M failed`.

float vr_ffx_on;    // the running test's number
float vr_ffx_s;     // its state
float vr_ffx_case;  // which pickup
float vr_ffx_t0;    // when the state began (a stuck test: FAIL "timed out")
float vr_ffx_wait;  // frames left before the state goes on (the word to the engine, the console's +attack)
float vr_ffx_ammo;  // shells before the shot
vector vr_ffx_from; // where the walk starts
entity vr_ffx_item; // the pickup of this case

float VR_FFX_GIVEUP = 8; // seconds a state may last
float VR_FFX_HOLD_CASES = 6;
float VR_FFX_WALK_CASES = 16;

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
    else                 { it.classname = "item_spikes"; item_spikes(); }
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
    return "nails";
};

// Placed: PlaceItem has run and (an object's) VR_PickupObj_Place after it.
float(entity xItem) VR_FFX_Placed =
{
    if(xItem.think == PlaceItem) { return FALSE; }
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

// Puts it where E's grab looks: along the free hand as it hangs, a short way out (VR_Forcegrab_FindTarget asks the hand,
// whatever the view; a thing a player walks up to and presses E on).
void(entity p, entity xItem) VR_FFX_LookAt =
{
    float hand = VR_Hand_Free(p, cVR_OffHand, VR_HANDFREE_LOCKED) ? cVR_OffHand : cVR_MainHand;
    makeforward(VRGetEntHandRot(p, hand));
    vector at = VRGetEntHandPos(p, hand) + v_forward * 36 - (xItem.mins + xItem.maxs) * 0.5;
    setorigin(xItem, at);
    xItem.velocity = '0 0 0';
};

vector(entity p) VR_FFX_Ahead =
{
    makevectors('0 1 0' * p.v_angle_y);
    return v_forward;
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
            VR_FFX_To(3, 0);
        }
        return;
    }
    if(vr_ffx_s == 3) // E
    {
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
            vr_ffx_ammo = p.ammo_shells;
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
        vr_ffx_ammo = p.ammo_shells;
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
        p.impulse = 2;
        vr_ffx_wait = 4;
        return;
    }
    if(vr_ffx_s == 5) // the shot
    {
        if(p.ammo_shells < vr_ffx_ammo)
        {
            localcmd("-attack\n");
            VR_Test_Check(TRUE, sprintf("%s: the shot goes with it %s (shells %g to %g)", vr_ffx_item.classname,
                vr_ffx_item.model == string_null ? "taken" : "held", vr_ffx_ammo, p.ammo_shells));
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
    return p.ammo_nails > 0;
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
            // (Ammo and health lie on the floor as rigid boxes under his feet, and E is how they are taken: reported, not
            // asked for. FLATPLAY.md, "Pickups".)
            float isBox = vr_ffx_case == 7 || vr_ffx_case == 8 || vr_ffx_case >= 13;
            if(isBox)
            {
                dprint(sprintf("flatitems: walked over the %s (a box): taken %g\n", VR_FFX_KindName(vr_ffx_case), gone));
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
};

#endif
