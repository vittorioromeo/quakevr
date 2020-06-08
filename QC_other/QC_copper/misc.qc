
/*QUAKED info_null (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for spotlights, etc. Not present in game.
*/
/*FGD
@PointClass base(Targetname) size(8 8 8) color(200 150 150) = info_null : "info_null (spotlight target)" []
*/
void() info_null =
{
	remove(self);
}

/*QUAKED info_notnull (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for lighting. Present in game.
*/
/*FGD
@PointClass base(Appearflags, Target, Targetname) size(8 8 8) color(200 150 150) = info_notnull : "Wildcard entity" // I love you too
[
	use(string) : "self.use"
	think(string) : "self.think"
	nextthink(integer) : "nextthink"
	noise(string) : "noise"
	touch(string) : "self.touch"
]
*/
void() info_notnull =
{
	if (!SUB_ShouldSpawn()) return;
}

/*QUAKED misc_external_map (0.6 0.7 0.8) (-4 -4 -4) (4 4 4)

"_external_map"				Specifies the filename of the .map to import.
"_external_map_classname"	What entity you want the external map to turn in to. You can use internal qbsp entity types such as "func_detail", or a regular bmodel classname like "func_wall" or "func_door".
"_external_map_angles"		Rotation for the prefab, "pitch yaw roll" format. Negative pitch is UP.
"_external_map_angle"		Short version of "_external_map_angles" for when you want to specify just a yaw rotation.
"_external_map_scale"		Scale factor for the prefab, defaults to 1. Either specify a single value or three scales, "x y z".
*/
/*FGD
@PointClass = misc_external_map : "Put another map in your map" [
	_external_map(string) : "Filename of .map to import"
	_external_map_classname(string) : "Classname to turn imported worldspawn into"
	_external_map_angles(string) : "Rotation (pitch yaw roll, negative pitch is up)"
	_external_map_angle(string) : "Rotation (yaw only)"
	_external_map_scale(string) : "Scale (one value or x y z)"
]
*/

void() misc_model_animate =
{
	vector anim;
	if (self.state) anim = self.pos2;
	else anim = self.pos1;
	
	self.frame += 1;
	if (self.frame > anim_z)
	{
		self.frame = anim_x;
		if (self.spawnflags & SPAWN_MODEL_ANIMONCE && self.state)
		{
			self.state = !self.state;
			self.frame = self.pos1_y;
			self.nextthink = time + self.speed;
			return;
		}
	}
	self.nextthink = time + self.speed;
}

void() misc_model_use =
{
	float tmp;
	self.state = !self.state;
	if (self.spawnflags & SPAWN_MODEL_TOGGLEVIS)
	{
		tmp = self.oldstyle;
		self.oldstyle = self.alpha;
		self.alpha = tmp;
	}
	if (self.state)
	{
		self.frame = self.pos2_y;
	}
	else
	{
		self.frame = self.pos1_y;
	}
	self.think = misc_model_animate;
	self.nextthink = time + self.speed;
}

float SPAWN_MODEL_TOGGLEVIS = 1;
float SPAWN_MODEL_STARTINVIS = 2;
float SPAWN_MODEL_ANIMONCE = 4;

