/*==============================================================================
BOIL/BOMB model by Ijed, QC/Gameplay by Sock
==============================================================================*/

$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8
$frame stand9 stand10 stand11 stand12 stand13 stand14 stand15 stand16

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12 walk13 walk14 walk15 walk16

$frame run1 run2 run3 run4 run5 run6 run7 run8

$frame explode1 explode2 explode3 explode4 explode5 explode6
$frame explode7 explode8 explode9

$frame hobA1 hobA2 hobA3 hobA4 hobA5 hobA6 hobA7 hobA8 hobA9 hobA10

$frame hobB1 hobB2 hobB3 hobB4 hobB5 hobB6 hobB7 hobB8 hobB9 hobB10

//=============================================================================
void() boil_stand1 = [ $stand1, boil_stand2 ] {monster_idle_sound(); ai_stand();}
void() boil_stand2 = [ $stand2, boil_stand3 ] {ai_stand();};
void() boil_stand3 = [ $stand3, boil_stand4 ] {ai_stand();};
void() boil_stand4 = [ $stand4, boil_stand5 ] {ai_stand();};
void() boil_stand5 = [ $stand5, boil_stand6 ] {ai_stand();};
void() boil_stand6 = [ $stand6, boil_stand7 ] {ai_stand();};
void() boil_stand7 = [ $stand7, boil_stand8 ] {ai_stand();};
void() boil_stand8 = [ $stand8, boil_stand9 ] {ai_stand();};
void() boil_stand9 = [ $stand9, boil_stand10 ] {ai_stand();};
void() boil_stand10 = [ $stand10, boil_stand11 ] {ai_stand();};
void() boil_stand11 = [ $stand11, boil_stand12 ] {ai_stand();};
void() boil_stand12 = [ $stand12, boil_stand13 ] {ai_stand();};
void() boil_stand13 = [ $stand13, boil_stand14 ] {ai_stand();};
void() boil_stand14 = [ $stand14, boil_stand15 ] {ai_stand();};
void() boil_stand15 = [ $stand15, boil_stand16 ] {ai_stand();};
void() boil_stand16 = [ $stand16, boil_stand1 ] {ai_stand();};

//=============================================================================
void() boil_walk1 = [ $walk1, boil_walk2 ] {
    monster_idle_sound(); ai_walk(0);};
void() boil_walk2 = [ $walk2, boil_walk3 ] {monster_footstep(TRUE);ai_walk(3);};
void() boil_walk3 = [ $walk3, boil_walk4 ] {ai_walk(4);};
void() boil_walk4 = [ $walk4, boil_walk5 ] {ai_walk(3);};
void() boil_walk5 = [ $walk5, boil_walk6 ] {ai_walk(3);};
void() boil_walk6 = [ $walk6, boil_walk7 ] {ai_walk(2);};
void() boil_walk7 = [ $walk7, boil_walk8 ] {ai_walk(1);};
void() boil_walk8 = [ $walk8, boil_walk9 ] {ai_walk(2);};

void() boil_walk9 = [ $walk9, boil_walk10 ] {ai_walk(0);};
void() boil_walk10 = [ $walk10, boil_walk11 ] {monster_footstep(TRUE);ai_walk(3);};
void() boil_walk11 = [ $walk11, boil_walk12 ] {ai_walk(1);};
void() boil_walk12 = [ $walk12, boil_walk13 ] {ai_walk(2);};
void() boil_walk13 = [ $walk13, boil_walk14 ] {ai_walk(3);};
void() boil_walk14 = [ $walk14, boil_walk15 ] {ai_walk(3);};
void() boil_walk15 = [ $walk15, boil_walk16 ] {ai_walk(2);};
void() boil_walk16 = [ $walk16, boil_walk1 ] {ai_walk(2);};

//----------------------------------------------------------------------
// Used for testing run animation speed and sliding feet problems
//----------------------------------------------------------------------
/*void() boil_walk1 = [ $run1, boil_walk2 ] {monster_footstep(TRUE);ai_walk(4);};
void() boil_walk2 = [ $run2, boil_walk3 ] {monster_idle_sound(); ai_walk(16);};
void() boil_walk3 = [ $run3, boil_walk4 ] {ai_walk(8);};
void() boil_walk4 = [ $run4, boil_walk5 ] {ai_walk(4);};
void() boil_walk5 = [ $run5, boil_walk6 ] {monster_footstep(TRUE);ai_walk(4);};
void() boil_walk6 = [ $run6, boil_walk7 ] {ai_walk(16);};
void() boil_walk7 = [ $run7, boil_walk8 ] {ai_walk(8);};
void() boil_walk8 = [ $run8, boil_walk1 ] {ai_walk(4);};*/

