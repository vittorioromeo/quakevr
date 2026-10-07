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
// Dawn of the Machine's Chthon (monster_boss_final; MG3_PLAN.md M3-24/25), adapted from quakec_mg3/monsters/boss_final.qc:
// id's Chthon (progs/boss.mdl) woken by his map's trigger, 12000 health, killable (spawnflag 2). He throws fans of MG3's
// spheres (lava balls without spawnflag 2) and, after his first fits, volleys of spheres at you; six fits as he is hurt
// (first blood, then 83%, 66%, 50%, 33%, 16% of his health), each with lightning and immune while it plays. After a fit
// his phase steps up: 2 sends wave1, 3 takes him and the players to the second arena (his tele_target fired; the
// players to the info_boss_teleport_first points, he sinks and waits for trigger_boss_teleport), 4 sends wave2, 5 wave3
// and a spiral of plasma. Killed: rings of spheres, his targets used up to five times in all, a shower of gibs, and
// (M3-25) the ending. Damage (killable): none during a fit, none from lava men, 0.8 of anything but the lightning gun's
// and the laser cannon's (combat.qc, MG3_ChthonDamage). Quake VR: a head zone (positional damage), his rocks batted as
// any monster's missiles, what the players carry comes along when they are moved, gibs that fly and stick as Quake VR's.
// Available wherever MG3's data is (Debug spawner 50, training dummy 40): without the second arena's points he fights on
// where he is, and only the map's Chthon in Dawn of the Machine ends the game.

$frame rise1 rise2 rise3 rise4 rise5 rise6 rise7 rise8 rise9 rise10
$frame rise11 rise12 rise13 rise14 rise15 rise16 rise17

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12 walk13 walk14 walk15
$frame walk16 walk17 walk18 walk19 walk20 walk21 walk22
$frame walk23 walk24 walk25 walk26 walk27 walk28 walk29 walk30 walk31

$frame death1 death2 death3 death4 death5 death6 death7 death8 death9

$frame attack1 attack2 attack3 attack4 attack5 attack6 attack7 attack8
$frame attack9 attack10 attack11 attack12 attack13 attack14 attack15
$frame attack16 attack17 attack18 attack19 attack20 attack21 attack22
$frame attack23

$frame shocka1 shocka2 shocka3 shocka4 shocka5 shocka6 shocka7 shocka8
$frame shocka9 shocka10

$frame shockb1 shockb2 shockb3 shockb4 shockb5 shockb6

$frame shockc1 shockc2 shockc3 shockc4 shockc5 shockc6 shockc7 shockc8
$frame shockc9 shockc10

float MG3_CHTHON_KILLABLE = 2; // spawnflag (upstream BOSS_KILLABLE): spheres instead of lava balls; the damage rules

.float mg3_ch_volley, mg3_ch_left; // a volley's size and the shots left in it (upstream ammo_shells, ammo_nails)
// Counts for the tests (vr_mg3_chthon_test.qc): spheres, lava balls, volleys, spirals, gibs, and the last pain's phase.
float mg3_chthon_spheres, mg3_chthon_lavaballs, mg3_chthon_volleys, mg3_chthon_spirals, mg3_chthon_gibs;
float mg3_chthon_fades, mg3_chthon_endings; // comfort fades sent, endings started

float() MG3_Chthon_Available =
{
    return fileexists("progs/boss.mdl") && fileexists("owned/mg3/progs/rogue/sphere.mdl") &&
           fileexists("owned/mg3/progs/rogue/plasma.mdl");
};

void() boss_final_missile1;
void() boss_final_upgrade;
void() boss_final_death1;
void(entity attacker, float damage) boss_final_pain;
void(vector p) boss_final_missile;
void(vector p, float spread, float fx) boss_final_missile_blast;
void() MG3_Chthon_Spiral;
void(float wait) MG3_Chthon_CircleLater;
void(float variant) MG3_Chthon_Teleport;
void() MG3_Chthon_Finale;

// He looks at his enemy, another player now and then (upstream: so he stops twitching between dead players' bodies).
void() boss_final_face =
{
    if(self.enemy.health <= 0 || random() < 0.02)
    {
        self.enemy = find(self.enemy, classname, "player");
        if(!self.enemy)
            self.enemy = find(self.enemy, classname, "player");
    }
    if(self.enemy.health > 0)
        ai_face();
};

void() boss_final_rise1  = [$rise1,  boss_final_rise2 ] { sound(self, CHAN_WEAPON, "boss1/out1.wav", 1, ATTN_NORM); };
void() boss_final_rise2  = [$rise2,  boss_final_rise3 ] { sound(self, CHAN_VOICE, "boss1/sight1.wav", 1, ATTN_NORM); };
void() boss_final_rise3  = [$rise3,  boss_final_rise4 ] {};
void() boss_final_rise4  = [$rise4,  boss_final_rise5 ] {};
void() boss_final_rise5  = [$rise5,  boss_final_rise6 ] {};
void() boss_final_rise6  = [$rise6,  boss_final_rise7 ] {};
void() boss_final_rise7  = [$rise7,  boss_final_rise8 ] {};
void() boss_final_rise8  = [$rise8,  boss_final_rise9 ] {};
void() boss_final_rise9  = [$rise9,  boss_final_rise10] {};
void() boss_final_rise10 = [$rise10, boss_final_rise11] {};
void() boss_final_rise11 = [$rise11, boss_final_rise12] {};
void() boss_final_rise12 = [$rise12, boss_final_rise13] {};
void() boss_final_rise13 = [$rise13, boss_final_rise14] {};
void() boss_final_rise14 = [$rise14, boss_final_rise15] {};
void() boss_final_rise15 = [$rise15, boss_final_rise16] {};
void() boss_final_rise16 = [$rise16, boss_final_rise17] {};
void() boss_final_rise17 = [$rise17, boss_final_missile1] {};

