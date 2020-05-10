/*
================================================================

			USETARGET SUBS

================================================================
*/


/*
=============
tgtcmp

checks if two entities target each other
returns 1 if n1 targets n2, -1 if n2 targets n1, 0 otherwise
===============
*/
float(entity n1, entity n2) tgtcmp =
{
	if (n1.target == n2.targetname) return 1;
	if (n1.target2 == n2.targetname) return 1;
	if (n1.target3 == n2.targetname) return 1;
	if (n1.target4 == n2.targetname) return 1;

	if (n2.target == n1.targetname) return -1;
	if (n2.target2 == n1.targetname) return -1;
	if (n2.target3 == n1.targetname) return -1;
	if (n2.target4 == n1.targetname) return -1;
	
	return 0;
}

/*
=============
SUB_CopyTargets
===============
*/
void(entity to) SUB_CopyTargets =
{
	if (to == world) return;
	to.target = self.target;
	to.target2 = self.target2;
	to.target3 = self.target3;
	to.target4 = self.target4;
	to.killtarget = self.killtarget;
}

/*
=============
SUB_AddTarget
===============
*/
void(entity to, string tname) SUB_AddTarget =
{
	if (to == world) return;
	if (to.target == string_null)
		to.target = tname;
	else if (to.target2 == string_null)
		to.target2 = tname;
	else if (to.target3 == string_null)
		to.target3 = tname;
	else if (to.target4 == string_null)
		to.target4 = tname;
	else
		dprint3("Warning: no room to add target to ", to.classname, "\n");
}

/*
=============
SUB_MergeTargets
===============
*/
void(entity to) SUB_MergeTargets =
{
	// iw -- fixed copying target over all 4 targets and never copying killtarget
	if (to == world) return;
	if (self.target != string_null)
		SUB_AddTarget(to, self.target);
	if (self.target2 != string_null)
		SUB_AddTarget(to, self.target2);
	if (self.target3 != string_null)
		SUB_AddTarget(to, self.target3);
	if (self.target4 != string_null)
		SUB_AddTarget(to, self.target4);
	if (self.killtarget != string_null)
	{
		if (to.killtarget != string_null)
			dprint3("Warning: no room to add killtarget to ", to.classname, "\n");
		else
			to.killtarget = self.killtarget;
	}		
}

/*
=============
DelayThink
===============
*/
void() DelayThink =
{
	activator = self.enemy;
	SUB_UseTargets ();
	remove(self);
}

/*
=============
DelayThinkRandom
===============
*/
void() DelayThinkRandom =
{
	activator = self.enemy;
	SUB_UseRandomTarget ();
	remove(self);
}

/*
=============
SUB_UseTargetsDelay
===============
*/
void( void() delayedthink ) SUB_UseTargetsDelay =
{
	local entity t;
	// create a temp object to fire at a later time
	t = spawn();
	t.classname = "DelayedUse";
	t.nextthink = time + self.delay;
	t.think = delayedthink;
	t.enemy = activator;
	t.message = self.message;
	SUB_CopyTargets(t);

	t.cnt = self.cnt;
}

/*
=============
SUB_PrintMessage
===============
*/
void() SUB_PrintMessage =
{
	if (self.message == string_null)
		return;
	if (self.classname == "info_storycamera")
		return;
	entity client = world;
	float i;
	
	dprint5("Printing message '",self.message,"' to ",activator.classname," ...\n");
	
	// print the message
	if (activator.classname == "player")
	{
		centerprint (activator, self.message);
		if (!self.noise)
			sound (activator, CHAN_VOICE, "misc/talk.wav", 1, ATTN_NORM);
	}
	else
	{
		// sometimes activator can be an infighting monster that killed a monster 
		// with a targetname, so just print to all clients
		client = find(client,classname,"player");
		i = 0;
		while (i < 4 && client != world)
		{
			centerprint (client, self.message);
			if (!self.noise)
				sound (client, CHAN_VOICE, "misc/talk.wav", 1, ATTN_NORM);
			if (!coop) break;
			client = find(client,classname,"player");
			i++;
		}
	}
}

