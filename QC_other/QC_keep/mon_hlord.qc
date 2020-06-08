/*
==============================================================================

KNIGHT

==============================================================================
*/

$cd id1/models/knight2
$origin 0 0 24
$base base
$skin skin

$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8 stand9

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9
$frame walk10 walk11 walk12 walk13 walk14 walk15 walk16 walk17
$frame walk18 walk19 walk20

$frame run1 run2 run3 run4 run5 run6 run7 run8

$frame pain1 pain2 pain3 pain4 pain5

$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12

$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9

$frame char_a1 char_a2 char_a3 char_a4 char_a5 char_a6 char_a7 char_a8
$frame char_a9 char_a10 char_a11 char_a12 char_a13 char_a14 char_a15 char_a16

$frame magica1 magica2 magica3 magica4 magica5 magica6 magica7 magica8
$frame magica9 magica10 magica11 magica12 magica13 magica14

$frame magicb1 magicb2 magicb3 magicb4 magicb5 magicb6 magicb7 magicb8
$frame magicb9 magicb10 magicb11 magicb12 magicb13

$frame char_b1 char_b2 char_b3 char_b4 char_b5 char_b6

$frame slice1 slice2 slice3 slice4 slice5 slice6 slice7 slice8 slice9 slice10

$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7 smash8 smash9 smash10
$frame smash11

$frame w_attack1 w_attack2 w_attack3 w_attack4 w_attack5 w_attack6 w_attack7 
$frame w_attack8 w_attack9 w_attack10 w_attack11 w_attack12 w_attack13 w_attack14
$frame w_attack15 w_attack16 w_attack17 w_attack18 w_attack19 w_attack20 
$frame w_attack21 w_attack22 

$frame magicc1 magicc2 magicc3 magicc4 magicc5 magicc6 magicc7 magicc8
$frame magicc9 magicc10 magicc11

$frame decap1 decap2 decap3 decap4 decap5 decap6 decap7 decap8
$frame decap9 decap10 decap11 decap12

$frame decapb1 decapb2 decapb3 decapb4 decapb5 decapb6 decapb7 decapb8
$frame decapb9


//- - - - - - - - -
float   SKIN_HELL_KNIGHT    = 0;
float   SKIN_HELL_KNIGHT2   = 1;
float   SKIN_HELL_LORD      = 2;
float   SKIN_HKBOSS         = 3;
float   SKIN_BANE           = 4;
float   SKIN_BANE_GRAY      = 5;
float   SKIN_DARK_LORD      = 6;

float   DARKLORD_NEAR       = 250;
//- - - - - - - - -
void() hknight_char_a1;
void() hknight_char_b1;     // PM:  Enabled unused charge anim.
void() hknight_run1;
void() hk_idle_sound;

vector(float offset) hknight_shot_dir =
{
    local   vector  vec;
//    local   vector  vx, vy, vz;

// PM:  The shots tend to choke up if enemy is directly above or below the
// attacking knight.  Could either be a bug or working as intended.
    vec = vectoangles (self.enemy.origin - self.origin);
    vec_y = vec_y + offset * 6;
    
    makevectors (vec);
//    vx = v_forward;  vy = v_right;  vz = v_up;

// set missile direction
    vec = normalize (v_forward);
    vec_z = 0 - vec_z + (random() - 0.5)*0.1;
    vec = Maim_Scatter (vec, '0 0 0');      // Added for invisibility.

//    v_forward = vx;  v_right = vy;  v_up = vz;
    return vec;
};

void(float offset) hknight_shot =
{
    local   vector  org, vec;

    vec = hknight_shot_dir (offset);    // Also calls 'makevectors'.

    if (( self.frame >= $magicb1 ) && ( self.frame <= $magicb13 ))
        org = self.origin + (v_forward * 16) + (v_up * 20);
    else
        org = self.origin + self.mins + self.size*0.5 + v_forward * 20;

    if (self.classname == "monster_bane")
    {
        KnightBall_Launch (org, vec);
        newmis.dmg = 60;
        newmis.style = newmis.style | DF_SAFE;
        newmis.velocity = vec*600;
        if (( self.frame == $magicb6 ) || ( self.frame == $magicc6 ))
            sound (self, CHAN_WEAPON, "weapons/mirv1.wav", 1, ATTN_NORM);
    }
    else if (self.classname == "monster_dark_lord")
    {
        NecroBolt_Launch (org, vec);
    }
    else
    {
        local   float   sped;

        if (cranked)
            sped = 450;     // PM:  Faster missiles.
        else
            sped = 300;
        Knightspike_Launch (org, vec, sped);
        sound (self, CHAN_WEAPON, "hknight/attack1.wav", 1, ATTN_NORM);
    }
};

void() CheckForCharge =
{
// check for mad charge
    if (!enemy_vis)
        return;
    if (time < self.attack_finished)
        return;    
    if ( fabs(self.origin_z - self.enemy.origin_z) > 20)
        return;        // too much height change
    if ( vlen (self.origin - self.enemy.origin) < 80)
        return;        // use regular attack
//- - - - - - - - -
    if (!self.fighting)
        return;     // PM:  Don't charge if not fighting anyone.
//    if (!self.enemy)
//        return;     // PM:  In case world origin is near and in sight.
//- - - - - - - - -

// charge
// PM:  Moved self.attack_finished to the 'knight_char_*1' functions.
    if (self.classname == "monster_hkboss")
    {   // Emulate old behavior.
        self.shots_left = 3;    // Replaced 'currentammo' with 'shots_*'.
        hknight_char_b1 ();
        return;
    }
    if (random() < 0.25)
    {   // char_b* scene has 6 frames.
        self.shots_left = random() * 4;    // Swing from 1 to 4 times.
        self.shots_left = ceil(self.shots_left);
        hknight_char_b1 ();     // PM:  Unused, more damaging attack anim.
    }
    else
        hknight_char_a1 ();
};

// PM:  Removed 'void() CheckContinueCharge'.

//===========================================================================
// STAND

void()  hknight_stand1  =[ $stand1, hknight_stand2 ] {ai_stand();};
void()  hknight_stand2  =[ $stand2, hknight_stand3 ] {ai_stand();};
void()  hknight_stand3  =[ $stand3, hknight_stand4 ] {ai_stand();};
void()  hknight_stand4  =[ $stand4, hknight_stand5 ] {ai_stand();};
void()  hknight_stand5  =[ $stand5, hknight_stand6 ] {ai_stand();};
void()  hknight_stand6  =[ $stand6, hknight_stand7 ] {ai_stand();};
void()  hknight_stand7  =[ $stand7, hknight_stand8 ] {ai_stand();};
void()  hknight_stand8  =[ $stand8, hknight_stand9 ] {ai_stand();};
void()  hknight_stand9  =[ $stand9, hknight_stand1 ] {ai_stand();};

//===========================================================================
// WALK

void() hk_idle_sound =
{
    if (random() < 0.2)
    {
        if (self.classname == "monster_bane")
        {
            sound (self, CHAN_VOICE, "bane/growl.wav", 1, ATTN_NORM);
            return;
        }
        if (self.classname == "monster_dark_lord")  // FIXME:  Find sfx.
            return;
        sound (self, CHAN_VOICE, "hknight/idle.wav", 1, ATTN_NORM);
    }
};

void() hknight_walk1  =[ $walk1,  hknight_walk2  ] {hk_idle_sound(); ai_walk(2);};
void() hknight_walk2  =[ $walk2,  hknight_walk3  ] {M_Step(); ai_walk(5);};
void() hknight_walk3  =[ $walk3,  hknight_walk4  ] {ai_walk(5);};
void() hknight_walk4  =[ $walk4,  hknight_walk5  ] {ai_walk(4);};
void() hknight_walk5  =[ $walk5,  hknight_walk6  ] {ai_walk(4);};
void() hknight_walk6  =[ $walk6,  hknight_walk7  ] {ai_walk(2);};
void() hknight_walk7  =[ $walk7,  hknight_walk8  ] {ai_walk(2);};
void() hknight_walk8  =[ $walk8,  hknight_walk9  ] {ai_walk(3);};
void() hknight_walk9  =[ $walk9,  hknight_walk10 ] {ai_walk(3);};
void() hknight_walk10 =[ $walk10, hknight_walk11 ] {ai_walk(4);};
void() hknight_walk11 =[ $walk11, hknight_walk12 ] {ai_walk(3);};
void() hknight_walk12 =[ $walk12, hknight_walk13 ] {M_Step(); ai_walk(4);};
void() hknight_walk13 =[ $walk13, hknight_walk14 ] {ai_walk(6);};
void() hknight_walk14 =[ $walk14, hknight_walk15 ] {ai_walk(2);};
void() hknight_walk15 =[ $walk15, hknight_walk16 ] {ai_walk(2);};
void() hknight_walk16 =[ $walk16, hknight_walk17 ] {ai_walk(4);};
void() hknight_walk17 =[ $walk17, hknight_walk18 ] {ai_walk(3);};
void() hknight_walk18 =[ $walk18, hknight_walk19 ] {ai_walk(3);};
void() hknight_walk19 =[ $walk19, hknight_walk20 ] {ai_walk(3);};
void() hknight_walk20 =[ $walk20, hknight_walk1  ] {ai_walk(2);};

//===========================================================================
// RUN

// Checks if the Bane can use a nearby dead body.
void() bane_magicd1;
void() bane_magice1;
void(float d) hk_run =
{
    if (self.classname == "monster_bane")
    {
        d = d * 2;

        if (!self.waitmax)
        {
            if (ai_medic (d))
            {
                // Hack:  Set attack_finished higher than time so
                //  that CheckForCharge fails, which would otherwise
                //  bypass ritual anim.
//                if ( self.frame == $run1 )
                    self.attack_finished = time + 0.2;
                bane_magicd1 ();
                return;
            }

            if (self.search_time + 5 < time)
            if (self.enemy)
            if (!visible (self.enemy))
            {   // Teleport to the enemy.
//                if ( self.frame == $run1 )
                    self.attack_finished = time + 0.2;
                self.search_time = time;    //+ 5;
                bane_magice1 ();
                return;
            }
        }
    }
    ai_run (d);
};
void() hknight_run1 =[ $run1, hknight_run2 ] {if (self.classname == "monster_dark_lord") {self.th_run(); return;}hk_idle_sound(); hk_run(20); CheckForCharge();};
void() hknight_run2 =[ $run2, hknight_run3 ] {M_Step(); hk_run(25);};
void() hknight_run3 =[ $run3, hknight_run4 ] {hk_run(18);};
void() hknight_run4 =[ $run4, hknight_run5 ] {hk_run(16);};
void() hknight_run5 =[ $run5, hknight_run6 ] {hk_run(14);};
void() hknight_run6 =[ $run6, hknight_run7 ] {M_Step(); hk_run(25);};
void() hknight_run7 =[ $run7, hknight_run8 ] {hk_run(21);};
void() hknight_run8 =[ $run8, hknight_run1 ] {hk_run(13);};

//============================================================================
// PAIN

// Makes the Bane gray and ghostly.  Called by 'painc' and 'magice' series.
void() bane_gray =
{
    if (self.health <= self.waitmin)
    {   // Set up clone magic.
        self.health     = self.waitmin; // Yes, reset health.
        self.waitmax    =           // If lower than min, then max too.
        self.waitmin    = 0;        // Cannot use clone trick again.
        self.cnt        = skill;    // Number of clones to spawn.
        if (cranked)
            self.cnt        = self.cnt + 1;
    }
    else
        self.cnt        = 0;
    // Now the Bane becomes a phantom as he shadow walks.
    self.touch      = SUB_Null;     // In case of jump attack.
    self.takedamage = DAMAGE_NO;
    self.solid      = SOLID_NOT;
    self.skin       = SKIN_BANE_GRAY;   // Use grayscale Bane.
};

// Taking a hint from the axeman...
// Update:  Removed 'ai_forward(6);' from first frame to stop stack overflow.
void() bane_pain1 =[ $pain1, bane_pain2   ] {};
void() bane_pain2 =[ $pain2, bane_pain3   ] {ai_forward(8);};
void() bane_pain3 =[ $pain4, bane_pain4   ] {ai_forward(4);};
void() bane_pain4 =[ $pain5, hknight_run1 ] {ai_forward(2);};

