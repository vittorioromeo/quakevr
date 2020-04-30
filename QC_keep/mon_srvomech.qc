/*
==============================================================================

SERVOMECH, MONSTER
by Carl Glave (General WarT)
==============================================================================
*/

// the servomech lasers
void(vector src, vector dir, float lsrdamage) FireLaser =
{
    local vector vel, org;
    local vector src;

    traceline (src, src + dir*2048, FALSE, self);


    org = trace_endpos - dir*4;

    if (trace_ent.takedamage && trace_ent != self)
    {
        vel = normalize(dir + v_up*crandom() + v_right*crandom());
        vel = vel + 2*trace_plane_normal;
        vel = vel * 200;

        T_Damage (trace_ent, self, self, lsrdamage);
        SpawnBlood (org, vel*0.2, lsrdamage, trace_ent);
    }
    else if(trace_fraction != 1)
    {
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_KNIGHTSPIKE);
        WriteCoord (MSG_BROADCAST, org_x);
        WriteCoord (MSG_BROADCAST, org_y);
        WriteCoord (MSG_BROADCAST, org_z);
    }

    // draw in the laser using the temp entity for a
    // Shambler's lightning with a new model
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_LIGHTNING1);
    WriteEntity (MSG_BROADCAST, self);
    WriteCoord (MSG_BROADCAST, src_x);
    WriteCoord (MSG_BROADCAST, src_y);
    WriteCoord (MSG_BROADCAST, src_z);
    WriteCoord (MSG_BROADCAST, trace_endpos_x);
    WriteCoord (MSG_BROADCAST, trace_endpos_y);
    WriteCoord (MSG_BROADCAST, trace_endpos_z);
};

//=======================================================================

float(float pitch) servobod_frame =
{return 15 + floor(pitch/5.3333);};

float(float pitch) servobod_fireframe =
{return 46 + floor(pitch/5.3333);};

float(float pitch) servobod_rightfireframe =
{return 77 + floor(pitch/5.3333);};

float(float pitch) servobod_leftfireframe =
{return 108 + floor(pitch/5.3333);};

float SERVOBOD_NONE = 0;
float SERVOBOD_BOTH = 1;
float SERVOBOD_RIGHT = 2;
float SERVOBOD_LEFT = 3;

void() servo_bodthink =
{
    local entity bod, tmpent;
    local vector tmpvec;

    bod = self.aiment;
    bod.skin = self.skin; // incase of beam weapon

    setorigin(bod, self.origin + self.pos1);

    tmpent = self;
    self = bod;

    if(tmpent.enemy)
    {
        if(visible(tmpent.enemy))
        {
//            tmpvec = vectoangles((tmpent.enemy.origin + tmpent.enemy.view_ofs) - (self.origin + '0 0 16'));
            tmpvec = vectoangles(tmpent.enemy.origin - (self.origin + '0 0 16'));
            self.ideal_yaw = tmpvec_y;
            if(tmpvec_x > 180)
            {
                tmpvec_x = 360 - tmpvec_x;
                if(tmpvec_x > 80)
                    tmpvec_x = 80;
            }
            else
            {
                tmpvec_x = tmpvec_x * -1;
                if(tmpvec_x < -80)
                    tmpvec_x = -80;
            }
            self.search_time = time + 2;
        }
        else if(self.search_time < time)
        {
            self.ideal_yaw = tmpent.angles_y;
            tmpvec = '0 0 0';
        }

        if(tmpvec_x > self.rotate_type)
        {
            if(tmpvec_x > (self.rotate_type + 6))
                self.rotate_type = self.rotate_type + 6;
            else
                self.rotate_type = tmpvec_x;
        }
        else if(tmpvec_x < self.rotate_type)
        {
            if(tmpvec_x < (self.rotate_type - 6))
                self.rotate_type = self.rotate_type - 6;
            else
                self.rotate_type = tmpvec_x;
        }
    }
    else if(self.invisible_time < time || fabs(self.ideal_yaw - tmpent.ideal_yaw) > 35)
    {
        self.ideal_yaw = tmpent.ideal_yaw + crandom()*35;
        self.rotate_type = 0;
        self.invisible_time = time + 1 + random()*3;
    }

    if(self.attack_state)
    {
        if(self.attack_state == SERVOBOD_BOTH)
            self.frame = servobod_fireframe(self.rotate_type);
        else if(self.attack_state == SERVOBOD_RIGHT)
            self.frame = servobod_rightfireframe(self.rotate_type);
        else if(self.attack_state == SERVOBOD_LEFT)
            self.frame = servobod_leftfireframe(self.rotate_type);
        self.attack_state = SERVOBOD_NONE;
    }
    else
        self.frame = servobod_frame(self.rotate_type);

    ChangeYaw();

    self = tmpent;
};

//=======================================================================

void() servo_stand =[ 0, servo_stand]
{ai_stand(); self.pos1 = '0 0 0'; servo_bodthink();};

//=======================================================================

