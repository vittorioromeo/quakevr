/*==============================================================================
DEATH SERGEANT (hell knights ++)
==============================================================================*/
$cd id1/models/dsergeant
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

// (049 - 072) Stand idle - checking flail
$frame standC1 standC2 standC3 standC4 standC5 standC6 standC7 standC8 
$frame standC9 standC10 standC11 standC12 standC13 standC14 standC15 standC16
$frame standC17 standC18 standC19 standC20 standC21 standC22 standC23 standC24

// (073 - 103) Stand idle - big sigh
$frame standD1 standD2 standD3 standD4 standD5 standD6 standD7 standD8 
$frame standD9 standD10 standD11 standD12 standD13 standD14 standD15 standD16
$frame standD17 standD18 standD19 standD20 standD21 standD22 standD23 standD24
$frame  standD25 standD26 standD27 standD28 standD29 standD30 standD31

// (104 - 117) Default walk - typical hell knight stride
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9
$frame walk10 walk11 walk12 walk13 walk14

// (118 - 123) Default run
$frame run1 run2 run3 run4 run5 run6

// (124 - 138) Slice (one hand slashing run attack) (file=SWING)
$frame slice1 slice2 slice3 slice4 slice5 slice6 slice7 slice8
$frame slice9 slice10 slice11 slice12 slice13 slice14 slice15

// (139 - 153) Smash (chop overhead to ground) (file=CHOP)
$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7 smash8
$frame smash9 smash10 smash11 smash12 smash13 smash14 smash15

// (154 - 175) W_Attack (two handed slashing attack) (file=SLASH)
$frame w_attack1 w_attack2 w_attack3 w_attack4 w_attack5 w_attack6 w_attack7 w_attack8
$frame w_attack9 w_attack10 w_attack11 w_attack12 w_attack13 w_attack14 w_attack15 w_attack16
$frame w_attack17 w_attack18 w_attack19 w_attack20 w_attack21 w_attack22 

// (176 - 189) MagicC - Typical magic range attack (file=SWEEPCAST)
$frame magicc1 magicc2 magicc3 magicc4 magicc5 magicc6 magicc7 magicc8
$frame magicc9 magicc10 magicc11 magicc12 magicc13 magicc14

// Pain animations A = Slow (190 - 194), D = Stagger (195 - 205)
$frame painA1 painA2 painA3 painA4 painA5
$frame painD1 painD2 painD3 painD4 painD5 painD6 painD7 painD8 painD9 painD10 painD11

// (206 - 217) Charging attack A - swinging sword left to right infront
$frame char_a1 char_a2 char_a3 char_a4 char_a5 char_a6 char_a7 char_a8
$frame char_a9 char_a10 char_a11 char_a12

// (218 - 223) Charging attack B - rotating sword left to right infront
$frame char_b1 char_b2 char_b3 char_b4 char_b5 char_b6

// (224 - 235) Death forward
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12

// (236 - 255) Death backwards with cool sword wobble
$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9 deathb10 deathb11 deathb12 deathb13 deathb14 deathb15 deathb16
$frame deathb17 deathb18 deathb19 deathb20

void() dsergeant_sword_sound;        // Sword swipe sounds
void(float soundtype) dsergeant_grunt_sound;

// Different types of grunting sounds
float DSERG_SOUND_SIDESWING = 1;
float DSERG_SOUND_OVERSMASH = 2;
float DSERG_SOUND_RUNSWING = 3;

//===========================================================================

void()    dsergeant_standA1    =[    $standA1,    dsergeant_standA2    ] {ai_stand();};
void()    dsergeant_standA2    =[    $standA2,    dsergeant_standA3    ] {ai_stand();};
void()    dsergeant_standA3    =[    $standA3,    dsergeant_standA4    ] {ai_stand();};
void()    dsergeant_standA4    =[    $standA4,    dsergeant_standA5    ] {ai_stand();};
void()    dsergeant_standA5    =[    $standA5,    dsergeant_standA6    ] {ai_stand();};
void()    dsergeant_standA6    =[    $standA6,    dsergeant_standA7    ] {ai_stand();};
void()    dsergeant_standA7    =[    $standA7,    dsergeant_standA8    ] {ai_stand();};
void()    dsergeant_standA8    =[    $standA8,    dsergeant_standA9    ] {ai_stand();};
void()    dsergeant_standA9    =[    $standA9,    dsergeant_standA10    ] {ai_stand();};
void()    dsergeant_standA10    =[    $standA10,    dsergeant_standA11    ] {ai_stand();};
void()    dsergeant_standA11    =[    $standA11,    dsergeant_standA12    ] {ai_stand();};
void()    dsergeant_standA12    =[    $standA12,    dsergeant_stand1    ] {ai_stand();};

