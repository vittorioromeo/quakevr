// dragon.qc

$base dragrest
$skin dragskin

$frame dragon6

$frame drgfly01 drgfly02 drgfly03 drgfly04
$frame drgfly05 drgfly06 drgfly07 drgfly08
$frame drgfly09 drgfly10 drgfly11 drgfly12 drgfly13

$frame drgtrn01 drgtrn02 drgtrn03 drgtrn04 drgtrn05 drgtrn06

$frame drgslh01 drgslh02 drgslh03 drgslh04 drgslh05 drgslh06
$frame drgslh07 drgslh08 drgslh09 drgslh10 drgslh11 drgslh12 drgslh13

$frame drgfir01 drgfir02 drgfir03 drgfir04 drgfir05
$frame drgfir06 drgfir07 drgfir08 drgfir09 drgfir10

$frame drgfix1a drgfix1b drgfix1c
$frame drgfix2a drgfix2b drgfix2c
$frame drgfix3a drgfix3b drgfix3c
$frame drgfix4a drgfix4b drgfix4c
$frame drgfix5a drgfix5b drgfix5c
$frame drgfix6a drgfix6b drgfix6c drgfix6d

$frame drgpan1a drgpan1b drgpan1c
$frame drgpan2a drgpan2b drgpan2c
$frame drgpan3a drgpan3b drgpan3c
$frame drgpan4a drgpan4b drgpan4c
$frame drgpan5a drgpan5b drgpan5c
$frame drgpan6a drgpan6b drgpan6c

$frame drgdth01 drgdth02 drgdth03 drgdth04 drgdth05 drgdth06
$frame drgdth07 drgdth08 drgdth09 drgdth10 drgdth11 drgdth12
$frame drgdth13 drgdth14 drgdth15 drgdth16 drgdth17 drgdth18
$frame drgdth19 drgdth20 drgdth21

void() dragon_fireball;
void() dragon_melee1;

// ===================================
// global variables
// ===================================

float DDIE_FALLING =     1;
float DDIE_STOPPED =    2;
float DDIE_EXPLODED =     3;

float DT_ROOM_A =     1;
float DT_ROOM_B =     2;
float DT_ROOM_C =     4;

float FIRE_PLASMA =        1;
float FIRE_FIREBALL =    2;

// ===================================
// dragon_stop_attack
// ===================================
void() dragon_stop_attack =
{
    local entity tempDest;

    if (self.dragonAttacking == FALSE)
        return;

//    skill ramp?
    self.attack_finished = time + (random() * 2 ) + 4;
    self.attack_finished = self.attack_finished - skill;

    self.dragonAttacking = FALSE;
    traceline (self.origin, self.movetarget.origin, TRUE, world);

    if (trace_fraction == 1)
        return;
    else
        bprint ("Error: Dragon cannot get to next target!\n");
};

// ===================================
// dragon_check_attack
// ===================================
void() dragon_check_attack =
{
    local vector    vec;
    local float        dot;

    if ( self.dragonAttacking == TRUE)
        return;
    if ( self.th_missile == SUB_Null)
        return;
    if ( self.attack_finished > time )
        return;
    if ( self.enemy.health < 0)
        self.enemy = world;
    if ( self.enemy.flags & FL_NOTARGET )
        return;

    if (self.enemy == world)
        FindTarget();
    else
    {
        makevectors (self.angles);
        vec = normalize (self.enemy.origin - self.origin);
        dot = vec * v_forward;
//        if ( dot > 0.55 )
        if ( dot > 0.3 )
        {
            traceline (self.origin, self.enemy.origin, TRUE, world);
            if (trace_fraction == 1)
            {
                dot = vlen ( self.enemy.origin - self.origin );
                if (dot < 350)
                {
                    self.dragonAttacking = TRUE;
                    self.think = dragon_melee1;
                    return;
                }
                else
                {
                    self.dragonAttacking = TRUE;
                    self.think = self.th_missile;
                }
            }
//            else
//                bprint ("can't see player!\n");
        }
    }
};

