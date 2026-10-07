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
// Dawn of the Machine's Shub-Niggurath (monster_oldone_new, boss2; MG3_PLAN.md M3-26), adapted from
// quakec_mg3/monsters/mg3_oldone_new.qc. id's Shub model (progs/oldone.mdl) with 12000 health that can be killed:
// she faces you, her 46-frame loop spitting volleys of diamonds (sphere chunks) and, every third volley, a child of her
// phase, two autoguns at her sides (skill 1 and up: more of them each phase) and two shub zombies a loop
// (vr_mg3_shub_zombie.qc). Her first wound starts phase 1; below 3/4, 1/2 and 1/4 of her health phases 2, 3 and 4
// (phase 2 sends a seeker eye; 2 fires her wave1 key's target, 3 and 4 her wave2's, as upstream). At each change she
// thrashes, immune for 1.5 s, and fires a sphere of 100 spheres (skill 3: as many volleys as her phase).
//
// Her children (classname oldnew_child, the seeker oldnew_eye): the spammer (13 plasma lobs, each a splash where it
// lands that 2 s later calls lightning and blows up, 100 radius), the swiper (a lightning beam swept through 180 degrees
// in 1.3 s: 25, 100 to a shub zombie), the blasters (two eyes at her sides spiralling spheres at you, 72 shots, 120
// health, counted), the vortex (one such eye, sides alternating), the seeker (an eye of 300 health chasing you; its
// touch 500). Killed, she clears her children and her zombies, dims the lights, bursts into gibs and ends the game:
// MG3_ShubEnding (vr_mg3_triggers.qc: the final text, then the credits). A dead single player completes nothing.
//
// Available wherever Dawn of the Machine's data is (MG3_PLAN.md, "Decisions" 5): its eye, sphere, plasma, diamond and
// splash models and the orb's pain sound are read in place from the owned pack (owned/mg3/...: vr_gamedir.cpp
// VR_OwnedFile), Shub's model is the registered game's. One from the Debug spawner (Thing 60) is free: killed, she
// bursts but ends nothing (.mg3_shub_free). Rigid models: no ragdoll, no head (her children as the orb). Quake VR: no
// shove or knockback moves her (VR_Push), 10000 kg to the grapple, a grenade's hit her own as upstream's.
// The rerelease's EF_CANDLELIGHT on her projectiles is a KEX light the engine leaves out (pr_edict.c): not set.

$frame old1 old2 old3 old4 old5 old6 old7 old8 old9
$frame old10 old11 old12 old13 old14 old15 old16 old17 old18 old19
$frame old20 old21 old22 old23 old24 old25 old26 old27 old28 old29
$frame old30 old31 old32 old33 old34 old35 old36 old37 old38 old39
$frame old40 old41 old42 old43 old44 old45 old46

$frame shake1 shake2 shake3 shake4 shake5 shake6 shake7 shake8
$frame shake9 shake10 shake11 shake12 shake13 shake14
$frame shake15 shake16 shake17 shake18 shake19 shake20

.float mg3_shub_phase;   // upstream boss_phase: 0 the prelude, 1..4 her phases
.float mg3_shub_immune;  // upstream boss_immune: no damage taken (her thrash; dying)
.float mg3_shub_free;    // the Debug spawner's: no ending
.float mg3_szombie;      // a shub zombie (vr_mg3_shub_zombie.qc)

// The tests' counts (vr_mg3_shub_test.qc): children made, projectiles, blasts.
float mg3_shub_spammers, mg3_shub_swipers, mg3_shub_blasters, mg3_shub_vortices, mg3_shub_eyes;
float mg3_shub_spheres, mg3_shub_chunks, mg3_shub_autogun, mg3_shub_spam, mg3_shub_spamblasts, mg3_shub_blasts;
float mg3_shub_gibs, mg3_shub_waves, mg3_shub_spamhits;

// Her files: the registered game's model, the rest read in place from the owned Dawn of the Machine pack.
float() MG3_Shub_Available =
{
    return fileexists("progs/oldone.mdl") && fileexists("owned/mg3/progs/teleporter_eye.mdl") &&
        fileexists("owned/mg3/progs/rogue/sphere.mdl") && fileexists("owned/mg3/progs/rogue/plasma.mdl") &&
        fileexists("owned/mg3/progs/diamond.mdl") && fileexists("owned/mg3/maps/bmodel/b_splash.bsp");
};

float(entity e) MG3_Shub_Is = { return e.classname == "monster_oldone_new"; };
float(entity e) MG3_Shub_IsChild = { return (e.classname == "oldnew_child") || (e.classname == "oldnew_eye"); };

void() oldnew_attack;
void() spawn_spammer;
void() spawn_swiper;
void() spawn_blaster;
void() spawn_vortex;
void() spawn_eye;
void(float max) spawn_sphere_manager;
void(float max) spawn_sphere_chunk_manager;
void(float b, float a, float c) oldnew_autogun_check;
void() oldnew_cleanup;
void() oldnew_finale_4;

// One of upstream's, M3-27's: a shub zombie raised at one of the map's info_szombie_spawn (vr_mg3_shub_zombie.qc).
void() MG3_SZombie_Raise = {};
void() MG3_SZombie_Precache = {};
float(entity e) MG3_SZombie_Is = { return (e.mg3_szombie && (e.health > 0)); };

// a x b
vector(vector a, vector b) MG3_Cross =
{
    vector c;
    c_x = a_y * b_z - a_z * b_y;
    c_y = a_z * b_x - a_x * b_z;
    c_z = a_x * b_y - a_y * b_x;
    return c;
};

// The cosine between `a` and `b` on the level.
float(vector a, vector b) MG3_FlatDot =
{
    a_z = 0;
    b_z = 0;
    return normalize(a) * normalize(b);
};

// The player (upstream's find: single player's).
entity() MG3_Shub_Player = { return find(world, classname, "player"); };

// A lightning bolt from `self` (beam `id`) between two points.
void(float te, float id, vector from, vector to) MG3_Shub_Beam =
{
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, te);
    WriteEntity(MSG_BROADCAST, self);
    WriteByte(MSG_BROADCAST, id); // (Quake VR's beam id: VR_ParseBeamEntity)
    WriteVec3(MSG_BROADCAST, from);
    WriteVec3(MSG_BROADCAST, to);
};

// The rerelease's big blast (TE_EXPLOSION2 in its colours).
void(vector org) MG3_Shub_Blast =
{
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION2);
    WriteVec3(MSG_BROADCAST, org);
    WriteByte(MSG_BROADCAST, 244);
    WriteByte(MSG_BROADCAST, 3);
};

// One more of the level's monsters (a child counted as it appears).
void() MG3_Shub_CountMonster =
{
    total_monsters = total_monsters + 1;
    WriteByte(MSG_ALL, SVC_UPDATESTAT);
    WriteByte(MSG_ALL, STAT_TOTALMONSTERS);
    WriteLong(MSG_ALL, total_monsters);
};

//============================================================================
// Her loops: idle until she sees you, then facing you, attacking on frames 14, 29 and 44, the autoguns on 21-25 and
// 36-40, a zombie raised on 23 and 45.

