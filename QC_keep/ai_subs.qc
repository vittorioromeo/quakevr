/*======================================================================
 AI (monster) FUNCTIONALITY
 
 Visibility checks
 - float(entity targ) visible
 - float(entity targ, vector s_offset, vector t_offset, float nomonsters) visxray
 - float(entity targ) visblocked
 - float (entity targ, vector s_ofset, vector t_ofset) visblocked_wide
 - float(entity targ) infront
 
 Distance checks
 - float(entity targ) range
 - float(entity targ, float flat2d) range_distance
 - float (entity source, entity targ) rangeattack_check
 
 Angle checks
 - float(entity source, entity targ, float offset, float reverse) targangle
 - float(entity source, entity targ) targzone
 
 Hunt & Target Enemies
 - void() HuntTarget
 - void() FoundTarget
 - void(float wakeupothers) FoundHuntTarget
 - float() FindTarget
 
 AI animation movement (used mostly in monsters QC file)
 - void(float dist) ai_forward
 - void(float dist) ai_back
 - void(float dist) ai_pain
 - void(float dist) ai_painforward
 - void() ai_turn
 - float() FacingIdeal
 - void() ai_face
 - void() ai_resetangles
 - void(float dist) ai_charge
 - void(float dist) ai_chargenoturn
 - void() ai_charge_side
 - void() ai_charge_front
 
 AI ATTACK states
 - void() ai_run_melee
 - void() ai_run_missile
 - void() ai_run_jump 
 - void(float dist) ai_run_slide
 - void(float dist) ai_run_sidestep
 - void(float dist) ai_run_backward
 
 AI interactions with Breakable System
 - float(entity source, entity targ, float ignorenoshoot) ai_foundbreakable
 - void(float brkdmg) ai_damagebreakable
 - void(float brkdmg) ai_jumpbreakable
 
 AI melee checks and damage
 - float(float attackdist) ai_checkmelee
 - void() ai_melee
 - void(float dmg_multiplier) ai_meleesmash
 - void() ai_melee_side

======================================================================*/

/*======================================================================
 check_liquidblock
 - Checks pointcontents of source+target because traceline parms
   are not 100% reliable for flagging water/air content
 - One function check for all the different visibility functions
 - Designed to work with the new liquidblock entity flag
======================================================================*/
float(vector org_source, vector org_targ) check_liquidblock =
{
    local float pc_source, pc_targ;
    
    // Is r_wateralpha set or monsters forced to check liquid surfaces?
    if (liquid_alpha == 1 || self.liquidblock) {
        // Check point contents first
        pc_source = pointcontents(org_source);
        pc_targ = pointcontents(org_targ);
        // Are point content different? (like water/air)
        if (pc_source != pc_targ) return TRUE;
        
        // Standard traceline check (not 100% reliable)
        if (trace_inopen && trace_inwater) return TRUE;
    }
    
    // No liquid check required
    return FALSE;
};

/*======================================================================
 visible (target entity)
 Conditions added to water surface check
 - most maps are compiled with transparent water nowadays
======================================================================*/
float(entity targ) visible =
{
    local vector    spot1, spot2;
    
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;
    traceline (spot1, spot2, TRUE, self);    // see through other monsters
    
    // Debug system (enabled via axe) uses funcs from subs_soc.qc
    // This will draw a diamond model where the AI is looking
    if (self.debuglvl) {
        if (!debugent1) debugent1 = spawn_devmarker(self.origin);
        spot1 = vectoangles(self.origin - trace_endpos);
        makevectors(spot1);
        spot2 = trace_endpos + (v_up * -16) + (v_forward * 50);
        setorigin(debugent1,'0 0 0');
        setorigin(debugent1, spot2);
    }

    // Does the world define any water transparency
    //if (liquid_alpha == 1) {    
        // sight line crossed contents
    //    if (trace_inopen && trace_inwater) return FALSE;
    //}
    // Check for liquid surface block condition
    if (check_liquidblock(spot1, spot2) == TRUE) return FALSE;

    if (trace_fraction == 1) return TRUE;
    return FALSE;
};

/*======================================================================
 visxray (variable options)
  Returns TRUE if there is no blocking, FALSE if something in the way
  Custom options for variable source/target offsets and monster blocking
  (nomonsters) TRUE = no block, FALSE = blocked
  self is excluded from any trace starting points
======================================================================*/
float(entity targ, vector s_offset, vector t_offset, float nomonsters) visxray =
{
    local vector    spot1, spot2;

    // Bmodels don't have proper origins
    if (self.bsporigin) spot1 = bmodel_origin(self) + s_offset;
    else spot1 = self.origin + s_offset;

    spot2 = targ.origin + t_offset;
    traceline (spot1, spot2, nomonsters, self);    // custom options
    
    // Does the world define any water transparency
    //if (liquid_alpha == 1) {    
        // sight line crossed contents
    //    if (trace_inopen && trace_inwater) return FALSE;
    //}
    // Check for liquid surface block condition
    if (check_liquidblock(spot1, spot2) == TRUE) return FALSE;

    // monster blocking changes which test to use
    if (nomonsters) {
        // Has the traceline gone from source>target without anything blocking?
        if (trace_fraction == 1) return TRUE;
        return FALSE;
    }
    else {
        // Is the entity hit by the traceline the targ destination?
        if (trace_ent == targ) return TRUE;
        return FALSE;
    }
};

