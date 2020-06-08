/*==============================================================================
FURY KNIGHT (one more night)
==============================================================================*/
$cd id1/models/dfury
$origin 0 0 24
$base base
$skin skin

// (001 - 012) Default Stand - LEFT Sword forward 
$frame standL1 standL2 standL3 standL4 standL5 standL6 standL7 standL8
$frame standL9 standL10 standL11 standL12

// (013 - 019) Switch dual swords LEFT -> RIGHT
$frame standLR1 standLR2 standLR3 standLR4 standLR5 standLR6 standLR7

// (020 - 031) Default Stand - RIGHT Sword forward 
$frame standR1 standR2 standR3 standR4 standR5 standR6 standR7 standR8
$frame standR9 standR10 standR11 standR12

// (032 - 038) Switch dual swords RIGHT -> LEFT
$frame standRL1 standRL2 standRL3 standRL4 standRL5 standRL6 standRL7

// (039 - 050) Stand idle - Quick look around (MUST following left sword idle)
$frame standB1 standB2 standB3 standB4 standB5 standB6 standB7 standB8 
$frame standB9 standB10 standB11 standB12

// (051 - 070) Stand idle - checking sword (MUST following left sword idle)
$frame standC1 standC2 standC3 standC4 standC5 standC6 standC7 standC8 
$frame standC9 standC10 standC11 standC12 standC13 standC14 standC15 standC16
$frame standC17 standC18 standC19 standC20

// (071 - 084) Default walk - typical hell knight stride
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9
$frame walk10 walk11 walk12 walk13 walk14

// (085 - 090) Default run
$frame run1 run2 run3 run4 run5 run6

// Pain animations A = Slow D = Stagger
// (091 - 095) (096 - 111)
$frame painA1 painA2 painA3 painA4 painA5

$frame painD1 painD2 painD3 painD4 painD5 painD6 painD7 painD8
$frame painD9 painD10 painD11 painD12 painD13 painD14 painD15 painD16

// (112 - 125) Forward dual sword Slice
$frame slice1 slice2 slice3 slice4 slice5 slice6 slice7 slice8
$frame slice9 slice10 slice11 slice12 slice13 slice14 

// (126 - 140) Forward dual sword Lunge
$frame lunge1 lunge2 lunge3 lunge4 lunge5 lunge6 lunge7 lunge8
$frame lunge9 lunge10 lunge11 lunge12 lunge13 lunge14 lunge15

// (141 - 164) Charging attack A - swinging swords left to right infront
$frame w_attack1 w_attack2 w_attack3 w_attack4 w_attack5 w_attack6 w_attack7 w_attack8
$frame w_attack9 w_attack10 w_attack11 w_attack12 w_attack13 w_attack14 w_attack15 w_attack16
$frame w_attack17 w_attack18 w_attack19 w_attack20 w_attack21 w_attack22 w_attack23 w_attack24

// (165 - 172) Right Swing
$frame rswing1 rswing2 rswing3 rswing4 rswing5 rswing6 rswing7 rswing8

// (173 - 180) Left Swing
$frame lswing1 lswing2 lswing3 lswing4 lswing5 lswing6 lswing7 lswing8

// (181 - 195) Smash (chop overhead to ground)
$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7 smash8
$frame smash9 smash10 smash11 smash12 smash13 smash14 smash15

// (196 - 211) MagicA - Swords lock together infront (missile attack)
$frame magica1 magica2 magica3 magica4 magica5 magica6 magica7 magica8
$frame magica9 magica10 magica11 magica12 magica13 magica14 magica15 magica16

// (212 - 224) MagicB - Swords Swipe downwards infront (enable bullet shield)
$frame magicb1 magicb2 magicb3 magicb4 magicb5 magicb6 magicb7 magicb8
$frame magicb9 magicb10 magicb11 magicb12 magicb13

// (225 - 241) Death forward
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12 death13 death14 death15 death16 death17

// (242 - 255) Death back
$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9 deathb10 deathb11 deathb12 deathb13 deathb14

void() dfury_standL1;
void() dfury_magic;
void() dfury_sword_sound;                    // Sword swipe sounds
void(float soundtype) dfury_grunt_sound;    // Grunting while attacking

// Different types of grunting sounds
float DFURY_SOUND_PULLAPART = 1;
float DFURY_SOUND_OVERSMASH = 2;
float DFURY_SOUND_JUMP = 3;
float DFURY_SOUND_LRFURY = 4;
float DFURY_SOUND_RUNSWING = 5;
float DFURY_SOUND_OPTIONAL = 6;

//----------------------------------------------------------------------
void()    dfury_standB1    =[    $standB1,    dfury_standB2    ] {ai_stand();};
void()    dfury_standB2    =[    $standB2,    dfury_standB3    ] {ai_stand();};
void()    dfury_standB3    =[    $standB3,    dfury_standB4    ] {ai_stand();};
void()    dfury_standB4    =[    $standB4,    dfury_standB5    ] {ai_stand();};
void()    dfury_standB5    =[    $standB5,    dfury_standB6    ] {ai_stand();};
void()    dfury_standB6    =[    $standB6,    dfury_standB7    ] {ai_stand();};
void()    dfury_standB7    =[    $standB7,    dfury_standB8    ] {ai_stand();};
void()    dfury_standB8    =[    $standB8,    dfury_standB9    ] {ai_stand();self.nextthink = self.nextthink + random()*0.2;};
void()    dfury_standB9    =[    $standB9,    dfury_standB10    ] {ai_stand();self.nextthink = self.nextthink + random()*0.2;};
void()    dfury_standB10    =[    $standB10,    dfury_standB11    ] {ai_stand();self.nextthink = self.nextthink + random()*0.2;};
void()    dfury_standB11    =[    $standB11,    dfury_standB12    ] {ai_stand();};
void()    dfury_standB12    =[    $standB12,    dfury_standL1    ] {ai_stand();};

