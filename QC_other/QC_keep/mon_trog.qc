/*
============================================================================

NEMESANT
(Zerstorer's Troglodyte.  Also the original Qtest shal-rath.)

============================================================================
*/

$cd /progs/nemesant
$origin 0 0 24
$base base
$skin skin

$frame float1 float2 float3 float4 float5 float6 float7

$frame attack1 attack2 attack3 attack4 attack5 attack6 attack7 attack8
$frame attack9 attack10 attack11 attack12 attack13 attack14 attack15

$frame attackb1 attackb2 attackb3 attackb4 attackb5 attackb6 attackb7

$frame pain1 pain2 pain3 pain4 pain5 pain6

$frame death1 death2 death3 death4 death5 death6 death7 death8 death9
$frame death10 death11 death12 death13 death14 death15 death16 death17

$frame decap1 decap2 decap3 decap4 decap5 decap6 decap7 decap8 decap9
$frame decap10 decap11 decap12 decap13 decap14 decap15 decap16 decap17


//==========================================================================
//  Warp Magic

// Update 6/2/09:  Circles and Explosion moved to fx.qc.
// Used by multiple effects.

// Copied from Zerstorer code.
void() DoCircles = {
local   vector  startpos, drag, dragto;
local   float   delta;

// Update 6/6/10:  Fizzle check.
// Assumes the nemesant is the attacker.  If not, change the conditions!
    if (( self.owner.frame >= $pain1 ) || (self.owner.health < 1)) {
        // The spell fizzles!
        super_active = 0;
        sound (self, CHAN_WEAPON, SOUND_EMPTY, 1, ATTN_NORM);
        if (self.trigger_field)     // Remove the explosion entity.
            remove (self.trigger_field);
        remove (self);
        return;
    }

    delta = self.count * 3;
    if (self.dmg == 1) {
        startpos = self.origin + '0 0 24';
        setorigin (self, self.origin + '0 0 8');
    } else {
        startpos = self.origin - '0 0 24';
        setorigin (self, self.origin - '0 0 8');
    }
    DrawCircleXY(self.origin, 84 - delta, 105);
    DrawCircleXY(startpos, 64 - delta, 232);
    self.count = self.count - self.dmg;
    if (self.count == 6) {
        super_active = 0;   // Done spamming particles.
        remove (self);
        return;
    }
    if (self.count == 0)
        self.dmg = -1;
    if (vlen(self.enemy.origin - self.origin) <= 250) {
        dragto = self.origin;
        dragto_z = self.enemy.origin_z;
        drag = normalize (dragto - self.enemy.origin);
        drag = drag*250;
        self.enemy.velocity = self.enemy.velocity + drag;
    }
        
    self.nextthink = time + 0.12;
};

void() TrogWarpMagic = {
// Moved all explosion effects into a separate function so other code
// can use it.
    WarpExplosion ();

// Damage stuff from the circle magic.
    if (self.enemy) {
        T_RadiusDamage (self, self.owner, 120, self.owner, DAMAGEALL);
        if (vlen(self.enemy.origin - self.origin) <= 120) {
        //- - - - - - - - -
        // For those immune to splash damage, inflict minor damage.
        // This is not unlike a Doom2 arch-vile vs. cyberdemon matchup.
            if (self.enemy.xfl & XFL_FIREANGEL)
                if (self.enemy != self.owner)
                    T_Damage (self.enemy, self, self.owner, 20, DAMARMOR);
        //- - - - - - - - -
            if (self.enemy.mass && (self.enemy.mass <= MASS_SUPER))
            {
                self.enemy.flags = self.enemy.flags - (self.enemy.flags & FL_ONGROUND);
                self.enemy.velocity_x = (random() * 400) - 200;
                self.enemy.velocity_y = (random() * 400) - 200;
                self.enemy.velocity_z = (random() * 300) + 400;
                if (self.enemy.mass == MASS_SUPER)
                    self.enemy.velocity = self.enemy.velocity * 0.5;
            }
        }
    }
};

void() StartTrogMagic =
{
// ** Mutual exclusion for trog magic attack so that we
//    don't overflow space!
    super_active = 1;       // Global defined in defs*.qc.

    local   entity  Mcircle, Tboom;
    local   vector  spot;

// Particle circle drawing entity.
    spot = self.enemy.origin - '0 0 12';
    Mcircle = spawn();
    Mcircle.movetype   = MOVETYPE_NONE;
    Mcircle.solid      = SOLID_NOT;
    Mcircle.owner      = self;
    Mcircle.count      = 5;
    Mcircle.dmg        = 1;
    Mcircle.enemy      = self.enemy;
    Mcircle.touch      = SUB_Null;
    Mcircle.nextthink  = time + 0.01;
    Mcircle.think      = DoCircles;
//    setmodel (Mcircle, "");
//    setsize (Mcircle, '0 0 0', '0 0 0');        
    setorigin (Mcircle, spot);
    sound (Mcircle, CHAN_WEAPON, "nemesant/charge.wav", 1, ATTN_NORM);

// Explosion entity.
    spot = self.enemy.origin + '0 0 24';
    Tboom = spawn();
    Tboom.movetype   = MOVETYPE_NONE;
    Tboom.solid      = SOLID_NOT;
    Tboom.owner      = self;
    Tboom.enemy      = self.enemy;
//    Tboom.angles     = VEC_ORIGIN;
    Tboom.touch      = SUB_Null;
    Tboom.nextthink  = time + 1.1;
    Tboom.think      = TrogWarpMagic;
//    setmodel (Tboom, "");
//    setsize (Tboom, '0 0 0', '0 0 0');        
    setorigin (Tboom, spot);

// Link the two together.
    Mcircle.trigger_field = Tboom;
};


