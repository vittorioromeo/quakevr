/*==============================================================================
HUNTER OGRE
==============================================================================*/
$cd id1/models/hogre_c
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

// (068) Thrust (Forward attack)
$frame thrust1 thrust2 thrust3 thrust4 thrust5 thrust6 thrust7 thrust8
$frame thrust9 thrust10 thrust11 thrust12 thrust13 thrust14

// (082) Turret idle (Turret mode)
$frame turidle1 turidle2 turidle3 turidle4
$frame turidle5 turidle6 turidle7 turidle8

// (090) Shoot (GL attack - chainsaw down)
$frame shoot1 shoot2 shoot3 shoot4 shoot5 shoot6 shoot7 shoot8

// (098) Shootpain (turret mode)
$frame turpainA1 turpainA2 turpainA3 turpainA4 turpainA5

// (103) Shoot (GL attack - chainsaw up)
$frame shootB1 shootB2 shootB3 shootB4 shootB5 shootB6 shootB7 shootB8

// (111) painA - Quick backwards flex
$frame painA1 painA2 painA3 painA4 painA5

// (116) painB - Stagger backwards
$frame painB1 painB2 painB3 painB4 painB5 painB6 painB7 painB8
$frame painB9 painB10 painB11 painB12 painB13

// (129) painC - Fall down
$frame painC1 painC2 painC3 painC4 painC5 painC6 painC7 painC8
$frame painC9 painC10 painC11 painC12 painC13 painC14 painC15 painC16 painC17

// (146) TurretWake (Turret mode)
$frame turwake1 turwake2 turwake3 turwake4

// (150) TurretRelease (Turret mode)
$frame turrel1 turrel2 turrel3 turrel4

// (154) deathSilly (comedy death)
$frame deathS1 deathS2 deathS3 deathS4 deathS5 deathS6 deathS7 deathS8
$frame deathS9 deathS10

// (164) deathA (Forward)
$frame deathA1 deathA2 deathA3 deathA4 deathA5 deathA6 deathA7 deathA8 
$frame deathA9 deathA10

// (174) deathB (Backward)
$frame deathB1 deathB2 deathB3 deathB4 deathB5 deathB6 deathB7 deathB8 
$frame deathB9 deathB10 deathB11 deathB12 deathB13 deathB14

void(float soundtype) hogre_sound;

//======================================================================
void() hogre_stand1    =[    $stand1,    hogre_stand2    ] {monster_idle_sound(); ai_stand();};
void() hogre_stand2    =[    $stand2,    hogre_stand3    ] {ai_stand();};
void() hogre_stand3    =[    $stand3,    hogre_stand4    ] {ai_stand();};
void() hogre_stand4    =[    $stand4,    hogre_stand5    ] {ai_stand();};
void() hogre_stand5    =[    $stand5,    hogre_stand6    ] {ai_stand();};
void() hogre_stand6    =[    $stand6,    hogre_stand7    ] {ai_stand();};
void() hogre_stand7    =[    $stand7,    hogre_stand8    ] {ai_stand();};
void() hogre_stand8    =[    $stand8,    hogre_stand1    ] {ai_stand();};

//======================================================================
void() hogre_walkB1    =[    $walkB1,    hogre_walkB2    ] {ai_walk(4);};
void() hogre_walkB2    =[    $walkB2,    hogre_walkB3    ] {ai_walk(6);};
void() hogre_walkB3    =[    $walkB3,    hogre_walkB4    ] {monster_idle_sound(); ai_walk(6);};
void() hogre_walkB4    =[    $walkB4,    hogre_walkB5    ] {ai_walk(4);};
void() hogre_walkB5    =[    $walkB5,    hogre_walkB6    ] {monster_footstep(FALSE); ai_walk(0);};
void() hogre_walkB6    =[    $walkB6,    hogre_walkB7    ] {ai_walk(0);};
void() hogre_walkB7    =[    $walkB7,    hogre_walkB8    ] {ai_walk(6);};
void() hogre_walkB8    =[    $walkB8,    hogre_walkB9    ] {ai_walk(7); hogre_sound(1);};
void() hogre_walkB9    =[    $walkB9,    hogre_walkB10    ] {ai_walk(6);};
void() hogre_walkB10=[    $walkB10,    hogre_walkB11    ] {ai_walk(2);};
void() hogre_walkB11=[    $walkB11,    hogre_walkB12    ] {monster_footstep(FALSE);ai_walk(0);};
void() hogre_walkB12=[    $walkB12,    hogre_walk1    ] {ai_walk(4);};

