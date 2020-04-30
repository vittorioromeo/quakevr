/*======================================================================
 TARGET FUNCTIONS
 
 Trigger_relay and trigger_count are not triggers, they don't have
 any touch functionality and don't even set/use any of the activator
 system used by most trigger/func entities.
 
 These entities are essentially targets that reply on input from other
 targets (use) and then use their targets (conditionally). They should
 be re-classifed as target_relay and target_count, but it is too late now
 for such a change, the opportunity has long gone!
 
======================================================================*/
float TRIG_COUNTNOMESSAGE = 1;    // Prevents all count messages
float TRIG_RANDOMTARGET = 2;    // Randomly trigger target/target2
float TRIG_RELAYSTARTDIS = 32;    // trigger_delay starts disabled
float TRIG_EXPLODENOEFF = 2;    // No old particle effect
float TRIG_EXPLODEDUST = 4;        // Exploding projectile dust
float TRIG_MONKILLDFUNC = 16;    // kill monsters via death function
float TRIG_DELAY = 64;            // Starts disabled

float TRIG_ENGFITZ = 1;            // Fitz engine
float TRIG_ENGDP = 2;            // DP engine
float TRIG_ENGFTE = 4;            // FTE/QSS engine
float TRIG_ENGRAIN = 16;        // Check for rain effects
float TRIG_ENGSNOW = 32;        // Check for snow effects

/*======================================================================
/*QUAKED trigger_relay (0.5 0 0.5) (-8 -8 -8) (8 8 8) x RANDOM x x x x x x Not_Easy Not_Normal Not_Hard
Triggers target(s) with custom sounds and messages
-------- KEYS --------
targetname : trigger entity
target  : targets to trigger when relay is activated
target2 : secondary targets to trigger when activated
upgrade_ssg : = 1 will only trigger if shotgun upgrade active on server
upgrade_axe : = 1 will only trigger if axe upgrade active on server
upgrade_lg  : = 1 will only trigger if lightning gun upgrade active on server
wait    : -1 = will only fire targets once
delay   : delay before firing (after being triggered)
cnt     : random amount of time to add to delay
waitmin : % random chance between target/target2
sounds  : 1=Secret,2=talk(def),3=switch,4=silent,5=custom,6=secret2
noise   : custom sound to play when triggered
message : message to display when triggered
-------- SPAWNFLAGS --------
RANDOM : Will randomly select between target/target2
-------- NOTES --------
This fixed size trigger cannot be touched, it can only be fired by other events.  
Can contain killtargets, targets, delays, and messages.

======================================================================*/
void() trigger_relay_use =
{
    if (self.estate & ESTATE_BLOCK) return;
    if (self.attack_finished > time) return;
    
    //----------------------------------------------------------------------
    // The Shotgun and Axe upgrades can make maps too easy, allow for
    // triggers to not fire if the key is TRUE and inventory empty
    //----------------------------------------------------------------------
    if ( other.flags & FL_CLIENT ) {
        if (self.upgrade_axe && !(other.moditems & IT_UPGRADE_AXE)) return;
        if (self.upgrade_ssg && !(other.moditems & IT_UPGRADE_SSG) ) return;
        if (self.upgrade_lg && !(other.moditems & IT_UPGRADE_LG) ) return;
    }

    // Setup to trigger once?
    if (self.wait < 0) self.attack_finished = LARGE_TIMER;
    if (self.noise != "") sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);

    // Randomly pick between target/target2
    // SUB_UseTargets will fire both target strings if found
    // the random choices need to be stored elsewhere (noise3/4)
    if (self.spawnflags & TRIG_RANDOMTARGET) {
        if (random() < self.waitmin) self.target = self.noise3;
        else self.target = self.noise4;
    }
    
    // One thing to note about trigger_relay is that it does not change the
    // activator global variable to the entity that used this trigger last
    // This is handy for client test triggers (like trigger_secret)
    SUB_UseTargets();
};

