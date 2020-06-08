 /*=============================================================================
 
monster_floyd (Automaton)
code by john fitzgibbons

==============================================================================*/
$skin floyd
$skin asleep

$frame skin
$frame idle1
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9 walk10 walk11 walk12 walk13 walk14 walk15 walk16
$frame run1 run2 run3 run4 run5 run6 run7 run8
$frame attack1 attack2 attack3 attack4 attack5 attack6 attack7 attack8 attack9
$frame paina1 paina2 paina3 paina4 paina5 paina6
$frame painb1 painb2 painb3 painb4 painb5 painb6 painb7 painb8 painb9 painb10
$frame painc1 painc2 painc3 painc4 painc5 painc6 painc7 painc8 painc9 painc10 painc11 painc12 painc13 painc14
$frame death1 death2 death3 death4 death5 death6 death7 death8 death9 death10 death11 death12 death13 death14 death15 death16

/*
================
stand, walk, run
================
*/

float FLOYD_ROLLING = 2;

float FLOYD_ASLEEP = 4;

void () floyd_sleep = [84, floyd_sleep] {ai_stand ();};

void () floyd_idle1 = [$idle1, floyd_idle1] {
    if (random () < 0.02)
        sound (self, CHAN_VOICE, "floyd/idle.wav", 1, ATTN_IDLE);
    ai_stand ();
};
void () floyd_walk1 = [$walk1, floyd_walk2]
{
    if (random () < 0.2)
        sound (self, CHAN_VOICE, "floyd/idle.wav", 1, ATTN_IDLE);
    ai_walk (1.875);
};
void () floyd_walk2 =  [$walk2,  floyd_walk3]  {ai_walk (1.875);};
void () floyd_walk3 =  [$walk3,  floyd_walk4]  {ai_walk (1.875);};
void () floyd_walk4 =  [$walk4,  floyd_walk5]  {ai_walk (1.875);};
void () floyd_walk5 =  [$walk5,  floyd_walk6]  {ai_walk (1.875);};
void () floyd_walk6 =  [$walk6,  floyd_walk7]  {ai_walk (1.875);};
void () floyd_walk7 =  [$walk7,  floyd_walk8]  {ai_walk (1.875);};
void () floyd_walk8 =  [$walk8,  floyd_walk9]  {ai_walk (1.875);};
void () floyd_walk9 =  [$walk9,  floyd_walk10] {ai_walk (1.875);};
void () floyd_walk10 = [$walk10, floyd_walk11] {ai_walk (1.875);};
void () floyd_walk11 = [$walk11, floyd_walk12] {ai_walk (1.875);};
void () floyd_walk12 = [$walk12, floyd_walk13] {ai_walk (1.875);};
void () floyd_walk13 = [$walk13, floyd_walk14] {ai_walk (1.875);};
void () floyd_walk14 = [$walk14, floyd_walk15] {ai_walk (1.875);};
void () floyd_walk15 = [$walk15, floyd_walk16] {ai_walk (1.875);};
void () floyd_walk16 = [$walk16, floyd_walk1]  {ai_walk (1.875);};

void () floyd_run1 = [$run1, floyd_run2] {ai_run (3.75);};
void () floyd_run2 = [$run2, floyd_run3] {ai_run (3.75);};
void () floyd_run3 = [$run3, floyd_run4] {ai_run (3.75);};
void () floyd_run4 = [$run4, floyd_run5] {ai_run (3.75);};
void () floyd_run5 = [$run5, floyd_run6] {ai_run (3.75);};
void () floyd_run6 = [$run6, floyd_run7] {ai_run (3.75);};
void () floyd_run7 = [$run7, floyd_run8] {ai_run (3.75);};
void () floyd_run8 = [$run8, floyd_run1] {ai_run (3.75);};

/*
================
attack
================
*/

void() FLaser_Touch =
{
    local vector org;
    
    if (other == self.owner)
        return;        // don't explode on owner

    if (pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }
    
    sound (self, CHAN_WEAPON, "enforcer/enfstop.wav", 1, ATTN_STATIC);
    org = self.origin - 10*normalize(self.velocity);

    if (other.health)
    {
        SpawnBlood (other, org, self.velocity*0.2, 15);
        T_Damage (other, self, self.owner, 15, DAMARMOR);
    }
    else
    {
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_GUNSHOT);
        WriteCoord (MSG_BROADCAST, org_x);
        WriteCoord (MSG_BROADCAST, org_y);
        WriteCoord (MSG_BROADCAST, org_z);
    }
    
    remove(self);    
};

