/*
==============================================================================

mummy

==============================================================================
*/
$cd /qwork/xpack/models/mummy

$origin 0 0 24

$base base
$skin skin

$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8
$frame stand9 stand10 stand11 stand12 stand13 stand14 stand15

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9 walk10 walk11
$frame walk12 walk13 walk14 walk15 walk16 walk17 walk18 walk19

$frame run1 run2 run3 run4 run5 run6 run7 run8 run9 run10 run11 run12
$frame run13 run14 run15 run16 run17 run18

$frame atta1 atta2 atta3 atta4 atta5 atta6 atta7 atta8 atta9 atta10 atta11
$frame atta12 atta13

$frame attb1 attb2 attb3 attb4 attb5 attb6 attb7 attb8 attb9 attb10 attb11
$frame attb12 attb13 attb14

$frame attc1 attc2 attc3 attc4 attc5 attc6 attc7 attc8 attc9 attc10 attc11
$frame attc12

$frame paina1 paina2 paina3 paina4 paina5 paina6 paina7 paina8 paina9 paina10
$frame paina11 paina12

$frame painb1 painb2 painb3 painb4 painb5 painb6 painb7 painb8 painb9 painb10
$frame painb11 painb12 painb13 painb14 painb15 painb16 painb17 painb18 painb19
$frame painb20 painb21 painb22 painb23 painb24 painb25 painb26 painb27 painb28

$frame painc1 painc2 painc3 painc4 painc5 painc6 painc7 painc8 painc9 painc10
$frame painc11 painc12 painc13 painc14 painc15 painc16 painc17 painc18

$frame paind1 paind2 paind3 paind4 paind5 paind6 paind7 paind8 paind9 paind10
$frame paind11 paind12 paind13

$frame paine1 paine2 paine3 paine4 paine5 paine6 paine7 paine8 paine9 paine10
$frame paine11 paine12 paine13 paine14 paine15 paine16 paine17 paine18 paine19
$frame paine20 paine21 paine22 paine23 paine24 paine25 paine26 paine27 paine28
$frame paine29 paine30

//=============================================================================

.float inpain;


void() mummy_stand1    =[  $stand1,        mummy_stand2   ] {ai_stand();};
void() mummy_stand2    =[  $stand2,        mummy_stand3   ] {ai_stand();};
void() mummy_stand3    =[  $stand3,        mummy_stand4   ] {ai_stand();};
void() mummy_stand4    =[  $stand4,        mummy_stand5   ] {ai_stand();};
void() mummy_stand5    =[  $stand5,        mummy_stand6   ] {ai_stand();};
void() mummy_stand6    =[  $stand6,        mummy_stand7   ] {ai_stand();};
void() mummy_stand7    =[  $stand7,        mummy_stand8   ] {ai_stand();};
void() mummy_stand8    =[  $stand8,        mummy_stand9   ] {ai_stand();};
void() mummy_stand9    =[  $stand9,        mummy_stand10  ] {ai_stand();};
void() mummy_stand10   =[  $stand10,       mummy_stand11  ] {ai_stand();};
void() mummy_stand11   =[  $stand11,       mummy_stand12  ] {ai_stand();};
void() mummy_stand12   =[  $stand12,       mummy_stand13  ] {ai_stand();};
void() mummy_stand13   =[  $stand13,       mummy_stand14  ] {ai_stand();};
void() mummy_stand14   =[  $stand14,       mummy_stand15  ] {ai_stand();};
void() mummy_stand15   =[  $stand15,       mummy_stand1   ] {ai_stand();};

