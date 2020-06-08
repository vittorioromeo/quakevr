/*==============================================================================
DROLE (Heavily inspired by Drole from Quoth - Kell/Necros/Preach)
==============================================================================*/
// Stand idle - Tentacle eye looks left
$frame standA1 standA2 standA3 standA4 standA5 standA6 standA7 standA8
$frame standA9 standA10 standA11 standA12 standA13 standA14 standA15 standA16
$frame standA17 standA18 standA19 standA20 standA21 standA22 standA23 standA24
$frame standA25 standA26 standA27 standA28 standA29 standA30

// Stand idle - Tentacle eye looks around
$frame standB1 standB2 standB3 standB4 standB5 standB6 standB7 standB8
$frame standB9 standB10 standB11 standB12 standB13 standB14 standB15 standB16
$frame standB17 standB18 standB19 standB20 standB21 standB22 standB23 standB24
$frame standB25 standB26 standB27 standB28 standB29 standB30 standB31 standB32
$frame standB33 standB34 standB35 standB36 standB37 standB38 standB39 standB40

// Stand idle - Swings arms back and forth
$frame standC1 standC2 standC3 standC4 standC5 standC6 standC7 standC8
$frame standC9 standC10 standC11 standC12 standC13 standC14 standC15 standC16
$frame standC17 standC18 standC19 standC20 standC21 standC22 standC23 standC24
$frame standC25 standC26 standC27 standC28 standC29 standC30

// Fireball attack
$frame magic1 magic2 magic3 magic4 magic5 magic6 magic7 magic8 magic9

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8

// Jump up 1-5, 6=flying
$frame jump1 jump2 jump3 jump4 jump5 jump6

$frame run1 run2 run3 run4 run5 run6 run7 run8

$frame pain1 pain2 pain3 pain4 pain5 pain6

$frame clawR1 clawR2 clawR3 clawR4 clawR5 clawR6 clawR7 clawR8
$frame clawR9 clawR10 clawR11 clawR12 clawR13 clawR14 clawR15 clawR16

$frame clawL1 clawL2 clawL3 clawL4 clawL5 clawL6 clawL7 clawL8
$frame clawL9 clawL10 clawL11 clawL12 clawL13 clawL14 clawL15 clawL16

$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12 death13 death14 death15 death16
$frame death17 death18 death19 death20 death21 death22 death23 death24
$frame death25 death26

$frame base1 base2 base3

//============================================================================
void()    drole_standA1    =[    $standA1,    drole_standA2    ] {monster_idle_sound(); ai_stand();};
void()    drole_standA2    =[    $standA2,    drole_standA3    ] {ai_stand();};
void()    drole_standA3    =[    $standA3,    drole_standA4    ] {ai_stand();};
void()    drole_standA4    =[    $standA4,    drole_standA5    ] {ai_stand();};
void()    drole_standA5    =[    $standA5,    drole_standA6    ] {ai_stand();};
void()    drole_standA6    =[    $standA6,    drole_standA7    ] {ai_stand();};
void()    drole_standA7    =[    $standA7,    drole_standA8    ] {ai_stand();};
void()    drole_standA8    =[    $standA8,    drole_standA9    ] {ai_stand();};
void()    drole_standA9    =[    $standA9,    drole_standA10    ] {ai_stand();};
void()    drole_standA10    =[    $standA10,    drole_standA11    ] {ai_stand();};
void()    drole_standA11    =[    $standA11,    drole_standA12    ] {ai_stand();};
void()    drole_standA12    =[    $standA12,    drole_standA13    ] {ai_stand();};
void()    drole_standA13    =[    $standA13,    drole_standA14    ] {ai_stand();};
void()    drole_standA14    =[    $standA14,    drole_standA15    ] {ai_stand();};
void()    drole_standA15    =[    $standA15,    drole_standA16    ] {ai_stand();};
void()    drole_standA16    =[    $standA16,    drole_standA17    ] {ai_stand();};
void()    drole_standA17    =[    $standA17,    drole_standA18    ] {ai_stand();};
void()    drole_standA18    =[    $standA18,    drole_standA19    ] {ai_stand();};
void()    drole_standA19    =[    $standA19,    drole_standA20    ] {ai_stand();};
void()    drole_standA20    =[    $standA20,    drole_standA21    ] {ai_stand();};
void()    drole_standA21    =[    $standA21,    drole_standA22    ] {ai_stand();};
void()    drole_standA22    =[    $standA22,    drole_standA23    ] {ai_stand();};
void()    drole_standA23    =[    $standA23,    drole_standA24    ] {ai_stand();};
void()    drole_standA24    =[    $standA24,    drole_standA25    ] {ai_stand();};
void()    drole_standA25    =[    $standA25,    drole_standA26    ] {ai_stand();};
void()    drole_standA26    =[    $standA26,    drole_standA27    ] {ai_stand();};
void()    drole_standA27    =[    $standA27,    drole_standA28    ] {ai_stand();};
void()    drole_standA28    =[    $standA28,    drole_standA29    ] {ai_stand();};
void()    drole_standA29    =[    $standA29,    drole_standA30    ] {ai_stand();};
void()    drole_standA30    =[    $standA30,    drole_standC1    ] {ai_stand();};

