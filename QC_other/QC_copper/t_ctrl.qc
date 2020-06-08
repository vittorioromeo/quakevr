/*
================================================================

CONTROL FLOW

All the relays, counters, and other gates that manipulate the firing
of other entities' .use() functions

including entities that aren't real triggers but are named trigger_*

================================================================
*/

void() trigger_relay_msg =
{
	activator = self.enemy;
	SUB_PrintMessage();
	
	if (self.noise)
		sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);

	if (self.spawnflags & 1)
		self.message = string_null;
}

void() trigger_relay_use =
{
	if (self.customflags & CFL_LOCKED)
	{
		dprint("  trigger_relay is locked: not relaying\n");
		return;
	}
	
	self.enemy = activator;
	
	if (self.spawnflags & 2)
	{
		SUB_CountTargets();
		//dprint2(ftos(self.cnt), " targets left to fire\n");
		SUB_UseRandomTarget();
	}
	else SUB_UseTargets();
	
	// separate from usetargets because of relays and triggers with odd messages all over quake
	if (self.delay)
	{
		self.think = trigger_relay_msg;
		self.nextthink = time + self.delay;
	}
	else
		trigger_relay_msg();
	
	if (self.count > 0)
	{
		self.count -= 1;
		if (self.count == 0)
			remove(self);
	}
}


/*QUAKED trigger_relay (.5 .0 .5) (-8 -8 -8) (8 8 8) MSG_ONCE RANDOM KILL_LAST
This fixed size trigger cannot be touched, it can only be fired by other events.  Target this with a target_lock to selectively disable/reenable it for logic fun.

Keys:
"delay" delay before firing (after trigger)
"killtarget" removes target
"message" displayed when fired
"target/2/3/4" fires target when triggered
"targetname"  entity name (required)
"sounds" same as other triggers, plays after 'delay'
"noise" or, pick any wav
"count" only fire this many times, then auto self-destruct

Spawnflags:
"MSG_ONCE"	only print its message on the first firing, otherwise it prints every time
"RANDOM"	randomly fire one of its targets (and/or kill one of its killtargets) instead of them all
"KILL_LAST"	will only kill its killtarget(s) if no entities can be found to trigger
*/
/*FGD
@PointClass base(AppearFlags, TriggerSounds, Target, Targetname) = trigger_relay : "Trigger: Relay"
[
	spawnflags(flags) =	[
		1 : "Only print message once" : 0
		2 : "Choose one target at random" : 0
		4 : "Only killtarget if no targets left" : 0
	]
	message(string) : "Print when triggered"
	count(integer) : "Limit uses" : 0
]
*/
void() trigger_relay =
{
	if (!SUB_ShouldSpawn()) return;
	if (self.targetname == string_null)
	{
		// IW fix - no longer objerror, which is a game-ender in old engines
		dprint3("WARNING: trigger_relay at ", vtos(self.origin), "not targeted by anything");
		remove(self);
		return;
	}
		
	self.use = trigger_relay_use;
	InitTriggerSounds();
}


//=============================================================================

void() target_state_use =
{
	if (self.customflags & CFL_LOCKED)
		return;

	if (self.spawnflags & 1)
	{
		entity inc;
		inc = find(world, targetname, self.include);
		if (inc)
		{
			if (inc.state == self.state || (self.spawnflags & 2 && inc.state != self.state) )
				SUB_UseTargets();
		}			
		return;
	}
	
	float mx = max(1, self.count);
	if (self.spawnflags & 2)
	{
		self.state = self.state - 1;
		if (self.state < 0) self.state = mx;
	}
	else
	{
		self.state = self.state + 1;
		if (self.state > mx) self.state = 0;
	}
}

