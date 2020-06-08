/*
================================================================

TRIGGERS

================================================================
*/

float	SPAWNFLAG_NOMESSAGE = 1;
float	SPAWNFLAG_NOTOUCH = 1;
float	SPAWNFLAG_SUPERSECRET = 2;
float	SPAWNFLAG_TRIGGER_FIRST = 4;
float	SPAWNFLAG_TRIGGER_TOGGLE = 4;
float	SPAWNFLAG_TRIGGER_MONSTERS = 8;

/*FGD
@baseclass = TriggerSounds
[
	sounds(choices) : "Sounds" : 0 =
	[
		0 : "None"
		1 : "secret"
		2 : "beep beep"
		3 : "large switch"
	]
	noise(string) : "Choose wav to play"
]
@baseclass base(AppearFlags, Target, Targetname) = Trigger 
[
	spawnflags(Flags) = [
		4 : "trigger first"
	]
	message(string) : "message to display when triggered"
]
@baseclass base(Trigger, TriggerSounds, Angle) = TriggerMulti
[
	health(integer) : "solid, must be killed to activate"
	delay(string) : "delay before firing (after trigger)" : "0"
	distance(string) : "dot product result to compare 'angle' against"
	spawnflags(Flags) = [
		1 : "notouch" : 0
		8 : "monsters will fire" : 0
	]
]
*/

void() trigger_reactivate =
{
	self.solid = SOLID_TRIGGER;
	self.use = self.think1;
	force_retouch = 2;		// make sure even still objects get hit
}

/*
================================
CheckValidTouch
health and playerhood checks were duplicated everywhere
added noclip check because Quake's default still-touch-everything noclip is awful
================================
*/
float() CheckValidTouch = 
{
	if (other.classname != "player")
		return FALSE;
	if (other.health <= 0)
		return FALSE;
	if (other.movetype == MOVETYPE_NOCLIP)
		return FALSE;
	return TRUE;
}


/*
================
InitTrigger
================
*/
void() InitTrigger =
{
// trigger angles are used for one-way touches.  An angle of 0 is assumed
// to mean no restrictions, so use a yaw of 360 instead.
	if (self.angles != '0 0 0')
		SetMovedir ();
	
	// do this first so setmodel links us correctly
	self.solid = SOLID_TRIGGER;
	setmodel (self, self.model);	// set size and link into world
	self.movetype = MOVETYPE_NONE;
	
	if (self.spawnflags & SPAWNFLAG_TRIGGER_FIRST)
	{
		self.solid = SOLID_NOT;
		self.think1 = self.use;
		self.use = trigger_reactivate;
	}

	self.modelindex = 0;
	self.model = string_null;
}

/*
================
InitTriggerBounds
quoth2-style edict-saving bounding box triggers
================
*/
void() InitTriggerBounds =
{
// trigger angles are used for one-way touches.  An angle of 0 is assumed
// to mean no restrictions, so use a yaw of 360 instead.

	// do this first so setsize links us correctly
	self.solid = SOLID_TRIGGER;
	setorigin (self, self.origin);
	
	setsize (self, self.mangle * -0.5, self.mangle * 0.5);
	self.mangle = '0 0 0';	// so setMoveDir doesn't think our mangle is our angle
	
	if (self.angles != '0 0 0')
		SetMovedir ();
	
	if (self.spawnflags & SPAWNFLAG_TRIGGER_FIRST)
	{
		self.solid = SOLID_NOT;
		self.think1 = self.use;
		self.use = trigger_reactivate;
	}
		
	self.movetype = MOVETYPE_NONE;
//	particle(self.origin - self.mangle, '0 0 0', 232, 4);
//	particle(self.origin + self.mangle, '0 0 0', 232, 4);
//	self.modelindex = 0;
//	self.model = string_null;
}

/*
================
InitTriggerSounds
================
*/
void() InitTriggerSounds =
{
	if (!self.noise)
	{
		if (self.sounds == 1)
			self.noise = "misc/secret.wav";
		else if (self.sounds == 2)
			self.noise = "misc/talk.wav";
		else if (self.sounds == 3)
			self.noise = "misc/trigger1.wav";
	}
	if (self.noise)
		precache_sound (self.noise);
}