//======================================================================
void() hogre_walk1    =[    $walk1,        hogre_walk2    ] {ai_walk(4);};
void() hogre_walk2    =[    $walk2,        hogre_walk3    ] {ai_walk(4);};
void() hogre_walk3    =[    $walk3,        hogre_walk4    ] {monster_idle_sound(); ai_walk(4);};
void() hogre_walk4    =[    $walk4,        hogre_walk5    ] {ai_walk(4);};
void() hogre_walk5    =[    $walk5,        hogre_walk6    ] {ai_walk(4);};
void() hogre_walk6    =[    $walk6,        hogre_walk7    ] {monster_footstep(FALSE); ai_walk(4);};
void() hogre_walk7    =[    $walk7,        hogre_walk8    ] {ai_walk(4);};
void() hogre_walk8    =[    $walk8,        hogre_walk9    ] {ai_walk(4);};
void() hogre_walk9    =[    $walk9,        hogre_walk10] {ai_walk(4); if (random() < 0.1) hogre_sound(1);};
void() hogre_walk10    =[    $walk10,    hogre_walk11] {ai_walk(4);};
void() hogre_walk11    =[    $walk11,    hogre_walk12] {ai_walk(3);};
void() hogre_walk12    =[    $walk12,    hogre_walk1    ] {monster_footstep(FALSE);
    if (random() < 0.05) self.think = hogre_walkB1;            // Alternative walk
    ai_walk(4);};

//======================================================================
void() hogre_run1    =[    $run1,        hogre_run2    ] {monster_idle_sound(); ai_run(8);};
void() hogre_run2    =[    $run2,        hogre_run3    ] {monster_footstep(FALSE); ai_run(13);};
void() hogre_run3    =[    $run3,        hogre_run4    ] {ai_run(13);};
void() hogre_run4    =[    $run4,        hogre_run5    ] {ai_run(13);};
void() hogre_run5    =[    $run5,        hogre_run6    ] {ai_run(8);};
void() hogre_run6    =[    $run6,        hogre_run7    ] {monster_footstep(FALSE); ai_run(16);};
void() hogre_run7    =[    $run7,        hogre_run8    ] {ai_run(18);};
void() hogre_run8    =[    $run8,        hogre_run1    ] {ai_run(13);};

/*======================================================================
 chainsaw (use version in mon_ogre.qc)
======================================================================*/
void() hogre_swing1    =[    $swing1,    hogre_swing2    ] {ai_charge(10);hogre_sound(2);};
void() hogre_swing2    =[    $swing2,    hogre_swing3    ] {ai_charge(10);};
void() hogre_swing3    =[    $swing3,    hogre_swing4    ] {ai_charge(6);};
void() hogre_swing4    =[    $swing4,    hogre_swing5    ] {ai_charge(6);};
void() hogre_swing5    =[    $swing5,    hogre_swing6    ] {ai_charge(7); chainsaw(0,1);self.angles_y = self.angles_y + random()*25;};
void() hogre_swing6    =[    $swing6,    hogre_swing7    ] {ai_forward(7); chainsaw(200,1);self.angles_y = self.angles_y + random()* 25;};
void() hogre_swing7    =[    $swing7,    hogre_swing8    ] {ai_forward(4); chainsaw(0,1);monster_footstep(FALSE); self.angles_y = self.angles_y + random()* 25;};
void() hogre_swing8    =[    $swing8,    hogre_swing9    ] {ai_forward(4); chainsaw(0,1);self.angles_y = self.angles_y + random()* 25;};
void() hogre_swing9    =[    $swing9,    hogre_swing10 ] {ai_forward(4); chainsaw(0,1);self.angles_y = self.angles_y + random()* 25;};
void() hogre_swing10=[    $swing10,    hogre_swing11 ] {ai_forward(4); chainsaw(-200,1);self.angles_y = self.angles_y + random()* 25;};
void() hogre_swing11=[    $swing11,    hogre_swing12 ] {ai_forward(4); chainsaw(0,1);self.angles_y = self.angles_y + random()* 25;};
void() hogre_swing12=[    $swing12,    hogre_swing13 ] {ai_face();};
void() hogre_swing13=[    $swing13,    hogre_swing14 ] {ai_face();};
void() hogre_swing14=[    $swing14,    hogre_run1    ] {ai_face();};

