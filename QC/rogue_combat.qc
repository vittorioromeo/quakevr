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

    while (head)
    {
        if(head.takedamage)
        {
            org = head.origin + (head.mins + head.maxs)*0.5;
            points = 0.5*vlen(inflictor.origin - org);
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
                        T_Damage (head, inflictor, attacker, points);
                }
            }
        }
        head = head.chain;
    }
};
