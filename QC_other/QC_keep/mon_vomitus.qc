/*
==============================================================================

VOMITUS

==============================================================================
*/

$cd id1/models/vomitus
$origin 0 0 24
$base base        
$skin skin

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9 walk10
$frame walk11 walk12 walk13 walk14 walk15 walk16 walk17 walk18 walk19
$frame walk20 walk21 walk22 walk23 walk24 walk25

$frame run1 run2 run3 run4 run5 run6 run7 run8

$frame attack1 attack2 attack3 attack4 attack5 attack6 attack7 attack8
$frame attack9 attack10 attack11 attack12 attack13

$frame pain1 pain2 pain3

$frame death1 death2 death3 death4 death5 death6 death7 death8 death9
$frame death10 death11 death12 death13 death14 death15

//==========================================================================

// Vomitus attacks are placed here since they may be called during a
// non-attack animation scene.

// Spews forth a tarbaby.
void() Vomit_Spawn =
{
    local   entity  parent;

    parent = self;

    self = spawn();
    self.flags      = 0;
    // Owner set to vomitus so the tarbaby can pass through it if necessary.
    self.owner      = self.master   = parent;
    setorigin (self, parent.origin);
    self.spawned    = TRUE;
    self.spawnflags = parent.spawnflags & SPAWN_WHITE;
    self.classname  = "monster_tarbaby";
    // Monster is spew out of parent's yaw, so face away.
    self.angles_x   = 0;
    self.angles_y   = parent.angles_y;
    self.angles_z   = 0;
// The monster shall appear now.
    monster_tarbaby ();

// Yaw may change during tarbaby ai, change yaw back.
    self.angles_y   = parent.angles_y;
    self.fixangle   = TRUE;
// Override normal ai with flying ai, since it is launched out of vomitus.
    self.enemy      = parent.enemy;
    tbaby_birth ();

// Pet counter, used to enforce limits.
    self.xfl = self.xfl | XFL_PET;
    parent.pets = parent.pets + 1;

// Successfully spawned a new creature.
    self = parent;
};

// Vomits chunks of meat at the enemy.
void(float no) Vomit_Meat =
{
    local   vector  org, dir, err;
    local   float   side, up;

    err = '0 0 0';
    if (no)
    {
        if (random() < no)
            return;
        err_x = 10;
    }
    side = crandom() * 3;
    up = random() * 300;
    makevectors (self.angles);
    org = self.origin + v_forward*20 + v_right*side - v_up*6;
    dir = Maim_Toss (org, self.enemy.origin, 800, up, world, err);

    up = random();
    Meat_Launch (org, dir);
    if (up < 0.2)
        setmodel (newmis, "progs/gib1.mdl");
    else
    {   // Bigger gibs...
        newmis.dmg = random() * 15 + 15;
        if (up < 0.4)
            setmodel (newmis, "progs/h_zombie.mdl");
        else if (up < 0.7)
            setmodel (newmis, "progs/gib2.mdl");
        else
            setmodel (newmis, "progs/gib3.mdl");
    }
    setsize (newmis, '0 0 0', '0 0 0');
};

// Vomitus chews on the target.
// Note:  This is called during running instead of an attack sequence.
void() vom_melee =
{
    local   entity  targ;
    local   float   ldmg;

//    if (self.gorging)
//        return;     // It's food, so don't bite -- just run over it.

    targ = self.enemy;
    if (!targ)
        return;        // removed before stroke
    if (!CanDamage (targ, self))  // PM:  Added just in case.
        return;
    if (!infront (targ))
        return;     // PM:  Unusual -- must be in front of mouth.
    if (!InMeleeRange (80))     // PM:  Dragons-related.
        return;     // Vomitus has short range for a fat monster.
    //if (IsGhost (targ))
        //return;     // Cannot hit wraith.

//- - - - - - - - -
// Vomitus does the following on a successful attack:
//
// * Inflicts anywhere from 1 to 30 damage.
// * Life leech: 10% of damage inflicted is given back as health to vomitus.
// * 10% of eating the target's current weapon.
//- - - - - - - - -
    sound (self, CHAN_VOICE, "fish/bite.wav", 1, ATTN_NORM);
    // Update 6/17/2011:  Removed poison code.
    if (random() < 0.1)     // Eat the player's weapon on a critical hit.
        War_Steal (self, targ, "Vomitus ate your ");
    // Heal a little as it feeds on its victim.
    ldmg = SUB_MeleeDamage (10);
    T_Damage (targ, self, self, ldmg, DAMARMOR);
};