/*======================================================================
 visblocked (target entity)
 check to see if sightline is blocked by other monsters
 Really only used in CheckAttack functions (was in org ID code)
======================================================================*/
float(entity targ) visblocked =
{
    local vector    spot1, spot2;
    
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;
    traceline (spot1, spot2, FALSE, self);    // blocked by monsters
    
    // Does the world define any water transparency
    //if (liquid_alpha == 1) {    
        // sight line crossed contents
    //    if (trace_inopen && trace_inwater) return FALSE;
    //}
    // Check for liquid surface block condition
    if (check_liquidblock(spot1, spot2) == TRUE) return FALSE;

    // Original ID behaviour for this type of function
    // Traceline entity hit = enemy?
    if (trace_ent == targ) return FALSE;
    return TRUE;
};

/*======================================================================
 viswide (target entity)
  check to see if sightline is blocked by other monsters
  extra wide check using 3 traces, +/- 16 left/right and origin
  All three traces have to complete for a positive result
======================================================================*/
float (entity targ, vector s_ofset, vector t_ofset) visblocked_wide =
{
    local vector    spot1, spot2, spot3, tr_line;
    local float tr_leftmon, tr_rightmon, tr_result;

    tr_line = '0 0 0';
    tr_leftmon = tr_rightmon = 0;
    makevectors(self.angles);

    // Traceline 1 - MIDDLE
    spot1 = self.origin + s_ofset_x * v_forward + s_ofset_y * v_right + s_ofset_z * v_up;
    spot2 = targ.origin + t_ofset;
    traceline (spot1, spot2, FALSE, self);

    // Is trace entity the same as the target entity?
    if (trace_ent == targ) tr_line_x = 1;
    // Check for liquid surface block condition
    // Only need to do this once for the central traceline
    // If this fails then the left/right should as well
    if (check_liquidblock(spot1, spot2) == TRUE) tr_line_x = 0;

    //------------------------------------------------------------------    
    // Traceline 2 - 32 units LEFT
    spot3 = spot1 - (v_right * 16);
    traceline (spot3, spot2, FALSE, self);
    // Work out if clear shot, hit world or another entity
    if (trace_ent == targ) tr_line_y = 1;
    else if (trace_ent != world) tr_leftmon = 1;

    if (self.debuglvl) {
        if (!debugent2) debugent2 = spawn_devmarker(self.origin);
        setorigin(debugent2, trace_endpos);
    }
        
    // Traceline 3 - 32 units RIGHT
    spot3 = spot1 + (v_right * 16);
    traceline (spot3, spot2, FALSE, self);
    // Work out if clear shot, hit world or another entity
    if (trace_ent == targ) tr_line_z = 1;
    else if (trace_ent != world) tr_rightmon = 1;

    if (self.debuglvl) {
        if (!debugent3) debugent3 = spawn_devmarker(self.origin);
        setorigin(debugent3, trace_endpos);
    }

    // Default = no clear shot (keep aim/moving)
    tr_result = TRUE;
    // SIMPLE 3 complete traces = nothing blocking visibility
    if (tr_line_x + tr_line_y + tr_line_z == 3) tr_result = FALSE;
    // Central trace is good but either side has hit something?
    else if (tr_line_x) {
        // Hit a monster on either side = always fail!
        if (tr_leftmon || tr_rightmon) tr_result = TRUE;
        // The crssbow bolt is fired from the LEFT side, only right can be clear
        else if (self.classtype == CT_MONDCROSS) {
            if (tr_line_y == 0 && tr_line_z == 1) tr_result = FALSE;
        }
        // Default - space available on either side
        else if (tr_line_y + tr_line_z == 1) tr_result = FALSE;
    }

    if (self.debuglvl) {
        dprint("Trace ("); dprint(ftos(tr_line_x));
        dprint(" "); dprint(ftos(tr_line_y));
        dprint(" "); dprint(ftos(tr_line_z));
        dprint(") Ent ("); dprint(ftos(tr_leftmon));
        dprint(" "); dprint(ftos(tr_rightmon));
        dprint(") = ("); dprint(ftos(tr_result));
        dprint(")\n");    
    }
    return tr_result;
};

/*======================================================================
 infront (target entity)
 - relies on self being set correctly before entering this function
 - returns 1 if the entity is in front (in sight) of self
======================================================================*/
float(entity targ) infront =
{
    local vector vec;
    local float dot;
    
    makevectors (self.angles);    // sets v_forward, etc globals
    vec = normalize (targ.origin - self.origin);
    dot = vec * v_forward;
    
    if ( dot > 0.3) return TRUE;
    else return FALSE;
};

/*======================================================================
 range (target entity)
  returns the range catagorization of an entity reletive to self
  uses constants defined in defs.qc
======================================================================*/
float(entity targ) range =
{
    local vector    spot1, spot2;
    local float        r;
        
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;
    r = vlen (spot1 - spot2);
    
    if (r < MON_RANGE_MELEE) return RANGE_MELEE;        // <120 = MELEE
    else if (r < MON_RANGE_NEAR) {
        if (r < (self.maxs_x + targ.maxs_x) * SQRT2 + 30) // PM:  Extend melee range for huge monsters like the dragon.
            return RANGE_MELEE;
        else
             return RANGE_NEAR;            // <500 = NEAR
    }
    else if (r < MON_RANGE_MID) return RANGE_MID;        // <1000 = MID
    return RANGE_FAR;                                                                    // >1000 = FAR
};

/*======================================================================
 range_distance (target entity)
     returns the vector distance between two points
======================================================================*/
float(entity targ, float flat2d) range_distance =
{
    local vector    spot1, spot2;
    local float        r;
        
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;
    if (flat2d) { spot1_z = spot2_z = 0; }
    r = vlen (spot1 - spot2);
    
    return r;
};