void() boss_final_idle1  = [$walk1,  boss_final_idle2 ] {};
void() boss_final_idle2  = [$walk2,  boss_final_idle3 ] { boss_final_face(); };
void() boss_final_idle3  = [$walk3,  boss_final_idle4 ] { boss_final_face(); };
void() boss_final_idle4  = [$walk4,  boss_final_idle5 ] { boss_final_face(); };
void() boss_final_idle5  = [$walk5,  boss_final_idle6 ] { boss_final_face(); };
void() boss_final_idle6  = [$walk6,  boss_final_idle7 ] { boss_final_face(); };
void() boss_final_idle7  = [$walk7,  boss_final_idle8 ] { boss_final_face(); };
void() boss_final_idle8  = [$walk8,  boss_final_idle9 ] { boss_final_face(); };
void() boss_final_idle9  = [$walk9,  boss_final_idle10] { boss_final_face(); };
void() boss_final_idle10 = [$walk10, boss_final_idle11] { boss_final_face(); };
void() boss_final_idle11 = [$walk11, boss_final_idle12] { boss_final_face(); };
void() boss_final_idle12 = [$walk12, boss_final_idle13] { boss_final_face(); };
void() boss_final_idle13 = [$walk13, boss_final_idle14] { boss_final_face(); };
void() boss_final_idle14 = [$walk14, boss_final_idle15] { boss_final_face(); };
void() boss_final_idle15 = [$walk15, boss_final_idle16] { boss_final_face(); };
void() boss_final_idle16 = [$walk16, boss_final_idle17] { boss_final_face(); };
void() boss_final_idle17 = [$walk17, boss_final_idle18] { boss_final_face(); };
void() boss_final_idle18 = [$walk18, boss_final_idle19] { boss_final_face(); };
void() boss_final_idle19 = [$walk19, boss_final_idle20] { boss_final_face(); };
void() boss_final_idle20 = [$walk20, boss_final_idle21] { boss_final_face(); };
void() boss_final_idle21 = [$walk21, boss_final_idle22] { boss_final_face(); };
void() boss_final_idle22 = [$walk22, boss_final_idle23] { boss_final_face(); };
void() boss_final_idle23 = [$walk23, boss_final_idle24] { boss_final_face(); };
void() boss_final_idle24 = [$walk24, boss_final_idle25] { boss_final_face(); };
void() boss_final_idle25 = [$walk25, boss_final_idle26] { boss_final_face(); };
void() boss_final_idle26 = [$walk26, boss_final_idle27] { boss_final_face(); };
void() boss_final_idle27 = [$walk27, boss_final_idle28] { boss_final_face(); };
void() boss_final_idle28 = [$walk28, boss_final_idle29] { boss_final_face(); };
void() boss_final_idle29 = [$walk29, boss_final_idle30] { boss_final_face(); };
void() boss_final_idle30 = [$walk30, boss_final_idle31] { boss_final_face(); };
// (upstream: he takes up a target again at the loop's end)
void() boss_final_idle31 = [$walk31, boss_final_idle1 ]
{
    boss_final_face();
    if(self.enemy)
        boss_final_missile1();
    else
        boss_final_idle1();
};

// A volley (after a throw, from his second phase on, 30% of the time): 15 to 25 blasts of spheres (skill 0; 20 to 30 on
// skill 1, 25 to 35 on 2, 30 to 40 on 3) at 10 a second, spreading wider as it goes, every other one wide with a flash.
void() boss_final_mg1 = [$attack1, boss_final_mg2] { boss_final_face(); };
void() boss_final_mg2 = [$attack2, boss_final_mg3] { boss_final_face(); };
void() boss_final_mg3 = [$attack3, boss_final_mg4] { boss_final_face(); };
void() boss_final_mg4 = [$attack4, boss_final_mg5] { boss_final_face(); };
void() boss_final_mg5 = [$attack5, boss_final_mg6] { boss_final_face(); };
void() boss_final_mg6 = [$attack6, boss_final_mg7] { boss_final_face(); };
void() boss_final_mg7 = [$attack7, boss_final_mg8]
{
    self.mg3_ch_volley = ((skill + 3) / 6) * 30;
    self.mg3_ch_volley = self.mg3_ch_volley + floor(crandom() * 10 + 0.5);
    self.mg3_ch_left = self.mg3_ch_volley;
    mg3_chthon_volleys = mg3_chthon_volleys + 1;
    if(skill > 0)
        self.attack_finished = time + 5; // (no fit interrupts it)
    boss_final_face();
};
void() boss_final_mg8 = [$attack8, boss_final_missile1]
{
    float spread = 1 - (self.mg3_ch_left / max(1, self.mg3_ch_volley)); // 0 at first, 1 at the end
    spread = spread * spread;
    float fx = 0;
    if(self.mg3_ch_left % 2 == 0)
        fx = 1; // (wide)
    else
        spread = spread * 0.5; // (focused)
    boss_final_missile_blast('270 60 210', spread, fx);
    if(self.mg3_ch_left > 0)
        self.think = boss_final_mg8;
    self.mg3_ch_left = self.mg3_ch_left - 1;
    boss_final_face();
};

