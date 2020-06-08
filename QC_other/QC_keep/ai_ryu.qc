.float  dodge_back;     // Time to reverse dodge, sort of like lefty.
float   RYU_RANGE_MELEE = 120;
float   RYU_RANGE_BEST  = 750;      // Maximum range for shot spray.
float   RYU_RANGE_MAX   = 2000;     // Maximum attack range.
float   SPEED_RYU_MIN   = 8;
float   SPEED_RYU_SLOW  = 20;

entity() RandomPlayer;

// edict.spawnflags
float   SPAWN_WYRM  = 2;    // Upgrades dragon to full Qtest-sized.
float   SPAWN_FAST  = 4;
float   SPAWN_SLOW  = 8;
float   SPAWNx_SKILL= 12;   // Combo - 4+8.

////////////////////////////////////////////////////////////////////////////
//  1.1.  Custom MoveToGoal AI                                            //
//                                                                        //
//  The fundamentals!  Used when neither 'walkmove' nor 'movetogoal'      //
//  alone are good enough.  This takes care of the problems of water      //
//  immersion and wall clipping.                                          //
////////////////////////////////////////////////////////////////////////////

//==========================================================================
//  Basic Movement
// Checks if the dragon should be allowed to move freely, or horizontal only.
// TRUE = Free move allowed, FALSE = Flat move only.
float() Ryu_UseFreeMove = {
    if (!self.enemy)
        return TRUE;    // Movement is automatically flat.
    if (self.h2olevel)
        return TRUE;    // Get out of the water ASAP.

//- - - - - - - - -
// The point of flat movement is to prevent the dragon from entering water.
// Builtin movement functions move the dragon 8 pixels toward its enemy
// per call.  Therefore, plot a point 7 pixels away from the bbox edge.
// If the point is in an offending liquid, use flat movement code to
// prevent entry.
//
// Also, if the dragon has a bbox maxs_z greater than 64, use flat movement
// to prevent (or greatly reduce the chance of) the dragon from clipping
// the ceiling.
//- - - - - - - - -
    local   vector  spot;
    local   float   diff, pc;

// NOTE:  Checks current point only, not post-move point.  Fix this!
    spot = self.origin;
    diff = spot_z - self.enemy.origin_z;
    if (diff < 30)
        spot_z = spot_z + self.maxs_z + 7;  // Going up.
    else if (diff > 40)
        spot_z = spot_z + self.mins_z - 7;  // Going down.
    else
        return TRUE;    // Already level, so no need to use flat move code
                        // because the engine code will do that for us.

    pc = pointcontents(spot);

// Don't let big dragons clip the ceiling.
// NOTE:  Doesn't always work -- fix.
    if (pc == CONTENT_SOLID)
        if (self.maxs_z > 64)
            return FALSE;   // Stop huge dragons from clipping the ceiling.

// Check for water.  Dragons who are afraid of water never go in.
// Dragons not afraid of water may enter water, but NOT slime/lava,
// unless the dragon has invunlerability.  No dragon may enter a sky brush.
    if (self.ryuflags & RYU_NO_WATER)
        diff = CONTENT_WATER;   // Never go into water no matter what.
    else if (self.invincible_finished > time + 3)
        diff = CONTENT_SKY;     // Invulnerable!
    else if (self.cross_finished > time + 3)
        diff = CONTENT_SKY;     // Protected from elements by cross.
    else
        diff = CONTENT_SLIME;   // Even those who can swim won't enter this.

// If test spot goes into the water, use flat move code.
    return (pc > diff);
};

float(float yaw, float dist) Ryu_WalkMove = {
    if (Ryu_UseFreeMove ())
        return walkmove (yaw, dist);
    return flat_move (yaw, dist);
};

void(float dist) Ryu_MoveToGoal = {
    if (Ryu_UseFreeMove ())
        movetogoal (dist);
    else
        flat_goal (dist);
};

