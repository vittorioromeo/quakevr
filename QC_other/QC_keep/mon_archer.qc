/*
==============================================================================

ARCHER

==============================================================================
*/

$cd id1/models/archer
$origin 0 0 24
$base base        
$skin skin

$frame backup1 backup2 backup3 backup4 backup5 backup6 backup7 backup8

$frame deathA1 deathA2 deathA3 deathA4 deathA5 deathA6 deathA7 deathA8
$frame deathA9 deathA10 deathA11 deathA12 deathA13 deathA14 deathA15
$frame deathA16 deathA17 deathA18 deathA19 deathA20 deathA21 deathA22

$frame draw1 draw2 draw3 draw4 draw5 draw6 draw7 draw8 draw9 draw10
$frame draw11 draw12 draw13

//$frame duck1 duck2 duck3 duck4 duck5 duck6 duck7 duck8 duck9 duck10
//$frame duck11 duck12 duck13 duck14

$frame fire1 fire2 fire3 fire4

$frame pain1 pain2 pain3 pain4 pain5 pain6 pain7 pain8 pain9 pain10
$frame pain11 pain12 pain13 pain14 pain15 pain16

$frame patrol1 patrol2 patrol3 patrol4 patrol5 patrol6 patrol7 patrol8
$frame patrol9 patrol10 patrol11 patrol12 patrol13 patrol14 patrol15
$frame patrol16 patrol17 patrol18 patrol19 patrol20 patrol21 patrol22

$frame redraw1 redraw2 redraw3 redraw4 redraw5 redraw6 redraw7 redraw8
$frame redraw9 redraw10 redraw11 redraw12

$frame tranA1 tranA2 tranA3 tranA4 tranA5 tranA6 tranA7 tranA8 tranA9
$frame tranA10 tranA11 tranA12 tranA13

$frame tranB1 tranB2 tranB3 tranB4 tranB5 tranB6 tranB7 tranB8 tranB9
$frame tranB10 tranB11 tranB12 tranB13 tranB14 tranB15 tranB16

//$frame tranC1 tranC2 tranC3 tranC4 tranC5 tranC6 tranC7 tranC8 tranC9 tranC10

//$frame tranD1 tranD2 tranD3 tranD4 tranD5 tranD6 tranD7 tranD8 tranD9

$frame waitA1 waitA2 waitA3 waitA4 waitA5 waitA6 waitA7 waitA8 waitA9 waitA10
$frame waitA11 waitA12 waitA13 waitA14 waitA15 waitA16 waitA17 waitA18

$frame waitB1 waitB2 waitB3 waitB4 waitB5 waitB6 waitB7 waitB8 waitB9
$frame waitB10 waitB11 waitB12

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9 walk10
$frame walk11 walk12 walk13 walk14 walk15 walk16

// SoE:  Vorpal Sword
$frame decap1 decap2 decap3 decap4 decap5 decap6 decap7 decap8 decap9 decap10
$frame decap11 decap12 decap13 decap14 decap15 decap16 decap17

//=============================================================================

//- - - - - - - - -
//  Model is based on the archer from Hexen2.  AI used is mostly Quake's
//  with bit of Hexen2's.  Fancy moves such as ducking and sidestepping
//  are NOT used, since it is so un-Quake (and I simply want a medieval
//  style grunt/enforcer).  Holding his shot after drawing the bow,
//  along with much of the shooting AI, was ported over.
//
//                                              -- Patrick Martin
//- - - - - - - - -

