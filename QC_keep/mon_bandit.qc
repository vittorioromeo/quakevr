/*
==============================================================================

SOLDIER / PLAYER

==============================================================================
*/

$cd id1/models/soldier3
$origin 0 -6 24
$base base        
$skin skin

$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8

$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10

$frame deathc1 deathc2 deathc3 deathc4 deathc5 deathc6 deathc7 deathc8
$frame deathc9 deathc10 deathc11

$frame load1 load2 load3 load4 load5 load6 load7 load8 load9 load10 load11

$frame pain1 pain2 pain3 pain4 pain5 pain6

$frame painb1 painb2 painb3 painb4 painb5 painb6 painb7 painb8 painb9 painb10
$frame painb11 painb12 painb13 painb14

$frame painc1 painc2 painc3 painc4 painc5 painc6 painc7 painc8 painc9 painc10
$frame painc11 painc12 painc13

$frame run1 run2 run3 run4 run5 run6 run7 run8

$frame shoot1 shoot2 shoot3 shoot4 shoot5 shoot6 shoot7 shoot8 shoot9

$frame prowl_1 prowl_2 prowl_3 prowl_4 prowl_5 prowl_6 prowl_7 prowl_8
$frame prowl_9 prowl_10 prowl_11 prowl_12 prowl_13 prowl_14 prowl_15 prowl_16
$frame prowl_17 prowl_18 prowl_19 prowl_20 prowl_21 prowl_22 prowl_23 prowl_24

// Extended scenes - - - - - - - - - - -
$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9

$frame decapb1 decapb2 decapb3 decapb4 decapb5 decapb6 decapb7 decapb8
$frame decapb9 decapb10 decapb11

$frame runb1 runb2 runb3 runb4 runb5 runb6

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9 walk10 walk11

$frame standb1 standb2 standb3 standb4 standb5 standb6
//- - - - - - - - - - - - - - - - - - -

/*
==============================================================================
SOLDIER CODE
==============================================================================
*/

// Update 6/7/2011:  Skins.
//float   SKIN_ARMY_GRUNT     = 0;
float   SKIN_ARMY_ROCKET    = 1;
float   SKIN_ARMY_ELITE     = 2;
float   SKIN_ARMY_ELITE_RED = 3;
float   SKIN_ARMY_CYBORG    = 4;

//===========
// SoldierCheckAttack
//
// The player is in view, so decide to move or launch an attack
// Returns FALSE if movement should continue
//============
float() SoldierCheckAttack =
{
    local   vector  spot1, spot2;    
    local   entity  targ;
    local   float   chance;

// PM:  Check these first to avoid unnecessary traceline calls.
    if (time < self.attack_finished)
        return FALSE;
    if (enemy_range == RANGE_FAR)
        return FALSE;

    targ = self.enemy;
    
// see if any entities are in the way of the shot
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;

    // PM:  Changed 3rd parm from FALSE to variable due to possible
    // fat flymissile arrows.
    traceline (spot1, spot2, self.height, self);

    if (self.ryuflags & RYU_REBEL)
    {   // Special AI for rebel archers in nsoe2.
        if (trace_fraction < 1)
        {   // don't have a clear shot
            if (DontHurtMe (trace_ent))
                return FALSE;   // Don't shoot whoever is in line of fire.

            // Otherwise, whoever is in the way is a potential enemy.
        }
        else
        {   // Target is non-solid.  It could be a wraith, downed zombie,
            // or something in spectator mode.
            // Unlike dragons, attack only if the target really is a ghost.
            if (targ.takedamage != DAMAGE_AIM)
                return FALSE;
            if (!IsGhost (targ))
                return FALSE;
        }
        // Alert is assumed to be on for rebels.

    // missile attack
        if (enemy_range == RANGE_MELEE)
            chance = 0.9;
        else
            chance = 0.4;   // Rebels are good marksmen, and must fire often.
    }
    else
    {   // Standard grunt AI.
        if (trace_inopen && trace_inwater)
            return FALSE;       // sight line crossed contents
        // PM:  Grunts are stupid, so never shoot if something is in the way.
        if (trace_ent != targ)
            return FALSE;       // don't have a clear shot

    // missile attack
        if (enemy_range == RANGE_MELEE)
            chance = 0.9;
        else if (enemy_range == RANGE_NEAR)
            chance = 0.4;
        else if (enemy_range == RANGE_MID)
            chance = 0.05;
        else if (enemy_range == RANGE_LONG)     // PM:  NEW!
            chance = 0.01;
        else
            chance = 0;
    }
            
    if (random () < chance)
    {
        if (self.ryuflags & RYU_REBEL)
            chance = 2 * random();
        else if ((self.classname == "monster_soldier") || (self.classname == "monster_army_rocket"))
            chance = 2 * random();
        else
            chance = 1 + random();
        self.th_missile ();
        SUB_AttackFinished (chance);
        if (random() < 0.3)
            self.lefty = !self.lefty;

        return TRUE;
    }

    return FALSE;
};


//=============================================================================

void() army_idle_sound =
{
    if (random() < 0.2)
    {
        if (self.classname == "monster_goblin")
        {
            if (random() < 0.7)
                sound (self, CHAN_VOICE, "goblin/idle1.wav", 1, ATTN_IDLE);
            else
                sound (self, CHAN_VOICE, "goblin/idle2.wav", 1, ATTN_IDLE);
        }
        else if (self.classname == "monster_bandit")
        {
            if (!(self.ryuflags & RYU_REBEL))
                sound (self, CHAN_VOICE, "soldier/idle.wav", 1, ATTN_IDLE);
        }
        else
            sound (self, CHAN_VOICE, "soldier/idle.wav", 1, ATTN_IDLE);
    }
};

// STAND - - - - - - - - - - - - - - - -
void() army_stand1  =[ $stand1,  army_stand2 ] {ai_stand();};
void() army_stand2  =[ $stand2,  army_stand3 ] {ai_stand();};
void() army_stand3  =[ $stand3,  army_stand4 ] {ai_stand();};
void() army_stand4  =[ $stand4,  army_stand5 ] {ai_stand();};
void() army_stand5  =[ $stand5,  army_stand6 ] {ai_stand();};
void() army_stand6  =[ $stand6,  army_stand7 ] {ai_stand();};
void() army_stand7  =[ $stand7,  army_stand8 ] {ai_stand();};
void() army_stand8  =[ $stand8,  army_stand1 ] {ai_stand();};

