/*==============================================================================
CROSSBOW KNIGHT (low health, good range attack)
==============================================================================*/
$cd id1/models/dcrossbow
$origin 0 0 24
$base base
$skin skin

// (001 - 010) Default stand - breathing
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8
$frame stand9 stand10

// (011 - 032) Stand idle - Look left and right checking for the player
$frame standA1 standA2 standA3 standA4 standA5 standA6 standA7 standA8 
$frame standA9 standA10 standA11 standA12 standA13 standA14 standA15 standA16
$frame standA17 standA18 standA19 standA20 standA21 standA22

// (033 - 052) Stand idle - lift crossbow up and look around
$frame standB1 standB2 standB3 standB4 standB5 standB6 standB7 standB8 
$frame standB9 standB10 standB11 standB12 standB13 standB14 standB15 standB16
$frame standB17 standB18 standB19 standB20

// (053 - 082) Stand idle - check out crossbow up close
$frame standD1 standD2 standD3 standD4 standD5 standD6 standD7 standD8 
$frame standD9 standD10 standD11 standD12 standD13 standD14 standD15 standD16
$frame standD17 standD18 standD19 standD20 standD21 standD22 standD23 standD24
$frame standD25 standD26 standD27 standD28 standD29 standD30

// (083 - 103) Stand idle - Lower crossbow and scratch back
$frame standE1 standE2 standE3 standE4 standE5 standE6 standE7 standE8 
$frame standE9 standE10 standE11 standE12 standE13 standE14 standE15 standE16
$frame standE17 standE18 standE19 standE20 standE21

// (104 - 114) Stand idle - Dust off the crossbow
$frame standF1 standF2 standF3 standF4 standF5 standF6 standF7 standF8 
$frame standF9 standF10 standF11

// (115 - 128) Default walk - strong walk with crossbow infront
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9
$frame walk10 walk11 walk12 walk13 walk14

// (129 - 134) Default run
$frame run1 run2 run3 run4 run5 run6

// (135 - 146) Slice (stationary one hand slashing attack) (file=SWING)
$frame slice1 slice2 slice3 slice4 slice5 slice6 slice7 slice8
$frame slice9 slice10 slice11 slice12

// (147 - 161) Smash (chop overhead to ground) (file=CHOP)
$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7 smash8
$frame smash9 smash10 smash11 smash12 smash13 smash14 smash15

// (162 - 168) Turret ready (brings crossbow up to shoulder height)
$frame tready1 tready2 tready3 tready4 tready5 tready6 tready7

// (169 - 174) Turret hold (keeps crossbow up high)
$frame thold1 thold2 thold3 thold4 thold5 thold6

// (175 - 193) Turret fire (fires crossbow and reload) (19 frames now)
// Removed - 4,6,8,10, 17,19,21,23, 25,27,29,31
$frame tfire1 tfire2 tfire3 tfire5 tfire7
$frame tfire9 tfire11 tfire12 tfire13 tfire14 tfire15 tfire16
$frame tfire18 tfire20 tfire22 tfire24
$frame tfire26 tfire28 tfire30

// (194 - 201) Turret stop (lower crossbow to relaxed position)
$frame tstop1 tstop2 tstop3 tstop4 tstop5 tstop6 tstop7 tstop8

// (202 - 209) Turret slide (strafe side to side movement, crossbow up high)
$frame tslide1 tslide2 tslide3 tslide4 tslide5 tslide6 tslide7 tslide8

// Pain animations A = Slow (210 - 214), D = Stagger (215 - 225)
$frame painA1 painA2 painA3 painA4 painA5
$frame painD1 painD2 painD3 painD4 painD5 painD6 painD7 painD8 painD9 painD10 painD11

// (226 - 236) Death forward
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11

// (237 - 256) Death backwards with cool crossbow wobble
$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9 deathb10 deathb11 deathb12 deathb13 deathb14 deathb15 deathb16
$frame deathb17 deathb18 deathb19 deathb20

void() dcrossbow_hold1;
void() dcrossbow_slide1;
void() dcrossbow_checkmelee;
void() dcrossbow_crossbow_sound;
float CROSS_DOWN = 1;
float CROSS_UP = 2;
float CROSS_TIMEOUT = 1;

//===========================================================================
void()    dcrossbow_standA1    =[    $standA1,    dcrossbow_standA2    ] {ai_stand();};
void()    dcrossbow_standA2    =[    $standA2,    dcrossbow_standA3    ] {ai_stand();};
void()    dcrossbow_standA3    =[    $standA3,    dcrossbow_standA4    ] {ai_stand();};
void()    dcrossbow_standA4    =[    $standA4,    dcrossbow_standA5    ] {ai_stand();};
void()    dcrossbow_standA5    =[    $standA5,    dcrossbow_standA6    ] {ai_stand();};
void()    dcrossbow_standA6    =[    $standA6,    dcrossbow_standA7    ] {ai_stand();};
void()    dcrossbow_standA7    =[    $standA7,    dcrossbow_standA8    ] {ai_stand();};
void()    dcrossbow_standA8    =[    $standA8,    dcrossbow_standA9    ] {ai_stand();};
void()    dcrossbow_standA9    =[    $standA9,    dcrossbow_standA10    ] {ai_stand();};
void()    dcrossbow_standA10    =[    $standA10,    dcrossbow_standA11    ] {ai_stand();};
void()    dcrossbow_standA11    =[    $standA11,    dcrossbow_standA12    ] {ai_stand();};
void()    dcrossbow_standA12    =[    $standA12,    dcrossbow_standA13    ] {ai_stand();};
void()    dcrossbow_standA13    =[    $standA13,    dcrossbow_standA14    ] {ai_stand();};
void()    dcrossbow_standA14    =[    $standA14,    dcrossbow_standA15    ] {ai_stand();};
void()    dcrossbow_standA15    =[    $standA15,    dcrossbow_standA16    ] {ai_stand();};
void()    dcrossbow_standA16    =[    $standA16,    dcrossbow_standA17    ] {ai_stand();};
void()    dcrossbow_standA17    =[    $standA17,    dcrossbow_standA18    ] {ai_stand();};
void()    dcrossbow_standA18    =[    $standA18,    dcrossbow_standA19    ] {ai_stand();};
void()    dcrossbow_standA19    =[    $standA19,    dcrossbow_standA20    ] {ai_stand();};
void()    dcrossbow_standA20    =[    $standA20,    dcrossbow_standA21    ] {ai_stand();};
void()    dcrossbow_standA21    =[    $standA21,    dcrossbow_standA22    ] {ai_stand();};
void()    dcrossbow_standA22    =[    $standA22,    dcrossbow_stand1    ] {ai_stand();};