// Check if the archer has a clear shot to his target.
float() archer_check_shot = {
    local   vector  spot1, spot2;
    local   entity  targ;

    targ = self.enemy;

    makevectors (self.angles);
    spot1 = self.origin + v_forward*16 + v_right*8 + v_up*24;
    spot2 = targ.origin + targ.view_ofs;

    traceline (spot1, spot2, ARROW_FLY, self);

    if (trace_ent != targ)
    {
        // Didn't hit the intended target.  Either something else blocked
        // the scan, or the scan was complete and could not hit the target.
        // Standard 1.06 progs would just return FALSE.
        if (trace_fraction < 1)
        {   // don't have a clear shot
            //if (DontHurtMe (trace_ent))
                return FALSE;   // Don't shoot whoever is in line of fire.

            // Otherwise, whoever is in the way is a potential enemy and
            // hitting him instead of the intended target with missile
            // attacks is still useful.
        }
        else
        {   // Target is non-solid.  It could be a wraith, downed zombie,
            // or something in spectator mode.  Attack only if target is
            // a ghost.
            if (targ.takedamage != DAMAGE_AIM)
                return FALSE;
            //if (!IsGhost (targ))
                //return FALSE;
// Commented out because those with no mindex should have DAMAGE_NO too.
//            if (!targ.modelindex)
//                return FALSE;
        }
    }

    // PM:  Added alert support so that some monsters can see and attack
    //      through water like players can.
    // FIXME:  Is the 'fish check' necessary here?
    if (trace_inopen)
        if (trace_inwater)
            if (!(self.ryuflags & RYU_ALERT))   // PM:  Alert support.
                return FALSE;            // sight line crossed contents

    return TRUE;
};

// Basically a copy of CheckAttack except for spot1 and spot2 in the
// traceline check.
void() ArcherCheckAttack = {
    if (enemy_range == RANGE_FAR)
        return;   // Don't call traceline again when we don't need to.
    if (!archer_check_shot ())
        return;
    if (time < self.attack_finished)
        return;

    local   float   chance;

    if (enemy_range == RANGE_MELEE)
    {
        chance = 0.9;
        self.attack_finished = 0;
    }
    else if (enemy_range == RANGE_NEAR)
            chance = 0.4;
    else if (enemy_range == RANGE_MID)
            chance = 0.1;
    else //if (enemy_range == RANGE_LONG)     // PM:  New range!
            chance = 0.04;
//    else
//        chance = 0;

    if (random () < chance) {
        chance = 2*random();
        self.th_missile ();
        SUB_AttackFinished (chance);
        return;
    }

    return;
};


//=============================================================================

float() archer_lord = {return (self.classname == "monster_archer_lord");};

void(vector st, float up) M_FireAkArrow = {
    local   vector  org, dir;
    local   entity  targ;
    local   float   type;

    if (archer_lord ())
        targ = self.enemy;
    else
        targ = world;

    makevectors (self.angles);
    org = self.origin + v_forward*st_x + v_right*st_y + v_up*st_z;
    dir = Aimpoint (self.enemy);
    dir = Maim_Line (org, dir, AKARROW_SPEED_FLY, targ, '0 0 0');

    // Note:  Magic arrows can hit ghosts, but normal arrows cannot.
    if (archer_lord ()) {   
        //if (IsGhost (self.enemy))
        //    {type = 2;  up = 0;}    // Auto upgrade to hit ghosts.
        //else
        //{
            type = 1;
            if (up)     // Upgrade check.
            {   // Check:  Don't upgrade arrow if hitting wrong target is likely.
                //  Used to stop blue arrows from hitting and moving monsters around
                //  by hitting them with a weaker arrow instead.
                traceline (org, org + dir*2048, ARROW_FLY, self);

                if ((trace_ent == self.enemy) || !(trace_ent.flags & FLx_CREATURE))
                    type = 2;   // Upgrade arrow to the next type.
            }
        //}
    } else {
        // In Hexen2, grunt arrow damage is the same regardless of type.
        // Therefore, do not upgrade arrows for them in Drake.
        type = self.skin & 1;
    }

    // In Hexen2, 0=green, 1=red, 2=gold.
    if (type & 1)
        sound (self, CHAN_WEAPON, "archer/arrowr.wav", 1, ATTN_NORM);
    else
        sound (self, CHAN_WEAPON, "archer/arrowg.wav", 1, ATTN_NORM);

    AkArrow_Launch (org, dir, type);
};