void() mummy_walk1     =[  $walk1,     mummy_walk2    ] {ai_walk(0);};
void() mummy_walk2     =[  $walk2,     mummy_walk3    ] {ai_walk(3);};
void() mummy_walk3     =[  $walk3,     mummy_walk4    ] {ai_walk(5);};
void() mummy_walk4     =[  $walk4,     mummy_walk5    ] {ai_walk(4);};
void() mummy_walk5     =[  $walk5,     mummy_walk6    ] {ai_walk(2);};
void() mummy_walk6     =[  $walk6,     mummy_walk7    ] {ai_walk(0);};
void() mummy_walk7     =[  $walk7,     mummy_walk8    ] {ai_walk(0);};
void() mummy_walk8     =[  $walk8,     mummy_walk9    ] {ai_walk(0);};
void() mummy_walk9     =[  $walk9,     mummy_walk10   ] {ai_walk(0);};
void() mummy_walk10    =[  $walk10,    mummy_walk11   ] {ai_walk(0);};
void() mummy_walk11    =[  $walk11,    mummy_walk12   ] {ai_walk(3);};
void() mummy_walk12    =[  $walk12,    mummy_walk13   ] {ai_walk(4);};
void() mummy_walk13    =[  $walk13,    mummy_walk14   ] {ai_walk(2);};
void() mummy_walk14    =[  $walk14,    mummy_walk15   ] {ai_walk(0);};
void() mummy_walk15    =[  $walk15,    mummy_walk16   ] {ai_walk(0);};
void() mummy_walk16    =[  $walk16,    mummy_walk17   ] {ai_walk(0);};
void() mummy_walk17    =[  $walk17,    mummy_walk18   ] {ai_walk(0);};
void() mummy_walk18    =[  $walk18,    mummy_walk19   ] {ai_walk(0);};
void() mummy_walk19    =[  $walk19,    mummy_walk1    ]
    {
    ai_walk(0);
    if(random() < 0.2)
        sound(self, CHAN_VOICE, "zombie/z_idle.wav", 1, ATTN_IDLE);
    };

void() mummy_run1      =[  $run1,      mummy_run2 ] {ai_run(2);self.inpain = 0;};
void() mummy_run2      =[  $run2,      mummy_run3 ] {ai_run(2);};
void() mummy_run3      =[  $run3,      mummy_run4 ] {ai_run(0);};
void() mummy_run4      =[  $run4,      mummy_run5 ] {ai_run(2);};
void() mummy_run5      =[  $run5,      mummy_run6 ] {ai_run(4);};
void() mummy_run6      =[  $run6,      mummy_run7 ] {ai_run(6);};
void() mummy_run7      =[  $run7,      mummy_run8 ] {ai_run(8);};
void() mummy_run8      =[  $run8,      mummy_run9 ] {ai_run(8);};
void() mummy_run9      =[  $run9,      mummy_run10    ] {ai_run(4);};
void() mummy_run10     =[  $run10,     mummy_run11    ] {ai_run(0);};
void() mummy_run11     =[  $run11,     mummy_run12    ] {ai_run(0);};
void() mummy_run12     =[  $run12,     mummy_run13    ] {ai_run(0);};
void() mummy_run13     =[  $run13,     mummy_run14    ] {ai_run(4);};
void() mummy_run14     =[  $run14,     mummy_run15    ] {ai_run(8);};
void() mummy_run15     =[  $run15,     mummy_run16    ] {ai_run(12);};
void() mummy_run16     =[  $run16,     mummy_run17    ] {ai_run(14);};
void() mummy_run17     =[  $run17,     mummy_run18    ] {ai_run(6);};
void() mummy_run18     =[  $run18,     mummy_run1 ]
    {
    ai_run(16);
    if(random() < 0.2)
        sound(self, CHAN_VOICE, "zombie/z_idle.wav", 1, ATTN_IDLE);
    if(random() > 0.8)
        sound(self, CHAN_VOICE, "zombie/z_idle1.wav", 1, ATTN_IDLE);
    };

/*
=============================================================================

ATTACKS

=============================================================================
*/

void() mummyGrenadeTouch =
{
    local float     mummyDamage;

    if(other == self.owner)
        return;     // don't explode on owner
    if(other.takedamage)
    {
        mummyDamage = 15 + random() * 15;

        T_Damage(other, self, self.owner, mummyDamage);
        sound(self, CHAN_WEAPON, "zombie/z_hit.wav", 1, ATTN_NORM);
        remove(self);
        return;
    }
    sound(self, CHAN_WEAPON, "zombie/z_miss.wav", 1, ATTN_NORM);   // bounce sound
    self.velocity = '0 0 0';
    self.avelocity = '0 0 0';
    self.touch = SUB_Remove;
};

/*
================
mummyFireGrenade
================
*/
void(vector st) mummyFireGrenade =
{
    local   entity missile, mpuff;
    local   vector  org;

    sound(self, CHAN_WEAPON, "zombie/z_shot1.wav", 1, ATTN_NORM);

    missile = spawn();
    missile.owner = self;
    missile.movetype = MOVETYPE_BOUNCE;
    missile.solid = SOLID_BBOX;

// calc org
    org = self.origin + st_x * v_forward + st_y * v_right + (st_z - 24) * v_up;

// set missile speed

    makevectors(self.angles);

    missile.velocity = normalize(self.enemy.origin - org);
    missile.velocity = missile.velocity * 600;
    missile.velocity_z = 200;

    missile.avelocity = '3000 1000 2000';

    missile.touch = mummyGrenadeTouch;

// set missile duration
    missile.nextthink = time + 2.5;
    missile.think = SUB_Remove;

    setmodel(missile, "progs/zom_gib.mdl");
    setsize(missile, '0 0 0', '0 0 0');
    setorigin(missile, org);
};


