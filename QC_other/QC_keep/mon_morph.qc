$skin aztec

$skin pharoah

$skin roman

$skin florskin

$frame mfstand1 
//$frame mfup01 mfup02 mfup03 mfup04 mfup05 mfup06
//$frame mfup07 mfup08 mfup09 mfup10 mfup11 mfup12

$frame start01 start02 start03 start04 start05 start06
$frame start07 start08 start09 start10 start11 start12
$frame start13 start14 start15 start16 start17 start18

$frame start19 start20 start21 start22 start23 start24

$frame start25 start26 start27 start28 start29 start30 start31

$frame mfrun01 mfrun02 mfrun03 mfrun04 mfrun05 mfrun06
$frame mfrun07 mfrun08 mfrun09 mfrun10 mfrun11
$frame mfwlk01 mfwlk02 mfwlk03 mfwlk04 mfwlk05 mfwlk06 mfwlk07 mfwlk08
$frame mfwlk09 mfwlk10 mfwlk11 mfwlk12 mfwlk13

$frame mfata01 mfata02 mfata03 mfata04 mfata05
$frame mfata06 mfata07 mfata08 mfata09

$frame mfatb01 mfatb02 mfatb03 mfatb04 mfatb05 mfatb06

$frame mfatb07 mfatb08 mfatb09 mfatb10 mfatb11

$frame mfatc01 mfatc02 mfatc03 mfatc04 mfatc05 mfatc06

$frame mfatc07 mfatc08 mfatc09 mfatc10 mfatc11 mfatc12
$frame mfatc13 mfatc14 mfatc15 mfatc16

$frame mfknck01 mfknck02 mfknck03 mfknck04 mfknck05 mfknck06
$frame mfknck07 mfknck08 mfknck09 mfknck10 mfknck11 mfknck12

$frame mfpain01 mfpain02 mfpain03 mfpain04 mfpain05
$frame mfpain06 mfpain07 mfpain08 mfpain09 mfpain10
$frame mfpbin01 mfpbin02 mfpbin03 mfpbin04 mfpbin05
$frame mfpbin06 mfpbin07
$frame mfdth01 mfdth02 mfdth03 mfdth04 mfdth05
$frame mfdth06 mfdth07 mfdth08 mfdth09 mfdth10
$frame mfdth11 mfdth12 mfdth13 mfdth14 mfdth15
$frame mfdth16 mfdth17 mfdth18 mfdth19 mfdth20
$frame mfdth21

float MORPH_AZTEC = 2;

float MORPH_EGYPT = 4;

float MORPH_GREEK = 8;


void() morph_minionspawn;

void() morph_use;
void() morph_phased;

entity () morph_find_dest = {
   local entity furthestDest;
   local entity bestDest;
   local entity curDest;
   local entity thePlayer;
   local float theDist;
   local float curDist;
   local float dot;
   local vector curVector;

   furthestDest = world;
   bestDest = world;
   theDist = FALSE;
   thePlayer = find (world,classname,"player");
   curDest = world;
   makevectors (thePlayer.angles);
   while ( TRUE ) {
      curDest = find (curDest,classname,"info_morph_destination");
      if ( (curDest == world) ) {
         if ( (bestDest != world) )
            return ( bestDest );

         return ( furthestDest );
      }
      if ( isSpawnPointEmpty (curDest) ) {
         curVector = (curDest.origin - thePlayer.origin);
         curDist = vlen (curVector);
         curVector = normalize (curVector);
         dot = (curVector * v_forward);
         if ( (dot > 0.600) ) {
            if ( (curDist > 150.000) )
               bestDest = curDest;
         }
         if ( (curDist > theDist) ) {
            furthestDest = curDest;
            theDist = curDist;
         }
      }
   }
   return ( world );
};

void() morph_stand1 = [ $mfstand1, morph_stand1 ] { ai_stand(); };


void() morph_walk1  = [ $mfwlk01, morph_walk2  ] { ai_walk(3); };
void() morph_walk2  = [ $mfwlk02, morph_walk3  ] { ai_walk(2); };
void() morph_walk3  = [ $mfwlk03, morph_walk4  ] { ai_walk(4); };
void() morph_walk4  = [ $mfwlk04, morph_walk5  ] { ai_walk(5); };
void() morph_walk5  = [ $mfwlk05, morph_walk6  ] { ai_walk(5); };
void() morph_walk6  = [ $mfwlk06, morph_walk7  ] { ai_walk(4); };

