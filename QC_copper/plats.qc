/*
=============================================================================

LIFTS AND TRAINS

=============================================================================
*/

float PLAT_LOW_TRIGGER = 1;


void() plat_spawn_inside_trigger =
{
	local entity	t;
	local vector	tmin, tmax;

	// FIXME: if the plat spawns before the trigger, will this fail?
	if (self.target)
	{
		// override touch zone for tweaking/effects
		t = find (world, targetname, self.target);
		if (!t) dprint("plat with unmatched target");
		if ( (t.classname != "trigger_once") && (t.classname != "trigger_multiple") && (t.classname != "trigger_multiple_box") ) 
			dprint("plat targets a non-trigger");
	}
	else
	{
		// touch zone should extend from the top of the plat in its lowest position
		// to 8 units above the top of the plat in its highest position. pos1 cannot
		// be assumed to be the highest position.
		t = spawn();
		local float high, low;
		
		high = max(self.pos1_z, self.pos2_z) + self.maxs_z;
		low = min(self.pos1_z, self.pos2_z) + self.maxs_z;
		
		tmin = self.mins + '25 25 0';
		tmax = self.maxs - '25 25 0';
		if (self.size_x <= 50)
		{
			tmin_x = (self.mins_x + self.maxs_x) / 2;
			tmax_x = tmin_x + 1;
		}
		if (self.size_y <= 50)
		{
			tmin_y = (self.mins_y + self.maxs_y) / 2;
			tmax_y = tmin_y + 1;
		}
		
		tmin_z = low;
		if (self.spawnflags & PLAT_LOW_TRIGGER)
			tmax_z = low + 8;
		else
			tmax_z = high + 8;
		
		t.solid = SOLID_TRIGGER;
		setsize (t, tmin, tmax);
	}
	
	t.touch = plat_center_touch;
	t.think = SUB_Null;
	t.movetype = MOVETYPE_NONE;
	t.enemy = self;
	self.trigger_field = t;
	
	self.target = "";
}

void() plat_hit_top =
{
	sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
	self.state = STATE_TOP;
	self.think = plat_go_down;
	self.nextthink = self.ltime + self.wait;
}

void() plat_hit_bottom =
{
	sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
	self.state = STATE_BOTTOM;
}

void() plat_go_down =
{
	sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
	self.state = STATE_DOWN;
	SUB_CalcMove (self.pos2, self.speed, plat_hit_bottom);
}

void() plat_go_up =
{
	sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
	self.state = STATE_UP;
	SUB_CalcMove (self.pos1, self.speed, plat_hit_top);
}

void() plat_center_touch =
{
	if (!CheckValidTouch()) return;

	self = self.enemy;
	if (self.state == STATE_BOTTOM)
		plat_go_up ();
	else if (self.state == STATE_TOP)
		self.nextthink = self.ltime + 1;	// delay going down
}
/*
void() plat_outside_touch =
{
	if (!CheckValidTouch()) return;
		
//dprint ("plat_outside_touch\n");
	self = self.enemy;
	if (self.state == STATE_TOP)
		plat_go_down ();
}
*/
void() plat_trigger_use =
{
	if (self.think)
		return;		// already activated
	plat_go_down();
}


void() plat_crush =
{
//dprint ("plat_crush\n");

	T_Damage (other, self, self, 1);
	
	if (self.state == STATE_UP)
		plat_go_down ();
	else if (self.state == STATE_DOWN)
		plat_go_up ();
	else
		dprint("plat_crush: bad self.state\n");
}

void() plat_use =
{
	self.use = SUB_Null;
	if (self.state != STATE_UP)
		dprint("plat_use: not in up state\n");
	plat_go_down();
}

void(entity p) plat_lock =
{
	p.trigger_field.touch = SUB_Null;
	p.use = SUB_Null;
	p.customflags |= CFL_LOCKED;
}