//=============================================================================

// the wait time has passed, so set back up for another activation
void() multi_wait =
{
	if (self.max_health)
	{
		self.health = self.max_health;
		self.takedamage = DAMAGE_YES;
		self.solid = SOLID_BBOX;
	}
}

// the trigger was just touched/killed/used
// self.enemy should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
float() multi_trigger =
{
	if (self.nextthink > time)
		return FALSE;		// already been triggered

	if (self.customflags & CFL_LOCKED)
	{
		dprint("  trigger is locked: not firing\n");
		return FALSE;
	}
	
	activator = self.enemy;
	SUB_UseTargets();
	
	if (self.noise)
		sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);

	if (self.classname == "trigger_secret")
	{
		secret_use();
		return FALSE;
	}

// don't trigger again until reset
	self.takedamage = DAMAGE_NO;
	
	if (self.wait > 0)	
	{
		self.think = multi_wait;
		self.nextthink = time + self.wait;
	}
	else
	{	// we can't just remove (self) here, because this is a touch function
		// called while C code is looping through area links...
		self.touch = SUB_Null;
		SUB_RemoveSoon();
	}
	
	return TRUE;
}

// trigger_multiples are sometimes used as shootable buttons :(
// most notably the 'well of wishes' secret message in start.bsp
void() multi_killed =
{
	self.enemy = damage_attacker;
	if (multi_trigger() ) SUB_PrintMessage();
}

void() multi_use =
{
	self.enemy = activator;
	if (multi_trigger() ) SUB_PrintMessage();
}

void() multi_touch =
{
	if (!CheckValidTouch())
	{
		if (other.flags & FL_MONSTER && (other.enemy != world || other.goalentity != world))
		{
			if (!(self.spawnflags & SPAWNFLAG_TRIGGER_MONSTERS))
				return;
		}
		else
			return;
	}
	
	// if the trigger has an angles field, check player's facing direction
	if (self.movedir != '0 0 0')
	{
		makevectors (other.v_angle);
		if (v_forward * self.movedir < self.distance)
			return;		// not facing the right way
	}
	
	self.enemy = other;
	
	if (multi_trigger()) SUB_PrintMessage();
}


/*QUAKED trigger_multiple (.5 .0 .5) ? notouch ? TRIGGER_FIRST MONSTERS
Variable sized repeatable trigger.
Must be targeted at one or more entities.

Flags:
"notouch" only triggered by other entities, not by touching
"trigger_first" won't be touchable until triggered once by something else
"monsters" monsters will fire this trigger (and count as activator, so be careful)

Keys:
"angle" the trigger will only fire when someone is facing the direction of the angle, use 360 for angle 0.
"distance" dot product result to compare "angle" against. 0 is 90 degrees off (default), 0.7 is ~45 degrees.
"delay" delay before firing (after trigger)
"health" button must be killed to activate
"message" message to display when triggered
"sounds"
    1 = secret
    2 = beep beep
	3 = large switch
	4 = it is a mysteryyy
	-1 = silent
"noise"	choose your own .wav
"target"  entity to trigger (required)
"targetname"  entity name
"wait" delay between triggerings (default: 0.2)
*/
/*FGD
@SolidClass base(TriggerMulti) = trigger_multiple : "Trigger: Activate multiple" 
[
	wait(string) : "Wait before reset" : "0.2"
]
*/
void() trigger_multiple =
{
	if (!SUB_ShouldSpawn()) return;
	InitTriggerSounds();
	
	if (!self.wait)
		self.wait = 0.2;
	self.use = multi_use;

	InitTrigger ();

	if (self.health)
	{
		if (self.spawnflags & SPAWNFLAG_NOTOUCH)
			objerror ("health and notouch don't make sense\n");
		self.max_health = self.health;
		self.th_die = multi_killed;
		self.takedamage = DAMAGE_YES;
		self.solid = SOLID_BBOX;
		setorigin (self, self.origin);	// make sure it links into the world
	}
	else
	{
		if ( !(self.spawnflags & SPAWNFLAG_NOTOUCH) )
		{
			self.touch = multi_touch;
		}
	}
}


