/*
=============
THE VOID

The bottoms of pits have to be lit so scrags are lit, but then you can see their bodies at 
the bottom along with heads and shit.  Dead things are problematic to remove since they're 
all SOLID_NOT and don't touch triggers, and trying to undo that requires hacky stuff like 
thinking trigger bodies, and if you cover the bottom with a func static to just hide them 
inside it, the player still sees all the garbage when he falls down there and dies himself.

thus, a func_void:
- Sets itself as an invisible trigger that kills whatever touches it
- Creates a nonsolid 'ghost object' 32 units higher using its own brushmodel that hides 
	accumulated garbage
- Creates another trigger a specified height above itself that fades out players' view 
	when they fall through it

A player's screen has fully faded to black by the time he hits the ghost, so he doesn't
see the trash underneath.  Since he doesn't know he's not still falling, we can hold him 
in stasis for a little while longer before we splat him, to imply the pit is deeper than 
it really is (often not that deep).

===============
*/

/*
============
TintScreen
============
*/
void(entity who, float amt) TintScreen
{
//	bprint(ftos(amt)); bprint("\n");
	stuffcmd(who, "\nv_cshift 0 0 0 ");
	stuffcmd(who, ftos(amt));
	stuffcmd(who, "\n");
}


/*
============
TintScreenColor
============
*/
void(entity who, float amt, vector color) TintScreenColor
{
	// cshifts are 0-255
	if (color_x <= 1 && color_y <= 1 && color_z <= 1)
		color *= 255;
	color_x = ceil(color_x);
	color_y = ceil(color_y);
	color_z = ceil(color_z);
	
	// ugh:
	stuffcmd(who, "\nv_cshift ");
	stuffcmd(who, ftos(color_x));
	stuffcmd(who, " ");
	stuffcmd(who, ftos(color_y));
	stuffcmd(who, " ");
	stuffcmd(who, ftos(color_z));
	stuffcmd(who, " ");
	stuffcmd(who, ftos(amt));
	stuffcmd(who, "\n");
}

// --------------------------------
// void_unplunge
// player was rescued, somehow, probably by being a noclipping whore
// --------------------------------
void(entity e) void_unplunge =
{
	e.customflags = not(e.customflags, CFL_PLUNGE);
	SUB_CallAsSelf(W_ResetWeaponState, e);	// restore weaponmodel
	TintScreen(e,0);
}

// --------------------------------
// void_salvation
// sometimes we don't want to instantly murder the player for minor navigational errors
// --------------------------------
float(entity o) void_salvation =
{
	if (!self.buddy)
		return FALSE;
	
	// this func_void targets a teledest, so it's only for light punishment

	if ( (ArmorSave(other, self.dmg) + other.health - 1 > self.dmg) ||
		other.customflags & CFL_UNDYING ||
		other.flags & FL_GODMODE ||
		other.items & IT_INVULNERABILITY )
	{
		void_unplunge(o);
		teleport(o, self.buddy, TRUE, FALSE);	
		T_Damage(o, self, self, self.dmg);
		o.velocity = '0 0 0';
		sound (o, CHAN_VOICE, "player/land2.wav", 1, ATTN_NORM);
		return TRUE;
	}
	// light punishment is still lethal, proceed with the display
	return FALSE;
}