//===========================================================================
void()    dsergeant_standB1    =[    $standB1,    dsergeant_standB2    ] {ai_stand();};
void()    dsergeant_standB2    =[    $standB2,    dsergeant_standB3    ] {ai_stand();};
void()    dsergeant_standB3    =[    $standB3,    dsergeant_standB4    ] {ai_stand();};
void()    dsergeant_standB4    =[    $standB4,    dsergeant_standB5    ] {ai_stand();};
void()    dsergeant_standB5    =[    $standB5,    dsergeant_standB6    ] {ai_stand();};
void()    dsergeant_standB6    =[    $standB6,    dsergeant_standB7    ] {ai_stand();};
void()    dsergeant_standB7    =[    $standB7,    dsergeant_standB8    ] {ai_stand();};
void()    dsergeant_standB8    =[    $standB8,    dsergeant_standB9    ] {ai_stand();};
void()    dsergeant_standB9    =[    $standB9,    dsergeant_standB10    ] {ai_stand();};
void()    dsergeant_standB10    =[    $standB10,    dsergeant_standB11    ] {ai_stand();};
void()    dsergeant_standB11    =[    $standB11,    dsergeant_standB12    ] {ai_stand();};
void()    dsergeant_standB12    =[    $standB12,    dsergeant_standB13    ] {ai_stand();};
void()    dsergeant_standB13    =[    $standB13,    dsergeant_standB14    ] {ai_stand();};
void()    dsergeant_standB14    =[    $standB14,    dsergeant_standB15    ] {ai_stand();};
void()    dsergeant_standB15    =[    $standB15,    dsergeant_standB16    ] {ai_stand();};
void()    dsergeant_standB16    =[    $standB16,    dsergeant_standB17    ] {ai_stand();};
void()    dsergeant_standB17    =[    $standB17,    dsergeant_standB18    ] {ai_stand();};
void()    dsergeant_standB18    =[    $standB18,    dsergeant_standB19    ] {ai_stand();};
void()    dsergeant_standB19    =[    $standB19,    dsergeant_standB20    ] {ai_stand();};
void()    dsergeant_standB20    =[    $standB20,    dsergeant_standB21    ] {ai_stand();};
void()    dsergeant_standB21    =[    $standB21,    dsergeant_standB22    ] {ai_stand();};
void()    dsergeant_standB22    =[    $standB22,    dsergeant_standB23    ] {ai_stand();};
void()    dsergeant_standB23    =[    $standB23,    dsergeant_standB24    ] {ai_stand();};
void()    dsergeant_standB24    =[    $standB24,    dsergeant_stand1    ] {ai_stand();};

//===========================================================================
void()    dsergeant_standC1    =[    $standC1,    dsergeant_standC2    ] {ai_stand();};
void()    dsergeant_standC2    =[    $standC2,    dsergeant_standC3    ] {ai_stand();};
void()    dsergeant_standC3    =[    $standC3,    dsergeant_standC4    ] {ai_stand();};
void()    dsergeant_standC4    =[    $standC4,    dsergeant_standC5    ] {ai_stand();};
void()    dsergeant_standC5    =[    $standC5,    dsergeant_standC6    ] {ai_stand();};
void()    dsergeant_standC6    =[    $standC6,    dsergeant_standC7    ] {ai_stand();};
void()    dsergeant_standC7    =[    $standC7,    dsergeant_standC8    ] {ai_stand();};
void()    dsergeant_standC8    =[    $standC8,    dsergeant_standC9    ] {ai_stand();};
void()    dsergeant_standC9    =[    $standC9,    dsergeant_standC10    ] {ai_stand();};
void()    dsergeant_standC10    =[    $standC10,    dsergeant_standC11    ] {ai_stand();};
void()    dsergeant_standC11    =[    $standC11,    dsergeant_standC12    ] {ai_stand();};
void()    dsergeant_standC12    =[    $standC12,    dsergeant_standC13    ] {ai_stand();};
void()    dsergeant_standC13    =[    $standC13,    dsergeant_standC14    ] {ai_stand();};
void()    dsergeant_standC14    =[    $standC14,    dsergeant_standC15    ] {ai_stand();};
void()    dsergeant_standC15    =[    $standC15,    dsergeant_standC16    ] {ai_stand();};
void()    dsergeant_standC16    =[    $standC16,    dsergeant_standC17    ] {ai_stand();};
void()    dsergeant_standC17    =[    $standC17,    dsergeant_standC18    ] {ai_stand();};
void()    dsergeant_standC18    =[    $standC18,    dsergeant_standC19    ] {ai_stand();};
void()    dsergeant_standC19    =[    $standC19,    dsergeant_standC20    ] {ai_stand();};
void()    dsergeant_standC20    =[    $standC20,    dsergeant_standC21    ] {ai_stand();};
void()    dsergeant_standC21    =[    $standC21,    dsergeant_standC22    ] {ai_stand();};
void()    dsergeant_standC22    =[    $standC22,    dsergeant_standC23    ] {ai_stand();};
void()    dsergeant_standC23    =[    $standC23,    dsergeant_standC24    ] {ai_stand();};
void()    dsergeant_standC24    =[    $standC24,    dsergeant_stand1    ] {ai_stand();};

//===========================================================================
// skipped 7 frames to reduce the animation down to 2.4s (10fps)
void()    dsergeant_standD1    =[    $standD1,    dsergeant_standD2    ] {ai_stand();
    sound (self, CHAN_VOICE, "dknight/dserg_hbreath.wav", 1,  ATTN_IDLE);};
