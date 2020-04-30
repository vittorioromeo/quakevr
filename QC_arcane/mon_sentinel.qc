/*==============================================================================
SENTINEL (Originally from Quoth - Kell/Necros/Preach)
  * QC was created for AD mappers to play with in their own projects
  * Original Quoth model/sounds not included with main MOD
 
  Interesting QC traits
  * All stand/walk/run go through single function, easier setup
  * Has no front section to model, wakeup event is 360 FOV
  * Does not move when attacking so can maintain original height
  * Will move very fast when out of sight of enemy
  * No head model or body on death, just a pile of gibs
  
  QUOTH assets required to get this monster working in AD
    (model's in 'progs' and wav's in 'sound' sub directories)
  * sentinel.mdl -> mon_sentinel.mdl (rename file)
  * sentinel/widle1.wav, sentinel/widle2.wav, sentinel/wsight.wav
  * sentinel/laser.wav, sentinel/nail.wav, sentinel/wpain.wav

 ==============================================================================*/
// Writhe (move tentacles) animation
$frame idle1 idle2 idle3 idle4 idle5 idle6 idle7 idle8 idle9 idle10

// Pain (move tentacles) animation
$frame pain1 pain2 pain3 pain4 pain5 pain6

// Export frames (ignored)
$frame base1

float SENT_STAND = 0;        // Default state
float SENT_WALK = 1;        // Patrolling
float SENT_RUN = 2;            // Attacking - Sentry mode

//======================================================================
// Update Sentinel every frame
// Its a great shame the quoth model has no idle animations
// - could have been body attachments or different tentacle movements
//======================================================================
void() sent_update = 
{
    // If Sentinel is dead, no more updates
    if (self.health < 1) return;

    // Time for an idle sound?
    if (self.idletimer < time) monster_idle_sound();

    // Update animation frame
    self.frame = $idle1 + self.walkframe;

    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 9) self.walkframe = 0;
    self.nextthink = time + 0.1;
    self.think = sent_update;
    
    // Check sentinel states
    if (self.attack_timer == SENT_STAND) {
        self.solid = SOLID_SLIDEBOX;
        self.movetype = MOVETYPE_FLY;

        // Change the movement type so that can easily move up/down
        // using velocity, forced origin movement is really jerky!
        if (self.velocity_x == 0 && self.velocity_y == 0) {
            if (self.attack_finished < time) {
                self.attack_finished = time + 2;
                if (self.lip < 1) self.lip = 1;
                else self.lip = -1;
                self.velocity_z = 2 * self.lip;
            }
        }
    
        ai_stand();
    }
    else if (self.attack_timer == SENT_WALK) {
        self.solid = SOLID_SLIDEBOX;
        self.movetype = MOVETYPE_STEP;
        ai_walk(8);
    }
    else if (self.attack_timer == SENT_RUN) {
        self.solid = SOLID_SLIDEBOX;
        self.movetype = MOVETYPE_STEP;
        // If can see enemy, don't move, just fire
        if (visible(self.enemy)) {
            self.movespeed = -1;
            ai_run(0);
        }
        // Cannot see enemy, track them down
        else {
            self.movespeed = 1;
            ai_run(8);
        }
    }
};

//======================================================================
// All stand, walk and run functions are condensed down to one entry
// Might as well be one loop as there is only one animation set
//
void() sent_stand = { self.attack_timer = SENT_STAND; sent_update(); };
void() sent_walk = { self.attack_timer = SENT_WALK; sent_update(); };
void() sent_run = { self.attack_timer = SENT_RUN; sent_update(); };

//===========================================================================
// RANGE ATTACK - Fires spikes or laser bolts
//===========================================================================
void() sent_attack =
{
    local vector org, dir, vec;

    // Keep cycling the pain animation
    self.think = sent_run;
    self.nextthink = time + 0.1;    

    if (self.enemy && self.health > 0) {
        
        // Always make sure there is no monster or obstacle in the way
        // Cannot use enemy entity direct, enemytarget will be active
        if ( !visxray(SUB_entEnemyTarget(), self.attack_offset, '0 0 12', FALSE) ) return;

        self.effects = self.effects | EF_MUZZLEFLASH;

        makevectors (self.angles);
        org = self.origin + attack_vector(self.attack_offset);

        // Aim high to catch jumping players
        dir = SUB_orgEnemyTarget() + '0 0 12';
        vec = normalize(dir - org);
        // Laser/nail speed : 575=easy, 650=normal, 725=hard, nm=800
        self.attack_speed = SPEED_SENTPROJ + (skill * SPEED_SENTPROJSKILL);

        // Switch projectile type
        if (self.spawnflags & MON_SENTINEL_NAIL) {
            if (random() < 0.2) sound (self, CHAN_WEAPON, "sentinel/nail.wav", 1, ATTN_NORM);
            else sound (self, CHAN_WEAPON, "weapons/rocket1i.wav", 1, ATTN_NORM);
            launch_projectile(org, vec, CT_PROJ_MONNG, self.attack_speed);
        }
        else {
            if (random() < 0.2) sound (self, CHAN_WEAPON, "sentinel/laser.wav", 1, ATTN_NORM);
            else sound (self, CHAN_WEAPON, "enforcer/enfire.wav", 1, ATTN_NORM);
            launch_projectile(org, vec, CT_PROJ_LASER, self.attack_speed);
        }
    }
};