//----------------------------------------------------------------------
void() trigger_relay =
{
    trigger_bmodel_sounds();            // Precache any sounds
    self.classtype = CT_TRIGRELAY;
    if (self.delay <= 0) self.delay = 0;
    if (self.cnt > 0) self.delay = self.delay + random()*self.cnt;
    
    // Setup random trigger selection strings and random %
    // No checks are done on the target/target so that they can
    // be blank and the mapper can randomly select an empty trigger
    // only check = The random % has got to exist between 0-1
    if (self.spawnflags & TRIG_RANDOMTARGET) {
        if (self.waitmin <=0 || self.waitmin >= 1) self.waitmin = 0.5;
        self.noise3 = self.noise4 = "";        // Initialize strings
        self.noise3 = self.target;            // Copy over strings
        self.noise4 = self.target2;
        self.target = self.target2 = "";    // Remove originals
    }

    // Check for firing conditions (nightmare, coop)
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = trigger_relay_use;
    // Setup intial entity state (can start disabled)
    if (self.spawnflags & TRIG_RELAYSTARTDIS) self.estate = ESTATE_DISABLE;
    else self.estate = ESTATE_ON;
};

/*======================================================================
/*QUAKED trigger_counter (0.5 0 0.1) (-8 -8 -8) (8 8 8) NOMESSAGE x x x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
A counter which triggers target(s) when complete
-------- KEYS --------
targetname : trigger entity (works with entity state system)
target  : trigger target(s) when complete
message2: message to display when complete (ignores NOMESSAGE spawnflag)
count   : number of triggers needed to fire own target, (def=2)
delay   : time delay to fire final trigger event
sounds  : 0=silent,1=Secret,2=talk,3=switch,5=custom,6=secret2
noise   : custom sound to play when complete
-------- SPAWNFLAGS --------
NOMESSAGE : disables count display
STARTOFF : Requires trigger to activate
-------- NOTES --------
A counter which triggers target(s) when complete

======================================================================*/
void() trigger_counter_reset =
{
    // Reset counter to initial (spawning) value
    self.count = self.height;
};

//----------------------------------------------------------------------
void() trigger_counter_use =
{
    if (self.estate & ESTATE_BLOCK) return;

    // Check for any pre-count messages
    if (self.startmsg != "") {
        centerprint (activator, self.startmsg);
        self.startmsg = "";
        self.nextthink = time + self.wait;
        self.think = self.estate_use;
        return;
    }

    self.count = self.count - 1;
    if (self.count < 0) return;
        
    // Count down messages for trigger
    if (self.count > 0) {
        if (activator.flags & FL_CLIENT && !(self.spawnflags & TRIG_COUNTNOMESSAGE) ) {
            if (self.count >= 4) centerprint (activator, "There are more to go...");
            else if (self.count == 3) centerprint (activator, "Only 3 more to go...");
            else if (self.count == 2) centerprint (activator, "Only 2 more to go...");
            else centerprint (activator, "Only 1 more to go...");
        }
        return;
    }
    
    // Reach zero on counter, time to trigger counter target
    if (activator.flags & FL_CLIENT) {
        if (self.message2 != "") centerprint(activator, self.message2);
        else if ( !(self.spawnflags & TRIG_COUNTNOMESSAGE) )
            centerprint(activator, "Sequence completed!");
    }    
    
    // If sound defined, play sound
    if (self.noise != "") sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);

    // This is a big problem with the trigger_counter entity, it does not update
    // the activator global variable and does not wake up enemies correctly.
    // If a trigger_count is targetting monsters then they will not get angry
    // at the player, the count has to target a _once or _multi trigger instead.
    self.enemy = activator;
    SUB_UseTargets();
};

//----------------------------------------------------------------------
void() trigger_counter_delay =
{
    if (self.estate & ESTATE_BLOCK) return;

    // Remove the trigger delay function
    if (self.spawnflags & ENT_STARTOFF)
        self.spawnflags = self.spawnflags - ENT_STARTOFF;
    
    // Re-route use function to actual counter
    self.estate_use = trigger_counter_use;
};

//----------------------------------------------------------------------
void() trigger_counter =
{
    trigger_bmodel_sounds();            // Precache any sounds
    if (!self.count) self.count = 2;    // default count
    self.height = self.count;            // Save for later, reset
    self.classtype = CT_TRIGCOUNT;

    // Check for firing conditions (nightmare, coop)
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_reset = trigger_counter_reset;
    self.estate = ESTATE_ON;

    // The delay function is not switched off, its activate to use
    if (self.spawnflags & ENT_STARTOFF) self.estate_use = trigger_counter_delay;
    else self.estate_use = trigger_counter_use;
};

