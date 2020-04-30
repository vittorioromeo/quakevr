/*==============================================================================
WRAITH from Rogue Expansion Pack
==============================================================================*/
$skin wrthskin

// Base animation pose
$frame basepose

// Walk/Idle pose (strange lerp at the end of frame set)
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8
$frame stand9 stand10 stand11 stand12

// Magic Attack A - Swing arms forward with hand meeting together
$frame magicA1 magicA2 magicA3 magicA4 magicA5 magicA6 magicA7 magicA8
$frame magicA9 magicA10 magicA11 magicA12 magicA13 magicA14

// Magic Attack B - Cup hands around a missile object infront and fire
$frame magicB1 magicB2 magicB3 magicB4 magicB5 magicB6 magicB7 magicB8
$frame magicB9 magicB10 magicB11 magicB12 magicB13

// Magic Attack C - Swing right arm forward firing a projectile
$frame magicC1 magicC2 magicC3 magicC4 magicC5 magicC6 magicC7 magicC8
$frame magicC9 magicC10 magicC11 magicC12 magicC13 magicC14 magicC15

// Pain A - Short flinch backwards
$frame painA1 painA2 painA3 painA4 painA5 painA6

// Pain B - Long motion of being hit several times
$frame painB1 painB2 painB3 painB4 painB5 painB6 painB7 painB8
$frame painB9 painB10 painB11

// Death - Shrink down and explode
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12 death13 death14 death15

float BONEOFFSET = 8;
float WRAITH_ALPHA = 0.7;
float WRAITH_ALPHAFIRE = 1;
float BALL_NAIL_COL1 = 0;        // Red
float BALL_NAIL_COL2 = 1;        // Sickly yellow
float BALL_SPIDER_COL1 = 1;        // Sickly yellow
float BALL_DEBUFF_COL1 = 3;        // Glowing yellow/red
float BALL_DEBUFF_COL2 = 3;        // Glowing yellow/red
float FADEDEBUFF_TIME = 0.01;    // Small amount of time
float WRAITH_SKINBLUE = 0;        // Fire wraith
float WRAITH_SKINGREEN = 2;        // Spider version
float WRAITH_SKINPURPLE = 4;    // Scorpion version

//======================================================================
void() wraith_stand1 =[ $stand4, wraith_stand2 ] { monster_idle_sound();ai_stand();};
void() wraith_stand2 =[ $stand5, wraith_stand3 ] { ai_stand();};
void() wraith_stand3 =[ $stand6, wraith_stand4 ] { ai_stand();};
void() wraith_stand4 =[ $stand7, wraith_stand5 ] { ai_stand();};
void() wraith_stand5 =[ $stand8, wraith_stand6 ] { ai_stand();};
void() wraith_stand6 =[ $stand9, wraith_stand7 ] { ai_stand();};
void() wraith_stand7 =[ $stand9, wraith_stand8 ] { ai_stand();};
void() wraith_stand8 =[ $stand8, wraith_stand9 ] { ai_stand();};
void() wraith_stand9 =[ $stand7, wraith_stand10 ] { ai_stand();};
void() wraith_stand10 =[ $stand6, wraith_stand11 ] { ai_stand();};
void() wraith_stand11 =[ $stand5, wraith_stand12 ] { ai_stand();};
void() wraith_stand12 =[ $stand4, wraith_stand1 ] { ai_stand();};

//======================================================================
void() wraith_walk1 =[ $stand3, wraith_walk2 ] { monster_idle_sound();ai_walk(8);};
void() wraith_walk2 =[ $stand4, wraith_walk3 ] { ai_walk(7);};
void() wraith_walk3 =[ $stand5, wraith_walk4 ] { ai_walk(7);};
void() wraith_walk4 =[ $stand6, wraith_walk5 ] { ai_walk(6);};
void() wraith_walk5 =[ $stand7, wraith_walk6 ] { ai_walk(6);};
void() wraith_walk6 =[ $stand8, wraith_walk7 ] { ai_walk(5);};
void() wraith_walk7 =[ $stand9, wraith_walk8 ] { ai_walk(5);};
void() wraith_walk8 =[ $stand10, wraith_walk9 ] { ai_walk(4);};
void() wraith_walk9 =[ $stand11, wraith_walk10 ] { ai_walk(8);};
void() wraith_walk10 =[ $stand12, wraith_walk1 ] { ai_walk(8);};

//======================================================================
void() wraith_run1 =[ $stand3, wraith_run2 ] {monster_idle_sound();
    if (self.meleeattack < time) {
        self.meleeattack = time + 4 + (random() + random() + random() * 4);
        self.lefty = rint(1 - self.lefty);
    }
    ai_run(8);
};
void() wraith_run2 =[ $stand4, wraith_run3 ] {ai_run(7);};
void() wraith_run3 =[ $stand5, wraith_run3 ] {ai_run(7);};
void() wraith_run4 =[ $stand6, wraith_run3 ] {ai_run(6);};
void() wraith_run5 =[ $stand7, wraith_run3 ] {ai_run(6);};
void() wraith_run6 =[ $stand8, wraith_run3 ] {ai_run(5);};
void() wraith_run7 =[ $stand9, wraith_run3 ] {ai_run(5);};
void() wraith_run8 =[ $stand10, wraith_run3 ] {ai_run(4);};
void() wraith_run9 =[ $stand11, wraith_run3 ] {ai_run(8);};
void() wraith_run10 =[ $stand12, wraith_run3 ] {ai_run(8);};

