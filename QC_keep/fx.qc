void Touch_Missile();

/*======================================================================
 PROJECTILE Explosion and Blood functions
 
======================================================================*/
// Classic ID rocket/grenade explosion
// Not used anymore, left for map hackers!
void() s_explode1 = [0,    s_explode2] {};
void() s_explode2 = [1,    s_explode3] {};
void() s_explode3 = [2,    s_explode4] {};
void() s_explode4 = [3,    s_explode5] {};
void() s_explode5 = [4,    s_explode6] {};
void() s_explode6 = [5,    SUB_Remove] {};

void() s_explode_think =
{
    if (self.frame >= self.cnt)
        {SUB_Remove ();  return;}

    self.frame = self.frame + 1;
    self.nextthink = time + self.wait;
    if (self.frame == self.cnt)
        self.think = SUB_Remove;
};

//----------------------------------------------------------------------
// General purpose animated spite function
// Types: Small, medium, big, plasma, puffpuff!
//----------------------------------------------------------------------
void() SpawnExplosion_think =
{
    self.frame = self.frame + 1;
    if (self.frame > self.count) SUB_Remove();
    else self.nextthink = time + self.speed;
};

//----------------------------------------------------------------------
void(float sprite_type, vector org, string expl_sound) SpawnExplosion =
{
    local string spr_name;
    local float spr_count, spr_speed;
    
    spr_count = -1;
    if (sprite_type == EXPLODE_SMALL) {
        if (ext_dppart)
            pointparticles(particleeffectnum(DPP_TEEXPLODE), org, '0 0 0', 1);
        else {
            spr_name = SEXP_SMALL;
            spr_count = 5;
            spr_speed = 0.1;
        }
    }
    else if (sprite_type == EXPLODE_MED) {
        if (ext_dppart)
            pointparticles(particleeffectnum(DPP_TEEXPLODE), org, '0 0 0', 1);
        else {
            spr_name = SEXP_MED;
            spr_count = 13;
            spr_speed = 0.05;
        }
    }
    else if (sprite_type == EXPLODE_BIG) {
        if (ext_dppart)
            pointparticles(particleeffectnum(DPP_TEEXPLODE), org, '0 0 0', 1);
        else {
            spr_name = SEXP_BIG;
            spr_count = 16;
            spr_speed = 0.05;
        }
    }
    else if (sprite_type == EXPLODE_PLASMA_SMALL) {
        if (ext_dppart)
            pointparticles(particleeffectnum(DPP_TEPLASMA), org, '0 0 0', 1);
        else {
            spr_name = SEXP_PLASMA_SMALL;
            spr_count = 12;
            spr_speed = 0.05;
        }
    }
    else if (sprite_type == EXPLODE_PLASMA_MED || sprite_type == EXPLODE_PLASMA_BIG) {
        if (ext_dppart)
            pointparticles(particleeffectnum(DPP_TEPLASMABIG), org, '0 0 0', 1);
        else {
            spr_name = SEXP_PLASMA_BIG;
            spr_count = 12;
            spr_speed = 0.05;
        }
    }
    else if (sprite_type == EXPLODE_POISON_SMALL) {
        if (ext_dppart)
            pointparticles(particleeffectnum(DPP_TEPOISON), org, '0 0 0', 1);
        else {
            spr_name = SEXP_POISON_SMALL;
            spr_count = 12;
            spr_speed = 0.05;
        }
    }
    else if (sprite_type == EXPLODE_POISON_MED || sprite_type == EXPLODE_POISON_BIG) {
        if (ext_dppart)
            pointparticles(particleeffectnum(DPP_TEPOISONMED), org, '0 0 0', 1);
        else {
            spr_name = SEXP_POISON_MED;
            spr_count = 12;
            spr_speed = 0.05;
        }
    }
    else if (sprite_type == EXPLODE_ELECT_SMALL || sprite_type == EXPLODE_ELECT_MED
        || sprite_type == EXPLODE_ELECT_BIG) {
        if (ext_dppart)
            pointparticles(particleeffectnum(DPP_TEPLASMA), org, '0 0 0', 1);
        else {
            spr_name = SEXP_ELECTRIC;
            spr_count = 4;
            spr_speed = 0.1;
        }
    }
    // This must be pre-cached by entity using it!
    // This is not cached in worldspawn as its rarely used
    else if (sprite_type == EXPLODE_ICE_BIG) {
        if (ext_dppart)
            pointparticles(particleeffectnum(DPP_TEPLASMA), org, '0 0 0', 1);
        else {
            spr_name = SEXP_ICE_BIG;
            spr_count = 9;
            spr_speed = 0.1;
        }
    }
    else if (sprite_type == EXPLODE_BURST_SMOKE) {
        if (ext_dppart)
            pointparticles(particleeffectnum(DPP_TEBSMOKE), org, '0 0 0', 1);
        else {
            spr_name = SBURST_SMOKE;
            spr_count = 6;
            spr_speed = 0.05;
        }
    }
    else if (sprite_type == EXPLODE_BURST_FLAME) {
        if (ext_dppart)
            pointparticles(particleeffectnum(DPP_TEBFLAME), org, '0 0 0', 1);
        else {
            spr_name = SBURST_FLAME;
            spr_count = 6;
            spr_speed = 0.05;
        }
    }
    else if (sprite_type == EXPLODE_BURST_POISON) {
        if (ext_dppart)
            pointparticles(particleeffectnum(DPP_TEBPOISON), org, '0 0 0', 1);
        else {
            spr_name = SBURST_POISON;
            spr_count = 6;
            spr_speed = 0.05;
        }
    }
    else return;

    // Always spawn a temporary entity
    // Need one for sprite and/or explosion sound
    newmis = spawn();
    newmis.classgroup = CG_TEMPENT;
    newmis.movetype = MOVETYPE_NONE;
    newmis.solid = SOLID_NOT;
    setorigin(newmis, org);

    // Any sprite requirements? (Fitz engine)
    if (spr_count > 0) {
        setmodel(newmis, spr_name);    // Setup sprite
        setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
        newmis.alpha = 0.85;        // Slightly transparent
        newmis.effects = 32;        // Additive blending
        newmis.count = spr_count;    // Total frames
        newmis.speed = spr_speed;    // Frame speed

        newmis.think = SpawnExplosion_think;
        newmis.nextthink = time + newmis.speed;
    }
    else {
        // No sprite required but need entity for sound to play
        // Allow for sound to finish and just remove
        setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
        newmis.think = SUB_Remove;
        newmis.nextthink = time + 4;
    }

    // Play any explosion sounds on temporary entity
    if (expl_sound != "") {
        sound(newmis, CHAN_WEAPON, expl_sound, 1, ATTN_NORM);
    }
};

