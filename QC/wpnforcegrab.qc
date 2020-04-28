
//
//
//
// ----------------------------------------------------------------------------
// CVar Getters
// ----------------------------------------------------------------------------

float() getForcegrabMode =
{
    return cvar("vr_forcegrab_mode");
}

float() getForcegrabRange =
{
    return cvar("vr_forcegrab_range");
}

float() getForcegrabRadius =
{
    return cvar("vr_forcegrab_radius");
}

float() getForcegrabPowermult =
{
    return cvar("vr_forcegrab_powermult");
}

float() getForcegrabEligibleParticles =
{
    return cvar("vr_forcegrab_eligible_particles");
}

float() getForcegrabEligibleHaptics =
{
    return cvar("vr_forcegrab_eligible_haptics");
}

//
//
//
// ----------------------------------------------------------------------------
// Force Grab Utilities
// ----------------------------------------------------------------------------

void(vector handPos, vector handRot) ForceGrabTraceLine =
{
    makeforward(handRot);
    traceline(handPos, handPos + v_forward * getForcegrabRange(), FALSE, self);
}

float(float dist, float zmult) getForcegrabParabolaPower =
{
    local float power;

    power = (-0.00231424 * pow(dist, 2.0))
            + (2.07623 * dist)
            + 164.076;

    power *= zmult;
    power *= getForcegrabPowermult();

    return power;
}

float(entity e) isEligibleForForceGrab
{
    return (e.classname == "thrown_weapon") &&
           (e.flags & FL_ONGROUND) &&
           (vlen(e.velocity) <= 10.f);
}

void(float xHand) VRForceGrabEligibleHaptic =
{
    haptic(xHand, 0.0, 0.12, 75, 0.2);
}

//
//
//
// ----------------------------------------------------------------------------
// Force Grab Implementations
// ----------------------------------------------------------------------------

void(float xHand, vector handPos, vector handRot) DoForceGrabImplParabola =
{
    if(VRGetForceGrabbing(xHand))
    {
        return;
    }

    ForceGrabTraceLine(handPos, handRot);

    local entity e;
    for(e = findradius(trace_endpos, getForcegrabRadius()); e; e = e.chain)
    {
        if(!isEligibleForForceGrab(e))
        {
            continue;
        }

        local vector tmpVec;
        tmpVec = normalize(handPos - e.origin);

        e.origin += tmpVec * 0.1f;

        tmpVec = handPos - e.origin;

        local float zdist;
        zdist = tmpVec_z;

        local float zmult;

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

        local float dist;
        dist = vlen(handPos - e.origin);

        e.velocity = tmpVec * getForcegrabParabolaPower(dist, zmult);

        sound (e, CHAN_OTHER, "knight/sword2.wav", 0.7, ATTN_NORM);
        sound (self, VRGetGunChannel(xHand), "forcegrab.wav", 0.65, ATTN_NORM);

        VRSetForceGrabbing(xHand, TRUE);
        break;
    }
}

void(float xHand, vector handPos, vector handRot) DoForceGrabImplLinear =
{
    if(VRGetForceGrabbing(xHand))
    {
        return;
    }

    ForceGrabTraceLine(handPos, handRot);

    local entity e;
    for(e = findradius(trace_endpos, getForcegrabRadius()); e; e = e.chain)
    {
        // TODO VR: (P0) test, it didnt have the velocity requirement before
        if(!isEligibleForForceGrab(e))
        {
            continue;
        }

        if(e.throwhit == QVR_THROWHIT_HIT)
        {
            if(e.flags & FL_ONGROUND)
            {
                e.throwhit = QVR_THROWHIT_FORCEGRAB;
                e.movetype = MOVETYPE_FLYMISSILE;

                local vector tmpVec;
                tmpVec = normalize(handPos - e.origin);
                e.origin += tmpVec * 0.1f;

                sound (e, CHAN_OTHER, "knight/sword2.wav", 0.7, ATTN_NORM);
                sound (self, VRGetGunChannel(xHand), "forcegrab.wav", 0.65, ATTN_NORM);
            }
        }

        if(e.throwhit == QVR_THROWHIT_FORCEGRAB)
        {
            local vector dir;
            dir = normalize(handPos - e.origin);

            e.velocity = dir * 700;
            e.velocity *= getForcegrabPowermult();

            VRSetForceGrabbing(xHand, TRUE);
            break;
        }
    }
}

void(float xHand, vector handPos, vector handRot) DoForceGrabImplInstant =
{
    if(VRGetForceGrabbing(xHand))
    {
        return;
    }

    ForceGrabTraceLine(handPos, handRot);

    local entity e;
    for(e = findradius(trace_endpos, getForcegrabRadius()); e; e = e.chain)
    {
        if(!isEligibleForForceGrab(e))
        {
            continue;
        }

        wpnthrow_forcegrab_instant_ent(e, self, xHand);

        sound (e, CHAN_OTHER, "knight/sword2.wav", 0.7, ATTN_NORM);
        sound (self, VRGetGunChannel(xHand), "forcegrab.wav", 0.65, ATTN_NORM);

        VRSetForceGrabbing(xHand, TRUE);
        break;
    }
}

void(float xHand, vector handPos, vector handRot) ParticlesAndHapticsForEligibleForceGrabWeapons =
{
    local float doParticles, doHaptics;

    doParticles = getForcegrabEligibleParticles();
    doHaptics = getForcegrabEligibleHaptics();

    if(doParticles == FALSE && doHaptics == FALSE)
    {
        return;
    }

    ForceGrabTraceLine(handPos, handRot);

    local entity e;
    for(e = findradius(trace_endpos, getForcegrabRadius()); e; e = e.chain)
    {
        if(!isEligibleForForceGrab(e))
        {
            continue;
        }

        if(doParticles)
        {
            if(random() < 0.4)
            {
                particle2(e.origin, '0 0 0', 9 /* gunforcegrab preset */, 1);
            }
        }

        if(doHaptics)
        {
            VRForceGrabEligibleHaptic(xHand);
        }
    }
}
