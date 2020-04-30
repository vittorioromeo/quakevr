//entity(entity attacker) Grapple_Pull_Who;

//==========================================================================
//  Shot Leading

//------------------------------------------------------------------------//
//  This returns the coefficient that multiplies an entity's velocity.
//------------------------------------------------------------------------//
float(entity ent) Aim_SpeedMod =
{
    local   float   sped;

    sped = 1;
// SoE:  Biosuit killed the wetsuit and took its stuff.
    if (ent.flags & FL_CLIENT)
    {   // Only a player is affected by all of these events.
        if (ent.waterlevel == 3)
        {
            if (ent.radsuit_finished)
                sped = 1.5;
        }
        else if (ent.waterlevel == 2)
        {
            if (ent.radsuit_finished)
                sped = 1.25;
        }
        else if (ent.form_active == FORM_DRAGON)
            sped = 1.429;       // 10/7 or 1.428571...

        // Update: 10/29/10:  Check all sources of slow.
        if ((ent.curse_type == CURSE_SLOW) || ent.slow_finished)
            sped = sped / 2;
    }
    return sped;
};

//------------------------------------------------------------------------//
//  This checks if an entity is falling.  Used by entities who lead shots.
//------------------------------------------------------------------------//
float(entity ent) IsFalling =
{
    // Not falling if flying, swimming, or already on the ground.
    if (ent.flags & FLx_NOT_FALLING)
        return FALSE;

    // Flying or ethereal targets cannot fall.
    if (ent.movetype == MOVETYPE_FLY)
        return FALSE;
    if (ent.movetype == MOVETYPE_NOCLIP)
        return FALSE;
    if (ent.movetype == MOVETYPE_FLYMISSILE)
        return FALSE;

    // Players need extra checking.
    if (ent.movetype == MOVETYPE_WALK)      //(ent.classname == "player")
    {
        if (ent.waterlevel >= 2)
            return FALSE;   // Swimming.
    }

    // Target is falling.
    return TRUE;
};

//------------------------------------------------------------------------//
//  Check if target's vertical velocity should be ignored.
//------------------------------------------------------------------------//
float(entity ent) Aim_IgnoreZ =
{
    //- - - - - - -
    // A player swimming along the surface of water has a velocity that
    // oscillates up and down constantly and should be ignored.
    //
    // Flight via Dark Angel wings also has a similar effect in the air.
    // Update:  Wings replaced by Boots of Levitation.  However, it is
    // probably a good idea to keep the same item check code.
    //- - - - - - -
    if (ent.movetype == MOVETYPE_WALK)      //(ent.classname == "player")
    {
        if (!(ent.flags & FL_ONGROUND))
        {
            if (ent.waterlevel)
                if (ent.waterlevel < 3)
                    return TRUE;    // Swimming along water surface.

            if (ent.wing_finished)
                if (ent.button2)
                    if (ent.waterlevel < 2)
                        return TRUE;    // Flying with Dark Angel wings.
        }
    }
    return FALSE;
};

