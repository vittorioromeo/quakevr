//
//
//
// ----------------------------------------------------------------------------
// Magnetic holsters (as RE4VR's; ROUND22.md, "Magnetic holsters")
// ----------------------------------------------------------------------------
//
// A hand at a holster with a weapon in it presses the bind button (vr_magholster_button: the engine takes the press and
// hands it over, magholsterpoll): that weapon is bound to that holster (its record's id, vr_weaponinst.qc, in
// .vr_magbind<slot>); pressed again, unbound. One weapon a holster, one holster a weapon (binding it elsewhere moves the
// binding). Thrown or dropped (a prop: thrown_weapon), once it has been out of the hands vr_magholster_return_time
// seconds or lies vr_magholster_return_distance metres from its holster (either; 0 turns one off), it flies back into
// it through the air, through walls, over vr_magholster_flight_time seconds and snaps in with a click. A hand closing on
// it on the way takes it (the flight ends: it is caught as a thrown weapon is). The holster taken meanwhile (another
// weapon put in it): it waits where it lies (or drops where its flight got to) and comes back the moment the holster is
// empty again. In a hand, or in a holster (its own or another), it stays. Weapon Mode Immersive only (quick slots'
// holsters keep copies). The client draws the binding as a chain (vr_magholster.cpp).
//
// The bindings are the player's fields (saved with the game). A level change brings the bound weapons lying about
// home first (when their holsters are empty), then carries the bindings to the weapons as carried (parm73: each
// holster's bound weapon's place, 4 bits: 1..6 a holster, 7 the main hand, 8 the off hand; 0 none).

.float vr_magbind0; // each holster's bound weapon's id (0: none; slot order: the holsterweapon<slot> fields')
.float vr_magbind1;
.float vr_magbind2;
.float vr_magbind3;
.float vr_magbind4;
.float vr_magbind5;
.float vr_mag_flying; // the slots whose weapon is on its way home (bit per slot): a catch on the way is told

.float vr_mag_since;  // a bound weapon's prop: when it was first seen out of the hands (the return's clock)
.float vr_mag_fly;    // when its flight home began (0: not flying)
.vector vr_mag_from;  // where from

float(entity p, float s) VR_MagHolster_Bound =
{
    if(s == 0) { return p.vr_magbind0; }
    if(s == 1) { return p.vr_magbind1; }
    if(s == 2) { return p.vr_magbind2; }
    if(s == 3) { return p.vr_magbind3; }
    if(s == 4) { return p.vr_magbind4; }
    if(s == 5) { return p.vr_magbind5; }
    return 0;
};

void(entity p, float s, float v) VR_MagHolster_SetBound =
{
    if(s == 0) { p.vr_magbind0 = v; }
    if(s == 1) { p.vr_magbind1 = v; }
    if(s == 2) { p.vr_magbind2 = v; }
    if(s == 3) { p.vr_magbind3 = v; }
    if(s == 4) { p.vr_magbind4 = v; }
    if(s == 5) { p.vr_magbind5 = v; }
};

// Slot 0..5 as its holster's hotspot (QVR_HS_*: 3..6, 8, 9).
float(float s) VR_MagHolster_Hotspot =
{
    return s < 4 ? s + QVR_HS_LEFT_SHOULDER_HOLSTER : s + QVR_HS_LEFT_SHOULDER_HOLSTER + 1;
};

float(float s) VR_MagHolster_Bit =
{
    return s == 0 ? 1 : (s == 1 ? 2 : (s == 2 ? 4 : (s == 3 ? 8 : (s == 4 ? 16 : 32))));
};

// Where `p`'s holster `s` is (the local player's body's; else the grappling hook's estimate).
vector(entity p, float s) VR_MagHolster_Home =
{
    vector at = holsterpos(p, s);
    if(at == '0 0 0')
    {
        at = VR_Grapple_HolsterPos(p, VR_MagHolster_Hotspot(s));
    }
    return at;
};