void() bane_painb1=[ $pain1, bane_pain3   ] {};

// This pain makes the Bane pull the log trick from Naruto,
// only leave behind a fading image of the Bane instead of a log.
void() bane_painc1=[ $pain1, bane_magice9 ]
{
    bane_gray();
    bane_shadow();
    self.nextthink  = time + 0.5;
    self.modelindex = 0;
    EntryPoint_Spawn (self, self.enemy, self.enemy.origin, EntryPoint_Random);
};

void() hknight_pain1 =[ $pain1, hknight_pain2 ] {}; // Sound already done in pain func.
void() hknight_pain2 =[ $pain2, hknight_pain3 ] {};
void() hknight_pain3 =[ $pain3, hknight_pain4 ] {};
void() hknight_pain4 =[ $pain4, hknight_pain5 ] {};
void() hknight_pain5 =[ $pain5, hknight_run1  ] {};

void() hkfast_pain1    =[ $pain1, hknight_pain4 ] {};

//============================================================================
// DEATH

// PM:  Beware -- With ai_forward on the first kill frame, T_Damage may be
// called at least twice on touch kills!
void() hknight_decap1  =[ $decap1,  hknight_decap2  ] {ai_forward(10); blood_spray('-2 0 28', 1);};
void() hknight_decap2  =[ $decap2,  hknight_decap3  ] {ai_forward(8); blood_spray('-3 0 26', 2);};
void() hknight_decap3  =[ $decap3,  hknight_decap4  ] {monster_drop(); ai_forward(7); blood_spray('-3 0 17', 1);};
void() hknight_decap4  =[ $decap4,  hknight_decap5  ] {};
void() hknight_decap5  =[ $decap5,  hknight_decap6  ] {};
void() hknight_decap6  =[ $decap6,  hknight_decap7  ] {};
void() hknight_decap7  =[ $decap7,  hknight_decap8  ] {blood_spray('8 -1 11', -1);};
void() hknight_decap8  =[ $decap8,  hknight_decap9  ] {ai_forward(10);};
void() hknight_decap9  =[ $decap9,  hknight_decap10 ] {ai_forward(11);};
void() hknight_decap10 =[ $decap10, hknight_decap11 ] {};
void() hknight_decap11 =[ $decap11, hknight_decap12 ] {};
void() hknight_decap12 =[ $decap12, decap_end       ] {};

void() hknight_decapb1 =[ $decapb1, hknight_decapb2 ] {blood_spray('-3 0 33', 1);};
void() hknight_decapb2 =[ $decapb2, hknight_decapb3 ] {blood_spray('-12 0 32', 2);};
void() hknight_decapb3 =[ $decapb3, hknight_decapb4 ] {blood_spray('-17 0 23', 1); monster_drop();};
void() hknight_decapb4 =[ $decapb4, hknight_decapb5 ] {blood_spray('-19 0 12', 1);};
void() hknight_decapb5 =[ $decapb5, hknight_decapb6 ] {};
void() hknight_decapb6 =[ $decapb6, hknight_decapb7 ] {};
void() hknight_decapb7 =[ $decapb7, hknight_decapb8 ] {};
void() hknight_decapb8 =[ $decapb8, hknight_decapb9 ] {};
void() hknight_decapb9 =[ $decapb9, decap_end       ] {};

void() hknight_die1  =[ $death1,  hknight_die2  ] {ai_forward(10);};
void() hknight_die2  =[ $death2,  hknight_die3  ] {ai_forward(8);};
void() hknight_die3  =[ $death3,  hknight_die4  ] {monster_drop(); ai_forward(7);};
void() hknight_die4  =[ $death4,  hknight_die5  ] {};
void() hknight_die5  =[ $death5,  hknight_die6  ] {};
void() hknight_die6  =[ $death6,  hknight_die7  ] {};
void() hknight_die7  =[ $death7,  hknight_die8  ] {};
void() hknight_die8  =[ $death8,  hknight_die9  ] {ai_forward(10);};
void() hknight_die9  =[ $death9,  hknight_die10 ] {ai_forward(11);};
void() hknight_die10 =[ $death10, hknight_die11 ] {};
void() hknight_die11 =[ $death11, hknight_die12 ] {};
void() hknight_die12 =[ $death12, monster_end   ] {};

void() hknight_dieb1 =[ $deathb1, hknight_dieb2 ] {};
void() hknight_dieb2 =[ $deathb2, hknight_dieb3 ] {};
void() hknight_dieb3 =[ $deathb3, hknight_dieb4 ] {monster_drop();};
void() hknight_dieb4 =[ $deathb4, hknight_dieb5 ] {};
void() hknight_dieb5 =[ $deathb5, hknight_dieb6 ] {};
void() hknight_dieb6 =[ $deathb6, hknight_dieb7 ] {};
void() hknight_dieb7 =[ $deathb7, hknight_dieb8 ] {};
void() hknight_dieb8 =[ $deathb8, hknight_dieb9 ] {};
void() hknight_dieb9 =[ $deathb9, monster_end   ] {};