// Alternative scene for super grunt.
void() army_standb1  =[ $standb1,  army_standb2 ] {ai_stand();};
void() army_standb2  =[ $standb2,  army_standb3 ] {ai_stand();};
void() army_standb3  =[ $standb3,  army_standb4 ] {ai_stand();};
void() army_standb4  =[ $standb4,  army_standb5 ] {ai_stand();};
void() army_standb5  =[ $standb5,  army_standb6 ] {ai_stand();};
void() army_standb6  =[ $standb6,  army_standb7 ] {ai_stand();};
void() army_standb7  =[ $standb1,  army_standb1 ] {ai_stand();};

// WALK - - - - - - - - - - - - - - - -
void() army_walk1   =[ $prowl_1,    army_walk2  ] {army_idle_sound(); ai_walk(1);};
void() army_walk2   =[ $prowl_2,    army_walk3  ] {ai_walk(1);};
void() army_walk3   =[ $prowl_3,    army_walk4  ] {ai_walk(1);};
void() army_walk4   =[ $prowl_4,    army_walk5  ] {ai_walk(1);};
void() army_walk5   =[ $prowl_5,    army_walk6  ] {ai_walk(2);};
void() army_walk6   =[ $prowl_6,    army_walk7  ] {ai_walk(3);};
void() army_walk7   =[ $prowl_7,    army_walk8  ] {ai_walk(4);};
void() army_walk8   =[ $prowl_8,    army_walk9  ] {ai_walk(4);};
void() army_walk9   =[ $prowl_9,    army_walk10 ] {ai_walk(2);};
void() army_walk10  =[ $prowl_10,   army_walk11 ] {ai_walk(2);};
void() army_walk11  =[ $prowl_11,   army_walk12 ] {ai_walk(2);};
void() army_walk12  =[ $prowl_12,   army_walk13 ] {M_Step(); ai_walk(1);};
void() army_walk13  =[ $prowl_13,   army_walk14 ] {ai_walk(0);};
void() army_walk14  =[ $prowl_14,   army_walk15 ] {ai_walk(1);};
void() army_walk15  =[ $prowl_15,   army_walk16 ] {ai_walk(1);};
void() army_walk16  =[ $prowl_16,   army_walk17 ] {ai_walk(1);};
void() army_walk17  =[ $prowl_17,   army_walk18 ] {ai_walk(3);};
void() army_walk18  =[ $prowl_18,   army_walk19 ] {ai_walk(3);};
void() army_walk19  =[ $prowl_19,   army_walk20 ] {ai_walk(3);};
void() army_walk20  =[ $prowl_20,   army_walk21 ] {ai_walk(3);};
void() army_walk21  =[ $prowl_21,   army_walk22 ] {ai_walk(2);};
void() army_walk22  =[ $prowl_22,   army_walk23 ] {ai_walk(1);};
void() army_walk23  =[ $prowl_23,   army_walk24 ] {ai_walk(1);};
void() army_walk24  =[ $prowl_24,   army_walk1  ] {M_Step(); ai_walk(1);};

// Alternative scene for super grunt.
void() army_walkb1   =[ $walk1,    army_walkb2  ] {
    if (random() < 0.1)
        sound (self, CHAN_VOICE, "soldier/idle.wav", 1, ATTN_IDLE);
    ai_walk(5);
};
void() army_walkb2   =[ $walk2,    army_walkb3  ] {M_Step(); ai_walk(5);};
void() army_walkb3   =[ $walk3,    army_walkb4  ] {ai_walk(4);};
void() army_walkb4   =[ $walk4,    army_walkb5  ] {ai_walk(2);};
void() army_walkb5   =[ $walk5,    army_walkb6  ] {ai_walk(8);};
void() army_walkb6   =[ $walk6,    army_walkb7  ] {ai_walk(5);};
void() army_walkb7   =[ $walk7,    army_walkb8  ] {M_Step(); ai_walk(8);};
void() army_walkb8   =[ $walk8,    army_walkb9  ] {ai_walk(4);};
void() army_walkb9   =[ $walk9,    army_walkb10 ] {ai_walk(3);};
void() army_walkb10  =[ $walk10,   army_walkb11 ] {ai_walk(6);};
void() army_walkb11  =[ $walk11,   army_walkb1  ] {ai_walk(6);};

// RUN - - - - - - - - - - - - - - - - -
void() army_run1  =[ $run1,  army_run2  ] {army_idle_sound(); ai_run(11);};
void() army_run2  =[ $run2,  army_run3  ] {ai_run(15);};
void() army_run3  =[ $run3,  army_run4  ] {M_Step(); ai_run(10);};
void() army_run4  =[ $run4,  army_run5  ] {ai_run(10);};
void() army_run5  =[ $run5,  army_run6  ] {ai_run(8);};
void() army_run6  =[ $run6,  army_run7  ] {ai_run(15);};
void() army_run7  =[ $run7,  army_run8  ] {M_Step(); ai_run(10);};
void() army_run8  =[ $run8,  army_run1  ] {ai_run(8);};

// For super grunt.
void() army_runb1  =[ $runb1,  army_runb2  ] {
    if (random() < 0.1)
        sound (self, CHAN_VOICE, "soldier/idle.wav", 1, ATTN_IDLE);
    ai_run(20);
};
void() army_runb2  =[ $runb2,  army_runb3  ] {ai_run(16);};
void() army_runb3  =[ $runb3,  army_runb4  ] {M_Step(); ai_run(9);};
void() army_runb4  =[ $runb4,  army_runb5  ] {ai_run(20);};
void() army_runb5  =[ $runb5,  army_runb6  ] {M_Step(); ai_run(16);};
void() army_runb6  =[ $runb6,  army_runb1  ] {ai_run(9);};

