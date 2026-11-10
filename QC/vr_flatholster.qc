// vr_flatholster.qc -- flat-screen play's holsters (vr_flat_holsters, on by default; docs/vr-port/FLATPLAY.md,
// "Holsters"). His words 2026-11-10: "For fairness with the VR gameplay ... the player will be able to only carry 6
// weapons of their choice in their holsters, plus 2 weapons in their hands maximum."
//
// What he carries is the headset's: the six holsters (holsterweapon0..5, vr_wpnholster.qc) and the two hands, nothing
// else. The weapon keys that conjured a weapon from what was collected (1, 2, 4..8, the wheel's cycle) do nothing while it
// is on (VR_FlatHolster_Impulses); 3 swaps the hands (vr_flat_key_swap, impulse 48) as before. A weapon walked over goes
// into a free hand, and with both hands full it is left lying, as in a headset (weapon_touch: "a hand must be free").
//
// The engine's holster menu (Quake/vr/vr_flatholster.cpp: the wheel's button held) sends one impulse a button:
//   60 + 3 * i + 0   holster i and the left hand (the off hand) trade what they hold
//   60 + 3 * i + 1   holster i's weapon dropped to the ground
//   60 + 3 * i + 2   holster i and the right hand (the main hand) trade what they hold
//   78, 79           the left hand's, the right hand's weapon (or the prop it carries) dropped (VR_Forcegrab_FlatDropHand)
//   81               DROP BOTH: both hands drop what they hold, the off hand's first (H's order, VR_Forcegrab_FlatDrop)
//   48               the hands swapped (VR_FlatSwapHands: the same as 3)
// (80 is vr_flatholster_test's, below.)
// Holster i is the engine's STAT_QVR_HOLSTERWEAPON0 + i: holsterweapon i (VR_FlatHolster_Hotspot).

float vr_flatholster_swaps;   // (the tests: what the menu did)
float vr_flatholster_drops;
float vr_flatholster_blocked; // the weapon keys swallowed
#ifdef QVR_TESTS
void() VR_FlatHolster_TestPickup;
#endif

float(entity p) VR_FlatHolsters =
{
    return VR_FlatPlay(p) && cvar_hget(cvarh_vr_flat_holsters) != 0 && getWeaponMode() == QVR_WEAPONMODE_IMMERSIVE;
};

// Holster i (holsterweapon i, the engine's stat order) as its hotspot (getHolsterWeapon's).
float(float i) VR_FlatHolster_Hotspot =
{
    if(i == 0) { return QVR_HS_LEFT_SHOULDER_HOLSTER; }
    if(i == 1) { return QVR_HS_RIGHT_SHOULDER_HOLSTER; }
    if(i == 2) { return QVR_HS_LEFT_HIP_HOLSTER; }
    if(i == 3) { return QVR_HS_RIGHT_HIP_HOLSTER; }
    if(i == 4) { return QVR_HS_LEFT_UPPER_HOLSTER; }
    return QVR_HS_RIGHT_UPPER_HOLSTER;
};

// Holster `hs` and hand `hand` trade what they hold, each weapon with its record (its magazine, its id): the holster's
// drawn into the hand (VRDrawHolsterWeapon), the hand's put in the holster (VRPutWeaponInHolster). A weapon too big for
// a holster stays in the hand (VRHolsterRefused: the word and the buzz a headset's gets); a hand carrying a box or a prop
// keeps it (drop it first). (`self` the player: the holster's getters are self's.)
void(float hs, float hand) VR_FlatHolster_Swap =
{
    float hw = getHolsterWeapon(hs);
    float empty = VRIsHandWeaponless(self, hand);
    if(VR_Carry_Held(self, hand) != world)
    {
        sprint(self, "That hand carries something: drop it first.\n");
        dprint(sprintf("flat holsters: holster %g, hand %g: the hand carries something\n", hs, hand));
        return;
    }
    if(empty && hw == WID_FIST)
    {
        dprint(sprintf("flat holsters: holster %g, hand %g: both empty\n", hs, hand));
        return;
    }
    float wid = VRGetEntWeapon(self, hand);
    float wfl = VRGetEntWeaponFlags(self, hand);
    if(!empty && !WeaponIdToHolsterable(wid, wfl))
    {
        VRHolsterRefused(self, hand, hs, wid, wfl);
        return;
    }
    entity inst = VRGetEntWeaponInst(self, hand);
    VRHandStopFiring(self, hand);
    if(!empty)
    {
        VRSetHandEmpty(self, hand); // (its record kept: `inst`, into the holster below)
    }
    if(hw != WID_FIST)
    {
        VRDrawHolsterWeapon(self, hand, hs); // (the holster empty, its record in the hand)
    }
    if(!empty)
    {
        VRPutWeaponInHolster(self, hand, hs, wid, wfl, inst);
    }
    W_SetCurrentAmmoFor(hand);
    vr_flatholster_swaps++;
    dprint(sprintf("flat holsters: holster %g <-> hand %g: hand now %g, holster now %g\n", hs, hand,
        VRGetEntWeapon(self, hand), getHolsterWeapon(hs)));
};

