float(float yaw, float dist) noclip_move;
void(float dist) noclip_goal;
void SuperDamageSound ();

// Checks if a given point in space is in water.
float(vector spot) VecInWater;;

// Called by 'W_Attack' and various monster AI functions.
void() W_ShowHostile =
{
    SuperDamageSound ();    // Moved from 'W_WeaponFrame' to here.
    self.show_hostile = time + 1;   // wake monsters up
    //self.attack_time = time;    // So imps know when to dodge.
};

// Prototype -- function at bottom of this file.
float(float yaw, float dist) safe_move;

// Based on 'SV_StepDirection'.
// Use this only for big multi-hulled monsters such as dragons.
float(float yaw, float dist) QC_StepDir =
{
    self.ideal_yaw = yaw;
    ChangeYaw ();

//- - - - - - - - -
// First, move self.  If successful, see if self is facing ideal.
// If not, snap self back.
//    if (self.solid == SOLID_NOT)
    {   // Solid not entities cannot touch things.
        // The move_box entity should be solid_not.
        local   vector  spot;
    
        spot = self.origin;
        if (safe_move (yaw, dist))
        {
            if (!FacingIdeal ())
                setorigin (self, spot);   // Didn't turn enough so don't move.
            return TRUE;
        }
    }
// FIXME:  Code below breaks dragons' move code if used.  Don't know how yet.
//    else
//    {   // Can't risk touching triggers that may hurt or teleport self,
//        // so get a dummy to test movement.
//        local   entity  swap;
//        local   float   moved, faced;
//
//        UpdateMovebox ();
//        swap = self;
//        self = move_box;
//        moved = safe_move (yaw, dist);      // This moves the dummy.
//        faced = FacingIdeal ();
//        self = swap;
//        if (moved)
//        {   // Dummy made a move.
//            if (faced)
//                safe_move (yaw, dist);      // Move where the dummy moved.
//            return TRUE;
//        }
//    }
//- - - - - - - - -

    return FALSE;
};

// Based on 'SV_NewChaseDir'.
void(float dist) QC_NewChaseDir =
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
            if (QC_StepDir (turn, dist))
                return;
    }

// Try other directions.
    local   float   ax, ay;

    ax = fabs(delta_x);
    ay = fabs(delta_y);
    if ((random() < 0.333) || (ay > ax))
        {turn = d1; d1 = d2; d2 = turn;}

    if (d1 != -1)
        if (d1 != back)
            if (QC_StepDir (d1, dist))
                return;
    if (d2 != -1)
        if (d2 != back)
            if (QC_StepDir (d2, dist))
                return;

// No direct path to goal, so pick another way.
    if (old != -1)
        if (QC_StepDir (old, dist))
            return;

    local   float   end, next;
    if (random() < 0.5)
        {turn = 0; end = 315; next = 45;}
    else
        {turn = 315; end = 0; next = -45;}

    while (turn <= end)
    {
        if (turn != back)
            if (QC_StepDir (turn, dist))
                return;
        turn = turn + next;
    }

    if (back != -1)
        if (QC_StepDir (back, dist))
            return;

// Can't move.
    self.ideal_yaw = old;

// Disabled bridge pull out code because only flying dragons use this code.
// Pull out code.
//    flat_move (0, 0);   // This will do the 'SV_FixCheckBottom' code.
};

// Based on 'SV_CloseEnough'.
float(float dist) QC_CloseEnough = {
    if (self.goalentity.absmin_x > self.absmax_x + dist)  return FALSE;
    if (self.goalentity.absmax_x < self.absmin_x - dist)  return FALSE;
    if (self.goalentity.absmin_y > self.absmax_y + dist)  return FALSE;
    if (self.goalentity.absmax_y < self.absmin_y - dist)  return FALSE;
    if (self.goalentity.absmin_z > self.absmax_z + dist)  return FALSE;
    if (self.goalentity.absmax_z < self.absmin_z - dist)  return FALSE;
    return TRUE;
};

