/*
==============================================================================

SCOURGE

==============================================================================
*/

$cd hipwork\models\scourge
$origin 0 0 23
$base scorbase.asc

$skin skin

$frame stand1.asc stand2.asc stand3.asc stand4.asc stand5.asc stand6.asc
$frame stand7.asc stand8.asc stand9.asc stand10.asc stand11.asc stand12.asc

$frame walk1.asc walk2.asc walk3.asc walk4.asc walk5.asc walk6.asc

$frame shoot1.asc shoot2.asc

$frame sting1.asc sting2.asc sting3.asc sting4.asc sting5.asc sting6.asc
$frame sting7.asc sting8.asc sting9.asc sting10.asc sting11.asc

$frame pain1.asc pain2.asc pain3.asc pain4.asc pain5.asc

$frame death1.asc death2.asc death3.asc death4.asc death5.asc

/*
===========
ScourgeCheckAttack

The player is in view, so decide to move or launch an attack
Returns FALSE if movement should continue
============
*/
float() ScourgeCheckAttack =
{
    entity targ = self.enemy;

    // see if any entities are in the way of the shot
    vector spot1 = self.origin + self.view_ofs;
    vector spot2 = targ.origin + targ.view_ofs;

    if(vlen(spot1 - spot2) <= 100)
    {
      if(CanDamage(self.enemy, self))
      {
         self.attack_state = AS_MELEE;
         return TRUE;
      }
    }

    if(time < self.attack_finished)
        return FALSE;

    if(!enemy_vis)
        return FALSE;


    float chance = spot2_z - spot1_z;

    if(chance > 64)
        return FALSE;

    if(chance < -200)
        return FALSE;

    if(vlen(spot1 - spot2) > 1000)
        return FALSE;

    if(vlen(spot1 - spot2) < 150)
        return FALSE;

    traceline(spot1, spot2, FALSE, self);

    if(trace_inopen && trace_inwater)
        return FALSE;            // sight line crossed contents

    if(trace_ent != targ)
    {
        return FALSE;    // don't have a clear shot
    }

    self.attack_state = AS_MISSILE;
    SUB_AttackFinished (2 + 2*random());
    return TRUE;
}

void(float ox) SCOURGE_FireSpikes =
{
    ai_face();
    makevectors(self.angles);
    vector src = self.origin - '0 0 19' + v_right*ox + v_forward*14;
    vector vec = (self.enemy.origin + (200*v_forward)) - src;
    vec = normalize(vec);
    sound(self, CHAN_WEAPON, "weapons/rocket1i.wav", 1, ATTN_NORM);
    launch_spike(src, vec);

    self.attack_finished = time + 0.2;
}

// Attack_With_Tail
//
void() Attack_With_Tail =
{
    ai_face();

    vector delta = self.enemy.origin - self.origin;

    if(vlen(delta) > 100)
        return;

    if(!CanDamage(self.enemy, self))
        return;

    float ldmg = (random() + random() + random()) * 40;
    T_Damage(self.enemy, self, self, ldmg);
    sound(self, CHAN_WEAPON, "shambler/smack.wav", 1, ATTN_NORM);

    SpawnMeatSpray(self.origin + v_forward*16, crandom() * 50 * v_right);
};

void() scourge_think =
{
    if(!self.state)
    {
        entity trig = spawn();
        trig.angles = '0 0 0';
        trig.solid = SOLID_TRIGGER;
        setsize(trig, '-64 -64 -24', '64 64 64');
        trig.takedamage = DAMAGE_NO;
        trig.movetype = MOVETYPE_NONE;
        trig.modelindex = 0;
        trig.model = "";
        trig.lastvictim = self;
        trig.think = ScourgeTriggerThink;
        trig.touch = ScourgeTriggerTouch;
        trig.nextthink = time + 0.1 + random();
        setorigin(trig, self.origin);
        self.lastvictim = trig;
        self.state = 1;
    }

    if((self.spawnsilent == 0) && (self.spawnmulti == 1))
    {
        sound(self, CHAN_BODY, "misc/null.wav", 1, ATTN_IDLE);
    }
    else if((self.spawnsilent == 1) && (self.spawnmulti == 0))
    {
        sound(self, CHAN_BODY, "scourge/walk.wav", 1, ATTN_IDLE);
    }

    self.spawnmulti = self.spawnsilent;
};

void(float dist) ai_right =
{
    walkmove((self.angles_y+90), dist);
};

void(float dist) ai_left =
{
    walkmove((self.angles_y+270), dist);
};

//============================================================================

