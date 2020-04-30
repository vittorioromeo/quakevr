/*==============================================================================
HUNTER OGRE - Mace variant
==============================================================================*/
$cd id1/models/hogremac_c
$origin 0 0 24
$base base        
$skin base

// (000) Default stand
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8

// (008) Walking
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12

// (020) Walking (alternative)
$frame walkB1 walkB2 walkB3 walkB4 walkB5 walkB6 walkB7 walkB8
$frame walkB9 walkB10 walkB11 walkB12

// (032) Running (classic pose)
$frame run1 run2 run3 run4 run5 run6 run7 run8

// (040) Swing (sideways attack)
$frame swing1 swing2 swing3 swing4 swing5 swing6 swing7
$frame swing8 swing9 swing10 swing11 swing12 swing13 swing14

// (054) Smash (Overarm attack)
$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7
$frame smash8 smash9 smash10 smash11 smash12 smash13 smash14

// (082) Leap (swing arm over and leap forward)
//$frame leap1 leap2 leap3 leap4 leap5 leap6 leap7 leap8
//$frame leap9 leap10 leap11 leap12 leap13 leap14 leap15

// () Turret idle (Turret mode)
$frame turidle1 turidle2 turidle3 turidle4
$frame turidle5 turidle6 turidle7 turidle8

// () Shoot (GL attack - chainsaw down)
$frame shoot1 shoot2 shoot3 shoot4 shoot5 shoot6 shoot7 shoot8

// () Shootpain (turret mode)
$frame turpainA1 turpainA2 turpainA3 turpainA4 turpainA5

// () painA - Quick backwards flex
$frame painA1 painA2 painA3 painA4 painA5

// () painB - Stagger backwards
$frame painB1 painB2 painB3 painB4 painB5 painB6 painB7 painB8
$frame painB9 painB10 painB11 painB12 painB13

// () painC - Fall down
$frame painC1 painC2 painC3 painC4 painC5 painC6 painC7 painC8
$frame painC9 painC10 painC11 painC12 painC13 painC14 painC15 painC16 painC17

// () turretWake (turret mode)
$frame turwake1 turwake2 turwake3 turwake4

// () turretRelease (turret mode)
$frame turrel1 turrel2 turrel3 turrel4

// () deathA (Forward)
$frame deathA1 deathA2 deathA3 deathA4 deathA5 deathA6 deathA7 deathA8 
$frame deathA9 deathA10

// () deathB (Backward)
$frame deathB1 deathB2 deathB3 deathB4 deathB5 deathB6 deathB7 deathB8 
$frame deathB9 deathB10 deathB11 deathB12 deathB13 deathB14

void(float soundtype) hogremac_sound;

//======================================================================
void() hogremac_stand1    =[    $stand1,    hogremac_stand2    ] {monster_idle_sound(); ai_stand();};
void() hogremac_stand2    =[    $stand2,    hogremac_stand3    ] {ai_stand();};
void() hogremac_stand3    =[    $stand3,    hogremac_stand4    ] {ai_stand();};
void() hogremac_stand4    =[    $stand4,    hogremac_stand5    ] {ai_stand();};
void() hogremac_stand5    =[    $stand5,    hogremac_stand6    ] {ai_stand();};
void() hogremac_stand6    =[    $stand6,    hogremac_stand7    ] {ai_stand();};
void() hogremac_stand7    =[    $stand7,    hogremac_stand8    ] {ai_stand();};
void() hogremac_stand8    =[    $stand8,    hogremac_stand1    ] {ai_stand();};

//======================================================================
void() hogremac_walkB1    =[    $walkB1,    hogremac_walkB2    ] {ai_walk(4);};
void() hogremac_walkB2    =[    $walkB2,    hogremac_walkB3    ] {ai_walk(6);};
void() hogremac_walkB3    =[    $walkB3,    hogremac_walkB4    ] {monster_idle_sound(); ai_walk(6);};
void() hogremac_walkB4    =[    $walkB4,    hogremac_walkB5    ] {ai_walk(4);};
void() hogremac_walkB5    =[    $walkB5,    hogremac_walkB6    ] {monster_footstep(FALSE); ai_walk(0);};
void() hogremac_walkB6    =[    $walkB6,    hogremac_walkB7    ] {ai_walk(0);};
void() hogremac_walkB7    =[    $walkB7,    hogremac_walkB8    ] {ai_walk(6);};
void() hogremac_walkB8    =[    $walkB8,    hogremac_walkB9    ] {ai_walk(7);
    if (random() < 0.1) hogremac_sound(1);};