void() mummy_atta1     =[  $atta1,     mummy_atta2    ] {ai_face();};
void() mummy_atta2     =[  $atta2,     mummy_atta3    ] {ai_face();};
void() mummy_atta3     =[  $atta3,     mummy_atta4    ] {ai_face();};
void() mummy_atta4     =[  $atta4,     mummy_atta5    ] {ai_face();};
void() mummy_atta5     =[  $atta5,     mummy_atta6    ] {ai_face();};
void() mummy_atta6     =[  $atta6,     mummy_atta7    ] {ai_face();};
void() mummy_atta7     =[  $atta7,     mummy_atta8    ] {ai_face();};
void() mummy_atta8     =[  $atta8,     mummy_atta9    ] {ai_face();};
void() mummy_atta9     =[  $atta9,     mummy_atta10   ] {ai_face();};
void() mummy_atta10    =[  $atta10,    mummy_atta11   ] {ai_face();};
void() mummy_atta11    =[  $atta11,    mummy_atta12   ] {ai_face();};
void() mummy_atta12    =[  $atta12,    mummy_atta13   ] {ai_face();};
void() mummy_atta13    =[  $atta13,    mummy_run1 ]
    { ai_face(); mummyFireGrenade('-10 -22 30'); };

void() mummy_attb1     =[  $attb1,     mummy_attb2    ] {ai_face();};
void() mummy_attb2     =[  $attb2,     mummy_attb3    ] {ai_face();};
void() mummy_attb3     =[  $attb3,     mummy_attb4    ] {ai_face();};
void() mummy_attb4     =[  $attb4,     mummy_attb5    ] {ai_face();};
void() mummy_attb5     =[  $attb5,     mummy_attb6    ] {ai_face();};
void() mummy_attb6     =[  $attb6,     mummy_attb7    ] {ai_face();};
void() mummy_attb7     =[  $attb7,     mummy_attb8    ] {ai_face();};
void() mummy_attb8     =[  $attb8,     mummy_attb9    ] {ai_face();};
void() mummy_attb9     =[  $attb9,     mummy_attb10   ] {ai_face();};
void() mummy_attb10    =[  $attb10,    mummy_attb11   ] {ai_face();};
void() mummy_attb11    =[  $attb11,    mummy_attb12   ] {ai_face();};
void() mummy_attb12    =[  $attb12,    mummy_attb13   ] {ai_face();};
void() mummy_attb13    =[  $attb13,    mummy_attb14   ] {ai_face();};
void() mummy_attb14    =[  $attb13,    mummy_run1 ]
    { ai_face(); mummyFireGrenade('-10 -24 29'); };

void() mummy_attc1     =[  $attc1,     mummy_attc2    ] {ai_face();};
void() mummy_attc2     =[  $attc2,     mummy_attc3    ] {ai_face();};
void() mummy_attc3     =[  $attc3,     mummy_attc4    ] {ai_face();};
void() mummy_attc4     =[  $attc4,     mummy_attc5    ] {ai_face();};
void() mummy_attc5     =[  $attc5,     mummy_attc6    ] {ai_face();};
void() mummy_attc6     =[  $attc6,     mummy_attc7    ] {ai_face();};
void() mummy_attc7     =[  $attc7,     mummy_attc8    ] {ai_face();};
void() mummy_attc8     =[  $attc8,     mummy_attc9    ] {ai_face();};
void() mummy_attc9     =[  $attc9,     mummy_attc10   ] {ai_face();};
void() mummy_attc10    =[  $attc10,    mummy_attc11   ] {ai_face();};
void() mummy_attc11    =[  $attc11,    mummy_attc12   ] {ai_face();};
void() mummy_attc12    =[  $attc12,    mummy_run1     ]
    { ai_face(); mummyFireGrenade('-12 -19 29'); };

void() mummy_missile =
{
    local float r;

    r = random();

    if(r < 0.3)
        mummy_atta1();
    else if(r < 0.6)
        mummy_attb1();
    else
        mummy_attc1();
};


