/*==============================================================================
DEATH GUARD (low health)
==============================================================================*/
$cd id1/models/dguard
$origin 0 0 24
$base base
$skin skin

// (001 - 012) Default stand - breathing
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8
$frame stand9 stand10 stand11 stand12

// (013 - 024) Stand idle - Shoulder hunch
$frame standA1 standA2 standA3 standA4 standA5 standA6 standA7 standA8
$frame standA9 standA10 standA11 standA12

// (025 - 048) Stand idle - Quick look around
$frame standB1 standB2 standB3 standB4 standB5 standB6 standB7 standB8 
$frame standB9 standB10 standB11 standB12 standB13 standB14 standB15 standB16
$frame standB17 standB18 standB19 standB20 standB21 standB22 standB23 standB24

// (049 - 072) Stand idle - checking sword
$frame standC1 standC2 standC3 standC4 standC5 standC6 standC7 standC8 
$frame standC9 standC10 standC11 standC12 standC13 standC14 standC15 standC16
$frame standC17 standC18 standC19 standC20 standC21 standC22 standC23 standC24

// (073 - 095) Stand idle - poke sword at the ground
$frame standD1 standD2 standD3 standD4 standD5 standD6 standD7 standD8 
$frame standD9 standD10 standD11 standD12 standD13 standD14 standD15 standD16
$frame standD17 standD18 standD19 standD20 standD21 standD22 standD23

// (096 - 109) Default walk - typical hell knight stride
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9
$frame walk10 walk11 walk12 walk13 walk14

// (110 - 115) Default run
$frame run1 run2 run3 run4 run5 run6

// (116 - 130) Slice (one hand slashing run attack) (file=swing)
$frame slice1 slice2 slice3 slice4 slice5 slice6 slice7 slice8
$frame slice9 slice10 slice11 slice12 slice13 slice14 slice15

// (131 - 145) Smash (chop overhead to ground) (file=chop)
$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7 smash8
$frame smash9 smash10 smash11 smash12 smash13 smash14 smash15

// (146 - 167) W_Attack (two handed slashing attack) (file=slash)
$frame w_attack1 w_attack2 w_attack3 w_attack4 w_attack5 w_attack6 w_attack7 w_attack8
$frame w_attack9 w_attack10 w_attack11 w_attack12 w_attack13 w_attack14 w_attack15 w_attack16
$frame w_attack17 w_attack18 w_attack19 w_attack20 w_attack21 w_attack22 

// Pain animations A = Slow (168 - 172), D = Stagger (173 - 183)
$frame painA1 painA2 painA3 painA4 painA5
$frame painD1 painD2 painD3 painD4 painD5 painD6 painD7 painD8 painD9 painD10 painD11

// (184 - 195) Charging attack A - swinging sword left to right infront (file=char)
$frame char_a1 char_a2 char_a3 char_a4 char_a5 char_a6 char_a7 char_a8
$frame char_a9 char_a10 char_a11 char_a12

// (196 - 207) Death FORWARD
$frame deathA1 deathA2 deathA3 deathA4 deathA5 deathA6 deathA7 deathA8
$frame deathA9 deathA10 deathA11 deathA12

// (208 - 227) Death BACKWARD - with cool sword wobble
$frame deathB1 deathB2 deathB3 deathB4 deathB5 deathB6 deathB7 deathB8
$frame deathB9 deathB10 deathB11 deathB12 deathB13 deathB14 deathB15 deathB16
$frame deathB17 deathB18 deathB19 deathB20

// (228 - 241) Death FORWARD - more dramatic, platoon like
$frame deathC1 deathC2 deathC3 deathC4 deathC5 deathC6 deathC7 deathC8
$frame deathC9 deathC10 deathC11 deathC12 deathC13 deathC14

// (242 - 256) Death SIDEWAYS - Similiar to original knights
$frame deathD1 deathD2 deathD3 deathD4 deathD5 deathD6 deathD7 deathD8
$frame deathD9 deathD10 deathD11 deathD12 deathD13 deathD14 deathD15

void() dguard_sword_sound;

//===========================================================================
void()    dguard_standA1    =[    $standA1,    dguard_standA2    ] {
    self.idlebusy = TRUE; ai_stand();};
