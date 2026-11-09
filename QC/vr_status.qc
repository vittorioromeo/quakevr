// vr_status.qc -- the status component (DYNAMIC_SPAWNS.md, "Prerequisites available"): what state a thing is in, the
// same question for every entity, the player included, server-side. Each state lives where its system keeps it (no
// copy to drift): burning is the fire's own (.burn_fire, vr_burning.qc), shocked lightning's (.vr_shock_until,
// vr_shock.qc), stunned a knockdown's or a parry's stagger; wet is kept here, by the rule the client's drips use to
// draw it (vr_drips.cpp: soaked while in water or slime, the coat full for the first quarter of vr_wounds_wet_time,
// then fading). Nothing is sent to the clients (they keep drawing wetness as before).
//
//   VR_Status(e)            its VR_STATUS_* bits now
//   VR_Status_Wet(e)        0 dry .. 1 soaked
//   VR_Status_Soak(e, lvl)  it is in water or slime `lvl` deep (1 feet .. 3 eyes) now (the liquid ticks call it)
//   VR_Status_Ignite(e, by) / VR_Status_Douse(e, why)   burning set and cleared through the fire's own code
// Bleeding is a bit for the future (the stuck-axe bleed and wounds keep their own state; not wired yet).

float VR_STATUS_BURNING = 1;
float VR_STATUS_WET = 2;
float VR_STATUS_SHOCKED = 4;
float VR_STATUS_STUNNED = 8;
float VR_STATUS_BLEEDING = 16;

.float st_wet_t;     // when it was last in water or slime (VR_Status_Soak); 0 never
.float st_wet_level; // ... how deep (1 the feet, 2 the waist, 3 the eyes)

void(entity e, float xLevel) VR_Status_Soak =
{
    if(xLevel >= 1)
    {
        e.st_wet_t = time;
        e.st_wet_level = xLevel;
    }
};

// How wet `e` is: 1 in water or slime and for the first quarter of vr_wounds_wet_time after, then less to 0 at its end.
float(entity e) VR_Status_Wet =
{
    float level = (e.flags & FL_CLIENT) ? e.waterlevel : VR_Liquid_Level(e);
    float liquid = (e.flags & FL_CLIENT) ? e.watertype : vr_liquid_type;
    if(liquid == CONTENT_WATER || liquid == CONTENT_SLIME)
    {
        VR_Status_Soak(e, level);
    }
    if(!e.st_wet_t)
    {
        return 0;
    }
    float dry = max(0.01, cvar_hget(cvarh_vr_wounds_wet_time));
    float dt = time - e.st_wet_t;
    if(dt <= dry * 0.25)
    {
        return 1;
    }
    return max(0, 1 - (dt - dry * 0.25) / (dry * 0.75));
};

float(entity e) VR_Status =
{
    float s = 0;
    if(e.burn_fire != world)
    {
        s = s | VR_STATUS_BURNING;
    }
    if(VR_Status_Wet(e) > 0)
    {
        s = s | VR_STATUS_WET;
    }
    if(e.vr_shock_until > time)
    {
        s = s | VR_STATUS_SHOCKED;
    }
    if(e.vr_knockdown || e.vr_parry_stagger_until > time)
    {
        s = s | VR_STATUS_STUNNED;
    }
    return s;
};

// Burning set through the fire's own code (VR_Burn_Ignite: it decides whether `e` can burn), at its middle.
void(entity e, entity xBy) VR_Status_Ignite =
{
    vector mid = (e.absmin + e.absmax) * 0.5;
    VR_Burn_Ignite(e, xBy, mid, '0 0 1', VR_BURN_TOUCH);
};

void(entity e, string xWhy) VR_Status_Douse =
{
    if(e.burn_fire != world)
    {
        VR_Burn_Out(e.burn_fire, xWhy);
    }
};
