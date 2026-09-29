//
//
//
// ----------------------------------------------------------------------------
// VR Utilities Implementation Details
// ----------------------------------------------------------------------------

// The per-hand getters run many times a frame (the QC VM interprets every statement, and a call
// costs several), so they are branchless selects that fteqcc inlines (`inline`: its inliner takes
// only branch-free bodies): the off hand's (cVR_OffHand, 0) value for 0, the main hand's for any
// other (as the old if/else did). Both fields are read; they are finite, so `a * 1 + b * 0` is `a`.
#define VR_IMPL_SELECT(xHand, xOff, xMain) ((xMain) * (xHand != cVR_OffHand) + (xOff) * (xHand == cVR_OffHand))

#define VR_IMPL_MAKE_VECTOR_GETTER(xFnName, xOff, xMain) \
    inline vector(float xHand) xFnName = \
    { \
        return VR_IMPL_SELECT(xHand, xOff, xMain); \
    }

#define VR_IMPL_MAKE_FLOAT_GETTER(xFnName, xOff, xMain) \
    inline float(float xHand) xFnName = \
    { \
        return VR_IMPL_SELECT(xHand, xOff, xMain); \
    }

#define VR_IMPL_MAKE_FLOAT_SETTER(xFnName, xOff, xMain) \
    void(float xHand, float xValue) xFnName = \
    { \
        if(xHand == cVR_OffHand) *xOff = xValue; \
        else                     *xMain = xValue; \
    }

// Whether a hotspot is a holster (QVR_HS_LEFT_SHOULDER_HOLSTER 3 .. QVR_HS_RIGHT_UPPER_HOLSTER 9,
// but for QVR_HS_HAND_SWITCH 7).
inline float(float xHotspot) isHolsterHotspot =
{
    return (xHotspot >= QVR_HS_LEFT_SHOULDER_HOLSTER) * (xHotspot <= QVR_HS_RIGHT_UPPER_HOLSTER) *
           (xHotspot != QVR_HS_HAND_SWITCH);
}

//
//
//
// ----------------------------------------------------------------------------
// VR Getters
// ----------------------------------------------------------------------------

VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandPos,
                           self.offhandpos,
                           self.handpos);

VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandRot,
                           self.offhandrot,
                           self.handrot);

VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandVel,
                           self.offhandvel,
                           self.handvel);

VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandThrowVel,
                           self.offhandthrowvel,
                           self.handthrowvel);

VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandAVel,
                           self.offhandavel,
                           self.handavel);

VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandThrowPos,
                           self.offhandthrowpos,
                           self.handthrowpos);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetHandThrowAge,
                          self.offhandthrowage,
                          self.handthrowage);

VR_IMPL_MAKE_VECTOR_GETTER(VRGetMuzzlePos,
                           self.offmuzzlepos,
                           self.muzzlepos);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetHandVelMag,
                          self.offhandvelmag,
                          self.handvelmag);

// Where a hand's shots, projectiles and beams go: its aim turned by the weapon's Shot Pitch and Yaw (the client's
// weapons::shotAngles). The hand's aim for a player the engine gives none (a bot: its shotrot stays 0 0 0).
vector(float xHand) VRGetWeaponFireRot =
{
    vector r = VR_IMPL_SELECT(xHand, self.offshotrot, self.shotrot);
    if(r == '0 0 0')
    {
        return VRGetHandRot(xHand);
    }
    return r;
}

vector(float xHand) VRGetWeaponFireRotFwd =
{
    makeforward(VRGetWeaponFireRot(xHand));
    return v_forward;
}

// The forward vector of `xAngles`, leaving v_forward (and v_right, v_up) as they were: for code
// that runs while a caller still needs them.
vector(vector xAngles) VRForwardOf =
{
    vector oldForward = v_forward;
    makeforward(xAngles);
    vector forward = v_forward;
    v_forward = oldForward;
    return forward;
}

// TODO VR: (P2) move:

typedef void() NullaryFn;
typedef void(entity, float) PainFn;

PainFn(NullaryFn fn) asPainFn =
{
    return fn;
}

NullaryFn(PainFn fn) fromPainToTouchFn =
{
    return fn;
}