//============================================================================
void() boil_run1 = [ $run1, boil_run2 ] {monster_footstep(TRUE);ai_run(4);};
void() boil_run2 = [ $run2, boil_run3 ] {monster_idle_sound(); ai_run(16);};
void() boil_run3 = [ $run3, boil_run4 ] {ai_run(8);};
void() boil_run4 = [ $run4, boil_run5 ] {ai_run(4);};
void() boil_run5 = [ $run5, boil_run6 ] {monster_footstep(TRUE);ai_run(4);};
void() boil_run6 = [ $run6, boil_run7 ] {ai_run(16);};
void() boil_run7 = [ $run7, boil_run8 ] {ai_run(8);};
void() boil_run8 = [ $run8, boil_run1 ] {ai_run(4);};

//======================================================================
// Running Boil Explosive Attack
//======================================================================
void() boil_expl1 = [ $explode1, boil_expl2 ] {ai_face();
    sound (self, CHAN_VOICE, "zombie/boil_windup.wav", 1, ATTN_NORM);};
void() boil_expl2 = [ $explode2, boil_expl3 ] {ai_face();};
void() boil_expl3 = [ $explode3, boil_expl4 ] {ai_face();};
void() boil_expl4 = [ $explode4, boil_expl5 ] {ai_face();};
void() boil_expl5 = [ $explode5, boil_expl6 ] {ai_face();};
void() boil_expl6 = [ $explode6, boil_expl7 ] {ai_face();};
void() boil_expl7 = [ $explode7, boil_expl8 ] {ai_face();};
void() boil_expl8 = [ $explode8, boil_expl9 ] {ai_face();};
void() boil_expl9 = [ $explode9, boil_expl9 ] {
    T_Damage (self, self, self, self.health+1, NOARMOR);
};

//======================================================================
// Stationary Boil - Nailed to the floor or wall
//======================================================================
void() boil_suicide =
{ 
    if (self.health < 1) return;
    self.use = SUB_Null;
    // Make sure current enemy is the cause of the damage
    T_Damage (self, self, self.enemy, self.health+1, NOARMOR);
};

//----------------------------------------------------------------------
void() boil_checktarget =
{
    local entity client;
    
    // Should never have an active enemy
    self.enemy = self.goalentity = world;
    
    // Get the obvious exception(s) done first
    if (self.health < 1) return;
    if (intermission_running) return;

    // Find a client in current PVS
    client = checkclient ();

    // Go through all the exception(s)
    if (!client) return FALSE;
    if (!(client.flags & FL_CLIENT)) return;
    if (client.flags & FL_NOTARGET) return;
    if (client.items & IT_INVISIBILITY) return;

    // Check if the player is visible?
    enemy_vis = visible(client);
    if (!enemy_vis) return;
    
    // Store client/player for later damage
    self.enemy = client;

    // Is the player within range to blow up?
    self.enemydist = range_distance(client, FALSE);
    if (self.enemydist < self.t_width) self.th_melee();
};

//----------------------------------------------------------------------
void() boil_squirtblood =
{
    if (self.waitmin < time) {
        self.waitmin = time + self.wait + random() * self.wait;
        sound (self, CHAN_WEAPON, "zombie/boil_squirt.wav", 1, ATTN_IDLE);
        
        SpawnProjectileMeat(self,self.origin,self.brkvelbase_x, self.brkvelbase_y, self.brkvelbase_z);
        // Randomly spawn an extra gib of flesh
        if (random() < 0.25) SpawnProjectileMeat(self,self.origin,self.brkvelbase_x, self.brkvelbase_y, self.brkvelbase_z);
    }
};

//----------------------------------------------------------------------
void() boil_hoba1 = [ $hobA1, boil_hoba2 ] {
    self.nextthink = self.nextthink + random()*0.2;
    monster_idle_sound();boil_squirtblood();boil_checktarget();
};
void() boil_hoba2 = [ $hobA2, boil_hoba3 ] {boil_checktarget();};
void() boil_hoba3 = [ $hobA3, boil_hoba4 ] {
    self.nextthink = self.nextthink + random()*0.2;
    boil_checktarget();};
