/*==============================================================================
OGRE
==============================================================================*/

$cd id1/models/ogre_c
$origin 0 0 24
$base base        
$skin base

$frame    stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8 stand9

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7
$frame walk8 walk9 walk10 walk11 walk12 walk13 walk14 walk15 walk16

$frame run1 run2 run3 run4 run5 run6 run7 run8

$frame swing1 swing2 swing3 swing4 swing5 swing6 swing7
$frame swing8 swing9 swing10 swing11 swing12 swing13 swing14

$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7
$frame smash8 smash9 smash10 smash11 smash12 smash13 smash14

$frame shoot1 shoot2 shoot3 shoot4 shoot5 shoot6

$frame pain1 pain2 pain3 pain4 pain5

$frame painb1 painb2 painb3

$frame painc1 painc2 painc3 painc4 painc5 painc6

$frame paind1 paind2 paind3 paind4 paind5 paind6 paind7 paind8 paind9 paind10
$frame paind11 paind12 paind13 paind14 paind15 paind16

$frame paine1 paine2 paine3 paine4 paine5 paine6 paine7 paine8 paine9 paine10
$frame paine11 paine12 paine13 paine14 paine15

$frame death1 death2 death3 death4 death5 death6
$frame death7 death8 death9 death10 death11 death12
$frame death13 death14

$frame bdeath1 bdeath2 bdeath3 bdeath4 bdeath5 bdeath6
$frame bdeath7 bdeath8 bdeath9 bdeath10

$frame pull1 pull2 pull3 pull4 pull5 pull6 pull7 pull8 pull9 pull10 pull11


//======================================================================
void() ogre_stand1    =[    $stand1,    ogre_stand2    ] {ai_stand();};
void() ogre_stand2    =[    $stand2,    ogre_stand3    ] {ai_stand();};
void() ogre_stand3    =[    $stand3,    ogre_stand4    ] {ai_stand();};
void() ogre_stand4    =[    $stand4,    ogre_stand5    ] {ai_stand();};
void() ogre_stand5    =[    $stand5,    ogre_stand6    ] {monster_idle_sound(); ai_stand();};
void() ogre_stand6    =[    $stand6,    ogre_stand7    ] {ai_stand();};
void() ogre_stand7    =[    $stand7,    ogre_stand8    ] {ai_stand();};
void() ogre_stand8    =[    $stand8,    ogre_stand9    ] {ai_stand();};
void() ogre_stand9    =[    $stand9,    ogre_stand1    ] {ai_stand();};

//======================================================================
void() ogre_walk1    =[    $walk1,        ogre_walk2    ] {monster_footstep(FALSE); ai_walk(3);};
void() ogre_walk2    =[    $walk2,        ogre_walk3    ] {ai_walk(2);};
void() ogre_walk3    =[    $walk3,        ogre_walk4    ] {monster_idle_sound(); ai_walk(2);};
void() ogre_walk4    =[    $walk4,        ogre_walk5    ] {ai_walk(2);};
void() ogre_walk5    =[    $walk5,        ogre_walk6    ] {ai_walk(2);};
void() ogre_walk6    =[    $walk6,        ogre_walk7    ] {ai_walk(5);
if (random() < 0.1) sound (self, CHAN_VOICE, "ogre/ogdrag.wav", 1, ATTN_IDLE);};
void() ogre_walk7    =[    $walk7,        ogre_walk8    ] {ai_walk(3);};
void() ogre_walk8    =[    $walk8,        ogre_walk9    ] {ai_walk(2);};
void() ogre_walk9    =[    $walk9,        ogre_walk10    ] {monster_footstep(FALSE); ai_walk(3);};
void() ogre_walk10    =[    $walk10,    ogre_walk11    ] {ai_walk(1);};
void() ogre_walk11    =[    $walk11,    ogre_walk12    ] {ai_walk(2);};
void() ogre_walk12    =[    $walk12,    ogre_walk13    ] {ai_walk(3);};
void() ogre_walk13    =[    $walk13,    ogre_walk14    ] {ai_walk(3);};
void() ogre_walk14    =[    $walk14,    ogre_walk15    ] {ai_walk(3);};
void() ogre_walk15    =[    $walk15,    ogre_walk16    ] {ai_walk(3);};
void() ogre_walk16    =[    $walk16,    ogre_walk1    ] {ai_walk(4);};

//======================================================================
void() ogre_run1    =[    $run1,        ogre_run2    ] {monster_idle_sound(); ai_run(9);};
void() ogre_run2    =[    $run2,        ogre_run3    ] {monster_footstep(FALSE);ai_run(12);};
void() ogre_run3    =[    $run3,        ogre_run4    ] {ai_run(8);};
void() ogre_run4    =[    $run4,        ogre_run5    ] {ai_run(22);};
void() ogre_run5    =[    $run5,        ogre_run6    ] {ai_run(16);};
void() ogre_run6    =[    $run6,        ogre_run7    ] {monster_footstep(FALSE);ai_run(4);};
void() ogre_run7    =[    $run7,        ogre_run8    ] {ai_run(13);};
void() ogre_run8    =[    $run8,        ogre_run1    ] {ai_run(24);};

