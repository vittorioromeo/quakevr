//==============================================================================
//CYBERDEMON
$origin 0 0 64
$base base     
$skin skin
$frame stand1 stand2 stand3 stand4 stand5 stand6
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7
$frame shoot1 shoot2 shoot3 shoot4 shoot5 shoot6 shoot7 shoot8

// CyberDemonFireMissile
// Adapted for Drake.
void() CyberDemonFireMissile = {
    local vector dir, org;

    if (!self.enemy) return;
    if (self.health < 1) return;

    // check for enemy origin (based on sight test)
    // if cannot see enemy fire at last known location
    ai_trackenemy();
    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "cyber/cybfire.wav", 1, ATTN_NORM);
    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);
    dir = normalize(self.attack_track - org);
    Launch_Missile (org, dir, '0 0 0', CT_PROJ_CYBER, 1000);
    self.attack_finished = time + 0.8;
};

// STAND -- (PM:  Originally had stand1 only.  Use MF's unused frames.
void() cyber_stand1  =[ $stand1,  cyber_stand2  ] {ai_stand();};
void() cyber_stand2  =[ $stand2,  cyber_stand3  ] {ai_stand();};
void() cyber_stand3  =[ $stand3,  cyber_stand4  ] {ai_stand();};
void() cyber_stand4  =[ $stand4,  cyber_stand5  ] {ai_stand();};
void() cyber_stand5  =[ $stand5,  cyber_stand6  ] {ai_stand();};
void() cyber_stand6  =[ $stand6,  cyber_stand7  ] {ai_stand();};
void() cyber_stand7  =[ $stand5,  cyber_stand8  ] {ai_stand();};
void() cyber_stand8  =[ $stand4,  cyber_stand9  ] {ai_stand();};
void() cyber_stand9  =[ $stand3,  cyber_stand10 ] {ai_stand();};
void() cyber_stand10 =[ $stand2,  cyber_stand1  ] {ai_stand();};

// WALK/RUN - - - - - - - - - - - - - -
void(float mad) cyber_anim = {
    if ((self.walkframe < 0) || (self.walkframe >= 7))
        self.walkframe = 0;

    self.frame = $walk1 + self.walkframe;
    self.walkframe = self.walkframe + 1;

    // Update 12/30/09:  Change hoof timings to match Doom.
    if (self.flags & FL_ONGROUND)
    {   // Footstep.
        if ( self.frame == $walk1 )
            sound (self, 6, "cyber/cybhoof.wav", 1, ATTN_NORM);
        else if ( self.frame == $walk6 )
            sound (self, 6, "cyber/cybmet.wav", 1, ATTN_NORM);
    }

    // Update 7/24/09:  Cyber moves 16 every 3 frames in Doom, and Doom ran
    // at 35 fps.  16 * 35 / 3 = ~186.666... per second.  Was 16, now 18.
    if (mad) {
        ai_run (18);
        if (prandom() < 3)
            sound (self, CHAN_VOICE, "cyber/cybidle.wav", 1, ATTN_IDLE);
    } else
        ai_walk (18);
};

void() cyber_reset = {
    if (( self.frame < $walk1 ) || ( self.frame > $walk7 )) self.walkframe = 0;
};

void() cyber_walk2  =[ $walk1,  cyber_walk2  ] {cyber_anim(FALSE);};
void() cyber_walk1  = {cyber_reset(); cyber_walk2();};

void() cyber_run2   =[ $walk1,  cyber_run2   ] {cyber_anim(TRUE);};
void() cyber_run1   = {cyber_reset(); cyber_run2();};


// ATTACK

// The original cyber patch skipped attacks if enemy was invisible or dead.
// In the older Dooms, the cyberdemon always shoots three rockets (unless
// interrupted by pain or death), even if enemy is invisible or dead.

// Update 12/30/09:  Timing closer to Doom.
// Frames in Doom is 6/shoot/24/shoot/24/shoot/12.  Doom runs at 35 fps.
void() cyber_missile1  =[ $shoot1,  cyber_missile2  ] {ai_face(); self.shots_left = 3;};
void() cyber_missile2  =[ $shoot3,  cyber_missile3  ] {ai_face();};
void() cyber_missile3  =[ $shoot4,  cyber_missile4  ] {ai_face();
    CyberDemonFireMissile();
    self.shots_left = self.shots_left - 1;
};
void() cyber_missile4  =[ $shoot4,  cyber_missile5  ] {ai_face();};
void() cyber_missile5  =[ $shoot5,  cyber_missile6  ] {ai_face();};
void() cyber_missile6  =[ $shoot6,  cyber_missile7  ] { if (self.shots_left < 1) {self.frame = $shoot1; self.think = cyber_run1;} ai_face(); };
void() cyber_missile7  =[ $shoot7,  cyber_missile8  ] {ai_face();};
void() cyber_missile8  =[ $shoot8,  cyber_missile2  ] {ai_face();};

    // PAIN
    // Pain duration was 10/35 in Doom2.  10/35 = 0.285714...