void() servo_walk1 =[ 1, servo_walk2]
{ai_walk(5);self.pos1 = '0 0 11.4'; servo_bodthink();};
void() servo_walk2 =[ 2, servo_walk3]
{ai_walk(6);self.pos1 = '0 0 11.8'; servo_bodthink();};
void() servo_walk3 =[ 3, servo_walk4]
{ai_walk(7);self.pos1 = '0 0 11.3'; servo_bodthink();};
void() servo_walk4 =[ 4, servo_walk5]
{ai_walk(3);self.pos1 = '0 0 9.5'; servo_bodthink();};
void() servo_walk5 =[ 5, servo_walk6]
{ai_walk(3);self.pos1 = '0 0 8.2'; servo_bodthink();};
void() servo_walk6 =[ 6, servo_walk7]
{FootFall("mech/mechstep.wav", 0.3, 1, 0.8);ai_walk(2);self.pos1 = '0 0 5.4'; servo_bodthink();};
void() servo_walk7 =[ 7, servo_walk8]
{ai_walk(3);self.pos1 = '0 0 9.6'; servo_bodthink();};
void() servo_walk8 =[ 8, servo_walk9]
{ai_walk(6);self.pos1 = '0 0 10.7'; servo_bodthink();};
void() servo_walk9 =[ 9, servo_walk10]
{ai_walk(5);self.pos1 = '0 0 10.8'; servo_bodthink();};
void() servo_walk10 =[ 10, servo_walk11]
{ai_walk(4);self.pos1 = '0 0 11.8'; servo_bodthink();};
void() servo_walk11 =[ 11, servo_walk12]
{ai_walk(7);self.pos1 = '0 0 11.9'; servo_bodthink();};
void() servo_walk12 =[ 12, servo_walk13]
{ai_walk(7);self.pos1 = '0 0 11.3'; servo_bodthink();};
void() servo_walk13 =[ 13, servo_walk14]
{ai_walk(3);self.pos1 = '0 0 9.6'; servo_bodthink();};
void() servo_walk14 =[ 14, servo_walk15]
{ai_walk(3);self.pos1 = '0 0 8.3'; servo_bodthink();};
void() servo_walk15 =[ 15, servo_walk16]
{FootFall("mech/mechstep.wav", 0.3, 1, 0.8);ai_walk(2);self.pos1 = '0 0 5.8'; servo_bodthink();};
void() servo_walk16 =[ 16, servo_walk17]
{ai_walk(3);self.pos1 = '0 0 9.6'; servo_bodthink();};
void() servo_walk17 =[ 17, servo_walk18]
{ai_walk(6);self.pos1 = '0 0 10.7'; servo_bodthink();};
void() servo_walk18 =[ 18, servo_walk1]
{ai_walk(4);self.pos1 = '0 0 10.8'; servo_bodthink();};

//=======================================================================
void() servo_stillrun;

void() servo_run1 =[ 1, servo_run2]
{
    ai_run(9);
    self.pos1 = '0 0 11.4';
    servo_bodthink();
    if(enemy_range == RANGE_MELEE)
        if(enemy_vis)
            self.think = servo_stillrun;
};
void() servo_run2 =[ 3, servo_run3]
{
    ai_run(13);
    self.pos1 = '0 0 11.3';
    servo_bodthink();
    if(enemy_range == RANGE_MELEE)
        if(enemy_vis)
            self.think = servo_stillrun;
};
void() servo_run3 =[ 5, servo_run4]
{
    ai_run(6);
    self.pos1 = '0 0 8.2';
    servo_bodthink();
    if(enemy_range == RANGE_MELEE)
        if(enemy_vis)
            self.think = servo_stillrun;
};
void() servo_run4 =[ 7, servo_run5]
{
    FootFall("mech/mechstep.wav", 0.3, 1, 1);
    ai_run(5);
    self.pos1 = '0 0 9.6';
    servo_bodthink();
    if(enemy_range == RANGE_MELEE)
        if(enemy_vis)
            self.think = servo_stillrun;
};
void() servo_run5 =[ 9, servo_run6]
{
    ai_run(11);
    self.pos1 = '0 0 10.8';
    servo_bodthink();
    if(enemy_range == RANGE_MELEE)
        if(enemy_vis)
            self.think = servo_stillrun;
};
void() servo_run6 =[ 11, servo_run7]
{
    ai_run(11);
    self.pos1 = '0 0 11.9';
    servo_bodthink();
    if(enemy_range == RANGE_MELEE)
        if(enemy_vis)
            self.think = servo_stillrun;
};
void() servo_run7 =[ 13, servo_run8]
{
    ai_run(10);
    self.pos1 = '0 0 9.6';
    servo_bodthink();
    if(enemy_range == RANGE_MELEE)
        if(enemy_vis)
            self.think = servo_stillrun;
};
void() servo_run8 =[ 15, servo_run9]
{
    FootFall("mech/mechstep.wav", 0.3, 1, 1);
    ai_run(5);
    self.pos1 = '0 0 5.8';
    servo_bodthink();
    if(enemy_range == RANGE_MELEE)
        if(enemy_vis)
            self.think = servo_stillrun;
};
void() servo_run9 =[ 17, servo_run1]
{
    ai_run(9);
    self.pos1 = '0 0 10.7';
    servo_bodthink();
    if(enemy_range == RANGE_MELEE)
        if(enemy_vis)
            self.think = servo_stillrun;
};

void() servo_stillrun =[ 0, servo_stillrun]
{
    ai_run(0);
    self.pos1 = '0 0 0';
    servo_bodthink();
    if(enemy_range != RANGE_MELEE)
        self.think = servo_run1;
    if(!enemy_vis)
        self.think = servo_run1;
};

//=======================================================================

void(entity attacker, float damage) servo_pain=
{
    local float tmpflt;

    if(self.pain_finished < time)
    {
        self.aiment.rotate_type = self.aiment.rotate_type + crandom() * damage * 0.3;
        if(self.aiment.rotate_type < -80)
            self.aiment.rotate_type = -80;
        else if(self.aiment.rotate_type > 80)
            self.aiment.rotate_type = 80;

        servo_bodthink();
        self.pain_finished = time + 1 + random()*2;
    }
};

//=======================================================================

void() servo_die1 =[ 0, servo_die2] {self.nextthink = time + 1;};
void() servo_die2 =[ 1, servo_die3] {};
void() servo_die3 =[ 2, servo_die4] {};
void() servo_die4 =[ 3, servo_die5] {};
void() servo_die5 =[ 4, servo_die6] {};
void() servo_die6 =[ 5, servo_die7] {};
void() servo_die7 =[ 6, servo_die8] {};
void() servo_die8 =[ 7, servo_die9]
{sound (self, CHAN_VOICE, "mech/metal.wav", 1, ATTN_NORM);};
void() servo_die9 =[ 8, servo_die10] {};
void() servo_die10 =[ 9, servo_die11] {};
void() servo_die11 =[ 10, servo_die12] {};
void() servo_die12 =[ 11, servo_die13]
{sound (self, CHAN_VOICE, "mech/metal.wav", 0.75, ATTN_NORM);};
void() servo_die13 =[ 12, servo_die14] {};
void() servo_die14 =[ 13, servo_die15]
{sound (self, CHAN_VOICE, "mech/metal.wav", 0.5, ATTN_NORM);};
void() servo_die15 =[ 14, servo_die16] {};
void() servo_die16 =[ 15, servo_die17]
{sound (self, CHAN_VOICE, "mech/metal.wav", 0.25, ATTN_NORM);};
void() servo_die17 =[ 15, servo_die17] {};