//----------------------------------------------------------------------
// Should not be used anymore, this is the old ID system
// Use SpawnExplosion instead, copes with DP effects better
//----------------------------------------------------------------------
void() BecomeExplosion =
{
    self.touch = SUB_Null;
    self.velocity = '0 0 0';
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel(self, "");
    SpawnExplosion(EXPLODE_SMALL, self.origin, "");
    self.nextthink = time + 0.6;
    self.think = SUB_Remove;
};

//----------------------------------------------------------------------
void(vector org, float velrnd, float upbase, float uprnd) SpawnProjectileSmoke =
{
    newmis = spawn();
    newmis.classgroup = CG_TEMPENT;
    newmis.movetype = MOVETYPE_TOSS;
    newmis.solid = SOLID_NOT;
    setmodel(newmis, MODEL_PROJ_SMOKE);
    setorigin(newmis, org);
    setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
    if (velrnd > 0) newmis.velocity = vecrand(0,velrnd,TRUE);
    else newmis.velocity = '0 0 0';
    newmis.velocity_z = upbase + random()*uprnd;
    newmis.nextthink = time + 1 + random()*3;
    newmis.think = SUB_Remove;
};

//----------------------------------------------------------------------
void(entity source, entity targ) ProjectileType =
{
    // Projectile types (Poison/Robot/Stone/Blood)
    if (source.poisonous) {
        setmodel (targ, MODEL_PROJ_FLESHP);
        targ.gibtype = GIBTYPE_POISON;
    }
    else if (source.classgroup == CG_ROBOT || source.classgroup == CG_STONE) {
        setmodel (targ, MODEL_PROJ_SMOKE);
        targ.gibtype = GIBTYPE_STONE;
    }
    else {
        setmodel (targ, MODEL_PROJ_FLESH);
        targ.gibtype = GIBTYPE_BLOOD;
    }
    // Finally add DP particle trails
    if (ext_dppart) DPP_blood_trail(targ);
};

//----------------------------------------------------------------------
// Mainly used to show resistance to an ammo type
// Also used by boils for their idle gibs
//----------------------------------------------------------------------
void(entity source, vector org, float velrnd, float upbase, float uprnd) SpawnProjectileMeat =
{
    newmis = spawn ();
    newmis.classtype = CT_TEMPGIB;
    newmis.classgroup = CG_TEMPENT;
    newmis.movetype = MOVETYPE_BOUNCE;
    newmis.solid = SOLID_NOT;
    // Projectile types (Poison/Robot/Stone/Blood)
    ProjectileType(source, newmis);
    setorigin (newmis, org);
    setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);        
    newmis.velocity = vecrand(0,velrnd,TRUE);
    newmis.velocity_z = upbase, random()*uprnd;
    newmis.avelocity = vecrand(100,velrnd,FALSE);
    newmis.nextthink = time + 1 + random()*3;
    newmis.think = SUB_Remove;
};

//----------------------------------------------------------------------
// SpawnBlood
//----------------------------------------------------------------------
void(entity targ, vector org, vector vel, float part_qty) SpawnBlood =
{
    local float loop_count, part_col;
    loop_count = 0;
    vel = vel * 0.1;
    part_qty = part_qty / 2;

    // Exception - breakables don't really bleed red blood
    if (targ.classgroup == CG_BREAKABLE) part_col = targ.bleedcolour;
    else {
        if (targ.poisonous) part_col = MON_BCOLOR_GREEN;
        else if (targ.classgroup == CG_ROBOT) part_col = MON_BCOLOR_YELLOW;
        else part_col = MON_BCOLOR_RED;
    }
    
    // Loop through particle count creating bursts of particles
    while(loop_count < 4) {
        if (loop_count == 2 && targ.bleedcolour > 0) part_col = targ.bleedcolour;
        particle (org, vel, part_col + rint(random()*7), part_qty);
        loop_count = loop_count + 1;
    }
};

//----------------------------------------------------------------------
// spawn_touchblood
// Triggered by Touch_Bullet, Touch_PlasmaProjectile, Touch_Projectile
// Used by monsters - DFURY, DOG, FISH, SCORPION, SPIDER, VORELING, ZOMBIEK
//----------------------------------------------------------------------
void(entity source, entity targ, float damage) spawn_touchblood =
{
    local vector org, vel;

    // The vel calculation uses v_up/right, make sure vectors is setup
    makevectors(source.angles);
    vel = normalize (source.velocity);
    vel = normalize(vel + v_up*(random()- 0.5) + v_right*(random()- 0.5));
    vel = vel + 2*trace_plane_normal;
    // Originally vel = ((vel * 200) * 0.2) * 0.01
    vel = vel * 0.4;
    
    // Check for an origin blood offset (monsters)
    if (CheckZeroVector(source.meleeoffset)) org = '0 0 0';
    else org = attack_vector(source.meleeoffset);
    
    SpawnBlood (targ, source.origin + org, vel, damage);
};