//===========================================================================
void()    dcrossbow_standB1    =[    $standB1,    dcrossbow_standB2    ] {ai_stand();};
void()    dcrossbow_standB2    =[    $standB2,    dcrossbow_standB3    ] {ai_stand();};
void()    dcrossbow_standB3    =[    $standB3,    dcrossbow_standB4    ] {ai_stand();};
void()    dcrossbow_standB4    =[    $standB4,    dcrossbow_standB5    ] {ai_stand();};
void()    dcrossbow_standB5    =[    $standB5,    dcrossbow_standB6    ] {ai_stand();};
void()    dcrossbow_standB6    =[    $standB6,    dcrossbow_standB7    ] {ai_stand();};
void()    dcrossbow_standB7    =[    $standB7,    dcrossbow_standB8    ] {ai_stand();};
void()    dcrossbow_standB8    =[    $standB8,    dcrossbow_standB9    ] {ai_stand();};
void()    dcrossbow_standB9    =[    $standB9,    dcrossbow_standB10    ] {ai_stand();};
void()    dcrossbow_standB10    =[    $standB10,    dcrossbow_standB11    ] {ai_stand();};
void()    dcrossbow_standB11    =[    $standB11,    dcrossbow_standB12    ] {ai_stand();};
void()    dcrossbow_standB12    =[    $standB12,    dcrossbow_standB13    ] {ai_stand();};
void()    dcrossbow_standB13    =[    $standB13,    dcrossbow_standB14    ] {ai_stand();};
void()    dcrossbow_standB14    =[    $standB14,    dcrossbow_standB15    ] {ai_stand();};
void()    dcrossbow_standB15    =[    $standB15,    dcrossbow_standB16    ] {ai_stand();};
void()    dcrossbow_standB16    =[    $standB16,    dcrossbow_standB17    ] {ai_stand();};
void()    dcrossbow_standB17    =[    $standB17,    dcrossbow_standB18    ] {ai_stand();};
void()    dcrossbow_standB18    =[    $standB18,    dcrossbow_standB19    ] {ai_stand();};
void()    dcrossbow_standB19    =[    $standB19,    dcrossbow_standB20    ] {ai_stand();};
void()    dcrossbow_standB20    =[    $standB20,    dcrossbow_stand1    ] {ai_stand();};

//===========================================================================
void()    dcrossbow_standC1    =[    $tready1,    dcrossbow_standC2    ] {ai_stand();};
void()    dcrossbow_standC2    =[    $tready2,    dcrossbow_standC3    ] {ai_stand();};
void()    dcrossbow_standC3    =[    $tready3,    dcrossbow_standC4    ] {ai_stand();};
void()    dcrossbow_standC4    =[    $tready4,    dcrossbow_standC5    ] {ai_stand();};
void()    dcrossbow_standC5    =[    $tready5,    dcrossbow_standC6    ] {ai_stand();};
void()    dcrossbow_standC6    =[    $tready6,    dcrossbow_standC7    ] {ai_stand();};
void()    dcrossbow_standC7    =[    $tready7,    dcrossbow_standC8    ] {ai_stand();};
void()    dcrossbow_standC8    =[    $thold1,    dcrossbow_standC9    ] {ai_stand();};
void()    dcrossbow_standC9    =[    $thold2,    dcrossbow_standC10    ] {ai_stand();};
void()    dcrossbow_standC10    =[    $thold3,    dcrossbow_standC11    ] {ai_stand();};
void()    dcrossbow_standC11    =[    $thold4,    dcrossbow_standC12    ] {ai_stand();};
void()    dcrossbow_standC12    =[    $thold5,    dcrossbow_standC13    ] {ai_stand();};
void()    dcrossbow_standC13    =[    $thold6,    dcrossbow_standC14    ] {ai_stand();};
void()    dcrossbow_standC14    =[    $tstop1,    dcrossbow_standC15    ] {ai_stand();};
void()    dcrossbow_standC15    =[    $tstop2,    dcrossbow_standC16    ] {ai_stand();};
void()    dcrossbow_standC16    =[    $tstop3,    dcrossbow_standC17    ] {ai_stand();};
void()    dcrossbow_standC17    =[    $tstop4,    dcrossbow_standC18    ] {ai_stand();};
void()    dcrossbow_standC18    =[    $tstop5,    dcrossbow_standC19    ] {ai_stand();};
void()    dcrossbow_standC19    =[    $tstop6,    dcrossbow_stand1    ] {ai_stand();};

