/*==============================================================================
GOLEM (From Ne_Ruins MOD) (Hexen2 Model)
==============================================================================*/

// (000) standing still with heavy breathing
$frame wait1 wait2 wait3 wait4 wait5 wait6 wait7 wait8
$frame wait9 wait10 wait11 wait12 wait13 wait14 wait15 wait16
$frame wait17 wait18 wait19 wait20 wait21 wait22

// (022) Transfer B
$frame tranB1 tranB2 tranB3 tranB4 tranB5 tranB6 tranB7 tranB8
$frame tranB9 tranB10 tranB11 tranB12 tranB13

// (035) statue to walking
$frame birth1 birth2 birth3 birth4 birth5 birth6 birth7 birth8
$frame birth9 birth10 birth11 birth12 birth13 birth14 birth15 birth16

// (051) Really slow walk
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12 walk13 walk14 walk15 walk16
$frame walk17 walk18 walk19 walk20 walk21 walk22 walk23 walk24
$frame walk25 walk26 walk27 walk28 walk29 walk30 walk31 walk32
$frame walk33 walk34

// (085) Running
$frame run1 run2 run3 run4 run5 run6 run7 run8
$frame run9 run10 run11 run12 run13 run14 run15 run16
$frame run17 run18 run19 run20 run21 run22 run23 run24

// (109) Right Fist Punch
$frame punch1 punch2 punch3 punch4 punch5 punch6 punch7 punch8
$frame punch9 punch10 punch11 punch12 punch13 punch14 punch15 punch16
$frame punch17 punch18 punch19 punch20 punch21 punch22 punch23 punch24

// (133) Right Fist Pound ground
$frame pound1 pound2 pound3 pound4 pound5 pound6 pound7 pound8
$frame pound9 pound10 pound11 pound12 pound13 pound14 pound15 pound16
$frame pound17 pound18 pound19 pound20 pound21 pound22 pound23 pound24

// (157) Death Forward
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12 death13 death14 death15 death16
$frame death17 death18 death19 death20 death21 death22

// (179) Magic attack from torso
$frame magic1 magic2 magic3 magic4 magic5 magic6 magic7 magic8
$frame magic9 magic10 magic11 magic12 magic13 magic14 magic15 magic16
$frame magic17 magic18 magic19 magic20 magic21 magic22 magic23 magic24

// (203) Stomp Foot on ground
$frame stomp1 stomp2 stomp3 stomp4 stomp5 stomp6 stomp7 stomp8
$frame stomp9 stomp10 stomp11 stomp12 stomp13 stomp14 stomp15 stomp16
$frame stomp17 stomp18 stomp19 stomp20 stomp21 stomp22 stomp23 stomp24

// (227) Rush forward and knock player away
$frame knock1 knock2 knock3 knock4 knock5 knock6 knock7 knock8
$frame knock9 knock10 knock11 knock12 knock13 knock14 knock15 knock16
$frame knock17 knock18 knock19 knock20 knock21 knock22 knock23 knock24

//============================================================================
void() golem_stand1 =[ $wait1, golem_stand2 ] {monster_idle_sound();ai_stand();};
void() golem_stand2 =[ $wait2, golem_stand3 ] {ai_stand();};
void() golem_stand3 =[ $wait3, golem_stand4 ] {ai_stand();};
void() golem_stand4 =[ $wait4, golem_stand5 ] {ai_stand();};
void() golem_stand5 =[ $wait5, golem_stand6 ] {ai_stand();};
void() golem_stand6 =[ $wait6, golem_stand7 ] {ai_stand();};
void() golem_stand7 =[ $wait7, golem_stand8 ] {ai_stand();};
void() golem_stand8 =[ $wait8, golem_stand9 ] {ai_stand();};
void() golem_stand9 =[ $wait9, golem_stand10 ] {ai_stand();};
void() golem_stand10 =[ $wait10, golem_stand11 ] {ai_stand();};
void() golem_stand11 =[ $wait11, golem_stand12 ] {ai_stand();};
void() golem_stand12 =[ $wait12, golem_stand13 ] {ai_stand();};
void() golem_stand13 =[ $wait13, golem_stand14 ] {ai_stand();};
void() golem_stand14 =[ $wait14, golem_stand15 ] {ai_stand();};
void() golem_stand15 =[ $wait15, golem_stand16 ] {ai_stand();};
void() golem_stand16 =[ $wait16, golem_stand17 ] {ai_stand();};
void() golem_stand17 =[ $wait17, golem_stand18 ] {ai_stand();};
void() golem_stand18 =[ $wait18, golem_stand19 ] {ai_stand();};
void() golem_stand19 =[ $wait19, golem_stand20 ] {ai_stand();};
void() golem_stand20 =[ $wait20, golem_stand21 ] {ai_stand();};
void() golem_stand21 =[ $wait21, golem_stand1 ] {ai_stand();};

//============================================================================
void() golem_ai_walk = {
    if (self.frame == $walk1 || self.frame == $walk17)
        { ai_walk(0); monster_footstep(FALSE); }
    else if (self.frame == $walk2 || self.frame == $walk18) ai_walk(1);
    else ai_walk(2); 
    self.nextthink = time + 0.05;
    if (self.frame == $walk8 || self.frame == $walk25) monster_idle_sound();
};