/*QUAKED misc_model (0.6 0.7 0.8) (-8 -8 -8) (8 8 8) TOGGLE_VIS START_INVIS ANIM_ONCE
display a model. will not be solid! for a misc_model which can be toggled on or off, use a func_wall_point with a targetname.

"model" filename of model to stand in for this entity. can be a bsp or an mdl. 
"mangle" exact orientation of the model, values are 'pitch yaw roll', positive pitch is down
"frame" frame of model (0-ordinal)
"skin" skin index on model (0-ordinal)
"pos1" override frame with an animation (firstframe startframe lastframe)
"pos2" if triggerable, second animation to toggle to (firstframe startframe lastframe)
"ideal_yaw" randomly select yaw angle (for quickly scattering props)

TOGGLE_VIS show/hide when triggered
START_INVIS start hidden
ANIM_ONCE if second triggered animation, will play once when triggered (instead of switching loops)

dead player.mdl frame reference because we both know that's what you want:
axdeth: 49 (on back (with axe (duh)))
deatha: 60 (on face)
deathb: 69 (sat)
deathc: 84 (also on face)
deathd: 93 (also on face but arms under forehead as if sad)
deathe: 102 (on side)
*/
/*FGD
@PointClass base(Appearflags) size(16 16 16) color(160 192 224) = misc_model : 
"Display a model. Not solid.
for a misc_model which can be toggled on or off, use a func_wall_point with a targetname."
[
	spawnflags(flags) = [
		1 : "Trigger Visibility" : 0
		2 : "Start Invisible" : 0
		4 : "2nd anim once" : 0	
	]
	model(string) : "Filename of model (mdl or bsp)"
	mangle(string) : "Orientation (pitch yaw roll, positive pitch is down)"
	frame(integer) : "frame of mdl to display" : 0
	skin(integer) : "skin of mdl to display" : 0
	pos1(string) : "override frame with an animation (firstframe startframe lastframe)"
	pos2(string) : "if triggerable, second animation to toggle to"
]
*/
void() misc_model = 
{ 
	if (!SUB_ShouldSpawn()) return;
	
	if (self.targetname == string_null && self.length == 0)
	{
		func_static_point(); 
		return;
	}
	
	precache_safe_model(self.model);
	
	if (self.mangle != '0 0 0')
	{
		self.angles = self.mangle;
	}
	else if (self.angles != '0 0 0')
	{
		if (self.angles == '0 -1 0')
			self.angles = '-90 0 0';
		else if (self.angles == '0 -2 0')
			self.angles = '90 0 0';
	}
	if (self.ideal_yaw)
		self.angles_y = random()*360;
	setmodel (self, self.model);
	setsize(self, '-16 -16 -16', '16 16 16');
	
	if (self.spawnflags & SPAWN_MODEL_STARTINVIS)
	{
		self.oldstyle = self.alpha;
		self.alpha = -1;		
	}

	self.solid = SOLID_NOT;
	self.movetype = MOVETYPE_NONE;
	if (!self.speed)
		self.speed = 0.1;
		
	// toggle animation
	if (self.targetname != string_null)
	{
		self.use = misc_model_use;
	}
	if (self.pos1)
	{
		self.frame = self.pos1_y;
		self.think = misc_model_animate;
		self.nextthink = time + 0.1;
	}		
}


//============================================================================


void() target_heal_setmax =
{
	if (self.max_health)
		self.count = self.max_health - activator.health;
	else
		self.count = activator.max_health - activator.health;
	if (self.strength)
		self.count = ceil(self.count * self.strength);
	self.count = max(self.lip, self.count);
}

void() target_heal_do =
{
	float h;
	entity tgt;
	
	if (self.count > 0)
		h = min(self.healamount, self.count);	// only heal for what's left if we're almost dry
	else
		h = self.healamount;
	
	// don't heal activator if we have target fields even if we find no targets, 
	// because if a target_heal's targets have all been killtargeted or removed
	// we don't want the player to receive mystery health instead
	if (self.target == string_null &&
		self.target2 == string_null &&
		self.target3 == string_null &&
		self.target4 == string_null)
	{
		if ((self.spawnflags & 2) && tgt.classname != "player")
			return; // player only
		tgt = activator;
		if (T_Heal(tgt, h, (self.spawnflags & 1)) && self.count > 0)
			self.count -= h;
		return;
	}
	
	if (self.target == string_null) return;
	tgt = world; while(1) {
		tgt = find(tgt,targetname,self.target); if (!tgt) break;
		if (T_Heal(tgt, h, (self.spawnflags & 1)) && self.count > 0)
			self.count -= h;
	}
	if (self.target2 == string_null) return;
	tgt = world; while(1) {
		tgt = find(tgt,targetname,self.target2); if (!tgt) break;
		if (T_Heal(tgt, h, (self.spawnflags & 1)) && self.count > 0)
			self.count -= h;
	}	
	if (self.target3 == string_null) return;
	tgt = world; while(1) {
		tgt = find(tgt,targetname,self.target3); if (!tgt) break;
		if (T_Heal(tgt, h, (self.spawnflags & 1)) && self.count > 0)
			self.count -= h;
	}	
	if (self.target4 == string_null) return;
	tgt = world; while(1) {
		tgt = find(tgt,targetname,self.target4); if (!tgt) break;
		if (T_Heal(tgt, h, (self.spawnflags & 1)) && self.count > 0)
			self.count -= h;
	}
}