//===========================================================================
void()    dcrossbow_standD1    =[    $standD1,    dcrossbow_standD2    ] {ai_stand();};
void()    dcrossbow_standD2    =[    $standD2,    dcrossbow_standD3    ] {ai_stand();};
void()    dcrossbow_standD3    =[    $standD3,    dcrossbow_standD4    ] {ai_stand();};
void()    dcrossbow_standD4    =[    $standD4,    dcrossbow_standD5    ] {ai_stand();};
void()    dcrossbow_standD5    =[    $standD5,    dcrossbow_standD6    ] {ai_stand();};
void()    dcrossbow_standD6    =[    $standD6,    dcrossbow_standD7    ] {ai_stand();};
void()    dcrossbow_standD7    =[    $standD7,    dcrossbow_standD8    ] {ai_stand();};
void()    dcrossbow_standD8    =[    $standD8,    dcrossbow_standD9    ] {ai_stand();};
void()    dcrossbow_standD9    =[    $standD9,    dcrossbow_standD10    ] {ai_stand();};
void()    dcrossbow_standD10    =[    $standD10,    dcrossbow_standD11    ] {ai_stand();};
void()    dcrossbow_standD11    =[    $standD11,    dcrossbow_standD12    ] {ai_stand();};
void()    dcrossbow_standD12    =[    $standD12,    dcrossbow_standD13    ] {ai_stand();};
void()    dcrossbow_standD13    =[    $standD13,    dcrossbow_standD14    ] {ai_stand();};
void()    dcrossbow_standD14    =[    $standD14,    dcrossbow_standD15    ] {ai_stand();};
void()    dcrossbow_standD15    =[    $standD15,    dcrossbow_standD16    ] {ai_stand();};
void()    dcrossbow_standD16    =[    $standD16,    dcrossbow_standD17    ] {ai_stand();};
void()    dcrossbow_standD17    =[    $standD17,    dcrossbow_standD18    ] {ai_stand();};
void()    dcrossbow_standD18    =[    $standD18,    dcrossbow_standD19    ] {ai_stand();};
void()    dcrossbow_standD19    =[    $standD19,    dcrossbow_standD20    ] {ai_stand();};
void()    dcrossbow_standD20    =[    $standD20,    dcrossbow_standD21    ] {ai_stand();};
void()    dcrossbow_standD21    =[    $standD21,    dcrossbow_standD22    ] {ai_stand();};
void()    dcrossbow_standD22    =[    $standD22,    dcrossbow_standD23    ] {ai_stand();};
void()    dcrossbow_standD23    =[    $standD23,    dcrossbow_standD24    ] {ai_stand();};
void()    dcrossbow_standD24    =[    $standD24,    dcrossbow_standD25    ] {ai_stand();};
void()    dcrossbow_standD25    =[    $standD25,    dcrossbow_standD26    ] {ai_stand();};
void()    dcrossbow_standD26    =[    $standD26,    dcrossbow_standD27    ] {ai_stand();};
void()    dcrossbow_standD27    =[    $standD27,    dcrossbow_standD28    ] {ai_stand();};
void()    dcrossbow_standD28    =[    $standD28,    dcrossbow_standD29    ] {ai_stand();};
void()    dcrossbow_standD29    =[    $standD29,    dcrossbow_standD30    ] {ai_stand();};
void()    dcrossbow_standD30    =[    $standD30,    dcrossbow_stand1    ] {ai_stand();};

//===========================================================================
void()    dcrossbow_standE1    =[    $standE1,    dcrossbow_standE2    ] {ai_stand();};
void()    dcrossbow_standE2    =[    $standE2,    dcrossbow_standE3    ] {ai_stand();};
void()    dcrossbow_standE3    =[    $standE3,    dcrossbow_standE4    ] {ai_stand();};
void()    dcrossbow_standE4    =[    $standE4,    dcrossbow_standE5    ] {ai_stand();};
void()    dcrossbow_standE5    =[    $standE5,    dcrossbow_standE6    ] {ai_stand();};
void()    dcrossbow_standE6    =[    $standE6,    dcrossbow_standE7    ] {ai_stand();};
void()    dcrossbow_standE7    =[    $standE7,    dcrossbow_standE8    ] {ai_stand();};
void()    dcrossbow_standE8    =[    $standE8,    dcrossbow_standE9    ] {ai_stand();};
void()    dcrossbow_standE9    =[    $standE9,    dcrossbow_standE10    ] {ai_stand();};
void()    dcrossbow_standE10    =[    $standE10,    dcrossbow_standE11    ] {ai_stand();};
void()    dcrossbow_standE11    =[    $standE11,    dcrossbow_standE12    ] {ai_stand();};
void()    dcrossbow_standE12    =[    $standE12,    dcrossbow_standE13    ] {ai_stand();};
void()    dcrossbow_standE13    =[    $standE13,    dcrossbow_standE14    ] {ai_stand();};
void()    dcrossbow_standE14    =[    $standE14,    dcrossbow_standE15    ] {ai_stand();};
void()    dcrossbow_standE15    =[    $standE15,    dcrossbow_standE16    ] {ai_stand();};
void()    dcrossbow_standE16    =[    $standE16,    dcrossbow_standE17    ] {ai_stand();};
void()    dcrossbow_standE17    =[    $standE17,    dcrossbow_standE18    ] {ai_stand();};
void()    dcrossbow_standE18    =[    $standE18,    dcrossbow_standE19    ] {ai_stand();};
void()    dcrossbow_standE19    =[    $standE19,    dcrossbow_standE20    ] {ai_stand();};
void()    dcrossbow_standE20    =[    $standE20,    dcrossbow_standE21    ] {ai_stand();};
void()    dcrossbow_standE21    =[    $standE21,    dcrossbow_stand1    ] {ai_stand();};

//===========================================================================
void()    dcrossbow_standF1    =[    $standF1,    dcrossbow_standF2    ] {ai_stand();};
void()    dcrossbow_standF2    =[    $standF2,    dcrossbow_standF3    ] {ai_stand();};
void()    dcrossbow_standF3    =[    $standF3,    dcrossbow_standF4    ] {ai_stand();};
void()    dcrossbow_standF4    =[    $standF4,    dcrossbow_standF5    ] {ai_stand();};
void()    dcrossbow_standF5    =[    $standF5,    dcrossbow_standF6    ] {ai_stand();};
void()    dcrossbow_standF6    =[    $standF6,    dcrossbow_standF7    ] {ai_stand();};
void()    dcrossbow_standF7    =[    $standF7,    dcrossbow_standF8    ] {ai_stand();};
void()    dcrossbow_standF8    =[    $standF8,    dcrossbow_standF9    ] {ai_stand();};
void()    dcrossbow_standF9    =[    $standF9,    dcrossbow_standF10    ] {ai_stand();};
void()    dcrossbow_standF10    =[    $standF10,    dcrossbow_standF11    ] {ai_stand();};
void()    dcrossbow_standF11    =[    $standF11,    dcrossbow_stand1    ] {ai_stand();};

