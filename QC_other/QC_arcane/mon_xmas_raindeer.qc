/*==============================================================================
XMAS Raindeer - From Twisted Christmas Mod by Twisted Matrix
==============================================================================*/

// (000 - 003) Rocket attack
$frame rattack1 rattack2 rattack3 rattack4

// (004 - 010) Alternative idle animation, scrape feet on floor
$frame idlefeet1 idlefeet2 idlefeet3 idlefeet4 idlefeet5 idlefeet6 idlefeet7

// (011 - 014) Fast and Heavy breathing
$frame stand1 stand2 stand3 stand4

// (015 - 022) Running/Charge attack
$frame run1 run2 run3 run4 run5 run6 run7 run8

// (023 - 023) Static death pose - on altar (need skin 1)
$frame altar1

// (024 - 025) Misc modelling stuff
$frame base1 base2

//======================================================================
void() raind_idlefeet1 = [ $idlefeet1, raind_idlefeet2 ] {monster_footstep(FALSE);ai_stand();};
void() raind_idlefeet2 = [ $idlefeet2, raind_idlefeet3 ] {ai_stand();};
void() raind_idlefeet3 = [ $idlefeet3, raind_idlefeet4 ] {monster_footstep(FALSE);ai_stand();};
void() raind_idlefeet4 = [ $idlefeet4, raind_idlefeet5 ] {ai_stand();};
void() raind_idlefeet5 = [ $idlefeet5, raind_idlefeet6 ] {ai_stand();};
void() raind_idlefeet6 = [ $idlefeet6, raind_idlefeet7 ] {ai_stand();};
void() raind_idlefeet7 = [ $idlefeet7, raind_stand1 ] {ai_stand();};

//----------------------------------------------------------------------
void() raind_stand1 = [ $stand1, raind_stand2 ] { monster_idle_sound();ai_stand();};
void() raind_stand2 = [ $stand1, raind_stand3 ] {ai_stand();};
void() raind_stand3 = [ $stand2, raind_stand4 ] {ai_stand();};
void() raind_stand4 = [ $stand2, raind_stand5 ] {ai_stand();};
void() raind_stand5 = [ $stand3, raind_stand6 ] {ai_stand();};
void() raind_stand6 = [ $stand3, raind_stand7 ] {ai_stand();};
void() raind_stand7 = [ $stand4, raind_stand8 ] {ai_stand();};
void() raind_stand8 = [ $stand4, raind_stand1 ] {
    self.idlebusy = FALSE; 
    if (random() < MON_IDLE_ANIMATION) self.think = raind_idlefeet1;
};

//======================================================================
void() raind_walk1 = [ $run5, raind_walk2 ] {monster_idle_sound();ai_walk(8);};
void() raind_walk2 = [ $run6, raind_walk3 ] {ai_walk(5);};
void() raind_walk3 = [ $run7, raind_walk4 ] {ai_walk(10);};
void() raind_walk4 = [ $run8, raind_walk5 ] {monster_footstep(FALSE);ai_walk(5);};
void() raind_walk5 = [ $run1, raind_walk6 ] {ai_walk(8);};
void() raind_walk6 = [ $run2, raind_walk7 ] {ai_walk(13);};
void() raind_walk7 = [ $run3, raind_walk8 ] {ai_walk(5);};
void() raind_walk8 = [ $run4, raind_walk1 ] {monster_footstep(FALSE);ai_walk(8);};

//======================================================================
void() raind_run1 = [ $run5, raind_run2 ] {monster_idle_sound();ai_run(15);};
void() raind_run2 = [ $run6, raind_run3 ] {ai_run(10);};
void() raind_run3 = [ $run7, raind_run4 ] {ai_run(20);};
void() raind_run4 = [ $run8, raind_run5 ] {monster_footstep(FALSE);ai_run(10);};
void() raind_run5 = [ $run1, raind_run6 ] {ai_run(15);};
void() raind_run6 = [ $run2, raind_run7 ] {ai_run(25);};
void() raind_run7 = [ $run3, raind_run8 ] {ai_run(10);};
void() raind_run8 = [ $run4, raind_run1 ] {monster_footstep(FALSE);ai_run(15);};