// ===================================
// dragon_move
// ===================================
void(float dist) dragon_move =
{
    local vector moveDir;
    local vector moveAngles;
    local float  leftTurn, rightTurn, turnOffset;
    local entity oldEnemy;

    if ( self.health < 1 )
    {
        remove(self);
        return;
    }

    if (self.dragonAttacking == FALSE) // && self.dragonInTransit == FALSE)
        dragon_check_attack();

    oldEnemy = self.enemy;
    if ( self.dragonAttacking == FALSE )
    {
        moveDir = self.movetarget.origin - self.origin;
        moveAngles = vectoangles ( moveDir );
        self.enemy = self.movetarget;
    }
    else
    {
        moveDir = self.enemy.origin - self.origin;
        moveAngles = vectoangles ( moveDir );
    }

    turnOffset = self.angles_y - moveAngles_y;
    if (turnOffset != 0)
    {
        turnOffset = 180 - self.angles_y;
        leftTurn = anglemod ( moveAngles_y + turnOffset ) - 180;
        rightTurn = 180 - anglemod ( moveAngles_y + turnOffset );

        if (leftTurn < 0)
            leftTurn = 360;
        else if (rightTurn < 0)
            rightTurn = 360;

        self.yaw_speed = 10;
        if (rightTurn < 180)
        {
            if ( self.yaw_speed < rightTurn )
                self.angles_y = self.angles_y - self.yaw_speed;
            else
                self.angles_y = moveAngles_y;

            if ( rightTurn > 5 )
            {
                self.angles_z = self.angles_z + 5;
                if (self.angles_z > 30)
                    self.angles_z = 30;
            }
            turnOffset = rightTurn;
        }
        else
        {
            if ( self.yaw_speed < rightTurn )
                self.angles_y = self.angles_y + self.yaw_speed;
            else
                self.angles_y = moveAngles_y;

            if ( leftTurn > 5 )
            {
                self.angles_z = self.angles_z - 5;
                if (self.angles_z < -30)
                    self.angles_z = -30;
            }
            turnOffset = leftTurn;
        }
    }
    else if (self.angles_z != 0)
    {
        if (self.angles_z < -5)
            self.angles_z = self.angles_z + 5;
        else if (self.angles_z < 5)
            self.angles_z = 0;
        else if (self.angles_z > 5)
            self.angles_z = self.angles_z - 5;
    }

    if ( moveDir_z > 5)
        setorigin ( self, self.origin + '0 0 5');
    else if ( moveDir_z < -5)
        setorigin ( self, self.origin - '0 0 5');

    moveAngles = self.origin;
    walkmove ( self.angles_y, dist );

    if (moveAngles == self.origin)
    {
//        bprint ( "Cannot move smoothly. movetogoal..\n");
        movetogoal ( dist );
    }
    self.enemy = oldEnemy;
};

// ===================================
// movement
// ===================================
void() dragon_stand1 = [ $drgfly01, dragon_walk1 ]
    {
//        bprint ( "dragon_stand1: ERROR - got to stand frame!\n");
    };

// attack A: start - walk1  stop - walk 5
// ================
void() dragon_atk_a1=[$drgfix1a, dragon_atk_a2 ]
    { dragon_move(17); };
void() dragon_atk_a2=[$drgfix1b, dragon_atk_a3 ]
    { dragon_move(17);dragon_fireball();};
void() dragon_atk_a3=[$drgfix1c, dragon_walk5 ]
    { dragon_move(17);dragon_stop_attack();};

// attack B: start - walk3  stop - walk7
// ================
void() dragon_atk_b1=[$drgfix2a, dragon_atk_b2 ]
    { dragon_move(17); };
void() dragon_atk_b2=[$drgfix2b, dragon_atk_b3 ]
    { dragon_move(17);dragon_fireball();};
void() dragon_atk_b3=[$drgfix2c, dragon_walk7 ]
    { dragon_move(17);dragon_stop_attack();};

// attack C: start - walk5  stop - walk9
// ================
void() dragon_atk_c1=[$drgfix3a, dragon_atk_c2 ]
    {dragon_move(17); };
