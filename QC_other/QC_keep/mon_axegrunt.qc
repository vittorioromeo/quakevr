/*
==============================================================================

AXE GRUNT
Suitable replacement for knight in base maps.  Grunts a lot.

==============================================================================
*/

$cd id1/models/player_4
$origin 0 -6 24
$base base        
$skin skin

//
// running
//
$frame axrun1 axrun2 axrun3 axrun4 axrun5 axrun6
$frame rockrun1 rockrun2 rockrun3 rockrun4 rockrun5 rockrun6

//
// standing
//
$frame stand1 stand2 stand3 stand4 stand5
$frame axstnd1 axstnd2 axstnd3 axstnd4 axstnd5 axstnd6
$frame axstnd7 axstnd8 axstnd9 axstnd10 axstnd11 axstnd12


//
// pain
//
$frame axpain1 axpain2 axpain3 axpain4 axpain5 axpain6
$frame pain1 pain2 pain3 pain4 pain5 pain6


//
// death
//

$frame axdeth1 axdeth2 axdeth3 axdeth4 axdeth5 axdeth6
$frame axdeth7 axdeth8 axdeth9
$frame deatha1 deatha2 deatha3 deatha4 deatha5 deatha6 deatha7 deatha8
$frame deatha9 deatha10 deatha11
$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9
$frame deathc1 deathc2 deathc3 deathc4 deathc5 deathc6 deathc7 deathc8
$frame deathc9 deathc10 deathc11 deathc12 deathc13 deathc14 deathc15
$frame deathd1 deathd2 deathd3 deathd4 deathd5 deathd6 deathd7
$frame deathd8 deathd9
$frame deathe1 deathe2 deathe3 deathe4 deathe5 deathe6 deathe7
$frame deathe8 deathe9

//
// attacks
//
$frame nailatt1 nailatt2
$frame light1 light2
$frame rockatt1 rockatt2 rockatt3 rockatt4 rockatt5 rockatt6
$frame shotatt1 shotatt2 shotatt3 shotatt4 shotatt5 shotatt6
$frame axatt1 axatt2 axatt3 axatt4 axatt5 axatt6
$frame axattb1 axattb2 axattb3 axattb4 axattb5 axattb6
$frame axattc1 axattc2 axattc3 axattc4 axattc5 axattc6
$frame axattd1 axattd2 axattd3 axattd4 axattd5 axattd6


void()    axeg_stand1    =[    $axstnd1,    axeg_stand2    ] {ai_stand();};
void()    axeg_stand2    =[    $axstnd2,    axeg_stand3    ] {ai_stand();};
void()    axeg_stand3    =[    $axstnd3,    axeg_stand4    ] {ai_stand();};
void()    axeg_stand4    =[    $axstnd4,    axeg_stand5    ] {ai_stand();};
void()    axeg_stand5    =[    $axstnd5,    axeg_stand6    ] {ai_stand();};
void()    axeg_stand6    =[    $axstnd6,    axeg_stand7    ] {ai_stand();};
void()    axeg_stand7    =[    $axstnd7,    axeg_stand8    ] {ai_stand();};
void()    axeg_stand8    =[    $axstnd8,    axeg_stand9    ] {ai_stand();};
void()    axeg_stand9    =[    $axstnd9,    axeg_stand10    ] {ai_stand();};
void()    axeg_stand10    =[    $axstnd10,    axeg_stand11    ] {ai_stand();};
void()    axeg_stand11    =[    $axstnd11,    axeg_stand12    ] {ai_stand();};
void()    axeg_stand12    =[    $axstnd12,    axeg_stand1    ] {ai_stand();};

void()    axeg_run1    =[    $axrun1,        axeg_run2    ] {
if (random() < 0.2)
    sound (self, CHAN_VOICE, "knight/idle.wav", 1,  ATTN_IDLE);
ai_run(25);};
void()    axeg_run2    =[    $axrun2,        axeg_run3    ] {ai_run(18); monster_footstep(FALSE);};
void()    axeg_run3    =[    $axrun3,        axeg_run4    ] {ai_run(12);};
void()    axeg_run4    =[    $axrun4,        axeg_run5    ] {ai_run(25);};
void()    axeg_run5    =[    $axrun5,        axeg_run6    ] {ai_run(18);monster_footstep(FALSE);};
void()    axeg_run6    =[    $axrun6,        axeg_run1    ] {ai_run(12);};