void(entity p) plat_unlock =
{
	p.trigger_field.touch = plat_center_touch;
	if (p.targetname != string_null)
		p.use = plat_use;
	p.customflags = not(p.customflags, CFL_LOCKED);
}


/*QUAKED func_plat (0 .5 .8) ? PLAT_LOW_TRIGGER
Plats are always drawn in the extended position, so they will light correctly.
If the plat is the target of another trigger or button, it will start out disabled in the top position until it is triggered, when it will lower and become a normal plat.
If the plat targets a trigger once/multiple/multiple_box, it will override the plat's default activation zone.

"PLAT_LOW_TRIGGER" activation zone will only be at the bottom of the movement course, instead of the whole vertical

"speed"		default 150
"height"	implicitly determine the distance the plat moves
"pos1"&"pos2"	directly override top and bottom positions - plat can be built anywhere else in the map
"wait"		time to pause at apex in seconds, default 3
"sounds"
	1: base fast
	2: chain slow (default if 0)
	-1: silent
*/
/*FGD
@SolidClass base(Appearflags, Targetname) = func_plat : "Elevator" 
[	
	spawnflags(Flags) =
	[
		1 : "Low trigger volume" : 0
	]
	target(target_destination) : "Custom activation trigger"
	speed(integer) : "Speed" : 150
	height(integer) : "Travel altitude (can be negative)" : 0
	wait(string) : "Apex pause time" : "3"
	pos1(string) : "Override top position (x y z)"
	pos2(string) : "Override bottom position (x y z)"
	sounds(choices) : "Sound" : 1 =
	[
		0: "None"
		1: "Base fast"
		2: "Chain Slow"
	]
]	
*/
void() func_plat =
{
	if (!SUB_ShouldSpawn()) return;

	precache_safe_model(self.model);

	if (self.sounds == 0)
		self.sounds = 2;
	// FIXME load generic plat sound

	if (self.sounds == -1)
	{
		if (!self.noise)
			self.noise = ("misc/null.wav");
		if (!self.noise1)
			self.noise1 = ("misc/null.wav");
		precache_sound(self.noise);
		precache_sound(self.noise1);
	}
	if (self.sounds == 1)
	{
		precache_sound ("plats/plat1.wav");
		precache_sound ("plats/plat2.wav");
		self.noise = "plats/plat1.wav";
		self.noise1 = "plats/plat2.wav";
	}

	if (self.sounds == 2)
	{
		precache_sound ("plats/medplat1.wav");
		precache_sound ("plats/medplat2.wav");
		self.noise = "plats/medplat1.wav";
		self.noise1 = "plats/medplat2.wav";
	}

	self.mangle = self.angles;
	self.angles = '0 0 0';

	self.classname = "plat";
	self.solid = SOLID_BSP;
	self.movetype = MOVETYPE_PUSH;
	setorigin (self, self.origin);	
	setmodel (self, self.model);
	setsize (self, self.mins, self.maxs);

	self.blocked = plat_crush;
	if (!self.speed)
		self.speed = 150;
	if (!self.wait)
		self.wait = 3;

	if (!self.pos1 && !self.pos2)
	{
		// pos1 is the top position, pos2 is the bottom
		self.pos1 = self.origin;
		self.pos2 = self.origin;
		if (self.height)
			self.pos2_z = self.origin_z - self.height;
		else
			self.pos2_z = self.origin_z - self.size_z + 8;
	}
	self.use = plat_trigger_use;

	plat_spawn_inside_trigger ();	// the "start moving" trigger	

	//if (self.targetname)
	if (self.targetname != string_null)
	{
		setorigin (self, self.pos1);
		self.state = STATE_UP;
		self.use = plat_use;
	}
	else
	{
		setorigin (self, self.pos2);
		self.state = STATE_BOTTOM;
	}
}

//============================================================================

