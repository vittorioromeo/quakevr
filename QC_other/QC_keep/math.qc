//==========================================================================
//  Math

// PM:  Moved here because some engines support builtin math functions.
//  (e.g., DarkPlaces).

//float(float a, float b) QC_Min = {if (a < b) return a;  return b;}
//float(float a, float b) QC_Max = {if (a > b) return a;  return b;}

// DISABLED -- Unused at the moment.
// Return a number 'n' to the 'exp' power.
// Called easy because it only accepts positive integer powers.
// FIXME:  This works only with integers.  Add floating type support.
//float(float n, float exp) QC_Power =
//{
//    local   float   loop;
//    local   float   total;
//
//    exp = floor(exp);
//    loop = fabs(exp);
//    if (!exp)
//        return 1;
//    total = n;
//    while (loop > 1)
//    {
//        total = total * n;
//        loop = loop - 1;
//    }
//    if (exp < 0)
//    {
//        if (total)
//            total = 1 / total;
//        else    // Infinity.
//            total = 16777215;   // Highest number we can use.
//    }
//    return total;
//};

// FIXME:  If DarkPlaces, use builtin.
// Returns a vector where x = cosine and y = sine of 'theta'.
vector(float theta) CoSine =
{
    local   vector  tx, ty, tz, vec;

    tx = v_forward;  ty = v_right;  tz = v_up;  // Save old makevectors.
    vec = '0 1 0' * theta;
    makevectors (vec);

    vec_x = v_forward_x;    // cos
    vec_y = v_forward_y;    // sin
    vec_z = 0;

    v_forward = tx;  v_right = ty;  v_up = tz;  // Restore old makevectors.
    return vec;
};

// Returns a vector where x = cosine, y = sine, and z = tangent of 'theta'.
vector(float theta) CoST =
{
    local   vector  vec;
    local   float   inf, qcsin, qccos;

    vec = CoSine (theta);

// 'inf' represents infinity.  Since a finite number is needed, set 'inf'
// to the highest possible value allowed to be returned.  The value chosen
// is one million.  I could go for broke by using ten million or even
// 16777215, the latter is all 24 bits on, instead.  However doing so
// causes rollover problems if a result of 'inf' that high gets added
// or multiplied by other numbers higher than one.  One million is
// sufficiently high and allows for some wiggle room.
    inf = 1000000;
    qccos = vec_x * inf;
    qcsin = vec_y * inf;
    if (fabs(qccos) < 1)
    {   // Avoid division by zero.
        if (qccos < 0)
            qccos = -1;
        else
            qccos = 1;
    }
    vec_z = qcsin / qccos;    // tan

    return vec;
};

// Trigonometric functions.
// Note:  Should use builtin when available.
float(float t) SUB_cos = {local vector v;  v = CoSine(t);  return v_x;};
float(float t) SUB_sin = {local vector v;  v = CoSine(t);  return v_y;};
float(float t) SUB_tan = {local vector v;  v = CoST(t);  return v_z;};
// FIXME:  Do we need the inverse functions (asin, acos, atan) too?

// a = dividend, n = divisor.
float(float a, float n) modulo =
{
    if (n)
        return a - n * floor(a/n);
    return INFINITY;
};


// DISABLED -- Like anglemod, except the range is -180 <= x < 180.
//float(float v) angle180 =
//{
//    v = v - (floor(v/360) * 360);
//    if (v >= 180)
//        v = v - 360;
//    return v;
//};

// DISABLED -- Bit shift.  +loop does n <<= loop, -loop does n >>= loop.
//float(float n, float loop) bit_shift =
//{
//    if (loop > 0)
//    {   while (loop > 0)
//            {n = n * 2;  loop = loop - 1;}
//    }
//    else if (loop < 0)
//    {   while (loop < 0)
//            {n = n / 2;  loop = loop + 1;}
//    }
//    return n;
//};

// Fixed version of 'vectoangles'.
// Note 8/4/2010:  Fixed by what standard?  Some need it, some don't.
vector(vector dir) Vangles =
{
    dir   = vectoangles(dir);
    dir_x = anglemod(0 - dir_x);  // 'vectoangles' reverses pitch -- fix it!
    return dir;
};


//==========================================================================
//  Points

// Returns a point relative to entity 'e'.  Vector 'a' adds to e's absmins.
// a: '0 0 0' = mins, '0.5 0.5 0.5' = midpoint, '1 1 1' = maxs.
vector(entity e, vector a) Anypoint =
{
    local   vector  v;

//    v = e.origin + e.mins;  // Was e.absmin.
    v = e.absmin;
    v_x = v_x + e.size_x * a_x;
    v_y = v_y + e.size_y * a_y;
    v_z = v_z + e.size_z * a_z;
    return v;
};

