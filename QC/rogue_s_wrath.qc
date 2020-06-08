// overlord.qc

$skin s_wrtrst
$frame s_wrtfrm

// walk
$frame s_wtwk01 s_wtwk02 s_wtwk03 s_wtwk04 s_wtwk05
$frame s_wtwk06 s_wtwk07 s_wtwk08 s_wtwk09 s_wtwk10
$frame s_wtwk11 s_wtwk12 s_wtwk13 s_wtwk14 s_wtwk15

// hand to hand attacks
$frame s_wtaa01 s_wtaa02 s_wtaa03 s_wtaa04 s_wtaa05
$frame s_wtaa06 s_wtaa07 s_wtaa08 s_wtaa09 s_wtaa10

$frame s_wtab01 s_wtab02 s_wtab03 s_wtab04 s_wtab05
$frame s_wtab06 s_wtab07 s_wtab08 s_wtab09 s_wtab10
$frame s_wtab11 s_wtab12 s_wtab13 s_wtab14

$frame s_wtac01 s_wtac02 s_wtac03 s_wtac04 s_wtac05
$frame s_wtac06 s_wtac07 s_wtac08 s_wtac09 s_wtac10
$frame s_wtac11 s_wtac12 s_wtac13 s_wtac14

// ranged attack
$frame s_wtba01 s_wtba02 s_wtba03 s_wtba04 s_wtba05
$frame s_wtba06 s_wtba07 s_wtba08 s_wtba09 s_wtba10 s_wtba11 s_wtba12

// pain
$frame s_wtpa01 s_wtpa02 s_wtpa03 s_wtpa04 s_wtpa05
$frame s_wtpa06 s_wtpa07 s_wtpa08 s_wtpa09 s_wtpa10
$frame s_wtpa11 s_wtpa12 s_wtpa13 s_wtpa14

$frame s_wtpb01 s_wtpb02 s_wtpb03 s_wtpb04 s_wtpb05
$frame s_wtpb06 s_wtpb07 s_wtpb08 s_wtpb09 s_wtpb10
$frame s_wtpb11

// death
$frame s_wtdt01 s_wtdt02 s_wtdt03 s_wtdt04 s_wtdt05
$frame s_wtdt06 s_wtdt07 s_wtdt08 s_wtdt09 s_wtdt10
$frame s_wtdt11 s_wtdt12 s_wtdt13 s_wtdt14 s_wtdt15
$frame s_wtdt16 s_wtdt17

float(entity thePoint) isSpawnPointEmpty =
{
    entity    neighbors;
    entity    start;

    neighbors = findradius (thePoint.origin, 64);
    if(neighbors == world)
        return 1;

    start = neighbors;
    while (neighbors)
    {
        if(neighbors != thePoint)
        {
            if(neighbors.flags & FL_MONSTER)
            {
                return 0;
            }
            else if(neighbors.classname == "player")
            {
                return 0;
            }
            else if(neighbors.think)
            {
                return 0;
            }
        }

        neighbors = neighbors.chain;
        if(neighbors == start)
            return 1;
    }
    return 1;
};

entity() overlord_find_dest3 =
{
    entity    furthestDest;
    entity    bestDest;
    entity    curDest;
    entity    thePlayer;
    float     theDist;
    float     curDist;
    float     dot;
    vector    curVector;

    furthestDest = world;
    bestDest = world;
    theDist=0;
    thePlayer=find(world, classname, "player");
    curDest=world;

    makevectors(thePlayer.angles);

    while (1)
    {
        curDest = find(curDest, classname, "info_overlord_destination");
        if(curDest == world)
        {
            if(bestDest != world)
                return bestDest;

            return furthestDest;
        }

        if(isSpawnPointEmpty(curDest))
        {
            curVector = curDest.origin - thePlayer.origin;
            curDist = vlen(curVector);
            curVector = normalize(curVector);
            dot = curVector * v_forward;
            if(dot > 0.6)
            {
                if(curDist > 150)
                {
                    bestDest = curDest;
                }
            }

            if(curDist > theDist)
            {
                    furthestDest = curDest;
                    theDist = curDist;
            }
        }
    }

    // just in case, return the world.
    return world;
};

