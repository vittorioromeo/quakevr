/*======================================================================
Zombie KNIGHT (undead version of original knight)
======================================================================*/
$cd id1/models/zombiek
$origin    0 0 24
$base base
$skin skin

// (001 - 015) Default stand - breathing
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8
$frame stand9 stand10 stand11 stand12 stand13 stand14 stand15

// (016 - 029) Default walk dragging left foot
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 
$frame walk9 walk10 walk11 walk12 walk13 walk14

// (030 - 040) Default run, fast for a zombie
$frame run1 run2 run3 run4 run5 run6 run7 run8 run9 run10 run11

// (041 - 052) Still moving forward
$frame paina1 paina2 paina3 paina4 paina5 paina6 paina7 paina8
$frame paina9 paina10 paina11 paina12

// (053 - 077) Stagger backwards, long animation
$frame painb1 painb2 painb3 painb4 painb5 painb6 painb7 painb8
$frame painb9 painb10 painb11 painb12 painb13 painb14 painb15 painb16
$frame painb17 painb18 painb19 painb20 painb21 painb22 painb23 painb24 painb25

// (078 - 098) Fall down to knees and get back up
$frame painc1 painc2 painc3 painc4 painc5 painc6 painc7 painc8
$frame painc9 painc10 painc11 painc12 painc13 painc14 painc15 painc16
$frame painc17 painc18 painc19 painc20 painc21

// (099 - 121) Fall down FORWARD (paind12 = on floor)
$frame paind1 paind2 paind3 paind4 paind5 paind6 paind7 paind8
$frame paind9 paind10 paind11 paind12 paind13 paind14 paind15 paind16
$frame paind17 paind18 paind19 paind20 paind21 paind22 paind23

// (122 - 152) Fall down BACKWARD (paine12 = on floor)
$frame paine1 paine2 paine3 paine4 paine5 paine6 paine7 paine8
$frame paine9 paine10 paine11 paine12 paine13 paine14 paine15 paine16
$frame paine17 paine18 paine19 paine20 paine21 paine22 paine23 paine24
$frame paine25 paine26 paine27 paine28 paine29 paine30 paine31

// (153 - 171) Melee Attack 1 - Smash (chop overhead to ground)
$frame atta1 atta2 atta3 atta4 atta5 atta6 atta7 atta8
$frame atta9 atta10 atta11 atta12 atta13 atta14 atta15 atta16 atta17 atta18 atta19

// (172 - 188) Melee Attack 2 - Stationary over arm swing 
$frame attb1 attb2 attb3 attb4 attb5 attb6 attb7 attb8
$frame attb9 attb10 attb11 attb12 attb13 attb14 attb15 attb16 attb17

// (189 - 203) Melee Attack 3 - Stationary sideways swing
$frame attc1 attc2 attc3 attc4 attc5 attc6 attc7 attc8
$frame attc9 attc10 attc11 attc12 attc13 attc14 attc15

// (204 - 223) idle 2 - scratch brains
$frame idleb1 idleb2 idleb3 idleb4 idleb5 idleb6 idleb7 idleb8
$frame idleb9 idleb10 idleb11 idleb12 idleb13 idleb14 idleb15 idleb16
$frame idleb17 idleb18 idleb19 idleb20

// (224 - 238) idle 3 - swing head LEFT
$frame idlec1 idlec2 idlec3 idlec4 idlec5 idlec6 idlec7 idlec8
$frame idlec9 idlec10 idlec11 idlec12 idlec13 idlec14 idlec15

// (239 - 253) idle 4 - swing head RIGHT
$frame idled1 idled2 idled3 idled4 idled5 idled6 idled7 idled8
$frame idled9 idled10 idled11 idled12 idled13 idled14 idled15

void(float type) zombiek_sound;
void() zombiek_knockdown;

//=============================================================================
void() zombiek_standB1    =[    $idleb1,        zombiek_standB2    ] {ai_stand();};
void() zombiek_standB2    =[    $idleb2,        zombiek_standB3    ] {ai_stand();};
void() zombiek_standB3    =[    $idleb3,        zombiek_standB4    ] {ai_stand();};
void() zombiek_standB4    =[    $idleb4,        zombiek_standB5    ] {ai_stand();};
void() zombiek_standB5    =[    $idleb5,        zombiek_standB6    ] {ai_stand();};
void() zombiek_standB6    =[    $idleb6,        zombiek_standB7    ] {ai_stand();};
void() zombiek_standB7    =[    $idleb7,        zombiek_standB8    ] {ai_stand();};
void() zombiek_standB8    =[    $idleb8,        zombiek_standB9    ] {ai_stand();};
void() zombiek_standB9    =[    $idleb9,        zombiek_standB10    ] {ai_stand();};
void() zombiek_standB10    =[    $idleb10,        zombiek_standB11    ] {ai_stand();};
void() zombiek_standB11    =[    $idleb11,        zombiek_standB12    ] {ai_stand();};
void() zombiek_standB12    =[    $idleb12,        zombiek_standB13    ] {ai_stand();};
void() zombiek_standB13    =[    $idleb13,        zombiek_standB14    ] {ai_stand();};
void() zombiek_standB14    =[    $idleb14,        zombiek_standB15    ] {ai_stand();};
void() zombiek_standB15    =[    $idleb15,        zombiek_standB16    ] {ai_stand();};
void() zombiek_standB16    =[    $idleb16,        zombiek_standB17    ] {ai_stand();};
void() zombiek_standB17    =[    $idleb17,        zombiek_standB18    ] {ai_stand();};
void() zombiek_standB18    =[    $idleb18,        zombiek_standB19    ] {ai_stand();};
void() zombiek_standB19    =[    $idleb19,        zombiek_standB20    ] {ai_stand();};
void() zombiek_standB20    =[    $idleb20,        zombiek_stand1    ] {ai_stand();};

