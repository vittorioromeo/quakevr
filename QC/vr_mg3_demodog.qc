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
// Dawn of the Machine's demo dog (MG3_PLAN.md, M3-17), adapted from the snapshot's monsters/mg3_demodog.qc
// (monster_demodog, DemoDog_JumpTouch, demodog_spawn_grenade, demodog_die; client.qc's obituary;
// docs/vr-port/OFFICIAL_QC_SOURCE.md).
//
// Quake VR's rottweiler (orig_mon_dog.qc: frames, bite, leap, its parry, knockdown) drawn with MG3's model read in place
// (owned/mg3/progs/dog_explosive.mdl), so in any campaign when the Dawn of the Machine data is there (decision 5), removed
// where it is not. Its class the rottweiler's (as upstream: its sight and idle sounds, the rottweiler's head zone, which
// its model's head is: the same mesh; beheading, ragdoll settings, hulls, grapple), marked .vr_mg3_mon MG3_MON_DEMODOG;
// its own rig (vr_ragdoll.cpp dogExplosiveSeeds). Kamikaze: its leap landing on a player at speed hurts him (10..20)
// and kills the dog there (upstream: the player deals it 200), unless the leap was parried (Quake VR's: the dog
// staggers). However it dies (shot, blown up, thrown down from a knockdown, its own leap), its bomb pack spills: three
// grenades (a fourth now and then on Nightmare) that bounce and go off 2.5 s later (60 each, catchable as any monster's
// grenade: vr_grenade.qc), owned by the dog (their kills are its). It always bursts into gibs and its head, as
// upstream; beheaded or a limb cut off by the killing blow, it lies as a ragdoll instead (the cut is what is seen; the
// grenades spill all the same: Quake VR's treatment, as a beheaded zombie lies: VR_Decap_ZombieDie).

string MG3_DEMODOG_MDL = "owned/mg3/progs/dog_explosive.mdl";
float vr_mg3_demo_grenades;      // demo dogs' grenades spilled since the map started (the tests)

float() MG3_DemoDog_Available = { return fileexists(MG3_DEMODOG_MDL); };

void() MG3_DemoDog_Precache =
{
    precache_model(MG3_DEMODOG_MDL);
    precache_model("progs/grenade.mdl");
    precache_model("progs/gib3.mdl");
    precache_sound("weapons/grenade.wav");
    precache_sound("weapons/bounce.wav");
    precache_sound("weapons/r_exp3.wav");
    precache_sound("player/udeath.wav");
};

void() MG3_DemoGrenadeExplode =
{
    T_RadiusDamage(self, self.owner, self.dmg, world);
    sound(self, CHAN_VOICE, "weapons/r_exp3.wav", 1, ATTN_NORM);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteVec3(MSG_BROADCAST, self.origin);
    self.velocity = '0 0 0';
    self.touch = SUB_Null;
    setmodel(self, "progs/s_explod.spr");
    self.solid = SOLID_NOT;
    s_explode1();
};

void() MG3_DemoGrenadeTouch =
{
    if(other == self.owner)
    {
        return; // don't explode on owner
    }
    if(other.takedamage == DAMAGE_AIM)
    {
        MG3_DemoGrenadeExplode();
        return;
    }
    sound(self, CHAN_VOICE, "weapons/bounce.wav", 1, ATTN_NORM);
    if(self.velocity == '0 0 0')
    {
        self.avelocity = '0 0 0';
    }
};

// One grenade of its pack (upstream's demodog_spawn_grenade): tossed up and about at random, ahead of it (`xUp`: straight
// up, the Nightmare one), 60, 2.5 s (a quarter either way).
void(float xUp) MG3_DemoDog_Grenade =
{
    vector v;
    v_x = 100 * crandom();
    v_y = 100 * crandom();
    v_z = 200 + 100 * random();
    v = v * 1.5;
    if(xUp)
    {
        v_x = 0;
        v_y = 0;
    }
    makevectors(self.angles);
    v = v + v_forward * 100;
    sound(self, CHAN_WEAPON, "weapons/grenade.wav", 1, ATTN_NORM);
    entity g = spawn();
    g.owner = self;
    g.movetype = MOVETYPE_BOUNCE;
    g.solid = SOLID_BBOX;
    g.classname = "grenade";
    g.dmg = 60;
    g.velocity = v;
    g.avelocity = '300 300 300';
    g.angles = vectoangles(g.velocity);
    g.touch = MG3_DemoGrenadeTouch;
    float fuse = 2.5 + 0.25 * crandom();
    g.nextthink = time + fuse;
    g.think = MG3_DemoGrenadeExplode;
    setmodel(g, "progs/grenade.mdl");
    setsize(g, '0 0 0', '0 0 0');
    setorigin(g, self.origin);
    VR_Grenade_Make(g, MG3_DemoGrenadeExplode, fuse, FALSE); // (a physics object you can catch: vr_grenade_catch)
    vr_mg3_demo_grenades = vr_mg3_demo_grenades + 1;
};

// Its death (dog_die, for it): the pack spills; it bursts, or (beheaded, a limb cut) lies as its ragdoll.
void() MG3_DemoDog_Die =
{
    MG3_DemoDog_Grenade(FALSE);
    MG3_DemoDog_Grenade(FALSE);
    MG3_DemoDog_Grenade(FALSE);
    if(skill > 2 && random() > 0.7)
    {
        MG3_DemoDog_Grenade(TRUE);
    }
    sound(self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
    if(self.vr_headless || self.vr_limbcut)
    {
        VR_Decap_ZombieDie(); // (its ragdoll, headless or a limb short: vr_decap.qc)
        return;
    }
    self.health = min(self.health, -50); // (upstream: always gibbed, its pieces thrown as hard)
    ThrowGib("progs/gib3.mdl", self.health);
    ThrowGib("progs/gib3.mdl", self.health);
    ThrowGib("progs/gib3.mdl", self.health);
    ThrowHead("progs/h_dog.mdl", self.health);
};

// Its leap's touch on `other` (Dog_JumpTouch, after the bite of it landed and was not parried): a player it lands on
// at speed kills it (upstream: T_Damage(self, other, other, 200)). TRUE: so it died.
float() MG3_DemoDog_LeapHit =
{
    if(self.vr_mg3_mon != MG3_MON_DEMODOG || !(other.flags & FL_CLIENT) || self.health <= 0)
    {
        return FALSE;
    }
    T_Damage(self, other, other, 200);
    return TRUE;
};

/*QUAKED monster_demodog (1 0 0) (-32 -32 -24) (32 32 40) Ambush
Dawn of the Machine: the demo dog. 25 health; a rottweiler with a bomb pack: its leap onto you kills it, and however it
dies three grenades spill (60 each, 2.5 s). Needs the Dawn of the Machine data (its model is read from it in place),
else it is not there.
*/
void() monster_demodog =
{
    if(!VR_Pack_RequireSpawn(MG3_DemoDog_Available(), "Dawn of the Machine"))
    {
        return;
    }
    MG3_DemoDog_Precache();
    self.vr_mg3_mon = MG3_MON_DEMODOG;
    self.classname = "monster_dog";
    monster_dog();
    if(self.model != "")
    {
        setmodel(self, MG3_DEMODOG_MDL);
        setsize(self, '-12 -12 -24', '12 12 16'); // (monster_dog's)
    }
};