//======================================================================
// MELEE : Antler attack
//======================================================================
void() raind_melee =
{
    local float ldmg;

    if (!self.enemy) return;
    if (self.health < 1) return;
        
    ai_charge(10);
    ai_damagebreakable(20);
    if (!ai_checkmelee(MONAI_MELEERAINDEER)) return;

    // Can the target bleed?
    if (!self.enemy.takedamage) return;

    sound(self, CHAN_WEAPON, "xmas/raindeer/hit.wav", TRUE, TRUE);

    // Antler attack is weak (1-9)
    ldmg = (random() + random() + random()) * 3;
    if (ldmg < 1) ldmg = 1;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);

    // Spawn blood at antlers
    spawn_touchblood (self, self.enemy, ldmg*3);
};

//----------------------------------------------------------------------
void() raind_ram1 = [ $rattack1, raind_ram2 ] {ai_charge(10);};
void() raind_ram2 = [ $rattack2, raind_ram3 ] {raind_melee();};
void() raind_ram3 = [ $rattack3, raind_ram4 ] {ai_charge(5);};
void() raind_ram4 = [ $rattack4, raind_run1 ] {
    // Check if raindeer is within range to attack again
    if (ai_checkmelee(MONAI_MELEERAINDEER) && self.enemy.health > 0) {
        if (self.health > 0) self.think = raind_ram1;
    }
};

//======================================================================
// RANGE : Rockets from backpack
//======================================================================
void(vector sideofs) raind_attack =
{
    local vector org, dir, vec;
    
    if (!self.enemy) return;
    if (self.health < 1) return;
    
    self.effects = self.effects | EF_MUZZLEFLASH;
    self.attack_speed = SPEED_RAINDROCK + (skill * SPEED_RAINDROCKSKILL);

    // Cycle between attack sounds
    if (self.lefty == 0) 
        sound (self, CHAN_WEAPON, "xmas/raindeer/attack1.wav", 1, ATTN_NORM);
    else
        sound (self, CHAN_WEAPON, "xmas/raindeer/attack2.wav", 1, ATTN_NORM);
    self.lefty = 1 - self.lefty;

    // turn and face your enemy!
    ai_face();
    makevectors (self.angles);

    org = self.origin + attack_vector(sideofs);
    // Fire rocket direction to match raindeer side
    vec = v_right*(sideofs_x+(random()*10));
    dir = normalize((self.enemy.origin + vec) - org);
    Launch_Missile (org, dir, '0 0 0', CT_PROJ_RAINDEER, self.attack_speed);
};

//----------------------------------------------------------------------
void() raind_shoot1 = [ $rattack1, raind_shoot2 ] {ai_face();};
void() raind_shoot2 = [ $rattack2, raind_shoot3 ] {};
void() raind_shoot3 = [ $rattack3, raind_shoot4 ] {ai_face();};
void() raind_shoot4 = [ $rattack4, raind_shoot5 ] {
    // Skills - EASY/NORMAL = 1 rocket, HARD/NM = 2 rockets
    if (skill < SKILL_HARD) {
        self.think = raind_run1;
        // Randomize upper side (LEFT=-12 RIGHT=12)
        if (random() < 0.5) raind_attack('-2 -12 12');
        else raind_attack('-2 12 12');
    }
    else raind_attack('-2 -12 12');
};
void() raind_shoot5 = [ $rattack1, raind_shoot6 ] {ai_face();};
void() raind_shoot6 = [ $rattack2, raind_shoot7 ] {};
void() raind_shoot7 = [ $rattack3, raind_shoot8 ] {ai_face();};
void() raind_shoot8 = [ $rattack4, raind_run1 ] {
    raind_attack('-2 12 12');
};