//----------------------------------------------------------------------
void()    dfury_standC1    =[    $standC1,    dfury_standC2    ] {ai_stand();};
void()    dfury_standC2    =[    $standC2,    dfury_standC3    ] {ai_stand();};
void()    dfury_standC3    =[    $standC3,    dfury_standC4    ] {ai_stand();};
void()    dfury_standC4    =[    $standC4,    dfury_standC5    ] {ai_stand();};
void()    dfury_standC5    =[    $standC5,    dfury_standC6    ] {ai_stand();};
void()    dfury_standC6    =[    $standC6,    dfury_standC7    ] {ai_stand();};
void()    dfury_standC7    =[    $standC7,    dfury_standC8    ] {ai_stand();};
void()    dfury_standC8    =[    $standC8,    dfury_standC9    ] {ai_stand();};
void()    dfury_standC9    =[    $standC9,    dfury_standC10    ] {ai_stand();};
void()    dfury_standC10    =[    $standC10,    dfury_standC11    ] {ai_stand();};
void()    dfury_standC11    =[    $standC11,    dfury_standC12    ] {ai_stand();};
void()    dfury_standC12    =[    $standC12,    dfury_standC13    ] {ai_stand();};
void()    dfury_standC13    =[    $standC13,    dfury_standC14    ] {ai_stand();};
void()    dfury_standC14    =[    $standC14,    dfury_standC15    ] {ai_stand();};
void()    dfury_standC15    =[    $standC15,    dfury_standC16    ] {ai_stand();};
void()    dfury_standC16    =[    $standC16,    dfury_standC17    ] {ai_stand();};
void()    dfury_standC17    =[    $standC17,    dfury_standC18    ] {ai_stand();};
void()    dfury_standC18    =[    $standC18,    dfury_standC19    ] {ai_stand();};
void()    dfury_standC19    =[    $standC19,    dfury_standC20    ] {ai_stand();};
void()    dfury_standC20    =[    $standC20,    dfury_standL1    ] {ai_stand();};

//----------------------------------------------------------------------
void()    dfury_standRL1    =[    $standRL1,    dfury_standRL2    ] {ai_stand();};
void()    dfury_standRL2    =[    $standRL2,    dfury_standRL3    ] {ai_stand();};
void()    dfury_standRL3    =[    $standRL3,    dfury_standRL4    ] {ai_stand();};
void()    dfury_standRL4    =[    $standRL4,    dfury_standRL5    ] {ai_stand();};
void()    dfury_standRL5    =[    $standRL5,    dfury_standRL6    ] {ai_stand();};
void()    dfury_standRL6    =[    $standRL6,    dfury_standRL7    ] {ai_stand();};
void()    dfury_standRL7    =[    $standRL7, dfury_standL1    ] {ai_stand();};

//----------------------------------------------------------------------
void()    dfury_standR1    =[    $standR1,    dfury_standR2    ] {monster_idle_sound();
    if (random() < MON_IDLE_ANIMATION && self.weaponswitch < time) 
        self.think = dfury_standRL1;    // Switch weapon stance
    ai_stand();
};
void()    dfury_standR2    =[    $standR2,    dfury_standR3    ] {ai_stand();};
void()    dfury_standR3    =[    $standR3,    dfury_standR4    ] {ai_stand();};
void()    dfury_standR4    =[    $standR4,    dfury_standR5    ] {ai_stand();};
void()    dfury_standR5    =[    $standR5,    dfury_standR6    ] {ai_stand();};
void()    dfury_standR6    =[    $standR6,    dfury_standR7    ] {monster_idle_sound(); ai_stand();};
void()    dfury_standR7    =[    $standR7,    dfury_standR8    ] {ai_stand();};
void()    dfury_standR8    =[    $standR8,    dfury_standR9    ] {ai_stand();};
void()    dfury_standR9    =[    $standR9,    dfury_standR10    ] {ai_stand();};
void()    dfury_standR10=[    $standR10, dfury_standR11    ] {ai_stand();};
void()    dfury_standR11=[    $standR11, dfury_standR12    ] {ai_stand();};
void()    dfury_standR12=[    $standR12, dfury_standR1    ] {ai_stand();};

//----------------------------------------------------------------------
void()    dfury_standLR1    =[    $standLR1,    dfury_standLR2    ] {ai_stand();};
void()    dfury_standLR2    =[    $standLR2,    dfury_standLR3    ] {ai_stand();};
void()    dfury_standLR3    =[    $standLR3,    dfury_standLR4    ] {ai_stand();};
void()    dfury_standLR4    =[    $standLR4,    dfury_standLR5    ] {ai_stand();};
void()    dfury_standLR5    =[    $standLR5,    dfury_standLR6    ] {ai_stand();};
void()    dfury_standLR6    =[    $standLR6,    dfury_standLR7    ] {ai_stand();};
void()    dfury_standLR7    =[    $standLR7, dfury_standR1    ] {ai_stand();};

//----------------------------------------------------------------------
void()    dfury_standL1    =[    $standL1,    dfury_standL2    ] {monster_idle_sound();
    self.idlebusy = FALSE;
    if (random() < MON_IDLE_ANIMATION && self.weaponswitch < time) {
        // Slow down the repeating of idle animations
        self.weaponswitch = time + 1 + random()*3;
        self.idlebusy = TRUE;

        // Make sure the idle animations don't repeat in a row
        self.lefty = self.lip;
        while (self.lefty == self.lip) { self.lefty = rint(random()*4);}
        self.lip = self.lefty;

        if (self.lip < 2) self.think = dfury_standC1;        // Look at weapon
        else if (self.lip == 2) self.think = dfury_standB1;    // Look around
        else self.think = dfury_standLR1;                    // Switch weapon stance
    }
    ai_stand();
};
void()    dfury_standL2    =[    $standL2,    dfury_standL3    ] {ai_stand();};
void()    dfury_standL3    =[    $standL3,    dfury_standL4    ] {ai_stand();};
void()    dfury_standL4    =[    $standL4,    dfury_standL5    ] {ai_stand();};
void()    dfury_standL5    =[    $standL5,    dfury_standL6    ] {ai_stand();};
void()    dfury_standL6    =[    $standL6,    dfury_standL7    ] {ai_stand();};
void()    dfury_standL7    =[    $standL7,    dfury_standL8    ] {ai_stand();};
void()    dfury_standL8    =[    $standL8,    dfury_standL9    ] {ai_stand();};
void()    dfury_standL9    =[    $standL9,    dfury_standL10    ] {ai_stand();};
void()    dfury_standL10=[    $standL10, dfury_standL11    ] {ai_stand();};
void()    dfury_standL11=[    $standL11, dfury_standL12    ] {ai_stand();};
void()    dfury_standL12=[    $standL12, dfury_standL1    ] {ai_stand();};

