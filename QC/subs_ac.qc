/*======================================================================
 SUB FUNCTIONS
======================================================================*/
void() SUB_Null = {};
void() SUB_Remove = {remove(self);};
void(entity inflictor, entity attacker, float damage) SUB_Null_pain = {};

void() SUB_CalcMoveDone;
void() SUB_CalcAngleMoveDone;
void(vector marker_org, float marker_type) spawn_marker;
void(float hud_item) update_hud_totals;

//======================================================================
// To determine if an entity is still active this function generates
// an unique number that can be checked/shared between linked entities
//----------------------------------------------------------------------
void(entity targ) gen_unique_no =
{
    // Check if target entity has an unique number already
    if (targ.entno_unique == 0)
        targ.entno_unique = rint(random()*1000) * rint(random()*1000);
};

//======================================================================
// Most of the monsters have an attack_offset setup to spawn their
// proejctiles in the right location relative to their gun model
// This function expects the makevectors has been done already
// makevectors (self.angles); <--- if missing, vector will be rubbish!
//----------------------------------------------------------------------
vector(vector attack_dir) attack_vector =
{
    local vector retvec;
    retvec = v_forward * attack_dir_x;
    retvec = retvec + (v_right * attack_dir_y);
    retvec = retvec + (v_up * attack_dir_z);
    return retvec;
};

//======================================================================
// Bmodels have a 0,0,0 origin key, need to calculate from mins/maxs
// This function will return the true origin using mins/max vectors
//----------------------------------------------------------------------
vector(entity bmodel_ent) bmodel_origin =
{
    local vector vdiff, vorigin;

    // Calculate the middle point of the mins/maxs size vectors
    vdiff_x = (bmodel_ent.maxs_x - bmodel_ent.mins_x) / 2;
    vdiff_y = (bmodel_ent.maxs_y - bmodel_ent.mins_y) / 2;
    vdiff_z = (bmodel_ent.maxs_z - bmodel_ent.mins_z) / 2;

    // The mins vector is an absolute value of where the Bmodel is located
    // Add mins vector to middle point to get true origin
    // When bmodel do move they also update their origin co-ordinates
    // So any additional movement needs to be added as well
    // This means that bmodels can be tracked by movement functions
    vorigin = bmodel_ent.origin + bmodel_ent.mins + vdiff;

    // Return origin
    return vorigin;
};

//======================================================================
// Check for bmodel errors, model key empty (can crash engine)
//----------------------------------------------------------------------
float() check_bmodel_keys =
{
    if (self.model == "") {
        // this error can be fatal for most engines, show warning
        dprint ("\b[BMODEL]\b "); dprint (self.classname);
        dprint (" ("); dprint (self.targetname);
        dprint (") missing MODEL key!\n");

        self.classtype = CT_DEVMARKER;        // Useless entity
        self.movetype = MOVETYPE_NONE;        // Stationary
        self.solid = SOLID_NOT;                // no world interaction

        // If developer active show yellow diamond
        if (developer > 0) setmodel(self, MODEL_BROKEN);
        else setmodel(self,"");

        // Make sure no world interaction
        setsize (self, VEC_ORIGIN, VEC_ORIGIN);

        // Randomly place debug diamond above 0,0,0
        self.pos1 = '0 0 0';
        self.pos1_z = random()*64;
        setorigin(self, self.pos1);

        // Do nothing with this entity
        return TRUE;
    }
    else return FALSE;
};

//----------------------------------------------------------------------
// Cannot have multiple upgrade restrictions on monster/items
//----------------------------------------------------------------------
void() remove_duplicate_upgrades =
{
    if (self.upgrade_axe) {
        if (self.upgrade_ssg) self.upgrade_ssg = FALSE;
        else if (self.upgrade_lg) self.upgrade_lg = FALSE;
    }
    else if (self.upgrade_ssg) {
        if (self.upgrade_axe) self.upgrade_axe = FALSE;
        else if (self.upgrade_lg) self.upgrade_lg = FALSE;
    }
    else if (self.upgrade_lg) {
        if (self.upgrade_axe) self.upgrade_axe = FALSE;
        else if (self.upgrade_ssg) self.upgrade_ssg = FALSE;
    }
}