/*======================================================================
 rangeattack_check
  check to see if (source) can get a clear shot at (targ)
  Ignores any water/air content checks
======================================================================*/
float (entity source, entity targ) rangeattack_check =
{
    local vector    spot1, spot2;
    local float r;

    // origin+offset, otherwise cannot see player up/down slopes
    spot1 = source.origin + source.view_ofs;
    spot2 = targ.origin + targ.view_ofs;
    r = vlen(spot1 - spot2);
    
    // Maximum range attack (was 600 taken from shambler, changed to 1000)
    if (r > MON_RANGE_MID) return FALSE;

    // Monsters without melee should always range anything really close
    if (!source.th_melee && r < MON_RANGE_MELEE) return TRUE;

    traceline (spot1, spot2, FALSE, source);    // trace hits monsters (infighting)
    if (trace_ent != targ) return FALSE;            // don't have a clear shot

    // Does the world define any water transparency
    if (liquid_alpha == 1 || source.liquidblock) {    
        // sight line crossed contents
        if (trace_inopen && trace_inwater) return FALSE;
    }

    return TRUE;
};

/*======================================================================
 targangle
  Returns what angle the target entity is relative to the source entity
  Useful for checking where the player is located.
  source, targ = entity origins to create vector angle
  reverse = the creation of the vector can be reversed
  offset = shift the final angle clockwise. (frontal sight cone 315-45)
======================================================================*/
float(entity source, entity targ, float offset, float reverse) targangle =
{
    local float targ_dir, targ_ang;

    // Calculate vector angle between two points (source, target)
    if (reverse) targ_dir = vectoyaw(source.origin - targ.origin);
    else targ_dir = vectoyaw(targ.origin - source.origin);

    targ_ang = anglemod( (source.angles_y - targ_dir) + offset );
    return targ_ang;
};

// Modified version of targangle not using entities but supplied origins
// Targets Y angles needs to be supplied as no entity reference available
float(vector source, vector targ, float offset, float reverse, float dst_ang) viewangle =
{
    local float targ_dir, targ_ang;

    // Calculate vector angle between two points (source, target)
    if (reverse) targ_dir = vectoyaw(source - targ);
    else targ_dir = vectoyaw(targ - source);

    targ_ang = anglemod( (dst_ang - targ_dir) + offset );
    return targ_ang;
};

/*======================================================================
 targzone (source entity, target entity)
 - Checks which zone the target enemy is located in relation to source (self)
  0 = Behind (315-45), 1 = Left (45-135), 2 = Front (135-225), Right = (225-315)
======================================================================*/
float(entity source, entity targ) targzone =
{
    local float target_ang;
    
    // Work out angle the angle of the player based on the monster (source)
    // Add 45 degree offset to shift clockwise the frontal sight cone
    target_ang = targangle(source, targ, 45, TRUE);

    if (target_ang > 270) return TARGET_RIGHT;
    else if (target_ang > 180) return TARGET_FRONT;
    else if (target_ang > 90) return TARGET_LEFT;
    else return TARGET_BACK;
};


/*======================================================================
 HuntTarget
  Main routine - constantly running/turning towards the enemy
======================================================================*/
void() HuntTarget =
{
    if (self.health < 1) return;    // Dead monsters don't hunt!
    //self.fighting       = TRUE;
    //self.search_time    = time + 5;
    self.oldorigin = self.origin;    // Save origin
    self.goalentity = self.enemy;    // Focus on enemy
    self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
    
    // Some monsters have special wakeup animations before attacking
    self.nextthink = time + 0.1;
    self.think = self.th_run;        // Start running
    // Default 1s wait (NM adjusted) before attacking
    
    if (self.ryuflags & RYU_ALERT)
        SUB_AttackFinished (0.5);   // alert monsters don't wait as long...
    else
        if (!self.attack_instant) SUB_AttackFinished (1); // wait a while before first attack    
};

/*======================================================================
 FoundTarget
 - An enemy target has been found, wakeup and hunt target
 - Used by FindTarget once an enemy target has been found
 - used by T_Damage (combat.qc) for infighting
======================================================================*/
void() FoundTarget = {
    if (self.health < 1) return FALSE;    // Dead monsters don't hunt!

    // Check for Liquid surface block entity key
    // if the monster has range attack, can NOW see through liquid surface
    if (self.liquidblock == TRUE && self.th_missile)
        self.liquidblock = FALSE;

    if ( (self.enemy.flags & FL_CLIENT) ) {
        if ( self.charmed ) {
            if ( ((self.charmer == self.enemy) || (self.charmer == self.enemy.charmer)) ) {
                self.enemy = world;
                return ;
            }
        }
        sight_entity = self;            // Highlight an angry monster for others
        sight_entity_time = time;        // reset wakeup timer
    } else {
        if ( self.charmed ) {
            if ( (self.charmer == self.enemy.charmer) ) {
                self.enemy = world;
                return ;
            }
        }
    }
    
    //----------------------------------------------------------------------
    // This will cause a group of monsters to all attack the same target
    // Using the global sight enemy system which works with FindTarget()
    // Always check for an empty sighttarget first because the
    // the global sight entity could have an empty targetname
    //----------------------------------------------------------------------
    if (self.sighttarget != "") {
        if (self.enemy.targetname == self.sighttarget) {
            // Make sure the sight target is alive before chasing it
            sight_entity = self.enemy;        // Highlight target for others
            sight_entity_time = time;        // reset wakeup timer
            monster_sightsound();            // Always make wakeup noise!
        }
    }
    
    self.show_hostile = time + 1;        // wake up other monsters
    // Only play wakeup sound if player, not for other monsters
    if (self.enemy.flags & FL_CLIENT && self.health > 0) monster_sightsound();
    HuntTarget ();                        // Keep running after enemy
};