// =======================================================

entity() overlord_find_dest =
{
    entity    curDest, lastDest;
    float     loopCount;

    curDest = find(world, classname, "info_overlord_destination");
    if(curDest != world)
    {
        lastDest = curDest;
        loopCount = random()*5;
        while (loopCount > 1)
        {
            curDest = nextent (curDest);
            curDest = find(curDest, classname, "info_overlord_destination");
            if(curDest != world)
            {
                if(isSpawnPointEmpty(curDest))
                {
                    loopCount = loopCount - 1;
                    lastDest = curDest;
                }
                else
                    curDest = world;
            }
            else
            {
                return lastDest;
            }
        }
        return curDest;
    }
    return curDest;
};


void() overlord_teleport =
{
    float     chance;
    entity    theDest;
    vector    org;

    if(!(self.spawnflags & 2))
        return;

    chance = random();
    if(chance > 0.75)
        return;

//    theDest = overlord_find_dest();
    theDest = overlord_find_dest3();
    if(theDest == world)
        return;

// put a tfog where the player was
    spawn_tfog (self.origin);

// spawn a tfog flash in front of the destination
    makevectors(self.angles);
    org = theDest.origin + 32 * v_forward;

    spawn_tfog (org);
    spawn_tdeath(theDest.origin, self);

    setorigin(self, theDest.origin);
    self.flags = self.flags - self.flags & FL_ONGROUND;
};
// =======================================================

void()  overlord_stand1 =[    $s_wtwk01,    overlord_stand1 ] { ai_stand();};

void()    overlord_walk01 =[    $s_wtwk01,    overlord_walk02 ] { ai_walk(8); };
void()    overlord_walk02 =[    $s_wtwk02,    overlord_walk03 ] { ai_walk(8); };
void()    overlord_walk03 =[    $s_wtwk03,    overlord_walk04 ] { ai_walk(8); };
void()    overlord_walk04 =[    $s_wtwk04,    overlord_walk05 ] { ai_walk(8); };
void()    overlord_walk05 =[    $s_wtwk05,    overlord_walk06 ] { ai_walk(8); };
void()    overlord_walk06 =[    $s_wtwk06,    overlord_walk07 ] { ai_walk(8); };
void()    overlord_walk07 =[    $s_wtwk07,    overlord_walk08 ] { ai_walk(8); };
void()    overlord_walk08 =[    $s_wtwk08,    overlord_walk09 ] { ai_walk(8); };
void()    overlord_walk09 =[    $s_wtwk09,    overlord_walk10 ] { ai_walk(8); };
void()    overlord_walk10 =[    $s_wtwk10,    overlord_walk11 ] { ai_walk(8); };
void()    overlord_walk11 =[    $s_wtwk11,    overlord_walk12 ] { ai_walk(8); };
void()    overlord_walk12 =[    $s_wtwk12,    overlord_walk13 ] { ai_walk(8); };
void()    overlord_walk13 =[    $s_wtwk13,    overlord_walk14 ] { ai_walk(8); };
void()    overlord_walk14 =[    $s_wtwk14,    overlord_walk15 ] { ai_walk(8); };
void()    overlord_walk15 =[    $s_wtwk15,    overlord_walk01 ] { ai_walk(8); };

