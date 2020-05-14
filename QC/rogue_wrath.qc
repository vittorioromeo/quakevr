// wrath.qc

$skin wrthskin

$frame wrthfrme

$frame wrthwk01 wrthwk02 wrthwk03 wrthwk04 wrthwk05 wrthwk06
$frame wrthwk07 wrthwk08 wrthwk09 wrthwk10 wrthwk11 wrthwk12

$frame wrthaa01 wrthaa02 wrthaa03 wrthaa04 wrthaa05 wrthaa06 wrthaa07
$frame wrthaa08 wrthaa09 wrthaa10 wrthaa11 wrthaa12 wrthaa13 wrthaa14

$frame wrthab01 wrthab02 wrthab03 wrthab04 wrthab05 wrthab06 wrthab07
$frame wrthab08 wrthab09 wrthab10 wrthab11 wrthab12 wrthab13

$frame wrthac01 wrthac02 wrthac03 wrthac04 wrthac05
$frame wrthac06 wrthac07 wrthac08 wrthac09 wrthac10
$frame wrthac11 wrthac12 wrthac13 wrthac14 wrthac15

$frame wrthpa01 wrthpa02 wrthpa03 wrthpa04 wrthpa05 wrthpa06

$frame wrthpb01 wrthpb02 wrthpb03 wrthpb04 wrthpb05 wrthpb06
$frame wrthpb07 wrthpb08 wrthpb09 wrthpb10 wrthpb11

$frame wrthdt01 wrthdt02 wrthdt03 wrthdt04 wrthdt05
$frame wrthdt06 wrthdt07 wrthdt08 wrthdt09 wrthdt10
$frame wrthdt11 wrthdt12 wrthdt13 wrthdt14 wrthdt15

void()    wrath_stand1 =[    $wrthwk01,    wrath_stand1 ] { ai_stand();};

void()    wrath_walk01 =[    $wrthwk01,    wrath_walk02 ] { ai_walk(8); };
void()    wrath_walk02 =[    $wrthwk02,    wrath_walk03 ] { ai_walk(8); };
void()    wrath_walk03 =[    $wrthwk03,    wrath_walk04 ] { ai_walk(8); };
void()    wrath_walk04 =[    $wrthwk04,    wrath_walk05 ] { ai_walk(8); };
void()    wrath_walk05 =[    $wrthwk05,    wrath_walk06 ] { ai_walk(8); };
void()    wrath_walk06 =[    $wrthwk06,    wrath_walk07 ] { ai_walk(8); };
void()    wrath_walk07 =[    $wrthwk07,    wrath_walk08 ] { ai_walk(8); };
void()    wrath_walk08 =[    $wrthwk08,    wrath_walk09 ] { ai_walk(8); };
void()    wrath_walk09 =[    $wrthwk09,    wrath_walk10 ] { ai_walk(8); };
void()    wrath_walk10 =[    $wrthwk10,    wrath_walk11 ] { ai_walk(8); };
void()    wrath_walk11 =[    $wrthwk11,    wrath_walk12 ] { ai_walk(8); };
void()    wrath_walk12 =[    $wrthwk12,    wrath_walk01 ] { ai_walk(8); };

void()    wrath_run01    =[    $wrthwk01,    wrath_run02 ] { ai_run(12); };
void()    wrath_run02    =[    $wrthwk02,    wrath_run03 ] { ai_run(12); };
void()    wrath_run03    =[    $wrthwk03,    wrath_run04 ] { ai_run(12); };
void()    wrath_run04    =[    $wrthwk04,    wrath_run05 ] { ai_run(12); };
void()    wrath_run05    =[    $wrthwk05,    wrath_run06 ] { ai_run(12); };
void()    wrath_run06    =[    $wrthwk06,    wrath_run07 ] { ai_run(12); };
void()    wrath_run07    =[    $wrthwk07,    wrath_run08 ] { ai_run(12); };
void()    wrath_run08    =[    $wrthwk08,    wrath_run09 ] { ai_run(12); };
void()    wrath_run09    =[    $wrthwk09,    wrath_run10 ] { ai_run(12); };
void()    wrath_run10 =[    $wrthwk10,    wrath_run11 ] { ai_run(12); };
void()    wrath_run11 =[    $wrthwk11,    wrath_run12 ] { ai_run(12); };
void()    wrath_run12 =[    $wrthwk12,    wrath_run01 ] { ai_run(12); };