//============================================================================
// Attachment management (create, finish and delete)
//============================================================================
void() wraith_create_attachment =
{
    // Are the attachments setup yet?
    if (!self.attachment) {
        self.attachment = spawn();
        self.attachment.owner = self;
        self.attachment.classtype = CT_ATTACHMENT;
        self.attachment.mdl = MODEL_PROJ_WBALL;
        self.attachment.alpha = 0.85;
    }
    if (!self.attachment2) {
        self.attachment2 = spawn();
        self.attachment2.owner = self;
        self.attachment2.classtype = CT_ATTACHMENT;
        self.attachment2.mdl = MODEL_PROJ_WBALL;
        self.attachment2.alpha = 0.85;
    }
    if (!self.attachment3) {
        self.attachment3 = spawn();
        self.attachment3.owner = self;
        self.attachment3.classtype = CT_ATTACHMENT;
        self.attachment3.mdl = self.weaponglow;
        self.attachment3.alpha = 0.5;
    }
};

//----------------------------------------------------------------------
void() wraith_finish_attachment =
{
    if (self.attachment) {
        setmodel(self.attachment, "");
        self.attachment.state = STATE_OFF;
    }
    if (self.attachment2) {
        setmodel(self.attachment2, "");
        self.attachment2.state = STATE_OFF;
    }
    if (self.attachment3) {
        setmodel(self.attachment3, "");
        self.attachment3.state = STATE_OFF;
        self.attachment3.velocity = '0 0 0';
    }
};

//----------------------------------------------------------------------
void() wraith_remove_attachment =
{
    if (self.attachment) {
        self.attachment.think = SUB_Remove;
        self.attachment.nextthink = time + 0.1;
    }
    if (self.attachment2) {
        self.attachment2.think = SUB_Remove;
        self.attachment2.nextthink = time + 0.1;
    }
    if (self.attachment3) {
        self.attachment3.think = SUB_Remove;
        self.attachment3.nextthink = time + 0.1;
    }
};

//======================================================================
// FADE OUT TEMPORARY MODELS
// Very quick fade (0.5s) - modified version of model_fade
//======================================================================
void() debuff_fade =
{
    self.alpha = 0.5 - ((time - self.ltime) / 0.5);

    if (self.alpha < 0.2) {
        self.alpha = 0;
        self.model = "";
        self.think = SUB_Remove;
        self.nextthink = time + 0.1;
        return;
    }
    self.nextthink = time + FADEDEBUFF_TIME;
};

//----------------------------------------------------------------------
// Start fading out (very quick, over 0.5s)
//----------------------------------------------------------------------
void() wraith_debuff_chunk =
{
    self.think = debuff_fade;
    self.nextthink = time + FADEDEBUFF_TIME;
    self.ltime = self.nextthink;
};

//----------------------------------------------------------------------
// Manual animation of ball at end of the debuff particle trail
//----------------------------------------------------------------------
void() wraith_debuff_shrink =
{
    // Count downwards getting smaller
    self.lip = self.frame - 1;
    if (self.lip < 0) wraith_debuff_chunk();
    else {
        // Check frame number before assigning
        // Cannot set models with negative frame numbers
        self.frame = self.lip;
        self.think = wraith_debuff_shrink;
        self.nextthink = time + 0.1;
    }
};