//======================================================================
// SpawnMeatSpray
// Changed to remove dependance on 'self' for missile origin
// Changed parameters to add source and destination of attack
// Changed velocity to side so it is calculated correctly from angles
//======================================================================
void(entity source, entity targ, float side) SpawnMeatSpray =
{
    local vector org;

    makevectors(source.angles);

    // Check for a melee offset? - Special vector offset
    if (CheckZeroVector(source.meleeoffset)) org = v_forward * 16;
    else org = attack_vector(source.meleeoffset);
    
    // Create starting point to spawn
    org = org + source.origin;

    if (targ.bleedcolour) SpawnBlood(targ, org, v_up*2, 100);
    else {
        newmis = spawn ();
        newmis.classtype = CT_TEMPGIB;
        newmis.classgroup = CG_TEMPENT;
        newmis.owner = source;
        newmis.movetype = MOVETYPE_BOUNCE;
        newmis.solid = SOLID_NOT;
        
        // Projectile types (Poison/Robot/Stone/Blood)
        ProjectileType(targ, newmis);
        setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);        
        setorigin (newmis, org);
    
        // Use side velocity to determine which direction to throw
        newmis.velocity = ((crandom()*16) * v_forward) + (side * v_right);
        newmis.velocity_z = newmis.velocity_z + 150 + 50*random();
        
        newmis.avelocity = vecrand(100,200,FALSE);
        // set newmis duration
        newmis.nextthink = time + 1 + random()*3;
        newmis.think = SUB_Remove;
    }
};

//================
// SpawnMeatSprayDrake - from drake mod's version
//================
void(vector org, vector vel, float btype) SpawnMeatSprayDrake =
{
    local   string  text;

    if (btype == BLOOD_RED)
        text = "progs/zom_gib.mdl";
    else if (btype == BLOOD_GREEN)
        {if (bloodbank & BB_GREEN) text = "progs/drake/gr_gib.mdl";}
    else if (btype == BLOOD_PURPLE)
        {if (bloodbank & BB_PURPLE) text = "progs/drake/pur_gib.mdl";}
    else if ((btype == BLOOD_FIRE) && (bloodbank & BB_FIRE))
        {text = "progs/drake/ember.mdl";}
    else if (btype != BLOOD_DARK)
        return;     // Unable to spew gore.

    local   entity  gib;

    gib = spawn ();
    gib.owner       = self;
    gib.movetype    = MOVETYPE_BOUNCE;
    gib.solid       = SOLID_NOT;

    gib.velocity    = vel;
    gib.velocity_z  = gib.velocity_z + 250 + 50*random();
    gib.avelocity   = '3000 1000 2000';

// set missile duration
    if (text)
    {
        gib.nextthink   = time + 1;
        gib.think       = SUB_Remove;
        setmodel (gib, text);
    }
    else
    {
        gib.bloodtype   = btype;
        gib.delay       = time + 1;
        gib.nextthink   = 0.01;
        gib.think       = MeatSprayThink;
        gib.modelindex  = 0;
    }
    setsize (gib, '0 0 0', '0 0 0');        
    setorigin (gib, org);
};


// ==============================================================================
// Drake effects:
// ==============================================================================
//==========================================================================
//  Temporary Entities

// PM:  These are common within the qc progs.

void(float fx, vector spot) Tent_Point =
{
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, fx);
    WriteCoord (MSG_BROADCAST, spot_x);
    WriteCoord (MSG_BROADCAST, spot_y);
    WriteCoord (MSG_BROADCAST, spot_z);
};

void(float fx, entity src, vector p1, vector p2) Tent_Beam =
{
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, fx);
    WriteEntity (MSG_BROADCAST, src);   // If client, origin overrides p1.
    WriteCoord (MSG_BROADCAST, p1_x);
    WriteCoord (MSG_BROADCAST, p1_y);
    WriteCoord (MSG_BROADCAST, p1_z);
    WriteCoord (MSG_BROADCAST, p2_x);
    WriteCoord (MSG_BROADCAST, p2_y);
    WriteCoord (MSG_BROADCAST, p2_z);
};

void(vector spot) Tent_Explosion =
{
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord (MSG_BROADCAST, spot_x);
    WriteCoord (MSG_BROADCAST, spot_y);
    WriteCoord (MSG_BROADCAST, spot_z);
};

void(vector spot, float rgb, float add) Tent_Explosion2 =
{
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, 12);      // TE_EXPLOSION2
    WriteCoord (MSG_BROADCAST, spot_x);
    WriteCoord (MSG_BROADCAST, spot_y);
    WriteCoord (MSG_BROADCAST, spot_z);
    WriteByte (MSG_BROADCAST, rgb);     // First index to use on the palette.
    WriteByte (MSG_BROADCAST, add);     // Range is from rgb to rgb+add-1.
};