void(vector org, vector vec) FLaunchLaser =
{        
    //local    vector    vec;
    
    if (self.classname == "monster_floyd")
        sound (self, CHAN_WEAPON, "floyd/fire.wav", 1, ATTN_NORM);

    
    newmis = spawn();
    newmis.owner = self;
    newmis.movetype = MOVETYPE_FLY;
    newmis.solid = SOLID_BBOX;
    newmis.effects = EF_DIMLIGHT;

    setmodel (newmis, "progs/flaser.mdl");
    setsize (newmis, '0 0 0', '0 0 0');        

    setorigin (newmis, org);

    vec = normalize(vec);
    newmis.velocity = vec * 800;
    newmis.angles = vectoangles(newmis.velocity);

    newmis.nextthink = time + 5;
    newmis.think = SUB_Remove;
    newmis.touch = FLaser_Touch;
};

void(float muzzle) floyd_fire =
{
    local vector org;

    self.effects = self.effects | EF_MUZZLEFLASH;
    makevectors (self.angles);
    
    org = self.origin + v_forward * 30 + '0 0 28' + v_right * muzzle;

    FLaunchLaser(org, self.enemy.origin - self.origin - '0 0 12');
};

void()    floyd_ready1    =[    $run1,        floyd_ready2    ] {ai_face();};
void()    floyd_ready2    =[    $run2,        floyd_ready3    ] {ai_face();};
void()    floyd_ready3    =[    $run3,        floyd_ready4    ] {ai_face();};
void()    floyd_ready4    =[    $run4,        floyd_attack1    ] {ai_face();
    if (!clean_shot (self.enemy)) self.think = floyd_run5; //abort if shot is blocked by monster or wall
};
void()    floyd_attack1    =[    $attack1,    floyd_attack2    ] {floyd_fire(6);};
void()    floyd_attack2    =[    $attack2,    floyd_attack3    ] {ai_face();};
void()    floyd_attack3    =[    $attack3,    floyd_attack4    ] {ai_face();};
void()    floyd_attack4    =[    $attack4,    floyd_attack5    ] {floyd_fire(0);};
void()    floyd_attack5    =[    $attack5,    floyd_attack6    ] {ai_face();};
void()    floyd_attack6    =[    $attack6,    floyd_attack7    ] {ai_face();};
void()    floyd_attack7    =[    $attack7,    floyd_attack8    ] {floyd_fire(-6);};
void()    floyd_attack8    =[    $attack8,    floyd_attack9    ] {ai_face();};
void()    floyd_attack9    =[    $attack9,    floyd_run1        ] { ai_face(); SUB_CheckRefire (floyd_attack1); };

/*
================
pain
================
*/

void () floyd_paina1 = [$paina1, floyd_paina2] {};
void () floyd_paina2 = [$paina2, floyd_paina3] {};
void () floyd_paina3 = [$paina3, floyd_paina4] {};
void () floyd_paina4 = [$paina4, floyd_paina5] {};
void () floyd_paina5 = [$paina5, floyd_paina6] {};
void () floyd_paina6 = [$paina6, floyd_run1]   {};

void () floyd_painb1 =  [$painb1,  floyd_painb2]  {};
void () floyd_painb2 =  [$painb2,  floyd_painb3]  {};
void () floyd_painb3 =  [$painb3,  floyd_painb4]  {};
void () floyd_painb4 =  [$painb4,  floyd_painb5]  {};
void () floyd_painb5 =  [$painb5,  floyd_painb6]  {};
void () floyd_painb6 =  [$painb6,  floyd_painb7]  {};
void () floyd_painb7 =  [$painb7,  floyd_painb8]  {};
void () floyd_painb8 =  [$painb8,  floyd_painb9]  {};
void () floyd_painb9 =  [$painb9,  floyd_painb10] {};
void () floyd_painb10 = [$painb10,  floyd_run1]   {};

void () floyd_painc1 =  [$painc1,  floyd_painc2]  {};
void () floyd_painc2 =  [$painc2,  floyd_painc3]  {};
void () floyd_painc3 =  [$painc3,  floyd_painc4]  {};
void () floyd_painc4 =  [$painc4,  floyd_painc5]  {};
void () floyd_painc5 =  [$painc5,  floyd_painc6]  {};
void () floyd_painc6 =  [$painc6,  floyd_painc7]  {};
void () floyd_painc7 =  [$painc7,  floyd_painc8]  {ai_painforward(1.25);};
void () floyd_painc8 =  [$painc8,  floyd_painc9]  {ai_painforward(1.25);};
void () floyd_painc9 =  [$painc9,  floyd_painc10] {ai_painforward(1.25);};
void () floyd_painc10 = [$painc10, floyd_painc11] {ai_painforward(1.25);};
void () floyd_painc11 = [$painc11, floyd_painc12] {ai_painforward(1.25);};
void () floyd_painc12 = [$painc12, floyd_painc13] {ai_painforward(1.25);};
void () floyd_painc13 = [$painc13, floyd_painc14] {ai_painforward(1.25);};
void () floyd_painc14 = [$painc14,  floyd_run1]   {ai_painforward(1.25);};