/*======================================================================
 MELEE - Chainsaw
======================================================================*/
void(float side, float dmgmultipler) chainsaw =
{
    local float ldmg;
    
    if (!self.enemy) return;
    if (self.health < 1) return;

    ai_damagebreakable(20);                            // Damage any breakables
    if (!ai_checkmelee(self.meleerange)) return;    // Too far away

    // Can the target bleed? - generate blood
    if (!CanDamage (self.enemy, self)) return;
    
    // Damage 1-12
    ldmg = (random() + random() + random()) * (4*dmgmultipler);
    if (ldmg < 1) ldmg = 1;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);

    // Check for poisonous attribute (new poison version)
    if (self.poisonous) PoisonDeBuff(self.enemy);

    SpawnMeatSpray (self, self.enemy, side);
};

//----------------------------------------------------------------------
void() ogre_swing1    =[    $swing1,        ogre_swing2    ] {ai_charge(11);};
void() ogre_swing2    =[    $swing2,        ogre_swing3    ] {ai_charge(1);};
void() ogre_swing3    =[    $swing3,        ogre_swing4    ] {ai_charge(4);};
void() ogre_swing4    =[    $swing4,        ogre_swing5    ] {ai_charge(13);};
void() ogre_swing5    =[    $swing5,        ogre_swing6    ] {monster_footstep(FALSE);
    ai_charge(9); chainsaw(0,1);self.angles_y = self.angles_y + random()*25;};
void() ogre_swing6    =[    $swing6,        ogre_swing7    ] {chainsaw(200,1);self.angles_y = self.angles_y + random()* 25;};
void() ogre_swing7    =[    $swing7,        ogre_swing8    ] {chainsaw(0,1);self.angles_y = self.angles_y + random()* 25;};
void() ogre_swing8    =[    $swing8,        ogre_swing9    ] {chainsaw(0,1);self.angles_y = self.angles_y + random()* 25;};
void() ogre_swing9    =[    $swing9,        ogre_swing10 ] {chainsaw(0,1);self.angles_y = self.angles_y + random()* 25;};
void() ogre_swing10    =[    $swing10,        ogre_swing11 ] {chainsaw(-200,1);self.angles_y = self.angles_y + random()* 25;};
void() ogre_swing11    =[    $swing11,        ogre_swing12 ] {chainsaw(0,1);self.angles_y = self.angles_y + random()* 25;};
void() ogre_swing12    =[    $swing12,        ogre_swing13 ] {ai_charge(3);};
void() ogre_swing13    =[    $swing13,        ogre_swing14 ] {ai_charge(8);};
void() ogre_swing14    =[    $swing14,        ogre_run1    ] {ai_charge(9);};

//----------------------------------------------------------------------
void() ogre_smash1    =[    $smash1,        ogre_smash2    ] {ai_charge(6);};
void() ogre_smash2    =[    $smash2,        ogre_smash3    ] {ai_charge(0);};
void() ogre_smash3    =[    $smash3,        ogre_smash4    ] {ai_charge(0);};
void() ogre_smash4    =[    $smash4,        ogre_smash5    ] {ai_charge(1);};
void() ogre_smash5    =[    $smash5,        ogre_smash6    ] {ai_charge(4);};
void() ogre_smash6    =[    $smash6,        ogre_smash7    ] {ai_charge(4); chainsaw(0,1);};
void() ogre_smash7    =[    $smash7,        ogre_smash8    ] {ai_charge(4); chainsaw(0,1);};
void() ogre_smash8    =[    $smash8,        ogre_smash9    ] {ai_charge(10); chainsaw(0,1);};
void() ogre_smash9    =[    $smash9,        ogre_smash10 ] {monster_footstep(FALSE); ai_charge(13); chainsaw(0,1);};
void() ogre_smash10    =[    $smash10,        ogre_smash11 ] {chainsaw(1,1);};
void() ogre_smash11    =[    $smash11,        ogre_smash12 ] {ai_charge(2); chainsaw(0,1);
self.nextthink = self.nextthink + random()*0.2;};    // slight variation
void() ogre_smash12    =[    $smash12,        ogre_smash13 ] {ai_charge(0);};
void() ogre_smash13    =[    $smash13,        ogre_smash14 ] {ai_charge(4);};
void() ogre_smash14    =[    $smash14,        ogre_run1    ] {ai_charge(12);};

//----------------------------------------------------------------------
void() ogre_melee =
{
    sound (self, CHAN_WEAPON, "ogre/ogsawatk.wav", 1, ATTN_NORM);
    if (random() > 0.5) ogre_smash1 ();
    else ogre_swing1 ();
};