void() train_sounds =
{
	if (self.sounds == 0)
	{
		if (!self.noise)
			self.noise = ("misc/null.wav");
		if (!self.noise1)
			self.noise1 = ("misc/null.wav");
		precache_sound(self.noise);
		precache_sound(self.noise1);
	}

	if (self.sounds == 1)
	{
		self.noise = ("plats/train2.wav");
		precache_sound ("plats/train2.wav");
		self.noise1 = ("plats/train1.wav");
		precache_sound ("plats/train1.wav");
	}
	if (self.sounds == 2)
	{
		precache_sound ("plats/plat1.wav");
		precache_sound ("plats/plat2.wav");
		self.noise1 = "plats/plat1.wav";
		self.noise = "plats/plat2.wav";
	}

}

void() flytrain_think = 
{
	self.think1();
	self.nextthink = self.ltime + 0.1;
	
//	dprint(ftos(vlen(self.velocity)));
//	dprint("\n");
}

void() flytrain_use =
{
	if (self.think1 == fly_idle)
		fly_newgoal( 1, 0 );
}

void() flytrain_init2 =
{
	//dprint("in flytrain_init2\n");
	fly_newgoal( 1, 0 );
	
	self.think = flytrain_think;
	flytrain_think();

	//dprint("end of flytrain_init2\n");
}

// flytrain_init
// switch the light and sparklies on
void() flytrain_init =
{
	//dprint("in flytrain_init\n");
	self.goalentity = self;		// haaax
	self.use = flytrain_use;
	self.think = flytrain_init2;
	self.nextthink = self.ltime + 0.1;
	//dprint("end of flytrain_init\n");
}

void() flytrain_stop =
{
	setorigin(self, self.goalentity.origin);
	self.velocity = '0 0 0';
	self.nextthink = -1;
}

/*FGD
@baseclass = TrainSounds [
	sounds(choices) : "Sound" : 1 =
	[
		0: "Silent/Override"
		1: "Ratchet Metal"
		2: "Base Lift"
	]
]
@baseclass base(Appearflags, Targetname, TrainSounds) = Train [
	speed(integer) : "Speed (units per second)" : 64
	target(target_source) : "Target to start at"
	dmg(integer) : "Damage on block" : 2
	noise(string) : "Override stop sound"
	noise1(string) : "Override move sound"
]
*/

/*QUAKED func_flytrain (0 .5 .8) ?
A func_train that follows a curved flypath and variable speed rules. Inherits movement speed from each path_corner's "speed" key.  
If the train is the target of a button or trigger, it will not begin moving until activated.

"sounds"
0) none, or, set your own with 'noise' (stop sound) and 'noise1' (move sound)
1) ratchet metal
2) base plat
*/
/*FGD
@PointClass base(Train) size(16 16 16) = func_flytrain : 
"A func_train that follows a curved flypath and variable speed rules. Inherits movement speed from each path_corner's 'speed' key." []
*/
void() func_flytrain =
{
	if (!SUB_ShouldSpawn()) return;
	if (!self.speed) {
		self.speed2 = 128;
		self.speed = 128;
	}
	if (!self.target)
		objerror ("func_train without a target");
	if (!self.dmg)
		self.dmg = 2;
	if (self.dmg == -1)
		self.dmg = 0;

	train_sounds();

	self.use = flytrain_init;
	self.th_die = flytrain_stop;
	self.th_stand = SUB_Null;
	
	self.solid = SOLID_BSP;
	self.movetype = MOVETYPE_PUSH;
	self.blocked = train_blocked;
	self.classname = "train";
}