//==========================================================================
//  Normal Attacks

// Used by necromancer and nemesant to shove an enemy away.
// Note:  Skullwiz version use different velocity and damage methods.
void() ai_repel =
{
    if (!self.enemy)
        return;     // No one to attack.
    if (!InMeleeRange (120))
        return;
    if (!CanDamage (self.enemy, self))
        return;

    // Note:  Different than Armagon's because we ignore elevation.
    local   vector  dir;

    dir = self.enemy.origin;
    dir_z = self.origin_z;     // Only interested in xy-plane, so ignore z.
    dir = normalize(dir - self.origin);
    dir = dir * 1000;
    dir_z = 120;    // Add a little up to overcome normal gravity.
// Send the enemy flying.
    if (self.enemy.mass && (self.enemy.mass <= MASS_SUPER))
    {
        self.enemy.flags = self.enemy.flags - (self.enemy.flags & FL_ONGROUND);
        self.enemy.velocity = self.enemy.velocity + dir;
    }
// Do the damage.
    sound (self, CHAN_WEAPON, "skullwiz/push.wav", 1, ATTN_NORM);
    T_Damage (self.enemy, self, self, 20, DAMARMOR);
};

// Returns the point of one of its hands.
vector(float left) Nemesant_Hand =
{
    makevectors (self.angles);
    if (left)   // TRUE = use left hand, FALSE = use right hand.
        return (self.origin + (v_forward*16) - (v_right*10) + (v_up*30));
    return (self.origin - (v_forward*16) + (v_right*12) + (v_up*45));
};

// This sends a bolt of lightning at an enemy, similar to the shamblers'.
void() Nemesant_Bolt =
{
// Assume not in water and don't check for discharge.
    local   vector  p1, p2;

    p1 = Nemesant_Hand (TRUE);      // This will call makevectors as well.
    p2 = self.enemy.origin + '0 0 16';
    p2 = Maim_Line (p1, p2, 0, self.enemy, '0 0 0');
    p2 = p1 + p2*600;

    self.effects = self.effects | EF_MUZZLEFLASH;

    sound (self, CHAN_WEAPON, "weapons/lhit.wav", 1, ATTN_NORM);
    sound (self, CHAN_AUTO, "weapons/lstart.wav", 1, ATTN_NORM);
    LightningBolt2 (p1, p2, self, 10);
};

//- - - - - - - - -
// Magic missile sidewinding tracking code.
void() nemes_track =
{
    if (self.delay <= time)
        {remove (self); return;}

    local   vector  vec;    // Multi-purpose vector.
    local   float   up, old;

    up = (self.cnt & 7) * 45;
    up = SUB_sin(up) * 5;       // Other sidewinder code uses cosine.
    self.cnt = self.cnt + 1;

    vec = self.enemy.origin + self.enemy.view_ofs;
//    if (1)
//        Sidewind_Go (vec + ('0 0 4' * up), self.speed);
//    else
    {   // Alternate sidewinding code for more controlled spiraling.
        vec = vectoangles(vec - self.origin);
    //- - - - - - - - - -
        old = self.angles_y;
        self.ideal_yaw = vec_x + up;
        self.angles_y = self.angles_x;
        ChangeYaw ();
        self.angles_x = self.angles_y;
        self.angles_y = old;
    //- - - - - - - - - -
        self.ideal_yaw = vec_y + self.lefty;
        ChangeYaw ();
        if (MatchingIdeal ())
            self.lefty = 0 - self.lefty;

        vec = self.angles;
        vec_x = 0 - vec_x;
        makevectors (vec);
        self.velocity = v_forward * self.speed;
    }

    self.nextthink = time + 0.1;
};

// Spawns each magic missile launched by the nemesant.
void(float ofs, float up, vector vec) nemes_shot =
{
    hknight_shot(ofs);
    setorigin (newmis, vec);
    newmis.velocity  = normalize(newmis.velocity) * 600;
    if (self.enemy)
    {   // Setup for homing sidewinders.
        newmis.enemy     = self.enemy;
        newmis.cnt       = up;
        newmis.lefty     = ofs * 20;
        newmis.speed     = 450;
        newmis.yaw_speed = 20;
        newmis.delay     = newmis.nextthink;
        newmis.nextthink = time + 0.1;
        newmis.think     = nemes_track;
    }
};