void() morph_walk7  = [ $mfwlk07, morph_walk8  ] { ai_walk(2); };
void() morph_walk8  = [ $mfwlk08, morph_walk9  ] { ai_walk(3); };
void() morph_walk9  = [ $mfwlk09, morph_walk10 ] { ai_walk(4); };
void() morph_walk10 = [ $mfwlk10, morph_walk11 ] { ai_walk(5); };
void() morph_walk11 = [ $mfwlk11, morph_walk12 ] { ai_walk(5); };
void() morph_walk12 = [ $mfwlk12, morph_walk13 ] { ai_walk(4); };
void() morph_walk13 = [ $mfwlk13, morph_walk1  ] { ai_walk(3); };

void() morph_run1 = [ $mfrun01, morph_run2 ] { ai_run( 7); };
void() morph_run2 = [ $mfrun02, morph_run3 ] { ai_run(11); };
void() morph_run3 = [ $mfrun03, morph_run4 ] { ai_run(16); };
void() morph_run4 = [ $mfrun04, morph_run5 ] { ai_run(16); };
void() morph_run5 = [ $mfrun05, morph_run6 ] { ai_run(11); };
void() morph_run6 = [ $mfrun06, morph_run7 ] { ai_run( 7); };

void() morph_run7 = [ $mfrun07, morph_run8 ] { ai_run(11); };
void() morph_run8 = [ $mfrun08, morph_run9 ] { ai_run(15); };

void() morph_run9 = [ $mfrun09, morph_run10 ] { ai_run(19); };
void() morph_run10 =[ $mfrun10, morph_run11 ] { ai_run(15); };

void() morph_run11 =[ $mfrun11, morph_run1 ] { ai_run(11); };

void() morph_stab2 = {
    local float dist;
    local float attackDamage;
    local vector org, dir;

    
if (!self.enemy) return;
    if (!CanDamage (self.enemy, self)) return;

    ai_face();
    dir = self.enemy.origin - self.origin + self.enemy.view_ofs;
    dist = vlen ( dir );
    dir = normalize ( dir );
    makevectors (self.angles);

    if ( dist <= 90 ) {
        sound (self, CHAN_WEAPON, "enforcer/enfstop.wav", 1, ATTN_STATIC);
           attackDamage = (random() * 10) + 20;
        T_Damage (self.enemy, self, self, attackDamage,DAMARMOR);
        SpawnBlood (self.enemy, self.enemy.origin, v_forward * 150, 14);
    } else {
        self.effects = self.effects | EF_MUZZLEFLASH;
        org = self.origin + v_forward * 80 + v_right * 4 + '0 0 4';
        launch_projectile(org, dir, CT_PROJ_LASER, SPEED_LASER);
        if ( dist ) {
            launch_projectile(org, (dir + v_right * 0.04), CT_PROJ_LASER, SPEED_LASER);
            launch_projectile(org, (dir + v_right * 0.04), CT_PROJ_LASER, SPEED_LASER);

        } else {
            launch_projectile(org, (dir + v_right * 0.10), CT_PROJ_LASER, SPEED_LASER);
            launch_projectile(org, (dir + v_right * 0.10), CT_PROJ_LASER, SPEED_LASER);
        }

    }
};


void() morph_attack01 = [ $mfatb01, morph_attack02 ] {ai_face();};
void() morph_attack02 = [ $mfatb02, morph_attack03 ] {ai_face();};
void() morph_attack03 = [ $mfatb03, morph_attack04 ] {ai_face();};
void() morph_attack04 = [ $mfatb04, morph_attack05 ] {ai_face();};
void() morph_attack05 = [ $mfatb05, morph_attack06 ] {ai_face();};
void() morph_attack06 = [ $mfatb06, morph_attack07 ] {ai_face();};
void() morph_attack07 = [ $mfatb07, morph_attack08 ] {morph_stab2();};
void() morph_attack08 = [ $mfatb08, morph_attack09 ] {ai_face();};
void() morph_attack09 = [ $mfatb09, morph_attack10 ] {ai_face();};
void() morph_attack10 = [ $mfatb10, morph_attack11 ] {ai_face();};
void() morph_attack11 = [ $mfatb11, morph_attack12 ] {ai_face();};
void() morph_attack12 = [ $mfatb01, morph_run1     ] {ai_face();};

