/*
===========================================
BUTTONS

button and multiple button
for all your button
===========================================
*/

void() button_wait =
{
	self.state = STATE_TOP;
	self.nextthink = self.ltime + self.wait;
	self.think = button_return;
	activator = self.enemy;
	SUB_UseTargets();
	SUB_PrintMessage();
	self.frame = 1;			// use alternate textures
}

void() button_done =
{
	self.state = STATE_BOTTOM;
}

void() button_return =
{
	if (self.customflags & CFL_LOCKED) return;
	self.state = STATE_DOWN;
	SUB_CalcMove (self.pos1, self.speed, button_done);
	self.frame = 0;			// use normal textures
	if (self.health)
		self.takedamage = DAMAGE_YES;	// can be shot again
}


void() button_blocked =
{	// do nothing, just don't ome all the way back out
}


void() button_fire =
{
	if (self.customflags & CFL_LOCKED) return;
	if (self.state == STATE_UP || self.state == STATE_TOP)
		return;

	if (self.noise != string_null)
		sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);

	self.state = STATE_UP;
	SUB_CalcMove (self.pos2, self.speed, button_wait);
}


void() button_use =
{
	if (self.customflags & CFL_LOCKED) return;
	self.enemy = activator;
	
	if (self.state == STATE_UP || self.state == STATE_DOWN)
		return;
	/*
	// this is what target_lock is for
	if (self.state == STATE_TOP)
	{
		if (self.wait < 0)
			button_return();
		return;
	}*/
	
	button_fire ();
}

void() button_touch =
{
	if (self.customflags & CFL_LOCKED) return;
	if (other.classname != "player" || other.health <= 0)
		return;
		
	self.enemy = other;
	button_fire ();
}

void() button_killed =
{
	self.enemy = damage_attacker;
	self.health = self.max_health;
	self.takedamage = DAMAGE_NO;	// wil be reset upon return
	button_fire ();
}

void(entity b) button_force =
{
	b.enemy = self;
	SUB_CallAsSelf(button_fire, b);
}

/*QUAKED func_button (0 .5 .8) ?
When a button is touched, it moves some distance in the direction of its angle, triggers all of its targets, waits some time, then returns to its original position where it can be triggered again.

"angle"		determines the opening direction
"target"	all entities with a matching targetname will be used
"speed"		override the default 40 speed
"wait"		override the default 1 second wait (-1 = never return)
"lip"		override the default 4 pixel lip remaining at end of move
"health"	if set, the button must be killed instead of touched
"sounds"
0) steam metal
1) wooden clunk
2) metallic click
3) in-out
4) trigger noise (big light switch)
*/
/*FGD
@SolidClass base(Angle,Target,Targetname,LipDist) = func_button : 
"Button. When a button is touched, it moves some distance in the direction of its angle, triggers all of its targets, waits some time, then returns to its original position where it can be triggered again.
Acts as if touched when triggered." 
[
	speed(integer) : "Speed" : 40
	health(integer) : "Health (shootable if > 0)"
	lip(integer) : "Lip remaining at end of move" : 4
	distance(string) : "Distance to travel (overrides Lip)" : "0.0"
	sounds(choices) : "Sounds" = 
	[
		0 : "Steam metal"
		1 : "Wooden clunk"
		2 : "Metallic clink"
		3 : "In-out"
		4 : "Trigger noise (big light switch)"
	]
	wait(string) : "Delay before reset" : "1"
	delay(string) : "Delay before trigger"
	message(string) : "Message" 
]
*/
void() func_button =
{
	if (!SUB_ShouldSpawn()) return;
	if (self.sounds == 0)
	{
		precache_sound ("buttons/airbut1.wav");
		self.noise = "buttons/airbut1.wav";
	}
	if (self.sounds == 1)
	{
		precache_sound ("buttons/switch21.wav");
		self.noise = "buttons/switch21.wav";
	}
	if (self.sounds == 2)
	{
		precache_sound ("buttons/switch02.wav");
		self.noise = "buttons/switch02.wav";
	}
	if (self.sounds == 3)
	{
		precache_sound ("buttons/switch04.wav");
		self.noise = "buttons/switch04.wav";
	}
	if (self.sounds == 4)
	{
		precache_sound ("misc/trigger1.wav");
		self.noise = "misc/trigger1.wav";
	}
	
	SetMovedir ();

	self.movetype = MOVETYPE_PUSH;
	self.solid = SOLID_BSP;
	setmodel (self, self.model);

	self.blocked = button_blocked;
	self.use = button_use;

	if (self.health)
	{
		self.max_health = self.health;
		self.th_die = button_killed;
		self.takedamage = DAMAGE_YES;
	}
	else
		self.touch = button_touch;

	if (!self.speed)
		self.speed = 40;
	if (!self.wait)
		self.wait = 1;
	if (!self.lip)
		self.lip = 4;
	if (!self.distance)
		self.distance = fabs(self.movedir*self.size) - self.lip;

	self.state = STATE_BOTTOM;

	self.pos1 = self.origin;
	self.pos2 = self.pos1 + self.movedir * self.distance;
}