// This tries to emulate movetogoal for multiple bboxes acting as a unit.
// This is probably a very slow function compared to other functions.
// (However, it is not noticable at all with a modern computer.)
void(float dist) QC_MoveToGoal =
{
// Commented out because only dragons use this, and they fly.
//    if (!(self.flags & FLx_NOT_FALLING))    //(FL_ONGROUND|FL_FLY|FL_SWIM)
//        return;

// If next step hits enemy, stop.
    if (self.enemy)
        if (QC_CloseEnough (dist))
            return;

// Bump around...
    if (random() < 0.333)
        QC_NewChaseDir (dist);
    else if (!QC_StepDir (self.ideal_yaw, dist))
        QC_NewChaseDir (dist);
};

//=========================================================================
//  Animation

// Sets nextthink.  'fr' is frame time delay based on 35 fps.
// Used by code that emulates stuff from the Doom or Heretic games.
void(float fr) Doom_NextThink = {self.nextthink = time + (fr / 35);};

// Checks if the given two entities' bboxes overlap.
// Similar to 'EntitiesTouching', but uses absmin/absmax.
float(entity e1, entity e2) Overlap =
{
    if (e1.absmin_x > e2.absmax_x)  return FALSE;
    if (e1.absmax_x < e2.absmin_x)  return FALSE;
    if (e1.absmin_y > e2.absmax_y)  return FALSE;
    if (e1.absmax_y < e2.absmin_y)  return FALSE;
    if (e1.absmin_z > e2.absmax_z)  return FALSE;
    if (e1.absmax_z < e2.absmin_z)  return FALSE;
    return TRUE;
};


// Like 'Overlap' above, but adds distance.
// Useful to check collisions when one entity has a clip bonus.
float(entity e1, entity e2, float dist) Overlapped =
{
    if (e1.absmin_x > e2.absmax_x + dist)  return FALSE;
    if (e1.absmax_x < e2.absmin_x - dist)  return FALSE;
    if (e1.absmin_y > e2.absmax_y + dist)  return FALSE;
    if (e1.absmax_y < e2.absmin_y - dist)  return FALSE;
    if (e1.absmin_z > e2.absmax_z + dist)  return FALSE;
    if (e1.absmax_z < e2.absmin_z - dist)  return FALSE;
    return TRUE;
};

float(vector amin, vector amax, vector bmin, vector bmax) Point_Overlap =
{
    if (amin_x > bmax_x)  return FALSE;
    if (amax_x < bmin_x)  return FALSE;
    if (amin_y > bmax_y)  return FALSE;
    if (amax_y < bmin_y)  return FALSE;
    if (amin_z > bmax_z)  return FALSE;
    if (amax_z < bmin_z)  return FALSE;
    return TRUE;
};

// Checks if entity 'e' is a missile.
float(entity e) IsMissile =
{
    if (!(e.flags & FLx_NOT_MISSILE))
    {
        if (e.solid == SOLID_BBOX)
        {
            if (e.touch)
                if (e.touch != SUB_Null)
                    if (e.touch != SUB_Remove)
                        return TRUE;
        }
        else if (e.solid == SOLID_TRIGGER)
        {
            if (e.classname == "fireball")  // Lavaball check.
                return TRUE;
        }
    }
    return FALSE;
};

//  This makes an entity do a think function right now.
void(entity ent, void() thinkst) SUB_Think = {
    local   entity  swap;

    swap = self;
    self = ent;
    thinkst ();
    self = swap;
};

//==========================================================================
//  Safe Message Functions

// These are for functions that display messages and are used by clients
// (players) and non-clients (monsters).  Such functions include item pickups
// and powerup countdowns.

void(entity ent, string text) Safe_sprint =
{
    if ((ent.flags & FLx_CREATURE) == FL_CLIENT)
        sprint (ent, text);
};

void(entity ent, string text) Safe_centerprint =
{
    if ((ent.flags & FLx_CREATURE) == FL_CLIENT)
        centerprint (ent, text);
};

void(entity ent, string text) Safe_stuffcmd =
{
// The server crashes if it attempts a stuffcmd on a non-client.
    if ((ent.flags & FLx_CREATURE) == FL_CLIENT)
        stuffcmd (ent, text);
};

// Use this to centerprint a message to everyone.
void(string text) Mass_Centerprint =
{
    local   entity  pc;

    pc = find (world, classname, "player");
    while (pc)
    {
        if ((pc.flags & FLx_CREATURE) == FL_CLIENT)   // Confirm pc status.
            centerprint (pc, text);
        pc = find (pc, classname, "player");
    }
};

