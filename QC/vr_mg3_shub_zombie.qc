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
// Dawn of the Machine's Shub finale II (MG3.md M3-27): her shub zombies, adapted from
// quakec_mg3/monsters/mg3_shub_zombie.qc, and boss2's sinking pillars (func_breakable, from mg3_oldone_new.qc).
//
// Shub zombies: Shub raises one twice a loop (vr_mg3_shub.qc) at one of the map's info_szombie_spawn (36 on boss2), at
// most 33 alive. A spawn point used rests 8 s; the pick is upstream's (a random index under the count of rested points,
// counted over all of them). A shub zombie is Quake's zombie, lying where it rose and getting up 7 s later (when there is
// room), hunting you, its flesh thrown only up close (upstream's melee "missile": Quake VR's MG3 close-throw zombie);
// its flesh passes Shub and her eyes. Quake VR: it is a monster_zombie (.mg3_szombie set): the zombie's rig, ragdoll,
// head zone and beheading, limb gore, knockdowns, the chainsaw and Super Axe rules; killed whole it gibs with its head (upstream's
// shub zombie threw no head). Her spheres pass it, her swiper's beam does it 100, her death's cleaner gibs them all.
// Quake's models: no MG3 data needed. Debug spawner Thing 61: one rising where you point.
//
// Pillars (func_breakable; boss2's 8 round Shub, items on some tops): any damage sinks one 20 units (20 a second for
// 1 s, from its last hit), so her blasts and beams and your shots bring them down over the fight. 10000 health,
// restored each hit: only a single blow of 10000 (a telefrag) removes one. Never raised, never stopped: one you stand on
// takes you down with it to the floor, which holds you as it sinks on (Quake's pusher, as upstream).

// Upstream's spawn point: wait -1 until used, then the time it rests until.
/*QUAKED info_szombie_spawn (1 0 0) (-16 -16 -24) (16 16 32)
A spot where Dawn of the Machine's Shub-Niggurath (monster_oldone_new) raises a shub zombie: it lies there, then gets up 7 s later. Used, it rests 8 s.
*/
void() info_szombie_spawn =
{
    MG3_SZombie_Precache();
    self.wait = -1;
};

void() MG3_SZombie_Precache =
{
    monster_zombie_precache();
};

// A shub zombie on `self`, lying at `org`: counted, hunting the player, up in 7 s.
void(vector org) MG3_SZombie_Setup =
{
    self.classname = "monster_zombie";
    self.mg3_szombie = TRUE;
    VR_Knockdown_SetupOwn(cvarh_vr_knockdown_chance_zombie, MG3_ZOMBIE_PAINE12, zombie_paine13);
    self.health = 60;
    self.max_health = 60;
    self.inpain = 2;
    self.th_stand = zombie_stand1;
    self.th_walk = zombie_walk1;
    self.th_run = zombie_run1;
    self.th_pain = zombie_pain;
    self.th_die = zombie_die;
    self.th_missile = SUB_Null;
    self.th_melee = zombie_missile; // (upstream: "unlike normal zombie, get close range first!")
    self.use = monster_use;
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_STEP;
    setmodel(self, "progs/zombie.mdl");
    setsize(self, '-12 -12 -24', '12 12 24'); // (monster_zombie's; standing up gives it the hull's)
    setorigin(self, org);
    self.takedamage = DAMAGE_AIM;
    self.ideal_yaw = self.angles_y;
    self.view_ofs = '0 0 25';
    self.flags = self.flags | FL_MONSTER;
    self.yaw_speed = 20;
    self.noise = "zombie/z_idle.wav";
    self.wad = ZOMBIE_ONGROUND;
    MG3_Shub_CountMonster();
    self.enemy = MG3_Shub_Player();
    self.frame = MG3_ZOMBIE_PAINE12 + 1; // ($paine13: lying)
    self.think = zombie_paine11; // (5 s more, then up if there is room)
    self.nextthink = time + 2;
};

// Shub raises a shub zombie (upstream spawn_szombie).
void() MG3_SZombie_Raise =
{
    float count = 0;
    entity e = find(world, classname, "monster_zombie");
    while(e)
    {
        if(MG3_SZombie_Is(e)) count++;
        e = find(e, classname, "monster_zombie");
    }
    if(count > 32)
        return;

    count = 0;
    float total = 0;
    e = find(world, classname, "info_szombie_spawn");
    if(!e)
        return; // (no spawn points: none raised)
    while(e)
    {
        if(e.wait && (e.wait < time)) count++;
        total++;
        e = find(e, classname, "info_szombie_spawn");
    }
    if(count == 0)
    {
        dprint("MG3 Shub: every info_szombie_spawn resting\n");
        return;
    }

    // (upstream: an index under the rested count, counted over every point)
    float pick = floor(random() * (count - 1) + 0.5);
    e = find(world, classname, "info_szombie_spawn");
    count = 0;
    while(count != pick)
    {
        count++;
        e = find(e, classname, "info_szombie_spawn");
    }
    if(!e)
        return;
    e.wait = time + 8;

    entity oldself = self;
    self = spawn();
    MG3_SZombie_Setup(e.origin);
    self = oldself;
};

/*QUAKED monster_szombie (1 0 0) (-16 -16 -24) (16 16 32)
Dawn of the Machine's shub zombie: Quake's zombie lying here, getting up 7 s later and hunting you; it throws its flesh only up close. Counted.
*/
void() monster_szombie =
{
    if(MG_MonsterInhibited()) return;
    if(deathmatch)
    {
        remove(self);
        return;
    }
    MG3_SZombie_Precache();
    MG3_SZombie_Setup(self.origin);
};

//============================================================================
// Pillars.

void() breakable_stop =
{
    self.velocity = '0 0 0';
};

void(entity attacker, float take) breakable_pain =
{
    self.health = 10000;
    self.velocity = '0 0 -20';
    self.think = breakable_stop;
    self.nextthink = self.ltime + 1;
};

/*QUAKED func_breakable (0 .5 .8) ?
Dawn of the Machine's sinking pillar (boss2): any damage sinks it 20 units (20 a second for a second from its last hit). 10000 health, restored each hit: only a blow of 10000 removes it.
*/
void() func_breakable =
{
    self.solid = SOLID_BSP;
    self.movetype = MOVETYPE_PUSH;
    self.health = 10000;
    self.max_health = 10000;
    self.takedamage = DAMAGE_YES;
    self.th_pain = breakable_pain;
    self.th_die = SUB_Remove;
    setorigin(self, self.origin);
    setmodel(self, self.model);
};