void() golem_walk1 =[ $walk1, golem_walk2 ] {golem_ai_walk();};
void() golem_walk2 =[ $walk2, golem_walk3 ] {golem_ai_walk();};
void() golem_walk3 =[ $walk3, golem_walk4 ] {golem_ai_walk();};
void() golem_walk4 =[ $walk4, golem_walk5 ] {golem_ai_walk();};
void() golem_walk5 =[ $walk5, golem_walk6 ] {golem_ai_walk();};
void() golem_walk6 =[ $walk6, golem_walk7 ] {golem_ai_walk();};
void() golem_walk7 =[ $walk7, golem_walk8 ] {golem_ai_walk();};
void() golem_walk8 =[ $walk8, golem_walk9 ] {golem_ai_walk();};
void() golem_walk9 =[ $walk9, golem_walk10 ] {golem_ai_walk();};
void() golem_walk10 =[ $walk10, golem_walk11 ] {golem_ai_walk();};
void() golem_walk11 =[ $walk11, golem_walk12 ] {golem_ai_walk();};
void() golem_walk12 =[ $walk12, golem_walk13 ] {golem_ai_walk();};
void() golem_walk13 =[ $walk13, golem_walk14 ] {golem_ai_walk();};
void() golem_walk14 =[ $walk14, golem_walk15 ] {golem_ai_walk();};
void() golem_walk15 =[ $walk15, golem_walk16 ] {golem_ai_walk();};
void() golem_walk16 =[ $walk16, golem_walk17 ] {golem_ai_walk();};
void() golem_walk17 =[ $walk17, golem_walk18 ] {golem_ai_walk();};
void() golem_walk18 =[ $walk18, golem_walk19 ] {golem_ai_walk();};
void() golem_walk19 =[ $walk19, golem_walk20 ] {golem_ai_walk();};
void() golem_walk20 =[ $walk20, golem_walk21 ] {golem_ai_walk();};
void() golem_walk21 =[ $walk21, golem_walk22 ] {golem_ai_walk();};
void() golem_walk22 =[ $walk22, golem_walk23 ] {golem_ai_walk();};
void() golem_walk23 =[ $walk23, golem_walk24 ] {golem_ai_walk();};
void() golem_walk24 =[ $walk24, golem_walk25 ] {golem_ai_walk();};
void() golem_walk25 =[ $walk25, golem_walk26 ] {golem_ai_walk();};
void() golem_walk26 =[ $walk26, golem_walk27 ] {golem_ai_walk();};
void() golem_walk27 =[ $walk27, golem_walk28 ] {golem_ai_walk();};
void() golem_walk28 =[ $walk28, golem_walk29 ] {golem_ai_walk();};
void() golem_walk29 =[ $walk29, golem_walk30 ] {golem_ai_walk();};
void() golem_walk30 =[ $walk30, golem_walk31 ] {golem_ai_walk();};
void() golem_walk31 =[ $walk31, golem_walk32 ] {golem_ai_walk();};
void() golem_walk32 =[ $walk32, golem_walk33 ] {golem_ai_walk();};
void() golem_walk33 =[ $walk33, golem_walk34 ] {golem_ai_walk();};
void() golem_walk34 =[ $walk34, golem_walk1 ] {golem_ai_walk();};

//============================================================================
void() golem_ai_run = {
    if (self.frame == $run1 || self.frame == $run13)
        { ai_run(0); monster_footstep(FALSE); }
    else if (self.frame == $run2 || self.frame == $run14) ai_run(3);
    else ai_run(5); 
    self.nextthink = time + 0.05;
    if (self.frame == $run7 || self.frame == $run18) monster_idle_sound();
};

void() golem_run1 =[ $run1, golem_run2 ] {golem_ai_run();};
void() golem_run2 =[ $run2, golem_run3 ] {golem_ai_run();};
void() golem_run3 =[ $run3, golem_run4 ] {golem_ai_run();};
void() golem_run4 =[ $run4, golem_run5 ] {golem_ai_run();};
void() golem_run5 =[ $run5, golem_run6 ] {golem_ai_run();};
void() golem_run6 =[ $run6, golem_run7 ] {golem_ai_run();};
void() golem_run7 =[ $run7, golem_run8 ] {golem_ai_run();};
void() golem_run8 =[ $run8, golem_run9 ] {golem_ai_run();};
void() golem_run9 =[ $run9, golem_run10 ] {golem_ai_run();};
void() golem_run10 =[ $run10, golem_run11 ] {golem_ai_run();};
void() golem_run11 =[ $run11, golem_run12 ] {golem_ai_run();};
void() golem_run12 =[ $run12, golem_run13 ] {golem_ai_run();};
void() golem_run13 =[ $run13, golem_run14 ] {golem_ai_run();};
void() golem_run14 =[ $run14, golem_run15 ] {golem_ai_run();};
void() golem_run15 =[ $run15, golem_run16 ] {golem_ai_run();};
void() golem_run16 =[ $run16, golem_run17 ] {golem_ai_run();};
void() golem_run17 =[ $run17, golem_run18 ] {golem_ai_run();};
void() golem_run18 =[ $run18, golem_run19 ] {golem_ai_run();};
void() golem_run19 =[ $run19, golem_run20 ] {golem_ai_run();};
void() golem_run20 =[ $run20, golem_run21 ] {golem_ai_run();};
void() golem_run21 =[ $run21, golem_run22 ] {golem_ai_run();};
void() golem_run22 =[ $run22, golem_run23 ] {golem_ai_run();};
void() golem_run23 =[ $run23, golem_run1 ] {golem_ai_run();};