void() hogremac_walkB9    =[    $walkB9,    hogremac_walkB10] {ai_walk(6);};
void() hogremac_walkB10    =[    $walkB10,    hogremac_walkB11] {ai_walk(2);};
void() hogremac_walkB11    =[    $walkB11,    hogremac_walkB12] {monster_footstep(FALSE);ai_walk(0);};
void() hogremac_walkB12    =[    $walkB12,    hogremac_walk1    ] {ai_walk(0);};

//======================================================================
void() hogremac_walk1    =[    $walk1,        hogremac_walk2    ] {ai_walk(4);};
void() hogremac_walk2    =[    $walk2,        hogremac_walk3    ] {ai_walk(4);};
void() hogremac_walk3    =[    $walk3,        hogremac_walk4    ] {monster_idle_sound(); ai_walk(4);};
void() hogremac_walk4    =[    $walk4,        hogremac_walk5    ] {ai_walk(4);};
void() hogremac_walk5    =[    $walk5,        hogremac_walk6    ] {ai_walk(4);};
void() hogremac_walk6    =[    $walk6,        hogremac_walk7    ] {monster_footstep(FALSE); ai_walk(4);};
void() hogremac_walk7    =[    $walk7,        hogremac_walk8    ] {ai_walk(4);};
void() hogremac_walk8    =[    $walk8,        hogremac_walk9    ] {ai_walk(4);};
void() hogremac_walk9    =[    $walk9,        hogremac_walk10] {ai_walk(4);
    if (random() < 0.1) hogremac_sound(1);};
void() hogremac_walk10    =[    $walk10,    hogremac_walk11] {ai_walk(4);};
void() hogremac_walk11    =[    $walk11,    hogremac_walk12] {ai_walk(3);};
void() hogremac_walk12    =[    $walk12,    hogremac_walk1    ] {monster_footstep(FALSE);
    if (random() < 0.05) self.think = hogremac_walkB1;
    ai_walk(4);};

//======================================================================
void() hogremac_run1    =[    $run1,        hogremac_run2    ] {monster_idle_sound(); ai_run(8);};
void() hogremac_run2    =[    $run2,        hogremac_run3    ] {monster_footstep(FALSE); ai_run(13);};
void() hogremac_run3    =[    $run3,        hogremac_run4    ] {ai_run(13);};
void() hogremac_run4    =[    $run4,        hogremac_run5    ] {ai_run(13);};
void() hogremac_run5    =[    $run5,        hogremac_run6    ] {ai_run(8);};
void() hogremac_run6    =[    $run6,        hogremac_run7    ] {monster_footstep(FALSE); ai_run(16);};
void() hogremac_run7    =[    $run7,        hogremac_run8    ] {ai_run(18);};
void() hogremac_run8    =[    $run8,        hogremac_run1    ] {ai_run(13);};

/*======================================================================
 New Mace attack
======================================================================*/
void() hogremac_swing1    =[    $swing1,    hogremac_swing2    ] {ai_charge(10);};
void() hogremac_swing2    =[    $swing2,    hogremac_swing3    ] {ai_charge(10);hogremac_sound(4);};
void() hogremac_swing3    =[    $swing3,    hogremac_swing4    ] {ai_charge(6);};
void() hogremac_swing4    =[    $swing4,    hogremac_swing5    ] {ai_charge(6);};
void() hogremac_swing5    =[    $swing5,    hogremac_swing6    ] {ai_charge(7);};
void() hogremac_swing6    =[    $swing6,    hogremac_swing7    ] {ai_forward(7);
    self.meleecontact = TRUE;ai_meleesmash(20);self.meleecontact = FALSE;};
