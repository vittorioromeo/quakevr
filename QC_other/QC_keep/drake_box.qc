//==========================================================================
//  Point Contents Checks

//------------------------------------------------------------------------//
//  This checks if a bbox is inside a wall or sky.  The bbox's origin
//  and corners are checked.  If any one points is within a solid or sky,
//  TRUE is returned.
//------------------------------------------------------------------------//
float(vector vmin, vector vmax, vector spot) BoxInWall =
{
    local   vector  p;
    local   float   content;

    // Check origin.
    p = spot;
    content = pointcontents(p);
    if (content == CONTENT_SOLID)  return TRUE;
    if (content == CONTENT_SKY)    return TRUE;

    // Check bbox faces.
    // Order:  1)-00  2)+00  3)0-0  4)0+0  5)00-  6)00+
    p_x = spot_x + vmin_x;
    content = pointcontents(p);
    if (content == CONTENT_SOLID)  return TRUE;
    if (content == CONTENT_SKY)    return TRUE;

    p_x = spot_x + vmax_x;
    content = pointcontents(p);
    if (content == CONTENT_SOLID)  return TRUE;
    if (content == CONTENT_SKY)    return TRUE;

    p = spot;
    p_y = spot_y + vmin_y;
    content = pointcontents(p);
    if (content == CONTENT_SOLID)  return TRUE;
    if (content == CONTENT_SKY)    return TRUE;

    p_y = spot_y + vmax_y;
    content = pointcontents(p);
    if (content == CONTENT_SOLID)  return TRUE;
    if (content == CONTENT_SKY)    return TRUE;

    p = spot;
    p_z = spot_z + vmin_z;
    content = pointcontents(p);
    if (content == CONTENT_SOLID)  return TRUE;
    if (content == CONTENT_SKY)    return TRUE;

    p_z = spot_z + vmax_z;
    content = pointcontents(p);
    if (content == CONTENT_SOLID)  return TRUE;
    if (content == CONTENT_SKY)    return TRUE;

    // Check bbox corners.
    // Order:  1)+++  2)-++  3)+-+  4)--+  5)++-  6)-+-  7)+--  8)---
    p_x = spot_x + vmax_x;
    p_y = spot_y + vmax_y;
    content = pointcontents(p);
    if (content == CONTENT_SOLID)  return TRUE;
    if (content == CONTENT_SKY)    return TRUE;

    p_x = spot_x + vmin_x;
    content = pointcontents(p);
    if (content == CONTENT_SOLID)  return TRUE;
    if (content == CONTENT_SKY)    return TRUE;

    p_x = spot_x + vmax_x;
    p_y = spot_y + vmin_y;
    content = pointcontents(p);
    if (content == CONTENT_SOLID)  return TRUE;
    if (content == CONTENT_SKY)    return TRUE;

    p_x = spot_x + vmin_x;
    content = pointcontents(p);
    if (content == CONTENT_SOLID)  return TRUE;
    if (content == CONTENT_SKY)    return TRUE;

    p_x = spot_x + vmax_x;
    p_y = spot_y + vmax_y;
    p_z = spot_z + vmin_z;
    content = pointcontents(p);
    if (content == CONTENT_SOLID)  return TRUE;
    if (content == CONTENT_SKY)    return TRUE;

    p_x = spot_x + vmin_x;
    content = pointcontents(p);
    if (content == CONTENT_SOLID)  return TRUE;
    if (content == CONTENT_SKY)    return TRUE;

    p_x = spot_x + vmax_x;
    p_y = spot_y + vmin_y;
    content = pointcontents(p);
    if (content == CONTENT_SOLID)  return TRUE;
    if (content == CONTENT_SKY)    return TRUE;

    p_x = spot_x + vmin_x;
    content = pointcontents(p);
    if (content == CONTENT_SOLID)  return TRUE;
    if (content == CONTENT_SKY)    return TRUE;

    return FALSE;
};

