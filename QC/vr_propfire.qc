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
           VR_Attach_EmbedFire(w.model, w.weaponinst) > 0; // (a gun with a bayonet: vr_attach.qc)
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
float  vr_pfire_wframe;

void(entity xP,float xHand, vector xPos, vector xRot, vector xShotRot, vector xMuzzle) VR_PropFire_SetHandPlace =
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

// The hand of self takes the prop `w` on for its trigger frame (or its firing frames' think, or a gun's own frame: its
// burst, its hammer; VR_PropFire_With): its weapon, its frame (the prop's model's, drawn on it: the nailgun's barrels, the
// shots' and the thunderbolt's frames), where it is and where it points.
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
    vr_pfire_wframe = VRGetWeaponFrame(xHand);
    *wp = w.weapon;
    *fp = w.weaponflags;
    VRSetEntWeaponInst(p, xHand, w.weaponinst);
    VRSetWeaponFrame(xHand, w.frame);
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
    w.weaponflags = VRGetEntWeaponFlags(p, xHand); // (what the frame did to it stays with it: a super shotgun's spent shells, opened)
    w.frame = VRGetWeaponFrame(xHand);             // (and its frame: drawn on the prop)
    *VRGetEntWeaponPtr(p, xHand) = vr_pfire_wid;
    *VRGetEntWeaponFlagsPtr(p, xHand) = vr_pfire_flags;
    VRSetEntWeaponInst(p, xHand, vr_pfire_inst);
    VRSetWeaponFrame(xHand, vr_pfire_wframe);
    VR_PropFire_SetHandPlace(p, xHand, vr_pfire_pos, vr_pfire_rot, vr_pfire_shotrot, vr_pfire_muzzle);
    // The hand's ammo mirror and the HUD's counters back (the prop's while the hand drives it: VR_PropFire_HudCounter). Not
    // W_SetCurrentAmmoFor: its player_run ended the firing frames' think every frame (embedfire2, his flat note 2026-11-28:
    // the stuck nailgun fired slower, one barrel only, the thunderbolt's start sound again and again).
    if(xHand == cVR_OffHand)
    {
        p.currentammo2 = WeaponIdToAmmoId(vr_pfire_wid, vr_pfire_flags);
    }
    else
    {
        p.currentammo = WeaponIdToAmmoId(vr_pfire_wid, vr_pfire_flags);
    }
    updateAmmoCounters();
};

// `xFn` for `xHand` of self, run on the weapon prop the hand drives (VR_PropFire_Held: taken on, VR_PropFire_Begin .. End)
// if it drives one, else on the hand's own weapon: the firing frames' think (player.qc doThinkArgFn), a gun's own frame (the
// revolver's hammer, the burst rifle's burst), so the stuck gun fires as in the hand (embedfire2).
void(float xHand, void(float) xFn) VR_PropFire_With =
{
    entity w = (self.flags & FL_CLIENT) ? VR_PropFire_Held(self, xHand) : world;
    if(w == world)
    {
        xFn(xHand);
        return;
    }
    VR_PropFire_Begin(w, xHand);
    xFn(xHand);
    VR_PropFire_End(w, xHand);
};

// The HUD (his flat note 2026-11-27: "when the shotgun is in the embedded state, the HUD stops displaying the ammo count"):
// a hand empty on a weapon prop it drives shows the prop's ammo, as if the prop were in it. updateAmmoCounter, for `xHand`
// of self: TRUE if the hand drives one (VR_PropFire_Held), its counter and clip size then the prop's (the hand takes the
// prop's weapon on for it, as for its trigger frame) and the ammo type the engine shows (.currentammo / 2) its. The engine
// sends the prop's weapon id (.vr_pfire_hud: STAT_QVR_PROPHUD) and its record's clip; every readout (the status bars, the
// flat visor, the wrist gadget) follows them (flathud::hudWeapon). The hand's own weapon stays the fist.
float(float xHand) VR_PropFire_HudCounter =
{
    entity w = VR_PropFire_Held(self, xHand); // (world while the hand has the prop on: not weaponless)
    if(w == world)
    {
        return FALSE;
    }
    float* wp = VRGetEntWeaponPtr(self, xHand);
    float* fp = VRGetEntWeaponFlagsPtr(self, xHand);
    float wid = VRGetEntWeapon(self, xHand);
    float flags = VRGetEntWeaponFlags(self, xHand);
    entity inst = VRGetEntWeaponInst(self, xHand);
    float aid = WeaponIdToAmmoId(w.weapon, w.weaponflags);
    *wp = w.weapon;
    *fp = w.weaponflags;
    VRSetEntWeaponInst(self, xHand, w.weaponinst);
    updateAmmoCounter(xHand, aid);
    *wp = wid;
    *fp = flags;
    VRSetEntWeaponInst(self, xHand, inst);
    if(xHand == cVR_OffHand)
    {
        self.currentammo2 = aid;
    }
    else
    {
        self.currentammo = aid;
    }
    return TRUE;
};

// Each PlayerPreThink (VR_Hand_Derive): the props the hands drive (.vr_pfire_hud, the engine's STAT_QVR_PROPHUD); when that
// changes, the counters again (a hand let go of one: its own weapon's mirrors back, W_SetCurrentAmmoImpl's).
void(entity p) VR_PropFire_HudFrame =
{
    entity o = VR_PropFire_Held(p, cVR_OffHand);
    entity m = VR_PropFire_Held(p, cVR_MainHand);
    float bits = (o != world ? o.weapon : 0) + 256 * (m != world ? m.weapon : 0);
    if(bits == p.vr_pfire_hud)
    {
        return;
    }
    p.vr_pfire_hud = bits;
    entity oldSelf = self;
    self = p;
    self.currentammo = VRGetCurrentAmmo(cVR_MainHand);
    self.currentammo2 = VRGetCurrentAmmo(cVR_OffHand);
    updateAmmoCounters();
    self = oldSelf;
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
    float was = VRGetAttackFinished(xHand);
    VR_PropFire_Begin(w, xHand);
    W_HandFireFrame(xHand, TRUE); // (as in the hand: the stream drawn, the spin, the refire, the trigger)
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