/*======================================================================
 Range - Grenades (uses generic attack function in projectiles.qc)
 Nail - Alternative firemode (based on nail ogre from Marcher Fortress)
======================================================================*/
void() OgreFireNails =
{
    local vector org, dir, vec;
    local float loopvar;
    
    // Check for death?
    if (self.health < 1) return;
    loopvar = 0;

    // Light up face, setup projectile speed and play fire sound
    self.effects = self.effects | EF_MUZZLEFLASH;
    self.attack_speed = SPEED_NAILOGREPROJ + (skill * SPEED_NAILOGRESKILL);
    sound(self, CHAN_WEAPON, "ogre/nail_fire.wav", 1, ATTN_NORM);

    // turn and face your enemy!
    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);
    
    // Loop through the nail burst (all at once)
    while (loopvar < self.attack_count) {
        // Random circle spread pattern
        vec = (crandom()*20)*v_right + (crandom()*20)*v_up;
        dir = normalize((self.enemy.origin + vec) - org);
        // Generate a red hot nail projectile
        launch_projectile(org, dir, CT_PROJ_MONNG, self.attack_speed);
        loopvar = loopvar + 1;
    }
};

//----------------------------------------------------------------------
void() ogre_shoot1 =[ $shoot1, ogre_shoot2 ] {
    ai_face();
    MonsterGrenadeSound(); 
    self.attack_speed = MonsterGrenadeSpeed();
    self.attack_elev = SUB_Elevation(ELEV_DEFAULT, self.origin, self.enemy.origin, self.attack_speed);
};
void() ogre_shoot2 =[ $shoot2, ogre_shoot3 ] {
    ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, self.attack_speed);
};
void() ogre_shoot3 =[ $shoot2, ogre_shoot4 ] {
    ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, self.attack_speed);
};
void() ogre_shoot4 =[ $shoot3, ogre_shoot5 ] {
    ai_face();
    MonsterFireGrenade(self.origin, self.enemy.origin);
};
void() ogre_shoot5 =[ $shoot4, ogre_shoot6 ] {};
void() ogre_shoot6 =[ $shoot5, ogre_shoot7 ] {};
void() ogre_shoot7 =[ $shoot6, ogre_run1 ] {ai_face();};

//----------------------------------------------------------------------
void() ogre_nail1 =[ $shoot1, ogre_nail2 ] {ai_face();};
void() ogre_nail2 =[ $shoot2, ogre_nail3 ] {ai_face();};
void() ogre_nail3 =[ $shoot2, ogre_nail4 ] {ai_face();};
void() ogre_nail4 =[ $shoot3, ogre_nail5 ] {ai_face();OgreFireNails();};
void() ogre_nail5 =[ $shoot4, ogre_nail6 ] {};
void() ogre_nail6 =[ $shoot5, ogre_nail7 ] {};
void() ogre_nail7 =[ $shoot6, ogre_run1  ] {ai_face();};

//======================================================================
// Lots of different pain animations (falling over is a classic
//======================================================================
void()    ogre_pain1    =[    $pain1,        ogre_pain2    ] {};
void()    ogre_pain2    =[    $pain2,        ogre_pain3    ] {};
void()    ogre_pain3    =[    $pain3,        ogre_pain4    ] {};
void()    ogre_pain4    =[    $pain4,        ogre_pain5    ] {};
void()    ogre_pain5    =[    $pain5,        ogre_run1    ] {};

//----------------------------------------------------------------------
void()    ogre_painb1    =[    $painb1,    ogre_painb2    ] {};
void()    ogre_painb2    =[    $painb2,    ogre_painb3    ] {};
void()    ogre_painb3    =[    $painb3,    ogre_run1    ] {};

//----------------------------------------------------------------------
void()    ogre_painc1    =[    $painc1,    ogre_painc2    ] {};
void()    ogre_painc2    =[    $painc2,    ogre_painc3    ] {};
void()    ogre_painc3    =[    $painc3,    ogre_painc4    ] {};
void()    ogre_painc4    =[    $painc4,    ogre_painc5    ] {};
void()    ogre_painc5    =[    $painc5,    ogre_painc6    ] {};
void()    ogre_painc6    =[    $painc6,    ogre_run1    ] {};