//
//
//
// ----------------------------------------------------------------------------
// TODO VR: (P2) refactor like above
// ----------------------------------------------------------------------------

VR_IMPL_MAKE_FLOAT_GETTER(VRGetMeleeHitSoundPlayed,
                          self.offhand_melee_hit_sound_played,
                          self.melee_hit_sound_played);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetMeleeHitSoundPlayed,
                          &self.offhand_melee_hit_sound_played,
                          &self.melee_hit_sound_played);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetGunChannel,
                          CHAN_WEAPON2,
                          CHAN_WEAPON);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetFireButtonPressed,
                          self.button3,
                          self.button0);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetCurrentAmmo,
                          self.currentammo2,
                          self.currentammo);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetAttackFinished,
                          self.offhand_attack_finished,
                          self.attack_finished);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetAttackFinished,
                          &self.offhand_attack_finished,
                          &self.attack_finished);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetWeaponFrame,
                          self.weaponframe2,
                          self.weaponframe);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetWeaponFrame,
                          &self.weaponframe2,
                          &self.weaponframe);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetAmmoCounter,
                          &self.ammocounter2,
                          &self.ammocounter);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetWeaponFlags,
                          self.weaponflags2,
                          self.weaponflags);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetWeaponFlags,
                          &self.weaponflags2,
                          &self.weaponflags);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetWeaponClip,
                          self.weaponclip2,
                          self.weaponclip);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetWeaponClip,
                          &self.weaponclip2,
                          &self.weaponclip);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetWeaponClipSize,
                          &self.weaponclipsize2,
                          &self.weaponclipsize);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetLavaGunFired,
                          self.offHandLavaGunFired,
                          self.mainHandLavaGunFired);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetLavaGunFired,
                          &self.offHandLavaGunFired,
                          &self.mainHandLavaGunFired);

// Whether `e` plays with tracked VR hands (not a flat-screen player, not a bot): the engine tells,
// per client.
inline float(entity e) VRHandsTracked =
{
    return !!(e.vrbits0 & QVR_VRBITS0_HANDSTRACKED);
}

inline float(entity e, float xHand) VRGetEntFireButtonPressed =
{
    return VR_IMPL_SELECT(xHand, e.button3, e.button0);
}

// The hand's upper face button held (B on the main hand, Y on the off hand; the engine's key, in the game): the
// grappling hook's reel (vr_grapple.qc).
inline float(entity e, float xHand) VRGetEntSecondaryPressed =
{
    return !!(e.vrbits0 & VR_IMPL_SELECT(xHand, QVR_VRBITS0_OFFHAND_SECONDARY, QVR_VRBITS0_MAINHAND_SECONDARY));
}

// The hand's lower face button held (A on the main hand, X on the off hand; its key, jump or reload, pressed as ever):
// the grappling hook's unreel (vr_grapple.qc).
inline float(entity e, float xHand) VRGetEntPrimaryPressed =
{
    return !!(e.vrbits0 & VR_IMPL_SELECT(xHand, QVR_VRBITS0_OFFHAND_PRIMARY, QVR_VRBITS0_MAINHAND_PRIMARY));
}

// The head (between the eyes): the client's, which leans away from the origin; the old eye point if
// the client sends none.
vector(entity e) VRGetHeadPos =
{
    return e.headpos != '0 0 0' ? e.headpos : e.origin + e.view_ofs;
};

inline vector(entity e, float xHand) VRGetEntHandPos =
{
    return VR_IMPL_SELECT(xHand, e.offhandpos, e.handpos);
}

inline vector(entity e, float xHand) VRGetEntHandRot =
{
    return VR_IMPL_SELECT(xHand, e.offhandrot, e.handrot);
}

// The hand's velocity, in metres per second.
inline vector(entity e, float xHand) VRGetEntHandVel =
{
    return VR_IMPL_SELECT(xHand, e.offhandvel, e.handvel);
}

float(float xHand) VRIsAmmolessWeapon =
{
    return VRGetCurrentAmmo(xHand) == AID_NONE;
}

inline float(float xHand) VRGetOtherHand =
{
    return VR_IMPL_SELECT(xHand, cVR_MainHand, cVR_OffHand);
}