void() cyber_pain1 =[ $shoot3,  cyber_pain2  ] {self.nextthink = time + 0.1857;};   // Could be T+0.2.
void() cyber_pain2 =[ $shoot1,  cyber_run1   ] {};

void(entity inflictor, entity attacker, float damage) cyber_pain = {
    // Monsters in Doom are allowed to attack anytime after taking damage.
    self.attack_finished = 0;   // We're awake now!  (reactiontime = 0)

    // Don't make multiple pain sounds right after each other.
    // PM:  However, that was the case in Doom, so make sure the time is minimal.
    if (self.pain_finished > time) return;

    // In Doom, the cyberdemon did show pain.  However, the chance of that
    // happening was 20/255 per attack.  In addition, the stun/interruption
    // lasted a little over a fourth of a second.
    
    // To keep Mr. C. faithful to his incarnation in Doom, the Nightmare
    // no pain rules of Drake (which mirrors Quake2's) are not used here.
    if (prandom() < 20) {
        sound (self, CHAN_VOICE, "cyber/cybpain.wav", 1, ATTN_NORM);
        self.pain_finished = time + 0.1;    // Just one frame.
        self.ryuflags = self.ryuflags | RYU_JUSTHIT;
        cyber_pain1();
    }
};

// DEATH
// Modified SpawnExplosion to use Drake's Tent functions.
// Also, use Rogue's TE_EXPLOSION2 effect for fewer particles per explosion.
//void(vector p) SpawnExplosion = {Rocket_Explosion2 (p, 96, 16);};

void() cyber_die1  =[ $walk1,  cyber_die2  ] {     
    // In Doom, the cyderdemon death cry was heard everywhere at full volume.
    sound (self, 7, "cyber/cybdie.wav", 1, ATTN_NONE);
    ai_back(3);
};
void() cyber_die2  =[ $walk2,  cyber_die3  ] {ai_back(3);};
void() cyber_die3  =[ $walk3,  cyber_die4  ] {ai_back(3);};
void() cyber_die4  =[ $walk4,  cyber_die5  ] {ai_back(3);};
void() cyber_die5  =[ $walk5,  cyber_die6  ] {ai_back(3);};
void() cyber_die6  =[ $walk6,  cyber_die7  ] {ai_back(3);};
void() cyber_die7  =[ $walk7,  cyber_die8  ] {ai_back(3);};
void() cyber_die8  =[ $walk1,  cyber_die9  ] {ai_back(3);
    sound (self, 5, "cyber/servo.wav", 1, ATTN_NORM);
};
void() cyber_die9  =[ $shoot1, cyber_die10 ] 
{   ai_back(3);
    SpawnExplosion (EXPLODE_MED,self.origin + '0 20 60',"weapons/r_exp3a.wav");
};
void() cyber_die10 =[ $shoot2, cyber_die11 ] {ai_back(3);};
void() cyber_die11 =[ $shoot2, cyber_die12 ] { ai_back(3); SpawnExplosion(EXPLODE_MED,self.origin + '0 -20 -40',"weapons/r_exp3a.wav"); };
void() cyber_die12 =[ $shoot3, cyber_die13 ] { sound (self, CHAN_VOICE, "cyber/cybdie.wav", 1, ATTN_NORM); ai_back(3); };
void() cyber_die13 =[ $shoot3, cyber_die14 ] {
    SpawnExplosion(EXPLODE_MED,self.origin + '-20 0 20',"weapons/r_exp3a.wav");
    SpawnExplosion(EXPLODE_MED,self.origin + '20 0 20',"");
    SpawnExplosion(EXPLODE_MED,self.origin + '0 -20 20',"");
    SpawnExplosion(EXPLODE_MED,self.origin + '0 20 20',"");
    ai_back(3);
};
void() cyber_die14 =[ $shoot3, cyber_die15 ]  {ai_back(3);};
void() cyber_die15 =[ $shoot2, cyber_die16 ]  {
    SpawnExplosion(EXPLODE_MED,self.origin + '-20 0 40',"");
    SpawnExplosion(EXPLODE_MED,self.origin + '20 0 40',"");
    SpawnExplosion(EXPLODE_MED,self.origin + '0 -20 40',"");
    SpawnExplosion(EXPLODE_MED,self.origin + '0 20 40',"");
    ai_back(2);
};
void() cyber_die16 =[ $shoot2, cyber_die17 ] { ai_back(3); };
void() cyber_die17 =[ $shoot3, cyber_die18 ] { ai_back(1);
    SpawnExplosion(EXPLODE_MED,self.origin + '-10 0 70',"");
    SpawnExplosion(EXPLODE_MED,self.origin + '10 0 70',"");
    SpawnExplosion(EXPLODE_MED,self.origin + '0 -10 70',"");
    SpawnExplosion(EXPLODE_MED,self.origin + '0 10 70',"");
};
void() cyber_die18 =[ $shoot3, cyber_die18    ] {
    self.solid = SOLID_NOT;
    // Update 1/4/10:  Add some explosive eye candy.
    newmis = spawn();
    setsize (newmis, '0 0 0', '0 0 0');        
    setorigin (newmis, self.origin + '0 0 24');
    SUB_Think (newmis, BecomeTheBigFire);
    SUB_Think (newmis, FlameWave);
    Quake_Self (300, 0, self.origin, 0, 0.5);
    SpawnExplosion(EXPLODE_MED,self.origin + '-10 0 40',"weapons/r_exp3a.wav");
    SpawnExplosion(EXPLODE_MED,self.origin + '10 0 40',"");
    SpawnExplosion(EXPLODE_MED,self.origin + '0 -10 40',"");
    SpawnExplosion(EXPLODE_MED,self.origin + '0 10 40',"");
    sound (self, 5, "cyber/boom.wav", 1, ATTN_NORM);
    ThrowHead();
    ThrowGib (13,1);
    ThrowGib (12,5);
    ThrowGib (11,1);
    // Stop thinking and hide entity (prevent wraith_death15 loop)
    self.nextthink = LARGE_TIMER;
    self.think = SUB_Null;
    entity_hide(self);
};