void() hogremac_swing7    =[    $swing7,    hogremac_swing8    ] {ai_forward(4);monster_footstep(FALSE);};
void() hogremac_swing8    =[    $swing8,    hogremac_swing9    ] {ai_forward(4);};
void() hogremac_swing9    =[    $swing9,    hogremac_swing10 ] {ai_forward(4);};
void() hogremac_swing10=[    $swing10,    hogremac_swing11 ] {ai_forward(4);};
void() hogremac_swing11=[    $swing11,    hogremac_swing12 ] {ai_forward(4);};
void() hogremac_swing12=[    $swing12,    hogremac_swing13 ] {ai_face();monster_footstep(FALSE);};
void() hogremac_swing13=[    $swing13,    hogremac_swing14 ] {ai_face();};
void() hogremac_swing14=[    $swing14,    hogremac_run1    ] {ai_face();};

//----------------------------------------------------------------------
void() hogremac_smash1    =[    $smash1,    hogremac_smash2    ] {ai_charge(9);};
void() hogremac_smash2    =[    $smash2,    hogremac_smash3    ] {ai_charge(9);hogreham_sound(1);};
void() hogremac_smash3    =[    $smash3,    hogremac_smash4    ] {ai_charge(9);};
void() hogremac_smash4    =[    $smash4,    hogremac_smash5    ] {ai_charge(12);};
void() hogremac_smash5    =[    $smash5,    hogremac_smash6    ] {ai_charge(12);};
void() hogremac_smash6    =[    $smash6,    hogremac_smash7    ] {ai_charge(5);};
void() hogremac_smash7    =[    $smash7,    hogremac_smash8    ] {ai_charge(5);};
void() hogremac_smash8    =[    $smash8,    hogremac_smash9    ] {ai_face();monster_footstep(FALSE);};
void() hogremac_smash9    =[    $smash9,    hogremac_smash10 ] {ai_face();};
void() hogremac_smash10=[    $smash10,    hogremac_smash11 ] {ai_face();
    self.meleecontact = TRUE;ai_meleesmash(30);self.meleecontact = FALSE;
    makevectors (self.angles);                // Put impact infront of ogre
    self.finalangle = self.origin + (v_forward * 56) + (-v_up * 22);
    particle_explode(self.finalangle, 24, 0.5, PARTICLE_BURST_FIRE, PARTICLE_BURST_SHOCKWAVE); 
    sound (self, CHAN_WEAPON, "ogre/ham_hit.wav", 1, ATTN_IDLE);
};
void() hogremac_smash11=[    $smash11,    hogremac_smash12 ] {};
void() hogremac_smash12=[    $smash12,    hogremac_smash13 ] {};
void() hogremac_smash13=[    $smash13,    hogremac_smash14 ] {ai_face();};
void() hogremac_smash14=[    $smash14,    hogremac_run1    ] {ai_face();};

//----------------------------------------------------------------------
void() hogremac_melee =
{
    // Mace ogres don't get interrupted with pain for melee
    self.pain_finished = time + 2;
    self.lip = random();
    if (self.lip < 0.5) hogremac_smash1();
    else hogremac_swing1();
};