/*======================================================================
 FoundHuntTarget
 - This is a combination of FoundTarget and HuntTarget
 - Does not do any sight sound, needs to be done outside this func
 - Does not pause or wait for any monster, straight to combat
 - Has no end funtion state, can decide this afterwards
======================================================================*/
void(float wakeupothers) FoundHuntTarget =
{
    if (self.health < 1) return;    // Dead monsters don't hunt!
    if (!self.enemy) return;

    // Check for Liquid surface block entity key
    // if the monster has range attack, can NOW see through liquid surface
    if (self.liquidblock == TRUE && self.th_missile)
        self.liquidblock = FALSE;

    if (self.enemy.flags & FL_CLIENT && wakeupothers == TRUE) {
        sight_entity = self;            // Highlight an angry monster for others
        sight_entity_time = time;        // reset wakeup timer
    }
    
    self.show_hostile = time + 1;        // wake up other monsters
    self.oldorigin = self.origin;        // Save origin
    self.goalentity = self.enemy;        // Focus on enemy
    self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
    SUB_AttackFinished (1);                // wait a while before first attack
};

/*======================================================================
 FindTarget
 - Main routine for finding a PLAYER target
 - Returns TRUE if an enemy was sighted
======================================================================*/
float() FindTarget = {
    local entity client, playtarg, selected, head;
    local float r, dist;

    if (self.health < 1) return FALSE;    // Dead monsters don't hunt!

    if ( self.charmed ) {
        self.effects = (self.effects | EF_DIMLIGHT);
        if ( (self.huntingcharmer > FALSE) ) {
            UpdateCharmerGoal ();
            r = vlen ((self.origin - self.goalentity.origin));
            if ( (r < MIN_CHARMER_DISTANCE) ) {
                if ( ((self.huntingcharmer == MOVETYPE_WALK) && (r > TOOCLOSE_CHARMER_DISTANCE)) ) {
                    return ( FALSE );
                }
                StopHuntingCharmer ();
                return (TRUE);
            }
        } else {
            if ( (vlen ((self.origin - self.charmer.origin)) > MAX_CHARMER_DISTANCE) ) {
                HuntCharmer ();
                return (FALSE);
            } else {
                 if ( (vlen ((self.origin - self.charmer.origin)) < TOOCLOSE_CHARMER_DISTANCE) ) {
                    FleeCharmer ();
                    return ( FALSE );
                }
             }
        }
    }

    //----------------------------------------------------------------------
    // Does the global sight entity match the enemy sight group?
    // Always check for an empty sighttarget first because the
    // the global sight entity could have an empty targetname
    //----------------------------------------------------------------------
    if (self.sighttarget != "") {
        if (sight_entity.targetname == self.sighttarget) {
            // Make sure the sight target is alive before chasing it
            if (sight_entity.health < 0) self.sighttarget = "";
            else {
                // If using enemytarget system, switch off tracking entity first
                if (self.enemy.classtype == CT_ENEMYTARGET) SUB_switchEnemyTarget();
                self.enemy = sight_entity;    // Switch to new enemy
                FoundTarget ();                // Run+turn towards new enemy
                return TRUE;
            }
        }
    }
    //----------------------------------------------------------------------
    // if the first spawnflag bit is set, the monster will only wake up on
    // really seeing the player, not another monster getting angry
    // Zombies have their ambush/crucified spawnflag mix up fixed
    //----------------------------------------------------------------------
    if (sight_entity_time >= time - 0.1 && !(self.spawnflags & MON_AMBUSH) && !self.charmed ) {
        client = sight_entity;        // Global variable
        if (client.enemy == self.enemy) return TRUE;
    } else {
        if ( self.charmed ) {
            selected = world;
            dist = CHARMED_RADIUS;
            head = findradius (self.origin,CHARMED_RADIUS);
        while (head) {
            if ( (!(head.flags & FL_NOTARGET) && (head.flags & FL_MONSTER)) ) {
                if ( ((visible (head) && (visible_distance < dist)) && (head.health > FALSE)) ) {
                    if ( (((head != self) && (head != self.charmer)) && (head.charmer != self.charmer)) ) {
                         selected = head;
                        dist = visible_distance;
                    }
                }
            }
            head = head.chain;
        }
        if ((selected == world)) return ( FALSE );
             client = selected;
        } else {
            client = checkclient ();        // Find a client in current PVS
            if ( !client ) return ( FALSE );    // If no client found, return
        }
    }

    //----------------------------------------------------------------------
    // Checkclient is *supposed* to find ONLY players in the current PVS
    // but it seems the code does something slightly different
    // Checkclient will return other 'objects' (monsters) who are mad
    // at the player, which is suppose to be caught with sight_enemy
    // Find the player entity (client,.enemy,world) for specific checks
    //----------------------------------------------------------------------
    if (client.flags & FL_CLIENT) playtarg = client;
    else {
        if (client.enemy) playtarg = client;
        else playtarg = find(world,targetname,"player");
    }

    //----------------------------------------------------------------------
    // If current enemy = client (player), already got *that* target
    //----------------------------------------------------------------------
    if (client == self.enemy) return FALSE;

    //----------------------------------------------------------------------
    // Check for debug mode, invis artifact, menu system active
    //----------------------------------------------------------------------
    if (playtarg.flags & FL_NOTARGET && !self.charmed) return FALSE;
    if (playtarg.items & IT_INVISIBILITY) return FALSE;
    if (intermission_running) return FALSE;

    //----------------------------------------------------------------------
    // Is the client/player >1000 = FAR do nothing, too far away
    //----------------------------------------------------------------------
    enemy_range = range (client);
    if (enemy_range == RANGE_FAR) return FALSE;
        
    //----------------------------------------------------------------------
    // Do not wake up unless the client (player/monster) is visible
    //----------------------------------------------------------------------
    enemy_vis = visible(client);
    if (!enemy_vis) return FALSE;

    if (!self.charmed) {
        //----------------------------------------------------------------------
        // (Distance >128 && < 500) Is client infront+side and hostile?
        // show_hostile flag is set when the player fires a weapon
        //----------------------------------------------------------------------
        if (enemy_range == RANGE_NEAR) {
            if (client.show_hostile < time && !infront (client))
                return FALSE;
        }
        //----------------------------------------------------------------------
        // (Distance < 1000) Is client infront?
        //----------------------------------------------------------------------
        else if (enemy_range == RANGE_MID) {
                if (!infront (client)) return FALSE;
        }
    }

    // At this point if the enemy is <120 (melee range) the enemy
    // will always wake up and turn around towards the player

    //----------------------------------------------------------------------
    // Finally got a target, check if its a player
    // This is an odd sitation because at this point client *should* be
    // the player/client and this code tries to fix the problem by
    // searching through enemy of enemy chains
    //----------------------------------------------------------------------
    self.enemy = client;

    // Don't hurt other player charmed pets!
    if ( !(client.flags & FL_CLIENT) && !self.charmed && !self.enemy.charmed ) {
        // Is the enemy of the enemy a player?
        self.enemy = self.enemy.enemy;
        if ( !(client.enemy.flags & FL_CLIENT) ) {
            self.enemy = world;
            return FALSE;
        }
    }            
    FoundTarget ();            // Run+turn towards new enemy
    return TRUE;            // stop previous routine got a target

};


