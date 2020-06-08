/*==============================================================================
XMAS Elf - From Twisted Christmas Mod by Twisted Matrix
==============================================================================*/

// (000 - 007) Rocking back and forth on feet
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8

// (008 - 017) Skipping! (Running)
$frame skip1 skip2 skip3 skip4 skip5 skip6 skip7 skip8
$frame skip9 skip10

// (018 - 029) Walking
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12

// (030 - 036) Suprise, jump up into the air with legs wide apart
$frame surprise1 surprise2 surprise3 surprise4 surprise5 surprise6 surprise7

// (037 - 048) ATTACK - Swing cane around (long version)
$frame magic1 magic2 magic3 magic4 magic5 magic6 magic7 magic8
$frame magic9 magic10 magic11 magic12

// (049 - 056) ATTACK - Jump up and smash cane into ground
$frame jump1 jump2 jump3 jump4 jump5 jump6 jump7 jump8

// (057 - 061) ATTACK - Swing cane around (short version)
$frame attack1 attack2 attack3 attack4 attack5

// (062 - 066) PAIN - Recoil backward
$frame pain1 pain2 pain3 pain4 pain5

// (067 - 078) ATTACK - Fall downward with cane infront
$frame ambush1 ambush2 ambush3 ambush4 ambush5 ambush6 ambush7 ambush8
$frame ambush9 ambush10 ambush11 ambush12

// (079 - 093) DEATH - Backward
$frame bdeath1 bdeath2 bdeath3 bdeath4 bdeath5 bdeath6 bdeath7 bdeath8
$frame bdeath9 bdeath10 bdeath11 bdeath12 bdeath13 bdeath14 bdeath15

// (094 - 107) DEATH - Forward
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12 death13 death14

// (108 - 109) Misc modelling stuff
$frame base1 base2

//======================================================================
void() elf_stand1 = [ $stand1, elf_stand2 ] {monster_idle_sound();ai_stand();};
void() elf_stand2 = [ $stand2, elf_stand3 ] {ai_stand();};
void() elf_stand3 = [ $stand3, elf_stand4 ] {ai_stand();};
void() elf_stand4 = [ $stand4, elf_stand5 ] {ai_stand();};
void() elf_stand5 = [ $stand5, elf_stand6 ] {ai_stand();};
void() elf_stand6 = [ $stand6, elf_stand7 ] {ai_stand();};
void() elf_stand7 = [ $stand7, elf_stand8 ] {ai_stand();};
void() elf_stand8 = [ $stand8, elf_stand1 ] {ai_stand();};

//======================================================================
void() elf_walk1 = [ $walk1, elf_walk2 ] {monster_idle_sound();ai_walk(10);};
void() elf_walk2 = [ $walk2, elf_walk3 ] {ai_walk(6);};
void() elf_walk3 = [ $walk3, elf_walk4 ] {monster_footstep(FALSE);ai_walk(4);};
void() elf_walk4 = [ $walk4, elf_walk5 ] {ai_walk(0);};
void() elf_walk5 = [ $walk5, elf_walk6 ] {ai_walk(2);};
void() elf_walk6 = [ $walk6, elf_walk7 ] {ai_walk(3);};

void() elf_walk7 = [ $walk7, elf_walk8 ] {ai_walk(10);};
void() elf_walk8 = [ $walk8, elf_walk9 ] {ai_walk(6);};
void() elf_walk9 = [ $walk9, elf_walk10 ] {monster_footstep(FALSE);ai_walk(4);};
void() elf_walk10 = [ $walk10, elf_walk11 ] {ai_walk(0);};
void() elf_walk11 = [ $walk11, elf_walk12 ] {ai_walk(2);};
void() elf_walk12 = [ $walk12, elf_walk1 ] {ai_walk(3);};

//======================================================================
void() elf_run1 = [ $skip1, elf_run2 ] {monster_idle_sound();ai_run(1);};
void() elf_run2 = [ $skip2, elf_run3 ] {ai_run(6);};
void() elf_run3 = [ $skip3, elf_run4 ] {ai_run(20);};
void() elf_run4 = [ $skip4, elf_run5 ] {monster_footstep(FALSE);ai_run(12);};
void() elf_run5 = [ $skip5, elf_run6 ] {ai_run(8);};

void() elf_run6 = [ $skip6, elf_run7 ] {ai_run(1);};
void() elf_run7 = [ $skip7, elf_run8 ] {ai_run(6);};
void() elf_run8 = [ $skip8, elf_run9 ] {ai_run(20);};
void() elf_run9 = [ $skip9, elf_run10 ] {monster_footstep(FALSE);ai_run(12);};
void() elf_run10 = [ $skip10, elf_run1 ] {ai_run(8);};