/*QUAKED trigger_multiple_box (.5 .5 0) (-8 -8 -8) (8 8 8) ? ? TRIGGER_FIRST MONSTERS
Quoth2-style cache-saving bounding box triggers: define size with "mangle", as total dimensions in xyz - trigger volume will be that size centered on the trigger_multiple_box's origin.  otherwise acts like a normal trigger.

Flags:
"notouch" only triggered by other entities, not by touching
"trigger_first" won't be touchable until triggered once by something else
"monsters" monsters will fire this trigger (and count as activator, so be careful)

Keys:
"angle" the trigger will only fire when someone is facing the direction of the angle, use "360" for angle 0.
"distance" dot product result to compare "angle" against. 0 is 90 degrees off (default), 0.7 is 45 degrees.
"delay" delay before firing (after trigger)
"health" button must be killed to activate
"message" message to display when triggered
"sounds"
    1 = secret
    2 = beep beep
	3 = large switch
	4 = it is a mysteryyy
	-1 = silent
"noise"	choose your own .wav
"target"  entity to trigger (required)
"targetname"  entity name
"wait" delay between triggerings (default: 0.2)
*/
/*FGD
@PointClass base(TriggerMulti, Bounds) = trigger_multiple_box : "Trigger: Activate multiple" 
[
	wait(string) : "Wait before reset" : "0.2"
]
*/
void() trigger_multiple_box =
{
	if (!SUB_ShouldSpawn()) return;
	InitTriggerSounds();
	
	if (!self.wait)
		self.wait = 0.2;
	self.use = multi_use;

	setorigin (self, self.origin);	// make sure it links into the world
	InitTriggerBounds ();

	if (self.health)
	{
		if (self.spawnflags & SPAWNFLAG_NOTOUCH)
			objerror ("health and notouch don't make sense\n");
		self.max_health = self.health;
		self.th_die = multi_killed;
		self.takedamage = DAMAGE_YES;
		self.solid = SOLID_BBOX;
	}
	else
	{
		if ( !(self.spawnflags & SPAWNFLAG_NOTOUCH) )
		{
			self.touch = multi_touch;
		}
	}
}


/*QUAKED trigger_once (.5 .0 .5) ? notouch ? TRIGGER_FIRST MONSTERS
Variable sized trigger. Triggers once, then removes itself. You must set the key "target" to the name of another object in the level that has a matching "targetname". If notouch is set, the trigger is only fired by other entities, not by touching.

Wait is always -1.

Flags:
"notouch" only triggered by other entities, not by touching
"trigger_first" won't be touchable until triggered once by something else
"monsters" monsters will fire this trigger (and count as activator, so be careful)

Keys:
"angle" the trigger will only fire when someone is facing the direction of the angle, use "360" for angle 0.
"distance" dot product result to compare "angle" against. 0 is 90 degrees off (default), 0.7 is 45 degrees.
"delay" delay before firing (after trigger)
"health" button must be killed to activate
"message" message to display when triggered
"sounds"
    1 = secret
    2 = beep beep
	3 = large switch
	4 = it is a mysteryyy
	-1 = silent
"noise"	choose your own .wav
"target"  entity to trigger (required)
"targetname"  entity name
*/
/*FGD
@SolidClass base(TriggerMulti) = trigger_once : "Trigger: Activate once" []
*/
void() trigger_once =
{
	self.wait = -1;
	trigger_multiple();
}

//=============================================================================



/*FGD
@baseclass = Secret
[
	spawnflags(flags) = [
		2 : "Supersecret" : 0
	]
	sounds(choices) : "Sounds" : 1 =
	[
		0 : "None"
		1 : "secret"
		2 : "beep beep"
		3 : "large switch"
	]
	message(string) : "Message"
]
*/
void() target_secret_use =
{
	if (self.noise)
		sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
	
	
	secret_use();
}

