/*
==============================================================================

NECROMANCER

==============================================================================
*/

$cd id1/models/necro
$origin 0 0 24
$base base        
$skin skin

$frame deathA1 deathA2 deathA3 deathA4 deathA5 deathA6 deathA7 deathA8
$frame deathA9 deathA10 deathA11 deathA12 deathA13 deathA14 deathA15
$frame deathA16 deathA17 deathA18 deathA19 deathA20

$frame decap1 decap2 decap3 decap4 decap5 decap6 decap7 decap8 decap9 decap10
$frame decap11 decap12 decap13 decap14 decap15 decap16 decap17 decap18
$frame decap19 decap20

$frame Hatak1 Hatak2 Hatak3 Hatak4 Hatak5 Hatak6 Hatak7 Hatak8

$frame Hpain1 Hpain2 Hpain3 Hpain4 Hpain5 Hpain6 Hpain7 Hpain8

$frame Hrun1 Hrun2 Hrun3 Hrun4 Hrun5 Hrun6 Hrun7 Hrun8 Hrun9 Hrun10
$frame Hrun11 Hrun12

$frame Hwait1 Hwait2 Hwait3 Hwait4 Hwait5 Hwait6 Hwait7 Hwait8 Hwait9 Hwait10
$frame Hwait11 Hwait12

$frame Siatak1 Siatak2 Siatak3 Siatak4 Siatak5 Siatak6 Siatak7 Siatak8
$frame Siatak9 Siatak10 Siatak11 Siatak12

$frame Sipain1 Sipain2 Sipain3 Sipain4 Sipain5 Sipain6 Sipain7 Sipain8

$frame Sirun1 Sirun2 Sirun3 Sirun4 Sirun5 Sirun6 Sirun7 Sirun8 Sirun9 Sirun10
$frame Sirun11 Sirun12

$frame Siwait1 Siwait2 Siwait3 Siwait4 Siwait5 Siwait6 Siwait7 Siwait8
$frame Siwait9 Siwait10 Siwait11 Siwait12

$frame Statak1 Statak2 Statak3 Statak4 Statak5 Statak6 Statak7 Statak8

$frame Stpain1 Stpain2 Stpain3 Stpain4 Stpain5 Stpain6 Stpain7 Stpain8

$frame Strun1 Strun2 Strun3 Strun4 Strun5 Strun6 Strun7 Strun8 Strun9 Strun10
$frame Strun11 Strun12

$frame Stwait1 Stwait2 Stwait3 Stwait4 Stwait5 Stwait6 Stwait7 Stwait8
$frame Stwait9 Stwait10 Stwait11 Stwait12

//=============================================================================

// Used by self.height.
float   NECRO_ACOLYTE   = 1;
float   NECRO_SORCERER  = 2;
float   NECRO_WARLOCK   = 3;

// Used by self.worldtype.  NSA = Necro Special Attack.
float   NSA_CURSE               = 1;
float   NSA_CORPSE_EXPLOSION    = 2;
float   NSA_SUMMON_GOLEM        = 3;
float   NSA_SUMMON_IMPS         = 4;

float   NECRO_MAX_IMPS      = 5;    // Maximum imps.

float   CORPSE_EXPLOSION_RADIUS = 200;

void() necro_activate_special;

float() necro_boss = {return (self.classname == "monster_necromancer");};

//=============================================================================

// Alternate thinking for necrobolts.  Used by necromancer boss.
void() NecroBolt_Summon =
{
    if (self.delay <= time)
    if (self.master.flags & FLx_CREATURE)
    if (self.enemy)
    if (visible (self.enemy))
    {
        if (count_petclass(self.master, "monster_imp") < NECRO_MAX_IMPS)
        {
            if (SpawnMonster_Point (self.master, self.origin, ID_IMP_KINN))
            {
                remove (self);
                return;
            }
        }
    }
    NecroBolt_Think ();
};

void() acolyte_fire =
{
    local   vector  org, dir;

    makevectors (self.angles);
    org = self.origin + v_forward*14 - v_right*8 + v_up*12;     // '14 -2 19'
    dir = normalize (self.enemy.origin - self.origin);
    NecroSnake_Launch (org, dir, 600);
};

// 'hits' is number of bounces the shot begins with.  Used to change the
// number of bounces allowed before exploding.  With no bounces (hits = 0),
// the fireball may bounce up to the nemesant standard of three times without
// exploding.  What 'hits' does is change the bounces to 3 - hits.
void(float hits) sorcerer_fire =
    {M_FireSmart ('8 -5 12');  newmis.count = hits;};

void(vector st) warlock_fire =
{
    local   vector  org, dir, vec;
    local   entity  targ;       // Lead shots toward this target.

    sound (self, CHAN_WEAPON, "necro/att_w.wav", 1, ATTN_NORM);
    self.effects = self.effects | EF_MUZZLEFLASH;

    targ = world;

    makevectors (self.angles);
    // old st was '12 7 26'.
    org = self.origin + v_forward*st_x + v_right*st_y + v_up*st_z;

    vec = Midpoint (self.enemy);
    vec_z = self.enemy.absmin_z + self.enemy.size_z*0.7;

    if (skill >= 2)     // In Hard/NM, aim at the enemy's feet occasionally.
    {
        if (org_z > self.enemy.absmin_z)
            if (random() < 0.2)
                if (vlen(vec - org) < 450)
                    vec_z = self.enemy.absmin_z;    // Aim for the feet.
// DISABLED -- Warlock is dangerous enough without leading.
//        if (random() < 0.1)
//            targ = self.enemy;
    }
    MakeFancy (org, X32_STAR_RED, 2);
    dir = Maim_Line (org, vec, 600, targ, '0 0 0');
    NecroBolt_Launch (org, dir);
// Update:  Secondary bolts from necro boss can spawn monsters.
    if (necro_boss ())
        if ( self.frame == $Hatak8 )
            newmis.think = NecroBolt_Summon;
};


//=============================================================================

// STAND - - - - - - - - - - - - - - - -
void() necro_idlesound =
{
// PM:  Replaced fwiz code with nemesants'.  This is due to replaced sounds.
    if (self.waitmin < time)
    {
        local   float   wr;

    // Use voice channel instead of auto -- don't want slain magi talking.
        wr = random();
        if (wr > 0.7) 
            sound (self, CHAN_VOICE, "necro/idle1.wav", 1, ATTN_NORM);
        else if (wr < 0.3)
            sound (self, CHAN_VOICE, "necro/idle2.wav", 1, ATTN_NORM);
        self.waitmin = time + 10;
    }
};