void()    dguard_standA2    =[    $standA2,    dguard_standA3    ] {ai_stand();};
void()    dguard_standA3    =[    $standA3,    dguard_standA4    ] {ai_stand();};
void()    dguard_standA4    =[    $standA4,    dguard_standA5    ] {ai_stand();};
void()    dguard_standA5    =[    $standA5,    dguard_standA6    ] {ai_stand();};
void()    dguard_standA6    =[    $standA6,    dguard_standA7    ] {ai_stand();};
void()    dguard_standA7    =[    $standA7,    dguard_standA8    ] {ai_stand();};
void()    dguard_standA8    =[    $standA8,    dguard_standA9    ] {ai_stand();};
void()    dguard_standA9    =[    $standA9,    dguard_standA10    ] {ai_stand();};
void()    dguard_standA10    =[    $standA10,    dguard_standA11    ] {ai_stand();};
void()    dguard_standA11    =[    $standA11,    dguard_standA12    ] {ai_stand();};
void()    dguard_standA12    =[    $standA12,    dguard_stand1    ] {ai_stand();};

//===========================================================================
void()    dguard_standB1    =[    $standB1,    dguard_standB2    ] {
    self.idlebusy = TRUE; ai_stand();};
void()    dguard_standB2    =[    $standB2,    dguard_standB3    ] {ai_stand();};
void()    dguard_standB3    =[    $standB3,    dguard_standB4    ] {ai_stand();};
void()    dguard_standB4    =[    $standB4,    dguard_standB5    ] {ai_stand();};
void()    dguard_standB5    =[    $standB5,    dguard_standB6    ] {ai_stand();};
void()    dguard_standB6    =[    $standB6,    dguard_standB7    ] {ai_stand();};
void()    dguard_standB7    =[    $standB7,    dguard_standB8    ] {ai_stand();};
void()    dguard_standB8    =[    $standB8,    dguard_standB9    ] {ai_stand();};
void()    dguard_standB9    =[    $standB9,    dguard_standB10    ] {ai_stand();};
void()    dguard_standB10    =[    $standB10,    dguard_standB11    ] {ai_stand();};
void()    dguard_standB11    =[    $standB11,    dguard_standB12    ] {ai_stand();};
void()    dguard_standB12    =[    $standB12,    dguard_standB13    ] {ai_stand();};
void()    dguard_standB13    =[    $standB13,    dguard_standB14    ] {ai_stand();};
void()    dguard_standB14    =[    $standB14,    dguard_standB15    ] {ai_stand();};
void()    dguard_standB15    =[    $standB15,    dguard_standB16    ] {ai_stand();};
void()    dguard_standB16    =[    $standB16,    dguard_standB17    ] {ai_stand();};
void()    dguard_standB17    =[    $standB17,    dguard_standB18    ] {ai_stand();};
void()    dguard_standB18    =[    $standB18,    dguard_standB19    ] {ai_stand();};
void()    dguard_standB19    =[    $standB19,    dguard_standB20    ] {ai_stand();};
void()    dguard_standB20    =[    $standB20,    dguard_standB21    ] {ai_stand();};
void()    dguard_standB21    =[    $standB21,    dguard_standB22    ] {ai_stand();};
void()    dguard_standB22    =[    $standB22,    dguard_standB23    ] {ai_stand();};
void()    dguard_standB23    =[    $standB23,    dguard_standB24    ] {ai_stand();};
void()    dguard_standB24    =[    $standB24,    dguard_stand1    ] {ai_stand();};

//===========================================================================
void()    dguard_standC1    =[    $standC1,    dguard_standC2    ] {
    self.idlebusy = TRUE; ai_stand();};
void()    dguard_standC2    =[    $standC2,    dguard_standC3    ] {ai_stand();};
void()    dguard_standC3    =[    $standC3,    dguard_standC4    ] {ai_stand();};
void()    dguard_standC4    =[    $standC4,    dguard_standC5    ] {ai_stand();};
void()    dguard_standC5    =[    $standC5,    dguard_standC6    ] {ai_stand();};
void()    dguard_standC6    =[    $standC6,    dguard_standC7    ] {ai_stand();};
void()    dguard_standC7    =[    $standC7,    dguard_standC8    ] {ai_stand();};
void()    dguard_standC8    =[    $standC8,    dguard_standC9    ] {ai_stand();};
void()    dguard_standC9    =[    $standC9,    dguard_standC10    ] {ai_stand();};
void()    dguard_standC10    =[    $standC10,    dguard_standC11    ] {ai_stand();};
void()    dguard_standC11    =[    $standC11,    dguard_standC12    ] {ai_stand();};
void()    dguard_standC12    =[    $standC12,    dguard_standC13    ] {ai_stand();};
void()    dguard_standC13    =[    $standC13,    dguard_standC14    ] {ai_stand();};
void()    dguard_standC14    =[    $standC14,    dguard_standC15    ] {ai_stand();};
void()    dguard_standC15    =[    $standC15,    dguard_standC16    ] {ai_stand();};
void()    dguard_standC16    =[    $standC16,    dguard_standC17    ] {ai_stand();};
void()    dguard_standC17    =[    $standC17,    dguard_standC18    ] {ai_stand();};
void()    dguard_standC18    =[    $standC18,    dguard_standC19    ] {ai_stand();};
void()    dguard_standC19    =[    $standC19,    dguard_standC20    ] {ai_stand();};
void()    dguard_standC20    =[    $standC20,    dguard_standC21    ] {ai_stand();};
void()    dguard_standC21    =[    $standC21,    dguard_standC22    ] {ai_stand();};
void()    dguard_standC22    =[    $standC22,    dguard_standC23    ] {ai_stand();};
void()    dguard_standC23    =[    $standC23,    dguard_standC24    ] {ai_stand();};
void()    dguard_standC24    =[    $standC24,    dguard_stand1    ] {ai_stand();};