//======================================================================
// RANGE ATTACK 1 - Apply debuff to player (hitscan!)
//
// Generate an animated particle trail from 32 unit segments
// Slowly remove segments from source to target
// Generate a ball at target location slowly shrinking
//======================================================================
void() wraith_fire_touch =
{
    if (other.solid == SOLID_TRIGGER) return;
    if (other.classtype == CT_TEMPSTREAM) return;
    if (other.health > 0) return;
    if (self.attack_finished > time) return;

    self.attack_finished = time + 1;
    self.model = "";
    self.think = SUB_Remove;
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void() wraith_fire_think =
{
    // Randomly drift upwards and slow down forward movement
    self.velocity_z = self.velocity_z + 10 + (random() * 20);
    self.velocity = self.velocity - (self.velocity * 0.125);
    
    self.frame = self.frame + 1;
    if (self.frame > self.count) SUB_Remove();
    else self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void(vector start_org) wraith_debuff_explode =
{
    local float fire_loop;
    local vector fire_angles, org, dir;
    
    fire_loop = 0;
    fire_angles = '0 0 0';
    
    while (fire_loop < 360) {
        newmis = spawn();
        newmis.classtype = CT_TEMPSTREAM;
        newmis.movetype = MOVETYPE_FLY;        // Fly, no gravity
        newmis.solid = SOLID_TRIGGER;
        setmodel(newmis, SBURST_FLAME);

        org = start_org;
        org_z = org_z + (crandom() * 24);
        setorigin(newmis, org);
        setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);

        fire_angles_y = fire_loop;
        makevectors(fire_angles);
        dir = vecrand(0,50,FALSE);
        dir = dir + (v_forward * (400 + random() * 50));
        newmis.velocity = dir;
        newmis.count = 6;
        newmis.frame = rint(random()*2);

        // If DP engine active remove particle shadow
        if (engine == ENG_DPEXT) newmis.effects = newmis.effects + EF_NOSHADOW;

        newmis.think = wraith_fire_think;
        newmis.touch = wraith_fire_touch;
        newmis.nextthink = time + 0.1;
        
        fire_loop = fire_loop + rint(random()*15);
    }
};

//----------------------------------------------------------------------
void() wraith_debuff_trail =
{
    local vector org, orgofs, dir, tvec, tang;
    local float tdist, tloop, tcount, tpc;
    local entity tent;

    // Starting point of beam matched to wraith animation
    makevectors (self.angles);
    orgofs = '52 4 6';

    // Calculate starting point and beam and vector direction
    org = self.origin + attack_vector(orgofs);
    tvec = SUB_orgEnemyTarget() - org;
    // Create facing angle for segments
    dir = normalize(tvec);
    tang = vectoangles(dir);
    
    // Check for solid wall very close
    tpc = pointcontents(org);
    if (tpc == CONTENT_SOLID || tpc == CONTENT_SKY) return;
    tpc = pointcontents(org+(dir*16));
    if (tpc == CONTENT_SOLID || tpc == CONTENT_SKY) return;
    
    // Check for obstacles (geo and monsters)
    traceline(org, SUB_orgEnemyTarget(), FALSE, self);
    tdist = trace_fraction * vlen(tvec);
    if (tdist < 32) return;
    tloop = tdist / 32;
    tcount = 0;

    //----------------------------------------------------------------------
    // Originally - BleedDeBuff(SUB_entEnemyTarget());
    // New version - Fire explosion + burning
    if (trace_ent.flags & FL_CLIENT) {
        self.owner = self;    // Fix for T_damage inside ApplyFireDmg
        ApplyFireDmg(SUB_entEnemyTarget(), DAMAGE_PYROWRAITH, DAMAGE_PYROWRAITH);
    }
    else if (ai_foundbreakable(self, trace_ent, FALSE))
        T_Damage (trace_ent, self, self, 100, DAMARMOR);
    
    //----------------------------------------------------------------------
    // Generate particle beam in segments
    while(tcount < tloop) {
        // Create segment in the middle of 32 segment
        tvec = org + (dir* (16 + tcount*32) );

        // Spawn temporary entity
        tent = spawn();
        tent.owner = self;
        tent.classtype = CT_PROJ_WBOLT;
        setsize (tent, VEC_ORIGIN, VEC_ORIGIN);
        tent.movetype = MOVETYPE_FLYMISSILE;    // Needs to float up
        tent.solid = SOLID_NOT;
        
        // Special animated particle model (still, slow rotate, fast rotate)
        setmodel(tent, MODEL_PROJ_WTRAIL);
        setorigin(tent, tvec);
        
        tent.velocity = -v_up*5;    // Slowly drift up to create trail particles
        tent.angles = tang;            // Rotate segment into place
        tent.frame = 2;                // Fast rotate animation group
        tent.skin = 3;                // Flame yellow/red
        tcount = tcount + 1;        // Next segment
        
        // Wait around for a while before fading out
        tent.think = wraith_debuff_chunk;
        tent.nextthink = time + (tcount *0.05);
    }

    // Workout final destination of explosion and effects
    self.finaldest = tvec+(dir *16);

    //----------------------------------------------------------------------
    // Spawn a ball of plasma at the final destionation
    // Shrinking downwards in size over 0.1s frames
    tent = spawn();
    tent.owner = self;
    tent.classtype = CT_PROJ_WBOLT;
    setsize (tent, VEC_ORIGIN, VEC_ORIGIN);
    tent.movetype = MOVETYPE_FLYMISSILE;// Needs to float upward
    tent.solid = SOLID_NOT;
    setmodel(tent, MODEL_PROJ_WBALL);    // Initial ball
    setorigin(tent, self.finaldest);    // Move ball to end of final segment
    tent.velocity = -v_up*5;            // Slowly drift upwards
    tent.avelocity = '100 100 100';        // Slight rotation on all axis
    tent.frame = 6;                        // Start really large
    tent.skin = BALL_DEBUFF_COL2;
    tent.think = wraith_debuff_shrink;    // Animate manually
    tent.nextthink = time + 0.1;

    //----------------------------------------------------------------------
    // Generate particle explosion (+ sound effect)
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION);    // hard coded explosion
    WriteCoord (MSG_BROADCAST, self.finaldest_x);
    WriteCoord (MSG_BROADCAST, self.finaldest_y);
    WriteCoord (MSG_BROADCAST, self.finaldest_z);
    // Play original explosion sound
    sound(tent, CHAN_WEAPON, "drole/r_explode.wav", 1, ATTN_NORM);

    //----------------------------------------------------------------------
    // Large circular flame explosion
    wraith_debuff_explode(self.finaldest);
};