// Returns the center of the entity 'e'.
vector(entity e) Midpoint = {return e.origin + (e.mins + e.maxs)*0.5;};

// Returns upper-mid point of a creature, or center of an object.
// Used for aiming by NPCs.
vector(entity e) Aimpoint =
{
    if (e.flags & FLx_CREATURE)
        return Anypoint (e, '0.5 0.5 0.7');
    return Midpoint (e);
};


//==========================================================================
//  Vectors

//- - - - - - - - -
// Calculates outgoing vector when bouncing off a surface.
// Variation of SV_ClipVelocity.
//
// dir = inbound vector.
// norm = trace_plane_normal.
// ob = overbounce.  Use 1.5 for grenades, 2 for HipLaser.
// stop = stop_epsilon.  0.1 in C code, but use 0 for HipLaser.
//- - - - - - - - -
vector(vector dir, vector norm, float ob, float stop) Vec_Reflect =
{
    local   float   ddot;       // Double dot product, or dot * 2.

    ddot = dir * norm * ob;     // 'ob' for overbounce.  Was 2.
    dir_x = dir_x - (norm_x * ddot);
    if (fabs(dir_x) < stop)
        dir_x = 0;
    dir_y = dir_y - (norm_y * ddot);
    if (fabs(dir_y) < stop)
        dir_y = 0;
    dir_z = dir_z - (norm_z * ddot);
    if (fabs(dir_z) < stop)
        dir_z = 0;
    return dir;
};

//- - - - - - - - - - - - - - - - - - -
//  Direction Vector Change

// Originally in 'mirv.qc'.  Placed here now that other rocket attacks
// use them.  Specifically, rocketters' rockets and players' hydra missile.

//------------------------------------------------------------------------//
//  This returns a unit vector whose direction points toward end if it
//  is within theta degrees of dir.  If not, the vector returned will
//  a direction somewhere between dir and the ideal vector.
//
//  Similar to 'ai_turn', but for three dimensions.
//
//  NOTE:  Assumes (0 <= theta <= 180).
//------------------------------------------------------------------------//
vector(vector dir, vector start, vector end, float theta) Dir_GotoIdeal =
{
    local   vector  trig;
    local   vector  v1, v2;
    local   float   dist, dot;
    local   float   qccos, qcsin;

    v1 = dir;
    v2 = end - start;
    dist = vlen(v2);
    v2 = normalize(v2);

    dot = v1 * v2;
    trig = CoSine(theta);
    qccos = trig_x;
    qcsin = trig_y;
    if (dot < qccos)
    {   // Endpoint not in the cone.  Calculate a new vector toward the
        // endpoint that is within the cone.
        local   vector  vx, vy, vz;
        local   float   side, up;

        // Get the three vectors that make up dir.
        dir = vectoangles (dir);
        dir_x = 0 - dir_x;
        makevectors (dir);
        vx = v_forward; vy = v_right; vz = v_up;

        // Project, from start, to the point where the endpoint is
        // neither ahead nor behind it.
        dist = dist * dot;
        start = start + v1*dist;

        // Get the dot products of the horizontal and vertical vectors.
        dir = normalize(end - start);
        side = dir * vy;
        up = dir * vz;

        dir = vy*side + vz*up;
        dist = vlen(dir);
        if (dist == 0)
        {   // Can't use zero vector, so create a random unit vector.
            theta = random() * 360;
            trig = CoSine(theta);
            dir = vy*trig_x + vz*trig_y;
        }
        else
        {   // Make sure we have a unit vector.
            dir = normalize(dir);
        }

        // Calculate the final vector.  The result is already normalized.
        dir = vx*qccos + dir*qcsin;
    }
    else
    {   // Endpoint is within the cone, so simply take its vector.
        dir = v2;
    }

    return dir;
};

//------------------------------------------------------------------------//
//  This changes a directional vector.
//  Useful for things that need drunken or spiraling paths.
//
// 'ang1' affects forward vector.
// 'ang2' affects side (right and up) vectors.
//------------------------------------------------------------------------//
vector(vector dir, float ang1, float ang2) Dir_Change =
{
    local   vector  trig;

    dir = Vangles (dir);
    makevectors (dir);

// Get a vector perpendicular to its direction vector.
    trig = CoSine(ang2);
    dir = v_right*trig_x + v_up*trig_y;

// Calculate new direction.
    trig = CoSine(ang1);
    dir = v_forward*trig_x + dir*trig_y;

    return dir;
};