// Rocketter special movement.
void(float d) army_back = {ai_face ();  ai_back (d);};
void() army_back1  =[ $run1,  army_back2  ] {army_idle_sound(); army_back(11);};
void() army_back2  =[ $run2,  army_back3  ] {army_back(15);};
void() army_back3  =[ $run3,  army_back4  ] {M_Step(); army_back(10);};
void() army_back4  =[ $run4,  army_back5  ] {army_back(10);};
void() army_back5  =[ $run5,  army_back6  ] {army_back(8);};
void() army_back6  =[ $run6,  army_back7  ] {army_back(15);};
void() army_back7  =[ $run7,  army_back8  ] {M_Step(); army_back(10);};
void() army_back8  =[ $run8,  army_run    ] {army_back(8);};

void() army_run =
{
    if (self.skin >= SKIN_ARMY_ELITE)   //(self.classname == "monster_soldier")
        {army_runb1();  return;}
    army_run1 ();
};

// ATTACK - - - - - - - - - - - - - - -
void() army_fire =
{
    local   vector  org, dir, err;

    ai_face ();

    makevectors (self.angles);
    if (self.war & WARx_USES_SHELLS)
    {
    // fire somewhat behind the player, so a dodging player is harder to hit
        org = self.origin;
        dir = self.enemy.origin - self.enemy.velocity*0.2;
    }
    else
    {
        dir = (v_forward*12) + (v_right*7) + (v_up*14);
        org = self.origin + dir;
        dir = self.enemy.origin + dir;
    }
    if (self.war & WARx_AI_RAPID_FIRE)
        err = '1.2 0.9 0';
    else
        err = '0 0 0';
    if (self.classname == "monster_army_rocket")
        if (self.war == WAR_SIDEWINDER)
        {   // Emulate rocketter's missile.
            if (!visible (self.enemy))
                dir = self.dest;
            M_AccelRocket (org, dir);
            M_Reload (2);       // Quoth enforces two second delay, usually.
            return;
        }
    if (self.classname == "monster_army_cyborg")
    {
// FIXME:  This uses direction instead of aimpoint.  Update attack code.
//  For now, Qexpo is near, and I have more urgent things to fix.
//  Here's a hack to keep things running until I can fix this properly.
        if ((self.war == WAR_SUPER_NAILGUN) || (self.war == WAR_BLASTER))
        {
            local   float   frac;

            self.effects = self.effects | EF_MUZZLEFLASH;
            frac = 1 - ((self.shots_left - 1) / self.shots_max); //Frac_ShotDown ();
            dir = Maim_LineEx (org, dir, 1000, self.enemy, err, frac);
            if (self.war == WAR_SUPER_NAILGUN)
            {
                sound (self, CHAN_WEAPON, "weapons/spike2.wav", 1, ATTN_NORM);
                launch_superspike (org, dir);
            }
            else
            {
                sound (self, CHAN_WEAPON, "me/me_fire1.wav", 1, ATTN_NORM);
                Needle_Launch (org, dir, 3);
            }
            M_Reload (0.1);

            return;
        }
    }
    M_Attack (self.war, org, dir, world, err, TRUE);
};