void() morph_bigattack01 = [ $mfatc01, morph_bigattack02 ] { ai_face(); };
void() morph_bigattack02 = [ $mfatc02, morph_bigattack03 ] { ai_face(); };

void() morph_bigattack03 = [ $mfatc03, morph_bigattack04 ] { ai_face(); };

void() morph_bigattack04 = [ $mfatc04, morph_bigattack05 ] { ai_face(); };

void() morph_bigattack05 = [ $mfatc05, morph_bigattack06 ] { ai_face(); };

void() morph_bigattack06 = [ $mfatc06, morph_bigattack07 ] { ai_face(); };
void() morph_bigattack07 = [ $mfatc07, morph_bigattack08 ] { ai_face(); };

void() morph_bigattack08 = [ $mfatc08, morph_bigattack09 ] { ai_face(); };
void() morph_bigattack09 = [ $mfatc09, morph_bigattack10 ] { ai_face(); };

void() morph_bigattack10 = [ $mfatc10, morph_bigattack11 ] { morph_stab2(); };
void() morph_bigattack11 = [ $mfatc11, morph_bigattack12 ] { ai_face(); };
void() morph_bigattack12 = [ $mfatc12, morph_bigattack13 ] { ai_face(); };

void() morph_bigattack13 = [ $mfatc13, morph_bigattack14 ] { ai_face(); };

void() morph_bigattack14 = [ $mfatc14, morph_bigattack15 ] { ai_face(); };

void() morph_bigattack15 = [ $mfatc15, morph_bigattack16 ] { ai_face(); };

void() morph_bigattack16 = [ $mfatc16, morph_bigattack17 ] { ai_face(); };

void() morph_bigattack17 = [ $mfatc01, morph_run1        ] { ai_face(); };



void() morph_smack = {
    local vector delta;
    local float ldmg;

    if (!self.enemy) return;
    if (!CanDamage (self.enemy, self)) return;

    ai_face();
    delta = self.enemy.origin - self.origin;
    if (vlen(delta) > 100) return;
        
    ldmg = (random() * 10) + 10;
    T_Damage (self.enemy, self, self, ldmg,DAMARMOR);

    makevectors (self.angles);

    self.enemy.velocity = v_forward * 100 + '0 0 100';
};



void() morph_knockback01 = [ $mfknck01, morph_knockback02 ] { ai_face(); };

void() morph_knockback02 = [ $mfknck02, morph_knockback03 ] { ai_face(); };

void() morph_knockback03 = [ $mfknck03, morph_knockback04 ] { ai_face(); };

void() morph_knockback04 = [ $mfknck04, morph_knockback05 ] { ai_face(); };

void() morph_knockback05 = [ $mfknck05, morph_knockback06 ] { ai_face(); };

void() morph_knockback06 = [ $mfknck06, morph_knockback07 ] { ai_face(); };

void() morph_knockback07 = [ $mfknck07, morph_knockback08 ] { ai_face(); };

void() morph_knockback08 = [ $mfknck08, morph_knockback09 ] { ai_face(); };

void() morph_knockback09 = [ $mfknck09, morph_knockback10 ] { morph_smack(); };

void() morph_knockback10 = [ $mfknck10, morph_knockback11 ] { ai_face(); };

void() morph_knockback11 = [ $mfknck11, morph_knockback12 ] { ai_face(); };

void() morph_knockback12 = [ $mfknck12, morph_run1        ] { ai_face(); };



void() morph_attack = 
{

    local float attackChoice;


    
attackChoice = random();

    
if (attackChoice < 0.50)

        morph_bigattack01();
    else if (attackChoice < 0.75)

        morph_attack01();

    else

        morph_knockback01();

};



void() morph_fire = {
    local vector org, dir;

    ai_face();
    self.effects = self.effects | EF_MUZZLEFLASH;
    makevectors (self.angles);
    org = self.origin + v_forward * 30 + v_right * 8.5 + '0 0 16';
    dir = normalize ( self.enemy.origin - self.origin ) ;
    launch_projectile(org, dir, CT_PROJ_LASER, SPEED_LASER);
    if ( vlen (self.enemy.origin - self.origin) > 400 ) {
        launch_projectile(org, (dir + v_right * 0.04), CT_PROJ_LASER, SPEED_LASER);
        launch_projectile(org, (dir + v_right * 0.04), CT_PROJ_LASER, SPEED_LASER);

    } else {
        launch_projectile(org, (dir + v_right * 0.10), CT_PROJ_LASER, SPEED_LASER);
        launch_projectile(org, (dir + v_right * 0.10), CT_PROJ_LASER, SPEED_LASER);
    }

};