// Shoots a salvo of magic missiles at an enemy.
void() Nemesant_Missiles =
{
    local   vector  p1;

    p1 = Nemesant_Hand (TRUE);
    self.effects = self.effects | EF_MUZZLEFLASH;
// Three shots, one per finger.  Left, center, and right.
// Note:  2nd parm -- 0,2,4 for sin.  6,0,2 for cos.
    nemes_shot(-1,0,p1); nemes_shot(0.5,2,p1); nemes_shot(1,4,p1);
};
//- - - - - - - - -

// Lobs its trademark bouncing seeker fireball.
// In Drake, more monsters copy and use this attack.
void() Nemesant_FireBall = {M_FireSmart ('16 12 45');};


//==========================================================================
//  Movement and Teleportation

// PM:  Teleport code mostly rewritten from the original.
void() tele_attack =
{
    local   vector  org, delta;

    delta = self.enemy.origin - self.origin;
    if (vlen(delta) < 100)
        return;     // Too close to the enemy.

    org = self.origin;    // Remember this point for fog spawning.
    if (random() < 0.5)
    {   // Standard Zerstorer AI.
        local   vector  enor;

        // Distance was originally 70, but thats too short vs. fat monsters.
        // Dist = 70 vs. players, 90 vs. ogre/shambler, 110+ vs. even fatter.
        delta_z = 0;    // Ignore elevation difference.
        delta = normalize(delta) * (self.enemy.maxs_x * 1.25 + 50);
        enor = self.enemy.origin + '0 0 32';

        // Aim behind target, and if that fails, aim in front of target.
        if (!EntryPoint_Spawn (self, self.enemy, enor + delta, EntryPoint_One))
            if (!EntryPoint_Spawn (self, self.enemy, enor - delta, EntryPoint_One))
                return;
    }
    else
    {   // Custom -- teleport to random spot within range_near.
        if (!EntryPoint_Spawn (self, self, org, EntryPoint_Random))
            return;
    }
// Teleport is a success.
    self.angles_y = vectoyaw(self.enemy.origin - self.origin);
    if (!self.invisible_finished)
    {   // Fog only if visible.
        spawn_tfog (org);           // Before
        spawn_tfog (self.origin);   // After
    }
    // Zerstorer has after first, before second.

    // Replaced 'super_*' with 'waitmax' because monsters use powerups.
    self.waitmax = time + 2;
    self.attack_finished = 0;   // PM:  This lets mage attack now.
};

void() check_tele =
{
local   entity  head;

    if (!self.enemy)
        return;
    if (self.enemy.health <= 0)
        return;
    if (self.waitmax > time)
        return;

    head = findradius(self.origin, 150);
    while (head)
    {
//- - - - - - - - -
        // Update:  Drake uses self.master for true ownership, since
        // reflection code changes owner when used.
        if (head.master == self.enemy)
            if (IsMissile (head))
                {tele_attack(); return;}
//- - - - - - - - -
        head = head.chain;
    }
};


//==========================================================================
//  Frames

void() nemes_idlesound =
{
    if (self.waitmin < time)
    {
        local   float   wr;

    // Use voice channel instead of auto -- don't want slain magi talking.
        wr = random();
        if (wr > 0.7) 
            sound (self, CHAN_VOICE, "nemesant/tidle1.wav", 1, ATTN_NORM);
        else if (wr < 0.3)
            sound (self, CHAN_VOICE, "nemesant/tidle2.wav", 1, ATTN_NORM);
        self.waitmin = time + 10;
    }
};

// STAND - - - - - - - - - - - - - - - -
void() nemes_stand1  =[ $float1,  nemes_stand2  ] {ai_stand();
    if (!(self.spawnflags & 1))     // Chant ominously if not in ambush mode.
        nemes_idlesound();
};
void() nemes_stand2  =[ $float2,  nemes_stand3  ] {ai_stand();};
void() nemes_stand3  =[ $float3,  nemes_stand4  ] {ai_stand();};
void() nemes_stand4  =[ $float4,  nemes_stand5  ] {ai_stand();};
void() nemes_stand5  =[ $float5,  nemes_stand6  ] {ai_stand();};
void() nemes_stand6  =[ $float6,  nemes_stand7  ] {ai_stand();};
void() nemes_stand7  =[ $float7,  nemes_stand1  ] {ai_stand();};

// WALK - - - - - - - - - - - - - - - -
void() nemes_walk1  =[ $float1,  nemes_walk2  ] {ai_walk(8); nemes_idlesound();};
void() nemes_walk2  =[ $float2,  nemes_walk3  ] {ai_walk(8);};
void() nemes_walk3  =[ $float3,  nemes_walk4  ] {ai_walk(8);};
void() nemes_walk4  =[ $float4,  nemes_walk5  ] {ai_walk(8);};
void() nemes_walk5  =[ $float5,  nemes_walk6  ] {ai_walk(8);};
void() nemes_walk6  =[ $float6,  nemes_walk7  ] {ai_walk(8);};
void() nemes_walk7  =[ $float7,  nemes_walk1  ] {ai_walk(8);};

