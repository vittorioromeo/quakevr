void() trigger_reactivate =
{
    self.solid = SOLID_TRIGGER;
};

//=============================================================================

float    SPAWNFLAG_NOMESSAGE = 1;
float    SPAWNFLAG_NOTOUCH = 1;

// the wait time has passed, so set back up for another activation
void() multi_wait =
{
    if(self.max_health)
    {
        self.health = self.max_health;
        self.takedamage = DAMAGE_YES;
        self.solid = SOLID_BBOX;
    }
};


// the trigger was just touched/killed/used
// self.enemy should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
void() multi_trigger =
{
    if(self.nextthink > time)
    {
        return;        // allready been triggered
    }

    if(self.classname == "trigger_secret")
    {
        if(self.enemy.classname != "player")
            return;
        found_secrets = found_secrets + 1;
        WriteByte(MSG_ALL, SVC_FOUNDSECRET);

        // HONEY
        self.think = SUB_Null;
        if(skill == 0){

            remove(self.oldenemy);
        }
    }

    if(self.noise)
        sound(self, CHAN_VOICE, self.noise, 1, ATTN_NORM);

// don't trigger again until reset
    self.takedamage = DAMAGE_NO;

    activator = self.enemy;

    SUB_UseTargets();

    if(self.wait > 0)
    {
        self.think = multi_wait;
        self.nextthink = time + self.wait;
    }
    else
    {    // we can't just remove(self) here, because this is a touch function
        // called while C code is looping through area links...
        self.touch = SUB_Null;
        self.nextthink = time + 0.1;
        self.think = SUB_Remove;
    }

    // HIPNOTIC
    //MED 12/01/96 added cnt stuff
    if(self.cnt > 0)
       {
       self.cnt = self.cnt - 1;
       if(self.cnt == 0)
          {
          self.touch = SUB_Null;
          self.nextthink = time + 0.1;
          self.think = SUB_Remove;
          }
       }
};

void() multi_killed =
{
    self.enemy = damage_attacker;
    multi_trigger();
};

void() multi_use =
{
    self.enemy = activator;
    multi_trigger();
};

void() multi_touch =
{
    if(other.classname != "player")
        return;

// if the trigger has an angles field, check player's facing direction
    if(self.movedir != '0 0 0')
    {
        makevectors(other.angles);
        if(v_forward * self.movedir < 0)
            return;        // not facing the right way
    }

    self.enemy = other;
    multi_trigger();
};

//MED 12/01/96 added count field
/*QUAKED trigger_multiple (.5 .5 .5) ? notouch
Variable sized repeatable trigger.  Must be targeted at one or more entities.  If "health" is set, the trigger must be killed to activate each time.
If "delay" is set, the trigger waits some time after activating before firing.
"wait" : Seconds between triggerings. (.2 default)
"cnt" how many times it can be triggered (infinite default)
If notouch is set, the trigger is only fired by other entities, not by touching.
NOTOUCH has been obsoleted by trigger_relay!
sounds
1)    secret
2)    beep beep
3)    large switch
4)
set "message" to text string
*/
void() trigger_multiple =
{
    if(self.sounds == 1)
    {
        precache_sound("misc/secret.wav");
        self.noise = "misc/secret.wav";
    }
    else if(self.sounds == 2)
    {
        precache_sound("misc/talk.wav");
        self.noise = "misc/talk.wav";
    }
    else if(self.sounds == 3)
    {
        precache_sound("misc/trigger1.wav");
        self.noise = "misc/trigger1.wav";
    }

    if(!self.wait)
        self.wait = 0.2;
    self.use = multi_use;

    InitTrigger();

    if(self.health)
    {
        if(self.spawnflags & SPAWNFLAG_NOTOUCH)
            objerror ("health and notouch don't make sense\n");
        self.max_health = self.health;
        self.th_die = multi_killed;
        self.takedamage = DAMAGE_YES;
        self.solid = SOLID_BBOX;
        setorigin(self, self.origin);    // make sure it links into the world
    }
    else
    {
        if(!(self.spawnflags & SPAWNFLAG_NOTOUCH))
        {
            self.touch = multi_touch;
        }
    }

    // HIPNOTIC
    //MED 12/01/96 added cnt stuff
    if(self.cnt == 0)
    {
        self.cnt = -1;
    }

    // HONEY
    // Stupid hack
    if(self.target == "" && self.message != "" && self.wait != 2)
    {
        self.wait = 2;
    }
};