//============================================================================
void()    drole_standB1    =[    $standB1,    drole_standB2    ] {monster_idle_sound(); ai_stand();};
void()    drole_standB2    =[    $standB2,    drole_standB3    ] {ai_stand();};
void()    drole_standB3    =[    $standB3,    drole_standB4    ] {ai_stand();};
void()    drole_standB4    =[    $standB4,    drole_standB5    ] {ai_stand();};
void()    drole_standB5    =[    $standB5,    drole_standB6    ] {ai_stand();};
void()    drole_standB6    =[    $standB6,    drole_standB7    ] {ai_stand();};
void()    drole_standB7    =[    $standB7,    drole_standB8    ] {ai_stand();};
void()    drole_standB8    =[    $standB8,    drole_standB9    ] {ai_stand();};
void()    drole_standB9    =[    $standB9,    drole_standB10    ] {ai_stand();};
void()    drole_standB10    =[    $standB10,    drole_standB11    ] {ai_stand();};
void()    drole_standB11    =[    $standB11,    drole_standB12    ] {ai_stand();};
void()    drole_standB12    =[    $standB12,    drole_standB13    ] {ai_stand();};
void()    drole_standB13    =[    $standB13,    drole_standB14    ] {ai_stand();};
void()    drole_standB14    =[    $standB14,    drole_standB15    ] {ai_stand();};
void()    drole_standB15    =[    $standB15,    drole_standB16    ] {ai_stand();};
void()    drole_standB16    =[    $standB16,    drole_standB17    ] {ai_stand();};
void()    drole_standB17    =[    $standB17,    drole_standB18    ] {ai_stand();};
void()    drole_standB18    =[    $standB18,    drole_standB19    ] {ai_stand();};
void()    drole_standB19    =[    $standB19,    drole_standB20    ] {ai_stand();};
void()    drole_standB20    =[    $standB20,    drole_standB21    ] {ai_stand();};
void()    drole_standB21    =[    $standB21,    drole_standB22    ] {ai_stand();};
void()    drole_standB22    =[    $standB22,    drole_standB23    ] {ai_stand();};
void()    drole_standB23    =[    $standB23,    drole_standB24    ] {ai_stand();};
void()    drole_standB24    =[    $standB24,    drole_standB25    ] {ai_stand();};
void()    drole_standB25    =[    $standB25,    drole_standB26    ] {ai_stand();};
void()    drole_standB26    =[    $standB26,    drole_standB27    ] {ai_stand();};
void()    drole_standB27    =[    $standB27,    drole_standB28    ] {ai_stand();};
void()    drole_standB28    =[    $standB28,    drole_standB29    ] {ai_stand();};
void()    drole_standB29    =[    $standB29,    drole_standB30    ] {ai_stand();};
void()    drole_standB30    =[    $standB30,    drole_standB31    ] {ai_stand();};
void()    drole_standB31    =[    $standB31,    drole_standB32    ] {ai_stand();};
void()    drole_standB32    =[    $standB32,    drole_standB33    ] {ai_stand();};
void()    drole_standB33    =[    $standB33,    drole_standB34    ] {ai_stand();};
void()    drole_standB34    =[    $standB34,    drole_standB35    ] {ai_stand();};
void()    drole_standB35    =[    $standB35,    drole_standB36    ] {ai_stand();};
void()    drole_standB36    =[    $standB36,    drole_standB37    ] {ai_stand();};
void()    drole_standB37    =[    $standB37,    drole_standB38    ] {ai_stand();};
void()    drole_standB38    =[    $standB38,    drole_standB39    ] {ai_stand();};
void()    drole_standB39    =[    $standB39,    drole_standB40    ] {ai_stand();};
void()    drole_standB40    =[    $standB40,    drole_standC1    ] {ai_stand();};

