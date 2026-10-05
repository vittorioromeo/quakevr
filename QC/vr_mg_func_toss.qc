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

const float TOSS_WAVE_ORIGIN = 1;
const float TOSS_REMOVE_ON_STOP = 2;
const float TOSS_NO_BOUNCE = 4;
const float TOSS_ONLY_REMOVE_IF_FAR_FALL = 8;
const string TOSS_WAVE_NETNAME = "_toss_origin";

void() MG_func_toss_think = {
    if(vlen(self.velocity) < 0.01)
    {
        if(self.spawnflags & TOSS_REMOVE_ON_STOP)
        {
            if(self.spawnflags & TOSS_ONLY_REMOVE_IF_FAR_FALL)
            {
                if(vlen(self.oldorigin - self.origin) > 64)
                {
                    remove(self); 
                    return;
                }
            }
            else
            {
                remove(self); 
                return;
            }
        }
        self.velocity = '0 0 0';
        self.solid = SOLID_BSP;
        self.movetype = MOVETYPE_PUSH;
        return;
    }
    self.nextthink = time + 0.5;
}

void() MG_func_toss_toss = {
    vector min = self.mins + '4 4 0';
    vector max = self.maxs - '4 4 0' - self.dest2;

    self.solid = SOLID_BBOX;
    if(self.spawnflags & TOSS_NO_BOUNCE)
        self.movetype = MOVETYPE_TOSS;
    else
        self.movetype = MOVETYPE_BOUNCE;
    setsize(self, min, max);

    self.velocity = self.movedir;

    if(self.noise != "")
    {
        sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
    }

    self.think = MG_func_toss_think;
    self.nextthink = time + 0.25;
}



void() MG_func_toss_use = {
    self.use = SUB_Null;
    if(self.delay)
    {
        self.think = MG_func_toss_toss;
        self.nextthink = self.ltime + self.delay;
    }
    else
    {
        MG_func_toss_toss();
    }
}

void() MG_func_toss_linkcascade = {
    entity o = find(world, netname, TOSS_WAVE_NETNAME);
    float dist, mindist = 16384;
    while(o)
    {
        if(o.targetname == self.targetname)
        {
            vector p = self.mins + self.size * 0.5;
            p -= o.mins + o.size * 0.5;
            dist = vlen(p) / o.speed;
            if(dist < mindist) mindist = dist;
        }
        o = find(o, netname, TOSS_WAVE_NETNAME);
    }
    if(mindist < 16384)
        self.delay = mindist;
}

void() func_toss = {
    if(!MG_WorldGuard()) return;
    self.solid = SOLID_BSP;
	self.movetype = MOVETYPE_PUSH;
	setmodel (self, self.model);
    setorigin (self, self.origin);
    self.oldorigin = self.origin;

    self.use = MG_func_toss_use;

    if(!self.movedir)
    {
        self.movedir = '0 0 200';
    }

    if(self.dest)
    {
        self.movedir_x += crandom() * self.dest_x;
        self.movedir_y += crandom() * self.dest_y;
        self.movedir_z += crandom() * self.dest_z;
    }

    if(self.spawnflags & TOSS_WAVE_ORIGIN)
    {
        self.netname = TOSS_WAVE_NETNAME;
        if(!self.speed) self.speed = 200;
    }
    else if(!self.delay)
    {
        self.think = MG_func_toss_linkcascade;
        self.nextthink = self.ltime + 0.2;
    }

    if(self.noise != "")
    {
        precache_sound (self.noise);
    }
 }
 
// Shatter variant
void() MG_func_shatter_use = {
	self.use = SUB_Null;
	self.solid = SOLID_NOT;
	self.movetype = MOVETYPE_TOSS;
	self.velocity = self.movedir;
};
void() func_shatter = {
    if(!MG_WorldGuard()) return;
	local float dist; // distance between origin and pos2 
	self.solid = SOLID_BSP;
	self.movetype = MOVETYPE_PUSH;
	setmodel (self, self.model);
    setorigin (self, self.origin);
	
	self.pos1 = (self.absmax - (self.size/2));
		
	if(!self.pos2)
	{
		dprint("No pos2 provided for func_shatter\n");
		self.pos2 = self.origin - '0 0 100';
	}
	
	dist = vlen(self.pos1 - self.pos2);
	dist = dist * dist;
	
	if(!self.speed)
	{
		self.speed = 200;
	}
	
	self.speed = self.speed * 10000/dist;
	
	if(!self.wait) // use for random variance
		self.wait = 10;
	
	// calculate velocity in advance
	self.movedir = normalize(self.pos1 - self.pos2) * self.speed;
	self.movedir_x = self.movedir_x + crandom() * self.wait;
	self.movedir_y = self.movedir_y + crandom() * self.wait;
	self.movedir_z = self.movedir_z + crandom() * self.wait;

    self.use = MG_func_shatter_use;

	/*
    if(self.noise != "")
    {
        precache_sound (self.noise);
    }
	*/
	
}