void()    dsergeant_standD2    =[    $standD2,    dsergeant_standD3    ] {ai_stand();};
void()    dsergeant_standD3    =[    $standD4,    dsergeant_standD4    ] {ai_stand();};
void()    dsergeant_standD4    =[    $standD6,    dsergeant_standD5    ] {ai_stand();};
void()    dsergeant_standD5    =[    $standD8,    dsergeant_standD6    ] {ai_stand();};
void()    dsergeant_standD6    =[    $standD9,    dsergeant_standD7    ] {ai_stand();};
void()    dsergeant_standD7    =[    $standD10,    dsergeant_standD8    ] {ai_stand();};
void()    dsergeant_standD8    =[    $standD11,    dsergeant_standD9    ] {ai_stand();};
void()    dsergeant_standD9    =[    $standD12,    dsergeant_standD10    ] {ai_stand();};
void()    dsergeant_standD10    =[    $standD13,    dsergeant_standD11    ] {ai_stand();};
void()    dsergeant_standD11    =[    $standD14,    dsergeant_standD12    ] {ai_stand();};
void()    dsergeant_standD12    =[    $standD15,    dsergeant_standD13    ] {ai_stand();};
void()    dsergeant_standD13    =[    $standD16,    dsergeant_standD14    ] {ai_stand();};
void()    dsergeant_standD14    =[    $standD17,    dsergeant_standD15    ] {ai_stand();};
void()    dsergeant_standD15    =[    $standD19,    dsergeant_standD16    ] {ai_stand();};
void()    dsergeant_standD16    =[    $standD21,    dsergeant_standD17    ] {ai_stand();};
void()    dsergeant_standD17    =[    $standD23,    dsergeant_standD18    ] {ai_stand();};
void()    dsergeant_standD18    =[    $standD24,    dsergeant_standD19    ] {ai_stand();};
void()    dsergeant_standD19    =[    $standD25,    dsergeant_standD20    ] {ai_stand();};
void()    dsergeant_standD20    =[    $standD26,    dsergeant_standD21    ] {ai_stand();};
void()    dsergeant_standD21    =[    $standD27,    dsergeant_standD22    ] {ai_stand();};
void()    dsergeant_standD22    =[    $standD28,    dsergeant_standD23    ] {ai_stand();};
void()    dsergeant_standD23    =[    $standD29,    dsergeant_standD24    ] {ai_stand();};
void()    dsergeant_standD24    =[    $standD30,    dsergeant_stand1    ] {ai_stand();};

//===========================================================================
void()    dsergeant_stand1    =[    $stand1,    dsergeant_stand2    ] {monster_idle_sound();ai_stand();};
void()    dsergeant_stand2    =[    $stand2,    dsergeant_stand3    ] {ai_stand();};
void()    dsergeant_stand3    =[    $stand3,    dsergeant_stand4    ] {ai_stand();};
void()    dsergeant_stand4    =[    $stand4,    dsergeant_stand5    ] {ai_stand();};
void()    dsergeant_stand5    =[    $stand5,    dsergeant_stand6    ] {ai_stand();};
void()    dsergeant_stand6    =[    $stand6,    dsergeant_stand7    ] {ai_stand();};
void()    dsergeant_stand7    =[    $stand7,    dsergeant_stand8    ] {ai_stand();};
void()    dsergeant_stand8    =[    $stand8,    dsergeant_stand9    ] {ai_stand();};
void()    dsergeant_stand9    =[    $stand9,    dsergeant_stand10    ] {ai_stand();};
void()    dsergeant_stand10    =[    $stand10,    dsergeant_stand11    ] {ai_stand();};
void()    dsergeant_stand11    =[    $stand11,    dsergeant_stand12    ] {ai_stand();};
void()    dsergeant_stand12    =[    $stand12,    dsergeant_stand1    ] {
    self.idlebusy = FALSE; 
    if (random() < MON_IDLE_ANIMATION) {
        self.idlebusy = TRUE; 
        // Make sure the idle animations don't repeat in a row
        self.lefty = self.lip;
        while (self.lefty == self.lip) { self.lefty = rint(random()*4);}
        self.lip = self.lefty;
        if (self.lip < 1) self.think = dsergeant_standD1;                // Big Sigh
        else if (self.lip < 2) self.think = dsergeant_standC1;    // Look at weapon
        else if (self.lip < 3) self.think = dsergeant_standA1;    // Shoulder hunch
        else self.think = dsergeant_standB1;                                            // Look around
    }
    ai_stand();};

//===========================================================================
void()    dsergeant_walk1    =[    $walk1,        dsergeant_walk2    ] {monster_idle_sound();
    self.idlebusy = FALSE; ai_walk(2);};
