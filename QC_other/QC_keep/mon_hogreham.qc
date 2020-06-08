/*==============================================================================
HUNTER OGRE - Hammer Time
==============================================================================*/
$cd id1/models/hogre_ham
$origin 0 0 24
$base base        
$skin base

// Default stand
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8

// Walking
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12

// Running (classic pose)
$frame run1 run2 run3 run4 run5 run6 run7 run8

// Swing (sideways attack)
$frame swing1 swing2 swing3 swing4 swing5 swing6 swing7
$frame swing8 swing9 swing10 swing11 swing12 swing13 swing14

// Smash (Overarm attack)
$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7
$frame smash8 smash9 smash10 smash11 smash12 smash13 smash14

// Thrust (Forward attack)
$frame slam1 slam2 slam3 slam4 slam5 slam6 slam7 slam8
$frame slam9 slam10 slam11 slam12 slam13 slam14 slam15 slam16 slam17

// turretidle (turret mode)
$frame turidle1 turidle2 turidle3 turidle4
$frame turidle5 turidle6 turidle7 turidle8

// Shoot (GL attack)
$frame shoot1 shoot2 shoot3 shoot4 shoot5 shoot6 shoot7 shoot8

// Shootpain (turret mode)
$frame turpainA1 turpainA2 turpainA3 turpainA4 turpainA5

// painA - Quick backwards flex
$frame painA1 painA2 painA3 painA4 painA5

// painB - Stagger backwards
$frame painB1 painB2 painB3 painB4 painB5 painB6 painB7 painB8
$frame painB9 painB10 painB11 painB12 painB13

// painC - Fall down
$frame painC1 painC2 painC3 painC4 painC5 painC6 painC7 painC8
$frame painC9 painC10 painC11 painC12 painC13 painC14 painC15 painC16 painC17

// turretWake (turret mode)
$frame turwake1 turwake2 turwake3 turwake4

// turretRelease (turret mode)
$frame turrel1 turrel2 turrel3 turrel4

// deathA (Forward)
$frame deathA1 deathA2 deathA3 deathA4 deathA5 deathA6 deathA7 deathA8 
$frame deathA9 deathA10

// deathB (Backward)
$frame deathB1 deathB2 deathB3 deathB4 deathB5 deathB6 deathB7 deathB8 
$frame deathB9 deathB10 deathB11 deathB12 deathB13 deathB14

void(float soundtype) hogreham_sound;

//============================================================================
void() hogreham_stand1    =[    $stand1,    hogreham_stand2    ] {monster_idle_sound(); ai_stand();};
void() hogreham_stand2    =[    $stand2,    hogreham_stand3    ] {ai_stand();};
void() hogreham_stand3    =[    $stand3,    hogreham_stand4    ] {ai_stand();};
void() hogreham_stand4    =[    $stand4,    hogreham_stand5    ] {ai_stand();};
void() hogreham_stand5    =[    $stand5,    hogreham_stand6    ] {ai_stand();};
void() hogreham_stand6    =[    $stand6,    hogreham_stand7    ] {ai_stand();};
void() hogreham_stand7    =[    $stand7,    hogreham_stand8    ] {ai_stand();};
void() hogreham_stand8    =[    $stand8,    hogreham_stand1    ] {ai_stand();};

//============================================================================
void() hogreham_walk1    =[    $walk1,        hogreham_walk2    ] {ai_walk(4);};
void() hogreham_walk2    =[    $walk2,        hogreham_walk3    ] {ai_walk(4);};
void() hogreham_walk3    =[    $walk3,        hogreham_walk4    ] {monster_idle_sound(); ai_walk(4);};
void() hogreham_walk4    =[    $walk4,        hogreham_walk5    ] {ai_walk(4);};
void() hogreham_walk5    =[    $walk5,        hogreham_walk6    ] {ai_walk(4);};
void() hogreham_walk6    =[    $walk6,        hogreham_walk7    ] {monster_footstep(FALSE); ai_walk(4);};
void() hogreham_walk7    =[    $walk7,        hogreham_walk8    ] {ai_walk(4);};
void() hogreham_walk8    =[    $walk8,        hogreham_walk9    ] {ai_walk(4);};
void() hogreham_walk9    =[    $walk9,        hogreham_walk10    ] {ai_walk(4);};
void() hogreham_walk10    =[    $walk10,    hogreham_walk11    ] {ai_walk(4);};
void() hogreham_walk11    =[    $walk11,    hogreham_walk12    ] {ai_walk(4);};
void() hogreham_walk12    =[    $walk12,    hogreham_walk1    ] {monster_footstep(FALSE);ai_walk(3);};