//============================================================================
// MELEE - Punch or Pound the ground with fist
//============================================================================
void(float dist) golem_mcharge = { ai_charge(dist);self.nextthink = time + 0.05;};

//----------------------------------------------------------------------
void() golem_punch1 = [ $punch1, golem_punch2 ] {golem_mcharge(5);};
void() golem_punch2 = [ $punch2, golem_punch3 ] {golem_mcharge(5);};
void() golem_punch3 = [ $punch3, golem_punch4 ] {golem_mcharge(5);};
void() golem_punch4 = [ $punch4, golem_punch5 ] {golem_mcharge(5);};
void() golem_punch5 = [ $punch5, golem_punch6 ] {golem_mcharge(5);};
void() golem_punch6 = [ $punch6, golem_punch7 ] {golem_mcharge(5);};
void() golem_punch7 = [ $punch7, golem_punch8 ] {golem_mcharge(5);};
void() golem_punch8 = [ $punch8, golem_punch9 ] {golem_mcharge(5);};
void() golem_punch9 = [ $punch9, golem_punch10 ] {golem_mcharge(5);
    sound (self, CHAN_VOICE, "golem/melee_swipe.wav", 1, ATTN_IDLE);};
void() golem_punch10 = [ $punch10, golem_punch11 ] {golem_mcharge(4);};
void() golem_punch11 = [ $punch11, golem_punch12 ] {golem_mcharge(3);monster_footstep(FALSE);};
void() golem_punch12 = [ $punch12, golem_punch13 ] {golem_mcharge(2);
    self.meleehitsound = "golem/melee_pound.wav";
    self.meleecontact = TRUE;ai_meleesmash(20);
    self.meleecontact = FALSE;
};
void() golem_punch13 = [ $punch13, golem_punch14 ] {golem_mcharge(1);};
void() golem_punch14 = [ $punch14, golem_punch15 ] {golem_mcharge(1);};
void() golem_punch15 = [ $punch15, golem_punch16 ] {golem_mcharge(1);};
void() golem_punch16 = [ $punch16, golem_punch17 ] {golem_mcharge(1);};
void() golem_punch17 = [ $punch17, golem_punch18 ] {golem_mcharge(2);};
void() golem_punch18 = [ $punch18, golem_punch19 ] {golem_mcharge(3);};
void() golem_punch19 = [ $punch19, golem_punch20 ] {golem_mcharge(4);};
void() golem_punch20 = [ $punch20, golem_punch21 ] {golem_mcharge(5);};
void() golem_punch21 = [ $punch21, golem_punch22 ] {golem_mcharge(5);};
void() golem_punch22 = [ $punch22, golem_punch23 ] {golem_mcharge(5);};
void() golem_punch23 = [ $punch23, golem_punch24 ] {golem_mcharge(5);};
void() golem_punch24 = [ $punch24, golem_run1 ] {golem_mcharge(5);monster_footstep(FALSE);};

//----------------------------------------------------------------------
void() golem_pound1 = [ $pound1, golem_pound2 ] {golem_mcharge(5);};
void() golem_pound2 = [ $pound2, golem_pound3 ] {golem_mcharge(5);};
void() golem_pound3 = [ $pound3, golem_pound4 ] {golem_mcharge(5);};
void() golem_pound4 = [ $pound4, golem_pound5 ] {golem_mcharge(5);};
void() golem_pound5 = [ $pound5, golem_pound6 ] {golem_mcharge(5);};
void() golem_pound6 = [ $pound6, golem_pound7 ] {golem_mcharge(5);};
void() golem_pound7 = [ $pound7, golem_pound8 ] {golem_mcharge(5);};
void() golem_pound8 = [ $pound8, golem_pound9 ] {golem_mcharge(5);};
void() golem_pound9 = [ $pound9, golem_pound10 ] {golem_mcharge(5);
    sound (self, CHAN_VOICE, "golem/melee_swipe.wav", 1, ATTN_IDLE);};
void() golem_pound10 = [ $pound10, golem_pound11 ] {golem_mcharge(4);};
void() golem_pound11 = [ $pound11, golem_pound12 ] {golem_mcharge(3);monster_footstep(FALSE);};
void() golem_pound12 = [ $pound12, golem_pound13 ] {golem_mcharge(2);
    self.meleehitsound = "golem/melee_pound.wav";
    self.meleecontact = TRUE;ai_meleesmash(40);
    self.meleecontact = FALSE;
};
void() golem_pound13 = [ $pound13, golem_pound14 ] {golem_mcharge(1);};
void() golem_pound14 = [ $pound14, golem_pound15 ] {golem_mcharge(1);};
void() golem_pound15 = [ $pound15, golem_pound16 ] {golem_mcharge(1);};
void() golem_pound16 = [ $pound16, golem_pound17 ] {golem_mcharge(1);};
void() golem_pound17 = [ $pound17, golem_pound18 ] {golem_mcharge(2);};
void() golem_pound18 = [ $pound18, golem_pound19 ] {golem_mcharge(3);};
void() golem_pound19 = [ $pound19, golem_pound20 ] {golem_mcharge(4);};
void() golem_pound20 = [ $pound20, golem_pound21 ] {golem_mcharge(5);};
void() golem_pound21 = [ $pound21, golem_pound22 ] {golem_mcharge(5);};
void() golem_pound22 = [ $pound22, golem_pound23 ] {golem_mcharge(5);};
void() golem_pound23 = [ $pound23, golem_pound24 ] {golem_mcharge(5);};
void() golem_pound24 = [ $pound24, golem_run1 ] {golem_mcharge(5);monster_footstep(FALSE);};