void() dragon_atk_c2=[$drgfix3b, dragon_atk_c3 ]
    { dragon_move(17);dragon_fireball();};
void() dragon_atk_c3=[$drgfix3c, dragon_walk9 ]
    { dragon_move(17);dragon_stop_attack();};

// attack d: start - walk7  stop - walk11
// ================
void() dragon_atk_d1=[$drgfix4a, dragon_atk_d2 ]
    { dragon_move(17); };
void() dragon_atk_d2=[$drgfix4b, dragon_atk_d3 ]
    { dragon_move(17);dragon_fireball();};
void() dragon_atk_d3=[$drgfix4c, dragon_walk11 ]
    { dragon_move(17);dragon_stop_attack();};

// attack E: start - walk9  stop - walk13
// ================
void() dragon_atk_e1=[$drgfix5a, dragon_atk_e2 ]
    { dragon_move(17); };
void() dragon_atk_e2=[$drgfix5b, dragon_atk_e3 ]
    { dragon_move(17);dragon_fireball();};
void() dragon_atk_e3=[$drgfix5c, dragon_walk13 ]
    { dragon_move(17);dragon_stop_attack();};

// attack F: start - walk11  stop - walk3
// ================
void() dragon_atk_f1=[$drgfix6a, dragon_atk_f2 ]
    {dragon_move(17); };
void() dragon_atk_f2=[$drgfix6b, dragon_atk_f3 ]
    { dragon_move(17);dragon_fireball();};
void() dragon_atk_f3=[$drgfix6c, dragon_atk_f4 ]
    { dragon_move(17); };
void() dragon_atk_f4=[$drgfix6c, dragon_walk3 ]
    { dragon_move(17);dragon_stop_attack();};

// ===================================
// dragon walk frames
// ===================================
void() dragon_walk1 =[ $drgfly01, dragon_walk2 ]
    {
        if (self.dragonAttacking)
            dragon_stop_attack();

        self.th_missile=dragon_atk_a1;
        self.dragonPainSequence = 1;
        dragon_move(17);
        if (random() < 0.2)
            sound (self, CHAN_VOICE, "dragon/active.wav", 0.6, ATTN_IDLE);
    };
void() dragon_walk2 =[ $drgfly02, dragon_walk3 ]
    { self.th_missile=SUB_Null; dragon_move(17);};
void() dragon_walk3 =[ $drgfly03, dragon_walk4 ]
    {
        self.th_missile=dragon_atk_b1; dragon_move(17);
        self.dragonPainSequence=2;
    };
void() dragon_walk4 =[ $drgfly04, dragon_walk5 ]
    { self.th_missile=SUB_Null; dragon_move(17);};
void() dragon_walk5 =[ $drgfly05, dragon_walk6 ]
    {
        self.th_missile=dragon_atk_c1; dragon_move(17);
        self.dragonPainSequence=3;
    };
void() dragon_walk6 =[ $drgfly06, dragon_walk7 ]
    { self.th_missile=SUB_Null; dragon_move(17); };
void() dragon_walk7 =[ $drgfly07, dragon_walk8 ]
    {
        self.th_missile=dragon_atk_d1; dragon_move(17);
        self.dragonPainSequence=4;
    };
void() dragon_walk8 =[ $drgfly08, dragon_walk9 ]
    { self.th_missile=SUB_Null; dragon_move(17); };
void() dragon_walk9 =[ $drgfly09, dragon_walk10 ]
    {
        self.th_missile=dragon_atk_e1; dragon_move(17);
        self.dragonPainSequence=5;
    };
void() dragon_walk10 =[ $drgfly10, dragon_walk11 ]
    { self.th_missile=SUB_Null; dragon_move(17); };
void() dragon_walk11 =[ $drgfly11, dragon_walk12 ]
    {
        self.th_missile=dragon_atk_f1; dragon_move(17);
        self.dragonPainSequence=6;
    };
void() dragon_walk12 =[ $drgfly12, dragon_walk13 ]
    { self.th_missile=SUB_Null; dragon_move(17); };