void()    dsergeant_walk2    =[    $walk2,        dsergeant_walk3    ] {monster_footstep(FALSE);ai_walk(5);};
void()    dsergeant_walk3    =[    $walk3,        dsergeant_walk4    ] {ai_walk(5);};
void()    dsergeant_walk4    =[    $walk4,        dsergeant_walk5    ] {ai_walk(4);};
void()    dsergeant_walk5    =[    $walk5,        dsergeant_walk6    ] {ai_walk(4);};
void()    dsergeant_walk6    =[    $walk6,        dsergeant_walk7    ] {ai_walk(2);};
void()    dsergeant_walk7    =[    $walk7,        dsergeant_walk8    ] {ai_walk(2);};
void()    dsergeant_walk8    =[    $walk8,        dsergeant_walk9    ] {monster_footstep(FALSE); ai_walk(3);};
void()    dsergeant_walk9    =[    $walk9,        dsergeant_walk10] {ai_walk(3);};
void()    dsergeant_walk10=[    $walk10,    dsergeant_walk11] {ai_walk(4);};
void()    dsergeant_walk11=[    $walk11,    dsergeant_walk12] {ai_walk(3);};
void()    dsergeant_walk12=[    $walk12,    dsergeant_walk13] {ai_walk(4);};
void()    dsergeant_walk13=[    $walk13,    dsergeant_walk14] {ai_walk(6);};
void()    dsergeant_walk14=[    $walk14,    dsergeant_walk1    ] {ai_walk(2);};

//----------------------------------------------------------------------
void()    dsergeant_run1    =[    $run1,    dsergeant_run2    ] {monster_idle_sound();
    self.idlebusy = FALSE; ai_run (20); CheckForCharge(); };
void()    dsergeant_run2    =[    $run2,    dsergeant_run3    ] {monster_footstep(FALSE); ai_run(25);};
void()    dsergeant_run3    =[    $run3,    dsergeant_run4    ] {ai_run(16);};
void()    dsergeant_run4    =[    $run4,    dsergeant_run5    ] {ai_run(14);};
void()    dsergeant_run5    =[    $run5,    dsergeant_run6    ] {monster_footstep(FALSE); ai_run(25);};
void()    dsergeant_run6    =[    $run6,    dsergeant_run1    ] {ai_run(13);};

//===========================================================================
void() CheckForSergeantCharge =
{
    self.attack_timer = time + 0.3;
    self.height = fabs(self.origin_z - self.enemy.origin_z);
    
    // If within melee range, go straight to melee
    if (ai_checkmelee(MONAI_MELEEFRONT)) {
        self.attack_state = AS_MELEE;
        self.think = self.th_melee;
    }
    // Is the player visible or there is a large height difference?
    else if (visblocked(self.enemy) || self.height > MONAI_CHARGEZAXIS) {
        self.attack_state = AS_STRAIGHT;
        self.think = dsergeant_run1;
    }
};

//----------------------------------------------------------------------
// Charging attack B - single swing of flail
void() dsergeant_char_b1 =[    $char_b1, dsergeant_char_b2 ] {ai_charge(23);
    dsergeant_sword_sound();CheckForSergeantCharge();};
void() dsergeant_char_b2 =[    $char_b2, dsergeant_char_b3 ] {ai_charge(17);
    monster_footstep(FALSE);CheckForSergeantCharge();};
void() dsergeant_char_b3 =[    $char_b3, dsergeant_char_b4 ] {ai_charge(12);
    CheckForSergeantCharge();};
void() dsergeant_char_b4 =[    $char_b4, dsergeant_char_b5 ] {ai_charge(22);
    self.meleecontact = TRUE;ai_meleesmash(20);CheckForSergeantCharge();};
void() dsergeant_char_b5 =[    $char_b5, dsergeant_char_b6 ] {ai_charge(18);
    monster_footstep(FALSE);CheckForSergeantCharge();};
void() dsergeant_char_b6 =[    $char_b6, dsergeant_char_b1 ] {ai_charge(8);
    CheckForSergeantCharge();};

//----------------------------------------------------------------------
// Charging attack A - swinging flail left to right infront
void()    dsergeant_char_a1    =[    $char_a1,    dsergeant_char_a2    ] {ai_charge(20);
    dsergeant_sword_sound();CheckForSergeantCharge();};
void()    dsergeant_char_a2    =[    $char_a2,    dsergeant_char_a3    ] {ai_charge(25);
    monster_footstep(FALSE);CheckForSergeantCharge();};
void()    dsergeant_char_a3    =[    $char_a3,    dsergeant_char_a4    ] {ai_charge(18);
    self.meleecontact = TRUE;ai_meleesmash(20);CheckForSergeantCharge();};
void()    dsergeant_char_a4    =[    $char_a4,    dsergeant_char_a5    ] {ai_charge(16);
    CheckForSergeantCharge();};
void()    dsergeant_char_a5    =[    $char_a5,    dsergeant_char_a6    ] {ai_charge(14);
    monster_footstep(FALSE);CheckForSergeantCharge();};
void()    dsergeant_char_a6    =[    $char_a6,    dsergeant_char_a7    ] {ai_charge(20);
    CheckForSergeantCharge();};
void()    dsergeant_char_a7    =[    $char_a7,    dsergeant_char_a8    ] {ai_charge(21);
    dsergeant_sword_sound();CheckForSergeantCharge();};
void()    dsergeant_char_a8    =[    $char_a8,    dsergeant_char_a9    ] {ai_charge(13);
    monster_footstep(FALSE);CheckForSergeantCharge();};
