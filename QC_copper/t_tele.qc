/*
==============================================================================

TELEPORT TRIGGERS

all teleport functionality handled by one function, to allow me to invoke
teleports with a couple different entities that don't replicate each other's
code

==============================================================================
*/

float	TELEPORT_PLAYER_ONLY	= 1;
float	TELEPORT_SILENT			= 2;
float	TELEPORT_PUSH			= 2;
float	TELEPORT_TRIGGER_FIRST	= 4;
float	TELEPORT_NOTRIGGER		= 8;
float	TELEPORT_NOFLASH		= 16;
float	TELEPORT_MONSTER_ONLY	= 32;

// --------------------------------

void() teleport_flash = 
{
	local	float v;
	local	string snd;
	v = random() * 5;
	if (v < 1)
		snd = "misc/r_tele1.wav";
	else if (v < 2)
		snd = "misc/r_tele2.wav";
	else if (v < 3)
		snd = "misc/r_tele3.wav";
	else if (v < 4)
		snd = "misc/r_tele4.wav";
	else
		snd = "misc/r_tele5.wav";

	sound (self, CHAN_VOICE, snd, 1, ATTN_NORM);

	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_TELEPORT);
	WriteCoord (MSG_BROADCAST, self.origin_x);
	WriteCoord (MSG_BROADCAST, self.origin_y);
	WriteCoord (MSG_BROADCAST, self.origin_z);
}

// player teleports are the only case we don't want to play the tflash right on
// an entity, so teleport_flash still accepts an entity argument and we just make
// a temp entity for it in this single case
// arguably not at all better than changing teleport_flash to accept a vector arg
// but we need an ent to play the sound on anyway
void() teleport_flasher =
{
	teleport_flash();
	remove(self);
}

// just call this to teleport something somewhere
void(entity traveler, entity dst, float tfog, float nofwd) teleport =
{
	entity tport;
	vector vdir;

	if (dst.classname == "misc_teleporttrain")
	{
		if (dst.think1 == fly_idle)
		{
			vdir = vectoangles(dst.origin - traveler.origin);
			nofwd = TRUE;
		}
		else
		{
			vdir = vectoangles(dst.velocity);
		}
		vdir_x = 0;	// null pitch
	}
	else
		vdir = dst.mangle;

	if (tfog)
	{
		SUB_CallAsSelf(teleport_flash, traveler);	// flash at departure point
		tport = spawn();
		
		// teleport controller's origin is desired teleport flash location on arrival
		if (traveler.classname == "player")
		{
			makevectors (vdir);
			setorigin(tport, dst.origin + v_forward * 16);
		}	
		else
		{
			setorigin(tport, dst.origin);
		}
	
		tport.think = teleport_flasher;
		tport.nextthink = time + 0.05;
	}

	spawn_tdeath(dst.origin, traveler);

	setorigin(traveler, dst.origin);
	traveler.angles = vdir;
	if (traveler.flags & FL_MONSTER)
	{
		traveler.ideal_yaw = traveler.angles_y;
	}

	if (traveler.classname == "player")
	{
		fog_setFromEnt(traveler, dst);
		traveler.fixangle = 1;		// turn this way immediately
		makevectors (vdir);
		SUB_UseEntTargets(dst);
		
		if (!nofwd)
		{
			traveler.teleport_time = time + 0.7;
			traveler.velocity = v_forward * 300;
		}
		else
		{
			traveler.velocity = VEC_ORIGIN;
		}
	}
	else
	{
		traveler.velocity = VEC_ORIGIN;
	}
		
	traveler.flags = not(traveler.flags, FL_ONGROUND);
}

// --------------------------------
// tdeath
// --------------------------------

void() tdeath_touch =
{
	if (other == self.owner)
		return;

// frag anyone who teleports in on top of an invincible player
	if (other.classname == "player")
	{
		if (other.invincible_finished > time)
			self.classname = "teledeath2";
		if (self.owner.classname != "player")
		{	// other monsters explode themselves
			T_Damage (self.owner, self, self, 50000);
			return;
		}
	}

	if (other.health)
		T_Damage (other, self, self, 50000);
}

void(vector org, entity death_owner) spawn_tdeath =
{
	local entity	death;

	// dumb hack so the world isn't flooded with a tdeath for every monster at map start
	if (time < 2) return;	
	
	death = spawn();
	death.classname = "teledeath";
	death.movetype = MOVETYPE_NONE;
	death.solid = SOLID_TRIGGER;
	setsize (death, death_owner.mins - '1 1 1', death_owner.maxs + '1 1 1');
	setorigin (death, org);
	death.angles = '0 0 0';
	death.touch = tdeath_touch;
	death.nextthink = time + 0.2;
	death.think = SUB_Remove;
	death.owner = death_owner;
	
	force_retouch = 2;		// make sure even still objects get hit
}