/*QUAKED trigger_once (.5 .5 .5) ? notouch
Variable sized trigger. Triggers once, then removes itself.  You must set the key "target" to the name of another object in the level that has a matching
"targetname".  If "health" is set, the trigger must be killed to activate.
If notouch is set, the trigger is only fired by other entities, not by touching.
if "killtarget" is set, any objects that have a matching "target" will be removed when the trigger is fired.
if "angle" is set, the trigger will only fire when someone is facing the direction of the angle.  Use "360" for an angle of 0.
sounds
1)    secret
2)    beep beep
3)    large switch
4)
set "message" to text string
*/
void() trigger_once =
{
    self.wait = -1;
    trigger_multiple();
};

//=============================================================================

/*QUAKED trigger_relay (.5 .5 .5) (-8 -8 -8) (8 8 8)
This fixed size trigger cannot be touched, it can only be fired by other events.  It can contain killtargets, targets, delays, and messages.
*/
void() trigger_relay =
{
    self.use = SUB_UseTargets;
};


//=============================================================================

/*QUAKED trigger_secret (.5 .5 .5) ?
secret counter trigger
sounds
1)    secret
2)    beep beep
3)
4)
set "message" to text string
*/
void() trigger_secret =
{
    local vector o;
    local entity secamb;
    local float sk;

    total_secrets = total_secrets + 1;
    self.wait = -1;
    if(!self.message)
        self.message = "You found a secret area!";
    if(!self.sounds)
        self.sounds = 1;

    if(self.sounds == 1)
    {
        precache_sound("misc/secret.wav");
        self.noise = "misc/secret.wav";
    }
    else if(self.sounds == 2)
    {
        precache_sound("misc/talk.wav");
        self.noise = "misc/talk.wav";
    }

    trigger_multiple();

    // HONEY
    sk = cvar_hget(cvarh_skill); //Stupid hack because the skill global isn't set until in the first StartFrame call
    if(sk == 0){
        o = (self.mins + self.maxs)*0.5;
        secamb = ambient_secret_spawn(o);
        self.oldenemy = secamb;
    }
};

//=============================================================================


void() counter_use =
{
    self.count = self.count - 1;
    if(self.count < 0)
        return;

    if(self.count != 0)
    {
        if(activator.classname == "player"
        && (self.spawnflags & SPAWNFLAG_NOMESSAGE) == 0)
        {
            if(self.count >= 4)
                centerprint(activator, "There are more to go...");
            else if(self.count == 3)
                centerprint(activator, "Only 3 more to go...");
            else if(self.count == 2)
                centerprint(activator, "Only 2 more to go...");
            else
                centerprint(activator, "Only 1 more to go...");
        }
        return;
    }

    if(activator.classname == "player"
    && (self.spawnflags & SPAWNFLAG_NOMESSAGE) == 0)
        centerprint(activator, "Sequence completed!");
    self.enemy = activator;
    multi_trigger();
};

/*QUAKED trigger_counter (.5 .5 .5) ? nomessage
Acts as an intermediary for an action that takes multiple inputs.

If nomessage is not set, t will print "1 more.. " etc when triggered and "sequence complete" when finished.

After the counter has been triggered "count" times (default 2), it will fire all of it's targets and remove itself.
*/
void() trigger_counter =
{
    self.wait = -1;
    if(!self.count)
        self.count = 2;

    self.use = counter_use;
};


/*
==============================================================================

TELEPORT TRIGGERS

==============================================================================
*/

float    PLAYER_ONLY    = 1;
float    SILENT = 2;
float    CTF_ONLY = 4;
float    IGNORE_TARGETNAME = 8; // HONEY