/*======================================================================
QuakeEd only writes a single float for angles (bad idea),
so up and down are just constant angles.
======================================================================*/
void() SetMovedir =
{
    if (self.angles == '0 -1 0') self.movedir = '0 0 1';
    else if (self.angles == '0 -2 0') self.movedir = '0 0 -1';
    else {
        makevectors (self.angles);
        self.movedir = v_forward;
    }
    // Bmodels need angles key reset otherwise twisted on spawn
    self.angles = '0 0 0';
};

//----------------------------------------------------------------------
float(vector check_vec) CheckZeroVector =
{
    if (check_vec_x == 0 && check_vec_y == 0 && check_vec_z == 0)
        return TRUE;
    else return FALSE;
};

//----------------------------------------------------------------------
// Some entities have directional parameters (angles -> movedir)
// This function searches for a target entity (useful for tracking)
// Always finds the first target key (will fail if multiple targets)
//----------------------------------------------------------------------
void() TargetMovedir =
{
    local entity targ;
    local vector org, targorg;

    if (self.target == "") return;

    // Check self for Bmodel origin
    if (self.bsporigin) org = bmodel_origin(self);
    else org = self.origin;

    // Find target entity
    targ = find(world, targetname, self.target);
    if (targ) {
        // Check for a Bmodel object (special origin)
        if (targ.bsporigin) targorg = bmodel_origin(targ);
        else targorg = targ.origin;
        // Calculate facing angle towards target
        self.movedir = normalize(targorg - org);
    }
};

/*======================================================================
InitTrigger
======================================================================*/
void() InitTrigger =
{
    // trigger angles are used for one-way touches.  An angle of 0 is assumed
    // to mean no restrictions, so use a yaw of 360 instead.
    if (CheckZeroVector(self.angles) == FALSE) SetMovedir ();
    self.solid = SOLID_TRIGGER;
    self.movetype = MOVETYPE_NONE;
    self.mdl = self.model;        // Save model for later
    setmodel (self, self.mdl);    // set size and link into world
    self.bbmins = self.mins;    // Save bmodel bounds for later
    self.bbmaxs = self.maxs;
    self.bsporigin = TRUE;        // bmodel origin 0,0,0
    self.modelindex = 0;
    self.model = "";            // hide bmodel surface
};

/*======================================================================
SUB_CalcMove

calculate self.velocity and self.nextthink to reach dest from
self.origin traveling at speed
======================================================================*/
void(vector tdest, float tspeed, void() func) SUB_CalcMove =
{
    local vector    vdestdelta;
    local float        len, traveltime;

    if (!tspeed) tspeed = 40;            // Create default speed if none specified
    self.think1 = func;                    // Finished moving function
    self.finaldest = tdest;                // Final destination
    self.think = SUB_CalcMoveDone;        // Tidy up function

    vdestdelta = tdest - self.origin;    // set destdelta to the vector needed to move
    len = vlen (vdestdelta);            // calculate length of vector
    traveltime = len / tspeed;            // divide by speed to get time to reach dest

    // If move distance or time too small, snap to final location
    if (traveltime < 0.1 || tdest == self.origin) {
        self.velocity = '0 0 0';
        self.nextthink = self.ltime + 0.1;
        return;
    }

    // scale the destdelta vector by the time spent traveling to get velocity
    self.velocity = vdestdelta * (1/traveltime);    // qcc won't take vec/float
    self.nextthink = self.ltime + traveltime;
};

/*======================================================================
After moving, set origin to exact final destination
======================================================================*/
void()  SUB_CalcMoveDone =
{
    self.velocity = '0 0 0';
    setorigin(self, self.finaldest);
    self.nextthink = -1;
    if (self.think1) self.think1();
};

//----------------------------------------------------------------------
// Not used anywhere
//----------------------------------------------------------------------
void(entity ent, vector tdest, float tspeed, void() func) SUB_CalcMoveEnt =
{
    local entity    stemp;
    stemp = self;
    self = ent;

    SUB_CalcMove (tdest, tspeed, func);
    self = stemp;
};

/*======================================================================
SUB_CalcMoveEntity

calculate self.velocity and self.nextthink to reach dest from
self.origin traveling at speed
======================================================================*/
void() SUB_MoveEntityDone =
{
    self.movetype = self.classmove;
    SUB_CalcMoveDone();
};