void() target_heal_use =
{
	if (self.customflags & CFL_LOCKED)
		return;
	if (self.count == 0)
		return;	// we're dry
	
	// trigger first
	if (!self.state) 
	{
		self.state = 1;
		// autobalance
		if ((self.spawnflags & 8) && activator.health >= 0 && activator.max_health)
			target_heal_setmax();
			
		return;
	}
	
	target_heal_do();
}

/*QUAKED target_heal (0.75 0.25 0) (-8 -8 -8) (8 8 8) OVERHEAL PLAYER_ONLY TRIGGER_FIRST AUTO_AMT
Heals activator for 'healamount' when triggered. Use for healing pools, first aid dispensers, etc.
If it has any targets, it heals them instead, even doors and buttons. This will not reset 'dead' doors or buttons with a wait of -1.

Spawnflags:
OVERHEAL	will ignore target's max_health when healing
PLAYER_ONLY	will ignore any targets or activators which aren't clients
TRIGGER_FIRST	will treat its first trigger as an activation, then work normally after
AUTO_AMT	if trigger_first, will choose 'count' based on how much health the activator needs when first triggered, for dirty lazy automatic game balance

Keyvalues:
"healamount" amount to heal per use, default 5
"count" expire after healing this much in total, default -1 (never)
  "strength" if auto, count will be multiplied by this much, default 1
  "lip" if auto, never set count less than this much, defaults to healamount
  "max_health" if auto, never set count to heal *target* above this much
*/
/*FGD
@Pointclass base(Target, Targetname, Appearflags) color(192 64 0) size(32 32 32) = target_heal : 
"Heals targets when triggered, or activator if no targets set" 
[
	spawnflags(flags) = [
		1 : "ignore max_health" : 0
		2 : "ignore non-clients" : 0
		4 : "first trigger activates" : 0
		8 : "auto-set count on trigger-first" : 0
	]
	healamount(integer) : "heal per use" : 5
	count(integer) : "lifetime limit" : -1
	strength(string) : "scale auto-limit" : "1.0"
	lip(integer) : "minimum for auto-limit"
	max_health(integer) : "max target health for auto-limit"
]
*/
void() target_heal =
{
	if (self.healamount <= 0)
		self.healamount = 5;
	if (!self.count)
		self.count = -1;
	if (!(self.spawnflags & 4))
		self.state = 1;
	if (self.spawnflags & 8)
	{
		if (!self.strength) self.strength = 1;
		if (!self.lip) self.lip = self.healamount;
	}
	
	self.use = target_heal_use;
}



//============================================================================


void() target_kill_use =
{
	local entity t;

	// kill targets
	if (self.target != string_null) {
		t = world;
		do	{
			t = find(t, targetname, self.target);
			if (!t) break;
			RemoveTarget (t);
		} while ( 1 );
	}
	if (self.target2 != string_null) {
		t = world;
		do	{
			t = find(t, targetname, self.target2);
			if (!t) break;
			RemoveTarget (t);
		} while ( 1 );
	}
	if (self.target3 != string_null) {
		t = world;
		do	{
			t = find(t, targetname, self.target3);
			if (!t) break;
			RemoveTarget (t);
		} while ( 1 );
	}
	if (self.target4 != string_null) {
		t = world;
		do	{
			t = find(t, targetname, self.target4);
			if (!t) break;
			RemoveTarget (t);
		} while ( 1 );
	}

	SUB_KillTargets(self.killtarget);
}

/*QUAKED target_kill (0.75 0.5 0) (-8 -8 -8) (8 8 8)
Kills ALL of its targets (1-4) as well as killtargets when triggered. for targets you really don't like.
*/
/*FGD
@Pointclass base(Target, Targetname, Appearflags) color(192 64 0) size(16 16 16) = target_kill : 
"Removes all targets when triggered." []
*/
void() target_kill =
{
	if (!SUB_ShouldSpawn()) return;
	self.use = target_kill_use;
}

//============================================================================