/*QUAKED target_state (.5 .0 .5) (-8 -8 -8) (8 8 8) RELAY INVERSE
Numeric variable. set 'state' to initial value. triggering this will cycle the value (swapping 0 and 1).

If "RELAY" is checked, it acts as a relay instead of holding state. It fires its targets based on the value of the 'state' member of another entity, specified by setting 'state' to the value it has to match and 'include' to that entity's targetname. This can be another target_state, or any other entity which has a 'state', such as doors or plats.
	0: off (lights/etc)/top (plats)/open (doors)/pushed (buttons)
	1: on (lights/etc)/bottom (plats)/closed (doors)/unpushed (buttons)
	2: going up (plats)/opening (doors)/pushing (buttons)
	3: going down (plats)/closing (doors)/unpushing (buttons)
A trigger_counter's 'state' is its current trigger count.

Keys:
"count" if > 1, state will be incremented on each trigger to a maximum of 'count' before cycling to 0
"delay" delay before firing (after trigger)
"target2/3/4/kill" targets to fire if RELAY

Spawnflags:
"INVERSE"	if RELAY, will fire if state is anything other than a match. otherwise, will decrement when triggered instead of increment
*/
/*FGD
@PointClass base(AppearFlags, Target, Targetname) = target_state : "Target : State. Holds a numeric variable, or tests other entities for theirs before firing targets."
[
	spawnflags(flags) =	[
		1 : "Relay" : 0
		2 : "Inverse" : 0
	]
	include(target_destination) : "Entity to monitor"
	count(integer) : "Max state to cycle to"
	state(integer) : "State to monitor"
]
*/
void() target_state =
{
	if (!SUB_ShouldSpawn()) return;
	if (self.targetname == string_null)
		objerror("target_state not targeted by anything");
	if (self.spawnflags & 1 && self.include == string_null)
		objerror("target_state relay has no include");

	self.use = target_state_use;
}

//=============================================================================

void(string tname) target_lock_do =
{
	if (tname == string_null) return;

	local entity t, c;
	t = world;
	
	do	{
		t = find (t, targetname, tname);
		if (!t) break;
		
		// outer loop finds doors targeted directly
		if (t.classname == "func_door")
		{
			if (self.state) door_unlock(t);
			else door_lock(t);
			// inner loop checks for doors that are linked by touch and not targetname
			c = t.enemy;
			do {
				if (c.targetname != t.targetname)	// outer loop will find this one
					if (self.state) door_unlock(t);
					else door_lock(t);
				c = c.enemy;
			} while (c != t);
		}
		else if (t.classname == "func_plat")
		{
			if (self.state) plat_unlock(t);
			else plat_lock(t);
		}
		else //if (t.classname == "trigger_relay" || t.classname == "func_button")
		{
			if (self.state)
			{
				t.customflags = not(t.customflags, CFL_LOCKED);
				dprint4(t.classname, " with targetname ", t.targetname, " is unlocked\n");
			}
			else
			{
				t.customflags |= CFL_LOCKED;
				dprint4(t.classname, " with targetname ", t.targetname, " is locked\n");
			}
		}
	} while ( t );
}

void() target_lock_use =
{
	//bprint("in target lock use\n");
	target_lock_do(self.target);
	target_lock_do(self.target2);
	target_lock_do(self.target3);
	target_lock_do(self.target4);
	
	if (self.spawnflags & 4) self.state = 1;
	else if (self.spawnflags & 2) self.state = 0;
	else self.state = 1 - self.state;
}

/*QUAKED target_lock (0 .5 .8) (-8 -8 -8) (8 8 8) START_LOCKED LOCK_ONLY UNLOCK_ONLY
Locks and unlocks targeted entities independent of their toggle/start_open/etc states.  Works on various entities:
- A locked func_door won't respond to touches or triggers. Use this for doors behind temporary bars.
- A locked func_plat behaves as an unreleased LOW_TRIGGER plat
- A locked trigger_relay will not fire or evaluate at all
- Locked path_corners are not considered valid patrol/train destinations
- Locked trigger volumes (once/multi/teleport/secret/changelevel/push/monsterjump) will not activate when touched or triggered

Spawnflags:
"start_locked"	Targeted entities start out locked. 
"lock_only"		Will only lock its targets rather than toggling
"unlock_only"	Will only unlock its targets rather than toggling
*/
/*FGD
@Pointclass base(Target, Targetname, Appearflags) color(192 64 0) size(16 16 16) = target_lock : 
"Locks and unlocks targeted entities independent of their toggle/start_open/etc states.  Works on various entities:
- A locked func_door won't respond to touches or triggers. Use this for doors behind temporary bars.
- A locked func_plat behaves as an unreleased LOW_TRIGGER plat
- A locked trigger_relay will not fire or evaluate at all
- Locked path_corners are not considered valid patrol/train destinations
- Locked trigger volumes (once/multi/teleport/secret/changelevel/push/monsterjump) will not activate when touched or triggered"
[
	spawnflags(flags) = [
		1: "Targeted entities start locked" : 0
		2: "Lock only" : 0
		4: "Unlock only" : 0
	]
]
*/
void() target_lock =
{
	if (!SUB_ShouldSpawn()) return;
	self.use = target_lock_use;
	if (self.spawnflags & 4) self.state = 1;
	else self.state = 0;
	
	if (self.spawnflags & 1)
	{
		self.state = 0;
		self.think = target_lock_use;
		self.nextthink = time + 0.25;
	}
}