void() boss_final_missile1  = [$attack1,  boss_final_missile2 ] { self.mg3_chthon_immune = 0; boss_final_face(); };
void() boss_final_missile2  = [$attack2,  boss_final_missile3 ] { boss_final_face(); };
void() boss_final_missile3  = [$attack3,  boss_final_missile4 ] { boss_final_face(); };
void() boss_final_missile4  = [$attack4,  boss_final_missile5 ] { boss_final_face(); };
void() boss_final_missile5  = [$attack5,  boss_final_missile6 ] { boss_final_face(); };
void() boss_final_missile6  = [$attack6,  boss_final_missile7 ] { boss_final_face(); };
void() boss_final_missile7  = [$attack7,  boss_final_missile8 ] { boss_final_face(); };
void() boss_final_missile8  = [$attack8,  boss_final_missile9 ] { boss_final_face(); };
void() boss_final_missile9  = [$attack9,  boss_final_missile10] { boss_final_face(); };
void() boss_final_missile10 = [$attack10, boss_final_missile11] { boss_final_missile('200 100 60'); };
void() boss_final_missile11 = [$attack11, boss_final_missile12] { boss_final_face(); };
void() boss_final_missile12 = [$attack12, boss_final_missile13] { boss_final_face(); };
void() boss_final_missile13 = [$attack13, boss_final_missile14] { boss_final_face(); };
void() boss_final_missile14 = [$attack14, boss_final_missile15] { boss_final_face(); };
void() boss_final_missile15 = [$attack15, boss_final_missile16] { boss_final_face(); };
void() boss_final_missile16 = [$attack16, boss_final_missile17] { boss_final_face(); };
void() boss_final_missile17 = [$attack17, boss_final_missile18] { boss_final_face(); };
void() boss_final_missile18 = [$attack18, boss_final_missile19] { boss_final_face(); };
void() boss_final_missile19 = [$attack19, boss_final_missile20] { boss_final_face(); };
void() boss_final_missile20 = [$attack20, boss_final_missile21] { boss_final_face(); };
void() boss_final_missile21 = [$attack21, boss_final_missile22] { boss_final_missile('200 -100 60'); };
void() boss_final_missile22 = [$attack22, boss_final_missile23] { boss_final_face(); };
void() boss_final_decide;
void() boss_final_missile23 = [$attack23, boss_final_decide  ] { boss_final_face(); };
// After a throw: from his second phase on, a volley 30% of the time; else another throw.
void() boss_final_decide =
{
    if(self.wait > 0 && random() < 0.3)
        boss_final_mg1();
    else
        boss_final_missile1();
};

// His fits: lightning from his body, then his phase steps up (boss_final_upgrade).
void() boss_final_shocka1  = [$shocka1,  boss_final_shocka2 ] {};
void() boss_final_shocka2  = [$shocka2,  boss_final_shocka3 ] { MG3_PainLightning('0 0 100'); };
void() boss_final_shocka3  = [$shocka3,  boss_final_shocka4 ] {};
void() boss_final_shocka4  = [$shocka4,  boss_final_shocka5 ] {};
void() boss_final_shocka5  = [$shocka5,  boss_final_shocka6 ] { MG3_PainLightning('0 0 100'); };
void() boss_final_shocka6  = [$shocka6,  boss_final_shocka7 ] {};
void() boss_final_shocka7  = [$shocka7,  boss_final_shocka8 ] {};
void() boss_final_shocka8  = [$shocka8,  boss_final_shocka9 ] { MG3_PainLightning('0 0 100'); };
void() boss_final_shocka9  = [$shocka9,  boss_final_shocka10] {};
void() boss_final_shocka10 = [$shocka10, boss_final_missile1] { boss_final_upgrade(); };

void() boss_final_shockb1  = [$shockb1, boss_final_shockb2 ] {};
void() boss_final_shockb2  = [$shockb2, boss_final_shockb3 ] { MG3_PainLightning('0 0 100'); };
void() boss_final_shockb3  = [$shockb3, boss_final_shockb4 ] {};
void() boss_final_shockb4  = [$shockb4, boss_final_shockb5 ] {};
void() boss_final_shockb5  = [$shockb5, boss_final_shockb6 ] { MG3_PainLightning('0 0 100'); };
void() boss_final_shockb6  = [$shockb6, boss_final_shockb7 ] {};
void() boss_final_shockb7  = [$shockb1, boss_final_shockb8 ] {};
void() boss_final_shockb8  = [$shockb2, boss_final_shockb9 ] { MG3_PainLightning('0 0 100'); };
void() boss_final_shockb9  = [$shockb3, boss_final_shockb10] {};
void() boss_final_shockb10 = [$shockb4, boss_final_missile1] { boss_final_upgrade(); };

// Whether this map has the second arena (its info_boss_teleport_first points): the phase 3 move. Without them (the Debug
// spawner's Chthon, another map's) he fights on where he is.
float() MG3_Chthon_HasArena = { return find(world, classname, "info_boss_teleport_first") != world; };