/*======================================================================
 Jump forward (leaving ground) and do heavy damage
======================================================================
void() hogre_leap11;
void() hogre_JumpTouch =
{
    local float ldmg;
    
    if (self.health <= 0) return;
    ai_jumpbreakable(30);            // Damage any breakables

    self.touch = SUB_Null;            // No more touching
    self.think = hogre_leap11;        // Exit frame

    if ( CanDamage(other, self) ) {
        if ( vlen(self.velocity) > 300 ) {
          ldmg = 20 + 10*random();
            T_Damage (other, self, self, ldmg, DAMARMOR);    
            spawn_touchblood (self, self.enemy, ldmg*3);
        }
    }
    // Next timer
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void()    hogre_leap1    =[    $leap1,    hogre_leap2    ] {ai_charge(5);hogre_sound(4);
    self.jump_flag = time + MONAI_JUMPHOGRETIME;
    self.pain_finished = time + 1;};
void()    hogre_leap2    =[    $leap2,    hogre_leap3    ] {ai_charge(12);};
void()    hogre_leap3    =[    $leap3,    hogre_leap4    ] {ai_charge(12);};
void()    hogre_leap4    =[    $leap4,    hogre_leap5    ] {ai_charge(15);};
void()    hogre_leap5    =[    $leap5,    hogre_leap6    ] {ai_charge(7);};
void()    hogre_leap6    =[    $leap6,    hogre_leap7    ] {ai_charge(14);};
void()    hogre_leap7    =[    $leap7,    hogre_leap8    ] {ai_charge(13);
    self.touch = hogre_JumpTouch;
    makevectors (self.angles);
    self.origin_z = self.origin_z + 1;
    self.velocity = v_forward * 350 + '0 0 200';
    if (self.flags & FL_ONGROUND) self.flags = self.flags - FL_ONGROUND;
};
void()    hogre_leap8    =[    $leap8,    hogre_leap9    ] {ai_face();};    // Inflight frames
void()    hogre_leap9    =[    $leap9,    hogre_leap10] {ai_face();};
void()    hogre_leap10=[    $leap10,hogre_leap10] {ai_face();};

//----------------------------------------------------------------------
void()    hogre_leap11    =[    $leap11,    hogre_leap12    ] {chainsaw(0,2);};
void()    hogre_leap12    =[    $leap12,    hogre_leap13    ] {chainsaw(0,1.75);};
void()    hogre_leap13    =[    $leap13,    hogre_leap14    ] {chainsaw(0,1.5);};
void()    hogre_leap14    =[    $leap14,    hogre_leap15    ] {};
void()    hogre_leap15    =[    $leap15,    hogre_run1    ] {};
*/

/*======================================================================
 OgreFireGrenade (use version in mon_ogre.qc)
======================================================================*/
void() hogremac_shoot1    =[    $shoot1,        hogremac_shoot2    ] {ai_face();
    MonsterGrenadeSound(); self.attack_speed = MonsterGrenadeSpeed();
    self.attack_elev = SUB_Elevation(ELEV_DEFAULT, self.origin, self.enemy.origin, self.attack_speed); };
void() hogremac_shoot2    =[    $shoot2,        hogremac_shoot3    ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, self.attack_speed); };
void() hogremac_shoot3    =[    $shoot3,        hogremac_shoot4    ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, self.attack_speed); };
void() hogremac_shoot4    =[    $shoot4,        hogremac_shoot5    ] {ai_face();
    MonsterFireGrenade(self.origin, self.enemy.origin);};
void() hogremac_shoot5    =[    $shoot5,        hogremac_shoot6    ] {ai_face();};
void() hogremac_shoot6    =[    $shoot6,        hogremac_shoot7    ] {ai_face();};
void() hogremac_shoot7    =[    $shoot7,        hogremac_run1    ] {ai_face();};

//======================================================================
// painA - Quick backwards flex
//----------------------------------------------------------------------
void()    hogremac_painA1    =[    $painA1,    hogremac_painA2 ] {};
void()    hogremac_painA2    =[    $painA2,    hogremac_painA3 ] {};
void()    hogremac_painA3    =[    $painA3,    hogremac_painA4 ] {};
void()    hogremac_painA4    =[    $painA4,    hogremac_painA5 ] {};
void()    hogremac_painA5    =[    $painA5,    hogremac_run1     ] {};