void() secret_use =
{
	if (activator.classname != "player")
		return;
	
	if (self.spawnflags & SPAWNFLAG_SUPERSECRET)
	{
		// super secrets aren't counted toward the total until found
		total_secrets = total_secrets + 1;
		WriteByte (MSG_ALL, SVC_UPDATESTAT);
		WriteByte (MSG_ALL, STAT_TOTALSECRETS);
		WriteLong (MSG_ALL, total_secrets);
	}
	
	found_secrets = found_secrets + 1;
	WriteByte (MSG_ALL, SVC_FOUNDSECRET);
	
	SUB_PrintMessage();
//	sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
	
	self.touch = SUB_Null;
	SUB_RemoveSoon();
}


void() trigger_secret_setup =
{
	if (!(self.spawnflags & SPAWNFLAG_SUPERSECRET))
	{
		// super secrets aren't counted toward the total until found
		total_secrets = total_secrets + 1;
	}
	self.spawnflags = not(self.spawnflags, SPAWNFLAG_TRIGGER_MONSTERS);	// jic of old stale spawnflags 8
	
	self.wait = -1;
	self.classname = "trigger_secret";
	if (self.message == string_null)
	{
		if (self.spawnflags & SPAWNFLAG_SUPERSECRET)
			self.message = "You found a \bvery\b secret area!";
		else
			self.message = "You found a secret area!";
	
	}
	if (!self.sounds)
		self.sounds = 1;
	
	InitTriggerSounds();
}

void() target_secret_check =
{
	// sanity check for dumb mappers like me
	entity w;
	w = find(world, target, self.targetname);
	if (w)
	{
		if (w.type == "ammo" || 
		//	( w.type == "armor" && w.classname != "item_armorpatch") || 
			( w.type == "health" && !(w.spawnflags & 2)) )
			objerror("target_secret targeted by something the player might not be able to grab\n");
	}
}

/*QUAKED target_secret (.5 .0 .5) (-8 -8 -8) (8 8 8) ? SUPERSECRET
Secret counter trigger.  Player gets a secret credit when triggered.  Note when using a target_secret: if the item can't be picked up (because of health/armor/ammo limits already being met, etc), it won't fire its targets and thus the target_secret won't give credit. Use a trigger in that case.

Set "SUPERSECRET" to hide its existence in the total until it's found.
Keys:
"message" message to display when triggered
"sounds"
    1 = secret
    2 = beep beep
	3 = large switch
	4 = it is a mysteryyy
	-1 = silent
"noise"	choose your own .wav
*/
/*FGD
@PointClass base(Secret) = target_secret : "Target : Secret
Counts as a secret, trigger to give credit" []
*/
void() target_secret =
{
	if (!SUB_ShouldSpawn()) return;
	if (deathmatch) { remove(self); return; }
	trigger_secret_setup();
	self.use = target_secret_use;
	
	self.think = target_secret_check;
	self.nextthink = time + 0.2;
}

/*QUAKED trigger_secret (.5 .0 .5) ? ? SUPERSECRET TRIGGER_FIRST
Secret counter trigger.  Player gets a secret credit when touched.  Set "SUPERSECRET" to hide its existence in the total until found.
Keys:
"message" message to display when triggered
"sounds"
    1 = secret
    2 = beep beep
	3 = large switch
	4 = it is a mysteryyy
	-1 = silent
"noise"	choose your own .wav

Flags:
"SUPERSECRET" hide its existence in the total until it's found
"trigger_first" won't be touchable until triggered once by something else
*/
/*FGD
@SolidClass base(Trigger, Secret) = trigger_secret : "Trigger : Secret
Counts as a secret, trigger to give credit" []
*/
void() trigger_secret =
{
	if (!SUB_ShouldSpawn()) return;
	if (deathmatch) { remove(self); return; }
	trigger_multiple ();
	trigger_secret_setup();
}