// The place of the weapon with id `uid` among `p`'s (1..6 holster slot + 1, 7 the main hand, 8 the off hand; 0 none).
float(entity p, float uid) VR_MagHolster_Carried =
{
    if(!uid) { return 0; }
    if(p.holsterweaponinst0.wi_uid == uid) { return 1; }
    if(p.holsterweaponinst1.wi_uid == uid) { return 2; }
    if(p.holsterweaponinst2.wi_uid == uid) { return 3; }
    if(p.holsterweaponinst3.wi_uid == uid) { return 4; }
    if(p.holsterweaponinst4.wi_uid == uid) { return 5; }
    if(p.holsterweaponinst5.wi_uid == uid) { return 6; }
    if(p.weaponinst.wi_uid == uid) { return 7; }
    if(p.weaponinst2.wi_uid == uid) { return 8; }
    return 0;
};

// The weapon with id `uid` lying about (or flying home), else world.
entity(float uid) VR_MagHolster_Prop =
{
    for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
    {
        if(e.model != string_null && e.weaponinst && e.weaponinst.wi_uid == uid)
        {
            return e;
        }
    }
    return world;
};

void() VR_MagHolster_FlyThink =
{
    self.nextthink = time + 0.02; // (the player's frame flies it: VR_MagHolster_Fly)
};

// Its flight over: back to lying about where it got to (the holster taken meanwhile).
void(entity e) VR_MagHolster_Drop =
{
    e.vr_mag_fly = 0;
    e.movetype = MOVETYPE_TOSS;
    e.vr_rigid = 1;
    e.velocity = '0 0 0';
    e.think = wpnthrow_think;
    e.nextthink = time + 0.02;
    e.touch = wpnthrow_touch;
};

// `e` into `self`'s holster `s` (empty): its record with it (its magazine, its id), the prop gone; a click.
void(entity e, float s) VR_MagHolster_Arrive =
{
    float hs = VR_MagHolster_Hotspot(s);
    VR_Saw_PropStop(e); // (a chainsaw lying about running: stopped)
    float flags = e.weaponflags - (e.weaponflags & QVR_WPNFLAG_FOREGRIP_CARRIED);
    flags = flags - (flags & QVR_WPNFLAG_SAW_STATE);
    setHolsterWeapon(hs, e.weapon);
    setHolsterWeaponModel(hs, WeaponIdToModel(e.weapon, flags));
    setHolsterWeaponFlags(hs, flags);
    VRSetEntHolsterWeaponInst(self, hs, e.weaponinst ? e.weaponinst : WeaponInst_New(0));
    VR_SoundVaried(self, CHAN_AUTO, "weapons/holster1.wav", 1, ATTN_NORM);
    dprint(sprintf("magholster: weapon %g (%s) home in holster %g after %.2f s of flight\n", e.weaponinst.wi_uid, e.netname, s,
        e.vr_mag_fly ? time - e.vr_mag_fly : 0));
    self.vr_mag_flying = self.vr_mag_flying - (self.vr_mag_flying & VR_MagHolster_Bit(s));
    remove(e);
    W_SetCurrentAmmo();
};

// `e` sets off home to `self`'s holster `s`: no longer a rigid body; it flies through everything (the player's frame
// steers it: VR_MagHolster_Fly), still a thrown weapon a hand can catch.
void(entity e, float s, string why) VR_MagHolster_Launch =
{
    if(VR_AxeStick_IsStuck(e))
    {
        VR_AxeStick_PulledOut(e); // (out of what it was stuck in)
    }
    e.vr_mag_fly = time;
    e.vr_mag_from = e.origin;
    e.vr_rigid = 0;
    e.movetype = MOVETYPE_NOCLIP;
    e.flags = e.flags - (e.flags & FL_ONGROUND);
    e.velocity = '0 0 0';
    e.avelocity = '0 0 0';
    e.vr_spin = '0 0 0';
    e.think = VR_MagHolster_FlyThink;
    e.nextthink = time + 0.02;
    e.touch = SUB_Null; // (it hurts nothing on the way, you least of all; a hand still catches it: its handtouch)
    e.throwhit = QVR_THROWHIT_HIT;
    self.vr_mag_flying = self.vr_mag_flying | VR_MagHolster_Bit(s);
    dprint(sprintf("magholster: weapon %g (%s) flies home to holster %g (%s), %.2f m away, %.2f s out of the hands\n",
        e.weaponinst.wi_uid, e.netname, s, why, vlen(e.origin - VR_MagHolster_Home(self, s)) / VRMetersToUnits(),
        time - e.vr_mag_since));
};