//===========================================================================
void()    dguard_standD1    =[    $standD1,    dguard_standD2    ] {
    self.idlebusy = TRUE; ai_stand();};
void()    dguard_standD2    =[    $standD2,    dguard_standD3    ] {ai_stand();};
void()    dguard_standD3    =[    $standD3,    dguard_standD4    ] {ai_stand();};
void()    dguard_standD4    =[    $standD4,    dguard_standD5    ] {ai_stand();};
void()    dguard_standD5    =[    $standD5,    dguard_standD6    ] {ai_stand();};
void()    dguard_standD6    =[    $standD6,    dguard_standD7    ] {ai_stand();};
void()    dguard_standD7    =[    $standD7,    dguard_standD8    ] {ai_stand();};
void()    dguard_standD8    =[    $standD8,    dguard_standD9    ] {ai_stand();};
void()    dguard_standD9    =[    $standD9,    dguard_standD10    ] {ai_stand();};
void()    dguard_standD10    =[    $standD10,    dguard_standD11    ] {ai_stand();};
void()    dguard_standD11    =[    $standD11,    dguard_standD12    ] {ai_stand();};
void()    dguard_standD12    =[    $standD12,    dguard_standD13    ] {ai_stand();};
void()    dguard_standD13    =[    $standD13,    dguard_standD14    ] {ai_stand();};
void()    dguard_standD14    =[    $standD14,    dguard_standD15    ] {ai_stand();};
void()    dguard_standD15    =[    $standD15,    dguard_standD16    ] {ai_stand();};
void()    dguard_standD16    =[    $standD16,    dguard_standD17    ] {ai_stand();};
void()    dguard_standD17    =[    $standD17,    dguard_standD18    ] {ai_stand();};
void()    dguard_standD18    =[    $standD18,    dguard_standD19    ] {ai_stand();};
void()    dguard_standD19    =[    $standD19,    dguard_standD20    ] {ai_stand();};
void()    dguard_standD20    =[    $standD20,    dguard_standD21    ] {ai_stand();};
void()    dguard_standD21    =[    $standD21,    dguard_standD22    ] {ai_stand();};
void()    dguard_standD22    =[    $standD22,    dguard_standD23    ] {ai_stand();};
void()    dguard_standD23    =[    $standD23,    dguard_stand1    ] {ai_stand();};

//===========================================================================
void()    dguard_stand1    =[    $stand1,    dguard_stand2    ] {monster_idle_sound(); 
    self.idlebusy = FALSE; ai_stand();};
void()    dguard_stand2    =[    $stand2,    dguard_stand3    ] {ai_stand();};
void()    dguard_stand3    =[    $stand3,    dguard_stand4    ] {ai_stand();};
void()    dguard_stand4    =[    $stand4,    dguard_stand5    ] {ai_stand();};
void()    dguard_stand5    =[    $stand5,    dguard_stand6    ] {ai_stand();};
void()    dguard_stand6    =[    $stand6,    dguard_stand7    ] {ai_stand();};
void()    dguard_stand7    =[    $stand7,    dguard_stand8    ] {ai_stand();};
void()    dguard_stand8    =[    $stand8,    dguard_stand9    ] {ai_stand();};
void()    dguard_stand9    =[    $stand9,    dguard_stand10    ] {ai_stand();};
void()    dguard_stand10    =[    $stand10,    dguard_stand11    ] {ai_stand();};
void()    dguard_stand11    =[    $stand11,    dguard_stand12    ] {ai_stand();};
void()    dguard_stand12    =[    $stand12,    dguard_stand1    ] {
    if (random() < MON_IDLE_ANIMATION) {
        self.lip = random();
        if (self.lip < 0.2) self.think = dguard_standD1;            // Poke the ground
        else if (self.lip < 0.4) self.think = dguard_standC1;    // Look at weapon
        else if (self.lip < 0.7) self.think = dguard_standA1;    // Shoulder hunch
        else self.think = dguard_standB1;                                            // Look around
    }
    ai_stand();
};