//------------------------------------------------------------------------//
//  This traces lines through a cube in space, in an attempt to find
//  solids such as creatures occupying the space inside said cube.
//  Returns TRUE if space is clear, FALSE if solid is found.
//------------------------------------------------------------------------//
float(vector vmin, vector vmax, vector spot, entity ignore) BoxInThing =
{
    local   vector  p1, p2;

// Check inside the bbox.
    p1 = vmin;
    p2 = vmax;
    traceline (spot + p1, spot + p2, FALSE, ignore);
    if (trace_ent)
        return TRUE;

// Check along the bbox boundaries, upwards and counter-clockwise.
    p2_y = vmin_y;
    traceline (spot + p1, spot + p2, FALSE, ignore);
    if (trace_ent)
        return TRUE;
    p1_x = vmax_x;
    p2_y = vmax_y;
    traceline (spot + p1, spot + p2, FALSE, ignore);
    if (trace_ent)
        return TRUE;
    p1_y = vmax_y;
    p2_x = vmin_x;
    traceline (spot + p1, spot + p2, FALSE, ignore);
    if (trace_ent)
        return TRUE;
    p1_x = vmin_x;
    p2_y = vmin_y;
    traceline (spot + p1, spot + p2, FALSE, ignore);
    if (trace_ent)
        return TRUE;

// Clear
    return FALSE;
};


//==========================================================================
//  Entry Points

// FIXME:  Add a trace that checks walls only, instead of everything.

//------------------------------------------------------------------------//
//  This checks if the path near the desired spot is blocked by
//  walls or monsters.
//------------------------------------------------------------------------//
float(vector spot) EntryPoint_Trace =
{
    traceline (self.enemy.origin, spot, FALSE, self.enemy);
    if (trace_fraction < 1)
    {
    // Unless the entity hit is the one doing the teleporting,
    // consider the spot blocked.
        if (trace_ent != self.owner)
            return TRUE;
    // Teleporting ent was hit, now check through it for more entities.
        traceline (trace_endpos, spot, FALSE, self.owner);
        if (trace_fraction < 1)
            return TRUE;
    }

    return FALSE;
};

//------------------------------------------------------------------------//
//  This traces lines from the beacon entity (self.enemy) to all eight
//  bbox points around the destination.
//------------------------------------------------------------------------//
float() EntryPoint_Path =
{
    local   vector  spot, p;

    // Check origin.
    spot = self.origin;
    p = spot;
    if (EntryPoint_Trace (p))
        return TRUE;

    // Check bbox corners.
    // Order:  1)+++  2)-++  3)+-+  4)--+  5)++-  6)-+-  7)+--  8)---
    p_x = spot_x + self.maxs_x;
    p_y = spot_y + self.maxs_y;
    p_z = spot_z + self.maxs_z;
    if (EntryPoint_Trace (p))
        return TRUE;

    p_x = spot_x + self.mins_x;
    if (EntryPoint_Trace (p))
        return TRUE;

    p_x = spot_x + self.maxs_x;
    p_y = spot_y + self.mins_y;
    if (EntryPoint_Trace (p))
        return TRUE;

    p_x = spot_x + self.mins_x;
    if (EntryPoint_Trace (p))
        return TRUE;

    p_x = spot_x + self.maxs_x;
    p_y = spot_y + self.maxs_y;
    p_z = spot_z + self.mins_z;
    if (EntryPoint_Trace (p))
        return TRUE;

    p_x = spot_x + self.mins_x;
    if (EntryPoint_Trace (p))
        return TRUE;

    p_x = spot_x + self.maxs_x;
    p_y = spot_y + self.mins_y;
    if (EntryPoint_Trace (p))
        return TRUE;

    p_x = spot_x + self.mins_x;
    if (EntryPoint_Trace (p))
        return TRUE;

    return FALSE;
};

