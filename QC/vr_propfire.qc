// vr_propfire.qc -- A weapon fired where it is: in the hand, or as a prop the hand holds (embedfire; ROUND26.md,
// "embedfire"; his VR notes 2026-11-26 20:24-20:27).
//
// One firing path (weapons.qc W_TriggerFrame: the charge, the attack, the ammo, the refire, the shot, its effects, sounds
// and messages) for the weapon in a hand and for a weapon prop a hand holds. Today's prop: a blade stuck in a body that the
// hand holds (VR_HOLD_EMBED, vr_embedhold.qc); later others (a weapon on a joint as a turret). The prop keeps its weapon's
// identity (.weapon, .weaponflags, .weaponinst: its variant, its clip). For its trigger frame the hand takes it on
// (VR_PropFire_Begin, as an armed grunt's hand takes its gun on: vr_armedgrunt.qc VR_AG_SetHand): the hand's weapon fields
// are the prop's, its muzzle the prop's model's (its Weapon Offsets' Muzzle anchor), its aim the prop's line (its handle
// to its muzzle), its place the prop's handle (its origin); VR_PropFire_End puts the empty hand back. The player pays (his
// ammo), and the hand's refire and charge (the Lance's .dw_lc*) go on as in the hand, so a charge carries over from the
// hand to the stuck weapon and back out. The shots know the prop (vr_fire_prop): a beam is drawn from it, its flash on it.
// Which weapons: weapons.txt embed_fire (1: Dwell's Crystal Lance held as a lance), under the rule vr_embed_fire.

// (.vr_pfire_shot, on the prop: when its trigger frame last fired it: vr_embedhold.qc, flat play's tap pull.)

float vr_pfire_shots;  // (the tests: shots and clicks fired from props)
float vr_pfire_frames; // (the tests: trigger frames run on props)

// Whether the weapon prop `w` fires where it is (its weapon's embed_fire, vr_embed_fire).
float(entity w) VR_PropFire_Capable =
{
    return w != world && w.weaponinst != world && w.model != string_null && cvar_hget(cvarh_vr_embed_fire) &&
           weaponvalue(w.model, "embed_fire") > 0;
};

// The weapon prop `xHand` of `p` holds and drives (its trigger fires it): a blade stuck in a body that the hand holds by
// its handle role (.vr_hold_handle: the hand that stabbed, or the one left on it), its weapon able to fire there; else world.
entity(entity p, float xHand) VR_PropFire_Held =
{
    if(VR_Hand_Kind(p, xHand) != VR_HOLD_EMBED || !VRIsHandWeaponless(p, xHand))
    {
        return world;
    }
    entity w = VR_Hand_BusyEnt(p, xHand);
    if(!VR_Embed_HeldBy(w, p, xHand) || w.vr_hold_handle != VR_Embed_Bit(xHand) || !VR_PropFire_Capable(w))
    {
        return world;
    }
    return w;
};

// The prop's muzzle (its model's, at its origin and angles; its origin if its model is no weapon's) and its aim: the way
// from its handle to its muzzle (the line a stuck point went in along; a muzzle on its handle: the way it went in).
vector vr_pfire_dir;
vector(entity w) VR_PropFire_Muzzle =
{
    vector m = weapondrawnpose(w, 0, 6);
    vr_pfire_dir = m - w.origin;
    if(vlen(vr_pfire_dir) > 1)
    {
        vr_pfire_dir = normalize(vr_pfire_dir);
    }
    else if(w.vr_stick_dir != '0 0 0')
    {
        vr_pfire_dir = normalize(w.vr_stick_dir);
    }
    else
    {
        makevectors(w.angles);
        vr_pfire_dir = v_forward;
    }
    return m;
};

// The hand's own fields while it fires the prop (VR_PropFire_Begin .. End).
float  vr_pfire_wid;
float  vr_pfire_flags;
entity vr_pfire_inst;
vector vr_pfire_pos;
vector vr_pfire_rot;
vector vr_pfire_shotrot;
vector vr_pfire_muzzle;