void() servo_gibbed1 =[ 16, servo_gibbed2] {self.nextthink = time + 1;};
void() servo_gibbed2 =[ 17, servo_gibbed3] {};
void() servo_gibbed3 =[ 18, servo_gibbed4] {};
void() servo_gibbed4 =[ 19, servo_gibbed5] {};
void() servo_gibbed5 =[ 20, servo_gibbed6] {};
void() servo_gibbed6 =[ 21, servo_gibbed7]
{
    local vector tmpvec;
    local entity tmpself;

    tmpself = self;
    makevectors(self.angles);
    tmpvec = self.origin + v_forward*16;

    self = spawn();
    setorigin(self, tmpvec);
    BecomeBigExplosion();

    self = spawn();
    setorigin(self, tmpvec);
    self.angles = tmpself.angles;
    self.skin = tmpself.skin;
    ThrowHead("progs/h_servo.mdl", -100);
    self.velocity = v_forward*300 + '0 0 250';
    sound (self, CHAN_VOICE, "drake/misc/shortexp.wav", 1, ATTN_NORM);    
};
void() servo_gibbed7 =[ 22, servo_gibbed8] {};
void() servo_gibbed8 =[ 23, servo_gibbed9] {};
void() servo_gibbed9 =[ 24, servo_gibbed10]
{sound (self, CHAN_VOICE, "mech/metal.wav", 0.75, ATTN_NORM);};
void() servo_gibbed10 =[ 25, servo_gibbed11] {};
void() servo_gibbed11 =[ 26, servo_gibbed12] {};
void() servo_gibbed12 =[ 27, servo_gibbed13] {};
void() servo_gibbed13 =[ 28, servo_gibbed14] {};
void() servo_gibbed14 =[ 29, servo_gibbed15] {};
void() servo_gibbed15 =[ 30, servo_gibbed16] {};
void() servo_gibbed16 =[ 31, servo_gibbed17] {};
void() servo_gibbed17 =[ 32, servo_gibbed18] {};
void() servo_gibbed18 =[ 33, servo_gibbed19] {};
void() servo_gibbed19 =[ 34, servo_gibbed20]
{sound (self, CHAN_VOICE, "mech/metal.wav", 0.5, ATTN_NORM);};
void() servo_gibbed20 =[ 35, servo_gibbed21] {};
void() servo_gibbed21 =[ 36, servo_gibbed22] {};
void() servo_gibbed22 =[ 37, servo_gibbed23] {};
void() servo_gibbed23 =[ 38, servo_gibbed24]
{sound (self, CHAN_VOICE, "mech/metal.wav", 0.25, ATTN_NORM);};
void() servo_gibbed24 =[ 38, servo_gibbed24] {};

void() servo_die =
{
    self.solid = SOLID_NOT;
    sound (self, CHAN_VOICE, "mech/pow_dn.wav", 1, ATTN_NORM);
    remove(self.aiment);
    setmodel(self, "progs/servodie.mdl");

// check for gib
    if (self.health < -40 || random() < 0.1)
    {
        servo_gibbed1();
        return;
    }

    servo_die1();    
};

//=======================================================================

void() servo_attack=
{
    local vector dir, org;

    dir = self.enemy.origin - (self.enemy.velocity*0.2 + self.enemy.velocity*0.3*random());

    org = self.aiment.angles;
    org_x = org_x + self.aiment.rotate_type;
    makevectors(org);

    org = self.aiment.origin + v_forward*37 + '0 0 16';
    if(self.lefty)
    {
        dir = normalize (dir - (self.aiment.origin + '0 0 16' - v_right*18));
        org = org - v_right*18;
        self.aiment.attack_state = SERVOBOD_LEFT;
    }
    else
    {
        dir = normalize (dir - (self.aiment.origin + '0 0 16' + v_right*18));
        org = org + v_right*18;
        self.aiment.attack_state = SERVOBOD_RIGHT;
    }
    self.lefty = 1 - self.lefty;

    sound (self, CHAN_WEAPON, "enforcer/enfire.wav", 1, ATTN_NORM);

    FireLaser(org, dir, 2);
};

void() servo_vollyattack=
{
    servo_attack();
    servo_attack();
    self.aiment.attack_state = SERVOBOD_BOTH;
};

//=======================================================================

float(entity targ) ServoInfront=
{
    local vector vec;
    local float dot;

    vec = self.aiment.angles;
    vec_x = vec_x + self.aiment.rotate_type;
    makevectors (vec);
    vec = normalize (targ.origin - self.aiment.origin);
    dot = vec * v_forward;

    if ( dot > 0.7)
    {
        return TRUE;
    }
    return FALSE;
};

float() ServomechCheckAttack=
{
    local vector    spot1, spot2;    
    local entity    targ;
    local float        chance;

    targ = self.enemy;
    
    if(!ServoInfront(self.enemy))
        return FALSE;

// see if any entities are in the way of the shot
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;

    traceline (spot1, spot2, FALSE, self);

    if (trace_ent != targ)
        return FALSE;        // don't have a clear shot
            
    if (trace_inopen && trace_inwater)
        return FALSE;            // sight line crossed contents

    if (time < self.attack_finished)
        return FALSE;

    if (enemy_range == RANGE_MELEE)
    {
        self.th_missile ();
        SUB_AttackFinished (0.2);
        return TRUE;
    }
        
    if (enemy_range == RANGE_FAR)
        chance = 0.05;
    else if (enemy_range == RANGE_NEAR)
        chance = 0.3;
    else if (enemy_range == RANGE_MID)
        chance = 0.15;
    else
        return FALSE;

    if (random () < chance)
    {
        self.th_missile ();
        SUB_AttackFinished (0.2);
        return TRUE;
    }

    return FALSE;
};

//=======================================================================