//----------------------------------------------------------------------
void() hogre_smash1    =[    $smash1,    hogre_smash2    ] {ai_charge(9);hogre_sound(2);};
void() hogre_smash2    =[    $smash2,    hogre_smash3    ] {ai_charge(9);};
void() hogre_smash3    =[    $smash3,    hogre_smash4    ] {monster_footstep(FALSE); ai_charge(9);};
void() hogre_smash4    =[    $smash4,    hogre_smash5    ] {ai_charge(12);};
void() hogre_smash5    =[    $smash5,    hogre_smash6    ] {ai_charge(12);};
void() hogre_smash6    =[    $smash6,    hogre_smash7    ] {ai_charge(5); chainsaw(0,1); monster_footstep(FALSE);};
void() hogre_smash7    =[    $smash7,    hogre_smash8    ] {ai_charge(5); chainsaw(0,1);};
void() hogre_smash8    =[    $smash8,    hogre_smash9    ] {ai_face(); chainsaw(0,1);};
void() hogre_smash9    =[    $smash9,    hogre_smash10 ] {ai_face(); chainsaw(0,1);};
void() hogre_smash10=[    $smash10,    hogre_smash11 ] {ai_face(); chainsaw(0,1);};
void() hogre_smash11=[    $smash11,    hogre_smash12 ] {ai_face(); chainsaw(0,1);
self.nextthink = self.nextthink + random()*0.2;};    // slight variation
void() hogre_smash12=[    $smash12,    hogre_smash13 ] {ai_face();};
void() hogre_smash13=[    $smash13,    hogre_smash14 ] {ai_face();};
void() hogre_smash14=[    $smash14,    hogre_run1    ] {ai_face();};

//----------------------------------------------------------------------
void() hogre_thrust1    =[    $thrust1,    hogre_thrust2    ] {ai_charge(12);hogre_sound(2);};
void() hogre_thrust2    =[    $thrust2,    hogre_thrust3    ] {ai_charge(12);};
void() hogre_thrust3    =[    $thrust3,    hogre_thrust4    ] {ai_charge(15);};
void() hogre_thrust4    =[    $thrust4,    hogre_thrust5    ] {ai_charge(7);};
void() hogre_thrust5    =[    $thrust5,    hogre_thrust6    ] {ai_charge(14);monster_footstep(FALSE);};
void() hogre_thrust6    =[    $thrust6,    hogre_thrust7    ] {ai_charge(13);chainsaw(0,2);};
void() hogre_thrust7    =[    $thrust7,    hogre_thrust8    ] {ai_charge(15);chainsaw(0,1.75);};
void() hogre_thrust8    =[    $thrust8,    hogre_thrust9    ] {ai_charge(11);chainsaw(0,1.5);};
void() hogre_thrust9    =[    $thrust9,    hogre_thrust10    ] {ai_charge(11);chainsaw(0,1.25);};
void() hogre_thrust10    =[    $thrust10,    hogre_thrust11    ] {ai_face();chainsaw(0,1);};
void() hogre_thrust11    =[    $thrust11,    hogre_thrust12    ] {ai_face();chainsaw(0,1);};
void() hogre_thrust12    =[    $thrust12,    hogre_thrust13    ] {ai_face();};
void() hogre_thrust13    =[    $thrust13,    hogre_thrust14    ] {ai_face();};
void() hogre_thrust14    =[    $thrust14,    hogre_run1    ] {ai_face();};