/*======================================================================
/*QUAKED trigger_engine (0 0 1) (-8 -8 -16) (8 8 16) FITZ DP FTE x RAIN SNOW x x
Triggers target(s) when certain engine active
-------- KEYS --------
targetname : trigger entity
target  : targets to trigger when relay is activated
wait    : -1 = will only fire targets once
delay   : delay before firing (after being triggered)
cnt     : random amount of time to add to delay
-------- SPAWNFLAGS --------
FITZ : Only trigger for Fitz engines (default type)
DP   : Only trigger for DP engine
FTE  : Only trigger for FTE/QSS engines
RAIN : Check for rain effect being active
SNOW : Check for snow effect being active
-------- NOTES --------
Triggers target(s) when certain engine active

======================================================================*/
void() trigger_engine_fire =
{
    // Parameter to check at of tests
    self.aflag = FALSE;
    
    // Check each engine type for conditions
    // This trigger will not reset the activator
    // Should be done with one of the targets instead
    if (self.spawnflags & TRIG_ENGFITZ && engine == ENG_FITZ) self.aflag = TRUE;
    else if (self.spawnflags & TRIG_ENGDP && engine == ENG_DPEXT) self.aflag = TRUE;
    // Extra check for FTE/QSS (they support DP particles)
    else if (self.spawnflags & TRIG_ENGFTE && engine == ENG_DPEXT) {
        if (checkextension("FTE_SV_POINTPARTICLES")) self.aflag = TRUE;
    }

    if (self.aflag == TRUE) {
        // Check for engine weather effects
        if (self.spawnflags & TRIG_ENGRAIN && !ext_dprain) self.aflag = FALSE;
        if (self.spawnflags & TRIG_ENGSNOW && !ext_dpsnow) self.aflag = FALSE;
        // Check for weather being disabled
        if (self.spawnflags & (TRIG_ENGRAIN | TRIG_ENGSNOW)) {
            if (query_configflag(SVR_WEATHER)) self.aflag = FALSE;
        }
    
        // If engine and/or weather active, fire targets!
        if (self.aflag == TRUE) SUB_UseTargets();
    }
};

//----------------------------------------------------------------------
void() trigger_engine_use =
{
    if (self.estate & ESTATE_BLOCK) return;
    if (self.attack_finished > time) return;

    // Setup to trigger once?
    if (self.wait < 0) self.attack_finished = LARGE_TIMER;
    
    // Check for any trigger delay?
    if (self.delay > 0) {
        self.think = trigger_engine_fire;
        self.nextthink = time + self.delay;
    }
    else trigger_engine_fire();
};        

//----------------------------------------------------------------------
void() trigger_engine =
{
    self.classtype = CR_TRIGENG;

    // Check for firing conditions (nightmare, coop)
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = trigger_engine_use;
    self.estate = ESTATE_ON;
};

/*======================================================================
/*QUAKED trigger_clientmsg (0.5 0.5 0) (-8 -8 -16) (8 8 16) x
Centerprints a message to all clients
-------- KEYS --------
targetname : trigger entity
message : Text to center print
sounds  : 1=Secret,2=talk,3=switch,4=silent(def),5=custom,6=secret2
noise   : custom sound to play when triggered
-------- SPAWNFLAGS --------
-------- NOTES --------
Centerprints a message to all clients

======================================================================*/
void() trigger_clientmsg_use =
{
    if (self.estate & ESTATE_BLOCK) return;
    
    // Write message to all clients
    msg_entity = self;
    WriteByte (MSG_ALL, SVC_CENTERPRINT);
    WriteString (MSG_ALL, self.message);

    // If any sounds defined play them at entity source
    if (self.noise != "") sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
};

//----------------------------------------------------------------------
void() trigger_clientmsg =
{
    // Default = the sound of silence!
    if (self.sounds == 0) self.sounds = 4;
    trigger_bmodel_sounds();
    
    // Setup default message
    if (self.message == "") self.message = "Default Trigger Message!\n";
    self.classtype = CT_TRIGCLMSG;

    // Check for firing conditions (nightmare, coop)
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = trigger_clientmsg_use;
    self.estate = ESTATE_ON;
};