////////////////////////////////////////////////////////////////////////////
//  1.2.  Dragon Movement AI                                              //
//                                                                        //
//  Does collision checking as well as detecting and dodging projectiles  //
//  or other dangerous entities nearby the dragon.                        //
//                                                                        //
//  Also deals with situations that involve water.                        //
////////////////////////////////////////////////////////////////////////////

//==========================================================================
//  Angular Movement

// Reverses angle changes caused by tilting.  Should be called each frame
// unless the dragon needs to tilt.
void() ryu_untilt = {
    if (self.angles_z != 0) {
        if (self.angles_z < -5)
            self.angles_z = self.angles_z + 5;
        else if (self.angles_z < 5)
            self.angles_z = 0;
        else //if (self.angles_z > 5)
            self.angles_z = self.angles_z - 5;
    }
};

// Called whenever the dragon strafes, this tilts the dragon's angles_z.
// Note:  If called by wyrm AI, dist should always be high for big turns.
// FIXME:  Should confirm angles with 'anglemod'.
void(float left, float dist) ryu_tilt = {
    local   float   hi;

    if (dist > 20)
        {dist = 10; hi = 30;}   // Fast -- Running
    else
        {dist = 5; hi = 20;}    // Slow -- Walking

    if (left)
    {   // Going left.
        hi = 0 - hi;
        left = self.angles_z - dist;
        if (left < hi)
            left = hi;
    }
    else
    {   // Going right.
        left = self.angles_z + dist;
        if (left > hi)
            left = hi;
    }
    self.angles_z = left;
};

// Returns the distance the dragon should move while strafing.  The idea is
// the dragon takes time to accelerate to full speed while sidestepping.
// However, the dragon will never move slower than the allowed minimum.
float(float dist) ryu_slide_speed = {
    if (dist <= SPEED_RYU_MIN)
        return SPEED_RYU_MIN;

    // Note:  Make sure the angle divisor matches the fast 'hi' in the
    // above tilt function.
    dist = (dist - SPEED_RYU_MIN) * (fabs(self.angles_z) / 30);
    dist = floor(dist) + SPEED_RYU_MIN;
    return dist;
};

//==========================================================================
//  Vertical Movement
//  This checks if the dragon should stay away from water.  Dragons that
//  only have breath weapons that are ineffective or too dangerous to use
//  underwater are usually afraid of water.  However, a dragon that is on
//  fire will venture into water, even if it normally avoids water.
//  Update:  Monsters can drown or fry in water.  Remember that!
//------------------------------------------------------------------------//
float() Ryu_HatesWater = {
    if (self.invincible_finished <= time + 3)   // Powerups ring at T-3 seconds.
    {
        // Cheap hack -- make the dragon afraid of water when low on air.
        // Give at least five seconds because the dragon may either be in
        // deep water or caught up in battle.
        if (!(self.xfl & XFL_NO_AIR))   // SoE:  Disabled drowning
            if (self.air_finished < time + 5)
                return TRUE;    // Running out of air, so leave the water now!

        // If frying in the hot sauce, get out!  Even the new dragon move AI
        // won't stop it from entering such liquid via teleportation.
        if (self.h2otype <= CONTENT_SLIME)
            return TRUE;    // Burning in slime or lava!
    }

    //if (IsBurning (self))
        //return FALSE;   // Any dragon on fire won't mind taking a dip.
    return (self.ryuflags & RYU_NO_WATER);  // Return fear status.
};

//------------------------------------------------------------------------//
//  This tells the dragon to escape a pool of water.
//------------------------------------------------------------------------//
float(float dist) Ryu_LeaveWater =
{
    // Check if air is above the water (like above a pool or river)
    // from an underwater point.
    traceline (self.origin, self.origin + '0 0 2048', FALSE, self);
    if (trace_inopen)
        if (trace_inwater)
            if (ai_up (dist))
                return TRUE;

    // Removed:  Don't bother checking for air below water.

    // No air is detected, continue on present course.
    return FALSE;
};

//------------------------------------------------------------------------//
//  If a dragon is in the water, this makes the dragon rise out of it.
//------------------------------------------------------------------------//
float(float dist) Ryu_GetOutOfWater = {
    if (Ryu_HatesWater ())
        if (self.h2olevel)
            if (Ryu_LeaveWater (dist))
                return TRUE;

    return FALSE;
};