float(float dm) hknight_gib =
{
// check for gib
    if (self.health < self.gib_health)
    {
        sound (self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
        ThrowGib ("progs/gib1.mdl", dm);
        ThrowGib ("progs/gib2.mdl", dm);
        ThrowGib ("progs/gib3.mdl", dm);
        ThrowHead ("progs/h_hellkn.mdl", dm);
        return TRUE;
    }
    return FALSE;
};

void() hknight_die =
{
// check for gib
    if (Shattered ())
        return;
    if (self.th_gib)
    {
        if (Decapitated ())
        {
            if (random() < 0.5)
                monster_decap ('-2 0 28', hknight_decap1);
            else
                monster_decap ('-3 0 33', hknight_decapb1);
            return;
        }
        if (hknight_gib (self.health))
            return;
    }

// regular death
    if (self.classname == "monster_hell_lord")
        sound (self, CHAN_VOICE, "archer/death2.wav", 1, ATTN_NORM);
    else if (self.classname == "monster_bane")
    {
        if (self.skin == SKIN_BANE_GRAY)
            self.skin = SKIN_BANE;
        sound (self, CHAN_VOICE, "bane/death.wav", 1, ATTN_NONE);
    }
    else
        sound (self, CHAN_VOICE, "hknight/death1.wav", 1, ATTN_NORM);
    if (random() > 0.5)
        hknight_die1 ();
    else
        hknight_dieb1 ();
};

float   BANE_CHAT       = 1;
float   BANE_RAGE       = 2;
float   BANE_CLONE      = 4;

// Broadcast message to players as if the Bane was a client.
void(string text) Bane_Speaks =
{
    local   entity  pl;

    pl = find(world, classname, "player");
    while (pl)
    {
        sound (pl, CHAN_VOICE, "misc/talk.wav", 1, ATTN_NORM);
        pl = find(pl, classname, "player");
    }
    bprint (text);
};

void() bane_die =
{
// ... You win... for now. (Τθε ΒαξεΊ ®®® Ωου χιξ®®® ζος ξοχ®)
// ... I'll be back! (Τθε ΒαξεΊ ®®® Ιΰμμ βε βαγλ‘)
// ... I will return! (Τθε ΒαξεΊ ®®® Ι χιμμ ςετυςξ‘)
// ... We will meet again! (Τθε ΒαξεΊ ®®® Χε χιμμ νεετ αηαιξ‘)
// Update:  New taunts hinting of nsoe6.
    if (random() < 0.5)     // ... I still have the runes!
        Bane_Speaks ("Τθε ΒαξεΊ ®®® Ι στιμμ θαφε τθε ςυξεσ‘\n");
    else                    // ... You have not won yet!
        Bane_Speaks ("Τθε ΒαξεΊ ®®® Ωου θαφε ξοτ χοξ ωετ‘\n");
    hknight_die ();
};


//============================================================================

// REVIVE - - - - - - - - - - - - - - -
void() hknight_revive1   =[ $death11,   hknight_revive2   ] {};
void() hknight_revive2   =[ $death10,   hknight_revive3   ] {};
void() hknight_revive3   =[ $death9,    hknight_revive4   ] {};
void() hknight_revive4   =[ $death8,    hknight_revive5   ] {};
void() hknight_revive5   =[ $death7,    hknight_revive6   ] {};
void() hknight_revive6   =[ $death6,    hknight_revive7   ] {};
void() hknight_revive7   =[ $death5,    hknight_revive8   ] {};
void() hknight_revive8   =[ $death4,    hknight_revive9   ] {};
void() hknight_revive9   =[ $death3,    hknight_revive10  ] {};
void() hknight_revive10  =[ $death2,    hknight_revive11  ] {};
void() hknight_revive11  =[ $death1,    hknight_revive12  ] {};
void() hknight_revive12  =[ $stand1,    hknight_run1      ] {};

void() hknight_reviveb1  =[ $deathb8,   hknight_reviveb2  ] {};
void() hknight_reviveb2  =[ $deathb7,   hknight_reviveb3  ] {};
void() hknight_reviveb3  =[ $deathb6,   hknight_reviveb4  ] {};
void() hknight_reviveb4  =[ $deathb5,   hknight_reviveb5  ] {};
void() hknight_reviveb5  =[ $deathb4,   hknight_reviveb6  ] {};
void() hknight_reviveb6  =[ $deathb3,   hknight_reviveb7  ] {};
void() hknight_reviveb7  =[ $deathb2,   hknight_reviveb8  ] {};
void() hknight_reviveb8  =[ $deathb1,   hknight_revive12  ] {};

void() hknight_revive =
{
    if ( self.frame == $deathb9 )
        hknight_reviveb1 ();
    else //if ( self.frame == $death10 )
        hknight_revive1 ();
};


//============================================================================
// BANE JUMP

void() Bane_JumpTouch;
void(float d) hk_charge;

// We need these first three jump frame functions in case the Bane
// needs to jump again without selecting other random attacks.
void() bane_jump1   =[ $char_a1,  bane_jump2 ] {hk_charge(20);
    sound (self, CHAN_WEAPON, "hknight/slash1.wav", 1, ATTN_NORM);
};
void() bane_jump2   =[ $char_a2,  bane_jump3 ] {hk_charge(18);};
void() bane_jump3   =[ $char_a3,  bane_jump4 ] {hk_charge(16);};
void() bane_jump4   =[ $char_a4,  bane_jump5 ]
{
    if (self.health <= 0)
        {self.touch = SUB_Null;  self.think = hknight_die1;  return;}

    local   float   g;

    g = cvar("sv_gravity") / 800;

    ai_face();
    // Bane is a bbeg, so don't bother with invisibility shenanigans.
    // Ditto for the dark lord.
    self.touch = Bane_JumpTouch;
    makevectors (self.angles);
    self.velocity = (v_forward * 600) + ('0 0 250' * g);
    self.flags = self.flags - (self.flags & FL_ONGROUND);
};
// Standard demon jumps reach level ground in about 0.5 sec in normal g.
void() bane_jump5   =[ $char_a5,  bane_jump6 ] {self.nextthink = time + 0.5;};
// If three seconds pass, assume stuck then jump again.
void() bane_jump6   =[ $char_a6,  bane_jump1 ] {self.nextthink = time + 3;};
void() bane_jump7   =[ $char_a8,  hknight_char_a10 ] {};

void() Bane_JumpTouch =
{
    local   float   ldmg;

    if (self.health <= 0)
        return;
        
    if (other.takedamage)
    {
        if ( vlen(self.velocity) > 400 )
        {
            ldmg = 40 + 10*random();
            T_Damage (other, self, self, ldmg);    
            if (self.health <= 0)
                return;     // PM:  Killed by thorns (empathy shields).
            if (( self.frame == $char_a4 ) || ( self.frame == $char_a5 ))
            {
                self.frame = $char_a6;  // Go to a stab frame.
                sound (self, CHAN_WEAPON, "weapons/axhit.wav", 1, ATTN_NORM);
            }
        }
    }

    if (!checkbottom(self))
    {
        if (self.flags & FL_ONGROUND)
        {    // jump randomly to not get hung up
//dprint ("popjump\n");
            self.touch = SUB_Null;
            self.think = bane_jump1;
            self.nextthink = time + 0.1;
        }
        return;    // not on ground yet
    }

    self.touch = SUB_Null;
    self.think = bane_jump7;
    self.nextthink = time + 0.1;
};

float() CheckBaneJump =
{
    local   vector  dist;
    local   float   d;

// FIXME:  Check if jump will land on solid ground.

// Elevation check done in CheckForCharge.
//    d = self.enemy.origin_z + self.enemy.mins_z;
//    if (self.origin_z + self.mins_z > d + 0.75 * self.enemy.size_z)
//        return FALSE;       // Target is too low.
//    if (self.origin_z + self.maxs_z < d + 0.25 * self.enemy.size_z)
//        return FALSE;       // Target is too high.

    dist = self.enemy.origin - self.origin;
    dist_z = 0;
    d = vlen(dist);
    if (d < 100)
        return FALSE;       // Target is too close.
//    if (d > 200)
//        if (random() < 0.9)
//            return FALSE;
    if (d > 1000)
        return FALSE;       // Target is too far away.
    return TRUE;
};

/* DISABLED
// 8/9/10:  Jumping anim.
void() hknight_jumptouch =
{
// If it is the Bane attacking, use its attack code instead.
    if (self.enemy)
    if (self.enemy == other)
    if ((self.classname == "monster_bane") || (self.classname == "monster_dark_lord"))
    {
        self.touch = Bane_JumpTouch;
        Bane_JumpTouch ();
        return;
    }

    if (self.health <= 0)
        return;

    self.touch = SUB_Null;
    self.think = hknight_run6;
    self.nextthink = time + 0.1;
};
// velocity and onground assumed to be set by jump trigger.
void() hknight_jump1   =[ $char_a4,  hknight_jump2 ] {self.touch = hknight_jumptouch;};
void() hknight_jump2   =[ $char_a5,  hknight_run6  ] {self.nextthink = time + 3;};
*/

//============================================================================
// BANE AI

void() bane_strafe =
{
    if (self.classname == "monster_bane")
    {
        // Strafing distance was defined by movedist, which could be
        // anything.  Drake has removed movedist, and since this progs
        // is based on Drake, set distance here.

        self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
        ChangeYaw ();
        bane_shadow ();     // Shadow trails, like shadow kick.
        ai_run_strafe (20);     // Player walk speed.
    }
};

void() hk_face = {ai_face();  bane_strafe();};

//============================================================================
// ATTACK #1 (Modified for Lightning Airstrike)

// Here for incidental damage in case enemy runs up to melee range while
// the sword is coming down during the magica animation.
void() magica_sword_chop =
{
    local   float   ldmg, side;
    local   vector  delta;
    
    ai_face ();
    // movement done elsewhere.

    delta = self.enemy.origin - self.origin;

    if (!InMeleeRange (80))     // 100 is too long.
        return;
    if (!CanDamage (self.enemy, self))
        return;
    if (self.classname != "monster_bane")
        if (self.classname != "monster_dark_lord")
            if (IsGhost (self.enemy))
                return;     // Not allowed to hit wraith.

    sound (self, CHAN_WEAPON, "weapons/axhit.wav", 1, ATTN_NORM);
    makevectors (self.angles);
    ldmg = 20 + 10*random();    // Player normally does 20 damage with axe.
    side = crandom() * 50;
    delta = (side * v_right) + (50 * v_up);

    T_NewDamage (self.enemy, self, self, ldmg, 0, "melee");
    SpawnMeatSpray (self.origin + v_forward*16, delta, self.enemy.bloodtype);
};

// Bane summons a thunderstorm.
void() bane_magica1  =[ $magica1,  bane_magica2  ] {bane_storm( $magica1, $magica13 ); hk_face();};
void() bane_magica2  =[ $magica2,  bane_magica3  ] {hk_face();};
void() bane_magica3  =[ $magica3,  bane_magica4  ] {hk_face();};
void() bane_magica4  =[ $magica4,  bane_magica5  ] {hk_face();};
void() bane_magica5  =[ $magica5,  bane_magica6  ] {hk_face();};
void() bane_magica6  =[ $magica6,  bane_magica7  ] {hk_face();};
void() bane_magica7  =[ $magica7,  bane_magica8  ] {hk_face();};
void() bane_magica8  =[ $magica8,  bane_magica9  ] {hk_face();};
void() bane_magica9  =[ $magica9,  bane_magica10 ] {hk_face(); magica_sword_chop();};
void() bane_magica10 =[ $magica10, hknight_magica11 ] {hk_face();};
//void() bane_magica10 =[ $magica10, bane_magica11 ] {hk_face();};
//void() bane_magica11 =[ $magica11, bane_magica12 ] {hk_face();};
//void() bane_magica12 =[ $magica12, bane_magica13 ] {hk_face();};
//void() bane_magica13 =[ $magica13, bane_magica14 ] {hk_face();};
//void() bane_magica14 =[ $magica14, hknight_run1  ] {hk_face();};

void() darklord_magica1  =[ $magica1,  darklord_magica2  ] {hk_face();};
void() darklord_magica2  =[ $magica2,  darklord_magica3  ] {hk_face();};
void() darklord_magica3  =[ $magica3,  darklord_magica4  ] {hk_face();};
void() darklord_magica4  =[ $magica4,  darklord_magica5  ] {hk_face();};
void() darklord_magica5  =[ $magica5,  darklord_magica6  ] {hk_face();
    ReflectorShield (self);
    sound(self, 6, "items/protect.wav", 1, ATTN_NORM);
    self.pain_finished = time + 1;      // Uninterruptable by this point.
};
void() darklord_magica6  =[ $magica6,  darklord_magica7  ] {hk_face();};
void() darklord_magica7  =[ $magica7,  darklord_magica8  ] {hk_face();};
void() darklord_magica8  =[ $magica8,  darklord_magica9  ] {hk_face();};
void() darklord_magica9  =[ $magica9,  darklord_magica10 ] {hk_face(); magica_sword_chop();};
void() darklord_magica10 =[ $magica10, hknight_magica11  ] {hk_face();
    makevectors (self.angles);
    M_Slam (self.origin + v_forward*46);
    self.waitmin = time + 5;
};

// Hell Lords rain meteors down.
void() lord_meteor = {boss_meteor('50 0 100', 300, FALSE, FALSE);};
void() lord_magica7  =[ $magica7,  lord_magica8     ] {hk_face(); lord_meteor();};
void() lord_magica8  =[ $magica8,  lord_magica9     ] {hk_face(); if (skill >= 3) lord_meteor();};
void() lord_magica9  =[ $magica9,  lord_magica10    ] {hk_face(); magica_sword_chop();};
void() lord_magica10 =[ $magica10, hknight_magica11 ] {hk_face();};

// Cranked Hell Knights summon a bolt of lightning as a pillar.
// Should launch a vacuum wave instead like in anime or J-style games,
// but that may not look so great in Quake.  So, use lightning for now.
void() hknight_magica1  =[ $magica1,  hknight_magica2  ] {hk_face();};
void() hknight_magica2  =[ $magica2,  hknight_magica3  ] {hk_face();};
void() hknight_magica3  =[ $magica3,  hknight_magica4  ] {hk_face();};
void() hknight_magica4  =[ $magica4,  hknight_magica5  ] {hk_face();};
void() hknight_magica5  =[ $magica5,  hknight_magica6  ] {hk_face();};
void() hknight_magica6  =[ $magica6,  hknight_magica7  ] {hk_face();
    if (self.classname == "monster_hell_lord")
        self.think = lord_magica7;
};
//void() hknight_magica7  =[ $magica7,  hknight_magica8  ] {hknight_shot(-2);};
//void() hknight_magica8  =[ $magica8,  hknight_magica9  ] {hknight_shot(-1);};
//void() hknight_magica9  =[ $magica9,  hknight_magica10 ] {hknight_shot(0);};
//void() hknight_magica10 =[ $magica10, hknight_magica11 ] {hknight_shot(1);};
//void() hknight_magica11 =[ $magica11, hknight_magica12 ] {hknight_shot(2);};
//void() hknight_magica12 =[ $magica12, hknight_magica13 ] {hknight_shot(3);};
void() hknight_magica7  =[ $magica7,  hknight_magica8  ] {hk_face();};
void() hknight_magica8  =[ $magica8,  hknight_magica9  ] {hk_face();};
void() hknight_magica9  =[ $magica9,  hknight_magica10 ] {hk_face(); magica_sword_chop(); M_Lightwall ();};
void() hknight_magica10 =[ $magica10, hknight_magica11 ] {hk_face();};
void() hknight_magica11 =[ $magica11, hknight_magica12 ] {hk_face();};
void() hknight_magica12 =[ $magica12, hknight_magica13 ] {hk_face();};
void() hknight_magica13 =[ $magica13, hknight_magica14 ] {hk_face();};
void() hknight_magica14 =[ $magica14, hknight_run1     ] {hk_face();};

//============================================================================
// ATTACK #2 (Modified for Fireball)

// Hell Lords launches more magic missiles.  Bane launches more fireballs.
void() lord_magicb6  =[ $magicb6,  lord_magicb7     ] {hknight_shot(0); bane_strafe();};
void() lord_magicb7  =[ $magicb7,  lord_magicb8     ] {hknight_shot(-1); bane_strafe();};
void() lord_magicb8  =[ $magicb8,  lord_magicb9     ] {hknight_shot(0); bane_strafe();};
void() lord_magicb9  =[ $magicb9,  lord_magicb10    ] {hknight_shot(1); bane_strafe();};
void() lord_magicb10 =[ $magicb10, hknight_magicb11 ] {hknight_shot(0); bane_strafe();};

void() darklord_magicb6  =[ $magicb6,  darklord_magicb7  ] {hk_face();};
void() darklord_magicb7  =[ $magicb7,  darklord_magicb8  ] {hk_face();};
void() darklord_magicb8  =[ $magicb8,  darklord_magicb9  ] {hk_face(); hknight_shot(0);
    local   vector  org;

    makevectors (self.angles);
    org = self.origin + (v_forward*16) + (v_right*2) + (v_up*24);
    MakeFancy (org, X32_STAR_RED, 2);
    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "plasma/fire.wav", 1, ATTN_NORM);
//    SUB_AttackFinished (3);
    M_Reload (3);   // Three second delay before next salvo in non-NM.
};
void() darklord_magicb9  =[ $magicb9,  darklord_magicb10 ] {hk_face(); hknight_shot(-1);};
void() darklord_magicb10 =[ $magicb10, hknight_magicb11  ] {hk_face(); hknight_shot(1);};

// Hell Knights launch a single fireball to the ground, like in Quoth.
void() hknight_magicb1  =[ $magicb1,  hknight_magicb2  ] {hk_face();};
void() hknight_magicb2  =[ $magicb2,  hknight_magicb3  ] {hk_face();};
void() hknight_magicb3  =[ $magicb3,  hknight_magicb4  ] {hk_face();};
void() hknight_magicb4  =[ $magicb4,  hknight_magicb5  ] {hk_face();};
void() hknight_magicb5  =[ $magicb5,  hknight_magicb6  ] {hk_face();
if (self.classname == "monster_dark_lord")
    {self.think = darklord_magicb6; return;}
// Update 6/18/2011:  If hell knight uses this, use hell lord missiles.
if (self.classname != "monster_hkboss")
    self.think = lord_magicb6;
};
void() hknight_magicb6  =[ $magicb6,  hknight_magicb7  ] {hk_face();};
//void() hknight_magicb7  =[ $magicb7,  hknight_magicb8  ] {hknight_shot(-2);};
//void() hknight_magicb8  =[ $magicb8,  hknight_magicb9  ] {hknight_shot(-1);};
//void() hknight_magicb9  =[ $magicb9,  hknight_magicb10 ] {hknight_shot(0);};
//void() hknight_magicb10 =[ $magicb10, hknight_magicb11 ] {hknight_shot(1);};
//void() hknight_magicb11 =[ $magicb11, hknight_magicb12 ] {hknight_shot(2);};
//void() hknight_magicb12 =[ $magicb12, hknight_magicb13 ] {hknight_shot(3);};
void() hknight_magicb7  =[ $magicb7,  hknight_magicb8  ] {hk_face();};
void() hknight_magicb8  =[ $magicb8,  hknight_magicb9  ] {hk_face();};
void() hknight_magicb9  =[ $magicb9,  hknight_magicb10 ] {hk_face();
//    if (self.classname == "monster_hkboss")
        M_Shal30 ('12 2 24');   // Vore bomb as in Rapture (and others).
//    else
//        M_FireKnight ('12 2 24', TRUE);
};
void() hknight_magicb10 =[ $magicb10, hknight_magicb11 ] {hk_face();};
void() hknight_magicb11 =[ $magicb11, hknight_magicb12 ] {hk_face();};
void() hknight_magicb12 =[ $magicb12, hknight_magicb13 ] {hk_face();};
void() hknight_magicb13 =[ $magicb13, hknight_run1     ] {hk_face();
if (self.classname == "monster_dark_lord")
    SUB_CheckRefire (hknight_magicb1);
};