//===========================================================================
void()    dfury_walk1    =[    $walk1,        dfury_walk2    ] { monster_idle_sound(); 
    self.idlebusy = FALSE; ai_walk(2);};
void()    dfury_walk2    =[    $walk2,        dfury_walk3    ] {monster_footstep(FALSE);ai_walk(5);};
void()    dfury_walk3    =[    $walk3,        dfury_walk4    ] {ai_walk(5);};
void()    dfury_walk4    =[    $walk4,        dfury_walk5    ] {ai_walk(4);};
void()    dfury_walk5    =[    $walk5,        dfury_walk6    ] {ai_walk(4);};
void()    dfury_walk6    =[    $walk6,        dfury_walk7    ] {ai_walk(2);};
void()    dfury_walk7    =[    $walk7,        dfury_walk8    ] {ai_walk(2);};
void()    dfury_walk8    =[    $walk8,        dfury_walk9    ] {monster_footstep(FALSE); ai_walk(3);};
void()    dfury_walk9    =[    $walk9,        dfury_walk10] {ai_walk(3);};
void()    dfury_walk10=[    $walk10,    dfury_walk11] {ai_walk(4);};
void()    dfury_walk11=[    $walk11,    dfury_walk12] {ai_walk(3);};
void()    dfury_walk12=[    $walk12,    dfury_walk13] {ai_walk(4);};
void()    dfury_walk13=[    $walk13,    dfury_walk14] {ai_walk(6);};
void()    dfury_walk14=[    $walk14,    dfury_walk1    ] {ai_walk(2);};

//===========================================================================
// 6 frames instead of 8 frames (skipping 3rd/7th timing of original)
void()    dfury_run1    =[    $run1,    dfury_run2    ] {monster_idle_sound();
    self.idlebusy = FALSE;ai_run (22); };
void()    dfury_run2    =[    $run2,    dfury_run3    ] {monster_footstep(FALSE); ai_run(27);};
void()    dfury_run3    =[    $run3,    dfury_run4    ] {ai_run(18);};
void()    dfury_run4    =[    $run4,    dfury_run5    ] {ai_run(16);};
void()    dfury_run5    =[    $run5,    dfury_run6    ] {monster_footstep(FALSE); ai_run(27);};
void()    dfury_run6    =[    $run6,    dfury_run1    ] {ai_run(15);};

//===========================================================================
// Melee Attack 1 - Slice (bring swords together and pull apart)
void()    dfury_slice1    =[    $slice2,    dfury_slice2    ] {dfury_grunt_sound(DFURY_SOUND_PULLAPART);ai_charge(9);};
void()    dfury_slice2    =[    $slice3,    dfury_slice3    ] {ai_charge(10); dfury_sword_sound();};
void()    dfury_slice3    =[    $slice4,    dfury_slice4    ] {ai_charge(17);};
void()    dfury_slice4    =[    $slice5,    dfury_slice5    ] {monster_footstep(FALSE); ai_charge(4);};
void()    dfury_slice5    =[    $slice6,    dfury_slice6    ] {ai_charge(11); dfury_sword_sound(); ai_melee();};
void()    dfury_slice6    =[    $slice7,    dfury_slice7    ] {ai_charge(19); ai_melee();};
void()    dfury_slice7    =[    $slice8,    dfury_slice8    ] {ai_charge(12); ai_melee();};
void()    dfury_slice8    =[    $slice9,    dfury_slice9    ] {ai_charge(6); ai_melee();};
void()    dfury_slice9    =[    $slice11,    dfury_slice10    ] {monster_footstep(FALSE); ai_melee();};
void()    dfury_slice10    =[    $slice13,    dfury_slice11    ] {ai_charge(0);};
void()    dfury_slice11    =[    $slice14,    dfury_run1    ] {monster_footstep(FALSE); ai_charge(7);};

//----------------------------------------------------------------------
// Melee Attack 2 - Smash (both swords chop overhead to ground)
void()    dfury_smash1    =[    $smash2,    dfury_smash2    ] {ai_charge(1);};
void()    dfury_smash2    =[    $smash3,    dfury_smash3    ] {ai_charge(13);};
void()    dfury_smash3    =[    $smash4,    dfury_smash4    ] {ai_charge(9); monster_footstep(FALSE);};
void()    dfury_smash4    =[    $smash5,    dfury_smash5    ] {ai_charge(11);};
void()    dfury_smash5    =[    $smash6,    dfury_smash6    ] {dfury_grunt_sound(DFURY_SOUND_OVERSMASH);ai_charge(10);};
void()    dfury_smash6    =[    $smash7,    dfury_smash7    ] {ai_charge(7); dfury_sword_sound();};
void()    dfury_smash7    =[    $smash8,    dfury_smash8    ] {ai_charge(12); monster_footstep(FALSE);};
void()    dfury_smash8    =[    $smash9,    dfury_smash9    ] {ai_charge(2);self.meleecontact = TRUE; ai_meleesmash(40);};
void()    dfury_smash9    =[    $smash10,    dfury_smash10    ] {ai_charge(3); ai_melee();self.meleecontact = FALSE;};
void()    dfury_smash10    =[    $smash12,    dfury_smash11    ] {ai_charge(0);};
void()    dfury_smash11    =[    $smash13,    dfury_run1    ] {ai_charge(0);};