//===========================================================================
void()    dguard_walk1    =[    $walk1,        dguard_walk2    ] { monster_footstep(FALSE);
self.idlebusy = FALSE; monster_idle_sound(); ai_walk(2);};
void()    dguard_walk2    =[    $walk2,        dguard_walk3    ] {ai_walk(5);};
void()    dguard_walk3    =[    $walk3,        dguard_walk4    ] {ai_walk(5);};
void()    dguard_walk4    =[    $walk4,        dguard_walk5    ] {ai_walk(4);};
void()    dguard_walk5    =[    $walk5,        dguard_walk6    ] {ai_walk(4);};
void()    dguard_walk6    =[    $walk6,        dguard_walk7    ] {ai_walk(2);};
void()    dguard_walk7    =[    $walk7,        dguard_walk8    ] {ai_walk(2);};
void()    dguard_walk8    =[    $walk8,        dguard_walk9    ] {ai_walk(3); monster_footstep(FALSE);};
void()    dguard_walk9    =[    $walk9,        dguard_walk10    ] {ai_walk(3);};
void()    dguard_walk10    =[    $walk10,    dguard_walk11    ] {ai_walk(4);};
void()    dguard_walk11    =[    $walk11,    dguard_walk12    ] {ai_walk(3);};
void()    dguard_walk12    =[    $walk12,    dguard_walk13    ] {ai_walk(4);};
void()    dguard_walk13    =[    $walk13,    dguard_walk14    ] {ai_walk(6);};
void()    dguard_walk14    =[    $walk14,    dguard_walk1    ] {ai_walk(2);};

//===========================================================================
// 6 frames instead of 8 frames (16, 20, 7, 16, 20, 6) Missing 13+14
// Added timing of 3rd/7th back to other frames

void()    dguard_run1    =[    $run1,    dguard_run2    ] {
    self.idlebusy = FALSE; monster_idle_sound(); ai_run (20); };
void()    dguard_run2    =[    $run2,    dguard_run3    ] {monster_footstep(FALSE); ai_run(24);};
void()    dguard_run3    =[    $run3,    dguard_run4    ] {ai_run(12);};
void()    dguard_run4    =[    $run4,    dguard_run5    ] {ai_run(20);};
void()    dguard_run5    =[    $run5,    dguard_run6    ] {monster_footstep(FALSE); ai_run(24);};
void()    dguard_run6    =[    $run6,    dguard_run1    ] {ai_run(12);};

//===========================================================================
// Melee Attack 1a - Slice (one hand slashing run attack)
// Knight - 11 frames (skipped 1, 6, 12, 15)

void()    dguard_runatk1    =[    $slice2,    dguard_runatk2    ] {ai_charge(20);};
void()    dguard_runatk2    =[    $slice3,    dguard_runatk3    ] {ai_charge_side();};
void()    dguard_runatk3    =[    $slice4,    dguard_runatk4    ] {monster_footstep(FALSE); ai_charge_side();};
void()    dguard_runatk4    =[    $slice5,    dguard_runatk5    ] {ai_charge_side(); dguard_sword_sound();};
void()    dguard_runatk5    =[    $slice7,    dguard_runatk6    ] {monster_footstep(FALSE); ai_melee_side();};
void()    dguard_runatk6    =[    $slice8,    dguard_runatk7    ] {ai_melee_side();};
void()    dguard_runatk7    =[    $slice9,    dguard_runatk8    ] {ai_melee_side();};
void()    dguard_runatk8    =[    $slice10,    dguard_runatk9    ] {monster_footstep(FALSE); ai_melee_side();};
void()    dguard_runatk9    =[    $slice11,    dguard_runatk10    ] {ai_melee_side();};
void()    dguard_runatk10    =[    $slice13,    dguard_runatk11    ] {monster_footstep(FALSE); ai_charge_side();};
void()    dguard_runatk11    =[    $slice14,    dguard_run1    ] {ai_charge(10);};