// RUN - - - - - - - - - - - - - - - - -
void() nemes_side1  =[ $float1,  nemes_side2  ] {ai_run(8); nemes_idlesound(); check_tele();};
void() nemes_side2  =[ $float2,  nemes_side3  ] {ai_run(8);};
void() nemes_side3  =[ $float3,  nemes_side4  ] {ai_run(8); check_tele();};
void() nemes_side4  =[ $float4,  nemes_side5  ] {ai_run(8);};
void() nemes_side5  =[ $float5,  nemes_side6  ] {ai_run(8); check_tele();};
void() nemes_side6  =[ $float6,  nemes_side7  ] {ai_run(8);};
void() nemes_side7  =[ $float7,  nemes_side1  ] {ai_run(8); check_tele();};

void() nemes_run1  =[ $float1,  nemes_run2  ] {ai_run(16); nemes_idlesound(); check_tele();};
void() nemes_run2  =[ $float2,  nemes_run3  ] {ai_run(16);};
void() nemes_run3  =[ $float3,  nemes_run4  ] {ai_run(16); check_tele();};
void() nemes_run4  =[ $float4,  nemes_run5  ] {ai_run(16);};
void() nemes_run5  =[ $float5,  nemes_run6  ] {ai_run(16); check_tele();};
void() nemes_run6  =[ $float6,  nemes_run7  ] {ai_run(16);};
void() nemes_run7  =[ $float7,  nemes_run1  ] {ai_run(16); check_tele();};

// ATTACK - - - - - - - - - - - - - - -
void() TrogAttackFinished =
{
    if (self.attack_finished)   // Repel hack -- set at zero bypasses delay.
        SUB_AttackFinished (2);
    if (enemy_range >= RANGE_MID || !enemy_vis)
        {self.attack_state = AS_STRAIGHT; self.think = nemes_run1;}
    else
        {self.attack_state = AS_SLIDING; self.think = nemes_side1;}
};

void() nemes_fast1  =[ $attackb1,  nemes_fast2  ] {ai_face();};
void() nemes_fast2  =[ $attackb2,  nemes_fast3  ] {ai_face();};
void() nemes_fast3  =[ $attackb3,  nemes_fast4  ] {ai_face();};
void() nemes_fast4  =[ $attackb4,  nemes_fast5  ] {ai_face();};
void() nemes_fast5  =[ $attackb5,  nemes_fast6  ] {
    local   float   dist, boss, mask;

    ai_face();
    sound (self, CHAN_VOICE, "nemesant/frblatck.wav", 1, ATTN_NORM);
    if (self.waitmin < time + 1) self.waitmin = time + 1;

    // The mage has more options when not bound by Zerstorer mode.
    dist = vlen(self.enemy.origin - self.origin);
    //zot = FALSE;

    // Update 6/6/10:  Only the boss version can shoot lightning.
    boss = FALSE;
    if (self.classname != "monster_nemesant") boss = TRUE;

    //if (boss)
        //if (IsGhost (self.enemy))
        //    if ( !((dist > 600) || (self.h2olevel == 3)) )
        //        zot = TRUE;

    // If TRUE, Only lightning is effective, at least in Drake.
    //if (zot) {
    //Nemesant_Bolt ();
    //self.nextthink = time + 0.2;
    //} else {
        // Use standard book of tricks.
        if ((dist <= 120) && (self.enemy.flags & FL_ONGROUND)) {
            // Try to push enemy away from melee.    
            ai_repel ();
            self.attack_finished = 0;   // Don't delay for next attack.
        } else {
            // Use pattern like Dracula in Castlevania: SotN.
            // Launch three fireballs then magic missiles.
            // Update:  Attack counter is shared by all.
            mask = AddOne() & 3;
            if (mask == 3) {
                // Use magic missiles sometimes if enemy is not immune.
                if (!(self.enemy.armortype == ARMOR_RED_TYPE)) {
                    Nemesant_Missiles ();
                    return;
                }
                    } else if (!mask && boss) {
                // The boss gets to shoot lightning every fourth shot.
                        // Must be inrange and out of the water.
                if ( !((dist > 600) || (self.waterlevel == 3)) ) {
                    Nemesant_Bolt ();
                    self.nextthink = time + 0.2;
                    return;
                }
            }
            Nemesant_FireBall ();   // The original.
        }
    //}
};
void() nemes_fast6  =[ $attackb6,  nemes_fast7  ] {ai_face();};
void() nemes_fast7  =[ $attackb2,  nemes_fast8  ] {ai_face();};
void() nemes_fast8  =[ $attackb1,  nemes_run1   ] {ai_face(); TrogAttackFinished();};

