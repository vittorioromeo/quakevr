//
//
//
// ----------------------------------------------------------------------------
// CVar Getters
// ----------------------------------------------------------------------------

float() VR_ForcegrabUtil_GetForcegrabMode =
{
    return cvar_hget(cvarh_vr_forcegrab_mode);
}

float() VR_ForcegrabUtil_GetForcegrabRange =
{
    return cvar_hget(cvarh_vr_forcegrab_range);
}

float() VR_ForcegrabUtil_GetForcegrabRadius =
{
    return cvar_hget(cvarh_vr_forcegrab_radius);
}

float() VR_ForcegrabUtil_GetForcegrabPowermult =
{
    return cvar_hget(cvarh_vr_forcegrab_powermult);
}

float() VR_ForcegrabUtil_GetForcegrabEligibleParticles =
{
    return cvar_hget(cvarh_vr_forcegrab_eligible_particles);
}

float() VR_ForcegrabUtil_GetForcegrabEligibleHaptics =
{
    return cvar_hget(cvarh_vr_forcegrab_eligible_haptics);
}

//
//
//
// ----------------------------------------------------------------------------
// Force Grab Utilities
// ----------------------------------------------------------------------------

float(float xHand, vector handPos, vector handRot,
     float(vector xPos, float xHand, vector handPos, vector handRot) logicFn)
     VR_ForcegrabUtil_Impl_PerformForcegrab =
{
    float nTraces = ceil(VR_ForcegrabUtil_GetForcegrabRange() /
                         VR_ForcegrabUtil_GetForcegrabRadius());

    if(nTraces > 15)
    {
        nTraces = 15;
    }

    makeforward(handRot);

    traceline(handPos,
        handPos + v_forward * VR_ForcegrabUtil_GetForcegrabRange(),
        FALSE, self);

    if(logicFn(trace_endpos, xHand, handPos, handRot))
    {
        return TRUE;
    }

    for(float i = 0; i <= nTraces * 2; ++i)
    {
        traceline(handPos,
            handPos + v_forward * ((VR_ForcegrabUtil_GetForcegrabRadius() / 2.0) * i),
            TRUE, self);

        if(logicFn(trace_endpos, xHand, handPos, handRot))
        {
            return TRUE;
        }
    }

    return FALSE;
}

float(float dist, float zmult) VR_ForcegrabUtil_Impl_GetForcegrabParabolaPower =
{
    // TODO VR: (P0) scale with player height?

    float power = (-0.00231424 * pow(dist, 2.0))
                + (2.01 * dist)
                + 172.0;

    power *= zmult;
    power *= VR_ForcegrabUtil_GetForcegrabPowermult();

    return power;
}

float(entity xEntThrownWeapon) VR_ForcegrabUtil_Impl_IsEligibleForForcegrab
{
    if(xEntThrownWeapon.classname != "thrown_weapon")
    {
        return FALSE;
    }

    if((xEntThrownWeapon.flags & FL_ONGROUND) &&
      vlen(xEntThrownWeapon.velocity) <= 10.f)
    {
        return TRUE;
    }

    if((xEntThrownWeapon.flags & FL_INWATER))
    {
        return TRUE;
    }

    return FALSE;
}

void(float xHand) VR_ForcegrabUtil_Impl_ForcegrabEligibleHaptic =
{
    if(!self.ishuman)
    {
        return;
    }

    haptic(xHand, 0.0, 0.12, 75, 0.2);
}

//
//
//
// ----------------------------------------------------------------------------
// Force Grab Implementations
// ----------------------------------------------------------------------------

float(vector xPos, float xHand, vector handPos, vector handRot)
VR_ForcegrabUtil_Impl_DoForcegrabParabola_Logic =
{
    for(entity e = findradius(xPos, VR_ForcegrabUtil_GetForcegrabRadius()); e; e = e.chain)
    {
        if(!VR_ForcegrabUtil_Impl_IsEligibleForForcegrab(e))
        {
            continue;
        }

        vector tmpVec = normalize(handPos - e.origin);

        e.origin += tmpVec * 0.1f;

        tmpVec = handPos - e.origin;

        float zdist = tmpVec_z;

        float zmult;
        if(zdist >= 30.0)
        {
            zmult = pow(fabs(zdist) / 40.0, 0.2);
        }
        else
        {
            zmult = pow((10 + fabs(zdist)) / 40.0, 0.13);
        }

        tmpVec_x = tmpVec_x / 2.0;
        tmpVec_y = tmpVec_y / 2.0;
        tmpVec_z = tmpVec_z * 2.0;

        if(tmpVec_z < 80)
        {
            tmpVec_z = 80;
        }

        tmpVec = normalize(tmpVec);

        float dist = vlen(handPos - e.origin);

        e.velocity = tmpVec * VR_ForcegrabUtil_Impl_GetForcegrabParabolaPower(dist, zmult);

        sound(e, CHAN_OTHER, "knight/sword2.wav", 0.7, ATTN_NORM);
        sound(self, VRGetGunChannel(xHand), "forcegrab.wav", 0.65, ATTN_NORM);

        VRSetForceGrabbing(xHand, TRUE);
        return TRUE;
    }

    return FALSE;
}