float(entity targ) IsZombieDown =
{
    if (targ.classname == "monster_zombie")
        if (targ.solid == SOLID_NOT)
            if (targ.frame == 172)  // paine11.
                return TRUE;
    return FALSE;
};

// Crushes all nearby corpses into edible gibs.
// Any damage done to achieve this is given back as health to vomitus.
void() Vomit_Crush =
{
    local   entity  head, next, swap;
    local   float   hp, gh;

// Note:  45 is approximately 32 * sqrt(2).
    head = QC_FindRadius (self.origin, 45);     // Picks up solid_not.
    while (head)
    {
        next = head.chain;  // We may remove head so get the next in line.

        if (head.flags & FLx_CREATURE)
        if (head.takedamage)    // Corpses can take damage in Drake.
        if (head != self)
        {
            // Crush the body into gibs it can eat.
            if (head.deadflag == DEAD_DEAD)
            {
                if (head.th_gib)
                {
                    gh = head.gib_health - 1;
                    hp = head.health - gh;
                    // Modified T_Heal rejects numbers under 1, so no need to fix.
                    swap = self;
                    self = head;
                    self.health = gh;
                    if (self.th_gib (0))    // Use 0 to minimize gib velocity.
                    {
                        self.th_gib = world.null_func;
                        T_Heal (swap, hp, TRUE);    // Can exceed max hp.
                    }
                    self = swap;
                }
            }
            else
            {
                if (IsZombieDown (head))
                {   // Devour fallen zombies.
                    T_Heal (self, head.health, TRUE);
                    T_Kill (head, self, self, 0, "melee");
                }
            }
//            if (self.enemy == head)
//                {self.enemy = world;  self.gorging = FALSE;}
        }
        head = next;
    }
};

// TODO:  Have vomitus scan for corpses, go to them, and devour them.

void() ai_vomit_stand = {Vomit_Crush(); ai_stand();};
void(float dist) ai_vomit_walk = {Vomit_Crush(); ai_walk(dist);};
void(float dist) ai_vomit_run  =
{
    local   float   close;

    close = QC_CloseEnough (dist);

    Vomit_Crush();
    if (close)
    {
//        if (self.gorging)
//        {
//            if (IsDead (self.enemy) && (self.enemy.solid == SOLID_NOT))
//                {walkmove (self.angles_y, dist);  return;}
//            if (self.enemy.touch == Gib_Touch)
//                {walkmove (self.angles_y, dist);  return;}
//        }
//        else
        {
            // Move over the fallen zombie and crush it for an instakill.
            if (IsZombieDown (self.enemy))
                {walkmove (self.angles_y, dist);  return;}
            // Turn around so it can bite the target while on the run.
            if (self.enemy)
                ai_face ();
        }
    }
    ai_run(dist);
};

//==========================================================================

// STAND - - - - - - - - - - - - - - - -
void() vom_stand1 =[ $walk1, vom_stand2 ] {ai_vomit_stand();};
void() vom_stand2 =[ $walk2, vom_stand3 ] {ai_vomit_stand();};
void() vom_stand3 =[ $walk3, vom_stand4 ] {ai_vomit_stand();};
void() vom_stand4 =[ $walk4, vom_stand5 ] {ai_vomit_stand();};
void() vom_stand5 =[ $walk5, vom_stand6 ] {ai_vomit_stand();};
void() vom_stand6 =[ $walk4, vom_stand7 ] {ai_vomit_stand();};
void() vom_stand7 =[ $walk3, vom_stand8 ] {ai_vomit_stand();};
void() vom_stand8 =[ $walk2, vom_stand1 ] {ai_vomit_stand();};

