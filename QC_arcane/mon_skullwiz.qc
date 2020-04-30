/*==============================================================================
SKULL WIZARD (Hexen2 model by Raven Software)
==============================================================================*/

// Fall backwards, vanish and robes fall down
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12 death13 death14 death15

// Summon minion
$frame summon1 summon2 summon3 summon4 summon5 summon6 summon7 summon8
$frame summon9 summon10 summon11 summon12 summon13 summon14 summon15 summon16
$frame summon17 summon18 summon19 summon20 summon21 summon22 summon23 summon24
$frame summon25 summon26 summon27 summon28 summon29 summon30

// Pain A
$frame painA1 painA2 painA3 painA4 painA5 painA6 painA7 painA8
$frame painA9 painA10 painA11 painA12

// Transition - Move from upright to crouch
$frame transA1 transA2 transA3 transA4 transA5 transA6 transA7 transA8
$frame transA9 transA10 transA11 transA12

// Cast fireball spell (frame 12 - fire)
$frame spellA1 spellA2 spellA3 spellA4 spellA5 spellA6 spellA7 spellA8
$frame spellA9 spellA10 spellA11 spellA12 spellA13 spellA14 spellA15

// Cast teleport spell (touching book)
$frame spellB1 spellB2 spellB3 spellB4 spellB5 spellB6 spellB7 spellB8
$frame spellB9 spellB10 spellB11 spellB12 spellB13 spellB14 spellB15

// Transition - Move from crouch to upright
$frame transB1 transB2 transB3 transB4 transB5 transB6 transB7

// Default stand - breathing
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8
$frame stand9 stand10 stand11 stand12 stand13 stand14 stand15 stand16
$frame stand17 stand18 stand19

// Default walk - very slow
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12 walk13 walk14 walk15 walk16
$frame walk17 walk18 walk19 walk20 walk21 walk22 walk23 walk24

// Teleport
$frame tele1 tele2 tele3 tele4 tele5 tele6 tele7 tele8 tele9

float SKULLW_STAND = 1;
float SKULLW_WALK = 2;
float SKULLW_HEIGHT = 96;
float SKULLW_DISTMIN = 256;
float SKULLW_DISTMAX = 512;

//============================================================================
void()    skullw_stand1    =[    $stand1,    skullw_stand2    ] {
self.idlebusy=SKULLW_STAND; monster_idle_sound();ai_stand();};
void()    skullw_stand2    =[    $stand18,    skullw_stand3    ] {ai_stand();};
void()    skullw_stand3    =[    $stand16,    skullw_stand4    ] {ai_stand();};
void()    skullw_stand4    =[    $stand14,    skullw_stand5    ] {ai_stand();};
void()    skullw_stand5    =[    $stand12,    skullw_stand6    ] {ai_stand();};
void()    skullw_stand6    =[    $stand3,    skullw_stand7    ] {ai_stand();};
void()    skullw_stand7    =[    $stand5,    skullw_stand8    ] {ai_stand();};
void()    skullw_stand8    =[    $stand7,    skullw_stand9    ] {ai_stand();};
void()    skullw_stand9    =[    $stand9,    skullw_stand10    ] {ai_stand();};
void()    skullw_stand10    =[    $stand11,    skullw_stand11    ] {ai_stand();};
void()    skullw_stand11    =[    $stand13,    skullw_stand12    ] {ai_stand();};
void()    skullw_stand12    =[    $stand15,    skullw_stand13    ] {ai_stand();};
void()    skullw_stand13    =[    $stand17,    skullw_stand14    ] {ai_stand();};
void()    skullw_stand14    =[    $stand19,    skullw_stand1    ] {ai_stand();};

//============================================================================
void()    skullw_walk1    =[    $walk1,        skullw_walk2    ] {monster_footstep(FALSE);
self.idlebusy=SKULLW_WALK; monster_idle_sound();ai_walk(2);};
void()    skullw_walk2    =[    $walk3,        skullw_walk3    ] {ai_walk(5);};
void()    skullw_walk3    =[    $walk4,        skullw_walk4    ] {ai_walk(5);};
void()    skullw_walk4    =[    $walk5,        skullw_walk5    ] {ai_walk(4);};
void()    skullw_walk5    =[    $walk7,        skullw_walk6    ] {ai_walk(4);};
void()    skullw_walk6    =[    $walk8,        skullw_walk7    ] {ai_walk(2);};
void()    skullw_walk7    =[    $walk9,        skullw_walk8    ] {ai_walk(2);};
void()    skullw_walk8    =[    $walk11,    skullw_walk9    ] {ai_walk(2);};
void()    skullw_walk9    =[    $walk12,    skullw_walk10    ] {monster_footstep(FALSE); ai_walk(2);};
void()    skullw_walk10    =[    $walk13,    skullw_walk11    ] {ai_walk(5);};
void()    skullw_walk11    =[    $walk15,    skullw_walk12    ] {ai_walk(5);};
void()    skullw_walk12    =[    $walk16,    skullw_walk13    ] {ai_walk(4);};
void()    skullw_walk13    =[    $walk17,    skullw_walk14    ] {ai_walk(4);};
void()    skullw_walk14    =[    $walk19,    skullw_walk15    ] {ai_walk(2);};
void()    skullw_walk15    =[    $walk20,    skullw_walk16    ] {ai_walk(2);};
void()    skullw_walk16    =[    $walk22,    skullw_walk1    ] {ai_walk(3);};