// CoE Acolyte
void() necro1_stand1  =[ $Siwait1,  necro1_stand2  ] {necro_idlesound(); ai_stand();};
void() necro1_stand2  =[ $Siwait2,  necro1_stand3  ] {ai_stand();};
void() necro1_stand3  =[ $Siwait3,  necro1_stand4  ] {ai_stand();};
void() necro1_stand4  =[ $Siwait4,  necro1_stand5  ] {ai_stand();};
void() necro1_stand5  =[ $Siwait5,  necro1_stand6  ] {ai_stand();};
void() necro1_stand6  =[ $Siwait6,  necro1_stand7  ] {ai_stand();};
void() necro1_stand7  =[ $Siwait7,  necro1_stand8  ] {ai_stand();};
void() necro1_stand8  =[ $Siwait8,  necro1_stand9  ] {ai_stand();};
void() necro1_stand9  =[ $Siwait9,  necro1_stand10 ] {ai_stand();};
void() necro1_stand10 =[ $Siwait10, necro1_stand11 ] {ai_stand();};
void() necro1_stand11 =[ $Siwait11, necro1_stand12 ] {ai_stand();};
void() necro1_stand12 =[ $Siwait12, necro1_stand1  ] {ai_stand();};

// CoE Sorcerer
void() necro2_stand1  =[ $Stwait1,  necro2_stand2  ] {necro_idlesound(); ai_stand();};
void() necro2_stand2  =[ $Stwait2,  necro2_stand3  ] {ai_stand();};
void() necro2_stand3  =[ $Stwait3,  necro2_stand4  ] {ai_stand();};
void() necro2_stand4  =[ $Stwait4,  necro2_stand5  ] {ai_stand();};
void() necro2_stand5  =[ $Stwait5,  necro2_stand6  ] {ai_stand();};
void() necro2_stand6  =[ $Stwait6,  necro2_stand7  ] {ai_stand();};
void() necro2_stand7  =[ $Stwait7,  necro2_stand8  ] {ai_stand();};
void() necro2_stand8  =[ $Stwait8,  necro2_stand9  ] {ai_stand();};
void() necro2_stand9  =[ $Stwait9,  necro2_stand10 ] {ai_stand();};
void() necro2_stand10 =[ $Stwait10, necro2_stand11 ] {ai_stand();};
void() necro2_stand11 =[ $Stwait11, necro2_stand12 ] {ai_stand();};
void() necro2_stand12 =[ $Stwait12, necro2_stand1  ] {ai_stand();};

// CoE Warlock
void() necro3_stand1  =[ $Hwait1,  necro3_stand2  ] {necro_idlesound(); ai_stand();};
void() necro3_stand2  =[ $Hwait2,  necro3_stand3  ] {ai_stand();};
void() necro3_stand3  =[ $Hwait3,  necro3_stand4  ] {ai_stand();};
void() necro3_stand4  =[ $Hwait4,  necro3_stand5  ] {ai_stand();};
void() necro3_stand5  =[ $Hwait5,  necro3_stand6  ] {ai_stand();};
void() necro3_stand6  =[ $Hwait6,  necro3_stand7  ] {ai_stand();};
void() necro3_stand7  =[ $Hwait7,  necro3_stand8  ] {ai_stand();};
void() necro3_stand8  =[ $Hwait8,  necro3_stand9  ] {ai_stand();};
void() necro3_stand9  =[ $Hwait9,  necro3_stand10 ] {ai_stand();};
void() necro3_stand10 =[ $Hwait10, necro3_stand11 ] {ai_stand();};
void() necro3_stand11 =[ $Hwait11, necro3_stand12 ] {ai_stand();};
void() necro3_stand12 =[ $Hwait12, necro3_stand1  ] {ai_stand();};

// WALK - - - - - - - - - - - - - - - -
void() necro1_walk1  =[ $Sirun1,  necro1_walk2  ] {necro_idlesound(); M_Step(); ai_walk(8);};
void() necro1_walk2  =[ $Sirun2,  necro1_walk3  ] {ai_walk(8);};
void() necro1_walk3  =[ $Sirun3,  necro1_walk4  ] {ai_walk(8);};
void() necro1_walk4  =[ $Sirun4,  necro1_walk5  ] {ai_walk(8);};
void() necro1_walk5  =[ $Sirun5,  necro1_walk6  ] {ai_walk(8);};
void() necro1_walk6  =[ $Sirun6,  necro1_walk7  ] {ai_walk(8);};
void() necro1_walk7  =[ $Sirun7,  necro1_walk8  ] {M_Step(); ai_walk(8);};
void() necro1_walk8  =[ $Sirun8,  necro1_walk9  ] {ai_walk(8);};
void() necro1_walk9  =[ $Sirun9,  necro1_walk10 ] {ai_walk(8);};
void() necro1_walk10 =[ $Sirun10, necro1_walk11 ] {ai_walk(8);};
void() necro1_walk11 =[ $Sirun11, necro1_walk12 ] {ai_walk(8);};
void() necro1_walk12 =[ $Sirun12, necro1_walk1  ] {ai_walk(8);};

void() necro2_walk1  =[ $Strun1,  necro2_walk2  ] {necro_idlesound(); M_Step(); ai_walk(8);};
void() necro2_walk2  =[ $Strun2,  necro2_walk3  ] {ai_walk(8);};
void() necro2_walk3  =[ $Strun3,  necro2_walk4  ] {ai_walk(8);};
void() necro2_walk4  =[ $Strun4,  necro2_walk5  ] {ai_walk(8);};
void() necro2_walk5  =[ $Strun5,  necro2_walk6  ] {ai_walk(8);};
void() necro2_walk6  =[ $Strun6,  necro2_walk7  ] {ai_walk(8);};
void() necro2_walk7  =[ $Strun7,  necro2_walk8  ] {M_Step(); ai_walk(8);};
void() necro2_walk8  =[ $Strun8,  necro2_walk9  ] {ai_walk(8);};
void() necro2_walk9  =[ $Strun9,  necro2_walk10 ] {ai_walk(8);};
void() necro2_walk10 =[ $Strun10, necro2_walk11 ] {ai_walk(8);};
void() necro2_walk11 =[ $Strun11, necro2_walk12 ] {ai_walk(8);};
void() necro2_walk12 =[ $Strun12, necro2_walk1  ] {ai_walk(8);};