//===========================================================================
void()    dcrossbow_stand1    =[    $stand1,    dcrossbow_stand2    ] {monster_idle_sound();ai_stand();};
void()    dcrossbow_stand2    =[    $stand2,    dcrossbow_stand3    ] {ai_stand();};
void()    dcrossbow_stand3    =[    $stand3,    dcrossbow_stand4    ] {ai_stand();};
void()    dcrossbow_stand4    =[    $stand4,    dcrossbow_stand5    ] {ai_stand();};
void()    dcrossbow_stand5    =[    $stand5,    dcrossbow_stand6    ] {ai_stand();};
void()    dcrossbow_stand6    =[    $stand6,    dcrossbow_stand7    ] {ai_stand();};
void()    dcrossbow_stand7    =[    $stand7,    dcrossbow_stand8    ] {ai_stand();};
void()    dcrossbow_stand8    =[    $stand8,    dcrossbow_stand9    ] {ai_stand();};
void()    dcrossbow_stand9    =[    $stand9,    dcrossbow_stand10    ] {ai_stand();};
void()    dcrossbow_stand10    =[    $stand10,    dcrossbow_stand1    ] {
    self.idlebusy = FALSE; 
    if (random() < MON_IDLE_ANIMATION) {
        self.idlebusy = TRUE; 
        // Make sure the idle animations don't repeat in a row
        self.lefty = self.lip;
        while (self.lefty == self.lip) { self.lefty = rint(random()*6);}
        self.lip = self.lefty;
        if (self.lip < 1) self.think = dcrossbow_standE1;        // Scratch back
        else if (self.lip < 2) self.think = dcrossbow_standD1;    // Look at weapon
        else if (self.lip < 3) self.think = dcrossbow_standC1;    // Ready, steady, hold!
        else if (self.lip < 4) self.think = dcrossbow_standF1;    // Dust off crossbow
        else if (self.lip < 5) self.think = dcrossbow_standA1;    // Look around
        else self.think = dcrossbow_standB1;                                        // Hold crossbow up
    }
    ai_stand();
};

//===========================================================================
void()    dcrossbow_walk1    =[    $walk1,        dcrossbow_walk2    ] {monster_idle_sound(); 
    self.idlebusy = FALSE; ai_walk(2);};
void()    dcrossbow_walk2    =[    $walk2,        dcrossbow_walk3    ] {monster_footstep(FALSE);ai_walk(5);};
void()    dcrossbow_walk3    =[    $walk3,        dcrossbow_walk4    ] {ai_walk(5);};
void()    dcrossbow_walk4    =[    $walk4,        dcrossbow_walk5    ] {ai_walk(4);};
void()    dcrossbow_walk5    =[    $walk5,        dcrossbow_walk6    ] {ai_walk(4);};
void()    dcrossbow_walk6    =[    $walk6,        dcrossbow_walk7    ] {ai_walk(2);};
void()    dcrossbow_walk7    =[    $walk7,        dcrossbow_walk8    ] {ai_walk(2);};
void()    dcrossbow_walk8    =[    $walk8,        dcrossbow_walk9    ] {ai_walk(3); monster_footstep(FALSE);};
void()    dcrossbow_walk9    =[    $walk9,        dcrossbow_walk10] {ai_walk(3);};
void()    dcrossbow_walk10 =[ $walk10,    dcrossbow_walk11] {ai_walk(4);};
void()    dcrossbow_walk11    =[    $walk11,    dcrossbow_walk12    ] {ai_walk(3);};
void()    dcrossbow_walk12    =[    $walk12,    dcrossbow_walk13    ] {ai_walk(4);};
void()    dcrossbow_walk13    =[    $walk13,    dcrossbow_walk14    ] {ai_walk(6);};
void()    dcrossbow_walk14    =[    $walk14,    dcrossbow_walk1    ] {ai_walk(2);};

//===========================================================================
// 6 frames instead of 8 frames (skipping 3rd/7th timing of original)

void()    dcrossbow_run1    =[    $run1,    dcrossbow_run2    ] { monster_idle_sound();
    self.idlebusy = FALSE; ai_run (16);};
void()    dcrossbow_run2    =[    $run2,    dcrossbow_run3    ] {ai_run(20);};
void()    dcrossbow_run3    =[    $run3,    dcrossbow_run4    ] {ai_run(7);};
void()    dcrossbow_run4    =[    $run4,    dcrossbow_run5    ] {ai_run(16);};
void()    dcrossbow_run5    =[    $run5,    dcrossbow_run6    ] {ai_run(20);};
void()    dcrossbow_run6    =[    $run6,    dcrossbow_run1    ] {ai_run(6);};

//===========================================================================
// FINISHING (STOP) - exit point for the range attack logic
// STOP  - lower crossbow and then run / hunt player
void()    dcrossbow_stop1 =[    $tstop1,    dcrossbow_stop2    ] {self.weaponstate = CROSS_UP;};
void()    dcrossbow_stop2 =[    $tstop2,    dcrossbow_stop3    ] {};
void()    dcrossbow_stop3 =[    $tstop3,    dcrossbow_stop4    ] {};
void()    dcrossbow_stop4 =[    $tstop4,    dcrossbow_stop5    ] {};
void()    dcrossbow_stop5 =[    $tstop5,    dcrossbow_stop6    ] {};
void()    dcrossbow_stop6 =[    $tstop6,    dcrossbow_stop7    ] {};
void()    dcrossbow_stop7 =[    $tstop7,    dcrossbow_run1 ] {self.weaponstate = CROSS_DOWN;};