/*
=============
SUB_UseNextTarget
===============
*/
entity(entity t, string tname) SUB_UseNextTarget =
{
	local entity otemp, stemp;
	t = find (t, targetname, tname);
	if (!t) return world;
	stemp = self;
	otemp = other;
	self = t;
	other = stemp;
	if (self.use != SUB_Null)
		if (self.use) self.use ();

	self = stemp;
	other = otemp;
	return t;
}


/*
=============
SUB_UseEntTargets
===============
*/
void(entity t) SUB_UseEntTargets =
{
	if (t == world) return;
	activator = self;
	entity oself = self;
	self = t;
	SUB_UseTargets();
	self = oself;
}

/*
=============
SUB_UseTargetsByField
===============
*/
string curTgt;
void(.string fld) SUB_UseTargetsByField =
{
	if (self.fld == string_null) return;
	local entity t, act;
	
	dprint5(activator.classname," caused ",self.classname," to fire target '",self.fld);
	dprint("'\n");
	
	act = activator;
	t = world;
	// some custom maps (like dm456sp.bsp) have cross-targeted doors that all open in unison
	// by targeting themselves and each other, which can smash the stack on older versions of
	// quake with MAX_STACK_DEPTH 32
	if (curTgt != self.fld)
	{
		curTgt = self.fld;
		do	{
			t = SUB_UseNextTarget( t, self.fld );
			activator = act;
			if (!t)
			{
				curTgt = string_null;
				return;
			}
		} while ( 1 );
	}
	dprint3("WARNING: circular tname reference: ", curTgt, ", skipping\n");
	curTgt = string_null;
}

void(string tname) SUB_KillTargets =
{
	// kill the killtargets
	if (tname == string_null)
		return;

	entity t = world;
	do
	{
		t = find (t, targetname, tname);
		if (!t)
			break;
		RemoveTarget (t);
	} while ( 1 );
}

/*
==============================
SUB_UseTargets

the global "activator" should be set to the entity that initiated the firing.

If self.delay is set, a DelayedUse entity will be created that will actually
do the SUB_UseTargets after that many seconds have passed.

Centerprints any self.message to the activator.

Removes all entities with a targetname that match self.killtarget.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function

lunaran: using "findalltargets" here would be much simpler and lead to a shorter
call stack, but entities that trigger each other in sequence within one frame
(like trigger_relays) would stomp each others' linked lists of targets and cause
targets not to fire.
==============================
*/
void() SUB_UseTargets =
{
	//local entity t;//, act;

	if (!self.target && !self.target2 && !self.target3 && !self.target4 && !self.killtarget)
	return;

	if (activator == world)
	{
		//error("activator was null. globals are bad, kids.\n");
		//return;
		dprint3("WARNING: Activator was null!\nuser (self): ", self.classname, "\n");
	}
	
	// check for a delay
	if (self.delay)
	{
		SUB_UseTargetsDelay(DelayThink);
		return;
	}
	
	if (self.killtarget != string_null)
	{
		if (self.killtarget == self.targetname)
			dprint5("WARNING: ", self.classname, " killtargets itself! (", self.killtarget, ")\n");
		if (self.killtarget == self.target ||
			self.killtarget == self.target2 ||
			self.killtarget == self.target3 ||
			self.killtarget == self.target4 )
			dprint5("WARNING: ", self.classname, " targets and killtargets the same targetname (", self.killtarget, ")\n");
	}
	
	// makes more logical sense to do killtarget after targets, but it was 
	// before in id1 so before is where it has to stay
	SUB_KillTargets(self.killtarget);
	
	SUB_UseTargetsByField(target);
	SUB_UseTargetsByField(target2);
	SUB_UseTargetsByField(target3);
	SUB_UseTargetsByField(target4);
}