//------------------------------------------------------------------------//
//  This tries to predict where the target will go, then returns that spot.
//
//  NOTE:  This only works on targets that have a velocity value.
//  In Quake, only players have a constantly updated velocity.  Monsters
//  do not update their velocity because they move in steps, unless it is
//  jumping or falling (e.g., fip2).  Bottom line:  This works reliably
//  only on players.
//
//  NOTE:  While using Law of Sines or application of (a/sin(A) = b/sin(B))
//  may be a more elegant way to a perfect answer, standard Quake does not
//  support trigometric functions, and I suspect adding trig functions via
//  QuakeC and using them may involve running more code than necessary when
//  a more primitive yet simplier way (as far as Quake is concerned) of
//  getting an answer that is "close enough" is sufficient.
//------------------------------------------------------------------------//
vector(vector p1, vector p2, float sped, entity targ) Aim_Lead =
{
    if (!targ)
        return p2;     // No target.
    if (targ.movetype == MOVETYPE_NONE)
        return p2;     // Can't move, ignore velocity.
    if (targ.movetype == MOVETYPE_STEP)
        if (targ.flags & FLx_NOT_FALLING)
            return p2;     // Target velocity is not accurate.
    // Removed invisibility checks -- done elsewhere.
    if (!sped)
        return p2;     // Avoid division by zero.

    // Now find the spot.
    local   float   mx;
    local   float   loop;
    local   float   airtime;
    local   float   xyz;    // TRUE = Use all three dimensions.
    local   float   wired;  // TRUE = Riding a grapple cable.
    local   vector  lead;
    local   vector  tv;

//- - - - - - - - -
// Update 7/31/09:  Check if target is riding on a grappling wire.
// Note:  Applies only to attackers using the grappler to move,
// not creatures hooked and drawn toward the grappler.  However,
// since pulling hooked monsters is rare enough and does not apply
// to players, I'm not adding another field and more code just to
// make the case of aiming at hooked monsters pulled by players work.
    wired = FALSE;
    if (targ.hook.state)    // Should be 0 if targ.hook == world.
        //if (Grapple_Pull_Who (targ) == targ)
            //wired = TRUE;
//- - - - - - - - -
    tv = targ.velocity;
// Check if target's vertical velocity should be ignored.
    xyz = TRUE;
    if (!wired)
        if (Aim_IgnoreZ (targ))
            {xyz = FALSE; tv_z = 0;}

// Calculate the time it takes for a missile to reach a given spot,
// then calculate where the moving target will be after that amount of time.
// However, after predicting where the target will be, it may take more or
// less time for the missile to reach that spot.  Therefore, we ought to
// recalculate the time needed to reach the real spot.  Each recalcuation
// reduces the margin of error.  Idea is similar to a power series.
// Yes, brute force may be ugly, but it gets the job done.
    mx = Aim_SpeedMod (targ);
    loop = 5;       // Make sure the number is odd.
    lead = p2;
    while (loop > 0)
    {
        airtime = vlen(lead - p1) / sped;
        lead = p2 + (tv * mx) * airtime;
        loop = loop - 1;
    }

// Update 7/31/09:  Check if target is riding on a grappling wire.
    if (wired)
    {
        if (targ.hook.state != 2)
            loop = (targ.hook.enemy.maxs_x + targ.maxs_x);
        else
            loop = targ.maxs_x;
        tv = targ.hook.origin - targ.origin;
        mx = (vlen(tv) - loop) / SPEED_HOOK;
        if (mx < airtime)
        {   // Target will be at the hook -- aim there!
            tv = targ.hook.origin - normalize(tv)*loop;
            return tv;
        }
    }
//- - - - - - - - -

// Check for gravity.  For better accuracy, this check should be done in
// the above loop, but since this has much more code, just do this once,
// even if accuracy isn't perfect.
    if (xyz)
        if (IsFalling (targ))
    {
        local   float   fell;       // Distance fallen.
        local   float   gforce;     // Target gravity.

        gforce = cvar("sv_gravity");
        if (targ.gravity)
            gforce = gforce * targ.gravity;

        fell = 0.5 * gforce * airtime * airtime;    // 1/2 gt^2.
        lead_z = lead_z - fell;

        // Check if the path to the spot is clear.
        traceline (p1, lead, TRUE, targ);
        if (trace_fraction < 1)
        {
            if (fell)
            {
                // Projected spot was blocked.  Check if the spot was blocked
                // due to the floor or ceiling alone.  If so, aim at the
                // surface.
                local   vector  spot;

                spot   = lead;
                spot_z = p2_z;

                traceline (spot, lead, TRUE, targ);
                lead = trace_endpos;
                if (fell > 0)
                    lead_z = lead_z - targ.mins_z;      // floor
                else
                    lead_z = lead_z - targ.maxs_z;      // ceiling
            }
        }
    }

// Our work here is done.
    return lead;
};

