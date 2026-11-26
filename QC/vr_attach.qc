// vr_attach.qc -- weapon attachments (docs/vr-port/ATTACHMENTS.md; the engine's vr_attach.cpp): an attachment (the bayonet,
// vr_bayonet.qc's prop) put on a weapon becomes part of the weapon: its record's .wi_attach (vr_weaponinst.qc), so it
// goes wherever the weapon goes (hands, holsters, drops, throws, embeds, saves, level changes: parm77) and the engine
// draws it there for everyone. Which weapons take which: weapons.txt attN_ok (Weapon Offsets > Attachments, the mount too).
//
// Putting one on: VR, the hand holding it brings its mount (its model's origin) within vr_attach_snap_radius of the gun's
// mount in the other hand, pointing along the gun (within vr_attach_snap_angle), and it snaps on as a magazine seats;
// flat play, the Attach key (vr_flat_key_attach, J: impulse 70) with it in the other hand. Taking it off: VR, the other
// hand, empty, grips it (within vr_attach_pull_radius of the mount) and pulls it off: it is in that hand; flat play, the
// Attach key with that hand empty.
//
// What a bayonet does: the gun strikes as a blade (vr_melee_params.qc VR_Melee_Thing, VR_Melee_Line: a sword's blows, its
// line to the bayonet's tip), sticks by its point (VR_Attach_Embed: weapons.txt embed 1 for this weapon; the engine's
// vr_axestick.cpp attachBlade: the bayonet's tip), and fires while stuck (VR_Attach_EmbedFire: embedfire's prop-fire
// path, vr_propfire.qc). Nothing else of the gun changes: it fires, reloads and breaks open as before, held or stuck.

float vr_att_attached; // (the tests: attachments put on, taken off)
float vr_att_detached;
.float vr_att_grip;    // a player's hands' grips last frame (bit 1 the off hand, 2 the main), for a pull's press

// The attachment kind the prop `e` is (0: not one).
float(entity e) VR_Attach_PropKind =
{
    return (e != world && e.classname == "vr_bayonet") ? VR_ATT_BAYONET : 0;
};

// Whether attachment `xKind` can go on the weapon in `xHand` of `p` now: a weapon it fits (attN_ok), with a record, and
// none on it yet.
float(entity p, float xHand, float xKind) VR_Attach_CanTake =
{
    entity inst = VRGetEntWeaponInst(p, xHand);
    if(xKind <= 0 || inst == world || inst.wi_attach || VRIsHandWeaponless(p, xHand) ||
       (VRGetEntWeaponFlags(p, xHand) & QVR_WPNFLAG_FOREGRIP_CARRIED))
    {
        return FALSE;
    }
    vector fits = attachinfo(WeaponIdToModel(VRGetEntWeapon(p, xHand), VRGetEntWeaponFlags(p, xHand)), xKind, 0);
    return fits_x > 0;
};

// Where point `xWhat` (1 its mount, 2 its tip) of attachment `xKind` on the weapon in `xHand` of `p` is: off the hand's
// muzzle along the hand's axes (the engine's attachinfo; the off hand's model mirrored).
vector(entity p, float xHand, float xKind, float xWhat) VR_Attach_HandPoint =
{
    vector d = attachinfo(WeaponIdToModel(VRGetEntWeapon(p, xHand), VRGetEntWeaponFlags(p, xHand)), xKind, xWhat);
    vector m = xHand == cVR_OffHand ? p.offmuzzlepos : p.muzzlepos;
    if(m == '0 0 0')
    {
        m = VRGetEntHandPos(p, xHand);
    }
    vector oldF = v_forward, oldR = v_right, oldU = v_up;
    makevectors(VRGetEntHandRot(p, xHand));
    float side = xHand == cVR_OffHand ? 1 : -1; // (left: -right; the off hand's mirrored)
    vector w = m + v_forward * d_x + v_right * (d_y * side) + v_up * d_z;
    v_forward = oldF;
    v_right = oldR;
    v_up = oldU;
    return w;
};

// The bayonet's tip on the gun in `xHand` of `p` (VR_Melee_Line's far end with one on).
vector(entity p, float xHand) VR_Attach_TipOf =
{
    return VR_Attach_HandPoint(p, xHand, VR_Attach_Hand(p, xHand), 2);
};

// The prop `xProp` (an attachment, carried) put on the weapon in `xGunHand` of `p`: part of it from now on.
void(entity p, float xGunHand, entity xProp) VR_Attach_Put =
{
    entity inst = VRGetEntWeaponInst(p, xGunHand);
    float kind = VR_Attach_PropKind(xProp);
    if(inst == world || !kind)
    {
        return;
    }
    inst.wi_attach = kind;
    VR_Carry_Forget(xProp);
    remove(xProp);
    sound(p, CHAN_AUTO, "vr/reload_mag_in.wav", 1, ATTN_NORM);
    vr_att_attached = vr_att_attached + 1;
    if(cvar_hget(cvarh_developer)) dprint(sprintf("attach: kind %g on weapon #%g (hand %g)\n", kind, inst.wi_uid, xGunHand));
};