//============================================================================
// WALK -> STAND
void()    skullw_transA1    =[    $transA5,    skullw_transA2    ] {};
void()    skullw_transA2    =[    $transA7,    skullw_transA3    ] {};
void()    skullw_transA3    =[    $transA9,    skullw_transA4    ] {};
void()    skullw_transA4    =[    $transA11,    skullw_stand1    ] {};

// STAND -> WALK
void()    skullw_transB1    =[    $transB3,    skullw_transB2    ] {};
void()    skullw_transB2    =[    $transB5,    skullw_transB3    ] {};
void()    skullw_transB3    =[    $transB7,    skullw_walk1    ] {};

//----------------------------------------------------------------------
void() skullw_stand =
{
    if (self.idlebusy == SKULLW_WALK) skullw_transA1();
    else skullw_stand1();
};

//----------------------------------------------------------------------
void() skullw_walk =
{
    if (self.idlebusy == SKULLW_STAND) skullw_transB1();
    else skullw_walk1();
};

//============================================================================
void()    skullw_run1    =[    $stand1,        skullw_run2    ] {ai_face();
    monster_idle_sound();ai_run(0);};
void()    skullw_run2    =[    $stand18,        skullw_run3    ] {ai_face();ai_run(0);};
void()    skullw_run3    =[    $stand16,        skullw_run4    ] {ai_run(0);};
void()    skullw_run4    =[    $stand14,        skullw_run5    ] {ai_run(0);};
void()    skullw_run5    =[    $stand12,        skullw_run6    ] {ai_face();ai_run(0);};
void()    skullw_run6    =[    $stand3,        skullw_run7    ] {ai_face();ai_run(0);};
void()    skullw_run7    =[    $stand5,        skullw_run8    ] {ai_run(0);};
void()    skullw_run8    =[    $stand7,        skullw_run9    ] {ai_run(0);};
void()    skullw_run9    =[    $stand9,        skullw_run10] {ai_face();ai_run(0);};
void()    skullw_run10=[    $stand11,        skullw_run11] {ai_face();ai_run(0);};
void()    skullw_run11=[    $stand13,        skullw_run12] {ai_run(0);};
void()    skullw_run12=[    $stand15,        skullw_run13] {ai_run(0);};
void()    skullw_run13=[    $stand17,        skullw_run14] {ai_run(0);};
void()    skullw_run14=[    $stand19,        skullw_run1    ] {ai_run(0);};

//============================================================================
// MELEE - Teleportation
//============================================================================
void() skullw_tfin1 =[ $tele9, skullw_tfin2 ] {
    self.velocity = '0 0 0';
    // Always phase in facing the enemy
    self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
    self.angles_y = self.ideal_yaw;
    sound (self, CHAN_AUTO, "skullwiz/blinkin.wav", 1, ATTN_NORM);
};
void() skullw_tfin2 =[ $tele7, skullw_tfin3 ] {ai_face();};
void() skullw_tfin3 =[ $tele5, skullw_tfin4 ] {ai_face();};
void() skullw_tfin4 =[ $tele3, skullw_tfin5 ] {ai_face();};
void() skullw_tfin5 =[ $tele1, skullw_run1 ] {ai_face();
    self.bodyphased = MONAI_SKULLWSOLID;
    self.takedamage = DAMAGE_AIM;
};