void()    wrath_at_a01 =[    $wrthaa01,    wrath_at_a02 ] {ai_charge(12);};
void()    wrath_at_a02 =[    $wrthaa02,    wrath_at_a03 ] {ai_charge(12);};
void()    wrath_at_a03 =[    $wrthaa03,    wrath_at_a04 ] {ai_charge(12);};
void()    wrath_at_a04 =[    $wrthaa04,    wrath_at_a05 ] {ai_charge(12);};
void()    wrath_at_a05 =[    $wrthaa05,    wrath_at_a06 ] {ai_charge(12);};
void()    wrath_at_a06 =[    $wrthaa06,    wrath_at_a07 ] {ai_charge(12);};
void()    wrath_at_a07 =[    $wrthaa07,    wrath_at_a08 ] {ai_charge(12);};
void()    wrath_at_a08 =[    $wrthaa08,    wrath_at_a09 ] {ai_charge(12);};
void()    wrath_at_a09 =[    $wrthaa09,    wrath_at_a10 ] {ai_charge(12);};
void()    wrath_at_a10 =[    $wrthaa10,    wrath_at_a11 ] {ai_charge(12);};
void()    wrath_at_a11 =[    $wrthaa11,    wrath_at_a12 ] {WrathMissile(1);};
void()    wrath_at_a12 =[    $wrthaa12,    wrath_at_a13 ] {ai_charge(12);};
void()    wrath_at_a13 =[    $wrthaa13,    wrath_at_a14 ] {ai_charge(12);};
void()    wrath_at_a14 =[    $wrthaa14,    wrath_run01 ] {ai_charge(12);};

void()    wrath_at_b01 =[    $wrthab01,    wrath_at_b02 ] {ai_charge(12);};
void()    wrath_at_b02 =[    $wrthab02,    wrath_at_b03 ] {ai_charge(12);};
void()    wrath_at_b03 =[    $wrthab03,    wrath_at_b04 ] {ai_charge(12);};
void()    wrath_at_b04 =[    $wrthab04,    wrath_at_b05 ] {ai_charge(12);};
void()    wrath_at_b05 =[    $wrthab05,    wrath_at_b06 ] {ai_charge(12);};
void()    wrath_at_b06 =[    $wrthab06,    wrath_at_b07 ] {WrathMissile(2);};
void()    wrath_at_b07 =[    $wrthab07,    wrath_at_b08 ] {ai_charge(12);};
void()    wrath_at_b08 =[    $wrthab08,    wrath_at_b09 ] {ai_charge(12);};
void()    wrath_at_b09 =[    $wrthab09,    wrath_at_b10 ] {ai_charge(12);};
void()    wrath_at_b10 =[    $wrthab10,    wrath_at_b11 ] {ai_charge(12);};
void()    wrath_at_b11 =[    $wrthab11,    wrath_at_b12 ] {ai_charge(12);};
void()    wrath_at_b12 =[    $wrthab12,    wrath_at_b13 ] {ai_charge(12);};
void()    wrath_at_b13 =[    $wrthab13,    wrath_run01 ] {ai_charge(12);};

void()    wrath_at_c01 =[    $wrthac01,    wrath_at_c02 ] {ai_charge(12);};
void()    wrath_at_c02 =[    $wrthac02,    wrath_at_c03 ] {ai_charge(12);};
void()    wrath_at_c03 =[    $wrthac03,    wrath_at_c04 ] {ai_charge(12);};
void()    wrath_at_c04 =[    $wrthac04,    wrath_at_c05 ] {ai_charge(12);};
void()    wrath_at_c05 =[    $wrthac05,    wrath_at_c06 ] {ai_charge(12);};
void()    wrath_at_c06 =[    $wrthac06,    wrath_at_c07 ] {ai_charge(12);};
void()    wrath_at_c07 =[    $wrthac07,    wrath_at_c08 ] {WrathMissile(3);};
void()    wrath_at_c08 =[    $wrthac08,    wrath_at_c09 ] {ai_charge(12);};
void()    wrath_at_c09 =[    $wrthac09,    wrath_at_c10 ] {ai_charge(12);};
void()    wrath_at_c10 =[    $wrthac10,    wrath_at_c11 ] {ai_charge(12);};
void()    wrath_at_c11 =[    $wrthac11,    wrath_at_c12 ] {ai_charge(12);};
void()    wrath_at_c12 =[    $wrthac12,    wrath_at_c13 ] {ai_charge(12);};
void()    wrath_at_c13 =[    $wrthac13,    wrath_at_c14 ] {ai_charge(12);};
void()    wrath_at_c14 =[    $wrthac14,    wrath_at_c15 ] {ai_charge(12);};
void()    wrath_at_c15 =[    $wrthac15,    wrath_run01 ] {ai_charge(12);};