/*QUAKED func_flytrain_point (0 .5 .8) (-8 -8 -8) (8 8 8)
A func_train that follows a curved flypath and variable speed rules. Inherits movement speed from each path_corner's "speed" key.  
If the train is the target of a button or trigger, it will not begin moving until activated.

"sounds"
0) none, or, set your own with 'noise' (stop sound) and 'noise1' (move sound)
1) ratchet metal
2) base plat
*/
/*FGD
@PointClass base(Train) size(16 16 16) = func_flytrain_point : 
"A func_train that follows a curved flypath and variable speed rules. Inherits movement speed from each path_corner's 'speed' key."
[
	model(string) : "Model to use"
]
*/
void() func_flytrain_point =
{	
	if (!SUB_ShouldSpawn()) return;

	func_flytrain();
	
	precache_safe_model(self.model);
	setmodel (self, self.model);
	setsize (self, self.mins, self.maxs);
	setorigin (self, self.origin);
}

//============================================================================

float TRAIN_INHERIT = 1;

void() train_blocked =
{
	if (time < self.attack_finished)
		return;
	self.attack_finished = time + self.dmgtime;
	T_Damage (other, self, self, self.dmg);
}

void() train_use =
{
	if (self.state == STATE_ON)
		return;		// already activated
	train_next();
}

void() train_wait =
{
	// trains now fire their pathcorners' targets on arrival
	SUB_UseEntTargets(self.enemy);
	
	// inherit speed from path_corners on arrival
	if (self.enemy.speed && self.spawnflags & TRAIN_INHERIT)
		self.speed = self.enemy.speed;
	
	if (self.wait == -1)
	{
		self.state = STATE_OFF;
		self.use = train_next;
		sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
	}
	else if (self.wait > 0)
	{
		self.think = train_next;
		self.nextthink = self.ltime + self.wait;
		if (self.wait > 0.1)
			sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
	}
	else 	// self.wait is 0 or a garbage value
	{
		train_next();
	}
}

void() train_next =
{
	local entity	targ;

	//targ = find (world, targetname, self.target);
	targ = findunlockedtarget(world);
	self.enemy = targ;
	self.target = targ.target;
	if (targ.wait)
		self.wait = targ.wait;
	else
		self.wait = self.pausetime;
	if (!self.target && self.wait != -1)	// if wait is forever, it doesn't matter if the path ends
	{
		dprint("train_next: no next target\n");
		remove(self);
		return;
	}
	sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
	self.state = STATE_ON;
	SUB_CalcMove (targ.origin - self.mins, self.speed, train_wait);
}

void() func_train_find =
{
	local entity	targ;

	//targ = find (world, targetname, self.target);
	targ = findunlockedtarget(world);
	self.enemy = targ;
	self.target = targ.target;
	setorigin (self, targ.origin - self.mins);
	if ((self.spawnflags & TRAIN_INHERIT) && targ.speed)
		self.speed = targ.speed;
	//if (!self.targetname)
	if (self.targetname == string_null)
	{	// not triggered, so start immediately
		self.state = STATE_ON;
		self.nextthink = self.ltime + 0.1;
		self.think = train_next;
	}
}

void() func_train_setup =
{
	if (!self.speed)
		self.speed = 100;
	if (!self.target)
		objerror("func_train without a target");
	self.dmg = zeroconvertdefault(self.dmg, 2);
	if (!self.dmgtime)
		self.dmgtime = 0.5;
	self.pausetime = zeroconvertdefault(self.pausetime, 0.1);
	self.wait = 0;	// this is american mcgee's fault

	train_sounds();

//	self.cnt = 1;
	self.solid = SOLID_BSP;
	self.movetype = MOVETYPE_PUSH;
	self.blocked = train_blocked;
	self.use = train_use;
	self.classname = "train";
	self.state = STATE_OFF;

	// start trains on the second frame, to make sure their targets have had
	// a chance to spawn
	self.nextthink = self.ltime + 0.1;
	self.think = func_train_find;

}

