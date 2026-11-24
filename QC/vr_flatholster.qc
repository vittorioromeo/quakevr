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
//   86, 87           the number keys 1, 2: the left, the right hand cycled through the holsters (VR_FlatHolster_Cycle)
//   88, 89           the number keys 4, 5: the left, the right hand's weapon into a free holster (VR_FlatHolster_Put)
//                    (6, 7 send 78, 79)
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
    return VR_FlatPlay(p) && clientpref(p, cvarh_vr_flat_holsters) != 0 && getWeaponMode() == QVR_WEAPONMODE_IMMERSIVE;
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

// The holsters' number keys (his words 2026-11-22; the engine's vr_flatplay.cpp handKeys): 1 and 2 cycle the left and the
// right hand through the holstered weapons (impulse 86, 87: VR_FlatHolster_Cycle), 4 and 5 put the hand's weapon in a
// free holster (88, 89: VR_FlatHolster_Put), 6 and 7 drop (78, 79, above). Each weapon moves with its record by the
// headset's own steps (VRDrawHolsterWeapon, VRPutWeaponInHolster, VRHolsterHandWeapon), so a multiplayer client's key
// is the same impulse and everyone sees the holsters' stats and models change.
.float vr_flatcyc_slot0;   // 1 + the holster the left (off) hand's weapon was drawn from by its key; 0 none
.float vr_flatcyc_slot1;   // the right (main) hand's
.entity vr_flatcyc_inst0;  // that weapon's record: the memory holds while the hand still holds that very weapon
.entity vr_flatcyc_inst1;
float vr_flatholster_cycles; // (the tests: the keys' moves)
float vr_flatholster_puts;
float vr_flatholster_keys;   // (the tests: impulses 86 .. 89 taken, whatever they did)

// The holster (0 .. 5) the weapon in `xHand` was drawn from by the cycle, -1 none (another weapon in it since).
float(float xHand) VR_FlatHolster_From =
{
    float s = xHand == cVR_MainHand ? self.vr_flatcyc_slot1 : self.vr_flatcyc_slot0;
    entity inst = xHand == cVR_MainHand ? self.vr_flatcyc_inst1 : self.vr_flatcyc_inst0;
    if(s <= 0 || inst == world || VRIsHandWeaponless(self, xHand) || VRGetEntWeaponInst(self, xHand) != inst)
    {
        return -1;
    }
    return s - 1;
};

void(float xHand, float i) VR_FlatHolster_Remember =
{
    entity inst = world;
    if(i >= 0)
    {
        inst = VRGetEntWeaponInst(self, xHand);
    }
    if(xHand == cVR_MainHand)
    {
        self.vr_flatcyc_slot1 = i + 1;
        self.vr_flatcyc_inst1 = inst;
    }
    else
    {
        self.vr_flatcyc_slot0 = i + 1;
        self.vr_flatcyc_inst0 = inst;
    }
};

// A free holster (0 .. 5) for `xHand`'s weapon, -1 none: `prefer` (the one it came from; -1 none) if empty, else the
// hand's own side's hip, chest and shoulder, then the other side's. `vacant` (-1 none) counts as empty: the holster the
// cycle draws from in the same move.
float(float xHand, float prefer, float vacant) VR_FlatHolster_FreeFor =
{
    if(prefer >= 0 && (prefer == vacant || getHolsterWeapon(VR_FlatHolster_Hotspot(prefer)) == WID_FIST))
    {
        return prefer;
    }
    for(float k = 0; k < 6; k++)
    {
        float row = k - floor(k / 3) * 3; // 0 the hip, 1 the chest, 2 the shoulder
        float side = 1;                    // 0 the left (even holsters), 1 the right
        if((k < 3) == (xHand == cVR_OffHand))
        {
            side = 0;
        }
        float i = side;
        if(row == 0)
        {
            i = 2 + side;
        }
        else if(row == 1)
        {
            i = 4 + side;
        }
        if(i == vacant || getHolsterWeapon(VR_FlatHolster_Hotspot(i)) == WID_FIST)
        {
            return i;
        }
    }
    return -1;
};

// Whether `xHand` may take part (not carrying a box or a prop: VR_FlatHolster_Swap's rule).
float(float xHand) VR_FlatHolster_KeyHandOk =
{
    if(VR_Carry_Held(self, xHand) != world)
    {
        sprint(self, "That hand carries something: drop it first.\n");
        return FALSE;
    }
    return TRUE;
};

float(float xHand, float next) VR_FlatHolster_DrawInto;