//----------------------------------------------------------------------
void() golem_knock1 = [ $knock1, golem_knock2 ] {golem_mcharge(5);};
void() golem_knock2 = [ $knock2, golem_knock3 ] {golem_mcharge(6);};
void() golem_knock3 = [ $knock3, golem_knock4 ] {golem_mcharge(7);};
void() golem_knock4 = [ $knock4, golem_knock5 ] {golem_mcharge(8);};
void() golem_knock5 = [ $knock5, golem_knock6 ] {golem_mcharge(9);};
void() golem_knock6 = [ $knock7, golem_knock7 ] {golem_mcharge(10);};
void() golem_knock7 = [ $knock6, golem_knock8 ] {golem_mcharge(9);};
void() golem_knock8 = [ $knock8, golem_knock9 ] {golem_mcharge(8);};
void() golem_knock9 = [ $knock9, golem_knock10 ] {golem_mcharge(7);};
void() golem_knock10 = [ $knock10, golem_knock11 ] {golem_mcharge(6);};
void() golem_knock11 = [ $knock11, golem_knock12 ] {golem_mcharge(5);monster_footstep(FALSE);};
void() golem_knock12 = [ $knock12, golem_knock13 ] {golem_mcharge(5);
    sound (self, CHAN_VOICE, "golem/melee_swipe.wav", 1, ATTN_IDLE);};
void() golem_knock13 = [ $knock13, golem_knock14 ] {golem_mcharge(4);};
void() golem_knock14 = [ $knock14, golem_knock15 ] {golem_mcharge(3);};
void() golem_knock15 = [ $knock15, golem_knock16 ] {golem_mcharge(2);};
void() golem_knock16 = [ $knock16, golem_knock17 ] {golem_mcharge(1);
    self.meleehitsound = "golem/melee_punch.wav";
    self.meleecontact = TRUE;ai_meleesmash(30);
    // If punch made contact, lift the enemy off ground
    if (self.meleecontact == FALSE) {
        self.pos1 = self.enemy.origin - self.origin;
        self.pos1_z = 0;
        self.pos1 = normalize(self.pos1);
        self.enemy.velocity = self.enemy.velocity + self.pos1 * 400;
        self.enemy.velocity_z = self.enemy.velocity_z + 250;
        self.enemy.flags = self.enemy.flags - (self.enemy.flags & FL_ONGROUND);
    }
    self.meleecontact = FALSE;
};
void() golem_knock17 = [ $knock17, golem_knock18 ] {golem_mcharge(1);};
void() golem_knock18 = [ $knock18, golem_knock19 ] {golem_mcharge(2);};
void() golem_knock19 = [ $knock19, golem_knock20 ] {golem_mcharge(3);};
void() golem_knock20 = [ $knock20, golem_knock21 ] {golem_mcharge(4);};
void() golem_knock21 = [ $knock21, golem_knock22 ] {golem_mcharge(5);};
void() golem_knock22 = [ $knock22, golem_knock23 ] {golem_mcharge(5);};
void() golem_knock23 = [ $knock23, golem_knock24 ] {golem_mcharge(5);};
void() golem_knock24 = [ $knock24, golem_run1 ] {
    golem_mcharge(5); 
    // Double check enemy is still alive?
    if (self.enemy.health > 1) {
        // If the enemy is visible, quickly go for range attack
        if (!visblocked_wide(self.enemy, '0 0 34', '0 0 24')) {
            SUB_AttackFinished (2 + 2*random());
            self.think = self.th_missile;
        }
    }
};

//----------------------------------------------------------------------
void() golem_footstomp = {self.nextthink = time + 0.05;};

void() golem_stomp1 = [ $stomp1, golem_stomp2 ] {golem_footstomp();
    self.pain_finished = time + self.pain_timeout;};
void() golem_stomp2 = [ $stomp2, golem_stomp3 ] {golem_footstomp();
    sound (self, CHAN_VOICE, self.sight_sound, 1, ATTN_IDLE);};