//----------------------------------------------------------------------
void()    ogre_paind1    =[    $paind1,    ogre_paind2    ] {};
void()    ogre_paind2    =[    $paind2,    ogre_paind3    ] {ai_pain(10);};
void()    ogre_paind3    =[    $paind3,    ogre_paind4    ] {ai_pain(9);};
void()    ogre_paind4    =[    $paind4,    ogre_paind5    ] {monster_footstep(FALSE);ai_pain(4);};
void()    ogre_paind5    =[    $paind5,    ogre_paind6    ] {};
void()    ogre_paind6    =[    $paind6,    ogre_paind7    ] {};
void()    ogre_paind7    =[    $paind7,    ogre_paind8    ] {};
void()    ogre_paind8    =[    $paind8,    ogre_paind9    ] {};
void()    ogre_paind9    =[    $paind9,    ogre_paind10    ] {};
void()    ogre_paind10=[    $paind10,    ogre_paind11    ] {monster_footstep(FALSE);};
void()    ogre_paind11=[    $paind11,    ogre_paind12    ] {};
void()    ogre_paind12=[    $paind12,    ogre_paind13    ] {monster_footstep(FALSE);};
void()    ogre_paind13=[    $paind13,    ogre_paind14    ] {};
void()    ogre_paind14=[    $paind14,    ogre_paind15    ] {};
void()    ogre_paind15=[    $paind15,    ogre_paind16    ] {};
void()    ogre_paind16=[    $paind16,    ogre_run1    ] {};

//----------------------------------------------------------------------
void()    ogre_paine1    =[    $paine1,    ogre_paine2    ] {};
void()    ogre_paine2    =[    $paine2,    ogre_paine3    ] {ai_pain(10);};
void()    ogre_paine3    =[    $paine3,    ogre_paine4    ] {monster_footstep(FALSE);ai_pain(9);};
void()    ogre_paine4    =[    $paine4,    ogre_paine5    ] {ai_pain(4);};
void()    ogre_paine5    =[    $paine5,    ogre_paine6    ] {};
void()    ogre_paine6    =[    $paine6,    ogre_paine7    ] {};
void()    ogre_paine7    =[    $paine7,    ogre_paine8    ] {};
void()    ogre_paine8    =[    $paine8,    ogre_paine9    ] {};
void()    ogre_paine9    =[    $paine9,    ogre_paine10    ] {};
void()    ogre_paine10=[    $paine10,    ogre_paine11    ] {};
void()    ogre_paine11=[    $paine11,    ogre_paine12    ] {};
void()    ogre_paine12=[    $paine12,    ogre_paine13    ] {};
void()    ogre_paine13=[    $paine13,    ogre_paine14    ] {};
void()    ogre_paine14=[    $paine14,    ogre_paine15    ] {};
void()    ogre_paine15=[    $paine15,    ogre_run1    ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) ogre_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
    
        if (self.pain_check == 1) {
            // Randomly pick which pain animation to play
            self.lip = random();
            if (self.lip < 0.25) ogre_pain1 ();
            else if (self.lip < 0.5) ogre_painb1 ();
            else if (self.lip < 0.75) ogre_painc1 ();
            else if (self.lip < 0.88) {
                // Fall down to the ground
                ogre_paind1 ();
                self.pain_finished = time + 2;
            }
            else {
                // Stagger backwards
                ogre_paine1 ();
                self.pain_finished = time + 2;
            }
        }
        if (self.pain_check == 2) {
            self.axhitme = 0;
            if (random() < 0.5) {
                self.pain_finished = time + 1.6;
                ogre_paind1 ();    // Fall to ground
            }
            else {
                self.pain_finished = time + 1.5;
                ogre_paine1 ();    // Stagger backwards
            }
        }
    }
};

//======================================================================
// DIE Ogre DIE!?!
//======================================================================
void()    ogre_die1    =[    $death1,    ogre_die2    ] {};
void()    ogre_die2    =[    $death2,    ogre_die3    ] {monster_check_gib();};
void()    ogre_die3    =[    $death3,    ogre_die4    ] {monster_check_gib();
    self.solid = SOLID_NOT; if (!self.gibbed) DropBackpack();};
void()    ogre_die4    =[    $death4,    ogre_die5    ] {};
void()    ogre_die5    =[    $death5,    ogre_die6    ] {};
void()    ogre_die6    =[    $death6,    ogre_die7    ] {};
void()    ogre_die7    =[    $death7,    ogre_die8    ] {};
void()    ogre_die8    =[    $death8,    ogre_die9    ] {};
void()    ogre_die9    =[    $death9,    ogre_die10    ] {};
void()    ogre_die10    =[    $death10,    ogre_die11    ] {};
void()    ogre_die11    =[    $death11,    ogre_die12    ] {};
void()    ogre_die12    =[    $death12,    ogre_die13    ] {};
void()    ogre_die13    =[    $death13,    ogre_die14    ] {monster_death_postcheck();};
void()    ogre_die14    =[    $death14,    ogre_die14    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void()    ogre_bdie1    =[    $bdeath1,    ogre_bdie2    ] {};
void()    ogre_bdie2    =[    $bdeath2,    ogre_bdie3    ] {monster_check_gib();ai_forward(5);};
void()    ogre_bdie3    =[    $bdeath3,    ogre_bdie4    ] {monster_check_gib();
    self.solid = SOLID_NOT; if (!self.gibbed) DropBackpack();};
void()    ogre_bdie4    =[    $bdeath4,    ogre_bdie5    ] {ai_forward(1);};
void()    ogre_bdie5    =[    $bdeath5,    ogre_bdie6    ] {ai_forward(3);};
void()    ogre_bdie6    =[    $bdeath6,    ogre_bdie7    ] {ai_forward(7);};
void()    ogre_bdie7    =[    $bdeath7,    ogre_bdie8    ] {ai_forward(25);};
void()    ogre_bdie8    =[    $bdeath8,    ogre_bdie9    ] {};
void()    ogre_bdie9    =[    $bdeath9,    ogre_bdie10    ] {monster_death_postcheck();};
void()    ogre_bdie10    =[    $bdeath10,    ogre_bdie10    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() ogre_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "ogre/ogdth.wav", 1, ATTN_NORM);
        self.lip = random();
        if (self.lip < 0.5) ogre_die1();
        else ogre_bdie1();
    }
};

