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
// Dawn of the Machine's super shambler, the blood shambler (monster_super_shambler; MG3.md M3-22), adapted from
// quakec_mg3/monsters/mg3_super_shambler.qc: the shambler's frames on MG3's own model (owned/mg3/progs/shambler_blood.mdl:
// 1076 vertices, 96 frames, the shambler's 94 first), 2000 health. Up close it smashes or claws, each blow with a spray of
// plasma (11 in a fan on the smash, 9 in a block on a claw) and after three to four blows (Bloody Nightmare: one fewer)
// its lightning; further off it closes with claws, or casts lightning (a long cast or a fast one, rate limited), reaching
// 600 to 1000 units. Half damage from explosions, as the shambler. Quake VR: its own ragdoll rig (vr_ragdoll.cpp
// shamblerBloodSeeds), head zone, beheading (the shambler's head gib) and limbs from the rig, knockdowns (its death
// backwards), parries, the shambler's ragdoll settings, small gibs and corpse health. Available wherever MG3's data is.

// (frames: the shambler's, as upstream; the model's 94 and 95 unused)
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8 stand9
$frame stand10 stand11 stand12 stand13 stand14 stand15 stand16 stand17
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7
$frame walk8 walk9 walk10 walk11 walk12
$frame run1 run2 run3 run4 run5 run6
$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7
$frame smash8 smash9 smash10 smash11 smash12
$frame swingr1 swingr2 swingr3 swingr4 swingr5
$frame swingr6 swingr7 swingr8 swingr9
$frame swingl1 swingl2 swingl3 swingl4 swingl5
$frame swingl6 swingl7 swingl8 swingl9
$frame magic1 magic2 magic3 magic4 magic5
$frame magic6 magic7 magic8 magic9 magic10 magic11 magic12
$frame pain1 pain2 pain3 pain4 pain5 pain6
$frame death1 death2 death3 death4 death5 death6
$frame death7 death8 death9 death10 death11

.entity supsham_child;      // its lightning's glow while it casts (s_light.mdl)
float mg3_supsham_plasma;   // plasma shots fired, lightning bolts cast (the tests' counts)
float mg3_supsham_bolts;

// A shambler, or Dawn of the Machine's: mostly immune to blasts and rockets (half), as upstream's combat.qc and weapons.qc.
float(entity e) VR_IsShambler =
{
    return e.classname == "monster_shambler" || e.classname == "monster_super_shambler";
};

float() MG3_SuperShambler_Available =
{
    return fileexists("owned/mg3/progs/shambler_blood.mdl") && fileexists("owned/mg3/progs/rogue/plasma.mdl");
};

void() supsham_run1;
void() supsham_magic1;
void() supsham_swingl1;
void() supsham_swingr1;

void(float f) supsham_setchildframe =
{
    if(self.supsham_child && self.supsham_child.owner == self && self.supsham_child.classname == "lightning_child")
        self.supsham_child.frame = f;
};

void() supsham_removechild =
{
    if(self.supsham_child && self.supsham_child.owner == self && self.supsham_child.classname == "lightning_child")
        remove(self.supsham_child);
    self.supsham_child = world;
};

// How high to throw its plasma: up at you above it, down at you below it, by distance on the level.
float() supsham_z_offset =
{
    vector at = VR_AI_Origin(self.enemy);
    float z = at_z - self.origin_z;
    if(z > 30)
        return 100 + crandom() * 50;
    if(z < -30)
        return -100 + crandom() * 50;
    z = vlen(at - self.origin);
    if(z > 300)
        return 50 + crandom() * 25;
    if(z < 150)
        return -50 + crandom() * 25;
    return 0;
};

// A plasma ball (the knights' spike's touch: 9) thrown along `vec` at `speed`, rising `up`.
void(vector org, vector vec, float speed, float up) supsham_plasma =
{
    entity m = launch_spike(org, vec);
    m.classname = "knightspike";
    setmodel(m, "owned/mg3/progs/rogue/plasma.mdl");
    m.avelocity = '300 300 300';
    m.avelocity_x = m.avelocity_x * crandom();
    m.avelocity_y = m.avelocity_y * crandom();
    m.avelocity_z = m.avelocity_z * crandom();
    m.movetype = MOVETYPE_TOSS;
    setsize(m, VEC_ORIGIN, VEC_ORIGIN);
    m.velocity = vec * speed;
    m.velocity_z = up + supsham_z_offset();
    mg3_supsham_plasma = mg3_supsham_plasma + 1;
};

