/*
==============================================================================

SPEARKNIGHT

==============================================================================
*/

$cd id1/models/knight2
$origin 0 0 24
$base base
$skin skin

$frame stand1


float SKNIGHT_NUMSPEARS = 18;


float SK_STATE_HUNT = 1;
float SK_STATE_DOWN = 2;
float SK_STATE_SATTACK = 3;
float SK_STATE_SLEEPING = 4;

float SPEAR_STATE_STUCK = 1;
float SPEAR_STATE_FLYING = 2;
float SPEAR_STATE_PARENTED = 3;

float SKNIGHT_RADIUS = 64;
float SKNIGHT_SPEED = 250;

float SKNIGHT_MOVETIME;


//To easily call a function on all spears.
void(void() call) sknight_spears_do =
{
    entity oself;
    oself = self;
    self = find(world, classname, "spear");

    while(self){
        call();
        self = find(self, classname, "spear");
    }
    self = oself;
};

void(float() conditional, void() call) sknight_spears_do_if =
{
    entity spear, oself;
    spear = find(world, classname, "spear");
    oself = self;

    while(spear){
        self = spear;
        if(conditional()){
            call();
        }
        spear = find(spear, classname, "spear");
    }
    self = oself;
};

void(float() conditional, void() call) sknight_spears_do_if_not =
{
    entity spear, oself;
    spear = find(world, classname, "spear");
    oself = self;

    while(spear){
        self = spear;
        if(conditional() == FALSE){
            call();
        }
        spear = find(spear, classname, "spear");
    }
    self = oself;
};

float() IS_PARENTED =
{
    return (self.classname == "spear") && (self.flags = SPEAR_STATE_PARENTED);
}

/**********************************************************************************************/
/* Returns a position a spear would be in if it was at position o with angle a                */
/*  and offset distance d towards the spear tip.                                              */
/**********************************************************************************************/
vector (vector o, vector a, float d) getSpearOffset = {

    vector dir;
    a_x = a_x * -1; //Why?

    makevectors(a);
    dir = normalize(v_forward) * d;

    return o + dir;
}



/**********************************************************************************************/
/* Reset spears to initial position                                                           */
/**********************************************************************************************/
void () sknight_reset_spears_think2 =
{
    vector d;
    d = self.owner.origin + self.pos1;


    SUB_CalcMoveRotateTime(d, SKNIGHT_MOVETIME / 4 , self.pos2, SUB_Null);
}

void () sknight_reset_spears_think =
{
    vector d;
    d = self.owner.origin + self.pos1;
    d = getSpearOffset(d, self.pos2, -192);


    //SUB_CalcMoveSpline(destination, finalAngle, time, cpOffset, angOffset, func);
    SUB_CalcMoveSpline(d, self.pos2, 3 * SKNIGHT_MOVETIME / 4, 192, '0 0 0', sknight_reset_spears_think2);

    //SUB_CalcMoveRotateTime(d, SKNIGHT_MOVETIME / 2 , self.pos2, sknight_reset_spears_think2);
}

void (float t) sknight_reset_spears =
{
    SKNIGHT_MOVETIME = t;
    sknight_spears_do(sknight_reset_spears_think);
}


/**********************************************************************************************/
/* Move all spears in a uniform manner to the given offset                                    */
/**********************************************************************************************/
void () sknight_move_spears_think =
{
    vector d;
    d = self.origin + self.owner.dest;
    SUB_CalcMoveRotate(d, SKNIGHT_SPEED, self.angles, SUB_Null);
}

void (vector offset) sknight_move_spears =
{
    self.dest = offset;
    sknight_spears_do(sknight_move_spears_think);
}


/**********************************************************************************************/
/* Everything needed for the spinning spear attack                                            */
/**********************************************************************************************/
vector(vector center, vector ang) sknight_getNextSpearPos =
{
    vector vx, vy, vz, org;

    makevectors (ang);

    org = center;
    vx = (v_forward * SKNIGHT_RADIUS);
    vy = (v_right   * SKNIGHT_RADIUS);
    //vy = vy * -1;
    //vz = (v_up      * 0);
    org = vx + vy;// + vz;
    //org_z = org_z + 14;

    return org + center;
}


void() sknight_spears_orbit =
{
    vector center, ang, nOrg;

    ang = '0 0 0';
    ang_y = ((self.cnt / SKNIGHT_NUMSPEARS) *360) + (time * SKNIGHT_SPEED);
    center = self.owner.origin;

    nOrg = sknight_getNextSpearPos(center, ang);
    ang_y = ang_y - 90;
    ang_z = 90;
    SUB_CalcMoveRotateTime(nOrg, 0.1, ang, sknight_spears_orbit);


}


void () sknight_star_attack_end_think =
{
    vector dir, ang, nOrg;

    ang = self.angles;
    //self.avelocity = '0 0 0';
    dir = self.origin - self.owner.origin;
    dir = normalize(dir);

    dir = dir * 384;
    dir = dir + self.owner.origin;

    SUB_CalcMoveRotate(dir, SKNIGHT_SPEED, self.angles, SUB_Null);
}

