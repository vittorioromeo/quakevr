// DRAGON (The Qtest/Drake version)
$cd progs/dragon
$origin 0 0 24
$base base 
$skin skin
$frame fly1 fly2 fly3 fly4 fly5 fly6 fly7 fly8
$frame attack1 attack2 attack3 attack4 attack5 attack6
$frame attack7 attack8 attack9
$frame pain1 pain2 pain3 pain4 pain5 pain6
$frame painb1 painb2 painb3 painb4 painb5 painb6
$frame painc1 painc2 painc3 painc4 painc5 painc6
$frame death1 death2 death3 death4 death5 death6 death7 death8 death9
$frame death10 death11 death12 death13
$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6
$frame decap1 decap2 decap3 decap4 decap5 decap6 decap7 decap8 decap9
$frame decap10 decap11 decap12 decap13
$frame decapb1 decapb2 decapb3 decapb4 decapb5 decapb6

//------------------------------------------------------------------------//
//  The dragon hovers in place.  If a water-fearing dragon is
//  underwater, it will escape the pool if possible. 
//
//  The dragon glides along its path.  Like ryu_stand(), a
//  water-fearing dragon will try to escape from any body of
//  water if it is placed in one.
//------------------------------------------------------------------------//
void(float dist) ryu_idle =
{
    if (Ryu_HatesWater ())
    {
        if (self.h2olevel)
        {
            if (Ryu_LeaveWater (self.speed))
                return;
        // Dragon could not rise toward air.  If in walk mode, increase
        // speed in the hope of finding another way toward air faster.
            if (dist)       // In walk mode.
                dist = self.speed;
        }
    }

    ryu_untilt ();  // Undo any angle changes due to evasion.

    // Don't move if guarding.
    if (Ryu_InGuardMode ()) return;

    if (dist) safe_goal (dist);
};

void() ryu_stand = {
    if (FindTarget ())
        return;

    ryu_idle (0);   // This will eject the dragon from water.
    //ai_pet_stay ();
};

void() ryu_walk = {
    if (self.master)    // Warning: 'self.master == self' causes weirdness.
    {
        if (self.ryuflags & RYU_PATROL)
        {
            if (!FindTarget ())     // check for noticing a player
            {
                ryu_idle (0);   // If underwater, get out!
                //ai_pet_patrol (SPEED_RYU_MIN);
            }
        }
        else
            self.th_run ();     // Override walk and run after master.
        return;
    }

    if (FindTarget ())
        return;

    ryu_idle (SPEED_RYU_MIN);
};

//------------------------------------------------------------------------//
//  This checks whether the dragon is allowed to attack its target
//  when one is in air while the other is in liquid.
//------------------------------------------------------------------------//
float() Ryu_BlockCrossfire = {
    if (self.th_missile)
    {
        local   float   elements;

        elements = self.ryuflags & RYU_ELEMENTS;
        if (elements == RYU_USE_LITE)
            return FALSE;   // Shoot lightning into water to fry targets.
        else if (elements == RYU_USE_ICE)
        {   // Don't shoot ice if either endpoint is in lava.
            if (self.h2otype == CONTENT_LAVA)
                if (self.h2olevel > 1)
                    return TRUE;    // Mouth in lava.
            if (self.enemy.h2otype == CONTENT_LAVA)
                //if (self.enemy.h2olevel == 3)
                    return TRUE;    // Target covered under lava.
        }
        // Check alert AFTER element check.
        if (self.ryuflags & RYU_ALERT)
            return FALSE;   // Smart enough to aim through water.
    }
    // Cannot attack at all.
    return TRUE;
};

//------------------------------------------------------------------------//
//  This checks if the dragon's target can be seen.  This is different
//  from the original visible() function in that some dragons can see
//  a swimming target through the water.
//------------------------------------------------------------------------//
float(entity targ) ryu_visible =
{
    local   vector  spot1, spot2;
    
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;
    traceline (spot1, spot2, TRUE, self);   // see through other monsters
    
    // Some dragons can see targets through liquid while others cannot.
    if (trace_inopen)
        if (trace_inwater)
            if (Ryu_BlockCrossfire ())
                if (vlen(self.enemy.origin - self.origin) > 120)
                    return FALSE;

    return (trace_fraction == 1);
};

//------------------------------------------------------------------------//
//  The dragon rotates in place (via multiple calls to this function)
//  until it faces its opponent.  Upon facing an opponent, the dragon
//  will launch an attack.
//------------------------------------------------------------------------//
void() ryu_run_attack =
{
    // Rotate to face opponent.
    self.ideal_yaw = enemy_yaw;
    ChangeYaw ();

    // Check if dragon is facing opponent.  If so, attack!
    if (FacingIdeal ())
    {
        self.attack_state = AS_STRAIGHT;

        // Check for melee attack first.
        if (enemy_range == RANGE_MELEE)
            if (self.th_melee)
            {
                self.th_melee ();
                return;
            }

        // Not close enough for melee, so shoot instead if possible.
        if (self.th_missile)
            if (!Ryu_NoShot (self))
            {
                //if (!IsGhost (self.enemy))
                //{
                    self.th_missile ();
                    return;
               // }
            }
    }

    if (Ryu_InGuardMode ())
        return;     // Guarding -- don't move.

    // Break off the attack if a projectile or some other danger is nearby.
    if (Ryu_DetectDanger (self.speed))
        self.attack_state = AS_STRAIGHT;
};

//------------------------------------------------------------------------//
//  This checks if the dragon is able to attack the target.
//------------------------------------------------------------------------//
float() Draco_CanAttackTarget =
{
    if (!enemy_vis)
        return FALSE;   // Standard.  Must see the target.
    if (!self.enemy.takedamage)
        return FALSE;   // In case mega-enforcer shields stays on long.
    // Don't let water-hating dragons get stuck trying to shoot zombies
    // that are underwater to death -- it is useless.
    if (Ryu_HatesWater ())
        if (self.enemy.xfl & XFL_ZOMBIE)    // Anyone harmed only by scar.
            if (!self.h2olevel)
                if (self.enemy.h2olevel > 2)
                    return FALSE;

    return TRUE;
};

//------------------------------------------------------------------------//
//  This is the dragons' version of 'ai_pet_follow'.
//------------------------------------------------------------------------//
void(float dist) ryu_follow =
{
    // Running while not in combat, so treat this like walking.
    if (FindTarget ())
        return;

    ryu_untilt ();  // Undo any angle changes due to evasion.

    // If the dragon does not like water, stay away from it.
    if (Ryu_GetOutOfWater (self.speed))
        return;

    //ai_pet_follow (dist);
};

//------------------------------------------------------------------------//
//  The dragon is flying around while fighting.  It may attack,
//  dodge missiles, or just simply move toward the opponent at
//  normal running speed.
//------------------------------------------------------------------------//
void() ryu_run =
{
    local   float   guarding, standby;
    local   float   dist;

    dist = self.speed;

    guarding = Ryu_InGuardMode ();
    if (guarding)
        standby = 1;
    else
        standby = 5;    // Standard.

//- - - - - - - - - - - - - -
    if (!self.fighting)
        if (self.master)
            {ryu_follow (dist);  return;}

// see if the enemy is dead
    if (self.enemy.health <= 0)
        if (ai_run_kill ())
            return;
//- - - - - - - - - - - - - -

// DISABLED:  Monsters reveal themselves only when waking up or attacking.
// This change was done due to the new alignment rules.
//    self.show_hostile = time + 1;        // wake up other monsters

// check knowledge of enemy
    enemy_vis = ryu_visible(self.enemy);
    if (enemy_vis)
        self.search_time = time + 5;

// look for other targets if current target is unavailable.
// Do this in single-player and DM because of minions and their alignment.
    if (self.search_time < time)
    {
        // Hasn't seen enemy in a while.  Get out of strafe mode.
        if (self.attack_state == AS_SLIDING)
            self.attack_state = AS_STRAIGHT;

        if (FindTarget ())
            return;

        if (self.master)
            if (self.enemy != self.master)
                {ai_run_done(); return;}  // Stop chase and return to master.
    }

//    enemy_infront = infront(self.enemy);
    enemy_range = range(self.enemy);
    enemy_yaw = vectoyaw(self.enemy.origin - self.origin);

    if (self.attack_state == AS_MELEE)
        {ryu_run_attack (); return;}

    if (guarding)
    {   // If in guard mode, hover in place and face the enemy before attack.
        ryu_untilt ();
        if (enemy_vis)
        {
            self.attack_state = AS_STRAIGHT;
            self.ideal_yaw = enemy_yaw;
            ChangeYaw ();
            DragonCheckAttack ();
        }
        return;
    }

    // Hunt mode.
    if (enemy_vis)
        DragonCheckAttack ();

    // Avoid projectiles or other threats.
    //if (!ImmuneToDamage (self))
        if (Ryu_DetectDanger (self.speed))
            return;

    ryu_untilt ();  // Undo any angle changes due to evasion.

    // If the dragon does not like water, stay away from it.
    if (Ryu_GetOutOfWater (self.speed))
        return;

// Check if dragon is in strafe mode.
    if (self.attack_state == AS_SLIDING)
    {
    // Set enemy to world for the first move so dragon stays at same
    // altitude.  Dragon should not sink toward enemy if it is trying
    // to fly up and away for whatever reason.
        local   entity  e;

        e = self.enemy;
        self.enemy = world;
        safe_goal (dist);
        self.enemy = e;

        Ryu_VerticalSlide (dist, self.enemy);
        return;
    }

// head straight in
    // Fast, larger dragons sometimes have trouble closing in for melee.
    // In close range, check if normal speed is too fast.  If so, fly in
    // at minimum speed.
    dist = self.speed;
    if (enemy_range == RANGE_NEAR)
        if (QC_CloseEnough (dist))
            dist = SPEED_RYU_MIN;
    safe_goal (dist);
};