//===========================================================================
// Melee Attack 1b - Charging attack A - swinging sword left to right infront
// Knight - 11 frames (skipped 1)

void()    dguard_runatkB1    =[    $char_a2,        dguard_runatkB2    ] {monster_footstep(FALSE); ai_charge(20); dguard_sword_sound();};
void()    dguard_runatkB2    =[    $char_a3,    dguard_runatkB3    ] {ai_charge_side();};
void()    dguard_runatkB3    =[    $char_a4,    dguard_runatkB4    ] {ai_charge_side();};
void()    dguard_runatkB4    =[    $char_a5,    dguard_runatkB5    ] {monster_footstep(FALSE); ai_charge_side();};
void()    dguard_runatkB5    =[    $char_a6,    dguard_runatkB6    ] {ai_melee_side();};
void()    dguard_runatkB6    =[    $char_a7,    dguard_runatkB7    ] {ai_melee_side(); dguard_sword_sound();};
void()    dguard_runatkB7    =[    $char_a8,    dguard_runatkB8    ] {monster_footstep(FALSE); ai_melee_side();};
void()    dguard_runatkB8    =[    $char_a9,    dguard_runatkB9    ] {ai_melee_side();};
void()    dguard_runatkB9    =[    $char_a10,    dguard_runatkB10    ] {ai_melee_side();};
void()    dguard_runatkB10=[    $char_a11,    dguard_runatkB11    ] {monster_footstep(FALSE); ai_melee_side();};
void()    dguard_runatkB11=[    $char_a12,    dguard_run1    ] {ai_charge(10);};

//===========================================================================
// Melee Attack 2a - Smash (chop overhead to ground)
// Knight - 11 frames (skipped (1, 11, 14, 15)

void()    dguard_atk1    =[    $smash2,        dguard_atk2    ] {ai_charge(0);};
void()    dguard_atk2    =[    $smash3,        dguard_atk3    ] {ai_charge(7);};
void()    dguard_atk3    =[    $smash4,        dguard_atk4    ] {monster_footstep(FALSE); ai_charge(4);};
void()    dguard_atk4    =[    $smash5,        dguard_atk5    ] {ai_charge(0);};
void()    dguard_atk5    =[    $smash6,        dguard_atk6    ] {ai_charge(3);};
void()    dguard_atk6    =[    $smash7,        dguard_atk7    ] {ai_charge(4); ai_melee(); dguard_sword_sound();};
void()    dguard_atk7    =[    $smash8,        dguard_atk8    ] {monster_footstep(FALSE); ai_charge(1); ai_melee();};
void()    dguard_atk8    =[    $smash9,        dguard_atk9    ] {ai_charge(3); ai_melee();};
void()    dguard_atk9    =[    $smash10,        dguard_atk10] {ai_charge(1);};
void()    dguard_atk10=[    $smash12,        dguard_atk11] {ai_charge(1);};
void()    dguard_atk11=[    $smash13,        dguard_run1    ] {ai_charge(5);};

//============================================================================
// Melee Attack 2b - W_Attack (two handed slashing attack)

void()    dguard_atkB1    =[    $w_attack2,        dguard_atkB2    ] {monster_footstep(FALSE); ai_charge(0);};
void()    dguard_atkB2    =[    $w_attack3,        dguard_atkB3    ] {ai_charge(7);};
void()    dguard_atkB3    =[    $w_attack4,        dguard_atkB4    ] {ai_charge(4);};
void()    dguard_atkB4    =[    $w_attack5,        dguard_atkB5    ] {ai_charge(0);};
void()    dguard_atkB5    =[    $w_attack6,        dguard_atkB6    ] {ai_charge(3); dguard_sword_sound();};
void()    dguard_atkB6    =[    $w_attack7,        dguard_atkB7    ] {monster_footstep(FALSE); ai_charge(4); ai_melee();};
void()    dguard_atkB7    =[    $w_attack8,        dguard_atkB8    ] {ai_charge(1); ai_melee();};
void()    dguard_atkB8    =[    $w_attack9,        dguard_atkB9    ] {ai_charge(3); ai_melee();};
void()    dguard_atkB9    =[    $w_attack10,    dguard_atkB10] {ai_charge(1);};
void()    dguard_atkB10=    [    $w_attack11,    dguard_atkB11] {ai_charge(1);};
void()    dguard_atkB11    =[    $w_attack12,    dguard_atkB12    ] {ai_charge(0);};
void()    dguard_atkB12    =[    $w_attack13,    dguard_atkB13    ] {ai_charge(7);};
void()    dguard_atkB13    =[    $w_attack14,    dguard_atkB14    ] {ai_charge(4);};
void()    dguard_atkB14    =[    $w_attack15,    dguard_atkB15    ] {ai_charge(0); dguard_sword_sound();};
void()    dguard_atkB15    =[    $w_attack16,    dguard_atkB16    ] {ai_charge(3);};
void()    dguard_atkB16    =[    $w_attack17,    dguard_atkB17    ] {monster_footstep(FALSE); ai_charge(4); ai_melee();};
void()    dguard_atkB17    =[    $w_attack18,    dguard_atkB18    ] {ai_charge(1); ai_melee();};
void()    dguard_atkB18    =[    $w_attack19,    dguard_atkB19    ] {ai_charge(3); ai_melee();};
void()    dguard_atkB19    =[    $w_attack20,    dguard_atkB20] {ai_charge(1);};
void()    dguard_atkB20=[    $w_attack21,        dguard_atkB21] {ai_charge(1);};
void()    dguard_atkB21=[    $w_attack22,        dguard_run1    ] {ai_charge(5);};