//======================================================================
// ai_face (generic)
//----------------------------------------------------------------------
void() ai_face =
{
    if (self.health < 1) return;// Unusual check, caught elsewhere
    self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
    ChangeYaw ();                 // Done in code
};

//----------------------------------------------------------------------
// ai_forward
//----------------------------------------------------------------------
void(float dist) ai_forward =
{
    walkmove (self.angles_y, dist);
};

//----------------------------------------------------------------------
// ai_forward + ai_face
//----------------------------------------------------------------------
void(float dist) ai_faceforward =
{
    ai_face();
    walkmove (self.angles_y, dist);
};

//----------------------------------------------------------------------
// ai_back + backface
//----------------------------------------------------------------------
void(float dist) ai_back =
{
    walkmove ( (self.angles_y+180), dist);
};

void(float dist) ai_backface =
{
    ai_face();
    ai_back(dist);
};

//----------------------------------------------------------------------
// ai_pain (does nothing, just uses ai_back instead)
//----------------------------------------------------------------------
void(float dist) ai_pain =
{
    ai_back (dist);
};

//----------------------------------------------------------------------
// ai_painforward
//----------------------------------------------------------------------
void(float dist) ai_painforward =
{
    walkmove (self.angles_y, dist);
    // Not sure why Id used ideal_yaw when other functions use angles_y
//    walkmove (self.ideal_yaw, dist);
};

//----------------------------------------------------------------------
// ai_turn (Very costly function, re-using FindTarget again)
//----------------------------------------------------------------------
void() ai_turn =
{
    if (FindTarget ()) return;
    ChangeYaw ();            // Code function
};


//----------------------------------------------------------------------
// FacingIdeal
//----------------------------------------------------------------------
float() FacingIdeal =
{
    local    float    delta;
    
    delta = anglemod(self.angles_y - self.ideal_yaw);
    if (delta > 45 && delta < 315) return FALSE;
    else return TRUE;
};

//----------------------------------------------------------------------
// ai_trackenemy
//----------------------------------------------------------------------
void() ai_trackenemy =
{
    if (visible(self.enemy)) {
        self.attack_track = self.enemy.origin;
        ai_face();
    }
};

//----------------------------------------------------------------------
// ai_resetangles
//----------------------------------------------------------------------
void() ai_resetangles =
{
    // There is a chance when finished a monster jump that some
    // of the angles (X/Z) values are wrong and need resetting
    // Should only be used after jump functions
    self.angles_x = self.angles_z = 0;
};

//----------------------------------------------------------------------
// ai_charge (generic)
//  - The monster is in a melee attack, 
//   so get as close as possible to .enemy
//----------------------------------------------------------------------
void(float dist) ai_charge =
{
    ai_face ();
    movetogoal (dist);
};

//----------------------------------------------------------------------
// ai_chargenoturn (generic)
//----------------------------------------------------------------------
void(float dist) ai_chargenoturn =
{
    movetogoal (dist);
};

//----------------------------------------------------------------------
// ai_charge_side (generic)
//----------------------------------------------------------------------
void() ai_charge_side =
{
    local    vector    dtemp;
    local    float    heading;
    
    // aim to the left of the enemy for a flyby
    self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
    ChangeYaw ();

    makevectors (self.angles);
    dtemp = self.enemy.origin - (30 * v_right);
    heading = vectoyaw(dtemp - self.origin);
    
    walkmove(heading, 20);
};

//----------------------------------------------------------------------
// ai_charge_front (generic)
//----------------------------------------------------------------------
void() ai_charge_front =
{
    local    float    heading;
    
    ai_face();            // Turn towards enemy first

    makevectors (self.angles);
    heading = vectoyaw(self.enemy.origin - self.origin);
    
    walkmove(heading, 10);
};