void(entity e, float xHand, float v) VRSetEntWeapon =
{
    if(xHand == cVR_OffHand) e.weapon2 = v;
    else                     e.weapon = v;
}

void(entity e, float xHand, float v) VRSetEntWeaponFlags =
{
    if(xHand == cVR_OffHand) e.weaponflags2 = v;
    else                     e.weaponflags = v;
}

void(entity e, float xHand, float v) VRSetEntWeaponClip =
{
    if(xHand == cVR_OffHand) e.weaponclip2 = v;
    else                     e.weaponclip = v;
}

void(float xHand, float v) VRSetWeapon =
{
    VRSetEntWeapon(self, xHand, v);
}

inline float(entity e, float xHand) VRGetEntWeapon =
{
    return VR_IMPL_SELECT(xHand, e.weapon2, e.weapon);
}

inline float(entity e, float xHand) VRGetEntWeaponFlags =
{
    return VR_IMPL_SELECT(xHand, e.weaponflags2, e.weaponflags);
}

inline float(entity e, float xHand) VRGetEntWeaponClip =
{
    return VR_IMPL_SELECT(xHand, e.weaponclip2, e.weaponclip);
}

float*(entity e, float xHand) VRGetEntWeaponPtr =
{
    if(xHand == cVR_OffHand) return &e.weapon2;
    else                     return &e.weapon;
}

float*(entity e, float xHand) VRGetEntWeaponFlagsPtr =
{
    if(xHand == cVR_OffHand) return &e.weaponflags2;
    else                     return &e.weaponflags;
}

float*(entity e, float xHand) VRGetEntWeaponClipPtr =
{
    if(xHand == cVR_OffHand) return &e.weaponclip2;
    else                     return &e.weaponclip;
}

inline float(entity e, float xHand) VRIsHandEmpty =
{
    return VR_IMPL_SELECT(xHand, e.weapon2, e.weapon) == WID_FIST;
}

// Whether `xHand` carries a gun by its foregrip (the hand-off).
inline float(entity e, float xHand) VRIsHandCarryingByForegrip =
{
    return (VR_IMPL_SELECT(xHand, e.weapon2, e.weapon) != WID_FIST) *
           !!(VR_IMPL_SELECT(xHand, e.weaponflags2, e.weaponflags) & QVR_WPNFLAG_FOREGRIP_CARRIED);
}

void(entity e, float xHand) VRSetHandEmpty =
{
    VRSetEntWeapon(e, xHand, WID_FIST);
    VRSetEntWeaponFlags(e, xHand, 0);
    VRSetEntWeaponClip(e, xHand, 0);
}

inline float(float xHand) VRGetWeapon =
{
    return VR_IMPL_SELECT(xHand, self.weapon2, self.weapon);
}

inline float(entity e, float xHand) VRIsEntHandGrabbingBit =
{
    return VR_IMPL_SELECT(xHand, !!(e.vrbits0 & QVR_VRBITS0_OFFHAND_GRABBING), !!(e.vrbits0 & QVR_VRBITS0_MAINHAND_GRABBING));
}

inline float(entity e, float xHand) VRIsEntHandPrevGrabbingBit =
{
    return VR_IMPL_SELECT(xHand, !!(e.vrbits0 & QVR_VRBITS0_OFFHAND_PREVGRABBING), !!(e.vrbits0 & QVR_VRBITS0_MAINHAND_PREVGRABBING));
}

inline float(entity e, float xHand) VRIsEntHandReloadingBit =
{
    return VR_IMPL_SELECT(xHand, !!(e.vrbits0 & QVR_VRBITS0_OFFHAND_RELOADING), !!(e.vrbits0 & QVR_VRBITS0_MAINHAND_RELOADING));
}

inline float(entity e, float xHand) VRIsEntHandReloadFlickingBit =
{
    return VR_IMPL_SELECT(xHand, !!(e.vrbits0 & QVR_VRBITS0_OFFHAND_RELOADFLICKING), !!(e.vrbits0 & QVR_VRBITS0_MAINHAND_RELOADFLICKING));
}

float(entity entPlayer) VRIsTouchingHandFake =
{
    return entPlayer.touchinghand == cVR_FakeHand;
}