//============================================================================
// ATTACK #3 (Standard)

void() hknight_magicc1  =[ $magicc1,  hknight_magicc2  ] {hk_face();};
void() hknight_magicc2  =[ $magicc2,  hknight_magicc3  ] {hk_face();};
void() hknight_magicc3  =[ $magicc3,  hknight_magicc4  ] {hk_face();};
void() hknight_magicc4  =[ $magicc4,  hknight_magicc5  ] {hk_face();};
void() hknight_magicc5  =[ $magicc5,  hknight_magicc6  ] {hk_face();};
void() hknight_magicc6  =[ $magicc6,  hknight_magicc7  ] {hknight_shot(-2); bane_strafe();};
void() hknight_magicc7  =[ $magicc7,  hknight_magicc8  ] {hknight_shot(-1); bane_strafe();};
void() hknight_magicc8  =[ $magicc8,  hknight_magicc9  ] {hknight_shot(0); bane_strafe();};
void() hknight_magicc9  =[ $magicc9,  hknight_magicc10 ] {hknight_shot(1); bane_strafe();};
void() hknight_magicc10 =[ $magicc10, hknight_magicc11 ] {hknight_shot(2); bane_strafe();};
void() hknight_magicc11 =[ $magicc11, hknight_run1     ] {hknight_shot(3); bane_strafe();};

//============================================================================
// ATTACK #4 (Firewall -- Used by Hell Lords)
void() hknight_magicd1  =[ $magica1,  hknight_magicd2  ] {hk_face();};
void() hknight_magicd2  =[ $magica14, hknight_magicd3  ] {hk_face();};
void() hknight_magicd3  =[ $magica13, hknight_magicd4  ] {hk_face();};
void() hknight_magicd4  =[ $magica12, hknight_magicd5  ] {hk_face();};
void() hknight_magicd5  =[ $magica11, hknight_magicd6  ] {hk_face(); sound (self, CHAN_WEAPON, "weapons/fwalltap.wav", 1, ATTN_NORM);};
void() hknight_magicd6  =[ $magica10, hknight_magicd7  ] {hk_face();};
void() hknight_magicd7  =[ $magica9,  hknight_magicd8  ] {hk_face(); M_Firewall();};
void() hknight_magicd8  =[ $magica8,  hknight_magicd9  ] {hk_face();};
void() hknight_magicd9  =[ $magica4,  hknight_magicd10 ] {hk_face();};
void() hknight_magicd10 =[ $magica3,  hknight_magicd11 ] {hk_face();};
void() hknight_magicd11 =[ $magica2,  hknight_magicd12 ] {hk_face();};
void() hknight_magicd12 =[ $magica1,  hknight_run1     ] {hk_face();};

//============================================================================
// ATTACK #5 (Wrath Summoning -- Used by Dark Lords)

void() monster_wrath;
void() wrath_grow1;

// FIXME:  Wrath summoning uses old code that needs updating.
void() dk_shield;
void() darklord_magicd1  =[ $magica1,  darklord_magicd2  ] {hk_face();};
void() darklord_magicd2  =[ $magica14, darklord_magicd3  ] {hk_face();};
void() darklord_magicd3  =[ $magica13, darklord_magicd4  ] {hk_face();};
void() darklord_magicd4  =[ $magica12, darklord_magicd5  ] {hk_face();};
void() darklord_magicd5  =[ $magica11, darklord_magicd6  ] {hk_face();};
void() darklord_magicd6  =[ $magica10, darklord_magicd7  ] {hk_face();};
void() darklord_magicd7  =[ $magica9,  hknight_magicd8   ] {hk_face(); SpawnMonster_Horde(ID_MP2_WRATH, 3); dk_shield();};

//============================================================================
// ATTACK #6 (Monster Resurrection -- Bane exclusive)

void() bane_magicd1  =[ $magica1,  bane_magicd2  ] {ai_ritual();};
void() bane_magicd2  =[ $magica14, bane_magicd3  ] {ai_ritual();};
void() bane_magicd3  =[ $magica13, bane_magicd4  ] {ai_ritual();};
void() bane_magicd4  =[ $magica12, bane_magicd5  ] {ai_ritual();};
void() bane_magicd5  =[ $magica11, bane_magicd6  ] {ai_ritual();};
void() bane_magicd6  =[ $magica10, bane_magicd7  ] {ai_ritual();};
void() bane_magicd7  =[ $magica9,  bane_magicd8  ] {ai_ritual();};
void() bane_magicd8  =[ $magica8,  bane_magicd12 ] {ai_ritual(); self.fallen = world;};
//void() bane_magicd9  =[ $magica7,  bane_magicd10  ] {};
//void() bane_magicd10 =[ $magica6,  bane_magicd11 ] {};
//void() bane_magicd11 =[ $magica5,  bane_magicd12 ] {};
void() bane_magicd12 =[ $magica4,  bane_magicd13 ] {};
void() bane_magicd13 =[ $magica3,  bane_magicd14 ] {};
void() bane_magicd14 =[ $magica2,  bane_magicd15 ] {};
void() bane_magicd15 =[ $magica1,  hknight_run1  ] {};

//============================================================================
// ATTACK #7 (Teleportation and Cloning -- also Bane exclusive)

void() bane_magice1  =[ $magicb1,  bane_magice2  ] {bane_gray(); hk_face();};
void() bane_magice2  =[ $magicb2,  bane_magice3  ] {hk_face();};
void() bane_magice3  =[ $magicb3,  bane_magice4  ] {hk_face();};
void() bane_magice4  =[ $magicb4,  bane_magice5  ] {hk_face();};
void() bane_magice5  =[ $magicb5,  bane_magice6  ] {hk_face();};
void() bane_magice6  =[ $magicb6,  bane_magice7  ] {hk_face();};
void() bane_magice7  =[ $magicb7,  bane_magice8  ] {hk_face();};
void() bane_magice8  =[ $magicb8,  bane_magice9  ] {hk_face();
// Disappear then teleport.
    self.nextthink  = time + 1;
    self.modelindex = 0;
    if (self.cnt > 0)
        EntryPoint_Spawn (self, self, self.origin, EntryPoint_Random);
    else
    {   if (!EntryPoint_Spawn (self, self.enemy, self.enemy.origin, EntryPoint_LongRandom))
            EntryPoint_Spawn (self, self, self.origin, EntryPoint_Random);
    }
};
void() bane_magice9  =[ $magicb8,  bane_magice10 ] {self.alpha = 0.2;
    self.modelindex = self.mindex2;
    hk_face();
    if (self.cnt > 0)
    {
        self.cnt = self.cnt - 1;    // Do before cloning.
        // Don't let the clones spawn more clones.
        if (self.master.classname != self.classname)
            clone_spawn();      // Only the master can clone himself.
    }
};
void() bane_magice10 =[ $magicb8,  bane_magice11 ] {self.alpha = 0.4; hk_face();};
void() bane_magice11 =[ $magicb9,  bane_magice12 ] {self.alpha = 0.6; hk_face();};
void() bane_magice12 =[ $magicb10, bane_magice13 ] {self.alpha = 0.8; hk_face();};
void() bane_magice13 =[ $magicb11, bane_magice14 ] {self.alpha = 0; hk_face();
    if (self.cnt < 1)
    {   if (walkmove (0, 0))
        {
            self.takedamage = DAMAGE_AIM;
            self.solid      = SOLID_SLIDEBOX;
            self.skin       = SKIN_BANE;
        // Last minute feature:  The Bane speaks!
            if (!self.master)
            if (self.height)
            {
                if (self.height & BANE_CLONE)
                {
                    if (random() < 0.5)     // Die, maggot!
                        Bane_Speaks ("Τθε ΒαξεΊ Διε¬ ναηηοτ‘\n");
                    else                    // Now die!
                        Bane_Speaks ("Τθε ΒαξεΊ Ξοχ διε‘\n");
                }
                else if (self.height & BANE_RAGE)
                {
                    if (random() < 0.5)     // Behold my power!
                        Bane_Speaks ("Τθε ΒαξεΊ Βεθομδ¬ νω ποχες‘\n");
                    else                    // Feel my wrath!
                        Bane_Speaks ("Τθε ΒαξεΊ Ζεεμ νω χςατθ‘\n");
                }
                self.height = BANE_CHAT;
            }
            return;
        }
    }
// Either not done cloning, or unable to move at current location.
// Either way, use dimensional travel magic again.
    self.think = bane_magice8;
};
void() bane_magice14 =[ $magicb12, bane_magice15 ] {hk_face();};
void() bane_magice15 =[ $magicb13, hknight_run1  ] {hk_face();};

//===========================================================================

void() hknight_magic =
{
//    if (cranked && (random() < 0.25))
//        hknight_magica1 ();
//    else
        hknight_magicc1 ();
};

void() lord_magic =
{
    local   float   r;
    local   float   add, opt;

    add = 1;
    opt = 2;

    if (M_FirewallCheck ())     // In fire.qc.
        opt = opt + 1;
    // Using 50 and 100 as sides of a right triangle, hypotenuse is 111.8.
    if (M_MeteorCheck ('50 0 112'))     // In boss.qc.  Was '50 -50 100'.
        {add = 0;  opt = opt + 1;}

    r = random() * opt + add;
    if (r <= 1)
        hknight_magica1 ();     // Meteor
    else if (r <= 2)
        hknight_magicb1 ();     // Magic missile straight
    else if (r <= 3)
        hknight_magicc1 ();     // Magic missile spread
    else
        hknight_magicd1 ();     // Firewall
};

// FIXME:  Is this done aside from missing corpse scan/use?
void() darklord_magic =
{
// Todo:
// If close, use slam, charge, or melee.
// else, shoot or summon.
    local   float   dist;

// FIXME:  Check for corpses to make wraths from.
//    if (self.pets < skill)
//        if (random() < 0.1)
//    if (random() < (skill - self.pets) * 0.1)
    if (self.spawnflags & 2)
        if (self.pets < 1)
            if (self.waitmax < time)
                {darklord_magicd1 ();  return;}

// Attacking...
    dist = vlen(self.enemy.origin - self.origin);
    if (dist <= DARKLORD_NEAR)
    {
        if (self.waitmin < time)
            {darklord_magica1();  return;}  // Thor's hand.
        CheckForCharge ();
        if (( self.frame == $char_a1 ) || ( self.frame == $char_b1 ))
            return;
    }
    hknight_magicb1 ();     // Magic missile straight
};

void() hkboss_magic =
{
// Pattern:  1 pillar, 2 bombs, 1 spray.
// Old behavior should be magica->magicb->magicc, but it seemed to favor B?
    local   float   r;

    r = AddOne() & 3;
    if (!r)
        hknight_magica1 ();
    else if (r != 3)
        hknight_magicb1 ();
    else
        hknight_magicc1 ();
};