// The attachment on the weapon in the other hand taken off into `xHand` of `p` (empty): a prop again, carried there.
void(entity p, float xHand) VR_Attach_TakeOff =
{
    float g = VRGetOtherHand(xHand);
    entity inst = VRGetEntWeaponInst(p, g);
    float kind = inst != world ? inst.wi_attach : 0;
    if(kind != VR_ATT_BAYONET)
    {
        return;
    }
    vector at = VR_Attach_HandPoint(p, g, kind, 1);
    entity e = spawn();
    setorigin(e, at);
    entity oldSelf = self;
    self = e;
    vr_bayonet(); // (it lies where it is put below: its spawn's drop to the floor undone)
    self = oldSelf;
    vector a = VRGetEntHandRot(p, g);
    a_x = -a_x; // (an entity's pitch)
    e.angles = a;
    setorigin(e, at);
    inst.wi_attach = 0;
    VR_Carry_Start(p, xHand, e);
    sound(p, CHAN_AUTO, "vr/reload_mag_out.wav", 1, ATTN_NORM);
    vr_att_detached = vr_att_detached + 1;
    if(cvar_hget(cvarh_developer)) dprint(sprintf("attach: kind %g off weapon #%g into hand %g\n", kind, inst.wi_uid, xHand));
};

// Every frame, each player (PlayerPostThink): an attachment a hand holds brought to the other hand's gun snaps on; the
// empty hand's grip pressed at the other gun's attachment pulls it off.
void(entity p) VR_Attach_Frame =
{
    for(float h = 0; h < 2; h++)
    {
        float g = VRGetOtherHand(h);
        entity c = VR_Carry_Held(p, h);
        float kind = VR_Attach_PropKind(c);
        if(kind && !c.carry_2h && VR_Attach_CanTake(p, g, kind))
        {
            vector at = VR_Attach_HandPoint(p, g, kind, 1);
            if(vlen(c.origin - at) <= cvar_hget(cvarh_vr_attach_snap_radius))
            {
                // Pointing along the gun (its model's +x: an alias model's pitch is drawn negated).
                vector oldF = v_forward, oldR = v_right, oldU = v_up;
                vector ca = c.angles;
                ca_x = -ca_x;
                makevectors(ca);
                vector cf = v_forward;
                makevectors(VRGetEntHandRot(p, g));
                float along = cf * v_forward;
                v_forward = oldF;
                v_right = oldR;
                v_up = oldU;
                if(along >= cos(cvar_hget(cvarh_vr_attach_snap_angle) * 0.0174533))
                {
                    VR_Attach_Put(p, g, c);
                }
            }
        }
        float bit = h == cVR_MainHand ? 2 : 1;
        float grip = VR_HandGrabUtil_IsHandGrabbing(p, h);
        float was = p.vr_att_grip & bit;
        p.vr_att_grip = grip ? (p.vr_att_grip | bit) : (p.vr_att_grip - (p.vr_att_grip & bit));
        if(grip && !was && VR_Attach_Hand(p, g) && VRIsHandEmpty(p, h) && VR_Carry_Held(p, h) == world &&
           vlen(VRGetEntHandPos(p, h) - VR_Attach_HandPoint(p, g, VR_Attach_Hand(p, g), 1)) <= cvar_hget(cvarh_vr_attach_pull_radius))
        {
            VR_Attach_TakeOff(p, h);
        }
    }
};

// The Attach key (flat play's vr_flat_key_attach, impulse 70; any mode): the attachment one hand holds put on the gun in
// the other (the main hand's gun first), else a gun's attachment taken off into the empty other hand.
void(entity p) VR_Attach_Key =
{
    for(float i = 0; i < 2; i++)
    {
        float g = i == 0 ? cVR_MainHand : cVR_OffHand;
        float o = VRGetOtherHand(g);
        entity c = VR_Carry_Held(p, o);
        float kind = VR_Attach_PropKind(c);
        if(kind && VR_Attach_CanTake(p, g, kind))
        {
            VR_Attach_Put(p, g, c);
            return;
        }
    }
    for(float j = 0; j < 2; j++)
    {
        float gg = j == 0 ? cVR_MainHand : cVR_OffHand;
        float oo = VRGetOtherHand(gg);
        if(VR_Attach_Hand(p, gg) && VRIsHandEmpty(p, oo) && VR_Carry_Held(p, oo) == world)
        {
            VR_Attach_TakeOff(p, oo);
            return;
        }
    }
    if(cvar_hget(cvarh_developer)) dprint("attach: nothing to put on or take off\n");
};

// The carried weapons' records (the hands', the holsters'), in the level parms' order.
entity(entity p, float i) VR_Attach_Carried =
{
    if(i == 0) return p.weaponinst;
    if(i == 1) return p.weaponinst2;
    if(i == 2) return p.holsterweaponinst0;
    if(i == 3) return p.holsterweaponinst1;
    if(i == 4) return p.holsterweaponinst2;
    if(i == 5) return p.holsterweaponinst3;
    if(i == 6) return p.holsterweaponinst4;
    return p.holsterweaponinst5;
};

// SetChangeParms: the carried weapons' attachments, 3 bits each (24 bits: exact in a float): parm77.
float(entity p) VR_Attach_Pack =
{
    float packed = 0, k = 1;
    for(float i = 0; i < 8; i++)
    {
        entity r = VR_Attach_Carried(p, i);
        float kind = r != world ? r.wi_attach : 0;
        packed = packed + min(7, max(0, kind)) * k;
        k = k * 8;
    }
    return packed;
};

// DecodeLevelParms, the records carried: their attachments back.
void(entity p, float xPacked) VR_Attach_Unpack =
{
    for(float i = 0; i < 8; i++)
    {
        float kind = xPacked - floor(xPacked / 8) * 8;
        xPacked = floor(xPacked / 8);
        entity r = VR_Attach_Carried(p, i);
        if(r != world)
        {
            r.wi_attach = kind;
        }
    }
};