float(float xHand, vector handPos, vector handRot) VR_ForcegrabUtil_Impl_DoForcegrabParabola =
{
    if(VRGetForceGrabbing(xHand))
    {
        return FALSE;
    }

    return VR_ForcegrabUtil_Impl_PerformForcegrab(xHand, handPos, handRot,
                            VR_ForcegrabUtil_Impl_DoForcegrabParabola_Logic);
}

// ---

float(vector xPos, float xHand, vector handPos, vector handRot)
VR_ForcegrabUtil_Impl_DoForcegrabLinear_Logic =
{
    for(entity e = findradius(xPos, VR_ForcegrabUtil_GetForcegrabRadius()); e; e = e.chain)
    {
        if(!VR_ForcegrabUtil_Impl_IsEligibleForForcegrab(e))
        {
            continue;
        }

        if(e.throwhit != QVR_THROWHIT_FORCEGRAB)
        {
            e.throwhit = QVR_THROWHIT_FORCEGRAB;
            e.movetype = MOVETYPE_FLYMISSILE;

            vector tmpVec = normalize(handPos - e.origin);
            e.origin += tmpVec * 0.1f;

            sound(e, CHAN_OTHER, "knight/sword2.wav", 0.7, ATTN_NORM);
            sound(self, VRGetGunChannel(xHand), "forcegrab.wav", 0.65, ATTN_NORM);
        }

        if(e.throwhit == QVR_THROWHIT_FORCEGRAB)
        {
            vector dir = normalize(handPos - e.origin);

            e.velocity = dir * 700;
            e.velocity *= VR_ForcegrabUtil_GetForcegrabPowermult();

            VRSetForceGrabbing(xHand, TRUE);
            return TRUE;
        }
    }

    return FALSE;
}

float(float xHand, vector handPos, vector handRot) VR_ForcegrabUtil_Impl_DoForcegrabLinear =
{
    if(VRGetForceGrabbing(xHand))
    {
        return FALSE;
    }

    return VR_ForcegrabUtil_Impl_PerformForcegrab(xHand, handPos, handRot,
                            VR_ForcegrabUtil_Impl_DoForcegrabLinear_Logic);
}

// ---

float(vector xPos, float xHand, vector handPos, vector handRot)
VR_ForcegrabUtil_Impl_DoForcegrabInstant_Logic =
{
    for(entity e = findradius(xPos, VR_ForcegrabUtil_GetForcegrabRadius()); e; e = e.chain)
    {
        if(!VR_ForcegrabUtil_Impl_IsEligibleForForcegrab(e))
        {
            continue;
        }

        wpnthrow_forcegrab_instant_ent(e, self, xHand);

        sound(e, CHAN_OTHER, "knight/sword2.wav", 0.7, ATTN_NORM);
        sound(self, VRGetGunChannel(xHand), "forcegrab.wav", 0.65, ATTN_NORM);

        VRSetForceGrabbing(xHand, TRUE);
        return TRUE;
    }

    return FALSE;
}

float(float xHand, vector handPos, vector handRot) VR_ForcegrabUtil_Impl_DoForcegrabInstant =
{
    if(VRGetForceGrabbing(xHand))
    {
        return FALSE;
    }

    return VR_ForcegrabUtil_Impl_PerformForcegrab(xHand, handPos, handRot,
                            VR_ForcegrabUtil_Impl_DoForcegrabInstant_Logic);
}

// ---

float(vector xPos, float xHand, vector handPos, vector handRot)
ParticlesAndHapticsForEligibleForceGrabWeapon_Logic =
{
    float doParticles = VR_ForcegrabUtil_GetForcegrabEligibleParticles();
    float doHaptics = VR_ForcegrabUtil_GetForcegrabEligibleHaptics();

    for(entity e = findradius(xPos, VR_ForcegrabUtil_GetForcegrabRadius()); e; e = e.chain)
    {
        if(!VR_ForcegrabUtil_Impl_IsEligibleForForcegrab(e))
        {
            continue;
        }

        if(doParticles)
        {
            if(random() < 0.4)
            {
                particle2(e.origin, '0 0 0', QVR_PARTICLE_PRESET_GUNFORCEGRAB, 1);
            }
        }

        if(doHaptics)
        {
            VR_ForcegrabUtil_Impl_ForcegrabEligibleHaptic(xHand);
        }

        return TRUE;
    }

    return FALSE;
}

float(float xHand, vector handPos, vector handRot) ParticlesAndHapticsForEligibleForceGrabWeapons =
{
    float doParticles = VR_ForcegrabUtil_GetForcegrabEligibleParticles();
    float doHaptics = VR_ForcegrabUtil_GetForcegrabEligibleHaptics();

    if(doParticles == FALSE && doHaptics == FALSE)
    {
        return FALSE;
    }

    return VR_ForcegrabUtil_Impl_PerformForcegrab(xHand, handPos, handRot,
                            ParticlesAndHapticsForEligibleForceGrabWeapon_Logic);
}