/*======================================================================
/*QUAKED trigger_cdtrack (0.8 0.5 0) (-8 -8 -16) (8 8 16) x
Change CD track for all clients
-------- KEYS --------
targetname : trigger entity
count      : CD track number (eg. 0-x)
-------- SPAWNFLAGS --------
-------- NOTES --------
Change CD track for all clients

======================================================================*/
void() trigger_cdtrack_change =
{
    // Write CD track to all clients
    msg_entity = self;
    WriteByte (MSG_ALL, SVC_CDTRACK);
    WriteByte (MSG_ALL, trig_cdtrack);
    WriteByte (MSG_ALL, trig_cdtrack);
};

//----------------------------------------------------------------------
void() trigger_cdtrack_use =
{
    // Check for entity state system block
    if (self.estate & ESTATE_BLOCK) return;
    
    // Make sure CD track change in savefile
    trig_cdtrack = self.count;

    // Write message to all clients
    trigger_cdtrack_change();
};

//----------------------------------------------------------------------
void() trigger_cdtrack =
{
    self.classtype = CT_TRIGCDTRACK;
    // Make sure specified cd track is integer number
    self.count = fabs(rint(self.count));

    // Check for firing conditions (nightmare, coop)
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = trigger_cdtrack_use;
    self.estate = ESTATE_ON;
};

/*======================================================================
/*QUAKED trigger_skybox (0.9 0.9 0.9) (-8 -8 -16) (8 8 16) x
Load/Change Skybox for all clients
-------- KEYS --------
targetname : trigger entity
message : Skybox name (eg moonrise_)
-------- SPAWNFLAGS --------
-------- NOTES --------
Load/Change Skybox for all clients

======================================================================*/
void(entity targ) trigger_skybox_change =
{
    // Write skybox change to all clients
    stuffcmd(targ, "SKY ");
    stuffcmd(targ, trig_skybox);
    stuffcmd(targ, "\n");
};

//----------------------------------------------------------------------
void() trigger_skybox_use =
{
    // Check for entity state system block
    if (self.estate & ESTATE_BLOCK) return;
    
    // Check for client trigger first
    if (activator.classtype == CT_PLAYER) self.enemy = activator;
    
    // Find a player for the stuff command
    if (!self.enemy) {
        self.enemy = find(world,targetname,"player");
        if (self.enemy.classtype != CT_PLAYER) return;
    }
    
    // Write skybox name to all clients
    trig_skybox = self.message;
    trigger_skybox_change(self.enemy);    
};

//----------------------------------------------------------------------
void() trigger_skybox =
{
    self.classtype = CT_TRIGSKYBOX;
    if (self.message == "") self.message = "Sky";

    // Check for firing conditions (nightmare, coop)
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = trigger_skybox_use;
    self.estate = ESTATE_ON;
};

/*======================================================================
/*QUAKED trigger_itemrespawnupd (0.5 0 0.5) (-8 -8 -16) (8 8 16) x x x x RESPAWN x x x
Change the state of respawn flag on items
-------- KEYS --------
targetname : trigger entity
target  : this points to the item to affect
-------- SPAWNFLAGS --------
RESPAWN : Value of respawn flag to copy to item
-------- NOTES --------
Change the state of respawn flag on items
Useful for switching off respawning items after arena fight is over

======================================================================*/
void() trigger_itemrespawnupd_use =
{
    if (self.estate & ESTATE_BLOCK) return;

    // Loop around target(s) and update respawn flag
    self.enemy = find (world, targetname, self.target);
    while(self.enemy) {
        // only work with active items
        if (self.enemy.flags & FL_ITEM) {
            if (self.spawnflags & ITEM_RESPAWN) {
                self.enemy.spawnflags = self.enemy.spawnflags | ITEM_RESPAWN;
            }
            else {
                // Remove respawn flag (even if missing)
                self.enemy.spawnflags = self.enemy.spawnflags - (self.enemy.spawnflags & ITEM_RESPAWN);
            }
        }
        // Are there anymore targets left in the list?
        self.enemy = find (self.enemy, targetname, self.target);
    } 
};

//----------------------------------------------------------------------
void() trigger_itemrespawnupd =
{
    self.classtype = CT_TRIGITEMFLAG;

    // Check for firing conditions (nightmare, coop)
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = trigger_itemrespawnupd_use;
    self.estate = ESTATE_ON;
};