void() necro3_walk1  =[ $Hrun1,  necro3_walk2  ] {necro_idlesound(); M_Step(); ai_walk(8);};
void() necro3_walk2  =[ $Hrun2,  necro3_walk3  ] {ai_walk(8);};
void() necro3_walk3  =[ $Hrun3,  necro3_walk4  ] {ai_walk(8);};
void() necro3_walk4  =[ $Hrun4,  necro3_walk5  ] {ai_walk(8);};
void() necro3_walk5  =[ $Hrun5,  necro3_walk6  ] {ai_walk(8);};
void() necro3_walk6  =[ $Hrun6,  necro3_walk7  ] {ai_walk(8);};
void() necro3_walk7  =[ $Hrun7,  necro3_walk8  ] {M_Step(); ai_walk(8);};
void() necro3_walk8  =[ $Hrun8,  necro3_walk9  ] {ai_walk(8);};
void() necro3_walk9  =[ $Hrun9,  necro3_walk10 ] {ai_walk(8);};
void() necro3_walk10 =[ $Hrun10, necro3_walk11 ] {ai_walk(8);};
void() necro3_walk11 =[ $Hrun11, necro3_walk12 ] {ai_walk(8);};
void() necro3_walk12 =[ $Hrun12, necro3_walk1  ] {ai_walk(8);};

// RUN - - - - - - - - - - - - - - - -
// Casting scenes are used when necro uses special abilities such as
// teleportation, shielding, or summoning.
void() necro_face =
{
    if (self.worldtype)
    {   // Attempting a special atttack.
        if (self.fallen)
            ai_ritual ();
        else if (!self.enemy)
            return;
    }
    ai_face ();
};

void() necro1_cast1  =[ $Siatak1,  necro1_cast2  ] {ai_face();};
void() necro1_cast2  =[ $Siatak5,  necro1_cast3  ] {ai_face();};
void() necro1_cast3  =[ $Siatak6,  necro1_cast4  ] {ai_face(); self.nextthink = time + 0.2;};
void() necro1_cast4  =[ $Siatak11, necro1_cast5  ] {ai_face(); necro_activate_special();};
void() necro1_cast5  =[ $Siatak12, necro1_run1   ] {ai_face();};

void() necro2_cast1  =[ $Statak1,  necro2_cast2  ] {ai_face();};
void() necro2_cast2  =[ $Statak2,  necro2_cast3  ] {ai_face();};
void() necro2_cast3  =[ $Statak5,  necro2_cast4  ] {ai_face();};
void() necro2_cast4  =[ $Statak6,  necro2_cast5  ] {ai_face();};
void() necro2_cast5  =[ $Statak7,  necro2_cast6  ] {ai_face(); necro_activate_special();};
void() necro2_cast6  =[ $Statak8,  necro2_run1   ] {ai_face();};

void() necro3_cast1  =[ $Hatak1,   necro3_cast2  ] {ai_face();};
void() necro3_cast2  =[ $Hatak2,   necro3_cast3  ] {ai_face();};
void() necro3_cast3  =[ $Hatak7,   necro3_cast4  ] {ai_face();};
void() necro3_cast4  =[ $Hatak8,   necro3_run1   ] {ai_face(); necro_activate_special();};

void() necro_cast =
{
    if (self.height == NECRO_ACOLYTE)
        necro1_cast1();
    else if (self.height == NECRO_SORCERER)
        necro2_cast1();
    else //if (self.height == NECRO_WARLOCK)
        necro3_cast1();
};

float   RANGE_NECROSHIELD   = 150;
float   DELAY_NECROSHIFT    = 1.5;  // Can't teleport to dodge missiles.

float(vector org) necro_clear =
{
    local   entity  head;

    head = findradius(org, 120);
    while (head)
    {
        if (head.flags & FLx_CREATURE)
            if (head != self)
                if (Ally_Check (head, self))
                    return FALSE;

        head = head.chain;
    }
    return TRUE;
};

// 'drop':  0 = No bomb, 1 = drop bomb, 2 = teleport only when attacking.
float(entity targ, vector spot, float drop) necro_teleport =
{
    local   vector  org;
    local   float   bomb;

// Check if the necro can drop a bomb near an enemy.
    bomb = FALSE;
    if (drop)
    {
        if (self.enemy)
            if (InMeleeRange (120))
                if (CanDamage (self.enemy, self))
                    bomb = TRUE;
        if (drop == 2)
            if (!bomb)
                return FALSE;
    }

// Do teleportation routine.
    org = self.origin;
    if (EntryPoint_Spawn (self, targ, spot, EntryPoint_Random))
    {
        // Necro successfully teleported away.
        self.waitmax = time + DELAY_NECROSHIFT;
        if (bomb)
        {   // Make sure necro and friends are a safe distance away before
            // dropping a explosive rune as a time bomb.
            if (vlen(org - self.origin) > 120)
                if (necro_clear (org))
                    NecroBomb (org);
        }
        spawn_tfog (org);
        spawn_tfog (self.origin);
        self.fallen = world;
        self.worldtype = 0;
        necro_cast ();
    }
    return FALSE;
};

// Called when teleporting away from an attack.
float() necro_teledef =
{
    if (self.waitmax <= time)   // Don't spam teleport as a defense.
        return  necro_teleport (self.enemy, self.origin, FALSE);
    return FALSE;
};

// Look for enemy missiles infront of the necro.  We don't care which
// missile is found, just whether or not any are found, so return T or F.
float() necro_find =
{
    if (self.enemy)
    {
        local   entity  head;

        head = findradius (self.origin, RANGE_NECROSHIELD);
        while (head)
        {   // If missile came from enemy, or targets the necro, take it
            // if the missile is infront of the necro.
            if (IsMissile (head))
                if ((head.master == self.enemy) || (head.enemy == self))
                    if (visible (head))
                        if (infront (head))
                            return TRUE;
            head = head.chain;
        }
    }
    return FALSE;
};

// Searches for missiles to defend against.  If none are found, then move.
// Called by sorcerer and warlock.  Update:  Called by acolyte code now
// because necro boss starts in acolyte mode.
void() necro_run =
{
    if (!self.mirror_finished)
        self.effects = self.effects - (self.effects & EF_BRIGHTFIELD);

// Sorcerer and warlock modes have special defenses.
    if (self.height >= NECRO_SORCERER)
    if (necro_find ())
    {
        if (self.height == NECRO_SORCERER)
        {   // Sorcerer -- Teleport to random nearby point.
            if (necro_teledef ())
                return;
            // Made it here?  Teleportation failed, so just move.
        }
        else if (self.height == NECRO_WARLOCK)
        {   // Warlock -- Raise shields if practical to do so.
            // Do NOT raise shields if the following apply:
            // 1) Enemy is too close.
            // 2) Necro is in a deadly environment (e.g., in lava).
            if (vlen(self.enemy.origin - self.origin) > (RANGE_NECROSHIELD + self.enemy.maxs_x))
                if (self.h2otype >= CONTENT_WATER)
                    if ((self.h2otype != CONTENT_WATER) || (self.h2olevel < 3))
                    {
                        self.fallen = world;
                        self.worldtype = 0;
                        ReflectorShield (self);     // Turn on shield.
                        necro3_cast1 ();
                        return;
                    }
            // Necro boss may teleport instead if shield cannot be raised.
            if (necro_boss ())
                if (necro_teledef ())
                    return;
        }
        // At this point, no special defense was activated.
    }
// No defense activated, so move.
    local   float   d;

    d = 16;
    if (necro_boss ())
    {
        // Boss may teleport to enemy.
        if (self.search_time + 5 < time)
        {
            local   entity  targ;

            targ = world;
            if (self.enemy)
                targ = self.enemy;
            else if (!self.fighting)
                targ = self.master;

            if (targ)
            if (!visible (targ))
            if (necro_teleport (targ, targ.origin, TRUE))
            {   // Teleport to the enemy.
                self.search_time = time;    //+ 5;
                return;
            }
        }
        // Didn't teleport, so run.
// DISABLED -- With teleport, faster run seems to be a liability for necro.
//        if (!QC_CloseEnough (d * 2))
//            d = d * 2;
    }
// Move.  All running frames use the same speed.
    ai_run(d);
};

