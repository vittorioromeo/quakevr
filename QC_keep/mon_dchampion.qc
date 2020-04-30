/*==============================================================================
KNIGHT
==============================================================================*/
$cd id1/models/knight2
$origin 0 0 24
$base base
$skin skin

// 000-008
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8 stand9

// 009-028
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9
$frame walk10 walk11 walk12 walk13 walk14 walk15 walk16 walk17
$frame walk18 walk19 walk20

// 029-036
$frame run1 run2 run3 run4 run5 run6 run7 run8

// 037-041
$frame pain1 pain2 pain3 pain4 pain5

// 042-053
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12

// 054-062
$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9

// 063-078
$frame char_a1 char_a2 char_a3 char_a4 char_a5 char_a6 char_a7 char_a8
$frame char_a9 char_a10 char_a11 char_a12 char_a13 char_a14 char_a15 char_a16

// 079-092
$frame magica1 magica2 magica3 magica4 magica5 magica6 magica7 magica8
$frame magica9 magica10 magica11 magica12 magica13 magica14

// 093-105
$frame magicb1 magicb2 magicb3 magicb4 magicb5 magicb6 magicb7 magicb8
$frame magicb9 magicb10 magicb11 magicb12 magicb13

// 106-111
$frame char_b1 char_b2 char_b3 char_b4 char_b5 char_b6

// 112-121
$frame slice1 slice2 slice3 slice4 slice5 slice6 slice7 slice8 slice9 slice10

// 122-132
$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7 smash8 smash9 smash10
$frame smash11

// 133-154
$frame w_attack1 w_attack2 w_attack3 w_attack4 w_attack5 w_attack6 w_attack7 
$frame w_attack8 w_attack9 w_attack10 w_attack11 w_attack12 w_attack13 w_attack14
$frame w_attack15 w_attack16 w_attack17 w_attack18 w_attack19 w_attack20 
$frame w_attack21 w_attack22 

// 155-165
$frame magicc1 magicc2 magicc3 magicc4 magicc5 magicc6 magicc7 magicc8
$frame magicc9 magicc10 magicc11

//----------------------------------------------------------------------
// ben - 08/04/03 - a champion knight  always strides into battle - never runs.
void()    hknight_stride1    =[    $walk1,    hknight_stride2    ] {monster_idle_sound();ai_run(2);};
void()    hknight_stride2    =[    $walk2,    hknight_stride3    ] {ai_run(5);};
void()    hknight_stride3    =[    $walk3,    hknight_stride4    ] {ai_run(5);};
void()    hknight_stride4    =[    $walk4,    hknight_stride5    ] {ai_run(4);};
void()    hknight_stride5    =[    $walk5,    hknight_stride6    ] {ai_run(4);};
void()    hknight_stride6    =[    $walk6,    hknight_stride7    ] {ai_run(2);};
void()    hknight_stride7    =[    $walk7,    hknight_stride8    ] {ai_run(2);};
void()    hknight_stride8    =[    $walk8,    hknight_stride9    ] {ai_run(3);};
void()    hknight_stride9    =[    $walk9,    hknight_stride10    ] {ai_run(3);};
void()    hknight_stride10    =[    $walk10,    hknight_stride11    ] {ai_run(4);};
void()    hknight_stride11    =[    $walk11,    hknight_stride12    ] {ai_run(3);};
void()    hknight_stride12    =[    $walk12,    hknight_stride13    ] {ai_run(4);};
void()    hknight_stride13    =[    $walk13,    hknight_stride14    ] {ai_run(6);};
void()    hknight_stride14    =[    $walk14,    hknight_stride15    ] {ai_run(2);};
void()    hknight_stride15    =[    $walk15,    hknight_stride16    ] {ai_run(2);};
void()    hknight_stride16    =[    $walk16,    hknight_stride17    ] {ai_run(4);};
void()    hknight_stride17    =[    $walk17,    hknight_stride18    ] {ai_run(3);};
void()    hknight_stride18    =[    $walk18,    hknight_stride19    ] {ai_run(3);};
void()    hknight_stride19    =[    $walk19,    hknight_stride20    ] {ai_run(3);};
void()    hknight_stride20    =[    $walk20,    hknight_stride1    ] {ai_run(2);};

void() hknight_champion_magic = {
// a champion has a choice of two missile attacks - a faster version of the standard attack,
// and a vore-style homing missile. eventually he will be able to teleport.

    local float chance;

    chance = random();

    if (chance < 0.6)
        hknight_magicc1();

    else
        hknight_magicb1();
};

void() monster_hell_knight_champion { 
if (self.deathstring == "")  self.deathstring = " was conquered by a Champion Death Knight\n";
monster_hell_knight();
}