void(entity inflictor, entity attacker, float damage)    floyd_pain = {
    local float d;
    
    if (self.pain_finished > time)
        return;
        
    if (random()*150 > damage)
        return;        // didn't flinch
    
    sound (self, CHAN_VOICE, "floyd/pain.wav", 1, ATTN_NORM);

    if (self.spawnflags & FLOYD_ROLLING)
        return;
    
    d = anglemod(self.angles_y - self.ideal_yaw);

    if (d > 90 && d < 270) {
        //lurch forward if hit in the back
        floyd_painc1 ();
        self.pain_finished = time + 2;
    } else {
        if (random() < 0.4) {
                floyd_paina1 ();
                self.pain_finished = time + 1;
        } else {
                floyd_painb1 ();
                self.pain_finished = time + 2;
        }
    }

};

/*
================
death
================
*/

void(string gibname, float dm) ThrowFloydHead = {
    local entity head;
    head = spawn();
    head.origin = self.origin;
    setmodel (head, gibname);
    head.frame = 0;
    head.nextthink = -1;
    head.movetype = MOVETYPE_BOUNCE;
    head.takedamage = DAMAGE_NO;
    head.solid = SOLID_NOT;
    setsize (head, '-16 -16 0', '16 16 32');
    head.velocity = VelocityForDamage (dm);
    head.origin_z = self.origin_z + 32;
    head.angles_y = self.angles_y;
    head.flags = self.flags - (self.flags & FL_ONGROUND);
    head.avelocity = crandom() * '0 600 0';
};

void(string gibname, float dm) ThrowFloydGib =
{
    local    entity new;
    new = spawn();
    new.origin_x = self.origin_x - 16 + random()*32;
    new.origin_y = self.origin_y - 16 + random()*32;
    new.origin_z = self.origin_z - 32 + random()*64;
    setmodel (new, gibname);
    setsize (new, '0 0 0', '0 0 0');
    new.velocity_x = crandom() * 400;
    new.velocity_y = crandom() * 400;
    new.velocity_z = 200 + random() * 400;
    new.movetype = MOVETYPE_BOUNCE;
    new.solid = SOLID_NOT;
    new.avelocity_x = random()*600;
    new.avelocity_y = random()*600;
    new.avelocity_z = random()*600;
    new.think = SUB_Remove;
    new.ltime = time;
    new.nextthink = time + 10 + random()*10;
    new.flags = 0;        
};

void () floyd_explode = {
    local float gibs;

    T_RadiusDamage (self, self, 100, world, DAMAGEALL);
    if (self.health > -50) self.health = -50;
    if (!(self.spawnflags & FLOYD_ROLLING)) //don't throw head if you're on your back, cuz it looks funny
        ThrowFloydHead ("progs/h_floyd.mdl", self.health);
        
    //gibs
    gibs = 8;
    while (gibs > 0) {
        ThrowFloydGib (self.gib1mdl, self.health);
        gibs = gibs - 1;
    }
    
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
    BecomeExplosion ();
}

void () floyd_death2 =  [$death2,  floyd_death3]  {ai_painforward(-3.33);};
void () floyd_death3 =  [$death3,  floyd_death4]  {ai_painforward(-3.34);};
void () floyd_death4 =  [$death4,  floyd_death5]  {ai_painforward(-3.33);};
void () floyd_death5 =  [$death5,  floyd_death6]  {ai_painforward(-5);};
void () floyd_death6 =  [$death6,  floyd_death7]  {ai_painforward(-5);};
void () floyd_death7 =  [$death7,  floyd_death8]  {ai_painforward(-8);};
void () floyd_death8 =  [$death8,  floyd_death9]  {ai_painforward(-8);};
void () floyd_death9 =  [$death9,  floyd_death10] {ai_painforward(-8);};
void () floyd_death10 = [$death10, floyd_death11] {ai_painforward(-8);};
void () floyd_death11 = [$death11, floyd_death12] {};
void () floyd_death12 = [$death12, floyd_death13] {};
void () floyd_death13 = [$death13, floyd_death14] {};
void () floyd_death14 = [$death14, floyd_death15] {};
void () floyd_death15 = [$death15, floyd_explode] {self.spawnflags = FLOYD_ROLLING; self.takedamage = DAMAGE_NO;};