/*
=============================================================================

PAIN

=============================================================================
*/

void() mummy_paina1    =[  $paina1,    mummy_paina2   ]
    {sound(self, CHAN_VOICE, "zombie/z_pain.wav", 1, ATTN_NORM);};
void() mummy_paina2    =[  $paina2,    mummy_paina3   ] {ai_painforward(3);};
void() mummy_paina3    =[  $paina3,    mummy_paina4   ] {ai_painforward(1);};
void() mummy_paina4    =[  $paina4,    mummy_paina5   ] {ai_pain(1);};
void() mummy_paina5    =[  $paina5,    mummy_paina6   ] {ai_pain(3);};
void() mummy_paina6    =[  $paina6,    mummy_paina7   ] {ai_pain(1);};
void() mummy_paina7    =[  $paina7,    mummy_paina8   ] {};
void() mummy_paina8    =[  $paina8,    mummy_paina9   ] {};
void() mummy_paina9    =[  $paina9,    mummy_paina10  ] {};
void() mummy_paina10   =[  $paina10,   mummy_paina11  ] {};
void() mummy_paina11   =[  $paina11,   mummy_paina12  ] {};
void() mummy_paina12   =[  $paina12,   mummy_run1     ] {};

void() mummy_painb1    =[  $painb1,    mummy_painb2   ]
    {sound(self, CHAN_VOICE, "zombie/z_pain1.wav", 1, ATTN_NORM);};
void() mummy_painb2    =[  $painb2,    mummy_painb3   ] {ai_pain(2);};
void() mummy_painb3    =[  $painb3,    mummy_painb4   ] {ai_pain(8);};
void() mummy_painb4    =[  $painb4,    mummy_painb5   ] {ai_pain(6);};
void() mummy_painb5    =[  $painb5,    mummy_painb6   ] {ai_pain(2);};
void() mummy_painb6    =[  $painb6,    mummy_painb7   ] {};
void() mummy_painb7    =[  $painb7,    mummy_painb8   ] {};
void() mummy_painb8    =[  $painb8,    mummy_painb9   ] {};
void() mummy_painb9    =[  $painb9,    mummy_painb10  ]
    {sound(self, CHAN_BODY, "zombie/z_fall.wav", 1, ATTN_NORM);};
void() mummy_painb10   =[  $painb10,   mummy_painb11  ] {};
void() mummy_painb11   =[  $painb11,   mummy_painb12  ] {};
void() mummy_painb12   =[  $painb12,   mummy_painb13  ] {};
void() mummy_painb13   =[  $painb13,   mummy_painb14  ] {};
void() mummy_painb14   =[  $painb14,   mummy_painb15  ] {};
void() mummy_painb15   =[  $painb15,   mummy_painb16  ] {};
void() mummy_painb16   =[  $painb16,   mummy_painb17  ] {};
void() mummy_painb17   =[  $painb17,   mummy_painb18  ] {};
void() mummy_painb18   =[  $painb18,   mummy_painb19  ] {};
void() mummy_painb19   =[  $painb19,   mummy_painb20  ] {};
void() mummy_painb20   =[  $painb20,   mummy_painb21  ] {};
void() mummy_painb21   =[  $painb21,   mummy_painb22  ] {};
void() mummy_painb22   =[  $painb22,   mummy_painb23  ] {};
void() mummy_painb23   =[  $painb23,   mummy_painb24  ] {};
void() mummy_painb24   =[  $painb24,   mummy_painb25  ] {};
void() mummy_painb25   =[  $painb25,   mummy_painb26  ] {ai_painforward(1);};
void() mummy_painb26   =[  $painb26,   mummy_painb27  ] {};
void() mummy_painb27   =[  $painb27,   mummy_painb28  ] {};
void() mummy_painb28   =[  $painb28,   mummy_run1     ] {};

void() mummy_painc1    =[  $painc1,    mummy_painc2   ]
    {sound(self, CHAN_VOICE, "zombie/z_pain1.wav", 1, ATTN_NORM);};