//----------------------------------------------------------------------
void(entity handent, vector orgofs, vector orgside, float dbframe) wraith_debuffball =
{
    local vector org;
    
    // Frame 0 is start of the sequence (move everything into place)
    if (dbframe == 0) {
        handent.state = STATE_ON;
        setorigin(handent, self.origin);
        setmodel(handent, handent.mdl);
        setsize (handent, VEC_ORIGIN, VEC_ORIGIN);
        handent.movetype = MOVETYPE_NONE;
        handent.solid = SOLID_NOT;
        handent.skin = BALL_DEBUFF_COL1;
    }
    
    // Generate attachment in hand (left)
    org = self.origin + v_forward*orgofs_x + orgside*orgofs_y + v_up*orgofs_z;
    setorigin(handent, org);
    handent.angles_y = rint(random()*359);
    handent.frame = dbframe;
};

//----------------------------------------------------------------------
void(vector orgofs, float dbframe) wraith_debuff =
{
    if (self.health < 1) return;
    
    if (dbframe == 0) {
        self.pain_finished = time + 1.1;
        sound (self, CHAN_WEAPON, "wraith/attack1.wav", 1, ATTN_NORM);

        // Setup halo/glow models around wraith (5% larger)
        setorigin(self.attachment3, self.origin);
        setmodel(self.attachment3, self.attachment3.mdl);
        setsize (self.attachment3, VEC_ORIGIN, VEC_ORIGIN);
        self.attachment3.movetype = MOVETYPE_NONE;
        self.attachment3.solid = SOLID_NOT;
    }
    
    // Should be visible during attack cycle
    self.skin = self.exactskin + 1;
    self.alpha = WRAITH_ALPHAFIRE;
    // Particles drifting up slowly from body
    particle_explode(self.origin, 20, 2, self.part_style, PARTICLE_BURST_UPWARD);
    // Update halo/glow model
    self.attachment3.angles = self.angles;
    self.attachment3.frame = self.frame;
    self.attachment3.velocity = self.velocity;
    // Randomly change alpha to make glow flicker
    self.attachment3.alpha = 0.1 + random()*0.5;


    // Keep turning towards enemy
    ai_face();
    makevectors(self.angles);
    
    // Generate a ball in each hand
    wraith_debuffball(self.attachment, orgofs, v_right, dbframe);
    wraith_debuffball(self.attachment2, orgofs, -v_right, dbframe);
};

//----------------------------------------------------------------------
// RANGE ATTACK 1 - Swing arms forward with hand meeting together
//
// The debuff attack is a hitscan attack but it does have 1s animation
// (backward frames) 30 32 8, 14 46 20, -6 46 28, -12 40 28
// (forward frames)   0 46 30, 16 42 24, 32 38 20, 48 26 10, 52 14 6, 52 4 6
void() wraith_debuff1 =[ $magicA1, wraith_magicA2 ] {ai_face();};
void() wraith_magicA2 =[ $magicA2, wraith_magicA3 ] {ai_face();};
void() wraith_magicA3 =[ $magicA3, wraith_magicA4 ] {wraith_debuff('30 32 8',0);};
void() wraith_magicA4 =[ $magicA4, wraith_magicA5 ] {wraith_debuff('14 46 20',1);};
void() wraith_magicA5 =[ $magicA5, wraith_magicA6 ] {wraith_debuff('-6 46 28',2);};
void() wraith_magicA6 =[ $magicA6, wraith_magicA7 ] {wraith_debuff('-12 40 28',3);};
void() wraith_magicA7 =[ $magicA7, wraith_magicA8 ] {wraith_debuff('0 46 30',3);};
void() wraith_magicA8 =[ $magicA8, wraith_magicA9 ] {wraith_debuff('16 42 24',4);};
void() wraith_magicA9 =[ $magicA9, wraith_magicA10 ] {wraith_debuff('32 38 20',4);};
void() wraith_magicA10 =[ $magicA10, wraith_magicA11 ] {wraith_debuff('48 26 10',5);};
void() wraith_magicA11 =[ $magicA11, wraith_magicA12 ] {wraith_debuff('52 14 6',5);};
void() wraith_magicA12 =[ $magicA12, wraith_magicA13 ] {
    wraith_debuff('52 4 6',6);
    wraith_debuff_trail(); 
    self.skin = self.exactskin; 
    self.alpha = WRAITH_ALPHA;
};
void() wraith_magicA13 =[ $magicA13, wraith_magicA14 ] {wraith_finish_attachment();};
void() wraith_magicA14 =[ $magicA14, wraith_run1 ] {};

