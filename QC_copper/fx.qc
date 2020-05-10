/*
================================================================

FX

================================================================
*/

void(vector org) gunshot =
{
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_GUNSHOT);
	WriteCoord (MSG_BROADCAST, org_x);
	WriteCoord (MSG_BROADCAST, org_y);
	WriteCoord (MSG_BROADCAST, org_z);
}

void(vector a, vector b, entity who) fx_beam =
{
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_LIGHTNING1);
	WriteEntity (MSG_BROADCAST, who);
	WriteCoord (MSG_BROADCAST, a_x);
	WriteCoord (MSG_BROADCAST, a_y);
	WriteCoord (MSG_BROADCAST, a_z);
	WriteCoord (MSG_BROADCAST, b_x);
	WriteCoord (MSG_BROADCAST, b_y);
	WriteCoord (MSG_BROADCAST, b_z);
}


void()	s_explode1	=	[0,		s_explode2] {}
void()	s_explode2	=	[1,		s_explode3] {}
void()	s_explode3	=	[2,		s_explode4] {}
void()	s_explode4	=	[3,		s_explode5] {}
void()	s_explode5	=	[4,		s_explode6] {}
void()	s_explode6	=	[5,		SUB_Remove] {}

void() BecomeBlast = 
{
	self.movetype = MOVETYPE_NONE;
	self.velocity = '0 0 0';
	self.touch = SUB_Null;
	setmodel (self, "progs/s_explod.spr");
	self.solid = SOLID_NOT;
	self.nextthink = time + 0.5;
	self.think = SUB_Remove;
	s_explode1 ();
}

void() BecomeExplosion =
{
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_EXPLOSION);
	WriteCoord (MSG_BROADCAST, self.origin_x);
	WriteCoord (MSG_BROADCAST, self.origin_y);
	WriteCoord (MSG_BROADCAST, self.origin_z);
	BecomeBlast();
}

void() fx_explode =
{
	kaboom(self.origin, 0, self, 0);
}

/*
========
kaboom

for making explosions when and where you please
========
*/
entity(vector org, float damg, entity parent, float multi) kaboom =
{
	local entity boom;
	
	boom = spawn();
	setorigin(boom, org);
	boom.nextthink = time;
	boom.think = BecomeExplosion;
	
	if (damg > 0) T_RadiusMultiDamage (boom, parent, damg, boom, multi);
	
	return boom;
}

void()	shockwave1	=[	0,	shockwave2	] {self.skin = 0; self.nextthink = time + 0.05;}
void()	shockwave2	=[	1,	shockwave3	] {self.skin = 1; self.nextthink = time + 0.05;}
void()	shockwave3	=[	2,	shockwave4	] {self.skin = 2; self.nextthink = time + 0.05;}
void()	shockwave4	=[	3,	shockwave5	] {self.skin = 3; self.nextthink = time + 0.05;}
void()	shockwave5	=[	4,	SUB_Remove	] {self.alpha = 0.6; self.nextthink = time + 0.05;}

void()	shockwave6	=[	5,	shockwave7	] {self.skin = 0; self.nextthink = time + 0.05;}
void()	shockwave7	=[	6,	shockwave8	] {self.skin = 0; self.nextthink = time + 0.05;}
void()	shockwave8	=[	7,	shockwave9	] {self.skin = 1; self.nextthink = time + 0.05;}
void()	shockwave9	=[	8,	shockwave10	] {self.skin = 2; self.nextthink = time + 0.05;}
void()	shockwave10	=[	9,	shockwave11	] {self.skin = 3; self.nextthink = time + 0.05;}
void()	shockwave11	=[	10,	SUB_Remove	] {self.alpha = 0.6; self.nextthink = time + 0.05;}


//=============================================================================

//flagrant abuse of particle()


void(vector a, vector b, float color) fx_trail =
{
	local float i, maxd;
	local vector dir;
	maxd = floor(vlen(b-a));
	dir = normalize(b-a);
	
	for (i=0; i<maxd; i+=8)
		particle(a + dir * i, dir, color, 2);
}

