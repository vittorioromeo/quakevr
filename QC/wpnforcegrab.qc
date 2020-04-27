
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

float() getForcegrabParabolaPowermult =
{
    return cvar("vr_forcegrab_parabola_powermult");
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
    power *= getForcegrabParabolaPowermult();

    return power;
}

//
//
//
// ----------------------------------------------------------------------------
// Force Grab Implementations
// ----------------------------------------------------------------------------

void(float xHand, vector handPos, vector handRot) DoForceGrabImplParabola =
{
    ForceGrabTraceLine(handPos, handRot);

    local entity e;
    for (e = findradius(trace_endpos, getForcegrabRadius()); e; e = e.chain)
    {
        if (e.classname != "thrown_weapon")
        {
            continue;
        }

        if((!(e.flags & FL_ONGROUND)) || vlen(e.velocity) > 10.f)
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
        sound (self, VRGunChannel(xHand), "forcegrab.wav", 0.65, ATTN_NORM);

        break;
    }
}

void(float xHand, vector handPos, vector handRot) DoForceGrabImplLinear =
{
    ForceGrabTraceLine(handPos, handRot);

    local entity e;
    for (e = findradius(trace_endpos, getForcegrabRadius()); e; e = e.chain)
    {
        if (e.classname != "thrown_weapon")
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
                sound (self, VRGunChannel(xHand), "forcegrab.wav", 0.65, ATTN_NORM);
            }
        }

        if(e.throwhit == QVR_THROWHIT_FORCEGRAB)
        {
            local vector dir;
            dir = normalize(handPos - e.origin);

            e.velocity = dir * 700;
            break;
        }
    }
}

void(float xHand, vector handPos, vector handRot) DoForceGrabImplInstant =
{
    ForceGrabTraceLine(handPos, handRot);

    local entity e;
    for (e = findradius(trace_endpos, getForcegrabRadius()); e; e = e.chain)
    {
        if (e.classname != "thrown_weapon")
        {
            continue;
        }

        if((!(e.flags & FL_ONGROUND)) || vlen(e.velocity) > 10.f)
        {
            continue;
        }

        wpnthrow_forcegrab_instant_ent(e, self, xHand);

        sound (e, CHAN_OTHER, "knight/sword2.wav", 0.7, ATTN_NORM);
        sound (self, VRGunChannel(xHand), "forcegrab.wav", 0.65, ATTN_NORM);

        break;
    }
}
