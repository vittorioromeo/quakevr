/*
==============================================================================

BLOB

==============================================================================
*/

$cd id1/models/tarbaby
$origin 0 0 24
$base base

//$skin skin
$skin base
$skin skin2
$skin skin

$frame walk1 walk2 walk3 walk4  walk5 walk6 walk7 walk8 walk9 walk10
$frame walk11 walk12 walk13 walk14 walk15 walk16 walk17 walk18 walk19
$frame walk20 walk21 walk22 walk23 walk24 walk25

$frame run1 run2 run3 run4 run5 run6  run7 run8 run9 run10 run11 run12 run13
$frame run14 run15 run16 run17 run18 run19 run20 run21 run22 run23
$frame run24 run25

$frame jump1 jump2 jump3 jump4 jump5 jump6

$frame fly1 fly2 fly3 fly4

$frame exp

void()    tbaby_stand1    =[    $walk1,        tbaby_stand1    ] {ai_stand();};

void()    tbaby_hang1        =[    $walk1,        tbaby_hang1    ] {ai_stand();};

void()    tbaby_walk1    =[    $walk1,        tbaby_walk2    ] {ai_turn();};
void()    tbaby_walk2    =[    $walk2,        tbaby_walk3    ] {ai_turn();};
void()    tbaby_walk3    =[    $walk3,        tbaby_walk4    ] {ai_turn();};
void()    tbaby_walk4    =[    $walk4,        tbaby_walk5    ] {ai_turn();};
void()    tbaby_walk5    =[    $walk5,        tbaby_walk6    ] {ai_turn();};
void()    tbaby_walk6    =[    $walk6,        tbaby_walk7    ] {ai_turn();};
void()    tbaby_walk7    =[    $walk7,        tbaby_walk8    ] {ai_turn();};
void()    tbaby_walk8    =[    $walk8,        tbaby_walk9    ] {ai_turn();};
void()    tbaby_walk9    =[    $walk9,        tbaby_walk10    ] {ai_turn();};
void()    tbaby_walk10    =[    $walk10,        tbaby_walk11    ] {ai_turn();};
void()    tbaby_walk11    =[    $walk11,        tbaby_walk12    ] {ai_walk(2);};
void()    tbaby_walk12    =[    $walk12,        tbaby_walk13    ] {ai_walk(2);};
void()    tbaby_walk13    =[    $walk13,        tbaby_walk14    ] {ai_walk(2);};
void()    tbaby_walk14    =[    $walk14,        tbaby_walk15    ] {ai_walk(2);};
void()    tbaby_walk15    =[    $walk15,        tbaby_walk16    ] {ai_walk(2);};
void()    tbaby_walk16    =[    $walk16,        tbaby_walk17    ] {ai_walk(2);};
void()    tbaby_walk17    =[    $walk17,        tbaby_walk18    ] {ai_walk(2);};
void()    tbaby_walk18    =[    $walk18,        tbaby_walk19    ] {ai_walk(2);};
void()    tbaby_walk19    =[    $walk19,        tbaby_walk20    ] {ai_walk(2);};
void()    tbaby_walk20    =[    $walk20,        tbaby_walk21    ] {ai_walk(2);};
void()    tbaby_walk21    =[    $walk21,        tbaby_walk22    ] {ai_walk(2);};
void()    tbaby_walk22    =[    $walk22,        tbaby_walk23    ] {ai_walk(2);};
void()    tbaby_walk23    =[    $walk23,        tbaby_walk24    ] {ai_walk(2);};
void()    tbaby_walk24    =[    $walk24,        tbaby_walk25    ] {ai_walk(2);};
void()    tbaby_walk25    =[    $walk25,        tbaby_walk1    ] {ai_walk(2);};

void()    tbaby_run1    =[    $run1,        tbaby_run2    ] {ai_face();};
void()    tbaby_run2    =[    $run2,        tbaby_run3    ] {ai_face();};
void()    tbaby_run3    =[    $run3,        tbaby_run4    ] {ai_face();};
void()    tbaby_run4    =[    $run4,        tbaby_run5    ] {ai_face();};
void()    tbaby_run5    =[    $run5,        tbaby_run6    ] {ai_face();};
void()    tbaby_run6    =[    $run6,        tbaby_run7    ] {ai_face();};
void()    tbaby_run7    =[    $run7,        tbaby_run8    ] {ai_face();};
void()    tbaby_run8    =[    $run8,        tbaby_run9    ] {ai_face();};
void()    tbaby_run9    =[    $run9,        tbaby_run10    ] {ai_face();};
void()    tbaby_run10    =[    $run10,        tbaby_run11    ] {ai_face();};
void()    tbaby_run11    =[    $run11,        tbaby_run12    ] {ai_run(2);};
void()    tbaby_run12    =[    $run12,        tbaby_run13    ] {ai_run(2);};
void()    tbaby_run13    =[    $run13,        tbaby_run14    ] {ai_run(2);};
void()    tbaby_run14    =[    $run14,        tbaby_run15    ] {ai_run(2);};
void()    tbaby_run15    =[    $run15,        tbaby_run16    ] {ai_run(2);};
void()    tbaby_run16    =[    $run16,        tbaby_run17    ] {ai_run(2);};
void()    tbaby_run17    =[    $run17,        tbaby_run18    ] {ai_run(2);};
void()    tbaby_run18    =[    $run18,        tbaby_run19    ] {ai_run(2);};
void()    tbaby_run19    =[    $run19,        tbaby_run20    ] {ai_run(2);};
void()    tbaby_run20    =[    $run20,        tbaby_run21    ] {ai_run(2);};
void()    tbaby_run21    =[    $run21,        tbaby_run22    ] {ai_run(2);};
void()    tbaby_run22    =[    $run22,        tbaby_run23    ] {ai_run(2);};
void()    tbaby_run23    =[    $run23,        tbaby_run24    ] {ai_run(2);};
void()    tbaby_run24    =[    $run24,        tbaby_run25    ] {ai_run(2);};
void()    tbaby_run25    =[    $run25,        tbaby_run1    ] {ai_run(2);};