//----------------------------------------------------------------------
// Special wakeup animation for attacking/breaking something infront
// The monster should be setup facing the right direction before call
// Starts in large overhead swing downwards (no damage checks)
//----------------------------------------------------------------------
void() ogre_wakeup1    =[    $smash7,    ogre_wakeup2] {monster_sightsound();};
void() ogre_wakeup2    =[    $smash6,    ogre_wakeup3] {};
void() ogre_wakeup3    =[    $smash8,    ogre_wakeup4] {};
void() ogre_wakeup4    =[    $smash9,    ogre_wakeup5 ] {};
void() ogre_wakeup5    =[    $smash10,    ogre_wakeup6 ] {};
void() ogre_wakeup6    =[    $smash11,    ogre_wakeup7 ] {};
void() ogre_wakeup7    =[    $smash12,    ogre_wakeup8 ] {};
void() ogre_wakeup8    =[    $smash13,    ogre_wakeup9 ] {};
void() ogre_wakeup9    =[    $smash14,    ogre_run1    ] {};

//======================================================================
// From the crazy mind of Madfox, comes fishing ogre!?!
//======================================================================
float OGREFISH_FISHY = 0;
float OGREFISH_FISHING = 19;
float OGREFISH_CATCH = 79;
float ANIMATION_OGREFLAP = 18;
float ANIMATION_OGREFISH = 59;
float ANIMATION_OGRECATCH = 25;

//----------------------------------------------------------------------
// Special idle animation - Flappy the fish waggles tail
//----------------------------------------------------------------------
void() ogre_flappyfish =
{
    // If dead, no more updates
    if (self.health < 1) return;

    self.nextthink = time + 0.1;
    self.frame = OGREFISH_FISHY + self.walkframe;

    // Check for special frame conditions
    if (self.walkframe == 0)
        sound (self, CHAN_VOICE, "ogre/ogrefish_flappy.wav", 1, ATTN_NORM);
    
    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > ANIMATION_OGREFLAP) {
        self.walkframe = 0;
        self.think = self.th_altstand;
    }
    else self.think = ogre_flappyfish;
};

//----------------------------------------------------------------------
// Special idle animation - Ogre finally catches something!
//----------------------------------------------------------------------
void() ogre_catchfish =
{
    // If dead, no more updates
    if (self.health < 1) return;

    self.nextthink = time + 0.1;
    self.frame = OGREFISH_CATCH + self.walkframe;

    // Check for special frame conditions
    if (self.walkframe == 6)
        sound (self, CHAN_VOICE, "ogre/ogrefish_out.wav", 1, ATTN_NORM);
    if (self.walkframe == 24)
        sound (self, CHAN_VOICE, "ogre/ogrefish_in.wav", 1, ATTN_NORM);
    
    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > ANIMATION_OGRECATCH) {
        self.walkframe = 0;
        self.think = self.th_altstand;
    }
    else self.think = ogre_catchfish;
};

