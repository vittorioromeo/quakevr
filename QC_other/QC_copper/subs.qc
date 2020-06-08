/*
================================================================

	MISC SUBSTITUTION FUNCTIONS

================================================================
*/

void() SUB_Null = {}

/*
=============
SUB_Remove/SUB_RemoveSoon

RemoveSoon is to be used during touch functions to avoid touchlinks errors
===============
*/
void() SUB_Remove = { remove(self); }
void() SUB_RemoveSoon = 
{
	// ensure no shenanigans take place in the next hundredth of a second
	// otherwise we get weirdness like items being picked up twice at high framerates
	self.model = string_null;
	self.solid = SOLID_NOT;
	self.touch = SUB_Null;
	self.use = SUB_Null;
	
	self.think = SUB_Remove;
	self.nextthink = time;
}

/*
=============
RemoveTarget

take an entity out of gameplay (independent of 'killing' it)
===============
*/
void(entity victim) RemoveTarget =
{
	
	// iw -- don't change kill count when removing a dead monster
	if (victim.flags & FL_MONSTER && victim.deadflag == DEAD_NO && !(victim.customflags & CFL_SPAWNER))
		addmonster(-1);		// since the player won't get credit for the kill

	// clean up door triggers so they don't get lonely and crash
	if (victim.trigger_field != world)
		remove(victim.trigger_field);

	
	remove (victim);
}

/*
=============
SUB_CallAsSelf

wrap the self/oself shuffle for code cleanliness elsewhere
===============
*/
void(void() fun, entity newself) SUB_CallAsSelf =
{
	local entity oself;
	
	oself = self;
	self = newself;
	fun();
	self = oself;	
}

/*
=============
SUB_ChangeModel

size changes after a model change, use this to preserve it
===============
*/
void(entity ent, string mdl) SUB_ChangeModel =
{
	local vector sz,smin,smax;
	sz = ent.size;
	smin = ent.mins;
	smax = ent.maxs;
	
	setmodel(ent, mdl);
	
	if (sz == VEC_ORIGIN)
		setsize(ent, VEC_ORIGIN, VEC_ORIGIN);
	else
		setsize(ent, smin, smax);
}

/*
void(float frame, void() th, float thtime) SUB_FrameThinkTime
{
	self.frame = frame;
	self.think = th;
	self.nextthink = time + thtime;
}
*/


/*
===============
SUB_ShouldSpawn

check coop spawn flags, because we invented them and the exe 
only checks skill/dm flags
===============
*/
float() SUB_ShouldSpawn =
{
	if (self.spawnflags & SPAWN_COOPONLY && self.spawnflags & SPAWN_NOTCOOP)
		objerror("cooponly and notincoop flags both set on the same entity");
	if ( ((self.spawnflags & SPAWN_NOTCOOP) && coop) || ((self.spawnflags & SPAWN_COOPONLY) && !(coop)) )
	{
		remove(self);
		return FALSE;
	}
	return TRUE;
}



