/*==============================================================================
DEATH KNIGHT
==============================================================================*/
$cd id1/models/dknight
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

// (073 - 092) Stand idle - scratching chin
$frame standD1 standD2 standD3 standD4 standD5 standD6 standD7 standD8 
$frame standD9 standD10 standD11 standD12 standD13 standD14 standD15 standD16
$frame standD17 standD18 standD19 standD20

// (093 - 106) Default walk - typical hell knight stride
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9
$frame walk10 walk11 walk12 walk13 walk14

// (107 - 112) Default run
$frame run1 run2 run3 run4 run5 run6

// (113 - 127) Slice (one hand slashing run attack) (file=SWING)
$frame slice1 slice2 slice3 slice4 slice5 slice6 slice7 slice8
$frame slice9 slice10 slice11 slice12 slice13 slice14 slice15

// (128 - 142) Smash (chop overhead to ground) (file=CHOP)
$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7 smash8
$frame smash9 smash10 smash11 smash12 smash13 smash14 smash15

// (143 - 164) W_Attack (two handed slashing attack) (file=SLASH)
$frame w_attack1 w_attack2 w_attack3 w_attack4 w_attack5 w_attack6 w_attack7 w_attack8
$frame w_attack9 w_attack10 w_attack11 w_attack12 w_attack13 w_attack14 w_attack15 w_attack16
$frame w_attack17 w_attack18 w_attack19 w_attack20 w_attack21 w_attack22 

// (165 - 178) MagicC - Typical magic range attack (file=SWEEPCAST)
$frame magicc1 magicc2 magicc3 magicc4 magicc5 magicc6 magicc7 magicc8
$frame magicc9 magicc10 magicc11 magicc12 magicc13 magicc14

// Pain animations A = Slow (179 - 183), D = Stagger (184 - 194)
$frame painA1 painA2 painA3 painA4 painA5
$frame painD1 painD2 painD3 painD4 painD5 painD6 painD7 painD8 painD9 painD10 painD11

// (194 - 206) Charging attack A - swinging sword left to right infront
$frame char_a1 char_a2 char_a3 char_a4 char_a5 char_a6 char_a7 char_a8
$frame char_a9 char_a10 char_a11 char_a12

// (207 - 212) Charging attack B - rotating sword left to right infront
$frame char_b1 char_b2 char_b3 char_b4 char_b5 char_b6

// (212 - 224) Death forward
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12

// (225 - 244) Death backwards with cool sword wobble
$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9 deathb10 deathb11 deathb12 deathb13 deathb14 deathb15 deathb16
$frame deathb17 deathb18 deathb19 deathb20

// magica and magicb missing

void() dknight_sword_sound;            // Sword swipe sounds
void() dknight_ghost_particle;        // Ghost particles
void() dknight_ghost_glow;            // Secondary ghost glow

//===========================================================================
void() dk_stand = {
    if (self.spawnflags & MON_GHOST_ONLY) {
        if (self.movetype != MOVETYPE_NONE) self.movetype = MOVETYPE_NONE;
        dknight_ghost_particle();
        dknight_ghost_glow();
    }
    else ai_stand();
};

//----------------------------------------------------------------------
void()    dknight_standA1    =[    $standA1,    dknight_standA2    ] {dk_stand();};
void()    dknight_standA2    =[    $standA2,    dknight_standA3    ] {dk_stand();};
void()    dknight_standA3    =[    $standA3,    dknight_standA4    ] {dk_stand();};
void()    dknight_standA4    =[    $standA4,    dknight_standA5    ] {dk_stand();};
void()    dknight_standA5    =[    $standA5,    dknight_standA6    ] {dk_stand();};
void()    dknight_standA6    =[    $standA6,    dknight_standA7    ] {dk_stand();};
void()    dknight_standA7    =[    $standA7,    dknight_standA8    ] {dk_stand();};
void()    dknight_standA8    =[    $standA8,    dknight_standA9    ] {dk_stand();};
void()    dknight_standA9    =[    $standA9,    dknight_standA10] {dk_stand();};
void()    dknight_standA10=[    $standA10,    dknight_standA11] {dk_stand();};
void()    dknight_standA11=[    $standA11,    dknight_standA12] {dk_stand();};
void()    dknight_standA12=[    $standA12,    dknight_stand1    ] {dk_stand();};