//============
// --> QUAKED monster_cyberdemon (1 0 0) (-32 -32 -24) (32 32 64) Ambush
//============
void() monster_cyberdemon = {
    if (deathmatch) { remove(self); return; }

    precache_model ("progs/drake/cyber.mdl");
    precache_model ("progs/drake/h_cyber.mdl");
    precache_model ("progs/drake/leg_gib.mdl");
    precache_model ("progs/missile.mdl");
    precache_model ("progs/drake/s_expbig.spr");
    precache_model ("progs/drake/s_wave.spr");
    precache_model ("progs/gib1.mdl");
    precache_model ("progs/gib3.mdl");
    precache_sound ("soldier/rocket_hit.wav");
    precache_sound ("cyber/cybsee.wav");
    precache_sound ("cyber/cybdie.wav");
    precache_sound ("cyber/cybhoof.wav");
    precache_sound ("cyber/cybpain.wav");    
    precache_sound ("cyber/cybmet.wav");
    precache_sound ("cyber/cybfire.wav");
    precache_sound ("cyber/cybidle.wav");
    precache_sound ("cyber/boom.wav");
    precache_sound ("cyber/servo.wav");
    
    self.headmdl = "progs/drake/h_cyber.mdl";
    self.gib1mdl = "progs/gib1.mdl";
    self.gib2mdl = "progs/gib3.mdl";
    self.gib3mdl = "progs/drake/leg_gib.mdl";
   
    // Note:  In Doom, cyberdemon had width 40 and height 110.
    // Shambler hullbox is width 32 and height 88.

    self.mdl = "progs/drake/cyber.mdl";
    if (self.bboxtype < 1) self.bboxtype = BBOX_CYBER;
    if (self.health < 1) self.health = 4000; // Health in classic Doom (and Doom3 for that matter) is 4000.
    self.gibhealth = MON_NEVERGIB;        // Cannot be gibbed by weapons
    self.mass       = 0;    // Was MASS_SUPER.  Now set to immovable.
    self.blockudeath = TRUE;            // no humanoid death sound
    self.th_stand   = cyber_stand1;
    self.th_walk    = cyber_walk1;
    self.th_run     = cyber_run1;
    self.th_die     = cyber_die1;
    // Update 12/30/2009:  Removed melee now that Doom style movement is in.
    self.th_missile = cyber_missile1;
    self.th_pain    = cyber_pain;
    self.sight_sound = "cyber/cybsee.wav";
    //self.th_checkattack = DoomCheckAttack;
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    self.th_updmissile = army_rocket_think;    // Update flying rocket

    // Immune to all splash damage as in Doom.
    // Also, immune to burning because his model is too big (i.e., looks bad).
    // Has boss flag because its Heretic counterpart, the Maulotaur, has it.
    //self.xfl        = XFL_FIREANGEL | XFL_NO_BURN | XFL_BOSS;

    // PM:  I don't think Doom has yaw speeds, and monsters can turn 180 degrees at any given moment.
    self.yaw_speed = 60;        // Fast, but not instant turning.
    self.attack_offset = '31 -20 36';
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    self.classtype = CT_MONCYBERDEMON;
    self.classgroup = CG_DEMON;
    self.classmove = MON_MOVEWALK;
    
    if ( (random () < 0.500) ) {
        if (self.deathstring == "")  self.deathstring = " dances the cyberboogie with a Cyberdemon\n";
    } else {
        if (self.deathstring == "")  self.deathstring = " was splattered by a Cyberdemon\n";
    }
    
    monster_start ();
};


//===========================/  END OF FILE  /===========================//