// --------------------------------

void() teleport_touch =
{
	local entity	t;
	if (self.customflags & CFL_LOCKED) return;
	if (other.health <= 0 || other.solid != SOLID_SLIDEBOX) return;	// only teleport living creatures
	if (other.movetype == MOVETYPE_NOCLIP) return;
	if ((self.spawnflags & TELEPORT_PLAYER_ONLY) && (other.classname != "player")) return;
	if ((self.spawnflags & TELEPORT_MONSTER_ONLY) && !(other.flags & FL_MONSTER)) return;

	if ((self.spawnflags & TELEPORT_NOTRIGGER) == 0)
		if (self.targetname != string_null)
			if ((self.spawnflags & TELEPORT_TRIGGER_FIRST) == 0) // not trigger first
				if (self.nextthink < time)
	{
		//dprint3("  teleport with targetname ", self.targetname, " not activated yet, not teleporting\n");
		return;
	}

	t = find (world, targetname, self.target);
	if (!t)
		objerror ("teleporter couldn't find target");

	if (!self.count) 
	{
		SUB_RemoveSoon();
		return;
	}
	else if (self.count > 0)
		self.count -= 1;
	
	teleport( other, t, ((self.spawnflags & TELEPORT_NOFLASH) == 0), 0 );
	
	activator = other;
	SUB_UseTargets();
}

/*QUAKED info_teleport_destination (.75 .25 .85) (-16 -16 0) (16 16 48)
This is the destination marker for a teleporter.

Keys:
"angle" new view angle after teleporting
"targetname" value used by teleporter
"fog_color"/"fog_density" set the fog for anyone teleporting here
*/
/*FGD
@PointClass size(-32 -32 0, 32 32 64) base(Appearflags, Angle, Targetname, Fog) model({ "path": ":progs/player.mdl" }) = info_teleport_destination : "Teleporter destination" []
*/
void() info_teleport_destination =
{
	if (!SUB_ShouldSpawn()) return;
// this does nothing, just serves as a target spot
	self.mangle = self.angles;
	self.angles = '0 0 0';
	self.model = string_null;
	self.origin = self.origin + '0 0 27';
	if (!self.targetname)
		objerror ("no targetname");
}

// --------------------------------
// info_teleport_target
//
// this entity is handy when activating a big trigger_teleport volume
// to try and teleport something is inconvenient
// --------------------------------

// teleports the activator to this spot
void() teleport_target_use =
{
	if (self.customflags & CFL_LOCKED)
	{
		dprint("  teleport_target is locked: not teleporting\n");
		return;
	}
	
	if (self.spawnflags & TELEPORT_PLAYER_ONLY)
		if (activator.classname != "player")
			return;
	if (self.spawnflags & TELEPORT_MONSTER_ONLY)
		if (!(activator.flags & FL_MONSTER))
			return;
	if (activator.movetype == MOVETYPE_NOCLIP)
		return;
	// only teleport living creatures
	if (activator.health <= 0 || activator.solid != SOLID_SLIDEBOX)
		return;

	if (!self.count) 
	{
		SUB_RemoveSoon();
		return;
	}
	else if (self.count > 0)
		self.count -= 1;

	teleport( activator, self, !(self.spawnflags & TELEPORT_NOFLASH), !(self.spawnflags & TELEPORT_PUSH) );
	activator.jump_flag = 0;
}


/*QUAKED info_teleport_target (.5 .75 .25) (-8 -8 -8) (8 8 32) PLAYER_ONLY PUSH TRIGGER_FIRST ? NOFLASH MONSTER_ONLY
This is a usable destination marker that acts as its own teleporter.  When triggered, the activator is teleported to this spot.

Flags:
PLAYER_ONLY		only players will teleport
PUSH			chuck the player forward like a trigger_teleport does
TRIGGER_FIRST	first use turns it on, and every subsequent use teleports
NOFLASH			don't spawn telefog or make noise
MONSTER_ONLY	only monsters will teleport

Keys:
"fog_color"/"fog_density" set the fog for anyone teleporting here
"count"		remove after this many uses
*/
/*FGD
@PointClass size(-32 -32 0, 32 32 64) base(Appearflags, Angle, Fog, Trigger) = info_teleport_target : "Triggerable Teleporter destination" [
	spawnflags(flags) = [
		1 : "Player only" : 0
		2 : "Push forward" : 0
		16 : "No tflash" : 0
	]
]
*/
void() info_teleport_target =
{
	if (!SUB_ShouldSpawn()) return;
	self.mangle = self.angles;
	self.angles = '0 0 0';
	setorigin(self, self.origin + '0 0 16'); // 
	//if (!self.targetname)
	if (self.targetname == string_null)
		objerror ("no targetname");
	self.count = zeroconvertdefault(self.count, -1);
	self.use = teleport_target_use;
}