// Update:  Acolytes use necro_run() instead of ai_run(16) since the
// necro boss uses all three necro modes.
void() necro1_run1  =[ $Sirun1,  necro1_run2  ] {necro_idlesound(); M_Step(); necro_run();};
void() necro1_run2  =[ $Sirun3,  necro1_run3  ] {necro_run();};
void() necro1_run3  =[ $Sirun5,  necro1_run4  ] {necro_run();};
void() necro1_run4  =[ $Sirun7,  necro1_run5  ] {M_Step(); necro_run();};
void() necro1_run5  =[ $Sirun9,  necro1_run6  ] {necro_run();};
void() necro1_run6  =[ $Sirun11, necro1_run1  ] {necro_run();};

void() necro2_run1  =[ $Strun1,  necro2_run2  ] {necro_idlesound(); M_Step(); necro_run();};
void() necro2_run2  =[ $Strun3,  necro2_run3  ] {necro_run();};
void() necro2_run3  =[ $Strun5,  necro2_run4  ] {necro_run();};
void() necro2_run4  =[ $Strun7,  necro2_run5  ] {M_Step(); necro_run();};
void() necro2_run5  =[ $Strun9,  necro2_run6  ] {necro_run();};
void() necro2_run6  =[ $Strun11, necro2_run1  ] {necro_run();};

void() necro3_run1  =[ $Hrun1,  necro3_run2  ] {necro_idlesound(); M_Step(); necro_run();};
void() necro3_run2  =[ $Hrun3,  necro3_run3  ] {necro_run();};
void() necro3_run3  =[ $Hrun5,  necro3_run4  ] {necro_run();};
void() necro3_run4  =[ $Hrun7,  necro3_run5  ] {M_Step(); necro_run();};
void() necro3_run5  =[ $Hrun9,  necro3_run6  ] {necro_run();};
void() necro3_run6  =[ $Hrun11, necro3_run1  ] {necro_run();};

// ATTACK - - - - - - - - - - - - - - -
void() ai_repel;    // In 'nemesant.qc'.

float() necro_encore =
{
    if (necro_boss ())
        return TRUE;

    if (cranked || (skill >= 3))
        return TRUE;
    if (skill >= 2)
        if (random() < 0.25)
            return TRUE;
    return FALSE;
};

void() necro_run_check =
{   // PM:  Hack for non-specials.
    if (self.height == NECRO_SORCERER)
        self.think = necro2_run1;
    else if (self.height == NECRO_WARLOCK)
        self.think = necro3_run1;
};

// PM:  Sped up acolyte's swings due to frame rate differences between
// Quake and Hexen2.
void() necro1_melee1 =[ $Siatak2,  necro1_melee2 ] {ai_charge(3);
    if (necro_boss ())
        self.cnt = 2;   // Extra swings.
    else
        self.cnt = 0;
};
void() necro1_melee2 =[ $Siatak4,  necro1_melee3 ] {
    sound (self, CHAN_WEAPON, "knight/sword1.wav", 1, ATTN_NORM);
    ai_charge(0); ai_melee();
};
void() necro1_melee3 =[ $Siatak6,  necro1_melee4 ] {ai_charge(3); ai_melee();};
void() necro1_melee4 =[ $Siatak8,  necro1_melee5 ] {ai_charge(2); ai_melee();
    if (self.enemy.health > 0)
    if (self.cnt > 0)
    if (InMeleeRange (80))
    {   // Interrupt for a new attack, much like Zeal.
        self.cnt = self.cnt - 1;
        self.think = necro1_melee2;
        return;
    }
    self.cnt = 0;
};
void() necro1_melee5 =[ $Siatak10, necro1_melee6 ] {ai_charge(1);};
void() necro1_melee6 =[ $Siatak12, necro1_run1   ] {ai_charge(0); necro_run_check();};
void() necro1_melee =
{
    if (InMeleeRange (80))
        necro1_melee1 ();
    else
    {
        if (necro_boss ())
            if (random() < 0.2)
            {   // In case the boss gets stuck, let him make some potshots.
                if (self.th_missile)
                    self.th_missile ();
                return;
            }
        ai_charge (16);     // Not close enough -- continue march.
    }
};

void() necro1_attack1  =[ $Siatak1,  necro1_attack2  ] {ai_face();};
void() necro1_attack2  =[ $Siatak5,  necro1_attack3  ] {ai_face(); self.cnt = 0;};
void() necro1_attack3  =[ $Siatak6,  necro1_attack4  ] {ai_face();
    if (!self.cnt)
    {   // Set refire count.
        if (necro_boss ())
            self.cnt = random() * 3 & 3 + 1;
        else if (necro_encore ())
            self.cnt = 1;
    }
    if (self.cnt)
        sound (self, CHAN_WEAPON, "necro/att_a2.wav", 1, ATTN_NORM);
    else
        sound (self, CHAN_WEAPON, "necro/att_a.wav", 1, ATTN_NORM);
    acolyte_fire();
// Make small poison cloud.  'makevectors' called in above function.
    local   vector  org;
    org = self.origin + v_forward*14 - v_right*8 + v_up*12;
    MakeFancy (org, X32_CLOUD_GREEN + 1, 3);
};
void() necro1_attack4  =[ $Siatak6,  necro1_attack5  ] {ai_face();
    if (self.cnt)
    {
        acolyte_fire();
        self.cnt = self.cnt - 1;
        if (self.cnt)
            self.think = necro1_attack3;
    }
};
void() necro1_attack5  =[ $Siatak11, necro1_attack6  ] {ai_face(); acolyte_fire();};
void() necro1_attack6  =[ $Siatak12, necro1_run1     ] {ai_face();};