// Generic beam.
// NOTE:  Nehahra engine feature.  Not available in standard Quake.
//void(string text, entity src, vector p1, vector p2) Tent_NehBeam =
//{
//    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
//    WriteByte (MSG_BROADCAST, 17);      // TE_LIGHTNING4
//    WriteString (MSG_BROADCAST, text);
//    WriteEntity (MSG_BROADCAST, src);   // If client, p1 is always origin.
//    WriteCoord (MSG_BROADCAST, p1_x);
//    WriteCoord (MSG_BROADCAST, p1_y);
//    WriteCoord (MSG_BROADCAST, p1_z);
//    WriteCoord (MSG_BROADCAST, p2_x);
//    WriteCoord (MSG_BROADCAST, p2_y);
//    WriteCoord (MSG_BROADCAST, p2_z);
//};

//void(vector p1, vector p2, vector ang) Tent_TomazRail =
//{   // MSG_BROADCAST should be 0.
//    WriteByte (0, SVC_TEMPENTITY); WriteByte (0, 17);
//    WriteCoord (0, p1_x); WriteCoord (0, p1_y); WriteCoord (0, p1_z);
//    WriteCoord (0, p2_x); WriteCoord (0, p2_y); WriteCoord (0, p2_z);
//    WriteCoord (0, ang_x); WriteCoord (0, ang_y); WriteCoord (0, ang_z);
//};

//- - - - - - - - -
void(vector spot) R_Exp3 =
{
    local   entity  e;

    e = spawn();
    setorigin (e, spot);
    sound (e, CHAN_AUTO, "weapons/r_exp3a.wav", 1, ATTN_NORM);
    remove (e);
};

void(vector spot) Rocket_Explosion1 =
{
    R_Exp3 (spot);

    Tent_Explosion (spot);
};

void(vector spot, float rgb, float add) Rocket_Explosion2 =
{
    R_Exp3 (spot);

    Tent_Explosion2 (spot, rgb, add);
};

void(vector spot) Rocket_TarExplosion =
{
    R_Exp3 (spot);

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_TAREXPLOSION);
    WriteCoord (MSG_BROADCAST, spot_x);
    WriteCoord (MSG_BROADCAST, spot_y);
    WriteCoord (MSG_BROADCAST, spot_z);
};
//- - - - - - - - -


//==========================================================================
//  Common Explosions

//float   CX_ORANGE   = 0;
//float   CX_RED      = 1;
//float   CX_YELLOW   = 2;
//float   CX_BLUE     = 3;
//float   CX_PURPLE   = 4;

void(vector spot, float r) Colored_Explosion =
{
    local   float   base, add;

    if (r == 3)
        {base = 244; add = 3;}
    else if (r == 2)
        {base = 240; add = 4;}
    else if (r == 1)
        {base = 247; add = 5;}
    else
    {
        r = random() * 3;
        if (r < 1)
            {base = 224; add = 16;}
        else if (r < 2)
            {base = 230; add = 8;}
        else
            {base = 232; add = 8;}
    }
    Rocket_Explosion2 (spot, base, add);
};

// As 'BecomeAnyExplosion', with adjustable frames per second.
// Used by some custom explosions.
void(float base, float add, float fps, string text) BecomeFpsExplosion =
{
    if (!add)
        add = 6;    // Assume standard duration of six frames.
    if (!fps)
        fps = 10;

// Become explosion code.
    self.movetype   = MOVETYPE_NONE;
    self.velocity   = '0 0 0';
    self.touch      = SUB_Null;
    setmodel (self, text);
    self.solid      = SOLID_NOT;
//- - - -
    self.frame      = base;
    self.cnt        = base + add - 1;
    self.wait       = 1 / fps;
    self.nextthink  = time + self.wait;
    self.think      = s_explode_think;
};

void(float base, float add, string text) BecomeAnyExplosion =
{
    if (!add)
        add = 6;    // Assume standard duration of six frames.

// Become explosion code.
    self.movetype   = MOVETYPE_NONE;
    self.velocity   = '0 0 0';
    self.touch      = SUB_Null;
    setmodel (self, text);
    self.solid      = SOLID_NOT;
//- - - -
    self.frame      = base;
    self.cnt        = base + add - 1;
    self.wait       = 0.1;
    self.nextthink  = time + 0.1;
    self.think      = s_explode_think;
};

void(vector spot, float base, float add, string text) MakeAnyExplosion = {
    local   entity  swap;

    swap = self;

    self = spawn();
    setsize (self, '0 0 0', '0 0 0');        
    setorigin (self, spot);
    BecomeAnyExplosion (base, add, text);

    self = swap;
};

void(float base, float add) BecomeFancy =
    {BecomeAnyExplosion (base, add, "progs/drake/s_fancy.spr");};

void(vector spot, float base, float add) MakeFancy =
    {MakeAnyExplosion (spot, base, add, "progs/drake/s_fancy.spr");};

//==========================================================================
//  Ice Explosions
// iceball.mdl:  frames 0-2 = iceballs, frames 3-7 = explosion of shards.
void() BecomeIcyExp =
{
    self.skin = 0;
    BecomeAnyExplosion (3, 5, "progs/drake/iceball.mdl");
    self.alpha = ALPHA_ICE;
};

void(vector spot) MakeIcyExp = {
    local   entity  iced;

    iced = spawn();
    iced.movetype   = MOVETYPE_NONE;
    iced.velocity   = '0 0 0';
    iced.touch      = SUB_Null;
    iced.solid      = SOLID_NOT;
    //- - - -
    iced.alpha      = ALPHA_ICE;
    iced.frame      = 3;
    iced.cnt        = iced.frame + 4;    // 5 - 1
    iced.wait       = 0.1;
    iced.nextthink  = time + 0.1;
    iced.think      = s_explode_think;
    //- - - -
    setall (iced, "progs/drake/iceball.mdl", '0 0 0', '0 0 0', spot);
};

//==========================================================================
//  Shockwaves