// The smash's fan: 11 balls 5 degrees apart (upstream blast1 / supsham_shot).
void(vector posOffset) supsham_blast1 =
{
    if(!self.enemy || !VR_AI_ShotReady(self.origin)) return;
    float a = -5;
    while(a <= 5)
    {
        vector offang = vectoangles(VR_AI_Origin(self.enemy) - self.origin);
        offang_y = offang_y + a * 5;
        makevectors(offang);
        vector org = self.origin + self.mins + self.size * 0.5 + v_forward * 20 + posOffset;
        vector vec = normalize(v_forward);
        vec_z = 0 - vec_z + (random() - 0.5) * 0.1;
        supsham_plasma(org, vec, crandom() * 100 + 400, 125 + crandom() * 50);
        a++;
    }
    sound(self, CHAN_WEAPON, "zombie/z_shot1.wav", 1, ATTN_NORM);
};

// A claw's block: 3 x 3 balls about 12 degrees apart (upstream blast2 / supsham_shot2).
void(vector posOffset) supsham_blast2 =
{
    if(!self.enemy || !VR_AI_ShotReady(self.origin)) return;
    float x = -1;
    while(x < 2)
    {
        float y = -1;
        while(y < 2)
        {
            vector offang = vectoangles(VR_AI_Origin(self.enemy) - self.origin);
            offang_y = offang_y + x * (12 + crandom() * 4);
            offang_x = offang_x + y * (12 + crandom() * 4);
            makevectors(offang);
            vector org = self.origin + self.mins + self.size * 0.5 + v_forward * 20 + posOffset;
            supsham_plasma(org, normalize(v_forward), crandom() * 100 + 500, 100 + crandom() * 50);
            y++;
        }
        x++;
    }
};

// After three or four blows (one fewer in Bloody Nightmare) its lightning.
float() supsham_attack_check =
{
    float cap = 3 + floor(random() * 2 + 0.5);
    if(MG3_BloodyNightmare())
        cap = cap - 1;
    self.count = self.count + 1;
    return self.count > cap;
};

void() supsham_stand1 = [$stand1, supsham_stand2] {ai_stand();};
void() supsham_stand2 = [$stand2, supsham_stand3] {ai_stand();};
void() supsham_stand3 = [$stand3, supsham_stand4] {ai_stand();};
void() supsham_stand4 = [$stand4, supsham_stand5] {ai_stand();};
void() supsham_stand5 = [$stand5, supsham_stand6] {ai_stand();};
void() supsham_stand6 = [$stand6, supsham_stand7] {ai_stand();};
void() supsham_stand7 = [$stand7, supsham_stand8] {ai_stand();};
void() supsham_stand8 = [$stand8, supsham_stand9] {ai_stand();};
void() supsham_stand9 = [$stand9, supsham_stand10] {ai_stand();};
void() supsham_stand10 = [$stand10, supsham_stand11] {ai_stand();};
void() supsham_stand11 = [$stand11, supsham_stand12] {ai_stand();};
void() supsham_stand12 = [$stand12, supsham_stand13] {ai_stand();};
void() supsham_stand13 = [$stand13, supsham_stand14] {ai_stand();};
void() supsham_stand14 = [$stand14, supsham_stand15] {ai_stand();};
void() supsham_stand15 = [$stand15, supsham_stand16] {ai_stand();};
void() supsham_stand16 = [$stand16, supsham_stand17] {ai_stand();};
void() supsham_stand17 = [$stand17, supsham_stand1] {ai_stand();};