//----------------------------------------------------------------------
void(vector tdest, float tspeed, void() func) SUB_CalcMoveEntity =
{
    local vector    vdestdelta;
    local float        traveltime;

    if (!tspeed) tspeed = 4;            // Create default travel time if none specified
    self.classmove = self.movetype;        // save previous movetype
    self.movetype = MOVETYPE_FLY;        // Allow entity to move smoothly
    self.think1 = func;                    // Store final function
    self.finaldest = tdest;                // Store final destination

    vdestdelta = tdest - self.origin;    // set destdelta to the vector needed to move
    traveltime = 1 / tspeed;                    // Work out percentage travel velocity
    self.velocity = vdestdelta * traveltime;    // Calc time travel fraction

    self.nextthink = time + tspeed;
    self.think = SUB_MoveEntityDone;
};

/*======================================================================
SUB_CalcAngleMove

calculate self.avelocity and self.nextthink to reach destangle from
self.angles rotating

The calling function should make sure self.think is valid
======================================================================*/
void(vector destangle, float tspeed, void() func) SUB_CalcAngleMove =
{
local vector    destdelta;
local float        len, traveltime;

    if (!tspeed)  dprint("[SUB_CALC_ANGLE] No speed is defined!");

    // set destdelta to the vector needed to move
    destdelta = destangle - self.angles;

    // calculate length of vector
    len = vlen (destdelta);

    // divide by speed to get time to reach dest
    traveltime = len / tspeed;

    // set nextthink to trigger a think when dest is reached
    self.nextthink = self.ltime + traveltime;

    // scale the destdelta vector by the time spent traveling to get velocity
    self.avelocity = destdelta * (1 / traveltime);

    self.think1 = func;
    self.finalangle = destangle;
    self.think = SUB_CalcAngleMoveDone;
};

/*======================================================================
After rotating, set angle to exact final angle
======================================================================*/
void() SUB_CalcAngleMoveDone =
{
    self.angles = self.finalangle;
    self.avelocity = '0 0 0';
    self.nextthink = -1;
    if (self.think1)
        self.think1();
};

//----------------------------------------------------------------------
// Not used anywhere
//----------------------------------------------------------------------
void(entity ent, vector destangle, float tspeed, void() func) SUB_CalcAngleMoveEnt =
{
    local entity        stemp;
    stemp = self;
    self = ent;
    SUB_CalcAngleMove (destangle, tspeed, func);
    self = stemp;
};

//----------------------------------------------------------------------
void() DelayThink =
{
    activator = self.enemy;
    SUB_UseTargets ();
    remove(self);
};