//------------------------------------------------------------------------//
//  This randomly changes a vector's path.  Used by drunken missiles.
//
//  NOTE:  Assumes (0 <= theta <= 180).
//------------------------------------------------------------------------//
vector(vector dir, float ang) Dir_Randomize =
{
    local   float   a1, a2;

    a1 = random()*ang;
    a2 = random()*360;  // Random angle for perpendicular vector.
    return Dir_Change (dir, a1, a2);
};


//==========================================================================
//  Randomizers

// Emulate 'P_Random' from Doom/Heretic source.
// Returns an integer from 0 to 255.
float() prandom = {return (random() * 256) & 255;};

//// DISABLED -- Returns a random number between two values.
//float(float lo, float hi) rvalue = {return lo + random() * (hi - lo);};


//==========================================================================
//  Damage

// Damage when hit by a rocket directly.  Emulates 100-120 when d = 120.
float(float d) SUB_RandomDamage = {return d / 6 * (5 + random());};

// Damage when hit by melee attack.  Typical amount is the sum of three
// random calls multiplied by a given number.
float(float mx) SUB_MeleeDamage =
{
    local   float   d;

    d = random();
    d = d + random();
    d = d + random();
    return d * mx;
};

//- - - - - - - - -
// Calculates damage as done in T_RadiusDamage, except for some checks that
// enforce a minimum damage so that the target struck will take damage.
//
// With:
//    T_RadiusDamage(self, self.owner, dmg, world);
// A low powered grenade (from an ogre) would cause no damage (or even pain)
// to the target struck if said target was fat enough.
//
// With:
//    points = damage calculated by this function, SUB_ExploDamage.
//    T_Damage (other, self, self.owner, points);
//    T_RadiusDamage (self, self.owner, dmg, world);
// Target struck will take damage.
//- - - - - - - - -
float(entity targ, entity inflictor, float d) SUB_ExploDamage =
{
    local   float   lo, dist;
    local   vector  org;

    org = Midpoint (targ);
    dist = vlen(inflictor.origin - org);
    if (dist > d + 40)  // Don't use distances greater than the maximum
        dist = d + 40;  // possible.  Helps keep damage higher.

    lo = ceil(d*0.1);   // Target will take minimum of 10% damage.
    d = d - (0.5 * dist);
    if (d < lo)
        return lo;  // Target takes minimum damage.
    return d;
};

// DISABLED -- Unused.
//// Calculate an entity's effective health from both its armor and health.
//float(entity ent) ArmoredHealth =
//{
//    if (ent.armorvalue)
//        if (ent.armortype)
//        {
//            local   float   points, maxed;
//
//            points = ent.health + ent.armorvalue;
//            if (ent.armortype < 1)
//            {   maxed = ent.health / (1 - ent.armortype);
//                if (points > maxed)
//                    return maxed;
//            }
//            return points;
//        }
//    return ent.health;      // Unarmored.
//};


//==========================================================================
//  Bits and Bytes

//========================================================================
// World of the 24-bit or 3 byte parm.
//-----------------------------------------------------------------
//       7       6       5       4       3       2       1       0
//-----------------------------------------------------------------
//     128      64      32      16       8       4       2       1
//   32768   16384    8192    4096    2048    1024     512     256
// 8388608 4194304 2097152 1048576  524288  262144  131072   65536
//
// Byte1 = bits 0-7.  Byte2 = bits 8-15.  Byte3 = bits 16-23.
//       = 255              = 65280             = 16711680
//
// Bit24 = 16777216
//------------------------------------------------------------------------
float(float byte1, float byte2, float byte3) parms_pack =
{
    byte1 = byte1 & 255;
    byte2 = (byte2 & 255) * 256;
    byte3 = (byte3 & 255) * 65536;
    return (byte1 + byte2 + byte3);
};

float(float pac, float byte) parms_unpack =
{
    if (byte == 1)
        return (pac & 255);
    if (byte == 2)
        return (pac & 65280) / 256;
    if (byte == 3)
        return (pac & 16711680) / 65536;
    return 0;
};


//==========================================================================
//  Other

// Returns gravity multiplier for 'self'.  Used by jumping monsters.
float() Gs =
{
    local   float   g;

    g = cvar("sv_gravity") / 800;
    if (self.gravity)
        g = g * self.gravity;
    return g;
};

float   SQRT2   = 1.4142;   // The square root of 2.