// Fires his `xTargets` (one of his wave fields): what his target would.
void(string xTargets) MG3_Chthon_UseField =
{
    string oldtarget = self.target;
    self.target = xTargets;
    activator = self.enemy;
    SUB_UseTargets();
    self.target = oldtarget;
};

// After a fit: phase 2 sends wave1; 3 fires his tele_target and moves the players to the second arena, where he waits
// for trigger_boss_teleport, sunk and immune; 4 wave2; 5 wave3 and the spiral.
void() boss_final_upgrade =
{
    self.wait = self.wait + 1;
    self.mg3_chthon_immune = 0;
    dprint(sprintf("Chthon: phase %g\n", self.wait));

    if(self.wait == 2)
    {
        MG3_Chthon_UseField(self.wave1);
    }
    else if(self.wait == 3)
    {
        MG3_Chthon_UseField(self.tele_target);
        if(MG3_Chthon_HasArena())
        {
            self.think = SUB_Null;
            self.frame = 0;
            self.nextthink = time;
            self.mg3_chthon_immune = 1;
            MG3_Chthon_Teleport(FALSE);
        }
    }
    else if(self.wait == 4)
    {
        MG3_Chthon_UseField(self.wave2);
    }
    else if(self.wait == 5)
    {
        MG3_Chthon_UseField(self.wave3);
        MG3_Chthon_Spiral();
    }
};

void() boss_final_shockc1  = [$shockc1,  boss_final_shockc2 ] {};
void() boss_final_shockc2  = [$shockc2,  boss_final_shockc3 ] { MG3_PainLightning('0 0 100'); };
void() boss_final_shockc3  = [$shockc3,  boss_final_shockc4 ] {};
void() boss_final_shockc4  = [$shockc4,  boss_final_shockc5 ] {};
void() boss_final_shockc5  = [$shockc5,  boss_final_shockc6 ] { MG3_PainLightning('0 0 100'); };
void() boss_final_shockc6  = [$shockc6,  boss_final_shockc7 ] {};
void() boss_final_shockc7  = [$shockc7,  boss_final_shockc8 ] {};
void() boss_final_shockc8  = [$shockc8,  boss_final_shockc9 ] { MG3_PainLightning('0 0 100'); };
void() boss_final_shockc9  = [$shockc9,  boss_final_shockc10] {};
void() boss_final_shockc10 = [$shockc10, boss_final_death1  ] {};

// Killed: three rings of spheres (1, 3 and 5 s on), he sinks into the lava, and once under, his targets used up to five
// times in all (upstream: one for each fit he did not get to, as the pains once fired them), the gibs and the ending.
void() boss_final_death1 = [$death1, boss_final_death2]
{
    sound(self, CHAN_VOICE, "boss1/death.wav", 1, ATTN_NORM);
    MG3_Chthon_CircleLater(1);
    MG3_Chthon_CircleLater(3);
    MG3_Chthon_CircleLater(5);
};
void() boss_final_death2 = [$death2, boss_final_death3] {};
void() boss_final_death3 = [$death3, boss_final_death4] {};
void() boss_final_death4 = [$death4, boss_final_death5] {};
void() boss_final_death5 = [$death5, boss_final_death6] {};
void() boss_final_death6 = [$death6, boss_final_death7] {};
void() boss_final_death7 = [$death7, boss_final_death8] {};
void() boss_final_death8 = [$death8, boss_final_death9] {};
void() boss_final_death9 = [$death9, boss_final_death10]
{
    sound(self, CHAN_BODY, "boss1/out1.wav", 1, ATTN_NORM);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_LAVASPLASH);
    WriteVec3(MSG_BROADCAST, self.origin);
};
// (upstream counts the kill here again; Killed has: once)
void() boss_final_death10 = [$death9, boss_final_death10]
{
    self.frags = self.frags + 1; // (the death already used his targets once: monster_death_use)
    activator = self.enemy;
    while(self.frags < 5)
    {
        SUB_UseTargets();
        self.frags = self.frags + 1;
    }
    MG3_Chthon_Finale();
    remove(self);
};

// ----------------------------------------------------------------------------
// His missiles.

// A sphere or a plasma ball (spheres and plasma: MG3's, read in place) from `org` along `dir` at `speed`, tumbling: 18 to
// what bleeds (the orb's sphere's touch, upstream T_RockTouch2: through orbs and lava men and each other). Batted as any
// monster's missile (its owner a monster).
entity(vector org, vector dir, float speed, string mdl) MG3_Chthon_Rock =
{
    entity m = launch_spike(org, dir);
    m.classname = "rock";
    setmodel(m, mdl);
    setsize(m, '0 0 0', '0 0 0');
    m.velocity = dir * speed;
    m.avelocity = '300 300 300';
    m.avelocity_x = m.avelocity_x * crandom();
    m.avelocity_y = m.avelocity_y * crandom();
    m.avelocity_z = m.avelocity_z * crandom();
    m.touch = T_OrbTouch;
    mg3_chthon_spheres = mg3_chthon_spheres + 1;
    return m;
};