// Convenience function -- set model, size, and origin.
// Called by various missile functions.
void(entity e, string s, vector v1, vector v2, vector org) setall =
{
    setmodel (e, s);        // May resize hullbox after this is called.
    setsize (e, v1, v2);    // Must do *after* setmodel.
    setorigin (e, org);     // Updates links immediately, so do last.
};

//==========================================================================
//  Miscellaneous Junk

//- - - - - - - - -
// How games handled pain in Nightmare.
// Doom:  Monsters take pain normally.  However, most monsters didn't
//  flinch reliably, and when they did, had short delays.
// Quake1:  Immune to pain for five seconds after taking damage.
// Quake2:  Immune to pain.
//
// Complete immunity to pain is not fun, and makes some monsters much
// more dangerous than normal.  For instance, the lowly grunt with his
// hitscan shotgun.
//- - - - - - - - -
// Checks if monster won't flinch from pain.
float() SUB_IgnorePain = {
    if (skill >= 3)
    {   // Nightmare or Hell.
        if (random() < 0.5)     // Update 1/5/10:  Go 50/50 instead of 100%.
            return TRUE;
    }
    return FALSE;
};

//- - - - - - - - -
// Checks if enemy is within melee range.  def_range is defined melee range.
// Assumes self has an enemy and can damage said enemy.
float(float def_range) InMeleeRange = {
    local   float   dist;

    if (!self.enemy.modelindex)
        if (self.enemy.solid == SOLID_NOT)
            return FALSE;       // Phased out of the level.

    dist = vlen(self.enemy.origin - self.origin);
    if (dist > def_range)
        if ( dist > ((self.maxs_x + self.enemy.maxs_x) * SQRT2 + 10) )
            return FALSE;

    return TRUE;    // Within range.
};

// Variant of 'FacingIdeal' in which TRUE is returned only if difference
// between self's yaw and ideal yaw is one-half of a degree or less.
// Used by patrol related code.
float() MatchingIdeal = {
    local   float   delta;
    
    delta = rint(self.angles_y - self.ideal_yaw);
    delta = anglemod(delta);
    return !delta;
};

// Add one to the global float.  Used to help calculate attack patterns for
// some monsters.  Update:  Rollover after 8-bit limit.
float() AddOne = {gloat = (gloat + 1) & 255;  return gloat;};

// From Drake
// DRAGONS /- - - -
//==========================================================================
//  TEMP GOAL (Custom addition)
//
//  'walkmove' and 'movetogoal' always causes a flying monster to drift
//  vertically toward edict.enemy by 8 per call.  In addition, if the
//  drift fails, the whole move aborts and no horizontal move is done.
//
//  Therefore, create a dummy entity that can be swapped in temporarily
//  as the enemy.
//==========================================================================
entity  temp_goal;

// Used to control movement of flying/swimming entities.
void() InitTempGoal =
{
    temp_goal = spawn();
    temp_goal.classname = "goal";
};

// From Drake
//==========================================================================
//  MOVE BOX (Custom addition)
//
//  Quake checks movement with only one of three hull sizes:
//  point-size, human-size, and shambler-size.  Anything bigger than
//  shambler-size will clip walls because it is bigger than the bbox
//  used by the Quake engine for movement.
//
//  Therefore, create a dummy entity that can test movement multiple
//  times on behalf of the monster to be moved.  The move box is
//  placed at the corners of the monster, and if all move checks are
//  good, the monster will be allowed to move.
//==========================================================================
entity  move_box;

// Used by big entities to prevent clipping of walls.
void() InitMoveBox =
{
    move_box = spawn();
    setsize (move_box, VEC_HULL2_MIN, VEC_HULL2_MAX);
    move_box.solid      = SOLID_NOT;
    move_box.movetype   = MOVETYPE_STEP;
    move_box.classname  = "move_box";
};

//==========================================================================
//  Safe Movement Functions