//------------------------------------------------------------------------//
//  Checks if a given point is clear for occupation.
//------------------------------------------------------------------------//
float(vector spot) EntryPoint_Test =
{
// Move the dummy to a test point.
    setorigin (self, spot);
    self.flags = self.flags - (self.flags & FL_ONGROUND);

// Test the point.
    if (self.flags & (FL_FLY | FL_SWIM))
    {
        // Note:  These only detect walls, not creatures.
        if (BoxInWall (self.mins, self.maxs, spot))
            return FALSE;

        if (self.flags & FL_SWIM)
            if (pointcontents(spot) > CONTENT_WATER)
                return FALSE;   // Fish must be in the water.

        // The past two checks look for walls, not creatures.
        // This check will hit both walls and creature.
//        traceline (spot + self.mins, spot + self.maxs, FALSE, self.owner);
//        if (trace_ent)
        if (BoxInThing (self.mins, self.maxs, spot, self.owner))
            return FALSE;
    }
    else
    {
        droptofloor ();
        if (!walkmove (0,0))
            return FALSE;
    }

// Found a viable entry point, so now check if a clear path exists between
// it and its beacon.  (If none exists, then skip this check.)
    if (self.enemy)
        if (EntryPoint_Path ())
            return FALSE;

// Got one!
    return TRUE;
};

//------------------------------------------------------------------------//
//  Selects one of twelve points equidistant from the beacon.
//
//  Used for summoning a monster to its master.
//------------------------------------------------------------------------//
float() EntryPoint_Circle =
{
    if (!self.enemy)
        return FALSE;   // No beacon.

// Spawn at one of twelve points around the master.
    local   float   cos0, cos30, cos60;
    local   vector  spot, org;

    org = self.enemy.origin + '0 0 32';
    makevectors (self.enemy.angles);

    // e.g., Human with Draco: 80 + (16 + 32) * 1.25 = 140.
    cos0  = 80 + (self.enemy.maxs_x + self.maxs_x) * 1.25;
    cos30 = cos0 * 0.866;
    cos60 = cos0 * 0.5;

// First attempt:  12:00.
    spot = org + (v_forward * cos0);
    if (EntryPoint_Test (spot))  return TRUE;
// Second attempt:  1:00.
    spot = org + (v_forward * cos30) + (v_right * cos60);
    if (EntryPoint_Test (spot))  return TRUE;
// Third attempt:  11:00.
    spot = org + (v_forward * cos30) - (v_right * cos60);
    if (EntryPoint_Test (spot))  return TRUE;
// Fourth attempt:  2:00.
    spot = org + (v_forward * cos60) + (v_right * cos30);
    if (EntryPoint_Test (spot))  return TRUE;
// Fifth attempt:  10:00.
    spot = org + (v_forward * cos60) - (v_right * cos30);
    if (EntryPoint_Test (spot))  return TRUE;
// Sixth attempt:  3:00.
    spot = org + (v_right * cos0);
    if (EntryPoint_Test (spot))  return TRUE;
// Seventh attempt:  9:00.
    spot = org - (v_right * cos0);
    if (EntryPoint_Test (spot))  return TRUE;
// Eigth attempt:  4:00.
    spot = org - (v_forward * cos60) + (v_right * cos30);
    if (EntryPoint_Test (spot))  return TRUE;
// Ninth attempt:  8:00.
    spot = org - (v_forward * cos60) - (v_right * cos30);
    if (EntryPoint_Test (spot))  return TRUE;
// Tenth attempt:  5:00.
    spot = org - (v_forward * cos30) + (v_right * cos60);
    if (EntryPoint_Test (spot))  return TRUE;
// Eleventh attempt:  7:00.
    spot = org - (v_forward * cos30) - (v_right * cos60);
    if (EntryPoint_Test (spot))  return TRUE;
// Twelth and final attempt:  6:00.
    spot = org - (v_forward * cos0);
    if (EntryPoint_Test (spot))  return TRUE;

// Failed.
    return FALSE;
};