// --------------------------------
// void_func_touch
// player hit the physical bottom of the pit but may not have hit the implied bottom
// --------------------------------
void() void_func_touch =
{
	if ( other.solid == SOLID_BSP || other.solid == SOLID_TRIGGER )
		return;
	
	if ( CheckValidTouch() )	// a living player not in noclip
	{
		if (void_salvation(other))
			return;

		other.customflags = not(other.customflags, CFL_UNDYING);
		other.flags = not(other.flags, FL_GODMODE);
		
		// a pent will not save you from THE LEVEL DESIGNER
		if (other.items & IT_INVULNERABILITY || other.invincible_finished > time)
		{
			other.items = other.items - (other.items & IT_INVULNERABILITY);
			other.invincible_finished = 0;
			other.invincible_time = 0;
		}

	//	other.customflags = not(other.customflags, CFL_UNDYING);
	//	other.customflags = not(other.customflags, CFL_PLUNGE);
		
		TintScreenColor(other,255,self.fadecolor);
		other.velocity = '0 0 0';
		
		other.solid = SOLID_NOT;
		
		other.movetype = MOVETYPE_NONE;
		other.flags = other.flags | FL_NOTARGET;
		
		other.weaponmodel = string_null;
		other.model = string_null;
		
		if (!(other.customflags & CFL_PLUNGE))	
		{
			// this is non-optimal - means the player got in under a plunge trigger somehow.
			// can happen if the pit is wider at the top than the bottom, and the func_void
			// was made only as wide as the bottom
			other.armorvalue = 0;
			if (!void_salvation(other))		// maybe save him anyway?
				T_Damage(other, self, self, 5000);	// or just blast him now and forget about it
			return;
		}
		
		other.takedamage = DAMAGE_NO;
		return;
	}
	else if (other.classname != "player")	// not a living player who IS in noclip
	{
		if (other.takedamage)
		{
			other.customflags |= CFL_PLUNGE;	// set this so Killed() doesn't throw gibs
			//T_Damage(other, self, other.enemy, other.health + 10);
			if (other.enemy.classname == "player")
				Killed(other,other.enemy);
			else if (other.oldenemy.classname == "player")
				Killed(other,other.oldenemy);
			// attacker must be the monster's enemy, so that players still count as the 
			// killer if they boosh monsters into a pit
			return;
		}

		// anything else just vanishes
		SUB_CallAsSelf(SUB_Remove,other);
	}
}


// --------------------------------
// void_hitbottom
// player hit the implied fake bottom (ie fell for 'delay' seconds), time to die
// --------------------------------
void() void_fall_hitbottom =
{
	
	self.enemy.takedamage = DAMAGE_YES;
	self.enemy.armorvalue = 0;
	T_Damage(self.enemy, self, self, 5000);
	
	SUB_RemoveSoon();
}


void() void_fall_unplunge =
{
	void_unplunge(self.enemy);
}

// --------------------------------
// void_fallthink
// timer for fading out the player's view as he's falling
// --------------------------------
void() void_fallthink =
{
	local float plummet;
	
	if ( self.enemy.origin_z > self.oldorigin_z )
	{
		// player rose back up out of the pit, mostly convenience for noclipping around
		void_fall_unplunge();
		SUB_RemoveSoon();
		return;
	}
	
	if ( self.enemy.movetype == MOVETYPE_NONE )	// has touched the func
	{
		if (self.pain_finished <= time)
		{
			void_fall_hitbottom();
		}
		else
		{
			self.think = void_fall_hitbottom;
			self.nextthink = self.pain_finished;
		}
		return;
	}
	
	plummet = (self.enemy.origin_z - self.lip) / self.height;
	plummet = 1 - plummet * plummet;	// exponential falloff seems to look more linear, since the player accelerates exponentially
	plummet = min( ceil( plummet * 255 ), 255);
	
	TintScreenColor(self.enemy,plummet,self.fadecolor);
	
	self.nextthink = time + 0.0333;
}


// --------------------------------
// void_trigger_touch
// the is-now-irretrievably-falling trigger
// --------------------------------
void() void_trigger_touch =
{
	if (!CheckValidTouch()) return;
	
	if (other.customflags & CFL_PLUNGE) return;	// don't touch the trigger a bunch of times on the way down
	// we could set a refire time on the trigger but players in coop could go over the edge simultaneously
	
	if (time > self.attack_finished)
	{
		activator = other;
		SUB_UseTargets();
		self.attack_finished = time + 0.5;
	}
	
	//bprint("waaa\n");
	sound (self, CHAN_VOICE, "player/q3fall.wav", 1, ATTN_NONE);
	
	other.customflags = other.customflags | CFL_PLUNGE;
	
	other.weaponmodel = string_null;
	
	local entity fall;
	
	fall = spawn();
	fall.classname = "trigger_void";
	fall.deathtype = self.deathtype;
	fall.nextthink = time + 0.05;
	fall.think = void_fallthink;
	fall.enemy = other;
	fall.height = self.height;
	fall.oldorigin = other.origin;	// track player's descent
	fall.lip = other.origin_z - self.height; // z at bottom
	fall.pain_finished = time + self.wait;
	fall.fadecolor = self.fadecolor;
}