//----------------------------------------------------------------------
// Find new teleport location
//----------------------------------------------------------------------
void() skullw_blink = {
    local float dist, edist, blinkfinished, blinkcount;
    local vector org, org_ofs, angle_vec;

    blinkfinished = TRUE; blinkcount = 0;
    org = angle_vec = org_ofs = '0 0 0'; 
    org_ofs_z = self.height;
    self.oldorigin = self.origin;
    
    // Trace upwards to find ceiling or maximum teleport height
    traceline(self.origin, self.origin+org_ofs, TRUE, self);
    org_ofs = trace_endpos;
    
    // Loop around for possible locations
    while(blinkfinished) {
        blinkcount = blinkcount + 1;        // next loop
        angle_vec_y = random() * 360;        // Random direction
        makevectors(angle_vec);                // make vector
        
        // Generate random angle and trace teleport direction
        dist = self.distmin + (random() * self.distmax);
        org = org_ofs + (v_forward * dist);
        traceline(org_ofs, org, TRUE, self);
        
        // Check distance is far enough away from enemy
        // Move away from any walls (traceline is surface contact)
        dist = vlen(trace_endpos - org_ofs) - 32;
        org = org_ofs + (v_forward * dist);
        edist = vlen(self.enemy.origin - org);

        // Is the enemy far enough away?
        if (edist > MONAI_MELEESKULLW) {
            // Trace down to floor below new point
            traceline(org, org - '0 0 512', TRUE, self);
            org = trace_endpos + self.view_ofs;
            
            // The teleport fuction is working with a radius command
            // and this will ignore walls and just find any position
            // in all directions. This can lead to the skull wizard
            // teleporting behind walls or other parts of the map!
            // This can be prevent in two ways:
            // 1) linesight between skull wizard and player
            // - can leads to less interesting locations (always insight)
            // 2) linesight between new location and old
            // - This allows skull wizard to teleport around corner
            //   and still be within the same area!
            //
            // Trace through monsters and check trace fraction
            traceline(org, self.origin, TRUE, self);
            if (trace_fraction == 1) blinkfinished = FALSE;
        }

        // Been looping too long, back to original location
        if (blinkcount > 10) {
            blinkfinished = FALSE;
            org = self.origin;
        }
    }
    
    // Move to new location
    self.solid = SOLID_SLIDEBOX;                // Standard monster movement
    setmodel(self, self.mdl);                    // Setup model
    setsize (self, self.bbmins, self.bbmaxs);    // Restore BB size
    setorigin(self, org);                        // move to new location
    
    // Test if new location is solid?
    if (!walkmove (0, 0)) {
        self.solid = SOLID_NOT;                    // No world interaction yet
        setmodel(self,"");                        // Turn off model
        setorigin(self, self.oldorigin);        // restore original location
        self.nextthink = time + 1;                // Keep checking
        self.think = skullw_blink;
    }
    else skullw_tfin1();
};

//----------------------------------------------------------------------
// Cycle around teleport chain lists instead
//----------------------------------------------------------------------
void() skullw_waitforlink =
{
    self.nextthink = time + 0.5 + random();
    self.think = skullw_waitforlink;
    setorigin(self, self.movelast.origin);    // move to new location
    self.enemydist = range_distance(self.enemy, FALSE);

    // Is the enemy too close or dead?
    if (self.enemydist > self.movelast.distance || self.enemy.health < 1) {
        // Move to new location
        self.solid = SOLID_SLIDEBOX;                // Standard monster movement
        setmodel(self, self.mdl);                    // Setup model
        setsize (self, self.bbmins, self.bbmaxs);    // Restore BB size
    
        // Test if new location is solid?
        if (!walkmove (0, 0)) {
            self.solid = SOLID_NOT;                    // No world interaction yet
            setmodel(self,"");                        // Turn off model
        }
        else skullw_tfin1();
    }
};

//----------------------------------------------------------------------
void() skullw_checkblinklist = {
        // Does the blink list exist already?
    if (self.movelast.classtype != CT_SKULLTELEPORT) {
        self.movelast = find(world,targetname,self.target2);
        // Does the entity chain exist?
        if (self.movelast.classtype == CT_SKULLTELEPORT)
            skullw_waitforlink();
        else {
            // target2 is not valid, do regular teleport
            self.target2 = "";
            skullw_blink();
        }
    }
    else {
        // Move forward in chain and try next spawn location
        self.movelast = self.movelast.enemy;
        skullw_waitforlink();
    }
};

//----------------------------------------------------------------------
// touch book and phase out
//----------------------------------------------------------------------
void() skullw_tele1    =[ $spellB3,    skullw_tele2    ] {
    self.pain_finished = time + 1;                // no pain during teleportation
    if (random() < 0.5) sound (self, CHAN_VOICE, "skullwiz/blinkspk1.wav", 1, ATTN_NORM);
    else sound (self, CHAN_VOICE, "skullwiz/blinkspk2.wav", 1, ATTN_NORM);
};
void() skullw_tele2    =[ $spellB5,    skullw_tele3 ] {};
void() skullw_tele3    =[ $spellB7,    skullw_tele4 ] {};
void() skullw_tele4    =[ $spellB9,    skullw_tele6 ] {};
void() skullw_tele6    =[ $spellB13,    skullw_tele8 ] {};
void() skullw_tele8    =[ $tele1,        skullw_tele9 ] {
    self.takedamage = DAMAGE_NO;    // No damage or impact, phasing out
    self.solid = SOLID_NOT;
    self.bodyphased = MONAI_SKULLWINVIS;
    particle_explode(self.origin, 100, 1, self.part_style, PARTICLE_BURST_SKULLUP);
    sound (self, CHAN_VOICE, "skullwiz/blinkout.wav", 1, ATTN_NORM);
};
void() skullw_tele9    =[ $tele3,    skullw_tele10 ] {};
void() skullw_tele10=[ $tele5,    skullw_tele11 ] {};
void() skullw_tele11=[ $tele7,    skullw_tele12 ] {};
void() skullw_tele12=[ $tele9,    skullw_tele13 ] {};
void() skullw_tele13 = {
    setmodel(self,"");
    // Check if a chain list has been defined
    if (self.target2 != "") skullw_checkblinklist();
    else skullw_blink();
};