void() oldnew_idle1        =[      $old1,  oldnew_idle2       ] {ai_stand();};
void() oldnew_idle2        =[      $old2,  oldnew_idle3       ] {ai_stand();};
void() oldnew_idle3        =[      $old3,  oldnew_idle4       ] {ai_stand();};
void() oldnew_idle4        =[      $old4,  oldnew_idle5       ] {ai_stand();};
void() oldnew_idle5        =[      $old5,  oldnew_idle6       ] {ai_stand();};
void() oldnew_idle6        =[      $old6,  oldnew_idle7       ] {ai_stand();};
void() oldnew_idle7        =[      $old7,  oldnew_idle8       ] {ai_stand();};
void() oldnew_idle8        =[      $old8,  oldnew_idle9       ] {ai_stand();};
void() oldnew_idle9        =[      $old9,  oldnew_idle10      ] {ai_stand();};
void() oldnew_idle10       =[      $old10, oldnew_idle11      ] {ai_stand();};
void() oldnew_idle11       =[      $old11, oldnew_idle12      ] {ai_stand();};
void() oldnew_idle12       =[      $old12, oldnew_idle13      ] {ai_stand();};
void() oldnew_idle13       =[      $old13, oldnew_idle14      ] {ai_stand();};
void() oldnew_idle14       =[      $old14, oldnew_idle15      ] {ai_stand();};
void() oldnew_idle15       =[      $old15, oldnew_idle16      ] {ai_stand();};
void() oldnew_idle16       =[      $old16, oldnew_idle17      ] {ai_stand();};
void() oldnew_idle17       =[      $old17, oldnew_idle18      ] {ai_stand();};
void() oldnew_idle18       =[      $old18, oldnew_idle19      ] {ai_stand();};
void() oldnew_idle19       =[      $old19, oldnew_idle20      ] {ai_stand();};
void() oldnew_idle20       =[      $old20, oldnew_idle21      ] {ai_stand();};
void() oldnew_idle21       =[      $old21, oldnew_idle22      ] {ai_stand();};
void() oldnew_idle22       =[      $old22, oldnew_idle23      ] {ai_stand();};
void() oldnew_idle23       =[      $old23, oldnew_idle24      ] {ai_stand();};
void() oldnew_idle24       =[      $old24, oldnew_idle25      ] {ai_stand();};
void() oldnew_idle25       =[      $old25, oldnew_idle26      ] {ai_stand();};
void() oldnew_idle26       =[      $old26, oldnew_idle27      ] {ai_stand();};
void() oldnew_idle27       =[      $old27, oldnew_idle28      ] {ai_stand();};
void() oldnew_idle28       =[      $old28, oldnew_idle29      ] {ai_stand();};
void() oldnew_idle29       =[      $old29, oldnew_idle30      ] {ai_stand();};
void() oldnew_idle30       =[      $old30, oldnew_idle31      ] {ai_stand();};
void() oldnew_idle31       =[      $old31, oldnew_idle32      ] {ai_stand();};
void() oldnew_idle32       =[      $old32, oldnew_idle33      ] {ai_stand();};
void() oldnew_idle33       =[      $old33, oldnew_idle34      ] {ai_stand();};
void() oldnew_idle34       =[      $old34, oldnew_idle35      ] {ai_stand();};
void() oldnew_idle35       =[      $old35, oldnew_idle36      ] {ai_stand();};
void() oldnew_idle36       =[      $old36, oldnew_idle37      ] {ai_stand();};
void() oldnew_idle37       =[      $old37, oldnew_idle38      ] {ai_stand();};
void() oldnew_idle38       =[      $old38, oldnew_idle39      ] {ai_stand();};
void() oldnew_idle39       =[      $old39, oldnew_idle40      ] {ai_stand();};
void() oldnew_idle40       =[      $old40, oldnew_idle41      ] {ai_stand();};
void() oldnew_idle41       =[      $old41, oldnew_idle42      ] {ai_stand();};
void() oldnew_idle42       =[      $old42, oldnew_idle43      ] {ai_stand();};
void() oldnew_idle43       =[      $old43, oldnew_idle44      ] {ai_stand();};
void() oldnew_idle44       =[      $old44, oldnew_idle45      ] {ai_stand();};
void() oldnew_idle45       =[      $old45, oldnew_idle46      ] {ai_stand();};
void() oldnew_idle46       =[      $old46, oldnew_idle1       ] {ai_stand();};

void() oldnew_walk1        =[      $old1,  oldnew_walk2       ] {ai_face(); self.mg3_shub_immune = 0;};
void() oldnew_walk2        =[      $old2,  oldnew_walk3       ] {ai_face();};
void() oldnew_walk3        =[      $old3,  oldnew_walk4       ] {ai_face();};
void() oldnew_walk4        =[      $old4,  oldnew_walk5       ] {ai_face();};
void() oldnew_walk5        =[      $old5,  oldnew_walk6       ] {ai_face();};
void() oldnew_walk6        =[      $old6,  oldnew_walk7       ] {ai_face();};
void() oldnew_walk7        =[      $old7,  oldnew_walk8       ] {ai_face();};
void() oldnew_walk8        =[      $old8,  oldnew_walk9       ] {ai_face();};
void() oldnew_walk9        =[      $old9,  oldnew_walk10      ] {ai_face();};
void() oldnew_walk10       =[      $old10, oldnew_walk11      ] {ai_face();};
void() oldnew_walk11       =[      $old11, oldnew_walk12      ] {ai_face();};
void() oldnew_walk12       =[      $old12, oldnew_walk13      ] {ai_face();};
void() oldnew_walk13       =[      $old13, oldnew_walk14      ] {ai_face();};
void() oldnew_walk14       =[      $old14, oldnew_walk15      ] {ai_face(); oldnew_attack();};
void() oldnew_walk15       =[      $old15, oldnew_walk16      ] {ai_face();};
void() oldnew_walk16       =[      $old16, oldnew_walk17      ] {ai_face();};
void() oldnew_walk17       =[      $old17, oldnew_walk18      ] {ai_face();};
void() oldnew_walk18       =[      $old18, oldnew_walk19      ] {ai_face();};
void() oldnew_walk19       =[      $old19, oldnew_walk20      ] {ai_face();};
void() oldnew_walk20       =[      $old20, oldnew_walk21      ] {ai_face();};
void() oldnew_walk21       =[      $old21, oldnew_walk22      ] {ai_face(); oldnew_autogun_check(1, 0.05, 1);};
void() oldnew_walk22       =[      $old22, oldnew_walk23      ] {ai_face(); oldnew_autogun_check(1, 0, 2);};
void() oldnew_walk23       =[      $old23, oldnew_walk24      ] {ai_face(); MG3_SZombie_Raise(); oldnew_autogun_check(1, -0.05, 3);};
void() oldnew_walk24       =[      $old24, oldnew_walk25      ] {ai_face(); oldnew_autogun_check(1, -0.1, 4);};
void() oldnew_walk25       =[      $old25, oldnew_walk26      ] {ai_face(); oldnew_autogun_check(1, -0.15, 5);};
void() oldnew_walk26       =[      $old26, oldnew_walk27      ] {ai_face();};
void() oldnew_walk27       =[      $old27, oldnew_walk28      ] {ai_face();};
void() oldnew_walk28       =[      $old28, oldnew_walk29      ] {ai_face();};
void() oldnew_walk29       =[      $old29, oldnew_walk30      ] {ai_face(); oldnew_attack();};
void() oldnew_walk30       =[      $old30, oldnew_walk31      ] {ai_face();};
void() oldnew_walk31       =[      $old31, oldnew_walk32      ] {ai_face();};
void() oldnew_walk32       =[      $old32, oldnew_walk33      ] {ai_face();};
void() oldnew_walk33       =[      $old33, oldnew_walk34      ] {ai_face();};
void() oldnew_walk34       =[      $old34, oldnew_walk35      ] {ai_face();};
void() oldnew_walk35       =[      $old35, oldnew_walk36      ] {ai_face();};
void() oldnew_walk36       =[      $old36, oldnew_walk37      ] {ai_face(); oldnew_autogun_check(-1, -0.05, 1);};
void() oldnew_walk37       =[      $old37, oldnew_walk38      ] {ai_face(); oldnew_autogun_check(-1, 0, 2);};
void() oldnew_walk38       =[      $old38, oldnew_walk39      ] {ai_face(); oldnew_autogun_check(-1, 0.05, 3);};
void() oldnew_walk39       =[      $old39, oldnew_walk40      ] {ai_face(); oldnew_autogun_check(-1, 0.1, 4);};
void() oldnew_walk40       =[      $old40, oldnew_walk41      ] {ai_face(); oldnew_autogun_check(-1, 0.15, 5);};
void() oldnew_walk41       =[      $old41, oldnew_walk42      ] {ai_face();};
void() oldnew_walk42       =[      $old42, oldnew_walk43      ] {ai_face();};
void() oldnew_walk43       =[      $old43, oldnew_walk44      ] {ai_face();};
void() oldnew_walk44       =[      $old44, oldnew_walk45      ] {ai_face(); oldnew_attack();};
void() oldnew_walk45       =[      $old45, oldnew_walk46      ] {ai_face(); MG3_SZombie_Raise();};
void() oldnew_walk46       =[      $old46, oldnew_walk1       ] {ai_face();};