/*QUAKED trigger_secret_box (.5 .0 .5) (-8 -8 -8) (8 8 8) ? SUPERSECRET TRIGGER_FIRST
Secret counter trigger.  Player gets a secret credit when touched.  Set "SUPERSECRET" to hide its existence in the total until found.

Quoth2-style edict-saving bounding box trigger: define size with "mangle", trigger volume will be that size centered on the trigger_secret_box's origin.

Keys:
"message" message to display when triggered
"sounds"
    1 = secret
    2 = beep beep
	3 = large switch
	4 = it is a mysteryyy
	-1 = silent
"noise"	choose your own .wav

Flags:
"SUPERSECRET" hide its existence in the total until it's found
"trigger_first" won't be touchable until triggered once by something else
*/
/*FGD
@PointClass base(Trigger, Secret, Bounds) = target_secret : "Trigger : Secret
Counts as a secret, trigger to give credit" []
*/
void() trigger_secret_box =
{
	if (!SUB_ShouldSpawn()) return;
	if (deathmatch) { remove(self); return; }
	trigger_multiple_box ();
	trigger_secret_setup();
}


//=============================================================================



/*
==============================================================================
trigger_setskill & target_setskill

I have a trigger_ and a target_ for setskill and route both through one function,
so that the skill teleporters themselves in the start map can be the 'barrier'
that changes the skill. nightmare changes max health, and I want players to see 
it change on the hud the instant they make their 'selection' by entering the 
teleporter, not at the start of the next map.
==============================================================================
*/

void(float h) skill_adjust_health =
{
	entity p;
	p = find(world, classname, "player");
	while (p)
	{
		p.max_health = h;
		if (p.health > p.max_health)
		{
			p.deathtype = "noarmor";
			T_Damage(p, world, world, p.health - p.max_health);
			p.deathtype = "";
		}
		else if (p.health < p.max_health)
		{
			T_Heal(p, p.max_health - p.health, FALSE);
		}
		p = find(p, classname, "player");
	}
}

void(entity e, string sk) skill_set =
{
	if (sk == "0" || sk == "1" || sk == "2" || sk == "")
	{
		skill_adjust_health(100);
	}
	else if (sk == "3")
	{
		skill_adjust_health(50);
	}

	cvar_set ("skill", sk);
	skill = cvar("skill");
}

void() target_setskill_use
{
	skill_set(activator, self.message);
}

/*FGD
@baseclass = Setskill [
	message(choices) : "Skill" : 0 = [
		0 : "Easy"
		1 : "Normal"
		2 : "Hard"
		3 : "NIGHTMAAARE"
	]
]
*/

/*QUAKED target_setskill (.5 .0 .5) (-8 -8 -8) (8 8 8)
Sets skill level to the value of "message". Only used on start map.

Keys:
"message"
    sets skill level
    0 = easy
    1 = normal
    2 = hard
    3 = nightmare
*/
/*FGD
@PointClass base(Setskill, Targetname) = target_setskill : "Target: Set Skill" []
*/
void() target_setskill =
{
	self.use = target_setskill_use;
}

void() trigger_skill_touch =
{
	if (!CheckValidTouch()) return;
	if (self.customflags & CFL_LOCKED) return;
	
	skill_set(other, self.message);
}

/*QUAKED trigger_setskill (.5 .0 .5) ? ? ? TRIGGER_FIRST
Sets skill level to the value of "message". Only used on start map.

Flags:
"trigger_first" won't be touchable until triggered once by something else

Keys:
"message"
    sets skill level
    0 = easy
    1 = normal
    2 = hard
    3 = nightmare
	4 = helllll
*/
/*FGD
@PointClass base(Setskill, Trigger) = trigger_setskill : "Trigger: Set Skill" []
*/
void() trigger_setskill =
{
	if (!SUB_ShouldSpawn()) return;
	InitTrigger ();
	self.touch = trigger_skill_touch;
}


//============================================================================