void() golem_stomp3 = [ $stomp3, golem_stomp4 ] {golem_footstomp();};
void() golem_stomp4 = [ $stomp4, golem_stomp5 ] {golem_footstomp();};
void() golem_stomp5 = [ $stomp5, golem_stomp6 ] {golem_footstomp();monster_idle_sound();};
void() golem_stomp6 = [ $stomp6, golem_stomp7 ] {golem_footstomp();};
void() golem_stomp7 = [ $stomp7, golem_stomp8 ] {golem_footstomp();};
void() golem_stomp8 = [ $stomp8, golem_stomp9 ] {golem_footstomp();};
void() golem_stomp9 = [ $stomp9, golem_stomp10 ] {golem_footstomp();};
void() golem_stomp10 = [ $stomp10, golem_stomp11 ] {golem_footstomp();};
void() golem_stomp11 = [ $stomp11, golem_stomp12 ] {golem_footstomp();};
void() golem_stomp12 = [ $stomp12, golem_stomp13 ] {golem_footstomp();};
void() golem_stomp13 = [ $stomp13, golem_stomp14 ] {
    golem_footstomp(); self.meleehitsound = "golem/groundslam.wav";
    ai_shockwave(MONAI_STOMPIMPACT, MONAI_STOMPDAMAGE, MONAI_STOMPRADIUS, MONAI_STOMPFORWARD, MONAI_STOMPUP);
};
void() golem_stomp14 = [ $stomp14, golem_stomp15 ] {golem_footstomp();};
void() golem_stomp15 = [ $stomp15, golem_stomp16 ] {golem_footstomp();};
void() golem_stomp16 = [ $stomp16, golem_stomp17 ] {golem_footstomp();monster_idle_sound();};
void() golem_stomp17 = [ $stomp17, golem_stomp18 ] {golem_footstomp();};
void() golem_stomp18 = [ $stomp18, golem_stomp19 ] {golem_footstomp();};
void() golem_stomp19 = [ $stomp19, golem_stomp20 ] {golem_footstomp();};
void() golem_stomp20 = [ $stomp20, golem_stomp21 ] {golem_footstomp();};
void() golem_stomp21 = [ $stomp21, golem_stomp22 ] {golem_footstomp();};
void() golem_stomp22 = [ $stomp22, golem_stomp23 ] {golem_footstomp();};
void() golem_stomp23 = [ $stomp23, golem_stomp24 ] {golem_footstomp();};
void() golem_stomp24 = [ $stomp24, golem_run1 ] {
    golem_footstomp(); monster_footstep(FALSE);
    // Double check enemy is still alive?
    // Make sure golem can do range attacks (melee only ver exists)
    if (self.enemy.health > 1 && !(self.spawnflags & MON_GOLEM_MELEEONLY)) {
        // If the enemy is visible, quickly go for range attack
        if (!visblocked_wide(self.enemy, '0 0 34', '0 0 24')) {
            SUB_AttackFinished (2 + 2*random());
            self.think = self.th_missile;
        }
    }
};


//----------------------------------------------------------------------
void() golem_melee =
{
    if (self.enemy.flags & FL_MONSTER) golem_knock1();
    else {
        self.meleeattack = random();
        if (self.meleeattack < 0.2) golem_knock1();
        else if (self.meleeattack < 0.6) golem_punch1();
        else golem_pound1();
    }
}

//============================================================================
// RANGE - ROCK STORM from Torso
//============================================================================
void() golem_rockstorm = 
{
    local float shotcount, zpart;
    local vector org, dir, yrand, zrand;

    if (!self.enemy) return;
    if (self.health < 1) return;

    // Make sure facing the right direction
    ai_face();
    makevectors(self.angles);

    // Spawn a mist of particles where projectiles came from
    org = self.origin + v_forward*20;
    zpart = 25 + rint(random()*25);
    particle_explode(org, zpart, 2, PARTICLE_BURST_WHITE, PARTICLE_BURST_UPWARD);

    // Randomize quantity each time called
    if (random() < 0.5) shotcount = 2;
    else shotcount = 3;

    while (shotcount > 0) {
        // Randomly spawn projectiles from golems chest
        yrand = (crandom()*30)*v_right;
        zrand = (crandom()*30)*v_up;
        org = self.origin + attack_vector(self.attack_offset) + yrand + zrand;

        // Straight line velocity from source to target
        dir = (self.enemy.origin - org) + (v_right*(crandom()*50));
        dir = normalize (dir);

        // Variable speed based on skill level with extra random spice
        self.attack_speed = SPEED_GOLEMSPIKE + (skill * SPEED_GOLEMSKILL) + random()*10;
        launch_projectile (org, dir, CT_PROJ_GROCK, self.attack_speed);
        shotcount = shotcount - 1;
    }
};

//----------------------------------------------------------------------
void() golem_magtime = {self.nextthink = time + 0.05;};