//=============================================================================
void() zombiek_standC1    =[    $idlec1,        zombiek_standC2    ] {ai_stand();};
void() zombiek_standC2    =[    $idlec2,        zombiek_standC3    ] {ai_stand();};
void() zombiek_standC3    =[    $idlec3,        zombiek_standC4    ] {ai_stand();};
void() zombiek_standC4    =[    $idlec4,        zombiek_standC5    ] {ai_stand();};
void() zombiek_standC5    =[    $idlec5,        zombiek_standC6    ] {ai_stand();};
void() zombiek_standC6    =[    $idlec6,        zombiek_standC7    ] {ai_stand();};
void() zombiek_standC7    =[    $idlec7,        zombiek_standC8    ] {ai_stand();};
void() zombiek_standC8    =[    $idlec8,        zombiek_standC9    ] {ai_stand();};
void() zombiek_standC9    =[    $idlec9,        zombiek_standC10    ] {ai_stand();};
void() zombiek_standC10    =[    $idlec10,        zombiek_standC11    ] {ai_stand();};
void() zombiek_standC11    =[    $idlec11,        zombiek_standC12    ] {ai_stand();};
void() zombiek_standC12    =[    $idlec12,        zombiek_standC13    ] {ai_stand();};
void() zombiek_standC13    =[    $idlec13,        zombiek_standC14    ] {ai_stand();};
void() zombiek_standC14    =[    $idlec14,        zombiek_standC15    ] {ai_stand();};
void() zombiek_standC15    =[    $idlec15,        zombiek_stand1    ] {ai_stand();};

//=============================================================================
void() zombiek_standD1    =[    $idled1,        zombiek_standD2    ] {ai_stand();};
void() zombiek_standD2    =[    $idled2,        zombiek_standD3    ] {ai_stand();};
void() zombiek_standD3    =[    $idled3,        zombiek_standD4    ] {ai_stand();};
void() zombiek_standD4    =[    $idled4,        zombiek_standD5    ] {ai_stand();};
void() zombiek_standD5    =[    $idled5,        zombiek_standD6    ] {ai_stand();};
void() zombiek_standD6    =[    $idled6,        zombiek_standD7    ] {ai_stand();};
void() zombiek_standD7    =[    $idled7,        zombiek_standD8    ] {ai_stand();};
void() zombiek_standD8    =[    $idled8,        zombiek_standD9    ] {ai_stand();};
void() zombiek_standD9    =[    $idled9,        zombiek_standD10    ] {ai_stand();};
void() zombiek_standD10    =[    $idled10,        zombiek_standD11    ] {ai_stand();};
void() zombiek_standD11    =[    $idled11,        zombiek_standD12    ] {ai_stand();};
void() zombiek_standD12    =[    $idled12,        zombiek_standD13    ] {ai_stand();};
void() zombiek_standD13    =[    $idled13,        zombiek_standD14    ] {ai_stand();};
void() zombiek_standD14    =[    $idled14,        zombiek_standD15    ] {ai_stand();};
void() zombiek_standD15    =[    $idled15,        zombiek_stand1    ] {ai_stand();};

//----------------------------------------------------------------------
void() zombiek_standE1    =[    $idlec1,        zombiek_standE2    ] {ai_stand();};
void() zombiek_standE2    =[    $idlec2,        zombiek_standE3    ] {ai_stand();};
void() zombiek_standE3    =[    $idlec3,        zombiek_standE4    ] {ai_stand();};
void() zombiek_standE4    =[    $idlec4,        zombiek_standE5    ] {ai_stand();};
void() zombiek_standE5    =[    $idlec5,        zombiek_standE6    ] {ai_stand();};
void() zombiek_standE6    =[    $idlec6,        zombiek_standE7    ] {ai_stand();};
void() zombiek_standE7    =[    $idlec7,        zombiek_standE8    ] {ai_stand();};
void() zombiek_standE8    =[    $idlec8,        zombiek_standE9    ] {ai_stand();};
void() zombiek_standE9    =[    $idlec9,        zombiek_standE10    ] {ai_stand();};
void() zombiek_standE10    =[    $idlec10,        zombiek_standE11    ] {ai_stand();};
void() zombiek_standE11    =[    $idlec11,        zombiek_standE12    ] {ai_stand();};
void() zombiek_standE12    =[    $idlec12,        zombiek_standE13    ] {ai_stand();};
void() zombiek_standE13    =[    $idlec13,        zombiek_standE14    ] {ai_stand();};
void() zombiek_standE14    =[    $idlec14,        zombiek_standE15    ] {ai_stand();};
void() zombiek_standE15    =[    $idlec15,        zombiek_standE16    ] {ai_stand();};
void() zombiek_standE16    =[    $idled1,        zombiek_standE17    ] {ai_stand();};
void() zombiek_standE17    =[    $idled2,        zombiek_standE18    ] {ai_stand();};
void() zombiek_standE18    =[    $idled3,        zombiek_standE19    ] {ai_stand();};
void() zombiek_standE19    =[    $idled4,        zombiek_standE20    ] {ai_stand();};
void() zombiek_standE20    =[    $idled5,        zombiek_standE21    ] {ai_stand();};
void() zombiek_standE21    =[    $idled6,        zombiek_standE22    ] {ai_stand();};
void() zombiek_standE22    =[    $idled7,        zombiek_standE23    ] {ai_stand();};
void() zombiek_standE23    =[    $idled8,        zombiek_standE24    ] {ai_stand();};
void() zombiek_standE24    =[    $idled9,        zombiek_standE25    ] {ai_stand();};
void() zombiek_standE25    =[    $idled10,        zombiek_standE26    ] {ai_stand();};
void() zombiek_standE26    =[    $idled11,        zombiek_standE27    ] {ai_stand();};
void() zombiek_standE27    =[    $idled12,        zombiek_standE28    ] {ai_stand();};
void() zombiek_standE28    =[    $idled13,        zombiek_standE29    ] {ai_stand();};
void() zombiek_standE29    =[    $idled14,        zombiek_standE30    ] {ai_stand();};
void() zombiek_standE30    =[    $idled15,        zombiek_stand1    ] {ai_stand();};

