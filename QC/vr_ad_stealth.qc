// AD support: Quake VR's stealth AI (vr_stealth.qc; STEALTH.md, "Arcane Dimensions") on Arcane Dimensions' monsters.
// AD's monsters run AD's own AI (ad_ai_subs.qc, ad_ai_states.qc): its FindTarget, stand, walk and run call these
// (ad_import.py FUNC_PATCHES), and vr_stealth.qc asks these for AD's thinks, path fields and FoundTarget (stl_ad).
// Declared early in vr_ad_mondefs.qc; after AD's files here (AD's names).

float(entity m) VR_AD_StealthKind =
{
    if(!m.ad_th_run && !m.ad_th_stand)
    {
        return 0;
    }
    // (bosses, flyers and swimmers keep AD's AI, as Quake VR's keep Quake's)
    if(!m.ad_th_stand || !m.ad_th_walk || m.ad_bossflag || m.ad_classmove == ad_MON_MOVEFLY ||
       m.ad_classmove == ad_MON_MOVESWIM)
    {
        return -1;
    }
    if(m.ad_movespeed < 0 || m.ad_classmove == ad_MON_MOVESTATIC || m.ad_th_walk == m.ad_th_stand)
    {
        return 2; // (a turret: it turns and searches where it stands)
    }
    return 1;
};

float(entity m) VR_AD_StealthPassive =
{
    return m.ad_passive_state == ad_MON_PASSIVESTATE_ON;
};

void(float xWalking) VR_AD_StealthAnim =
{
    if(xWalking)
    {
        self.ad_th_walk();
        return;
    }
    self.ad_pausetime = 99999999;
    self.ad_th_stand();
};

entity(entity m) VR_AD_StealthPath =
{
    return m.ad_movetarget;
};

void(entity m, entity xPath) VR_AD_StealthSetPath =
{
    m.ad_movetarget = xPath;
};

float(entity m) VR_AD_StealthOnPath =
{
    return m.ad_movetarget.ad_classtype == ad_CT_PATHCORNER && m.ad_pausetime < 99999999;
};

void() VR_AD_StealthFound =
{
    ad_FoundTarget();
};

void(entity m) VR_AD_StealthVoice =
{
    if(m.ad_idle_sound != "")
    {
        sound(m, CHAN_VOICE, m.ad_idle_sound, 0.7, ATTN_NORM);
    }
};

void(entity m) VR_AD_StealthTestReset =
{
    m.ad_oldenemy = m.ad_movetarget = world;
    m.ad_pausetime = 99999999;
    ad_sight_entity = ad_sightgroup_entity = world;
    ad_sight_entity_time = ad_sightgroup_entity_time = -100;
};

void() VR_AD_StealthTestUse =
{
    ad_activator = activator;
    ad_monster_use();
};

// AD's FindTarget, at its start: Quake VR's tick (the meter drains; beams, torches and bodies seen). TRUE: it went
// Hostile in it, AD's FindTarget is done.
float() VR_AD_StealthTick =
{
    entity was = self.enemy;
    VR_Stealth_Tick();
    return was == world && self.enemy != world;
};

// AD's FindTarget: a player (not AD's sight relay: a monster that saw him, or a sight target's group) is seen by the
// meter, not at once.
float(entity client) VR_AD_StealthSight =
{
    if(client.classname != "player" || !VR_Stealth_MeterOn())
    {
        return FALSE;
    }
    return !(client == ad_sight_entity && ad_sight_entity_time >= time - 0.1 && !(self.spawnflags & ad_MON_AMBUSH));
};

float(entity client) VR_AD_StealthSee =
{
    return VR_Stealth_See(client);
};

void() VR_AD_StealthHostile =
{
    VR_Stealth_Hostile();
};

float() VR_AD_StealthStand =
{
    return VR_Stealth_Stand();
};

float(float dist) VR_AD_StealthWalk =
{
    return VR_Stealth_Walk(dist);
};

// AD's ai_run: lost him a while: Alert (vr_stealth_lose), AD's old enemy forgotten too.
float(float xVis) VR_AD_StealthLose =
{
    if(!VR_Stealth_Lose(xVis))
    {
        return FALSE;
    }
    self.ad_oldenemy = world;
    return TRUE;
};