//======================================================================
// RANGE ATTACK 2 - Small bone/nail attack (low damage)
//
// Create a ball of blood and throw it towards the player
// Turning the ball into a pile of bones/nails
//======================================================================
void(vector orgofs, float nbframe) wraith_nailball =
{
    local vector org;
    if (self.health < 1) return;

    // Frame 0 is start of the sequence (move everything into place)
    if (nbframe == 0) {
        self.attachment.state = STATE_ON;
        setorigin(self.attachment, self.origin);
        setmodel(self.attachment, self.attachment.mdl);
        setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);
        self.attachment.movetype = MOVETYPE_NONE;
        self.attachment.solid = SOLID_NOT;
        // Setup regular/poison projectile
        if (self.spawnflags & MON_WRAITH_SPIDERS) self.attachment.skin = BALL_NAIL_COL2;
        else self.attachment.skin = BALL_NAIL_COL1;

        self.pain_finished = time + 1;
        sound (self, CHAN_WEAPON, "wraith/attack2.wav", 1, ATTN_NORM);
    }
    
    // Turn towards enemy and make vector
    ai_face();
    makevectors(self.angles);
    
    // Generate attachment in hand (left)
    org = self.origin + attack_vector(orgofs);
    setorigin(self.attachment, org);
    self.attachment.angles_y = rint(random()*359);
    self.attachment.frame = nbframe;
};

//----------------------------------------------------------------------
void(vector boneofs) wraith_firenails =
{
    local vector org, dir;
    local float bone_type;
    if (self.health < 1) return;
    
    if (self.spawnflags & MON_WRAITH_SPIDERS) bone_type = CT_PROJ_WPOISON;
    else bone_type = CT_PROJ_WBONE;
    self.attack_speed = SPEED_WRAITHBONE + (skill * SPEED_WBONESKILL);
    makevectors(self.angles);

    // Generate spike/bone at final animation position
    org = self.origin + attack_vector(boneofs);
    dir = SUB_orgEnemyTarget() + v_right*(BONEOFFSET*3);
    dir = normalize(dir - org);
    launch_projectile (org, dir, bone_type, self.attack_speed+random()*25);
    dir = SUB_orgEnemyTarget() - v_right*(BONEOFFSET*3);
    dir = normalize(dir - org);
    launch_projectile (org, dir, bone_type, self.attack_speed+random()*25);
    dir = SUB_orgEnemyTarget() + v_up*BONEOFFSET;
    dir = normalize(dir - org);
    launch_projectile (org, dir, bone_type, self.attack_speed+random()*25);
    dir = SUB_orgEnemyTarget() - v_up*BONEOFFSET;
    dir = normalize(dir - org);
    launch_projectile (org, dir, bone_type, self.attack_speed+random()*25);
};

//----------------------------------------------------------------------
// RANGE ATTACK 2 - Small bone/nail attack (low damage)
//
// 32 22 0, 29 29 6, 15 33 16, 4 28 24, -6 20 24, -10 16 20, 6 16 14, 
// 14 16 10, 54 5 6, 60 4 14
void() wraith_nails1 =[ $magicC1, wraith_magicC2 ] {ai_face();
    SUB_AttackFinished(1 + random());    // Fire more frequent
};
void() wraith_magicC2 =[ $magicC2, wraith_magicC3 ] {ai_face();};
void() wraith_magicC3 =[ $magicC3, wraith_magicC4 ] {wraith_nailball('15 33 16',0);};
void() wraith_magicC4 =[ $magicC4, wraith_magicC5 ] {wraith_nailball('4 28 24',1);};
void() wraith_magicC5 =[ $magicC5, wraith_magicC6 ] {wraith_nailball('-6 20 24',2);};
void() wraith_magicC6 =[ $magicC6, wraith_magicC7 ] {wraith_nailball('-10 16 20',3);};
void() wraith_magicC7 =[ $magicC7, wraith_magicC8 ] {wraith_nailball('6 16 14',4);};
void() wraith_magicC8 =[ $magicC8, wraith_magicC9 ] {wraith_nailball('14 16 10',5);};
void() wraith_magicC9 =[ $magicC9, wraith_magicC10 ] {wraith_nailball('54 5 6',6);};
void() wraith_magicC10 =[ $magicC10, wraith_magicC11 ] {
    wraith_finish_attachment();
    wraith_firenails('60 4 14');
};
void() wraith_magicC11 =[ $magicC11, wraith_magicC12 ] {monster_idle_sound();};
void() wraith_magicC12 =[ $magicC12, wraith_magicC13 ] {};
void() wraith_magicC13 =[ $magicC13, wraith_magicC14 ] {};
void() wraith_magicC14 =[ $magicC14, wraith_magicC15 ] {ai_face();};
void() wraith_magicC15 =[ $magicC15, wraith_run1 ] {ai_face();};

