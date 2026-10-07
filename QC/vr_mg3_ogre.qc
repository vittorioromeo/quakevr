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
// Dawn of the Machine's rocket ogre (MG3_PLAN.md, M3-16), adapted from the snapshot's monsters/ogre.qc
// (monster_ogre_rocket, OgreFireRocket, T_OgreMissileTouch, its .aflag branches; ai.qc's sight sound; client.qc's
// obituary; docs/vr-port/OFFICIAL_QC_SOURCE.md).
//
// Quake VR's ogre (orig_mon_ogre.qc: its frames, knockdown, chainsaw, drops) drawn with MG3's own model, read from the
// owned pack in place (owned/mg3/progs/ogre_rocket.mdl: vr_gamedir.cpp VR_OwnedFile), so in any campaign when the Dawn
// of the Machine data is there (decision 5), removed where it is not. Its class is the ogre's (as upstream: no infighting,
// the ogre's head zone class, beheading, ragdoll settings, hulls, grapple), marked .vr_mg3_mon MG3_MON_ROCKET_OGRE. Its
// own: its rig (vr_ragdoll.cpp ogreRocketSeeds: the model's death frames are in id's ogre's order), its head zone
// (MG3_PositionalHead), Armagon's voice (MG3's armagon/ sounds), no chainsaw dropped (it holds a rocket launcher), and
// at range a volley of two rockets instead of a grenade: led at where you will be, the second 64 units aside; a rocket
// explodes for 40 (its blast 40), a zombie 60, a shambler 20; within 0.2 s of its launch it only stabs for 20. A pain
// that does not take 200 x random() makes it flinch, and its pain lasts 2 s longer. Its rockets are monsters' missiles:
// batted and parried as any (vr_juice.qc VR_Deflect_IsProjectile).

float MG3_MON_ROCKET_OGRE = 1;   // .vr_mg3_mon (combat.qc): Dawn of the Machine's own monster kinds
float MG3_MON_DEMODOG = 2;
float MG3_MON_RANGED_KNIGHT = 3;

string MG3_OGRE_ROCKET_MDL = "owned/mg3/progs/ogre_rocket.mdl";

.float vr_mg3_volley;            // the rockets of its volley fired so far
.float vr_mg3_close_until;       // a rocket: until when it only stabs (upstream's ammo_nails: too close to explode)
float vr_mg3_rockets;            // rocket ogres' rockets fired since the map started (the tests)

float MG3_RO_IDLE_STAND = 1;
float MG3_RO_IDLE_WALK = 2;
float MG3_RO_IDLE_RUN = 3;
float MG3_RO_PAIN = 4;
float MG3_RO_DEATH = 5;
float MG3_RO_SIGHT = 6;

float() MG3_RocketOgre_Available = { return fileexists(MG3_OGRE_ROCKET_MDL); };

void() MG3_RocketOgre_Precache =
{
    precache_model(MG3_OGRE_ROCKET_MDL);
    precache_model("progs/missile.mdl");
    precache_sound("weapons/sgun1.wav");
    precache_sound("owned/mg3/armagon/idle1.wav");
    precache_sound("owned/mg3/armagon/idle2.wav");
    precache_sound("owned/mg3/armagon/idle3.wav");
    precache_sound("owned/mg3/armagon/idle4.wav");
    precache_sound("owned/mg3/armagon/pain.wav");
    precache_sound("owned/mg3/armagon/sight.wav");
    precache_sound("owned/mg3/armagon/sight2.wav");
};

// The ogre's sound `xOwn` for its use `xWhich` (MG3_RO_*), or the rocket ogre's (Armagon's voice, as upstream).
string(float xWhich, string xOwn) MG3_RocketOgre_Sound =
{
    if(self.vr_mg3_mon != MG3_MON_ROCKET_OGRE)
    {
        return xOwn;
    }
    float r = random() < 0.5;
    if(xWhich == MG3_RO_IDLE_STAND || xWhich == MG3_RO_IDLE_RUN)
    {
        return r ? "owned/mg3/armagon/idle1.wav" : "owned/mg3/armagon/idle2.wav";
    }
    if(xWhich == MG3_RO_IDLE_WALK)
    {
        return r ? "owned/mg3/armagon/idle3.wav" : "owned/mg3/armagon/idle4.wav";
    }
    if(xWhich == MG3_RO_PAIN) { return "owned/mg3/armagon/pain.wav"; }
    if(xWhich == MG3_RO_DEATH) { return "owned/mg3/armagon/sight2.wav"; }
    if(xWhich == MG3_RO_SIGHT) { return "owned/mg3/armagon/sight.wav"; }
    return xOwn;
};

// Its pain (ogre_pain, before the ogre's): FALSE when it shrugs a blow off (upstream: random() * 200 > damage).
float(float xDamage) MG3_RocketOgre_Flinches =
{
    return self.vr_mg3_mon != MG3_MON_ROCKET_OGRE || random() * 200 <= xDamage;
};