void() morph_fire1 = [ $mfata01, morph_fire2 ] {ai_face();};
void() morph_fire2 = [ $mfata02, morph_fire3 ] {ai_face();};
void() morph_fire3 = [ $mfata03, morph_fire4 ] {ai_face();};
void() morph_fire4 = [ $mfata04, morph_fire5 ] {morph_fire();};
void() morph_fire5 = [ $mfata05, morph_fire6 ] {ai_face();};
void() morph_fire6 = [ $mfata06, morph_fire7 ] {ai_face();};
void() morph_fire7 = [ $mfata07, morph_fire8 ] {ai_face();};
void() morph_fire8 = [ $mfata08, morph_fire9] {ai_face();};
void() morph_fire9 = [ $mfata09, morph_run1 ] {ai_face();};

void() morph_painA1 = [ $mfpain01, morph_painA2 ] {};
void() morph_painA2 = [ $mfpain02, morph_painA3 ] {};
void() morph_painA3 = [ $mfpain03, morph_painA4 ] {};
void() morph_painA4 = [ $mfpain04, morph_painA5 ] {};
void() morph_painA5 = [ $mfpain05, morph_painA6 ] {};
void() morph_painA6 = [ $mfpain06, morph_painA7 ] {};
void() morph_painA7 = [ $mfpain07, morph_painA8 ] {};
void() morph_painA8 = [ $mfpain08, morph_painA9 ] {};
void() morph_painA9 = [ $mfpain09, morph_painA10 ] {};
void() morph_painA10 = [ $mfpain10, morph_run1 ] { morph_minionspawn(); };


void() morph_painB1 = [ $mfpbin01, morph_painB2 ] {};
void() morph_painB2 = [ $mfpbin02, morph_painB3 ] {};
void() morph_painB3 = [ $mfpbin03, morph_painB4 ] {};
void() morph_painB4 = [ $mfpbin04, morph_painB5 ] {};
void() morph_painB5 = [ $mfpbin05, morph_painB6 ] {};
void() morph_painB6 = [ $mfpbin06, morph_painB7 ] {};
void() morph_painB7 = [ $mfpbin07, morph_run1 ] { morph_minionspawn(); };


void(entity inflictor, entity attacker, float damage) morph_pain = {

    local float painChoice;


    if (random() > 0.1) {
        morph_minionspawn();
        return;
    }


    
if (self.pain_finished > time)    return;

    if (random() > 0.25)
 return;


    painChoice = random();
    self.pain_finished = time + 2;
    sound (self, CHAN_VOICE, "guard/pain1.wav", 1, ATTN_NORM );

    if ( painChoice > 0.6)
        self.think = morph_painB1;

    else
        self.think = morph_painA1;


    self.nextthink = time + 0.1;

};



void() morph_die1 = [ $mfdth01, morph_die2 ] {};
void() morph_die2 = [ $mfdth02, morph_die3 ] {};
void() morph_die3 = [ $mfdth03, morph_die4 ] {};
void() morph_die4 = [ $mfdth04, morph_die5 ] {};
void() morph_die5 = [ $mfdth05, morph_die6 ] {};
void() morph_die6 = [ $mfdth06, morph_die7 ] {};
void() morph_die7 = [ $mfdth07, morph_die8 ] {};
void() morph_die8 = [ $mfdth08, morph_die9 ] {};
void() morph_die9 = [ $mfdth09, morph_die10 ] { self.skin = self.skin + 1; };

void() morph_die10 = [ $mfdth10, morph_die11 ] {};

void() morph_die11 = [ $mfdth11, morph_die12 ] {};
void() morph_die12 = [ $mfdth12, morph_die13 ] {};
void() morph_die13 = [ $mfdth13, morph_die14 ] {};
void() morph_die14 = [ $mfdth14, morph_die15 ] {};

void() morph_die15 = [ $mfdth15, morph_die16 ] {};

