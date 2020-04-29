// ending.qc
//    code to handle big ending of Xpack.
// ------------------------------------------------
$frame axrun1 axrun2 axrun3 axrun4 axrun5 axrun6
$frame rockrun1 rockrun2 rockrun3 rockrun4 rockrun5 rockrun6
$frame stand1 stand2 stand3 stand4 stand5
$frame axstnd1 axstnd2 axstnd3 axstnd4 axstnd5 axstnd6
$frame axstnd7 axstnd8 axstnd9 axstnd10 axstnd11 axstnd12
$frame axpain1 axpain2 axpain3 axpain4 axpain5 axpain6
$frame pain1 pain2 pain3 pain4 pain5 pain6
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
$frame nailatt1 nailatt2
$frame light1 light2
$frame rockatt1 rockatt2 rockatt3 rockatt4 rockatt5 rockatt6
$frame shotatt1 shotatt2 shotatt3 shotatt4 shotatt5 shotatt6
$frame axatt1 axatt2 axatt3 axatt4 axatt5 axatt6
$frame axattb1 axattb2 axattb3 axattb4 axattb5 axattb6
$frame axattc1 axattc2 axattc3 axattc4 axattc5 axattc6
$frame axattd1 axattd2 axattd3 axattd4 axattd5 axattd6

entity    theActor;
entity    theMachine;

float    actorStage;

float    STAGE_START        = 0;
float    STAGE_TO_POINT1 = 1;
float    STAGE_AT_POINT1 = 2;
float    STAGE_TO_POINT2 = 3;
float     STAGE_AT_POINT2 = 4;
float    STAGE_FIRING    = 5;

void(entity thePlayer) spawn_actor;
void() actor_control;
void(entity playerEnt) move_camera;
void() time_crash;
void() ending_remove_stuff;

void() ending_for_coop =
{
    intermission_exittime = time + 10000000;    // never allow exit
    intermission_running = 1;

    self.model = "";
    self.yaw_speed = 20;
    self.view_ofs = '0 0 0';
    self.takedamage = DAMAGE_NO;
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    self.modelindex = 0;
    setorigin (self, self.origin + '0 0 48');

    WriteByte (MSG_ALL, SVC_FINALE);
    WriteString (MSG_ALL, "You have destroyed Quake's\nTemporal Teleporter. His assault\non Time has been defeated.");

    ending_remove_stuff();

    theMachine.think = time_crash;
    theMachine.nextthink = time + 0.1;
};

// ------------------------------------------------
// Camera and control functions
// ------------------------------------------------

void() xpackEnding =
{
    local entity cameraview;

    if (coop)
    {
        ending_for_coop();
        return;
    }

    intermission_exittime = time + 10000000;    // never allow exit
    intermission_running = 1;

    WriteByte (MSG_ALL, SVC_CUTSCENE);
    WriteString (MSG_ALL, "");

    cameraview = find ( world, targetname, "cameraview");
    if (cameraview == world)
    {
        ending_for_coop();
        return;
    }

    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    spawn_actor(self);
    move_camera(self);
};

void() track_camera =
{
    local vector    cameraAngle;

    cameraAngle = theActor.origin - self.origin;
    cameraAngle_z = 0 - cameraAngle_z;
    self.angles = vectoangles ( cameraAngle );
    self.v_angle = self.angles;

    self.think = track_camera;
    self.nextthink = time + 0.1;
};

void(entity playerEnt) move_camera =
{
    local entity    cameraPoint;
    local vector    cameraAngle;

    cameraPoint = find ( world, targetname, "cameraview");
    if ( cameraPoint == world )
        objerror ("Could not find camerapoint!");

    self.model = "";
    self.yaw_speed = 20;
    self.view_ofs = '0 0 0';
    self.takedamage = DAMAGE_NO;
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    self.modelindex = 0;
    setorigin (self, cameraPoint.origin);

    cameraAngle = theActor.origin - self.origin;
    self.angles = vectoangles ( cameraAngle );
    self.v_angle = self.angles;

    self.think = track_camera;
    self.nextthink = time + 0.05;
};

