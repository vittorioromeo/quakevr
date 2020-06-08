$cd /coe/progs/axeman
$origin 0 -6 24
$base base
$skin skin

$frame axrun1 axrun2 axrun3 axrun4 axrun5 axrun6
$frame axstand1 axstand2 axstand3 axstand4 axstand5 axstand6
$frame axstand7 axstand8 axstand9 axstand10 axstand11 axstand12
$frame axpain1 axpain2 axpain3 axpain4 axpain5 axpain6
$frame axdeth1 axdeth2 axdeth3 axdeth4 axdeth5 axdeth6 axdeth7 axdeth8 axdeth9
$frame axatt1 axatt2 axatt3 axatt4 axatt5 axatt6
$frame axattb1 axattb2 axattb3 axattb4 axattb5 axattb6
$frame axattc1 axattc2 axattc3 axattc4 axattc5 axattc6
$frame axattd1 axattd2 axattd3 axattd4 axattd5 axattd6

float axeman_type;

void() axe_stand1 =[$axstand1,axe_stand2] {ai_stand(); };
void() axe_stand2 =[$axstand2,axe_stand3] {ai_stand(); };
void() axe_stand3 =[$axstand3,axe_stand4] {ai_stand(); };
void() axe_stand4 =[$axstand4,axe_stand5] {ai_stand(); };
void() axe_stand5 =[$axstand5,axe_stand6] {ai_stand(); };
void() axe_stand6 =[$axstand6,axe_stand7] {ai_stand(); };
void() axe_stand7 =[$axstand7,axe_stand8] {ai_stand(); };
void() axe_stand8 =[$axstand8,axe_stand9] {ai_stand(); };
void() axe_stand9 =[$axstand9,axe_stand10] {ai_stand(); };
void() axe_stand10 =[$axstand10,axe_stand11] {ai_stand(); };
void() axe_stand11 =[$axstand11,axe_stand12] {ai_stand(); };
void() axe_stand12 =[$axstand12,axe_stand1] {ai_stand(); };

void() axe_walk1 =[$axrun1,axe_walk2] { if (random() < 0.2) sound (self, CHAN_VOICE, self.idle_sound, 1,  ATTN_IDLE); ai_walk(10); };
void() axe_walk2 =[$axrun2,axe_walk3] {ai_walk(10); };
void() axe_walk3 =[$axrun3,axe_walk4] {ai_walk(15); };
void() axe_walk4 =[$axrun4,axe_walk5] {ai_walk(10); };
void() axe_walk5 =[$axrun5,axe_walk6] {ai_walk(10); };
void() axe_walk6 =[$axrun6,axe_walk1] {ai_walk(15); };


void() axe_run1 =[$axrun1,axe_run2] { if (random() < 0.2) sound (self, CHAN_VOICE, self.idle_sound, 1,  ATTN_IDLE); ai_run(24); };
void() axe_run2 =[$axrun2,axe_run3] {ai_run(45); };
void() axe_run3 =[$axrun3,axe_run4] {ai_run(31); };
void() axe_run4 =[$axrun4,axe_run5] {ai_run(20); };
void() axe_run5 =[$axrun5,axe_run6] {ai_run(36); };
void() axe_run6 =[$axrun6,axe_run1] {ai_run(45); };

void() axe_atk1 =[$axatt1,axe_atk2] { sound (self, CHAN_WEAPON, "axeman/axe1.wav", 1, ATTN_NORM); ai_charge(0); };
void() axe_atk2    =[$axatt2,axe_atk3] {ai_charge(28); ai_melee(); };
void() axe_atk3    =[$axatt3,axe_atk4] {ai_charge(16); ai_melee(); };
void() axe_atk4    =[$axatt4,axe_atk5] {ai_charge(0); ai_melee(); };
void() axe_atk5    =[$axatt5,axe_atk6] {ai_charge(6); };
void() axe_atk6    =[$axatt6,axe_run1] {ai_charge(8); };

void() axe_atkb1 =[$axattb1,axe_atk2] { sound (self, CHAN_WEAPON, "axeman/axe1.wav", 1, ATTN_NORM); ai_charge(0); };
void() axe_atkb2 =[$axattb2,axe_atk3] {ai_charge(28); ai_melee(); };
void() axe_atkb3 =[$axattb3,axe_atk4] {ai_charge(16); ai_melee(); };
void() axe_atkb4 =[$axattb4,axe_atkb5] {ai_charge(0); };
void() axe_atkb5 =[$axattb5,axe_atkb6] {ai_charge(6); };
void() axe_atkb6 =[$axattb6,axe_run1] {ai_charge(8); };