/*======================================================================
 ai_run_melee
 - Turn and close until within an angle to launch a melee attack
======================================================================*/
void() ai_run_melee =
{
    self.ideal_yaw = enemy_yaw;        // This is defined in ai_run
    ChangeYaw ();                    // Code function

    //----------------------------------------------------------------------
    // Facing towards the ENEMY target? (ideal_yaw)
    //----------------------------------------------------------------------
    if (FacingIdeal()) {
        self.th_melee ();
        self.attack_state = AS_STRAIGHT;
    }
};

/*======================================================================
 ai_run_missile
 - Turn in place until within an angle to launch a missile attack
======================================================================*/
void() ai_run_missile =
{
    self.ideal_yaw = enemy_yaw;        // This is defined in ai_run
    ChangeYaw ();                    // Code function
    
    //----------------------------------------------------------------------
    // Facing towards the ENEMY target? (ideal_yaw)
    //----------------------------------------------------------------------
    if (FacingIdeal()) {
        self.th_missile ();
        self.attack_state = AS_STRAIGHT;
    }
};

/*======================================================================
 ai_run_jump
 - Turn in place until within an angle to jump attack
======================================================================*/
void() ai_run_jump =
{
    self.ideal_yaw = enemy_yaw;        // This is defined in ai_run
    ChangeYaw ();                    // Code function
    
    //----------------------------------------------------------------------
    // Facing towards the ENEMY target? (ideal_yaw)
    //----------------------------------------------------------------------
    if (FacingIdeal()) {
        self.th_jump ();
        self.attack_state = AS_STRAIGHT;
    }
};

/*======================================================================
 ai_run_slide
 - Strafe sideways, but stay at aproximately the same range
======================================================================*/
void(float dist) ai_run_slide =
{
    local float    ofs;

    // It is better to re-calcuate this than wait for ai_run to update
    self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);    
    // self.ideal_yaw = enemy_yaw;        // This is defined in ai_run
    ChangeYaw ();                        // Code function
    if (self.lefty > 0) ofs = 90;
    else ofs = -90;
    
    // Tests if monsters can strafe or not by moving the monster
    if (walkmove (self.ideal_yaw + ofs, dist)) return;
    
    // Switch strafe sides for later use    
    self.lefty = rint(1 - self.lefty);
    if (walkmove (self.ideal_yaw - ofs, dist)) return;
    
    // Try moving backwards if both sides are blocked
    walkmove (self.ideal_yaw - 180, dist);
};

/*======================================================================
 ai_run_sidestep
 - Turn 90 degrees and move to the side
======================================================================*/
void(float dist) ai_run_sidestep =
{
    local float    ofs;

    if (self.attack_sidestep < time) {
        self.attack_sidestep = time + 4 + random()*4;
        self.lefty = rint(1 - self.lefty);
    }
    if (self.lefty > 0) ofs = 85;
    else ofs = -85;

    // Work out angle to face for enemy and then turn +/- 90 degrees
    // so that the monster is moving sideways to the enemy
    self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin) + ofs;    
    ChangeYaw ();
    
    // Tests if monsters can move sideways
    if (walkmove (self.ideal_yaw, dist)) return;
    // Switch strafe sides for later use
    self.attack_sidestep = -1;
};

/*======================================================================
 ai_run_backwards
 - Turn 180 degrees and move backwards
======================================================================*/
void(float dist) ai_run_backward =
{
    // Work out angle to face for enemy and then turn +/- 90 degrees
    // so that the monster is moving sideways to the enemy
    self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin) + 180;    
    ChangeYaw ();
    
    // Tests if monsters can move backwards
    if (walkmove (self.ideal_yaw, dist)) return;
    // Cannot walk backwards, move sideways instead
    self.attack_state = AS_SIDESTEP;
};

/*======================================================================
 ai_foundbreakable
  Check if the monster is allowed to break the breakable!?!
======================================================================*/
float(entity source, entity targ, float ignorenoshoot) ai_foundbreakable =
{
    // Found a breakable in the way?
    if (targ.classtype == CT_FUNCBREAK) {
        // Check if a monster and nomonster damage/trigger allowed?
        if (source.flags & FL_MONSTER && targ.spawnflags & BREAK_NOMONSTER)
            return FALSE;
        // Ignoring noshoot spawnflag? (designed for jump/missile events)
        else if (ignorenoshoot == TRUE) return TRUE;
        // Can the breakable be damaged?
        else if (targ.spawnflags & BREAK_NOSHOOT) return FALSE;
        // Its a breakable that breaks!
        else return TRUE;
    }
    return FALSE;
};

/*======================================================================
 ai_immunedamage
  Check if a breakable/pushable is immune to damage from monsters
======================================================================*/
float(entity source, entity targ) ai_immunedamage =
{
    // Found a breakable?
    if (targ.classtype == CT_FUNCBREAK) {
        // Check no damage spawnflag?
        if (source.flags & FL_MONSTER && targ.spawnflags & BREAK_NOMONSTER)
            return TRUE;
    }
    // Found a pushable?
    else if (targ.classtype == CT_FUNCPUSHABLE) {
        // Check no damage spawnflag?
        if (source.flags & FL_MONSTER && targ.spawnflags & PUSH_NOMONSTER)
            return TRUE;
    }
    return FALSE;
};