void(entity xP, float xHand, vector xPos, vector xRot, vector xShotRot, vector xMuzzle) VR_PropFire_SetHandPlace =
{
    if(xHand == cVR_OffHand)
    {
        xP.offhandpos = xPos;
        xP.offhandrot = xRot;
        xP.offshotrot = xShotRot;
        xP.offmuzzlepos = xMuzzle;
    }
    else
    {
        xP.handpos = xPos;
        xP.handrot = xRot;
        xP.shotrot = xShotRot;
        xP.muzzlepos = xMuzzle;
    }
};

// The hand of self takes the prop `w` on for its trigger frame: its weapon, where it is and where it points.
void(entity w, float xHand) VR_PropFire_Begin =
{
    entity p = self;
    float* wp = VRGetEntWeaponPtr(p, xHand);
    float* fp = VRGetEntWeaponFlagsPtr(p, xHand);
    vr_pfire_wid = VRGetEntWeapon(p, xHand);
    vr_pfire_flags = VRGetEntWeaponFlags(p, xHand);
    vr_pfire_inst = VRGetEntWeaponInst(p, xHand);
    vr_pfire_pos = VRGetHandPos(xHand);
    vr_pfire_rot = VRGetHandRot(xHand);
    vr_pfire_shotrot = xHand == cVR_OffHand ? p.offshotrot : p.shotrot;
    vr_pfire_muzzle = VRGetMuzzlePos(xHand);
    *wp = w.weapon;
    *fp = w.weaponflags;
    VRSetEntWeaponInst(p, xHand, w.weaponinst);
    vector m = VR_PropFire_Muzzle(w);
    vector a = vectoangles(vr_pfire_dir);
    a_x = -a_x; // (a view's pitch, as the hands')
    VR_PropFire_SetHandPlace(p, xHand, w.origin, a, a, m);
    vr_fire_prop = w;
};

// ... and puts the hand back as it was (empty, on the prop).
void(entity w, float xHand) VR_PropFire_End =
{
    entity p = self;
    vr_fire_prop = world;
    *VRGetEntWeaponPtr(p, xHand) = vr_pfire_wid;
    *VRGetEntWeaponFlagsPtr(p, xHand) = vr_pfire_flags;
    VRSetEntWeaponInst(p, xHand, vr_pfire_inst);
    VR_PropFire_SetHandPlace(p, xHand, vr_pfire_pos, vr_pfire_rot, vr_pfire_shotrot, vr_pfire_muzzle);
    W_SetCurrentAmmoFor(xHand);
};

// W_WeaponFrameImpl, each frame for `xHand` of self: TRUE if it holds a weapon prop that fires (VR_PropFire_Held), its
// trigger frame run on it (W_TriggerFrame, as for a weapon in the hand: the player ready, its refire over); FALSE: the
// hand's own weapon frame.
float(float xHand) VR_PropFire_Frame =
{
    entity w = VR_PropFire_Held(self, xHand);
    if(w == world)
    {
        return FALSE;
    }
    if((!deathmatch && time < self.vr_readytime) || time < VRGetAttackFinished(xHand))
    {
        return TRUE;
    }
    float pressed = VRGetFireButtonPressed(xHand); // (the hand is on the prop: no force grab, no reload trip, no throw)
    float was = VRGetAttackFinished(xHand);
    VR_PropFire_Begin(w, xHand);
    W_TriggerFrame(xHand, pressed);
    VR_PropFire_End(w, xHand);
    vr_pfire_frames++;
    if(VRGetAttackFinished(xHand) != was)
    {
        w.vr_pfire_shot = time;
        vr_pfire_shots++;
        if(cvar_hget(cvarh_vr_debug_axestick))
        {
            dprint(sprintf("propfire: %s fired where it is (hand %g, from %v along %v)\n", w.netname, xHand,
                weapondrawnpose(w, 0, 6), vr_pfire_dir));
        }
    }
    return TRUE;
};