//------------------------------------------------------------------------//
//  The dragon flies up or down based on current conditions.
//------------------------------------------------------------------------//
void(float dist, entity ent) Ryu_VerticalSlide = {
// If in the unlikely event a water-fearing dragon is in the water,
// find some air and go in the direction it is found.
    if (Ryu_GetOutOfWater (dist))
        return;

// Fly away from... whatever 'ent' is.
    if (ent.origin_z > self.origin_z) {
        // Going down would increase chances of evading ent.
        // Do it only if dragon is already above the goalentity.
        if (self.goalentity)
        {
            local   float   delta;

            delta = (self.origin_z + 30) - self.goalentity.origin_z;
            if (delta < 0)
                return;     // Don't go down when climbing toward a goal.
            if (delta < dist)
                dist = delta;   // Don't descend fully.
        }
        dist = 0 - dist;
    }
    else
    {   // Thinking about going up.
        if (self.goalentity)
            if (self.origin_z - self.goalentity.origin_z > 1000)
                return;     // Don't fly so high up beyond medium range.
    }
// Now we're really going.
    ai_up (dist);
};


//==========================================================================
//  Danger Evasion

//------------------------------------------------------------------------//
//  Fly toward to direction that will most likely dodge the threat.
//------------------------------------------------------------------------//
void(float newyaw, float dist, entity threat) Ryu_Dodge =
{
// Strafe away from the detected danger.  If path is blocked,
// flag it with a time so that future checks use a different path.
    if (!safe_move (newyaw, dist))
    {
        if (self.dodge_back > time)
            self.dodge_back = 0;
        else
            self.dodge_back = time + 0.5 + random();
    }

    local   entity  targ;

    targ = self.goalentity;

// Face its goal.
    self.ideal_yaw = vectoyaw (targ.origin - self.origin);
    ChangeYaw ();

// Next, check if the dragon should slide vertically away from its goal.
    if (targ != threat)
    {
        // For elevation, if dragon is between the goal and threat entity,
        // don't slide vertically away from goal.  We do not want the dragon
        // to move toward the threat now would we?
        local   float   tz, sz;

        tz = targ.origin_z;
        sz = self.origin_z + self.view_ofs_z;
        if (tz < sz)
            {if (sz < threat.origin_z)  return;}
        else if (tz > sz)
            {if (sz > threat.origin_z)  return;}
    }
    Ryu_VerticalSlide (dist, targ);
};

//------------------------------------------------------------------------//
//  Find the best direction to head for.
//------------------------------------------------------------------------//
float(float theta) Ryu_ChangeCourse =
{
    theta = anglemod (theta);
    if (theta < 60)   return -90;   // Dodge right.
    if (theta < 120)  return 0;     // Continue straight ahead.
    if (theta < 180)  return -90;   // Dodge right.
    if (theta < 240)  return 90;    // Dodge left.
    if (theta < 300)  return 0;     // Continue straight ahead.
    return 90;                      // Dodge left.
};

//------------------------------------------------------------------------//
//  Compare the yaws of the dragon's direction and the direction
//  toward the threat, and find the best direction to dodge the
//  threat on the results.
//------------------------------------------------------------------------//
void(float dist, entity ent) Ryu_FindBestPath =
{
    local   float   yaw;

    yaw = vectoyaw (ent.origin - self.origin);
    yaw = Ryu_ChangeCourse (yaw - self.angles_y);
    if (self.dodge_back > time)
        yaw = 0 - yaw;      // Reverse direction if blocked recently.

// Flip the dragon and adjust speed based on its angles.
    if (yaw != 0)
    {   // Sidestep.  +yaw = left, -yaw = right.
        ryu_tilt (yaw > 0, dist);
        dist = ryu_slide_speed (dist);
    }
    else
        ryu_untilt ();

// Set new course and head in that direction if possible.
    Ryu_Dodge (self.angles_y + yaw, dist, ent);
};


//==========================================================================
//  Danger Detection