void() supsham_walk1 = [$walk1, supsham_walk2] {ai_walk(10);};
void() supsham_walk2 = [$walk2, supsham_walk3] {ai_walk(9);};
void() supsham_walk3 = [$walk3, supsham_walk4] {ai_walk(9);};
void() supsham_walk4 = [$walk4, supsham_walk5] {ai_walk(5);};
void() supsham_walk5 = [$walk5, supsham_walk6] {ai_walk(6);};
void() supsham_walk6 = [$walk6, supsham_walk7] {ai_walk(12);};
void() supsham_walk7 = [$walk7, supsham_walk8] {ai_walk(8);};
void() supsham_walk8 = [$walk8, supsham_walk9] {ai_walk(3);};
void() supsham_walk9 = [$walk9, supsham_walk10] {ai_walk(13);};
void() supsham_walk10 = [$walk10, supsham_walk11] {ai_walk(9);};
void() supsham_walk11 = [$walk11, supsham_walk12] {ai_walk(7);};
void() supsham_walk12 = [$walk12, supsham_walk1]
{
    ai_walk(7);
    if(random() > 0.8)
        sound(self, CHAN_VOICE, "shambler/sidle.wav", 1, ATTN_IDLE);
};

void() supsham_run1 = [$run1, supsham_run2] {supsham_removechild(); ai_run(20);};
void() supsham_run2 = [$run2, supsham_run3] {ai_run(24);};
void() supsham_run3 = [$run3, supsham_run4] {ai_run(20);};
void() supsham_run4 = [$run4, supsham_run5] {ai_run(20);};
void() supsham_run5 = [$run5, supsham_run6] {ai_run(24);};
void() supsham_run6 = [$run6, supsham_run1]
{
    ai_run(20);
    if(random() > 0.8)
        sound(self, CHAN_VOICE, "shambler/sidle.wav", 1, ATTN_IDLE);
};

void() supsham_smash1 = [$smash1, supsham_smash2]
{
    sound(self, CHAN_VOICE, "shambler/melee1.wav", 1, ATTN_NORM);
    ai_charge(2);
};
void() supsham_smash2 = [$smash2, supsham_smash3] {ai_charge(6);};
void() supsham_smash3 = [$smash3, supsham_smash4] {ai_charge(6);};
void() supsham_smash4 = [$smash4, supsham_smash5] {ai_charge(5);};
void() supsham_smash5 = [$smash5, supsham_smash6] {ai_charge(4);};
void() supsham_smash6 = [$smash6, supsham_smash7] {ai_charge(1);};
void() supsham_smash7 = [$smash7, supsham_smash8] {ai_charge(0);};
void() supsham_smash8 = [$smash8, supsham_smash9] {ai_charge(0);};
void() supsham_smash9 = [$smash9, supsham_smash10] {ai_charge(0);};
void() supsham_smash10 = [$smash10, supsham_smash11]
{
    supsham_blast1('0 0 0');
    if(!self.enemy)
        return;
    ai_charge(0);
    vector delta = self.enemy.origin - self.origin;
    if(vlen(delta) > 100)
        return;
    if(!CanDamage(self.enemy, self))
        return;
    float ldmg = (random() + random() + random()) * 40;
    T_Damage(self.enemy, self, self, ldmg);
    sound(self, CHAN_VOICE, "shambler/smack.wav", 1, ATTN_NORM);
    SpawnMeatSpray(self.origin + v_forward * 16, crandom() * 100 * v_right);
    SpawnMeatSpray(self.origin + v_forward * 16, crandom() * 100 * v_right);
};
void() supsham_smash11 = [$smash11, supsham_smash12] {ai_charge(5);};
void() supsham_smash12 = [$smash12, supsham_run1]
{
    ai_charge(4);
    if(supsham_attack_check())
        self.think = supsham_magic1;
};

void(float side) SupShamClaw =
{
    if(!self.enemy)
        return;
    ai_charge(10);
    vector delta = self.enemy.origin - self.origin;
    if(vlen(delta) > 100)
        return;
    float ldmg = (random() + random() + random()) * 20;
    T_Damage(self.enemy, self, self, ldmg);
    sound(self, CHAN_VOICE, "shambler/smack.wav", 1, ATTN_NORM);
    if(side)
    {
        makevectors(self.angles);
        SpawnMeatSpray(self.origin + v_forward * 16, side * v_right);
    }
};