////////////////////////////////////////////////////////////////////////////
//  1.4.  Wyrm AI                                                         //
//                                                                        //
//  Special AI for huge (full Qtest-sized) boss dragons.                  //
////////////////////////////////////////////////////////////////////////////

// Movement when not on a path.
void() wyrm_glide =
{
    local   entity  swap;
    local   float   sped, ofs, left;

// Fly at walk speed when idle or run speed when mad at someone.
    sped = 20;
    if (self.enemy)
        if (!(self.spawnflags & SPAWN_SLOW))
            sped = 32;

// Change angles.
    ofs = anglemod(self.angles_y - self.ideal_yaw);
    if (ofs >= 180)
        {left = 1;  ofs = ofs - 360;}
    else
        left = 0;
    ChangeYaw ();
    if (fabs(ofs) <= self.yaw_speed)
        {ryu_untilt ();  left = 1;}
    else
        {ryu_tilt (left, 32);  left = 0;  sped = 20;}

// Forward movement.
    swap = self.enemy;
    self.enemy = world;
//- - - - - - - - -
    if (!self.th_move (self.angles_y, sped))
    {   // Something got in the way.
        if (left)
            self.ideal_yaw = anglemod(self.ideal_yaw + 180);
    }
//- - - - - - - - -
    self.enemy = swap;
};

// Normal move.
void(entity targ) wyrm_move =
{
    local   entity  swap, oe;
    local   float   sped, ofs, left, oy, nopath;
    local   vector  ang;

    if (self.movetarget)
        nopath = FALSE;
    else
        nopath = TRUE;

    sped = 20;
    if (self.enemy)
        if (!(self.spawnflags & SPAWN_SLOW))
            if (targ != self.enemy)     // Attacking if FALSE.
                sped = 32;

    self.goalentity = targ;     // In case we need 'movetogoal'.
    self.ideal_yaw = vectoyaw(targ.origin - self.origin);

    // Remember previous angles in case we need to change them back.
    ang = self.angles;
    oy = self.ideal_yaw;
// Change angles.
    ofs = anglemod(self.angles_y - self.ideal_yaw);
    if (ofs >= 180)
        {left = 1;  ofs = ofs - 360;}
    else
        left = 0;
    ChangeYaw ();
    if (fabs(ofs) <= self.yaw_speed)
        ryu_untilt ();
    else
        {ryu_tilt (left, sped);  sped = 20;}

// Forward movement.
    swap = self.enemy;
    if (nopath || (targ != self.enemy))
        self.enemy = world;

    oe = self.enemy;
    if (!self.th_move (self.angles_y, sped))
    {   // Something got in the way.
        if (!self.enemy)
        {   // Restore previous angles then bump around with movetogoal.
            self.ideal_yaw = oy;
            self.angles = ang;
            self.th_goal (sped);
            ryu_untilt ();
        }
    }
    self.enemy = oe;    // In case of change by trigger_hurt and the like.

// Vertical movement.  Must have a path to be allowed.
    if (!nopath)
        if (targ)           // If FALSE, no target to follow.
            if (!self.enemy)    // If FALSE, done already with builtin move.
            {
                left = targ.origin_z - self.origin_z;
                if (left > 8)
                    left = 8;
                else if (left < -8)
                    left = -8;
                ai_up (left);
            }

    self.enemy = swap;
};

// Checks if a target is inview for the wyrm.
float(entity targ) wyrm_inview =
{
    local   vector  spot1, spot2;

    if (targ.health <= 0)
        return FALSE;   // Dead or the world -- No target!
    if (!targ.modelindex)
        return FALSE;   // sf64 and/or eth wraith.  Treat as non-existant.
    if (targ != self.enemy)
    {
        if (targ.flags & FL_NOTARGET)
            return FALSE;   // Target has stealth.
        // This is boss AI, so ignore invisibility ring.
    }

// Is the target in front of the dragon?
    makevectors (self.angles);
    spot1 = targ.origin;
    spot1_z = self.origin_z;    // Ignore elevation difference.
    spot1 = normalize (spot1 - self.origin);
    if (spot1 * v_forward < 0)      // Was <= 0.3.
        return FALSE;   // In the rear.

// Can the dragon see the target?
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;
    traceline (spot1, spot2, TRUE, self);
    // This is a boss -- cross contents won't save you, mwahaha!
    if (trace_fraction != 1)
        return FALSE;   // Cannot see target.

    // Note:  Don't bother checking for friendly fire.  Attacks tend
    // to spread out a bit, so a simple traceline check won't cut it.
    // Besides, the boss doesn't care about small fry getting in the way.

    return TRUE;    // "Rebel base in range!" -- Star Wars.
};

// TRUE = Dragon roared and won't attack, FALSE = May attack.
// Roaring stops dragon from attacking instantly, unless in Nightmare.
float(entity targ) wyrm_see_new =
{
    if (!self.enemy)
    {   // Found an enemy, so roar.
        self.enemy = targ;  // Must do before warning other monsters.
        monster_sightsound();
        if (skill < 3)
            return TRUE;    // Just roar only.
    }
    else
    {   // Replace oldenemy with previous enemy.
        if (self.enemy != targ)
            self.oldenemy = self.enemy;
    }
    self.enemy = targ;
    return FALSE;
};

// Scan for an enemy to attack.
void() wyrm_scan =
{
    // Update enemy list if target dies.
    if (self.enemy)
    {
        if (self.enemy.health <= 0)
        {
            self.attack_finished = 0;
            if (self.enemy.classname == "player")
                if (self.enemy.enemy == self)
                {   // Killed a PC, so gloat for a moment.
                    if (self.th_win)
                        self.th_win ();
                }
            if (self.oldenemy.health <= 0)
                self.enemy = world;
            else
                self.enemy = self.oldenemy;
            self.oldenemy = world;
        }
    }

    if (cutscene)
        return;     // Don't search for targets during a cutscene.

// NOTE:  As a wyrm, check for enemies only.
// Don't bother checking for players to serve even if good aligned.
// What the latter means is an NPC wyrm must be triggered to ally with PC.
    // Check for missile attack.
    if (self.attack_finished <= time)
    {
        local   entity  targ, seen;

        seen = world;
        /*if (self.alignment == GOOD)
        {
//            targ = CheckTarget (blacky);
//            if (wyrm_inview (targ))
//                {if (wyrm_see_new (targ)) return;  seen = targ;}
//            if (!seen)
            {
                targ = findradius (self.origin, 2000);
                while (targ)
                {
                    if (targ.alignment == EVIL)
                    if (targ.health > 0)
                    if (wyrm_inview (targ))
                        {if (wyrm_see_new (targ)) return;  seen = targ;}
                    if (seen)
                        targ = world;
                    else
                        targ = targ.chain;
                }
            }
            if (!seen)
            {
            // No bad guy to fry?  Check for rogues.  Don't waste time
            // with clients even if commanded by a player in deathmatch.
                targ = CheckTarget (whitey);
                if (!UnitedWeStand (targ))
                {   // Ally turned bad?
                    if (wyrm_inview (targ))
                        {if (wyrm_see_new (targ)) return;  seen = targ;}
                }
            }
        }*/
        //else
        //{   // Check for a player first, then npc next.
            // Don't use checkclient because wyrm attacks are checked only
            // in one frame once per animation cycle.
            targ = RandomPlayer ();
            if (targ != self.enemy)     // If FALSE, check targ later.
                if (wyrm_inview (targ))
                    {if (wyrm_see_new (targ)) return;  seen = targ;}
            // Divide attacks between pcs and npcs when applicable.
            if (!seen || (random() < 0.5))
            {
                targ = CheckTarget (self.enemy);    // FIXME:  Use findradius?
                if (wyrm_inview (targ))
                    {if (wyrm_see_new (targ)) return;  seen = targ;}
            }
        //}

        if (!seen)
        {   // No fresh meat?  Check its current targets.
            if (self.enemy)
            {
                if (!wyrm_inview (self.enemy))
                {   // Current enemy not found.
                    targ = world;
                    if (self.oldenemy)
                        if (wyrm_inview (self.oldenemy))
                            targ = self.enemy;
                    if (targ)
                    {   // Secondary enemy found, so swap targets.
                        self.enemy = self.oldenemy;
                        self.oldenemy = targ;
                    }
                    else
                    {   // No targets handy.  If target is still visible,
                        // dragon has a small chance to fire anyway.
                        if (random() < (1 - (self.yaw_speed * 0.02)))
                            return;
                        if (!visible (self.enemy))
                            return;
                    }
                }
            }
        }

        if (self.enemy)
        {
            if (self.th_missile)
                self.think = self.th_missile;
        }
    }
};


