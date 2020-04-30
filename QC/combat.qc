// combat.qc

float IsExplosionDamage;

void() T_MissileTouch;
void() info_player_start;
void(entity targ, entity attacker) ClientObituary;

void() monster_death_use;

//============================================================================


// TODO VR: (P2) rogue
// ====
// Function prototype for shield impact handler.
// ====
// float(entity targ, entity inflictor, entity attacker, float damage) shield_hit;

void(vector org, float damage) SpawnBloodSplash;


float() getVRMeleeBloodlust =
{
    return cvar("vr_melee_bloodlust");
}

/*
============
T_EELZap
============
*/
void(entity inflictor, entity attacker, float damage) T_EELZap =
{
    local    float     points;
    local    entity    head;
    local    vector    org;

    head = findradius(inflictor.origin, damage+40);

    while(head)
    {
        if(head.takedamage)
        {
            org = head.origin + (head.mins + head.maxs)*0.5;
            points = 0.5*vlen (inflictor.origin - org);
            if(points < 0)
                points = 0;
            points = damage - points;
            if(head == attacker)
                points = points * 0.5;
            if(points > 0)
            {
                if(CanDamage (head, inflictor))
                {    // eels take no damage from this attack
                    if(head.classname != "monster_eel" &&
                        (head.flags & FL_INWATER))
                        T_Damage(head, inflictor, attacker, points);
                }
            }
        }
        head = head.chain;
    }
};

/*
============
T_BeamDamage
============
*/
void(entity attacker, float damage) T_BeamDamage =
{
    local    float     points;
    local    entity    head;

    head = findradius(attacker.origin, damage+40);

    while(head)
    {
        if(head.takedamage)
        {
            points = 0.5*vlen (attacker.origin - head.origin);
            if(points < 0)
                points = 0;
            points = damage - points;
            if(head == attacker)
                points = points * 0.5;
            if(points > 0)
            {
                if(CanDamage (head, attacker))
                {
                    if(head.classname == "monster_shambler")
                        T_Damage(head, attacker, attacker, points*0.5);
                    else
                        T_Damage(head, attacker, attacker, points);
                }
            }
        }
        head = head.chain;
    }
};