// 1 / 2: the next holstered weapon after the one the hand holds (the holsters' order, 0 .. 5, the menu's) into `xHand`;
// what it held back into a holster (the one it came from, else a free one: VR_FlatHolster_FreeFor, the drawn one's too:
// a trade); after the last, the hand empty. A holstered weapon is never in the other hand, so the other hand's is skipped
// by itself. A weapon too big for a holster, or no holster free, and nothing moves (a short word why).
void(float xHand) VR_FlatHolster_Cycle =
{
    if(!VR_FlatHolster_KeyHandOk(xHand))
    {
        return;
    }
    float empty = VRIsHandWeaponless(self, xHand);
    float from = VR_FlatHolster_From(xHand);
    float next = -1;
    for(float i = from + 1; i < 6 && next < 0; i++)
    {
        if(getHolsterWeapon(VR_FlatHolster_Hotspot(i)) != WID_FIST)
        {
            next = i;
        }
    }
    if(empty && next < 0)
    {
        sprint(self, "No weapon in the holsters.\n");
        dprint(sprintf("flat holsters: cycle hand %g: nothing holstered\n", xHand));
        return;
    }
    if(VR_FlatHolster_DrawInto(xHand, next))
    {
        vr_flatholster_cycles++;
    }
};

// Holster `next`'s weapon (0 .. 5; -1 none: the hand left empty) drawn into `xHand`, and what the hand held back into a
// holster (the one it came from, else a free one: VR_FlatHolster_FreeFor, `next`'s own counting as free: a trade), each
// weapon with its record by the headset's own steps (VRDrawHolsterWeapon, VRPutWeaponInHolster). The number keys' cycle
// and the headset's weapon wheel (VR_Wheel_Pick) share it. A weapon too big for a holster, or no holster free: nothing
// moves (a short word why). TRUE when it moved.
float(float xHand, float next) VR_FlatHolster_DrawInto =
{
    float empty = VRIsHandWeaponless(self, xHand);
    if(empty && next < 0)
    {
        return FALSE;
    }
    float from = VR_FlatHolster_From(xHand);
    float wid = VRGetEntWeapon(self, xHand);
    float wfl = VRGetEntWeaponFlags(self, xHand);
    float to = -1;
    if(!empty)
    {
        if(!WeaponIdToHolsterable(wid, wfl))
        {
            VRHolsterRefused(self, xHand, VR_FlatHolster_Hotspot(max(0, next)), wid, wfl);
            return FALSE;
        }
        to = VR_FlatHolster_FreeFor(xHand, from, next);
        if(to < 0)
        {
            sprint(self, sprintf("No free holster for the %s.\n", WeaponNameOf(wid, wfl)));
            dprint(sprintf("flat holsters: cycle hand %g: no free holster for weapon %g\n", xHand, wid));
            return FALSE;
        }
    }
    entity inst = VRGetEntWeaponInst(self, xHand);
    VRHandStopFiring(self, xHand);
    if(!empty)
    {
        VRSetHandEmpty(self, xHand); // (its record kept: `inst`, into the holster below)
    }
    if(next >= 0)
    {
        VRDrawHolsterWeapon(self, xHand, VR_FlatHolster_Hotspot(next));
    }
    if(!empty)
    {
        VRPutWeaponInHolster(self, xHand, VR_FlatHolster_Hotspot(to), wid, wfl, inst);
    }
    W_SetCurrentAmmoFor(xHand);
    VR_FlatHolster_Remember(xHand, next);
    dprint(sprintf("flat holsters: cycle hand %g: holster %g drawn, weapon %g into holster %g: hand now %g\n", xHand, next,
        wid, to, VRGetEntWeapon(self, xHand)));
    return TRUE;
};

// 4 / 5: `xHand`'s weapon into a free holster (VR_FlatHolster_FreeFor: the one it came from first), the hand empty, as a
// headset's hand lets it go at a holster (VRHolsterHandWeapon). An empty hand, none free or one too big: nothing moves.
void(float xHand) VR_FlatHolster_Put =
{
    if(!VR_FlatHolster_KeyHandOk(xHand))
    {
        return;
    }
    if(VRIsHandWeaponless(self, xHand))
    {
        dprint(sprintf("flat holsters: holster hand %g: empty\n", xHand));
        return;
    }
    float wid = VRGetEntWeapon(self, xHand);
    float wfl = VRGetEntWeaponFlags(self, xHand);
    if(!WeaponIdToHolsterable(wid, wfl))
    {
        VRHolsterRefused(self, xHand, VR_FlatHolster_Hotspot(0), wid, wfl);
        return;
    }
    float to = VR_FlatHolster_FreeFor(xHand, VR_FlatHolster_From(xHand), -1);
    if(to < 0)
    {
        sprint(self, sprintf("No free holster for the %s.\n", WeaponNameOf(wid, wfl)));
        dprint(sprintf("flat holsters: holster hand %g: no free holster for weapon %g\n", xHand, wid));
        return;
    }
    VRHandStopFiring(self, xHand);
    VRHolsterHandWeapon(self, xHand, VR_FlatHolster_Hotspot(to));
    VR_FlatHolster_Remember(xHand, -1);
    vr_flatholster_puts++;
    dprint(sprintf("flat holsters: holster hand %g: weapon %g into holster %g\n", xHand, wid, to));
};