//----------------------------------------------------------------------
void() ogre_fishing =
{
    // If dead, no more updates
    if (self.health < 1) return;

    self.idlebusy = FALSE;
    self.nextthink = time + 0.1;
    self.frame = OGREFISH_FISHING + self.walkframe;

    // Check for fishing idle animations
    if (self.walkframe == 0) {
        self.lip = random();
        // Finally caught a fish?
        if (self.lip < 0.1) {
            self.think = ogre_catchfish;
            self.idlebusy = TRUE;
        }
        // Flappy the fish gasping for air
        else if (self.lip < 0.2) {
            self.think = ogre_flappyfish;
            self.idlebusy = TRUE;
        }
        // Classic ogre idle + water
        else if (self.lip < 0.3) 
            sound (self, CHAN_VOICE, "ogre/ogrefish_idle.wav", 1, ATTN_NORM);
    }
    
    // Ghost version updates alpha and spawns particles
    if (self.spawnflags & MON_GHOST_ONLY) {
        // Is ogre setup invisible?
        if (self.alpha < 1) {
            // Wait for the ogre to get into position
            if (self.respawn_time < time) {
                // exponential fade in model alpha 
                self.alpha = self.alpha + self.lefty;
                self.lefty = self.lefty + self.lefty;
                if (self.alpha >= 1) self.alpha = 1;
            }
        }
        else {
            // Model is all screwed up, with loads of extra surfaces
            // cannot do alpha sparkle because of back surface problem
            //self.alpha = 0.5 + (0.1*random());
            self.pos1 = '0 0 0';
            self.pos1_x = crandom()*16;
            self.pos1_y = crandom()*16;
            self.pos1_z = crandom()*24;
            if (random() < 0.5) 
                particle_explode(self.origin+self.pos1, 2, 1, PARTICLE_BURST_WHITE, PARTICLE_BURST_LOSTUP);
        }
    }

    if (self.idlebusy == FALSE) {
        // Move frame forward, check for conditions
        self.walkframe = self.walkframe + 1;
        if (self.walkframe > ANIMATION_OGREFISH) self.walkframe = 0;
        self.think = ogre_fishing;
    }
};

//----------------------------------------------------------------------
// Re-using fall down pain animation
//----------------------------------------------------------------------
void()    ogre_fishd6    =[ $paind6,    ogre_fishd7    ] {};
void()    ogre_fishd7    =[ $paind7,    ogre_fishd8    ] {};
void()    ogre_fishd8    =[ $paind8,    ogre_fishd9    ] {};
void()    ogre_fishd9    =[ $paind9,    ogre_fishd10 ] {};
void()    ogre_fishd10=[ $paind10, ogre_fishd11 ] {monster_footstep(FALSE);};
void()    ogre_fishd11=[ $paind11, ogre_fishd12 ] {};
void()    ogre_fishd12=[ $paind12, ogre_fishd13 ] {monster_footstep(FALSE);};
void()    ogre_fishd13=[ $paind13, ogre_fishd14 ] {};
void()    ogre_fishd14=[ $paind14, ogre_fishd15 ] {};
void()    ogre_fishd15=[ $paind15, ogre_fishd16 ] {};
void()    ogre_fishd16=[ $paind16, ogre_run1 ] {
    // Restore pain/damage functionality
    self.flags = self.flags | FL_MONSTER;
    self.health = self.max_health;
    
    // Fishing ogre is wearing a grenade bounce shirt!
    self.takedamage = DAMAGE_YES;
    self.noradiusdmg = FALSE;
    self.use = monster_use;
    
    // Does not feel much pain and attacks more often
    self.pain_flinch = 200;
    self.pain_timeout = 3;
    
    // Restore animation/game logic
    self.th_stand = ogre_stand1;
    self.th_walk = ogre_walk1;
    self.th_run = ogre_run1;
    self.th_melee = ogre_melee;
    self.th_missile = ogre_shoot1;
    self.th_pain = ogre_pain;
    self.th_die = ogre_die;
};

//----------------------------------------------------------------------
void() ogre_fishwakeup = {
    self.use = SUB_Null;    // Stop re-triggering
    monster_sightsound();    // Classic wakeup sound

    // Make sure no more pain or damage
    self.takedamage = DAMAGE_NO;
    self.pain_finished = time + 2;
    
    // Check for model in original ogre position
    // This is to save the space for the returning ogre
    if (self.attachment) {
        setmodel(self.attachment, "");
        self.attachment.solid = SOLID_NOT;
        self.attachment.movetype = MOVETYPE_NONE;
    }
    
    // Check for any wakeup triggers
    if (self.target2 != "") {
        trigger_strs(self.target2, self);
        self.target2 = "";
    }
    
    // Shift ogre into pain (on floor) state
    setmodel(self, self.mdl);
    self.frame = $paind6;    // Has to be set after model change
    setorigin(self, self.pos1);
    setsize (self, self.bbmins, self.bbmaxs);
    ogre_fishd6();
};

//----------------------------------------------------------------------
void(entity inf, entity att, float dam) ogre_fishpain = {ogre_fishwakeup();};
void() ogre_fishdie = {};