/*QUAKED func_train (0 .5 .8) ? PATH_SPEED
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner. The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
A train will fire all targets of a pathcorner on arrival.
Keys:
"speed"	default 100
"dmg"	default	2
"sounds"
	0) none, or, set your own with 'noise' (stop sound) and 'noise1' (move sound)
	1) ratchet metal
	2) base plat
"pausetime"	pausetime at a pathcorner that doesn't specify a wait, default 0.1, -1 for continuous motion

Spawnflags:
PATH_SPEED: train will inherit 'speed' value from path_corners as it reaches them
*/
/*FGD
@SolidClass base(Train) = func_train : "Moving platform.
Origin is min corner of the train.
A train will fire all targets of a pathcorner on arrival." [
	spawnflags(Flags) = [
		1 : "Speed from path" : 0
	]
]
*/
void() func_train =
{	
	if (!SUB_ShouldSpawn()) return;
	func_train_setup();
	
	precache_safe_model(self.model);
	setmodel (self, self.model);
	setsize (self, self.mins , self.maxs);
	setorigin (self, self.origin);
}

/*QUAKED func_train_point (0 .5 .8) (-8 -8 -8) (8 8 8) PATH_SPEED
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner. The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
A train will fire all targets of a pathcorner on arrival.
Keys:
"model" model to use
"speed"	default 100
"dmg"	default	2
"sounds"
	0) none, or, set your own with 'noise' (stop sound) and 'noise1' (move sound)
	1) ratchet metal
	2) base plat
"pausetime"	pausetime at a pathcorner that doesn't specify a wait, default 0.1, -1 for continuous motion

Spawnflags:
PATH_SPEED: train will inherit 'speed' value from path_corners as it reaches them
*/
/*FGD
@PointClass base(Train) = func_train_point : "Moving platform.
Origin is min corner of the train.
A train will fire all targets of a pathcorner on arrival." 
[
	model(string) : "Model to use"
	spawnflags(Flags) = [
		1 : "Speed from path" : 0
	]
]
*/
void() func_train_point =
{	
	if (!SUB_ShouldSpawn()) return;
	func_train_setup();
	
	precache_model (self.model);
	setmodel (self, self.model);
	setsize (self, self.mins, self.maxs);
	setorigin (self, self.origin);
}

// =======================================

void() func_bobbing_blocked = 
{
	T_Damage (other, self, self, self.dmg);
	
	if ((self.spawnflags & 2))	// don't reverse if we're a CRUSHER
		return;
		
	// reversing direction entails reversing the phase
	self.delay = (self.delay * -1) + 1;
	func_bobbing_think();
}

void() func_bobbing_use = 
{
	self.state = !self.state;
	
	
	if (self.state)
	{
		if (self.noise != string_null)	// cut off move sound with stop sound
			sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
		self.nextthink = 0;
		return;
	}
	
	if (self.noise1 != string_null)	// move
		sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
	self.think();
}

vector(float phase) func_bobbing_pos =
{
	vector pos;
	float cosD;
	
	cosD = (1 - cos(360 * phase)) / 2;	// flip -1/1 and scale to 0-1
	pos = self.pos1 + (cosD * (self.pos2 - self.pos1));
	return pos;
}

void() func_bobbing_think = 
{
	vector dst;
	float phase;
	
	// delay = current phase offset (0-1)
	// length = current think rate
	// wait = total wave interval
	phase = self.delay + self.length / self.wait;	// advance the phase
	if ( (self.delay < 0.5 && phase > 0.5) || (self.delay < 1 && phase > 1) )
	{
		// when we pass either apex, repeat the noises
		if (self.noise1 != string_null)	// move
			sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
		if (self.noise != string_null)	// stop
			sound (self, CHAN_BODY, self.noise, 1, ATTN_NORM);
	}
	self.delay = phase;
	while (self.delay > 1)	// wrap phase to 0-1
		self.delay -= 1;
		
	dst = func_bobbing_pos(self.delay);
	self.velocity = (dst - self.origin) / self.length;
	self.nextthink = self.ltime + self.length;
}