////////////////////////////////////////////////////////////////////////////
//  2.1.  Dragon Attacks                                                  //
////////////////////////////////////////////////////////////////////////////

//- - - - - - - - - - - - - - - - - - -
//  NOTE:  Use of fields normally unused (by monsters) in standard Quake.
//  Mod-makers beware!  If other custom code uses these fields,
//  make sure they do not interfere with their use by the dragons.
//
// .worldtype = attack type dragon will use; like .weapon for clients.
// .count     = max shots per salvo; like .ammo_shells for clients.
// .height    = shot damage.
// .volume    = shots remaining in a salvo.
//
// Update 9/3/09:  count/volume replaced buy shots_max/shots_left.
//
//  NOTE #2:  There are even more such recycled fields further below.
//- - - - - - - - - - - - - - - - - - -

//==========================================================================
//  Dragon Melee

//------------------------------------------------------------------------//
//  This returns the point of the dragon's mouth relative to its origin.
//  All frames from 8 to 16 are part of the attack scene.
//------------------------------------------------------------------------//
vector() Ryu_Offset =
{
    local   float   fr;

    // HACK:  If nightshade, convert its frames to dragon frames.
    if (self.classname == "monster_nightshade")
        fr = self.frame - 7;    // Nightshade appears only as medium-sized.
    else
        fr = self.frame;

// Now find where the dragon's mouth is.
// FIXME:  Wyrm spawn?
    if (self.maxs_x > 96)
    {   // Wyrm drake/dragon2.mdl.  Assumes +20.
        fr = self.frame;    // Define 'fr'!

        if (fr==16)  return '88 -9 2';    if (fr==15)  return '92 -7 20';
        if (fr==14)  return '94 -1 32';   if (fr==13)  return '86 -1 36';
//        if (fr==12)  return '72 -1 48';   if (fr==11)  return '80 -1 32';
//        if (fr==10)  return '94 -1 24';
        return '96 -1 20';
    }
//    if (self.flags & FL_MONSTER)
    {   // Half-scale drake/dragon1.mdl.  Assumes +2.
        if (fr==16)  return '43 -5 -6';   if (fr==15)  return '46 -3 2';
        if (fr==14)  return '47 -1 8';    if (fr==13)  return '41 -1 12';
//        if (fr==12)  return '35 -1 17';   if (fr==11)  return '39 -1 10';
//        if (fr==10)  return '46 -1 4';
        return '47 -1 3';
    }
};

//------------------------------------------------------------------------//
//  The dragon makes a bite attack.
//------------------------------------------------------------------------//
void() Ryu_Bite = {
    // Check if the bite attack can hit.
    if (!InMeleeRange (RYU_RANGE_MELEE))
        return;
    if (!CanDamage (self.enemy, self))
        return;
    if (!self.enemy.takedamage)   // For mega-enforcer.
        return;

    // Bite attack is successful.
    local   vector  org;        // Location of dragon's head.
    local   vector  dir;        // Velocity of meat chunk.
    local   float   damage;

    org = Ryu_Offset ();

    // Inflict the damage.  Scar damage hurts and kills zombies.
    // Like many of Quake2 monsters' melee attacks, damage ranges from
    // x to x+5, while x is a multiple of 5, and damage has a minimum of 5.
    // Note:  x = ((maxs_x / 16) - 1 + random + cranked) * 5.
    // Update 4/23/2009:  Do 15-20 damage for standard.  Wyrms multiply damage.
    damage = 15 + random()*5;
    if (cranked)
        damage = damage + 5;
    if (self.spawnflags & SPAWN_WYRM)   // Moot since wyrms can't bite.
        damage = damage * 4;
    sound (self, CHAN_WEAPON, "demon/dhit2.wav", 1, ATTN_NORM);
    T_Damage (self.enemy, self, self, damage,DAMARMOR);

    // Throw a piece of meat from the dragon's head (mouth).
    makevectors (self.angles);
    org = self.origin + (v_forward*org_x) + (v_right*org_y) + (v_up*org_z);
    dir = (random() * -50) * v_forward;
    dir = dir + ((random() * 200 - 100) * v_right);
    SpawnMeatSprayDrake (org, dir, self.enemy.bloodtype);
};


//==========================================================================
//  Dragon Aiming
//------------------------------------------------------------------------//
//  This returns the spot where a projectile will strike.  Dragons
//  will shoot at the spot where the target is currently standing.
//------------------------------------------------------------------------//
vector() Ryu_TargetPoint = {
    local vector end;

    // If the dragon has no enemy, aim straight ahead.
    if (self.enemy) {
        end = Midpoint (self.enemy);

        // If target is in water and not the dragon, aim at the head.
        if (self.enemy.h2olevel >= 2)
            if (self.h2olevel < 2)
                end_z = end_z + (self.enemy.maxs_z * 0.5);

        return end;
    }
    // Aiming on its own... Aim forward and slightly downward.
    makevectors (self.angles);
    return (self.origin + v_forward*1024 - '0 0 16');
};


//==========================================================================
//  Missile Attacks

//------------------------------------------------------------------------//
//  This creates a fireball shot by a dragon.  Used by both PCs and NPCs.
//------------------------------------------------------------------------//
void(vector start, vector dir, float sped, float up, float damage) Ryu_Fire = {
    if (damage) {
        // Big super fireball.
        Meteor_Launch (self, start, dir, sped, damage);
        newmis.delay      = time + 2.5;     // Same fuse as a grenade.
        newmis.movetype   = MOVETYPE_TOSS;
    } else {
        // Normal fireball.
        // Fuse is (10 * reach / speed) + 1.  If speed is double the reach,
        // duration is 6 frames.  Fade anim is 3 frames, so the pre-fade
        // duration is 3 frames.
        // NOTE:  Don't use self.height for damage here because of PC use.
        Fireball_Launch (self, start, dir, sped, 9);
        newmis.delay      = time + 0.3;
        newmis.noise      = "dragon/firehit.wav";
        if (Fireball_InWater (newmis))
            return;
    }
    // Note:  Make sure the damage flags field is same one used in 'fire.qc'.
    newmis.style      = newmis.style | DF_SAFE;   // Harmless to attacker.
    if (newmis.movetype == MOVETYPE_TOSS) {
        newmis.velocity_z = newmis.velocity_z + up;
        dir = normalize (newmis.velocity);
        newmis.angles     = vectoangles(dir) + '90 0 0';
    }
    if (self.flags & FL_CLIENT)
        SUB_Flash ();   // Don't light up custom muzzle flashes in Qrack.
    else
        self.effects = self.effects | EF_MUZZLEFLASH;
};