void() golem_magic1 = [ $magic1, golem_magic2 ] {golem_magtime();ai_face();
    self.pain_finished = time + 1.5;
    sound (self, CHAN_WEAPON, "golem/rockstorm.wav", 1, ATTN_NORM);
};
void() golem_magic2 = [ $magic2, golem_magic3 ] {golem_magtime();};
void() golem_magic3 = [ $magic3, golem_magic4 ] {golem_magtime();ai_face();};
void() golem_magic4 = [ $magic4, golem_magic5 ] {golem_magtime();};
void() golem_magic5 = [ $magic5, golem_magic6 ] {golem_magtime();ai_face();};
void() golem_magic6 = [ $magic6, golem_magic7 ] {golem_magtime();};
void() golem_magic7 = [ $magic7, golem_magic8 ] {golem_magtime();ai_face();};
void() golem_magic8 = [ $magic8, golem_magic9 ] {golem_magtime();};
void() golem_magic9 = [ $magic9, golem_magic10 ] {golem_magtime();ai_face();};
void() golem_magic10 = [ $magic10, golem_magic11 ] {golem_magtime();};
void() golem_magic11 = [ $magic11, golem_magic12 ] {golem_magtime();ai_face();};
void() golem_magic12 = [ $magic12, golem_magic13 ] {golem_magtime();};
void() golem_magic13 = [ $magic13, golem_magic14 ] {golem_magtime();golem_rockstorm();};
void() golem_magic14 = [ $magic14, golem_magic15 ] {golem_magtime();};
void() golem_magic15 = [ $magic15, golem_magic16 ] {golem_magtime();golem_rockstorm();};
void() golem_magic16 = [ $magic16, golem_magic17 ] {golem_magtime();};
void() golem_magic17 = [ $magic17, golem_magic18 ] {golem_magtime();golem_rockstorm();};
void() golem_magic18 = [ $magic18, golem_magic19 ] {golem_magtime();};
void() golem_magic19 = [ $magic19, golem_magic20 ] {golem_magtime();golem_rockstorm();};
void() golem_magic20 = [ $magic20, golem_magic21 ] {golem_magtime();};
void() golem_magic21 = [ $magic21, golem_magic22 ] {golem_magtime();golem_rockstorm();};
void() golem_magic22 = [ $magic22, golem_magic23 ] {golem_magtime();};
void() golem_magic23 = [ $magic23, golem_magic24 ] {golem_magtime();};
void() golem_magic24 = [ $magic24, golem_run1 ] {golem_magtime();};

//============================================================================
// PAIN
//============================================================================
// Short & fast pain animation
void()     golem_painA1 =[ $birth8, golem_painA2 ] {};
void()     golem_painA2 =[ $birth10, golem_painA3 ] {};
void()     golem_painA3 =[ $birth12, golem_painA4 ] {};
void()     golem_painA4 =[ $birth14, golem_painA5 ] {};
void()     golem_painA5 =[ $birth16, golem_run1 ] {};

// Long slow pain animation
void()     golem_painB1 =[ $birth8, golem_painB2 ] {};
void()     golem_painB2 =[ $birth9, golem_painB3 ] {};
void()     golem_painB3 =[ $birth10, golem_painB4 ] {};
void()     golem_painB4 =[ $birth11, golem_painB5 ] {};
void()     golem_painB5 =[ $birth12, golem_painB6 ] {};
void()     golem_painB6 =[ $birth13, golem_painB7 ] {};
void()     golem_painB7 =[ $birth14, golem_painB8 ] {};
void()     golem_painB8 =[ $birth15, golem_painB9 ] {};
void()     golem_painB9 =[ $birth16, golem_run1 ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) golem_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        if (random() < 0.5) sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
        if (self.pain_check == 1) {
            if (self.lip < 0.4) golem_painB1();    // Long
            else golem_painA1();                // Short
        }
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.axhitme = 0;
            self.pain_finished = time + 0.9;
            golem_painB1 ();
        }
    }
};

//============================================================================
void() golem_death1    = [ $death1, golem_death2 ] {};
void() golem_death2    = [ $death2, golem_death3 ] {};
void() golem_death3    = [ $death3, golem_death4 ] {ai_forward(1);self.solid = SOLID_NOT;};
void() golem_death4    = [ $death4, golem_death5 ] {ai_forward(3);};
void() golem_death5    = [ $death5, golem_death6 ] {ai_forward(3);};
void() golem_death6    = [ $death6, golem_death7 ] {ai_forward(6);};
void() golem_death7    = [ $death7, golem_death8 ] {ai_forward(8);monster_footstep(FALSE);};
void() golem_death8    = [ $death8, golem_death9 ] {ai_forward(6);};
void() golem_death9    = [ $death9, golem_death10] {};
void() golem_death10 = [ $death10, golem_death11] {};
void() golem_death11 = [ $death11, golem_death12] {};
void() golem_death12 = [ $death12, golem_death13] {};
void() golem_death13 = [ $death13, golem_death14] {};
void() golem_death14 = [ $death14, golem_death15] {};
void() golem_death15 = [ $death15, golem_death16] {ai_forward(2);};
void() golem_death16 = [ $death16, golem_death17] {ai_forward(2);};
void() golem_death17 = [ $death17, golem_death18] {ai_forward(2);};
void() golem_death18 = [ $death18, golem_death19] {ai_forward(2);};
void() golem_death19 = [ $death19, golem_death20] {ai_forward(2);};
void() golem_death20 = [ $death20, golem_death21] {ai_forward(2);};
void() golem_death21 = [ $death21, golem_death22] {ai_forward(2);monster_death_postcheck();};
void() golem_death22 = [ $death22, golem_death22] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() golem_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();
    
    // Spawn some extra rubble before dying
    SpawnProjectileSmoke(self.origin, 200, 50, 150);
    SpawnProjectileSmoke(self.origin, 300, 50, 150);
    SpawnProjectileSmoke(self.origin, 200, 50, 250);

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "golem/death.wav", 1, ATTN_NORM);
        golem_death1 ();
    }
};

//============================================================================
// Wakeup From Statue form
//============================================================================
void() golem_birth = {
    if (random() < 0.8) SpawnProjectileSmoke(self.origin, 200, 50, 250);
    self.nextthink = time + 0.05;
};