// The following two "safe_" functions help improve stability at the
// cost of speed.  Monsters should have th_move and th_goal defined,
// but in case they do not, do something to prevent a crash by call to
// undefined function.
float(float yaw, float dist) safe_move =
{
    if (self.th_move)
        return self.th_move (yaw, dist);
// If the first check failed, that means self.th_move was undefined,
// and the game would have crashed due to a call to an undefined function.
    if (self.movetype == MOVETYPE_NOCLIP)
        return noclip_move (yaw, dist);
// Disabled:  Monster should call swim_* via self.th_*.
//    if (self.flags & FL_SWIM)
//        return swim_move (yaw, dist);
    return walkmove (yaw, dist);
};

void(float dist) safe_goal =
{
    if (self.th_goal)
        self.th_goal (dist);
// If the first check failed, that means self.th_goal was undefined,
// and the game would have crashed due to a call to an undefined function.
    else
    {
        if (self.movetype == MOVETYPE_NOCLIP)
            noclip_goal (dist);
//        else if (self.flags & FL_SWIM)
//            swim_goal (dist);
        else
            movetogoal (dist);  // The original builtin.
    }
};

//==========================================================================
//  Noclip Movement -- Used by wraiths and other such ghostly monsters.

//============
// This is 'walkmove' for monsters with noclip movement.  Based on
// 'SV_movestep'.  Noclip entities are capable of unrestricted 3D movement.
//
// NOTE:  This function *always* returns TRUE.
// This function is float type for convenience (i.e., cut-and-paste code).
//============
float(float yaw, float dist) noclip_move =
{
    local   vector  move;

    move = '0 0 0'; move_y = yaw;
    makevectors (move);
    move = v_forward * dist * 10;
    if (self.enemy)
    {
        local   float   delta;

        delta = self.origin_z - self.enemy.origin_z;
        if (delta < 30)
            move_z = move_z + 80;
        else if (delta > 40)
            move_z = move_z - 80;
    }
    self.velocity = move;
    return TRUE;
};

// Based on 'SV_StepDirection'.
// Originally float type, but noclip movement cannot be blocked.
void(float yaw, float dist) noclip_step =
{
    self.ideal_yaw = yaw;
    ChangeYaw ();

    if (FacingIdeal ())
        noclip_move (yaw, dist);
    else
        self.velocity = '0 0 0';    // Didn't turn enough so don't move.
};

// Based on 'SV_NewChaseDir'.
void(float dist) noclip_chase =
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
            {noclip_step (turn, dist); return;}
    }

// Try other directions.
    local   float   ax, ay;

    ax = fabs(delta_x);
    ay = fabs(delta_y);
    if ((random() < 0.333) || (ay > ax))
        {turn = d1; d1 = d2; d2 = turn;}

    if (d1 != -1)
        if (d1 != back)
            {noclip_step (d1, dist); return;}
    if (d2 != -1)
        if (d2 != back)
            {noclip_step (d2, dist); return;}

// No direct path to goal, so pick another way.
    noclip_step (old, dist);
};

//============
// This is 'movetogoal' for monsters with noclip movement.
// Based on 'SV_MoveToGoal'.
//============
void(float dist) noclip_goal 
{
// Noclip entities have full, unrestricted 3D movement.

// If next step hits enemy, stop... but only if wraith is not in a wall.
// Why?  Because the wraith can't attack effectively if pinned in the wall.
    if (self.enemy)
        if (QC_CloseEnough (dist))
            if (pointcontents (self.origin) != CONTENT_SOLID)
                {self.velocity = '0 0 0'; return;}

    if (random() < 0.333)
        noclip_chase (dist);
    else
        noclip_step (self.ideal_yaw, dist);
};

//=============================================================================

// Spawns a muzzleflash.  Used by phased out monsters with a modelindex
// of zero, who cannot display effects while modelindex is zero.
// Update 9/5/09:  Also by players for some weapons to get around
// Qrack's custom client muzzleflashes.
void() SUB_Flash = {
    local   entity  e;

    e = spawn();
    e.movetype  = MOVETYPE_NONE;    //MOVETYPE_NOCLIP;
    e.velocity  = '0 0 0';          //self.velocity;
    e.touch     = SUB_Null;
    e.solid     = SOLID_NOT;
    e.effects   = EF_MUZZLEFLASH;       // The point of this function.
    e.nextthink = time + 0.1;
    e.think     = SUB_Remove;
    // Non-zero invisible model.
    setall (e, "progs/null.spr", '0 0 0', '0 0 0', self.origin); //was null_256
};