void()    overlord_run01 =[ $s_wtwk01,    overlord_run02 ] { ai_run(12); };
void()    overlord_run02 =[ $s_wtwk02,    overlord_run03 ] { ai_run(12); };
void()    overlord_run03 =[ $s_wtwk03,    overlord_run04 ] { ai_run(12); };
void()    overlord_run04 =[ $s_wtwk04,    overlord_run05 ] { ai_run(12); };
void()    overlord_run05 =[ $s_wtwk05,    overlord_run06 ] { ai_run(12); };
void()    overlord_run06 =[ $s_wtwk06,    overlord_run07 ] { ai_run(12); };
void()    overlord_run07 =[ $s_wtwk07,    overlord_run08 ] { ai_run(12); };
void()    overlord_run08 =[ $s_wtwk08,    overlord_run09 ] { ai_run(12); };
void()    overlord_run09 =[ $s_wtwk09,    overlord_run10 ] { ai_run(12); };
void()    overlord_run10 =[ $s_wtwk10,    overlord_run11 ] { ai_run(12); };
void()    overlord_run11 =[ $s_wtwk11,    overlord_run12 ] { ai_run(12); };
void()    overlord_run12 =[ $s_wtwk12,    overlord_run13 ] { ai_run(12); };
void()    overlord_run13 =[ $s_wtwk13,    overlord_run14 ] { ai_run(12); };
void()    overlord_run14 =[ $s_wtwk14,    overlord_run15 ] { ai_run(12); };
void()    overlord_run15 =[ $s_wtwk15,    overlord_run01 ] { ai_run(12); };

// =================================================
// overlord hand-to-hand attack
// =================================================
void() overlord_smash;

void()    overlord_at_a01 =[    $s_wtaa01, overlord_at_a02 ] { ai_charge(12);};
void()    overlord_at_a02 =[    $s_wtaa02, overlord_at_a03 ] { ai_charge(12);};
void()    overlord_at_a03 =[    $s_wtaa03, overlord_at_a04 ] { ai_charge(12);};
void()    overlord_at_a04 =[    $s_wtaa04, overlord_at_a05 ] { ai_charge(12);};
void()    overlord_at_a05 =[    $s_wtaa05, overlord_at_a06 ] { ai_charge(12);};
void()    overlord_at_a06 =[    $s_wtaa06, overlord_at_a07 ] { ai_charge(12);};
void()  overlord_at_a07 =[  $s_wtaa07, overlord_at_a08 ] {overlord_smash();};
void()    overlord_at_a08 =[    $s_wtaa08, overlord_at_a09 ] { ai_charge(12);};
void()    overlord_at_a09 =[    $s_wtaa09, overlord_at_a10 ] { ai_charge(12);};
void()    overlord_at_a10 =[    $s_wtaa10, overlord_run01 ] { ai_charge(12);};

void()    overlord_at_b01 =[    $s_wtab01,    overlord_at_b02 ] { ai_charge(12);};
void()    overlord_at_b02 =[    $s_wtab02,    overlord_at_b03 ] { ai_charge(12);};
void()    overlord_at_b03 =[    $s_wtab03,    overlord_at_b04 ] { ai_charge(12);};
void()    overlord_at_b04 =[    $s_wtab04,    overlord_at_b05 ] { ai_charge(12);};
void()    overlord_at_b05 =[    $s_wtab05,    overlord_at_b06 ] { ai_charge(12);};
void()  overlord_at_b06 =[  $s_wtab06,  overlord_at_b07 ] { ai_charge(12);};
void()    overlord_at_b07 =[    $s_wtab07,    overlord_at_b08 ] { ai_charge(12);};
void()  overlord_at_b08 =[  $s_wtab08,  overlord_at_b09 ] {overlord_smash();};
void()    overlord_at_b09 =[    $s_wtab09,    overlord_at_b10 ] { ai_charge(12);};
void()    overlord_at_b10 =[    $s_wtab10,    overlord_at_b11 ] { ai_charge(12);};
void()    overlord_at_b11 =[    $s_wtab11,    overlord_at_b12 ] { ai_charge(12);};
void()    overlord_at_b12 =[    $s_wtab12,    overlord_at_b13 ] { ai_charge(12);};
void()    overlord_at_b13 =[    $s_wtab13,    overlord_at_b14 ] { ai_charge(12);};
void()    overlord_at_b14 =[    $s_wtab14,    overlord_run01 ] { ai_charge(12);};