//===========================================================================
void() dcrossbow_shot =
{
    local    vector org, dir;

    if (!self.enemy) return;
    if (self.health < 1) return;

    // Flash effect to show where bolt is coming from    
    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "weapons/crossbow_fire.wav", 1, ATTN_NORM);

    // Skill level adjustment
    self.attack_speed = SPEED_DCROSS + (skill*SPEED_BOLTSKILL);

    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);

    //It certainly was really cool to write the code for tracking and
    //lots of fun to watch how the AI adapts to the player movement,
    //but as a player it just feels cheap and unfair!
    //- The tracking feature is now just a spawnflag option
    if (self.spawnflags & MON_DCROSSTRACK) dir = normalize(self.attack_track - org);
    else dir = normalize(self.enemy.origin - org);
    
    // Fire projectile at player    
    launch_projectile (org, dir, CT_PROJ_BOLT1, self.attack_speed);
};

//----------------------------------------------------------------------------
// FIRE  - fires crossbow and reload
// Trimmed down from 32 frames to 20 (now 2s reload time)
// Removed - 4,6,8,10, 17,19,21,23, 25,27,29,31
//----------------------------------------------------------------------------
void()    dcrossbow_fire1 =[    $tfire1,    dcrossbow_fire2    ] {dcrossbow_shot();};
void()    dcrossbow_fire2 =[    $tfire2,    dcrossbow_fire3    ] {ai_face();};
void()    dcrossbow_fire3 =[    $tfire3,    dcrossbow_fire5    ] {ai_face();};
void()    dcrossbow_fire5 =[    $tfire5,    dcrossbow_fire7    ] {};
void()    dcrossbow_fire7 =[    $tfire7,    dcrossbow_fire9    ] {};
void()    dcrossbow_fire9 =[    $tfire9,    dcrossbow_fire11] {};
void()    dcrossbow_fire11 =[    $tfire11,    dcrossbow_fire12] {};
void()    dcrossbow_fire12 =[    $tfire12,    dcrossbow_fire13] {};
void()    dcrossbow_fire13 =[    $tfire13,    dcrossbow_fire14] {};
void()    dcrossbow_fire14 =[    $tfire14,    dcrossbow_fire15] {};
void()    dcrossbow_fire15 =[    $tfire15,    dcrossbow_fire16] {
    sound (self, CHAN_WEAPON, "weapons/crossbow_rdy.wav", 1, ATTN_NORM);};
void()    dcrossbow_fire16 =[    $tfire16,    dcrossbow_fire18] {};
void()    dcrossbow_fire18 =[    $tfire18,    dcrossbow_fire20] {};
void()    dcrossbow_fire20 =[    $tfire20,    dcrossbow_fire22] {};
void()    dcrossbow_fire22 =[    $tfire22,    dcrossbow_fire24] {};
void()    dcrossbow_fire24 =[    $tfire24,    dcrossbow_fire26] {};
void()    dcrossbow_fire26 =[    $tfire26,    dcrossbow_fire28] {ai_face();};
void()    dcrossbow_fire28 =[    $tfire28,    dcrossbow_fire30] {ai_face();};
void()    dcrossbow_fire30 =[    $tfire30,    dcrossbow_hold1] {ai_face();
    self.weaponswitch = time + CROSS_TIMEOUT;            // Don't exit straight away
};

//===========================================================================
void() dcrossbow_checkattack =
{
    local float boltspeed;

    // Always return if monster is dead!
    if (self.health < 1) return;

    // Check to see if enemy is dead? back to run/hunt cycle
    if (self.enemy.health < 1) dcrossbow_stop1();
    else {
        // is the enemy visible? (liquid check)
        if (visible(self.enemy)) {
            self.weaponswitch = time + CROSS_TIMEOUT;
            // Is the enemy blocked by another enemy?
            if (!visblocked_wide(self.enemy, self.attack_offset, '0 0 0')) {
                // Is the enemy within melee range?
                if (ai_checkmelee(MONAI_MELEEKNIGHT)) self.th_melee();
                else {
                    // Skill level adjustment for bolt speed
                    boltspeed = SPEED_DCROSS + (skill*SPEED_BOLTSKILL);
                    self.attack_track = SUB_Tracking(self.attack_track, self.attack_offset, self.enemy, boltspeed);
        
                    // Chance of attack? start out low and get better chance+tracking over time    
                    self.attack_chance = self.attack_chance + 0.2;
                    if (random() < self.attack_chance) dcrossbow_fire1();
                }
            }
        }
        else {
            // Have not see player for a while, stop wating and hunt for them
            if (self.weaponswitch < time+0.1) dcrossbow_stop1();
            else {
                // If cannot see player try strafe side to side first before hunt mode
                if (self.attack_state == AS_MISSILE) self.think = dcrossbow_slide1;
            }
        }
    }
};

//----------------------------------------------------------------------------
// SLIDE  - strafe side to side movement, crossbow up high
void()    dcrossbow_slide1 =[    $tslide1,    dcrossbow_slide2    ] {ai_run_slide(4);
    self.attack_state = AS_SLIDING;            // Update type of attack
    self.attack_chance = 0.5;                // higher chance to attack because moving
    self.attack_track = self.enemy.origin;    // reset enemy origin ready for tracking
    dcrossbow_checkattack();                // Start tracking the player
};
void()    dcrossbow_slide2 =[    $tslide2,    dcrossbow_slide3    ] {ai_run_slide(6);dcrossbow_checkattack();};
void()    dcrossbow_slide3 =[    $tslide3,    dcrossbow_slide4    ] {ai_run_slide(4);dcrossbow_checkattack();};
void()    dcrossbow_slide4 =[    $tslide4,    dcrossbow_slide5    ] {ai_run_slide(2);dcrossbow_checkattack();monster_footstep(FALSE);};
void()    dcrossbow_slide5 =[    $tslide5,    dcrossbow_slide6    ] {ai_run_slide(4);dcrossbow_checkattack();};
void()    dcrossbow_slide6 =[    $tslide6,    dcrossbow_slide7    ] {ai_run_slide(6);dcrossbow_checkattack();};
void()    dcrossbow_slide7 =[    $tslide7,    dcrossbow_slide8    ] {ai_run_slide(4);dcrossbow_checkattack();};
void()    dcrossbow_slide8 =[    $tslide8,    dcrossbow_slide1    ] {ai_run_slide(2);
    if (!visblocked_wide(self.enemy, '0 0 28', '0 0 0')) self.think = dcrossbow_hold1;
    else dcrossbow_checkattack();
};