//----------------------------------------------------------------------
// Melee Attack 4a - Single Right Swing (fast attack)
void()    dfury_lswing3;
void()    dfury_rswing1    =[    $rswing2,    dfury_rswing3    ] {ai_charge(2);};
void()    dfury_rswing3    =[    $rswing3,    dfury_rswing4    ] {dfury_grunt_sound(DFURY_SOUND_LRFURY);ai_charge(5);};
void()    dfury_rswing4    =[    $rswing4,    dfury_rswing5    ] {ai_charge(8);ai_melee();monster_footstep(FALSE);};
void()    dfury_rswing5    =[    $rswing5,    dfury_rswing6    ] {ai_charge(7);ai_melee();};
void()    dfury_rswing6    =[    $rswing6,    dfury_rswing7    ] {ai_charge(5);ai_melee(); dfury_sword_sound();};
void()    dfury_rswing7    =[    $rswing7,    dfury_rswing8    ] {ai_charge(2);monster_footstep(FALSE);};
void()    dfury_rswing8    =[    $rswing8,    dfury_run1    ] {ai_charge(1);
    if (ai_checkmelee(MONAI_MELEEFRONT) && self.enemy.health > 0) self.think = dfury_lswing3;
};

//----------------------------------------------------------------------
// Melee Attack 4a - Single Right Swing (fast attack)
void()    dfury_lswing1    =[    $lswing2,    dfury_lswing3    ] {ai_charge(2);};
void()    dfury_lswing3    =[    $lswing3,    dfury_lswing4    ] {dfury_grunt_sound(DFURY_SOUND_OPTIONAL);ai_charge(5);};
void()    dfury_lswing4    =[    $lswing4,    dfury_lswing5    ] {ai_charge(8);ai_melee();monster_footstep(FALSE);};
void()    dfury_lswing5    =[    $lswing5,    dfury_lswing6    ] {ai_charge(7);ai_melee();};
void()    dfury_lswing6    =[    $lswing6,    dfury_lswing7    ] {ai_charge(5);ai_melee(); dfury_sword_sound();};
void()    dfury_lswing7    =[    $lswing7,    dfury_lswing8    ] {ai_charge(2);monster_footstep(FALSE);};
void()    dfury_lswing8    =[    $lswing8,    dfury_run1    ] {ai_charge(1);
    if (ai_checkmelee(MONAI_MELEEFRONT) && self.enemy.health > 0) self.think = dfury_rswing3;
};