// An attack: a volley of diamonds (sphere chunks: one row, two from phase 4), every third time instead her phase's
// child in turn (phase 1: spammer, swiper; 2: vortex, swiper, spammer; 3: blasters, three swipers; 4: spammer,
// seeker, swiper, blasters). None while she thrashes.
void() oldnew_attack =
{
    if(self.mg3_shub_immune)
        return;
    if(self.cnt % 3 != 0)
    {
        if(self.mg3_shub_phase <= 3)
            spawn_sphere_chunk_manager(1);
        else
            spawn_sphere_chunk_manager(2);
    }
    else if(self.mg3_shub_phase == 0) // the prelude
    {
        spawn_sphere_chunk_manager(1);
    }
    else if(self.mg3_shub_phase == 1)
    {
        if(self.count == 0)
            spawn_spammer();
        else
            spawn_swiper();
        self.count = self.count + 1;
        if(self.count > 1)
            self.count = 0;
    }
    else if(self.mg3_shub_phase == 2)
    {
        if(self.count == 0)
            spawn_vortex();
        else if(self.count == 1)
            spawn_swiper();
        else
            spawn_spammer();
        self.count = self.count + 1;
        if(self.count > 2)
            self.count = 0;
    }
    else if(self.mg3_shub_phase == 3)
    {
        if(self.count == 0)
            spawn_blaster();
        else
            spawn_swiper(); // (upstream: the seeker was once the second)
        self.count = self.count + 1;
        if(self.count > 3)
            self.count = 0;
    }
    else
    {
        if(self.count == 0)
            spawn_spammer();
        else if(self.count == 1)
            spawn_eye();
        else if(self.count == 2)
            spawn_swiper();
        else
            spawn_blaster();
        self.count = self.count + 1;
        if(self.count > 3)
            self.count = 0;
    }
    self.cnt = self.cnt + 1;
};

//============================================================================
// Her thrash at a phase change (immune): lightning bolts, then a sphere of spheres.

void() oldnew_thrash1        =[ $shake1,  oldnew_thrash2       ] {};
void() oldnew_thrash2        =[ $shake2,  oldnew_thrash3       ] {};
void() oldnew_thrash3        =[ $shake3,  oldnew_thrash4       ] {};
void() oldnew_thrash4        =[ $shake4,  oldnew_thrash5       ] {MG3_PainLightning('0 0 100');};
void() oldnew_thrash5        =[ $shake5,  oldnew_thrash6       ] {};
void() oldnew_thrash6        =[ $shake6,  oldnew_thrash7       ] {};
void() oldnew_thrash7        =[ $shake7,  oldnew_thrash8       ] {MG3_PainLightning('0 0 100');};
void() oldnew_thrash8        =[ $shake8,  oldnew_thrash9       ] {};
void() oldnew_thrash9        =[ $shake9,  oldnew_thrash10      ] {};
void() oldnew_thrash10       =[ $shake10, oldnew_thrash11      ] {};
void() oldnew_thrash11       =[ $shake11, oldnew_thrash12      ] {MG3_PainLightning('0 0 100');};
void() oldnew_thrash12       =[ $shake12, oldnew_thrash13      ] {};
void() oldnew_thrash13       =[ $shake13, oldnew_thrash14      ] {};
void() oldnew_thrash14       =[ $shake14, oldnew_thrash15      ]
{
    self.mg3_shub_immune = 1;
    if(skill > 2)
        spawn_sphere_manager(self.mg3_shub_phase);
    else
        spawn_sphere_manager(1);
};
void() oldnew_thrash15       =[ $shake15, oldnew_walk1         ] {self.mg3_shub_immune = 0;};

//============================================================================
// Her death: three thrashes, the lights dimmed out, the burst.

void() oldnew_death1        =[ $shake1,  oldnew_death2       ] {sound(self, CHAN_VOICE, "boss2/death.wav", 1, ATTN_NORM);};
void() oldnew_death2        =[ $shake2,  oldnew_death3       ] {};
void() oldnew_death3        =[ $shake3,  oldnew_death4       ] {};
void() oldnew_death4        =[ $shake4,  oldnew_death5       ] {MG3_PainLightning('0 0 100');};
void() oldnew_death5        =[ $shake5,  oldnew_death6       ] {};
void() oldnew_death6        =[ $shake6,  oldnew_death7       ] {};
void() oldnew_death7        =[ $shake7,  oldnew_death8       ] {MG3_PainLightning('0 0 100');};
void() oldnew_death8        =[ $shake8,  oldnew_death9       ] {};
void() oldnew_death9        =[ $shake9,  oldnew_death10      ] {};
void() oldnew_death10       =[ $shake10, oldnew_death11      ] {MG3_PainLightning('0 0 100');};
void() oldnew_death11       =[ $shake11, oldnew_death12      ] {};
void() oldnew_death12       =[ $shake12, oldnew_death13      ] {};
void() oldnew_death13       =[ $shake13, oldnew_death14      ] {};
void() oldnew_death14       =[ $shake14, oldnew_death15      ] {};
void() oldnew_death15       =[ $shake15, oldnew_death16      ]
{
    self.cnt = self.cnt + 1;
    if(self.cnt != 3)
        self.think = oldnew_death1;
};
void() oldnew_death16       =[ $shake16, oldnew_death17      ] {lightstyle(0, "g");};
void() oldnew_death17       =[ $shake17, oldnew_death18      ] {lightstyle(0, "c");};
void() oldnew_death18       =[ $shake18, oldnew_death19      ] {lightstyle(0, "b");};
void() oldnew_death19       =[ $shake19, oldnew_death20      ] {lightstyle(0, "a");};
void() oldnew_death20       =[ $shake20, oldnew_death20      ] {oldnew_finale_4();};