//----------------------------------------------------------------------
// painB - Stagger backwards
//----------------------------------------------------------------------
void()    hogremac_painB1    =[    $painB1,    hogremac_painB2 ] {ai_pain(7);};
void()    hogremac_painB2    =[    $painB2,    hogremac_painB3 ] {ai_pain(5);};
void()    hogremac_painB3    =[    $painB3,    hogremac_painB4 ] {};
void()    hogremac_painB4    =[    $painB4,    hogremac_painB5 ] {monster_footstep(FALSE); };
void()    hogremac_painB5    =[    $painB5,    hogremac_painB6 ] {};
void()    hogremac_painB6    =[    $painB6,    hogremac_painB7 ] {ai_forward(2);};
void()    hogremac_painB7    =[    $painB7,    hogremac_painB8 ] {ai_forward(2);};
void()    hogremac_painB8    =[    $painB8,    hogremac_painB9 ] {ai_forward(2);};
void()    hogremac_painB9    =[    $painB9,    hogremac_painB10 ] {ai_forward(2);};
void()    hogremac_painB10=[    $painB10,    hogremac_painB11 ] {ai_forward(2);};
void()    hogremac_painB11=[    $painB11,    hogremac_painB12 ] {ai_forward(2);};
void()    hogremac_painB12=[    $painB12,    hogremac_painB13 ] {ai_forward(2);};
void()    hogremac_painB13=[    $painB13,    hogremac_run1    ] {monster_footstep(FALSE); ai_forward(2);};

//----------------------------------------------------------------------
// painC - Fall down
//----------------------------------------------------------------------
void()    hogremac_painC1 =[    $painC1,    hogremac_painC2    ] {ai_pain(5);};
void()    hogremac_painC2 =[    $painC2,    hogremac_painC3    ] {ai_pain(2);};
void()    hogremac_painC3 =[    $painC3,    hogremac_painC4    ] {};
void()    hogremac_painC4 =[    $painC4,    hogremac_painC5    ] {};
void()    hogremac_painC5 =[    $painC5,    hogremac_painC6    ] {hogremac_sound(3);}; // Ogre hits floor
void()    hogremac_painC6 =[    $painC6,    hogremac_painC7    ] {};
void()    hogremac_painC7 =[    $painC7,    hogremac_painC8    ] {};
void()    hogremac_painC8 =[    $painC8,    hogremac_painC9    ] {};
void()    hogremac_painC9 =[    $painC9,    hogremac_painC10] {};
void()    hogremac_painC10=[    $painC10,    hogremac_painC11] {monster_footstep(FALSE);};
void()    hogremac_painC11=[    $painC11,    hogremac_painC12] {};
void()    hogremac_painC12=[    $painC12,    hogremac_painC13] {};
void()    hogremac_painC13=[    $painC13,    hogremac_painC14] {monster_footstep(FALSE);ai_forward(3);};
void()    hogremac_painC14=[    $painC14,    hogremac_painC15] {ai_forward(4);};
void()    hogremac_painC15=[    $painC15,    hogremac_painC16] {ai_forward(4);};
void()    hogremac_painC16=[    $painC16,    hogremac_painC17] {ai_forward(5);};
void()    hogremac_painC17=[    $painC17,    hogremac_run1    ] {ai_forward(4);};

//======================================================================
void(entity inflictor, entity attacker, float damage) hogremac_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
    
        if (self.pain_check == 1) {
            // Randomly pick which pain animation to play
            self.lip = random();
            if (self.lip < 0.65) hogremac_painA1 ();
            else if (self.lip < 0.85) {
                self.pain_finished = time + 2;
                // Stagger backwards
                hogremac_painB1 ();
            }
            else {
                self.pain_finished = time + 2;
                // Fall on the floor
                hogremac_painC1 ();
            }
        }
        if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.axhitme = 0;
            if (random() < 0.5) {
                self.pain_finished = time + 1.3;
                hogremac_painC1 ();    // Fall to ground
            }
            else {
                self.pain_finished = time + 1.7;
                hogremac_painB1 ();    // Stagger backwards
            }
        }
    }
};

//----------------------------------------------------------------------
// deathA (Forward)
//----------------------------------------------------------------------
void()    hogremac_dieA1    =[    $deathA1,    hogremac_dieA2    ] {ai_forward(14);};
void()    hogremac_dieA2    =[    $deathA2,    hogremac_dieA3    ] {monster_check_gib();ai_forward(14);};
void()    hogremac_dieA3    =[    $deathA3,    hogremac_dieA4    ] {monster_check_gib();ai_forward(3);
    self.solid = SOLID_NOT; if (!self.gibbed) DropBackpack(); };
