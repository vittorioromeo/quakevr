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

        float initialVelocity = 400;
        float flightTime = 0.75;
        float grav = -cvar("sv_gravity"); // * frametime;

        vector startPos = e.origin;
        vector endPos = handPos;

        float angleX = acos((endPos.x - startPos.x) / (initialVelocity * flightTime));
        float angleY = acos((endPos.y - startPos.y) / (initialVelocity * flightTime));
        float angleZ = 360 + asin((endPos.z - startPos.z + (0.5 * grav * flightTime * flightTime)) / (initialVelocity * flightTime));

        BPRINT_FVAR(angleX);
        BPRINT_FVAR(angleY);
        BPRINT_FVAR(angleZ);

        float avgAngleXY = (angleX + angleY) / 2.0;

        vector diff = normalize(endPos - startPos);
        diff_z = 0;

        // TODO VR: (P0): avgAngleXY seems reasonable, but the below rotation returns weird vectors.
        vector rotdiff = rotatevec(diff, v_right, avgAngleXY);

        e.velocity = rotdiff * initialVelocity;

        /*return;


        vector handWpnDiff = handPos - e.origin;
        e.origin += normalize(handWpnDiff) * 0.15f;


        float zdist = handWpnDiff_z;

        float zmult;
        if(zdist >= 30.0)
        {
            zmult = pow(fabs(zdist) / 40.0, 0.2);
        }
        else
        {
            zmult = pow((10 + fabs(zdist)) / 40.0, 0.13);
        }

        vector tmpVec = handWpnDiff;
        tmpVec_x = tmpVec_x / 2.0;
        tmpVec_y = tmpVec_y / 2.0;
        tmpVec_z = tmpVec_z * 2.0;

        if(tmpVec_z < 80)
        {
            tmpVec_z = 80;
        }

        tmpVec = normalize(tmpVec);

        float dist = vlen(handWpnDiff);

        e.velocity = tmpVec * VR_ForcegrabUtil_Impl_GetForcegrabParabolaPower(dist, zmult);


        /*
        vector tgt = handWpnDiff;
        tgt_x = tgt_x / 2.0;
        tgt_y = tgt_y / 2.0;
        tgt_z = tgt_z * 2.0;
        tgt += e.origin;


        float dist = vlen(handWpnDiff);
        float power = 700;// VR_ForcegrabUtil_Impl_GetForcegrabParabolaPower(dist, 1.0);
        float throwAngle = calcthrowangle(e.gravity, power, e.origin, tgt);

        BPRINT_FVAR(dist);
        BPRINT_FVAR(power);
        BPRINT_FVAR(throwAngle);

        vector diff = normalize(tgt - e.origin);
        diff_z = 0;
        BRPINT_VVAR(diff);

        makevectors(vectoangles(diff));

              BRPINT_VVAR(v_forward);
        BRPINT_VVAR(v_right);
        BRPINT_VVAR(v_up);


        vector rotdiff = rotatevec(diff, v_right, throwAngle);
        rotdiff_x *= -1;
        rotdiff_y *= -1;
        BRPINT_VVAR(rotdiff);

        e.velocity = rotdiff * power;
        */

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