void() bane_magic =
{
// PM:  Jumping done at charge anim.  However...
// If enemy is low enough that CheckForCharge would abort, then we can
// check for jumping.  We want the Bane to occasionally jump down toward
// the enemy, much like old SoE.
// Update:  If enemy is above, try to teleport to higher ground occasionally.
    if (random() < 0.25)
        if (CheckBaneJump ())
        {
            if (self.origin_z - self.enemy.origin_z > 20)
                {bane_jump1 ();  return;}
            if (!self.waitmax)  // Only if using new AI.
                if (self.enemy.origin_z - self.origin_z > 20)
                    {bane_magice1 ();  return;}
        }

// Check for thunderstorm.
    if (!self.waitmax)
        if (skill >= 1)
            if (!bane_has_storm ())
                if (random() < 0.5)
                    if (UpIsSky (self.enemy.origin))
                        {bane_magica1 ();  return;}   // Start a thunderstorm!

// Spray some fireballs.
    if (random() <= 0.5)
        hknight_magicb1 ();
    else
        hknight_magicc1 ();
};

//===========================================================================

void(float side) bane_melee =
{
    if (self.classname != "monster_bane")
    if (self.classname != "monster_dark_lord")
        return;

    local   float   ldmg;
    local   vector  delta;
    
    ai_face ();
    // movement done elsewhere.

    delta = self.enemy.origin - self.origin;

    if (!InMeleeRange (80))     // 100 is too long.
        return;
    if (!CanDamage (self.enemy, self))
        return;
    // Let the Bane hit ghosts.  He's just that badass.
    // Update:  Dark Lord is Bane-Lite, at least in human form...

    sound (self, CHAN_WEAPON, "bane/hit.wav", 1, ATTN_NORM);
    makevectors (self.angles);
    ldmg = 10 + 5*random();
    if (side)
        delta = side * v_right;
    else
    {
        side = crandom() * 50;
        delta = (side * v_right) + (50 * v_up);
    }

    T_NewDamage (self.enemy, self, self, ldmg, DF_SCAR, "vorpal");
    SpawnMeatSpray (self.origin + v_forward*16, delta, self.enemy.bloodtype);
};

void(float d) hk_charge =
{
    if ((self.classname == "monster_bane") || (self.classname == "monster_dark_lord"))
    {
        local   float   dist;

        dist = d * 2;
        if (!QC_CloseEnough (dist))
            d = dist;   // Move double speed.
    }
    ai_charge (d);
};
// PM:  Added sounds like in my old Cranked mod.
void() hknight_char_a1  =[ $char_a1,  hknight_char_a2  ]
{
// PM:  Don't use 'SUB_AttackFinished' because it won't be set on Nightmare,
// which results in a charge lock for the knight.  In other words, don't let
// him forget his magic.
    self.attack_finished = time + 2;    //SUB_AttackFinished (2);
    hk_charge(20);
    sound (self, CHAN_WEAPON, "hknight/slash1.wav", 1, ATTN_NORM);
};
void() hknight_char_a2  =[ $char_a2,  hknight_char_a3  ] {M_Step(); hk_charge(25);};
void() hknight_char_a3  =[ $char_a3,  hknight_char_a4  ] {hk_charge(18);};
void() hknight_char_a4  =[ $char_a4,  hknight_char_a5  ]
{
// The bane may jump instead.  Now is the best time to check.
// Update 3/27/09:  Allow dark lord the priviledge too.
    if ((self.classname == "monster_bane") || (self.classname == "monster_dark_lord"))
        if (CheckBaneJump ())
            if (random() < 0.5)
            {
                bane_jump4 ();
                return;
            }
    hk_charge(16);
};
void() hknight_char_a5  =[ $char_a5,  hknight_char_a6  ] {hk_charge(14);};
void() hknight_char_a6  =[ $char_a6,  hknight_char_a7  ] {M_Step(); hk_charge(20); bane_melee(0); ai_melee();};
void() hknight_char_a7  =[ $char_a7,  hknight_char_a8  ] {hk_charge(21); ai_melee();};
void() hknight_char_a8  =[ $char_a8,  hknight_char_a9  ] {hk_charge(13); ai_melee();};
void() hknight_char_a9  =[ $char_a9,  hknight_char_a10 ] {hk_charge(20); ai_melee();};
void() hknight_char_a10 =[ $char_a10, hknight_char_a11 ] {M_Step(); hk_charge(20); ai_melee();};
void() hknight_char_a11 =[ $char_a11, hknight_char_a12 ] {hk_charge(18); ai_melee();};
void() hknight_char_a12 =[ $char_a12, hknight_char_a13 ] {hk_charge(16);
    sound (self, CHAN_WEAPON, "knight/sword2.wav", 1, ATTN_NORM);
    bane_melee(200);
};
void() hknight_char_a13 =[ $char_a13, hknight_char_a14 ] {hk_charge(14);};
void() hknight_char_a14 =[ $char_a14, hknight_char_a15 ] {M_Step(); hk_charge(25);};
void() hknight_char_a15 =[ $char_a15, hknight_char_a16 ] {hk_charge(21);};
void() hknight_char_a16 =[ $char_a16, hknight_run1     ] {hk_charge(13);};

//===========================================================================

void() hknight_char_b1 =[ $char_b1, hknight_char_b2 ]
{
    self.attack_finished = time + 1;    // PM:  Don't use SUB_AttackFinished.
    knight_sword_sound(); M_Step(); hk_charge(23); bane_melee(0); ai_melee();
};
void() hknight_char_b2 =[ $char_b2, hknight_char_b3 ] {hk_charge(17); ai_melee();};
void() hknight_char_b3 =[ $char_b3, hknight_char_b4 ] {hk_charge(12); ai_melee();};
void() hknight_char_b4 =[ $char_b4, hknight_char_b5 ] {M_Step(); hk_charge(22); ai_melee();};
void() hknight_char_b5 =[ $char_b5, hknight_char_b6 ] {hk_charge(18); ai_melee();};
void() hknight_char_b6 =[ $char_b6, hknight_run1    ] {hk_charge(8); ai_melee();
    if (vlen (self.origin - self.enemy.origin) < 80)
        self.shots_left = 0;   // Close enough for melee.
    else
    {
        self.shots_left = self.shots_left - 1;
        if (self.shots_left > 0)
            self.think = hknight_char_b1;
    }
};

//===========================================================================

void() bane_slice1  =[ $slice1,  bane_slice2  ] {hk_charge(9);};
void() bane_slice2  =[ $slice3,  bane_slice3  ] {M_Step(); hk_charge(19);};
void() bane_slice3  =[ $slice5,  bane_slice4  ] {hk_charge(11); bane_melee(-200); ai_melee();};
void() bane_slice4  =[ $slice6,  bane_slice5  ] {hk_charge(15); ai_melee();};
void() bane_slice5  =[ $slice7,  bane_slice6  ] {M_Step(); hk_charge(8); ai_melee();};
void() bane_slice6  =[ $slice9,  bane_slice7  ] {hk_charge(2); ai_melee();};
void() bane_slice7  =[ $slice10, hknight_run1 ] {hk_charge(3);};

void() hknight_slice1  =[ $slice1,  hknight_slice2  ] {hk_charge(9);};
void() hknight_slice2  =[ $slice2,  hknight_slice3  ] {hk_charge(6);};
void() hknight_slice3  =[ $slice3,  hknight_slice4  ] {M_Step(); hk_charge(13);};
void() hknight_slice4  =[ $slice4,  hknight_slice5  ] {hk_charge(4);};
void() hknight_slice5  =[ $slice5,  hknight_slice6  ] {hk_charge(7); bane_melee(-200); ai_melee();};
void() hknight_slice6  =[ $slice6,  hknight_slice7  ] {hk_charge(15); ai_melee();};
void() hknight_slice7  =[ $slice7,  hknight_slice8  ] {M_Step(); hk_charge(8); ai_melee();};
void() hknight_slice8  =[ $slice8,  hknight_slice9  ] {hk_charge(2); ai_melee();};
void() hknight_slice9  =[ $slice9,  hknight_slice10 ] {ai_melee();};
void() hknight_slice10 =[ $slice10, hknight_run1    ] {hk_charge(3);};

//===========================================================================

void() bane_smash1  =[ $smash2,  bane_smash2  ] {hk_charge(14);};
void() bane_smash2  =[ $smash4,  bane_smash3  ] {M_Step(); hk_charge(20);};
void() bane_smash3  =[ $smash5,  bane_smash4  ] {hk_charge(10); bane_melee(0); ai_melee();};
void() bane_smash4  =[ $smash6,  bane_smash5  ] {hk_charge(7); ai_melee();};
void() bane_smash5  =[ $smash7,  bane_smash6  ] {hk_charge(12); ai_melee();};
void() bane_smash6  =[ $smash8,  bane_smash7  ] {hk_charge(2); ai_melee();};
void() bane_smash7  =[ $smash9,  bane_smash8  ] {M_Step(); hk_charge(3); ai_melee();};
void() bane_smash8  =[ $smash11, hknight_run1 ] {hk_charge(0);};

void() hknight_smash1  =[ $smash1,  hknight_smash2  ] {hk_charge(1);};
void() hknight_smash2  =[ $smash2,  hknight_smash3  ] {hk_charge(13);};
void() hknight_smash3  =[ $smash3,  hknight_smash4  ] {hk_charge(9);};
void() hknight_smash4  =[ $smash4,  hknight_smash5  ] {M_Step(); hk_charge(11);};
void() hknight_smash5  =[ $smash5,  hknight_smash6  ] {hk_charge(10); bane_melee(0); ai_melee();};
void() hknight_smash6  =[ $smash6,  hknight_smash7  ] {hk_charge(7); ai_melee();};
void() hknight_smash7  =[ $smash7,  hknight_smash8  ] {hk_charge(12); ai_melee();};
void() hknight_smash8  =[ $smash8,  hknight_smash9  ] {hk_charge(2); ai_melee();};
void() hknight_smash9  =[ $smash9,  hknight_smash10 ] {M_Step(); hk_charge(3); ai_melee();};
void() hknight_smash10 =[ $smash10, hknight_smash11 ] {hk_charge(0);};  // Skipped in Neh.
void() hknight_smash11 =[ $smash11, hknight_run1    ] {hk_charge(0);};

//============================================================================

void() bane_watk1  =[ $w_attack1,  bane_watk2  ] {hk_charge(2);};
void() bane_watk2  =[ $w_attack5,  bane_watk3  ] {bane_melee(0); ai_melee();};
void() bane_watk3  =[ $w_attack7,  bane_watk4  ] {hk_charge(1);};
void() bane_watk4  =[ $w_attack8,  bane_watk5  ] {hk_charge(4);};
void() bane_watk5  =[ $w_attack9,  bane_watk6  ] {hk_charge(5);};
void() bane_watk6  =[ $w_attack10, bane_watk7  ] {hk_charge(3); ai_melee();};
void() bane_watk7  =[ $w_attack11, bane_watk8  ] {M_Step(); hk_charge(2); ai_melee();};
void() bane_watk8  =[ $w_attack13, bane_watk9  ] {hk_charge(2); ai_melee();};
void() bane_watk9  =[ $w_attack14, bane_watk10 ] {hk_charge(0);};
void() bane_watk10 =[ $w_attack16, bane_watk11 ] {hk_charge(1);
    sound (self, CHAN_WEAPON, "knight/sword2.wav", 1, ATTN_NORM);
};
void() bane_watk11 =[ $w_attack18, bane_watk12 ] {hk_charge(4); bane_melee(200); ai_melee();};
void() bane_watk12 =[ $w_attack19, bane_watk13 ] {hk_charge(4); ai_melee();};
void() bane_watk13 =[ $w_attack20, bane_watk14 ] {hk_charge(6);};
void() bane_watk14 =[ $w_attack22, hknight_run1] {hk_charge(10);};