// Holster `hs`'s weapon dropped to the ground ahead of him (a weapon lying about, as a hand's drop leaves it: its record
// with it), the holster empty.
void(float hs) VR_FlatHolster_Drop =
{
    float wid = getHolsterWeapon(hs);
    if(wid == WID_FIST)
    {
        dprint(sprintf("flat holsters: holster %g: empty, nothing dropped\n", hs));
        return;
    }
    float wfl = getHolsterWeaponFlags(hs);
    entity inst = VRGetEntHolsterWeaponInst(self, hs);
    makevectors([0, self.v_angle_y, 0]);
    vector org = self.origin + v_forward * cvar_hget(cvarh_vr_flat_holster_drop_ahead) + '0 0 8';
    traceline(self.origin + '0 0 8', org, TRUE, self);
    org = trace_endpos - v_forward * 4; // (short of a wall)
    entity thrown = CreateThrownWeapon(self, wid, wfl, inst, org, [0, self.v_angle_y + 90, 0],
        v_forward * 60 + '0 0 60' + self.velocity * 0.5, '0 0 0', FALSE /* disappear on hit */, FALSE /* deal damage */, 0);
    if(thrown != world)
    {
        thrown.vr_wpn_from = self;
        thrown.vr_rigid = 1;
        float gravityScale = VR_Throw_GravityScale();
        if(gravityScale)
        {
            thrown.gravity = gravityScale;
        }
    }
    setHolsterWeapon(hs, WID_FIST);
    setHolsterWeaponModel(hs, WeaponIdToModel(WID_FIST, 0 /* weaponflags */));
    setHolsterWeaponFlags(hs, 0);
    VRSetEntHolsterWeaponInst(self, hs, world);
    VR_SoundVaried(self, CHAN_ITEM, "weapons/holster0.wav", 1, ATTN_NORM);
    vr_flatholster_drops++;
    dprint(sprintf("flat holsters: holster %g: weapon %g dropped\n", hs, wid));
};

// ImpulseCommands, first: the menu's impulses (60 .. 79) whenever he plays on the screen, and while the holsters are
// on, the weapon keys swallowed (1 .. 8, the cycle's 10, 12, 15, 16, Quake's 225, 226): no weapon comes from what was
// collected. TRUE when the impulse was taken (cleared).
float() VR_FlatHolster_Impulses =
{
    float imp = self.impulse;
    if(((imp >= 60 && imp <= 79) || imp == 81) && VR_FlatPlay(self))
    {
        self.impulse = 0;
        if(self.health <= 0)
        {
            return TRUE;
        }
        if(imp == 81)
        {
            // Each hand's own drop, the off hand's first (a prop carried in both: let go of by the first).
            float droppedOff = VR_Forcegrab_FlatDropHand(cVR_OffHand);
            float droppedMain = VR_Forcegrab_FlatDropHand(cVR_MainHand);
            vr_flatholster_drops += droppedOff + droppedMain;
            dprint(sprintf("flat holsters: DROP BOTH: off hand %g, main hand %g\n", droppedOff, droppedMain));
            return TRUE;
        }
        if(imp == 78 || imp == 79)
        {
            float hand = imp == 78 ? cVR_OffHand : cVR_MainHand;
            if(VR_Forcegrab_FlatDropHand(hand))
            {
                vr_flatholster_drops++;
            }
            return TRUE;
        }
        float i = floor((imp - 60) / 3);
        float b = imp - 60 - i * 3;
        float hs = VR_FlatHolster_Hotspot(i);
        if(b == 1)
        {
            VR_FlatHolster_Drop(hs);
        }
        else
        {
            VR_FlatHolster_Swap(hs, b == 0 ? cVR_OffHand : cVR_MainHand);
        }
        return TRUE;
    }
#ifdef QVR_TESTS
    if(imp == 80 && !deathmatch && !coop)
    {
        self.impulse = 0;
        VR_FlatHolster_TestPickup();
        return TRUE;
    }
#endif
    if(VR_FlatHolsters(self) &&
       ((imp >= 1 && imp <= 8) || imp == 10 || imp == 12 || imp == 15 || imp == 16 || imp == 225 || imp == 226))
    {
        self.impulse = 0;
        vr_flatholster_blocked++;
        dprint(sprintf("flat holsters: impulse %g does nothing (the holster menu: the wheel's button)\n", imp));
        return TRUE;
    }
    return FALSE;
};

#ifdef QVR_TESTS
// vr_flatholster_test's QC side (Quake/vr/vr_flatholster.cpp runs it): impulse 80 lays a super shotgun at his feet and
// has him walk onto it (its touch with him: the body's, as a flat-screen player's walk takes things), then says whether
// it was taken: with both hands full it must be left lying (the headset's rule: a hand must be free).
float vr_flatholster_pickup_left; // 1 left lying, 2 taken
void() VR_FlatHolster_TestPickup =
{
    entity p = self;
    entity e = spawn();
    e.classname = "weapon_supershotgun";
    setorigin(e, p.origin);
    entity oself = self;
    self = e;
    weapon_supershotgun();
    self = oself;
    float wasMain = VRGetEntWeapon(p, cVR_MainHand), wasOff = VRGetEntWeapon(p, cVR_OffHand);
    float oldHand = p.touchinghand;
    p.touchinghand = cVR_FakeHand;
    other = p;
    self = e;
    if(e.touch)
    {
        e.touch();
    }
    self = oself;
    p.touchinghand = oldHand;
    float taken = e.model == string_null || VRGetEntWeapon(p, cVR_MainHand) != wasMain || VRGetEntWeapon(p, cVR_OffHand) != wasOff;
    vr_flatholster_pickup_left = taken ? 2 : 1;
    bprint(sprintf("flatholster pickup: hands %g %g, the super shotgun %s\n", wasMain, wasOff, taken ? "taken" : "left lying"));
    if(!taken)
    {
        remove(e);
    }
};
#endif
