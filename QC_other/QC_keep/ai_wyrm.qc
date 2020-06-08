//- - - - - - - - -
//  Wyrm_WalkMove
//- - - - - - - - -
// Any monster, regardless of size, uses one of three clipping hull sizes.
//
//  if (size_x < 3)
//      clip size:  '0 0 0'
//  else if (size_x <= 32)
//      clip size:  mins = '-16 -16 -24', maxs = '16 16 32'
//  else
//      clip size:  mins = '-32 -32 -24', maxs = '32 32 64'
//
// The largest clip size available is size_x = 64.
//
// Monsters whose actual size is bigger than the largest hull size will
// clip into the north and east walls.  Their bbox begins at the point
// mins, and ends at mins + '64 64 88'.  The excess is ignored, which is
// why they clip into said walls.
//
// To fix the clipping problem, move a test clip box of the largest size
// multiple times from different points.  If each test move succeeds,
// then the actual move should not clip any walls... in theory.
//
// Notes about the anti-clip code below:
// size_x <= 64:  Totally unnecessary, just use one of the standard hulls.
//
// 64 < size_x <= 128:  Works almost flawlessly.
//
// size_x > 128:  Works okay with simple architecture and a relatively flat
//  and smooth floor and ceiling (such as the open sky).  Works not-so-well
//  with complex architecture and broken terrain, where the dragon may clip
//  walls and get stuck inside of them.
//- - - - - - - - -
float(float yaw, float dist) Wyrm_WalkMove = {
    local   entity  swap;
    local   vector  org;
    local   float   flat, up;

    org = self.origin;

// Prepare empty hull for movement.
    setorigin (move_box, self.origin);
    setsize (move_box, self.mins, self.maxs);
//    move_box.solid      = SOLID_NOT;
//    move_box.movetype   = self.movetype;
    move_box.flags      = FL_FLY;
    move_box.angles     = self.angles;
    move_box.ideal_yaw  = self.ideal_yaw;
    move_box.enemy      = self.enemy;   // For walkmove's up/down motion.
    move_box.goalentity = self.goalentity;
    move_box.owner      = self;         // So box can pass through owner.
// Splash sound control.  Synchronize builtin water values with the dragon's.
// Otherwise, any time the dragon is underwater, Quake will think the
// hull box enters water when it moves, resulting in repeating splash
// sounds each frame.
    move_box.h2olevel = self.h2olevel;
    move_box.watertype  = self.watertype;

    swap = self;
    self = move_box;

// Check if the dragon could move at all.  Use the move box to test the move.
// This checks the SW corner.
    if (!Ryu_WalkMove (yaw, dist))
        {self = swap; return FALSE;}

// Remember this!  All movement must have the same elevation.
    flat = FALSE;
    up = self.origin_z;

// The dragon could move, so now use move box to check.
// The SW corner is guaranteed to move correctly, so check the other corners.
    local   float   lo, hi;
    local   vector  add;

    lo = swap.mins_x + 32;
    hi = swap.maxs_x - 32;

// Reset hull size to standard big size.
    setsize (move_box, VEC_HULL2_MIN, VEC_HULL2_MAX);

    add = '0 0 0';
// Check NW corner.
    add_x = lo;
    add_y = hi;
    setorigin (self, org + add);
    if (!Ryu_WalkMove (yaw, dist))
        {self = swap; return FALSE;}
    if (self.origin_z != up)
        flat = TRUE;
// Check NE corner.
    add_x = hi;
    setorigin (self, org + add);
    if (!Ryu_WalkMove (yaw, dist))
        {self = swap; return FALSE;}
    if (self.origin_z != up)
        flat = TRUE;
// Check SE corner.
    add_y = lo;
    setorigin (self, org + add);
    if (!Ryu_WalkMove (yaw, dist))
        {self = swap; return FALSE;}
    if (self.origin_z != up)
        flat = TRUE;
// If the dragon is larger than a 128x128 area, areas between the corners
// must be checked too.  Otherwise, we are done.
    if (swap.maxs_x > 96)
    {   // 4x4 -- Safe up to 256x256.
    // Check S face.
        add_x = 32;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
        add_x = -32;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
    // Check N face.
        add_y = hi;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
        add_x = 32;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
    // Check W face.
        add_x = lo;
        add_y = 32;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
        add_y = -32;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
    // Check E face.
        add_x = hi;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
        add_y = 32;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
    // Check the middle.  Yes, we need to do this too to prevent the
    // dragon for clipping things directly above or below it, such as
    // the tops of pyramids or obelisks.
        add_x = 32;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
        add_x = -32;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
        add_y = -32;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
        add_x = 32;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
    }
    else if (swap.maxs_x > 64)
    {   // 3x3 -- Safe up to 192x192.
    // Check S face.
        add_x = 0;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
    // Check N face.
        add_y = hi;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
    // Check W face.
        add_x = lo;
        add_y = 0;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
    // Check E face.
        add_x = hi;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
    // Check the middle.  Yes, we need to do this too to prevent the
    // dragon for clipping things directly above or below it, such as
    // the tops of pyramids or obelisks.
        add_x = 0;
        setorigin (self, org + add);
        if (!Ryu_WalkMove (yaw, dist))
            {self = swap; return FALSE;}
        if (self.origin_z != up)
            flat = TRUE;
    }

// Test movement successful.  Now move the dragon for real.
    self = swap;
    if (flat)
        return flat_move (yaw, dist);
    return Ryu_WalkMove (yaw, dist);
};