void() monster_servomech =
{
    local entity bodent;

    if (deathmatch)
    {
        remove(self);
        return;
    }

    precache_model ("progs/servobod.mdl");
    precache_model ("progs/servoleg.mdl");
    precache_model ("progs/servodie.mdl");
    precache_model ("progs/h_servo.mdl");

    precache_sound ("mech/mechstep.wav");
    precache_sound ("mech/pow_up.wav");
    precache_sound ("mech/pow_dn.wav");
    precache_sound ("mech/metal.wav");
    precache_sound ("drake/misc/shortexp.wav");
    precache_sound ("enforcer/enfire.wav");

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;

    setmodel (self, "progs/servoleg.mdl");

    setsize (self, '-32 -32 -48', '32 32 28');
    self.health = 500;
    self.pos1 = '0 0 0';
    self.yaw_speed = 30;
    self.view_ofs = '0 0 12';
    if(!self.skin)
        self.skin = 1;

    self.th_stand = servo_stand;
    self.th_walk = servo_walk1;
    self.th_run = servo_run1;
    self.th_die = servo_die;
    self.th_melee = servo_vollyattack;
    self.th_missile = servo_attack;
    self.th_pain = servo_pain;
    
    
// ### Added for Custents all affecting gravity
    if(!self.gravity)
        self.gravity = 1;
    else
          self.gravity = ((self.gravity - 1) / 100);
// ###

    // setup upper body entity
    bodent = spawn();
    bodent.solid = SOLID_NOT;
    bodent.movetype = MOVETYPE_NONE;
    setmodel (bodent, "progs/servobod.mdl");
    setorigin(bodent, self.origin);
    bodent.angles = self.angles;
    bodent.yaw_speed = 10;
    bodent.rotate_type = 0;
    bodent.skin = self.skin;
    self.aiment = bodent;

    walkmonster_start();
};


/*
==============================================================================

SERVOMECH, CONTROLLABLE

==============================================================================
*/

void(entity targent) cservo_control_on=
{
    local entity targcam;

    targcam = self.movetarget;

    targent.rcam = targcam;
    targcam.owner = targent;
    targent.weaponmodel = "progs/frame.mdl";

    dummy_setup(targent,self.walkframe, self.invincible_finished);

    msg_entity = targent;
    WriteByte (MSG_ONE, SVC_SETVIEWANGLES);
    WriteAngle( MSG_ONE, self.aiment.angles_x); 
    WriteAngle( MSG_ONE, self.aiment.angles_y);
    WriteAngle( MSG_ONE, self.aiment.angles_z);
    stuffcmd(targent, "exec mech.cfg\n");
    stuffcmd(targent, "wait\n");
    targent.camofs = '0 160 40';
    sprint(targent, "Press F7 to Toggle the Apoc Cam\n");

    sound (self, CHAN_VOICE, "mech/pow_up.wav", 1, ATTN_NORM);
};

void(entity targent) cservo_control_off=
{
    local entity targcam, tmpent;

    targcam = self.movetarget;

    dummy_remove(targent);

    targent.rcam = world;
    targcam.owner = world;

    targent.camofs = '0 64 30';
    if(targent.cament)
    {
        msg_entity = targent;
        WriteByte (MSG_ONE, SVC_SETVIEWPORT);
        WriteEntity (MSG_ONE, targent.cament);
        stuffcmd(targent, "cl_bob 0; wait\n");
    }
    else
    {
        tmpent = self;
        self = targent;
        W_SetCurrentAmmo();
        self = tmpent;
    }

    stuffcmd(targent, "exec config.cfg\n");
    stuffcmd(targent, "wait\n");
    stuffcmd(targent, "exec autoexec.cfg\n");
    stuffcmd(targent, "wait\n");
};

//=======================================================================

void() cservo_attack;
void() cservo_stand;
void() cservo_think=
{
    local entity contplyr, temp;

    contplyr = self.movetarget.owner;
    if(contplyr)
    {
        if(contplyr.deadflag != DEAD_NO)
            cservo_control_off(contplyr);
        else if(contplyr.rcam != self.movetarget)
        {
            self.movetarget.owner = world;
            self.style = 0;
            cservo_stand();
        }
        // this is done to make Quake use the proper VIS information for the view
        else
        {
            setorigin(contplyr, self.movetarget.origin);
            // check is external camera adjustment is needed
            if(contplyr.cament)
            {
                temp = self;
                self = contplyr;
                Cam_position();
                self = temp;
            }
        }
    }

    if (self.flags & FL_GODMODE)
    {
        if (!(contplyr.flags & FL_GODMODE))
            self.flags = self.flags - FL_GODMODE;
    }
    else if (contplyr.flags & FL_GODMODE)
        self.flags = self.flags | FL_GODMODE;

    if(self.attack_state)
    {
        cservo_attack();
        self.attack_state = 0;
    }
};

//=======================================================================

void() cservo_attack=
{
    if(self.attack_finished > time)
        return;
    local vector dir, org, fireang, targ;

    fireang = self.aiment.angles;
    fireang_x = self.aiment.rotate_type; // scale down a bit for proper alignment
    makevectors(fireang);

    org = self.aiment.origin + v_forward*38 + v_up*17;

    if(self.lefty)
    {
        traceline(org - v_forward*16 - v_right*8, org + v_forward*2048 - v_right*8, FALSE, self.aiment);
        targ = trace_endpos;
        org = org - v_right*18;
        self.aiment.attack_state = SERVOBOD_LEFT;
        self.aiment.frame = servobod_leftfireframe(self.aiment.rotate_type);
    }
    else
    {
        traceline(org - v_forward*16 + v_right*8, org + v_forward*2048 + v_right*8, FALSE, self.aiment);
        targ = trace_endpos;
        org = org + v_right*18;
        self.aiment.attack_state = SERVOBOD_RIGHT;
        self.aiment.frame = servobod_rightfireframe(self.aiment.rotate_type);
    }
    self.lefty = 1 - self.lefty;

    dir = normalize(targ - org);
    FireLaser(org, dir, 4);

    sound (self, CHAN_WEAPON, "enforcer/enfire.wav", 1, ATTN_NORM);

    self.attack_finished = time + 0.15;
    self.movetarget.owner.attack_finished = time + 1;
    self.show_hostile = time + 1;
};

//=======================================================================