//============================================================================
void()    drole_standC1    =[    $standC1,    drole_standC2    ] {monster_idle_sound(); ai_stand();};
void()    drole_standC2    =[    $standC2,    drole_standC3    ] {ai_stand();};
void()    drole_standC3    =[    $standC3,    drole_standC4    ] {ai_stand();};
void()    drole_standC4    =[    $standC4,    drole_standC5    ] {ai_stand();};
void()    drole_standC5    =[    $standC5,    drole_standC6    ] {ai_stand();};
void()    drole_standC6    =[    $standC6,    drole_standC7    ] {ai_stand();};
void()    drole_standC7    =[    $standC7,    drole_standC8    ] {ai_stand();};
void()    drole_standC8    =[    $standC8,    drole_standC9    ] {ai_stand();};
void()    drole_standC9    =[    $standC9,    drole_standC10    ] {ai_stand();};
void()    drole_standC10    =[    $standC10,    drole_standC11    ] {ai_stand();};
void()    drole_standC11    =[    $standC11,    drole_standC12    ] {ai_stand();};
void()    drole_standC12    =[    $standC12,    drole_standC13    ] {ai_stand();};
void()    drole_standC13    =[    $standC13,    drole_standC14    ] {ai_stand();};
void()    drole_standC14    =[    $standC14,    drole_standC15    ] {ai_stand();};
void()    drole_standC15    =[    $standC15,    drole_standC16    ] {ai_stand();};
void()    drole_standC16    =[    $standC16,    drole_standC17    ] {ai_stand();};
void()    drole_standC17    =[    $standC17,    drole_standC18    ] {ai_stand();};
void()    drole_standC18    =[    $standC18,    drole_standC19    ] {ai_stand();};
void()    drole_standC19    =[    $standC19,    drole_standC20    ] {ai_stand();};
void()    drole_standC20    =[    $standC20,    drole_standC21    ] {ai_stand();};
void()    drole_standC21    =[    $standC21,    drole_standC22    ] {ai_stand();};
void()    drole_standC22    =[    $standC22,    drole_standC23    ] {ai_stand();};
void()    drole_standC23    =[    $standC23,    drole_standC24    ] {ai_stand();};
void()    drole_standC24    =[    $standC24,    drole_standC25    ] {ai_stand();};
void()    drole_standC25    =[    $standC25,    drole_standC26    ] {ai_stand();};
void()    drole_standC26    =[    $standC26,    drole_standC27    ] {ai_stand();};
void()    drole_standC27    =[    $standC27,    drole_standC28    ] {ai_stand();};
void()    drole_standC28    =[    $standC28,    drole_standC29    ] {ai_stand();};
void()    drole_standC29    =[    $standC29,    drole_standC30    ] {ai_stand();};
void()    drole_standC30    =[    $standC30,    drole_standC1    ] {
    self.idlebusy = FALSE; 
    if (random() < MON_IDLE_ANIMATION) {
        self.idlebusy = TRUE;
        self.lefty = random();
        if (self.lefty < 0.3) self.think = drole_standA1;
        else if (self.lefty < 0.6) self.think = drole_standB1;
        else self.think = drole_standC1;
    }
    ai_stand();
};