//======================================================================
// RANGE ATTACK 3 - spawn spider minion (optional)
//
// Create a ball of green goo and throw it towards the player
// Once ball has landed, hatch/explode and create spider (brown/green)
//======================================================================
void(vector eggofs, float eggframe) Setup_WraithEgg =
{
    local vector org;
    if (self.health < 1) return;
    
    // Frame 0 is start of the sequence (move everything into place)
    if (eggframe == 0) {
        self.attachment.state = STATE_ON;
        setorigin(self.attachment, self.origin);
        setmodel(self.attachment, self.attachment.mdl);
        setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);
        self.attachment.movetype = MOVETYPE_NONE;
        self.attachment.solid = SOLID_NOT;
        self.attachment.skin = BALL_SPIDER_COL1;

        // Present wraith from going into pain during egg cycle
        self.pain_finished = time + 1;
        sound (self, CHAN_WEAPON, "wraith/attack3.wav", 1, ATTN_NORM);
        // Should be visible during spawn minion cycle
        self.skin = self.exactskin + 1;
        self.alpha = WRAITH_ALPHAFIRE;
    }

    // Turn towards enemy and update model attachment
    // The offset is based on the wraith facing forward
    ai_face();
    makevectors(self.angles);
    org = self.origin + attack_vector(eggofs);
    setorigin(self.attachment, org);
    self.attachment.angles = self.angles;
    self.attachment.frame = eggframe;
};

//======================================================================
// RANGE ATTACK 3 - Cup hands around a spider egg infront and fire
//
// Spawn a yellow/green egg ball shape and then drop it towards the player
// The egg is spawned in the middle and moved up/down to generate particles
// 20 2 12, 18 2 12
void() wraith_minion1 =[ $magicB1, wraith_magicB2 ] {ai_face();};
void() wraith_magicB2 =[ $magicB2, wraith_magicB3 ] {Setup_WraithEgg('20 2 12',0);};
void() wraith_magicB3 =[ $magicB3, wraith_magicB4 ] {Setup_WraithEgg('18 2 12',1);};
void() wraith_magicB4 =[ $magicB4, wraith_magicB5 ] {Setup_WraithEgg('20 2 12',2);};
void() wraith_magicB5 =[ $magicB5, wraith_magicB6 ] {Setup_WraithEgg('18 2 12',3);};
void() wraith_magicB6 =[ $magicB6, wraith_magicB7 ] {Setup_WraithEgg('20 2 12',4);};
void() wraith_magicB7 =[ $magicB7, wraith_magicB8 ] {Setup_WraithEgg('18 2 12',5);};
void() wraith_magicB8 =[ $magicB8, wraith_magicB9 ] {Setup_WraithEgg('20 2 12',6);};
void() wraith_magicB9 =[ $magicB9, wraith_magicB10 ] {
    self.skin = self.exactskin;    // Not glowing anymore
    self.alpha = WRAITH_ALPHA;    // Fade out
    wraith_finish_attachment();
    Create_Egg('20 2 12');
    // Particles drifting up slowly from body
    particle_explode(self.origin, 30, 2, self.part_style, PARTICLE_BURST_UPWARD);

};
void() wraith_magicB10 =[ $magicB10, wraith_magicB11 ] {};
void() wraith_magicB11 =[ $magicB11, wraith_magicB12 ] {};
void() wraith_magicB12 =[ $magicB12, wraith_magicB13 ] {ai_face();};
void() wraith_magicB13 =[ $magicB13, wraith_run1 ] {ai_face();};

//----------------------------------------------------------------------
void() wraith_magic =
{
    local entity truetarg, miniondef;
    
    // Make sure the attachments are setup ready
    wraith_create_attachment();
    truetarg = SUB_entEnemyTarget();

    if (self.spawnflags & MON_WRAITH_SPIDERS && !self.minion_active) {
        // Check for minion template first
        miniondef = find(world,classname,"monster_wraithminion");
        if (miniondef.classtype == CT_CACHESPIDER) setup_minionsupport();
        else {
            // If template no available, warn and remove feature
            dprint("\b[WRAITH]\b Cannot find spider template!\n");
            self.spawnflags = self.spawnflags - MON_WRAITH_SPIDERS;
        }
    }
    if (self.spawnflags & MON_WRAITH_SCORPIONS && !self.minion_active) {
        // Check for minion template first
        miniondef = find(world,classname,"monster_wraithminion2");
        if (miniondef.classtype == CT_CACHESCORPION) setup_minionsupport();
        else {
            // If template no available, warn and remove feature
            dprint("\b[WRAITH]\b Cannot find scorpion template!\n");
            self.spawnflags = self.spawnflags - MON_WRAITH_SCORPIONS;
        }
    }
    
    // Is the target the player or monster?
    if (truetarg.flags & FL_CLIENT) {
        if (self.spawnflags & (MON_WRAITH_SPIDERS | MON_WRAITH_SCORPIONS)) {
            // Calculate a flat vector to ignore Z axis difference
            self.enemydist = range_distance(self.enemy, TRUE);
            // If the player is too close (melee distance) try old base attack
            if (self.minion_baseattack > 0 && self.enemydist < MON_RANGE_MELEE) wraith_nails1();
            else {
                // has the minion limit been reached?
                if (query_minionactive(self) == TRUE) wraith_minion1();
                else wraith_nails1();
            }
        }
        // First ever attack is the fire debuff
        else if (self.attack_speed < time) wraith_debuff1();
        // Does the player have the debuff already?
        else if (truetarg.burning) wraith_nails1();
        else {
            // Greater chance of debuff attack first
            if (random() < 0.7) wraith_debuff1();
            else wraith_nails1();
        }
        self.attack_speed = LARGE_TIMER;
    }
    // Always attack monsters with nails
    else wraith_nails1();
};

