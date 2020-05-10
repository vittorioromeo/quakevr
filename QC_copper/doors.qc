/*
=============================================================================
Doors are similar to buttons, but can spawn a fat trigger field around them
to open without a touch, and they link together to form simultaneous
double/quad doors.
 
Door.owner is the master door.  If there is only one door, it points to itself.
If multiple doors, all will point to a single one.

Door.enemy chains from the master door through all doors linked in the chain.
=============================================================================
*/

float DOOR_START_OPEN = 1;
float DOOR_SPECIAL = 2;
float DOOR_DONT_LINK = 4;
float DOOR_GOLD_KEY = 8;
float DOOR_SILVER_KEY = 16;
float DOOR_TOGGLE = 32;
float DOOR_FLIP_SKIN = 64;

/*
=============================================================================

THINK FUNCTIONS

=============================================================================
*/

void() door_blocked =
{
	T_Damage (other, self, self, self.dmg);
	
	// if a door has a negative wait, it would never come back if blocked,
	// so let it just squash the object to death real fast
	if (self.wait >= 0)
	{
		if (self.state == STATE_DOWN)
			door_go_up ();
		else
			door_go_down ();
	}
}


void() door_hit_top =
{
	sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
	self.state = STATE_TOP;
	if (self.spawnflags & DOOR_TOGGLE)
		return;		// don't come down automatically
	self.think = door_go_down;
	self.nextthink = self.ltime + self.wait;
}

void() door_hit_bottom =
{
	sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
	self.state = STATE_BOTTOM;
}