//===========================================================================

void( vector dir ) axeg_chop =
{
    local vector    delta, toss;
    local float     ldmg;

    if (!self.enemy)
        return;        // removed before stroke
        
    ai_charge_side();
    
    delta = self.enemy.origin - self.origin;

    if (vlen(delta) > 60)
        return;
    if (!CanDamage (self.enemy, self))
        return;
    ldmg = (random() + random() + random()) * 2;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);

    toss = v_forward * dir_x + v_right * dir_y + v_up * dir_z;
    self.enemy.velocity = self.enemy.velocity + toss;
}


void()    axe_runatka1    =[    $axatt1,        axe_runatka2    ] {
if (random() > 0.5)
    sound (self, CHAN_WEAPON, "knight/sword2.wav", 1, ATTN_NORM);
else
    sound (self, CHAN_WEAPON, "knight/sword1.wav", 1, ATTN_NORM);
ai_charge(30);
};
void()    axe_runatka2    =[    $axatt2,        axe_runatka3    ] {ai_charge(20);};
void()    axe_runatka3    =[    $axatt3,        axe_runatka4    ] {ai_charge(10); axeg_chop('0 0 0');};
void()    axe_runatka4    =[    $axatt4,        axe_runatka5    ] {axeg_chop('50 -20 10');};
void()    axe_runatka5    =[    $axatt5,        axe_runatka6    ] {axeg_chop('100 -40 10');};
void()    axe_runatka6    =[    $axatt6,        axeg_run1    ] {ai_charge_side();};

void()    axe_runatkb1    =[    $axattb1,        axe_runatkb2    ] {
if (random() > 0.5)
    sound (self, CHAN_WEAPON, "knight/sword2.wav", 1, ATTN_NORM);
else
    sound (self, CHAN_WEAPON, "knight/sword1.wav", 1, ATTN_NORM);
ai_charge(30);
};
void()    axe_runatkb2    =[    $axattb2,        axe_runatkb3    ] {ai_charge(20);};
void()    axe_runatkb3    =[    $axattb3,        axe_runatkb4    ] {ai_charge(10); axeg_chop('0 0 0');};
void()    axe_runatkb4    =[    $axattb4,        axe_runatkb5    ] {axeg_chop('40 75 10');};
void()    axe_runatkb5    =[    $axattb5,        axe_runatkb6    ] {axeg_chop('80 150 20');};
void()    axe_runatkb6    =[    $axattb6,        axeg_run1    ] {ai_charge_side();};

void()    axe_runatkc1    =[    $axattc1,        axe_runatkc2    ] {
if (random() > 0.5)
    sound (self, CHAN_WEAPON, "knight/sword2.wav", 1, ATTN_NORM);
else
    sound (self, CHAN_WEAPON, "knight/sword1.wav", 1, ATTN_NORM);
ai_charge(30);
};
void()    axe_runatkc2    =[    $axattc2,        axe_runatkc3    ] {ai_charge(20);};
void()    axe_runatkc3    =[    $axattc3,        axe_runatkc4    ] {ai_charge(10); axeg_chop('0 0 0');};
void()    axe_runatkc4    =[    $axattc4,        axe_runatkc5    ] {axeg_chop('60 0 10');};
void()    axe_runatkc5    =[    $axattc5,        axe_runatkc6    ] {axeg_chop('120 0 20');};
void()    axe_runatkc6    =[    $axattc6,        axeg_run1    ] {ai_charge_side();};

void()    axe_runatkd1    =[    $axattd1,        axe_runatkd2    ] {
if (random() > 0.5)
    sound (self, CHAN_WEAPON, "knight/sword2.wav", 1, ATTN_NORM);
else
    sound (self, CHAN_WEAPON, "knight/sword1.wav", 1, ATTN_NORM);
ai_charge(30);
};
void()    axe_runatkd2    =[    $axattd2,        axe_runatkd3    ] {ai_charge(20);};
void()    axe_runatkd3    =[    $axattd3,        axe_runatkd4    ] {ai_charge(10); axeg_chop('0 0 0');};
void()    axe_runatkd4    =[    $axattd4,        axe_runatkd5    ] {axeg_chop('30 -75 100');};
void()    axe_runatkd5    =[    $axattd5,        axe_runatkd6    ] {axeg_chop('60 -150 200');};
void()    axe_runatkd6    =[    $axattd6,        axeg_run1    ] {ai_charge_side();};