/*
=============
SUB_UseRandomTarget
===============
*/
void() SUB_UseRandomTarget =
{
	local entity t, act, otemp, stemp;
	local float randy;

	// check for a delay
	if (self.delay)
	{
		SUB_UseTargetsDelay(DelayThinkRandom);
		return;
	}

	// fire targets
	if (self.target != string_null) {
		randy = ceil( random() * self.cnt );
		act = activator;
		t = world;
		do	{
			t = find (t, targetname, self.target);
			randy = randy - 1;
			if (!t) break;
		} while ( randy > 0 );
		stemp = self;
		otemp = other;
		self = t;
		other = stemp;
		if (self.use != SUB_Null)	{
			if (self.use) self.use ();
		}
		self = stemp;
		other = otemp;
		activator = act;
	}
	
	// kill the killtargets
	if (self.killtarget != string_null)
	{
		randy = ceil( random() * self.frags );
		t = world;
		do
		{
			t = find (t, targetname, self.killtarget);
			randy = randy - 1;
			if (!t) break;
		} while ( randy > 0 );
		RemoveTarget(t);
	}
}



/*
=============
SUB_CountTargets
===============
*/
void() SUB_CountTargets =
{
	local entity t;
	local float i;
	
	i = 0;
	t = world;
	while (self.target != string_null) 
	{
		t = find(t, targetname, self.target);
		if (!t) break;
		i = i + 1;
	}
	t = world;
	while (self.target2 != string_null) 
	{
		t = find(t, targetname, self.target2);
		if (!t) break;
		i = i + 1;
	}
	t = world;
	while (self.target3 != string_null) 
	{
		t = find(t, targetname, self.target3);
		if (!t) break;
		i = i + 1;
	}
	t = world;
	while (self.target4 != string_null) 
	{
		t = find(t, targetname, self.target4);
		if (!t) break;
		i = i + 1;
	}

	self.cnt = i;
	
	i = 0;
	t = world;
	while (self.killtarget != string_null) 
	{
		t = find(t, targetname, self.killtarget);
		if (!t) break;
		i = i + 1;
	}
	
	self.frags = i;
}

/*
=============
SUB_VerifyTrigger

somewhat unreliable attempt to ensure items with targetnames that aren't actually
triggered by anything don't stay unspawned forever and break a stock map
examples: weapons in antediluvium targeted by spotlights, the silver key in e4m4
===============
*/
float(entity tg) SUB_VerifyTrigger =
{
	// things that could conceivably be targeted at an item but cannot fire targets
	if (tg.classname == "light" ||
		//tg.classname == "path_corner" ||	// trains do fire targets
		tg.classname == "trap_shooter" ||
		tg.classname == "target_drop" ||
		tg.classname == "trap_spikeshooter")
		return FALSE;
		
	if (tg.killtarget == self.targetname)	// id1 'killtarget must target' protection
		return FALSE;
	return TRUE;
}

/*
=============
SUB_VerifyTriggerable
===============
*/
float() SUB_VerifyTriggerable =
{
	if (self.targetname == string_null) return FALSE;
	// some quake maps use SUB_regen as a hack to make items respawn, but
	// they expect those items to still spawn at map start, so catch those
	// cases and return false to force the item to spawn
	if (self.use == SUB_regen) return FALSE;
		
	float validTriggerers;
	entity tg;
	
	validTriggerers = FALSE;
	tg = world;
	while(1) {
		tg = find(tg, target, self.targetname);
		if (!tg) break;
		validTriggerers |= SUB_VerifyTrigger(tg);
	}
	tg = world;
	while(1) {
		tg = find(tg, target2, self.targetname);
		if (!tg) break;
		validTriggerers |= SUB_VerifyTrigger(tg);
	}
	tg = world;
	while(1) {
		tg = find(tg, target3, self.targetname);
		if (!tg) break;
		validTriggerers |= SUB_VerifyTrigger(tg);
	}
	tg = world;
	while(1) {
		tg = find(tg, target4, self.targetname);
		if (!tg) break;
		validTriggerers |= SUB_VerifyTrigger(tg);
	}
	
	return validTriggerers;
}