//----------------------------------------------------------------------------
// STARTING (SLIDE) - brings crossbow up to shoulder height
void()    dcrossbow_slideup1 =[    $tready1,    dcrossbow_slideup2    ] {ai_face();
    self.weaponstate = CROSS_DOWN;        // Crossbow animation state
    self.lefty = rint(random());        // Randomize strafe direction
};
void()    dcrossbow_slideup2 =[    $tready2,    dcrossbow_slideup3    ] {};
void()    dcrossbow_slideup3 =[    $tready3,    dcrossbow_slideup4    ] {ai_face();};
void()    dcrossbow_slideup4 =[    $tready4,    dcrossbow_slideup5    ] {};
void()    dcrossbow_slideup5 =[    $tready5,    dcrossbow_slideup6    ] {ai_face();};
void()    dcrossbow_slideup6 =[    $tready6,    dcrossbow_slideup7    ] {monster_footstep(FALSE);};
void()    dcrossbow_slideup7 =[    $tready7,    dcrossbow_slide1    ] {ai_face();
    self.weaponswitch = time + CROSS_TIMEOUT;            // Don't exit straight away
    self.weaponstate = CROSS_UP;        // Crossbow animation in correct state for slide/hold
};

//----------------------------------------------------------------------------
void() dcrossbow_checkslide =
{
    if (self.weaponstate == CROSS_DOWN) dcrossbow_slideup1();
    else dcrossbow_slide1();
};

//----------------------------------------------------------------------------
// HOLD  - keeps crossbow up high and wait to fire at player
// Random chance to slide to the side instead of being a stationary target
void()    dcrossbow_hold1 =[    $thold1,    dcrossbow_hold2    ] {ai_face();
    self.attack_state = AS_MISSILE;            // Update type of attack
    // Quick fire coming from melee attack, otherwise slow build up
    if (self.attack_chance < 0) self.attack_chance = 0.5;
    else self.attack_chance = 0;            // Initial chance of attack is low
    self.attack_track = self.enemy.origin;    // reset enemy origin ready for tracking
    dcrossbow_checkattack();
};
void()    dcrossbow_hold2 =[    $thold2,    dcrossbow_hold3    ] {dcrossbow_checkattack();};
void()    dcrossbow_hold3 =[    $thold3,    dcrossbow_hold4    ] {ai_face();dcrossbow_checkattack();};
void()    dcrossbow_hold4 =[    $thold4,    dcrossbow_hold5    ] {dcrossbow_checkattack();};
void()    dcrossbow_hold5 =[    $thold5,    dcrossbow_hold6    ] {ai_face();dcrossbow_checkattack();};
void()    dcrossbow_hold6 =[    $thold6,    dcrossbow_hold1    ] {
    if (random() < 0.5) self.think = dcrossbow_slide1;
    else dcrossbow_checkattack();
};
        
//----------------------------------------------------------------------------
// STARTING (HOLD) - brings crossbow up to shoulder height
void()    dcrossbow_ready1 =[    $tready1,    dcrossbow_ready2    ] {ai_face();
    self.weaponstate = CROSS_DOWN;        // Crossbow animation state
    self.lefty = rint(random());        // Randomize strafe direction
};
void()    dcrossbow_ready2 =[    $tready2,    dcrossbow_ready3    ] {};
void()    dcrossbow_ready3 =[    $tready3,    dcrossbow_ready4    ] {ai_face();};
void()    dcrossbow_ready4 =[    $tready4,    dcrossbow_ready5    ] {};
void()    dcrossbow_ready5 =[    $tready5,    dcrossbow_ready6    ] {ai_face();};
void()    dcrossbow_ready6 =[    $tready6,    dcrossbow_ready7    ] {monster_footstep(FALSE);};
void()    dcrossbow_ready7 =[    $tready7,    dcrossbow_hold1    ] {ai_face();
    self.weaponswitch = time + CROSS_TIMEOUT;    // Don't exit straight away
    self.weaponstate = CROSS_UP;                // Crossbow animation in correct state for slide/hold
};

//===========================================================================
// Cycle between melee attacks
// Attack 1 - Slice (stationary one hand slashing attack)
// Attack 2 - Smash (chop overhead to ground)
//----------------------------------------------------------------------
void()    dcrossbow_slice1    =[    $slice2,    dcrossbow_slice2    ] {self.weaponstate = CROSS_DOWN;ai_face();};
void()    dcrossbow_slice2    =[    $slice3,    dcrossbow_slice3    ] {ai_face(); dcrossbow_crossbow_sound();};
void()    dcrossbow_slice3    =[    $slice4,    dcrossbow_slice4    ] {ai_face();};
void()    dcrossbow_slice4    =[    $slice5,    dcrossbow_slice5    ] {ai_melee();};
void()    dcrossbow_slice5    =[    $slice6,    dcrossbow_slice6    ] {self.meleecontact=TRUE;ai_meleesmash(15);};
void()    dcrossbow_slice6    =[    $slice7,    dcrossbow_slice7    ] {ai_melee();self.meleecontact=FALSE;};
void()    dcrossbow_slice7    =[    $slice8,    dcrossbow_slice8    ] {};
void()    dcrossbow_slice8    =[    $slice9,    dcrossbow_slice9    ] {};
void()    dcrossbow_slice9    =[    $slice10,    dcrossbow_slice10    ] {};
void()    dcrossbow_slice10    =[    $slice11,    dcrossbow_slice11    ] {ai_face();};
void()    dcrossbow_slice11    =[    $slice12,    dcrossbow_slice1    ] {dcrossbow_checkmelee();};