/*======================================================================
QUAKED trigger_monstermovespeed (0.5 0 0.5) (-8 -8 -16) (8 8 16) x
Toggle the monster move speed state
-------- KEYS --------
targetname : trigger entity (works with entity state system)
target  : this points to the monster(s) to affect
state   : -1 = No movement, 0 = Toggle, 1 = Free movement
wait    : -1 = trigger once only
-------- SPAWNFLAGS --------
-------- NOTES --------
Toggle the monster move speed state

======================================================================*/
void() trigger_monstermovespeed_use =
{
    if (self.estate & ESTATE_BLOCK) return;

    // Loop around target(s) and update move speed
    self.enemy = find (world, targetname, self.target);
    while(self.enemy) {
        // only work with living monsters (ignore rest)
        if (self.enemy.flags & FL_MONSTER && self.enemy.health > 0) {
            // Is the state a toggle or exact update?
            if (self.state == 0) {
                if (self.enemy.movespeed < 1) self.enemy.movespeed = 1;
                else self.enemy.movespeed = -1;
            }
            // Exact value update
            else self.enemy.movespeed = self.state;
        }
        // Are there anymore targets left in the list?
        self.enemy = find (self.enemy, targetname, self.target);
    } 
    
    // Trigger once functionality?
    if (self.wait < 0) self.estate = ESTATE_OFF;
};

//----------------------------------------------------------------------
void() trigger_monstermovespeed =
{
    self.classtype = CT_TRIGMONMOVE;
    if (self.target == "") {
        dprint("\b[MON_MOVESPEED]\b No target set, removing\n");
        remove(self);
    }
    
    // make sure state has the correct values
    if (self.state < -1 || self.state > 1) self.state = 0;

    // Check for firing conditions (nightmare, coop)
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = trigger_monstermovespeed_use;
    self.estate = ESTATE_ON;
};

/*======================================================================
/*QUAKED trigger_monsterkill (0.5 0 0.5) (-8 -8 -16) (8 8 16) x x x x DEATH x x x
Remove monster(s) from the map
-------- KEYS --------
targetname : Name of this trigger entity
target  : Name of monster(s) to remove/stop
-------- SPAWNFLAGS --------
DEATH : kill monsters via death function
-------- NOTES --------
Remove monster(s) from the map

======================================================================*/
void() trigger_monsterkill_use =
{
    if (self.estate & ESTATE_BLOCK) return;
    // This only works once!
    self.use = SUB_Null;

    // Loop around target(s) and remove the game
    self.enemy = find (world, targetname, self.target);
    while(self.enemy) {
        // There are always exceptions to the use of the monster flag
        // Point Hell knights don't use monster flag to prevent damage!
        if (self.enemy.spawnflags & MON_POINT_KNIGHT && self.enemy.health > 0) 
            self.enemy.flags = self.enemy.flags | FL_MONSTER;
        
        // only work with monsters, can't be dead, dying or negative health!
        if (self.enemy.flags & FL_MONSTER && self.enemy.health > 0 && !self.enemy.deadflag) {
            // Check if death function is required for monster
            // Also double check they have a death function setup!
            if (self.spawnflags & TRIG_MONKILLDFUNC && self.enemy.th_die) {
                self.enemy.takedamage = DAMAGE_YES;
                // Always do enough damage to kill the monster
                // Can be a problem for ammo resistant
                T_Damage (self.enemy, self, self, self.enemy.health+1, NOARMOR);
            }
            else {
                self.enemy.deadflag = DEAD_DEAD;
                self.enemy.enemy = world;
                self.enemy.health = self.enemy.gibhealth;

                // Update kill counter if no exceptions!
                if (!self.enemy.nomonstercount) {
                    killed_monsters = killed_monsters + 1;
                    WriteByte (MSG_ALL, SVC_KILLEDMONSTER);
                }

                // Stop all animation think functions
                self.enemy.think = SUB_Null;
                self.enemy.nextthink = time + LARGE_TIMER;
                // Finally stop and hide all models
                // Should not remove monster, it might have dependancies
                entity_hide(self.enemy);
                entity_remove(self.enemy.attachment, 0.1);
                entity_remove(self.enemy.attachment2, 0.1);
                entity_remove(self.enemy.attachment3, 0.1);
            }
        }
        // Are there anymore targets left in the list?
        self.enemy = find (self.enemy, targetname, self.target);
    } 
};