// WALK - - - - - - - - - - - - - - - -
void() vom_idle_sfx =
{
// Yes, use attn_norm instead of attn_idle.
    sound (self, CHAN_VOICE, "gibfnt/gf_plop.wav", 1, ATTN_NORM);
};
void() vom_walk1  =[ $walk1,  vom_walk2  ] {
    if (random() < 0.5)
        vom_idle_sfx();
    ai_vomit_walk(2);
};   // 3
void() vom_walk2  =[ $walk2,  vom_walk3  ] {ai_vomit_walk(2);};
void() vom_walk3  =[ $walk3,  vom_walk4  ] {ai_vomit_walk(2);};
void() vom_walk4  =[ $walk4,  vom_walk5  ] {ai_vomit_walk(2);};
void() vom_walk5  =[ $walk5,  vom_walk6  ] {ai_vomit_walk(2);};
void() vom_walk6  =[ $walk6,  vom_walk7  ] {ai_vomit_walk(2);};   // 2
void() vom_walk7  =[ $walk7,  vom_walk8  ] {ai_vomit_walk(2);};
void() vom_walk8  =[ $walk8,  vom_walk9  ] {ai_vomit_walk(2);};
void() vom_walk9  =[ $walk9,  vom_walk10 ] {ai_vomit_walk(2);};
void() vom_walk10 =[ $walk10, vom_walk11 ] {ai_vomit_walk(2);};   // 1
void() vom_walk11 =[ $walk11, vom_walk12 ] {ai_vomit_walk(2);};
void() vom_walk12 =[ $walk12, vom_walk13 ] {ai_vomit_walk(2);};
void() vom_walk13 =[ $walk13, vom_walk14 ] {ai_vomit_walk(2);};
void() vom_walk14 =[ $walk14, vom_walk15 ] {ai_vomit_walk(2);};
void() vom_walk15 =[ $walk15, vom_walk16 ] {ai_vomit_walk(2);};
void() vom_walk16 =[ $walk16, vom_walk17 ] {ai_turn();};
void() vom_walk17 =[ $walk17, vom_walk18 ] {ai_turn();};
void() vom_walk18 =[ $walk18, vom_walk19 ] {ai_turn();};
void() vom_walk19 =[ $walk19, vom_walk20 ] {ai_turn();};
void() vom_walk20 =[ $walk20, vom_walk21 ] {ai_turn();};
void() vom_walk21 =[ $walk21, vom_walk22 ] {ai_turn();};
void() vom_walk22 =[ $walk22, vom_walk23 ] {ai_turn();};
void() vom_walk23 =[ $walk23, vom_walk24 ] {ai_turn();};
void() vom_walk24 =[ $walk24, vom_walk25 ] {ai_turn();};
void() vom_walk25 =[ $walk25, vom_walk1  ] {ai_turn();};

// RUN - - - - - - - - - - - - - - - - -
void() vom_run1  =[ $run1,  vom_run2  ] {
    if (random() < 0.2)
        vom_idle_sfx();
    ai_vomit_run(4);
};
void() vom_run2  =[ $run2,  vom_run3  ] {ai_vomit_run(4);};
void() vom_run3  =[ $run3,  vom_run4  ] {ai_vomit_run(4);};
void() vom_run4  =[ $run4,  vom_run5  ] {vom_melee(); ai_vomit_run(4);};
void() vom_run5  =[ $run5,  vom_run6  ] {ai_vomit_run(4);};
void() vom_run6  =[ $run6,  vom_run7  ] {ai_vomit_run(4);};
void() vom_run7  =[ $run7,  vom_run8  ] {ai_vomit_run(4);};
void() vom_run8  =[ $run8,  vom_run1  ] {ai_vomit_run(4);};

// ATTACK - - - - - - - - - - - - - - -
void() vom_end1 =[ $attack11, vom_end2     ] {ai_face();};
void() vom_end2 =[ $attack12, vom_end3     ] {ai_face();};
void() vom_end3 =[ $attack13, vom_run1     ] {ai_face();};

void() vom_attack1  =[ $attack1,  vom_attack2  ] {
    self.pain_finished = time + 1.2;    // Ignore pain during attack.
    sound (self, CHAN_WEAPON, "gibfnt/gf_sprt1.wav", 1, ATTN_NORM);
    ai_face();
};
void() vom_attack2  =[ $attack2,  vom_attack3  ] {ai_face();};
void() vom_attack3  =[ $attack3,  vom_attack4  ] {ai_face();};
void() vom_attack4  =[ $attack4,  vom_attack5  ] {ai_face();};
void() vom_attack5  =[ $attack5,  vom_attack6  ] {ai_face();};
void() vom_attack6  =[ $attack6,  vom_attack7  ] {ai_face();};
void() vom_attack7  =[ $attack7,  vom_attack8  ] {ai_face();};
void() vom_attack8  =[ $attack8,  vom_attack9  ] {ai_face();};
void() vom_attack9  =[ $attack9,  vom_attack10 ] {ai_face();};
void() vom_attack10 =[ $attack10, vom_attack11 ] {ai_face();
    if (random() < 0.5)
        sound (self, CHAN_WEAPON, "vomitus/flesh1.wav", 1, ATTN_NORM);
    else
        sound (self, CHAN_WEAPON, "vomitus/flesh2.wav", 1, ATTN_NORM);
    if (self.skin)
        Vomit_Meat(0);
    else
    {
        Vomit_Spawn();
        self.think = vom_end1;
    }
};
void() vom_attack11 =[ $attack11, vom_attack12 ] {ai_face(); Vomit_Meat(0.5);};
void() vom_attack12 =[ $attack12, vom_attack13 ] {ai_face(); Vomit_Meat(0.5);};
void() vom_attack13 =[ $attack13, vom_run1     ] {ai_face(); Vomit_Meat(0.5);};
void() vom_attack =
{
    local   float   stop;

    stop = FALSE;
    if (enemy_range == RANGE_MELEE)
        if (random() < 0.5)     // Don't auto-vomit all the time.
            stop = TRUE;
    if (skill < 1)
    {   // Only one child in Easy.
        if (self.pets)
            stop = TRUE;
    }
    else
    {   // Up to three.
        if (self.pets >= 3)
            stop = TRUE;
    }

// FIXME:  Let blue vomitus spit vermis-like spores.  Low priority though.
    if (stop)
        ai_charge (4);      // Cannot vomit anymore for now.
    else
        vom_attack1 ();     // Cough up a tarbaby.
};