//============================================================================

void()    Tar_JumpTouch =
{
    local    float    ldmg;

    if(other.takedamage && other.classname != self.classname)
    {
        if(vlen(self.velocity) > 400)
        {
            ldmg = 10 + 10*random();
            T_Damage (other, self, self, ldmg);
            sound(self, CHAN_WEAPON, "blob/hit1.wav", 1, ATTN_NORM);
        }
    }
    else
        sound(self, CHAN_WEAPON, "blob/land1.wav", 1, ATTN_NORM);


    if(!checkbottom(self))
    {
        if(self.flags & FL_ONGROUND)
        {    // jump randomly to not get hung up
//dprint("popjump\n");
    self.touch = SUB_Null;
    self.think = tbaby_run1;
    self.movetype = MOVETYPE_STEP;
    self.nextthink = time + 0.1;

//            self.velocity_x = (random() - 0.5) * 600;
//            self.velocity_y = (random() - 0.5) * 600;
//            self.velocity_z = 200;
//            self.flags = self.flags - FL_ONGROUND;
        }
        return;    // not on ground yet
    }

    self.touch = SUB_Null;
    self.think = tbaby_jump1;
    self.nextthink = time + 0.1;
};

void()    tbaby_fly1        =[    $fly1,    tbaby_fly2    ] {};
void()    tbaby_fly2        =[    $fly2,    tbaby_fly3    ] {};
void()    tbaby_fly3        =[    $fly3,    tbaby_fly4    ] {};
void()    tbaby_fly4        =[    $fly4,    tbaby_fly1    ] {
self.cnt = self.cnt + 1;
if(self.cnt == 4)
{
//dprint("spawn hop\n");
tbaby_jump5();
}
};

void()    tbaby_jump1        =[    $jump1,    tbaby_jump2        ] {ai_face();};
void()    tbaby_jump2        =[    $jump2,    tbaby_jump3        ] {ai_face();};
void()    tbaby_jump3        =[    $jump3,    tbaby_jump4        ] {ai_face();};
void()    tbaby_jump4        =[    $jump4,    tbaby_jump5        ] {ai_face();};
void()    tbaby_jump5        =[    $jump5,    tbaby_jump6        ]
{
    // HONEY
    local vector power;
    if(self.spawnflags & SPAWNFLAG_TBABY_LIVID){
        self.th_missile = tbaby_jump1; //Reset
        self.spawnflags = self.spawnflags - SPAWNFLAG_TBABY_LIVID;
    }

    self.movetype = MOVETYPE_BOUNCE;
    self.touch = Tar_JumpTouch;
    makevectors(self.angles);
    self.origin_z = self.origin_z + 1;

    // HONEY
    if(self.spawnflags & SPAWNFLAG_TBABY_HIPHOP){// Makes it start off by jumping really high the first time
        self.velocity = v_forward * 150 + '0 0 700';
        self.spawnflags = self.spawnflags - SPAWNFLAG_TBABY_HIPHOP;
    }else{
        power = '0 0 0' ;
        power_z = self.distance;
        self.velocity = v_forward * 600 + power;
        self.velocity_z = self.velocity_z + random()*150;
        self.distance = 200;
    }

    self.velocity_z = self.velocity_z + random()*150;
    if(self.flags & FL_ONGROUND)
        self.flags = self.flags - FL_ONGROUND;
    self.cnt = 0;
};
void()    tbaby_jump6    =[    $jump6,tbaby_fly1    ] {};



//=============================================================================

void()    tbaby_die1    =[    $exp,        tbaby_die2    ] {
self.takedamage = DAMAGE_NO;
};
void()    tbaby_die2    =[    $exp,        tbaby_run1    ]
{
    T_RadiusDamage(self, self, 120, world);

    sound(self, CHAN_VOICE, "blob/death1.wav", 1, ATTN_NORM);
    self.origin = self.origin - 8*normalize(self.velocity);

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_TAREXPLOSION);
    WriteCoord(MSG_BROADCAST, self.origin_x);
    WriteCoord(MSG_BROADCAST, self.origin_y);
    WriteCoord(MSG_BROADCAST, self.origin_z);

    BecomeExplosion();
};

