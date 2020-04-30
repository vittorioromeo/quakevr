/*
==============================================================================

ZOMBIE MASTER

==============================================================================
*/

$cd id1/models/knight2
$origin 0 0 24
$base base
$skin skin

$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8 stand9

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9
$frame walk10 walk11 walk12 walk13 walk14 walk15 walk16 walk17
$frame walk18 walk19 walk20

$frame run1 run2 run3 run4 run5 run6 run7 run8

$frame pain1 pain2 pain3 pain4 pain5

$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12

$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9

$frame char_a1 char_a2 char_a3 char_a4 char_a5 char_a6 char_a7 char_a8
$frame char_a9 char_a10 char_a11 char_a12 char_a13 char_a14 char_a15 char_a16

$frame magica1 magica2 magica3 magica4 magica5 magica6 magica7 magica8
$frame magica9 magica10 magica11 magica12 magica13 magica14

$frame magicb1 magicb2 magicb3 magicb4 magicb5 magicb6 magicb7 magicb8
$frame magicb9 magicb10 magicb11 magicb12 magicb13

$frame char_b1 char_b2 char_b3 char_b4 char_b5 char_b6

$frame slice1 slice2 slice3 slice4 slice5 slice6 slice7 slice8 slice9 slice10

$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7 smash8 smash9 smash10
$frame smash11

$frame w_attack1 w_attack2 w_attack3 w_attack4 w_attack5 w_attack6 w_attack7 
$frame w_attack8 w_attack9 w_attack10 w_attack11 w_attack12 w_attack13 w_attack14
$frame w_attack15 w_attack16 w_attack17 w_attack18 w_attack19 w_attack20 
$frame w_attack21 w_attack22 

$frame magicc1 magicc2 magicc3 magicc4 magicc5 magicc6 magicc7 magicc8
$frame magicc9 magicc10 magicc11


void(float dist) zmaster_run;
void() zmaster_run1;
void() zm_idle_sound;
float() zmaster_find_easy_victim;
float(entity testent) corpse_status;

//===========================================================================

void()    zmaster_stand1    =[    $stand1,    zmaster_stand2    ]
{
    zmaster_find_easy_victim();
    if(self.effects != 0)
    {
        sound (self, CHAN_WEAPON, "items/protect2.wav", 1, ATTN_NORM);
        self.effects = 0;
    }
    if(random() < 0.2)
        zm_idle_sound();
    ai_stand();
};
void()    zmaster_stand2    =[    $stand2,    zmaster_stand3    ] {ai_stand();};
void()    zmaster_stand3    =[    $stand3,    zmaster_stand4    ] {ai_stand();};
void()    zmaster_stand4    =[    $stand4,    zmaster_stand5    ] {ai_stand();};
void()    zmaster_stand5    =[    $stand5,    zmaster_stand6    ] {ai_stand();};
void()    zmaster_stand6    =[    $stand6,    zmaster_stand7    ] {ai_stand();};
void()    zmaster_stand7    =[    $stand7,    zmaster_stand8    ] {ai_stand();};
void()    zmaster_stand8    =[    $stand8,    zmaster_stand9    ] {ai_stand();};
void()    zmaster_stand9    =[    $stand9,    zmaster_stand1    ] {ai_stand();};

//===========================================================================

