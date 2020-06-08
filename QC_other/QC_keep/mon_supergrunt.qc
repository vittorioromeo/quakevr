void() SGruntCheckAttack;

// running
// a stupid, lurching hopscotch-type maneouvre...

void() sgrunt_run1 = [92, sgrunt_run2] {ai_run(20);};
void() sgrunt_run2 = [93, sgrunt_run3] {monster_footstep(FALSE); ai_run(25);};
void() sgrunt_run3 = [94, sgrunt_run4] {ai_run(6);};
void() sgrunt_run4 = [95, sgrunt_run5] {ai_run(9);};
void() sgrunt_run5 = [96, sgrunt_run6] {ai_run(20);};
void() sgrunt_run6 = [97, sgrunt_run7] {monster_footstep(FALSE); ai_run(25);};
void() sgrunt_run7 = [98, sgrunt_run8] {ai_run(3);};
void() sgrunt_run8 = [99, sgrunt_run1] {ai_run(7);};

// standing
// stand50-stand71 is a standing loop.

void() sgrunt_stand =
{
    if ((self.frame < 50) || (self.frame >= 71))
        self.frame = 50;

    else
        self.frame = self.frame + 1;

    self.think = sgrunt_stand;
    self.nextthink = time + 0.1;
    ai_stand();    // always put this at the end of the function otherwise things get really messed up.
};

// walking

void() sgrunt_swagger1 = [74, sgrunt_swagger2 ] 
{
    if (random() < 0.1)
        sound (self, CHAN_VOICE, "sgrunt/infidle1.wav", 1, ATTN_IDLE);

    ai_walk(7);
};
void() sgrunt_swagger2 = [75, sgrunt_swagger3 ] {ai_walk(5);};
void() sgrunt_swagger3 = [76, sgrunt_swagger4 ] {ai_walk(5);};
void() sgrunt_swagger4 = [77, sgrunt_swagger5 ] {ai_walk(6);};
void() sgrunt_swagger5 = [78, sgrunt_swagger6 ] {ai_walk(5); monster_footstep(FALSE);};
void() sgrunt_swagger6 = [79, sgrunt_swagger7 ] {ai_walk(6);};
void() sgrunt_swagger7 = [80, sgrunt_swagger8 ] {ai_walk(6);};
void() sgrunt_swagger8 = [81, sgrunt_swagger9 ] {ai_walk(5);};
void() sgrunt_swagger9 = [82, sgrunt_swagger10 ] {ai_walk(5);};
void() sgrunt_swagger10 = [83, sgrunt_swagger11 ] {ai_walk(5);};
void() sgrunt_swagger11 = [84, sgrunt_swagger12 ] {ai_walk(5);};
void() sgrunt_swagger12 = [85, sgrunt_swagger1 ] {ai_walk(6);};

// firing
// attak1-attak15 is the firing anim.
// in q2, he is frozen on attak11 whilst firing.

void() sgrunt_fire12;

float() ai_face_last =
{
    if ( (visible(self.enemy)) || (self.enemy.origin == '0 0 0') ) {
        ai_face();
        return TRUE;
    }

    self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
    ChangeYaw ();
    return FALSE;
};
/*
void() SpawnSGruntMuzzleFlash =
{
    local entity new;

    new = spawn();

    new.owner = self;

    new.cnt = 0;    // used to keep track of positioning

    new.solid = SOLID_NOT;
    new.movetype = MOVETYPE_NONE;
    new.angles = self.angles;

    new.think = SUB_Remove;        // just in case it gets left behind
    new.nextthink = time + 0.2;

    setmodel(new, "progs/sgrflash.mdl");
    setsize(new, '0 0 0', '0 0 0');
    setorigin(new, self.origin);

    self.trigger_field = new;    // for want of a better variable
};

void() SGruntMuzzleFlash =
{
    local float f, f1;
    local vector ofs;
    local entity t;

    t = self.trigger_field;
    f = t.cnt;

    makevectors(self.angles);

    ofs = v_forward*36;    

    if (f == 0)
        ofs = ofs + v_right*9 + '0 0 24';
    else if (f == 1)
        ofs = ofs + v_right*6 + '0 0 22';
    else if (f == 2)
        ofs = ofs + v_right*6 + '0 0 19';
    else if (f == 3)
        ofs = ofs + v_right*9 + '0 0 17';
    else if (f == 4)
        ofs = ofs + v_right*12 + '0 0 19';
    else
        ofs = ofs + v_right*12 + '0 0 22';
    
    t.angles = self.angles;
    setorigin(t, self.origin + ofs);

    f1 = f + 1;
    if (f1 > 5)
        f1 = 0;

    t.cnt = f1;
    t.nextthink = time + 0.2;    // don't want to remove it yet
};
*/