void() morph_die16 = [ $mfdth16, morph_die17 ] {};

void() morph_die17 = [ $mfdth17, morph_die18 ] {};
void() morph_die18 = [ $mfdth18, morph_die19 ] {};
void() morph_die19 = [ $mfdth19, morph_die20 ] {};

void() morph_die20 = [ $mfdth20, morph_die21 ] {};

void() morph_die21 = [ $mfdth21, morph_die21 ] { remove(self); };

void() morph_die = {
    sound ( self, CHAN_VOICE, "guard/death.wav", 1, ATTN_NORM);

    self.solid = SOLID_NOT;
    self.think = morph_die1;
    self.nextthink = time + 0.1;
};




void() morph_wake1 = [ $start01, morph_wake2 ] { sound ( self, CHAN_VOICE, "guard/see1.wav", 1, ATTN_NORM );
 };
void() morph_wake2 = [ $start02, morph_wake3 ] {};
void() morph_wake3 = [ $start03, morph_wake4 ] {};
void() morph_wake4 = [ $start04, morph_wake5 ] {};
void() morph_wake5 = [ $start05, morph_wake6 ] {};
void() morph_wake6 = [ $start06, morph_wake7 ] {};
void() morph_wake7 = [ $start07, morph_wake8 ] {};
void() morph_wake8 = [ $start08, morph_wake9 ] {};
void() morph_wake9 = [ $start09, morph_wake10 ] {};
void() morph_wake10 = [ $start10, morph_wake11 ] {};
void() morph_wake11 = [ $start11, morph_wake12 ] {};
void() morph_wake12 = [ $start12, morph_wake13 ] {};
void() morph_wake13 = [ $start13, morph_wake14 ] {};
void() morph_wake14 = [ $start14, morph_wake15 ] {};
void() morph_wake15 = [ $start15, morph_wake16 ] { self.skin = 1; };
void() morph_wake16 = [ $start16, morph_wake17 ] {};
void() morph_wake17 = [ $start17, morph_wake18 ] {};
void() morph_wake18 = [ $start18, morph_wake20 ] {};
//void() morph_wake18 = [ $start18, morph_wake19 ] {};
//void() morph_wake19 = [ $start19, morph_wake20 ] {};
void() morph_wake20 = [ $start20, morph_wake21 ] {};
void() morph_wake21 = [ $start21, morph_wake22 ] {};
void() morph_wake22 = [ $start22, morph_wake23 ] {};
void() morph_wake23 = [ $start23, morph_wake24 ] {};
void() morph_wake24 = [ $start24, morph_wake25 ] {};
void() morph_wake25 = [ $start25, morph_wake26 ] {};
void() morph_wake26 = [ $start26, morph_wake27 ] {};
void() morph_wake27 = [ $start27, morph_wake28 ] {};
void() morph_wake28 = [ $start28, morph_wake29 ] {};
void() morph_wake29 = [ $start29, morph_wake30 ] {};
void() morph_wake30 = [ $start30, morph_wake31 ] {};
void() morph_wake31 = [ $start31, morph_stand1 ] {
    
self.solid = SOLID_SLIDEBOX;
    self.takedamage = DAMAGE_AIM;
    self.skin = self.skin - 1;
    if ( self.owner ) {
        self.enemy = self.owner.enemy;
        self.movetarget = self.owner.movetarget;
        self.goalentity = self.owner.goalentity;
        self.think = morph_run1;
        self.nextthink = time + 0.1;
    }
};