void(vector spot, float base, float add, string text) ShockWave =
{
    local   entity  wav1, wav2, ts;
    local   vector  ang;

// NOTE:  Don't use world in the 4th (ignore) parm or else the traceline
// will pass through any non-world bsp entites, such as shootable
// trap doors.  Use self instead, which should be an inflictor entity.
    traceline (spot, spot - '0 0 64', TRUE, self);
    if (trace_fraction >= 1.0)
        return;     // Too far from ground.
    if ((trace_plane_normal * '0 0 1') < 0.5)   //<= 0.7071
        return;     // Ground slopes too much.

    spot = trace_endpos + trace_plane_normal * 2;
    ang = vectoangles(trace_plane_normal);
    ang_y = anglemod(ang_y + 180);

// Top side.
    wav1 = spawn();
    wav1.angles     = ang;
    setorigin (wav1, spot);

// Bottom side.
    ang_x = anglemod(ang_x + 180);
    wav2 = spawn();
    wav2.angles     = ang;
    setorigin (wav2, spot);

// Thinking
    ts = self;
    self = wav1;
    BecomeAnyExplosion (base, add, text);
    self = wav2;
    BecomeAnyExplosion (base, add, text);
    self = ts;
};

// Used by exploding barrels and MIRVs.  Yes, add 7 instead of 8.
void() FlameWave = {ShockWave (self.origin, RING_FIRE, 7, "progs/drake/s_wave.spr");};

// Modified Custents explosion -- appended after standard explosion sprites.
// Fast version below.  Slow version was <7, 8, 10, 12, 14, 16, 18, 19, 20>.
void() s_explodeb1  =[  7,  s_explodeb2 ] {};
void() s_explodeb2  =[ 10,  s_explodeb3 ] {};
void() s_explodeb3  =[ 13,  s_explodeb4 ] {};
void() s_explodeb4  =[ 16,  s_explodeb5 ] {};
void() s_explodeb5  =[ 19,  s_explodeb6 ] {};
void() s_explodeb6  =[ 20,  SUB_Remove  ] {};

//==========================================================================
//  More Fiery Explosions

void() s_fiery1 =[  1, s_fiery2 ] {};
void() s_fiery2 =[  3, s_fiery3 ] {};
void() s_fiery3 =[  5, s_fiery4 ] {};
void() s_fiery4 =[  7, s_fiery5 ] {};
void() s_fiery5 =[  9, s_fiery6 ] {};
void() s_fiery6 =[ 11, s_fiery7 ] {};
void() s_fiery7 =[ 13, s_fiery8 ] {};
void() s_fiery8 =[ 15, SUB_Remove ] {};

void(string fx, void() thinkst) Explosion_Start =
{
    self.movetype   = MOVETYPE_NONE;
    self.velocity   = '0 0 0';
    self.touch      = SUB_Null;
    setmodel (self, fx);
    self.solid      = SOLID_NOT;
    thinkst ();
};

void() BecomeTheBigFire =
{
    if (deathmatch || coop)
        Explosion_Start ("progs/drake/s_expbig.spr", s_fiery1);
    else
        BecomeFpsExplosion (0, 17, 20, "progs/drake/s_expbig.spr");
};

void() BecomeExplosion2 =
{
    if (deathmatch || coop)
        Explosion_Start ("progs/drake/s_explod.spr", s_explodeb1);
    else
        BecomeFpsExplosion (6, 16, 25, "progs/drake/s_explod.spr");
};

// This creates a small particle trail behind the shot.
void(float rgb, float density, float full) Meteor_Trail =
{
    Particle_Beam (self.oldorigin, self.origin, rgb, rgb, density, full);
    self.oldorigin = self.origin;   // Update previous point.
};

// 'waitmin' = slow color, 'waitmax' = fast color.
void(entity shot, float rgb) Laser_Color =
{
    shot.skin = rgb;
    if (rgb == 3)   // Blue
        {shot.waitmin = 9;  shot.waitmax = 41;}
    else if (rgb == 2)  // Yellow
        {shot.waitmin = 105;  shot.waitmax = 193;}
    else if (rgb == 1)  // Red
        {shot.waitmin = 73;  shot.waitmax = 225;}
    else    // Orange
        {shot.skin = 0;  shot.waitmin = 97;  shot.waitmax = 233;}
    // Note:  For orange, I replaced 225 with 97 to stop blood in some ports.
};

//==========================================================================
//  Nemesant Effects

// PM:  Like BecomeExplosion, but for the Nemesant's signature explosion.
// In Drake, some bosses use this for their post-mortem explosions.
void() WarpExplosion =
{
    sound (self, CHAN_AUTO, "nemesant/bigboom.wav", 1, ATTN_NORM);
    Tent_Point (TE_EXPLOSION, self.origin);
    FlameWave ();       // PM:  New shockwave effect at ground zero.
// DISABLED -- Quake effects may be a bit excessive.
//    Quake_Spawn (300, 0, self.origin, 0, 1);
    BecomeTheBigFire ();    // Replaced old fireball mdl with big sprite.
};