void() cservo_bodthink =
{
    local entity bod, tmpent, contplyr;
    local vector tmpvec, viewang;

    bod = self.aiment;
    bod.skin = self.skin;

    droptofloor();

    setorigin(bod, self.origin + self.pos1);

    tmpent = self;
    self = bod;
    contplyr = self.movetarget.owner;

    if(contplyr)
    {
        tmpvec = contplyr.v_angle;
        self.ideal_yaw = tmpvec_y;
        if(tmpvec_x > self.rotate_type)
        {
            if(tmpvec_x > (self.rotate_type + 6))
                self.rotate_type = self.rotate_type + 6;
            else
                self.rotate_type = tmpvec_x;
        }
        else if(tmpvec_x < self.rotate_type)
        {
            if(tmpvec_x < (self.rotate_type - 6))
                self.rotate_type = self.rotate_type - 6;
            else
                self.rotate_type = tmpvec_x;
        }
    }

    if(self.attack_state)
    {
        if(self.attack_state == SERVOBOD_BOTH)
            self.frame = servobod_fireframe(self.rotate_type);
        else if(self.attack_state == SERVOBOD_RIGHT)
            self.frame = servobod_rightfireframe(self.rotate_type);
        else if(self.attack_state == SERVOBOD_LEFT)
            self.frame = servobod_leftfireframe(self.rotate_type);
        self.attack_state = SERVOBOD_NONE;
    }
    else
        self.frame = servobod_frame(self.rotate_type);

    ChangeYaw();

    // position the cockpit and camera
    tmpvec = self.angles;
    tmpvec_x = self.rotate_type;
    self.movetarget.angles = tmpvec;
    self.movetarget.angles_x = self.movetarget.angles_x * -1;
    makevectors(tmpvec);
    tmpvec = self.origin + v_forward*5 + v_up*16;
    setorigin(self.movetarget, tmpvec);

    self = tmpent;
};

//=======================================================================

void() cservo_stand =[ 0, cservo_stand]
{self.pos1 = '0 0 0'; cservo_bodthink();cservo_think();};

void() cservo_run1 =[ 1, cservo_run2]
{walkmove(self.ideal_yaw, 9);self.pos1 = '0 0 11.4';cservo_bodthink();cservo_think();};
void() cservo_run2 =[ 3, cservo_run3]
{walkmove(self.ideal_yaw, 13);self.pos1 = '0 0 11.3';cservo_bodthink();cservo_think();};
void() cservo_run3 =[ 5, cservo_run4]
{walkmove(self.ideal_yaw, 6);self.pos1 = '0 0 8.2';cservo_bodthink();cservo_think();};
void() cservo_run4 =[ 7, cservo_run5]
{FootFall("mech/mechstep.wav", 0.3, 1, 1);walkmove(self.ideal_yaw, 5);self.pos1 = '0 0 9.6';cservo_bodthink();cservo_think();};
void() cservo_run5 =[ 9, cservo_run6]
{walkmove(self.ideal_yaw, 11);self.pos1 = '0 0 10.8';cservo_bodthink();cservo_think();};
void() cservo_run6 =[ 11, cservo_run7]
{walkmove(self.ideal_yaw, 11);self.pos1 = '0 0 11.9';cservo_bodthink();cservo_think();};
void() cservo_run7 =[ 13, cservo_run8]
{walkmove(self.ideal_yaw, 10);self.pos1 = '0 0 9.6';cservo_bodthink();cservo_think();};
void() cservo_run8 =[ 15, cservo_run9]
{FootFall("mech/mechstep.wav", 0.3, 1, 1);walkmove(self.ideal_yaw, 5);self.pos1 = '0 0 5.8';cservo_bodthink();cservo_think();};
void() cservo_run9 =[ 17, cservo_run1]
{walkmove(self.ideal_yaw, 9);self.pos1 = '0 0 10.7';cservo_bodthink();cservo_think();};

void() cservo_walk1 =[ 1, cservo_walk2]
{walkmove(self.ideal_yaw, 5);self.pos1 = '0 0 11.4'; cservo_bodthink();cservo_think();};
void() cservo_walk2 =[ 2, cservo_walk3]
{walkmove(self.ideal_yaw, 6);self.pos1 = '0 0 11.8'; cservo_bodthink();cservo_think();};
void() cservo_walk3 =[ 3, cservo_walk4]
{walkmove(self.ideal_yaw, 7);self.pos1 = '0 0 11.3'; cservo_bodthink();cservo_think();};
void() cservo_walk4 =[ 4, cservo_walk5]
{walkmove(self.ideal_yaw, 3);self.pos1 = '0 0 9.5'; cservo_bodthink();cservo_think();};
void() cservo_walk5 =[ 5, cservo_walk6]
{walkmove(self.ideal_yaw, 3);self.pos1 = '0 0 8.2'; cservo_bodthink();cservo_think();};
void() cservo_walk6 =[ 6, cservo_walk7]
{walkmove(self.ideal_yaw, 3);FootFall("mech/mechstep.wav", 0.3, 1, 0.8);self.pos1 = '0 0 5.4'; cservo_bodthink();cservo_think();};
void() cservo_walk7 =[ 7, cservo_walk8]
{walkmove(self.ideal_yaw, 6);self.pos1 = '0 0 9.6'; cservo_bodthink();cservo_think();};
void() cservo_walk8 =[ 8, cservo_walk9]
{walkmove(self.ideal_yaw, 5);self.pos1 = '0 0 10.7'; cservo_bodthink();cservo_think();};
void() cservo_walk9 =[ 9, cservo_walk10]
{walkmove(self.ideal_yaw, 4);self.pos1 = '0 0 10.8'; cservo_bodthink();cservo_think();};
void() cservo_walk10 =[ 10, cservo_walk11]
{walkmove(self.ideal_yaw, 3);self.pos1 = '0 0 11.8'; cservo_bodthink();cservo_think();};
void() cservo_walk11 =[ 11, cservo_walk12]
{walkmove(self.ideal_yaw, 3);self.pos1 = '0 0 11.9'; cservo_bodthink();cservo_think();};
void() cservo_walk12 =[ 12, cservo_walk13]
{walkmove(self.ideal_yaw, 7);self.pos1 = '0 0 11.3'; cservo_bodthink();cservo_think();};
void() cservo_walk13 =[ 13, cservo_walk14]
{walkmove(self.ideal_yaw, 7);self.pos1 = '0 0 9.6'; cservo_bodthink();cservo_think();};
void() cservo_walk14 =[ 14, cservo_walk15]
{walkmove(self.ideal_yaw, 3);self.pos1 = '0 0 8.3'; cservo_bodthink();cservo_think();};
void() cservo_walk15 =[ 15, cservo_walk16]
{walkmove(self.ideal_yaw, 3);FootFall("mech/mechstep.wav", 0.3, 1, 0.8);self.pos1 = '0 0 5.8'; cservo_bodthink();cservo_think();};
void() cservo_walk16 =[ 16, cservo_walk17]
{walkmove(self.ideal_yaw, 3);self.pos1 = '0 0 9.6'; cservo_bodthink();cservo_think();};
void() cservo_walk17 =[ 17, cservo_walk18]
{walkmove(self.ideal_yaw, 6);self.pos1 = '0 0 10.7'; cservo_bodthink();cservo_think();};
void() cservo_walk18 =[ 18, cservo_walk1]
{walkmove(self.ideal_yaw, 4);self.pos1 = '0 0 10.8'; cservo_bodthink();cservo_think();};

