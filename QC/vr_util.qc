//
//
//
// ----------------------------------------------------------------------------
// VR Utilities Implementation Details
// ----------------------------------------------------------------------------

void(string fnName, float xHand) VRImpl_InvalidHandIdMessage =
{
    bprint("`");
    bprint(fnName);
    bprint("`: invalid hand id '");
    bprint(ftos(xHand));
    bprint("'\n");
}

vector(string fnName, float xHand, vector xOff, vector xMain) VRImpl_VectorGetter =
{
    if(xHand == cVR_OffHand)  return xOff;
    if(xHand == cVR_MainHand) return xMain;

    VRImpl_InvalidHandIdMessage(fnName, xHand);
    return '0 0 0';
}

float(string fnName, float xHand, float xOff, float xMain) VRImpl_FloatGetter =
{
    if(xHand == cVR_OffHand)  return xOff;
    if(xHand == cVR_MainHand) return xMain;

    VRImpl_InvalidHandIdMessage(fnName, xHand);
    return 0.f;
}

void(string fnName, float xHand,
      float* xOutOff, float* xOutMain, float xValue) VRImpl_FloatSetter =
{
    if(xHand == cVR_OffHand)  { *xOutOff = xValue; return; }
    if(xHand == cVR_MainHand) { *xOutMain = xValue; return; }

    VRImpl_InvalidHandIdMessage(fnName, xHand);
}

#define VR_IMPL_MAKE_VECTOR_GETTER(xFnName, xOff, xMain) \
    vector(float xHand) xFnName = \
    { \
        return VRImpl_VectorGetter(#xFnName, xHand, xOff, xMain); \
    }

#define VR_IMPL_MAKE_FLOAT_GETTER(xFnName, xOff, xMain) \
    float(float xHand) xFnName = \
    { \
        return VRImpl_FloatGetter(#xFnName, xHand, xOff, xMain); \
    }

#define VR_IMPL_MAKE_FLOAT_SETTER(xFnName, xOff, xMain) \
    void(float xHand, float xValue) xFnName = \
    { \
        VRImpl_FloatSetter(#xFnName, xHand, xOff, xMain, xValue); \
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

VR_IMPL_MAKE_VECTOR_GETTER(VRGetMuzzlePos,
                           self.offmuzzlepos,
                           self.muzzlepos);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetHandVelMag,
                          self.offhandvelmag,
                          self.handvelmag);

vector(float xHand) VRGetWeaponFireRot =
{
    return VRGetHandRot(xHand);
}

vector(float xHand) VRGetWeaponFireRotFwd =
{
    makeforward(VRGetWeaponFireRot(xHand));
    return v_forward;
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

void(vector v) bprintVec =
{
    bprint("{");
    bprint(ftos(v_x));
    bprint("; ");
    bprint(ftos(v_y));
    bprint("; ");
    bprint(ftos(v_z));
    bprint("}");

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

VR_IMPL_MAKE_FLOAT_SETTER(VRSetCurrentAmmo,
                          &self.currentammo2,
                          &self.currentammo);

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

VR_IMPL_MAKE_FLOAT_GETTER(VRGetForceGrabbing,
                          self.offhand_forcegrabbing,
                          self.mainhand_forcegrabbing);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetForceGrabbing,
                          &self.offhand_forcegrabbing,
                          &self.mainhand_forcegrabbing);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetAmmoCounter,
                          self.ammocounter2,
                          self.ammocounter);

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

VR_IMPL_MAKE_FLOAT_GETTER(VRGetWeaponClipSize,
                          self.weaponclipsize2,
                          self.weaponclipsize);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetWeaponClipSize,
                          &self.weaponclipsize2,
                          &self.weaponclipsize);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetLavaGunFired,
                          self.offHandLavaGunFired,
                          self.mainHandLavaGunFired);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetLavaGunFired,
                          &self.offHandLavaGunFired,
                          &self.mainHandLavaGunFired);

float(entity e, float xHand) VRGetEntFireButtonPressed =
{
    if(xHand == cVR_OffHand) return e.button3;
    else                     return e.button0;
}

vector(entity e, float xHand) VRGetEntHandPos =
{
    if(xHand == cVR_OffHand) return e.offhandpos;
    else                     return e.handpos;
}

float(float xHand) VRIsAmmolessWeapon =
{
    return VRGetCurrentAmmo(xHand) == AID_NONE;
}