void() boil_hoba4 = [ $hobA2, boil_hoba1 ] {boil_checktarget();};

//----------------------------------------------------------------------
void() boil_HexpA1 = [ $hobA2, boil_HexpA2 ] {
    sound (self, CHAN_VOICE, "zombie/boil_windup.wav", 1, ATTN_NORM);};
void() boil_HexpA2 = [ $hobA3, boil_HexpA3 ] {};
void() boil_HexpA3 = [ $hobA4, boil_HexpA4 ] {};
void() boil_HexpA4 = [ $hobA5, boil_HexpA5 ] {};
void() boil_HexpA5 = [ $hobA6, boil_HexpA6 ] {};
void() boil_HexpA6 = [ $hobA7, boil_HexpA7 ] {};
void() boil_HexpA7 = [ $hobA8, boil_HexpA8 ] {};
void() boil_HexpA8 = [ $hobA9, boil_HexpA9 ] {};
void() boil_HexpA9 = [ $hobA10, boil_HexpA9 ] {boil_suicide();};

//----------------------------------------------------------------------
void() boil_hobb1 = [ $hobB1, boil_hobb2 ] {
    self.nextthink = self.nextthink + random()*0.2;
    monster_idle_sound();boil_squirtblood();boil_checktarget();
};
void() boil_hobb2 = [ $hobB2, boil_hobb3 ] {boil_checktarget();};
void() boil_hobb3 = [ $hobB3, boil_hobb4 ] {
    self.nextthink = self.nextthink + random()*0.2;
    boil_checktarget();};
void() boil_hobb4 = [ $hobB2, boil_hobb1 ] {boil_checktarget();};

//----------------------------------------------------------------------
void() boil_HexpB1 = [ $hobB2, boil_HexpB2 ] {
    sound (self, CHAN_VOICE, "zombie/boil_windup.wav", 1, ATTN_NORM);};
void() boil_HexpB2 = [ $hobB3, boil_HexpB3 ] {};
void() boil_HexpB3 = [ $hobB4, boil_HexpB4 ] {};
void() boil_HexpB4 = [ $hobB5, boil_HexpB5 ] {};
void() boil_HexpB5 = [ $hobB6, boil_HexpB6 ] {};
void() boil_HexpB6 = [ $hobB7, boil_HexpB7 ] {};
void() boil_HexpB7 = [ $hobB8, boil_HexpB8 ] {};
void() boil_HexpB8 = [ $hobB9, boil_HexpB9 ] {};
void() boil_HexpB9 = [ $hobB10, boil_HexpB9 ] {boil_suicide();};

//======================================================================
// NO PAIN
//======================================================================
void(entity inflictor, entity attacker, float take) boil_pain =
{
    // Spawn some grey particles and chunk of meat
    SpawnProjectileSmoke(self.origin, self.brkvelbase_x, self.brkvelbase_y, self.brkvelbase_z);
    SpawnProjectileMeat(self,self.origin, self.brkvelbase_x, self.brkvelbase_y, self.brkvelbase_z);
    // Random chance of more smokey meat
    if (random() < 0.5) {
        SpawnProjectileSmoke(self.origin, self.brkvelbase_x, self.brkvelbase_y, self.brkvelbase_z);
        SpawnProjectileMeat(self,self.origin, self.brkvelbase_x, self.brkvelbase_y, self.brkvelbase_z);
    }

    // No special animation
    return; 
};