//============================================================================
// Rocket Skull Attack (Regular)
//============================================================================
void(vector orgofs) skullw_fire =
{
    local vector org, dir;

    ai_face();
    self.effects = self.effects | EF_MUZZLEFLASH;
    if (random() < 0.5) sound (self, CHAN_WEAPON, "skullwiz/skull1.wav", 1, ATTN_NORM);
    else sound (self, CHAN_WEAPON, "skullwiz/skull2.wav", 1, ATTN_NORM);

    makevectors (self.angles);
    org = self.origin + attack_vector(orgofs);
    dir = normalize(self.enemy.origin - org);

    self.attack_speed = SPEED_SWIZMISSILE + (skill * SPEED_SWIZSKILL);
    Launch_Missile (org, dir, '0 0 0', CT_PROJ_SKULLW, self.attack_speed);
};

//----------------------------------------------------------------------
void() skullw_attack1    =[    $spellA3,    skullw_attack2    ] {
sound (self, CHAN_VOICE, "skullwiz/attack.wav", 1, ATTN_NORM);};
void() skullw_attack2    =[    $spellA5,    skullw_attack3    ] {ai_face();};
void() skullw_attack3    =[    $spellA7,    skullw_attack4    ] {ai_face();};
void() skullw_attack4    =[    $spellA10,    skullw_attack5    ] {ai_face();};
void() skullw_attack5    =[    $spellA11,    skullw_attack6    ] {ai_face();};
void() skullw_attack6    =[    $spellA12,    skullw_attack7    ] {skullw_fire('30 0 16');};
void() skullw_attack7    =[    $spellA13,    skullw_attack8    ] {};
void() skullw_attack8    =[    $spellA15,    skullw_run1        ] {};

//============================================================================
// Poison Skull Attack (Guardians)
//============================================================================
void() skullw_poison_touch =
{
    if (other.solid == SOLID_TRIGGER) return;
    if (other.classtype == CT_TEMPSTREAM) return;
    if (other.health > 0) return;
    if (self.attack_finished > time) return;

    self.attack_finished = time + 1;
    self.modelindex = 0;            // Make sure no model
    self.model = "";
    self.think = SUB_Remove;
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void() skullw_poison_think =
{
    // Randomly drift upwards and slow down forward movement
    self.velocity_z = self.velocity_z + 10 + (random() * 20);
    self.velocity = self.velocity - (self.velocity * 0.125);
    
    self.frame = self.frame + 1;
    if (self.frame > self.count) SUB_Remove();
    else self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void() skullw_guardian =
{
    local float poison_loop;
    local vector poison_angles, org, dir;
    
    // Double check the skull wizard has poisonous flag set
    if (!self.poisonous) return;
    
    // Remove projectile
    entity_remove(self, 1);
    // Play explosion sound
    sound (self, CHAN_WEAPON, "skullwiz/poison_hit.wav", 1, ATTN_NORM);
    
    // Apply any direct damage and poison effect
    if (other.takedamage) {
        T_Damage (other, self, self.owner, DAMAGE_SWIZPOISON, DAMARMOR);
        if (other.flags & FL_CLIENT) PoisonDeBuff(other);
    }
    
    poison_loop = 0;
    poison_angles = '0 0 0';
    
    while (poison_loop < 360) {
        newmis = spawn();
        newmis.classtype = CT_TEMPSTREAM;
        newmis.movetype = MOVETYPE_FLY;        // Fly, no gravity
        newmis.solid = SOLID_TRIGGER;
        setmodel(newmis, SBURST_POISON);

        org = self.origin;
        org_z = org_z + (crandom() * 24);
        setorigin(newmis, org);
        setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);

        poison_angles_y = poison_loop;
        makevectors(poison_angles);
        dir = vecrand(0,50,FALSE);
        dir = dir + (v_forward * (400 + random() * 50));
        newmis.velocity = dir;
        newmis.count = 6;
        newmis.frame = rint(random()*2);

        // If DP engine active remove particle shadow
        if (engine == ENG_DPEXT) newmis.effects = newmis.effects + EF_NOSHADOW;

        newmis.think = skullw_poison_think;
        newmis.touch = skullw_poison_touch;
        newmis.nextthink = time + 0.1;
        
        poison_loop = poison_loop + rint(random()*15);
    }
};

//============================================================================
// SUMMON MINION - Purple magic ball
//============================================================================
void() skullw_create_attachment =
{
    // Are the attachments setup yet?
    if (!self.attachment) {
        self.attachment = spawn();
        self.attachment.owner = self;
        self.attachment.classtype = CT_ATTACHMENT;
        self.attachment.mdl = MODEL_PROJ_SWBALL;
    }
};

//----------------------------------------------------------------------
void() skullw_finish_attachment =
{
    if (self.attachment) {
        setmodel(self.attachment, "");
        self.attachment.state = STATE_OFF;
    }
};

//----------------------------------------------------------------------
void() skullw_remove_attachment =
{
    if (self.attachment) {
        self.attachment.think = SUB_Remove;
        self.attachment.nextthink = time + 0.1;
    }
};

//----------------------------------------------------------------------
// Display a purple ball in the hand of the skull wizard
//----------------------------------------------------------------------
void(vector orgofs, float dbframe) skullw_sumball =
{
    local vector org;

    // Frame 0 is start of the sequence (move everything into place)
    if (dbframe == 0) {
        self.attachment.state = STATE_ON;
        setorigin(self.attachment, self.pos2);
        setmodel(self.attachment, self.attachment.mdl);
        setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);
        self.attachment.movetype = MOVETYPE_NONE;
        self.attachment.solid = SOLID_NOT;
        self.attachment.skin = 0;
        self.attachment.alpha = 0.85;
    }

    // Generate attachment in hand (left)
    makevectors(self.angles);
    org = self.origin + attack_vector(orgofs);
    setorigin(self.attachment, org);
    self.attachment.angles_y = rint(random()*359);
    self.attachment.frame = dbframe;

    // Create particles inside of skull wizard robes floating up
    // once lost soul is spawning, then switch particle location
    if (self.aflag == TRUE) particle_explode(self.pos2, 3, 1, self.part_style, PARTICLE_BURST_MINOTAUR);
    // Need to cut down on particles, too much visual noise
    //    else particle_debuff(self.origin, 16, rint(4+random()*4), self.part_style);
};