//----------------------------------------------------------------------
void()    dknight_standB1    =[    $standB1,    dknight_standB2    ] {dk_stand();};
void()    dknight_standB2    =[    $standB2,    dknight_standB3    ] {dk_stand();};
void()    dknight_standB3    =[    $standB3,    dknight_standB4    ] {dk_stand();};
void()    dknight_standB4    =[    $standB4,    dknight_standB5    ] {dk_stand();};
void()    dknight_standB5    =[    $standB5,    dknight_standB6    ] {dk_stand();};
void()    dknight_standB6    =[    $standB6,    dknight_standB7    ] {dk_stand();};
void()    dknight_standB7    =[    $standB7,    dknight_standB8    ] {dk_stand();};
void()    dknight_standB8    =[    $standB8,    dknight_standB9    ] {dk_stand();};
void()    dknight_standB9    =[    $standB9,    dknight_standB10] {dk_stand();};
void()    dknight_standB10=[    $standB10,    dknight_standB11] {dk_stand();};
void()    dknight_standB11=[    $standB11,    dknight_standB12] {dk_stand();};
void()    dknight_standB12=[    $standB12,    dknight_standB13] {dk_stand();};
void()    dknight_standB13=[    $standB13,    dknight_standB14] {dk_stand();};
void()    dknight_standB14=[    $standB14,    dknight_standB15] {dk_stand();};
void()    dknight_standB15=[    $standB15,    dknight_standB16] {dk_stand();};
void()    dknight_standB16=[    $standB16,    dknight_standB17] {dk_stand();};
void()    dknight_standB17=[    $standB17,    dknight_standB18] {dk_stand();};
void()    dknight_standB18=[    $standB18,    dknight_standB19] {dk_stand();};
void()    dknight_standB19=[    $standB19,    dknight_standB20] {dk_stand();};
void()    dknight_standB20=[    $standB20,    dknight_standB21] {dk_stand();};
void()    dknight_standB21=[    $standB21,    dknight_standB22] {dk_stand();};
void()    dknight_standB22=[    $standB22,    dknight_standB23] {dk_stand();};
void()    dknight_standB23=[    $standB23,    dknight_standB24] {dk_stand();};
void()    dknight_standB24=[    $standB24,    dknight_stand1    ] {dk_stand();};

//----------------------------------------------------------------------
void()    dknight_standC1    =[    $standC1,    dknight_standC2    ] {dk_stand();};
void()    dknight_standC2    =[    $standC2,    dknight_standC3    ] {dk_stand();};
void()    dknight_standC3    =[    $standC3,    dknight_standC4    ] {dk_stand();};
void()    dknight_standC4    =[    $standC4,    dknight_standC5    ] {dk_stand();};
void()    dknight_standC5    =[    $standC5,    dknight_standC6    ] {dk_stand();};
void()    dknight_standC6    =[    $standC6,    dknight_standC7    ] {dk_stand();};
void()    dknight_standC7    =[    $standC7,    dknight_standC8    ] {dk_stand();};
void()    dknight_standC8    =[    $standC8,    dknight_standC9    ] {dk_stand();};
void()    dknight_standC9    =[    $standC9,    dknight_standC10] {dk_stand();};
void()    dknight_standC10=[    $standC10,    dknight_standC11] {dk_stand();};
void()    dknight_standC11=[    $standC11,    dknight_standC12] {dk_stand();};
void()    dknight_standC12=[    $standC12,    dknight_standC13] {dk_stand();};
void()    dknight_standC13=[    $standC13,    dknight_standC14] {dk_stand();};
void()    dknight_standC14=[    $standC14,    dknight_standC15] {dk_stand();};
void()    dknight_standC15=[    $standC15,    dknight_standC16] {dk_stand();};
void()    dknight_standC16=[    $standC16,    dknight_standC17] {dk_stand();};
void()    dknight_standC17=[    $standC17,    dknight_standC18] {dk_stand();};
void()    dknight_standC18=[    $standC18,    dknight_standC19] {dk_stand();};
void()    dknight_standC19=[    $standC19,    dknight_standC20] {dk_stand();};
void()    dknight_standC20=[    $standC20,    dknight_standC21] {dk_stand();};
void()    dknight_standC21=[    $standC21,    dknight_standC22] {dk_stand();};
void()    dknight_standC22=[    $standC22,    dknight_standC23] {dk_stand();};
void()    dknight_standC23=[    $standC23,    dknight_standC24] {dk_stand();};
void()    dknight_standC24=[    $standC24,    dknight_stand1    ] {dk_stand();};

//----------------------------------------------------------------------
void()    dknight_standD1    =[    $standD1,    dknight_standD2    ] {dk_stand();};
void()    dknight_standD2    =[    $standD2,    dknight_standD3    ] {dk_stand();};
void()    dknight_standD3    =[    $standD3,    dknight_standD4    ] {dk_stand();};
void()    dknight_standD4    =[    $standD4,    dknight_standD5    ] {dk_stand();};
void()    dknight_standD5    =[    $standD5,    dknight_standD6    ] {dk_stand();};
void()    dknight_standD6    =[    $standD6,    dknight_standD7    ] {dk_stand();};
void()    dknight_standD7    =[    $standD7,    dknight_standD8    ] {dk_stand();};
void()    dknight_standD8    =[    $standD8,    dknight_standD9    ] {dk_stand();};
void()    dknight_standD9    =[    $standD9,    dknight_standD10] {dk_stand();};
void()    dknight_standD10=[    $standD10,    dknight_standD11] {dk_stand();};
void()    dknight_standD11=[    $standD11,    dknight_standD12] {dk_stand();};
void()    dknight_standD12=[    $standD12,    dknight_standD13] {dk_stand();};
void()    dknight_standD13=[    $standD13,    dknight_standD14] {dk_stand();};
void()    dknight_standD14=[    $standD14,    dknight_standD15] {dk_stand();};
void()    dknight_standD15=[    $standD15,    dknight_standD16] {dk_stand();};
void()    dknight_standD16=[    $standD16,    dknight_standD17] {dk_stand();};
void()    dknight_standD17=[    $standD17,    dknight_standD18] {dk_stand();};
void()    dknight_standD18=[    $standD18,    dknight_standD19] {dk_stand();};
void()    dknight_standD19=[    $standD19,    dknight_standD20] {dk_stand();};
void()    dknight_standD20=[    $standD20,    dknight_stand1    ] {dk_stand();};

