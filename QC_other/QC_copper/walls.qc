/*
==============================================================================

SIMPLE BMODELS

func_s that don't do more than appear and disappear

==============================================================================
*/

/*
metlslime on making walls disappear:

"i finally got it to work, and here's how.

1) to enable being solid after turning inivisible and then visible again, i only ever set the model using setmodel once, and from then on alternate between setting modelindex to 0 or the actual index.

Since setmodel is never called, the entity stays linked into the world with the original physics bounds. self.solid can be changed without issue.

2) for entities that are set to START_OFF, there was still a problem where they'd never appear at all, and never become visible. My hack solution is to wait 1 second, then make them invisible/nonsolid in a special think function, rather than in the spawn function."
*/

void() bmodel_appear =
{
	// ***
	// IN THIS ORDER
	// ***
	self.solid = SOLID_BSP;
	self.movetype = MOVETYPE_PUSH;
	setorigin (self, self.origin);	
	setmodel (self, self.model);
	self.alpha = 1;
	// ***
	// NOT ANY OTHER ORDERS
	// ***
	
	self.state = STATE_BOTTOM;
	//force_retouch = 2;
}

void() bmodel_vanish =
{
	self.solid = SOLID_NOT;
	self.state = STATE_TOP;
	self.alpha = -1;
	
	// drop things that are sitting on the bmodel:
	//force_retouch = 2;	// nope
	entity drop;
	drop = world;
	do {
		drop = nextent(drop);
		if (drop.flags & FL_ONGROUND && EntitiesTouching(self,drop))
			drop.flags = not(drop.flags, FL_ONGROUND);
	} while (drop);
}


// ==========================================


void() func_wall_use =
{	// change to alternate textures
	if (self.noise) sound(self, CHAN_BODY, self.noise, 1.0, ATTN_NORM);
	
	if (self.spawnflags & 2)
	{
		if (self.state == STATE_TOP)
		{
			bmodel_appear();
			if (self.spawnflags & 1)
				self.solid = SOLID_NOT;
		}
		else
		{
			bmodel_vanish();
		}
		return;
	}
	
	self.frame = 1 - self.frame;
}

/*FGD
@baseclass base(Appearflags, Targetname) = Wall [
	spawnflags(flags) = [
		1 : "Illusionary" : 0
		2 : "Toggle Visible+Solid" : 0
		4 : "Start Invisible" : 0
	]
	noise(string) : "Sound to play when triggered"
]
*/

/*QUAKED func_wall (0 .5 .8) ? NONSOLID TOGGLE_VIS START_INVIS
This is just a solid wall if not inhibited.  Changes its texture(s) to alternate ones (e.g. basebtn), if targeted.
NOT STATIC.  Use a func_static for inconsequential bmodels.

Flags:
NONSOLID	act as a func_illusionary, but still respond to trigger events
TOGGLE_VIS	change visibility and solidity instead of texture when triggered. starts solid and visible unless START_INVIS is checked.
 
Keys:
"targetname" entity name
"mangle" orientation of the model, values are 'pitch yaw roll', positive pitch is down. not recommended on bmodels.
"noise" sound to play when triggered.
*/
/*FGD
@SolidClass base(Wall) = func_wall : "Wall, appears/disappears or starts animation when triggered (if supporting texture)" []
*/
void() func_wall =
{
	if (!SUB_ShouldSpawn()) return;
	
	if (self.noise) precache_sound(self.noise);
	precache_safe_model(self.model);
	
	self.angles = self.mangle;
	self.use = func_wall_use;
	//self.angles = '0 0 0';

	self.movetype = MOVETYPE_PUSH;	// so it doesn't get pushed by anything
	if (self.spawnflags & 1)
		self.solid = SOLID_NOT;
	else
		self.solid = SOLID_BSP;

	setmodel (self, self.model);
	
	if (self.spawnflags & 4)
		bmodel_vanish();
	else
		self.state = STATE_BOTTOM;
}

/*QUAKED func_wall_point (0 .5 .8) (-8 -8 -8) (8 8 8) NONSOLID
This is just a solid wall if not inhibited.  Changes its texture(s) to alternate ones (e.g. basebtn), if triggered.

Keys:
"model" filename of model to stand in for this entity. can be a bsp or an mdl. mdls will not be solid.
"mangle" orientation of the model, values are 'pitch yaw roll', positive pitch is down
"noise" sound to play when triggered.
"targetname"	entity name
*/
/*FGD
@PointClass base(Wall) = func_wall_point : "Wall (point), appears/disappears or starts animation when triggered (if supporting texture)" [
	model(string) : "External model"
	mangle(string) : "Orientation of external model"
]
*/
void() func_wall_point = { func_wall(); }