//----------------------------------------------------------------------
// Melee Attack 3 - Lunge forward (leaving ground) and do heavy damage
void() dfury_JumpTouch =
{
    local float ldmg;
    
    if (self.health < 1) return;
    ai_jumpbreakable(30);            // Damage any breakables
    self.touch = SUB_Null;            // No more touching
    self.think = self.th_jumpexit;    // Exit frame
    self.jumptouch = other;            // Keep track of touch target

    if ( CanDamage(other, self) ) {
        if ( vlen(self.velocity) > 300 ) {
          ldmg = 20 + 10*random();
            T_Damage (other, self, self, ldmg, DAMARMOR);    
            spawn_touchblood (self, self.enemy, ldmg*3);
        }
    }

    // Is the fury knight floating in the air?
    if (!checkbottom(self)) {
        // Is the fury knight standing on something?        
        if (self.flags & FL_ONGROUND) {    
            // Do an extra jump if got the count
            if (self.count < 2)  self.think = self.th_jump;
        }
    }

    // Next timer
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void()    dfury_lunge1    =[    $lunge1,    dfury_lunge2    ] {ai_face();
    self.jump_flag = time + MONAI_JUMPFURYTIME;
    dfury_grunt_sound(DFURY_SOUND_JUMP);
};
void()    dfury_lunge2    =[    $lunge2,    dfury_lunge3    ] {ai_face();};
void()    dfury_lunge3    =[    $lunge3,    dfury_lunge4    ] {ai_face();};
void()    dfury_lunge4    =[    $lunge4,    dfury_lunge5    ] {
    ai_face();
    self.jump_flag = time + MONAI_JUMPFURYTIME;
    self.touch = dfury_JumpTouch;
    makevectors (self.angles);
    self.velocity = v_forward * 500 + '0 0 250';
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.oldorigin = self.origin;
};

void()    dfury_lunge5    =[    $lunge5,    dfury_lunge6    ] {};
void()    dfury_lunge6    =[    $lunge6,    dfury_lunge7    ] {};
void()    dfury_lunge7    =[    $lunge7,    dfury_lunge8    ] {};
void()    dfury_lunge8    =[    $lunge8,    dfury_lunge8    ] {
    // Double check monster is still falling?
    if (CheckZeroVector(self.velocity) || self.oldorigin == self.origin) {
        self.ideal_yaw = random() * 360;     //random jump angle
        self.think = dfury_lunge4;            // Keep jumping
    }
    self.oldorigin = self.origin;
};

//----------------------------------------------------------------------
// Landed with thrush attack
void()    dfury_lunge9    =[    $lunge9,    dfury_lunge10    ] {ai_melee();};
void()    dfury_lunge10    =[    $lunge10,    dfury_lunge11    ] {ai_charge(5);ai_melee();};
void()    dfury_lunge11    =[    $lunge11,    dfury_lunge12    ] {ai_charge(4);ai_melee();dfury_sword_sound();};
void()    dfury_lunge12    =[    $lunge12,    dfury_lunge13    ] {ai_charge(1);ai_melee();};
void()    dfury_lunge13    =[    $lunge13,    dfury_lunge14    ] {ai_charge(1);ai_melee();monster_footstep(FALSE);};
void()    dfury_lunge14    =[    $lunge14,    dfury_lunge15    ] {ai_charge(1);};
void()    dfury_lunge15    =[    $lunge15,    dfury_run1    ] {ai_resetangles();
    // If close enough starting swinging left+right ELSE return to run cycle
    if (ai_checkmelee(MONAI_MELEEFRONT) && self.enemy.health > 0) self.think = dfury_rswing3;
};

//----------------------------------------------------------------------
// Melee Attack 5 - W_Attack (two handed slashing attack)
// Identical frame set (Hell Knight / Death Knight)
// Speed boost (+16 charge) to all frames, fast charging attack!

void()    dfury_watk1    =[    $w_attack2,    dfury_watk3    ] {ai_charge(18);monster_footstep(FALSE);};
void()    dfury_watk3    =[    $w_attack3,    dfury_watk4    ] {ai_charge(18);};
void()    dfury_watk4    =[    $w_attack4,    dfury_watk5    ] {ai_charge(18);};
void()    dfury_watk5    =[    $w_attack5,    dfury_watk6    ] {ai_charge(18);dfury_grunt_sound(DFURY_SOUND_RUNSWING);};
void()    dfury_watk6    =[    $w_attack6,    dfury_watk7    ] {ai_charge(18);};
void()    dfury_watk7    =[    $w_attack7,    dfury_watk8    ] {ai_charge(19);monster_footstep(FALSE);};
void()    dfury_watk8    =[    $w_attack8,    dfury_watk9    ] {ai_charge(22);dfury_sword_sound();ai_melee();};
void()    dfury_watk9    =[    $w_attack9,    dfury_watk10] {ai_charge(23);ai_melee(); };
void()    dfury_watk10=[    $w_attack10,dfury_watk11] {ai_charge(21); ai_melee();};
void()    dfury_watk11=[    $w_attack11,dfury_watk12] {ai_charge(19);};
void()    dfury_watk12=[    $w_attack12,dfury_watk13] {ai_charge(19);};
void()    dfury_watk13=[    $w_attack13,dfury_watk14] {ai_charge(18);};
void()    dfury_watk14=[    $w_attack14,dfury_watk15] {ai_charge(18);dfury_grunt_sound(DFURY_SOUND_OPTIONAL);};
void()    dfury_watk15=[    $w_attack15,dfury_watk16] {ai_charge(18);};
void()    dfury_watk16=[    $w_attack16,dfury_watk17] {ai_charge(19);dfury_sword_sound();ai_melee();};
void()    dfury_watk17=[    $w_attack17,dfury_watk18] {ai_charge(19); monster_footstep(FALSE); ai_melee();};
void()    dfury_watk18=[    $w_attack18,dfury_watk19] {ai_charge(21); ai_melee();};
void()    dfury_watk19=[    $w_attack19,dfury_watk20] {ai_charge(22);};
void()    dfury_watk20=[    $w_attack20,dfury_watk21] {ai_charge(24);};
void()    dfury_watk21=[    $w_attack21,dfury_watk22] {ai_charge(25);};
void()    dfury_watk22=[    $w_attack22,dfury_run1    ] {ai_charge(21);
    // this is a long animations set for chasing the player
    // Quick check if within melee range, close melee range or random magic attack!
    if (self.enemy.health > 0) {
        if (ai_checkmelee(MONAI_MELEEFRONT)) self.think = dfury_rswing3;
        else if (ai_checkmelee(MONAI_JUMPFURYNEAR)) self.think = dfury_slice1;
        else if (random() < 0.3) self.think = dfury_magic;
    }
};

//----------------------------------------------------------------------
void() dfury_melee =
{
    // Use heavy overhead smash for monsters
    if (self.enemy.flags & FL_MONSTER) dfury_smash1();
    else {
        // Make sure next attack is different to previous (randomly while loop)
        self.lip = self.meleeattack;
        while (self.lip == self.meleeattack) { self.lip = rint(random()*2);    }
        self.meleeattack = self.lip;
        if (self.meleeattack == 0) dfury_lswing1();            // Quick left
        else if (self.meleeattack == 1) dfury_rswing1();    // Quick right
        else dfury_smash1();                                // Overhead smash
    }
};

//===========================================================================
// Magic attacks (glowing swords)
//===========================================================================
// Magic Attack A - Spray of spikes from raised up swords
//----------------------------------------------------------------------
void(float offset) dfury_magica_shot =
{
    local float shotcount;
    local vector soffset, dir, offang;

    if (!self.enemy) return;
    if (self.health < 1) return;

    // Single sound per volley of bullets
    sound (self, CHAN_WEAPON, "hknight/attack1.wav", 1, ATTN_NORM);

    // Randomize quantity each time called
    if (random() < 0.5) shotcount = 2;
    else shotcount = 3;

    // based angle of projectiles on straight line between source and target
    offang = vectoangles (self.enemy.origin - self.origin);
    offang_y = offang_y + offset * 6;        // Arc offset
    makevectors (offang);

    while (shotcount > 0) {
        // Randomly spawn projectiles up and down sword length
        soffset = v_forward * 20 + v_up * random() * 20;

        // Double check that missile is not blocked by any world geo
        if (!visxray(self.enemy, soffset, '0 0 0', TRUE)) soffset = soffset + '0 0 24';
        soffset = self.origin + soffset;

        // Straight line velocity from source to target
        dir = normalize (v_forward);
        dir_z = 0 - dir_z + (random() - 0.5)*0.1;    // Slight wiggle up/down
        
        // Variable speed based on skill level with extra random spice
        self.attack_speed = SPEED_DFURYSPIKE + (skill * SPEED_DFURYSKILL) + random()*10;
        launch_projectile (soffset, dir, CT_PROJ_FURY2, self.attack_speed);
        shotcount = shotcount - 1;
    }
};

//----------------------------------------------------------------------
// Magic Attack B - 3 x rockets launched in an arc
//----------------------------------------------------------------------
void() dfury_magicb_shot =
{
    local float offset, shotcount;
    local vector soffset, dir, offang, avel;

    if (!self.enemy) return;
    if (self.health < 1) return;

    // Single sound for all rockets fired
    sound (self, CHAN_WEAPON, "hknight/attack1.wav", 1, ATTN_NORM);

    // create an arc of rockets fanning outwards from source
    offset = -4; shotcount = 3;
    while (shotcount > 0) {
    // based angle of projectiles on straight line between source and target
        offang = vectoangles (self.enemy.origin - self.origin);
        offang_y = offang_y + offset;
        makevectors (offang);
    
        // Randomly spawn projectiles around central point
        soffset = v_forward * 20 + v_up * (5 + random() * 10);
        
        // Double check that missile is not blocked by any world geo
        if (!visxray(self.enemy, soffset, '0 0 0', TRUE)) soffset = soffset + '0 0 24';
        soffset = self.origin + soffset;
    
        // Straight line velocity from source to target
        dir = normalize (v_forward);
        dir_z = 0 - dir_z + (random() - 0.5)*0.1;    // Slight wiggle up/down
        avel = vecrand(100,200,FALSE);
        
        // Variable speed based on skill level with extra random spice
        self.attack_speed = SPEED_DFURYSPIKE + (skill * SPEED_DFURYSKILL) + random()*10;
        Launch_Missile (soffset, dir, avel, CT_PROJ_FURY1, self.attack_speed );
        offset = offset + 4;
        shotcount = shotcount - 1;
    }
};

//----------------------------------------------------------------------
void() dfury_attachment_finish =
{
    if (self.state) {
        self.state = FALSE;
        self.alpha = 0.1;
        setmodel(self, "");
    }
};

//----------------------------------------------------------------------
void(float skincolor) dfury_attachment_setup =
{
    if (self.attachment && !self.attachment.state) {
        self.attachment.state = TRUE;
        setmodel(self.attachment, self.weaponglow);
        self.attachment.alpha = 0.1;
        self.attachment.skin = skincolor;
        setorigin(self.attachment, self.origin);
        self.attachment.angles = self.angles;
        self.attachment.nextthink = time + 0.15;
        self.attachment.think = dfury_attachment_finish;
    }
};

//----------------------------------------------------------------------
void() dfury_attachment_update =
{        
    if (self.attachment && self.attachment.state) {
        setorigin(self.attachment, self.origin);
        self.attachment.angles = self.angles;
        self.attachment.frame = self.frame;
        self.attachment.alpha = 0.3 + random()*0.7;
        self.attachment.nextthink = time + 0.15;
        self.attachment.think = dfury_attachment_finish;
    }
};

//----------------------------------------------------------------------
// Magic Attack A - Spray of spikes from raised up swords
// Block pain function from interrupting magic attack
void()    dfury_magica1 =[    $magica2,    dfury_magica2    ] {ai_face();self.pain_finished = time + 2;};
void()    dfury_magica2 =[    $magica3,    dfury_magica3    ] {ai_face();};
void()    dfury_magica3 =[    $magica4,    dfury_magica4    ] {ai_face();};
void()    dfury_magica4 =[    $magica5,    dfury_magica5    ] {ai_face();};
void()    dfury_magica5 =[    $magica6,    dfury_magica6    ] {ai_charge(2);dfury_attachment_setup(0);};
void()    dfury_magica6 =[    $magica7,    dfury_magica7    ] {ai_charge(2);dfury_attachment_update();};
void()    dfury_magica7 =[    $magica8,    dfury_magica8    ] {ai_charge(2);dfury_attachment_update();};
void()    dfury_magica8 =[    $magica9,    dfury_magica9    ] {ai_charge(2);dfury_attachment_update();};
void()    dfury_magica9 =[    $magica10,    dfury_magica10] {ai_charge(2);dfury_attachment_update();};
void()    dfury_magica10 =[    $magica11,    dfury_magica11] {dfury_attachment_update();dfury_magica_shot(-3);};
void()    dfury_magica11 =[    $magica12,    dfury_magica12] {dfury_attachment_update();dfury_magica_shot(-1);};
void()    dfury_magica12 =[    $magica13,    dfury_magica13] {dfury_attachment_update();dfury_magica_shot(0);};
void()    dfury_magica13 =[    $magica14,    dfury_magica14] {dfury_attachment_update();dfury_magica_shot(1);};
void()    dfury_magica14 =[    $magica15,    dfury_magica15] {dfury_attachment_update();dfury_magica_shot(2);};
void()    dfury_magica15 =[    $magica16,    dfury_run1] {};

//----------------------------------------------------------------------
// Magic Attack B - 3 x rockets launched in an arc
// Block pain function from interrupting magic attack
void()    dfury_magicb1 =[    $magicb1,    dfury_magicb2    ] {self.pain_finished = time + 2;};
void()    dfury_magicb2 =[    $magicb2,    dfury_magicb3    ] {ai_face();dfury_attachment_setup(1);};
void()    dfury_magicb3 =[    $magicb3,    dfury_magicb4    ] {ai_face();dfury_attachment_update();};
void()    dfury_magicb4 =[    $magicb4,    dfury_magicb5    ] {ai_face();dfury_attachment_update();};
void()    dfury_magicb5 =[    $magicb5,    dfury_magicb6    ] {ai_face();dfury_attachment_update();};
void()    dfury_magicb6 =[    $magicb6,    dfury_magicb7    ] {ai_face();dfury_attachment_update();};
void()    dfury_magicb7 =[    $magicb7,    dfury_magicb8    ] {ai_face();dfury_attachment_update();};
void()    dfury_magicb8 =[    $magicb8,    dfury_magicb9    ] {ai_face();dfury_attachment_update();};
void()    dfury_magicb9 =[    $magicb9,    dfury_magicb10] {ai_face();dfury_attachment_update();dfury_magicb_shot();};
void()    dfury_magicb10 =[    $magicb10,    dfury_magicb11] {dfury_attachment_update();};
void()    dfury_magicb11 =[    $magicb11,    dfury_magicb12] {};
void()    dfury_magicb12 =[    $magicb12,    dfury_magicb13] {};
void()    dfury_magicb13 =[    $magica13,    dfury_run1] {};

//----------------------------------------------------------------------
// MagicB is explosive so only use if there is range available
// Otherwise randomly pick between the two types (spikes/rockets)
//----------------------------------------------------------------------
void() dfury_magic =
{
    SUB_AttackFinished (2 + random());
    self.enemydist = range_distance(self.enemy, FALSE);
    if (self.enemydist < MONAI_JUMPFURYNEAR) dfury_magica1();
    else {
        if (random() < 0.2) dfury_magica1();
        else dfury_magicb1();
    }
};

//============================================================================
void()    dfury_painA1    =[    $painA1,        dfury_painA2    ] {};
void()    dfury_painA2    =[    $painA2,        dfury_painA3    ] {};
void()    dfury_painA3    =[    $painA3,        dfury_painA4    ] {};
void()    dfury_painA4    =[    $painA4,        dfury_painA5    ] {};
void()    dfury_painA5    =[    $painA5,        dfury_run1    ] {};

//----------------------------------------------------------------------
void()    dfury_painD1    =[    $painD1,        dfury_painD2    ] {};
void()    dfury_painD2    =[    $painD2,        dfury_painD3    ] {};
void()    dfury_painD3    =[    $painD3,        dfury_painD4    ] {};
void()    dfury_painD4    =[    $painD4,        dfury_painD5    ] {monster_footstep(FALSE); };
void()    dfury_painD5    =[    $painD5,        dfury_painD6    ] {};
void()    dfury_painD6    =[    $painD6,        dfury_painD7    ] {};
void()    dfury_painD7    =[    $painD7,        dfury_painD8    ] {};
void()    dfury_painD8    =[    $painD8,        dfury_painD9    ] {monster_footstep(FALSE); };
void()    dfury_painD9    =[    $painD9,        dfury_painD10    ] {};
void()    dfury_painD10    =[    $painD10,        dfury_painD11    ] {};
void()    dfury_painD11    =[    $painD11,        dfury_run1    ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage)    dfury_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);
    
    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1) {
            // Randomly pick which pain animation to play
            if (random() < 0.9) dfury_painA1 ();        // classic, body recoil
            else {
                dfury_painD1 ();                        // Cool stumble
                self.pain_finished = time + 2;            // long animation
            }
        }
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 1.1;
            self.axhitme = 0;
            dfury_painD1 ();
        }
    }
};