void()    overlord_at_c01 =[    $s_wtac01,    overlord_at_c02 ] { ai_charge(12);};
void()    overlord_at_c02 =[    $s_wtac02,    overlord_at_c03 ] { ai_charge(12);};
void()    overlord_at_c03 =[    $s_wtac03,    overlord_at_c04 ] { ai_charge(12);};
void()    overlord_at_c04 =[    $s_wtac04,    overlord_at_c05 ] { ai_charge(12);};
void()    overlord_at_c05 =[    $s_wtac05,    overlord_at_c06 ] { ai_charge(12);};
void()  overlord_at_c06 =[  $s_wtac06,  overlord_at_c07 ] {overlord_smash();};
void()    overlord_at_c07 =[    $s_wtac07,    overlord_at_c08 ] { ai_charge(12);};
void()    overlord_at_c08 =[    $s_wtac08,    overlord_at_c09 ] { ai_charge(12);};
void()    overlord_at_c09 =[    $s_wtac09,    overlord_at_c10 ] { ai_charge(12);};
void()    overlord_at_c10 =[    $s_wtac10,    overlord_at_c11 ] { ai_charge(12);};
void()  overlord_at_c11 =[  $s_wtac11,  overlord_at_c12 ] {overlord_smash();};
void()    overlord_at_c12 =[    $s_wtac12,    overlord_at_c13 ] { ai_charge(12);};
void()    overlord_at_c13 =[    $s_wtac13,    overlord_at_c14 ] { ai_charge(12);};
void()    overlord_at_c14 =[    $s_wtac14,    overlord_run01 ] { ai_charge(12);};

void() overlord_smash =
{
    vector    delta;
    float        smashDmg;

    if(!self.enemy)
        return;
    if(!CanDamage(self.enemy, self))
        return;

    ai_charge(10);

    delta = self.enemy.origin - self.origin;

    if(vlen(delta) > 100)
        return;

    smashDmg = 20 + random () * 10;

    sound(self, CHAN_WEAPON, "s_wrath/smash.wav", 1, ATTN_NORM);
    T_Damage(self.enemy, self, self, smashDmg);
    delta = normalize(self.enemy.origin - self.origin + self.enemy.view_ofs);
    SpawnBlood (self.enemy.origin - delta * 30, delta * -100, smashDmg);
};

void() overlord_melee =
{
    float r;

    r = random();

    if(r < 0.33)
    {
        overlord_at_a01();
    }
    else if(r < 0.66)
    {
        overlord_at_b01();
    }
    else
    {
        overlord_at_c01();
    }
};

// ==================================================
// missile attacks
// ==================================================
void(float AttackNumber) WrathMissile;

void() overlord_msl_a01 =[ $s_wtba01, overlord_msl_a02 ] { ai_charge(12);};
void() overlord_msl_a02 =[ $s_wtba02, overlord_msl_a03 ] { ai_charge(12);};
void() overlord_msl_a03 =[ $s_wtba03, overlord_msl_a04 ] { ai_charge(12);};
void() overlord_msl_a04 =[ $s_wtba04, overlord_msl_a05 ] { ai_charge(12);};
void() overlord_msl_a05 =[ $s_wtba05, overlord_msl_a06 ] { ai_charge(12);};
void() overlord_msl_a06 =[ $s_wtba06, overlord_msl_a07 ] {WrathMissile(4);};
void() overlord_msl_a07 =[ $s_wtba07, overlord_msl_a08 ] { ai_charge(12);};
void() overlord_msl_a08 =[ $s_wtba08, overlord_msl_a09 ] { ai_charge(12);};
void() overlord_msl_a09 =[ $s_wtba09, overlord_msl_a10 ] { ai_charge(12);};
void() overlord_msl_a10 =[ $s_wtba10, overlord_msl_a11 ] { ai_charge(12);};
void() overlord_msl_a11 =[ $s_wtba11, overlord_msl_a12 ] { ai_charge(12);};
void() overlord_msl_a12 =[ $s_wtba12, overlord_run01 ] {overlord_teleport();};

void() overlord_missile =
{
    float r;
    r = random();

    overlord_msl_a01();
};