void()    hogremac_dieA4    =[    $deathA4,    hogremac_dieA5    ] {ai_forward(1);};
void()    hogremac_dieA5    =[    $deathA5,    hogremac_dieA6    ] {ai_forward(1);};
void()    hogremac_dieA6    =[    $deathA6,    hogremac_dieA7    ] {ai_forward(1);};
void()    hogremac_dieA7    =[    $deathA7,    hogremac_dieA8    ] {};
void()    hogremac_dieA8    =[    $deathA8,    hogremac_dieA9    ] {hogremac_sound(3);};    // Chainsaw hits floor
void()    hogremac_dieA9    =[    $deathA9,    hogremac_dieA10    ] {monster_death_postcheck();};
void()    hogremac_dieA10    =[    $deathA10,    hogremac_dieA10    ] {};

//----------------------------------------------------------------------
// deathB (Backward)
//----------------------------------------------------------------------
void()    hogremac_dieB1    =[    $deathB1,    hogremac_dieB2    ] {ai_back(4);};
void()    hogremac_dieB2    =[    $deathB2,    hogremac_dieB3    ] {monster_check_gib();ai_back(10);};
void()    hogremac_dieB3    =[    $deathB3,    hogremac_dieB4    ] {monster_check_gib();ai_back(3);
    self.solid = SOLID_NOT; if (!self.gibbed) DropBackpack(); };
void()    hogremac_dieB4    =[    $deathB4,    hogremac_dieB5    ] {ai_back(1);};
void()    hogremac_dieB5    =[    $deathB5,    hogremac_dieB6    ] {};
void()    hogremac_dieB6    =[    $deathB6,    hogremac_dieB7    ] {};
void()    hogremac_dieB7    =[    $deathB7,    hogremac_dieB8    ] {ai_back(1);};
void()    hogremac_dieB8    =[    $deathB8,    hogremac_dieB9    ] {ai_back(1);};
void()    hogremac_dieB9    =[    $deathB9,    hogremac_dieB10] {hogremac_sound(3);};    // Chainsaw hits floor
void()    hogremac_dieB10=[    $deathB10,    hogremac_dieB11] {};
void()    hogremac_dieB11=[    $deathB11,    hogremac_dieB12] {};
void()    hogremac_dieB12=[    $deathB12,    hogremac_dieB13] {};
void()    hogremac_dieB13=[    $deathB13,    hogremac_dieB14] {monster_death_postcheck();};
void()    hogremac_dieB14=[    $deathB14,    hogremac_dieB14] {};

//======================================================================
void() hogremac_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "ogre/ogdth.wav", 1, ATTN_NORM);
        if (self.lip < 0.5) hogremac_dieA1();
        else hogremac_dieB1();
    }
};

//============================================================================
void(float soundtype) hogremac_sound =
{
    if (soundtype == 1) sound (self, CHAN_VOICE, "ogre/ogdrag.wav", 1, ATTN_IDLE);
    else if (soundtype == 2) sound (self, CHAN_WEAPON, "ogre/ogsawatk.wav", 1, ATTN_NORM);
    else if (soundtype == 3) sound (self, CHAN_WEAPON, GIB_SOUND_WOOD, 1, ATTN_NORM);
    else if (soundtype == 4) sound (self, CHAN_VOICE, "ogre/ham_swipe.wav", 1, ATTN_IDLE);
};

//----------------------------------------------------------------------
// Special wakeup animation for attacking/breaking something infront
// The monster should be setup facing the right direction before call
// Starts in large overhead swing downwards (no damage checks)
//----------------------------------------------------------------------
void() hogremac_wakeup1a    =[    $smash7,        hogremac_wakeup2a ] {monster_sightsound();};
void() hogremac_wakeup2a    =[    $smash6,        hogremac_wakeup3a ] {};
void() hogremac_wakeup3a    =[    $smash8,        hogremac_wakeup4a ] {};
void() hogremac_wakeup4a    =[    $smash9,        hogremac_wakeup5a ] {};
void() hogremac_wakeup5a    =[    $smash10,        hogremac_wakeup6a ] {};
void() hogremac_wakeup6a    =[    $smash11,        hogremac_wakeup7a ] {};
void() hogremac_wakeup7a    =[    $smash12,        hogremac_wakeup8a ] {};
void() hogremac_wakeup8a    =[    $smash13,        hogremac_wakeup9a ] {};
void() hogremac_wakeup9a    =[    $smash14,        hogremac_run1    ] {};