// Nehahra skipped every even frame in the scene.
void() hknight_watk1  =[ $w_attack1,  hknight_watk2  ] {hk_charge(2);};
void() hknight_watk2  =[ $w_attack2,  hknight_watk3  ] {hk_charge(0);};
void() hknight_watk3  =[ $w_attack3,  hknight_watk4  ] {hk_charge(0);};
void() hknight_watk4  =[ $w_attack4,  hknight_watk5  ] {ai_melee();};
void() hknight_watk5  =[ $w_attack5,  hknight_watk6  ] {ai_melee();};
void() hknight_watk6  =[ $w_attack6,  hknight_watk7  ] {ai_melee();};
void() hknight_watk7  =[ $w_attack7,  hknight_watk8  ] {hk_charge(1);};
void() hknight_watk8  =[ $w_attack8,  hknight_watk9  ] {hk_charge(4);};
void() hknight_watk9  =[ $w_attack9,  hknight_watk10 ] {hk_charge(5);};
void() hknight_watk10 =[ $w_attack10, hknight_watk11 ] {hk_charge(3); bane_melee(0); ai_melee();};
void() hknight_watk11 =[ $w_attack11, hknight_watk12 ] {M_Step(); hk_charge(2); ai_melee();};
void() hknight_watk12 =[ $w_attack12, hknight_watk13 ] {hk_charge(2); ai_melee();};
void() hknight_watk13 =[ $w_attack13, hknight_watk14 ] {hk_charge(0);};
void() hknight_watk14 =[ $w_attack14, hknight_watk15 ] {hk_charge(0);};
void() hknight_watk15 =[ $w_attack15, hknight_watk16 ] {hk_charge(0);
    // PM:  Something from my old Cranked mod.
    sound (self, CHAN_WEAPON, "knight/sword2.wav", 1, ATTN_NORM);
};
void() hknight_watk16 =[ $w_attack16, hknight_watk17 ] {hk_charge(1);};
void() hknight_watk17 =[ $w_attack17, hknight_watk18 ] {hk_charge(1); bane_melee(200); ai_melee();};
void() hknight_watk18 =[ $w_attack18, hknight_watk19 ] {hk_charge(3); ai_melee();};
void() hknight_watk19 =[ $w_attack19, hknight_watk20 ] {hk_charge(4); ai_melee();};
void() hknight_watk20 =[ $w_attack20, hknight_watk21 ] {hk_charge(6);};
void() hknight_watk21 =[ $w_attack21, hknight_watk22 ] {hk_charge(7);};
void() hknight_watk22 =[ $w_attack22, hknight_run1   ] {hk_charge(3);};

//============================================================================

void(entity attacker, float damage) hknight_pain =
{
    if (self.pain_finished > time)
        return;

    if (self.classname == "monster_hell_lord")
        sound (self, CHAN_VOICE, "archer/pain2.wav", 1, ATTN_NORM);
    else if (self.classname == "monster_dark_lord")
        sound (self, CHAN_VOICE, "darklord/pain.wav", 1, ATTN_NORM);
    else
        sound (self, CHAN_VOICE, "hknight/pain1.wav", 1, ATTN_NORM);
    self.pain_finished = time + 1;

    // Removed mandatory pain anim after five seconds.
    
    if (random()*30 > damage)
        return;        // didn't flinch
    if (SUB_IgnorePain ())
        return;         // No pain anims in Nightmare.

    if (self.classname == "monster_dark_lord")
    {
        if (self.lip > time)
            return;     // Too soon to flinch.
        self.lip = time + 3;    // Give him time to use magic.

        self.waitmax = 0;       // Raise shield again ASAP!

        if (!self.mirror_finished)
            self.effects = self.effects - (self.effects & EF_BRIGHTFIELD);
    }

// Update:  Quicker pain for lords.
    if ((self.classname == "monster_hell_lord") || (self.classname == "monster_dark_lord"))
        if (random() < 0.5)
            {hkfast_pain1 ();  return;}

    hknight_pain1 ();
    attacker = attacker;    // FrikQcc warning squelch!
};

void(entity attacker, float damage) bane_pain =
{
// Shadow clone magic check.
    if (!self.waitmax)
    {
        if (self.health <= self.waitmin)  // Does Bane have the ace in the hole?
        if ((self.enemy == attacker) || (self.oldenemy == attacker))    // Is Bane fighting his enemy?
        if (self.touch != Bane_JumpTouch)   // Is Bane not locked in motion?
        if (self.classname != self.master.classname)    // Is Bane real?
        if (walkmove (0, 0))            // Can the Bane move?
        {   //"Bushin no Kage!\n"
            if (self.height)
                self.height = self.height | BANE_CLONE;
            sound (self, CHAN_VOICE, "bane/pain.wav", 1, ATTN_NORM);
            // Cloning will take a while, so no need to add to pain_finished.
            bane_painc1();
            return;
        }
    }
    else
    {
        if (self.health <= self.waitmax)
        if (self.touch != Bane_JumpTouch)   // Is Bane not locked in motion?
        if (self.classname != self.master.classname)    // Is Bane real?
        if (walkmove (0, 0))            // Can the Bane move?
        {   // The kid gloves come off.
            self.health = self.waitmax;
            self.waitmax = 0;
            if (self.height)
                self.height = self.height | BANE_RAGE;
            sound (self, CHAN_VOICE, "bane/pain.wav", 1, ATTN_NORM);
            bane_painc1();
            return;
        }
    }
//- - - - - - - - -

// Normal pain code.
    if (self.pain_finished > time)
        return;

    sound (self, CHAN_VOICE, "bane/pain.wav", 1, ATTN_NORM);
    self.pain_finished = time + 1;

    if (self.touch == Bane_JumpTouch)
        return;         // Don't interrupt attack anim.
//- - - - - - - - -
// Teleport check.
    if (!self.waitmax)
    if (self.enemy)
    if (walkmove (0, 0))
    {
    // If caught in deadly environment, warp out!
        if (self.h2otype <= CONTENT_SLIME)
            if (!attacker)  // Attacker is world when damaged by slime/lava.
                {bane_painc1 ();  return;}
    // Warp occasionally.
        if (random() < 0.1)
            {bane_painc1 ();  return;}
    }
//- - - - - - - - -
    if (random()*30 > damage)
        return;        // didn't flinch
    if (SUB_IgnorePain ())
        return;         // No pain anims in Nightmare.

    if (random() < 0.5)
        bane_pain1 ();
    else
        bane_painb1 ();
};

// Update 1/7/10:  Moved hknight_type to globals.qc.

void() hknight_melee =
{
    local   float   fast;

    hknight_type = hknight_type + 1;

    sound (self, CHAN_WEAPON, "hknight/slash1.wav", 1, ATTN_NORM);
    fast = 1;
    if (!SUB_QuickAttack ())
        if (self.classname == "monster_hell_knight")
            fast = 0;
    if (fast)
    {
        if (hknight_type == 1)
            bane_slice1 ();
        else if (hknight_type == 2)
            bane_smash1 ();
        else if (hknight_type == 3)
        {
            bane_watk1 ();
            hknight_type = 0;
        }
        return;
    }

    if (hknight_type == 1)
        hknight_slice1 ();
    else if (hknight_type == 2)
        hknight_smash1 ();
    else if (hknight_type == 3)
    {
        hknight_watk1 ();
        hknight_type = 0;
    }
};

//==============================================================================

void() bane_win =
{
// Update:  Changed from ATTN_NORM to ATTN_NONE for level-wide low laugh.
    sound (self, CHAN_VOICE, "bane/laugh.wav", 1, ATTN_NONE);
// DISABLED -- Player can't really listen if he's dead.
//    if (self.height)
//        if (!self.master)     // Death is only the beginning.
//            bprint ("Τθε ΒαξεΊ Δεατθ ισ οξμω τθε βεηιξξιξη®\n");

    monster_sow ();
};

void() bane_void =
{
    monster_void ();
    sound (self, CHAN_VOICE, "bane/death.wav", 1, ATTN_NONE);
};

void(float hello) bane_sight =
{
    if (!hello)
        sound (self, CHAN_VOICE, "bane/sight.wav", 1, 0.5);
};
void(float hello) bane_hello =
{
    if (self.enemy.classname == "player")
    {
        if (world.model == "maps/soe1m5.bsp")
        {   // Thou dare challenge me?!
            Bane_Speaks ("Τθε ΒαξεΊ Τθου δαςε γθαμμεξηε νεΏ‘\n");
        }
        else
        {
            if (random() < 0.5)     // Vengeance shall be mine!
                Bane_Speaks ("Τθε ΒαξεΊ Φεξηεαξγε σθαμμ βε νιξε‘\n");
            else                    // Welcome to thy doom!
                Bane_Speaks ("Τθε ΒαξεΊ Χεμγονε το τθω δοον‘\n");
        }
        self.th_sight = bane_sight;
    }
    bane_sight (hello);
};

void(float hello) lord_sight =
{
    if (hello)
        sound (self, CHAN_VOICE, "hknight/idle.wav", 1, ATTN_NORM);
    else
        sound (self, CHAN_VOICE, "archer/sight2.wav", 1, ATTN_NORM);
};

void(float hello) hknight_sight =
{
    if (hello)
        sound (self, CHAN_VOICE, "hknight/idle.wav", 1, ATTN_NORM);
    else
        sound (self, CHAN_VOICE, "hknight/sight1.wav", 1, ATTN_NORM);
};

// Files common to all of the hell knight variants.
void() hk_cache =
{
    precache_model ("progs/hknight.mdl");
    precache_model ("progs/k_spike.mdl");
    precache_model ("progs/h_hellkn.mdl");

    precache_sound ("hknight/attack1.wav");
    precache_sound ("hknight/hit.wav");    // used by C code, so don't sound2
    precache_sound ("hknight/slash1.wav");
    precache_sound ("hknight/idle.wav");
    precache_sound ("knight/sword1.wav");
    precache_sound ("knight/sword2.wav");

//    precache_sound ("hknight/death1.wav");
//    precache_sound ("hknight/pain1.wav");
//    precache_sound ("hknight/sight1.wav");
};

// Classic, standard hell knight.
void() hknight_cache =
{
    hk_cache ();

    precache_sound ("hknight/death1.wav");
    precache_sound ("hknight/pain1.wav");
    precache_sound ("hknight/sight1.wav");
};

// Tougher and meaner red version with fire magic.
void() hklord_cache =
{
    hk_cache ();

    precache_model ("progs/lavaball.mdl");

    // Borrow sounds from archer lords.
    // Update:  Don't use growl2.wav or growl3.wav.
    precache_sound ("archer/death2.wav");
    precache_sound ("archer/pain2.wav");
    precache_sound ("archer/sight2.wav");
    precache_sound ("boss1/throw.wav");
    precache_sound ("weapons/fwall.wav");
    precache_sound ("weapons/fwallhit.wav");
    precache_sound ("weapons/fwalltap.wav");
};

// For Rapture's hkboss.
void() hkboss_cache =
{
    hknight_cache ();

    precache_model ("progs/v_spike.mdl");       // Used by hkboss.
    precache_sound ("hkboss/iceball.wav");      // Used by hkboss.
};