//----------------------------------------------------------------------
void()    dknight_stand1    =[    $stand1,    dknight_stand2    ] {monster_idle_sound();dk_stand();};
void()    dknight_stand2    =[    $stand2,    dknight_stand3    ] {dk_stand();};
void()    dknight_stand3    =[    $stand3,    dknight_stand4    ] {dk_stand();};
void()    dknight_stand4    =[    $stand4,    dknight_stand5    ] {dk_stand();};
void()    dknight_stand5    =[    $stand5,    dknight_stand6    ] {dk_stand();};
void()    dknight_stand6    =[    $stand6,    dknight_stand7    ] {dk_stand();};
void()    dknight_stand7    =[    $stand7,    dknight_stand8    ] {dk_stand();};
void()    dknight_stand8    =[    $stand8,    dknight_stand9    ] {dk_stand();};
void()    dknight_stand9    =[    $stand9,    dknight_stand10    ] {dk_stand();};
void()    dknight_stand10    =[    $stand10,    dknight_stand11    ] {dk_stand();};
void()    dknight_stand11    =[    $stand11,    dknight_stand12    ] {dk_stand();};
void()    dknight_stand12    =[    $stand12,    dknight_stand1    ] {
    self.idlebusy = FALSE; 
    if (random() < MON_IDLE_ANIMATION) {
        self.idlebusy = TRUE; 
        // Make sure the idle animations don't repeat in a row
        self.lefty = self.lip;
        while (self.lefty == self.lip) { self.lefty = rint(random()*4);}
        self.lip = self.lefty;
        if (self.lip < 1) self.think = dknight_standD1;            // Scratch chin
        else if (self.lip < 2) self.think = dknight_standC1;    // Look at weapon
        else if (self.lip < 3) self.think = dknight_standA1;    // Shoulder hunch
        else self.think = dknight_standB1;                        // Look around
    }
};

//===========================================================================
void(float dist) dk_walk = {
    if (self.spawnflags & MON_GHOST_ONLY) {
        if (self.movetype != MOVETYPE_STEP) self.movetype = MOVETYPE_STEP;
        movetogoal (dist);
        dknight_ghost_particle();
        dknight_ghost_glow();
        
        // Find out if the ghost is close enough to next path corner
        // The ghost is not a monster, negative health and no body!
        // The path_corner touch function won't work with ghosts
        self.enemydist = range_distance(self.goalentity, FALSE);
        if (self.enemydist < 64) {
            // Check for any new targets to walk toward
            if (self.goalentity.target != "") {
                self.movetarget = find(world, targetname, self.goalentity.target);
                if (self.movetarget) {
                    if (self.movetarget.classtype == CT_PATHCORNER) {
                        self.goalentity = self.movetarget;
                        self.ideal_yaw = vectoyaw(self.goalentity.origin - self.origin);
                        return;
                    }
                }
            }
            // It seems its time to stand around!
            self.th_stand();
            // Check for any path corner event triggers
            if (self.goalentity.corner_event) 
                trigger_strs(self.goalentity.corner_event, self);
        }
    }
    // Default walk for regular knights
    else ai_walk(dist);
};

//----------------------------------------------------------------------
void()    dknight_walk1    =[    $walk1,        dknight_walk2    ] {self.idlebusy = FALSE;
    monster_idle_sound(); dk_walk(2);};
void()    dknight_walk2    =[    $walk2,        dknight_walk3    ] {monster_footstep(FALSE);dk_walk(5);};
void()    dknight_walk3    =[    $walk3,        dknight_walk4    ] {dk_walk(5);};
void()    dknight_walk4    =[    $walk4,        dknight_walk5    ] {dk_walk(4);};
void()    dknight_walk5    =[    $walk5,        dknight_walk6    ] {dk_walk(4);};
void()    dknight_walk6    =[    $walk6,        dknight_walk7    ] {dk_walk(2);};
void()    dknight_walk7    =[    $walk7,        dknight_walk8    ] {dk_walk(2);};
void()    dknight_walk8    =[    $walk8,        dknight_walk9    ] {monster_footstep(FALSE); dk_walk(3);};
void()    dknight_walk9    =[    $walk9,        dknight_walk10    ] {dk_walk(3);};
void()    dknight_walk10    =[    $walk10,    dknight_walk11    ] {dk_walk(4);};
void()    dknight_walk11    =[    $walk11,    dknight_walk12    ] {dk_walk(3);};
void()    dknight_walk12    =[    $walk12,    dknight_walk13    ] {dk_walk(4);};
void()    dknight_walk13    =[    $walk13,    dknight_walk14    ] {dk_walk(6);};
void()    dknight_walk14    =[    $walk14,    dknight_walk1    ] {dk_walk(2);};

//===========================================================================
void()    dknight_run1    =[    $run1,    dknight_run2    ] {monster_idle_sound(); 
    self.idlebusy = FALSE; ai_run (20); CheckForCharge(); };