//============================================================================
void() boil_explode = {
    // Needs special version of radius damage function (T_RadiusDamage)
    // Boss monsters are always immune to this kind of damage
    // Needs to do infighting damage before T_Damage function
    // There is no rocket resistance to this kind damage!
    // Must pass on self.enemy so that any death triggers work properly
    //
    self.enemytarget = findradius(self.origin, self.death_dmg+40);
    // Loop through chain list
    while(self.enemytarget) {
        // Ignore self and world and can never damage bosses
        if (self.enemytarget != world && self.enemytarget != self && self.enemytarget.bossflag == 0) {
            // Check for any breakables which are prone to explosive damage
            if (ai_foundbreakable(self.enemy, self.enemytarget,TRUE) && 
            self.enemytarget.brktrigmissile != 0) {
                trigger_ent(self.enemytarget, self.enemy);
            }
            else {
                // Can be damaged and NOT immune to radius (splash) damage
                if (self.enemytarget.takedamage > 0 && self.enemytarget.noradiusdmg == 0) {
                    // Make sure monsters take more damage
                    if (self.enemytarget.flags & FL_MONSTER)
                        self.dmg = self.death_dmg * self.infightextra;
                    else {
                        // Use original radius formula for non monsters
                        self.pos1 = self.enemytarget.origin + (self.enemytarget.mins + self.enemytarget.maxs)*0.5;
                        self.dmg = 0.5*vlen (self.origin - self.pos1);
                        if (self.dmg < 0) self.dmg = 0;
                        self.dmg = self.death_dmg - self.dmg;
                    }
                    
                    if (self.dmg > 0 && self.enemytarget.health > 0) {
                        // Need CanDamage to check for anything blocking LoS
                        if (CanDamage (self.enemytarget, self))
                            // Pass on self.enemy so death triggers fire correctly
                            T_Damage (self.enemytarget, self, self.enemy, self.dmg, DAMARMOR);
                            // make sure to check for poisonous and apply debuff
                            if (self.dmg > 0 && self.poisonous == TRUE) PoisonDeBuff(self.enemytarget);
                    }
                }
            }
        }
        // Move forward in chain to next entity
        self.enemytarget = self.enemytarget.chain;
    }
    
    /*----------------------------------------------------------------------
    // I really liked this idea of the boil destroying any monster
    // bodies lying closeby, BUT this is a serious processor drain
    // on maps with a huge amount of monsters.
    // Every monster that dies is marked and left on the floor and
    // this function would search them all everytime a boil explodes!
    //
    // The code below works fine, BUT it should only be enabled
    // for maps with small amount of available monsters
    //----------------------------------------------------------------------
    
    // Check for any nearby dead/lying bodies
    self.enemytarget = find(world, bodyonflr, MON_ONFLR);
    self.dmg = self.death_dmg * self.infightextra;
    // restrict size of dead body search list
    self.count = 64;

    // Cycle through list of "marked" dead bodies
    // These entities cannot be found by a findradius command
    // They are setup nonsolid and have to be checked manually
    while (self.enemytarget) {
        // Work out how close the body is to the boil
        self.enemydist = vlen(self.origin - self.enemytarget.origin);
        if (self.enemydist < self.death_dmg+40) {
            // Use shadow axe gib function (moved to ai_gib)
            monster_flrbody_gib(self.enemytarget, self.dmg);
        }
        // Only check the first xx dead bodies in the list
        self.count = self.count - 1;
        if (self.count < 1) self.enemytarget = world;
        else self.enemytarget = find(self.enemytarget, bodyonflr, MON_ONFLR);
    }
    */
    
    // Show classic grenade explosion effect
    // Switch explosions effect based on poison flag
    if (self.poisonous) {
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_EXPLOSION2);
        WriteCoord (MSG_BROADCAST, self.origin_x);
        WriteCoord (MSG_BROADCAST, self.origin_y);
        WriteCoord (MSG_BROADCAST, self.origin_z);
        WriteByte (MSG_BROADCAST, 51);
        WriteByte (MSG_BROADCAST, 8);
    }
    else {
        // Show classic grenade explosion effect
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_EXPLOSION);
        WriteCoord (MSG_BROADCAST, self.origin_x);
        WriteCoord (MSG_BROADCAST, self.origin_y);
        WriteCoord (MSG_BROADCAST, self.origin_z);
    }
};

//----------------------------------------------------------------------
void() boil_die =
{
    // Upward fountain of gibs
    self.max_health = MON_GIBFOUNTAIN;
    monster_death_precheck();
    // New gib + loud explosion sound
    sound (self, CHAN_AUTO, "zombie/boil_explode.wav", 1, ATTN_NORM);
    // Delay the t_radius explosion for one frame
    // Just in case several are standing next to each other
    // otherwise there will be a runaway crash loop
    self.think = boil_explode;
    self.nextthink = time + 0.1;
};