// Prototypes...
void() monster_axeman;
//============
// QUAKED monster_hell_knight (1 0 0) (-16 -16 -24) (16 16 40) Ambush Lord
//============
void() monster_hell_knight =
{
    local   float   look;

// SoE:  sf 1 upgrades, sf 2 is ambush.
    if (!respawning)
    {
        if (soul_evil)
        {
            look = self.spawnflags & 1;
            if (look)
            {
                self.spawnflags = self.spawnflags - 1;
                if (self.classname != "monster_hkboss")
                    self.classname  = "monster_hell_lord";
            }
            if (self.spawnflags & 2)
                self.spawnflags = (self.spawnflags - 2) | 1;
//- - - - - - - - -
// SoE Indian Summer Hack.
            if (look)
            {   // nsoe1 and nsoe2 have too many Hell Lords -- downgrade some of them!
                if (world.model == "maps/nsoe1.bsp")
                {
                    AddOne();   // Also called by item_health -- gross!
                    if ((gloat == 2) || (gloat == 3))   // First two knights.
                        self.classname  = "monster_hell_knight";
                    else if ((gloat == 8) || (gloat == 9))
                    {   // No lords on Easy.
                        if (cvar("skill") == 0)
                            self.classname  = "monster_hell_knight";
                    }
                    else if (gloat == 12)
                        {self.classname = "monster_axeman";  monster_axeman();  return;}
                }
                else if (world.model == "maps/nsoe2.bsp")
                {
                    AddOne();
                    if ((gloat == 13) || (gloat == 14)) // Near Sha'Vile.
                        {self.classname = "monster_axeman";  monster_axeman();  return;}
                    // 1 = City entrance, 4-5 = Outside near nailgun, 16-17 = Chapel
                    if ((gloat == 0) || (gloat == 1) || (gloat == 4) || (gloat == 5) ||
                    (gloat == 16) || (gloat == 17))
                        self.classname  = "monster_hell_knight";
                }
            }
//- - - - - - - - -
        }
        else
        {
            if (self.spawnflags & 2)
                if (self.classname != "monster_hkboss")
                    self.classname  = "monster_hell_lord";
        }
    }
//- - - - - - - - -
    if (!self.spawned)
    {
        if (noenemies)
            {remove (self); return;}
        if (self.classname == "monster_hkboss")
            hkboss_cache ();
        else if (self.classname == "monster_hell_lord")
            hklord_cache ();
        else
            hknight_cache ();
    }

    if (self.classname == "monster_hell_lord")
    {
        if (soul_evil)
            monster_purse (3, 0.75, 0.25);
        self.skin       = SKIN_HELL_LORD;
        self.th_missile = lord_magic;
        self.th_sight   = lord_sight;
        // Originally 500 health, too much.
        monster_form ("progs/hknight.mdl", '-16 -16 -24', '16 16 40', 400, -40);
    }
    else if (self.classname == "monster_hkboss")
    {
        if (soul_evil)
            monster_purse (5, 0.75, 0.25);
        self.skin       = SKIN_HKBOSS;
        self.th_missile = hkboss_magic;
        monster_form ("progs/hknight.mdl", '-16 -16 -24', '16 16 40', 900, -40);
    }
    else
    {
        if (soul_evil)
        {
            monster_purse (3, 0.5, 0);
            self.skin       = (random() * 2) & 1;
        }
        else
            self.skin       = self.skin & 1;    // First two only.
        monster_form ("progs/hknight.mdl", '-16 -16 -24', '16 16 40', 250, -40);
    }
    self.mdl_head   = "progs/h_hellkn.mdl";
    self.mass       = MASS_MEDIUM;

    self.th_stand   = hknight_stand1;
    self.th_walk    = hknight_walk1;
    self.th_run     = hknight_run1;
    if (!self.th_missile)
        self.th_missile = hknight_magic;    // Was 'hknight_magicc1'
    self.th_melee   = hknight_melee;
    self.th_pain    = hknight_pain;
    self.th_die     = hknight_die;
    //- - - - - - - -
//    self.th_jump    = hknight_jump1;
    if (!self.th_sight)
        self.th_sight   = hknight_sight;
    self.th_revive  = hknight_revive;
    self.th_gib     = hknight_gib;
    self.th_spawn   = monster_hell_knight;
    
    self.family     = FAM_BANE;
    self.xfl        = XFL_ARMORED;
    self.hittype    = HIT_METAL;

    monster_start ();
};

void() monster_hell_lord = {monster_hell_knight ();};
void() monster_lord =
    {self.classname = "monster_hell_lord";  monster_hell_knight ();};

// For Rapture.
void() monster_hkboss = {monster_hell_knight ();};

//==========================================================================
//  The Bane  (SoE Big Bad Evil Guy, Hell Knight boss variant)
//==========================================================================
float() BaneCheckAttack =
{
// Here so Bane can destroy downed zombies and what not.
    if (enemy_range == RANGE_MELEE)
        if (infront (self.enemy))
            if (CanDamage (self.enemy, self))
                {self.th_melee ();  return TRUE;}

// Use bog standard.
    return CheckAttack();
};

// In case Bane gets aggravated during teleportation...
void() bane_run =
{
    if (self.skin == SKIN_BANE_GRAY)
        bane_painc1 ();     // Restart shadow walk.
    else
        hknight_run1 ();
};

void() bane_cache =
{
// Bane - - - - - -
    precache_model ("progs/hknight.mdl");
    precache_model ("progs/drake/k_ball.mdl");    // Bane fireball.
    precache_model ("progs/h_hellkn.mdl");

    precache_sound ("ambience/thunder1.wav");
    precache_sound ("ambience/thunder2.wav");
    precache_sound ("bane/death.wav");
    precache_sound ("bane/growl.wav");
    precache_sound ("bane/hit.wav");
    precache_sound ("bane/laugh.wav");
    precache_sound ("bane/pain.wav");
    precache_sound ("bane/sight.wav");
    precache_sound ("bane/thunder.wav");
    precache_sound ("hknight/slash1.wav");
    precache_sound ("knight/sword1.wav");
    precache_sound ("knight/sword2.wav");
    precache_sound ("hipweap/mjolhit.wav");
    precache_sound ("misc/dsslop.wav");
    precache_sound ("weapons/axhit.wav");
//- - - - - - - - -
};

void() monster_bane =
{
    local   float   sklv;

    if (!self.spawned)
    {
        if (noenemies)
            {remove (self); return;}
        bane_cache ();
    }

    // Update:  Make it very hard but possible to gib/behead the Bane.
    self.skin       = SKIN_BANE;
    monster_form ("progs/hknight.mdl", '-16 -16 -24', '16 16 40', 2000, -90);
    self.mdl_head   = "progs/h_hellkn.mdl";
    self.mass       = MASS_MEDIUM;
    sklv = cvar("skill");
    if (sklv < 1)
        self.waitmin = self.waitmax = -1;   // No clones or rage on Easy.
    else if (sklv < 2)
    {
        self.waitmin = floor(self.health / 4);   // Clones with 500 health.
        self.waitmax = floor(self.health / 2);   // Rage at half hp.
    }
    else
    {
        self.waitmax = self.health / 3;
        self.waitmin = floor(self.waitmax);      // Clones with 666 health.
        self.waitmax = floor(self.waitmax * 2);  // Rage at 66% hp.
    }

    self.th_stand   = hknight_stand1;
    self.th_walk    = hknight_walk1;
    self.th_run     = bane_run;     //hknight_run1;
    self.th_missile = bane_magic;
    self.th_melee   = hknight_melee;
    self.th_pain    = bane_pain;
    self.th_die     = hknight_die;
    //- - - - - - - -
//    self.th_jump    = hknight_jump1;
    self.th_sight   = bane_sight;
    self.th_check   = BaneCheckAttack;
    self.th_gib     = hknight_gib;
    self.th_win     = bane_win;
    self.th_void    = bane_void;
    self.th_spawn   = monster_bane;

// SoE Hack:  Disable the Bane's new powers in soe1m5.
    self.height = 0;
    if (!self.spawned)
    {
        if (self.spawnflags & 2)
        {
            self.height  = BANE_CHAT;    // The Bane speaks!
            self.waitmin = self.waitmax = -1;   // No clones or rage.
            self.th_sight = bane_hello;
        }
        else if (world.model == "maps/nsoe5.bsp")
        {
            self.height = BANE_CHAT;    // The Bane speaks!
            self.th_die = bane_die;
            self.th_sight = bane_hello;
        }
    }

    self.family     = FAM_BANE;
    self.xfl        = XFL_ARMORED | XFL_BOSS;   // Don't dare hit the king!
    self.ryuflags   = RYU_ALERT | RYU_INSANE;
    self.hittype    = HIT_METAL;
    if (soul_evil)
        monster_purse (20, 1, 0.5);

    monster_start ();
};


//==========================================================================
//  The Dark Lord  (Wraith in Black Armor, a bit like Tolkien's Nazgul)
//
//  Update 5/??/2011:  Now includes a two-stage version similar to
//  Dracula in (early) Castlevania, the human form and beast form.
//==========================================================================

void(float d) dk_slow;
void(float d) dk_fast;

void() darklord_run1  =[ $walk1,  darklord_run2  ] {dk_slow(2);};
void() darklord_run2  =[ $walk2,  darklord_run3  ] {M_Step(); dk_slow(5);};
void() darklord_run3  =[ $walk3,  darklord_run4  ] {dk_slow(5);};
void() darklord_run4  =[ $walk4,  darklord_run5  ] {dk_slow(4);};
void() darklord_run5  =[ $walk5,  darklord_run6  ] {dk_slow(4);};
void() darklord_run6  =[ $walk6,  darklord_run7  ] {dk_slow(2);};
void() darklord_run7  =[ $walk7,  darklord_run8  ] {dk_slow(2);};
void() darklord_run8  =[ $walk8,  darklord_run9  ] {dk_slow(3);};
void() darklord_run9  =[ $walk9,  darklord_run10 ] {dk_slow(3);};
void() darklord_run10 =[ $walk10, darklord_run11 ] {dk_slow(4);};
void() darklord_run11 =[ $walk11, darklord_run12 ] {dk_slow(3);};
void() darklord_run12 =[ $walk12, darklord_run13 ] {M_Step(); dk_slow(4);};
void() darklord_run13 =[ $walk13, darklord_run14 ] {dk_slow(6);};
void() darklord_run14 =[ $walk14, darklord_run15 ] {dk_slow(2);};
void() darklord_run15 =[ $walk15, darklord_run16 ] {dk_slow(2);};
void() darklord_run16 =[ $walk16, darklord_run17 ] {dk_slow(4);};
void() darklord_run17 =[ $walk17, darklord_run18 ] {dk_slow(3);};
void() darklord_run18 =[ $walk18, darklord_run19 ] {dk_slow(3);};
void() darklord_run19 =[ $walk19, darklord_run20 ] {dk_slow(3);};
void() darklord_run20 =[ $walk20, darklord_run1  ] {dk_slow(2);};
//void() darklord_run20 =[ $walk20, darklord_run21 ] {dk_slow(2);};
//void() darklord_run21 =[ $walk20, darklord_run2  ] {dk_slow(2);};

void() darklord_fast1 =[ $run1, darklord_fast2 ] {hk_idle_sound(); dk_fast(20);};
void() darklord_fast2 =[ $run2, darklord_fast3 ] {M_Step(); dk_fast(25);};
void() darklord_fast3 =[ $run3, darklord_fast4 ] {dk_fast(18);};
void() darklord_fast4 =[ $run4, darklord_fast5 ] {dk_fast(16);};
void() darklord_fast5 =[ $run5, darklord_fast6 ] {dk_fast(14);};
void() darklord_fast6 =[ $run6, darklord_fast7 ] {M_Step(); dk_fast(25);};
void() darklord_fast7 =[ $run7, darklord_fast8 ] {dk_fast(21);};
void() darklord_fast8 =[ $run8, darklord_fast1 ] {dk_fast(13);};

void() dk_shield =
{
    if (self.pets < 1)  // FIXME:  Check for wraths.
    {
        if (self.flags & FL_GODMODE)
        {
            self.flags = self.flags - FL_GODMODE;   //(self.flags & FL_GODMODE);
            self.waitmax = time + 10;   // Give player time to attack.
        }
        if (!self.mirror_finished)
            self.effects = self.effects - (self.effects & EF_BRIGHTFIELD);
    }
    else
    {
        if (!(self.flags & FL_GODMODE))
        {
            self.flags = self.flags | FL_GODMODE;
            ReflectorShield (self);
            sound(self, 6, "items/protect.wav", 1, ATTN_NORM);
        }
    }
};

void(float d) dk_slow =
{
    if (!self.fighting)
        {darklord_fast1 ();  return;}
    if (self.enemy)
        if (!visible (self.enemy))
            {darklord_fast1 ();  return;}
    dk_shield ();
    ai_run(d);
};
void(float d) dk_fast =
{
    if (self.fighting)
        if (self.enemy)
            if (visible (self.enemy))
                {darklord_run1 ();  return;}
    dk_shield ();
    ai_run(d);
};

//===========================================================================

void(float d) dk_charge =
{
//    bane_shadow ();
    hk_charge(d*2);
};

void() dk_idle_sound =
{
    if (random() < 0.2)
        sound (self, CHAN_VOICE, "darklord/idle1.wav", 1, ATTN_NORM);
//    if (random() < 0.5)
//        sound (self, CHAN_VOICE, "darklord/sword1.wav", 1, ATTN_NORM);
//    else
//        sound (self, CHAN_VOICE, "darklord/sword2.wav", 1, ATTN_NORM);
};