//------------------------------------------------------------------------//
//  This is similar to traceline, except gravity affects this.
//
//  The value returned is the closest distance the path was to a point,
//  which is necessary when comparing the better of two different traces.
//
//  (Originally, the value returned is the time of flight before impact.)
//
//  Note:  If mtype == MOVETYPE_BOUNCE, trace grenade path.
//------------------------------------------------------------------------//
float(vector p1, vector vel, float nomon, entity ignore, float fuse,
    vector spot, float mtype)  TraceBallistic =
{
    local   float   fps, tpf;   // Frame Per Second, Time Per Frame.
    local   float   g, minv, maxv;
    local   float   t, dist, best;

// The closer time per frame is to hosttime, the more accurate the trace is.
// However, try to keep the number of traceline calls to a minimum.  Thus,
// 20 fps is a good compromise between accuracy and efficiency.
    fps = 20;       // Don't use zero or negative number.
    tpf = 1 / fps;
// Note:  If you really want to work more for more accuracy, use the
// following two lines instead.  Warning:  frametimes on a fast computer
// can approach a frametime of 0.01, meaning about 100 calls.  That's
// A LOT of traceline calling!
//    tpf = frametime;
//    fps = 1 / tpf;

    best = 20000;   // More than 8000 * 2.
    t = 0;
    fuse = floor(fuse * fps);
    g = cvar("sv_gravity") * tpf;
    maxv = cvar("sv_maxvelocity");
    minv = 0 - maxv;
//- - - - - - - - -
    // Velocity check
    if (vel_x > maxv)       vel_x = maxv;
    else if (vel_x < minv)  vel_x = minv;
    if (vel_y > maxv)       vel_y = maxv;
    else if (vel_y < minv)  vel_y = minv;
    if (vel_z > maxv)       vel_z = maxv;
    // Lower bound for z done in loop.
//- - - - - - - - -
    while (t < fuse)
    {
        if (vel_z < minv)   // Velocity check.
            vel_z = minv;   // Note:  Don't bother with reverse gravity.
        vel_z = vel_z - g;
        traceline (p1, p1 + (vel * tpf), nomon, ignore);
        p1 = trace_endpos;
    //- - - - - - - - -
        dist = vlen(p1 - spot);
        if (dist < best)
            best = dist;
    //- - - - - - - - -
        if (trace_fraction < 1)
        {
            if ((mtype != MOVETYPE_BOUNCE) || (trace_ent.takedamage == DAMAGE_AIM))
                fuse = 0;   // Got a hit -- break the loop!
            else
            {   // Rebound.  Emulate C physics code.
                // overbounce = 1.5, stop_epsilon = 0.1, as per C code.
                vel = Vec_Reflect (vel, trace_plane_normal, 1.5, 0.1);
                if (vel == '0 0 0')
                    fuse = 0;   // Stopped.
                else
                {
                    trace_ent = world;      // Bounced off the guy.
                    trace_fraction = 1;
                }
            }
        }
        t = t + 1;
    }
    return best;        //return t * tpf;
};

// FIXME:  DarkPlaces has a builtin function for this.
void(vector p1, vector vel, float nomon, entity ignore, float fuse)
    TraceToss =
{
    TraceBallistic (p1, vel, nomon, ignore, fuse, p1, MOVETYPE_TOSS);
};

//------------------------------------------------------------------------//
//  This returns how much speed to add up for a toss projectile to
//  hit the target point.
//
//  NOTE:  The resulting speed can be much higher than the original.
//------------------------------------------------------------------------//
float(vector p1, vector p2, float sped) Aim_TossUp =
{
    local   float   gforce;     // Target gravity.
    local   float   up;         // Add to z.

    if (sped <= 0)
        return 0;   // Avoid division by zero.

    gforce = cvar("sv_gravity");
    p2 = p2 - p1;

    up = 0.5 * gforce / sped;
    up = vlen(p2) * up;
    return up;
};


//==========================================================================
//  Monster Aiming

//- NOTE - - - - - -
// The set of numbers in the 'spread' vectors passed to these aim
// functions are angles, not their tangents.
//
// Why pass the angles?  Because the values returned by crandom() with
// tangents passed to it tends to favor the edges.  For example, passing
// a tangent of 10000000 to crandom will very likely return a value that,
// if you calcualated its inverse tangent, be 89.x degrees.
//
// By passing the angle instead, there is no bias for any angle, assuming
// the random() function does its job.  After randomization, the angle may
// be converted to a tangent for whatever purpose.
//- - - - - - - - -
vector(vector dir, vector vtan) Aim_Tangent =
{
    local   vector  vang;
    local   float   mx;

    // Combine the tangent side vector to the old directional vector.
    mx = vlen (dir);
    dir = normalize (dir);
    vang = Vangles (dir);
    makevectors (vang);
    dir = dir + (vtan_x * v_right) + (vtan_y * v_up);
    dir = normalize (dir);

    return (dir * mx);
};

// spread:  x = horizontal, y = vertical, z = 0/1 for randomize/set.
vector(vector dir, vector spread) Aim_Scatter =
{
    // Randomize the angles before converting them into tangents.
    if (!spread_z)
        {spread_x = crandom() * spread_x;  spread_y = crandom() * spread_y;}
    spread_x = SUB_tan (spread_x);  spread_y = SUB_tan (spread_y);

    // Combine the tangent side vector to the old directional vector.
    return Aim_Tangent (dir, spread);
};

vector(vector dir, vector spread) Maim_Scatter =
{
    local   float   r;

    if (self.enemy.invisible_finished || (self.enemy.items & IT_INVISIBILITY))
    {
        r = random() * 4;   // Monsters have 25% chance to guess right.
        r = floor(r) * 5;   // Aim up to 15 degrees to either side.
        if (random() < 0.5)
            r = 0 - r;
    }
    else
        r = 0;

    // Randomize the angles before converting them into tangents.
    if (!spread_z)
        {spread_x = crandom() * spread_x;  spread_y = crandom() * spread_y;}
    spread_x = SUB_tan (spread_x + r);  spread_y = SUB_tan (spread_y);

    // Combine the tangent side vector to the old directional vector.
    return Aim_Tangent (dir, spread);
};