// STAND - - - - - - - - - - - - - - - -
void() archer_stand1  =[ $waitA1,  archer_stand2  ] {ai_stand();};
void() archer_stand2  =[ $waitA2,  archer_stand3  ] {ai_stand();};
void() archer_stand3  =[ $waitA3,  archer_stand4  ] {ai_stand();};
void() archer_stand4  =[ $waitA4,  archer_stand5  ] {ai_stand();};
void() archer_stand5  =[ $waitA5,  archer_stand6  ] {ai_stand();};
void() archer_stand6  =[ $waitA6,  archer_stand7  ] {ai_stand();};
void() archer_stand7  =[ $waitA7,  archer_stand8  ] {ai_stand();};
void() archer_stand8  =[ $waitA8,  archer_stand9  ] {ai_stand();};
void() archer_stand9  =[ $waitA9,  archer_stand10 ] {ai_stand();};
void() archer_stand10 =[ $waitA10, archer_stand11 ] {ai_stand();};
void() archer_stand11 =[ $waitA11, archer_stand12 ] {ai_stand();};
void() archer_stand12 =[ $waitA12, archer_stand13 ] {ai_stand();};
void() archer_stand13 =[ $waitA13, archer_stand14 ] {ai_stand();};
void() archer_stand14 =[ $waitA14, archer_stand15 ] {ai_stand();};
void() archer_stand15 =[ $waitA15, archer_stand16 ] {ai_stand();};
void() archer_stand16 =[ $waitA16, archer_stand17 ] {ai_stand();};
void() archer_stand17 =[ $waitA17, archer_stand18 ] {ai_stand();};
void() archer_stand18 =[ $waitA18, archer_stand1  ] {ai_stand();};

// WALK - - - - - - - - - - - - - - - -
void() archer_draw = {sound (self, CHAN_WEAPON, "archer/draw.wav", 1, ATTN_NORM);};

void() archer_growl =
{
    if (random() < 0.1)
    {
        if (archer_lord ())
        {
            if (random() < 0.7)
                sound (self, CHAN_VOICE, "archer/growl2.wav", 1, ATTN_NORM);
            else
                sound (self, CHAN_VOICE, "archer/growl3.wav", 1, ATTN_NORM);
        }
        else
            sound (self, CHAN_VOICE, "archer/growl.wav", 1, ATTN_NORM);
    }
};

void() archer_walk1  =[ $patrol2,  archer_walk2  ] {archer_growl(); ai_walk(2);};
void() archer_walk2  =[ $patrol4,  archer_walk3  ] {ai_walk(3);};
void() archer_walk3  =[ $patrol6,  archer_walk4  ] {ai_walk(6);};
void() archer_walk4  =[ $patrol8,  archer_walk5  ] {ai_walk(6);};
void() archer_walk5  =[ $patrol10, archer_walk6  ] {monster_footstep(FALSE); ai_walk(4);};
void() archer_walk6  =[ $patrol12, archer_walk7  ] {ai_walk(1);};
void() archer_walk7  =[ $patrol14, archer_walk8  ] {ai_walk(3);};
void() archer_walk8  =[ $patrol16, archer_walk9  ] {ai_walk(4);};
void() archer_walk9  =[ $patrol18, archer_walk10 ] {ai_walk(4);};
void() archer_walk10 =[ $patrol20, archer_walk11 ] {ai_walk(4);};
void() archer_walk11 =[ $patrol22, archer_walk1  ] {monster_footstep(FALSE); ai_walk(2);};

// RUN - - - - - - - - - - - - - - - - -
void() archer_run1  =[ $walk2,  archer_run2  ] {archer_growl(); ai_run(4);};
void() archer_run2  =[ $walk4,  archer_run3  ] {ai_run(8);};
void() archer_run3  =[ $walk6,  archer_run4  ] {ai_run(10);};
void() archer_run4  =[ $walk8,  archer_run5  ] {monster_footstep(FALSE); ai_run(9);};
void() archer_run5  =[ $walk10, archer_run6  ] {ai_run(4);};
void() archer_run6  =[ $walk12, archer_run7  ] {ai_run(8);};
void() archer_run7  =[ $walk14, archer_run8  ] {ai_run(10);};
void() archer_run8  =[ $walk16, archer_run1  ] {monster_footstep(FALSE); ai_run(9);};

// ATTACK - - - - - - - - - - - - - - -
float   ARCHER_CLOSE    = 200;
float   ARCHER_FAR      = 500;

void() archer_fire1;