void()    dknight_run2    =[    $run2,    dknight_run3    ] {monster_footstep(FALSE); ai_run(25);};
void()    dknight_run3    =[    $run3,    dknight_run4    ] {ai_run(16);};
void()    dknight_run4    =[    $run4,    dknight_run5    ] {ai_run(14);};
void()    dknight_run5    =[    $run5,    dknight_run6    ] {monster_footstep(FALSE); ai_run(25);};
void()    dknight_run6    =[    $run6,    dknight_run1    ] {ai_run(13);};

//===========================================================================
// Charging attack A - swinging sword left to right infront
// Hell Knight - 16 frames / Death Knight - 12 frames
// Death Knight is faster (-4 frames will cycle damage frames quicker)
// Damage Frames - Hell Knight (6-11, Total 6) Death Knight (6-10, Total 5)

void()    dknight_char_a1    =[    $char_a1,    dknight_char_a2    ] {ai_charge(20); dknight_sword_sound();};
void()    dknight_char_a2    =[    $char_a2,    dknight_char_a3    ] {monster_footstep(FALSE); ai_charge(25);ai_melee(); };
void()    dknight_char_a3    =[    $char_a3,    dknight_char_a4    ] {ai_charge(18);ai_melee(); };
void()    dknight_char_a4    =[    $char_a4,    dknight_char_a5    ] {ai_charge(16);};
void()    dknight_char_a5    =[    $char_a5,    dknight_char_a6    ] {monster_footstep(FALSE); ai_charge(14);};
void()    dknight_char_a6    =[    $char_a6,    dknight_char_a7    ] {ai_charge(20); ai_melee();};
void()    dknight_char_a7    =[    $char_a7,    dknight_char_a8    ] {ai_charge(21); ai_melee(); dknight_sword_sound();};
void()    dknight_char_a8    =[    $char_a8,    dknight_char_a9    ] {monster_footstep(FALSE); ai_charge(13); ai_melee();};
void()    dknight_char_a9    =[    $char_a9,    dknight_char_a10    ] {ai_charge(20); ai_melee();};
void()    dknight_char_a10=[    $char_a10,    dknight_char_a11    ] {ai_charge(20); ai_melee();};
void()    dknight_char_a11=[    $char_a11,    dknight_char_a12    ] {monster_footstep(FALSE); ai_charge(18);};
void()    dknight_char_a12=[    $char_a12,    dknight_run1    ] {ai_charge(16);};

//----------------------------------------------------------------------
// Identical frame set (Hell Knight / Death Knight)

void()    dknight_char_b1    =[    $char_b1,    dknight_char_b2    ] {
    CheckContinueCharge (); ai_charge(23); ai_melee(); dknight_sword_sound();};
void()    dknight_char_b2    =[    $char_b2,    dknight_char_b3    ] {monster_footstep(FALSE); ai_charge(17); ai_melee();};
void()    dknight_char_b3    =[    $char_b3,    dknight_char_b4    ] {ai_charge(12); ai_melee();};
void()    dknight_char_b4    =[    $char_b4,    dknight_char_b5    ] {ai_charge(22); ai_melee();};
void()    dknight_char_b5    =[    $char_b5,    dknight_char_b6    ] {monster_footstep(FALSE); ai_charge(18); ai_melee();};
void()    dknight_char_b6    =[    $char_b6,    dknight_char_b1    ] {ai_charge(8); ai_melee();};

//===========================================================================
// Melee Attack 1 - Slice (one hand slashing run attack)
// Hell Knight - 10 frames / Death Knight - 15 frames (skipped 1, 3, 12, 15)
// Death Knight - trimmed frames so melee attack 1 and 2 the same

void()    dknight_slice1    =[    $slice2,    dknight_slice2    ] {ai_charge(9);};
void()    dknight_slice2    =[    $slice4,    dknight_slice3    ] {monster_footstep(FALSE); ai_charge(6);};
void()    dknight_slice3    =[    $slice5,    dknight_slice4    ] {ai_charge(13);};
void()    dknight_slice4    =[    $slice6,    dknight_slice5    ] {ai_charge(4); dknight_sword_sound();};
void()    dknight_slice5    =[    $slice7,    dknight_slice6    ] {monster_footstep(FALSE); ai_charge(7); ai_melee();};
void()    dknight_slice6    =[    $slice8,    dknight_slice7    ] {ai_charge(15); ai_melee();};
void()    dknight_slice7    =[    $slice9,    dknight_slice8    ] {ai_charge(8); ai_melee();};
void()    dknight_slice8    =[    $slice10,    dknight_slice9    ] {monster_footstep(FALSE); ai_charge(2); ai_melee();};
void()    dknight_slice9    =[    $slice11,    dknight_slice10    ] {ai_melee();};
void()    dknight_slice10    =[    $slice13,    dknight_slice11    ] {monster_footstep(FALSE); ai_charge(0);};
void()    dknight_slice11    =[    $slice14,    dknight_run1    ] {ai_charge(3);};

//----------------------------------------------------------------------
// Melee Attack 2 - Smash (chop overhead to ground)
// Hell Knight - 11 frames / Death Knight - 15 frames (skipped (1, 11, 14, 15)
// Death Knight frames trimmed down to match Hell Knight (damage ratio)

