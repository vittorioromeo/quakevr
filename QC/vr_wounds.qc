// ----------------------------------------------------------------------------
// VR: wounds painted on models (the engine's vr_wounds.cpp: vr_wounds, vr_wounds_burns, vr_wounds_wet).
//
// The server tells every client where a monster or a player was hit, by what kind of blow, and how hard; the
// clients paint it onto the model's own skin, in its texture space (blood, burns, wetness), where the model is at
// that moment. Sent for every hit that takes health from a monster or a player (T_DamageImpl), each pellet of a
// shotgun blast on its own (TraceAttack), and, a few times a second, for everyone standing in a liquid (the surface's
// height: what is under it gets wet, or burnt by lava, or eaten by slime). Unreliable, as particles are: a lost one
// only misses a mark. Nothing here uses random(): the game plays the same with or without them.
// ----------------------------------------------------------------------------

float QVR_WOUND_SHOT  = 1; // a bullet or a pellet (extra: pellets in one event)
float QVR_WOUND_NAIL  = 2; // a nail or a small missile
float QVR_WOUND_MELEE = 3; // a blade, a fist, a bite, a claw (dir: the blow's way)
float QVR_WOUND_BLAST = 4; // an explosion (org: its centre)
float QVR_WOUND_BURN  = 5; // fire: a lava ball, a lava nail, a laser bolt
float QVR_WOUND_ZAP   = 6; // lightning
float QVR_WOUND_LAVA  = 7; // standing in lava (org_z: its surface)
float QVR_WOUND_SLIME = 8; // in slime (org_z: its surface)
float QVR_WOUND_WATER = 9; // in a liquid (org_z: its surface; extra: 0 water, 1 slime, 2 lava)

.float vr_wound_liquid_next; // when a monster or a player in a liquid is next told about (VR_Wounds_Frame)

float vr_wound_liquid; // set by VR_Wound_Level: the liquid (CONTENT_*)

// How deep `e` is in a liquid: a player's waterlevel (the engine's), a monster's measured the same way (vr_liquids.qc:
// the engine keeps none for walking monsters).
float(entity e) VR_Wound_Level =
{
    if(e.flags & FL_CLIENT)
    {
        vr_wound_liquid = e.watertype;
        return e.waterlevel;
    }
    float level = VR_Liquid_Level(e);
    vr_wound_liquid = vr_liquid_type;
    return level;
};

float(entity e) VR_Wound_Takes =
{
    return (e.flags & (FL_MONSTER | FL_CLIENT)) && e.classname != "vr_dummy";
};

// The height of the surface of the liquid `e` is in (it is, waterlevel > 0), or over its top when it is under.
float(entity e) VR_Wound_LiquidTop =
{
    vector top = e.origin + '0 0 1' * (e.maxs_z + 1);
    vector at = liquidentry(top, e.origin + '0 0 1' * (e.mins_z + 1));
    if(at == top)
    {
        return top_z + 64; // under it (or no crossing found: the whole of it)
    }
    return at_z;
};

// Each pellet of a shotgun blast that struck `e` (TraceAttack): where, going which way.
void(entity e, vector at, vector dir, float damage) VR_Wound_Pellet =
{
    if(!VR_Wound_Takes(e) || !e.takedamage)
    {
        return;
    }
    woundevent(e, at, normalize(dir), QVR_WOUND_SHOT, damage, 1);
};

