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
// Dawn of the Machine's slime (monster_slime; MG3_PLAN.md M3-20), adapted from quakec_mg3/monsters/tarbaby.qc: a spawn
// (classname monster_tarbaby, Quake's model and skin 0, no mitosis) that, blowing up, throws two or three blobs (the
// vore's ball model) that become spawns where they land, which do the same once more: two generations of children (up
// to 3 + 9). A blob that lands on something alive blows up there instead (120, radius); a child that starts in a wall
// blows up harmlessly. A child counts among the level's monsters only once it has started. Quake's models: no MG3 data
// needed.

// .mg3_slime (defs.qc): the generations still to come out of this spawn (2 a map's slime; its children 1, theirs 0);
// .mg3_slimekin: a slime or one of its children ("was slimed").
float mg3_slime_blobs;       // blobs thrown, children started, children that blew up in a wall (the tests' counts)
float mg3_slime_children;
float mg3_slime_stuck;
float mg3_slime_hits;        // blobs that landed on something alive

// A blob or a child going off without harm: the spawn's own blast, no damage (upstream tbaby_explode).
void() MG3_Slime_Explode =
{
    sound(self, CHAN_VOICE, "blob/death1.wav", 1, ATTN_NORM);
    self.origin = self.origin - 8 * normalize(self.velocity);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_TAREXPLOSION);
    WriteVec3(MSG_BROADCAST, self.origin);
    BecomeExplosion();
};

// A child's first think: in a wall it goes off; else it is a monster now (counted), hunting its parent's enemy.
void() MG3_Slime_StartCheck =
{
    if(!walkmove(0, 0))
    {
        mg3_slime_stuck = mg3_slime_stuck + 1;
        MG3_Slime_Explode();
        return;
    }
    self.think = FoundTarget;
    self.nextthink = time + 0.1;
    total_monsters = total_monsters + 1;
    WriteByte(MSG_ALL, SVC_UPDATESTAT);
    WriteByte(MSG_ALL, STAT_TOTALMONSTERS);
    WriteLong(MSG_ALL, total_monsters);
    self.takedamage = DAMAGE_AIM;
    mg3_slime_children = mg3_slime_children + 1;
};

void() MG3_Slime_BlobTouch =
{
    if(other == self.owner)
        return;
    if(other.health)
    {
        mg3_slime_hits = mg3_slime_hits + 1;
        T_RadiusDamage(self, self, 120, world);
        MG3_Slime_Explode();
        return;
    }
    // A child where it landed (upstream's spawn, as tbaby_checknew makes Rogue's).
    entity c = spawn();
    c.classname = "monster_tarbaby";
    c.angles_y = crandom() * 180;
    c.health = 80;
    setorigin(c, self.origin + '0 0 25');
    c.mg3_slime = self.mg3_slime;
    c.mg3_slimekin = TRUE;
    c.enemy = self.enemy;
    c.ideal_yaw = c.angles_y;
    c.yaw_speed = 20;
    c.view_ofs = '0 0 25';
    c.flags = c.flags | FL_MONSTER;
    c.th_stand = tbaby_stand1;
    c.th_walk = tbaby_walk1;
    c.th_run = tbaby_run1;
    c.th_missile = tbaby_jump1;
    c.th_melee = tbaby_jump1;
    c.th_die = tbaby_die1;
    c.use = monster_use;
    c.solid = SOLID_SLIDEBOX;
    c.movetype = MOVETYPE_STEP;
    setmodel(c, "progs/tarbaby.mdl");
    setsize(c, '-12 -12 -24', '12 12 24'); // (Quake VR's spawn's box: monster_tarbaby)
    c.pausetime = 99999999;
    c.think = MG3_Slime_StartCheck;
    c.nextthink = time + 0.1;
    remove(self);
};

// A blob thrown `xOffset` degrees off the spawn's facing, 200 up and 200 out, bouncing; gone after 5 s.
void(float xOffset) MG3_Slime_Blob =
{
    entity m = spawn();
    m.classname = "slime_blob";
    m.owner = self;
    m.enemy = self.enemy;
    m.mg3_slime = self.mg3_slime;
    m.solid = SOLID_BBOX;
    m.movetype = MOVETYPE_BOUNCE;
    setmodel(m, "progs/v_spike.mdl");
    setsize(m, '0 0 0', '0 0 0');
    setorigin(m, self.origin + '0 0 16');
    makevectors(self.angles + '0 1 0' * xOffset);
    m.velocity = v_forward * 200;
    m.velocity_z = 200;
    m.avelocity = '300 300 300';
    m.touch = MG3_Slime_BlobTouch;
    m.think = SUB_Remove;
    m.nextthink = time + 5;
    mg3_slime_blobs = mg3_slime_blobs + 1;
};

// tbaby_die2's (the spawn blowing up): a slime with generations to come throws its blobs.
void() MG3_Slime_Split =
{
    if(self.mg3_slime <= 0)
        return;
    self.mg3_slime = self.mg3_slime - 1;
    float offset = 180 + crandom() * 20;
    if(random() > 0.7)
    {
        offset = 120 + crandom() * 20;
        MG3_Slime_Blob(-offset);
    }
    MG3_Slime_Blob(crandom() * 20);
    MG3_Slime_Blob(offset);
};

/*QUAKED monster_slime (1 0 0) (-16 -16 -24) (16 16 24) Ambush
Dawn of the Machine's slime: a spawn that, blowing up, throws two or three blobs that become spawns where they land, which do the same once more (two generations). A blob landing on something alive blows up instead.
*/
void() monster_slime =
{
    precache_model("progs/v_spike.mdl");
    self.mg3_slime = 2;
    self.mg3_slimekin = TRUE;
    self.classname = "monster_tarbaby";
    monster_tarbaby();
    if(self.model == "")
        return; // (removed: not in this game)
    // Quake VR's spawn picks a skin and Rogue's mitosis at random outside the official campaigns: a slime is MG3's plain one.
    self.skin = 0;
    self.health = 80;
    self.th_pain = SUB_NullPain;
};
