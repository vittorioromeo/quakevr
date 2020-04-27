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
    if(xHand == OFF_HAND)  return xOff;
    if(xHand == MAIN_HAND) return xMain;

    VRImpl_InvalidHandIdMessage(fnName, xHand);
    return '0 0 0';
}

float(string fnName, float xHand, float xOff, float xMain) VRImpl_FloatGetter =
{
    if(xHand == OFF_HAND)  return xOff;
    if(xHand == MAIN_HAND) return xMain;

    VRImpl_InvalidHandIdMessage(fnName, xHand);
    return 0.f;
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

//
//
//
// ----------------------------------------------------------------------------
// VR Getters
// ----------------------------------------------------------------------------

VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandPos,      self.offhandpos,      self.handpos);
VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandRot,      self.offhandrot,      self.handrot);
VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandVel,      self.offhandvel,      self.handvel);
VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandThrowVel, self.offhandthrowvel, self.handthrowvel);
VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandAVel,     self.offhandavel,     self.handavel);
VR_IMPL_MAKE_VECTOR_GETTER(VRGetMuzzlePos,    self.offmuzzlepos,    self.muzzlepos);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetHandVelMag,    self.offhandvelmag,   self.handvelmag);

vector(float xHand) VRGetWeaponFireRot =
{
    return VRGetHandRot(xHand);
}

vector(float xHand) VRGetWeaponFireRotFwd =
{
    makeforward (VRGetWeaponFireRot(xHand));
    return v_forward;
}

//
//
//
// ----------------------------------------------------------------------------
// TODO VR: (P2) refactor like above
// ----------------------------------------------------------------------------

float(float xHand) VRMeleeHitSoundPlayed =
{
    if(xHand == OFF_HAND) return self.offhand_melee_hit_sound_played;
    else                  return self.melee_hit_sound_played;
}

void(float xHand, float v) VRMeleeSetHitSoundPlayed =
{
    if(xHand == OFF_HAND) self.offhand_melee_hit_sound_played = v;
    else                  self.melee_hit_sound_played = v;
}

float(float xHand) VRGunChannel =
{
    if(xHand == OFF_HAND) return CHAN_WEAPON2;
    else                  return CHAN_WEAPON;
}

float(float xHand) VRIsAmmolessWeapon =
{
    if(xHand == OFF_HAND) return self.currentammo2 == AID_NONE;
    else                  return self.currentammo == AID_NONE;
}

float(float xHand) VROtherHand =
{
    if(xHand == OFF_HAND) return MAIN_HAND;
    else                  return OFF_HAND;
}

float(float xHand) VRFireButtonPressed =
{
    if(xHand == OFF_HAND) return self.button3;
    else                  return self.button0;
}

void(entity e, float xHand, float v) VRSetEntWeapon =
{
    if(xHand == OFF_HAND) e.weapon2 = v;
    else                  e.weapon = v;
}

void(float xHand, float v) VRSetWeapon =
{
    VRSetEntWeapon(self, xHand, v);
}

float(entity e, float xHand) VRGetEntWeapon =
{
    if(xHand == OFF_HAND) return e.weapon2;
    else                  return e.weapon;
}

float(entity e, float xHand) VRIsHandEmpty =
{
    return VRGetEntWeapon(e, xHand) == WID_FIST;
}

void(entity e, float xHand) VRSetHandEmpty =
{
    VRSetEntWeapon(e, xHand, WID_FIST);
}

float(float xHand) VRGetWeapon =
{
    return VRGetEntWeapon(self, xHand);
}

float(entity e, float xHand) VRIsEntHandGrabbing =
{
    if(xHand == OFF_HAND) return e.offhand_grabbing;
    else                  return e.mainhand_grabbing;
}

float(entity e, float xHand) VRIsEntHandPrevGrabbing =
{
    if(xHand == OFF_HAND) return e.offhand_prevgrabbing;
    else                  return e.mainhand_prevgrabbing;
}

float(entity e, float xHand) VREntStartedHandGrabbing =
{
    return VRIsEntHandPrevGrabbing(e, xHand) == FALSE &&
           VRIsEntHandGrabbing(e, xHand) == TRUE;
}

float(entity e, float xHand) VREntStoppedHandGrabbing =
{
    return VRIsEntHandPrevGrabbing(e, xHand) == TRUE &&
           VRIsEntHandGrabbing(e, xHand) == FALSE;
}

float(float xHand) VRIsHandGrabbing =
{
    return VRIsEntHandGrabbing(self, xHand);
}

float(float xHand) VRIsHandPrevGrabbing =
{
    return VRIsEntHandPrevGrabbing(self, xHand);
}

float(float xHand) VRStartedHandGrabbing =
{
    return VREntStartedHandGrabbing(self, xHand);
}

float(float xHand) VRStoppedHandGrabbing =
{
    return VREntStoppedHandGrabbing(self, xHand);
}

float(entity entPlayer) VRTouchingHandIsBody =
{
    return entPlayer.touchinghand == FAKE_HAND;
}

float(entity entPlayer) VRTouchingHand =
{
    if(VRTouchingHandIsBody(entPlayer))
    {
        return MAIN_HAND;
    }

    return entPlayer.touchinghand;
}

float(entity entTarget) VRHandtouchHandIsBody =
{
    return entTarget.handtouch_hand == FAKE_HAND;
}

float(entity entTarget) VRHandtouchHand =
{
    if(VRHandtouchHandIsBody(entTarget))
    {
        return MAIN_HAND;
    }

    return entTarget.handtouch_hand;
}