void() ending_remove_stuff =
{
    local entity     curEnt;
    local entity    removeEnt;

    curEnt = find ( world, classname, "ltrail_start");
    while(curEnt)
    {
        remove(curEnt);
        curEnt = find ( world, classname, "ltrail_start");
    }

    curEnt = find ( world, classname, "item_time_core");
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION2);
    WriteCoord (MSG_BROADCAST, curEnt.origin_x);
    WriteCoord (MSG_BROADCAST, curEnt.origin_y);
    WriteCoord (MSG_BROADCAST, curEnt.origin_z);
    WriteByte (MSG_BROADCAST, 230);
    WriteByte (MSG_BROADCAST, 5);

    curEnt.think = SUB_Remove;
    curEnt.nextthink = time + 0.1;
};

// ------------------------------------------------
// actor code
// ------------------------------------------------
void() actor_run =
{
    local vector     dist;
    local float        ang;
    local entity    pointOne;

    if ( pointcontents (self.origin) == CONTENT_LAVA )
    {
        pointOne = find ( world, targetname, "point1" );
        if (pointOne != world)
            setorigin ( self, pointOne.origin );
    }

    if ( self.goalentity.targetname == "endpoint1")
    {
        actorStage = STAGE_AT_POINT1;
        self.think = actor_control;
        self.nextthink = time + 0.1;
        return;
    }
    if ( self.goalentity.targetname == "endpoint2")
    {
        actorStage = STAGE_AT_POINT2;
        self.think = actor_control;
        self.nextthink = time + 0.1;
        return;
    }

    self.frame = self.frame + 1;
    if (self.frame > 11)
        self.frame = 6;

    dist = self.goalentity.origin - self.origin;
//    self.ideal_yaw = vectoyaw ( dist );
//    ChangeYaw();

    movetogoal ( 15 );
    self.think = actor_run;
    self.nextthink = time + 0.1;
};

void() actor_fire1 =[$rockatt1, actor_fire2 ]
    {
        self.goalentity = theMachine;
        theMachine.th_pain = time_crash;
        theMachine.th_die = time_crash;
        theMachine.health = 1;

        self.angles = vectoangles ( self.goalentity.origin - self.origin );
        self.v_angle = self.angles;
        self.v_angle_x = 0 - self.angles_x;
        self.effects = EF_MUZZLEFLASH;
        W_FireRocket();

        // start the end text
        WriteByte (MSG_ALL, SVC_FINALE);

        WriteString (MSG_ALL, "\nFinally, Quake's Temporal Teleporter\nyields to your assault. A high\npitched scream emits from the\ndevastated device as stressed steel\nblasts outward to rock the cavern.\nThe machine is devoured by molten lava.\n\nThe ground shudders as reality shifts\nback to its predestined path.\n\nYou run to enter the charged time pod,\nscrambling in as the chamber closes.\nYour consciousness fades as you realize\nyou have halted Quake's plans for...\n\nThe Dissolution of Eternity.");
    };
void() actor_fire2 =[$rockatt2, actor_fire3 ]
    {self.v_angle_x = 0; self.angles_x = 0; self.nextthink=time+0.15;};
void() actor_fire3 =[$rockatt3, actor_fire4 ] {self.nextthink=time+0.15;};
void() actor_fire4 =[$rockatt4, actor_fire5 ] {self.nextthink=time+0.15;};
void() actor_fire5 =[$rockatt5, actor_fire6 ]
    {
        self.nextthink=time+0.15;
        ending_remove_stuff();
        if (theMachine.health > 0)
        {
            theMachine.think = time_crash;
            theMachine.nextthink = time + 0.1;
        }
    };
void() actor_fire6 =[$rockatt6, actor_fire7 ]
    { self.effects=0;self.nextthink=time+0.15;};