/*======================================================================
SUB_UseTargets

the global "activator" should be set to the entity that initiated the firing.

If self.delay is set, a DelayedUse entity will be created that will actually
do the SUB_UseTargets after that many seconds have passed.

Centerprints any self.message to the activator.

Removes all entities with a targetname that match self.killtarget,
and removes them, so some events can remove other triggers.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function

======================================================================*/
void() SUB_UseTargets =
{
    local entity t, stemp, otemp, act;

    //----------------------------------------------------------------------
    // Check for recursive loops!?!
    //----------------------------------------------------------------------
    if (self.targetname != "") {
        if (self.targetname == self.target || self.targetname == self.target2) {
            dprint("\n----------------------------------------\n");
            dprint("\b[USE_TARGET]\b target == targetname, really bad!!!\n");
            dprint(self.classname);
            dprint(" ("); dprint(self.targetname); dprint(") at ");
            if (self.bsporigin) dprint(vtos(bmodel_origin(self)));
            else dprint(vtos(self.origin));
            dprint("\ntarg ("); dprint(self.target);
            dprint(") targ2 ("); dprint(self.target2);
            dprint(") kill ("); dprint(self.killtarget);
            dprint(")\n----------------------------------------\n");
            return;
        }
    }
    //----------------------------------------------------------------------
    // check for a delay
    //----------------------------------------------------------------------
    if (self.delay) {
    // create a temp object to fire at a later time
        t = spawn();
        t.classname = "DelayedUse";
        t.classtype = CT_TRIGRELAYDELAY;
        t.owner = self;
        t.nextthink = time + self.delay;
        t.think = DelayThink;
        t.enemy = activator;
        t.message = self.message;
        t.killtarget = self.killtarget;
        t.target = self.target;
        t.target2 = self.target2;
        t.noise = self.noise;
        return;
    }

    //----------------------------------------------------------------------
    // Check if previous delay has been disabled
    // Unfortunately start off = OFF state and not easy to check for
    // because some entities use that spawnflag bit for other things!
    //----------------------------------------------------------------------
    if (self.classtype == CT_TRIGRELAYDELAY) {
        if (self.owner.estate & ESTATE_DISABLE) return;
    }

    //----------------------------------------------------------------------
    // print any messages
    //----------------------------------------------------------------------
    if (activator.flags & FL_CLIENT && self.message != "") {
        if (coop > 0) {
            // Write message to all clients
            msg_entity = self;
            WriteByte (MSG_ALL, SVC_CENTERPRINT);
            WriteString (MSG_ALL, self.message);
        }
        // Write message to single client
        else centerprint (activator, self.message);
        // Default noise = classic beep beep sound
        if (self.noise == "") sound (activator, CHAN_VOICE, SOUND_TALK, 1, ATTN_NORM);
    }

    //----------------------------------------------------------------------
    // kill targets
    // I really do hate this function because there is hardly any checks
    // on entities being removed, added a player check for sanity!
    //----------------------------------------------------------------------
    if (self.killtarget != "") {
        t = find (world, targetname, self.killtarget);
        while(t) {
            // Check for activate mosnters? Tidy up kill counter
            if (t.flags & FL_MONSTER && t.health > 0) {
                // If a monster marked with no monster count dies
                // update HUD totals to reflect death properly
                if (t.nomonstercount) {
                    total_monsters = total_monsters + 1;
                    update_hud_totals(HUD_MONSTERS);
                }
                // Update global monster death totals
                killed_monsters = killed_monsters + 1;
                WriteByte (MSG_ALL, SVC_KILLEDMONSTER);
            }
            // Added extra check to prevent player client removal!?!
            if (!(t.flags & FL_CLIENT)) {
                // Try to minimize the entity chain damage, check for
                // entity attachments and delete them as well!
                if (t.attachment) remove(t.attachment);
                if (t.attachment2) remove(t.attachment2);
                if (t.attachment3) remove(t.attachment3);
                // Make sure any unique ent number is reset
                if (t.entno_unique > 0) t.entno_unique = 0;
                // Remove entity from chain list
                remove (t);
            }
            // Find the next killtarget
            t = find (t, targetname, self.killtarget);
        }
    }
    //----------------------------------------------------------------------
    // fire targets
    //----------------------------------------------------------------------
    if (self.target != "") {
        act = activator;    // ID Code - Never switch around, not sure why it is here?
        t = find (world, targetname, self.target);
        stemp = self; otemp = other;        // Store self/other
        while(t) {
            if (t.use != SUB_Null) {
                if (t.use) {
                    self = t; other = stemp;        // Switch self/other
                    self.activate = stemp;            // used by the breakable system
                    self.use ();                    // fire trigger
                    self = stemp; other = otemp;    // restore self/other
                    activator = act;        // ID code - Never changed, why restore it?
                }
            }
            // Are there anymore targets left in the list?
            t = find (t, targetname, self.target);
        }
    }
    //----------------------------------------------------------------------
    if (self.target2 != "") {
        act = activator;    // ID Code - Never switch around, not sure why it is here?
        t = find (world, targetname, self.target2);
        stemp = self; otemp = other;        // Store self/other
        while(t) {
            if (t.use != SUB_Null) {
                if (t.use) {
                    self = t; other = stemp;// Switch self/other
                    self.activate = stemp;    // used mostly by the breakable system
                    self.use ();            // fire trigger
                    self = stemp; other = otemp;    // restore self/other
                    activator = act;        // ID code - Never changed, why restore it?
                }
            }
            t = find (t, targetname, self.target2);
        }
    }
};