// Upstream's T_BossRadiusDamage: `damage` less half the distance from his middle to each thing's that takes damage
// (not lava men, half to himself), a hand's slam on what stands at his feet.
void(entity inflictor, entity attacker, float damage, entity ignore) MG3_Chthon_RadiusDamage =
{
    entity head = findradius(inflictor.origin, damage + 40);
    while(head)
    {
        if(head != ignore && head.takedamage && head.classname != "monster_lava_man")
        {
            vector org = head.origin + (head.mins + head.maxs) * 0.5;
            float points = damage - max(0, 0.5 * vlen(inflictor.origin - org));
            if(head == attacker)
                points = points * 0.5;
            if(points > 0 && CanDamage(head, inflictor))
                T_Damage(head, inflictor, attacker, points);
        }
        head = head.chain;
    }
};

// A fan of spheres from a hand (`p`: forward, right, up of him) at you along the ground: 4 on skill 0, 8 on 1, 11 on 2,
// 15 on 3 (300 a second, give or take 100; some a little up), every other one sparking on walls.
void(vector p) boss_final_missile_line_b =
{
    float count = 11;
    if(skill == 0)
        count = 4;
    else if(skill == 1)
        count = 8;
    else if(skill == 3)
        count = 15;

    makevectors(self.angles);
    vector org = self.origin + p_x * v_forward + p_y * v_right + p_z * '0 0 1';
    if(!VR_AI_ShotReady(org)) { return; }

    vector vec = VR_AI_Origin(self.enemy) - org;
    vec_z = 0; // (along the ground)
    vec = normalize(vec);

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteVec3(MSG_BROADCAST, org);

    float offset, spread;
    if(p_y < 0) // (his left hand: the fan sweeps left)
    {
        offset = (1 / count) * 4;
        spread = (1 / count) * -2;
    }
    else
    {
        offset = (1 / count) * -4;
        spread = (1 / count) * 2;
    }

    float i = 0;
    while(i < count)
    {
        MG3_Chthon_RadiusDamage(self, self, 10 * random() + 10, self);

        vector nv = normalize(vec + (v_right * offset) + (v_right * spread * i));
        nv_z = 0;
        nv = normalize(nv);
        entity m = MG3_Chthon_Rock(org + nv * 8, nv, 300 + crandom() * 100, "owned/mg3/progs/rogue/sphere.mdl");
        if(random() > 0.5)
            m.velocity_z = (random() + 1) * 8;
        if(i % 2 == 0)
            m.frags = 1; // (upstream also lit these with KEX's candle light, which Quake VR doesn't draw)
        i = i + 1;
    }
};

// A blast of the volley: 0 to 6 spheres from his mouth (`p`) at you, leading you a quarter of the way (400, 450 on skills
// 1 and 2, 500 on 3, give or take 100), spread by `spread`, with a flash when `fx`.
void(vector p, float spread, float fx) boss_final_missile_blast =
{
    float baseSpeed = 400;
    if(skill > 2)
        baseSpeed = 500;
    else if(skill > 0)
        baseSpeed = 450;
    vector spreadVec = '0.15 0.15 0' + '0.2 0.2 0' * fabs(spread);
    float orbs = floor(random() * 6 + 0.5);

    vector target = VR_AI_Origin(self.enemy);
    makevectors(vectoangles(target - self.origin));
    vector org = self.origin + p_x * v_forward + p_y * v_right + p_z * '0 0 1';
    if(!VR_AI_ShotReady(org)) { return; }

    float t = vlen(target - org) / baseSpeed;
    vector vel = VR_AI_Vector(self.enemy.velocity);
    vel_z = 0;
    vector vec = normalize(target + t / 4 * vel - org);

    if(fx)
    {
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_EXPLOSION);
        WriteVec3(MSG_BROADCAST, org);
    }

    while(orbs > 0)
    {
        vector nv = normalize(vec + crandom() * spreadVec_x * v_right + crandom() * spreadVec_y * v_up);
        MG3_Chthon_Rock(org + nv * 8, nv, baseSpeed + crandom() * 100, "owned/mg3/progs/rogue/sphere.mdl");
        orbs = orbs - 1;
    }
};

// A throw from a hand (`p`): killable (spawnflag 2) a fan of spheres; else a lava ball straight at you (300), as id's.
void(vector p) boss_final_missile =
{
    if(self.spawnflags & MG3_CHTHON_KILLABLE)
    {
        boss_final_missile_line_b(p);
        return;
    }

    makevectors(vectoangles(VR_AI_Origin(self.enemy) - self.origin));
    vector org = self.origin + p_x * v_forward + p_y * v_right + p_z * '0 0 1';
    if(!VR_AI_ShotReady(org)) { return; }
    vector vec = normalize(VR_AI_Origin(self.enemy) - org); // (upstream: no lead on any skill)

    entity m = launch_spike(org, vec);
    setmodel(m, "progs/lavaball.mdl");
    m.avelocity = '200 100 300';
    setsize(m, VEC_ORIGIN, VEC_ORIGIN);
    m.velocity = vec * 300;
    m.touch = T_MissileTouch; // (a rocket's blast)
    mg3_chthon_lavaballs = mg3_chthon_lavaballs + 1;
    sound(self, CHAN_WEAPON, "boss1/throw.wav", 1, ATTN_NORM);

    if(self.enemy.health <= 0)
        boss_final_idle1();
};