//============================================================================
// Cycle between melee attacks
// Attack 1a - Slice (one hand slashing run attack)
// Attack 1b - Char_A (swinging sword left to right infront)
// Attack 2a - Smash (chop overhead to ground)
// Attack 2b - W_Attack (two handed slashing attack)

void() dguard_melee =
{
    local float        len;

    // decide if now is a good swing time
    len = vlen(self.enemy.origin + self.enemy.view_ofs - (self.origin + self.view_ofs));
    
    // Close melee attack 1a, 1b (slice and char_a)
    if ( len < MONAI_MELEEKNIGHT ) {
        // Randomly pick between attacks
        if (random() > 0.5) dguard_atk1();
        else dguard_atkB1();
    }
    // Running attack 2a, 2b (smash, w_attack (2x attacks in one))
    else {
        if (random() > 0.5) dguard_runatk1();
        else dguard_runatkB1();
    }
};

//============================================================================
void()    dguard_painA1    =[    $painA1,        dguard_painA2    ] {};
void()    dguard_painA2    =[    $painA2,        dguard_painA3    ] {};
void()    dguard_painA3    =[    $painA3,        dguard_painA4    ] {};
void()    dguard_painA4    =[    $painA4,        dguard_painA5    ] {};
void()    dguard_painA5    =[    $painA5,        dguard_run1    ] {};
//----------------------------------------------------------------------
void()    dguard_painD1    =[    $painD1,        dguard_painD2    ] {};
void()    dguard_painD2    =[    $painD2,        dguard_painD3    ] {};
void()    dguard_painD3    =[    $painD3,        dguard_painD4    ] {};
void()    dguard_painD4    =[    $painD4,        dguard_painD5    ] {monster_footstep(FALSE);};
void()    dguard_painD5    =[    $painD5,        dguard_painD6    ] {};
void()    dguard_painD6    =[    $painD6,        dguard_painD7    ] {};
void()    dguard_painD7    =[    $painD7,        dguard_painD8    ] {};
void()    dguard_painD8    =[    $painD8,        dguard_painD9    ] {monster_footstep(FALSE);};
void()    dguard_painD9    =[    $painD9,        dguard_painD10    ] {};
void()    dguard_painD10=[    $painD10,        dguard_painD11    ] {};
void()    dguard_painD11=[    $painD11,        dguard_run1    ] {};
//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage)    dguard_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1 || self.pain_check == 2) {
            // Randomly pick which pain animation to play
            if (random() < 0.85) dguard_painA1 ();        // classic, body recoil
            else {
                dguard_painD1 ();                                            // Cool stumble, hand over mouth
                self.pain_finished = time + 2;                // long animation
            }
        }
    }
};

//============================================================================
void()    dguard_dieA1    =[    $deathA1,    dguard_dieA2    ] {ai_forward(5);};
void()    dguard_dieA2    =[    $deathA2,    dguard_dieA3    ] {monster_check_gib();ai_forward(8);};
void()    dguard_dieA3    =[    $deathA3,    dguard_dieA4    ] {monster_check_gib();
    self.solid = SOLID_NOT; ai_forward(7);};
void()    dguard_dieA4    =[    $deathA4,    dguard_dieA5    ] {};
void()    dguard_dieA5    =[    $deathA5,    dguard_dieA6    ] {};
void()    dguard_dieA6    =[    $deathA6,    dguard_dieA7    ] {
    sound (self, CHAN_WEAPON, GIB_SOUND_METALA, 1, ATTN_NORM);};