//----------------------------------------------------------------------
void() hogre_melee =
{
    self.lip = random();
    if (self.lip < 0.2 && skill > SKILL_NORMAL) hogre_thrust1();
    else if (self.lip < 0.4) hogre_smash1();
    else hogre_swing1();
};

/*======================================================================
 OgreFireGrenade (use version in mon_ogre.qc)
======================================================================*/
void() hogre_shoot1    =[    $shootB1,        hogre_shoot2    ] {ai_face();
    MonsterGrenadeSound(); self.attack_speed = MonsterGrenadeSpeed();
    self.attack_elev = SUB_Elevation(ELEV_DEFAULT, self.origin, self.enemy.origin, self.attack_speed); };
void() hogre_shoot2    =[    $shootB2,        hogre_shoot3    ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, self.attack_speed); };
void() hogre_shoot3    =[    $shootB3,        hogre_shoot4    ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, self.attack_speed); };
void() hogre_shoot4    =[    $shootB4,        hogre_shoot5    ] {ai_face();
    MonsterFireGrenade(self.origin, self.enemy.origin);};
void() hogre_shoot5    =[    $shootB5,        hogre_shoot6    ] {ai_face();};
void() hogre_shoot6    =[    $shootB6,        hogre_shoot7    ] {ai_face();};
void() hogre_shoot7    =[    $shootB7,        hogre_run1    ] {ai_face();};

//======================================================================
// painA - Quick backwards flex
//----------------------------------------------------------------------
void()    hogre_painA1    =[    $painA1,    hogre_painA2 ] {};
void()    hogre_painA2    =[    $painA2,    hogre_painA3 ] {};
void()    hogre_painA3    =[    $painA3,    hogre_painA4 ] {};
void()    hogre_painA4    =[    $painA4,    hogre_painA5 ] {};
void()    hogre_painA5    =[    $painA5,    hogre_run1     ] {};

//----------------------------------------------------------------------
// painB - Stagger backwards
//----------------------------------------------------------------------
void()    hogre_painB1    =[    $painB1,    hogre_painB2 ] {ai_pain(7);};
void()    hogre_painB2    =[    $painB2,    hogre_painB3 ] {ai_pain(5);};
void()    hogre_painB3    =[    $painB3,    hogre_painB4 ] {};
void()    hogre_painB4    =[    $painB4,    hogre_painB5 ] {monster_footstep(FALSE); };
void()    hogre_painB5    =[    $painB5,    hogre_painB6 ] {};
void()    hogre_painB6    =[    $painB6,    hogre_painB7 ] {ai_forward(2);};
void()    hogre_painB7    =[    $painB7,    hogre_painB8 ] {ai_forward(2);};
void()    hogre_painB8    =[    $painB8,    hogre_painB9 ] {ai_forward(2);};
void()    hogre_painB9    =[    $painB9,    hogre_painB10 ] {ai_forward(2);};
void()    hogre_painB10    =[    $painB10,    hogre_painB11 ] {ai_forward(2);};
void()    hogre_painB11    =[    $painB11,    hogre_painB12 ] {ai_forward(2);};
void()    hogre_painB12    =[    $painB12,    hogre_painB13 ] {ai_forward(2);};
void()    hogre_painB13    =[    $painB13,    hogre_run1    ] {monster_footstep(FALSE); ai_forward(2);};

