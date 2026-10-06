// `vr_pickup_test <n>` (Debug > Tests > Weapon Pickups): the map's weapons drawn as their props
// (vr_pickup_prop_models, VR_WeaponPickup_SetModel). Single player only.
// 1 every weapon pickup there is (the mission pack's when it is loaded) spawned in rows of five ahead of you, each
//   with the same weapon dropped on the floor in front of it (lying still: no throw), to compare them; a second later
//   each pickup's model, offset and drawn box are printed (pickuptest: in the console).
// 2 the weapon pickup nearest you taken into an empty hand (its own touch, as a grip or a force grab's catch takes it;
//   the hand's grip held: +grabmain):
//   the hand's weapon, its magazine and the ammo left printed.
// 3 the next of test 1's pairs before you (you moved 44 units behind it), for a look or an eyeshot.

float VR_PICKUPTEST_COUNT = 10;
.float vr_pickuptest; // vr_pickup_test 1's pair (from 1): its pickup and the weapon dropped beside it
float vr_pickuptest_view; // the pair vr_pickup_test 3 last showed

float(float i) VR_PickupTest_Weapon =
{
    if(i == 0) { return WID_SHOTGUN; }
    if(i == 1) { return WID_SUPER_SHOTGUN; }
    if(i == 2) { return WID_NAILGUN; }
    if(i == 3) { return WID_SUPER_NAILGUN; }
    if(i == 4) { return WID_GRENADE_LAUNCHER; }
    if(i == 5) { return WID_ROCKET_LAUNCHER; }
    if(i == 6) { return WID_LIGHTNING; }
    if(i == 7) { return WID_MJOLNIR; }
    if(i == 8) { return WID_LASER_CANNON; }
    return WID_PROXIMITY_GUN;
};

// The map's spawn function of weapon `wid`'s pickup, run on self.
void(float wid) VR_PickupTest_SpawnFunc =
{
    if(wid == WID_SHOTGUN)               { self.classname = "weapon_shotgun"; weapon_shotgun(); }
    else if(wid == WID_SUPER_SHOTGUN)    { self.classname = "weapon_supershotgun"; weapon_supershotgun(); }
    else if(wid == WID_NAILGUN)          { self.classname = "weapon_nailgun"; weapon_nailgun(); }
    else if(wid == WID_SUPER_NAILGUN)    { self.classname = "weapon_supernailgun"; weapon_supernailgun(); }
    else if(wid == WID_GRENADE_LAUNCHER) { self.classname = "weapon_grenadelauncher"; weapon_grenadelauncher(); }
    else if(wid == WID_ROCKET_LAUNCHER)  { self.classname = "weapon_rocketlauncher"; weapon_rocketlauncher(); }
    else if(wid == WID_LIGHTNING)        { self.classname = "weapon_lightning"; weapon_lightning(); }
    else if(wid == WID_MJOLNIR)          { self.classname = "weapon_mjolnir"; weapon_mjolnir(); }
    else if(wid == WID_LASER_CANNON)     { self.classname = "weapon_laser_gun"; weapon_laser_gun(); }
    else                                 { self.classname = "weapon_proximity_gun"; weapon_proximity_gun(); }
};

void(entity e) VR_PickupTest_Print =
{
    bprint(sprintf("pickuptest: %s %s spin %g rigid %g obj %g at %v", e.classname, e.model, e.vr_pickup_spin, e.vr_rigid,
        e.vr_pickup_obj, e.origin));
    bprint(sprintf(" scale %v box %v .. %v mid %g", e.model_scale, e.mins, e.maxs, e.vr_pickup_mid));
    bprint(sprintf(" offset %v drawn %v .. %v\n", e.model_offset, drawnbounds(e, 0), drawnbounds(e, 1)));
};

void() VR_PickupTest_Report =
{
    entity e = nextent(world);
    while(e)
    {
        if(e.vr_pickuptest)
        {
            VR_PickupTest_Print(e);
        }
        e = nextent(e);
    }
    remove(self);
};