//- - - - - - - - -
// PM:  Moved circle drawing code from Zerstorer's misc.qc to here.
void(vector Center, float Radius, float CColor) DrawCircleXY =
{
local   float   MajorAxis, MinorAxis;
local   float   RadSqminusMajAxis, MinAxisSqThreshold;
local   vector  circlepos, vel;

    MajorAxis = 0;
    MinorAxis = Radius;
    circlepos_z = Center_z;
    vel = '0 0 1';

    RadSqminusMajAxis = Radius * Radius;
    MinAxisSqThreshold = MinorAxis * MinorAxis - MinorAxis;

    do {
        circlepos_x = Center_x+MajorAxis; circlepos_y = Center_y-MinorAxis;
        particle(circlepos, vel, CColor, 15);
        circlepos_x = Center_x-MajorAxis;
        particle(circlepos, vel, CColor, 15);
        circlepos_x = Center_x+MajorAxis; circlepos_y = Center_y+MinorAxis;
        particle(circlepos, vel, CColor, 15);
        circlepos_x = Center_x-MajorAxis;
        particle(circlepos, vel, CColor, 15);
        circlepos_x = Center_x+MinorAxis; circlepos_y = Center_y-MajorAxis;
        particle(circlepos, vel, CColor, 15);
        circlepos_x = Center_x-MinorAxis;
        particle(circlepos, vel, CColor, 15);
        circlepos_x = Center_x+MinorAxis; circlepos_y = Center_y+MajorAxis;
        particle(circlepos, vel, CColor, 15);
        circlepos_x = Center_x-MinorAxis;
        particle(circlepos, vel, CColor, 15);
        MajorAxis = MajorAxis + 20;
        RadSqminusMajAxis = RadSqminusMajAxis - (MajorAxis + MajorAxis - 20);
        if ( RadSqminusMajAxis <= MinAxisSqThreshold )
        {
            MinorAxis = MinorAxis - 20;
            MinAxisSqThreshold = MinAxisSqThreshold - (MinorAxis + MinorAxis);
        }
    } while ( MajorAxis <= MinorAxis );
};

//==========================================================================
//  Normal Lightning Bolt

// Creates a discharge effect.
//void(entity ent) Discharge =
//{
//    local   float   damage;

   // if (!ent)
   //     return;
   // damage = floor (ent.ammo_cells * 35 / 2);  // Max cells is 200.
    //if (damage < 1)
    //    return;
    //Update_Cells (ent, 0);
   // ent.currentammo = 0;    // For NPCs.
  //  if (ent.flags & FL_CLIENT)
  //      SUB_Think (ent, War_FindBestWeapon);

// FIXME:  In DarkPlaces, clients can set lightning color for themselves.
 //   sound (self, CHAN_AUTO, "plasma/explode.wav", 1, ATTN_NORM);
 //   Rocket_Explosion2 (ent.origin, 244, 3);
//    MakeFancy (ent.origin, X56_PLASMA_BLUE, 6);
    // PM:  Added rocket damage flag for shamblers' resistance.
//    T_RadiusDamage (ent, ent, damage, world, DAMAGEALL);
//};

//------------------------------------------------------------------------//
//  This inflicts damage to an entity hit by the lightning bolt.
//------------------------------------------------------------------------//
void LightningBlood (entity targ,vector org,float damage);


void(entity targ, entity inflictor, entity attacker, vector org, float damage, string dtype) BoltDamage = {
    LightningBlood (targ, org, damage);
    T_Damage (targ, inflictor, attacker, damage, DAMARMOR);
};

void(entity targ, entity inflictor, entity attacker, vector org, float damage, float dflags, string dtype) BoltDamage_Ex = {
    LightningBlood (targ, org, damage);
    T_Damage (targ, inflictor, attacker, damage, DAMARMOR);
};

// Spawns the blood caused by hits from lightning bolts.
void(entity targ, vector org, float damage) LightningBlood =
{
    local   float   den;

    den = damage * targ.resist_cells * 4;   // Was 'damage*4'
    den = floor (den);
    if (den > 0)
    {
        local   float   blud;

        if (targ.bloodtype == BLOOD_RED)
            blud = 225;     // Lightning blood.
        else
            blud = targ.bloodtype;
        if (blud > 0)
            particle (org, '0 0 100', blud, den);     // Hit/bleed
        else
            particle (org, '0 0 20', 0 - blud, 10);   // chunks
    }
};

//------------------------------------------------------------------------//
//  This strikes all entities hit by the lightning bolt.
//------------------------------------------------------------------------//
void(vector p1, vector p2, entity attacker, float damage, entity inflictor,
    float fx, string dtype) LightningBolt =
{
// The old lightning code spawned two more invisible beams at weird places
// that could damage and kill more enemies (or teammates).  The new code
// below creates a single piercing bolt that hits everything in its path,
// like the railgun.  Lightning in many games (e.g., D&D, Diablo) pierce.

    local   entity  ruler;
    local   entity  ignore;
    local   entity  targ;
    local   vector  start;
    local   vector  dir;
    local   float   dist;
    local   float   delta;
    local   float   yes;
    local   float   first;

    //GhostTouch_On ();

    first =
    yes = TRUE;
    ruler = inflictor;

    dir = p2 - p1;
    delta = vlen(dir);  // Get total length in case of reflection.
    dir = normalize(dir);

    while (ruler)
    {
        ignore = inflictor = ruler;
        ruler = world;      // End loop unless set otherwise by reflection.
        start = p1;

        do
        {
            traceline (p1, p2, FALSE, ignore);
    
            p1 = trace_endpos;
            targ = trace_ent;

            if (targ.solid != SOLID_BSP)
                ignore = targ;
            else
                ignore = world;

            if (targ != inflictor)      // Dirty hack to prevent suicide.
            {
                //if (HasReflection (targ, p1, damage))
                //{
                    //if (yes)
                    //{
                    //    yes = FALSE;
                    //    ruler = targ;
                    //    delta = delta - vlen(p1 - start);   // Get remaining dist.
                   //     dir = normalize(p1 - targ.origin);
                        p2 = p1 + dir*delta;
                    //}
                    //else    // ...we give up.
                    //{   // Use dummy entity to draw reflected bolt.
                        // If there are many, only one is drawn.
                    //    inflictor = lightning;
                    //}
                    //ignore = world;     // Stop bolt.
                    //ReflectSound (targ);
                //}
                //else
                //{
                    if (first)
                    if (targ.takedamage)
                        BoltDamage (targ, inflictor, attacker, p1, damage, dtype);
                //}
            }

        // If current trace hits inflictor/attacker, next trace will miss all.
        // Move the new start point ahead slightly to prevent attacker from
        // hitting himself and preventing the bolt from piercing anymore.
        // (Note:  If attacker is inside the monster because of noclip,
        // he can still hit himself.)
            if (ignore)     // Always world if 'p2 == trace_endpos'.
            {
                dist = vlen(p2 - p1);
                if (dist > 1)
                    p1 = p1 + dir;
                else
                    ignore = world;   // Not enough distance left, so just stop.
            }
        }
        while (ignore);

        // Draw the lightning bolt between the two points.
        Tent_Beam (fx, inflictor, start, p1);
    }

    //GhostTouch_Off ();
};
void(vector p1, vector p2, entity attacker, float damage) LightningBolt1 =
    { LightningBolt (p1, p2, attacker, damage, attacker, TE_LIGHTNING1, "lightning"); };