/*======================================================================
 SUB_AttackFinished
    in nightmare mode, all attack_finished times become 0
    some monsters refire twice automatically (using .cnt variable)
    Change nightmare to be less range spam and better time scaling
======================================================================*/
void(float normal) SUB_AttackFinished =
{
    local float nightadj;    // Easier to work out percentage
    self.cnt = 0;            // reset checkfire counter

    // Nightmare skill attacks are faster (turret like) and with
    // hitscan enemies this can easily overwhelm targets locking
    // them into damage/pain function loops. This can especially be
    // be bad if a large enough enemy group is involved!
    // Changed - NM behaviour for boss fights is default
    //
    if (skill == SKILL_NIGHTMARE && self.enemy.bossflag == FALSE) {
        // Nightmare mode is just endless range attacks!
        // I never thought this was a good way to end skill levels
        // Changed nightmare mode to 50-75% decrease in time
        // Exceptions to nightmare mode (minion spawning) - 75-100% time
        nightadj = 0.5 + (random()*0.25);
        if (self.minion_active) nightadj = nightadj + 0.25;
        // The original ID shamblers were lightning turrets on Nightmare
        // Shamblers don't scale with skill levels in AD because
        // they don't have projectiles, so they need a switch instead
        // On Nightmare skill Shamblers go from 2-4s to 1s!
        // Instead of 2-4s, NM is 0.6-1.2s, Average 1.2s +2 frames
        if (self.classtype == CT_MONSHAM) nightadj = 0.4;
        self.attack_finished = time + (normal*nightadj);
    }
    // Easy, Nornal and Hard skill all pass through, no change
    else self.attack_finished = time + normal;
    //dprint("Time ("); dprint(ftos(time));
    //dprint(") AF ("); dprint(ftos(normal));
    //dprint(") Dif ("); dprint(ftos(self.attack_finished - time));
    //dprint(") Att ("); dprint(ftos(self.attack_finished));
    //dprint(")\n");
};

//----------------------------------------------------------------------
// Used by soliders and enforcers (special double fire mode)
//----------------------------------------------------------------------
void (void() thinkst) SUB_CheckRefire =
{
    if (skill != SKILL_NIGHTMARE) return;
    if (self.cnt == 1) return;
    if (!visible (self.enemy)) return;
    self.cnt = 1;
    self.think = thinkst;
};

//----------------------------------------------------------------------
// Not used anywhere in QC
//----------------------------------------------------------------------
/*float() SUB_CountTargets =
{
    local entity t;
    local float i;

    i = 0;
    if (self.target != "") {
        t = world;
        do    {
            t = find(t, targetname, self.target);
            if (!t) break;
            i = i + 1;
        } while ( 1 );
    }
    return i;
};*/

/*======================================================================
 ChooseTurn
 - can't find any reference to this in the QC
======================================================================*/
/*void(vector dest3) ChooseTurn =
{
    local vector    dir, newdir;

    dir = self.origin - dest3;

    newdir_x = trace_plane_normal_y;
    newdir_y = 0 - trace_plane_normal_x;
    newdir_z = 0;

    if (dir * newdir > 0) {
        dir_x = 0 - trace_plane_normal_y;
        dir_y = trace_plane_normal_x;
    }
    else {
        dir_x = trace_plane_normal_y;
        dir_y = 0 - trace_plane_normal_x;
    }

    dir_z = 0;
    self.ideal_yaw = vectoyaw(dir);
};*/

/*======================================================================
 SPAWN_TFOG and SPAWN_TDEATH

 These functions were originally in triggers.qc, but are used all
 over the place by monsters, items and players. Subs seems a better
 home for these generic functions.

 ======================================================================*/
void() play_teleport =
{
    self.lip = random() * 5;
    if (self.lip < 1) self.noise = "misc/r_tele1.wav";
    else if (self.lip < 2) self.noise = "misc/r_tele2.wav";
    else if (self.lip < 3) self.noise = "misc/r_tele3.wav";
    else if (self.lip < 4) self.noise = "misc/r_tele4.wav";
    else self.noise = "misc/r_tele5.wav";

    sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
    self.nextthink = time + 2;    // Allow time for the sound to finish
    self.think = SUB_Remove;    // tidy up function
};