void() VR_PickupTest_Row =
{
    makevectors(self.v_angle_y * '0 1 0');
    vector fwd = v_forward;
    vector right = v_right;
    entity player = self;

    float n = 0;
    float i;
    for(i = 0; i < VR_PICKUPTEST_COUNT; i++)
    {
        if(VR_Pack_WeaponAvailable(VR_PickupTest_Weapon(i))) { n = n + 1; }
    }

    float k = 0;
    for(i = 0; i < VR_PICKUPTEST_COUNT; i++)
    {
        float wid = VR_PickupTest_Weapon(i);
        if(!VR_Pack_WeaponAvailable(wid))
        {
            continue;
        }
        // Rows of five pairs, 72 units apart, a row 100 units behind the last.
        float row = floor(k / 5);
        float inRow = min(5, n - row * 5);
        vector side = right * ((k - row * 5 - (inRow - 1) * 0.5) * 72);
        vector ahead = fwd * (100 + row * 100);
        k = k + 1;

        // Its pickup, as the map's: placed (dropped to the floor, floated, made an object) by its own spawn function.
        entity pickup = spawn();
        setorigin(pickup, player.origin + ahead + side - right * 12);
        pickup.angles = '0 1 0' * player.v_angle_y;
        self = pickup;
        VR_PickupTest_SpawnFunc(wid);
        self = player;
        pickup.vr_pickuptest = k;

        // The same weapon dropped on the floor to its right, pointing the way you face.
        vector dropAt = player.origin + ahead + side + right * 14;
        traceline(dropAt, dropAt - '0 0 256', TRUE, player);
        entity dropped = CreateThrownWeapon(world, wid, 0, WeaponInst_New(WeaponIdToBaseClipSize(wid)),
            trace_endpos + '0 0 12', '0 1 0' * player.v_angle_y, '0 0 0', '0 0 0', FALSE, FALSE,
            WeaponIdToThrowDamage(wid));
        dropped.vr_pickuptest = k;
    }
    bprint(sprintf("pickuptest: 1 %g weapon pickups ahead, each with the same weapon dropped beside it (vr_pickup_prop_models %g)\n",
        n, cvar_hget(cvarh_vr_pickup_prop_models)));

    vr_pickuptest_view = 0;
    entity r = spawn();
    r.think = VR_PickupTest_Report;
    r.nextthink = time + 1;
};

void() VR_PickupTest_Take =
{
    entity best = world;
    float bestDist = 100000;
    entity e = nextent(world);
    while(e)
    {
        if(e.model != string_null && (e.think2 == weapon_item_think || e.vr_pickup_obj == QVR_PICKUPOBJ_WEAPON) &&
           vlen(e.origin - self.origin) < bestDist)
        {
            best = e;
            bestDist = vlen(e.origin - self.origin);
        }
        e = nextent(e);
    }
    float hand = VRIsHandEmpty(self, cVR_MainHand) ? cVR_MainHand : VRIsHandEmpty(self, cVR_OffHand) ? cVR_OffHand : -1;
    if(best == world || hand < 0)
    {
        bprint("pickuptest: 2 no weapon pickup in the map, or no empty hand\n");
        return;
    }
    bprint(sprintf("pickuptest: 2 taking %s (%s) %g units away into the %s hand\n", best.classname, best.model, bestDist,
        hand == cVR_MainHand ? "main" : "off"));
    VR_PickupTest_Print(best);
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
    float wid = VRGetEntWeapon(self, hand);
    bprint(sprintf("pickuptest: 2 the hand holds weapon %g (%s), magazine %g, ammo left %g; the pickup %s\n", wid,
        WeaponIdToWeaponName(wid), VRGetEntWeaponClip(self, hand),
        VR_AmmoUtil_GetAmmoCountByAmmoId(self, WeaponIdToAmmoId(wid, 0)),
        best.model == string_null ? "taken" : "still there"));
};

// The next pair (vr_pickup_test 1's) before you: you put 44 units behind its middle, facing it.
void() VR_PickupTest_View =
{
    vr_pickuptest_view = vr_pickuptest_view + 1;
    entity e = nextent(world);
    while(e)
    {
        if(e.vr_pickuptest == vr_pickuptest_view && e.classname != "thrown_weapon")
        {
            makevectors(self.v_angle_y * '0 1 0');
            vector at = e.origin + v_right * 12 - v_forward * 44;
            at_z = self.origin_z;
            setorigin(self, at);
            self.velocity = '0 0 0';
            bprint(sprintf("pickuptest: 3 pair %g (%s at %v) before you, at %v\n", vr_pickuptest_view, e.classname,
                e.origin, self.origin));
            return;
        }
        e = nextent(e);
    }
    bprint(sprintf("pickuptest: 3 no pair %g\n", vr_pickuptest_view));
    vr_pickuptest_view = 0;
};

// Every server frame (StartFrame): a pickup test asked for (vr_pickup_test).
void() VR_PickupTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_pickup_test);
    if(!ask)
    {
        return;
    }
    cvar_set("vr_pickup_test", "0");
    entity p = find(world, classname, "player");
    if(p == world || deathmatch || coop)
    {
        return;
    }
    entity oself = self;
    self = p;
    if(ask == 1)
    {
        VR_PickupTest_Row();
    }
    else if(ask == 2)
    {
        VR_PickupTest_Take();
    }
    else if(ask == 3)
    {
        VR_PickupTest_View();
    }
    self = oself;
};
