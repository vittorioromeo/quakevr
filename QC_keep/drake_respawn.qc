//============================================================================

// Checks if starting point of 'self' is clear.
float() M_CheckPosition =
{
    local   entity  head;
    local   vector  vmin, vmax;

    vmin = self.oldorigin + self.dest1;
    vmax = self.oldorigin + self.dest2;
// Is the spawn point blocked by terrain now?
// Don't use BoxInWall because monsters' feet may be in the ground on purpose.
    if (pointcontents (self.oldorigin) == CONTENT_SOLID)
    if (pointcontents (vmin) == CONTENT_SOLID)
    if (pointcontents (vmax) == CONTENT_SOLID)
    {   //dprint ("engulfed\n");
        return FALSE;
    }
// Check if the spawn point is occupied or not.
    head = findradius (self.oldorigin, 256);
    while (head)
    {
        if (head.solid >= SOLID_BBOX)
        if (head != self)
        if (Point_Overlap (head.absmin, head.absmax, vmin, vmax))
        {   //dprint ("overlap\n");
            return FALSE;
        }
        head = head.chain;
    }
    return TRUE;
};

// Checks if 'ent' is allowed to respawn.
float() M_RespawnCheck =
{
    if (doomed)
        if (self.th_spawn)
            if (!self.spawned)
                if (self.spawnflags & SPAWN_WHITE == 0)   // Can't be good.
                    return TRUE;
    return FALSE;
};

//================
// monster_respawn
//
// Monster attempts to respawn.  Thinking set by 'monster_end'.
//================
void() monster_respawn = {
    local   float   first;

// Respawn the creature.
    // If self has solid_trigger and a defined touch func, game may crash.
    self.touch = SUB_Null;
    self.solid = SOLID_NOT;
    self.deadflag = DEAD_NO;
    self.health = self.max_health;
    self.enemy = world;     // Forget all targets like in Doom.
    self.oldenemy = world;
    self.targetname =
    self.killtarget = self.target = self.target2 = "";
    self.angles = self.mangle;

    // Zero out fields that may have been changed when killed.
    self.cnt =
    self.volume =
    self.healtype =
    self.healamount = 0;
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.groundentity = world;
    self.gorging = FALSE;
    self.view_ofs = '0 0 0';

// Move creature back to its original spawn point and cover current point
// and spawn point with teleporter effects.
    // 1. Spawn effects at original spawn point.
    if (self.dest2_x > 96)
    {   // Hack for oldone/wyrm.
        Tent_Point (TE_LAVASPLASH, self.oldorigin - '0 0 24');
        sound (self, CHAN_VOICE, "boss1/out1.wav", 1, ATTN_NORM);
    }
    else
        spawn_tfog (self.oldorigin);
    // 2. Spawn effects at its current point, if a body is visible.
    if (self.mdl)
        if (self.mdl != MODEL_PLAYER_EYES)
            spawn_tfog (self.origin);
    // 3. Move back to spawn point.
    setorigin (self, self.oldorigin);

// Doom removed the old monster and spawned a new one in its place.
// Here, in Quake, just recycle the old monster.
    self.skin = self.spawnskin;
    respawning = TRUE;
    first = self.spawned;
    self.spawned = TRUE;    // Bypass precaching and thinking delays.
    self.th_spawn();
    self.spawned = first;
    respawning = FALSE;
};

void() monster_respawn_random =
{
    if (intermission_running)
        {self.nextthink = -1; self.think = SUB_Null; return;} // End of level.

    Doom_NextThink (32);    // T+32/35.

    if (cutscene)
        return;     // Not during a cutscene.
    if (!self.th_spawn)
        return;     // No spawn function!
    if (prandom() > self.cnt)   // cnt is normally 4.
        return;     // Small chance of success per check, same as in Doom.
    if (!M_CheckPosition ())
        return;     // Spawn point occupied.

// Respawn the creature.
    monster_respawn ();
};

void() monster_respawn_timed =
{
    if (intermission_running)
        {self.nextthink = -1; self.think = SUB_Null; return;} // End of level.

    self.nextthink = time + 0.1;    // No need to emulate Doom if not random.

// Since we are on a timer, stop respawning only if it is impossible.
    if (!self.th_spawn)
        return;     // No spawn function!
    if (!M_CheckPosition ())
        return;     // Spawn point occupied.

// Respawn the creature.
    monster_respawn ();
};

void(float secs) M_RespawnNextthink =
{
    if (secs)
    {
        self.nextthink  = time + secs;
        self.think      = monster_respawn_timed;
    }
    else
    {   // Emulate Doom style respawn timings.
        self.nextthink  = time + 12;    // Minimum delay.
        self.think      = monster_respawn_random;
    }
};

void() SUB_Disappear = {
    self.movetype   = MOVETYPE_NONE;
    self.velocity   = '0 0 0';
    self.touch      = SUB_Null;      //SUB_Null;
    self.solid      = SOLID_NOT;
    self.takedamage = DAMAGE_NO;
    setmodel (self, "");
};

void() monster_disappear = { M_RespawnNextthink (0); SUB_Disappear (); };

// rod = Respawn or Die.
void() monster_rod =
{
    if (M_RespawnCheck ())
        {M_RespawnNextthink (0);  SUB_Disappear ();}
    else
        SUB_Remove ();
};


//============================================================================