//============================================================================
void() hogreham_run1    =[    $run1,        hogreham_run2    ] {monster_idle_sound(); ai_run(8);};
void() hogreham_run2    =[    $run2,        hogreham_run3    ] {monster_footstep(FALSE); ai_run(13);};
void() hogreham_run3    =[    $run3,        hogreham_run4    ] {ai_run(13);};
void() hogreham_run4    =[    $run4,        hogreham_run5    ] {ai_run(13);};
void() hogreham_run5    =[    $run5,        hogreham_run6    ] {ai_run(8);};
void() hogreham_run6    =[    $run6,        hogreham_run7    ] {monster_footstep(FALSE); ai_run(16);};
void() hogreham_run7    =[    $run7,        hogreham_run8    ] {ai_run(18);};
void() hogreham_run8    =[    $run8,        hogreham_run1    ] {ai_run(13);};

//============================================================================
// ogreham_melee(float melee_attack, float side) - defined in aisubs.qc
//----------------------------------------------------------------------
void() hogreham_swing1    =[    $swing1,    hogreham_swing2    ] {ai_charge(4);};
void() hogreham_swing2    =[    $swing2,    hogreham_swing3    ] {ai_charge(8);hogreham_sound(4);};
void() hogreham_swing3    =[    $swing3,    hogreham_swing4    ] {ai_charge(8);};
void() hogreham_swing4    =[    $swing4,    hogreham_swing5    ] {ai_charge(8);};
void() hogreham_swing5    =[    $swing5,    hogreham_swing6    ] {ai_charge(12);};
void() hogreham_swing6    =[    $swing6,    hogreham_swing7    ] {ai_forward(14);
    self.meleehitsound = GIB_SOUND_HEAVY;
    self.meleecontact = TRUE;ai_meleesmash(20);self.meleecontact = FALSE;
};
void() hogreham_swing7    =[    $swing7,    hogreham_swing8    ] {ai_forward(7);monster_footstep(FALSE);};
void() hogreham_swing8    =[    $swing8,    hogreham_swing9    ] {ai_forward(3);};
void() hogreham_swing9    =[    $swing9,    hogreham_swing10 ] {ai_forward(3);};
void() hogreham_swing10    =[    $swing10,    hogreham_swing11 ] {ai_forward(3);};
void() hogreham_swing11    =[    $swing11,    hogreham_swing12 ] {ai_forward(3);};
void() hogreham_swing12    =[    $swing12,    hogreham_swing13 ] {ai_face();monster_footstep(FALSE);};
void() hogreham_swing13    =[    $swing13,    hogreham_swing14 ] {ai_face();};
void() hogreham_swing14    =[    $swing14,    hogreham_run1    ] {ai_face();};

//----------------------------------------------------------------------
void() hogreham_smash1    =[    $smash1,    hogreham_smash2    ] {ai_charge(10);};
void() hogreham_smash2    =[    $smash2,    hogreham_smash3    ] {ai_charge(10);hogreham_sound(4);};
void() hogreham_smash3    =[    $smash3,    hogreham_smash4    ] {ai_charge(10);};
void() hogreham_smash4    =[    $smash4,    hogreham_smash5    ] {ai_charge(8);};
void() hogreham_smash5    =[    $smash5,    hogreham_smash6    ] {ai_charge(6);};
void() hogreham_smash6    =[    $smash6,    hogreham_smash7    ] {ai_face();};
void() hogreham_smash7    =[    $smash7,    hogreham_smash8    ] {ai_face();};
void() hogreham_smash8    =[    $smash8,    hogreham_smash9    ] {ai_face();monster_footstep(FALSE);};
void() hogreham_smash9    =[    $smash9,    hogreham_smash10 ] {ai_face();};
void() hogreham_smash10    =[    $smash10,    hogreham_smash11 ] {ai_face();
    self.meleehitsound = GIB_SOUND_HEAVY;
    self.meleecontact = TRUE;ai_meleesmash(30);self.meleecontact = FALSE;
    makevectors (self.angles);                // Put impact infront of ogre
    self.finalangle = self.origin + (v_forward * 56) + (-v_up * 22);
    particle_explode(self.finalangle, 24, 0.5, PARTICLE_BURST_FIRE, PARTICLE_BURST_SHOCKWAVE); 
    sound (self, CHAN_WEAPON, "ogre/ham_hit.wav", 1, ATTN_IDLE);
};
void() hogreham_smash11    =[    $smash11,    hogreham_smash12 ] {};
void() hogreham_smash12    =[    $smash12,    hogreham_smash13 ] {};
void() hogreham_smash13    =[    $smash13,    hogreham_smash14 ] {ai_face();};
void() hogreham_smash14    =[    $smash14,    hogreham_run1    ] {ai_face();};