void()    dknight_smash1    =[    $smash2,    dknight_smash2    ] {ai_charge(1);};
void()    dknight_smash2    =[    $smash3,    dknight_smash3    ] {ai_charge(13);};
void()    dknight_smash3    =[    $smash4,    dknight_smash4    ] {monster_footstep(FALSE); ai_charge(9);};
void()    dknight_smash4    =[    $smash5,    dknight_smash5    ] {ai_charge(11);dknight_sword_sound();};
void()    dknight_smash5    =[    $smash6,    dknight_smash6    ] {ai_charge(10);};
void()    dknight_smash6    =[    $smash7,    dknight_smash7    ] {ai_charge(7); ai_melee();};
void()    dknight_smash7    =[    $smash8,    dknight_smash8    ] {monster_footstep(FALSE); ai_charge(12); ai_melee();};
void()    dknight_smash8    =[    $smash9,    dknight_smash9    ] {ai_charge(2); ai_melee();};
void()    dknight_smash9    =[    $smash10,    dknight_smash10    ] {ai_charge(3); ai_melee();};
void()    dknight_smash10    =[    $smash12,    dknight_smash11    ] {ai_charge(0);ai_melee();};
void()    dknight_smash11    =[    $smash13,    dknight_run1    ] {ai_charge(0);};

//----------------------------------------------------------------------
// Melee Attack 3 - W_Attack (two handed slashing attack)
// Identical frame set (Hell Knight / Death Knight)

void()    dknight_watk1    =[    $w_attack1,    dknight_watk2    ] {ai_charge(2);};
void()    dknight_watk2    =[    $w_attack2,    dknight_watk3    ] {monster_footstep(FALSE); ai_charge(0);};
void()    dknight_watk3    =[    $w_attack3,    dknight_watk4    ] {ai_charge(0);};
void()    dknight_watk4    =[    $w_attack4,    dknight_watk5    ] {ai_charge(0); dknight_sword_sound();};
void()    dknight_watk5    =[    $w_attack5,    dknight_watk6    ] {ai_melee();};
void()    dknight_watk6    =[    $w_attack6,    dknight_watk7    ] {ai_melee();};
void()    dknight_watk7    =[    $w_attack7,    dknight_watk8    ] {monster_footstep(FALSE); ai_charge(1);};
void()    dknight_watk8    =[    $w_attack8,    dknight_watk9    ] {ai_charge(4);};
void()    dknight_watk9    =[    $w_attack9,    dknight_watk10    ] {ai_charge(5);};
void()    dknight_watk10    =[    $w_attack10,    dknight_watk11    ] {ai_charge(3); ai_melee();};
void()    dknight_watk11    =[    $w_attack11,    dknight_watk12    ] {ai_charge(2); ai_melee();};
void()    dknight_watk12    =[    $w_attack12,    dknight_watk13    ] {ai_charge(2); ai_melee();};
void()    dknight_watk13    =[    $w_attack13,    dknight_watk14    ] {ai_charge(0);};
void()    dknight_watk14    =[    $w_attack14,    dknight_watk15    ] {ai_charge(0);};
void()    dknight_watk15    =[    $w_attack15,    dknight_watk16    ] {ai_charge(0); dknight_sword_sound();};
void()    dknight_watk16    =[    $w_attack16,    dknight_watk17    ] {ai_charge(1);};
void()    dknight_watk17    =[    $w_attack17,    dknight_watk18    ] {monster_footstep(FALSE); ai_charge(1); ai_melee();};
void()    dknight_watk18    =[    $w_attack18,    dknight_watk19    ] {ai_charge(3); ai_melee();};
void()    dknight_watk19    =[    $w_attack19,    dknight_watk20    ] {ai_charge(4); ai_melee();};
void()    dknight_watk20    =[    $w_attack20,    dknight_watk21    ] {ai_charge(6);};
void()    dknight_watk21    =[    $w_attack21,    dknight_watk22    ] {ai_charge(7);};
void()    dknight_watk22    =[    $w_attack22,    dknight_run1    ] {ai_charge(3);};

//============================================================================
// Cycle between 3 melee attacks
// Attack 1 - Slice (one hand slashing run attack)
// Attack 2 - Smash (chop overhead to ground)
// Attack 3 - W_Attack (two handed slashing attack)

//----------------------------------------------------------------------
void() dknight_melee =
{
    self.meleeattack = self.meleeattack + 1;

    if (self.meleeattack == 1) dknight_slice1 ();
    else if (self.meleeattack == 2) dknight_smash1 ();
    else {
        dknight_watk1 ();
        self.meleeattack = 0;
    }
};

//===========================================================================
// Hell Knight - 11 frames / Death Knight - 14 frames (skipped 1, 10,14)
// Death Knight frames trimmed down to match Hell Knight (damage ratio)