void() morph_setup = {

    self.classname = "monster_morph";
    if (self.spawnflags & MORPH_AZTEC) {
        self.mdl = "progs/rogue/mon_morph_az.mdl";    // Aztec

    }
 else if (self.spawnflags & MORPH_EGYPT)    {
        self.mdl = "progs/rogue/mon_morph_eg.mdl";    // Egyptian
    }
 else if (self.spawnflags & MORPH_GREEK)
    {
        self.mdl = "progs/rogue/mon_morph_gr.mdl";    // greek

    }
 else
 self.mdl = "progs/rogue/mon_morph_eg.mdl";
    // Default to Egyptian

    if (self.owner)
        self.mdl = self.owner.mdl;

    self.pain_sound = "guard/pain1.wav";
    self.sight_sound = "guard/see1.wav";

    self.solid = SOLID_NOT;            // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;

    self.yaw_speed = 20;
    self.view_ofs = '0 0 25';
    self.frame = $start01;
    self.effects = self.effects | EF_DIMLIGHT;
    if (self.health < 1) self.health = 2000;
    if ( self.owner ) {
        self.effects = 0;
        self.health = 200;
        self.spawnflags = self.owner.spawnflags;
    }
    
    if (!self.minion_maxcount) self.minion_maxcount = 3;
    self.gibhealth = MON_NEVERGIB;        // Never gib
    self.gibbed = FALSE;            // In one piece
    self.pain_flinch = 35;
    self.steptype = FS_TYPEHEAVY;
    self.movespeed = 1;            // Can never be a turret

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    // Morphs have special animation movement sets
    // Need extra parameters for their morphing ability
    // Can be setup to start "phased" out and surprise the player, morphing up from the ground
    self.idlebusy = SKULLW_STAND;
    if (self.height == 0) self.height = SKULLW_HEIGHT;
    if (self.distmin == 0) self.distmin = SKULLW_DISTMIN;
    if (self.distmax == 0) self.distmax = SKULLW_DISTMAX;
    if (!self.bodyphased) self.bodyphased = MONAI_SKULLWSOLID;
    else self.bodyphased = MONAI_SKULLWINVIS;

    self.th_pain = morph_pain;
    self.th_die = morph_die;
    if (self.targetname) self.use = morph_use;

    // Does the morph start "phased" out?
    // Need to intercept idle/combat functions
    if (self.bodyphased == MONAI_SKULLWINVIS) {
        self.takedamage = DAMAGE_NO; // Can't hurt it when morphing
        self.skin = 2;

        self.th_stand = morph_phased;
        self.th_walk = morph_phased;
        self.th_run = morph_wake1;
        self.th_missile = morph_wake1;
        self.th_melee = morph_wake1;
    } else {
        self.takedamage = DAMAGE_AIM;
        self.skin = 0;
        self.th_stand = morph_stand1;
        self.th_walk = morph_walk1;
        self.th_run = morph_run1;
        self.th_missile = morph_fire1;
        self.th_melee = morph_attack;
    }
    
    self.classtype = CT_MONMORPH;
    self.classgroup = CG_BOSS;
    self.classmove = MON_MOVEWALK;
    if (self.deathstring == "")  self.deathstring = " was crushed by a Guardian\n";

    monster_start();
};


void() morph_use = {
    self.enemy = activator;
    if ( self.delay ) {
            self.think = morph_wake1;
            self.nextthink = time + self.delay;
        } else {
               self.think = morph_wake1;
            self.nextthink = time + 0.1;
    }
};


void() morph_minionspawn = {

    local entity    theDest;
    local entity    theChild;


    if (self.owner) return;

    if (self.childrenSpawned > self.minion_maxcount) return;

    
theDest = morph_find_dest();
    if (theDest == world) return;


    theChild = spawn();
    theChild.model = self.model;
    theChild.angles = theDest.mangle;
    theChild.mangle = theDest.mangle;
    theChild.owner = self;

    theChild.enemy = self.enemy;
    theChild.movetarget = self.movetarget;
    theChild.goalentity = self.goalentity;
    setorigin (theChild, theDest.origin);
    theChild.think = morph_setup;
    theChild.nextthink = time + 0.1 + random()*0.2;
};

void() morph_phased = { 
    self.nextthink = time + 0.1;    // Keep looking for player
    self.think = morph_phased;    // Keep looping
    ai_stand();             // Keep looking
};

// monster_morph (1 0 0) (-16 -16 -24) (16 16 32) Ambush Aztec Egyptian Greek  The morphing monster.
// Select the look of the monster (Aztec,Greek,Egyptian).
void() monster_morph = {
    if (deathmatch) { remove(self); return; }

    precache_model ("progs/rogue/mon_morph_gr.mdl");
    precache_model ("progs/rogue/mon_morph_az.mdl");
    precache_model ("progs/rogue/mon_morph_eg.mdl");
    precache_model ("progs/laser.mdl");
    precache_sound ("enforcer/enfire.wav");
    precache_sound ("guard/death.wav");

    precache_sound ("guard/pain1.wav");

    precache_sound ("guard/see1.wav");


    morph_setup();
};

void() info_morph_destination = {};