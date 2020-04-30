/*======================================================================
 MONSTER AMMO RESISTANCE SYSTEM

======================================================================*/

float(entity targ, float resist_type, float damage) Resist_Damage =
{
    local float ret_damage;
    
    if (resist_type == IT_SHELLS) ret_damage = damage * (1 - targ.resist_shells);
    else if (resist_type == IT_NAILS) ret_damage = damage * (1 - targ.resist_nails);
    else if (resist_type == IT_ROCKETS) ret_damage = damage * (1 - targ.resist_rockets);
    else if (resist_type == IT_CELLS) ret_damage = damage * (1 - targ.resist_cells);

    return ret_damage;
};

//----------------------------------------------------------------------
// Double check that all ammo resistance are within range
//----------------------------------------------------------------------
void(entity targ) Resist_CheckRange = 
{
    // Check Ammo resistance is within range
    if (targ.resist_shells < 0) targ.resist_shells = 0;
    else if (targ.resist_shells > 1) targ.resist_shells = 1;
    if (targ.resist_nails < 0) targ.resist_nails = 0;
    else if (targ.resist_nails > 1) targ.resist_nails = 1;
    if (targ.resist_rockets < 0) targ.resist_rockets = 0;
    else if (targ.resist_rockets > 1) targ.resist_rockets = 1;
    if (targ.resist_cells < 0) targ.resist_cells = 0;
    else if (targ.resist_cells > 1) targ.resist_cells = 1;
};

//----------------------------------------------------------------------
// Save current ammo resistance to ammo variables
// ** Ammo variables are used for backpack, use with caution
//----------------------------------------------------------------------
void(entity targ) Resist_Save =
{
    // Store the ammo resistance for switching later
    targ.ammo_shells = targ.resist_shells;
    targ.ammo_nails = targ.resist_nails;
    targ.ammo_rockets = targ.resist_rockets;
    targ.ammo_cells = targ.resist_cells;
};

//----------------------------------------------------------------------
// Switch resistance from FULL to monster default
// This works with Resist_Save system, use with caution
// Designed for bosses which don't drop things on death
//----------------------------------------------------------------------
void(entity targ, float artype) Resist_ChangeType =
{
    if (artype == TRUE) {
        // Complete resistance to everything
        targ.resist_shells = targ.resist_nails = 1;
        targ.resist_rockets = targ.resist_cells = 1;
        targ.pain_finished = LARGE_TIMER;
    }
    else {
        // Default ammo resist, rocket/cell immunnity
        targ.resist_shells = targ.ammo_shells;
        targ.resist_nails = targ.ammo_nails;
        targ.resist_rockets = targ.ammo_rockets;
        targ.resist_cells = targ.ammo_cells;
        targ.pain_finished = time + 1;
    }
    dprint("\b[AMMO RESIST]\b");
    dprint(" S="); dprint(ftos(targ.resist_shells));
    dprint(" N="); dprint(ftos(targ.resist_nails));
    dprint(" R="); dprint(ftos(targ.resist_rockets));
    dprint(" C="); dprint(ftos(targ.resist_cells));
    dprint("\n");
};

//----------------------------------------------------------------------
void(entity targ, vector org, vector vel, float damage) Resist_Shells =
{
    if (targ.health > 0 && targ.pain_finished < time) {
        targ.pain_finished = time + 0.3 + random();
        sound (targ, CHAN_VOICE, targ.pain_sound, 1, ATTN_NORM);
    }
    // Drastically reduce blood particles
    if (targ.resist_shells < 1) SpawnBlood (targ, org, vel*0.2, damage/4);
    if (random() < 0.3) {
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        if (random() < 0.5) WriteByte (MSG_BROADCAST, TE_SPIKE);
        else WriteByte (MSG_BROADCAST, TE_GUNSHOT);
        WriteCoord (MSG_BROADCAST, org_x);
        WriteCoord (MSG_BROADCAST, org_y);
        WriteCoord (MSG_BROADCAST, org_z);
        SpawnProjectileSmoke(org, 200, 50, 150);
    }
};

//----------------------------------------------------------------------
void(entity targ, vector org) Resist_Nails =
{
    if (targ.health > 0 && targ.pain_finished < time && targ.resist_nails < 1) {
        targ.pain_finished = time + 0.3 + random();
        sound (targ, CHAN_VOICE, targ.pain_sound, 1, ATTN_NORM);
    }    
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    if (random() < 0.5) WriteByte (MSG_BROADCAST, TE_SPIKE);
    else WriteByte (MSG_BROADCAST, TE_GUNSHOT);
    WriteCoord (MSG_BROADCAST, org_x);
    WriteCoord (MSG_BROADCAST, org_y);
    WriteCoord (MSG_BROADCAST, org_z);
    if (random() < 0.5) SpawnProjectileSmoke(org, 200, 50, 150);
};

//----------------------------------------------------------------------
void(entity targ, vector org) Resist_Rockets =
{
    if (targ.health > 0 && targ.pain_finished < time && targ.resist_rockets < 1) {
        targ.pain_finished = time + 0.3 + random();
        sound (targ, CHAN_VOICE, targ.pain_sound, 1, ATTN_NORM);
    }
    SpawnExplosion(EXPLODE_BURST_SMOKE,org,SOUND_RESIST_ROCKET);
    SpawnProjectileSmoke(org, 200, 50, 150);
    SpawnProjectileSmoke(org, 200, 50, 250);
    SpawnProjectileSmoke(org, 300, 50, 150);
    // Can the target bleed?
    if (targ.resist_rockets < 1) {
        SpawnProjectileMeat(targ, org, 200, 50, 150);
        SpawnProjectileMeat(targ, org, 300, 50, 150);
    }
};

//----------------------------------------------------------------------
void(entity targ, vector org) Resist_Lightning =
{
    if (targ.health > 0 && targ.pain_finished < time && targ.resist_cells < 1) {
        targ.pain_finished = time + 0.3 + random();
        sound (targ, CHAN_VOICE, targ.pain_sound, 1, ATTN_NORM);
    }    
    // Don't spawn smoke constantly (let the sprite finish)
    if (self.lightning_timer < time) {
        self.lightning_timer = time + 0.3;
        SpawnExplosion(EXPLODE_BURST_SMOKE, org, "");
    }
    // Spawn a random smoke particle and chance of blood
    SpawnProjectileSmoke(org, 200, 50, 150);
    if (targ.resist_cells < 1 && random() < 0.4) 
        SpawnProjectileMeat(targ, org, 200, 50, 150);
};

//----------------------------------------------------------------------
void(entity targ, vector org) Resist_Plasma =
{
    if (targ.health > 0 && targ.pain_finished < time && targ.resist_cells < 1) {
        targ.pain_finished = time + 0.3 + random();
        sound (targ, CHAN_VOICE, targ.pain_sound, 1, ATTN_NORM);
    }    
    SpawnExplosion(EXPLODE_BURST_SMOKE, org, SOUND_PLASMA_HIT);
    SpawnProjectileSmoke(org, 200, 50, 150);
    SpawnProjectileSmoke(org, 200, 50, 250);
    SpawnProjectileSmoke(org, 300, 50, 150);
    // Can the target bleed?
    if (targ.resist_cells < 1 && random() < 0.3) 
        SpawnProjectileMeat(targ,org, 200, 50, 150);
};