void()   scourge_stand1  =[ $stand1.asc, scourge_stand2  ] {self.spawnsilent = 0;ai_stand();scourge_think();};
void()   scourge_stand2  =[ $stand2.asc, scourge_stand3  ] {self.spawnsilent = 0;ai_stand();scourge_think();};
void()   scourge_stand3  =[ $stand3.asc, scourge_stand4  ] {self.spawnsilent = 0;ai_stand();scourge_think();};
void()   scourge_stand4  =[ $stand4.asc, scourge_stand5  ] {self.spawnsilent = 0;ai_stand();scourge_think();};
void()   scourge_stand5  =[ $stand5.asc, scourge_stand6  ] {self.spawnsilent = 0;ai_stand();scourge_think();};
void()   scourge_stand6  =[ $stand6.asc, scourge_stand7  ] {self.spawnsilent = 0;ai_stand();scourge_think();};
void()   scourge_stand7  =[ $stand7.asc, scourge_stand8  ] {self.spawnsilent = 0;ai_stand();scourge_think();};
void()   scourge_stand8  =[ $stand8.asc, scourge_stand9  ] {self.spawnsilent = 0;ai_stand();scourge_think();};
void()   scourge_stand9  =[ $stand9.asc, scourge_stand10  ] {self.spawnsilent = 0;ai_stand();scourge_think();};
void()   scourge_stand10  =[ $stand10.asc, scourge_stand11  ] {self.spawnsilent = 0;ai_stand();scourge_think();};
void()   scourge_stand11  =[ $stand11.asc, scourge_stand12  ] {self.spawnsilent = 0;ai_stand();scourge_think();};
void()   scourge_stand12  =[ $stand12.asc, scourge_stand1  ] {self.spawnsilent = 0;ai_stand();scourge_think();};

void()   scourge_walk1   =[ $walk1.asc , scourge_walk2   ] {
if(random() < 0.1)
   sound(self, CHAN_VOICE, "scourge/idle.wav", 1, ATTN_IDLE);
self.spawnsilent = 1;
scourge_think();
ai_walk(8);};
void()   scourge_walk2   =[ $walk2.asc , scourge_walk3   ] {ai_walk(8);};
void()   scourge_walk3   =[ $walk3.asc , scourge_walk4   ] {ai_walk(8);};
void()   scourge_walk4   =[ $walk4.asc , scourge_walk5   ] {ai_walk(8);};
void()   scourge_walk5   =[ $walk5.asc , scourge_walk6   ] {ai_walk(8);};
void()   scourge_walk6   =[ $walk6.asc , scourge_walk1   ] {ai_walk(8);};

void()   scourge_run1 =[ $walk1.asc  , scourge_run2 ] {
if(random() < 0.1)
   sound(self, CHAN_VOICE, "scourge/idle.wav", 1, ATTN_IDLE);
self.spawnsilent = 1;
scourge_think();
ai_run(18);};
void()   scourge_run2 =[ $walk2.asc  , scourge_run3 ] {scourge_think();ai_run(14);};
void()   scourge_run3 =[ $walk3.asc  , scourge_run4 ] {ai_run(14);};
void()   scourge_run4 =[ $walk4.asc  , scourge_run5 ] {ai_run(14);};
void()   scourge_run5 =[ $walk5.asc  , scourge_run6 ] {ai_run(14);};
void()   scourge_run6 =[ $walk6.asc  , scourge_run1 ] {ai_run(14);};

void()   scourge_strafeleft1   =[ $walk1.asc , scourge_strafeleft2   ] {self.spawnsilent = 1;scourge_think();ai_left(20);};
void()   scourge_strafeleft2   =[ $walk2.asc , scourge_strafeleft3   ] {ai_left(20);};
void()   scourge_strafeleft3   =[ $walk3.asc , scourge_strafeleft4   ] {ai_left(20);};
void()   scourge_strafeleft4   =[ $walk4.asc , scourge_strafeleft5   ] {ai_left(14);};
void()   scourge_strafeleft5   =[ $walk5.asc , scourge_strafeleft6   ] {ai_left(14);};
void()   scourge_strafeleft6   =[ $walk6.asc , scourge_run1   ] {ai_left(14);};

void()   scourge_straferight1   =[ $walk1.asc , scourge_straferight2   ] {self.spawnsilent = 1;scourge_think();ai_right(20);};
void()   scourge_straferight2   =[ $walk2.asc , scourge_straferight3   ] {ai_right(20);};
void()   scourge_straferight3   =[ $walk3.asc , scourge_straferight4   ] {ai_right(20);};
void()   scourge_straferight4   =[ $walk4.asc , scourge_straferight5   ] {ai_right(14);};
void()   scourge_straferight5   =[ $walk5.asc , scourge_straferight6   ] {ai_right(14);};
void()   scourge_straferight6   =[ $walk6.asc , scourge_run1   ] {ai_right(14);};