//----------------------------------------------------------------------
void() ogre_fishsetup =
{
    // Setup ghost version (no pain or death)
    if (self.spawnflags & MON_GHOST_ONLY) {
        self.respawn_time = time + 0.3;    // Wait to get into position
        self.lefty = 0.01;                // Exponential increase
        self.solid = SOLID_NOT;            // No interaction with world
        self.movetype = MOVETYPE_NONE;    // Static item, no movement
        self.takedamage = DAMAGE_NO;
        self.noradiusdmg = TRUE;
        self.gibhealth = MON_NEVERGIB;
        self.health = LARGE_TIMER;
        self.use = SUB_Null;
    }
    else {
        // Take direct damage only and grenades bounce!
        self.takedamage = DAMAGE_YES;
        self.noradiusdmg = TRUE;
        self.health = MEGADEATH;
        self.use = ogre_fishwakeup;
        
        // Create a temporary entity to fill up space behind ogre
        // Prevent anthing getting in the way when ogre wakes up
        if (!self.attachment) {
            self.attachment = spawn();
            setmodel(self.attachment, MODEL_EMPTY);
            setorigin(self.attachment, self.origin);
            setsize (self.attachment, self.bbmins, self.bbmaxs);
            self.attachment.solid = SOLID_BBOX;
            self.attachment.movetype = MOVETYPE_STEP;
        }
    }
    
    setmodel(self, self.weaponglow);    // Setup fishing model pose
    self.pos1 = self.origin;            // Save current origin for later
    if (self.spawnflags & MON_GHOST_ONLY) self.skin = 2;
    
    // Move ogre into fishing animation position
    makevectors(self.angles);
    self.pos2 = self.origin + attack_vector('36 0 -24');
    setorigin(self, self.pos2);
    // Got to reset bounding box after model change
    setsize (self, self.bbmins, self.bbmaxs);
    self.walkframe = 0;
    ogre_fishing();
};

/*======================================================================
 QUAKED monster_ogre (1 0 0) (-32 -32 -24) (32 32 64) Ambush
======================================================================*/
void() monster_ogre_precache =
{
    self.mdl = "progs/mon_ogre.mdl";        // Rogue Ogre
    self.headmdl = "progs/h_ogre.mdl";
    self.gib1mdl = "progs/w_chainsaw.mdl";    // Chainsaw
    self.gib2mdl = "progs/gib_ogfoot1.mdl";    // Left foot
    self.gib3mdl = "progs/gib_ogfoot2.mdl";    // Right foot
    self.gib4mdl = "progs/w_ogregl.mdl";    // Grenade Launcher

    precache_model (self.mdl);
    precache_model (MODEL_PROJ_GRENADE);
    precache_model (self.headmdl);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    precache_model (self.gib4mdl);

    // Randomly swap in GL instead of Chainsaw
    self.gib1sound = GIB_IMPACT_WOOD;
    if (random() < 0.5) self.gib1mdl = self.gib4mdl; 
    self.gib4mdl = "";

    self.idle_sound = "ogre/ogidle.wav";
    precache_sound (self.idle_sound);
    self.idle_soundcom = "ogre/ogidle2.wav";
    precache_sound (self.idle_soundcom);

    precache_sound ("ogre/ogdth.wav");
    self.pain_sound = "ogre/ogpain1.wav";
    precache_sound (self.pain_sound);
    
    precache_sound ("ogre/ogdrag.wav");
    precache_sound ("ogre/ogsawatk.wav");

    self.sight_sound = "ogre/ogwake.wav";
    precache_sound (self.sight_sound);

    // Load additional models/sounds for nail ogre
    if (self.spawnflags & MON_OGRE_NAIL) {
        // Default attack - red glowing nails
        precache_model (MODEL_PROJ_NGRED);
        precache_sound("ogre/nail_fire.wav");
        // Default amount of nails to burst fire (5x9=45dmg)
        if (self.attack_count < 1) self.attack_count = 5;
    }
    
    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
        precache_model (MODEL_PROJ_GRENADEGRN);
    }
};

//----------------------------------------------------------------------
void() monster_ogre =
{
    if (deathmatch) { remove(self); return; }

    // Precache basic ogre models and sounds
    monster_ogre_precache();

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    self.idmins = VEC_HULL2_MIN;        // -32 -32 -24, 32 32 64
    self.idmaxs = VEC_HULL2_MAX;
    if (self.bboxtype < 1) self.bboxtype = BBOX_WIDE;
    self.gibhealth = -80;
    self.gibbed = FALSE;
    self.pain_flinch = 30;                // Low pain flinch
    self.pain_longanim = TRUE;            // Can be chopped with shadow axe
    self.steptype = FS_TYPESLOW;        // Small sounding feet
    self.meleerange = MONAI_MELEEOGRE;    // Chainsaw range
    self.no_zaware = TRUE;                // Always NOT zware
    self.attack_offset = '24 0 20';        // Run + Gun vector
    if (!self.ammo_rockets) self.ammo_rockets = 2;    // Default ammo drop
    self.deathstring = " was destroyed by an Ogre\n";

    // Standard ogre health 200 with green+ option
    if (self.health < 1) self.health = 200;
    if (self.spawnflags & MON_OGRE_GREEN) {
        self.health = self.health + MON_OGRE_GRNUPG;
        self.skin = 1;
    }
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.th_checkattack = OgreCheckAttack;
    self.th_stand = ogre_stand1;
    self.th_walk = ogre_walk1;
    self.th_run = ogre_run1;
    self.th_melee = ogre_melee;
    self.th_missile = ogre_shoot1;
    self.th_pain = ogre_pain;
    self.th_die = ogre_die;
    self.th_wakeup = ogre_wakeup1;
    
    // Switch attack types
    if (self.spawnflags & MON_OGRE_NAIL) self.th_missile = ogre_nail1;
    else self.th_missile = ogre_shoot1;
        
    self.classtype = CT_MONOGRE;        // Old style ogre
    self.classgroup = CG_OGRE;            // One big happy ogre family!
    self.classmove = MON_MOVEWALK;

    monster_start();
};