//- - - - - - - - -
// Constants
float   SHOT_TIME_TOSS      = 5;
float   SHOT_TIME_BOUNCE    = 2.5;      // Assuming grenades.
float   SMARTBALL           = -1;   // Used instead of a movetype to do hack.

//------------------------------------------------------------------------//
//  This returns the directional vector needed for a monster to hit its
//  target.
//
//  NOTE:  The vector returned can be smaller than a unit vector.
//  (Really, it should not because that is cheating.  Oh well.)
//------------------------------------------------------------------------//
vector(vector p1, vector p2, float sped, float up, entity targ, float ex,
    float fuse, float mtype)  Aim_Ballistic =
{
    if (!sped)
        return '0 0 0';     // Avoid divison by zero.

    local   vector  dir, vel, v1, v2, vup;
    local   float   d1, d2;
    local   float   toss, mx;

    vup = '0 0 0';
    vup_z = up;

// Locate the point we want to aim at.
    if (targ)
    {
        dir = Aim_Lead (p1, p2, sped, targ);
        if (ex < 1)
            dir = p2 + ((dir - p2) * ex);   // Partial leading.
        // Check if the path to the projected endpoint is clear.
        // NOTE:  This does not check if monsters are in the way.
        traceline (p1, dir, TRUE, self);
        if (trace_fraction == 1)
            p2 = dir;   // Success.
    }
    self.dest = p2;

// Get first vector:  Direction in the xy-plane only, eliminate z-axis.
// This is closest, albeit not identical, to id's old aiming code.
    dir = normalize(p2 - p1);
    vel = dir * sped;
    vel_z = vel_z + up;

    v1 = v2 = dir;
    v1_z = 0;
    v1 = normalize (v1);

// Get second vector:  Direction needed to hit opponent.  Does not account
// for obstacles in the way.
    // Determine the extra vertical speed needed to hit the target point.
    toss = Aim_TossUp (p1, p2, sped);
    vel = dir * sped;
    vel_z = vel_z + toss - up;
    //- - - - - - - - -
    // At this point, the upward speed returned would let the monster hit the
    // target point with perfect accuracy.  However, that is cheating since
    // the player cannot do that trick, especially if it lets the monster hit
    // at ranges longer than the player could hit if using the same attack
    // (for example, grenades).
    //
    // For the sake of efficiency, allow this sort of cheating only if the
    // resulting speed is slower than normal, which lets the monster drop
    // grenades from above without overshooting the target point.
    //- - - - - - - - -
    mx = vlen(vel) / sped;
    vel = normalize (vel);
    if (mx < 1)     // Yes, we may return a shorter-than-unit vector.
        vel = vel * mx;
    v2 = vel;

//- - - - - - - - -
    if (mtype == SMARTBALL)
        return v2;      // Smartball hack.
//- - - - - - - - -
// Project flight paths from the given two vectors.  If either hit the enemy,
// return the direction.  In case both vectors can hit, select the first
// forward-only vector, since it is the closest to the old monster aim code.
    if (self.enemy)
    {   // Check if we can hit our intended target.
        local   entity  vic;    // VICtim.
        local   entity  oown;   // Old OWNer, placeholder for swapping.

        vic = self.enemy;       // The target the trace needs to hit.
        oown = self.enemy.owner;    // See below in target spawn section.
        if (targ)
        {   // Shot leading on.
            // Note:  Leading shots to targ, so use targ, regardless of
            // self.enemy.  FWIW, targ is usually, if not always, self.enemy.
            if (p2 != targ.origin)
            {
                vic = spawn();   // Definitely NOT self.enemy or world.
                vic.owner = targ;
                vic.solid = targ.solid;
                vic.takedamage = targ.takedamage;
                setorigin (vic, p2);
                setsize (vic, targ.mins, targ.maxs);
                // Setting the enemy's owner to self lets traceline ignore
                // the enemy, if it ignores self.
                self.enemy.owner = self;
            }
        }
    // One
        d1 = TraceBallistic (p1, v1*sped + vup, FALSE, self, fuse, p2, mtype);
        if (trace_ent == vic)
        {   if (vic != self.enemy)
                {self.enemy.owner = oown;  remove (vic);}
            return v1;      // ONE!
        }
    // Two
        d2 = TraceBallistic (p1, v2*sped + vup, FALSE, self, fuse, p2, mtype);
        if (trace_ent == vic)
        {   if (vic != self.enemy)
                {self.enemy.owner = oown;  remove (vic);}
            return v2;      // TWO!
        }
        // If we didn't eliminate the dummy target yet, remove it now.
        if (vic != self.enemy)
            {self.enemy.owner = oown;  remove (vic);}
    }
    else
    {   // No target to hit, so simply aim at the given point.
        d1 = TraceBallistic (p1, v1*sped + vup, FALSE, self, fuse, p2, mtype);
        d2 = TraceBallistic (p1, v2*sped + vup, FALSE, self, fuse, p2, mtype);
    }

// Neither projections could hit the enemy, so pick the one that reached
// the closest to the enemy.
    if (d2 < d1)
        return v2;      // 3D Ballistic
    return v1;      // 2D Forward
};