//------------------------------------------------------------------------//
//  Ryu_Multishot (formerly Ryu_Ice)
//
//  This creates a shotgun burst of ice (or whatever).
//
//  The family of the Ps explained:
//  p1 = Origin of attack, generally out of one of the attacker's orifices.
//  p2 = Endpoint 1, generally where the target is when attack is launched.
//  p3 = Endpoint 2, where the target is predicted to be after some time.
//  p4 = Endpoint 3, a point chosen somewhere between p2 and p3.  Aim here!
//------------------------------------------------------------------------//
void(vector p1, vector p2, float sped, float damage, entity targ, float shots, float atype)  Ryu_Multishot = {
    local   float   bomb;
    local   float   sped2;      // New speed for hellball.
    local   float   frac, loop;
    local   vector  dir, err, p3, p4;

    // Extra stuff for more multishot types.
    bomb = atype & RYU_BOMB;
    atype = atype & RYU_ELEMENTS;

    // Begin old multishot stuff.
    err = '6 4.5 0';    // For ice dragon, 5x the spread as fire spray.
    p3 = Aim_Lead (p1, p2, sped, targ);

    loop = floor(shots);
    while (loop > 0) {
        loop = loop - 1;    // Do first because we use 'loop - 1' in calcs.
        if (loop) {
            frac = 1 - (loop / shots);
            if (self.spawnflags & SPAWN_SLOW)
                frac = frac * 0.5;      // Can't fully track if slow.
            p4 = p2 + ((p3 - p2) * frac);
            sped2 = sped * (random() / 3 + 1);  // Boost speed by up to 33%.
        } else {
            // Aim final shot on target.
            err = '0 0 0';
            p4 = p3;
            sped2 = sped;
        }
        traceline (p1, p4, TRUE, self);
        if (trace_fraction < 1)
            p4 = p2;    // Point is blocked, so don't lead.

        // Calculate aim vector.
        if ( bomb && (atype != RYU_USE_LITE) )
            dir = Aim_Toss (p1, p4, sped2, 200, world);
        else
            dir = normalize (p4 - p1);
        dir = Maim_Scatter (dir, err);

        if (atype & RYU_ELEMENTS == RYU_USE_ICE) {
            // Ice spray.
            if (bomb)
            {   // Explosive iceballs.
                Iceball_Toss (p1, dir, sped2, self.height);
                newmis.velocity_z = newmis.velocity_z + 200;
            }
            else
            {   // Ice shards.  Default damage is 9, so raise it if too weak.
                Ice_Launch (p1, dir, sped2);    // Use 'sped' for same speed.
                newmis.delay = time + 0.7;  // Total duration.  Was 0.8.
            }
            if (newmis.dmg < damage)
                newmis.dmg = damage;
        }
        else if (bomb)
        {
            if (atype == RYU_USE_FIRE)
                Ryu_Fire (p1, dir, sped2, 200, self.height);
            else if (atype == RYU_USE_LITE)
                RyuPlasma_Launch (p1, dir, sped2, self.height);
            else if (atype == RYU_USE_ACID)
            {
                Acidball_Launch (p1, dir, sped2, self.height);
                newmis.velocity_z = newmis.velocity_z + 200;
            }
        }
        // ELSE unknown attack type and do nothing.
    }
};


//==========================================================================
//  Missile Spawning

//------------------------------------------------------------------------//
//  This spawns a missile at the dragon's mouth then launches said
//  missile at the target.  The missile cast varies by type.
//------------------------------------------------------------------------//
void() Ryu_Shot_Fire = {
    local   entity  targ;   // Use shot leading if targ != world;
    local   vector  dir;    // Vector from 'start' to 'end'.
    local   vector  start;  // Location of dragon's mouth.
    local   vector  end;    // Place where projectile will strike.
    local   float   sped;   // Missile speed or lightning bolt range.
    local   float   frac;   // Lead coefficient.  0 = no lead, 1 = perfect.

// As with players, pay ammo cost first then attack.
    if (self.shots_left < 1)
        return;     // Out of ammo.

    // Calculate lead coefficient.  The value begins at or close to zero at
    // the start of the salvo, then the value increases with each successive
    // shot until it reaches one at the final shot.
    frac = Frac_ShotDown();     // Lowers 'shots_left', pays ammo cost.
    if (self.spawnflags & SPAWN_SLOW)
        frac = frac * 0.5;      // Can't fully track if slow.

    targ = self.enemy;
    // Shot speed varies by dragon size -- bigger is faster.
    // Faster because of gravity or short range.
    // Note:  Proper speed is ((maxs_x / 16) * 100) + 600.
    // Update 4/23/2009:  1000 for dragons, 1200 for wyrms.
    // Speed may be increased by another 66%, so limit
    // speed to 1200 so that it cannot exceed 2000.
    if (self.spawnflags & SPAWN_WYRM)
        sped = 1200;
    else
        sped = 1000;

    makevectors (self.angles);

    // Find the location of the dragon's mouth (or head).
    start = Ryu_Offset ();
    start = self.origin + (v_forward * start_x) + (v_right * start_y) +
        (v_up * start_z);
    end = Ryu_TargetPoint ();   // Get the spot where dragon is aiming at.

    // Select an attack.
    if (self.worldtype == RYU_USE_FIRE) {
        sped = sped * 1.25;     // Fire spray shots are 25% faster.
        dir = Maim_LineEx (start, end, sped, targ, '1.2 0.9 0', frac);
        Ryu_Fire (start, dir, sped, 200, 0);
        if (self.height > 9)
            newmis.dmg = self.height;   // Extra damage.
            if ( !self.shots_left || (self.shots_left == (self.shots_max - 1)) )
                newmis.effects = newmis.effects | EF_DIMLIGHT;
    } else if (self.worldtype == RYU_USE_ACID) {
        dir = Maim_TossEx (start, end, sped, 200, targ, '0 0 0', frac);
        Acid_Toss (start, dir, sped);
        if (self.height > 9)
            newmis.dmg = self.height;   // Extra damage.
    } else if (self.worldtype == RYU_USE_LITE) {
        // Check for discharge, just like Thunderbolt-wielding player.
        self.effects = self.effects | EF_MUZZLEFLASH;
        if (self.h2olevel > 1) {
            // Dragon is deep enough in water so discharge.
            // Note:  Discharge is weak, so just fizzle harmlessly.
            sound (self, CHAN_WEAPON, "misc/power.wav", 1, ATTN_NORM);
            self.shots_left = 0;
            return;
        }
        sped = RYU_RANGE_BEST;

        // Dragon will aim bolt behind the target during the early frames
        // of a salvo.  This is done to reduce damage from an otherwise
        // unavoidable attack.  An exception is made if the target is in
        // the water, in which case, the aim becomes perfect (or rather
        // the error calcuation is skipped) to simulate water conductivity.
        if (self.enemy)
            if (self.enemy.h2olevel < 2)
                end = end - (self.enemy.velocity * ((1 - frac) * 0.2));

        dir = Maim_Line (start, end, 0, world, '0 0 0');    // Insta-hit.
        end = start + (dir * sped);

    // Create the bolt of lightning.
        LightningBolt2 (start, end, self, self.height);
    } else {
    // Multiple shot attacks.
        if (self.worldtype & RYU_BOMB)
        {   // Super shots
            if (!(self.ryuflags & RYU_ALERT))     // Leading is unfairly hard,
                targ = world;                   // so don't unless cranked.
            if (self.worldtype & RYU_ELEMENTS == RYU_USE_LITE)
                sped = sped - 200;      // Treat as one size less.
        }
        else if (self.worldtype & RYU_ELEMENTS == RYU_USE_ICE)
            sped = sped * 1.25;
        else
            return;     // Invalid attack type.
        self.shots_left = 0;   // Launch all shots in one burst.
        Ryu_Multishot (start, end, sped, self.height, targ, self.shots_max, self.worldtype);
    }
};

// Used for delayed calls to Ryu_Shot_Fire by Ryu_Shot.
void() Ryu_Shot_Think = {
    if (self.owner.health > 0)
        SUB_Think (self.owner, Ryu_Shot_Fire);

    remove (self);
};

// This calls the dragon's missile spawning function now or later.
// If delayed, a dummy entity to spawned to call the delayed function
// at the time designated by 'pause'.
void(float pause) Ryu_Shot = {
    local entity echo;
    if (pause) {
        echo = spawn();
        echo.owner = self;
        echo.nextthink = time + pause;
        echo.think = Ryu_Shot_Think;
    } else
        Ryu_Shot_Fire ();
};


////////////////////////////////////////////////////////////////////////////
//  3.1.  Sounds                                                          //
////////////////////////////////////////////////////////////////////////////
void() ryu_sfx_fly = {
    // Not while Quad is sounding.
    if (self.super_sound < time) {
        if (self.h2olevel <= 1)     // Not while in the water.
            sound (self, CHAN_BODY, "dragon/fly.wav", 1, ATTN_NORM);
        else if ((self.h2otype == CONTENT_WATER) || (self.h2otype == CONTENT_SLIME))
            sound (self, CHAN_BODY, "dragon/flywater.wav", 1, ATTN_NORM);
    }
};

void() ryu_sfx_fire = {
    local float type;

    type = self.worldtype & RYU_ELEMENTS;
    if (type == RYU_USE_FIRE)
        sound (self, CHAN_WEAPON, "dragon/fire.wav", 1, ATTN_NORM);
    else if (type == RYU_USE_ACID)
        sound (self, CHAN_WEAPON, "dragon/acid.wav", 1, ATTN_NORM);
    else if (type == RYU_USE_LITE) {
        if (self.worldtype & RYU_BOMB)
            sound (self, CHAN_WEAPON, "weapons/lstart.wav", 1, ATTN_NORM);
        else {
            sound (self, CHAN_WEAPON, "weapons/lhit.wav", 1, ATTN_NORM);
            sound (self, CHAN_AUTO, "weapons/lstart.wav", 1, ATTN_NORM);
        }
    }
    else if (type == RYU_USE_ICE)
        sound (self, CHAN_WEAPON, "imp/shard.wav", 1, ATTN_NORM);
};

