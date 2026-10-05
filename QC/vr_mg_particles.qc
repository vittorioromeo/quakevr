/*  Copyright (C) 1996-2026 id Software LLC

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/
void() MG_particle_embers_think =
{
	local vector pos;
	local vector speed;
	speed = '0 0 2' * random() + '0 0 2';
	speed_x = crandom() * self.velocity_x;
	speed_y = crandom() * self.velocity_y;
	speed_z = speed_z * self.velocity_z;
	pos = self.origin;
	pos_x = pos_x + self.size_x * crandom();
	pos_y = pos_y + self.size_y * crandom();
	//pos = crandom() * self.size + self.origin;
	particle(pos, speed, 234, 2);
	self.nextthink = time + self.wait + self.delay * random();
}

/*QUAKED particle_embers (1 .5 .2) (-8 -8 -8) (8 8 8)
Owned official campaign particle effect.
*/
void() particle_embers =
{
	// size determines the box the particles can spawn within
	if (!self.size)
		self.size = '128 128 0';
	// delay is random time added per loop
	if (!self.delay)
		self.delay = 0.1;
	
	// wait is time always added per loop
	if (!self.wait)
		self.wait = 0.05;
	
	// velocity is used as a scalar on speed
	if (!self.velocity)
		self.velocity = '1 1 1';
	
	self.think = MG_particle_embers_think;
	self.nextthink = time + self.wait + self.delay * random();
};

/*QUAKED particle_embers_tall (1 .5 .2) (-8 -8 -8) (8 8 8)
Owned official campaign particle effect.
*/
void() particle_embers_tall =
{
	if (!self.size)
		self.size = '40 40 0';
	if (!self.delay)
		self.delay = 0.1;
	if (!self.wait)
		self.wait = 0.05;
	if (!self.velocity) // scalar for speed
		self.velocity = '1 1 2';
	self.think = MG_particle_embers_think;
	self.nextthink = time + self.wait + self.delay * random();
};

//============================================================================
// particle_tele
//============================================================================
void() MG_particle_tele_think =
{
	local vector pos; // where to spawn
	local float dist; // scalar from org, used for speed too
	local vector rando;
	
	rando_x = crandom() * 10;
	rando_y = crandom() * 10;
	rando_z = crandom() * 5;
	rando = normalize(rando);
	
	dist = 64;
	if (self.distance)
		dist = self.distance;
	
	pos = self.origin + (rando * dist);
	
	// spawn particle
	particle(pos, rando * dist * -.125, 3, 3);
	self.nextthink = time + self.wait + self.delay * random();
}


/*QUAKED particle_tele (1 .5 .2) (-8 -8 -8) (8 8 8)
Owned official campaign particle effect.
*/
void() particle_tele =
{
	/*
	1. get origin
	2. get a random vector, then normalize it. Save it for later
	3. scale a copy of the vector by distance
	4. spawn the particle at vector + origin
	5. set the particle's velocity to the old normalized vector * -scalar.
	*/
	// size determines the box the particles can spawn within
	if (!self.size)
		self.size = '128 128 0';
	if (!self.distance)
		self.distance = 64;
	// delay is random time added per loop
	if (!self.delay)
		self.delay = 0.1;
	
	// wait is time always added per loop
	if (!self.wait)
		self.wait = 0;
	
	self.think = MG_particle_tele_think;
	self.nextthink = time + self.wait + self.delay * random();
}

void() MG_particle_tele_fountain_think =
{
	local vector dir;
	dir_x = crandom() * self.velocity_x;
	dir_y = crandom() * self.velocity_y;
	dir_z = self.velocity_z;
	particle(self.origin, dir, 13, 2);
	self.nextthink = time + self.wait + self.delay * random();
};
void() MG_particle_tele_fountain_use =
{
	self.think = MG_particle_tele_fountain_think;
	self.nextthink = time + 0.1;
};
/*QUAKED particle_tele_fountain (1 .5 .2) (-8 -8 -8) (8 8 8)
Owned official campaign particle effect.
*/
void() particle_tele_fountain =
{
	if (!self.delay)
		self.delay = 0.1;
	
	if (!self.wait)
		self.wait = 0.05;
	
	if (!self.velocity)
		self.velocity = '1 1 6';
	
	if (self.spawnflags & START_OFF)
		self.use = MG_particle_tele_fountain_use;
	else
	{
		self.think = MG_particle_tele_fountain_think;
		self.nextthink = time + self.wait + self.delay * random();
	}
};