// ==================================================
// pain
// ==================================================
void()    overlord_pn_a01    =[    $s_wtpa01,    overlord_pn_a02    ] {};
void()    overlord_pn_a02    =[    $s_wtpa02,    overlord_pn_a03    ] {};
void()  overlord_pn_a03 =[  $s_wtpa03,  overlord_pn_a04 ] {overlord_teleport();};
void()    overlord_pn_a04    =[    $s_wtpa04,    overlord_pn_a05    ] {};
void()    overlord_pn_a05    =[    $s_wtpa05,    overlord_pn_a06    ] {};
void()    overlord_pn_a06    =[    $s_wtpa06,    overlord_pn_a07    ] {};
void()    overlord_pn_a07    =[    $s_wtpa07,    overlord_run01  ] {};

void()    overlord_pn_b01    =[    $s_wtpb01,    overlord_pn_b02    ] {};
void()    overlord_pn_b02    =[    $s_wtpb02,    overlord_pn_b03    ] {};
void()  overlord_pn_b03 =[  $s_wtpb03,  overlord_pn_b04 ] {overlord_teleport();};
void()    overlord_pn_b04    =[    $s_wtpb04,    overlord_pn_b05    ] {};
void()    overlord_pn_b05    =[    $s_wtpb05,    overlord_pn_b06    ] {};
void()    overlord_pn_b06    =[    $s_wtpb06,    overlord_pn_b07    ] {};
void()    overlord_pn_b07    =[    $s_wtpb07,    overlord_run01  ] {};


void(entity attacker, float damage) overlord_pain =
{
    float r;

    if(self.pain_finished > time)
        return;

    r = random();

    if(r > 0.2)
        return;

    if(r < 0.15)
        overlord_pn_a01();
    else
        overlord_pn_b01();

    self.pain_finished = time + 2;
    sound(self, CHAN_VOICE, "wrath/wpain.wav", 1, ATTN_NORM);
};

void(string gibName) ol_toss =
{
    vector flingVelocity;
    entity myGib;

    makevectors(self.angles);
    flingVelocity = v_forward*250 + v_up*300;
    flingVelocity = flingVelocity + v_up * (random() * 100 - 50);
    flingVelocity = flingVelocity + v_right * (random() * 200 - 100);

    myGib = StartGib (gibName);
    myGib.velocity = flingVelocity;
};

// note - death frame 9 is intentionally done twice.
void()    overlord_die01 =[ $s_wtdt01, overlord_die02 ] {self.nextthink = time +0.05;};
void()    overlord_die02 =[ $s_wtdt02, overlord_die03 ]
    { self.flags = self.flags | FL_FLY; self.nextthink = time +0.05;};