//======================================================================
void() elf_slide1 = [ $surprise1, elf_slide2 ] {ai_run_slide(2);monster_idle_sound();};
void() elf_slide2 = [ $surprise2, elf_slide3 ] {ai_run_slide(4);};
void() elf_slide3 = [ $surprise3, elf_slide4 ] {ai_run_slide(6);};
void() elf_slide4 = [ $surprise4, elf_slide5 ] {ai_run_slide(6);};
void() elf_slide5 = [ $surprise5, elf_slide6 ] {ai_run_slide(4);};
void() elf_slide6 = [ $surprise6, elf_slide7 ] {ai_run_slide(3);};
void() elf_slide7 = [ $surprise7, elf_run1 ] {ai_run(2);};

//======================================================================
// ELF 2 - SPRAY MAGIC (Range)
//======================================================================
void(float sideang) elf_shot =
{
    local    vector org, ang, dir, avel;
    
    if (!self.enemy) return;
    if (self.health < 1) return;
        
    // Flash effect to show where projectile is coming from    
    self.effects = self.effects | EF_MUZZLEFLASH;
    if (sideang < 0) sound (self, CHAN_WEAPON, "xmas/elf/attack1.wav", 1, ATTN_NORM);

    makevectors (self.angles);    
    org = self.origin + attack_vector(self.attack_offset);

    // Create elevation angle and use makevectors to create projectile direction
    ang = vectoangles(self.enemy.origin - org);
    ang_x = -self.attack_elev;    // Negative = upwards angle 
    makevectors (ang);
    // fire magic in arc pattern (sideang)
    dir = (v_forward + v_right * sideang) * SPEED_ELF;    

    avel = vecrand(100,200,FALSE);
    Launch_Grenade(org, dir, avel, CT_PROJ_ELF);
};

//----------------------------------------------------------------------
void() elf_magic1 = [ $magic12, elf_magic2 ] {
    sound(self, CHAN_WEAPON, "xmas/elf/magic.wav", 1, ATTN_NORM);};
void() elf_magic2 = [ $magic11, elf_magic3 ] {ai_face();};
void() elf_magic3 = [ $magic10, elf_magic4 ] {ai_face();};
void() elf_magic4 = [ $magic9, elf_magic5 ] {ai_face();};
void() elf_magic5 = [ $magic8, elf_magic6 ] {ai_face();
    self.attack_elev = SUB_Elevation(ELEV_DEFAULT, self.origin, self.enemy.origin, SPEED_ELF); };
void() elf_magic6 = [ $magic7, elf_magic7 ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, SPEED_ELF); };
void() elf_magic7 = [ $magic6, elf_magic8 ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, SPEED_ELF); };
void() elf_magic8 = [ $magic5, elf_magic9 ] {elf_shot(-0.1);};
void() elf_magic9 = [ $magic4, elf_magic10 ] {elf_shot(0);};
void() elf_magic10 = [ $magic3, elf_magic11 ] {elf_shot(0.1);};
void() elf_magic11 = [ $magic2, elf_magic12 ] {};
void() elf_magic12 = [ $magic1, elf_run1 ] {};

//======================================================================
// ELF 1 - SWING CANE - Both elf types can swing cane if within range
//======================================================================
void() elf_melee =
{
    local float ldmg;

    if (!self.enemy) return;
    if (self.health < 1) return;
        
    ai_charge(10);
    ai_damagebreakable(10);
    if (!ai_checkmelee(MONAI_MELEEELF)) return;

    // Can the target bleed?
    if (!self.enemy.takedamage) return;

    if (random() < 0.5) sound(self, CHAN_VOICE, "xmas/elf/cane_hit1.wav", 1, ATTN_NORM);
    else sound(self, CHAN_VOICE, "xmas/elf/cane_hit2.wav", 1, ATTN_NORM);

    // Cane Impact (damage 1-9) is very weak
    ldmg = (random() + random() + random()) * 3;
    if (ldmg < 1) ldmg = 1;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);

    // Spawn blood at cane of elf
    spawn_touchblood (self, self.enemy, ldmg*3);
}

//----------------------------------------------------------------------
void() elf_cane1 = [ $attack1, elf_cane2 ] {ai_charge(5);
    if (random() < 0.5) sound(self, CHAN_WEAPON, "xmas/elf/cane_swipe1.wav", 1, ATTN_NORM);
    else sound(self, CHAN_WEAPON, "xmas/elf/cane_swipe2.wav", 1, ATTN_NORM);    
};
void() elf_cane2 = [ $attack2, elf_cane3 ] {ai_charge(5);};
void() elf_cane3 = [ $attack3, elf_cane4 ] {elf_melee();};
void() elf_cane4 = [ $attack4, elf_cane5 ] {};
void() elf_cane5 = [ $attack5, elf_run1 ] {
    if (ai_checkmelee(MONAI_MELEEELF) && self.enemy.health > 0) {
        if (self.health > 0) self.think = elf_cane1;
    }
};