void() floyd_die = {
    //check for gib
    if (self.health < self.gibhealth || random() > 0.75 || self.spawnflags & FLOYD_ROLLING) {
        self.takedamage = DAMAGE_NO;
        floyd_explode();
    } else {
        sound (self, CHAN_VOICE, "floyd/death.wav", 1, ATTN_NORM);
        floyd_death2 (); //skip $death1, which is the idle frame
    }
};

void () floyd_roll = {
    self.frame = self.frame + 1;
    
    if (self.frame > $death15 || self.frame < $death10)
        self.frame = $death10;
    
    self.think = floyd_roll;
    self.nextthink = time + 0.1;
}

/*
================
this chunk of code figures out rolling floyd's ideal bounding box no matter at what angle he's placed.
================
*/

vector (vector testmoos, float testang, float testradius, float minmax) test_moos_angle = {
    local vector testangs, floyd_moos;
    floyd_moos = testmoos;
    testangs = '0 0 0';
    testangs_y = testang;
    makevectors(testangs); 
    v_forward_y = v_forward_y * testradius;
    v_forward_x = v_forward_x * testradius;
    if (minmax == 1) //1 for maxes, 0 for mins
    {
        if (v_forward_y > floyd_moos_y) floyd_moos_y = v_forward_y;
        if (v_forward_x > floyd_moos_x) floyd_moos_x = v_forward_x;
    } else {
        if (v_forward_y < floyd_moos_y) floyd_moos_y = v_forward_y;
        if (v_forward_x < floyd_moos_x) floyd_moos_x = v_forward_x;
    }
    return floyd_moos;
}

vector (entity floyd) get_floyd_mins =
{
    local float feetyaw;
    local vector floyd_mins;
    
    floyd_mins = '0 0 -24';
    feetyaw = anglemod(floyd.angles_y);

    floyd_mins = test_moos_angle(floyd_mins, feetyaw + 17.4, 38, 0); //foot
    floyd_mins = test_moos_angle(floyd_mins, feetyaw + 180 - 17.4, 38, 0); //shoulder
    floyd_mins = test_moos_angle(floyd_mins, feetyaw + 180, 40, 0); //head
    floyd_mins = test_moos_angle(floyd_mins, feetyaw + 180 + 17.4, 38, 0); //shoulder
    floyd_mins = test_moos_angle(floyd_mins, feetyaw + 360 - 17.4, 38, 0); //foot

    return floyd_mins;
}

vector (entity floyd) get_floyd_maxs = {
    local float feetyaw;
    local vector floyd_maxs;
    
    floyd_maxs = '0 0 12';
    feetyaw = anglemod(floyd.angles_y);

    floyd_maxs = test_moos_angle(floyd_maxs, feetyaw + 17.4, 38, 1); //foot
    floyd_maxs = test_moos_angle(floyd_maxs, feetyaw + 180 - 17.4, 38, 1); //shoulder
    floyd_maxs = test_moos_angle(floyd_maxs, feetyaw + 180, 40, 1); //head
    floyd_maxs = test_moos_angle(floyd_maxs, feetyaw + 180 + 17.4, 38, 1); //shoulder
    floyd_maxs = test_moos_angle(floyd_maxs, feetyaw + 360 - 17.4, 38, 1); //foot
    
    return floyd_maxs;
}

void () floyd_wake1 = [84, floyd_wake2] {};
void () floyd_wake2 = [83, floyd_wake3] {};
void () floyd_wake3 = [82, floyd_wake4] {};
void () floyd_wake4 = [81, floyd_idle1] {
    if (self.enemy) {
        monster_sightsound();
        FoundHuntTarget(TRUE);
        self.think = self.th_run;
        self.nextthink = time + 0.1;
    } else self.think = self.th_stand;
};


void() floyd_awake = {
    self.spawnstatue = FALSE;  // No longer stationary
    self.use = SUB_Null;   // no re-trigger
    self.spawnflags = self.spawnflags - FLOYD_ASLEEP;
    self.skin = 0;
    self.th_stand = floyd_idle1;
    self.th_walk = floyd_walk1;
    self.th_run = floyd_run1;
    self.th_pain = floyd_pain;
    self.th_missile = floyd_ready1;
    self.pain_flinch = self.health;
    self.pain_timeout = 2;
    self.pain_finished = time + self.pain_timeout;
    self.flags = self.flags | FL_MONSTER;
    self.think = floyd_wake1;
    self.nextthink = time + 0.1;
};