void()    dsergeant_char_a9    =[    $char_a9,    dsergeant_char_a10] {    ai_charge(20);
    self.meleecontact = TRUE;ai_meleesmash(20);CheckForSergeantCharge();};
void()    dsergeant_char_a10=[    $char_a10,dsergeant_char_a11] {ai_charge(20);
    CheckForSergeantCharge();};
void()    dsergeant_char_a11=[    $char_a11,dsergeant_char_a12] {ai_charge(18);
    monster_footstep(FALSE);CheckForSergeantCharge();};
void()    dsergeant_char_a12=[    $char_a12,dsergeant_char_b1    ] {ai_charge(16);
    CheckForSergeantCharge();};

//----------------------------------------------------------------------
void() dsergeant_charging =
{
    if (random() < 0.35) dsergeant_char_a1 ();    // Slow left+right swing
    else dsergeant_char_b1 ();                                    // fast spin around head
};

//===========================================================================
// Melee Attack 1 - Slice (one hand swinging run attack)
void()    dsergeant_slice1    =[    $slice2,    dsergeant_slice2    ] {ai_charge(9);};
void()    dsergeant_slice2    =[    $slice4,    dsergeant_slice3    ] {ai_charge(6);monster_footstep(FALSE);};
void()    dsergeant_slice3    =[    $slice5,    dsergeant_slice4    ] {ai_charge(13);};
void()    dsergeant_slice4    =[    $slice6,    dsergeant_slice5    ] {ai_charge(4);
    dsergeant_grunt_sound(DSERG_SOUND_SIDESWING);};
void()    dsergeant_slice5    =[    $slice7,    dsergeant_slice6    ] {ai_charge(7);monster_footstep(FALSE);};
void()    dsergeant_slice6    =[    $slice8,    dsergeant_slice7    ] {ai_charge(15);dsergeant_sword_sound();};
void()    dsergeant_slice7    =[    $slice9,    dsergeant_slice8    ] {ai_charge(8);
    self.meleecontact = TRUE;ai_meleesmash(30);};
void()    dsergeant_slice8    =[    $slice10,    dsergeant_slice9    ] {ai_charge(2);monster_footstep(FALSE);};
void()    dsergeant_slice9    =[    $slice11,    dsergeant_slice10    ] {ai_charge(0);};
void()    dsergeant_slice10    =[    $slice13,    dsergeant_slice11    ] {ai_charge(0);monster_footstep(FALSE);};
void()    dsergeant_slice11    =[    $slice14,    dsergeant_run1    ] {ai_charge(3);};

//----------------------------------------------------------------------
// Melee Attack 2 - Smash (chop overhead to ground)
void()    dsergeant_smash1    =[    $smash2,    dsergeant_smash3    ] {ai_charge(1);};
void()    dsergeant_smash3    =[    $smash3,    dsergeant_smash4    ] {ai_charge(13);};
void()    dsergeant_smash4    =[    $smash4,    dsergeant_smash5    ] {ai_charge(9);monster_footstep(FALSE);};
void()    dsergeant_smash5    =[    $smash5,    dsergeant_smash6    ] {ai_charge(11);
    dsergeant_grunt_sound(DSERG_SOUND_OVERSMASH);};
void()    dsergeant_smash6    =[    $smash6,    dsergeant_smash7    ] {ai_charge(10);dsergeant_sword_sound();};
void()    dsergeant_smash7    =[    $smash7,    dsergeant_smash8    ] {ai_charge(7);};
void()    dsergeant_smash8    =[    $smash8,    dsergeant_smash9    ] {ai_charge(12);
self.meleecontact = TRUE;ai_meleesmash(30);monster_footstep(FALSE);};
void()    dsergeant_smash9    =[    $smash9,    dsergeant_smash10    ] {ai_charge(2);};
void()    dsergeant_smash10    =[    $smash10,    dsergeant_smash11    ] {ai_charge(3);};
void()    dsergeant_smash11    =[    $smash11,    dsergeant_smash12    ] {ai_charge(0);};
void()    dsergeant_smash12    =[    $smash12,    dsergeant_smash13    ] {ai_charge(0);};
void()    dsergeant_smash13    =[    $smash13,    dsergeant_smash14    ] {ai_charge(0);};
void()    dsergeant_smash14    =[    $smash14,    dsergeant_run1    ] {ai_charge(0);};

//----------------------------------------------------------------------
// Melee Attack 3 - W_Attack (two handed swinging attack)
void()    dsergeant_watk1    =[    $w_attack1,    dsergeant_watk2    ] {ai_charge(2);};
void()    dsergeant_watk2    =[    $w_attack2,    dsergeant_watk3    ] {ai_charge(0);monster_footstep(FALSE);};
void()    dsergeant_watk3    =[    $w_attack3,    dsergeant_watk4    ] {ai_charge(0);};
void()    dsergeant_watk4    =[    $w_attack4,    dsergeant_watk5    ] {ai_charge(0);dsergeant_sword_sound();};
void()    dsergeant_watk5    =[    $w_attack5,    dsergeant_watk6    ] {ai_charge(0);
    dsergeant_grunt_sound(DSERG_SOUND_RUNSWING);};