void(vector c, float r, float color) fx_ring =
{
	local float segments, ang, i;
	local vector a, b;
	segments = ceil(sqrt(2 * r));
	
	for (i=0; i<segments; i++)
	{
		ang = (360.0/segments) * i;
		makevectors('0 1 0' * ang);
		a = v_forward * r + c;
		ang = (360.0/segments) * (i + 1);
		makevectors('0 1 0' * ang);
		b = v_forward * r + c;
		fx_trail(a,b,color);
	}
}

//============================================================================

void(float num) particlefield_burst =
{
	vector org, org2, dir;
	while (num > 0)
	{
		num -= 1;
		org = Vector(crandom(), crandom(), crandom());
		org2 = vec_scale(org, self.mangle);
		
		dir = self.movedir;
		if (self.spawnflags & 2)
		{
			// in
			dir -= org * vlen(self.movedir);
		}
		else if (self.spawnflags & 4)
		{
			// out
			dir += org * vlen(self.movedir);
		}
		
		particle (org2 + self.origin, dir, self.health, self.speed2);
	}
}

void() particlefield_think =
{
	float num;
	
	num = floor(0.5 + self.speed / 10);
	
	particlefield_burst(num);
	
	if (self.state)
		self.nextthink = time + 0.1;
}

void() particlefield_use =
{
	if (self.spawnflags & 8)
	{
		particlefield_burst(self.speed);
		return;
	}
	
	self.state = !self.state;
	if (self.state)
		self.nextthink = time + 0.1;
}


/*QUAKED misc_particlefield_box (0 .6 1) (-8 -8 -8) (8 8 8) START_ON INWARD OUTWARD BURST
Makes dots appear continuously within its bounds. Trigger to toggle on or off. 

"mangle" dimensions of sparkly zone
"speed" number of clumps per second
"speed2" number of dots per clump
"health" color index (0-255). quake will use every color in the row (each row is 16 colors)
"movedir" give particles a velocity

Flags:
INWARD		particles are sucked toward origin. additive with movedir.
OUTWARD		particles are blown out from origin. additive with movedir.
BURST		spew one second's worth of particles in one shot when triggered rather than toggling on/off

Color Reference:
0-15 grey (higher = lighter)
16-31 dirt brown (higher = lighter)
32-47 sky blue (higher = lighter)
48-63 grass green (higher = lighter)
64-79 red (higher = lighter)
80-95 green/orange (higher = lighter)
96-111 copper/gold (higher = lighter)
112-127 flesh tone (higher = lighter)
128-143 pale purple (higher = darker)
144-159 reddish purple (higher = darker)
160-175 pale beige (higher = darker)
176-191 turquoise (higher = darker)
192-207 yellow (higher = darker)
208-223 deep blue (higher = darker)
224-239 fire/lava (higher = lighter)
240-255 fullbright mess
*/
/*FGD
@Pointclass base(Targetname, Appearflags, Bounds) color(0 160 255) size(16 16 16) = misc_particlefield_box : 
"Makes dots appear continuously within its bounds. Trigger to toggle on/off.

Color Reference:
0-15 grey (higher = lighter)
16-31 dirt brown (higher = lighter)
32-47 sky blue (higher = lighter)
48-63 grass green (higher = lighter)
64-79 red (higher = lighter)
80-95 green/orange (higher = lighter)
96-111 copper/gold (higher = lighter)
112-127 flesh tone (higher = lighter)
128-143 pale purple (higher = darker)
144-159 reddish purple (higher = darker)
160-175 pale beige (higher = darker)
176-191 turquoise (higher = darker)
192-207 yellow (higher = darker)
208-223 deep blue (higher = darker)
224-239 fire/lava (higher = lighter)
240-255 fullbright mess" 
[
	spawnflags(flags) = [
		1 : "Start on" : 0
		2 : "Particles are sucked inward" : 0
		4 : "Particles are blown outward" : 0
		8 : "Oneshot burst when triggered" : 0
	]
	speed(integer) : "Clumps per second" : 10
	speed2(integer) : "Dots per clump" : 16
	health(integer) : "Color index (0-255)" : 0
	movedir(string) : "Velocity of particles (x y z)"
]
*/
void() misc_particlefield_box =
{
	if (!SUB_ShouldSpawn()) return;
	self.state = self.spawnflags & 1;
	self.think = particlefield_think;
	self.use = particlefield_use;
	
	if (!self.speed)
		self.speed = 10;
	if (!self.speed2)
		self.speed2 = 16;
		
	if (self.state)
		self.nextthink = time + 0.1;
	
	// innie and outie cancel
	if (self.spawnflags & 2 && self.spawnflags & 4)
		self.spawnflags -= 6;
	
	self.mangle_x *= 0.5;
	self.mangle_y *= 0.5;
	self.mangle_z *= 0.5;
}


