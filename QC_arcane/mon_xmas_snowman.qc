/*==============================================================================
XMAS Snowman - From Twisted Christmas Mod by Twisted Matrix
==============================================================================*/

// (000) Standing Still
$frame stand1

// (001 - 005) Fast Claw attack
$frame claw1 claw2 claw3 claw4 claw5

// (006 - 008) Fast Nail Attack
$frame attack1 attack2 attack3

// (009 - 010) Misc modelling stuff
$frame base1 base2

//======================================================================
void() snowm_stand1 = [ $stand1, snowm_stand2 ] {monster_idle_sound();ai_stand();}
void() snowm_stand2 = [ $stand1, snowm_stand3 ] {ai_stand();};
void() snowm_stand3 = [ $stand1, snowm_stand4 ] {ai_stand();};
void() snowm_stand4 = [ $stand1, snowm_stand5 ] {ai_stand();};
void() snowm_stand5 = [ $stand1, snowm_stand6 ] {ai_stand();};
void() snowm_stand6 = [ $stand1, snowm_stand7 ] {ai_stand();};
void() snowm_stand7 = [ $stand1, snowm_stand8 ] {ai_stand();};
void() snowm_stand8 = [ $stand1, snowm_stand1 ] {ai_stand();};

//======================================================================
void() snowm_walk1 = [ $stand1, snowm_walk2 ] {monster_idle_sound();ai_walk(0);}
void() snowm_walk2 = [ $stand1, snowm_walk3 ] {ai_walk(0);};
void() snowm_walk3 = [ $stand1, snowm_walk4 ] {ai_walk(0);};
void() snowm_walk4 = [ $stand1, snowm_walk5 ] {ai_walk(0);};
void() snowm_walk5 = [ $stand1, snowm_walk6 ] {ai_walk(0);};
void() snowm_walk6 = [ $stand1, snowm_walk7 ] {ai_walk(0);};
void() snowm_walk7 = [ $stand1, snowm_walk8 ] {ai_walk(0);};
void() snowm_walk8 = [ $stand1, snowm_walk1 ] {ai_walk(0);};

//======================================================================
void() snowm_run1 = [ $stand1, snowm_run2 ] {monster_idle_sound();ai_run(0);}
void() snowm_run2 = [ $stand1, snowm_run3 ] {ai_run(0);};
void() snowm_run3 = [ $stand1, snowm_run4 ] {ai_run(0);};
void() snowm_run4 = [ $stand1, snowm_run5 ] {ai_run(0);};
void() snowm_run5 = [ $stand1, snowm_run6 ] {ai_run(0);};
void() snowm_run6 = [ $stand1, snowm_run7 ] {ai_run(0);};
void() snowm_run7 = [ $stand1, snowm_run8 ] {ai_run(0);};
void() snowm_run8 = [ $stand1, snowm_run1 ] {ai_run(0);};

//======================================================================
// MELEE : Crows Claw
//======================================================================
void() SnowmMelee =
{
    local float ldmg;

    if (!self.enemy) return;
    if (self.health < 1) return;
        
    ai_damagebreakable(10);                            // Damage any breakables
    if (!ai_checkmelee(MONAI_MELEESNOWMAN)) return; // Too far away

    // Can the target bleed?
    if (!self.enemy.takedamage) return;

    sound(self, CHAN_WEAPON, "xmas/snowman/claw.wav", TRUE, TRUE);

    // Claw attack is good (1-15)
    ldmg = (random() + random() + random()) * 5;
    if (ldmg < 1) ldmg = 1;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);

    // Spawn blood at claw
    spawn_touchblood (self, self.enemy, ldmg*3);
};