//----------------------------------------------------------------------
void() trigger_monsterkill =
{
    self.classtype = CT_TRIGMONKILL;

    // Check for firing conditions (nightmare, coop)
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;
    
    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = trigger_monsterkill_use;
    self.estate = ESTATE_ON;
};

/*======================================================================
/*QUAKED trigger_monsterattack (0.5 0 0.5) (-8 -8 -16) (8 8 16) x
Force a monster to attack a certain target
-------- KEYS --------
targetname : Name of this trigger entity
target  : Name of monster to affect
target2 : Name of entity to attack
-------- SPAWNFLAGS --------
-------- NOTES --------
Force a monster to attack a certain target

======================================================================*/
void() trigger_monsterattack_use =
{
    if (self.estate & ESTATE_BLOCK) return;

    // Find target monster first and see if alive?
    self.enemy = find (world, targetname, self.target);
    // only work with monsters, can't be dead, dying or negative health!
    if (self.enemy.flags & FL_MONSTER && self.enemy.health > 0) {
        self.oldenemy = find(world, targetname, self.target2);
        // Check if attacking target is alive and can be damaged?
        if (self.oldenemy.health > 0 && self.oldenemy.takedamage != DAMAGE_NO) {
            // Setup monster to attack new target (use damage function)
            T_Damage (self.enemy, self.oldenemy, self.oldenemy, 1, NOARMOR);
        }
    } 
};

//----------------------------------------------------------------------
void() trigger_monsterattack =
{
    self.classtype = CT_TRIGMONATT;

    // Check for firing conditions (nightmare, coop)
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = trigger_monsterattack_use;
    self.estate = ESTATE_ON;
};

//======================================================================
// General purpose sprite+particle explosion
// Entity is fired once (default)
//======================================================================
/*QUAKED trigger_explode (.8 .5 .5) (-4 -4 -4) (4 4 4) x EXPLODENOEFF DUST
Triggered entity producing damage + sprite explosion
-------- KEYS --------
targetname : trigger entity (works with entity state system)
dmg    : explosive radius damage (def=40, -1=no damage)
delay  : time delay to explosion (def=0s)
noise  : string name for explosion sound (def=weapons/r_exp3b.wav)
wait   : re-trigger explosions (def=-1 trigger once)
style  : 0=Explosion, 1=Plasma, 2=Poison, 3=Electric, 4=Burst (Smoke/Flame/Poison)
height : 0=Small, 1=Medium, 2=Large, -1=Random
count  : Random amount of dust particles to spawn (1-x)
pos1   : Base dust velocity (X=Forward, Y=Right, Z=Up)
pos2   : Random dust velocity (X=Forward, Y=Right, Z=Up)
-------- SPAWNFLAGS --------
EXPLODENOEFF : no old school particle explosion
DUST : Dust particle explosion (use angle for direction)
-------- NOTES --------
Triggered entity producing damage + sprite explosion

=============================================================================*/
void() trig_explode_fire =
{
    // Dust particles are empty models with rocket smoke trails
    if (self.spawnflags & TRIG_EXPLODEDUST) {
        // Setup direction of projectile
        makevectors(self.angles);
        // Always spawn at least 1 dust particle
        self.lip = 1 + rint(random() * self.count);
        while (self.lip > 0) {
            // Keep spawning temporary entities
            newmis = spawn();
            newmis.classgroup = CG_TEMPENT;
            newmis.movetype = MOVETYPE_TOSS;
            newmis.solid = SOLID_NOT;
            setmodel(newmis, MODEL_PROJ_SMOKE);
            
            // Randomize the origin of the particles
            self.oldorigin = self.origin + (v_right*(crandom()*self.pos1_y));
            setorigin(newmis, self.oldorigin);
            setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
            
            // Use pos1+pos2 vector (calculated from angles)
            self.pos3_x = self.pos1_x + ( random() * self.pos2_x );
            self.pos3_y = crandom() * (self.pos1_y + ( random() * self.pos2_y ));
            self.pos3_z = self.pos1_z + ( random() * self.pos2_z );
            newmis.velocity = (v_forward*self.pos3_x) + (v_right*self.pos3_y) + (v_up*self.pos3_z);
            
            // Setup removal and keep counting
            newmis.nextthink = time + 1 + random()*3;
            newmis.think = SUB_Remove;
            self.lip = self.lip - 1;
        }
        // No damage or sprites needed
        return;
    }
    
    // create any explosive radius damage    
    if (self.dmg > 0) T_RadiusDamage (self, self, self.dmg, self, DAMAGEALL);

    // Check for old particle effect
    if ( !(self.spawnflags & TRIG_EXPLODENOEFF) ) {
        // classic ID explosion
        if (self.style == 0) {
            WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
            WriteByte (MSG_BROADCAST, TE_EXPLOSION);
            WriteCoord (MSG_BROADCAST, self.origin_x);
            WriteCoord (MSG_BROADCAST, self.origin_y);
            WriteCoord (MSG_BROADCAST, self.origin_z);
        }
        // New Rogue extension - Blue
        else if (self.style == 1) {
            WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
            WriteByte (MSG_BROADCAST, TE_EXPLOSION2);
            WriteCoord (MSG_BROADCAST, self.origin_x);
            WriteCoord (MSG_BROADCAST, self.origin_y);
            WriteCoord (MSG_BROADCAST, self.origin_z);
            WriteByte (MSG_BROADCAST, 39);    // Blue 32 + 7
            WriteByte (MSG_BROADCAST, 8);    // Colour range
        }
        // New Rogue extension - Green
        else if (self.style == 2) {
            WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
            WriteByte (MSG_BROADCAST, TE_EXPLOSION2);
            WriteCoord (MSG_BROADCAST, self.origin_x);
            WriteCoord (MSG_BROADCAST, self.origin_y);
            WriteCoord (MSG_BROADCAST, self.origin_z);
            WriteByte (MSG_BROADCAST, 55);    // Green 48 + 7
            WriteByte (MSG_BROADCAST, 8);    // Colour range
        }
    }

    // Check for randomly sized explosion type
    if (self.height == -1) self.lip = rint(0.5+ random() * 2.5);
    else self.lip = rint(self.height);
    // Work out explosion type offset
    self.lip = self.lip + rint(self.style) * 10;
    
    // Use global function (Fitz/DP aware)
    SpawnExplosion(self.lip, self.origin, self.noise);
};