void()    overlord_die03 =[ $s_wtdt03, overlord_die04 ] {self.nextthink = time +0.05;};
void()    overlord_die04 =[ $s_wtdt04, overlord_die05 ] {self.nextthink = time +0.05;};
void()    overlord_die05 =[ $s_wtdt05, overlord_die06 ] {self.nextthink = time +0.05;};
void()    overlord_die06 =[ $s_wtdt06, overlord_die07 ] {self.nextthink = time +0.05;};
void()    overlord_die07 =[ $s_wtdt07, overlord_die08 ] {self.nextthink = time +0.05;};
void()    overlord_die08 =[ $s_wtdt08, overlord_die09 ] {self.nextthink = time +0.05;};
void()    overlord_die09 =[ $s_wtdt09, overlord_die10 ] {self.nextthink = time +0.05;};
void()    overlord_die10 =[ $s_wtdt09, overlord_die11 ] {self.nextthink = time +0.05;};
void()    overlord_die11 =[ $s_wtdt10, overlord_die12 ] {self.nextthink = time +0.05;};
void()    overlord_die12 =[ $s_wtdt11, overlord_die13 ] {self.nextthink = time +0.05;};
void()    overlord_die13 =[ $s_wtdt12, overlord_die14 ] {self.nextthink = time +0.05;};
void()    overlord_die14 =[ $s_wtdt13, overlord_die15 ] {self.nextthink = time +0.05;};
void()    overlord_die15 =[ $s_wtdt14, overlord_die16 ] {self.nextthink = time +0.05;};
void()    overlord_die16 =[ $s_wtdt15, overlord_die17 ] {self.nextthink = time +0.05;};
void()    overlord_die17 =[ $s_wtdt16, overlord_die18 ]
{
    self.model = "";

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION2);
    WriteVec3(MSG_BROADCAST, self.origin);
    WriteByte(MSG_BROADCAST, 0);
    WriteByte(MSG_BROADCAST, 4);

    ol_toss("progs/s_wrtgb2.mdl");
    ol_toss("progs/s_wrtgb3.mdl");
    ol_toss("progs/wrthgib1.mdl");
    ol_toss("progs/wrthgib2.mdl");
    ol_toss("progs/wrthgib3.mdl");
    self.nextthink = time + 0.1;
};
void()    overlord_die18 =[ $s_wtdt17, overlord_die19 ]
{
    float counter;

    counter = 2;

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION2);
    WriteVec3(MSG_BROADCAST, self.origin);
    WriteByte(MSG_BROADCAST, 0);
    WriteByte(MSG_BROADCAST, 4);

    while(counter>0)
    {
        ol_toss("progs/gib1.mdl");
        ol_toss("progs/gib2.mdl");
        ol_toss("progs/gib3.mdl");
        counter = counter - 1;
    }
    self.nextthink = time + 0.1;
};
void() overlord_die19 = [ $s_wtdt17, overlord_die19 ]
{
    float counter;

    counter = 2;

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION2);
    WriteVec3(MSG_BROADCAST, self.origin);
    WriteByte(MSG_BROADCAST, 0);
    WriteByte(MSG_BROADCAST, 4);

    while(counter>0)
    {
        ol_toss("progs/gib1.mdl");
        ol_toss("progs/gib2.mdl");
        ol_toss("progs/gib3.mdl");
        counter = counter - 1;
    }

    remove(self);
};

/*QUAKED monster_super_wrath (1 0 0) (-16 -16 -24) (16 16 32) Ambush BossMode
The super wrath (Overlord).

BossMode enables the superwrath's powershield and the random teleporting.
*/
void() monster_super_wrath =
{
    if(deathmatch)
    {
        remove(self);
        return;
    }
    precache_model("progs/s_wrath.mdl");
//
    precache_model("progs/s_wrtgb1.mdl");

    precache_model("progs/s_wrtgb2.mdl");

    precache_model("progs/s_wrtgb3.mdl");
    precache_model("progs/wrthgib1.mdl");
    precache_model("progs/wrthgib2.mdl");
    precache_model("progs/wrthgib3.mdl");

    precache_model("progs/w_ball.mdl");
    precache_sound("wrath/wsee.wav");
    precache_sound("wrath/watt.wav");
    precache_sound("wrath/wpain.wav");
    precache_sound("wrath/wdthc.wav");
    precache_sound("s_wrath/smash.wav");

    // HONEY
    monster_add_to_counter();

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;

    setmodel(self, "progs/s_wrath.mdl");

    setsize(self, VEC_HULL_MIN, VEC_HULL_MAX);

    self.health = 1000;
    self.pain_finished = 0;

    self.th_stand = overlord_stand1;
    self.th_walk = overlord_walk01;
    self.th_run = overlord_run01;
    self.th_missile = overlord_missile;
    self.th_melee = overlord_melee;
    self.th_pain = overlord_pain;
    self.th_die = overlord_die02;

    flymonster_start();
};

/*QUAKED info_overlord_destination (.5 .5 .5) (-8 -8 -8) (8 8 32)
This is the destination marker for the overlord.

It does not require any other fields filled in.
*/
void() info_overlord_destination =
{
// this does nothing, just serves as a target spot
    self.mangle = self.angles;
    self.angles = '0 0 0';
    self.model = "";
    self.origin = self.origin + '0 0 27';
};