/*======================================================================
 ai_damagebreakable
  Check if the monster (self) can hit any breakables infront (target)
  Triple trace infront (self.angles) up,middle,down
  If any breakables found, damage them to see if they will break
======================================================================*/
void(float brkdmg) ai_damagebreakable =
{
    local vector    spot1, spot2;
    local float ldmg;
    
    // Setup damage, forward facing vector and initial trace origin
    ldmg = (random() + random() + random()) * brkdmg;
    makevectors(self.angles);
    spot1 = self.origin;
    
    // Trace directly infront of entity using angles
    spot2 = spot1 + v_forward * 100;
    traceline (spot1, spot2, TRUE, self);
    if (ai_foundbreakable(self, trace_ent, FALSE)) {
        // Check for any monster damage modifier on breakable
        if (trace_ent.brkmondmg>0) ldmg = ldmg * trace_ent.brkmondmg;
        T_Damage (trace_ent, self, self, ldmg, DAMARMOR);
    }
    else {
        // Trace upwards
        spot2 = spot1 + v_forward * 100 + v_up * 64;
        traceline (spot1, spot2, TRUE, self);
        if (ai_foundbreakable(self, trace_ent, FALSE)) {
            // Check for any monster damage modifier on breakable
            if (trace_ent.brkmondmg>0) ldmg = ldmg * trace_ent.brkmondmg;
            T_Damage (trace_ent, self, self, ldmg, DAMARMOR);
        }
        else {
            // Trace downwards
            spot2 = spot1 + v_forward * 100 - v_up * 64;
            traceline (spot1, spot2, TRUE, self);
            if (ai_foundbreakable(self, trace_ent, FALSE)) {
                // Check for any monster damage modifier on breakable
                if (trace_ent.brkmondmg>0) ldmg = ldmg * trace_ent.brkmondmg;
                T_Damage (trace_ent, self, self, ldmg, DAMARMOR);
            }
        }
    }
};

/*======================================================================
 ai_jumpbreakable
  Check if the monster can trigger a breakable from jump attack
======================================================================*/
void(float brkdmg) ai_jumpbreakable =
{
    if (ai_foundbreakable(self, other,TRUE) && other.brktrigjump != 0) {
        // Found a breakable which is prone to jump damage
        trigger_ent(other, self);
    }
    else ai_damagebreakable(brkdmg);    // Damage any breakables
};

/*======================================================================
 ai_checkmelee
  Check if the monster (self) can attack enemy (target)
  and returns TRUE if the monster is within XYZ range
======================================================================*/
float(float attackdist) ai_checkmelee =
{
    local vector    spot1, spot2;
    local float     delta, zdiff;

    // Calculate distance and z axis difference seperate
    spot1 = SUB_orgEnemyTarget();
    spot2 = self.origin;
    zdiff = fabs(spot1_z - spot2_z);
    spot1_z = spot2_z = 0;                // Flatten Z axis before vector length
    delta = vlen(spot1 - spot2);        // Calculate vector distance

    // Is the enemy too far away and the zaxis is wrong (too low/high)
    if (delta < attackdist && zdiff < MONAI_MELEEZAXIS) return TRUE;
    else return FALSE;
};

/*======================================================================
 ai_melee (generic)
 - slashing type of damage facing forward
   really assuming the monster is stationary while attacking
======================================================================*/
void() ai_melee =
{
    local float ldmg;

    if (!self.enemy) return;            // removed before stroke
    ai_damagebreakable(10);                // Damage any breakables
    if (!ai_checkmelee(MONAI_MELEEFRONT)) return;    // Too far away

    // Can the target bleed? - no blood/damage, quick exit
    // This candamage test not in origina id code (sync'd to melee_side)
    if (!CanDamage (self.enemy, self)) return;

    ldmg = (random() + random() + random()) * 3;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);

    // Only spawn blood/gore/sound every second
    if (self.meleetimer < time) {
        self.meleetimer = time + 1;
        SpawnMeatSpray (self, self.enemy, random() * 100);
    }

    // Check for poisonous blades!
    if (self.poisonous) PoisonDeBuff(self.enemy);
    
    // Some melee weapons have swing and hit as separate sounds
    // This is trigger_once per melee combat swing
    // No check if meleehitsound is defined or not
    if (self.meleecontact) {
        self.meleecontact = FALSE;
        sound (self, CHAN_WEAPON, self.meleehitsound, 1, ATTN_NORM);
    }
};

/*======================================================================
 ai_meleesmash (generic)
 - large smashing damage overhead attack, usually a single strike
   really assuming the monster is stationary while attacking
======================================================================*/
void(float dmg_multiplier) ai_meleesmash =
{
    local float ldmg;

    if (!self.enemy) return;                        // removed before stroke
    ai_damagebreakable(3*dmg_multiplier);            // Damage any breakables
    if (!ai_checkmelee(self.meleerange)) return;    // Too far away

    // Can the target bleed? - no blood/damage, quick exit
    // This candamage test not in origina id code (sync'd to melee_side)
    if (!CanDamage (self.enemy, self)) return;

    // This function is designed for monsters attacking players or infighting
    // If this is infighting do more damage as it will look more impressive
    // Also if this damage is enough to kill in a single blow, gib for effect
    if (self.enemy.flags & FL_MONSTER) dmg_multiplier = dmg_multiplier * 2;
    ldmg = (random() + random() + random()) * dmg_multiplier;
    if (self.enemy.health < ldmg) ldmg = ldmg*3;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);
    // Lots of blood and gore
    SpawnMeatSpray (self, self.enemy, crandom() * 100);
    SpawnMeatSpray (self, self.enemy, crandom() * 100);
    
    // Check for poisonous blades!
    if (self.poisonous) PoisonDeBuff(self.enemy);

    // Some melee weapons have swing and hit as separate sounds
    // This is trigger_once per melee combat swing
    if (self.meleecontact) {
        self.meleecontact = FALSE;
        if (self.meleehitsound) sound (self, CHAN_WEAPON, self.meleehitsound, 1, ATTN_NORM);
    }
};