// th_die: immune, her children and zombies cleared, a last sphere, the thrashes.
void() oldnew_finale_1 =
{
    self.takedamage = DAMAGE_NO;
    self.mg3_shub_immune = 1;
    self.cnt = 0;
    oldnew_death1();
    oldnew_cleanup();
    if(skill > 2)
        spawn_sphere_manager(self.mg3_shub_phase);
    else
        spawn_sphere_manager(1);
};

// A gib from `org` flung along `vec` (upstream ThrowGibVec: 800 +- 200, gone in 10-20 s).
void(vector org, vector vec) MG3_Shub_ThrowGibVec =
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
    g.velocity = vec * (800 + crandom() * 200);
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
    mg3_shub_gibs = mg3_shub_gibs + 1;
};

vector(float index) MG3_Shub_SphereDir;

// The burst: gibs flung up all round; then the ending (a map's Shub; the Debug spawner's ends nothing). A single
// player who died meanwhile completes nothing: she lies still (upstream's idle), as upstream.
void() oldnew_finale_4 =
{
    sound(self, CHAN_VOICE, "boss2/pop2.wav", 1, ATTN_NORM);

    if(!coop)
    {
        entity p = MG3_Shub_Player();
        if(p.health <= 0)
        {
            dprint("MG3 Shub: the player is dead, no completion\n");
            oldnew_idle1();
            return;
        }
    }

    float i = 0;
    while(i < 100)
    {
        vector vec = MG3_Shub_SphereDir(i);
        if(vec_z > 0)
            MG3_Shub_ThrowGibVec(self.origin + vec * 64 + '0 0 48', vec);
        i++;
    }

    float free = self.mg3_shub_free;
    remove(self);
    lightstyle(0, "m");
    if(free)
        return;

    WriteByte(MSG_ALL, SVC_CDTRACK);
    WriteByte(MSG_ALL, 3);
    WriteByte(MSG_ALL, 3);
    MG3_ShubEnding();
};

//============================================================================

// A child of hers told to finish: one that can be hurt is (5000), the rest go (upstream left them, thinking nothing).
void() oldnew_cleanup_think =
{
    if(self.takedamage)
        T_Damage(self, self, self, 5000);
    else
        remove(self);
};

// Her shub zombies, one every 0.2 to 0.7 s, gibbed (500), until none is left (it keeps looking, as upstream).
void() oldnew_cleanup_zombies =
{
    entity f = find(world, classname, "monster_zombie");
    while(f && !MG3_SZombie_Is(f))
        f = find(f, classname, "monster_zombie");
    if(f)
    {
        T_Damage(f, world, world, 500);
        self.nextthink = time + 0.2 + random() * 0.5;
    }
    else
    {
        self.nextthink = time + 1.5;
    }
};

void() oldnew_cleanup =
{
    entity f = find(world, classname, "oldnew_child");
    while(f)
    {
        f.nextthink = time + 0.1;
        f.think = oldnew_cleanup_think;
        f = find(f, classname, "oldnew_child");
    }
    f = find(world, classname, "oldnew_eye");
    while(f)
    {
        f.think = oldnew_cleanup_think;
        f.nextthink = time + 0.1;
        f = find(f, classname, "oldnew_eye");
    }
    f = spawn();
    f.classname = "oldnew_zombie_cleaner";
    f.think = oldnew_cleanup_zombies;
    f.nextthink = time + 2;
};

// `self` fires `target` held in `wave` (upstream: her target swapped for the wave's, SUB_UseTargets).
void(string wave) MG3_Shub_Wave =
{
    if(wave == "")
        return;
    string otarget = self.target;
    self.target = wave;
    SUB_UseTargets();
    self.target = otarget;
    mg3_shub_waves = mg3_shub_waves + 1;
};

// th_pain: her phases. Her first wound: phase 1; under 3/4 of her health: 2 (a seeker; her wave1); under 1/2: 3 (her
// wave2); under 1/4: 4 (her wave2 again, as upstream). Each change: her thrash, immune.
void(entity attacker, float damage) oldnew_pain =
{
    if(self.mg3_shub_immune)
        return;
    float change = FALSE;
    if(self.mg3_shub_phase == 0)
    {
        change = TRUE;
        self.mg3_shub_phase = 1;
    }
    if(self.mg3_shub_phase == 1 && self.health < self.max_health * 0.75)
    {
        change = TRUE;
        self.mg3_shub_phase = 2;
        spawn_eye();
        MG3_Shub_Wave(self.wave1);
    }
    if(self.mg3_shub_phase == 2 && self.health < self.max_health * 0.5)
    {
        change = TRUE;
        self.mg3_shub_phase = 3;
        MG3_Shub_Wave(self.wave2);
    }
    if(self.mg3_shub_phase == 3 && self.health < self.max_health * 0.25)
    {
        change = TRUE;
        self.mg3_shub_phase = 4;
        MG3_Shub_Wave(self.wave2);
    }
    if(change)
    {
        self.mg3_shub_immune = 1;
        sound(self, CHAN_VOICE, "owned/mg3/orb/orb_pain.wav", 1, ATTN_NORM);
        self.pain_finished = time + 2.1;
        oldnew_thrash1();
    }
};

void() monster_oldone_new_precache =
{
    precache_model("progs/oldone.mdl");
    precache_model("owned/mg3/progs/rogue/plasma.mdl");
    precache_model("owned/mg3/maps/bmodel/b_splash.bsp");
    precache_model("owned/mg3/progs/rogue/sphere.mdl");
    precache_model("owned/mg3/progs/teleporter_eye.mdl");
    precache_model("owned/mg3/progs/diamond.mdl");
    precache_sound("boss2/death.wav");
    precache_sound("boss2/idle.wav");
    precache_sound("boss2/sight.wav");
    precache_sound("boss2/pop2.wav");
    precache_sound("misc/power.wav");
    precache_sound("owned/mg3/orb/orb_pain.wav");
    precache_sound("weapons/spike2.wav");
    precache_sound("weapons/grenade.wav");
    precache_sound("weapons/lhit.wav");
    precache_sound("weapons/lstart.wav");
    MG3_SZombie_Precache();
};