// From T_DamageImpl, as `take` is taken: the blow's kind, where it struck and which way it went.
void(entity targ, entity inflictor, entity attacker, float take) VR_Wound_Hit =
{
    if(take <= 0 || !VR_Wound_Takes(targ))
    {
        return;
    }

    vector mid = (targ.absmin + targ.absmax) * 0.5;
    if(inflictor == world)
    {
        // The liquid it is in: burnt by lava, eaten by slime (drowning, falls: nothing to see).
        float level = VR_Wound_Level(targ);
        if(level > 0 && (vr_wound_liquid == CONTENT_LAVA || vr_wound_liquid == CONTENT_SLIME))
        {
            vector surf = mid;
            surf_z = VR_Wound_LiquidTop(targ);
            woundevent(targ, surf, '0 0 -1', vr_wound_liquid == CONTENT_LAVA ? QVR_WOUND_LAVA : QVR_WOUND_SLIME,
                min(255, take), level);
        }
        return;
    }

    // A shotgun blast's pellets were each sent as they struck (VR_Wound_Pellet).
    if(vr_hitpellets > 0)
    {
        return;
    }

    vector org = mid;
    vector dir = vr_hitdir;
    float kind = QVR_WOUND_NAIL;
    string m = inflictor.model;

    if(vr_hitkind == QVR_HITKIND_SPLASH || IsExplosionDamage)
    {
        kind = QVR_WOUND_BLAST;
        org = inflictor.origin;
        dir = mid - inflictor.origin;
    }
    else if(vr_hitkind == QVR_HITKIND_MELEE || vr_hitkind == QVR_HITKIND_HEADBUTT || vr_hitkind == QVR_HITKIND_BASH ||
            vr_hitkind == QVR_HITKIND_SHOVE)
    {
        kind = QVR_WOUND_MELEE;
    }
    else if(vr_hitkind == QVR_HITKIND_SHOT)
    {
        kind = QVR_WOUND_ZAP; // a hitscan shot that is not pellets: lightning
    }
    else if(m == "progs/lavaball.mdl" || inflictor.classname == "lava_spike" || m == "progs/laser.mdl" ||
            inflictor.classname == "vr_torch_burn") // (a torch's fire: vr_walltorch.qc)
    {
        kind = QVR_WOUND_BURN;
    }
    else if(inflictor.flags & (FL_MONSTER | FL_CLIENT))
    {
        kind = QVR_WOUND_MELEE; // a monster's claw, bite or blade (the shambler's lightning: from afar)
        if(inflictor.classname == "monster_shambler" && vlen(inflictor.origin - targ.origin) > 120)
        {
            kind = QVR_WOUND_ZAP;
        }
    }

    if(kind != QVR_WOUND_BLAST)
    {
        if(vr_hitpos != '0 0 0' && vlen(vr_hitpos - mid) < 64)
        {
            org = vr_hitpos;
        }
        else if(inflictor != targ && !(inflictor.flags & (FL_MONSTER | FL_CLIENT)) && vlen(inflictor.origin - mid) < 64)
        {
            org = inflictor.origin; // a missile where it touched
        }
        if(dir == '0 0 0' && inflictor != targ)
        {
            if(inflictor.velocity != '0 0 0' && !(inflictor.flags & (FL_MONSTER | FL_CLIENT)))
            {
                dir = inflictor.velocity; // a missile's flight
            }
            else
            {
                dir = mid - (inflictor.absmin + inflictor.absmax) * 0.5;
            }
        }
    }
    if(dir == '0 0 0')
    {
        dir = '0 0 -1';
    }
    woundevent(targ, org, normalize(dir), kind, min(255, take), 0);
};

// StartFrame: every monster and player in a liquid, four times a second: how high it comes up them (wet under it;
// lava and slime burn only as they hurt: VR_Wound_Hit).
void() VR_Wounds_Frame =
{
    entity e = nextent(world);
    while(e)
    {
        if(e.vr_wound_liquid_next <= time && VR_Wound_Takes(e) && e.modelindex)
        {
            e.vr_wound_liquid_next = time + 0.25;
            float level = VR_Wound_Level(e);
            if(level > 0)
            {
                vector at = (e.absmin + e.absmax) * 0.5;
                at_z = VR_Wound_LiquidTop(e);
                woundevent(e, at, '0 0 -1', QVR_WOUND_WATER, 0, vr_wound_liquid == CONTENT_WATER ? 0 : (vr_wound_liquid == CONTENT_SLIME ? 1 : 2));
            }
        }
        e = nextent(e);
    }
};