//============================================================================
void()    drole_walk1 =[    $walk1, drole_walk2 ] {monster_footstep(FALSE); 
monster_idle_sound(); ai_walk(10);};
void()    drole_walk2 =[    $walk2, drole_walk3 ] {ai_walk(9);};
void()    drole_walk3 =[    $walk3, drole_walk4 ] {ai_walk(9);};
void()    drole_walk4 =[    $walk4, drole_walk5 ] {ai_walk(5);};
void()    drole_walk5 =[    $walk5, drole_walk6 ] {monster_footstep(FALSE); ai_walk(6);};
void()    drole_walk6 =[    $walk6, drole_walk7 ] {ai_walk(12);};
void()    drole_walk7 =[    $walk7, drole_walk8 ] {ai_walk(8);};
void()    drole_walk8 =[    $walk8, drole_walk1 ] {ai_walk(3);};

//============================================================================
// Drole's have two run speeds, regular and rage
// * each run type uses different animation sets
// * once the drole is enraged there is no going back
// * moved the health trigger to a percentage (custom HP)
//============================================================================
void() drole_run;

void() drole_rage1 = [ $run1, drole_rage2 ] {monster_idle_sound();ai_run(27);};
void() drole_rage2 = [ $run2, drole_rage3 ] {ai_run(26);};
void() drole_rage3 = [ $run3, drole_rage4 ] {ai_run(30);};
void() drole_rage4 = [ $run4, drole_rage5 ] {monster_footstep(FALSE); ai_run(28);};
void() drole_rage5 = [ $run5, drole_rage6 ] {ai_run(27);};
void() drole_rage6 = [ $run6, drole_rage7 ] {ai_run(26);};
void() drole_rage7 = [ $run7, drole_rage8 ] {ai_run(30);};
void() drole_rage8 = [ $run8, drole_run ] {monster_footstep(FALSE); ai_run(28);};

//----------------------------------------------------------------------
void() drole_run1 = [ $walk1, drole_run2 ] {monster_footstep(FALSE); ai_run(15);};
void() drole_run2 = [ $walk2, drole_run3 ] {monster_idle_sound();ai_run(14);};
void() drole_run3 = [ $walk3, drole_run4 ] {ai_run(14);};
void() drole_run4 = [ $walk4, drole_run5 ] {ai_run(10);};
void() drole_run5 = [ $walk5, drole_run6 ] {monster_footstep(FALSE); ai_run(11);};
void() drole_run6 = [ $walk6, drole_run7 ] {ai_run(17);};
void() drole_run7 = [ $walk7, drole_run8 ] {ai_run(13);};
void() drole_run8 = [ $walk8, drole_run ] {ai_run(8);};

//----------------------------------------------------------------------
void() drole_run =
{
    if (self.attack_rage) drole_rage1();
    else drole_run1();
};

//===========================================================================
// MELEE ATTACK - Double claws
//===========================================================================
void(float side) drole_claw =
{
    local float ldmg;
    
    if (!self.enemy) return;
    if (self.health < 1) return;
    ai_face ();                        // Turn towards enemy target
    ai_damagebreakable(50);            // Damage any breakables

    if (!ai_checkmelee(MONAI_MELEEDROLE1) || !self.enemy.takedamage) {
        // Melee claw miss sound
        sound (self, CHAN_WEAPON, "drole/swipe.wav", 1, ATTN_NORM);
    }
    else {
        // Melee claw hit sound
        if (random() < 0.5) sound (self, CHAN_WEAPON, "drole/strike1.wav", 1, ATTN_NORM);
        else sound (self, CHAN_WEAPON, "drole/strike2.wav", 1, ATTN_NORM);
        ldmg = 12 + 6*random();
        T_Damage (self.enemy, self, self, ldmg, DAMARMOR);        
        SpawnMeatSpray (self, self.enemy, side);
    }
};

//----------------------------------------------------------------------
void(float ragespd) drole_charge =
{
    if (self.attack_rage) ai_charge(ragespd);
    else ai_charge(rint(ragespd*0.7));
}