void() door_go_down =
{
	sound (self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
	if (self.max_health)
	{
		self.takedamage = DAMAGE_YES;
		self.health = self.max_health;
	}
	
	self.state = STATE_DOWN;
	self.frame = !self.frame;
	SUB_CalcMove (self.pos1, self.speed2, door_hit_bottom);
}

void() door_go_up =
{
	if (self.state == STATE_UP)
		return;		// already going up

	if (self.state == STATE_TOP)
	{	// reset top wait time
		self.nextthink = self.ltime + self.wait;
		return;
	}
	
	sound (self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
	self.state = STATE_UP;
	SUB_CalcMove (self.pos2, self.speed, door_hit_top);
	self.frame = !self.frame;

	SUB_UseTargets();
}

/*
=============================================================================

ACTIVATION FUNCTIONS

=============================================================================
*/

void() door_fire =
{
	local entity 	oself;
	local entity	starte;
		
	if (self.owner != self)
		objerror ("door_fire: self.owner != self");

	//dprint("door firing\n");
		
	// play use key sound
	if (self.items)
		sound (self, CHAN_BODY, self.noise4, 1, ATTN_NORM);	// was chan_voice, door move sound was cutting it off

	if (!( self.spawnflags & DOOR_TOGGLE ) ) 
	{
		self.message = string_null;		// no more message
	}
	else
	{
		oself = self;
		if (self.state == STATE_TOP || self.state == STATE_UP )
		{
			starte = self;
			do
			{
				door_go_down ();
				self = self.enemy;
			} while ( (self != starte) && (self != world) );
			self = oself;
			return;
		}
	}
	
// trigger all paired doors
	starte = self;
	if ( !( self.spawnflags & DOOR_TOGGLE ) || ( self.spawnflags & DOOR_TOGGLE && self.state == STATE_BOTTOM) )
	do {
		door_go_up ();
		self = self.enemy;
	} while ( (self != starte) && (self != world) );
	self = oself;
}


void() door_use =
{
	//local entity 	oself;
	//local entity	starte;
	
	// trigger all paired doors
	/*
	oself = self;
	starte = self;
	if (self.spawnflags & DOOR_FLIP_SKIN) do {
		self.frame = !self.frame;
		self = self.enemy;
	} while ( (self != starte) && (self != world) );
	self = oself;
	*/
	if (!( self.spawnflags & DOOR_TOGGLE ) ) 
	{
		self.message = string_null;			// door message are for touch only
		self.owner.message = string_null;	
		self.enemy.message = string_null;
	}
	SUB_CallAsSelf(door_fire,self.owner);
}

void() door_trigger_touch =
{
	if (other.health <= 0) return;
	if (other.movetype == MOVETYPE_NOCLIP) return;
	if (time < self.attack_finished) return;
		
	// silly fix for sleeping monsters standing behind doors inside their touch fields causing them to open
	if (other.flags & FL_MONSTER && other.enemy == world && other.goalentity == world) return;
	
	self.attack_finished = time + 1;
	activator = other;
	self = self.owner;
	door_use ();
}

void() door_killed =
{
	local entity oself;
	
	oself = self;
	self = self.owner;
	self.health = self.max_health;
	self.takedamage = DAMAGE_NO;	// wil be reset upon return
	door_use ();
	self = oself;
}

void(entity d) door_force =
{
	SUB_CallAsSelf(door_fire,d.owner);
}

/*
================
door_touch

Prints messages and opens key doors
================
*/
void() door_touch =
{
	if (!CheckValidTouch()) return;

	if (self.owner.attack_finished > time)
		return;

	self.owner.attack_finished = time + 2;

	if (self.owner.message != string_null && self.owner.state == STATE_BOTTOM && !(self.owner.items))
	{
		centerprint (other, self.owner.message);
		sound (other, CHAN_VOICE, "doors/locked4.wav", 1, ATTN_NORM);
	}
	
	// from here on is key door stuff
	if (!self.items)
		return;
	// key doors don't spawn trigger fields, so door_touch does a lot of what door_trigger_touch does

	if ( (self.items & other.items) != self.items || self.customflags & CFL_LOCKED )
	{
		centerprint (other, self.owner.message);
		sound (self, CHAN_VOICE, self.noise3, 1, ATTN_NORM);
		return;
	}
	
	if (self.items & IT_KEY1)
	{
		key_take_silver(other);
	}
	else if (self.items & IT_KEY2)
	{
		key_take_gold(other);
	}
	else
	{
		other.items = other.items - self.items;
	}
	
	self.touch = SUB_Null;
	if (self.enemy)
		self.enemy.touch = SUB_Null;	// get paired door
		
	activator = other;
	
	door_use ();
}


void(entity d) door_lock =
{
	entity e;
	d.customflags = d.customflags | CFL_LOCKED;
	
//	d.owner.touch = SUB_Null;
//	d.owner.use = SUB_Null;
	if (d.owner.trigger_field == world && !d.owner.items) objerror("door being locked doesn't have a trigger");
	else if (d.owner.trigger_field)
		d.owner.trigger_field.touch = SUB_Null;

	// close any open doors immediately
	if (d.state != STATE_BOTTOM)
	{
		e = d;
		do {
			SUB_CallAsSelf(door_go_down,d);
			e = e.enemy;
		}
		while ( e != d && e != world );
	}	
}

void(entity d) door_unlock =
{
	d.customflags = not(d.customflags, CFL_LOCKED);
	
//	d.owner.touch = door_touch;
//	d.owner.use = door_use;
	if (d.owner.trigger_field == world && !d.owner.items) objerror("door being unlocked doesn't have a trigger");
	else if (d.owner.trigger_field)
		d.owner.trigger_field.touch = door_trigger_touch;
}

float(entity d) door_getslocked =
{
	if (d.targetname == string_null)
		return FALSE;
	
	entity t;
	t = find(world, target, d.targetname);
	while (t) {
		if (t.classname == "target_lock") return TRUE;
		t = find(t, target, d.targetname);
	}
	t = find(world, target2, d.targetname);
	while (t) {
		if (t.classname == "target_lock") return TRUE;
		t = find(t, target2, d.targetname);
	}
	t = find(world, target3, d.targetname);
	while (t) {
		if (t.classname == "target_lock") return TRUE;
		t = find(t, target3, d.targetname);
	}
	t = find(world, target4, d.targetname);
	while (t) {
		if (t.classname == "target_lock") return TRUE;
		t = find(t, target4, d.targetname);
	}
	return FALSE;
}

/*
=============================================================================

SPAWNING FUNCTIONS

=============================================================================
*/

entity(vector fmins, vector fmaxs) door_spawn_field =
{
	local entity	trigger;
	local	vector	t1, t2;

	trigger = spawn();
	trigger.movetype = MOVETYPE_NONE;
	trigger.solid = SOLID_TRIGGER;
	trigger.owner = self;
	trigger.touch = door_trigger_touch;

	t1 = fmins;
	t2 = fmaxs;
	setsize (trigger, t1 - '60 60 8', t2 + '60 60 8');
	return (trigger);
}

void(vector cmins, vector cmaxs) door_finalize =
{
	entity t;
	// switch positions after linking so START_OPEN doors link properly
	t = self;
	do {
		// DOOR_START_OPEN is to allow an entity to be lighted in the closed position
		// but spawn in the open position
		if (t.spawnflags & DOOR_START_OPEN)
		{
			setorigin (t, t.pos2);
			t.pos2 = t.pos1;
			t.pos1 = t.origin;
		}
		t = t.enemy;
	} while (t != self);
			
	// shootable, fired, or key doors just needed the owner/enemy links,
	// they don't spawn a field
	if (self.items)
	{
		t = self;
		do {
			//self.items = t.items;
			door_key_init();
			self = self.enemy;
		} while (t != self);
		return;
	}
	if (self.health)
		return;
	if (self.targetname != string_null)
		if (!door_getslocked(self))
			return;

	self.trigger_field = door_spawn_field(cmins, cmaxs);
}

/*
=============
door_link
=============
*/
void() door_link =
{
	local entity	t, masterdoor;
	local vector	cmins, cmaxs;

	if (self.enemy)
		return;		// already linked by another door
	if (self.spawnflags & DOOR_DONT_LINK)
	{
		self.owner = self.enemy = self;
		door_finalize(self.mins, self.maxs);
		return;		// don't want to link this door
	}

	cmins = self.mins;
	cmaxs = self.maxs;
	
	masterdoor = self;
	t = self;
	
	do
	{
		self.owner = masterdoor;

		if (self != masterdoor)
		{
			if (self.health)
				masterdoor.health = self.health;
			if (self.targetname != string_null)
			{
				masterdoor.targetname = self.targetname;
				self.targetname = string_null;
				// all the doors in the chain having the same targetname will cause
				// the whole chain to be triggered once for every door in the chain
			}
			SUB_MergeTargets(masterdoor);	// same deal
			self.target = string_null;
			self.target2 = string_null;
			self.target3 = string_null;
			self.target4 = string_null;
			self.killtarget = string_null;
			if (self.message != string_null)
				masterdoor.message = self.message;
			if (self.items)
			{
				masterdoor.items |= self.items;
			}
		}
		
		t = find (t, classname, self.classname);	
		if (!t)
		{
			self.enemy = masterdoor;		// make the chain a loop
			self = self.owner;

			door_finalize(cmins, cmaxs);
			
			return;
		}

		if (EntitiesTouching(self,t) && !(t.spawnflags & DOOR_DONT_LINK))
		{
			if (t.enemy)
				objerror ("cross connected doors");
			
			self.enemy = t;
			self = t;

			cmins_x = min(t.mins_x, cmins_x);
			cmins_y = min(t.mins_y, cmins_y);
			cmins_z = min(t.mins_z, cmins_z);
			cmaxs_x = max(t.maxs_x, cmaxs_x);
			cmaxs_y = max(t.maxs_y, cmaxs_y);
			cmaxs_z = max(t.maxs_z, cmaxs_z);
		}
	} while (1);
}

void() LinkDoors = {door_link();}

void() door_key_init =
{
	if (self.message == string_null)
	{
		if (self.items & IT_KEY1)
		{
			if (world.worldtype == 2)
				self.message = "You need the silver keycard";
			else if (world.worldtype == 1)
				self.message = "You need the silver runekey";
			else if (world.worldtype == 0)
				self.message = "You need the silver key";
		}
		else if (self.items & IT_KEY2)
		{
			if (world.worldtype == 2)
				self.message = "You need the gold keycard";
			else if (world.worldtype == 1)
				self.message = "You need the gold runekey";			
			else if (world.worldtype == 0)
				self.message = "You need the gold key";
		}
	}
	if (self.items)
		self.wait = -1;
}

/*QUAKED func_door (0 .5 .8) ? START_OPEN ? DONT_LINK GOLD_KEY SILVER_KEY TOGGLE CHANGE_SKIN
Door. Opens; closes. If two doors touch, they are assumed to be connected and operate as a unit. Targetnames, spawnflags, and items are safely shared between linked doors automatically.

SPAWNFLAGS
"start_open" causes the door to move to its destination when spawned, and operate in reverse. It is used to temporarily or permanently close off an area when triggered (not usefull for touch or takedamage doors).
"dont_link" don't link to any touching doors
"toggle" causes the door to wait in both the start and end states for a trigger event.
"change_skin" causes the door to change textures when activated

Key doors are always wait -1.

"message"	is printed when the door is touched if it is a trigger door and it hasn't been fired yet.  if locked, will print this instead of default silver/gold message.
"angle"		determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger activates the door, unless the door is only targeted by a target_lock.
"health"	if set, door must be shot open
"speed"		movement speed (100 default)
"speed2"	make close speed different from open speed
"wait"		wait before returning (3 default, -1 = never return)
"lip"		lip remaining at end of move (8 default)
"distance"	specify movement distance directly, overrides lip
"dmg"		damage to inflict when blocked (2 default)
"sounds"
0) no sound, or override with noise1/noise2
1) stone
2) base
3) stone chain
4) hissy metal
5) winchy secret door
6) base secret door
*/
/*FGD
@SolidClass base(Angle, Appearflags, Targetname, Target) = func_door : 
"Basic door. Opens; closes. 
If two doors touch, they are assumed to be connected and operate as a unit. Targetnames, spawnflags, and items are safely shared between linked doors automatically." 
[
	speed(integer) : "Speed" : 100
	speed2(integer) : "Close Speed (if different from open speed)" : 0
	sounds(choices) : "Sound" : 0 = 
	[
        0: "Silent"
		1: "Stone"
		2: "Machine"
		3: "Stone Chain"
		4: "Screechy Metal"
		5: "Winchy secret door"
		6: "Base secret door"
	]
	noise1(string) : "Override move sound (requires sounds 0)"
	noise2(string) : "Override stop sound (requires sounds 0)"
	wait(string) : "Wait before close" : "3" 
	lip(integer) : "Lip remaining at end of move" : 8
	distance(string) : "Distance to travel (overrides Lip)" : "0.0"
	dmg(integer) : "Damage inflicted when blocked" : 2
	message(string) : "Message if touched. Overrides key messages if locked."
	health(integer) : "Health (shootable)" : 0
	spawnflags(flags) =
	[
		1 : "Starts Open" : 0
		4 : "Don't link with touching doors" : 0
		8 : "Gold Key required" : 0
        16: "Silver Key required" : 0
        32: "Toggle" : 0
        64: "Change +0/+a skin" : 0
	]
]
*/
void() func_door =
{
	if (!SUB_ShouldSpawn()) return;
	
	if (world.worldtype == 1)
	{
		precache_sound ("doors/runetry.wav");
		precache_sound ("doors/runeuse.wav");
		self.noise3 = "doors/runetry.wav";
		self.noise4 = "doors/runeuse.wav";
	}
	else if (world.worldtype == 2)
	{
		precache_sound ("doors/basetry.wav");
		precache_sound ("doors/baseuse.wav");
		self.noise3 = "doors/basetry.wav";
		self.noise4 = "doors/baseuse.wav";
	}
	else
	{
		precache_sound ("doors/medtry.wav");
		precache_sound ("doors/meduse.wav");
		self.noise3 = "doors/medtry.wav";
		self.noise4 = "doors/meduse.wav";
	} 
	
	if (self.sounds == 0)
	{
		if (!self.noise1)
			self.noise1 = ("misc/null.wav");
		if (!self.noise2)
			self.noise2 = ("misc/null.wav");
		precache_sound(self.noise1);
		precache_sound(self.noise2);
	}
	if (self.sounds == 1)
	{
		precache_sound ("doors/drclos4.wav");
		precache_sound ("doors/doormv1.wav");
		self.noise1 = "doors/drclos4.wav";
		self.noise2 = "doors/doormv1.wav";
	}
	if (self.sounds == 2)
	{
		precache_sound ("doors/hydro1.wav");
		precache_sound ("doors/hydro2.wav");
		self.noise2 = "doors/hydro1.wav";
		self.noise1 = "doors/hydro2.wav";
	}
	if (self.sounds == 3)
	{
		precache_sound ("doors/stndr1.wav");
		precache_sound ("doors/stndr2.wav");
		self.noise2 = "doors/stndr1.wav";
		self.noise1 = "doors/stndr2.wav";
	}
	if (self.sounds == 4)
	{
		precache_sound ("doors/ddoor1.wav");
		precache_sound ("doors/ddoor2.wav");
		self.noise1 = "doors/ddoor2.wav";
		self.noise2 = "doors/ddoor1.wav";
	}
	if (self.sounds == 5)
	{
		precache_sound ("doors/winch2.wav");
		precache_sound ("doors/drclos4.wav");
		self.noise2 = "doors/winch2.wav";
		self.noise1 = "doors/drclos4.wav";
	}
	if (self.sounds == 6)
	{
		precache_sound ("doors/basesec1.wav");
		precache_sound ("doors/basesec2.wav");
		self.noise2 = "doors/basesec1.wav";
		self.noise1 = "doors/basesec2.wav";
	}
	precache_sound3 ("doors/locked4.wav");

	SetMovedir ();

	self.max_health = self.health;
	self.solid = SOLID_BSP;
	self.movetype = MOVETYPE_PUSH;
	setorigin (self, self.origin);	
	setmodel (self, self.model);
	//self.classname = "door";

	self.blocked = door_blocked;
	self.use = door_use;
	
	if (!self.speed)
		self.speed = 100;
	if (!self.speed2)
		self.speed2 = self.speed;
	if (!self.wait)
		self.wait = 3;
	self.dmg = zeroconvertdefault(self.dmg, 2);
	if (!self.lip)
		self.lip = 8;
	if (!self.distance)
		self.distance = BoundsAngleSize( self.movedir, self.size ) - self.lip;
		// ^^ super-pejankery for non-cardinal angles

	if (self.spawnflags & DOOR_SILVER_KEY) self.items = IT_KEY1;
	else if (self.spawnflags & DOOR_GOLD_KEY) self.items = IT_KEY2;
	//door_key_init();
	
	if (self.pos1 && self.pos2)
	{
		setorigin(self, self.pos1);
	}
	else
	{
		self.pos1 = self.origin;
		self.pos2 = self.pos1 + self.movedir * self.distance;
	}

	self.state = STATE_BOTTOM;

	if (self.health)
	{
		self.takedamage = DAMAGE_YES;
		self.th_die = door_killed;
	}
	
	self.touch = door_touch;

	// door_link can't be done until all of the doors have been spawned, so
	// the sizes can be detected properly.
	self.think = door_link;
	self.nextthink = self.ltime + 0.1;
}

/*
=============================================================================

SECRET DOORS

=============================================================================
*/

void() secretdoor_move1;
void() secretdoor_move2;
void() secretdoor_move3;
void() secretdoor_move4;
void() secretdoor_move5;
void() secretdoor_move6;
void() secretdoor_done;

float SECRET_OPEN_ONCE = 1;		// stays open
float SECRET_1ST_LEFT = 2;		// 1st move is left of angle
float SECRET_1ST_DOWN = 4;		// 1st move is down from angle
float SECRET_NO_SHOOT = 8;		// only opened by trigger
float SECRET_YES_SHOOT = 16;	// shootable even if targeted

void () secretdoor_use =
{
	self.health = self.max_health;

	// enter correct phase of opening if retriggered while moving
	if (self.origin != self.oldorigin)
	{
		secret_door_force(self);
		return;
	}
	
	self.message = string_null;		// no more message

	SUB_UseTargets();				// fire all targets / killtargets
	
	if (!(self.spawnflags & SECRET_NO_SHOOT))
	{
		// self.th_pain = SUB_Null;
		self.takedamage = DAMAGE_NO;
	}
	self.velocity = '0 0 0';

	secretdoor_move0();
}

// hodor correctly depending on what state the door is in
void(entity door) secret_door_force =
{
	entity oself;
	oself = self;
	self = door;
	if (self.think == secretdoor_move4)	// waiting in open pos, extend timer
	{
		self.nextthink = self.ltime + self.wait;
	}
	else if (self.think1 == secretdoor_move5)	// 2nd reverse move in progress or done
	{
		secretdoor_move2();	// restart 2nd open move
	}
	else if (self.think == secretdoor_move6 || self.origin == self.oldorigin)	// 1st reverse move in progress or done
	{
		secretdoor_move0();	// restart 1st open move
	}
	self = oself;
}

void(entity attacker, float damage) secretdoor_pain =
{
	if (attacker.classname != "player") return;	// no monsters opening secrets for you
	
	secretdoor_use();
}

// Make a sound, wait a little...
void () secretdoor_move0 = 
{
	sound(self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
	self.nextthink = self.ltime + 0.1;

	SUB_CalcMove(self.dest1, self.speed, secretdoor_move1);
	sound(self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
}

// Wait after first movement...
void () secretdoor_move1 = 
{
	self.nextthink = self.ltime + 1.0;
	self.think = secretdoor_move2;
	sound(self, CHAN_VOICE, self.noise3, 1, ATTN_NORM);
}

// Start moving sideways w/sound...
void () secretdoor_move2 =
{
	sound(self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
	SUB_CalcMove(self.dest2, self.speed, secretdoor_move3);
}

// Wait here until time to go back...
void () secretdoor_move3 =
{
	sound(self, CHAN_VOICE, self.noise3, 1, ATTN_NORM);
	if (!(self.spawnflags & SECRET_OPEN_ONCE) || self.wait > 0)
	{
		self.nextthink = self.ltime + self.wait;
		self.think = secretdoor_move4;
		return;
	}
	self.think = SUB_Null;
}

// Move backward...
void () secretdoor_move4 =
{
	self.think = SUB_Null;
	sound(self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
	SUB_CalcMove(self.dest1, self.speed, secretdoor_move5);		
}

// Wait 1 second...
void () secretdoor_move5 = 
{
	self.nextthink = self.ltime + 1.0;
	self.think = secretdoor_move6;
	sound(self, CHAN_VOICE, self.noise3, 1, ATTN_NORM);
}

// last move ...
void () secretdoor_move6 =
{
	sound(self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
	SUB_CalcMove(self.oldorigin, self.speed, secretdoor_done);
}

void () secretdoor_done =
{
	self.think1 = SUB_Null;
	if (!self.targetname || self.spawnflags & SECRET_YES_SHOOT)
	{
		self.health = self.max_health;
		self.takedamage = DAMAGE_YES;
		self.th_pain = secretdoor_pain;	
	}
	sound(self, CHAN_VOICE, self.noise3, 1, ATTN_NORM);
}

void () secret_blocked =
{
	if (time < self.attack_finished)
		return;
	self.attack_finished = time + 0.5;
	T_Damage (other, self, self, self.dmg);
}

/*
================
secret_touch

Prints messages
================
*/
void() secret_touch =
{
	if (!CheckValidTouch()) return;

	if (self.attack_finished > time)
		return;

	self.attack_finished = time + 2;
	
	if (self.message != string_null)
	{
		centerprint (other, self.message);
		sound (other, CHAN_BODY, "doors/locked4.wav", 1, ATTN_NORM);
	}
}


/*QUAKED func_door_secret (0 .5 .8) ? open_once 1st_left 1st_down no_shoot always_shoot
Basic secret door. Slides back, then to the side. Angle determines direction of second move.  First move is v_right unless inhibited.

Flags:
"open_once" stays open
"1st_left" First move is opposite of v_right, then toward "angle"
"1st_down" First move is down, then toward "angle"
"no_shoot" only opened by trigger
"always_shoot" even if targeted, keep shootable

Keys:
"dmg" damage to inflict when blocked (default: 2). If a secret door has a targetname, it will only be opened by it's button or trigger, not by damage.
"sounds"
    1 = medieval
    2 = metal
    3 = base
"t_width" override distance for first move
"t_length" override distance for second move
"dest1"+"dest2" if both set, uses dest1 and dest2 for first and second move vectors, ignoring 1st_whatever spawnflags. still needs width and length.
"wait"  # of seconds before coming back, default 5
*/
/*FGD
@SolidClass base(Appearflags, Targetname, Target) = func_door_secret : 
"Secret door. Slides back, then to the side. 
Angle determines direction of second move. First move is to the right of 'angle'." 
[
	angle(integer) : "Direction of second move"
	t_width(integer) : "First move length"
	t_length(integer) : "Second move length"
	dmg(integer) : "Damage when blocked" : 2
	wait(string) : "Wait before close" : "2"
	dest1(string) : "Force vector for first move (requires dest2)"
	dest2(string) : "Force vector for second move (requires dest1)"
	sounds(choices) : "Sounds" : 3 =
	[
		1: "Medieval"
		2: "Metal"
		3: "Base"
	]
	message(string) : "Message"
	spawnflags(flags) = 
	[
		1 : "Open once" : 0
		2 : "Move left first" : 0
		4 : "Move down first" : 0
		8 : "Not shootable" : 0
        16 : "Always shootable" : 0
	]
]
*/
void () func_door_secret =
{
	if (!SUB_ShouldSpawn()) return;
	self.sounds = zeroconvertdefault(self.sounds, 3);
	
	if (self.sounds == 1)
	{
		precache_sound ("doors/latch2.wav");
		precache_sound ("doors/winch2.wav");
		precache_sound ("doors/drclos4.wav");
		self.noise1 = "doors/latch2.wav";
		self.noise2 = "doors/winch2.wav";
		self.noise3 = "doors/drclos4.wav";
	}
	if (self.sounds == 2)
	{
		precache_sound ("doors/airdoor1.wav");
		precache_sound ("doors/airdoor2.wav");
		self.noise1 = "doors/airdoor2.wav";
		self.noise2 = "doors/airdoor1.wav";
		self.noise3 = "doors/airdoor2.wav";
	}
	if (self.sounds == 3)
	{
		precache_sound ("doors/basesec1.wav");
		precache_sound ("doors/basesec2.wav");
		self.noise1 = "doors/basesec2.wav";
		self.noise2 = "doors/basesec1.wav";
		self.noise3 = "doors/basesec2.wav";
	}

	self.dmg = zeroconvertdefault(self.dmg, 2);
	
	self.mangle = self.angles;
	self.angles = '0 0 0';
	self.solid = SOLID_BSP;
	self.movetype = MOVETYPE_PUSH;
	//self.classname = "door";
	setmodel (self, self.model);
	setorigin (self, self.origin);	
	
	self.touch = secret_touch;
	self.blocked = secret_blocked;
	self.speed = 50;
	self.use = secretdoor_use;
	if ( !self.targetname || self.spawnflags & SECRET_YES_SHOOT)
	{
		if (!self.health)
			self.health = 10;
		self.max_health = self.health;
		self.takedamage = DAMAGE_YES;
		self.th_pain = secretdoor_pain;
		self.th_die = secretdoor_use;
	}
	self.oldorigin = self.origin;
	
	if (self.spawnflags & SECRET_OPEN_ONCE) self.wait = -1;
	if (!self.wait) self.wait = 5;	// 5 seconds before closing
	
	// Magic formula...
	// lunaran: the magic formula sucked
	// also you moved this to secret_door_use at some point, don't know why, if a secret door breaks somewhere that might be why
	local vector firstmove, secondmove;
	
	if (!self.dest1 && !self.dest2)
	{
		// dumb old way
		makevectors(self.mangle);
		if (self.spawnflags & SECRET_1ST_LEFT)
			firstmove = v_right * -1;
		else if (self.spawnflags & SECRET_1ST_DOWN) 
			firstmove = v_up * -1;
		else
			firstmove = v_right;
		secondmove = v_forward;
	}
	else
	{
		firstmove = normalize(self.dest1);
		secondmove = normalize(self.dest2);
	}
	
	if (!self.t_width)
		self.t_width = BoundsAngleSize(firstmove, self.size);
		
	if (!self.t_length)
		self.t_length = BoundsAngleSize(secondmove, self.size);

	self.dest1 = self.origin + firstmove * self.t_width;
	self.dest2 = self.dest1 + secondmove * self.t_length;	
}