//----------------------------------------------------------------------
// Raise hand up to the sky and spawn purple particles from body
// Always check if there is space to spawn a lost soul first
// Block monster pain function so that the summons is completed
//----------------------------------------------------------------------
void() skullw_summon1 =[ $summon1, skullw_summon2 ] {
    ai_face();self.pain_finished = time + 2;};
void() skullw_summon2 =[ $summon3, skullw_summon3 ] {
    ai_face();skullw_sumball('22 -2 6',0);};
void() skullw_summon3 =[ $summon5, skullw_summon4 ] {
    ai_face(); skullw_sumball('23 -3 7',1);
    sound (self, CHAN_BODY, "skullwiz/summon.wav", 1, ATTN_NORM);
    /* The touch trigger is created early (4 frames) so its gets
    // a chance for a client to touch its bounding box
    // This is a giant trigger area to catch speedy players
    makevectors(self.angles);
    self.pos1 = self.origin + self.view_ofs + v_forward*64 + v_up*16;
    setup_minionspace(self.pos1, '-32 -32 -32', '32 32 32'); */
};
void() skullw_summon4 =[ $summon7, skullw_summon5 ] {skullw_sumball('24 -4 9',2);};
void() skullw_summon5 =[ $summon9, skullw_summon6 ] {skullw_sumball('26 -4 10',3);};
void() skullw_summon6 =[ $summon11, skullw_summon7 ] {skullw_sumball('26 -4 12',3);};
void() skullw_summon7 =[ $summon13, skullw_summon8 ] {
    // Find out if there is enough space to spawn a lost soul
    // This is really a tricky thing to test for ...
    // * pointcontents only checks against world space
    // * walkmove does not work properly on the lower plane face
    // * touch triggers only work when something touches not inside
    // * findradius will find a lot of entities depending on radius
    
    // Touch trigger version
    //self.aflag = test_minionspace();

    // Findradius version
    makevectors(self.angles);
    self.pos1 = self.origin + self.view_ofs + v_forward*64 + v_up*16;
    self.aflag = find_minionspace(self.pos1);
    
    // If the spawn locaiton all clear, spawn something!
    if (self.aflag == TRUE) {
        minion_lostsoul(self.pos1, self.enemy);
        self.pos2 = self.pos1 - '0 0 16';    // Where to spawn particles
        skullw_sumball('26 -4 14',4);
    }
};
void() skullw_summon8 =[ $summon15, skullw_summon9 ] {skullw_sumball('26 -4 16',4);};
void() skullw_summon9 =[ $summon17, skullw_summon10 ] {skullw_sumball('26 -4 19',4);};
void() skullw_summon10 =[ $summon19, skullw_summon11 ] {skullw_sumball('26 -5 23',4);};
void() skullw_summon11 =[ $summon21, skullw_summon12 ] {skullw_sumball('26 -6 30',3);};
void() skullw_summon12 =[ $summon23, skullw_summon13 ] {skullw_sumball('19 -7 32',1);};
void() skullw_summon13 =[ $summon25, skullw_summon14 ] {skullw_finish_attachment();
    if (self.aflag == TRUE) particle_explode(self.pos1, 15, 2, self.part_style, PARTICLE_BURST_MINOTAUR);};