void() void_find_teleports =
{
	if (self.target == string_null)
		return;

	entity head;
	head = world;
	while (1)
	{
		head = find(head, targetname, self.target);
		if (!head) return;
		if (head.classname == "info_teleport_destination")
			self.buddy = head;
	}
}


/*QUAKED func_void (.5 .2 0) ?
for holes into which one should not fall

Texture this with black (or sky?) and cover the bottom of the pit with it.  Automatically offsets its own visual model upward, to hide pesky heads and scrag corpses underneath.  Also automatically spawns a start-of-plunge trigger of the same size "height" units above itself that plays a yell sound and fades out the view of clients as they start to fall in.

Keyvalues
"delay" how long the player should 'fall' before the splat, default 3
"height" how high above the func_void the plunge trigger should spawn. do not make this high enough that players will touch it without falling in or they'll hear random screaming noises as they walk around and that'll be strange.
"lip" distance to raise the visual model, default 32
"fadecolor" color to fade the screen to while falling, default black

Fires all "target"s/"killtarget"s when a client touches the plunge trigger (ie the yell, not the splat).
If the func_void's target is an info_teleport_destination, it will hurt the player for "dmg" damage and teleport him to the destination. If "dmg" is enough to kill the player when he falls in, it continues with the fade and splat as normal.
*/
/*FGD
@SolidClass base(Target, Appearflags) color(128 50 0) = func_void : 
"for holes into which one should not fall
Texture this with black (or sky?) and cover the bottom of the pit with it.  Automatically offsets its own visual model upward, to hide pesky heads and scrag corpses underneath.  Also automatically spawns a start-of-plunge trigger of the same size 'height' units above itself that plays a yell sound and fades out the view of clients as they start to fall in.
Fires all targets when a client touches the plunge trigger (ie the yell, not the splat).
If the func_void's target is an info_teleport_destination, it will hurt the player and teleport him to the destination. If the damage is enough to kill the player, it continues with the fade and splat as normal without teleporting."
[
	delay(string) : "how long the player should 'fall' before the splat" : "3"
	height(integer) : "how high above the func_void the plunge trigger should spawn. do not make this high enough that players will touch it without falling in or they'll hear random screaming noises as they walk around and that'll be strange."
	lip(integer) : "distance to raise the visual model" : 32
	fadecolor(string) : "color to fade the screen to while falling" : "0 0 0"
]
*/
void() func_void =
{
	if (!SUB_ShouldSpawn()) return;
	entity trig, ghost;
	vector offset;
	
	precache_sound3("player/q3fall.wav");
	precache_sound3("player/splat1.wav");
	
	// set own model to link into world, then set to null to be solid but invisible
	self.solid = SOLID_TRIGGER;
	self.movetype = MOVETYPE_NONE;
	setmodel (self, self.model);
	self.touch = void_func_touch;
	
	// recycle the same model on a nonsolid entity 32 units higher to hide detritus
	if (!self.lip) self.lip = 32;
	offset = VEC_UP * self.lip;
	ghost = spawn();
	ghost.solid = SOLID_NOT;
	ghost.movetype = MOVETYPE_NONE;
	setorigin(ghost, offset);
	setmodel (ghost, self.model);
	ghost.modelindex = self.modelindex;
	ghost.classname = "void_ghost";

	self.model = string_null;
	
	if (!self.height) self.height = 128;	// height above func to spawn plunge trigger
	if (!self.delay) self.delay = 3;		// time to spend "falling" after touching plunge trigger
	
	// wait for other things to spawn
	self.think = void_find_teleports;
	self.nextthink = time + 0.1;
	if (!self.dmg)
		self.dmg = 60;
	
	// create another solid entity with the same bounds with solid_trigger as a catch on the way down
	trig = spawn();
	trig.movetype = MOVETYPE_NONE;
	trig.solid = SOLID_TRIGGER;
	trig.owner = self;
	trig.buddy = self.buddy;
	trig.touch = void_trigger_touch;
	trig.height = self.height;
	trig.wait = self.delay;	// if trigger has 'delay' field it adds a delay to its own usetargets
	trig.deathtype = self.deathtype;
	trig.fadecolor = self.fadecolor;
	
	SUB_CopyTargets(trig);

	offset = VEC_UP * self.height;
	
	setsize (trig, self.mins + offset, self.maxs + offset);
}