void() play_teleport =
{
    local    float v;
    local    string tmpstr;

    v = random() * 5;
    if(v < 1)
        tmpstr = "misc/r_tele1.wav";
    else if(v < 2)
        tmpstr = "misc/r_tele2.wav";
    else if(v < 3)
        tmpstr = "misc/r_tele3.wav";
    else if(v < 4)
        tmpstr = "misc/r_tele4.wav";
    else
        tmpstr = "misc/r_tele5.wav";

    sound(self, CHAN_VOICE, tmpstr, 1, ATTN_NORM);
    remove(self);
};

void(vector org) spawn_tfog =
{
    local entity s;
    s = spawn();
    s.origin = org;
    s.nextthink = time + 0.2;
    s.think = play_teleport;

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_TELEPORT);
    WriteCoord(MSG_BROADCAST, org_x);
    WriteCoord(MSG_BROADCAST, org_y);
    WriteCoord(MSG_BROADCAST, org_z);
};


void() tdeath_touch =
{
    if(other == self.owner)
        return;

// frag anyone who teleports in on top of an invincible player
    if(other.classname == "player")
    {
        if(other.invincible_finished > time)
            self.classname = "teledeath2";
        if(self.owner.classname != "player")
        {    // other monsters explode themselves
            T_Damage (self.owner, self, self, 50000);
            return;
        }

    }

    if(other.health)
    {
        T_Damage (other, self, self, 50000);
    }
};


void(vector org, entity death_owner) spawn_tdeath =
{
local entity    death;

    death = spawn();
    death.classname = "teledeath";
    death.movetype = MOVETYPE_NONE;
    death.solid = SOLID_TRIGGER;
    death.angles = '0 0 0';
    setsize(death, death_owner.mins - '1 1 1', death_owner.maxs + '1 1 1');
    setorigin(death, org);
    death.touch = tdeath_touch;
    death.nextthink = time + 0.2;
    death.think = SUB_Remove;
    death.owner = death_owner;

    force_retouch = 2;        // make sure even still objects get hit
};

void() teleport_touch =
{
local entity    t;
local vector    org;

    if(self.targetname)
    {
        if (/* HONEY */ !(self.spawnflags & IGNORE_TARGETNAME) && self.nextthink < time)
        {
            return;        // not fired yet
        }
    }

    if(self.spawnflags & PLAYER_ONLY)
    {
        if(other.classname != "player")
            return;
    }

// only teleport living creatures
    if(other.health <= 0 || other.solid != SOLID_SLIDEBOX)
        return;

    SUB_UseTargets();

// put a tfog where the player was
    spawn_tfog (other.origin);

    t = find(world, targetname, self.target);
    if(!t)
        objerror ("couldn't find target");

// spawn a tfog flash in front of the destination
    makevectors(t.mangle);
    org = t.origin + 32 * v_forward;

    spawn_tfog (org);
    spawn_tdeath(t.origin, other);

// move the player and lock him down for a little while
    if(!other.health)
    {
        other.origin = t.origin;
        other.velocity = (v_forward * other.velocity_x) + (v_forward * other.velocity_y);
        return;
    }

    setorigin(other, t.origin);
    other.angles = t.mangle;
    if(other.classname == "player")
    {
        other.fixangle = 1;        // turn this way immediately
        other.teleport_time = time + 0.7;
        if(other.flags & FL_ONGROUND)
            other.flags = other.flags - FL_ONGROUND;
        other.velocity = v_forward * 300;
    }
    other.flags = other.flags - other.flags & FL_ONGROUND;
};

/*QUAKED info_teleport_destination (.5 .5 .5) (-8 -8 -8) (8 8 32)
This is the destination marker for a teleporter.  It should have a "targetname" field with the same value as a teleporter's "target" field.
*/
void() info_teleport_destination =
{
// this does nothing, just serves as a target spot
    self.mangle = self.angles;
    self.angles = '0 0 0';
    self.model = "";
    self.origin = self.origin + '0 0 27';
    if(!self.targetname)
        objerror ("no targetname");
};