void() supsham_swingl1 = [$swingl1, supsham_swingl2]
{
    sound(self, CHAN_VOICE, "shambler/melee2.wav", 1, ATTN_NORM);
    ai_charge(5);
};
void() supsham_swingl2 = [$swingl2, supsham_swingl3] {ai_charge(3);};
void() supsham_swingl3 = [$swingl3, supsham_swingl4] {ai_charge(7);};
void() supsham_swingl4 = [$swingl4, supsham_swingl5] {ai_charge(3);};
void() supsham_swingl5 = [$swingl5, supsham_swingl6] {ai_charge(7);};
void() supsham_swingl6 = [$swingl6, supsham_swingl7] {ai_charge(9);};
void() supsham_swingl7 = [$swingl7, supsham_swingl8]
{
    makevectors(self.angles);
    supsham_blast2(v_right * 16 + '0 0 32');
    ai_charge(5);
    SupShamClaw(250);
};
void() supsham_swingl8 = [$swingl8, supsham_swingl9] {ai_charge(4);};
void() supsham_swingl9 = [$swingl9, supsham_run1]
{
    ai_charge(8);
    makevectors(self.angles);
    if(supsham_attack_check())
        self.think = supsham_magic1;
    else if(random() < 0.5)
        self.think = supsham_swingr1;
};

void() supsham_swingr1 = [$swingr1, supsham_swingr2]
{
    sound(self, CHAN_VOICE, "shambler/melee1.wav", 1, ATTN_NORM);
    ai_charge(1);
};
void() supsham_swingr2 = [$swingr2, supsham_swingr3] {ai_charge(8);};
void() supsham_swingr3 = [$swingr3, supsham_swingr4] {ai_charge(14);};
void() supsham_swingr4 = [$swingr4, supsham_swingr5] {ai_charge(7);};
void() supsham_swingr5 = [$swingr5, supsham_swingr6] {ai_charge(3);};
void() supsham_swingr6 = [$swingr6, supsham_swingr7] {ai_charge(6);};
void() supsham_swingr7 = [$swingr7, supsham_swingr8]
{
    makevectors(self.angles);
    supsham_blast2(v_right * -16 + '0 0 32');
    ai_charge(6);
    SupShamClaw(-250);
};
void() supsham_swingr8 = [$swingr8, supsham_swingr9] {ai_charge(3);};
void() supsham_swingr9 = [$swingr9, supsham_run1]
{
    ai_charge(1);
    ai_charge(10);
    makevectors(self.angles);
    if(supsham_attack_check())
        self.think = supsham_magic1;
    else if(random() < 0.5)
        self.think = supsham_swingl1;
};

void() supsham_magic_b1;

// Its attack choice (melee and missile alike): smash or claw up close, claws while near (or its lightning rested),
// else lightning (a long cast 60%, rested 5 s, or a fast one, rested 3 s).
void() supsham_melee =
{
    vector vec = self.origin - VR_AI_Origin(self.enemy);
    float dist = vlen(vec);
    if(dist < 110)
    {
        if(random() < 0.8)
            supsham_smash1();
        else if(random() < 0.5)
            supsham_swingl1();
        else
            supsham_swingr1();
    }
    else if(dist < 200 || self.wait > time)
    {
        makevectors(self.angles);
        float chance = v_right * normalize(vec);
        if(chance < 0.45)
            supsham_swingr1();
        else if(chance < 0.9)
            supsham_swingl1();
        else
            supsham_smash1();
    }
    else
    {
        self.count = 0;
        if(random() > 0.4)
        {
            supsham_magic1();
            self.wait = time + 5;
        }
        else
        {
            supsham_magic_b1();
            self.wait = time + 3;
        }
    }
};

// A bolt from its hands at you: reaching 600 to 1000 units (upstream SupCastLightning: the shambler's reaches 600).
void() SupCastLightning =
{
    self.effects = self.effects | EF_MUZZLEFLASH;
    ai_face();
    vector org = self.origin + '0 0 40';
    vector dir = VR_AI_Origin(self.enemy) + '0 0 16' - org;
    float dist = max(600, min(vlen(dir), 1000));
    self.frags = self.frags + 1; // (upstream: its casts counted on .frags)
    mg3_supsham_bolts = mg3_supsham_bolts + 1;
    VR_AI_Lightning(org, normalize(dir), dist, TE_LIGHTNING1, 10);
};

// Its lightning's glow over its hands (`xLife` s).
void(float xLife) supsham_makechild =
{
    self.effects = self.effects | EF_MUZZLEFLASH;
    ai_face();
    supsham_removechild();
    entity o = spawn();
    self.supsham_child = o;
    o.owner = self;
    o.classname = "lightning_child";
    setmodel(o, "progs/s_light.mdl");
    setorigin(o, self.origin);
    o.angles = self.angles;
    o.nextthink = time + xLife;
    o.think = SUB_Remove;
};