void() wrath_attack =
{
    float r;

    r = random();

    if(r < 0.25)
    {
        wrath_at_a01();
    }
    else if(r < 0.65)
    {
        wrath_at_b01();
    }
    else
    {
        wrath_at_c01();
    }
    sound(self, CHAN_VOICE, "wrath/watt.wav", 1, ATTN_NORM);
};

void()    wrath_pn_a01    =[    $wrthpa01,    wrath_pn_a02    ] {};
void()    wrath_pn_a02    =[    $wrthpa02,    wrath_pn_a03    ] {};
void()    wrath_pn_a03    =[    $wrthpa03,    wrath_pn_a04    ] {};
void()    wrath_pn_a04    =[    $wrthpa04,    wrath_pn_a05    ] {};
void()    wrath_pn_a05    =[    $wrthpa05,    wrath_pn_a06    ] {};
void()    wrath_pn_a06    =[    $wrthpa06,    wrath_run01        ] {};

void()    wrath_pn_b01    =[    $wrthpb01,    wrath_pn_b02    ] {};
void()    wrath_pn_b02    =[    $wrthpb02,    wrath_pn_b03    ] {};
void()    wrath_pn_b03    =[    $wrthpb03,    wrath_pn_b04    ] {};
void()    wrath_pn_b04    =[    $wrthpb04,    wrath_pn_b05    ] {};
void()    wrath_pn_b05    =[    $wrthpb05,    wrath_pn_b06    ] {};
void()    wrath_pn_b06    =[    $wrthpb06,    wrath_pn_b07    ] {};
void()    wrath_pn_b07    =[    $wrthpb07,    wrath_pn_b08    ] {};
void()    wrath_pn_b08    =[    $wrthpb08,    wrath_pn_b09    ] {};
void()    wrath_pn_b09    =[    $wrthpb09,    wrath_pn_b10    ] {};
void()    wrath_pn_b10    =[    $wrthpb10,    wrath_pn_b11    ] {};
void()    wrath_pn_b11    =[    $wrthpb11,    wrath_run01        ] {};


void(entity attacker, float damage) wrath_pain =
{
    float r;

    if(self.pain_finished > time)
        return;

    r = random();

    if(r > 0.1)
    {
        self.pain_finished = time + 0.5;
        return;
    }

    if(r < 0.07)
        wrath_pn_a01();
    else
        wrath_pn_b01();

    self.pain_finished = time + 3;
    sound(self, CHAN_VOICE, "wrath/wpain.wav", 1, ATTN_NORM);
};

void()    wrath_die02 =[ $wrthdt02, wrath_die03 ]
    { sound(self, CHAN_VOICE, "wrath/wdthc.wav", 1, ATTN_NORM); };
void()    wrath_die03 =[ $wrthdt03, wrath_die04 ] {};
void()    wrath_die04 =[ $wrthdt04, wrath_die05 ] {};
void()    wrath_die05 =[ $wrthdt05, wrath_die07 ] {};
void()    wrath_die07 =[ $wrthdt07, wrath_die09 ] {};
void()    wrath_die09 =[ $wrthdt09, wrath_die11 ] {};
void()    wrath_die11 =[ $wrthdt11, wrath_die13 ] {};
void()    wrath_die13 =[ $wrthdt13, wrath_die15 ] {};
void()    wrath_die15 =[ $wrthdt15, wrath_die15 ]
{
    ThrowGib("progs/wrthgib1.mdl", self.health);
    ThrowGib("progs/wrthgib2.mdl", self.health);
    ThrowGib("progs/wrthgib3.mdl", self.health);
    T_RadiusDamage(self, self, 80, world);

    self.origin = self.origin + '0 0 24';

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION2);
    WriteCoord(MSG_BROADCAST, self.origin_x);
    WriteCoord(MSG_BROADCAST, self.origin_y);
    WriteCoord(MSG_BROADCAST, self.origin_z);
    WriteByte(MSG_BROADCAST, 0);
    WriteByte(MSG_BROADCAST, 4);

    remove(self);
};