//------------------------------------------------------------------------//
//  This tells the dragon whether or not it is best to stay away from
//  the entity.  TRUE is returned if the dragon should stay away.
//  FALSE is returned if the dragon is better off closing in on entity.
//  Only alert dragons can spot who the entity is and what kind of
//  weapons, items, or other abilities it has.
//------------------------------------------------------------------------//
float(entity ent) Ryu_KeepAway =
{
// Check for burning first because even the stupid know that fire is bad.
// FIXME:  Burning is slated for removal.
    //if (IsBurning (ent))
        //if (self.resist_fire < 1)
            //if (!self.invincible_finished)
                //return TRUE;    // ent burns, and dragon isn't immune to dmg.

// Begin standard check.
// 6/17/2011:  Disabled alert.  The remaining checks are something even
// an idiot can do.
//    if (!(self.ryuflags & RYU_ALERT))
//        return FALSE;   // Not smart enough to use keep away tactics.
    if (!self.th_missile)
        return FALSE;   // Has no choice but to attack up close.

    // If player, check his items.
    if ((ent.classname == "player") || (ent.classname == "monster_ranger"))
    {   // Must be alive and opposed.
        if (ent.health > 0)
            /*if (!Align_Match (ent, self)) {
            if (ent.invincible_finished || ent.super_damage_finished)
                return TRUE;    // Avoid if using pentagram or quad damage.

            // Check weapons.  Requires alertness to spot weapons.
            if (self.ryuflags & RYU_ALERT)
            if (!ent.form_active)
            {
                // FIXME:  Berserker helm enables leech, but it's limited.
                if (self.tome_finished)
                {   // ToP makes melee weapons leech life.
                    if (ent.war == WAR_AXE)
                        return TRUE;
                    if (ent.war == WAR_CHAINSAW)
                        return TRUE;
                }
                // FIXME:  Rocket check could be removed.
                if (vlen(ent.origin - self.origin) >= 120)
                {   // Far enough away for ent to use big guns safely.
                    if (ent.war & WARx_USES_ROCKETS)
                        return TRUE;
                    if (ent.war == WAR_PLASMA_GUN)
                        return TRUE;
                }
            }
        }*/

        // Close in on the player.
        return FALSE;
    }

    // The rest of the checks apply only if the entity is a live monster.
    if (ent.flags & FL_MONSTER)
    if (ent.health <= 0)
    {
        if (ent.invincible_finished || ent.super_damage_finished)
            return TRUE;    // Avoid if using invulnerability or quad damage.

    // Update 6/2/2011:  Removed .threat field and simplified checks.
        // Stay away from an enemy if it has more health than the dragon,
        // or anytime the dragon has low health and the enemy is melee only.
        // Enemy is defined as any monster targeting the dragon, or the
        // dragon targeting the given monster.
        if ((self.enemy == ent) || (ent.enemy == self))
        {
            if (ent.classname == "monster_tarbaby")
                return TRUE;    // These guys are bouncing bombs -- bad news!

            if (ent.health > self.health)
                return TRUE;

            if (self.health <= 100)
            {   // Dragon becomes more evasive against melee when weakened.
                // After all, a knight cannot kill a dragon he cannot reach.
                if (!ent.th_missile)
                    return TRUE;    // More likely to live by hanging back.
            }
        }
    }

    // It is best to close in on the enemy.
    return FALSE;
};