//============================================================================
// Pain A - Short flinch backwards
void() wraith_painA1 = [ $painA1, wraith_painA2 ] {};
void() wraith_painA2 = [ $painA2, wraith_painA3 ] {};
void() wraith_painA3 = [ $painA3, wraith_painA4 ] {};
void() wraith_painA4 = [ $painA4, wraith_painA5 ] {};
void() wraith_painA5 = [ $painA5, wraith_painA6 ] {};
void() wraith_painA6 = [ $painA6, wraith_run1 ] {};

//----------------------------------------------------------------------
// Pain B - Long motion of being hit several times
void() wraith_painB1 = [ $painB1, wraith_painB2 ] {};
void() wraith_painB2 = [ $painB2, wraith_painB3 ] {};
void() wraith_painB3 = [ $painB3, wraith_painB4 ] {};
void() wraith_painB4 = [ $painB4, wraith_painB5 ] {};
void() wraith_painB5 = [ $painB5, wraith_painB6 ] {};
void() wraith_painB6 = [ $painB6, wraith_painB7 ] {};
void() wraith_painB7 = [ $painB7, wraith_painB8 ] {};
void() wraith_painB8 = [ $painB8, wraith_painB9 ] {};
void() wraith_painB9 = [ $painB9, wraith_painB10 ] {};
void() wraith_painB10 = [ $painB10, wraith_painB11 ] {};
void() wraith_painB11 = [ $painB11, wraith_run1 ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) wraith_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Make sure fire wraith skin/alpha is reset on pain/death
    self.skin = self.exactskin;
    self.alpha = WRAITH_ALPHA;
    
    // Hide any attachments
    wraith_finish_attachment();
    
    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1) {
            // Randomly pick which pain animation to play
            if (random() < 0.85) wraith_painA1 ();
            else {
                self.pain_finished = time + 2;
                wraith_painB1 ();
            }
        }
        // The new axe forces monsters into long pain animations
        else if (self.pain_check == 2) {
            self.axhitme = 0;
            self.pain_finished = time + 2;
            wraith_painB1 ();
        }
    }
};

//============================================================================
void() wraith_death2 =[ $death2, wraith_death3] {};
void() wraith_death3 =[ $death3, wraith_death4] {};
void() wraith_death4 =[ $death4, wraith_death5] {};
void() wraith_death5 =[ $death5, wraith_death7] {};
void() wraith_death7 =[ $death7, wraith_death9] {};
void() wraith_death9 =[ $death9, wraith_death11] {};
void() wraith_death11 =[ $death11, wraith_death13] {};
void() wraith_death13 =[ $death13, wraith_death15] {};
void() wraith_death15 =[ $death15, wraith_death15]
{
    // Throw gibs, but create more XY outward velocity
    self.max_health = MON_XYGIBVELOCITY;
    monster_ThrowGib();
    // Move the explosion up to the middle/top of the body
    // Use to be a regular explosion, switched to tarbaby instead
    self.origin = self.origin + '0 0 24';
    T_RadiusDamage (self, self, DAMAGE_WRAITH, world, IGNORECLASS);
    
    // Big purple explosion
    if (ext_dppart) 
        pointparticles(particleeffectnum(DPP_WRAITHEXPLODE), self.origin, '0 0 0', 1);
    else {
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_TAREXPLOSION);    // Hard coded explosion
        WriteCoord (MSG_BROADCAST, self.origin_x);
        WriteCoord (MSG_BROADCAST, self.origin_y);
        WriteCoord (MSG_BROADCAST, self.origin_z);
    }
    
    // Play original explosion sound or replacement
    sound(self, CHAN_WEAPON, SOUND_REXP3, 1, ATTN_NORM);
    // Stop thinking and hide entity (prevent wraith_death15 loop)
    self.nextthink = LARGE_TIMER;
    self.think = SUB_Null;
    entity_hide(self);
};

//----------------------------------------------------------------------
void() wraith_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();
    self.solid = SOLID_NOT;
    sound (self, CHAN_VOICE, "wraith/death1.wav", 1, ATTN_NORM);

    // Make sure fire wraith skin/alpha is reset on pain/death
    self.skin = self.exactskin;
    self.alpha = WRAITH_ALPHA;

    // Make sure any attachments are removed
    wraith_remove_attachment();

    // Make sure body stays floating up where it finally dies
    // and do not drop to the ground (reset by monster_death_use func)
    self.flags = self.flags | FL_FLY;
    self.movetype = MOVETYPE_FLY;
    wraith_death2();
};