//============================================================================
// ROBOT PAIN!?!
//============================================================================
void() sent_inpain =
{
    // Update animation frame
    self.frame = $pain1 + self.inpain;
    // Move backwards away from damage
    ai_backface(5-self.inpain);

    // Keep cycling the pain animation
    self.think = sent_inpain;
    self.nextthink = time + 0.1;
    
    // Start of pain cycle
    if (self.inpain ==  0) {
        // Spawn a pile of sparks and dust falling down
        particle_dust(self.origin, 10+random()*10, PARTICLE_BURST_YELLOW);
    }
    // Finished, back to combat
    else if (self.inpain >= 5) {
        self.think = self.th_run;
    }
    
    // Next pain animation
    self.inpain = self.inpain + 1;
};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) sent_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Stop any ai_run velocity and reset movetype
    self.velocity = '0 0 0';
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
    self.inpain = 0;
    
    sent_inpain();
};

//============================================================================
void() sent_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // Final fireworks!
    particle_dust(self.origin, 10+random()*10, PARTICLE_BURST_YELLOW);
    SpawnProjectileSmoke(self.origin, 150, 50, 150);
    SpawnProjectileSmoke(self.origin, 150, 50, 150);
    SpawnExplosion(EXPLODE_BIG, self.origin, self.death_sound);

    // no more sentinel
    entity_hide (self);
    // Make sure gibs go flying up
    self.max_health = MON_GIBFOUNTAIN;
    self.health = -100;

    // Regular blood like gibs
    ThrowGib(4, 2 + rint(random()*4));
    ThrowGib(5, 1);
    // Metal and custom body parts
    self.gibtype = GIBTYPE_METAL;
    ThrowGib(11, 2 + rint(random()*2));
    ThrowGib(12, 2 + rint(random()*2));    
};

/*======================================================================
QUAKED monster_sentinel (1 0 0) (-16 -16 -24) (16 16 24) Ambush
======================================================================*/
void() monster_sentinel =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_sentinel.mdl";        // AD naming
    self.gib1mdl = "progs/gib_metal1.mdl";        // Breakable metal
    self.gib2mdl = "progs/gib_metal3.mdl";        // Breakable metal

    precache_model (self.mdl);
    precache_model (self.gib1mdl);                // Generic metal1_2
    precache_model (self.gib2mdl);                // Generic metal1_2
    
    self.idle_sound = "sentinel/widle1.wav";
    self.idle_sound2 = "sentinel/widle2.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);

    // Default attack - lasers!
    precache_model (MODEL_PROJ_LASER);            // Copy of enforcer laser
    precache_sound("sentinel/laser.wav");        // Unique laser fire
    precache_sound ("enforcer/enfire.wav");
    precache_sound ("enforcer/enfstop.wav");

    // Alternative attack - red hot nails!?!
    if (self.spawnflags & MON_SENTINEL_NAIL) {
        self.exactskin = 1;                        // Nail version
        precache_model (MODEL_PROJ_NGRED);        // Copy of freddie nails
        precache_sound("weapons/rocket1i.wav");
        precache_sound("sentinel/nail.wav");    // Unique nail fire
    }

    self.pain_sound = "sentinel/wpain.wav";
    precache_sound (self.pain_sound);
    self.death_sound = "jim/explode_major.wav";
    precache_sound (self.death_sound);
    
    self.sight_sound = "sentinel/wsight.wav";
    precache_sound (self.sight_sound);

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_SHORT;
    if (self.health < 1) self.health = 75;
    self.gibhealth = MON_NEVERGIB;        // Cannot be gibbed by weapons
    self.gibbed = FALSE;
    self.pain_flinch = 30;                // Sometimes flinch
    self.steptype = FS_FLYING;            // Silent feet
    self.pain_longanim = FALSE;            // No long pain animation
    self.blockudeath = TRUE;            // No humanoid death sound
    self.idlemoreoften = TRUE;            // More creepy idle sounds
    self.poisonous = FALSE;                // Robots are not poisonous
    if (self.height == 0) self.height = 32; // Custom height
    self.walkframe = 0;                    // Reset frame counter
    self.attack_offset = '0 0 14';        // front/middle of body
    self.sight_nofront = TRUE;            // Has no front facing
    self.deathstring = " was scorched by a Sentinel\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    self.th_checkattack = SentinelCheckAttack;
    self.th_stand = sent_stand;
    self.th_walk = sent_walk;
    self.th_run = sent_run;
    self.th_missile = sent_attack;
    self.th_pain = sent_pain;
    self.th_die = sent_die;

    self.classtype = CT_MONSENTINEL;
    self.classgroup = CG_ROBOT;
    self.classmove = MON_MOVEFLY;
    
    monster_start();
};