//----------------------------------------------------------------------
void() drole_clawL1;
//----------------------------------------------------------------------
void() drole_clawR1 = [ $clawR1, drole_clawR2 ] {drole_charge(27);};
void() drole_clawR2 = [ $clawR2, drole_clawR3 ] {drole_charge(26);
sound (self, CHAN_WEAPON, "drole/swipe.wav", 1, ATTN_NORM);};
void() drole_clawR3 = [ $clawR3, drole_clawR4 ] {monster_footstep(FALSE);drole_charge(30);};
void() drole_clawR4 = [ $clawR4, drole_clawR5 ] {drole_charge(28);};
void() drole_clawR5 = [ $clawR5, drole_clawR6 ] {drole_charge(27);
drole_claw(-250);};
void() drole_clawR6 = [ $clawR6, drole_clawR7 ] {drole_charge(26);};
void() drole_clawR7 = [ $clawR7, drole_clawR8 ] {monster_footstep(FALSE);drole_charge(30);};
void() drole_clawR8 = [ $clawR8, drole_clawR9 ] {drole_charge(28);};
void() drole_clawR9 = [ $clawR9, drole_clawR10 ] {drole_charge(27);};
void() drole_clawR10 = [ $clawR10, drole_clawR11 ] {drole_charge(26);};
void() drole_clawR11 = [ $clawR11, drole_clawR12 ] {monster_footstep(FALSE);drole_charge(30);
drole_claw(-250);};
void() drole_clawR12 = [ $clawR12, drole_clawR13 ] {drole_charge(28);};
void() drole_clawR13 = [ $clawR13, drole_clawR14 ] {drole_charge(27);};
void() drole_clawR14 = [ $clawR14, drole_clawR15 ] {drole_charge(26);
sound (self, CHAN_WEAPON, "drole/swipe.wav", 1, ATTN_NORM);};
void() drole_clawR15 = [ $clawR15, drole_clawR16 ] {monster_footstep(FALSE);drole_charge(30);};
void() drole_clawR16 = [ $clawR16, drole_run ] {drole_charge(28);
    if (random() < 0.5 && ai_checkmelee(MONAI_MELEEDROLE2) && 
        self.enemy.health > 0 && visible(self.enemy)) drole_clawL1();
};

//----------------------------------------------------------------------
void() drole_clawL1 = [ $clawL1, drole_clawL2 ] {drole_charge(27);};
void() drole_clawL2 = [ $clawL2, drole_clawL3 ] {drole_charge(26);
sound (self, CHAN_WEAPON, "drole/swipe.wav", 1, ATTN_NORM);};
void() drole_clawL3 = [ $clawL3, drole_clawL4 ] {monster_footstep(FALSE);drole_charge(30);};
void() drole_clawL4 = [ $clawL4, drole_clawL5 ] {drole_charge(28);};
void() drole_clawL5 = [ $clawL5, drole_clawL6 ] {drole_charge(27);
drole_claw(250);};
void() drole_clawL6 = [ $clawL6, drole_clawL7 ] {drole_charge(26);};
void() drole_clawL7 = [ $clawL7, drole_clawL8 ] {monster_footstep(FALSE);drole_charge(30);};
void() drole_clawL8 = [ $clawL8, drole_clawL9 ] {drole_charge(28);};
void() drole_clawL9 = [ $clawL9, drole_clawL10 ] {drole_charge(27);};
void() drole_clawL10 = [ $clawL10, drole_clawL11 ] {drole_charge(26);};
void() drole_clawL11 = [ $clawL11, drole_clawL12 ] {monster_footstep(FALSE);drole_charge(30);
drole_claw(250);};
void() drole_clawL12 = [ $clawL12, drole_clawL13 ] {drole_charge(28);};
void() drole_clawL13 = [ $clawL13, drole_clawL14 ] {drole_charge(27);};
void() drole_clawL14 = [ $clawL14, drole_clawL15 ] {drole_charge(26);
sound (self, CHAN_WEAPON, "drole/swipe.wav", 1, ATTN_NORM);};
void() drole_clawL15 = [ $clawL15, drole_clawL16 ] {monster_footstep(FALSE);drole_charge(30);};
void() drole_clawL16 = [ $clawL16, drole_run ] {drole_charge(28);
    if (random() < 0.5 && ai_checkmelee(MONAI_MELEEDROLE2) && 
        self.enemy.health > 0 && visible(self.enemy)) drole_clawR1();
};