void()    dknight_magicc1 =[    $magicc2,    dknight_magicc2    ] {ai_face();};
void()    dknight_magicc2 =[    $magicc3,    dknight_magicc3    ] {ai_face();};
void()    dknight_magicc3 =[    $magicc4,    dknight_magicc4    ] {ai_face();};
void()    dknight_magicc4 =[    $magicc5,    dknight_magicc5    ] {ai_face();};
void()    dknight_magicc5 =[    $magicc6,    dknight_magicc6    ] {ai_face();};
void()    dknight_magicc6 =[    $magicc7,    dknight_magicc7    ] {hknight_shot(-2);};
void()    dknight_magicc7 =[    $magicc8,    dknight_magicc8    ] {hknight_shot(-1);};
void()    dknight_magicc8 =[    $magicc9,    dknight_magicc9    ] {hknight_shot(0);};
void()    dknight_magicc9 =[    $magicc11,    dknight_magicc10] {hknight_shot(1);};
void()    dknight_magicc10 =[    $magicc12,    dknight_magicc11] {hknight_shot(2);};
void()    dknight_magicc11 =[    $magicc13,    dknight_run1] {hknight_shot(3);};

//============================================================================
void()    dknight_painA1    =[    $painA1,        dknight_painA2    ] {};
void()    dknight_painA2    =[    $painA2,        dknight_painA3    ] {};
void()    dknight_painA3    =[    $painA3,        dknight_painA4    ] {};
void()    dknight_painA4    =[    $painA4,        dknight_painA5    ] {};
void()    dknight_painA5    =[    $painA5,        dknight_run1    ] {};

//----------------------------------------------------------------------
void()    dknight_painD1    =[    $painD1,        dknight_painD2    ] {};
void()    dknight_painD2    =[    $painD2,        dknight_painD3    ] {};
void()    dknight_painD3    =[    $painD3,        dknight_painD4    ] {};
void()    dknight_painD4    =[    $painD4,        dknight_painD5    ] {monster_footstep(FALSE); };
void()    dknight_painD5    =[    $painD5,        dknight_painD6    ] {};
void()    dknight_painD6    =[    $painD6,        dknight_painD7    ] {};
void()    dknight_painD7    =[    $painD7,        dknight_painD8    ] {};
void()    dknight_painD8    =[    $painD8,        dknight_painD9    ] {monster_footstep(FALSE); };
void()    dknight_painD9    =[    $painD9,        dknight_painD10    ] {};
void()    dknight_painD10    =[    $painD10,        dknight_painD11    ] {};
void()    dknight_painD11    =[    $painD11,        dknight_run1    ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage)    dknight_pain =
{
    if (self.spawnflags & MON_GHOST_ONLY) return;
    
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1) {
            // Randomly pick which pain animation to play
            if (random() < 0.85) dknight_painA1 ();    // classic, body recoil
            else {
                dknight_painD1 ();                // Cool stumble, hand over mouth
                self.pain_finished = time + 2;    // long animation
            }
        }
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 1.1;
            self.axhitme = 0;
            dknight_painD1 ();
        }
    }
};

//============================================================================
void()    dknight_die1    =[    $death1,    dknight_die2    ] {ai_forward(10);};
void()    dknight_die2    =[    $death2,    dknight_die3    ] {monster_check_gib();ai_forward(8);};
void()    dknight_die3    =[    $death3,    dknight_die4    ] {monster_check_gib();
    self.solid = SOLID_NOT; ai_forward(7);};
void()    dknight_die4    =[    $death4,    dknight_die5    ] {};
void()    dknight_die5    =[    $death5,    dknight_die6    ] {};
void()    dknight_die6    =[    $death6,    dknight_die7    ] {
    sound (self, CHAN_WEAPON, GIB_SOUND_METALA, 1, ATTN_NORM);};
void()    dknight_die7    =[    $death7,    dknight_die8    ] {};
void()    dknight_die8    =[    $death8,    dknight_die9    ] {ai_forward(10);};
void()    dknight_die9    =[    $death9,    dknight_die10    ] {ai_forward(11);};
void()    dknight_die10    =[    $death10,    dknight_die11    ] {};
void()    dknight_die11    =[    $death11,    dknight_die12    ] {monster_death_postcheck();};
void()    dknight_die12    =[    $death12,    dknight_die12    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void()    dknight_dieb1    =[    $deathb1,    dknight_dieb2    ] {};
void()    dknight_dieb2    =[    $deathb2,    dknight_dieb3    ] {monster_check_gib();};
void()    dknight_dieb3    =[    $deathb3,    dknight_dieb4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void()    dknight_dieb4    =[    $deathb4,    dknight_dieb5    ] {};
void()    dknight_dieb5    =[    $deathb5,    dknight_dieb6    ] {};
void()    dknight_dieb6    =[    $deathb6,    dknight_dieb7    ] {};
void()    dknight_dieb7    =[    $deathb7,    dknight_dieb8    ] {};
void()    dknight_dieb8    =[    $deathb8,    dknight_dieb9    ] {};
void()    dknight_dieb9    =[    $deathb9,    dknight_dieb10    ] {
    sound (self, CHAN_WEAPON, GIB_SOUND_METALB, 1, ATTN_NORM);};
void()    dknight_dieb10    =[    $deathb10,    dknight_dieb11    ] {};
void()    dknight_dieb11    =[    $deathb11,    dknight_dieb12    ] {};
void()    dknight_dieb12    =[    $deathb12,    dknight_dieb13    ] {};
void()    dknight_dieb13    =[    $deathb13,    dknight_dieb14    ] {};
void()    dknight_dieb14    =[    $deathb14,    dknight_dieb15    ] {};
void()    dknight_dieb15    =[    $deathb15,    dknight_dieb16    ] {};
void()    dknight_dieb16    =[    $deathb16,    dknight_dieb17    ] {};
void()    dknight_dieb17    =[    $deathb17,    dknight_dieb18    ] {};
void()    dknight_dieb18    =[    $deathb18,    dknight_dieb19    ] {};
void()    dknight_dieb19    =[    $deathb19,    dknight_dieb20    ] {monster_death_postcheck();};
void()    dknight_dieb20    =[    $deathb20,    dknight_dieb20    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() dknight_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, self.death_sound, 1, ATTN_NORM);
        if (random() > 0.5) dknight_die1 ();
        else dknight_dieb1 ();
    }
};