//----------------------------------------------------------------------
void() snowm_claw1 = [ $claw1, snowm_claw2 ] {ai_face();};
void() snowm_claw2 = [ $claw2, snowm_claw3 ] {ai_face();};
void() snowm_claw3 = [ $claw3, snowm_claw4 ] {ai_face();SnowmMelee();};
void() snowm_claw4 = [ $claw4, snowm_claw5 ] {};
void() snowm_claw5 = [ $claw5, snowm_run1 ] {
    // Check if snowman is within range to attack again
    if (ai_checkmelee(MONAI_MELEESNOWMAN) && self.enemy.health > 0) {
        if (self.health > 0) self.think = snowm_claw1;
    }
};

//======================================================================
// RANGE : Burst of Nails
//======================================================================
void() SnowmFireNails =
{
    local vector org, dir, vec;
    local float loopvar;
    
    // Check for death?
    if (self.health < 1) return;
    loopvar = 0;

    // Light up face, setup projectile speed and play fire sound
    self.effects = self.effects | EF_MUZZLEFLASH;
    self.attack_speed = SPEED_NAILSNOWMPROJ + (skill * SPEED_NAILSNOWMSKILL);
    sound(self, CHAN_WEAPON, "xmas/snowman/nail_fire.wav", 1, ATTN_NORM);

    // turn and face your enemy!
    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);
    
    // Loop through the nail burst (all at once)
    while (loopvar < self.attack_count) {
        // Random circle spread pattern
        vec = (crandom()*20)*v_right + (crandom()*20)*v_up;
        dir = normalize((self.enemy.origin + vec) - org);
        // Generate a red hot nail projectile
        launch_projectile(org, dir, CT_PROJ_MONNG, self.attack_speed);
        loopvar = loopvar + 1;
    }
};

//----------------------------------------------------------------------
void() snowm_nail1 = [ $attack1, snowm_nail2 ] {ai_face();};
void() snowm_nail2 = [ $attack2, snowm_nail3 ] {ai_face();};
void() snowm_nail3 = [ $attack3, snowm_nail4 ] {ai_face();SnowmFireNails();};
void() snowm_nail4 = [ $attack2, snowm_nail5 ] {};
void() snowm_nail5 = [ $attack1, snowm_run1 ] {ai_face();};

//======================================================================
// PAIN : quick reflection
//======================================================================
void() snowm_pain1 = [ $claw4, snowm_pain2 ] {};
void() snowm_pain2 = [ $claw3, snowm_pain3 ] {};
void() snowm_pain3 = [ $claw2, snowm_pain4 ] {};
void() snowm_pain4 = [ $claw1, snowm_run1 ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) snowm_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        if (random() < 0.5) sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
        if (self.pain_check == 1) snowm_pain1 ();
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 0.6;
            self.axhitme = 0;
            snowm_pain1 ();
        }
    }
};

//======================================================================
// Exploding snowball!
//======================================================================
void() snowm_explode =
{
    // no more raindeer!
    entity_hide (self);
    // Make sure gibs go flying up
    self.max_health = MON_GIBFOUNTAIN;
    self.health = -100;

    // ID style explosion
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
    if (self.death_dmg > 0)
        T_RadiusDamage (self, self, self.death_dmg, world, DAMAGEALL);
    
    // Sprite + Dust
    SpawnExplosion(EXPLODE_BIG, self.origin, self.death_sound);
    SpawnProjectileSmoke(self.origin, 200, 250, 100);
    SpawnProjectileSmoke(self.origin, 200, 250, 100);

    // No blood or guts!
    ThrowGib(25, 1);
    ThrowGib(11, 1);
    ThrowGib(12, 1);
    ThrowGib(13, 8 + rint(random()*8));

    /*-------------------------------------------------------------------
    // The snowball model has a melting animation (6 frames)
    // Unfortunately it does look really old and dated,
    // its better to let model just fade away.
    //
    // The final gib is a snowball which has a melting animation
    // Setup animation start/finish parameters before throwing
    self.gibAnimstart = 1;
    self.gibAnimfinal = 5;
    self.gibAnimTimer = 2;        // timer + (random() * (timer*2))
    ThrowGib(13, 8 + rint(random()*8)); */
};