//----------------------------------------------------------------------
void() drole_melee =
{
    if (random() < 0.5) drole_clawR1();
    else drole_clawL1();
};

//============================================================================
// RANGE ATTACK - Fireball (rocket)
// * The trick to drole rockets is that they start really slow
// * Over time (4 frames) the rocket speeds up (linked to animation)
// * requires special projectile and new th_updmissile function
// * Has a different explosion sound (does not play rocket default)
//
//============================================================================
void() drole_rocket1 = [ 1, drole_rocket2 ] {self.nextthink = time + 0.2;};
void() drole_rocket2 = [ 2, drole_rocket3 ] {self.velocity = self.velocity * 1.5;};
void() drole_rocket3 = [ 3, drole_rocket4 ] {self.velocity = self.velocity * 1.5;};
void() drole_rocket4 = [ 3, SUB_Remove ] {
    self.nextthink = time + 5;
    if (skill == SKILL_EASY) self.velocity = self.velocity * 3.5;
    else self.velocity = self.velocity * 4.5;
};

//----------------------------------------------------------------------
void() drole_fire =
{
    local vector org, dir, avel;
    if (!self.enemy) return;
    if (self.health < 1) return;
    
    ai_face();
    self.attack_speed = 80;
    
    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);
    dir = normalize((self.enemy.origin+'0 0 -8') - org);
    avel = avel = vecrand(100,200,FALSE);
    
    Launch_Missile (org, dir, avel, CT_PROJ_DROLE, self.attack_speed);
};

//----------------------------------------------------------------------
void() drole_magic1 = [ $magic1, drole_magic2 ] {ai_face();
sound(self, CHAN_WEAPON, "drole/fire.wav", 1, ATTN_NORM);};
void() drole_magic2 = [ $magic2, drole_magic3 ] {ai_face();};
void() drole_magic3 = [ $magic3, drole_magic4 ] {ai_face();};
void() drole_magic4 = [ $magic4, drole_magic5 ] {drole_fire();};
void() drole_magic5 = [ $magic5, drole_magic6 ] {ai_face();};
void() drole_magic6 = [ $magic6, drole_magic7 ] {ai_face();};
void() drole_magic7 = [ $magic7, drole_magic8 ] {ai_face();};
void() drole_magic8 = [ $magic8, drole_magic9 ] {ai_face();};
void() drole_magic9 = [ $magic9, drole_run ] {ai_face();};

//============================================================================
void() drole_pain1 = [ $pain1, drole_pain2 ] {ai_back(8);};
void() drole_pain2 = [ $pain2, drole_pain3 ] {ai_back(10);};
void() drole_pain3 = [ $pain3, drole_pain4 ] {ai_back(10);};
void() drole_pain4 = [ $pain4, drole_pain5 ] {ai_back(8);};
void() drole_pain5 = [ $pain5, drole_pain6 ] {ai_back(6);};
void() drole_pain6 = [ $pain6, drole_run ] {ai_back(4);};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) drole_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        if (self.attack_rage) sound (self, CHAN_VOICE, "drole/enrage.wav", 0.5, ATTN_NORM);
        else sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);

        if (self.pain_check == 1) drole_pain1 ();
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 0.6;
            self.axhitme = 0;
            drole_pain1 ();
        }
    }
};