void() supsham_magic1 = [$magic1, supsham_magic2]
{
    ai_face();
    sound(self, CHAN_WEAPON, "shambler/sattck1.wav", 1, ATTN_NORM);
    self.count = 0;
};
void() supsham_magic2 = [$magic2, supsham_magic3] {ai_face();};
void() supsham_magic3 = [$magic3, supsham_magic4] {ai_face(); self.nextthink = self.nextthink + 0.2; supsham_makechild(1.4);};
void() supsham_magic4 = [$magic4, supsham_magic5] {self.effects = self.effects | EF_MUZZLEFLASH; supsham_setchildframe(1);};
void() supsham_magic5 = [$magic5, supsham_magic4b] {self.effects = self.effects | EF_MUZZLEFLASH; supsham_setchildframe(2);};
void() supsham_magic4b = [$magic4, supsham_magic5b] {self.effects = self.effects | EF_MUZZLEFLASH; supsham_setchildframe(3);};
void() supsham_magic5b = [$magic5, supsham_magic4c] {self.effects = self.effects | EF_MUZZLEFLASH; supsham_setchildframe(2);};
void() supsham_magic4c = [$magic4, supsham_magic5c] {self.effects = self.effects | EF_MUZZLEFLASH; supsham_setchildframe(1);};
void() supsham_magic5c = [$magic5, supsham_magic6] {self.effects = self.effects | EF_MUZZLEFLASH; supsham_setchildframe(2);};
void() supsham_magic6 = [$magic6, supsham_magic9]
{
    supsham_removechild();
    SupCastLightning();
    sound(self, CHAN_WEAPON, "shambler/sboom.wav", 1, ATTN_NORM);
};
void() supsham_magic9 = [$magic9, supsham_magic10] {SupCastLightning();};
void() supsham_magic10 = [$magic10, supsham_magic11] {SupCastLightning();};
void() supsham_magic11 = [$magic11, supsham_magic12] {};
void() supsham_magic12 = [$magic12, supsham_run1] {};

void() supsham_magic_b1 = [$magic1, supsham_magic_b2]
{
    ai_face();
    sound(self, CHAN_WEAPON, "shambler/sattck1.wav", 1, ATTN_NORM);
    self.count = 0;
};
void() supsham_magic_b2 = [$magic2, supsham_magic_b3] {ai_face();};
void() supsham_magic_b3 = [$magic3, supsham_magic_b4] {ai_face(); self.nextthink = self.nextthink + 0.2; supsham_makechild(0.7);};
void() supsham_magic_b4 = [$magic4, supsham_magic_b5] {self.effects = self.effects | EF_MUZZLEFLASH; supsham_setchildframe(1);};
void() supsham_magic_b5 = [$magic5, supsham_magic_b6] {self.effects = self.effects | EF_MUZZLEFLASH; supsham_setchildframe(2);};
void() supsham_magic_b6 = [$magic6, supsham_magic_b9]
{
    supsham_removechild();
    SupCastLightning();
    sound(self, CHAN_WEAPON, "shambler/sboom.wav", 1, ATTN_NORM);
};
void() supsham_magic_b9 = [$magic9, supsham_magic_b10] {SupCastLightning();};
void() supsham_magic_b10 = [$magic10, supsham_magic_b11] {SupCastLightning();};
void() supsham_magic_b11 = [$magic11, supsham_magic_b12] {};
void() supsham_magic_b12 = [$magic12, supsham_run1] {};

void() supsham_pain1 = [$pain1, supsham_pain2] {};
void() supsham_pain2 = [$pain2, supsham_pain3] {};
void() supsham_pain3 = [$pain3, supsham_pain4] {};
void() supsham_pain4 = [$pain4, supsham_pain5] {};
void() supsham_pain5 = [$pain5, supsham_pain6] {};
void() supsham_pain6 = [$pain6, supsham_run1] {};

