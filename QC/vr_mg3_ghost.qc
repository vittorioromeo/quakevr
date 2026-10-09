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
// Dawn of the Machine's ghosts (monster_ghost; MG3.md M3-20), adapted from quakec_mg3/monsters/mg3_player_ghost.qc:
// a player's ghost (progs/player.mdl) that drifts about its home (a few stand loops, then a run to a point 'wait' units
// away, 128 by default, and back the next time), 10 health, not a monster (not counted). It dies at the touch of a living
// player, whose hand too in Quake VR (handtouch), or by any damage: one of the player's deaths, then it fades away in a
// teleport flash. Quake's own model: no MG3 data needed.

$frame axrun1 axrun2 axrun3 axrun4 axrun5 axrun6
$frame rockrun1 rockrun2 rockrun3 rockrun4 rockrun5 rockrun6
$frame stand1 stand2 stand3 stand4 stand5
$frame axstnd1 axstnd2 axstnd3 axstnd4 axstnd5 axstnd6
$frame axstnd7 axstnd8 axstnd9 axstnd10 axstnd11 axstnd12
$frame axpain1 axpain2 axpain3 axpain4 axpain5 axpain6
$frame pain1 pain2 pain3 pain4 pain5 pain6
$frame axdeth1 axdeth2 axdeth3 axdeth4 axdeth5 axdeth6
$frame axdeth7 axdeth8 axdeth9
$frame deatha1 deatha2 deatha3 deatha4 deatha5 deatha6 deatha7 deatha8
$frame deatha9 deatha10 deatha11
$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9
$frame deathc1 deathc2 deathc3 deathc4 deathc5 deathc6 deathc7 deathc8
$frame deathc9 deathc10 deathc11 deathc12 deathc13 deathc14 deathc15
$frame deathd1 deathd2 deathd3 deathd4 deathd5 deathd6 deathd7
$frame deathd8 deathd9
$frame deathe1 deathe2 deathe3 deathe4 deathe5 deathe6 deathe7
$frame deathe8 deathe9

float mg3_ghosts_gone; // ghosts that faded away (the tests' count)

// See-through, as a ghost is meant to look (vr_mg3_ghost_alpha, 0.4; 1: solid, as the port first drew it: upstream's
// code sets no alpha). Entity alpha: drawn in the translucent pass (order-independent with r_oit), no shadow and no
// ambient occlusion (vr_lighting.cpp, vr_shadows.cpp, vr_ao.cpp skip translucent entities). Read again as it stands
// and runs, so the setting takes at once.
void() mg3_ghost_alpha =
{
    float a = cvar("vr_mg3_ghost_alpha");
    self.alpha = a >= 1 ? 0 : max(0.05, a); // (0: Quake's default, opaque, with its shadow)
};

void() ghost_run1;
void() mg3_ghost_touch;

void() ghost_die =
{
    spawn_tfog(self.origin);
    mg3_ghosts_gone = mg3_ghosts_gone + 1;
    remove(self);
};

void() ghost_step =
{
    vector vec = self.dest2 - self.origin;
    vec_z = 0;
    vec = normalize(vec);
    self.angles = vectoangles(vec);
    self.velocity = vec * 150;
};

void() ghost_stand1 = [$stand1, ghost_stand2] {self.velocity = '0 0 0'; mg3_ghost_alpha();};
void() ghost_stand2 = [$stand2, ghost_stand3] {};
void() ghost_stand3 = [$stand3, ghost_stand4] {};
void() ghost_stand4 = [$stand4, ghost_stand5] {};
void() ghost_stand5 = [$stand5, ghost_stand1]
{
    self.count = self.count - 1;
    if(!self.count)
    {
        self.think = ghost_run1;
        makevectors(random() * 360 * '0 1 0');
        self.dest2 = self.dest1 + v_forward * self.wait;
    }
};

void() ghost_run1 = [$rockrun1, ghost_run2] {ghost_step(); mg3_ghost_alpha();};
void() ghost_run2 = [$rockrun2, ghost_run3] {};
void() ghost_run3 = [$rockrun3, ghost_run4] {};
void() ghost_run4 = [$rockrun4, ghost_run5] {};
void() ghost_run5 = [$rockrun5, ghost_run6] {};
void() ghost_run6 = [$rockrun6, ghost_stand1] {self.count = floor(random() * 5 + 0.5) + 3;};

void() ghost_diea1 = [$deatha1, ghost_diea2] {DeathSound(); self.velocity = '0 0 0';};
void() ghost_diea2 = [$deatha2, ghost_diea3] {};
void() ghost_diea3 = [$deatha3, ghost_diea4] {};
void() ghost_diea4 = [$deatha4, ghost_diea5] {};
void() ghost_diea5 = [$deatha5, ghost_diea6] {};
void() ghost_diea6 = [$deatha6, ghost_diea7] {};
void() ghost_diea7 = [$deatha7, ghost_diea8] {};
void() ghost_diea8 = [$deatha8, ghost_diea9] {};
void() ghost_diea9 = [$deatha9, ghost_diea10] {};
void() ghost_diea10 = [$deatha10, ghost_diea11] {};
void() ghost_diea11 = [$deatha11, ghost_diea11] {ghost_die();};