float() archer_face =
{
// SoE:  Normal archers use stupid old enforcer AI.
    if (soul_evil)
    {
        if (archer_lord ())
            if (!visible (self.enemy))
                {self.think = archer_run1;  return FALSE;}
        ai_face();
        return TRUE;
    }

// Emulate Hexen2 behavior.
    if (visible (self.enemy))
        {ai_face();  return TRUE;}
    else
        {self.think = archer_run1;  return FALSE;}
};

void() archer_hold =
{
// SoE:  Make archer grunts emulate Quake enforcers better.
    if (soul_evil)
    {
        if (!archer_lord ())
        {
            self.shots_left = !self.shots_left;
            archer_fire1 ();
            return;
        }
    }

//- - - - - - - - - - - - - - - - - - -
// Animate the archer.
    local   float   chance, dist, r;

    r = 1;
    dist = vlen (self.enemy.origin - self.origin);
    chance = infront (self.enemy);
    if ( chance && ((dist <= ARCHER_CLOSE) || (dist > ARCHER_FAR)) )
    {
        if (dist <= ARCHER_CLOSE)
        {   // Enemy too close -- backup.
            if ( self.frame == $backup1 )
                {self.frame = $backup2; ai_back(1);}
            else if ( self.frame == $backup2 )
                {self.frame = $backup3; ai_back(1);}
            else if ( self.frame == $backup3 )
                {self.frame = $backup4; ai_back(2);}
            else if ( self.frame == $backup4 )
                {self.frame = $backup5; ai_back(1);}
            else if ( self.frame == $backup5 )
                {self.frame = $backup6; ai_back(2);}
            else if ( self.frame == $backup6 )
                {self.frame = $backup7; ai_back(4);}
            else if ( self.frame == $backup7 )
                {self.frame = $backup8; ai_back(4); r = 0;}
            else
                {self.frame = $backup1; ai_back(2);}
        }
        else
        {   // Enemy should be closer -- advance.
            if ( self.frame == $backup8 )
                {self.frame = $backup7; ai_forward(4);}
            else if ( self.frame == $backup7 )
                {self.frame = $backup6; ai_forward(4);}
            else if ( self.frame == $backup6 )
                {self.frame = $backup5; ai_forward(2);}
            else if ( self.frame == $backup5 )
                {self.frame = $backup4; ai_forward(1);}
            else if ( self.frame == $backup4 )
                {self.frame = $backup3; ai_forward(2);}
            else if ( self.frame == $backup3 )
                {self.frame = $backup2; ai_forward(2);}
            else if ( self.frame == $backup2 )
                {self.frame = $backup1; ai_forward(1); r = 0;}
            else
                {self.frame = $backup8; ai_forward(2);}
        }
    }
    else
    {   // Enemy is either within ideal distance or behind archer -- stay.
        if ( ( self.frame >= $waitB2 ) && ( self.frame <= $waitB10 ) )
            self.frame = self.frame + 2;
        else
            {self.frame = $waitB2; r = 0;}
    }
    self.nextthink = time + 0.1;
    self.think = archer_hold;
//- - - - - - - - - - - - - - - - - - -

// Don't shoot blindly if enemy is confirmed to be dead.
// Doing so might hit other monsters (behind target) we'd rather not anger.
    if (self.enemy.health <= 0)
        if (self.enemy.deadflag >= DEAD_DEAD)
            {self.think = archer_run1;  return;}

    if (!archer_face ())
        return;     // Thinking set to th_run by call to function.

// Borrowing v_forward from the prior call to makevectors in 'infront'...
    if (!chance)
    {   // See if the target is still not infront after elevation is gone.
        local   vector  spot;

        spot = self.enemy.origin;
        spot_z = self.origin_z;
        spot = normalize(spot - self.origin);
        if (spot * v_forward <= 0.3)
            return;     // Target is really not infront.
    }

    if (r)
    {   // Random hold algorithm from Hexen2.
        chance = (skill * 0.05) + (self.skin * 0.10);
        if (random() >= chance)
            return;
    }

    if (!archer_check_shot ())
    {   // Something got in the way.  Abort shot then move.
        sound (self, CHAN_WEAPON, SOUND_EMPTY, 1, ATTN_NORM);
        self.think = archer_run1;
        return;
    }

// Hexen2:  40% chance to automatically shoot in melee.
// In Hexen, this is done before random and shot checks.
// Here in Quake, do this after checking shot because it is possible for
// thin monsters to squeeze between the archer and target even in melee range.
    enemy_range = range(self.enemy);
    if (enemy_range < RANGE_NEAR)
        if (random() < 0.4)
        {
            archer_fire1 ();
            return;
        }

    if (!archer_lord ())
    {   // Grunts don't always shoot immediately.
        if (enemy_range == RANGE_MELEE)
            chance = 1;
        else if (enemy_range == RANGE_NEAR)
            chance = 0.7;
        else if (enemy_range == RANGE_MID)
            chance = 0.6;
        else
            chance = 0.5;

        if (random() >= chance)
            return;
    }

    archer_fire1 ();
};