//----------------------------------------------------------------------
void() trig_explode_use =
{
    if (self.estate & ESTATE_BLOCK) return;
    if (self.attack_finished > time) return;
    
    // Check for Trigger once setting
    if (self.wait < 0) self.attack_finished = LARGE_TIMER;
    
    // Check for explosion delay or straight away blow up
    if (self.delay > 0) {
        self.nextthink = time + self.delay;
        self.think = trig_explode_fire;
    }
    else trig_explode_fire();
};

//----------------------------------------------------------------------
void() trigger_explode =
{
    // Default settings
    self.classtype = CT_TRIGEXPLODE;    
    if (self.dmg >= 0) self.dmg = DAMAGE_MONROCKET;
    if (self.wait == 0) self.wait = -1;
    if (self.height == 0 || self.height > 3) self.height = 1;
    if (self.style < 0 || self.style > 4) self.style = 0;
    self.attack_finished = 0;
    
    if (self.noise == "") {
        if (self.style == 3) self.noise = SOUND_PLASMA_HIT;
        else if (self.style == 4) self.noise = SOUND_RESIST_ROCKET;
        else self.noise = SOUND_REXP3;        // Default
    }
    precache_sound (self.noise);

    // Precache smoke particles and set default movement
    if (self.spawnflags & TRIG_EXPLODEDUST) {
        precache_model(MODEL_PROJ_SMOKE);
        if (CheckZeroVector(self.angles)) self.angles_y = 360;
        if (CheckZeroVector(self.pos1)) self.pos1 = '100 25 100';
        if (CheckZeroVector(self.pos2)) self.pos2 = '100 25 100'; 
    }        
    
    // Burst Sprites are NOT precached in world.qc
    if (self.style == 4) {
        // Check for random selection
        if (self.height == -1) {
            precache_model(SBURST_SMOKE);
            precache_model(SBURST_FLAME);
            precache_model(SBURST_POISON);
        }
        else if (self.height == 1) precache_model(SBURST_SMOKE);
        else if (self.height == 2) precache_model(SBURST_FLAME);
        else precache_model(SBURST_POISON);
    }

    // Check for firing conditions (nightmare, coop)
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = trig_explode_use;
    self.estate = ESTATE_ON;
};