void()    dguard_dieA7    =[    $deathA7,    dguard_dieA8    ] {};
void()    dguard_dieA8    =[    $deathA8,    dguard_dieA9    ] {ai_forward(10);};
void()    dguard_dieA9    =[    $deathA9,    dguard_dieA10    ] {ai_forward(11);};
void()    dguard_dieA10    =[    $deathA10,    dguard_dieA11    ] {};
void()    dguard_dieA11    =[    $deathA11,    dguard_dieA12    ] {monster_death_postcheck();};
void()    dguard_dieA12    =[    $deathA12,    dguard_dieA12    ] {monster_deadbody_check();};
//----------------------------------------------------------------------
void()    dguard_dieB1    =[    $deathB1,    dguard_dieB2    ] {};
void()    dguard_dieB2    =[    $deathB2,    dguard_dieB3    ] {monster_check_gib();};
void()    dguard_dieB3    =[    $deathB3,    dguard_dieB4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void()    dguard_dieB4    =[    $deathB4,    dguard_dieB5    ] {};
void()    dguard_dieB5    =[    $deathB5,    dguard_dieB6    ] {};
void()    dguard_dieB6    =[    $deathB6,    dguard_dieB7    ] {};
void()    dguard_dieB7    =[    $deathB7,    dguard_dieB8    ] {};
void()    dguard_dieB8    =[    $deathB8,    dguard_dieB9    ] {};
void()    dguard_dieB9    =[    $deathB9,    dguard_dieB10    ] {
    sound (self, CHAN_WEAPON, GIB_SOUND_METALB, 1, ATTN_NORM);};
void()    dguard_dieB10    =[    $deathB10,    dguard_dieB11    ] {};
void()    dguard_dieB11    =[    $deathB11,    dguard_dieB12    ] {};
void()    dguard_dieB12    =[    $deathB12,    dguard_dieB13    ] {};
void()    dguard_dieB13    =[    $deathB13,    dguard_dieB14    ] {};
void()    dguard_dieB14    =[    $deathB14,    dguard_dieB15    ] {};
void()    dguard_dieB15    =[    $deathB15,    dguard_dieB16    ] {};
void()    dguard_dieB16    =[    $deathB16,    dguard_dieB17    ] {};
void()    dguard_dieB17    =[    $deathB17,    dguard_dieB18    ] {};
void()    dguard_dieB18    =[    $deathB18,    dguard_dieB19    ] {};
void()    dguard_dieB19    =[    $deathB19,    dguard_dieB20    ] {monster_death_postcheck();};
void()    dguard_dieB20    =[    $deathB20,    dguard_dieB20    ] {monster_deadbody_check();};
//----------------------------------------------------------------------
void()    dguard_dieC1    =[    $deathC1,    dguard_dieC2    ] {ai_forward(5);};
void()    dguard_dieC2    =[    $deathC2,    dguard_dieC3    ] {monster_check_gib();ai_forward(8);};
void()    dguard_dieC3    =[    $deathC3,    dguard_dieC4    ] {monster_check_gib();
    self.solid = SOLID_NOT; ai_forward(7);};
void()    dguard_dieC4    =[    $deathC4,    dguard_dieC5    ] {};
void()    dguard_dieC5    =[    $deathC5,    dguard_dieC6    ] {
    sound (self, CHAN_WEAPON, GIB_SOUND_METALA, 1, ATTN_NORM);};
void()    dguard_dieC6    =[    $deathC6,    dguard_dieC7    ] {};
void()    dguard_dieC7    =[    $deathC7,    dguard_dieC8    ] {};
void()    dguard_dieC8    =[    $deathC8,    dguard_dieC9    ] {ai_forward(10);};
void()    dguard_dieC9    =[    $deathC9,    dguard_dieC10    ] {ai_forward(11);};
void()    dguard_dieC10    =[    $deathC10,    dguard_dieC11    ] {};
void()    dguard_dieC11    =[    $deathC11,    dguard_dieC12    ] {};
void()    dguard_dieC12    =[    $deathC12,    dguard_dieC13    ] {};
void()    dguard_dieC13    =[    $deathC13,    dguard_dieC14    ] {monster_death_postcheck();};
void()    dguard_dieC14    =[    $deathC14,    dguard_dieC14    ] {monster_deadbody_check();};
//----------------------------------------------------------------------
// The sideways death happens to quick and looks odd when finished
// Skipping this death sequence
void()    dguard_dieD1    =[    $deathD1,    dguard_dieD2    ] {};
void()    dguard_dieD2    =[    $deathD2,    dguard_dieD3    ] {monster_check_gib();};
void()    dguard_dieD3    =[    $deathD3,    dguard_dieD4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void()    dguard_dieD4    =[    $deathD4,    dguard_dieD5    ] {};
void()    dguard_dieD5    =[    $deathD5,    dguard_dieD6    ] {};
void()    dguard_dieD6    =[    $deathD6,    dguard_dieD7    ] {};
void()    dguard_dieD7    =[    $deathD7,    dguard_dieD8    ] {};
void()    dguard_dieD8    =[    $deathD8,    dguard_dieD9    ] {
    sound (self, CHAN_WEAPON, GIB_SOUND_METALB, 1, ATTN_NORM);};
void()    dguard_dieD9    =[    $deathD9,    dguard_dieD10    ] {};
void()    dguard_dieD10    =[    $deathD10,    dguard_dieD11    ] {};
void()    dguard_dieD11    =[    $deathD11,    dguard_dieD12    ] {};
void()    dguard_dieD12    =[    $deathD12,    dguard_dieD13    ] {};
void()    dguard_dieD13    =[    $deathD13,    dguard_dieD14    ] {};
void()    dguard_dieD14    =[    $deathD14,    dguard_dieD15    ] {monster_death_postcheck();};
void()    dguard_dieD15    =[    $deathD15,    dguard_dieD15 ] {monster_deadbody_check();};
//----------------------------------------------------------------------
void() dguard_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "knight/kdeath.wav", 1, ATTN_NORM);
        if (random() > 0.75) dguard_dieC1 ();                // Forward - platoon like
        else if (random() > 0.5) dguard_dieA1 ();        // Forward - standard
        else dguard_dieB1 ();                                                // Backward - standard
    }
};