/*QUAKED monster_oldone_new (1 0 0) (-128 -128 -24) (128 128 256) X X X X X X X Tiny_box
Dawn of the Machine's Shub-Niggurath (needs the owned Dawn of the Machine data, else nothing spawns): 12000 health, killable. She faces you, firing volleys of diamonds, autoguns and, by her phase (her first wound, then under 3/4, 1/2 and 1/4 of her health), lobbed plasma, a sweeping lightning beam, eyes that spiral spheres at you and a seeker eye; she raises shub zombies at the map's info_szombie_spawn. Killed: the final text, then the credits. 'wave1': fired at phase 2; 'wave2': at phases 3 and 4.
*/
void() monster_oldone_new =
{
    if(MG_MonsterInhibited()) return;
    if(deathmatch)
    {
        remove(self);
        return;
    }
    if(!VR_Pack_RequireSpawn(MG3_Shub_Available(), "Dawn of the Machine")) return;

    monster_oldone_new_precache();

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;

    setmodel(self, "progs/oldone.mdl");

    if(self.spawnflags & 128)
        setsize(self, '1 1 -24', '1 1 0');
    else
        setsize(self, '-128 -128 -24', '128 128 256');

    self.health = 12000;
    self.max_health = self.health;
    self.think = oldnew_idle1;
    self.nextthink = time + 0.1;
    self.takedamage = DAMAGE_AIM;
    self.flags = FL_MONSTER;
    self.view_ofs = '0 0 24';
    self.th_pain = oldnew_pain;
    self.th_die = oldnew_finale_1;
    self.th_walk = SUB_Null;
    self.use = monster_use;
    self.th_run = oldnew_walk1;
    self.yaw_speed = 10;
    self.ideal_yaw = self.angles * '0 1 0';
    monster_add_to_counter();
};

//============================================================================
// The spammer: 13 plasma lobs along a wavering line at you, 0.1 s apart. Each sticks where it lands as a splash; 2 s
// later lightning strikes it and it blows up (100, radius). One that hits something alive does 10 instead.

void() oldnew_spam_think2 =
{
    MG3_Shub_Blast(self.origin);
    T_RadiusDamage(self, self, 100, world);
    mg3_shub_spamblasts = mg3_shub_spamblasts + 1;
    remove(self);
};

void() oldnew_spam_think =
{
    MG3_Shub_Beam(TE_LIGHTNING1, 0, self.origin, self.origin + '0 0 500');
    self.think = oldnew_spam_think2;
    self.nextthink = time + 0.1;
};

void() oldnew_spam_touch =
{
    if(other == self.owner)
        return;
    if(other == world)
    {
        self.velocity = '0 0 0';
        self.angles = '0 1 0' * 360 * random();
        self.movetype = MOVETYPE_NONE;
        self.solid = SOLID_NOT;
        setmodel(self, "owned/mg3/maps/bmodel/b_splash.bsp");
        self.touch = SUB_Null;
        self.think = oldnew_spam_think;
        self.nextthink = time + 2;
    }
    else if(other.health)
    {
        T_Damage(other, self.owner, self, 10);
        mg3_shub_spamhits = mg3_shub_spamhits + 1;
        remove(self);
    }
};

void(vector offset) oldnew_spam_shot =
{
    vector vec = self.angles;
    vec_y = vec_y + offset_y * 6;
    makevectors(vec);

    entity m = spawn();
    m.owner = self.owner;
    m.classname = "spam";
    VR_AI_Tag(m);
    setmodel(m, "owned/mg3/progs/rogue/plasma.mdl");
    setorigin(m, self.origin + '0 0 50' + v_forward * 48);
    setsize(m, '0 0 0', '0 0 0');
    m.touch = oldnew_spam_touch;
    sound(self, CHAN_WEAPON, "weapons/grenade.wav", 1, ATTN_NORM);
    m.solid = SOLID_BBOX;
    m.movetype = MOVETYPE_TOSS;

    float oomph = vlen(self.enemy.origin - self.origin) * 0.8;
    if(oomph < 200)
        oomph = 200;
    m.velocity = v_forward * (oomph + 25 * self.count);
    m.velocity_z = 200;
    self.count = self.count + 1;
    mg3_shub_spam = mg3_shub_spam + 1;
};

// The spammer's line: its yaw steps (x 6 degrees), shot by shot.
float(float n) MG3_Shub_SpamStep =
{
    if(n == 1) return 1;
    if(n == 2) return -3;
    if(n == 3) return 2;
    if(n == 4) return -2;
    if(n == 5) return 3;
    if(n == 6) return -1;
    if(n == 7) return 4;
    if(n == 8) return 2;
    if(n == 9) return -1;
    if(n == 10) return -3;
    if(n == 11) return 2;
    if(n == 12) return -4;
    return 0;
};

void() spammer_think =
{
    if(self.owner)
        self.angles = self.owner.angles;
    if(self.count > 12)
    {
        remove(self);
        return;
    }
    oldnew_spam_shot('0 1 0' * MG3_Shub_SpamStep(self.count));
    self.nextthink = time + 0.1;
};

void() spawn_spammer =
{
    entity s = spawn();
    s.classname = "oldnew_child";
    setorigin(s, self.origin);
    s.think = spammer_think;
    s.owner = self;
    if(self.enemy)
        s.enemy = self.enemy;
    else
        s.enemy = MG3_Shub_Player();
    s.nextthink = time + 0.1;
    mg3_shub_spammers = mg3_shub_spammers + 1;
};

//============================================================================
// The swiper: a lightning beam from her, swept through 180 degrees (one side then the other) in 50 steps of 0.025 s,
// faster as it goes: 25 to what it touches each step, 100 to a shub zombie.

void() oldnew_swipe =
{
    float a = self.count / 50;
    a = a * a;
    if(!self.aflag)
        a = -60 + a * 180;
    else
        a = 60 - a * 180;

    vector ang = self.angles;
    ang_y = ang_y + a;
    makevectors(ang);
    vector org = self.origin + v_forward * 130;
    traceline(org, self.origin + v_forward * 1000, FALSE, self);

    if(trace_ent && (trace_ent.health != 0))
    {
        if(MG3_SZombie_Is(trace_ent))
            T_Damage(trace_ent, self.owner, self.owner, 100);
        else
            T_Damage(trace_ent, self.owner, self.owner, 25);
    }

    if(self.count % 2 == 0)
        MG3_Shub_Beam(TE_LIGHTNING3, 0, org, trace_endpos);

    if(self.t_width < time)
    {
        sound(self, CHAN_WEAPON, "weapons/lhit.wav", 1, ATTN_NORM);
        self.t_width = time + 0.6;
    }

    self.count = self.count + 1;
    if(self.count > 50)
        self.think = SUB_Remove;
    self.nextthink = time + 0.025;
};

void() spawn_swiper =
{
    entity s = spawn();
    s.classname = "oldnew_child";
    s.think = oldnew_swipe;
    s.nextthink = time + 0.025;
    s.count = 0;
    setorigin(s, self.origin);
    s.angles = self.angles;
    s.owner = self;
    sound(self, CHAN_AUTO, "weapons/lstart.wav", 1, ATTN_NORM);
    s.aflag = self.aflag;
    self.aflag = !self.aflag;
    mg3_shub_swipers = mg3_shub_swipers + 1;
};

//============================================================================
// The eyes that spiral spheres at you (the blasters, the vortex): after 2 s a sphere every 0.2 s (15 each), weaving
// across the line to you; after 72 they burst. 120 health; counted among the level's monsters.

void() blast_touch =
{
    if(other == world)
    {
        remove(self);
        return;
    }
    if(other.classname == "sphere")
        return;
    if(other == self.owner)
        return;
    if(MG3_Shub_Is(other))
    {
        remove(self);
        return;
    }
    if(other.solid == SOLID_TRIGGER)
        return; // trigger field, do nothing
    if(other.health)
        T_Damage(other, self, self.owner, 15);
    remove(self);
};