// FIXME:  Merge spark functions.  Possibly with the yakmens' too.
void(vector st) nemes_sparks =
{
    local   vector  side;

    makevectors (self.angles);
    side = v_right * st_y;
    st = self.origin + v_forward*st_x + v_up*st_z;
    particle (st + side, '0 0 50', 145, 10);
    particle (st - side, '0 0 50', 145, 10);
};
void() nemes_summon1  =[ $attack1,  nemes_summon2  ] {ai_face(); nemes_sparks('0 35 22');};
void() nemes_summon2  =[ $attack3,  nemes_summon3  ] {ai_face(); nemes_sparks('-2 31 15');};
void() nemes_summon3  =[ $attack5,  nemes_summon4  ] {ai_face(); nemes_sparks('-14 30 15');};
void() nemes_summon4  =[ $attack7,  nemes_summon5  ] {ai_face(); nemes_sparks('-15 32 25');};
void() nemes_summon5  =[ $attack9,  nemes_summon6  ] {ai_face(); nemes_sparks('-15 31 28');};
void() nemes_summon6  =[ $attack10, nemes_summon7  ] {ai_face(); nemes_sparks('12 27 30');};
void() nemes_summon7  =[ $attack11, nemes_summon8  ] {ai_face(); nemes_sparks('22 3 30');
// DISABLED -- Vores as part of this boss fight is harsh.
//    if (!self.pets)
//    if (random() < 0.5)
//    if (self.health < (self.max_health / 2))
//    if (SpawnMonster (self, self.origin, ID_SHALRATH_CLASSIC, EntryPoint_Fan))
//        return;

    //if (self.pets < 3)
    //    SpawnMonster_Horde (ID_WIZARD_SCRAG, 3);
    //else
        //SpawnMonster_Horde (CT_MONWIZARD, 2);
};
void() nemes_summon8  =[ $attack13, nemes_summon9  ] {ai_face();};
void() nemes_summon9  =[ $attack14, nemes_summon10 ] {ai_face();};
void() nemes_summon10 =[ $attack15, nemes_run1     ] {ai_face(); TrogAttackFinished();};

void(vector st) nemes_sparks2 =
{
    local   vector  side;

    makevectors (self.angles);
    side = v_right * st_y;
    st = self.origin + v_forward*st_x + v_up*st_z;
    particle (st + side, '0 0 50', 241, 10);
    particle (st - side, '0 0 50', 241, 10);
};
void() nemes_inviso1  =[ $attack1,  nemes_inviso2  ] {ai_face(); nemes_sparks2('0 35 22');};
void() nemes_inviso2  =[ $attack3,  nemes_inviso3  ] {ai_face(); nemes_sparks2('-2 31 15');};
void() nemes_inviso3  =[ $attack5,  nemes_inviso4  ] {ai_face(); nemes_sparks2('-14 30 15');};
void() nemes_inviso4  =[ $attack7,  nemes_inviso5  ] {ai_face(); nemes_sparks2('-15 32 25');};
void() nemes_inviso5  =[ $attack9,  nemes_inviso6  ] {ai_face(); nemes_sparks2('-15 31 28');};
void() nemes_inviso6  =[ $attack10, nemes_inviso7  ] {ai_face(); nemes_sparks2('12 27 30');};
void() nemes_inviso7  =[ $attack11, nemes_inviso8  ] {ai_face(); nemes_sparks2('22 3 30');
    sound (self, CHAN_VOICE, "nemesant/frblatck.wav", 1, ATTN_NORM);
    spawn_tfog (self.origin);
    self.invisible_finished = time + 10;
};
void() nemes_inviso8  =[ $attack13, nemes_inviso9  ] {ai_face();};
void() nemes_inviso9  =[ $attack14, nemes_inviso10 ] {ai_face();};
void() nemes_inviso10 =[ $attack15, nemes_run1     ] {ai_face(); TrogAttackFinished();};

void() nemes_sboom1  =[ $attack1,  nemes_sboom2  ] {ai_face();};
void() nemes_sboom2  =[ $attack2,  nemes_sboom3  ] {ai_face();};
void() nemes_sboom3  =[ $attack3,  nemes_sboom4  ] {ai_face();};
void() nemes_sboom4  =[ $attack4,  nemes_sboom5  ]
{   ai_face();
    if (super_active)  // Someone else started it first -- use alt. instead.
        {self.think = nemes_fast1;  return;}
    StartTrogMagic();
};
void() nemes_sboom5  =[ $attack5,  nemes_sboom6  ] {ai_face();};
void() nemes_sboom6  =[ $attack6,  nemes_sboom7  ] {ai_face();};
void() nemes_sboom7  =[ $attack7,  nemes_sboom8  ] {ai_face();};
void() nemes_sboom8  =[ $attack8,  nemes_sboom9  ] {ai_face();};
void() nemes_sboom9  =[ $attack9,  nemes_sboom10 ] {ai_face();};
void() nemes_sboom10 =[ $attack10, nemes_sboom11 ] {ai_face();};
void() nemes_sboom11 =[ $attack11, nemes_sboom12 ] {ai_face();};
void() nemes_sboom12 =[ $attack12, nemes_sboom13 ] {ai_face();};
void() nemes_sboom13 =[ $attack13, nemes_sboom14 ] {ai_face();};
void() nemes_sboom14 =[ $attack14, nemes_sboom15 ] {ai_face();};
void() nemes_sboom15 =[ $attack15, nemes_run1    ] {ai_face(); TrogAttackFinished();};

