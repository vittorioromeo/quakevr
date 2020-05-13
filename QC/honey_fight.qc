/*
===========
OgreMarksmanCheckAttack

The player is in view, so decide to move or launch an attack
Returns FALSE if movement should continue
============
*/
float() OgreMarksmanCheckAttack =
{
    vector    spot1, spot2;
    entity    targ;
//    float        chance;

    if(enemy_range == RANGE_MELEE)
    {
        if(CanDamage(self.enemy, self))
        {
            self.attack_state = AS_MELEE;
            return TRUE;
        }
    }

    if(time < self.attack_finished)
        return FALSE;

    if(!enemy_vis)
        return FALSE;

    targ = self.enemy;

// see if any entities are in the way of the shot
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;

    traceline (spot1, spot2, FALSE, self);

    if(trace_inopen && trace_inwater)
        return FALSE;            // sight line crossed contents

    if(trace_ent != targ)
    {
        return FALSE;    // don't have a clear shot
    }

// missile attack
    if(time < self.attack_finished)
        return FALSE;

    self.attack_state = AS_MISSILE;
    SUB_AttackFinished (2 + 2*random());
    return TRUE;
};