/*QUAKED monster_wrath (1 0 0) (-16 -16 -24) (16 16 32) Ambush
*/
void() monster_wrath =
{
    if(deathmatch)
    {
        remove(self);
        return;
    }
    precache_model("progs/wrath.mdl");
    precache_model("progs/w_ball.mdl");
    precache_model("progs/wrthgib1.mdl");
    precache_model("progs/wrthgib2.mdl");
    precache_model("progs/wrthgib3.mdl");

    precache_sound("wrath/wsee.wav");
    precache_sound("wrath/watt.wav");
    precache_sound("wrath/wpain.wav");
    precache_sound("wrath/wdthc.wav");

    // HONEY
    monster_add_to_counter();

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;

    setmodel(self, "progs/wrath.mdl");

    setsize(self, VEC_HULL_MIN, VEC_HULL_MAX);

    self.health = 400;
    self.yaw_speed = 35;

    self.th_stand = wrath_stand1;
    self.th_walk = wrath_walk01;
    self.th_run = wrath_run01;
    self.th_missile = wrath_attack;
    self.th_pain = wrath_pain;
    self.th_die = wrath_die02;

    flymonster_start();
};

/*
================
WrathMissile
================
*/
void(float AttackNumber) WrathMissile =
{
    entity     missile;
    vector    dir, org;
    float    dist, flytime;

    dir = normalize((self.enemy.origin + '0 0 10') - self.origin);
    dist = vlen(self.enemy.origin - self.origin);
    flytime = dist * 0.002;
    if(flytime < 0.1)
        flytime = 0.1;

    self.effects = self.effects | EF_MUZZLEFLASH;

    makevectors(self.angles);

    missile = spawn();
    missile.owner = self;
    missile.solid = SOLID_BBOX;
    missile.movetype = MOVETYPE_FLYMISSILE;
    setmodel(missile, "progs/w_ball.mdl");

    setsize(missile, '0 0 0', '0 0 0');

    if(AttackNumber == 1)
    {
        org = self.origin + v_forward*20 + v_up*12; // 20 // 44;
    }
    else if(AttackNumber == 2)
    {
        org = self.origin + v_forward*18 + v_up*10; // 18 // 42;
    }
    else if(AttackNumber == 3)
    {
        org = self.origin + v_forward*12 + v_up*12 + v_right*20;
// up20
    }
    else
    {
        ASSERT(AttackNumber == 4);
        org = self.origin + v_forward*20 + v_up*16;
    }

    setorigin(missile, org);
    missile.velocity = dir * 400;
    missile.avelocity = '300 300 300';
    missile.enemy = self.enemy;
    missile.touch = WrathMissileTouch;
    missile.nextthink = time + 0.1;
    missile.think = WrathHome;

    self.attack_finished = time + 2;
};

void() WrathHome =
{
    if(self.enemy.health < 1)
    {
        remove(self);
        return;
    }

    if(skill == 3)
        ai_track(self.enemy, 550);
    else
        ai_track(self.enemy, 400);

    self.nextthink = time + 0.1;
    self.think = WrathHome;
};

void() WrathMissileTouch =
{
    if(other == self.owner ||
        other.classname == "monster_wrath" ||
        other.classname == "monster_super_wrath")
    {
        remove(self);
        return;        // don't explode on any wraths

    }

    if(other.classname == "monster_zombie")
        T_Damage(other, self, self, 110);

    T_RadiusDamage(self, self.owner, 20, world);
    sound(self, CHAN_WEAPON, "weapons/r_exp3.wav", 1, ATTN_NORM);

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord(MSG_BROADCAST, self.origin_x);
    WriteCoord(MSG_BROADCAST, self.origin_y);
    WriteCoord(MSG_BROADCAST, self.origin_z);

    self.velocity = '0 0 0';
    self.touch = SUB_Null;
    setmodel(self, "progs/s_explod.spr");
    self.solid = SOLID_NOT;
    s_explode1();
};