void() target_autosave_use =
{
	if (self.enemy)
	{
		activator = self.enemy;
		self.enemy = world;
	}
	if (activator.classname != "player") return;
	if (time < 2)	// make sure an autosave fired from a player start doesn't happen too early
	{
		if (serverflags & SVFL_RESPAWNING)
		{
			dprint("RESPAWNING flag set, skipping autosave\n");
			return;
		}
		self.enemy = activator;
		self.think = target_autosave_use;
		self.nextthink = 2;
		return;
	}
	autosave(activator, self.message);
}

/*QUAKED target_autosave (1 .0 .5) (-8 -8 -8) (8 8 8)
Saves the game when triggered by a player. Never appears in multiplayer. the bprint tends to stomp any other prints on screen in most quake clients, so use a delayed trigger_relay if you fire this from an important pickup/trigger_counter/something else that puts text on screen more important than the autosave blurb.

Keys:
"message" change save file name, defaults to 'auto'
*/
/*FGD
@Pointclass base(Target, Targetname, Appearflags) color(255 0 128) size(32 32 32) = target_autosave : 
"Saves the game when triggered by a player. Never appears in multiplayer. 
the bprint tends to stomp any other prints on screen in most quake clients, so use a delayed trigger_relay if you fire this from an important pickup/trigger_counter/something else that puts text on screen more important than the autosave blurb." 
[
	message(string) : "Change save filename" : "auto"
]
*/
void() target_autosave =
{
	if (deathmatch)
	{
		remove(self);
		return;
	}
	
	// never autosave in coop anyway
	self.spawnflags = not(self.spawnflags, SPAWN_COOPONLY) | SPAWN_NOTCOOP;
	if (!SUB_ShouldSpawn()) return;
	
	if (self.message == string_null)
		self.message = "auto";
	
	self.use = target_autosave_use;
}


//============================================================================




void() lavaball_touch =
{
	if (other.takedamage)
	{
		sound (self, CHAN_WEAPON, "zombie/z_hit.wav", 1, ATTN_IDLE);
		T_Damage (other, self, self, self.dmg);
	}
	remove(self);
}

void() lavaball_fly =
{
	entity fireball;
	vector vel;
	vel = self.movedir * self.speed;
	vel_x += crandom() * self.mangle_x;
	vel_y += crandom() * self.mangle_y;
	vel_z += crandom() * self.mangle_z;

	fireball = launch_lavamissile(self.origin, vel);
	fireball.movetype = MOVETYPE_TOSS;
	
	fireball.classname = "fireball";

	if (!(self.spawnflags & 4))
		fireball.touch = lavaball_touch;
	fireball.dmg = self.dmg;
}

void() lavaball_toggle = 
{
	self.state = !self.state;
	if (!self.state)
	{	
		self.nextthink = 0;
		return;
	}
	self.think = lavaball_idle;
	self.nextthink = time + self.delay + random() * self.rand;
}

void() lavaball_use =
{
	self.nextthink = time + self.delay + random() * self.rand;
	self.think = lavaball_fly;
}

void() lavaball_idle =
{
	lavaball_fly();
	self.nextthink = time + self.wait + random() * self.rand;
}