void()    dsergeant_watk6    =[    $w_attack6,    dsergeant_watk7    ] {ai_charge(0);};
void()    dsergeant_watk7    =[    $w_attack7,    dsergeant_watk8    ] {ai_charge(1);
    self.meleecontact = TRUE;ai_meleesmash(20);monster_footstep(FALSE);};
void()    dsergeant_watk8    =[    $w_attack8,    dsergeant_watk9    ] {ai_charge(4);};
void()    dsergeant_watk9    =[    $w_attack9,    dsergeant_watk10] {ai_charge(5);};
void()    dsergeant_watk10=[    $w_attack10,dsergeant_watk11] {ai_charge(3);};
void()    dsergeant_watk11=[    $w_attack11,dsergeant_watk12] {ai_charge(2);};
void()    dsergeant_watk12=[    $w_attack12,dsergeant_watk13] {ai_charge(2);};
void()    dsergeant_watk13=[    $w_attack13,dsergeant_watk14] {ai_charge(0);};
void()    dsergeant_watk14=[    $w_attack14,dsergeant_watk15] {ai_charge(0);
    dsergeant_grunt_sound(DSERG_SOUND_RUNSWING);};
void()    dsergeant_watk15=[    $w_attack15,dsergeant_watk16] {ai_charge(0);dsergeant_sword_sound();};
void()    dsergeant_watk16=[    $w_attack16,dsergeant_watk17] {ai_charge(1);monster_footstep(FALSE);};
void()    dsergeant_watk17=[    $w_attack17,dsergeant_watk18] {ai_charge(1);
    self.meleecontact = TRUE;ai_meleesmash(20);};
void()    dsergeant_watk18=[    $w_attack18,dsergeant_watk19] {ai_charge(3);};
void()    dsergeant_watk19=[    $w_attack19,dsergeant_watk20] {ai_charge(4);};
void()    dsergeant_watk20=[    $w_attack20,dsergeant_watk21] {ai_charge(6);};
void()    dsergeant_watk21=[    $w_attack21,dsergeant_watk22] {ai_charge(7);};
void()    dsergeant_watk22=[    $w_attack22,dsergeant_run1    ] {ai_charge(3);};

//----------------------------------------------------------------------
void() dsergeant_melee =
{
    self.meleeattack = random();
    if (self.meleeattack < 0.4) dsergeant_slice1 ();
    else if (self.meleeattack < 0.9) dsergeant_smash1 ();
    else dsergeant_watk1 ();
};

//===========================================================================
// Death Sergeant Homing Missile Attack
//===========================================================================
void() SergMissile =
{
    if (!self.enemy) return;
    if (self.health < 1) return;
    // Removing a fired homing missiles looks weird
    //if (!visxray(self.enemy, self.attack_offset, '0 0 10', FALSE)) return;

    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "dknight/dserg_magic.wav", 1, ATTN_NORM);
    self.attack_speed = SPEED_SERGMISSILE + (skill * SPEED_SERGSKILL);
    Launch_HomingMissile (self.attack_offset, '0 0 10', CT_PROJ_SERG, self.attack_speed);
};

//----------------------------------------------------------------------
// Hell Knight - 11 frames / Death Knight - 14 frames (skipped 1, 10,14)
// Death Knight frames trimmed down to match Hell Knight (damage ratio)
// Block pain function from interrupting magic attack
void()    dsergeant_magicc1 =[    $magicc2,    dsergeant_magicc2    ] {ai_face();
    SUB_AttackFinished (2 + random()*2);
    self.pain_finished = time + 1;
};
void()    dsergeant_magicc2 =[    $magicc3,    dsergeant_magicc3    ] {ai_face();};
void()    dsergeant_magicc3 =[    $magicc4,    dsergeant_magicc4    ] {ai_face();};
void()    dsergeant_magicc4 =[    $magicc5,    dsergeant_magicc5    ] {ai_face();};
void()    dsergeant_magicc5 =[    $magicc6,    dsergeant_magicc6    ] {ai_face();};
void()    dsergeant_magicc6 =[    $magicc7,    dsergeant_magicc7    ] {ai_face();};
void()    dsergeant_magicc7 =[    $magicc8,    dsergeant_magicc8    ] {ai_face();};
void()    dsergeant_magicc8 =[    $magicc9,    dsergeant_magicc9    ] {SergMissile();};
void()    dsergeant_magicc9 =[    $magicc11,    dsergeant_magicc10] {ai_face();};
void()    dsergeant_magicc10 =[    $magicc12,    dsergeant_magicc11] {ai_face();};
void()    dsergeant_magicc11 =[    $magicc13,    dsergeant_run1] {ai_face();};

//============================================================================