void() trigger_onlyregistered_touch =
{
	if (!CheckValidTouch()) return;
	if (self.attack_finished > time)
		return;

	self.attack_finished = time + 2;
	if (cvar("registered"))
	{
		self.message = string_null;
		activator = other;
		SUB_UseTargets ();
		remove (self);
	}
	else
	{
		if (self.message != string_null)
		{
			centerprint (other, self.message);
			sound (other, CHAN_BODY, "misc/talk.wav", 1, ATTN_NORM);
		}
	}
}

/*QUAKED trigger_onlyregistered (.5 .5 .5) ?
Only fires if playing the registered version, otherwise prints the message
*/
/*FGD
@SolidClass base(Trigger) = trigger_onlyregistered : "Trigger: Registered only" []
*/
void() trigger_onlyregistered =
{
	precache_sound ("misc/talk.wav");
	InitTrigger ();
	self.touch = trigger_onlyregistered_touch;
}




//============================================================================

void() hurt_on =
{
	self.solid = SOLID_TRIGGER;
	self.nextthink = -1;
}

void() hurt_touch =
{
	if (!CheckValidTouch()) return;
	if (self.customflags & CFL_LOCKED) return;
	if (!other.takedamage)
		return;
	if ((self.spawnflags & 4) && (other.flags & FL_MONSTER))
		return;
	if ((self.spawnflags & 2) && (other.radsuit_finished > time))
		return;
	
	// fix for multiple coop clients touching this at once only hurting the first one
	if (time != self.dmgtime)
		if (time < self.attack_finished)
			return;
	
	if (self.spawnflags & 1)
	{
		T_Damage (other, self, self, other.health + min(other.armorvalue, other.health / other.armortype) + 5);
	}
	else
		T_Damage (other, self, self, self.dmg);
	
	self.dmgtime = time;
	self.attack_finished = time + self.delay;
}

/*FGD
@baseclass base(Trigger) = Hurt [
	spawnflags(flags) = [
		1 : "Kill" : 0
		2 : "Biosuit Immune" : 0
		8 : "Won't Hurt Monsters" : 0
	]
	dmg(integer) : "Damage" : 5
	delay(string) : "Interval" : "1"
]
*/

/*QUAKED trigger_hurt (.5 .0 .5) ? KILL NO_BIOSUIT TRIGGER_FIRST NO_MONSTERS
any entity touching this will be hurt / to see if it still feels

Keys:
"dmg" sets damage, default is 5
"delay" frequency of damage in seconds, default 1

Flags:
"kill" always do enough damage to kill instantly
"no_biosuit" the biosuit protects against this trigger
"trigger_first" won't be touchable until triggered once by something else
"no_monsters" trigger will only hurt players
*/
/*FGD
@SolidClass base(Hurt) = trigger_hurt : "Trigger: Hurt" []
*/
void() trigger_hurt =
{
	if (!SUB_ShouldSpawn()) return;
	InitTrigger ();
	self.touch = hurt_touch;
	if (!self.dmg)
		self.dmg = 5;
	if (!self.delay)
		self.delay = 1;
}

/*QUAKED trigger_hurt_box (.5 .0 .5) (-8 -8 -8) (8 8 8) ? ? TRIGGER_FIRST
Any object touching this will be hurt.
Quoth2-style edict-saving bounding box trigger: define bounds size with "mangle" - trigger volume will be centered on the trigger_hurt_box's origin.
Resist the urge to put a tiny one of these on every light_flame.

Keys:
"dmg" sets damage, default is 5
"delay" frequency of damage in seconds, default 1

Flags:
"trigger_first" won't be touchable until triggered once by something else
*/
/*FGD
@PointClass base(Hurt,Bounds) = trigger_hurt_box : "Trigger: Hurt bounds" []
*/
void() trigger_hurt_box =
{
	if (!SUB_ShouldSpawn()) return;
	InitTriggerBounds ();
	self.touch = hurt_touch;
	if (!self.dmg)
		self.dmg = 5;
	if (!self.delay)
		self.delay = 1;
	self.classname = "trigger_hurt";
}