void() dragon_walk13 =[ $drgfly13, dragon_walk1 ]
    { self.th_missile=SUB_Null; dragon_move(17); self.dragonPainSequence=1;};

// ===================================
// attack
// ===================================

void() FireballTouch =
{
    if (other == self.owner)
        return;        // don't explode on owner

    if (self.owner.classname == "monster_dragon")
        T_RadiusDamage (self, self.owner, 90, self.owner);
    else
        T_RadiusDamage (self, self.owner, 30, world);

    sound (self, CHAN_WEAPON, "weapons/r_exp3.wav", 1, ATTN_NORM);
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION2);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
    WriteByte (MSG_BROADCAST, 228);  // 247
    WriteByte (MSG_BROADCAST, 5);

    remove(self);
//    BecomeExplosion();
};

void(vector org, vector dir) launch_fireball =
{
    local entity    fireball;
    local float        fireSpeed;

    self.effects = self.effects | EF_MUZZLEFLASH;
    fireball = spawn();
    fireball.owner = self;
    fireball.movetype = MOVETYPE_FLYMISSILE;
    fireball.solid = SOLID_BBOX;
    fireball.classname = "fireball";
    setmodel (fireball, "progs/fireball.mdl");
    setorigin (fireball, org);
    setsize (fireball, '0 0 0', '0 0 0');

    fireSpeed = random() * 300 + 900;
    fireball.velocity = dir * fireSpeed;
    fireball.avelocity = '0 0 300';
    fireball.angles = vectoangles(fireball.velocity);
    fireball.enemy = self.enemy;

    fireball.touch = FireballTouch;
    fireball.nextthink = time + 6;
    fireball.think = SUB_Remove;
};

// ===========================================
// dragon_fireball
// ===========================================
void() dragon_fireball =
{
    local float     fireCount;
    local vector    org, dir;
    local float        distortion;
    local float     fireType;

    sound (self, CHAN_VOICE, "dragon/attack.wav", 1, ATTN_NORM);

    makevectors (self.angles);
    org = self.origin + v_forward*112 + v_up*32;

    if (random() > 0.66)
    {
        fireCount = 1;
        if ( skill > 1)
            fireCount = 2;
        fireType = FIRE_PLASMA;
    }
    else
    {
        fireType = FIRE_FIREBALL;
        fireCount = random() * skill;            // easy 0, med 0-1, hard 0-2
           fireCount = rint ( fireCount ) + 1;
    // easy 1, med 1-2, hard 1-3
    }

    while (fireCount > 0)
    {
        distortion = ( random() - 0.5 ) * 0.25;
        dir = normalize ( self.enemy.origin - org );
        makevectors (dir);
        dir = dir + v_right * distortion;

         if (fireType == FIRE_FIREBALL)
            launch_fireball(org, dir);
        else
            launch_plasma(org, dir);

        fireCount = fireCount - 1;
    }

};

// ===================================
// melee attacks
// ===================================
void() dragon_tail_touch =
{
    local vector    delta;
    local float        tailDamage;

    if (!self.enemy)
        return;
    if (!CanDamage (self.enemy, self))
        return;

    ai_charge(10);

    delta = self.enemy.origin - self.origin;
    tailDamage = vlen(delta);
    if (vlen(delta) > 150)
    {
        return;
    }

    tailDamage = ( random() * 30 ) + 30;
    T_Damage ( self.enemy, self, self, tailDamage);
};

void() dragon_tail =
{
    local float dist;
    local vector dir;

    if (!self.enemy)
        return;
    if (!CanDamage (self.enemy, self))
        return;

    dragon_move(10);

    dir = self.enemy.origin - self.origin;
    dist = vlen ( dir );
    if ( dist < 250 )
    {
        T_Damage ( self.enemy, self, self, 30 );
        dir = normalize (dir);
        self.enemy.velocity = dir * 500;
        self.enemy.velocity_z = 350;
    }

    dragon_stop_attack();
};