void() mummy_painc2    =[  $painc2,    mummy_painc3   ] {};
void() mummy_painc3    =[  $painc3,    mummy_painc4   ] {ai_pain(3);};
void() mummy_painc4    =[  $painc4,    mummy_painc5   ] {ai_pain(1);};
void() mummy_painc5    =[  $painc5,    mummy_painc6   ] {};
void() mummy_painc6    =[  $painc6,    mummy_painc7   ] {};
void() mummy_painc7    =[  $painc7,    mummy_painc8   ] {};
void() mummy_painc8    =[  $painc8,    mummy_painc9   ] {};
void() mummy_painc9    =[  $painc9,    mummy_painc10  ] {};
void() mummy_painc10   =[  $painc10,   mummy_painc11  ] {};
void() mummy_painc11   =[  $painc11,   mummy_painc12  ] {ai_painforward(1);};
void() mummy_painc12   =[  $painc12,   mummy_painc13  ] {ai_painforward(1);};
void() mummy_painc13   =[  $painc13,   mummy_painc14  ] {};
void() mummy_painc14   =[  $painc14,   mummy_painc15  ] {};
void() mummy_painc15   =[  $painc15,   mummy_painc16  ] {};
void() mummy_painc16   =[  $painc16,   mummy_painc17  ] {};
void() mummy_painc17   =[  $painc17,   mummy_painc18  ] {};
void() mummy_painc18   =[  $painc18,   mummy_run1 ] {};

void() mummy_paind1    =[  $paind1,    mummy_paind2   ]
    {sound(self, CHAN_VOICE, "zombie/z_pain.wav", 1, ATTN_NORM);};
void() mummy_paind2    =[  $paind2,    mummy_paind3   ] {};
void() mummy_paind3    =[  $paind3,    mummy_paind4   ] {};
void() mummy_paind4    =[  $paind4,    mummy_paind5   ] {};
void() mummy_paind5    =[  $paind5,    mummy_paind6   ] {};
void() mummy_paind6    =[  $paind6,    mummy_paind7   ] {};
void() mummy_paind7    =[  $paind7,    mummy_paind8   ] {};
void() mummy_paind8    =[  $paind8,    mummy_paind9   ] {};
void() mummy_paind9    =[  $paind9,    mummy_paind10  ] {ai_pain(1);};
void() mummy_paind10   =[  $paind10,   mummy_paind11  ] {};
void() mummy_paind11   =[  $paind11,   mummy_paind12  ] {};
void() mummy_paind12   =[  $paind12,   mummy_paind13  ] {};
void() mummy_paind13   =[  $paind13,   mummy_run1 ] {};

void() mummy_paine1    =[  $paine1,    mummy_paine2   ]
    { sound(self, CHAN_VOICE, "zombie/z_pain.wav", 1, ATTN_NORM); };
void() mummy_paine2    =[  $paine2,    mummy_paine3   ] {ai_pain(8);};
void() mummy_paine3    =[  $paine3,    mummy_paine4   ] {ai_pain(5);};
void() mummy_paine4    =[  $paine4,    mummy_paine5   ] {ai_pain(3);};
void() mummy_paine5    =[  $paine5,    mummy_paine6   ] {ai_pain(1);};
void() mummy_paine6    =[  $paine6,    mummy_paine7   ] {ai_pain(2);};
void() mummy_paine7    =[  $paine7,    mummy_paine8   ] {ai_pain(1);};
void() mummy_paine8    =[  $paine8,    mummy_paine9   ] {ai_pain(1);};
void() mummy_paine9    =[  $paine9,    mummy_paine10  ] {ai_pain(2);};
void() mummy_paine10   =[  $paine10,   mummy_paine11  ]
    {
        sound(self, CHAN_BODY, "zombie/z_fall.wav", 1, ATTN_NORM);
        self.solid = SOLID_NOT;
    };
void() mummy_paine11   =[  $paine11,   mummy_paine12  ]
    { self.nextthink = self.nextthink + 5; };
void() mummy_paine12   =[  $paine12,   mummy_paine13  ]
    {
        // see if ok to stand up
        sound(self, CHAN_VOICE, "zombie/z_idle.wav", 1, ATTN_IDLE);
        setsize(self, VEC_HULL_MIN, VEC_HULL_MAX);
        self.solid = SOLID_SLIDEBOX;
        if(!walkmove(0, 0))
        {
            self.think = mummy_paine11;
            self.solid = SOLID_NOT;
            return;
        }
    };