/*============================================================================
 Special new ground slam attack - knockback shockwave
============================================================================*/
//----------------------------------------------------------------------
// Slow wind up animation (impact frame - 13) so it is easier to spot
// Pain animations are suppressed so that the attack always happens
//----------------------------------------------------------------------
void() hogreham_slam1    =[    $slam1,        hogreham_slam2    ] {ai_charge(2);};
void() hogreham_slam2    =[    $slam2,        hogreham_slam3    ] {ai_charge(2);};
void() hogreham_slam3    =[    $slam3,        hogreham_slam4    ] {ai_charge(2);};
void() hogreham_slam4    =[    $slam4,        hogreham_slam5    ] {ai_charge(2);};
void() hogreham_slam5    =[    $slam5,        hogreham_slam6    ] {ai_charge(3);};
void() hogreham_slam6    =[    $slam6,        hogreham_slam7    ] {ai_charge(3);};
void() hogreham_slam7    =[    $slam7,        hogreham_slam8    ] {ai_face();};
void() hogreham_slam8    =[    $slam8,        hogreham_slam9    ] {ai_face();};
void() hogreham_slam9    =[    $slam9,        hogreham_slam10    ] {ai_charge(6);
    // Chance of ogre roar (sightsight) instead of weapon swipe
    if (random() < 0.2) sound (self, CHAN_VOICE, self.sight_sound, 1, ATTN_IDLE);
    else hogreham_sound(4);
};
void() hogreham_slam10    =[    $slam10,    hogreham_slam11 ] {ai_charge(8);};
void() hogreham_slam11    =[    $slam11,    hogreham_slam12 ] {ai_charge(8);};
void() hogreham_slam12    =[    $slam12,    hogreham_slam13 ] {ai_charge(4);};
void() hogreham_slam13    =[    $slam13,    hogreham_slam14 ] {
    ai_chargenoturn(4); self.meleehitsound = "ogre/ham_wave.wav";
    ai_shockwave(MONAI_HAMMERIMPACT, MONAI_HAMMERDAMAGE, MONAI_HAMMERRADIUS, MONAI_HAMMERFORWARD, MONAI_HAMMERUP);
};
void() hogreham_slam14    =[    $slam14,    hogreham_slam15 ] {ai_chargenoturn(3);};
void() hogreham_slam15    =[    $slam15,    hogreham_slam16 ] {};
void() hogreham_slam16    =[    $slam16,    hogreham_slam17 ] {};
void() hogreham_slam17    =[    $slam17,    hogreham_run1 ] {
    // Double check enemy is still alive?
    if (self.enemy.health > 1) {
        // If the enemy is visible, quickly fire a grenade at them
        if (visxray(self.enemy, self.view_ofs, self.enemy.view_ofs, FALSE)) {
            self.attack_state = AS_MISSILE;
            self.think = self.th_missile;
        }
    }
};

//----------------------------------------------------------------------
void() hogreham_melee =
{
    // Hammer ogres don't get interrupted with pain for melee
    self.pain_finished = time + 2;
    if (self.spawnflags & MON_HOGRE_METAL) hogreham_slam1();
    else {
        self.lip = random();
        if (self.lip < 0.5) hogreham_smash1();
        else hogreham_swing1();
    }
};

/*============================================================================
 OgreFireGrenade (use version in mon_ogre.qc)
============================================================================*/
void() hogreham_shoot1    =[    $shoot1,        hogreham_shoot2    ] {ai_face(); 
    MonsterGrenadeSound(); self.attack_speed = MonsterGrenadeSpeed();
    self.attack_elev = SUB_Elevation(ELEV_DEFAULT, self.origin, self.enemy.origin, self.attack_speed); };