//----------------------------------------------------------------------
void(vector org) spawn_tfog =
{
    local entity spawneffect;

    spawneffect = spawn ();
    spawneffect.origin = org;
    spawneffect.nextthink = time + 0.2;
    spawneffect.think = play_teleport;

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_TELEPORT);
    WriteCoord (MSG_BROADCAST, org_x);
    WriteCoord (MSG_BROADCAST, org_y);
    WriteCoord (MSG_BROADCAST, org_z);
};

//----------------------------------------------------------------------
void() tdeath_touch =
{
    // Cannot telefrag yourself!?!
    if (other == self.owner) return;
    // Check if found a breakable than cannot be broken
    if (ai_foundbreakable(self, other, FALSE)) return;

    // A boss cannot be telefragged (except shub)
    // Telefrag entity trying to telefrag boss instead!?!
    if (other.bossflag == TRUE) {
        if (other.classtype != CT_MONIDSHUB) {
            T_Damage(self.owner, other, other, MEGADEATH, NOARMOR);
            return;
        }
    }

    // frag anyone who teleports in on top of an invincible player
    if (other.flags & FL_CLIENT) {
        if (other.invincible_finished > time) self.classname = "teledeath2";
        // other monsters explode themselves
        if ( !(self.owner.flags & FL_CLIENT) ) {
            T_Damage(self.owner, self, self, MEGADEATH, NOARMOR);
            return;
        }
    }

    // Sudden death incoming ...
    if (other.health) T_Damage(other, self, self, MEGADEATH, NOARMOR);
};

//----------------------------------------------------------------------
void(vector org, entity death_owner) spawn_tdeath =
{
    local entity death;

    death = spawn();
    death.classname = "teledeath";
    death.owner = death_owner;
    death.movetype = MOVETYPE_NONE;
    death.solid = SOLID_TRIGGER;
    death.angles = '0 0 0';
    setsize (death, death_owner.mins - '1 1 1', death_owner.maxs + '1 1 1');
    setorigin (death, org);
    death.touch = tdeath_touch;
    death.nextthink = time + 0.2;
    death.think = SUB_Remove;

    // This is extremely costly on the engine because it re-checks
    // every trigger in the map to see if it is touching something
    force_retouch = 2;
};

/*======================================================================
 skill_chanceattack
  Calculate chance to attack player based on skill level and distance
  The standard ID range formula
    (enemy_range == RANGE_MELEE) chance = 0.9
    (enemy_range == RANGE_NEAR) chance = 0.4
    (enemy_range == RANGE_MID) chance = 0.05
    else chance = 0;
    The problem with this formula is that the chance factor is not gradual
    and will spike in chance when close to range boundaries
    Also this formula has no skill modifiers leading to limited gameplay rules

======================================================================*/
float() SUB_ChanceAttack =
{
    local float chrange, chskill, chance;

    chrange = chskill = 0;                    // taniwha patch

    // Upper and lower limits of range have fixed chance rates
    if (self.enemydist > MON_MAX_RANGE) chance =  0.05;
    else if (self.enemydist < MON_RANGE_MELEE) chance = 0.95;
    else {
        // Random chance of range attack based on player distance
        chrange = ((MON_MAX_RANGE - self.enemydist) / MON_STEPRANGE) / 10;

        // Skill modifier to chance (Easy -0.2 Norm -0.1, Hard 0, Night 0.1)
        chskill = ((skill-SKILL_HARD)*0.1);
        chance = chrange + chskill;

        // There Should always be a chance to HIT or MISS
        if (chance > 0.95) chance = 0.95;
        else if (chance < 0.05) chance = 0.05;
    }

    // Return result and finish debug information line
    if (random() < chance) return TRUE;
    else return FALSE;
};

/*======================================================================
 skill_subattack
  Calculate attack_finished time based on skill level
======================================================================*/
float (float min_time, float ranbase_time) SUB_SkillAttack =
{
    local float ran_time, sub_time;

    // Easy = min + random(base + 1.5)    Normal = min + random(base + 1.0)
    // Hard = min + random(base + 0.5)    Nightm = min + random(base + 0.0)
    ran_time = ranbase_time + ((SKILL_NIGHTMARE - skill) * 0.5);
    sub_time = min_time + (ran_time * random());

    return sub_time;
};