//======================================================================
// ELF 1 - JUMP FUNCTION (range)
//======================================================================
void()    Elf_JumpTouch =
{
    local    float    ldmg;

    if (self.health <= 0) return;
    ai_jumpbreakable(20);            // Damage any breakables
    self.touch = SUB_Null;            // No more touching
    self.count = self.count + 1;    // Total amount of touch jumps
    self.think = self.th_jumpexit;    // Exit frame
    self.jumptouch = other;            // Keep track of touch target

    // Do not damage other elves with jump attacks
    // Prevents packs from killing themselves
    if (self.classtype != other.classtype && other.takedamage) {
        if ( vlen(self.velocity) > 300 ) {
            ldmg = 5 + 5*random();
            T_Damage (other, self, self, ldmg, DAMARMOR);    
            // Spawn some touch blood (no explicit direction)
            spawn_touchblood (self.enemy, self.enemy, ldmg*3);
        }
    }

    // Is the elf floating in the air?
    if (!checkbottom(self)) {
        // Is the elf standing on something?        
        if (self.flags & FL_ONGROUND) {    
            // Do an extra jump if got the count
            if (self.count < 2)  self.think = self.th_jump;
        }
    }

    // Next timer
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------------
void() elf_leap1 =[ $jump1, elf_leap2 ] {ai_face();
    self.jump_flag = time + MONAI_JUMPTIMEOUT; // Stop jumping so much
    if (random() < 0.5) sound(self, CHAN_WEAPON, "xmas/elf/jump1.wav", 1, ATTN_NORM);
    else sound(self, CHAN_WEAPON, "xmas/elf/jump2.wav", 1, ATTN_NORM);
};
void() elf_leap2 =[ $jump2, elf_leap3 ] {ai_face();};
void() elf_leap3 =[ $jump3, elf_leap4 ] {
    ai_face();
    self.jump_flag = time + MONAI_JUMPTIMEOUT; // Stop jumping so much
    self.touch = Elf_JumpTouch;
    makevectors (self.angles);
    self.velocity = v_forward * MONAI_JUMPELFDIST + '0 0 200';
    self.origin_z = self.origin_z + 4;
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.oldorigin = self.origin;
};
void() elf_leap4 =[ $jump4, elf_leap5 ] {};
void() elf_leap5 =[ $jump5, elf_leap6 ] {};
void() elf_leap6 =[ $jump6, elf_leap7 ] {
    // Double check monster is still falling?
    if (CheckZeroVector(self.velocity) || self.oldorigin == self.origin) {
        self.ideal_yaw = random() * 360; //random jump angle
        self.think = elf_leap3;
    }
    self.oldorigin = self.origin;
};

//----------------------------------------------------------------------
void() elf_leap7 =[ $jump7, elf_leap8] {monster_footstep(FALSE);};
void() elf_leap8 =[ $jump8, elf_run1] {ai_resetangles();};

//======================================================================
// Elven Pain!
//======================================================================
void() elf_pain1 = [ $pain1, elf_pain2 ] {};
void() elf_pain2 = [ $pain2, elf_pain3 ] {};
void() elf_pain3 = [ $pain3, elf_pain4 ] {};
void() elf_pain4 = [ $pain4, elf_pain5 ] {};
void() elf_pain5 = [ $pain5, elf_run1 ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) elf_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1) elf_pain1 ();
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.axhitme = 0;
            self.pain_finished = time + 1;
            elf_pain1 ();
        }
    }
};

//======================================================================
// Typical forward/backward death animation
//======================================================================
void() elf_die1 =[ $death1, elf_die2] {};
void() elf_die2 =[ $death2, elf_die3] {monster_check_gib();};
void() elf_die3 =[ $death3, elf_die4] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() elf_die4 =[ $death4, elf_die5] {};
void() elf_die5 =[ $death5, elf_die6] {};
void() elf_die6 =[ $death6, elf_die7] {
    // Drop metal cane sound
    sound (self, CHAN_WEAPON, GIB_SOUND_METALA, 1, ATTN_NORM);
};
void() elf_die7 =[ $death7, elf_die8] {};
void() elf_die8 =[ $death8, elf_die9] {monster_death_postcheck();};
void() elf_die9 =[ $death9, elf_die9] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() elf_dieb1 =[ $bdeath1, elf_dieb2] {};
void() elf_dieb2 =[ $bdeath2, elf_dieb3] {monster_check_gib();};
void() elf_dieb3 =[ $bdeath3, elf_dieb4] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() elf_dieb4 =[ $bdeath4, elf_dieb5] {};
void() elf_dieb5 =[ $bdeath5, elf_dieb6] {};
void() elf_dieb6 =[ $bdeath6, elf_dieb7] {};
void() elf_dieb7 =[ $bdeath7, elf_dieb8] {};
void() elf_dieb8 =[ $bdeath8, elf_dieb9] {ai_back(4);monster_death_postcheck();};
void() elf_dieb9 =[ $bdeath9, elf_dieb9] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() elf_die =
{
    // Sync cane to elf skin
    self.gib1skin = self.exactskin;

    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        // Pick a random death sound
        if (random() > 0.5)
            sound (self, CHAN_VOICE, self.death_sound, 1, ATTN_NORM);
        else 
            sound (self, CHAN_VOICE, self.death_sound2, 1, ATTN_NORM);

        // Pick a random fall direction
        if (random() > 0.5) elf_die1 ();
        else elf_dieb1 ();
    }
};