void() cservo_back1 =[ 18, cservo_back2]
{walkmove(self.ideal_yaw+180, 4);self.pos1 = '0 0 10.8'; cservo_bodthink();cservo_think();};
void() cservo_back2 =[ 17, cservo_back3]
{walkmove(self.ideal_yaw+180, 6);self.pos1 = '0 0 10.7'; cservo_bodthink();cservo_think();};
void() cservo_back3 =[ 16, cservo_back4]
{walkmove(self.ideal_yaw+180, 3);self.pos1 = '0 0 9.6'; cservo_bodthink();cservo_think();};
void() cservo_back4 =[ 15, cservo_back5]
{walkmove(self.ideal_yaw+180, 3);self.pos1 = '0 0 5.8'; cservo_bodthink();cservo_think();};
void() cservo_back5 =[ 14, cservo_back6]
{walkmove(self.ideal_yaw+180, 3);self.pos1 = '0 0 8.3'; cservo_bodthink();cservo_think();};
void() cservo_back6 =[ 13, cservo_back7]
{walkmove(self.ideal_yaw+180, 7);FootFall("mech/mechstep.wav", 0.3, 1, 0.8);self.pos1 = '0 0 9.6'; cservo_bodthink();cservo_think();};
void() cservo_back7 =[ 12, cservo_back8]
{walkmove(self.ideal_yaw+180, 7);self.pos1 = '0 0 11.3'; cservo_bodthink();cservo_think();};
void() cservo_back8 =[ 11, cservo_back9]
{walkmove(self.ideal_yaw+180, 3);self.pos1 = '0 0 11.9'; cservo_bodthink();cservo_think();};
void() cservo_back9 =[ 10, cservo_back10]
{walkmove(self.ideal_yaw+180, 3);self.pos1 = '0 0 11.8'; cservo_bodthink();cservo_think();};
void() cservo_back10 =[ 9, cservo_back11]
{walkmove(self.ideal_yaw+180, 4);self.pos1 = '0 0 10.8'; cservo_bodthink();cservo_think();};
void() cservo_back11 =[ 8, cservo_back12]
{walkmove(self.ideal_yaw+180, 5);self.pos1 = '0 0 10.7'; cservo_bodthink();cservo_think();};
void() cservo_back12 =[ 7, cservo_back13]
{walkmove(self.ideal_yaw+180, 6);self.pos1 = '0 0 9.6'; cservo_bodthink();cservo_think();};
void() cservo_back13 =[ 6, cservo_back14]
{walkmove(self.ideal_yaw+180, 3);self.pos1 = '0 0 5.4'; cservo_bodthink();cservo_think();};
void() cservo_back14 =[ 5, cservo_back15]
{walkmove(self.ideal_yaw+180, 3);self.pos1 = '0 0 8.2'; cservo_bodthink();cservo_think();};
void() cservo_back15 =[ 4, cservo_back16]
{walkmove(self.ideal_yaw+180, 3);FootFall("mech/mechstep.wav", 0.3, 1, 0.8);self.pos1 = '0 0 9.5'; cservo_bodthink();cservo_think();};
void() cservo_back16 =[ 3, cservo_back17]
{walkmove(self.ideal_yaw+180, 7);self.pos1 = '0 0 11.3'; cservo_bodthink();cservo_think();};
void() cservo_back17 =[ 2, cservo_back18]
{walkmove(self.ideal_yaw+180, 6);self.pos1 = '0 0 11.8'; cservo_bodthink();cservo_think();};
void() cservo_back18 =[ 1, cservo_back1]
{walkmove(self.ideal_yaw+180, 5);self.pos1 = '0 0 11.4'; cservo_bodthink();cservo_think();};

void() cservo_turn1 =[ 1, cservo_turn2]
{self.pos1 = '0 0 11.4'; cservo_bodthink();cservo_think();};
void() cservo_turn2 =[ 2, cservo_turn3]
{self.pos1 = '0 0 11.8'; cservo_bodthink();cservo_think();};
void() cservo_turn3 =[ 3, cservo_turn4]
{self.pos1 = '0 0 11.3'; cservo_bodthink();cservo_think();};
void() cservo_turn4 =[ 4, cservo_turn5]
{self.pos1 = '0 0 9.5'; cservo_bodthink();cservo_think();};
void() cservo_turn5 =[ 5, cservo_turn6]
{self.pos1 = '0 0 8.2'; cservo_bodthink();cservo_think();};
void() cservo_turn6 =[ 6, cservo_turn7]
{FootFall("mech/mechstep.wav", 0.3, 1, 0.8);self.pos1 = '0 0 5.4'; cservo_bodthink();cservo_think();};
void() cservo_turn7 =[ 7, cservo_turn8]
{self.pos1 = '0 0 9.6'; cservo_bodthink();cservo_think();};
void() cservo_turn8 =[ 8, cservo_turn9]
{self.pos1 = '0 0 10.7'; cservo_bodthink();cservo_think();};
void() cservo_turn9 =[ 9, cservo_turn10]
{self.pos1 = '0 0 10.8'; cservo_bodthink();cservo_think();};
void() cservo_turn10 =[ 10, cservo_turn11]
{self.pos1 = '0 0 11.8'; cservo_bodthink();cservo_think();};
void() cservo_turn11 =[ 11, cservo_turn12]
{self.pos1 = '0 0 11.9'; cservo_bodthink();cservo_think();};
void() cservo_turn12 =[ 12, cservo_turn13]
{self.pos1 = '0 0 11.3'; cservo_bodthink();cservo_think();};
void() cservo_turn13 =[ 13, cservo_turn14]
{self.pos1 = '0 0 9.6'; cservo_bodthink();cservo_think();};
void() cservo_turn14 =[ 14, cservo_turn15]
{self.pos1 = '0 0 8.3'; cservo_bodthink();cservo_think();};
void() cservo_turn15 =[ 15, cservo_turn16]
{FootFall("mech/mechstep.wav", 0.3, 1, 0.8);self.pos1 = '0 0 5.8'; cservo_bodthink();cservo_think();};
void() cservo_turn16 =[ 16, cservo_turn17]
{self.pos1 = '0 0 9.6'; cservo_bodthink();cservo_think();};
void() cservo_turn17 =[ 17, cservo_turn18]
{self.pos1 = '0 0 10.7'; cservo_bodthink();cservo_think();};
void() cservo_turn18 =[ 18, cservo_turn1]
{self.pos1 = '0 0 10.8'; cservo_bodthink();cservo_think();};