void()    zmaster_walk1    =[    $walk1,        zmaster_walk2    ]
{
    if(self.effects != 0)
    {
        sound (self, CHAN_WEAPON, "items/protect2.wav", 1, ATTN_NORM);
        self.effects = 0;
    }
    if(random() < 0.2)
        zm_idle_sound();
    FootFall("zmaster/echo.wav", 0.2, 1, 0.8);
    ai_walk(2);
};
void()    zmaster_walk2    =[    $walk2,        zmaster_walk3    ] {ai_walk(5);};
void()    zmaster_walk3    =[    $walk3,        zmaster_walk4    ] {ai_walk(5);};
void()    zmaster_walk4    =[    $walk4,        zmaster_walk5    ] {ai_walk(4);};
void()    zmaster_walk5    =[    $walk5,        zmaster_walk6    ] {ai_walk(4);};
void()    zmaster_walk6    =[    $walk6,        zmaster_walk7    ] {ai_walk(2);};
void()    zmaster_walk7    =[    $walk7,        zmaster_walk8    ] {ai_walk(2);};
void()    zmaster_walk8    =[    $walk8,        zmaster_walk9    ] {ai_walk(3);};
void()    zmaster_walk9    =[    $walk9,        zmaster_walk10    ] {ai_walk(3);};
void()    zmaster_walk10    =[    $walk10,    zmaster_walk11    ] {ai_walk(4);};
void()    zmaster_walk11    =[    $walk11,    zmaster_walk12    ]
{FootFall("zmaster/echo.wav", 0.2, 1, 0.8);ai_walk(3);};
void()    zmaster_walk12    =[    $walk12,    zmaster_walk13    ] {ai_walk(4);};
void()    zmaster_walk13    =[    $walk13,    zmaster_walk14    ] {ai_walk(6);};
void()    zmaster_walk14    =[    $walk14,    zmaster_walk15    ] {ai_walk(2);};
void()    zmaster_walk15    =[    $walk15,    zmaster_walk16    ] {ai_walk(2);};
void()    zmaster_walk16    =[    $walk16,    zmaster_walk17    ] {ai_walk(4);};
void()    zmaster_walk17    =[    $walk17,    zmaster_walk18    ] {ai_walk(3);};
void()    zmaster_walk18    =[    $walk18,    zmaster_walk19    ] {ai_walk(3);};
void()    zmaster_walk19    =[    $walk19,    zmaster_walk20    ] {ai_walk(3);};
void()    zmaster_walk20    =[    $walk20,    zmaster_walk1    ] {ai_walk(2);};

//============================================================================

void()    zmaster_pain1    =[    $pain1,        zmaster_pain2    ]
{
    sound (self, CHAN_VOICE, "zmaster/pain.wav", 1, ATTN_NORM);
};
void()    zmaster_pain2    =[    $pain2,        zmaster_pain3    ] {};
void()    zmaster_pain3    =[    $pain3,        zmaster_pain4    ] {};
void()    zmaster_pain4    =[    $pain4,        zmaster_pain5    ] {};
void()    zmaster_pain5    =[    $pain5,        zmaster_run1    ] {};

//============================================================================

void()    zmaster_die1    =[    $death1,    zmaster_die2    ] {ai_forward(10);};
void()    zmaster_die2    =[    $death2,    zmaster_die3    ] {ai_forward(8);};
void()    zmaster_die3    =[    $death3,    zmaster_die4    ]
{self.solid = SOLID_NOT;ai_forward(7);};
void()    zmaster_die4    =[    $death4,    zmaster_die5    ] {};
void()    zmaster_die5    =[    $death5,    zmaster_die6    ] {};
void()    zmaster_die6    =[    $death6,    zmaster_die7    ] {};
void()    zmaster_die7    =[    $death7,    zmaster_die8    ] {};
void()    zmaster_die8    =[    $death8,    zmaster_die9    ] {ai_forward(10);};
void()    zmaster_die9    =[    $death9,    zmaster_die10    ] {ai_forward(11);};
void()    zmaster_die10    =[    $death10,    zmaster_die11    ] {};
void()    zmaster_die11    =[    $death11,    zmaster_die12    ] {};
void()    zmaster_die12    =[    $death12,    zmaster_die12    ] {};

void()    zmaster_dieb1    =[    $deathb1,    zmaster_dieb2    ] {};
void()    zmaster_dieb2    =[    $deathb2,    zmaster_dieb3    ] {self.solid = SOLID_NOT;};
void()    zmaster_dieb3    =[    $deathb3,    zmaster_dieb4    ] {};
void()    zmaster_dieb4    =[    $deathb4,    zmaster_dieb5    ] {};
void()    zmaster_dieb5    =[    $deathb5,    zmaster_dieb6    ] {};
void()    zmaster_dieb6    =[    $deathb6,    zmaster_dieb7    ] {};
void()    zmaster_dieb7    =[    $deathb7,    zmaster_dieb8    ] {};
void()    zmaster_dieb8    =[    $deathb8,    zmaster_dieb9    ] {};
void()    zmaster_dieb9    =[    $deathb9,    zmaster_dieb9    ] {};