/*======================================================================
 QUAKED monster_boil (1 0 0) (-16 -16 -24) (16 16 32)
======================================================================*/
void() monster_boil =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_boil.mdl";
    precache_model (self.mdl);

    self.idle_sound = "zombie/boil_idle1.wav";
    self.idle_sound2 = "zombie/boil_idle2.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);
    
    self.sight_sound = "zombie/boil_sight.wav";
    precache_sound (self.sight_sound);
    self.pain_sound = SOUND_EMPTY;
    precache_sound ("zombie/boil_squirt.wav");
    precache_sound ("zombie/boil_windup.wav");
    precache_sound ("zombie/boil_explode.wav");

    precache_poisongibs();                // precache gibs
    if (self.poisonous) self.gibtype = GIBTYPE_POISON;
    
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_SHORT;
    if (self.health < 1) self.health = 40;
    self.gibhealth = -60;                // Never used
    self.gibbed = FALSE;                // In one piece
    self.pain_flinch = 10;                // Has gib splash
    self.steptype = FS_TYPESLOW;        // Tiny feet
    self.gibondeath = TRUE;                // Always blow up!
    self.blockudeath = TRUE;            // No humanoid death sound
    if (self.death_dmg < 1) self.death_dmg = DAMAGE_BOIL;
    if (self.infightextra < 1) self.infightextra = 16;

    self.attack_instant = 1;            // Start moving right away
    
    // Setup timer for flesh gib squirting from body
    if (self.wait < 1) self.wait = 4;
    self.waitmin = time + self.wait + random() * self.wait;
    // Randomly pick one from the first four skins
    // 0=flesh, 1=brown, 2=green1, 3=green2, 4=flesh
    if (!self.exactskin) self.randomskin = 4;
    self.brkvelbase = '200 50 150';

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    // All boils die the same way
    self.th_checkattack = BoilCheckAttack;
    self.th_pain = boil_pain;
    self.th_die = boil_die;

    self.classtype = CT_MONBOIL;
    self.classgroup = CG_SPAWN;
    self.classmove = MON_MOVEWALK;

    // Setup random hobble animation
    if (self.spawnflags & ( MON_BOIL_HOBBLED | MON_BOIL_HANGING)) {
        // A = Crossed legs, B = Straight legs
        if (random() < 0.5) {        
            self.th_stand = self.th_walk = self.th_run = boil_hoba1;
            self.th_melee = boil_HexpA1;
        }
        else {
            self.th_stand = self.th_walk = self.th_run = boil_hobb1;
            self.th_melee = boil_HexpB1;
        }
        // No movement, turret mode
        self.movespeed = -1;
        // The stationary boil cannot start angry at anything
        self.spawnflags = self.spawnflags - (self.spawnflags & MON_SPAWN_ANGRY);
        self.angrytarget = "";
        // Instantly blow up when triggered
        self.think1 = boil_suicide;
        // Cannot have sight sound, its nailed to wall/floor!
        self.sight_sound = SOUND_EMPTY;
        // Allow for override on exploding trigger radius
        if (self.t_width < 1) self.t_width = MONAI_HOBBLEBOIL;
    }
    
    // Hobbled hanging upside down on wall?
    if (self.spawnflags & MON_BOIL_HANGING) {
        // Make sure angle is within range (0-360)
        if (self.angles_y < 0) self.angles_y = 360;
        // Calculate opposite facing direction
        self.movedir = '0 0 0';
        self.movedir_y = anglemod(self.angles_y + 180);
        // Calculate upward vector and rotate monster
        makevectors(self.angles);
        self.angles = self.movedir + vectoangles(v_up);
        // Pull monster away from wall
        self.origin = self.origin + v_forward*10;
        // Allow monster to float and no ground check
        self.classmove = MON_MOVEFLY;
        self.flags = self.flags | FL_FLY;
        // Setup squirt distance for blood
        self.brkvelbase = '100 50 150';
    }
    // Hobbled legs with spikes (ground pose)
    else if (self.spawnflags & MON_BOIL_HOBBLED) {
        // Check for random floor rotation and setup no movment
        if (self.angles_y < 0) self.angles_y = rint(random()*360);
        // Setup squirt distance for blood
        self.brkvelbase = '100 300 100';
    }
    else {
        // Allow for override on exploding trigger radius
        if (self.t_width < 1) self.t_width = MONAI_MELEEBOIL;
        // Default stand, walk & run
        self.th_stand = boil_stand1;
        self.th_walk = boil_walk1;
        self.th_run = boil_run1;
        self.th_melee = boil_expl1;
    }

    if (self.deathstring == "")  self.deathstring = " was blown away by a Boil\n";
    
    monster_start();
};