float() sorcerer_tele =
{
//- - - - - - - - -
//  The main purpose of this teleportation check is to increase distance
//  between the enemy and the necro.  Otherwise, enemies that reach the
//  necro can layeth the smackdown on him.
//- - - - - - - - -
    return necro_teleport (self.enemy, self.origin, 2);
};
void() necro2_attack1  =[ $Statak1,  necro2_attack2  ] {ai_face();};
void() necro2_attack2  =[ $Statak2,  necro2_attack3  ] {ai_face();};
void() necro2_attack3  =[ $Statak3,  necro2_attack4  ] {ai_face();
    if (!sorcerer_tele ())
        sorcerer_fire(2);
};
void() necro2_attack4  =[ $Statak4,  necro2_attack5  ] {ai_face();};
void() necro2_attack5  =[ $Statak5,  necro2_attack6  ] {ai_face();
    if (necro_encore ())
        sorcerer_fire(3);
};
void() necro2_attack6  =[ $Statak6,  necro2_attack7  ] {ai_face();};
void() necro2_attack7  =[ $Statak7,  necro2_attack8  ] {ai_face();};
void() necro2_attack8  =[ $Statak8,  necro2_run1     ] {ai_face();};

float() necro4_gangup =
{
    local   entity  head;

    head = findradius (self.origin, 120);
    while (head)
    {
        if (head.enemy == self)
            if (self.enemy != head)
                if (CanDamage (head, self))
                    return TRUE;
        head = head.chain;
    }
    return FALSE;
};
void() necro3_attack1  =[ $Hatak1,  necro3_attack2  ] {ai_face();};
void() necro3_attack2  =[ $Hatak2,  necro3_attack3  ] {ai_face();};
void() necro3_attack3  =[ $Hatak4,  necro3_attack4  ] {ai_face();
    if (InMeleeRange (80))
        if (!(self.enemy.flags & (FL_FLY | FL_SWIM)))
        {
            if (necro_boss ())
            {   // Boss escapes instead if ganged-up by more enemies.
                if (necro4_gangup ())
                    if (sorcerer_tele ())
                        return;
            }
            ai_repel ();
            return;
        }
// Shoving won't work, so zap the target instead.
    if (necro_boss ())
    {   // Update:  Boss may teleport away instead.
        if (sorcerer_tele ())
            return;
    }
    warlock_fire('12 9 27');
};
void() necro3_attack4  =[ $Hatak6,  necro3_attack5  ] {ai_face();};
void() necro3_attack5  =[ $Hatak8,  necro3_run1     ] {ai_face();
    if (necro_encore ())
        warlock_fire('14 8 27');
};
void() necro3_melee =
{
// If necro is not close enough to repel target, he will zap the enemy
// continuously with bolts, which is a bit overkill.  Instead, charge
// most of the time.
    if (!InMeleeRange (80))
        if (random() < 0.6)     // 40% chance per frame to attack.
            {ai_charge (16);  return;}
    necro3_attack1 ();
};

void() necro_activate_special =
{
    local   float   wtype;
    local   float   loop;

    wtype = self.worldtype;
    if (!wtype)
        return;

    if (wtype == NSA_CURSE)
    {   // Curses have a duration of ten seconds.
        if (self.enemy.flags & FL_MONSTER)
        {   // Special cases for some monsters.
            if (self.enemy.xfl & XFL_ZOMBIE)
            {   // Supress zombie immortality so any damage can kill zombies.
                T_Curse (self.enemy, self, CURSE_ARMOR, 10);
                return;
            }
            // If target is mad at a friend of necro, try to redirect its rage.
            if (Align_Match (self.enemy.enemy, self))
                if (T_Confuse (self.enemy, self))
                    return;
        }
        // Invoke one of several nasty status effects.
        T_Curse (self.enemy, self, 0, 10);
        return;
    }

    if (wtype == NSA_SUMMON_GOLEM)
    {   // If necro cannot summon a golem near an enemy, summon one near
        // himself.  High-flying targets often prevent telebomb summoning. 
        if (!SpawnMonster_Attack (self, ID_GOLEM_KNIGHT))
            SpawnMonster (self, self.origin, ID_GOLEM_KNIGHT, EntryPoint_Fan);
        return;
    }

    if (wtype == NSA_SUMMON_IMPS)
    {
        loop = NECRO_MAX_IMPS - count_petclass(self, "monster_imp");
        if (loop > self.height)
            loop = self.height;
        SpawnMonster_Horde (ID_IMP_KINN, loop);
        return;
    }

    if (wtype == NSA_CORPSE_EXPLOSION)
    {
        if (self.fallen.th_gib)
        if (self.fallen.deadflag == DEAD_DEAD)
        {
            local   entity  swap;
            local   float   sfx;
    
            sfx = random() * 4 & 3;
    
            swap = self;
            self = self.fallen;
            self.health = self.gib_health - 1;
            if (self.th_gib (-50))
            {
                local   float   damage, rgb;
    
                rgb = ((fabs(self.bloodtype) / 16) & 15) * 16;
                damage = self.max_health * (random() * 0.4 + 0.6) * 0.2;    //120
                Tent_Explosion2 (self.origin, rgb, 16);
                if (sfx == 1)
                    sound (self, CHAN_AUTO, "necro/pop1.wav", 1, ATTN_NORM);
                else if (sfx == 2)
                    sound (self, CHAN_AUTO, "necro/pop2.wav", 1, ATTN_NORM);
                else if (sfx == 3)
                    sound (self, CHAN_AUTO, "necro/pop3.wav", 1, ATTN_NORM);
                else
                    sound (self, CHAN_AUTO, "necro/pop4.wav", 1, ATTN_NORM);
                T_RadiusDamage_II (self, swap, damage, self, DF_FIRE|DF_ROCKET|DF_SAFE, "corpse", CORPSE_EXPLOSION_RADIUS, 0);
            }
            self = swap;
        }
        self.fallen = world;
        self.worldtype = 0;
        return;
    }
};