void () sknight_star_attack_end =
{
    sknight_spears_do(sknight_star_attack_end_think);
    self.state = SK_STATE_DOWN;
}

void () sknight_star_attack_begin_think =
{
    vector center, ang, nOrg;

    ang = '0 0 0';
    ang_y = ((self.cnt / SKNIGHT_NUMSPEARS) *360) + ((time + 0.9) * SKNIGHT_SPEED);
    center = self.owner.origin;

    nOrg = sknight_getNextSpearPos(center, ang);
    ang_y = ang_y - 90;
    ang_z = 90;
    SUB_CalcMoveRotateTime(nOrg, 1, ang, sknight_spears_orbit);
}

void () sknight_star_attack_begin =
{
    sknight_spears_do(sknight_star_attack_begin_think);
    self.state = SK_STATE_SATTACK;
}

/**********************************************************************************************/
/* Create and initialize all the spears                                                       */
/**********************************************************************************************/
void(entity sknight) sknight_setup_spears =
{
    entity spear;
    float spin;
    vector ang, dOrg;

    dOrg = sknight.origin;


    float i;
    i = 0;
    while(i < SKNIGHT_NUMSPEARS){
        spear = spawn();
        spear.classname = "spear";
        spear.cnt = i;
        spear.solid = SOLID_NOT;
        spear.movetype = MOVETYPE_NOCLIP;
        spear.owner = sknight;
        spear.flags = SPEAR_STATE_PARENTED;

        setmodel(spear, "progs/spear.mdl");
        setsize(spear, '-1 -1 -34', '1 1 37');


//        ang = '0 0 0';
//        spin = ((spear.cnt / SKNIGHT_NUMSPEARS) * 360) + (time * SKNIGHT_SPEED);
//        ang_y = spin;
//        sknight_setSpearPos(spear, dOrg, ang);

//        spear.think = sknight_spears_orbit;
//        spear.nextthink = time + 0.1;


        i = i + 1;
    }

    //see sknight_tbl.qc
    sknight_spears_do(sknight_spearpos_init);

    sknight_reset_spears(0.2);
};

/**********************************************************************************************/
/* Actual AI stuff                                                                            */
/**********************************************************************************************/
void() sknight_die =
{
// check for gib
    if (self.health < -40)
    {
        sound(self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
        ThrowHead ("progs/h_hellkn.mdl", self.health);
        ThrowGib ("progs/gib1.mdl", self.health);
        ThrowGib ("progs/gib2.mdl", self.health);
        ThrowGib ("progs/gib3.mdl", self.health);
    }else{
        sound(self, CHAN_VOICE, "hknight/death1.wav", 1, ATTN_NORM);
    }
    self.think = SUB_Null;
    self.nextthink = -1;
};



/*QUAKED monster_sknight (1 0 0) (-16 -16 -24) (16 16 40)
    Only one per level please!
*/
void() monster_sknight =
{
    if (deathmatch)
    {
        remove(self);
        return;
    }
    //precache_model2 ("progs/sknight.mdl");
    precache_model2 ("progs/hknight.mdl");
    precache_model2 ("progs/k_spike.mdl");
    precache_model2 ("progs/h_hellkn.mdl");
    precache_model2 ("progs/spear.mdl");
    precache_model2 ("progs/s_light.spr");
    precache_model2 ("progs/s_bubble.spr");


    precache_sound2 ("hknight/attack1.wav");
    precache_sound2 ("hknight/death1.wav");
    precache_sound2 ("hknight/pain1.wav");
    precache_sound2 ("hknight/sight1.wav");
    precache_sound("hknight/hit.wav");        // used by C code, so don't sound2
    precache_sound2 ("hknight/slash1.wav");
    precache_sound2 ("hknight/idle.wav");
    precache_sound2 ("hknight/grunt.wav");

    precache_sound("knight/sword1.wav");
    precache_sound("knight/sword2.wav");

    if (self.spawnflags & SPAWNFLAG_TRIGGER_SPAWN_MULTIPLE && !self.cnt){
        self.cnt = -1;
    }
    self.health = 250;

    self.th_stand = hknight_stand1;
    self.th_walk = SUB_Null;
    self.th_run = hknight_run1;
    self.th_melee = SUB_Null;
    self.th_missile = hknight_magicc1;
    self.th_pain = SUB_NullPain;
    self.th_die = hknight_die;

    self.state = SK_STATE_SLEEPING;

    sknight_setup_spears(self);

    self.solid = SOLID_SLIDEBOX;

    if(self.targetname){

        total_monsters = total_monsters + 1;

        self.movetype = MOVETYPE_STEP;

        self.use = flymonster_start;
    }else{
        flymonster_start();
    }

    setmodel(self, "progs/hknight.mdl");

    setsize(self, '-16 -16 -24', '16 16 40');


};