// --------------------------------

void() teleport_hum =
{
	local vector o;
	if (self.origin != '0 0 0')
		o = self.origin;
	else
		o = (self.absmin + self.absmax)*0.5;
	ambientsound (o, "ambience/hum1.wav",0.5 , ATTN_STATIC);
}

void() teleport_use =
{
	self.nextthink = time + 0.2;
	force_retouch = 2;		// make sure even still objects get hit
	self.think = SUB_Null;
}

void() teleport_reactivate =
{
	trigger_reactivate();
	// triggerfirst + nonsilent = delayed ambientsound, so shortcut teleporters don't sound like secrets
	if ( self.spawnflags & TELEPORT_TRIGGER_FIRST && !(self.spawnflags & TELEPORT_SILENT) )
		teleport_hum();
	
	self.use = teleport_use;
}

void() trigger_teleport_setup =
{
	self.touch = teleport_touch;
	// find the destination 
	if (!self.target)
		objerror ("no target");
	self.use = teleport_use;
	self.count = zeroconvertdefault(self.count, -1);
	if (!(self.spawnflags & TELEPORT_SILENT))
	{
		precache_sound ("ambience/hum1.wav");
		if (!(self.spawnflags & TELEPORT_TRIGGER_FIRST))
			teleport_hum();
	}
}

/*FGD
@baseclass base(Appearflags, Target, Targetname) = Teleport [
	spawnflags(Flags) =
	[
		1 : "Player only" : 0
		2 : "Silent" : 0
		8 : "Not triggerable (ignore targetname)" : 0
		16 : "No tflash" : 0
		32 : "Monsters only" : 0
	]
	count(integer) : "Limited uses" : -1
]

*/

/*QUAKED trigger_teleport (.5 .0 .5) ? player_only silent trigger_first notrigger noflash monster_only
Any object touching this will be transported to the corresponding info_teleport_destination entity.
You must set the "target" field, and create an object with a "targetname" field that matches.

Flags:
"player_only" only players will teleport
"silent" no whispering from the slipgate
"trigger_first" won't be touchable until triggered once by something else
"notrigger" default behavior for a teleporter with a targetname is to only teleport what's touching it
	when triggered.  this will preserve ordinary no-targetname behavior (if you're using the teleporter's
	targetname for a killtarget for example)
"noflash" don't spawn tfog or make noise
"monster_only" only monsters will teleport

Keys:
"target" targetname of info_teleport_destination (required)
"targetname" entity name (required)
"count" remove after this many uses
*/
/*FGD
@SolidClass base(Teleport) = trigger_teleport : "Trigger: Teleporter" []
*/
void() trigger_teleport =
{
	if (!SUB_ShouldSpawn()) return;
	InitTrigger ();
	trigger_teleport_setup();	// after inittrigger, so that size/model is set, so the telehum hums in the right place
	if (self.spawnflags & TELEPORT_TRIGGER_FIRST)
		self.use = teleport_reactivate;
}

/*QUAKED trigger_teleport_box (.5 .0 .5) (-8 -8 -8) (8 8 8) player_only silent trigger_first notrigger
Any object touching this will be transported to the corresponding info_teleport_destination entity.
You must set the "target" field, and create an object with a "targetname" field that matches.
Quoth2-style edict-saving bounding box trigger: define bounds size with "mangle" - trigger volume will be centered on the trigger_teleport_box's origin.

Flags:
"player_only" only players will teleport
"silent" no whispering from the slipgate
"trigger_first" won't be touchable until triggered once by something else
"notrigger" default behavior for a teleporter with a targetname is to only teleport what's touching it
	when triggered.  this will preserve ordinary no-targetname behavior (if you're using the teleporter's
	targetname for a killtarget for example)
"noflash" don't spawn tfog or make noise

Keys:
"target" targetname of info_teleport_destination (required)
"targetname" entity name (required)
*/
/*FGD
@PointClass base(Teleport, Bounds) = trigger_teleport_box : "Trigger: Teleporter bounds" []
*/
void() trigger_teleport_box =
{
	if (!SUB_ShouldSpawn()) return;
	InitTriggerBounds();
	trigger_teleport_setup();
	if (self.spawnflags & TELEPORT_TRIGGER_FIRST)
		self.use = teleport_reactivate;
}