//----------------------------------------------------------------------
void() zombiek_stand1    =[    $stand1,        zombiek_stand2    ] {ai_stand();};
void() zombiek_stand2    =[    $stand2,        zombiek_stand3    ] {ai_stand();};
void() zombiek_stand3    =[    $stand3,        zombiek_stand4    ] {ai_stand();};
void() zombiek_stand4    =[    $stand4,        zombiek_stand5    ] {ai_stand();};
void() zombiek_stand5    =[    $stand5,        zombiek_stand6    ] {ai_stand();};
void() zombiek_stand6    =[    $stand6,        zombiek_stand7    ] {ai_stand();};
void() zombiek_stand7    =[    $stand7,        zombiek_stand8    ] {ai_stand();};
void() zombiek_stand8    =[    $stand8,        zombiek_stand9    ] {ai_stand();};
void() zombiek_stand9    =[    $stand9,        zombiek_stand10    ] {ai_stand();};
void() zombiek_stand10    =[    $stand10,        zombiek_stand11    ] {ai_stand();};
void() zombiek_stand11    =[    $stand11,        zombiek_stand12    ] {ai_stand();};
void() zombiek_stand12    =[    $stand12,        zombiek_stand13    ] {ai_stand();};
void() zombiek_stand13    =[    $stand13,        zombiek_stand14    ] {ai_stand();};
void() zombiek_stand14    =[    $stand14,        zombiek_stand15    ] {ai_stand();};
void() zombiek_stand15    =[    $stand15,        zombiek_stand1    ] {
    self.idlebusy = FALSE;
    self.solid = SOLID_SLIDEBOX;
    if (random() < MON_IDLE_ANIMATION) {
        self.idlebusy = TRUE;
        // Make sure the idle animations don't repeat in a row
        self.lefty = self.lip;
        while (self.lefty == self.lip) { self.lefty = rint(random()*4);}
        self.lip = self.lefty;
        if (self.lip < 1) self.think = zombiek_standB1;                // Pick brains!
        else if (self.lip < 2) self.think = zombiek_standE1;    // Look BOTH
        else if (self.lip < 3) self.think = zombiek_standC1;    // Look LEFT
        else self.think = zombiek_standD1;                                        // Look RIGHT
    }
    ai_stand();
};

//============================================================================
void() zombiek_walk1    =[    $walk1,        zombiek_walk2    ] {
    self.inpain = 0; self.solid = SOLID_SLIDEBOX; ai_walk(1);};
void() zombiek_walk2    =[    $walk2,        zombiek_walk3    ] {ai_walk(1);};
void() zombiek_walk3    =[    $walk3,        zombiek_walk4    ] {ai_walk(4);};
void() zombiek_walk4    =[    $walk4,        zombiek_walk5    ] {ai_walk(4);};
void() zombiek_walk5    =[    $walk5,        zombiek_walk6    ] {ai_walk(2);};
void() zombiek_walk6    =[    $walk6,        zombiek_walk7    ] {ai_walk(1);};
void() zombiek_walk7    =[    $walk7,        zombiek_walk8    ] {ai_walk(1);};
void() zombiek_walk8    =[    $walk8,        zombiek_walk9    ] {ai_walk(1);monster_footstep(FALSE);};
void() zombiek_walk9    =[    $walk9,        zombiek_walk10] {ai_walk(1);};
void() zombiek_walk10    =[    $walk10,    zombiek_walk11] {ai_walk(1);};
void() zombiek_walk11    =[    $walk11,    zombiek_walk12] {ai_walk(1);};
void() zombiek_walk12    =[    $walk12,    zombiek_walk13] {ai_walk(1);monster_footstep(TRUE);};    // Scrapping Clubfoot
void() zombiek_walk13    =[    $walk13,    zombiek_walk14] {ai_walk(1);};
void() zombiek_walk14    =[    $walk14,    zombiek_walk1    ] {zombiek_sound(2);ai_walk(1);};

//============================================================================
void() zombiek_run1        =[    $run1,        zombiek_run2    ] {
    self.inpain = 0; self.solid = SOLID_SLIDEBOX; ai_run(12);};
void() zombiek_run2        =[    $run2,        zombiek_run3    ] {ai_run(12);};
void() zombiek_run3        =[    $run3,        zombiek_run4    ] {ai_run(13);};
void() zombiek_run4        =[    $run4,        zombiek_run5    ] {ai_run(12);};
void() zombiek_run5        =[    $run5,        zombiek_run6    ] {ai_run(12);monster_footstep(FALSE);};
void() zombiek_run6        =[    $run6,        zombiek_run7    ] {ai_run(12);};
void() zombiek_run7        =[    $run7,        zombiek_run8    ] {ai_run(12);};
void() zombiek_run8        =[    $run8,        zombiek_run9    ] {ai_run(11);};
void() zombiek_run9        =[    $run9,        zombiek_run10    ] {ai_run(14);};
void() zombiek_run10    =[    $run10,        zombiek_run11    ] {ai_run(13);};
void() zombiek_run11    =[    $run11,        zombiek_run1    ] {ai_run(11); 
    self.steptype = FS_TYPEHEAVY;        // Heavy clubfoot
    monster_footstep(FALSE);
    self.steptype = FS_TYPESLOW;        // Normal zombie foot
};

//===========================================================================
// Melee Attack 2 - Stationary over arm swing (17->12 frames)
// Removed frames 1,3,4,5,7
void()    zombiek_attb1    =[    $attb2,        zombiek_attb6 ] {ai_face();};
void()    zombiek_attb6    =[    $attb6,        zombiek_attb8    ] {ai_face();};
void()    zombiek_attb8    =[    $attb8,        zombiek_attb9    ] {ai_face();zombiek_sound(7);};
void()    zombiek_attb9    =[    $attb9,        zombiek_attb10] {ai_face();};
void()    zombiek_attb10=[    $attb10,    zombiek_attb11] {ai_face();ai_melee();};
void()    zombiek_attb11=[    $attb11,    zombiek_attb12] {ai_melee();};
void()    zombiek_attb12=[    $attb12,    zombiek_attb13] {ai_melee();};
void()    zombiek_attb13=[    $attb13,    zombiek_attb14] {};
void()    zombiek_attb14=[    $attb14,    zombiek_attb15] {};
void()    zombiek_attb15=[    $attb15,    zombiek_attb16] {};
void()    zombiek_attb16=[    $attb16,    zombiek_attb17] {};
void()    zombiek_attb17=[    $attb17,    zombiek_run1    ] {};