//----------------------------------------------------------------------
void() snowm_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // There is no death animation
    if (random() < 0.5)
        sound (self, CHAN_BODY, self.death_sound, 1, ATTN_NORM);
    else sound (self, CHAN_BODY, self.death_sound2, 1, ATTN_NORM);
    snowm_explode();
};

/*======================================================================
QUAKED monster_snowman (1 0 0) (-16 -16 -24) (16 16 32) Ambush
======================================================================*/
void() monster_xmas_snowman =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/xmas/mon_snowman.mdl";        // Top Hat Snow
    self.headmdl = "progs/xmas/h_snowman.mdl";        // Top Hat
    self.gib1mdl = "progs/xmas/gib_snowclaw.mdl";    // Claw
    self.gib2mdl = "progs/xmas/gib_snowgun.mdl";    // Nail Gun
    self.gib3mdl = "progs/xmas/gib_snowball.mdl";    // Snowballs
    self.gib1sound = GIB_IMPACT_WOOD;
    self.gib2sound = GIB_IMPACT_METALA;

    precache_model (self.mdl);
    precache_model (self.headmdl);                // Top Hat
    precache_model (self.gib1mdl);                // Claw
    precache_model (self.gib2mdl);                // Gun
    precache_model (self.gib3mdl);                // Snow-BALLS!
    precache_model (MODEL_PROJ_NGRED);            // Red Nails (easier to see)

    // Idle calm/angry
    self.idle_sound = "xmas/snowman/idle.wav";    // Rustling sound
    precache_sound (self.idle_sound);

    // Many different sight sounds
    self.sight_sound = "xmas/snowman/sight1.wav";    // Deep voice
    self.sight_sound2 = "xmas/snowman/sight2.wav";    // Growling
    precache_sound (self.sight_sound);
    precache_sound (self.sight_sound2);
    self.sight_count = 2;

    // Melee/Range Attacks
    // Original version used ice, switched to red nails
    // Ice nails are really difficult to see
    precache_sound ("xmas/snowman/claw.wav");
    precache_sound ("xmas/snowman/nail_fire.wav");
    
    // precache sight, pain and death (changes on type)
    self.pain_sound = "xmas/snowman/pain1.wav";
    self.pain_sound2 = "xmas/snowman/pain2.wav";
    self.death_sound = "xmas/snowman/death1.wav";
    self.death_sound2 = "xmas/snowman/death2.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.pain_sound2);
    precache_sound (self.death_sound);
    precache_sound (self.death_sound2);

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_SHORT;
    if (self.health < 1) self.health = 125;
    if (self.death_dmg < 1) self.death_dmg = DAMAGE_SNOWMAN;
    self.gibhealth = MON_NEVERGIB;        // Cannot be gibbed by weapons
    self.blockudeath = TRUE;            // No humanoid death sound
    self.gibbed = FALSE;                // Still in one peice
    self.pain_flinch = 30;                // Light pain threshold
    self.pain_longanim = FALSE;            // No long pain animation
    self.steptype = FS_TYPESLOW;        // Small sounding feet
    self.poisonous = FALSE;                // Not Supported
    self.meleeoffset = '24 0 20';        // Claw attack offset
    self.attack_offset = '24 0 20';        // Run + Gun vector
    self.bleedcolour = MON_BCOLOR_SILVER;// Red is odd for snow
    self.deathstring = " was smashed on a Snowman\n";

    // Default amount of nails to burst fire (5x9=45dmg)
    if (self.attack_count < 1) self.attack_count = 5;

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    self.th_checkattack = SnowmanCheckAttack;
    self.th_stand = snowm_stand1;
    self.th_walk = snowm_walk1;
    self.th_run = snowm_run1;
    self.th_melee = snowm_claw1;
    self.th_missile = snowm_nail1;
    self.th_pain = snowm_pain;
    self.th_die = snowm_die;

    self.classtype = CT_MONSNOWMAN;
    self.classgroup = CG_XMAS;
    self.classmove = MON_MOVEWALK;

    monster_start();    
};