void() teleport_use =
{
    self.nextthink = time + 0.2;
    force_retouch = 2;        // make sure even still objects get hit
    self.think = SUB_Null;
};

/*QUAKED trigger_teleport (.5 .5 .5) ? PLAYER_ONLY SILENT CTF_ONLY
Any object touching this will be transported to the corresponding info_teleport_destination entity. You must set the "target" field, and create an object with a "targetname" field that matches.

If the trigger_teleport has a targetname, it will only teleport entities when it has been fired.
*/
void() trigger_teleport =
{
    local vector o;

    // HONEY
    if(self.spawnflags & SPAWNFLAG_TRIGGER_SPAWN){
        self.use = trigger_teleport;
        self.spawnflags = (self.spawnflags & PLAYER_ONLY) | SILENT | IGNORE_TARGETNAME;
        return;
    }

    if(self.spawnflags & CTF_ONLY)
    {
        if(teamplay < TEAM_CTF || teamplay > TEAM_CTF_ALT)
        {
            remove(self);
            return;
        }
    }

    InitTrigger();
    self.touch = teleport_touch;
    // find the destination
    if(!self.target)
        objerror ("no target");
    self.use = teleport_use;

    if(!(self.spawnflags & SILENT))
    {
        precache_sound("ambience/hum1.wav");
        o = (self.mins + self.maxs)*0.5;
        ambientsound(o, "ambience/hum1.wav",0.5 , ATTN_STATIC);
    }
};

/*
==============================================================================

trigger_setskill

==============================================================================
*/

void() trigger_skill_touch =
{
    if(other.classname != "player")
        return;

    cvar_set ("skill", self.message);
};

/*QUAKED trigger_setskill (.5 .5 .5) ?
sets skill level to the value of "message".
Only used on start map.
*/
void() trigger_setskill =
{
    InitTrigger();
    self.touch = trigger_skill_touch;
};


/*
==============================================================================

ONLY REGISTERED TRIGGERS

==============================================================================
*/

void() trigger_onlyregistered_touch =
{
    if(other.classname != "player")
        return;
    if(self.attack_finished > time)
        return;

    self.attack_finished = time + 2;
    if(cvar_hget(cvarh_registered))
    {
        self.message = "";
        SUB_UseTargets();
        remove(self);
    }
    else
    {
        if(self.message != "")
        {
            centerprint(other, self.message);
            sound(other, CHAN_BODY, "misc/talk.wav", 1, ATTN_NORM);
        }
    }
};

/*QUAKED trigger_onlyregistered (.5 .5 .5) ?
Only fires if playing the registered version, otherwise prints the message
*/
void() trigger_onlyregistered =
{
    precache_sound("misc/talk.wav");
    InitTrigger();
    self.touch = trigger_onlyregistered_touch;
};

//============================================================================

// HIPNOTIC
//JIM
void(entity ent, float amount) hurt_setdamage =
   {
   ent.dmg = amount;
   if(!amount)
      {
      ent.solid = SOLID_NOT;
      }
   else
      {
      ent.solid = SOLID_TRIGGER;
      }
   ent.nextthink = -1;
   };

void() hurt_on =
{
    self.solid = SOLID_TRIGGER;
    self.nextthink = -1;

    // HONEY
    force_retouch = 2;
};

void() hurt_touch =
{
    if(other.takedamage)
    {
        self.solid = SOLID_NOT;
        T_Damage (other, self, self, self.dmg);
        self.think = hurt_on;
        self.nextthink = time + 1;

    // HIPNOTIC
//MED 12/01/96 added count stuff
      if(self.cnt > 0)
         {
         self.cnt = self.cnt - 1;
         if(self.cnt == 0)
            {
            self.touch = SUB_Null;
            self.nextthink = time + 0.1;
            self.think = SUB_Remove;
            }
         }
   }

    return;
};