//============================================================================

float PUSH_ONCE = 1;
float PUSH_ADD_VEL = 2;

void() trigger_push_touch =
{
	if (other.movetype == MOVETYPE_NOCLIP)
		return;
	if (self.customflags & CFL_LOCKED)
		return;
	else if (other.health > 0 || other.classname == "grenade")
	{
		if (self.spawnflags & PUSH_ADD_VEL)
			other.velocity += self.speed * self.movedir;
		else
			other.velocity = self.speed * self.movedir * 10;
		if (other.classname == "player")
		{
			if (other.nextmovesound < time)
			{
				other.nextmovesound = time + 1.5;
				sound (other, CHAN_AUTO, "ambience/windfly.wav", 1, ATTN_NORM);
			}
		}
	}
	if (self.spawnflags & PUSH_ONCE)
		remove(self);
}


/*QUAKED trigger_push (.5 .0 .5) ? PUSH_ONCE ADD_VEL TRIGGER_FIRST
Pushes the player and Grenades.

Flags:
"push_once" removes itself after firing
"trigger_first" won't be touchable until triggered once by something else

Keys:
"angle" direction of push (-2 is down, -1 up)
"speed" speed of push (default: 1000)
*/
/*FGD
@SolidClass base(Angle, Trigger) = trigger_push : "Trigger: Push"
[
	spawnflags(flags) = [ 1: "Push once" : 0 ]
	speed(integer) : "Speed" : 1000
]
*/
void() trigger_push =
{
	if (!SUB_ShouldSpawn()) return;
	if (self.angles == '0 0 0')
		self.angles = '0 360 0';
	
	InitTrigger ();
	precache_sound ("ambience/windfly.wav");
	self.touch = trigger_push_touch;
	if (!self.speed)
		self.speed = 1000;
}


//============================================================================

float SPAWN_MJUMP_NOLARGE = 1;
float SPAWN_MJUMP_NOSMALL = 2;
float SPAWN_MJUMP_MELEES = 8;
float SPAWN_MJUMP_ONLYFRONT = 16;
float SPAWN_MJUMP_ONLYBELOW = 32;
float SPAWN_MJUMP_ADDVEL = 64;

void() trigger_monsterjump_touch =
{
	vector dir;
	
	if (self.customflags & CFL_LOCKED) return;
	if ( other.flags & (FL_MONSTER | FL_FLY | FL_SWIM) != FL_MONSTER )
		return;
	if (time < self.attack_finished)
		return;
	
	// filtration
	if (other.classname != self.include)	// always allow this classname
	{
		if (self.spawnflags & SPAWN_MJUMP_NOLARGE && other.maxs_x == 32) return;
		if (self.spawnflags & SPAWN_MJUMP_NOSMALL && other.maxs_x == 16) return;
		if (self.spawnflags & SPAWN_MJUMP_MELEES && !(other.customflags & CFL_MELEEONLY)) return;
		
		if (self.exclude == other.classname) return;	// always exclude this classname
	}
	if (self.spawnflags & SPAWN_MJUMP_ONLYBELOW && (other.enemy.absmin_z) >= (other.absmin_z)) return;
	if (self.spawnflags & SPAWN_MJUMP_ONLYFRONT)
	{
		dir = other.enemy.origin - other.origin;
		dir_z = 0;
		dir = normalize(dir);
		if (dir * self.movedir < self.distance)
			return;
	}
	
	self.attack_finished = time + self.wait;
	// set XY even if not on ground, so the jump will clear lips
	if (self.spawnflags & SPAWN_MJUMP_ADDVEL)
	{
		other.velocity_x += self.movedir_x * self.speed;
		other.velocity_y += self.movedir_y * self.speed;
	}
	else
	{
		other.velocity_x = self.movedir_x * self.speed;
		other.velocity_y = self.movedir_y * self.speed;
	}
	
	if ( !(other.flags & FL_ONGROUND) )
	{
		if (self.spawnflags & SPAWN_MJUMP_ADDVEL)
			other.velocity_z += self.height;
		else
			return;
	}
	
	other.flags = not(other.flags, FL_ONGROUND);
	other.velocity_z = self.height;
}