void() skullw_summon14 =[ $summon27, skullw_summon15 ] {ai_face();};
void() skullw_summon15 =[ $summon29, skullw_run1 ] {
    ai_face();SUB_AttackFinished (1 + 2*random());};

//----------------------------------------------------------------------
// Both type of skull wizards go through the same magic function
//----------------------------------------------------------------------
void() skullw_magic =
{
    local entity miniondef;
    
    // Make sure the attachments are setup ready
    skullw_create_attachment();

    if (self.spawnflags & MON_SKULLWIZ_MINIONS && !self.minion_active) {
        // Check for minion template first
        miniondef = find(world,classname,"monster_skullwizminion");
        if (miniondef.classtype == CT_CACHELOSTSOUL) setup_minionsupport();
        else {
            // If template no available, warn and remove feature
            dprint("\b[SKULLWIZ]\b Cannot find minion template!\n");
            self.spawnflags = self.spawnflags - MON_SKULLWIZ_MINIONS;
        }
    }
    
    // Is the target the player or monster?
    if (self.enemy.flags & FL_CLIENT) {
        if (self.spawnflags & MON_SKULLWIZ_MINIONS) {
            // has the lost soul limit been reached?
            if (query_minionactive(self) == TRUE) skullw_summon1();
            else {
                if (self.minion_baseattack > 0 ) skullw_attack1();
                else skullw_run1();
            }
        }
        // Red Skull wizards attacks with skull rockets
        else skullw_attack1();
        
    }
    // Always attack monsters with skull rockets
    else skullw_attack1();
};
    
//============================================================================
void() skullw_phased = { 
    self.nextthink = time + 0.1;    // Keep looking for player
    self.think = skullw_phased;        // Keep looping
    if (random() < 0.02) 
        particle_explode(self.origin, rint(10+random()*10), 1, self.part_style, PARTICLE_BURST_SKULLUP);
    monster_idle_sound();             // High chance of idle sound
    ai_stand();                     // Keep looking
};

//----------------------------------------------------------------------
void() skullw_wakeup =
{
    // Reset all monster state functions back to default
    self.th_stand = skullw_stand;
    self.th_walk = skullw_walk;
    self.th_run = skullw_run1;
    self.th_missile = skullw_magic;
    self.th_melee = skullw_tele1;
    self.th_updmissile = skullw_guardian;

    // Temporarily restore model/bound box
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    setmodel(self, self.mdl);
    setsize (self, self.bbmins, self.bbmaxs);
        
    // Can the skull wizard move?
    if (!walkmove (0, 0)) {
        // Something blocking, teleport elsewhere
        self.solid = SOLID_NOT;
        setmodel(self,"");
        self.nextthink = time + 0.1;
        self.think = skullw_blink;
    }
    else skullw_tfin1();
};

//============================================================================
// Short body recoil (6 frames)
void()    skullw_painA1    =[    $painA2,    skullw_painA2    ] {};
void()    skullw_painA2    =[    $painA4,    skullw_painA3    ] {};
void()    skullw_painA3    =[    $painA6,    skullw_painA4    ] {};
void()    skullw_painA4    =[    $painA8,    skullw_painA5    ] {};
void()    skullw_painA5    =[    $painA10,    skullw_painA6    ] {};
void()    skullw_painA6    =[    $painA12,    skullw_run1    ]     {};

// Long expanded out version (10 frames)
void()    skullw_painB1    =[    $painA2,    skullw_painB3    ] {};
void()    skullw_painB3    =[    $painA3,    skullw_painB4    ] {};
void()    skullw_painB4    =[    $painA4,    skullw_painB5    ] {};
void()    skullw_painB5    =[    $painA5,    skullw_painB6    ] {};
void()    skullw_painB6    =[    $painA6,    skullw_painB7    ] {};
void()    skullw_painB7    =[    $painA7,    skullw_painB8    ] {};
void()    skullw_painB8    =[    $painA8,    skullw_painB9    ] {};
void()    skullw_painB9    =[    $painA9,    skullw_painB10    ] {};
void()    skullw_painB10    =[    $painA10,    skullw_painB11    ] {};
void()    skullw_painB11    =[    $painA11,    skullw_run1    ]     {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) skullw_pain =
{
    // Cannot feel pain when phased out
    if (self.bodyphased == MONAI_SKULLWINVIS) return;
    
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);
    skullw_finish_attachment();

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        if (random() < 0.5) sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
        if (self.pain_check == 1 || self.pain_check == 2) {
            if (random() < 0.85) skullw_painA1();    // classic, body recoil
            else skullw_painB1();                    // Really long version
        }
    }
};

//============================================================================
void() skullw_fadeaway =
{
    //    sound (self, CHAN_BODY, "skullwiz/fadeaway.wav", 1, ATTN_NORM);
    // Particle burst upwards as robes fade away
    particle_explode(self.origin, 100, 3, self.part_style, PARTICLE_BURST_SKULLUP);
    self.think = model_fade;
    self.nextthink = time + 0.1;
    self.ltime = self.nextthink;
};