//----------------------------------------------------------------------
// Melee Attack 3 - Stationary sideways swing (15->12 frames)
// Removed frames 1,3,15
void()    zombiek_attc1    =[    $attc2,        zombiek_attc4    ] {ai_face();};
void()    zombiek_attc4    =[    $attc4,        zombiek_attc5    ] {ai_face();};
void()    zombiek_attc5    =[    $attc5,        zombiek_attc6    ] {zombiek_sound(7); ai_face();};
void()    zombiek_attc6    =[    $attc6,        zombiek_attc7    ] {ai_face();};
void()    zombiek_attc7    =[    $attc7,        zombiek_attc8    ] {ai_face();ai_melee();};
void()    zombiek_attc8    =[    $attc8,        zombiek_attc9    ] {ai_melee();};
void()    zombiek_attc9    =[    $attc9,        zombiek_attc10] {ai_melee();};
void()    zombiek_attc10=[    $attc10,    zombiek_attc11] {};
void()    zombiek_attc11=[    $attc11,    zombiek_attc12] {};
void()    zombiek_attc12=[    $attc12,    zombiek_attc13] {};
void()    zombiek_attc13=[    $attc13,    zombiek_attc14] {};
void()    zombiek_attc14=[    $attc14,    zombiek_run1    ] {};

//----------------------------------------------------------------------
void() zombiek_melee =
{
    ai_face ();        // Turn towards enemy target
    walkmove (self.ideal_yaw, 20);

    // Randomly select melee attacks
    if (random() < 0.5) zombiek_attb1();    // Stationary over arm swing 
    else zombiek_attc1();                    // Stationary sideways swing
};