//=======================================================================

void(entity attacker, float damage) cservo_pain=
{
    local float tmpflt;

    if(self.pain_finished < time)
    {
        self.aiment.rotate_type = self.aiment.rotate_type + crandom() * damage * 0.3;
        if(self.aiment.rotate_type < -80)
            self.aiment.rotate_type = -80;
        else if(self.aiment.rotate_type > 80)
            self.aiment.rotate_type = 80;

        cservo_bodthink();
        self.pain_finished = time + 2 + random()*2;
        
        // display servomech status to controller
        if(self.movetarget.owner)
        {
            tmpflt = self.health / 8;
            
            if(tmpflt > 90)
                centerprint(self.movetarget.owner, "\n\n\n\nServomech at 100%                       ");
            else if(tmpflt > 80)
                centerprint(self.movetarget.owner, "\n\n\n\nServomech at  90%                       ");
            else if(tmpflt > 70)
                centerprint(self.movetarget.owner, "\n\n\n\nServomech at  80%                       ");
            else if(tmpflt > 60)
                centerprint(self.movetarget.owner, "\n\n\n\nServomech at  70%                       ");
            else if(tmpflt > 50)
                centerprint(self.movetarget.owner, "\n\n\n\nServomech at  60%                       ");
            else if(tmpflt > 40)
                centerprint(self.movetarget.owner, "\n\n\n\nServomech at  50%                       ");
            else if(tmpflt > 30)
                centerprint(self.movetarget.owner, "\n\n\n\nServomech at  40%                       ");
            else if(tmpflt > 20)
                centerprint(self.movetarget.owner, "\n\n\n\nServomech at  30%                       ");
            else if(tmpflt > 10)
                centerprint(self.movetarget.owner, "\n\n\n\nServomech at  20%                       ");
            else if(tmpflt > 0)
                centerprint(self.movetarget.owner, "\n\n\n\nServomech at  10%                       ");
            else
                centerprint(self.movetarget.owner, "\n\n\n\nServomech destroyed...                  ");
        }
    }
};

void() cservo_die=
{
    if(self.movetarget.owner)
        cservo_control_off(self.movetarget.owner);

    remove(self.movetarget);
    self.targetname = string_null;

    servo_die();
};

//=======================================================================

void() cservo_apply_controlls=
{
    local entity stemp;
    local float tmpflt;

    stemp = self;
    self = self.rcam.spawnmaster;

    if(stemp.impulse == 248) // stop using the mech
    {
        cservo_control_off(stemp);
        self = stemp;
        return;
    }

    if(stemp.impulse == 247) // run forward
    {
        if(self.style != 4)
        {
            self.think = cservo_run1;
            self.style = 4;
        }
        stemp.impulse = 0;
    }
    else if(stemp.impulse == 240) // walk forward
    {
        if(self.style != 1)
        {
            self.think = cservo_walk1;
            self.style = 1;
        }
        stemp.impulse = 0;
    }
    else if(stemp.impulse == 241) // walk backwards
    {
        if(self.style != 2)
        {
            self.think = cservo_back1;
            self.style = 2;
        }
        stemp.impulse = 0;
    }
    else if(stemp.impulse == 242) // turn left
    {
        self.angles_y = self.angles_y + 80*frametime;
        self.angles_y = anglemod(self.angles_y);
        self.ideal_yaw = self.angles_y;
        if(self.style == 0)
        {
            self.think = cservo_turn1;
            self.style = 3;
        }
    }
    else if(stemp.impulse == 243) // turn right
    {
        self.angles_y = self.angles_y - 80*frametime;
        self.angles_y = anglemod(self.angles_y);
        self.ideal_yaw = self.angles_y;
        if(self.style == 0)
        {
            self.think = cservo_turn1;
            self.style = 3;
        }
    }
    else if(stemp.impulse == 244) // straighten legs
    {
        tmpflt = fabs(self.ideal_yaw - stemp.v_angle *'0 1 0');
        if(tmpflt > 10 && tmpflt < 350)
        {
            self.ideal_yaw = stemp.v_angle *'0 1 0';
            ChangeYaw();
            self.ideal_yaw = self.angles_y;
            if(self.style == 0)
            {
                self.think = cservo_turn1;
                self.style = 3;
            }
        }
        else
        {
            self.ideal_yaw = stemp.v_angle *'0 1 0';
            self.angles_y = self.ideal_yaw;
            if(self.style == 3)
            {
                self.think = cservo_stand;
                self.style = 0;
            }
            stemp.impulse = 0;
        }
    }
    else if(stemp.impulse == 245) // stop
    {
        self.think = cservo_stand;
        self.style = 0;
        stemp.impulse = 0;
    }
    else if(stemp.impulse == 246) // stop turning
    {
        if(self.style == 3)
        {
            self.think = cservo_stand;
            self.style = 0;
        }
        stemp.impulse = 0;
    }
    else if(stemp.impulse == APOCCAM_IMPULSE) // allow toggling the Apoc Cam
        Cam_toggle();

    if(stemp.button0)
        self.attack_state = 1;

    self = stemp;
    self.frame = 0;
};