// The headset's weapon wheel (Quake/vr/vr_weaponwheel.cpp; the client's "vr_wheel_pick <hand> <slot>", run with `self`
// the player who let go of it): `xHand` takes holster `slot`'s weapon (0 .. 5, the holster menu's order) as reaching
// there would, by the holster menu's own steps. Immersive: holster `slot`'s weapon drawn (VR_FlatHolster_DrawInto: what
// the hand held back to its own holster, else a free one, `slot`'s counting); an empty holster takes the hand's weapon
// (as a hand letting go there: too big refused, VRHolsterRefused); `slot` -1 ("empty hand"): the hand's weapon put away
// (VR_FlatHolster_Put). Quick Slots: the holster's copy drawn (its weapon stays in the holster, the hand's own let go: it
// is still in what was collected), an empty holster given a copy of the hand's (VRLetGoAtHolster's), -1 the hand empty.
// A weapon drawn stays in the hand until its grip is pressed (VR_Scene_HoldGiven: a wheel needs no grip held), then the
// grip holds it as ever. A holstered weapon is never in the other hand (immersive), so the wheel never offers that one.
// The wheel's two actions (his words 2026-11-23), either mode: `slot` -2 DROP, what the hand holds dropped (6 and 7's,
// the holster menu's DROP: VR_Forcegrab_FlatDropHand, a carried prop too); -3 UNLOAD, the hand's gun's live rounds back
// into the reserve (the holster menu's UNLOAD: VR_FlatHolster_Unload).
float VR_WHEEL_DROP = -2;
float VR_WHEEL_UNLOAD = -3;
void(float xHand, float slot) VR_Wheel_Pick =
{
    if(self.health <= 0)
    {
        return;
    }
    if(slot == VR_WHEEL_DROP)
    {
        float dropped = VR_Forcegrab_FlatDropHand(xHand);
        vr_flatholster_drops += dropped;
        dprint(sprintf("weapon wheel: hand %g, DROP: %s\n", xHand, dropped ? "dropped" : "nothing held"));
        return;
    }
    if(slot == VR_WHEEL_UNLOAD)
    {
        VR_FlatHolster_Unload(xHand);
        return;
    }
    if(!VR_FlatHolster_KeyHandOk(xHand))
    {
        return;
    }
    float was = VRGetEntWeapon(self, xHand);
    float hs = VR_FlatHolster_Hotspot(max(0, slot));
    float held = getHolsterWeapon(hs);
    float empty = VRIsHandWeaponless(self, xHand);
    if(getWeaponMode() != QVR_WEAPONMODE_IMMERSIVE)
    {
        VRHandStopFiring(self, xHand);
        if(slot >= 0 && held == WID_FIST)
        {
            if(!empty)
            {
                VRLetGoAtHolster(self, xHand, hs, VRGetEntWeapon(self, xHand), VRGetEntWeaponFlags(self, xHand), FALSE);
            }
        }
        else
        {
            if(!empty)
            {
                VRSetHandEmpty(self, xHand);
            }
            if(slot >= 0)
            {
                VRDrawHolsterWeapon(self, xHand, hs);
            }
            W_SetCurrentAmmoFor(xHand);
        }
    }
    else if(slot < 0)
    {
        VR_FlatHolster_Put(xHand);
    }
    else if(held == WID_FIST)
    {
        if(!empty)
        {
            float wid = VRGetEntWeapon(self, xHand);
            float wfl = VRGetEntWeaponFlags(self, xHand);
            if(!WeaponIdToHolsterable(wid, wfl))
            {
                VRHolsterRefused(self, xHand, hs, wid, wfl);
                return;
            }
            VRHandStopFiring(self, xHand);
            VRHolsterHandWeapon(self, xHand, hs);
            VR_FlatHolster_Remember(xHand, -1);
        }
    }
    else
    {
        VR_FlatHolster_DrawInto(xHand, slot);
    }
    if(!VRIsHandWeaponless(self, xHand))
    {
        VR_Scene_HoldGiven(self, xHand);
    }
    dprint(sprintf("weapon wheel: hand %g, holster %g: weapon %g -> %g, the holster %g -> %g\n", xHand, slot, was,
        VRGetEntWeapon(self, xHand), held, getHolsterWeapon(hs)));
};

// ImpulseCommands, first: the menu's impulses (24 .. 38, 60 .. 79, 81 .. 85) and the number keys' (86 .. 89, the holsters on) whenever he plays on the screen, and while the holsters are
// on, the weapon keys swallowed (1 .. 8, the cycle's 10, 12, 15, 16, Quake's 225, 226): no weapon comes from what was
// collected. TRUE when the impulse was taken (cleared).
float() VR_FlatHolster_Impulses =
{
    float imp = self.impulse;
    if(((imp >= 60 && imp <= 79) || (imp >= 81 && imp <= 89) || (imp >= 24 && imp <= 38)) && VR_FlatPlay(self))
    {
        self.impulse = 0;
        if(self.health <= 0)
        {
            return TRUE;
        }
        if(imp >= 86)
        {
            vr_flatholster_keys++;
            if(!VR_FlatHolsters(self))
            {
                dprint(sprintf("flat holsters: impulse %g: the holsters are off\n", imp));
            }
            else if(imp <= 87)
            {
                VR_FlatHolster_Cycle(imp == 86 ? cVR_OffHand : cVR_MainHand);
            }
            else
            {
                VR_FlatHolster_Put(imp == 88 ? cVR_OffHand : cVR_MainHand);
            }
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