void() dragon_melee1=[$drgslh01, dragon_melee2 ] { dragon_move(12);};
void() dragon_melee2=[$drgslh02, dragon_melee3 ] { dragon_move(12);};
void() dragon_melee3=[$drgslh03, dragon_melee4 ] { dragon_move(12);};
void() dragon_melee4=[$drgslh04, dragon_melee5 ] { dragon_move(12);};
void() dragon_melee5=[$drgslh05, dragon_melee6 ] { dragon_move(12);};
void() dragon_melee6=[$drgslh06, dragon_melee7 ] { dragon_move(12);};
void() dragon_melee7=[$drgslh07, dragon_melee8 ] { dragon_move(12);};
void() dragon_melee8=[$drgslh08, dragon_melee9 ] { dragon_move(12);};
void() dragon_melee9=[$drgslh09, dragon_melee10] { dragon_move(12);};
void() dragon_melee10=[$drgslh10, dragon_melee11] { dragon_tail();};
void() dragon_melee11=[$drgslh11, dragon_melee12] { dragon_move(10);};
void() dragon_melee12=[$drgslh12, dragon_melee13] { dragon_move(10);};
void() dragon_melee13=[$drgslh13, dragon_walk1 ]
{ dragon_move(10);};

// ===================================
// pain
// ===================================
void() dragon_painA1 = [ $drgpan1a, dragon_painA2 ] {dragon_move(12);};
void() dragon_painA2 = [ $drgpan1b, dragon_painA3 ] {dragon_move(12);};
void() dragon_painA3 = [ $drgpan1c, dragon_walk5 ] {dragon_move(12);};

void() dragon_painB1 = [ $drgpan2a, dragon_painB2 ] {dragon_move(12);};
void() dragon_painB2 = [ $drgpan2b, dragon_painB3 ] {dragon_move(12);};
void() dragon_painB3 = [ $drgpan2c, dragon_walk7 ] {dragon_move(12);};

void() dragon_painC1 = [ $drgpan3a, dragon_painC2 ] {dragon_move(12);};
void() dragon_painC2 = [ $drgpan3b, dragon_painC3 ] {dragon_move(12);};
void() dragon_painC3 = [ $drgpan3c, dragon_walk9 ] {dragon_move(12);};

void() dragon_painD1 = [ $drgpan4a, dragon_painD2 ] {dragon_move(12);};
void() dragon_painD2 = [ $drgpan4b, dragon_painD3 ] {dragon_move(12);};
void() dragon_painD3 = [ $drgpan4c, dragon_walk11 ] {dragon_move(12);};

void() dragon_painE1 = [ $drgpan5a, dragon_painE2 ] {dragon_move(12);};
void() dragon_painE2 = [ $drgpan5b, dragon_painE3 ] {dragon_move(12);};
void() dragon_painE3 = [ $drgpan5c, dragon_walk13 ] {dragon_move(12);};

void() dragon_painF1 = [ $drgpan6a, dragon_painF2 ] {dragon_move(12);};
void() dragon_painF2 = [ $drgpan6b, dragon_painF3 ] {dragon_move(12);};
void() dragon_painF3 = [ $drgpan6c, dragon_walk2 ] {dragon_move(12);};

void(entity attacker, float damage) dragon_pain =
{
    local float r;

    if (self.pain_finished > time)
        return;

    r = random();

    if (r < 0.25)
    {
        dragon_stop_attack();

        sound (self, CHAN_VOICE, "dragon/pain.wav", 1, ATTN_NORM);
        self.pain_finished = time + 2;

        if (self.dragonPainSequence == 1)
            self.think = dragon_painA1;
        else if (self.dragonPainSequence == 2)
            self.think = dragon_painF1;
        else if (self.dragonPainSequence == 3)
            self.think = dragon_painE1;
        else if (self.dragonPainSequence == 4)
            self.think = dragon_painD1;
        else if (self.dragonPainSequence == 5)
            self.think = dragon_painC1;
        else if (self.dragonPainSequence == 6)
            self.think = dragon_painB1;
    }
};

// ===================================
// death
// ===================================
void() dragon_boom2 =
{
    self.velocity = self.dragonLastVelocity;
    ViolentDeath(15);
    self.target = "dragondoor";
    SUB_UseTargets();
    remove(self);
};