//=============================================================================


void(entity targ, float mag) screenshake_go =
{
	mag = mag + 1;
	
	// punchangle doesn't work when we're in an intermission camera, so do
	// this with actual angle offsets when intermission_running > 0
	if (intermission_running)
	{
		local vector kick;
		kick_x = (random()- 0.5) * mag;
		kick_y = (random()- 0.5) * mag;
		kick_z = (random()- 0.5) * mag * 2;
		targ.angles = targ.finalangle + kick;
		targ.angles_z = kick_z;
		targ.fixangle = 1;
		return;
	}	
	
/*	local vector kick;
	kick_x = (-0.5 + random()) * mag;
	kick_y = (-0.5 + random()) * mag;
	kick_z = (-0.5 + random()) * mag;
	targ.angles = targ.v_angle + kick;
	targ.angles_z = kick_z;
	targ.fixangle = 1;		// turn this way immediately*/

	targ.punchangle_x = random() * mag * 2 - mag;
	targ.punchangle_y = random() * mag * 2 - mag;
	
	// punchangle seems to get integerized, so we need some kind of
	// activity when the magnitude is low so it feels like a faint 
	// rumble instead of just a dying fish kicking occasionally
	mag = mag * 2;	// the rolls feel a lot less violent, so bump them up a little
	
	targ.punchangle_z = random() * mag * 2 - mag;
}

void() screenshake_think =
{
	local entity t;
	local float mag;
	
	if (time > self.pain_finished)
	{
		t = find( world, classname, "player" );
		while (t)
		{
			t.v_angle_z = 0;	// unroll
			t = find( t, classname, "player" );
		}
		return;
	}
	
	if (time < self.super_time + self.delay)
	{
		mag = 1 - ((self.super_time + self.delay) - time) / self.delay;
	}
	else if (time < self.super_time + self.delay + self.length)
	{
		mag = 1;
	}
	else if (time < self.pain_finished)
	{
		mag = (self.pain_finished - time) / self.wait;
	}
	else mag = 0;
	
	mag = mag * mag;	// power of 2 falloff seems smoother than linear
	
	t = find( world, classname, "player" );
	while (t)
	{
//		bprint(ftos(self.strength * mag));
//		bprint("\n");
		screenshake_go(t, self.strength * mag);
		t = find( t, classname, "player" );
	}

	self.nextthink = time + 0.05;
}

void() screenshake_use =
{
	self.pain_finished = time + self.wait + self.delay + self.length;
	self.super_time = time;
	self.nextthink = time + 0.05;
	self.think = screenshake_think;
}


/*QUAKED target_screenshake (0 .5 .8) (-8 -8 -8) (8 8 8)
Shakes the screen. Jostles the view of all clients, but doesn't physically move their bounding boxes or apply velocities.
Keys
"length" Duration of full intensity quake
"delay" Time to ramp up quake intensity from 0
"wait" Time to ramp down quake intensity to 0. Total quake duration is length + delay + wait.
"strength" Intensity of quake. 1 is a hearty rumble, 10 is spasmodic bordering on hilarious.
*/
/*FGD
@Pointclass base(Targetname, Appearflags) color(0 128 224) size(16 16 16) = target_screenshake : 
"Shakes the screen. Jostles the view of all clients, but doesn't physically move their bounding boxes or apply velocities.
Total shake duration is length + delay + wait." 
[
	length(string) : "Duration of full intensity (sustain)"
	delay(string) : "Time to ramp up from 0 (attack)"
	wait(string) : "Time to ramp down quake intensity to 0."
	strength(string) : "Intensity of quake. 1 is a hearty rumble, 10 is spasmodic bordering on hilarious."
]
*/
void() target_screenshake =
{
	if (!SUB_ShouldSpawn()) return;
	self.wait = max(0, self.wait);
	self.delay = max(0, self.delay);
	self.length = max(0, self.length);
	
	if (!self.delay && !self.length && !self.wait)
		dprint("misc_screenshake will last 0 seconds\n");
	
	if (!self.strength)
	{
		if (self.volume)
			objerror("convert target_screenshake to .strength please");
		
		self.strength = 10;	// set your own value goddamnit
	}
	self.use = screenshake_use;
}


