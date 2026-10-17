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
float QVR_WOUND_MELEE = 3; // a blade, a fist, a bite, a claw (dir: the blow's way; extra: 1 blunt, + one of these:)
float QVR_WOUND_MELEE_BLADE = 2;  // a blade in your hand (the engine's bloodstream::Melee * 2: its stream's chance,
float QVR_WOUND_MELEE_BLUNT = 4;  // a blunt blow in your hand              vr_gore_streams_melee_blade, _blunt,
float QVR_WOUND_MELEE_THROWN = 6; // a blade you threw                       _thrown_blade)
float QVR_WOUND_BLAST = 4; // an explosion (org: its centre)
float QVR_WOUND_BURN  = 5; // fire: a lava ball, a lava nail, a laser bolt, a fire's flame (extra 1: a burn alone, no wound under it)
float QVR_WOUND_ZAP   = 6; // lightning
float QVR_WOUND_LAVA  = 7; // standing in lava (org_z: its surface)
float QVR_WOUND_SLIME = 8; // in slime (org_z: its surface)
float QVR_WOUND_WATER = 9; // in a liquid (org_z: its surface; extra: 0 water, 1 slime, 2 lava)
// (10, a weapon thrown from a hand, is no more: a weapon's blood goes with its id, vr_weaponinst.qc.)

.float vr_wound_liquid_next; // when a monster or a player in a liquid is next told about (VR_Wounds_Frame)

float vr_wound_liquid; // set by VR_Wound_Level: the liquid (CONTENT_*)
float vr_wound_sent;   // hits' wound events sent (the small gibs' tests count them)
float vr_wound_lastkind; // the last hit's kind (QVR_WOUND_*; + 0.5: a blunt blow), for the dents' tests

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
    return (e.flags & (FL_MONSTER | FL_CLIENT)) && !VR_Dummy_Clean(e);
};

// A hit's wound (VR_Wound_Hit, VR_Wound_Pellet): as VR_Wound_Takes, and a clean training dummy's too while its dents
// or blood streams are on (vr_dummy_dents, vr_dummy_streams), sent marked QVR_WOUND_CLEAN (the engine paints nothing
// then: the dent and the stream alone, each with its toggle).
float QVR_WOUND_CLEAN = 64; // added to the kind
float(entity e) VR_Wound_TakesHit =
{
    return VR_Wound_Takes(e) ||
           (VR_Dummy_Clean(e) && (cvar_hget(cvarh_vr_dummy_dents) || cvar_hget(cvarh_vr_dummy_streams)));
};
float(entity e) VR_Wound_CleanFlag =
{
    return VR_Dummy_Clean(e) ? QVR_WOUND_CLEAN : 0;
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
    if(!VR_Wound_TakesHit(e) || !e.takedamage)
    {
        return;
    }
    vr_wound_sent++;
    woundevent(e, at, normalize(dir), QVR_WOUND_SHOT + VR_Wound_CleanFlag(e), damage, 1);
};

// From T_DamageImpl, as `take` is taken: the blow's kind, where it struck and which way it went.
void(entity targ, entity inflictor, entity attacker, float take) VR_Wound_Hit =
{
    // (A fire paints its own burns, under its flames: vr_burning.qc.)
    if(take <= 0 || !VR_Wound_TakesHit(targ) || vr_hitkind == QVR_HITKIND_BLEED || inflictor.classname == "vr_burn" ||
       VR_HitBloodless())
    {
        return;
    }

    vector mid = (targ.absmin + targ.absmax) * 0.5;
    if(inflictor == world)
    {
        if(VR_Dummy_Clean(targ))
        {
            return; // (no dent nor stream from a liquid)
        }
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
    float extra = 0;
    string m = inflictor.model;
    // What struck, as the small gibs and the corpses tell it (vr_smallgibs.qc): a blunt thing's blow is a round crater
    // (extra 1: the engine's blunt dent), a blade's a cut drawn out along the swing.
    float sk = VR_SmallGib_Kind(inflictor, attacker);

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
        extra = (sk == QVR_SGIB_BLUNT || sk == QVR_SGIB_PROPS); // a punch, a pommel, a gun, a brick in the fist
        if(vr_hitkind == QVR_HITKIND_MELEE && (attacker.flags & FL_CLIENT))
        {
            extra += extra ? QVR_WOUND_MELEE_BLUNT : QVR_WOUND_MELEE_BLADE; // your hand's blow: its blood stream's chance
        }
    }
    else if((sk == QVR_SGIB_PROPS && !(inflictor.vr_deflect_time > 0)) || sk == QVR_SGIB_BLADES)
    {
        // A thing thrown or flung (a brick, a rock, a crate, a gib, a weapon: ROUND22.md, "Every hit dents"): a blow as
        // a hand's with it, not a nail's (a thrown blade cuts).
        kind = QVR_WOUND_MELEE;
        extra = (sk == QVR_SGIB_PROPS);
        if(sk == QVR_SGIB_BLADES && (attacker.flags & FL_CLIENT))
        {
            extra += QVR_WOUND_MELEE_THROWN; // a blade you threw: its blood stream's chance
        }
    }
    else if(vr_hitkind == QVR_HITKIND_SHOT)
    {
        kind = QVR_WOUND_ZAP; // a hitscan shot that is not pellets: lightning
    }
    else if(m == "progs/lavaball.mdl" || inflictor.classname == "lava_spike" || m == "progs/laser.mdl")
    {
        kind = QVR_WOUND_BURN;
    }
    else if(inflictor.flags & (FL_MONSTER | FL_CLIENT))
    {
        kind = QVR_WOUND_MELEE; // a monster's claw, bite or blade (the shambler's lightning: from afar)
        if(VR_IsShambler(inflictor) && vlen(inflictor.origin - targ.origin) > 120)
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
    vr_wound_sent++;
    vr_wound_lastkind = kind + (extra & 1) * 0.5; // (the dents' tests: vr_dents_test.qc)
    woundevent(targ, org, normalize(dir), kind + VR_Wound_CleanFlag(targ), min(255, take), extra);
};

// StartFrame: every monster and player in a liquid, four times a second: how high it comes up them (wet under it;
// lava and slime burn only as they hurt: VR_Wound_Hit). The due monsters and players alone are stepped through (findflagsdue:
// VR_Wound_Takes's flags), the cheap checks first; the dummy's time is set as the others' (it takes none with vr_dummy_gore off).
void() VR_Wounds_Frame =
{
    entity e = findflagsdue(world, flags, FL_MONSTER | FL_CLIENT, vr_wound_liquid_next, time, modelindex);
    while(e)
    {
        e.vr_wound_liquid_next = time + 0.25;
        if(!VR_Dummy_Clean(e))
        {
            float level = VR_Wound_Level(e);
            if(level > 0)
            {
                vector at = (e.absmin + e.absmax) * 0.5;
                at_z = VR_Wound_LiquidTop(e);
                woundevent(e, at, '0 0 -1', QVR_WOUND_WATER, 0, vr_wound_liquid == CONTENT_WATER ? 0 : (vr_wound_liquid == CONTENT_SLIME ? 1 : 2));
            }
        }
        e = findflagsdue(e, flags, FL_MONSTER | FL_CLIENT, vr_wound_liquid_next, time, modelindex);
    }
};
