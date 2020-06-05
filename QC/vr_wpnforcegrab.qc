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

float(entity xEntThrownWeapon) VR_ForcegrabUtil_Impl_IsEligibleForForcegrab
{
    if(xEntThrownWeapon.classname != "thrown_weapon" &&
       !(xEntThrownWeapon.flags & FL_FORCEGRABBABLE))
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
// Force Grab State
// ----------------------------------------------------------------------------

float(entity entPlayer, float xHand)
VR_ForceGrabUtil_IsEntForceGrabbing =
{
    return time < VRGetEntForceGrabbing(entPlayer, xHand) + 0.3;
}

void(entity entPlayer, float xHand)
VR_ForceGrabUtil_EntStartForceGrabbing =
{
    VRSetEntForceGrabbing(entPlayer, xHand, time);
}

void(entity entPlayer, float xHand)
VR_ForceGrabUtil_EntStopForceGrabbing =
{
    VRSetEntForceGrabbing(entPlayer, xHand, 0);
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

        vector startPos = e.origin;

        makevectors(handRot);
        vector endPos = handPos - v_forward * 3.0;

        vector diff = endPos - startPos;
        float distFactor = vlen(diff) / 130;

        float initialSpeed = 180 + 275 * distFactor;
        float grav = -cvar("sv_gravity");

        vector diffXY = diff;
        diffXY.z = 0;

        float groundFactor = 1;
        /*
        if(startPos.z < endPos.z)
        {
            float gf = fabs(startPos.z - endPos.z) / 70;
            if(gf > 1)
            {
                groundFactor = pow(gf, 0.7);
            }
        }
        */

        float groundDist = vlen(diffXY);

        float speed2 = initialSpeed * initialSpeed;
        float speed4 = initialSpeed * initialSpeed * initialSpeed * initialSpeed;
        float z = diff.z;
        float x = groundDist;
        float gx = grav * x;

        float root = speed4 - grav*(grav*x*x + 2*z*speed2);

        if (root < 0)
        {
            bprint("could not solve equation");
            return FALSE;
        }

        root = sqrt(root);

        float highAng = atan2(speed2 + root, gx);

        vector groundDir = normalize(diffXY);

        e.velocity = -groundDir*cos(highAng)*initialSpeed + '0 0 1'*sin(highAng)*initialSpeed;
        e.velocity *= 1.05;
        e.velocity *= groundFactor;

        e.throwhit = QVR_THROWHIT_FORCEGRAB_PARABOLA;
        SetThrownWeaponForcegrabTime(e, time);
        e.enemy = self;
        e.thinkArg = xHand;

        sound(e, CHAN_AUTO, "knight/sword2.wav", 0.7, ATTN_NORM);
        sound(self, CHAN_AUTO, "forcegrab.wav", 0.65, ATTN_NORM);

        VR_ForceGrabUtil_EntStartForceGrabbing(self, xHand);
        return TRUE;
    }

    return FALSE;
}

float(float xHand, vector handPos, vector handRot) VR_ForcegrabUtil_Impl_DoForcegrabParabola =
{
    if(VR_ForceGrabUtil_IsEntForceGrabbing(self, xHand))
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
            SetThrownWeaponForcegrabTime(e, time);
            SetThrownWeaponForcegrabHand(e, xHand);

            vector dir = normalize(handPos - e.origin);
            e.origin += dir * 0.25f;

            sound(e, CHAN_AUTO, "knight/sword2.wav", 0.7, ATTN_NORM);
            sound(self, CHAN_AUTO, "forcegrab.wav", 0.65, ATTN_NORM);
        }

        if(e.throwhit == QVR_THROWHIT_FORCEGRAB)
        {
            e.movetype = MOVETYPE_FLYMISSILE;

            vector dir = normalize(handPos - e.origin);

            e.velocity = dir * 700;
            e.velocity *= VR_ForcegrabUtil_GetForcegrabPowermult();

            VR_ForceGrabUtil_EntStartForceGrabbing(self, xHand);
            return TRUE;
        }
    }

    return FALSE;
}

float(float xHand, vector handPos, vector handRot) VR_ForcegrabUtil_Impl_DoForcegrabLinear =
{
    if(VR_ForceGrabUtil_IsEntForceGrabbing(self, xHand))
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

        sound(e, CHAN_AUTO, "knight/sword2.wav", 0.7, ATTN_NORM);
        sound(self, CHAN_AUTO, "forcegrab.wav", 0.65, ATTN_NORM);

        VR_ForceGrabUtil_EntStartForceGrabbing(self, xHand);
        return TRUE;
    }

    return FALSE;
}

float(float xHand, vector handPos, vector handRot)
VR_ForcegrabUtil_Impl_DoForcegrabInstant =
{
    if(VR_ForceGrabUtil_IsEntForceGrabbing(self, xHand))
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
            if(random() < 0.35)
            {
                particle2(realorigin(e), '0 0 0', QVR_PARTICLE_PRESET_GUNFORCEGRAB, 1);
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

float(float xHand, vector handPos, vector handRot)
ParticlesAndHapticsForEligibleForceGrabWeapons =
{
    if(!VR_ForcegrabUtil_GetForcegrabEligibleParticles() &&
       !VR_ForcegrabUtil_GetForcegrabEligibleHaptics())
    {
        return FALSE;
    }

    return VR_ForcegrabUtil_Impl_PerformForcegrab(xHand, handPos, handRot,
        ParticlesAndHapticsForEligibleForceGrabWeapon_Logic);
}