void(entity inflictor, entity attacker, float damage)    floyd_painawake = {
    floyd_awake();
}

//=============================================================================

/*QUAKED monster_floyd (1 0 0) (-32 -32 -24) (32 32 64) AMBUSH ROLLING ASLEEP
Automaton, 200 health points.

AMBUSH: floyd will only wake up on really seeing the player, not another monster getting angry.

ROLLING: floyd is on his back and helpless. explodes after taking 50 damage.

ASLEEP: floyd will not respond to anything unless he is shot or his targetname is triggered.
*/

void() monster_floyd = {    
    if (deathmatch) { remove(self); return; }

    precache_model2("progs/floyd.mdl");
    precache_model2("progs/flaser.mdl");
    precache_model2("progs/h_floyd.mdl");
    precache_model2("progs/floydgib1.mdl");
    
    precache_sound2("floyd/idle.wav");
    precache_sound2("floyd/sight.wav");
    precache_sound2("floyd/pain.wav");
    precache_sound2("floyd/fire.wav");
    precache_sound2("floyd/death.wav");
    precache_sound2("enforcer/enfstop.wav");
    
    self.mdl = "progs/floyd.mdl";
    self.headmdl = "progs/h_floyd.mdl";
    self.gib1mdl = "progs/floydgib1.mdl";
    if (self.health < 1) self.health = 200;
    self.gibhealth = -35;

    // Common specialty settings for FLOYD_ROLLING or FLOYD_ASLEEP spawnflags
    self.skin = 0;
    self.movetype = MOVETYPE_STEP;
    self.solid = SOLID_SLIDEBOX;
    self.th_die = floyd_die;
    self.no_liquiddmg = TRUE;        // Immune to slime/lava

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.classtype = CT_MONFLOYD;
    self.classgroup = CG_ROBOT;

    self.hittype = HIT_METAL;

    if (self.spawnflags & FLOYD_ROLLING) {
        setmodel(self, "progs/floyd.mdl");
        setsize(self, get_floyd_mins(self), get_floyd_maxs(self));
        self.health = 75;
        self.th_stand = floyd_roll;
        self.th_walk = floyd_roll;
        self.th_run = floyd_roll;
        self.th_pain = floyd_pain;
        self.takedamage = DAMAGE_AIM;
        self.flags = self.flags | FL_MONSTER;        
        total_monsters = total_monsters + 1;
        if (self.deathstring == "") self.deathstring = " rolled on the floor with an Automaton\n";
        self.pausetime = 99999999;
        self.th_stand ();
    } else if (self.spawnflags & FLOYD_ASLEEP) {
        setmodel(self, "progs/floyd.mdl");
        if (!self.bboxtype) self.bboxtype = BBOX_GIANT;
        setsize(self, '-24 -24 -24', '24 24 64');
        self.skin = 1;
        self.th_stand = floyd_sleep;
        self.th_run = floyd_sleep;
        self.th_pain = floyd_painawake;
        self.flags = self.flags | FL_MONSTER;
        self.think1 = floyd_awake;
        self.pain_longanim = FALSE;        // resistant to shadow axe
        self.spawnstatue = TRUE;
        self.reset_health = self.health;
        self.ideal_yaw = self.angles * '0 1 0';
        if (!self.yaw_speed) self.yaw_speed = 20;
        self.view_ofs = '0 0 25';
        self.flags = self.flags | FL_MONSTER;
        self.classmove = MON_MOVEWALK;
        if (self.deathstring == "") self.deathstring = " was made obsolete by an awoken Automaton\n";

        monster_start();
        self.takedamage = DAMAGE_AIM;
    } else {
        //normal operation
        if (!self.bboxtype) self.bboxtype = BBOX_GIANT;
        if (self.health < 1) self.health = 200;
        self.gibhealth = -35;
        self.movetype = MOVETYPE_NONE;
        self.solid = SOLID_NOT;
        self.th_stand = floyd_idle1;
        self.th_walk = floyd_walk1;
        self.th_pain = floyd_pain;
        self.th_run = floyd_run1;
        self.th_missile = floyd_ready1;
        if (self.deathstring == "") self.deathstring = " was made obsolete by an Automaton\n";

        self.classmove = MON_MOVEWALK;

        monster_start();
    }
};