void()    axeg_atk =
{
    local float r;
    if (self.pain_finished > time)
        return;
    r = random();
    
    if (r < 0.25)
        axe_runatka1();
    else    if (r < 0.5)
        axe_runatkb1();
    else    if (r < 0.75)
        axe_runatkc1();
    else
        axe_runatkd1();
};


//===========================================================================


void()    axeg_pain1    =[    $axpain1,    axeg_pain2    ] {ai_painforward(0);};
void()    axeg_pain2    =[    $axpain2,    axeg_pain3    ] {ai_painforward(-1);};
void()    axeg_pain3    =[    $axpain3,    axeg_pain4    ] {ai_painforward(-2);};
void()    axeg_pain4    =[    $axpain4,    axeg_pain5    ] {};
void()    axeg_pain5    =[    $axpain5,    axeg_pain6    ] {};
void()    axeg_pain6    =[    $axpain6,    axeg_run1    ] {};

void(entity inflictor, entity attacker, float damage)    axeg_pain = {
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    if (self.pain_check > 0) {
        self.pain_finished = time + 0.75;
        self.axhitme = 0;
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        axeg_pain1();    
    }
};

//===========================================================================



void()    axeg_die1    =[    $axdeth1,    axeg_die2    ] {};
void()    axeg_die2    =[    $axdeth2,    axeg_die3    ] {monster_check_gib();};
void()    axeg_die3    =[    $axdeth3,    axeg_die4    ] {monster_check_gib(); self.solid = SOLID_NOT;};
void()    axeg_die4    =[    $axdeth4,    axeg_die5    ] {};
void()    axeg_die5    =[    $axdeth5,    axeg_die6    ] {};
void()    axeg_die6    =[    $axdeth6,    axeg_die7    ] {};
void()    axeg_die7    =[    $axdeth7,    axeg_die8    ] {};
void()    axeg_die8    =[    $axdeth8,    axeg_die9    ] {monster_death_postcheck();};
void()    axeg_die9    =[    $axdeth9,    axeg_die9    ] {monster_deadbody_check();};

void() axeg_die = {
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "knight/kdeath.wav", 1, ATTN_NORM);
        axeg_die1();                                    // Backward/side
    }
};

/*QUAKED monster_axegrunt (1 0 0) (-16 -16 -24) (16 16 40) Ambush
*/
void() monster_axegrunt = {
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/axegrunt.mdl";
    self.headmdl = "progs/h_guard.mdl";
    self.gib1mdl = "progs/gib1.mdl";
    self.gib2mdl = "progs/gib2.mdl";
    self.gib3mdl = "progs/gib3.mdl";
    precache_model ("progs/axegrunt.mdl");
    precache_model ("progs/h_guard.mdl");

    precache_sound ("knight/kdeath.wav");
    precache_sound ("knight/khurt.wav");
    precache_sound ("knight/ksight.wav");
    precache_sound ("knight/sword1.wav");
    precache_sound ("knight/sword2.wav");
    precache_sound ("knight/idle.wav");
    precache_sound ("player/udeath.wav");    // gib death
    self.gib1sound = GIB_IMPACT_WOOD;

    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    self.pain_sound = "knight/khurt.wav";
    self.sight_sound = "knight/ksight.wav";
    if (self.bboxtype < 1) self.bboxtype = BBOX_SHORT;
    if (self.health < 1) self.health = 75;
    self.gibhealth = -35;                // Easy to gib
    self.gibbed = FALSE;                // Still together
    self.pain_flinch = 20;                // Easy to pain
    self.pain_longanim = TRUE;            // can be chopped with shadow axe
    self.steptype = FS_TYPEMEDIUM;        // Average feet
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.th_stand = axeg_stand1;
//    self.th_walk = axe_walk1;
    self.th_run = axeg_run1;
    self.th_melee = axeg_atk;
    self.th_pain = axeg_pain;
    self.th_die = axeg_die;
    
    self.classtype = CT_MONARMYAXE;
    self.classgroup = CG_ARMY;
    self.classmove = MON_MOVEWALK;

    if (self.deathstring == "")  self.deathstring = " was axed by an Axe Grunt\n";
    
    monster_start ();
};