//MED 12/01/96 added count field
/*QUAKED trigger_hurt (.5 .5 .5) ?
Any object touching this will be hurt
set dmg to damage amount
defalt dmg = 5
"cnt" default infinite, how many times to trigger
*/
void() trigger_hurt =
{
    InitTrigger();
    self.touch = hurt_touch;
    if(!self.dmg)
        self.dmg = 5;

    // HIPNOTIC
//MED 12/01/96 added count stuff
   if(self.cnt == 0)
      self.cnt = -1;
};

//============================================================================

float PUSH_ONCE =     1;
float PUSH_TOGL =     2;
float PUSH_ACTIVE =    4;

void() trigger_push_touch =
{
    if(!(self.spawnflags & PUSH_ACTIVE))
        return;

    if(other.classname == "grenade")
        other.velocity = self.speed * self.movedir * 10;
    else if(other.classname == "MiniGrenade")
        other.velocity = self.speed * self.movedir * 10;
    else if(other.classname == "MultiGrenade")
        other.velocity = self.speed * self.movedir * 10;

    else if(other.health > 0)
    {
        other.velocity = self.speed * self.movedir * 10;
        if(other.classname == "player")
        {
            if(other.fly_sound < time)
            {
                other.fly_sound = time + 1.5;
                sound(other, CHAN_AUTO, "ambience/windfly.wav", 1, ATTN_NORM);
            }
        }
    }
    if(self.spawnflags & PUSH_ONCE)
        remove(self);
};

void() trigger_push_use =
{
    if(self.spawnflags & PUSH_ACTIVE)
        self.spawnflags = self.spawnflags - PUSH_ACTIVE;
    else
        self.spawnflags = self.spawnflags + PUSH_ACTIVE;
};


/*QUAKED trigger_push (.5 .5 .5) ? PUSH_ONCE PUSH_TOGL PUSH_ACTIVE
Pushes the player.

speed: velocity of push. (default 1000)
*/
void() trigger_push =
{
    if(self.spawnflags & PUSH_TOGL)
        self.use = trigger_push_use;
    else
        self.spawnflags = self.spawnflags + PUSH_ACTIVE;

    InitTrigger();
    precache_sound("ambience/windfly.wav");
    self.touch = trigger_push_touch;
    if(!self.speed)
        self.speed = 1000;
};

//============================================================================

// HONEY
float ALWAYS_JUMP = 2;
float NO_VELOCITY = 4;


void() trigger_monsterjump_touch =
{
    if(other.flags & (FL_MONSTER | FL_FLY | FL_SWIM) != FL_MONSTER)
        return;

    // HONEY
    if (self.message != "" && other.classname != self.message)
        return;

// set XY even if not on ground, so the jump will clear lips
    other.velocity_x = self.movedir_x * self.speed;
    other.velocity_y = self.movedir_y * self.speed;

    if (!(other.flags & FL_ONGROUND)){
        return;
    }

    other.flags = other.flags - FL_ONGROUND;

    other.velocity_z = self.height;

    // HONEY
    if(self.spawnflags & PUSH_ONCE){
        if(self.delay){
            SUB_DoLater(SUB_Remove, self.delay);
        }else{
            SUB_Remove();
        }
    }

    // HIPNOTIC
    //MED 12/01/96 added count stuff
   if(self.cnt>0)
      {
      self.cnt = self.cnt - 1;
      if(self.cnt == 0)
         {
         self.touch = SUB_Null;
         self.nextthink = time + 0.1;
         self.think = SUB_Remove;
         }
      }
};

//MED 12/01/96 added count field
/*QUAKED trigger_monsterjump (.5 .5 .5) ?
Walking monsters that touch this will jump in the direction of the trigger's angle
"speed" default to 200, the speed thrown forward
"height" default to 200, the speed thrown upwards
"cnt" default infinite, how many times to trigger
*/
void() trigger_monsterjump =
{
    if(!self.speed)
        self.speed = 200;
    if(!self.height)
        self.height = 200;
    if(self.angles == '0 0 0')
        self.angles = '0 360 0';

    // HIPNOTIC
//MED 12/01/96 added count stuff
   if(self.cnt == 0)
      self.cnt = -1;
    InitTrigger();
    self.touch = trigger_monsterjump_touch;
};
