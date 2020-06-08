/*==============================================================================

Rubicon 2 code by john fitzgibbons

Files created for rubicon 2:
- rubicon2.qc
                    * misc_flag                animated hanging banner
                    * misc_sparks            spark emitter
- dread.qc            * monster_dreadnaught    soldier with flame thrower
- floyd.qc            * monster_floyd            automaton enemy
- centurion.qc        * monster_centurion        air centurion
- turret.qc            * func_turret            defense turret

Files modified for rubicon 2 (search for "johnfitz" to find each modified bit):

- ai.qc                * modified pain_forward() so floyd animations look better
                    * added new sight sounds: floyd, dread
                    * fixed monsters attacking intermission camera
- buttons.qc        * added "movedir" key -- use it to set a button's move vector directly
- client.qc            * added new obituaries
                    * removed axe and shotgun from default items in SetNewParms() but only on certain maps
                    * added code for impulse 666 (resurrection cheat)
                    * fixed armor absorbs drowning damage
                    * added ladder movement code
                    * added burning player code
- combat.qc            * modified T_Damage() so that armour won't protect you from drowning damage
- defs.qc            * added new fields and constants
                    * changed IT_EXTRA_WEAPON to IT_NO_WEAPON
- doors.qc            * fixed unlock sound not being played on keyed doors
                    * added "movedir" key -- use it to set a door's move vector directly
- items.qc            * weapon_axe
                    * weapon_shotgun
- fish.qc            * fixed fish staying solid too long after death
- misc.qc            * fixed ambient sounds consume edicts
                    * ambient_general
- monsters.qc        * fixed fish count twice towards total_monsters
- plats.qc            * added "retrigger" flag to func_train -- train will wait to be retriggered at each path_corner
                    * more func_train sound options
- player.qc            * added player hurt sounds: steam(smoke), flame
- progs.src            * added lines to compile the new .qc files
- triggers.qc        * trigger_setskill is now targetable
- weapons.qc        * added code for having no weapons (IT_NO_WEAPON)
                    * added code to make a clanking sound when hitting damagable brushmodels with axe
                    * fixed impulsecommands() called even though impulse = 0
                    * added new cheats (impulse 252, 253, 254 = ring, biosuit, pent)
- world.qc            * fixed sounds for cheats not being precached
                    * added lightstyles 12 through 15
                    
Files added for rubicon2:

- hiprot.qc            * info_rotate
                    * func_movewall
                    * func_rotate_door
                    * func_rotate_entity
                    * func_rotate_train
                    * path_rotate
                    * rotate_object
                    * added sounds 4 = screechy metal for func_rotate_door

==============================================================================*/

/*
===============================================================================
misc_flag
===============================================================================
*/

/*QUAKED misc_flag (1 0 0) (-8 -8 -8) (8 8 8) NOT_ANIMATED BIG
A hanging banner, gently waving in the wind. Normal dimensions: 64 wide by 144 long.

Flags:
"Not Animated" Banner is not animated.
"Big" Banner is twice as big: 128 wide by 288 long.
*/
void () misc_flag =
{
    precache_model ("progs/flag.mdl");
    setmodel (self, "progs/flag.mdl");
    self.frame = (self.spawnflags & 3);
    makestatic (self);
};

/*
===============================================================================
misc_sparks
===============================================================================
*/

void()    sparks_fade1 = [0, sparks_fade2] {self.alpha = 0.8; self.nextthink = time + 0.05;};
void()    sparks_fade2 = [0, sparks_fade3] {self.alpha = 0.6; self.nextthink = time + 0.05;};
void()    sparks_fade3 = [0, sparks_fade4] {self.alpha = 0.4; self.nextthink = time + 0.05;};
void()    sparks_fade4 = [0, SUB_Remove]   {self.alpha = 0.2; self.nextthink = time + 0.05;};

void() sparks_use =
{
    if (self.spawnflags & ENT_STARTOFF)
        self.spawnflags = self.spawnflags - ENT_STARTOFF;
    else
        self.spawnflags = self.spawnflags + ENT_STARTOFF;
};

void() make_sparks;

void() spark_turnofflight = 
{
    SUB_UseTargets();
    self.think = make_sparks;
    self.nextthink = time + (random() + 0.5)*self.wait - 0.15;
}

void() make_sparks =
{
      
    if (self.spawnflags & ENT_STARTOFF)
    {
        self.nextthink = time + 0.1;
        self.think = make_sparks;
    }
    else
    {
        local float i;
        i = -0.25*self.cnt + random()*0.5*self.cnt;
        while (i < self.cnt)
        {
            
            local entity spark;
            spark = spawn();
            spark.owner = self;
            setmodel (spark, "progs/spark.mdl");
            setorigin (spark, self.origin);
            spark.movetype = MOVETYPE_BOUNCE;
            spark.solid = SOLID_TRIGGER;
            spark.gravity = 0.3;
            spark.velocity_x = -40 + random() * 80;
            spark.velocity_y = -40 + random() * 80;
            spark.velocity_z = -40 + random() * 80;
            spark.avelocity = '3000 3000 3000';
            spark.nextthink = time + 0.5 + 1.5*random();
            spark.think = sparks_fade1;
            spark.classname = "spark";
            
            if (random() < 0.33)
                spark.skin = 0;
            else if (random() < 0.5)
                spark.skin = 1;
            else
                spark.skin = 2;
                
            if (self.spawnflags & SPARKS_PALE)
                spark.skin = spark.skin + 6;
            else if (self.spawnflags & SPARKS_BLUE)
                spark.skin = spark.skin + 3;
            
            setsize (spark, '0 0 0', '0 0 0');
            i = i + 1;
        }
        if (self.sounds == 1)
            sound (self, CHAN_AUTO, "misc/spark.wav", 1, ATTN_STATIC);
        SUB_UseTargets();
        self.nextthink = time + 0.1 + random() * 0.1;
        self.think = spark_turnofflight;

    }
    
};

/*QUAKED misc_sparks (0 .5 .8) (-8 -8 -8) (8 8 8) START_OFF SPARKS_BLUE SPARKS_PALE
Produces a burst of yellow sparks at random intervals. If targeted, it will toggle between on or off.  If it targets a light, that light will flash allong with each burst of sparks. Note: targeted lights should be set to START_OFF.

SPARKS_BLUE: sparks are blue in color

SPARKS_PALE: sparks are pale yellow in color

Keys:

"wait" is the average delay between bursts (variance is 1/2 wait). Default is 2. 

"cnt" is the average number of sparks in a burst (variance is 1/4 cnt). Default is 15.

"sounds"
0)    no sound
1)    sparks
*/
void() misc_sparks =
{
    precache_model ("progs/spark.mdl");
    precache_sound ("misc/spark.wav");
        
    if (!self.movedir)
        self.movedir = '0 0 -30';
    if (!self.wait)
        self.wait = 2;
    if (!self.cnt)
        self.cnt = 15;
    
    self.use = sparks_use;
    self.nextthink = time + random()*0.1;
    self.think = make_sparks;
};