//=============================================================================


void() tbaby_checknew =
{
    if(!walkmove(0,0))
    {
        remove(self);
        return;
    }

    self.enemy = self.enemy;
    self.th_stand = tbaby_stand1;
    self.th_walk = tbaby_walk1;
    self.th_run = tbaby_run1;
    self.th_missile = tbaby_jump1;
    self.th_melee = tbaby_jump1;
    self.th_die = tbaby_die1;
    self.origin_z = self.origin_z + 1;      // raise off floor a bit
    droptofloor();

    if(!walkmove(0,0))
    {
        self.owner.th_pain = tbaby_mitosis;
        remove(self);
        return;
    }

    self.takedamage = DAMAGE_AIM;
    self.ideal_yaw = self.angles * '0 1 0';
    if(!self.yaw_speed)
        self.yaw_speed = 20;
    self.view_ofs = '0 0 25';
    self.use = monster_use;
    self.flags = self.flags | FL_MONSTER;
    self.pausetime = 99999999;
    self.th_stand ();
    self.nextthink = self.nextthink + random()*0.5;

    total_monsters = total_monsters + 1;

    WriteByte (MSG_BROADCAST, SVC_UPDATESTAT);
    WriteByte (MSG_BROADCAST, STAT_TOTALMONSTERS);
    WriteLong (MSG_BROADCAST, total_monsters);
};

void(entity attacker, float damage) tbaby_mitosis =
{
    local entity    newSpawn;

    if(random() < 0.35)
    {
        newSpawn = spawn();
        newSpawn.classname = self.classname;
        newSpawn.movetype = MOVETYPE_STEP;
        setsize (newSpawn, '-16 -16 -24', '16 16 40');
        newSpawn.health = 80;
        if(self.skin == 1)
            newSpawn.health = 60;
        else if(self.skin == 2)
            newSpawn.health = 100;

        newSpawn.owner = self;
        self.th_pain = SUB_NullPain;

        setmodel (newSpawn, "progs/tarbaby.mdl");
        newSpawn.solid = SOLID_SLIDEBOX;
        newSpawn.skin = self.skin;

        setorigin (newSpawn, self.origin);
        newSpawn.think = tbaby_checknew;
        newSpawn.nextthink = time + 0.5;

        sound(newSpawn, CHAN_VOICE, "blob/mytosis.wav", 1, ATTN_NORM);
    }
};

/*QUAKED monster_tarbaby (1 0 0) (-16 -16 -24) (16 16 24) Ambush Blue Green Spots
The Spawn (and hellspawn)

The spawn type is random by default.
If you want a specific type, select "Blue", "Green", or "Spots".
*/
void() monster_tarbaby =
{
    local float     myRand;
    local float     skillLevel;

    if(deathmatch)
    {
        remove(self);
        return;
    }
    precache_model2("progs/tarbaby.mdl");

    precache_sound2("blob/death1.wav");
    precache_sound2("blob/hit1.wav");
    precache_sound2("blob/land1.wav");
    precache_sound2("blob/sight1.wav");
    precache_sound2("blob/mytosis.wav");


    if (self.spawnflags & SPAWNFLAG_TRIGGER_SPAWN_MULTIPLE && !self.cnt){
        self.cnt = -1;
    }

    self.health = 80;

    self.th_stand = tbaby_stand1;
    self.th_walk = tbaby_walk1;
    self.th_run = tbaby_run1;

    // HONEY
    if(self.spawnflags & SPAWNFLAG_TBABY_LIVID) { self.th_missile = tbaby_jump5; } //Really go flying
    else                              { self.th_missile = tbaby_jump1; }

    self.th_melee = tbaby_jump1;
    self.th_die = tbaby_die1;

    // TODO VR: (P0) might need to control with cvar due to missing resources
    // ROGUE
    if(self.spawnflags & 8)
    {
        self.th_pain = tbaby_mitosis;
        self.skin = 2;
        self.health = 100;
    }
    else if(self.spawnflags & 4)
    {
        self.th_pain = tbaby_mitosis;
        self.skin = 1;
        self.health = 60;
    }
    else if(self.spawnflags & 2)
    {
        self.skin = 0;
    }
    else
    {
        skillLevel = cvar ("skill");
        myRand = skillLevel * 0.2;
        myRand = myRand + random();

        if(myRand > 0.8 && skillLevel > 1)
        {
            self.th_pain = tbaby_mitosis;
            self.skin = 2;
            self.health = 100;
        }
        else if(myRand > 0.5)
        {
            self.th_pain = tbaby_mitosis;
            self.skin = 1;
            self.health = 60;
        }
        else
        {
            self.skin = 0;
        }
    }

    // HONEY
    monster_add_to_counter();

    if (self.spawnflags & SPAWNFLAG_TRIGGER_SPAWN)
    {
        self.use = tarbaby_start;
        return;
    }

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;

    setmodel(self, "progs/tarbaby.mdl");
    setsize(self, '-12 -12 -24', '12 12 24');


    walkmonster_start();
};