//============================================================================
void()    dfury_die1    =[    $death2,    dfury_die2    ] {ai_forward(10);};
void()    dfury_die2    =[    $death3,    dfury_die3    ] {monster_check_gib();ai_forward(8);};
void()    dfury_die3    =[    $death4,    dfury_die4    ] {monster_check_gib();
    self.solid = SOLID_NOT; ai_forward(7);};
void()    dfury_die4    =[    $death5,    dfury_die5    ] {};
void()    dfury_die5    =[    $death6,    dfury_die6    ] {};
void()    dfury_die6    =[    $death7,    dfury_die7    ] {
    sound (self, CHAN_WEAPON, GIB_SOUND_METALA, 1, ATTN_NORM);};
void()    dfury_die7    =[    $death8,    dfury_die8    ] {};
void()    dfury_die8    =[    $death9,    dfury_die9    ] {ai_forward(10);};
void()    dfury_die9    =[    $death10,    dfury_die10    ] {ai_forward(11);};
void()    dfury_die10    =[    $death11,    dfury_die11    ] {};
void()    dfury_die11    =[    $death12,    dfury_die12    ] {};
void()    dfury_die12    =[    $death13,    dfury_die13    ] {};
void()    dfury_die13    =[    $death14,    dfury_die14    ] {};
void()    dfury_die14    =[    $death15,    dfury_die15    ] {
    sound (self, CHAN_WEAPON, GIB_SOUND_METALA, 1, ATTN_NORM);};