void() archer_redraw1  =[ $redraw2,  archer_redraw2  ] {archer_face();};
void() archer_redraw2  =[ $redraw4,  archer_redraw3  ] {archer_face();};
void() archer_redraw3  =[ $redraw6,  archer_redraw4  ] {archer_face();};
void() archer_redraw4  =[ $redraw8,  archer_redraw5  ] {archer_face(); archer_draw();};
void() archer_redraw5  =[ $redraw10, archer_redraw6  ] {archer_face();};
void() archer_redraw6  =[ $redraw11, archer_redraw7  ] {archer_face();};
void() archer_redraw7  =[ $redraw12, archer_hold     ] {archer_face();};

// Advancing attack
void() archer_attack1  =[ $tranA2,  archer_attack2  ] {archer_face(); ai_forward(8);};
void() archer_attack2  =[ $tranA4,  archer_attack3  ] {archer_face(); ai_forward(7);};
void() archer_attack3  =[ $tranA6,  archer_attack4  ] {archer_face(); ai_forward(6); archer_draw();};
void() archer_attack4  =[ $tranA8,  archer_attack5  ] {archer_face(); ai_forward(4);};
void() archer_attack5  =[ $tranA10, archer_attack6  ] {archer_face(); monster_footstep(FALSE); ai_forward(5);};
void() archer_attack6  =[ $tranA12, archer_attack7  ] {archer_face();};
void() archer_attack7  =[ $tranA13, archer_hold     ] {archer_face();};

// Retreating attack
void() archer_attackb1 =[ $draw2,  archer_attackb2  ] {archer_face();};
void() archer_attackb2 =[ $draw4,  archer_attackb3  ] {archer_face(); ai_back(4);};
void() archer_attackb3 =[ $draw6,  archer_attackb4  ] {archer_face(); ai_back(6);};
void() archer_attackb4 =[ $draw8,  archer_attackb5  ] {archer_face(); ai_back(7); archer_draw();};
void() archer_attackb5 =[ $draw10, archer_attackb6  ] {archer_face(); ai_back(8);};
void() archer_attackb6 =[ $draw12, archer_attackb7  ] {archer_face(); monster_footstep(FALSE); ai_back(4);};
void() archer_attackb7 =[ $draw13, archer_hold      ] {archer_face();};

void() archer_attack =
{
    if (enemy_range < RANGE_MID)    // Range set by CheckAttack.
        archer_attackb1 ();     // Step back.
    else
        archer_attack1 ();      // Step forward.
};

void() archer_fire1    =[ $fire2,  archer_fire2    ] {ai_face();
    local   float   chance, up;

    // Check for upgrade chance.
    chance = (0.5 - (range(self.enemy) * 0.2));
    if (chance < 0)
        up = chance = 0;
    else
        up = (random() < chance);       // Upgrade arrow type if TRUE.

    // Launch!
    M_FireAkArrow ('16 8 24', up);

    // Normal Hexen2 refire check.
    if (self.enemy.health <= 0)
        chance = 0;     // Enemy is dead.
    else {
        chance = chance + (skill * 0.1);
        if (!archer_lord ()) chance = chance - 0.3;
    }
// SoE:  Grunts emulate enforcer ai.
    if (soul_evil) {
        if (!archer_lord ()) {
            if (self.shots_left)
                chance = 2;
            else
                chance = 0;
        }
    }
//- - - - - - - - -

    if (random() < chance)
        self.think = archer_redraw5;
    else
        SUB_CheckRefire (archer_redraw5);
};
void() archer_fire2    =[ $fire4,  archer_fire3    ] {ai_face();};
// The rest is a transition scene from fire to run.
void() archer_fire3    =[ $tranB2,  archer_fire4   ] {ai_run(3);};
void() archer_fire4    =[ $tranB4,  archer_fire5   ] {ai_run(10);};
void() archer_fire5    =[ $tranB6,  archer_fire6   ] {ai_run(12);};
void() archer_fire6    =[ $tranB8,  archer_fire7   ] {ai_run(8);};
void() archer_fire7    =[ $tranB10, archer_fire8   ] {monster_footstep(FALSE); ai_run(3);};
void() archer_fire8    =[ $tranB12, archer_fire9   ] {ai_run(8);};
void() archer_fire9    =[ $tranB14, archer_fire10  ] {ai_run(11);};
void() archer_fire10   =[ $tranB16, archer_run1    ] {ai_run(8);};