void() darklord_slice1  =[ $slice1,  darklord_slice2  ] {dk_idle_sound(); dk_charge(9);};
void() darklord_slice2  =[ $slice3,  darklord_slice3  ] {M_Step(); dk_charge(19);};
void() darklord_slice3  =[ $slice5,  darklord_slice4  ] {dk_charge(11); bane_melee(-200); ai_melee();};
void() darklord_slice4  =[ $slice6,  darklord_slice5  ] {dk_charge(15); ai_melee();};
void() darklord_slice5  =[ $slice7,  darklord_slice6  ] {M_Step(); dk_charge(8); ai_melee();};
void() darklord_slice6  =[ $slice9,  darklord_slice7  ] {dk_charge(2); ai_melee();};
void() darklord_slice7  =[ $slice10, hknight_run1 ] {dk_charge(3);};

void() darklord_smash1  =[ $smash2,  darklord_smash2  ] {dk_idle_sound(); dk_charge(14);};
void() darklord_smash2  =[ $smash4,  darklord_smash3  ] {M_Step(); dk_charge(20);};
void() darklord_smash3  =[ $smash5,  darklord_smash4  ] {dk_charge(10); bane_melee(0); ai_melee();};
void() darklord_smash4  =[ $smash6,  darklord_smash5  ] {dk_charge(7); ai_melee();};
void() darklord_smash5  =[ $smash7,  darklord_smash6  ] {dk_charge(12); ai_melee();};
void() darklord_smash6  =[ $smash8,  darklord_smash7  ] {dk_charge(2); ai_melee();};
void() darklord_smash7  =[ $smash9,  darklord_smash8  ] {M_Step(); dk_charge(3); ai_melee();};
void() darklord_smash8  =[ $smash11, hknight_run1 ] {dk_charge(0);};

void() darklord_watk1  =[ $w_attack1,  darklord_watk2  ] {dk_idle_sound(); dk_charge(2);};
void() darklord_watk2  =[ $w_attack5,  darklord_watk3  ] {bane_melee(0); ai_melee();};
void() darklord_watk3  =[ $w_attack7,  darklord_watk4  ] {dk_charge(1);};
void() darklord_watk4  =[ $w_attack8,  darklord_watk5  ] {dk_charge(4);};
void() darklord_watk5  =[ $w_attack9,  darklord_watk6  ] {dk_charge(5);};
void() darklord_watk6  =[ $w_attack10, darklord_watk7  ] {dk_charge(3); ai_melee();};
void() darklord_watk7  =[ $w_attack11, darklord_watk8  ] {M_Step(); dk_charge(2); ai_melee();};
void() darklord_watk8  =[ $w_attack13, darklord_watk9  ] {dk_charge(2); ai_melee();};
void() darklord_watk9  =[ $w_attack14, darklord_watk10 ] {dk_charge(0);};
void() darklord_watk10 =[ $w_attack16, darklord_watk11 ] {dk_charge(1);
    sound (self, CHAN_WEAPON, "knight/sword2.wav", 1, ATTN_NORM);
};
void() darklord_watk11 =[ $w_attack18, darklord_watk12 ] {dk_charge(4); bane_melee(200); ai_melee();};
void() darklord_watk12 =[ $w_attack19, darklord_watk13 ] {dk_charge(4); ai_melee();};
void() darklord_watk13 =[ $w_attack20, darklord_watk14 ] {dk_charge(6);};
void() darklord_watk14 =[ $w_attack22, hknight_run1] {dk_charge(10);};

void() darklord_melee =
{
    if (self.waitmin < time)
        {darklord_magica1(); return;}

    hknight_type = hknight_type + 1;

    sound (self, CHAN_WEAPON, "hknight/slash1.wav", 1, ATTN_NORM);
    if (hknight_type == 1)
        darklord_slice1 ();
    else if (hknight_type == 2)
        darklord_smash1 ();
    else if (hknight_type == 3)
    {
        darklord_watk1 ();
        hknight_type = 0;
    }
};

//============================================================================

void() darklord_thebeast;   // Look in 'dragon.qc'.

void() darklord_fall1  =[ $decap1,  darklord_fall2  ] {ai_forward(10);};
void() darklord_fall2  =[ $decap2,  darklord_fall3  ] {ai_forward(8);};
void() darklord_fall3  =[ $decap3,  darklord_fall4  ] {monster_drop(); ai_forward(7);};
void() darklord_fall4  =[ $decap4,  darklord_fall5  ] {};
void() darklord_fall5  =[ $decap5,  darklord_fall6  ] {};
void() darklord_fall6  =[ $decap6,  darklord_fall7  ] {};
void() darklord_fall7  =[ $decap7,  darklord_fall8  ] {};
void() darklord_fall8  =[ $decap8,  darklord_fall9  ] {ai_forward(10);};
void() darklord_fall9  =[ $decap9,  darklord_fall10 ] {ai_forward(11);};
void() darklord_fall10 =[ $decap10, darklord_fall11 ] {};
void() darklord_fall11 =[ $decap11, darklord_fall12 ] {};
void() darklord_fall12 =[ $decap12, darklord_thebeast ]
{
    Quake_Level (0, self.origin, 5);
    self.nextthink = time + 3;
};

void(entity attacker, float damage) darklord_pain =
{
    if (self.max_health - self.health >= 1000)
    {   // Took enough damage to die.  So feign death before transforming.
        local   vector  st;

        // Become immune to damage and reset health.
        self.flags = self.flags | FL_GODMODE;
        self.health = self.max_health - 1000;

        // Reset some things.
        self.th_stand   =
        self.th_walk    =
        self.th_run     = darklord_thebeast;
        self.th_melee   =
        self.th_missile =
        self.th_pain    = SUB_Null;

        SUB_RemovePowerups ();

        // Fake death.
        sound (self, CHAN_VOICE, "darklord/death.wav", 1, ATTN_NORM);
    
        makevectors (self.angles);
        st = '-2 0 28';
        st = self.origin + v_forward*st_x + v_right*st_y + v_up*st_z;
        Rocket_Explosion2 (st, 32, 5);
        darklord_fall1 ();

        return;
    }

// Proceed as usual.
    hknight_pain (attacker, damage);
};

//============================================================================

// Note:  The dark lord's helmet explodes when destroyed.
void() darklord_die1  =[ $decap1,  darklord_die2  ] {ai_forward(10);};
void() darklord_die2  =[ $decap2,  darklord_die3  ] {ai_forward(8);};
void() darklord_die3  =[ $decap3,  darklord_die4  ] {monster_drop(); ai_forward(7);};
void() darklord_die4  =[ $decap4,  darklord_die5  ] {};
void() darklord_die5  =[ $decap5,  darklord_die6  ] {};
void() darklord_die6  =[ $decap6,  darklord_die7  ] {};
void() darklord_die7  =[ $decap7,  darklord_die8  ] {};
void() darklord_die8  =[ $decap8,  darklord_die9  ] {ai_forward(10);};
void() darklord_die9  =[ $decap9,  darklord_die10 ] {ai_forward(11);};
void() darklord_die10 =[ $decap10, darklord_die11 ] {};
void() darklord_die11 =[ $decap11, darklord_die12 ] {};
void() darklord_die12 =[ $decap12, monster_end   ] {};

void() darklord_dieb1 =[ $decapb1, darklord_dieb2 ] {};
void() darklord_dieb2 =[ $decapb2, darklord_dieb3 ] {};
void() darklord_dieb3 =[ $decapb3, darklord_dieb4 ] {monster_drop();};
void() darklord_dieb4 =[ $decapb4, darklord_dieb5 ] {};
void() darklord_dieb5 =[ $decapb5, darklord_dieb6 ] {};
void() darklord_dieb6 =[ $decapb6, darklord_dieb7 ] {};
void() darklord_dieb7 =[ $decapb7, darklord_dieb8 ] {};
void() darklord_dieb8 =[ $decapb8, darklord_dieb9 ] {};
void() darklord_dieb9 =[ $decapb9, monster_end   ] {};

void() darklord_die =
{
// check for gib
//    if (Shattered ())
//        return;

// regular death
    local   vector  st;

    sound (self, CHAN_VOICE, "darklord/death.wav", 1, ATTN_NORM);

    if (random() > 0.5)
        {st = '-2 0 28'; darklord_die1 ();}
    else
        {st = '-3 0 33'; darklord_dieb1 ();}
    makevectors (self.angles);
    st = self.origin + v_forward*st_x + v_right*st_y + v_up*st_z;
    Rocket_Explosion2 (st, 32, 5);
};

void(float hello) darklord_sight =
{
    if (hello)
        sound (self, CHAN_VOICE, "darklord/idle1.wav", 1, ATTN_NORM);
    else
        sound (self, CHAN_VOICE, "darklord/sight.wav", 1, ATTN_NORM);
};

void() dark_lord_cache =
{
// Bane - - - - - -
    precache_model ("progs/hknight.mdl");
    precache_model ("progs/r_bolt.mdl");
    // The dark lord's head explodes when killed, so no head gib needed.

    precache_sound ("bane/hit.wav");
    precache_sound ("darklord/death.wav");
    precache_sound ("darklord/idle1.wav");
    precache_sound ("darklord/pain.wav");
    precache_sound ("darklord/sight.wav");
    precache_sound ("darklord/death.wav");
    precache_sound ("knight/sword1.wav");
    precache_sound ("knight/sword2.wav");
    precache_sound ("hipweap/mjolhit.wav");
    precache_sound ("items/protect.wav");   // Powerup sound
    precache_sound ("plasma/explode.wav");
    precache_sound ("plasma/fire.wav");
    precache_sound ("weapons/axhit.wav");
    precache_sound ("skullwiz/gate.wav");   // For wrath summoning.
//- - - - - - - - -
};

// FIXME:  Where do I put this aside from here?
void() dragon_doe_cache;

//============
// --> QUAKED monster_dark_lord (1 0 0) (-16 -16 -24) (16 16 40) Ambush Guardian Beast
// Guardian version can raise a shield and summon wraths.
// Beast version has 4000+ hp and morphs into a dragon when damaged enough.
// The boss flags can stack!
//============
void() monster_dark_lord =
{
    local   float   hp;

// FIXME:  Test -- good to go?  Remove?
//self.spawnflags = self.spawnflags | 4;
    if (!self.spawned)
    {
        if (noenemies)
            {remove (self); return;}
        dark_lord_cache ();
        wrath_cache ();
        if (self.spawnflags & 4)
            dragon_doe_cache ();
    }

    if (self.spawnflags & 4) {
        hp = 4000 + (500 * cvar("skill"));
        if (self.deathstring == "")  self.deathstring = " fell before a Beastly Dark Lord\n";
    } else {
        if (self.deathstring == "")  self.deathstring = " fell before a Dark Lord\n";
        hp = 1000;
    }

    self.skin       = SKIN_DARK_LORD;
    monster_form ("progs/hknight.mdl", '-16 -16 -24', '16 16 40', hp, 0);
    self.mass       = MASS_MEDIUM;

    // Takes reduced damage from lightning and cold.
    self.resist_cold = 0.5;

    self.th_stand   = hknight_stand1;
    self.th_walk    = hknight_walk1;
    self.th_run     = darklord_run1;    //hknight_run1;
    self.th_missile = darklord_magic;   //lord_magic;
    self.th_melee   = darklord_melee;   //hknight_melee;
    if (self.spawnflags & 4)
        self.th_pain    = darklord_pain;    // Checks for stage change.
    else
        self.th_pain    = hknight_pain;
    self.th_die     = darklord_die;
    //- - - - - - - -
//    self.th_jump    = hknight_jump1;
    self.th_sight   = darklord_sight;
    self.th_spawn   = monster_dark_lord;
    self.th_check   = BaneCheckAttack;
    // As a wraith bound in armor, it cannot be gibbed.

    self.bloodtype  = 0 - BLOOD_DARK;   // Less blood and no gibs.

    self.family     = FAM_BANE | FAM_UNDEAD;
    self.xfl        = XFL_ARMORED;
    if (self.spawnflags & 6) {
        self.deathstring = " fell before a Guardian Dark Lord\n";
        self.xfl        = XFL_BOSS;
    }
    self.ryuflags   = RYU_ALERT | RYU_INSANE;
    self.hittype    = HIT_METAL;
    if (soul_evil)
        monster_purse (20, 1, 0.5);

    

    monster_start ();
};


//===========================/  END OF FILE  /===========================//