void() blast =
{
    if(MG3_Shub_Is(self.enemy))
        self.enemy = world; // (upstream: "accidentally set itself as its enemy")
    if((!self.enemy) || (self.enemy.classname != "player"))
        self.enemy = MG3_Shub_Player();

    vector vec = normalize(self.enemy.origin - self.origin);
    vector cross = MG3_Cross(vec, '0 0 1');
    vec = normalize(vec + cos(self.count * 15) * 0.5 * self.aflag * cross);
    self.angles = vectoangles(vec);
    entity m = launch_spike(self.origin + vec * 8, vec);
    m.touch = blast_touch;
    m.owner = self;
    setmodel(m, "owned/mg3/progs/rogue/sphere.mdl");
    sound(self, CHAN_WEAPON, "weapons/spike2.wav", 1, ATTN_NORM);
    setsize(m, '0 0 0', '0 0 0');
    m.velocity = vec * 500;
    m.avelocity = '300 300 300' * crandom();
    mg3_shub_blasts = mg3_shub_blasts + 1;

    self.nextthink = time + 0.2;
    self.count = self.count + 1;
    if(self.count > 72)
        T_Damage(self, world, world, 500);
};

// Dead (or spent): a blast where it was.
void() eye_die =
{
    MG3_Shub_Blast(self.origin);
    remove(self);
};

void() tdeath_boss_touch =
{
    if(other == self.owner)
        return;
    // a player or Shub where it appears: the newcomer bursts
    if((other.classname == "player") || MG3_Shub_Is(other))
    {
        if(other.invincible_finished > time)
            self.classname = "teledeath2";
        if(self.owner.classname != "player")
        {
            T_Damage(self.owner, self, self, 50000);
            return;
        }
    }
    if(other.health)
        T_Damage(other, self, self, 50000);
};

// What stands where `death_owner` appears at `org` dies (a player or Shub there: the newcomer instead).
void(vector org, entity death_owner) spawn_boss_tdeath =
{
    entity death = spawn();
    death.classname = "teledeath";
    death.movetype = MOVETYPE_NONE;
    death.solid = SOLID_TRIGGER;
    death.angles = '0 0 0';
    setsize(death, death_owner.mins - '1 1 1', death_owner.maxs + '1 1 1');
    setorigin(death, org);
    death.touch = tdeath_boss_touch;
    death.nextthink = time + 0.2;
    death.think = SUB_Remove;
    death.owner = death_owner;
    force_retouch = 2; // make sure even still objects get hit
};

// An eye at `org` that spirals spheres at you (`side`: which way it weaves), from `self` (Shub, or nothing).
entity(vector org, float side) MG3_Shub_TurretEye =
{
    entity e = spawn();
    e.classname = "oldnew_child";
    setorigin(e, org);
    setmodel(e, "owned/mg3/progs/teleporter_eye.mdl");
    e.solid = SOLID_SLIDEBOX;
    e.th_die = eye_die;
    e.th_run = blast;
    e.movetype = MOVETYPE_STEP;
    e.flags = FL_MONSTER | FL_FLY;
    setsize(e, VEC_HULL2_MIN, VEC_HULL2_MAX);
    spawn_boss_tdeath(org, e);
    e.health = 120;
    e.max_health = 120;
    e.takedamage = DAMAGE_AIM;
    MG3_Shub_CountMonster();
    e.owner = self;
    e.angles = self.angles;
    spawn_tfog(org);
    e.aflag = side;
    e.count = 0;
    e.think = blast;
    e.nextthink = time + 2;
    return e;
};

void() spawn_blaster =
{
    makevectors(self.angles);
    MG3_Shub_TurretEye(self.origin + v_right * 300 + v_up * 80 + v_forward * 200, 1);
    MG3_Shub_TurretEye(self.origin - v_right * 300 + v_up * 80 + v_forward * 200, -1);
    mg3_shub_blasters = mg3_shub_blasters + 1;
};

void() spawn_vortex =
{
    makevectors(self.angles);
    float side = self.ammo_cells ? 1 : -1;
    entity v = MG3_Shub_TurretEye(self.origin + v_right * side * 300 + v_up * 80 + v_forward * 200, side);
    v.enemy = self.enemy;
    self.ammo_cells = 1 - self.ammo_cells;
    mg3_shub_vortices = mg3_shub_vortices + 1;
};

//============================================================================
// The seeker: an eye of 300 health that chases you (up to 300 a second, never below your head height + 32), pushed
// back by what hurts it; its touch does 500. Not counted.

void() eye_chase =
{
    self.speed = vlen(self.velocity);
    if(self.speed < 300)
        self.speed = self.speed + 10;

    vector v = normalize(self.enemy.origin - self.origin);
    self.velocity = v * 0.3 + normalize(self.velocity) * 0.7;
    if(self.origin_z < self.enemy.origin_z + 32)
    {
        self.velocity_z = 0;
        self.velocity = normalize(self.velocity);
    }
    self.angles = vectoangles(self.velocity);
    self.velocity = self.velocity * self.speed;
    self.nextthink = time + 0.1;

    if(self.count % 2 == 0)
    {
        self.effects = self.effects | EF_MUZZLEFLASH;
        sound(self, CHAN_VOICE, "misc/power.wav", 1, ATTN_NORM);
    }
    self.count = self.count + 1;
};

void(entity attacker, float take) eye_pain =
{
    vector v = self.origin - attacker.origin;
    v_z = 0;
    v = normalize(v) * 0.8 + '0 0 0.2';
    self.velocity = self.velocity + v * take * 10;
};

void() eye_touch =
{
    if(other == world)
        return;
    if(other == self.owner)
        return;
    if(other.health)
        T_Damage(other, self, self.owner, 500);
};

entity(vector org) MG3_Shub_SeekerEye =
{
    entity e = spawn();
    e.classname = "oldnew_eye";
    spawn_tfog(org);
    setorigin(e, org);
    setmodel(e, "owned/mg3/progs/teleporter_eye.mdl");
    setsize(e, VEC_HULL2_MIN, VEC_HULL2_MAX);
    e.solid = SOLID_SLIDEBOX;
    e.movetype = MOVETYPE_FLY;
    e.takedamage = DAMAGE_AIM;
    e.think = eye_chase;
    e.th_run = eye_chase;
    e.nextthink = time + 0.1;
    e.th_pain = eye_pain;
    e.owner = self;
    e.speed = 100;
    e.health = 300;
    e.max_health = 300;
    e.th_die = eye_die;
    e.touch = eye_touch;
    e.enemy = MG3_Shub_Player();
    return e;
};

void() spawn_eye =
{
    MG3_Shub_SeekerEye(self.origin + '0 0 300');
    mg3_shub_eyes = mg3_shub_eyes + 1;
};

//============================================================================
// Her spheres and diamonds: they pass her children and zombies, 18 to anything else that can be hurt.

void() sphere_mis_touch =
{
    if(other == self.owner)
        return;
    if(MG3_Shub_IsChild(other) || MG3_SZombie_Is(other))
    {
        remove(self);
        return;
    }
    if(other.solid == SOLID_TRIGGER)
        return; // trigger field, do nothing
    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }
    if(other.takedamage)
        T_Damage(other, self, self.owner, 18);
    remove(self);
};