// The spiral (phase 5): from above his head, four arms of plasma balls (100 a second, 30 s each) turning 5 degrees a
// step, every 0.15 s (0.2 on skills 0 to 2; on 0 and 1 half the steps skipped), while he lives.
void() MG3_Chthon_SpiralStep =
{
    if(!self.owner || self.owner.health <= 0 || self.owner.classname != "monster_boss")
    {
        remove(self);
        return;
    }
    self.nextthink = time + self.delay;
    self.angles = self.angles + '0 5 0';
    self.count = self.count - 1;
    if(skill < 2 && (self.count % 10) < 5)
        return;
    float arm = 3;
    while(arm >= 0)
    {
        makevectors(self.angles + '0 1 0' * 90 * arm);
        entity m = MG3_Chthon_Rock(self.origin + v_forward * 100, v_forward, 100, "owned/mg3/progs/rogue/plasma.mdl");
        m.owner = self.owner;
        m.nextthink = time + 30;
        arm = arm - 1;
    }
};
void() MG3_Chthon_Spiral =
{
    entity s = spawn();
    s.classname = "chthon_spiral";
    setorigin(s, self.origin + '0 0 60');
    vector vec = self.enemy.origin - self.origin;
    vec_z = 0;
    s.angles = vectoangles(normalize(vec));
    s.owner = self;
    s.delay = (skill <= 2) ? 0.2 : 0.15;
    s.count = 50;
    s.think = MG3_Chthon_SpiralStep;
    s.nextthink = time + s.delay;
    mg3_chthon_spirals = mg3_chthon_spirals + 1;
};

// A ring of 72 spheres (250 a second, give or take 100, 20 s each; every fourth glowing) round where he died, `wait` s on.
// The ring's thinker owns them (upstream) and stays while they fly, so they never take a removed entity for theirs.
void() MG3_Chthon_Circle =
{
    vector tang = '0 0 0';
    float steps = 72;
    while(steps > 0)
    {
        makevectors(tang);
        entity m = MG3_Chthon_Rock(self.origin, v_forward, 250 + crandom() * 100, "owned/mg3/progs/rogue/sphere.mdl");
        m.nextthink = time + 20;
        if(steps % 4 == 0)
            m.effects = m.effects | EF_DIMLIGHT;
        tang = tang + '0 5 0';
        steps = steps - 1;
    }
    self.think = SUB_Remove;
    self.nextthink = time + 21;
};
void(float wait) MG3_Chthon_CircleLater =
{
    entity t = spawn();
    t.classname = "chthon_circle";
    t.origin = self.origin + '0 0 60';
    t.think = MG3_Chthon_Circle;
    t.nextthink = time + wait;
};

// ----------------------------------------------------------------------------
// Waking, his fits, his damage.

// His body: id's model and box, solid, standing.
void() MG3_Chthon_Body =
{
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    setmodel(self, "progs/boss.mdl");
    setsize(self, '-128 -128 -24', '128 128 256');
};

// Used by his map's trigger: he rises at whoever woke him, 12000 health, no fit for 3 s.
void() boss_final_awake =
{
    MG3_Chthon_Body();
    self.takedamage = DAMAGE_AIM;
    self.attack_finished = time + 3;
    self.enemy = activator;
    self.th_die = boss_final_death1;
    self.health = 12000;
    self.max_health = self.health;
    self.use = monster_use; // (not woken twice)
    self.th_pain = boss_final_pain;

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_LAVASPLASH);
    WriteVec3(MSG_BROADCAST, self.origin);

    self.yaw_speed = 20;
    boss_final_rise1();
};

// His fits (upstream boss_final_pain): first blood, then 83%, 66%, 50%, 33% and 16% of his health, each fit once
// (several thresholds crossed by one blow: the last's fit), not while he throws a volley or rises, nor during another.
void(entity attacker, float damage) boss_final_pain =
{
    if(self.attack_finished > time || self.pain_finished > time || self.mg3_chthon_immune)
        return;

    float fit = 0; // 1 shock a, 2 shock b
    if(self.frags == 0)
    {
        fit = 1; self.frags = 1;
    }
    if(self.frags == 1 && self.health < self.max_health * 0.83)
    {
        fit = 1; self.frags = 2;
    }
    if(self.frags == 2 && self.health < self.max_health * 0.66)
    {
        fit = 1; self.frags = 3;
    }
    if(self.frags == 3 && self.health < self.max_health * 0.5)
    {
        fit = 2; self.frags = 4;
    }
    if(self.frags == 4 && self.health < self.max_health * 0.33)
    {
        fit = 1; self.frags = 5;
    }
    if(self.frags == 5 && self.health < self.max_health * 0.16)
    {
        fit = 2; self.frags = 6;
    }
    if(!fit)
        return;

    self.mg3_chthon_immune = 1;
    sound(self, CHAN_WEAPON, "boss1/pain.wav", 1, ATTN_NORM);
    self.pain_finished = time + 3;
    if(fit == 2)
        boss_final_shockb1();
    else
        boss_final_shocka1();
};