//============================================================================
void() drole_death1 = [ $death1, drole_death2 ] {};
void() drole_death2 = [ $death2, drole_death3 ] {monster_check_gib();};
void() drole_death3 = [ $death3, drole_death4 ] {monster_check_gib();
self.solid = SOLID_NOT;};
void() drole_death4 = [ $death4, drole_death5 ] {};
void() drole_death5 = [ $death5, drole_death6 ] {};
void() drole_death6 = [ $death6, drole_death7 ] {};
void() drole_death7 = [ $death7, drole_death8 ] {};
void() drole_death8 = [ $death8, drole_death9 ] {};
void() drole_death9 = [ $death9, drole_death10 ] {};
void() drole_death10 = [ $death10, drole_death11 ] {};
void() drole_death11 = [ $death11, drole_death12 ] {};
void() drole_death12 = [ $death12, drole_death13 ] {};
void() drole_death13 = [ $death13, drole_death14 ] {};
void() drole_death14 = [ $death14, drole_death15 ] {};
void() drole_death15 = [ $death15, drole_death16 ] {};
void() drole_death16 = [ $death16, drole_death17 ] {};
void() drole_death17 = [ $death17, drole_death18 ] {};
void() drole_death18 = [ $death18, drole_death19 ] {};
void() drole_death19 = [ $death19, drole_death20 ] {};
void() drole_death20 = [ $death20, drole_death21 ] {};
void() drole_death21 = [ $death21, drole_death22 ] {};
void() drole_death22 = [ $death22, drole_death23 ] {};
void() drole_death23 = [ $death23, drole_death24 ] {};
void() drole_death24 = [ $death24, drole_death25 ] {};
void() drole_death25 = [ $death25, drole_death26 ] {monster_death_postcheck();};
void() drole_death26 = [ $death26, drole_death26 ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() drole_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "drole/death.wav", 1, ATTN_NORM);
        drole_death1 ();
    }
};

/*======================================================================
QUAKED monster_drole (1 0 0) (-32 -32 -24) (32 32 64) Ambush
======================================================================*/
void() monster_drole =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_drole.mdl";
    self.headmdl = "progs/ad171/h_drole.mdl";
    self.gib1mdl = "progs/ad171/gib_drolearm1.mdl";    // Top arm (pose 1)
    self.gib2mdl = "progs/ad171/gib_drolearm2.mdl";    // Top arm (pose 2)
    self.gib3mdl = "progs/ad171/gib_droleleg1.mdl";    // stretched out leg

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    precache_model (MODEL_PROJ_DROLE);        // growing fireball
    
    self.idle_sound = "drole/idle.wav";
    precache_sound (self.idle_sound);

    precache_sound ("drole/enrage.wav");    // sight2.wav
    precache_sound ("drole/death.wav");
    self.pain_sound = "drole/pain.wav";
    precache_sound (self.pain_sound);
    
    precache_sound ("drole/fire.wav");        // Launch rockets
    precache_sound ("drole/r_explode.wav");    // unique explosion sound
    precache_sound ("drole/strike1.wav");    // melee impacts
    precache_sound ("drole/strike2.wav");
    precache_sound ("drole/swipe.wav");        // Missed melee

    self.sight_sound = "drole/sight.wav";
    precache_sound (self.sight_sound);

    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
        precache_model (MODEL_PROJ_DROLEGRN);    // growing poisonball
    }

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_GIANT;
    if (self.health < 1) self.health = 500;
    self.gibhealth = -60;                // Hard to gib
    self.gibbed = FALSE;
    self.pain_flinch = 200;                // takes alot to pain
    self.pain_longanim = TRUE;            // can be chopped with shadow axe
    self.pain_timeout = 2;                // High pain threshold
    self.steptype = FS_TYPEHEAVY;        // heavy feet
    self.blockudeath = TRUE;            // no humanoid death sound
    self.attack_offset = '0 0 34';        // Middle of chest
    if (self.deathstring == "") self.deathstring = " was decimated by a Drole\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    self.th_checkattack = DroleCheckAttack;
    self.th_stand = drole_standC1;
    self.th_walk = drole_walk1;
    self.th_run = drole_run;
    self.th_melee = drole_melee;        // Claw attack
    self.th_missile = drole_magic1;        // Fireball incoming!
    self.th_updmissile = drole_rocket1;    // Update flying rocket
    self.th_pain = drole_pain;
    self.th_die = drole_die;

    self.classtype = CT_MONDROLE;
    self.classgroup = CG_DEMON;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};