void(vector p1, vector p2, entity attacker, float damage) LightningBolt2 =
    { LightningBolt (p1, p2, attacker, damage, attacker, TE_LIGHTNING2, "lightning"); };

//==========================================================================
//  Smart Fireballs -- Used by nemesant (and SoE sorcerer).

void() SmartBall_Think =
{
    if (self.delay <= time)
        {remove(self); return;}

    self.angles = vectoangles(self.velocity);

// Changed particle count from 25 at 10/sec to 10 at 20/sec.
// Note:  2000 speed rocket trails spawn 11 at about 50/sec.
    particle (self.origin, '0 0 1', 233, 10);   //particle (self.origin, '0 0 1', 233, 25);
    self.nextthink = time + 0.05;               //self.nextthink = time + 0.1;
};

void() SmartBall_Bounce =
{
    local   vector  p1, p2;
    local   float   up;

    p1 = self.origin;
    p2 = self.enemy.origin + self.enemy.view_ofs;   // Go for the eyes.
    up = Aim_TossUp (p1, p2, 400);

// Fireballs can be fooled by invisibility.
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.velocity = Maim_SmartBall (p1, p2, 400, up, world, '0 0 0');
    self.velocity = self.velocity * 400;
    self.velocity_z = self.velocity_z + up;
    self.angles = vectoangles(self.velocity);

    self.nextthink = time + 0.01;
    self.think = SmartBall_Think;
};

void() SmartBall_Touch =
{
    if (pointcontents(self.origin) == CONTENT_SKY)
        {remove(self); return;}     // Disappeared into space.
    //if (Reflected_Damage (COURAGE_DAMAGE))
    //    return;     // Bounced off reflective shield.

// Explode if...
// 1) Entity struck can take damage.
// 2) Fireball bounced more than three times.  One more than HipLaser.
// 3) Target is already dead.
    if (other.takedamage || (self.count >= 3) || (self.enemy.health <= 0))
    {   // Go BOOM!
        Touch_Missile();
        return;
    }

    // PM:  Removed 'hknight/hit.wav'.
    sound (self, CHAN_WEAPON, "nemesant/frblfly.wav", 0.6, ATTN_NORM);
    self.count = self.count + 1;
    self.think = SmartBall_Bounce;
    self.nextthink = time + 0.01;
};

void(vector start, vector dir, vector add) SmartBall_Launch = {
    newmis = spawn ();
    newmis.master       = newmis.owner      = self;
    newmis.movetype     = MOVETYPE_TOSS;
    newmis.solid        = SOLID_BBOX;
    newmis.classname    = "fireball";
// set missile speed
    newmis.speed        = 600;
    newmis.velocity     = (dir * 600) + add;
    newmis.angles       = vectoangles(newmis.velocity);
// set missile damage
    if (cranked)
        newmis.dmg          = 60;
    else
        newmis.dmg          = 40;
    newmis.enemy        = self.enemy;
    newmis.style        = DF_ROCKET | DF_SAFE;
    newmis.worldtype    = MDAM_GRENADE;
// set missile special stuff
    newmis.count        = 0;
// set missile duration
    newmis.touch        = SmartBall_Touch;
    newmis.delay        = time + 20;    // Total duration
    newmis.nextthink    = time + 0.1;
    newmis.think        = SmartBall_Think;
    newmis.classtype     = CT_PROJ_SMARTBALL;

    setall (newmis, "progs/drake/smartbal.mdl", '0 0 0', '0 0 0', start);
};

void(vector st) M_FireSmart = {
    local   vector  p1, p2, dir, up;
    local   entity  targ;

    targ = world;
    makevectors (self.angles);
    p1 = self.origin + (v_forward*st_x) + (v_right*st_y) + (v_up*st_z);
    p2 = Aimpoint (self.enemy);

    up = '0 0 0';
    up_z = Aim_TossUp (p1, p2, 600);
    dir = Maim_SmartBall (p1, p2, 600, up_z, targ, '0 0 0');
    if (!targ)
    {   // In case we can't hit enemy, just lob it straight ahead.
        TraceToss (p1, up + dir*600, FALSE, self, 5);
        if (trace_ent != self.enemy)
            up_z = 200;
    }

    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_AUTO, "nemesant/frblfly.wav", 0.6, ATTN_NORM);

    SmartBall_Launch (p1, dir, up);
};