void() sgrunt_plasma_touch ={
    if (other == self.owner) return;

    if (other.solid == SOLID_TRIGGER) return;    // trigger field, do nothing

    if (pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

// hit something that bleeds
    if (other.takedamage)
    {
        spawn_touchblood (self, other, 6);
        T_Damage (other, self, self.owner, 6, DAMARMOR);
    }    

    // do sprite explosion

    self.origin = self.origin - 8*normalize(self.velocity);

    self.movetype = MOVETYPE_NONE;
    self.velocity = '0 0 0';
    self.touch = SUB_Null;
    self.solid = SOLID_NOT;

    self.think = BDW_SpriteGenericPlayOnce;
    self.nextthink = time + 0.05;

    self.frame = 0;
    self.idleweaponframe = 6;
    setmodel(self, "progs/s_plasgx.spr");

    // make sound
    sound(self, CHAN_VOICE, "weapons/plasexpl.wav", 1, ATTN_NORM);
};

void(vector org, vector dir) sgrunt_spawn_plasma =
{
    local entity plasma;
    local float f;

    self.effects = self.effects | EF_MUZZLEFLASH;
    sound(self, CHAN_WEAPON, "sgrunt/hovatck1.wav", 1, ATTN_NORM);

    plasma = spawn();
    plasma.classname = "plasma";
    plasma.owner = self;
    plasma.movetype = MOVETYPE_FLYMISSILE;
    plasma.solid = SOLID_BBOX;
    plasma.touch = sgrunt_plasma_touch;
    plasma.velocity = dir*750;
    plasma.think = BDW_SpriteGenericLoop;
    plasma.nextthink = time + 0.05;

    // randomise the starting frame...
    f = random()*6;    // 0 < f < 6
    f = ceil(f);    // 1 <= f <=6
    f = f - 1;        // 0 <= f <= 5

    plasma.frame = f;
    plasma.idleweaponframe = 6;

    setmodel(plasma, "progs/s_plasgr.spr");
    setsize(plasma, VEC_ORIGIN, VEC_ORIGIN);
    setorigin(plasma, org);
};    

void(vector ofs) sgrunt_death_plasma =
{
    local vector org, dir;

    makevectors(self.angles);

    org = self.origin + v_forward*ofs_x + v_right*ofs_y;
    org_z = org_z + ofs_z;

    dir = org - (self.origin + '0 0 24' + v_right*5);
    dir = normalize(dir);

    sgrunt_spawn_plasma(org, dir);
};

void(float o_r, float o_z) sgrunt_plasma_launch =
{
    local vector ofs, dir;
    local entity en;
    local float t;        

    // lead the player
    if (ai_face_last())
    {
        en = self.enemy;
        t = (vlen(en.origin - self.origin)) / 750;    // rough calculation of time to reach enemy

        dir = en.velocity * t * 0.5;    // 0.5 tones it down a bit
        dir_z = 0;                // jumping produces sillyness
        dir = dir + en.origin;
        dir = normalize(dir - self.origin);
    }
    else
        dir = normalize(self.enemy.origin - self.origin);

    makevectors(self.angles);
    // do a hyberblaster-style offset cycle
    ofs = v_forward*36 + v_right*o_r;    
    ofs_z = ofs_z + o_z;

    dir = dir + crandom()*0.05*v_right + crandom()*0.05*v_up;    // give it a bit of spread. FIXME: spread based on skill?
    dir = normalize(dir);    // FIXME: is this necessary?

    sgrunt_spawn_plasma(self.origin + ofs, dir);

    self.cnt = self.cnt - 1;

    if (self.cnt <= 0)
        self.think = sgrunt_fire12;
};

void() sgrunt_plasma1 = [194, sgrunt_plasma2] {sgrunt_plasma_launch(9, 24);};
void() sgrunt_plasma2 = [194, sgrunt_plasma3] {sgrunt_plasma_launch(6, 22);};
void() sgrunt_plasma3 = [194, sgrunt_plasma4] {sgrunt_plasma_launch(6, 19);};
void() sgrunt_plasma4 = [194, sgrunt_plasma5] {sgrunt_plasma_launch(9, 17);};
void() sgrunt_plasma5 = [194, sgrunt_plasma6] {sgrunt_plasma_launch(12, 19);};
void() sgrunt_plasma6 = [194, sgrunt_plasma1] {sgrunt_plasma_launch(12, 22);};

void() sgrunt_fire_loop = [194, sgrunt_fire_loop]
{
    local    vector    dir;
    
    self.effects = self.effects | EF_MUZZLEFLASH;
    sound(self, CHAN_WEAPON, "sgrunt/infatck1.wav", 1, ATTN_NORM);    

    // fire somewhat behind the player, so a dodging player is harder to hit
    if (ai_face_last())
    {    
        dir = self.enemy.origin - self.enemy.velocity*0.2;
        dir = normalize (dir - self.origin);
    }
    else
        dir = normalize(self.enemy.origin - self.origin);
    
    FireBullets (4, SPREAD_GRUNT);    

    self.cnt = self.cnt - 1;

    if (self.cnt <= 0) {
        self.think = sgrunt_fire12;
    }    
};

void() sgrunt_fire1 = [184, sgrunt_fire2] {ai_face_last();ai_forward(5);};
void() sgrunt_fire2 = [185, sgrunt_fire3] {ai_face_last();ai_back(1);};
void() sgrunt_fire3 = [186, sgrunt_fire4] {ai_face_last();ai_back(1);};
void() sgrunt_fire4 = [187, sgrunt_fire5] {ai_face_last();sound(self, CHAN_WEAPON, "sgrunt/infatck3.wav", 1, ATTN_NORM);};
void() sgrunt_fire5 = [188, sgrunt_fire6] {ai_face_last();ai_back(1);};
void() sgrunt_fire6 = [189, sgrunt_fire7] {ai_face_last();ai_forward(1);};
void() sgrunt_fire7 = [190, sgrunt_fire8] {ai_face_last();ai_forward(1);};
void() sgrunt_fire8 = [191, sgrunt_fire9] {ai_face_last();ai_forward(2);};
void() sgrunt_fire9 = [192, sgrunt_fire10] {ai_face_last();ai_back(2);};
void() sgrunt_fire10 = [193, sgrunt_fire11] {ai_face_last();ai_back(4);};
void() sgrunt_fire11 = [194, sgrunt_fire12] {
    ai_forward(1);
    self.cnt = 10 + random()*10;    // number of shots to fire
    self.cnt = rint(self.cnt);
    if (self.monsterflags & MF_COMMANDER)
        sgrunt_plasma1();
    else
        sgrunt_fire_loop();
};
void() sgrunt_fire12 = [195, sgrunt_fire13] {ai_face();ai_forward(6);};
void() sgrunt_fire13 = [196, sgrunt_fire14] {ai_face();ai_back(1);};
void() sgrunt_fire14 = [197, sgrunt_fire15] {ai_face();ai_back(2);};
void() sgrunt_fire15 = [198, sgrunt_run1] {
    local vector spot1, spot2;
    local float r;

    ai_face();
    ai_back(4);

// check for automatic refire based on skill level, but if enemy is quite close
// then give time to move in for a melee attack...
    if (random() < 0.25*skill)
    {
        spot1 = self.origin + self.view_ofs;
        spot2 = self.enemy.origin + self.enemy.view_ofs;

        r = vlen(spot1 - spot2);
        if (r > 250)
        {
            if (visible(self.enemy))
            {
                self.think = sgrunt_fire1;
                return;
            }
        }
    }
    self.attack_finished = time + 1 + random()*2;
};

void() sgrunt_wallop = {
    local vector delta;
    local float ldmg;

    if (!self.enemy)
        return;

    delta = self.enemy.origin - self.origin;

    if (vlen(delta) > 60)
        return;
    if (!CanDamage(self.enemy, self))
        return;
        
    ldmg = (random() + random() + random()) * 20;
    T_Damage (self.enemy, self, self, ldmg,DAMARMOR);
    sound (self, CHAN_WEAPON, "sgrunt/melee2.wav", 1, ATTN_NORM);
};

void() sgrunt_smash1 = [199, sgrunt_smash2] {ai_face();ai_forward(3);};
void() sgrunt_smash2 = [200, sgrunt_smash3] {ai_face();ai_back(7);sound(self, CHAN_WEAPON, "sgrunt/infatck2.wav", 1, ATTN_NORM);};
void() sgrunt_smash3 = [201, sgrunt_smash4] {ai_face();ai_forward(1);};
void() sgrunt_smash4 = [202, sgrunt_smash5] {ai_face();ai_forward(9);};
void() sgrunt_smash5 = [203, sgrunt_smash6] {ai_face();ai_forward(6);};
void() sgrunt_smash6 = [204, sgrunt_smash7] {ai_face();ai_forward(9);sgrunt_wallop();};
void() sgrunt_smash7 = [205, sgrunt_smash8] {ai_face();ai_back(7);};
void() sgrunt_smash8 = [206, sgrunt_run1] {ai_face();ai_back(4);};


void() sgrunt_damage_skin = {
    if (self.health <= 175)
        self.skin = 1;
    else
        self.skin = 0;
};

void() sgrunt_paina1 = [100, sgrunt_paina2] {ai_back(4);};
void() sgrunt_paina2 = [101, sgrunt_paina3] {ai_back(2);};
void() sgrunt_paina3 = [102, sgrunt_paina4] {ai_back(2);};
void() sgrunt_paina4 = [103, sgrunt_paina5] {ai_back(2);};
void() sgrunt_paina5 = [104, sgrunt_paina6] {ai_back(1);};
void() sgrunt_paina6 = [105, sgrunt_paina7] {ai_forward(1);};
void() sgrunt_paina7 = [106, sgrunt_paina8] {ai_back(1);};
void() sgrunt_paina8 = [107, sgrunt_paina9] {ai_forward(1);};
void() sgrunt_paina9 = [108, sgrunt_paina10] {ai_forward(7);};
void() sgrunt_paina10 = [109, sgrunt_run1] {ai_forward(2);};

void() sgrunt_painb1 = [110, sgrunt_painb2] {ai_back(3);};
void() sgrunt_painb2 = [111, sgrunt_painb3] {ai_back(4);};
void() sgrunt_painb3 = [112, sgrunt_painb4] {};
void() sgrunt_painb4 = [113, sgrunt_painb5] {ai_back(1);};
void() sgrunt_painb5 = [114, sgrunt_painb6] {ai_back(2);};
void() sgrunt_painb6 = [115, sgrunt_painb7] {};
void() sgrunt_painb7 = [116, sgrunt_painb8] {};
void() sgrunt_painb8 = [117, sgrunt_painb9] {ai_forward(2);};
void() sgrunt_painb9 = [118, sgrunt_painb10] {ai_forward(6);};
void() sgrunt_painb10 = [119, sgrunt_run1] {ai_forward(2);};

void(entity inflictor, entity attacker, float damage) sgrunt_pain = {
    sgrunt_damage_skin();

    if (self.pain_finished > time)
    //if (NotAGoonSlapper(attacker))
    //    return;

    if (random()*200 > damage)
    //if (NotAGoonSlapper(attacker))
    //    return;        // didn't flinch

    self.pain_finished = time + 2;

    if (random() < 0.5) {
        sound(self, CHAN_VOICE, "sgrunt/infpain1.wav", 1, ATTN_NORM);
        sgrunt_paina1();
    } else {
        sound(self, CHAN_VOICE, "sgrunt/infpain2.wav", 1, ATTN_NORM);
        sgrunt_painb1();
    }
};

void() sgrunt_die1 = [125, sgrunt_die2] {ai_back(5);};
void() sgrunt_die2 = [126, sgrunt_die3] {monster_check_gib();};
void() sgrunt_die3 = [127, sgrunt_die4] {monster_check_gib();};
void() sgrunt_die4 = [128, sgrunt_die5] {ai_back(1);};
void() sgrunt_die5 = [129, sgrunt_die6] {ai_back(5);};
void() sgrunt_die6 = [130, sgrunt_die7] {};
void() sgrunt_die7 = [131, sgrunt_die8] {};
void() sgrunt_die8 = [132, sgrunt_die9] {};
void() sgrunt_die9 = [133, sgrunt_die10] {ai_back(1);};
void() sgrunt_die10 = [134, sgrunt_die11] {ai_forward(4);};
void() sgrunt_die11 = [135, sgrunt_die12] {ai_forward(1);};
void() sgrunt_die12 = [136, sgrunt_die13] {ai_forward(1);};
void() sgrunt_die13 = [137, sgrunt_die14] {ai_back(2);};
void() sgrunt_die14 = [138, sgrunt_die15] {ai_forward(2);};
void() sgrunt_die15 = [139, sgrunt_die16] {ai_forward(2);};
void() sgrunt_die16 = [140, sgrunt_die17] {ai_forward(7);self.solid = SOLID_NOT;DropBackpack();};
void() sgrunt_die17 = [141, sgrunt_die18] {ai_forward(9);};
void() sgrunt_die18 = [142, sgrunt_die19] {ai_forward(6);BDW_BodyDropSound();};
void() sgrunt_die19 = [143, sgrunt_die20] {ai_back(3);monster_death_postcheck();};
void() sgrunt_die20 = [144, sgrunt_die20] {monster_deadbody_check();};

//note that plasma offsets on the death anim. are currently based on the centre of the barrel...
//if you've got time, you could do a cyclical movement, like normal firing mode
    
void() sgrunt_bdie1 = [145, sgrunt_bdie2] {};
void() sgrunt_bdie2 = [146, sgrunt_bdie3] {ai_forward(2);monster_check_gib();};
void() sgrunt_bdie3 = [147, sgrunt_bdie4] {ai_forward(6);monster_check_gib();};
void() sgrunt_bdie4 = [148, sgrunt_bdie5] {ai_back(1);};
void() sgrunt_bdie5 = [149, sgrunt_bdie6] {};
void() sgrunt_bdie6 = [150, sgrunt_bdie7] {ai_forward(2);};
void() sgrunt_bdie7 = [151, sgrunt_bdie8] {ai_forward(1);};
void() sgrunt_bdie8 = [152, sgrunt_bdie9] {ai_forward(5);sgrunt_death_plasma('17 6 -2');};        //fire start
void() sgrunt_bdie9 = [153, sgrunt_bdie10] {ai_forward(4);sgrunt_death_plasma('24 2 16');};
void() sgrunt_bdie10 = [154, sgrunt_bdie11] {sgrunt_death_plasma('22 5 20');};
void() sgrunt_bdie11 = [155, sgrunt_bdie12] {ai_back(2);sgrunt_death_plasma('21 9 23');};
void() sgrunt_bdie12 = [156, sgrunt_bdie13] {ai_back(3);sgrunt_death_plasma('21 11 25');};
void() sgrunt_bdie13 = [157, sgrunt_bdie14] {ai_back(3);sgrunt_death_plasma('20 15 28');};
void() sgrunt_bdie14 = [158, sgrunt_bdie15] {ai_back(1);sgrunt_death_plasma('17 18 30');};
void() sgrunt_bdie15 = [159, sgrunt_bdie16] {ai_back(2);sgrunt_death_plasma('15 20 32');};
void() sgrunt_bdie16 = [160, sgrunt_bdie17] {sgrunt_death_plasma('14 24 30');};
void() sgrunt_bdie17 = [161, sgrunt_bdie18] {ai_forward(2);sgrunt_death_plasma('11 27 27');};
void() sgrunt_bdie18 = [162, sgrunt_bdie19] {ai_forward(3);sgrunt_death_plasma('17 22 26');};
void() sgrunt_bdie19 = [163, sgrunt_bdie20] {ai_forward(4);sgrunt_death_plasma('18 19 24');};
void() sgrunt_bdie20 = [164, sgrunt_bdie21] {ai_back(12);self.solid = SOLID_NOT;DropBackpack();sgrunt_death_plasma('13 18 35');};
void() sgrunt_bdie21 = [165, sgrunt_bdie22] {ai_back(9);sgrunt_death_plasma('-2 18 38');};
void() sgrunt_bdie22 = [166, sgrunt_bdie23] {ai_back(10);sgrunt_death_plasma('-15 15 28');};
void() sgrunt_bdie23 = [167, sgrunt_bdie24] {ai_back(10);BDW_BodyDropSound();sgrunt_death_plasma('-27 16 12');};    //fire end
void() sgrunt_bdie24 = [168, sgrunt_bdie25] {ai_forward(5);monster_death_postcheck();};
void() sgrunt_bdie25 = [169, sgrunt_bdie25] {monster_deadbody_check();};

void() sgrunt_die =  {
    local vector org;
    local float gibcount;

    self.skin = 0;
    monster_death_precheck();
    
    if (!self.gibbed) {
        sgrunt_damage_skin();

        // regular death
        sound(self, CHAN_VOICE, "gibs/ut2gib1.wav", 1, ATTN_NORM);

        // head explodes on death...
        makevectors(self.angles);
        org = self.origin + '0 0 40' - v_forward*8;

        // todo: put explosion sprite here?

        gibcount = 6;
        while (gibcount > 0) {
            SpawnMeatSpray (self, self, (250*crandom()));
            gibcount = gibcount - 1;
        }
    
        if (random() < 0.5)
            sgrunt_die1();
        else
            sgrunt_bdie1();
    }
};

void() monster_super_grunt = {
    if (deathmatch) { remove(self); return; }
    
    self.mdl = "progs/sgrunt.mdl";
    self.headmdl = "progs/head2.mdl";
    precache_model("progs/sgrunt.mdl");
    precache_model("progs/head2.mdl");
    precache_model("progs/s_plasgr.spr");    // super grunt commander plasma shot
    precache_model("progs/s_plasgx.spr");    // super grunt commander plasma explosion

    //precache_sound ("sgrunt/infdeth1.wav");    
    //precache_sound ("sgrunt/infdeth2.wav");    
    precache_sound ("sgrunt/infidle1.wav");    
    precache_sound ("sgrunt/infpain1.wav");    
    precache_sound ("sgrunt/infpain2.wav");    
    precache_sound ("sgrunt/infsght1.wav");    
    precache_sound ("sgrunt/infsrch1.wav");
    precache_sound ("sgrunt/infatck1.wav");
    precache_sound ("sgrunt/infatck2.wav");
    precache_sound ("sgrunt/infatck3.wav");
    precache_sound ("sgrunt/melee2.wav");
    precache_sound ("sgrunt/hovatck1.wav");

    precache_sound ("gibs/ut2gib1.wav");        
    precache_sound ("weapons/plasexpl.wav");

    if (random() < 0.5)    
        self.sight_sound = "sgrunt/infsght1.wav";
    else
        self.sight_sound = "sgrunt/infsrch1.wav";

    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;

    setmodel (self, "progs/sgrunt.mdl");

    setsize (self, '-16 -16 -24', '16 16 40');
    if (self.health < 1) self.health = 350;
    self.gibhealth = -60;
    self.th_stand = sgrunt_stand;
    self.th_walk = sgrunt_swagger1;
    self.th_run = sgrunt_run1;
    self.th_melee = sgrunt_smash1;
    self.th_missile = sgrunt_fire1;
    self.th_pain = sgrunt_pain;
    self.th_die = sgrunt_die;
    if (self.ammo_cells < 1) self.ammo_cells = 10;
    self.th_checkattack = SGruntCheckAttack;

    self.monsterflags = self.monsterflags | MF_COMMANDER;

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    if (self.deathstring == "")  self.deathstring = " was slaughtered by a Sergeant\n";

    self.classtype = CT_MONSUPERGRUNT;
    self.classgroup = CG_ARMY;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};

/*
===========
SGruntCheckAttack

The player is in view, so decide to move or launch an attack
Returns FALSE if movement should continue
============
*/
void() SGruntCheckAttack = {
    local vector    spot1, spot2;
    local entity    targ;
    local float    r;

    targ = self.enemy;

    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;

    if (enemy_range == RANGE_MELEE) {
        r = vlen(spot1 - spot2);
        if (r < 80)                // needs to be really close
        {
            if (CanDamage (self.enemy, self))
            {
                self.attack_state = AS_MELEE;
                return;
            }
        }
    }

    if (time < self.attack_finished) return;

    if (!enemy_vis) return;

// see if any entities are in the way of the shot
    traceline (spot1, spot2, FALSE, self);

    if (trace_ent != targ) return;    // don't have a clear shot

    // ben - air/water check is implicit in enemy_vis
    //if (trace_inopen && trace_inwater)
    //    return;            

// missile attack    

    //if (enemy_range == RANGE_FAR)
    //    return;    

    self.attack_state = AS_MISSILE;
    //SUB_AttackFinished (3 + 3*random());
};