void()    dsergeant_painA1    =[    $painA1,        dsergeant_painA2    ] {};
void()    dsergeant_painA2    =[    $painA2,        dsergeant_painA3    ] {};
void()    dsergeant_painA3    =[    $painA3,        dsergeant_painA4    ] {};
void()    dsergeant_painA4    =[    $painA4,        dsergeant_painA5    ] {};
void()    dsergeant_painA5    =[    $painA5,        dsergeant_run1    ] {};
//----------------------------------------------------------------------
// Not used, too slow for the segeant
void()    dsergeant_painD1    =[    $painD1,        dsergeant_painD2    ] {};
void()    dsergeant_painD2    =[    $painD2,        dsergeant_painD3    ] {};
void()    dsergeant_painD3    =[    $painD3,        dsergeant_painD4    ] {};
void()    dsergeant_painD4    =[    $painD4,        dsergeant_painD5    ] {monster_footstep(FALSE);};
void()    dsergeant_painD5    =[    $painD5,        dsergeant_painD6    ] {};
void()    dsergeant_painD6    =[    $painD6,        dsergeant_painD7    ] {};
void()    dsergeant_painD7    =[    $painD7,        dsergeant_painD8    ] {};
void()    dsergeant_painD8    =[    $painD8,        dsergeant_painD9    ] {monster_footstep(FALSE);};
void()    dsergeant_painD9    =[    $painD9,        dsergeant_painD10    ] {};
void()    dsergeant_painD10    =[    $painD10,        dsergeant_painD11    ] {};
void()    dsergeant_painD11    =[    $painD11,        dsergeant_run1    ] {};
//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage)    dsergeant_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1) {
            // Randomly pick which pain animation to play
            if (random() < 0.85) dsergeant_painA1 ();    // classic, body recoil
            else {
                dsergeant_painD1 ();                    // Cool stumble
                self.pain_finished = time + 2;            // long animation
            }
        }
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 1.1;
            self.axhitme = 0;
            dsergeant_painD1 ();
        }
    }
};

//============================================================================

void()    dsergeant_die1    =[    $death1,    dsergeant_die2    ] {ai_forward(10);};
void()    dsergeant_die2    =[    $death2,    dsergeant_die3    ] {monster_check_gib();ai_forward(8);};
void()    dsergeant_die3    =[    $death3,    dsergeant_die4    ] {monster_check_gib();
    self.solid = SOLID_NOT; ai_forward(7);};
void()    dsergeant_die4    =[    $death4,    dsergeant_die5    ] {};
void()    dsergeant_die5    =[    $death5,    dsergeant_die6    ] {};
void()    dsergeant_die6    =[    $death6,    dsergeant_die7    ] {
    sound (self, CHAN_WEAPON, GIB_SOUND_CHAIN, 1, ATTN_NORM);};
void()    dsergeant_die7    =[    $death7,    dsergeant_die8    ] {};
void()    dsergeant_die8    =[    $death8,    dsergeant_die9    ] {ai_forward(10);};
void()    dsergeant_die9    =[    $death9,    dsergeant_die10    ] {ai_forward(11);};
void()    dsergeant_die10    =[    $death10,    dsergeant_die11    ] {};
void()    dsergeant_die11    =[    $death11,    dsergeant_die12    ] {monster_death_postcheck();};
void()    dsergeant_die12    =[    $death12,    dsergeant_die12    ] {monster_deadbody_check();};
//----------------------------------------------------------------------
void()    dsergeant_dieb1    =[    $deathb1,    dsergeant_dieb2    ] {};
void()    dsergeant_dieb2    =[    $deathb2,    dsergeant_dieb3    ] {monster_check_gib();};
void()    dsergeant_dieb3    =[    $deathb3,    dsergeant_dieb4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void()    dsergeant_dieb4    =[    $deathb4,    dsergeant_dieb5    ] {};
void()    dsergeant_dieb5    =[    $deathb5,    dsergeant_dieb6    ] {};
void()    dsergeant_dieb6    =[    $deathb6,    dsergeant_dieb7    ] {};
void()    dsergeant_dieb7    =[    $deathb7,    dsergeant_dieb8    ] {};
void()    dsergeant_dieb8    =[    $deathb8,    dsergeant_dieb9    ] {};
void()    dsergeant_dieb9    =[    $deathb9,    dsergeant_dieb10    ] {
    sound (self, CHAN_WEAPON, GIB_SOUND_CHAIN, 1, ATTN_NORM);};
void()    dsergeant_dieb10    =[    $deathb10,    dsergeant_dieb11    ] {};
void()    dsergeant_dieb11    =[    $deathb11,    dsergeant_dieb12    ] {};
void()    dsergeant_dieb12    =[    $deathb12,    dsergeant_dieb13    ] {};
void()    dsergeant_dieb13    =[    $deathb13,    dsergeant_dieb14    ] {};
void()    dsergeant_dieb14    =[    $deathb14,    dsergeant_dieb15    ] {};
void()    dsergeant_dieb15    =[    $deathb15,    dsergeant_dieb16    ] {};
void()    dsergeant_dieb16    =[    $deathb16,    dsergeant_dieb17    ] {};
void()    dsergeant_dieb17    =[    $deathb17,    dsergeant_dieb18    ] {};
void()    dsergeant_dieb18    =[    $deathb18,    dsergeant_dieb19    ] {};
void()    dsergeant_dieb19    =[    $deathb19,    dsergeant_dieb20    ] {monster_death_postcheck();};
void()    dsergeant_dieb20    =[    $deathb20,    dsergeant_dieb20    ] {monster_deadbody_check();};
//----------------------------------------------------------------------
void() dsergeant_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "hknight/death1.wav", 1, ATTN_NORM);
        if (random() > 0.5) dsergeant_die1 ();
        else dsergeant_dieb1 ();
    }
};

