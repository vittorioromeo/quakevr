float(float yaw, float dist) flat_move;

//==========================================================================
//  Doom Movement -- Used by the cyberdemon from Doom/II.

float(float yaw, float dist) doom_step =
{
    if (QC_StepDir (yaw, dist))
    {
        self.movecount = prandom() & 15;
        return TRUE;
    }
    self.movecount = 0;     // Not in Doom's p_enemy.c.
    return FALSE;
};

// Based on 'SV_NewChaseDir'.
void(float dist) doom_chase =
{
    local   vector  delta;
    local   float   d1, d2;
    local   float   old, back, turn;

    old = floor (self.ideal_yaw / 45);  // IIRC, int chops off decimals.
    old = anglemod (old * 45);
    back = anglemod (old - 180);

    delta = self.goalentity.origin - self.origin;
    if (delta_x > 10)
        d1 = 0;
    else if (delta_x < -10)
        d1 = 180;
    else
        d1 = -1;
    if (delta_y < -10)
        d2 = 270;
    else if (delta_y > 10)
        d2 = 90;
    else
        d2 = -1;

// Try direct route.
    if (d1 != -1)
        if (d2 != -1)
    {
        if (d1 == 0)
            {if (d2 == 90) turn = 45; else turn = 315;}
        else
            {if (d2 == 90) turn = 135; else turn = 215;}

        if (turn != back)
            if (doom_step (turn, dist))
                return;
    }

// Try other directions.
    local   float   ax, ay;

    ax = fabs(delta_x);
    ay = fabs(delta_y);
    // Note:  Quake uses 1/3, Doom uses 200/255 (0.2156862745).
    if ((random() < 0.2157) || (ay > ax))
        {turn = d1; d1 = d2; d2 = turn;}

    if (d1 != -1)
        if (d1 != back)
            if (doom_step (d1, dist))
                return;
    if (d2 != -1)
        if (d2 != back)
            if (doom_step (d2, dist))
                return;

// No direct path to goal, so pick another way.
    if (old != -1)
        if (doom_step (old, dist))
            return;

    local   float   end, next;
    if (random() < 0.5)
        {turn = 0; end = 315; next = 45;}
    else
        {turn = 315; end = 0; next = -45;}

    while (turn <= end)
    {
        if (turn != back)
            if (doom_step (turn, dist))
                return;
        turn = turn + next;
    }

    if (back != -1)
        if (doom_step (back, dist))
            return;

// Can't move.
    self.ideal_yaw = old;

// Pull out code.
    flat_move (0, 0);   // This will do the 'SV_FixCheckBottom' code.
};

void(float dist) doom_goal =
{
    local   vector  org;

    org = self.origin;
    self.movecount = 0;
    if (self.flags & FLx_NOT_FALLING)
    {   // Bump around...
// DISABLED -- Not useful if not following Doom code exactly.
// I do not fully understand Doom code, so wing it and use Quake code.
//        self.movecount = self.movecount - 1;
//        if (self.movecount < 0)     // Was (random() < 0.333)
        if (random() < 0.333)
            doom_chase (dist);
        else if (!doom_step (self.ideal_yaw, dist))
            doom_chase (dist);
//        dprint(ftos(self.movecount)); dprint ("\n");
        return;
    }
};


//===========
// DoomCheckAttack
//
// The player is in view, so decide to move or launch an attack
// Returns FALSE if movement should continue
//
// Used by monsters originally from the classic Doom games,
// such as the cyberdemon.  Tries to emulate some of the Doom AI.
//============
void() DoomCheckAttack = {
// DISABLED -- Monsters in Doom can attack from any range.
//    if (enemy_range == RANGE_FAR)
//        return FALSE;   // Don't call traceline again when we don't need to.
    if (!self.enemy.modelindex)
        return;   // Enemy phased out of the level.

    local   vector  spot1, spot2;
    local   entity  targ;
    local   float   dist;

    targ = self.enemy;
    
// see if any entities are in the way of the shot
    spot1 = self.origin + self.attack_offset;
    spot2 = targ.origin + targ.attack_offset;

// Doom monsters don't care who is in the way.
    traceline (spot1, spot2, TRUE, self);
    if (trace_fraction < 1)
        return;   // Blocked by a wall.

    // PM:  Added alert support so that some monsters can see and attack
    //      through water like players can.
    // FIXME:  Is the 'fish check' necessary here?
    if (trace_inopen)
        if (trace_inwater)
            if (!(self.ryuflags & RYU_ALERT))   // PM:  Alert support.
                return;            // sight line crossed contents

    if (enemy_range == RANGE_MELEE)
        if (self.th_melee)
            {self.th_melee ();  return;}

// missile attack
    if (!self.th_missile)
        return;
    if (self.ryuflags & RYU_JUSTHIT)    // TRUE after recovery from pain frame.
    {
        self.ryuflags = self.ryuflags - RYU_JUSTHIT;
        self.movecount = 1;     // See end of function for explanation.
        self.th_missile ();
        return;
    }
    if (time < self.attack_finished)    // Used as reactiontime here.
        return;
    if (self.movecount)
        if (skill < 3)
            return;       // Not allowed to attack now.

// FIXME:  Add global in range function.
    dist = vlen(spot2 - spot1);
    if (!self.th_melee)
        dist = dist - 128;
    dist = fabs(dist);      // Is 'dist >>= 16' in Doom's p_enemy.c.

/* DISABLED -- These monsters don't exist in Quake.
    if (self.classname == "monster_archvile")
        if (dist > 896)     //14*64
            return FALSE;
    else if (self.classname == "monster_revenant")
    {
        if (dist < 196)
            return FALSE;
        dist = dist / 2;
    }
    else if ((self.classname == "monster_spiderdemon") || (self.classname == "monster_lostsoul"))
        dist = dist / 2;
*/

    if (self.classname == "monster_cyberdemon")
    {   dist = dist / 2;
        if (dist > 160)
            dist = 160;
    }
    else
    {   if (dist > 200)
            dist = 200;
    }

    if (prandom() < dist)
        return;

    if (self.attack_offset != VEC_ORIGIN)
    {   // Make sure missile can hit the target from where it spawns.
        // Was not applicable in Doom, but it is here!
        makevectors (self.angles);
        spot1 = self.origin + (v_forward*self.attack_offset_x)
            + (v_right*self.attack_offset_y) + (v_up*self.attack_offset_z);
        traceline (spot1, spot2, TRUE, self);
        if (trace_fraction < 1)
            return;   // Blocked by a wall.
    }

// attack_finished does not exist in Doom, so don't set it!
// In fact, attack_finished is used as reactiontime for Doom monsters.
    // In Quake, attack is checked before movement, so set movecount
    // to ensure movement checks first.  Hack-ish and not ideal, but
    // works for me.
    self.movecount = 1;
    self.th_missile ();
    return;
};


//===========================/  END OF FILE  /===========================//