/*QUAKED func_bobbing (0 .5 .8) ? START_OFF CRUSHER
bobbatron 2000. does not need a 'speed' key; specify "wait" for the period and the bobbalondo 9000 does the rest. 
trigger to freeze in place or reactivate.

"angle" set bobaxis
"distance" total distance of bob, will move half this far in either direction
"pos1"/"pos2" ignore angle/distance and set apex offsets directly
"wait" wavelength of bob in seconds
"delay" phase offset to start at (0 = pos1, 0.5 = pos2, 1.0 = back at pos1 again)
"sounds"
0) none, or, set your own with 'noise' (stop sound) and 'noise1' (move sound)
1) ratchet metal
2) base plat

Spawnflags
START_OFF	start off
CRUSHER		don't reverse when blocked
*/
/*FGD
@baseclass base(Targetname, TrainSounds) = Bob
[
	spawnflags(flags) = [
		1 : "Start off" : 0
		2 : "Crusher (no reverse)" : 0
	]
	wait(string) : "period of bob" : "4"
	distance(integer) : "length of bob" : 64
	delay(string) : "starting phase offset (0-1)" : "0.0"
	dmg(integer) : "Damage on block" : 2
	noise(string) : "Override stop sound"
	noise1(string) : "Override move sound"
	pos1(string) : "Override apex 1"
	pos2(string) : "Override apex 2"
]
@SolidClass base(Bob) = func_bobbing : "Bobbing platform"  []
*/
void() func_bobbing =
{
	if (!SUB_ShouldSpawn()) return;

	self.solid = zeroconvertdefault(self.solid, SOLID_BSP);
	self.movetype = MOVETYPE_PUSH;

	SetMovedir();
	if (!self.distance)
		self.distance = 64;
	if (self.wait <= 0)
		self.wait = 4;
	
	self.dmg = zeroconvertdefault(self.dmg, 2);
	
	if (!self.pos1 && !self.pos2)
	{
		self.pos1 = self.origin + self.distance * 0.5 * self.movedir;
		self.pos2 = self.origin - self.distance * 0.5 * self.movedir;
	}
	self.use = func_bobbing_use;
	self.blocked = func_bobbing_blocked;
	self.think = func_bobbing_think;
	
	self.length = min(0.1, self.wait / 6);
	
	self.state = self.spawnflags & 1;
	if (!(self.state))
		self.nextthink = self.ltime + self.length;
	
	train_sounds();
	
	precache_model (self.model);
	setmodel (self, self.model);
	setsize (self, self.mins, self.maxs);
	setorigin (self, func_bobbing_pos(self.delay));
}

/*QUAKED func_bobbing_point (0 .5 .8) (-8 -8 -8) (8 8 8) START_OFF CRUSHER 
bobbatron 2000. does not need a 'speed' key; specify "wait" for the period and the bobbalondo 9000 does the rest. 
trigger to freeze in place or reactivate.

"angle" set bobaxis
"distance" total distance of bob, will move half this far in either direction
"pos1"/"pos2" ignore angle/distance and set apex offsets directly
"wait" wavelength of bob in seconds
"delay" phase offset to start at (0 = pos1, 0.5 = pos2, 1.0 = back at pos1 again)
"sounds"
0) none, or, set your own with 'noise' (stop sound) and 'noise1' (move sound)
1) ratchet metal
2) base plat

Spawnflags
START_OFF	start off
CRUSHER		don't reverse when blocked
*/
/*FGD
@PointClass base(Bob) = func_bobbing_point : "Bobbing platform point"  [
	model(string) : "Model to use"
]
*/
void() func_bobbing_point = {func_bobbing();}

// =======================================

void() teleball_think = 
{
	self.think1();
	self.nextthink = time + 0.1;
}

void() teleball_use =
{
	self.avelocity = 50 * Vector(crandom(), crandom(), crandom());
	if (self.think1 == fly_idle)
		fly_newgoal( 1, 0 );
}