void() actor_fire7 =[$stand1,  actor_fire8 ] {self.nextthink=time+0.15;};
void() actor_fire8 =[$stand2,  actor_fire9 ] {self.nextthink=time+0.15;};
void() actor_fire9 =[$stand3,  actor_fire10 ] {self.nextthink=time+0.15;};
void() actor_fire10 =[$stand4, actor_fire11 ] {self.nextthink=time+0.15;};
void() actor_fire11 =[$stand5, actor_fire12 ] {self.nextthink=time+0.15;};
void() actor_fire12 =[$stand1, actor_fire13 ] {self.nextthink=time+0.15;};
void() actor_fire13 =[$stand2, actor_fire14 ] {self.nextthink=time+0.15;};
void() actor_fire14 =[$stand3, actor_fire15 ] {self.nextthink=time+0.15;};
void() actor_fire15 =[$stand4, actor_fire16 ] {self.nextthink=time+0.15;};
void() actor_fire16 =[$stand5, actor_fire17 ] {self.nextthink=time+0.15;};
void() actor_fire17 =[$stand1, actor_fire18 ] {self.nextthink=time+0.15;};
void() actor_fire18 =[$stand2, actor_fire19 ] {self.nextthink=time+0.15;};
void() actor_fire19 =[$stand3, actor_fire20 ] {self.nextthink=time+0.15;};
void() actor_fire20 =[$stand4, actor_fire21 ] {self.nextthink=time+0.15;};
void() actor_fire21 =[$stand5, actor_control ]
    {
        self.nextthink=time+0.15;
        actorStage = STAGE_TO_POINT2;
    };

void() actor_teleport =
{
    spawn_tfog(self.origin);
    self.model = string_null;
    self.think = SUB_Null;
    self.nextthink = time + 999999;
};

void() actor_control =
{
    local entity    timepod;

    if (actorStage == STAGE_START)
    {
        self.target = "point1";
        self.movetarget = find (world, targetname, self.target);
        self.goalentity = self.movetarget;
        if ( self.goalentity == world )
            objerror ("End Sequence point1 placing screwed up!");
        self.frame = 6;
        self.think = actor_run;
        self.nextthink = time + 0.1;
        actorStage = STAGE_TO_POINT1;
    }
    else if (actorStage == STAGE_AT_POINT1)
    {
        self.target = "machine";
        self.movetarget = find (world, targetname, self.target);
        self.goalentity = self.movetarget;
        if ( self.goalentity == world )
            objerror ("End Sequence machine placing screwed up!");
        actorStage = STAGE_FIRING;
        self.think = actor_fire1;
        self.nextthink = time + 0.1;
    }
    else if (actorStage == STAGE_AT_POINT2)
    {
        self.frame = $stand1;
        self.think = actor_teleport;
        self.nextthink = time + 2;
    }
    else if (actorStage == STAGE_TO_POINT2)
    {
        self.target = "timepod";
        SUB_UseTargets();

        self.target = "point2";
        self.movetarget = find (world, targetname, self.target);
        self.goalentity = self.movetarget;
        if ( self.goalentity == world )
            objerror ("End Sequence point2 placing screwed up!");
        self.frame = 6;
        self.think = actor_run;
        self.nextthink = time + 0.1;
    }
};

// ==============
// player actor spawn function
// ==============
void(entity thePlayer) spawn_actor =
{
    local entity    pointOne;

    theActor = spawn();
    theActor.owner = self;
    theActor.classname = "actor";
    theActor.health = 100;
    theActor.solid = SOLID_SLIDEBOX;
    theActor.movetype = MOVETYPE_STEP;
    theActor.frame = self.frame;
    setmodel ( theActor, "progs/player.mdl");
    setorigin ( theActor, self.origin);
    setsize (theActor, VEC_HULL_MIN, VEC_HULL_MAX);
    theActor.view_ofs = '0 0 22';
    theActor.angles = self.angles;
    theActor.ideal_yaw = theActor.angles * '0 1 0';
    if (!theActor.yaw_speed)
        theActor.yaw_speed = 20;
    theActor.view_ofs = '0 0 25';
    theActor.flags = theActor.flags | FL_MONSTER;
    actorStage == STAGE_START;

    if ( pointcontents (theActor.origin) == CONTENT_LAVA )
    {
        pointOne = find ( world, targetname, "point1" );
        if (pointOne != world)
            setorigin ( theActor, pointOne.origin );
    }

    theActor.think = actor_control;
    theActor.nextthink = time + 0.1;
};