//----------------------------------------------------------------------
void() skullw_explode =
{
    self.health = MON_NOGIBVELOCITY;    // Low gib directional modifier
    particle_explode(self.origin, 100, 3, PARTICLE_BURST_WHITE, PARTICLE_BURST_SKULLUP);
    self.gib2skin = self.exactskin;        // Update book skin
    ThrowGib(11,1); ThrowGib(12,1);        // Throw head and book
};

//----------------------------------------------------------------------
void()    skullw_death1    =[    $death2,    skullw_death2    ] {ai_face();};
void()    skullw_death2    =[    $death3,    skullw_death3    ] {ai_face();};
void()    skullw_death3    =[    $death4,    skullw_death4    ] {ai_face();self.solid = SOLID_NOT;};
void()    skullw_death4    =[    $death5,    skullw_death5    ] {ai_face();};
void()    skullw_death5    =[    $death6,    skullw_death6    ] {skullw_explode();};
void()    skullw_death6    =[    $death7,    skullw_death7    ] {};
void()    skullw_death7    =[    $death8,    skullw_death8    ] {};
void()    skullw_death8    =[    $death9,    skullw_death9    ] {};
void()    skullw_death9    =[    $death10,    skullw_death10    ] {};
void()    skullw_death10    =[    $death11,    skullw_death11    ] {};
void()    skullw_death11    =[    $death12,    skullw_death12    ] {};
void()    skullw_death12    =[    $death13,    skullw_death13    ] {};
void()    skullw_death13    =[    $death14,    skullw_death14  ] {};
void()    skullw_death14    =[    $death15,    skullw_death14] {
    // Random timer for robes to fade away
    self.think = skullw_fadeaway;
    self.nextthink = time + 8 + random() * 8;
};

//----------------------------------------------------------------------
void() skullw_die =
{
    // Play fade away/death sound
    sound (self, CHAN_VOICE, "skullwiz/death.wav", 1, ATTN_NORM);

    // Pre-check routine to tidy up extra entities
    monster_death_precheck();
    skullw_remove_attachment();

    // Special death, Instantly vaporize cloak
    // This is really a gib death without the gibs!
    if (self.health < -40) {
        // Typical Grenade explosion
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_EXPLOSION);
        WriteCoord (MSG_BROADCAST, self.origin_x);
        WriteCoord (MSG_BROADCAST, self.origin_y);
        WriteCoord (MSG_BROADCAST, self.origin_z);
        SpawnExplosion(EXPLODE_MED, self.origin, SOUND_REXP3);

        self.health = MON_NOGIBVELOCITY;    // Low gib directional modifier
        self.gib2skin = self.exactskin;        // Update book skin
        ThrowGib(11,1); ThrowGib(12,1);        // Throw head and book
        entity_hide(self);
    }
    // Regular death, collapse and fade away
    else {
        // Large particle explosion upwards
        particle_explode(self.origin, 100, 3, self.part_style, PARTICLE_BURST_SKULLUP);
        skullw_death1();
    }
};