// PAIN - - - - - - - - - - - - - - - -
void() vom_pain1  =[ $pain1,  vom_pain2  ] {};
void() vom_pain2  =[ $pain2,  vom_pain3  ] {};
void() vom_pain3  =[ $pain3,  vom_pain4  ] {};
void() vom_pain4  =[ $pain2,  vom_pain5  ] {};
void() vom_pain5  =[ $pain1,  vom_run1   ] {};

void(entity inflictor, entity attacker, float damage) vom_pain = {
// don't make multiple pain sounds right after each other
    if (self.pain_finished > time)
        return;

    self.pain_finished = time + 2;
    sound (self, CHAN_VOICE, "vomitus/groan.wav", 1, ATTN_NORM);        

    if (SUB_IgnorePain ())
        return;         // No pain anims in Nightmare.

    vom_pain1 ();
    attacker = attacker;  damage = damage;      // Appease FrikQcc.
};
/*
// DEATH - - - - - - - - - - - - - - - -
float(float dm) vom_gib =
{
// PM:  Don't even think about calling T_RadiusDamage here!
// This is because gib functions may be called by T_Damage,
// which may be called within in T_RadiusDamage loop.
    if (self.health < self.gib_health)
    {
        local   float   loop;

        sound (self, CHAN_VOICE, "blob/death1.wav", 1, ATTN_NORM);
        if (self.skin)
        {   // Meatwad
            Tent_Explosion2 (self.origin, 64, 16);  // Last could be an 8.
            ThrowGib ("progs/gib1.mdl", dm);
            ThrowGib ("progs/gib2.mdl", dm);
            ThrowGib ("progs/gib2.mdl", dm);
            ThrowGib ("progs/gib3.mdl", dm);
            ThrowGib ("progs/gib3.mdl", dm);
            ThrowHead ("progs/h_vomit.mdl", dm);
        }
        else
        {   // Tar mother
            Tent_Point (TE_TAREXPLOSION, self.origin);
            while (loop < 5)
            {
                ThrowGib ("progs/tar_gib.mdl", dm);
                loop = loop + 1;
            }
            ThrowHead ("progs/tar_gib.mdl", dm);
        }
        return TRUE;
    }
    return FALSE;
};
*/

void() vom_die1  =[ $death1,  vom_die2    ] {};
void() vom_die2  =[ $death2,  vom_die3    ] {};
void() vom_die3  =[ $death3,  vom_die4    ] {};
void() vom_die4  =[ $death4,  vom_die5    ] {};
void() vom_die5  =[ $death5,  vom_die6    ] {};
void() vom_die6  =[ $death6,  vom_die7    ] {/*monster_drop();*/};
void() vom_die7  =[ $death7,  vom_die8    ] {};
void() vom_die8  =[ $death8,  vom_die9    ] {};
void() vom_die9  =[ $death9,  vom_die10   ] {};
void() vom_die10 =[ $death10, vom_die11   ] {};
void() vom_die11 =[ $death11, vom_die12   ] {};
void() vom_die12 =[ $death12, vom_die13   ] {};
void() vom_die13 =[ $death13, vom_die14   ] {};
void() vom_die14 =[ $death14, vom_die15   ] {};
void() vom_die15 =[ $death15, vom_die16   ] {};
void() vom_die16 =[ $death15, vom_die16 ] {
    self.health = self.gib_health - 1;
    vom_gib (self.health);      // Pop into gibs.
};

void() vom_die =
{
    monster_death_precheck();
    //if (Shattered ())
        //return;

// check for gib
    //if (vom_gib (self.health))
        //return;

    sound (self, CHAN_VOICE, "vomitus/death.wav", 1, ATTN_NORM);
    vom_die1 ();
};