// PAIN - - - - - - - - - - - - - - - -
// Counter means counterattack here.  Use at end of pain anims.
void() archer_counter =
{
    if (self.enemy)
        if (visible (self.enemy))
            if (infront (self.enemy))
                if (archer_check_shot ())
                    self.think = archer_fire1;
};

// This first pain anim is Quake-style flinching.  Not found in Hexen2.
void() archer_pain1  =[ $pain1,  archer_pain2  ] {};
void() archer_pain2  =[ $pain2,  archer_pain3  ] {};
void() archer_pain3  =[ $pain3,  archer_pain4  ] {};
void() archer_pain4  =[ $pain2,  archer_pain5  ] {};
void() archer_pain5  =[ $pain1,  archer_run1   ] {};

// The rest are redraw pain anims as per Hexen2.
void() archer_painb1 =[ $pain2,  archer_painb2 ] {};
void() archer_painb2 =[ $pain4,  archer_painb3 ] {};
void() archer_painb3 =[ $pain6,  archer_painb4 ] {};
void() archer_painb4 =[ $pain8,  archer_painb5 ] {};
void() archer_painb5 =[ $pain10, archer_painb6 ] {monster_footstep(FALSE);};
void() archer_painb6 =[ $pain12, archer_painb7 ] {ai_face(); archer_draw();};
void() archer_painb7 =[ $pain14, archer_painb8 ] {ai_face();};
void() archer_painb8 =[ $pain16, archer_hold   ] {ai_face(); archer_counter();};

// Note:  Do not use redraw functions as a pain anim because the archer can
// short-curcuit the anim if no enemy is seen, and we don't want to let him.
// Update:  Use redraw sequence after the second frame.
void() archer_painc1  =[ $redraw2,  archer_painc2  ] {};
void() archer_painc2  =[ $redraw4,  archer_redraw3 ] {};
//void() archer_painc3  =[ $redraw6,  archer_painc4  ] {ai_face();};
//void() archer_painc4  =[ $redraw8,  archer_painc5  ] {ai_face(); archer_draw();};
//void() archer_painc5  =[ $redraw10, archer_painc6  ] {ai_face();};
//void() archer_painc6  =[ $redraw11, archer_painc7  ] {ai_face();};
//void() archer_painc7  =[ $redraw12, archer_hold    ] {ai_face(); archer_counter();};

void(entity inflictor, entity attacker, float damage) archer_pain = {
    // don't make multiple pain sounds right after each other
    // Note:  Archers redraw bow during this time in Hexen2.  Don't do it here.
    if (self.pain_finished > time)
        return;

    self.pain_finished = time + 1;
    if (archer_lord ()) {
        sound (self, CHAN_VOICE, "archer/pain2.wav", 1, ATTN_NORM);
    // The archer looks like a hell knight, so give him the same no-flinch,
    // but only if he is a lord class.
        if (random()*30 > damage)
            return;        // didn't flinch
    } else
        sound (self, CHAN_VOICE, "archer/pain.wav", 1, ATTN_NORM);

    if (SUB_IgnorePain ())
        return;         // No pain anims in Nightmare.

    if (self.fighting)  // In case enemy is set to master during follow AI.
        if (self.enemy)
        {   // In Hexen2, both redraw and pain anims redraw the bow.
            // Here, in Quake, do not always allow a redraw.
            damage = random();
            if (damage < 0.25)
                archer_painc1 ();
            else if (damage < 0.5)
                archer_painb1 ();
            else
                archer_pain1 ();
            return;
        }
    archer_pain1 ();
    attacker = attacker;    // FrikQcc warning squelch!
};