//----------------------------------------------------------------------
// painC - Fall down
//----------------------------------------------------------------------
void()    hogre_painC1 =[    $painC1,    hogre_painC2    ] {ai_pain(5);};
void()    hogre_painC2 =[    $painC2,    hogre_painC3    ] {ai_pain(2);};
void()    hogre_painC3 =[    $painC3,    hogre_painC4    ] {};
void()    hogre_painC4 =[    $painC4,    hogre_painC5    ] {};
void()    hogre_painC5 =[    $painC5,    hogre_painC6    ] {hogre_sound(3);}; // Ogre hits floor
void()    hogre_painC6 =[    $painC6,    hogre_painC7    ] {};
void()    hogre_painC7 =[    $painC7,    hogre_painC8    ] {};
void()    hogre_painC8 =[    $painC8,    hogre_painC9    ] {};
void()    hogre_painC9 =[    $painC9,    hogre_painC10    ] {};
void()    hogre_painC10=[    $painC10,    hogre_painC11    ] {monster_footstep(FALSE);};
void()    hogre_painC11=[    $painC11,    hogre_painC12    ] {};
void()    hogre_painC12=[    $painC12,    hogre_painC13    ] {};
void()    hogre_painC13=[    $painC13,    hogre_painC14    ] {monster_footstep(FALSE);ai_forward(3);};
void()    hogre_painC14=[    $painC14,    hogre_painC15    ] {ai_forward(4);};
void()    hogre_painC15=[    $painC15,    hogre_painC16    ] {ai_forward(4);};
void()    hogre_painC16=[    $painC16,    hogre_painC17    ] {ai_forward(5);};
void()    hogre_painC17=[    $painC17,    hogre_run1    ] {ai_forward(4);};

//======================================================================
void(entity inflictor, entity attacker, float damage) hogre_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
    
        if (self.pain_check == 1) {
            // Randomly pick which pain animation to play
            self.lip = random();
            if (self.lip < 0.65) hogre_painA1 ();
            else if (self.lip < 0.85) {
                self.pain_finished = time + 2;
                // Stagger backwards
                hogre_painB1 ();
            }
            else {
                self.pain_finished = time + 2;
                // Fall on the floor
                hogre_painC1 ();
            }
        }
        if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.axhitme = 0;
            if (random() < 0.5) {
                self.pain_finished = time + 1.3;
                hogre_painC1 ();    // Fall to ground
            }
            else {
                self.pain_finished = time + 1.7;
                hogre_painB1 ();    // Stagger backwards
            }
        }
    }
};

//----------------------------------------------------------------------
// (146 - 155) deathSilly (comedy death)
//----------------------------------------------------------------------
void()    hogre_dieS1    =[    $deathS1,    hogre_dieS2    ] {ai_forward(14);};
void()    hogre_dieS2    =[    $deathS2,    hogre_dieS3    ] {monster_check_gib();ai_forward(14);};
void()    hogre_dieS3    =[    $deathS3,    hogre_dieS4    ] {monster_check_gib();ai_forward(3);
    self.solid = SOLID_NOT; if (!self.gibbed) DropBackpack(); };
void()    hogre_dieS4    =[    $deathS4,    hogre_dieS5    ] {ai_forward(1);};
void()    hogre_dieS5    =[    $deathS5,    hogre_dieS6    ] {ai_forward(1);};
void()    hogre_dieS6    =[    $deathS6,    hogre_dieS7    ] {ai_forward(1);};
void()    hogre_dieS7    =[    $deathS7,    hogre_dieS8    ] {};
void()    hogre_dieS8    =[    $deathS8,    hogre_dieS9    ] {hogre_sound(3);};    // Chainsaw hits floor
void()    hogre_dieS9    =[    $deathS9,    hogre_dieS10] {monster_death_postcheck();};
void()    hogre_dieS10=[    $deathS10,    hogre_dieS10] {monster_deadbody_check();};

//----------------------------------------------------------------------
// (156 - 165) deathA (Forward)
//----------------------------------------------------------------------
void()    hogre_dieA1    =[    $deathA1,    hogre_dieA2    ] {ai_forward(14);};
void()    hogre_dieA2    =[    $deathA2,    hogre_dieA3    ] {monster_check_gib();ai_forward(14);};
void()    hogre_dieA3    =[    $deathA3,    hogre_dieA4    ] {monster_check_gib();ai_forward(3);
    self.solid = SOLID_NOT; if (!self.gibbed) DropBackpack(); };