// Its place `t` (0..1) of the way home: eased in and out, over a low arc (a quarter of the way up, at most half a metre).
vector(entity e, vector xHome, float t) VR_MagHolster_FlyPoint =
{
    float k = t * t * (3 - 2 * t);
    float lift = min(0.5 * VRMetersToUnits(), 0.25 * vlen(xHome - e.vr_mag_from));
    return e.vr_mag_from + (xHome - e.vr_mag_from) * k + '0 0 1' * (4 * t * (1 - t) * lift);
};

// Each frame of its flight: its velocity takes it where it should be next frame (the holster moves with you).
void(entity e, float s) VR_MagHolster_Fly =
{
    float flight = max(0.05, cvar_hget(cvarh_vr_magholster_flight_time));
    float t = (time - e.vr_mag_fly) / flight;
    if(t >= 1)
    {
        if(getHolsterWeapon(VR_MagHolster_Hotspot(s)) == WID_FIST)
        {
            VR_MagHolster_Arrive(e, s);
        }
        else
        {
            dprint(sprintf("magholster: holster %g taken meanwhile: weapon %g drops where it got to\n", s, e.weaponinst.wi_uid));
            self.vr_mag_flying = self.vr_mag_flying - (self.vr_mag_flying & VR_MagHolster_Bit(s));
            VR_MagHolster_Drop(e);
        }
        return;
    }
    float dt = max(frametime, 0.001);
    vector next = VR_MagHolster_FlyPoint(e, VR_MagHolster_Home(self, s), min(1, t + dt / flight));
    e.velocity = (next - e.origin) * (1 / dt);
    e.angles_z = e.angles_z * 0.85; // (levels out on the way)
};

// A hand's bind button at a holster: binds its weapon there, or unbinds what is bound there.
void(float xHand) VR_MagHolster_Press =
{
    float hs = xHand == cVR_MainHand ? self.mainhand_hotspot : self.offhand_hotspot;
    if(!isHolsterHotspot(hs))
    {
        return;
    }
    if(getWeaponMode() != QVR_WEAPONMODE_IMMERSIVE)
    {
        sprint(self, "Magnetic holsters need Weapon Mode Immersive.\n");
        return;
    }
    float s = VRHolsterToIndex(hs);
    float uid = VR_MagHolster_Bound(self, s);
    if(uid)
    {
        VR_MagHolster_SetBound(self, s, 0);
        VR_SoundVaried(self, VRGetHandChannel(xHand), "vr/flashlight_detach.wav", 0.8, ATTN_NORM);
        haptic(xHand, 0, 0.06, 90, 0.6);
        dprint(sprintf("magholster: holster %g unbound (weapon %g)\n", s, uid));
        return;
    }
    if(getHolsterWeapon(hs) == WID_FIST)
    {
        VRHolsterHapticBuzz(self, xHand, hs); // (nothing in it to bind)
        return;
    }
    entity inst = WeaponInst_Ensure(VRGetEntHolsterWeaponInst(self, hs), getHolsterWeapon(hs), 0);
    VRSetEntHolsterWeaponInst(self, hs, inst);
    for(float k = 0; k < 6; k++)
    {
        if(VR_MagHolster_Bound(self, k) == inst.wi_uid)
        {
            VR_MagHolster_SetBound(self, k, 0); // (one holster a weapon: bound here now)
        }
    }
    VR_MagHolster_SetBound(self, s, inst.wi_uid);
    VR_SoundVaried(self, VRGetHandChannel(xHand), "vr/flashlight_attach.wav", 1, ATTN_NORM);
    haptic(xHand, 0, 0.05, 120, 0.8);
    haptic(xHand, 0.09, 0.05, 160, 0.5);
    dprint(sprintf("magholster: weapon %g bound to holster %g by the %s hand\n", inst.wi_uid, s,
        xHand == cVR_MainHand ? "main" : "off"));
};