//----------------------------------------------------------------------
void() golem_wakefinished =
{
    // No longer need statue spawnflag, remove it
    if (self.spawnflags & MON_STATUE) 
        self.spawnflags = self.spawnflags - MON_STATUE;
    self.spawnstatue = FALSE;        // No longer stationary

    self.th_stand = golem_stand1;    // Idling on feet
    self.th_walk = golem_walk1;        // Walking
    self.th_run = golem_run1;        // Running
    self.th_melee = golem_melee;    // Fist attack
    // Check for melee + stomp attack only version
    if (!(self.spawnflags & MON_GOLEM_MELEEONLY)) 
        self.th_missile = golem_magic1;
    self.th_slide = golem_stomp1;    // Floor stomp
    self.th_pain = golem_pain;        // Stumble and pain
    self.th_die = golem_die;        // Face meets floor
    self.takedamage = DAMAGE_AIM;    // Can receive damage
    monster_targets();                // Check for targets
    golem_birth();                    // more rubble
};

//----------------------------------------------------------------------
void() golem_wake1 =[ $birth1, golem_wake2 ] {golem_birth();
    sound (self, CHAN_BODY, "golem/wakestatue.wav", 1, ATTN_NORM);};
void() golem_wake2 =[ $birth2, golem_wake3 ] {golem_birth();ai_forward(2);};
void() golem_wake3 =[ $birth3, golem_wake4 ] {golem_birth();ai_forward(3);};
void() golem_wake4 =[ $birth4, golem_wake5 ] {golem_birth();ai_forward(4);};
void() golem_wake5 =[ $birth5, golem_wake6 ] {golem_birth();ai_forward(3);};
void() golem_wake6 =[ $birth6, golem_wake7 ] {golem_birth();ai_forward(3);};
void() golem_wake7 =[ $birth7, golem_wake8 ] {golem_birth();ai_forward(1);};
void() golem_wake8 =[ $birth8, golem_wake9 ] {golem_birth();monster_footstep(FALSE);};
void() golem_wake9 =[ $birth9, golem_wake10 ] {golem_birth();};
void() golem_wake10 =[ $birth10, golem_wake11 ] {golem_birth();};
void() golem_wake11 =[ $birth11, golem_wake12 ] {golem_birth();monster_idle_sound();};
void() golem_wake12 =[ $birth12, golem_wake13 ] {golem_birth();};
void() golem_wake13 =[ $birth13, golem_wake14 ] {golem_birth();};
void() golem_wake14 =[ $birth14, golem_wake15 ] {golem_birth();};
void() golem_wake15 =[ $birth15, golem_wake16 ] {golem_birth();};
void() golem_wake16 =[ $birth16, golem_tranB1 ] {golem_birth();};

void() golem_tranB1 = [ $tranB1, golem_tranB2 ] {golem_birth();ai_forward(5);};
void() golem_tranB2 = [ $tranB2, golem_tranB3 ] {golem_birth();ai_forward(5);};
void() golem_tranB3 = [ $tranB3, golem_tranB4 ] {golem_birth();ai_forward(5);monster_idle_sound();};
void() golem_tranB4 = [ $tranB4, golem_tranB5 ] {golem_birth();ai_forward(5);};
void() golem_tranB5 = [ $tranB5, golem_tranB6 ] {golem_birth();ai_forward(5);};
void() golem_tranB6 = [ $tranB6, golem_tranB7 ] {golem_birth();ai_forward(5);};
void() golem_tranB7 = [ $tranB7, golem_tranB8 ] {golem_birth();ai_forward(5);monster_idle_sound();};
void() golem_tranB8 = [ $tranB8, golem_tranB9 ] {golem_birth();ai_forward(5);};
void() golem_tranB9 = [ $tranB9, golem_tranB10 ] {golem_birth();ai_forward(5);};
void() golem_tranB10 = [ $tranB10, golem_tranB11 ] {golem_birth();ai_forward(5);};
void() golem_tranB11 = [ $tranB11, golem_tranB12 ] {golem_birth();ai_forward(5);};
void() golem_tranB12 = [ $tranB12, golem_tranB13 ] {golem_birth();ai_forward(5);};
void() golem_tranB13 = [ $tranB13, golem_run14 ] {golem_wakefinished(); monster_footstep(FALSE);};

//----------------------------------------------------------------------
void() golem_wakeup =
{
    // Check for sudden death!
    if (self.health < 0) return;
    
    // Only call wakeup function once
    self.use = SUB_Null;
    self.th_stand = self.th_walk = self.th_run = SUB_Null;
    self.skin = 0;    // Glowing eyes!
    golem_wake1();
};

//----------------------------------------------------------------------
void() golem_statue =
{
    local float targ_angle, new_frame;

    // Check for sudden death!
    if (self.health < 0) return;
    
    if (random() < MON_IDLE_SOUND && self.pos1_z > 1) {
        new_frame = TRUE;
        self.oldenemy = checkclient();
        if (self.oldenemy.flags & FL_CLIENT) {
            targ_angle  = viewangle(self.origin, self.oldenemy.origin, 0, TRUE, self.oldenemy.v_angle_y);
            if (targ_angle < 135 || targ_angle > 225) new_frame = FALSE;
        }
        self.lip = self.pos1_x + rint(random()*self.pos1_z);
        if (new_frame) {
            sound (self, CHAN_VOICE, self.idle_sound, 1, ATTN_NORM);
            self.frame = self.lip;
        }
    }
    
     self.think = golem_statue;
     self.nextthink = time + 5 + random()*5;
};