//------------------------------------------------------------------------//
//  This checks if the entity found is dangerous and should be avoided
//  (if possible).
//------------------------------------------------------------------------//
float(entity ent) Ryu_IsHostile =
{
    // Ignore entities owned by its owner (for obvious reasons).
    if (ent.owner == self)
        return FALSE;

    // Check if entity is an avoidable projectile.
    if (IsMissile (ent))
    {   // Found a projectile.  See if the dragon should avoid it.
        if (ent.enemy == self)
        {   // Homing missile.
            if (ent.yaw_speed >= 180)
                return FALSE;   // Can't dodge this (vore/wrath) missile.
            return infront(ent);    // Dodge only if seen; hope it misses.
        }

        // Check if projectile can hit dragon.
        // NOTE:  32 = 16 for missile clip bonus + 16 for 2 builtin z moves.
        if (ent.velocity_z > 0)
        {   if (ent.origin_z > self.origin_z + self.maxs_z + 32)
                return FALSE;   // Can't hit dragon below if going up.
        }
        else if (ent.velocity_z < 0)
        {   if (ent.origin_z < self.origin_z + self.mins_z - 32)
                return FALSE;   // Can't hit dragon above if going down.
        }

    // Note:  If this section slows play too much, remove and return TRUE.
        // Check distance to projectile now (before) vs. one frame later
        // (after).  If the expected distance increases (before < after),
        // assume projectile is flying away and ignore it.  Otherwise,
        // assume it is incoming and avoid it.
        local   vector  vec;
        local   float   before, after;

        vec = ent.origin - self.origin;
        before = vlen(vec);
        after = vlen(vec + (ent.velocity*0.1));
        return (before >= after);
    //- - - - - - - - -
    }

    // Decide whether or not to stay away from the entity detected.
    return Ryu_KeepAway (ent);
};

//------------------------------------------------------------------------//
//  This searches for dangerous entities near the dragon.
//------------------------------------------------------------------------//
entity() Ryu_FindThreat =
{
    local   entity  head;

    // Return the first threat found (if any).
    // Note:  Proper radius is ((maxs_x / 16) * 25) + 275.
    head = findradius(self.origin, self.maxs_x * 1.5625 + 275);
    while (head)
    {
        if (head != self)
        {
            traceline (self.origin, head.origin, TRUE, self);
            if (trace_fraction == 1)
                if (Ryu_IsHostile (head))
                    return head;    // Got one!
        }
        head = head.chain;
    }

    // If the function made it this far, nothing valid was found.
    return head;
};

//------------------------------------------------------------------------//
//  This checks if a dangerous entity is nearby and if so, get out of
//  its way.
//------------------------------------------------------------------------//
float(float dist) Ryu_DetectDanger =
{
// Find the first entity that poses a threat if any are nearby.
    local   entity  head;

    head = Ryu_FindThreat ();
    if (head)
        {Ryu_FindBestPath (dist, head); return TRUE;}   // Got one!
    return FALSE;   // Nothing dangerous was found.
};


////////////////////////////////////////////////////////////////////////////
//  1.3.  General Dragon AI                                               //
//                                                                        //
//  Dictates the AI of the dragons.                                       //
////////////////////////////////////////////////////////////////////////////

//==========================================================================
//  Dragon Combat

//------------------------------------------------------------------------//
//  Check if dragon is in guard mode.
//------------------------------------------------------------------------//
float() Ryu_InGuardMode =
{
// Other monsters always move if fighting.
    return FALSE;
};

//------------------------------------------------------------------------//
//  This checks if the dragon cannot use its missile attacks because
//  of some detrimental side effect that could result from it.
//------------------------------------------------------------------------//
float(entity ent) Ryu_NoShot = {
    local float element;

    if (ent.h2olevel > 1) {
        element = ent.ryuflags & RYU_ELEMENTS;
        if (element == RYU_USE_LITE)
            return TRUE;    // Lightning is useless underwater.
        else if (element == RYU_USE_ICE) { 
            if (ent.h2otype == CONTENT_LAVA)
                return TRUE;    // Melts in lava!
        }
    }
//- - - - - - - - -
// Update:  Reflector shield.
// Watching attacks bounce off the reflector shield is fun, even if the
// dragon knows better not to attack.  Therefore, dragon must be alert
// to avoid attacking.  (Otherwise, what fun would that be to the player?)
    //if (ent.ryuflags & RYU_ALERT)
        //if (ent.enemy)
            //if (HasReflection (ent.enemy, self.origin, 0))
                //return TRUE;    // Attacks can bounce back.
//- - - - - - - - -
    return FALSE;   // May attack.
};

//==========================================================================
//  Dragon Behavior