// Holster `s`'s bound weapon this frame: kept on the body, or out of the hands, timed, sent home, flown.
void(float s, float xNow) VR_MagHolster_Watch =
{
    float uid = VR_MagHolster_Bound(self, s);
    if(!uid)
    {
        return;
    }
    float bit = VR_MagHolster_Bit(s);
    float at = VR_MagHolster_Carried(self, uid);
    if(at)
    {
        if(self.vr_mag_flying & bit)
        {
            self.vr_mag_flying = self.vr_mag_flying - bit;
            dprint(sprintf("magholster: weapon %g caught on its way home (%s)\n", uid,
                at == 7 ? "the main hand" : (at == 8 ? "the off hand" : "a holster")));
        }
        return; // (held, or in a holster: on the body)
    }
    entity e = VR_MagHolster_Prop(uid);
    if(!e)
    {
        if(!WeaponInst_Find(uid))
        {
            VR_MagHolster_SetBound(self, s, 0); // (the weapon is gone: used up, discarded, destroyed)
            self.vr_mag_flying = self.vr_mag_flying - (self.vr_mag_flying & bit);
            dprint(sprintf("magholster: weapon %g is gone: holster %g unbound\n", uid, s));
        }
        return; // (somewhere else: in another's hands)
    }
    if(e.vr_mag_fly)
    {
        VR_MagHolster_Fly(e, s);
        return;
    }
    if(!e.vr_mag_since)
    {
        e.vr_mag_since = time;
    }
    if(e.fg_state || getHolsterWeapon(VR_MagHolster_Hotspot(s)) != WID_FIST)
    {
        return; // (a force grab pulls it; the holster is taken: it waits)
    }
    float after = cvar_hget(cvarh_vr_magholster_return_time);
    float beyond = cvar_hget(cvarh_vr_magholster_return_distance) * VRMetersToUnits();
    if(xNow)
    {
        VR_MagHolster_Launch(e, s, "called");
    }
    else if(after > 0 && time - e.vr_mag_since >= after)
    {
        VR_MagHolster_Launch(e, s, "time");
    }
    else if(beyond > 0 && vlen(e.origin - VR_MagHolster_Home(self, s)) >= beyond)
    {
        VR_MagHolster_Launch(e, s, "distance");
    }
};

// Once a frame, the player's (W_Frame).
void() VR_MagHolster_Frame =
{
    float press = magholsterpoll(self);
    if(!cvar_hget(cvarh_vr_magholster) || getWeaponMode() != QVR_WEAPONMODE_IMMERSIVE || self.health <= 0)
    {
        return; // (the bindings kept; nothing comes back)
    }
    if(press & 1) { VR_MagHolster_Press(cVR_OffHand); }
    if(press & 2) { VR_MagHolster_Press(cVR_MainHand); }
    if(!(self.vr_magbind0 || self.vr_magbind1 || self.vr_magbind2 || self.vr_magbind3 || self.vr_magbind4 || self.vr_magbind5))
    {
        return;
    }
    for(float s = 0; s < 6; s++)
    {
        VR_MagHolster_Watch(s, press & 4);
    }
};

// SetChangeParms (self: the player): the bound weapons lying about home (their holsters empty), then the bindings.
void() VR_MagHolster_EncodeParms =
{
    parm73 = 0;
    if(self.health <= 0)
    {
        return;
    }
    float k = 1;
    for(float s = 0; s < 6; s++)
    {
        float uid = VR_MagHolster_Bound(self, s);
        if(uid && !VR_MagHolster_Carried(self, uid) && cvar_hget(cvarh_vr_magholster) &&
           getHolsterWeapon(VR_MagHolster_Hotspot(s)) == WID_FIST)
        {
            entity e = VR_MagHolster_Prop(uid);
            if(e)
            {
                VR_MagHolster_Arrive(e, s);
            }
        }
        parm73 = parm73 + VR_MagHolster_Carried(self, uid) * k;
        k = k * 16;
    }
};

// DecodeLevelParms (self: the player, the carried weapons' records made): the bindings to them.
void() VR_MagHolster_DecodeParms =
{
    float v = parm73;
    self.vr_mag_flying = 0;
    for(float s = 0; s < 6; s++)
    {
        float at = v - floor(v / 16) * 16;
        v = floor(v / 16);
        entity inst = world;
        if(at == 1) { inst = self.holsterweaponinst0; }
        if(at == 2) { inst = self.holsterweaponinst1; }
        if(at == 3) { inst = self.holsterweaponinst2; }
        if(at == 4) { inst = self.holsterweaponinst3; }
        if(at == 5) { inst = self.holsterweaponinst4; }
        if(at == 6) { inst = self.holsterweaponinst5; }
        if(at == 7) { inst = self.weaponinst; }
        if(at == 8) { inst = self.weaponinst2; }
        VR_MagHolster_SetBound(self, s, inst.wi_uid);
    }
};