// T_Damage's (combat.qc) for him: killable (spawnflag 2), none during a fit, none from a lava man, 0.8 of anything but
// the lightning gun's bolt (a shot of a hand holding it) and a laser cannon's bolt (upstream: the attacker's weapon).
float(entity targ, entity inflictor, entity attacker, float damage) MG3_ChthonDamage =
{
    if(!(targ.spawnflags & MG3_CHTHON_KILLABLE))
        return damage;
    if(targ.mg3_chthon_immune)
        return 0;
    if(attacker.classname == "monster_lava_man")
        return 0;
    if(inflictor.classname == "hiplaser")
        return damage;
    if(attacker.classname == "player" && vr_hitkind == QVR_HITKIND_SHOT && vr_hithand >= 0 &&
       VRGetEntWeapon(attacker, vr_hithand) == WID_LIGHTNING)
        return damage;
    return damage * 0.8;
};

// ----------------------------------------------------------------------------
// The second arena.

// VR: player `p`'s view fades in from black (vr_comfort_fade, the engine's vr_comfortfade.cpp: vr_comfort_teleport_fade
// seconds) as the game moves him at once.
void(entity p) VR_ComfortFade =
{
    if(!p.ishuman)
        return;
    stuffcmd(p, "vr_comfort_fade\n");
    mg3_chthon_fades = mg3_chthon_fades + 1;
};

// Where he and the players are moved (upstream boss_teleport): the players to the info_boss_teleport_first points (or,
// `variant`, the info_boss_teleport_second ones, and Chthon to info_boss_teleport_boss, rising there), each player to
// a point of his own, turned its way and popped up a little, a teleport flash before his eyes. VR: what his hands carry
// comes along; his view fades in from black (comfort).
void(float variant) MG3_Chthon_Teleport =
{
    string pointClass = "info_boss_teleport_first";
    entity e, pt;
    if(variant)
    {
        pointClass = "info_boss_teleport_second";
        e = find(world, classname, "monster_boss"); // (the map's Chthon of Dawn of the Machine first, then any)
        while(e && !e.mg3_chthon)
            e = find(e, classname, "monster_boss");
        if(!e)
            e = find(world, classname, "monster_boss");
        pt = find(world, classname, "info_boss_teleport_boss");
        if(pt && e)
        {
            setorigin(e, pt.origin);
            WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
            WriteByte(MSG_BROADCAST, TE_LAVASPLASH);
            WriteVec3(MSG_BROADCAST, e.origin);
            e.think = boss_final_rise1;
            e.nextthink = time + 0.1;
        }
    }

    pt = find(world, classname, pointClass);
    while(pt)
    {
        pt.wait = 0; // (free)
        pt = find(pt, classname, pointClass);
    }

    entity p = find(world, classname, "player");
    while(p)
    {
        pt = find(world, classname, pointClass);
        while(pt && pt.wait != 0)
            pt = find(pt, classname, pointClass);
        if(!pt)
        {
            dprint("Chthon: no free teleport point for a player\n");
            return;
        }
        pt.wait = 1;
        vector from = p.origin;
        setorigin(p, pt.origin);
        p.angles = pt.mangle;
        p.flags = p.flags - (p.flags & FL_ONGROUND);
        p.fixangle = TRUE;
        p.velocity = '0 0 300';
        VR_Carry_Teleported(p, from);
        VR_ComfortFade(p);
        makevectors(pt.mangle);
        spawn_tfog(pt.origin + v_forward * 32);
        p = find(p, classname, "player");
    }
};

/*QUAKED info_boss_teleport_first (1 0 0) (-16 -16 -24) (16 16 32)
Where Dawn of the Machine's Chthon sends the players at his third phase (one point each).
*/
void() info_boss_teleport_first =
{
    self.mangle = self.angles;
    self.angles = '0 0 0';
    self.model = "";
    self.origin = self.origin + '0 0 27';
};

/*QUAKED info_boss_teleport_second (1 0 0) (-16 -16 -24) (16 16 32)
Where trigger_boss_teleport (spawnflag 1) sends the players (one point each).
*/
void() info_boss_teleport_second =
{
    self.mangle = self.angles;
    self.angles = '0 0 0';
    self.model = "";
    self.origin = self.origin + '0 0 27';
};

/*QUAKED info_boss_teleport_boss (1 0 0) (-16 -16 -24) (16 16 32)
Where trigger_boss_teleport (spawnflag 1) sends Dawn of the Machine's Chthon; he rises there.
*/
void() info_boss_teleport_boss =
{
    self.mangle = self.angles;
    self.angles = '0 0 0';
    self.model = "";
};

void() trigger_boss_teleport_use =
{
    MG3_Chthon_Teleport(self.spawnflags & 1);
};

/*QUAKED trigger_boss_teleport (.5 .5 .5) (-8 -8 -8) (8 8 8) BOSS
When used: the players to the info_boss_teleport_first points; with BOSS, to the info_boss_teleport_second points and
Chthon to info_boss_teleport_boss, rising (Dawn of the Machine).
*/
void() trigger_boss_teleport =
{
    self.use = trigger_boss_teleport_use;
};

// ----------------------------------------------------------------------------
// His end.