void() ryu_sfx_idle = {sound (self, CHAN_VOICE, "dragon/idle1.wav", 1, ATTN_NORM);};

////////////////////////////////////////////////////////////////////////////
//  3.2.  Animation                                                       //
////////////////////////////////////////////////////////////////////////////
//============
// MOVEMENT
//============
//------------------------------------------------------------------------//
//  Fly animation forward.  Used for stand, walk, and run anims.
//
//  Stand = Dragon hovers in place.
//  Walk  = The dragon slowly glides along its path.
//  Run   = The dragon flies quickly toward the enemy.
//------------------------------------------------------------------------//
void() wyrm_fly_anim = {
    local   float   fadd;

    if ((self.frame > 7) || (self.walkframe >= 13))
        fadd = self.walkframe = 0;
    else if (self.walkframe >= 7)
        fadd = 14 - self.walkframe;
    else
        fadd = self.walkframe;

    self.frame = $fly1 + fadd;
    self.walkframe = self.walkframe + 1;

    if ( self.frame == $fly8 )
        ryu_sfx_fly ();
};

void() ryu_stand1 =[ $fly1, ryu_stand1  ] {wyrm_fly_anim(); ryu_stand();};
void() ryu_walk1  =[ $fly1, ryu_walk1   ] {wyrm_fly_anim(); ryu_walk();};
void() ryu_run1   =[ $fly1, ryu_run1    ] {wyrm_fly_anim(); ryu_run();};

void() wyrm_fly   =[ $fly1, wyrm_fly    ] {
    wyrm_fly_anim();

    // Custom move and attack AI.
    if (self.movetarget)
        wyrm_move (self.movetarget);
    else
        wyrm_glide ();

    if ( self.frame == $fly1 ) wyrm_scan ();
};

void() wyrm_start = {
    if (self.movetarget.classname != "path_corner") {
        self.movetarget = world;
        self.ideal_yaw = self.angles_y;
    }

    self.th_stand = self.th_walk = self.th_run = wyrm_fly;
    wyrm_fly ();
};

void() wyrm_go = {
    if (self.ryuflags & RYU_PATH)
        wyrm_fly ();
    else
        ryu_run1 ();
};


//============
// ATTACKS
//============
void() dragon_bite1  =[ $attack1,  dragon_bite2  ] {W_ShowHostile(); ryu_melee();};
void() dragon_bite2  =[ $attack2,  dragon_bite3  ] {ryu_melee();};
void() dragon_bite3  =[ $attack3,  dragon_bite4  ] {ryu_melee();};
void() dragon_bite4  =[ $attack4,  dragon_bite5  ] {ryu_melee();};
void() dragon_bite5  =[ $attack5,  dragon_bite6  ] {ryu_melee();};
void() dragon_bite6  =[ $attack6,  dragon_bite7  ] {ryu_melee();};
void() dragon_bite7  =[ $attack7,  dragon_bite8  ] {ryu_melee();};
void() dragon_bite8  =[ $attack8,  dragon_bite9  ] {ryu_melee();};
void() dragon_bite9  =[ $attack9,  dragon_bite10 ] {ryu_melee(); ryu_sfx_fly (); Ryu_Bite ();};
void() dragon_bite10 =[ $attack8,  dragon_bite11 ] {ryu_melee();};
void() dragon_bite11 =[ $attack7,  dragon_bite12 ] {ryu_melee();};
void() dragon_bite12 =[ $attack6,  dragon_bite13 ] {ryu_melee();};
void() dragon_bite13 =[ $attack5,  dragon_bite14 ] {ryu_melee();};
void() dragon_bite14 =[ $attack4,  dragon_bite15 ] {ryu_melee();};
void() dragon_bite15 =[ $attack3,  dragon_bite16 ] {ryu_melee();};
void() dragon_bite16 =[ $attack2,  dragon_bite17 ] {ryu_melee(); Ryu_Bite ();};
void() dragon_bite17 =[ $attack1,  ryu_go     ] {ryu_melee(); self.attack_state = AS_STRAIGHT; self.walkframe = 0;};

float(float chance) Ryu_CanBite = {
    // Check if missile attack is effective or not.
    if (!self.th_missile)
        return TRUE;    // Can't shoot.
    if (Ryu_NoShot (self))
        return TRUE;    // Not allowed to fire.
    if (self.enemy.solid <= SOLID_TRIGGER)
        return TRUE;    // Missile attacks pass through non-solid enemies.

    if (Ryu_BlockCrossfire ()) {
        // Not allowed to see into water, so check contents.
        traceline (self.origin, self.enemy.origin, TRUE, self);
        if (trace_inopen)
            if (trace_inwater)
                return TRUE;    // Can't see through water.
    }

    // Now check if the dragon really wants to bite.
    if (random() < chance)
        if (InMeleeRange (RYU_RANGE_MELEE))
            return TRUE;

    // Don't bite -- fire away instead.
    return FALSE;
};

//------------------------------------------------------------------------//
//  Melee attack start.  About midway into the animation, the dragon
//  decides whether to bite or to shoot something.  This is used by
//  smart dragons to foil the 'run in-and-out' strategy used by players.
//  (Also called 'shambler dance' by the authors of Quoth.)
//
//  That is, run in to trigger melee attack, then run away to make said
//  attack whiff.  
//------------------------------------------------------------------------//
void() dragon_strike = {
    local   float   chance;

    if (self.inpain >= time) {
    // Dragon wants to circle-strafe and shoot.
        self.attack_state = AS_SLIDING;
        if (random() < 0.5) self.lefty = !self.lefty;
        chance = 0;
    } else {
    // Try to close in.  High chance of bite.  Always bite zombies
        // or anyone else who is hurt only by scar damage.
        self.attack_state = AS_STRAIGHT;
        if (Ryu_KeepAway (self.enemy))
            chance = 0;     // Always shoot if trying to get away.
        else if (self.enemy.xfl & XFL_ZOMBIE)
            chance = 1;     // Bite causes scar damage.
        else
            chance = 0.8;   // Standard.
    }

    // If dragon can bite the target, do it.
    if (Ryu_CanBite (chance))
        dragon_bite1 ();
    else
        self.th_missile ();   // Shoot at target.  Assumes th_missile exists.
};

//------------------------------------------------------------------------//
//  This makes the dragon decide whether or not to use meteor instead
//  of fire spray.
//------------------------------------------------------------------------//
float() Ryu_UseSpecial = {
    local   float   atype;

    if (self.spawnflags & SPAWN_WYRM)
        return TRUE;    // Big enough to spam multiple destructive blasts.

    // Smaller dragons choose between several small shots or one big blast.
    // Most small shots are faster and fatter but have limited range.
    // Big shots are either slower or bound by gravity, but cause high damage
    // and splash.  A spray of small shots is preferable to one big shot,
    // so if the enemy is in range and not a zombie, spray away!
    atype = self.worldtype & RYU_ELEMENTS;
    if (atype == RYU_USE_FIRE) {
    // If water is in the way, shoot meteors to penetrate it.
        if (self.h2olevel >= 2)
            return TRUE;    // Underwater.  Only meteors are effective.
        if (self.enemy)
            if (self.enemy.h2olevel >= 3)
                return TRUE;    // Target is underwater.
    }
    if (self.enemy) {
    // If enemy is a zombie or too far away, lob big shots.
        if (self.enemy.xfl & XFL_ZOMBIE) {
            // Zombies need high damage to kill.
            if (atype != RYU_USE_FIRE)      // Fire causes scar damage.
                return TRUE;
        }

        // All acid shots have same flight.
        if (atype != RYU_USE_ACID) {
            if (vlen(self.enemy.origin - self.origin) > RYU_RANGE_BEST)
                return TRUE;
        }
    }
    return FALSE;   // Spray small shots.
};