void() necro4_attack =
{
    local   entity  head;
    local   float   safe, yes;

    self.worldtype = 0;
    safe = !InMeleeRange(80);

// Check for cursing.
    if (safe)
    {
        if (self.height >= NECRO_SORCERER)      // Not in acolyte mode.
        if (!self.enemy.curse_type)
        if (random() < 0.5)
        {
            self.worldtype = NSA_CURSE;
            sound (self, CHAN_AUTO, "necro/curse.wav", 1, ATTN_NORM);
            necro_cast ();
            return;
        }
    }
// Check for corpse explosion.
    if (safe)
    {
        if (self.pets >= self.height)
            yes = 1;
        else if (self.pets)
            yes = 0.75;
        else
            yes = 0.5;
    }
    else
        yes = 0.25;
    if (random() < yes)
    {
        if (self.height >= NECRO_SORCERER)      // Not in acolyte mode.
        {
            self.fallen = world;
            head = QC_FindRadius (self.enemy.origin, CORPSE_EXPLOSION_RADIUS);
            while (head)
            {
                if (head.th_gib)
                if (head.deadflag == DEAD_DEAD)
                if (infront (head))
                {
                    traceline (self.origin + self.view_ofs, head.origin, TRUE, self);
                    if (trace_fraction == 1)
                    {
                        if (CanDamage (self.enemy, head))
                        {   // Seek the corpse with the highest max health,
                            // since damage is based on max health.
                            if (head.max_health > self.fallen.max_health)
                                self.fallen = head;
                        }
                    }
                }
                head = head.chain;
            }
            if (self.fallen)
            {
                self.worldtype = NSA_CORPSE_EXPLOSION;
                sound (self, CHAN_AUTO, "necro/pop.wav", 1, ATTN_NORM);
                necro_cast ();
                return;
            }
        }
    }
// Check for summoning.
    if (safe)
    {
        if (random() < 0.75)
        {
        // Check for golem.  Do only in warlock mode.
            if (self.height == NECRO_WARLOCK)
            {
                yes = 1;
                head = find(world, classname, "monster_golem");
                while (head)
                {
                    if (head.master == self)
                    {
                        if (head.deadflag < DEAD_DEAD)
                        {
                            yes = 0;   // Already have a golem out.
                            if (self.enemy)
                            if (!IsDead (head))
                            if (!visible (head))
                            if (!visible2 (self.enemy, head))
                            {   // Unsummon current, and summon new one.
                                T_Kill (head, self, self, head.gib_health - 1, "impulse");
                                if (head.health <= 0)   // Confirm destruction.
                                    self.worldtype = NSA_SUMMON_GOLEM;
                            }
                        }
                    }
                    if (yes)
                        head = find(head, classname, "monster_golem");
                    else
                        head = world;
                }
                if (yes)
                    self.worldtype = NSA_SUMMON_GOLEM;
            }
        // Check for imps.
            if (!self.worldtype)
            {
                if (random() < 0.75)
                {
                    if (count_petclass(self, "monster_imp") < 2)    //self.height)
                        self.worldtype = NSA_SUMMON_IMPS;
                }
            }
        // Now see if necro will summon something.
            if (self.worldtype)
            {
                sound (self, CHAN_AUTO, "necro/summon.wav", 1, ATTN_NORM);
                necro_cast ();
                return;
            }
        }
    }
// Just blast the enemy.
    if (self.height == NECRO_ACOLYTE)
        necro1_attack1 ();
    else if (self.height == NECRO_SORCERER)
        necro2_attack1 ();
    else
        necro3_attack1 ();
};

// PAIN - - - - - - - - - - - - - - - -
void(void() skip) necro_skip = {if (random() < 0.5) skip();};

// Check for an escape during pain.
void() necro_telepain =
{
    if (necro_boss ())
    {
        if (self.enemy)
        {
            if ((random() < 0.25) || (self.h2otype <= CONTENT_SLIME))
                necro_teleport (self.enemy, self.origin, TRUE);
        }
    }
};

void() necro1_pain1  =[ $Sipain1,  necro1_pain2  ] {necro_skip(necro1_pain2);};
void() necro1_pain2  =[ $Sipain2,  necro1_pain3  ] {};
void() necro1_pain3  =[ $Sipain3,  necro1_pain4  ] {};
void() necro1_pain4  =[ $Sipain4,  necro1_pain5  ] {};
void() necro1_pain5  =[ $Sipain5,  necro1_pain6  ] {necro_skip(necro1_pain6);};
void() necro1_pain6  =[ $Sipain6,  necro1_pain7  ] {};
void() necro1_pain7  =[ $Sipain7,  necro1_pain8  ] {};
void() necro1_pain8  =[ $Sipain8,  necro1_run1   ] {necro_run_check();};

void() necro2_pain1  =[ $Stpain1,  necro2_pain2  ] {necro_skip(necro2_pain2);};
void() necro2_pain2  =[ $Stpain2,  necro2_pain3  ] {};
void() necro2_pain3  =[ $Stpain3,  necro2_pain4  ] {necro_skip(necro2_pain4);};
void() necro2_pain4  =[ $Stpain4,  necro2_pain5  ] {};
void() necro2_pain5  =[ $Stpain5,  necro2_pain6  ] {necro_skip(necro2_pain6);};
void() necro2_pain6  =[ $Stpain6,  necro2_pain7  ] {};
void() necro2_pain7  =[ $Stpain7,  necro2_pain8  ] {necro_skip(necro2_pain8);};
void() necro2_pain8  =[ $Stpain8,  necro2_run1   ] {necro_telepain();};

void() necro3_pain1  =[ $Hpain2,  necro3_pain2  ] {};
void() necro3_pain2  =[ $Hpain3,  necro3_pain3  ] {necro_skip(necro3_pain3);};
void() necro3_pain3  =[ $Hpain4,  necro3_pain4  ] {};
void() necro3_pain4  =[ $Hpain6,  necro3_pain5  ] {};
void() necro3_pain5  =[ $Hpain7,  necro3_pain6  ] {necro_skip(necro3_pain6);};
void() necro3_pain6  =[ $Hpain8,  necro3_run1   ] {necro_telepain();};

// Named (foo)switch as C's switch.
void() necro_painswitch =
{
    if (self.height == NECRO_ACOLYTE)
        necro1_pain1 ();
//    else if (( self.frame >= $Siatak1 ) && ( self.frame <= $Siwait12 ))
//        necro1_pain1 ();
    else if (self.height == NECRO_SORCERER)
        necro2_pain1 ();
    else //if (self.height == NECRO_WARLOCK)
        necro3_pain1 ();
};