void()   scourge_turn1  =[ $walk1.asc, scourge_turn2  ] {self.spawnsilent = 1;scourge_think();ai_turn_in_place();};
void()   scourge_turn2  =[ $walk2.asc, scourge_turn3  ] {ai_turn_in_place();};
void()   scourge_turn3  =[ $walk3.asc, scourge_turn4  ] {ai_turn_in_place();};
void()   scourge_turn4  =[ $walk4.asc, scourge_turn5  ] {ai_turn_in_place();};
void()   scourge_turn5  =[ $walk5.asc, scourge_turn6  ] {ai_turn_in_place();};
void()   scourge_turn6  =[ $walk6.asc, scourge_turn1  ] {ai_turn_in_place();};

void()   scourge_atk1 =[ $shoot1.asc,   scourge_atk2 ]
   {
   self.spawnsilent = 0;
   scourge_think();
   self.effects = self.effects | EF_MUZZLEFLASH;
   SCOURGE_FireSpikes(40);
   };
void()   scourge_atk2 =[ $shoot2.asc,   scourge_atk3 ]
   {
   self.effects = self.effects | EF_MUZZLEFLASH;
   SCOURGE_FireSpikes(-56);
   };
void()   scourge_atk3 =[ $shoot1.asc,   scourge_atk4 ]
   {
   self.effects = self.effects | EF_MUZZLEFLASH;
   SCOURGE_FireSpikes(-40);
   };
void()   scourge_atk4 =[ $shoot2.asc,   scourge_atk5 ]
   {
   self.effects = self.effects | EF_MUZZLEFLASH;
   SCOURGE_FireSpikes(56);
   };
void()   scourge_atk5 =[ $shoot1.asc,   scourge_atk6 ]
   {
   self.effects = self.effects | EF_MUZZLEFLASH;
   SCOURGE_FireSpikes(40);
   };
void()   scourge_atk6 =[ $shoot2.asc,   scourge_atk7 ]
   {
   self.effects = self.effects | EF_MUZZLEFLASH;
   SCOURGE_FireSpikes(-56);
   };
void()   scourge_atk7 =[ $shoot1.asc,   scourge_atk8 ]
   {
   self.effects = self.effects | EF_MUZZLEFLASH;
   SCOURGE_FireSpikes(-40);
   };
void()   scourge_atk8 =[ $shoot2.asc,   scourge_run1 ]
   {
   self.effects = self.effects | EF_MUZZLEFLASH;
   SCOURGE_FireSpikes(56);
   SUB_AttackFinished (4*random());
   };

void()   scourge_melee1 =[ $sting1.asc,   scourge_melee2 ] {self.spawnsilent = 0;scourge_think();ai_charge(3);};
void()   scourge_melee2 =[ $sting2.asc,   scourge_melee3 ] {ai_charge(3);};
void()   scourge_melee3 =[ $sting3.asc,   scourge_melee4 ] {ai_charge(2);};
void()   scourge_melee4 =[ $sting4.asc,   scourge_melee5 ] {ai_charge(2);};
void()   scourge_melee5 =[ $sting5.asc,   scourge_melee6 ]
   {
   sound(self, CHAN_WEAPON, "scourge/tailswng.wav", 1, ATTN_NORM);
   ai_charge(3);
   };
void()   scourge_melee6 =[ $sting6.asc,   scourge_melee7 ] {ai_charge(1);};
void()   scourge_melee7   =[ $sting7.asc,   scourge_melee8   ]
   {
   Attack_With_Tail();
   };
void()   scourge_melee8   =[ $sting8.asc,   scourge_melee9   ] {ai_face();};
void()   scourge_melee9   =[ $sting9.asc,   scourge_melee10   ] {ai_face();};
void()   scourge_melee10   =[ $sting10.asc,   scourge_melee11   ] {ai_face();};
void()   scourge_melee11   =[ $sting11.asc,  scourge_run1 ] {ai_face();
SUB_CheckRefire (scourge_melee1);
};

void()   scourge_pain1  =[ $pain1.asc, scourge_pain2  ] {self.spawnsilent = 0;scourge_think();};
void()   scourge_pain2  =[ $pain2.asc, scourge_pain3  ] {};
void()   scourge_pain3  =[ $pain3.asc, scourge_pain4  ] {};
void()   scourge_pain4  =[ $pain4.asc, scourge_pain5  ] {};
void()   scourge_pain5  =[ $pain5.asc, scourge_run1 ] {};