void() teleball_init2 =
{
	fly_newgoal( 1, 0 );
	self.think = teleball_think;
	teleball_think();
}

void() teleball_init =
{
	//dprint("in teleball_init\n");
	if (self.spawnflags & 1)
	{
		setmodel (self, "progs/teleport.mdl");
		setsize (self, VEC_ORIGIN, VEC_ORIGIN);	
	}
	
	self.avelocity = 50 * Vector(crandom(), crandom(), crandom());
	self.goalentity = self;		// haaax
	self.use = teleball_use;
	self.think = teleball_init2;
	self.nextthink = time + 0.1;
}

void() misc_teleball_curvy = 
{
	//dprint("in misc_teleball\n");
	setorigin (self, self.origin);
	if (!(self.spawnflags & 1))
	{
		teleball_init();
		setmodel (self, "progs/teleport.mdl");
	}
	setsize (self, VEC_ORIGIN, VEC_ORIGIN);	
	self.movetype = MOVETYPE_NOCLIP;
	
	if (!self.speed) {
		self.speed2 = 100;
		self.speed = 100;
	}
	
	self.use = teleball_init;
	self.th_die = SUB_Remove;
	self.th_stand = SUB_Null;
}


// special linear move function for the dumb teletrain so it doesn't have to be MOVETYPE_PUSH
void() teletrain_go =
{
	float l;
	vector dir;
	
	if (self.spawnflags & 1)
	{
		setmodel (self, "progs/teleport.mdl");
		setsize (self, VEC_ORIGIN, VEC_ORIGIN);	
		self.spawnflags -= 1;
	}

	self.enemy = find(world, targetname, self.target);
	self.target = self.enemy.target;
	
	dir = (self.enemy.origin + self.pos1) - self.origin;
	l = vlen(dir);
	self.velocity = normalize(dir) * self.speed;
	self.nextthink = time + (l / self.speed);
	self.think = teletrain_go;
}

void() teletrain_find =
{
	self.enemy = find(world, targetname, self.target);
	self.target = self.enemy.target;
	setorigin(self, self.enemy.origin + self.pos1);
}

/*QUAKED misc_teleporttrain (0 .5 .8) (-16 -16 -16) (16 16 16) START_INVIS CURVY
This is used for the final boss

Spawnflags
START_INVIS	Invisible until triggered
CURVY	behaves like a func_flytrain, following path_corners, waiting at them (a wait of -1 on a path_corner will stop the ball until triggered again), then disappearing at the last path_corner.  Inherits movement speed from each path_corner's "speed" key. movement is smoothed out so it doesn't move in straight lines between waypoints - sharp turns near walls could make it clip out of the level and disappear for a moment, so place pathcorners carefully.
*/
/*FGD
@PointClass base(Appearflags ) size(16 16 16) model({ "path": ":progs/teleport.mdl" }) = misc_teleporttrain : "Flying teleporter destination"
[
	target(string) : "First stop target"
	targetname(target_source) : "Name" 
	spawnflags(flags) =
	[
		1 : "Start invisible" : 0
		2 : "Curving path" : 0
	]
]
*/
void() misc_teleporttrain =
{	
	if (!SUB_ShouldSpawn()) return;
	
	precache_model2 ("progs/teleport.mdl");
	if (self.spawnflags & 2)
	{
		misc_teleball_curvy();
		return;
	}
	
	if (!self.speed)
		self.speed = 100;
	if (!self.target)
		objerror("misc_teleporttrain without a target");

	self.solid = SOLID_NOT;
	self.movetype = MOVETYPE_FLY;
	self.use = teletrain_go;

	if (!(self.spawnflags & 1))
		setmodel (self, "progs/teleport.mdl");
	setsize (self, self.mins, self.maxs);
	self.pos1 = 0.5 * (self.maxs - self.mins);
	setorigin (self, self.origin);
	
	self.think = teletrain_find;
	self.nextthink = time + 0.1;
}