// REVIVE - - - - - - - - - - - - - - -
void() vom_revive1  =[ $death14,  vom_revive2  ] {};
void() vom_revive2  =[ $death13,  vom_revive3  ] {};
void() vom_revive3  =[ $death12,  vom_revive4  ] {};
void() vom_revive4  =[ $death11,  vom_revive5  ] {};
void() vom_revive5  =[ $death10,  vom_revive6  ] {};
void() vom_revive6  =[ $death9,   vom_revive7  ] {};
void() vom_revive7  =[ $death8,   vom_revive8  ] {};
void() vom_revive8  =[ $death7,   vom_revive9  ] {};
void() vom_revive9  =[ $death6,   vom_revive10 ] {};
void() vom_revive10 =[ $death5,   vom_revive11 ] {};
void() vom_revive11 =[ $death4,   vom_revive12 ] {};
void() vom_revive12 =[ $death3,   vom_revive13 ] {};
void() vom_revive13 =[ $death2,   vom_revive14 ] {};
void() vom_revive14 =[ $death1,   vom_run1     ] {};

// OTHER - - - - - - - - - - - - - - - -
void(float hello) vom_sight =
{
// No greeting sound.  Just croak if mad at something.
    if (!hello)
        sound (self, CHAN_VOICE, "gibfnt/gf_sbig.wav", 1, ATTN_NORM);
};

void() vom_cache =
{
    precache_gluttony ();   // Sound when it eats gibs.

    precache_model ("progs/h_vomit.mdl");
    precache_model ("progs/h_zombie.mdl");
    precache_model ("progs/tar_gib.mdl");
    precache_model ("progs/drake/vomitus.mdl");
    precache_model ("progs/zom_gib.mdl");

    precache_sound ("blob/death1.wav");
    precache_sound ("fish/bite.wav");
    precache_sound ("gibfnt/gf_plop.wav");
    precache_sound ("gibfnt/gf_sbig.wav");
    precache_sound ("gibfnt/gf_sprt1.wav");
    precache_sound ("vomitus/groan.wav");
    precache_sound ("vomitus/death.wav");
    precache_sound ("vomitus/flesh1.wav");
    precache_sound ("vomitus/flesh2.wav");
    precache_sound ("zombie/z_miss.wav");
    precache_sound ("zombie/z_hit.wav");
};

//============
// --> QUAKED monster_vomit (1 0 0) (-32 -32 -24) (32 32 24) Ambush
//
// Bbox should be '-40 40 -24' to '40 40 24', but then it would need
// multiple bboxs.  Since Nehahra used standard shambler size, and
// the shambler is nearly as fat, use that size.
//
// Note to self:  Quoth's gug is even fatter, yet it uses shambler bbox.
//============
void() monster_vomit =
{
    if (!self.spawned)
    {
        if (noenemies)
            {remove (self); return;}
        vom_cache ();
        tbaby_cache ();     // It vomits tarbabies.
    }

    // Nehahra used 300 health, but Doom cacodemon has 400.
    monster_form ("progs/drake/vomitus.mdl", '-32 -32 -24', '32 32 24', 400, -65);
    self.mass   = MASS_HEAVY;

    self.th_stand   = vom_stand1;
    self.th_walk    = vom_walk1;
    self.th_run     = vom_run1;
    self.th_missile = vom_attack;
    self.th_pain    = vom_pain;
    self.th_die     = vom_die;

    self.th_sight   = vom_sight;
    self.th_gib     = vom_gib;
    self.th_revive  = vom_revive1;
    self.th_spawn   = monster_vomit;
    if (self.deathstring == "") self.deathstring = " was vomited on by a Vomitus\n";

    if (!self.yaw_speed) self.yaw_speed = 15;    // Slower than normal.

    self.headmdl   = "progs/h_vomit.mdl";
    if (!self.skin)
    {
        self.headmdl   = "progs/tar_gib.mdl";
        self.bloodtype  = BLOOD_PURPLE;
    }

    self.classtype = CT_MONVOMITUS;
    self.classgroup = CG_SPAWN;
    self.classmove = MON_MOVEWALK;

    monster_start ();
};

void() monster_vomitus = {self.classname = "monster_vomit"; monster_vomit();};

// Vomitus with meat skin.  Looks a bit like Meatwad.
// Now where's Master Shake and Frylock?
void() monster_meatwad = {
    self.skin = 1;
    self.classname = "monster_vomit";
    monster_vomit();
};