void(entity attacker, float damage) scourge_pain =
{
    if(random()*50 > damage)
        return; // didn't flinch

    if(self.pain_finished > time)
        return;

    float r = random();
    sound(self, CHAN_VOICE, "scourge/pain.wav", 1, ATTN_NORM);

    self.pain_finished = time + 2.0;
    scourge_pain1();
};

//============================================================================

void() ScourgeDrop =
{
    entity entPlayer = FindEntPlayerForDrop();

    self.solid = SOLID_NOT;

    if(entPlayer != world && ShouldDropEnemyWpn(0.65))
    {
        if(!TryEnemyDrop(self, entPlayer, WID_SUPER_NAILGUN, 0.4))
        if(!TryEnemyDrop(self, entPlayer, WID_NAILGUN, 0.5))
        { /* Intentionally empty. */ }
    }
}

void()   scourge_die1 =[ $death1.asc, scourge_die2 ] {self.spawnsilent = 0;scourge_think();};
void()   scourge_die2 =[ $death2.asc, scourge_die3 ] {};
void()   scourge_die3 =[ $death3.asc, scourge_die4 ] {ScourgeDrop();};
void()   scourge_die4 =[ $death4.asc, scourge_die5 ] {};
void()   scourge_die5 =[ $death5.asc, scourge_die5 ] {};

void() scourge_die =
{
    // check for gib
   // stop sound
   remove(self.lastvictim);
   self.spawnsilent = 0;
   scourge_think();
   if(self.health < -35)
   {
      sound(self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
      ThrowHead ("progs/h_scourg.mdl", self.health);
      ThrowGib("progs/gib1.mdl", self.health);
      ThrowGib("progs/gib2.mdl", self.health);
      ThrowGib("progs/gib3.mdl", self.health);
      ScourgeDrop();
      return;
   }

// regular death
   sound(self, CHAN_VOICE, "scourge/pain2.wav", 1, ATTN_NORM);
   scourge_die1();
};

void() scourge_melee =
{
    scourge_melee1();
    SUB_AttackFinished (2*random());
}

void() ScourgeTriggerThink =
{
    if(self.lastvictim.health<=0)
    {
        remove(self);
        return;
    }

    entity targ = self.lastvictim;
    makevectors(targ.angles);
    setorigin(self, targ.origin + (v_forward*300));
    self.nextthink = time + 0.1;
}

void() ScourgeTriggerTouch =
{
    if(other.flags & (FL_MONSTER | FL_CLIENT))
        return;

    if(other.movetype != MOVETYPE_FLYMISSILE)
        return;

    entity targ = self.lastvictim;
    if(targ.health <= 0)
    {
        remove(self);
        return;
    }

    vector dir = targ.origin - other.origin;
    dir = normalize(dir);

    if((dir * normalize(other.velocity))<0.8)
        return;

    entity tempent;
    if(time > self.lastvictim.duration)
    {
        tempent = self;
        self = targ;

        if(random()<0.5)
            scourge_strafeleft1();
        else
            scourge_straferight1();

        self = tempent;
        self.lastvictim.duration = time + 1.25;
    }

};

/*QUAKED monster_scourge (1 0 0) (-16 -16 -24) (16 16 40) Ambush

*/
void() monster_scourge =
{
    if(deathmatch)
    {
        remove(self);
        return;
    }

    precache_model2("progs/scor.mdl");
    precache_model2("progs/h_scourg.mdl");
    precache_model2("progs/laser.mdl");
    precache_model2("progs/s_light.mdl");
    precache_model2("progs/spike.mdl");

    precache_sound("misc/null.wav");
    precache_sound("scourge/idle.wav");
    precache_sound("scourge/pain.wav");
    precache_sound("scourge/pain2.wav");
    precache_sound("scourge/sight.wav");
    precache_sound("scourge/tailswng.wav");
    precache_sound("scourge/walk.wav");
    precache_sound2("shambler/smack.wav");
    precache_sound2("weapons/rocket1i.wav");

    // HONEY
    monster_add_to_counter();

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;

    setmodel(self, "progs/scor.mdl");

    setsize(self, '-32 -32 -24', '32 32 24');
    self.health = 300;

    self.th_stand = scourge_stand1;
    self.th_walk = scourge_walk1;
    self.th_run = scourge_run1;
    self.th_pain = scourge_pain;
    self.th_die = scourge_die;
    self.th_melee = scourge_melee;
    self.th_missile = scourge_atk1;
    self.yaw_speed = 60;
    self.attack_state = AS_DODGING;
    self.state = 0;
    self.duration = time;

    walkmonster_start();
};