/*======================================================================
/*QUAKED monster_wraith (1 0 0) (-16 -16 -24) (16 16 32) Ambush
======================================================================*/
void() monster_wraith =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_wraith.mdl";
    self.weaponglow = "progs/ad171/mon_wraith_glow.mdl";
    self.gib1mdl = "progs/ad171/gib_wraith1.mdl";    // Forearm
    self.gib2mdl = "progs/ad171/gib_wraith2.mdl";    // Hand/Claw
    self.gib3mdl = "progs/ad171/gib_wraith3.mdl";    // stump?
    
    precache_model (self.mdl);
    precache_model (self.weaponglow);        // Glowing halo effect
    precache_model (MODEL_PROJ_WTRAIL);        // Hitscan blood bolt
    precache_model (MODEL_PROJ_WBALL);        // ball growing 8 frames
    precache_model (MODEL_PROJ_WEGG);        // ball turned into egg
    precache_model (MODEL_PROJ_WSHELL);        // Gibs from Egg
    
    precache_model (MODEL_PROJ_WBONE);        // Bone nails (fire/scorpion)
    precache_model (MODEL_PROJ_WPOISON);    // Poison nails (spider)
    
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    
    self.idle_sound = "wraith/idle1.wav";
    self.idle_sound2 = "wraith/idle2.wav";
    self.idle_soundcom = "wraith/idle3.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);
    precache_sound (self.idle_soundcom);
    
    precache_sound ("wraith/death1.wav");
    self.pain_sound = "wraith/pain1.wav";
    precache_sound (self.pain_sound);

    precache_sound ("wraith/attack1.wav");    // HITSCAN bolt
    precache_sound ("drole/r_explode.wav");
    precache_model (SBURST_FLAME);            // Flames

    precache_sound ("wraith/attack2.wav");    // Nail/Bones
    precache_sound ("wraith/attack3.wav");    // Minions
    precache_sound ("wraith/bounce.wav");    // Egg bounce

    self.sight_sound = "wraith/sight1.wav";
    precache_sound (self.sight_sound);

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_WIDE;
    if (self.health < 1) self.health = 350;
    self.alpha = WRAITH_ALPHA;            // Semi transparent
    self.yaw_speed = 35;                // Can turn really fast
    self.gibhealth = MON_NEVERGIB;        // Cannot be gibbed by weapons
    self.gibbed = FALSE;                // not falling apart
    self.pain_flinch = 70;                // High pain threshold
    self.blockudeath = TRUE;            // no humanoid death sound
    if (self.height == 0) self.height = MONAI_ABOVEDIST; // Custom height
    self.frame = 1;                        // First frame is never used!
    
    // Hitscan = 52 4 6; Nails = 60 4 14; Egg = 20 2 12;
    self.attack_offset = '30 2 8';        // Half way point 

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.th_checkattack = WraithCheckAttack;
    self.th_stand = wraith_stand1;
    self.th_walk = wraith_walk1;
    self.th_run = wraith_run1;
    self.th_missile = wraith_magic;
    self.th_pain = wraith_pain;
    self.th_die = wraith_die;

    if (self.deathstring == "") self.deathstring = " was shattered by a Wraith\n";

    // Cannot be both scorpion and spider minion spawner, def=spider
    if (self.spawnflags & MON_WRAITH_SCORPIONS && self.spawnflags & MON_WRAITH_SPIDERS)
        self.spawnflags = self.spawnflags - MON_WRAITH_SCORPIONS;
    
    // Setup each wraith type with different skins/particles
    if (self.spawnflags & MON_WRAITH_SCORPIONS) {
        self.minion_maxcount = 3;    // default is usually 5!
        self.exactskin = WRAITH_SKINPURPLE;            // Purple/Blood skin
        self.poisonous = FALSE;                        // Must be off for scorpion wraith
        self.part_style = PARTICLE_BURST_PURPLE;    // Purple cloud
    }
    else if (self.spawnflags & MON_WRAITH_SPIDERS) {
        self.exactskin = WRAITH_SKINGREEN;                // Poison/Green skin
        if (self.poisonous < 0) self.poisonous = FALSE;    // Poison debuff
        else self.poisonous = TRUE;
        self.part_style = PARTICLE_BURST_GREEN;            // Poison cloud
    }
    else {
        self.exactskin = WRAITH_SKINBLUE;    // Blue/Red skin
        self.poisonous = FALSE;                // Must be off for fire wraith
        self.part_style = PARTICLE_BURST_FIRE;
    }
        
    self.classtype = CT_MONWRAITH;
    self.classgroup = CG_SPIDER;
    self.classmove = MON_MOVEFLY;
    
    monster_start();
};