//----------------------------------------------------------------------
// Knight - 11 frames (skipped (1, 11, 14, 15)
void()    dcrossbow_smash1    =[    $smash2,    dcrossbow_smash2    ] {self.weaponstate = CROSS_DOWN;ai_charge(1);};
void()    dcrossbow_smash2    =[    $smash3,    dcrossbow_smash3    ] {ai_charge(7);};
void()    dcrossbow_smash3    =[    $smash4,    dcrossbow_smash4    ] {monster_footstep(FALSE); ai_charge(4);};
void()    dcrossbow_smash4    =[    $smash5,    dcrossbow_smash5    ] {ai_face();};
void()    dcrossbow_smash5    =[    $smash6,    dcrossbow_smash6    ] {dcrossbow_crossbow_sound(); ai_charge(3);};
void()    dcrossbow_smash6    =[    $smash7,    dcrossbow_smash7    ] {ai_charge(4);ai_melee();};
void()    dcrossbow_smash7    =[    $smash8,    dcrossbow_smash8    ] {monster_footstep(FALSE);self.meleecontact=TRUE;ai_meleesmash(15);};
void()    dcrossbow_smash8    =[    $smash9,    dcrossbow_smash9    ] {ai_charge(3);ai_melee();self.meleecontact=FALSE;};
void()    dcrossbow_smash9    =[    $smash10,    dcrossbow_smash10    ] {ai_charge(1);};
void()    dcrossbow_smash10    =[    $smash12,    dcrossbow_smash11    ] {ai_charge(1);};
void()    dcrossbow_smash11    =[    $smash13,    dcrossbow_smash1    ] {dcrossbow_checkmelee();};

//----------------------------------------------------------------------
void() dcrossbow_melee =
{
    // Cycle between different melee attacks
    self.meleeattack = self.meleeattack + 1;
    if (self.meleeattack == 1) dcrossbow_slice1();
    else { dcrossbow_smash1(); self.meleeattack = 0; }
};

//----------------------------------------------------------------------
void() dcrossbow_checkmelee =
{
    // Always return if monster is dead!
    if (self.health < 1) return;

    // Check to see if enemy is dead? back to run/hunt cycle
    if (self.enemy.health < 1) ai_run(0);
    else {
        // Is the enemy within melee range?
        // If not then do a quick hipshot at the player
        if (!ai_checkmelee(MONAI_MELEEKNIGHT)) {
            self.attack_chance = -1;    // Start with higher percentage
            dcrossbow_ready1();            // Raise up crossbow ready
        }
        else dcrossbow_melee();
    }
};

//============================================================================
void()    dcrossbow_painA1    =[    $painA1,        dcrossbow_painA2    ] {self.weaponstate = CROSS_DOWN;};
void()    dcrossbow_painA2    =[    $painA2,        dcrossbow_painA3    ] {};
void()    dcrossbow_painA3    =[    $painA3,        dcrossbow_painA4    ] {};
void()    dcrossbow_painA4    =[    $painA4,        dcrossbow_painA5    ] {};
void()    dcrossbow_painA5    =[    $painA5,        dcrossbow_run1    ] {};
//----------------------------------------------------------------------
void()    dcrossbow_painD1    =[    $painD1,        dcrossbow_painD2    ] {self.weaponstate = CROSS_DOWN;};
void()    dcrossbow_painD2    =[    $painD2,        dcrossbow_painD3    ] {};
void()    dcrossbow_painD3    =[    $painD3,        dcrossbow_painD4    ] {};
void()    dcrossbow_painD4    =[    $painD4,        dcrossbow_painD5    ] {monster_footstep(FALSE);};
void()    dcrossbow_painD5    =[    $painD5,        dcrossbow_painD6    ] {};
void()    dcrossbow_painD6    =[    $painD6,        dcrossbow_painD7    ] {};
void()    dcrossbow_painD7    =[    $painD7,        dcrossbow_painD8    ] {};
void()    dcrossbow_painD8    =[    $painD8,        dcrossbow_painD9    ] {monster_footstep(FALSE);};
void()    dcrossbow_painD9    =[    $painD9,        dcrossbow_painD10    ] {};
void()    dcrossbow_painD10=[    $painD10,        dcrossbow_painD11    ] {};
void()    dcrossbow_painD11=[    $painD11,        dcrossbow_run1    ] {};
//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage)    dcrossbow_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1) {
            // Randomly pick which pain animation to play
            if (random() < 0.85) dcrossbow_painA1 ();    // classic, body recoil
            else {
                dcrossbow_painD1 ();            // Cool stumble, hand over mouth
                self.pain_finished = time + 2;    // long animation
            }
        }
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 1.1;
            self.axhitme = 0;
            dcrossbow_painD1 ();
        }
    }
};

//============================================================================
void()    dcrossbow_die1    =[    $death1,    dcrossbow_die2    ] {ai_forward(10);};
void()    dcrossbow_die2    =[    $death2,    dcrossbow_die3    ] {monster_check_gib();ai_forward(8);};
void()    dcrossbow_die3    =[    $death3,    dcrossbow_die4    ] {monster_check_gib();
    self.solid = SOLID_NOT; ai_forward(7);};
void()    dcrossbow_die4    =[    $death4,    dcrossbow_die5    ] {};
void()    dcrossbow_die5    =[    $death5,    dcrossbow_die6    ] {};
void()    dcrossbow_die6    =[    $death6,    dcrossbow_die7    ] {};
void()    dcrossbow_die7    =[    $death7,    dcrossbow_die8    ] {};
void()    dcrossbow_die8    =[    $death8,    dcrossbow_die9    ] {ai_forward(10);
    sound (self, CHAN_WEAPON, GIB_SOUND_WOOD, 1, ATTN_NORM);};