void() zmaster_die =
{
// check for gib
    if (self.health < -40)
    {
        sound (self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
        ThrowHead ("progs/drake/h_zmastr.mdl", self.health);
        ThrowGib ("progs/drake/h_axzomb.mdl", self.health);
        ThrowGib ("progs/drake/h_axzomb.mdl", self.health);
        ThrowGib ("progs/drake/h_axzomb.mdl", self.health);
        ThrowGib ("progs/zom_gib.mdl", self.health);
        ThrowGib ("progs/zom_gib.mdl", self.health);
        ThrowGib ("progs/zom_gib.mdl", self.health);
        return;
    }

// play death sound when not hit by arrow
    sound (self, CHAN_VOICE, "zmaster/death.wav", 1, ATTN_NORM);

// regular death

    if (random() > 0.5)
        zmaster_die1 ();
    else
        zmaster_dieb1 ();
};

//============================================================================

void() zmaster_seeker_hunt =
{
    self.nextthink = time + 0.1;
    // the missiles can only seek for a few seconds
    self.gorging = self.gorging + 1;
    if(self.gorging > 30)
    {
        remove(self);
        return;
    }
        

    if(visible(self.enemy))
        target_missile(self, self.enemy.origin, 4);
};

void() zmaster_start_seeker =
{
    self.gorging = ceil(random()*20);
    self.nextthink = time + 0.01 + random() * 0.5;
    self.think = zmaster_seeker_hunt;
};

void(float offseth, float offsetv) zmaster_shot =
{
    local entity firemis;
    local    vector    org, vec, dir;
    
    org = self.origin + v_up * 30 + v_forward * 32;
    dir_x = 300;
    dir_y = offseth;
    dir_z = offsetv;

    firemis = launch_missile("progs/k_spike.mdl", org, dir);
    // his missiles start seeking at different times
    setup_missile(firemis, 0.2 + random()*2, zmaster_start_seeker, M_Spike_Touch, 1, 1);
    target_missile(firemis, self.enemy.origin, 5);
    firemis.enemy = self.enemy;

    sound (self, CHAN_WEAPON, "hknight/attack1.wav", 1, ATTN_NORM);
};

// looks for any nearby corpses or zombies to take over
// and goes after the nearest
float() zmaster_find_easy_victim =
{
    local entity victim, enemyold;
    local float curr_dist, min_dist, found_one, vdist;


    enemyold = self.enemy;
    min_dist = 1000;
    found_one = FALSE;
    victim = nextent(world);
    while(victim)
    {
        if (corpse_status(victim))
        {
            if(visible(victim))
            {
                curr_dist = vlen(self.origin - victim.origin);
                if(curr_dist < min_dist)
                {
                    self.enemy = victim;
                    found_one = TRUE;
                    min_dist = curr_dist;
                }
            }
        }
        else if(victim.classname == "monster_zombie" ||
            victim.classname == "monster_axzombie")
        {
            if(victim.owner == world && visible(victim))
            {
                curr_dist = vlen(self.origin - victim.origin);
                if(curr_dist < min_dist)
                {
                    self.enemy = victim;
                    found_one = TRUE;
                    min_dist = curr_dist;
                }
            }
        }
        victim = nextent(victim);
    }
    if(found_one && enemyold != self.enemy)
    {
        FoundTarget ();
        if(self.effects == 0)
        {
            sound (self, CHAN_WEAPON, "items/protect.wav", 1, ATTN_NORM);
            self.effects = EF_BRIGHTFIELD;
        }
        self.oldenemy = enemyold;
        sound (self, CHAN_VOICE, "zmaster/toreturn.wav", 1, ATTN_NORM);
        self.currentammo = time + 4;
    }
    return found_one;
};

//===========================================================================

void()    zmaster_run1    =[    $walk1,        zmaster_run2    ]
{zm_idle_sound();FootFall("zmaster/echo.wav", 0.5, 1, 0.8);zmaster_run(2);};
void()    zmaster_run2    =[    $walk2,    zmaster_run3    ] {zmaster_run(5);};
void()    zmaster_run3    =[    $walk3,    zmaster_run4    ] {zmaster_run(5);};
void()    zmaster_run4    =[    $walk4,    zmaster_run5    ] {zmaster_run(4);};
void()    zmaster_run5    =[    $walk5,    zmaster_run6    ] {zmaster_run(4);};
void()    zmaster_run6    =[    $walk6,    zmaster_run7    ] {zmaster_run(2);};
void()    zmaster_run7    =[    $walk7,    zmaster_run8    ] {zmaster_run(2);};
void()    zmaster_run8    =[    $walk8,    zmaster_run9    ] {zmaster_run(3);};
void()    zmaster_run9    =[    $walk9,    zmaster_run10    ] {zmaster_run(3);};
void()    zmaster_run10    =[    $walk10,    zmaster_run11    ] {zmaster_run(4);};
void()    zmaster_run11    =[    $walk11,    zmaster_run12    ]
{FootFall("zmaster/echo.wav", 0.5, 1, 0.8);zmaster_run(3);};
void()    zmaster_run12    =[    $walk12,    zmaster_run13    ] {zmaster_run(4);};
void()    zmaster_run13    =[    $walk13,    zmaster_run14    ] {zmaster_run(6);};
void()    zmaster_run14    =[    $walk14,    zmaster_run15    ] {zmaster_run(2);};
void()    zmaster_run15    =[    $walk15,    zmaster_run16    ] {zmaster_run(2);};
void()    zmaster_run16    =[    $walk16,    zmaster_run17    ] {zmaster_run(4);};
void()    zmaster_run17    =[    $walk17,    zmaster_run18    ] {zmaster_run(3);};
void()    zmaster_run18    =[    $walk18,    zmaster_run19    ] {zmaster_run(3);};
void()    zmaster_run19    =[    $walk19,    zmaster_run20    ] {zmaster_run(3);};
void()    zmaster_run20    =[    $walk20,    zmaster_run1    ] {zmaster_run(2);};

//============================================================================

// Turns the current enemy into a controlled zombie
void() zmaster_zombify =
{
    local entity temp;
    local vector shift;

    temp = self;
    self = self.enemy;

    self.enemy = world;

    if(self.classname != "monster_zombie" &&
        self.classname != "monster_axzombie")
    {
        self.ammo_shells = 0; // death counter
        
        self.solid = SOLID_SLIDEBOX;
        self.movetype = MOVETYPE_STEP;

        setmodel (self, "progs/axzombie.mdl");
        setsize (self, '-16 -16 -24', '16 16 40');

        self.takedamage = DAMAGE_AIM;

        self.th_stand = zombie_stand1;
        self.th_walk = zombie_walk1;
        self.th_run = zombie_run1;
        self.th_pain = axzombie_pain;
        self.th_die = zombie_die;
        self.th_missile = zombie_missile;
        self.th_melee = zombie_missile;

        self.classname = "monster_axzombie";
        self.max_health = 400;
        self.health = self.max_health + 1;
        
        // make sure it's on the ground
        shift = self.origin + '0 0 8';
        setorigin(self, shift);
        traceline (self.origin, self.origin + '0 0 -60', TRUE, self);
        while(trace_fraction != 1.0)
        {
            shift = self.origin + '0 0 8';
            setorigin(self, shift);
            traceline (self.origin, self.origin + '0 0 -60', TRUE, self);
        }
        droptofloor();

        zombie_paine14b();
    }
    else
    {
        self.solid = SOLID_SLIDEBOX;
        self.movetype = MOVETYPE_STEP;

        setmodel (self, "progs/axzombie.mdl");
        setsize (self, '-16 -16 -24', '16 16 40');

        self.takedamage = DAMAGE_AIM;

        self.th_stand = zombie_stand1;
        self.th_walk = zombie_walk1;
        self.th_run = zombie_run1;
        self.th_pain = axzombie_pain;
        self.th_die = zombie_die;
        self.th_missile = zombie_missile;
        self.th_melee = zombie_missile;

        if(self.classname == "monster_zombie")
        {
            self.classname = "monster_axzombie";
            self.max_health = 400;
        }
        self.health = self.max_health + 1;

        zombie_painc8b();
    }

    self.owner = temp;
    self.nextthink = time + 2;
    self = temp;
    self.wait = time + 3 + random()*3;
};

//============================================================================

void()    zmaster_magica1 =[    $magica1,    zmaster_magica2    ]
{
    ai_face();
    if(self.effects != EF_BRIGHTFIELD)
        sound (self, CHAN_WEAPON, "items/protect.wav", 1, ATTN_NORM);
};
void()    zmaster_magica2 =[    $magica1,    zmaster_magica3    ]
{
    ai_face();
    self.effects = EF_BRIGHTFIELD;
    self.nextthink = time + 0.25;
};
void()    zmaster_magica3 =[    $magica1,    zmaster_magica4    ] {self.nextthink = time + 0.5;ai_face();};
void()    zmaster_magica4 =[    $magica2,    zmaster_magica5    ] {self.nextthink = time + 0.15;ai_face();};
void()    zmaster_magica5 =[    $magica3,    zmaster_magica6    ] {self.nextthink = time + 0.15;ai_face();};
void()    zmaster_magica6 =[    $magica4,    zmaster_magica7    ] {self.nextthink = time + 0.15;ai_face();};
void()    zmaster_magica7 =[    $magica5,    zmaster_magica8    ]
{
    sound (self, CHAN_VOICE, "zmaster/energize.wav", 1, ATTN_NORM);
    self.nextthink = time + 0.15;
    ai_face();
    self.effects = self.effects | EF_DIMLIGHT;
};
void()    zmaster_magica8 =[    $magica6,    zmaster_magica9    ]
{
    self.nextthink = time + 1;
    ai_face();
};
void()    zmaster_magica9 =[    $magica7,    zmaster_magica10    ] {ai_face();};
void()    zmaster_magica10 =[    $magica8,    zmaster_magica11    ] {ai_face();};
void()    zmaster_magica11 =[    $magica9,    zmaster_magica12    ] {ai_face();};
void()    zmaster_magica12 =[    $magica10,    zmaster_magica13    ] {ai_face();};
void()    zmaster_magica13 =[    $magica11,    zmaster_magica14    ]
{
    ai_face();

    // when gorging != 0, attacking a living creature
    if(self.gorging != 0)
    {
        // don't hit if creature is alive and out of range
        if(range(self.enemy) != RANGE_MELEE)
        {
            self.endtime = 0.1;
            return;
        }
        else
            self.endtime = 0.2;

        if(self.enemy.classname != "player")
            T_KillDamage(self.enemy, self, self);
        else
            T_Damage(self.enemy, self, self, 100);
    }
    else
        zmaster_zombify();

    self.effects = EF_BRIGHTLIGHT + EF_BRIGHTFIELD;

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    if(self.gorging != 0)
        WriteByte (MSG_BROADCAST, TE_EXPLOSION);
    else
        WriteByte (MSG_BROADCAST, TE_TAREXPLOSION);
    WriteCoord (MSG_BROADCAST, self.enemy.origin_x);
    WriteCoord (MSG_BROADCAST, self.enemy.origin_y);
    WriteCoord (MSG_BROADCAST, self.enemy.origin_z);
};
void()    zmaster_magica14 =[    $magica11,    zmaster_magica15    ] {};
void()    zmaster_magica15 =[    $magica11,    zmaster_magica16    ]
{
    self.effects = EF_DIMLIGHT + EF_BRIGHTFIELD;
};
void()    zmaster_magica16 =[    $magica11,    zmaster_magica17    ]
{
    self.nextthink = time + 0.75;
};
void()    zmaster_magica17 =[    $magica12,    zmaster_magica18    ]
{
    self.effects = self.effects - EF_DIMLIGHT;
    self.nextthink = time + self.endtime;
};
void()    zmaster_magica18 =[    $magica13,    zmaster_magica19    ]
{self.nextthink = time + self.endtime;};
void()    zmaster_magica19 =[    $magica14,    zmaster_magica20    ]
{self.nextthink = time + self.endtime;};
void()    zmaster_magica20 =[    $magica1,    zmaster_magica21    ]
{self.nextthink = time + self.endtime;};
void()    zmaster_magica21 =[    $pain5,    zmaster_run1    ]
{
    if(self.endtime == 0.2)
        self.nextthink =time + 1;
    else
        self.nextthink =time + 0.3;
};

//============================================================================

void()    zmaster_magicb1 =[    $magicb1,    zmaster_magicb2    ] {ai_face();};
void()    zmaster_magicb2 =[    $magicb2,    zmaster_magicb3    ] {ai_face();};
void()    zmaster_magicb3 =[    $magicb3,    zmaster_magicb4    ] {ai_face();};
void()    zmaster_magicb4 =[    $magicb4,    zmaster_magicb5    ] {ai_face();};
void()    zmaster_magicb5 =[    $magicb5,    zmaster_magicb6    ] {ai_face();};
void()    zmaster_magicb6 =[    $magicb6,    zmaster_magicb7    ] {ai_face();};
void()    zmaster_magicb7 =[    $magicb7,    zmaster_magicb8    ] {ai_face();};
void()    zmaster_magicb8 =[    $magicb8,    zmaster_magicb9    ] {ai_face();};
void()    zmaster_magicb9 =[    $magicb9,    zmaster_magicb10]
{
    zmaster_shot(0,16);
    zmaster_shot(80,16);
    zmaster_shot(-80,16);
    zmaster_shot(0,80);
    zmaster_shot(0,-64);
    zmaster_shot(50,80);
    zmaster_shot(-50,80);
    zmaster_shot(50,-35);
    zmaster_shot(-50,-35);
};
void()    zmaster_magicb10 =[    $magicb10,    zmaster_magicb11] {ai_face();};
void()    zmaster_magicb11 =[    $magicb11,    zmaster_magicb12] {ai_face();};
void()    zmaster_magicb12 =[    $magicb12,    zmaster_magicb13] {ai_face();};
void()    zmaster_magicb13 =[    $magicb13,    zmaster_run1] {ai_face();};

//============================================================================

void() zm_idle_sound =
{
    if (random() < 0.2 && self.currentammo < time)
    {
        if(random() < 0.5)
        {
            sound (self, CHAN_VOICE, "zmaster/idle1.wav", 1, ATTN_NORM);
            self.currentammo = time + 2.5;
        }
        else
        {
            sound (self, CHAN_VOICE, "zmaster/idle2.wav", 1, ATTN_NORM);
            self.currentammo = time + 1.5;
        }
    }
};

void() zmaster_missile =
{
    if (corpse_status(self.enemy))
        return;

    if(self.gorging > time)
        return;
    zmaster_magicb1();
    self.gorging = time + 2.5;
};

void() zmaster_melee =
{
    if (corpse_status(self.enemy) ||
        self.enemy.classname == "monster_zombie" ||
        self.enemy.classname == "monster_axzombie")
    {
        self.gorging = 0;
        zmaster_magica1();
    }
    else
    {
        if(random() < 0.25)
        {
            self.gorging = time + 5;
            zmaster_magica1();
        }
        else
        {
            if(self.gorging > time)
                return;
            zmaster_magicb1();
            self.gorging = time + 1.5;
        }
    }
};

void(entity attacker, float damage)    zmaster_pain =
{
//    monster_beam_skin();

    if (self.pain_finished > time)
        return;

    sound (self, CHAN_VOICE, "zmaster/pain.wav", 1, ATTN_NORM);

    if (time - self.pain_finished > 5)
    {    // always go into pain frame if it has been a while
        zmaster_pain1 ();
        self.pain_finished = time + 1;
        return;
    }
    
    if (random()*50 > damage)
        return;        // didn't flinch

    self.pain_finished = time + 2;
    zmaster_pain1 ();
};

/*QUAKED monster_zombie_master (1 0 0) (-16 -16 -24) (16 16 40) Ambush
*/
void() monster_zombie_master =
{
    if (deathmatch)
    {
        remove(self);
        return;
    }
    precache_model2 ("progs/zmaster.mdl");
    precache_model2 ("progs/k_spike.mdl");
    precache_model2 ("progs/h_zmastr.mdl");

    
    precache_sound2 ("hknight/attack1.wav");
    precache_sound2 ("zmaster/death.wav");
    precache_sound ("hknight/hit.wav");        // used by C code, so don't sound2
    precache_sound2 ("hknight/slash1.wav");
    precache_sound2 ("zmaster/idle1.wav");
    precache_sound2 ("zmaster/idle2.wav");
    precache_sound2 ("zmaster/sight.wav");
    precache_sound2 ("zmaster/pain.wav");

    precache_sound2 ("zmaster/energize.wav");
    precache_sound2 ("zmaster/toreturn.wav");
    precache_sound2 ("zmaster/comnback.wav");

    precache_sound ("items/protect.wav");
    precache_sound ("items/protect2.wav");
    precache_sound ("items/protect3.wav");

// sound prechache for foot steps
    precache_sound ("zmaster/echo.wav");

// precaches for the controlled zombies
    precache_model ("progs/axzombie.mdl");
    precache_model ("progs/h_axzomb.mdl");
    precache_model ("progs/zom_gib.mdl");

    precache_sound ("zombie/z_idle.wav");
    precache_sound ("zombie/z_idle1.wav");
    precache_sound ("zombie/z_shot1.wav");
    precache_sound ("zombie/z_gib.wav");
    precache_sound ("zombie/z_pain.wav");
    precache_sound ("zombie/z_pain1.wav");
    precache_sound ("zombie/z_fall.wav");
    precache_sound ("zombie/z_miss.wav");
    precache_sound ("zombie/z_hit.wav");
    precache_sound ("zombie/idle_w2.wav");

// a clue on how to kill the modified axzombies
    precache_sound ("zombie/trick.wav");

    if (self.deathstring == "") self.deathstring = " joins a Zombie Master's zombie hoard\n";
    
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;

    setmodel (self, "progs/zmaster.mdl");

    setsize (self, '-16 -16 -24', '16 16 40');
    self.health = 1250;
    self.hittype    = HIT_METAL;
    self.th_stand = zmaster_stand1;
    self.th_walk = zmaster_walk1;
    self.th_run = zmaster_run1;
    self.th_melee = zmaster_melee;
    self.th_missile = zmaster_missile;
    self.th_pain = zmaster_pain;
    self.th_die = zmaster_die;
    
    
// ### Added for Custents all affecting gravity
    if(!self.gravity)
        self.gravity = 1;
    else
          self.gravity = ((self.gravity - 1) / 100);
// ###

    walkmonster_start ();
};

void(float dist) zmaster_run =
{
    local    vector    delta;
    local    float    axis;
    local    float    direct, ang_rint, ang_floor, ang_ceil, r;

    movedist = dist;

// a zombie master's first priority is getting more zombies
    if(time > self.wait)
    {
        zmaster_find_easy_victim();
        self.wait = time + 1.5;
    }
    if(self.effects != 0)
    {
        if(self.enemy.classname != "monster_zombie" &&
            self.enemy.classname != "monster_axzombie" &&
               !corpse_status(self.enemy))
        {
            sound (self, CHAN_WEAPON, "items/protect2.wav", 1, ATTN_NORM);
            self.effects = 0;
        }
    }
    else if((self.enemy.classname == "monster_zombie" ||
        self.enemy.classname == "monster_axzombie") &&
        self.enemy.owner != world)
    {
        if(self.oldenemy)
        {
            self.enemy = self.oldenemy;
            self.oldenemy = world;
            FoundTarget ();
        }
    }

// take control of a corpse when close enough
    if(corpse_status(self.enemy))
    {
        r = vlen(self.enemy.origin - self.origin);
        if (r < 80)
        {
            self.th_melee ();
            return;
        }

        if(!visible(self.enemy))
            if(visible(self.oldenemy))
                self.enemy = self.oldenemy;
    }
// see if the enemy is no longer of interest
    if ((self.enemy.classname == "headib") || self.enemy.owner != world)
    {
        self.enemy = world;
    // FIXME: look all around for other targets
        if (self.oldenemy.owner != self && 
            (self.oldenemy.health > 0 ||
            corpse_status(self.oldenemy)))
        {
            self.enemy = self.oldenemy;
            self.oldenemy = world;
            HuntTarget ();
            sound (self, CHAN_VOICE, "zmaster/comnback.wav", 1, ATTN_NORM);
            self.currentammo = time + 2.5;
        }
        else
        {
            if (self.movetarget)
                self.th_walk ();
            else
                self.th_stand ();
            return;
        }
    }

    self.show_hostile = time + 1;        // wake up other monsters

// check knowledge of enemy
    enemy_vis = visible(self.enemy);
    if (enemy_vis)
        self.search_time = time + 5;

// look for other coop players
    enemy_infront = infront(self.enemy);
    enemy_range = range(self.enemy);
    enemy_yaw = vectoyaw(self.enemy.origin - self.origin);

    if (CheckAnyAttack ())
          return;              // beginning an attack

      // head straight in
      movetogoal (dist);      // done in C code...
};

float(entity testent) corpse_status =
{
    if(testent.classname == "corpse")
        return TRUE;
    else if ((testent.health <= 0) && (testent.flags & (FL_MONSTER|FL_CLIENT)))
    {
        if(testent.classname != "headgib")
            return TRUE;
    }
    else if(testent.classname == "player")
    {
        if(testent.deadflag)
            return TRUE;
    }

    return FALSE;
};