void(float attack, float sustain, float release, float str) screenshake_oneshot =
{
	entity shake, oself;
	
	shake = spawn();
	
	shake.delay = attack;
	shake.length = sustain;
	shake.wait = release;
	shake.strength = str;
	
	oself = self;
	self = shake;
	
	target_screenshake();
	screenshake_use();
	
	self = oself;
}

//============================================================================

void() target_explosion_go =
{
	if (self.dmg) T_RadiusMultiDamage (self, self, self.dmg, self, 0);
	BecomeExplosion();
}

void() target_explosion_use =
{
	float i;
	entity bewm;
	
	if (self.count == 1 && self.delay == 0)
	{
		kaboom(self.origin, self.dmg, self, 0);
		return;
	}
	
	for (i=0;i<self.count;i++)
	{
		bewm = spawn();
		setorigin(bewm, self.origin);
		bewm.think = target_explosion_go;
		bewm.nextthink = time + self.delay + self.wait * i;
		bewm.dmg = self.dmg;
		bewm.deathtype = self.deathtype;
	}
}

/*QUAKED target_explosion (0 .5 .8) (-8 -8 -8) (8 8 8) 
Causes explosions, same appearance as a rocket/grenade blast.

Keys
"count" number of times to explode after triggering, default 1
"delay" Time to wait before first explode
"wait" Time to wait between explosions
"dmg" Do splash damage, default 0
"deathtype" death message if the player is killed by your horrible trap
*/
/*FGD
@Pointclass base(Targetname, Appearflags, Deathtype) color(0 128 224) size(16 16 16) = target_explosion : 
"Causes explosions, same appearance as a rocket/grenade blast." 
[
	count(integer) : "Repeat count" : 1
	delay(string) : "Delay before first blast" : "0"
	wait(string) : "Wait between blasts" : "1"
	dmg(integer) : "Splash damage" : 0
]
*/
void() target_explosion =
{
	if (!SUB_ShouldSpawn()) return;
	self.count = zeroconvertdefault(self.count, 1);
	self.use = target_explosion_use;
	if (!self.wait)
		self.wait = 1;
	if (self.deathtype == string_null)
		self.deathtype = "joins the smithereens";
}

//============================================================================

void() target_telefog_use =
{
	float i;
	entity tfog;
	
	if (self.count == 1 && self.delay == 0)
	{
		teleport_flash();
		return;
	}
	
	for (i=0;i<self.count;i++)
	{
		tfog = spawn();
		setorigin(tfog, self.origin);
		tfog.think = teleport_flasher;
		tfog.nextthink = time + self.delay + self.wait * i;
	}
}

/*QUAKED target_telefog (0 .5 .8) (-8 -8 -8) (8 8 8) 
Causes a flash of teleport fog.

Keys
"count" number of times to flash after triggering, default 1
"delay" Time to wait before first flash
"wait" Time to wait between flashes
*/
/*FGD
@Pointclass base(Targetname, Appearflags) color(0 128 224) size(16 16 16) = target_telefog : 
"Causes a flash of teleport fog." 
[
	count(integer) : "Repeat count" : 1
	delay(string) : "Delay before first flash" : "0"
	wait(string) : "Wait between flashes" : "1"
]
*/
void() target_telefog =
{
	if (!SUB_ShouldSpawn()) return;
	self.count = zeroconvertdefault(self.count, 1);
	self.use = target_telefog_use;
	if (!self.wait)
		self.wait = 1;
}




