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
//   82, 83           UNLOAD: the left hand's, the right hand's gun's live rounds back into the reserve (VR_FlatHolster_Unload)
//   84, 85           FLIP GRIP: the left hand's, the right hand's sword or dagger turned over (VR_RevGrip_Flip: the eject button's flip)
//   48               the hands swapped (VR_FlatSwapHands: the same as 3)
//   24 + k           a drag from holster a to holster b: they trade what they hold (k the pair a < b: VR_FlatHolster_Pair)
// A drag from a hand to a holster (or back) sends that holster's L or R impulse, from hand to hand 48's swap.
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

// Holster `hs` set to weapon `wid` (its flags and record), or emptied (WID_FIST).
void(float hs, float wid, float wfl, entity inst) VR_FlatHolster_Set =
{
    setHolsterWeapon(hs, wid);
    setHolsterWeaponModel(hs, wid == WID_FIST ? WeaponIdToModel(WID_FIST, 0 /* weaponflags */) : AD_WeaponModel(wid, wfl, inst));
    setHolsterWeaponFlags(hs, wid == WID_FIST ? 0 : wfl);
    VRSetEntHolsterWeaponInst(self, hs, wid == WID_FIST ? world : inst);
};

// Holsters `a` and `b` (the engine's order: VR_FlatHolster_Hotspot) trade what they hold: the menu's drag from one to
// the other (his words 2026-11-13: "holster to holster (swap)"). Each weapon moves with its flags and record (its
// magazine, its grip); one of them empty: a move. Any weapon in a holster fits any holster (WeaponIdToHolsterable is
// the weapon's, not the holster's).
void(float a, float b) VR_FlatHolster_SwapHolsters =
{
    float hsA = VR_FlatHolster_Hotspot(a);
    float hsB = VR_FlatHolster_Hotspot(b);
    float widA = getHolsterWeapon(hsA);
    float widB = getHolsterWeapon(hsB);
    if(a == b || (widA == WID_FIST && widB == WID_FIST))
    {
        dprint(sprintf("flat holsters: holsters %g, %g: nothing to move\n", a, b));
        return;
    }
    float flA = getHolsterWeaponFlags(hsA);
    float flB = getHolsterWeaponFlags(hsB);
    entity instA = VRGetEntHolsterWeaponInst(self, hsA);
    entity instB = VRGetEntHolsterWeaponInst(self, hsB);
    VR_FlatHolster_Set(hsA, widB, flB, instB);
    VR_FlatHolster_Set(hsB, widA, flA, instA);
    VR_SoundVaried(self, CHAN_ITEM, "weapons/holster1.wav", 1, ATTN_NORM);
    vr_flatholster_swaps++;
    dprint(sprintf("flat holsters: holster %g <-> holster %g: now %g and %g\n", a, b, getHolsterWeapon(hsA), getHolsterWeapon(hsB)));
};

// The holster pair of impulse 24 + k (the engine's vr_flatholster.cpp holsterPairImpulse): k counts the pairs a < b
// in order (0 1, 0 2, .. 0 5, 1 2, .., 4 5). Sets vr_flatholster_pa, vr_flatholster_pb; FALSE when out of range.
float vr_flatholster_pa;
float vr_flatholster_pb;
float(float k) VR_FlatHolster_Pair =
{
    float n = 0;
    for(float a = 0; a < 5; a++)
    {
        for(float b = a + 1; b < 6; b++)
        {
            if(n == k)
            {
                vr_flatholster_pa = a;
                vr_flatholster_pb = b;
                return TRUE;
            }
            n++;
        }
    }
    return FALSE;
};

// UNLOAD (his words 2026-11-10: "add a per-hand 'unload button' that empties a gun's magazine (if applicable) and returns
// the ammunition to the ammo storage"): the live rounds in the hand's gun (its magazine, the super shotgun's barrels, the
// revolver's cylinder: its clip, VR_FlatReload_Unloadable) back into his reserve as a pickup would, up to its cap; what
// doesn't fit stays in the gun (nothing is dropped on the floor). Spent shells stay where they are. A short sound
// (vr_flat_unload_volume). Not while that hand reloads.
float vr_flatholster_unloads; // (the tests: rounds unloaded so far)
void(float xHand) VR_FlatHolster_Unload =
{
    float clip = VR_FlatReload_Unloadable(xHand);
    if(clip <= 0 || VR_FlatReload_Busy(self, xHand))
    {
        dprint(sprintf("flat holsters: UNLOAD hand %g: nothing to unload (clip %g, reloading %g)\n", xHand, clip,
            VR_FlatReload_Busy(self, xHand)));
        return;
    }
    float wid = VRGetEntWeapon(self, xHand);
    float n = VR_FlatReload_ClipBack(xHand, wid, VRGetEntWeaponFlags(self, xHand), TRUE);
    if(n <= 0)
    {
        VR_Carry_FullCue(xHand, self); // (full up of that ammo: the rounds stay in the gun)
        dprint(sprintf("flat holsters: UNLOAD hand %g: the reserve is full, %g rounds stay in weapon %g\n", xHand, clip, wid));
        return;
    }
    if(VRGetEntWeaponClip(self, xHand) <= 0)
    {
        if(xHand == cVR_MainHand) { self.vr_flatrl_unl1 = wid; } else { self.vr_flatrl_unl0 = wid; } // (no auto reload of it)
    }
    VRHandStopFiring(self, xHand);
    VR_Reload_AmmoChanged(self);
    W_SetCurrentAmmoFor(xHand);
    float vol = min(1, max(0, cvar_hget(cvarh_vr_flat_unload_volume)));
    if(vol > 0)
    {
        sound(self, VRGetHandChannel(xHand), "vr/reload_pouch.wav", vol, ATTN_NORM);
    }
    vr_flatholster_unloads += n;
    dprint(sprintf("flat holsters: UNLOAD hand %g: %g of %g rounds of weapon %g back into the reserve\n", xHand, n, clip, wid));
};

// ImpulseCommands, first: the menu's impulses (24 .. 38, 60 .. 79, 81 .. 85) whenever he plays on the screen, and while the holsters are
// on, the weapon keys swallowed (1 .. 8, the cycle's 10, 12, 15, 16, Quake's 225, 226): no weapon comes from what was
// collected. TRUE when the impulse was taken (cleared).
float() VR_FlatHolster_Impulses =
{
    float imp = self.impulse;
    if(((imp >= 60 && imp <= 79) || (imp >= 81 && imp <= 85) || (imp >= 24 && imp <= 38)) && VR_FlatPlay(self))
    {
        self.impulse = 0;
        if(self.health <= 0)
        {
            return TRUE;
        }
        if(imp <= 38)
        {
            if(VR_FlatHolster_Pair(imp - 24))
            {
                VR_FlatHolster_SwapHolsters(vr_flatholster_pa, vr_flatholster_pb);
            }
            return TRUE;
        }
        if(imp == 84 || imp == 85)
        {
            VR_RevGrip_Flip(imp == 84 ? cVR_OffHand : cVR_MainHand); // FLIP GRIP: as the eject button's flip (vr_revgrip.qc)
            return TRUE;
        }
        if(imp == 82 || imp == 83)
        {
            VR_FlatHolster_Unload(imp == 82 ? cVR_OffHand : cVR_MainHand);
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