/*======================================================================
 SUB_Tracking
 Progressively tracks the origin of enemies making attacks more accurate
  * Original code by Necros (http://shoresofnis.com/)
======================================================================*/
vector(vector iorg, vector org_offset, entity targ, float zspeed) SUB_Tracking =
{
    local    vector    vec, org;
    local    float    diff;

    // Allow for source origin to be fine tuned (directional)
    makevectors (self.angles);
    org = self.origin + attack_vector(org_offset);

    diff = vlen(iorg - org) / zspeed;        // Workout difference
    vec = targ.velocity;
    if (targ.flags & FL_MONSTER) vec_z = 0;    // Flatten Z axis for monsters
    else vec_z = vec_z * 0.2;                // Aim for headshot with players

    iorg = targ.origin + diff * vec;        // Refine target origin
    return iorg;
};

/*======================================================================
 SUB_Elevation
  Calculates the Elevation for Z Aware monsters
  * Original code by Preach (https://tomeofpreach.wordpress.com/)
  * Added support for variable origin locations and proj speeds
  * Added support for distance scaling linked to skill levels
======================================================================*/
float(float theta, vector sorg, vector mdest, float zspeed) SUB_Elevation =
{
    local float a, b, c;     //constants in the equation to be solved
    local vector ofs;         //displacement we wish the projectile to travel
    local float y, z;         //horizontal and vertical components of ofs
    local float tan_theta;     //trig values of the angle theta
    local vector vec, eorg;
    local float dist, sperc;

    // Place projectile destination at player head level
    if (self.classtype == CT_MONSPIDER) mdest = mdest + ELEV_SPID_OFS;
    else mdest = mdest + ELEV_VIEW_OFS;

    // Work out vector distance between source and destination
    // Normalize the distance and create a percentage of that
    // This is a skill based calculation so the aiming gets better
    // The grenade is aimed infront of target so that it bounces
    // No check for vertical difference so it bad going up height!
    // Zombie, poison spiders zaware calculation is fine, exception!
    if (self.classgroup != CG_ZOMBIE && self.classtype != CT_MONSPIDER
        && self.classtype != CT_MONBOGLORD) {
        vec = mdest - sorg;
        dist = vlen(vec);
        vec = normalize(vec);
        sperc = 0.45 + skill*0.15;
        eorg = sorg + (vec * (dist*sperc));
        ofs = eorg - sorg;
    }
    else ofs = mdest - sorg;

    //calculate how far we are firing
    //    ofs = mdest - sorg;
    z = ofs_z;
    ofs_z = 0;
    y = vlen(ofs);

    // Map gravity is a global variable defined in world.qc
    //find the coefficients of the quadratic in tan(theta)
    a = 0.5 * map_gravity * y * y / (zspeed * zspeed);
    b = -y;
    c = a + z;

    //check if the destination is too far to reach
    if(b*b < 4*a*c) return ELEV_DEFAULT;

    //calculate the tan value of the given theta
    tan_theta = mathlib_tan(theta);

    //reuse ang to create the improved firing direction
    theta = mathlib_atan2(a*tan_theta*tan_theta - c, 2*a*tan_theta + b);

    //constrain the values to stop anything too mad happening
    while(theta > 90) theta = theta - 180;
    return theta;
};

/*======================================================================
 SUB_Elevat (Alternative version, not used anymore, left for reference)
  Calculates the Elevation for Z Aware monsters
  * Original formula WIKI with QC help from ericw
======================================================================*/
/*float(vector source, vector targ, float zspeed) SUB_Elevat =
{
    local float g;                // Gravity
    local float targx, targy;    // Sub parts of distance
    local float v2, v4;            // speed*speed & speed*speed*speed*speed
    local vector dist;            // Distance to travel
    local float ang, theta;

    g = map_gravity;            // default 800
    dist = targ - source;        // vector distance
    targy = dist_z;                // Up direction
    dist_z = 0;
    targx = vlen(dist);            // X,Y direction

    v2 = zspeed * zspeed;
    v4 = v2 * v2;

    // First part of equation
    ang = v4 - (g * ( (g * targx * targx) + (2 * targy * v2) ) );
    // Cap elevation angle at default value
    //if(ang < 0) return ELEV_DEFAULT;

    // Second section of equation
    theta = mathlib_atan2 ((v2 - mathlib_sqrt(ang)) / (g * targx),1);
    return theta;
}; */