/*======================================================================
 ai_melee_side (generic)
 - move forward/side and attack (can cause animation sliding errors)
   this is really designed for glancing blows and monsters which are
   moving fast like they are charging at the player
======================================================================*/
void() ai_melee_side =
{
    local float ldmg;

    if (!self.enemy) return;    // removed before stroke
    ai_damagebreakable(10);        // Damage any breakables
    ai_charge_side();                    // move (20 units) to the side of enemy
    if (!ai_checkmelee(MONAI_MELEESIDE)) return;    // Too far away

    // Can the target bleed? - no blood/damage, quick exit
    if (!CanDamage (self.enemy, self)) return;

    ldmg = (random() + random() + random()) * 3;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);
    SpawnMeatSpray (self, self.enemy, random() * 50);

    // Check for poisonous blades!
    if (self.poisonous) PoisonDeBuff(self.enemy);

    // Some melee weapons have swing and hit as separate sounds
    // This is trigger_once per melee combat swing
    // No check if meleehitsound is defined or not
    if (self.meleecontact) {
        self.meleecontact = FALSE;
        sound (self, CHAN_WEAPON, self.meleehitsound, 1, ATTN_NORM);
    }
};

/*======================================================================
 ai_shockwave
 - produce a large ground slam/shockwave effect
 - Used by hammer ogres and golems
======================================================================*/
void() ai_shockwave_think =
{
    self.wait = rint((time - self.ltime)*10);
    if (self.wait > 6) SUB_Remove();
    else {
        self.frame = self.wait;
        if (self.wait > 4) {
            if (!self.delay) {self.delay = time;}
            self.alpha = 1-((time - self.delay)*5);
        }
        self.angles_y = anglemod(self.angles_y + rint(random()*20));
    self.nextthink = time + TIME_MINTICK;
    }
};

//----------------------------------------------------------------------
void(vector imp_vec, float imp_damage, float imp_radius, float imp_forward, float imp_up) ai_shockwave =
{
    local entity swave, sjump;
    local vector impact, fvel;
    local float vdist, vpercent, vpart;
    local float impzdiff;
    
    self.effects = self.effects | EF_MUZZLEFLASH;
    // Check if the player is close enough for damage?
    self.meleecontact = TRUE;
    ai_meleesmash(imp_damage);
    self.meleecontact = FALSE;
    
    // Play impact sound and work out where the impact is going to happen
    sound (self, CHAN_WEAPON, self.meleehitsound, 1, ATTN_IDLE);
    makevectors (self.angles);
//    impact = self.origin + v_forward*imp_vec_x + v_right*imp_vec_y + v_up*imp_vec_z;
    impact = self.origin + attack_vector(imp_vec);

    // Push all entities within a certain radius outwards
    // Do this find search before spawning ring and particles
    // findradius will find ALL entities regardless of type
    sjump = findradius(impact, imp_radius);
    while(sjump) {
        // Ignore monster (self)
        if (sjump != self) {
            // Only affect players and other monsters
            if (sjump.flags & FL_CLIENT || sjump.flags & FL_MONSTER) {
                // Exclude statues, stone/heavy monsters and bosses!
                if (sjump.classgroup != CG_STONE && sjump.bossflag == FALSE) {
                    // Is the entity on the ground?
                    if (sjump.flags & FL_ONGROUND) {
                        // Is the entity too far above or below the impact?
                        impzdiff = fabs(sjump.origin_z - impact_z);
                        if (impzdiff < MONAI_IMPACTZAXIS) {
                            // Distance between impact and enemy
                            vdist = vlen(sjump.origin - impact);
                            // Percentage of impact force
                            vpercent = 1 - (vdist / imp_radius);
                            // Monsters have less impact
                            if (sjump.flags & FL_MONSTER) vpercent = vpercent * 0.75;
                            // Ogre facing angle (easier to understand)
                            makevectors(self.angles);                                    
                            // Combine forward/up force with existing velocity
                            fvel = (v_forward * (imp_forward * vpercent)) + (v_up * (imp_up * vpercent));
                            sjump.velocity = sjump.velocity + fvel;
                            sjump.flags = sjump.flags - (sjump.flags & FL_ONGROUND);
                        }
                    }
                }
            }
        }
        sjump = sjump.chain;
    }

    // Spawn impact model ring on the ground
    swave = spawn();
    swave.mdl = MODEL_PROJ_RINGSHOCK;    
    setmodel(swave,swave.mdl);
    setsize(swave, VEC_ORIGIN, VEC_ORIGIN);
    setorigin(swave, impact);        // on floor, slight up (2 pixels)
    swave.solid = SOLID_NOT;        // No interaction with world
    swave.movetype = MOVETYPE_NONE;    // Static item, no movement
    swave.think = ai_shockwave_think;
    swave.nextthink = time + TIME_MINTICK;    // High time loop
    swave.ltime = swave.nextthink;
    
    // Spawn particle explosion where impact is located
    vpart = 64 + rint(random()*64);
    particle_explode(impact, vpart, 0.5, PARTICLE_BURST_FIRE, PARTICLE_BURST_SHOCKWAVE); 
};

// From Drake
float(float yaw, float dist) flat_move = {
    local   entity  swap;

    swap = self.enemy;
    self.enemy = world;     // This prevents 'self' from going up/down.
    dist = walkmove (yaw, dist);    // After this, dist = TRUE or FALSE.
    self.enemy = swap;
    return dist;
};

// From Drake
// This is 'movetogoal' without the vertical movement.
void(float dist) flat_goal = {
    local   entity  swap;

    swap = self.enemy;
    self.enemy = world;     // This prevents 'self' from going up/down.
    movetogoal (dist);
    self.enemy = swap;
};