void() Nemesant_Attack =
{
// Boss version has other tricks it can use.
    if (self.xfl & XFL_BOSS)
    if (random() < 0.5)
    {
        // If hurt badly enough, cast invisibility sometimes.
        if (!self.invisible_finished)
            if (self.health <= self.max_health / 2)
                if (random() < 0.5)
                    {nemes_inviso1 ();  return;}
        // Summon monsters to fight.
        if (self.count <= 1)
            {nemes_summon1 ();  return;}
    }
//- - - - - - - - -
// Use quick attack instead of super attack if...
// 1) Someone is using super attack now (only one at a time, please).
// 2) Enemy is off the ground (player gets sucked in unfairly).
// 3) Enemy is invisible (mage cannot pinpoint enemy).
//  NOTE:  Zerstorer didn't check for invisibility.
//
// If super attack is available, always use it if nothing else can hurt
// the enemy, which includes ghosts and zombies.
//
// Assuming no special conditions, chances of attack paths are as follows.
//  70% chance of A -- fast magic
//  30% chance of B -- super magic
//- - - - - - - - -
    local   float   fast;

    if ((random() < 0.7) || !(self.enemy.flags & FL_ONGROUND)
    || self.enemy.form_active || super_active
    || (self.enemy.items & IT_INVISIBILITY) || self.enemy.invisible_finished)
    {
        fast = 1;
        // Allow exceptions if nothing else is effective.
        if (!super_active)
        {
            if (self.enemy.classname == "monster_zombie")
                fast = 0;
            //else if (IsGhost (self.enemy))
            //    fast = 0;
            // FIXME:  Should we add reflection too?
        }
    }
    else
        fast = 0;

    if (fast)
        nemes_fast1 ();
    else
        nemes_sboom1 ();
};

// PAIN - - - - - - - - - - - - - - - -
void() nemes_pain1  =[ $pain1,  nemes_pain2  ] {};
void() nemes_pain2  =[ $pain2,  nemes_pain3  ] {};
void() nemes_pain3  =[ $pain3,  nemes_pain4  ] {};
void() nemes_pain4  =[ $pain4,  nemes_pain5  ] {};
void() nemes_pain5  =[ $pain5,  nemes_pain6  ] {};
void() nemes_pain6  =[ $pain6,  nemes_run1   ] {};

void(entity inflictor, entity attacker, float damage) nemes_pain =
{
    if (self.pain_finished > time)
        return;
    if (( self.frame >= $attack1 ) && ( self.frame <= $attack15 ))
    {   // Casting the big spell, so be resistant to pain like the arch-vile.
        if (prandom() > 10)
            return;     // Failed to break its concentration.
    }

    self.pain_finished = time + 0.5;

    sound (self, CHAN_VOICE, "nemesant/tpain.wav", 1, ATTN_NORM);
    if (random()*70 > damage)
        return;         // didn't flinch
    if (SUB_IgnorePain ())
        return;         // No pain anims in Nightmare.
    if (self.classname != "monster_nemesant")
    {   // Make boss versions harder to flinch and stunlock.
        if (self.lip > time)
            return;     // Too soon to flinch.
        self.lip = time + 2;
    }

    self.attack_finished = 0;   // So mage can counterattack after recovery.
    nemes_pain1 ();
    attacker = attacker;    // FrikQcc warning squelch!
};

// DEATH - - - - - - - - - - - - - - - -
void() nemes_decap1  =[ $decap1,  nemes_decap2  ] {blood_spray('0 0 28', 1);
    self.velocity_x = -200 + 400*random();
    self.velocity_y = -200 + 400*random();
    self.velocity_z = 100 + 100*random();
    self.flags = self.flags - (self.flags & FL_ONGROUND);
};
void() nemes_decap2  =[ $decap2,  nemes_decap3  ] {blood_spray('-4 -1 27', 2);};
void() nemes_decap3  =[ $decap3,  nemes_decap4  ] {blood_spray('-6 -1 26', 1); /*monster_drop();*/};
void() nemes_decap4  =[ $decap4,  nemes_decap5  ] {blood_spray('-8 -1 25', 1);};
void() nemes_decap5  =[ $decap5,  nemes_decap6  ] {blood_spray('-11 0 23', 1);};
void() nemes_decap6  =[ $decap6,  nemes_decap7  ] {};
void() nemes_decap7  =[ $decap7,  nemes_decap8  ] {};
void() nemes_decap8  =[ $decap8,  nemes_decap9  ] {};
void() nemes_decap9  =[ $decap9,  nemes_decap10 ] {};
void() nemes_decap10 =[ $decap10, nemes_decap11 ] {};
void() nemes_decap11 =[ $decap11, nemes_decap12 ] {};
void() nemes_decap12 =[ $decap12, nemes_decap13 ] {};
void() nemes_decap13 =[ $decap13, nemes_decap14 ] {};
void() nemes_decap14 =[ $decap14, nemes_decap15 ] {};
void() nemes_decap15 =[ $decap15, nemes_decap16 ] {};
void() nemes_decap16 =[ $decap16, nemes_decap17 ] {};
void() nemes_decap17 =[ $decap17, nemes_decap17 ] {};