// Finds a random player in the game.
entity() RandomPlayer = {
    local entity e;
    local float loop;

    e = find (world, classname, "player");
    if (deathmatch || coop) {
        loop = 0;
        while (e) {
            loop = loop + 1;
            e = find (e, classname, "player");
        }
        loop = loop * random();
        loop = ceil (loop);
        e = world;
        do {
            e = find (e, classname, "player");
            loop = loop - 1;
        } while (loop > 0);
    }
    return e;
};

// DRAGONS /- - - -
//  This sets an entity's waterlevel and watertype in a way similar to
//  the C code.
void(entity ent) Water_Set = {
    local   vector  spot;

    spot = ent.origin;
    spot_z = spot_z + ent.mins_z + 1;
    ent.h2otype = pointcontents (spot);
    if (ent.h2otype <= CONTENT_WATER) {
        ent.h2olevel = 1;
        spot_z = ent.origin_z + (ent.mins_z + ent.maxs_z) * 0.5;
        if (pointcontents (spot) <= CONTENT_WATER) {
            ent.h2olevel = 2;
            spot_z = ent.origin_z + ent.view_ofs_z;
            if (pointcontents (spot) <= CONTENT_WATER)
                ent.h2olevel = 3;
        }
    }
    else
        ent.h2olevel = 0;
};

// Searches all entities to do pre-thinking stuff for monsters.
void() WorldScan = {
    local   entity  e;

    e = nextent (world);
    while (e)
    {
        // For now, just check for water stats for dragons.
        // Used by dragons to check movement.
        if (e.flags & FL_MONSTER)
            if (e.classtype == CT_MONRYU || e.classtype == CT_MONWYRM)
                Water_Set (e);

        e = nextent (e);
    }
};
//- - - - - - - - -

float(entity e) IsDead = {return ((e.health <= 0) || e.deadflag);};

/*
==============================================================================

MULTI-DAMAGE

Collects multiple small damages into a single damage

==============================================================================
*/

entity    multi_ent;
float    multi_damage;

void() ClearMultiDamage =
{
    multi_ent = world;
    multi_damage = 0;
};

void() ApplyMultiDamage =
{
    if (!multi_ent)
        return;
    T_Damage (multi_ent, self, self, multi_damage,DAMARMOR);
};

void(entity hit, float damage) AddMultiDamage =
{
    if (!hit)
        return;
    
    if (hit != multi_ent)
    {
        ApplyMultiDamage ();
        multi_damage = damage;
        multi_ent = hit;
    }
    else
        multi_damage = multi_damage + damage;
};

//==========================================================================
// MULTI-DAMAGE
//
// Collects multiple small damages into a single damage
//
// PM:  Damage that is affected by damage flags should be handled
// in the function that calls any of MultiDamage_xxx, not here.
// That is because in case I may want weapons that shoot multiple
// attacks that have different properties, yet hit simultaneously.
//==========================================================================
// PM:  Globals moved to global.qc.

void() MultiDamage_Clear = {multi_ent = world;  multi_damage = 0;  multi_headinjury = 0;};

void(string dtype) MultiDamage_Apply =
{
    if (multi_ent)
    {
        T_Damage (multi_ent, self, self, multi_damage,DAMARMOR);
    }
};

void(entity hit, float damage, float trauma, string dtype) MultiDamage_Add =
{
    if (!hit)
        return;

    if (hit != multi_ent)
    {
        MultiDamage_Apply (dtype);
        multi_headinjury = trauma;
        multi_damage = damage;
        multi_ent = hit;
    }
    else
    {
        multi_headinjury = multi_headinjury + trauma;
        multi_damage = multi_damage + damage;
    //- - - - - - - - -
    // If the target takes enough damage to gib, damage it now.
    // Gibbing makes the target smaller and non-solid, allowing the remaining
    // shotgun pellets to hit more targets behind the one to be gibbed.
    // Note:  Also works for IK's antimatter gun.
    //- - - - - - - - -
    /*
        if (hit.th_gib)
            if (hit.health >= hit.gib_health)
        {
            local   vector  dv;     // x=damage taken, y=armor damage, z=x+y

            dv = CalcDamage (hit, self, multi_damage, DF_NONE);
            if (dv_x > (dv_y + hit.health - hit.gib_health))
            {   // Enough damage to gib the target, so do it now!
                if (hit.health < 0) // HACK:  Make it easier to gib dead guys.
                    multi_damage = multi_damage - hit.health;
                MultiDamage_Apply (dtype);
                MultiDamage_Clear ();
            }
            else
            {
            //- - - - - - - - -
            // Hack:  If zombie, treat head damage as double.  This means if
            // zombie has 60 health, it takes only 30 to the head to kill.
            // Headshot damage must be 25 or more to decapitate.
            //
            // Update 6/11/2011:  Only if not in classic mode.
            //- - - - - - - - -
                if (hit.classname == "monster_zombie")
                if (!q1shot)
                if (multi_headinjury >= 25)
                if (multi_headinjury * 2 >= hit.health)
                if (dv_x * 2 >= hit.health)
                {
                    multi_damage = multi_damage * 2;    // Boom! Headshot!
                    MultiDamage_Apply (dtype);
                    MultiDamage_Clear ();
                }
            }
        }*/
    }
};