//======================================================================
// PAIN : quick reflection
//======================================================================
void() raind_pain1 = [ $rattack1, raind_pain2 ] {};
void() raind_pain2 = [ $rattack2, raind_pain3 ] {ai_pain(4);};
void() raind_pain3 = [ $rattack3, raind_pain4 ] {};
void() raind_pain4 = [ $rattack4, raind_run1 ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) raind_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1) raind_pain1 ();
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 0.6;
            self.axhitme = 0;
            raind_pain1 ();
        }
    }
};

//======================================================================
// DEATH : Its raining raindeer tonight!
//======================================================================
void() raind_explode =
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

    // Regular blood like gibs
    ThrowGib(4, 2 + rint(random()*4));
    ThrowGib(5, 2 + rint(random()*4));
    ThrowGib(25, 1);
    ThrowGib(11, 2 + rint(random()*2));
    // Metal backpack
    self.gibtype = GIBTYPE_METAL;
    ThrowGib(12, rint(random()*2));
};

//----------------------------------------------------------------------
void() raind_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // There is no death animation
    sound (self, CHAN_BODY, self.death_sound, 1, ATTN_NORM);
    raind_explode();
};

//----------------------------------------------------------------------
void() raind_deadbody =
{
    // Setup bounding box based on presets
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    monster_bbox();

    // Sacrifice pose + skin
    self.frame = $altar1;
    self.exactskin = self.skin = 1;
    self.gib2mdl = self.gib1mdl;
    if (random() < 0.5) self.gib3mdl = self.gib1mdl;
    if (random() < 0.5) self.gib4mdl = self.gib1mdl;
    misc_deadbody_setup();
};

//=============================================================================
//QUAKED monster_xmas_raindeer (0.5 0.75 0) (-16 -16 -24) (16 16 40) Ambush
//=============================================================================
void() monster_xmas_raindeer =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/xmas/mon_raindeer.mdl";        // Brown meat
    self.headmdl = "progs/xmas/h_raindeer.mdl";        // Bloody meat
    self.gib1mdl = "progs/xmas/gib_rdleg1.mdl";        // Bloody stump
    self.gib2mdl = "progs/xmas/gib_rdrockpack.mdl";    // Rocket backpack

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_RAIND);    // Snowy Rockets
    precache_model (self.gib1mdl);        // Legs
    precache_model (self.gib2mdl);        // Rocket Packs

    self.idle_sound = "xmas/raindeer/idle.wav";
    precache_sound (self.idle_sound);
    self.sight_sound = "xmas/raindeer/sight.wav";
    precache_sound (self.sight_sound);

    // Alternating rocket firing sound
    precache_sound ("xmas/raindeer/attack1.wav");
    precache_sound ("xmas/raindeer/attack2.wav");
    
    self.pain_sound = "xmas/raindeer/hit.wav";
    precache_sound (self.pain_sound);
    self.death_sound = "xmas/raindeer/death.wav";
    precache_sound (self.death_sound);

    // Dead body version is setup different
    if (self.spawnflags & MON_RAINDEER_DEAD) {
        raind_deadbody(); return;
    }
    
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    if (self.health < 1) self.health = 200;
    if (self.death_dmg < 1) self.death_dmg = DAMAGE_RAINDEER;
    self.gibhealth = MON_NEVERGIB;        // Cannot be gibbed by weapons
    self.blockudeath = TRUE;            // no humanoid death sound
    self.gibbed = FALSE;                // Still in one piece
    self.pain_flinch = 30;                // Light pain threshold
    self.pain_longanim = FALSE;            // No long pain animation
    self.steptype = FS_TYPESLOW;        // Small sounding feet
    self.poisonous = FALSE;                // Not Supported
    self.meleeoffset = '24 0 20';        // Antler attack offset
    self.lefty = 0;                        // Cycle attack sounds
    self.deathstring = " was trampled by a Raindeer\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.th_checkattack = RaindeerCheckAttack;
    self.th_stand = raind_stand1;
    self.th_walk = raind_walk1;
    self.th_run = raind_run1;
    self.th_melee = raind_ram1;
    self.th_missile = raind_shoot1;
    self.th_pain = raind_pain;
    self.th_die = raind_die;
    
    self.classtype = CT_MONRAINDEER;
    self.classgroup = CG_XMAS;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};