// Fire start.
void() army_fires =
{
    local   float   ls;

    ls = FALSE;
    if (self.war & WARx_SHAFT)
        if (self.ammo_cells > 0)
            if (!self.punked)
                ls = TRUE;
    army_fire();
    if (ls)     // Play thunder if firing a lightning weapon.
        M_Lstart();
};
//----------------
void() army_face =
{
    ai_face ();
    if (self.skin == SKIN_ARMY_CYBORG)
        ai_run_strafe(16);
    else if (self.skin >= SKIN_ARMY_ELITE)
        ai_run_strafe(8);
};
void() army_reload1  =[ $load1,   army_reload2   ] {};
void() army_reload2  =[ $load2,   army_reload3   ] {ai_back(2);};
void() army_reload3  =[ $load3,   army_reload4   ] {ai_back(2);};
void() army_reload4  =[ $load4,   army_reload5   ] {};
void() army_reload5  =[ $load5,   army_reload6   ] {};
void() army_reload6  =[ $load6,   army_reload7   ] {ai_forward(3);};
void() army_reload7  =[ $load7,   army_reload8   ] {ai_forward(1);};
void() army_reload8  =[ $load8,   army_reload9   ] {sound (self, CHAN_WEAPON, "weapons/lock4.wav", 1, ATTN_NORM);};
void() army_reload9  =[ $load9,   army_reload10  ] {};
void() army_reload10 =[ $load10,  army_reload11  ] {};
void() army_reload11 =[ $load11,  army_run       ] {};
//----------------
void() army_atk1  =[ $shoot1,   army_atk2    ] {army_face();};
void() army_atk2  =[ $shoot2,   army_atk3    ] {army_face();};
void() army_atk3  =[ $shoot3,   army_atk4    ] {army_face();};
void() army_atk4  =[ $shoot4,   army_atk5    ] {army_face();};
void() army_atk5  =[ $shoot5,   army_atk6    ] {army_face();army_fires();};
void() army_atk6  =[ $shoot6,   army_atk7    ] {army_face();};
void() army_atk7  =[ $shoot7,   army_atk8    ] {army_face();SUB_CheckRefire (army_atk1);};
void() army_atk8  =[ $shoot8,   army_atk9    ] {army_face();};
void() army_atk9  =[ $shoot9,   army_run     ] {army_face();};
//----------------
//void() army_atkb8;
void() army_atkb1 =[ $shoot1,   army_atkb2   ] {army_face();};
void() army_atkb2 =[ $shoot2,   army_atkb3   ] {army_face();};
void() army_atkb3 =[ $shoot3,   army_atkb4   ] {army_face();};
void() army_atkb4 =[ $shoot4,   army_atkb5   ] {army_face();};
void() army_atkb5 =[ $shoot5,   army_atkb6   ] {army_face();army_fires();};
void() army_atkb6 =[ $shoot4,   army_atkb7   ] {army_face();army_fire();};
void() army_atkb7 =[ $shoot5,   army_atkb8   ] {army_face();army_fire();SUB_CheckRefire (army_atkb6);};
void() army_atkb8 =[ $shoot4,   army_run     ] {army_face();};
//----------------
void() army_clip;
void() army_stop  =[ $shoot4,   army_run     ] {
//    if (self.count >= 5)
//        {self.count = 0;  army_reload1 ();  return;}
    if (self.skin != SKIN_ARMY_CYBORG)
        if (random() < 0.25)
            {army_reload1 ();  return;}
    army_face();
};
void() army_atkc1 =[ $shoot1,   army_atkc2   ] {army_face();};
void() army_atkc2 =[ $shoot2,   army_atkc3   ] {army_face();};
void() army_atkc3 =[ $shoot3,   army_atkc5   ] {army_face();};
void() army_atkc4 =[ $shoot4,   army_atkc5   ] {army_face();};
void() army_atkc5 =[ $shoot5,   army_atkc6   ] {army_face(); army_fires(); army_clip();};
void() army_atkc6 =[ $shoot4,   army_atkc7   ] {army_face(); army_fire(); army_clip();};
void() army_atkc7 =[ $shoot5,   army_atkc6   ] {army_face(); army_fire(); army_clip();};
void() army_echo =
{
    self.shots_left = 2;
    if ( self.frame == $shoot5 )
        army_atkc6 ();
    else
        army_atkc7 ();
};
void() army_clip =
{
    if (self.shots_left > 0)
        self.shots_left = self.shots_left - 1;

    M_Break (army_stop);
    if (self.shots_left < 1)
    {
        self.think = army_stop;
        SUB_CheckRefire (army_echo);
    }
};
//----------------
float() army_abort =
{
    if ((self.war & WARx_USES_ROCKETS) || (self.war == WAR_PLASMA_GUN))
    {
        local   float   dist;

        if (self.war == WAR_CRUISE_MISSILE)
            dist = 120;     // Still too short, but alas...
        else if ((self.war == WAR_ROCKET_LAUNCHER) || (self.war == WAR_GRENADE_LAUNCHER))
            dist = 100;
        else
            dist = 80;
        if (InMeleeRange (dist))
            return TRUE;    // Too close.
    }
    return FALSE;
};
void() army_rem =
{
    if (visible (self.enemy))
        self.dest = self.enemy.origin;
};
void() army_load1  =[ $load1,   army_load2   ] {ai_face(); army_rem();};
void() army_load2  =[ $load2,   army_load3   ] {ai_face(); ai_back(2); army_rem();};
void() army_load3  =[ $load3,   army_load4   ] {ai_face(); ai_back(2); army_rem();};
void() army_load4  =[ $load4,   army_load5   ] {ai_face(); army_rem();};
void() army_load5  =[ $load5,   army_load6   ] {ai_face(); army_rem();};
void() army_load6  =[ $load6,   army_load7   ] {ai_face(); ai_forward(3); army_rem();};
void() army_load7  =[ $load7,   army_load8   ] {ai_face(); ai_forward(1); army_rem();};
void() army_load8  =[ $load8,   army_load9   ] {ai_face(); army_rem(); sound (self, CHAN_WEAPON, "weapons/lock4.wav", 1, ATTN_NORM);};
void() army_load9  =[ $load9,   army_load10  ] {ai_face(); army_rem();};
void() army_load10 =[ $load10,  army_load11  ] {ai_face(); army_rem();};
void() army_load11 =[ $load11,  army_atk5    ] {ai_face(); army_rem();
// Short curcuit check for rocketter.
    if (self.classname == "monster_army_rocket")
    {   // FIXME:  Clear shot, like archer lords.
        if (army_abort ())
            self.think = army_back1;
    }
};
//----------------
void() army_fatk1 =[ $shoot2,   army_fatk2   ] {army_face();};
void() army_fatk2 =[ $shoot3,   army_atk5    ] {army_face();};
//----------------
void() army_fatkb1 =[ $shoot2,   army_fatkb2  ] {army_face();};
void() army_fatkb2 =[ $shoot3,   army_atkb5   ] {army_face();};
//----------------
void() army_fatkc1 =[ $shoot2,   army_fatkc2  ] {army_face();};
void() army_fatkc2 =[ $shoot3,   army_atkc5   ] {army_face();};
//----------------
void() army_attack =
{
    local   float   fast;
    local   float   ow;

// FIXME:  Attack code in 'mong.qc' needs a revision.
    if (self.skin == SKIN_ARMY_CYBORG)
        {fast = TRUE;  self.shots_max = self.shots_left = 3;}
    else if (self.skin >= SKIN_ARMY_ELITE)      //(self.classname == "monster_soldier")
        {fast = TRUE;  self.shots_left = 4;}
    else
        {fast = FALSE;  self.shots_left = 3;}

    ow = self.war;
    M_SelectGun ();
    if (ow == self.war)
        self.count = self.count + 1;
    if (self.war & WARx_USES_ROCKETS)
    {
        if (self.classname == "monster_army_rocket")
        {   // Emulate Rocketter AI.
            if (army_abort ())
                {army_back1 ();  return;}   // Too close.
        }
        army_load1 ();
    }
    else if (self.skin == SKIN_ARMY_CYBORG)
    {
        if (self.war & (WARx_AI_RAPID_FIRE | WAR_BLASTER))
            army_atkc2();
        else if (self.war)
            army_fatk1 ();
    }
    else if (fast)
    {
        if (self.war & WARx_AI_RAPID_FIRE)
            army_atkc2();   //fatkb1 ();
        else if (self.war)
            army_fatk1 ();
    }
    else if (self.war & WARx_AI_RAPID_FIRE)
        army_atkb1 ();
    else if (self.war || self.ammo_shells)
        army_atk1 ();
// No weapon?!  Leave the think function alone.
};


// PAIN - - - - - - - - - - - - - - - -
void() army_pain1   =[ $pain1,   army_pain2   ] {};
void() army_pain2   =[ $pain2,   army_pain3   ] {};
void() army_pain3   =[ $pain3,   army_pain4   ] {};
void() army_pain4   =[ $pain4,   army_pain5   ] {};
void() army_pain5   =[ $pain5,   army_pain6   ] {};
void() army_pain6   =[ $pain6,   army_run     ] {ai_back(1);};

