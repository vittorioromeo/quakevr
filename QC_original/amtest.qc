/*~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>
~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~*/

void() test_teleport_touch;
void() tele_done;

/*QUAKED test_teleport (0 .5 .8) ?
Teleporter testing
*/
void() test_teleport =
{
	precache_model ("sprites/s_aball.spr");
	setsize (self, self.mins, self.maxs);	
	self.touch = test_teleport_touch;
	self.solid = 1;
	
	if (!self.target)
		objerror ("no target\n");
};

void() test_teleport_touch =
{
local entity oldself;
	other.movetype = MOVETYPE_TOSS;
//	other.solid = SOLID_NOT;
	other.dest = '256 -128 -128';
	oldself = self;
	self = other;
//	SUB_CalcMove (self.dest, 200, tele_done);
	self.velocity = '1000 0 0 ';
	self = oldself;
};

void() tele_done =
{
	self.movetype = MOVETYPE_WALK;
	self.solid = SOLID_SLIDEBOX;	
};

/*~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>
~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~*/

void() test_goaway;
void() test_spawn;

/*QUAKED test_fodder (0 .5 .8) ?
beating guy
*/
void() test_fodder =
{
	self.nextthink = time + 3;
	self.think = test_spawn;
};

void() test_spawn =
{
local entity	body;
	makevectors (self.angles);

	body = spawn();
	setmodel (body, "progs/soldier.mdl");
	setorigin (body, self.origin);
	body.classname = "player";
	body.health = 1000;
	body.frags = 0;
	body.takedamage = DAMAGE_AIM;
	body.solid = SOLID_SLIDEBOX;
	body.movetype = MOVETYPE_WALK;
	body.show_hostile = 0;
	body.weapon = 1;
	body.velocity = v_forward * 200;

	body.nextthink = time + 5;
	body.think = test_goaway;

self.nextthink = time + 3;
self.think = test_spawn;

};

void() test_goaway =
{
	remove (self);
};