// Check for weapon breakage and update ai accordingly.
// As weapon breaks, the necro uses stronger moves.  Only boss should use.
float() Necro_WeaponBroke =
{
    local   vector  org, vel, spin;
    local   float   head, shaft;

// Check which parts need to break.
    head = shaft = FALSE;
    if (self.height < NECRO_WARLOCK)
    {
        if (self.health <= self.max_health / 2)
            shaft = TRUE;
        if (self.height < NECRO_SORCERER)
        {
            if (self.health <= self.max_health * 5 / 6)
                head = TRUE;
            else if (!shaft)
                return FALSE;   // Nothing broke.
        }
        else if (!shaft)
            return FALSE;   // Nothing broke.
    }
    else
        return FALSE;   // Weapon already broke!

    makevectors (self.angles);

// Sickle head.
    if (head)
    {
        sound (self, CHAN_AUTO, "misc/metalbrk.wav", 1, ATTN_NORM);
    
        vel = VelocityForDamage (0);
        spin_x = random()*600;
        spin_y = random()*600;
        spin_z = random()*600;
        org = self.origin + v_forward*17 + v_right*20 + v_up*31;    // '16.57 19.78 30.68'
        Gib_Spawn (org, "progs/nec_rod1.mdl", vel, spin, 0, 0, 0, TRUE);
        if (!shaft)
        {
            self.height     = NECRO_SORCERER;
            self.th_stand   = necro2_stand1;
            self.th_walk    = necro2_walk1;
            self.th_run     = necro2_run1;
            self.th_missile = necro4_attack;
            self.th_melee   = necro4_attack;
        }
    }

// Shaft.
    if (shaft)
    {
        sound (self, CHAN_AUTO, "misc/woodbrk.wav", 1, ATTN_NORM);
    
        vel = VelocityForDamage (0);
        vel = vel + v_right*50;
        spin_x = random()*600;
        spin_y = random()*600;
        spin_z = random()*600;
        org = self.origin + v_forward*15 + v_right*10 + v_up*25;
        Gib_Spawn (org, "progs/nec_rod2.mdl", vel, spin, 0, 0, 0, TRUE);

        vel = VelocityForDamage (0);
        vel = vel - v_right*50;
        spin_x = random()*600;
        spin_y = random()*600;
        spin_z = random()*600;
        org = self.origin + v_forward*15 - v_right*7 + v_up*15;     // '14.12 9.67 24.54'
        Gib_Spawn (org, "progs/nec_rod3.mdl", vel, spin, 0, 0, 0, TRUE);
    
        self.height     = NECRO_WARLOCK;
        self.th_stand   = necro3_stand1;
        self.th_walk    = necro3_walk1;
        self.th_run     = necro3_run1;
        self.th_missile = necro4_attack;
        self.th_melee   = necro4_attack;
    }

    return TRUE;
};

void(entity attacker, float damage) necro_pain =
{
// Update:  Check for evolution/weapon breakage for boss necro.
    if (necro_boss ())
    if (Necro_WeaponBroke ())
    {   // Must go into pain for weapon breaking effects to look right.
        self.pain_finished = time + 1;
        sound (self, CHAN_VOICE, "necro/pain1.wav", 1, ATTN_NORM);
        necro_painswitch ();
        return;
    }
//- - - - - - - - -

    if (self.pain_finished > time)
        return;   // Don't make multiple pain sounds right after each other.

    sound (self, CHAN_VOICE, "necro/pain1.wav", 1, ATTN_NORM);
    self.pain_finished = time + 1;      // time + 0.5 is too short.

    if (random()*50 > damage)       // Was flat 40.
        return;         // Didn't flinch
    if (SUB_IgnorePain ())
        return;         // No pain anims in Nightmare.

// Will go into a pain anim, so turn off the shield if it is on.
    if (!self.mirror_finished)
        self.effects = self.effects - (self.effects & EF_BRIGHTFIELD);

    necro_painswitch ();

    attacker = attacker;    // Appease FrikQcc!
};

// DEATH - - - - - - - - - - - - - - - -
// Special decapitation code.
void() necro_decap1  =[ $decap2,  necro_decap2  ] {blood_spray('0 0 28', 1);};
void() necro_decap2  =[ $decap4,  necro_decap3  ] {blood_spray('-9 0 29', 2);};
void() necro_decap3  =[ $decap6,  necro_decap4  ] {blood_spray('-12 0 26', 1); monster_drop();};
void() necro_decap4  =[ $decap8,  necro_decap5  ] {};
void() necro_decap5  =[ $decap10, necro_decap6  ] {};
void() necro_decap6  =[ $decap12, necro_decap7  ] {};
void() necro_decap7  =[ $decap14, necro_decap8  ] {blood_spray('7 -1 24', -1);};
void() necro_decap8  =[ $decap16, necro_decap9  ] {ai_forward(4);};
void() necro_decap9  =[ $decap18, necro_decap10 ] {ai_forward(4);};
void() necro_decap10 =[ $decap19, necro_decap11 ] {ai_forward(1);};
void() necro_decap11 =[ $decap20, decap_end     ] {ai_forward(1);};
// Yes, use monster_remove to dispose of the body since the necro is a
// head gib.  The think function should be safe enough for body to use.
// Update:  decap_end used instead so body doesn't vanish instantly.

// Standard death functions.
void() necro_die1  =[ $deathA2,  necro_die2  ] {};
void() necro_die2  =[ $deathA4,  necro_die3  ] {};
void() necro_die3  =[ $deathA6,  necro_die4  ] {monster_drop();};
void() necro_die4  =[ $deathA8,  necro_die5  ] {};
void() necro_die5  =[ $deathA10, necro_die6  ] {};
void() necro_die6  =[ $deathA12, necro_die7  ] {};
void() necro_die7  =[ $deathA14, necro_die8  ] {};
void() necro_die8  =[ $deathA16, necro_die9  ] {ai_forward(4);};
void() necro_die9  =[ $deathA18, necro_die10 ] {ai_forward(4);};
void() necro_die10 =[ $deathA19, necro_die11 ] {ai_forward(1);};
void() necro_die11 =[ $deathA20, monster_end ] {ai_forward(1);};