//------------------------------------------------------------------------//
//  This selects the attack used based on type.
//------------------------------------------------------------------------//
void() Ryu_SetAttack = {
    local   float   shots;
    local   float   element;
    local   float   fts;    // Frames To Shoot.
    local   float   multi;  // Multishot attack if TRUE.

// Select attack type.
    element = self.worldtype = self.ryuflags & RYU_ELEMENTS;
    if (Ryu_UseSpecial ()) self.worldtype = self.worldtype | RYU_BOMB;

// Select attack based on type and change the thinking logic accordingly.
//- - - - - - - - - - - - - - - - - - -
// Recycled field alert!
// .wait        = time to add between shots in a salvo.
// .delay       = time between an attack function call and the attack itself.
//- - - - - - - - - - - - - - - - - - -
    if (self.worldtype & RYU_BOMB) {
        if (self.spawnflags & SPAWN_WYRM)
            shots = 4;
        else
            shots = 1;
        if (element == RYU_USE_LITE) 
            self.height = 40;
        else
            self.height = 80;
        if (cranked)    // Increase damage by 50% if true.
            self.height = self.height * 3 / 2;

        self.shots_max = self.shots_left = shots;
        self.wait = 0.6 / shots;
        self.delay = 0.3;
    } else {
    // Generic projectile attacks
        if (cranked)
            shots = 12;
        else
            shots = 8;

        fts = 6;
        multi = FALSE;
        if (self.worldtype == RYU_USE_LITE) {
            shots = ((shots / 2) & 255) + ((random() * 2 * (shots & 1)) & 1);
            self.height = 10;   // Same damage as shambler bolt.
            if (shots > 12) {self.height = shots; shots = 10; fts = 5;}
        } else {
            // Damage.
            self.height = 9;
            // Special:  If too many shots, cap number and increase damage.
            if (self.worldtype == RYU_USE_FIRE)
                {if (shots > 18) {self.height = shots / 2; shots = 18;}}
            else
                {if (shots > 9) {self.height = shots; shots = 9;}}
            // Ice check.
            if (self.worldtype == RYU_USE_ICE)
                multi = TRUE;
        }
        // Shots.
        self.shots_max = self.shots_left = shots;
        // Rate of fire.
        if (shots > fts)
            self.wait = fts / 10 / shots;
        else
            self.wait = 0.1;
        // Attack delay.
        // Note:  Multishot attacks launches all ammo in an instant, so treat
        // them as single-shot.
        if ((self.shots_left <= 1) || multi)
            self.delay = 0.3;  // $attack9
        else if (self.shots_left <= 2)
            self.delay = 0.2;  // $attack8
        else if (self.shots_left <= 4)
            self.delay = 0.1;  // $attack7
        else
            self.delay = 0;    // $attack6
    }
};

// Standard missile attack for dragons.
void() ryu_rapid_fire = {
    if (!self.wait)
        self.wait = 0.1; // Do this to avoid infinite loop.
    else if (self.wait < 0.01)
        self.wait = 0.01; // 10 shots per frame max -- spamtastic.

    // Note:  Start _click with 0.1 or higher to delay attack.
    // 0.1 is for ten frames per second, the default animation rate.
    while (self.delay < 0.1) {
        if (self.shots_left == self.shots_max) ryu_sfx_fire(); // Play sound on the first shot.
        Ryu_Shot (self.delay); // Spawn the attack.
        self.delay = self.delay + self.wait;
    }
    self.delay = self.delay - 0.1;
};

void() dragon_ranged = {
    if (self.ryuflags & RYU_PATH)
        wyrm_move (self.enemy);
    else
        ryu_missile ();
};
void() dragon_missile1  =[ $attack1,  dragon_missile2  ] {
    W_ShowHostile ();
    if (!(self.ryuflags & RYU_PATH))
        Ryu_StrafeOption ();
    dragon_ranged();
};
void() dragon_missile2  =[ $attack2,  dragon_missile3  ] {dragon_ranged();};
void() dragon_missile3  =[ $attack3,  dragon_missile4  ] {dragon_ranged();};
void() dragon_missile4  =[ $attack4,  dragon_missile5  ] {dragon_ranged();};
void() dragon_missile5  =[ $attack5,  dragon_missile6  ] {dragon_ranged();};
void() dragon_missile6  =[ $attack6,  dragon_missile7  ] {dragon_ranged(); Ryu_SetAttack(); ryu_rapid_fire();};
void() dragon_missile7  =[ $attack7,  dragon_missile8  ] {dragon_ranged(); ryu_rapid_fire();};
void() dragon_missile8  =[ $attack8,  dragon_missile9  ] {dragon_ranged(); ryu_rapid_fire();};
void() dragon_missile9  =[ $attack9,  dragon_missile10 ] {dragon_ranged(); ryu_sfx_fly(); ryu_rapid_fire();};
void() dragon_missile10 =[ $attack8,  dragon_missile11 ] {dragon_ranged(); ryu_rapid_fire();};
void() dragon_missile11 =[ $attack7,  dragon_missile12 ] {dragon_ranged(); ryu_rapid_fire();};
void() dragon_missile12 =[ $attack6,  dragon_missile13 ] {dragon_ranged();};
void() dragon_missile13 =[ $attack5,  dragon_missile14 ] {dragon_ranged();};
void() dragon_missile14 =[ $attack4,  dragon_missile15 ] {dragon_ranged();};
void() dragon_missile15 =[ $attack3,  dragon_missile16 ] {dragon_ranged();};
void() dragon_missile16 =[ $attack2,  dragon_missile17 ] {dragon_ranged();};
void() dragon_missile17 =[ $attack1,  ryu_go        ] {
    dragon_ranged();
    if (self.ryuflags & RYU_PATH)
        self.attack_finished = time + (1.2 * (3 - skill));
    self.walkframe = 0;
};

void() dragon_pain1  =[ $pain1,   dragon_pain2  ] {ryu_untilt();};
void() dragon_pain2  =[ $pain2,   dragon_pain3  ] {ryu_untilt();};
void() dragon_pain3  =[ $pain3,   dragon_pain4  ] {ryu_untilt();};
void() dragon_pain4  =[ $pain4,   dragon_pain5  ] {ryu_untilt();};
void() dragon_pain5  =[ $pain5,   dragon_pain6  ] {ryu_untilt();};
void() dragon_pain6  =[ $pain6,   ryu_go     ] {ryu_untilt();};

void() dragon_painb1 =[ $painb1,  dragon_painb2 ] {ryu_untilt();};
void() dragon_painb2 =[ $painb2,  dragon_painb3 ] {ryu_untilt();};
void() dragon_painb3 =[ $painb3,  dragon_painb4 ] {ryu_untilt();};
void() dragon_painb4 =[ $painb4,  dragon_painb5 ] {ryu_untilt();};
void() dragon_painb5 =[ $painb5,  dragon_painb6 ] {ryu_untilt();};
void() dragon_painb6 =[ $painb6,  ryu_go     ] {ryu_untilt();};

void() dragon_painc1 =[ $painc1,  dragon_painc2 ] {ryu_untilt();};
void() dragon_painc2 =[ $painc2,  dragon_painc3 ] {ryu_untilt();};
void() dragon_painc3 =[ $painc3,  dragon_painc4 ] {ryu_untilt();};
void() dragon_painc4 =[ $painc4,  dragon_painc5 ] {ryu_untilt();};
void() dragon_painc5 =[ $painc5,  dragon_painc6 ] {ryu_untilt();};
void() dragon_painc6 =[ $painc6,  ryu_go     ] {ryu_untilt();};

void(entity inflictor, entity attacker, float damage) wyrm_pain = {
    local   float   r;      // Multi-purpose number.
    local   string  sfx;

    if (self.pain_finished > time) return;
    
    self.pain_finished = time + 1;

    if (self.ryuflags & RYU_PATH) {
    // Wyrm:  Spin toward attacker if not on a path.
        // Do only if it can show pain at this time.
        if (!self.movetarget)       // Must not have a path to follow.
            if (attacker)
                if (self.enemy == attacker)
                    if (self.attack_finished <= time)   // Don't spin if it can't attack.
                        if (visible (attacker))
                            self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
    }

    r = 400;
    sfx = "dragon/pain1.wav";
    sound (self, CHAN_VOICE, sfx, 1, ATTN_NORM);
    if ((random() * r) > damage)
        return;         // didn't flinch
    if (damage < self.health * 0.01)    // FIXME:  Use max_health instead?
        return;         // Damage dealt is too low to cause flinching.
    if (SUB_IgnorePain ())
        return;         // No pain anims in Nightmare.

    self.inpain = time + 2; // Strafe soon after pain.
    self.attack_state = AS_STRAIGHT;
    self.attack_finished = 0;       // can attack anytime now!
    self.walkframe = 0;     // Reset fly anim.

    // Choose one of three pain scenes.
    r = random();
    if (r < 0.4)
        dragon_pain1 ();
    else if (r < 0.7)       // Was 0.8 in older Dragons.
        dragon_painb1 ();
    else
        dragon_painc1 ();
};

void() dragon_die1  =[ $death1,   dragon_die2  ] {};
void() dragon_die2  =[ $death2,   dragon_die3  ] {monster_check_gib();};
void() dragon_die3  =[ $death3,   dragon_die4  ] {monster_check_gib(); self.solid      = SOLID_NOT; };
void() dragon_die4  =[ $death4,   dragon_die5  ] {};
void() dragon_die5  =[ $death5,   dragon_die6  ] {/*monster_drop();*/};
void() dragon_die6  =[ $death6,   dragon_die7  ] {};
void() dragon_die7  =[ $death7,   dragon_die8  ] {};
void() dragon_die8  =[ $death8,   dragon_die9  ] {};
void() dragon_die9  =[ $death9,   dragon_die10 ] {};
void() dragon_die10 =[ $death10,  dragon_die11 ] {};
void() dragon_die11 =[ $death11,  dragon_die12 ] {};
void() dragon_die12 =[ $death12,  dragon_die13 ] {monster_death_postcheck();};
void() dragon_die13 =[ $death13,  dragon_die13  ] { monster_deadbody_check(); };