/*QUAKED misc_fireball (0 .5 .8) (-8 -8 -8) (8 8 8) START_OFF BURST EXPLODE
LavA_Ballz
Fires randomly. Trigger to turn on/off. 

"wait" min pause between balls
"rand" max extra pause between balls (total pause is 'wait' + 'rand' * random())
"speed" launch velocity, defaults to 1000
"movedir" launch direction, defaults to straight up (0 0 1)
"mangle" amount of randomness to apply to velocity (x, y, z)
"dmg" damage on contact, default 20

Spawnflags
START_OFF start off
BURST fires "delay" seconds after being triggered
EXPLODE blow up on contact, does 'dmg' damage (default 20)
*/
/*FGD
@PointClass base(Appearflags, Targetname) model({ "path": ":progs/lavaball.mdl" }) = misc_fireball : "Lavaball Launcher. Trigger to toggle on/off." 
[
	spawnflags(flags) = [
		1 : "Start off" : 0
		2 : "Launch when triggered (no toggle)" : 0
		4 : "Explode on contact" : 0
	]
	delay(string) : "Pause before first ball" : "0"
	wait(string) : "Pause between balls" : "3"
	rand(string) : "Random extra pause before ball" : "5"
	speed(integer) : "Speed" : 1000
	movedir(string) : "Launch Dir" : "0 0 1"
	mangle(string) : "Random velocity addition (x y z)" : "50 50 100"
	dmg(integer) : "Contact damage" : 20
]
*/
void() misc_fireball =
{
	if (!SUB_ShouldSpawn()) return;
	
	precache_model ("progs/lavaball.mdl");
	self.classname = "fireball";

	self.wait = zeroconvertdefault(self.wait, 3);
	self.rand = zeroconvertdefault(self.rand, 5);
	self.dmg = zeroconvertdefault(self.dmg, 20);
	
	if (self.targetname == string_null)
	{
		self.think = lavaball_idle;
		self.nextthink = time + self.delay + (random() * self.rand);
	}
	else	// targeted
	{
		if (self.spawnflags & 2)	// fire on trigger
			self.use = lavaball_use;
		else						// toggle on trigger
		{
			self.state = !(self.spawnflags & 1);
			self.use = lavaball_toggle;
		}
	}
	
	if (self.mangle == VEC_ORIGIN)
		self.mangle = '50 50 100';
	if (self.movedir == VEC_ORIGIN)
		self.movedir = '0 0 1';
	else
		self.movedir = normalize(self.movedir);
	self.speed = zeroconvertdefault(self.speed, 1000);
}



//============================================================================

float SPAWNFLAG_SHOOTER_SUPERSPIKE	= 1;
float SPAWNFLAG_SHOOTER_LASER		= 2;

/*FGD
@baseclass base(Appearflags, Targetname, Angle) color(220 150 150) = Shooter
[
	spawnflags(Flags) = 
	[
		1 : "Spike" : 0
		2 : "Laser" : 0
	]
	movedir(string) : "Override launch vector (x y z)"
	volume(string) : "Override sound volume"
	mangle(string) : "Random velocity addition (x y z)" : "50 50 100"
	delay(string) : "Pause before firing" : "0"
]
*/

void() spikeshooter_fire =
{
	if (self.state) return;
	
	vector dir;
	entity tgt;
	
	if (self.target != string_null)
		tgt = find(world, targetname, self.target);
		
	if (tgt)
		dir = normalize(tgt.origin - self.origin);
	else
		dir = self.movedir;
	
	if (self.spawnflags & SPAWNFLAG_SHOOTER_LASER)
	{
		sound (self, CHAN_VOICE, "enforcer/enfire.wav", self.volume, ATTN_NORM);
		dprint5("shot from ", vtos(self.origin), " at ",vtos(dir), "\n");
		launch_laser (self.origin, dir*600);
	}
	else
	{
		sound (self, CHAN_VOICE, "weapons/spike2.wav", self.volume, ATTN_NORM);
		launch_nail (self.origin, dir * 500, (self.spawnflags & SPAWNFLAG_SHOOTER_SUPERSPIKE));
	}
}

void() shooter_proxy_think =
{
	spikeshooter_fire();
	remove(self);
}

void() shooter_once_think =
{
	// iw - if a shooter is triggered more often than its .delay, nextthink is
	// repeatedly overwritten and the shooter never fires (bbin1)
	// lunaran - spawn a proxy that'll fire at the right time to 'queue up'
	// shots, so that none are missed
	if (self.nextthink)
	{
		entity shooty = spawn();
		shooty.think = shooter_proxy_think;
		shooty.nextthink = time + self.delay;
		return;
	}
	self.think = spikeshooter_fire;
	self.nextthink = time + self.delay;
}

void() shooter_think =
{
	spikeshooter_fire ();
	self.nextthink = time + self.wait + self.rand * random();
}

void() shooter_use =
{
	self.state = !(self.state);
	
	if (self.state) return;
	
	self.nextthink = time + self.delay;
}