void() death_puff =
{
    local   vector  spot;
    local   entity  puff;

    spot_x = random();
    spot_y = random();
    spot_z = random() * 0.5;
    spot = Anypoint (self, spot);

    puff = spawn();
    setorigin (puff, spot);
    puff.effects = EF_MUZZLEFLASH;
    sound (puff, CHAN_AUTO, "hknight/hit.wav", 1, ATTN_NORM);
    SUB_Think (puff, BecomeExplosion);  // FIXME:  Invisible in Qrack.
};

// Like ShockWave, but show only the top side.
// Use for mini-rings, when you need a lot of them.
void(vector spot, float base, float add, string text) HalfWave =
{
    local   entity  wav1, ts;
    local   vector  ang;

    traceline (spot, spot - '0 0 64', TRUE, world);
    if (trace_fraction >= 1.0)
        return;     // Too far from ground.
    // Removed slope check.

    spot = trace_endpos + trace_plane_normal * 2;
    ang = vectoangles(trace_plane_normal);
    ang_y = anglemod(ang_y + 180);

// Top side.
    wav1 = spawn();
    wav1.angles     = ang;
    setorigin (wav1, spot);

// Thinking
    ts = self;
    self = wav1;
    BecomeAnyExplosion (base, add, text);
    self = ts;
};

//==========================================================================
//  Wind Wall -- for SoE

//- - - - - - - - -
// Missile_Up -- Variation of 'ReflectMissile'.
// Redirects a missile straight up.
//
// Entity who is reflecting the shot is assumed to be 'self'.
//
// shot:  Missile to be redirected.
//- - - - - - - - -
void(entity shot) Missile_Up =
{
    if (shot.movetype == MOVETYPE_NONE)
        return;     // Can't move.

// Reset fields that may affect its movement.
    if (!shot.speed)
        shot.speed = vlen(shot.velocity);
    shot.movedir = '0 0 1';
    shot.velocity = shot.movedir * shot.speed;
    shot.dest = shot.velocity;      // For lasers and spikes.
    shot.angles = vectoangles(shot.movedir);
    if (shot.pos1_x)    // For fireballs.
        shot.angles_x = shot.angles_x + shot.pos1_x;
    shot.oldorigin = shot.origin;   // For particle trails.
// Unstick missile if engine thinks it is on the ground.
    shot.flags = shot.flags - (shot.flags & FL_ONGROUND);
    shot.groundentity = world;
// Special:  If remote controlled mirv, sever link.
    if (shot.master)
        if (shot.master.mirv1 == shot)
            shot.master.mirv1 = world;
};

// Spawns a dust cloud upward.  Used by Storm Baron's gust attack.
void(entity ent) Particle_Gust =
{
    local   vector  spot;
    local   float   loop;

    loop = 9;
    while (loop > 0)
    {
        loop = loop - 1;
        spot_x = random() * 2 - 0.5;
        spot_y = random() * 2 - 0.5;
        spot_z = loop / 8;
        spot = Anypoint (ent, spot);
        particle (spot, '0 0 8', 1, 5);
    }
};

float SPRITE_PLAYONCE = 4;
float SPRITE_STARTOFF = 8;
float S_ACTIVE = 1;
float S_DEACTIVE = 0;
float S_ALPHANULL = 0.001;

void() env_sprite_loop = {
    if (self.spritestarted == FALSE) {
        self.frame = self.pos1_x;
        self.spritestarted = TRUE;
        self.alpha = self.pos1_y;
    } else {
        self.frame = self.frame + 1;
    }

    if (self.frame > (self.idleweaponframe)) {
        self.frame = self.pos1_x;
        if (self.spawnflags & SPRITE_PLAYONCE) {
            self.use();
            return;
        }
    }

    self.think = env_sprite_loop;
    self.nextthink = time + 0.05;
};

// Added by Qmaster
void() env_sprite_hide = {
    //self.model = "";
    //self.modelindex = 0;
    self.alpha = 0.0001;
    self.spritestarted = FALSE;
    self.frame = self.pos1_x;    //reset
    self.effects = 0;
    self.solid = SOLID_NOT;
    self.takedamage = DAMAGE_NO;
    self.movetype = MOVETYPE_NONE;
    self.velocity = VEC_ORIGIN;
    self.angles = VEC_ORIGIN;
    self.use = self.think1;
    self.think = SUB_Null;
    self.nextthink = time + 0.1;
};

void () env_sprite_use = {
    if (self.state == S_ACTIVE) {
        self.alpha = S_ALPHANULL;
        self.spritestarted = FALSE;
        self.state = S_DEACTIVE;
    } else {
        self.state = S_ACTIVE;
        self.alpha = self.pos1_y;
    }
};

void () env_sprite_on = {
    
};

void () env_sprite_off = {

};

void () env_sprite_disable = {

};

void () env_sprite_reset = {

};


void () env_sprite = {
    if (self.mdl == "") {
        dprint("ERROR: no model for env_sprite\n");
        return;
    }

    precache_model(self.mdl);
    setmodel(self,self.mdl);
    if (self.alpha <= 0) self.alpha = 1;
    self.pos1_x = self.frame;
    self.pos1_y = self.alpha;
    self.use = env_sprite_use;
    self.estate_on = env_sprite_on;
    self.estate_off = env_sprite_off;
    self.estate_disable = env_sprite_disable;
    self.estate_reset = env_sprite_reset;
    self.estate = ESTATE_ON;

    if (self.spawnflags & SPRITE_STARTOFF) {
        self.estate_off();
    }

    if (self.idleweaponframe > 0) {
        self.think = env_sprite_loop;
    } else {
        self.think = SUB_Null;
    }
    self.nextthink = time + 0.1;
};