// The sphere: 100 spheres at 800 from around her, mostly ahead; `count` volleys 0.8 s apart, turning 20 degrees.
void() actual_sphere =
{
    if(!self.enemy)
        self.enemy = MG3_Shub_Player();
    makevectors(self.angles);
    vector vec = v_forward;
    float i = 0;
    while(i < 100)
    {
        vector org = MG3_Shub_SphereDir(i);
        vector vec2 = vec * 0.65 + normalize(org) * 0.35;
        entity m = launch_spike(self.origin + '0 0 32' + org * 32, vec2);
        m.owner = self.owner;
        m.touch = sphere_mis_touch;
        m.velocity = vec2 * 800;
        m.dmg = 18;
        setmodel(m, "owned/mg3/progs/rogue/sphere.mdl");
        setsize(m, '0 0 0', '0 0 0');
        i++;
    }
    mg3_shub_spheres = mg3_shub_spheres + 1;

    self.count = self.count - 1;
    if(self.count > 0)
    {
        self.nextthink = time + 0.8;
        if(self.aflag)
            self.angles = self.angles + '0 20 0';
        else
            self.angles = self.angles - '0 20 0';
    }
    else
        remove(self);
};

void(float max) spawn_sphere_manager =
{
    entity m = spawn();
    m.classname = "oldnew_spheres";
    setorigin(m, self.origin);
    m.owner = self;
    if(!self.enemy)
        self.enemy = MG3_Shub_Player();
    vector vec = self.enemy.origin - self.origin;
    vec_z = 0;
    vec = normalize(vec);
    makevectors(self.angles);
    // (upstream turns clockwise when this cosine is over 1: never)
    m.aflag = (MG3_FlatDot(v_right, MG3_Cross(vec, '0 0 1')) > 1);
    m.angles = vectoangles(vec);
    m.count = max;
    m.think = actual_sphere;
    m.nextthink = time + 0.1;
};

// A volley of diamonds: two rows (16 high) at 400 in a 120-degree fan at you, a gap either side of the line to you.
void() sphere_chunk =
{
    vector attack = self.enemy.origin - self.origin;
    attack_z = 0;
    attack = normalize(attack);
    self.angles = vectoangles(attack);

    makevectors(self.angles + '0 22.5 0');
    vector exclude1 = v_forward;
    makevectors(self.angles - '0 22.5 0');
    vector exclude2 = v_forward;

    sound(self, CHAN_WEAPON, "weapons/spike2.wav", 1, ATTN_NORM);

    float y = -1;
    while(y < 1)
    {
        float x = 0;
        while(x < 45)
        {
            makevectors(self.angles + '0 1 0' * x * 8);
            if((MG3_FlatDot(v_forward, attack) > 0.5) &&
               !((MG3_FlatDot(v_forward, exclude1) > 0.985) || (MG3_FlatDot(v_forward, exclude2) > 0.985)))
            {
                entity m = launch_spike(self.origin + v_forward * 64 + v_up * y * 16 + '0 0 8', v_forward * 200);
                m.owner = self.owner;
                m.touch = sphere_mis_touch;
                setmodel(m, "owned/mg3/progs/diamond.mdl");
                setsize(m, '0 0 0', '0 0 0');
                m.velocity = normalize(m.velocity) * 400;
                m.avelocity = '0 0 -100';
            }
            x++;
        }
        self.angles_y = self.angles_y + 4;
        y++;
    }
    mg3_shub_chunks = mg3_shub_chunks + 1;
    self.count = self.count + 1;
    if(self.count > self.cnt)
        remove(self);
};

void(float max) spawn_sphere_chunk_manager =
{
    entity m = spawn();
    m.classname = "oldnew_chunks";
    setorigin(m, self.origin);
    m.aflag = 1;
    m.owner = self;
    m.wait = 22.5;
    m.delay = 0.8;
    m.angles = self.angles;
    m.count = 0;
    m.cnt = max;
    if(self.enemy)
        m.enemy = self.enemy;
    else
        m.enemy = MG3_Shub_Player();
    m.think = sphere_chunk;
    m.nextthink = time + 0.1;
};

//============================================================================
// Her autoguns, at her sides (80 up, 64 out): a sphere at you (600; aimed off by `a` across: 800). The c-th of a run
// fires only up to her phase + 2 and skill + 1, never on skill 0.

void(vector org, float a) auto_gun =
{
    entity p = MG3_Shub_Player();
    vector vec = normalize(p.origin - org);
    if(a != 0)
        vec = MG3_Cross(vec, '0 0 1') * a + vec * (1 - fabs(a));
    entity m = launch_spike(org, vec);
    m.owner = self;
    m.touch = sphere_mis_touch;
    if(a == 0)
        m.velocity = vec * 600;
    else
        m.velocity = vec * 800;
    setmodel(m, "owned/mg3/progs/rogue/sphere.mdl");
    setsize(m, '0 0 0', '0 0 0');
    mg3_shub_autogun = mg3_shub_autogun + 1;
};

void(float b, float a, float c) oldnew_autogun_check =
{
    if(self.mg3_shub_immune)
        return;
    if(c > self.mg3_shub_phase + 2)
        return;
    if(skill == 0)
        return;
    if(c > skill + 1)
        return;
    makevectors(self.angles);
    sound(self, CHAN_WEAPON, "weapons/spike2.wav", 1, ATTN_NORM);
    auto_gun(self.origin + '0 0 80' + v_right * b * 64, a);
};

// A grenade striking her: its full damage on her (her box is far bigger than her body: a blast's falloff from her
// middle would spare her), its blast round about without her (upstream weapons.qc GrenadeTouch).
void() MG3_Shub_GrenadeHit =
{
    float dmg = VR_WpnDmg(self.owner, cvarh_vr_dmg_grenade, 120);
    T_Damage(other, self, self.owner, dmg);
    T_RadiusDamage(self, self.owner, dmg, other);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteVec3(MSG_BROADCAST, self.origin);
    BecomeExplosion();
};

//============================================================================
// The Debug spawner's (Things 60, 62, 63) and the training dummy's (50, 51): Shub free (no ending), an eye that
// spirals spheres at you, the seeker.

/*QUAKED monster_shub_eye (1 0 0) (-32 -32 -24) (32 32 64)
One of Dawn of the Machine's Shub's eyes (needs the owned Dawn of the Machine data, else nothing spawns): it hangs where it is and after 2 s spirals 72 spheres at you (15 each), then bursts. 120 health.
*/
void() monster_shub_eye =
{
    if(MG_MonsterInhibited()) return;
    if(deathmatch)
    {
        remove(self);
        return;
    }
    if(!VR_Pack_RequireSpawn(MG3_Shub_Available(), "Dawn of the Machine")) return;
    monster_oldone_new_precache();
    // This entity is the eye (no Shub: no owner).
    entity e = self;
    e.classname = "oldnew_child";
    setmodel(e, "owned/mg3/progs/teleporter_eye.mdl");
    e.solid = SOLID_SLIDEBOX;
    e.th_die = eye_die;
    e.th_run = blast;
    e.movetype = MOVETYPE_STEP;
    e.flags = FL_MONSTER | FL_FLY;
    setsize(e, VEC_HULL2_MIN, VEC_HULL2_MAX);
    e.health = 120;
    e.max_health = 120;
    e.takedamage = DAMAGE_AIM;
    e.aflag = 1;
    e.count = 0;
    e.think = blast;
    e.nextthink = time + 2;
    monster_add_to_counter();
};