//------------------------------------------------------------------------//
//  This checks if the dragon should strafe.
//------------------------------------------------------------------------//
float() Ryu_DoStrafe = {
    //if (ImmuneToDamage (self))
    //    return FALSE;   // No need to circle-strafe if invulnerable.

    // Always strafe if it wants to, especially after flinching from pain.
    if (self.inpain >= time)
        return TRUE;

    if (self.enemy.xfl & XFL_ZOMBIE)
        return FALSE;   // Always approach zombies, unless panicked.
                        // Reason is only melee attacks can hurt them.

    // If inclined to strafe, do it half the time.
    if (random() < 0.5)
    {   // Strafe when enemy attacked recently.
        if (self.enemy.attack_finished >= time)
            return TRUE;
        if (self.enemy.show_hostile >= time)
            return TRUE;
    }

    // Dragon decided not to strafe after all.
    return FALSE;
};

//------------------------------------------------------------------------//
//  The dragon decides whether to strafe or hold still while attacking.
//  Used in missile attacks.
//------------------------------------------------------------------------//
void() Ryu_StrafeOption =
{
    if (Ryu_DoStrafe ())
    {
        self.attack_state = AS_SLIDING;
        if (random() < 0.5)
            self.lefty = !self.lefty;
        return;
    }
    self.attack_state = AS_STRAIGHT;    // Don't strafe.
};

//------------------------------------------------------------------------//
//  Check if an attack can be made.
//------------------------------------------------------------------------//
void() DragonCheckAttack = {
    local   vector  spot1, spot2;
    local   entity  targ;
    local   float   guarding;
    local   float   nofire;     // Melee only if TRUE.
    local   float   chance, reach;

    guarding = Ryu_InGuardMode ();

    // Check if the dragon has a breath weapon.
    if (self.th_missile)
        nofire = FALSE;
    else
        nofire = TRUE;

    // Check if dragon can see the target.
    targ = self.enemy;
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;
    chance = 1;     // Melee attack enabled.

    traceline (spot1, spot2, FALSE, self);
    if (trace_ent != targ)
    {
        // Could not see the target.
        if (trace_fraction < 1)
        {   // Blocked by obstacle.  If blocked by monster, flag it so
            // dragon may try to move around it vertically.
            targ = trace_ent;
            if (!guarding)
            {
                if (targ)
                    if (targ.solid == SOLID_SLIDEBOX)
                        if (targ.health > 0)
                            self.attack_state = AS_SLIDING;
            }
            //if (DontHurtMe (targ))
                //return FALSE;

        // Whoever blocked the way is an enemy too, so continue the check.
            chance = 0;     // Disable melee this time.
            targ = self.enemy;
        }
        else
        {   // Target is non-solid.  (e.g., downed zombies, nightshade.)
            // Dragon can try to engage it in melee, provided the target is
            // vulnerable to damage.  Don't use missile attack because it
            // will pass through the enemy harmlessly.

            // NOTE:  Don't use ghost check because it fails on downed
            // zombies.

            if (!targ.takedamage) return;
// Commented out because those with no mindex should have DAMAGE_NO too.
//            if (!targ.modelindex)
//                return;

            // Update:  Lightning will hit ghosts, so fail only if either
            // does not apply.
            if (self.ryuflags & RYU_ELEMENTS != RYU_USE_LITE)
                nofire = TRUE;
            //else if (!IsGhost (targ))
            //    nofire = TRUE;
        }
    }

    // If target is reachable within melee range, the dragon will attack
    // the target if the dragon has a melee attack.
    if (enemy_range == RANGE_MELEE)
        if (chance)
            if (self.th_melee)
                if (CanDamage (targ, self)) { self.attack_state = AS_MELEE; return ; }

// missile attack

    // Check if the dragon can shoot.
    if (nofire)
    {
        self.attack_state = AS_STRAIGHT;
        return;
    }
    if (Ryu_NoShot (self))
        return;

    if (guarding)
    {
        if (!FacingIdeal ())
            return;

        // Quake2 gave 40% chance for attacking while in standing ground AI.
        if (random() < 0.4)
        {   // If enemy is within range, fire at will.
            local   float   dist;

            dist = vlen (spot2 - spot1);
            reach = RYU_RANGE_MAX;
            if (dist < reach)
            {
                self.th_missile ();
                SUB_AttackFinished (0);
                return;
            }
        }
        return;
    }

    // Check if allowed to attack.
    if (time < self.attack_finished)
        return;

// Determine the chance that the dragon will use its breath weapon.
    if (enemy_range == RANGE_MELEE)
    {   // Any dragon will almost certainly attack within melee range.
        self.attack_finished = 0;
        chance = 0.9;
    }
    else if (enemy_range == RANGE_NEAR)
    {   // Short range (120-500)
        chance = 0.2;
    }
    else
    {
        reach = RYU_RANGE_MAX;
        if (vlen(spot2 - spot1) > reach)
        {   // Target is outside of attack range.
            self.attack_state = AS_STRAIGHT;
            return;
        }
        if (enemy_range == RANGE_MID)
            chance = 0.1;   // Medium range (500-1000)
        else
            chance = 0.02;  // Long range (1000+)
    }

// The dragon decides whether to attack or move on.
    if (random () < chance) {
        local   float   pause;

        pause = random() * 2 + 1;   // Has long attack anim.
        SUB_AttackFinished (pause);   // Reversed in custom 'ai.qc' too.
        self.th_missile ();
        return;
    }
// If the function made it this far, the dragon moves on.
};