void() nemes_death1  =[ $death1,  nemes_death2  ] {};
void() nemes_death2  =[ $death2,  nemes_death3  ] {};
void() nemes_death3  =[ $death3,  nemes_death4  ] {/*monster_drop();*/};
void() nemes_death4  =[ $death4,  nemes_death5  ] {};
void() nemes_death5  =[ $death5,  nemes_death6  ] {};
void() nemes_death6  =[ $death6,  nemes_death7  ] {};
void() nemes_death7  =[ $death7,  nemes_death8  ] {};
void() nemes_death8  =[ $death8,  nemes_death9  ] {};
void() nemes_death9  =[ $death9,  nemes_death10 ] {};
void() nemes_death10 =[ $death10, nemes_death11 ] {};
void() nemes_death11 =[ $death11, nemes_death12 ] {};
void() nemes_death12 =[ $death12, nemes_death13 ] {};
void() nemes_death13 =[ $death13, nemes_death14 ] {};
void() nemes_death14 =[ $death14, nemes_death15 ] {};
void() nemes_death15 =[ $death15, nemes_death16 ] {};
void() nemes_death16 =[ $death16, nemes_death17 ] {};
void() nemes_death17 =[ $death17, nemes_death17 ] {};

//float(float dm) nemes_gib =
//{
//    if (self.health < self.gibhealth)
//    {
//        sound (self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
    // Zerstorer spawned gib2.mdl * 3.
//        ThrowGib ("progs/gib1.mdl", dm);
//        ThrowGib ("progs/gib2.mdl", dm);
//        ThrowGib ("progs/gib3.mdl", dm);
//        ThrowHead ("progs/h_nemes.mdl", dm);
//        return TRUE;
//    }
//    return FALSE;
//};

void() nemes_die =
{
    //if (Shattered ())
    //    return;
    //if (Decapitated ())
    //{
    //    monster_decap ('0 0 28', nemes_decap1);
    //    return;
    //}
//- - - - -
// check for gib
    //if (self.th_gib)
    //    if (nemes_gib (self.health))
    //        return;

    self.velocity_x = -200 + 400*random();
    self.velocity_y = -200 + 400*random();
    self.velocity_z = 200 + 100*random();   // PM:  Used to be 300.
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    if (random() < 0.5)
        sound (self, CHAN_VOICE, "nemesant/tdeath1.wav", 1, ATTN_NORM);
    else
        sound (self, CHAN_VOICE, "nemesant/tdeath2.wav", 1, ATTN_NORM);
    nemes_death1 ();
};

// REVIVE - - - - - - - - - - - - - - -
void() nemes_revive1  =[ $death16, nemes_revive2  ] {self.flags = (self.flags - (self.flags & FL_ONGROUND)) | FL_FLY;};
void() nemes_revive2  =[ $death15, nemes_revive3  ] {};
void() nemes_revive3  =[ $death14, nemes_revive4  ] {};
void() nemes_revive4  =[ $death13, nemes_revive5  ] {};
void() nemes_revive5  =[ $death12, nemes_revive6  ] {};
void() nemes_revive6  =[ $death11, nemes_revive7  ] {};
void() nemes_revive7  =[ $death10, nemes_revive8  ] {};
void() nemes_revive8  =[ $death9,  nemes_revive9  ] {};
void() nemes_revive9  =[ $death8,  nemes_revive10 ] {};
void() nemes_revive10 =[ $death7,  nemes_revive11 ] {};
void() nemes_revive11 =[ $death6,  nemes_revive12 ] {};
void() nemes_revive12 =[ $death5,  nemes_revive13 ] {};
void() nemes_revive13 =[ $death4,  nemes_revive14 ] {};
void() nemes_revive14 =[ $death3,  nemes_revive15 ] {};
void() nemes_revive15 =[ $death2,  nemes_revive16 ] {};
void() nemes_revive16 =[ $death1,  nemes_run1     ] {};

// OTHER - - - - - - - - - - - - - - - -
void(float hello) nemes_sight =
{
    if (hello)
        sound (self, CHAN_VOICE, "nemesant/tpain.wav", 1, ATTN_NORM);
    else
    {
        if (random() < 0.5)
            sound (self, CHAN_VOICE, "nemesant/tsight1.wav", 1, ATTN_NORM);
        else
            sound (self, CHAN_VOICE, "nemesant/tsight2.wav", 1, ATTN_NORM);
    }

    if (self.waitmin < time + 2)
        self.waitmin = time + 2;
};