void() ghost_dieb1 = [$deathb1, ghost_dieb2] {DeathSound(); self.velocity = '0 0 0';};
void() ghost_dieb2 = [$deathb2, ghost_dieb3] {};
void() ghost_dieb3 = [$deathb3, ghost_dieb4] {};
void() ghost_dieb4 = [$deathb4, ghost_dieb5] {};
void() ghost_dieb5 = [$deathb5, ghost_dieb6] {};
void() ghost_dieb6 = [$deathb6, ghost_dieb7] {};
void() ghost_dieb7 = [$deathb7, ghost_dieb8] {};
void() ghost_dieb8 = [$deathb8, ghost_dieb9] {};
void() ghost_dieb9 = [$deathb9, ghost_dieb9] {ghost_die();};

void() ghost_diec1 = [$deathc1, ghost_diec2] {DeathSound(); self.velocity = '0 0 0';};
void() ghost_diec2 = [$deathc2, ghost_diec3] {};
void() ghost_diec3 = [$deathc3, ghost_diec4] {};
void() ghost_diec4 = [$deathc4, ghost_diec5] {};
void() ghost_diec5 = [$deathc5, ghost_diec6] {};
void() ghost_diec6 = [$deathc6, ghost_diec7] {};
void() ghost_diec7 = [$deathc7, ghost_diec8] {};
void() ghost_diec8 = [$deathc8, ghost_diec9] {};
void() ghost_diec9 = [$deathc9, ghost_diec10] {};
void() ghost_diec10 = [$deathc10, ghost_diec11] {};
void() ghost_diec11 = [$deathc11, ghost_diec12] {};
void() ghost_diec12 = [$deathc12, ghost_diec13] {};
void() ghost_diec13 = [$deathc13, ghost_diec14] {};
void() ghost_diec14 = [$deathc14, ghost_diec15] {};
void() ghost_diec15 = [$deathc15, ghost_diec15] {ghost_die();};

void() ghost_died1 = [$deathd1, ghost_died2] {DeathSound(); self.velocity = '0 0 0';};
void() ghost_died2 = [$deathd2, ghost_died3] {};
void() ghost_died3 = [$deathd3, ghost_died4] {};
void() ghost_died4 = [$deathd4, ghost_died5] {};
void() ghost_died5 = [$deathd5, ghost_died6] {};
void() ghost_died6 = [$deathd6, ghost_died7] {};
void() ghost_died7 = [$deathd7, ghost_died8] {};
void() ghost_died8 = [$deathd8, ghost_died9] {};
void() ghost_died9 = [$deathd9, ghost_died9] {ghost_die();};

// (upstream's last frame here is deathd's: kept)
void() ghost_diee1 = [$deathe1, ghost_diee2] {DeathSound(); self.velocity = '0 0 0';};
void() ghost_diee2 = [$deathe2, ghost_diee3] {};
void() ghost_diee3 = [$deathe3, ghost_diee4] {};
void() ghost_diee4 = [$deathe4, ghost_diee5] {};
void() ghost_diee5 = [$deathe5, ghost_diee6] {};
void() ghost_diee6 = [$deathe6, ghost_diee7] {};
void() ghost_diee7 = [$deathe7, ghost_diee8] {};
void() ghost_diee8 = [$deathe8, ghost_diee9] {};
void() ghost_diee9 = [$deathd9, ghost_diee9] {ghost_die();};

// A living player's touch (its body, or a hand: handtouch) lays it to rest.
void() mg3_ghost_touch =
{
    if(!other || !other.health || self.health <= 0)
        return; // (dying already: a blow, or a touch before)
    if(self.pain_finished > time)
        return;
    if(other.classname == "player")
    {
        self.touch = SUB_Null;
        self.handtouch = SUB_Null;
        self.takedamage = DAMAGE_NO;
        self.health = 0;
        self.th_die();
    }
};

/*QUAKED monster_ghost (1 0 0) (-16 -16 -24) (16 16 32)
Dawn of the Machine's ghost: a player's ghost drifting about where it stands ('wait': how far, 128 by default). It vanishes at a living player's touch or a blow (10 health); not a monster (not counted).
*/
void() monster_ghost =
{
    if(MG_MonsterInhibited()) return;
    if(deathmatch)
    {
        remove(self);
        return;
    }
    precache_model("progs/player.mdl");
    float r = random() * 5;
    self.th_stand = ghost_stand1;
    self.th_walk = ghost_run1;
    self.th_run = ghost_run1;
    if(!self.wait)
        self.wait = 128;
    if(r < 1)
        self.th_die = ghost_diea1;
    else if(r < 2)
        self.th_die = ghost_dieb1;
    else if(r < 3)
        self.th_die = ghost_diec1;
    else if(r < 4)
        self.th_die = ghost_died1;
    else
        self.th_die = ghost_diee1;
    self.health = 10;
    self.solid = SOLID_SLIDEBOX;
    setmodel(self, "progs/player.mdl");
    mg3_ghost_alpha();
    setsize(self, VEC_HULL_MIN, VEC_HULL_MAX);
    self.takedamage = DAMAGE_YES;
    self.touch = mg3_ghost_touch;
    self.handtouch = mg3_ghost_touch;
    self.movetype = MOVETYPE_FLY;
    self.dest1 = self.origin; // home
    makevectors(random() * 360 * '0 1 0');
    self.dest2 = self.dest1 + v_forward * self.wait;
    self.th_stand();
    self.count = floor(random() * 5 + 0.5) + 3; // stand loops before the next move
};
