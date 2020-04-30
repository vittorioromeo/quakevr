void HuntTarget ();

//- - - - - - - - -
// Like checkclient, but for monsters.
// Checks if a given target is available for attacking.
// The entity targ is supposed to be the current entity on a given hitlist.
entity(entity targ) CheckTarget =
{
    if (targ.health > 0)
        if (visible (targ))
            return targ;
    return world;
};

//- - - - - - - - -
// SOW = Stand or walk.  Called at the end of a victory dance.
void() monster_sow =
{
    if (self.movetarget)
        self.th_walk ();
    else
        self.th_stand ();
};

//- - - - - - - - -
// Tells the monster to go straight up, if distance is positive.
// A negative distance will make the monster go down instead.
// Returns TRUE if fully or partially successful at moving.
//
// Originally from Dragons.
//- - - - - - - - -
float(float dist) ai_up =
{
    local   entity  box, os;
    local   entity  swap;
    local   vector  spot;

    if (dist < 0)
    {
        dist = 0 - dist;    // 'fabs' it.
        temp_goal.origin_z = self.origin_z - 40 - dist;     // Going down.
    }
    else
        temp_goal.origin_z = self.origin_z - 30 + dist;     // Going up.
    setorigin (temp_goal, temp_goal.origin);

    spot = self.origin;

    swap = self.enemy;
    self.enemy = temp_goal;
    while (dist > 0)
    {
        if (dist < 8)   // walkmove moves self up or down 8 units at a time.
        {   // Last run through loop.
            // Go through the trouble of moving less than 8 units.
            // Create a dummy to move for the monster.  If the dummy
            // can move, it is safe to set origin directly for monster.
            box = spawn();
            box.oldorigin   = self.origin;
            box.angles      = self.angles;
            box.solid       = self.solid;
            box.movetype    = self.movetype;
            box.flags       = self.flags & FLx_NOT_FALLING;
            box.enemy       = self.enemy;
            box.owner       = self;     // Pass through monster.
            setsize (box, self.mins, self.maxs);
            setorigin (box, self.origin);
            os = self;
            self = box;
            if (safe_move (self.angles_y, 0))
            {   // Dummy moved, now move the monster precisely.
                if (self.origin_z > self.oldorigin_z)
                {   // Going up.
                    os.origin_z = os.origin_z + dist;
                    setorigin (os, os.origin);
                }
                else if (self.origin_z < self.oldorigin_z)
                {   // Going down.
                    os.origin_z = os.origin_z - dist;
                    setorigin (os, os.origin);
                }
            }
            remove (box);
            self = os;
        }
        else
            safe_move (self.angles_y, 0);    // Fails only in solid.

        dist = dist - 8;
    }
    self.enemy = swap;

    return (self.origin != spot);
};

float(float dist) ai_down = {return ai_up (0 - dist);};

// Changes mode from fight to idle.  Does not actually change thinking.
void() ai_calm_down =
{
// Reset fight status.
    self.fighting     = FALSE;
    self.search_time  = time + 3;

// Forget all enemies and go idle.
    self.enemy = self.oldenemy = world;
};

// Check if self should stop running after enemy.
float() ai_run_kill =
{
    local   float   winner;

// Check for victory sound.  Must do first because enemy will be changed.
// Sound is played only if enemy slain is a client.
    winner = FALSE;
    if (self.enemy)
    if (self.enemy.classname == "player")
    if (visible (self.enemy))
    {
        winner = TRUE;
        // Set yaw in case we need to face the fallen during the dance.
        self.ideal_yaw = vectoyaw (self.enemy.origin - self.origin);
    }

    self.enemy = world;
// FIXME: look all around for other targets
    if (self.oldenemy.health > 0)
    {
        self.enemy = self.oldenemy;
        self.oldenemy = world;
        // FIXME:  Replace with simpler version without the thinking.
        HuntTarget ();
    }
    else
    {
        ai_calm_down ();
        if (winner)
            if (self.th_win)
                {self.th_win ();  return TRUE;}   // We have a winner!
        monster_sow ();
        return TRUE;
    }
    return FALSE;
};

// Monster is done running and going back to stand/walk.
void() ai_run_done = {
    ai_calm_down ();
    monster_sow ();
};

// Same as W_ShowHostile, minus the quad sound.  Some monsters can abort
// attacks in their attack functions so don't blindly play the quad sound.
void() M_ShowHostile = //{self.show_hostile = time + 1;};
{
    self.show_hostile = time + 1;
    //self.attack_time = time;
};

//=============
// ai_run_bank -- Custom strafing AI for Dragons!
//
// Move to the side of ideal_yaw.  Assumes ideal_yaw was set prior to a
// call to this function.  While similar to 'ai_run_slide', this function
// has two important differences.
//
// * Only one move attempt is made.  If it fails, it does not get a second
//   try to move to the other side.
//
// * If the move fails, the 'lefty' value is toggled only if 'self' was
//   leaning toward the side it tried to move to.
//
// This is used to help provide the illusion of a momentum change.
//=============
void(float dist) ai_run_bank = {
    local   float   ofs, flip;
    
    if (self.lefty)
        {ofs = 90;  flip = (self.angles_z < 0);}
    else
        {ofs = -90;  flip = (self.angles_z > 0);}

    if (!safe_move (self.ideal_yaw + ofs, dist))  // Do this first to move!
        if (flip)
            self.lefty = !self.lefty;
};

// Adds 'fadd' to frame 'fr'.  Used to animate monsters.
void(float fr, float fadd, float fmax) WalkFrame_Add =
{
    if ((self.walkframe < 0) || (self.walkframe > fmax))
        self.walkframe = 0;
    self.frame = fr + self.walkframe;
    self.walkframe = self.walkframe + fadd;
};