void()    hogre_dieA4    =[    $deathA4,    hogre_dieA5    ] {ai_forward(1);};
void()    hogre_dieA5    =[    $deathA5,    hogre_dieA6    ] {ai_forward(1);};
void()    hogre_dieA6    =[    $deathA6,    hogre_dieA7    ] {ai_forward(1);};
void()    hogre_dieA7    =[    $deathA7,    hogre_dieA8    ] {};
void()    hogre_dieA8    =[    $deathA8,    hogre_dieA9    ] {hogre_sound(3);};    // Chainsaw hits floor
void()    hogre_dieA9    =[    $deathA9,    hogre_dieA10] {monster_death_postcheck();};
void()    hogre_dieA10=[    $deathA10,    hogre_dieA10] {monster_deadbody_check();};

//----------------------------------------------------------------------
// (166 - 179) deathB (Backward)
//----------------------------------------------------------------------
void()    hogre_dieB1    =[    $deathB1,    hogre_dieB2    ] {ai_back(4);};
void()    hogre_dieB2    =[    $deathB2,    hogre_dieB3    ] {monster_check_gib();ai_back(10);};
void()    hogre_dieB3    =[    $deathB3,    hogre_dieB4    ] {monster_check_gib();ai_back(3);
    self.solid = SOLID_NOT; if (!self.gibbed) DropBackpack(); };
void()    hogre_dieB4    =[    $deathB4,    hogre_dieB5    ] {ai_back(1);};
void()    hogre_dieB5    =[    $deathB5,    hogre_dieB6    ] {};
void()    hogre_dieB6    =[    $deathB6,    hogre_dieB7    ] {};
void()    hogre_dieB7    =[    $deathB7,    hogre_dieB8    ] {ai_back(1);};
void()    hogre_dieB8    =[    $deathB8,    hogre_dieB9    ] {ai_back(1);};
void()    hogre_dieB9    =[    $deathB9,    hogre_dieB10] {hogre_sound(3);};    // Chainsaw
void()    hogre_dieB10=[    $deathB10,    hogre_dieB11] {};
void()    hogre_dieB11=[    $deathB11,    hogre_dieB12] {};
void()    hogre_dieB12=[    $deathB12,    hogre_dieB13] {};
void()    hogre_dieB13=[    $deathB13,    hogre_dieB14] {monster_death_postcheck();};
void()    hogre_dieB14=[    $deathB14,    hogre_dieB14] {monster_deadbody_check();};

//======================================================================
void() hogre_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "ogre/ogdth.wav", 1, ATTN_NORM);
        self.lip = random();
        if (self.lip < 0.05) hogre_dieS1();
        else if (self.lip < 0.5) hogre_dieA1();
        else hogre_dieB1();
    }
};

//============================================================================
void(float soundtype) hogre_sound =
{
    if (soundtype == 1) sound (self, CHAN_VOICE, "ogre/ogdrag.wav", 1, ATTN_IDLE);
    else if (soundtype == 2) sound (self, CHAN_WEAPON, "ogre/ogsawatk.wav", 1, ATTN_NORM);
    else if (soundtype == 3) sound (self, CHAN_WEAPON, GIB_SOUND_WOOD, 1, ATTN_NORM);
    else if (soundtype == 4) sound (self, CHAN_WEAPON, "ogre/ogleap.wav", 1, ATTN_NORM);
};

//----------------------------------------------------------------------
// Special wakeup animation for attacking/breaking something infront
// The monster should be setup facing the right direction before call
// Starts in large overhead swing downwards (no damage checks)
//----------------------------------------------------------------------
void() hogre_wakeup1a    =[    $smash7,    hogre_wakeup2a ] {monster_sightsound();};
void() hogre_wakeup2a    =[    $smash6,    hogre_wakeup3a ] {};
void() hogre_wakeup3a    =[    $smash8,    hogre_wakeup4a ] {};
void() hogre_wakeup4a    =[    $smash9,    hogre_wakeup5a ] {};
void() hogre_wakeup5a    =[    $smash10,    hogre_wakeup6a ] {};
void() hogre_wakeup6a    =[    $smash11,    hogre_wakeup7a ] {};
void() hogre_wakeup7a    =[    $smash12,    hogre_wakeup8a ] {};
void() hogre_wakeup8a    =[    $smash13,    hogre_wakeup9a ] {};
void() hogre_wakeup9a    =[    $smash14,    hogre_run1    ] {};