/*QUAKED trap_spikeshooter (0 .5 .8) (-8 -8 -8) (8 8 8) superspike laser
When triggered, fires a spike in the direction set by "angle".

"movedir" override angle with a launch vector of your choosing
"mangle" random velocity offset
"delay" delay before firing (for staggering shooters)
"volume" override how loud these things are because damn
*/
/*FGD
@PointClass base(Shooter) = trap_spikeshooter : "Triggered shooter" []
*/
void() trap_spikeshooter =
{
	if (!SUB_ShouldSpawn()) return;
	SetMovedir ();
	self.volume = zeroconvertdefault(self.volume, 1);
	self.use = shooter_once_think;
	if (self.spawnflags & SPAWNFLAG_SHOOTER_LASER)
	{
		precache_model2 ("progs/laser.mdl");
		
		precache_sound2 ("enforcer/enfire.wav");
		precache_sound2 ("enforcer/enfstop.wav");
	}
	else
		precache_sound ("weapons/spike2.wav");
}


/*QUAKED trap_shooter (0 .5 .8) (-8 -8 -8) (8 8 8) superspike laser frost 
Continuously fires spikes in the direction set by "angle". Trigger to turn on, trigger again to turn off.
Laser is only for REGISTERED. YOU PAY FOR LASER
"wait" time between shots (1.0 default)
"delay" delay before firing first spike, so multiple shooters can be staggered
"movedir" override angle with a launch vector of your choosing
*/
/*FGD
@PointClass base(Shooter) = trap_shooter : "Continuous shooter"
[
	nextthink(integer) : "Delay before first spike"
	wait(integer) : "Wait between spikes"
	rand(string) : "Random extra delay between spikes"
]
*/
void() trap_shooter =
{
	if (!SUB_ShouldSpawn()) return;
	trap_spikeshooter ();
	
	if (self.targetname != string_null)	// start off if something targets us
		self.state = 1;
	self.use = shooter_use;
	if (self.wait == 0)
		self.wait = 1;
		
	// original id trap_shooter used nextthink as a mapper-editable delay, which is dumb
	// add it to nextthink here anyway for backwards compatibility
	self.nextthink = self.nextthink + self.delay + self.wait + self.ltime;
	self.think = shooter_think;
}

//=============================================================================

void() target_multiprint_use =
{
	//dprint("in multiprint_use\n");
	// print the message
	if (activator.classname == "player")
	{
		if ( self.message != string_null )
		{
			centerprint (activator, self.message);
			if (!(self.spawnflags & 1))
				sound (activator, CHAN_VOICE, "misc/talk.wav", 1, ATTN_NORM);
			
			self.message = self.noise;
			self.noise = self.noise1;
			self.noise1 = self.noise2;
			self.noise2 = self.noise3;
			self.noise3 = self.noise4;
			self.noise4 = string_null;
		}
		else
		{
			SUB_UseTargets();
		}
	}
}

/*QUAKED target_multiprint (.5 .5 .5) (-8 -8 -8) (8 8 8) SILENT
This entity will print a different string every time it"s triggered.  Prints "message", 
"noise", "noise1", "noise2", "noise3", then "noise4."  Once all strings are used up, it will 
fire its targets instead (so several of these can be chained if you really need that 
many prints in a row).

SILENT: Don't play the beep wav when triggered.
*/
/*FGD
@PointClass base(Appearflags, Targetname, Target) color(128 128 128) = target_multiprint : 
"Target : Multiprint
Prints a different string every time it's triggered, or fires its targets if it ran out."
[
	spawnflags(flags) = [
		1 : "Silent" : 0
	]
	message(string) : "First print"
	noise(string) : "Second print"
	noise1(string) : "Third print"
	noise2(string) : "Fourth print"
	noise3(string) : "Fifth print"
	noise4(string) : "Sixth print"
]
*/
void() target_multiprint =
{
	if (!SUB_ShouldSpawn()) return;
	self.use = target_multiprint_use;
}



/*~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>
~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~*/

/* viewthing (0 .5 .8) (-8 -8 -8) (8 8 8)

Just for the debugging level.  Don't use
*/

void() viewthing =

{
	self.movetype = MOVETYPE_NONE;
	self.solid = SOLID_NOT;
	precache_model ("progs/player.mdl");
	setmodel (self, "progs/player.mdl");
}



/* noclass (0 0 0) (-8 -8 -8) (8 8 8)
prints a warning message when spawned
*/
void() noclass =
{
	dprint3("noclass spawned at",vtos(self.origin),"\n");
	remove (self);
}