//============================================================================
// The Death Knight ghost system (this is not a monster)
//============================================================================
void() dknight_ghostnull = {};
//----------------------------------------------------------------------
void() dknight_ghost_glow =
{
    if (self.attachment) {
        self.attachment.frame = self.frame;
        setorigin(self.attachment, self.origin);
        if (random() < 0.5) self.attachment.alpha = 0.02;
        else self.attachment.alpha = 0.05;
        self.attachment.angles = self.angles;
        self.attachment.velocity = self.velocity;
    }
};
//----------------------------------------------------------------------
void() dknight_ghost_particle =
{
    self.alpha = 0.5 + (0.1*random());
    self.pos1 = '0 0 0';
    self.pos1_x = crandom()*16;
    self.pos1_y = crandom()*16;
    self.pos1_z = crandom()*24;
    if (random() < 0.5) 
        particle_explode(self.origin+self.pos1, 2, 1, PARTICLE_BURST_WHITE, PARTICLE_BURST_LOSTUP);
    if (self.idletimer < time) {
        self.idletimer = time + 7 + (random() * 3);
        self.lefty = 1 - self.lefty;
        if (self.lefty) sound (self, CHAN_VOICE, self.idle_sound, 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, self.idle_sound2, 1, ATTN_NORM);
    }
};

//----------------------------------------------------------------------
// The use function is for triggering the ghost to move
void() dknight_ghost_use = { 
    // Check for a delay target route (using target2)
    if (self.estate == ESTATE_ON && self.target2 != "") {
        self.target = self.target2;
        self.target2 = "";
        self.estate = ESTATE_OFF;
    }
    // Always go to an on state
    self.estate_on();
};

//----------------------------------------------------------------------
void() dknight_ghost_fade =
{
    if (self.estate == ESTATE_ON) return;    
    self.alpha = self.alpha - FADEMODEL_TIME;

    if (self.alpha <= 0) setmodel (self, "");
    else self.nextthink = time + FADEMODEL_TIME;
};
//----------------------------------------------------------------------
void() dknight_ghost_off = {
    // Block if already off
    if (self.estate == ESTATE_OFF) return;    

    self.estate = ESTATE_OFF;
    particle_explode(self.origin+'0 0 -20', 75, 2, PARTICLE_BURST_FIRE, PARTICLE_BURST_UPWARD);
    self.flags = self.effects = 0;        // Always reset
    self.takedamage = DAMAGE_NO;        // No pain/death triggers
    self.movetype = MOVETYPE_NONE;        // Stationary
    self.solid = SOLID_NOT;                // no world interaction
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);        // No size, no impact
    self.velocity = self.avelocity = '0 0 0';    // Reset velocity
    if (self.attachment) {
        self.attachment.movetype = MOVETYPE_NONE;    // Stationary
        self.attachment.solid = SOLID_NOT;            // no world interaction
        setmodel(self.attachment,"");
        self.attachment.velocity = '0 0 0';
    }
    sound (self, CHAN_VOICE, SOUND_EMPTY, 1, ATTN_NORM);
    self.think = dknight_ghost_fade;
    self.nextthink = time + FADEMODEL_TIME;
}

//----------------------------------------------------------------------
void() dknight_ghost_on = {
    // Block if already ON
    if (self.estate == ESTATE_ON) return;    
    
    self.estate = ESTATE_ON;
    particle_explode(self.origin, 30, 2, PARTICLE_BURST_WHITE, PARTICLE_BURST_SKULLUP);
    self.flags = self.effects = 0;        // Always reset
    self.takedamage = DAMAGE_NO;        // No pain/death triggers
    self.movetype = MOVETYPE_NONE;        // Regular walking
    self.solid = SOLID_NOT;                // no world interaction
    setmodel(self, self.mdl);                    // Setup model
    if (self.exactskin) self.skin = self.exactskin;
    if (CheckZeroVector(self.bbmins)) monster_bbox();
    setsize (self, self.bbmins, self.bbmaxs);    // Restore BB size
    self.velocity = self.avelocity = '0 0 0';    // Reset velocity
    
    // Setup ghost glow effect
    if (!self.attachment) self.attachment = spawn();
    self.attachment.movetype = MOVETYPE_NOCLIP;    // No movement
    self.attachment.solid = SOLID_NOT;            // no world interaction
    setmodel(self.attachment, self.headmdl);
    setsize(self.attachment,VEC_ORIGIN, VEC_ORIGIN);
    setorigin(self.attachment, self.origin);

    // Check for any targets to walk toward
    if (self.target != "") {
        self.movetarget = find(world, targetname, self.target);
        if (self.movetarget) {
            if (self.movetarget.classtype == CT_PATHCORNER) {
                self.goalentity = self.movetarget;
                self.ideal_yaw = vectoyaw(self.goalentity.origin - self.origin);
                self.th_walk();
                return;
            }
        }
    }
    // No valid target, just stand around
    self.movetarget = self.goalentity = world;
    self.th_stand();
}