void(entity attacker, float damage) supsham_pain =
{
    sound(self, CHAN_VOICE, "shambler/shurt2.wav", 1, ATTN_NORM);
    if(self.health <= 0)
        return; // already dying
    if(25 + random() * 400 > damage)
        return; // didn't flinch
    if(self.pain_finished > time)
        return;
    self.pain_finished = time + 5;
    supsham_removechild();
    supsham_pain1();
};

// Dying, the super shamblers it owns die too (upstream cleanup_orbs: a boss's summons).
void() supsham_cleanup =
{
    entity f = find(world, classname, "monster_super_shambler");
    while(f)
    {
        if(f.owner && f.owner == self && f.health > 0)
        {
            f.think = f.th_die;
            f.nextthink = time + 0.1;
        }
        f = find(f, classname, "monster_super_shambler");
    }
};

void() supsham_death1 = [$death1, supsham_death2] {};
void() supsham_death2 = [$death2, supsham_death3] {};
void() supsham_death3 = [$death3, supsham_death4] {self.solid = SOLID_NOT; supsham_cleanup();};
void() supsham_death4 = [$death4, supsham_death5] {};
void() supsham_death5 = [$death5, supsham_death6] {};
void() supsham_death6 = [$death6, supsham_death7] {};
void() supsham_death7 = [$death7, supsham_death8] {};
void() supsham_death8 = [$death8, supsham_death9] {};
void() supsham_death9 = [$death9, supsham_death10] {};
void() supsham_death10 = [$death10, supsham_death11] {};
void() supsham_death11 = [$death11, supsham_death11] {};

void() supsham_die =
{
    supsham_removechild();
    if(self.health < -60)
    {
        sound(self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
        ThrowHead("progs/h_shams.mdl", self.health);
        ThrowGib("progs/gib1.mdl", self.health);
        ThrowGib("progs/gib2.mdl", self.health);
        ThrowGib("progs/gib3.mdl", self.health);
        return;
    }
    sound(self, CHAN_VOICE, "shambler/sdeath.wav", 1, ATTN_NORM);
    supsham_death1();
};

void() monster_super_shambler_precache =
{
    precache_model("owned/mg3/progs/shambler_blood.mdl");
    precache_model("owned/mg3/progs/rogue/plasma.mdl");
    precache_model("progs/s_light.mdl");
    precache_model("progs/h_shams.mdl");
    precache_model("progs/bolt.mdl");
    precache_sound("shambler/sattck1.wav");
    precache_sound("shambler/sboom.wav");
    precache_sound("shambler/sdeath.wav");
    precache_sound("shambler/shurt2.wav");
    precache_sound("shambler/sidle.wav");
    precache_sound("shambler/ssight.wav");
    precache_sound("shambler/melee1.wav");
    precache_sound("shambler/melee2.wav");
    precache_sound("shambler/smack.wav");
    precache_model("progs/zom_gib.mdl");
    precache_sound("zombie/z_shot1.wav");
    precache_model("progs/k_spike.mdl");
    precache_sound("hknight/attack1.wav");
};

/*QUAKED monster_super_shambler (1 0 0) (-32 -32 -24) (32 32 64) Ambush
Dawn of the Machine's blood shambler (needs the owned Dawn of the Machine data, else nothing spawns): 2000 health; its smashes and claws spray plasma, it casts lightning near and far (600 to 1000 units), half damage from explosions. 'health_target': its health relays.
*/
void() monster_super_shambler =
{
    if(MG_MonsterInhibited()) return;
    VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_shambler, $death11, $death1, -1, -1); // (the shambler's: its death backwards)
    if(deathmatch)
    {
        remove(self);
        return;
    }
    if(!VR_Pack_RequireSpawn(MG3_SuperShambler_Available(), "Dawn of the Machine")) return;

    monster_super_shambler_precache();

    self.classname = "monster_super_shambler";
    self.health = 2000;
    self.frags = 0;

    self.th_stand = supsham_stand1;
    self.th_walk = supsham_walk1;
    self.th_run = supsham_run1;
    self.th_die = supsham_die;
    self.th_melee = supsham_melee;
    self.th_missile = supsham_melee;
    self.th_pain = supsham_pain;

    monster_add_to_counter();

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;

    setmodel(self, "owned/mg3/progs/shambler_blood.mdl");
    setsize(self, VEC_HULL2_MIN, VEC_HULL2_MAX);

    walkmonster_start();
};