void() hogreham_shoot2    =[    $shoot2,        hogreham_shoot3    ] {ai_face(); 
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, self.attack_speed); };
void() hogreham_shoot3    =[    $shoot3,        hogreham_shoot4    ] {ai_face(); 
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, self.attack_speed); };
void() hogreham_shoot4    =[    $shoot4,        hogreham_shoot5    ] {ai_face();
    MonsterFireGrenade(self.origin, self.enemy.origin);};
void() hogreham_shoot5    =[    $shoot5,        hogreham_shoot6    ] {ai_face();};
void() hogreham_shoot6    =[    $shoot6,        hogreham_shoot7    ] {ai_face();};
void() hogreham_shoot7    =[    $shoot7,        hogreham_run1    ] {ai_face();};

//============================================================================
// painA - Quick backwards flex
//----------------------------------------------------------------------
void()    hogreham_painA1    =[    $painA1,    hogreham_painA2    ] {};
void()    hogreham_painA2    =[    $painA2,    hogreham_painA3    ] {};
void()    hogreham_painA3    =[    $painA3,    hogreham_painA4    ] {};
void()    hogreham_painA4    =[    $painA4,    hogreham_painA5    ] {};
void()    hogreham_painA5    =[    $painA5,    hogreham_run1    ] {};

//----------------------------------------------------------------------
// painB - Stagger backwards
//----------------------------------------------------------------------
void()    hogreham_painB1    =[    $painB1,    hogreham_painB2    ] {ai_pain(7);};
void()    hogreham_painB2    =[    $painB2,    hogreham_painB3    ] {ai_pain(5);};
void()    hogreham_painB3    =[    $painB3,    hogreham_painB4    ] {};
void()    hogreham_painB4    =[    $painB4,    hogreham_painB5    ] {monster_footstep(FALSE); };
void()    hogreham_painB5    =[    $painB5,    hogreham_painB6    ] {};
void()    hogreham_painB6    =[    $painB6,    hogreham_painB7    ] {ai_forward(2);};
void()    hogreham_painB7    =[    $painB7,    hogreham_painB8    ] {ai_forward(2);};
void()    hogreham_painB8    =[    $painB8,    hogreham_painB9    ] {ai_forward(2);};
void()    hogreham_painB9    =[    $painB9,    hogreham_painB10] {ai_forward(2);};
void()    hogreham_painB10=[    $painB10,    hogreham_painB11] {ai_forward(2);};
void()    hogreham_painB11=[    $painB11,    hogreham_painB12] {ai_forward(2);};
void()    hogreham_painB12=[    $painB12,    hogreham_painB13] {ai_forward(2);};
void()    hogreham_painB13=[    $painB13,    hogreham_run1    ] {monster_footstep(FALSE); ai_forward(2);};

