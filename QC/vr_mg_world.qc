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
void() MG_func_explode_detonate = {
	vector pos = self.mins + (self.size * 0.5);
	pos_z -= 32;
	setmodel(self, string_null);
	setorigin(self, pos);

	self.origin_z += 32;
    self.takedamage = DAMAGE_NO;
    self.solid = SOLID_NOT;
    T_RadiusDamage(self, self, 160, world);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteVec3(MSG_BROADCAST, self.origin);
    activator = self.enemy;
    SUB_UseTargets();
    BecomeExplosion();
}

void() MG_func_explode_die =
{
	self.takedamage = DAMAGE_NO;
	self.think = MG_func_explode_detonate;
	self.nextthink = self.ltime + 0.15;
}


/*QUAKED func_explode (0 .5 .8) (0 0 0) (32 32 64)
Custom exploding box
*/

void() func_explode =
{
    if(!MG_WorldGuard()) return;
	self.angles = '0 0 0';
	self.movetype = MOVETYPE_PUSH;
	self.solid = SOLID_BSP;
	setmodel (self, self.model);

	precache_sound ("weapons/r_exp3.wav");
	self.health = 20;
	self.th_die = MG_func_explode_die;
    self.classname = "func_explode";
	self.takedamage = DAMAGE_AIM;

	
};

const float FUNC_HURT_START_ON = 1;

void() MG_func_hurt_touch = {
	if(!self.state) return;
	if(other.health <= 0) return;
	if(!other.takedamage) return;
	if(!(other.flags & (FL_CLIENT | FL_MONSTER))) return;
	if(self.attack_finished > time) return;
	
	T_Damage(other, self, world, self.dmg);
	self.attack_finished = time + self.wait;
}

void() MG_func_hurt_use = {
	self.state = 1 - self.state;
	self.attack_finished = 0;
}

// Solid brush hull contact for the tracked hands: the body's existing engine touch still uses the authored wait.
void(float dt) MG_func_hurt_tick = {
    if(!self.state || self.attack_finished > time) return;
    entity oother = other;
    entity p = find(world, classname, "player");
    while(p) {
        if(p.ishuman && p.health > 0 && VRHandsTracked(p)) {
            traceline(p.handpos, p.handpos, FALSE, p);
            float got = ((trace_ent == self) && (trace_startsolid || trace_allsolid));
            if(!got) {
                traceline(p.offhandpos, p.offhandpos, FALSE, p);
                got = ((trace_ent == self) && (trace_startsolid || trace_allsolid));
            }
            if(got) { other = p; MG_func_hurt_touch(); }
        }
        p = find(p, classname, "player");
    }
    other = oother;
};

/*QUAKED func_hurt (0 .5 .8) ?
Hurts alive things when they touch it
*/
void() func_hurt = {
    if(!MG_WorldGuard()) return;
	self.solid = SOLID_BSP;
	self.movetype = MOVETYPE_PUSH;
	setmodel(self, self.model);
	setorigin(self, self.origin);
	self.touch = MG_func_hurt_touch;
	self.use = MG_func_hurt_use;
    self.MG_tick = MG_func_hurt_tick;
    MG_RegisterFrameTickEntity(self);

	if(!self.dmg) self.dmg = 10;
	if(!self.wait) self.wait = 0.2;

	if(self.spawnflags & FUNC_HURT_START_ON)
	{
		self.state = 1;
	}
}

/*QUAKED ambient_generic (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void() ambient_generic =
{
	if(self.noise == "")
	{
		dprint("ambient_generic with no noise. Removing.\n");
		remove(self);
        return;
	}
	if(!self.volume) self.volume = 0.5;
	if(!self.delay) self.delay = 3;
	
	precache_sound (self.noise);
	ambientsound (self.origin, self.noise, self.volume, self.delay);
	makestatic(self);
};