void() army_painb1  =[ $painb1,  army_painb2  ] {};
void() army_painb2  =[ $painb2,  army_painb3  ] {ai_forward(13);};
void() army_painb3  =[ $painb3,  army_painb4  ] {ai_forward(9);};
void() army_painb4  =[ $painb4,  army_painb5  ] {};
void() army_painb5  =[ $painb5,  army_painb6  ] {};
void() army_painb6  =[ $painb6,  army_painb7  ] {};
void() army_painb7  =[ $painb7,  army_painb8  ] {};
void() army_painb8  =[ $painb8,  army_painb9  ] {};
void() army_painb9  =[ $painb9,  army_painb10 ] {};
void() army_painb10 =[ $painb10, army_painb11 ] {};
void() army_painb11 =[ $painb11, army_painb12 ] {};
void() army_painb12 =[ $painb12, army_painb13 ] {ai_back(2);};
void() army_painb13 =[ $painb13, army_painb14 ] {};
void() army_painb14 =[ $painb14, army_run     ] {};

void() army_painc1  =[ $painc1,  army_painc2  ] {};
void() army_painc2  =[ $painc2,  army_painc3  ] {ai_back(1);};
void() army_painc3  =[ $painc3,  army_painc4  ] {};
void() army_painc4  =[ $painc4,  army_painc5  ] {};
void() army_painc5  =[ $painc5,  army_painc6  ] {ai_forward(1);};
void() army_painc6  =[ $painc6,  army_painc7  ] {ai_forward(1);};
void() army_painc7  =[ $painc7,  army_painc8  ] {};
void() army_painc8  =[ $painc8,  army_painc9  ] {ai_back(1);};
void() army_painc9  =[ $painc9,  army_painc10 ] {ai_forward(4);};
void() army_painc10 =[ $painc10, army_painc11 ] {ai_forward(3);};
void() army_painc11 =[ $painc11, army_painc12 ] {ai_forward(6);};
void() army_painc12 =[ $painc12, army_painc13 ] {ai_forward(8);};
void() army_painc13 =[ $painc13, army_run     ] {};

void(entity attacker, float damage) army_pain =
{
    if (self.pain_finished > time)
        return;

    if (SUB_IgnorePain ())
    {
        self.pain_finished = time + 0.6;
        sound (self, CHAN_VOICE, "soldier/pain1.wav", 1, ATTN_NORM);
        return;
    }

    local   float   r;
    
    r = random();

    if ((r < 0.2) || (skill >= 3))
    {
        self.pain_finished = time + 0.6;
        army_pain1 ();
        sound (self, CHAN_VOICE, "soldier/pain1.wav", 1, ATTN_NORM);
    }
    else if (r < 0.6)
    {
        self.pain_finished = time + 1.1;
        army_painb1 ();
        sound (self, CHAN_VOICE, "soldier/pain2.wav", 1, ATTN_NORM);
    }
    else
    {
        self.pain_finished = time + 1.1;
        army_painc1 ();
        sound (self, CHAN_VOICE, "soldier/pain2.wav", 1, ATTN_NORM);
    }
    damage = damage;  attacker = attacker;  // FrikQcc warning squelch!
};

void(entity attacker, float damage) army_rocket_pain =
{
    if ( (( self.frame >= $load1 ) && ( self.frame <= $load11 ))
    || (( self.frame >= $shoot1 ) && ( self.frame <= $shoot9 )) )
        return;     // Cannot flinch during an attack.

    army_pain (attacker, damage);
};


// DEATH - - - - - - - - - - - - - - - -
void() army_decap1   =[ $deathb1,   army_decap2   ] {blood_spray('-8 0 24', 1);};
void() army_decap2   =[ $deathb2,   army_decap3   ] {blood_spray('-16 0 21', 2);};
void() army_decap3   =[ $deathb3,   army_decap4   ] {blood_spray('-22 0 21', 1);monster_drop();};
void() army_decap4   =[ $deathb4,   army_decap5   ] {blood_spray('-23 0 14', 1);};
void() army_decap5   =[ $deathb5,   army_decap6   ] {};
void() army_decap6   =[ $deathb6,   army_decap7   ] {};
void() army_decap7   =[ $deathb7,   army_decap8   ] {};
void() army_decap8   =[ $deathb8,   army_decap9   ] {};
void() army_decap9   =[ $deathb9,   decap_end     ] {};

void() army_decapb1  =[ $decapb1,  army_decapb2  ] {blood_spray('-11 0 23', 1);};
void() army_decapb2  =[ $decapb2,  army_decapb3  ] {ai_back(5); blood_spray('-16 0 21', 2);};
void() army_decapb3  =[ $decapb3,  army_decapb4  ] {monster_drop();ai_back(4); blood_spray('-19 -1 18', 1);};
void() army_decapb4  =[ $decapb4,  army_decapb5  ] {ai_back(13); blood_spray('-22 0 15', 1);};
void() army_decapb5  =[ $decapb5,  army_decapb6  ] {ai_back(3);};
void() army_decapb6  =[ $decapb6,  army_decapb7  ] {ai_back(4);};
void() army_decapb7  =[ $decapb7,  army_decapb8  ] {};
void() army_decapb8  =[ $decapb8,  army_decapb9  ] {};
void() army_decapb9  =[ $decapb9,  army_decapb10 ] {};
void() army_decapb10 =[ $decapb10, army_decapb11 ] {};
void() army_decapb11 =[ $decapb11, decap_end     ] {};

void() army_die1   =[ $death1,   army_die2   ] {};
void() army_die2   =[ $death2,   army_die3   ] {};
void() army_die3   =[ $death3,   army_die4   ] {monster_drop();};
void() army_die4   =[ $death4,   army_die5   ] {};
void() army_die5   =[ $death5,   army_die6   ] {};
void() army_die6   =[ $death6,   army_die7   ] {};
void() army_die7   =[ $death7,   army_die8   ] {};
void() army_die8   =[ $death8,   army_die9   ] {};
void() army_die9   =[ $death9,   army_die10  ] {};
void() army_die10  =[ $death10,  monster_end ] {};