//==========================================================================
//  Particle Beam

// Spawns a beam of particles.
void(vector start, vector end, float color1, float color2,
    float density, float full)  Particle_Beam =
{
    local   vector  delta;
    local   float   points;
    local   float   dist;
    local   vector  next;

    delta = end - start;
    dist = vlen(delta);

    if (full)
    {   // Unbroken, continuous trail.  Used by meteor.
        points = dist / 24;
        points = ceil(points);
        dist = -24;
    }
    else
    {   // Sparse, broken trail.  Useful for frost trails.
        if (deathmatch || coop)
            points = 2;
        else
            points = 3;
        dist = dist / points;
        dist = 0 - dist;
    }

    delta = normalize(delta);
    next = delta * dist;
    full = color1;

    while (points > 0)
    {
        particle (end, delta, full, density);
        end = end + next;
        points = points - 1;
        if (full == color1)
            full = color2;
        else
            full = color1;
    }
};

// Used by shotguns and other hitscan scatterguns such IK's Asha gun.
// Update:  Also used by flare gun/ruby wand.
// Note:  'damage' is used only to check reflection against courage armor.
// 'sparks' -- x=color1, y=color2, z=density (0=none).
void(vector org, vector dir, float dist, float damage, entity attacker,
    vector sparks)  Lead_Hitscan =
{
    local   entity  ignore;
    local   vector  end;
    local   float   loop;

    ignore = attacker;
    loop = 3;
    while (loop > 0)
    {
        end = org + dir*dist;
        traceline (org, end, FALSE, ignore);
        if (sparks_z >= 1)
            Particle_Beam (org, trace_endpos, sparks_x, sparks_y, sparks_z, TRUE);
        if (trace_fraction != 1.0)
        {
            /*if (HasReflection (trace_ent, trace_endpos, damage))
            {
                ignore = trace_ent;
                org = trace_endpos;
                dir = normalize (org - ignore.origin);
                dist = dist * (1 - trace_fraction);
                ReflectSound (trace_ent);
            }
            else*/
                return;
        }
        else
            return;

        loop = loop - 1;
    }
// Reflected too many times, so fake a miss.
    trace_fraction = 1.0;
    trace_ent = world;
};

// Sound to be played when an attack is reflected.
void(entity ent) ReflectSound =
{
// Play sound only if entity carries an artifact that provides reflection.
// That is, don't play sound for warlocks who cast a reflection spell.
    if (ent.mirror_finished)
    {
        if (ent.flags & FL_CLIENT)
            if (ent.mirror_sound < time)
            {
                ent.mirror_sound = time + 2;    // t+1 boings too much.
                sound (ent, CHAN_ITEM, "items/reflect3.wav", 1, ATTN_NORM);
            }
    }
};

float(entity e) ImmuneToDamage = {
    if (!e.takedamage)
        return TRUE;
    if (e.flags & FL_GODMODE)
        return TRUE;
    return (e.invincible_finished);
};

// Checks if entity 'e' can grunt in pain.
float(entity e) FeelsPain = {
    if (e.health > 0)
        if (e.th_pain)
            if (e.th_pain != SUB_Null)
                if (e.pain_finished <= time)
                    if (!ImmuneToDamage (e))
                        return TRUE;

    return FALSE;
};