/*======================================================================
QUAKED monster_skullwiz (0.5 0 1) (-16 -16 -24) (16 16 40)
======================================================================*/
void() monster_skullwiz =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_skullwiz.mdl";
    precache_model (self.mdl);
    precache_model (MODEL_PROJ_SWSKULL);        // explosive flaming skull
    precache_model (MODEL_PROJ_SWSKULLP);        // explosive poison skull
    precache_model (MODEL_PROJ_SWBALL);            // Purple minion ball
    precache_model (SBURST_POISON);                // Poison explosion

    self.gib1mdl = "progs/h_skullwiz.mdl";        // Skull head
    self.gib2mdl = "progs/w_skullbook.mdl";        // Unique book item
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);

    self.idle_sound = "skullwiz/idle1.wav";
    self.idle_sound2 = "skullwiz/idle2.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);

    precache_sound ("skullwiz/death.wav");
    self.pain_sound = "skullwiz/pain1.wav";
    self.pain_sound2 = "skullwiz/pain2.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.pain_sound2);

    precache_sound ("skullwiz/attack.wav");        // fire spell
    precache_sound ("skullwiz/skull1.wav");        // Screaming skull attack
    precache_sound ("skullwiz/skull2.wav");
    precache_sound ("skullwiz/summon.wav");
    precache_sound ("skullwiz/poison_hit.wav");    // Guardian Explosion

    precache_sound ("skullwiz/blinkspk1.wav");    // Talk casting blink spell
    precache_sound ("skullwiz/blinkspk2.wav");
    precache_sound ("skullwiz/blinkout.wav");    // Sound of blink animation
    precache_sound ("skullwiz/blinkin.wav");

    self.sight_sound = "skullwiz/sight.wav";
    precache_sound (self.sight_sound);

    self.solid = SOLID_NOT;            // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    if (self.health < 1) self.health = 120;
    self.gibhealth = MON_NEVERGIB;    // Cannot be gibbed by weapons
    self.gibbed = FALSE;            // Still in one piece
    self.pain_flinch = 50;            // Armour strength robes!
    self.steptype = FS_TYPEMEDIUM;    // Humanoid footsteps
    self.pain_longanim = FALSE;        // No long pain animation
    self.poisonous = FALSE;            // Only Guardians can be poisonous
    self.deathstring = " was blown apart by a Skull Wizard\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    // Skull wizards have special animation movement sets
    // Need extra parameters for their teleporting ability
    // Can be setup to start phased out and surprise the player
    self.idlebusy = SKULLW_STAND;
    if (self.height == 0) self.height = SKULLW_HEIGHT;
    if (self.distmin == 0) self.distmin = SKULLW_DISTMIN;
    if (self.distmax == 0) self.distmax = SKULLW_DISTMAX;
    if (!self.bodyphased) self.bodyphased = MONAI_SKULLWSOLID;
    else self.bodyphased = MONAI_SKULLWINVIS;

    self.th_checkattack = SkullWizCheckAttack;
    self.th_updmissile = skullw_guardian;
    self.th_pain = skullw_pain;
    self.th_die = skullw_die;

    // Does the skull wizard start phased out?
    // Need to intercept idle/combat functions
    if (self.bodyphased == MONAI_SKULLWINVIS) {
        self.th_stand = skullw_phased;
        self.th_walk = skullw_phased;
        self.th_run = skullw_wakeup;
        self.th_missile = skullw_wakeup;
        self.th_melee = skullw_wakeup;
    }
    else {
        self.th_stand = skullw_stand;
        self.th_walk = skullw_walk;
        self.th_run = skullw_run1;
        self.th_missile = skullw_magic;
        self.th_melee = skullw_tele1;
    }
    
    self.classtype = CT_MONSKULLW;
    self.classgroup = CG_WIZARD;
    self.classmove = MON_MOVEWALK;

    // Special black robes for skull spawner
    if (self.spawnflags & MON_SKULLWIZ_MINIONS) {
        self.health = self.health * 1.5;            // 150% HP
        if (!self.exactskin) self.exactskin = 2;    // Black robes
    }
    // Special green robes for rune guardians
    else if (self.spawnflags & MON_SKULLWIZ_GUARDIAN) {
        self.health = self.health * 2;                // 200% HP
        if (!self.exactskin) self.exactskin = 3;    // Green robes
        self.poisonous = TRUE;                        // Explosive Poison
    }
    
    // Make sure the particles match the skin
    if (self.exactskin == 1) self.part_style = PARTICLE_BURST_GREEN;
    else if (self.exactskin == 2) self.part_style = PARTICLE_BURST_PURPLE;
    else if (self.exactskin == 3) self.part_style = PARTICLE_BURST_GREEN;
    else if (self.exactskin == 4) self.part_style = PARTICLE_BURST_WHITE;
    else self.part_style = PARTICLE_BURST_RED;
            
    monster_start();
};

/*======================================================================
/*QUAKED info_skullwiz_destination (0.6 0 0.8) (-16 -16 -24) (16 16 40) x
Teleport destination for skull wizard ONLY
-------- KEYS --------
targetname : part of a chain group (required)
target   : next destination in the chain
distance : Minimum distance enemy needs to be before spawning
-------- SPAWNFLAGS --------
-------- NOTES --------
Teleport destination for skull wizard ONLY

======================================================================*/
void() info_skullwiz_link =
{
    local entity currlink, master;
    local float loopcondition;
    
    // Teleport destination already been linked
    if (self.enemy) return;
    
    // The master entity is the start of the chain
    // the currlink is a link in the chain
    master = self;
    loopcondition = TRUE;
    
    while (loopcondition) 
    {
        // Setup next link to master and find next link
        self.enemy = master;
        currlink = find(world, targetname, self.target);
        // reached end of list?
        if (!currlink || currlink == master) 
            loopcondition = FALSE;
        else {
            // Move forward in the chain
            self.enemy = currlink;
            self = currlink;
        }
    }
};

//----------------------------------------------------------------------
void() info_skullwiz_destination =
{
    // All teleportation points need to be part of a chain
    // Otherwise the skull Wizard would have to use findradius
    // which is expensive if lots of entities are around
    if (self.targetname == "" || self.target == "") {
        dprint("\b[SKULLWIZ_DEST]\b missing target or targetname\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        remove(self);
        return;
    }
    
    self.classtype = CT_SKULLTELEPORT;
    self.mangle = self.angles;
    // Setup minimum player spawn distance
    if (self.distance <=0) self.distance = MONAI_MELEESKULLW;
    // Setup if the destination is active or not
    self.state = FALSE;

    // Check for targetname->target loops
    if (self.targetname == self.target) {
        self.enemy = self;
        self.owner = self;
    }
    else {
        // Wait for all targets to spawn
        self.think = info_skullwiz_link;
        self.nextthink = time + 0.2 + random();
    }
};