float(float dm) necro_gib =
{
    if (self.health < self.gib_health)
    {
        sound (self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
        ThrowGib ("progs/gib1.mdl", dm);
        ThrowGib ("progs/gib2.mdl", dm);
        ThrowGib ("progs/gib3.mdl", dm);
        ThrowHead ("progs/h_necro.mdl", dm);
        return TRUE;
    }
    return FALSE;
};

void() necro_die =
{
// Note:  Wand may be a handgun in the final release, so don't let the
// sorcerer drop his wand.

// Now to our death animation...
//- - - - -
    if (Shattered ())
        return;
    if (Decapitated ())
    {
        monster_decap ('0 0 28', necro_decap1);
        return;
    }
//- - - - -
    if (necro_gib (self.health))
        return;

    if (random() < 0.5)
        sound (self, CHAN_VOICE, "necro/death1.wav", 1, ATTN_NORM);
    else
        sound (self, CHAN_VOICE, "necro/death2.wav", 1, ATTN_NORM);
    necro_die1 ();
};

// REVIVE - - - - - - - - - - - - - - -
void() necro_revive1  =[ $deathA19, necro_revive2  ] {};
void() necro_revive2  =[ $deathA18, necro_revive3  ] {};
void() necro_revive3  =[ $deathA16, necro_revive4  ] {};
void() necro_revive4  =[ $deathA14, necro_revive5  ] {};
void() necro_revive5  =[ $deathA12, necro_revive6  ] {};
void() necro_revive6  =[ $deathA10, necro_revive7  ] {};
void() necro_revive7  =[ $deathA8,  necro_revive8  ] {};
void() necro_revive8  =[ $deathA6,  necro_revive9  ] {};
void() necro_revive9  =[ $deathA4,  necro_revive10 ] {};
void() necro_revive10 =[ $deathA2,  necro_revive11 ] {};
void() necro_revive11 = {self.th_run();};

// OTHER - - - - - - - - - - - - - - - -
void(float hello) necro_sight =
{
    if (hello)
        sound (self, CHAN_VOICE, "necro/idle1.wav", 1, ATTN_NORM);
    else
        sound (self, CHAN_VOICE, "necro/sight1.wav", 1, ATTN_NORM);
};

void() necro_cache =
{
    precache_model ("progs/necro.mdl");
    precache_model ("progs/h_necro.mdl");
    // Removed "progs/blood.mdl" precache since most can get beheaded.
    precache_model ("progs/r_bolt.mdl");
    precache_model ("progs/smartbal.mdl");
    precache_model ("progs/w_spike.mdl");
    precache_model ("progs/nec_rune.mdl");
    // Removed "progs/drake/g_wand.mdl" because sorcs don't drop their wands anymore.

    precache_sound ("curse/x_armor.wav");
    precache_sound ("necro/att_a.wav");
    precache_sound ("necro/att_a2.wav");
    precache_sound ("necro/att_w.wav");
    precache_sound ("necro/death1.wav");
    precache_sound ("necro/death2.wav");
    precache_sound ("necro/idle1.wav");
    precache_sound ("necro/idle2.wav");
    precache_sound ("necro/pain1.wav");
    precache_sound ("necro/sight1.wav");
    precache_sound ("nemesant/frblfly.wav");
//    precache_sound ("plasma/explode.wav");
    precache_sound ("skullwiz/push.wav");
};

void() necro_boss_cache =
{
    necro_cache ();
    precache_model ("progs/nec_rod1.mdl");
    precache_model ("progs/nec_rod2.mdl");
    precache_model ("progs/nec_rod3.mdl");
    precache_model ("progs/drake/s_spark.spr");

    precache_sound ("curse/x_armor.wav");
    precache_sound ("curse/x_damage.wav");
    precache_sound ("curse/x_muddle.wav");
    precache_sound ("curse/x_slow.wav");
    precache_sound ("curse/x_weaken.wav");
    precache_sound ("misc/metalbrk.wav");
    precache_sound ("misc/woodbrk.wav");
    precache_sound ("necro/curse.wav");
    precache_sound ("necro/pop.wav");
    precache_sound ("necro/pop1.wav");
    precache_sound ("necro/pop2.wav");
    precache_sound ("necro/pop3.wav");
    precache_sound ("necro/pop4.wav");
    precache_sound ("necro/summon.wav");
};

//============
// --> QUAKED monster_necromancer (1 0 0) (-16 -16 -24) (16 16 40) Ambush
//============
void() monster_necromancer =
{
    local   float   nec, hp;

    if (self.classname == "monster_acolyte") {
        nec = NECRO_ACOLYTE;  hp = 150;
        if (self.deathstring == "") self.deathstring = " was humiliated by an Acolyte\n";
    } else if (self.classname == "monster_sorcerer") {
        if (self.deathstring == "") self.deathstring = " was bested by a Sorcerer\n";
        nec = NECRO_SORCERER;  hp = 300;
    } else if (self.classname == "monster_warlock") {
        if (self.deathstring == "") self.deathstring = " was no match for a Warlock\n";
        nec = NECRO_WARLOCK;  hp = 450;}
    } else {
        if (self.deathstring == "") self.deathstring = " was snuffed by a Necromancer\n";
        nec = 0;  hp = 1800;
    }

    if (!self.spawned)
    {
        if (noenemies)
            {remove (self); return;}
        necro_cache ();
        if (!nec)
        {
            necro_boss_cache ();
            imp_cache ();
            kolem_cache ();
        }
    }

    monster_form ("progs/necro.mdl", '-16 -16 -24', '16 16 40', hp, -40);
    self.mdl_head   = "progs/h_necro.mdl";     // Gremlin shenanigans.
    self.mass       = MASS_MEDIUM;

    if (nec)
    {
        if (nec == NECRO_ACOLYTE)
        {   // Acolyte
            self.th_stand   = necro1_stand1;
            self.th_walk    = necro1_walk1;
            self.th_run     = necro1_run1;
            self.th_missile = necro1_attack1;
            self.th_melee   = necro1_melee;
        }
        else if (nec == NECRO_SORCERER)
        {   // Sorcerer
            self.th_stand   = necro2_stand1;
            self.th_walk    = necro2_walk1;
            self.th_run     = necro2_run1;
            self.th_missile = necro2_attack1;
            self.th_melee   = necro2_attack1;
        }
        else //if (nec == NECRO_WARLOCK)
        {   // Warlock
            self.th_stand   = necro3_stand1;
            self.th_walk    = necro3_walk1;
            self.th_run     = necro3_run1;
            self.th_missile = necro3_attack1;
            self.th_melee   = necro3_melee;
        }
        self.th_revive  = necro_revive1;

        self.height     = nec;
        self.purse      = '1 0.5 0.5';  // max = 1, 50% for 1, 50% gem.
        self.purse_x    = 1 + nec;
    }
    else
    {   // Necromancer boss
        self.skin       = 1;
        self.classname  = "monster_necromancer";
        self.height     = NECRO_ACOLYTE;    // Start in acolyte mode.
        self.ryuflags   = RYU_ALERT;
        self.purse      = '20 1 0.5';
        self.xfl        = XFL_BOSS;

        self.th_stand   = necro1_stand1;
        self.th_walk    = necro1_walk1;
        self.th_run     = necro1_run1;
        self.th_missile = necro4_attack;    //necro1_attack1;
        self.th_melee   = necro1_melee;
    }
    self.th_pain    = necro_pain;
    self.th_die     = necro_die;
    //- - - - - - - -
    self.th_sight   = necro_sight;
    self.th_gib     = necro_gib;
    self.th_spawn   = monster_necromancer;

    self.family     = FAM_NECRO;
    self.waitmax    = 0;    // Teleport delay, similar to nemesant.
    if (soul_evil)
        monster_purse (self.purse_x, self.purse_y, self.purse_z);
    else
        self.purse      = '0 0 0';

    monster_start ();
};

void() monster_acolyte = {monster_necromancer ();};
void() monster_sorcerer = {monster_necromancer ();};
void() monster_warlock = {monster_necromancer ();};

// Coven of Ebony entities.
void() monster_fragwiz =
    {self.classname = "monster_acolyte"; monster_necromancer ();};
void() monster_sorceror =   // PM:  Spelled 'sorcerer' NOT 'sorceror'.
    {self.classname = "monster_sorcerer"; monster_necromancer ();};


//===========================/  END OF FILE  /===========================//