void() axe_atkc1 =[$axatt1,axe_atkc2] { sound (self, CHAN_WEAPON, "axeman/axe1.wav", 1, ATTN_NORM); ai_charge(0); };
void() axe_atkc2 =[$axattc2,axe_atkc3] {ai_charge(28); };
void() axe_atkc3 =[$axattc3,axe_atkc4] {ai_charge(16); ai_melee(); };
void() axe_atkc4 =[$axattc4,axe_atkc5] {ai_charge(0); ai_melee(); };
void() axe_atkc5 =[$axattc5,axe_atkc6] {ai_charge(6); };
void() axe_atkc6 =[$axattc6,axe_run1] {ai_charge(8); };

void() axe_atkd1 =[$axattd1,axe_atkd2] { sound (self, CHAN_WEAPON, "axeman/axe1.wav", 1, ATTN_NORM); ai_charge(0); };
void() axe_atkd2 =[$axattd2,axe_atkd3] {ai_charge(28); ai_melee(); };
void() axe_atkd3 =[$axattd3,axe_atkd4] {ai_charge(16); ai_melee(); };
void() axe_atkd4 =[$axattd4,axe_atkd5] {ai_charge(0); };
void() axe_atkd5 =[$axattd5,axe_atkd6] {ai_charge(6); };
void() axe_atkd6 =[$axattd6,axe_run1] {ai_charge(8); };

void() axe_atk = {
    axeman_type = axeman_type + 1;

    if (axeman_type == 1)
        axe_atkb1();
    else if (axeman_type == 2)
        axe_atkd1();
    else if (axeman_type == 3) {
        if (random() < 0.5)
            axe_atkc1();
        else
            axe_atk1();
        axeman_type = 0;
    }
};

void() axe_pain1 =[$axpain1,axe_pain2] {ai_painforward(0); };
void() axe_pain2 =[$axpain2,axe_pain3] {ai_painforward(6); };
void() axe_pain3 =[$axpain3,axe_pain4] {ai_painforward(8); };
void() axe_pain4 =[$axpain4,axe_pain5] {ai_painforward(4); };
void() axe_pain5 =[$axpain5,axe_pain6] {ai_painforward(2); };
void() axe_pain6 =[$axpain6,axe_run1] {};

void(entity inflictor, entity attacker, float damage)    axe_pain = {
    if (self.pain_finished > time)
        return;

    sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
    axe_pain1 ();
    self.pain_finished = time + 1;
};

void() axe_die1    =[$axdeth1,axe_die2] {monster_check_gib(); };
void() axe_die2    =[$axdeth2,axe_die3] {monster_check_gib(); };
void() axe_die3    =[$axdeth3,axe_die4] {self.solid = SOLID_NOT; };
void() axe_die4    =[$axdeth4,axe_die5] {};
void() axe_die5    =[$axdeth5,axe_die6] {};
void() axe_die6    =[$axdeth6,axe_die7] {};
void() axe_die7    =[$axdeth7,axe_die8] {};
void() axe_die8    =[$axdeth8,axe_die9] {monster_death_postcheck(); };
void() axe_die9    =[$axdeth9,axe_die9] {};

void() axe_die = {
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, self.death_sound, 1, ATTN_NORM);
        axe_die1 ();
    }
};

// monster_axeman (1 0 0) (-16 -16 -24) (16 16 40) Ambush, Ferocious axe-wielding maniac :)

void() monster_ax = {
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_axeman.mdl";
    self.headmdl = "progs/h_knight.mdl";
    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_sound ("axeman/adeath1.wav");
    precache_sound ("axeman/axe1.wav");
    precache_sound ("axeman/axe2.wav");

    self.gib1sound = GIB_IMPACT_METALA;
    self.idle_sound = "dguard/idle.wav";
    precache_sound ("dguard/idle.wav");

    self.pain_sound = "dguard/pain1.wav";
    precache_sound ("dguard/pain1.wav");

    self.sight_sound = "dknight/dfury_grunt4.wav";
    precache_sound ("dknight/dfury_grunt4.wav");

    self.death_sound = "dguard/death2.wav";
    precache_sound (self.death_sound);

    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    if (self.health < 1) self.health = 300;
    self.gibhealth = -35;
    self.gibbed = FALSE;
    self.pain_flinch = 40;
    self.pain_timeout = 1.5;            // Higher than a knight
    self.weapon = IT_AXE;
    self.steptype = FS_TYPEHEAVY;

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.th_stand = axe_stand1;
    self.th_walk = axe_walk1;
    self.th_run = axe_run1;
    self.th_melee = axe_atk;
    self.th_pain = axe_pain;
    self.th_die = axe_die;

    self.classtype = CT_MONDKNIGHT;
    self.classgroup = CG_DKNIGHT;
    self.classmove = MON_MOVEWALK;
    if (self.deathstring == "")  {
        if ( (random () < 0.500) ) {
            if (self.deathstring == "")  self.deathstring = " was felled by an Axeman\n";
        } else {
            if (self.deathstring == "")  self.deathstring = " was scalped by an Axe Knight\n";
            if (self.skin) {if (self.deathstring == "")  self.deathstring = " was bushwhacked by a Forest Thug\n"; }
        }
    }

    monster_start ();
};

void() monster_axeman = { monster_ax(); };