void() army_cdie1  =[ $deathc1,  army_cdie2  ] {};
void() army_cdie2  =[ $deathc2,  army_cdie3  ] {ai_back(5);};
void() army_cdie3  =[ $deathc3,  army_cdie4  ] {monster_drop();ai_back(4);};
void() army_cdie4  =[ $deathc4,  army_cdie5  ] {ai_back(13);};
void() army_cdie5  =[ $deathc5,  army_cdie6  ] {ai_back(3);};
void() army_cdie6  =[ $deathc6,  army_cdie7  ] {ai_back(4);};
void() army_cdie7  =[ $deathc7,  army_cdie8  ] {};
void() army_cdie8  =[ $deathc8,  army_cdie9  ] {};
void() army_cdie9  =[ $deathc9,  army_cdie10 ] {};
void() army_cdie10 =[ $deathc10, army_cdie11 ] {};
void() army_cdie11 =[ $deathc11, monster_end ] {};

float(float dm) army_gib =
{
    if (self.health < self.gib_health)
    {
        sound (self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
        ThrowGib ("progs/gib1.mdl", dm);
        ThrowGib ("progs/gib2.mdl", dm);
        ThrowGib ("progs/gib3.mdl", dm);
        ThrowHead ("progs/h_guard.mdl", dm);
        return TRUE;
    }
    return FALSE;
};

void() army_die =
{
    if (Shattered ())
        return;
    if (Decapitated ())
    {
        if (random() < 0.5)
            monster_decap ('-8 0 24', army_decap1);
        else
            monster_decap ('-11 0 23', army_decapb1);
        return;
    }
// check for gib
    if (army_gib (self.health))
        return;

// regular death
    sound (self, CHAN_VOICE, "soldier/death1.wav", 1, ATTN_NORM);
    if (random() < 0.5)
        army_die1 ();
    else
        army_cdie1 ();
};

// REVIVE - - - - - - - - - - - - - - -
void() army_revive1   =[ $death9,   army_revive2   ] {};
void() army_revive2   =[ $death8,   army_revive3   ] {};
void() army_revive3   =[ $death7,   army_revive4   ] {};
void() army_revive4   =[ $death6,   army_revive5   ] {};
void() army_revive5   =[ $death5,   army_revive6   ] {};
void() army_revive6   =[ $death4,   army_revive7   ] {};
void() army_revive7   =[ $death3,   army_revive8   ] {};
void() army_revive8   =[ $death2,   army_revive9   ] {};
void() army_revive9   =[ $death1,   army_run       ] {};

void() army_revivec1  =[ $deathc10, army_revivec2  ] {};
void() army_revivec2  =[ $deathc9,  army_revivec3  ] {};
void() army_revivec3  =[ $deathc8,  army_revivec4  ] {};
void() army_revivec4  =[ $deathc7,  army_revivec5  ] {};
void() army_revivec5  =[ $deathc6,  army_revivec6  ] {};
void() army_revivec6  =[ $deathc5,  army_revivec7  ] {};
void() army_revivec7  =[ $deathc4,  army_revivec8  ] {};
void() army_revivec8  =[ $deathc3,  army_revivec9  ] {};
void() army_revivec9  =[ $deathc2,  army_revivec10 ] {};
void() army_revivec10 =[ $deathc1,  army_run       ] {};

void() army_revive =
{
    if ( self.frame == $deathc11 )
        army_revivec1 ();
    else
        army_revive1 ();
};


//=============================================================================

// OTHER - - - - - - - - - - - - - - - -
void() monster_bandit;

void(float hello) army_sight =
{
    if (hello)
        sound (self, CHAN_VOICE, "soldier/idle.wav", 1, ATTN_NORM);
    else
        sound (self, CHAN_VOICE, "soldier/sight1.wav", 1, ATTN_NORM);
};

void() army_cache =
{
    precache_model ("progs/soldier.mdl");
    precache_model ("progs/h_guard.mdl");
    precache_model ("progs/gib1.mdl");
    precache_model ("progs/gib2.mdl");
    precache_model ("progs/gib3.mdl");

    precache_sound ("soldier/death1.wav");
    precache_sound ("soldier/idle.wav");
    precache_sound ("soldier/pain1.wav");
    precache_sound ("soldier/pain2.wav");
    precache_sound ("soldier/sight1.wav");
    precache_sound ("weapons/guncock.wav");     // Was 'soldier/sattck1.wav'.

    precache_sound ("player/udeath.wav");        // gib death
};

//============
// QUAKED monster_army (1 0 0) (-16 -16 -24) (16 16 40) Ambush Elite
//============
void() monster_army =
{
// Tronyn:  Goblins
    if (medieval)
    {
        if (soul_evil)
            {self.skin = 1;  self.classname = "monster_goblin";}
        else
            {self.skin = 0;  self.classname = "monster_bandit";}
        monster_bandit ();
        return;
    }
//- - - - - - - - -
    if (!self.spawned)
    {
        if (noenemies)
            {remove (self); return;}
        army_cache ();
    }

    local   float   hp, guns;

    if (self.spawnflags & 2)
    {   // Elite soldier.
        self.skin = SKIN_ARMY_ELITE;
        self.classname = "monster_soldier";
// DISABLED -- Was always ambush in OUM.
//        self.spawnflags = self.spawnflags | 1;
    }

    // FIXME:  Finalize cyborg weapon.
    if (self.skin == SKIN_ARMY_CYBORG)
        {hp = 200;  guns = WAR_SUPER_NAILGUN;}  //BLASTER;}
    else if (self.skin == SKIN_ARMY_ELITE_RED)
        {hp = 100;  guns = WAR_SUPER_SHOTGUN;}
    else if (self.skin == SKIN_ARMY_ELITE)
        {hp = 90;  guns = WAR_NAILGUN;}     // OUM uses 90.  I'd use 75...
    else if (self.skin == SKIN_ARMY_ROCKET)
        {hp = 60;  guns = WAR_SIDEWINDER;}
    else    // Default to standard grunt.
        {self.skin = 0;  hp = 30;  guns = WAR_SHOTGUN;}

    monster_form ("progs/soldier.mdl", '-16 -16 -24', '16 16 40', hp, -35);
    self.mdl_head   = "progs/h_guard.mdl";
    self.mass       = MASS_MEDIUM;

    if (self.skin >= SKIN_ARMY_ELITE)
    {
        self.attack_state   = AS_FLANKING;
        self.th_stand   = army_standb1;
        self.th_walk    = army_walkb1;
    }
    else
    {
        self.th_stand   = army_stand1;
        self.th_walk    = army_walk1;
    }
    self.th_run     = army_run;
    self.th_missile = army_attack;  //army_atk1;
    self.th_pain    = army_pain;
    self.th_die     = army_die;
    //- - - - - - - -
    self.th_check   = SoldierCheckAttack;
    self.th_sight   = army_sight;
    self.th_gib     = army_gib;
    self.th_revive  = army_revive;
    self.th_spawn   = monster_army;

    self.height     = FALSE;
    self.family     = FAM_ARMY;
    if (self.skin == SKIN_ARMY_ROCKET)
    {
        self.th_melee   = army_attack;      // Spam rockets or run away.
        self.th_pain    = army_rocket_pain;
        self.th_check   = world.null_func;
    }
    else if (self.skin == SKIN_ARMY_CYBORG)
        self.th_check   = world.null_func;
    else if (!self.skin)
        self.ryuflags   = RYU_INSANE;

    monster_guns (guns, 0, 0, 0, 0);

    if (soul_evil)
        monster_purse (2, 0.2, 0);

    monster_start ();
};

// Variant cleaner soldier.
void() monster_soldier = {self.skin = SKIN_ARMY_ELITE;  monster_army ();};

// Quoth's rocketeer.
void() monster_army_rocket = {self.skin = SKIN_ARMY_ROCKET;  monster_army ();};

// Quoth's rocketeer.
void() monster_army_cyborg = {self.skin = SKIN_ARMY_CYBORG;  monster_army ();};


//============================================================================
//  BANDIT / GOBLIN  (Soldier variant armed with a bow.)
//============================================================================

// ATTACK - - - - - - - - - - - - - - -
void() bandit_fire =
{
    local   vector  org, dir;
    local   float   artype;

    ai_face ();

    makevectors (self.angles);
    dir = (v_forward*9) + (v_right*6) + (v_up*16);
    org = self.origin + dir;
    dir = self.enemy.origin + dir;
//    if (ARROW_FLY)
        dir = Maim_Line (org, dir, AKARROW_SPEED_FLY, world, '0 0 0');
//    else
//        dir = Maim_Toss (org, dir, AKARROW_SPEED_TOSS, 200, world, '0 0 0');

    sound (self, CHAN_WEAPON, "archer/arrowg.wav", 1, ATTN_NORM);
    // PM:  Assumes skin is 0 or 1.  If 2+, rewrite!
    if (self.ryuflags & RYU_REBEL)
        artype = 0;     // Rebel arrows are stronger.
    else
        artype = 4 + (self.skin & 1);
    AkArrow_Launch (org, dir, artype);
};

// SoE:  Check if shot is blocked.  Used by rebel to avoid friendly fire.
void() bandit_short =
{
    if (self.ryuflags & RYU_REBEL)
    {
        local   vector  p1, p2;

        p1 = self.origin + self.view_ofs;
        p2 = self.enemy.origin + self.enemy.view_ofs;

        traceline (p1, p2, ARROW_FLY, self);
        if (trace_ent != self.enemy)
        if (trace_fraction < 1)
        if (trace_ent.flags & FLx_CREATURE)
        if (DontHurtMe (trace_ent))
        {
            StopSound (self, CHAN_WEAPON);
            self.think = army_run1;
        }
    }
};

// SoE:  Shoot scene modified from shooting a gun to drawing a bow.
void() bandit_atk1  =[ $shoot1,   bandit_atk2    ] {ai_face();};
void() bandit_atk2  =[ $shoot2,   bandit_atk3    ] {ai_face(); sound (self, CHAN_WEAPON, "archer/draw.wav", 1, ATTN_NORM);};
void() bandit_atk3  =[ $shoot3,   bandit_atk4    ] {ai_face();};
void() bandit_atk4  =[ $shoot4,   bandit_atk5    ] {ai_face();};
void() bandit_atk5  =[ $shoot5,   bandit_atk6    ] {ai_face();};
void() bandit_atk6  =[ $shoot5,   bandit_atk7    ] {ai_face();bandit_short();};
void() bandit_atk7  =[ $shoot6,   bandit_atk8    ] {ai_face();bandit_fire();};
void() bandit_atk8  =[ $shoot7,   army_run1      ] {ai_face();
    if (self.ryuflags & RYU_REBEL)
    {   // Rebels have their own refire check.
        if (self.enemy.health > 0)
        {
            if (random() < 0.4)
            {   // If shot cannot hit enemy, stop.
                local   vector  p1, p2;

                p1 = self.origin + self.view_ofs;
                p2 = self.enemy.origin + self.enemy.view_ofs;

                traceline (p1, p2, ARROW_FLY, self);
                if (trace_ent == self.enemy)
                {   // Refire.
                    if (self.attack_finished)
                        self.attack_finished = self.attack_finished + 0.5;
                    self.think = bandit_atk4;
                    return;
                }
            }
        }
    }
    else
        SUB_CheckRefire (bandit_atk3);
};
//void() bandit_atk8  =[ $shoot8,   bandit_atk9    ] {ai_face();};
//void() bandit_atk9  =[ $shoot9,   army_run1      ] {ai_face();};

// PAIN - - - - - - - - - - - - - - - -
void(entity attacker, float damage) bandit_pain =
{
    if (self.pain_finished > time)
        return;

    if (SUB_IgnorePain ())
    {
        self.pain_finished = time + 0.6;
        if (self.classname == "monster_bandit")
            sound (self, CHAN_VOICE, "soldier/pain1.wav", 1, ATTN_NORM);
        else
            sound (self, CHAN_VOICE, "goblin/pain1.wav", 1, ATTN_NORM);
        return;         // No pain anims in Nightmare.
    }

    local   float   r;
    local   float   p1, p2;
    
    r = random();
    if (self.ryuflags & RYU_REBEL)
        {p1 = 0.85; p2 = 0.925;}
    else
        {p1 = 0.2;  p2 = 0.6;}

    if ((r < p1) || (skill >= 3))
    {
        self.pain_finished = time + 0.6;
        army_pain1 ();
        if (self.classname == "monster_bandit")
            sound (self, CHAN_VOICE, "soldier/pain1.wav", 1, ATTN_NORM);
        else
            sound (self, CHAN_VOICE, "goblin/pain1.wav", 1, ATTN_NORM);
    }
    else if (r < p2)
    {
        self.pain_finished = time + 1.1;
        army_painb1 ();
        if (self.classname == "monster_bandit")
            sound (self, CHAN_VOICE, "soldier/pain2.wav", 1, ATTN_NORM);
        else
            sound (self, CHAN_VOICE, "goblin/pain1.wav", 1, ATTN_NORM);
    }
    else
    {
        self.pain_finished = time + 1.1;
        army_painc1 ();
        if (self.classname == "monster_bandit")
            sound (self, CHAN_VOICE, "soldier/pain2.wav", 1, ATTN_NORM);
        else
            sound (self, CHAN_VOICE, "goblin/pain2.wav", 1, ATTN_NORM);
    }
    damage = damage;  attacker = attacker;  // FrikQcc warning squelch!
};

// DEATH - - - - - - - - - - - - - - - -
float(float dm) bandit_gib =
{
    if (self.health < self.gib_health)
    {
        sound (self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
        ThrowGib ("progs/gib1.mdl", dm);
        ThrowGib ("progs/gib2.mdl", dm);
        ThrowGib ("progs/gib3.mdl", dm);
        ThrowHead ("progs/h_bandit.mdl", dm);
        return TRUE;
    }
    return FALSE;
};

void() bandit_die =
{
    if (Shattered ())
        return;
    if (Decapitated ())
    {
        if (random() < 0.5)
            monster_decap ('-8 0 24', army_decap1);
        else
            monster_decap ('-11 0 23', army_decapb1);
        return;
    }
// check for gib
    if (bandit_gib (self.health))
        return;

// regular death
    if (self.classname == "monster_bandit")
        sound (self, CHAN_VOICE, "soldier/death1.wav", 1, ATTN_NORM);
    else
    {
        if (random() < 0.5)
            sound (self, CHAN_VOICE, "goblin/death1.wav", 1, ATTN_NORM);
        else
            sound (self, CHAN_VOICE, "goblin/death2.wav", 1, ATTN_NORM);
    }
    if (random() < 0.5)
        army_die1 ();
    else
        army_cdie1 ();
};


// OTHER - - - - - - - - - - - - - - - -
void(float hello) bandit_sight =
{
    if (self.classname == "monster_bandit")
    {
        if (!hello)
            sound (self, CHAN_VOICE, "soldier/sight1.wav", 1, ATTN_NORM);
        else if (!(self.ryuflags & RYU_REBEL))
            sound (self, CHAN_VOICE, "soldier/idle.wav", 1, ATTN_NORM);
        return;
    }
    if (hello)
        sound (self, CHAN_VOICE, "goblin/idle1.wav", 1, ATTN_NORM);
    else
        sound (self, CHAN_VOICE, "goblin/sight1.wav", 1, ATTN_NORM);
};

void() bandar_cache =
{
    precache_model ("progs/drake/arrow.mdl");
    precache_model ("progs/drake/bandit.mdl");
    precache_model ("progs/drake/h_bandit.mdl");

    precache_sound ("archer/arrow1.wav");
    precache_sound ("archer/arrow2.wav");
    precache_sound ("archer/arrow3.wav");
    precache_sound ("archer/arrowbrk.wav");
    precache_sound ("archer/arrowg.wav");
    precache_sound ("archer/draw.wav");
//    precache_sound ("player/udeath.wav");        // gib death
};

void() bandit_cache =
{
    bandar_cache ();
    precache_sound ("soldier/death1.wav");
    precache_sound ("soldier/idle.wav");
    precache_sound ("soldier/pain1.wav");
    precache_sound ("soldier/pain2.wav");
    precache_sound ("soldier/sight1.wav");
};

void() goblin_cache =
{
    bandar_cache ();
    precache_sound ("goblin/death1.wav");
    precache_sound ("goblin/death2.wav");
    precache_sound ("goblin/idle1.wav");
    precache_sound ("goblin/idle2.wav");
    precache_sound ("goblin/pain1.wav");
    precache_sound ("goblin/pain2.wav");
    precache_sound ("goblin/sight1.wav");
};

//============
// QUAKED monster_bandit (1 0 0) (-16 -16 -24) (16 16 40) Ambush
//============
void() monster_bandit =
{    
    if (!self.spawned)
    {
        if (noenemies)
            {remove (self); return;}
        if (self.skin)
            goblin_cache ();
        else
            bandit_cache ();
    }

    monster_form ("progs/bandit.mdl", '-16 -16 -24', '16 16 40', 30, -35);
    self.mdl_head   = "progs/h_bandit.mdl";
    self.mass       = MASS_MEDIUM;

    self.th_stand   = army_stand1;      // Same frames as base grunt.
    self.th_walk    = army_walk1;
    self.th_run     = army_run1;
    self.th_missile = bandit_atk1;
    self.th_pain    = bandit_pain;
    self.th_die     = bandit_die;
    //- - - - - - - -
    self.th_check   = SoldierCheckAttack;
    self.th_sight   = bandit_sight;
    self.th_gib     = bandit_gib;
    self.th_revive  = army_revive;
    self.th_spawn   = monster_bandit;

    self.height     = 2;
    self.ryuflags   = RYU_INSANE;
    if (soul_evil)
        monster_guns (0, 0, 10, 0, 0);
    monster_purse (2, 0.2, 0);      // Bandits may drop some coins.
// SoE:  Modified stats for nsoe2 rebel fighters.

    if (self.deathstring == "")  self.deathstring = " was shot by a Bandit\n";

    if (self.classname == "monster_goblin") {
        if (self.deathstring == "")  self.deathstring = " was shot by a Goblin\n";
    }

    if (!self.skin && (self.spawnflags & SPAWN_WHITE) && !self.spawned)
    {
        if (self.health < 100)
            {self.health = self.max_health = 100;}
        self.ryuflags = RYU_ALERT | RYU_REBEL;
        if (self.deathstring == "")  self.deathstring = " was shot by friendly fire\n";
    }



    
//- - - - - - - - -

    monster_start ();
};

void() monster_goblin = {self.skin = 1;  monster_bandit ();};


//===========================/  END OF FILE  /===========================//