//----------------------------------------------------------------------
// painC - Fall down
//----------------------------------------------------------------------
void()    hogreham_painC1    =[    $painC1,    hogreham_painC2    ] {ai_pain(5);};
void()    hogreham_painC2    =[    $painC2,    hogreham_painC3    ] {ai_pain(2);};
void()    hogreham_painC3    =[    $painC3,    hogreham_painC4    ] {};
void()    hogreham_painC4    =[    $painC4,    hogreham_painC5    ] {};
void()    hogreham_painC5    =[    $painC5,    hogreham_painC6    ] {hogreham_sound(3);};    // Ogre hits floor
void()    hogreham_painC6    =[    $painC6,    hogreham_painC7    ] {};
void()    hogreham_painC7    =[    $painC7,    hogreham_painC8    ] {};
void()    hogreham_painC8    =[    $painC8,    hogreham_painC9    ] {};
void()    hogreham_painC9    =[    $painC9,    hogreham_painC10    ] {};
void()    hogreham_painC10=[    $painC10,    hogreham_painC11    ] {monster_footstep(FALSE);};
void()    hogreham_painC11=[    $painC11,    hogreham_painC12    ] {};
void()    hogreham_painC12=[    $painC12,    hogreham_painC13    ] {};
void()    hogreham_painC13=[    $painC13,    hogreham_painC14    ] {monster_footstep(FALSE);ai_forward(3);};
void()    hogreham_painC14=[    $painC14,    hogreham_painC15    ] {ai_forward(4);};
void()    hogreham_painC15=[    $painC15,    hogreham_painC16    ] {ai_forward(4);};
void()    hogreham_painC16=[    $painC16,    hogreham_painC17    ] {ai_forward(5);};
void()    hogreham_painC17=[    $painC17,    hogreham_run1    ] {ai_forward(4);};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) hogreham_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
    
        if (self.pain_check == 1) {
            // Randomly pick which pain animation to play
            self.lip = random();
            if (self.lip < 0.65) hogreham_painA1 ();
            else if (self.lip < 0.85) {
                self.pain_finished = time + 2;
                // Stagger backwards
                hogreham_painB1 ();
            }
            else {
                self.pain_finished = time + 2;
                // Fall on the floor
                hogreham_painC1 ();
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

//============================================================================
void()    hogreham_dieA1    =[    $deathA1,    hogreham_dieA2    ] {ai_forward(14);};
void()    hogreham_dieA2    =[    $deathA2,    hogreham_dieA3    ] {monster_check_gib();ai_forward(14);};
void()    hogreham_dieA3    =[    $deathA3,    hogreham_dieA4    ] {monster_check_gib();ai_forward(3);
    self.solid = SOLID_NOT; if (!self.gibbed) DropBackpack(); };
void()    hogreham_dieA4    =[    $deathA4,    hogreham_dieA5    ] {ai_forward(1);};
void()    hogreham_dieA5    =[    $deathA5,    hogreham_dieA6    ] {ai_forward(1);};
void()    hogreham_dieA6    =[    $deathA6,    hogreham_dieA7    ] {ai_forward(1);};
void()    hogreham_dieA7    =[    $deathA7,    hogreham_dieA8    ] {hogreham_sound(3);};    // Hammer hits floor
void()    hogreham_dieA8    =[    $deathA8,    hogreham_dieA9    ] {};
void()    hogreham_dieA9    =[    $deathA9,    hogreham_dieA10    ] {monster_death_postcheck();};
void()    hogreham_dieA10    =[    $deathA10,    hogreham_dieA10    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void()    hogreham_dieB1    =[    $deathB1,    hogreham_dieB2    ] {ai_back(4);};
void()    hogreham_dieB2    =[    $deathB2,    hogreham_dieB3    ] {monster_check_gib();ai_back(10);};
void()    hogreham_dieB3    =[    $deathB3,    hogreham_dieB4    ] {monster_check_gib();ai_back(3);};
void()    hogreham_dieB4    =[    $deathB4,    hogreham_dieB5    ] {ai_back(1);
    self.solid = SOLID_NOT; if (!self.gibbed) DropBackpack(); };
void()    hogreham_dieB5    =[    $deathB5,    hogreham_dieB6    ] {};
void()    hogreham_dieB6    =[    $deathB6,    hogreham_dieB7    ] {};
void()    hogreham_dieB7    =[    $deathB7,    hogreham_dieB8    ] {ai_back(1);};
void()    hogreham_dieB8    =[    $deathB8,    hogreham_dieB9    ] {ai_back(1);};
void()    hogreham_dieB9    =[    $deathB9,    hogreham_dieB10    ] {hogreham_sound(3);};    // Hammer hits floor
void()    hogreham_dieB10    =[    $deathB10,    hogreham_dieB11    ] {};
void()    hogreham_dieB11    =[    $deathB11,    hogreham_dieB12    ] {};
void()    hogreham_dieB12    =[    $deathB12,    hogreham_dieB13    ] {};
void()    hogreham_dieB13    =[    $deathB13,    hogreham_dieB14    ] {monster_death_postcheck();};
void()    hogreham_dieB14    =[    $deathB14,    hogreham_dieB14    ] {monster_deadbody_check();};
//----------------------------------------------------------------------
void() hogreham_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "ogre/ogdth.wav", 1, ATTN_NORM);
        self.lip = random();
        if (self.lip < 0.5) hogreham_dieA1();
        else hogreham_dieB1();
    }
};

//============================================================================
void(float soundtype) hogreham_sound =
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
void() hogreham_wakeup1a    =[    $smash7,        hogreham_wakeup2a ] {monster_sightsound();};
void() hogreham_wakeup2a    =[    $smash6,        hogreham_wakeup3a ] {};
void() hogreham_wakeup3a    =[    $smash8,        hogreham_wakeup4a ] {};
void() hogreham_wakeup4a    =[    $smash9,        hogreham_wakeup5a ] {};
void() hogreham_wakeup5a    =[    $smash10,        hogreham_wakeup6a ] {};
void() hogreham_wakeup6a    =[    $smash11,        hogreham_wakeup7a ] {};
void() hogreham_wakeup7a    =[    $smash12,        hogreham_wakeup8a ] {};
void() hogreham_wakeup8a    =[    $smash13,        hogreham_wakeup9a ] {};
void() hogreham_wakeup9a    =[    $smash14,        hogreham_run1    ] {};