vector(vector p1, vector p2, float sped, float up, entity targ, float ex)
    Aim_TossEx =
{
    return Aim_Ballistic (p1, p2, sped, up, targ, ex, SHOT_TIME_TOSS, MOVETYPE_TOSS);
};

vector(vector p1, vector p2, float sped, float up, entity targ) Aim_Toss =
{
    return Aim_Ballistic (p1, p2, sped, up, targ, 1, SHOT_TIME_TOSS, MOVETYPE_TOSS);
};

vector(vector p1, vector p2, float sped, float up, entity targ,
    vector spread, float ex) Maim_TossEx =
{
    p2 = Aim_Ballistic (p1, p2, sped, up, targ, ex, SHOT_TIME_TOSS, MOVETYPE_TOSS);
    p2 = Maim_Scatter (p2, spread);
    return p2;
};

vector(vector p1, vector p2, float sped, float up, entity targ, vector spread)
    Maim_Toss =
{
    p2 = Aim_Ballistic (p1, p2, sped, up, targ, 1, SHOT_TIME_TOSS, MOVETYPE_TOSS);
    p2 = Maim_Scatter (p2, spread);
    return p2;
};

// Specially made for grenade lobbers such as ogres.
vector(vector p1, vector p2, float sped, float up, entity targ, vector spread)
    Maim_Grenade =
{
    p2 = Aim_Ballistic (p1, p2, sped, up, targ, 1, SHOT_TIME_BOUNCE, MOVETYPE_BOUNCE);
    p2 = Maim_Scatter (p2, spread);
    return p2;
};

// Specially made for the "smart balls" launched by Zerstorer's Nemesant.
vector(vector p1, vector p2, float sped, float up, entity targ, vector spread)
    Maim_SmartBall =
{
    p2 = Aim_Ballistic (p1, p2, sped, up, targ, 1, SHOT_TIME_TOSS, SMARTBALL);
    p2 = Maim_Scatter (p2, spread);
    return p2;
};

//- - - - - - - - -
vector(vector p1, vector p2, float sped, entity targ, float ex) Aim_LineEx =
{
    local   vector  dir;

    // If no speed, assume instant-hit... and avoid division by zero.
    if (sped)
        if (targ)
        {
            dir = Aim_Lead (p1, p2, sped, targ);
            if (ex < 1)
                dir = p2 + ((dir - p2) * ex);   // Partial leading.
            // Check if the path to the projected endpoint is clear.
            // NOTE:  This does not check if monsters are in the way.
            traceline (p1, dir, TRUE, self);
            if (trace_fraction == 1)
                p2 = dir;   // Success.
        }
    dir = normalize (p2 - p1);
    return dir;
};

vector(vector p1, vector p2, float sped, entity targ) Aim_Line =
{
    return Aim_LineEx (p1, p2, sped, targ, 1);
};

vector(vector p1, vector p2, float sped, entity targ, vector spread, float ex)
    Maim_LineEx =
{
    p2 = Aim_LineEx (p1, p2, sped, targ, ex);
    p2 = Maim_Scatter (p2, spread);
    return p2;
};

vector(vector p1, vector p2, float sped, entity targ, vector spread)
    Maim_Line =
{
    p2 = Aim_LineEx (p1, p2, sped, targ, 1);
    p2 = Maim_Scatter (p2, spread);
    return p2;
};


//==========================================================================
//  Other

// Reduces shots in a series by one and returns a number between 0 and 1.
// Number returned is used for shot leading.  0 for none, 1 for perfect.
// Used by monsters who gradually improve their aim during an attack salvo.
float() Frac_ShotDown =
{
    local   float   d;

    if (self)
    {
        self.shots_left = self.shots_left - 1;
        d = self.shots_max - 1;
        if (d > 1)
            return fabs((d - self.shots_left) / d);
    }
    return 1;
};


//===========================/  END OF FILE  /===========================//