void()    dfury_die15    =[    $death16,    dfury_die16    ] {monster_death_postcheck();};
void()    dfury_die16    =[    $death17,    dfury_die16    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void()    dfury_dieb1    =[    $deathb1,    dfury_dieb2    ] {};
void()    dfury_dieb2    =[    $deathb2,    dfury_dieb3    ] {monster_check_gib();};
void()    dfury_dieb3    =[    $deathb3,    dfury_dieb4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void()    dfury_dieb4    =[    $deathb4,    dfury_dieb5    ] {};
void()    dfury_dieb5    =[    $deathb5,    dfury_dieb6    ] {};
void()    dfury_dieb6    =[    $deathb6,    dfury_dieb7    ] {
    sound (self, CHAN_WEAPON, GIB_SOUND_METALB, 1, ATTN_NORM);};
void()    dfury_dieb7    =[    $deathb7,    dfury_dieb8    ] {};
void()    dfury_dieb8    =[    $deathb8,    dfury_dieb9    ] {};
void()    dfury_dieb9    =[    $deathb9,    dfury_dieb10] {};
void()    dfury_dieb10=[    $deathb10,    dfury_dieb11] {};
void()    dfury_dieb11=[    $deathb11,    dfury_dieb12] {};
void()    dfury_dieb12=[    $deathb12,    dfury_dieb13] {};
void()    dfury_dieb13=[    $deathb13,    dfury_dieb14] {monster_death_postcheck();};
void()    dfury_dieb14=[    $deathb14,    dfury_dieb14] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() dfury_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "hknight/death1.wav", 1, ATTN_NORM);
        if (random() > 0.5) dfury_die1 ();
        else dfury_dieb1 ();
    }
};