/*======================================================================
QUAKED monster_xmas_elf (1 0 0) (-16 -16 -24) (16 16 16) Ambush
======================================================================*/
void() monster_xmas_elf =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/xmas/mon_elf.mdl";        // Crazy Grinning Fool
    self.headmdl = "progs/xmas/h_elf.mdl";        // Shrunken Head
    self.gib1mdl = "progs/xmas/gib_elfcane.mdl";// Candy Cane
    self.gib1sound = GIB_IMPACT_METALA;
    
    precache_model (self.mdl);
    precache_model (self.headmdl);                // Grinning Pointy Hat
    precache_model (self.gib1mdl);                // Sugar Cane
    precache_model (MODEL_PROJ_ELF);            // Magic ice
    
    // Idle calm/angry
    self.idle_sound = "xmas/elf/idle.wav";        // Lots of giggling
    precache_sound (self.idle_sound);

    // Many different sight sounds (2 is very annoying)
    self.sight_sound = "xmas/elf/sight1.wav";    // Snarling sound
    self.sight_sound2 = "xmas/elf/sight2.wav";    // DIE HUMAN!?!
    self.sight_sound3 = "xmas/elf/sight3.wav";    // Screams
    self.sight_sound4 = "xmas/elf/sight4.wav";    // Screams
    precache_sound (self.sight_sound);
    precache_sound (self.sight_sound2);
    precache_sound (self.sight_sound3);
    precache_sound (self.sight_sound4);
    self.sight_count = 4;

    // Elf 1 - Melee/Jump attacks - cane impacts
    precache_sound ("xmas/elf/cane_swipe1.wav");
    precache_sound ("xmas/elf/cane_swipe2.wav");
    precache_sound ("xmas/elf/cane_hit1.wav");
    precache_sound ("xmas/elf/cane_hit2.wav");
    precache_sound ("xmas/elf/jump1.wav");
    precache_sound ("xmas/elf/jump2.wav");
    
    // Elf 2 - Magic attacks
    precache_sound ("xmas/elf/magic.wav");
    precache_sound ("xmas/elf/attack1.wav");
    precache_sound ("xmas/elf/miss.wav");
    
    // precache sight, pain and death (changes on type)
    self.pain_sound = "xmas/elf/pain.wav";
    self.death_sound = "xmas/elf/death1.wav";
    self.death_sound2 = "xmas/elf/death2.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.death_sound);
    precache_sound (self.death_sound2);
    
    if (self.spawnflags & MON_ELF_MAGIC) {
        if (self.health < 1) self.health = 75;
        self.gibhealth = -25;
        self.th_missile = elf_magic1;
        self.exactskin = 2;
    }
    else {
        if (self.health < 1) self.health = 30;
        self.gibhealth = -20;
        self.th_jump = elf_leap1;
        self.th_jumpexit = elf_leap7;
        // pick a random one instead
        if (self.exactskin < 0 || self.exactskin > 1) self.exactskin = 0;
        if (self.exactskin == 0 && random() < 0.5) self.exactskin = 1;
    }
    
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TINY;
    self.gibbed = FALSE;                // Still in one peice
    self.blockudeath = TRUE;            // No humanoid death sound
    self.pain_flinch = 10;                // Light pain threshold
    self.steptype = FS_TYPELIGHT;        // Very light feet
    self.meleeoffset = '20 0 20';        // Cane attack offset
    self.attack_offset = '10 0 4';        // Magic attack offset
    self.poisonous = FALSE;                // Not Supported
    self.deathstring = " was torn to pieces by a Elf\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    self.th_checkattack = ElfCheckAttack;
    self.th_stand = elf_stand1;
    self.th_walk = elf_walk1;
    self.th_run = elf_run1;
    self.th_melee = elf_cane1;
    self.th_slide = elf_slide1;
    self.th_pain = elf_pain;
    self.th_die = elf_die;

    self.classtype = CT_MONELF;
    self.classgroup = CG_XMAS;
    self.classmove = MON_MOVEWALK;

    monster_start();
};