void() MG3_OgreMissileTouch =
{
    if(other == self.owner)
    {
        return; // don't explode on owner
    }
    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }
    // Too close to its launch to explode: a stab (upstream: 20, a knight spike's spark).
    if(self.vr_mg3_close_until > time)
    {
        spawn_touchblood(18);
        T_Damage(other, self, self.owner, 20);
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_KNIGHTSPIKE);
        WriteVec3(MSG_BROADCAST, self.origin);
        remove(self);
        return;
    }
    float damg = 40;
    if(other.health)
    {
        if(other.classname == "monster_shambler")
        {
            T_Damage(other, self, self.owner, damg * 0.5);
        }
        else if(other.classname == "monster_zombie")
        {
            T_Damage(other, self, self.owner, damg + 20);
        }
        else
        {
            T_Damage(other, self, self.owner, damg);
        }
    }
    // don't do radius damage to the other, because all the damage was done in the impact
    T_RadiusDamage(self, self.owner, damg, other);
    self.origin = self.origin - 8 * normalize(self.velocity);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteVec3(MSG_BROADCAST, self.origin);
    BecomeExplosion();
};

// One rocket of its volley (upstream's OgreFireRocket): led at where the enemy will be (its speed over the flight at
// 1200), the second (and later) ones `64` units to a side at random, 600 u/s.
void() MG3_OgreFireRocket =
{
    makevectors(self.angles);
    float offset = self.vr_mg3_volley * 64;
    if(random() > 0.5)
    {
        offset = -offset;
    }
    float t = vlen(self.enemy.origin - self.origin) / 1200;
    vector lead = self.enemy.velocity;
    lead_z = 0;
    vector at = self.enemy.origin + '0 0 -8' + t * lead;
    vector v = normalize(at - (self.origin + '0 0 16') + v_right * offset) * 600;

    self.effects = self.effects | EF_MUZZLEFLASH;
    sound(self, CHAN_WEAPON, "weapons/sgun1.wav", 1, ATTN_NORM);
    entity missile = spawn();
    missile.owner = self;
    missile.movetype = MOVETYPE_FLYMISSILE;
    missile.solid = SOLID_BBOX;
    missile.classname = "ogre_missile";
    missile.velocity = v;
    missile.angles = vectoangles(missile.velocity);
    missile.touch = MG3_OgreMissileTouch;
    missile.vr_mg3_close_until = time + 0.2;
    missile.nextthink = time + 5;
    missile.think = SUB_Remove;
    setmodel(missile, "progs/missile.mdl");
    setsize(missile, '0 0 0', '0 0 0');
    setorigin(missile, self.origin + v_forward * 8 + '0 0 16');
    vr_mg3_rockets = vr_mg3_rockets + 1;
};

// The ogre's shot (ogre_nail4): the rocket ogre's rocket, TRUE while its volley has more (it aims again from $shoot2).
float() MG3_RocketOgre_Shoot =
{
    if(self.vr_mg3_mon != MG3_MON_ROCKET_OGRE)
    {
        return FALSE;
    }
    MG3_OgreFireRocket();
    self.vr_mg3_volley = self.vr_mg3_volley + 1;
    if(self.vr_mg3_volley < 2)
    {
        return TRUE;
    }
    self.vr_mg3_volley = 0;
    return FALSE;
};

// Its head (PositionalHead, weapons.qc): MG3's models' own, measured on their $stand1 (Misc/quakevr/ragdoll/headfit.py:
// the head's vertices, their middle and the radius holding most of them).
float(entity e) MG3_PositionalHead =
{
    if(e.vr_mg3_mon == MG3_MON_ROCKET_OGRE)
    {
        xHeadForward = 10; xHeadUp = 33; xHeadRadius = 9; // (headfit.py ogre_rocket 28 2 9: 9.4 0 33.5, 9.7)
        return TRUE;
    }
    if(e.vr_mg3_mon == MG3_MON_RANGED_KNIGHT)
    {
        xHeadForward = 7; xHeadUp = 33; xHeadRadius = 6.5; // (headfit.py rknight 28 0 6: 7.7 -1.1 33.0, 5.9; a helmet)
        return TRUE;
    }
    return FALSE;
};

/*QUAKED monster_ogre_rocket (1 0 0) (-32 -32 -24) (32 32 64) Ambush
Dawn of the Machine: the rocket ogre. 200 health, a chainsaw up close, volleys of two rockets at range. Needs the Dawn
of the Machine data (its model is read from it in place), else it is not there.
*/
void() monster_ogre_rocket =
{
    if(!VR_Pack_RequireSpawn(MG3_RocketOgre_Available(), "Dawn of the Machine"))
    {
        return;
    }
    MG3_RocketOgre_Precache();
    self.vr_mg3_mon = MG3_MON_ROCKET_OGRE;
    self.classname = "monster_ogre";
    if(!MG_WorldCampaign() && self.spawnflags <= 1)
    {
        self.spawnflags = self.spawnflags | OGRE_NORMAL; // (never Honey's random multi-grenade boss: it fires rockets)
    }
    self.wad = MG3_OGRE_ROCKET_MDL;
    monster_ogre_real();
};