/*======================================================================
/*QUAKED monster_hogremac (0.5 0.75 0) (-32 -32 -24) (32 32 64) AMBUSH x METAL NOSIGHT NOIDLE NOGFX STARTOFF ANGRY Not_Easy Not_Normal Not_Hard Not_DM
{ model(":progs/ad171/mon_hogremac.mdl"); }
Ogre, 200 health points.
-------- KEYS --------
-------- SPAWNFLAGS --------
METAL : Chain mail metal armour and +100HP
-------- NOTES --------

======================================================================*/
void() monster_hogremac =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_hogremac.mdl";        // New Ogre Model
    self.headmdl = "progs/ad171/h_hogre.mdl";            // Large Head
    self.gib1mdl = "progs/ad171/w_ogremac.mdl";        // Heavy Mace
    self.gib2mdl = "progs/ad171/gib_ogfoot1.mdl";        // Left foot
    self.gib3mdl = "progs/ad171/gib_ogfoot2.mdl";        // Right foot
    self.gib4mdl = "progs/ad171/w_ogregl.mdl";        // Grenade Launcher

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_GRENADE);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    precache_model (self.gib4mdl);

    self.gib1sound = GIB_IMPACT_WOOD;
    if (random() < 0.5) self.gib1mdl = self.gib4mdl;

    self.idle_sound = "ogre/ogidle.wav";
    precache_sound (self.idle_sound);
    self.idle_soundcom = "ogre/ogidle2.wav";
    precache_sound (self.idle_soundcom);

    precache_sound ("ogre/ogdrag.wav");        // Chainsaw scrap on floor sound
    precache_sound ("ogre/ogdth.wav");        // Death sound
    self.pain_sound = "ogre/ogpain1.wav";
    precache_sound (self.pain_sound);        // Pain
    
    precache_sound ("ogre/ogsawatk.wav");    // Attack
    precache_sound ("ogre/ham_swipe.wav");    // mace swipe
    precache_sound ("ogre/ham_hit.wav");    // mace hit ground
    
    self.meleehitsound = GIB_SOUND_HEAVY;
    precache_sound (self.meleehitsound);    // mace hit

    self.sight_sound = "ogre/ogwake.wav";
    precache_sound (self.sight_sound);

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_WIDE;

    // Standard ogre health 200 with metal+ option
    if (self.health < 1) self.health = 200;
    if (self.spawnflags & MON_HOGRE_METAL) {
        if (self.deathstring == "")  self.deathstring = " was smashed by an armored Mace Ogre\n";
        self.health = self.health + MON_HOGRE_METUPG;
        self.skin = 1;
    } else {
        if (self.deathstring == "")  self.deathstring = " was smashed by a Mace Ogre\n";
    }
    
    self.gibhealth = -80;
    self.gibbed = FALSE;
    self.pain_flinch = 30;
    self.pain_longanim = TRUE;
    self.steptype = FS_TYPEMEDIUM;
    self.meleecontact = FALSE;                // Smash contact
    self.meleerange = MONAI_MELEEBLUNTOGRE;    // Blunt mace attack
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.th_checkattack = OgreCheckAttack;
    self.th_stand = hogremac_stand1;
    self.th_walk = hogremac_walk1;
    self.th_run = hogremac_run1;
    self.th_die = hogremac_die;
    self.th_melee = hogremac_melee;
    self.th_missile = hogremac_shoot1;
//    self.th_jump = hogre_leap1;
    self.th_pain = hogremac_pain;
    self.th_wakeup = hogremac_wakeup1a;
    
    self.ammo_rockets = 2;
    self.classtype = CT_MONHOGREMAC;    // new style hunter ogre
    self.classgroup = CG_OGRE;            // One big happy ogre family!
    self.classmove = MON_MOVEWALK;

    monster_start();
};
