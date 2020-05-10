/*
=======================================
FLYMOVE

.distance = estimated total time to reach target
.delay = estimated time left to reach target
delay / distance = % of movement remaining

.speed2 = previous movement speed
.speed = destination movement speed

actual velocity is a lerp from .speed2 to .speed by (delay / distance) so that 
the flying entity eases in and out of waypoints/idles

=======================================
*/

// FIXME: if triggered when waiting at a path_corner in holding pattern mode,
// if current idle drift is away from the next path corner, flying object
// will make an unsightly long arc to get back on track
// sometimes this is desirable but sometimes not (big flytrains)
void() fly_go =
{
	//dprint("in fly_go\n");
	local vector vel, destvel;
	local float frac, lspeed;
	
	destvel = self.dest - self.origin;
	
	lspeed = vlen(self.velocity);
	
	if (vlen(destvel) < (lspeed * 0.15))
	{
		// we'll be past it next think so this is close enough
		// avoids the sprite spazzing out very near the destination
		SUB_UseEntTargets(self.goalentity);	// fire this path_corner's target2/3/4s
		fly_newgoal( 0, 0 );
		destvel = self.dest - self.origin;
	}
	
	destvel = normalize(destvel);
	
	// figure out where we're flitting to
	frac = self.delay / self.distance;

	vel = self.velocity;
	vel = normalize(vel);
	// proportionally mix vectors to gradually steer towards the destination
	vel = vel * frac + destvel * (1 - frac);
	// proportionally mix speeds to accel/decel towards destination speed
	lspeed = self.speed2 * frac + self.speed * (1 - frac);
	
	if (self.delay > 0)
		self.delay = self.delay - 0.1;
	
//	dprint( ftos(lspeed));
//	dprint( "\n");
	
	self.velocity = vel * lspeed;
}

void() fly_idle =
{
	//dprint("in fly_idle," );
	local vector vel, destvel;
	local float frac, lspeed;
	
	destvel = self.dest - self.origin;
	
	if (vlen(destvel) < 4) {
		// close enough
		fly_newgoal( 0, 1 );
		destvel = self.dest - self.origin;
	}
	
	destvel = normalize(destvel);
	
	// figure out where we're flitting to
	frac = self.delay / self.distance;	// frac = % *remaining*
	
	local string what;
	what = ftos(frac);
	//dprint("frac: ");
	//dprint( what );
	//dprint("\n");
	
	vel = self.velocity;
	vel = normalize(vel);
	vel = vel * frac + destvel * (1 - frac);
	lspeed = self.speed2 * frac + self.speed * (1 - frac);
	
	if (self.delay > 0)
		self.delay = self.delay - 0.1;
	else self.delay = 0;
	
	self.velocity = vel * lspeed;
}

void() fly_delayuse =
{
	local entity oldself;
	oldself = self;
	self = self.enemy;
	self.use();
	self = oldself;
	remove(self);
}

void() fly_findnextgoal =
{
	entity oself;
	oself = self;
	self = self.goalentity;
	oself.goalentity = findunlockedtarget(world);
	self = oself;
}

void( float nowait, float idlespeed ) fly_newgoal = 
{
	//dprint("in fly_newgoal\n");

	
	if (self.goalentity.wait && !nowait)
	{
		//dprint("waiting at pathcorner\n");
		self.speed2 = vlen(self.velocity); // self.speed;
		if ( self.classname == "train" )
			self.speed = 8;
		else
			self.speed = 32;
		
		local vector randy;
		randy_x = crandom();
		randy_y = crandom();
		randy_z = crandom();
		if ( self.classname == "train" )
			randy = normalize(randy) * 4;
		else
			randy = normalize(randy) * 12;
		
		self.think1 = fly_idle;
		self.dest = self.goalentity.origin + randy;
		
		if (self.goalentity.wait > 0) {
			local entity t;
			//dprint("waiting here for a few seconds\n");

			// create a temp object to fire at a later time
			t = spawn();
			t.classname = "DelayedUse";
			t.nextthink = time + self.goalentity.wait;
			t.think = fly_delayuse;
			t.enemy = self;
		}
		self.th_stand();
	}
	else
	{
		// get new destination
		if (!self.goalentity.target)
		{
			//dprint("no goalentity target\n");
			if (self.classname != "train")
			{
				self.th_die();
				return;
			}
			SUB_CalcMove(self.goalentity.origin, self.speed, self.th_die);	// lerp straight to the finish point, then die
		}

		//self.goalentity = find(world, targetname, self.goalentity.target);
		fly_findnextgoal();

		if (!self.goalentity) {
			//dprint("no goalentity\n");
			self.th_die();
			return;
		}
		else
			self.think1 = fly_go;
		
		// get new speed from where we are now
		self.speed2 = vlen(self.velocity);//self.speed;
		if (self.speed2 < 32) self.speed2 = 32;
		if (idlespeed)
			self.speed = 32;
		else if (self.goalentity.wait != 0)
			self.speed = 16;	// should be 32 but this makes up for a hard jolt when arriving at a waited pathcorner
								// the math here is really rough
		else if (self.goalentity.speed)
			self.speed = self.goalentity.speed;

		self.dest = self.goalentity.origin;
	}
	// ... really, really rough:
	self.distance = self.delay = 4 * (vlen( self.origin - self.dest ) / (self.speed + self.speed2));	
}