float(float xHand) VRGetOtherHand =
{
    if(xHand == cVR_OffHand) return cVR_MainHand;
    else                     return cVR_OffHand;
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

void(entity e, float xHand, float v) VRSetEntWeaponClipSize =
{
    if(xHand == cVR_OffHand) e.weaponclipsize2 = v;
    else                     e.weaponclipsize = v;
}

void(float xHand, float v) VRSetWeapon =
{
    VRSetEntWeapon(self, xHand, v);
}

float(entity e, float xHand) VRGetEntWeapon =
{
    if(xHand == cVR_OffHand) return e.weapon2;
    else                     return e.weapon;
}

float(entity e, float xHand) VRGetEntWeaponFlags =
{
    if(xHand == cVR_OffHand) return e.weaponflags2;
    else                     return e.weaponflags;
}

float(entity e, float xHand) VRGetEntWeaponClip =
{
    if(xHand == cVR_OffHand) return e.weaponclip2;
    else                     return e.weaponclip;
}

float(entity e, float xHand) VRGetEntWeaponClipSize =
{
    if(xHand == cVR_OffHand) return e.weaponclipsize2;
    else                     return e.weaponclipsize;
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

float*(entity e, float xHand) VRGetEntWeaponClipSizePtr =
{
    if(xHand == cVR_OffHand) return &e.weaponclipsize2;
    else                     return &e.weaponclipsize;
}

float(entity e, float xHand) VRIsHandEmpty =
{
    return VRGetEntWeapon(e, xHand) == WID_FIST;
}

void(entity e, float xHand) VRSetHandEmpty =
{
    VRSetEntWeapon(e, xHand, WID_FIST);
    VRSetEntWeaponFlags(e, xHand, 0);
    VRSetEntWeaponClip(e, xHand, 0);
}

float(float xHand) VRGetWeapon =
{
    return VRGetEntWeapon(self, xHand);
}

float(entity e, float xHand) VRIsEntHandGrabbingBit =
{
    if(xHand == cVR_OffHand) return VRGetOffHandGrabbingBit(e);
    else                     return VRGetMainHandGrabbingBit(e);
}

float(entity e, float xHand) VRIsEntHandPrevGrabbingBit =
{
    if(xHand == cVR_OffHand) return VRGetOffHandPrevGrabbingBit(e);
    else                     return VRGetMainHandPrevGrabbingBit(e);
}

float(entity e, float xHand) VRIsEntHandReloadingBit =
{
    if(xHand == cVR_OffHand) return VRGetOffHandReloadingBit(e);
    else                     return VRGetMainHandReloadingBit(e);
}

float(entity e, float xHand) VRIsEntHandPrevReloadingBit =
{
    if(xHand == cVR_OffHand) return VRGetOffHandPrevReloadingBit(e);
    else                     return VRGetMainHandPrevReloadingBit(e);
}

float(entity e, float xHand) VRGetEntForceGrabbing =
{
    if(xHand == cVR_OffHand) return e.offhand_forcegrabbing;
    else                     return e.mainhand_forcegrabbing;
}

float(entity e, float xHand) VRIsEntHandReloadFlickingBit =
{
    if(xHand == cVR_OffHand) return VRGetOffHandReloadFlickingBit(e);
    else                     return VRGetMainHandReloadFlickingBit(e);
}

float(entity e, float xHand) VRIsEntHandPrevReloadFlickingBit =
{
    if(xHand == cVR_OffHand) return VRGetOffHandPrevReloadFlickingBit(e);
    else                     return VRGetMainHandPrevReloadFlickingBit(e);
}

void(entity e, float xHand, float v) VRSetEntForceGrabbing =
{
    if(xHand == cVR_OffHand) e.offhand_forcegrabbing = v;
    else                     e.mainhand_forcegrabbing = v;
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

float(float flags, float bit) VRHasBit =
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

float(entity ent) VRGetTeleporting =
{
    return VRHasBit(ent.vrbits0, QVR_VRBITS0_TELEPORTING);
}

float(entity ent) VRGetOffHandGrabbingBit =
{
    return VRHasBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_GRABBING);
}

float(entity ent) VRGetOffHandPrevGrabbingBit =
{
    return VRHasBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_PREVGRABBING);
}

float(entity ent) VRGetMainHandGrabbingBit =
{
    return VRHasBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_GRABBING);
}

float(entity ent) VRGetMainHandPrevGrabbingBit =
{
    return VRHasBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_PREVGRABBING);
}

float(entity ent) VRGet2HAiming =
{
    return VRHasBit(ent.vrbits0, QVR_VRBITS0_2H_AIMING);
}

float(entity ent) VRGetOffHandReloadingBit =
{
    return VRHasBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_RELOADING);
}

float(entity ent) VRGetOffHandPrevReloadingBit =
{
    return VRHasBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_PREVRELOADING);
}

float(entity ent) VRGetMainHandReloadingBit =
{
    return VRHasBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_RELOADING);
}

float(entity ent) VRGetMainHandPrevReloadingBit =
{
    return VRHasBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_PREVRELOADING);
}

float(entity ent) VRGetOffHandReloadFlickingBit =
{
    return VRHasBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_RELOADFLICKING);
}

float(entity ent) VRGetOffHandPrevReloadFlickingBit =
{
    return VRHasBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_PREVRELOADFLICKING);
}

float(entity ent) VRGetMainHandReloadFlickingBit =
{
    return VRHasBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_RELOADFLICKING);
}

float(entity ent) VRGetMainHandPrevReloadFlickingBit =
{
    return VRHasBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_PREVRELOADFLICKING);
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

void(entity ent, float value) VRSetOffHandReloadingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_RELOADING, value);
}

void(entity ent, float value) VRSetOffHandPrevReloadingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_PREVRELOADING, value);
}

void(entity ent, float value) VRSetMainHandReloadingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_RELOADING, value);
}

void(entity ent, float value) VRSetMainHandPrevReloadingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_PREVRELOADING, value);
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