/*======================================================================
QUAKED monster_golem (1 0.2 0) (-16 -16 -24) (16 16 24)
======================================================================*/
void() monster_golem =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_golem.mdl";
    precache_model (self.mdl);
    precache_model (MODEL_PROJ_RINGSHOCK);
    precache_model (MODEL_PROJ_GROCK1);
    precache_model (MODEL_PROJ_GROCK2);
    
    self.gibtype = GIBTYPE_STONE;        // Stoney gibs!
    self.gib1mdl = MODEL_PROJ_GROCK1;    // small golem rock
    self.gib2mdl = MODEL_PROJ_GROCK2;    // medium golem rock
    self.gib1frame = self.gib2frame = 9;

    // Idle1a-1b (long sound) idle2a-2b (short sound)
    self.idle_sound = "golem/idle1a.wav";
    self.idle_sound2 = "golem/idle1b.wav";
    self.idle_soundcom = "golem/idle2a.wav";
    self.idle_soundcom2 = "golem/idle2b.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);
    precache_sound (self.idle_soundcom);
    precache_sound (self.idle_soundcom2);
    
    // Break free of statue pose
    precache_sound ("golem/wakestatue.wav");
    
    // Several melee smack sounds and giant ground slam
    precache_sound ("golem/melee_swipe.wav");
    precache_sound ("golem/melee_punch.wav");
    precache_sound ("golem/melee_pound.wav");
    precache_sound ("golem/groundslam.wav");
    
    // Rockstorm - range attack
    precache_sound ("golem/rockstorm.wav");
    precache_sound (SOUND_IMP_ROCK1);
    precache_sound (SOUND_IMP_ROCK2);
    precache_sound (SOUND_IMP_ROCK3);
    precache_sound (SOUND_IMP_ROCK4);
    
    self.pain_sound = "golem/pain1.wav";
    self.pain_sound2 = "golem/pain2.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.pain_sound2);
    precache_sound ("golem/death.wav");

    // Roar!
    self.sight_sound = "golem/sight.wav";
    precache_sound (self.sight_sound);

    self.solid = SOLID_NOT;            // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_GOLEM;
    if (self.health < 1) self.health = 500;
    self.gibhealth = -60;            // Same as shambler
    self.gibbed = FALSE;            // Still in one piece
    self.pain_flinch = 200;            // Demon level
    self.pain_longanim = FALSE;        // resistant to shadow axe
    self.pain_timeout = 2;            // Does not repeat pain
    self.infightextra = 3;            // Triple damage to other monsters
    self.steptype = FS_TYPEGIANT;    // Giant sound
    self.blockudeath = TRUE;        // No humanoid death
    self.bleedcolour = 16;            // Brown dust/dirt
    self.no_liquiddmg = TRUE;        // Immune to slime/lava
    self.meleerange = MONAI_MELEEGOLEM;
    self.attack_offset = '10 0 30';        // Rock storm
    
    self.th_checkattack = GolemCheckAttack;
    self.th_die = golem_die;

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = 0; self.resist_nails = 0.5;
    self.resist_rockets = self.resist_cells = 0;

    //----------------------------------------------------------------------
    // Detect statues and setup different parameters
    // Tougher skin, less pain, heavy feet etc    
    //----------------------------------------------------------------------
    if (self.spawnflags & MON_STATUE) {
        // statues can change position when the player is not looking
        // Specify a start/finish frame using pos1 or use frame
        if (!self.pos1_x) {
            self.pos1_x = $magic1;        // Default pose (wakeup)
            self.pos1_y = $magic24;
        }
        // A frame has been specified, create a small range from that
        if (self.frame > 0) {
            self.pos1_x = self.pos1_y = self.frame;
        }
        // Work out range, make sure no negative values
        self.pos1_z = fabs(self.pos1_y - self.pos1_x);
        self.frame = self.pos1_x;        // Setup frame / pose
        
        self.spawnstatue = TRUE;        // Start as a statue
        self.th_stand = golem_statue;    // Stone idle sound
        self.think1 = golem_wakeup;        // Wakeup statue angry at player
        self.skin = 1;                    // Statue skin (+grey, no eyes)
    }
    else {
        self.th_stand = golem_stand1;
        self.th_walk = golem_walk1;
        self.th_run = golem_run1;
        self.th_melee = golem_melee;
        // Check for melee + stomp attack only version
        if (!(self.spawnflags & MON_GOLEM_MELEEONLY)) 
            self.th_missile = golem_magic1;
        self.th_slide = golem_stomp1;
        self.th_pain = golem_pain;
    }

    if ( (random () < 0.500) ) {
        if (self.deathstring == "")  self.deathstring = " was crushed by a Golem\n";
    } else {
        if (self.deathstring == "")  self.deathstring = " was terminated by a Golem\n";
    }
        
    self.hittype = HIT_STONE;
    self.classtype = CT_MONGOLEM;
    self.classgroup = CG_STONE;
    self.classmove = MON_MOVEWALK;

    monster_start();
};