float(entity entPlayer) VRGetTouchingHand =
{
    if(VRIsTouchingHandFake(entPlayer))
    {
        return cVR_MainHand;
    }

    return entPlayer.touchinghand;
}

float(entity entTarget) VRIsHandtouchHandFake =
{
    return entTarget.handtouch_hand == cVR_FakeHand;
}

float(entity entTarget) VRGetHandtouchHand =
{
    if(VRIsHandtouchHandFake(entTarget))
    {
        return cVR_MainHand;
    }

    return entTarget.handtouch_hand;
}

// TODO VR: (P2) organize bits

inline float(float flags, float bit) VRHasBit =
{
    return !!(flags & bit);
}

float(float flags, float bit, float value) VRSetBit =
{
    if(value)
    {
        return flags | bit;
    }
    else
    {
        return flags & ~bit;
    }
}

inline float(entity ent) VRGetOffHandGrabbingBit =
{
    return !!(ent.vrbits0 & QVR_VRBITS0_OFFHAND_GRABBING);
}

inline float(entity ent) VRGetOffHandPrevGrabbingBit =
{
    return !!(ent.vrbits0 & QVR_VRBITS0_OFFHAND_PREVGRABBING);
}

inline float(entity ent) VRGetMainHandGrabbingBit =
{
    return !!(ent.vrbits0 & QVR_VRBITS0_MAINHAND_GRABBING);
}

inline float(entity ent) VRGetMainHandPrevGrabbingBit =
{
    return !!(ent.vrbits0 & QVR_VRBITS0_MAINHAND_PREVGRABBING);
}

inline float(entity ent) VRGet2HAiming =
{
    return !!(ent.vrbits0 & QVR_VRBITS0_2H_AIMING);
}

inline float(entity ent) VRGetOffHandReloadingBit =
{
    return !!(ent.vrbits0 & QVR_VRBITS0_OFFHAND_RELOADING);
}

inline float(entity ent) VRGetMainHandReloadingBit =
{
    return !!(ent.vrbits0 & QVR_VRBITS0_MAINHAND_RELOADING);
}

inline float(entity ent) VRGetOffHandReloadFlickingBit =
{
    return !!(ent.vrbits0 & QVR_VRBITS0_OFFHAND_RELOADFLICKING);
}

inline float(entity ent) VRGetMainHandReloadFlickingBit =
{
    return !!(ent.vrbits0 & QVR_VRBITS0_MAINHAND_RELOADFLICKING);
}


void(entity ent, float value) VRSetTeleporting =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_TELEPORTING, value);
}

void(entity ent, float value) VRSetOffHandGrabbingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_GRABBING, value);
}

void(entity ent, float value) VRSetOffHandPrevGrabbingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_PREVGRABBING, value);
}

void(entity ent, float value) VRSetMainHandGrabbingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_GRABBING, value);
}

void(entity ent, float value) VRSetMainHandPrevGrabbingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_PREVGRABBING, value);
}

void(entity ent, float value) VRSet2HAiming =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_2H_AIMING, value);
}

void(entity ent, float value) VRSetOffHandReloadFlickingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_RELOADFLICKING, value);
}

void(entity ent, float value) VRSetOffHandPrevReloadFlickingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_PREVRELOADFLICKING, value);
}

void(entity ent, float value) VRSetMainHandReloadFlickingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_RELOADFLICKING, value);
}

void(entity ent, float value) VRSetMainHandPrevReloadFlickingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_PREVRELOADFLICKING, value);
}

float() VRIsWeaponReloadingEnabled =
{
    return (cvar_hget(cvarh_vr_reload_mode) != QVR_RELOADMODE_NONE) &&
           (getWeaponMode() == QVR_WEAPONMODE_IMMERSIVE);
}

float(entity ent, float xHand) VRIsWeaponReloadingEnabledFor =
{
    return ent.ishuman &&
           VRIsWeaponReloadingEnabled() &&
           WeaponIdToSupportsReload(VRGetWeapon(xHand));
}

float(entity ent, float xHand) VRIsWeaponReloadFlickingEnabledFor =
{
    return ent.ishuman &&
           VRIsWeaponReloadingEnabled() &&
           WeaponIdToSupportsReloadFlicking(VRGetWeapon(xHand));
}