void() dragon_boom1 =
{
    if ( self.dragonDeathState > DDIE_STOPPED )
//        objerror ( "dragon_boom1");
        return;

    self.dragonDeathState = DDIE_EXPLODED;
    ThrowGib ("progs/drggib01.mdl", -100);
    ThrowGib ("progs/drggib02.mdl", -100);
    ThrowGib ("progs/drggib03.mdl", -100);
    sound (self, CHAN_BODY, "player/tornoff2.wav", 1, ATTN_NONE);
    self.think = dragon_boom2;
    self.nextthink = time + 0.1;
};

void() dragon_explode =
{
    if ( self.dragonDeathState > DDIE_FALLING )
//        objerror ( "dragon_explode");
        return;

    if (vlen(self.velocity) < 100 || self.flags & FL_INWATER)
    {
        self.dragonDeathState = DDIE_STOPPED;
        dragon_boom1();
    }
    else
    {
        makevectors ( self.angles );
        self.velocity = self.velocity - 40 * v_up;
        self.dragonLastVelocity = self.velocity;
    }
};

void() dragon_squish =
{
    if (other.classname == "player")
    {
        self.classname = "monster_dragon_dead";
        T_Damage( other, self, self, 200);
    }
    if (other == world)
    {
        self.velocity = '0 0 0';
        dragon_explode();
    }
};

void() dragon_death1 = [ $drgdth01, dragon_death2 ]
    {
    if ( self.dragonDeathState > 0)
//        objerror ( "dragon_death1");
        return;

    self.dragonDeathState = DDIE_FALLING;
    self.use = SUB_Null;
    makevectors ( self.angles );
    self.velocity = 300 * v_forward - 40 * v_up;
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    setsize (self, VEC_HULL_MIN, VEC_HULL_MAX);
    self.touch = dragon_squish;

    sound (self, CHAN_VOICE, "dragon/death.wav", 1, ATTN_NONE);
    self.dragonAttacking = FALSE;
    };
void() dragon_death2 = [ $drgdth02, dragon_death3 ] {dragon_explode();};
void() dragon_death3 = [ $drgdth03, dragon_death4 ] {dragon_explode();};
void() dragon_death4 = [ $drgdth04, dragon_death5 ] {dragon_explode();};
void() dragon_death5 = [ $drgdth05, dragon_death6 ] {dragon_explode();};
void() dragon_death6 = [ $drgdth06, dragon_death7 ] {dragon_explode();};
void() dragon_death7 = [ $drgdth07, dragon_death8 ] {dragon_explode();};
void() dragon_death8 = [ $drgdth08, dragon_death9 ] {dragon_explode();};
void() dragon_death9 = [ $drgdth09, dragon_death10 ] {dragon_explode();};
void() dragon_death10 = [ $drgdth10, dragon_death11 ] {dragon_explode();};
void() dragon_death11 = [ $drgdth11, dragon_death12 ] {dragon_explode();};
void() dragon_death12 = [ $drgdth12, dragon_death13 ] {dragon_explode();};
void() dragon_death13 = [ $drgdth13, dragon_death14 ] {dragon_explode();};
void() dragon_death14 = [ $drgdth14, dragon_death15 ] {dragon_explode();};
void() dragon_death15 = [ $drgdth15, dragon_death16 ] {dragon_explode();};
void() dragon_death16 = [ $drgdth16, dragon_death17 ] {dragon_explode();};
void() dragon_death17 = [ $drgdth17, dragon_death18 ] {dragon_explode();};
void() dragon_death18 = [ $drgdth18, dragon_death19 ] {dragon_explode();};
void() dragon_death19 = [ $drgdth19, dragon_death20 ] {dragon_explode();};
void() dragon_death20 = [ $drgdth20, dragon_death21 ] {dragon_explode();};
void() dragon_death21 = [ $drgdth21, dragon_death21 ]
{
    ViolentDeath(39);
    self.target = "dragondoor";
    SUB_UseTargets();
    remove(self);
};

