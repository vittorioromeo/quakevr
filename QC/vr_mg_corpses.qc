/*  Copyright (C) 1996-2022 id Software LLC

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


//============================================================================

typedef struct 
{
    string mdl;
    float frame;
}MG_poseInfo;

// QVR: each pose is a death's last frame. The ogre's were 116 and 126 ($death5 and $bdeath1, upright: "dead ogres
// standing" on their blood pools, Dawn of the Machine, 1.0.1) and the shambler's 87 ($death5): now 125, 135 and 93, the
// deaths' ends (as every other monster's here). The indices are id's (Quake VR's models keep their frame order).
#pragma warning disable F317 //Use of stale framemacros
MG_poseInfo MG_poses[] =
{
    { "progs/demon.mdl", 53 },
    { "progs/dog.mdl", 16 },
    { "progs/dog.mdl", 25 },
    { "progs/enforcer.mdl", 54 },
    { "progs/enforcer.mdl", 65 },
    { "progs/fish.mdl", 38 },
    { "progs/hknight.mdl", 53 },
    { "progs/hknight.mdl", 62 },
    { "progs/knight.mdl", 85 },
    { "progs/knight.mdl", 96 },
    { "progs/ogre.mdl", 125 },
    { "progs/ogre.mdl", 135 },
    { "progs/shalrath.mdl", 22 },
    { "progs/shambler.mdl", 93 },
    { "progs/soldier.mdl", 17 },
    { "progs/soldier.mdl", 28 },
    { "progs/wizard.mdl", 53 },
    { "progs/player.mdl", 49 },
    { "progs/player.mdl", 60 },
    { "progs/player.mdl", 69 },
    { "progs/player.mdl", 84 },
    { "progs/player.mdl", 93 },
    { "progs/player.mdl", 102 },
    // Heads
    { "progs/h_demon.mdl", 0 },
    { "progs/h_dog.mdl", 0 },
    { "progs/h_guard.mdl", 0 },
    { "progs/h_hellkn.mdl", 0 },
    { "progs/h_knight.mdl", 0 },
    { "progs/h_mega.mdl", 0 },
    { "progs/h_ogre.mdl", 0 },
    { "progs/h_player.mdl", 0 },
    { "progs/h_shal.mdl", 0 },
    { "progs/h_shams.mdl", 0 },
    { "progs/h_wizard.mdl", 0 },
    { "progs/h_zombie.mdl", 0 },
    // Gibs
    { "progs/gib1.mdl", 0 },
    { "progs/gib2.mdl", 0 },
    { "progs/gib3.mdl", 0 },
};
#pragma warning enable F317

void() VR_MG_PlacedBody; // (below)

void() misc_corpse = {
    if(!MG_WorldGuard()) return;
    if(MG3_RuneRemoved()) return; // Dawn of the Machine: not with this many runes
    if(self.style < 0 || self.style >= MG_poses.length)
    {
        objerror("misc_corpse with invalid style.");
        return;
    }
    MG_poseInfo pose = MG_poses[self.style];
    precache_model(pose.mdl);
    self.frame = pose.frame;
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    setmodel (self, pose.mdl);
    VR_MG_PlacedBody();
}

// QVR: a placed monster's or player's corpse (vr_corpse_placed 1) is a dead monster's body, as one killed in the game: a
// corpse in the physics (pushed, shot, gibbed, set on fire), and once a player comes near (vr_corpse_placed_wake, the
// engine: vr_box3d.cpp) a ragdoll a hand takes, cuts and beheads. Its monster's classname and death function name it
// for the corpse's head and gibs (VR_Corpse_Parts), the ragdoll's settings and the gore tables. Heads, gibs and the fish
// (a swimmer: no ragdoll) stay scenery; 0: all scenery, as Dawn of the Machine has them.
void() VR_MG_PlacedBody =
{
    if(!cvar("vr_corpse_placed"))
    {
        return;
    }
    string m = self.model;
    if(m == "progs/soldier.mdl")       { self.classname = "monster_army";        self.th_die = army_die; }
    else if(m == "progs/ogre.mdl")     { self.classname = "monster_ogre";        self.th_die = ogre_die; }
    else if(m == "progs/knight.mdl")   { self.classname = "monster_knight";      self.th_die = knight_die; }
    else if(m == "progs/hknight.mdl")  { self.classname = "monster_hell_knight"; self.th_die = hknight_die; }
    else if(m == "progs/enforcer.mdl") { self.classname = "monster_enforcer";    self.th_die = enf_die; }
    else if(m == "progs/dog.mdl")      { self.classname = "monster_dog";         self.th_die = dog_die; }
    else if(m == "progs/demon.mdl")    { self.classname = "monster_demon1";      self.th_die = demon_die; }
    else if(m == "progs/shambler.mdl") { self.classname = "monster_shambler";    self.th_die = sham_die; }
    else if(m == "progs/shalrath.mdl") { self.classname = "monster_shalrath";    self.th_die = shalrath_die; }
    else if(m == "progs/wizard.mdl")   { self.classname = "monster_wizard";      self.th_die = wiz_die; }
    else if(m != "progs/player.mdl")   { return; } // (a player's body keeps misc_corpse: the global ragdoll settings)
    self.flags = self.flags | FL_MONSTER;
    self.health = -1;
    self.vr_corpse_placed = 1;
    self.vr_corpse_since = time;
    VR_Corpse_Arm(self); // (lying still: its low box, its corpse health, not solid but touchable, takes damage)
    // (What gibbing it throws: VR_Corpse_Parts' picks, set by VR_Corpse_Arm. A map with no such monster alive has them
    // not precached.)
    if(vr_corpse_head != "")
    {
        precache_model(vr_corpse_head);
    }
    precache_model(vr_corpse_gib1);
    precache_model(vr_corpse_gib2);
    precache_model(vr_corpse_gib3);
};