void() dragon_dieb1 =[ $deathb1,  dragon_dieb2 ] {};
void() dragon_dieb2 =[ $deathb2,  dragon_dieb3 ] {monster_check_gib();};
void() dragon_dieb3 =[ $deathb3,  dragon_dieb4 ] {monster_check_gib(); self.solid      = SOLID_NOT; };
void() dragon_dieb4 =[ $deathb4,  dragon_dieb5 ] {};
void() dragon_dieb5 =[ $deathb5,  dragon_dieb6 ] {/*monster_drop();*/monster_death_postcheck();};
void() dragon_dieb6 =[ $deathb6,  dragon_dieb6  ] { monster_deadbody_check(); };

void() dragon_die = {
    monster_death_precheck();

    // Snap tilt angles back to zero now.
    self.angles_x = self.angles_z = 0;

    if (!self.gibbed) {
        self.velocity_x = -200 + 400*random();
        self.velocity_y = -200 + 400*random();
        self.velocity_z =  100 + 100*random();
        self.flags = self.flags - (self.flags & FL_ONGROUND);
        sound (self, CHAN_VOICE, "dragon/death1.wav", 1, ATTN_NORM);
        if (random() < 0.5) {
            // Shove it up a little for the fallback death anim.
            self.velocity_z = 250 + 100*random();
            dragon_die1 ();
        } else
            dragon_dieb1 ();
    }
};

//==========================================================================