// DEATH - - - - - - - - - - - - - - - -
void() archer_decap1  =[ $decap1,  archer_decap2  ] {ai_back(3); blood_spray('0 0 27', 1);};
void() archer_decap2  =[ $decap3,  archer_decap3  ] {ai_back(9); blood_spray('-7 -1 26', 2);};
void() archer_decap3  =[ $decap5,  archer_decap4  ] {blood_spray('-4 -1 23', 1); self.solid = SOLID_NOT; DropBackpack(); };
void() archer_decap4  =[ $decap7,  archer_decap5  ] {}; //blood_spray('0 1 16', 1);};
void() archer_decap5  =[ $decap9,  archer_decap6  ] {blood_spray('2 0 8', -1);};
void() archer_decap6  =[ $decap11, archer_decap7  ] {};
void() archer_decap7  =[ $decap13, archer_decap8  ] {};
void() archer_decap8  =[ $decap15, archer_decap9  ] {};
void() archer_decap9  =[ $decap17, archer_decap9  ] {};

void() archer_die1  =[ $deathA2,  archer_die2  ] {ai_back(3);};
void() archer_die2  =[ $deathA4,  archer_die3  ] {ai_back(2);};
void() archer_die3  =[ $deathA6,  archer_die4  ] {ai_back(2); self.solid = SOLID_NOT; DropBackpack(); };
void() archer_die4  =[ $deathA8,  archer_die5  ] {ai_back(4);};
void() archer_die5  =[ $deathA10, archer_die6  ] {ai_back(1);};
void() archer_die6  =[ $deathA12, archer_die7  ] {};
void() archer_die7  =[ $deathA14, archer_die8  ] {};
void() archer_die8  =[ $deathA16, archer_die9  ] {};
void() archer_die9  =[ $deathA18, archer_die10 ] {};
void() archer_die10 =[ $deathA20, archer_die11 ] {
// PM:  Archer death sound split into two files.  Play part 2.
    if (self.flags & FL_ONGROUND && !(archer_lord())) sound (self, CHAN_AUTO, "archer/crash.wav", 1, ATTN_NORM);
};
void() archer_die11 =[ $deathA22, archer_die11  ] {};

void() archer_die = {
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    //if (Shattered ()) return;
    //if (Decapitated ()) { monster_decap ('0 0 27', archer_decap1); return; }

    // Clear weapon sound
    sound (self, CHAN_WEAPON, SOUND_EMPTY, 1, ATTN_NORM);

    // regular death
    if (!self.gibbed) {
        if (archer_lord ())
            sound (self, CHAN_VOICE, "archer/death2.wav", 1, ATTN_NORM);
        else
            sound (self, CHAN_VOICE, "archer/death.wav", 1, ATTN_NORM);
        archer_die1 ();
    }
};

// REVIVE - - - - - - - - - - - - - - -
void() archer_revive1  =[ $deathA20, archer_revive2  ] {};
void() archer_revive2  =[ $deathA18, archer_revive3  ] {};
void() archer_revive3  =[ $deathA16, archer_revive4  ] {};
void() archer_revive4  =[ $deathA14, archer_revive5  ] {};
void() archer_revive5  =[ $deathA12, archer_revive6  ] {};
void() archer_revive6  =[ $deathA10, archer_revive7  ] {};
void() archer_revive7  =[ $deathA8,  archer_revive8  ] {};
void() archer_revive8  =[ $deathA6,  archer_revive9  ] {};
void() archer_revive9  =[ $deathA4,  archer_revive10 ] {};
void() archer_revive10 =[ $deathA2,  archer_run1     ] {};