//============================================================================
void() dknight_sword_sound =
{
    self.lip = random();
    if (self.lip < 0.25) sound (self, CHAN_WEAPON, "weapons/sword1a.wav", 1, ATTN_NORM);
    else if (self.lip < 0.5) sound (self, CHAN_WEAPON, "weapons/sword2a.wav", 1, ATTN_NORM);
    else if (self.lip < 0.75) sound (self, CHAN_WEAPON, "weapons/sword1b.wav", 1, ATTN_NORM);
    else sound (self, CHAN_WEAPON, "weapons/sword2b.wav", 1, ATTN_NORM);
};

/*======================================================================
QUAKED monster_dknight (1 0 0) (-16 -16 -24) (16 16 40) Ambush
======================================================================*/
void() monster_dknight =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_dknight.mdl";            // New Hell Knight
    self.headmdl = "progs/h_dknight.mdl";
    self.gib1mdl = "progs/w_dknightsword.mdl";    // Unique sword
    self.gib2mdl = "progs/gib_knfoot_l.mdl";    // left foot
    self.gib3mdl = "progs/gib_knfoot_r.mdl";    // right foot

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_HKN);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    precache_model (self.gib1mdl);

    self.gib1sound = GIB_IMPACT_METALA;
    if (random() < 0.5) self.gib2mdl = "";
    if (random() < 0.5) self.gib3mdl = "";

    self.idle_sound = "hknight/idle.wav";
    precache_sound (self.idle_sound);
    
    // Firing of magic spikes (first 0.1s must loop)
    precache_sound ("hknight/attack1.wav");
    
    // TE_KNIGHTSPIKE = Magic spike hitting the wall
    // Has got to be in this location because the 
    // filename is hardcoded (engine.exe)
    precache_sound ("hknight/hit.wav");                

    self.sight_sound = "hknight/sight1.wav";
    precache_sound (self.sight_sound);

    self.pain_sound = "hknight/pain1.wav";
    self.death_sound = "hknight/death1.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.death_sound);

    // New sword sweep/slash sounds
    precache_sound ("weapons/sword1a.wav");
    precache_sound ("weapons/sword1b.wav");
    precache_sound ("weapons/sword2a.wav");
    precache_sound ("weapons/sword2b.wav");
    
    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
        precache_model (MODEL_PROJ_HKNGRN);
    }

    // dknight model does not support stone skins
    //precache_stonegibs();
    
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    if (self.health < 1) self.health = 250;
    self.gibhealth = -40;
    self.gibbed = FALSE;
    self.pain_flinch = 40;
    self.pain_longanim = TRUE;            // can be chopped with shadow axe
    self.pain_timeout = 1.5;            // Higher than a knight
    self.steptype = FS_TYPEHEAVY;
    self.deathstring = " was cleaved by a Death Knight\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    // randomize last melee attack
    self.meleeattack = rint(random()*3);    

    self.classtype = CT_MONDKNIGHT;
    self.classgroup = CG_DKNIGHT;
    self.classmove = MON_MOVEWALK;
    
    // Special ghost prop, stands around or walks
    if (self.spawnflags & MON_GHOST_ONLY) {
        self.headmdl = "progs/mon_dknight_glow.mdl";
        precache_model (self.headmdl);
        
        self.idle_sound = "dknight/ghost_idle1.wav";
        self.idle_sound2 = "dknight/ghost_idle2.wav";
        precache_sound (self.idle_sound);
        precache_sound (self.idle_sound2);
        
        self.steptype = FS_FLYING;
        self.exactskin = 2;
        self.alpha = 0.1;
        self.nomonstercount = TRUE;
        self.gibhealth = MON_NEVERGIB;
        self.health = LARGE_TIMER;
        self.yaw_speed = 20;
        
        self.th_stand = dknight_stand1;
        self.th_walk = dknight_walk1;
        self.th_run = dknight_walk1;
        
        // The ghost is a scenery prop, not a monster
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = dknight_ghost_on;
        self.estate_off = dknight_ghost_off;
        self.estate_use = dknight_ghost_use;

        // Always start off and wait for a trigger
        self.estate = ESTATE_OFF;
        self.takedamage = DAMAGE_NO;        // No pain/death triggers
        self.movetype = MOVETYPE_NONE;        // Stationary
        self.solid = SOLID_NOT;                // no world interaction
        setmodel (self, "");                // invisible
    }
    else {
        self.th_stand = dknight_stand1;
        self.th_walk = dknight_walk1;
        self.th_run = dknight_run1;
        self.th_melee = dknight_melee;
        self.th_charge = dknight_char_a1;
        self.th_missile = dknight_magicc1;
        self.th_pain = dknight_pain;
        self.th_die = dknight_die;
        // Start monster (default)
        monster_start();
    }
};

//----------------------------------------------------------------------
void() monster_dknightghost =
{
    self.spawnflags = MON_GHOST_ONLY;
    monster_dknight();
};