//============================================================================
void() dguard_sword_sound =
{
    self.lip = random();
    if (self.lip < 0.25) sound (self, CHAN_WEAPON, "weapons/sword1a.wav", 1, ATTN_NORM);
    else if (self.lip < 0.5) sound (self, CHAN_WEAPON, "weapons/sword2a.wav", 1, ATTN_NORM);
    else if (self.lip < 0.75) sound (self, CHAN_WEAPON, "weapons/sword1b.wav", 1, ATTN_NORM);
    else sound (self, CHAN_WEAPON, "weapons/sword2b.wav", 1, ATTN_NORM);
};

/*======================================================================
QUAKED monster_dguard (1 0 0) (-16 -16 -24) (16 16 40) Ambush
======================================================================*/
void() monster_dguard =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_dguard.mdl";
    self.headmdl = "progs/h_dguard.mdl";                // Large head
    self.gib1mdl = "progs/w_dguardsword.mdl";        // Unique sword
    self.gib2mdl = "progs/gib_knfoot_l.mdl";        // left foot
    self.gib3mdl = "progs/gib_knfoot_r.mdl";        // right foot

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    
    self.gib1sound = GIB_IMPACT_METALA;
    if (random() < 0.5) self.gib2mdl = "";
    if (random() < 0.5) self.gib3mdl = "";

    self.idle_sound = "knight/idle.wav";
    precache_sound (self.idle_sound);

    precache_sound ("knight/kdeath.wav");
    self.pain_sound = "knight/khurt.wav";
    precache_sound (self.pain_sound);

    // New sword sweep/slash sounds
    precache_sound ("weapons/sword1a.wav");
    precache_sound ("weapons/sword1b.wav");
    precache_sound ("weapons/sword2a.wav");
    precache_sound ("weapons/sword2b.wav");
    
    self.sight_sound = "knight/ksight.wav";
    precache_sound (self.sight_sound);

    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
    }
    
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_SHORT;
    if (self.health < 1) self.health = 30;
    self.gibhealth = -35;
    self.gibbed = FALSE;
    self.pain_flinch = 20;
    self.steptype = FS_TYPEMEDIUM;
    self.deathstring = " was chopped up by a Death Guard\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    self.th_stand = dguard_stand1;
    self.th_walk = dguard_walk1;
    self.th_run = dguard_run1;
    self.th_melee = dguard_melee;
    self.th_pain = dguard_pain;
    self.th_die = dguard_die;
    
    self.classtype = CT_MONDGUARD;
    self.classgroup = CG_KNIGHT;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};