// OTHER - - - - - - - - - - - - - - - -
void(float hello) archer_sight = {
    if (archer_lord ()) {
        if (hello)
            sound (self, CHAN_VOICE, "archer/growl2.wav", 1, ATTN_NORM);
        else
            sound (self, CHAN_VOICE, "archer/sight2.wav", 1, ATTN_NORM);
    } else {
        if (hello)
            sound (self, CHAN_VOICE, "archer/growl.wav", 1, ATTN_NORM);
        else
            sound (self, CHAN_VOICE, "archer/sight.wav", 1, ATTN_NORM);
    }
};

//============
// --> QUAKED monster_archer (1 0 0) (-16 -16 -24) (16 16 40) Ambush
//============
void() monster_archer = {
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/drake/archer.mdl";
    self.headmdl   = "progs/drake/h_archer.mdl";
    precache_model ("progs/drake/arrow.mdl");     // Replaced 'akarrow.mdl'.
    precache_model ("progs/drake/archer.mdl");
    precache_model ("progs/drake/h_archer.mdl");

    precache_sound ("archer/draw.wav");
    precache_sound ("archer/arrow1.wav");
    precache_sound ("archer/arrow2.wav");
    precache_sound ("archer/arrow3.wav");
    precache_sound ("archer/arrowbrk.wav");
    precache_sound ("archer/arrowg.wav");   // Type 0 and 2 arrows.
    precache_sound ("archer/arrowr.wav");   // Type 1 arrows.
    precache_sound ("archer/death.wav");
    precache_sound ("archer/growl.wav");
    precache_sound ("archer/pain.wav");
    precache_sound ("archer/sight.wav");
    precache_sound ("archer/crash.wav");
    precache_sound ("archer/death2.wav");
    precache_sound ("archer/growl2.wav");
    precache_sound ("archer/growl3.wav");
    precache_sound ("archer/pain2.wav");
    precache_sound ("archer/sight2.wav");
    precache_sound ("archer/explode.wav");

    // Hexen2 archers have 80 health.  (Lords have 325 health.)
    // Note:  Hexen2 archers gibbed at -30 hp.  Since the archers have armor
    // similar to Quake knights, use -40 hp instead.
    if (self.classname == "monster_archer_lord") {
        // Archer Lord.
        self.skin = 1;
        self.ryuflags = self.ryuflags | RYU_ALERT;    // From Dragons.
        if (soul_evil)
            monster_purse (3, 0.75, 0.25);

        if (self.health < 1) self.health = 325;
        self.gibhealth = -40;
        if (self.deathstring == "")  self.deathstring = " was assassinated by an Assassin\n";
    } else {
        // Grunt class.
        self.skin = 0;
        if (soul_evil)
            monster_purse (2, 0.5, 0);

        if (self.health < 1) self.health = 80;
        self.gibhealth = -40;
        if (self.deathstring == "")  self.deathstring = " was skewered by an Archer\n";
    }

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_SHORT;
    
    self.gibbed = FALSE;
    self.pain_flinch = 40;
    self.pain_longanim = TRUE;
    self.steptype = FS_TYPEMEDIUM;
    self.mass       = MASS_MEDIUM;
    self.th_stand = archer_stand1;
    self.th_walk = archer_walk1;
    self.th_run = archer_run1;
    self.th_missile = archer_attack;
    self.th_pain = archer_pain;
    self.th_die = archer_die;
    self.th_sight = archer_sight;
    self.th_checkattack = ArcherCheckAttack;
    self.th_revive  = archer_revive1;
    self.th_spawn   = monster_archer;

    self.attack_state   = AS_FLANKING;
    //if (!soul_evil || (self.skin == 1))
    //{   // Archer/Assassin of Bane.
    //    self.family     = FAM_BANE;
    //    self.xfl        = XFL_ARMORED;  //| XFL_METALHEAD;
    //}

// As SoE enforcer replacements, give them ammo to drop.
    if (soul_evil)
    self.ammo_bolts = 2;
        //monster_guns (0, 0, 20, 0, 0);
    //self.family = self.family | FAM_ARCHER;

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.classtype = CT_MONARCHER;
    self.classgroup = CG_KNIGHT;
    self.classmove = MON_MOVEWALK;

    monster_start ();
};

//============
// --> QUAKED monster_archer_lord (1 0 0) (-16 -16 -24) (16 16 40) Ambush
//============
void() monster_archer_lord = { monster_archer (); };