// A gib from `org` flung along `dir` (upstream ThrowGibVec, 800 a second, give or take 200): Quake VR's (grabbable,
// destroyable, it may stick to a wall or a ceiling, blood behind it).
void(vector org, vector dir) MG3_Chthon_Gib =
{
    entity g = spawn();
    g.origin = org;
    float r = random();
    if(r < 0.3)
        setmodel(g, "progs/gib1.mdl");
    else if(r < 0.6)
        setmodel(g, "progs/gib2.mdl");
    else
        setmodel(g, "progs/gib3.mdl");
    setsize(g, '0 0 0', '0 0 0');
    g.velocity = VR_Gore_Fling(dir * (800 + crandom() * 200));
    g.movetype = MOVETYPE_BOUNCE;
    g.solid = SOLID_NOT;
    g.avelocity_x = random() * 600;
    g.avelocity_y = random() * 600;
    g.avelocity_z = random() * 600;
    g.think = SUB_Remove;
    g.ltime = time;
    g.nextthink = time + 10 + random() * 10;
    g.frame = 0;
    g.flags = 0;
    VR_MakeGibGrabbable(g);
    VR_Gib_MakeDestroyable(g, 1);
    VR_Gore_WatchFling(g);
    particle2(org, g.velocity, QVR_PARTICLE_PRESET_BLOOD, 40);
    mg3_chthon_gibs = mg3_chthon_gibs + 1;
};

// 8 s after he sank (upstream boss_end): the ending (vr_mg3_triggers.qc MG3_BossEnding: the finale text, then the
// credits, or Bloody Nightmare's new game; nothing for a dead single player).
void() MG3_Chthon_End =
{
    mg3_chthon_endings = mg3_chthon_endings + 1;
    MG3_BossEnding();
    remove(self);
};

// Upstream chthon_finale: a burst of gibs up and out over the lava where he sank (upstream get_org's directions: a
// golden-angle spiral over the sphere, its upper half; its 19th missing), then the ending 8 s on: the map's Chthon in
// Dawn of the Machine only (not the Debug spawner's, nor one elsewhere).
void() MG3_Chthon_Finale =
{
    sound(self, CHAN_VOICE, "boss2/pop2.wav", 1, ATTN_NORM);
    float i = 1;
    while(i < 51)
    {
        if(i != 19)
        {
            float z = 1.01 - 0.02 * i;
            float rad = sqrt(1 - z * z);
            float a = (i - 1) * -137.508;
            vector dir = '0 0 0';
            dir_x = rad * cos(a * (3.14159265 / 180));
            dir_y = rad * sin(a * (3.14159265 / 180));
            dir_z = z;
            MG3_Chthon_Gib(self.origin + dir * 64 + '0 0 48', dir);
        }
        i = i + 1;
    }
    if(self.mg3_chthon == 1 && MG3_Campaign())
    {
        entity t = spawn();
        t.classname = "chthon_ending";
        t.think = MG3_Chthon_End;
        t.nextthink = time + 8;
    }
};

// ----------------------------------------------------------------------------

void() MG3_Chthon_Precache =
{
    precache_model("progs/boss.mdl");
    precache_model("progs/lavaball.mdl");
    precache_model("owned/mg3/progs/rogue/sphere.mdl");
    precache_model("owned/mg3/progs/rogue/plasma.mdl");
    precache_model("progs/gib1.mdl");
    precache_model("progs/gib2.mdl");
    precache_model("progs/gib3.mdl");
    precache_sound("weapons/rocket1i.wav");
    precache_sound("boss1/out1.wav");
    precache_sound("boss1/sight1.wav");
    precache_sound("misc/power.wav");
    precache_sound("boss1/throw.wav");
    precache_sound("boss1/pain.wav");
    precache_sound("boss1/death.wav");
    precache_sound("boss2/pop2.wav");
};

/*QUAKED monster_boss_final (1 0 0) (-128 -128 -24) (128 128 256) X KILLABLE
Dawn of the Machine's Chthon (needs the owned Dawn of the Machine data, else nothing spawns): waits under the lava until
used, then rises at whoever used him; 12000 health, six fits as he is hurt. 'wave1', 'wave2', 'wave3': fired at his
phases 2, 4 and 5; 'tele_target' at phase 3, when he and the players move to the info_boss_teleport points (if the map
has them). KILLABLE: fans of spheres, 0.8 damage but from lightning and lasers, none during his fits.
*/
void() monster_boss_final =
{
    if(MG_MonsterInhibited()) return;
    if(deathmatch)
    {
        remove(self);
        return;
    }
    if(!VR_Pack_RequireSpawn(MG3_Chthon_Available(), "Dawn of the Machine")) return;

    MG3_Chthon_Precache();
    self.classname = "monster_boss";
    self.mg3_chthon = 1;
    self.mg3_chthon_immune = 1;
    self.flags = FL_MONSTER;
    total_monsters = total_monsters + 1;
    self.th_run = boss_final_missile1; // (HuntTarget's, upstream)
    self.frags = 0;
    self.use = boss_final_awake;
};

// The Debug spawner's (func_enemy_dispenser 50): killable, woken at its user once placed; never the ending.
void() MG3_Chthon_SpawnerWake =
{
    activator = self.enemy;
    if(!activator)
        activator = find(world, classname, "player");
    boss_final_awake();
};
void(entity user) MG3_Chthon_FromSpawner =
{
    self.classname = "monster_boss_final";
    monster_boss_final();
    if(self.classname != "monster_boss")
        return; // (removed: no data)
    self.mg3_chthon = 2;
    self.spawnflags = MG3_CHTHON_KILLABLE;
    self.enemy = user;
    self.think = MG3_Chthon_SpawnerWake;
    self.nextthink = time + 0.05;
};