//============
// --> QUAKED monster_nemesant (1 0 0) (-16 -16 -24) (16 16 40) Ambush Boss
//============
void() monster_nemesant = {
    if (deathmatch) { remove(self); return; }
    
    self.mdl = "progs/drake/nemesant.mdl";
    self.headmdl = "progs/drake/h_nemes.mdl";
    precache_model ("progs/drake/nemesant.mdl");
    precache_model ("progs/drake/h_nemes.mdl");
    precache_model ("progs/k_spike.mdl");   // For sidewinder missiles.
    precache_model ("progs/drake/smartbal.mdl");  // Homing bouncing fireball.
    precache_model ("progs/drake/warp2.mdl");     // Big explosion.

    precache_sound ("hknight/attack1.wav");
    precache_sound ("hknight/hit.wav");    // used by C code, so don't sound2
    precache_sound ("nemesant/frblatck.wav");
    precache_sound ("nemesant/frblfly.wav");
    precache_sound ("nemesant/charge.wav");
    precache_sound ("nemesant/bigboom.wav");
    precache_sound ("nemesant/tdeath1.wav");
    precache_sound ("nemesant/tdeath2.wav");
    precache_sound ("nemesant/tidle1.wav");
    precache_sound ("nemesant/tidle2.wav");
    precache_sound ("nemesant/tpain.wav");
    precache_sound ("nemesant/tsight1.wav");
    precache_sound ("nemesant/tsight2.wav");
    precache_sound ("skullwiz/push.wav");       // Melee attack.
    precache_blood (BLOOD_GREEN);
    precache_model ("progs/drake/gr_gib2.mdl");
    precache_model ("progs/drake/s_wave.spr");
    precache_model ("progs/drake/s_expbig.spr");

    precache_model ("progs/wizard.mdl");
    precache_model ("progs/drake/h_drake.mdl");
    precache_model ("progs/h_wizard.mdl");
    precache_model ("progs/w_spike.mdl");

    precache_sound ("wizard/hit.wav");        // used by c code
    precache_sound ("wizard/wattack.wav");
    precache_sound ("wizard/wdeath.wav");
    precache_sound ("wizard/widle1.wav");
    precache_sound ("wizard/widle2.wav");
    precache_sound ("wizard/wpain.wav");
    precache_sound ("wizard/wsight.wav");

    self.solid = SOLID_NOT;        // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    if (self.health < 1) self.health = 280;
    self.gibhealth = -40;
    self.mass       = MASS_HEAVY;

    self.th_stand   = nemes_stand1;
    self.th_walk    = nemes_walk1;
    self.th_run     = nemes_run1;
    self.th_missile = Nemesant_Attack;
    self.th_pain    = nemes_pain;
    self.th_die     = nemes_die;
    //- - - - - - - -
    self.th_sight   = nemes_sight;
    self.th_spawn   = monster_nemesant;
    if (self.classname == "monster_nemesant") {
        //self.th_gib     = nemes_gib;
        self.th_revive  = nemes_revive1;
        //if (soul_evil)
        //    monster_purse (3, 0.5, 0.5);
    }
    // ELSE... Boss versions can neither be beheaded nor revived.
    //else
    //{
        //if (self.classname != "monster_cultlord")
            //self.th_gib     = nemes_gib;
        //if (soul_evil)
        //    monster_purse (10, 1, 0.5);  // 10 items, 50% gem.
    //}

    self.waitmin    = time + 5;     // Wait for idle sounds

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    self.classtype = CT_MONNEMESANT;
    self.classgroup = CG_WIZARD;
    self.classmove = MON_MOVEFLY;
    if ( (random () < 0.500) ) {
        if (self.deathstring == "") self.deathstring = " was buried by a Nemesant\n";
    } else {
        if (self.deathstring == "") self.deathstring = " was hexed by a Nemesant\n";
    }

    monster_start ();
};

// SoE entity.
void() monster_trogboss = {
    self.lip = 0;
    if (self.skin != 2)
        self.skin = 1;
    self.xfl  = XFL_BOSS;
    if (!self.health)
        self.health = 1600;

    if (self.deathstring == "") self.deathstring = " failed to impress the Master Nemesant\n";
    monster_nemesant ();
};

// Original classname from Zerstorer.
void() monster_troglodyte = {
    if (world.model == "maps/coe4.bsp")
        {self.classname = "monster_trogboss";  monster_trogboss();}
//    else if (world.model == "maps/e3secret1.bsp")
//        {self.classname = "monster_trogboss";  monster_trogboss();}
    else
        {self.classname = "monster_nemesant";  monster_nemesant();}
};

// Todo (maybe):  Add purple mind-flayer version that can rez bad guys.


//===========================/  END OF FILE  /===========================//