void()    dcrossbow_die9    =[    $death9,    dcrossbow_die10    ] {ai_forward(11);};
void()    dcrossbow_die10    =[    $death10,    dcrossbow_die11    ] {monster_death_postcheck();};
void()    dcrossbow_die11    =[    $death11,    dcrossbow_die11    ] {monster_deadbody_check();};
//----------------------------------------------------------------------
void()    dcrossbow_dieb1    =[    $deathb1,    dcrossbow_dieb2    ] {};
void()    dcrossbow_dieb2    =[    $deathb2,    dcrossbow_dieb3    ] {monster_check_gib();};
void()    dcrossbow_dieb3    =[    $deathb3,    dcrossbow_dieb4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void()    dcrossbow_dieb4    =[    $deathb4,    dcrossbow_dieb5    ] {};
void()    dcrossbow_dieb5    =[    $deathb5,    dcrossbow_dieb6    ] {};
void()    dcrossbow_dieb6    =[    $deathb6,    dcrossbow_dieb7    ] {};
void()    dcrossbow_dieb7    =[    $deathb7,    dcrossbow_dieb8    ] {
    sound (self, CHAN_WEAPON, GIB_SOUND_WOOD, 1, ATTN_NORM);};
void()    dcrossbow_dieb8    =[    $deathb8,    dcrossbow_dieb9    ] {};
void()    dcrossbow_dieb9    =[    $deathb9,    dcrossbow_dieb10    ] {};
void()    dcrossbow_dieb10    =[    $deathb10,    dcrossbow_dieb11    ] {};
void()    dcrossbow_dieb11    =[    $deathb11,    dcrossbow_dieb12    ] {};
void()    dcrossbow_dieb12    =[    $deathb12,    dcrossbow_dieb13    ] {};
void()    dcrossbow_dieb13    =[    $deathb13,    dcrossbow_dieb14    ] {};
void()    dcrossbow_dieb14    =[    $deathb14,    dcrossbow_dieb15    ] {};
void()    dcrossbow_dieb15    =[    $deathb15,    dcrossbow_dieb16    ] {};
void()    dcrossbow_dieb16    =[    $deathb16,    dcrossbow_dieb17    ] {};
void()    dcrossbow_dieb17    =[    $deathb17,    dcrossbow_dieb18    ] {};
void()    dcrossbow_dieb18    =[    $deathb18,    dcrossbow_dieb19    ] {};
void()    dcrossbow_dieb19    =[    $deathb19,    dcrossbow_dieb20    ] {monster_death_postcheck();};
void()    dcrossbow_dieb20    =[    $deathb20,    dcrossbow_dieb20    ] {monster_deadbody_check();};
//----------------------------------------------------------------------
void() dcrossbow_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // Clear weapon sound
    sound (self, CHAN_WEAPON, SOUND_EMPTY, 1, ATTN_NORM);

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "knight/kdeath.wav", 1, ATTN_NORM);
        if (random() > 0.5) dcrossbow_die1 ();
        else dcrossbow_dieb1 ();
    }
};

//============================================================================
void() dcrossbow_crossbow_sound =
{
    sound (self, CHAN_WEAPON, "weapons/crossbow_swipe.wav", 1, ATTN_NORM);
};

/*======================================================================
QUAKED monster_dcrossbow (1 0 0) (-16 -16 -24) (16 16 40) Ambush
======================================================================*/
void() monster_dcrossbow =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_dcrossbow.mdl";            // crossbow guy
    self.headmdl = "progs/ad171/h_dguard.mdl";
    self.gib1mdl = "progs/ad171/w_dcrossbow.mdl";            // Unique weapon
    self.gib2mdl = "progs/ad171/gib_knfoot_l.mdl";        // left foot
    self.gib3mdl = "progs/ad171/gib_knfoot_r.mdl";        // right foot
    
    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_BOLT1);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    
    self.gib1sound = GIB_IMPACT_WOOD;
    if (random() < 0.5) self.gib2mdl = string_null;
    if (random() < 0.5) self.gib3mdl = string_null;

    self.idle_sound = "dknight/dcross_idle.wav";
    precache_sound (self.idle_sound);
    self.idle_soundcom = "dknight/dcross_idlecom.wav";
    precache_sound (self.idle_soundcom);

    self.pain_sound = "knight/khurt.wav";
    precache_sound (self.pain_sound);
    precache_sound ("knight/kdeath.wav");

    // Crossbow and bolt sounds
    precache_sound ("weapons/bolt_hit1.wav");
    precache_sound ("weapons/bolt_hit2.wav");
    precache_sound ("weapons/bolt_hit3.wav");
    precache_sound ("weapons/bolt_hit4.wav");
    precache_sound ("weapons/crossbow_fire.wav");
    precache_sound ("weapons/crossbow_swipe.wav");

    // Heavy wood sound
    self.meleehitsound = "weapons/crossbow_hit1.wav";
    precache_sound (self.meleehitsound);

    self.sight_sound = "weapons/crossbow_rdy.wav";
    precache_sound (self.sight_sound);

    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
        precache_model ("progs/ad171/proj_bolt2.mdl");    // Green poison bolt
        self.skin = self.exactskin = 1;        // Greener Green+++ Skin!?!
        if (self.deathstring == "") self.deathstring = " was skewered by a Poisonous Crossbow Knight\n";
    } else {
        if (self.deathstring == "") self.deathstring = " was skewered by a Crossbow Knight\n";
    }
    
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_SHORT;
    if (self.health < 1) self.health = 75;
    self.gibhealth = -25;                // Can be gib from a rocket/grenade
    self.gibbed = FALSE;
    self.pain_flinch = 40;
    self.pain_longanim = TRUE;
    self.steptype = FS_TYPEMEDIUM;
    self.weaponstate = CROSS_DOWN;        // Maintain crossbow state
    self.meleecontact = FALSE;            // Smash contact
    self.meleerange = MONAI_MELEEFRONT;    // Smash attack
    self.attack_offset = '20 12 28';    // Where to fire bolt from
    
    // check for sniper mode, no distance checks for attacks
    if (self.spawnflags & MON_DCROSSSNIPER)
        self.enemymaxdist = TRUE;

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    self.th_checkattack = DCrossCheckAttack;
    self.th_stand = dcrossbow_stand1;
    self.th_walk = dcrossbow_walk1;
    self.th_run = dcrossbow_run1;
    self.th_melee = dcrossbow_melee;
    self.th_missile = dcrossbow_ready1;
    self.th_slide = dcrossbow_checkslide;
    self.th_pain = dcrossbow_pain;
    self.th_die = dcrossbow_die;

    self.hittype = HIT_METAL;

    self.classtype = CT_MONDCROSS;
    self.classgroup = CG_KNIGHT;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};