/*QUAKED monster_shub_seeker (1 0 0) (-32 -32 -24) (32 32 64)
Dawn of the Machine's Shub's seeker eye (needs the owned Dawn of the Machine data, else nothing spawns): it chases you, faster and faster (up to 300), and its touch does 500. 300 health; not counted.
*/
void() monster_shub_seeker =
{
    if(deathmatch)
    {
        remove(self);
        return;
    }
    if(!VR_Pack_RequireSpawn(MG3_Shub_Available(), "Dawn of the Machine")) return;
    monster_oldone_new_precache();
    entity e = self;
    e.classname = "oldnew_eye";
    setmodel(e, "owned/mg3/progs/teleporter_eye.mdl");
    setsize(e, VEC_HULL2_MIN, VEC_HULL2_MAX);
    e.solid = SOLID_SLIDEBOX;
    e.movetype = MOVETYPE_FLY;
    e.takedamage = DAMAGE_AIM;
    e.think = eye_chase;
    e.th_run = eye_chase;
    e.nextthink = time + 0.1;
    e.th_pain = eye_pain;
    e.speed = 100;
    e.health = 300;
    e.max_health = 300;
    e.th_die = eye_die;
    e.touch = eye_touch;
    e.enemy = MG3_Shub_Player();
};

//============================================================================
// 100 directions over the sphere, top to bottom (upstream get_org's table).
vector(float index) MG3_Shub_SphereDir =
{
    switch(index)
    {
        case 0: return '0.056 0.13 -0.99';
        case 1: return '0.141 0.0 0.99';
        case 2: return '-0.179 -0.164 0.97';
        case 3: return '0.027 0.311 0.95';
        case 4: return '0.224 -0.292 0.93';
        case 5: return '-0.408 0.072 0.91';
        case 6: return '0.385 0.245 0.89';
        case 7: return '-0.128 -0.476 0.87';
        case 8: return '-0.243 0.467 0.85';
        case 9: return '0.524 -0.191 0.83';
        case 10: return '-0.542 -0.224 0.81';
        case 11: return '0.26 0.555 0.79';
        case 12: return '0.191 -0.609 0.77';
        case 13: return '-0.572 0.332 0.75';
        case 14: return '0.668 0.147 0.73';
        case 15: return '-0.405 -0.576 0.71';
        case 16: return '-0.093 0.718 0.69';
        case 17: return '0.568 -0.478 0.67';
        case 18: return '-0.759 -0.031 0.65';
        case 19: return '-0.222 -0.1 -0.97'; // (upstream's "oops")
        case 20: return '0.55 0.548 0.63';
        case 21: return '-0.037 -0.791 0.61';
        case 22: return '-0.517 0.62 0.59';
        case 23: return '0.814 -0.11 0.57';
        case 24: return '-0.686 -0.477 0.55';
        case 25: return '0.186 0.827 0.53';
        case 26: return '0.428 -0.746 0.51';
        case 27: return '-0.83 0.265 0.49';
        case 28: return '0.801 0.37 0.47';
        case 29: return '-0.345 -0.824 0.45';
        case 30: return '-0.306 0.85 0.43';
        case 31: return '0.807 -0.424 0.41';
        case 32: return '-0.89 -0.235 0.39';
        case 33: return '0.502 0.781 0.37';
        case 34: return '0.159 -0.923 0.35';
        case 35: return '-0.746 0.578 0.33';
        case 36: return '0.947 0.078 0.31';
        case 37: return '-0.65 -0.703 0.29';
        case 38: return '0.005 0.963 0.27';
        case 39: return '0.651 -0.717 0.25';
        case 40: return '-0.969 0.09 0.23';
        case 41: return '0.779 0.591 0.21';
        case 42: return '-0.176 -0.966 0.19';
        case 43: return '-0.525 0.834 0.17';
        case 44: return '0.954 -0.26 0.15';
        case 45: return '-0.882 -0.453 0.13';
        case 46: return '0.346 0.932 0.11';
        case 47: return '0.376 -0.922 0.09';
        case 48: return '-0.901 0.427 0.07';
        case 49: return '0.954 0.294 0.05';
        case 50: return '-0.505 -0.862 0.03';
        case 51: return '-0.21 0.978 0.01';
        case 52: return '0.815 -0.579 -0.01';
        case 53: return '-0.992 -0.124 -0.03';
        case 54: return '0.647 0.761 -0.05';
        case 55: return '0.036 -0.997 -0.07';
        case 56: return '-0.699 0.709 -0.09';
        case 57: return '0.993 -0.051 -0.11';
        case 58: return '-0.764 -0.632 -0.13';
        case 59: return '0.137 0.979 -0.15';
        case 60: return '0.559 -0.812 -0.17';
        case 61: return '-0.957 0.22 -0.19';
        case 62: return '0.851 0.482 -0.21';
        case 63: return '-0.3 -0.926 -0.23';
        case 64: return '-0.402 0.881 -0.25';
        case 65: return '0.886 -0.376 -0.27';
        case 66: return '-0.902 -0.32 -0.29';
        case 67: return '0.446 0.839 -0.31';
        case 68: return '0.236 -0.914 -0.33';
        case 69: return '-0.785 0.51 -0.35';
        case 70: return '0.916 0.153 -0.37';
        case 71: return '-0.567 -0.725 -0.39';
        case 72: return '-0.071 0.909 -0.41';
        case 73: return '0.66 -0.616 -0.43';
        case 74: return '-0.893 0.009 -0.45';
        case 75: return '0.657 0.599 -0.47';
        case 76: return '-0.085 -0.868 -0.49';
        case 77: return '-0.517 0.688 -0.51';
        case 78: return '0.834 -0.156 -0.53';
        case 79: return '-0.709 -0.441 -0.55';
        case 80: return '0.221 0.791 -0.57';
        case 81: return '0.365 -0.72 -0.59';
        case 82: return '-0.742 0.279 -0.61';
        case 83: return '0.721 0.289 -0.63';
        case 84: return '-0.329 -0.685 -0.65';
        case 85: return '-0.215 0.71 -0.67';
        case 86: return '0.623 -0.369 -0.69';
        case 87: return '-0.689 -0.144 -0.71';
        case 88: return '0.399 0.555 -0.73';
        case 89: return '0.079 -0.657 -0.75';
        case 90: return '-0.484 0.416 -0.77';
        case 91: return '0.613 0.019 -0.79';
        case 92: return '-0.42 -0.41 -0.81';
        case 93: return '0.031 0.557 -0.83';
        case 94: return '0.334 -0.408 -0.85';
        case 95: return '-0.488 0.071 -0.87';
        case 96: return '0.377 0.257 -0.89';
        case 97: return '-0.095 -0.404 -0.91';
        case 98: return '-0.18 0.321 -0.93';
        case 99: return '0.297 -0.098 -0.95';
    }
    return '0 0 0';
};