//============================================================================
void() dsergeant_sword_sound =
{
    self.meleecontact = TRUE;
    self.lip = random();
    if (self.lip < 0.25) sound (self, CHAN_WEAPON, "weapons/sword1a.wav", 1, ATTN_NORM);
    else if (self.lip < 0.5) sound (self, CHAN_WEAPON, "weapons/sword2a.wav", 1, ATTN_NORM);
    else if (self.lip < 0.75) sound (self, CHAN_WEAPON, "weapons/sword1b.wav", 1, ATTN_NORM);
    else sound (self, CHAN_WEAPON, "weapons/sword2b.wav", 1, ATTN_NORM);
};

//============================================================================
void(float soundtype) dsergeant_grunt_sound =
{
    if (soundtype == DSERG_SOUND_SIDESWING)
        sound (self, CHAN_WEAPON, "dknight/dfury_grunt2.wav", 1, ATTN_NORM);
    else if (soundtype == DSERG_SOUND_OVERSMASH)
        sound (self, CHAN_WEAPON, "dknight/dfury_leap.wav", 1, ATTN_NORM);
    else {
        self.weaponswitch = 0.6 + random()*0.4;
        if (random() < 0.5) sound (self, CHAN_WEAPON, "dknight/dfury_grunt3.wav", self.weaponswitch, ATTN_NORM);
        else sound (self, CHAN_WEAPON, "dknight/dfury_grunt4.wav", self.weaponswitch, ATTN_NORM);
    }
};

/*======================================================================
QUAKED monster_dsergeant (1 0 0) (-16 -16 -24) (16 16 40) Ambush
======================================================================*/
void() monster_dsergeant =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_dsergeant.mdl";            // Blue badass!
    self.headmdl = "progs/h_dknight.mdl";
    self.gib1mdl = "progs/w_blueflail.mdl";            // Unique weapon
    self.gib2mdl = "progs/gib_knfoot_l.mdl";        // left foot
    self.gib3mdl = "progs/gib_knfoot_r.mdl";        // right foot

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_SERG);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);

    self.gib2skin = self.gib3skin = 3;                // Sergeant skin
    self.gib1sound = GIB_IMPACT_CHAIN;

    self.idle_sound = "hknight/idle.wav";
    precache_sound (self.idle_sound);

    // Special extra sounds
    precache_sound ("dknight/dserg_hbreath.wav");    // Special long sigh (idle animation)
    precache_sound ("dknight/dserg_magic.wav");        // New Spike ball launch sound
    precache_sound ("hknight/death1.wav");
    self.pain_sound = "hknight/pain1.wav";
    precache_sound (self.pain_sound);

    // Jingle, Jangle of the flail chain
    self.sight_sound = "dknight/dserg_sight.wav";
    precache_sound (self.sight_sound);
    
    // Heavy thud sound    
    self.meleehitsound = GIB_SOUND_HEAVY2;
    precache_sound (self.meleehitsound);

    // New sword sweep/slash sounds
    precache_sound ("weapons/sword1a.wav");
    precache_sound ("weapons/sword1b.wav");
    precache_sound ("weapons/sword2a.wav");
    precache_sound ("weapons/sword2b.wav");

    // New grunt sounds during combat
    precache_sound ("dknight/dfury_grunt2.wav");
    precache_sound ("dknight/dfury_grunt3.wav");
    precache_sound ("dknight/dfury_grunt4.wav");
    precache_sound ("dknight/dfury_leap.wav");

    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
        precache_model (MODEL_PROJ_SHALGRN);    // Green voreball
    }

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    if (self.health < 1) self.health = 400;        // Shalrath level
    self.gibhealth = -60;
    self.gibbed = FALSE;
    self.pain_flinch = 80;                // Take a lot to go into pain
    self.pain_longanim = TRUE;            // can be chopped with shadow axe
    self.pain_timeout = 2;                // Shambler like pain level
    self.infightextra = 2;                // Double damage to other monsters
    self.steptype = FS_TYPEHEAVY;        // Heavy Player feet
    self.meleecontact = FALSE;            // Smash contact
    self.meleerange = MONAI_MELEEFRONT;    // Smash attack
    self.attack_offset = '0 0 10';
    self.deathstring = " was crushed by a Blue Flail Knight\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    self.th_checkattack = DSergeantCheckAttack;
    self.th_stand = dsergeant_stand1;
    self.th_walk = dsergeant_walk1;
    self.th_run = dsergeant_run1;
    self.th_melee = dsergeant_melee;
    self.th_missile = dsergeant_magicc1;
    self.th_pain = dsergeant_pain;
    self.th_die = dsergeant_die;
    self.th_charge = dsergeant_charging;
    
    self.meleeattack = rint(random()*3);    // randomize last melee attack

    self.classtype = CT_MONDSERG;
    self.classgroup = CG_DCAPTAIN;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};