void() mummy_paine13   =[  $paine13,   mummy_paine14  ] {};
void() mummy_paine14   =[  $paine14,   mummy_paine15  ] {};
void() mummy_paine15   =[  $paine15,   mummy_paine16  ] {};
void() mummy_paine16   =[  $paine16,   mummy_paine17  ] {};
void() mummy_paine17   =[  $paine17,   mummy_paine18  ] {};
void() mummy_paine18   =[  $paine18,   mummy_paine19  ] {};
void() mummy_paine19   =[  $paine19,   mummy_paine20  ] {};
void() mummy_paine20   =[  $paine20,   mummy_paine21  ] {};
void() mummy_paine21   =[  $paine21,   mummy_paine22  ] {};
void() mummy_paine22   =[  $paine22,   mummy_paine23  ] {};
void() mummy_paine23   =[  $paine23,   mummy_paine24  ] {};
void() mummy_paine24   =[  $paine24,   mummy_paine25  ] {};
void() mummy_paine25   =[  $paine25,   mummy_paine26  ] {ai_painforward(5);};
void() mummy_paine26   =[  $paine26,   mummy_paine27  ] {ai_painforward(3);};
void() mummy_paine27   =[  $paine27,   mummy_paine28  ] {ai_painforward(1);};
void() mummy_paine28   =[  $paine28,   mummy_paine29  ] {ai_pain(1);};
void() mummy_paine29   =[  $paine29,   mummy_paine30  ] {};
void() mummy_paine30   =[  $paine30,   mummy_run1     ]
    { };

void() mummy_die =
{
    self.health = -35;
    sound(self, CHAN_VOICE, "zombie/z_gib.wav", 1, ATTN_NORM);
    ThrowHead ("progs/h_zombie.mdl", self.health);
    ThrowGib ("progs/gib1.mdl", self.health);
    ThrowGib ("progs/gib2.mdl", self.health);
    ThrowGib ("progs/gib3.mdl", self.health);
};

/*
=================
mummy_pain
=================
*/
void(entity attacker, float take) mummy_pain =
{
    local float     r;

    if(self.pain_finished > time)
        return;

    r = random();
    if(r > 0.24)
        return;

    self.pain_finished = time + 2.5;

    if(r < 0.06)
        mummy_paina1();
    else if(r <  0.12)
        mummy_painb1();
    else if(r <  0.18)
        mummy_painc1();
    else
        mummy_paind1();
};

void() mummy_sleep = [ $paine11, mummy_sleep ] { ai_stand(); };

void() mummy_wake =
{
    self.th_stand = mummy_stand1;
    self.th_walk = mummy_walk1;
    self.th_pain = mummy_pain;
    self.th_run = mummy_run1;
    self.th_missile = mummy_missile;

    mummy_paine12();
};

//============================================================================

float MUMMY_STRONG = 4;

/*QUAKED monster_mummy (1 0 0) (-16 -16 -24) (16 16 32) Ambush Lying Stronger

Lying - mummy will start lying down, and stand when player spotted.
Stronger - mummy starts with 1000 hitpoints, rather than 500.
*/
void() monster_mummy =
{
    if(deathmatch)
    {
        remove(self);
        return;
    }

    precache_model("progs/mummy.mdl");
    precache_model("progs/h_zombie.mdl");
    precache_model("progs/zom_gib.mdl");

    precache_sound("zombie/z_idle.wav");
    precache_sound("zombie/z_idle1.wav");
    precache_sound("zombie/z_shot1.wav");
    precache_sound("zombie/z_gib.wav");
    precache_sound("zombie/z_pain.wav");
    precache_sound("zombie/z_pain1.wav");
    precache_sound("zombie/z_fall.wav");
    precache_sound("zombie/z_miss.wav");
    precache_sound("zombie/z_hit.wav");
    precache_sound("zombie/idle_w2.wav");

    // HONEY
    monster_add_to_counter();

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;

    setmodel(self, "progs/mummy.mdl");

    setsize(self, VEC_HULL_MIN, VEC_HULL_MAX);
    if(self.spawnflags & MUMMY_STRONG)
        self.health = 1000;
    else
        self.health = 500;

    self.th_stand = mummy_stand1;
    self.th_walk = mummy_walk1;
    self.th_run = mummy_run1;
    self.th_pain = mummy_pain;
    self.th_die = mummy_die;
    self.th_missile = mummy_missile;

    if(self.spawnflags & SPAWN_LYING)
    {
        setsize(self, '-16 -16 -24', '16 16 -16');
        self.solid = SOLID_NOT;
        self.th_stand = mummy_sleep;
        self.th_run = mummy_wake;
        self.th_walk = mummy_wake;
        self.th_pain = asPainFn(mummy_wake);
        self.th_missile = mummy_wake;
        walkmonster_start();
    }
    else
        walkmonster_start();
};