//------------------------------------------------------------------------//
//  This checks if the dragon is allowed to make a step toward its enemy.
//  Conditions that force the dragon to either stand its ground or dodge
//  incoming threats causes this function to return FALSE.  Otherwise,
//  the dragon will be allowed to step forward and TRUE will be returned.
//------------------------------------------------------------------------//
float(float melee) ryu_step =
{
    // Turn to face enemy.
    ai_face ();

    if (Ryu_InGuardMode ())
    {
        ryu_untilt ();
        return FALSE;     // Guarding -- don't move.
    }

    /*if (!ImmuneToDamage (self))
    {
        local   float   evade;

        evade = TRUE;
        if (melee)
        {   // Disable evasion if attempting a melee with nearby opponent.
            local   float   r;

            r = (self.maxs_x + self.enemy.maxs_x) * 1.4142 + 30;
            if (r < 120)
                r = 120;    // Standard melee range.
            if (vlen(self.enemy.origin - self.origin) <= r)
                evade = FALSE;      // In range, close in for the kill.
            // If evade is still TRUE, enemy moved out of range, so back off.
        }
        if (evade)
        {
            if (self.ryuflags & RYU_ALERT)
            {
                if (Ryu_DetectDanger (self.speed))
                    return FALSE;
            }
            else
            {
                if (self.attack_state != AS_SLIDING)
                    if (Ryu_DetectDanger (SPEED_RYU_MIN))
                        return FALSE;
            }
        }
    }*/

    // Check if the dragon is in strafe mode.
    if (self.attack_state == AS_SLIDING)
    {
        local   float   dist;

        ryu_tilt (self.lefty, self.speed);

        dist = ryu_slide_speed (self.speed);
        // Replaced 'ai_run_circle (self.speed);' with new bank code.
        // Call 'ai_face' before AND after the move function to circlestrafe.
        ai_face ();
        ai_run_bank (dist);
        ai_face ();

        return FALSE;
    }

// Proceed forward.
    ryu_untilt ();  // Undo any angle changes due to evasion.
    return TRUE;
};

//------------------------------------------------------------------------//
//  The dragon is engaged in melee with its opponent.
//------------------------------------------------------------------------//
void() ryu_melee =
{
    if (ryu_step (TRUE))
        safe_goal (SPEED_RYU_MIN);   // Close in on the opponent.
};

//------------------------------------------------------------------------//
//  Face and close in on the opponent while attacking.
//------------------------------------------------------------------------//
void() ryu_missile =
{
    if (ryu_step (FALSE))
        safe_move (self.angles_y, SPEED_RYU_MIN);    // Move straight in.
};