//============================================================================

//	bubbas

entity(vector org) bubble_spawn =
{
	entity	bubble;

	bubble = spawn();
	setmodel (bubble, "progs/s_bubble.spr");
	setorigin (bubble, org);
	bubble.movetype = MOVETYPE_NOCLIP;
	bubble.solid = SOLID_NOT;
	setsize (bubble, '-8 -8 -8', '8 8 8');
	bubble.velocity = '0 0 18';
	
	bubble.nextthink = time + 0.25;
	bubble.think = bubble_bob;
	bubble.touch = SUB_RemoveSoon;
	bubble.classname = "bubble";
	if (random() < 0.2)
	{
		bubble.frame = 1;
		bubble.cnt = 11;
	}
	else
	{
		bubble.frame = 0;
		bubble.cnt = 0;
	}
	return bubble;
}

void() air_bubbles_use =
{
	self.state = !(self.state);
	self.think = make_bubbles;
	if (self.state)
	{
		self.nextthink = 0;
		return;
	}
	self.nextthink = time + random() * self.rand + self.wait;
}

/*QUAKED air_bubbles (0 .5 .8) (-8 -8 -8) (8 8 8) START_OFF
air bubble emitter. always bubbles.
"wait" bubble interval, default 1
"rand" random extra interval, default 0
*/
/*FGD
@PointClass base(Appearflags, Targetname) color(0 150 220) = air_bubbles : "Air bubbles" 
[
	spawnflags(flags) = [
		1 : "Start off" : 0
	]
	wait(string) : "Bubble interval" : "0.5"
	rand(string) : "Random extra interval" : "1"
]
*/
void() air_bubbles =
{
	if (!SUB_ShouldSpawn()) return;
	if (deathmatch)
	{
		remove (self);
		return;
	}
	precache_model ("progs/s_bubble.spr");
	self.wait = zeroconvertdefault(self.wait, 1);
	self.rand = zeroconvertdefault(self.rand, 1);
	
	self.think = make_bubbles;
	if (self.targetname != string_null)
	{
		self.use = air_bubbles_use;
		self.state = self.spawnflags & 1;
		if (self.state)
			return;
	}
	self.nextthink = time + self.wait;
}

void() make_bubbles =
{
	if (self.state)
		return;
	bubble_spawn(self.origin);
	self.nextthink = time + random() * self.rand + self.wait;
	self.think = make_bubbles;
}

void() bubble_split =
{
	entity	bubble;
	bubble = bubble_spawn(self.origin);
	bubble.frame = 1;
	bubble.cnt = floor(9 + random()*4);
	bubble.velocity = self.velocity + '0 0 2';
	self.frame = 1;
	self.cnt = 10;
	if (pointcontents(self.origin + '0 0 8') != CONTENT_WATER)
		remove (self);
}

void() bubble_remove =
{
	remove(self);
}

void() bubble_bob =
{
	float		rnd1, rnd2, rnd3;

	if (pointcontents(self.origin + '0 0 8') != CONTENT_WATER)
	{
		SUB_Remove();
		return;
	}
	
	self.cnt = self.cnt + 1;
	if (self.cnt == 10)
		bubble_split();
	if (self.cnt >= 20 + random()*5)
		remove(self);

	rnd1 = self.velocity_x + crandom() * 20;
	rnd2 = self.velocity_y + crandom() * 20;
	rnd3 = self.velocity_z + crandom() * 15;

	if (rnd1 > 10)
		rnd1 = 5;
	if (rnd1 < -10)
		rnd1 = -5;
		
	if (rnd2 > 10)
		rnd2 = 5;
	if (rnd2 < -10)
		rnd2 = -5;
		
	if (rnd3 < 20)
		rnd3 = 25;
	if (rnd3 > 40)
		rnd3 = 35;
	
	self.velocity_x = rnd1;
	self.velocity_y = rnd2;
	self.velocity_z = rnd3;
		
	self.nextthink = time + 0.25;
	self.think = bubble_bob;
}