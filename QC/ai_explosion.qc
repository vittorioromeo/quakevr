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
// SpawnBlood (All blood particles through this function)
//----------------------------------------------------------------------
// TODO VR: (P2) (AD) - this is called just SpawnBlood in AD
void(entity targ, vector org, vector vel, float part_qty) SpawnBlood_AD =
{
    local float loop_count, part_col;
    loop_count = 0;
    vel = vel * 0.1;
    // monsters show 2 x blood particles (default), it seems pointless
    // to reduce that back down here. When a monster has a high ammo
    // resistance it needs to be obviously less, the default needs to
    // be higher to make a greater visual impact.
    // part_qty = part_qty / 2;

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
// TODO VR: (P2) (AD) - this is called just spawn_touchblood in AD
void(entity source, entity targ, float damage) spawn_touchblood_AD =
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

    SpawnBlood_AD (targ, source.origin + org, vel, damage);
};

//======================================================================
// SpawnMeatSpray
// Changed to remove dependance on 'self' for missile origin
// Changed parameters to add source and destination of attack
// Changed velocity to side so it is calculated correctly from angles
//======================================================================
// TODO VR: (P2) (AD) - this is called just SpawnMeatSpray in AD
void(entity source, entity targ, float side) SpawnMeatSpray_AD =
{
    local vector org;

    makevectors(source.angles);

    // Check for a melee offset? - Special vector offset
    if (CheckZeroVector(source.meleeoffset)) org = v_forward * 16;
    else org = attack_vector(source.meleeoffset);

    // Create starting point to spawn
    org = org + source.origin;

    if (targ.bleedcolour) SpawnBlood_AD(targ, org, v_up*2, 100);
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