//=======================================================================

//entity currcheckmech; // the current controlled servomech to check for attacking.

// now also has added targeting for the assasin and the mech
// so that bugs will attack them as well
void() cmech_attselect=
{
    self.nextthink = time + 0.1;

    while(1)
    {
        checkmech = find(checkmech, classname, "controlled_servomech");
        if(!checkmech)
            return;

//        if(checkmech.classname == "controlled_servomech")
            if(checkmech.movetarget.owner.flags & FL_CLIENT)
                return;
//        if(checkmech.enemy.classname == "controlled_servomech")
            if(checkmech.show_hostile > time && random() > 0.25)
                return;
    }

};


//=======================================================================
void() cservo_setup=
{
    local entity tmpent;

    self.origin_z = self.origin_z + 1;    // raise off floor a bit
    droptofloor();
    
    if (!walkmove(0,0))
    {
        dprint ("controllable servomech in wall at: ");
        dprint (vtos(self.origin));
        dprint ("\n");
    }
    
    self.takedamage = DAMAGE_AIM;

    if(!self.targetname)
    {
        cservo_stand();
        self.think = SUB_Null;
        remove(self.movetarget);
    }
    else
    {
        cservo_stand();
        self.nextthink = self.nextthink + random()*0.5;
    }
};

void() func_servomech=
{
    local entity bodent, legsent, pitent;
    local entity cmechatt;

    precache_model ("progs/servobod.mdl");
    precache_model ("progs/servoleg.mdl");
    precache_model ("progs/servodie.mdl");
    precache_model ("progs/h_servo.mdl");
    precache_model ("progs/csm_cpit.mdl");
    precache_model ("progs/frame.mdl");
    precache_model ("progs/s_null.spr");

    precache_sound ("mech/mechstep.wav");
    precache_sound ("mech/pow_up.wav");
    precache_sound ("mech/pow_dn.wav");
    precache_sound ("mech/metal.wav");
    precache_sound ("drake/misc/shortexp.wav");
    precache_sound ("enforcer/enfire.wav");

    // setup the self.as the legs
    setorigin(self, self.origin);
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    setmodel (self, "progs/servoleg.mdl");
    self.classname = "controlled_servomech";
      setsize (self, '-32 -32 -48', '32 32 28');
    self.health = 800;
    self.pos1 = '0 0 0';
    self.angles = self.angles;
    self.ideal_yaw = self.angles_y;
    self.yaw_speed = 10;
    self.view_ofs = '0 0 12';
    self.hittype    = HIT_METAL;

    self.th_die = cservo_die;
    self.th_pain = cservo_pain;
    
// ### Added for Custents all affecting gravity
    if(!self.gravity)
        self.gravity = 1;
    else
          self.gravity = ((self.gravity - 1) / 100);
// ###

    // setup upper body entity
    bodent = spawn();
    bodent.solid = SOLID_NOT;
    bodent.movetype = MOVETYPE_NONE;
    setmodel (bodent, "progs/servobod.mdl");
    setorigin(bodent, self.origin);
    bodent.angles = self.angles;
    bodent.ideal_yaw = self.ideal_yaw;
    bodent.view_ofs = '0 0 12';
    bodent.yaw_speed = 10;
    bodent.rotate_type = 0;
    bodent.skin = self.skin;
    bodent.hittype = HIT_METAL;


    // setup cockpit entity
    pitent = spawn();
    pitent.solid = SOLID_NOT;
    pitent.movetype = MOVETYPE_NONE;
    setmodel(pitent, "progs/csm_cpit.mdl");
    pitent.classname = "mech_cockpit";
    pitent.angles = bodent.angles;
    pitent.spawnflags = RCAM_ALOWIMP;


    // link the entities together
    self.aiment = bodent;
    self.movetarget = pitent;
    bodent.movetarget = pitent;
    bodent.owner = self;
    pitent.spawnmaster = self;

    self.nextthink = self.nextthink + random()*0.4;
    self.think = cservo_setup;
    
    // generate an entity to select controllable servomechs to be checked for attacks
    // like checkclient
    cmechatt = find(world, classname, "cmech_attackselector");
    if(!cmechatt)
    {
        cmechatt = spawn();
        cmechatt.classname = "cmech_attackselector";
        cmechatt.think = cmech_attselect;
        cmechatt.nextthink = time + 1;
    }
};

//=======================================================================

void() trigger_servomech_use=
{
    local entity contmech;

    contmech = find(world, targetname, self.target);
    while(contmech)
    {
        if(!contmech.movetarget.owner)
        {
            contmech.walkframe = self.walkframe;
            contmech.invincible_finished = self.invincible_finished;
            self = contmech;
            cservo_control_on(activator);
            return;
        }
        contmech = find(contmech, targetname, self.target);
    }
    sprint(activator, "No servomech responded.\n");
};

void() trigger_servomech=
{
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;

    if(self.spawnflags & 1)
        self.walkframe = 0;
    else
        self.walkframe = 1;
    
    if(self.spawnflags & 2)
        self.invincible_finished = 1;
    else
        self.invincible_finished = 0;

    self.use = trigger_servomech_use;
};

//=======================================================================

/*
FindMechTarget

Used to make monsters check for attackable, player controlled servomechs
*/
float() FindMechTarget=
{
    local entity client;
    local float r;
    
    client = checkmech;
    if (!client)
        return FALSE;

    if (client == self.enemy)
        return FALSE;

    r = range (client);
    if (r == RANGE_FAR)
        return FALSE;

    if (!visible (client))
        return FALSE;

    if (client.enemy.classname == "controlled_servomech")
    {
        if (r == RANGE_MID)
        {
            if (client.show_hostile < time && !infront (client))
                return FALSE;
        }
        else
        {
            if(client.show_hostile < time)
                return FALSE;
        }
    }
    else
    {
            if (client.show_hostile < time && !infront (client))
                return FALSE;
    }

//
// got one
//
    self.enemy = client;

    if (self.enemy.classname != "controlled_servomech")
    {
        self.enemy = self.enemy.enemy;
        if (self.enemy.classname != "controlled_servomech")
        {
            self.enemy = world;
            return FALSE;
        }
    }

    FoundTarget ();

    return TRUE;
};