//------------------------------------------------------------------------//
//  Selects a random point between a range set by given parameters.
//
//  Useful as defense for mages, and as a trick move for bosses.
//  Arguments:  r = random distance, m = minimum distance.  (max = r+m).
//------------------------------------------------------------------------//
float(float r, float m, vector ofs) EntryPoint_Randomize_Ex =
{
    local   vector  ang;
    local   vector  org;    // Original point.
    local   vector  spot;   // Potential teleport destination.
    local   float   dist;   // Distance from 'org' to new 'spot'.
    local   float   three;  // Boolean: 0 = ground only, 1 = three dimension.
    local   float   loop;   // Number of attempts before giving up.

    loop = 100;
    ang = '0 0 0';
    three = self.flags & (FL_FLY|FL_SWIM);  // Three dimensional
    org = self.origin;
    if (!three)     // Now +56, was +32.
        org_z = org_z + 56;     // Go up slightly in case of sloping ground.
    while (loop > 0)
    {
        dist = random() * r + m;
        if (three)
            ang_x = random() * (0 - ofs_x);     //random() * 360;
        if (ofs_z)
        {
            ang_y = random() * ofs_y;
            if (ofs_z > 0)
                ang_y = 0 - ang_y;
        }
        else
            ang_y = crandom() * ofs_y;
        ang_y = anglemod (self.angles_y + ang_y);   //random() * 360;
        makevectors (ang);
        spot = org + (v_forward * dist);
        if (EntryPoint_Test (spot))
            return TRUE;

        loop = loop - 1;
    }
    return FALSE;
};

float(float r, float m) EntryPoint_Randomize =
    {return EntryPoint_Randomize_Ex (r, m, VEC_360);};

//  Selects a random point between close and medium range.
float() EntryPoint_Random = {return EntryPoint_Randomize_Ex (400, 100, VEC_360);};

//  Selects a random point between mid-close and long range.
float() EntryPoint_LongRandom = {return EntryPoint_Randomize_Ex (600, 250, VEC_360);};

float() EntryPoint_Fan = {return EntryPoint_Randomize_Ex (100, 60, '90 60 0');};
//- - - - - - - - -

//------------------------------------------------------------------------//
//  Selects and tests a single given point.
//------------------------------------------------------------------------//
float() EntryPoint_One = {return EntryPoint_Test (self.origin);};

//------------------------------------------------------------------------//
//  This spawns a dummy entity to test points its owner may use as a
//  spawn point or teleport destination.  An entity is needed to check
//  if such points are occupied by solids, and it is better to use an
//  invisible, intangible dummy than the monster who is to be moved.
//
// 'ent' = Entity to be moved.
// 'targ' = Beacon entity, must have clear path to beacon for success.
//           If using summon function, appear somewhere around the beacon.
// 'spot' = Where the entity wants to go.
// 'func' = Function to use when finding a spot.
//------------------------------------------------------------------------//
float(entity ent, entity targ, vector spot, float() func) EntryPoint_Spawn =
{
// Make a dummy entity to test the entry points so the entity to be
// teleported need not be moved around.
    local   entity  test;
    local   entity  os;         // os = old self
    local   float   success;

    test = spawn();
    test.angles    = ent.angles;
    test.movetype  = MOVETYPE_STEP;
    test.velocity  = '0 0 0';
    test.touch     = SUB_Null;
    test.solid     = SOLID_NOT;
    test.owner     = ent;       // Lets dummy pass through entity at will.
    test.enemy     = targ;      // Beacon entity.
    test.flags     = ent.flags & (FL_FLY | FL_SWIM);
    setmodel (test, "");   // invisible
    setsize (test, ent.mins, ent.maxs);
    setorigin (test, spot);

// We are ready to find an entry point.
    os = self;
    self = test;
    success = func ();  //EntryPoint_Find ();
    self = os;

// Done testing, so use any found entry point and remove the dummy.
    if (success)
        setorigin (ent, test.origin);
    remove (test);

// Our work here is done.
    return success;
};


//==========================================================================
//  Speciality Updates

//------------------------------------------------------------------------//
//  Like EntryPoint_Test, but ignores creatures and beacon LoS.
//------------------------------------------------------------------------//
float() EntryPoint_BossCube =
{
// Move the dummy to a test point.
    setorigin (self, self.origin);
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    droptofloor ();     // Do this even if creature can fly or swim.

// Test the point.
    // Note:  These only detect walls, not creatures.
    if (BoxInWall (self.mins, self.maxs, self.origin))
        return FALSE;
    if (self.flags & FL_SWIM)
        if (pointcontents(self.origin) > CONTENT_WATER)
            return FALSE;   // Fish must be in the water.
    // Don't check for creatures because of telefragging.

// Got one!
    return TRUE;
};


//===========================/  END OF FILE  /===========================//