//============================================================================
void() dfury_sword_sound =
{
    self.weaponstate = random();
    if (self.weaponstate < 0.25) sound (self, CHAN_WEAPON, "weapons/sword1a.wav", 1, ATTN_NORM);
    else if (self.weaponstate < 0.5) sound (self, CHAN_WEAPON, "weapons/sword2a.wav", 1, ATTN_NORM);
    else if (self.weaponstate < 0.75) sound (self, CHAN_WEAPON, "weapons/sword1b.wav", 1, ATTN_NORM);
    else sound (self, CHAN_WEAPON, "weapons/sword2b.wav", 1, ATTN_NORM);
};

//============================================================================
void(float soundtype) dfury_grunt_sound =
{
    if (soundtype == DFURY_SOUND_PULLAPART)
        sound (self, CHAN_WEAPON, "dknight/dfury_grunt3.wav", 1, ATTN_NORM);
    else if (soundtype == DFURY_SOUND_OVERSMASH)
        sound (self, CHAN_WEAPON, "dknight/dfury_grunt4.wav", 1, ATTN_NORM);
    else if (soundtype == DFURY_SOUND_JUMP)
        sound (self, CHAN_WEAPON, "dknight/dfury_leap.wav", 1, ATTN_NORM);
    else {
        if (soundtype == DFURY_SOUND_OPTIONAL && random() < 0.8) return;
        self.weaponstate = random();
        self.weaponswitch = 0.6 + random()*0.4;
        if (self.weaponstate < 0.25) sound (self, CHAN_WEAPON, "dknight/dfury_grunt1.wav", self.weaponswitch, ATTN_NORM);
        else if (self.weaponstate < 0.5) sound (self, CHAN_WEAPON, "dknight/dfury_grunt2.wav", self.weaponswitch, ATTN_NORM);
        else if (self.weaponstate < 0.75) sound (self, CHAN_WEAPON, "dknight/dfury_grunt3.wav", self.weaponswitch, ATTN_NORM);
        else sound (self, CHAN_WEAPON, "dknight/dfury_grunt4.wav", self.weaponswitch, ATTN_NORM);
    }
};

/*======================================================================
QUAKED monster_dfury (1 0 0) (-16 -16 -24) (16 16 40) Ambush
======================================================================*/
void() monster_dfury =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_dfury.mdl";
    self.headmdl = "progs/ad171/h_dfury.mdl";
    self.weaponglow = "progs/ad171/mon_dfury_glow.mdl";
    self.gib1mdl = "progs/ad171/w_dfurysword1.mdl";        // Unique sword L
    self.gib2mdl = "progs/ad171/w_dfurysword2.mdl";        // Unique sword R
    self.gib3mdl = "progs/ad171/gib_knfoot_r.mdl";        // right foot
    
    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (self.weaponglow);        // Glowing magical swords
    precache_model (MODEL_PROJ_FURY1);    // Vore spike ball with smoke trail
    precache_model (MODEL_PROJ_FURY2);    // Wizard spit with smoke trail
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);

    self.gib3skin = 2;
    self.gib1sound = self.gib2sound = GIB_IMPACT_METALA;

    self.idle_sound = "hknight/idle.wav";
    precache_sound (self.idle_sound);

    // Firing of magic spikes (first 0.1s must loop)
    precache_sound ("hknight/attack1.wav");
    precache_sound ("hknight/death1.wav");
    self.pain_sound = "hknight/pain1.wav";
    precache_sound (self.pain_sound);

    // TE_KNIGHTSPIKE = Magic spike hitting the wall
    // Has got to be in this location because the filename is hardcoded (engine.exe)
    precache_sound ("hknight/hit.wav");                

    // Double sword swipe (like death knight++)
    self.sight_sound = "dknight/dfury_sight.wav";
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
    precache_sound ("dknight/dfury_grunt1.wav");
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
    if (self.health < 1) self.health = 400;
    self.gibhealth = -50;
    self.gibbed = FALSE;
    self.pain_flinch = 80;
    self.pain_longanim = TRUE;            // can be chopped with shadow axe
    self.pain_timeout = 2;                // High pain level
    if (!self.infightextra) self.infightextra = 2;    // 2x infighting dmg
    self.steptype = FS_TYPEMEDIUM;
    self.meleecontact = FALSE;            // Smash contact
    self.meleerange = MONAI_MELEEFRONT;    // Smash attack
    self.meleeoffset = '24 0 16';        // Jump attack offset
    self.attack_offset = '20 0 10';        // Average of both attacks
    if (self.deathstring == "") self.deathstring = " was sliced by a Fury Knight\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    self.th_checkattack = DFuryCheckAttack;
    self.th_stand = dfury_standL1;
    self.th_walk = dfury_walk1;
    self.th_run = dfury_run1;
    self.th_melee = dfury_melee;
    self.th_missile = dfury_magic;
    self.th_pain = dfury_pain;
    self.th_die = dfury_die;
    self.th_jump = dfury_lunge1;
    self.th_jumpexit = dfury_lunge9;
    self.th_charge = dfury_watk1;
    self.th_slide = dfury_slice1;
    self.hittype = HIT_METAL;
    
    self.meleeattack = rint(random()*5);
    self.weaponswitch = time + 5;
    
    self.classtype = CT_MONDFURY;
    self.classgroup = CG_DCAPTAIN;
    self.classmove = MON_MOVEWALK;
    
    // Setup magic glowing sword effect
    self.attachment = spawn();
    self.attachment.owner = self;
    self.attachment.classtype = CT_ATTACHMENT;
    setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);
    self.attachment.movetype = MOVETYPE_NONE;
    self.attachment.solid = SOLID_NOT;
    setorigin(self.attachment, self.origin);
    
    monster_start();
};