//============================================================================
// Jump + melee attack
// Melee Attack 1 - Smash, chop overhead to ground (19->10/11 frames)
//============================================================================
void()    zombiek_JumpTouch =
{
    local float ldmg;
    
    if (self.health < 1) return;
    ai_jumpbreakable(20);            // Damage any breakables
    self.touch = SUB_Null;            // No more touching
    self.think = self.th_jumpexit;    // Exit frame
    self.jumptouch = other;            // Keep track of touch target

    if ( CanDamage(other, self) ) {
        if ( vlen(self.velocity) > 300 ) {
          ldmg = 10 + 10*random();
            T_Damage (other, self, self, ldmg, DAMARMOR);    
            spawn_touchblood (self, self.enemy, ldmg*3);
        }
    }

    // Is the zombie knight floating in the air?
    if (!checkbottom(self)) {
        // Is the zombie knight standing on something?        
        if (self.flags & FL_ONGROUND) {    
            // Do an extra jump if got the count
            if (self.count < 2)  self.think = self.th_jump;
        }
    }

    // Next timer
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void()    zombiek_jump1    =[    $atta1,        zombiek_jump2 ] {ai_charge(12);
    self.jump_flag = time + MONAI_JUMPZOMBTIME;
    zombiek_sound(3);    // Moan about jumping forward
};
void()    zombiek_jump2    =[    $atta2,        zombiek_jump3 ] {ai_charge(16);};
void()    zombiek_jump3    =[    $atta3,        zombiek_jump4 ] {
    ai_face();
    self.jump_flag = time + MONAI_JUMPZOMBTIME;
    self.touch = zombiek_JumpTouch;
    makevectors (self.angles);
    self.velocity = v_forward * (150 + random() * 50) + '0 0 200';
    self.origin_z = self.origin_z + 4;
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.oldorigin = self.origin;
};

// Flying through the air waiting to touch something!
void()    zombiek_jump4    =[    $atta4,        zombiek_jump5 ] {ai_face();};
void()    zombiek_jump5    =[    $atta5,        zombiek_jump6    ] {ai_face();};
void()    zombiek_jump6    =[    $atta6,        zombiek_jump6    ] {ai_face();};
void()    zombiek_jump7    =[    $atta7,        zombiek_jump7    ] {ai_face();};
void()    zombiek_jump8    =[    $atta8,        zombiek_jump9    ] {ai_face();
    // Double check monster is still falling?
    if (CheckZeroVector(self.velocity) || self.oldorigin == self.origin) {
        if (!checkbottom(self)) {
            self.ideal_yaw = random() * 360;     //random jump angle
            self.think = zombiek_jump3;
        }
    }
    self.oldorigin = self.origin;
};

//----------------------------------------------------------------------
void() zombiek_jump15;        // Chance of quick animation exit
void()    zombiek_jump9    =[    $atta9,        zombiek_jump10] {ai_melee();};
void()    zombiek_jump10=[    $atta10,    zombiek_jump11] {ai_melee();};
void()    zombiek_jump11=[    $atta11,    zombiek_jump12] {};
void()    zombiek_jump12=[    $atta12,    zombiek_jump13] {
    if (random() < 0.5) self.think = zombiek_jump15;};    // quick recovery
void()    zombiek_jump13=[    $atta13,    zombiek_jump14] {};
void()    zombiek_jump14=[    $atta14,    zombiek_jump16] {};
void()    zombiek_jump15=[    $atta15,    zombiek_jump16] {};
void()    zombiek_jump16=[    $atta16,    zombiek_jump18] {};
void()    zombiek_jump18=[    $atta18,    zombiek_run1 ] {ai_resetangles();};

/*======================================================================
  PAIN
========================================================================*/
void()    zombiek_paina1    =[    $paina1,        zombiek_paina2    ] {ai_painforward(8);};
void()    zombiek_paina2    =[    $paina2,        zombiek_paina3    ] {ai_painforward(3);};
void()    zombiek_paina3    =[    $paina3,        zombiek_paina4    ] {ai_painforward(3);};
void()    zombiek_paina4    =[    $paina4,        zombiek_paina5    ] {ai_painforward(3);monster_footstep(FALSE);};
void()    zombiek_paina5    =[    $paina5,        zombiek_paina6    ] {ai_painforward(3);};
void()    zombiek_paina6    =[    $paina6,        zombiek_paina7    ] {ai_painforward(3);};
void()    zombiek_paina7    =[    $paina7,        zombiek_paina8    ] {ai_painforward(3);};
void()    zombiek_paina8    =[    $paina8,        zombiek_paina9    ] {ai_painforward(3);};
void()    zombiek_paina9    =[    $paina9,        zombiek_paina10    ] {ai_painforward(2);monster_footstep(FALSE);};
void()    zombiek_paina10    =[    $paina10,        zombiek_paina11    ] {ai_painforward(1);};
void()    zombiek_paina11    =[    $paina11,        zombiek_paina12    ] {ai_painforward(2);};
void()    zombiek_paina12    =[    $paina12,        zombiek_run1    ] {ai_painforward(1);self.inpain = 0;};

//----------------------------------------------------------------------
void()    zombiek_painb1    =[    $painb1,        zombiek_painb2    ] {ai_painforward(8);};
void()    zombiek_painb2    =[    $painb2,        zombiek_painb3    ] {ai_pain(1);};
void()    zombiek_painb3    =[    $painb3,        zombiek_painb4    ] {ai_pain(1);};
void()    zombiek_painb4    =[    $painb4,        zombiek_painb5    ] {ai_pain(1);};
void()    zombiek_painb5    =[    $painb5,        zombiek_painb6    ] {ai_pain(1);};
void()    zombiek_painb6    =[    $painb6,        zombiek_painb7    ] {ai_pain(2);};
void()    zombiek_painb7    =[    $painb7,        zombiek_painb8    ] {ai_pain(2);};
void()    zombiek_painb8    =[    $painb8,        zombiek_painb9    ] {ai_pain(4);};
void()    zombiek_painb9    =[    $painb9,        zombiek_painb10    ] {ai_pain(4);};
void()    zombiek_painb10    =[    $painb10,        zombiek_painb11    ] {ai_pain(4);};
void()    zombiek_painb11    =[    $painb11,        zombiek_painb12    ] {ai_pain(2);monster_footstep(FALSE);};
void()    zombiek_painb12    =[    $painb12,        zombiek_painb13    ] {ai_pain(2);};
void()    zombiek_painb13    =[    $painb13,        zombiek_painb14    ] {ai_pain(3);};
void()    zombiek_painb14    =[    $painb14,        zombiek_painb15    ] {};
void()    zombiek_painb15    =[    $painb15,        zombiek_painb16    ] {};
void()    zombiek_painb16    =[    $painb16,        zombiek_painb17    ] {};
void()    zombiek_painb17    =[    $painb17,        zombiek_painb18    ] {};
void()    zombiek_painb18    =[    $painb18,        zombiek_painb19    ] {ai_painforward(3);};
void()    zombiek_painb19    =[    $painb19,        zombiek_painb20    ] {ai_painforward(3);};
void()    zombiek_painb20    =[    $painb20,        zombiek_painb21    ] {ai_painforward(3);};
void()    zombiek_painb21    =[    $painb21,        zombiek_painb22    ] {ai_painforward(3);};
void()    zombiek_painb22    =[    $painb22,        zombiek_painb23    ] {ai_painforward(3);};
void()    zombiek_painb23    =[    $painb23,        zombiek_painb24    ] {ai_painforward(3);};
void()    zombiek_painb24    =[    $painb24,        zombiek_painb25    ] {ai_painforward(3);};
void()    zombiek_painb25    =[    $painb25,        zombiek_run1    ] {ai_painforward(3);};

//----------------------------------------------------------------------
void()    zombiek_painc1    =[    $painc1,        zombiek_painc2    ] {ai_painforward(8);};
void()    zombiek_painc2    =[    $painc2,        zombiek_painc3    ] {ai_painforward(6);};
void()    zombiek_painc3    =[    $painc3,        zombiek_painc4    ] {ai_painforward(5);};
void()    zombiek_painc4    =[    $painc4,        zombiek_painc5    ] {ai_painforward(7);zombiek_sound(5);};
void()    zombiek_painc5    =[    $painc5,        zombiek_painc6    ] {ai_painforward(6);};
void()    zombiek_painc6    =[    $painc6,        zombiek_painc7    ] {ai_painforward(3);};
void()    zombiek_painc7    =[    $painc7,        zombiek_painc8    ] {ai_painforward(3);};
void()    zombiek_painc8    =[    $painc8,        zombiek_painc9    ] {};
void()    zombiek_painc9    =[    $painc9,        zombiek_painc10    ] {};

// Re-entry point for painD wakeup animation
void()    zombiek_painc10    =[    $painc10,        zombiek_painc11    ] {};
void()    zombiek_painc11    =[    $painc11,        zombiek_painc12    ] {};
void()    zombiek_painc12    =[    $painc12,        zombiek_painc13    ] {};
void()    zombiek_painc13    =[    $painc13,        zombiek_painc14    ] {};
void()    zombiek_painc14    =[    $painc14,        zombiek_painc15    ] {};
void()    zombiek_painc15    =[    $painc15,        zombiek_painc16    ] {};
void()    zombiek_painc16    =[    $painc16,        zombiek_painc17    ] {};
void()    zombiek_painc17    =[    $painc17,        zombiek_painc18    ] {};
void()    zombiek_painc18    =[    $painc18,        zombiek_painc19    ] {};
void()    zombiek_painc19    =[    $painc19,        zombiek_painc20    ] {monster_footstep(FALSE);};
void()    zombiek_painc20    =[    $painc20,        zombiek_painc21    ] {};
void()    zombiek_painc21    =[    $painc21,        zombiek_run1    ] {};

//----------------------------------------------------------------------
void()    zombiek_paind1    =[    $paind1,        zombiek_paind2    ] {self.inpain = 2; ai_painforward(8);};
void()    zombiek_paind2    =[    $paind2,        zombiek_paind3    ] {ai_painforward(4);};
void()    zombiek_paind3    =[    $paind3,        zombiek_paind4    ] {ai_painforward(4);};
void()    zombiek_paind4    =[    $paind4,        zombiek_paind5    ] {ai_painforward(4);};
void()    zombiek_paind5    =[    $paind5,        zombiek_paind6    ] {ai_painforward(3);};
void()    zombiek_paind6    =[    $paind6,        zombiek_paind7    ] {ai_painforward(2);};
void()    zombiek_paind7    =[    $paind7,        zombiek_paind8    ] {ai_painforward(2);};
void()    zombiek_paind8    =[    $paind8,        zombiek_paind9    ] {};
void()    zombiek_paind9    =[    $paind9,        zombiek_paind10    ] {};
void()    zombiek_paind10    =[    $paind10,        zombiek_paind11    ] {zombiek_sound(5);};
void()    zombiek_paind11    =[    $paind11,        zombiek_paind12    ] {self.solid = SOLID_NOT;};
void()    zombiek_paind12    =[    $paind12,        zombiek_paind12    ] {zombiek_onground(MONAI_ZOMBIEUPD);};

// Start of get up sequence
void()    zombiek_paind13    =[    $paind13,        zombiek_paind14    ] {};
void()    zombiek_paind14    =[    $paind14,        zombiek_paind15    ] {};
void()    zombiek_paind15    =[    $paind15,        zombiek_paind16    ] {};
void()    zombiek_paind16    =[    $paind16,        zombiek_paind17    ] {};
void()    zombiek_paind17    =[    $paind17,        zombiek_paind18    ] {
    monster_footstep(TRUE);ai_pain(1);};        // Scrapping feet sound while standing up
void()    zombiek_paind18    =[    $paind18,        zombiek_paind19    ] {ai_pain(1);};
void()    zombiek_paind19    =[    $paind19,        zombiek_paind20    ] {ai_pain(1);};
void()    zombiek_paind20    =[    $paind20,        zombiek_paind21    ] {ai_pain(1);};
void()    zombiek_paind21    =[    $paind21,        zombiek_paind22    ] {ai_pain(1);};
void()    zombiek_paind22    =[    $paind22,        zombiek_paind23    ] {ai_pain(2);};
void()    zombiek_paind23    =[    $paind23,        zombiek_painw10    ] {ai_pain(2);};
// Originally re-entry to C, need onfloor check
void()    zombiek_painw10    =[    $painc10,        zombiek_painw11    ] {};
void()    zombiek_painw11    =[    $painc11,        zombiek_painw12    ] {};
void()    zombiek_painw12    =[    $painc12,        zombiek_painw13    ] {};
void()    zombiek_painw13    =[    $painc13,        zombiek_painw14    ] {};
void()    zombiek_painw14    =[    $painc14,        zombiek_painw15    ] {};
void()    zombiek_painw15    =[    $painc15,        zombiek_painw16    ] {};
void()    zombiek_painw16    =[    $painc16,        zombiek_painw17    ] {};
void()    zombiek_painw17    =[    $painc17,        zombiek_painw18    ] {};
void()    zombiek_painw18    =[    $painc18,        zombiek_painw19    ] {};
void()    zombiek_painw19    =[    $painc19,        zombiek_painw20    ] {monster_footstep(FALSE);};
void()    zombiek_painw20    =[    $painc20,        zombiek_painw21    ] {};
void()    zombiek_painw21    =[    $painc21,        zombiek_run1    ] {
    // If getting up for the first time (onfloor) check for targets
    if (self.spawnflags & MON_ONFLOOR) {
        self.spawnflags = self.spawnflags - MON_ONFLOOR;
        monster_targets();
    }
};

//----------------------------------------------------------------------
void()    zombiek_paine1    =[    $paine1,        zombiek_paine2    ] {self.inpain = 2; ai_pain(8);};
void()    zombiek_paine2    =[    $paine2,        zombiek_paine3    ] {ai_pain(4);};
void()    zombiek_paine3    =[    $paine3,        zombiek_paine4    ] {ai_pain(3);};
void()    zombiek_paine4    =[    $paine4,        zombiek_paine5    ] {ai_pain(3);};
void()    zombiek_paine5    =[    $paine5,        zombiek_paine6    ] {ai_pain(3);};
void()    zombiek_paine6    =[    $paine6,        zombiek_paine7    ] {ai_pain(2);};
void()    zombiek_paine7    =[    $paine7,        zombiek_paine8    ] {ai_pain(2);};
void()    zombiek_paine8    =[    $paine8,        zombiek_paine9    ] {};
void()    zombiek_paine9    =[    $paine9,        zombiek_paine10    ] {};
void()    zombiek_paine10    =[    $paine10,        zombiek_paine11    ] {self.solid = SOLID_NOT;};
void()    zombiek_paine11    =[    $paine11,        zombiek_paine12    ] {zombiek_sound(5);};
void()    zombiek_paine12    =[    $paine12,        zombiek_paine12    ] {zombiek_onground(MONAI_ZOMBIEUPE);};

// Start of get up sequence
void()    zombiek_paine13    =[    $paine13,        zombiek_paine14    ] {
    monster_footstep(TRUE);        // Scrapping feet sound while standing up
};
void()    zombiek_paine14    =[    $paine14,        zombiek_paine15    ] {};
void()    zombiek_paine15    =[    $paine15,        zombiek_paine16    ] {};
void()    zombiek_paine16    =[    $paine16,        zombiek_paine17    ] {};
void()    zombiek_paine17    =[    $paine18,        zombiek_paine18    ] {};
void()    zombiek_paine18    =[    $paine17,        zombiek_paine19    ] {};
void()    zombiek_paine19    =[    $paine19,        zombiek_paine20    ] {
    self.steptype = FS_TYPEHEAVY;        // Heavy clubfoot
    monster_footstep(FALSE);
    self.steptype = FS_TYPESLOW;        // Normal zombie foot
};
void()    zombiek_paine20    =[    $paine20,        zombiek_paine21    ] {};
void()    zombiek_paine21    =[    $paine21,        zombiek_paine22    ] {};
void()    zombiek_paine22    =[    $paine22,        zombiek_paine23    ] {};
void()    zombiek_paine23    =[    $paine23,        zombiek_paine24    ] {};
void()    zombiek_paine24    =[    $paine24,        zombiek_paine25    ] {};
void()    zombiek_paine25    =[    $paine25,        zombiek_paine26    ] {};
void()    zombiek_paine26    =[    $paine26,        zombiek_paine27    ] {};
void()    zombiek_paine27    =[    $paine27,        zombiek_paine28    ] {ai_painforward(3);};
void()    zombiek_paine28    =[    $paine28,        zombiek_paine29    ] {ai_painforward(3);};
void()    zombiek_paine29    =[    $paine29,        zombiek_paine30    ] {ai_painforward(3);};
void()    zombiek_paine30    =[    $paine30,        zombiek_paine31    ] {ai_painforward(3);};
void()    zombiek_paine31    =[    $paine31,        zombiek_run1    ] {ai_painforward(3);
    // If getting up for the first time (onfloor) check for targets
    if (self.spawnflags & MON_ONFLOOR) {
        self.spawnflags = self.spawnflags - MON_ONFLOOR;
        monster_targets();
    }
};

/*======================================================================
 ZOMBIE PAIN
 * All damage must come through pain function to reset health
 * Be careful blocking zombie pain function with pain_finished
 * zombies react in 2 ways, minor pain (a,c,d) and full knockdown (b,e)
======================================================================*/
void(entity inflictor, entity attacker, float take) zombiek_pain =
{
    self.health = self.max_health;                // always reset health    
    if (take < MONAI_ZOMBIELOW_DAM) return;        // totally ignore
    if (self.inpain > 0) return;                // in pain animation, do nothing

    zombiek_sound(4);            // Moan about the pain
    self.inpain = 1;            // In pain, don't interrupt animation
    self.attack_elev = targzone(self, attacker);
    
    //----------------------------------------------------------------------
    // Damage 25-60, wobble or drop to ground (SSG, Quad NG)
    //----------------------------------------------------------------------
    if (take > MONAI_ZOMBIEHGH_DAM)    zombiek_knockdown();// ON THE GROUND
    //----------------------------------------------------------------------
    // Damage 9-25 random pain animation (random chance of knockdown)
    //----------------------------------------------------------------------
    else {
        self.lip = random();
        if (self.lip < 0.25) zombiek_paina1 ();            // 12 frames (drag foot)
        else if (self.lip < 0.5) zombiek_knockdown();    // ON THE GROUND
        else if (self.lip <  0.75 && self.attack_elev == TARGET_FRONT) 
            zombiek_painb1 ();                    // 25 frames (flail arms backwards)
        else zombiek_painc1 ();                    // 21 frames (fall to knees)
    }
};

/*=============================================================================
 ZOMBIE WAKEUP (check if can get up)
=============================================================================*/
void() zombiek_wakeup    = {
    if (self.gibbed) return;            // Do nothing dead
    
    self.use = SUB_Null;                // No more using at this point
    if (!self.enemy) FindTarget();        // If no enemy, look around?

    // Turn monster collision on and test if can walk forward
    self.solid = SOLID_SLIDEBOX;
    if (!walkmove (0, 0))
    {
        self.inpain = 2;                // On ground
        self.nextthink = time + 0.1;    // Keep checking
        self.think = self.th_wakeup;    // Same function
        self.solid = SOLID_NOT;            // No world interaction yet
        // Check if something has been blocking the zombie from getting up
        // This is a check for weird world/bmodel geo or monster/players
        // standing on zombies while on the ground trying to get up!
        self.count = self.count + 1;
        if (self.count > MONAI_ZOMBGRDBLOCK) {
            self.health = self.gibhealth;    // Gib time
            Killed(self, self);                // Register death
            return;
        }
    }
    else {
        self.bodyonflr = string_null;    // Body is chop proof!
        self.inpain = 1;                // Animation not finished
        zombiek_sound(6);                // Wake up sound
        self.nextthink = time + 0.1;
        self.use = monster_use;            // Just in case interruption
        if (self.zombieflr == MONAI_ZOMBIEUPD) self.think = zombiek_paind13;
        else self.think = zombiek_paine13;
    }
};

//----------------------------------------------------------------------
// Puts the zombie on the floor with a random Yaw rotation
// IF frameno = MONAI_ZOMBIEFLR Setup on floor with use trigger
// ELSE setup next think timer and wait for opportunity to get back up
//----------------------------------------------------------------------
void(float frameno) zombiek_onground = {    
    self.health = self.max_health;    // reset health
    self.inpain = 2;                // Lying on floor, no pain
    self.count = 0;                    // Reset get up tries
    self.solid = SOLID_NOT;            // No world interaction
    self.bodyonflr = MON_ONFLR;        // mark body for the chop

    // Does the zombie start on the ground?
    if (frameno == MONAI_ZOMBIEFLR) {
        // Do nothing else while on the floor
        self.nextthink = self.pausetime;
        self.think = SUB_Null;

        // Pick random starting pose
        if (random() < 0.5) {
            self.zombieflr = MONAI_ZOMBIEUPD;
            self.frame = $paind12;
        }
        else {
            self.zombieflr = MONAI_ZOMBIEUPE;
            self.frame = $paine12;
        }
        // If no angle set, Random fall direction
        if (self.angles_y < 1) self.angles_y = rint(random()*359);
        self.use = self.th_wakeup;
        self.fixangle = TRUE;
    }
    else {
        // Zombie fall down function defines wake up pose
        self.zombieflr = frameno;
        // Keep zombies on the ground and check again later
        self.nextthink = self.nextthink + MONAI_ZOMBGRDTIMER;
        self.think = self.th_wakeup;
    }
};

//----------------------------------------------------------------------
// Puts the zombie on the floor based on the angle of attack
// 270-360, 360-90 : Fall forward (3 checks Left, middle, right)
// 180-270, 90-180 : Fall backward
//----------------------------------------------------------------------
void() zombiek_knockdown =
{
    local float enemy_ang, comp_ang;
    
    // Work out which direction the damage came from (enemy)
    enemy_ang = targangle(self, self.enemy, 0, FALSE);
    comp_ang = anglemod(enemy_ang + 45);    // Shift clockwise (front cone)
    
    if (self.debuglvl) {
        dprint("Self ("); dprint(ftos(self.angles_y));
        dprint(") Targ ("); dprint(ftos(enemy_ang));
        dprint(") Comp ("); dprint(ftos(comp_ang));
        dprint(")\n");
    }
    
    // NORTH WEST : 270-315 (fall BACKWARD, reverse force)
    if (comp_ang > 315) {
        if (self.debuglvl) dprint("\b[KNOCKDOWN]\b HIT from the UPPER LEFT\n");
        self.ideal_yaw = anglemod(enemy_ang + 180);
        ChangeYaw ();
        zombiek_paine1();
    }
    // SOUTH WEST : 225-270 (fall FORWARD, side force +45)
    else if (comp_ang > 270) {
        if (self.debuglvl) dprint("\b[KNOCKDOWN]\b HIT from the LOWER LEFT\n");
        self.ideal_yaw = comp_ang;
        ChangeYaw ();
        zombiek_paind1();
    }
    // SOUTH : 135-225 (fall FORWARD)
    else if (comp_ang > 180) {
        if (self.debuglvl) dprint("\b[KNOCKDOWN]\b HIT from the BACK\n");
        self.ideal_yaw = self.angles_y;    // Fix for ai_painforward
        ChangeYaw ();
        zombiek_paind1();
    }
    // SOUTH EAST : 90-135 (fall FORWARD, side force -45)
    else if (comp_ang > 135) {
        if (self.debuglvl) dprint("\b[KNOCKDOWN]\b HIT from the LOWER RIGHT\n");
        self.ideal_yaw = anglemod(enemy_ang+315);
        ChangeYaw ();    
        zombiek_paind1();
    }
    // NORTH EAST : 45-90 (fall BACKWARD, reverse force)
    else if (comp_ang > 90) {
        if (self.debuglvl) dprint("\b[KNOCKDOWN]\b HIT from the UPPER RIGHT\n");
        self.ideal_yaw = anglemod(enemy_ang + 180);
        ChangeYaw ();
        zombiek_paine1();
    }
    // NORTH : 315-45    (fall BACKWARD)
    else {
        if (self.debuglvl) dprint("\b[KNOCKDOWN]\b HIT from the FRONT\n");
        zombiek_paine1();
    }
};

/*============================================================================
 All sounds used
 2 = strutting the stuff (walking)
 3 = running (in slow motion)
 4 = all pain routines (zombiek_paina1, _painb1, _painc1, _paind1)
 5 = its a knockout!
 6 = get up and moan about it
 7 = sword swipe
============================================================================*/
void(float type) zombiek_sound =
{
    // check for gibbed or idle sound block
    if (self.health < 1) return;
    if (self.spawnflags & MON_SPAWN_NOIDLE && type < 3) return;
    
    // Random sound check
     self.lip = random();
    if (type == 2 && self.idletimer < time) {
         self.idletimer = time + 1 + (random()*2);
        if (self.lip < 0.2)
            sound (self, CHAN_VOICE, "zombie/z_idle.wav", 1, ATTN_IDLE);
    }
    else if (type == 3 && self.idletimer < time) {
         self.idletimer = time + 1 + (random()*2);
        if (self.lip < 0.2) sound (self, CHAN_VOICE, "zombie/z_idle.wav", 1, ATTN_IDLE);
        if (self.lip > 0.8) sound (self, CHAN_VOICE, "zombie/z_idle1.wav", 1, ATTN_IDLE);
    }
    else if (type == 4) {
        if (self.lip < 0.25) sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        else if (self.lip < 0.5) sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
    }
    else if (type == 5) {
        sound (self, CHAN_BODY, "zombie/z_fall.wav", 1, ATTN_NORM);
    }
    else if (type == 6) {
        if (self.lip < 0.5) sound (self, CHAN_VOICE, "zombie/z_idle.wav", 1, ATTN_IDLE);
        else sound (self, CHAN_VOICE, "zombie/z_idle1.wav", 1, ATTN_IDLE);
    }
    else if (type == 7) {
        if (self.lip < 0.25) sound (self, CHAN_WEAPON, "weapons/sword1a.wav", 1, ATTN_NORM);
        else if (self.lip < 0.5) sound (self, CHAN_WEAPON, "weapons/sword2a.wav", 1, ATTN_NORM);
        else if (self.lip < 0.75) sound (self, CHAN_WEAPON, "weapons/sword1b.wav", 1, ATTN_NORM);
        else sound (self, CHAN_WEAPON, "weapons/sword2b.wav", 1, ATTN_NORM);
    }
};

//============================================================================
void() zombiek_die =
{
    self.health = self.gibhealth; // Force gib
    monster_death_precheck();
};

/*======================================================================
 QUAKED monster_zombiek (1 0 0) (-16 -16 -24) (16 16 32) Ambush x FLOOR
======================================================================*/
void() monster_zombiek =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_zombiek.mdl";        // Zombie Knight
    self.headmdl = "progs/ad171/h_zombiek.mdl";
    precache_model (self.mdl);
    precache_model (self.headmdl);

    self.gib1mdl = "progs/ad171/w_knightsword.mdl";    // Unique sword
    self.gib1sound = GIB_IMPACT_METALA;
    precache_model (self.gib1mdl);

    precache_sound ("zombie/z_idle.wav");    // Idle A
    precache_sound ("zombie/z_idle1.wav");    // Idle B

    self.pain_sound = "zombie/z_pain.wav";
    self.pain_sound2 = "zombie/z_pain1.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.pain_sound2);

    precache_sound ("zombie/z_fall.wav");    // Impact - floor
    precache_sound ("zombie/z_gib.wav");    // Death sound

    // New sword sweep/slash sounds
    precache_sound ("weapons/sword1a.wav");
    precache_sound ("weapons/sword1b.wav");
    precache_sound ("weapons/sword2a.wav");
    precache_sound ("weapons/sword2b.wav");

    // New sight sound, mixture of knight + zombie
    self.sight_sound = "zombie/zk_sight.wav";
    precache_sound (self.sight_sound);

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_SHORT;
    self.health = 60;                    // Basic health (cannot override)
    self.gibhealth = -60;                // Gib value, not used
    self.gibbed = FALSE;                // In one piece
    self.pain_flinch = MONAI_ZOMBIEHGH_DAM;
    self.steptype = FS_TYPESLOW;        // Small leg
    self.altsteptype = FS_TYPEDRAG;        // Clubfoot
    self.pain_timeout = -5;                // Always go into pain (health reset)
    self.inpain = 0;                    // Zombie starts with no pain lock
    self.count = 0;                        // Reset get up timer

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    if (self.deathstring == "") self.deathstring = " was cornered by a Zombie Knight\n";

    self.hittype    = HIT_METAL;
    self.th_checkattack = ZombiekCheckAttack;
    self.th_stand = zombiek_stand1;
    self.th_walk = zombiek_walk1;
    self.th_run = zombiek_run1;
    self.th_melee = zombiek_melee;        // two melee attacks (stationary)
    self.th_jump = zombiek_jump1;        // Leap attack
    self.th_jumpexit = zombiek_jump9;    // Exit animation for jump
    self.th_pain = zombiek_pain;
    self.th_die = zombiek_die;
    self.th_wakeup = zombiek_wakeup;    // Get off floor
    
    self.classtype = CT_MONZOMBIEK;
    self.classgroup = CG_ZOMBIE;        // One big happy zombie family!
    self.classmove = MON_MOVEWALK;

    monster_start();
};