//----------------------------------------------------------------------
void() monster_ogre_marksman = { monster_ogre (); };

//----------------------------------------------------------------------
void() monster_ogre_fishing =
{
    if (deathmatch) { remove(self); return; }
        
    // Precache basic ogre models and sounds
    monster_ogre_precache();

    // Extra fishing ogre precache stuff
    self.weaponglow = "progs/mon_ogrefish.mdl";
    precache_model (self.weaponglow);
    precache_sound ("ogre/ogrefish_out.wav");
    precache_sound ("ogre/ogrefish_in.wav");
    precache_sound ("ogre/ogrefish_idle.wav");
    precache_sound ("ogre/ogrefish_flappy.wav");

    self.solid = SOLID_NOT;            // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_WIDE;
    self.gibhealth = -80;
    self.gibbed = FALSE;
    self.pain_flinch = 1;            // Pain is the wakeup trigger
    self.pain_longanim = FALSE;        // Cannot be chopped with shadow axe
    self.steptype = FS_TYPESLOW;    // Small sounding feet
    self.no_zaware = FALSE;            // Fishing ogre has perfect aim!
    self.bouncegrenade = TRUE;        // Grenades bounce off!
    self.meleerange = MONAI_MELEEOGRE;    // Chainsaw range
    self.exactskin = 0;                // Skin defined by spawnflag
    self.noinfighting = TRUE;        // No infighting
    self.pain_ignore = TRUE;        // Will not wake up from monsters
    self.movespeed = 0;                // This is not a turret
    self.frame = OGREFISH_FISHING;    // Start in fishing frame
    self.deathstring = " was reeled in by a Fishing Ogre\n";

    // Remove spawnflag options not needed for this monster
    self.lip = 0;
    // Ghost version has unique skin, cannot have green version as well
    if (self.spawnflags & MON_GHOST_ONLY) self.lip = self.lip | MON_GHOST_ONLY;
    else if (self.spawnflags & MON_OGRE_GREEN) self.lip = self.lip | MON_OGRE_GREEN;
    // Classic check for fx/spawn options
    if (self.spawnflags & MON_SPAWN_NOGFX) self.lip = self.lip | MON_SPAWN_NOGFX;
    if (self.spawnflags & MON_SPAWN_DELAY) self.lip = self.lip | MON_SPAWN_DELAY;
    self.spawnflags = self.lip;
    
    // Special event monster has unique HP setup
    if (self.spawnflags & MON_OGRE_GREEN) {
        self.ammo_rockets = 6;    // Tons of rockets!
        self.health = 350;        // Much stronger
        self.skin = 1;            // Green (Rogue) skin
    }
    else {
        self.ammo_rockets = 4;    // More rockets than fish!
        self.health = 150;        // Lower HP than regular
    }

    // Fishing ogre can never uses nails, grenades only
    self.spawnflags = self.spawnflags - (self.spawnflags & MON_OGRE_NAIL);
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = 1;    // Rocket resistance
    self.resist_cells = 0;

    self.th_checkattack = OgreCheckAttack;
    self.th_stand = self.th_walk = self.th_run = ogre_fishsetup;
    self.th_altstand = ogre_fishing;
    // Setup ghost version (no pain or death)
    if (self.spawnflags & MON_GHOST_ONLY) {
        self.th_pain = SUB_Null_pain;
        self.th_die = SUB_Null;
        self.headmdl = "";
    }
    else {
        self.th_pain = ogre_fishpain;
        self.th_die = ogre_fishdie;
    }

    self.classtype = CT_MONOGREFISH;
    self.classgroup = CG_OGRE;            // One big happy ogre family!
    self.classmove = MON_MOVEWALK;

    monster_start();
}

//----------------------------------------------------------------------
void() monster_ogre_fishing_ghost =
{
    // Special spawnflag (code only)
    self.spawnflags = self.spawnflags | MON_GHOST_ONLY;
    // Always no monster count
    self.nomonstercount = TRUE;
    // Start model invisible (to prevent snapping)
    self.alpha = 0.01;
    monster_ogre_fishing();
};