void() wyrm_explode = {
    local   entity  swap;
    local   vector  org;
    local   vector  vx, vy, vz;

    // Lavasplash particles form a box with a size of 270x270x64.
    // 135 * 1.4142 = 190.9188.  So use 160 damage for explosion radius of 200.
    // Damage first, then effects.
    T_RadiusDamage (self, self, 160, self, DAMAGEALL);
    sound (self, CHAN_BODY, "boss1/out1.wav", 1, ATTN_NORM);
    Tent_Point (TE_LAVASPLASH, self.origin - '0 0 32');

    // The big fireball at the center.
    swap = self;
    self = spawn();
    setorigin (self, swap.origin + '0 0 24');
    WarpExplosion ();
    self = swap;

    // Spawn dragon parts.
    self.health = -99;      // Some gibs get some velocity.

    org = self.origin;
    makevectors (self.angles);
    vx = v_forward;  vy = v_right;  vz = v_up;

    sound (self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
    // Throw dragon parts first.
    // They will be thrown from a point relative to the body instead of randomly.
    // Since we do not want to update corpse origin, don't use setorigin.
    self.origin = org - vy*72 - vz*20;
    ThrowIt ("progs/drake/dr2_lw.mdl", self.health, GIB_SKIN_MATCH);
    self.origin = org + vy*72 - vz*20;
    ThrowIt ("progs/drake/dr2_rw.mdl", self.health, GIB_SKIN_MATCH);
    if ( self.frame == $deathb6 )
        self.origin = org - vx*72 - vz*20;
    else
        self.origin = org + vx*72 - vz*20;
    ThrowIt ("progs/drake/dr2_tail.mdl", self.health, GIB_SKIN_MATCH);
    self.origin = org;      // Restore origin.

    // Throw random meat chunks.
    ThrowGib (2, 5);
    ThrowGib (3, 2);

    // Throw the head.  Now we can set origin for real.
    if ( self.frame == $deathb6 )
        org = org + (vx * 88);
    else
        org = org - (vx * 88);
    setorigin (self, org);
    ThrowHead ();
};

void() ryu_go = {
    if (self.ryuflags & RYU_PATH)
        wyrm_fly ();
    else
        ryu_run1 ();
};

void() wyrm_countdown = {
    if (self.cnt > 0)
        self.cnt = self.cnt - 1;
    else
        self.think = wyrm_explode;
};

void() wyrm_die1  =[ $death1,  wyrm_die2  ] {};
void() wyrm_die2  =[ $death2,  wyrm_die3  ] {};
void() wyrm_die3  =[ $death3,  wyrm_die4  ] {};
void() wyrm_die4  =[ $death4,  wyrm_die5  ] {};
void() wyrm_die5  =[ $death5,  wyrm_die6  ] {/*monster_drop();*/};
void() wyrm_die6  =[ $death6,  wyrm_die7  ] {death_puff();};
void() wyrm_die7  =[ $death7,  wyrm_die8  ] {};
void() wyrm_die8  =[ $death8,  wyrm_die9  ] {};
void() wyrm_die9  =[ $death9,  wyrm_die10 ] {};
void() wyrm_die10 =[ $death10, wyrm_die11 ] {};
void() wyrm_die11 =[ $death11, wyrm_die12 ] {death_puff();};
void() wyrm_die12 =[ $death12, wyrm_die13 ] {};
void() wyrm_die13 =[ $death13, wyrm_die14 ] {death_puff(); self.nextthink = time + 0.2;};
void() wyrm_die14 =[ $death13, wyrm_die15 ] {death_puff(); self.nextthink = time + 0.3;};
void() wyrm_die15 =[ $death13, wyrm_die16 ] {death_puff(); self.nextthink = time + 0.2;};
void() wyrm_die16 =[ $death13, wyrm_die16 ] {death_puff(); wyrm_countdown();};

void() wyrm_dieb1  =[ $deathb1,  wyrm_dieb2  ] {};
void() wyrm_dieb2  =[ $deathb2,  wyrm_dieb3  ] {};
void() wyrm_dieb3  =[ $deathb3,  wyrm_dieb4  ] {};
void() wyrm_dieb4  =[ $deathb4,  wyrm_dieb5  ] {};
void() wyrm_dieb5  =[ $deathb5,  wyrm_dieb6  ] {/*monster_drop();*/};
void() wyrm_dieb6  =[ $deathb6,  wyrm_dieb7  ] {death_puff(); self.nextthink = time + 0.5;};
void() wyrm_dieb7  =[ $deathb6,  wyrm_dieb8  ] {death_puff(); self.nextthink = time + 0.4;};
void() wyrm_dieb8  =[ $deathb6,  wyrm_dieb9  ] {death_puff(); self.nextthink = time + 0.3;};
void() wyrm_dieb9  =[ $deathb6,  wyrm_dieb10 ] {death_puff(); self.nextthink = time + 0.2;};
void() wyrm_dieb10 =[ $deathb6,  wyrm_dieb10 ] {death_puff(); wyrm_countdown();};

// Special death for the dragon king.
void() wyrm_die = {
    monster_death_precheck();
    
    // Snap tilt angles back to zero now.
    self.angles_x = self.angles_z = 0;

    // No gib check, no decapitation, just a simple death scene.
    // Well, it is like Shub's with an initial dying anim then the big kaboom.
    self.cnt = (random() * 10 + 5) & 15;    // Extra explosions before last.
    self.velocity_x = -200 + 400*random();
    self.velocity_y = -200 + 400*random();
    self.velocity_z =  100 + 100*random();
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    // The wyrm is a boss, so use full volume.
    sound (self, CHAN_VOICE, "dragon/death1.wav", 1, ATTN_NONE);
    if (random() < 0.5) {
        // Shove it up a little for the fallback death anim.
        self.velocity_z = 250 + 100*random();
        wyrm_die1 ();
    }
    else
        wyrm_dieb1 ();
};

//==========================================================================

//============
// REVIVE
//============
void() dragon_revive1  =[ $death12, dragon_revive2  ] {};
void() dragon_revive2  =[ $death11, dragon_revive3  ] {};
void() dragon_revive3  =[ $death10, dragon_revive4  ] {};
void() dragon_revive4  =[ $death9,  dragon_revive5  ] {};
void() dragon_revive5  =[ $death8,  dragon_revive6  ] {};
void() dragon_revive6  =[ $death7,  dragon_revive7  ] {};
void() dragon_revive7  =[ $death6,  dragon_revive8  ] {};
void() dragon_revive8  =[ $death5,  dragon_revive9  ] {};
void() dragon_revive9  =[ $death4,  dragon_revive10 ] {};
void() dragon_revive10 =[ $death3,  dragon_revive11 ] {};
void() dragon_revive11 =[ $death2,  dragon_revive12 ] {};
void() dragon_revive12 =[ $death1,  ryu_run      ] {};

void() dragon_reviveb1 =[ $deathb5, dragon_reviveb2 ] {};
void() dragon_reviveb2 =[ $deathb4, dragon_reviveb3 ] {};
void() dragon_reviveb3 =[ $deathb3, dragon_reviveb4 ] {};
void() dragon_reviveb4 =[ $deathb2, dragon_pain4    ] {};

void() dragon_revive = {
    // Choose revive scene based on current frame.
    self.flags = (self.flags - (self.flags & FL_ONGROUND)) | FL_FLY;
    if ( self.frame == $deathb6 )
        dragon_reviveb1 ();
    else
        dragon_revive1 ();
};

void() dragon_win = { ryu_sfx_idle();  monster_sow(); };

float   RYUSKIN_RED        = 0;
float   RYUSKIN_GREEN      = 1;
float   RYUSKIN_BLUE       = 2;
float   RYUSKIN_WHITE      = 3;
float   RYUSKIN_GOLD       = 4;        // Kept only for Soul of Evil.

//============
// --> QUAKED monster_ryu (1 0 0) (-56 -56 -24) (56 56 56) Ambush Boss Fast Slow
//============
void() monster_ryu = {
    local float powlevel, element, evade;
    if (deathmatch) { remove(self); return; }

    precache_model ("progs/drake/dr1_head.mdl");  // Gib (head)
    precache_model ("progs/drake/dr1_lw.mdl");    // Gib (left wing)
    precache_model ("progs/drake/dr1_rw.mdl");    // Gib (right wing)
    precache_model ("progs/drake/dr1_tail.mdl");  // Gib (tail)
    precache_model ("progs/drake/dragon1.mdl");   // Qtest dragon (various sizes)
    precache_model ("progs/drake/acidbal1.mdl");  // Bile bomb
    precache_model ("progs/drake/flame2.mdl");    // Flames
    precache_model ("progs/drake/flame2a.mdl");   // Flames
    precache_model ("progs/drake/ice.mdl");       // Ice Shard
    precache_model ("progs/drake/iceball.mdl");   // Ice Ball
    precache_model ("progs/drake/plasma.mdl");    // Plasma ball
    precache_model ("progs/drake/s_wave2.spr");   // Ice bolt ripple
    precache_model ("progs/ad171/proj_wiz.mdl");   // Acid shot
    precache_sound ("demon/dhit2.wav");
    precache_sound ("dragon/acid.wav");
    precache_sound ("dragon/death1.wav");
    precache_sound ("dragon/fire.wav");
    precache_sound ("dragon/firehit.wav");
    precache_sound ("dragon/fly.wav");
    precache_sound ("dragon/flywater.wav");
    precache_sound ("dragon/icehit.wav");
    precache_sound ("dragon/idle1.wav");
    precache_sound ("dragon/pain1.wav");
    precache_sound ("dragon/sight1.wav");
    precache_sound ("imp/shard.wav");
    precache_sound ("misc/power.wav");      // Discharge & Fizzle
    precache_sound ("weapons/shatter.wav");
    precache_sound ("wizard/hit.wav");
    precache_sound ("plasma/explode.wav");
    precache_model ("progs/drake/dr2_head.mdl");  // Gib (head)
    precache_model ("progs/drake/dr2_lw.mdl");    // Gib (left wing)
    precache_model ("progs/drake/dr2_rw.mdl");    // Gib (right wing)
    precache_model ("progs/drake/dr2_tail.mdl");  // Gib (tail)
    precache_model ("progs/drake/dragon2.mdl");   // Qtest dragon (full-sized)
    precache_model ("progs/drake/warp2.mdl");     // Big explosion
    precache_sound ("boss1/out1.wav");
    precache_sound ("hknight/hit.wav");     // used by C code, so don't sound2
    precache_sound ("nemesant/bigboom.wav");

    self.gib1mdl = "progs/drake/dr1_lw.mdl";
    self.gib2mdl = "progs/drake/dr1_rw.mdl";
    self.gib3mdl = "progs/drake/dr1_tail.mdl";

    if (self.skin == RYUSKIN_GREEN)        self.ryuflags = RYU_USE_ACID;
    else if (self.skin == RYUSKIN_BLUE)    self.ryuflags = RYU_USE_LITE;
    else if (self.skin == RYUSKIN_WHITE)   self.ryuflags = RYU_USE_ICE;
    else                                   self.ryuflags = RYU_USE_FIRE; //self.skin == RYUSKIN_RED

    // Ryu Elemental stats based on elemental type.
    evade = TRUE;
    element = self.ryuflags & RYU_ELEMENTS;
    if (element == RYU_USE_ACID) {
        /*ent.resist_acid = 0.5;*/ evade = FALSE;
        if (self.deathstring == "")  self.deathstring = " was melted by a Dragon\n";
    } else if (element == RYU_USE_LITE) {
        self.resist_cells = 0.5;
        if (self.deathstring == "")  self.deathstring = " was blasted by a Dragon\n";
    } else if (element == RYU_USE_ICE) {
        /*self.resist_cold = 0.5; self.resist_fire = -0.5;*/ evade = FALSE;
        if (self.deathstring == "")  self.deathstring = " was iced by a Dragon\n";
    } else {
        // Default to fire.
        element=RYU_USE_FIRE;
        /*self.resist_fire = 0.5;
        self.resist_cold = -0.5;*/
        if (self.deathstring == "")  self.deathstring = " was fried by a Dragon\n";
    }

    // Set water evasion flag.
    if (evade) { self.ryuflags = self.ryuflags | RYU_NO_WATER; } else { self.ryuflags = self.ryuflags - (self.ryuflags & RYU_NO_WATER); }

    self.ryuflags = self.ryuflags - (self.ryuflags & RYU_ELEMENTS); // Set breath attack flags.
    self.ryuflags = self.ryuflags | element;

    // Fast = more alert -- better dodging and aiming.
    // Slow = slower speed, poor tracking.
    if (self.spawnflags & SPAWNx_SKILL == SPAWNx_SKILL)
        powlevel = cvar("skill");      // Conflict -- let skill decide.
    else if (self.spawnflags & SPAWN_FAST)
        powlevel = 2;
    else if (self.spawnflags & SPAWN_SLOW)
        powlevel = 0;
    else
        powlevel = 1;

    if (cranked) powlevel = powlevel + 1;      // Just increase awareness.

    // Set speed.
    if (powlevel) {
        self.spawnflags = self.spawnflags - (self.spawnflags & SPAWN_SLOW);
        if (powlevel >= 2)
            self.ryuflags = self.ryuflags | RYU_ALERT;
        self.speed = 32;
    } else
        self.speed = 20;

    self.solid = SOLID_NOT;        // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement

    if (self.spawnflags & SPAWN_WYRM) {
        self.headmdl   = "progs/drake/dr2_head.mdl";
    self.mdl = "progs/drake/dragon2.mdl";
        self.th_stand = self.th_walk = self.th_run = wyrm_start;
        self.ryuflags = self.ryuflags | RYU_PATH;
    if (self.bboxtype < 1) self.bboxtype = BBOX_WYRM;
    if (self.health < 1) self.health = 3000;
    self.gibhealth = MON_NEVERGIB;
        self.th_die     = wyrm_die;
    self.bossflag = TRUE;
    self.classtype = CT_MONWYRM;
    } else {
        self.headmdl   = "progs/drake/dr1_head.mdl";
    self.mdl = "progs/drake/dragon1.mdl";
        self.th_stand   = ryu_stand1;
        self.th_walk    = ryu_walk1;
        self.th_run     = ryu_run1;
        self.th_die     = dragon_die;
        self.th_melee   = dragon_strike;
        self.th_revive  = dragon_revive;
    if (self.bboxtype < 1) self.bboxtype = BBOX_RYU;
    if (self.health < 1) self.health = 750;
    self.gibhealth = -90;
        if (!self.yaw_speed) self.yaw_speed = 20;    // Same as walkmonster.
    }

    self.sight_sound = "dragon/sight1.wav";
    self.th_missile = dragon_missile1;
    self.th_pain    = wyrm_pain;
    self.th_checkattack   = DragonCheckAttack;
    self.th_win     = dragon_win;
    self.th_move    = Wyrm_WalkMove;
    self.th_goal    = QC_MoveToGoal;

    //self.xfl        = XFL_IMMUNITY | XFL_GHOST_BANE;
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    if(!self.classtype) self.classtype = CT_MONRYU;
    if (!self.classgroup) self.classgroup = CG_DRAGON;
    self.classmove = MON_MOVEFLY;

    monster_start ();
};

void() monster_ryu_wyrm = {
    self.spawnflags = self.spawnflags | SPAWN_WYRM;
    self.classname = "monster_ryu";
    if (self.deathstring == "")  self.deathstring = " was obliterated by a Wyrm Dragon\n";
    monster_ryu();
};