//============================================================================


void() counter_use =
{
	self.state = self.state + 1;
	if (self.state > self.count)
		return;
	
	if (self.state != self.count)
	{
		float rem = self.count - self.state;
		if (activator.classname == "player" && !(self.spawnflags & SPAWNFLAG_NOMESSAGE))
		{
			if (rem >= 6)
				centerprint (activator, "There are more to go ...");
			else if (rem == 5)
				centerprint (activator, "There are 5 more to go ...");
			else if (rem == 4)
				centerprint (activator, "There are 4 more to go ...");
			else if (rem == 3)
				centerprint (activator, "There are 3 more to go ...");
			else if (rem == 2)
				centerprint (activator, "Only 2 more to go ...");
			else
				centerprint (activator, "Only 1 more to go ...");
		}
		return;
	}
	
	if (activator.classname == "player" && (self.spawnflags & SPAWNFLAG_NOMESSAGE) == 0)
		centerprint(activator, self.message);
	
	self.enemy = activator;
	if (self.spawnflags & 2) 
		self.state = 0;	// repeatable
	multi_trigger ();
}

/*QUAKED trigger_counter (.5 .0 .5) (-8 -8 -8) (8 8 8) nomessage repeat
Acts as an intermediary for an action that takes multiple inputs. 
If nomessage is not set, it will print "1 more.. " etc when triggered and "sequence complete" when finished. After the counter has been triggered "count" times, it will fire all of it's targets and remove itself, unless 'repeat' has been set.

Flags:
"nomessage" disables count display

Keys:
"count" number of triggers needed to fire own target, default is 2
"target"  entity to trigger (required)
"targetname"  entity name (required)
"message"	override text to print on completion
*/
/*FGD
@PointClass base(Appearflags, Target, Targetname) = trigger_counter : "Trigger: Counter" 
[
	spawnflags(flags) = [ 
		1: "No Message" : 0
		2: "Repeat" : 0
	]
	count(integer) : "Count before trigger" : 2
	delay (integer) : "Delay"
	message(string) : "Message"
]
*/
void() trigger_counter =
{
	if (!SUB_ShouldSpawn()) return;
	if (self.spawnflags & 2)
		self.wait = 1;
	else
		self.wait = -1;
	
	if (!self.count)
		self.count = 2;
	if (self.message == string_null)
		self.message = "Sequence completed!";
		
	//self.cnt = self.count;

	self.use = counter_use;
}


//============================================================================


void() timer_think =
{
	if (self.count == 0) {
		//SUB_Remove();
		return;
	}

	if (self.nextthink > time)	// turn off again
	{
		self.nextthink = 0;
		return;
	}		

	self.nextthink = time + self.wait + random() * self.rand;
	self.count = self.count - 1;
	activator = self.owner;
	SUB_UseTargets();
}

void() timer_use =
{
	dprint("in timer_use\n");
	
	self.owner = activator;
	self.count = self.cnt;
	timer_think();
}

/*QUAKED trigger_timer (.5 .5 .5) (-8 -8 -8) (8 8 8) start_on
Fires its targets once every "wait" seconds. If "count" is set, it will only fire that many times and then stop. Set "rand" to add a random extra delay to the wait interval.

SPAWNFLAGS
START_ON: do not wait until triggered to begin firing
*/
/*FGD
@PointClass base(Appearflags, Target, Targetname) = trigger_timer : "Trigger: Timer" 
[
	spawnflags(flags) = [ 
		1: "Start On" : 0
	]
	count(integer) : "Limit" : 0
	wait(string) : "Interval"
	rand(string) : "Random Extra Interval"
]
*/
void() trigger_timer =
{
	if (!SUB_ShouldSpawn()) return;
	if (!self.wait)
		self.wait = 1;
	if (self.rand < 0)
		self.rand = 0;
		
	self.think = timer_think;
	self.use = timer_use;

	if (self.spawnflags & 1)
	{
		self.nextthink = time + self.wait;
	}
	
	if (!self.count)
		self.count = -1;
	self.cnt = self.count;
}

//============================================================================