/*QUAKED func_static (0 .5 .8) ?
Like a func_wall but calls makestatic.  Static entities are always nonsolid.
*/
/*
@SolidClass base(Appearflags) = func_static : "Nonsolid, untriggerable static wall" []
*/
void() func_static =
{
	if (!SUB_ShouldSpawn()) return;
	func_wall();
	makestatic(self);
}

/*QUAKED func_static_point (0 .5 .8) (-8 -8 -8) (8 8 8)
Like a func_wall but calls makestatic.  Static entities are always nonsolid.

"model"		filename of model to stand in for this entity. can be bsp or mdl.
*/
/*
@PointClass base(Appearflags) = func_static_point : "Static (untriggerable) nonsolid model (point)" [
	model(string) : "External model"
	mangle(string) : "Orientation of external model"
]
*/
void() func_static_point = { func_static(); }


/*QUAKED func_illusionary (0 .5 .8) ?
A simple entity that looks solid but lets you walk through it. Functionally identical to a func_static.
*/
/*FGD
@SolidClass base(Appearflags) = func_illusionary : "Static nonsolid wall"  []
*/
void() func_illusionary =
{
	func_static();
}


// ==========================================

void() gate_use =
{
	activator = self;
	SUB_UseTargets();
}

/*QUAKED func_episodegate (0 .5 .8) ? E1 E2 E3 E4
This bmodel will appear if the episode has already been completed, so players can't reenter it.
If it does spawn, it fires its targets immediately.
*/
/*FGD
@SolidClass base(Appearflags, Target) = func_episodegate : 
"Episode Gate. 
If players have completed the episode, it fires its targets.
If players have NOT completed the episode, it self-removes."
[
	spawnflags(Flags) =
	[
		1 : "Episode 1" : 1
		2 : "Episode 2" : 0
		4 : "Episode 3" : 0
		8 : "Episode 4" : 0
	]
]
*/
void() func_episodegate =
{
	if (!SUB_ShouldSpawn()) return;
	if (!(serverflags & self.spawnflags))
		return;			// can still enter episode

	self.spawnflags = 0;
	func_wall();

	self.think = gate_use;
	self.nextthink = time + 0.2;
}

/*QUAKED func_bossgate (0 .5 .8) ?
This bmodel does not spawn unless players have all of the episode sigils.
If it does spawn, it fires its targets immediately.
*/
/*FGD
@SolidClass base(Appearflags, Target) = func_bossgate : 
"Boss Gate. 
If players have NOT completed all episodes, it fires its targets.
If players have completed all episodes, it self-removes." 
[]
*/
void() func_bossgate =
{
	if (!SUB_ShouldSpawn()) return;
	if ( (serverflags & SVFL_ALLEPISODES) == SVFL_ALLEPISODES)
		return;		// all episodes completed
	self.spawnflags = 0;
	func_wall();
	
	self.think = gate_use;
	self.nextthink = time + 0.2;
}


// ==========================================


/*QUAKED func_group (0.6 0.7 0.8) ?
groups brushes into a group that may or may not be easier to work with
*/
/*QUAKED func_detail (0.2 .5 .6) ?
Apparently this behaves like a detail brush. Doesn't seal against leaks.
*/
/*QUAKED func_detail_wall (0.2 .5 .6) ?
Apparently this behaves like a detail brush. Doesn't seal against leaks. Doesn't cut geometry it touches.
*/
/*QUAKED func_detail_illusionary (0.2 .5 .6) ?
Apparently this behaves like a detail brush. Doesn't seal against leaks. Doesn't cut geometry it touches. Non-solid.
*/
/*FGD
@SolidClass base(Phong) = func_group : "Group of brushes for in-editor use" []
@SolidClass base(Phong) = func_detail : "Group of brushes which aren't abusive to bsp" []
@SolidClass base(Phong) = func_detail_wall : "Group of brushes which aren't abusive to bsp or geometry" []
@SolidClass base(Phong) = func_detail_illusionary : "Group of brushes which aren't solid, or abusive to bsp or geometry" []
*/