void() trigger_monsterjump_init =
{
	self.speed = zeroconvertdefault(self.speed, 200);
	self.height = zeroconvertdefault(self.height, 200);
	if (!self.distance)
		self.distance = 0.5;
	if (self.angles == '0 0 0')
		self.angles = '0 360 0';
	self.touch = trigger_monsterjump_touch;
}

/*QUAKED trigger_monsterjump (.5 .0 .5) ? NO_LARGE NO_SMALL TRIGGER_FIRST ONLY_MELEES ONLY_FRONT ONLY_BELOW ADD_VEL
Walking monsters that touch this will jump in the direction of the trigger's angle.

Flags:
NO_LARGE only monsters with 32x32 bounds
NO_SMALL only monsters with 64x64 bounds
TRIGGER_FIRST won't be touchable until triggered once by something else
ONLY_MELEES only monsters with no ranged attack (dogs/fiends/spawns)
ONLY_FRONT will only trigger if monster's enemy is in front of the monster relative to jump angle
ONLY_BELOW will only trigger if monster's enemy is below the monster
ADD_VEL add velocity to monster rather than setting it

Keys:
"angle" angle towards the monster jumps
"height" the speed thrown upwards (default: 200)
"speed" the speed thrown forward (default: 200)
"target"  entity to trigger
"targetname"  entity name
"include" allow this classname of monster no matter what
"exclude" exclude this classname of monster no matter what
*/
/*FGD
@baseclass base(Trigger, Angle) = Monsterjump [
	spawnflags(flags) = [
		1 : "No large monsters" : 0
		2 : "No small monsters" : 0
		8 : "Only melee monsters" : 0
		16 : "Only if target in front" : 0
		32 : "Only if target below" : 0
		64 : "Additive push" : 0
	]
	speed(integer) : "Jump Speed" : 200
	height(integer) : "Jump Height" : 200
	include(string) : "allow this classname of monster no matter what"
	exclude(string) : "exclude this classname of monster no matter what"
]
@SolidClass base(Monsterjump) = trigger_monsterjump : "Trigger: Monster jump" []
*/
void() trigger_monsterjump =
{
	if (!SUB_ShouldSpawn()) return;
	trigger_monsterjump_init();
	InitTrigger();
}


/*QUAKED trigger_monsterjump_box (.5 .0 .5) (-8 -8 -8) (8 8 8) NO_LARGE NO_SMALL TRIGGER_FIRST ONLY_MELEES ONLY_FRONT ONLY_BELOW ADD_VEL
Walking monsters that touch this will jump in the direction of the trigger's angle.
Quoth2-style edict-saving bounding box trigger: define bounds size with "mangle" - trigger volume will be centered on the trigger_monsterjump_box's origin.

Flags:
NO_LARGE only monsters with 32x32 bounds
NO_SMALL only monsters with 64x64 bounds
TRIGGER_FIRST won't be touchable until triggered once by something else
ONLY_MELEES only monsters with no ranged attack (dogs/fiends/spawns)
ONLY_FRONT will only trigger if monster's enemy is in front of the monster relative to jump angle
ONLY_BELOW will only trigger if monster's enemy is below the monster
ADD_VEL add velocity to monster rather than setting it

Keys:
"angle" angle towards the monster jumps
"height" the speed thrown upwards (default: 200)
"speed" the speed thrown forward (default: 200)
"target"  entity to trigger
"targetname"  entity name
"include" allow this classname of monster no matter what
"exclude" exclude this classname of monster no matter what
*/
/*FGD
@PointClass base(Monsterjump, Bounds) = trigger_monsterjump_box : "Trigger: Monster jump bounds" []
*/
void() trigger_monsterjump_box =
{
	if (!SUB_ShouldSpawn()) return;
	trigger_monsterjump_init();
	InitTriggerBounds();
	self.classname = "trigger_monsterjump";
}