/*======================================================================
/*QUAKED monster_hogreham (0.5 0.75 0) (-32 -32 -24) (32 32 64) AMBUSH x METAL NOSIGHT NOIDLE NOGFX STARTOFF ANGRY Not_Easy Not_Normal Not_Hard Not_DM
{ model(":progs/ad171/mon_hogreham.mdl"); }
Ogre, 300 health points.
-------- KEYS --------
-------- SPAWNFLAGS --------
METAL : Full plate metal armour and +100HP
-------- NOTES --------

======================================================================*/
void() monster_hogreham =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_hogreham.mdl";        // New Ogre Model
    self.headmdl = "progs/ad171/h_hogreham.mdl";        // Large head
    self.gib1mdl = "progs/ad171/w_ogreham.mdl";        // Ogre Hammer (3 skins)
    self.gib2mdl = "progs/ad171/gib_ogfoot1.mdl";        // Left foot
    self.gib3mdl = "progs/ad171/gib_ogfoot2.mdl";        // Right foot
    self.gib4mdl = "progs/ad171/w_ogregl.mdl";        // Grenade Launcher

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_GRENADE);
    precache_model (MODEL_PROJ_RINGSHOCK);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    precache_model (self.gib4mdl);
    self.gib1sound = GIB_IMPACT_WOOD;

    self.idle_sound = "ogre/ogidle.wav";
    precache_sound (self.idle_sound);
    self.idle_soundcom = "ogre/ogidle2.wav";
    precache_sound (self.idle_soundcom);

    precache_sound ("ogre/ogdrag.wav");        // Chainsaw scrap on floor sound
    precache_sound ("ogre/ogdth.wav");        // Death sound
    self.pain_sound = "ogre/ogpain1.wav";
    precache_sound (self.pain_sound);        // Pain
    
    precache_sound ("ogre/ogsawatk.wav");    // Attack
    precache_sound ("ogre/ham_swipe.wav");    // hammer swipe
    precache_sound ("ogre/ham_hit.wav");    // hammer hit ground
    precache_sound ("ogre/ham_wave.wav");    // hammer shockwave
    
    self.meleehitsound = GIB_SOUND_HEAVY;
    precache_sound (self.meleehitsound);    // hammer hit

    self.sight_sound = "ogre/ogwake.wav";
    precache_sound (self.sight_sound);

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_WIDE;
    
    // Upgraded ogre health 300 with metal+ option
    if (self.health < 1) self.health = 300;
    if (self.spawnflags & MON_HOGRE_METAL) {
        self.health = self.health + MON_HOGRE_METUPG;
        if (self.deathstring == "")  self.deathstring = " was pulverized by an armored Hammer Ogre\n";
        // Always drop hammer, make sure correct skin
        self.gib1skin = 1;
        self.skin = 1;
    } else {
        if (self.deathstring == "")  self.deathstring = " was pulverized by a Hammer Ogre\n";
        // Random chance to switch GL for hammer
        if (random() < 0.5) self.gib1mdl = self.gib4mdl;
    }
        
    self.gibhealth = -80;
    self.gibbed = FALSE;
    self.pain_flinch = 100;
    self.pain_timeout = 2;        // High pain resistance
    self.pain_longanim = FALSE;
    self.steptype = FS_TYPEHEAVY;
    self.meleecontact = FALSE;                // Smash contact
    self.meleerange = MONAI_MELEEBLUNTOGRE;    // Hammer range
    self.movespeed = 0;                        // Cannot be turret
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.th_checkattack = OgreHamCheckAttack;
    self.th_stand = hogreham_stand1;
    self.th_walk = hogreham_walk1;
    self.th_run = hogreham_run1;
    self.th_die = hogreham_die;
    self.th_melee = hogreham_melee;
    self.th_missile = hogreham_shoot1;
    self.th_pain = hogreham_pain;
    self.th_wakeup = hogreham_wakeup1a;
    
    self.ammo_rockets = 4;
    self.classtype = CT_MONHOGREHAM;    // new style hunter ogre
    self.classgroup = CG_OGRE;            // One big happy ogre family!
    self.classmove = MON_MOVEWALK;

    monster_start();
};