// ===================================
// Delayed Activation
// ===================================
void() dragon_use =
{
    if (self.health < 1)
    {
        self.use = SUB_Null;
        return;
    }

    self.th_stand = dragon_walk1;
    self.th_walk = dragon_walk1;
    self.think = dragon_walk1;
    self.nextthink = time + 0.1;
};

// ===================================
// dragon activation code
// ===================================
void() dragon_activate =
{
    self.takedamage = DAMAGE_AIM;
    self.ideal_yaw = self.angles * '0 1 0';
    if (!self.yaw_speed)
        self.yaw_speed = 10;
    self.view_ofs = '0 0 25';

    self.flags = self.flags | FL_FLY;
    self.flags = self.flags | FL_MONSTER;

    if (!walkmove(0,0))
    {
        dprint ("flymonster in wall!\n");
        dprint (self.classname);
        dprint (" at ");
        dprint (vtos(self.origin));
        dprint ("\n");
    }

    if (self.target)
    {
        self.movetarget = find(world, targetname, self.target);
        self.goalentity = self.movetarget;
        if (!self.movetarget)
        {
            dprint ("Monster can't find target at ");
            dprint (vtos(self.origin));
            dprint ("\n");
        }
    }

    if (self.targetname)
        self.use = dragon_use;
    else
        dragon_use();

};

/*QUAKED monster_dragon (1 0 0) (-32 -32 -24) (32 32 64) Ambush
*/
void() monster_dragon =
{
    if (deathmatch)
    {
        remove(self);
        return;
    }
    precache_model ("progs/dragon.mdl");
    precache_model ("progs/fireball.mdl");
    precache_model ("progs/drggib01.mdl");
    precache_model ("progs/drggib02.mdl");
    precache_model ("progs/drggib03.mdl");

    precache_sound ("dragon/see.wav");
    precache_sound ("dragon/active.wav");
    precache_sound ("dragon/attack.wav");
    precache_sound ("dragon/pain.wav");
    precache_sound ("dragon/death.wav");

    self.dragonInRoom = DT_ROOM_A;
    self.dragonInTransit = 0;
    self.dragonAttacking = FALSE;
    self.playerInRoom = DT_ROOM_A;
    self.playerInTransit = 0;

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;

    setmodel (self, "progs/dragon.mdl");
    setsize (self, VEC_HULL2_MIN, VEC_HULL2_MAX);
    self.health = 3000 + (1000 * cvar("skill"));

    self.th_run = dragon_walk1;
    self.th_pain = dragon_pain;
    self.th_die = dragon_death1;

    self.dragonPainSequence=1;

    total_monsters = total_monsters + 1;
    self.nextthink = self.nextthink + 0.1;
    self.think = dragon_activate;
};

// =================================================
// dragon trigger
// -- OBSOLETE!!
// =================================================

/*QUAKED trigger_dragon (.5 .5 .5) ? A B C
Trigger to let dragon know what room you're in.

Set the A,B,C to which room this trigger is at.

Point the trigger towards the room. If the player is going
the direction the trigger points, he is entering the room.
If not, he is leaving.
*/
void() trigger_dragon =
{
    remove(self);
};

// =================================================
// dragon corners
// =================================================
void() dragon_corner_touch =
{
    if (other.movetarget != self)
        return;

    if (other.classname != "monster_dragon")
        return;

    other.movetarget = find (world, targetname, self.target);
    other.goalentity = other.movetarget;
    other.target = self.target;

    if (!other.movetarget)
    {
        objerror ("dragon_corner: no target found");
    }
};

/*QUAKED dragon_corner (0.5 0.3 0) (-16 -16 -16) (16 16 16)
Only for use with monster_dragon. Use only as transitions
between rooms.
*/
void() dragon_corner =
{
    if (!self.targetname)
        objerror ("dragon_corner: no targetname");

    self.solid = SOLID_TRIGGER;
    self.movetype = MOVETYPE_NONE;
    self.touch = dragon_corner_touch;
    self.modelindex = 0;
    self.model = "";
    setsize (self, '-16 -16 -16', '16 16 16');
};