void() hogre_wakeup1b    =[    $thrust7,    hogre_wakeup2b ] {monster_sightsound();};
void() hogre_wakeup2b    =[    $thrust6,    hogre_wakeup3b ] {};
void() hogre_wakeup3b    =[    $thrust8,    hogre_wakeup4b ] {};
void() hogre_wakeup4b    =[    $thrust9,    hogre_wakeup5b ] {};
void() hogre_wakeup5b    =[    $thrust10,    hogre_wakeup6b ] {};
void() hogre_wakeup6b    =[    $thrust11,    hogre_wakeup7b ] {};
void() hogre_wakeup7b    =[    $thrust12,    hogre_wakeup8b ] {};
void() hogre_wakeup8b    =[    $thrust13,    hogre_wakeup9b ] {};
void() hogre_wakeup9b    =[    $thrust14,    hogre_run1    ] {};

void() hogre_wakeup1 = {
    if (random() < 0.7) hogre_wakeup1a();    // More over head swing than thrust
    else hogre_wakeup1b();
};

/*======================================================================
/*QUAKED monster_hogre (0.5 0.75 0) (-32 -32 -24) (32 32 64) AMBUSH x METAL NOSIGHT NOIDLE NOGFX STARTOFF ANGRY Not_Easy Not_Normal Not_Hard Not_DM
{ model(":progs/ad171/mon_hogre.mdl"); }
Ogre, 200 health points.
-------- KEYS --------
-------- SPAWNFLAGS --------
METAL : Chain mail metal armour and +100HP
-------- NOTES --------

======================================================================*/
void() monster_hogre =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_hogre.mdl";            // New Ogre Model
    self.headmdl = "progs/ad171/h_hogre.mdl";
    self.gib1mdl = "progs/ad171/w_chainsaw.mdl";        // Chainsaw
    self.gib2mdl = "progs/ad171/gib_ogfoot1.mdl";        // Left foot
    self.gib3mdl = "progs/ad171/gib_ogfoot2.mdl";        // Right foot
    self.gib4mdl = "progs/ad171/w_ogregl.mdl";        // Grenade Launcher

    precache_model ("progs/ogre.mdl");            // Precache for upgrade
    precache_model ("progs/h_ogre.mdl");        // Load/Quickload need this
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
    precache_sound ("ogre/ogleap.wav");        // Leap Attack

    self.sight_sound = "ogre/ogwake.wav";
    precache_sound (self.sight_sound);

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_WIDE;
    self.gibhealth = -80;
    self.gibbed = FALSE;
    self.pain_flinch = 30;
    self.pain_longanim = TRUE;
    self.steptype = FS_TYPESLOW;
    self.meleerange = MONAI_MELEEOGRE;    // Chainsaw range

    
    // Standard ogre health 200 with metal+ option
    if (self.health < 1) self.health = 200;
    if (self.spawnflags & MON_HOGRE_METAL) {
        self.health = self.health + MON_HOGRE_METUPG;
        if (self.deathstring == "")  self.deathstring = " was blown up by an armored Ogre Hunter\n";
        self.skin = 1;
    } else {
        if (self.deathstring == "")  self.deathstring = " was blown up by an Ogre Hunter\n";
    }

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.th_checkattack = OgreCheckAttack;
    self.th_stand = hogre_stand1;
    self.th_walk = hogre_walk1;
    self.th_run = hogre_run1;
    self.th_die = hogre_die;
    self.th_melee = hogre_melee;
    self.th_missile = hogre_shoot1;
    self.th_pain = hogre_pain;
    self.th_wakeup = hogre_wakeup1;
    
    self.ammo_rockets = 2;
    self.classtype = CT_MONHOGRE;        // new style hunter ogre
    self.classgroup = CG_OGRE;            // One big happy ogre family!
    self.classmove = MON_MOVEWALK;

    monster_start();
};
